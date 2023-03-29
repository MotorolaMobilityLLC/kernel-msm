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
 * DOC: Define API's for suspend / resume handling
 */

#include "wlan_pmo_wow.h"
#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_main.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"
#include "wlan_pmo_lphb.h"
#include "wlan_pmo_hw_filter.h"
#include "wlan_pmo_suspend_resume.h"
#include "cdp_txrx_ops.h"
#include "cdp_txrx_misc.h"
#include "cdp_txrx_flow_ctrl_legacy.h"
#include "hif.h"
#include "htc_api.h"
#include "wlan_pmo_obj_mgmt_api.h"
#include <wlan_scan_ucfg_api.h>
#include "cds_api.h"
#include "wlan_pmo_static_config.h"
#include "wlan_mlme_ucfg_api.h"
#include "cfg_mlme_sap.h"
#include "cfg_ucfg_api.h"
#include "cdp_txrx_bus.h"

/**
 * pmo_core_get_vdev_dtim_period() - Get vdev dtim period
 * @vdev: objmgr vdev handle
 *
 * Return: Vdev dtim period
 */
static uint8_t pmo_core_get_vdev_dtim_period(struct wlan_objmgr_vdev *vdev)
{
	uint8_t dtim_period = 0;
	struct pmo_psoc_priv_obj *psoc_ctx;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	psoc = pmo_vdev_get_psoc(vdev);

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		if (psoc_ctx->get_dtim_period)
			ret = psoc_ctx->get_dtim_period(pmo_vdev_get_id(vdev),
							&dtim_period);
	}

	if (QDF_IS_STATUS_ERROR(ret))
		pmo_err("Failed to get to dtim period for vdevId %d",
				pmo_vdev_get_id(vdev));

	return dtim_period;
}

/**
 * pmo_core_get_vdev_beacon_interval() - Get vdev beacon interval
 * @vdev: objmgr vdev handle
 *
 * Return: Vdev beacon interval
 */
static uint16_t pmo_core_get_vdev_beacon_interval(struct wlan_objmgr_vdev *vdev)
{
	uint16_t beacon_interval = 0;
	struct pmo_psoc_priv_obj *psoc_ctx;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	psoc = pmo_vdev_get_psoc(vdev);

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		if (psoc_ctx->get_beacon_interval)
			ret = psoc_ctx->get_beacon_interval(
							pmo_vdev_get_id(vdev),
							&beacon_interval);
	}

	if (QDF_IS_STATUS_ERROR(ret))
		pmo_err("Failed to get beacon interval for vdev id %d",
			pmo_vdev_get_id(vdev));

	return beacon_interval;
}

/**
 * pmo_core_calculate_listen_interval() - Calculate vdev listen interval
 * @vdev: objmgr vdev handle
 * @vdev_ctx: pmo vdev priv ctx
 * @listen_interval: listen interval which is computed for vdev
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS pmo_core_calculate_listen_interval(
			struct wlan_objmgr_vdev *vdev,
			struct pmo_vdev_priv_obj *vdev_ctx,
			uint32_t *listen_interval)
{
	uint32_t max_mod_dtim, max_dtim;
	uint32_t beacon_interval_mod;
	struct pmo_psoc_cfg *psoc_cfg = &vdev_ctx->pmo_psoc_ctx->psoc_cfg;
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);

	if (psoc_cfg->sta_dynamic_dtim) {
		*listen_interval = psoc_cfg->sta_dynamic_dtim;
	} else if ((psoc_cfg->sta_mod_dtim) &&
		   (psoc_cfg->sta_max_li_mod_dtim)) {
		/*
		 * When the system is in suspend
		 * (maximum beacon will be at 1s == 10)
		 * If maxModulatedDTIM ((MAX_LI_VAL = 10) / AP_DTIM)
		 * equal or larger than MDTIM
		 * (configured in WCNSS_qcom_cfg.ini)
		 * Set LI to MDTIM * AP_DTIM
		 * If Dtim = 2 and Mdtim = 2 then LI is 4
		 * Else
		 * Set LI to maxModulatedDTIM * AP_DTIM
		 */
		beacon_interval_mod =
			pmo_core_get_vdev_beacon_interval(vdev) / 100;
		if (beacon_interval_mod == 0)
			beacon_interval_mod = 1;

		max_dtim = pmo_core_get_vdev_dtim_period(vdev) *
					beacon_interval_mod;

		if (!max_dtim) {
			pmo_err("Invalid dtim period");
			return QDF_STATUS_E_INVAL;
		}

		max_mod_dtim = psoc_cfg->sta_max_li_mod_dtim / max_dtim;

		if (max_mod_dtim <= 0)
			max_mod_dtim = 1;

		if (max_mod_dtim >= psoc_cfg->sta_mod_dtim) {
			*listen_interval =
				(psoc_cfg->sta_mod_dtim *
				pmo_core_get_vdev_dtim_period(vdev));
		} else {
			*listen_interval =
				(max_mod_dtim *
				pmo_core_get_vdev_dtim_period(vdev));
		}
	} else {
		/* Get Listen Interval */
		if (QDF_IS_STATUS_ERROR(ucfg_mlme_get_listen_interval(psoc,
							    listen_interval))) {
			pmo_err("Failed to get value for listen interval");
			*listen_interval = cfg_default(CFG_LISTEN_INTERVAL);
		}
	}
	return QDF_STATUS_SUCCESS;
}

static void pmo_configure_vdev_suspend_params(
					struct wlan_objmgr_psoc *psoc,
					struct wlan_objmgr_vdev *vdev,
					struct pmo_vdev_priv_obj *vdev_ctx)
{
	QDF_STATUS ret;
	uint8_t vdev_id;
	enum QDF_OPMODE opmode = pmo_core_get_vdev_op_mode(vdev);
	struct pmo_psoc_cfg *psoc_cfg = &vdev_ctx->pmo_psoc_ctx->psoc_cfg;
	uint8_t  ito_repeat_count_value = 0;
	uint32_t non_wow_inactivity_time, wow_inactivity_time;

	pmo_enter();

	vdev_id = pmo_vdev_get_id(vdev);
	if (!PMO_VDEV_IN_STA_MODE(opmode))
		return;
	ret = pmo_tgt_send_vdev_sta_ps_param(vdev,
					pmo_sta_ps_param_inactivity_time,
					psoc_cfg->wow_data_inactivity_timeout);
	if (QDF_IS_STATUS_ERROR(ret)) {
		pmo_debug("Failed to Set wow inactivity timeout vdevId %d",
			  vdev_id);
	}

	non_wow_inactivity_time = psoc_cfg->ps_data_inactivity_timeout;
	wow_inactivity_time = psoc_cfg->wow_data_inactivity_timeout;
	/*
	 * To keep ito repeat count same in wow mode as in non wow mode,
	 * modulating ito repeat count value.
	 */
	ito_repeat_count_value = (non_wow_inactivity_time /
				  wow_inactivity_time) *
					psoc_cfg->ito_repeat_count;
	if (ito_repeat_count_value)
		ret = pmo_tgt_send_vdev_sta_ps_param(vdev,
					pmo_sta_ps_param_ito_repeat_count,
					psoc_cfg->wow_data_inactivity_timeout);
	if (QDF_IS_STATUS_ERROR(ret)) {
		pmo_err("Failed to Set ito repeat count vdevId %d",
			vdev_id);
	}

	pmo_exit();
}

static void pmo_configure_vdev_resume_params(
					struct wlan_objmgr_psoc *psoc,
					struct wlan_objmgr_vdev *vdev,
					struct pmo_vdev_priv_obj *vdev_ctx)
{
	QDF_STATUS ret;
	uint8_t vdev_id;
	enum QDF_OPMODE opmode = pmo_core_get_vdev_op_mode(vdev);
	struct pmo_psoc_cfg *psoc_cfg = &vdev_ctx->pmo_psoc_ctx->psoc_cfg;

	pmo_enter();

	vdev_id = pmo_vdev_get_id(vdev);
	if (!PMO_VDEV_IN_STA_MODE(opmode))
		return;
	ret = pmo_tgt_send_vdev_sta_ps_param(vdev,
					 pmo_sta_ps_param_inactivity_time,
					 psoc_cfg->ps_data_inactivity_timeout);
	if (QDF_IS_STATUS_ERROR(ret)) {
		pmo_debug("Failed to Set inactivity timeout vdevId %d",
			  vdev_id);
	}

	pmo_exit();
}

/**
 * pmo_core_set_vdev_suspend_dtim() - set suspend dtim parameters in fw
 * @psoc: objmgr psoc handle
 * @vdev: objmgr vdev handle
 * @vdev_ctx: pmo vdev priv ctx
 *
 * Return: none
 */
static void pmo_core_set_vdev_suspend_dtim(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_vdev *vdev,
		struct pmo_vdev_priv_obj *vdev_ctx)
{
	QDF_STATUS ret;
	uint8_t vdev_id;
	enum QDF_OPMODE opmode = pmo_core_get_vdev_op_mode(vdev);
	uint32_t listen_interval = cfg_default(CFG_LISTEN_INTERVAL);

	vdev_id = pmo_vdev_get_id(vdev);
	if (PMO_VDEV_IN_STA_MODE(opmode) &&
	    pmo_core_get_vdev_dtim_period(vdev) != 0) {
		/* calculate listen interval */
		ret = pmo_core_calculate_listen_interval(vdev, vdev_ctx,
				&listen_interval);
		if (ret != QDF_STATUS_SUCCESS) {
			/* even it fails continue fwr will take default LI */
			pmo_debug("Fail to calculate listen interval");
		}
		ret = pmo_tgt_vdev_update_param_req(vdev,
					pmo_vdev_param_listen_interval,
					listen_interval);
		if (QDF_IS_STATUS_ERROR(ret)) {
			/* even it fails continue fwr will take default LI */
			pmo_debug("Failed to Set Listen Interval vdevId %d",
				 vdev_id);
		}
		pmo_debug("Set Listen Interval vdevId %d Listen Intv %d",
			  vdev_id, listen_interval);

		pmo_core_vdev_set_restore_dtim(vdev, true);
	}
}

/*
 * pmo_is_listen_interval_user_set() - Check if listen interval is configured
 * by user or not
 * @vdev_ctx: PMO vdev private object
 *
 * Return: true if listen interval is user configured else false
 */
static inline
bool pmo_is_listen_interval_user_set(struct pmo_vdev_priv_obj *vdev_ctx)
{
	bool retval;

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	retval = vdev_ctx->dyn_modulated_dtim_enabled
		 || vdev_ctx->dyn_listen_interval;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return retval;
}

/**
 * pmo_core_set_suspend_dtim() - set suspend dtim
 * @psoc: objmgr psoc handle
 *
 * Return: none
 */
static void pmo_core_set_suspend_dtim(struct wlan_objmgr_psoc *psoc)
{
	uint8_t vdev_id;
	struct wlan_objmgr_vdev *vdev;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct pmo_psoc_priv_obj *psoc_ctx;
	bool li_offload_support = false;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		li_offload_support = psoc_ctx->caps.li_offload;
	}

	if (li_offload_support)
		pmo_debug("listen interval offload support is enabled");

	/* Iterate through VDEV list */
	for (vdev_id = 0; vdev_id < WLAN_UMAC_PSOC_MAX_VDEVS; vdev_id++) {
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
							    WLAN_PMO_ID);
		if (!vdev)
			continue;

		vdev_ctx = pmo_vdev_get_priv(vdev);
		if (!pmo_is_listen_interval_user_set(vdev_ctx)
		    && !li_offload_support)
			pmo_core_set_vdev_suspend_dtim(psoc, vdev, vdev_ctx);
		pmo_configure_vdev_suspend_params(psoc, vdev, vdev_ctx);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
	}
}

/**
 * pmo_core_update_wow_bus_suspend() - set wow bus suspend flag
 * @psoc: objmgr psoc handle
 * @psoc_ctx: pmo psoc priv ctx
 * @val: true for enable else false
 * Return: none
 */
static inline
void pmo_core_update_wow_bus_suspend(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_priv_obj *psoc_ctx, int val)
{
	qdf_spin_lock_bh(&psoc_ctx->lock);
	psoc_ctx->wow.is_wow_bus_suspended = val;
	qdf_spin_unlock_bh(&psoc_ctx->lock);
	pmo_tgt_psoc_update_wow_bus_suspend_state(psoc, val);
}

/* Define for conciseness */
#define BM_LEN PMO_WOW_MAX_EVENT_BM_LEN
#define EV_NLO WOW_NLO_SCAN_COMPLETE_EVENT
#define EV_PWR WOW_CHIP_POWER_FAILURE_DETECT_EVENT

void pmo_core_configure_dynamic_wake_events(struct wlan_objmgr_psoc *psoc)
{
	int vdev_id;
	uint32_t adapter_type;
	uint32_t enable_mask[BM_LEN];
	uint32_t disable_mask[BM_LEN];
	struct wlan_objmgr_vdev *vdev;
	struct pmo_psoc_priv_obj *psoc_ctx;
	bool enable_configured;
	bool disable_configured;

	/* Iterate through VDEV list */
	for (vdev_id = 0; vdev_id < WLAN_UMAC_PSOC_MAX_VDEVS; vdev_id++) {

		enable_configured = false;
		disable_configured = false;

		qdf_mem_zero(enable_mask,  sizeof(uint32_t) * BM_LEN);
		qdf_mem_zero(disable_mask, sizeof(uint32_t) * BM_LEN);

		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
							    WLAN_PMO_ID);
		if (!vdev)
			continue;

		if (ucfg_scan_get_pno_in_progress(vdev)) {
			if (ucfg_scan_get_pno_match(vdev)) {
				pmo_set_wow_event_bitmap(EV_NLO,
							 BM_LEN,
							 enable_mask);
				enable_configured = true;
			} else {
				pmo_set_wow_event_bitmap(EV_NLO,
							 BM_LEN,
							 disable_mask);
				disable_configured = true;
			}
		}

		adapter_type = pmo_get_vdev_opmode(vdev);

		psoc_ctx = pmo_psoc_get_priv(psoc);

		if (psoc_ctx->psoc_cfg.auto_power_save_fail_mode ==
		    PMO_FW_TO_SEND_WOW_IND_ON_PWR_FAILURE &&
		    (adapter_type == QDF_STA_MODE ||
		     adapter_type == QDF_P2P_CLIENT_MODE)) {
			if (psoc_ctx->is_device_in_low_pwr_mode &&
			    psoc_ctx->is_device_in_low_pwr_mode(vdev_id)) {
				pmo_set_wow_event_bitmap(EV_PWR,
							 BM_LEN,
							 enable_mask);
				enable_configured = true;
			}
		}

		if (enable_configured)
			pmo_tgt_enable_wow_wakeup_event(vdev, enable_mask);
		if (disable_configured)
			pmo_tgt_disable_wow_wakeup_event(vdev, disable_mask);

		wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
	}

}

static void pmo_core_enable_runtime_pm_offloads(struct wlan_objmgr_psoc *psoc)
{
	uint8_t vdev_id;
	struct wlan_objmgr_vdev *vdev;

	/* Iterate through VDEV list */
	for (vdev_id = 0; vdev_id < WLAN_UMAC_PSOC_MAX_VDEVS; vdev_id++) {
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
							    WLAN_PMO_ID);
		if (!vdev)
			continue;

		pmo_register_action_frame_patterns(vdev, QDF_RUNTIME_SUSPEND);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
	}
}

static void pmo_core_disable_runtime_pm_offloads(struct wlan_objmgr_psoc *psoc)
{
	uint8_t vdev_id;
	struct wlan_objmgr_vdev *vdev;

	/* Iterate through VDEV list */
	for (vdev_id = 0; vdev_id < WLAN_UMAC_PSOC_MAX_VDEVS; vdev_id++) {
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
							    WLAN_PMO_ID);
		if (!vdev)
			continue;

		pmo_clear_action_frame_patterns(vdev);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
	}
}

/**
 * pmo_core_psoc_configure_suspend(): configure suspend req events
 * @psoc: objmgr psoc
 * @is_runtime_pm: indicate if it is used by runtime PM
 *
 * Responsibility of the caller to take the psoc reference.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS pmo_core_psoc_configure_suspend(struct wlan_objmgr_psoc *psoc,
						  bool is_runtime_pm)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_enter();

	psoc_ctx = pmo_psoc_get_priv(psoc);

	if (is_runtime_pm)
		pmo_core_enable_runtime_pm_offloads(psoc);

	if (pmo_core_is_wow_applicable(psoc)) {
		pmo_debug("WOW Suspend");
		pmo_core_apply_lphb(psoc);
		/*
		 * Dynamic wake events should not be needed for runtime PM.
		 * Any wake events can be configed by default if they are
		 * really needed for runtime PM. In fact, most of them are
		 * only needed for system suspend.
		 */
		if (!is_runtime_pm)
			pmo_core_configure_dynamic_wake_events(psoc);
		pmo_core_update_wow_enable(psoc_ctx, true);
		pmo_core_update_wow_enable_cmd_sent(psoc_ctx, false);
	} else {
		pmo_debug("Non WOW PDEV Suspend");
		pmo_core_update_wow_enable(psoc_ctx, false);
	}

	/*
	 * For runtime PM, since system is awake, DTIM related commands
	 * do not have to be sent with WOW sequence. They can be sent
	 * through other paths which will just trigger a runtime resume.
	 */
	if (!is_runtime_pm)
		pmo_core_set_suspend_dtim(psoc);

	/*
	 * To handle race between hif_pci_suspend and unpause/pause tx handler.
	 * This happens when host sending WMI_WOW_ENABLE_CMDID to FW and receive
	 * WMI_TX_PAUSE_EVENT with ACTON_UNPAUSE almost at same time.
	 */
	pmo_core_update_wow_bus_suspend(psoc, psoc_ctx, true);

	pmo_exit();

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_core_psoc_user_space_suspend_req(struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type)
{
	QDF_STATUS status;

	pmo_enter();

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		goto out;
	}

	status = pmo_core_psoc_configure_suspend(psoc, false);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to configure suspend");

	pmo_psoc_put_ref(psoc);
out:
	pmo_exit();

	return status;
}

/**
 * pmo_core_set_vdev_resume_dtim() - set resume dtim parameters in fw
 * @psoc: objmgr psoc handle
 * @vdev: objmgr vdev handle
 * @vdev_ctx: pmo vdev priv ctx
 *
 * Return: none
 */
static void pmo_core_set_vdev_resume_dtim(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_vdev *vdev,
		struct pmo_vdev_priv_obj *vdev_ctx)
{
	QDF_STATUS ret;
	uint8_t vdev_id;
	enum QDF_OPMODE opmode = pmo_core_get_vdev_op_mode(vdev);
	int32_t cfg_data_val = 0;

	vdev_id = pmo_vdev_get_id(vdev);
	if ((PMO_VDEV_IN_STA_MODE(opmode)) &&
	    (pmo_core_vdev_get_restore_dtim(vdev))) {
		/* Get Listen Interval */
		if (QDF_IS_STATUS_ERROR(ucfg_mlme_get_listen_interval(psoc,
							      &cfg_data_val))) {
			pmo_err("Failed to get value for listen interval");
			cfg_data_val = cfg_default(CFG_LISTEN_INTERVAL);
		}

		ret = pmo_tgt_vdev_update_param_req(vdev,
				pmo_vdev_param_listen_interval, cfg_data_val);
		if (QDF_IS_STATUS_ERROR(ret)) {
			/* Even it fails continue Fw will take default LI */
			pmo_err("Failed to Set Listen Interval vdevId %d",
				 vdev_id);
		}
		pmo_debug("Set Listen Interval vdevId %d Listen Intv %d",
			  vdev_id, cfg_data_val);
		pmo_core_vdev_set_restore_dtim(vdev, false);
	}
}

/**
 * pmo_core_set_resume_dtim() - set resume time dtim
 * @psoc: objmgr psoc handle
 *
 * Return: none
 */
static void pmo_core_set_resume_dtim(struct wlan_objmgr_psoc *psoc)
{
	uint8_t vdev_id;
	struct wlan_objmgr_vdev *vdev;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct pmo_psoc_priv_obj *psoc_ctx;
	bool li_offload_support = false;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		li_offload_support = psoc_ctx->caps.li_offload;
	}

	if (li_offload_support)
		pmo_debug("listen interval offload support is enabled");

	/* Iterate through VDEV list */
	for (vdev_id = 0; vdev_id < WLAN_UMAC_PSOC_MAX_VDEVS; vdev_id++) {
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
							    WLAN_PMO_ID);
		if (!vdev)
			continue;

		vdev_ctx = pmo_vdev_get_priv(vdev);
		if (!pmo_is_listen_interval_user_set(vdev_ctx)
		    && !li_offload_support)
			pmo_core_set_vdev_resume_dtim(psoc, vdev, vdev_ctx);
		pmo_configure_vdev_resume_params(psoc, vdev, vdev_ctx);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
	}
}

#if defined(QCA_LL_LEGACY_TX_FLOW_CONTROL) || defined(QCA_LL_TX_FLOW_CONTROL_V2)
/**
 * pmo_unpause_vdev - unpause all vdev
 * @psoc: objmgr psoc handle
 *
 * unpause all vdev aftter resume/coming out of wow mode
 *
 * Return: none
 */
static void pmo_unpause_all_vdev(struct wlan_objmgr_psoc *psoc,
				 struct pmo_psoc_priv_obj *psoc_ctx)
{
	uint8_t vdev_id;

	/* Iterate through VDEV list */
	for (vdev_id = 0; vdev_id < WLAN_UMAC_PSOC_MAX_VDEVS; vdev_id++) {
		/* When host resumes, by default unpause all active vdev */
		if (pmo_core_vdev_get_pause_bitmap(psoc_ctx, vdev_id)) {
			cdp_fc_vdev_unpause(pmo_core_psoc_get_dp_handle(psoc),
					    vdev_id,
					    0xffffffff, 0);
			if (psoc_ctx->pause_bitmap_notifier)
				psoc_ctx->pause_bitmap_notifier(vdev_id, 0);
		}
	}
}
#else
static inline void pmo_unpause_all_vdev(struct wlan_objmgr_psoc *psoc,
					struct pmo_psoc_priv_obj *psoc_ctx)
{
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

/**
 * pmo_core_psoc_configure_resume(): configure events after bus resume
 * @psoc: objmgr psoc
 * @is_runtime_pm: indicate if it is used by runtime PM
 *
 * Responsibility of the caller to take the psoc reference.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS pmo_core_psoc_configure_resume(struct wlan_objmgr_psoc *psoc,
						 bool is_runtime_pm)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_enter();

	psoc_ctx = pmo_psoc_get_priv(psoc);
	if (is_runtime_pm)
		pmo_core_disable_runtime_pm_offloads(psoc);

	/*
	 * For runtime PM, since system is awake, DTIM related commands
	 * do not have to be sent with WOW sequence. They can be sent
	 * through other paths which will just trigger a runtime resume.
	 */
	if (!is_runtime_pm)
		pmo_core_set_resume_dtim(psoc);
	pmo_core_update_wow_bus_suspend(psoc, psoc_ctx, false);
	pmo_unpause_all_vdev(psoc, psoc_ctx);

	pmo_exit();

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_core_psoc_user_space_resume_req(struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_enter();

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		goto out;
	}

	status = pmo_core_psoc_configure_resume(psoc, false);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to configure resume");

	pmo_psoc_put_ref(psoc);
out:
	pmo_exit();

	return status;
}

/**
 * pmo_core_enable_wow_in_fw() - enable wow in fw
 * @psoc: objmgr psoc handle
 * @psoc_ctx: pmo psoc private ctx
 * @wow_params: collection of wow enable override parameters
 * @type: type of wow suspend
 *
 * Return: QDF status
 */
static QDF_STATUS
pmo_core_enable_wow_in_fw(struct wlan_objmgr_psoc *psoc,
			  struct pmo_psoc_priv_obj *psoc_ctx,
			  struct pmo_wow_enable_params *wow_params,
			  enum qdf_suspend_type type)
{
	int host_credits, wmi_pending_cmds;
	struct pmo_wow_cmd_params param = {0};
	struct pmo_psoc_cfg *psoc_cfg = &psoc_ctx->psoc_cfg;
	QDF_STATUS status;
	void *hif_ctx;

	pmo_enter();

	hif_ctx = pmo_core_psoc_get_hif_handle(psoc);
	qdf_event_reset(&psoc_ctx->wow.target_suspend);
	pmo_core_set_wow_nack(psoc_ctx, false);
	host_credits = pmo_tgt_psoc_get_host_credits(psoc);
	wmi_pending_cmds = pmo_tgt_psoc_get_pending_cmnds(psoc);
	pmo_debug("Credits:%d; Pending_Cmds: %d",
		host_credits, wmi_pending_cmds);

	param.enable = true;
	if (wow_params->is_unit_test)
		param.flags = WMI_WOW_FLAG_UNIT_TEST_ENABLE;

	switch (wow_params->interface_pause) {
	default:
		pmo_err("Invalid interface pause setting: %d",
			 wow_params->interface_pause);
		/* intentional to default */
		/* fallthrough */
	case PMO_WOW_INTERFACE_PAUSE_DEFAULT:
		param.can_suspend_link =
			htc_can_suspend_link(
				pmo_core_psoc_get_htc_handle(psoc));
		break;
	case PMO_WOW_INTERFACE_PAUSE_ENABLE:
		param.can_suspend_link = true;
		break;
	case PMO_WOW_INTERFACE_PAUSE_DISABLE:
		param.can_suspend_link = false;
		break;
	}

	switch (wow_params->resume_trigger) {
	default:
		pmo_err("Invalid resume trigger setting: %d",
			 wow_params->resume_trigger);
		/* intentional fall-through to default */
	case PMO_WOW_RESUME_TRIGGER_DEFAULT:
	case PMO_WOW_RESUME_TRIGGER_GPIO:
		/*
		 * GPIO is currently implicit. This means you can't actually
		 * force GPIO if a platform's default wake trigger is HTC wakeup
		 */
		break;
	case PMO_WOW_RESUME_TRIGGER_HTC_WAKEUP:
		param.flags |= WMI_WOW_FLAG_DO_HTC_WAKEUP;
		break;
	}

	if (psoc_ctx->psoc_cfg.d0_wow_supported &&
	    !psoc_ctx->caps.unified_wow &&
	    !param.can_suspend_link) {
		psoc_ctx->wow.wow_state = pmo_wow_state_legacy_d0;
	} else if (param.can_suspend_link) {
		psoc_ctx->wow.wow_state = pmo_wow_state_unified_d3;
	} else {
		psoc_ctx->wow.wow_state = pmo_wow_state_unified_d0;
	}

	if (htc_can_suspend_link(pmo_core_psoc_get_htc_handle(psoc))) {
		if (qdf_is_drv_connected()) {
			pmo_info("drv wow is enabled");
			param.flags |= WMI_WOW_FLAG_ENABLE_DRV_PCIE_L1SS_SLEEP;
		} else {
			pmo_info("non-drv wow is enabled");
		}
	} else {
		pmo_info("Prevent link down, non-drv wow is enabled");
		if (hif_ctx)
			hif_print_runtime_pm_prevent_list(hif_ctx);
	}

	if (type == QDF_SYSTEM_SUSPEND) {
		pmo_info("system suspend wow");
		param.flags |= WMI_WOW_FLAG_SYSTEM_SUSPEND_WOW;
	} else {
		pmo_info("RTPM wow");
	}

	if (psoc_cfg->is_mod_dtim_on_sys_suspend_enabled) {
		pmo_info("mod DTIM enabled");
		param.flags |= WMI_WOW_FLAG_MOD_DTIM_ON_SYS_SUSPEND;
	}

	if (psoc_cfg->sta_forced_dtim) {
		pmo_info("forced DTIM enabled");
		param.flags |= WMI_WOW_FLAG_FORCED_DTIM_ON_SYS_SUSPEND;
	}
	status = pmo_tgt_psoc_send_wow_enable_req(psoc, &param);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("Failed to enable wow in fw");
		goto out;
	}

	pmo_tgt_update_target_suspend_flag(psoc, true);

	status = qdf_wait_for_event_completion(&psoc_ctx->wow.target_suspend,
					       PMO_TARGET_SUSPEND_TIMEOUT);
	if (QDF_IS_STATUS_ERROR(status)) {
		pmo_err("Failed to receive WoW Enable Ack from FW");
		pmo_err("Credits:%d; Pending_Cmds: %d",
			pmo_tgt_psoc_get_host_credits(psoc),
			pmo_tgt_psoc_get_pending_cmnds(psoc));
		pmo_tgt_update_target_suspend_flag(psoc, false);
		if (!psoc_ctx->wow.target_suspend.force_set)
			qdf_trigger_self_recovery(psoc, QDF_SUSPEND_TIMEOUT);
		goto out;
	}

	if (pmo_core_get_wow_nack(psoc_ctx)) {
		pmo_err("FW not ready to WOW");
		pmo_tgt_update_target_suspend_flag(psoc, false);
		status = QDF_STATUS_E_AGAIN;
		goto out;
	}

	pmo_tgt_update_target_suspend_acked_flag(psoc, true);

	host_credits = pmo_tgt_psoc_get_host_credits(psoc);
	wmi_pending_cmds = pmo_tgt_psoc_get_pending_cmnds(psoc);

	if (host_credits < PMO_WOW_REQUIRED_CREDITS) {
		pmo_err("No Credits after HTC ACK:%d, pending_cmds:%d,"
			 "cannot resume back", host_credits, wmi_pending_cmds);
		htc_dump_counter_info(pmo_core_psoc_get_htc_handle(psoc));
		qdf_trigger_self_recovery(psoc, QDF_SUSPEND_NO_CREDIT);
	}
	pmo_debug("WOW enabled successfully in fw: credits:%d pending_cmds: %d",
		host_credits, wmi_pending_cmds);

	hif_latency_detect_timer_stop(pmo_core_psoc_get_hif_handle(psoc));

	pmo_core_update_wow_enable_cmd_sent(psoc_ctx, true);

out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_psoc_suspend_target(struct wlan_objmgr_psoc *psoc,
					int disable_target_intr)
{
	QDF_STATUS status;
	struct pmo_suspend_params param;
	struct pmo_psoc_priv_obj *psoc_ctx;
	void *dp_soc = pmo_core_psoc_get_dp_handle(psoc);

	pmo_enter();

	psoc_ctx = pmo_psoc_get_priv(psoc);

	cdp_process_target_suspend_req(dp_soc, OL_TXRX_PDEV_ID);
	qdf_event_reset(&psoc_ctx->wow.target_suspend);
	param.disable_target_intr = disable_target_intr;
	status = pmo_tgt_psoc_send_supend_req(psoc, &param);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	pmo_tgt_update_target_suspend_flag(psoc, true);

	status = qdf_wait_for_event_completion(&psoc_ctx->wow.target_suspend,
					       PMO_TARGET_SUSPEND_TIMEOUT);
	if (QDF_IS_STATUS_ERROR(status)) {
		pmo_err("Failed to get ACK from firmware for pdev suspend");
		pmo_tgt_update_target_suspend_flag(psoc, false);
		if (!psoc_ctx->wow.target_suspend.force_set)
			qdf_trigger_self_recovery(psoc, QDF_SUSPEND_TIMEOUT);
	} else {
		pmo_tgt_update_target_suspend_acked_flag(psoc, true);
	}

out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_psoc_bus_suspend_req(struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type,
		struct pmo_wow_enable_params *wow_params)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;
	bool wow_mode_selected = false;
	qdf_time_t begin, end;

	pmo_enter();
	if (!psoc) {
		pmo_err("psoc is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	if (!wow_params) {
		pmo_err("wow_params is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		goto out;
	}

	psoc_ctx = pmo_psoc_get_priv(psoc);

	wow_mode_selected = pmo_core_is_wow_enabled(psoc_ctx);
	pmo_debug("wow mode selected %d", wow_mode_selected);

	begin = qdf_get_log_timestamp_usecs();
	if (wow_mode_selected)
		status = pmo_core_enable_wow_in_fw(psoc, psoc_ctx,
						   wow_params,
						   type);
	else
		status = pmo_core_psoc_suspend_target(psoc, 0);
	end = qdf_get_log_timestamp_usecs();
	pmo_debug("fw took total time %lu microseconds to enable wow",
		  end - begin);

	pmo_psoc_put_ref(psoc);
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_txrx_suspend(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_ctx;
	QDF_STATUS status;
	void *hif_ctx;
	void *dp_soc;
	int ret;

	status = pmo_psoc_get_ref(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_ctx = pmo_psoc_get_priv(psoc);
	if (pmo_core_get_wow_state(pmo_ctx) != pmo_wow_state_unified_d3)
		goto out;

	hif_ctx = pmo_core_psoc_get_hif_handle(psoc);
	dp_soc = pmo_core_psoc_get_dp_handle(psoc);
	if (!hif_ctx || !dp_soc) {
		pmo_err("Invalid ctx hif: %pK, dp: %pK", hif_ctx, dp_soc);
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	ret = hif_disable_grp_irqs(hif_ctx);
	if (ret && ret != -EOPNOTSUPP) {
		pmo_err("Prevent suspend, failed to disable grp irqs: %d", ret);
		status =  qdf_status_from_os_return(ret);
		goto out;
	}

	if (ret == -EOPNOTSUPP)
		goto out;

	cdp_drain_txrx(dp_soc);
	pmo_ctx->wow.txrx_suspended = true;
out:
	pmo_psoc_put_ref(psoc);
	return status;
}

QDF_STATUS pmo_core_txrx_resume(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_ctx;
	QDF_STATUS status;
	void *hif_ctx;
	int ret;

	status = pmo_psoc_get_ref(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		pmo_err("pmo cannot get the reference out of psoc");
		return status;
	}

	pmo_ctx = pmo_psoc_get_priv(psoc);
	if (!pmo_ctx->wow.txrx_suspended)
		goto out;

	hif_ctx = pmo_core_psoc_get_hif_handle(psoc);
	if (!hif_ctx) {
		pmo_err("Invalid hif ctx");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	ret = hif_enable_grp_irqs(hif_ctx);
	if (ret && ret != -EOPNOTSUPP) {
		pmo_err("Failed to enable grp irqs: %d", ret);
		status = qdf_status_from_os_return(ret);
		goto out;
	}

	pmo_ctx->wow.txrx_suspended = false;
out:
	pmo_psoc_put_ref(psoc);
	return status;
}

#ifdef FEATURE_RUNTIME_PM
#define PMO_CORE_PSOC_RUNTIME_PM_QDF_BUG(__condition) ({ \
	typeof(__condition) condition = __condition; \
	if (condition && !qdf_is_fw_down()) \
		QDF_BUG(0); \
})

QDF_STATUS pmo_core_psoc_bus_runtime_suspend(struct wlan_objmgr_psoc *psoc,
					     pmo_pld_auto_suspend_cb pld_cb)
{
	void *hif_ctx;
	void *dp_soc;
	uint8_t pdev_id;
	void *htc_ctx;
	QDF_STATUS status;
	int ret;
	struct pmo_wow_enable_params wow_params = {0};
	struct pmo_psoc_priv_obj *psoc_ctx;
	qdf_time_t begin, end;
	int pending;

	pmo_enter();

	if (!psoc) {
		pmo_err("psoc is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		goto out;
	}

	hif_ctx = pmo_core_psoc_get_hif_handle(psoc);
	dp_soc = pmo_core_psoc_get_dp_handle(psoc);
	pdev_id = pmo_core_psoc_get_txrx_handle(psoc);
	htc_ctx = pmo_core_psoc_get_htc_handle(psoc);
	if (!hif_ctx || !dp_soc || !htc_ctx ||
	    pdev_id == OL_TXRX_INVALID_PDEV_ID) {
		pmo_err("Invalid hif: %pK, dp: %pK, pdev_id: %d, htc: %pK",
			hif_ctx, dp_soc, pdev_id, htc_ctx);
		status = QDF_STATUS_E_INVAL;
		goto dec_psoc_ref;
	}

	wow_params.interface_pause = PMO_WOW_INTERFACE_PAUSE_ENABLE;
	wow_params.resume_trigger = PMO_WOW_RESUME_TRIGGER_GPIO;

	ret = hif_pre_runtime_suspend(hif_ctx);
	if (ret) {
		status = qdf_status_from_os_return(ret);
		goto runtime_failure;
	}

	status = cdp_runtime_suspend(dp_soc, pdev_id);
	if (status != QDF_STATUS_SUCCESS)
		goto runtime_failure;

	ret = htc_runtime_suspend(htc_ctx);
	if (ret) {
		status = qdf_status_from_os_return(ret);
		goto cdp_runtime_resume;
	}

	status = pmo_tgt_psoc_set_runtime_pm_inprogress(psoc, true);
	if (status != QDF_STATUS_SUCCESS)
		goto resume_htc;

	status = pmo_core_psoc_configure_suspend(psoc, true);
	if (status != QDF_STATUS_SUCCESS)
		goto resume_htc;

	hif_pm_set_link_state(hif_ctx, HIF_PM_LINK_STATE_DOWN);

	status = pmo_core_psoc_bus_suspend_req(psoc, QDF_RUNTIME_SUSPEND,
					       &wow_params);
	if (status != QDF_STATUS_SUCCESS)
		goto pmo_resume_configure;

	ret = hif_runtime_suspend(hif_ctx);
	if (ret) {
		status = qdf_status_from_os_return(ret);
		goto pmo_bus_resume;
	}

	status = pmo_core_txrx_suspend(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto resume_hif;

	pending = cdp_rx_get_pending(cds_get_context(QDF_MODULE_ID_SOC));
	if (pending) {
		pmo_debug("Prevent suspend, RX frame pending %d", pending);
		status = QDF_STATUS_E_BUSY;
		goto resume_txrx;
	}

	if (pld_cb) {
		begin = qdf_get_log_timestamp_usecs();
		ret = pld_cb();
		end = qdf_get_log_timestamp_usecs();
		pmo_debug("runtime pci bus suspend took total time %lu microseconds",
			  end - begin);

		if (ret) {
			status = qdf_status_from_os_return(ret);
			goto resume_txrx;
		}
	}

	if (hif_pm_get_wake_irq_type(hif_ctx) == HIF_PM_CE_WAKE) {
		/*
		 * In moselle, there is no separate interrupt for wake_irq,
		 * shares CE interrupt, there is a chance of wow wakeup
		 * while suspend is in-progress, so handling such scenario
		 */
		hif_pm_runtime_suspend_lock(hif_ctx);
		psoc_ctx = pmo_psoc_get_priv(psoc);
		if (pmo_core_get_wow_initial_wake_up(psoc_ctx)) {
			hif_pm_runtime_suspend_unlock(hif_ctx);
			pmo_err("Target wake up received before suspend completion");
			status = QDF_STATUS_E_BUSY;
			goto resume_txrx;
		}
		hif_process_runtime_suspend_success(hif_ctx);
		hif_pm_runtime_suspend_unlock(hif_ctx);
	} else {
		hif_process_runtime_suspend_success(hif_ctx);
	}

	if (hif_try_prevent_ep_vote_access(hif_ctx)) {
		pmo_debug("Prevent suspend, ep work pending");
		status = QDF_STATUS_E_BUSY;
		goto resume_txrx;
	}

	goto dec_psoc_ref;

resume_txrx:
	PMO_CORE_PSOC_RUNTIME_PM_QDF_BUG(QDF_STATUS_SUCCESS !=
		pmo_core_txrx_resume(psoc));

resume_hif:
	PMO_CORE_PSOC_RUNTIME_PM_QDF_BUG(hif_runtime_resume(hif_ctx));

pmo_bus_resume:
	PMO_CORE_PSOC_RUNTIME_PM_QDF_BUG(QDF_STATUS_SUCCESS !=
		pmo_core_psoc_bus_resume_req(psoc, QDF_RUNTIME_SUSPEND));

pmo_resume_configure:
	PMO_CORE_PSOC_RUNTIME_PM_QDF_BUG(QDF_STATUS_SUCCESS !=
		pmo_core_psoc_configure_resume(psoc, true));

	hif_pm_set_link_state(hif_ctx, HIF_PM_LINK_STATE_UP);

resume_htc:
	PMO_CORE_PSOC_RUNTIME_PM_QDF_BUG(QDF_STATUS_SUCCESS !=
		pmo_tgt_psoc_set_runtime_pm_inprogress(psoc, false));

cdp_runtime_resume:
	PMO_CORE_PSOC_RUNTIME_PM_QDF_BUG(QDF_STATUS_SUCCESS !=
		cdp_runtime_resume(dp_soc, pdev_id));

runtime_failure:
	hif_process_runtime_suspend_failure(hif_ctx);

/* always make sure HTC queue kicker is at the end, so if any
 * cmd is pending during suspending, it can re-trigger if suspend
 * failure.
 */
PMO_CORE_PSOC_RUNTIME_PM_QDF_BUG(htc_runtime_resume(htc_ctx));

dec_psoc_ref:
	pmo_psoc_put_ref(psoc);

out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_psoc_bus_runtime_resume(struct wlan_objmgr_psoc *psoc,
					    pmo_pld_auto_resume_cb pld_cb)
{
	int ret;
	void *hif_ctx;
	void *dp_soc;
	uint8_t pdev_id;
	void *htc_ctx;
	QDF_STATUS status;
	qdf_time_t begin, end;

	pmo_enter();

	if (!psoc) {
		pmo_err("psoc is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		goto out;
	}

	hif_ctx = pmo_core_psoc_get_hif_handle(psoc);
	dp_soc = pmo_core_psoc_get_dp_handle(psoc);
	pdev_id = pmo_core_psoc_get_txrx_handle(psoc);
	htc_ctx = pmo_core_psoc_get_htc_handle(psoc);
	if (!hif_ctx || !dp_soc || !htc_ctx ||
	    pdev_id == OL_TXRX_INVALID_PDEV_ID) {
		pmo_err("Invalid hif: %pK, dp: %pK, pdev_id: %d, htc: %pK",
			hif_ctx, dp_soc, pdev_id, htc_ctx);
		status = QDF_STATUS_E_INVAL;
		goto dec_psoc_ref;
	}

	hif_pre_runtime_resume(hif_ctx);
	if (pld_cb) {
		begin = qdf_get_log_timestamp_usecs();
		ret = pld_cb();
		end = qdf_get_log_timestamp_usecs();
		pmo_debug("pci bus resume took total time %lu microseconds",
			  end - begin);
		if (ret) {
			status = QDF_STATUS_E_FAILURE;
			goto fail;
		}
	}

	if (hif_runtime_resume(hif_ctx)) {
		status = QDF_STATUS_E_FAILURE;
		goto fail;
	}

	status = pmo_core_psoc_bus_resume_req(psoc, QDF_RUNTIME_SUSPEND);
	if (status != QDF_STATUS_SUCCESS)
		goto fail;

	status = pmo_core_txrx_resume(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto fail;

	hif_pm_set_link_state(hif_ctx, HIF_PM_LINK_STATE_UP);

	status = pmo_core_psoc_configure_resume(psoc, true);
	if (status != QDF_STATUS_SUCCESS)
		goto fail;

	status = pmo_tgt_psoc_set_runtime_pm_inprogress(psoc, false);
	if (status != QDF_STATUS_SUCCESS)
		goto fail;

	hif_process_runtime_resume_success(hif_ctx);

	if (htc_runtime_resume(htc_ctx)) {
		status = QDF_STATUS_E_FAILURE;
		goto fail;
	}

	status = cdp_runtime_resume(dp_soc, pdev_id);
	if (status != QDF_STATUS_SUCCESS)
		goto fail;

fail:
	if (status != QDF_STATUS_SUCCESS)
		qdf_trigger_self_recovery(psoc, QDF_RESUME_TIMEOUT);

dec_psoc_ref:
	pmo_psoc_put_ref(psoc);

out:
	pmo_exit();

	return status;
}
#endif

/**
 * pmo_core_psoc_send_host_wakeup_ind_to_fw() - send wakeup ind to fw
 * @psoc: objmgr psoc handle
 * @psoc_ctx: pmo psoc private context
 *
 * Sends host wakeup indication to FW. On receiving this indication,
 * FW will come out of WOW.
 *
 * Return: QDF status
 */
static
QDF_STATUS pmo_core_psoc_send_host_wakeup_ind_to_fw(
			struct wlan_objmgr_psoc *psoc,
			struct pmo_psoc_priv_obj *psoc_ctx)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	void *hif_ctx;

	pmo_enter();

	hif_ctx = pmo_core_psoc_get_hif_handle(psoc);

	hif_set_ep_intermediate_vote_access(hif_ctx);

	qdf_event_reset(&psoc_ctx->wow.target_resume);

	status = pmo_tgt_psoc_send_host_wakeup_ind(psoc);
	if (status) {
		hif_set_ep_vote_access(hif_ctx,
				       HIF_EP_VOTE_NONDP_ACCESS,
				       HIF_EP_VOTE_ACCESS_DISABLE);
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}
	pmo_info("Host wakeup indication sent to fw");

	status = qdf_wait_for_event_completion(&psoc_ctx->wow.target_resume,
					PMO_RESUME_TIMEOUT);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("Timeout waiting for resume event from FW");
		pmo_err("Pending commands %d credits %d",
			pmo_tgt_psoc_get_pending_cmnds(psoc),
			pmo_tgt_psoc_get_host_credits(psoc));

		if (!psoc_ctx->wow.target_resume.force_set)
			qdf_trigger_self_recovery(psoc, QDF_RESUME_TIMEOUT);
	} else {
		pmo_debug("Host wakeup received");
		pmo_tgt_update_target_suspend_flag(psoc, false);
		pmo_tgt_update_target_suspend_acked_flag(psoc, false);
		hif_set_ep_vote_access(hif_ctx,
				       HIF_EP_VOTE_NONDP_ACCESS,
				       HIF_EP_VOTE_ACCESS_ENABLE);
		hif_set_ep_vote_access(hif_ctx,
				       HIF_EP_VOTE_DP_ACCESS,
				       HIF_EP_VOTE_ACCESS_ENABLE);
	}
out:
	pmo_exit();

	return status;
}

/**
 * pmo_core_psoc_disable_wow_in_fw() -  Disable wow in bus resume context.
 * @psoc: objmgr psoc handle
 * @psoc_ctx: pmo psoc private context
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static
QDF_STATUS pmo_core_psoc_disable_wow_in_fw(struct wlan_objmgr_psoc *psoc,
			struct pmo_psoc_priv_obj *psoc_ctx)
{
	QDF_STATUS ret;

	pmo_enter();
	ret = pmo_core_psoc_send_host_wakeup_ind_to_fw(psoc, psoc_ctx);
	if (ret != QDF_STATUS_SUCCESS)
		goto out;

	pmo_core_update_wow_enable_cmd_sent(psoc_ctx, false);

	/* To allow the tx pause/unpause events */
	pmo_core_update_wow_bus_suspend(psoc, psoc_ctx, false);
	/* Unpause the vdev as we are resuming */
	pmo_unpause_all_vdev(psoc, psoc_ctx);
out:
	pmo_exit();

	return ret;
}

/**
 * pmo_core_psoc_resume_target() - resume target
 * @psoc: objmgr psoc handle
 * @psoc_ctx: pmo psoc private context
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static
QDF_STATUS pmo_core_psoc_resume_target(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_priv_obj *psoc_ctx)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_enter();
	qdf_event_reset(&psoc_ctx->wow.target_resume);

	status = pmo_tgt_psoc_send_target_resume_req(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	status = qdf_wait_single_event(&psoc_ctx->wow.target_resume,
			PMO_RESUME_TIMEOUT);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_fatal("Timeout waiting for resume event from FW");
		pmo_fatal("Pending commands %d credits %d",
			pmo_tgt_psoc_get_pending_cmnds(psoc),
			pmo_tgt_psoc_get_host_credits(psoc));
		if (!psoc_ctx->wow.target_resume.force_set)
			qdf_trigger_self_recovery(psoc, QDF_RESUME_TIMEOUT);
	} else {
		pmo_debug("Host wakeup received");
		pmo_tgt_update_target_suspend_flag(psoc, false);
		pmo_tgt_update_target_suspend_acked_flag(psoc, false);
	}
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_psoc_bus_resume_req(struct wlan_objmgr_psoc *psoc,
		enum qdf_suspend_type type)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	bool wow_mode;
	QDF_STATUS status;
	qdf_time_t begin, end;

	pmo_enter();
	if (!psoc) {
		pmo_err("psoc is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		goto out;
	}

	hif_latency_detect_timer_start(pmo_core_psoc_get_hif_handle(psoc));

	psoc_ctx = pmo_psoc_get_priv(psoc);
	wow_mode = pmo_core_is_wow_enabled(psoc_ctx);
	pmo_debug("wow mode %d", wow_mode);

	pmo_core_update_wow_initial_wake_up(psoc_ctx, 0);

	/* If target was not suspended, bail out */
	if (qdf_is_fw_down() || !pmo_tgt_is_target_suspended(psoc)) {
		pmo_psoc_put_ref(psoc);
		status = QDF_STATUS_E_AGAIN;
		goto out;
	}

	begin = qdf_get_log_timestamp_usecs();
	if (wow_mode)
		status = pmo_core_psoc_disable_wow_in_fw(psoc, psoc_ctx);
	else
		status = pmo_core_psoc_resume_target(psoc, psoc_ctx);
	end = qdf_get_log_timestamp_usecs();
	pmo_debug("fw took total time %lu microseconds to disable wow",
		  end - begin);

	pmo_psoc_put_ref(psoc);

out:
	pmo_exit();

	return status;
}

void pmo_core_psoc_target_suspend_acknowledge(void *context, bool wow_nack)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)context;
	void *dp_soc = pmo_core_psoc_get_dp_handle(psoc);
	QDF_STATUS status;

	pmo_enter();
	if (!psoc) {
		pmo_err("psoc is null");
		goto out;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("Failed to get psoc reference");
		goto out;
	}

	psoc_ctx = pmo_psoc_get_priv(psoc);

	pmo_core_set_wow_nack(psoc_ctx, wow_nack);
	qdf_event_set(&psoc_ctx->wow.target_suspend);
	if (!pmo_tgt_psoc_get_runtime_pm_in_progress(psoc)) {
		if (wow_nack)
			qdf_wake_lock_timeout_acquire(
				&psoc_ctx->wow.wow_wake_lock,
				PMO_WAKE_LOCK_TIMEOUT);
		else
			cdp_process_wow_ack_rsp(dp_soc, OL_TXRX_PDEV_ID);
	}

	pmo_psoc_put_ref(psoc);
out:
	pmo_exit();
}

void pmo_core_psoc_wakeup_host_event_received(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_enter();
	if (!psoc) {
		pmo_err("psoc is null");
		goto out;
	}

	psoc_ctx = pmo_psoc_get_priv(psoc);
	psoc_ctx->wow.wow_state = pmo_wow_state_none;
	qdf_event_set(&psoc_ctx->wow.target_resume);
out:
	pmo_exit();
}

int pmo_core_psoc_is_target_wake_up_received(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	int ret = 0;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is NULL");
		ret = -EAGAIN;
		goto out;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("Failed to get psoc reference");
		ret = -EAGAIN;
		goto out;
	}

	psoc_ctx = pmo_psoc_get_priv(psoc);
	if (pmo_core_get_wow_initial_wake_up(psoc_ctx)) {
		pmo_err("Target initial wake up received try again");
		ret = -EAGAIN;
	}

	pmo_psoc_put_ref(psoc);
out:
	pmo_exit();

	return ret;
}


int pmo_core_psoc_clear_target_wake_up(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	int ret = 0;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is NULL");
		ret = -EAGAIN;
		goto out;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("Failed to get psoc reference");
		ret = -EAGAIN;
		goto out;
	}

	psoc_ctx = pmo_psoc_get_priv(psoc);
	pmo_core_update_wow_initial_wake_up(psoc_ctx, 0);

	pmo_psoc_put_ref(psoc);
out:
	pmo_exit();

	return ret;
}

void pmo_core_psoc_handle_initial_wake_up(void *cb_ctx)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)cb_ctx;

	pmo_enter();
	if (!psoc) {
		pmo_err("cb ctx/psoc is null");
		goto out;
	}

	psoc_ctx = pmo_psoc_get_priv(psoc);
	pmo_core_update_wow_initial_wake_up(psoc_ctx, 1);

out:
	pmo_exit();
}

QDF_STATUS pmo_core_config_listen_interval(struct wlan_objmgr_vdev *vdev,
					   uint32_t new_li)
{
	QDF_STATUS status;
	uint8_t vdev_id;
	uint32_t listen_interval;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct pmo_psoc_priv_obj *psoc_ctx;
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);

	pmo_enter();

	status = pmo_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto out;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	vdev_id =  pmo_vdev_get_id(vdev);

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	if (vdev_ctx->dyn_listen_interval == new_li) {
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
		status = QDF_STATUS_SUCCESS;
		pmo_debug("Listen Interval(%d) already set for vdev id %d",
			new_li, vdev_id);
		goto dec_ref;
	}

	vdev_ctx->dyn_listen_interval = new_li;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	listen_interval = new_li ? new_li : cfg_default(CFG_LISTEN_INTERVAL);

	if (!new_li) {
		/* Configure default LI as we do on resume */
		pmo_psoc_with_ctx(pmo_vdev_get_psoc(vdev), psoc_ctx) {
			if (QDF_IS_STATUS_ERROR(
				ucfg_mlme_get_listen_interval(psoc,
							   &listen_interval))) {
				pmo_err("Failed to get listen interval");
				listen_interval =
					       cfg_default(CFG_LISTEN_INTERVAL);
			}
		}
	}

	pmo_debug("Set Listen Interval %d for vdevId %d", listen_interval,
			vdev_id);
	status = pmo_tgt_vdev_update_param_req(vdev,
					       pmo_vdev_param_listen_interval,
					       listen_interval);
	if (QDF_IS_STATUS_ERROR(status)) {
		/* even it fails continue fwr will take default LI */
		pmo_err("Failed to Set Listen Interval");
	}

	/* Set it to Normal DTIM */
	status = pmo_tgt_vdev_update_param_req(vdev,
					       pmo_vdev_param_dtim_policy,
					       pmo_normal_dtim);
	if (QDF_IS_STATUS_ERROR(status)) {
		pmo_err("Failed to set Normal DTIM for vdev id %d", vdev_id);
	} else {
		pmo_debug("Set DTIM Policy to Normal for vdev id %d", vdev_id);
		pmo_core_vdev_set_restore_dtim(vdev, true);
	}

dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:
	pmo_exit();

	return status;
}

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
QDF_STATUS
pmo_core_enable_igmp_offload(struct wlan_objmgr_vdev *vdev,
			     struct pmo_igmp_offload_req *pmo_igmp_req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t vdev_id;
	enum QDF_OPMODE op_mode;
	struct pmo_vdev_priv_obj *vdev_ctx;
	uint32_t version_support;

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_INVAL;

	op_mode = pmo_get_vdev_opmode(vdev);
	if (QDF_STA_MODE != op_mode) {
		pmo_debug("igmp offload supported in STA mode");
		return QDF_STATUS_E_INVAL;
	}

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	if (!vdev_ctx->pmo_psoc_ctx->psoc_cfg.igmp_offload_enable) {
		pmo_debug("igmp offload not supported");
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
		return QDF_STATUS_E_NOSUPPORT;
	}
	version_support =
		vdev_ctx->pmo_psoc_ctx->psoc_cfg.igmp_version_support;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_id = pmo_vdev_get_id(vdev);
	pmo_igmp_req->vdev_id = vdev_id;
	pmo_igmp_req->version_support = version_support;
	status = pmo_tgt_send_igmp_offload_req(vdev, pmo_igmp_req);

	return status;
}
#endif

QDF_STATUS pmo_core_config_forced_dtim(struct wlan_objmgr_vdev *vdev,
				       uint32_t dynamic_dtim)
{
	struct pmo_vdev_priv_obj *vdev_ctx;
	uint8_t vdev_id;
	QDF_STATUS status;

	vdev_id = pmo_vdev_get_id(vdev);
	vdev_ctx = pmo_vdev_get_priv(vdev);

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->dyn_modulated_dtim = dynamic_dtim;
	vdev_ctx->dyn_modulated_dtim_enabled = dynamic_dtim >= 1;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	status = pmo_tgt_vdev_update_param_req(vdev,
					       pmo_vdev_param_forced_dtim_count,
					       dynamic_dtim);

	if (QDF_IS_STATUS_ERROR(status)) {
		pmo_err("Failed to set forced DTIM for vdev id %d",
			vdev_id);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
	return status;
}

QDF_STATUS pmo_core_config_modulated_dtim(struct wlan_objmgr_vdev *vdev,
					  uint32_t mod_dtim)
{
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct pmo_psoc_cfg *psoc_cfg;
	bool prev_dtim_enabled;
	uint32_t listen_interval;
	uint32_t beacon_interval_mod;
	uint32_t max_mod_dtim;
	QDF_STATUS status;
	uint8_t vdev_id;
	uint32_t max_dtim;

	pmo_enter();

	status = pmo_vdev_get_ref(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	vdev_id = pmo_vdev_get_id(vdev);
	vdev_ctx = pmo_vdev_get_priv(vdev);
	psoc_cfg = &vdev_ctx->pmo_psoc_ctx->psoc_cfg;

	if (psoc_cfg->sta_forced_dtim)
		return pmo_core_config_forced_dtim(vdev, mod_dtim);

	/* Calculate Maximum allowed modulated DTIM */
	beacon_interval_mod =
		pmo_core_get_vdev_beacon_interval(vdev) / 100;
	if (!beacon_interval_mod)
		beacon_interval_mod = 1;

	max_dtim = (pmo_core_get_vdev_dtim_period(vdev)
		 * beacon_interval_mod);

	if (!max_dtim) {
		pmo_err("Invalid dtim period");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
		return QDF_STATUS_E_INVAL;
	}

	max_mod_dtim = psoc_cfg->sta_max_li_mod_dtim /
		max_dtim;

	if (!max_mod_dtim)
		max_mod_dtim = 1;

	/* Calculate Listen Interval from provided mod DTIM */
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->dyn_modulated_dtim = mod_dtim;
	prev_dtim_enabled = vdev_ctx->dyn_modulated_dtim_enabled;
	vdev_ctx->dyn_modulated_dtim_enabled = mod_dtim != 1;
	if (vdev_ctx->dyn_modulated_dtim > max_mod_dtim) {
		listen_interval = max_mod_dtim *
			pmo_core_get_vdev_dtim_period(vdev);
	} else {
		listen_interval = vdev_ctx->dyn_modulated_dtim  *
			pmo_core_get_vdev_dtim_period(vdev);
	}
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	if (prev_dtim_enabled || mod_dtim != 1) {
		status = pmo_tgt_vdev_update_param_req(vdev,
					pmo_vdev_param_listen_interval,
					listen_interval);
		if (QDF_IS_STATUS_ERROR(status))
			/* even it fails continue fwr will take default LI */
			pmo_err("Failed to set Listen Interval for vdev id %d",
				vdev_id);
		else
			pmo_debug("Set Listen Interval %d for  vdev id %d",
				  listen_interval, vdev_id);

		status = pmo_tgt_vdev_update_param_req(vdev,
				pmo_vdev_param_dtim_policy,
				pmo_normal_dtim);
		if (QDF_IS_STATUS_ERROR(status)) {
			pmo_err("Failed to set Normal DTIM for vdev id %d",
				vdev_id);
		} else {
			pmo_debug("Set DTIM Policy to Normal for vdev id %d",
				  vdev_id);
			pmo_core_vdev_set_restore_dtim(vdev, true);
		}
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:
	pmo_exit();
	return status;
}

#ifdef SYSTEM_PM_CHECK
void pmo_core_system_resume(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status;

	if (!psoc) {
		pmo_err("psoc is NULL");
		return;
	}

	status = pmo_psoc_get_ref(psoc);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("pmo cannot get the reference out of psoc");
		return;
	}

	psoc_ctx = pmo_psoc_get_priv(psoc);

	htc_system_resume(psoc_ctx->htc_hdl);

	pmo_psoc_put_ref(psoc);
}
#endif
