/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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
 * DOC: target_if_pmo_static.c
 *
 * Target interface file for pmo component to
 * send suspend / resume related cmd and process event.
 */

#include "wma.h"
#include "target_if.h"
#include "target_if_pmo.h"
#include "wmi_unified_api.h"
#include "qdf_types.h"
#include "pld_common.h"

#define TGT_WILDCARD_PDEV_ID 0x0

QDF_STATUS target_if_pmo_send_vdev_update_param_req(
		struct wlan_objmgr_vdev *vdev,
		uint32_t param_id, uint32_t param_value)
{
	uint8_t vdev_id;
	struct wlan_objmgr_psoc *psoc;
	struct vdev_set_params param = {0};
	wmi_unified_t wmi_handle;

	if (!vdev) {
		target_if_err("vdev ptr passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	vdev_id = wlan_vdev_get_id(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/* Any new param_id added here please also add it to
	 * wmi_tag_vdev_set_cmd to be tagged for runtime PM feature
	 * so that it will not invoke runtime PM "get" which will
	 * result resume right after suspend (WOW_ENABLE).
	 */

	switch (param_id) {
	case pmo_vdev_param_listen_interval:
		param_id = WMI_VDEV_PARAM_LISTEN_INTERVAL;
		break;
	case pmo_vdev_param_dtim_policy:
		param_id = WMI_VDEV_PARAM_DTIM_POLICY;
		break;
	case pmo_vdev_param_forced_dtim_count:
		param_id = WMI_VDEV_PARAM_FORCE_DTIM_CNT;
		break;
	default:
		target_if_err("invalid vdev param id %d", param_id);
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	param.vdev_id = vdev_id;
	param.param_id = param_id;
	param.param_value = param_value;
	target_if_debug("set vdev param vdev_id: %d value: %d for param_id: %d",
			vdev_id, param_value, param_id);
	return wmi_unified_vdev_set_param_send(wmi_handle, &param);
}

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
QDF_STATUS target_if_pmo_send_igmp_offload_req(
			struct wlan_objmgr_vdev *vdev,
			struct pmo_igmp_offload_req *pmo_igmp_req)
{
	struct wlan_objmgr_psoc *psoc;
	wmi_unified_t wmi_handle;

	if (!vdev) {
		target_if_err("vdev ptr passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_send_igmp_offload_cmd(wmi_handle,
						 pmo_igmp_req);
}
#endif

QDF_STATUS target_if_pmo_send_vdev_ps_param_req(
		struct wlan_objmgr_vdev *vdev,
		uint32_t param_id,
		uint32_t param_value)
{
	uint8_t vdev_id;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;
	struct sta_ps_params sta_ps_param = {0};
	wmi_unified_t wmi_handle;

	if (!vdev) {
		target_if_err("vdev ptr passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	vdev_id = wlan_vdev_get_id(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/*
	 * Any new param_id added here must be added to
	 * wmi_tag_sta_powersave_cmd() to be tagged for runtime PM feature
	 * so that it will not invoke runtime PM "get" which will
	 * result resume right after suspend (WOW_ENABLE).
	 */
	switch (param_id) {
	case pmo_sta_ps_enable_advanced_power:
		param_id = WMI_STA_PS_ENABLE_QPOWER;
		break;
	case pmo_sta_ps_param_inactivity_time:
		param_id = WMI_STA_PS_PARAM_INACTIVITY_TIME;
		break;
	case pmo_sta_ps_param_ito_repeat_count:
		param_id = WMI_STA_PS_PARAM_MAX_RESET_ITO_COUNT_ON_TIM_NO_TXRX;
		break;
	default:
		target_if_err("invalid vdev param id %d", param_id);
		return QDF_STATUS_E_INVAL;
	}

	sta_ps_param.vdev_id = vdev_id;
	sta_ps_param.param_id = param_id;
	sta_ps_param.value = param_value;
	target_if_debug("set vdev param vdev_id: %d value: %d for param_id: %d",
			vdev_id, param_value, param_id);

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_sta_ps_cmd_send(wmi_handle, &sta_ps_param);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return status;
}

void target_if_pmo_psoc_update_bus_suspend(struct wlan_objmgr_psoc *psoc,
		uint8_t value)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return;
	}

	wmi_set_is_wow_bus_suspended(wmi_handle, value);
}

int target_if_pmo_psoc_get_host_credits(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return 0;
	}

	return wmi_get_host_credits(wmi_handle);
}

int target_if_pmo_psoc_get_pending_cmnds(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return 0;
	}

	return wmi_get_pending_cmds(wmi_handle);
}

void target_if_pmo_update_target_suspend_flag(struct wlan_objmgr_psoc *psoc,
		uint8_t value)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return;
	}

	wmi_set_target_suspend(wmi_handle, value);
}

void target_if_pmo_update_target_suspend_acked_flag(
					struct wlan_objmgr_psoc *psoc,
					uint8_t value)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return;
	}

	wmi_set_target_suspend_acked(wmi_handle, value);
}

bool target_if_pmo_is_target_suspended(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return false;
	}

	return wmi_is_target_suspended(wmi_handle);
}

QDF_STATUS target_if_pmo_psoc_send_wow_enable_req(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_wow_cmd_params *param)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	wma_check_and_set_wake_timer(SIR_INSTALL_KEY_TIMEOUT_MS);
	return wmi_unified_wow_enable_send(wmi_handle,
					   (struct wow_cmd_params *)param,
					   TGT_WILDCARD_PDEV_ID);
}

QDF_STATUS target_if_pmo_psoc_send_suspend_req(
		struct wlan_objmgr_psoc *psoc,
		struct pmo_suspend_params *param)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_suspend_send(wmi_handle,
					(struct suspend_params *) param,
					TGT_WILDCARD_PDEV_ID);
}

void target_if_pmo_set_runtime_pm_in_progress(struct wlan_objmgr_psoc *psoc,
					      bool value)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return;
	}

	return wmi_set_runtime_pm_inprogress(wmi_handle, value);
}

bool target_if_pmo_get_runtime_pm_in_progress(
		struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return false;
	}

	return wmi_get_runtime_pm_inprogress(wmi_handle);
}

#ifdef HOST_WAKEUP_OVER_QMI
QDF_STATUS target_if_pmo_psoc_send_host_wakeup_ind(
		struct wlan_objmgr_psoc *psoc)
{
	qdf_device_t qdf_dev;
	int ret;

	qdf_dev = wlan_psoc_get_qdf_dev(psoc);
	if (!qdf_dev)
		return QDF_STATUS_E_INVAL;

	ret = pld_exit_power_save(qdf_dev->dev);
	if (ret) {
		target_if_err("Failed to exit power save, ret: %d", ret);
		return qdf_status_from_os_return(ret);
	}

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS target_if_pmo_psoc_send_host_wakeup_ind(
		struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_host_wakeup_ind_to_fw_cmd(wmi_handle);
}
#endif

QDF_STATUS target_if_pmo_psoc_send_target_resume_req(
		struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_resume_send(wmi_handle, TGT_WILDCARD_PDEV_ID);
}

QDF_STATUS
target_if_pmo_psoc_send_idle_monitor_cmd(struct wlan_objmgr_psoc *psoc,
					 uint8_t val)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_send_idle_trigger_monitor(wmi_handle, val);
}

#ifdef FEATURE_WLAN_D0WOW
QDF_STATUS target_if_pmo_psoc_send_d0wow_enable_req(
		struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_d0wow_enable_send(wmi_handle, TGT_WILDCARD_PDEV_ID);
}

QDF_STATUS target_if_pmo_psoc_send_d0wow_disable_req(
		struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_d0wow_disable_send(wmi_handle, TGT_WILDCARD_PDEV_ID);
}
#else
QDF_STATUS target_if_pmo_psoc_send_d0wow_enable_req(
		struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_E_INVAL;
}

QDF_STATUS target_if_pmo_psoc_send_d0wow_disable_req(
		struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_E_INVAL;
}
#endif
