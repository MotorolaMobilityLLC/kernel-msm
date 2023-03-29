/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: declare public APIs exposed by the mlme component
 */

#ifndef _WLAN_MLME_API_H_
#define _WLAN_MLME_API_H_

#include <wlan_mlme_public_struct.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_cmn.h>
#include "sme_api.h"

/**
 * wlan_mlme_get_cfg_str() - Copy the uint8_t array for a particular CFG
 * @dst:       pointer to the destination buffer.
 * @cfg_str:   pointer to the cfg string structure
 * @len:       length to be copied
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_mlme_get_cfg_str(uint8_t *dst, struct mlme_cfg_str *cfg_str,
				 qdf_size_t *len);

/**
 * wlan_mlme_set_cfg_str() - Set values for a particular CFG
 * @src:            pointer to the source buffer.
 * @dst_cfg_str:    pointer to the cfg string structure to be modified
 * @len:            length to be written
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_E_FAILURE
 */
QDF_STATUS wlan_mlme_set_cfg_str(uint8_t *src, struct mlme_cfg_str *dst_cfg_str,
				 qdf_size_t len);

/**
 * wlan_mlme_get_edca_params() - get the EDCA parameters corresponding to the
 * edca profile access category
 * @edca_params:   pointer to mlme edca parameters structure
 * @data:          data to which the parameter is to be copied
 * @edca_ac:       edca ac type enum passed to get the cfg value
 *
 * Return QDF_STATUS_SUCCESS or QDF_STATUS_E_FAILURE
 *
 */
QDF_STATUS wlan_mlme_get_edca_params(struct wlan_mlme_edca_params *edca_params,
				     uint8_t *data, enum e_edca_type edca_ac);

/**
 * wlan_mlme_update_cfg_with_tgt_caps() - Update mlme cfg with tgt caps
 * @psoc: pointer to psoc object
 * @tgt_caps:  Pointer to the mlme related capability structure
 *
 * Return: None
 */
void
wlan_mlme_update_cfg_with_tgt_caps(struct wlan_objmgr_psoc *psoc,
				   struct mlme_tgt_caps *tgt_caps);

/*
 * mlme_get_wep_key() - get the wep key to process during auth frame
 * @vdev: VDEV object for which the wep key is being requested
 * @wep_params: cfg wep parameters structure
 * @wep_key_id: default key number
 * @default_key: default key to be copied
 * @key_len: length of the key to copy
 *
 * Return QDF_STATUS
 */
QDF_STATUS mlme_get_wep_key(struct wlan_objmgr_vdev *vdev,
			    struct wlan_mlme_wep_cfg *wep_params,
			    enum wep_key_id wep_keyid, uint8_t *default_key,
			    qdf_size_t *key_len);

/**
 * mlme_set_wep_key() - set the wep keys during auth
 * @wep_params: cfg wep parametrs structure
 * @wep_key_id: default key number that needs to be copied
 * @key_to_set: destination buffer to be copied
 * @len:        size to be copied
 */
QDF_STATUS mlme_set_wep_key(struct wlan_mlme_wep_cfg *wep_params,
			    enum wep_key_id wep_keyid, uint8_t *key_to_set,
			    qdf_size_t len);

/**
 * wlan_mlme_get_tx_power() - Get the max tx power in particular band
 * @psoc: pointer to psoc object
 * @band: 2ghz/5ghz band
 *
 * Return: value of tx power in the respective band
 */
uint8_t wlan_mlme_get_tx_power(struct wlan_objmgr_psoc *psoc,
			       enum band_info band);

/**
 * wlan_mlme_get_power_usage() - Get the power usage info
 * @psoc: pointer to psoc object
 *
 * Return: pointer to character array of power usage
 */
char *wlan_mlme_get_power_usage(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_ht_cap_info() - Get the HT cap info config
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_ht_cap_info(struct wlan_objmgr_psoc *psoc,
				     struct mlme_ht_capabilities_info
				     *ht_cap_info);

/**
 * wlan_mlme_get_manufacturer_name() - get manufacturer name
 * @psoc: pointer to psoc object
 * @pbuf: pointer of the buff which will be filled for the caller
 * @plen: pointer of max buffer length
 *        actual length will be returned at this address
 * This function gets manufacturer name
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
wlan_mlme_get_manufacturer_name(struct wlan_objmgr_psoc *psoc,
				uint8_t *pbuf, uint32_t *plen);

/**
 * wlan_mlme_get_model_number() - get model number
 * @psoc: pointer to psoc object
 * @pbuf: pointer of the buff which will be filled for the caller
 * @plen: pointer of max buffer length
 *        actual length will be returned at this address
 * This function gets model number
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
wlan_mlme_get_model_number(struct wlan_objmgr_psoc *psoc,
			   uint8_t *pbuf, uint32_t *plen);

/**
 * wlan_mlme_get_model_name() - get model name
 * @psoc: pointer to psoc object
 * @pbuf: pointer of the buff which will be filled for the caller
 * @plen: pointer of max buffer length
 *        actual length will be returned at this address
 * This function gets model name
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
wlan_mlme_get_model_name(struct wlan_objmgr_psoc *psoc,
			 uint8_t *pbuf, uint32_t *plen);

/**
 * wlan_mlme_get_manufacture_product_name() - get manufacture product name
 * @psoc: pointer to psoc object
 * @pbuf: pointer of the buff which will be filled for the caller
 * @plen: pointer of max buffer length
 *        actual length will be returned at this address
 * This function gets manufacture product name
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
wlan_mlme_get_manufacture_product_name(struct wlan_objmgr_psoc *psoc,
				       uint8_t *pbuf, uint32_t *plen);

/**
 * wlan_mlme_get_manufacture_product_version() - get manufacture product version
 * @psoc: pointer to psoc object
 * @pbuf: pointer of the buff which will be filled for the caller
 * @plen: pointer of max buffer length
 *        actual length will be returned at this address
 * This function gets manufacture product version
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS
wlan_mlme_get_manufacture_product_version(struct wlan_objmgr_psoc *psoc,
					  uint8_t *pbuf, uint32_t *plen);

/**
 * wlan_mlme_set_ht_cap_info() - Set the HT cap info config
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_ht_cap_info(struct wlan_objmgr_psoc *psoc,
				     struct mlme_ht_capabilities_info
				     ht_cap_info);

/**
 * wlan_mlme_get_max_amsdu_num() - get the max amsdu num
 * @psoc: pointer to psoc object
 * @value: pointer to the value where the max_amsdu num is to be filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_max_amsdu_num(struct wlan_objmgr_psoc *psoc,
				       uint8_t *value);

/**
 * wlan_mlme_set_max_amsdu_num() - set the max amsdu num
 * @psoc: pointer to psoc object
 * @value: value to be set for max_amsdu_num
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_set_max_amsdu_num(struct wlan_objmgr_psoc *psoc,
				       uint8_t value);

/**
 * wlan_mlme_get_ht_mpdu_density() - get the ht mpdu density
 * @psoc: pointer to psoc object
 * @value: pointer to the value where the ht mpdu density is to be filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_ht_mpdu_density(struct wlan_objmgr_psoc *psoc,
					 uint8_t *value);

/**
 * wlan_mlme_set_ht_mpdu_density() - set the ht mpdu density
 * @psoc: pointer to psoc object
 * @value: value to be set for ht mpdu density
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_set_ht_mpdu_density(struct wlan_objmgr_psoc *psoc,
					 uint8_t value);

/**
 * wlan_mlme_get_band_capability() - Get the Band capability config
 * @psoc: pointer to psoc object
 * @band_capability: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_band_capability(struct wlan_objmgr_psoc *psoc,
					 uint32_t *band_capability);

/**
 * wlan_mlme_set_band_capability() - Set the Band capability config
 * @psoc: pointer to psoc object
 * @band_capability: Value to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_band_capability(struct wlan_objmgr_psoc *psoc,
					 uint32_t band_capability);

/**
 * wlan_mlme_get_prevent_link_down() - Get the prevent link down config
 * @psoc: pointer to psoc object
 * @prevent_link_down: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_prevent_link_down(struct wlan_objmgr_psoc *psoc,
					   bool *prevent_link_down);

/**
 * wlan_mlme_get_select_5ghz_margin() - Get the select 5Ghz margin config
 * @psoc: pointer to psoc object
 * @select_5ghz_margin: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_select_5ghz_margin(struct wlan_objmgr_psoc *psoc,
					    uint8_t *select_5ghz_margin);

/**
 * wlan_mlme_get_rtt_mac_randomization() - Get the RTT MAC randomization config
 * @psoc: pointer to psoc object
 * @rtt_mac_randomization: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_rtt_mac_randomization(struct wlan_objmgr_psoc *psoc,
					       bool *rtt_mac_randomization);

/**
 * wlan_mlme_get_crash_inject() - Get the crash inject config
 * @psoc: pointer to psoc object
 * @crash_inject: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_crash_inject(struct wlan_objmgr_psoc *psoc,
				      bool *crash_inject);

/**
 * wlan_mlme_get_lpass_support() - Get the LPASS Support config
 * @psoc: pointer to psoc object
 * @lpass_support: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_lpass_support(struct wlan_objmgr_psoc *psoc,
				       bool *lpass_support);

/**
 * wlan_mlme_get_wls_6ghz_cap() - Get the wifi location service(WLS)
 * 6ghz capability
 * @psoc: pointer to psoc object
 * @wls_6ghz_capable: Pointer to the variable from caller
 *
 * Return: void
 */
void wlan_mlme_get_wls_6ghz_cap(struct wlan_objmgr_psoc *psoc,
				bool *wls_6ghz_capable);

/**
 * wlan_mlme_get_self_recovery() - Get the self recovery config
 * @psoc: pointer to psoc object
 * @self_recovery: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_self_recovery(struct wlan_objmgr_psoc *psoc,
				       bool *self_recovery);

/**
 * wlan_mlme_get_sub_20_chan_width() - Get the sub 20 chan width config
 * @psoc: pointer to psoc object
 * @sub_20_chan_width: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sub_20_chan_width(struct wlan_objmgr_psoc *psoc,
					   uint8_t *sub_20_chan_width);

/**
 * wlan_mlme_get_fw_timeout_crash() - Get the fw timeout crash config
 * @psoc: pointer to psoc object
 * @fw_timeout_crash: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_fw_timeout_crash(struct wlan_objmgr_psoc *psoc,
					  bool *fw_timeout_crash);

/**
 * wlan_mlme_get_ito_repeat_count() - Get the fw timeout crash config
 * @psoc: pointer to psoc object
 * @ito_repeat_count: Pointer to the variable from caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_ito_repeat_count(struct wlan_objmgr_psoc *psoc,
					  uint8_t *ito_repeat_count);

/**
 * wlan_mlme_get_acs_with_more_param() - Get the acs_with_more_param flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_acs_with_more_param(struct wlan_objmgr_psoc *psoc,
					     bool *value);

/**
 * wlan_mlme_get_auto_channel_weight() - Get the auto channel weight
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_auto_channel_weight(struct wlan_objmgr_psoc *psoc,
					     uint32_t *value);

/**
 * wlan_mlme_get_vendor_acs_support() - Get the vendor based channel selece
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */

QDF_STATUS wlan_mlme_get_vendor_acs_support(struct wlan_objmgr_psoc *psoc,
					    bool *value);

/**
 * wlan_mlme_get_acs_support_for_dfs_ltecoex() - Get the flag for
 *						 acs support for dfs ltecoex
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_acs_support_for_dfs_ltecoex(struct wlan_objmgr_psoc *psoc,
					  bool *value);

/**
 * wlan_mlme_get_external_acs_policy() - Get the flag for external acs policy
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_external_acs_policy(struct wlan_objmgr_psoc *psoc,
				  bool *value);

/**
 *
 * wlan_mlme_get_sap_inactivity_override() - Check if sap max inactivity
 * override flag is set.
 * @psoc: pointer to psoc object
 * @sme_config - Sme config struct
 *
 * Return: QDF Status
 */
void wlan_mlme_get_sap_inactivity_override(struct wlan_objmgr_psoc *psoc,
					   bool *value);

/**
 * wlan_mlme_get_ignore_peer_ht_mode() - Get the ignore peer ht opmode flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_ignore_peer_ht_mode(struct wlan_objmgr_psoc *psoc,
					bool *value);
/**
 * wlan_mlme_get_tx_chainmask_cck() - Get the tx_chainmask_cfg value
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_get_tx_chainmask_cck(struct wlan_objmgr_psoc *psoc,
					  bool *value);

/**
 * wlan_mlme_get_tx_chainmask_1ss() - Get the tx_chainmask_1ss value
 * @psoc: pointer to psoc object
 * @value: Value that caller needs to get
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_get_tx_chainmask_1ss(struct wlan_objmgr_psoc *psoc,
					  uint8_t *value);

/**
 * wlan_mlme_get_num_11b_tx_chains() -  Get the number of 11b only tx chains
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_get_num_11b_tx_chains(struct wlan_objmgr_psoc *psoc,
					   uint16_t *value);

/**
 * wlan_mlme_get_num_11ag_tx_chains() - get the total number of 11a/g tx chains
 * @psoc: pointer to psoc object
 * @value: Value that caller needs to get
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_get_num_11ag_tx_chains(struct wlan_objmgr_psoc *psoc,
					    uint16_t *value);

/**
 * wlan_mlme_get_bt_chain_separation_flag() - get the enable_bt_chain_separation
 * flag
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_get_bt_chain_separation_flag(struct wlan_objmgr_psoc *psoc,
						  bool *value);
/**
 * wlan_mlme_configure_chain_mask() - configure chainmask parameters
 * @psoc: pointer to psoc object
 * @session_id: vdev_id
 *
 * Return: QDF_STATUS_FAILURE or QDF_STATUS_SUCCESS
 */
QDF_STATUS wlan_mlme_configure_chain_mask(struct wlan_objmgr_psoc *psoc,
					  uint8_t session_id);

/**
 * wlan_mlme_is_chain_mask_supported() - check if configure chainmask can
 * be supported
 * @psoc: pointer to psoc object
 *
 * Return: true if supported else false
 */
bool wlan_mlme_is_chain_mask_supported(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_listen_interval() - Get listen interval
 * @psoc: pointer to psoc object
 * @value: Pointer to value that needs to be filled by MLME
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_listen_interval(struct wlan_objmgr_psoc *psoc,
					     int *value);

/**
 * wlan_mlme_set_sap_listen_interval() - Set the sap listen interval
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_sap_listen_interval(struct wlan_objmgr_psoc *psoc,
					     int value);

/**
 * wlan_mlme_set_assoc_sta_limit() - Set the assoc sta limit
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_assoc_sta_limit(struct wlan_objmgr_psoc *psoc,
					 int value);

/**
 * wlan_mlme_get_assoc_sta_limit() - Get the assoc sta limit
 * @psoc: pointer to psoc object
 * @value: Pointer to value that needs to be filled by MLME
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_assoc_sta_limit(struct wlan_objmgr_psoc *psoc,
					 int *value);

/**
 * wlan_mlme_set_sap_get_peer_info() - get the sap get peer info
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_get_peer_info(struct wlan_objmgr_psoc *psoc,
					   bool *value);

/**
 * wlan_mlme_set_sap_get_peer_info() - set the sap get peer info
 * @psoc: pointer to psoc object
 * @value: value to overwrite the sap get peer info
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_sap_get_peer_info(struct wlan_objmgr_psoc *psoc,
					   bool value);

/**
 * wlan_mlme_get_sap_bcast_deauth_enabled() - get the enable/disable value
 *                                           for broadcast deauth in sap
 * @psoc: pointer to psoc object
 * @value: Value that needs to get from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_sap_bcast_deauth_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *value);

/**
 * wlan_mlme_get_sap_allow_all_channels() - get the value of sap allow all
 * channels
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_allow_all_channels(struct wlan_objmgr_psoc *psoc,
						bool *value);

/**
 * wlan_mlme_is_6g_sap_fd_enabled() - get the enable/disable value
 *                                     for 6g sap fils discovery
 * @psoc: pointer to psoc object
 * @value: Value that needs to get from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_is_6g_sap_fd_enabled(struct wlan_objmgr_psoc *psoc,
			       bool *value);

/**
 * wlan_mlme_get_sap_allow_all_channels() - get the value sap max peers
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_max_peers(struct wlan_objmgr_psoc *psoc,
				       int *value);

/**
 * wlan_mlme_set_sap_max_peers() - set the value sap max peers
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_sap_max_peers(struct wlan_objmgr_psoc *psoc,
				       int value);

/**
 * wlan_mlme_get_sap_max_offload_peers() - get the value sap max offload peers
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_max_offload_peers(struct wlan_objmgr_psoc *psoc,
					       int *value);

/**
 * wlan_mlme_get_sap_max_offload_reorder_buffs() - get the value sap max offload
 * reorder buffs.
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_max_offload_reorder_buffs(struct wlan_objmgr_psoc
						       *psoc, int *value);

/**
 * wlan_mlme_get_sap_chn_switch_bcn_count() - get the value sap max channel
 * switch beacon count
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_chn_switch_bcn_count(struct wlan_objmgr_psoc *psoc,
						  int *value);

/**
 * wlan_mlme_get_sap_chn_switch_mode() - get the sap channel
 * switch mode
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_chn_switch_mode(struct wlan_objmgr_psoc *psoc,
					     bool *value);

/**
 * wlan_mlme_get_sap_internal_restart() - get the sap internal
 * restart
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_internal_restart(struct wlan_objmgr_psoc *psoc,
					      bool *value);
/**
 * wlan_mlme_get_sap_max_modulated_dtim() - get the max modulated dtim
 * restart
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_max_modulated_dtim(struct wlan_objmgr_psoc *psoc,
						uint8_t *value);

/**
 * wlan_mlme_get_sap_chan_pref_location() - get the sap chan pref location
 * restart
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_chan_pref_location(struct wlan_objmgr_psoc *psoc,
						uint8_t *value);

/**
 * wlan_mlme_get_sap_country_priority() - get the sap country code priority
 * restart
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_country_priority(struct wlan_objmgr_psoc *psoc,
					      bool *value);

/**
 * wlan_mlme_get_sap_reduced_beacon_interval() - get the sap reduced
 * beacon interval
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_reduced_beacon_interval(struct wlan_objmgr_psoc
						     *psoc, int *value);

/**
 * wlan_mlme_get_sap_chan_switch_rate_enabled() - get the sap rate hostapd
 * enabled beacon interval
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_chan_switch_rate_enabled(struct wlan_objmgr_psoc
						      *psoc, bool *value);

/**
 * wlan_mlme_get_sap_force_11n_for_11ac() - get the sap 11n for 11ac
 *
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_sap_force_11n_for_11ac(struct wlan_objmgr_psoc
						*psoc, bool *value);

/**
 * wlan_mlme_get_go_force_11n_for_11ac() - get the go 11n for 11ac
 *
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_go_force_11n_for_11ac(struct wlan_objmgr_psoc
					       *psoc, bool *value);

/**
 * wlan_mlme_is_go_11ac_override() - Override 11ac bandwdith for P2P GO
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_is_go_11ac_override(struct wlan_objmgr_psoc *psoc,
					 bool *value);

/**
 * wlan_mlme_is_sap_11ac_override() - Override 11ac bandwdith for SAP
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_is_sap_11ac_override(struct wlan_objmgr_psoc *psoc,
					  bool *value);

/**
 * wlan_mlme_set_go_11ac_override() - set override 11ac bandwdith for P2P GO
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_go_11ac_override(struct wlan_objmgr_psoc *psoc,
					  bool value);

/**
 * wlan_mlme_set_sap_11ac_override() - set override 11ac bandwdith for SAP
 *
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_sap_11ac_override(struct wlan_objmgr_psoc *psoc,
					   bool value);

/**
 * wlan_mlme_get_oce_sta_enabled_info() - Get the OCE feature enable
 * info for STA
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_oce_sta_enabled_info(struct wlan_objmgr_psoc *psoc,
					      bool *value);

/**
 * wlan_mlme_get_bigtk_support() - Get the BIGTK support
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_bigtk_support(struct wlan_objmgr_psoc *psoc,
				       bool *value);

/**
 * wlan_mlme_get_ocv_support() - Get the OCV support
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_ocv_support(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * wlan_mlme_get_host_scan_abort_support() - Get support for stop all host
 * scans service capability.
 * @psoc: PSOC object pointer
 *
 * Return: True if capability is supported, else False
 */
bool wlan_mlme_get_host_scan_abort_support(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_dual_sta_roam_support  - Get support for dual sta roaming
 * feature
 * @psoc: PSOC object pointer
 *
 * Return: True if capability is supported, else False
 */
bool wlan_mlme_get_dual_sta_roam_support(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_oce_sap_enabled_info() - Get the OCE feature enable
 * info for SAP
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_oce_sap_enabled_info(struct wlan_objmgr_psoc *psoc,
					      bool *value);

/**
 * wlan_mlme_update_oce_flags() - Update the oce flags to FW
 * @pdev: pointer to pdev object
 *
 * Return: void
 */
void wlan_mlme_update_oce_flags(struct wlan_objmgr_pdev *pdev);

#ifdef WLAN_FEATURE_11AX
/**
 * wlan_mlme_cfg_get_he_ul_mumimo() - Get the HE Ul Mumio
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_cfg_get_he_ul_mumimo(struct wlan_objmgr_psoc *psoc,
					  uint32_t *value);

/**
 * wlan_mlme_cfg_set_he_ul_mumimo() - Set the HE Ul Mumio
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_cfg_set_he_ul_mumimo(struct wlan_objmgr_psoc *psoc,
					  uint32_t value);

/**
 * mlme_cfg_get_he_caps() - Get the HE capability info
 * @psoc: pointer to psoc object
 * @he_cap: Caps that needs to be filled.
 *
 * Return: QDF Status
 */
QDF_STATUS mlme_cfg_get_he_caps(struct wlan_objmgr_psoc *psoc,
				tDot11fIEhe_cap *he_cap);

/**
 * wlan_mlme_cfg_get_enable_ul_mimo() - Get the HE Ul mimo
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_cfg_get_enable_ul_mimo(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value);

/**
 * wlan_mlme_cfg_get_enable_ul_ofdm() - Get enable ul ofdm
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_cfg_get_enable_ul_ofdm(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value);

/**
 * mlme_update_tgt_he_caps_in_cfg() - Update tgt he cap in mlme component
 *
 * @psoc: pointer to psoc object
 * @cfg: pointer to config params from target
 *
 * This api to be used by callers to update
 * he caps in mlme.
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS mlme_update_tgt_he_caps_in_cfg(struct wlan_objmgr_psoc *psoc,
					  struct wma_tgt_cfg *cfg);
#endif

/**
 * wlan_mlme_is_ap_prot_enabled() - check if sap protection is enabled
 * @psoc: pointer to psoc object
 *
 * Return: is_ap_prot_enabled flag
 */
bool wlan_mlme_is_ap_prot_enabled(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_ap_protection_mode() - Get ap_protection_mode value
 * @psoc: pointer to psoc object
 * @value: pointer to the value which needs to be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_ap_protection_mode(struct wlan_objmgr_psoc *psoc,
					    uint16_t *value);

/**
 * wlan_mlme_is_ap_obss_prot_enabled() - Get ap_obss_protection is
 * enabled/disabled
 * @psoc: pointer to psoc object
 * @value: pointer to the value which needs to be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_is_ap_obss_prot_enabled(struct wlan_objmgr_psoc *psoc,
					     bool *value);

/**
 * wlan_mlme_get_rts_threshold() - Get the RTS threshold config
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_rts_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t *value);

/**
 * wlan_mlme_set_rts_threshold() - Set the RTS threshold config
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_rts_threshold(struct wlan_objmgr_psoc *psoc,
				       uint32_t value);

/**
 * wlan_mlme_get_frag_threshold() - Get the Fragmentation threshold
 *                                  config
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_frag_threshold(struct wlan_objmgr_psoc *psoc,
					uint32_t *value);

/**
 * wlan_mlme_set_frag_threshold() - Set the Fragmentation threshold
 *                                  config
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_frag_threshold(struct wlan_objmgr_psoc *psoc,
					uint32_t value);

/**
 * wlan_mlme_get_fils_enabled_info() - Get the fils enable info for driver
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_fils_enabled_info(struct wlan_objmgr_psoc *psoc,
					   bool *value);
/**
 * wlan_mlme_set_fils_enabled_info() - Set the fils enable info for driver
 * @psoc: pointer to psoc object
 * @value: value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_fils_enabled_info(struct wlan_objmgr_psoc *psoc,
					   bool value);

/**
 * wlan_mlme_get_tl_delayed_trgr_frm_int() - Get delay interval(in ms)
 * of UAPSD auto trigger
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: None
 */
void wlan_mlme_get_tl_delayed_trgr_frm_int(struct wlan_objmgr_psoc *psoc,
					   uint32_t *value);

/**
 * wlan_mlme_get_wmm_dir_ac_vi() - Get TSPEC direction
 * for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_dir_ac_vi(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_wmm_nom_msdu_size_ac_vi() - Get normal
 * MSDU size for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_vi(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value);

/**
 * wlan_mlme_get_wmm_mean_data_rate_ac_vi() - mean data
 * rate for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_mean_data_rate_ac_vi(struct wlan_objmgr_psoc *psoc,
					uint32_t *value);

/**
 * wlan_mlme_get_wmm_min_phy_rate_ac_vi() - min PHY
 * rate for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_vi(struct wlan_objmgr_psoc *psoc,
						uint32_t *value);

/**
 * wlan_mlme_get_wmm_sba_ac_vi() - surplus bandwidth
 * allowance for VI
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_sba_ac_vi(struct wlan_objmgr_psoc *psoc, uint16_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_vi_srv_intv() - Get Uapsd service
 * interval for video
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_vi_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_vi_sus_intv() - Get Uapsd suspension
 * interval for video
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_vi_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_dir_ac_be() - Get TSPEC direction
 * for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_dir_ac_be(struct wlan_objmgr_psoc *psoc,
			    uint8_t *value);

/**
 * wlan_mlme_get_wmm_nom_msdu_size_ac_be() - Get normal
 * MSDU size for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_be(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value);

/**
 * wlan_mlme_get_wmm_mean_data_rate_ac_be() - mean data
 * rate for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_mean_data_rate_ac_be(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value);

/**
 * wlan_mlme_get_wmm_min_phy_rate_ac_be() - min PHY
 * rate for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_be(struct wlan_objmgr_psoc *psoc,
						uint32_t *value);

/**
 * wlan_mlme_get_wmm_sba_ac_be() - surplus bandwidth
 * allowance for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_sba_ac_be(struct wlan_objmgr_psoc *psoc, uint16_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_be_srv_intv() - Get Uapsd service
 * interval for BE
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_be_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_be_sus_intv() - Get Uapsd suspension
 * interval for BE
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_be_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_dir_ac_bk() - Get TSPEC direction
 * for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_dir_ac_bk(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_wmm_nom_msdu_size_ac_bk() - Get normal
 * MSDU size for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_bk(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value);

/**
 * wlan_mlme_get_wmm_mean_data_rate_ac_bk() - mean data
 * rate for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_mean_data_rate_ac_bk(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value);

/**
 * wlan_mlme_get_wmm_min_phy_rate_ac_bk() - min PHY
 * rate for BK
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_bk(struct wlan_objmgr_psoc *psoc,
						uint32_t *value);

/**
 * wlan_mlme_get_wmm_sba_ac_bk() - surplus bandwidth
 * allowance for BE
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_sba_ac_bk(struct wlan_objmgr_psoc *psoc, uint16_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_bk_srv_intv() - Get Uapsd service
 * interval for BK
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_bk_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_bk_sus_intv() - Get Uapsd suspension
 * interval for BK
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_bk_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_mode() - Enable WMM feature
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_mode(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_80211e_is_enabled() - Enable 802.11e feature
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_80211e_is_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_wmm_uapsd_mask() - setup U-APSD mask for ACs
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_mask(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_implicit_qos_is_enabled() - Enable implicit QOS
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_implicit_qos_is_enabled(struct wlan_objmgr_psoc *psoc,
						 bool *value);

#ifdef FEATURE_WLAN_ESE
/**
 * wlan_mlme_get_inactivity_interval() - Infra Inactivity Interval
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: None
 */
void
wlan_mlme_get_inactivity_interval(struct wlan_objmgr_psoc *psoc,
				  uint32_t *value);
#endif

/**
 * wlan_mlme_get_is_ts_burst_size_enable() - Get TS burst size flag
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: None
 */
void wlan_mlme_get_is_ts_burst_size_enable(struct wlan_objmgr_psoc *psoc,
					   bool *value);

/**
 * wlan_mlme_get_ts_info_ack_policy() - Get TS ack policy
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: None
 */
void wlan_mlme_get_ts_info_ack_policy(struct wlan_objmgr_psoc *psoc,
				      enum mlme_ts_info_ack_policy *value);

/**
 * wlan_mlme_get_ts_acm_value_for_ac() - Get ACM value for AC
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_ts_acm_value_for_ac(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_wmm_dir_ac_vo() - Get TSPEC direction
 * for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_dir_ac_vo(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_wmm_nom_msdu_size_ac_vo() - Get normal
 * MSDU size for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_nom_msdu_size_ac_vo(struct wlan_objmgr_psoc *psoc,
						 uint16_t *value);

/**
 * wlan_mlme_get_wmm_mean_data_rate_ac_vo() - mean data rate for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_mean_data_rate_ac_vo(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value);
/**
 * wlan_mlme_get_wmm_min_phy_rate_ac_vo() - min PHY
 * rate for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_wmm_min_phy_rate_ac_vo(struct wlan_objmgr_psoc *psoc,
						uint32_t *value);
/**
 * wlan_mlme_get_wmm_sba_ac_vo() - surplus bandwidth allowance for VO
 * @psoc: pointer to psoc object
 * @value: Value that needs to be set from the caller
 *
 *  Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_sba_ac_vo(struct wlan_objmgr_psoc *psoc, uint16_t *value);

/**
 * wlan_mlme_set_enable_bcast_probe_rsp() - Set enable bcast probe resp info
 * @psoc: pointer to psoc object
 * @value: value that needs to be set from the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_set_enable_bcast_probe_rsp(struct wlan_objmgr_psoc *psoc,
						bool value);

/**
 * wlan_mlme_get_wmm_uapsd_vo_srv_intv() - Get Uapsd service
 * interval for voice
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_vo_srv_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_get_wmm_uapsd_vo_sus_intv() - Get Uapsd suspension
 * interval for voice
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_wmm_uapsd_vo_sus_intv(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_cfg_get_vht_max_mpdu_len() - gets vht max mpdu length from cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_max_mpdu_len(struct wlan_objmgr_psoc *psoc,
				   uint8_t *value);

/**
 * wlan_mlme_cfg_set_vht_max_mpdu_len() - sets vht max mpdu length into cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_max_mpdu_len(struct wlan_objmgr_psoc *psoc,
				   uint8_t value);

/**
 * wlan_mlme_cfg_get_vht_chan_width() - gets vht supported channel width from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_chan_width(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value);

/**
 * wlan_mlme_cfg_set_vht_chan_width() - sets vht supported channel width into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_chan_width(struct wlan_objmgr_psoc *psoc,
					    uint8_t value);

/**
 * wlan_mlme_cfg_get_vht_chan_width() - sets vht supported channel width into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_chan_width(struct wlan_objmgr_psoc *psoc,
					    uint8_t *value);

/**
 * wlan_mlme_cfg_get_vht_ldpc_coding_cap() - gets vht ldpc coding cap from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_ldpc_coding_cap(struct wlan_objmgr_psoc *psoc,
						 bool *value);

/**
 * wlan_mlme_cfg_set_vht_ldpc_coding_cap() - sets vht ldpc coding cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_ldpc_coding_cap(struct wlan_objmgr_psoc *psoc,
						 bool value);

/**
 * wlan_mlme_cfg_get_vht_short_gi_80mhz() - gets vht short gi 80MHz from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_short_gi_80mhz(struct wlan_objmgr_psoc *psoc,
						bool *value);

/**
 * wlan_mlme_cfg_set_vht_short_gi_80mhz() - sets vht short gi 80MHz into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_short_gi_80mhz(struct wlan_objmgr_psoc *psoc,
						bool value);

/**
 * wlan_mlme_cfg_get_short_gi_160_mhz() - gets vht short gi 160MHz from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_short_gi_160_mhz(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_short_gi_160_mhz() - sets vht short gi 160MHz into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_short_gi_160_mhz(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_tx_stbc() - gets vht tx stbc from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_tx_stbc(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_get_vht_rx_stbc() - gets vht rx stbc from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_rx_stbc(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_tx_stbc() - sets vht tx stbc into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_tx_stbc(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_rx_stbc() - gets vht rx stbc from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_rx_stbc(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_rx_stbc() - sets vht rx stbc into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_rx_stbc(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_su_bformer() - gets vht su beam former cap from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_su_bformer(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_su_bformer() - sets vht su beam former cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_su_bformer(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_set_vht_su_bformee() - sets vht su beam formee cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_su_bformee(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_set_vht_tx_bfee_ant_supp() - sets vht Beamformee antenna
 * support cap
 * into cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_tx_bfee_ant_supp(struct wlan_objmgr_psoc *psoc,
						  uint8_t value);

/**
 * wlan_mlme_cfg_get_vht_tx_bfee_ant_supp() - Gets vht Beamformee antenna
 * support cap into cfg item
 *
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_tx_bfee_ant_supp(struct wlan_objmgr_psoc *psoc,
						  uint8_t *value);

/**
 * wlan_mlme_cfg_set_vht_num_sounding_dim() - sets vht no of sounding dimensions
 * into cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_num_sounding_dim(struct wlan_objmgr_psoc *psoc,
						  uint8_t value);

/**
 * wlan_mlme_cfg_get_vht_mu_bformer() - gets vht mu beam former cap from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_mu_bformer(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_mu_bformer() - sets vht mu beam former cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_mu_bformer(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_mu_bformee() - gets vht mu beam formee cap from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_mu_bformee(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_mu_bformee() - sets vht mu beam formee cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_mu_bformee(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_txop_ps() - gets vht tx ops ps cap from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_txop_ps(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_cfg_set_vht_txop_ps() - sets vht tx ops ps cap into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_txop_ps(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_cfg_get_vht_ampdu_len_exp() - gets vht max AMPDU length exponent from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_ampdu_len_exp(struct wlan_objmgr_psoc *psoc,
				    uint8_t *value);

/**
 * wlan_mlme_cfg_set_vht_ampdu_len_exp() - sets vht max AMPDU length exponent into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_ampdu_len_exp(struct wlan_objmgr_psoc *psoc,
				    uint8_t value);

/**
 * wlan_mlme_cfg_get_vht_rx_mcs_map() - gets vht rx mcs map from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_rx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t *value);

/**
 * wlan_mlme_cfg_set_vht_rx_mcs_map() - sets rx mcs map into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_rx_mcs_map(struct wlan_objmgr_psoc *psoc, uint32_t value);

/**
 * wlan_mlme_cfg_get_vht_tx_mcs_map() - gets vht tx mcs map from
 * cfg item
 * @psoc: psoc context
 * @value: pointer to get required data
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_get_vht_tx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t *value);

/**
 * wlan_mlme_cfg_set_vht_tx_mcs_map() - sets tx mcs map into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_cfg_set_vht_tx_mcs_map(struct wlan_objmgr_psoc *psoc,
					    uint32_t value);

/**
 * wlan_mlme_cfg_set_vht_rx_supp_data_rate() - sets rx supported data rate into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_rx_supp_data_rate(struct wlan_objmgr_psoc *psoc,
					uint32_t value);

/**
 * wlan_mlme_cfg_set_vht_tx_supp_data_rate() - sets tx supported data rate into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_tx_supp_data_rate(struct wlan_objmgr_psoc *psoc,
					uint32_t value);

/**
 * wlan_mlme_cfg_get_vht_basic_mcs_set() - gets basic mcs set from
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_get_vht_basic_mcs_set(struct wlan_objmgr_psoc *psoc,
				    uint32_t *value);

/**
 * wlan_mlme_cfg_set_vht_basic_mcs_set() - sets basic mcs set into
 * cfg item
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_cfg_set_vht_basic_mcs_set(struct wlan_objmgr_psoc *psoc,
				    uint32_t value);

/**
 * wlan_mlme_get_vht_enable_tx_bf() - Get vht enable tx bf
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_enable_tx_bf(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_vht_tx_su_beamformer() - VHT enable tx su beamformer
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_tx_su_beamformer(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_vht_channel_width() - gets Channel width capability
 * for 11ac
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_vht_channel_width(struct wlan_objmgr_psoc *psoc,
					   uint8_t *value);

/**
 * wlan_mlme_get_vht_rx_mcs_8_9() - VHT Rx MCS capability for 1x1 mode
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_vht_rx_mcs_8_9(struct wlan_objmgr_psoc *psoc,
					uint8_t *value);

/**
 * wlan_mlme_get_vht_tx_mcs_8_9() - VHT Tx MCS capability for 1x1 mode
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_tx_mcs_8_9(struct wlan_objmgr_psoc *psoc, uint8_t *value);

/**
 * wlan_mlme_get_vht_rx_mcs_2x2() - VHT Rx MCS capability for 2x2 mode
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_vht_rx_mcs_2x2(struct wlan_objmgr_psoc *psoc,
					uint8_t *value);

/**
 * wlan_mlme_get_vht_tx_mcs_2x2() - VHT Tx MCS capability for 2x2 mode
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_get_vht_tx_mcs_2x2(struct wlan_objmgr_psoc *psoc,
					uint8_t *value);

/**
 * wlan_mlme_get_vht20_mcs9() - Enables VHT MCS9 in 20M BW operation
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht20_mcs9(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_srd_master_mode_for_vdev  - Get SRD master mode for vdev
 * @psoc:          pointer to psoc object
 * @vdev_opmode:   vdev operating mode
 * @value:  pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_srd_master_mode_for_vdev(struct wlan_objmgr_psoc *psoc,
				       enum QDF_OPMODE vdev_opmode,
				       bool *value);

/**
 * wlan_mlme_get_indoor_support_for_nan  - Get indoor channel support for NAN
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_indoor_support_for_nan(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * wlan_mlme_get_force_sap_enabled() - Get the value of force SAP enabled
 * @psoc: psoc context
 * @value: data to get
 *
 * Get the value of force SAP enabled
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS
wlan_mlme_get_force_sap_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_enable_dynamic_nss_chains_cfg() - API to get whether dynamic
 * nss and chain config is enabled or not
 * @psoc: psoc context
 * @value: data to be set
 *
 * API to get whether dynamic nss and chain config is enabled or not
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_FAILURE
 */
QDF_STATUS
wlan_mlme_get_enable_dynamic_nss_chains_cfg(struct wlan_objmgr_psoc *psoc,
					    bool *value);

/**
 * wlan_mlme_get_vht_enable2x2() - Enables/disables VHT Tx/Rx MCS values for 2x2
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_enable2x2(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_vht_enable2x2() - Enables/disables VHT Tx/Rx MCS values for 2x2
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_vht_enable2x2(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_get_vht_enable_paid() - Enables/disables paid feature
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_enable_paid(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_vht_enable_gid() - Enables/disables VHT GID feature
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_enable_gid(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_get_vht_for_24ghz() - Enables/disables VHT for 24 ghz
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_vht_for_24ghz() - Enables/disables VHT for 24 ghz
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_get_vendor_vht_for_24ghz() - nables/disables vendor VHT for 24 ghz
 * @psoc: psoc context
 * @value: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_vendor_vht_for_24ghz(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * mlme_update_vht_cap() - update vht capabilities
 * @psoc: psoc context
 * @cfg: data to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlme_update_vht_cap(struct wlan_objmgr_psoc *psoc, struct wma_tgt_vht_cap *cfg);

/**
 * mlme_update_nss_vht_cap() - Update the number of spatial
 * streams supported for vht
 * @psoc: psoc context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_update_nss_vht_cap(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_is_sap_uapsd_enabled() - Get if SAP UAPSD is enabled/disabled
 * @psoc: psoc context
 * @value: value to be filled for caller
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_is_sap_uapsd_enabled(struct wlan_objmgr_psoc *psoc,
					  bool *value);

/**
 * wlan_mlme_set_sap_uapsd_flag() - Enable/Disable SAP UAPSD
 * @psoc:  psoc context
 * @value: Enable/Disable control value for sap_uapsd_enabled field
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_mlme_set_sap_uapsd_flag(struct wlan_objmgr_psoc *psoc,
					bool value);
/**
 * wlan_mlme_is_11h_enabled() - Get the 11h flag
 * @psoc: psoc context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_is_11h_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_11h_enabled() - Set the 11h flag
 * @psoc: psoc context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_11h_enabled(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_is_11d_enabled() - Get the 11d flag
 * @psoc: psoc context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_is_11d_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_11d_enabled() - Set the 11h flag
 * @psoc: psoc context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_11d_enabled(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_is_rf_test_mode_enabled() - Get the rf test mode flag
 * @psoc: psoc context
 * @value: Enable/Disable value ptr.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_is_rf_test_mode_enabled(struct wlan_objmgr_psoc *psoc, bool *value);

/**
 * wlan_mlme_set_rf_test_mode_enabled() - Set the rf test mode flag
 * @psoc: psoc context
 * @value: Enable/Disable value.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_rf_test_mode_enabled(struct wlan_objmgr_psoc *psoc, bool value);

/**
 * wlan_mlme_get_sta_miracast_mcc_rest_time() - Get STA/MIRACAST MCC rest time
 *
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * This API gives rest time to be used when STA and MIRACAST MCC conc happens
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_sta_miracast_mcc_rest_time(struct wlan_objmgr_psoc *psoc,
					 uint32_t *value);
/**
 * wlan_mlme_get_sap_mcc_chnl_avoid() - Check if SAP MCC needs to be avoided
 *
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * This API fetches the user setting to determine if SAP MCC with other persona
 * to be avoided.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_sap_mcc_chnl_avoid(struct wlan_objmgr_psoc *psoc,
				 uint8_t *value);
/**
 * wlan_mlme_get_mcc_bcast_prob_resp() - Get broadcast probe rsp in MCC
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
QDF_STATUS
wlan_mlme_get_mcc_bcast_prob_resp(struct wlan_objmgr_psoc *psoc,
				  uint8_t *value);
/**
 * wlan_mlme_get_mcc_rts_cts_prot() - To get RTS-CTS protection in MCC.
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
QDF_STATUS
wlan_mlme_get_mcc_rts_cts_prot(struct wlan_objmgr_psoc *psoc,
			       uint8_t *value);
/**
 * wlan_mlme_get_mcc_feature() - To find out to enable/disable MCC feature
 *
 * @psoc: pointer to psoc object
 * @value: value which needs to filled by API
 *
 * To get INI value which helps to determine whether to enable MCC feature
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_mcc_feature(struct wlan_objmgr_psoc *psoc,
			  uint8_t *value);

/**
 * wlan_mlme_get_rrm_enabled() - Get the RRM enabled ini
 * @psoc: pointer to psoc object
 * @value: pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS wlan_mlme_get_rrm_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * wlan_mlme_get_dtim_selection_diversity() - get dtim selection diversity
 * bitmap
 * @psoc: pointer to psoc object
 * @dtim_selection_div: value that is requested by the caller
 * This function gets the dtim selection diversity bitmap to be
 * sent to the firmware
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_get_dtim_selection_diversity(struct wlan_objmgr_psoc *psoc,
						  uint32_t *dtim_selection_div);

/**
 * wlan_mlme_get_bmps_min_listen_interval() - get beacon mode powersave
 * minimum listen interval value
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_get_bmps_min_listen_interval(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value);

/**
 * wlan_mlme_get_bmps_max_listen_interval() - get beacon mode powersave
 * maximum listen interval value
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_get_bmps_max_listen_interval(struct wlan_objmgr_psoc *psoc,
						  uint32_t *value);

/**
 * wlan_mlme_get_auto_bmps_timer_value() - get bmps timer value
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_get_auto_bmps_timer_value(struct wlan_objmgr_psoc *psoc,
					       uint32_t *value);

/**
 * wlan_mlme_is_bmps_enabled() - check if beacon mode powersave is
 * enabled/disabled
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_is_bmps_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * wlan_mlme_override_bmps_imps() - disable imps/bmps
 * @psoc: pointer to psoc object
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_override_bmps_imps(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_is_imps_enabled() - check if idle mode powersave is
 * enabled/disabled
 * @psoc: pointer to psoc object
 * @value: value that is requested by the caller
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_mlme_is_imps_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *value);

/**
 * wlan_mlme_get_wps_uuid() - get the wps uuid string
 * @wps_params:   pointer to mlme wps parameters structure
 * @data:          data to which the parameter is to be copied
 *
 * Return None
 *
 */
void
wlan_mlme_get_wps_uuid(struct wlan_mlme_wps_params *wps_params, uint8_t *data);

/**
 * wlan_mlme_get_self_gen_frm_pwr() - get self gen frm pwr
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_self_gen_frm_pwr(struct wlan_objmgr_psoc *psoc,
			       uint32_t *value);

/**
 * wlan_mlme_get_4way_hs_offload() - get 4-way hs offload to fw cfg
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_4way_hs_offload(struct wlan_objmgr_psoc *psoc, uint32_t *value);

/**
 * wlan_mlme_get_bmiss_skip_full_scan_value() - To get value of
 * bmiss_skip_full_scan ini
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bmiss_skip_full_scan_value(struct wlan_objmgr_psoc *psoc,
					 bool *value);

/**
 * mlme_get_peer_phymode() - get phymode of peer
 * @psoc: pointer to psoc object
 * @mac:  Pointer to the mac addr of the peer
 * @peer_phymode: phymode
 *
 * Return: QDF Status
 */
QDF_STATUS
mlme_get_peer_phymode(struct wlan_objmgr_psoc *psoc, uint8_t *mac,
		      enum wlan_phymode *peer_phymode);

/**
 * mlme_set_tgt_wpa3_roam_cap() - Set the target WPA3 roam support
 * to mlme
 * @psoc: pointer to PSOC object
 * @akm_bitmap: Bitmap of akm suites supported for roaming by the firmware
 *
 * Return: QDF Status
 */
QDF_STATUS mlme_set_tgt_wpa3_roam_cap(struct wlan_objmgr_psoc *psoc,
				      uint32_t akm_bitmap);
/**
 * wlan_mlme_get_ignore_fw_reg_offload_ind() - Get the
 * ignore_fw_reg_offload_ind ini
 * @psoc: pointer to psoc object
 * @disabled: output pointer to hold user config
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_ignore_fw_reg_offload_ind(struct wlan_objmgr_psoc *psoc,
					bool *disabled);

/**
 * mlme_get_roam_trigger_str() - Get the string for enum
 * WMI_ROAM_TRIGGER_REASON_ID reason.
 * @roam_scan_trigger: roam scan trigger ID
 *
 *  Return: Meaningful string from enum WMI_ROAM_TRIGGER_REASON_ID
 */
char *mlme_get_roam_trigger_str(uint32_t roam_scan_trigger);

/**
 * mlme_get_roam_scan_type_str() - Get the string for roam sacn type
 * @roam_scan_type: roam scan type coming from fw via
 * wmi_roam_scan_info tlv
 *
 *  Return: Meaningful string for roam sacn type
 */
char *mlme_get_roam_scan_type_str(uint32_t roam_scan_type);

/**
 * mlme_get_roam_status_str() - Get the string for roam status
 * @roam_status: roam status coming from fw via
 * wmi_roam_scan_info tlv
 *
 *  Return: Meaningful string for roam status
 */
char *mlme_get_roam_status_str(uint32_t roam_status);

/**
 * mlme_get_converted_timestamp() - Return time of the day
 * from timestamp
 * @timestamp:    Timestamp value in milliseconds
 * @time:         Output buffer to fill time into
 *
 * Return: Time of the day in [HH:MM:SS.uS]
 */
void mlme_get_converted_timestamp(uint32_t timestamp, char *time);

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * wlan_mlme_set_sae_single_pmk_bss_cap - API to set WPA3 single pmk AP IE
 * @psoc: Pointer to psoc object
 * @vdev_id: vdev id
 * @val: value to be set
 *
 * Return : None
 */
void wlan_mlme_set_sae_single_pmk_bss_cap(struct wlan_objmgr_psoc *psoc,
					  uint8_t vdev_id, bool val);

/**
 * wlan_mlme_update_sae_single_pmk - API to update mlme_pmkid_info
 * @vdev: vdev object
 * @sae_single_pmk: pointer to sae_single_pmk_info struct
 *
 * Return : None
 */
void
wlan_mlme_update_sae_single_pmk(struct wlan_objmgr_vdev *vdev,
				struct mlme_pmk_info *sae_single_pmk);

/**
 * wlan_mlme_get_sae_single_pmk_info - API to get mlme_pmkid_info
 * @vdev: vdev object
 * @pmksa: pointer to PMKSA struct
 *
 * Return : None
 */
void
wlan_mlme_get_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
				  struct wlan_mlme_sae_single_pmk *pmksa);
/**
 * wlan_mlme_clear_sae_single_pmk_info - API to clear mlme_pmkid_info ap caps
 * @vdev: vdev object
 * @pmk : pmk info to clear
 *
 * Return : None
 */
void wlan_mlme_clear_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
					 struct mlme_pmk_info *pmk);
#else
static inline void
wlan_mlme_set_sae_single_pmk_bss_cap(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id, bool val)
{
}

static inline void
wlan_mlme_update_sae_single_pmk(struct wlan_objmgr_vdev *vdev,
				struct mlme_pmk_info *sae_single_pmk)
{
}

static inline void
wlan_mlme_get_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
				  struct wlan_mlme_sae_single_pmk *pmksa)
{
}

static inline
void wlan_mlme_clear_sae_single_pmk_info(struct wlan_objmgr_vdev *vdev,
					 struct mlme_pmk_info *pmk)
{
}
#endif

/**
 * mlme_get_roam_fail_reason_str() - Get fail string from enum
 * WMI_ROAM_FAIL_REASON_ID
 * @result:   Roam fail reason
 *
 * Return: Meaningful string from enum
 */
char *mlme_get_roam_fail_reason_str(uint32_t result);

/**
 * mlme_get_sub_reason_str() - Get roam trigger sub reason from enum
 * WMI_ROAM_TRIGGER_SUB_REASON_ID
 * @sub_reason: Sub reason value
 *
 * Return: Meaningful string from enum WMI_ROAM_TRIGGER_SUB_REASON_ID
 */
char *mlme_get_sub_reason_str(uint32_t sub_reason);

/**
 * wlan_mlme_get_mgmt_max_retry() - Get the
 * max mgmt retry
 * @psoc: pointer to psoc object
 * @max_retry: output pointer to hold user config
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mgmt_max_retry(struct wlan_objmgr_psoc *psoc,
			     uint8_t *max_retry);

/**
 * wlan_mlme_get_status_ring_buffer() - Get the
 * status of ring buffer
 * @psoc: pointer to psoc object
 * @enable_ring_buffer: output pointer to point the configured value of
 * ring buffer
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_status_ring_buffer(struct wlan_objmgr_psoc *psoc,
				 bool *enable_ring_buffer);

/**
 * wlan_mlme_get_peer_unmap_conf() - Indicate if peer unmap confirmation
 * support is enabled or disabled
 * @psoc: pointer to psoc object
 *
 * Return: true if peer unmap confirmation support is enabled, else false
 */
bool wlan_mlme_get_peer_unmap_conf(struct wlan_objmgr_psoc *psoc);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wlan_mlme_get_roam_reason_vsie_status() - Indicate if roam reason
 * vsie is enabled or disabled
 * @psoc: pointer to psoc object
 * @roam_reason_vsie_enabled: pointer to hold value of roam reason
 * vsie
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t *roam_reason_vsie_enabled);

/**
 * wlan_mlme_set_roam_reason_vsie_status() - Update roam reason vsie status
 * @psoc: pointer to psoc object
 * @roam_reason_vsie_enabled: value of roam reason vsie
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t roam_reason_vsie_enabled);

/**
 * wlan_mlme_get_roaming_triggers  - Get the roaming triggers bitmap
 * @psoc: Pointer to PSOC object
 *
 * Return: Roaming triggers value
 */
uint32_t wlan_mlme_get_roaming_triggers(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_get_roaming_offload() - Get roaming offload setting
 * @psoc: pointer to psoc object
 * @val:  Pointer to enable/disable roaming offload
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_roaming_offload(struct wlan_objmgr_psoc *psoc,
			      bool *val);

/**
 * wlan_mlme_get_enable_disconnect_roam_offload() - Get emergency roaming
 * Enable/Disable status during deauth/disassoc
 * @psoc: pointer to psoc object
 * @val:  Pointer to emergency roaming Enable/Disable status
 *        during deauth/disassoc
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_enable_disconnect_roam_offload(struct wlan_objmgr_psoc *psoc,
					     bool *val);

/**
 * wlan_mlme_get_enable_idle_roam() - Get Enable/Disable idle roaming status
 * @psoc: pointer to psoc object
 * @val:  Pointer to Enable/Disable idle roaming status
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_enable_idle_roam(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_mlme_get_idle_roam_rssi_delta() - Get idle roam rssi delta
 * @psoc: pointer to psoc object
 * @val:  Pointer to idle roam rssi delta
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_idle_roam_rssi_delta(struct wlan_objmgr_psoc *psoc,
				   uint32_t *val);

/**
 * wlan_mlme_get_idle_roam_inactive_time() - Get idle roam inactive time
 * @psoc: pointer to psoc object
 * @val:  Pointer to idle roam inactive time
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_idle_roam_inactive_time(struct wlan_objmgr_psoc *psoc,
				      uint32_t *val);
/**
 * wlan_mlme_get_idle_data_packet_count() - Get idle data packet count
 * @psoc: pointer to psoc object
 * @val:  Pointer to idle data packet count
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_idle_data_packet_count(struct wlan_objmgr_psoc *psoc,
				     uint32_t *val);

/**
 * wlan_mlme_get_idle_roam_min_rssi() - Get idle roam min rssi
 * @psoc: pointer to psoc object
 * @val:  Pointer to idle roam min rssi
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_idle_roam_min_rssi(struct wlan_objmgr_psoc *psoc, uint32_t *val);

/**
 * wlan_mlme_get_idle_roam_band() - Get idle roam band
 * @psoc: pointer to psoc object
 * @val:  Pointer to idle roam band
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_idle_roam_band(struct wlan_objmgr_psoc *psoc, uint32_t *val);
#else
static inline QDF_STATUS
wlan_mlme_get_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t *roam_reason_vsie_enable)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
wlan_mlme_set_roam_reason_vsie_status(struct wlan_objmgr_psoc *psoc,
				      uint8_t roam_reason_vsie_enable)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
uint32_t wlan_mlme_get_roaming_triggers(struct wlan_objmgr_psoc *psoc)
{
	return 0xFFFF;
}

static inline QDF_STATUS
wlan_mlme_get_roaming_offload(struct wlan_objmgr_psoc *psoc,
			      bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wlan_mlme_get_dfs_chan_ageout_time() - Get the DFS Channel ageout time
 * @psoc: pointer to psoc object
 * @dfs_chan_ageout_time: output pointer to hold configured value of DFS
 * Channel ageout time
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_dfs_chan_ageout_time(struct wlan_objmgr_psoc *psoc,
				   uint8_t *dfs_chan_ageout_time);

#ifdef WLAN_FEATURE_SAE
/**
 * wlan_mlme_get_sae_assoc_retry_count() - Get the sae assoc retry count
 * @psoc: pointer to psoc object
 * @retry_count: assoc retry count
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_sae_assoc_retry_count(struct wlan_objmgr_psoc *psoc,
				    uint8_t *retry_count);
/**
 * wlan_mlme_get_sae_assoc_retry_count() - Get the sae auth retry count
 * @psoc: pointer to psoc object
 * @retry_count: auth retry count
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_sae_auth_retry_count(struct wlan_objmgr_psoc *psoc,
				   uint8_t *retry_count);

/**
 * wlan_mlme_get_sae_roam_auth_retry_count() - Get the sae roam auth retry count
 * @psoc: pointer to psoc object
 * @retry_count: auth retry count
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_sae_roam_auth_retry_count(struct wlan_objmgr_psoc *psoc,
					uint8_t *retry_count);

#else
static inline QDF_STATUS
wlan_mlme_get_sae_assoc_retry_count(struct wlan_objmgr_psoc *psoc,
				    uint8_t *retry_count)
{
	*retry_count = 0;
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_get_sae_auth_retry_count(struct wlan_objmgr_psoc *psoc,
				    uint8_t *retry_count)
{
	*retry_count = 0;
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_mlme_get_sae_roam_auth_retry_count(struct wlan_objmgr_psoc *psoc,
					uint8_t *retry_count)
{
	*retry_count = 0;
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wlan_mlme_get_dual_sta_roaming_enabled  - API to get if the dual sta
 * roaming support is enabled.
 * @psoc: Pointer to global psoc object
 *
 * Return: True if dual sta roaming feature is enabled else return false
 */
bool
wlan_mlme_get_dual_sta_roaming_enabled(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
wlan_mlme_get_dual_sta_roaming_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * mlme_store_fw_scan_channels - Update the valid channel list to mlme.
 * @psoc: Pointer to global psoc object
 * @chan_list: Source channel list pointer
 *
 * Currently the channel list is saved to wma_handle to be updated in the
 * PCL command. This cannot be accesed at target_if while sending vdev
 * set pcl command. So save the channel list to mlme.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlme_store_fw_scan_channels(struct wlan_objmgr_psoc *psoc,
			    tSirUpdateChanList *chan_list);

/**
 * mlme_get_fw_scan_channels  - Copy the saved valid channel
 * list to the provided buffer
 * @psoc: Pointer to global psoc object
 * @freq_list: Pointer to the frequency list buffer to be filled
 * @saved_num_chan: Number of channels filled
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_get_fw_scan_channels(struct wlan_objmgr_psoc *psoc,
				     uint32_t *freq_list,
				     uint8_t *saved_num_chan);
/**
 * wlan_mlme_get_roam_scan_offload_enabled() - Roam scan offload enable or not
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_roam_scan_offload_enabled(struct wlan_objmgr_psoc *psoc,
					bool *val);

/**
 * wlan_mlme_get_roam_bmiss_final_bcnt() - Get roam bmiss final count
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_roam_bmiss_final_bcnt(struct wlan_objmgr_psoc *psoc,
				    uint8_t *val);

/**
 * wlan_mlme_get_roam_bmiss_first_bcnt() - Get roam bmiss first count
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_roam_bmiss_first_bcnt(struct wlan_objmgr_psoc *psoc,
				    uint8_t *val);

/**
 * wlan_mlme_adaptive_11r_enabled() - check if adaptive 11r feature is enaled
 * or not
 * @psoc: pointer to psoc object
 *
 * Return: bool
 */
#ifdef WLAN_ADAPTIVE_11R
bool wlan_mlme_adaptive_11r_enabled(struct wlan_objmgr_psoc *psoc);
#else
static inline bool wlan_mlme_adaptive_11r_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

/**
 * wlan_mlme_get_mawc_enabled() - Get mawc enabled status
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_enabled(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_mlme_get_mawc_roam_enabled() - Get mawc roam enabled status
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_roam_enabled(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_mlme_get_mawc_roam_traffic_threshold() - Get mawc traffic threshold
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_roam_traffic_threshold(struct wlan_objmgr_psoc *psoc,
					  uint32_t *val);

/**
 * wlan_mlme_get_mawc_roam_ap_rssi_threshold() - Get AP RSSI threshold for
 * MAWC roaming
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_roam_ap_rssi_threshold(struct wlan_objmgr_psoc *psoc,
					  uint32_t *val);

/**
 * wlan_mlme_get_mawc_roam_rssi_high_adjust() - Get high adjustment value
 * for suppressing scan
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_roam_rssi_high_adjust(struct wlan_objmgr_psoc *psoc,
					 uint8_t *val);

/**
 * wlan_mlme_get_mawc_roam_rssi_low_adjust() - Get low adjustment value
 * for suppressing scan
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_mawc_roam_rssi_low_adjust(struct wlan_objmgr_psoc *psoc,
					uint8_t *val);

/**
 * wlan_mlme_get_bss_load_enabled() - Get bss load based roam trigger
 * enabled status
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_enabled(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * wlan_mlme_get_bss_load_threshold() - Get bss load threshold
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_threshold(struct wlan_objmgr_psoc *psoc, uint32_t *val);

/**
 * wlan_mlme_get_bss_load_sample_time() - Get bss load sample time
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_sample_time(struct wlan_objmgr_psoc *psoc,
				   uint32_t *val);

/**
 * wlan_mlme_get_bss_load_rssi_threshold_5ghz() - Get bss load RSSI
 * threshold on 5G
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_rssi_threshold_5ghz(struct wlan_objmgr_psoc *psoc,
					   int32_t *val);

/**
 * wlan_mlme_get_bss_load_rssi_threshold_24ghz() - Get bss load RSSI
 * threshold on 2.4G
 * @psoc: pointer to psoc object
 * @val:  Pointer to the value which will be filled for the caller
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_bss_load_rssi_threshold_24ghz(struct wlan_objmgr_psoc *psoc,
					    int32_t *val);
/**
 * wlan_mlme_check_chan_param_has_dfs() - Get dfs flag based on
 * channel & channel parameters
 * @pdev: pdev object
 * @ch_params: channel parameters
 * @chan_freq: channel frequency in MHz
 *
 * Return: True for dfs
 */
bool
wlan_mlme_check_chan_param_has_dfs(struct wlan_objmgr_pdev *pdev,
				   struct ch_params *ch_params,
				   uint32_t chan_freq);

/**
 * wlan_mlme_set_usr_disabled_roaming() - Set user config for roaming disable
 * @psoc: pointer to psoc object
 * @val: user config for roaming disable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_set_usr_disabled_roaming(struct wlan_objmgr_psoc *psoc, bool val);

/**
 * wlan_mlme_get_usr_disabled_roaming() - Get user config for roaming disable
 * @psoc: pointer to psoc object
 * @val: user config for roaming disable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_mlme_get_usr_disabled_roaming(struct wlan_objmgr_psoc *psoc, bool *val);

/**
 * mlme_get_opr_rate() - get operational rate
 * @vdev: vdev pointer
 * @dst: pointer to get operational rate
 * @len: length of operational rate
 *
 * Return: QDF_SUCCESS if success
 */
QDF_STATUS mlme_get_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *dst,
			     qdf_size_t *len);

/**
 * mlme_set_opr_rate() - set operational rate
 * @vdev: vdev pointer
 * @src: pointer to set operational rate
 * @len: length of operational rate
 *
 * Return: QDF_SUCCESS if success
 */
QDF_STATUS mlme_set_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *src,
			     qdf_size_t len);

/**
 * mlme_get_ext_opr_rate() - get extended operational rate
 * @vdev: vdev pointer
 * @dst: pointer to get extended operational rate
 * @len: length of extended operational rate
 *
 * Return: QDF_SUCCESS if success
 */
QDF_STATUS mlme_get_ext_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *dst,
				 qdf_size_t *len);

/**
 * mlme_set_ext_opr_rate() - set extended operational rate
 * @vdev: vdev pointer
 * @src: pointer to set extended operational rate
 * @len: length of extended operational rate
 *
 * Return: QDF_SUCCESS if success
 */
QDF_STATUS mlme_set_ext_opr_rate(struct wlan_objmgr_vdev *vdev, uint8_t *src,
				 qdf_size_t len);

/**
 * wlan_mlme_is_sta_mon_conc_supported() - Check if STA + Monitor mode
 * concurrency is supported
 * @psoc: pointer to psoc object
 *
 * Return: True if supported
 */
bool wlan_mlme_is_sta_mon_conc_supported(struct wlan_objmgr_psoc *psoc);

#ifdef WLAN_SUPPORT_TWT
/**
 * mlme_is_twt_enabled() - Get if TWT is enabled via ini.
 * @psoc: pointer to psoc object
 * @val: pointer to the value to be filled
 *
 * Return: True if TWT is enabled else false.
 */
bool
mlme_is_twt_enabled(struct wlan_objmgr_psoc *psoc);
#else
static inline bool
mlme_is_twt_enabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif /* WLAN_SUPPORT_TWT */

/**
 * wlan_mlme_is_local_tpe_pref() - Get preference to use local TPE or
 * regulatory TPE values
 * @psoc: pointer to psoc object
 *
 * Return: True if there is local preference, false if there is regulatory
 * preference
 */
bool wlan_mlme_is_local_tpe_pref(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_skip_tpe() - Get preference to not consider TPE in 2G/5G case
 *
 * @psoc: pointer to psoc object
 *
 * Return: True if host should not consider TPE IE in TX power calculation when
 * operating in 2G/5G bands, false if host should always consider TPE IE values
 */
bool wlan_mlme_skip_tpe(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_mlme_set_ba_2k_jump_iot_ap() - Set a flag if ba 2k jump IOT AP is found
 * @vdev: vdev pointer
 * @found: Carries the value true if ba 2k jump IOT AP is found
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_set_ba_2k_jump_iot_ap(struct wlan_objmgr_vdev *vdev, bool found);

/**
 * wlan_mlme_is_ba_2k_jump_iot_ap() - Check if ba 2k jump IOT AP is found
 * @vdev: vdev pointer
 *
 * Return: true if ba 2k jump IOT AP is found
 */
bool
wlan_mlme_is_ba_2k_jump_iot_ap(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlme_set_bad_htc_he_iot_ap() - Set a flag if bad htc he IOT AP is found
 * @vdev: vdev pointer
 * @found: Carries the value true if bad htc he AP is found
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_set_bad_htc_he_iot_ap(struct wlan_objmgr_vdev *vdev, bool found);

/**
 * wlan_mlme_is_bad_htc_he_iot_ap() - Check if bad htc he IOT AP is found
 * @vdev: vdev pointer
 *
 * Return: true if bad htc he IOT AP is found
 */
bool
wlan_mlme_is_bad_htc_he_iot_ap(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_mlme_set_last_delba_sent_time() - Cache the last delba sent ts
 * @vdev: vdev pointer
 * @delba_sent_time: Last delba sent timestamp
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_set_last_delba_sent_time(struct wlan_objmgr_vdev *vdev,
				   qdf_time_t delba_sent_time);

/**
 * wlan_mlme_get_last_delba_sent_time() - Get the last delba sent ts
 * @vdev: vdev pointer
 *
 * Return: Last delba timestamp if cached, 0 otherwise
 */
qdf_time_t
wlan_mlme_get_last_delba_sent_time(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_user_ps() - Set the PS user config
 * @vdev: Vdev object pointer
 * @ps_enable: User PS enable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_set_user_ps(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			    bool ps_enable);

/**
 * mlme_get_user_ps() - Set the user ps flag
 * @psoc: Pointer to psoc object
 * @vdev_id: vdev id
 *
 * Return: True if user_ps flag is set
 */
bool mlme_get_user_ps(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id);

/**
 * wlan_mlme_get_mgmt_hw_tx_retry_count() - Get mgmt frame hw tx retry count
 *
 * @psoc: pointer to psoc object
 * @frm_type: frame type of the query
 *
 * Return: hw tx retry count
 */
uint8_t
wlan_mlme_get_mgmt_hw_tx_retry_count(struct wlan_objmgr_psoc *psoc,
				     enum mlme_cfg_frame_type frm_type);

/**
 * wlan_mlme_get_tx_retry_multiplier() - Get the tx retry multiplier percentage
 *
 * @psoc: pointer to psoc object
 * @tx_retry_multiplier: pointer to hold user config value of
 * tx_retry_multiplier
 *
 * Return: QDF Status
 */
QDF_STATUS
wlan_mlme_get_tx_retry_multiplier(struct wlan_objmgr_psoc *psoc,
				  uint32_t *tx_retry_multiplier);

#endif /* _WLAN_MLME_API_H_ */
