/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains RoC API definitions
 */

#include <wlan_mgmt_txrx_utils_api.h>
#include <wlan_scan_public_structs.h>
#include <wlan_scan_ucfg_api.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_policy_mgr_api.h>
#include <wlan_utility.h>
#include "wlan_p2p_public_struct.h"
#include "wlan_p2p_tgt_api.h"
#include "wlan_p2p_ucfg_api.h"
#include "wlan_p2p_roc.h"
#include "wlan_p2p_main.h"
#include "wlan_p2p_off_chan_tx.h"

/**
 * p2p_mgmt_rx_ops() - register or unregister rx callback
 * @psoc: psoc object
 * @isregister: register if true, unregister if false
 *
 * This function registers or unregisters rx callback to mgmt txrx
 * component.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_mgmt_rx_ops(struct wlan_objmgr_psoc *psoc,
	bool isregister)
{
	struct mgmt_txrx_mgmt_frame_cb_info frm_cb_info;
	QDF_STATUS status;

	p2p_debug("psoc:%pK, is register rx:%d", psoc, isregister);

	frm_cb_info.frm_type = MGMT_PROBE_REQ;
	frm_cb_info.mgmt_rx_cb = tgt_p2p_mgmt_frame_rx_cb;

	if (isregister)
		status = wlan_mgmt_txrx_register_rx_cb(psoc,
				WLAN_UMAC_COMP_P2P, &frm_cb_info, 1);
	else
		status = wlan_mgmt_txrx_deregister_rx_cb(psoc,
				WLAN_UMAC_COMP_P2P, &frm_cb_info, 1);

	return status;
}

/**
 * p2p_scan_start() - Start scan
 * @roc_ctx: remain on channel request
 *
 * This function trigger a start scan request to scan component.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_scan_start(struct p2p_roc_context *roc_ctx)
{
	QDF_STATUS status;
	struct scan_start_request *req;
	struct wlan_objmgr_vdev *vdev;
	struct p2p_soc_priv_obj *p2p_soc_obj = roc_ctx->p2p_soc_obj;
	uint32_t go_num;
	uint8_t ndp_num = 0, nan_disc_enabled_num = 0;
	bool is_dbs;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
			p2p_soc_obj->soc, roc_ctx->vdev_id,
			WLAN_P2P_ID);
	if (!vdev) {
		p2p_err("vdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req) {
		status = QDF_STATUS_E_NOMEM;
		goto fail;
	}

	ucfg_scan_init_default_params(vdev, req);

	if (!roc_ctx->duration) {
		roc_ctx->duration = P2P_ROC_DEFAULT_DURATION;
		p2p_debug("use default duration %d",
			  P2P_ROC_DEFAULT_DURATION);
	}

	roc_ctx->scan_id = ucfg_scan_get_scan_id(p2p_soc_obj->soc);
	req->vdev = vdev;
	req->scan_req.scan_id = roc_ctx->scan_id;
	req->scan_req.scan_type = SCAN_TYPE_P2P_LISTEN;
	req->scan_req.scan_req_id = p2p_soc_obj->scan_req_id;
	req->scan_req.chan_list.num_chan = 1;
	req->scan_req.chan_list.chan[0].freq = wlan_chan_to_freq(roc_ctx->chan);
	req->scan_req.dwell_time_passive = roc_ctx->duration;
	req->scan_req.dwell_time_active = 0;
	req->scan_req.scan_priority = SCAN_PRIORITY_HIGH;
	req->scan_req.num_bssid = 1;
	qdf_set_macaddr_broadcast(&req->scan_req.bssid_list[0]);

	if (req->scan_req.dwell_time_passive < P2P_MAX_ROC_DURATION) {
		go_num = policy_mgr_mode_specific_connection_count(
				p2p_soc_obj->soc, PM_P2P_GO_MODE, NULL);
		policy_mgr_mode_specific_num_active_sessions(p2p_soc_obj->soc,
							     QDF_NDI_MODE,
							     &ndp_num);
		policy_mgr_mode_specific_num_active_sessions(p2p_soc_obj->soc,
							     QDF_NAN_DISC_MODE,
							     &nan_disc_enabled_num);

		p2p_debug("present go number:%d, NDP number:%d, NAN number:%d",
			  go_num, ndp_num, nan_disc_enabled_num);

		is_dbs = policy_mgr_is_hw_dbs_capable(p2p_soc_obj->soc);

		if (go_num)
			req->scan_req.dwell_time_passive *=
					P2P_ROC_DURATION_MULTI_GO_PRESENT;
		else
			req->scan_req.dwell_time_passive *=
					P2P_ROC_DURATION_MULTI_GO_ABSENT;
		/* this is to protect too huge value if some customers
		 * give a higher value from supplicant
		 */
		if (ndp_num) {
			if (is_dbs && req->scan_req.dwell_time_passive >
			    P2P_MAX_ROC_DURATION_DBS_NDP_PRESENT)
				req->scan_req.dwell_time_passive =
					P2P_MAX_ROC_DURATION_DBS_NDP_PRESENT;
			else if (!is_dbs && req->scan_req.dwell_time_passive >
				 P2P_MAX_ROC_DURATION_NON_DBS_NDP_PRESENT)
				req->scan_req.dwell_time_passive =
					P2P_MAX_ROC_DURATION_NON_DBS_NDP_PRESENT;
		} else if (nan_disc_enabled_num) {
			if (is_dbs && req->scan_req.dwell_time_passive >
			    P2P_MAX_ROC_DURATION_DBS_NAN_PRESENT)
				req->scan_req.dwell_time_passive =
					P2P_MAX_ROC_DURATION_DBS_NAN_PRESENT;
			else if (!is_dbs && req->scan_req.dwell_time_passive >
				 P2P_MAX_ROC_DURATION_NON_DBS_NAN_PRESENT)
				req->scan_req.dwell_time_passive =
					P2P_MAX_ROC_DURATION_NON_DBS_NAN_PRESENT;
		} else if (req->scan_req.dwell_time_passive >
			   P2P_MAX_ROC_DURATION) {
			req->scan_req.dwell_time_passive = P2P_MAX_ROC_DURATION;
		}
	}
	p2p_debug("FW requested roc duration is:%d",
		  req->scan_req.dwell_time_passive);

	status = ucfg_scan_start(req);

	p2p_debug("start scan, scan req id:%d, scan id:%d, status:%d",
		p2p_soc_obj->scan_req_id, roc_ctx->scan_id, status);
fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return status;
}

/**
 * p2p_scan_abort() - Abort scan
 * @roc_ctx: remain on channel request
 *
 * This function trigger an abort scan request to scan component.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_scan_abort(struct p2p_roc_context *roc_ctx)
{
	QDF_STATUS status;
	struct scan_cancel_request *req;
	struct wlan_objmgr_vdev *vdev;
	struct p2p_soc_priv_obj *p2p_soc_obj = roc_ctx->p2p_soc_obj;

	p2p_debug("abort scan, scan req id:%d, scan id:%d",
		p2p_soc_obj->scan_req_id, roc_ctx->scan_id);

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
			p2p_soc_obj->soc, roc_ctx->vdev_id,
			WLAN_P2P_ID);
	if (!vdev) {
		p2p_err("vdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req) {
		status = QDF_STATUS_E_NOMEM;
		goto fail;
	}

	req->vdev = vdev;
	req->cancel_req.requester = p2p_soc_obj->scan_req_id;
	req->cancel_req.scan_id = roc_ctx->scan_id;
	req->cancel_req.vdev_id = roc_ctx->vdev_id;
	req->cancel_req.req_type = WLAN_SCAN_CANCEL_SINGLE;

	qdf_mtrace(QDF_MODULE_ID_P2P, QDF_MODULE_ID_SCAN,
		   req->cancel_req.req_type,
		   req->vdev->vdev_objmgr.vdev_id, req->cancel_req.scan_id);
	status = ucfg_scan_cancel(req);

	p2p_debug("abort scan, scan req id:%d, scan id:%d, status:%d",
		p2p_soc_obj->scan_req_id, roc_ctx->scan_id, status);
fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return status;
}

/**
 * p2p_send_roc_event() - Send roc event
 * @roc_ctx: remain on channel request
 * @evt: roc event information
 *
 * This function send out roc event to up layer.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_send_roc_event(
	struct p2p_roc_context *roc_ctx, enum p2p_roc_event evt)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_event p2p_evt;
	struct p2p_start_param *start_param;

	p2p_soc_obj = roc_ctx->p2p_soc_obj;
	if (!p2p_soc_obj || !(p2p_soc_obj->start_param)) {
		p2p_err("Invalid p2p soc object or start parameters");
		return QDF_STATUS_E_INVAL;
	}
	start_param = p2p_soc_obj->start_param;
	if (!(start_param->event_cb)) {
		p2p_err("Invalid p2p event callback to up layer");
		return QDF_STATUS_E_INVAL;
	}

	p2p_evt.vdev_id = roc_ctx->vdev_id;
	p2p_evt.roc_event = evt;
	p2p_evt.cookie = (uint64_t)roc_ctx->id;
	p2p_evt.chan = roc_ctx->chan;
	p2p_evt.duration = roc_ctx->duration;

	p2p_debug("roc_event: %d, cookie:%llx", p2p_evt.roc_event,
		  p2p_evt.cookie);

	start_param->event_cb(start_param->event_cb_data, &p2p_evt);

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_destroy_roc_ctx() - destroy roc ctx
 * @roc_ctx:            remain on channel request
 * @up_layer_event:     if send uplayer event
 * @in_roc_queue:       if roc context in roc queue
 *
 * This function destroy roc context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_destroy_roc_ctx(struct p2p_roc_context *roc_ctx,
	bool up_layer_event, bool in_roc_queue)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct p2p_soc_priv_obj *p2p_soc_obj = roc_ctx->p2p_soc_obj;

	p2p_debug("p2p_soc_obj:%pK, roc_ctx:%pK, up_layer_event:%d, in_roc_queue:%d vdev_id:%d chan:%d duration:%d",
		p2p_soc_obj, roc_ctx, up_layer_event, in_roc_queue,
		roc_ctx->vdev_id, roc_ctx->chan, roc_ctx->duration);

	if (up_layer_event) {
		if (roc_ctx->roc_state < ROC_STATE_ON_CHAN)
			p2p_send_roc_event(roc_ctx, ROC_EVENT_READY_ON_CHAN);
		p2p_send_roc_event(roc_ctx, ROC_EVENT_COMPLETED);
	}

	if (in_roc_queue) {
		status = qdf_list_remove_node(&p2p_soc_obj->roc_q,
				(qdf_list_node_t *)roc_ctx);
		if (QDF_STATUS_SUCCESS != status)
			p2p_err("Failed to remove roc req, status %d", status);
	}

	qdf_idr_remove(&p2p_soc_obj->p2p_idr, roc_ctx->id);
	qdf_mem_free(roc_ctx);

	return status;
}

/**
 * p2p_execute_cancel_roc_req() - Execute cancel roc request
 * @roc_ctx: remain on channel request
 *
 * This function stop roc timer, abort scan and unregister mgmt rx
 * callbak.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_execute_cancel_roc_req(
	struct p2p_roc_context *roc_ctx)
{
	QDF_STATUS status;
	struct p2p_soc_priv_obj *p2p_soc_obj = roc_ctx->p2p_soc_obj;

	p2p_debug("p2p execute cancel roc req");

	roc_ctx->roc_state = ROC_STATE_CANCEL_IN_PROG;

	status = qdf_mc_timer_stop_sync(&roc_ctx->roc_timer);
	if (status != QDF_STATUS_SUCCESS)
		p2p_err("Failed to stop roc timer, roc %pK", roc_ctx);

	status = p2p_scan_abort(roc_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to abort scan, status:%d, destroy roc %pK",
			status, roc_ctx);
		qdf_mc_timer_destroy(&roc_ctx->roc_timer);
		p2p_mgmt_rx_ops(p2p_soc_obj->soc, false);
		p2p_destroy_roc_ctx(roc_ctx, true, true);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_roc_timeout() - Callback for roc timeout
 * @pdata: pointer to p2p soc private object
 *
 * This function is callback for roc time out.
 *
 * Return: None
 */
static void p2p_roc_timeout(void *pdata)
{
	struct p2p_roc_context *roc_ctx;
	struct p2p_soc_priv_obj *p2p_soc_obj;

	p2p_debug("p2p soc obj:%pK", pdata);

	p2p_soc_obj = pdata;
	if (!p2p_soc_obj) {
		p2p_err("Invalid p2p soc object");
		return;
	}

	roc_ctx = p2p_find_current_roc_ctx(p2p_soc_obj);
	if (!roc_ctx) {
		p2p_debug("No P2P roc is pending");
		return;
	}

	p2p_debug("p2p soc obj:%pK, roc ctx:%pK, vdev_id:%d, scan_id:%d, tx ctx:%pK, chan:%d, phy_mode:%d, duration:%d, roc_type:%d, roc_state:%d",
		roc_ctx->p2p_soc_obj, roc_ctx, roc_ctx->vdev_id,
		roc_ctx->scan_id, roc_ctx->tx_ctx, roc_ctx->chan,
		roc_ctx->phy_mode, roc_ctx->duration,
		roc_ctx->roc_type, roc_ctx->roc_state);

	if (roc_ctx->roc_state == ROC_STATE_CANCEL_IN_PROG) {
		p2p_err("Cancellation already in progress");
		return;
	}
	p2p_execute_cancel_roc_req(roc_ctx);
}

/**
 * p2p_execute_roc_req() - Execute roc request
 * @roc_ctx: remain on channel request
 *
 * This function init roc timer, start scan and register mgmt rx
 * callbak.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_execute_roc_req(struct p2p_roc_context *roc_ctx)
{
	QDF_STATUS status;
	struct p2p_soc_priv_obj *p2p_soc_obj = roc_ctx->p2p_soc_obj;

	p2p_debug("p2p soc obj:%pK, roc ctx:%pK, vdev_id:%d, scan_id:%d, tx ctx:%pK, chan:%d, phy_mode:%d, duration:%d, roc_type:%d, roc_state:%d",
		p2p_soc_obj, roc_ctx, roc_ctx->vdev_id,
		roc_ctx->scan_id, roc_ctx->tx_ctx, roc_ctx->chan,
		roc_ctx->phy_mode, roc_ctx->duration,
		roc_ctx->roc_type, roc_ctx->roc_state);

	/* prevent runtime suspend */
	qdf_runtime_pm_prevent_suspend(&p2p_soc_obj->roc_runtime_lock);

	status = qdf_mc_timer_init(&roc_ctx->roc_timer,
			QDF_TIMER_TYPE_SW, p2p_roc_timeout,
			p2p_soc_obj);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("failed to init roc timer, status:%d", status);
		goto fail;
	}

	roc_ctx->roc_state = ROC_STATE_REQUESTED;
	status = p2p_scan_start(roc_ctx);
	if (status != QDF_STATUS_SUCCESS) {
		qdf_mc_timer_destroy(&roc_ctx->roc_timer);
		p2p_err("Failed to start scan, status:%d", status);
		goto fail;
	}

fail:
	if (status != QDF_STATUS_SUCCESS) {
		p2p_destroy_roc_ctx(roc_ctx, true, true);
		qdf_runtime_pm_allow_suspend(
			&p2p_soc_obj->roc_runtime_lock);
		return status;
	}

	p2p_soc_obj->cur_roc_vdev_id = roc_ctx->vdev_id;
	status = p2p_mgmt_rx_ops(p2p_soc_obj->soc, true);
	if (status != QDF_STATUS_SUCCESS)
		p2p_err("Failed to register mgmt rx callback, status:%d",
			status);

	return status;
}

/**
 * p2p_find_roc_ctx() - Find out roc context by cookie
 * @p2p_soc_obj: p2p psoc private object
 * @cookie: cookie is the key to find out roc context
 *
 * This function find out roc context by cookie from p2p psoc private
 * object
 *
 * Return: Pointer to roc context - success
 *         NULL                   - failure
 */
static struct p2p_roc_context *p2p_find_roc_ctx(
	struct p2p_soc_priv_obj *p2p_soc_obj, uint64_t cookie)
{
	struct p2p_roc_context *curr_roc_ctx;
	qdf_list_node_t *p_node;
	QDF_STATUS status;

	p2p_debug("p2p soc obj:%pK, cookie:%llx", p2p_soc_obj, cookie);

	status = qdf_list_peek_front(&p2p_soc_obj->roc_q, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		curr_roc_ctx = qdf_container_of(p_node,
					struct p2p_roc_context, node);
		if ((uintptr_t) curr_roc_ctx == cookie)
			return curr_roc_ctx;
		status = qdf_list_peek_next(&p2p_soc_obj->roc_q,
						p_node, &p_node);
	}

	return NULL;
}

/**
 * p2p_process_scan_start_evt() - Process scan start event
 * @roc_ctx: remain on channel request
 *
 * This function process scan start event.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_process_scan_start_evt(
	struct p2p_roc_context *roc_ctx)
{
	roc_ctx->roc_state = ROC_STATE_STARTED;
	p2p_debug("scan started, scan id:%d", roc_ctx->scan_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_process_ready_on_channel_evt() - Process ready on channel event
 * @roc_ctx: remain on channel request
 *
 * This function process ready on channel event. Starts roc timer.
 * Indicates this event to up layer if this is user request roc. Sends
 * mgmt frame if this is off channel rx roc.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_process_ready_on_channel_evt(
	struct p2p_roc_context *roc_ctx)
{
	uint64_t cookie;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	QDF_STATUS status;

	p2p_soc_obj = roc_ctx->p2p_soc_obj;
	roc_ctx->roc_state = ROC_STATE_ON_CHAN;

	p2p_debug("scan_id:%d, roc_state:%d", roc_ctx->scan_id,
		  roc_ctx->roc_state);

	status = qdf_mc_timer_start(&roc_ctx->roc_timer,
		(roc_ctx->duration + P2P_EVENT_PROPAGATE_TIME));
	if (status != QDF_STATUS_SUCCESS)
		p2p_err("Remain on Channel timer start failed");
	if (roc_ctx->roc_type == USER_REQUESTED) {
		p2p_debug("user required roc, send roc event");
		status = p2p_send_roc_event(roc_ctx,
				ROC_EVENT_READY_ON_CHAN);
	}

	cookie = (uintptr_t)roc_ctx;
		/* ready to tx frame */
	p2p_ready_to_tx_frame(p2p_soc_obj, cookie);

	return status;
}

/**
 * p2p_process_scan_complete_evt() - Process scan complete event
 * @roc_ctx: remain on channel request
 *
 * This function process scan complete event.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_process_scan_complete_evt(
	struct p2p_roc_context *roc_ctx)
{
	QDF_STATUS status;
	qdf_list_node_t *next_node;
	uint32_t size;
	struct p2p_soc_priv_obj *p2p_soc_obj = roc_ctx->p2p_soc_obj;

	p2p_debug("vdev_id:%d scan_id:%d", roc_ctx->vdev_id, roc_ctx->scan_id);

	/* allow runtime suspend */
	qdf_runtime_pm_allow_suspend(&p2p_soc_obj->roc_runtime_lock);


	status = qdf_mc_timer_stop_sync(&roc_ctx->roc_timer);
	if (QDF_IS_STATUS_ERROR(status))
		p2p_err("Failed to stop roc timer");

	status = qdf_mc_timer_destroy(&roc_ctx->roc_timer);
	if (status != QDF_STATUS_SUCCESS)
		p2p_err("Failed to destroy roc timer");

	status = p2p_mgmt_rx_ops(p2p_soc_obj->soc, false);
	p2p_soc_obj->cur_roc_vdev_id = P2P_INVALID_VDEV_ID;
	if (status != QDF_STATUS_SUCCESS)
		p2p_err("Failed to deregister mgmt rx callback");

	if (roc_ctx->roc_type == USER_REQUESTED)
		status = p2p_send_roc_event(roc_ctx,
				ROC_EVENT_COMPLETED);

	p2p_destroy_roc_ctx(roc_ctx, false, true);
	qdf_event_set(&p2p_soc_obj->cleanup_roc_done);

	size = qdf_list_size(&p2p_soc_obj->roc_q);

	if (size > 0) {
		status = qdf_list_peek_front(&p2p_soc_obj->roc_q,
				&next_node);
		if (QDF_STATUS_SUCCESS != status) {
			p2p_err("Failed to peek roc req from front, status %d",
				status);
			return status;
		}
		roc_ctx = qdf_container_of(next_node,
				struct p2p_roc_context, node);
		status = p2p_execute_roc_req(roc_ctx);
	}
	return status;
}

QDF_STATUS p2p_mgmt_rx_action_ops(struct wlan_objmgr_psoc *psoc,
	bool isregister)
{
	struct mgmt_txrx_mgmt_frame_cb_info frm_cb_info[2];
	QDF_STATUS status;

	p2p_debug("psoc:%pK, is register rx:%d", psoc, isregister);

	frm_cb_info[0].frm_type = MGMT_ACTION_VENDOR_SPECIFIC;
	frm_cb_info[0].mgmt_rx_cb = tgt_p2p_mgmt_frame_rx_cb;
	frm_cb_info[1].frm_type = MGMT_ACTION_CATEGORY_VENDOR_SPECIFIC;
	frm_cb_info[1].mgmt_rx_cb = tgt_p2p_mgmt_frame_rx_cb;

	if (isregister)
		status = wlan_mgmt_txrx_register_rx_cb(psoc,
				WLAN_UMAC_COMP_P2P, frm_cb_info, 2);
	else
		status = wlan_mgmt_txrx_deregister_rx_cb(psoc,
				WLAN_UMAC_COMP_P2P, frm_cb_info, 2);

	return status;
}

struct p2p_roc_context *p2p_find_current_roc_ctx(
	struct p2p_soc_priv_obj *p2p_soc_obj)
{
	struct p2p_roc_context *roc_ctx;
	qdf_list_node_t *p_node;
	QDF_STATUS status;

	status = qdf_list_peek_front(&p2p_soc_obj->roc_q, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		roc_ctx = qdf_container_of(p_node,
				struct p2p_roc_context, node);
		if (roc_ctx->roc_state != ROC_STATE_IDLE) {
			p2p_debug("p2p soc obj:%pK, roc ctx:%pK, vdev_id"
				":%d, scan_id:%d, tx ctx:%pK, chan:"
				"%d, phy_mode:%d, duration:%d, "
				"roc_type:%d, roc_state:%d",
				roc_ctx->p2p_soc_obj, roc_ctx,
				roc_ctx->vdev_id, roc_ctx->scan_id,
				roc_ctx->tx_ctx, roc_ctx->chan,
				roc_ctx->phy_mode, roc_ctx->duration,
				roc_ctx->roc_type, roc_ctx->roc_state);

			return roc_ctx;
		}
		status = qdf_list_peek_next(&p2p_soc_obj->roc_q,
						p_node, &p_node);
	}

	return NULL;
}

struct p2p_roc_context *p2p_find_roc_by_tx_ctx(
	struct p2p_soc_priv_obj *p2p_soc_obj, uint64_t cookie)
{
	struct p2p_roc_context *curr_roc_ctx;
	qdf_list_node_t *p_node;
	QDF_STATUS status;

	p2p_debug("p2p soc obj:%pK, cookie:%llx", p2p_soc_obj, cookie);

	status = qdf_list_peek_front(&p2p_soc_obj->roc_q, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		curr_roc_ctx = qdf_container_of(p_node,
					struct p2p_roc_context, node);
		if ((uintptr_t) curr_roc_ctx->tx_ctx == cookie)
			return curr_roc_ctx;
		status = qdf_list_peek_next(&p2p_soc_obj->roc_q,
						p_node, &p_node);
	}

	return NULL;
}

struct p2p_roc_context *p2p_find_roc_by_chan(
	struct p2p_soc_priv_obj *p2p_soc_obj, uint8_t chan)
{
	struct p2p_roc_context *roc_ctx;
	qdf_list_node_t *p_node;
	QDF_STATUS status;

	status = qdf_list_peek_front(&p2p_soc_obj->roc_q, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		roc_ctx = qdf_container_of(p_node,
					   struct p2p_roc_context,
					   node);
		if (roc_ctx->chan == chan) {
			p2p_debug("p2p soc obj:%pK, roc ctx:%pK, vdev_id:%d, scan_id:%d, tx ctx:%pK, chan:%d, phy_mode:%d, duration:%d, roc_type:%d, roc_state:%d",
				  roc_ctx->p2p_soc_obj, roc_ctx,
				  roc_ctx->vdev_id, roc_ctx->scan_id,
				  roc_ctx->tx_ctx, roc_ctx->chan,
				  roc_ctx->phy_mode, roc_ctx->duration,
				  roc_ctx->roc_type, roc_ctx->roc_state);

			return roc_ctx;
		}
		status = qdf_list_peek_next(&p2p_soc_obj->roc_q,
					    p_node, &p_node);
	}

	return NULL;
}

QDF_STATUS p2p_restart_roc_timer(struct p2p_roc_context *roc_ctx)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (QDF_TIMER_STATE_RUNNING ==
		qdf_mc_timer_get_current_state(&roc_ctx->roc_timer)) {
		p2p_debug("roc restart duration:%d", roc_ctx->duration);
		status = qdf_mc_timer_stop_sync(&roc_ctx->roc_timer);
		if (status != QDF_STATUS_SUCCESS) {
			p2p_err("Failed to stop roc timer");
			return status;
		}

		status = qdf_mc_timer_start(&roc_ctx->roc_timer,
						roc_ctx->duration);
		if (status != QDF_STATUS_SUCCESS)
			p2p_err("Remain on Channel timer start failed");
	}

	return status;
}

QDF_STATUS p2p_cleanup_roc_sync(
	struct p2p_soc_priv_obj *p2p_soc_obj,
	struct wlan_objmgr_vdev *vdev)
{
	struct scheduler_msg msg = {0};
	struct p2p_cleanup_param *param;
	QDF_STATUS status;
	uint32_t vdev_id;

	if (!p2p_soc_obj) {
		p2p_err("p2p soc context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	p2p_debug("p2p_soc_obj:%pK, vdev:%pK", p2p_soc_obj, vdev);
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return QDF_STATUS_E_NOMEM;

	param->p2p_soc_obj = p2p_soc_obj;
	if (vdev)
		vdev_id = (uint32_t)wlan_vdev_get_id(vdev);
	else
		vdev_id = P2P_INVALID_VDEV_ID;
	param->vdev_id = vdev_id;
	qdf_event_reset(&p2p_soc_obj->cleanup_roc_done);
	msg.type = P2P_CLEANUP_ROC;
	msg.bodyptr = param;
	msg.callback = p2p_process_cmd;
	status = scheduler_post_message(QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_OS_IF, &msg);
	if (status != QDF_STATUS_SUCCESS) {
		qdf_mem_free(param);
		return status;
	}

	status = qdf_wait_single_event(
			&p2p_soc_obj->cleanup_roc_done,
			P2P_WAIT_CLEANUP_ROC);

	if (status != QDF_STATUS_SUCCESS)
		p2p_err("wait for cleanup roc timeout, %d", status);

	return status;
}

QDF_STATUS p2p_process_cleanup_roc_queue(
	struct p2p_cleanup_param *param)
{
	uint32_t vdev_id;
	uint8_t count = 0;
	QDF_STATUS status, ret;
	struct p2p_roc_context *roc_ctx;
	qdf_list_node_t *p_node;
	struct p2p_soc_priv_obj *p2p_soc_obj;

	if (!param || !(param->p2p_soc_obj)) {
		p2p_err("Invalid cleanup param");
		return QDF_STATUS_E_FAILURE;
	}

	p2p_soc_obj = param->p2p_soc_obj;
	vdev_id = param->vdev_id;

	p2p_debug("clean up idle roc request, roc queue size:%d, vdev id:%d",
		  qdf_list_size(&p2p_soc_obj->roc_q), vdev_id);
	status = qdf_list_peek_front(&p2p_soc_obj->roc_q, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		roc_ctx = qdf_container_of(p_node,
				struct p2p_roc_context, node);

		p2p_debug("p2p soc obj:%pK, roc ctx:%pK, vdev_id:%d, scan_id:%d, tx ctx:%pK, chan:%d, phy_mode:%d, duration:%d, roc_type:%d, roc_state:%d",
			  roc_ctx->p2p_soc_obj, roc_ctx,
			  roc_ctx->vdev_id, roc_ctx->scan_id,
			  roc_ctx->tx_ctx, roc_ctx->chan,
			  roc_ctx->phy_mode, roc_ctx->duration,
			  roc_ctx->roc_type, roc_ctx->roc_state);
		status = qdf_list_peek_next(&p2p_soc_obj->roc_q,
						p_node, &p_node);
		if ((roc_ctx->roc_state == ROC_STATE_IDLE) &&
		    ((vdev_id == P2P_INVALID_VDEV_ID) ||
		     (vdev_id == roc_ctx->vdev_id))) {
			ret = qdf_list_remove_node(
					&p2p_soc_obj->roc_q,
					(qdf_list_node_t *)roc_ctx);
			if (ret == QDF_STATUS_SUCCESS)
				qdf_mem_free(roc_ctx);
			else
				p2p_err("Failed to remove roc ctx from queue");
		}
	}

	p2p_debug("clean up started roc request, roc queue size:%d",
		  qdf_list_size(&p2p_soc_obj->roc_q));
	status = qdf_list_peek_front(&p2p_soc_obj->roc_q, &p_node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		roc_ctx = qdf_container_of(p_node,
				struct p2p_roc_context, node);

		p2p_debug("p2p soc obj:%pK, roc ctx:%pK, vdev_id:%d, scan_id:%d, tx ctx:%pK, chan:%d, phy_mode:%d, duration:%d, roc_type:%d, roc_state:%d",
			  roc_ctx->p2p_soc_obj, roc_ctx, roc_ctx->vdev_id,
			  roc_ctx->scan_id, roc_ctx->tx_ctx, roc_ctx->chan,
			  roc_ctx->phy_mode, roc_ctx->duration,
			  roc_ctx->roc_type, roc_ctx->roc_state);

		status = qdf_list_peek_next(&p2p_soc_obj->roc_q,
						p_node, &p_node);
		if ((roc_ctx->roc_state != ROC_STATE_IDLE) &&
		    ((vdev_id == P2P_INVALID_VDEV_ID) ||
		     (vdev_id == roc_ctx->vdev_id))) {
			if (roc_ctx->roc_state !=
			    ROC_STATE_CANCEL_IN_PROG)
				p2p_execute_cancel_roc_req(roc_ctx);

			count++;
		}
	}

	p2p_debug("count %d", count);
	if (!count)
		qdf_event_set(&p2p_soc_obj->cleanup_roc_done);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS p2p_process_roc_req(struct p2p_roc_context *roc_ctx)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_roc_context *curr_roc_ctx;
	QDF_STATUS status;
	uint32_t size;

	p2p_soc_obj = roc_ctx->p2p_soc_obj;

	p2p_debug("p2p soc obj:%pK, roc ctx:%pK, vdev_id:%d, scan_id:%d, tx_ctx:%pK, chan:%d, phy_mode:%d, duration:%d, roc_type:%d, roc_state:%d",
		p2p_soc_obj, roc_ctx, roc_ctx->vdev_id,
		roc_ctx->scan_id, roc_ctx->tx_ctx, roc_ctx->chan,
		roc_ctx->phy_mode, roc_ctx->duration,
		roc_ctx->roc_type, roc_ctx->roc_state);

	status = qdf_list_insert_back(&p2p_soc_obj->roc_q,
			&roc_ctx->node);
	if (QDF_STATUS_SUCCESS != status) {
		p2p_destroy_roc_ctx(roc_ctx, true, false);
		p2p_debug("Failed to insert roc req, status %d", status);
		return status;
	}

	size = qdf_list_size(&p2p_soc_obj->roc_q);
	if (size == 1) {
		status = p2p_execute_roc_req(roc_ctx);
	} else if (size > 1) {
		curr_roc_ctx = p2p_find_current_roc_ctx(p2p_soc_obj);
		/*TODO, to handle extend roc */
	}

	return status;
}

QDF_STATUS p2p_process_cancel_roc_req(
	struct cancel_roc_context *cancel_roc_ctx)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_roc_context *curr_roc_ctx;
	QDF_STATUS status;

	p2p_soc_obj = cancel_roc_ctx->p2p_soc_obj;
	curr_roc_ctx = p2p_find_roc_ctx(p2p_soc_obj,
				cancel_roc_ctx->cookie);

	if (!curr_roc_ctx) {
		p2p_debug("Failed to find roc req by cookie, cookie %llx",
				cancel_roc_ctx->cookie);
		return QDF_STATUS_E_INVAL;
	}

	p2p_debug("roc ctx:%pK vdev_id:%d, scan_id:%d, roc_type:%d, roc_state:%d",
		curr_roc_ctx, curr_roc_ctx->vdev_id, curr_roc_ctx->scan_id,
		curr_roc_ctx->roc_type, curr_roc_ctx->roc_state);

	if (curr_roc_ctx->roc_state == ROC_STATE_IDLE) {
		status = p2p_destroy_roc_ctx(curr_roc_ctx, true, true);
	} else if (curr_roc_ctx->roc_state ==
				ROC_STATE_CANCEL_IN_PROG) {
		p2p_debug("Receive cancel roc req when roc req is canceling, cookie %llx",
			cancel_roc_ctx->cookie);
		status = QDF_STATUS_SUCCESS;
	} else {
		status = p2p_execute_cancel_roc_req(curr_roc_ctx);
	}

	return status;
}

void p2p_scan_event_cb(struct wlan_objmgr_vdev *vdev,
	struct scan_event *event, void *arg)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_roc_context *curr_roc_ctx;

	p2p_debug("soc:%pK, scan event:%d", arg, event->type);

	p2p_soc_obj = (struct p2p_soc_priv_obj *)arg;
	if (!p2p_soc_obj) {
		p2p_err("Invalid P2P context");
		return;
	}

	curr_roc_ctx = p2p_find_current_roc_ctx(p2p_soc_obj);
	if (!curr_roc_ctx) {
		p2p_err("Failed to find valid P2P roc context");
		return;
	}

	qdf_mtrace(QDF_MODULE_ID_SCAN, QDF_MODULE_ID_P2P, event->type,
		   event->vdev_id, event->scan_id);
	switch (event->type) {
	case SCAN_EVENT_TYPE_STARTED:
		p2p_process_scan_start_evt(curr_roc_ctx);
		break;
	case SCAN_EVENT_TYPE_FOREIGN_CHANNEL:
		p2p_process_ready_on_channel_evt(curr_roc_ctx);
		break;
	case SCAN_EVENT_TYPE_COMPLETED:
	case SCAN_EVENT_TYPE_DEQUEUED:
	case SCAN_EVENT_TYPE_START_FAILED:
		p2p_process_scan_complete_evt(curr_roc_ctx);
		break;
	default:
		p2p_debug("drop scan event, %d", event->type);
	}
}
