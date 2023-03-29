/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
 * DOC: Declare public API related to the ftm time_sync called by north bound
 * HDD/OSIF/LIM
 */

#ifndef _FTM_TIME_SYNC_UCFG_API_H_
#define _FTM_TIME_SYNC_UCFG_API_H_

#include <qdf_status.h>
#include <qdf_types.h>
#include "ftm_time_sync_objmgr.h"
#include "wlan_ftm_time_sync_public_struct.h"

#ifdef FEATURE_WLAN_TIME_SYNC_FTM

/**
 * ucfg_ftm_time_sync_init() - FTM time sync component initialization.
 *
 * This function initializes the ftm time sync component and registers
 * the handlers which are invoked on vdev creation.
 *
 * Return: For successful registration - QDF_STATUS_SUCCESS,
 *	   else QDF_STATUS error codes.
 */
QDF_STATUS ucfg_ftm_time_sync_init(void);

/**
 * ucfg_ftm_time_sync_deinit() - FTM time sync component deinit.
 *
 * This function deinits ftm time sync component.
 *
 * Return: None
 */
void ucfg_ftm_time_sync_deinit(void);

/**
 * ucfg_is_ftm_time_sync_enable() - FTM time sync feature enable/disable
 * @psoc: psoc context
 *
 * This function advertises whether the ftm time sync feature is enabled or not
 *
 * Return: true if enable else false
 */
bool ucfg_is_ftm_time_sync_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_ftm_time_sync_set_enable() - FTM time sync feature set enable/disable
 * @psoc: psoc context
 * @value: value to be set
 *
 * This function enables/disables the feature.
 *
 * Return: None
 */
void ucfg_ftm_time_sync_set_enable(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * ucfg_ftm_time_sync_update_sta_connect_state() - Handler for STA state change
 * @vdev: STA vdev
 * @state: connected/disconnected state
 * @bssid: bssid of connected AP
 *
 * This function triggers the FTM time sync feature in case of connection and
 * stops the ftm sync feature in case of disconnection.
 *
 * Return: None
 */
void
ucfg_ftm_time_sync_update_sta_connect_state(struct wlan_objmgr_vdev *vdev,
					    enum ftm_time_sync_sta_state state,
					    struct qdf_mac_addr bssid);

/**
 * ucfg_ftm_time_sync_update_bss_state() - Handler to notify bss start/stop
 * @vdev: SAP vdev
 * @ap_state: BSS start/stop state
 *
 * This function triggers the FTM time sync feature in case of bss start and
 * stops the ftm sync feature in case of bss stop.
 *
 * Return: None.
 */
void ucfg_ftm_time_sync_update_bss_state(struct wlan_objmgr_vdev *vdev,
					 enum ftm_time_sync_bss_state ap_state);

/**
 * ucfg_ftm_time_sync_show() - Show the ftm time sync offset values derived
 * @vdev: vdev context
 * @buf: buffer in which the values to be written
 *
 * This function prints the offset values derived after ftm time sync
 * between the qtime of STA(slave) and connected SAP(master).
 *
 * Return: number of bytes written in buffer
 */
ssize_t ucfg_ftm_time_sync_show(struct wlan_objmgr_vdev *vdev, char *buf);
#else

static inline
QDF_STATUS ucfg_ftm_time_sync_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_ftm_time_sync_deinit(void)
{
}

static inline
bool ucfg_is_ftm_time_sync_enable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
void ucfg_ftm_time_sync_set_enable(struct wlan_objmgr_psoc *psoc, bool value)
{
}

static inline void
ucfg_ftm_time_sync_update_sta_connect_state(struct wlan_objmgr_vdev *vdev,
					    enum ftm_time_sync_sta_state state)
{
}

static inline void
ucfg_ftm_time_sync_update_bss_state(struct wlan_objmgr_vdev *vdev,
				    enum ftm_time_sync_bss_state ap_state)
{
}

static inline
ssize_t ucfg_ftm_time_sync_show(struct wlan_objmgr_vdev *vdev, char *buf)
{
	return 0;
}
#endif /* FEATURE_WLAN_TIME_SYNC_FTM */
#endif /* _FTM_TIME_SYNC_UCFG_API_H_ */
