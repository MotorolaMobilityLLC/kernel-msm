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
#ifndef __WLAN_POLICY_MGR_UCFG
#define __WLAN_POLICY_MGR_UCFG
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_global_obj.h"
#include "qdf_status.h"


/**
 * ucfg_policy_mgr_psoc_open() - This API sets CFGs to policy manager context
 *
 * This API pulls policy manager's context from PSOC and initialize the CFG
 * structure of policy manager.
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_psoc_open(struct wlan_objmgr_psoc *psoc);
/**
 * ucfg_policy_mgr_psoc_close() - This API resets CFGs for policy manager ctx
 *
 * This API pulls policy manager's context from PSOC and resets the CFG
 * structure of policy manager.
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
void ucfg_policy_mgr_psoc_close(struct wlan_objmgr_psoc *psoc);
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * ucfg_policy_mgr_get_mcc_scc_switch() - To mcc to scc switch setting from INI
 * @psoc: pointer to psoc
 * @mcc_scc_switch: value to be filled
 *
 * This API pulls mcc to scc switch setting which is given as part of INI and
 * stored in policy manager's CFGs.
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_mcc_scc_switch(struct wlan_objmgr_psoc *psoc,
					      uint8_t *mcc_scc_switch);
#else
static inline
QDF_STATUS ucfg_policy_mgr_get_mcc_scc_switch(struct wlan_objmgr_psoc *psoc,
					      uint8_t *mcc_scc_switch)
{
	return QDF_STATUS_SUCCESS;
}
#endif //FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * ucfg_policy_mgr_get_sys_pref() - to get system preference
 * @psoc: pointer to psoc
 * @sys_pref: value to be filled
 *
 * This API pulls the system preference for policy manager to provide
 * PCL
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_sys_pref(struct wlan_objmgr_psoc *psoc,
					uint8_t *sys_pref);
/**
 * ucfg_policy_mgr_set_sys_pref() - to set system preference
 * @psoc: pointer to psoc
 * @sys_pref: value to be applied as new INI setting
 *
 * This API is meant to override original INI setting for system pref
 * with new value which is used by policy manager to provide PCL
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_set_sys_pref(struct wlan_objmgr_psoc *psoc,
					uint8_t sys_pref);

/**
 * ucfg_policy_mgr_get_conc_rule1() - to find out if conc rule1 is enabled
 * @psoc: pointer to psoc
 * @conc_rule1: value to be filled
 *
 * This API is used to find out if conc rule-1 is enabled by user
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_conc_rule1(struct wlan_objmgr_psoc *psoc,
						uint8_t *conc_rule1);
/**
 * ucfg_policy_mgr_get_conc_rule2() - to find out if conc rule2 is enabled
 * @psoc: pointer to psoc
 * @conc_rule2: value to be filled
 *
 * This API is used to find out if conc rule-2 is enabled by user
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_conc_rule2(struct wlan_objmgr_psoc *psoc,
						uint8_t *conc_rule2);

/**
 * policy_mgr_get_chnl_select_plcy() - to get channel selection policy
 * @psoc: pointer to psoc
 * @chnl_select_plcy: value to be filled
 *
 * This API is used to find out which channel selection policy has been
 * configured
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_chnl_select_plcy(struct wlan_objmgr_psoc *psoc,
						uint32_t *chnl_select_plcy);
/**
 * policy_mgr_get_mcc_adaptive_sch() - to get mcc adaptive scheduler
 * @psoc: pointer to psoc
 * @enable_mcc_adaptive_sch: value to be filled
 *
 * This API is used to find out if mcc adaptive scheduler enabled or disabled
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_mcc_adaptive_sch(struct wlan_objmgr_psoc *psoc,
				     bool *enable_mcc_adaptive_sch);

/**
 * ucfg_policy_mgr_get_dynamic_mcc_adaptive_sch() - to get dynamic mcc adaptive
 *                                                  scheduler
 * @psoc: pointer to psoc
 * @dynamic_mcc_adaptive_sch: value to be filled
 *
 * This API is used to get dynamic mcc adaptive scheduler
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_dynamic_mcc_adaptive_sch(struct wlan_objmgr_psoc *psoc,
					     bool *dynamic_mcc_adaptive_sch);

/**
 * ucfg_policy_mgr_set_dynamic_mcc_adaptive_sch() - to set dynamic mcc adaptive
 *                                                  scheduler
 * @psoc: pointer to psoc
 * @dynamic_mcc_adaptive_sch: value to be set
 *
 * This API is used to set dynamic mcc adaptive scheduler
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_set_dynamic_mcc_adaptive_sch(struct wlan_objmgr_psoc *psoc,
					     bool dynamic_mcc_adaptive_sch);

/**
 * ucfg_policy_mgr_get_sta_cxn_5g_band() - to get STA's connection in 5G config
 *
 * @psoc: pointer to psoc
 * @enable_sta_cxn_5g_band: value to be filled
 *
 * This API is used to find out if STA connection in 5G band is allowed or
 * disallowed.
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_sta_cxn_5g_band(struct wlan_objmgr_psoc *psoc,
					       uint8_t *enable_sta_cxn_5g_band);
/**
 * ucfg_policy_mgr_get_allow_mcc_go_diff_bi() - to get information on whether GO
 *						can have diff BI than STA in MCC
 * @psoc: pointer to psoc
 * @allow_mcc_go_diff_bi: value to be filled
 *
 * This API is used to find out whether GO's BI can different than STA in MCC
 * scenario
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_allow_mcc_go_diff_bi(struct wlan_objmgr_psoc *psoc,
					 uint8_t *allow_mcc_go_diff_bi);
/**
 * ucfg_policy_mgr_get_enable_overlap_chnl() - to find out if overlap channels
 *						are enabled for SAP
 * @psoc: pointer to psoc
 * @enable_overlap_chnl: value to be filled
 *
 * This API is used to find out whether overlap channels are enabled for SAP
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_enable_overlap_chnl(struct wlan_objmgr_psoc *psoc,
					uint8_t *enable_overlap_chnl);
/**
 * ucfg_policy_mgr_get_dual_mac_feature() - to find out if DUAL MAC feature is
 *					    enabled
 * @psoc: pointer to psoc
 * @dual_mac_feature: value to be filled
 *
 * This API is used to find out whether dual mac (dual radio) specific feature
 * is enabled or not
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_dual_mac_feature(struct wlan_objmgr_psoc *psoc,
						uint8_t *dual_mac_feature);
/**
 * ucfg_policy_mgr_get_force_1x1() - to find out if 1x1 connection is enforced
 *
 * @psoc: pointer to psoc
 * @force_1x1: value to be filled
 *
 * This API is used to find out if 1x1 connection is enforced.
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_force_1x1(struct wlan_objmgr_psoc *psoc,
					 uint8_t *force_1x1);
/**
 * ucfg_policy_mgr_get_sta_sap_scc_on_dfs_chnl() - to find out if STA and SAP
 *						   SCC is allowed on DFS channel
 * @psoc: pointer to psoc
 * @sta_sap_scc_on_dfs_chnl: value to be filled
 *
 * This API is used to find out whether STA and SAP SCC is allowed on
 * DFS channels
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_sta_sap_scc_on_dfs_chnl(struct wlan_objmgr_psoc *psoc,
					    uint8_t *sta_sap_scc_on_dfs_chnl);
/**
 * ucfg_policy_mgr_get_sta_sap_scc_lte_coex_chnl() - to find out if STA & SAP
 *						     SCC is allowed on LTE COEX
 * @psoc: pointer to psoc
 * @sta_sap_scc_lte_coex: value to be filled
 *
 * This API is used to find out whether STA and SAP scc is allowed on LTE COEX
 * channel
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_sta_sap_scc_lte_coex_chnl(struct wlan_objmgr_psoc *psoc,
					      uint8_t *sta_sap_scc_lte_coex);

/**
 * ucfg_policy_mgr_get_dfs_master_dynamic_enabled() - support dfs master or not
 *  AP interface when STA+SAP(GO) concurrency
 * @psoc: pointer to psoc
 * @vdev_id: sap vdev id
 *
 * This API is used to check SAP (GO) dfs master functionality enabled or not
 * when STA+SAP(GO) concurrency.
 * If g_sta_sap_scc_on_dfs_chan is non-zero, the STA+SAP(GO) is allowed on DFS
 * channel SCC and the SAP's DFS master functionality should be enable/disable
 * according to:
 * 1. g_sta_sap_scc_on_dfs_chan is 0: function return true - dfs master
 *     capability enabled.
 * 2. g_sta_sap_scc_on_dfs_chan is 1: function return false - dfs master
 *     capability disabled.
 * 3. g_sta_sap_scc_on_dfs_chan is 2: dfs master capability based on STA on
 *     5G or not:
 *      a. 5G STA active - return false
 *      b. no 5G STA active -return true
 *
 * Return: true if dfs master functionality should be enabled.
 */
bool
ucfg_policy_mgr_get_dfs_master_dynamic_enabled(struct wlan_objmgr_psoc *psoc,
					       uint8_t vdev_id);

/**
 * ucfg_policy_mgr_init_chan_avoidance() - init channel avoidance in policy
 *					   manager
 * @psoc: pointer to psoc
 * @chan_freq_list: channel frequency list
 * @chan_cnt: channel count
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_init_chan_avoidance(struct wlan_objmgr_psoc *psoc,
				    qdf_freq_t *chan_freq_list,
				    uint16_t chan_cnt);

/**
 * ucfg_policy_mgr_get_sap_mandt_chnl() - to find out if SAP mandatory channel
 *					  support is enabled
 * @psoc: pointer to psoc
 * @sap_mandt_chnl: value to be filled
 *
 * This API is used to find out whether SAP's mandatory channel support
 * is enabled
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS ucfg_policy_mgr_get_sap_mandt_chnl(struct wlan_objmgr_psoc *psoc,
					      uint8_t *sap_mandt_chnl);
/**
 * ucfg_policy_mgr_get_indoor_chnl_marking() - to get if indoor channel can be
 *						marked as disabled
 * @psoc: pointer to psoc
 * @indoor_chnl_marking: value to be filled
 *
 * This API is used to find out whether indoor channel can be marked as disabled
 *
 * Return: QDF_STATUS_SUCCESS up on success and any other status for failure.
 */
QDF_STATUS
ucfg_policy_mgr_get_indoor_chnl_marking(struct wlan_objmgr_psoc *psoc,
					uint8_t *indoor_chnl_marking);
#endif //__WLAN_POLICY_MGR_UCFG
