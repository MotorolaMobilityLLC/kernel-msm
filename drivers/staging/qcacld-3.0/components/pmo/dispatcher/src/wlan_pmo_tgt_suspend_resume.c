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
 * DOC: Implements public API for pmo to interact with target/WMI
 */

#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_wow.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"
#include "wlan_pmo_main.h"

QDF_STATUS pmo_tgt_vdev_update_param_req(struct wlan_objmgr_vdev *vdev,
		enum pmo_vdev_param_id param_id, uint32_t param_value)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();

	psoc = pmo_vdev_get_psoc(vdev);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_vdev_param_update_req) {
		pmo_err("send_vdev_param_update_req is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_tx_ops.send_vdev_param_update_req(vdev, param_id,
			param_value);
out:
	pmo_exit();

	return status;
}

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
QDF_STATUS
pmo_tgt_send_igmp_offload_req(struct wlan_objmgr_vdev *vdev,
			      struct pmo_igmp_offload_req *pmo_igmp_req)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	psoc = pmo_vdev_get_psoc(vdev);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_igmp_offload_req) {
		pmo_err("send_vdev_param_set_req is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = pmo_tx_ops.send_igmp_offload_req(vdev, pmo_igmp_req);

	return status;
}
#endif

QDF_STATUS pmo_tgt_send_vdev_sta_ps_param(struct wlan_objmgr_vdev *vdev,
		enum pmo_sta_powersave_param ps_param, uint32_t param_value)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();

	psoc = pmo_vdev_get_psoc(vdev);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_vdev_sta_ps_param_req) {
		pmo_err("send_vdev_param_set_req is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_tx_ops.send_vdev_sta_ps_param_req(vdev, ps_param,
			param_value);
out:
	pmo_exit();

	return status;
}

void pmo_tgt_psoc_update_wow_bus_suspend_state(struct wlan_objmgr_psoc *psoc,
		uint8_t val)
{
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.psoc_update_wow_bus_suspend) {
		pmo_err("psoc_update_wow_bus_suspend is null");
		return;
	}
	pmo_tx_ops.psoc_update_wow_bus_suspend(psoc, val);
}

int pmo_tgt_psoc_get_host_credits(struct wlan_objmgr_psoc *psoc)
{

	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.psoc_get_host_credits) {
		pmo_err("psoc_get_host_credits is null");
		return 0;
	}

	return pmo_tx_ops.psoc_get_host_credits(psoc);
}

int pmo_tgt_psoc_get_pending_cmnds(struct wlan_objmgr_psoc *psoc)
{

	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.psoc_get_pending_cmnds) {
		pmo_err("psoc_get_pending_cmnds is null");
		return -EAGAIN;
	}

	return pmo_tx_ops.psoc_get_pending_cmnds(psoc);
}

void pmo_tgt_update_target_suspend_flag(struct wlan_objmgr_psoc *psoc,
		uint8_t val)
{
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.update_target_suspend_flag) {
		pmo_err("update_target_suspend_flag is null");
		return;
	}
	pmo_tx_ops.update_target_suspend_flag(psoc, val);
}

void pmo_tgt_update_target_suspend_acked_flag(struct wlan_objmgr_psoc *psoc,
		uint8_t val)
{
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.update_target_suspend_acked_flag) {
		pmo_err("update_target_suspend_acked_flag is null");
		return;
	}
	pmo_tx_ops.update_target_suspend_acked_flag(psoc, val);
}

bool pmo_tgt_is_target_suspended(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.is_target_suspended) {
		pmo_err("is_target_suspended is null");
		return false;
	}
	return pmo_tx_ops.is_target_suspended(psoc);
}

QDF_STATUS pmo_tgt_psoc_send_wow_enable_req(struct wlan_objmgr_psoc *psoc,
	struct pmo_wow_cmd_params *param)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	psoc_ctx = pmo_psoc_get_priv(psoc);
	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);

	if (psoc_ctx->wow.wow_state == pmo_wow_state_legacy_d0) {
		if (!pmo_tx_ops.psoc_send_d0wow_enable_req) {
			pmo_err("psoc_send_d0wow_enable_req is null");
			return QDF_STATUS_E_NULL_VALUE;
		}
		pmo_debug("Sending D0WOW enable command...");
		return pmo_tx_ops.psoc_send_d0wow_enable_req(psoc);
	}

	if (!pmo_tx_ops.psoc_send_wow_enable_req) {
		pmo_err("psoc_send_wow_enable_req is null");
		return QDF_STATUS_E_NULL_VALUE;
	}
	return pmo_tx_ops.psoc_send_wow_enable_req(psoc, param);
}

QDF_STATUS pmo_tgt_psoc_send_supend_req(struct wlan_objmgr_psoc *psoc,
		struct pmo_suspend_params *param)
{
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.psoc_send_supend_req) {
		pmo_err("psoc_send_supend_req is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return pmo_tx_ops.psoc_send_supend_req(psoc, param);
}

QDF_STATUS pmo_tgt_psoc_set_runtime_pm_inprogress(struct wlan_objmgr_psoc *psoc,
						  bool value)
{
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.psoc_set_runtime_pm_in_progress) {
		pmo_err("pmo ops is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pmo_tx_ops.psoc_set_runtime_pm_in_progress(psoc, value);

	return QDF_STATUS_SUCCESS;
}

bool pmo_tgt_psoc_get_runtime_pm_in_progress(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.psoc_get_runtime_pm_in_progress) {
		pmo_err("psoc_get_runtime_pm_in_progress is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return pmo_tx_ops.psoc_get_runtime_pm_in_progress(psoc);
}

QDF_STATUS pmo_tgt_psoc_send_host_wakeup_ind(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	psoc_ctx = pmo_psoc_get_priv(psoc);
	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);

	if (psoc_ctx->psoc_cfg.d0_wow_supported &&
	    psoc_ctx->wow.wow_state == pmo_wow_state_legacy_d0) {
		if (!pmo_tx_ops.psoc_send_d0wow_disable_req) {
			pmo_err("psoc_send_d0wow_disable_req is null");
			return QDF_STATUS_E_NULL_VALUE;
		}
		pmo_debug("Sending D0WOW disable command...");
		return pmo_tx_ops.psoc_send_d0wow_disable_req(psoc);
	}

	if (!pmo_tx_ops.psoc_send_host_wakeup_ind) {
		pmo_err("psoc_send_host_wakeup_ind is null");
		return QDF_STATUS_E_NULL_VALUE;
	}
	return pmo_tx_ops.psoc_send_host_wakeup_ind(psoc);
}

QDF_STATUS pmo_tgt_psoc_send_target_resume_req(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.psoc_send_target_resume_req) {
		pmo_err("send_target_resume_req is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return pmo_tx_ops.psoc_send_target_resume_req(psoc);
}

QDF_STATUS pmo_tgt_psoc_send_idle_roam_monitor(struct wlan_objmgr_psoc *psoc,
					       uint8_t val)
{
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.psoc_send_idle_roam_suspend_mode) {
		pmo_err("NULL fp");
		return QDF_STATUS_E_NULL_VALUE;
	}
	return pmo_tx_ops.psoc_send_idle_roam_suspend_mode(psoc, val);
}
