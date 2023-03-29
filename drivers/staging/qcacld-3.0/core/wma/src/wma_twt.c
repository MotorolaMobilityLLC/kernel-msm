/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wma_twt.c
 *
 * WLAN Host Device Driver TWT - Target Wake Time Implementation
 */
#include "wma_twt.h"
#include "wmi_unified_twt_api.h"
#include "wma_internal.h"
#include "wmi_unified_priv.h"

void wma_send_twt_enable_cmd(uint32_t pdev_id,
			     struct twt_enable_disable_conf *conf)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct wmi_twt_enable_param twt_enable_params = {0};
	int32_t ret;

	if (!wma) {
		wma_err("Invalid WMA context, enable TWT failed");
		return;
	}
	twt_enable_params.pdev_id = pdev_id;
	twt_enable_params.sta_cong_timer_ms = conf->congestion_timeout;
	twt_enable_params.b_twt_enable = conf->bcast_en;
	twt_enable_params.ext_conf_present = conf->ext_conf_present;
	twt_enable_params.twt_role = conf->role;
	twt_enable_params.twt_oper = conf->oper;
	ret = wmi_unified_twt_enable_cmd(wma->wmi_handle, &twt_enable_params);

	if (ret)
		wma_err("Failed to enable TWT");
}

/**
 * wma_twt_en_complete_event_handler - TWT enable complete event handler
 * @handle: wma handle
 * @event: buffer with event
 * @len: buffer length
 *
 * Return: 0 on success, negative value on failure
 */
static
int wma_twt_en_complete_event_handler(void *handle,
				      uint8_t *event, uint32_t len)
{
	struct wmi_twt_enable_complete_event_param param;
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	int status = -EINVAL;

	if (!wma_handle) {
		wma_err("Invalid wma handle for TWT complete");
		return status;
	}
	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle for TWT complete");
		return status;
	}
	if (!mac) {
		wma_err("Invalid MAC context");
		return status;
	}
	if (wmi_handle->ops->extract_twt_enable_comp_event)
		status = wmi_handle->ops->extract_twt_enable_comp_event(
								wmi_handle,
								event,
								&param);
	wma_debug("TWT: Received TWT enable comp event, status:%d", status);

	if (mac->sme.twt_enable_cb)
		mac->sme.twt_enable_cb(mac->hdd_handle, &param);

	return status;
}

void wma_send_twt_disable_cmd(uint32_t pdev_id,
			      struct twt_enable_disable_conf *conf)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct wmi_twt_disable_param twt_disable_params = {0};
	int32_t ret;

	if (!wma) {
		wma_err("Invalid WMA context, Disable TWT failed");
		return;
	}
	twt_disable_params.pdev_id = pdev_id;
	twt_disable_params.ext_conf_present = conf->ext_conf_present;
	twt_disable_params.twt_role = conf->role;
	twt_disable_params.twt_oper = conf->oper;

	ret = wmi_unified_twt_disable_cmd(wma->wmi_handle, &twt_disable_params);

	if (ret)
		wma_err("Failed to disable TWT");
}

/**
 * wma_twt_disable_comp_event_handler- TWT disable complete event handler
 * @handle: wma handle
 * @event: buffer with event
 * @len: buffer length
 *
 * Return: 0 on success, negative value on failure
 */
static
int wma_twt_disable_comp_event_handler(void *handle, uint8_t *event,
				       uint32_t len)
{
	struct mac_context *mac;

	mac = (struct mac_context *)cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		wma_err("Invalid MAC context");
		return -EINVAL;
	}

	wma_debug("TWT: Rcvd TWT disable comp event");

	if (mac->sme.twt_disable_cb)
		mac->sme.twt_disable_cb(mac->hdd_handle);

	return 0;
}

void wma_set_twt_peer_caps(tpAddStaParams params, struct peer_assoc_params *cmd)
{
	if (params->twt_requestor)
		cmd->twt_requester = 1;
	if (params->twt_responder)
		cmd->twt_responder = 1;
}

QDF_STATUS wma_twt_process_add_dialog(t_wma_handle *wma_handle,
				      struct wmi_twt_add_dialog_param *params)
{
	wmi_unified_t wmi_handle;

	if (!wma_handle) {
		wma_err("Invalid WMA context, twt add dialog failed");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle, twt add dialog failed");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_twt_add_dialog_cmd(wmi_handle, params);
}

/**
 * wma_twt_add_dialog_complete_event_handler - TWT add dialog complete event
 * handler
 * @handle: wma handle
 * @event: buffer with event
 * @len: buffer length
 *
 * Return: 0 on success, negative value on failure
 */
static
int wma_twt_add_dialog_complete_event_handler(void *handle,
					      uint8_t *event, uint32_t len)
{
	struct twt_add_dialog_complete_event *add_dialog_event;
	struct scheduler_msg sme_msg = {0};
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	QDF_STATUS status;

	if (!wma_handle) {
		wma_err("Invalid wma handle for TWT add dialog complete");
		return -EINVAL;
	}

	if (!mac) {
		wma_err("Invalid MAC context");
		return -EINVAL;
	}

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wma handle for TWT add dialog complete");
		return -EINVAL;
	}

	add_dialog_event = qdf_mem_malloc(sizeof(*add_dialog_event));
	if (!add_dialog_event)
		return -ENOMEM;

	status = wmi_extract_twt_add_dialog_comp_event(wmi_handle, event,
						       &add_dialog_event->params);
	if (QDF_IS_STATUS_ERROR(status))
		goto exit;

	if (add_dialog_event->params.num_additional_twt_params) {
		status = wmi_extract_twt_add_dialog_comp_additional_params(wmi_handle,
									   event,
									   len, 0,
									   &add_dialog_event->additional_params);
		if (QDF_IS_STATUS_ERROR(status))
			goto exit;
	}

	wma_debug("TWT: Extract TWT add dialog event id:%d",
		  add_dialog_event->params.dialog_id);

	sme_msg.type = eWNI_SME_TWT_ADD_DIALOG_EVENT;
	sme_msg.bodyptr = add_dialog_event;
	sme_msg.bodyval = 0;
	status = scheduler_post_message(QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &sme_msg);
	if (QDF_IS_STATUS_ERROR(status))
		goto exit;

exit:
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(add_dialog_event);

	return qdf_status_to_os_return(status);
}

QDF_STATUS
wma_twt_process_del_dialog(t_wma_handle *wma_handle,
			   struct wmi_twt_del_dialog_param *params)
{
	wmi_unified_t wmi_handle;

	if (!wma_handle) {
		wma_err("Invalid WMA context, twt del dialog failed");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle, twt del dialog failed");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_twt_del_dialog_cmd(wmi_handle, params);
}

/**
 * wma_twt_del_dialog_complete_event_handler - TWT del dialog complete event
 * handler
 * @handle: wma handle
 * @event: buffer with event
 * @len: buffer length
 *
 * Return: 0 on success, negative value on failure
 */
static
int wma_twt_del_dialog_complete_event_handler(void *handle,
					      uint8_t *event, uint32_t len)
{
	struct wmi_twt_del_dialog_complete_event_param *param;
	struct scheduler_msg sme_msg = {0};
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	int status = -EINVAL;

	if (!wma_handle) {
		wma_err("Invalid wma handle for TWT del dialog complete");
		return status;
	}
	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wma handle for TWT del dialog complete");
		return status;
	}
	if (!mac) {
		wma_err("Invalid MAC context");
		return status;
	}

	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return -ENOMEM;

	status = wmi_extract_twt_del_dialog_comp_event(wmi_handle, event,
						       param);
	wma_debug("TWT: Extract TWT del dlg comp event, status:%d", status);

	sme_msg.type = eWNI_SME_TWT_DEL_DIALOG_EVENT;
	sme_msg.bodyptr = param;
	sme_msg.bodyval = 0;
	status = scheduler_post_message(QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &sme_msg);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	return status;
}

QDF_STATUS
wma_twt_process_pause_dialog(t_wma_handle *wma_handle,
			     struct wmi_twt_pause_dialog_cmd_param *params)
{
	wmi_unified_t wmi_handle;

	if (!wma_handle) {
		wma_err("Invalid wma handle, twt pause dialog failed");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle, twt pause dialog failed");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_twt_pause_dialog_cmd(wmi_handle, params);
}

QDF_STATUS
wma_twt_process_nudge_dialog(t_wma_handle *wma_handle,
			     struct wmi_twt_nudge_dialog_cmd_param *params)
{
	wmi_unified_t wmi_handle;

	if (wma_validate_handle(wma_handle))
		return QDF_STATUS_E_INVAL;

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle, twt nudge dialog failed");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_twt_nudge_dialog_cmd(wmi_handle, params);
}

/**
 * wma_twt_pause_dialog_complete_event_handler - TWT pause dlg complete evt
 * handler
 * @handle: wma handle
 * @event: buffer with event
 * @len: buffer length
 *
 * Return: 0 on success, negative value on failure
 */
static
int wma_twt_pause_dialog_complete_event_handler(void *handle, uint8_t *event,
						uint32_t len)
{
	struct wmi_twt_pause_dialog_complete_event_param *param;
	struct scheduler_msg sme_msg = {0};
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	int status = -EINVAL;

	if (!mac) {
		wma_err("Invalid MAC context");
		return status;
	}

	if (!wma_handle) {
		wma_err("Invalid wma handle for TWT pause dialog complete");
		return status;
	}

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle for TWT pause dialog complete");
		return status;
	}

	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return -ENOMEM;

	if (wmi_handle->ops->extract_twt_pause_dialog_comp_event)
		status = wmi_handle->ops->extract_twt_pause_dialog_comp_event(wmi_handle,
									      event,
									      param);
	wma_debug("TWT: Extract pause dialog comp event status:%d", status);

	sme_msg.type = eWNI_SME_TWT_PAUSE_DIALOG_EVENT;
	sme_msg.bodyptr = param;
	sme_msg.bodyval = 0;
	status = scheduler_post_message(QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &sme_msg);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	return status;
}

/**
 * wma_twt_nudge_dialog_complete_event_handler - TWT nudge dlg complete evt
 * handler
 * @handle: wma handle
 * @event: buffer with event
 * @len: buffer length
 *
 * Return: 0 on success, negative value on failure
 */
static
int wma_twt_nudge_dialog_complete_event_handler(void *handle, uint8_t *event,
						uint32_t len)
{
	struct wmi_twt_nudge_dialog_complete_event_param *param;
	struct scheduler_msg sme_msg = {0};
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	int status = -EINVAL;

	if (!mac)
		return status;

	if (wma_validate_handle(wma_handle))
		return status;

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle for TWT nudge dialog complete");
		return status;
	}

	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return -ENOMEM;

	if (wmi_handle->ops->extract_twt_nudge_dialog_comp_event)
		status = wmi_handle->ops->extract_twt_nudge_dialog_comp_event(
						      wmi_handle, event, param);

	wma_debug("TWT: Extract nudge dialog comp event status:%d", status);

	sme_msg.type = eWNI_SME_TWT_NUDGE_DIALOG_EVENT;
	sme_msg.bodyptr = param;
	sme_msg.bodyval = 0;
	status = scheduler_post_message(QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &sme_msg);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	return status;
}
QDF_STATUS
wma_twt_process_resume_dialog(t_wma_handle *wma_handle,
			      struct wmi_twt_resume_dialog_cmd_param *params)
{
	wmi_unified_t wmi_handle;

	if (!wma_handle) {
		wma_err("Invalid wma handle, twt resume dialog failed");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle, twt resume dialog failed");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_twt_resume_dialog_cmd(wmi_handle, params);
}

/**
 * wma_twt_notify_event_handler - TWT notify event handler
 * @handle: wma handle
 * @event: buffer with event
 * @len: buffer length
 *
 * Return: 0 on success, negative value on failure
 */
static
int wma_twt_notify_event_handler(void *handle, uint8_t *event, uint32_t len)
{
	struct wmi_twt_notify_event_param *param;
	struct scheduler_msg sme_msg = {0};
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	int status = -EINVAL;

	if (!mac)
		return status;

	if (wma_validate_handle(wma_handle))
		return status;

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle for TWT notify event");
		return status;
	}

	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return -ENOMEM;

	if (wmi_handle->ops->extract_twt_notify_event)
		status = wmi_handle->ops->extract_twt_notify_event(wmi_handle,
								   event,
								   param);
	wma_debug("Extract Notify event status:%d", status);

	sme_msg.type = eWNI_SME_TWT_NOTIFY_EVENT;
	sme_msg.bodyptr = param;
	sme_msg.bodyval = 0;
	status = scheduler_post_message(QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &sme_msg);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	return 0;
}

/**
 * wma_twt_resume_dialog_complete_event_handler - TWT resume dlg complete evt
 * handler
 * @handle: wma handle
 * @event: buffer with event
 * @len: buffer length
 *
 * Return: 0 on success, negative value on failure
 */
static
int wma_twt_resume_dialog_complete_event_handler(void *handle, uint8_t *event,
						 uint32_t len)
{
	struct wmi_twt_resume_dialog_complete_event_param *param;
	struct scheduler_msg sme_msg = {0};
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	int status = -EINVAL;

	if (!mac) {
		wma_err("Invalid MAC context");
		return status;
	}

	if (!wma_handle) {
		wma_err("Invalid wma handle for TWT resume dialog complete");
		return status;
	}

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle for TWT resume dialog complete");
		return status;
	}

	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return -ENOMEM;

	if (wmi_handle->ops->extract_twt_resume_dialog_comp_event)
		status = wmi_handle->ops->extract_twt_resume_dialog_comp_event(wmi_handle,
									       event,
									       param);
	wma_debug("TWT: Extract resume dialog comp event status:%d", status);

	sme_msg.type = eWNI_SME_TWT_RESUME_DIALOG_EVENT;
	sme_msg.bodyptr = param;
	sme_msg.bodyval = 0;
	status = scheduler_post_message(QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &sme_msg);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	return status;
}

static
int wma_twt_ack_complete_event_handler(void *handle, uint8_t *event,
				       uint32_t len)
{
	struct wmi_twt_ack_complete_event_param *param;
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	QDF_STATUS status;

	if (!mac)
		return -EINVAL;

	if (wma_validate_handle(wma_handle))
		return -EINVAL;

	wmi_handle = (wmi_unified_t)wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("Invalid wmi handle for TWT ack complete");
		return -EINVAL;
	}

	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return -ENOMEM;

	status = wmi_extract_twt_ack_comp_event(wmi_handle, event,
						param);

	wma_debug("TWT: Received TWT ack comp event, status:%d", status);

	if (QDF_IS_STATUS_ERROR(status))
		goto exit;

	if (mac->sme.twt_ack_comp_cb)
		mac->sme.twt_ack_comp_cb(param, mac->sme.twt_ack_context_cb);

exit:
	qdf_mem_free(param);
	return qdf_status_to_os_return(status);
}

/**
 * wma_update_bcast_twt_support() - update bcost twt support
 * @wh: wma handle
 * @tgt_cfg: target configuration to be updated
 *
 * Update braodcast twt support based on service bit.
 *
 * Return: None
 */
void wma_update_bcast_twt_support(tp_wma_handle wh,
				  struct wma_tgt_cfg *tgt_cfg)
{
	if (wmi_service_enabled(wh->wmi_handle,
				wmi_service_bcast_twt_support))
		tgt_cfg->legacy_bcast_twt_support = true;
	else
		tgt_cfg->legacy_bcast_twt_support = false;

	if (wmi_service_enabled(wh->wmi_handle,
				wmi_service_twt_bcast_req_support))
		tgt_cfg->twt_bcast_req_support = true;
	else
		tgt_cfg->twt_bcast_req_support = false;

	if (wmi_service_enabled(wh->wmi_handle,
				wmi_service_twt_bcast_resp_support))
		tgt_cfg->twt_bcast_res_support = true;
	else
		tgt_cfg->twt_bcast_res_support = false;
}

void wma_update_twt_tgt_cap(tp_wma_handle wh, struct wma_tgt_cfg *tgt_cfg)
{
	if (wmi_service_enabled(wh->wmi_handle, wmi_service_twt_nudge))
		tgt_cfg->twt_nudge_enabled = true;

	if (wmi_service_enabled(wh->wmi_handle, wmi_service_all_twt))
		tgt_cfg->all_twt_enabled = true;

	if (wmi_service_enabled(wh->wmi_handle, wmi_service_twt_statistics))
		tgt_cfg->twt_stats_enabled = true;
}

void wma_register_twt_events(tp_wma_handle wma_handle)
{
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_twt_enable_complete_event_id,
					   wma_twt_en_complete_event_handler,
					   WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_twt_disable_complete_event_id,
					   wma_twt_disable_comp_event_handler,
					   WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler
				(wma_handle->wmi_handle,
				 wmi_twt_add_dialog_complete_event_id,
				 wma_twt_add_dialog_complete_event_handler,
				 WMA_RX_WORK_CTX);
	wmi_unified_register_event_handler
				(wma_handle->wmi_handle,
				 wmi_twt_del_dialog_complete_event_id,
				 wma_twt_del_dialog_complete_event_handler,
				 WMA_RX_WORK_CTX);

	wmi_unified_register_event_handler
				(wma_handle->wmi_handle,
				 wmi_twt_pause_dialog_complete_event_id,
				 wma_twt_pause_dialog_complete_event_handler,
				 WMA_RX_WORK_CTX);
	wmi_unified_register_event_handler
				(wma_handle->wmi_handle,
				 wmi_twt_resume_dialog_complete_event_id,
				 wma_twt_resume_dialog_complete_event_handler,
				 WMA_RX_WORK_CTX);
	wmi_unified_register_event_handler
				(wma_handle->wmi_handle,
				 wmi_twt_nudge_dialog_complete_event_id,
				 wma_twt_nudge_dialog_complete_event_handler,
				 WMA_RX_WORK_CTX);
	wmi_unified_register_event_handler
				(wma_handle->wmi_handle,
				 wmi_twt_notify_event_id,
				 wma_twt_notify_event_handler,
				 WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler
				(wma_handle->wmi_handle,
				 wmi_twt_ack_complete_event_id,
				 wma_twt_ack_complete_event_handler,
				 WMA_RX_WORK_CTX);
}
