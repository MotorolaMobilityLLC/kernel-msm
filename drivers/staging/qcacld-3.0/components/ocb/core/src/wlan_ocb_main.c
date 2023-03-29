/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
 * DOC: contains core ocb function definitions
 */

#include <qdf_status.h>
#include "scheduler_api.h"
#include "wlan_objmgr_cmn.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include <cdp_txrx_handle.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_ocb.h>
#include "wlan_ocb_main.h"
#include "wlan_ocb_tgt_api.h"
#include "target_if_ocb.h"

/**
 * ocb_get_cmd_type_str() - parse cmd to string
 * @cmd_type: ocb cmd type
 *
 * This function parse ocb cmd to string.
 *
 * Return: command string
 */
static const char *ocb_get_evt_type_str(enum ocb_southbound_event evt_type)
{
	switch (evt_type) {
	case OCB_CHANNEL_CONFIG_STATUS:
		return "OCB channel config status";
	case OCB_TSF_TIMER:
		return "OCB TSF timer";
	case OCB_DCC_STATS_RESPONSE:
		return "OCB DCC get stats response";
	case OCB_NDL_RESPONSE:
		return "OCB NDL indication";
	case OCB_DCC_INDICATION:
		return "OCB DCC stats indication";
	default:
		return "Invalid OCB command";
	}
}

/**
 * ocb_set_chan_info() - Set channel info to dp
 * @dp_soc: data path soc handle
 * @dp_pdev: data path pdev handle
 * @vdev_id: OCB vdev_id
 * @config: channel config parameters
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS ocb_set_chan_info(void *dp_soc,
				    void *dp_pdev,
				    uint32_t vdev_id,
				    struct ocb_config *config)
{
	struct ol_txrx_ocb_set_chan ocb_set_chan;
	struct ol_txrx_ocb_chan_info *ocb_channel_info;

	if (!dp_soc || !dp_pdev) {
		ocb_err("DP global handle is null");
		return QDF_STATUS_E_INVAL;
	}

	ocb_set_chan.ocb_channel_count = config->channel_count;

	/* release old settings */
	ocb_channel_info = cdp_get_ocb_chan_info(dp_soc, vdev_id);
	if (ocb_channel_info)
		qdf_mem_free(ocb_channel_info);

	if (config->channel_count) {
		int i, buf_size;

		buf_size = sizeof(*ocb_channel_info) * config->channel_count;
		ocb_set_chan.ocb_channel_info = qdf_mem_malloc(buf_size);
		if (!ocb_set_chan.ocb_channel_info)
			return QDF_STATUS_E_NOMEM;

		ocb_channel_info = ocb_set_chan.ocb_channel_info;
		for (i = 0; i < config->channel_count; i++) {
			ocb_channel_info[i].chan_freq =
				config->channels[i].chan_freq;
			if (config->channels[i].flags &
				OCB_CHANNEL_FLAG_DISABLE_RX_STATS_HDR)
				ocb_channel_info[i].disable_rx_stats_hdr = 1;
		}
	} else {
		ocb_debug("No channel config to dp");
		ocb_set_chan.ocb_channel_info = NULL;
	}
	ocb_debug("Sync channel config to dp");
	cdp_set_ocb_chan_info(dp_soc, vdev_id, ocb_set_chan);

	return QDF_STATUS_SUCCESS;
}

/**
 * ocb_channel_config_status() - Process set channel config response
 * @evt: response event
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS ocb_channel_config_status(struct ocb_rx_event *evt)
{
	QDF_STATUS status;
	uint32_t vdev_id;
	struct ocb_rx_event *event;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct ocb_pdev_obj *ocb_obj;
	struct ocb_callbacks *cbs;
	struct ocb_set_config_response config_rsp;

	if (!evt) {
		ocb_err("Event buffer is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	event = evt;
	psoc = event->psoc;
	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0, WLAN_OCB_SB_ID);
	if (!pdev) {
		ocb_alert("Pdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ocb_obj = wlan_get_pdev_ocb_obj(pdev);
	if (!ocb_obj) {
		ocb_err("Pdev object is NULL");
		status = QDF_STATUS_E_INVAL;
		goto exit;
	}

	cbs = &ocb_obj->ocb_cbs;
	if (ocb_obj->channel_config) {
		vdev_id = ocb_obj->channel_config->vdev_id;
		config_rsp = event->rsp.channel_cfg_rsp;

		/* Sync channel status to data path */
		if (config_rsp.status == OCB_CHANNEL_CONFIG_SUCCESS)
			ocb_set_chan_info(ocb_obj->dp_soc,
					  ocb_obj->pdev,
					  vdev_id,
					  ocb_obj->channel_config);
		qdf_mem_free(ocb_obj->channel_config);
		ocb_obj->channel_config = NULL;
	} else {
		ocb_err("Failed to sync channel info to DP");
		config_rsp.status = OCB_CHANNEL_CONFIG_FAIL;
	}

	if (cbs->ocb_set_config_callback) {
		cbs->ocb_set_config_callback(cbs->ocb_set_config_context,
					     &config_rsp);
		status = QDF_STATUS_SUCCESS;
	} else {
		ocb_err("ocb_set_config_resp_cb is NULL");
		status = QDF_STATUS_E_INVAL;
	}
exit:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_OCB_SB_ID);

	return status;
}

/**
 * ocb_tsf_timer() - Process get TSF timer response
 * @evt: repsonse event
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS ocb_tsf_timer(struct ocb_rx_event *evt)
{
	QDF_STATUS status;
	struct ocb_rx_event *event;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct ocb_callbacks *cbs;
	struct ocb_get_tsf_timer_response *tsf_timer;

	if (!evt) {
		ocb_err("Event buffer is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	event = evt;
	vdev = event->vdev;
	pdev = wlan_vdev_get_pdev(vdev);
	cbs = wlan_ocb_get_callbacks(pdev);
	tsf_timer = &event->rsp.tsf_timer;
	ocb_debug("TSF timer low=%d, high=%d",
		  tsf_timer->timer_low, tsf_timer->timer_high);
	if (cbs && cbs->ocb_get_tsf_timer_callback) {
		ocb_debug("send TSF timer");
		cbs->ocb_get_tsf_timer_callback(cbs->ocb_get_tsf_timer_context,
						tsf_timer);
		status = QDF_STATUS_SUCCESS;
	} else {
		ocb_err("ocb_get_tsf_timer_cb is NULL");
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

/**
 * ocb_dcc_stats_response() - Process get DCC stats response
 * @evt: repsonse event
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS ocb_dcc_stats_response(struct ocb_rx_event *evt)
{
	QDF_STATUS status;
	struct ocb_rx_event *event;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct ocb_callbacks *cbs;
	struct ocb_dcc_get_stats_response *dcc_stats;

	if (!evt) {
		ocb_err("Event buffer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	event = evt;
	vdev = event->vdev;
	pdev = wlan_vdev_get_pdev(vdev);
	cbs = wlan_ocb_get_callbacks(pdev);
	dcc_stats = &event->rsp.dcc_stats;
	if (cbs && cbs->ocb_dcc_get_stats_callback) {
		ocb_debug("send DCC stats");
		cbs->ocb_dcc_get_stats_callback(cbs->ocb_dcc_get_stats_context,
						dcc_stats);
		status = QDF_STATUS_SUCCESS;
	} else {
		ocb_err("dcc_get_stats_cb is NULL");
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

/**
 * ocb_ndl_response() - Process NDL update response
 * @evt: repsonse event
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS ocb_ndl_response(struct ocb_rx_event *evt)
{
	QDF_STATUS status;
	struct ocb_rx_event *event;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct ocb_callbacks *cbs;
	struct ocb_dcc_update_ndl_response *ndl;

	if (!evt) {
		ocb_err("Event buffer is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	event = evt;
	vdev = event->vdev;
	pdev = wlan_vdev_get_pdev(vdev);
	cbs = wlan_ocb_get_callbacks(pdev);
	ndl = &event->rsp.ndl;
	if (cbs && cbs->ocb_dcc_update_ndl_callback) {
		ocb_debug("NDL update response");
		cbs->ocb_dcc_update_ndl_callback(
				cbs->ocb_dcc_update_ndl_context, ndl);
		status = QDF_STATUS_SUCCESS;
	} else {
		ocb_err("dcc_update_ndl is NULL");
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

/**
 * ocb_dcc_indication() - Process DCC stats indication
 * @evt: repsonse event
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS ocb_dcc_indication(struct ocb_rx_event *evt)
{
	QDF_STATUS status;
	struct ocb_rx_event *event;
	struct wlan_objmgr_vdev *vdev;
	struct ocb_callbacks *cbs;
	struct wlan_objmgr_pdev *pdev;
	struct ocb_dcc_get_stats_response *dcc_stats;

	if (!evt) {
		ocb_err("Event buffer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	event = evt;
	vdev = event->vdev;
	pdev = wlan_vdev_get_pdev(vdev);
	cbs = wlan_ocb_get_callbacks(pdev);
	dcc_stats = &event->rsp.dcc_stats;
	if (cbs && cbs->ocb_dcc_stats_event_callback) {
		ocb_debug("DCC stats indication");
		cbs->ocb_dcc_stats_event_callback(
				cbs->ocb_dcc_stats_event_context, dcc_stats);
		status = QDF_STATUS_SUCCESS;
	} else {
		ocb_err("dcc_get_stats_cb is NULL");
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

/**
 * ocb_flush_start_msg() - Flush ocb start message
 * @msg: OCB start vdev message
 *
 * Return: QDF_STATUS_SUCCESS on success.
 */
static QDF_STATUS ocb_flush_start_msg(struct scheduler_msg *msg)
{
	struct ocb_pdev_obj *ocb_obj;

	if (!msg) {
		ocb_err("Null point for OCB message");
		return QDF_STATUS_E_INVAL;
	}

	ocb_obj = msg->bodyptr;
	if (ocb_obj && ocb_obj->channel_config) {
		ocb_info("release the backed config parameters");
		qdf_mem_free(ocb_obj->channel_config);
		ocb_obj->channel_config = NULL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * ocb_process_start_vdev_msg() - Handler for OCB vdev start message
 * @msg: OCB start vdev message
 *
 * Return: QDF_STATUS_SUCCESS on success.
 */
static QDF_STATUS ocb_process_start_vdev_msg(struct scheduler_msg *msg)
{
	QDF_STATUS status;
	struct ocb_config *config;
	struct ocb_pdev_obj *ocb_obj;
	struct ocb_callbacks *ocb_cbs;
	struct wlan_objmgr_vdev *vdev;

	if (!msg || !msg->bodyptr) {
		ocb_err("invalid message");
		return QDF_STATUS_E_INVAL;
	}

	ocb_obj = msg->bodyptr;
	ocb_cbs = &ocb_obj->ocb_cbs;
	if (!ocb_cbs->start_ocb_vdev) {
		ocb_err("No callback to start ocb vdev");
		return QDF_STATUS_E_FAILURE;
	}

	config = ocb_obj->channel_config;
	if (!config) {
		ocb_err("NULL config parameters");
		return QDF_STATUS_E_INVAL;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(ocb_obj->pdev,
						    config->vdev_id,
						    WLAN_OCB_SB_ID);
	if (!vdev) {
		ocb_err("Cannot get vdev");
		return QDF_STATUS_E_FAILURE;
	}

	ocb_debug("Start to send OCB vdev start cmd");
	status = ocb_cbs->start_ocb_vdev(config);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_OCB_SB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		ocb_err("Failed to start OCB vdev");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ocb_vdev_start(struct ocb_pdev_obj *ocb_obj)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0};

	msg.bodyptr = ocb_obj;
	msg.callback = ocb_process_start_vdev_msg;
	msg.flush_callback = ocb_flush_start_msg;
	status = scheduler_post_message(QDF_MODULE_ID_OCB,
					QDF_MODULE_ID_OCB,
					QDF_MODULE_ID_TARGET_IF, &msg);

	return status;
}

QDF_STATUS ocb_process_evt(struct scheduler_msg *msg)
{
	QDF_STATUS status;
	struct ocb_rx_event *event;

	ocb_debug("msg type %d, %s", msg->type,
		  ocb_get_evt_type_str(msg->type));

	if (!(msg->bodyptr)) {
		ocb_err("Invalid message body");
		return QDF_STATUS_E_INVAL;
	}
	event = msg->bodyptr;
	switch (msg->type) {
	case OCB_CHANNEL_CONFIG_STATUS:
		status = ocb_channel_config_status(event);
		break;
	case OCB_TSF_TIMER:
		status = ocb_tsf_timer(event);
		break;
	case OCB_DCC_STATS_RESPONSE:
		status = ocb_dcc_stats_response(event);
		break;
	case OCB_NDL_RESPONSE:
		status = ocb_ndl_response(event);
		break;
	case OCB_DCC_INDICATION:
		status = ocb_dcc_indication(event);
		break;
	default:
		status = QDF_STATUS_E_INVAL;
		break;
	}

	wlan_ocb_release_rx_event(event);
	msg->bodyptr = NULL;

	return status;
}

struct ocb_config *ocb_copy_config(struct ocb_config *src)
{
	struct ocb_config *dst;
	uint32_t length;
	uint8_t *cursor;

	length = sizeof(*src) +
		src->channel_count * sizeof(*src->channels) +
		src->schedule_size * sizeof(*src->schedule) +
		src->dcc_ndl_chan_list_len +
		src->dcc_ndl_active_state_list_len;

	dst = qdf_mem_malloc(length);
	if (!dst)
		return NULL;

	*dst = *src;

	cursor = (uint8_t *)dst;
	cursor += sizeof(*dst);
	dst->channels = (struct ocb_config_chan *)cursor;
	cursor += src->channel_count * sizeof(*dst->channels);
	qdf_mem_copy(dst->channels, src->channels,
		     src->channel_count * sizeof(*dst->channels));
	dst->schedule = (struct ocb_config_schdl *)cursor;
	cursor += src->schedule_size * sizeof(*dst->schedule);
	qdf_mem_copy(dst->schedule, src->schedule,
		     src->schedule_size * sizeof(*dst->schedule));
	dst->dcc_ndl_chan_list = cursor;
	cursor += src->dcc_ndl_chan_list_len;
	qdf_mem_copy(dst->dcc_ndl_chan_list, src->dcc_ndl_chan_list,
		     src->dcc_ndl_chan_list_len);
	dst->dcc_ndl_active_state_list = cursor;
	cursor += src->dcc_ndl_active_state_list_len;
	qdf_mem_copy(dst->dcc_ndl_active_state_list,
		     src->dcc_ndl_active_state_list,
		     src->dcc_ndl_active_state_list_len);

	return dst;
}

QDF_STATUS ocb_pdev_obj_create_notification(struct wlan_objmgr_pdev *pdev,
					    void *arg_list)
{
	QDF_STATUS status;
	struct ocb_pdev_obj *ocb_obj;

	ocb_notice("ocb pdev created");
	ocb_obj = qdf_mem_malloc(sizeof(*ocb_obj));
	if (!ocb_obj)
		return QDF_STATUS_E_FAILURE;

	status = wlan_objmgr_pdev_component_obj_attach(pdev,
						       WLAN_UMAC_COMP_OCB,
						       (void *)ocb_obj,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		ocb_err("Failed to attach pdev ocb component");
		qdf_mem_free(ocb_obj);
		return status;
	}
	ocb_obj->pdev = pdev;

	/* register OCB tx/rx ops */
	tgt_ocb_register_rx_ops(&ocb_obj->ocb_rxops);
	target_if_ocb_register_tx_ops(&ocb_obj->ocb_txops);
	ocb_notice("ocb pdev attached");

	return status;
}

QDF_STATUS ocb_pdev_obj_destroy_notification(struct wlan_objmgr_pdev *pdev,
					     void *arg_list)
{
	QDF_STATUS status;
	struct ocb_pdev_obj *ocb_obj;

	ocb_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
							WLAN_UMAC_COMP_OCB);
	if (!ocb_obj) {
		ocb_err("Failed to get ocb pdev object");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_pdev_component_obj_detach(pdev,
						       WLAN_UMAC_COMP_OCB,
						       ocb_obj);
	if (QDF_IS_STATUS_ERROR(status))
		ocb_err("Failed to detatch ocb pdev object");

	qdf_mem_free(ocb_obj);

	return QDF_STATUS_SUCCESS;
}
