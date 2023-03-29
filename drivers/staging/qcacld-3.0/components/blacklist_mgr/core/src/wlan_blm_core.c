/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * DOC: declare internal APIs related to the blacklist component
 */

#include <wlan_objmgr_pdev_obj.h>
#include <wlan_blm_core.h>
#include <qdf_mc_timer.h>
#include <wlan_scan_public_structs.h>
#include <wlan_scan_utils_api.h>
#include "wlan_blm_tgt_api.h"
#include <wlan_cm_bss_score_param.h>

#define SECONDS_TO_MS(params)       (params * 1000)
#define MINUTES_TO_MS(params)       (SECONDS_TO_MS(params) * 60)
#define RSSI_TIMEOUT_VALUE          60

static void
blm_update_ap_info(struct blm_reject_ap *blm_entry, struct blm_config *cfg,
		   struct scan_cache_entry *scan_entry)
{
	qdf_time_t cur_timestamp = qdf_mc_timer_get_system_time();
	qdf_time_t entry_add_time = 0;
	bool update_done = false;
	uint8_t old_reject_ap_type;

	old_reject_ap_type = blm_entry->reject_ap_type;

	if (BLM_IS_AP_AVOIDED_BY_USERSPACE(blm_entry)) {
		entry_add_time =
			blm_entry->ap_timestamp.userspace_avoid_timestamp;

		if ((cur_timestamp - entry_add_time) >=
		     MINUTES_TO_MS(cfg->avoid_list_exipry_time)) {
			/* Move AP to monitor list as avoid list time is over */
			blm_entry->userspace_avoidlist = false;
			blm_entry->avoid_userspace = false;
			blm_entry->driver_monitorlist = true;

			blm_entry->ap_timestamp.driver_monitor_timestamp =
								cur_timestamp;
			blm_debug("Userspace avoid list timer expired, moved to monitor list");
			update_done = true;
		}
	}

	if (BLM_IS_AP_AVOIDED_BY_DRIVER(blm_entry)) {
		entry_add_time = blm_entry->ap_timestamp.driver_avoid_timestamp;

		if ((cur_timestamp - entry_add_time) >=
		     MINUTES_TO_MS(cfg->avoid_list_exipry_time)) {
			/* Move AP to monitor list as avoid list time is over */
			blm_entry->driver_avoidlist = false;
			blm_entry->nud_fail = false;
			blm_entry->sta_kickout = false;
			blm_entry->ho_fail = false;
			blm_entry->driver_monitorlist = true;

			blm_entry->ap_timestamp.driver_monitor_timestamp =
								cur_timestamp;
			blm_debug("Driver avoid list timer expired, moved to monitor list");
			update_done = true;
		}
	}

	if (BLM_IS_AP_BLACKLISTED_BY_DRIVER(blm_entry)) {
		entry_add_time =
			blm_entry->ap_timestamp.driver_blacklist_timestamp;

		if ((cur_timestamp - entry_add_time) >=
		     MINUTES_TO_MS(cfg->black_list_exipry_time)) {
			/* Move AP to monitor list as black list time is over */
			blm_entry->driver_blacklist = false;
			blm_entry->driver_monitorlist = true;
			blm_entry->nud_fail = false;
			blm_entry->sta_kickout = false;
			blm_entry->ho_fail = false;
			blm_entry->ap_timestamp.driver_monitor_timestamp =
								cur_timestamp;
			blm_debug("Driver blacklist timer expired, moved to monitor list");
			update_done = true;
		}
	}

	if (BLM_IS_AP_IN_RSSI_REJECT_LIST(blm_entry)) {
		qdf_time_t entry_age = cur_timestamp -
			    blm_entry->ap_timestamp.rssi_reject_timestamp;

		if ((blm_entry->rssi_reject_params.retry_delay &&
		     entry_age >= blm_entry->rssi_reject_params.retry_delay) ||
		    (scan_entry && (scan_entry->rssi_raw >= blm_entry->
					   rssi_reject_params.expected_rssi))) {
			/*
			 * Remove from the rssi reject list as:-
			 * 1. In case of OCE reject, both the time, and RSSI
			 *    param are present, and one of them have improved
			 *    now, so the STA can now connect to the AP.
			 *
			 * 2. In case of BTM message received from the FW,
			 *    the STA just needs to wait for a certain time,
			 *    hence RSSI is not a restriction (MIN RSSI needed
			 *    in that case is filled as 0).
			 *    Hence the above check will still pass, if BTM
			 *    delay is over, and will fail is not. RSSI check
			 *    for BTM message will fail (expected), as BTM does
			 *    not care about the same.
			 */
			blm_entry->poor_rssi = false;
			blm_entry->oce_assoc_reject = false;
			blm_entry->btm_bss_termination = false;
			blm_entry->btm_disassoc_imminent = false;
			blm_entry->btm_mbo_retry = false;
			blm_entry->no_more_stas = false;
			blm_entry->reassoc_rssi_reject = false;
			blm_entry->rssi_reject_list = false;
			blm_debug("Remove BSSID from rssi reject expected RSSI = %d, current RSSI = %d, retry delay required = %d ms, delay = %lu ms",
				  blm_entry->rssi_reject_params.expected_rssi,
				  scan_entry ? scan_entry->rssi_raw : 0,
				  blm_entry->rssi_reject_params.retry_delay,
				  entry_age);
			update_done = true;
		}
	}

	if (!update_done)
		return;

	blm_debug(QDF_MAC_ADDR_FMT" Old %d Updated reject ap type = %x",
		  QDF_MAC_ADDR_REF(blm_entry->bssid.bytes),
		  old_reject_ap_type,
		  blm_entry->reject_ap_type);
}

#define MAX_BL_TIME 255000

static enum cm_blacklist_action
blm_prune_old_entries_and_get_action(struct blm_reject_ap *blm_entry,
				     struct blm_config *cfg,
				     struct scan_cache_entry *entry,
				     qdf_list_t *reject_ap_list)
{
	blm_update_ap_info(blm_entry, cfg, entry);

	/*
	 * If all entities have cleared the bits of reject ap type, then
	 * the AP is not needed in the database,(reject_ap_type should be 0),
	 * then remove the entry from the reject ap list.
	 */
	if (!blm_entry->reject_ap_type) {
		blm_debug(QDF_MAC_ADDR_FMT" cleared from list",
			 QDF_MAC_ADDR_REF(blm_entry->bssid.bytes));
		qdf_list_remove_node(reject_ap_list, &blm_entry->node);
		qdf_mem_free(blm_entry);
		return CM_BLM_NO_ACTION;
	}

	if (BLM_IS_AP_IN_RSSI_REJECT_LIST(blm_entry) &&
	    !blm_entry->userspace_blacklist && !blm_entry->driver_blacklist &&
	    blm_entry->rssi_reject_params.original_timeout > MAX_BL_TIME) {
		blm_info("Allow BSSID "QDF_MAC_ADDR_FMT" as the retry delay is greater than %u ms, expected RSSI = %d, current RSSI = %d, retry delay = %u ms original timeout %u time added %lu source %d reason %d",
			 QDF_MAC_ADDR_REF(blm_entry->bssid.bytes), MAX_BL_TIME,
			 blm_entry->rssi_reject_params.expected_rssi,
			 entry ? entry->rssi_raw : 0,
			 blm_entry->rssi_reject_params.retry_delay,
			 blm_entry->rssi_reject_params.original_timeout,
			 blm_entry->rssi_reject_params.received_time,
			 blm_entry->source, blm_entry->reject_ap_reason);

		if (BLM_IS_AP_IN_AVOIDLIST(blm_entry)) {
			blm_debug(QDF_MAC_ADDR_FMT" in avoid list, deprioritize it",
				  QDF_MAC_ADDR_REF(blm_entry->bssid.bytes));
			return CM_BLM_AVOID;
		}

		return CM_BLM_NO_ACTION;
	}
	if (BLM_IS_AP_IN_BLACKLIST(blm_entry)) {
		blm_debug(QDF_MAC_ADDR_FMT" in blacklist list, reject ap type %d removing from candidate list",
			  QDF_MAC_ADDR_REF(blm_entry->bssid.bytes),
			  blm_entry->reject_ap_type);

		if (BLM_IS_AP_BLACKLISTED_BY_USERSPACE(blm_entry) ||
		    BLM_IS_AP_IN_RSSI_REJECT_LIST(blm_entry))
			return CM_BLM_FORCE_REMOVE;

		return CM_BLM_REMOVE;
	}

	if (BLM_IS_AP_IN_AVOIDLIST(blm_entry)) {
		blm_debug(QDF_MAC_ADDR_FMT" in avoid list, deprioritize it",
			  QDF_MAC_ADDR_REF(blm_entry->bssid.bytes));
		return CM_BLM_AVOID;
	}

	return CM_BLM_NO_ACTION;
}

static enum cm_blacklist_action
blm_action_on_bssid(struct wlan_objmgr_pdev *pdev,
		    struct scan_cache_entry *entry)
{
	struct blm_pdev_priv_obj *blm_ctx;
	struct blm_psoc_priv_obj *blm_psoc_obj;
	struct blm_config *cfg;
	struct blm_reject_ap *blm_entry = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	QDF_STATUS status;
	enum cm_blacklist_action action = CM_BLM_NO_ACTION;

	blm_ctx = blm_get_pdev_obj(pdev);
	blm_psoc_obj = blm_get_psoc_obj(wlan_pdev_get_psoc(pdev));
	if (!blm_ctx || !blm_psoc_obj) {
		blm_err("blm_ctx or blm_psoc_obj is NULL");
		return CM_BLM_NO_ACTION;
	}

	status = qdf_mutex_acquire(&blm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("failed to acquire reject_ap_list_lock");
		return CM_BLM_NO_ACTION;
	}

	cfg = &blm_psoc_obj->blm_cfg;

	qdf_list_peek_front(&blm_ctx->reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(&blm_ctx->reject_ap_list, cur_node,
				   &next_node);

		blm_entry = qdf_container_of(cur_node, struct blm_reject_ap,
					    node);

		if (qdf_is_macaddr_equal(&blm_entry->bssid, &entry->bssid)) {
			action = blm_prune_old_entries_and_get_action(blm_entry,
					cfg, entry, &blm_ctx->reject_ap_list);
			qdf_mutex_release(&blm_ctx->reject_ap_list_lock);
			return action;
		}
		cur_node = next_node;
		next_node = NULL;
	}
	qdf_mutex_release(&blm_ctx->reject_ap_list_lock);

	return CM_BLM_NO_ACTION;
}

enum cm_blacklist_action
wlan_blacklist_action_on_bssid(struct wlan_objmgr_pdev *pdev,
			       struct scan_cache_entry *entry)
{
	return blm_action_on_bssid(pdev, entry);
}

static void
blm_update_avoidlist_reject_reason(struct blm_reject_ap *entry,
				   enum blm_reject_ap_reason reject_reason)
{
	entry->nud_fail = false;
	entry->sta_kickout = false;
	entry->ho_fail = false;

	switch(reject_reason) {
	case REASON_NUD_FAILURE:
		entry->nud_fail = true;
		break;
	case REASON_STA_KICKOUT:
		entry->sta_kickout = true;
		break;
	case REASON_ROAM_HO_FAILURE:
		entry->ho_fail = true;
		break;
	default:
		blm_err("Invalid reason passed %d", reject_reason);
	}
}

static void
blm_handle_avoid_list(struct blm_reject_ap *entry,
		      struct blm_config *cfg,
		      struct reject_ap_info *ap_info)
{
	qdf_time_t cur_timestamp = qdf_mc_timer_get_system_time();

	if (ap_info->reject_ap_type == USERSPACE_AVOID_TYPE) {
		entry->userspace_avoidlist = true;
		entry->avoid_userspace = true;
		entry->ap_timestamp.userspace_avoid_timestamp = cur_timestamp;
	} else if (ap_info->reject_ap_type == DRIVER_AVOID_TYPE) {
		entry->driver_avoidlist = true;
		blm_update_avoidlist_reject_reason(entry,
						   ap_info->reject_reason);
		entry->ap_timestamp.driver_avoid_timestamp = cur_timestamp;
	} else {
		return;
	}
	entry->source = ap_info->source;
	/* Update bssid info for new entry */
	entry->bssid = ap_info->bssid;

	/* Clear the monitor list bit if the AP was present in monitor list */
	entry->driver_monitorlist = false;

	/* Increment bad bssid counter as NUD failure happenend with this ap */
	entry->bad_bssid_counter++;

	/* If bad bssid counter has reached threshold, move it to blacklist */
	if (entry->bad_bssid_counter >= cfg->bad_bssid_counter_thresh) {
		if (ap_info->reject_ap_type == USERSPACE_AVOID_TYPE)
			entry->userspace_avoidlist = false;
		else if (ap_info->reject_ap_type == DRIVER_AVOID_TYPE)
			entry->driver_avoidlist = false;

		/* Move AP to blacklist list */
		entry->driver_blacklist = true;
		entry->ap_timestamp.driver_blacklist_timestamp = cur_timestamp;

		blm_debug(QDF_MAC_ADDR_FMT" moved to black list with counter %d",
			  QDF_MAC_ADDR_REF(entry->bssid.bytes), entry->bad_bssid_counter);
		return;
	}
	blm_debug("Added "QDF_MAC_ADDR_FMT" to avoid list type %d, counter %d reason %d updated reject reason %d source %d",
		  QDF_MAC_ADDR_REF(entry->bssid.bytes), ap_info->reject_ap_type,
		  entry->bad_bssid_counter, ap_info->reject_reason,
		  entry->reject_ap_reason, entry->source);

	entry->connect_timestamp = qdf_mc_timer_get_system_time();
}

static void
blm_handle_blacklist(struct blm_reject_ap *entry,
		     struct reject_ap_info *ap_info)
{
	/*
	 * No entity will blacklist an AP internal to driver, so only
	 * userspace blacklist is the case to be taken care. Driver blacklist
	 * will only happen when the bad bssid counter has reached the max
	 * threshold.
	 */
	entry->bssid = ap_info->bssid;
	entry->userspace_blacklist = true;
	entry->ap_timestamp.userspace_blacklist_timestamp =
						qdf_mc_timer_get_system_time();

	entry->source = ADDED_BY_DRIVER;
	entry->blacklist_userspace = true;
	blm_debug(QDF_MAC_ADDR_FMT" added to userspace blacklist",
		  QDF_MAC_ADDR_REF(entry->bssid.bytes));
}

static void
blm_update_rssi_reject_reason(struct blm_reject_ap *entry,
			      enum blm_reject_ap_reason reject_reason)
{
	entry->poor_rssi = false;
	entry->oce_assoc_reject = false;
	entry->btm_bss_termination = false;
	entry->btm_disassoc_imminent = false;
	entry->btm_mbo_retry = false;
	entry->no_more_stas = false;
	entry->reassoc_rssi_reject = false;

	switch(reject_reason) {
	case REASON_ASSOC_REJECT_POOR_RSSI:
		entry->poor_rssi = true;
		break;
	case REASON_ASSOC_REJECT_OCE:
		entry->oce_assoc_reject = true;
		break;
	case REASON_BTM_DISASSOC_IMMINENT:
		entry->btm_disassoc_imminent = true;
		break;
	case REASON_BTM_BSS_TERMINATION:
		entry->btm_bss_termination = true;
		break;
	case REASON_BTM_MBO_RETRY:
		entry->btm_mbo_retry = true;
		break;
	case REASON_REASSOC_RSSI_REJECT:
		entry->reassoc_rssi_reject = true;
		break;
	case REASON_REASSOC_NO_MORE_STAS:
		entry->no_more_stas = true;
		break;
	default:
		blm_err("Invalid reason passed %d", reject_reason);
	}
}

static void
blm_handle_rssi_reject_list(struct blm_reject_ap *entry,
			    struct reject_ap_info *ap_info)
{
	bool bssid_newly_added;

	if (entry->rssi_reject_list) {
		bssid_newly_added = false;
	} else {
		entry->rssi_reject_params.source = ap_info->source;
		entry->bssid = ap_info->bssid;
		entry->rssi_reject_list = true;
		bssid_newly_added = true;
	}

	entry->ap_timestamp.rssi_reject_timestamp =
					qdf_mc_timer_get_system_time();
	entry->rssi_reject_params = ap_info->rssi_reject_params;
	blm_update_rssi_reject_reason(entry, ap_info->reject_reason);
	blm_info(QDF_MAC_ADDR_FMT" %s to rssi reject list, expected RSSI %d retry delay %u source %d original timeout %u received time %lu reject reason %d updated reason %d",
		 QDF_MAC_ADDR_REF(entry->bssid.bytes),
		 bssid_newly_added ? "ADDED" : "UPDATED",
		 entry->rssi_reject_params.expected_rssi,
		 entry->rssi_reject_params.retry_delay,
		 entry->rssi_reject_params.source,
		 entry->rssi_reject_params.original_timeout,
		 entry->rssi_reject_params.received_time,
		 ap_info->reject_reason, entry->reject_ap_reason);
}

static void
blm_modify_entry(struct blm_reject_ap *entry, struct blm_config *cfg,
		 struct reject_ap_info *ap_info)
{
	/* Modify the entry according to the ap_info */
	switch (ap_info->reject_ap_type) {
	case USERSPACE_AVOID_TYPE:
	case DRIVER_AVOID_TYPE:
		blm_handle_avoid_list(entry, cfg, ap_info);
		break;
	case USERSPACE_BLACKLIST_TYPE:
		blm_handle_blacklist(entry, ap_info);
		break;
	case DRIVER_RSSI_REJECT_TYPE:
		blm_handle_rssi_reject_list(entry, ap_info);
		break;
	default:
		blm_debug("Invalid input of ap type %d",
			  ap_info->reject_ap_type);
	}
}

static bool
blm_is_bssid_present_only_in_list_type(enum blm_reject_ap_type list_type,
				       struct blm_reject_ap *blm_entry)
{
	switch (list_type) {
	case USERSPACE_AVOID_TYPE:
		return IS_AP_IN_USERSPACE_AVOID_LIST_ONLY(blm_entry);
	case USERSPACE_BLACKLIST_TYPE:
		return IS_AP_IN_USERSPACE_BLACKLIST_ONLY(blm_entry);
	case DRIVER_AVOID_TYPE:
		return IS_AP_IN_DRIVER_AVOID_LIST_ONLY(blm_entry);
	case DRIVER_BLACKLIST_TYPE:
		return IS_AP_IN_DRIVER_BLACKLIST_ONLY(blm_entry);
	case DRIVER_RSSI_REJECT_TYPE:
		return IS_AP_IN_RSSI_REJECT_LIST_ONLY(blm_entry);
	case DRIVER_MONITOR_TYPE:
		return IS_AP_IN_MONITOR_LIST_ONLY(blm_entry);
	default:
		blm_debug("Wrong list type %d passed", list_type);
		return false;
	}
}

static bool
blm_is_bssid_of_type(enum blm_reject_ap_type reject_ap_type,
		     struct blm_reject_ap *blm_entry)
{
	switch (reject_ap_type) {
	case USERSPACE_AVOID_TYPE:
		return BLM_IS_AP_AVOIDED_BY_USERSPACE(blm_entry);
	case USERSPACE_BLACKLIST_TYPE:
		return BLM_IS_AP_BLACKLISTED_BY_USERSPACE(blm_entry);
	case DRIVER_AVOID_TYPE:
		return BLM_IS_AP_AVOIDED_BY_DRIVER(blm_entry);
	case DRIVER_BLACKLIST_TYPE:
		return BLM_IS_AP_BLACKLISTED_BY_DRIVER(blm_entry);
	case DRIVER_RSSI_REJECT_TYPE:
		return BLM_IS_AP_IN_RSSI_REJECT_LIST(blm_entry);
	case DRIVER_MONITOR_TYPE:
		return BLM_IS_AP_IN_MONITOR_LIST(blm_entry);
	default:
		blm_err("Wrong list type %d passed", reject_ap_type);
		return false;
	}
}

static qdf_time_t
blm_get_delta_of_bssid(enum blm_reject_ap_type list_type,
		       struct blm_reject_ap *blm_entry,
		       struct blm_config *cfg)
{
	qdf_time_t cur_timestamp = qdf_mc_timer_get_system_time();
	int32_t disallowed_time;
	/*
	 * For all the list types, delta would be the entry age only. Hence the
	 * oldest entry would be removed first in case of list is full, and the
	 * driver needs to make space for newer entries.
	 */

	switch (list_type) {
	case USERSPACE_AVOID_TYPE:
		return MINUTES_TO_MS(cfg->avoid_list_exipry_time) -
			(cur_timestamp -
			 blm_entry->ap_timestamp.userspace_avoid_timestamp);
	case USERSPACE_BLACKLIST_TYPE:
		return cur_timestamp -
			  blm_entry->ap_timestamp.userspace_blacklist_timestamp;
	case DRIVER_AVOID_TYPE:
		return MINUTES_TO_MS(cfg->avoid_list_exipry_time) -
			(cur_timestamp -
			 blm_entry->ap_timestamp.driver_avoid_timestamp);
	case DRIVER_BLACKLIST_TYPE:
		return MINUTES_TO_MS(cfg->black_list_exipry_time) -
			(cur_timestamp -
			 blm_entry->ap_timestamp.driver_blacklist_timestamp);

	/*
	 * For RSSI reject lowest delta would be the BSSID whose retry delay
	 * is about to expire, hence the delta would be remaining duration for
	 * de-blacklisting the AP from rssi reject list.
	 */
	case DRIVER_RSSI_REJECT_TYPE:
		if (blm_entry->rssi_reject_params.retry_delay)
			disallowed_time =
				blm_entry->rssi_reject_params.retry_delay -
				(cur_timestamp -
				blm_entry->ap_timestamp.rssi_reject_timestamp);
		else
			disallowed_time = (int32_t)(MINUTES_TO_MS(RSSI_TIMEOUT_VALUE) -
				(cur_timestamp -
				 blm_entry->ap_timestamp.rssi_reject_timestamp));
		return ((disallowed_time < 0) ? 0 : disallowed_time);
	case DRIVER_MONITOR_TYPE:
		return cur_timestamp -
			       blm_entry->ap_timestamp.driver_monitor_timestamp;
	default:
		blm_debug("Wrong list type %d passed", list_type);
		return 0;
	}
}

static bool
blm_is_oldest_entry(enum blm_reject_ap_type list_type,
		    qdf_time_t cur_node_delta,
		    qdf_time_t oldest_node_delta)
{
	switch (list_type) {
	/*
	 * For RSSI reject, userspace avoid, driver avoid/blacklist type the
	 * lowest retry delay has to be found out hence if oldest_node_delta is
	 * 0, mean this is the first entry and thus return true, If
	 * oldest_node_delta is non zero, compare the delta and return true if
	 * the cur entry has lower retry delta.
	 */
	case DRIVER_RSSI_REJECT_TYPE:
	case USERSPACE_AVOID_TYPE:
	case DRIVER_AVOID_TYPE:
	case DRIVER_BLACKLIST_TYPE:
		if (!oldest_node_delta || (cur_node_delta < oldest_node_delta))
			return true;
		break;
	case USERSPACE_BLACKLIST_TYPE:
	case DRIVER_MONITOR_TYPE:
		if (cur_node_delta > oldest_node_delta)
			return true;
		break;
	default:
		blm_debug("Wrong list type passed %d", list_type);
		return false;
	}

	return false;
}

static QDF_STATUS
blm_try_delete_bssid_in_list(qdf_list_t *reject_ap_list,
			     enum blm_reject_ap_type list_type,
			     struct blm_config *cfg)
{
	struct blm_reject_ap *blm_entry = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct blm_reject_ap *oldest_blm_entry = NULL;
	qdf_time_t oldest_node_delta = 0;
	qdf_time_t cur_node_delta = 0;

	qdf_list_peek_front(reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(reject_ap_list, cur_node, &next_node);

		blm_entry = qdf_container_of(cur_node, struct blm_reject_ap,
					    node);

		if (blm_is_bssid_present_only_in_list_type(list_type,
							   blm_entry)) {
			cur_node_delta = blm_get_delta_of_bssid(list_type,
								blm_entry, cfg);

			if (blm_is_oldest_entry(list_type, cur_node_delta,
						oldest_node_delta)) {
				/* now this is the oldest entry*/
				oldest_blm_entry = blm_entry;
				oldest_node_delta = cur_node_delta;
			}
		}
		cur_node = next_node;
		next_node = NULL;
	}

	if (oldest_blm_entry) {
		/* Remove this entry to make space for the next entry */
		blm_debug("Removed "QDF_MAC_ADDR_FMT", type = %d",
			  QDF_MAC_ADDR_REF(oldest_blm_entry->bssid.bytes),
			  list_type);
		qdf_list_remove_node(reject_ap_list, &oldest_blm_entry->node);
		qdf_mem_free(oldest_blm_entry);
		return QDF_STATUS_SUCCESS;
	}
	/* If the flow has reached here, that means no entry could be removed */

	return QDF_STATUS_E_FAILURE;
}

static QDF_STATUS
blm_remove_lowest_delta_entry(qdf_list_t *reject_ap_list,
			      struct blm_config *cfg)
{
	QDF_STATUS status;

	/*
	 * According to the Priority, the driver will try to remove the entries,
	 * as the least priority list, that is monitor list would not penalize
	 * the BSSIDs for connection. The priority order for the removal is:-
	 * 1. Monitor list
	 * 2. Driver avoid list
	 * 3. Userspace avoid list.
	 * 4. RSSI reject list.
	 * 5. Driver Blacklist.
	 * 6. Userspace Blacklist.
	 */

	status = blm_try_delete_bssid_in_list(reject_ap_list,
					      DRIVER_MONITOR_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	status = blm_try_delete_bssid_in_list(reject_ap_list,
					      DRIVER_AVOID_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	status = blm_try_delete_bssid_in_list(reject_ap_list,
					      USERSPACE_AVOID_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	status = blm_try_delete_bssid_in_list(reject_ap_list,
					      DRIVER_RSSI_REJECT_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	status = blm_try_delete_bssid_in_list(reject_ap_list,
					      DRIVER_BLACKLIST_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	status = blm_try_delete_bssid_in_list(reject_ap_list,
					      USERSPACE_BLACKLIST_TYPE, cfg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	blm_debug("Failed to remove AP from blacklist manager");

	return QDF_STATUS_E_FAILURE;
}

static enum blm_reject_ap_reason
blm_get_rssi_reject_reason(struct blm_reject_ap *blm_entry)
{
	if (blm_entry->poor_rssi)
		return REASON_ASSOC_REJECT_POOR_RSSI;
	else if (blm_entry->oce_assoc_reject)
		return REASON_ASSOC_REJECT_OCE;
	else if(blm_entry->btm_bss_termination)
		return REASON_BTM_BSS_TERMINATION;
	else if (blm_entry->btm_disassoc_imminent)
		return REASON_BTM_DISASSOC_IMMINENT;
	else if (blm_entry->btm_mbo_retry)
		return REASON_BTM_MBO_RETRY;
	else if (blm_entry->no_more_stas)
		return REASON_REASSOC_NO_MORE_STAS;
	else if (blm_entry->reassoc_rssi_reject)
		return REASON_REASSOC_RSSI_REJECT;

	return REASON_UNKNOWN;
}

static void
blm_fill_rssi_reject_params(struct blm_reject_ap *blm_entry,
			    enum blm_reject_ap_type reject_ap_type,
			    struct reject_ap_config_params *blm_reject_list)
{
	if (reject_ap_type != DRIVER_RSSI_REJECT_TYPE)
		return;

	blm_reject_list->source = blm_entry->rssi_reject_params.source;
	blm_reject_list->original_timeout =
			blm_entry->rssi_reject_params.original_timeout;
	blm_reject_list->received_time =
			blm_entry->rssi_reject_params.received_time;
	blm_reject_list->reject_reason = blm_get_rssi_reject_reason(blm_entry);
	blm_debug(QDF_MAC_ADDR_FMT" source %d original timeout %u received time %lu reject reason %d",
		   QDF_MAC_ADDR_REF(blm_entry->bssid.bytes), blm_reject_list->source,
		   blm_reject_list->original_timeout,
		   blm_reject_list->received_time,
		   blm_reject_list->reject_reason);
}

/**
 * blm_find_reject_type_string() - Function to convert int to string
 * @reject_ap_type:   blm_reject_ap_type
 *
 * This function is used to convert int value of enum blm_reject_ap_type
 * to string format.
 *
 * Return: String
 *
 */
static const char *
blm_find_reject_type_string(enum blm_reject_ap_type reject_ap_type)
{
	switch (reject_ap_type) {
	CASE_RETURN_STRING(USERSPACE_AVOID_TYPE);
	CASE_RETURN_STRING(USERSPACE_BLACKLIST_TYPE);
	CASE_RETURN_STRING(DRIVER_AVOID_TYPE);
	CASE_RETURN_STRING(DRIVER_BLACKLIST_TYPE);
	CASE_RETURN_STRING(DRIVER_RSSI_REJECT_TYPE);
	CASE_RETURN_STRING(DRIVER_MONITOR_TYPE);
	default:
		return "REJECT_REASON_UNKNOWN";
	}
}

/**
 * blm_get_reject_ap_type() - Function to find reject ap type
 * @blm_entry:   blm_reject_ap
 *
 * This function is used to get reject ap type.
 *
 * Return: blm_reject_ap_type
 *
 */
static enum blm_reject_ap_type
blm_get_reject_ap_type(struct blm_reject_ap *blm_entry)
{
	if (BLM_IS_AP_AVOIDED_BY_USERSPACE(blm_entry))
		return USERSPACE_AVOID_TYPE;
	if (BLM_IS_AP_BLACKLISTED_BY_USERSPACE(blm_entry))
		return USERSPACE_BLACKLIST_TYPE;
	if (BLM_IS_AP_AVOIDED_BY_DRIVER(blm_entry))
		return DRIVER_AVOID_TYPE;
	if (BLM_IS_AP_BLACKLISTED_BY_DRIVER(blm_entry))
		return DRIVER_BLACKLIST_TYPE;
	if (BLM_IS_AP_IN_RSSI_REJECT_LIST(blm_entry))
		return DRIVER_RSSI_REJECT_TYPE;
	if (BLM_IS_AP_IN_MONITOR_LIST(blm_entry))
		return DRIVER_MONITOR_TYPE;

	return REJECT_REASON_UNKNOWN;
}

/**
 * blm_dump_blacklist_bssid() - Function to dump blacklisted bssid
 * @pdev:  pdev object
 *
 * This function is used to dump blacklisted bssid along with reject
 * ap type, source, delay and required rssi
 *
 * Return: None
 *
 */
void blm_dump_blacklist_bssid(struct wlan_objmgr_pdev *pdev)
{
	struct blm_reject_ap *blm_entry = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	struct blm_pdev_priv_obj *blm_ctx;
	struct blm_psoc_priv_obj *blm_psoc_obj;
	uint32_t reject_duration;
	enum blm_reject_ap_type reject_ap_type;
	qdf_list_t *reject_db_list;
	QDF_STATUS status;

	blm_ctx = blm_get_pdev_obj(pdev);
	blm_psoc_obj = blm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!blm_ctx || !blm_psoc_obj) {
		blm_err("blm_ctx or blm_psoc_obj is NULL");
		return;
	}

	status = qdf_mutex_acquire(&blm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("failed to acquire reject_ap_list_lock");
		return;
	}

	reject_db_list = &blm_ctx->reject_ap_list;
	qdf_list_peek_front(reject_db_list, &cur_node);
	while (cur_node) {
		qdf_list_peek_next(reject_db_list, cur_node, &next_node);

		blm_entry = qdf_container_of(cur_node, struct blm_reject_ap,
					     node);

		reject_ap_type = blm_get_reject_ap_type(blm_entry);

		reject_duration = blm_get_delta_of_bssid(
						reject_ap_type, blm_entry,
						&blm_psoc_obj->blm_cfg);

			blm_nofl_debug("BLACKLIST BSSID "QDF_MAC_ADDR_FMT" type %s retry delay %dms expected RSSI %d reject reason %d rejection source %d",
				QDF_MAC_ADDR_REF(blm_entry->bssid.bytes),
				blm_find_reject_type_string(reject_ap_type),
				reject_duration,
				blm_entry->rssi_reject_params.expected_rssi,
				blm_entry->reject_ap_reason,
				blm_entry->rssi_reject_params.source);

		cur_node = next_node;
		next_node = NULL;
	}

	qdf_mutex_release(&blm_ctx->reject_ap_list_lock);
}

static void blm_fill_reject_list(qdf_list_t *reject_db_list,
				 struct reject_ap_config_params *reject_list,
				 uint8_t *num_of_reject_bssid,
				 enum blm_reject_ap_type reject_ap_type,
				 uint8_t max_bssid_to_be_filled,
				 struct blm_config *cfg)
{
	struct blm_reject_ap *blm_entry = NULL;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;

	qdf_list_peek_front(reject_db_list, &cur_node);
	while (cur_node) {
		if (*num_of_reject_bssid == max_bssid_to_be_filled) {
			blm_debug("Max size reached in list, reject_ap_type %d",
				  reject_ap_type);
			return;
		}
		qdf_list_peek_next(reject_db_list, cur_node, &next_node);

		blm_entry = qdf_container_of(cur_node, struct blm_reject_ap,
					    node);

		blm_update_ap_info(blm_entry, cfg, NULL);
		if (!blm_entry->reject_ap_type) {
			blm_debug(QDF_MAC_ADDR_FMT" cleared from list",
				  QDF_MAC_ADDR_REF(blm_entry->bssid.bytes));
			qdf_list_remove_node(reject_db_list, &blm_entry->node);
			qdf_mem_free(blm_entry);
			cur_node = next_node;
			next_node = NULL;
			continue;
		}

		if (blm_is_bssid_of_type(reject_ap_type, blm_entry)) {
			struct reject_ap_config_params *blm_reject_list;

			blm_reject_list = &reject_list[*num_of_reject_bssid];
			blm_reject_list->expected_rssi =
				    blm_entry->rssi_reject_params.expected_rssi;
			blm_reject_list->reject_duration =
			       blm_get_delta_of_bssid(reject_ap_type, blm_entry,
						      cfg);

			blm_fill_rssi_reject_params(blm_entry, reject_ap_type,
						    blm_reject_list);
			blm_reject_list->reject_ap_type = reject_ap_type;
			blm_reject_list->bssid = blm_entry->bssid;
			(*num_of_reject_bssid)++;
			blm_debug("Adding BSSID "QDF_MAC_ADDR_FMT" of type %d retry delay %d expected RSSI %d, entries added = %d reject reason %d",
				  QDF_MAC_ADDR_REF(blm_entry->bssid.bytes),
				  reject_ap_type,
				  reject_list[*num_of_reject_bssid -1].reject_duration,
				  blm_entry->rssi_reject_params.expected_rssi,
				  *num_of_reject_bssid,
				  blm_entry->reject_ap_reason);
		}
		cur_node = next_node;
		next_node = NULL;
	}
}

#if defined(WLAN_FEATURE_ROAM_OFFLOAD)
void blm_update_reject_ap_list_to_fw(struct wlan_objmgr_psoc *psoc)
{
	struct blm_config *cfg;
	struct wlan_objmgr_pdev *pdev;
	struct blm_pdev_priv_obj *blm_ctx;
	struct blm_psoc_priv_obj *blm_psoc_obj;
	QDF_STATUS status;

	blm_psoc_obj = blm_get_psoc_obj(psoc);
	if (!blm_psoc_obj) {
		blm_err("BLM psoc obj NULL");
		return;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, blm_psoc_obj->pdev_id,
					  WLAN_MLME_CM_ID);
	if (!pdev) {
		blm_err("pdev obj NULL");
		return;
	}

	blm_ctx = blm_get_pdev_obj(pdev);
	if (!blm_ctx) {
		blm_err("BLM pdev obj NULL");
		goto end;
	}

	status = qdf_mutex_acquire(&blm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("failed to acquire reject_ap_list_lock");
		goto end;
	}

	cfg = &blm_psoc_obj->blm_cfg;
	blm_send_reject_ap_list_to_fw(pdev, &blm_ctx->reject_ap_list, cfg);
	qdf_mutex_release(&blm_ctx->reject_ap_list_lock);

end:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_CM_ID);
}

static void blm_store_pdevid_in_blm_psocpriv(struct wlan_objmgr_pdev *pdev)
{
	struct blm_psoc_priv_obj *blm_psoc_obj;

	blm_psoc_obj = blm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!blm_psoc_obj) {
		blm_err("BLM psoc obj NULL");
		return;
	}

	blm_psoc_obj->pdev_id = pdev->pdev_objmgr.wlan_pdev_id;
}

void
blm_send_reject_ap_list_to_fw(struct wlan_objmgr_pdev *pdev,
			      qdf_list_t *reject_db_list,
			      struct blm_config *cfg)
{
	QDF_STATUS status;
	bool is_blm_suspended;
	struct reject_ap_params reject_params = {0};

	ucfg_blm_psoc_get_suspended(wlan_pdev_get_psoc(pdev),
				    &is_blm_suspended);
	if (is_blm_suspended) {
		blm_store_pdevid_in_blm_psocpriv(pdev);
		blm_debug("Failed to send reject AP list to FW as BLM is suspended");
		return;
	}

	reject_params.bssid_list =
			qdf_mem_malloc(sizeof(*reject_params.bssid_list) *
				       PDEV_MAX_NUM_BSSID_DISALLOW_LIST);
	if (!reject_params.bssid_list)
		return;

	/* The priority for filling is as below */
	blm_fill_reject_list(reject_db_list, reject_params.bssid_list,
			     &reject_params.num_of_reject_bssid,
			     USERSPACE_BLACKLIST_TYPE,
			     PDEV_MAX_NUM_BSSID_DISALLOW_LIST, cfg);
	blm_fill_reject_list(reject_db_list, reject_params.bssid_list,
			     &reject_params.num_of_reject_bssid,
			     DRIVER_BLACKLIST_TYPE,
			     PDEV_MAX_NUM_BSSID_DISALLOW_LIST, cfg);
	blm_fill_reject_list(reject_db_list, reject_params.bssid_list,
			     &reject_params.num_of_reject_bssid,
			     DRIVER_RSSI_REJECT_TYPE,
			     PDEV_MAX_NUM_BSSID_DISALLOW_LIST, cfg);
	blm_fill_reject_list(reject_db_list, reject_params.bssid_list,
			     &reject_params.num_of_reject_bssid,
			     USERSPACE_AVOID_TYPE,
			     PDEV_MAX_NUM_BSSID_DISALLOW_LIST, cfg);
	blm_fill_reject_list(reject_db_list, reject_params.bssid_list,
			     &reject_params.num_of_reject_bssid,
			     DRIVER_AVOID_TYPE,
			     PDEV_MAX_NUM_BSSID_DISALLOW_LIST, cfg);

	status = tgt_blm_send_reject_list_to_fw(pdev, &reject_params);

	if (QDF_IS_STATUS_ERROR(status))
		blm_err("failed to send the reject Ap list to FW");

	qdf_mem_free(reject_params.bssid_list);
}
#endif

QDF_STATUS
blm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
			     struct reject_ap_info *ap_info)
{
	struct blm_pdev_priv_obj *blm_ctx;
	struct blm_psoc_priv_obj *blm_psoc_obj;
	struct blm_config *cfg;
	struct blm_reject_ap *blm_entry;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	QDF_STATUS status;

	blm_ctx = blm_get_pdev_obj(pdev);
	blm_psoc_obj = blm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!blm_ctx || !blm_psoc_obj) {
		blm_err("blm_ctx or blm_psoc_obj is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (qdf_is_macaddr_zero(&ap_info->bssid) ||
	    qdf_is_macaddr_group(&ap_info->bssid)) {
		blm_err("Zero/Broadcast BSSID received, entry not added");
		return QDF_STATUS_E_INVAL;
	}

	status = qdf_mutex_acquire(&blm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("failed to acquire reject_ap_list_lock");
		return status;
	}

	cfg = &blm_psoc_obj->blm_cfg;

	qdf_list_peek_front(&blm_ctx->reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(&blm_ctx->reject_ap_list,
				   cur_node, &next_node);

		blm_entry = qdf_container_of(cur_node, struct blm_reject_ap,
					    node);

		/* Update the AP info to the latest list first */
		blm_update_ap_info(blm_entry, cfg, NULL);
		if (!blm_entry->reject_ap_type) {
			blm_debug(QDF_MAC_ADDR_FMT" cleared from list",
				  QDF_MAC_ADDR_REF(blm_entry->bssid.bytes));
			qdf_list_remove_node(&blm_ctx->reject_ap_list,
					     &blm_entry->node);
			qdf_mem_free(blm_entry);
			cur_node = next_node;
			next_node = NULL;
			continue;
		}

		if (qdf_is_macaddr_equal(&blm_entry->bssid, &ap_info->bssid)) {
			blm_modify_entry(blm_entry, cfg, ap_info);
			goto end;
		}

		cur_node = next_node;
		next_node = NULL;
	}

	if (qdf_list_size(&blm_ctx->reject_ap_list) == MAX_BAD_AP_LIST_SIZE) {
		/* List is FULL, need to delete entries */
		status =
			blm_remove_lowest_delta_entry(&blm_ctx->reject_ap_list,
						      cfg);

		if (QDF_IS_STATUS_ERROR(status)) {
			qdf_mutex_release(&blm_ctx->reject_ap_list_lock);
			return status;
		}
	}

	blm_entry = qdf_mem_malloc(sizeof(*blm_entry));
	if (!blm_entry) {
		qdf_mutex_release(&blm_ctx->reject_ap_list_lock);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_list_insert_back(&blm_ctx->reject_ap_list, &blm_entry->node);
	blm_modify_entry(blm_entry, cfg, ap_info);

end:
	blm_send_reject_ap_list_to_fw(pdev, &blm_ctx->reject_ap_list, cfg);
	qdf_mutex_release(&blm_ctx->reject_ap_list_lock);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
blm_clear_userspace_blacklist_info(struct wlan_objmgr_pdev *pdev)
{
	struct blm_pdev_priv_obj *blm_ctx;
	struct blm_reject_ap *blm_entry;
	QDF_STATUS status;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;

	blm_ctx = blm_get_pdev_obj(pdev);
	if (!blm_ctx) {
		blm_err("blm_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	status = qdf_mutex_acquire(&blm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("failed to acquire reject_ap_list_lock");
		return QDF_STATUS_E_RESOURCES;
	}

	qdf_list_peek_front(&blm_ctx->reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(&blm_ctx->reject_ap_list, cur_node,
				   &next_node);
		blm_entry = qdf_container_of(cur_node, struct blm_reject_ap,
					    node);

		if (IS_AP_IN_USERSPACE_BLACKLIST_ONLY(blm_entry)) {
			blm_debug("removing bssid: "QDF_MAC_ADDR_FMT,
				 QDF_MAC_ADDR_REF(blm_entry->bssid.bytes));
			qdf_list_remove_node(&blm_ctx->reject_ap_list,
					     &blm_entry->node);
			qdf_mem_free(blm_entry);
		} else if (BLM_IS_AP_BLACKLISTED_BY_USERSPACE(blm_entry)) {
			blm_debug("Clearing userspace blacklist bit for "QDF_MAC_ADDR_FMT,
				  QDF_MAC_ADDR_REF(blm_entry->bssid.bytes));
			blm_entry->userspace_blacklist = false;
			blm_entry->blacklist_userspace = false;
		}
		cur_node = next_node;
		next_node = NULL;
	}
	qdf_mutex_release(&blm_ctx->reject_ap_list_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
blm_add_userspace_black_list(struct wlan_objmgr_pdev *pdev,
			     struct qdf_mac_addr *bssid_black_list,
			     uint8_t num_of_bssid)
{
	uint8_t i = 0;
	struct reject_ap_info ap_info;
	QDF_STATUS status;
	struct blm_pdev_priv_obj *blm_ctx;
	struct blm_psoc_priv_obj *blm_psoc_obj;
	struct blm_config *cfg;

	blm_ctx = blm_get_pdev_obj(pdev);
	blm_psoc_obj = blm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!blm_ctx || !blm_psoc_obj) {
		blm_err("blm_ctx or blm_psoc_obj is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/* Clear all the info of APs already existing in BLM first */
	blm_clear_userspace_blacklist_info(pdev);
	cfg = &blm_psoc_obj->blm_cfg;

	status = qdf_mutex_acquire(&blm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("failed to acquire reject_ap_list_lock");
		return status;
	}

	blm_send_reject_ap_list_to_fw(pdev, &blm_ctx->reject_ap_list, cfg);
	qdf_mutex_release(&blm_ctx->reject_ap_list_lock);

	if (!bssid_black_list || !num_of_bssid) {
		blm_debug("Userspace blacklist/num of blacklist NULL");
		return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < num_of_bssid; i++) {
		ap_info.bssid = bssid_black_list[i];
		ap_info.reject_ap_type = USERSPACE_BLACKLIST_TYPE;
		ap_info.source = ADDED_BY_DRIVER;
		ap_info.reject_reason = REASON_USERSPACE_BL;
		status = blm_add_bssid_to_reject_list(pdev, &ap_info);
		if (QDF_IS_STATUS_ERROR(status)) {
			blm_err("Failed to add bssid to userspace blacklist");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

void
blm_flush_reject_ap_list(struct blm_pdev_priv_obj *blm_ctx)
{
	struct blm_reject_ap *blm_entry = NULL;
	QDF_STATUS status;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;

	status = qdf_mutex_acquire(&blm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("failed to acquire reject_ap_list_lock");
		return;
	}

	qdf_list_peek_front(&blm_ctx->reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(&blm_ctx->reject_ap_list, cur_node,
				   &next_node);
		blm_entry = qdf_container_of(cur_node, struct blm_reject_ap,
					    node);
		qdf_list_remove_node(&blm_ctx->reject_ap_list,
				     &blm_entry->node);
		qdf_mem_free(blm_entry);
		cur_node = next_node;
		next_node = NULL;
	}

	blm_debug("BLM reject ap list flushed");
	qdf_mutex_release(&blm_ctx->reject_ap_list_lock);
}

uint8_t
blm_get_bssid_reject_list(struct wlan_objmgr_pdev *pdev,
			  struct reject_ap_config_params *reject_list,
			  uint8_t max_bssid_to_be_filled,
			  enum blm_reject_ap_type reject_ap_type)
{
	struct blm_pdev_priv_obj *blm_ctx;
	struct blm_psoc_priv_obj *blm_psoc_obj;
	uint8_t num_of_reject_bssid = 0;
	QDF_STATUS status;

	blm_ctx = blm_get_pdev_obj(pdev);
	blm_psoc_obj = blm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!blm_ctx || !blm_psoc_obj) {
		blm_err("blm_ctx or blm_psoc_obj is NULL");
		return 0;
	}

	status = qdf_mutex_acquire(&blm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("failed to acquire reject_ap_list_lock");
		return 0;
	}

	blm_fill_reject_list(&blm_ctx->reject_ap_list, reject_list,
			     &num_of_reject_bssid, reject_ap_type,
			     max_bssid_to_be_filled, &blm_psoc_obj->blm_cfg);

	qdf_mutex_release(&blm_ctx->reject_ap_list_lock);

	return num_of_reject_bssid;
}

void
blm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				struct qdf_mac_addr bssid,
				enum blm_connection_state con_state)
{
	struct blm_pdev_priv_obj *blm_ctx;
	struct blm_psoc_priv_obj *blm_psoc_obj;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	QDF_STATUS status;
	struct blm_reject_ap *blm_entry = NULL;
	qdf_time_t connection_age = 0;
	bool entry_found = false;
	qdf_time_t max_entry_time;

	blm_ctx = blm_get_pdev_obj(pdev);
	blm_psoc_obj = blm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!blm_ctx || !blm_psoc_obj) {
		blm_err("blm_ctx or blm_psoc_obj is NULL");
		return;
	}

	status = qdf_mutex_acquire(&blm_ctx->reject_ap_list_lock);
	if (QDF_IS_STATUS_ERROR(status)) {
		blm_err("failed to acquire reject_ap_list_lock");
		return;
	}

	qdf_list_peek_front(&blm_ctx->reject_ap_list, &cur_node);

	while (cur_node) {
		qdf_list_peek_next(&blm_ctx->reject_ap_list, cur_node,
				   &next_node);
		blm_entry = qdf_container_of(cur_node, struct blm_reject_ap,
					     node);

		if (!qdf_mem_cmp(blm_entry->bssid.bytes, bssid.bytes,
				 QDF_MAC_ADDR_SIZE)) {
			blm_debug(QDF_MAC_ADDR_FMT" present in BLM reject list, updating connect info con_state = %d",
				  QDF_MAC_ADDR_REF(blm_entry->bssid.bytes),
				  con_state);
			entry_found = true;
			break;
		}
		cur_node = next_node;
		next_node = NULL;
	}

	/* This means that the BSSID was not added in the reject list of BLM */
	if (!entry_found) {
		qdf_mutex_release(&blm_ctx->reject_ap_list_lock);
		return;
	}
	switch (con_state) {
	case BLM_AP_CONNECTED:
		blm_entry->connect_timestamp = qdf_mc_timer_get_system_time();
		break;
	case BLM_AP_DISCONNECTED:
		/* Update the blm info first */
		blm_update_ap_info(blm_entry, &blm_psoc_obj->blm_cfg, NULL);

		max_entry_time = blm_entry->connect_timestamp;
		if (blm_entry->driver_blacklist) {
			max_entry_time =
			   blm_entry->ap_timestamp.driver_blacklist_timestamp;
		} else if (blm_entry->driver_avoidlist) {
			max_entry_time =
			 QDF_MAX(blm_entry->ap_timestamp.driver_avoid_timestamp,
				 blm_entry->connect_timestamp);
		}
		connection_age = qdf_mc_timer_get_system_time() -
							max_entry_time;
		if ((connection_age >
		     SECONDS_TO_MS(blm_psoc_obj->blm_cfg.
				   bad_bssid_counter_reset_time))) {
			blm_entry->driver_avoidlist = false;
			blm_entry->driver_blacklist = false;
			blm_entry->driver_monitorlist = false;
			blm_entry->userspace_avoidlist = false;
			blm_debug("updated reject ap type %d ",
				  blm_entry->reject_ap_type);
			if (!blm_entry->reject_ap_type) {
				blm_debug("Bad Bssid timer expired/AP cleared from all blacklisting, removed "QDF_MAC_ADDR_FMT" from list",
					  QDF_MAC_ADDR_REF(blm_entry->bssid.bytes));
				qdf_list_remove_node(&blm_ctx->reject_ap_list,
						     &blm_entry->node);
				qdf_mem_free(blm_entry);
				blm_send_reject_ap_list_to_fw(pdev,
					&blm_ctx->reject_ap_list,
					&blm_psoc_obj->blm_cfg);
			}
		}
		break;
	default:
		blm_debug("Invalid AP connection state recevied %d", con_state);
	};

	qdf_mutex_release(&blm_ctx->reject_ap_list_lock);
}

int32_t blm_get_rssi_blacklist_threshold(struct wlan_objmgr_pdev *pdev)
{
	struct blm_pdev_priv_obj *blm_ctx;
	struct blm_psoc_priv_obj *blm_psoc_obj;
	struct blm_config *cfg;

	blm_ctx = blm_get_pdev_obj(pdev);
	blm_psoc_obj = blm_get_psoc_obj(wlan_pdev_get_psoc(pdev));

	if (!blm_ctx || !blm_psoc_obj) {
		blm_err("blm_ctx or blm_psoc_obj is NULL");
		return 0;
	}

	cfg = &blm_psoc_obj->blm_cfg;
	return cfg->delta_rssi;
}

