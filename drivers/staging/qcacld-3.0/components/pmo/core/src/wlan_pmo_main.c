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
 * DOC: Implement various api / helper function which shall be used
 * PMO user and target interface.
 */

#include "wlan_pmo_main.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"
#include "wlan_pmo_cfg.h"
#include "cfg_ucfg_api.h"
#include "wlan_fwol_ucfg_api.h"

static struct wlan_pmo_ctx *gp_pmo_ctx;

QDF_STATUS pmo_allocate_ctx(void)
{
	/* If it is already created, ignore */
	if (gp_pmo_ctx) {
		pmo_debug("already allocated pmo_ctx");
		return QDF_STATUS_SUCCESS;
	}

	/* allocate offload mgr ctx */
	gp_pmo_ctx = (struct wlan_pmo_ctx *)qdf_mem_malloc(
			sizeof(*gp_pmo_ctx));
	if (!gp_pmo_ctx)
		return QDF_STATUS_E_NOMEM;

	qdf_spinlock_create(&gp_pmo_ctx->lock);

	return QDF_STATUS_SUCCESS;
}

void pmo_free_ctx(void)
{
	if (!gp_pmo_ctx) {
		pmo_err("pmo ctx is already freed");
		QDF_ASSERT(0);
		return;
	}
	qdf_spinlock_destroy(&gp_pmo_ctx->lock);
	qdf_mem_free(gp_pmo_ctx);
	gp_pmo_ctx = NULL;
}

struct wlan_pmo_ctx *pmo_get_context(void)
{
	return gp_pmo_ctx;
}

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
static void wlan_extwow_init_cfg(struct wlan_objmgr_psoc *psoc,
				 struct pmo_psoc_cfg *psoc_cfg)
{
	psoc_cfg->extwow_goto_suspend =
			cfg_get(psoc, CFG_EXTWOW_GOTO_SUSPEND);
	psoc_cfg->extwow_app1_wakeup_pin_num =
			cfg_get(psoc, CFG_EXTWOW_APP1_WAKE_PIN_NUMBER);
	psoc_cfg->extwow_app2_wakeup_pin_num =
			cfg_get(psoc, CFG_EXTWOW_APP2_WAKE_PIN_NUMBER);
	psoc_cfg->extwow_app2_init_ping_interval =
			cfg_get(psoc, CFG_EXTWOW_KA_INIT_PING_INTERVAL);
	psoc_cfg->extwow_app2_min_ping_interval =
			cfg_get(psoc, CFG_EXTWOW_KA_MIN_PING_INTERVAL);
	psoc_cfg->extwow_app2_max_ping_interval =
			cfg_get(psoc, CFG_EXTWOW_KA_MAX_PING_INTERVAL);
	psoc_cfg->extwow_app2_inc_ping_interval =
			cfg_get(psoc, CFG_EXTWOW_KA_INC_PING_INTERVAL);
	psoc_cfg->extwow_app2_tcp_src_port =
			cfg_get(psoc, CFG_EXTWOW_TCP_SRC_PORT);
	psoc_cfg->extwow_app2_tcp_dst_port =
			cfg_get(psoc, CFG_EXTWOW_TCP_DST_PORT);
	psoc_cfg->extwow_app2_tcp_tx_timeout =
			cfg_get(psoc, CFG_EXTWOW_TCP_TX_TIMEOUT);
	psoc_cfg->extwow_app2_tcp_rx_timeout =
			cfg_get(psoc, CFG_EXTWOW_TCP_RX_TIMEOUT);
}
#else
static void wlan_extwow_init_cfg(struct wlan_objmgr_psoc *psoc,
				 struct pmo_psoc_cfg *psoc_cfg)
{
}
#endif

#ifdef WLAN_FEATURE_WOW_PULSE
static void wlan_pmo_wow_pulse_init_cfg(struct wlan_objmgr_psoc *psoc,
					struct pmo_psoc_cfg *psoc_cfg)
{
	psoc_cfg->is_wow_pulse_supported =
			cfg_get(psoc, CFG_PMO_WOW_PULSE_ENABLE);
	psoc_cfg->wow_pulse_pin = cfg_get(psoc, CFG_PMO_WOW_PULSE_PIN);
	psoc_cfg->wow_pulse_interval_high =
			cfg_get(psoc, CFG_PMO_WOW_PULSE_HIGH);
	psoc_cfg->wow_pulse_interval_low =
			cfg_get(psoc, CFG_PMO_WOW_PULSE_LOW);
	psoc_cfg->wow_pulse_repeat_count =
			cfg_get(psoc, CFG_PMO_WOW_PULSE_REPEAT);
	psoc_cfg->wow_pulse_init_state =
			cfg_get(psoc, CFG_PMO_WOW_PULSE_INIT);
}
#else
static void wlan_pmo_wow_pulse_init_cfg(struct wlan_objmgr_psoc *psoc,
					struct pmo_psoc_cfg *psoc_cfg)
{
}
#endif

#ifdef WLAN_FEATURE_PACKET_FILTERING
static void wlan_pmo_pkt_filter_init_cfg(struct wlan_objmgr_psoc *psoc,
					 struct pmo_psoc_cfg *psoc_cfg)
{
	psoc_cfg->packet_filters_bitmap = cfg_get(psoc, CFG_PMO_PKT_FILTER);
	psoc_cfg->packet_filter_enabled =
		!cfg_get(psoc, CFG_PMO_DISABLE_PKT_FILTER);
}
#else
static void wlan_pmo_pkt_filter_init_cfg(struct wlan_objmgr_psoc *psoc,
					 struct pmo_psoc_cfg *psoc_cfg)
{
	psoc_cfg->packet_filter_enabled =
		!cfg_get(psoc, CFG_PMO_DISABLE_PKT_FILTER);
}
#endif

#ifdef FEATURE_RUNTIME_PM
static void wlan_pmo_runtime_pm_init_cfg(struct wlan_objmgr_psoc *psoc,
					 struct pmo_psoc_cfg *psoc_cfg)
{
	psoc_cfg->runtime_pm_delay = cfg_get(psoc, CFG_PMO_RUNTIME_PM_DELAY);
}
#else
static void wlan_pmo_runtime_pm_init_cfg(struct wlan_objmgr_psoc *psoc,
					 struct pmo_psoc_cfg *psoc_cfg)
{
}
#endif

#if FEATURE_WLAN_RA_FILTERING
static void wlan_pmo_ra_filtering_init_cfg(struct wlan_objmgr_psoc *psoc,
					   struct pmo_psoc_cfg *psoc_cfg)
{
	bool ra_enable;

	if (QDF_IS_STATUS_SUCCESS(ucfg_fwol_get_is_rate_limit_enabled(psoc,
								   &ra_enable)))
		psoc_cfg->ra_ratelimit_enable = ra_enable;

	psoc_cfg->ra_ratelimit_interval =
			cfg_get(psoc, CFG_RA_RATE_LIMIT_INTERVAL);
}
#else
static void wlan_pmo_ra_filtering_init_cfg(struct wlan_objmgr_psoc *psoc,
					   struct pmo_psoc_cfg *psoc_cfg)
{
}
#endif

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
static void
wlan_pmo_get_igmp_version_support_cfg(struct wlan_objmgr_psoc *psoc,
				      struct pmo_psoc_cfg *psoc_cfg)
{
	psoc_cfg->igmp_version_support =
				cfg_get(psoc, CFG_IGMP_VERSION_SUPPORT);
}

static void
wlan_pmo_get_igmp_offload_enable_cfg(struct wlan_objmgr_psoc *psoc,
				     struct pmo_psoc_cfg *psoc_cfg)
{
	psoc_cfg->igmp_offload_enable = cfg_get(psoc,
						CFG_PMO_ENABLE_IGMP_OFFLOAD);
}
#else
static void
wlan_pmo_get_igmp_version_support_cfg(struct wlan_objmgr_psoc *psoc,
				      struct pmo_psoc_cfg *psoc_cfg)
{}

static void
wlan_pmo_get_igmp_offload_enable_cfg(struct wlan_objmgr_psoc *psoc,
				     struct pmo_psoc_cfg *psoc_cfg)
{}
#endif

static void wlan_pmo_init_cfg(struct wlan_objmgr_psoc *psoc,
			      struct pmo_psoc_cfg *psoc_cfg)
{
	psoc_cfg->arp_offload_enable =
			cfg_get(psoc, CFG_PMO_ENABLE_HOST_ARPOFFLOAD);
	psoc_cfg->hw_filter_mode_bitmap = cfg_get(psoc, CFG_PMO_HW_FILTER_MODE);
	psoc_cfg->ssdp = cfg_get(psoc, CFG_PMO_ENABLE_HOST_SSDP);
	psoc_cfg->ns_offload_enable_static =
			cfg_get(psoc, CFG_PMO_ENABLE_HOST_NSOFFLOAD);
	psoc_cfg->ns_offload_enable_dynamic =
			cfg_get(psoc, CFG_PMO_ENABLE_HOST_NSOFFLOAD);
	psoc_cfg->sta_dynamic_dtim = cfg_get(psoc, CFG_PMO_ENABLE_DYNAMIC_DTIM);
	wlan_pmo_get_igmp_version_support_cfg(psoc, psoc_cfg);
	psoc_cfg->sta_mod_dtim = cfg_get(psoc, CFG_PMO_ENABLE_MODULATED_DTIM);
	psoc_cfg->enable_mc_list = cfg_get(psoc, CFG_PMO_MC_ADDR_LIST_ENABLE);
	psoc_cfg->power_save_mode = cfg_get(psoc, CFG_PMO_POWERSAVE_MODE);
	psoc_cfg->sta_forced_dtim = cfg_get(psoc, CFG_PMO_ENABLE_FORCED_DTIM);
	psoc_cfg->is_mod_dtim_on_sys_suspend_enabled =
			cfg_get(psoc, CFG_PMO_MOD_DTIM_ON_SYS_SUSPEND);
	psoc_cfg->is_bus_suspend_enabled_in_sap_mode =
		cfg_get(psoc, CFG_ENABLE_BUS_SUSPEND_IN_SAP_MODE);
	psoc_cfg->is_bus_suspend_enabled_in_go_mode =
		cfg_get(psoc, CFG_ENABLE_BUS_SUSPEND_IN_GO_MODE);
	psoc_cfg->default_power_save_mode = psoc_cfg->power_save_mode;
	psoc_cfg->max_ps_poll = cfg_get(psoc, CFG_PMO_MAX_PS_POLL);

	psoc_cfg->wow_enable = cfg_get(psoc, CFG_PMO_WOW_ENABLE);

	wlan_extwow_init_cfg(psoc, psoc_cfg);
	psoc_cfg->apf_enable = cfg_get(psoc, CFG_PMO_APF_ENABLE);
	psoc_cfg->active_mode_offload = cfg_get(psoc, CFG_PMO_ACTIVE_MODE);
	wlan_pmo_wow_pulse_init_cfg(psoc, psoc_cfg);
	wlan_pmo_pkt_filter_init_cfg(psoc, psoc_cfg);
	wlan_pmo_runtime_pm_init_cfg(psoc, psoc_cfg);
	psoc_cfg->auto_power_save_fail_mode =
			cfg_get(psoc, CFG_PMO_PWR_FAILURE);
	psoc_cfg->enable_sap_suspend = cfg_get(psoc, CFG_ENABLE_SAP_SUSPEND);
	psoc_cfg->wow_data_inactivity_timeout =
			cfg_get(psoc, CFG_PMO_WOW_DATA_INACTIVITY_TIMEOUT);
	psoc_cfg->ps_data_inactivity_timeout =
			cfg_get(psoc, CFG_PS_DATA_INACTIVITY_TIMEOUT);
	psoc_cfg->active_uc_apf_mode =
			cfg_get(psoc, CFG_ACTIVE_UC_APF_MODE);
	psoc_cfg->active_mc_bc_apf_mode =
			cfg_get(psoc, CFG_ACTIVE_MC_BC_APF_MODE);
	psoc_cfg->ito_repeat_count = cfg_get(psoc, CFG_ITO_REPEAT_COUNT);
	wlan_pmo_ra_filtering_init_cfg(psoc, psoc_cfg);
	wlan_pmo_get_igmp_offload_enable_cfg(psoc, psoc_cfg);
}

QDF_STATUS pmo_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	struct pmo_psoc_priv_obj *pmo_psoc_ctx;

	if (!psoc) {
		pmo_err("null psoc");
		return QDF_STATUS_E_INVAL;
	}

	pmo_psoc_ctx = pmo_psoc_get_priv(psoc);
	wlan_pmo_init_cfg(psoc, &pmo_psoc_ctx->psoc_cfg);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

bool pmo_is_vdev_in_beaconning_mode(enum QDF_OPMODE vdev_opmode)
{
	switch (vdev_opmode) {
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_IBSS_MODE:
		return true;
	default:
		return false;
	}
}

QDF_STATUS pmo_get_vdev_bss_peer_mac_addr(struct wlan_objmgr_vdev *vdev,
		struct qdf_mac_addr *bss_peer_mac_address)
{
	struct wlan_objmgr_peer *peer;

	if (!vdev) {
		pmo_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_PMO_ID);
	if (!peer) {
		pmo_err("peer is null");
		return QDF_STATUS_E_INVAL;
	}
	wlan_peer_obj_lock(peer);
	qdf_mem_copy(bss_peer_mac_address->bytes, wlan_peer_get_macaddr(peer),
		QDF_MAC_ADDR_SIZE);
	wlan_peer_obj_unlock(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_PMO_ID);

	return QDF_STATUS_SUCCESS;
}

bool pmo_core_is_ap_mode_supports_arp_ns(struct wlan_objmgr_psoc *psoc,
	enum QDF_OPMODE vdev_opmode)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	psoc_ctx = pmo_psoc_get_priv(psoc);

	if ((vdev_opmode == QDF_SAP_MODE ||
		vdev_opmode == QDF_P2P_GO_MODE) &&
		!psoc_ctx->psoc_cfg.ap_arpns_support) {
		pmo_debug("ARP/NS Offload is not supported in SAP/P2PGO mode");
		return false;
	}

	return true;
}

bool pmo_core_is_vdev_supports_offload(struct wlan_objmgr_vdev *vdev)
{
	enum QDF_OPMODE opmode;
	bool val;

	opmode = pmo_get_vdev_opmode(vdev);
	pmo_debug("vdev opmode: %d", opmode);
	switch (opmode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_NDI_MODE:
		val = true;
		break;
	default:
		val = false;
		break;
	}

	return val;
}

QDF_STATUS pmo_core_get_psoc_config(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_enter();
	if (!psoc || !psoc_cfg) {
		pmo_err("%s is null", !psoc ? "psoc":"psoc_cfg");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		qdf_mem_copy(psoc_cfg, &psoc_ctx->psoc_cfg, sizeof(*psoc_cfg));
	}

out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_update_psoc_config(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_enter();
	if (!psoc || !psoc_cfg) {
		pmo_err("%s is null", !psoc ? "psoc":"psoc_cfg");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		qdf_mem_copy(&psoc_ctx->psoc_cfg, psoc_cfg, sizeof(*psoc_cfg));
	}

out:
	pmo_exit();

	return status;
}

void pmo_psoc_set_caps(struct wlan_objmgr_psoc *psoc,
		       struct pmo_device_caps *caps)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		qdf_mem_copy(&psoc_ctx->caps, caps, sizeof(psoc_ctx->caps));
	}
}

void pmo_core_psoc_set_hif_handle(struct wlan_objmgr_psoc *psoc,
				  void *hif_hdl)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->hif_hdl = hif_hdl;
	}
}

void *pmo_core_psoc_get_hif_handle(struct wlan_objmgr_psoc *psoc)
{
	void *hif_hdl = NULL;
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		hif_hdl = psoc_ctx->hif_hdl;
	}

	return hif_hdl;
}

void pmo_core_psoc_set_txrx_pdev_id(struct wlan_objmgr_psoc *psoc,
				    uint8_t txrx_pdev_id)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->txrx_pdev_id = txrx_pdev_id;
	}
}

uint8_t pmo_core_psoc_get_txrx_handle(struct wlan_objmgr_psoc *psoc)
{
	uint8_t txrx_pdev_id = OL_TXRX_INVALID_PDEV_ID;
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		txrx_pdev_id = psoc_ctx->txrx_pdev_id;
	}

	return txrx_pdev_id;
}
