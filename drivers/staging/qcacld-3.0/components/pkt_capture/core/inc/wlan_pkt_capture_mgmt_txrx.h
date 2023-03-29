/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Declare mgmt txrx APIs which shall be used internally only
 * in pkt_capture component.
 * Note: These APIs should be never accessed out of pkt_capture component.
 */

#ifndef _WLAN_PKT_CAPTURE_MGMT_TXRX_H_
#define _WLAN_PKT_CAPTURE_MGMT_TXRX_H_

#include "wlan_pkt_capture_public_structs.h"

#define PKTCAPTURE_PKT_FORMAT_8023	(0)
#define PKTCAPTURE_PKT_FORMAT_80211	(1)
#define WLAN_INVALID_TID		(31)
#define RESERVE_BYTES			(100)
#define RATE_LIMIT			(16)
#define INVALID_RSSI_FOR_TX		(-128)
#define PKTCAPTURE_RATECODE_CCK		(1)

/**
 * pkt_capture_process_mgmt_tx_data() - process management tx packets
 * @pdev: pointer to pdev object
 * @params: management offload event params
 * @nbuf: netbuf
 * @status: status
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pkt_capture_process_mgmt_tx_data(struct wlan_objmgr_pdev *pdev,
				 struct mgmt_offload_event_params *params,
				 qdf_nbuf_t nbuf,
				 uint8_t status);
/**
 * pkt_capture_mgmt_tx() - process mgmt tx completion
 * for pkt capture mode
 * @pdev: pointer to pdev object
 * @nbuf: netbuf
 * @chan_freq: channel freq
 * @preamble_type: preamble_type
 *
 * Return: none
 */
void pkt_capture_mgmt_tx(struct wlan_objmgr_pdev *pdev,
			 qdf_nbuf_t nbuf,
			 uint16_t chan_freq,
			 uint8_t preamble_type);

/**
 * pkt_capture_mgmt_tx_completion() - process mgmt tx completion
 * for pkt capture mode
 * @pdev: pointer to pdev object
 * @desc_id: desc_id
 * @status: status
 * @params: management offload event params
 *
 * Return: none
 */
void
pkt_capture_mgmt_tx_completion(struct wlan_objmgr_pdev *pdev,
			       uint32_t desc_id,
			       uint32_t status,
			       struct mgmt_offload_event_params *params);

/**
 * pkt_capture_mgmt_rx_ops() - Register packet capture mgmt rx ops
 * @psoc: psoc context
 * @is_register: register if true, unregister if false
 *
 * This funciton registers or deregisters rx callback
 * to mgmt txrx component.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pkt_capture_mgmt_rx_ops(struct wlan_objmgr_psoc *psoc,
				   bool is_register);

/**
 * pkt_capture_mgmt_status_map() - map Tx status for MGMT packets
 * with packet capture Tx status
 * @status: Tx status
 *
 * Return: pkt_capture_tx_status enum
 */
enum pkt_capture_tx_status
pkt_capture_mgmt_status_map(uint8_t status);
#endif
