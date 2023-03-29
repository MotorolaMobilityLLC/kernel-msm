/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sta_info.c
 *
 * Store and manage station info structure.
 *
 */

#include <wlan_hdd_includes.h>
#include "wlan_hdd_sta_info.h"

#define HDD_MAX_PEERS 32

char *sta_info_string_from_dbgid(wlan_sta_info_dbgid id)
{
	static const char *strings[] = {
				"STA_INFO_ID_RESERVED",
				"STA_INFO_CFG80211_GET_LINK_PROPERTIES",
				"STA_INFO_SOFTAP_INSPECT_TX_EAP_PKT",
				"STA_INFO_SOFTAP_CHECK_WAIT_FOR_TX_EAP_PKT",
				"STA_INFO_SOFTAP_INSPECT_DHCP_PACKET",
				"STA_INFO_SOFTAP_HARD_START_XMIT",
				"STA_INFO_SOFTAP_INIT_TX_RX_STA",
				"STA_INFO_SOFTAP_RX_PACKET_CBK",
				"STA_INFO_SOFTAP_REGISTER_STA",
				"STA_INFO_GET_CACHED_STATION_REMOTE",
				"STA_INFO_HDD_GET_STATION_REMOTE",
				"STA_INFO_WLAN_HDD_GET_STATION_REMOTE",
				"STA_INFO_SOFTAP_DEAUTH_CURRENT_STA",
				"STA_INFO_SOFTAP_DEAUTH_ALL_STA",
				"STA_INFO_CFG80211_DEL_STATION",
				"STA_INFO_HDD_CLEAR_ALL_STA",
				"STA_INFO_FILL_STATION_INFO",
				"STA_INFO_HOSTAPD_SAP_EVENT_CB",
				"STA_INFO_SAP_INDICATE_DISCONNECT_FOR_STA",
				"STA_INFO_IS_PEER_ASSOCIATED",
				"STA_INFO_SAP_SET_TWO_INTS_GETNONE",
				"STA_INFO_SAP_GETASSOC_STAMACADDR",
				"STA_INFO_SOFTAP_GET_STA_INFO",
				"STA_INFO_GET_SOFTAP_LINKSPEED",
				"STA_INFO_CONNECTION_IN_PROGRESS_ITERATOR",
				"STA_INFO_SOFTAP_STOP_BSS",
				"STA_INFO_SOFTAP_CHANGE_STA_STATE",
				"STA_INFO_CLEAR_CACHED_STA_INFO",
				"STA_INFO_ATTACH_DETACH",
				"STA_INFO_SHOW",
				"STA_INFO_ID_MAX"};
	int32_t num_dbg_strings = QDF_ARRAY_SIZE(strings);

	if (id >= num_dbg_strings) {
		char *ret = "";

		hdd_err("Debug string not found for debug id %d", id);
		return ret;
	}

	return (char *)strings[id];
}

QDF_STATUS hdd_sta_info_init(struct hdd_sta_info_obj *sta_info_container)
{
	if (!sta_info_container) {
		hdd_err("Parameter null");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spinlock_create(&sta_info_container->sta_obj_lock);
	qdf_list_create(&sta_info_container->sta_obj, HDD_MAX_PEERS);

	return QDF_STATUS_SUCCESS;
}

void hdd_sta_info_deinit(struct hdd_sta_info_obj *sta_info_container)
{
	if (!sta_info_container) {
		hdd_err("Parameter null");
		return;
	}

	qdf_list_destroy(&sta_info_container->sta_obj);
	qdf_spinlock_destroy(&sta_info_container->sta_obj_lock);
}

QDF_STATUS hdd_sta_info_attach(struct hdd_sta_info_obj *sta_info_container,
			       struct hdd_station_info *sta_info)
{
	if (!sta_info_container || !sta_info) {
		hdd_err("Parameter(s) null");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&sta_info_container->sta_obj_lock);

	hdd_take_sta_info_ref(sta_info_container, sta_info, false,
			      STA_INFO_ATTACH_DETACH);
	qdf_list_insert_front(&sta_info_container->sta_obj,
			      &sta_info->sta_node);
	sta_info->is_attached = true;

	qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);

	return QDF_STATUS_SUCCESS;
}

void hdd_sta_info_detach(struct hdd_sta_info_obj *sta_info_container,
			 struct hdd_station_info **sta_info)
{
	struct hdd_station_info *info;

	if (!sta_info_container || !sta_info) {
		hdd_err("Parameter(s) null");
		return;
	}

	info = *sta_info;

	if (!info)
		return;

	qdf_spin_lock_bh(&sta_info_container->sta_obj_lock);

	if (info->is_attached) {
		info->is_attached = false;
		hdd_put_sta_info_ref(sta_info_container, sta_info, false,
				     STA_INFO_ATTACH_DETACH);
	} else {
		hdd_info("Stainfo is already detached");
	}

	qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);
}

struct hdd_station_info *hdd_get_sta_info_by_mac(
				struct hdd_sta_info_obj *sta_info_container,
				const uint8_t *mac_addr,
				wlan_sta_info_dbgid sta_info_dbgid)
{
	struct hdd_station_info *sta_info = NULL;

	if (!mac_addr || !sta_info_container) {
		hdd_err("Parameter(s) null");
		return NULL;
	}

	qdf_spin_lock_bh(&sta_info_container->sta_obj_lock);

	qdf_list_for_each(&sta_info_container->sta_obj, sta_info, sta_node) {
		if (qdf_is_macaddr_equal(&sta_info->sta_mac,
					 (struct qdf_mac_addr *)mac_addr)) {
			hdd_take_sta_info_ref(sta_info_container,
					      sta_info, false, sta_info_dbgid);
			qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);
			return sta_info;
		}
	}

	qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);

	return NULL;
}

void hdd_take_sta_info_ref(struct hdd_sta_info_obj *sta_info_container,
			   struct hdd_station_info *sta_info,
			   bool lock_required,
			   wlan_sta_info_dbgid sta_info_dbgid)
{
	if (!sta_info_container || !sta_info) {
		hdd_err("Parameter(s) null");
		return;
	}

	if (sta_info_dbgid >= STA_INFO_ID_MAX) {
		hdd_err("Invalid sta_info debug id %d", sta_info_dbgid);
		return;
	}

	if (lock_required)
		qdf_spin_lock_bh(&sta_info_container->sta_obj_lock);

	qdf_atomic_inc(&sta_info->ref_cnt);
	qdf_atomic_inc(&sta_info->ref_cnt_dbgid[sta_info_dbgid]);

	if (lock_required)
		qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);
}

void
hdd_put_sta_info_ref(struct hdd_sta_info_obj *sta_info_container,
		     struct hdd_station_info **sta_info, bool lock_required,
		     wlan_sta_info_dbgid sta_info_dbgid)
{
	struct hdd_station_info *info;
	struct qdf_mac_addr addr;

	if (!sta_info_container || !sta_info) {
		hdd_err("Parameter(s) null");
		return;
	}

	info = *sta_info;

	if (!info) {
		hdd_err("station info NULL");
		return;
	}

	if (sta_info_dbgid >= STA_INFO_ID_MAX) {
		hdd_err("Invalid sta_info debug id %d", sta_info_dbgid);
		return;
	}

	if (lock_required)
		qdf_spin_lock_bh(&sta_info_container->sta_obj_lock);

	/*
	 * In case the put_ref is called more than twice for a single take_ref,
	 * this will result in either a BUG or page fault. In both the cases,
	 * the root cause would be known and the buggy put_ref can be taken
	 * care of.
	 */
	if (!qdf_atomic_read(&info->ref_cnt_dbgid[sta_info_dbgid])) {
		hdd_err("Sta_info ref count put is detected without get for debug id %s",
			sta_info_string_from_dbgid(sta_info_dbgid));

		QDF_BUG(0);
	}

	qdf_atomic_dec(&info->ref_cnt);
	qdf_atomic_dec(&info->ref_cnt_dbgid[sta_info_dbgid]);

	if (qdf_atomic_read(&info->ref_cnt)) {
		if (lock_required)
			qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);
		return;
	}

	qdf_copy_macaddr(&addr, &info->sta_mac);
	if (info->assoc_req_ies.len) {
		qdf_mem_free(info->assoc_req_ies.data);
		info->assoc_req_ies.data = NULL;
		info->assoc_req_ies.len = 0;
	}

	qdf_list_remove_node(&sta_info_container->sta_obj, &info->sta_node);
	qdf_mem_free(info);
	*sta_info = NULL;

	if (lock_required)
		qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);

	hdd_nofl_debug("STA_INFO: " QDF_MAC_ADDR_FMT " freed",
		       QDF_MAC_ADDR_REF(addr.bytes));
}

void hdd_clear_cached_sta_info(struct hdd_adapter *adapter)
{
	struct hdd_station_info *sta_info = NULL, *tmp = NULL;

	if (!adapter) {
		hdd_err("Parameter null");
		return;
	}

	hdd_for_each_sta_ref_safe(adapter->cache_sta_info_list, sta_info, tmp,
				  STA_INFO_CLEAR_CACHED_STA_INFO) {
		hdd_sta_info_detach(&adapter->cache_sta_info_list, &sta_info);
		hdd_put_sta_info_ref(&adapter->cache_sta_info_list, &sta_info,
				     true, STA_INFO_CLEAR_CACHED_STA_INFO);
	}
}

QDF_STATUS
hdd_get_front_sta_info_no_lock(struct hdd_sta_info_obj *sta_info_container,
			       struct hdd_station_info **out_sta_info)
{
	QDF_STATUS status;
	qdf_list_node_t *node;

	*out_sta_info = NULL;

	status = qdf_list_peek_front(&sta_info_container->sta_obj, &node);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	*out_sta_info =
		qdf_container_of(node, struct hdd_station_info, sta_node);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
hdd_get_next_sta_info_no_lock(struct hdd_sta_info_obj *sta_info_container,
			      struct hdd_station_info *current_sta_info,
			      struct hdd_station_info **out_sta_info)
{
	QDF_STATUS status;
	qdf_list_node_t *node;

	if (!current_sta_info)
		return QDF_STATUS_E_INVAL;

	*out_sta_info = NULL;

	status = qdf_list_peek_next(&sta_info_container->sta_obj,
				    &current_sta_info->sta_node,
				    &node);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	*out_sta_info =
		qdf_container_of(node, struct hdd_station_info, sta_node);

	return QDF_STATUS_SUCCESS;
}

