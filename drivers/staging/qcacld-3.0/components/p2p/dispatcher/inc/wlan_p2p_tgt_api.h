/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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
 * DOC: Contains p2p south bound interface definitions
 */

#ifndef _WLAN_P2P_TGT_API_H_
#define _WLAN_P2P_TGT_API_H_

#include <qdf_types.h>
#include <qdf_nbuf.h>

struct scan_event;
struct wlan_objmgr_psoc;
struct wlan_objmgr_peer;
struct p2p_noa_info;
struct p2p_lo_event;
struct mgmt_rx_event_params;
enum mgmt_frame_type;
struct p2p_set_mac_filter_evt;

#ifdef FEATURE_P2P_LISTEN_OFFLOAD

/**
 * tgt_p2p_lo_event_cb() - Listen offload stop request
 * @psoc: soc object
 * @event_info: lo stop event buffer
 *
 * This function gets called from target interface.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_lo_event_cb(struct wlan_objmgr_psoc *psoc,
			       struct p2p_lo_event *event_info);

/**
 * tgt_p2p_register_lo_ev_handler() - register lo event
 * @psoc: soc object
 *
 * p2p tgt api to register listen offload event handler.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_register_lo_ev_handler(
	struct wlan_objmgr_psoc *psoc);

/**
 * tgt_p2p_unregister_lo_ev_handler() - unregister lo event
 * @psoc: soc object
 *
 * p2p tgt api to unregister listen offload event handler.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_unregister_lo_ev_handler(
	struct wlan_objmgr_psoc *psoc);
#else
static inline QDF_STATUS tgt_p2p_register_lo_ev_handler(
	struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS tgt_p2p_unregister_lo_ev_handler(
	struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * tgt_p2p_register_macaddr_rx_filter_evt_handler() - register add mac rx
 *    filter status event
 * @psoc: soc object
 * @register: register or unregister
 *
 * p2p tgt api to register add mac rx filter status event
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_register_macaddr_rx_filter_evt_handler(
	struct wlan_objmgr_psoc *psoc, bool register);

/**
 * tgt_p2p_register_noa_ev_handler() - register noa event
 * @psoc: soc object
 *
 * p2p tgt api to register noa event handler.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_register_noa_ev_handler(
	struct wlan_objmgr_psoc *psoc);

/**
 * tgt_p2p_unregister_noa_ev_handler() - unregister noa event
 * @psoc: soc object
 *
 * p2p tgt api to unregister noa event handler.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_unregister_noa_ev_handler(
	struct wlan_objmgr_psoc *psoc);

/**
 * tgt_p2p_scan_event_cb() - Callback for scan event
 * @vdev: vdev object
 * @event: event information
 * @arg: registered arguments
 *
 * This function gets called from scan component when getting P2P
 * scan event.
 *
 * Return: None
 */
void tgt_p2p_scan_event_cb(struct wlan_objmgr_vdev *vdev,
	struct scan_event *event, void *arg);

/**
 * tgt_p2p_mgmt_download_comp_cb() - Callback for mgmt frame tx
 * complete
 * @context: tx context
 * @buf: buffer address
 * @free: need to free or not
 *
 * This function gets called from mgmt tx/rx component when mgmt
 * frame tx complete.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_mgmt_download_comp_cb(void *context,
	qdf_nbuf_t buf, bool free);

/**
 * tgt_p2p_mgmt_ota_comp_cb() - Callback for mgmt frame tx ack
 * @context: tx context
 * @buf: buffer address
 * @status: tx status
 * @tx_compl_params: tx complete parameters
 *
 * This function gets called from mgmt tx/rx component when getting
 * mgmt frame tx ack.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_mgmt_ota_comp_cb(void *context, qdf_nbuf_t buf,
	uint32_t status, void *tx_compl_params);

/**
 * tgt_p2p_mgmt_frame_rx_cb() - Callback for rx mgmt frame
 * @psoc: soc context
 * @peer: peer context
 * @buf: rx buffer
 * @mgmt_rx_params: mgmt rx parameters
 * @frm_type: frame type
 *
 * This function gets called from mgmt tx/rx component when rx mgmt
 * frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_mgmt_frame_rx_cb(struct wlan_objmgr_psoc *psoc,
	struct wlan_objmgr_peer *peer, qdf_nbuf_t buf,
	struct mgmt_rx_event_params *mgmt_rx_params,
	enum mgmt_frame_type frm_type);
/**
 * tgt_p2p_noa_event_cb() - Callback for noa event
 * @psoc: soc object
 * @event_info: noa event information
 *
 * This function gets called from target interface.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS tgt_p2p_noa_event_cb(struct wlan_objmgr_psoc *psoc,
		struct p2p_noa_info *event_info);

/**
 * tgt_p2p_add_mac_addr_status_event_cb() - Callback for set mac addr filter evt
 * @psoc: soc object
 * @event_info: event information type of p2p_set_mac_filter_evt
 *
 * This function gets called from target interface.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
tgt_p2p_add_mac_addr_status_event_cb(
	struct wlan_objmgr_psoc *psoc,
	struct p2p_set_mac_filter_evt *event_info);

#endif /* _WLAN_P2P_TGT_API_H_ */
