/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC: declare various api which shall be used by
 * pmo user configuration and target interface
 */

#ifndef _WLAN_PMO_MAIN_H_
#define _WLAN_PMO_MAIN_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_pmo_common_public_struct.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"
#include "wlan_pmo_priv.h"
#include "wlan_pmo_objmgr.h"

#define pmo_fatal(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_PMO, params)
#define pmo_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_PMO, params)
#define pmo_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_PMO, params)
#define pmo_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_PMO, params)
#define pmo_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_PMO, params)

#define pmo_nofl_fatal(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_PMO, params)
#define pmo_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_PMO, params)
#define pmo_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_PMO, params)
#define pmo_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_PMO, params)
#define pmo_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_PMO, params)

#define pmo_enter() QDF_TRACE_ENTER(QDF_MODULE_ID_PMO, "enter")
#define pmo_exit() QDF_TRACE_EXIT(QDF_MODULE_ID_PMO, "exit")

#define PMO_VDEV_IN_STA_MODE(mode) \
	((mode) == QDF_STA_MODE || (mode) == QDF_P2P_CLIENT_MODE ? 1 : 0)

static inline enum QDF_OPMODE pmo_get_vdev_opmode(struct wlan_objmgr_vdev *vdev)
{
	return wlan_vdev_mlme_get_opmode(vdev);
}

/**
 * pmo_allocate_ctx() - Api to allocate pmo ctx
 *
 * Helper function to allocate pmo ctx
 *
 * Return: Success or failure.
 */
QDF_STATUS pmo_allocate_ctx(void);

/**
 * pmo_free_ctx() - to free pmo context
 *
 * Helper function to free pmo context
 *
 * Return: None.
 */
void pmo_free_ctx(void);

/**
 * pmo_get_context() - to get pmo context
 *
 * Helper function to get pmo context
 *
 * Return: pmo context.
 */
struct wlan_pmo_ctx *pmo_get_context(void);

/**
 * pmo_psoc_open() - pmo psoc object open
 * @psoc: objmgr vdev
 *.
 * This function used to open pmo psoc object
 *
 * Return: Success or failure
 */
QDF_STATUS pmo_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_psoc_close() - pmo psoc object close
 * @psoc: objmgr vdev
 *.
 * This function used to close pmo psoc object
 *
 * Return: Success or failure
 */
QDF_STATUS pmo_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_get_vdev_bss_peer_mac_addr() - API to get bss peer mac address
 * @vdev: objmgr vdev
 * @bss_peer_mac_address: bss peer mac address
 *.
 * Helper function to  get bss peer mac address
 *
 * Return: if success pmo vdev ctx else NULL
 */
QDF_STATUS pmo_get_vdev_bss_peer_mac_addr(struct wlan_objmgr_vdev *vdev,
		struct qdf_mac_addr *bss_peer_mac_address);

/**
 * pmo_is_vdev_in_beaconning_mode() - check if vdev is in a beaconning mode
 * @vdev_opmode: vdev opmode
 *
 * Helper function to know whether given vdev
 * is in a beaconning mode or not.
 *
 * Return: True if vdev needs to beacon.
 */
bool pmo_is_vdev_in_beaconning_mode(enum QDF_OPMODE vdev_opmode);

/**
 * pmo_core_is_ap_mode_supports_arp_ns() - To check ap mode supports arp/ns
 * @vdev_opmode: vdev opmode
 *
 * API to check if ap mode supports arp/ns offload
 *
 * Return: True  if ap mode supports arp/ns offload
 */

bool pmo_core_is_ap_mode_supports_arp_ns(struct wlan_objmgr_psoc *psoc,
	enum QDF_OPMODE vdev_opmode);

/**
 * pmo_core_is_vdev_supports_offload() - Check offload is supported on vdev
 * @vdev: objmgr vdev
 *
 * Return: true in case success else false
 */
bool pmo_core_is_vdev_supports_offload(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_core_get_psoc_config(): API to get the psoc user configurations of pmo
 * @psoc: objmgr psoc handle
 * @psoc_cfg: fill the current psoc user configurations.
 *
 * Return pmo psoc configurations
 */
QDF_STATUS pmo_core_get_psoc_config(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg);

/**
 * pmo_core_update_psoc_config(): API to update the psoc user configurations
 * @psoc: objmgr psoc handle
 * @psoc_cfg: pmo psoc configurations
 *
 * This api shall be used for soc config initialization as well update.
 * In case of update caller must first call pmo_get_psoc_cfg to get
 * current config and then apply changes on top of current config.
 *
 * Return QDF_STATUS -in case of success else return error
 */
QDF_STATUS pmo_core_update_psoc_config(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg);

/**
 * pmo_psoc_set_caps() - overwrite configured device capability flags
 * @psoc: the psoc for which the capabilities apply
 * @caps: the cabability information to configure
 *
 * Return: None
 */
void pmo_psoc_set_caps(struct wlan_objmgr_psoc *psoc,
		       struct pmo_device_caps *caps);

/**
 * pmo_core_get_vdev_op_mode(): API to get the vdev operation mode
 * @vdev: objmgr vdev handle
 *
 * API to get the vdev operation mode
 *
 * Return QDF_MAX_NO_OF_MODE - in case of error else return vdev opmode
 */
static inline enum QDF_OPMODE pmo_core_get_vdev_op_mode(
					struct wlan_objmgr_vdev *vdev)
{
	enum QDF_OPMODE op_mode = QDF_MAX_NO_OF_MODE;

	if (!vdev)
		return op_mode;
	op_mode = wlan_vdev_mlme_get_opmode(vdev);

	return op_mode;
}

/**
 * pmo_core_psoc_update_dp_handle() - update psoc data path handle
 * @psoc: objmgr psoc handle
 * @dp_hdl: psoc data path handle
 *
 * Return: None
 */
static inline void
pmo_core_psoc_update_dp_handle(struct wlan_objmgr_psoc *psoc, void *dp_hdl)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->dp_hdl = dp_hdl;
	}
}

/**
 * pmo_core_psoc_get_dp_handle() - Get psoc data path handle
 * @psoc: objmgr psoc handle
 *
 * Return: psoc data path handle
 */
static inline void *pmo_core_psoc_get_dp_handle(struct wlan_objmgr_psoc *psoc)
{
	void *dp_hdl = NULL;
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		dp_hdl = psoc_ctx->dp_hdl;
	}

	return dp_hdl;
}

/**
 * pmo_core_vdev_get_dp_handle() - Get vdev data path handle
 * @psoc_ctx: pmo psoc private context
 * @vdev_id: vdev id config to get data path handle
 *
 * Return: Vdev data path handle
 */
static inline
struct cdp_vdev *pmo_core_vdev_get_dp_handle(struct pmo_psoc_priv_obj *psoc_ctx,
					     uint8_t vdev_id)
{
	struct cdp_vdev *dp_hdl = NULL;
	pmo_get_vdev_dp_handle handler;

	qdf_spin_lock_bh(&psoc_ctx->lock);
	handler = psoc_ctx->get_vdev_dp_handle;
	qdf_spin_unlock_bh(&psoc_ctx->lock);

	if (handler)
		dp_hdl = handler(vdev_id);

	return dp_hdl;
}

/**
 * pmo_core_psoc_update_htc_handle() - update psoc htc layer handle
 * @psoc: objmgr psoc handle
 * @htc_hdl: psoc htc layer handle
 *
 * Return: None
 */
static inline void
pmo_core_psoc_update_htc_handle(struct wlan_objmgr_psoc *psoc, void *htc_hdl)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->htc_hdl = htc_hdl;
	}
}

/**
 * pmo_core_psoc_get_htc_handle() - Get psoc htc layer handle
 * @psoc: objmgr psoc handle
 *
 * Return: psoc htc layer handle
 */
static inline void *pmo_core_psoc_get_htc_handle(struct wlan_objmgr_psoc *psoc)
{
	void *htc_hdl = NULL;
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		htc_hdl = psoc_ctx->htc_hdl;
	}

	return htc_hdl;
}

/**
 * pmo_core_psoc_set_hif_handle() - update psoc hif layer handle
 * @psoc: objmgr psoc handle
 * @hif_hdl: hif context handle
 *
 * Return: None
 */
void pmo_core_psoc_set_hif_handle(struct wlan_objmgr_psoc *psoc,
				  void *hif_hdl);

/**
 * pmo_core_psoc_get_hif_handle() - Get psoc hif layer handle
 * @psoc: objmgr psoc handle
 *
 * Return: psoc hif layer handle
 */
void *pmo_core_psoc_get_hif_handle(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_core_psoc_set_txrx_pdev_id() - update psoc pdev txrx layer handle
 * @psoc: objmgr psoc handle
 * @txrx_pdev_id: txrx pdev identifier
 *
 * Return: None
 */
void pmo_core_psoc_set_txrx_pdev_id(struct wlan_objmgr_psoc *psoc,
				    uint8_t txrx_pdev_id);

/**
 * pmo_core_psoc_get_txrx_handle() - Get psoc pdev txrx handle
 * @psoc: objmgr psoc handle
 *
 * Return: txrx pdev identifier
 */
uint8_t pmo_core_psoc_get_txrx_handle(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_intersect_arp_ns_offload() - intersect config and firmware capability for
 *	the ARP/NS Offload feature
 * @psoc_ctx: A PMO psoc context
 *
 * Note: The caller is expected to grab the PMO context lock.
 *
 * Return: True if firmware supports and configuration has enabled the feature
 */
static inline bool
pmo_intersect_arp_ns_offload(struct pmo_psoc_priv_obj *psoc_ctx)
{
	struct pmo_psoc_cfg *cfg = &psoc_ctx->psoc_cfg;
	bool arp_ns_enabled =
		cfg->ns_offload_enable_static ||
		cfg->ns_offload_enable_dynamic ||
		cfg->arp_offload_enable;

	return arp_ns_enabled && psoc_ctx->caps.arp_ns_offload;
}

/**
 * pmo_intersect_apf() - intersect config and firmware capability for
 *	the APF feature
 * @psoc_ctx: A PMO psoc context
 *
 * Note: The caller is expected to grab the PMO context lock.
 *
 * Return: True if firmware supports and configuration has enabled the feature
 */
static inline bool pmo_intersect_apf(struct pmo_psoc_priv_obj *psoc_ctx)
{
	return psoc_ctx->psoc_cfg.apf_enable && psoc_ctx->caps.apf;
}

/**
 * pmo_intersect_packet_filter() - intersect config and firmware capability for
 *	the APF feature
 * @psoc_ctx: A PMO psoc context
 *
 * Note: The caller is expected to grab the PMO context lock.
 *
 * Return: True if firmware supports and configuration has enabled the feature
 */
static inline bool
pmo_intersect_packet_filter(struct pmo_psoc_priv_obj *psoc_ctx)
{
	return psoc_ctx->psoc_cfg.packet_filter_enabled &&
		psoc_ctx->caps.packet_filter;
}

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* end  of _WLAN_PMO_MAIN_H_ */
