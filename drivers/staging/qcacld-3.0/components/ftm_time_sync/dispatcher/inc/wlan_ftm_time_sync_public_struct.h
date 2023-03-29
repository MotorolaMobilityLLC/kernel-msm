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
 * DOC: Declare various struct, macros which are used for obj mgmt  in ftm time
 * sync.
 *
 * Note: This file shall not contain public API's prototype/declarations.
 *
 */

#ifndef _WLAN_TIME_SYNC_FTM_PUBLIC_STRUCT_H_
#define _WLAN_TIME_SYNC_FTM_PUBLIC_STRUCT_H_

struct wlan_objmgr_psoc;

/**
 * enum ftm_time_sync_mode - ftm time sync modes
 * @FTM_TIMESYNC_AGGREGATED_MODE: Ftm time sync aggregated mode
 *				  Only one aggregated offset is provided
 * @FTM_TIMESYNC_BURST_MODE: Ftm time sync burst mode, offset for each FTM
 *			     frame is provided
 */
enum ftm_time_sync_mode {
	FTM_TIMESYNC_AGGREGATED_MODE,
	FTM_TIMESYNC_BURST_MODE,
};

/**
 * enum ftm_time_sync_role - ftm time sync role
 * @FTM_TIMESYNC_SLAVE_ROLE: Slave/STA role
 * @FTM_TIMESYNC_MASTER_ROLE: Master/SAP role
 */
enum ftm_time_sync_role {
	FTM_TIMESYNC_SLAVE_ROLE,
	FTM_TIMESYNC_MASTER_ROLE,
};

/**
 * enum ftm_time_sync_sta_state - ftm time sync sta states
 * @FTM_TIME_SYNC_STA_CONNECTED: STA connected to AP
 * @FTM_TIME_SYNC_STA_DISCONNECTED: STA disconnected
 */
enum ftm_time_sync_sta_state {
	FTM_TIME_SYNC_STA_CONNECTED,
	FTM_TIME_SYNC_STA_DISCONNECTED,
};

/**
 * enum ftm_time_sync_bss_state - ftm time sync bss states
 * @FTM_TIME_SYNC_BSS_STARTED: BSS started
 * @FTM_TIME_SYNC_BSS_STOPPED: BSS stopped
 */
enum ftm_time_sync_bss_state {
	FTM_TIME_SYNC_BSS_STARTED,
	FTM_TIME_SYNC_BSS_STOPPED,
};
/**
 * struct wlan_ftm_time_sync_tx_ops - structure of tx operation function
 *				      pointers for ftm time_sync component
 * @ftm_time_sync_send_qtime: send qtime wmi cmd to FW
 * @ftm_time_sync_send_trigger: send ftm time sync trigger cmd
 *
 */
struct wlan_ftm_time_sync_tx_ops {
	QDF_STATUS (*ftm_time_sync_send_qtime)(struct wlan_objmgr_psoc *psoc,
					       uint32_t vdev_id,
					       uint64_t lpass_ts);
	QDF_STATUS (*ftm_time_sync_send_trigger)(struct wlan_objmgr_psoc *psoc,
						 uint32_t vdev_id, bool mode);
};

/**
 * struct wlan_ftm_time_sync_rx_ops - structure of rx operation function
 *				      pointers for ftm time_sync component
 * @ftm_time_sync_register_start_stop: register ftm time_sync start stop event
 * @ftm_time_sync_regiser_master_slave_offset: register master slave qtime
 *					       offset event
 */
struct wlan_ftm_time_sync_rx_ops {
	QDF_STATUS (*ftm_time_sync_register_start_stop)
					(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*ftm_time_sync_regiser_master_slave_offset)
					(struct wlan_objmgr_psoc *psoc);
};
#endif /*_WLAN_TIME_SYNC_FTM_PUBLIC_STRUCT_H_ */
