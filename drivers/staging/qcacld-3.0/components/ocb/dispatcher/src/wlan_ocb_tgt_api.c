/*
 * Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains ocb south bound interface definitions
 */

#include <scheduler_api.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include "wlan_ocb_public_structs.h"
#include "wlan_ocb_ucfg_api.h"
#include "wlan_ocb_tgt_api.h"
#include "wlan_ocb_main.h"

/**
 * wlan_ocb_flush_callback() - OCB message flash callback
 * @msg: OCB message
 *
 * Return: QDF_STATUS_SUCCESS on success.
 */
static QDF_STATUS wlan_ocb_flush_callback(struct scheduler_msg *msg)
{
	struct ocb_rx_event *event;

	if (!msg) {
		ocb_err("Null point for OCB message");
		return QDF_STATUS_E_INVAL;
	}

	event = msg->bodyptr;
	wlan_ocb_release_rx_event(event);

	return QDF_STATUS_SUCCESS;
}

/**
 * tgt_ocb_channel_config_status() - handler for channel config response
 * @psoc: psoc handle
 * @status: status for last channel config
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
tgt_ocb_channel_config_status(struct wlan_objmgr_psoc *psoc,
			      uint32_t status)
{
	QDF_STATUS qdf_status;
	struct scheduler_msg msg = {0};
	struct ocb_rx_event *event;

	event = qdf_mem_malloc(sizeof(*event));
	if (!event)
		return QDF_STATUS_E_NOMEM;

	qdf_status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_OCB_SB_ID);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		ocb_err("Failed to get psoc ref");
		wlan_ocb_release_rx_event(event);
		return qdf_status;
	}
	event->psoc = psoc;
	event->rsp.channel_cfg_rsp.status = status;
	event->evt_id = OCB_CHANNEL_CONFIG_STATUS;
	msg.type = OCB_CHANNEL_CONFIG_STATUS;
	msg.bodyptr = event;
	msg.callback = ocb_process_evt;
	msg.flush_callback = wlan_ocb_flush_callback;
	qdf_status = scheduler_post_message(QDF_MODULE_ID_OCB,
					    QDF_MODULE_ID_OCB,
					    QDF_MODULE_ID_TARGET_IF, &msg);

	if (QDF_IS_STATUS_SUCCESS(qdf_status))
		return QDF_STATUS_SUCCESS;

	ocb_err("failed to send OCB_CHANNEL_CONFIG_STATUS msg");
	wlan_ocb_release_rx_event(event);

	return qdf_status;
}

/**
 * tgt_ocb_get_tsf_timer() - handle for TSF timer response
 * @psoc: psoc handle
 * @response: TSF timer response
 *
 * Return: QDF_STATUS_SUCCESS on succcess
 */
static QDF_STATUS
tgt_ocb_get_tsf_timer(struct wlan_objmgr_psoc *psoc,
		      struct ocb_get_tsf_timer_response *response)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct ocb_rx_event *event;

	event = qdf_mem_malloc(sizeof(*event));
	if (!event)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_OCB_SB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		ocb_err("Failed to get psoc ref");
		goto flush_ref;
	}
	event->psoc = psoc;
	event->vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
							   response->vdev_id,
							   WLAN_OCB_SB_ID);
	if (!event->vdev) {
		ocb_err("Cannot get vdev handle");
		status = QDF_STATUS_E_FAILURE;
		goto flush_ref;
	}

	event->evt_id = OCB_TSF_TIMER;
	event->rsp.tsf_timer.vdev_id = response->vdev_id;
	event->rsp.tsf_timer.timer_high = response->timer_high;
	event->rsp.tsf_timer.timer_low = response->timer_low;
	msg.type = OCB_TSF_TIMER;
	msg.bodyptr = event;
	msg.callback = ocb_process_evt;
	msg.flush_callback = wlan_ocb_flush_callback;

	status = scheduler_post_message(QDF_MODULE_ID_OCB,
					QDF_MODULE_ID_OCB,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	ocb_err("failed to send OCB_TSF_TIMER msg");
flush_ref:
	wlan_ocb_release_rx_event(event);

	return status;
}

/**
 * tgt_ocb_dcc_ndl_update() - handler for NDL update response
 * @psoc: psoc handle
 * @resp: NDL update response
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
tgt_ocb_dcc_ndl_update(struct wlan_objmgr_psoc *psoc,
		       struct ocb_dcc_update_ndl_response *resp)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct ocb_rx_event *event;

	event = qdf_mem_malloc(sizeof(*event));
	if (!event)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_OCB_SB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		ocb_err("Failed to get psoc ref");
		goto flush_ref;
	}
	event->psoc = psoc;
	event->vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
							   resp->vdev_id,
							   WLAN_OCB_SB_ID);
	if (!event->vdev) {
		ocb_err("Cannot get vdev handle");
		status = QDF_STATUS_E_FAILURE;
		goto flush_ref;
	}

	event->evt_id = OCB_NDL_RESPONSE;
	qdf_mem_copy(&event->rsp.ndl, resp, sizeof(*resp));
	msg.type = OCB_NDL_RESPONSE;
	msg.bodyptr = event;
	msg.callback = ocb_process_evt;
	msg.flush_callback = wlan_ocb_flush_callback;

	status = scheduler_post_message(QDF_MODULE_ID_OCB,
					QDF_MODULE_ID_OCB,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	ocb_err("failed to send OCB_NDL_RESPONSE msg");
flush_ref:
	wlan_ocb_release_rx_event(event);

	return status;
}

/**
 * tgt_ocb_dcc_stats_indicate() - handler for DCC stats indication
 * @psoc: psoc handle
 * @response: DCC stats
 * @bool: true for active query, false for passive indicate
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
tgt_ocb_dcc_stats_indicate(struct wlan_objmgr_psoc *psoc,
			   struct ocb_dcc_get_stats_response *response,
			   bool active)
{
	QDF_STATUS status;
	uint8_t *buf;
	uint32_t size;
	struct scheduler_msg msg = {0};
	struct ocb_rx_event *event;

	size = sizeof(*event) +
		response->channel_stats_array_len;
	buf = qdf_mem_malloc(size);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	event = (struct ocb_rx_event *)buf;
	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_OCB_SB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		ocb_err("Failed to get psoc ref");
		goto flush_ref;
	}
	event->psoc = psoc;
	event->vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
							   response->vdev_id,
							   WLAN_OCB_SB_ID);
	if (!event->vdev) {
		ocb_err("Cannot get vdev handle");
		status = QDF_STATUS_E_FAILURE;
		goto flush_ref;
	}

	event->rsp.dcc_stats.channel_stats_array =
		(uint8_t *)&event->rsp.dcc_stats +
		sizeof(struct ocb_dcc_get_stats_response);
	event->rsp.dcc_stats.vdev_id = response->vdev_id;
	event->rsp.dcc_stats.num_channels = response->num_channels;
	event->rsp.dcc_stats.channel_stats_array_len =
		response->channel_stats_array_len;
	qdf_mem_copy(event->rsp.dcc_stats.channel_stats_array,
		     response->channel_stats_array,
		     response->channel_stats_array_len);
	ocb_debug("Message type is %s",
		  active ? "Get stats response" : "DCC stats indication");
	if (active)
		msg.type = OCB_DCC_STATS_RESPONSE;
	else
		msg.type = OCB_DCC_INDICATION;
	event->evt_id = msg.type;
	msg.bodyptr = event;
	msg.callback = ocb_process_evt;
	msg.flush_callback = wlan_ocb_flush_callback;

	status = scheduler_post_message(QDF_MODULE_ID_OCB,
					QDF_MODULE_ID_OCB,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_SUCCESS;

	ocb_err("failed to send DCC stats msg(%d)", msg.type);
flush_ref:
	wlan_ocb_release_rx_event(event);

	return status;
}

QDF_STATUS tgt_ocb_register_ev_handler(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_ocb_tx_ops *ocb_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		ocb_err("Null soc handle");
		return QDF_STATUS_E_INVAL;
	}
	ocb_ops = wlan_pdev_get_ocb_tx_ops(pdev);
	if (ocb_ops && ocb_ops->ocb_reg_ev_handler) {
		status = ocb_ops->ocb_reg_ev_handler(psoc, NULL);
		ocb_debug("register ocb event, status:%d", status);
	} else {
		ocb_alert("No ocb objects or ocb_reg_ev_handler");
	}

	return status;
}

QDF_STATUS tgt_ocb_unregister_ev_handler(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_ocb_tx_ops *ocb_ops;
	QDF_STATUS status;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		ocb_err("Null soc handle");
		return QDF_STATUS_E_INVAL;
	}
	ocb_ops = wlan_pdev_get_ocb_tx_ops(pdev);
	if (ocb_ops && ocb_ops->ocb_unreg_ev_handler) {
		status = ocb_ops->ocb_unreg_ev_handler(psoc, NULL);
		ocb_debug("unregister ocb event, status:%d", status);
	} else {
		ocb_alert("No ocb objects or ocb_unreg_ev_handler");
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

QDF_STATUS tgt_ocb_register_rx_ops(struct wlan_ocb_rx_ops *ocb_rxops)
{
	ocb_rxops->ocb_set_config_status = tgt_ocb_channel_config_status;
	ocb_rxops->ocb_tsf_timer = tgt_ocb_get_tsf_timer;
	ocb_rxops->ocb_dcc_ndl_update = tgt_ocb_dcc_ndl_update;
	ocb_rxops->ocb_dcc_stats_indicate = tgt_ocb_dcc_stats_indicate;

	return QDF_STATUS_SUCCESS;
}
