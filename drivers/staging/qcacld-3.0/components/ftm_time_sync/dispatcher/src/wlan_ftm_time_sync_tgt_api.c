/*
 *Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
 * DOC: Implements public API for ftm time sync to interact with target/WMI
 */

#include "wlan_ftm_time_sync_tgt_api.h"
#include "ftm_time_sync_main.h"
#include "wlan_ftm_time_sync_public_struct.h"
#include <wmi_unified_api.h>

QDF_STATUS
tgt_ftm_ts_start_stop_evt(struct wlan_objmgr_psoc *psoc,
			  struct ftm_time_sync_start_stop_params *param)
{
	struct wlan_objmgr_vdev *vdev;
	struct ftm_time_sync_vdev_priv *vdev_priv;
	uint8_t vdev_id;

	vdev_id = param->vdev_id;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    FTM_TIME_SYNC_ID);
	if (!vdev) {
		ftm_time_sync_err("failed to get vdev");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_priv = ftm_time_sync_vdev_get_priv(vdev);

	qdf_mutex_acquire(&vdev_priv->ftm_time_sync_mutex);

	vdev_priv->time_sync_interval = param->timer_interval;
	vdev_priv->num_reads = param->num_reads * 2;

	if (vdev_priv->num_reads && vdev_priv->time_sync_interval) {
		vdev_priv->num_reads--;
		qdf_mutex_release(&vdev_priv->ftm_time_sync_mutex);
		qdf_delayed_work_start(&vdev_priv->ftm_time_sync_work,
				       param->timer_interval);
	} else if (vdev_priv->time_sync_interval == 0) {
		qdf_mutex_release(&vdev_priv->ftm_time_sync_mutex);
		vdev_priv->ftm_ts_priv.qtime_ref = param->qtime;
		vdev_priv->ftm_ts_priv.mac_ref = param->mac_time;
		qdf_delayed_work_stop_sync(&vdev_priv->ftm_time_sync_work);
	} else {
		qdf_mutex_release(&vdev_priv->ftm_time_sync_mutex);
	}

	ftm_time_sync_vdev_put_ref(vdev);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tgt_ftm_ts_offset_evt(struct wlan_objmgr_psoc *psoc,
				 struct ftm_time_sync_offset *param)
{
	struct ftm_time_sync_vdev_priv *vdev_priv;
	struct wlan_objmgr_vdev *vdev;
	uint8_t vdev_id;
	int iter;

	vdev_id = param->vdev_id;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    FTM_TIME_SYNC_ID);
	if (!vdev) {
		ftm_time_sync_err("failed to get vdev");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_priv = ftm_time_sync_vdev_get_priv(vdev);

	vdev_priv->num_qtime_pair = param->num_qtime <
			FTM_TIME_SYNC_QTIME_PAIR_MAX ? param->num_qtime :
			FTM_TIME_SYNC_QTIME_PAIR_MAX;

	for (iter = 0; iter < vdev_priv->num_qtime_pair; iter++) {
		vdev_priv->ftm_ts_priv.time_pair[iter].qtime_master =
						param->pairs[iter].qtime_master;
		vdev_priv->ftm_ts_priv.time_pair[iter].qtime_slave =
						param->pairs[iter].qtime_slave;
	}

	ftm_time_sync_vdev_put_ref(vdev);

	return QDF_STATUS_SUCCESS;
}

