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
 * DOC: declare internal API related to the fwol component
 */

#ifndef _WLAN_FWOL_UCFG_API_H_
#define _WLAN_FWOL_UCFG_API_H_

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_cmn.h>
#include "wlan_fw_offload_main.h"
#include "wlan_fwol_public_structs.h"

#ifdef WLAN_FW_OFFLOAD
/**
 * ucfg_fwol_psoc_open() - FWOL component Open
 * @psoc: pointer to psoc object
 *
 * Open the FWOL component and initialize the FWOL structure
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_psoc_close() - FWOL component close
 * @psoc: pointer to psoc object
 *
 * Close the FWOL component and clear the FWOL structures
 *
 * Return: None
 */
void ucfg_fwol_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_psoc_enable() - FWOL component enable
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
void ucfg_fwol_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_psoc_disable() - FWOL component disable
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
void ucfg_fwol_psoc_disable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_init() - initialize fwol_ctx context.
 *
 * This function initializes the fwol context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_fwol_init(void);

/**
 * ucfg_fwol_deinit() - De initialize fwol_ctx context.
 *
 * This function De initializes fwol contex.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
void ucfg_fwol_deinit(void);

/**
 * ucfg_fwol_get_coex_config_params() - Get coex config params
 * @psoc: Pointer to psoc object
 * @coex_config: Pointer to struct wlan_fwol_coex_config
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_fwol_get_coex_config_params(struct wlan_objmgr_psoc *psoc,
				 struct wlan_fwol_coex_config *coex_config);

/**
 * ucfg_fwol_get_thermal_temp() - Get thermal temperature config params
 * @psoc: Pointer to psoc object
 * @thermal_temp: Pointer to struct wlan_fwol_thermal_temp
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_thermal_temp(struct wlan_objmgr_psoc *psoc,
			   struct wlan_fwol_thermal_temp *thermal_temp);

/**
 * ucfg_fwol_get_neighbor_report_cfg() - Get neighbor report config params
 * @psoc: Pointer to psoc object
 * @fwol_neighbor_report_cfg: Pointer to return neighbor report config
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_neighbor_report_cfg(struct wlan_objmgr_psoc *psoc,
				  struct wlan_fwol_neighbor_report_cfg
				  *fwol_neighbor_report_cfg);

/**
 * ucfg_fwol_get_neighbor_report_req() - Get neighbor report request bit
 * @psoc: Pointer to psoc object
 * @neighbor_report_req: Pointer to return value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_is_neighbor_report_req_supported(struct wlan_objmgr_psoc *psoc,
					   bool *neighbor_report_req);

/**
 * ucfg_fwol_get_ie_whitelist() - Get IE whitelist param value
 * @psoc: Pointer to psoc object
 * @ie_whitelist: Pointer to return the IE whitelist param value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_ie_whitelist(struct wlan_objmgr_psoc *psoc, bool *ie_whitelist);

/**
 * ucfg_fwol_set_ie_whitelist() - Set IE whitelist param value
 * @psoc: Pointer to psoc object
 * @ie_whitelist: Value to set IE whitelist param
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_set_ie_whitelist(struct wlan_objmgr_psoc *psoc, bool ie_whitelist);

/**
 * ucfg_fwol_get_all_whitelist_params() - Get all IE whitelist param values
 * @psoc: Pointer to psoc object
 * @whitelist: Pointer to struct wlan_fwol_ie_whitelist
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_all_whitelist_params(struct wlan_objmgr_psoc *psoc,
				   struct wlan_fwol_ie_whitelist *whitelist);

/** ucfg_fwol_get_ani_enabled() - Assigns the ani_enabled value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_ani_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *ani_enabled);

/**
 * ucfg_fwol_get_ani_enabled() - Assigns the enable_rts_sifsbursting value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_enable_rts_sifsbursting(struct wlan_objmgr_psoc *psoc,
					    bool *enable_rts_sifsbursting);

/**
 * ucfg_get_enable_sifs_burst() - Get the enable_sifs_burst value
 * @psoc: pointer to the psoc object
 * @enable_sifs_burst: pointer to return enable_sifs_burst value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_enable_sifs_burst(struct wlan_objmgr_psoc *psoc,
				      uint8_t *enable_sifs_burst);

/**
 * ucfg_get_max_mpdus_inampdu() - Assigns the max_mpdus_inampdu value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_max_mpdus_inampdu(struct wlan_objmgr_psoc *psoc,
				      uint8_t *max_mpdus_inampdu);

/**
 * ucfg_get_enable_phy_reg_retention() - Assigns enable_phy_reg_retention value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_enable_phy_reg_retention(struct wlan_objmgr_psoc *psoc,
					     uint8_t *enable_phy_reg_retention);

/**
 * ucfg_get_upper_brssi_thresh() - Assigns upper_brssi_thresh value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_upper_brssi_thresh(struct wlan_objmgr_psoc *psoc,
				       uint16_t *upper_brssi_thresh);

/**
 * ucfg_get_lower_brssi_thresh() - Assigns lower_brssi_thresh value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_lower_brssi_thresh(struct wlan_objmgr_psoc *psoc,
				       uint16_t *lower_brssi_thresh);

/**
 * ucfg_get_enable_dtim_1chrx() - Assigns enable_dtim_1chrx value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_enable_dtim_1chrx(struct wlan_objmgr_psoc *psoc,
				      bool *enable_dtim_1chrx);

/**
 * ucfg_get_alternate_chainmask_enabled() - Assigns alt chainmask_enabled value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_get_alternative_chainmask_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *alternative_chainmask_enabled);

/**
 * ucfg_get_smart_chainmask_enabled() - Assigns smart_chainmask_enabled value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_get_smart_chainmask_enabled(struct wlan_objmgr_psoc *psoc,
					    bool *smart_chainmask_enabled);

/**
 * ucfg_fwol_get_rts_profile() - Assigns get_rts_profile value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_rts_profile(struct wlan_objmgr_psoc *psoc,
				     uint16_t *get_rts_profile);

/**
 * ucfg_fwol_get_enable_fw_log_level() - Assigns enable_fw_log_level value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_enable_fw_log_level(struct wlan_objmgr_psoc *psoc,
					     uint16_t *enable_fw_log_level);

/**
 * ucfg_fwol_get_enable_fw_log_type() - Assigns enable_fw_log_type value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_enable_fw_log_type(struct wlan_objmgr_psoc *psoc,
					    uint16_t *enable_fw_log_type);
/**
 * ucfg_fwol_get_enable_fw_module_log_level() - Assigns
 * enable_fw_module_log_level string
 * @psoc: pointer to the psoc object
 * @enable_fw_module_log_level:
 * pointer to enable_fw_module_log_level array
 * @enable_fw_module_log_level_num:
 * pointer to enable_fw_module_log_level array element num
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_enable_fw_module_log_level(
				struct wlan_objmgr_psoc *psoc,
				uint8_t **enable_fw_module_log_level,
				uint8_t *enable_fw_module_log_level_num);

/**
 * ucfg_fwol_wow_get_enable_fw_module_log_level() - Assigns
 * enable_fw_module_log_level string
 *
 * @psoc: pointer to the psoc object
 * @enable_fw_wow_module_log_level:
 * pointer to enable_fw_wow_module_log_level array
 * @enable_fw_wow_module_log_level_num:
 * pointer to enable_fw_wow_module_log_level array element num
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_wow_get_enable_fw_module_log_level(
				struct wlan_objmgr_psoc *psoc,
				uint8_t **enable_fw_wow_module_log_level,
				uint8_t *enable_fw_wow_module_log_level_num);

/**
 * ucfg_fwol_get_sap_xlna_bypass() - Assigns sap_xlna_bypass value
 * @psoc: pointer to the psoc object
 * @sap_xlna_bypass: pointer to return sap_xlna_bypass bool
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_sap_xlna_bypass(struct wlan_objmgr_psoc *psoc,
					 bool *sap_xlna_bypass);

#ifdef FEATURE_WLAN_RA_FILTERING
/**
 * ucfg_fwol_set_is_rate_limit_enabled() - Sets the is_rate_limit_enabled value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_set_is_rate_limit_enabled(struct wlan_objmgr_psoc *psoc,
					       bool is_rate_limit_enabled);

/**
 * ucfg_fwol_get_is_rate_limit_enabled() - Assigns is_rate_limit_enabled value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_is_rate_limit_enabled(struct wlan_objmgr_psoc *psoc,
					       bool *is_rate_limit_enabled);

#endif /* FEATURE_WLAN_RA_FILTERING */

/**
 * ucfg_fwol_get_tsf_gpio_pin() - Assigns tsf_gpio_pin value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */

QDF_STATUS ucfg_fwol_get_tsf_gpio_pin(struct wlan_objmgr_psoc *psoc,
				      uint32_t *tsf_gpio_pin);

#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_IRQ
/**
 * ucfg_fwol_get_tsf_irq_host_gpio_pin() - Assigns tsf_irq_host_gpio_pin value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */

QDF_STATUS
ucfg_fwol_get_tsf_irq_host_gpio_pin(struct wlan_objmgr_psoc *psoc,
				    uint32_t *tsf_irq_host_gpio_pin);
#endif

#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC
/**
 * ucfg_fwol_get_tsf_sync_host_gpio_pin() - Assigns tsf_sync_host_gpio_pin value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */

QDF_STATUS
ucfg_fwol_get_tsf_sync_host_gpio_pin(struct wlan_objmgr_psoc *psoc,
				     uint32_t *tsf_irq_host_gpio_pin);
#endif

#ifdef DHCP_SERVER_OFFLOAD
/**
 * ucfg_fwol_get_enable_dhcp_server_offload()-Assign enable_dhcp_server_offload
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_enable_dhcp_server_offload(struct wlan_objmgr_psoc *psoc,
					 bool *enable_dhcp_server_offload);

/**
 * ucfg_fwol_get_dhcp_max_num_clients() - Assigns dhcp_max_num_clients value
 * @psoc: pointer to the psoc object
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_dhcp_max_num_clients(struct wlan_objmgr_psoc *psoc,
					      uint32_t *dhcp_max_num_clients);
#endif

/**
 * ucfg_fwol_get_tsf_ptp_options() - Get TSF Plus feature options
 * @psoc: pointer to the psoc object
 * @tsf_ptp_options: Pointer to return tsf ptp options
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_tsf_ptp_options(struct wlan_objmgr_psoc *psoc,
					 uint32_t *tsf_ptp_options);
/**
 * ucfg_fwol_get_lprx_enable() - Get LPRx feature enable status
 * @psoc: pointer to the psoc object
 * @lprx_enable: Pointer to return LPRX feature enable status
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_lprx_enable(struct wlan_objmgr_psoc *psoc,
				     bool *lprx_enable);

/**
 * ucfg_fwol_get_sae_enable() - Get SAE feature enable status
 * @psoc: pointer to the psoc object
 *
 * Return: True if enabled else false
 */
bool ucfg_fwol_get_sae_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_get_gcmp_enable() - Get GCMP feature enable status
 * @psoc: pointer to the psoc object
 *
 * Return: True if enabled else false
 */
bool ucfg_fwol_get_gcmp_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_fwol_get_enable_tx_sch_delay() - Get enable tx sch delay
 * @psoc: pointer to the psoc object
 * @enable_tx_sch_delay: Pointer to return enable_tx_sch_delay value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_enable_tx_sch_delay(struct wlan_objmgr_psoc *psoc,
					     uint8_t *enable_tx_sch_delay);

/**
 * ucfg_fwol_get_enable_secondary_rate() - Get enable secondary rate
 * @psoc: pointer to the psoc object
 * @enable_tx_sch_delay: Pointer to return enable secondary rate value
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_enable_secondary_rate(struct wlan_objmgr_psoc *psoc,
					       uint32_t *enable_secondary_rate);
/**
 * ucfg_fwol_get_all_adaptive_dwelltime_params() - Get all adaptive
						   dwelltime_params
 * @psoc: Pointer to psoc object
 * @dwelltime_params: Pointer to struct adaptive_dwelltime_params
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_all_adaptive_dwelltime_params(
			struct wlan_objmgr_psoc *psoc,
			struct adaptive_dwelltime_params *dwelltime_params);
/**
 * ucfg_fwol_get_adaptive_dwell_mode_enabled() - API to globally disable/enable
 *                                               the adaptive dwell config.
 * Acceptable values for this:
 * 0: Config is disabled
 * 1: Config is enabled
 *
 * @psoc: pointer to psoc object
 * @adaptive_dwell_mode_enabled: adaptive dwell mode enable/disable
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_adaptive_dwell_mode_enabled(struct wlan_objmgr_psoc *psoc,
					  bool *adaptive_dwell_mode_enabled);

/**
 * ucfg_fwol_get_global_adapt_dwelltime_mode() - API to set default
 *                                               adaptive mode.
 * It will be used if any of the scan dwell mode is set to default.
 * For uses : see enum scan_dwelltime_adaptive_mode
 *
 * @psoc: pointer to psoc object
 * global_adapt_dwelltime_mode@: global adaptive dwell mode value
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_global_adapt_dwelltime_mode(struct wlan_objmgr_psoc *psoc,
					  uint8_t *global_adapt_dwelltime_mode);
/**
 * ucfg_fwol_get_adapt_dwell_lpf_weight() - API to get weight to calculate
 *                       the average low pass filter for channel congestion
 * @psoc: pointer to psoc object
 * @adapt_dwell_lpf_weight: adaptive low pass filter weight
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_adapt_dwell_lpf_weight(struct wlan_objmgr_psoc *psoc,
				     uint8_t *adapt_dwell_lpf_weight);

/**
 * ucfg_fwol_get_adapt_dwell_passive_mon_intval() - API to get interval value
 *                      for montitoring wifi activity in passive scan in msec.
 * @psoc: pointer to psoc object
 * @adapt_dwell_passive_mon_intval: adaptive monitor interval in passive scan
 *
 * Return: QDF Status
 */
QDF_STATUS
ucfg_fwol_get_adapt_dwell_passive_mon_intval(
				struct wlan_objmgr_psoc *psoc,
				uint8_t *adapt_dwell_passive_mon_intval);

/**
 * ucfg_fwol_get_adapt_dwell_wifi_act_threshold - API to get % of wifi activity
 *                                                used in passive scan
 * @psoc: pointer to psoc object
 * @adapt_dwell_wifi_act_threshold: percent of wifi activity in passive scan
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_get_adapt_dwell_wifi_act_threshold(
				struct wlan_objmgr_psoc *psoc,
				uint8_t *adapt_dwell_wifi_act_threshold);

/**
 * ucfg_fwol_init_adapt_dwelltime_in_cfg - API to initialize adaptive
 *                                         dwell params
 * @psoc: pointer to psoc object
 * @adaptive_dwelltime_params: pointer to adaptive_dwelltime_params structure
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_fwol_init_adapt_dwelltime_in_cfg(
			struct wlan_objmgr_psoc *psoc,
			struct adaptive_dwelltime_params *dwelltime_params)
{
	return fwol_init_adapt_dwelltime_in_cfg(psoc, dwelltime_params);
}

/**
 * ucfg_fwol_set_adaptive_dwelltime_config - API to set adaptive
 *                                           dwell params config
 * @adaptive_dwelltime_params: adaptive_dwelltime_params structure
 *
 * Return: QDF Status
 */
static inline QDF_STATUS
ucfg_fwol_set_adaptive_dwelltime_config(
			struct adaptive_dwelltime_params *dwelltime_params)
{
	return fwol_set_adaptive_dwelltime_config(dwelltime_params);
}

#ifdef WLAN_FEATURE_ELNA
/**
 * ucfg_fwol_set_elna_bypass() - send set eLNA bypass request
 * @vdev: vdev handle
 * @req: set eLNA bypass request
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_fwol_set_elna_bypass(struct wlan_objmgr_vdev *vdev,
				     struct set_elna_bypass_request *req);

/**
 * ucfg_fwol_get_elna_bypass() - send get eLNA bypass request
 * @vdev: vdev handle
 * @req: get eLNA bypass request
 * @callback: get eLNA bypass response callback
 * @context: request manager context
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_fwol_get_elna_bypass(struct wlan_objmgr_vdev *vdev,
				     struct get_elna_bypass_request *req,
				     void (*callback)(void *context,
				     struct get_elna_bypass_response *response),
				     void *context);
#endif /* WLAN_FEATURE_ELNA */

#ifdef WLAN_SEND_DSCP_UP_MAP_TO_FW
/**
 * ucfg_fwol_send_dscp_up_map_to_fw() - send dscp_up map to FW
 * @vdev: vdev handle
 * @dscp_to_up_map: DSCP to UP map array
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_fwol_send_dscp_up_map_to_fw(
		struct wlan_objmgr_vdev *vdev,
		uint32_t *dscp_to_up_map);
#else
static inline
QDF_STATUS ucfg_fwol_send_dscp_up_map_to_fw(
		struct wlan_objmgr_vdev *vdev,
		uint32_t *dscp_to_up_map)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * ucfg_fwol_update_fw_cap_info - API to update fwol capability info
 * @psoc: pointer to psoc object
 * @caps: pointer to wlan_fwol_capability_info struct
 *
 * Used to update fwol capability info.
 *
 * Return: void
 */
void ucfg_fwol_update_fw_cap_info(struct wlan_objmgr_psoc *psoc,
				  struct wlan_fwol_capability_info *caps);

#ifdef THERMAL_STATS_SUPPORT
QDF_STATUS ucfg_fwol_send_get_thermal_stats_cmd(struct wlan_objmgr_psoc *psoc,
				      enum thermal_stats_request_type req_type,
				      void (*callback)(void *context,
				      struct thermal_throttle_info *response),
				      void *context);
#endif /* THERMAL_STATS_SUPPORT */

/**
 * ucfg_fwol_configure_global_params - API to configure global params
 * @psoc: pointer to psoc object
 * @pdev: pointer to pdev object
 *
 * Used to configure global firmware params. This is invoked from hdd during
 * bootup.
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_configure_global_params(struct wlan_objmgr_psoc *psoc,
					     struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_fwol_configure_vdev_params - API to configure vdev specific params
 * @psoc: pointer to psoc object
 * @pdev: pointer to pdev object
 * @device_mode: device mode
 * @vdev_id: vdev ID
 *
 * Used to configure per vdev firmware params based on device mode. This is
 * invoked from hdd during vdev creation.
 *
 * Return: QDF Status
 */
QDF_STATUS ucfg_fwol_configure_vdev_params(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_pdev *pdev,
					   enum QDF_OPMODE device_mode,
					   uint8_t vdev_id);
#else
static inline QDF_STATUS ucfg_fwol_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void ucfg_fwol_psoc_close(struct wlan_objmgr_psoc *psoc)
{
}

static inline QDF_STATUS ucfg_fwol_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline void ucfg_fwol_deinit(void)
{
}

static inline QDF_STATUS
ucfg_fwol_get_coex_config_params(struct wlan_objmgr_psoc *psoc,
				 struct wlan_fwol_coex_config *coex_config)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_thermal_temp(struct wlan_objmgr_psoc *psoc,
			   struct wlan_fwol_thermal_temp *thermal_temp)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_neighbor_report_cfg(struct wlan_objmgr_psoc *psoc,
				  struct wlan_fwol_neighbor_report_cfg
				  *fwol_neighbor_report_cfg)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_is_neighbor_report_req_supported(struct wlan_objmgr_psoc *psoc,
					   bool *neighbor_report_req)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_ie_whitelist(struct wlan_objmgr_psoc *psoc, bool *ie_whitelist)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_set_ie_whitelist(struct wlan_objmgr_psoc *psoc, bool ie_whitelist)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_all_whitelist_params(struct wlan_objmgr_psoc *psoc,
				   struct wlan_fwol_ie_whitelist *whitelist)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_ani_enabled(struct wlan_objmgr_psoc *psoc,
			  bool *ani_enabled)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_enable_rts_sifsbursting(struct wlan_objmgr_psoc *psoc,
				 bool *enable_rts_sifsbursting)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_max_mpdus_inampdu(struct wlan_objmgr_psoc *psoc,
			   uint8_t *max_mpdus_inampdu)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_enable_phy_reg_retention(struct wlan_objmgr_psoc *psoc,
				  uint8_t *enable_phy_reg_retention)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_upper_brssi_thresh(struct wlan_objmgr_psoc *psoc,
			    uint16_t *upper_brssi_thresh)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_lower_brssi_thresh(struct wlan_objmgr_psoc *psoc,
			    uint16_t *lower_brssi_thresh)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_enable_dtim_1chrx(struct wlan_objmgr_psoc *psoc,
			   bool *enable_dtim_1chrx)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_alternative_chainmask_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *alternative_chainmask_enabled)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_get_smart_chainmask_enabled(struct wlan_objmgr_psoc *psoc,
				 bool *smart_chainmask_enabled)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_rts_profile(struct wlan_objmgr_psoc *psoc,
			  uint16_t *get_rts_profile)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_enable_fw_log_level(struct wlan_objmgr_psoc *psoc,
				  uint16_t *enable_fw_log_level)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_enable_fw_log_type(struct wlan_objmgr_psoc *psoc,
				 uint16_t *enable_fw_log_type)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_enable_fw_module_log_level(
				struct wlan_objmgr_psoc *psoc,
				uint8_t **enable_fw_module_log_level,
				uint8_t *enable_fw_module_log_level_num)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_sap_xlna_bypass(struct wlan_objmgr_psoc *psoc,
			      uint8_t *sap_xlna_bypass)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_tsf_gpio_pin(struct wlan_objmgr_psoc *psoc,
			   uint32_t *tsf_gpio_pin)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_tsf_ptp_options(struct wlan_objmgr_psoc *psoc,
			      uint32_t *tsf_ptp_options)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_lprx_enable(struct wlan_objmgr_psoc *psoc,
			  bool *lprx_enable)
{
	return QDF_STATUS_E_FAILURE;
}

static inline bool ucfg_fwol_get_sae_enable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool ucfg_fwol_get_gcmp_enable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline QDF_STATUS
ucfg_fwol_get_enable_tx_sch_delay(struct wlan_objmgr_psoc *psoc,
				  uint8_t *enable_tx_sch_delay)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_enable_secondary_rate(struct wlan_objmgr_psoc *psoc,
				    uint32_t *enable_secondary_rate)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_all_adaptive_dwelltime_params(
			struct wlan_objmgr_psoc *psoc,
			struct adaptive_dwelltime_params *dwelltime_params)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_adaptive_dwell_mode_enabled(struct wlan_objmgr_psoc *psoc,
					  bool *adaptive_dwell_mode_enabled)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_global_adapt_dwelltime_mode(struct wlan_objmgr_psoc *psoc,
					  uint8_t *global_adapt_dwelltime_mode)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_adapt_dwell_lpf_weight(struct wlan_objmgr_psoc *psoc,
				     uint8_t *adapt_dwell_lpf_weight)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_adapt_dwell_passive_mon_intval(
				struct wlan_objmgr_psoc *psoc,
				uint8_t *adapt_dwell_passive_mon_intval)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_adapt_dwell_wifi_act_threshold(
				struct wlan_objmgr_psoc *psoc,
				uint8_t *adapt_dwell_wifi_act_threshold)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_init_adapt_dwelltime_in_cfg(
			struct wlan_objmgr_psoc *psoc,
			struct adaptive_dwelltime_params *dwelltime_params)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_set_adaptive_dwelltime_config(
			struct adaptive_dwelltime_params *dwelltime_params)
{
	return QDF_STATUS_E_FAILURE;
}

#ifdef FEATURE_WLAN_RA_FILTERING
static inline QDF_STATUS
ucfg_fwol_set_is_rate_limit_enabled(struct wlan_objmgr_psoc *psoc,
				    bool is_rate_limit_enabled)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_get_is_rate_limit_enabled(struct wlan_objmgr_psoc *psoc,
				    bool *is_rate_limit_enabled)
{
	return QDF_STATUS_E_FAILURE;
}
#endif /* FEATURE_WLAN_RA_FILTERING */

static inline QDF_STATUS
ucfg_fwol_configure_global_params(struct wlan_objmgr_psoc *psoc,
				  struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
ucfg_fwol_configure_vdev_params(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_pdev *pdev,
				enum QDF_OPMODE device_mode, uint8_t vdev_id)
{
	return QDF_STATUS_E_FAILURE;
}

#endif /* WLAN_FW_OFFLOAD */

#endif /* _WLAN_FWOL_UCFG_API_H_ */
