/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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
 * DOC: Declare private API which shall be used internally only
 * in pkt_capture component. This file shall include prototypes of
 * pkt_capture parsing and send logic.
 *
 * Note: This API should be never accessed out of pkt_capture component.
 */

#ifndef _WLAN_PKT_CAPTURE_PRIV_STRUCT_H_
#define _WLAN_PKT_CAPTURE_PRIV_STRUCT_H_

#include "wlan_pkt_capture_objmgr.h"
#include "wlan_pkt_capture_public_structs.h"
#include "wlan_pkt_capture_mon_thread.h"

/**
 * struct pkt_capture_cfg - struct to store config values
 * @pkt_capture_mode: packet capture mode
 * @pkt_capture_config: config for trigger, qos and beacon frames
 */
struct pkt_capture_cfg {
	enum pkt_capture_mode pkt_capture_mode;
	enum pkt_capture_config pkt_capture_config;
};

/**
 * struct pkt_capture_cb_context - packet capture callback context
 * @mon_cb: monitor callback function pointer
 * @mon_ctx: monitor callback context
 */
struct pkt_capture_cb_context {
	QDF_STATUS (*mon_cb)(void *, qdf_nbuf_t);
	void *mon_ctx;
};

/**
 * struct pkt_capture_vdev_priv - Private object to be stored in vdev
 * @vdev: pointer to vdev object
 * @mon_ctx: pointer to packet capture mon context
 * @cb_ctx: pointer to packet capture mon callback context
 * @frame_filter: config filter set by vendor command
 * @cfg_params: packet capture config params
 * @rx_avg_rssi: avg rssi of rx data packets
 * @ppdu_stats_q: list used for storing smu related ppdu stats
 * @lock_q: spinlock for ppdu_stats q
 * @tx_nss: nss of tx data packets received from ppdu stats
 */
struct pkt_capture_vdev_priv {
	struct wlan_objmgr_vdev *vdev;
	struct pkt_capture_mon_context *mon_ctx;
	struct pkt_capture_cb_context *cb_ctx;
	struct pkt_capture_frame_filter frame_filter;
	struct pkt_capture_cfg cfg_params;
	int32_t rx_avg_rssi;
	qdf_list_t ppdu_stats_q;
	qdf_spinlock_t lock_q;
	uint8_t tx_nss;
};

/**
 * struct pkt_psoc_priv - Private object to be stored in psoc
 * @psoc: pointer to psoc object
 * @cfg_param: INI config params for packet capture
 * @cb_obj: struct contaning callback pointers
 * @rx_ops: rx ops
 * @tx_ops: tx ops
 */
struct pkt_psoc_priv {
	struct wlan_objmgr_psoc *psoc;
	struct pkt_capture_cfg cfg_param;
	struct pkt_capture_callbacks cb_obj;
	struct wlan_pkt_capture_rx_ops rx_ops;
	struct wlan_pkt_capture_tx_ops tx_ops;
};
#endif /* End  of _WLAN_PKT_CAPTURE_PRIV_STRUCT_H_ */
