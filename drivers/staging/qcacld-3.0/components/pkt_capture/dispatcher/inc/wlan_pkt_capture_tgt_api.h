/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: packet capture south bound interface declaration
 */
#ifndef _WLAN_PKT_CAPTURE_TGT_API_H
#define _WLAN_PKT_CAPTURE_TGT_API_H

#include "wlan_pkt_capture_objmgr.h"
#include "wlan_pkt_capture_main.h"
#include "wlan_pkt_capture_public_structs.h"

/**
 * tgt_pkt_capture_register_ev_handler() - register pkt capture ev handler
 * @vdev: pointer to vdev object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_pkt_capture_register_ev_handler(struct wlan_objmgr_vdev *vdev);

/**
 * tgt_pkt_capture_unregister_ev_handler() - unregister pkt capture ev handler
 * @vdev: pointer to vdev object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_pkt_capture_unregister_ev_handler(struct wlan_objmgr_vdev *vdev);

/**
 * tgt_pkt_capture_send_mode() - send packet capture mode to firmware
 * @vdev: pointer to vdev object
 * @mode: packet capture mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_pkt_capture_send_mode(struct wlan_objmgr_vdev *vdev,
			  enum pkt_capture_mode mode);

/**
 * tgt_pkt_capture_send_beacon_interval() - send beacon interval to firmware
 * @vdev: pointer to vdev object
 * @nth_value: Beacon report period
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_pkt_capture_send_beacon_interval(struct wlan_objmgr_vdev *vdev,
				     uint32_t nth_value);

/**
 * tgt_pkt_capture_send_config() - send packet capture config to firmware
 * @vdev: pointer to vdev object
 * @config: packet capture config
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_pkt_capture_send_config(struct wlan_objmgr_vdev *vdev,
			    enum pkt_capture_config config);

#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * tgt_pkt_capture_smu_event() - Receive smart monitor event from firmware
 * @psoc: pointer to psoc
 * @param: smart monitor event params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_pkt_capture_smu_event(struct wlan_objmgr_psoc *psoc,
			  struct smu_event_params *param);
#else
static inline QDF_STATUS
tgt_pkt_capture_smu_event(struct wlan_objmgr_psoc *psoc,
			  struct smu_event_params *param)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#endif /* _WLAN_PKT_CAPTURE_TGT_API_H */
