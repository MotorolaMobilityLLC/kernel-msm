/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_tdls_mgmt.h
 *
 * TDLS management frames include file
 */

#ifndef _WLAN_TDLS_MGMT_H_
#define _WLAN_TDLS_MGMT_H_

#define TDLS_PUBLIC_ACTION_FRAME_OFFSET 24
#define TDLS_PUBLIC_ACTION_FRAME 4
#define TDLS_PUBLIC_ACTION_DISC_RESP 14
#define TDLS_ACTION_FRAME 12
#define TDLS_80211_PEER_ADDR_OFFSET (TDLS_PUBLIC_ACTION_FRAME + \
				     QDF_MAC_ADDR_SIZE)
#define TDLS_ACTION_FRAME_TYPE_MAX 11

/**
 * struct tdls_rx_mgmt_event - tdls rx mgmt frame event
 * @tdls_soc_obj: tdls soc private object
 * @rx_mgmt: tdls rx mgmt frame structure
 */
struct tdls_rx_mgmt_event {
	struct tdls_soc_priv_obj *tdls_soc_obj;
	struct tdls_rx_mgmt_frame *rx_mgmt;
};

/**
 * tdls_process_mgmt_req() - send a TDLS mgmt request to serialize module
 * @tdls_mgmt_req: tdls management request
 *
 * TDLS request API, called from cfg80211 to send a TDLS frame in
 * serialized manner to PE
 *
 *Return: QDF_STATUS
 */
QDF_STATUS tdls_process_mgmt_req(
			struct tdls_action_frame_request *tdls_mgmt_req);

/**
 * tdls_mgmt_rx_ops() - register or unregister rx callback
 * @psoc: psoc object
 * @isregister: register if true, unregister if false
 *
 * This function registers or unregisters rx callback to mgmt txrx
 * component.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_mgmt_rx_ops(struct wlan_objmgr_psoc *psoc,
	bool isregister);

/**
 * tdls_process_rx_frame() - process tdls rx frames
 * @msg: scheduler msg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_process_rx_frame(struct scheduler_msg *msg);

/**
 * tdls_set_rssi() - Set TDLS RSSI on peer given by mac
 * @vdev: vdev object
 * @mac: MAC address of Peer
 * @rssi: rssi value
 *
 * Set RSSI on TDSL peer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_set_rssi(struct wlan_objmgr_vdev *vdev,
			 uint8_t *mac, int8_t rssi);
#endif

