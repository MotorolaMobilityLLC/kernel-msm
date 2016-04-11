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
 * DOC: wma_nan_datapath.h
 *
 * WMA NAN Data path API specification
 */

#ifndef __WMA_NAN_DATAPATH_H
#define __WMA_NAN_DATAPATH_H

#include "wma.h"
#include "sirApi.h"

#ifdef WLAN_FEATURE_NAN_DATAPATH
#define WMA_IS_VDEV_IN_NDI_MODE(intf, vdev_id) \
				(WMI_VDEV_TYPE_NDI == intf[vdev_id].type)

void wma_add_bss_ndi_mode(tp_wma_handle wma, tpAddBssParams add_bss);

/**
 * wma_update_hdd_cfg_ndp() - Update target device NAN datapath capability
 * @wma_handle: pointer to WMA context
 * @hdd_tgt_cfg: Pointer to HDD target configuration data structure
 *
 * Return: none
 */
static inline void wma_update_hdd_cfg_ndp(tp_wma_handle wma_handle,
					  struct hdd_tgt_cfg *tgt_cfg)
{
	tgt_cfg->nan_datapath_enabled = wma_handle->nan_datapath_enabled;
}
void wma_delete_all_nan_remote_peers(tp_wma_handle wma,
					uint32_t vdev_id);
void wma_ndp_register_all_event_handlers(tp_wma_handle wma_handle);
void wma_ndp_unregister_all_event_handlers(tp_wma_handle wma_handle);
void wma_ndp_add_wow_wakeup_event(tp_wma_handle wma_handle,
						bool enable);
void wma_ndp_wow_event_callback(void *handle, void *event, uint32_t len);
#else
static inline void wma_add_bss_ndi_mode(tp_wma_handle wma,
					tpAddBssParams add_bss)
{
}
static inline void wma_update_hdd_cfg_ndp(tp_wma_handle wma_handle,
					  struct hdd_tgt_cfg *tgt_cfg) {}
static inline void wma_ndp_register_all_event_handlers(
					tp_wma_handle wma_handle) {}
static inline void wma_ndp_unregister_all_event_handlers(
					tp_wma_handle wma_handle) {}
#define WMA_IS_VDEV_IN_NDI_MODE(intf, vdev_id) (false)
#define wma_delete_all_nan_remote_peers(x, y)   ((void)0)
static inline void wma_ndp_add_wow_wakeup_event(tp_wma_handle wma_handle,
						bool enable) {}
static inline void wma_ndp_wow_event_callback(void *handle, void *event,
						uint32_t len) {}
#endif /* WLAN_FEATURE_NAN_DATAPATH */
#endif /* __WMA_NAN_DATAPATH_H */
