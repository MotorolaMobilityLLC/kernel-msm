/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

/*
 * DOC: wlan_cm_roam_offload.c
 *
 * Implementation for the common roaming offload api interfaces.
 */

#include "wlan_mlme_main.h"
#include "wlan_cm_roam_offload.h"
#include "wlan_cm_tgt_if_tx_api.h"
#include "wlan_cm_roam_api.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include "wlan_crypto_global_api.h"
#include "wlan_roam_debug.h"

/**
 * cm_roam_scan_bmiss_cnt() - set roam beacon miss count
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @params: roam beacon miss count parameters
 *
 * This function is used to set roam beacon miss count parameters
 *
 * Return: None
 */
static void
cm_roam_scan_bmiss_cnt(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		       struct wlan_roam_beacon_miss_cnt *params)
{
	uint8_t beacon_miss_count;

	params->vdev_id = vdev_id;

	wlan_mlme_get_roam_bmiss_first_bcnt(psoc, &beacon_miss_count);
	params->roam_bmiss_first_bcnt = beacon_miss_count;

	wlan_mlme_get_roam_bmiss_final_bcnt(psoc, &beacon_miss_count);
	params->roam_bmiss_final_bcnt = beacon_miss_count;
}

QDF_STATUS
cm_roam_fill_rssi_change_params(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				struct wlan_roam_rssi_change_params *params)
{
	struct cm_roam_values_copy temp;

	params->vdev_id = vdev_id;
	wlan_cm_roam_cfg_get_value(psoc, vdev_id,
				   RSSI_CHANGE_THRESHOLD, &temp);
	params->rssi_change_thresh = temp.int_value;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id,
				   BEACON_RSSI_WEIGHT, &temp);
	params->bcn_rssi_weight = temp.uint_value;

	wlan_cm_roam_cfg_get_value(psoc, vdev_id,
				   HI_RSSI_DELAY_BTW_SCANS, &temp);
	params->hirssi_delay_btw_scans = temp.uint_value;

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_is_per_roam_allowed()  - Check if PER roam trigger needs to be
 * disabled based on the current connected rates.
 * @psoc:   Pointer to the psoc object
 * @vdev_id: Vdev id
 *
 * Return: true if PER roam trigger is allowed
 */
static bool
cm_roam_is_per_roam_allowed(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct qdf_mac_addr connected_bssid = {0};
	struct wlan_objmgr_vdev *vdev;
	enum wlan_phymode peer_phymode = WLAN_PHYMODE_AUTO;
	QDF_STATUS status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_CM_ID);
	if (!vdev) {
		mlme_err("Vdev is null for vdev_id:%d", vdev_id);
		return false;
	}

	status = wlan_vdev_get_bss_peer_mac(vdev, &connected_bssid);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_CM_ID);

	if (QDF_IS_STATUS_ERROR(status))
		return false;

	mlme_get_peer_phymode(psoc, connected_bssid.bytes, &peer_phymode);
	if (peer_phymode < WLAN_PHYMODE_11NA_HT20) {
		mlme_debug("vdev:%d PER roam trigger disabled for phymode:%d",
			   peer_phymode, vdev_id);
		return false;
	}

	return true;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * cm_roam_reason_vsie() - set roam reason vsie
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @params: roam reason vsie parameters
 *
 * This function is used to set roam reason vsie parameters
 *
 * Return: None
 */
static void
cm_roam_reason_vsie(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		    struct wlan_roam_reason_vsie_enable *params)
{
	uint8_t enable_roam_reason_vsie;

	params->vdev_id = vdev_id;

	wlan_mlme_get_roam_reason_vsie_status(psoc, &enable_roam_reason_vsie);
	params->enable_roam_reason_vsie = enable_roam_reason_vsie;
}

/**
 * cm_roam_triggers() - set roam triggers
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @params: roam triggers parameters
 *
 * This function is used to set roam triggers parameters
 *
 * Return: None
 */
static void
cm_roam_triggers(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		 struct wlan_roam_triggers *params)
{
	bool is_per_roam_enabled;

	params->vdev_id = vdev_id;
	params->trigger_bitmap =
		mlme_get_roam_trigger_bitmap(psoc, vdev_id);

	/*
	 * Disable PER trigger for phymode less than 11n to avoid
	 * frequent roams as the PER rate threshold is greater than
	 * 11a/b/g rates
	 */
	is_per_roam_enabled = cm_roam_is_per_roam_allowed(psoc, vdev_id);
	if (!is_per_roam_enabled)
		params->trigger_bitmap &= ~BIT(ROAM_TRIGGER_REASON_PER);

	params->roam_scan_scheme_bitmap =
		wlan_cm_get_roam_scan_scheme_bitmap(psoc, vdev_id);
	wlan_cm_roam_get_vendor_btm_params(psoc, &params->vendor_btm_param);
	wlan_cm_roam_get_score_delta_params(psoc, params);
	wlan_cm_roam_get_min_rssi_params(psoc, params);
}

/**
 * cm_roam_bss_load_config() - set bss load config
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @params: bss load config parameters
 *
 * This function is used to set bss load config parameters
 *
 * Return: None
 */
static void
cm_roam_bss_load_config(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			struct wlan_roam_bss_load_config *params)
{
	params->vdev_id = vdev_id;
	wlan_mlme_get_bss_load_threshold(psoc, &params->bss_load_threshold);
	wlan_mlme_get_bss_load_sample_time(psoc, &params->bss_load_sample_time);
	wlan_mlme_get_bss_load_rssi_threshold_5ghz(
					psoc, &params->rssi_threshold_5ghz);
	wlan_mlme_get_bss_load_rssi_threshold_24ghz(
					psoc, &params->rssi_threshold_24ghz);
}

/**
 * cm_roam_disconnect_params() - set disconnect roam parameters
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @params: disconnect roam parameters
 *
 * This function is used to set disconnect roam parameters
 *
 * Return: None
 */
static void
cm_roam_disconnect_params(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			  struct wlan_roam_disconnect_params *params)
{
	params->vdev_id = vdev_id;
	wlan_mlme_get_enable_disconnect_roam_offload(psoc, &params->enable);
}

/**
 * cm_roam_idle_params() - set roam idle parameters
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @params: roam idle parameters
 *
 * This function is used to set roam idle parameters
 *
 * Return: None
 */
static void
cm_roam_idle_params(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		    struct wlan_roam_idle_params *params)
{
	params->vdev_id = vdev_id;
	wlan_mlme_get_enable_idle_roam(psoc, &params->enable);
	wlan_mlme_get_idle_roam_rssi_delta(psoc, &params->conn_ap_rssi_delta);
	wlan_mlme_get_idle_roam_inactive_time(psoc, &params->inactive_time);
	wlan_mlme_get_idle_data_packet_count(psoc, &params->data_pkt_count);
	wlan_mlme_get_idle_roam_min_rssi(psoc, &params->conn_ap_min_rssi);
	wlan_mlme_get_idle_roam_band(psoc, &params->band);
}

/**
 * cm_roam_send_rt_stats_config() - set roam stats parameters
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @param_value: roam stats param value
 *
 * This function is used to set roam event stats parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_roam_send_rt_stats_config(struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id, uint8_t param_value)
{
	struct roam_disable_cfg *req;
	QDF_STATUS status;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	req->vdev_id = vdev_id;
	req->cfg = param_value;

	status = wlan_cm_tgt_send_roam_rt_stats_config(psoc, req);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_debug("fail to send roam rt stats config");

	qdf_mem_free(req);

	return status;
}
#else
static inline void
cm_roam_reason_vsie(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		    struct wlan_roam_reason_vsie_enable *params)
{
}

static inline void
cm_roam_triggers(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		 struct wlan_roam_triggers *params)
{
}

static void
cm_roam_bss_load_config(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			struct wlan_roam_bss_load_config *params)
{
}

static void
cm_roam_disconnect_params(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			  struct wlan_roam_disconnect_params *params)
{
}

static void
cm_roam_idle_params(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		    struct wlan_roam_idle_params *params)
{
}
#endif

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(WLAN_FEATURE_FILS_SK)
QDF_STATUS cm_roam_scan_offload_add_fils_params(
		struct wlan_objmgr_psoc *psoc,
		struct wlan_roam_scan_offload_params *rso_cfg,
		uint8_t vdev_id)
{
	QDF_STATUS status;
	uint32_t usr_name_len;
	struct wlan_fils_connection_info *fils_info;
	struct wlan_roam_fils_params *fils_roam_config =
				&rso_cfg->fils_roam_config;

	fils_info = wlan_cm_get_fils_connection_info(psoc, vdev_id);
	if (!fils_info)
		return QDF_STATUS_SUCCESS;

	if (fils_info->key_nai_length > FILS_MAX_KEYNAME_NAI_LENGTH ||
	    fils_info->r_rk_length > WLAN_FILS_MAX_RRK_LENGTH) {
		mlme_err("Fils info len error: keyname nai len(%d) rrk len(%d)",
			 fils_info->key_nai_length, fils_info->r_rk_length);
		return QDF_STATUS_E_FAILURE;
	}

	fils_roam_config->next_erp_seq_num = fils_info->erp_sequence_number;

	usr_name_len =
		qdf_str_copy_all_before_char(fils_info->keyname_nai,
					     sizeof(fils_info->keyname_nai),
					     fils_roam_config->username,
					     sizeof(fils_roam_config->username),
					     '@');
	if (fils_info->key_nai_length <= usr_name_len) {
		mlme_err("Fils info len error: key nai len %d, user name len %d",
			 fils_info->key_nai_length, usr_name_len);
		return QDF_STATUS_E_INVAL;
	}

	fils_roam_config->username_length = usr_name_len;
	qdf_mem_copy(fils_roam_config->rrk, fils_info->r_rk,
		     fils_info->r_rk_length);
	fils_roam_config->rrk_length = fils_info->r_rk_length;
	fils_roam_config->realm_len = fils_info->key_nai_length -
				fils_roam_config->username_length - 1;
	qdf_mem_copy(fils_roam_config->realm,
		     (fils_info->keyname_nai +
		     fils_roam_config->username_length + 1),
		     fils_roam_config->realm_len);

	/*
	 * Set add FILS tlv true for initial FULL EAP connection and subsequent
	 * FILS connection.
	 */
	rso_cfg->add_fils_tlv = true;
	mlme_debug("Fils: next_erp_seq_num %d rrk_len %d realm_len:%d",
		   fils_info->erp_sequence_number,
		   fils_info->r_rk_length,
		   fils_info->realm_len);
	if (!fils_info->is_fils_connection)
		return QDF_STATUS_SUCCESS;

	/* Update rik from crypto to fils roam config buffer */
	status = wlan_crypto_create_fils_rik(fils_info->r_rk,
					     fils_info->r_rk_length,
					     fils_info->rik,
					     &fils_info->rik_length);
	qdf_mem_copy(fils_roam_config->rik, fils_info->rik,
		     fils_info->rik_length);
	fils_roam_config->rik_length = fils_info->rik_length;

	fils_roam_config->fils_ft_len = fils_info->fils_ft_len;
	qdf_mem_copy(fils_roam_config->fils_ft, fils_info->fils_ft,
		     fils_info->fils_ft_len);

	return status;
}
#endif

/**
 * cm_roam_mawc_params() - set roam mawc parameters
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @params: roam mawc parameters
 *
 * This function is used to set roam mawc parameters
 *
 * Return: None
 */
static void
cm_roam_mawc_params(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		    struct wlan_roam_mawc_params *params)
{
	bool mawc_enabled;
	bool mawc_roam_enabled;

	params->vdev_id = vdev_id;
	wlan_mlme_get_mawc_enabled(psoc, &mawc_enabled);
	wlan_mlme_get_mawc_roam_enabled(psoc, &mawc_roam_enabled);
	params->enable = mawc_enabled && mawc_roam_enabled;
	wlan_mlme_get_mawc_roam_traffic_threshold(
				psoc, &params->traffic_load_threshold);
	wlan_mlme_get_mawc_roam_ap_rssi_threshold(
				psoc, &params->best_ap_rssi_threshold);
	wlan_mlme_get_mawc_roam_rssi_high_adjust(
				psoc, &params->rssi_stationary_high_adjust);
	wlan_mlme_get_mawc_roam_rssi_low_adjust(
				psoc, &params->rssi_stationary_low_adjust);
}

QDF_STATUS
cm_roam_send_disable_config(struct wlan_objmgr_psoc *psoc,
			    uint8_t vdev_id, uint8_t cfg)
{
	struct roam_disable_cfg *req;
	QDF_STATUS status;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	req->vdev_id = vdev_id;
	req->cfg = cfg;

	status = wlan_cm_tgt_send_roam_disable_config(psoc, vdev_id, req);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_debug("fail to send roam disable config");

	qdf_mem_free(req);

	return status;
}

/**
 * cm_roam_init_req() - roam init request handling
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_init_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id, bool enable)
{
	return wlan_cm_tgt_send_roam_offload_init(psoc, vdev_id, enable);
}

QDF_STATUS cm_rso_set_roam_trigger(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id,
				   struct wlan_roam_triggers *trigger)
{
	QDF_STATUS status;
	uint8_t reason = REASON_SUPPLICANT_DE_INIT_ROAMING;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!psoc)
		return QDF_STATUS_E_INVAL;

	mlme_set_roam_trigger_bitmap(psoc, trigger->vdev_id,
				     trigger->trigger_bitmap);

	if (trigger->trigger_bitmap)
		reason = REASON_SUPPLICANT_INIT_ROAMING;

	status = cm_roam_state_change(pdev, vdev_id,
			trigger->trigger_bitmap ? WLAN_ROAM_RSO_ENABLED :
			WLAN_ROAM_DEINIT,
			reason);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return wlan_cm_tgt_send_roam_triggers(psoc, vdev_id, trigger);
}

static void cm_roam_set_roam_reason_better_ap(struct wlan_objmgr_psoc *psoc,
					      uint8_t vdev_id, bool set)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev)
		return;
	mlme_set_roam_reason_better_ap(vdev, set);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}

/**
 * cm_roam_start_req() - roam start request handling
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_start_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		  uint8_t reason)
{
	struct wlan_roam_start_config *start_req;
	QDF_STATUS status;

	start_req = qdf_mem_malloc(sizeof(*start_req));
	if (!start_req)
		return QDF_STATUS_E_NOMEM;

	cm_roam_set_roam_reason_better_ap(psoc, vdev_id, false);
	/* fill from mlme directly */
	cm_roam_scan_bmiss_cnt(psoc, vdev_id, &start_req->beacon_miss_cnt);
	cm_roam_reason_vsie(psoc, vdev_id, &start_req->reason_vsie_enable);
	cm_roam_triggers(psoc, vdev_id, &start_req->roam_triggers);
	cm_roam_fill_rssi_change_params(psoc, vdev_id,
					&start_req->rssi_change_params);
	cm_roam_mawc_params(psoc, vdev_id, &start_req->mawc_params);
	cm_roam_bss_load_config(psoc, vdev_id, &start_req->bss_load_config);
	cm_roam_disconnect_params(psoc, vdev_id, &start_req->disconnect_params);
	cm_roam_idle_params(psoc, vdev_id, &start_req->idle_params);

	/* fill from legacy through this API */
	wlan_cm_roam_fill_start_req(psoc, vdev_id, start_req, reason);

	start_req->wlan_roam_rt_stats_config =
			wlan_cm_get_roam_rt_stats(psoc, ROAM_RT_STATS_ENABLE);

	status = wlan_cm_tgt_send_roam_start_req(psoc, vdev_id, start_req);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_debug("fail to send roam start");

	qdf_mem_free(start_req);

	return status;
}

/**
 * cm_roam_update_config_req() - roam update config request handling
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_update_config_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			  uint8_t reason)
{
	struct wlan_roam_update_config *update_req;
	QDF_STATUS status;

	cm_roam_set_roam_reason_better_ap(psoc, vdev_id, false);

	update_req = qdf_mem_malloc(sizeof(*update_req));
	if (!update_req)
		return QDF_STATUS_E_NOMEM;

	/* fill from mlme directly */
	cm_roam_scan_bmiss_cnt(psoc, vdev_id, &update_req->beacon_miss_cnt);
	cm_roam_fill_rssi_change_params(psoc, vdev_id,
					&update_req->rssi_change_params);
	if (MLME_IS_ROAM_STATE_RSO_ENABLED(psoc, vdev_id)) {
		cm_roam_disconnect_params(psoc, vdev_id,
					  &update_req->disconnect_params);
		cm_roam_idle_params(psoc, vdev_id,
				    &update_req->idle_params);
		cm_roam_triggers(psoc, vdev_id,
				 &update_req->roam_triggers);
	}

	/* fill from legacy through this API */
	wlan_cm_roam_fill_update_config_req(psoc, vdev_id, update_req, reason);

	update_req->wlan_roam_rt_stats_config =
			wlan_cm_get_roam_rt_stats(psoc, ROAM_RT_STATS_ENABLE);

	status = wlan_cm_tgt_send_roam_update_req(psoc, vdev_id, update_req);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_debug("fail to send update config");

	qdf_mem_free(update_req);

	return status;
}

/**
 * cm_roam_restart_req() - roam restart req for LFR2
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_restart_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		    uint8_t reason)
{

	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev)
		return QDF_STATUS_E_INVAL;

	/* Rome offload engine does not stop after any scan.
	 * If this command is sent because all preauth attempts failed
	 * and WMI_ROAM_REASON_SUITABLE_AP event was received earlier,
	 * now it is time to call it heartbeat failure.
	 */
	if (reason == REASON_PREAUTH_FAILED_FOR_ALL
	    && mlme_get_roam_reason_better_ap(vdev)) {
		mlme_err("Sending heartbeat failure after preauth failures");
		wlan_cm_send_beacon_miss(vdev_id, mlme_get_hb_ap_rssi(vdev));
		mlme_set_roam_reason_better_ap(vdev, false);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_abort_req() - roam scan abort req
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_abort_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		  uint8_t reason)
{
	QDF_STATUS status;

	status = wlan_cm_tgt_send_roam_abort_req(psoc, vdev_id);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_debug("fail to send abort start");

	return status;
}

QDF_STATUS
cm_roam_stop_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		 uint8_t reason)
{
	struct wlan_roam_stop_config *stop_req;
	QDF_STATUS status;

	cm_roam_set_roam_reason_better_ap(psoc, vdev_id, false);
	stop_req = qdf_mem_malloc(sizeof(*stop_req));
	if (!stop_req)
		return QDF_STATUS_E_NOMEM;

	stop_req->btm_config.vdev_id = vdev_id;
	stop_req->disconnect_params.vdev_id = vdev_id;
	stop_req->idle_params.vdev_id = vdev_id;
	stop_req->roam_triggers.vdev_id = vdev_id;
	stop_req->rssi_params.vdev_id = vdev_id;

	/* do the filling as csr_post_rso_stop */
	status = wlan_cm_roam_fill_stop_req(psoc, vdev_id, stop_req, reason);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_debug("fail to fill stop config req");
		qdf_mem_free(stop_req);
		return status;
	}

	/*
	 * If roam synch propagation is in progress and an user space
	 * disconnect is requested, then there is no need to send the
	 * RSO STOP to firmware, since the roaming is already complete.
	 * If the RSO STOP is sent to firmware, then an HO_FAIL will be
	 * generated and the expectation from firmware would be to
	 * clean up the peer context on the host and not send down any
	 * WMI PEER DELETE commands to firmware. But, if the user space
	 * disconnect gets processed first, then there is a chance to
	 * send down the PEER DELETE commands. Hence, if we do not
	 * receive the HO_FAIL, and we complete the roam sync
	 * propagation, then the host and firmware will be in sync with
	 * respect to the peer and then the user space disconnect can
	 * be handled gracefully in a normal way.
	 *
	 * Ensure to check the reason code since the RSO Stop might
	 * come when roam sync failed as well and at that point it
	 * should go through to the firmware and receive HO_FAIL
	 * and clean up.
	 */
	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id) &&
	    stop_req->reason == REASON_ROAM_STOP_ALL) {
		mlme_info("vdev_id:%d : Drop RSO stop during roam sync",
			  vdev_id);
		qdf_mem_free(stop_req);
		return QDF_STATUS_SUCCESS;
	}

	status = wlan_cm_tgt_send_roam_stop_req(psoc, vdev_id, stop_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_debug("fail to send roam stop");
	} else {
		status = wlan_cm_roam_scan_offload_rsp(vdev_id, reason);
		if (QDF_IS_STATUS_ERROR(status))
			mlme_debug("fail to send rso rsp msg");
	}

	qdf_mem_free(stop_req);

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_fill_per_roam_request() - create PER roam offload config request
 * @psoc: psoc context
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_fill_per_roam_request(struct wlan_objmgr_psoc *psoc,
			      struct wlan_per_roam_config_req *req)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return QDF_STATUS_E_FAILURE;

	req->per_config.enable = mlme_obj->cfg.lfr.per_roam_enable;
	req->per_config.tx_high_rate_thresh =
		mlme_obj->cfg.lfr.per_roam_config_high_rate_th;
	req->per_config.rx_high_rate_thresh =
		mlme_obj->cfg.lfr.per_roam_config_high_rate_th;
	req->per_config.tx_low_rate_thresh =
		mlme_obj->cfg.lfr.per_roam_config_low_rate_th;
	req->per_config.rx_low_rate_thresh =
		mlme_obj->cfg.lfr.per_roam_config_low_rate_th;
	req->per_config.per_rest_time = mlme_obj->cfg.lfr.per_roam_rest_time;
	req->per_config.tx_per_mon_time =
		mlme_obj->cfg.lfr.per_roam_monitor_time;
	req->per_config.rx_per_mon_time =
		mlme_obj->cfg.lfr.per_roam_monitor_time;
	req->per_config.tx_rate_thresh_percnt =
		mlme_obj->cfg.lfr.per_roam_config_rate_th_percent;
	req->per_config.rx_rate_thresh_percnt =
		mlme_obj->cfg.lfr.per_roam_config_rate_th_percent;
	req->per_config.min_candidate_rssi =
		mlme_obj->cfg.lfr.per_roam_min_candidate_rssi;

	mlme_debug("PER based roaming configuaration enable: %d vdev: %d high_rate_thresh: %d low_rate_thresh: %d rate_thresh_percnt: %d per_rest_time: %d monitor_time: %d min cand rssi: %d",
		   req->per_config.enable, req->vdev_id,
		   req->per_config.tx_high_rate_thresh,
		   req->per_config.tx_low_rate_thresh,
		   req->per_config.tx_rate_thresh_percnt,
		   req->per_config.per_rest_time,
		   req->per_config.tx_per_mon_time,
		   req->per_config.min_candidate_rssi);

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_offload_per_config() - populates roam offload scan request and sends
 * to fw
 * @psoc: psoc context
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_offload_per_config(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct wlan_per_roam_config_req *req;
	bool is_per_roam_enabled;
	QDF_STATUS status;

	/*
	 * Disable PER trigger for phymode less than 11n to avoid
	 * frequent roams as the PER rate threshold is greater than
	 * 11a/b/g rates
	 */
	is_per_roam_enabled = cm_roam_is_per_roam_allowed(psoc, vdev_id);
	if (!is_per_roam_enabled)
		return QDF_STATUS_SUCCESS;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	req->vdev_id = vdev_id;
	status = cm_roam_fill_per_roam_request(psoc, req);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(req);
		mlme_debug("fail to fill per config");
		return status;
	}

	status = wlan_cm_tgt_send_roam_per_config(psoc, vdev_id, req);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_debug("fail to send roam stop");

	qdf_mem_free(req);

	return status;
}

/*
 * similar to csr_roam_offload_scan, will be used from many legacy
 * process directly, generate a new function wlan_cm_roam_send_rso_cmd
 * for external usage.
 */
QDF_STATUS cm_roam_send_rso_cmd(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id, uint8_t rso_command,
				uint8_t reason)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	status = wlan_cm_roam_cmd_allowed(psoc, vdev_id, rso_command, reason);

	if (status == QDF_STATUS_E_NOSUPPORT)
		return QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_debug("ROAM: not allowed");
		return status;
	}

	/*
	 * Update PER config to FW. No need to update in case of stop command,
	 * FW takes care of stopping this internally
	 */
	if (rso_command != ROAM_SCAN_OFFLOAD_STOP)
		cm_roam_offload_per_config(psoc, vdev_id);

	if (rso_command == ROAM_SCAN_OFFLOAD_START)
		status = cm_roam_start_req(psoc, vdev_id, reason);
	else if (rso_command == ROAM_SCAN_OFFLOAD_UPDATE_CFG)
		status = cm_roam_update_config_req(psoc, vdev_id, reason);
	else if (rso_command == ROAM_SCAN_OFFLOAD_RESTART)
		status = cm_roam_restart_req(psoc, vdev_id, reason);
	else if (rso_command == ROAM_SCAN_OFFLOAD_ABORT_SCAN)
		status = cm_roam_abort_req(psoc, vdev_id, reason);
	else
		mlme_debug("ROAM: invalid RSO command %d", rso_command);

	return status;
}

/**
 * cm_roam_switch_to_rso_stop() - roam state handling for rso stop
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAM_RSO_STOPPED roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_rso_stop(struct wlan_objmgr_pdev *pdev,
			   uint8_t vdev_id,
			   uint8_t reason)
{
	enum roam_offload_state cur_state;
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	cur_state = mlme_get_roam_state(psoc, vdev_id);
	switch (cur_state) {
	case WLAN_ROAM_RSO_ENABLED:
	case WLAN_ROAMING_IN_PROG:
	case WLAN_ROAM_SYNCH_IN_PROG:
		status = cm_roam_stop_req(psoc, vdev_id, reason);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlme_err("ROAM: Unable to switch to RSO STOP State");
			return QDF_STATUS_E_FAILURE;
		}
		break;

	case WLAN_ROAM_DEINIT:
	case WLAN_ROAM_RSO_STOPPED:
	case WLAN_ROAM_INIT:
	/*
	 * Already the roaming module is initialized at fw,
	 * nothing to do here
	 */
	default:
		/* For LFR2 BTM request, need handoff even roam disabled */
		if (reason == REASON_OS_REQUESTED_ROAMING_NOW)
			wlan_cm_roam_neighbor_proceed_with_handoff_req(vdev_id);
		return QDF_STATUS_SUCCESS;
	}
	mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_RSO_STOPPED);

	return QDF_STATUS_SUCCESS;
}

static void cm_roam_roam_invoke_in_progress(struct wlan_objmgr_psoc *psoc,
					    uint8_t vdev_id, bool set)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_roam_after_data_stall *vdev_roam_params;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev)
		return;
	vdev_roam_params = mlme_get_roam_invoke_params(vdev);
	if (vdev_roam_params)
		vdev_roam_params->roam_invoke_in_progress = set;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
}
/**
 * cm_roam_switch_to_deinit() - roam state handling for roam deinit
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAM_DEINIT roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_deinit(struct wlan_objmgr_pdev *pdev,
			 uint8_t vdev_id,
			 uint8_t reason)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	enum roam_offload_state cur_state = mlme_get_roam_state(psoc, vdev_id);
	bool sup_disabled_roam;

	cm_roam_roam_invoke_in_progress(psoc, vdev_id, false);

	switch (cur_state) {
	/*
	 * If RSO stop is not done already, send RSO stop first and
	 * then post deinit.
	 */
	case WLAN_ROAM_RSO_ENABLED:
	case WLAN_ROAMING_IN_PROG:
	case WLAN_ROAM_SYNCH_IN_PROG:
		cm_roam_switch_to_rso_stop(pdev, vdev_id, reason);
		break;
	case WLAN_ROAM_RSO_STOPPED:
		/*
		 * When Supplicant disabled roaming is set and roam invoke
		 * command is received from userspace, fw starts to roam.
		 * But meanwhile if a disassoc/deauth is received from AP or if
		 * NB disconnect is initiated while supplicant disabled roam,
		 * RSO stop with ROAM scan mode as 0 is not sent to firmware
		 * since the previous state was RSO_STOPPED. This could lead
		 * to firmware not sending peer unmap event for the current
		 * AP. To avoid this, if previous RSO stop was sent with
		 * ROAM scan mode as 4, send RSO stop with Roam scan mode as 0
		 * and then switch to ROAM_DEINIT.
		 */
		sup_disabled_roam =
			mlme_get_supplicant_disabled_roaming(psoc,
							     vdev_id);
		if (sup_disabled_roam) {
			mlme_err("vdev[%d]: supplicant disabled roam. clear roam scan mode",
				 vdev_id);
			cm_roam_switch_to_rso_stop(pdev, vdev_id, reason);
		}

	case WLAN_ROAM_INIT:
		break;

	case WLAN_ROAM_DEINIT:
	/*
	 * Already the roaming module is de-initialized at fw,
	 * do nothing here
	 */
	default:
		return QDF_STATUS_SUCCESS;
	}

	status = cm_roam_init_req(psoc, vdev_id, false);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_DEINIT);
	mlme_clear_operations_bitmap(psoc, vdev_id);
	wlan_cm_roam_activate_pcl_per_vdev(psoc, vdev_id, false);

	if (reason != REASON_SUPPLICANT_INIT_ROAMING)
		wlan_cm_enable_roaming_on_connected_sta(pdev, vdev_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_switch_to_init() - roam state handling for roam init
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAM_INIT roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_init(struct wlan_objmgr_pdev *pdev,
		       uint8_t vdev_id,
		       uint8_t reason)
{
	enum roam_offload_state cur_state;
	uint8_t temp_vdev_id, roam_enabled_vdev_id;
	uint32_t roaming_bitmap;
	bool dual_sta_roam_active, usr_disabled_roaming;
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	cm_roam_roam_invoke_in_progress(psoc, vdev_id, false);

	dual_sta_roam_active =
		wlan_mlme_get_dual_sta_roaming_enabled(psoc);

	cur_state = mlme_get_roam_state(psoc, vdev_id);
	switch (cur_state) {
	case WLAN_ROAM_DEINIT:
		roaming_bitmap = mlme_get_roam_trigger_bitmap(psoc, vdev_id);
		if (!roaming_bitmap) {
			mlme_info("CM_RSO: Cannot change to INIT state for vdev[%d]",
				  vdev_id);
			return QDF_STATUS_E_FAILURE;
		}

		if (dual_sta_roam_active)
			break;
		/*
		 * Disable roaming on the enabled sta if supplicant wants to
		 * enable roaming on this vdev id
		 */
		temp_vdev_id = policy_mgr_get_roam_enabled_sta_session_id(
								psoc, vdev_id);
		if (temp_vdev_id != WLAN_UMAC_VDEV_ID_MAX) {
			/*
			 * Roam init state can be requested as part of
			 * initial connection or due to enable from
			 * supplicant via vendor command. This check will
			 * ensure roaming does not get enabled on this STA
			 * vdev id if it is not an explicit enable from
			 * supplicant.
			 */
			if (reason != REASON_SUPPLICANT_INIT_ROAMING) {
				mlme_info("CM_RSO: Roam module already initialized on vdev:[%d]",
					  temp_vdev_id);
				return QDF_STATUS_E_FAILURE;
			}
			cm_roam_state_change(pdev, temp_vdev_id,
					     WLAN_ROAM_DEINIT, reason);
		}
		break;

	case WLAN_ROAM_SYNCH_IN_PROG:
		mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_INIT);
		return QDF_STATUS_SUCCESS;

	case WLAN_ROAM_INIT:
	case WLAN_ROAM_RSO_STOPPED:
	case WLAN_ROAM_RSO_ENABLED:
	case WLAN_ROAMING_IN_PROG:
	/*
	 * Already the roaming module is initialized at fw,
	 * just return from here
	 */
	default:
		return QDF_STATUS_SUCCESS;
	}

	status = cm_roam_init_req(psoc, vdev_id, true);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_INIT);

	roam_enabled_vdev_id =
		policy_mgr_get_roam_enabled_sta_session_id(psoc, vdev_id);

	/* Send PDEV pcl command if only one STA is in connected state
	 * If there is another STA connection exist, then set the
	 * PCL type to vdev level
	 */
	if (roam_enabled_vdev_id != WLAN_UMAC_VDEV_ID_MAX &&
	    dual_sta_roam_active)
		wlan_cm_roam_activate_pcl_per_vdev(psoc, vdev_id, true);

	/* Set PCL before sending RSO start */
	policy_mgr_set_pcl_for_existing_combo(psoc, PM_STA_MODE, vdev_id);

	wlan_mlme_get_usr_disabled_roaming(psoc, &usr_disabled_roaming);
	if (usr_disabled_roaming) {
		status =
		cm_roam_send_disable_config(
			psoc, vdev_id,
			WMI_VDEV_ROAM_11KV_CTRL_DISABLE_FW_TRIGGER_ROAMING);

		if (!QDF_IS_STATUS_SUCCESS(status))
			mlme_err("ROAM: fast roaming disable failed. status %d",
				 status);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_switch_to_rso_enable() - roam state handling for rso started
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAM_RSO_ENABLED roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_rso_enable(struct wlan_objmgr_pdev *pdev,
			     uint8_t vdev_id,
			     uint8_t reason)
{
	enum roam_offload_state cur_state, new_roam_state;
	QDF_STATUS status;
	uint8_t control_bitmap;
	bool sup_disabled_roaming;
	bool rso_allowed;
	uint8_t rso_command = ROAM_SCAN_OFFLOAD_START;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	wlan_mlme_get_roam_scan_offload_enabled(psoc, &rso_allowed);
	sup_disabled_roaming = mlme_get_supplicant_disabled_roaming(psoc,
								    vdev_id);
	control_bitmap = mlme_get_operations_bitmap(psoc, vdev_id);

	cur_state = mlme_get_roam_state(psoc, vdev_id);
	switch (cur_state) {
	case WLAN_ROAM_INIT:
	case WLAN_ROAM_RSO_STOPPED:
		break;

	case WLAN_ROAM_DEINIT:
		status = cm_roam_switch_to_init(pdev, vdev_id, reason);
		if (QDF_IS_STATUS_ERROR(status))
			return status;

		break;
	case WLAN_ROAM_RSO_ENABLED:
		/*
		 * Send RSO update config if roaming already enabled
		 */
		rso_command = ROAM_SCAN_OFFLOAD_UPDATE_CFG;
		break;
	case WLAN_ROAMING_IN_PROG:
		/*
		 * When roam abort happens, the roam offload
		 * state machine moves to RSO_ENABLED state.
		 * But if Supplicant disabled roaming is set in case
		 * of roam invoke or if roaming was disabled due to
		 * other reasons like SAP start/connect on other vdev,
		 * the state should be transitioned to RSO STOPPED.
		 */
		if (sup_disabled_roaming || control_bitmap)
			new_roam_state = WLAN_ROAM_RSO_STOPPED;
		else
			new_roam_state = WLAN_ROAM_RSO_ENABLED;

		mlme_set_roam_state(psoc, vdev_id, new_roam_state);

		return QDF_STATUS_SUCCESS;
	case WLAN_ROAM_SYNCH_IN_PROG:
		if (reason == REASON_ROAM_ABORT) {
			mlme_debug("Roam synch in progress, drop Roam abort");
			return QDF_STATUS_SUCCESS;
		}
		/*
		 * After roam sych propagation is complete, send
		 * RSO start command to firmware to update AP profile,
		 * new PCL.
		 * If this is roam invoke case and supplicant has already
		 * disabled firmware roaming, then move to RSO stopped state
		 * instead of RSO enabled.
		 */
		if (sup_disabled_roaming || control_bitmap) {
			new_roam_state = WLAN_ROAM_RSO_STOPPED;
			mlme_set_roam_state(psoc, vdev_id, new_roam_state);

			return QDF_STATUS_SUCCESS;
		}

		break;
	default:
		return QDF_STATUS_SUCCESS;
	}

	if (!rso_allowed) {
		mlme_debug("ROAM: RSO disabled via INI");
		return QDF_STATUS_E_FAILURE;
	}

	if (control_bitmap) {
		mlme_debug("ROAM: RSO Disabled internaly: vdev[%d] bitmap[0x%x]",
			   vdev_id, control_bitmap);
		return QDF_STATUS_E_FAILURE;
	}

	status = cm_roam_send_rso_cmd(psoc, vdev_id, rso_command, reason);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_debug("ROAM: RSO start failed");
		return status;
	}
	mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_RSO_ENABLED);

	/*
	 * If supplicant disabled roaming, driver does not send
	 * RSO cmd to fw. This causes roam invoke to fail in FW
	 * since RSO start never happened at least once to
	 * configure roaming engine in FW. So send RSO start followed
	 * by RSO stop if supplicant disabled roaming is true.
	 */
	if (!sup_disabled_roaming)
		return QDF_STATUS_SUCCESS;

	mlme_debug("ROAM: RSO disabled by Supplicant on vdev[%d]", vdev_id);
	return cm_roam_state_change(pdev, vdev_id, WLAN_ROAM_RSO_STOPPED,
				    REASON_SUPPLICANT_DISABLED_ROAMING);
}

/**
 * cm_roam_switch_to_roam_start() - roam state handling for ROAMING_IN_PROG
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAMING_IN_PROG roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_roam_start(struct wlan_objmgr_pdev *pdev,
			     uint8_t vdev_id,
			     uint8_t reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	enum roam_offload_state cur_state =
				mlme_get_roam_state(psoc, vdev_id);
	switch (cur_state) {
	case WLAN_ROAM_RSO_ENABLED:
		mlme_set_roam_state(psoc, vdev_id, WLAN_ROAMING_IN_PROG);
		break;

	case WLAN_ROAM_RSO_STOPPED:
		/*
		 * When supplicant has disabled roaming, roam invoke triggered
		 * from supplicant can cause firmware to send roam start
		 * notification. Allow roam start in this condition.
		 */
		if (mlme_get_supplicant_disabled_roaming(psoc, vdev_id) &&
		    mlme_is_roam_invoke_in_progress(psoc, vdev_id)) {
			mlme_set_roam_state(psoc, vdev_id,
					    WLAN_ROAMING_IN_PROG);
			break;
		}
	case WLAN_ROAM_INIT:
	case WLAN_ROAM_DEINIT:
	case WLAN_ROAM_SYNCH_IN_PROG:
	default:
		mlme_err("ROAM: Roaming start received in invalid state: %d",
			 cur_state);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * cm_roam_switch_to_roam_sync() - roam state handling for roam sync
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function is used for WLAN_ROAM_SYNCH_IN_PROG roam state handling
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
cm_roam_switch_to_roam_sync(struct wlan_objmgr_pdev *pdev,
			    uint8_t vdev_id,
			    uint8_t reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	enum roam_offload_state cur_state = mlme_get_roam_state(psoc, vdev_id);

	switch (cur_state) {
	case WLAN_ROAM_RSO_ENABLED:
		/*
		 * Roam synch can come directly without roam start
		 * after waking up from power save mode or in case of
		 * deauth roam trigger to stop data path queues
		 */
	case WLAN_ROAMING_IN_PROG:
		if (!wlan_cm_is_sta_connected(vdev_id)) {
			mlme_err("ROAM: STA not in connected state");
			return QDF_STATUS_E_FAILURE;
		}

		mlme_set_roam_state(psoc, vdev_id, WLAN_ROAM_SYNCH_IN_PROG);
		break;
	case WLAN_ROAM_RSO_STOPPED:
		/*
		 * If roaming is disabled by Supplicant and if this transition
		 * is due to roaming invoked by the supplicant, then allow
		 * this state transition
		 */
		if (mlme_get_supplicant_disabled_roaming(psoc, vdev_id) &&
		    mlme_is_roam_invoke_in_progress(psoc, vdev_id)) {
			mlme_set_roam_state(psoc, vdev_id,
					    WLAN_ROAM_SYNCH_IN_PROG);
			break;
		}
		/*
		 * transition to WLAN_ROAM_SYNCH_IN_PROG not allowed otherwise
		 * if we're already RSO stopped, fall through to return failure
		 */
	case WLAN_ROAM_INIT:
	case WLAN_ROAM_DEINIT:
	case WLAN_ROAM_SYNCH_IN_PROG:
	default:
		mlme_err("ROAM: Roam synch not allowed in [%d] state",
			 cur_state);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_ROAM_DEBUG
/**
 * union rso_rec_arg1 - argument 1 record rso state change
 * @request_st: requested rso state
 * @cur_st: current rso state
 * @new_st: new rso state
 * @status: qdf status for the request
 */
union rso_rec_arg1 {
	uint32_t value;
	struct {
		uint32_t request_st:4,
			 cur_st:4,
			 new_st:4,
			 status:8;
	};
};

/**
 * get_rso_arg1 - get argument 1 record rso state change
 * @request_st: requested rso state
 * @cur_st: current rso state
 * @new_st: new rso state
 * @status: qdf status for the request
 *
 * Return: u32 value of rso information
 */
static uint32_t get_rso_arg1(enum roam_offload_state request_st,
			     enum roam_offload_state cur_st,
			     enum roam_offload_state new_st,
			     QDF_STATUS status)
{
	union rso_rec_arg1 rso_arg1;

	rso_arg1.value = 0;
	rso_arg1.request_st = request_st;
	rso_arg1.cur_st = cur_st;
	rso_arg1.new_st = new_st;
	rso_arg1.status = status;

	return rso_arg1.value;
}

/**
 * union rso_rec_arg2 - argument 2 record rso state change
 * @is_up: vdev is up
 * @supp_dis_roam: supplicant disable roam
 * @roam_progress: roam in progress
 * @ctrl_bitmap: control bitmap
 * @reason: reason code
 *
 * Return: u32 value of rso information
 */
union rso_rec_arg2 {
	uint32_t value;
	struct {
		uint32_t is_up: 1,
			 supp_dis_roam:1,
			 roam_progress:1,
			 ctrl_bitmap:8,
			 reason:8;
	};
};

/**
 * get_rso_arg2 - get argument 2 record rso state change
 * @is_up: vdev is up
 * @supp_dis_roam: supplicant disable roam
 * @roam_progress: roam in progress
 * @ctrl_bitmap: control bitmap
 * @reason: reason code
 */
static uint32_t get_rso_arg2(bool is_up,
			     bool supp_dis_roam,
			     bool roam_progress,
			     uint8_t ctrl_bitmap,
			     uint8_t reason)
{
	union rso_rec_arg2 rso_arg2;

	rso_arg2.value = 0;
	if (is_up)
		rso_arg2.is_up = 1;
	if (supp_dis_roam)
		rso_arg2.supp_dis_roam = 1;
	if (roam_progress)
		rso_arg2.roam_progress = 1;
	rso_arg2.ctrl_bitmap = ctrl_bitmap;
	rso_arg2.reason = reason;

	return rso_arg2.value;
}

/**
 * cm_record_state_change() - record rso state change to roam history log
 * @pdev: pdev object
 * @vdev_id: vdev id
 * @cur_st: current state
 * @request_state: requested state
 * @reason: reason
 * @is_up: vdev is up
 * @status: request result code
 *
 * This function will record the RSO state change to roam history log.
 *
 * Return: void
 */
static void
cm_record_state_change(struct wlan_objmgr_pdev *pdev,
		       uint8_t vdev_id,
		       enum roam_offload_state cur_st,
		       enum roam_offload_state requested_state,
		       uint8_t reason,
		       bool is_up,
		       QDF_STATUS status)
{
	enum roam_offload_state new_state;
	bool supp_dis_roam;
	bool roam_progress;
	uint8_t control_bitmap;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!psoc)
		return;

	new_state = mlme_get_roam_state(psoc, vdev_id);
	control_bitmap = mlme_get_operations_bitmap(psoc, vdev_id);
	supp_dis_roam = mlme_get_supplicant_disabled_roaming(psoc, vdev_id);
	roam_progress = wlan_cm_roaming_in_progress(pdev, vdev_id);
	wlan_rec_conn_info(vdev_id, DEBUG_CONN_RSO,
			   NULL,
			   get_rso_arg1(requested_state, cur_st,
					new_state, status),
			   get_rso_arg2(is_up,
					supp_dis_roam, roam_progress,
					control_bitmap, reason));
}
#else
static inline void
cm_record_state_change(struct wlan_objmgr_pdev *pdev,
		       uint8_t vdev_id,
		       enum roam_offload_state cur_st,
		       enum roam_offload_state requested_state,
		       uint8_t reason,
		       bool is_up,
		       QDF_STATUS status)
{
}
#endif

QDF_STATUS
cm_roam_state_change(struct wlan_objmgr_pdev *pdev,
		     uint8_t vdev_id,
		     enum roam_offload_state requested_state,
		     uint8_t reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_vdev *vdev;
	bool is_up;
	enum roam_offload_state cur_state;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!psoc)
		return QDF_STATUS_E_INVAL;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev)
		return status;

	is_up = QDF_IS_STATUS_SUCCESS(wlan_vdev_is_up(vdev));
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	cur_state = mlme_get_roam_state(psoc, vdev_id);

	if (requested_state != WLAN_ROAM_DEINIT && !is_up) {
		mlme_debug("ROAM: roam state change requested in disconnected state");
		goto end;
	}

	switch (requested_state) {
	case WLAN_ROAM_DEINIT:
		status = cm_roam_switch_to_deinit(pdev, vdev_id, reason);
		break;
	case WLAN_ROAM_INIT:
		status = cm_roam_switch_to_init(pdev, vdev_id, reason);
		break;
	case WLAN_ROAM_RSO_ENABLED:
		status = cm_roam_switch_to_rso_enable(pdev, vdev_id, reason);
		break;
	case WLAN_ROAM_RSO_STOPPED:
		status = cm_roam_switch_to_rso_stop(pdev, vdev_id, reason);
		break;
	case WLAN_ROAMING_IN_PROG:
		status = cm_roam_switch_to_roam_start(pdev, vdev_id, reason);
		break;
	case WLAN_ROAM_SYNCH_IN_PROG:
		status = cm_roam_switch_to_roam_sync(pdev, vdev_id, reason);
		break;
	default:
		mlme_debug("ROAM: Invalid roam state %d", requested_state);
		break;
	}
end:
	cm_record_state_change(pdev, vdev_id, cur_state, requested_state,
			       reason, is_up, status);

	return status;
}

uint32_t cm_crypto_cipher_wmi_cipher(int32_t cipherset)
{
	if (!cipherset || cipherset < 0)
		return WMI_CIPHER_NONE;

	if (QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_AES_GCM) ||
	    QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_AES_GCM_256))
		return WMI_CIPHER_AES_GCM;

	if (QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_AES_CCM) ||
	    QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_AES_OCB) ||
	    QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_AES_CCM_256))
		return WMI_CIPHER_AES_CCM;

	if (QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_TKIP))
		return WMI_CIPHER_TKIP;

	if (QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_AES_CMAC) ||
	    QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_AES_CMAC_256))
		return WMI_CIPHER_AES_CMAC;

	if (QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_WAPI_GCM4) ||
	    QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_WAPI_SMS4))
		return WMI_CIPHER_WAPI;

	if (QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_AES_GMAC) ||
	    QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_AES_GMAC_256))
		return WMI_CIPHER_AES_GMAC;

	if (QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_WEP) ||
	    QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_WEP_40) ||
	    QDF_HAS_PARAM(cipherset, WLAN_CRYPTO_CIPHER_WEP_104))
		return WMI_CIPHER_WEP;

	return WMI_CIPHER_NONE;
}

static uint32_t cm_get_rsn_wmi_auth_type(int32_t akm)
{
	/* Try the more preferred ones first. */
	if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA384))
		return WMI_AUTH_FT_RSNA_FILS_SHA384;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA256))
		return WMI_AUTH_FT_RSNA_FILS_SHA256;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FILS_SHA384))
		return WMI_AUTH_RSNA_FILS_SHA384;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FILS_SHA256))
		return WMI_AUTH_RSNA_FILS_SHA256;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_SAE))
		return WMI_AUTH_FT_RSNA_SAE;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_SAE))
		return WMI_AUTH_WPA3_SAE;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_OWE))
		return WMI_AUTH_WPA3_OWE;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X))
		return WMI_AUTH_FT_RSNA;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_PSK))
		return WMI_AUTH_FT_RSNA_PSK;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X))
		return WMI_AUTH_RSNA;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_PSK))
		return WMI_AUTH_RSNA_PSK;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_CCKM))
		return WMI_AUTH_CCKM_RSNA;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_PSK_SHA256))
		return WMI_AUTH_RSNA_PSK_SHA256;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256))
		return WMI_AUTH_RSNA_8021X_SHA256;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B))
		return WMI_AUTH_RSNA_SUITE_B_8021X_SHA256;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B_192))
		return WMI_AUTH_RSNA_SUITE_B_8021X_SHA384;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X_SHA384))
		return WMI_AUTH_FT_RSNA_SUITE_B_8021X_SHA384;
	else
		return WMI_AUTH_NONE;
}

static uint32_t cm_get_wpa_wmi_auth_type(int32_t akm)
{
	/* Try the more preferred ones first. */
	if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_IEEE8021X))
		return WMI_AUTH_WPA;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_PSK))
		return WMI_AUTH_WPA_PSK;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_CCKM))
		return WMI_AUTH_CCKM_WPA;
	else
		return WMI_AUTH_NONE;
}

static uint32_t cm_get_wapi_wmi_auth_type(int32_t akm)
{
	/* Try the more preferred ones first. */
	if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_WAPI_CERT))
		return WMI_AUTH_WAPI;
	else if (QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_WAPI_PSK))
		return WMI_AUTH_WAPI_PSK;
	else
		return WMI_AUTH_NONE;
}

uint32_t cm_crypto_authmode_to_wmi_authmode(int32_t authmodeset,
					    int32_t akm, int32_t ucastcipherset)
{
	if (!authmodeset || authmodeset < 0)
		return WMI_AUTH_OPEN;

	if (QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_OPEN))
		return WMI_AUTH_OPEN;

	if (QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_AUTO) ||
	    QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_NONE)) {
		if ((QDF_HAS_PARAM(ucastcipherset, WLAN_CRYPTO_CIPHER_WEP) ||
		     QDF_HAS_PARAM(ucastcipherset, WLAN_CRYPTO_CIPHER_WEP_40) ||
		     QDF_HAS_PARAM(ucastcipherset,
				   WLAN_CRYPTO_CIPHER_WEP_104) ||
		     QDF_HAS_PARAM(ucastcipherset, WLAN_CRYPTO_CIPHER_TKIP) ||
		     QDF_HAS_PARAM(ucastcipherset,
				   WLAN_CRYPTO_CIPHER_AES_GCM) ||
		     QDF_HAS_PARAM(ucastcipherset,
				   WLAN_CRYPTO_CIPHER_AES_GCM_256) ||
		     QDF_HAS_PARAM(ucastcipherset,
				   WLAN_CRYPTO_CIPHER_AES_CCM) ||
		     QDF_HAS_PARAM(ucastcipherset,
				   WLAN_CRYPTO_CIPHER_AES_OCB) ||
		     QDF_HAS_PARAM(ucastcipherset,
				   WLAN_CRYPTO_CIPHER_AES_CCM_256)))
			return WMI_AUTH_OPEN;
		else
			return WMI_AUTH_NONE;
	}

	if (QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_SHARED))
		return WMI_AUTH_SHARED;

	if (QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_8021X) ||
	    QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_RSNA) ||
	    QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_CCKM) ||
	    QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_SAE) ||
	    QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_FILS_SK))
		return cm_get_rsn_wmi_auth_type(akm);

	if (QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_WPA))
		return cm_get_wpa_wmi_auth_type(akm);

	if (QDF_HAS_PARAM(authmodeset, WLAN_CRYPTO_AUTH_WAPI))
		return cm_get_wapi_wmi_auth_type(akm);

	return WMI_AUTH_OPEN;
}
