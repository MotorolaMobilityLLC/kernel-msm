/*
 * Copyright (c) 2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
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
 * DOC: wma_nan_datapath.c
 *
 * WMA NAN Data path API implementation
 */

#include "wma.h"
#include "wma_api.h"
#include "vos_api.h"
#include "wmi_unified_api.h"
#include "wmi_unified.h"
#include "wma_nan_datapath.h"

/**
 * wma_handle_ndp_initiator_req() - NDP initiator request handler
 * @wma_handle: wma handle
 * @req_params: request parameters
 *
 * Return: VOS_STATUS_SUCCESS on success; error number otherwise
 */
VOS_STATUS wma_handle_ndp_initiator_req(tp_wma_handle wma_handle,
					struct ndp_initiator_req *req_params)
{
	return VOS_STATUS_SUCCESS;
}

/**
 * wma_handle_ndp_responder_req() - NDP responder request handler
 * @wma_handle: wma handle
 * @req_params: request parameters
 *
 * Return: VOS_STATUS_SUCCESS on success; error number otherwise
 */
VOS_STATUS wma_handle_ndp_responder_req(tp_wma_handle wma_handle,
					struct ndp_responder_req *req_params)
{
	return VOS_STATUS_SUCCESS;
}

/**
 * wma_handle_ndp_end_req() - NDP end request handler
 * @wma_handle: wma handle
 * @req_params: request parameters
 *
 * Return: VOS_STATUS_SUCCESS on success; error number otherwise
 */
VOS_STATUS wma_handle_ndp_end_req(tp_wma_handle wma_handle,
				struct ndp_end_req *req_params)
{
	return VOS_STATUS_SUCCESS;
}

/**
 * wma_handle_ndp_sched_update_req() - NDP schedule update request handler
 * @wma_handle: wma handle
 * @req_params: request parameters
 *
 * Return: VOS_STATUS_SUCCESS on success; error number otherwise
 */
VOS_STATUS wma_handle_ndp_sched_update_req(tp_wma_handle wma_handle,
					struct ndp_end_req *req_params)
{
	return VOS_STATUS_SUCCESS;
}

/**
 * wma_ndp_indication_event_handler() - NDP indication event handler
 * @handle: wma handle
 * @event_info: event handler data
 * @len: length of event_info
 *
 * Handler for WMI_NDP_INDICATION_EVENTID
 * Return: 0 on success, negative errno on failure
 */
static int wma_ndp_indication_event_handler(void *handle,
	uint8_t  *event_info, uint32_t len)
{
	return 0;
}

/**
 * wma_ndp_responder_rsp_event_handler() - NDP responder response event handler
 * @handle: wma handle
 * @event_info: event handler data
 * @len: length of event_info
 *
 * Handler for WMI_NDP_RESPONDER_RSP_EVENTID
 * Return: 0 on success, negative errno on failure
 */
static int wma_ndp_responder_rsp_event_handler(void *handle,
	uint8_t  *event_info, uint32_t len)
{
	return 0;
}

/**
 * wma_ndp_confirm_event_handler() - NDP confirm event handler
 * @handle: wma handle
 * @event_info: event handler data
 * @len: length of event_info
 *
 * Handler for WMI_NDP_CONFIRM_EVENTID
 * Return: 0 on success, negative errno on failure
 */
static int wma_ndp_confirm_event_handler(void *handle,
	uint8_t  *event_info, uint32_t len)
{
	return 0;
}

/**
 * wma_ndp_end_response_event_handler() - NDP end response event handler
 * @handle: wma handle
 * @event_info: event handler data
 * @len: length of event_info
 *
 * Handler for WMI_NDP_END_RSP_EVENTID
 * Return: 0 on success, negative errno on failure
 */
static int wma_ndp_end_response_event_handler(void *handle,
	uint8_t  *event_info, uint32_t len)
{
	return 0;
}

/**
 * wma_ndp_end_indication_event_handler() - NDP end indication event handler
 * @handle: wma handle
 * @event_info: event handler data
 * @len: length of event_info
 *
 * Handler for WMI_NDP_END_INDICATION_EVENTID
 * Return: 0 on success, negative errno on failure
 */
static int wma_ndp_end_indication_event_handler(void *handle,
	uint8_t  *event_info, uint32_t len)
{
	return 0;
}

/**
 * wma_ndp_initiator_rsp_event_handler() -NDP initiator rsp event handler
 * @handle: wma handle
 * @event_info: event handler data
 * @len: length of event_info
 *
 * Handler for WMI_NDP_INITIATOR_RSP_EVENTID
 * Return: 0 on success, negative errno on failure
 */
static int wma_ndp_initiator_rsp_event_handler(void *handle,
	uint8_t  *event_info, uint32_t len)
{
	return 0;
}

/**
 * wma_ndp_register_all_event_handlers() - Register all NDP event handlers
 * @wma_handle: WMA context
 *
 * Register the event handlers for NAN data path events from firmware.
 *
 * Return: None
 */
void wma_ndp_register_all_event_handlers(tp_wma_handle wma_handle)
{
	WMA_LOGD(FL("Register WMI_NDP_INITIATOR_RSP_EVENTID"));
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
		WMI_NDP_INITIATOR_RSP_EVENTID,
		wma_ndp_initiator_rsp_event_handler);

	WMA_LOGD(FL("Register WMI_NDP_RESPONDER_RSP_EVENTID"));
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
		WMI_NDP_RESPONDER_RSP_EVENTID,
		wma_ndp_responder_rsp_event_handler);

	WMA_LOGD(FL("Register WMI_NDP_END_RSP_EVENTID"));
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
		WMI_NDP_END_RSP_EVENTID,
		wma_ndp_end_response_event_handler);

	WMA_LOGD(FL("Register WMI_NDP_INDICATION_EVENTID"));
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
		WMI_NDP_INDICATION_EVENTID,
		wma_ndp_indication_event_handler);

	WMA_LOGD(FL("Register WMI_NDP_CONFIRM_EVENTID"));
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
		WMI_NDP_CONFIRM_EVENTID,
		wma_ndp_confirm_event_handler);

	WMA_LOGD(FL("Register WMI_NDP_END_INDICATION_EVENTID"));
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
		WMI_NDP_END_INDICATION_EVENTID,
		wma_ndp_end_indication_event_handler);
}

/**
 * wma_ndp_unregister_all_event_handlers() - Unregister all NDP event handlers
 * @wma_handle: WMA context
 *
 * Register the event handlers for NAN data path events from firmware.
 *
 * Return: None
 */
void wma_ndp_unregister_all_event_handlers(tp_wma_handle wma_handle)
{
	WMA_LOGD(FL("Unregister WMI_NDP_INITIATOR_RSP_EVENTID"));
	wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
		WMI_NDP_INITIATOR_RSP_EVENTID);

	WMA_LOGD(FL("Unregister WMI_NDP_RESPONDER_RSP_EVENTID"));
	wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
		WMI_NDP_RESPONDER_RSP_EVENTID);

	WMA_LOGD(FL("Unregister WMI_NDP_END_RSP_EVENTID"));
	wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
		WMI_NDP_END_RSP_EVENTID);

	WMA_LOGD(FL("Unregister WMI_NDP_INDICATION_EVENTID"));
	wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
		WMI_NDP_INDICATION_EVENTID);

	WMA_LOGD(FL("Unregister WMI_NDP_CONFIRM_EVENTID"));
	wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
		WMI_NDP_CONFIRM_EVENTID);

	WMA_LOGD(FL("Unregister WMI_NDP_END_INDICATION_EVENTID"));
	wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
		WMI_NDP_END_INDICATION_EVENTID);
}

/**
 * wma_ndp_add_wow_wakeup_event() - Add Wake on Wireless event for NDP
 * @wma_handle: WMA context
 * @enable: dis/enable flag to enable the bit for WOW_NAN_DATA_EVENT
 *
 * Enables the firmware to wake up the host on NAN data path event.
 * All NDP events such as NDP_INDICATION, NDP_CONFIRM, etc. use the
 * same event. They can be distinguished using their TLV tags.
 *
 * Return: none
 */
void wma_ndp_add_wow_wakeup_event(tp_wma_handle wma_handle,
						bool enable)
{
	wma_add_wow_wakeup_event(wma_handle, WOW_NAN_DATA_EVENT, enable);
}

/**
 * wma_ndp_get_eventid_from_tlvtag() - map tlv tag to event id
 * @tag: WMI TLV tag
 *
 * map the tag to known NDP event fixed_param tags and return the
 * corresponding NDP event id.
 *
 * Return: 0 if TLV tag is invalid
 *           else return corresponding WMI event id
 */
static int wma_ndp_get_eventid_from_tlvtag(uint32_t tag)
{
	uint32_t event_id;

	switch (tag) {
	case WMITLV_TAG_STRUC_wmi_ndp_initiator_rsp_event_fixed_param:
		event_id = WMI_NDP_INITIATOR_RSP_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_ndp_responder_rsp_event_fixed_param:
		event_id = WMI_NDP_RESPONDER_RSP_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_ndp_end_rsp_event_fixed_param:
		event_id = WMI_NDP_END_RSP_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_ndp_indication_event_fixed_param:
		event_id = WMI_NDP_INDICATION_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_ndp_confirm_event_fixed_param:
		event_id = WMI_NDP_CONFIRM_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_ndp_end_indication_event_fixed_param:
		event_id = WMI_NDP_END_INDICATION_EVENTID;
		break;

	default:
		event_id = 0;
		WMA_LOGE(FL("Unknown tag: %d"), tag);
		break;
	}

	WMA_LOGI(FL("For tag %d WMI event 0x%x"), tag, event_id);
	return event_id;
}

/**
 * wma_ndp_wow_event_callback() - NAN data path wow event callback
 * @handle: WMA handle
 * @event: event buffer
 * @len: length of @event buffer
 *
 * The wow event WOW_REASON_NAN_DATA is followed by the payload of the event
 * which generated the wow event.
 * Payload is 4 bytes of length followed by event buffer. First 4 bytes
 * of event buffer is common tlv header, which is a combination
 * of tag (higher 2 bytes) and length (lower 2 bytes). The tag is used to
 * identify the event which triggered wow event.
 *
 * Return: none
 */
void wma_ndp_wow_event_callback(void *handle, void *event,
						  uint32_t len)
{
	uint32_t id;
	int tlv_ok_status = 0;
	void *wmi_cmd_struct_ptr = NULL;
	uint32_t tag = WMITLV_GET_TLVTAG(WMITLV_GET_HDR(event));

	/* Reverse map fixed params tag to EVENT_ID */
	id = wma_ndp_get_eventid_from_tlvtag(tag);
	if (!id) {
		WMA_LOGE(FL("Invalid  Tag: %d"), tag);
		return;
	}

	tlv_ok_status = wmitlv_check_and_pad_event_tlvs(handle, event, len,
							id,
							&wmi_cmd_struct_ptr);
	if (tlv_ok_status != 0) {
		WMA_LOGE(FL("Invalid Tag: %d could not check and pad tlvs"),
			 tag);
		return;
	}

	switch (id) {
	case WMI_NDP_INITIATOR_RSP_EVENTID:
		wma_ndp_initiator_rsp_event_handler(handle,
						wmi_cmd_struct_ptr, len);
		break;

	case WMI_NDP_RESPONDER_RSP_EVENTID:
		wma_ndp_responder_rsp_event_handler(handle,
						wmi_cmd_struct_ptr, len);
		break;

	case WMI_NDP_END_RSP_EVENTID:
		wma_ndp_end_response_event_handler(handle,
						wmi_cmd_struct_ptr, len);
		break;

	case WMI_NDP_INDICATION_EVENTID:
		wma_ndp_indication_event_handler(handle,
						wmi_cmd_struct_ptr, len);
		break;

	case WMI_NDP_CONFIRM_EVENTID:
		wma_ndp_confirm_event_handler(handle,
						wmi_cmd_struct_ptr, len);
		break;

	case WMI_NDP_END_INDICATION_EVENTID:
		wma_ndp_end_indication_event_handler(handle,
						wmi_cmd_struct_ptr, len);
		break;

	default:
		WMA_LOGE(FL("Unknown tag: %d"), tag);
		break;
	}
	wmitlv_free_allocated_event_tlvs(id, &wmi_cmd_struct_ptr);
}

/**
 * wma_add_bss_ndi_mode() - Process BSS creation request while adding NaN
 * Data interface
 * @wma: wma handle
 * @add_bss: Parameters for ADD_BSS command
 *
 * Sends VDEV_START command to firmware
 * Return: None
 */
void wma_add_bss_ndi_mode(tp_wma_handle wma, tpAddBssParams add_bss)
{
	ol_txrx_pdev_handle pdev;
	struct wma_vdev_start_req req;
	ol_txrx_peer_handle peer = NULL;
	struct wma_target_req *msg;
	uint8_t vdev_id, peer_id;
	VOS_STATUS status;
	uint8_t nss_2g, nss_5g;

	WMA_LOGE("%s: enter", __func__);
	if (NULL == wma_find_vdev_by_addr(wma, add_bss->bssId, &vdev_id)) {
		WMA_LOGE("%s: Failed to find vdev", __func__);
		goto send_fail_resp;
	}
	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		goto send_fail_resp;
	}

	nss_2g = wma->interfaces[vdev_id].nss_2g;
	nss_5g = wma->interfaces[vdev_id].nss_5g;
	wma_set_bss_rate_flags(&wma->interfaces[vdev_id], add_bss);

	peer = ol_txrx_find_peer_by_addr(pdev, add_bss->selfMacAddr, &peer_id);
	if (!peer) {
		WMA_LOGE("%s Failed to find peer %pM", __func__,
			add_bss->selfMacAddr);
		goto send_fail_resp;
	}

	msg = wma_fill_vdev_req(wma, vdev_id, WDA_ADD_BSS_REQ,
			WMA_TARGET_REQ_TYPE_VDEV_START, add_bss,
			WMA_VDEV_START_REQUEST_TIMEOUT);
	if (!msg) {
		WMA_LOGE("%s Failed to allocate vdev request vdev_id %d",
			 __func__, vdev_id);
		goto send_fail_resp;
	}

	add_bss->staContext.staIdx = ol_txrx_local_peer_id(peer);

	/*
	 * beacon_intval, dtim_period, hidden_ssid, is_dfs, ssid
	 * will be ignored for NDI device.
	 */
	vos_mem_zero(&req, sizeof(req));
	req.vdev_id = vdev_id;
	req.chan = add_bss->currentOperChannel;
	req.chan_offset = add_bss->currentExtChannel;
	req.vht_capable = add_bss->vhtCapable;
	req.max_txpow = add_bss->maxTxPower;
	req.oper_mode = add_bss->operMode;

	status = wma_vdev_start(wma, &req, VOS_FALSE);
	if (status != VOS_STATUS_SUCCESS) {
		wma_remove_vdev_req(wma, vdev_id,
			WMA_TARGET_REQ_TYPE_VDEV_START);
		goto send_fail_resp;
	}
	WMA_LOGI("%s: vdev start request for NDI sent to target", __func__);

	/* Initialize protection mode to no protection */
	if (wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
		WMI_VDEV_PARAM_PROTECTION_MODE,
		IEEE80211_PROT_NONE)) {
		WMA_LOGE("Failed to initialize protection mode");
	}

	return;
send_fail_resp:
	add_bss->status = VOS_STATUS_E_FAILURE;
	wma_send_msg(wma, WDA_ADD_BSS_RSP, (void *)add_bss, 0);
}

/**
 * wma_delete_all_nan_remote_peers() - Delete all nan peer
 * @wma:  wma handle
 * @vdev_id: vdev id
 *
 * Return: void
 */
void wma_delete_all_nan_remote_peers(tp_wma_handle wma, uint32_t vdev_id)
{
	ol_txrx_vdev_handle vdev;
	ol_txrx_peer_handle peer, temp;

	if (!wma || vdev_id > wma->max_bssid)
		return;

	vdev = wma->interfaces[vdev_id].handle;
	if (!vdev)
		return;

	/* remove all remote peers of ndi*/
	adf_os_spin_lock_bh(&vdev->pdev->peer_ref_mutex);

	temp = NULL;
	TAILQ_FOREACH_REVERSE(peer, &vdev->peer_list,
		peer_list_t, peer_list_elem) {
		if (temp) {
			adf_os_spin_unlock_bh(&vdev->pdev->peer_ref_mutex);
			if (adf_os_atomic_read(
				&temp->delete_in_progress) == 0)
				wma_remove_peer(wma, temp->mac_addr.raw,
					vdev_id, temp, VOS_FALSE);
			adf_os_spin_lock_bh(&vdev->pdev->peer_ref_mutex);
		}
		/* self peer is deleted last */
		if (peer == TAILQ_FIRST(&vdev->peer_list)) {
			WMA_LOGE("%s: self peer removed", __func__);
			break;
		} else
			temp = peer;
	}
	adf_os_spin_unlock_bh(&vdev->pdev->peer_ref_mutex);

	/* remove ndi self peer last */
	peer = TAILQ_FIRST(&vdev->peer_list);
	wma_remove_peer(wma, peer->mac_addr.raw, vdev_id, peer,
			false);
}
