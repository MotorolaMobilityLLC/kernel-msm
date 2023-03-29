/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
 * DOC: declare UCFG APIs exposed by the mlme component
 */

#ifndef _WLAN_MLME_UCFG_API_H_
#define _WLAN_MLME_UCFG_API_H_

#include <wlan_mlme_public_struct.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_cmn.h>
#include <wlan_mlme_api.h>
#include <wlan_mlme_main.h>
#include "wma_tgt_cfg.h"

/**
 * ucfg_mlme_init() - initialize mlme_ctx context.
 *
 * This function initializes the mlme context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_mlme_init(void);

/**
 * ucfg_mlme_deinit() - De initialize mlme_ctx context.
 *
 * This function De initializes mlme contex.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_mlme_deinit(void);

/**
 * ucfg_mlme_psoc_open() - MLME component Open
 * @psoc: pointer to psoc object
 *
 * Open the MLME component and initialize the MLME strucutre
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_mlme_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_mlme_psoc_close() - MLME component close
 * @psoc: pointer to psoc object
 *
 * Close the MLME component and clear the MLME structures
 *
 * Return: None
 */
void ucfg_mlme_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_mlme_pdev_open() - MLME component pdev Open
 * @pdev: pointer to pdev object
 *
 * Open the MLME component and initialize the MLME pdev strucutre
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_mlme_pdev_open(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_mlme_pdev_close() - MLME component pdev close
 * @pdev: pointer to pdev object
 *
 * close the MLME pdev information
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_mlme_pdev_close(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_mlme_global_init() - initialize global mlme ops and structure
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_mlme_global_init(void);
/**
 * ucfg_mlme_global_deinit() - deinitialize global mlme ops and structure
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_mlme_global_deinit(void);

/**
 * ucfg_mlme_cfg_chan_to_freq() - convert channel numbers to frequencies
 * @pdev: pointer to pdev object
 *
 * convert the channels numbers received as part of cfg items to
 * frequencies.
 *
 * Return: None
 */
void ucfg_mlme_cfg_chan_to_freq(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_mlme_get_power_usage() - Get the power usage info
 * @psoc: pointer to psoc object
 *
 * Return: pointer to character array of power usage
 */
static inline
char *ucfg_mlme_get_power_usage(struct wlan_objmgr_psoc *psoc)
{
	return wlan_mlme_get_power_usage(psoc);
}

/**
 * ucfg_get_tx_power() - Get the max tx power in particular band
 * @psoc: pointer to psoc object
 * @band: 2ghz/5ghz band
 *
 * Return: value of tx power in the respective band
 */
static inline
uint8_t ucfg_get_tx_power(struct wlan_objmgr_psoc *psoc, uint8_t band)
{
	return wlan_mlme_get_tx_power(psoc, band);
}

/**
 * ucfg_mlme_get_ht_cap_info() - Get the HT cap info config
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_ht_cap_info(struct wlan_objmgr_psoc *psoc,
				     struct mlme_ht_capabilities_info
				     *ht_cap_info)
{
	return wlan_mlme_get_ht_cap_info(psoc, ht_cap_info);
}

/**
 * ucfg_mlme_set_ht_cap_info() - Set the HT cap info config
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_set_ht_cap_info(struct wlan_objmgr_psoc *psoc,
				     struct mlme_ht_capabilities_info
				     ht_cap_info)
{
	return wlan_mlme_set_ht_cap_info(psoc, ht_cap_info);
}

/**
 * ucfg_mlme_get_max_amsdu_num() - get the max amsdu num
 * @psoc: pointer to psoc object
 * @value: pointer to the value where the max_amsdu num is to be filled
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS ucfg_mlme_get_max_amsdu_num(struct wlan_objmgr_psoc *psoc,
				       uint8_t *value)
{
	return wlan_mlme_get_max_amsdu_num(psoc, value);
}

/**
 * ucfg_mlme_set_max_amsdu_num() - set the max amsdu num
 * @psoc: pointer to psoc object
 * @value: value to be set for max_amsdu_num
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS ucfg_mlme_set_max_amsdu_num(struct wlan_objmgr_psoc *psoc,
				       uint8_t value)
{
	return wlan_mlme_set_max_amsdu_num(psoc, value);
}

/**
 * ucfg_mlme_get_ht_mpdu_density() - get the ht mpdu density
 * @psoc: pointer to psoc object
 * @value: pointer to the value where the ht mpdu density is to be filled
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS ucfg_mlme_get_ht_mpdu_density(struct wlan_objmgr_psoc *psoc,
					 uint8_t *value)
{
	return wlan_mlme_get_ht_mpdu_density(psoc, value);
}

/**
 * ucfg_mlme_set_ht_mpdu_density() - set the ht mpdu density
 * @psoc: pointer to psoc object
 * @value: value to be set for ht mpdu density
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS ucfg_mlme_set_ht_mpdu_density(struct wlan_objmgr_psoc *psoc,
					 uint8_t value)
{
	return wlan_mlme_set_ht_mpdu_density(psoc, value);
}

/**
 * ucfg_mlme_get_band_capability() - Get the Band capability config
 * @psoc: pointer to psoc object
 * @band_capability: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_band_capability(struct wlan_objmgr_psoc *psoc,
					 uint32_t *band_capability)
{
	return wlan_mlme_get_band_capability(psoc, band_capability);
}

/**
 * ucfg_mlme_set_band_capability() - Set the Band capability config
 * @psoc: pointer to psoc object
 * @band_capability: Value to be set from the caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_set_band_capability(struct wlan_objmgr_psoc *psoc,
					 uint32_t band_capability)
{
	return wlan_mlme_set_band_capability(psoc, band_capability);
}

/**
 * ucfg_mlme_get_prevent_link_down() - Get the prevent link down config
 * @psoc: pointer to psoc object
 * @prevent_link_down: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_prevent_link_down(struct wlan_objmgr_psoc *psoc,
					   bool *prevent_link_down)
{
	return wlan_mlme_get_prevent_link_down(psoc, prevent_link_down);
}

/**
 * ucfg_mlme_get_select_5ghz_margin() - Get the select 5Ghz margin config
 * @psoc: pointer to psoc object
 * @select_5ghz_margin: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_select_5ghz_margin(struct wlan_objmgr_psoc *psoc,
					    uint8_t *select_5ghz_margin)
{
	return wlan_mlme_get_select_5ghz_margin(psoc, select_5ghz_margin);
}

/**
 * ucfg_mlme_get_rtt_mac_randomization() - Get the RTT MAC randomization config
 * @psoc: pointer to psoc object
 * @rtt_mac_randomization: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_rtt_mac_randomization(struct wlan_objmgr_psoc *psoc,
					       bool *rtt_mac_randomization)
{
	return wlan_mlme_get_rtt_mac_randomization(psoc, rtt_mac_randomization);
}

/**
 * ucfg_mlme_get_crash_inject() - Get the crash inject config
 * @psoc: pointer to psoc object
 * @crash_inject: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_crash_inject(struct wlan_objmgr_psoc *psoc,
				      bool *crash_inject)
{
	return wlan_mlme_get_crash_inject(psoc, crash_inject);
}

/**
 * ucfg_mlme_get_lpass_support() - Get the LPASS Support config
 * @psoc: pointer to psoc object
 * @lpass_support: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_lpass_support(struct wlan_objmgr_psoc *psoc,
				       bool *lpass_support)
{
	return wlan_mlme_get_lpass_support(psoc, lpass_support);
}

/**
 * ucfg_mlme_get_wls_6ghz_cap() - Get the WiFi Location Service(WLS)
 * 6ghz capability
 * @psoc: pointer to psoc object
 * @wls_6ghz_capable: Pointer to the variable from caller
 *
 * Return: void
 */
static inline
void ucfg_mlme_get_wls_6ghz_cap(struct wlan_objmgr_psoc *psoc,
				bool *wls_6ghz_capable)
{
	wlan_mlme_get_wls_6ghz_cap(psoc, wls_6ghz_capable);
}

/**
 * ucfg_mlme_get_self_recovery() - Get the self recovery config
 * @psoc: pointer to psoc object
 * @self_recovery: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_self_recovery(struct wlan_objmgr_psoc *psoc,
				       bool *self_recovery)
{
	return wlan_mlme_get_self_recovery(psoc, self_recovery);
}

/**
 * ucfg_mlme_get_sub_20_chan_width() - Get the sub 20 chan width config
 * @psoc: pointer to psoc object
 * @sub_20_chan_width: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sub_20_chan_width(struct wlan_objmgr_psoc *psoc,
					   uint8_t *sub_20_chan_width)
{
	return wlan_mlme_get_sub_20_chan_width(psoc, sub_20_chan_width);
}

/**
 * ucfg_mlme_get_fw_timeout_crash() - Get the fw timeout crash config
 * @psoc: pointer to psoc object
 * @fw_timeout_crash: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_fw_timeout_crash(struct wlan_objmgr_psoc *psoc,
					  bool *fw_timeout_crash)
{
	return wlan_mlme_get_fw_timeout_crash(psoc, fw_timeout_crash);
}

/**
 * ucfg_mlme_get_ito_repeat_count() - Get the fw timeout crash config
 * @psoc: pointer to psoc object
 * @ito_repeat_count: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_ito_repeat_count(struct wlan_objmgr_psoc *psoc,
					  uint8_t *ito_repeat_count)
{
	return wlan_mlme_get_ito_repeat_count(psoc, ito_repeat_count);
}

/**
 * ucfg_mlme_get_acs_with_more_param() - Get the flag for acs with
 *					 more param
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_acs_with_more_param(struct wlan_objmgr_psoc *psoc,
					     bool *value)
{
	return wlan_mlme_get_acs_with_more_param(psoc, value);
}

/**
 * ucfg_mlme_get_auto_channel_weight() - Get the auto channel select weight
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_auto_channel_weight(struct wlan_objmgr_psoc *psoc,
					     uint32_t *value)
{
	return wlan_mlme_get_auto_channel_weight(psoc, value);
}

/**
 * ucfg_mlme_get_vendor_acs_support() - Get the flag for
 *					vendor acs support
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_vendor_acs_support(struct wlan_objmgr_psoc *psoc,
					    bool *value)
{
	return wlan_mlme_get_vendor_acs_support(psoc, value);
}

/**
 * ucfg_mlme_get_external_acs_policy() - Get flag for external control
 *					 acs policy
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_external_acs_policy(struct wlan_objmgr_psoc *psoc,
				  bool *value)
{
	return wlan_mlme_get_external_acs_policy(psoc, value);
}

/**
 * ucfg_mlme_set_ht_cap_info() - Set the HT cap info config
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS
ucfg_mlme_get_acs_support_for_dfs_ltecoex(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	return wlan_mlme_get_acs_support_for_dfs_ltecoex(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_dir_ac_vo() - Get TSPEC direction for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_dir_ac_vo(struct wlan_objmgr_psoc *psoc,
			    uint8_t *value)
{
	return wlan_mlme_get_wmm_dir_ac_vo(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_nom_msdu_size_ac_vo() - Get normal
 * MSDU size for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_nom_msdu_size_ac_vo(struct wlan_objmgr_psoc *psoc,
				      uint16_t *value)
{
	return wlan_mlme_get_wmm_nom_msdu_size_ac_vo(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_mean_data_rate_ac_vo() - mean data rate for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_mean_data_rate_ac_vo(struct wlan_objmgr_psoc *psoc,
				       uint32_t *value)
{
	return wlan_mlme_get_wmm_mean_data_rate_ac_vo(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_min_phy_rate_ac_vo() - min PHY
 * rate for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_min_phy_rate_ac_vo(struct wlan_objmgr_psoc *psoc,
				     uint32_t *value)
{
	return wlan_mlme_get_wmm_min_phy_rate_ac_vo(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_sba_ac_vo() - surplus bandwidth
 * allowance for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_sba_ac_vo(struct wlan_objmgr_psoc *psoc,
			    uint16_t *value)
{
	return wlan_mlme_get_wmm_sba_ac_vo(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_uapsd_vo_srv_intv() - Get Uapsd service
 * interval for voice
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_uapsd_vo_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	return wlan_mlme_get_wmm_uapsd_vo_srv_intv(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_uapsd_vo_sus_intv() - Get Uapsd suspension
 * interval for voice
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_uapsd_vo_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	return wlan_mlme_get_wmm_uapsd_vo_sus_intv(psoc, value);
}

/**
 *
 * ucfg_mlme_get_sap_inactivity_override() - Check if sap max inactivity
 * override flag is set.
 * @psoc: pointer to psoc object
 * @sme_config - Sme config struct
 *
 * Inline UCFG API to be used by HDD/OSIF callers to call
 * the mlme function wlan_mlme_get_sap_inactivity_override
 *
 * Return: QDF Status
 */
static inline
void ucfg_mlme_get_sap_inactivity_override(struct wlan_objmgr_psoc *psoc,
					   bool *value)
{
	wlan_mlme_get_sap_inactivity_override(psoc, value);
}

/**
 * ucfg_mlme_get_tx_chainmask_1ss() - Get the tx_chainmask_1ss value
 *
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
static inline
QDF_STATUS ucfg_mlme_get_tx_chainmask_1ss(struct wlan_objmgr_psoc *psoc,
					  uint8_t *value)
{
	return wlan_mlme_get_tx_chainmask_1ss(psoc, value);
}

/**
 * ucfg_mlme_get_num_11b_tx_chains() -  Get the number of 11b only tx chains
 *
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
static inline
QDF_STATUS ucfg_mlme_get_num_11b_tx_chains(struct wlan_objmgr_psoc *psoc,
					   uint16_t *value)
{
	return wlan_mlme_get_num_11b_tx_chains(psoc, value);
}

/**
 * ucfg_mlme_get_num_11ag_tx_chains() - get the total number of 11a/g tx chains
 *
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
static inline
QDF_STATUS ucfg_mlme_get_num_11ag_tx_chains(struct wlan_objmgr_psoc *psoc,
					    uint16_t *value)
{
	return wlan_mlme_get_num_11ag_tx_chains(psoc, value);
}

/**
 * ucfg_mlme_get_bt_chain_separation_flag() - bt chain separation enable/disable
 * @psoc: pointer to psoc object
 * @value: Value that needs to be got for the caller
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
static inline
QDF_STATUS ucfg_mlme_get_bt_chain_separation_flag(struct wlan_objmgr_psoc *psoc,
						  bool *value)
{
	return wlan_mlme_get_bt_chain_separation_flag(psoc, value);
}

/**
 * ucfg_mlme_configure_chain_mask() - configure chainmask parameters
 *
 * @psoc: pointer to psoc object
 * @session_id: vdev_id
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
static inline
QDF_STATUS ucfg_mlme_configure_chain_mask(struct wlan_objmgr_psoc *psoc,
					  uint8_t session_id)
{
	return wlan_mlme_configure_chain_mask(psoc, session_id);
}

/**
 * ucfg_mlme_is_chain_mask_supported() - check if configure chainmask can
 * be supported
 * @psoc: pointer to psoc object
 *
 * Return: true if supported else false
 */
static inline
bool ucfg_mlme_is_chain_mask_supported(struct wlan_objmgr_psoc *psoc)
{
	return wlan_mlme_is_chain_mask_supported(psoc);
}

/*
 * ucfg_mlme_get_sta_keep_alive_period() - Get the sta keep alive period
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_sta_keep_alive_period(struct wlan_objmgr_psoc *psoc,
				    uint32_t *val);

/*
 * ucfg_mlme_get_dfs_master_capability() - Get the dfs master capability
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_dfs_master_capability(struct wlan_objmgr_psoc *psoc,
				    bool *val);

/*
 * ucfg_mlme_get_dfs_disable_channel_switch() - Get the dfs channel switch
 * @psoc: pointer to psoc object
 * @dfs_disable_channel_switch:  Pointer to the value which will be filled
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_dfs_disable_channel_switch(struct wlan_objmgr_psoc *psoc,
					 bool *dfs_disable_channel_switch);

/*
 * ucfg_mlme_set_dfs_disable_channel_switch() - Set the dfs channel switch
 * @psoc: pointer to psoc object
 * @dfs_disable_channel_switch: Value that needs to be set.
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_dfs_disable_channel_switch(struct wlan_objmgr_psoc *psoc,
					 bool dfs_disable_channel_switch);
/*
 * ucfg_mlme_get_dfs_ignore_cac() - GSet the dfs ignore cac
 * @psoc: pointer to psoc object
 * @dfs_ignore_cac: Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_dfs_ignore_cac(struct wlan_objmgr_psoc *psoc,
			     bool *dfs_ignore_cac);

/*
 * ucfg_mlme_set_dfs_ignore_cac() - Set the dfs ignore cac
 * @psoc: pointer to psoc object
 * @dfs_ignore_cac: Value that needs to be set.
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_dfs_ignore_cac(struct wlan_objmgr_psoc *psoc,
			     bool dfs_ignore_cac);

/*
 * ucfg_mlme_get_sap_tx_leakage_threshold() - Get sap tx leakage threshold
 * @psoc: pointer to psoc object
 * @sap_tx_leakage_threshold: Pointer to the value which will be filled
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_sap_tx_leakage_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t *sap_tx_leakage_threshold);

/*
 * ucfg_mlme_set_sap_tx_leakage_threshold() - Set sap tx leakage threshold
 * @psoc: pointer to psoc object
 * @sap_tx_leakage_threshold: Value that needs to be set.
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_sap_tx_leakage_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t sap_tx_leakage_threshold);

/*
 * ucfg_mlme_get_dfs_pri_multiplier() - Get dfs pri multiplier
 * @psoc: pointer to psoc object
 * @dfs_pri_multiplier: Pointer to the value which will be filled
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_dfs_pri_multiplier(struct wlan_objmgr_psoc *psoc,
				 uint32_t *dfs_pri_multiplier);

/*
 * ucfg_mlme_set_dfs_pri_multiplier() - Set dfs pri multiplier
 * @psoc: pointer to psoc object
 * @dfs_pri_multiplier: Value that needs to be set.
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_dfs_pri_multiplier(struct wlan_objmgr_psoc *psoc,
				 uint32_t dfs_pri_multiplier);

/*
 * ucfg_mlme_get_dfs_filter_offload() - Get the dfs filter offload
 * @psoc: pointer to psoc object
 * @dfs_filter_offload: Pointer to the value which will be filled
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_dfs_filter_offload(struct wlan_objmgr_psoc *psoc,
				 bool *dfs_filter_offload);

/*
 * ucfg_mlme_set_dfs_filter_offload() - Set the dfs filter offload
 * @psoc: pointer to psoc object
 * @dfs_filter_offload: Value that needs to be set.
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_dfs_filter_offload(struct wlan_objmgr_psoc *psoc,
				 bool dfs_filter_offload);

/**
 * ucfg_mlme_get_oem_6g_supported() - Get oem 6Ghz supported
 * @psoc: pointer to psoc object
 * @oem_6g_supported: Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_oem_6g_supported(struct wlan_objmgr_psoc *psoc,
			       bool *oem_6g_supported);

/**
 * ucfg_mlme_get_fine_time_meas_cap() - Get fine timing measurement capability
 * @psoc: pointer to psoc object
 * @fine_time_meas_cap: Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_fine_time_meas_cap(struct wlan_objmgr_psoc *psoc,
				 uint32_t *fine_time_meas_cap);

/**
 * ucfg_mlme_set_fine_time_meas_cap() - Set fine timing measurement capability
 * @psoc: pointer to psoc object
 * @fine_time_meas_cap:  Value to be set
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_fine_time_meas_cap(struct wlan_objmgr_psoc *psoc,
				 uint32_t fine_time_meas_cap);

/**
 * ucfg_mlme_get_pmkid_modes() - Get PMKID modes
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_pmkid_modes(struct wlan_objmgr_psoc *psoc,
			  uint32_t *val);

/**
 * ucfg_mlme_set_pmkid_modes() - Set PMKID modes
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_pmkid_modes(struct wlan_objmgr_psoc *psoc,
			  uint32_t val);

/**
 * ucfg_mlme_get_dot11p_mode() - Get the setting about 802.11p mode
 * @psoc: pointer to psoc object
 * @out_mode:  Pointer to the mode which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_dot11p_mode(struct wlan_objmgr_psoc *psoc,
			  enum dot11p_mode *out_mode);

/**
 * ucfg_mlme_get_go_cts2self_for_sta() - Stop NOA and start using cts2self
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_go_cts2self_for_sta(struct wlan_objmgr_psoc *psoc,
				  bool *val);

/**
 * ucfg_mlme_get_qcn_ie_support() - QCN IE support or not
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_qcn_ie_support(struct wlan_objmgr_psoc *psoc,
			     bool *val);

/**
 * ucfg_mlme_get_tgt_gtx_usr_cfg() - Get the target gtx user config
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_tgt_gtx_usr_cfg(struct wlan_objmgr_psoc *psoc,
			      uint32_t *val);

/**
 * ucfg_mlme_is_override_ht20_40_24g() - use channel bonding in 2.4 GHz or not
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_is_override_ht20_40_24g(struct wlan_objmgr_psoc *psoc, bool *val);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * ucfg_mlme_get_roam_disable_config() - Get sta roam disable value
 * @psoc: pointer to psoc object
 * @val:  Pointer to bitmap of interfaces for those sta roaming is disabled
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_roam_disable_config(struct wlan_objmgr_psoc *psoc,
				  uint32_t *val);

/**
 * ucfg_mlme_get_roaming_offload() - Get roaming offload setting
 * @psoc: pointer to psoc object
 * @val:  Pointer to enable/disable roaming offload
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_roaming_offload(struct wlan_objmgr_psoc *psoc,
			      bool *val);

/**
 * ucfg_mlme_set_roaming_offload() - Enable/disable roaming offload
 * @psoc: pointer to psoc object
 * @val:  enable/disable roaming offload
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_roaming_offload(struct wlan_objmgr_psoc *psoc,
			      bool val);

/**
 * ucfg_mlme_get_roaming_triggers() - Get roaming triggers bitmap
 * value
 * @psoc: pointer to psoc object
 *
 * Return: Roaming triggers value
 */
static inline uint32_t
ucfg_mlme_get_roaming_triggers(struct wlan_objmgr_psoc *psoc)
{
	return wlan_mlme_get_roaming_triggers(psoc);
}
#else
static inline QDF_STATUS
ucfg_mlme_get_roam_disable_config(struct wlan_objmgr_psoc *psoc,
				  uint32_t *val)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_mlme_get_roaming_offload(struct wlan_objmgr_psoc *psoc,
			      bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_mlme_set_roaming_offload(struct wlan_objmgr_psoc *psoc,
			      bool val)
{
	return QDF_STATUS_SUCCESS;
}

static inline uint32_t
ucfg_mlme_get_roaming_triggers(struct wlan_objmgr_psoc *psoc)
{
	return 0xffff;
}
#endif

/**
 * ucfg_mlme_is_mawc_enabled() - MAWC enabled or not
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_is_mawc_enabled(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * ucfg_mlme_set_mawc_enabled() - Set MAWC enable or disable
 * @psoc: pointer to psoc object
 * @val:  enable or disable MAWC
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_mawc_enabled(struct wlan_objmgr_psoc *psoc, bool val);

/**
 * ucfg_mlme_is_fast_transition_enabled() - Fast transition enable or not
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_is_fast_transition_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *val);

/**
 * ucfg_mlme_set_fast_transition_enabled() - Set fast transition enable
 * @psoc: pointer to psoc object
 * @val:  Fast transition enable or disable
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_fast_transition_enabled(struct wlan_objmgr_psoc *psoc,
				      bool val);

/**
 * ucfg_mlme_is_roam_scan_offload_enabled() - Roam scan offload enable or not
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_is_roam_scan_offload_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *val);

#ifdef WLAN_ADAPTIVE_11R
/**
 * ucfg_mlme_set_tgt_adaptive_11r_cap() - Set adaptive 11r target service
 * capability
 * @psoc: pointer to psoc object
 * @val:  Target capability of adaptive 11r
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_tgt_adaptive_11r_cap(struct wlan_objmgr_psoc *psoc,
				   bool val);
/**
 * ucfg_mlme_get_adaptive11r_enabled() - get adaptive 11R enabled status
 * @psoc:   pointer to psoc object
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_adaptive11r_enabled(struct wlan_objmgr_psoc *psoc,
				  bool *value);
#else
static inline QDF_STATUS
ucfg_mlme_set_tgt_adaptive_11r_cap(struct wlan_objmgr_psoc *psoc,
				   bool val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_mlme_get_adaptive11r_enabled(struct wlan_objmgr_psoc *psoc,
				  bool *value)
{
	*value = false;
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * ucfg_mlme_set_roam_scan_offload_enabled() - Set roam scan offload enable
 * @psoc: pointer to psoc object
 * @val:  Roam scan offload enable or disable
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_roam_scan_offload_enabled(struct wlan_objmgr_psoc *psoc,
					bool val);

/**
 * ucfg_mlme_get_neighbor_scan_max_chan_time() - Get neighbor scan max
 * channel time
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_neighbor_scan_max_chan_time(struct wlan_objmgr_psoc *psoc,
					  uint16_t *val);

/**
 * ucfg_mlme_get_neighbor_scan_min_chan_time() - Get neighbor scan min
 * channel time
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_neighbor_scan_min_chan_time(struct wlan_objmgr_psoc *psoc,
					  uint16_t *val);

/**
 * ucfg_mlme_get_delay_before_vdev_stop() - Get the delay before vdev stop
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_delay_before_vdev_stop(struct wlan_objmgr_psoc *psoc,
				     uint8_t *val);

/**
 * ucfg_mlme_get_roam_bmiss_final_bcnt() - Get roam bmiss first count
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_roam_bmiss_final_bcnt(struct wlan_objmgr_psoc *psoc,
				    uint8_t *val);

/**
 * ucfg_mlme_get_roam_bmiss_first_bcnt() - Get roam bmiss final count
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_roam_bmiss_first_bcnt(struct wlan_objmgr_psoc *psoc,
				    uint8_t *val);

/**
 * ucfg_mlme_is_lfr_enabled() - LFR enable or not
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_is_lfr_enabled(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * ucfg_mlme_set_lfr_enabled() - Enable or disable LFR
 * @psoc: pointer to psoc object
 * @val:  Enable or disable LFR
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_lfr_enabled(struct wlan_objmgr_psoc *psoc, bool val);

/**
 * ucfg_mlme_is_roam_prefer_5ghz() - prefer 5ghz or not
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_is_roam_prefer_5ghz(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * ucfg_mlme_is_roam_intra_band() - Get the preference to roam within band
 * @psoc: pointer to psoc object
 *
 * Return: True if vdev should roam within band, false otherwise
 */
bool ucfg_mlme_is_roam_intra_band(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_mlme_set_roam_intra_band() - Set roam intra modes
 * @psoc: pointer to psoc object
 * @val:  roam intra modes or not
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_roam_intra_band(struct wlan_objmgr_psoc *psoc, bool val);

/**
 * ucfg_mlme_get_home_away_time() - Get home away time
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_home_away_time(struct wlan_objmgr_psoc *psoc, uint16_t *val);

/**
 * ucfg_mlme_set_fast_roam_in_concurrency_enabled() - Enable fast roam in
 * concurrency
 * @psoc: pointer to psoc object
 * @val:  Enable or disable fast roam in concurrency
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_fast_roam_in_concurrency_enabled(struct wlan_objmgr_psoc *psoc,
					       bool val);

/**
 * ucfg_mlme_get_wmi_wq_watchdog_timeout() - Get timeout for wmi watchdog bite
 * @psoc: pointer to psoc object
 * @wmi_wq_watchdog_timeout: buffer to hold value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_wmi_wq_watchdog_timeout(struct wlan_objmgr_psoc *psoc,
				      uint32_t *wmi_wq_watchdog_timeout);

/**
 * ucfg_mlme_set_wmi_wq_watchdog_timeout() - Set timeout for wmi watchdog bite
 * @psoc: pointer to psoc object
 * @wmi_wq_watchdog_timeout: value to be set
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_wmi_wq_watchdog_timeout(struct wlan_objmgr_psoc *psoc,
				      uint32_t wmi_wq_watchdog_timeout);

/**
 * ucfg_mlme_get_ps_data_inactivity_timeout() - Get data inactivity timeout
 * @psoc: pointer to psoc object
 * @inactivity_timeout: buffer to hold value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_ps_data_inactivity_timeout(struct wlan_objmgr_psoc *psoc,
					 uint32_t *inactivity_timeout);

/**
 * ucfg_mlme_set_ps_data_inactivity_timeout() - Set data inactivity timeout
 * @psoc: pointer to psoc object
 * @inactivity_timeout: value to be set
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_ps_data_inactivity_timeout(struct wlan_objmgr_psoc *psoc,
					 uint32_t inactivity_timeout);

/**
 * ucfg_mlme_set_sap_listen_interval() - Set the Sap listen interval
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_set_sap_listen_interval(struct wlan_objmgr_psoc *psoc,
					     int value)
{
	return wlan_mlme_set_sap_listen_interval(psoc, value);
}

/**
 * ucfg_mlme_set_assoc_sta_limit() - Set the assoc sta limit
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_set_assoc_sta_limit(struct wlan_objmgr_psoc *psoc,
					 int value)
{
	return wlan_mlme_set_assoc_sta_limit(psoc, value);
}

/**
 * ucfg_mlme_get_assoc_sta_limit() - Get the assoc sta limit
 * @psoc: pointer to psoc object
 * @value: Pointer to variable that needs to be filled by MLME
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_assoc_sta_limit(struct wlan_objmgr_psoc *psoc,
					 int *value)
{
	return wlan_mlme_get_assoc_sta_limit(psoc, value);
}

/**
 * ucfg_mlme_get_listen_interval() - Get listen interval
 * @psoc: pointer to psoc object
 * @value: Pointer to variable that needs to be filled by MLME
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_listen_interval(struct wlan_objmgr_psoc *psoc,
					     int *value)
{
	return wlan_mlme_get_listen_interval(psoc, value);
}


/**
 * ucfg_mlme_get_sap_get_peer_info() - get the sap get peer info
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_get_peer_info(struct wlan_objmgr_psoc *psoc,
					   bool *value)
{
	return wlan_mlme_get_sap_get_peer_info(psoc, value);
}

/**
 * ucfg_mlme_set_sap_get_peer_info() - set the sap get peer info
 * @psoc: pointer to psoc object
 * @value: value to overwrite the sap get peer info
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_set_sap_get_peer_info(struct wlan_objmgr_psoc *psoc,
					   bool value)
{
	return wlan_mlme_set_sap_get_peer_info(psoc, value);
}

/**
 * ucfg_mlme_get_sap_bcast_deauth_enabled() - get the sap bcast deauth
 *                                           enabled value
 * @psoc: pointer to psoc object
 * @value: Value that needs to be get from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_sap_bcast_deauth_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *value)
{
	return wlan_mlme_get_sap_bcast_deauth_enabled(psoc, value);
}

/**
 * ucfg_mlme_is_6g_sap_fd_enabled() - get the sap fils discovery
 *                                           enabled value
 * @psoc: pointer to psoc object
 * @value: Value that needs to be get from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_is_6g_sap_fd_enabled(struct wlan_objmgr_psoc *psoc,
			       bool *value)
{
	return wlan_mlme_is_6g_sap_fd_enabled(psoc, value);
}

/**
 * ucfg_mlme_get_sap_allow_all_channels() - get the sap allow all channels
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_allow_all_channels(struct wlan_objmgr_psoc *psoc,
						bool *value)
{
	return wlan_mlme_get_sap_allow_all_channels(psoc, value);
}

/**
 * ucfg_mlme_get_sap_max_peers() - get the sap max peers
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_max_peers(struct wlan_objmgr_psoc *psoc,
				       int *value)
{
	return wlan_mlme_get_sap_max_peers(psoc, value);
}

/**
 * ucfg_mlme_set_sap_max_peers() - Set the sap max peers
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_set_sap_max_peers(struct wlan_objmgr_psoc *psoc, int value)
{
	return wlan_mlme_set_sap_max_peers(psoc, value);
}

/**
 * ucfg_mlme_get_sap_max_offload_peers() - get the sap max offload peers
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_max_offload_peers(struct wlan_objmgr_psoc *psoc,
					       int *value)
{
	return wlan_mlme_get_sap_max_offload_peers(psoc, value);
}

/**
 * ucfg_mlme_get_sap_max_offload_reorder_buffs() - get the sap max offload
 * reorder buffs
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_max_offload_reorder_buffs(struct wlan_objmgr_psoc
						       *psoc, int *value)
{
	return wlan_mlme_get_sap_max_offload_reorder_buffs(psoc, value);
}

/**
 * ucfg_mlme_get_sap_chn_switch_bcn_count() - get the sap channel
 * switch beacon count
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_chn_switch_bcn_count(struct wlan_objmgr_psoc *psoc,
						  int *value)
{
	return wlan_mlme_get_sap_chn_switch_bcn_count(psoc, value);
}

/**
 * ucfg_mlme_get_sap_channel_switch_mode() - get the sap channel switch mode
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_channel_switch_mode(struct wlan_objmgr_psoc *psoc,
						 bool *value)
{
	return wlan_mlme_get_sap_chn_switch_mode(psoc, value);
}

/**
 * ucfg_mlme_get_sap_internal_restart() - get sap internal restart value
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_internal_restart(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	return wlan_mlme_get_sap_internal_restart(psoc, value);
}

/**
 * ucfg_mlme_get_sap_max_modulated_dtim() - get sap max modulated dtim
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_max_modulated_dtim(struct wlan_objmgr_psoc *psoc,
						uint8_t *value)
{
	return wlan_mlme_get_sap_max_modulated_dtim(psoc, value);
}

/**
 * ucfg_mlme_get_pref_chan_location() - get sap pref chan location
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_pref_chan_location(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value)
{
	return wlan_mlme_get_sap_chan_pref_location(psoc, value);
}

/**
 * ucfg_mlme_get_sap_country_priority() - get sap country code priority
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_country_priority(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	return wlan_mlme_get_sap_country_priority(psoc, value);
}

/**
 * ucfg_mlme_get_sap_reduces_beacon_interval() - get the sap reduces beacon
 * interval
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_reduces_beacon_interval(struct wlan_objmgr_psoc
						     *psoc, int *value)
{
	return wlan_mlme_get_sap_reduced_beacon_interval(psoc, value);
}

/**
 * ucfg_mlme_get_sap_chan_switch_rate_enabled() - get the sap channel
 * switch rate enabled.
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_chan_switch_rate_enabled(struct wlan_objmgr_psoc
						      *psoc, bool *value)
{
	return wlan_mlme_get_sap_chan_switch_rate_enabled(psoc, value);
}

/**
 * ucfg_mlme_get_sap_force_11n_for_11ac() - get the sap 11n for 11ac
 *
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_sap_force_11n_for_11ac(struct wlan_objmgr_psoc
						*psoc, bool *value)
{
	return wlan_mlme_get_sap_force_11n_for_11ac(psoc, value);
}

/**
 * ucfg_mlme_get_go_force_11n_for_11ac() - get the GO 11n for 11ac
 *
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_go_force_11n_for_11ac(struct wlan_objmgr_psoc
					       *psoc, bool *value)
{
	return wlan_mlme_get_go_force_11n_for_11ac(psoc, value);
}

/**
 * ucfg_mlme_is_sap_11ac_override() - Override 11ac bandwdith for SAP
 *
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_is_sap_11ac_override(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	return wlan_mlme_is_sap_11ac_override(psoc, value);
}

/**
 * ucfg_mlme_is_go_11ac_override() - Override 11ac bandwdith for P2P GO
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_is_go_11ac_override(struct wlan_objmgr_psoc *psoc,
					 bool *value)
{
	return wlan_mlme_is_go_11ac_override(psoc, value);
}

/**
 * ucfg_mlme_set_sap_11ac_override() - Set override 11ac bandwdith for SAP
 *
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_set_sap_11ac_override(struct wlan_objmgr_psoc *psoc,
					   bool value)
{
	return wlan_mlme_set_sap_11ac_override(psoc, value);
}

/**
 * ucfg_mlme_set_go_11ac_override() - Set override 11ac bandwdith for P2P GO
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_set_go_11ac_override(struct wlan_objmgr_psoc *psoc,
					  bool value)
{
	return wlan_mlme_set_go_11ac_override(psoc, value);
}

/**
 * ucfg_mlme_get_oce_sta_enabled_info() - Get OCE feature enable/disable
 * info for STA
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * OCE STA feature enable value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_get_oce_sta_enabled_info(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	return wlan_mlme_get_oce_sta_enabled_info(psoc, value);
}

/**
 * ucfg_mlme_get_bigtk_support() - Get whether bigtk is supported or not.
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the BIGTK support
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_get_bigtk_support(struct wlan_objmgr_psoc *psoc,
				       bool *value)
{
	return wlan_mlme_get_bigtk_support(psoc, value);
}

/**
 * ucfg_mlme_get_ocv_support() - Get whether ocv is supported or not.
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the OCV support
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_get_ocv_support(struct wlan_objmgr_psoc *psoc,
				     bool *value)
{
	return wlan_mlme_get_ocv_support(psoc, value);
}

/**
 * ucfg_mlme_get_oce_sap_enabled_info() - Get OCE feature enable/disable
 * info for SAP
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * OCE SAP feature enable value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_get_oce_sap_enabled_info(struct wlan_objmgr_psoc *psoc,
					      bool *value)
{
	return wlan_mlme_get_oce_sap_enabled_info(psoc, value);
}

/**
 * ucfg_mlme_update_oce_flags: Update the OCE flags
 *
 * @pdev: pointer to pdev object
 *
 * Inline UCFG API to be used by HDD/OSIF callers to update the
 * OCE feature flags
 *
 * Return: void
 */
static inline
void ucfg_mlme_update_oce_flags(struct wlan_objmgr_pdev *pdev)
{
	wlan_mlme_update_oce_flags(pdev);
}

/**
 * ucfg_mlme_is_ap_prot_enabled() - Check if sap is enabled
 * @psoc: pointer to psoc object
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * sap protection enabled/disabled
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
bool ucfg_mlme_is_ap_prot_enabled(struct wlan_objmgr_psoc *psoc)
{
	return wlan_mlme_is_ap_prot_enabled(psoc);
}

/**
 * ucfg_mlme_get_ap_protection_mode() - Get ap protection mode info
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ap protection mode value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_get_ap_protection_mode(struct wlan_objmgr_psoc *psoc,
					    uint16_t *value)
{
	return wlan_mlme_get_ap_protection_mode(psoc, value);
}

/**
 * ucfg_mlme_is_ap_obss_prot_enabled() - Get ap obss protection enable/disable
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * obss protection enable value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_is_ap_obss_prot_enabled(struct wlan_objmgr_psoc *psoc,
					     bool *value)
{
	return wlan_mlme_is_ap_obss_prot_enabled(psoc, value);
}

/**
 * ucfg_mlme_get_rts_threshold() - Get the rts threshold config
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_rts_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t *value)
{
	return wlan_mlme_get_rts_threshold(psoc, value);
}

/**
 * ucfg_mlme_set_rts_threshold() - Set the rts threshold config
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_set_rts_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t value)
{
	return wlan_mlme_set_rts_threshold(psoc, value);
}

/**
 * ucfg_mlme_get_frag_threshold() - Get the fragmentation threshold
 *                                  config
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_frag_threshold(struct wlan_objmgr_psoc *psoc,
					uint32_t *value)
{
	return wlan_mlme_get_frag_threshold(psoc, value);
}

/**
 * ucfg_mlme_set_frag_threshold() - set the frag threshold config
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_set_frag_threshold(struct wlan_objmgr_psoc *psoc,
					uint32_t value)
{
	return wlan_mlme_set_frag_threshold(psoc, value);
}

/**
 * ucfg_mlme_get_fils_enabled_info() - Get fils enable/disable info
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * fils enable value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_get_fils_enabled_info(struct wlan_objmgr_psoc *psoc,
					   bool *value)
{
	return wlan_mlme_get_fils_enabled_info(psoc, value);
}

/**
 * ucfg_mlme_set_fils_enabled_info() - Set fils enable info
 *
 * @psoc: pointer to psoc object
 * @value: value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers to set the
 * fils enable value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_set_fils_enabled_info(struct wlan_objmgr_psoc *psoc,
					   bool value)
{
	return wlan_mlme_set_fils_enabled_info(psoc, value);
}

/**
 * ucfg_mlme_set_enable_bcast_probe_rsp() - Set enable bcast probe resp info
 * @psoc: pointer to psoc object
 * @value: value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers to set the
 * enable bcast probe resp info
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_set_enable_bcast_probe_rsp(struct wlan_objmgr_psoc *psoc,
						bool value)
{
	return wlan_mlme_set_enable_bcast_probe_rsp(psoc, value);
}

/**
 * ucfg_mlme_set_vht_ch_width() - set the vht supported channel width cfg
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_set_vht_ch_width(struct wlan_objmgr_psoc *psoc, uint8_t value)
{
	return wlan_mlme_cfg_set_vht_chan_width(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_vht_chan_width() - gets vht supported channel width into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_get_vht_chan_width(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_cfg_get_vht_chan_width(psoc, value);
}

/**
 * ucfg_mlme_cfg_set_vht_ldpc_coding_cap() - sets vht ldpc coding cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_set_vht_ldpc_coding_cap(struct wlan_objmgr_psoc *psoc, bool value)
{
	return wlan_mlme_cfg_set_vht_ldpc_coding_cap(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_short_gi_160_mhz() - Get SHORT GI 160MHZ from cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_get_short_gi_160_mhz(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_cfg_get_short_gi_160_mhz(psoc, value);
}

/**
 * ucfg_mlme_cfg_set_short_gi_160_mhz() - sets basic set SHORT GI 160MHZ into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_set_short_gi_160_mhz(struct wlan_objmgr_psoc *psoc, bool value)
{
	return wlan_mlme_cfg_set_short_gi_160_mhz(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_vht_tx_stbc() - gets vht tx stbc from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_get_vht_tx_stbc(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_cfg_get_vht_tx_stbc(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_vht_rx_stbc() - gets vht rx stbc from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_get_vht_rx_stbc(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_cfg_get_vht_rx_stbc(psoc, value);
}

/**
 * ucfg_mlme_cfg_set_vht_tx_bfee_ant_supp() - sets vht Beamformee antenna
 * support cap into cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_cfg_set_vht_tx_bfee_ant_supp(struct wlan_objmgr_psoc *psoc,
						  uint8_t value)
{
	return wlan_mlme_cfg_set_vht_tx_bfee_ant_supp(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_vht_tx_bfee_ant_supp() - gets vht Beamformee antenna
 * support cap into cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_cfg_get_vht_tx_bfee_ant_supp(struct wlan_objmgr_psoc *psoc,
						  uint8_t *value)
{
	return wlan_mlme_cfg_get_vht_tx_bfee_ant_supp(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_vht_rx_mcs_map() - gets vht rx mcs map from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_get_vht_rx_mcs_map(struct wlan_objmgr_psoc *psoc, uint32_t *value)
{
	return wlan_mlme_cfg_get_vht_rx_mcs_map(psoc, value);
}

/**
 * ucfg_mlme_cfg_set_vht_rx_mcs_map() - sets rx mcs map into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_set_vht_rx_mcs_map(struct wlan_objmgr_psoc *psoc, uint32_t value)
{
	return wlan_mlme_cfg_set_vht_rx_mcs_map(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_vht_tx_mcs_map() - gets vht tx mcs map from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_get_vht_tx_mcs_map(struct wlan_objmgr_psoc *psoc, uint32_t *value)
{
	return wlan_mlme_cfg_get_vht_tx_mcs_map(psoc, value);
}

/**
 * ucfg_mlme_cfg_set_vht_tx_mcs_map() - sets tx mcs map into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_set_vht_tx_mcs_map(struct wlan_objmgr_psoc *psoc, uint32_t value)
{
	return wlan_mlme_cfg_set_vht_tx_mcs_map(psoc, value);
}

/**
 * ucfg_mlme_cfg_set_vht_rx_supp_data_rate() - sets rx supported data
 * rate into cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_set_vht_rx_supp_data_rate(struct wlan_objmgr_psoc *psoc,
					uint32_t value)
{
	return wlan_mlme_cfg_set_vht_rx_supp_data_rate(psoc, value);
}

/**
 * ucfg_mlme_cfg_set_vht_tx_supp_data_rate() - sets tx supported data rate into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_set_vht_tx_supp_data_rate(struct wlan_objmgr_psoc *psoc,
					uint32_t value)
{
	return wlan_mlme_cfg_set_vht_tx_supp_data_rate(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_vht_basic_mcs_set() - gets basic mcs set from
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_get_vht_basic_mcs_set(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	return wlan_mlme_cfg_get_vht_basic_mcs_set(psoc, value);
}

/**
 * ucfg_mlme_cfg_set_vht_basic_mcs_set() - sets basic mcs set into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_cfg_set_vht_basic_mcs_set(struct wlan_objmgr_psoc *psoc,
				    uint32_t value)
{
	return wlan_mlme_cfg_set_vht_basic_mcs_set(psoc, value);
}

/**
 * ucfg_mlme_get_vht_enable_tx_bf() - gets enable TXBF for 20MHZ
 * for 11ac
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_enable_tx_bf(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_vht_enable_tx_bf(psoc, value);
}

/**
 * ucfg_mlme_get_vht_tx_su_beamformer() - gets enable tx_su_beamformer
 * for 11ac
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_tx_su_beamformer(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_vht_tx_su_beamformer(psoc, value);
}

/**
 * ucfg_mlme_get_vht_channel_width() - gets Channel width capability
 * for 11ac
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_channel_width(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_get_vht_channel_width(psoc, value);
}

/**
 * ucfg_mlme_get_vht_rx_mcs_8_9() - VHT Rx MCS capability for 1x1 mode
 * for 11ac
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_rx_mcs_8_9(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_get_vht_rx_mcs_8_9(psoc, value);
}

/**
 * ucfg_mlme_get_vht_tx_mcs_8_9() - VHT Tx MCS capability for 1x1 mode
 * for 11ac
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_tx_mcs_8_9(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_get_vht_tx_mcs_8_9(psoc, value);
}

/**
 * ucfg_mlme_get_vht_rx_mcs_2x2() - VHT Rx MCS capability for 2x2 mode
 * for 11ac
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_rx_mcs_2x2(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_get_vht_rx_mcs_2x2(psoc, value);
}

/**
 * ucfg_mlme_get_vht_tx_mcs_2x2() - VHT Tx MCS capability for 2x2 mode
 * for 11ac
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_tx_mcs_2x2(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_get_vht_tx_mcs_2x2(psoc, value);
}

/**
 * ucfg_mlme_get_ini_vdev_config() - get the ini capability of vdev
 * @vdev: pointer to the vdev obj
 *
 * This API will get the ini config of the vdev related to
 * the nss, chains params
 *
 * Return: pointer to the nss, chain param ini cfg structure
 */
static inline struct wlan_mlme_nss_chains *
ucfg_mlme_get_ini_vdev_config(struct wlan_objmgr_vdev *vdev)
{
	return mlme_get_ini_vdev_config(vdev);
}

/**
 * ucfg_mlme_get_dynamic_vdev_config() - get the dynamic capability of vdev
 * @vdev: pointer to the vdev obj
 *
 * This API will get the dynamic config of the vdev related to nss,
 * chains params
 *
 * Return: pointer to the nss, chain param dynamic cfg structure
 */
static inline struct wlan_mlme_nss_chains *
ucfg_mlme_get_dynamic_vdev_config(struct wlan_objmgr_vdev *vdev)
{
	return mlme_get_dynamic_vdev_config(vdev);
}

/**
 * ucfg_mlme_get_vht20_mcs9() - Enables VHT MCS9 in 20M BW operation
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht20_mcs9(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_vht20_mcs9(psoc, value);
}

/**
 * ucfg_mlme_get_enable_dynamic_nss_chains_cfg() - API to get whether dynamic
 * nss and chain config is enabled or not
 * @psoc: psoc context
 * @value: data to be set
 *
 * API to get whether dynamic nss and chain config is enabled or not
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_enable_dynamic_nss_chains_cfg(struct wlan_objmgr_psoc *psoc,
					    bool *value)
{
	return wlan_mlme_get_enable_dynamic_nss_chains_cfg(psoc, value);
}

/**
 * ucfg_mlme_get_vht_enable2x2() - Enables/disables VHT Tx/Rx MCS values for 2x2
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_enable2x2(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_vht_enable2x2(psoc, value);
}

/**
 * ucfg_mlme_get_force_sap_enabled() - Get the value of force SAP enabled
 * @psoc: psoc context
 * @value: data to get
 *
 * Get the value of force SAP enabled
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_force_sap_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_force_sap_enabled(psoc, value);
}

/**
 * ucfg_mlme_set_vht_enable2x2() - Enables/disables VHT Tx/Rx MCS values for 2x2
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_set_vht_enable2x2(struct wlan_objmgr_psoc *psoc, bool value)
{
	return wlan_mlme_set_vht_enable2x2(psoc, value);
}

/**
 * ucfg_mlme_get_vht_enable_paid() - Enables/disables paid feature
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_enable_paid(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_vht_enable_paid(psoc, value);
}

/**
 * ucfg_mlme_get_vht_enable_gid() - Enables/disables gid feature
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_enable_gid(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_vht_enable_gid(psoc, value);
}

/**
 * ucfg_mlme_get_vht_for_24ghz() - Get mlme cfg of vht for 24ghz
 * @psoc: psoc context
 * @value: data to get
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_vht_for_24ghz(psoc, value);
}

/**
 * ucfg_mlme_set_vht_for_24ghz() - Enables/disables vht for 24ghz
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_set_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool value)
{
	return wlan_mlme_set_vht_for_24ghz(psoc, value);
}

/**
 * ucfg_mlme_get_vendor_vht_for_24ghz() - Get mlme cfg of vendor vht for 24ghz
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_get_vendor_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_vendor_vht_for_24ghz(psoc, value);
}

/**
 * ucfg_mlme_update_vht_cap() - Update vht capabilities
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline
QDF_STATUS ucfg_mlme_update_vht_cap(struct wlan_objmgr_psoc *psoc,
				    struct wma_tgt_vht_cap *cfg)
{
	return mlme_update_vht_cap(psoc, cfg);
}

/**
 * ucfg_mlme_update_nss_vht_cap() -Update the number of spatial
 * streams supported for vht
 * @psoc: psoc context
 * @value: data to be set
 *
 * Inline UCFG API to be used by HDD/OSIF callers to get the
 * ignore_peer_ht_opmode flag value
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_update_nss_vht_cap(struct wlan_objmgr_psoc *psoc)
{
	return mlme_update_nss_vht_cap(psoc);
}

/**
 * ucfg_mlme_is_11h_enabled() - Get 11h flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_is_11h_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_is_11h_enabled(psoc, value);
}

/**
 * ucfg_mlme_set_11h_enabled() - Set 11h flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_set_11h_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	return wlan_mlme_set_11h_enabled(psoc, value);
}

/**
 * ucfg_mlme_is_11d_enabled() - Get 11d flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_is_11d_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_is_11d_enabled(psoc, value);
}

/**
 * ucfg_mlme_set_11d_enabled() - Set 11d flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_set_11d_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	return wlan_mlme_set_11d_enabled(psoc, value);
}

/**
 * ucfg_mlme_is_rf_test_mode_enabled() - Get rf test mode flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_is_rf_test_mode_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_is_rf_test_mode_enabled(psoc, value);
}

/**
 * ucfg_mlme_set_rf_test_mode_enabled() - Set rf test mode flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_set_rf_test_mode_enabled(struct wlan_objmgr_psoc *psoc, bool value)
{
	return wlan_mlme_set_rf_test_mode_enabled(psoc, value);
}

/**
 * ucfg_mlme_get_opr_rate() - Get operational rate set
 * @psoc: pointer to vdev object
 * @buf: buffer to get rates set
 * @len: length of the buffer
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *buf,
		       qdf_size_t *len)
{
	return mlme_get_opr_rate(vdev, buf, len);
}

/**
 * ucfg_mlme_get_ext_opr_rate() - Get extended operational rate set
 * @psoc: pointer to vdev object
 * @buf: buffer to get rates set
 * @len: length of the buffer
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_ext_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *buf,
			   qdf_size_t *len)
{
	return mlme_get_ext_opr_rate(vdev, buf, len);
}

/**
 * ucfg_mlme_get_supported_mcs_set() - Get Supported MCS set
 * @psoc: pointer to psoc object
 * @buf:  caller buffer to copy mcs set info
 * @len: length of the buffer
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_supported_mcs_set(struct wlan_objmgr_psoc *psoc, uint8_t *buf,
				qdf_size_t *len);

/**
 * ucfg_mlme_set_supported_mcs_set() - Get Supported MCS set
 * @psoc: pointer to psoc object
 * @buf: caller buffer having mcs set info
 * @len: length of the buffer
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_supported_mcs_set(struct wlan_objmgr_psoc *psoc, uint8_t *buf,
				qdf_size_t len);

/**
 * ucfg_mlme_get_current_mcs_set() - Get current MCS set
 * @psoc: pointer to psoc object
 * @buf:  caller buffer to copy mcs set info
 * @len: length of the buffer
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_current_mcs_set(struct wlan_objmgr_psoc *psoc, uint8_t *buf,
			      qdf_size_t *len);

/**
 * ucfg_mlme_get_sta_keepalive_method() - Get sta_keepalive_method
 * @psoc: pointer to psoc object
 * @val:  Value to pass to the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_sta_keepalive_method(struct wlan_objmgr_psoc *psoc,
				   enum station_keepalive_method *val);

/**
 * ucfg_mlme_stats_get_periodic_display_time() - get display time
 * @psoc: pointer to psoc object
 * @periodic_display_time: buffer to hold value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_stats_get_periodic_display_time(struct wlan_objmgr_psoc *psoc,
					  uint32_t *periodic_display_time);

/**
 * ucfg_mlme_stats_get_cfg_values() - get stats cfg values
 * @psoc: pointer to psoc object
 * @link_speed_rssi_high: link speed high limit
 * @link_speed_rssi_mid: link speed high mid
 * @link_speed_rssi_low: link speed high low
 * @link_speed_rssi_report: link speed report limit
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_stats_get_cfg_values(struct wlan_objmgr_psoc *psoc,
			       int *link_speed_rssi_high,
			       int *link_speed_rssi_mid,
			       int *link_speed_rssi_low,
			       uint32_t *link_speed_rssi_report);

/**
 * ucfg_mlme_stats_is_link_speed_report_actual() - is link speed report set
 * actual
 * @psoc: pointer to psoc object
 *
 * Return: True is report set to actual
 */
bool
ucfg_mlme_stats_is_link_speed_report_actual(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_mlme_stats_is_link_speed_report_max() - is link speed report set
 * max
 * @psoc: pointer to psoc object
 *
 * Return: True is report set to max
 */
bool
ucfg_mlme_stats_is_link_speed_report_max(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_mlme_stats_is_link_speed_report_max_scaled() - is link speed report set
 * max scaled
 * @psoc: pointer to psoc object
 *
 * Return: True is report set to max scaled
 */
bool
ucfg_mlme_stats_is_link_speed_report_max_scaled(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_mlme_get_tl_delayed_trgr_frm_int() - Get delay interval(in ms)
 * of UAPSD auto trigger.
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: None
 */
static inline
void ucfg_mlme_get_tl_delayed_trgr_frm_int(struct wlan_objmgr_psoc *psoc,
					   uint32_t *value)
{
	wlan_mlme_get_tl_delayed_trgr_frm_int(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_dir_ac_vi() - Get TSPEC direction
 * for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_dir_ac_vi(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_get_wmm_dir_ac_vi(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_nom_msdu_size_ac_vi() - Get normal
 * MSDU size for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_wmm_nom_msdu_size_ac_vi(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	return wlan_mlme_get_wmm_nom_msdu_size_ac_vi(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_mean_data_rate_ac_vi() - mean data
 * rate for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_wmm_mean_data_rate_ac_vi(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value)
{
	return wlan_mlme_get_wmm_mean_data_rate_ac_vi(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_min_phy_rate_ac_vi() - min PHY
 * rate for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_wmm_min_phy_rate_ac_vi(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	return wlan_mlme_get_wmm_min_phy_rate_ac_vi(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_sba_ac_vi() - surplus bandwidth
 * allowance for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_sba_ac_vi(struct wlan_objmgr_psoc *psoc, uint16_t *value)
{
	return wlan_mlme_get_wmm_sba_ac_vi(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_uapsd_vi_srv_intv() - Get Uapsd service
 * interval for video
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_uapsd_vi_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	return wlan_mlme_get_wmm_uapsd_vi_srv_intv(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_uapsd_vi_sus_intv() - Get Uapsd suspension
 * interval for video
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_uapsd_vi_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	return wlan_mlme_get_wmm_uapsd_vi_sus_intv(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_dir_ac_be() - Get TSPEC direction
 * for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_dir_ac_be(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_get_wmm_dir_ac_be(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_nom_msdu_size_ac_be() - Get normal
 * MSDU size for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_wmm_nom_msdu_size_ac_be(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	return wlan_mlme_get_wmm_nom_msdu_size_ac_be(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_mean_data_rate_ac_be() - mean data
 * rate for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_wmm_mean_data_rate_ac_be(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value)
{
	return wlan_mlme_get_wmm_mean_data_rate_ac_be(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_min_phy_rate_ac_be() - min PHY
 * rate for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_wmm_min_phy_rate_ac_be(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	return wlan_mlme_get_wmm_min_phy_rate_ac_be(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_sba_ac_be() - surplus bandwidth
 * allowance for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_sba_ac_be(struct wlan_objmgr_psoc *psoc, uint16_t *value)
{
	return wlan_mlme_get_wmm_sba_ac_be(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_uapsd_be_srv_intv() - Get Uapsd service
 * interval for BE
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_uapsd_be_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	return wlan_mlme_get_wmm_uapsd_be_srv_intv(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_uapsd_be_sus_intv() - Get Uapsd suspension
 * interval for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_uapsd_be_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	return wlan_mlme_get_wmm_uapsd_be_sus_intv(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_dir_ac_bk() - Get TSPEC direction
 * for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_dir_ac_bk(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_get_wmm_dir_ac_bk(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_nom_msdu_size_ac_be() - Get normal
 * MSDU size for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_wmm_nom_msdu_size_ac_bk(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value)
{
	return wlan_mlme_get_wmm_nom_msdu_size_ac_bk(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_mean_data_rate_ac_bk() - mean data
 * rate for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_wmm_mean_data_rate_ac_bk(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value)
{
	return wlan_mlme_get_wmm_mean_data_rate_ac_bk(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_min_phy_rate_ac_bk() - min PHY
 * rate for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_wmm_min_phy_rate_ac_bk(struct wlan_objmgr_psoc *psoc,
						uint32_t *value)
{
	return wlan_mlme_get_wmm_min_phy_rate_ac_bk(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_sba_ac_bk() - surplus bandwidth
 * allowance for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_sba_ac_bk(struct wlan_objmgr_psoc *psoc, uint16_t *value)
{
	return wlan_mlme_get_wmm_sba_ac_bk(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_uapsd_bk_srv_intv() - Get Uapsd service
 * interval for BK
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_uapsd_bk_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	return wlan_mlme_get_wmm_uapsd_bk_srv_intv(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_uapsd_bk_sus_intv() - Get Uapsd suspension
 * interval for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_uapsd_bk_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	return wlan_mlme_get_wmm_uapsd_bk_sus_intv(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_mode() - Enable WMM feature
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_mode(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_get_wmm_mode(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_wlm_level() - Get the WLM level value
 * @psoc: pointer to psoc object
 * @level: level that needs to be filled.
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_cfg_get_wlm_level(struct wlan_objmgr_psoc *psoc,
				       uint8_t *level)
{
	return mlme_get_cfg_wlm_level(psoc, level);
}

/**
 * ucfg_mlme_cfg_get_wlm_reset() - Get the WLM reset flag
 * @psoc: pointer to psoc object
 * @reset: reset that needs to be filled.
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_cfg_get_wlm_reset(struct wlan_objmgr_psoc *psoc,
				       bool *reset)
{
	return mlme_get_cfg_wlm_reset(psoc, reset);
}

#ifdef WLAN_FEATURE_11AX
/**
 * ucfg_mlme_update_tgt_he_cap() - Update tgt he cap in mlme component
 *
 * @psoc: pointer to psoc object
 * @cfg: pointer to config params from target
 *
 * Inline UCFG API to be used by HDD/OSIF callers to update
 * he caps in mlme.
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
static inline QDF_STATUS
ucfg_mlme_update_tgt_he_cap(struct wlan_objmgr_psoc *psoc,
			    struct wma_tgt_cfg *cfg)
{
	return mlme_update_tgt_he_caps_in_cfg(psoc, cfg);
}

/**
 * ucfg_mlme_cfg_get_he_caps() - Get the HE capability info
 * @psoc: pointer to psoc object
 * @he_cap: Caps that needs to be filled.
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_cfg_get_he_caps(struct wlan_objmgr_psoc *psoc,
				     tDot11fIEhe_cap *he_cap)
{
	return mlme_cfg_get_he_caps(psoc, he_cap);
}

/**
 * ucfg_mlme_cfg_get_he_ul_mumimo() - Get the HE Ul Mumio
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_cfg_get_he_ul_mumimo(struct wlan_objmgr_psoc *psoc,
					  uint32_t *value)
{
	return wlan_mlme_cfg_get_he_ul_mumimo(psoc, value);
}

/**
 * ucfg_mlme_cfg_set_he_ul_mumimo() - Set the HE Ul Mumio
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_cfg_set_he_ul_mumimo(struct wlan_objmgr_psoc *psoc,
					  uint32_t value)
{
	return wlan_mlme_cfg_set_he_ul_mumimo(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_enable_ul_mimo() - Get the HE Ul mimo
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_cfg_get_enable_ul_mimo(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value)
{
	return wlan_mlme_cfg_get_enable_ul_mimo(psoc, value);
}

/**
 * ucfg_mlme_cfg_get_enable_ul_ofdm() - Get enable ul ofdm
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_cfg_get_enable_ul_ofdm(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value)
{
	return wlan_mlme_cfg_get_enable_ul_ofdm(psoc, value);
}
#endif

/**
 * ucfg_mlme_get_80211e_is_enabled() - Enable 802.11e feature
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_80211e_is_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_80211e_is_enabled(psoc, value);
}

/**
 * ucfg_mlme_get_wmm_uapsd_mask() - setup U-APSD mask for ACs
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_wmm_uapsd_mask(struct wlan_objmgr_psoc *psoc, uint8_t *value)
{
	return wlan_mlme_get_wmm_uapsd_mask(psoc, value);
}

/**
 * ucfg_mlme_get_implicit_qos_is_enabled() - Enable implicit QOS
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_mlme_get_implicit_qos_is_enabled(struct wlan_objmgr_psoc *psoc,
						 bool *value)
{
	return wlan_mlme_get_implicit_qos_is_enabled(psoc, value);
}

#ifdef FEATURE_WLAN_ESE
/**
 * ucfg_mlme_get_inactivity_interval() - Infra Inactivity Interval
 * @psoc: pointer to psoc object
 * @value: Value that needs to be get from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: None
 */
static inline void
ucfg_mlme_get_inactivity_interval(struct wlan_objmgr_psoc *psoc,
				  uint32_t *value)
{
	wlan_mlme_get_inactivity_interval(psoc, value);
}

/**
 * ucfg_mlme_is_ese_enabled() - ese feature enable or not
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_is_ese_enabled(struct wlan_objmgr_psoc *psoc, bool *val);
#endif /* FEATURE_WLAN_ESE */

/**
 * ucfg_mlme_get_is_ts_burst_size_enable() - Get TS burst size flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be get from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: None
 */
static inline
void ucfg_mlme_get_is_ts_burst_size_enable(struct wlan_objmgr_psoc *psoc,
					   bool *value)
{
	wlan_mlme_get_is_ts_burst_size_enable(psoc, value);
}

/**
 * ucfg_mlme_get_ts_info_ack_policy() - Get TS ack policy
 * @psoc: pointer to psoc object
 * @value: Value that needs to be get from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: None
 */
static inline void
ucfg_mlme_get_ts_info_ack_policy(struct wlan_objmgr_psoc *psoc,
				 enum mlme_ts_info_ack_policy *value)
{
	wlan_mlme_get_ts_info_ack_policy(psoc, value);
}

/**
 * ucfg_mlme_get_ts_acm_value_for_ac() - Get ACM value for AC
 * @psoc: pointer to psoc object
 * @value: Value that needs to be get from the caller
 *
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_ts_acm_value_for_ac(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_get_ts_acm_value_for_ac(psoc, value);
}

/*
 * ucfg_mlme_is_sap_uapsd_enabled() - SAP UAPSD enabled status.
 * @psoc: pointer to psoc object
 * @value: sap uapsd enabled flag value requested from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_is_sap_uapsd_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_is_sap_uapsd_enabled(psoc, value);
}

/*
 * ucfg_mlme_set_sap_uapsd_flag() - SAP UAPSD enabled status.
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_set_sap_uapsd_flag(struct wlan_objmgr_psoc *psoc, bool value)
{
	return wlan_mlme_set_sap_uapsd_flag(psoc, value);
}

/**
 * ucfg_mlme_get_enable_deauth_to_disassoc_map() - Enable deauth_to_disassoc_map
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_enable_deauth_to_disassoc_map(struct wlan_objmgr_psoc *psoc,
					    bool *value);

/**
 * ucfg_mlme_get_ap_random_bssid_enable() - Enable random bssid
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_ap_random_bssid_enable(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * ucfg_mlme_get_sta_miracast_mcc_rest_time() - Get STA/MIRACAST MCC rest time
 *
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * This API gives rest time to be used when STA and MIRACAST MCC conc happens
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_mlme_get_sta_miracast_mcc_rest_time(struct wlan_objmgr_psoc *psoc,
					 uint32_t *value)
{
	return wlan_mlme_get_sta_miracast_mcc_rest_time(psoc, value);
}

/**
 * ucfg_mlme_get_sap_mcc_chnl_avoid() - Check if SAP MCC needs to be avoided
 *
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * This API fetches the user setting to determine if SAP MCC with other persona
 * to be avoided.
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_mlme_get_sap_mcc_chnl_avoid(struct wlan_objmgr_psoc *psoc,
				 uint8_t *value)
{
	return wlan_mlme_get_sap_mcc_chnl_avoid(psoc, value);
}

/**
 * ucfg_mlme_get_mcc_bcast_prob_resp() - Get broadcast probe rsp in MCC
 *
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * To get INI value which helps to determe whether to enable/disable use of
 * broadcast probe response to increase the detectability of SAP in MCC mode.
 *
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_mlme_get_mcc_bcast_prob_resp(struct wlan_objmgr_psoc *psoc,
				  uint8_t *value)
{
	return wlan_mlme_get_mcc_bcast_prob_resp(psoc, value);
}

/**
 * ucfg_mlme_get_mcc_rts_cts_prot() - To get RTS-CTS protection in MCC.
 *
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * To get INI value which helps to determine whether to enable/disable
 * use of long duration RTS-CTS protection when SAP goes off
 * channel in MCC mode.
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_mlme_get_mcc_rts_cts_prot(struct wlan_objmgr_psoc *psoc,
			       uint8_t *value)
{
	return wlan_mlme_get_mcc_rts_cts_prot(psoc, value);
}

/**
 * ucfg_mlme_get_mcc_feature() - To find out to enable/disable MCC feature
 *
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * To get INI value which helps to determine whether to enable MCC feature
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
ucfg_mlme_get_mcc_feature(struct wlan_objmgr_psoc *psoc,
			  uint8_t *value)
{
	return wlan_mlme_get_mcc_feature(psoc, value);
}

/**
 * ucfg_wlan_mlme_get_rrm_enabled() - Get the rrm enabled
 * @psoc: pointer to psoc object
 * @value: Value that needs to be get from the caller
 *
 * Return: QDF Status
 */
static inline
QDF_STATUS ucfg_wlan_mlme_get_rrm_enabled(struct wlan_objmgr_psoc *psoc,
					  bool *value)
{
	return wlan_mlme_get_rrm_enabled(psoc, value);
}

/**
 * ucfg_mlme_get_latency_enable() - Get the latency_enable
 * @psoc: pointer to psoc object
 * @value: Value that needs to be get from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_latency_enable(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * ucfg_mlme_get_latency_level() - Get the latency level
 * @psoc: pointer to psoc object
 * @value: Value that needs to be get from the caller
 *         latency values are defined in WMI_WLM_LATENCY_LEVEL
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_latency_level(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * ucfg_mlme_get_latency_host_flags() - Get host flags for latency level
 * @psoc: pointer to psoc object
 * @latency_level: latency level
 * @value: Value that needs to be get from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_latency_host_flags(struct wlan_objmgr_psoc *psoc,
				 uint8_t latency_level, uint32_t *value);

/**
 * ucfg_mlme_get_dtim_selection_diversity() - get dtim selection diversity
 * bitmap
 * @psoc: pointer to psoc object
 * @dtim_selection_div: value that is requested by the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static inline QDF_STATUS
ucfg_mlme_get_dtim_selection_diversity(struct wlan_objmgr_psoc *psoc,
				       uint32_t *dtim_selection_div)
{
	return wlan_mlme_get_dtim_selection_diversity(psoc, dtim_selection_div);
}

/**
 * ucfg_mlme_get_bmps_min_listen_interval() - get beacon mode powersave
 * minimum listen interval value
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static inline QDF_STATUS
ucfg_mlme_get_bmps_min_listen_interval(struct wlan_objmgr_psoc *psoc,
				       uint32_t *value)
{
	return wlan_mlme_get_bmps_min_listen_interval(psoc, value);
}

/**
 * ucfg_mlme_get_bmps_max_listen_interval() - get beacon mode powersave
 * maximum listen interval value
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static inline QDF_STATUS
ucfg_mlme_get_bmps_max_listen_interval(struct wlan_objmgr_psoc *psoc,
				       uint32_t *value)
{
	return wlan_mlme_get_bmps_max_listen_interval(psoc, value);
}

/**
 * ucfg_mlme_get_auto_bmps_timer_value() - get bmps timer value
 * minimum listen interval value
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static inline QDF_STATUS
ucfg_mlme_get_auto_bmps_timer_value(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value)
{
	return wlan_mlme_get_auto_bmps_timer_value(psoc, value);
}

/**
 * ucfg_mlme_is_bmps_enabled() - check if beacon mode powersave is
 * enabled/disabled
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static inline QDF_STATUS
ucfg_mlme_is_bmps_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_is_bmps_enabled(psoc, value);
}

/**
 * ucfg_mlme_is_imps_enabled() - check if idle mode powersave is
 * enabled/disabled
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static inline QDF_STATUS
ucfg_mlme_is_imps_enabled(struct wlan_objmgr_psoc *psoc, bool *value)
{
	return wlan_mlme_is_imps_enabled(psoc, value);
}

/**
 * ucfg_mlme_override_bmps_imps() - disable imps/bmps as part of
 * override to disable all ps features
 * @psoc: pointer to psoc object
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static inline QDF_STATUS
ucfg_mlme_override_bmps_imps(struct wlan_objmgr_psoc *psoc)
{
	return wlan_mlme_override_bmps_imps(psoc);
}

#ifdef MWS_COEX
/**
 * ucfg_mlme_get_mws_coex_4g_quick_tdm() - Get mws coex 4g quick tdm
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_mws_coex_4g_quick_tdm(struct wlan_objmgr_psoc *psoc,
				    uint32_t *val);

/**
 * ucfg_mlme_get_mws_coex_5g_nr_pwr_limit() - Get mws coex 5g nr pwr limit
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_mws_coex_5g_nr_pwr_limit(struct wlan_objmgr_psoc *psoc,
				       uint32_t *val);

/**
 * ucfg_mlme_get_mws_coex_pcc_channel_avoid_delay() - Get mws coex pcc
 *                                                    avoid channel delay
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_mws_coex_pcc_channel_avoid_delay(struct wlan_objmgr_psoc *psoc,
					       uint32_t *val);

/**
 * ucfg_mlme_get_mws_coex_scc_channel_avoid_delay() - Get mws coex scc
 *                                                    avoidance channel delay
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_mws_coex_scc_channel_avoid_delay(struct wlan_objmgr_psoc *psoc,
					       uint32_t *val);
#endif

/**
 * ucfg_mlme_get_etsi_srd_chan_in_master_mode  - get etsi srd chan
 * in master mode
 * @psoc:   pointer to psoc object
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_etsi_srd_chan_in_master_mode(struct wlan_objmgr_psoc *psoc,
					   uint8_t *value);

/**
 * ucfg_mlme_get_5dot9_ghz_chan_in_master_mode  - get fcc 5.9 GHz chan
 * in master mode
 * @psoc:   pointer to psoc object
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_5dot9_ghz_chan_in_master_mode(struct wlan_objmgr_psoc *psoc,
					    bool *value);

/**
 * ucfg_mlme_get_srd_master_mode_for_vdev()  - Get SRD master mode for vdev
 * @psoc:          pointer to psoc object
 * @vdev_opmode:   vdev opmode
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_srd_master_mode_for_vdev(struct wlan_objmgr_psoc *psoc,
				       enum QDF_OPMODE vdev_opmode,
				       bool *value);

#ifdef SAP_AVOID_ACS_FREQ_LIST
/**
 * ucfg_mlme_get_acs_avoid_freq_list  - get acs avoid frequency list
 * @psoc: pointer to psoc object
 * @freq_list: Pointer to output freq list
 * @freq_list_num: Pointer to the output number of frequencies filled
 * in the freq_list
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_acs_avoid_freq_list(struct wlan_objmgr_psoc *psoc,
				  uint16_t *freq_list, uint8_t *freq_list_num);

#else
static inline QDF_STATUS
ucfg_mlme_get_acs_avoid_freq_list(struct wlan_objmgr_psoc *psoc,
				  uint16_t *freq_list, uint8_t *freq_list_num)
{
	*freq_list_num = 0;
	return QDF_STATUS_E_INVAL;
}
#endif

/**
 * ucfg_mlme_get_11d_in_world_mode  - get whether 11d is enabled in world mode
 * in master mode
 * @psoc:   pointer to psoc object
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_11d_in_world_mode(struct wlan_objmgr_psoc *psoc,
				bool *value);

/**
 * ucfg_mlme_restart_beaconing_on_ch_avoid() - get restart beaconing on ch avoid
 * @psoc:   pointer to psoc object
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_restart_beaconing_on_ch_avoid(struct wlan_objmgr_psoc *psoc,
					    uint32_t *value);

/**
 * ucfg_mlme_get_indoor_channel_support() - get indoor channel support
 * @psoc:   pointer to psoc object
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_indoor_channel_support(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * ucfg_mlme_get_scan_11d_interval() - get scan 11d interval
 * @psoc: pointer to psoc object
 * @value:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_scan_11d_interval(struct wlan_objmgr_psoc *psoc,
				uint32_t *value);

/**
 * ucfg_mlme_get_nol_across_regdmn() - get scan 11d interval
 * @psoc: pointer to psoc object
 * @value:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */

QDF_STATUS
ucfg_mlme_get_nol_across_regdmn(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * ucfg_mlme_get_valid_channel_freq_list() - get valid channel
 * list
 * @psoc: pointer to psoc object
 * @channel_list: pointer to return channel list
 * @channel_list_num: pointer to return channel list number
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_valid_channel_freq_list(struct wlan_objmgr_psoc *psoc,
				      uint32_t *channel_list,
				      uint32_t *channel_list_num);

#ifdef FEATURE_LFR_SUBNET_DETECTION
/**
 * ucfg_mlme_is_subnet_detection_enabled() - check if sub net detection is
 * enabled/disabled
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Inline UCFG API to be used by HDD/OSIF callers
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
ucfg_mlme_is_subnet_detection_enabled(struct wlan_objmgr_psoc *psoc, bool *val);
#else
static QDF_STATUS
ucfg_mlme_is_subnet_detection_enabled(struct wlan_objmgr_psoc *psoc, bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_LFR_SUBNET_DETECTION */

/**
 * ucfg_mlme_set_current_tx_power_level() - set current tx power level
 * @psoc:   pointer to psoc object
 * @value:  data to be set
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_current_tx_power_level(struct wlan_objmgr_psoc *psoc,
				     uint8_t value);

/**
 * ucfg_mlme_get_current_tx_power_level() - get current tx power level
 * @psoc:   pointer to psoc object
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_current_tx_power_level(struct wlan_objmgr_psoc *psoc,
				     uint8_t *value);

/**
 * ucfg_mlme_set_obss_detection_offload_enabled() - Enable obss offload
 * @psoc:   pointer to psoc object
 * @value:  enable or disable
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_obss_detection_offload_enabled(struct wlan_objmgr_psoc *psoc,
					     uint8_t value);

/**
 * ucfg_mlme_set_obss_color_collision_offload_enabled() - Enable obss color
 * collision offload
 * @psoc:   pointer to psoc object
 * @value:  enable or disable
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_obss_color_collision_offload_enabled(
		struct wlan_objmgr_psoc *psoc, uint8_t value);

/**
 * ucfg_mlme_set_bss_color_collision_det_sta() - Enable bss color
 * collision detection offload for STA mode
 * @psoc:   pointer to psoc object
 * @value:  enable or disable
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_bss_color_collision_det_sta(struct wlan_objmgr_psoc *psoc,
					  uint8_t value);

/**
 * ucfg_mlme_set_restricted_80p80_bw_supp() - Set the restricted 80p80 support
 * @psoc: pointer to psoc object
 * @restricted_80p80_supp: Value to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_mlme_set_restricted_80p80_bw_supp(struct wlan_objmgr_psoc *psoc,
						  bool restricted_80p80_supp);

/**
 * ucfg_mlme_get_restricted_80p80_bw_supp() - Get the restricted 80p80 support
 * @psoc: pointer to psoc object
 *
 * Return: true or false
 */
bool ucfg_mlme_get_restricted_80p80_bw_supp(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_mlme_get_channel_bonding_24ghz() - get channel bonding mode of 24ghz
 * @psoc:   pointer to psoc object
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_channel_bonding_24ghz(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * ucfg_mlme_set_channel_bonding_24ghz() - set channel bonding mode for 24ghz
 * @psoc:   pointer to psoc object
 * @value:  channel bonding mode
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_channel_bonding_24ghz(struct wlan_objmgr_psoc *psoc,
				    uint32_t value);
/**
 * ucfg_mlme_get_channel_bonding_5ghz() - get channel bonding mode of 5ghz
 * @psoc:   pointer to psoc object
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_get_channel_bonding_5ghz(struct wlan_objmgr_psoc *psoc,
				   uint32_t *value);

/**
 * ucfg_mlme_set_channel_bonding_5ghz() - set channel bonding mode for 5ghz
 * @psoc:   pointer to psoc object
 * @value:    channel bonding mode
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_mlme_set_channel_bonding_5ghz(struct wlan_objmgr_psoc *psoc,
				   uint32_t value);

/**
 * ucfg_mlme_get_peer_phymode() - get phymode of peer
 * @psoc: pointer to psoc object
 * @mac:  Pointer to the mac addr of the peer
 * @peer_phymode: phymode
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_peer_phymode(struct wlan_objmgr_psoc *psoc, uint8_t *mac,
			   enum wlan_phymode *peer_phymode)
{
	return mlme_get_peer_phymode(psoc, mac, peer_phymode);
}

/**
 * ucfg_mlme_validate_full_roam_scan_period() - Validate full roam scan period
 * @full_roam_scan_period: Idle period in seconds between two successive
 * full channel roam scans
 *
 * Return: True if full_roam_scan_period is in expected range, false otherwise.
 */
bool ucfg_mlme_validate_full_roam_scan_period(uint32_t full_roam_scan_period);

/**
 * ucfg_mlme_validate_scan_period() - Validate if scan period is in valid range
 * @value: Scan period in msec
 *
 * Return: True if roam_scan_period is in expected range, false otherwise.
 */
bool ucfg_mlme_validate_scan_period(uint32_t roam_scan_period);
/**
 * ucfg_mlme_get_ignore_fw_reg_offload_ind() - Get the
 * ignore_fw_reg_offload_ind ini
 * @psoc: pointer to psoc object
 * @disabled: output pointer to hold user config
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_mlme_get_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc,
					bool *disabled)
{
	return wlan_mlme_get_ignore_fw_reg_offload_ind(psoc, disabled);
}

/**
 * ucfg_mlme_get_peer_unmap_conf() - Indicate if peer unmap confirmation
 * support is enabled or disabled
 * @psoc: pointer to psoc object
 *
 * Return: true if peer unmap confirmation support is enabled, else false
 */
static inline
QDF_STATUS ucfg_mlme_get_peer_unmap_conf(struct wlan_objmgr_psoc *psoc)
{
	return wlan_mlme_get_peer_unmap_conf(psoc);
}

/**
 * ucfg_mlme_get_discon_reason_n_from_ap() - Get disconnect reason and from ap
 * @psoc: PSOC pointer
 * @vdev_id: vdev id
 * @from_ap: Get the from_ap cached through mlme_set_discon_reason_n_from_ap
 *           and copy to this buffer.
 * @reason_code: Get the reason_code cached through
 *               mlme_set_discon_reason_n_from_ap and copy to this buffer.
 *
 * Fetch the contents of from_ap and reason_codes.
 *
 * Return: void
 */
static inline void
ucfg_mlme_get_discon_reason_n_from_ap(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id, bool *from_ap,
				      uint32_t *reason_code)
{
	mlme_get_discon_reason_n_from_ap(psoc, vdev_id, from_ap, reason_code);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * ucfg_mlme_get_roam_reason_vsie_status() - Get roam reason vsie is
 * enabled or disabled
 * @psoc: pointer to psoc object
 * @roam_reason_vsie_enabled: pointer to hold value of roam reason vsie
 *
 * Return: Success if able to get bcn rpt err vsie value, else failure
 */
static inline QDF_STATUS
ucfg_mlme_get_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t *roam_reason_vsie_enabled)
{
	return wlan_mlme_get_roam_reason_vsie_status(psoc,
					roam_reason_vsie_enabled);
}

/**
 * ucfg_mlme_set_roam_reason_vsie_status() - Update roam reason vsie status
 * value with user configured value
 * @psoc: pointer to psoc object
 * @roam_reason_vsie_enabled: value of roam reason vsie status
 *
 * Return: Success if able to get bcn rpt err vsie value, else failure
 */
static inline QDF_STATUS
ucfg_mlme_set_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t roam_reason_vsie_enabled)
{
	return wlan_mlme_set_roam_reason_vsie_status(psoc,
					roam_reason_vsie_enabled);
}

#endif

/**
 * ucfg_mlme_set_user_ps()  - Set the PS user config
 * @psoc: pointer to psoc object
 * @vdev_id: Vdev id
 * @ps_enable: Flag to indicate if user PS is enabled
 *
 * Return: QDF_STATUS
 */
static inline
QDF_STATUS ucfg_mlme_set_user_ps(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 bool ps_enable)
{
	return mlme_set_user_ps(psoc, vdev_id, ps_enable);
}

/**
 * ucfg_mlme_get_user_ps()  - Get user PS flag
 * @psoc: pointer to psoc object
 * @vdev_id: Vdev id
 *
 * Return: True if user ps is enabled else false
 */
static inline
bool ucfg_mlme_get_user_ps(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	return mlme_get_user_ps(psoc, vdev_id);
}

/**
 * ucfg_mlme_is_sta_mon_conc_supported() - Check if STA + Monitor mode
 * concurrency is supported
 * @psoc: pointer to psoc object
 *
 * Return: True if supported, else false.
 */
static inline bool
ucfg_mlme_is_sta_mon_conc_supported(struct wlan_objmgr_psoc *psoc)
{
	return wlan_mlme_is_sta_mon_conc_supported(psoc);
}

#endif /* _WLAN_MLME_UCFG_API_H_ */
