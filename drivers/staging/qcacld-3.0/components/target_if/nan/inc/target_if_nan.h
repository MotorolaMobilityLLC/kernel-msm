/*
 * Copyright (c) 2017-2018, 2020 The Linux Foundation. All rights reserved.
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
 * DOC: contains nan target if declarations
 */

#ifndef _WLAN_NAN_TGT_IF_H_
#define _WLAN_NAN_TGT_IF_H_

#include "qdf_types.h"

#include <qdf_mem.h>
#include <qdf_status.h>
#include <wmi_unified_api.h>
#include <wmi_unified_priv.h>
#include <wmi_unified_param.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_scan_tgt_api.h>
#include <target_if.h>
#include "nan_public_structs.h"

struct wlan_objmgr_psoc;

/**
 * target_if_nan_get_tx_ops() - retrieve the nan tx_ops
 * @psoc: psoc context
 *
 * API to retrieve the nan tx_ops from the psoc context
 *
 * Return: nan tx_ops pointer
 */
struct wlan_nan_tx_ops *target_if_nan_get_tx_ops(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_nan_get_rx_ops() - retrieve the nan rx_ops
 * @psoc: psoc context
 *
 * API to retrieve the nan rx_ops from the psoc context
 *
 * Return: nan rx_ops pointer
 */
struct wlan_nan_rx_ops *target_if_nan_get_rx_ops(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_nan_register_tx_ops() - registers nan tx ops
 * @tx_ops: tx ops
 *
 * Return: none
 */
void target_if_nan_register_tx_ops(struct wlan_nan_tx_ops *tx_ops);

/**
 * target_if_nan_register_rx_ops() - registers nan rx ops
 * @tx_ops: rx ops
 *
 * Return: none
 */
void target_if_nan_register_rx_ops(struct wlan_nan_rx_ops *rx_ops);

/**
 * target_if_nan_register_events() - registers with NDP events
 * @psoc: pointer to psoc object
 *
 * Return: status of operation
 */
QDF_STATUS target_if_nan_register_events(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_nan_deregister_events() - registers nan rx ops
 * @psoc: pointer to psoc object
 *
 * Return: status of operation
 */
QDF_STATUS target_if_nan_deregister_events(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_nan_rsp_handler() - Target IF handler for NAN Discovery events
 * @scn: target handle
 * @data: event buffer
 * @len: event buffer length
 *
 * Return: 0 for success or error code
 */
int target_if_nan_rsp_handler(ol_scn_t scn, uint8_t *data, uint32_t len);

/**
 * target_if_nan_set_vdev_feature_config() - Init NAN feature config params
 * @psoc: Pointer to PSOC Object
 * @vdev_id: vdev_id of the current vdev
 *
 * This function updates NAN feature config bitmap to firmware
 */
void target_if_nan_set_vdev_feature_config(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id);
#endif /* _WIFI_POS_TGT_IF_H_ */
