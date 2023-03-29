/*
 * Copyright (c) 2015-2020 The Linux Foundation. All rights reserved.
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

#include "sme_power_save.h"
#include "sme_power_save_api.h"
#include <sir_common.h>
#include <ani_global.h>
#include <utils_api.h>
#include "sme_trace.h"
#include "qdf_mem.h"
#include "qdf_types.h"
#include "wma.h"
#include "wma_internal.h"
#include "wmm_apsd.h"
#include "csr_inside_api.h"

/**
 * sme_post_ps_msg_to_wma(): post message to WMA.
 * @type: type
 * @body: body pointer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sme_post_ps_msg_to_wma(uint16_t type, void *body)
{
	struct scheduler_msg msg = {0};

	msg.type = type;
	msg.reserved = 0;
	msg.bodyptr = body;
	msg.bodyval = 0;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"%s: Posting message %d failed",
				__func__, type);
		qdf_mem_free(body);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_enable_uapsd_req_params(): enables UASPD req params
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
static void sme_ps_fill_uapsd_req_params(struct mac_context *mac_ctx,
		tUapsd_Params *uapsdParams, uint32_t session_id,
		enum ps_state *ps_state)
{

	uint8_t uapsd_delivery_mask = 0;
	uint8_t uapsd_trigger_mask = 0;
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];

	uapsd_delivery_mask =
		ps_param->uapsd_per_ac_bit_mask |
		ps_param->uapsd_per_ac_delivery_enable_mask;

	uapsd_trigger_mask =
		ps_param->uapsd_per_ac_bit_mask |
		ps_param->uapsd_per_ac_trigger_enable_mask;

	uapsdParams->bkDeliveryEnabled =
		LIM_UAPSD_GET(ACBK, uapsd_delivery_mask);

	uapsdParams->beDeliveryEnabled =
		LIM_UAPSD_GET(ACBE, uapsd_delivery_mask);

	uapsdParams->viDeliveryEnabled =
		LIM_UAPSD_GET(ACVI, uapsd_delivery_mask);

	uapsdParams->voDeliveryEnabled =
		LIM_UAPSD_GET(ACVO, uapsd_delivery_mask);

	uapsdParams->bkTriggerEnabled =
		LIM_UAPSD_GET(ACBK, uapsd_trigger_mask);

	uapsdParams->beTriggerEnabled =
		LIM_UAPSD_GET(ACBE, uapsd_trigger_mask);

	uapsdParams->viTriggerEnabled =
		LIM_UAPSD_GET(ACVI, uapsd_trigger_mask);

	uapsdParams->voTriggerEnabled =
		LIM_UAPSD_GET(ACVO, uapsd_trigger_mask);
	if (ps_param->ps_state != FULL_POWER_MODE) {
		uapsdParams->enable_ps = true;
		*ps_state = UAPSD_MODE;
	} else {
		uapsdParams->enable_ps = false;
		*ps_state = FULL_POWER_MODE;
	}
}

static void sme_set_ps_state(struct mac_context *mac_ctx,
		uint32_t session_id, enum ps_state ps_state)
{
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];

	ps_param->ps_state = ps_state;
}

static void sme_get_ps_state(struct mac_context *mac_ctx,
		uint32_t session_id, enum ps_state *ps_state)
{
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];
	*ps_state = ps_param->ps_state;
}
/**
 * sme_ps_enable_ps_req_params(): enables power save req params
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
sme_ps_enable_ps_req_params(struct mac_context *mac_ctx, uint32_t vdev_id)
{
	struct sEnablePsParams *enable_ps_req_params;
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[vdev_id];
	enum ps_state ps_state;

	enable_ps_req_params =  qdf_mem_malloc(sizeof(*enable_ps_req_params));
	if (!enable_ps_req_params)
		return QDF_STATUS_E_NOMEM;

	if (ps_param->uapsd_per_ac_bit_mask) {
		enable_ps_req_params->psSetting = eSIR_ADDON_ENABLE_UAPSD;
		sme_ps_fill_uapsd_req_params(mac_ctx,
				&enable_ps_req_params->uapsdParams,
				vdev_id, &ps_state);
		ps_state = UAPSD_MODE;
		enable_ps_req_params->uapsdParams.enable_ps = true;
	} else {
		enable_ps_req_params->psSetting = eSIR_ADDON_NOTHING;
		ps_state = LEGACY_POWER_SAVE_MODE;
	}
	enable_ps_req_params->sessionid = vdev_id;

	wma_enable_sta_ps_mode(enable_ps_req_params);

	qdf_mem_free(enable_ps_req_params);

	sme_debug("Powersave Enable sent to FW");
	ps_param->ps_state = ps_state;

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_disable_ps_req_params(): Disable power save req params
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sme_ps_disable_ps_req_params(struct mac_context *mac_ctx,
					       uint32_t vdev_id)
{
	struct  sDisablePsParams *disable_ps_req_params;

	disable_ps_req_params = qdf_mem_malloc(sizeof(*disable_ps_req_params));
	if (!disable_ps_req_params)
		return QDF_STATUS_E_NOMEM;

	disable_ps_req_params->psSetting = eSIR_ADDON_NOTHING;
	disable_ps_req_params->sessionid = vdev_id;

	wma_disable_sta_ps_mode(disable_ps_req_params);
	qdf_mem_free(disable_ps_req_params);

	sme_debug("Powersave disable sent to FW");
	sme_set_ps_state(mac_ctx, vdev_id, FULL_POWER_MODE);

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_enable_uapsd_req_params(): enables UASPD req params
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sme_ps_enable_uapsd_req_params(struct mac_context *mac_ctx,
		uint32_t session_id)
{

	struct sEnableUapsdParams *enable_uapsd_req_params;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum ps_state ps_state;

	enable_uapsd_req_params =
		qdf_mem_malloc(sizeof(*enable_uapsd_req_params));
	if (!enable_uapsd_req_params)
		return QDF_STATUS_E_NOMEM;

	sme_ps_fill_uapsd_req_params(mac_ctx,
			&enable_uapsd_req_params->uapsdParams,
			session_id, &ps_state);
	enable_uapsd_req_params->sessionid = session_id;

	status = sme_post_ps_msg_to_wma(WMA_ENABLE_UAPSD_REQ,
					enable_uapsd_req_params);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_E_FAILURE;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		    FL("Msg WMA_ENABLE_UAPSD_REQ Successfully sent to WMA"));
	sme_set_ps_state(mac_ctx, session_id, ps_state);
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_disable_uapsd_req_params(): disables UASPD req params
 * @mac_ctx: global mac context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sme_ps_disable_uapsd_req_params(struct mac_context *mac_ctx,
		uint32_t session_id)
{
	struct sDisableUapsdParams *disable_uapsd_req_params;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum ps_state ps_state;

	sme_get_ps_state(mac_ctx, session_id, &ps_state);
	if (ps_state != UAPSD_MODE) {
		sme_err("UAPSD is already disabled");
		return QDF_STATUS_SUCCESS;
	}
	disable_uapsd_req_params =
		qdf_mem_malloc(sizeof(*disable_uapsd_req_params));
	if (!disable_uapsd_req_params)
		return QDF_STATUS_E_NOMEM;

	disable_uapsd_req_params->sessionid = session_id;
	status = sme_post_ps_msg_to_wma(WMA_DISABLE_UAPSD_REQ,
					disable_uapsd_req_params);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return QDF_STATUS_E_FAILURE;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		FL("Message WMA_DISABLE_UAPSD_REQ Successfully sent to WMA"));
	sme_set_ps_state(mac_ctx, session_id, LEGACY_POWER_SAVE_MODE);
	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_process_command(): Sme process power save messages
 *			and pass messages to WMA.
 * @mac_ctx: global mac context
 * @session_id: session id
 * sme_ps_cmd: power save message
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_process_command(struct mac_context *mac_ctx, uint32_t session_id,
		enum sme_ps_cmd command)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid Session_id: %d", session_id);
		return QDF_STATUS_E_INVAL;
	}
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Power Save command %d"), command);
	switch (command) {
	case SME_PS_ENABLE:
		status = sme_ps_enable_ps_req_params(mac_ctx, session_id);
		break;
	case SME_PS_DISABLE:
		status = sme_ps_disable_ps_req_params(mac_ctx, session_id);
		break;
	case SME_PS_UAPSD_ENABLE:
		status = sme_ps_enable_uapsd_req_params(mac_ctx, session_id);
		break;
	case SME_PS_UAPSD_DISABLE:
		status = sme_ps_disable_uapsd_req_params(mac_ctx, session_id);
		break;
	default:
		sme_err("Invalid command type: %d", command);
		status = QDF_STATUS_E_FAILURE;
		break;
	}
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			FL("Not able to enter in PS, Command: %d"), command);
	}
	return status;
}

/**
 * sme_enable_sta_ps_check(): Checks if it is ok to enable power save or not.
 * @mac_ctx: global mac context
 * @session_id: session id
 * @ command: sme_ps_cmd
 * Pre Condition for enabling sta mode power save
 * 1) Sta Mode Ps should be enabled in ini file.
 * 2) Session should be in infra mode and in connected state.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_enable_sta_ps_check(struct mac_context *mac_ctx,
				   uint32_t session_id, enum sme_ps_cmd command)
{
	struct wlan_mlme_powersave *powersave_params;
	bool usr_cfg_ps_enable;

	QDF_BUG(session_id < WLAN_MAX_VDEVS);
	if (session_id >= WLAN_MAX_VDEVS)
		return QDF_STATUS_E_INVAL;

	/* Check if Sta Ps is enabled. */
	powersave_params = &mac_ctx->mlme_cfg->ps_params;
	if (!powersave_params->is_bmps_enabled) {
		sme_debug("Cannot initiate PS. PS is disabled in ini");
		return QDF_STATUS_E_FAILURE;
	}

	usr_cfg_ps_enable = mlme_get_user_ps(mac_ctx->psoc, session_id);
	if (command == SME_PS_ENABLE && !usr_cfg_ps_enable) {
		sme_debug("vdev:%d Cannot initiate PS. PS is disabled by usr(ioctl)",
			  session_id);
		return QDF_STATUS_E_FAILURE;
	}

	/* Check whether the given session is Infra and in Connected State
	 * also if command is power save disable  there is not need to check
	 * for connected state as firmware can handle this
	 */
	if (!csr_is_conn_state_connected_infra(mac_ctx, session_id)) {
		sme_debug("vdev:%d STA not infra/connected state",
			  session_id);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_enable_disable(): function to enable/disable PS.
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: session id
 * sme_ps_cmd: power save message
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_enable_disable(mac_handle_t mac_handle, uint32_t session_id,
				 enum sme_ps_cmd command)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	status =  sme_enable_sta_ps_check(mac_ctx, session_id, command);
	if (status != QDF_STATUS_SUCCESS) {
		/*
		 * In non associated state driver wont handle the power save
		 * But kernel expects return status success even
		 * in the disconnected state.
		 */
		if (!csr_is_conn_state_connected_infra(mac_ctx, session_id))
			status = QDF_STATUS_SUCCESS;
		return status;
	}
	status = sme_ps_process_command(mac_ctx, session_id, command);
	return status;
}

QDF_STATUS sme_ps_timer_flush_sync(mac_handle_t mac_handle, uint8_t session_id)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct ps_params *ps_parm;
	enum ps_state ps_state;
	QDF_TIMER_STATE tstate;
	struct sEnablePsParams *req;

	QDF_BUG(session_id < WLAN_MAX_VDEVS);
	if (session_id >= WLAN_MAX_VDEVS)
		return QDF_STATUS_E_INVAL;

	status = sme_enable_sta_ps_check(mac_ctx, session_id, SME_PS_ENABLE);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_debug("Power save not allowed for vdev id %d", session_id);
		return QDF_STATUS_SUCCESS;
	}

	ps_parm = &mac_ctx->sme.ps_global_info.ps_params[session_id];
	tstate = qdf_mc_timer_get_current_state(&ps_parm->auto_ps_enable_timer);
	if (tstate != QDF_TIMER_STATE_RUNNING)
		return QDF_STATUS_SUCCESS;

	sme_debug("flushing powersave enable for vdev %u", session_id);

	qdf_mc_timer_stop(&ps_parm->auto_ps_enable_timer);

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	if (ps_parm->uapsd_per_ac_bit_mask) {
		req->psSetting = eSIR_ADDON_ENABLE_UAPSD;
		sme_ps_fill_uapsd_req_params(mac_ctx, &req->uapsdParams,
					     session_id, &ps_state);
		ps_state = UAPSD_MODE;
		req->uapsdParams.enable_ps = true;
	} else {
		req->psSetting = eSIR_ADDON_NOTHING;
		ps_state = LEGACY_POWER_SAVE_MODE;
	}
	req->sessionid = session_id;

	wma_enable_sta_ps_mode(req);
	qdf_mem_free(req);

	ps_parm->ps_state = ps_state;

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_ps_uapsd_enable(): function to enable UAPSD.
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_uapsd_enable(mac_handle_t mac_handle, uint32_t session_id)
{

	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	status =  sme_enable_sta_ps_check(mac_ctx, session_id,
					SME_PS_UAPSD_ENABLE);
	if (status != QDF_STATUS_SUCCESS)
		return status;
	status = sme_ps_process_command(mac_ctx, session_id,
			SME_PS_UAPSD_ENABLE);
	if (status == QDF_STATUS_SUCCESS)
		sme_offload_qos_process_into_uapsd_mode(mac_ctx, session_id);

	return status;
}

/**
 * sme_ps_uapsd_disable(): function to disable UAPSD.
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_uapsd_disable(mac_handle_t mac_handle, uint32_t session_id)
{

	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	status = sme_enable_sta_ps_check(mac_ctx, session_id,
					SME_PS_UAPSD_DISABLE);
	if (status != QDF_STATUS_SUCCESS)
		return status;
	status = sme_ps_process_command(mac_ctx, session_id,
			SME_PS_UAPSD_DISABLE);
	if (status == QDF_STATUS_SUCCESS)
		sme_offload_qos_process_out_of_uapsd_mode(mac_ctx, session_id);

	return status;
}

/**
 * sme_set_tspec_uapsd_mask_per_session(): set tspec UAPSD mask per session
 * @mac_ctx: global mac context
 * @ts_info: tspec info.
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
void sme_set_tspec_uapsd_mask_per_session(struct mac_context *mac_ctx,
					  struct mac_ts_info *ts_info,
					  uint8_t session_id)
{
	uint8_t user_prio = (uint8_t) ts_info->traffic.userPrio;
	uint16_t direction = ts_info->traffic.direction;
	uint8_t ac = upToAc(user_prio);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];

	sme_err("Set UAPSD mask for AC: %d dir: %d action: %d",
		ac, direction, ts_info->traffic.psb);

	/* Converting AC to appropriate Uapsd Bit Mask
	 * AC_BE(0) --> UAPSD_BITOFFSET_ACVO(3)
	 * AC_BK(1) --> UAPSD_BITOFFSET_ACVO(2)
	 * AC_VI(2) --> UAPSD_BITOFFSET_ACVO(1)
	 * AC_VO(3) --> UAPSD_BITOFFSET_ACVO(0)
	 */
	ac = ((~ac) & 0x3);
	if (ts_info->traffic.psb) {
		ps_param->uapsd_per_ac_bit_mask |= (1 << ac);
		if (direction == SIR_MAC_DIRECTION_UPLINK)
			ps_param->uapsd_per_ac_trigger_enable_mask |=
				(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_DNLINK)
			ps_param->uapsd_per_ac_delivery_enable_mask |=
				(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_BIDIR) {
			ps_param->uapsd_per_ac_trigger_enable_mask |=
				(1 << ac);
			ps_param->uapsd_per_ac_delivery_enable_mask |=
				(1 << ac);
		}
	} else {
		ps_param->uapsd_per_ac_bit_mask &= ~(1 << ac);
		if (direction == SIR_MAC_DIRECTION_UPLINK)
			ps_param->uapsd_per_ac_trigger_enable_mask &=
				~(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_DNLINK)
			ps_param->uapsd_per_ac_delivery_enable_mask &=
				~(1 << ac);
		else if (direction == SIR_MAC_DIRECTION_BIDIR) {
			ps_param->uapsd_per_ac_trigger_enable_mask &=
				~(1 << ac);
			ps_param->uapsd_per_ac_delivery_enable_mask &=
				~(1 << ac);
		}
	}

	/*
	 * ADDTS success, so AC is now admitted. We shall now use the default
	 * EDCA parameters as advertised by AP and send the updated EDCA params
	 * to HAL.
	 */
	if (direction == SIR_MAC_DIRECTION_UPLINK) {
		ps_param->ac_admit_mask[SIR_MAC_DIRECTION_UPLINK] |=
			(1 << ac);
	} else if (direction == SIR_MAC_DIRECTION_DNLINK) {
		ps_param->ac_admit_mask[SIR_MAC_DIRECTION_DNLINK] |=
			(1 << ac);
	} else if (direction == SIR_MAC_DIRECTION_BIDIR) {
		ps_param->ac_admit_mask[SIR_MAC_DIRECTION_UPLINK] |=
			(1 << ac);
		ps_param->ac_admit_mask[SIR_MAC_DIRECTION_DNLINK] |=
			(1 << ac);
	}

	sme_debug("New ps_param->uapsd_per_ac_trigger_enable_mask: 0x%x",
		ps_param->uapsd_per_ac_trigger_enable_mask);
	sme_debug("New  ps_param->uapsd_per_ac_delivery_enable_mask: 0x%x",
		ps_param->uapsd_per_ac_delivery_enable_mask);
	sme_debug("New ps_param->ac_admit_mask[SIR_MAC_DIRECTION_UPLINK]: 0x%x",
		ps_param->ac_admit_mask[SIR_MAC_DIRECTION_UPLINK]);
}

/**
 * sme_ps_start_uapsd(): function to start UAPSD.
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id: session id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS sme_ps_start_uapsd(mac_handle_t mac_handle, uint32_t session_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	status = sme_ps_uapsd_enable(mac_handle, session_id);
	return status;
}

/**
 * sme_set_ps_host_offload(): Set the host offload feature.
 * @mac_handle - The handle returned by mac_open.
 * @request - Pointer to the offload request.
 *
 * Return QDF_STATUS
 *            QDF_STATUS_E_FAILURE  Cannot set the offload.
 *            QDF_STATUS_SUCCESS  Request accepted.
 */
QDF_STATUS sme_set_ps_host_offload(mac_handle_t mac_handle,
				   struct sir_host_offload_req *request,
				   uint8_t session_id)
{
	struct sir_host_offload_req *request_buf;
	struct scheduler_msg msg = {0};
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"%s: IP address = %d.%d.%d.%d", __func__,
			request->params.hostIpv4Addr[0],
			request->params.hostIpv4Addr[1],
			request->params.hostIpv4Addr[2],
			request->params.hostIpv4Addr[3]);

	if (!session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"%s: SESSION not Found", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	request_buf = qdf_mem_malloc(sizeof(struct sir_host_offload_req));
	if (!request_buf)
		return QDF_STATUS_E_NOMEM;

	qdf_copy_macaddr(&request->bssid, &session->connectedProfile.bssid);

	qdf_mem_copy(request_buf, request, sizeof(struct sir_host_offload_req));

	msg.type = WMA_SET_HOST_OFFLOAD;
	msg.reserved = 0;
	msg.bodyptr = request_buf;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 session_id, msg.type));
	if (QDF_STATUS_SUCCESS !=
			scheduler_post_message(QDF_MODULE_ID_SME,
					       QDF_MODULE_ID_WMA,
					       QDF_MODULE_ID_WMA, &msg)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
		      FL("Not able to post WMA_SET_HOST_OFFLOAD msg to WMA"));
		qdf_mem_free(request_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_NS_OFFLOAD
/**
 * sme_set_ps_ns_offload(): Set the host offload feature.
 * @mac_handle - The handle returned by mac_open.
 * @request - Pointer to the offload request.
 *
 * Return QDF_STATUS
 *		QDF_STATUS_E_FAILURE  Cannot set the offload.
 *		QDF_STATUS_SUCCESS  Request accepted.
 */
QDF_STATUS sme_set_ps_ns_offload(mac_handle_t mac_handle,
				 struct sir_host_offload_req *request,
				 uint8_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct sir_host_offload_req *request_buf;
	struct scheduler_msg msg = {0};
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("Session not found");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_copy_macaddr(&request->bssid, &session->connectedProfile.bssid);

	request_buf = qdf_mem_malloc(sizeof(*request_buf));
	if (!request_buf)
		return QDF_STATUS_E_NOMEM;

	*request_buf = *request;

	msg.type = WMA_SET_NS_OFFLOAD;
	msg.reserved = 0;
	msg.bodyptr = request_buf;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 session_id, msg.type));
	if (QDF_STATUS_SUCCESS !=
			scheduler_post_message(QDF_MODULE_ID_SME,
					       QDF_MODULE_ID_WMA,
					       QDF_MODULE_ID_WMA, &msg)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"Not able to post SIR_HAL_SET_HOST_OFFLOAD message to HAL");
		qdf_mem_free(request_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_NS_OFFLOAD */
/**
 * sme_post_pe_message
 *
 * FUNCTION:
 * Post a message to the pmm message queue
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param msg pointer to message
 * @return None
 */

QDF_STATUS sme_post_pe_message(struct mac_context *mac_ctx,
			       struct scheduler_msg *msg)
{
	QDF_STATUS qdf_status;

	qdf_status = scheduler_post_message(QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_PE,
					    QDF_MODULE_ID_PE,
					    msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		sme_err("scheduler_post_msg failed with status: %d",
			qdf_status);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_ps_enable_auto_ps_timer(mac_handle_t mac_handle,
				       uint32_t session_id, uint32_t timeout)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];
	QDF_STATUS qdf_status;
	QDF_TIMER_STATE cur_state;
	bool usr_cfg_ps_enable;

	usr_cfg_ps_enable =
		mlme_get_user_ps(mac_ctx->psoc, session_id);
	if (!timeout && !usr_cfg_ps_enable) {
		sme_debug("auto_ps_timer called with timeout 0; ignore");
		return QDF_STATUS_SUCCESS;
	}
	if (!timeout)
		timeout = AUTO_PS_ENTRY_TIMER_DEFAULT_VALUE;

	cur_state =
		qdf_mc_timer_get_current_state(&ps_param->auto_ps_enable_timer);
	if (cur_state == QDF_TIMER_STATE_STARTING ||
	    cur_state == QDF_TIMER_STATE_RUNNING) {
		sme_debug("auto_ps_timer is already started: %d", cur_state);
		return QDF_STATUS_SUCCESS;
	}

	sme_debug("Start auto_ps_timer for %d ms", timeout);
	qdf_status = qdf_mc_timer_start(&ps_param->auto_ps_enable_timer,
		timeout);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		if (QDF_STATUS_E_ALREADY == qdf_status) {
			/* Consider this ok since the timer is already started*/
			sme_debug("auto_ps_timer is already started");
		} else {
			sme_err("Cannot start auto_ps_timer");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_ps_disable_auto_ps_timer(mac_handle_t mac_handle,
					uint32_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param;

	if (!sme_is_session_id_valid(mac_handle, session_id))
		return QDF_STATUS_SUCCESS;

	ps_param = &ps_global_info->ps_params[session_id];
	/*
	 * Stop the auto ps entry timer if runnin
	 */
	if (QDF_TIMER_STATE_RUNNING ==
			qdf_mc_timer_get_current_state(
				&ps_param->auto_ps_enable_timer)) {
		sme_debug("Stop auto_ps_enable_timer Timer for session ID: %d",
				session_id);
		qdf_mc_timer_stop(&ps_param->auto_ps_enable_timer);
	}
	return QDF_STATUS_SUCCESS;
}


QDF_STATUS sme_ps_open(mac_handle_t mac_handle)
{
	uint32_t i;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		mlme_set_user_ps(mac->psoc, i, true);

		if (QDF_STATUS_SUCCESS != sme_ps_open_per_session(mac_handle,
								  i)) {
			sme_err("PMC Init Failed for session: %d", i);
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}


QDF_STATUS sme_ps_open_per_session(mac_handle_t mac_handle, uint32_t session_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];

	ps_param->session_id = session_id;
	ps_param->mac_ctx = mac_ctx;
	/* Allocate a timer to enable ps automatically */
	if (!QDF_IS_STATUS_SUCCESS(qdf_mc_timer_init(
					&ps_param->auto_ps_enable_timer,
					QDF_TIMER_TYPE_SW,
					sme_auto_ps_entry_timer_expired,
					ps_param)))     {
		sme_err("Cannot allocate timer for auto ps entry");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;

}

void sme_auto_ps_entry_timer_expired(void *data)
{
	struct ps_params *ps_params = (struct ps_params *)data;
	struct mac_context *mac_ctx;
	uint32_t session_id;
	QDF_STATUS status;

	if (!ps_params) {
		sme_err("ps_params is NULL");
		return;
	}
	mac_ctx = (struct mac_context *)ps_params->mac_ctx;
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return;
	}
	session_id = ps_params->session_id;
	sme_debug("auto_ps_timer expired, enabling powersave");

	status = sme_enable_sta_ps_check(mac_ctx, session_id, SME_PS_ENABLE);
	if (QDF_STATUS_SUCCESS == status)
		sme_ps_enable_disable(MAC_HANDLE(mac_ctx), session_id,
				      SME_PS_ENABLE);
}

QDF_STATUS sme_ps_close(mac_handle_t mac_handle)
{
	uint32_t i;

	for (i = 0; i < WLAN_MAX_VDEVS; i++)
		sme_ps_close_per_session(mac_handle, i);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_ps_close_per_session(mac_handle_t mac_handle,
				    uint32_t session_id)
{

	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[session_id];
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	/*
	 * Stop the auto ps entry timer if running
	 */
	if (QDF_TIMER_STATE_RUNNING ==
			qdf_mc_timer_get_current_state(
				&ps_param->auto_ps_enable_timer))
		qdf_mc_timer_stop(&ps_param->auto_ps_enable_timer);

	qdf_status =
		qdf_mc_timer_destroy(&ps_param->auto_ps_enable_timer);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		sme_err("Cannot deallocate suto PS timer");
	return qdf_status;
}
