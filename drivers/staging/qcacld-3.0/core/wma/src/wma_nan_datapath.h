/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
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

#include <sir_common.h>
#include <ani_global.h>
#include "wma.h"
#include "sir_api.h"
#include "sme_nan_datapath.h"

static inline int wma_ndp_wow_event_callback(void *handle, void *event,
					     uint32_t len, uint32_t event_id)
{
	return 0;
}

static inline uint32_t wma_ndp_get_eventid_from_tlvtag(uint32_t tag)
{
	return 0;
}

#ifdef WLAN_FEATURE_NAN
#define WMA_IS_VDEV_IN_NDI_MODE(intf, vdev_id) \
				(WMI_VDEV_TYPE_NDI == intf[vdev_id].type)

void wma_add_sta_ndi_mode(tp_wma_handle wma, tpAddStaParams add_sta);

/**
 * wma_update_hdd_cfg_ndp() - Update target device NAN datapath capability
 * @wma_handle: pointer to WMA context
 * @tgt_cfg: Pointer to target configuration data structure
 *
 * Return: none
 */
static inline void wma_update_hdd_cfg_ndp(tp_wma_handle wma_handle,
					struct wma_tgt_cfg *tgt_cfg)
{
	tgt_cfg->nan_datapath_enabled = wma_handle->nan_datapath_enabled;
}

void wma_delete_sta_req_ndi_mode(tp_wma_handle wma,
					tpDeleteStaParams del_sta);

/**
 * wma_is_ndi_active() - Determines of the nan data iface is active
 * @wma_handle: handle to wma context
 *
 * Returns: true if ndi active, flase otherwise
 */
static inline bool wma_is_ndi_active(tp_wma_handle wma_handle)
{
	int i;

	for (i = 0; i < wma_handle->max_bssid; i++) {
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_NDI &&
				wma_handle->interfaces[i].peer_count > 0)
			return true;
	}
	return false;
}
#else /* WLAN_FEATURE_NAN */
#define WMA_IS_VDEV_IN_NDI_MODE(intf, vdev_id) (false)
static inline void wma_update_hdd_cfg_ndp(tp_wma_handle wma_handle,
					struct wma_tgt_cfg *tgt_cfg)
{
	return;
}

static inline void wma_delete_sta_req_ndi_mode(tp_wma_handle wma,
					tpDeleteStaParams del_sta)
{
}
static inline void wma_add_sta_ndi_mode(tp_wma_handle wma,
					tpAddStaParams add_sta) {}

static inline bool wma_is_ndi_active(tp_wma_handle wma_handle) { return false; }
#endif /* WLAN_FEATURE_NAN */

#endif /* __WMA_NAN_DATAPATH_H */
