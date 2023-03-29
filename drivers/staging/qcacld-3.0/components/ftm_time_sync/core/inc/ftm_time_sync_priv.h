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
 * DOC: Declare private API which shall be used internally only
 * in ftm_time_sync component. This file shall include prototypes of
 * ftm_time_sync parsing and send logic.
 *
 * Note: This API should be never accessed out of ftm_time_sync component.
 */

#ifndef _FTM_TIME_SYNC_PRIV_STRUCT_H_
#define _FTM_TIME_SYNC_PRIV_STRUCT_H_

#include <qdf_list.h>
#include <qdf_types.h>
#include "ftm_time_sync_objmgr.h"
#include "wlan_ftm_time_sync_public_struct.h"

#define WLAN_FTM_TIME_SYNC_PAIR_MAX 32

/**
 * struct wlan_time_sync_pair - wlan time sync pair
 * @qtime_master: master qtime
 * @qtime_slave: slave qtime
 */
struct wlan_time_sync_pair {
	uint64_t qtime_master;
	uint64_t qtime_slave;
};

/**
 * struct ftm_time_sync_vdev_priv - Private object to be stored in vdev
 * @qtime_ref: qtime ref
 * @mac_ref: mac time ref
 * @time_pair: array of master/slave qtime pair
 */

struct ftm_time_sync_priv {
	uint64_t qtime_ref;
	uint64_t mac_ref;
	struct wlan_time_sync_pair time_pair[WLAN_FTM_TIME_SYNC_PAIR_MAX];
};

/**
 * struct ftm_time_sync_cfg - Cfg ini param for FTM time sync
 * @enable: FTM time_sync feature enable/disable
 * @mode: Aggregated/burst mode applicable iff enable = 1
 * @role: Slave/Master Role applicable iff enable = 1
 */
struct ftm_time_sync_cfg {
	bool enable;
	enum ftm_time_sync_mode mode;
	enum ftm_time_sync_role role;
};

/**
 * struct ftm_time_sync_psoc_priv - Private object to be stored in psoc
 * @psoc: pointer to psoc object
 * @cfg_param: INI config param for ftm time sync
 */
struct ftm_time_sync_psoc_priv {
	struct wlan_objmgr_psoc *psoc;
	struct ftm_time_sync_cfg cfg_param;
};

/**
 * struct ftm_time_sync_vdev_priv - Private object to be stored in vdev
 * @vdev: pointer to vdev object
 * @ftm_ts_priv: time sync private struct
 * @rx_ops: rx operations for ftm time sync
 * @tx_ops: tx operations for ftm time sync
 * @ftm_time_sync_mutex: mutex to access ftm time sync priv members
 * @ftm_time_sync_work: work to capture audio qtime and send it to FW
 * @time_sync_interval: interval between two qtime capture
 * @num_qtime_pair: number of qmaster and qslave pair derived
 * @num_reads: number of times the qtime to be captured
 * @valid: send qtime to FW only if this is true
 * @bssid: bssid of connected AP
 */
struct ftm_time_sync_vdev_priv {
	struct wlan_objmgr_vdev *vdev;
	struct ftm_time_sync_priv ftm_ts_priv;
	struct wlan_ftm_time_sync_rx_ops rx_ops;
	struct wlan_ftm_time_sync_tx_ops tx_ops;
	qdf_mutex_t ftm_time_sync_mutex;
	struct qdf_delayed_work ftm_time_sync_work;
	uint32_t time_sync_interval;
	int num_qtime_pair;
	int num_reads;
	bool valid;
	struct qdf_mac_addr bssid;
};

#endif /* End  of _FTM_TIME_SYNC_PRIV_STRUCT_H_ */
