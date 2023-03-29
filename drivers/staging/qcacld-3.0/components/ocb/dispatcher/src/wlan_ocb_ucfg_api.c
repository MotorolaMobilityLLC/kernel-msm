/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains ocb north bound interface definitions
 */

#include <scheduler_api.h>
#include <wlan_defs.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_reg_services_api.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_ocb_public_structs.h>
#include <wlan_ocb_ucfg_api.h>
#include <wlan_ocb_tgt_api.h>
#include <wlan_lmac_if_def.h>
#include "wlan_ocb_main.h"

/**
 * wlan_ocb_get_tx_ops() - get target interface tx operations
 * @pdev: pdev handle
 *
 * Return: fp to target interface operations
 */
static struct wlan_ocb_tx_ops *
wlan_ocb_get_tx_ops(struct wlan_objmgr_pdev *pdev)
{
	struct ocb_pdev_obj *ocb_obj;

	ocb_obj = wlan_get_pdev_ocb_obj(pdev);
	if (!ocb_obj) {
		ocb_err("failed to get OCB pdev object");
		return NULL;
	}

	return &ocb_obj->ocb_txops;
}

QDF_STATUS ucfg_ocb_init(void)
{
	QDF_STATUS status;

	ocb_notice("ocb module dispatcher init");
	status = wlan_objmgr_register_pdev_create_handler(WLAN_UMAC_COMP_OCB,
		ocb_pdev_obj_create_notification, NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		ocb_err("Failed to register pdev create handler for ocb");

		return status;
	}

	status = wlan_objmgr_register_pdev_destroy_handler(WLAN_UMAC_COMP_OCB,
		ocb_pdev_obj_destroy_notification, NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		ocb_err("Failed to register pdev destroy handler for ocb");
		goto fail_delete_pdev;
	}

	return status;

fail_delete_pdev:
	wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_OCB,
		ocb_pdev_obj_create_notification, NULL);

	return status;
}

QDF_STATUS ucfg_ocb_deinit(void)
{
	QDF_STATUS status;

	ocb_notice("ocb module dispatcher deinit");
	status = wlan_objmgr_unregister_pdev_destroy_handler(WLAN_UMAC_COMP_OCB,
				ocb_pdev_obj_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		ocb_err("Failed to unregister pdev destroy handler");

	status = wlan_objmgr_unregister_pdev_create_handler(WLAN_UMAC_COMP_OCB,
				ocb_pdev_obj_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		ocb_err("Failed to unregister pdev create handler");

	return status;
}

QDF_STATUS ocb_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_OCB_NB_ID);
	if (!pdev) {
		ocb_err("Failed to get pdev handle");

		return QDF_STATUS_E_FAILURE;
	}
	tgt_ocb_register_ev_handler(pdev);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_OCB_NB_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ocb_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_OCB_NB_ID);
	if (!pdev) {
		ocb_err("Failed to get pdev handle");
		return QDF_STATUS_E_FAILURE;
	}
	tgt_ocb_unregister_ev_handler(pdev);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_OCB_NB_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_ocb_set_txrx_pdev_id(struct wlan_objmgr_psoc *psoc,
				     uint8_t pdev_id)
{
	struct wlan_objmgr_pdev *pdev;
	struct ocb_pdev_obj *ocb_obj;

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_OCB_NB_ID);
	if (!pdev) {
		ocb_err("Failed to get pdev handle");
		return QDF_STATUS_E_FAILURE;
	}
	ocb_obj = wlan_get_pdev_ocb_obj(pdev);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_OCB_NB_ID);
	if (!ocb_obj) {
		ocb_err("OCB object is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	ocb_obj->dp_pdev_id = pdev_id;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_ocb_update_dp_handle(struct wlan_objmgr_psoc *psoc,
				     void *dp_soc)
{
	struct wlan_objmgr_pdev *pdev;
	struct ocb_pdev_obj *ocb_obj;

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_OCB_NB_ID);
	if (!pdev) {
		ocb_err("Failed to get pdev handle");
		return QDF_STATUS_E_FAILURE;
	}
	ocb_obj = wlan_get_pdev_ocb_obj(pdev);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_OCB_NB_ID);
	if (!ocb_obj) {
		ocb_err("OCB object is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	ocb_obj->dp_soc = dp_soc;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_ocb_config_channel(struct wlan_objmgr_pdev *pdev)
{
	QDF_STATUS status;
	struct ocb_config *config;
	struct ocb_pdev_obj *ocb_obj;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_ocb_tx_ops *tx_ops;

	ocb_obj = wlan_get_pdev_ocb_obj(pdev);
	if (!ocb_obj || !ocb_obj->channel_config) {
		ocb_alert("The request could not be found");
		return QDF_STATUS_E_FAILURE;
	}

	config = ocb_obj->channel_config;
	ocb_debug("Set config to vdev%d", config->vdev_id);
	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		ocb_err("Null pointer for psoc");
		return QDF_STATUS_E_INVAL;
	}
	tx_ops = wlan_ocb_get_tx_ops(pdev);
	if (tx_ops && tx_ops->ocb_set_config)
		status = tx_ops->ocb_set_config(psoc, ocb_obj->channel_config);
	else
		status = QDF_STATUS_E_IO;

	if (QDF_IS_STATUS_SUCCESS(status)) {
		ocb_debug("Set channel cmd is sent to southbound");
	} else {
		ocb_err("Failed to set channel config to southbound");

		if (ocb_obj->channel_config) {
			/*
			 * On success case, backup parameters will be released
			 * after channel info is synced to DP
			 */
			ocb_info("release the backed config parameters");
			qdf_mem_free(ocb_obj->channel_config);
			ocb_obj->channel_config = NULL;
		}
	}

	return status;
}

QDF_STATUS ucfg_ocb_set_channel_config(struct wlan_objmgr_vdev *vdev,
				       struct ocb_config *config,
				       ocb_sync_callback set_config_cb,
				       void *arg)
{
	QDF_STATUS status;
	enum wlan_vdev_state state;
	uint32_t i;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_ocb_tx_ops *tx_ops;
	struct ocb_pdev_obj *ocb_obj;
	struct ocb_callbacks *ocb_cbs;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		ocb_err("Null pointer for pdev");
		return QDF_STATUS_E_INVAL;
	}
	ocb_obj = wlan_get_pdev_ocb_obj(pdev);
	if (!ocb_obj) {
		ocb_alert("Failed to get OCB vdev object");
		return QDF_STATUS_E_IO;
	}

	if (!config) {
		ocb_err("Invalid config input");
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < config->channel_count; i++) {
		if (WLAN_REG_CHAN_TO_BAND(wlan_reg_freq_to_chan(pdev,
					  config->channels[i].chan_freq))
				== BAND_2G)
			config->channels[i].ch_mode = MODE_11G;
		else
			config->channels[i].ch_mode = MODE_11A;
	}

	/*
	 * backup the new configuration,
	 * it will be released after target's response
	 */
	ocb_obj->channel_config = ocb_copy_config(config);
	if (!ocb_obj->channel_config) {
		ocb_err("Failed to backup config");
		return QDF_STATUS_E_NOMEM;
	}

	ocb_cbs = &ocb_obj->ocb_cbs;
	ocb_cbs->ocb_set_config_callback = set_config_cb;
	ocb_cbs->ocb_set_config_context = arg;

	state = wlan_vdev_mlme_get_state(vdev);
	if (state != WLAN_VDEV_S_START) {
		/* Vdev is not started, start it */
		ocb_debug("OCB vdev%d is not up", config->vdev_id);
		status = ocb_vdev_start(ocb_obj);
	} else {
		psoc = wlan_vdev_get_psoc(vdev);
		tx_ops = wlan_ocb_get_tx_ops(pdev);
		if (tx_ops && tx_ops->ocb_set_config)
			status = tx_ops->ocb_set_config(psoc, config);
		else
			status = QDF_STATUS_E_IO;

		ocb_debug("Set config to vdev%d", config->vdev_id);
		if (QDF_IS_STATUS_SUCCESS(status))
			ocb_debug("Set channel cmd is sent to southbound");
		else
			ocb_err("Failed to set channel config to southbound");
	}

	if (QDF_IS_STATUS_ERROR(status) && ocb_obj->channel_config) {
		/*
		 * On success case, backup parameters will be released
		 * after channel info is synced to DP
		 */
		ocb_info("release the backed config parameters");
		qdf_mem_free(ocb_obj->channel_config);
		ocb_obj->channel_config = NULL;
	}

	return status;
}

QDF_STATUS ucfg_ocb_set_utc_time(struct wlan_objmgr_vdev *vdev,
				 struct ocb_utc_param *utc)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_ocb_tx_ops *tx_ops;

	psoc = wlan_vdev_get_psoc(vdev);
	pdev = wlan_vdev_get_pdev(vdev);
	if (!psoc || !pdev) {
		ocb_err("Null pointer for psoc/pdev");
		return QDF_STATUS_E_INVAL;
	}
	tx_ops = wlan_ocb_get_tx_ops(pdev);

	if (!tx_ops) {
		ocb_alert("tx_ops is null");
		return QDF_STATUS_E_IO;
	}
	if (!tx_ops->ocb_set_utc_time) {
		ocb_alert("ocb_set_utc_time is null");
		return QDF_STATUS_E_IO;
	}

	status = tx_ops->ocb_set_utc_time(psoc, utc);
	if (QDF_IS_STATUS_ERROR(status))
		ocb_err("Failed to set UTC time to southbound");
	else
		ocb_debug("UTC time is sent to southbound");

	return status;
}

QDF_STATUS ucfg_ocb_start_timing_advert(struct wlan_objmgr_vdev *vdev,
					struct ocb_timing_advert_param *ta)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_ocb_tx_ops *tx_ops;

	psoc = wlan_vdev_get_psoc(vdev);
	pdev = wlan_vdev_get_pdev(vdev);
	if (!psoc || !pdev) {
		ocb_err("Null pointer for psoc/pdev");
		return QDF_STATUS_E_INVAL;
	}
	tx_ops = wlan_ocb_get_tx_ops(pdev);
	if (!tx_ops) {
		ocb_alert("tx_ops is null");
		return QDF_STATUS_E_IO;
	}
	if (!tx_ops->ocb_start_timing_advert) {
		ocb_alert("ocb_start_timing_advert is null");
		return QDF_STATUS_E_IO;
	}
	status = tx_ops->ocb_start_timing_advert(psoc, ta);
	if (QDF_IS_STATUS_ERROR(status))
		ocb_err("Failed to sent start timing advert to southbound");
	else
		ocb_debug("Start timing advert is sent to southbound");

	return status;
}

QDF_STATUS ucfg_ocb_stop_timing_advert(struct wlan_objmgr_vdev *vdev,
				       struct ocb_timing_advert_param *ta)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_ocb_tx_ops *tx_ops;

	psoc = wlan_vdev_get_psoc(vdev);
	pdev = wlan_vdev_get_pdev(vdev);
	if (!psoc || !pdev) {
		ocb_err("Null pointer for psoc/pdev");
		return QDF_STATUS_E_INVAL;
	}
	tx_ops = wlan_ocb_get_tx_ops(pdev);
	if (!tx_ops) {
		ocb_alert("tx_ops is null");
		return QDF_STATUS_E_IO;
	}
	if (!tx_ops->ocb_stop_timing_advert) {
		ocb_alert("ocb_stop_timing_advert is null");
		return QDF_STATUS_E_IO;
	}
	status = tx_ops->ocb_stop_timing_advert(psoc, ta);
	if (QDF_IS_STATUS_ERROR(status))
		ocb_err("Failed to sent start timing advert to southbound");
	else
		ocb_debug("Start timing advert is sent to southbound");

	return status;
}

QDF_STATUS ucfg_ocb_get_tsf_timer(struct wlan_objmgr_vdev *vdev,
				  struct ocb_get_tsf_timer_param *req,
				  ocb_sync_callback get_tsf_cb,
				  void *arg)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct ocb_get_tsf_timer_param request;
	struct wlan_ocb_tx_ops *tx_ops;
	struct ocb_callbacks *ocb_cbs;
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		ocb_err("Null pointer for pdev");
		return QDF_STATUS_E_INVAL;
	}

	tx_ops = wlan_ocb_get_tx_ops(pdev);
	if (!tx_ops) {
		ocb_alert("tx_ops is null");
		return QDF_STATUS_E_IO;
	}
	if (!tx_ops->ocb_get_tsf_timer) {
		ocb_alert("ocb_get_tsf_timer is null");
		return QDF_STATUS_E_IO;
	}

	ocb_cbs = wlan_ocb_get_callbacks(pdev);
	ocb_cbs->ocb_get_tsf_timer_context = arg;
	ocb_cbs->ocb_get_tsf_timer_callback = get_tsf_cb;
	request.vdev_id =  req->vdev_id;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		ocb_err("Null pointer for psoc");
		return QDF_STATUS_E_INVAL;
	}
	status = tx_ops->ocb_get_tsf_timer(psoc, &request);
	if (QDF_IS_STATUS_ERROR(status))
		ocb_err("Failed to sent get tsf timer to southbound");
	else
		ocb_debug("Get tsf timer is sent to southbound");

	return status;
}

QDF_STATUS ucfg_ocb_dcc_get_stats(struct wlan_objmgr_vdev *vdev,
				  struct ocb_dcc_get_stats_param *request,
				  ocb_sync_callback dcc_get_stats_cb,
				  void *arg)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct ocb_callbacks *ocb_cbs;
	struct wlan_ocb_tx_ops *tx_ops;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		ocb_err("Null pointer for pdev");
		return QDF_STATUS_E_INVAL;
	}
	ocb_cbs = wlan_ocb_get_callbacks(pdev);
	ocb_cbs->ocb_dcc_get_stats_context = arg;
	ocb_cbs->ocb_dcc_get_stats_callback = dcc_get_stats_cb;

	tx_ops = wlan_ocb_get_tx_ops(pdev);
	if (!tx_ops) {
		ocb_alert("tx_ops is null");
		return QDF_STATUS_E_IO;
	}

	if (!tx_ops->ocb_dcc_get_stats) {
		ocb_alert("ocb_dcc_get_stats is null");
		return QDF_STATUS_E_IO;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		ocb_err("Null pointer for psoc");
		return QDF_STATUS_E_INVAL;
	}
	status = tx_ops->ocb_dcc_get_stats(psoc, request);
	if (QDF_IS_STATUS_ERROR(status))
		ocb_err("Failed to sent get dcc stats to southbound");
	else
		ocb_debug("Get dcc stats is sent to southbound");

	return status;
}

QDF_STATUS ucfg_ocb_dcc_clear_stats(struct wlan_objmgr_vdev *vdev,
				    uint16_t vdev_id,
				    uint32_t bitmap)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_ocb_tx_ops *tx_ops;
	struct ocb_dcc_clear_stats_param clear_stats_param;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		ocb_err("Null pointer for pdev");
		return QDF_STATUS_E_INVAL;
	}
	tx_ops = wlan_ocb_get_tx_ops(pdev);
	if (!tx_ops) {
		ocb_alert("tx_ops is null");
		return QDF_STATUS_E_IO;
	}

	if (!tx_ops->ocb_dcc_clear_stats) {
		ocb_alert("ocb_dcc_clear_stats is null");
		return QDF_STATUS_E_IO;
	}

	clear_stats_param.vdev_id = vdev_id;
	clear_stats_param.dcc_stats_bitmap = bitmap;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		ocb_err("Null pointer for psoc");
		return QDF_STATUS_E_INVAL;
	}
	status = tx_ops->ocb_dcc_clear_stats(psoc, &clear_stats_param);
	if (QDF_IS_STATUS_ERROR(status))
		ocb_err("Failed to sent clear dcc stats to southbound");
	else
		ocb_debug("clear dcc stats is sent to southbound");

	return status;
}

QDF_STATUS ucfg_ocb_dcc_update_ndl(struct wlan_objmgr_vdev *vdev,
				   struct ocb_dcc_update_ndl_param *request,
				   ocb_sync_callback dcc_update_ndl_cb,
				   void *arg)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct ocb_callbacks *ocb_cbs;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_ocb_tx_ops *tx_ops;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		ocb_err("Null pointer for pdev");
		return QDF_STATUS_E_INVAL;
	}
	ocb_cbs = wlan_ocb_get_callbacks(pdev);
	ocb_cbs->ocb_dcc_update_ndl_context = arg;
	ocb_cbs->ocb_dcc_update_ndl_callback = dcc_update_ndl_cb;

	tx_ops = wlan_ocb_get_tx_ops(pdev);
	if (!tx_ops) {
		ocb_alert("tx_ops is null");
		return QDF_STATUS_E_IO;
	}
	if (!tx_ops->ocb_dcc_update_ndl) {
		ocb_alert("dcc_update_ndl is null");
		return QDF_STATUS_E_IO;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		ocb_err("Null pointer for psoc");
		return QDF_STATUS_E_INVAL;
	}
	status = tx_ops->ocb_dcc_update_ndl(psoc, request);
	if (QDF_IS_STATUS_ERROR(status))
		ocb_err("Failed to sent update ndl to southbound");
	else
		ocb_debug("Update ndl is sent to southbound");

	return status;
}

QDF_STATUS ucfg_ocb_register_for_dcc_stats_event(struct wlan_objmgr_pdev *pdev,
				void *ctx, ocb_sync_callback dcc_stats_cb)
{
	struct ocb_callbacks *ocb_cbs;

	if (!pdev) {
		ocb_err("Null pointer for pdev");
		return QDF_STATUS_E_INVAL;
	}
	ocb_cbs = wlan_ocb_get_callbacks(pdev);

	if (!ocb_cbs) {
		ocb_err("Failed to register dcc stats callback");
		return QDF_STATUS_E_FAILURE;
	}
	ocb_cbs->ocb_dcc_stats_event_context = ctx;
	ocb_cbs->ocb_dcc_stats_event_callback = dcc_stats_cb;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_ocb_register_vdev_start(struct wlan_objmgr_pdev *pdev,
				QDF_STATUS (*ocb_start)(struct ocb_config *))
{
	struct ocb_callbacks *ocb_cbs;

	if (!pdev) {
		ocb_err("Null pointer for pdev");
		return QDF_STATUS_E_INVAL;
	}
	ocb_cbs = wlan_ocb_get_callbacks(pdev);

	if (!ocb_cbs) {
		ocb_err("Failed to register dcc stats callback");
		return QDF_STATUS_E_FAILURE;
	}
	ocb_cbs->start_ocb_vdev = ocb_start;

	return QDF_STATUS_SUCCESS;
}
