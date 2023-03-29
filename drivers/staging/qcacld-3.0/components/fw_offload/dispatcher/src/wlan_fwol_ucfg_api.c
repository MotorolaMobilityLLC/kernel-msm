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
 * DOC: define internal APIs related to the fwol component
 */

#include "wlan_fw_offload_main.h"
#include "wlan_fwol_public_structs.h"
#include "wlan_fwol_ucfg_api.h"
#include "wlan_fwol_tgt_api.h"
#include "target_if_fwol.h"
#include "wlan_objmgr_vdev_obj.h"

QDF_STATUS ucfg_fwol_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;

	status = fwol_cfg_on_psoc_enable(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		fwol_err("Failed to initialize FWOL CFG");

	return status;
}

void ucfg_fwol_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	/* Clear the FWOL CFG Structure */
}

void ucfg_fwol_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	tgt_fwol_register_ev_handler(psoc);
}

void ucfg_fwol_psoc_disable(struct wlan_objmgr_psoc *psoc)
{

	tgt_fwol_unregister_ev_handler(psoc);
}

/**
 * fwol_psoc_object_created_notification(): fwol psoc create handler
 * @psoc: psoc which is going to created by objmgr
 * @arg: argument for vdev create handler
 *
 * Register this api with objmgr to detect psoc is created
 *
 * Return QDF_STATUS status in case of success else return error
 */
static QDF_STATUS
fwol_psoc_object_created_notification(struct wlan_objmgr_psoc *psoc, void *arg)
{
	QDF_STATUS status;
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = qdf_mem_malloc(sizeof(*fwol_obj));
	if (!fwol_obj)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
						       WLAN_UMAC_COMP_FWOL,
						       fwol_obj,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		fwol_err("Failed to attach psoc_ctx with psoc");
		qdf_mem_free(fwol_obj);
		return status;
	}

	tgt_fwol_register_rx_ops(&fwol_obj->rx_ops);
	target_if_fwol_register_tx_ops(&fwol_obj->tx_ops);

	return status;
}

/**
 * fwol_psoc_object_destroyed_notification(): fwol psoc delete handler
 * @psoc: psoc which is going to delete by objmgr
 * @arg: argument for vdev delete handler
 *
 * Register this api with objmgr to detect psoc is deleted
 *
 * Return QDF_STATUS status in case of success else return error
 */
static QDF_STATUS fwol_psoc_object_destroyed_notification(
		struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct wlan_fwol_psoc_obj *fwol_obj;
	QDF_STATUS status;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
						       WLAN_UMAC_COMP_FWOL,
						       fwol_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		fwol_err("Failed to detach psoc_ctx from psoc");
		return status;
	}

	qdf_mem_free(fwol_obj);

	return status;
}

QDF_STATUS ucfg_fwol_init(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_register_psoc_create_handler(
			WLAN_UMAC_COMP_FWOL,
			fwol_psoc_object_created_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		fwol_err("unable to register psoc create handle");
		return status;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
			WLAN_UMAC_COMP_FWOL,
			fwol_psoc_object_destroyed_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		fwol_err("unable to register psoc create handle");
		wlan_objmgr_unregister_psoc_create_handler(
			WLAN_UMAC_COMP_FWOL,
			fwol_psoc_object_created_notification,
			NULL);
	}

	return status;
}

void ucfg_fwol_deinit(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_unregister_psoc_destroy_handler(
			WLAN_UMAC_COMP_FWOL,
			fwol_psoc_object_destroyed_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status))
		fwol_err("unable to unregister psoc destroy handle");

	status = wlan_objmgr_unregister_psoc_create_handler(
			WLAN_UMAC_COMP_FWOL,
			fwol_psoc_object_created_notification,
			NULL);
	if (QDF_IS_STATUS_ERROR(status))
		fwol_err("unable to unregister psoc create handle");
}

QDF_STATUS
ucfg_fwol_get_coex_config_params(struct wlan_objmgr_psoc *psoc,
				 struct wlan_fwol_coex_config *coex_config)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		return QDF_STATUS_E_FAILURE;
	}

	*coex_config = fwol_obj->cfg.coex_config;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_fwol_get_thermal_temp(struct wlan_objmgr_psoc *psoc,
			   struct wlan_fwol_thermal_temp *thermal_info)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		return QDF_STATUS_E_FAILURE;
	}

	*thermal_info = fwol_obj->cfg.thermal_temp_cfg;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_fwol_get_neighbor_report_cfg(struct wlan_objmgr_psoc *psoc,
				  struct wlan_fwol_neighbor_report_cfg
				  *fwol_neighbor_report_cfg)
{
	struct wlan_fwol_psoc_obj *fwol_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!fwol_neighbor_report_cfg)
		return QDF_STATUS_E_FAILURE;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		fwol_init_neighbor_report_cfg(psoc, fwol_neighbor_report_cfg);
		status =  QDF_STATUS_E_FAILURE;
	} else {
		*fwol_neighbor_report_cfg = fwol_obj->cfg.neighbor_report_cfg;
	}

	return status;
}

QDF_STATUS
ucfg_fwol_is_neighbor_report_req_supported(struct wlan_objmgr_psoc *psoc,
					   bool *neighbor_report_req)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		*neighbor_report_req =
			 !!(cfg_get(psoc,
			    CFG_OFFLOAD_11K_ENABLE_BITMASK) &
			    OFFLOAD_11K_BITMASK_NEIGHBOR_REPORT_REQUEST);
		return QDF_STATUS_E_FAILURE;
	}

	*neighbor_report_req =
		!!(fwol_obj->cfg.neighbor_report_cfg.enable_bitmask &
		   OFFLOAD_11K_BITMASK_NEIGHBOR_REPORT_REQUEST);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_fwol_get_ie_whitelist(struct wlan_objmgr_psoc *psoc, bool *ie_whitelist)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		return QDF_STATUS_E_FAILURE;
	}

	*ie_whitelist = fwol_obj->cfg.ie_whitelist_cfg.ie_whitelist;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_fwol_set_ie_whitelist(struct wlan_objmgr_psoc *psoc, bool ie_whitelist)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		return QDF_STATUS_E_FAILURE;
	}

	fwol_obj->cfg.ie_whitelist_cfg.ie_whitelist = ie_whitelist;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_ani_enabled(struct wlan_objmgr_psoc *psoc,
				     bool *ani_enabled)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*ani_enabled = fwol_obj->cfg.ani_enabled;
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ucfg_fwol_get_ilp_config(struct wlan_objmgr_psoc *psoc,
					   uint32_t *enable_ilp)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*enable_ilp = fwol_obj->cfg.enable_ilp;
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS ucfg_fwol_get_hw_assist_config(struct wlan_objmgr_psoc *psoc,
						 bool *disable_hw_assist)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*disable_hw_assist = fwol_obj->cfg.disable_hw_assist;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_get_enable_rts_sifsbursting(struct wlan_objmgr_psoc *psoc,
					    bool *enable_rts_sifsbursting)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*enable_rts_sifsbursting = fwol_obj->cfg.enable_rts_sifsbursting;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_get_enable_sifs_burst(struct wlan_objmgr_psoc *psoc,
				      uint8_t *enable_sifs_burst)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*enable_sifs_burst = fwol_obj->cfg.enable_sifs_burst;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_get_max_mpdus_inampdu(struct wlan_objmgr_psoc *psoc,
				      uint8_t *max_mpdus_inampdu)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*max_mpdus_inampdu = fwol_obj->cfg.max_mpdus_inampdu;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_get_enable_phy_reg_retention(struct wlan_objmgr_psoc *psoc,
					     uint8_t *enable_phy_reg_retention)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*enable_phy_reg_retention = fwol_obj->cfg.enable_phy_reg_retention;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_fwol_get_all_whitelist_params(struct wlan_objmgr_psoc *psoc,
				   struct wlan_fwol_ie_whitelist *whitelist)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		return QDF_STATUS_E_FAILURE;
	}

	*whitelist = fwol_obj->cfg.ie_whitelist_cfg;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_get_upper_brssi_thresh(struct wlan_objmgr_psoc *psoc,
				       uint16_t *upper_brssi_thresh)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*upper_brssi_thresh = fwol_obj->cfg.upper_brssi_thresh;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_get_lower_brssi_thresh(struct wlan_objmgr_psoc *psoc,
				       uint16_t *lower_brssi_thresh)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*lower_brssi_thresh = fwol_obj->cfg.lower_brssi_thresh;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_get_enable_dtim_1chrx(struct wlan_objmgr_psoc *psoc,
				      bool *enable_dtim_1chrx)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*enable_dtim_1chrx = fwol_obj->cfg.enable_dtim_1chrx;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_get_alternative_chainmask_enabled(struct wlan_objmgr_psoc *psoc,
				       bool *alternative_chainmask_enabled)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		return QDF_STATUS_E_FAILURE;
	}

	*alternative_chainmask_enabled =
				fwol_obj->cfg.alternative_chainmask_enabled;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_get_smart_chainmask_enabled(struct wlan_objmgr_psoc *psoc,
					    bool *smart_chainmask_enabled)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*smart_chainmask_enabled =
				fwol_obj->cfg.smart_chainmask_enabled;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_rts_profile(struct wlan_objmgr_psoc *psoc,
				     uint16_t *get_rts_profile)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*get_rts_profile = fwol_obj->cfg.get_rts_profile;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_enable_fw_log_level(struct wlan_objmgr_psoc *psoc,
					     uint16_t *enable_fw_log_level)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*enable_fw_log_level = fwol_obj->cfg.enable_fw_log_level;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_enable_fw_log_type(struct wlan_objmgr_psoc *psoc,
					    uint16_t *enable_fw_log_type)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*enable_fw_log_type = fwol_obj->cfg.enable_fw_log_type;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_enable_fw_module_log_level(
				struct wlan_objmgr_psoc *psoc,
				uint8_t **enable_fw_module_log_level,
				uint8_t *enable_fw_module_log_level_num)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*enable_fw_module_log_level = fwol_obj->cfg.enable_fw_module_log_level;
	*enable_fw_module_log_level_num =
				fwol_obj->cfg.enable_fw_module_log_level_num;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_wow_get_enable_fw_module_log_level(
				struct wlan_objmgr_psoc *psoc,
				uint8_t **enable_fw_wow_module_log_level,
				uint8_t *enable_fw_wow_module_log_level_num)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*enable_fw_wow_module_log_level =
				fwol_obj->cfg.enable_fw_mod_wow_log_level;
	*enable_fw_wow_module_log_level_num =
				fwol_obj->cfg.enable_fw_mod_wow_log_level_num;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_sap_xlna_bypass(struct wlan_objmgr_psoc *psoc,
					 bool *sap_xlna_bypass)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*sap_xlna_bypass = fwol_obj->cfg.sap_xlna_bypass;
	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_RA_FILTERING
QDF_STATUS ucfg_fwol_set_is_rate_limit_enabled(struct wlan_objmgr_psoc *psoc,
					       bool is_rate_limit_enabled)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	fwol_obj->cfg.is_rate_limit_enabled = is_rate_limit_enabled;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_is_rate_limit_enabled(struct wlan_objmgr_psoc *psoc,
					       bool *is_rate_limit_enabled)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*is_rate_limit_enabled = fwol_obj->cfg.is_rate_limit_enabled;
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_TSF
QDF_STATUS ucfg_fwol_get_tsf_gpio_pin(struct wlan_objmgr_psoc *psoc,
				      uint32_t *tsf_gpio_pin)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		*tsf_gpio_pin = cfg_default(CFG_SET_TSF_GPIO_PIN);
		return QDF_STATUS_E_FAILURE;
	}

	*tsf_gpio_pin = fwol_obj->cfg.tsf_gpio_pin;
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_TSF_PLUS
QDF_STATUS ucfg_fwol_get_tsf_ptp_options(struct wlan_objmgr_psoc *psoc,
					 uint32_t *tsf_ptp_options)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		*tsf_ptp_options = cfg_default(CFG_SET_TSF_PTP_OPT);
		return QDF_STATUS_E_FAILURE;
	}

	*tsf_ptp_options = fwol_obj->cfg.tsf_ptp_options;
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_IRQ
QDF_STATUS
ucfg_fwol_get_tsf_irq_host_gpio_pin(struct wlan_objmgr_psoc *psoc,
				    uint32_t *tsf_irq_host_gpio_pin)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		*tsf_irq_host_gpio_pin =
			cfg_default(CFG_SET_TSF_IRQ_HOST_GPIO_PIN);
		return QDF_STATUS_E_FAILURE;
	}

	*tsf_irq_host_gpio_pin = fwol_obj->cfg.tsf_irq_host_gpio_pin;
	return QDF_STATUS_SUCCESS;
}

#endif

#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC
QDF_STATUS
ucfg_fwol_get_tsf_sync_host_gpio_pin(struct wlan_objmgr_psoc *psoc,
				     uint32_t *tsf_sync_host_gpio_pin)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		*tsf_sync_host_gpio_pin =
			cfg_default(CFG_SET_TSF_SYNC_HOST_GPIO_PIN);
		return QDF_STATUS_E_FAILURE;
	}

	*tsf_sync_host_gpio_pin = fwol_obj->cfg.tsf_sync_host_gpio_pin;
	return QDF_STATUS_SUCCESS;
}

#endif
#endif
#else
QDF_STATUS ucfg_fwol_get_tsf_gpio_pin(struct wlan_objmgr_psoc *psoc,
				      uint32_t *tsf_gpio_pin)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_tsf_ptp_options(struct wlan_objmgr_psoc *psoc,
					 uint32_t *tsf_ptp_options)
{
	return QDF_STATUS_E_NOSUPPORT;
}

#endif

QDF_STATUS ucfg_fwol_get_lprx_enable(struct wlan_objmgr_psoc *psoc,
				     bool *lprx_enable)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		*lprx_enable = cfg_default(CFG_LPRX);
		return QDF_STATUS_E_FAILURE;
	}

	*lprx_enable = fwol_obj->cfg.lprx_enable;
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_SAE
bool ucfg_fwol_get_sae_enable(struct wlan_objmgr_psoc *psoc)
{
	return cfg_get(psoc, CFG_IS_SAE_ENABLED);
}

#else
bool ucfg_fwol_get_sae_enable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}
#endif

bool ucfg_fwol_get_gcmp_enable(struct wlan_objmgr_psoc *psoc)
{
	return cfg_get(psoc, CFG_ENABLE_GCMP);
}

QDF_STATUS ucfg_fwol_get_enable_tx_sch_delay(struct wlan_objmgr_psoc *psoc,
					     uint8_t *enable_tx_sch_delay)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		*enable_tx_sch_delay = cfg_default(CFG_TX_SCH_DELAY);
		return QDF_STATUS_E_FAILURE;
	}

	*enable_tx_sch_delay = fwol_obj->cfg.enable_tx_sch_delay;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_enable_secondary_rate(struct wlan_objmgr_psoc *psoc,
					       uint32_t *enable_secondary_rate)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		*enable_secondary_rate = cfg_default(CFG_ENABLE_SECONDARY_RATE);
		return QDF_STATUS_E_FAILURE;
	}

	*enable_secondary_rate = fwol_obj->cfg.enable_secondary_rate;
	return QDF_STATUS_SUCCESS;
}

#ifdef DHCP_SERVER_OFFLOAD
QDF_STATUS
ucfg_fwol_get_enable_dhcp_server_offload(struct wlan_objmgr_psoc *psoc,
					 bool *enable_dhcp_server_offload)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*enable_dhcp_server_offload = fwol_obj->cfg.enable_dhcp_server_offload;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_dhcp_max_num_clients(struct wlan_objmgr_psoc *psoc,
					      uint32_t *dhcp_max_num_clients)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*dhcp_max_num_clients = fwol_obj->cfg.dhcp_max_num_clients;
	return QDF_STATUS_SUCCESS;
}

#endif

QDF_STATUS
ucfg_fwol_get_all_adaptive_dwelltime_params(
			struct wlan_objmgr_psoc *psoc,
			struct adaptive_dwelltime_params *dwelltime_params)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		return QDF_STATUS_E_FAILURE;
	}

	*dwelltime_params = fwol_obj->cfg.dwelltime_params;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_fwol_get_adaptive_dwell_mode_enabled(
				struct wlan_objmgr_psoc *psoc,
				bool *adaptive_dwell_mode_enabled)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*adaptive_dwell_mode_enabled =
			fwol_obj->cfg.dwelltime_params.is_enabled;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_fwol_get_global_adapt_dwelltime_mode(struct wlan_objmgr_psoc *psoc,
					  uint8_t *global_adapt_dwelltime_mode)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*global_adapt_dwelltime_mode =
			fwol_obj->cfg.dwelltime_params.dwelltime_mode;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ucfg_fwol_get_adapt_dwell_lpf_weight(struct wlan_objmgr_psoc *psoc,
				     uint8_t *adapt_dwell_lpf_weight)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*adapt_dwell_lpf_weight = fwol_obj->cfg.dwelltime_params.lpf_weight;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_adapt_dwell_passive_mon_intval(
				struct wlan_objmgr_psoc *psoc,
				uint8_t *adapt_dwell_passive_mon_intval)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*adapt_dwell_passive_mon_intval =
			fwol_obj->cfg.dwelltime_params.passive_mon_intval;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_get_adapt_dwell_wifi_act_threshold(
				struct wlan_objmgr_psoc *psoc,
				uint8_t *adapt_dwell_wifi_act_threshold)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL obj");
		return QDF_STATUS_E_FAILURE;
	}

	*adapt_dwell_wifi_act_threshold =
			fwol_obj->cfg.dwelltime_params.wifi_act_threshold;
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_ELNA
QDF_STATUS ucfg_fwol_set_elna_bypass(struct wlan_objmgr_vdev *vdev,
				     struct set_elna_bypass_request *req)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_fwol_psoc_obj *fwol_obj;
	struct wlan_fwol_tx_ops *tx_ops;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		fwol_err("NULL pointer for psoc");
		return QDF_STATUS_E_INVAL;
	}

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL Obj");
		return QDF_STATUS_E_INVAL;
	}

	tx_ops = &fwol_obj->tx_ops;
	if (tx_ops->set_elna_bypass)
		status = tx_ops->set_elna_bypass(psoc, req);
	else
		status = QDF_STATUS_E_IO;

	return status;
}

QDF_STATUS ucfg_fwol_get_elna_bypass(struct wlan_objmgr_vdev *vdev,
				     struct get_elna_bypass_request *req,
				     void (*callback)(void *context,
				     struct get_elna_bypass_response *response),
				     void *context)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_fwol_psoc_obj *fwol_obj;
	struct wlan_fwol_tx_ops *tx_ops;
	struct wlan_fwol_callbacks *cbs;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		fwol_err("NULL pointer for psoc");
		return QDF_STATUS_E_INVAL;
	}

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL Obj");
		return QDF_STATUS_E_INVAL;
	}

	cbs = &fwol_obj->cbs;
	cbs->get_elna_bypass_callback = callback;
	cbs->get_elna_bypass_context = context;

	tx_ops = &fwol_obj->tx_ops;
	if (tx_ops->get_elna_bypass)
		status = tx_ops->get_elna_bypass(psoc, req);
	else
		status = QDF_STATUS_E_IO;

	return status;
}
#endif /* WLAN_FEATURE_ELNA */

#ifdef WLAN_SEND_DSCP_UP_MAP_TO_FW
QDF_STATUS ucfg_fwol_send_dscp_up_map_to_fw(struct wlan_objmgr_vdev *vdev,
					   uint32_t *dscp_to_up_map)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_fwol_psoc_obj *fwol_obj;
	struct wlan_fwol_tx_ops *tx_ops;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		fwol_err("NULL pointer for psoc");
		return QDF_STATUS_E_INVAL;
	}

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL Obj");
		return QDF_STATUS_E_INVAL;
	}

	tx_ops = &fwol_obj->tx_ops;
	if (tx_ops && tx_ops->send_dscp_up_map_to_fw)
		status = tx_ops->send_dscp_up_map_to_fw(psoc, dscp_to_up_map);
	else
		status = QDF_STATUS_E_IO;

	return status;
}
#endif /* WLAN_SEND_DSCP_UP_MAP_TO_FW */

void ucfg_fwol_update_fw_cap_info(struct wlan_objmgr_psoc *psoc,
				  struct wlan_fwol_capability_info *caps)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		return;
	}

	qdf_mem_copy(&fwol_obj->capability_info, caps,
		     sizeof(fwol_obj->capability_info));
}

#ifdef THERMAL_STATS_SUPPORT
static QDF_STATUS
ucfg_fwol_get_cap(struct wlan_objmgr_psoc *psoc,
		  struct wlan_fwol_capability_info *cap_info)
{
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get fwol obj");
		return QDF_STATUS_E_FAILURE;
	}

	if (!cap_info) {
		fwol_err("Failed to get fwol obj");
		return QDF_STATUS_E_FAILURE;
	}
	*cap_info = fwol_obj->capability_info;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS ucfg_fwol_send_get_thermal_stats_cmd(struct wlan_objmgr_psoc *psoc,
				       enum thermal_stats_request_type req_type,
				       void (*callback)(void *context,
				       struct thermal_throttle_info *response),
				       void *context)
{
	QDF_STATUS status;
	struct wlan_fwol_psoc_obj *fwol_obj;
	struct wlan_fwol_tx_ops *tx_ops;
	struct wlan_fwol_thermal_temp thermal_temp = {0};
	struct wlan_fwol_capability_info cap_info;
	struct wlan_fwol_callbacks *cbs;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		fwol_err("Failed to get FWOL Obj");
		return QDF_STATUS_E_INVAL;
	}

	status = ucfg_fwol_get_thermal_temp(psoc, &thermal_temp);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_INVAL;

	status = ucfg_fwol_get_cap(psoc, &cap_info);
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_INVAL;

	if (!thermal_temp.therm_stats_offset ||
	    !cap_info.fw_thermal_stats_cap) {
		fwol_err("Command Disabled in Ini gThermalStatsTempOffset %d or not enabled in FW %d",
			 thermal_temp.therm_stats_offset,
			 cap_info.fw_thermal_stats_cap);
		return QDF_STATUS_E_INVAL;
	}

	/* Registering Callback for the Request command */
	if (callback && context) {
		cbs = &fwol_obj->cbs;
		cbs->get_thermal_stats_callback = callback;
		cbs->get_thermal_stats_context = context;
	}

	tx_ops = &fwol_obj->tx_ops;
	if (tx_ops && tx_ops->get_thermal_stats)
		status = tx_ops->get_thermal_stats(psoc, req_type,
					thermal_temp.therm_stats_offset);
	else
		status = QDF_STATUS_E_INVAL;

	return status;
}
#endif /* THERMAL_STATS_SUPPORT */

QDF_STATUS ucfg_fwol_configure_global_params(struct wlan_objmgr_psoc *psoc,
					     struct wlan_objmgr_pdev *pdev)
{
	QDF_STATUS status;
	uint32_t enable_ilp;
	bool value;

	/* Configure ILP feature in FW */
	status = ucfg_fwol_get_ilp_config(psoc, &enable_ilp);
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	status = fwol_set_ilp_config(pdev, enable_ilp);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	/* Configure HW assist feature in FW */
	status = ucfg_fwol_get_hw_assist_config(psoc, &value);
	if (QDF_IS_STATUS_ERROR(status))
		return status;
	status = fwol_configure_hw_assist(pdev, value);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	return status;
}

QDF_STATUS ucfg_fwol_configure_vdev_params(struct wlan_objmgr_psoc *psoc,
					   struct wlan_objmgr_pdev *pdev,
					   enum QDF_OPMODE device_mode,
					   uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
