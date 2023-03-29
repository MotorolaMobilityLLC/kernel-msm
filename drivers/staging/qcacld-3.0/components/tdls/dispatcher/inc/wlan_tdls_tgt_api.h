/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_tdls_tgt_api.h
 *
 * TDLS south bound interface declaration
 */

#ifndef _WLAN_TDLS_TGT_API_H_
#define _WLAN_TDLS_TGT_API_H_
#include <wlan_tdls_public_structs.h>
#include "../../core/src/wlan_tdls_main.h"

/**
 * tgt_tdls_set_fw_state() - invoke lmac tdls update fw
 * @psoc: soc object
 * @tdls_param: update tdls state parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_tdls_set_fw_state(struct wlan_objmgr_psoc *psoc,
				 struct tdls_info *tdls_param);

/**
 * tgt_tdls_set_peer_state() - invoke lmac tdls update peer state
 * @psoc: soc object
 * @peer_param: update tdls peer parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_tdls_set_peer_state(struct wlan_objmgr_psoc *psoc,
				   struct tdls_peer_update_state *peer_param);

/**
 * tgt_tdls_set_offchan_mode() - invoke lmac tdls set off-channel mode
 * @psoc: soc object
 * @param: set tdls off channel parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_tdls_set_offchan_mode(struct wlan_objmgr_psoc *psoc,
				     struct tdls_channel_switch_params *param);

/**
 * tgt_tdls_send_mgmt_rsp() - process tdls mgmt response
 * @pmsg: sheduler msg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_tdls_send_mgmt_rsp(struct scheduler_msg *pmsg);

/**
 * tgt_tdls_send_mgmt_tx_completion() -process tx completion message
 * @pmsg: sheduler msg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_tdls_send_mgmt_tx_completion(struct scheduler_msg *pmsg);

/**
 * tgt_tdls_del_peer_rsp() - handle TDLS del peer response
 * @pmsg: sheduler msg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_tdls_del_peer_rsp(struct scheduler_msg *pmsg);

/**
 * tgt_tdls_add_peer_rsp() - handle TDLS add peer response
 * @pmsg: sheduler msg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_tdls_add_peer_rsp(struct scheduler_msg *pmsg);

/**
 * tgt_tdls_register_ev_handler() - invoke lmac register tdls event handler
 * @psoc: soc object
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS tgt_tdls_register_ev_handler(struct wlan_objmgr_psoc *psoc);

/**
 * tgt_tdls_unregister_ev_handler() - invoke lmac unregister tdls event handler
 * @psoc: soc object
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS tgt_tdls_unregister_ev_handler(struct wlan_objmgr_psoc *psoc);

/**
 * tgt_tdls_event_handler() - The callback registered to WMI for tdls events
 * @psoc: psoc object
 * @info: tdls event info
 *
 * The callback is registered by tgt as tdls rx ops handler.
 *
 * Return: 0 for success or err code.
 */
QDF_STATUS
tgt_tdls_event_handler(struct wlan_objmgr_psoc *psoc,
		       struct tdls_event_info *info);

/**
 * tgt_tdls_mgmt_frame_rx_cb() - callback for rx mgmt frame
 * @psoc: soc context
 * @peer: peer context
 * @buf: rx buffer
 * @mgmt_rx_params: mgmt rx parameters
 * @frm_type: frame type
 *
 * This function gets called from mgmt tx/rx component when rx mgmt
 * received.
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS tgt_tdls_mgmt_frame_rx_cb(struct wlan_objmgr_psoc *psoc,
	struct wlan_objmgr_peer *peer, qdf_nbuf_t buf,
	struct mgmt_rx_event_params *mgmt_rx_params,
	enum mgmt_frame_type frm_type);

/**
 * tgt_tdls_peers_deleted_notification()- notification from legacy lim
 * @psoc: soc object
 * @session_id: session id
 *
 * This function called from legacy lim to notify tdls peer deletion
 *
 * Return: None
 */
void tgt_tdls_peers_deleted_notification(struct wlan_objmgr_psoc *psoc,
					 uint32_t session_id);

/**
 * tgt_tdls_delete_all_peers_indication()- Indication to tdls component
 * @psoc: soc object
 * @session_id: session id
 *
 * This function called from legacy lim to tdls component to delete tdls peers.
 *
 * Return: None
 */
void tgt_tdls_delete_all_peers_indication(struct wlan_objmgr_psoc *psoc,
					  uint32_t session_id);

#endif
