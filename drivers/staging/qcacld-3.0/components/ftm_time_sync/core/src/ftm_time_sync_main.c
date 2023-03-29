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
 * DOC: Implement various notification handlers which are accessed
 * internally in ftm_time_sync component only.
 */

#include "ftm_time_sync_main.h"
#include "target_if_ftm_time_sync.h"
#include "wlan_objmgr_vdev_obj.h"
#include "cfg_ftm_time_sync.h"
#include "cfg_ucfg_api.h"
#include <pld_common.h>

void ftm_time_sync_set_enable(struct wlan_objmgr_psoc *psoc, bool value)
{
	struct ftm_time_sync_psoc_priv *psoc_priv;

	if (!psoc) {
		ftm_time_sync_err("psoc is NULL");
		return;
	}

	psoc_priv = ftm_time_sync_psoc_get_priv(psoc);
	if (!psoc_priv) {
		ftm_time_sync_err("psoc priv is NULL");
		return;
	}

	psoc_priv->cfg_param.enable &= value;
}

bool ftm_time_sync_is_enable(struct wlan_objmgr_psoc *psoc)
{
	struct ftm_time_sync_psoc_priv *psoc_priv;

	if (!psoc) {
		ftm_time_sync_err("psoc is NULL");
		return false;
	}

	psoc_priv = ftm_time_sync_psoc_get_priv(psoc);
	if (!psoc_priv) {
		ftm_time_sync_err("psoc priv is NULL");
		return false;
	}

	return psoc_priv->cfg_param.enable;
}

enum ftm_time_sync_mode ftm_time_sync_get_mode(struct wlan_objmgr_psoc *psoc)
{
	struct ftm_time_sync_psoc_priv *psoc_priv;

	if (!psoc) {
		ftm_time_sync_err("psoc is NULL");
		return FTM_TIMESYNC_AGGREGATED_MODE;
	}

	psoc_priv = ftm_time_sync_psoc_get_priv(psoc);
	if (!psoc_priv) {
		ftm_time_sync_err("psoc priv is NULL");
		return FTM_TIMESYNC_AGGREGATED_MODE;
	}

	return psoc_priv->cfg_param.mode;
}

enum ftm_time_sync_role ftm_time_sync_get_role(struct wlan_objmgr_psoc *psoc)
{
	struct ftm_time_sync_psoc_priv *psoc_priv;

	if (!psoc) {
		ftm_time_sync_err("psoc is NULL");
		return FTM_TIMESYNC_SLAVE_ROLE;
	}

	psoc_priv = ftm_time_sync_psoc_get_priv(psoc);
	if (!psoc_priv) {
		ftm_time_sync_err("psoc priv is NULL");
		return FTM_TIMESYNC_SLAVE_ROLE;
	}

	return psoc_priv->cfg_param.role;
}

static void ftm_time_sync_work_handler(void *arg)
{
	struct ftm_time_sync_vdev_priv *vdev_priv = arg;
	struct wlan_objmgr_psoc *psoc;
	qdf_device_t qdf_dev;
	QDF_STATUS status;
	uint8_t vdev_id;
	uint64_t lpass_ts;

	if (!vdev_priv) {
		ftm_time_sync_err("ftm vdev priv is Null");
		return;
	}

	psoc = wlan_vdev_get_psoc(vdev_priv->vdev);
	if (!psoc) {
		ftm_time_sync_err("Failed to get psoc");
		return;
	}

	vdev_id = wlan_vdev_get_id(vdev_priv->vdev);

	qdf_dev = wlan_psoc_get_qdf_dev(psoc);
	pld_get_audio_wlan_timestamp(qdf_dev->dev, PLD_TRIGGER_NEGATIVE_EDGE,
				     &lpass_ts);

	qdf_mutex_acquire(&vdev_priv->ftm_time_sync_mutex);

	if (vdev_priv->num_reads) {
		vdev_priv->num_reads--;
		qdf_mutex_release(&vdev_priv->ftm_time_sync_mutex);
		qdf_delayed_work_start(&vdev_priv->ftm_time_sync_work,
				       vdev_priv->time_sync_interval);
	} else {
		qdf_mutex_release(&vdev_priv->ftm_time_sync_mutex);
	}

	if (vdev_priv->valid) {
		status = vdev_priv->tx_ops.ftm_time_sync_send_qtime(
						psoc, vdev_id, lpass_ts);
		if (status != QDF_STATUS_SUCCESS)
			ftm_time_sync_err("send_ftm_time_sync_qtime failed %d",
					  status);
		vdev_priv->valid = false;
	} else {
		vdev_priv->valid = true;
	}
}

QDF_STATUS
ftm_time_sync_vdev_create_notification(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct ftm_time_sync_vdev_priv *vdev_priv;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		ftm_time_sync_err("Failed to get psoc");
		return QDF_STATUS_E_INVAL;
	}

	if (!ftm_time_sync_is_enable(psoc))
		return QDF_STATUS_SUCCESS;

	vdev_priv = qdf_mem_malloc(sizeof(*vdev_priv));
	if (!vdev_priv) {
		status = QDF_STATUS_E_NOMEM;
		goto exit;
	}

	status = wlan_objmgr_vdev_component_obj_attach(
				vdev, WLAN_UMAC_COMP_FTM_TIME_SYNC,
				(void *)vdev_priv, QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		ftm_time_sync_err("Failed to attach priv with vdev");
		goto free_vdev_priv;
	}

	vdev_priv->vdev = vdev;
	status = qdf_delayed_work_create(&vdev_priv->ftm_time_sync_work,
					 ftm_time_sync_work_handler, vdev_priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		ftm_time_sync_err("Failed to create ftm time sync work\n");
		goto free_vdev_priv;
	}

	qdf_mutex_create(&vdev_priv->ftm_time_sync_mutex);

	target_if_ftm_time_sync_register_tx_ops(&vdev_priv->tx_ops);
	target_if_ftm_time_sync_register_rx_ops(&vdev_priv->rx_ops);

	vdev_priv->rx_ops.ftm_time_sync_register_start_stop(psoc);
	vdev_priv->rx_ops.ftm_time_sync_regiser_master_slave_offset(psoc);

	vdev_priv->valid = true;

	goto exit;

free_vdev_priv:
	qdf_mem_free(vdev_priv);
	status = QDF_STATUS_E_INVAL;
exit:
	return status;
}

static QDF_STATUS
ftm_time_sync_deregister_wmi_events(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		ftm_time_sync_err("Failed to get psoc");
		return QDF_STATUS_E_INVAL;
	}

	status = target_if_ftm_time_sync_unregister_ev_handlers(psoc);
	return status;
}

QDF_STATUS
ftm_time_sync_vdev_destroy_notification(struct wlan_objmgr_vdev *vdev,
					void *arg)

{
	struct ftm_time_sync_vdev_priv *vdev_priv = NULL;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		ftm_time_sync_err("Failed to get psoc");
		return QDF_STATUS_E_INVAL;
	}

	if (!ftm_time_sync_is_enable(psoc))
		return QDF_STATUS_SUCCESS;

	vdev_priv = ftm_time_sync_vdev_get_priv(vdev);
	if (!vdev_priv) {
		ftm_time_sync_err("vdev priv is NULL");
		goto exit;
	}

	qdf_mutex_destroy(&vdev_priv->ftm_time_sync_mutex);
	qdf_delayed_work_destroy(&vdev_priv->ftm_time_sync_work);

	ftm_time_sync_deregister_wmi_events(vdev);

	status = wlan_objmgr_vdev_component_obj_detach(
					vdev, WLAN_UMAC_COMP_FTM_TIME_SYNC,
					(void *)vdev_priv);
	if (QDF_IS_STATUS_ERROR(status))
		ftm_time_sync_err("Failed to detach priv with vdev");

	qdf_mem_free(vdev_priv);
	vdev_priv = NULL;

exit:
	return status;
}

static void
ftm_time_sync_cfg_init(struct ftm_time_sync_psoc_priv *psoc_priv)
{
	psoc_priv->cfg_param.enable = cfg_get(psoc_priv->psoc,
					      CFG_ENABLE_TIME_SYNC_FTM);
	psoc_priv->cfg_param.role = cfg_get(psoc_priv->psoc,
					    CFG_TIME_SYNC_FTM_ROLE);
	psoc_priv->cfg_param.mode = cfg_get(psoc_priv->psoc,
					    CFG_TIME_SYNC_FTM_MODE);
}

QDF_STATUS
ftm_time_sync_psoc_create_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct ftm_time_sync_psoc_priv *psoc_priv;
	QDF_STATUS status;

	psoc_priv = qdf_mem_malloc(sizeof(*psoc_priv));
	if (!psoc_priv)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_component_obj_attach(
				psoc, WLAN_UMAC_COMP_FTM_TIME_SYNC,
				psoc_priv, QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		ftm_time_sync_err("Failed to attach psoc component obj");
		goto free_psoc_priv;
	}

	psoc_priv->psoc = psoc;
	ftm_time_sync_cfg_init(psoc_priv);

	return status;

free_psoc_priv:
	qdf_mem_free(psoc_priv);
	return status;
}

QDF_STATUS
ftm_time_sync_psoc_destroy_notification(struct wlan_objmgr_psoc *psoc,
					void *arg)
{
	struct ftm_time_sync_psoc_priv *psoc_priv;
	QDF_STATUS status;

	psoc_priv = ftm_time_sync_psoc_get_priv(psoc);
	if (!psoc_priv) {
		ftm_time_sync_err("psoc priv is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_psoc_component_obj_detach(
					psoc, WLAN_UMAC_COMP_FTM_TIME_SYNC,
					psoc_priv);
	if (QDF_IS_STATUS_ERROR(status)) {
		ftm_time_sync_err("Failed to detach psoc component obj");
		return status;
	}

	qdf_mem_free(psoc_priv);
	return status;
}

QDF_STATUS ftm_time_sync_send_trigger(struct wlan_objmgr_vdev *vdev)
{
	struct ftm_time_sync_vdev_priv *vdev_priv;
	struct wlan_objmgr_psoc *psoc;
	enum ftm_time_sync_mode mode;
	uint8_t vdev_id;
	QDF_STATUS status;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		ftm_time_sync_err("Failed to get psoc");
		return QDF_STATUS_E_INVAL;
	}

	vdev_priv = ftm_time_sync_vdev_get_priv(vdev);
	if (!vdev_priv) {
		ftm_time_sync_err("Failed to get ftm time sync vdev_priv");
		return QDF_STATUS_E_INVAL;
	}

	vdev_id = wlan_vdev_get_id(vdev_priv->vdev);
	mode = ftm_time_sync_get_mode(psoc);

	status = vdev_priv->tx_ops.ftm_time_sync_send_trigger(psoc,
							      vdev_id, mode);
	if (QDF_IS_STATUS_ERROR(status))
		ftm_time_sync_err("send_ftm_time_sync_trigger failed %d",
				  status);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ftm_time_sync_stop(struct wlan_objmgr_vdev *vdev)
{
	struct ftm_time_sync_vdev_priv *vdev_priv;
	int iter;

	vdev_priv = ftm_time_sync_vdev_get_priv(vdev);
	if (!vdev_priv) {
		ftm_time_sync_err("Failed to get ftm time sync vdev_priv");
		return QDF_STATUS_E_INVAL;
	}

	qdf_delayed_work_stop_sync(&vdev_priv->ftm_time_sync_work);

	for (iter = 0; iter < vdev_priv->num_qtime_pair; iter++) {
		vdev_priv->ftm_ts_priv.time_pair[iter].qtime_master = 0;
		vdev_priv->ftm_ts_priv.time_pair[iter].qtime_slave = 0;
	}

	vdev_priv->num_qtime_pair = 0;

	return QDF_STATUS_SUCCESS;
}

ssize_t ftm_time_sync_show(struct wlan_objmgr_vdev *vdev, char *buf)
{
	struct ftm_time_sync_vdev_priv *vdev_priv;
	uint64_t q_master, q_slave;
	ssize_t size = 0;
	int iter;

	vdev_priv = ftm_time_sync_vdev_get_priv(vdev);
	if (!vdev_priv) {
		ftm_time_sync_debug("Failed to get ftm time sync vdev_priv");
		return 0;
	}

	size = qdf_scnprintf(buf, PAGE_SIZE, "%s %pM\n", "BSSID",
			     vdev_priv->bssid.bytes);

	for (iter = 0; iter < vdev_priv->num_qtime_pair; iter++) {
		q_master = vdev_priv->ftm_ts_priv.time_pair[iter].qtime_master;
		q_slave = vdev_priv->ftm_ts_priv.time_pair[iter].qtime_slave;

		size += qdf_scnprintf(buf + size, PAGE_SIZE - size,
				      "%s %llu %s %llu %s %lld\n",
				      "Qtime_master", q_master, "Qtime_slave",
				      q_slave, "Offset", q_slave > q_master ?
				      q_slave - q_master : q_master - q_slave);
	}
	return size;
}

void ftm_time_sync_update_bssid(struct wlan_objmgr_vdev *vdev,
				struct qdf_mac_addr bssid)
{
	struct ftm_time_sync_vdev_priv *vdev_priv;

	vdev_priv = ftm_time_sync_vdev_get_priv(vdev);
	if (!vdev_priv) {
		ftm_time_sync_debug("Failed to get ftm time sync vdev_priv");
		return;
	}

	vdev_priv->bssid = bssid;
}
