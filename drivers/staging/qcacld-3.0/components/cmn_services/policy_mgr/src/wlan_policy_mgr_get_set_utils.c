/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_policy_mgr_get_set_utils.c
 *
 * WLAN Concurrenct Connection Management APIs
 *
 */

/* Include files */
#include "target_if.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_policy_mgr_i.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_nan_api.h"
#include "nan_public_structs.h"
#include "wlan_reg_services_api.h"
#include "wlan_cm_roam_public_struct.h"
#include "csr_neighbor_roam.h"
#include "wlan_mlme_main.h"
#include "wlan_mlme_vdev_mgr_interface.h"
/* invalid channel id. */
#define INVALID_CHANNEL_ID 0

/**
 * policy_mgr_debug_alert() - fatal error alert
 *
 * This function will flush host drv log and
 * disable all level logs.
 * It can be called in fatal error detected in policy
 * manager.
 * This is to avoid host log overwritten in stress
 * test to help issue debug.
 *
 * Return: none
 */
static void
policy_mgr_debug_alert(void)
{
	int module_id;
	int qdf_print_idx;

	policy_mgr_err("fatal error detected to flush and pause host log");
	qdf_logging_flush_logs();
	qdf_print_idx = qdf_get_pidx();
	for (module_id = 0; module_id < QDF_MODULE_ID_MAX; module_id++)
		qdf_print_set_category_verbose(
					qdf_print_idx,
					module_id, QDF_TRACE_LEVEL_NONE,
					0);
}

QDF_STATUS
policy_mgr_get_allow_mcc_go_diff_bi(struct wlan_objmgr_psoc *psoc,
				    uint8_t *allow_mcc_go_diff_bi)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*allow_mcc_go_diff_bi = pm_ctx->cfg.allow_mcc_go_diff_bi;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_enable_overlap_chnl(struct wlan_objmgr_psoc *psoc,
				   uint8_t *enable_overlap_chnl)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*enable_overlap_chnl = pm_ctx->cfg.enable_overlap_chnl;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_set_dual_mac_feature(struct wlan_objmgr_psoc *psoc,
					   uint8_t dual_mac_feature)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->cfg.dual_mac_feature = dual_mac_feature;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_dual_mac_feature(struct wlan_objmgr_psoc *psoc,
					   uint8_t *dual_mac_feature)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*dual_mac_feature = pm_ctx->cfg.dual_mac_feature;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_force_1x1(struct wlan_objmgr_psoc *psoc,
				    uint8_t *force_1x1)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*force_1x1 = pm_ctx->cfg.is_force_1x1_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_set_sta_sap_scc_on_dfs_chnl(struct wlan_objmgr_psoc *psoc,
				       uint8_t sta_sap_scc_on_dfs_chnl)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->cfg.sta_sap_scc_on_dfs_chnl = sta_sap_scc_on_dfs_chnl;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_sta_sap_scc_on_dfs_chnl(struct wlan_objmgr_psoc *psoc,
				       uint8_t *sta_sap_scc_on_dfs_chnl)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*sta_sap_scc_on_dfs_chnl = pm_ctx->cfg.sta_sap_scc_on_dfs_chnl;

	return QDF_STATUS_SUCCESS;
}

static bool
policy_mgr_update_dfs_master_dynamic_enabled(
	struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool sta_on_5g = false;
	bool sta_on_2g = false;
	uint32_t i;
	bool enable = true;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return true;
	}

	if (!pm_ctx->cfg.sta_sap_scc_on_dfs_chnl) {
		enable = true;
		goto end;
	}
	if (pm_ctx->cfg.sta_sap_scc_on_dfs_chnl ==
	    PM_STA_SAP_ON_DFS_MASTER_MODE_DISABLED) {
		enable = false;
		goto end;
	}
	if (pm_ctx->cfg.sta_sap_scc_on_dfs_chnl !=
	    PM_STA_SAP_ON_DFS_MASTER_MODE_FLEX) {
		policy_mgr_debug("sta_sap_scc_on_dfs_chnl %d unknown",
				 pm_ctx->cfg.sta_sap_scc_on_dfs_chnl);
		enable = true;
		goto end;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (!((pm_conc_connection_list[i].vdev_id != vdev_id) &&
		      pm_conc_connection_list[i].in_use &&
		      (pm_conc_connection_list[i].mode == PM_STA_MODE ||
		       pm_conc_connection_list[i].mode == PM_P2P_CLIENT_MODE)))
			continue;
		if (WLAN_REG_IS_5GHZ_CH_FREQ(pm_conc_connection_list[i].freq))
			sta_on_5g = true;
		else
			sta_on_2g = true;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (policy_mgr_is_hw_dbs_capable(psoc) && !sta_on_5g)
		enable = true;
	else if (!sta_on_5g && !sta_on_2g)
		enable = true;
	else
		enable = false;
end:
	pm_ctx->dynamic_dfs_master_disabled = !enable;
	if (!enable)
		policy_mgr_debug("sta_sap_scc_on_dfs_chnl %d sta_on_2g %d sta_on_5g %d enable %d",
				 pm_ctx->cfg.sta_sap_scc_on_dfs_chnl, sta_on_2g,
				 sta_on_5g, enable);

	return enable;
}

bool
policy_mgr_get_dfs_master_dynamic_enabled(
	struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return true;
	}

	return policy_mgr_update_dfs_master_dynamic_enabled(psoc, vdev_id);
}

bool
policy_mgr_get_can_skip_radar_event(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return false;
	}

	return pm_ctx->dynamic_dfs_master_disabled;
}

QDF_STATUS
policy_mgr_get_sta_sap_scc_lte_coex_chnl(struct wlan_objmgr_psoc *psoc,
					 uint8_t *sta_sap_scc_lte_coex)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*sta_sap_scc_lte_coex = pm_ctx->cfg.sta_sap_scc_on_lte_coex_chnl;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_sap_mandt_chnl(struct wlan_objmgr_psoc *psoc,
					 uint8_t *sap_mandt_chnl)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*sap_mandt_chnl = pm_ctx->cfg.sap_mandatory_chnl_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
policy_mgr_get_indoor_chnl_marking(struct wlan_objmgr_psoc *psoc,
				   uint8_t *indoor_chnl_marking)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*indoor_chnl_marking = pm_ctx->cfg.mark_indoor_chnl_disable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_mcc_scc_switch(struct wlan_objmgr_psoc *psoc,
					      uint8_t *mcc_scc_switch)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*mcc_scc_switch = pm_ctx->cfg.mcc_to_scc_switch;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_sys_pref(struct wlan_objmgr_psoc *psoc,
					uint8_t *sys_pref)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*sys_pref = pm_ctx->cfg.sys_pref;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_set_sys_pref(struct wlan_objmgr_psoc *psoc,
				   uint8_t sys_pref)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->cfg.sys_pref = sys_pref;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_conc_rule1(struct wlan_objmgr_psoc *psoc,
						uint8_t *conc_rule1)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*conc_rule1 = pm_ctx->cfg.conc_rule1;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_conc_rule2(struct wlan_objmgr_psoc *psoc,
						uint8_t *conc_rule2)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*conc_rule2 = pm_ctx->cfg.conc_rule2;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_chnl_select_plcy(struct wlan_objmgr_psoc *psoc,
					   uint32_t *chnl_select_plcy)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*chnl_select_plcy = pm_ctx->cfg.chnl_select_plcy;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_set_ch_select_plcy(struct wlan_objmgr_psoc *psoc,
					 uint32_t ch_select_policy)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->cfg.chnl_select_plcy = ch_select_policy;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_set_dynamic_mcc_adaptive_sch(
				struct wlan_objmgr_psoc *psoc,
				bool dynamic_mcc_adaptive_sched)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pm_ctx->dynamic_mcc_adaptive_sched = dynamic_mcc_adaptive_sched;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_dynamic_mcc_adaptive_sch(
				struct wlan_objmgr_psoc *psoc,
				bool *dynamic_mcc_adaptive_sched)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*dynamic_mcc_adaptive_sched = pm_ctx->dynamic_mcc_adaptive_sched;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_mcc_adaptive_sch(struct wlan_objmgr_psoc *psoc,
					   bool *enable_mcc_adaptive_sch)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*enable_mcc_adaptive_sch = pm_ctx->cfg.enable_mcc_adaptive_sch;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_sta_cxn_5g_band(struct wlan_objmgr_psoc *psoc,
					  uint8_t *enable_sta_cxn_5g_band)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("pm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	*enable_sta_cxn_5g_band = pm_ctx->cfg.enable_sta_cxn_5g_band;

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_update_new_hw_mode_index(struct wlan_objmgr_psoc *psoc,
		uint32_t new_hw_mode_index)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	pm_ctx->new_hw_mode_index = new_hw_mode_index;
}

void policy_mgr_update_old_hw_mode_index(struct wlan_objmgr_psoc *psoc,
		uint32_t old_hw_mode_index)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	pm_ctx->old_hw_mode_index = old_hw_mode_index;
}

void policy_mgr_update_hw_mode_index(struct wlan_objmgr_psoc *psoc,
		uint32_t new_hw_mode_index)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	if (POLICY_MGR_DEFAULT_HW_MODE_INDEX == pm_ctx->new_hw_mode_index) {
		pm_ctx->new_hw_mode_index = new_hw_mode_index;
	} else {
		pm_ctx->old_hw_mode_index = pm_ctx->new_hw_mode_index;
		pm_ctx->new_hw_mode_index = new_hw_mode_index;
	}
	policy_mgr_debug("Updated: old_hw_mode_index:%d new_hw_mode_index:%d",
		pm_ctx->old_hw_mode_index, pm_ctx->new_hw_mode_index);
}

/**
 * policy_mgr_get_num_of_setbits_from_bitmask() - to get num of
 * setbits from bitmask
 * @mask: given bitmask
 *
 * This helper function should return number of setbits from bitmask
 *
 * Return: number of setbits from bitmask
 */
static uint32_t policy_mgr_get_num_of_setbits_from_bitmask(uint32_t mask)
{
	uint32_t num_of_setbits = 0;

	while (mask) {
		mask &= (mask - 1);
		num_of_setbits++;
	}
	return num_of_setbits;
}

/**
 * policy_mgr_map_wmi_channel_width_to_hw_mode_bw() - returns
 * bandwidth in terms of hw_mode_bandwidth
 * @width: bandwidth in terms of wmi_channel_width
 *
 * This function returns the bandwidth in terms of hw_mode_bandwidth.
 *
 * Return: BW in terms of hw_mode_bandwidth.
 */
static enum hw_mode_bandwidth policy_mgr_map_wmi_channel_width_to_hw_mode_bw(
		wmi_channel_width width)
{
	switch (width) {
	case WMI_CHAN_WIDTH_20:
		return HW_MODE_20_MHZ;
	case WMI_CHAN_WIDTH_40:
		return HW_MODE_40_MHZ;
	case WMI_CHAN_WIDTH_80:
		return HW_MODE_80_MHZ;
	case WMI_CHAN_WIDTH_160:
		return HW_MODE_160_MHZ;
	case WMI_CHAN_WIDTH_80P80:
		return HW_MODE_80_PLUS_80_MHZ;
	case WMI_CHAN_WIDTH_5:
		return HW_MODE_5_MHZ;
	case WMI_CHAN_WIDTH_10:
		return HW_MODE_10_MHZ;
	default:
		return HW_MODE_BW_NONE;
	}

	return HW_MODE_BW_NONE;
}

static void policy_mgr_get_hw_mode_params(
		struct wlan_psoc_host_mac_phy_caps *caps,
		struct policy_mgr_mac_ss_bw_info *info)
{
	if (!caps) {
		policy_mgr_err("Invalid capabilities");
		return;
	}

	info->mac_tx_stream = policy_mgr_get_num_of_setbits_from_bitmask(
		QDF_MAX(caps->tx_chain_mask_2G,
		caps->tx_chain_mask_5G));
	info->mac_rx_stream = policy_mgr_get_num_of_setbits_from_bitmask(
		QDF_MAX(caps->rx_chain_mask_2G,
		caps->rx_chain_mask_5G));
	info->mac_bw = policy_mgr_map_wmi_channel_width_to_hw_mode_bw(
		QDF_MAX(caps->max_bw_supported_2G,
		caps->max_bw_supported_5G));
	info->mac_band_cap = caps->supported_bands;
}

/**
 * policy_mgr_set_hw_mode_params() - sets TX-RX stream,
 * bandwidth and DBS in hw_mode_list
 * @wma_handle: pointer to wma global structure
 * @mac0_ss_bw_info: TX-RX streams, BW for MAC0
 * @mac1_ss_bw_info: TX-RX streams, BW for MAC1
 * @pos: refers to hw_mode_list array index
 * @hw_mode_id: hw mode id value used by firmware
 * @dbs_mode: dbs_mode for the dbs_hw_mode
 * @sbs_mode: sbs_mode for the sbs_hw_mode
 *
 * This function sets TX-RX stream, bandwidth and DBS mode in
 * hw_mode_list.
 *
 * Return: none
 */
static void policy_mgr_set_hw_mode_params(struct wlan_objmgr_psoc *psoc,
			struct policy_mgr_mac_ss_bw_info mac0_ss_bw_info,
			struct policy_mgr_mac_ss_bw_info mac1_ss_bw_info,
			uint32_t pos, uint32_t hw_mode_id, uint32_t dbs_mode,
			uint32_t sbs_mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_tx_stream);
	POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_rx_stream);
	POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_bw);
	POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac1_ss_bw_info.mac_tx_stream);
	POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac1_ss_bw_info.mac_rx_stream);
	POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac1_ss_bw_info.mac_bw);
	POLICY_MGR_HW_MODE_DBS_MODE_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		dbs_mode);
	POLICY_MGR_HW_MODE_AGILE_DFS_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		HW_MODE_AGILE_DFS_NONE);
	POLICY_MGR_HW_MODE_SBS_MODE_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		sbs_mode);
	POLICY_MGR_HW_MODE_MAC0_BAND_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_band_cap);
	POLICY_MGR_HW_MODE_ID_SET(
		pm_ctx->hw_mode.hw_mode_list[pos],
		hw_mode_id);
}

QDF_STATUS policy_mgr_update_hw_mode_list(struct wlan_objmgr_psoc *psoc,
					  struct target_psoc_info *tgt_hdl)
{
	struct wlan_psoc_host_mac_phy_caps *tmp;
	uint32_t i, hw_config_type, j = 0;
	uint32_t dbs_mode, sbs_mode;
	struct policy_mgr_mac_ss_bw_info mac0_ss_bw_info = {0};
	struct policy_mgr_mac_ss_bw_info mac1_ss_bw_info = {0};
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct tgt_info *info;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	info = &tgt_hdl->info;
	if (!info->service_ext_param.num_hw_modes) {
		policy_mgr_err("Number of HW modes: %d",
			       info->service_ext_param.num_hw_modes);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * This list was updated as part of service ready event. Re-populate
	 * HW mode list from the device capabilities.
	 */
	if (pm_ctx->hw_mode.hw_mode_list) {
		qdf_mem_free(pm_ctx->hw_mode.hw_mode_list);
		pm_ctx->hw_mode.hw_mode_list = NULL;
		policy_mgr_debug("DBS list is freed");
	}

	pm_ctx->num_dbs_hw_modes = info->service_ext_param.num_hw_modes;
	pm_ctx->hw_mode.hw_mode_list =
		qdf_mem_malloc(sizeof(*pm_ctx->hw_mode.hw_mode_list) *
		pm_ctx->num_dbs_hw_modes);
	if (!pm_ctx->hw_mode.hw_mode_list) {
		pm_ctx->num_dbs_hw_modes = 0;
		return QDF_STATUS_E_NOMEM;
	}

	policy_mgr_debug("Updated HW mode list: Num modes:%d",
		pm_ctx->num_dbs_hw_modes);

	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		/* Update for MAC0 */
		tmp = &info->mac_phy_cap[j++];
		policy_mgr_get_hw_mode_params(tmp, &mac0_ss_bw_info);
		hw_config_type = tmp->hw_mode_config_type;
		dbs_mode = HW_MODE_DBS_NONE;
		sbs_mode = HW_MODE_SBS_NONE;
		mac1_ss_bw_info.mac_tx_stream = 0;
		mac1_ss_bw_info.mac_rx_stream = 0;
		mac1_ss_bw_info.mac_bw = 0;

		/* SBS and DBS have dual MAC. Upto 2 MACs are considered. */
		if ((hw_config_type == WMI_HW_MODE_DBS) ||
			(hw_config_type == WMI_HW_MODE_SBS_PASSIVE) ||
			(hw_config_type == WMI_HW_MODE_SBS)) {
			/* Update for MAC1 */
			tmp = &info->mac_phy_cap[j++];
			policy_mgr_get_hw_mode_params(tmp, &mac1_ss_bw_info);
			if (hw_config_type == WMI_HW_MODE_DBS)
				dbs_mode = HW_MODE_DBS;
			if ((hw_config_type == WMI_HW_MODE_SBS_PASSIVE) ||
				(hw_config_type == WMI_HW_MODE_SBS))
				sbs_mode = HW_MODE_SBS;
		}

		/* Updating HW mode list */
		policy_mgr_set_hw_mode_params(psoc, mac0_ss_bw_info,
			mac1_ss_bw_info, i, tmp->hw_mode_id, dbs_mode,
			sbs_mode);
	}
	return QDF_STATUS_SUCCESS;
}

void policy_mgr_init_dbs_hw_mode(struct wlan_objmgr_psoc *psoc,
		uint32_t num_dbs_hw_modes,
		uint32_t *ev_wlan_dbs_hw_mode_list)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	pm_ctx->num_dbs_hw_modes = num_dbs_hw_modes;
	pm_ctx->hw_mode.hw_mode_list =
		qdf_mem_malloc(sizeof(*pm_ctx->hw_mode.hw_mode_list) *
		pm_ctx->num_dbs_hw_modes);
	if (!pm_ctx->hw_mode.hw_mode_list) {
		pm_ctx->num_dbs_hw_modes = 0;
		return;
	}

	qdf_mem_copy(pm_ctx->hw_mode.hw_mode_list,
		ev_wlan_dbs_hw_mode_list,
		(sizeof(*pm_ctx->hw_mode.hw_mode_list) *
		pm_ctx->num_dbs_hw_modes));
}

void policy_mgr_dump_dbs_hw_mode(struct wlan_objmgr_psoc *psoc)
{
	uint32_t i, param;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		param = pm_ctx->hw_mode.hw_mode_list[i];
		policy_mgr_debug("[%d]-MAC0: tx_ss:%d rx_ss:%d bw_idx:%d band_cap:%d",
				 i,
				 POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_GET(param),
				 POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_GET(param),
				 POLICY_MGR_HW_MODE_MAC0_BANDWIDTH_GET(param),
				 POLICY_MGR_HW_MODE_MAC0_BAND_GET(param));
		policy_mgr_debug("[%d]-MAC1: tx_ss:%d rx_ss:%d bw_idx:%d",
				 i,
				 POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_GET(param),
				 POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_GET(param),
				 POLICY_MGR_HW_MODE_MAC1_BANDWIDTH_GET(param));
		policy_mgr_debug("[%d] DBS:%d SBS:%d hw_mode_id:%d", i,
				 POLICY_MGR_HW_MODE_DBS_MODE_GET(param),
				 POLICY_MGR_HW_MODE_SBS_MODE_GET(param),
				 POLICY_MGR_HW_MODE_ID_GET(param));
	}
}

void policy_mgr_init_dbs_config(struct wlan_objmgr_psoc *psoc,
		uint32_t scan_config, uint32_t fw_config)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t dual_mac_feature;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	pm_ctx->dual_mac_cfg.cur_scan_config = 0;
	pm_ctx->dual_mac_cfg.cur_fw_mode_config = 0;

	dual_mac_feature = pm_ctx->cfg.dual_mac_feature;
	/* If dual mac features are disabled in the INI, we
	 * need not proceed further
	 */
	if (dual_mac_feature == DISABLE_DBS_CXN_AND_SCAN) {
		policy_mgr_err("Disabling dual mac capabilities");
		/* All capabilities are initialized to 0. We can return */
		goto done;
	}

	/* Initialize concurrent_scan_config_bits with default FW value */
	WMI_DBS_CONC_SCAN_CFG_ASYNC_DBS_SCAN_SET(
		pm_ctx->dual_mac_cfg.cur_scan_config,
		WMI_DBS_CONC_SCAN_CFG_ASYNC_DBS_SCAN_GET(scan_config));
	WMI_DBS_CONC_SCAN_CFG_SYNC_DBS_SCAN_SET(
		pm_ctx->dual_mac_cfg.cur_scan_config,
		WMI_DBS_CONC_SCAN_CFG_SYNC_DBS_SCAN_GET(scan_config));
	WMI_DBS_CONC_SCAN_CFG_DBS_SCAN_SET(
		pm_ctx->dual_mac_cfg.cur_scan_config,
		WMI_DBS_CONC_SCAN_CFG_DBS_SCAN_GET(scan_config));
	WMI_DBS_CONC_SCAN_CFG_AGILE_SCAN_SET(
		pm_ctx->dual_mac_cfg.cur_scan_config,
		WMI_DBS_CONC_SCAN_CFG_AGILE_SCAN_GET(scan_config));
	WMI_DBS_CONC_SCAN_CFG_AGILE_DFS_SCAN_SET(
		pm_ctx->dual_mac_cfg.cur_scan_config,
		WMI_DBS_CONC_SCAN_CFG_AGILE_DFS_SCAN_GET(scan_config));

	/* Initialize fw_mode_config_bits with default FW value */
	WMI_DBS_FW_MODE_CFG_DBS_SET(
		pm_ctx->dual_mac_cfg.cur_fw_mode_config,
		WMI_DBS_FW_MODE_CFG_DBS_GET(fw_config));
	WMI_DBS_FW_MODE_CFG_AGILE_DFS_SET(
		pm_ctx->dual_mac_cfg.cur_fw_mode_config,
		WMI_DBS_FW_MODE_CFG_AGILE_DFS_GET(fw_config));
	WMI_DBS_FW_MODE_CFG_DBS_FOR_CXN_SET(
		pm_ctx->dual_mac_cfg.cur_fw_mode_config,
		WMI_DBS_FW_MODE_CFG_DBS_FOR_CXN_GET(fw_config));
done:
	/* Initialize the previous scan/fw mode config */
	pm_ctx->dual_mac_cfg.prev_scan_config =
		pm_ctx->dual_mac_cfg.cur_scan_config;
	pm_ctx->dual_mac_cfg.prev_fw_mode_config =
		pm_ctx->dual_mac_cfg.cur_fw_mode_config;

	policy_mgr_debug("cur_scan_config:%x cur_fw_mode_config:%x",
		pm_ctx->dual_mac_cfg.cur_scan_config,
		pm_ctx->dual_mac_cfg.cur_fw_mode_config);
}

void policy_mgr_update_dbs_scan_config(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	pm_ctx->dual_mac_cfg.prev_scan_config =
		pm_ctx->dual_mac_cfg.cur_scan_config;
	pm_ctx->dual_mac_cfg.cur_scan_config =
		pm_ctx->dual_mac_cfg.req_scan_config;
}

void policy_mgr_update_dbs_fw_config(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	pm_ctx->dual_mac_cfg.prev_fw_mode_config =
		pm_ctx->dual_mac_cfg.cur_fw_mode_config;
	pm_ctx->dual_mac_cfg.cur_fw_mode_config =
		pm_ctx->dual_mac_cfg.req_fw_mode_config;
}

void policy_mgr_update_dbs_req_config(struct wlan_objmgr_psoc *psoc,
		uint32_t scan_config, uint32_t fw_mode_config)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	pm_ctx->dual_mac_cfg.req_scan_config = scan_config;
	pm_ctx->dual_mac_cfg.req_fw_mode_config = fw_mode_config;
}

bool policy_mgr_get_dbs_plus_agile_scan_config(struct wlan_objmgr_psoc *psoc)
{
	uint32_t scan_config;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	if (policy_mgr_is_dual_mac_disabled_in_ini(psoc))
		return false;


	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		/* We take that it is disabled and proceed */
		return false;
	}
	scan_config = pm_ctx->dual_mac_cfg.cur_scan_config;

	return WMI_DBS_CONC_SCAN_CFG_AGILE_SCAN_GET(scan_config);
}

bool policy_mgr_get_single_mac_scan_with_dfs_config(
		struct wlan_objmgr_psoc *psoc)
{
	uint32_t scan_config;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	if (policy_mgr_is_dual_mac_disabled_in_ini(psoc))
		return false;


	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		/* We take that it is disabled and proceed */
		return false;
	}
	scan_config = pm_ctx->dual_mac_cfg.cur_scan_config;

	return WMI_DBS_CONC_SCAN_CFG_AGILE_DFS_SCAN_GET(scan_config);
}

int8_t policy_mgr_get_num_dbs_hw_modes(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return -EINVAL;
	}
	return pm_ctx->num_dbs_hw_modes;
}

bool policy_mgr_find_if_fw_supports_dbs(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wmi_unified *wmi_handle;
	bool dbs_support;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}
	dbs_support =
	wmi_service_enabled(wmi_handle,
			    wmi_service_dual_band_simultaneous_support);

	/* The agreement with FW is that: To know if the target is DBS
	 * capable, DBS needs to be supported both in the HW mode list
	 * and in the service ready event
	 */
	if (!dbs_support)
		return false;

	return true;
}

bool policy_mgr_find_if_hwlist_has_dbs(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t param, i, found = 0;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		param = pm_ctx->hw_mode.hw_mode_list[i];
		if (POLICY_MGR_HW_MODE_DBS_MODE_GET(param)) {
			found = 1;
			break;
		}
	}
	if (found)
		return true;

	return false;
}

static bool policy_mgr_find_if_hwlist_has_sbs(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t param, i, found = 0;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		param = pm_ctx->hw_mode.hw_mode_list[i];
		if (POLICY_MGR_HW_MODE_SBS_MODE_GET(param)) {
			found = 1;
			break;
		}
	}
	if (found)
		return true;

	return false;
}

bool policy_mgr_is_dbs_scan_allowed(struct wlan_objmgr_psoc *psoc)
{
	uint8_t dbs_type = DISABLE_DBS_CXN_AND_SCAN;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (!policy_mgr_find_if_fw_supports_dbs(psoc) ||
	    !policy_mgr_find_if_hwlist_has_dbs(psoc))
		return false;

	policy_mgr_get_dual_mac_feature(psoc, &dbs_type);
	/*
	 * If DBS support for scan is disabled through INI then DBS is not
	 * supported for scan.
	 *
	 * For DBS scan check the INI value explicitly
	 */
	switch (dbs_type) {
	case DISABLE_DBS_CXN_AND_SCAN:
	case ENABLE_DBS_CXN_AND_DISABLE_DBS_SCAN:
		return false;
	default:
		return true;
	}
}

bool policy_mgr_is_hw_dbs_capable(struct wlan_objmgr_psoc *psoc)
{
	if (!policy_mgr_is_dbs_enable(psoc))
		return false;

	if (!policy_mgr_find_if_fw_supports_dbs(psoc))
		return false;

	if (!policy_mgr_find_if_hwlist_has_dbs(psoc)) {
		policymgr_nofl_debug("HW mode list has no DBS");
		return false;
	}

	return true;
}

bool policy_mgr_is_interband_mcc_supported(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct wmi_unified *wmi_handle;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}

	return !wmi_service_enabled(wmi_handle,
				    wmi_service_no_interband_mcc_support);
}

bool policy_mgr_is_hw_sbs_capable(struct wlan_objmgr_psoc *psoc)
{
	if (!policy_mgr_find_if_fw_supports_dbs(psoc)) {
		return false;
	}

	if (!policy_mgr_find_if_hwlist_has_sbs(psoc)) {
		policymgr_nofl_debug("HW mode list has no SBS");
		return false;
	}

	return true;
}

QDF_STATUS policy_mgr_get_dbs_hw_modes(struct wlan_objmgr_psoc *psoc,
		bool *one_by_one_dbs, bool *two_by_two_dbs)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i;
	int8_t found_one_by_one = -EINVAL, found_two_by_two = -EINVAL;
	uint32_t conf1_tx_ss, conf1_rx_ss;
	uint32_t conf2_tx_ss, conf2_rx_ss;

	*one_by_one_dbs = false;
	*two_by_two_dbs = false;

	if (policy_mgr_is_hw_dbs_capable(psoc) == false) {
		policy_mgr_rl_debug("HW is not DBS capable");
		/* Caller will understand that DBS is disabled */
		return QDF_STATUS_SUCCESS;

	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	/* To check 1x1 capability */
	policy_mgr_get_tx_rx_ss_from_config(HW_MODE_SS_1x1,
			&conf1_tx_ss, &conf1_rx_ss);
	/* To check 2x2 capability */
	policy_mgr_get_tx_rx_ss_from_config(HW_MODE_SS_2x2,
			&conf2_tx_ss, &conf2_rx_ss);

	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		uint32_t t_conf0_tx_ss, t_conf0_rx_ss;
		uint32_t t_conf1_tx_ss, t_conf1_rx_ss;
		uint32_t dbs_mode;

		t_conf0_tx_ss = POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_GET(
				pm_ctx->hw_mode.hw_mode_list[i]);
		t_conf0_rx_ss = POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_GET(
				pm_ctx->hw_mode.hw_mode_list[i]);
		t_conf1_tx_ss = POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_GET(
				pm_ctx->hw_mode.hw_mode_list[i]);
		t_conf1_rx_ss = POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_GET(
				pm_ctx->hw_mode.hw_mode_list[i]);
		dbs_mode = POLICY_MGR_HW_MODE_DBS_MODE_GET(
				pm_ctx->hw_mode.hw_mode_list[i]);

		if (((((t_conf0_tx_ss == conf1_tx_ss) &&
		    (t_conf0_rx_ss == conf1_rx_ss)) ||
		    ((t_conf1_tx_ss == conf1_tx_ss) &&
		    (t_conf1_rx_ss == conf1_rx_ss))) &&
		    (dbs_mode == HW_MODE_DBS)) &&
		    (found_one_by_one < 0)) {
			found_one_by_one = i;
			policy_mgr_debug("1x1 hw_mode index %d found", i);
			/* Once an entry is found, need not check for 1x1
			 * again
			 */
			continue;
		}

		if (((((t_conf0_tx_ss == conf2_tx_ss) &&
		    (t_conf0_rx_ss == conf2_rx_ss)) ||
		    ((t_conf1_tx_ss == conf2_tx_ss) &&
		    (t_conf1_rx_ss == conf2_rx_ss))) &&
		    (dbs_mode == HW_MODE_DBS)) &&
		    (found_two_by_two < 0)) {
			found_two_by_two = i;
			policy_mgr_debug("2x2 hw_mode index %d found", i);
			/* Once an entry is found, need not check for 2x2
			 * again
			 */
			continue;
		}
	}

	if (found_one_by_one >= 0)
		*one_by_one_dbs = true;
	if (found_two_by_two >= 0)
		*two_by_two_dbs = true;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_get_current_hw_mode(struct wlan_objmgr_psoc *psoc,
		struct policy_mgr_hw_mode_params *hw_mode)
{
	QDF_STATUS status;
	uint32_t old_hw_index = 0, new_hw_index = 0;

	status = policy_mgr_get_old_and_new_hw_index(psoc, &old_hw_index,
			&new_hw_index);
	if (QDF_STATUS_SUCCESS != status) {
		policy_mgr_err("Failed to get HW mode index");
		return QDF_STATUS_E_FAILURE;
	}

	if (new_hw_index == POLICY_MGR_DEFAULT_HW_MODE_INDEX) {
		policy_mgr_err("HW mode is not yet initialized");
		return QDF_STATUS_E_FAILURE;
	}

	status = policy_mgr_get_hw_mode_from_idx(psoc, new_hw_index, hw_mode);
	if (QDF_STATUS_SUCCESS != status) {
		policy_mgr_err("Failed to get HW mode index");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_is_current_hwmode_dbs(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_hw_mode_params hw_mode;

	if (!policy_mgr_is_hw_dbs_capable(psoc))
		return false;
	if (QDF_STATUS_SUCCESS !=
		policy_mgr_get_current_hw_mode(psoc, &hw_mode))
		return false;
	if (hw_mode.dbs_cap)
		return true;
	return false;
}

bool policy_mgr_is_dbs_enable(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	if (policy_mgr_is_dual_mac_disabled_in_ini(psoc)) {
		policy_mgr_rl_debug("DBS is disabled from ini");
		return false;
	}

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (WMI_DBS_FW_MODE_CFG_DBS_GET(
			pm_ctx->dual_mac_cfg.cur_fw_mode_config))
		return true;

	return false;
}

bool policy_mgr_is_hw_dbs_2x2_capable(struct wlan_objmgr_psoc *psoc)
{
	struct dbs_nss nss_dbs = {0};
	uint32_t nss;

	nss = policy_mgr_get_hw_dbs_nss(psoc, &nss_dbs);
	if (nss >= HW_MODE_SS_2x2 && (nss_dbs.mac0_ss == nss_dbs.mac1_ss))
		return true;
	else
		return false;
}

bool policy_mgr_is_hw_dbs_required_for_band(struct wlan_objmgr_psoc *psoc,
					    enum hw_mode_mac_band_cap band)
{
	struct dbs_nss nss_dbs = {0};
	uint32_t nss;

	nss = policy_mgr_get_hw_dbs_nss(psoc, &nss_dbs);
	if (nss >= HW_MODE_SS_1x1 && nss_dbs.mac0_ss == nss_dbs.mac1_ss &&
	    !(nss_dbs.single_mac0_band_cap & band))
		return true;
	else
		return false;
}

bool policy_mgr_is_dp_hw_dbs_2x2_capable(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_is_hw_dbs_2x2_capable(psoc) ||
		policy_mgr_is_hw_dbs_required_for_band(psoc,
						       HW_MODE_MAC_BAND_2G);
}

/*
 * policy_mgr_is_2x2_1x1_dbs_capable() - check 2x2+1x1 DBS supported or not
 * @psoc: PSOC object data
 *
 * This routine is called to check 2x2 5G + 1x1 2G (DBS1) or
 * 2x2 2G + 1x1 5G (DBS2) support or not.
 * Either DBS1 or DBS2 supported
 *
 * Return: true/false
 */
bool policy_mgr_is_2x2_1x1_dbs_capable(struct wlan_objmgr_psoc *psoc)
{
	struct dbs_nss nss_dbs;
	uint32_t nss;

	nss = policy_mgr_get_hw_dbs_nss(psoc, &nss_dbs);
	if (nss >= HW_MODE_SS_2x2 && (nss_dbs.mac0_ss > nss_dbs.mac1_ss))
		return true;
	else
		return false;
}

/*
 * policy_mgr_is_2x2_5G_1x1_2G_dbs_capable() - check Genoa DBS1 enabled or not
 * @psoc: PSOC object data
 *
 * This routine is called to check support DBS1 or not.
 * Notes: DBS1: 2x2 5G + 1x1 2G.
 * This function will call policy_mgr_get_hw_mode_idx_from_dbs_hw_list to match
 * the HW mode from hw mode list. The parameters will also be matched to
 * 2x2 5G +2x2 2G HW mode. But firmware will not report 2x2 5G + 2x2 2G alone
 * with 2x2 5G + 1x1 2G at same time. So, it is safe to find DBS1 with
 * policy_mgr_get_hw_mode_idx_from_dbs_hw_list.
 *
 * Return: true/false
 */
bool policy_mgr_is_2x2_5G_1x1_2G_dbs_capable(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_is_2x2_1x1_dbs_capable(psoc) &&
		(policy_mgr_get_hw_mode_idx_from_dbs_hw_list(
					psoc,
					HW_MODE_SS_2x2,
					HW_MODE_80_MHZ,
					HW_MODE_SS_1x1, HW_MODE_40_MHZ,
					HW_MODE_MAC_BAND_5G,
					HW_MODE_DBS,
					HW_MODE_AGILE_DFS_NONE,
					HW_MODE_SBS_NONE) >= 0);
}

/*
 * policy_mgr_is_2x2_2G_1x1_5G_dbs_capable() - check Genoa DBS2 enabled or not
 * @psoc: PSOC object data
 *
 * This routine is called to check support DBS2 or not.
 * Notes: DBS2: 2x2 2G + 1x1 5G
 *
 * Return: true/false
 */
bool policy_mgr_is_2x2_2G_1x1_5G_dbs_capable(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_is_2x2_1x1_dbs_capable(psoc) &&
		(policy_mgr_get_hw_mode_idx_from_dbs_hw_list(
					psoc,
					HW_MODE_SS_2x2,
					HW_MODE_40_MHZ,
					HW_MODE_SS_1x1, HW_MODE_40_MHZ,
					HW_MODE_MAC_BAND_2G,
					HW_MODE_DBS,
					HW_MODE_AGILE_DFS_NONE,
					HW_MODE_SBS_NONE) >= 0);
}

uint32_t policy_mgr_get_connection_count(struct wlan_objmgr_psoc *psoc)
{
	uint32_t conn_index, count = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if (pm_conc_connection_list[conn_index].in_use)
			count++;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

uint32_t policy_mgr_mode_specific_vdev_id(struct wlan_objmgr_psoc *psoc,
					  enum policy_mgr_con_mode mode)
{
	uint32_t conn_index = 0;
	uint32_t vdev_id = WLAN_INVALID_VDEV_ID;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return vdev_id;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	/*
	 * Note: This gives you the first vdev id of the mode type in a
	 * sta+sta or sap+sap or p2p + p2p case
	 */
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if ((pm_conc_connection_list[conn_index].mode == mode) &&
			pm_conc_connection_list[conn_index].in_use) {
			vdev_id = pm_conc_connection_list[conn_index].vdev_id;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return vdev_id;
}

uint32_t policy_mgr_mode_specific_connection_count(
		struct wlan_objmgr_psoc *psoc,
		enum policy_mgr_con_mode mode,
		uint32_t *list)
{
	uint32_t conn_index = 0, count = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if ((pm_conc_connection_list[conn_index].mode == mode) &&
			pm_conc_connection_list[conn_index].in_use) {
			if (list)
				list[count] = conn_index;
			 count++;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

QDF_STATUS policy_mgr_check_conn_with_mode_and_vdev_id(
		struct wlan_objmgr_psoc *psoc, enum policy_mgr_con_mode mode,
		uint32_t vdev_id)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return qdf_status;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if ((pm_conc_connection_list[conn_index].mode == mode) &&
		    (pm_conc_connection_list[conn_index].vdev_id == vdev_id)) {
			qdf_status = QDF_STATUS_SUCCESS;
			break;
		}
		conn_index++;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	return qdf_status;
}

void policy_mgr_soc_set_dual_mac_cfg_cb(enum set_hw_mode_status status,
		uint32_t scan_config,
		uint32_t fw_mode_config)
{
	policy_mgr_debug("Status:%d for scan_config:%x fw_mode_config:%x",
			 status, scan_config, fw_mode_config);
}

void policy_mgr_set_dual_mac_scan_config(struct wlan_objmgr_psoc *psoc,
		uint8_t dbs_val,
		uint8_t dbs_plus_agile_scan_val,
		uint8_t single_mac_scan_with_dbs_val)
{
	struct policy_mgr_dual_mac_config cfg;
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	/* Any non-zero positive value is treated as 1 */
	if (dbs_val != 0)
		dbs_val = 1;
	if (dbs_plus_agile_scan_val != 0)
		dbs_plus_agile_scan_val = 1;
	if (single_mac_scan_with_dbs_val != 0)
		single_mac_scan_with_dbs_val = 1;

	status = policy_mgr_get_updated_scan_config(psoc, &cfg.scan_config,
			dbs_val,
			dbs_plus_agile_scan_val,
			single_mac_scan_with_dbs_val);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("policy_mgr_get_updated_scan_config failed %d",
			status);
		return;
	}

	status = policy_mgr_get_updated_fw_mode_config(psoc,
			&cfg.fw_mode_config,
			policy_mgr_get_dbs_config(psoc),
			policy_mgr_get_agile_dfs_config(psoc));
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("policy_mgr_get_updated_fw_mode_config failed %d",
			status);
		return;
	}

	cfg.set_dual_mac_cb = policy_mgr_soc_set_dual_mac_cfg_cb;

	policy_mgr_debug("scan_config:%x fw_mode_config:%x",
			cfg.scan_config, cfg.fw_mode_config);

	status = pm_ctx->sme_cbacks.sme_soc_set_dual_mac_config(cfg);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("sme_soc_set_dual_mac_config failed %d", status);
}

void policy_mgr_set_dual_mac_fw_mode_config(struct wlan_objmgr_psoc *psoc,
			uint8_t dbs, uint8_t dfs)
{
	struct policy_mgr_dual_mac_config cfg;
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	/* Any non-zero positive value is treated as 1 */
	if (dbs != 0)
		dbs = 1;
	if (dfs != 0)
		dfs = 1;

	status = policy_mgr_get_updated_scan_config(psoc, &cfg.scan_config,
			policy_mgr_get_dbs_scan_config(psoc),
			policy_mgr_get_dbs_plus_agile_scan_config(psoc),
			policy_mgr_get_single_mac_scan_with_dfs_config(psoc));
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("policy_mgr_get_updated_scan_config failed %d",
			status);
		return;
	}

	status = policy_mgr_get_updated_fw_mode_config(psoc,
				&cfg.fw_mode_config, dbs, dfs);
	if (status != QDF_STATUS_SUCCESS) {
		policy_mgr_err("policy_mgr_get_updated_fw_mode_config failed %d",
			status);
		return;
	}

	cfg.set_dual_mac_cb = policy_mgr_soc_set_dual_mac_cfg_cb;

	policy_mgr_debug("scan_config:%x fw_mode_config:%x",
			cfg.scan_config, cfg.fw_mode_config);

	status = pm_ctx->sme_cbacks.sme_soc_set_dual_mac_config(cfg);
	if (status != QDF_STATUS_SUCCESS)
		policy_mgr_err("sme_soc_set_dual_mac_config failed %d", status);
}

bool policy_mgr_is_scc_with_this_vdev_id(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t i, ch_freq;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	/* Get the channel freq for a given vdev_id */
	status = policy_mgr_get_chan_by_session_id(psoc, vdev_id,
						   &ch_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Failed to get channel for vdev:%d", vdev_id);
		return false;
	}

	/* Compare given vdev_id freq against other vdev_id's */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if ((pm_conc_connection_list[i].vdev_id != vdev_id) &&
		    (pm_conc_connection_list[i].in_use) &&
		    (pm_conc_connection_list[i].freq == ch_freq)) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return false;
}

bool policy_mgr_current_concurrency_is_scc(struct wlan_objmgr_psoc *psoc)
{
	uint32_t num_connections = 0;
	bool is_scc = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return is_scc;
	}

	num_connections = policy_mgr_get_connection_count(psoc);

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	switch (num_connections) {
	case 1:
		break;
	case 2:
		if (pm_conc_connection_list[0].freq ==
		    pm_conc_connection_list[1].freq &&
		    pm_conc_connection_list[0].mac ==
		    pm_conc_connection_list[1].mac) {
			is_scc = true;
		}
		break;
	case 3:
		if (policy_mgr_is_current_hwmode_dbs(psoc) &&
		    (pm_conc_connection_list[0].freq ==
		     pm_conc_connection_list[1].freq ||
		     pm_conc_connection_list[0].freq ==
		     pm_conc_connection_list[2].freq ||
		     pm_conc_connection_list[1].freq ==
		     pm_conc_connection_list[2].freq)) {
			is_scc = true;
		} else if ((pm_conc_connection_list[0].freq ==
			    pm_conc_connection_list[1].freq) &&
			   (pm_conc_connection_list[0].freq ==
			   pm_conc_connection_list[2].freq)) {
			is_scc = true;
		}
		break;
	default:
		policy_mgr_debug("unexpected num_connections value %d",
				 num_connections);
		break;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return is_scc;
}

bool policy_mgr_current_concurrency_is_mcc(struct wlan_objmgr_psoc *psoc)
{
	uint32_t num_connections = 0;
	bool is_mcc = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return is_mcc;
	}

	num_connections = policy_mgr_get_connection_count(psoc);

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	switch (num_connections) {
	case 1:
		break;
	case 2:
		if (pm_conc_connection_list[0].freq !=
		    pm_conc_connection_list[1].freq &&
		    pm_conc_connection_list[0].mac ==
		    pm_conc_connection_list[1].mac) {
			is_mcc = true;
		}
		break;
	case 3:
		if (pm_conc_connection_list[0].freq !=
		    pm_conc_connection_list[1].freq ||
		    pm_conc_connection_list[0].freq !=
		    pm_conc_connection_list[2].freq ||
		    pm_conc_connection_list[1].freq !=
		    pm_conc_connection_list[2].freq){
			is_mcc = true;
		}
		break;
	default:
		policy_mgr_debug("unexpected num_connections value %d",
				 num_connections);
		break;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return is_mcc;
}

bool policy_mgr_is_sap_p2pgo_on_dfs(struct wlan_objmgr_psoc *psoc)
{
	int index, count;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct policy_mgr_psoc_priv_obj *pm_ctx = NULL;

	if (psoc)
		pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	index = 0;
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	count = policy_mgr_mode_specific_connection_count(psoc,
							  PM_SAP_MODE,
							  list);
	while (index < count) {
		if (pm_conc_connection_list[list[index]].ch_flagext &
		    (IEEE80211_CHAN_DFS | IEEE80211_CHAN_DFS_CFREQ2)) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return true;
		}
		index++;
	}
	count = policy_mgr_mode_specific_connection_count(psoc,
							  PM_P2P_GO_MODE,
							  list);
	index = 0;
	while (index < count) {
		if (pm_conc_connection_list[list[index]].ch_flagext &
		    (IEEE80211_CHAN_DFS | IEEE80211_CHAN_DFS_CFREQ2)) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return true;
		}
		index++;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	return false;
}

/**
 * policy_mgr_set_concurrency_mode() - To set concurrency mode
 * @psoc: PSOC object data
 * @mode: device mode
 *
 * This routine is called to set the concurrency mode
 *
 * Return: NONE
 */
void policy_mgr_set_concurrency_mode(struct wlan_objmgr_psoc *psoc,
				     enum QDF_OPMODE mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return;
	}

	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_MONITOR_MODE:
		pm_ctx->concurrency_mode |= (1 << mode);
		pm_ctx->no_of_open_sessions[mode]++;
		break;
	default:
		break;
	}

	policy_mgr_debug("concurrency_mode = 0x%x Number of open sessions for mode %d = %d",
			 pm_ctx->concurrency_mode, mode,
		pm_ctx->no_of_open_sessions[mode]);
}

/**
 * policy_mgr_clear_concurrency_mode() - To clear concurrency mode
 * @psoc: PSOC object data
 * @mode: device mode
 *
 * This routine is called to clear the concurrency mode
 *
 * Return: NONE
 */
void policy_mgr_clear_concurrency_mode(struct wlan_objmgr_psoc *psoc,
				       enum QDF_OPMODE mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return;
	}

	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_MONITOR_MODE:
		pm_ctx->no_of_open_sessions[mode]--;
		if (!(pm_ctx->no_of_open_sessions[mode]))
			pm_ctx->concurrency_mode &= (~(1 << mode));
		break;
	default:
		break;
	}

	policy_mgr_debug("concurrency_mode = 0x%x Number of open sessions for mode %d = %d",
			 pm_ctx->concurrency_mode, mode,
			 pm_ctx->no_of_open_sessions[mode]);
}

/**
 * policy_mgr_validate_conn_info() - validate conn info list
 * @psoc: PSOC object data
 *
 * This function will check connection list to see duplicated
 * vdev entry existing or not.
 *
 * Return: true if conn list is in abnormal state.
 */
static bool
policy_mgr_validate_conn_info(struct wlan_objmgr_psoc *psoc)
{
	uint32_t i, j, conn_num = 0;
	bool panic = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return true;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].in_use) {
			for (j = i + 1; j < MAX_NUMBER_OF_CONC_CONNECTIONS;
									j++) {
				if (pm_conc_connection_list[j].in_use &&
				    pm_conc_connection_list[i].vdev_id ==
				    pm_conc_connection_list[j].vdev_id) {
					policy_mgr_debug(
					"dup entry %d",
					pm_conc_connection_list[i].vdev_id);
					panic = true;
				}
			}
			conn_num++;
		}
	}
	if (panic)
		policy_mgr_err("dup entry");

	for (i = 0, j = 0; i < QDF_MAX_NO_OF_MODE; i++)
		j += pm_ctx->no_of_active_sessions[i];

	if (j != conn_num) {
		policy_mgr_err("active session/conn count mismatch %d %d",
			       j, conn_num);
		panic = true;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (panic)
		policy_mgr_debug_alert();

	return panic;
}

void policy_mgr_incr_active_session(struct wlan_objmgr_psoc *psoc,
				enum QDF_OPMODE mode,
				uint8_t session_id)
{
	mac_handle_t mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_6ghz_flag = 0, conn_idx = 0;
	uint8_t vdev_id = WLAN_INVALID_VDEV_ID;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	/*
	 * Need to aquire mutex as entire functionality in this function
	 * is in critical section
	 */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_NAN_DISC_MODE:
	case QDF_NDI_MODE:
		pm_ctx->no_of_active_sessions[mode]++;
		break;
	default:
		break;
	}

	if (mode == QDF_NDI_MODE &&
	    pm_ctx->hdd_cbacks.wlan_hdd_indicate_active_ndp_cnt)
		pm_ctx->hdd_cbacks.wlan_hdd_indicate_active_ndp_cnt(
				psoc, session_id,
				pm_ctx->no_of_active_sessions[mode]);

	if (mode != QDF_NAN_DISC_MODE && pm_ctx->dp_cbacks.hdd_v2_flow_pool_map)
		pm_ctx->dp_cbacks.hdd_v2_flow_pool_map(session_id);
	if (mode == QDF_SAP_MODE)
		policy_mgr_get_ap_6ghz_capable(psoc, session_id,
					       &conn_6ghz_flag);

	policy_mgr_debug("No.# of active sessions for mode %d = %d",
		mode, pm_ctx->no_of_active_sessions[mode]);
	policy_mgr_incr_connection_count(psoc, session_id, mode);

	if ((policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, NULL) > 0) && (mode != QDF_STA_MODE)) {
		/* Send set pcl for all the connected STA vdev */
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
		for (conn_idx = 0; conn_idx < MAX_NUMBER_OF_CONC_CONNECTIONS;
		     conn_idx++) {
			qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
			if (!(pm_conc_connection_list[conn_idx].mode ==
			      PM_STA_MODE &&
			      pm_conc_connection_list[conn_idx].in_use)) {
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				continue;
			}

			vdev_id = pm_conc_connection_list[conn_idx].vdev_id;
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

			pm_ctx->sme_cbacks.sme_rso_stop_cb(
					mac_handle, vdev_id,
					REASON_DRIVER_DISABLED,
					RSO_SET_PCL);

			policy_mgr_set_pcl_for_existing_combo(psoc, PM_STA_MODE,
							      vdev_id);
			pm_ctx->sme_cbacks.sme_rso_start_cb(
					mac_handle, vdev_id,
					REASON_DRIVER_ENABLED,
					RSO_SET_PCL);
		}
		qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	}

	/* Notify tdls */
	if (pm_ctx->tdls_cbacks.tdls_notify_increment_session)
		pm_ctx->tdls_cbacks.tdls_notify_increment_session(psoc);

	/*
	 * Disable LRO/GRO if P2P or SAP connection has come up or
	 * there are more than one STA connections
	 */
	if ((policy_mgr_mode_specific_connection_count(psoc, PM_STA_MODE, NULL) > 1) ||
	    (policy_mgr_mode_specific_connection_count(psoc, PM_SAP_MODE, NULL) > 0) ||
	    (policy_mgr_mode_specific_connection_count(psoc, PM_P2P_CLIENT_MODE, NULL) >
									0) ||
	    (policy_mgr_mode_specific_connection_count(psoc, PM_P2P_GO_MODE, NULL) > 0) ||
	    (policy_mgr_mode_specific_connection_count(psoc,
						       PM_NDI_MODE,
						       NULL) > 0)) {
		if (pm_ctx->dp_cbacks.hdd_disable_rx_ol_in_concurrency)
			pm_ctx->dp_cbacks.hdd_disable_rx_ol_in_concurrency(true);
	};

	/* Enable RPS if SAP interface has come up */
	if (policy_mgr_mode_specific_connection_count(psoc, PM_SAP_MODE, NULL)
		== 1) {
		if (pm_ctx->dp_cbacks.hdd_set_rx_mode_rps_cb)
			pm_ctx->dp_cbacks.hdd_set_rx_mode_rps_cb(true);
	}
	if (mode == QDF_SAP_MODE)
		policy_mgr_init_ap_6ghz_capable(psoc, session_id,
						conn_6ghz_flag);
	if (mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE ||
	    mode == QDF_STA_MODE || mode == QDF_P2P_CLIENT_MODE)
		policy_mgr_update_dfs_master_dynamic_enabled(psoc, session_id);

	policy_mgr_dump_current_concurrency(psoc);

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
}

QDF_STATUS policy_mgr_decr_active_session(struct wlan_objmgr_psoc *psoc,
				enum QDF_OPMODE mode,
				uint8_t session_id)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	QDF_STATUS qdf_status;
	bool mcc_mode;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
		return QDF_STATUS_E_EMPTY;
	}

	qdf_status = policy_mgr_check_conn_with_mode_and_vdev_id(psoc,
			policy_mgr_convert_device_mode_to_qdf_type(mode),
			session_id);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		policy_mgr_debug("No connection with mode:%d vdev_id:%d",
			policy_mgr_convert_device_mode_to_qdf_type(mode),
			session_id);
		return qdf_status;
	}

	switch (mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_SAP_MODE:
	case QDF_NAN_DISC_MODE:
	case QDF_NDI_MODE:
		if (pm_ctx->no_of_active_sessions[mode])
			pm_ctx->no_of_active_sessions[mode]--;
		break;
	default:
		break;
	}

	if (mode != QDF_NAN_DISC_MODE &&
	    pm_ctx->dp_cbacks.hdd_v2_flow_pool_unmap)
		pm_ctx->dp_cbacks.hdd_v2_flow_pool_unmap(session_id);

	if (mode == QDF_NDI_MODE &&
	    pm_ctx->hdd_cbacks.wlan_hdd_indicate_active_ndp_cnt)
		pm_ctx->hdd_cbacks.wlan_hdd_indicate_active_ndp_cnt(
				psoc, session_id,
				pm_ctx->no_of_active_sessions[mode]);

	policy_mgr_debug("No.# of active sessions for mode %d = %d",
		mode, pm_ctx->no_of_active_sessions[mode]);

	policy_mgr_decr_connection_count(psoc, session_id);

	/* Notify tdls */
	if (pm_ctx->tdls_cbacks.tdls_notify_decrement_session)
		pm_ctx->tdls_cbacks.tdls_notify_decrement_session(psoc);
	/* Enable LRO/GRO if there no concurrency */
	if ((policy_mgr_get_connection_count(psoc) == 0) ||
	    ((policy_mgr_mode_specific_connection_count(psoc,
							PM_STA_MODE,
							NULL) == 1) &&
	     (policy_mgr_mode_specific_connection_count(psoc,
							PM_SAP_MODE,
							NULL) == 0) &&
	     (policy_mgr_mode_specific_connection_count(psoc,
							PM_P2P_CLIENT_MODE,
							NULL) == 0) &&
	     (policy_mgr_mode_specific_connection_count(psoc,
							PM_P2P_GO_MODE,
							NULL) == 0) &&
	     (policy_mgr_mode_specific_connection_count(psoc,
							PM_NDI_MODE,
							NULL) == 0))) {
		if (pm_ctx->dp_cbacks.hdd_disable_rx_ol_in_concurrency)
			pm_ctx->dp_cbacks.hdd_disable_rx_ol_in_concurrency(false);
	};

	/* Disable RPS if SAP interface has come up */
	if (policy_mgr_mode_specific_connection_count(psoc, PM_SAP_MODE, NULL)
		== 0) {
		if (pm_ctx->dp_cbacks.hdd_set_rx_mode_rps_cb)
			pm_ctx->dp_cbacks.hdd_set_rx_mode_rps_cb(false);
	}

	policy_mgr_dump_current_concurrency(psoc);

	/*
	 * Check mode of entry being removed. Update mcc_mode only when STA
	 * or SAP since IPA only cares about these two
	 */
	if (mode == QDF_STA_MODE || mode == QDF_SAP_MODE) {
		mcc_mode = policy_mgr_current_concurrency_is_mcc(psoc);

		if (pm_ctx->dp_cbacks.hdd_ipa_set_mcc_mode_cb)
			pm_ctx->dp_cbacks.hdd_ipa_set_mcc_mode_cb(mcc_mode);
	}
	/*
	 * When STA disconnected, we need to move DFS SAP
	 * to Non-DFS if g_sta_sap_scc_on_dfs_chan enabled.
	 * The same if g_sta_sap_scc_on_lte_coex_chan enabled,
	 * need to move SAP on unsafe channel to safe channel.
	 * The flag will be checked by
	 * policy_mgr_is_sap_restart_required_after_sta_disconnect.
	 */
	if (mode == QDF_STA_MODE || mode == QDF_P2P_CLIENT_MODE)
		pm_ctx->do_sap_unsafe_ch_check = true;

	if (mode == QDF_SAP_MODE || mode == QDF_P2P_GO_MODE ||
	    mode == QDF_STA_MODE || mode == QDF_P2P_CLIENT_MODE)
		policy_mgr_update_dfs_master_dynamic_enabled(psoc, session_id);

	return qdf_status;
}

QDF_STATUS policy_mgr_incr_connection_count(struct wlan_objmgr_psoc *psoc,
					    uint32_t vdev_id,
					    enum QDF_OPMODE op_mode)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index;
	struct policy_mgr_vdev_entry_info conn_table_entry = {0};
	enum policy_mgr_chain_mode chain_mask = POLICY_MGR_ONE_ONE;
	uint8_t nss_2g = 0, nss_5g = 0;
	enum policy_mgr_con_mode mode;
	uint32_t ch_freq;
	uint32_t nss = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool update_conn = true;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("context is NULL");
		return status;
	}

	conn_index = policy_mgr_get_connection_count(psoc);
	if (pm_ctx->cfg.max_conc_cxns < conn_index) {
		policy_mgr_err("exceeded max connection limit %d",
			pm_ctx->cfg.max_conc_cxns);
		return status;
	}

	if (op_mode == QDF_NAN_DISC_MODE) {
		status = wlan_nan_get_connection_info(psoc, &conn_table_entry);
		if (QDF_IS_STATUS_ERROR(status)) {
			policy_mgr_err("Can't get NAN Connection info");
			return status;
		}
	} else if (pm_ctx->wma_cbacks.wma_get_connection_info) {
		status = pm_ctx->wma_cbacks.wma_get_connection_info(
				vdev_id, &conn_table_entry);
		if (QDF_STATUS_SUCCESS != status) {
			policy_mgr_err("can't find vdev_id %d in connection table",
			vdev_id);
			return status;
		}
	} else {
		policy_mgr_err("wma_get_connection_info is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mode = policy_mgr_get_mode(conn_table_entry.type,
					conn_table_entry.sub_type);
	ch_freq = conn_table_entry.mhz;
	status = policy_mgr_get_nss_for_vdev(psoc, mode, &nss_2g, &nss_5g);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if ((WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq) && nss_2g > 1) ||
		    (WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq) && nss_5g > 1))
			chain_mask = POLICY_MGR_TWO_TWO;
		else
			chain_mask = POLICY_MGR_ONE_ONE;
		nss = (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq)) ? nss_2g : nss_5g;
	} else {
		policy_mgr_err("Error in getting nss");
	}

	if (mode == PM_STA_MODE || mode == PM_P2P_CLIENT_MODE)
		update_conn = false;

	/* add the entry */
	policy_mgr_update_conc_list(psoc, conn_index,
			mode,
			ch_freq,
			policy_mgr_get_bw(conn_table_entry.chan_width),
			conn_table_entry.mac_id,
			chain_mask,
			nss, vdev_id, true, update_conn,
			conn_table_entry.ch_flagext);
	policy_mgr_debug("Add at idx:%d vdev %d mac=%d",
		conn_index, vdev_id,
		conn_table_entry.mac_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_decr_connection_count(struct wlan_objmgr_psoc *psoc,
					uint32_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t conn_index = 0, next_conn_index = 0;
	bool found = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool panic = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return status;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (vdev_id == pm_conc_connection_list[conn_index].vdev_id) {
			/* debug msg */
			found = true;
			break;
		}
		conn_index++;
	}
	if (!found) {
		policy_mgr_err("can't find vdev_id %d in pm_conc_connection_list",
			vdev_id);
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
		return status;
	}
	next_conn_index = conn_index + 1;
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(next_conn_index)) {
		pm_conc_connection_list[conn_index].vdev_id =
			pm_conc_connection_list[next_conn_index].vdev_id;
		pm_conc_connection_list[conn_index].mode =
			pm_conc_connection_list[next_conn_index].mode;
		pm_conc_connection_list[conn_index].mac =
			pm_conc_connection_list[next_conn_index].mac;
		pm_conc_connection_list[conn_index].freq =
			pm_conc_connection_list[next_conn_index].freq;
		pm_conc_connection_list[conn_index].bw =
			pm_conc_connection_list[next_conn_index].bw;
		pm_conc_connection_list[conn_index].chain_mask =
			pm_conc_connection_list[next_conn_index].chain_mask;
		pm_conc_connection_list[conn_index].original_nss =
			pm_conc_connection_list[next_conn_index].original_nss;
		pm_conc_connection_list[conn_index].in_use =
			pm_conc_connection_list[next_conn_index].in_use;
		pm_conc_connection_list[conn_index].ch_flagext =
			pm_conc_connection_list[next_conn_index].ch_flagext;
		conn_index++;
		next_conn_index++;
	}

	/* clean up the entry */
	qdf_mem_zero(&pm_conc_connection_list[next_conn_index - 1],
		sizeof(*pm_conc_connection_list));

	conn_index = 0;
	while (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
		if (vdev_id == pm_conc_connection_list[conn_index].vdev_id) {
			panic = true;
			break;
		}
		conn_index++;
	}

	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	if (panic) {
		policy_mgr_err("dup entry occur");
		policy_mgr_debug_alert();
	}
	if (pm_ctx->conc_cbacks.connection_info_update)
		pm_ctx->conc_cbacks.connection_info_update();

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_map_concurrency_mode(enum QDF_OPMODE *old_mode,
				     enum policy_mgr_con_mode *new_mode)
{
	bool status = true;

	switch (*old_mode) {

	case QDF_STA_MODE:
		*new_mode = PM_STA_MODE;
		break;
	case QDF_SAP_MODE:
		*new_mode = PM_SAP_MODE;
		break;
	case QDF_P2P_CLIENT_MODE:
		*new_mode = PM_P2P_CLIENT_MODE;
		break;
	case QDF_P2P_GO_MODE:
		*new_mode = PM_P2P_GO_MODE;
		break;
	case QDF_NAN_DISC_MODE:
		*new_mode = PM_NAN_DISC_MODE;
		break;
	case QDF_NDI_MODE:
		*new_mode = PM_NDI_MODE;
		break;
	default:
		*new_mode = PM_MAX_NUM_OF_MODE;
		status = false;
		break;
	}

	return status;
}

uint32_t policy_mgr_get_mode_specific_conn_info(
		struct wlan_objmgr_psoc *psoc,
		uint32_t *ch_freq_list, uint8_t *vdev_id,
		enum policy_mgr_con_mode mode)
{

	uint32_t count = 0, index = 0;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}
	if (!vdev_id) {
		policy_mgr_err("Null pointer error");
		return count;
	}

	count = policy_mgr_mode_specific_connection_count(
				psoc, mode, list);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	if (count == 1) {
		if (ch_freq_list)
			*ch_freq_list =
				pm_conc_connection_list[list[index]].freq;
		*vdev_id =
			pm_conc_connection_list[list[index]].vdev_id;
	} else {
		for (index = 0; index < count; index++) {
			if (ch_freq_list)
				ch_freq_list[index] =
			pm_conc_connection_list[list[index]].freq;

			vdev_id[index] =
			pm_conc_connection_list[list[index]].vdev_id;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

bool policy_mgr_max_concurrent_connections_reached(
		struct wlan_objmgr_psoc *psoc)
{
	uint8_t i = 0, j = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (pm_ctx) {
		for (i = 0; i < QDF_MAX_NO_OF_MODE; i++)
			j += pm_ctx->no_of_active_sessions[i];
		return j >
			(pm_ctx->cfg.max_conc_cxns - 1);
	}

	return false;
}

static bool policy_mgr_is_sub_20_mhz_enabled(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	return pm_ctx->user_cfg.sub_20_mhz_enabled;
}

/**
 * policy_mgr_allow_wapi_concurrency() - Check if WAPI concurrency is allowed
 * @pm_ctx: policy_mgr_psoc_priv_obj policy mgr context
 *
 * This routine is called to check vdev security mode allowed in concurrency.
 * At present, WAPI security mode is not allowed to run concurrency with any
 * other vdev if the hardware doesn't support WAPI concurrency.
 *
 * Return: true - allow
 */
static bool
policy_mgr_allow_wapi_concurrency(struct policy_mgr_psoc_priv_obj *pm_ctx)
{
	struct wlan_objmgr_pdev *pdev = pm_ctx->pdev;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;

	if (!pdev) {
		policy_mgr_debug("pdev is Null");
		return false;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return false;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}

	if (!wmi_service_enabled(wmi_handle,
				 wmi_service_wapi_concurrency_supported) &&
	    mlme_is_wapi_sta_active(pdev) &&
	    policy_mgr_get_connection_count(pm_ctx->psoc) > 0)
		return false;

	return true;
}

#ifdef FEATURE_FOURTH_CONNECTION
static bool policy_mgr_is_concurrency_allowed_4_port(
					struct wlan_objmgr_psoc *psoc,
					enum policy_mgr_con_mode mode,
					uint32_t ch_freq,
					struct policy_mgr_pcl_list pcl)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t sap_cnt, go_cnt;

	/* new STA may just have ssid, no channel until bssid assigned */
	if (ch_freq == 0 && mode == PM_STA_MODE)
		return true;

	sap_cnt = policy_mgr_mode_specific_connection_count(psoc,
							    PM_SAP_MODE, NULL);

	go_cnt = policy_mgr_mode_specific_connection_count(psoc,
							   PM_P2P_GO_MODE, NULL);
	if (sap_cnt || go_cnt) {
		pm_ctx = policy_mgr_get_context(psoc);
		if (!pm_ctx) {
			policy_mgr_err("context is NULL");
			return false;
		}

		if (policy_mgr_get_mcc_to_scc_switch_mode(psoc) !=
		    QDF_MCC_TO_SCC_SWITCH_FORCE_WITHOUT_DISCONNECTION) {
			policy_mgr_err("couldn't start 4th port for bad force scc cfg");
			return false;
		}
		if (pm_ctx->cfg.dual_mac_feature ||
		    !pm_ctx->cfg.sta_sap_scc_on_dfs_chnl  ||
		    !pm_ctx->cfg.sta_sap_scc_on_lte_coex_chnl) {
			policy_mgr_err(
				"Couldn't start 4th port for bad cfg of dual mac, dfs scc, lte coex scc");
			return false;
		}

		for (i = 0; i < pcl.pcl_len; i++)
			if (ch_freq == pcl.pcl_list[i])
				return true;

		policy_mgr_err("4th port failed on ch freq %d with mode %d",
			       ch_freq, mode);

		return false;
	}

	return true;
}
#else
static inline bool policy_mgr_is_concurrency_allowed_4_port(
				struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				uint32_t ch_freq,
				struct policy_mgr_pcl_list pcl)
{return false; }
#endif

/**
 * policy_mgr_allow_multiple_sta_connections() - check whether multiple STA
 * concurrency is allowed and F/W supported
 * @psoc: Pointer to soc
 * @second_sta_freq: 2nd STA channel frequency
 * @first_sta_freq: 1st STA channel frequency
 *
 *  Return: true if supports else false.
 */
static bool policy_mgr_allow_multiple_sta_connections(struct wlan_objmgr_psoc *psoc,
					       uint32_t second_sta_freq,
					       uint32_t first_sta_freq)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}
	if (!wmi_service_enabled(wmi_handle,
				 wmi_service_sta_plus_sta_support)) {
		policy_mgr_rl_debug("STA+STA is not supported");
		return false;
	}

	return true;
}

#if defined(CONFIG_BAND_6GHZ) && defined(WLAN_FEATURE_11AX)
bool policy_mgr_is_6ghz_conc_mode_supported(
	struct wlan_objmgr_psoc *psoc, enum policy_mgr_con_mode mode)
{
	if (mode == PM_STA_MODE || mode == PM_SAP_MODE ||
	    mode == PM_P2P_CLIENT_MODE || mode == PM_P2P_GO_MODE)
		return true;
	else
		return false;
}
#endif

/**
 * policy_mgr_is_6g_channel_allowed() - Check new 6Ghz connection
 * allowed or not
 * @psoc: Pointer to soc
 * @mode: new connection mode
 * @ch_freq: channel freq
 *
 * 1. Only STA/SAP are allowed on 6Ghz.
 * 2. If there is DFS beacon entity existing on 5G band, 5G+6G MCC is not
 * allowed.
 *
 *  Return: true if supports else false.
 */
static bool policy_mgr_is_6g_channel_allowed(
	struct wlan_objmgr_psoc *psoc, enum policy_mgr_con_mode mode,
	uint32_t ch_freq)
{
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info *conn;
	bool is_dfs;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	if (!WLAN_REG_IS_6GHZ_CHAN_FREQ(ch_freq)) {
		policy_mgr_rl_debug("Not a 6Ghz channel Freq");
		return true;
	}
	/* Only STA/SAP is supported on 6Ghz currently */
	if (!policy_mgr_is_6ghz_conc_mode_supported(psoc, mode)) {
		policy_mgr_rl_debug("mode %d for 6ghz not supported", mode);
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
				conn_index++) {
		conn = &pm_conc_connection_list[conn_index];
		if (!conn->in_use)
			continue;
		is_dfs = (conn->ch_flagext &
			(IEEE80211_CHAN_DFS | IEEE80211_CHAN_DFS_CFREQ2)) &&
			WLAN_REG_IS_5GHZ_CH_FREQ(conn->freq);
		if ((conn->mode == PM_SAP_MODE ||
		     conn->mode == PM_P2P_GO_MODE) &&
		    is_dfs && ch_freq != conn->freq) {
			policy_mgr_rl_debug("don't allow MCC if SAP/GO on DFS channel");
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return false;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return true;
}

bool policy_mgr_is_concurrency_allowed(struct wlan_objmgr_psoc *psoc,
				       enum policy_mgr_con_mode mode,
				       uint32_t ch_freq,
				       enum hw_mode_bandwidth bw)
{
	uint32_t num_connections = 0, count = 0, index = 0;
	bool status = false, match = false;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	bool sta_sap_scc_on_dfs_chan;
	bool go_force_scc;
	uint32_t sta_freq;
	enum channel_state chan_state;
	bool is_dfs_ch = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return status;
	}
	/* find the current connection state from pm_conc_connection_list*/
	num_connections = policy_mgr_get_connection_count(psoc);

	if (num_connections && policy_mgr_is_sub_20_mhz_enabled(psoc)) {
		policy_mgr_rl_debug("dont allow concurrency if Sub 20 MHz is enabled");
		status = false;
		goto done;
	}

	if (policy_mgr_max_concurrent_connections_reached(psoc)) {
		policy_mgr_rl_debug("Reached max concurrent connections: %d",
				    pm_ctx->cfg.max_conc_cxns);
		policy_mgr_validate_conn_info(psoc);
		goto done;
	}

	if (ch_freq) {
		chan_state = wlan_reg_get_5g_bonded_channel_state_for_freq(
					pm_ctx->pdev, ch_freq,
					policy_mgr_get_ch_width(bw));
		if (chan_state == CHANNEL_STATE_DFS)
			is_dfs_ch = true;
		/* don't allow 3rd home channel on same MAC
		 * also check for single mac target which doesn't
		 * support interbad MCC as well
		 */
		if (!policy_mgr_allow_new_home_channel(psoc, mode, ch_freq,
						       num_connections,
						       is_dfs_ch))
			goto done;

		/*
		 * 1) DFS MCC is not yet supported
		 * 2) If you already have STA connection on 5G channel then
		 *    don't allow any other persona to make connection on DFS
		 *    channel because STA 5G + DFS MCC is not allowed.
		 * 3) If STA is on 2G channel and SAP is coming up on
		 *    DFS channel then allow concurrency but make sure it is
		 *    going to DBS and send PCL to firmware indicating that
		 *    don't allow STA to roam to 5G channels.
		 * 4) On a single MAC device, if a SAP/P2PGO is already on a DFS
		 *    channel, don't allow a 2 channel as it will result
		 *    in MCC which is not allowed.
		 */
		if (!policy_mgr_is_5g_channel_allowed(psoc,
			ch_freq, list, PM_P2P_GO_MODE))
			goto done;
		if (!policy_mgr_is_5g_channel_allowed(psoc,
			ch_freq, list, PM_SAP_MODE))
			goto done;
		if (!policy_mgr_is_6g_channel_allowed(psoc, mode,
						      ch_freq))
			goto done;

		sta_sap_scc_on_dfs_chan =
			policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);
		go_force_scc = policy_mgr_go_scc_enforced(psoc);
		if ((!sta_sap_scc_on_dfs_chan && mode == PM_SAP_MODE) ||
		    (!go_force_scc && mode == PM_P2P_GO_MODE)) {
			if (is_dfs_ch)
				match = policy_mgr_disallow_mcc(psoc,
								ch_freq);
		}
		if (true == match) {
			policy_mgr_rl_debug("No MCC, SAP/GO about to come up on DFS channel");
			goto done;
		}
		if ((policy_mgr_is_hw_dbs_capable(psoc) != true) &&
		    num_connections) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq)) {
				if (policy_mgr_is_sap_p2pgo_on_dfs(psoc)) {
					policy_mgr_rl_debug("MCC not allowed: SAP/P2PGO on DFS");
					goto done;
				}
			}
		}
	}

	/* Check for STA+STA concurrency */
	count = policy_mgr_mode_specific_connection_count(psoc, PM_STA_MODE,
							  list);
	if (mode == PM_STA_MODE && count) {
		if (count >= 2) {
			policy_mgr_rl_debug("3rd STA isn't permitted");
			goto done;
		}
		sta_freq = pm_conc_connection_list[list[0]].freq;
		if (!policy_mgr_allow_multiple_sta_connections(psoc, ch_freq,
							       sta_freq))
			goto done;
	}

	if (!policy_mgr_allow_sap_go_concurrency(psoc, mode, ch_freq,
						 WLAN_INVALID_VDEV_ID)) {
		policy_mgr_rl_debug("This concurrency combination is not allowed");
		goto done;
	}
	/* don't allow two P2P GO on same band */
	if (ch_freq && mode == PM_P2P_GO_MODE && num_connections) {
		index = 0;
		count = policy_mgr_mode_specific_connection_count(
				psoc, PM_P2P_GO_MODE, list);
		qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
		while (index < count) {
			if (WLAN_REG_IS_SAME_BAND_FREQS(
			    ch_freq,
			    pm_conc_connection_list[list[index]].freq)) {
				policy_mgr_rl_debug("Don't allow P2P GO on same band");
				qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
				goto done;
			}
			index++;
		}
		qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	}

	if (!policy_mgr_allow_wapi_concurrency(pm_ctx)) {
		policy_mgr_rl_debug("Don't allow new conn when wapi security conn existing");
		goto done;
	}

	status = true;

done:
	return status;
}

bool policy_mgr_allow_concurrency(struct wlan_objmgr_psoc *psoc,
				  enum policy_mgr_con_mode mode,
				  uint32_t ch_freq,
				  enum hw_mode_bandwidth bw)
{
	QDF_STATUS status;
	struct policy_mgr_pcl_list pcl;
	bool allowed;

	qdf_mem_zero(&pcl, sizeof(pcl));
	status = policy_mgr_get_pcl(psoc, mode, pcl.pcl_list, &pcl.pcl_len,
				    pcl.weight_list,
				    QDF_ARRAY_SIZE(pcl.weight_list));
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("disallow connection:%d", status);
		return false;
	}

	allowed = policy_mgr_is_concurrency_allowed(psoc, mode, ch_freq, bw);

	/* Fourth connection concurrency check */
	if (allowed && policy_mgr_get_connection_count(psoc) == 3)
		allowed = policy_mgr_is_concurrency_allowed_4_port(
				psoc,
				mode,
				ch_freq,
				pcl);
	return allowed;
}

bool
policy_mgr_allow_concurrency_csa(struct wlan_objmgr_psoc *psoc,
				 enum policy_mgr_con_mode mode,
				 uint32_t ch_freq, uint32_t vdev_id,
				 bool forced, enum sap_csa_reason_code reason)
{
	bool allow = false;
	struct policy_mgr_conc_connection_info
			info[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t num_cxn_del = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t old_ch_freq;
	QDF_STATUS status;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return allow;
	}
	policy_mgr_debug("check concurrency_csa vdev:%d ch %d, forced %d, reason %d",
			 vdev_id, ch_freq, forced, reason);

	status = policy_mgr_get_chan_by_session_id(psoc, vdev_id,
						   &old_ch_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Failed to get channel for vdev:%d",
			       vdev_id);
		return allow;
	}
	qdf_mem_zero(info, sizeof(info));

	/*
	 * Store the connection's parameter and temporarily delete it
	 * from the concurrency table. This way the allow concurrency
	 * check can be used as though a new connection is coming up,
	 * after check, restore the connection to concurrency table.
	 *
	 * In SAP+SAP SCC case, when LTE unsafe event processing,
	 * we should remove the all SAP conn entry on the same ch before
	 * do the concurrency check. Otherwise the left SAP on old channel
	 * will cause the concurrency check failure because of dual beacon
	 * MCC not supported. for the CSA request reason code,
	 * PM_CSA_REASON_UNSAFE_CHANNEL, we remove all the SAP
	 * entry on old channel before do concurrency check.
	 *
	 * The assumption is both SAP should move to the new channel later for
	 * the reason code.
	 */
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);

	if (forced && (reason == CSA_REASON_UNSAFE_CHANNEL ||
		       reason == CSA_REASON_DCS))
		policy_mgr_store_and_del_conn_info_by_chan_and_mode(
			psoc, old_ch_freq, mode, info, &num_cxn_del);
	else
		policy_mgr_store_and_del_conn_info_by_vdev_id(
			psoc, vdev_id, info, &num_cxn_del);

	allow = policy_mgr_allow_concurrency(psoc, mode, ch_freq,
					     HW_MODE_20_MHZ);
	/* Restore the connection entry */
	if (num_cxn_del > 0)
		policy_mgr_restore_deleted_conn_info(psoc, info, num_cxn_del);
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	if (!allow)
		policy_mgr_err("CSA concurrency check failed");

	return allow;
}

/**
 * policy_mgr_get_concurrency_mode() - return concurrency mode
 * @psoc: PSOC object information
 *
 * This routine is used to retrieve concurrency mode
 *
 * Return: uint32_t value of concurrency mask
 */
uint32_t policy_mgr_get_concurrency_mode(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STA_MASK;
	}

	policy_mgr_debug("concurrency_mode: 0x%x",
			 pm_ctx->concurrency_mode);

	return pm_ctx->concurrency_mode;
}

QDF_STATUS policy_mgr_get_channel_from_scan_result(
		struct wlan_objmgr_psoc *psoc,
		void *roam_profile, uint32_t *ch_freq, uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	void *scan_cache = NULL;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_INVAL;
	}

	if (!roam_profile || !ch_freq) {
		policy_mgr_err("Invalid input parameters");
		return QDF_STATUS_E_INVAL;
	}

	if (pm_ctx->sme_cbacks.sme_get_ap_channel_from_scan) {
		status = pm_ctx->sme_cbacks.sme_get_ap_channel_from_scan
			(roam_profile, &scan_cache, ch_freq, vdev_id);
		if (status != QDF_STATUS_SUCCESS) {
			policy_mgr_err("Get AP channel failed");
			return status;
		}
	} else {
		policy_mgr_err("sme_get_ap_channel_from_scan_cache NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (pm_ctx->sme_cbacks.sme_scan_result_purge)
		status = pm_ctx->sme_cbacks.sme_scan_result_purge(scan_cache);
	else
		policy_mgr_err("sme_scan_result_purge NULL");

	return status;
}

QDF_STATUS policy_mgr_set_user_cfg(struct wlan_objmgr_psoc *psoc,
				struct policy_mgr_user_cfg *user_cfg)
{

	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}
	if (!user_cfg) {
		policy_mgr_err("Invalid User Config");
		return QDF_STATUS_E_FAILURE;
	}

	pm_ctx->user_cfg = *user_cfg;
	policy_mgr_debug("dbs_selection_plcy 0x%x",
			 pm_ctx->cfg.dbs_selection_plcy);
	policy_mgr_debug("vdev_priority_list 0x%x",
			 pm_ctx->cfg.vdev_priority_list);
	pm_ctx->cur_conc_system_pref = pm_ctx->cfg.sys_pref;

	return QDF_STATUS_SUCCESS;
}

uint32_t policy_mgr_search_and_check_for_session_conc(
		struct wlan_objmgr_psoc *psoc,
		uint8_t session_id,
		void *roam_profile)
{
	uint32_t ch_freq = 0;
	QDF_STATUS status;
	enum policy_mgr_con_mode mode;
	bool ret;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return ch_freq;
	}

	if (pm_ctx->hdd_cbacks.get_mode_for_non_connected_vdev) {
		mode = pm_ctx->hdd_cbacks.get_mode_for_non_connected_vdev(
			psoc, session_id);
		if (PM_MAX_NUM_OF_MODE == mode) {
			policy_mgr_err("Invalid mode");
			return ch_freq;
		}
	} else
		return ch_freq;

	status = policy_mgr_get_channel_from_scan_result(
			psoc, roam_profile, &ch_freq, session_id);
	if (QDF_STATUS_SUCCESS != status || ch_freq == 0) {
		policy_mgr_err("status: %d ch_freq: %d", status, ch_freq);
		return 0;
	}

	/* Take care of 160MHz and 80+80Mhz later */
	ret = policy_mgr_allow_concurrency(psoc, mode, ch_freq,
					   HW_MODE_20_MHZ);
	if (false == ret) {
		policy_mgr_err("Connection failed due to conc check fail");
		return 0;
	}

	return ch_freq;
}

/**
 * policy_mgr_is_two_connection_mcc() - Check if MCC scenario
 * when there are two connections
 *
 * If if MCC scenario when there are two connections
 *
 * Return: true or false
 */
static bool policy_mgr_is_two_connection_mcc(void)
{
	return ((pm_conc_connection_list[0].freq !=
		 pm_conc_connection_list[1].freq) &&
		(pm_conc_connection_list[0].mac ==
		 pm_conc_connection_list[1].mac) &&
		(pm_conc_connection_list[0].freq <=
		 WLAN_REG_MAX_24GHZ_CHAN_FREQ) &&
		(pm_conc_connection_list[1].freq <=
		 WLAN_REG_MAX_24GHZ_CHAN_FREQ)) ? true : false;
}

/**
 * policy_mgr_is_three_connection_mcc() - Check if MCC scenario
 * when there are three connections
 *
 * If if MCC scenario when there are three connections
 *
 * Return: true or false
 */
static bool policy_mgr_is_three_connection_mcc(void)
{
	return (((pm_conc_connection_list[0].freq !=
		  pm_conc_connection_list[1].freq) ||
		 (pm_conc_connection_list[0].freq !=
		  pm_conc_connection_list[2].freq) ||
		 (pm_conc_connection_list[1].freq !=
		  pm_conc_connection_list[2].freq)) &&
		(pm_conc_connection_list[0].freq <=
		 WLAN_REG_MAX_24GHZ_CHAN_FREQ) &&
		(pm_conc_connection_list[1].freq <=
		 WLAN_REG_MAX_24GHZ_CHAN_FREQ) &&
		(pm_conc_connection_list[2].freq <=
		 WLAN_REG_MAX_24GHZ_CHAN_FREQ)) ? true : false;
}

bool policy_mgr_is_mcc_in_24G(struct wlan_objmgr_psoc *psoc)
{
	uint32_t num_connections = 0;
	bool is_24G_mcc = false;

	num_connections = policy_mgr_get_connection_count(psoc);

	switch (num_connections) {
	case 1:
		break;
	case 2:
		if (policy_mgr_is_two_connection_mcc())
			is_24G_mcc = true;
		break;
	case 3:
		if (policy_mgr_is_three_connection_mcc())
			is_24G_mcc = true;
		break;
	default:
		policy_mgr_err("unexpected num_connections value %d",
			num_connections);
		break;
	}

	return is_24G_mcc;
}

bool policy_mgr_check_for_session_conc(struct wlan_objmgr_psoc *psoc,
				       uint8_t session_id, uint32_t ch_freq)
{
	enum policy_mgr_con_mode mode;
	bool ret;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (pm_ctx->hdd_cbacks.get_mode_for_non_connected_vdev) {
		mode = pm_ctx->hdd_cbacks.get_mode_for_non_connected_vdev(
			psoc, session_id);
		if (PM_MAX_NUM_OF_MODE == mode) {
			policy_mgr_err("Invalid mode");
			return false;
		}
	} else
		return false;

	if (ch_freq == 0) {
		policy_mgr_err("Invalid channel number 0");
		return false;
	}

	/* Take care of 160MHz and 80+80Mhz later */
	ret = policy_mgr_allow_concurrency(psoc, mode, ch_freq, HW_MODE_20_MHZ);
	if (false == ret) {
		policy_mgr_err("Connection failed due to conc check fail");
		return 0;
	}

	return true;
}

/**
 * policy_mgr_change_mcc_go_beacon_interval() - Change MCC beacon interval
 * @psoc: PSOC object information
 * @vdev_id: vdev id
 * @dev_mode: device mode
 *
 * Updates the beacon parameters of the GO in MCC scenario
 *
 * Return: Success or Failure depending on the overall function behavior
 */
QDF_STATUS policy_mgr_change_mcc_go_beacon_interval(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id, enum QDF_OPMODE dev_mode)
{
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	policy_mgr_debug("UPDATE Beacon Params");

	if (QDF_SAP_MODE == dev_mode) {
		if (pm_ctx->sme_cbacks.sme_change_mcc_beacon_interval
		    ) {
			status = pm_ctx->sme_cbacks.
				sme_change_mcc_beacon_interval(vdev_id);
			if (status == QDF_STATUS_E_FAILURE) {
				policy_mgr_err("Failed to update Beacon Params");
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			policy_mgr_err("sme_change_mcc_beacon_interval callback is NULL");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

struct policy_mgr_conc_connection_info *policy_mgr_get_conn_info(uint32_t *len)
{
	struct policy_mgr_conc_connection_info *conn_ptr =
		&pm_conc_connection_list[0];
	*len = MAX_NUMBER_OF_CONC_CONNECTIONS;

	return conn_ptr;
}

enum policy_mgr_con_mode policy_mgr_convert_device_mode_to_qdf_type(
			enum QDF_OPMODE device_mode)
{
	enum policy_mgr_con_mode mode = PM_MAX_NUM_OF_MODE;
	switch (device_mode) {
	case QDF_STA_MODE:
		mode = PM_STA_MODE;
		break;
	case QDF_P2P_CLIENT_MODE:
		mode = PM_P2P_CLIENT_MODE;
		break;
	case QDF_P2P_GO_MODE:
		mode = PM_P2P_GO_MODE;
		break;
	case QDF_SAP_MODE:
		mode = PM_SAP_MODE;
		break;
	case QDF_NAN_DISC_MODE:
		mode = PM_NAN_DISC_MODE;
		break;
	case QDF_NDI_MODE:
		mode = PM_NDI_MODE;
		break;
	default:
		policy_mgr_debug("Unsupported mode (%d)",
				 device_mode);
	}

	return mode;
}

enum QDF_OPMODE policy_mgr_get_qdf_mode_from_pm(
			enum policy_mgr_con_mode device_mode)
{
	enum QDF_OPMODE mode = QDF_MAX_NO_OF_MODE;

	switch (device_mode) {
	case PM_STA_MODE:
		mode = QDF_STA_MODE;
		break;
	case PM_SAP_MODE:
		mode = QDF_SAP_MODE;
		break;
	case PM_P2P_CLIENT_MODE:
		mode = QDF_P2P_CLIENT_MODE;
		break;
	case PM_P2P_GO_MODE:
		mode = QDF_P2P_GO_MODE;
		break;
	case PM_NAN_DISC_MODE:
		mode = QDF_NAN_DISC_MODE;
		break;
	case PM_NDI_MODE:
		mode = QDF_NDI_MODE;
		break;
	default:
		policy_mgr_debug("Unsupported policy mgr mode (%d)",
				 device_mode);
	}
	return mode;
}

QDF_STATUS policy_mgr_mode_specific_num_open_sessions(
		struct wlan_objmgr_psoc *psoc, enum QDF_OPMODE mode,
		uint8_t *num_sessions)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	*num_sessions = pm_ctx->no_of_open_sessions[mode];
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS policy_mgr_mode_specific_num_active_sessions(
		struct wlan_objmgr_psoc *psoc, enum QDF_OPMODE mode,
		uint8_t *num_sessions)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return QDF_STATUS_E_FAILURE;
	}

	*num_sessions = pm_ctx->no_of_active_sessions[mode];
	return QDF_STATUS_SUCCESS;
}

/**
 * policy_mgr_concurrent_open_sessions_running() - Checks for
 * concurrent open session
 * @psoc: PSOC object information
 *
 * Checks if more than one open session is running for all the allowed modes
 * in the driver
 *
 * Return: True if more than one open session exists, False otherwise
 */
bool policy_mgr_concurrent_open_sessions_running(
	struct wlan_objmgr_psoc *psoc)
{
	uint8_t i = 0;
	uint8_t j = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return false;
	}

	for (i = 0; i < QDF_MAX_NO_OF_MODE; i++)
		j += pm_ctx->no_of_open_sessions[i];

	return j > 1;
}

/**
 * policy_mgr_concurrent_beaconing_sessions_running() - Checks
 * for concurrent beaconing entities
 * @psoc: PSOC object information
 *
 * Checks if multiple beaconing sessions are running i.e., if SAP or GO
 * are beaconing together
 *
 * Return: True if multiple entities are beaconing together, False otherwise
 */
bool policy_mgr_concurrent_beaconing_sessions_running(
	struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid context");
		return false;
	}

	return (pm_ctx->no_of_open_sessions[QDF_SAP_MODE] +
		pm_ctx->no_of_open_sessions[QDF_P2P_GO_MODE]
		> 1) ? true : false;
}


void policy_mgr_clear_concurrent_session_count(struct wlan_objmgr_psoc *psoc)
{
	uint8_t i = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (pm_ctx) {
		for (i = 0; i < QDF_MAX_NO_OF_MODE; i++)
			pm_ctx->no_of_active_sessions[i] = 0;
	}
}

bool policy_mgr_is_multiple_active_sta_sessions(struct wlan_objmgr_psoc *psoc)
{
	return policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, NULL) > 1;
}

bool policy_mgr_is_sta_present_on_dfs_channel(struct wlan_objmgr_psoc *psoc,
					      uint8_t *vdev_id,
					      qdf_freq_t *ch_freq,
					      enum hw_mode_bandwidth *ch_width)
{
	struct policy_mgr_conc_connection_info *conn_info;
	bool status = false;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    (conn_info->mode == PM_STA_MODE ||
		     conn_info->mode == PM_P2P_CLIENT_MODE) &&
		    (wlan_reg_is_dfs_for_freq(pm_ctx->pdev, conn_info->freq) ||
		     (wlan_reg_is_5ghz_ch_freq(conn_info->freq) &&
		      conn_info->bw == HW_MODE_160_MHZ))) {
			*vdev_id = conn_info->vdev_id;
			*ch_freq = pm_conc_connection_list[conn_index].freq;
			*ch_width = conn_info->bw;
			status = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

bool policy_mgr_is_sta_present_on_freq(struct wlan_objmgr_psoc *psoc,
				       uint8_t *vdev_id,
				       qdf_freq_t ch_freq,
				       enum hw_mode_bandwidth *ch_width)
{
	struct policy_mgr_conc_connection_info *conn_info;
	bool status = false;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    (conn_info->mode == PM_STA_MODE ||
		     conn_info->mode == PM_P2P_CLIENT_MODE) &&
		    ch_freq == conn_info->freq) {
			*vdev_id = conn_info->vdev_id;
			*ch_width = conn_info->bw;
			status = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

bool policy_mgr_is_sta_gc_active_on_mac(struct wlan_objmgr_psoc *psoc,
					uint8_t mac_id)
{
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t index, count;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	count = policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, list);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (index = 0; index < count; index++) {
		if (mac_id == pm_conc_connection_list[list[index]].mac) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	count = policy_mgr_mode_specific_connection_count(
		psoc, PM_P2P_CLIENT_MODE, list);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (index = 0; index < count; index++) {
		if (mac_id == pm_conc_connection_list[list[index]].mac) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return false;
}

/**
 * policy_mgr_is_sta_active_connection_exists() - Check if a STA
 * connection is active
 * @psoc: PSOC object information
 *
 * Checks if there is atleast one active STA connection in the driver
 *
 * Return: True if an active STA session is present, False otherwise
 */
bool policy_mgr_is_sta_active_connection_exists(
	struct wlan_objmgr_psoc *psoc)
{
	return (!policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, NULL)) ? false : true;
}

bool policy_mgr_is_any_nondfs_chnl_present(struct wlan_objmgr_psoc *psoc,
					   uint32_t *ch_freq)
{
	bool status = false;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		if (pm_conc_connection_list[conn_index].in_use &&
		    !wlan_reg_is_dfs_for_freq(pm_ctx->pdev,
		    pm_conc_connection_list[conn_index].freq)) {
			*ch_freq = pm_conc_connection_list[conn_index].freq;
			status = true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

uint32_t policy_mgr_get_dfs_beaconing_session_id(
		struct wlan_objmgr_psoc *psoc)
{
	uint32_t session_id = WLAN_UMAC_VDEV_ID_MAX;
	struct policy_mgr_conc_connection_info *conn_info;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return session_id;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    WLAN_REG_IS_5GHZ_CH_FREQ(conn_info->freq) &&
		    (conn_info->ch_flagext & (IEEE80211_CHAN_DFS |
					      IEEE80211_CHAN_DFS_CFREQ2)) &&
		    (conn_info->mode == PM_SAP_MODE ||
		     conn_info->mode == PM_P2P_GO_MODE)) {
			session_id =
				pm_conc_connection_list[conn_index].vdev_id;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return session_id;
}

bool policy_mgr_is_any_dfs_beaconing_session_present(
		struct wlan_objmgr_psoc *psoc, qdf_freq_t *ch_freq,
		enum hw_mode_bandwidth *ch_width)
{
	struct policy_mgr_conc_connection_info *conn_info;
	bool status = false;
	uint32_t conn_index = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use &&
		    (conn_info->mode == PM_SAP_MODE ||
		     conn_info->mode == PM_P2P_GO_MODE) &&
		     (wlan_reg_is_dfs_for_freq(pm_ctx->pdev, conn_info->freq) ||
		      (wlan_reg_is_5ghz_ch_freq(conn_info->freq) &&
		      conn_info->bw == HW_MODE_160_MHZ))) {
			*ch_freq = pm_conc_connection_list[conn_index].freq;
			*ch_width = conn_info->bw;
			status = true;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return status;
}

bool policy_mgr_scan_trim_5g_chnls_for_dfs_ap(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	qdf_freq_t dfs_ch_frq = 0;
	uint8_t vdev_id;
	enum hw_mode_bandwidth ch_width;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	if (policy_mgr_is_sta_present_on_dfs_channel(psoc, &vdev_id,
						     &dfs_ch_frq,
						     &ch_width)) {
		policymgr_nofl_err("DFS STA present vdev_id %d ch_feq %d ch_width %d",
				   vdev_id, dfs_ch_frq, ch_width);
		return false;
	}

	policy_mgr_is_any_dfs_beaconing_session_present(psoc, &dfs_ch_frq,
							&ch_width);
	if (!dfs_ch_frq)
		return false;
	/*
	 * 1) if agile & DFS scans are supportet
	 * 2) if hardware is DBS capable
	 * 3) if current hw mode is non-dbs
	 * if all above 3 conditions are true then don't skip any
	 * channel from scan list
	 */
	if (policy_mgr_is_hw_dbs_capable(psoc) &&
	    !policy_mgr_is_current_hwmode_dbs(psoc) &&
	    policy_mgr_get_dbs_plus_agile_scan_config(psoc) &&
	    policy_mgr_get_single_mac_scan_with_dfs_config(psoc))
		return false;

	policy_mgr_debug("scan skip 5g chan due to dfs ap(ch %d / ch_width %d) present",
			 dfs_ch_frq, ch_width);

	return true;
}

QDF_STATUS policy_mgr_get_nss_for_vdev(struct wlan_objmgr_psoc *psoc,
				enum policy_mgr_con_mode mode,
				uint8_t *nss_2g, uint8_t *nss_5g)
{
	enum QDF_OPMODE dev_mode;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	dev_mode = policy_mgr_get_qdf_mode_from_pm(mode);
	if (dev_mode == QDF_MAX_NO_OF_MODE)
		return  QDF_STATUS_E_FAILURE;
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	if (pm_ctx->sme_cbacks.sme_get_nss_for_vdev) {
		pm_ctx->sme_cbacks.sme_get_nss_for_vdev(
			dev_mode, nss_2g, nss_5g);

	} else {
		policy_mgr_err("sme_get_nss_for_vdev callback is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void policy_mgr_dump_connection_status_info(struct wlan_objmgr_psoc *psoc)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		policy_mgr_debug("%d: use:%d vdev:%d mode:%d mac:%d freq:%d orig chainmask:%d orig nss:%d bw:%d, ch_flags %0X",
				 i, pm_conc_connection_list[i].in_use,
				 pm_conc_connection_list[i].vdev_id,
				 pm_conc_connection_list[i].mode,
				 pm_conc_connection_list[i].mac,
				 pm_conc_connection_list[i].freq,
				 pm_conc_connection_list[i].chain_mask,
				 pm_conc_connection_list[i].original_nss,
				 pm_conc_connection_list[i].bw,
				 pm_conc_connection_list[i].ch_flagext);
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	policy_mgr_validate_conn_info(psoc);
}

bool policy_mgr_is_any_mode_active_on_band_along_with_session(
						struct wlan_objmgr_psoc *psoc,
						uint8_t session_id,
						enum policy_mgr_band band)
{
	uint32_t i;
	bool status = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		status = false;
		goto send_status;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		switch (band) {
		case POLICY_MGR_BAND_24:
			if ((pm_conc_connection_list[i].vdev_id != session_id)
			&& (pm_conc_connection_list[i].in_use) &&
			(WLAN_REG_IS_24GHZ_CH_FREQ(
			pm_conc_connection_list[i].freq))) {
				status = true;
				goto release_mutex_and_send_status;
			}
			break;
		case POLICY_MGR_BAND_5:
			if ((pm_conc_connection_list[i].vdev_id != session_id)
			&& (pm_conc_connection_list[i].in_use) &&
			(WLAN_REG_IS_5GHZ_CH_FREQ(
			pm_conc_connection_list[i].freq))) {
				status = true;
				goto release_mutex_and_send_status;
			}
			break;
		default:
			policy_mgr_err("Invalidband option:%d", band);
			status = false;
			goto release_mutex_and_send_status;
		}
	}
release_mutex_and_send_status:
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
send_status:
	return status;
}

QDF_STATUS policy_mgr_get_chan_by_session_id(struct wlan_objmgr_psoc *psoc,
					     uint8_t session_id,
					     uint32_t *ch_freq)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if ((pm_conc_connection_list[i].vdev_id == session_id) &&
		    (pm_conc_connection_list[i].in_use)) {
			*ch_freq = pm_conc_connection_list[i].freq;
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS policy_mgr_get_mac_id_by_session_id(struct wlan_objmgr_psoc *psoc,
					       uint8_t session_id,
					       uint8_t *mac_id)
{
	uint32_t i;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if ((pm_conc_connection_list[i].vdev_id == session_id) &&
		    (pm_conc_connection_list[i].in_use)) {
			*mac_id = pm_conc_connection_list[i].mac;
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return QDF_STATUS_E_FAILURE;
}

uint32_t policy_mgr_get_sap_go_count_on_mac(struct wlan_objmgr_psoc *psoc,
					    uint32_t *list, uint8_t mac_id)
{
	uint32_t conn_index;
	uint32_t count = 0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].mac == mac_id &&
		    pm_conc_connection_list[conn_index].in_use &&
		    (pm_conc_connection_list[conn_index].mode == PM_SAP_MODE ||
		     pm_conc_connection_list[conn_index].mode ==
		     PM_P2P_GO_MODE)) {
			if (list)
				list[count] =
				    pm_conc_connection_list[conn_index].vdev_id;
			count++;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

QDF_STATUS policy_mgr_get_mcc_session_id_on_mac(struct wlan_objmgr_psoc *psoc,
					uint8_t mac_id, uint8_t session_id,
					uint8_t *mcc_session_id)
{
	uint32_t i, ch_freq;
	QDF_STATUS status;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	status = policy_mgr_get_chan_by_session_id(psoc, session_id, &ch_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Failed to get channel for session id:%d",
			       session_id);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (pm_conc_connection_list[i].mac != mac_id)
			continue;
		if (pm_conc_connection_list[i].vdev_id == session_id)
			continue;
		/* Inter band or intra band MCC */
		if (pm_conc_connection_list[i].freq != ch_freq &&
		    pm_conc_connection_list[i].in_use) {
			*mcc_session_id = pm_conc_connection_list[i].vdev_id;
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return QDF_STATUS_SUCCESS;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return QDF_STATUS_E_FAILURE;
}

uint32_t policy_mgr_get_mcc_operating_channel(struct wlan_objmgr_psoc *psoc,
					      uint8_t session_id)
{
	uint8_t mac_id, mcc_session_id;
	QDF_STATUS status;
	uint32_t ch_freq;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return INVALID_CHANNEL_ID;
	}

	status = policy_mgr_get_mac_id_by_session_id(psoc, session_id, &mac_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get MAC ID");
		return INVALID_CHANNEL_ID;
	}

	status = policy_mgr_get_mcc_session_id_on_mac(psoc, mac_id, session_id,
			&mcc_session_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("failed to get MCC session ID");
		return INVALID_CHANNEL_ID;
	}

	status = policy_mgr_get_chan_by_session_id(psoc, mcc_session_id,
						   &ch_freq);
	if (QDF_IS_STATUS_ERROR(status)) {
		policy_mgr_err("Failed to get channel for MCC session ID:%d",
			       mcc_session_id);
		return INVALID_CHANNEL_ID;
	}

	return ch_freq;
}

bool policy_mgr_is_dnsc_set(struct wlan_objmgr_vdev *vdev)
{
	bool roffchan;

	if (!vdev) {
		policy_mgr_err("Invalid parameter");
		return false;
	}

	roffchan = wlan_vdev_mlme_cap_get(vdev, WLAN_VDEV_C_RESTRICT_OFFCHAN);

	if (roffchan)
		policy_mgr_debug("Restrict offchannel is set");

	return roffchan;
}

QDF_STATUS policy_mgr_is_chan_ok_for_dnbs(struct wlan_objmgr_psoc *psoc,
					  uint32_t ch_freq, bool *ok)
{
	uint32_t cc_count = 0, i;
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id[MAX_NUMBER_OF_CONC_CONNECTIONS];
	struct wlan_objmgr_vdev *vdev;

	if (!ok) {
		policy_mgr_err("Invalid parameter");
		return QDF_STATUS_E_INVAL;
	}

	cc_count = policy_mgr_get_mode_specific_conn_info(
			psoc, &op_ch_freq_list[cc_count],
			&vdev_id[cc_count], PM_SAP_MODE);

	if (cc_count < MAX_NUMBER_OF_CONC_CONNECTIONS)
		cc_count = cc_count +
			   policy_mgr_get_mode_specific_conn_info(
					psoc, &op_ch_freq_list[cc_count],
					&vdev_id[cc_count], PM_P2P_GO_MODE);

	if (!cc_count) {
		*ok = true;
		return QDF_STATUS_SUCCESS;
	}

	if (!ch_freq) {
		policy_mgr_err("channel is 0, cc count %d", cc_count);
		return QDF_STATUS_E_INVAL;
	}

	if (cc_count <= MAX_NUMBER_OF_CONC_CONNECTIONS) {
		for (i = 0; i < cc_count; i++) {
			vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
					psoc, vdev_id[i], WLAN_POLICY_MGR_ID);
			if (!vdev) {
				policy_mgr_err("vdev for vdev_id:%d is NULL",
					       vdev_id[i]);
				return QDF_STATUS_E_INVAL;
			}

			/**
			 * If channel passed is same as AP/GO operating
			 * channel, return true.
			 *
			 * If channel is different from operating channel but
			 * in same band, return false.
			 *
			 * If operating channel in different band
			 * (DBS capable), return true.
			 *
			 * If operating channel in different band
			 * (not DBS capable), return false.
			 */
			/* TODO: To be enhanced for SBS */
			if (policy_mgr_is_dnsc_set(vdev)) {
				if (op_ch_freq_list[i] == ch_freq) {
					*ok = true;
					wlan_objmgr_vdev_release_ref(
							vdev,
							WLAN_POLICY_MGR_ID);
					break;
				} else if (WLAN_REG_IS_SAME_BAND_FREQS(
					op_ch_freq_list[i], ch_freq)) {
					*ok = false;
					wlan_objmgr_vdev_release_ref(
							vdev,
							WLAN_POLICY_MGR_ID);
					break;
				} else if (policy_mgr_is_hw_dbs_capable(psoc)) {
					*ok = true;
					wlan_objmgr_vdev_release_ref(
							vdev,
							WLAN_POLICY_MGR_ID);
					break;
				} else {
					*ok = false;
					wlan_objmgr_vdev_release_ref(
							vdev,
							WLAN_POLICY_MGR_ID);
					break;
				}
			} else {
				*ok = true;
			}
			wlan_objmgr_vdev_release_ref(vdev, WLAN_POLICY_MGR_ID);
		}
	}

	return QDF_STATUS_SUCCESS;
}

uint32_t policy_mgr_get_hw_dbs_nss(struct wlan_objmgr_psoc *psoc,
				   struct dbs_nss *nss_dbs)
{
	int i, param;
	uint32_t dbs, sbs, tx_chain0, rx_chain0, tx_chain1, rx_chain1;
	uint32_t min_mac0_rf_chains, min_mac1_rf_chains;
	uint32_t max_rf_chains, final_max_rf_chains = HW_MODE_SS_0x0;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return final_max_rf_chains;
	}

	nss_dbs->single_mac0_band_cap = 0;
	for (i = 0; i < pm_ctx->num_dbs_hw_modes; i++) {
		param = pm_ctx->hw_mode.hw_mode_list[i];
		dbs = POLICY_MGR_HW_MODE_DBS_MODE_GET(param);
		sbs = POLICY_MGR_HW_MODE_SBS_MODE_GET(param);

		if (!dbs && !sbs && !nss_dbs->single_mac0_band_cap)
			nss_dbs->single_mac0_band_cap =
				POLICY_MGR_HW_MODE_MAC0_BAND_GET(param);

		if (dbs) {
			tx_chain0
				= POLICY_MGR_HW_MODE_MAC0_TX_STREAMS_GET(param);
			rx_chain0
				= POLICY_MGR_HW_MODE_MAC0_RX_STREAMS_GET(param);

			tx_chain1
				= POLICY_MGR_HW_MODE_MAC1_TX_STREAMS_GET(param);
			rx_chain1
				= POLICY_MGR_HW_MODE_MAC1_RX_STREAMS_GET(param);

			min_mac0_rf_chains = QDF_MIN(tx_chain0, rx_chain0);
			min_mac1_rf_chains = QDF_MIN(tx_chain1, rx_chain1);

			max_rf_chains
			= QDF_MAX(min_mac0_rf_chains, min_mac1_rf_chains);

			if (final_max_rf_chains < max_rf_chains) {
				final_max_rf_chains
					= (max_rf_chains == 2)
					? HW_MODE_SS_2x2 : HW_MODE_SS_1x1;

				nss_dbs->mac0_ss
					= (min_mac0_rf_chains == 2)
					? HW_MODE_SS_2x2 : HW_MODE_SS_1x1;

				nss_dbs->mac1_ss
					= (min_mac1_rf_chains == 2)
					? HW_MODE_SS_2x2 : HW_MODE_SS_1x1;
			}
		} else {
			continue;
		}
	}

	return final_max_rf_chains;
}

bool policy_mgr_is_scan_simultaneous_capable(struct wlan_objmgr_psoc *psoc)
{
	uint8_t dual_mac_feature = DISABLE_DBS_CXN_AND_SCAN;

	policy_mgr_get_dual_mac_feature(psoc, &dual_mac_feature);
	if ((dual_mac_feature == DISABLE_DBS_CXN_AND_SCAN) ||
	    (dual_mac_feature == ENABLE_DBS_CXN_AND_DISABLE_DBS_SCAN) ||
	    (dual_mac_feature ==
	     ENABLE_DBS_CXN_AND_DISABLE_SIMULTANEOUS_SCAN) ||
	     !policy_mgr_is_hw_dbs_capable(psoc))
		return false;

	return true;
}

void policy_mgr_set_cur_conc_system_pref(struct wlan_objmgr_psoc *psoc,
		uint8_t conc_system_pref)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);

	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}

	policy_mgr_debug("conc_system_pref %hu", conc_system_pref);
	pm_ctx->cur_conc_system_pref = conc_system_pref;
}

uint8_t policy_mgr_get_cur_conc_system_pref(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return PM_THROUGHPUT;
	}

	policy_mgr_debug("conc_system_pref %hu", pm_ctx->cur_conc_system_pref);
	return pm_ctx->cur_conc_system_pref;
}

QDF_STATUS policy_mgr_get_updated_scan_and_fw_mode_config(
		struct wlan_objmgr_psoc *psoc, uint32_t *scan_config,
		uint32_t *fw_mode_config, uint32_t dual_mac_disable_ini,
		uint32_t channel_select_logic_conc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return QDF_STATUS_E_FAILURE;
	}

	*scan_config = pm_ctx->dual_mac_cfg.cur_scan_config;
	*fw_mode_config = pm_ctx->dual_mac_cfg.cur_fw_mode_config;
	switch (dual_mac_disable_ini) {
	case DISABLE_DBS_CXN_AND_ENABLE_DBS_SCAN_WITH_ASYNC_SCAN_OFF:
		policy_mgr_debug("dual_mac_disable_ini:%d async/dbs off",
			dual_mac_disable_ini);
		WMI_DBS_CONC_SCAN_CFG_ASYNC_DBS_SCAN_SET(*scan_config, 0);
		WMI_DBS_FW_MODE_CFG_DBS_FOR_CXN_SET(*fw_mode_config, 0);
		break;
	case DISABLE_DBS_CXN_AND_ENABLE_DBS_SCAN:
		policy_mgr_debug("dual_mac_disable_ini:%d dbs_cxn off",
			dual_mac_disable_ini);
		WMI_DBS_FW_MODE_CFG_DBS_FOR_CXN_SET(*fw_mode_config, 0);
		break;
	case ENABLE_DBS_CXN_AND_ENABLE_SCAN_WITH_ASYNC_SCAN_OFF:
		policy_mgr_debug("dual_mac_disable_ini:%d async off",
			dual_mac_disable_ini);
		WMI_DBS_CONC_SCAN_CFG_ASYNC_DBS_SCAN_SET(*scan_config, 0);
		break;
	case ENABLE_DBS_CXN_AND_DISABLE_DBS_SCAN:
		policy_mgr_debug("dual_mac_disable_ini:%d ",
				 dual_mac_disable_ini);
		WMI_DBS_CONC_SCAN_CFG_DBS_SCAN_SET(*scan_config, 0);
		break;
	default:
		break;
	}

	WMI_DBS_FW_MODE_CFG_DBS_FOR_STA_PLUS_STA_SET(*fw_mode_config,
		PM_CHANNEL_SELECT_LOGIC_STA_STA_GET(channel_select_logic_conc));
	WMI_DBS_FW_MODE_CFG_DBS_FOR_STA_PLUS_P2P_SET(*fw_mode_config,
		PM_CHANNEL_SELECT_LOGIC_STA_P2P_GET(channel_select_logic_conc));

	policy_mgr_debug("*scan_config:%x ", *scan_config);
	policy_mgr_debug("*fw_mode_config:%x ", *fw_mode_config);

	return QDF_STATUS_SUCCESS;
}

bool policy_mgr_is_force_scc(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return 0;
	}

	return ((pm_ctx->cfg.mcc_to_scc_switch ==
		QDF_MCC_TO_SCC_SWITCH_FORCE_WITHOUT_DISCONNECTION) ||
		(pm_ctx->cfg.mcc_to_scc_switch ==
		QDF_MCC_TO_SCC_SWITCH_WITH_FAVORITE_CHANNEL) ||
		(pm_ctx->cfg.mcc_to_scc_switch ==
		QDF_MCC_TO_SCC_SWITCH_FORCE_PREFERRED_WITHOUT_DISCONNECTION) ||
		(pm_ctx->cfg.mcc_to_scc_switch ==
		QDF_MCC_TO_SCC_WITH_PREFERRED_BAND));
}

bool policy_mgr_is_sap_allowed_on_dfs_freq(struct wlan_objmgr_pdev *pdev,
					   uint8_t vdev_id, qdf_freq_t ch_freq)
{
	struct wlan_objmgr_psoc *psoc;
	uint32_t sta_sap_scc_on_dfs_chan;
	uint32_t sta_cnt, gc_cnt;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return false;

	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);
	sta_cnt = policy_mgr_mode_specific_connection_count(psoc,
							    PM_STA_MODE, NULL);
	gc_cnt = policy_mgr_mode_specific_connection_count(psoc,
						PM_P2P_CLIENT_MODE, NULL);

	policy_mgr_debug("sta_sap_scc_on_dfs_chan %u, sta_cnt %u, gc_cnt %u",
			 sta_sap_scc_on_dfs_chan, sta_cnt, gc_cnt);

	/* if sta_sap_scc_on_dfs_chan ini is set, DFS master capability is
	 * assumed disabled in the driver.
	 */
	if ((wlan_reg_get_channel_state_for_freq(pdev, ch_freq) ==
	    CHANNEL_STATE_DFS) &&
	    !sta_cnt && !gc_cnt && sta_sap_scc_on_dfs_chan &&
	    !policy_mgr_get_dfs_master_dynamic_enabled(psoc, vdev_id)) {
		policy_mgr_err("SAP not allowed on DFS channel if no dfs master capability!!");
		return false;
	}

	return true;
}

bool policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(
		struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t sta_sap_scc_on_dfs_chnl = 0;
	bool status = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return status;
	}

	policy_mgr_get_sta_sap_scc_on_dfs_chnl(psoc,
					       &sta_sap_scc_on_dfs_chnl);
	if (policy_mgr_is_force_scc(psoc) && sta_sap_scc_on_dfs_chnl)
		status = true;

	return status;
}

bool policy_mgr_is_special_mode_active_5g(struct wlan_objmgr_psoc *psoc,
					  enum policy_mgr_con_mode mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index;
	bool ret = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return ret;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].mode == mode &&
		    pm_conc_connection_list[conn_index].freq >=
					WLAN_REG_MIN_5GHZ_CHAN_FREQ &&
		    pm_conc_connection_list[conn_index].in_use)
			ret = true;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return ret;
}

bool policy_mgr_is_sta_connected_2g(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index;
	bool ret = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return ret;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (pm_conc_connection_list[conn_index].mode == PM_STA_MODE &&
		    pm_conc_connection_list[conn_index].freq <=
				WLAN_REG_MAX_24GHZ_CHAN_FREQ &&
		    pm_conc_connection_list[conn_index].in_use)
			ret = true;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return ret;
}

uint32_t policy_mgr_get_connection_info(struct wlan_objmgr_psoc *psoc,
					struct connection_info *info)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index, count = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return count;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
	     conn_index++) {
		if (PM_CONC_CONNECTION_LIST_VALID_INDEX(conn_index)) {
			info[count].vdev_id =
				pm_conc_connection_list[conn_index].vdev_id;
			info[count].mac_id =
				pm_conc_connection_list[conn_index].mac;
			info[count].channel = wlan_reg_freq_to_chan(
				pm_ctx->pdev,
				pm_conc_connection_list[conn_index].freq);
			info[count].ch_freq =
				pm_conc_connection_list[conn_index].freq;
			count++;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return count;
}

bool policy_mgr_allow_sap_go_concurrency(struct wlan_objmgr_psoc *psoc,
					 enum policy_mgr_con_mode mode,
					 uint32_t ch_freq,
					 uint32_t vdev_id)
{
	enum policy_mgr_con_mode con_mode;
	int id;
	uint32_t vdev, con_freq;
	bool dbs;

	if (mode != PM_SAP_MODE && mode != PM_P2P_GO_MODE)
		return true;
	if (policy_mgr_dual_beacon_on_single_mac_mcc_capable(psoc))
		return true;
	dbs = policy_mgr_is_hw_dbs_capable(psoc);
	for (id = 0; id < MAX_NUMBER_OF_CONC_CONNECTIONS; id++) {
		if (!pm_conc_connection_list[id].in_use)
			continue;
		vdev = pm_conc_connection_list[id].vdev_id;
		if (vdev_id == vdev)
			continue;
		con_mode = pm_conc_connection_list[id].mode;
		if (con_mode != PM_SAP_MODE && con_mode != PM_P2P_GO_MODE)
			continue;
		con_freq = pm_conc_connection_list[id].freq;
		if (policy_mgr_dual_beacon_on_single_mac_scc_capable(psoc) &&
		    (ch_freq == con_freq)) {
			policy_mgr_debug("SCC enabled, 2 AP on same channel, allow 2nd AP");
			return true;
		}
		if (!dbs) {
			policy_mgr_debug("DBS unsupported, mcc and scc unsupported too, don't allow 2nd AP");
			return false;
		}
		if (WLAN_REG_IS_SAME_BAND_FREQS(ch_freq, con_freq)) {
			policy_mgr_debug("DBS supported, 2 SAP on same band, reject 2nd AP");
			return false;
		}
	}

	/* Don't block the second interface */
	return true;
}

bool policy_mgr_dual_beacon_on_single_mac_scc_capable(
		struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}

	if (wmi_service_enabled(
			wmi_handle,
			wmi_service_dual_beacon_on_single_mac_scc_support)) {
		policy_mgr_debug("Dual beaconing on same channel on single MAC supported");
		return true;
	}
	policy_mgr_debug("Dual beaconing on same channel on single MAC is not supported");
	return false;
}

bool policy_mgr_dual_beacon_on_single_mac_mcc_capable(
		struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		policy_mgr_debug("Invalid WMI handle");
		return false;
	}

	if (wmi_service_enabled(
			wmi_handle,
			wmi_service_dual_beacon_on_single_mac_mcc_support)) {
		policy_mgr_debug("Dual beaconing on different channel on single MAC supported");
		return true;
	}
	policy_mgr_debug("Dual beaconing on different channel on single MAC is not supported");
	return false;
}

bool policy_mgr_sta_sap_scc_on_lte_coex_chan(
	struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint8_t scc_lte_coex = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	policy_mgr_get_sta_sap_scc_lte_coex_chnl(psoc, &scc_lte_coex);

	return scc_lte_coex;
}

#if defined(CONFIG_BAND_6GHZ) && defined(WLAN_FEATURE_11AX)
void policy_mgr_init_ap_6ghz_capable(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id,
				     enum conn_6ghz_flag ap_6ghz_capable)
{
	struct policy_mgr_conc_connection_info *conn_info;
	uint32_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum conn_6ghz_flag conn_6ghz_flag = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use && PM_SAP_MODE == conn_info->mode &&
		    vdev_id == conn_info->vdev_id) {
			conn_info->conn_6ghz_flag = ap_6ghz_capable;
			conn_info->conn_6ghz_flag |= CONN_6GHZ_FLAG_VALID;
			conn_6ghz_flag = conn_info->conn_6ghz_flag;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_debug("vdev %d init conn_6ghz_flag %x new %x",
			 vdev_id, ap_6ghz_capable, conn_6ghz_flag);
}

void policy_mgr_set_ap_6ghz_capable(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    bool set,
				    enum conn_6ghz_flag ap_6ghz_capable)
{
	struct policy_mgr_conc_connection_info *conn_info;
	uint32_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum conn_6ghz_flag conn_6ghz_flag = 0;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use && PM_SAP_MODE == conn_info->mode &&
		    vdev_id == conn_info->vdev_id) {
			if (set)
				conn_info->conn_6ghz_flag |= ap_6ghz_capable;
			else
				conn_info->conn_6ghz_flag &= ~ap_6ghz_capable;
			conn_info->conn_6ghz_flag |= CONN_6GHZ_FLAG_VALID;
			conn_6ghz_flag = conn_info->conn_6ghz_flag;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
	policy_mgr_debug("vdev %d %s conn_6ghz_flag %x new %x",
			 vdev_id, set ? "set" : "clr",
			 ap_6ghz_capable, conn_6ghz_flag);
}

bool policy_mgr_get_ap_6ghz_capable(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    uint32_t *conn_flag)
{
	struct policy_mgr_conc_connection_info *conn_info;
	uint32_t conn_index;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	enum conn_6ghz_flag conn_6ghz_flag = 0;
	bool is_6g_allowed = false;

	if (conn_flag)
		*conn_flag = 0;
	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
			conn_index++) {
		conn_info = &pm_conc_connection_list[conn_index];
		if (conn_info->in_use && PM_SAP_MODE == conn_info->mode &&
		    vdev_id == conn_info->vdev_id) {
			conn_6ghz_flag = conn_info->conn_6ghz_flag;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	/* If the vdev connection is not active, policy mgr will query legacy
	 * hdd to get sap acs and security information.
	 * The assumption is no legacy client connected for non active
	 * connection.
	 */
	if (!(conn_6ghz_flag & CONN_6GHZ_FLAG_VALID) &&
	    pm_ctx->hdd_cbacks.hdd_get_ap_6ghz_capable)
		conn_6ghz_flag = pm_ctx->hdd_cbacks.hdd_get_ap_6ghz_capable(
					psoc, vdev_id) |
					CONN_6GHZ_FLAG_NO_LEGACY_CLIENT;

	if ((conn_6ghz_flag & CONN_6GHZ_CAPABLIE) == CONN_6GHZ_CAPABLIE)
		is_6g_allowed = true;
	policy_mgr_debug("vdev %d conn_6ghz_flag %x 6ghz %s", vdev_id,
			 conn_6ghz_flag, is_6g_allowed ? "allowed" : "deny");
	if (conn_flag)
		*conn_flag = conn_6ghz_flag;

	return is_6g_allowed;
}
#endif

bool policy_mgr_is_valid_for_channel_switch(struct wlan_objmgr_psoc *psoc,
					    uint32_t ch_freq)
{
	uint32_t sta_sap_scc_on_dfs_chan;
	uint32_t sap_count;
	enum channel_state state;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	sta_sap_scc_on_dfs_chan =
			policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);
	sap_count = policy_mgr_mode_specific_connection_count(psoc,
							      PM_SAP_MODE,
							      NULL);
	state = wlan_reg_get_channel_state_for_freq(pm_ctx->pdev, ch_freq);

	policy_mgr_debug("sta_sap_scc_on_dfs_chan %u, sap_count %u, ch freq %u, state %u",
			 sta_sap_scc_on_dfs_chan, sap_count, ch_freq, state);

	if ((state == CHANNEL_STATE_ENABLE) || (sap_count == 0) ||
	    ((state == CHANNEL_STATE_DFS) && sta_sap_scc_on_dfs_chan)) {
		policy_mgr_debug("Valid channel for channel switch");
		return true;
	}

	policy_mgr_debug("Invalid channel for channel switch");
	return false;
}

bool policy_mgr_is_sta_sap_scc(struct wlan_objmgr_psoc *psoc,
			       uint32_t sap_freq)
{
	uint32_t conn_index;
	bool is_scc = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return is_scc;
	}

	if (!policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, NULL)) {
		policy_mgr_debug("There is no STA+SAP conc");
		return is_scc;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		conn_index++) {
		if (pm_conc_connection_list[conn_index].in_use &&
				(pm_conc_connection_list[conn_index].mode ==
				PM_STA_MODE ||
				pm_conc_connection_list[conn_index].mode ==
				PM_P2P_CLIENT_MODE) && (sap_freq ==
				pm_conc_connection_list[conn_index].freq)) {
			is_scc = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return is_scc;
}

bool policy_mgr_go_scc_enforced(struct wlan_objmgr_psoc *psoc)
{
	uint32_t mcc_to_scc_switch;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}
	mcc_to_scc_switch = policy_mgr_get_mcc_to_scc_switch_mode(psoc);
	if (mcc_to_scc_switch ==
	    QDF_MCC_TO_SCC_SWITCH_FORCE_PREFERRED_WITHOUT_DISCONNECTION)
		return true;

	if (pm_ctx->cfg.go_force_scc && policy_mgr_is_force_scc(psoc))
		return true;

	return false;
}

QDF_STATUS policy_mgr_update_nan_vdev_mac_info(struct wlan_objmgr_psoc *psoc,
					       uint8_t nan_vdev_id,
					       uint8_t mac_id)
{
	struct policy_mgr_hw_mode_params hw_mode = {0};
	struct policy_mgr_vdev_mac_map vdev_mac_map = {0};
	QDF_STATUS status;

	vdev_mac_map.vdev_id = nan_vdev_id;
	vdev_mac_map.mac_id = mac_id;

	status = policy_mgr_get_current_hw_mode(psoc, &hw_mode);

	if (QDF_IS_STATUS_SUCCESS(status))
		policy_mgr_update_hw_mode_conn_info(psoc, 1, &vdev_mac_map,
						    hw_mode);

	return status;
}

bool policy_mgr_is_sap_go_on_2g(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	uint32_t conn_index;
	bool ret = false;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return ret;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	for (conn_index = 0; conn_index < MAX_NUMBER_OF_CONC_CONNECTIONS;
		 conn_index++) {
		if ((pm_conc_connection_list[conn_index].mode == PM_SAP_MODE ||
		     pm_conc_connection_list[conn_index].mode == PM_P2P_GO_MODE) &&
			 pm_conc_connection_list[conn_index].freq <=
				WLAN_REG_MAX_24GHZ_CHAN_FREQ &&
			 pm_conc_connection_list[conn_index].in_use)
			ret = true;
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return ret;
}

bool policy_mgr_get_5g_scc_prefer(
	struct wlan_objmgr_psoc *psoc, enum policy_mgr_con_mode mode)
{
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	return pm_ctx->cfg.prefer_5g_scc_to_dbs & (1 << mode);
}

bool policy_mgr_is_restart_sap_required(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id,
					qdf_freq_t freq,
					tQDF_MCC_TO_SCC_SWITCH_MODE scc_mode)
{
	uint8_t i, mac = 0;
	bool restart_required = false;
	bool is_sta_p2p_cli;
	bool is_same_mac;
	bool sap_on_dfs = false;
	struct policy_mgr_psoc_priv_obj *pm_ctx;
	struct policy_mgr_conc_connection_info *connection;
	bool sta_sap_scc_on_dfs_chan;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid psoc");
		return false;
	}
	if (scc_mode == QDF_MCC_TO_SCC_SWITCH_DISABLE) {
		policy_mgr_debug("No scc required");
		return false;
	}

	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);
	connection = pm_conc_connection_list;
	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		if (connection[i].vdev_id == vdev_id &&
		    connection[i].in_use) {
			mac = connection[i].mac;

			if (WLAN_REG_IS_5GHZ_CH_FREQ(connection[i].freq) &&
			    (connection[i].ch_flagext & (IEEE80211_CHAN_DFS |
					      IEEE80211_CHAN_DFS_CFREQ2)))
				sap_on_dfs = true;
			break;
		}
	}
	if (i == MAX_NUMBER_OF_CONC_CONNECTIONS) {
		policy_mgr_err("Invalid vdev id: %d", vdev_id);
		return false;
	}
	sta_sap_scc_on_dfs_chan =
		policy_mgr_is_sta_sap_scc_allowed_on_dfs_chan(psoc);

	for (i = 0; i < MAX_NUMBER_OF_CONC_CONNECTIONS; i++) {
		is_sta_p2p_cli =
			connection[i].in_use &&
			(connection[i].mode == PM_STA_MODE ||
			connection[i].mode == PM_P2P_CLIENT_MODE);
		if (!is_sta_p2p_cli)
			continue;
		is_same_mac = connection[i].freq != freq &&
			      (connection[i].mac == mac ||
			       !policy_mgr_is_hw_dbs_capable(psoc));
		if (is_same_mac) {
			restart_required = true;
			break;
		}
		if (connection[i].freq == freq &&
		    !sta_sap_scc_on_dfs_chan && sap_on_dfs) {
			restart_required = true;
			break;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return restart_required;
}

uint8_t policy_mgr_get_roam_enabled_sta_session_id(
					struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id)
{
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t index, count;
	struct policy_mgr_psoc_priv_obj *pm_ctx;

	pm_ctx = policy_mgr_get_context(psoc);
	if (!pm_ctx) {
		policy_mgr_err("Invalid Context");
		return false;
	}

	count = policy_mgr_mode_specific_connection_count(
		psoc, PM_STA_MODE, list);
	qdf_mutex_acquire(&pm_ctx->qdf_conc_list_lock);

	for (index = 0; index < count; index++) {
		if (vdev_id == pm_conc_connection_list[list[index]].vdev_id)
			continue;
		if (MLME_IS_ROAM_INITIALIZED(
			psoc, pm_conc_connection_list[list[index]].vdev_id)) {
			qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);
			return pm_conc_connection_list[list[index]].vdev_id;
		}
	}
	qdf_mutex_release(&pm_ctx->qdf_conc_list_lock);

	return WLAN_UMAC_VDEV_ID_MAX;
}

bool policy_mgr_is_sta_mon_concurrency(struct wlan_objmgr_psoc *psoc)
{
	uint32_t conc_mode;

	if (wlan_mlme_is_sta_mon_conc_supported(psoc)) {
		conc_mode = policy_mgr_get_concurrency_mode(psoc);
		if (conc_mode & QDF_STA_MASK &&
		    conc_mode & QDF_MONITOR_MASK) {
			policy_mgr_err("STA + MON mode is UP");
			return true;
		}
	}
	return false;
}

QDF_STATUS policy_mgr_check_mon_concurrency(struct wlan_objmgr_psoc *psoc)
{
	uint8_t num_open_session = 0;

	if (policy_mgr_mode_specific_num_open_sessions(
				psoc,
				QDF_MONITOR_MODE,
				&num_open_session) != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_INVAL;

	if (num_open_session) {
		policy_mgr_err("monitor mode already exists, only one is possible");
		return QDF_STATUS_E_BUSY;
	}

	num_open_session = policy_mgr_mode_specific_connection_count(
					psoc,
					PM_SAP_MODE,
					NULL);

	if (num_open_session) {
		policy_mgr_err("cannot add monitor mode, due to SAP concurrency");
		return QDF_STATUS_E_INVAL;
	}

	/* Ensure there is only one station interface */
	if (policy_mgr_mode_specific_num_open_sessions(
				psoc,
				QDF_STA_MODE,
				&num_open_session) != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_INVAL;

	if (num_open_session > 1) {
		policy_mgr_err("cannot add monitor mode, due to %u sta interfaces",
			       num_open_session);
		return QDF_STATUS_E_INVAL;
	}

	num_open_session = policy_mgr_mode_specific_connection_count(
					psoc,
					PM_P2P_CLIENT_MODE,
					NULL);

	if (num_open_session) {
		policy_mgr_err("cannot add monitor mode, due to P2P CLIENT concurrency");
		return QDF_STATUS_E_INVAL;
	}

	num_open_session = policy_mgr_mode_specific_connection_count(
					psoc,
					PM_P2P_GO_MODE,
					NULL);

	if (num_open_session) {
		policy_mgr_err("cannot add monitor mode, due to P2P GO concurrency");
		return QDF_STATUS_E_INVAL;
	}

	num_open_session = policy_mgr_mode_specific_connection_count(
					psoc,
					PM_NAN_DISC_MODE,
					NULL);

	if (num_open_session) {
		policy_mgr_err("cannot add monitor mode, due to NAN concurrency");
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}
