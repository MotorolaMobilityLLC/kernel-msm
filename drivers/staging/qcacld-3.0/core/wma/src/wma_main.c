/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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
 *  DOC:  wma_main.c
 *
 *  This file contains wma initialization and FW exchange
 *  related functions.
 */

/* Header files */

#include "wma.h"
#include "wma_api.h"
#include "cds_api.h"
#include "wmi_unified_api.h"
#include "wlan_qct_sys.h"
#include "wni_api.h"
#include "ani_global.h"
#include "wmi_unified.h"
#include "wni_cfg.h"
#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif
#include "qdf_nbuf.h"
#include "qdf_types.h"
#include "qdf_mem.h"
#include "wma_types.h"
#include "lim_api.h"
#include "lim_session_utils.h"
#include "wlan_cm_tgt_if_tx_api.h"
#include "wlan_cm_roam_api.h"

#include "cds_utils.h"

#if !defined(REMOVE_PKT_LOG)
#include "pktlog_ac.h"
#endif /* REMOVE_PKT_LOG */

#include "dbglog_host.h"
#include "csr_api.h"
#include "ol_fw.h"

#include "wma_internal.h"

#include "wma_ocb.h"
#include "wlan_policy_mgr_api.h"
#include "cdp_txrx_cfg.h"
#include "cdp_txrx_flow_ctrl_legacy.h"
#include "cdp_txrx_flow_ctrl_v2.h"
#include "cdp_txrx_ipa.h"
#include "cdp_txrx_misc.h"
#include "wma_fips_api.h"
#include "wma_nan_datapath.h"
#include "wma_fw_state.h"
#include "wlan_lmac_if_def.h"
#include "wlan_lmac_if_api.h"
#include "target_if.h"
#include "target_if_scan.h"
#include "wlan_global_lmac_if_api.h"
#include "target_if_pmo.h"
#include "wma_he.h"
#include "wlan_pmo_obj_mgmt_api.h"

#include "wlan_reg_tgt_api.h"
#include "wlan_reg_services_api.h"
#include <cdp_txrx_handle.h>
#include <wlan_pmo_ucfg_api.h>
#include "wifi_pos_api.h"
#include "hif_main.h"
#ifdef WLAN_CONV_SPECTRAL_ENABLE
#include <target_if_spectral.h>
#include <wlan_spectral_utils_api.h>
#endif
#include "init_event_handler.h"
#include "init_deinit_lmac.h"
#include "target_if_green_ap.h"
#include "service_ready_param.h"
#include "wlan_cp_stats_mc_ucfg_api.h"
#include "cfg_nan_api.h"
#include "wlan_mlme_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "cfg_ucfg_api.h"
#include "init_cmd_api.h"
#include "nan_ucfg_api.h"
#include "wma_coex.h"
#include "wma_twt.h"
#include "target_if_vdev_mgr_rx_ops.h"
#include "wlan_tdls_cfg_api.h"
#include "wlan_policy_mgr_i.h"
#include "target_if_psoc_timer_tx_ops.h"
#include <ftm_time_sync_ucfg_api.h>
#include "wlan_ipa_ucfg_api.h"

#ifdef DIRECT_BUF_RX_ENABLE
#include <target_if_direct_buf_rx_api.h>
#endif

#include "wlan_pkt_capture_ucfg_api.h"
#include "wlan_fwol_ucfg_api.h"

#define WMA_LOG_COMPLETION_TIMER 3000 /* 3 seconds */
#define WMI_TLV_HEADROOM 128

#define WMA_FW_TIME_SYNC_TIMER 60000 /* 1 min */

static uint32_t g_fw_wlan_feat_caps;
/**
 * wma_get_fw_wlan_feat_caps() - get fw feature capablity
 * @feature: feature enum value
 *
 * Return: true/false
 */
bool wma_get_fw_wlan_feat_caps(enum cap_bitmap feature)
{
	return (g_fw_wlan_feat_caps & (1 << feature)) ? true : false;
}

/**
 * wma_set_fw_wlan_feat_caps() - set fw feature capablity
 * @feature: feature enum value
 *
 * Return: None
 */
void wma_set_fw_wlan_feat_caps(enum cap_bitmap feature)
{
	g_fw_wlan_feat_caps |= (1 << feature);
}

/**
 * wma_service_ready_ext_evt_timeout() - Service ready extended event timeout
 * @data: Timeout handler data
 *
 * This function is called when the FW fails to send WMI_SERVICE_READY_EXT_EVENT
 * message
 *
 * Return: None
 */
static void wma_service_ready_ext_evt_timeout(void *data)
{
	tp_wma_handle wma_handle;

	wma_alert("Timeout waiting for WMI_SERVICE_READY_EXT_EVENT");

	wma_handle = (tp_wma_handle) data;

	if (!wma_handle) {
		wma_err("Invalid WMA handle");
		goto end;
	}

end:
	/* Assert here. Panic is being called in insmod thread */
	QDF_ASSERT(0);
}

/**
 * wma_get_ini_handle() - API to get WMA ini info handle
 * @wma: WMA Handle
 *
 * Returns the pointer to WMA ini structure.
 * Return: struct wma_ini_config
 */
struct wma_ini_config *wma_get_ini_handle(tp_wma_handle wma)
{
	if (!wma) {
		wma_err("Invalid WMA context");
		return NULL;
	}

	return &wma->ini_config;
}

int __wma_validate_handle(tp_wma_handle wma_handle, const char *func)
{
	if (!wma_handle) {
		wma_err("Invalid WMA handle (via %s)", func);
		return -EINVAL;
	}

	return 0;
}

#define MAX_SUPPORTED_PEERS_REV1_1 14
#define MAX_SUPPORTED_PEERS_REV1_3 32
#ifdef WLAN_MAX_CLIENTS_ALLOWED
#define MAX_SUPPORTED_PEERS WLAN_MAX_CLIENTS_ALLOWED
#else
#define MAX_SUPPORTED_PEERS 32
#endif
#define MIN_NO_OF_PEERS 1

/**
 * wma_get_number_of_peers_supported - API to query for number of peers
 * supported
 * @wma: WMA Handle
 *
 * Return: Max Number of Peers Supported
 */
static uint8_t wma_get_number_of_peers_supported(tp_wma_handle wma)
{
	struct wma_ini_config *cfg = wma_get_ini_handle(wma);
	uint8_t max_no_of_peers = cfg ? cfg->max_no_of_peers : MIN_NO_OF_PEERS;

	return max_no_of_peers;
}

/**
 * wma_get_number_of_tids_supported - API to query for number of tids supported
 * @no_of_peers_supported: Number of peer supported
 *
 * Return: Max number of tids supported
 */
#if defined(CONFIG_HL_SUPPORT)
static uint32_t wma_get_number_of_tids_supported(uint8_t no_of_peers_supported,
						 uint8_t num_vdevs)
{
	return 4 * no_of_peers_supported;
}
#else
static uint32_t wma_get_number_of_tids_supported(uint8_t no_of_peers_supported,
						 uint8_t num_vdevs)
{
	return 2 * (no_of_peers_supported + num_vdevs + 2);
}
#endif

#if (defined(IPA_DISABLE_OVERRIDE)) && (!defined(IPA_OFFLOAD))
static void wma_set_ipa_disable_config(
					target_resource_config *tgt_cfg)
{
	tgt_cfg->ipa_disable = true;
}
#else
static void wma_set_ipa_disable_config(
					target_resource_config *tgt_cfg)
{
	tgt_cfg->ipa_disable = ucfg_ipa_is_enabled() ? false : true;
}
#endif

#ifndef NUM_OF_ADDITIONAL_FW_PEERS
#define NUM_OF_ADDITIONAL_FW_PEERS	2
#endif

/**
 * wma_update_num_peers_tids() - Update num_peers and tids based on num_vdevs
 * @wma_handle: wma handle
 * @tgt_cfg: Resource config given to target
 *
 * Get num_vdevs from tgt_cfg and update num_peers and tids based on it.
 *
 * Return: none
 */
static void wma_update_num_peers_tids(t_wma_handle *wma_handle,
				      target_resource_config *tgt_cfg)

{
	uint8_t no_of_peers_supported;

	no_of_peers_supported = wma_get_number_of_peers_supported(wma_handle);

	tgt_cfg->num_peers = no_of_peers_supported + tgt_cfg->num_vdevs +
				NUM_OF_ADDITIONAL_FW_PEERS;
	/* The current firmware implementation requires the number of
	 * offload peers should be (number of vdevs + 1).
	 */
	tgt_cfg->num_tids =
		wma_get_number_of_tids_supported(no_of_peers_supported,
						 tgt_cfg->num_vdevs);
}

/**
 * wma_set_default_tgt_config() - set default tgt config
 * @wma_handle: wma handle
 * @tgt_cfg: Resource config given to target
 *
 * Return: none
 */
static void wma_set_default_tgt_config(tp_wma_handle wma_handle,
				       target_resource_config *tgt_cfg,
				       struct cds_config_info *cds_cfg)
{
	qdf_mem_zero(tgt_cfg, sizeof(target_resource_config));

	tgt_cfg->num_vdevs = cds_cfg->num_vdevs;
	wma_update_num_peers_tids(wma_handle, tgt_cfg);

	/* The current firmware implementation requires the number of
	 * offload peers should be (number of vdevs + 1).
	 */
	tgt_cfg->num_offload_peers = cds_cfg->ap_maxoffload_peers + 1;
	tgt_cfg->num_offload_reorder_buffs =
				cds_cfg->ap_maxoffload_reorderbuffs + 1;
	tgt_cfg->num_peer_keys = CFG_TGT_NUM_PEER_KEYS;
	tgt_cfg->ast_skid_limit = CFG_TGT_AST_SKID_LIMIT;
	tgt_cfg->tx_chain_mask = CFG_TGT_DEFAULT_TX_CHAIN_MASK;
	tgt_cfg->rx_chain_mask = CFG_TGT_DEFAULT_RX_CHAIN_MASK;
	tgt_cfg->rx_timeout_pri[0] = CFG_TGT_RX_TIMEOUT_LO_PRI;
	tgt_cfg->rx_timeout_pri[1] = CFG_TGT_RX_TIMEOUT_LO_PRI;
	tgt_cfg->rx_timeout_pri[2] = CFG_TGT_RX_TIMEOUT_LO_PRI;
	tgt_cfg->rx_timeout_pri[3] = CFG_TGT_RX_TIMEOUT_HI_PRI;
	tgt_cfg->rx_decap_mode = CFG_TGT_RX_DECAP_MODE;
	tgt_cfg->scan_max_pending_req = WLAN_MAX_ACTIVE_SCANS_ALLOWED;
	tgt_cfg->bmiss_offload_max_vdev =
			CFG_TGT_DEFAULT_BMISS_OFFLOAD_MAX_VDEV;
	tgt_cfg->roam_offload_max_vdev = CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_VDEV;
	tgt_cfg->roam_offload_max_ap_profiles =
		CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_PROFILES;
	tgt_cfg->num_mcast_groups = CFG_TGT_DEFAULT_NUM_MCAST_GROUPS;
	tgt_cfg->num_mcast_table_elems = CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS;
	tgt_cfg->mcast2ucast_mode = CFG_TGT_DEFAULT_MCAST2UCAST_MODE;
	tgt_cfg->tx_dbg_log_size = CFG_TGT_DEFAULT_TX_DBG_LOG_SIZE;
	tgt_cfg->num_wds_entries = CFG_TGT_WDS_ENTRIES;
	tgt_cfg->dma_burst_size = CFG_TGT_DEFAULT_DMA_BURST_SIZE;
	tgt_cfg->mac_aggr_delim = CFG_TGT_DEFAULT_MAC_AGGR_DELIM;
	tgt_cfg->rx_skip_defrag_timeout_dup_detection_check =
		CFG_TGT_DEFAULT_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK,
	tgt_cfg->vow_config = CFG_TGT_DEFAULT_VOW_CONFIG;
	tgt_cfg->gtk_offload_max_vdev = CFG_TGT_DEFAULT_GTK_OFFLOAD_MAX_VDEV;
	tgt_cfg->num_msdu_desc = CFG_TGT_NUM_MSDU_DESC;
	tgt_cfg->max_frag_entries = CFG_TGT_MAX_FRAG_TABLE_ENTRIES;
	tgt_cfg->num_tdls_vdevs = CFG_TGT_NUM_TDLS_VDEVS;
	tgt_cfg->num_tdls_conn_table_entries =
			QDF_MIN(CFG_TGT_NUM_TDLS_CONN_TABLE_ENTRIES,
				cfg_tdls_get_max_peer_count(wma_handle->psoc));
	tgt_cfg->beacon_tx_offload_max_vdev =
		CFG_TGT_DEFAULT_BEACON_TX_OFFLOAD_MAX_VDEV;
	tgt_cfg->num_multicast_filter_entries =
		CFG_TGT_MAX_MULTICAST_FILTER_ENTRIES;
	tgt_cfg->num_wow_filters = 0;
	tgt_cfg->num_keep_alive_pattern = MAXNUM_PERIODIC_TX_PTRNS;
	tgt_cfg->num_max_sta_vdevs = CFG_TGT_DEFAULT_MAX_STA_VDEVS;
	tgt_cfg->keep_alive_pattern_size = 0;
	tgt_cfg->max_tdls_concurrent_sleep_sta =
		CFG_TGT_NUM_TDLS_CONC_SLEEP_STAS;
	tgt_cfg->max_tdls_concurrent_buffer_sta =
		CFG_TGT_NUM_TDLS_CONC_BUFFER_STAS;
	tgt_cfg->wmi_send_separate = 0;
	tgt_cfg->num_ocb_vdevs = CFG_TGT_NUM_OCB_VDEVS;
	tgt_cfg->num_ocb_channels = CFG_TGT_NUM_OCB_CHANNELS;
	tgt_cfg->num_ocb_schedules = CFG_TGT_NUM_OCB_SCHEDULES;
	tgt_cfg->twt_ap_sta_count = CFG_TGT_DEFAULT_TWT_AP_STA_COUNT;
	tgt_cfg->enable_pci_gen = cfg_get(wma_handle->psoc, CFG_ENABLE_PCI_GEN);

	tgt_cfg->mgmt_comp_evt_bundle_support = true;
	tgt_cfg->tx_msdu_new_partition_id_support = true;
	tgt_cfg->is_sap_connected_d3wow_enabled =
		ucfg_pmo_get_sap_mode_bus_suspend(wma_handle->psoc);
	tgt_cfg->is_go_connected_d3wow_enabled =
		ucfg_pmo_get_go_mode_bus_suspend(wma_handle->psoc);

	cfg_nan_get_max_ndi(wma_handle->psoc,
			    &tgt_cfg->max_ndi);

	if (cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE)
		tgt_cfg->rx_decap_mode = CFG_TGT_RX_DECAP_MODE_RAW;

	cfg_nan_get_ndp_max_sessions(wma_handle->psoc,
				     &tgt_cfg->max_ndp_sessions);

	wma_set_ipa_disable_config(tgt_cfg);
}

/**
 * wma_cli_get_command() - WMA "get" command processor
 * @vdev_id: virtual device for the command
 * @param_id: parameter id
 * @vpdev: parameter category
 *
 * Return: parameter value on success, -EINVAL on failure
 */
int wma_cli_get_command(int vdev_id, int param_id, int vpdev)
{
	int ret = 0;
	tp_wma_handle wma;
	struct wma_txrx_node *intr = NULL;

	wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err("Invalid wma handle");
		return -EINVAL;
	}

	intr = wma->interfaces;

	if (VDEV_CMD == vpdev) {
		switch (param_id) {
		case WMI_VDEV_PARAM_NSS:
			ret = intr[vdev_id].config.nss;
			break;
#ifdef QCA_SUPPORT_GTX
		case WMI_VDEV_PARAM_GTX_HT_MCS:
			ret = intr[vdev_id].config.gtx_info.gtxRTMask[0];
			break;
		case WMI_VDEV_PARAM_GTX_VHT_MCS:
			ret = intr[vdev_id].config.gtx_info.gtxRTMask[1];
			break;
		case WMI_VDEV_PARAM_GTX_USR_CFG:
			ret = intr[vdev_id].config.gtx_info.gtxUsrcfg;
			break;
		case WMI_VDEV_PARAM_GTX_THRE:
			ret = intr[vdev_id].config.gtx_info.gtxPERThreshold;
			break;
		case WMI_VDEV_PARAM_GTX_MARGIN:
			ret = intr[vdev_id].config.gtx_info.gtxPERMargin;
			break;
		case WMI_VDEV_PARAM_GTX_STEP:
			ret = intr[vdev_id].config.gtx_info.gtxTPCstep;
			break;
		case WMI_VDEV_PARAM_GTX_MINTPC:
			ret = intr[vdev_id].config.gtx_info.gtxTPCMin;
			break;
		case WMI_VDEV_PARAM_GTX_BW_MASK:
			ret = intr[vdev_id].config.gtx_info.gtxBWMask;
			break;
#endif /* QCA_SUPPORT_GTX */
		case WMI_VDEV_PARAM_LDPC:
			ret = intr[vdev_id].config.ldpc;
			break;
		case WMI_VDEV_PARAM_TX_STBC:
			ret = intr[vdev_id].config.tx_stbc;
			break;
		case WMI_VDEV_PARAM_RX_STBC:
			ret = intr[vdev_id].config.rx_stbc;
			break;
		case WMI_VDEV_PARAM_SGI:
			ret = intr[vdev_id].config.shortgi;
			break;
		case WMI_VDEV_PARAM_ENABLE_RTSCTS:
			ret = intr[vdev_id].config.rtscts_en;
			break;
		case WMI_VDEV_PARAM_CHWIDTH:
			ret = intr[vdev_id].config.chwidth;
			break;
		case WMI_VDEV_PARAM_FIXED_RATE:
			ret = intr[vdev_id].config.tx_rate;
			break;
		case WMI_VDEV_PARAM_HE_DCM:
		case WMI_VDEV_PARAM_HE_RANGE_EXT:
			ret = wma_get_he_vdev_param(&intr[vdev_id], param_id);
			break;
		default:
			wma_err("Invalid cli_get vdev command/Not yet implemented 0x%x",
				param_id);
			return -EINVAL;
		}
	} else if (PDEV_CMD == vpdev) {
		switch (param_id) {
		case WMI_PDEV_PARAM_ANI_ENABLE:
			ret = wma->pdevconfig.ani_enable;
			break;
		case WMI_PDEV_PARAM_ANI_POLL_PERIOD:
			ret = wma->pdevconfig.ani_poll_len;
			break;
		case WMI_PDEV_PARAM_ANI_LISTEN_PERIOD:
			ret = wma->pdevconfig.ani_listen_len;
			break;
		case WMI_PDEV_PARAM_ANI_OFDM_LEVEL:
			ret = wma->pdevconfig.ani_ofdm_level;
			break;
		case WMI_PDEV_PARAM_ANI_CCK_LEVEL:
			ret = wma->pdevconfig.ani_cck_level;
			break;
		case WMI_PDEV_PARAM_DYNAMIC_BW:
			ret = wma->pdevconfig.cwmenable;
			break;
		case WMI_PDEV_PARAM_CTS_CBW:
			ret = wma->pdevconfig.cts_cbw;
			break;
		case WMI_PDEV_PARAM_TX_CHAIN_MASK:
			ret = wma->pdevconfig.txchainmask;
			break;
		case WMI_PDEV_PARAM_RX_CHAIN_MASK:
			ret = wma->pdevconfig.rxchainmask;
			break;
		case WMI_PDEV_PARAM_TXPOWER_LIMIT2G:
			ret = wma->pdevconfig.txpow2g;
			break;
		case WMI_PDEV_PARAM_TXPOWER_LIMIT5G:
			ret = wma->pdevconfig.txpow5g;
			break;
		default:
			wma_err("Invalid cli_get pdev command/Not yet implemented 0x%x",
				param_id);
			return -EINVAL;
		}
	} else if (GEN_CMD == vpdev) {
		switch (param_id) {
		case GEN_VDEV_PARAM_AMPDU:
			ret = intr[vdev_id].config.ampdu;
			break;
		case GEN_VDEV_PARAM_AMSDU:
			ret = intr[vdev_id].config.amsdu;
			break;
		case GEN_VDEV_ROAM_SYNCH_DELAY:
			ret = intr[vdev_id].roam_synch_delay;
			break;
		case GEN_VDEV_PARAM_TX_AMPDU:
			ret = intr[vdev_id].config.tx_ampdu;
			break;
		case GEN_VDEV_PARAM_RX_AMPDU:
			ret = intr[vdev_id].config.rx_ampdu;
			break;
		case GEN_VDEV_PARAM_TX_AMSDU:
			ret = intr[vdev_id].config.tx_amsdu;
			break;
		case GEN_VDEV_PARAM_RX_AMSDU:
			ret = intr[vdev_id].config.rx_amsdu;
			break;
		default:
			wma_warn("Invalid generic vdev command/Not yet implemented 0x%x",
				 param_id);
			return -EINVAL;
		}
	} else if (PPS_CMD == vpdev) {
		switch (param_id) {
		case WMI_VDEV_PPS_PAID_MATCH:
			ret = intr[vdev_id].config.pps_params.paid_match_enable;
			break;
		case WMI_VDEV_PPS_GID_MATCH:
			ret = intr[vdev_id].config.pps_params.gid_match_enable;
			break;
		case WMI_VDEV_PPS_EARLY_TIM_CLEAR:
			ret = intr[vdev_id].config.pps_params.tim_clear;
			break;
		case WMI_VDEV_PPS_EARLY_DTIM_CLEAR:
			ret = intr[vdev_id].config.pps_params.dtim_clear;
			break;
		case WMI_VDEV_PPS_EOF_PAD_DELIM:
			ret = intr[vdev_id].config.pps_params.eof_delim;
			break;
		case WMI_VDEV_PPS_MACADDR_MISMATCH:
			ret = intr[vdev_id].config.pps_params.mac_match;
			break;
		case WMI_VDEV_PPS_DELIM_CRC_FAIL:
			ret = intr[vdev_id].config.pps_params.delim_fail;
			break;
		case WMI_VDEV_PPS_GID_NSTS_ZERO:
			ret = intr[vdev_id].config.pps_params.nsts_zero;
			break;
		case WMI_VDEV_PPS_RSSI_CHECK:
			ret = intr[vdev_id].config.pps_params.rssi_chk;
			break;
		default:
			wma_err("Invalid pps vdev command/Not yet implemented 0x%x",
				param_id);
			return -EINVAL;
		}
	} else if (QPOWER_CMD == vpdev) {
		switch (param_id) {
		case WMI_STA_PS_PARAM_QPOWER_PSPOLL_COUNT:
			ret = intr[vdev_id].config.qpower_params.
			      max_ps_poll_cnt;
			break;
		case WMI_STA_PS_PARAM_QPOWER_MAX_TX_BEFORE_WAKE:
			ret = intr[vdev_id].config.qpower_params.
			      max_tx_before_wake;
			break;
		case WMI_STA_PS_PARAM_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL:
			ret = intr[vdev_id].config.qpower_params.
			      spec_ps_poll_wake_interval;
			break;
		case WMI_STA_PS_PARAM_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL:
			ret = intr[vdev_id].config.qpower_params.
			      max_spec_nodata_ps_poll;
			break;
		default:
			wma_warn("Invalid generic vdev command/Not yet implemented 0x%x",
				 param_id);
			return -EINVAL;
		}
	} else if (GTX_CMD == vpdev) {
		switch (param_id) {
		case WMI_VDEV_PARAM_GTX_HT_MCS:
			ret = intr[vdev_id].config.gtx_info.gtxRTMask[0];
			break;
		case WMI_VDEV_PARAM_GTX_VHT_MCS:
			ret = intr[vdev_id].config.gtx_info.gtxRTMask[1];
			break;
		case WMI_VDEV_PARAM_GTX_USR_CFG:
			ret = intr[vdev_id].config.gtx_info.gtxUsrcfg;
			break;
		case WMI_VDEV_PARAM_GTX_THRE:
			ret = intr[vdev_id].config.gtx_info.gtxPERThreshold;
			break;
		case WMI_VDEV_PARAM_GTX_MARGIN:
			ret = intr[vdev_id].config.gtx_info.gtxPERMargin;
			break;
		case WMI_VDEV_PARAM_GTX_STEP:
			ret = intr[vdev_id].config.gtx_info.gtxTPCstep;
			break;
		case WMI_VDEV_PARAM_GTX_MINTPC:
			ret = intr[vdev_id].config.gtx_info.gtxTPCMin;
			break;
		case WMI_VDEV_PARAM_GTX_BW_MASK:
			ret = intr[vdev_id].config.gtx_info.gtxBWMask;
			break;
		default:
			wma_warn("Invalid generic vdev command/Not yet implemented 0x%x",
				 param_id);
			return -EINVAL;
		}
	}
	return ret;
}

/**
 * wma_cli_set2_command() - WMA "set 2 params" command processor
 * @vdev_id: virtual device for the command
 * @param_id: parameter id
 * @sval1: first parameter value
 * @sval2: second parameter value
 * @vpdev: parameter category
 *
 * Command handler for set operations which require 2 parameters
 *
 * Return: 0 on success, errno on failure
 */
int wma_cli_set2_command(int vdev_id, int param_id, int sval1,
			 int sval2, int vpdev)
{
	struct scheduler_msg msg = { 0 };
	wma_cli_set_cmd_t *iwcmd;

	iwcmd = qdf_mem_malloc(sizeof(*iwcmd));
	if (!iwcmd)
		return -ENOMEM;

	qdf_mem_zero(iwcmd, sizeof(*iwcmd));
	iwcmd->param_value = sval1;
	iwcmd->param_sec_value = sval2;
	iwcmd->param_vdev_id = vdev_id;
	iwcmd->param_id = param_id;
	iwcmd->param_vp_dev = vpdev;
	msg.type = WMA_CLI_SET_CMD;
	msg.reserved = 0;
	msg.bodyptr = iwcmd;

	if (QDF_STATUS_SUCCESS !=
	    scheduler_post_message(QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_WMA, &msg)) {
		qdf_mem_free(iwcmd);
		return -EIO;
	}
	return 0;
}

/**
 * wma_cli_set_command() - WMA "set" command processor
 * @vdev_id: virtual device for the command
 * @param_id: parameter id
 * @sval: parameter value
 * @vpdev: parameter category
 *
 * Command handler for set operations
 *
 * Return: 0 on success, errno on failure
 */
int wma_cli_set_command(int vdev_id, int param_id, int sval, int vpdev)
{
	return wma_cli_set2_command(vdev_id, param_id, sval, 0, vpdev);

}

QDF_STATUS wma_form_unit_test_cmd_and_send(uint32_t vdev_id,
			uint32_t module_id, uint32_t arg_count, uint32_t *arg)
{
	struct wmi_unit_test_cmd *unit_test_args;
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	uint32_t i;
	QDF_STATUS status;

	wma_debug("enter");

	if (QDF_GLOBAL_FTM_MODE != cds_get_conparam()) {
		if (!wma_is_vdev_valid(vdev_id))
			return QDF_STATUS_E_FAILURE;
	}

	if (arg_count > WMA_MAX_NUM_ARGS) {
		wma_err("arg_count is crossed the boundary");
		return QDF_STATUS_E_FAILURE;
	}
	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("Invalid WMA/WMI handle");
		return QDF_STATUS_E_FAILURE;
	}
	unit_test_args = qdf_mem_malloc(sizeof(*unit_test_args));
	if (!unit_test_args)
		return QDF_STATUS_E_NOMEM;

	unit_test_args->vdev_id = vdev_id;
	unit_test_args->module_id = module_id;
	unit_test_args->num_args = arg_count;
	for (i = 0; i < arg_count; i++)
		unit_test_args->args[i] = arg[i];

	status = wmi_unified_unit_test_cmd(wma_handle->wmi_handle,
					   unit_test_args);
	qdf_mem_free(unit_test_args);
	wma_debug("exit");

	return status;
}

static void wma_process_send_addba_req(tp_wma_handle wma_handle,
		struct send_add_ba_req *send_addba)
{
	QDF_STATUS status;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("Invalid WMA/WMI handle");
		qdf_mem_free(send_addba);
		return;
	}

	status = wmi_unified_addba_send_cmd_send(wma_handle->wmi_handle,
					   send_addba->mac_addr,
					   &send_addba->param);
	if (QDF_STATUS_SUCCESS != status) {
		wma_err("Failed to process WMA_SEND_ADDBA_REQ");
	}
	wma_debug("sent ADDBA req to" QDF_MAC_ADDR_FMT "tid %d buff_size %d",
			QDF_MAC_ADDR_REF(send_addba->mac_addr),
			send_addba->param.tidno,
			send_addba->param.buffersize);

	qdf_mem_free(send_addba);
}

/**
 * wma_set_priv_cfg() - set private config parameters
 * @wma_handle: wma handle
 * @privcmd: private command
 *
 * Return: 0 for success or error code
 */
static int32_t wma_set_priv_cfg(tp_wma_handle wma_handle,
				wma_cli_set_cmd_t *privcmd)
{
	int32_t ret = 0;

	switch (privcmd->param_id) {
	case WMA_VDEV_TXRX_FWSTATS_ENABLE_CMDID:
		ret = wma_set_txrx_fw_stats_level(wma_handle,
						  privcmd->param_vdev_id,
						  privcmd->param_value);
		break;
	case WMA_VDEV_TXRX_FWSTATS_RESET_CMDID:
		ret = wma_txrx_fw_stats_reset(wma_handle,
					      privcmd->param_vdev_id,
					      privcmd->param_value);
		break;
	case WMI_STA_SMPS_FORCE_MODE_CMDID:
		ret = wma_set_mimops(wma_handle,
				     privcmd->param_vdev_id,
				     privcmd->param_value);
		break;
	case WMI_STA_SMPS_PARAM_CMDID:
		wma_set_smps_params(wma_handle, privcmd->param_vdev_id,
				    privcmd->param_value);
		break;
	case WMA_VDEV_MCC_SET_TIME_LATENCY:
	{
		/* Extract first MCC adapter/vdev channel number and latency */
		uint8_t mcc_channel = privcmd->param_value & 0x000000FF;
		uint8_t mcc_channel_latency =
			(privcmd->param_value & 0x0000FF00) >> 8;
		int ret = -1;

		wma_debug("Parsed input: Channel #1:%d, latency:%dms",
			 mcc_channel, mcc_channel_latency);
		ret = wma_set_mcc_channel_time_latency(wma_handle,
						       mcc_channel,
						       mcc_channel_latency);
	}
		break;
	case WMA_VDEV_MCC_SET_TIME_QUOTA:
	{
		/* Extract the MCC 2 adapters/vdevs channel numbers and time
		 * quota value for the first adapter only (which is specified
		 * in iwpriv command.
		 */
		uint8_t adapter_2_chan_number =
			privcmd->param_value & 0x000000FF;
		uint8_t adapter_1_chan_number =
			(privcmd->param_value & 0x0000FF00) >> 8;
		uint8_t adapter_1_quota =
			(privcmd->param_value & 0x00FF0000) >> 16;
		int ret = -1;

		wma_debug("Parsed input: Channel #1:%d, Channel #2:%d, quota 1:%dms",
			  adapter_1_chan_number,
			  adapter_2_chan_number, adapter_1_quota);

		ret = wma_set_mcc_channel_time_quota(wma_handle,
						     adapter_1_chan_number,
						     adapter_1_quota,
						     adapter_2_chan_number);
	}
		break;
	default:
		wma_err("Invalid wma config command id:%d", privcmd->param_id);
		ret = -EINVAL;
	}
	return ret;
}

/**
 * wma_set_dtim_period() - set dtim period to FW
 * @wma: wma handle
 * @dtim_params: dtim params
 *
 * Return: none
 */
static void wma_set_dtim_period(tp_wma_handle wma,
				struct set_dtim_params *dtim_params)
{
	struct wma_txrx_node *iface =
		&wma->interfaces[dtim_params->session_id];
	if (!wma_is_vdev_valid(dtim_params->session_id)) {
		wma_err("invalid VDEV");
		return;
	}
	wma_debug("set dtim_period %d", dtim_params->dtim_period);
	iface->dtimPeriod = dtim_params->dtim_period;

}

static inline bool wma_is_tx_chainmask_valid(int value,
					     struct target_psoc_info *tgt_hdl)
{
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap;
	uint8_t total_mac_phy_cnt, i;

	mac_phy_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
	if (!mac_phy_cap) {
		wma_err("Invalid MAC PHY capabilities handle");
		return false;
	}

	total_mac_phy_cnt = target_psoc_get_total_mac_phy_cnt(tgt_hdl);
	for (i = 0; i < total_mac_phy_cnt; i++) {
		if (((mac_phy_cap[i].tx_chain_mask_5G) & (value))) {
			return true;
		}
	}

	return false;
}

/**
 * wma_process_cli_set_cmd() - set parameters to fw
 * @wma: wma handle
 * @privcmd: command
 *
 * Return: none
 */
static void wma_process_cli_set_cmd(tp_wma_handle wma,
				    wma_cli_set_cmd_t *privcmd)
{
	int vid = privcmd->param_vdev_id, pps_val = 0;
	QDF_STATUS ret;
	struct wma_txrx_node *intr = wma->interfaces;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	struct qpower_params *qparams = &intr[vid].config.qpower_params;
	struct pdev_params pdev_param = {0};
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct target_psoc_info *tgt_hdl;

	if (!mac) {
		wma_err("Failed to get mac");
		return;
	}

	tgt_hdl = wlan_psoc_get_tgt_if_handle(wma->psoc);
	if (!tgt_hdl) {
		wma_err("target psoc info is NULL");
		return;
	}

	if (privcmd->param_id >= WMI_CMDID_MAX) {
		/*
		 * This configuration setting is not done using any wmi
		 * command, call appropriate handler.
		 */
		if (wma_set_priv_cfg(wma, privcmd))
			wma_err("Failed to set wma priv configuration");
		return;
	}

	switch (privcmd->param_vp_dev) {
	case VDEV_CMD:
		if (!wma_is_vdev_valid(privcmd->param_vdev_id)) {
			wma_err("Vdev id is not valid");
			return;
		}

		wma_debug("vdev id %d pid %d pval %d", privcmd->param_vdev_id,
			 privcmd->param_id, privcmd->param_value);
		ret = wma_vdev_set_param(wma->wmi_handle,
						      privcmd->param_vdev_id,
						      privcmd->param_id,
						      privcmd->param_value);
		if (QDF_IS_STATUS_ERROR(ret)) {
			wma_err("wma_vdev_set_param failed ret %d", ret);
			return;
		}
		break;
	case PDEV_CMD:
		wma_debug("pdev pid %d pval %d", privcmd->param_id,
			 privcmd->param_value);
		if ((privcmd->param_id == WMI_PDEV_PARAM_RX_CHAIN_MASK) ||
		    (privcmd->param_id == WMI_PDEV_PARAM_TX_CHAIN_MASK)) {
			if (QDF_STATUS_SUCCESS !=
					wma_check_txrx_chainmask(
					target_if_get_num_rf_chains(tgt_hdl),
					privcmd->param_value)) {
				wma_debug("Chainmask value is invalid");
				return;
			}
		}

		if (privcmd->param_id == WMI_PDEV_PARAM_TX_CHAIN_MASK) {
			if (!wma_is_tx_chainmask_valid(privcmd->param_value,
						       tgt_hdl)) {
				wma_debug("Chainmask value is invalid");
				return;
			}
		}
		pdev_param.param_id = privcmd->param_id;
		pdev_param.param_value = privcmd->param_value;
		ret = wmi_unified_pdev_param_send(wma->wmi_handle,
						 &pdev_param,
						 privcmd->param_sec_value);
		if (QDF_IS_STATUS_ERROR(ret)) {
			wma_err("wma_vdev_set_param failed ret %d", ret);
			return;
		}
		break;
	case GEN_CMD:
	{
		struct wma_txrx_node *intr = wma->interfaces;
		wmi_vdev_custom_aggr_type_t aggr_type =
			WMI_VDEV_CUSTOM_AGGR_TYPE_AMSDU;

		wma_debug("gen pid %d pval %d", privcmd->param_id,
			 privcmd->param_value);

		switch (privcmd->param_id) {
		case GEN_VDEV_PARAM_AMSDU:
		case GEN_VDEV_PARAM_AMPDU:
			if (!soc) {
				wma_err("SOC context is NULL");
				return;
			}

			if (privcmd->param_id == GEN_VDEV_PARAM_AMPDU) {
				ret = cdp_aggr_cfg(soc, privcmd->param_vdev_id,
						   privcmd->param_value, 0);
				if (ret)
					wma_err("cdp_aggr_cfg set ampdu failed ret %d",
						ret);
				else
					intr[privcmd->param_vdev_id].config.
						ampdu = privcmd->param_value;

				aggr_type =
					WMI_VDEV_CUSTOM_AGGR_TYPE_AMPDU;
			}

			ret = wma_set_tx_rx_aggr_size(vid,
						      privcmd->param_value,
						      privcmd->param_value,
						      aggr_type);
			if (QDF_IS_STATUS_ERROR(ret)) {
				wma_err("set_aggr_size failed ret %d", ret);
				return;
			}
			break;
		case GEN_PARAM_CRASH_INJECT:
			if (QDF_GLOBAL_FTM_MODE  == cds_get_conparam())
				wma_err("Crash inject not allowed in FTM mode");
			else
				ret = wma_crash_inject(wma,
						privcmd->param_value,
						privcmd->param_sec_value);
			break;
		case GEN_PARAM_CAPTURE_TSF:
			ret = wma_capture_tsf(wma, privcmd->param_value);
			break;
		case GEN_PARAM_RESET_TSF_GPIO:
			ret = wma_reset_tsf_gpio(wma, privcmd->param_value);
			break;
		default:
			wma_err("Invalid param id 0x%x", privcmd->param_id);
			break;
		}
		break;
	}
	case DBG_CMD:
		wma_debug("dbg pid %d pval %d", privcmd->param_id,
			 privcmd->param_value);
		switch (privcmd->param_id) {
		case WMI_DBGLOG_LOG_LEVEL:
			ret = dbglog_set_log_lvl(wma->wmi_handle,
						   privcmd->param_value);
			if (ret)
				wma_err("dbglog_set_log_lvl failed ret %d",
					 ret);
			break;
		case WMI_DBGLOG_VAP_ENABLE:
			ret = dbglog_vap_log_enable(wma->wmi_handle,
						    privcmd->param_value, true);
			if (ret)
				wma_err("dbglog_vap_log_enable failed ret %d",
					 ret);
			break;
		case WMI_DBGLOG_VAP_DISABLE:
			ret = dbglog_vap_log_enable(wma->wmi_handle,
						privcmd->param_value, false);
			if (ret)
				wma_err("dbglog_vap_log_enable failed ret %d",
					 ret);
			break;
		case WMI_DBGLOG_MODULE_ENABLE:
			ret = dbglog_module_log_enable(wma->wmi_handle,
						privcmd->param_value, true);
			if (ret)
				wma_err("dbglog_module_log_enable failed ret %d",
					ret);
			break;
		case WMI_DBGLOG_MODULE_DISABLE:
			ret = dbglog_module_log_enable(wma->wmi_handle,
						privcmd->param_value, false);
			if (ret)
				wma_err("dbglog_module_log_enable failed ret %d",
					ret);
			break;
		case WMI_DBGLOG_MOD_LOG_LEVEL:
			ret = dbglog_set_mod_log_lvl(wma->wmi_handle,
						       privcmd->param_value);
			if (ret)
				wma_err("dbglog_module_log_enable failed ret %d",
					ret);
			break;
		case WMI_DBGLOG_MOD_WOW_LOG_LEVEL:
			ret = dbglog_set_mod_wow_log_lvl(wma->wmi_handle,
							 privcmd->param_value);
			if (ret)
				wma_err("WMI_DBGLOG_MOD_WOW_LOG_LEVEL failed ret %d",
					ret);
			break;
		case WMI_DBGLOG_TYPE:
			ret = dbglog_parser_type_init(wma->wmi_handle,
							privcmd->param_value);
			if (ret)
				wma_err("dbglog_parser_type_init failed ret %d",
					ret);
			break;
		case WMI_DBGLOG_REPORT_ENABLE:
			ret = dbglog_report_enable(wma->wmi_handle,
						     privcmd->param_value);
			if (ret)
				wma_err("dbglog_report_enable failed ret %d",
					 ret);
			break;
		case WMI_WLAN_PROFILE_TRIGGER_CMDID:
			ret = wma_unified_fw_profiling_cmd(wma->wmi_handle,
					 WMI_WLAN_PROFILE_TRIGGER_CMDID,
					 privcmd->param_value, 0);
			if (ret)
				wma_err("Profile cmd failed for %d ret %d",
					WMI_WLAN_PROFILE_TRIGGER_CMDID, ret);
			break;
		case WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID:
			ret = wma_unified_fw_profiling_cmd(wma->wmi_handle,
				  WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID,
				  privcmd->param_value,
				  privcmd->param_sec_value);
			if (ret)
				wma_err("Profile cmd failed for %d ret %d",
					WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID,
					ret);
			break;
		case WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID:
			ret = wma_unified_fw_profiling_cmd(wma->wmi_handle,
					 WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID,
					 privcmd->param_value,
					 privcmd->param_sec_value);
			if (ret)
				wma_err("Profile cmd failed for %d ret %d",
					WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID,
					ret);
			break;
		case WMI_WLAN_PROFILE_LIST_PROFILE_ID_CMDID:
			ret = wma_unified_fw_profiling_cmd(wma->wmi_handle,
					 WMI_WLAN_PROFILE_LIST_PROFILE_ID_CMDID,
					 0, 0);
			if (ret)
				wma_err("Profile cmd failed for %d ret %d",
					WMI_WLAN_PROFILE_LIST_PROFILE_ID_CMDID,
					ret);
			break;
		case WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID:
			ret = wma_unified_fw_profiling_cmd(wma->wmi_handle,
					WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID,
					0, 0);
			if (ret)
				wma_err("Profile cmd failed for %d ret %d",
					WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID,
					ret);
			break;
		case WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID:
			/* Set the Green AP */
			ret = wmi_unified_green_ap_ps_send
					(wma->wmi_handle, privcmd->param_value,
					 WMA_WILDCARD_PDEV_ID);
			if (ret) {
				wma_err("Set GreenAP Failed val %d",
					privcmd->param_value);
			}
			break;

		default:
			wma_err("Invalid param id 0x%x", privcmd->param_id);
			break;
		}
		break;
	case PPS_CMD:
		wma_debug("dbg pid %d pval %d", privcmd->param_id,
			 privcmd->param_value);
		switch (privcmd->param_id) {

		case WMI_VDEV_PPS_PAID_MATCH:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				  (PKT_PWR_SAVE_PAID_MATCH & 0xffff);
			intr[vid].config.pps_params.paid_match_enable =
				privcmd->param_value;
			break;
		case WMI_VDEV_PPS_GID_MATCH:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				  (PKT_PWR_SAVE_GID_MATCH & 0xffff);
			intr[vid].config.pps_params.gid_match_enable =
				privcmd->param_value;
			break;
		case WMI_VDEV_PPS_EARLY_TIM_CLEAR:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				  (PKT_PWR_SAVE_EARLY_TIM_CLEAR & 0xffff);
			intr[vid].config.pps_params.tim_clear =
				privcmd->param_value;
			break;
		case WMI_VDEV_PPS_EARLY_DTIM_CLEAR:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				  (PKT_PWR_SAVE_EARLY_DTIM_CLEAR & 0xffff);
			intr[vid].config.pps_params.dtim_clear =
				privcmd->param_value;
			break;
		case WMI_VDEV_PPS_EOF_PAD_DELIM:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				  (PKT_PWR_SAVE_EOF_PAD_DELIM & 0xffff);
			intr[vid].config.pps_params.eof_delim =
				privcmd->param_value;
			break;
		case WMI_VDEV_PPS_MACADDR_MISMATCH:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				  (PKT_PWR_SAVE_MACADDR_MISMATCH & 0xffff);
			intr[vid].config.pps_params.mac_match =
				privcmd->param_value;
			break;
		case WMI_VDEV_PPS_DELIM_CRC_FAIL:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				  (PKT_PWR_SAVE_DELIM_CRC_FAIL & 0xffff);
			intr[vid].config.pps_params.delim_fail =
				privcmd->param_value;
			break;
		case WMI_VDEV_PPS_GID_NSTS_ZERO:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				  (PKT_PWR_SAVE_GID_NSTS_ZERO & 0xffff);
			intr[vid].config.pps_params.nsts_zero =
				privcmd->param_value;
			break;
		case WMI_VDEV_PPS_RSSI_CHECK:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				  (PKT_PWR_SAVE_RSSI_CHECK & 0xffff);
			intr[vid].config.pps_params.rssi_chk =
				privcmd->param_value;
			break;
		case WMI_VDEV_PPS_5G_EBT:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				  (PKT_PWR_SAVE_5G_EBT & 0xffff);
			intr[vid].config.pps_params.ebt_5g =
				privcmd->param_value;
			break;
		default:
			wma_err("Invalid param id 0x%x", privcmd->param_id);
			break;
		}
		break;

	case QPOWER_CMD:
		wma_debug("QPOWER CLI CMD pid %d pval %d", privcmd->param_id,
			 privcmd->param_value);
		switch (privcmd->param_id) {
		case WMI_STA_PS_PARAM_QPOWER_PSPOLL_COUNT:
			wma_debug("QPOWER CLI CMD:Ps Poll Cnt val %d",
				 privcmd->param_value);
			/* Set the QPower Ps Poll Count */
			ret = wma_unified_set_sta_ps_param(wma->wmi_handle,
				vid, WMI_STA_PS_PARAM_QPOWER_PSPOLL_COUNT,
				privcmd->param_value);
			if (ret) {
				wma_err("Set Q-PsPollCnt Failed vdevId %d val %d",
					vid, privcmd->param_value);
			} else {
				qparams->max_ps_poll_cnt = privcmd->param_value;
			}
			break;
		case WMI_STA_PS_PARAM_QPOWER_MAX_TX_BEFORE_WAKE:
			wma_debug("QPOWER CLI CMD:Max Tx Before wake val %d",
				 privcmd->param_value);
			/* Set the QPower Max Tx Before Wake */
			ret = wma_unified_set_sta_ps_param(wma->wmi_handle,
				vid, WMI_STA_PS_PARAM_QPOWER_MAX_TX_BEFORE_WAKE,
				privcmd->param_value);
			if (ret) {
				wma_err("Set Q-MaxTxBefWake Failed vId %d val %d",
					vid, privcmd->param_value);
			} else {
				qparams->max_tx_before_wake =
						privcmd->param_value;
			}
			break;
		case WMI_STA_PS_PARAM_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL:
			wma_debug("QPOWER CLI CMD:Ps Poll Wake Inv val %d",
				 privcmd->param_value);
			/* Set the QPower Spec Ps Poll Wake Inv */
			ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vid,
				WMI_STA_PS_PARAM_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL,
				privcmd->param_value);
			if (ret) {
				wma_err("Set Q-PsPoll WakeIntv Failed vId %d val %d",
					vid, privcmd->param_value);
			} else {
				qparams->spec_ps_poll_wake_interval =
					privcmd->param_value;
			}
			break;
		case WMI_STA_PS_PARAM_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL:
			wma_debug("QPOWER CLI CMD:Spec NoData Ps Poll val %d",
				 privcmd->param_value);
			/* Set the QPower Spec NoData PsPoll */
			ret = wma_unified_set_sta_ps_param(wma->wmi_handle, vid,
				WMI_STA_PS_PARAM_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL,
				privcmd->param_value);
			if (ret) {
				wma_err("Set Q-SpecNoDataPsPoll Failed vId %d val %d",
					vid, privcmd->param_value);
			} else {
				qparams->max_spec_nodata_ps_poll =
					privcmd->param_value;
			}
			break;

		default:
			wma_err("Invalid param id 0x%x", privcmd->param_id);
			break;
		}
		break;
	case GTX_CMD:
		wma_debug("vdev id %d pid %d pval %d", privcmd->param_vdev_id,
			 privcmd->param_id, privcmd->param_value);
		switch (privcmd->param_id) {
		case WMI_VDEV_PARAM_GTX_HT_MCS:
			intr[vid].config.gtx_info.gtxRTMask[0] =
				privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
					privcmd->param_vdev_id,
					&intr[vid].config.gtx_info);
			break;
		case WMI_VDEV_PARAM_GTX_VHT_MCS:
			intr[vid].config.gtx_info.gtxRTMask[1] =
				privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
					privcmd->param_vdev_id,
					&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_USR_CFG:
			intr[vid].config.gtx_info.gtxUsrcfg =
				privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
					privcmd->param_vdev_id,
					&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_THRE:
			intr[vid].config.gtx_info.gtxPERThreshold =
				privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
					privcmd->param_vdev_id,
					&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_MARGIN:
			intr[vid].config.gtx_info.gtxPERMargin =
				privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
					privcmd->param_vdev_id,
					&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_STEP:
			intr[vid].config.gtx_info.gtxTPCstep =
				privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
					privcmd->param_vdev_id,
					&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_MINTPC:
			intr[vid].config.gtx_info.gtxTPCMin =
				privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
					privcmd->param_vdev_id,
					&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_BW_MASK:
			intr[vid].config.gtx_info.gtxBWMask =
				privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
					privcmd->param_vdev_id,
					&intr[vid].config.gtx_info);
			if (ret) {
				wma_err("wma_vdev_set_param failed ret %d",
					 ret);
				return;
			}
			break;
		default:
			break;
		}
		break;

	default:
		wma_err("Invalid vpdev command id");
	}
	if (1 == privcmd->param_vp_dev) {
		switch (privcmd->param_id) {
		case WMI_VDEV_PARAM_NSS:
			intr[vid].config.nss = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_LDPC:
			intr[vid].config.ldpc = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_TX_STBC:
			intr[vid].config.tx_stbc = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_RX_STBC:
			intr[vid].config.rx_stbc = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_SGI:
			intr[vid].config.shortgi = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_ENABLE_RTSCTS:
			intr[vid].config.rtscts_en = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_CHWIDTH:
			intr[vid].config.chwidth = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_FIXED_RATE:
			intr[vid].config.tx_rate = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_ADJUST_ENABLE:
			intr[vid].config.erx_adjust = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_TGT_BMISS_NUM:
			intr[vid].config.erx_bmiss_num = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_BMISS_SAMPLE_CYCLE:
			intr[vid].config.erx_bmiss_cycle = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_SLOP_STEP:
			intr[vid].config.erx_slop_step = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_INIT_SLOP:
			intr[vid].config.erx_init_slop = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_ADJUST_PAUSE:
			intr[vid].config.erx_adj_pause = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_DRIFT_SAMPLE:
			intr[vid].config.erx_dri_sample = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_HE_DCM:
		case WMI_VDEV_PARAM_HE_RANGE_EXT:
			wma_set_he_vdev_param(&intr[vid], privcmd->param_id,
					      privcmd->param_value);
			break;
		default:
			wma_debug("Invalid wma_cli_set vdev command/Not yet implemented 0x%x",
				 privcmd->param_id);
			break;
		}
	} else if (2 == privcmd->param_vp_dev) {
		switch (privcmd->param_id) {
		case WMI_PDEV_PARAM_ANI_ENABLE:
			wma->pdevconfig.ani_enable = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_ANI_POLL_PERIOD:
			wma->pdevconfig.ani_poll_len = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_ANI_LISTEN_PERIOD:
			wma->pdevconfig.ani_listen_len = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_ANI_OFDM_LEVEL:
			wma->pdevconfig.ani_ofdm_level = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_ANI_CCK_LEVEL:
			wma->pdevconfig.ani_cck_level = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_DYNAMIC_BW:
			wma->pdevconfig.cwmenable = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_CTS_CBW:
			wma->pdevconfig.cts_cbw = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_TX_CHAIN_MASK:
			wma->pdevconfig.txchainmask = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_RX_CHAIN_MASK:
			wma->pdevconfig.rxchainmask = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_TXPOWER_LIMIT2G:
			wma->pdevconfig.txpow2g = privcmd->param_value;
			if (mac->mlme_cfg->gen.band_capability & BIT(REG_BAND_2G))
				mac->mlme_cfg->power.current_tx_power_level =
					(uint8_t)privcmd->param_value;
			else
				wma_err("Current band is not 2G");
			break;
		case WMI_PDEV_PARAM_TXPOWER_LIMIT5G:
			wma->pdevconfig.txpow5g = privcmd->param_value;
			if (mac->mlme_cfg->gen.band_capability & BIT(REG_BAND_5G))
				mac->mlme_cfg->power.current_tx_power_level =
					(uint8_t)privcmd->param_value;
			else
				wma_err("Current band is not 5G");
			break;
		default:
			wma_debug("Invalid wma_cli_set pdev command/Not yet implemented 0x%x",
				 privcmd->param_id);
			break;
		}
	} else if (5 == privcmd->param_vp_dev) {
		ret = wma_vdev_set_param(wma->wmi_handle,
					privcmd->param_vdev_id,
					WMI_VDEV_PARAM_PACKET_POWERSAVE,
					pps_val);
		if (ret)
			wma_err("Failed to send wmi packet power save cmd");
		else
			wma_debug("Sent packet power save cmd %d value %x to target",
				privcmd->param_id, pps_val);
	}
}

uint32_t wma_critical_events_in_flight(void)
{
	t_wma_handle *wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma || !wma->wmi_handle) {
		wma_err("Invalid wma or wmi handle");
		return 0;
	}
	return wmi_critical_events_in_flight(wma->wmi_handle);
}

/**
 * wma_process_hal_pwr_dbg_cmd() - send hal pwr dbg cmd to fw.
 * @handle: wma handle
 * @sir_pwr_dbg_params: unit test command
 *
 * This function send unit test command to fw.
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wma_process_hal_pwr_dbg_cmd(WMA_HANDLE handle,
				       struct sir_mac_pwr_dbg_cmd *
				       sir_pwr_dbg_params)
{
	tp_wma_handle wma_handle = (tp_wma_handle)handle;
	int i;
	struct wmi_power_dbg_params wmi_pwr_dbg_params;
	QDF_STATUS status;

	if (!sir_pwr_dbg_params) {
		wma_err("sir_pwr_dbg_params is null");
		return QDF_STATUS_E_INVAL;
	}
	wmi_pwr_dbg_params.module_id = sir_pwr_dbg_params->module_id;
	wmi_pwr_dbg_params.pdev_id = sir_pwr_dbg_params->pdev_id;
	wmi_pwr_dbg_params.num_args = sir_pwr_dbg_params->num_args;

	for (i = 0; i < wmi_pwr_dbg_params.num_args; i++)
		wmi_pwr_dbg_params.args[i] = sir_pwr_dbg_params->args[i];

	status = wmi_unified_send_power_dbg_cmd(wma_handle->wmi_handle,
						&wmi_pwr_dbg_params);

	return status;
}

static QDF_STATUS wma_discard_fw_event(struct scheduler_msg *msg)
{
	if (!msg->bodyptr)
		return QDF_STATUS_E_INVAL;

	qdf_mem_free(msg->bodyptr);
	msg->bodyptr = NULL;
	msg->bodyval = 0;
	msg->type = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wma_vdev_nss_chain_params_send(uint8_t vdev_id,
			       struct wlan_mlme_nss_chains *user_cfg)
{
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	struct vdev_nss_chains vdev_user_cfg;
	if (!wma_handle) {
		wma_err("wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_user_cfg.disable_rx_mrc[NSS_CHAINS_BAND_2GHZ] =
				user_cfg->disable_rx_mrc[NSS_CHAINS_BAND_2GHZ];
	vdev_user_cfg.disable_tx_mrc[NSS_CHAINS_BAND_2GHZ] =
				user_cfg->disable_tx_mrc[NSS_CHAINS_BAND_2GHZ];
	vdev_user_cfg.disable_rx_mrc[NSS_CHAINS_BAND_5GHZ] =
				user_cfg->disable_rx_mrc[NSS_CHAINS_BAND_5GHZ];
	vdev_user_cfg.disable_tx_mrc[NSS_CHAINS_BAND_5GHZ] =
				user_cfg->disable_tx_mrc[NSS_CHAINS_BAND_5GHZ];

	vdev_user_cfg.num_rx_chains[NSS_CHAINS_BAND_2GHZ]
				= user_cfg->num_rx_chains[NSS_CHAINS_BAND_2GHZ];
	vdev_user_cfg.num_tx_chains[NSS_CHAINS_BAND_2GHZ]
				= user_cfg->num_tx_chains[NSS_CHAINS_BAND_2GHZ];
	vdev_user_cfg.num_rx_chains[NSS_CHAINS_BAND_5GHZ] =
				user_cfg->num_rx_chains[NSS_CHAINS_BAND_5GHZ];
	vdev_user_cfg.num_tx_chains[NSS_CHAINS_BAND_5GHZ] =
				user_cfg->num_tx_chains[NSS_CHAINS_BAND_5GHZ];

	vdev_user_cfg.rx_nss[NSS_CHAINS_BAND_2GHZ] =
				user_cfg->rx_nss[NSS_CHAINS_BAND_2GHZ];
	vdev_user_cfg.tx_nss[NSS_CHAINS_BAND_2GHZ] =
				user_cfg->tx_nss[NSS_CHAINS_BAND_2GHZ];
	vdev_user_cfg.rx_nss[NSS_CHAINS_BAND_5GHZ] =
				user_cfg->rx_nss[NSS_CHAINS_BAND_5GHZ];
	vdev_user_cfg.tx_nss[NSS_CHAINS_BAND_5GHZ] =
				user_cfg->tx_nss[NSS_CHAINS_BAND_5GHZ];

	vdev_user_cfg.num_tx_chains_11a = user_cfg->num_tx_chains_11a;
	vdev_user_cfg.num_tx_chains_11b = user_cfg->num_tx_chains_11b;
	vdev_user_cfg.num_tx_chains_11g = user_cfg->num_tx_chains_11g;

	return wmi_unified_vdev_nss_chain_params_send(wma_handle->wmi_handle,
						      vdev_id,
						      &vdev_user_cfg);
}

/**
 * wma_antenna_isolation_event_handler() - antenna isolation event handler
 * @handle: wma handle
 * @param: event data
 * @len: length
 *
 * Return: 0 for success or error code
 */
static int wma_antenna_isolation_event_handler(void *handle,
					       u8 *param,
					       u32 len)
{
	struct scheduler_msg cds_msg = {0};
	wmi_coex_report_isolation_event_fixed_param *event;
	WMI_COEX_REPORT_ANTENNA_ISOLATION_EVENTID_param_tlvs *param_buf;
	struct sir_isolation_resp *pisolation;
	struct mac_context *mac = NULL;

	wma_debug("handle %pK param %pK len %d", handle, param, len);

	mac = (struct mac_context *)cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		wma_err("Invalid mac context");
		return -EINVAL;
	}

	pisolation = qdf_mem_malloc(sizeof(*pisolation));
	if (!pisolation)
		return 0;

	param_buf =
		(WMI_COEX_REPORT_ANTENNA_ISOLATION_EVENTID_param_tlvs *)param;
	if (!param_buf) {
		wma_err("Invalid isolation event");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	pisolation->isolation_chain0 = event->isolation_chain0;
	pisolation->isolation_chain1 = event->isolation_chain1;
	pisolation->isolation_chain2 = event->isolation_chain2;
	pisolation->isolation_chain3 = event->isolation_chain3;

	wma_debug("chain1 %d chain2 %d chain3 %d chain4 %d",
		 pisolation->isolation_chain0, pisolation->isolation_chain1,
		 pisolation->isolation_chain2, pisolation->isolation_chain3);

	cds_msg.type = eWNI_SME_ANTENNA_ISOLATION_RSP;
	cds_msg.bodyptr = pisolation;
	cds_msg.bodyval = 0;
	if (QDF_STATUS_SUCCESS !=
	    scheduler_post_message(QDF_MODULE_ID_WMA,
				   QDF_MODULE_ID_SME,
				   QDF_MODULE_ID_SME, &cds_msg)) {
		wma_err("could not post peer info rsp msg to SME");
		/* free the mem and return */
		qdf_mem_free(pisolation);
	}

	return 0;
}

/**
 * wma_init_max_no_of_peers - API to initialize wma configuration params
 * @wma_handle: WMA Handle
 * @max_peers: Max Peers supported
 *
 * Return: void
 */
static uint8_t wma_init_max_no_of_peers(tp_wma_handle wma_handle,
				     uint16_t max_peers)
{
	struct wma_ini_config *cfg = wma_get_ini_handle(wma_handle);
	struct hif_opaque_softc *scn = cds_get_context(QDF_MODULE_ID_HIF);
	uint32_t tgt_version = hif_get_target_info_handle(scn)->target_version;
	uint8_t max_no_of_peers;
	uint8_t max_supported_peers;

	if (!cfg) {
		wma_err("NULL WMA ini handle");
		return 0;
	}

	switch (tgt_version) {
	case AR6320_REV1_1_VERSION:
		max_supported_peers = MAX_SUPPORTED_PEERS_REV1_1;
		break;
	case AR6320_REV1_3_VERSION:
		max_supported_peers = MAX_SUPPORTED_PEERS_REV1_3;
		break;
	default:
		max_supported_peers = MAX_SUPPORTED_PEERS;
		break;
	}
	max_no_of_peers = (max_peers > max_supported_peers) ?
				max_supported_peers : max_peers;
	cfg->max_no_of_peers = max_no_of_peers;

	return max_no_of_peers;
}

/**
 * wma_cleanup_hold_req() - cleanup hold request queue
 * @wma: wma handle
 *
 * Return: none
 */
static void wma_cleanup_hold_req(tp_wma_handle wma)
{
	struct wma_target_req *req_msg = NULL;
	qdf_list_node_t *node1 = NULL;

	qdf_spin_lock_bh(&wma->wma_hold_req_q_lock);
	if (!qdf_list_size(&wma->wma_hold_req_queue)) {
		qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
		wma_debug("request queue is empty");
		return;
	}

	/* peek front, and then cleanup it in wma_hold_req_timer */
	while (QDF_STATUS_SUCCESS ==
		qdf_list_peek_front(&wma->wma_hold_req_queue, &node1)) {
		req_msg = qdf_container_of(node1, struct wma_target_req, node);
		qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
		/* Cleanup timeout handler */
		qdf_mc_timer_stop(&req_msg->event_timeout);
		wma_hold_req_timer(req_msg);
		qdf_spin_lock_bh(&wma->wma_hold_req_q_lock);
	}
	qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
}

/**
 * wma_cleanup_vdev_resp_and_hold_req() - cleaunup the vdev resp and hold req
 * queue
 * @msg :scheduler msg
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wma_cleanup_vdev_resp_and_hold_req(struct scheduler_msg *msg)
{
	tp_wma_handle wma;

	if (!msg || !msg->bodyptr) {
		wma_err("msg or body pointer is NULL");
		return QDF_STATUS_E_INVAL;
	}

	wma = msg->bodyptr;
	target_if_flush_psoc_vdev_timers(wma->psoc);
	wma_cleanup_hold_req(wma);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_cleanup_vdev_resp_and_hold_req_flush_cb() - flush cb for the msg to clean
 * up vdev resp and hold req
 * @msg :scheduler msg
 *
 * As passed msg->bodyptr is wma in this case this is dummy flush cb so that
 * driver doesnt try to free msg->bodyptr when this msg is flushed.
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
wma_cleanup_vdev_resp_and_hold_req_flush_cb(struct scheduler_msg *msg)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_shutdown_notifier_cb - Shutdown notifer call back
 * @priv : WMA handle
 *
 * During recovery, WMA may wait for resume to complete if the crash happens
 * while in suspend. This may cause delays in completing the recovery. This call
 * back would be called during recovery and the event is completed so that if
 * the resume is waiting on FW to respond then it can get out of the wait so
 * that recovery thread can start bringing down all the modules.
 *
 * Return: None
 */
static void wma_shutdown_notifier_cb(void *priv)
{
	tp_wma_handle wma_handle = priv;
	struct scheduler_msg msg = { 0 };
	QDF_STATUS status;

	ucfg_pmo_psoc_wakeup_host_event_received(wma_handle->psoc);
	wmi_stop(wma_handle->wmi_handle);

	msg.bodyptr = wma_handle;
	msg.callback = wma_cleanup_vdev_resp_and_hold_req;
	msg.flush_callback = wma_cleanup_vdev_resp_and_hold_req_flush_cb;
	status = scheduler_post_message(QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_TARGET_IF, &msg);
}

struct wma_version_info g_wmi_version_info;

#ifdef WLAN_FEATURE_MEMDUMP_ENABLE
/**
 * wma_state_info_dump() - prints state information of wma layer
 * @buf: buffer pointer
 * @size: size of buffer to be filled
 *
 * This function is used to dump state information of wma layer
 *
 * Return: None
 */
static void wma_state_info_dump(char **buf_ptr, uint16_t *size)
{
	uint8_t vdev_id;
	uint16_t len = 0;
	t_wma_handle *wma;
	char *buf = *buf_ptr;
	struct wma_txrx_node *iface;
	struct wake_lock_stats stats;
	struct wlan_objmgr_vdev *vdev;
	uint32_t rate_flag;
	QDF_STATUS status;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("WMA context is invalid!");
		return;
	}

	wma_debug("size of buffer: %d", *size);

	for (vdev_id = 0; vdev_id < wma->max_bssid; vdev_id++) {
		iface = &wma->interfaces[vdev_id];
		vdev = iface->vdev;
		if (!vdev)
			continue;

		status = wma_get_vdev_rate_flag(iface->vdev, &rate_flag);
		if (QDF_IS_STATUS_ERROR(status))
			continue;

		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(wma->psoc,
						vdev_id, WLAN_LEGACY_WMA_ID);
		if (!vdev)
			continue;
		ucfg_mc_cp_stats_get_vdev_wake_lock_stats(vdev, &stats);
		len += qdf_scnprintf(buf + len, *size - len,
			"\n"
			"vdev_id %d\n"
			"WoW Stats\n"
			"\tpno_match %u\n"
			"\tpno_complete %u\n"
			"\tgscan %u\n"
			"\tlow_rssi %u\n"
			"\trssi_breach %u\n"
			"\tucast %u\n"
			"\tbcast %u\n"
			"\ticmpv4 %u\n"
			"\ticmpv6 %u\n"
			"\tipv4_mcast %u\n"
			"\tipv6_mcast %u\n"
			"\tipv6_mcast_ra %u\n"
			"\tipv6_mcast_ns %u\n"
			"\tipv6_mcast_na %u\n"
			"\toem_response %u\n"
			"\tuc_drop %u\n"
			"\tfatal_event %u\n"
			"dtimPeriod %d\n"
			"chan_width %d\n"
			"vdev_active %d\n"
			"vdev_up %d\n"
			"aid %d\n"
			"rate_flags %d\n"
			"nss %d\n"
			"nwType %d\n"
			"tx_streams %d",
			vdev_id,
			stats.pno_match_wake_up_count,
			stats.pno_complete_wake_up_count,
			stats.gscan_wake_up_count,
			stats.low_rssi_wake_up_count,
			stats.rssi_breach_wake_up_count,
			stats.ucast_wake_up_count,
			stats.bcast_wake_up_count,
			stats.icmpv4_count,
			stats.icmpv6_count,
			stats.ipv4_mcast_wake_up_count,
			stats.ipv6_mcast_wake_up_count,
			stats.ipv6_mcast_ra_stats,
			stats.ipv6_mcast_ns_stats,
			stats.ipv6_mcast_na_stats,
			stats.oem_response_wake_up_count,
			stats.uc_drop_wake_up_count,
			stats.fatal_event_wake_up_count,
			iface->dtimPeriod,
			iface->chan_width,
			iface->vdev_active,
			wma_is_vdev_up(vdev_id),
			iface->aid,
			rate_flag,
			iface->nss,
			iface->nwType,
			iface->tx_streams);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_WMA_ID);
	}

	*size -= len;
	*buf_ptr += len;
}

/**
 * wma_register_debug_callback() - registration function for wma layer
 * to print wma state information
 */
static void wma_register_debug_callback(void)
{
	qdf_register_debug_callback(QDF_MODULE_ID_WMA, &wma_state_info_dump);
}
#else /* WLAN_FEATURE_MEMDUMP_ENABLE */
static void wma_register_debug_callback(void)
{
}
#endif /* WLAN_FEATURE_MEMDUMP_ENABLE */
/**
 * wma_register_tx_ops_handler() - register tx_ops of southbound
 * @tx_ops:  tx_ops pointer in southbound
 *
 * Return: 0 on success, errno on failure
 */
static QDF_STATUS
wma_register_tx_ops_handler(struct wlan_lmac_if_tx_ops *tx_ops)
{
	/*
	 * Assign tx_ops, it's up to UMAC modules to declare and define these
	 * functions which are used to send wmi command to target.
	 */

	if (!tx_ops) {
		wma_err("pointer to lmac if tx ops is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/* mgmt_txrx component's tx ops */
	tx_ops->mgmt_txrx_tx_ops.mgmt_tx_send = wma_mgmt_unified_cmd_send;

	/* mgmt txrx component nbuf op for nbuf dma unmap */
	tx_ops->mgmt_txrx_tx_ops.tx_drain_nbuf_op = wma_mgmt_nbuf_unmap_cb;

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_target_if_open() - Attach UMAC modules' interface with wmi layer
 * @wma_handle: wma handle
 *
 * Separate module defines below functions:
 * 1. tgt_wmi_<module>_<action> api sends wmi command, assigned to south bound
 *    tx_ops function pointers;
 * 2. module's south dispatcher handles information from lower layer, assigned
 *    to south bound rx_ops function pointers;
 * 3. wmi event handler deals with wmi event, extracts umac needed information,
 *    and call rx_ops(module's dispatcher). It executes in tasklet context and
 *    is up to dispatcher to decide the context to reside in tasklet or in
 *    thread context.
 *
 * Return: None
 */
static void wma_target_if_open(tp_wma_handle wma_handle)
{
	struct wlan_objmgr_psoc *psoc = wma_handle->psoc;

	if (!psoc)
		return;

	wlan_global_lmac_if_set_txops_registration_cb(WLAN_DEV_OL,
					target_if_register_tx_ops);
	wlan_lmac_if_set_umac_txops_registration_cb(
		wma_register_tx_ops_handler);
	wlan_global_lmac_if_open(psoc);

}

/**
 * wma_legacy_service_ready_event_handler() - legacy (ext)service ready handler
 * @event_id: event_id
 * @handle: wma handle
 * @event_data: event data
 * @length: event length
 *
 * Return: 0 for success, negative error code for failure
 */
static int wma_legacy_service_ready_event_handler(uint32_t event_id,
						  void *handle,
						  uint8_t *event_data,
						  uint32_t length)
{
	switch (event_id) {
	case wmi_service_ready_event_id:
		return wma_rx_service_ready_event(handle, event_data, length);
	case wmi_service_ready_ext_event_id:
		return wma_rx_service_ready_ext_event(handle, event_data,
						      length);
	case wmi_ready_event_id:
		return wma_rx_ready_event(handle, event_data, length);
	default:
		wma_err("Legacy callback invoked with invalid event_id:%d",
			 event_id);
		QDF_BUG(0);
	}

	return 0;
}

#ifdef WLAN_FEATURE_CAL_FAILURE_TRIGGER
/**
 * wma_process_cal_fail_info() - Process cal failure event and
 *                               send it to userspace
 * @wmi_event:  Cal failure event data
 */
static void wma_process_cal_fail_info(uint8_t *wmi_event)
{
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	uint8_t *buf_ptr;
	wmi_debug_mesg_fw_cal_failure_param *cal_failure_event;

	if (!mac) {
		wma_err("Invalid mac context");
		return;
	}

	if (!mac->cal_failure_event_cb) {
		wma_err("Callback not registered for cal failure event");
		return;
	}

	buf_ptr = wmi_event;
	buf_ptr = buf_ptr + sizeof(wmi_debug_mesg_flush_complete_fixed_param) +
		  WMI_TLV_HDR_SIZE +
		  sizeof(wmi_debug_mesg_fw_data_stall_param) + WMI_TLV_HDR_SIZE;

	cal_failure_event = (wmi_debug_mesg_fw_cal_failure_param *)buf_ptr;

	if (((cal_failure_event->tlv_header & 0xFFFF0000) >> 16 ==
			WMITLV_TAG_STRUC_wmi_debug_mesg_fw_cal_failure_param)) {
		/**
		 * Log calibration failure information received from FW
		 */
		wma_debug("Calibration failure event:");
		wma_debug("calType: %x calFailureReasonCode: %x",
			  cal_failure_event->cal_type,
			  cal_failure_event->cal_failure_reason_code);
		mac->cal_failure_event_cb(
				cal_failure_event->cal_type,
				cal_failure_event->cal_failure_reason_code);
	} else {
		wma_err("Invalid TLV header in cal failure event");
	}
}
#else
static inline void wma_process_cal_fail_info(uint8_t *wmi_event)
{
}
#endif

/**
 * wma_flush_complete_evt_handler() - FW log flush complete event handler
 * @handle: WMI handle
 * @event:  Event recevied from FW
 * @len:    Length of the event
 *
 */
static int wma_flush_complete_evt_handler(void *handle,
		u_int8_t *event,
		u_int32_t len)
{
	QDF_STATUS status;
	tp_wma_handle wma = (tp_wma_handle) handle;

	WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID_param_tlvs *param_buf;
	wmi_debug_mesg_flush_complete_fixed_param *wmi_event;
	wmi_debug_mesg_fw_data_stall_param *data_stall_event;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t *buf_ptr;
	uint32_t reason_code;

	param_buf = (WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		wma_err("Invalid log flush complete event buffer");
		return QDF_STATUS_E_FAILURE;
	}

	wmi_event = param_buf->fixed_param;
	reason_code = wmi_event->reserved0;
	wma_debug("Received reason code %d from FW", reason_code);

	if (reason_code == WMI_DIAG_TRIGGER_DATA_STALL) {
		buf_ptr = (uint8_t *)wmi_event;
		buf_ptr = buf_ptr +
			  sizeof(wmi_debug_mesg_flush_complete_fixed_param) +
			  WMI_TLV_HDR_SIZE;
		data_stall_event =
				(wmi_debug_mesg_fw_data_stall_param *)buf_ptr;
	}

	if (reason_code == WMI_DIAG_TRIGGER_DATA_STALL &&
	    ((data_stall_event->tlv_header & 0xFFFF0000) >> 16 ==
	      WMITLV_TAG_STRUC_wmi_debug_mesg_fw_data_stall_param)) {
		/**
		 * Log data stall info received from FW:
		 *
		 * Possible data stall recovery types:
		 * WLAN_DBG_DATA_STALL_RECOVERY_CONNECT_DISCONNECT
		 * WLAN_DBG_DATA_STALL_RECOVERY_CONNECT_MAC_PHY_RESET
		 * WLAN_DBG_DATA_STALL_RECOVERY_CONNECT_PDR
		 *
		 * Possible data stall event types:
		 * WLAN_DBG_DATA_STALL_VDEV_PAUSE
		 * WLAN_DBG_DATA_STALL_HWSCHED_CMD_FILTER
		 * WLAN_DBG_DATA_STALL_HWSCHED_CMD_FLUSH
		 * WLAN_DBG_DATA_STALL_RX_REFILL_FAILED
		 * WLAN_DBG_DATA_STALL_RX_FCS_LEN_ERROR
		 *
		 * reason_code1:
		 * The information stored in reason_code1 varies based on the
		 * data stall type values:
		 *
		 * data_stall_type      | reason_code1
		 * -----------------------------------------------------
		 * HWSCHED_CMD_FLUSH    | flush req reason (0-40)
		 * RX_REFILL_FAILED     | ring_id (0-7)
		 * RX_FCS_LEN_ERROR     | exact error type
		 *
		 * reasone_code2:
		 * on which tid/hwq stall happened
		 *
		 */
		QDF_TRACE(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_DEBUG,
			  "Data Stall event:");
		QDF_TRACE(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_DEBUG,
			  "data_stall_type: %x vdev_id_bitmap: %x reason_code1: %x reason_code2: %x recovery_type: %x ",
			  data_stall_event->data_stall_type,
			  data_stall_event->vdev_id_bitmap,
			  data_stall_event->reason_code1,
			  data_stall_event->reason_code2,
			  data_stall_event->recovery_type);

		cdp_post_data_stall_event(soc,
					DATA_STALL_LOG_INDICATOR_FIRMWARE,
					data_stall_event->data_stall_type,
					OL_TXRX_PDEV_ID,
					data_stall_event->vdev_id_bitmap,
					data_stall_event->recovery_type);
	}

	if (reason_code == WMI_DIAG_TRIGGER_CAL_FAILURE) {
		wma_process_cal_fail_info((uint8_t *)wmi_event);
		return QDF_STATUS_SUCCESS;
	}

	/*
	 * reason_code = 0; Flush event in response to flush command
	 * reason_code = other value; Asynchronous flush event for fatal events
	 */
	if (!reason_code && (cds_is_log_report_in_progress() == false)) {
		wma_debug("Received WMI flush event without sending CMD");
		return -EINVAL;
	} else if (!reason_code && cds_is_log_report_in_progress() == true) {
		/* Flush event in response to flush command */
		wma_debug("Received WMI flush event in response to flush CMD");
		status = qdf_mc_timer_stop(&wma->log_completion_timer);
		if (status != QDF_STATUS_SUCCESS)
			wma_err("Failed to stop the log completion timeout");
		cds_logging_set_fw_flush_complete();
		return QDF_STATUS_SUCCESS;
	} else if (reason_code && cds_is_log_report_in_progress() == false) {
		/* Asynchronous flush event for fatal events */
		status = cds_set_log_completion(WLAN_LOG_TYPE_FATAL,
				WLAN_LOG_INDICATOR_FIRMWARE,
				reason_code, false);
		if (QDF_STATUS_SUCCESS != status) {
			wma_err("Failed to set log trigger params");
			return QDF_STATUS_E_FAILURE;
		}
		cds_logging_set_fw_flush_complete();
		return status;
	} else {
		/* Asynchronous flush event for fatal event,
		 * but, report in progress already
		 */
		wma_debug("Bug report already in progress - dropping! type:%d, indicator=%d reason_code=%d",
				WLAN_LOG_TYPE_FATAL,
				WLAN_LOG_INDICATOR_FIRMWARE, reason_code);
		return QDF_STATUS_E_FAILURE;
	}
	/* Asynchronous flush event for fatal event,
	 * but, report in progress already
	 */
	wma_warn("Bug report already in progress - dropping! type:%d, indicator=%d reason_code=%d",
		WLAN_LOG_TYPE_FATAL,
		WLAN_LOG_INDICATOR_FIRMWARE, reason_code);
	return QDF_STATUS_E_FAILURE;
}

#ifdef WLAN_CONV_SPECTRAL_ENABLE
/**
 * wma_extract_single_phyerr_spectral() - extract single phy error from event
 * @handle: wma handle
 * @param evt_buf: pointer to event buffer
 * @param datalen: data length of event buffer
 * @param buf_offset: Pointer to hold value of current event buffer offset
 * post extraction
 * @param phyerr: Pointer to hold phyerr
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wma_extract_single_phyerr_spectral(void *handle,
		void *evt_buf,
		uint16_t datalen, uint16_t *buf_offset,
		wmi_host_phyerr_t *phyerr)
{
	wmi_single_phyerr_rx_event *ev;
	int n = *buf_offset;

	ev = (wmi_single_phyerr_rx_event *)((uint8_t *)evt_buf + n);

	if (n < datalen) {
		/* ensure there's at least space for the header */
		if ((datalen - n) < sizeof(ev->hdr)) {
			wma_err("not enough space? (datalen=%d, n=%d, hdr=%zu bytes",
				datalen, n, sizeof(ev->hdr));
			return QDF_STATUS_E_FAILURE;
		}

		phyerr->bufp = ev->bufp;
		phyerr->buf_len = ev->hdr.buf_len;

		/*
		 * Sanity check the buffer length of the event against
		 * what we currently have.
		 *
		 * Since buf_len is 32 bits, we check if it overflows
		 * a large 32 bit value.  It's not 0x7fffffff because
		 * we increase n by (buf_len + sizeof(hdr)), which would
		 * in itself cause n to overflow.
		 *
		 * If "int" is 64 bits then this becomes a moot point.
		 */
		if (ev->hdr.buf_len > 0x7f000000) {
			wma_err("buf_len is garbage? (0x%x)", ev->hdr.buf_len);
			return QDF_STATUS_E_FAILURE;
		}
		if (n + ev->hdr.buf_len > datalen) {
			wma_err("buf_len exceeds available space n=%d, buf_len=%d, datalen=%d",
				n, ev->hdr.buf_len, datalen);
			return QDF_STATUS_E_FAILURE;
		}

		phyerr->phy_err_code = WMI_UNIFIED_PHYERRCODE_GET(&ev->hdr);
		phyerr->tsf_timestamp = ev->hdr.tsf_timestamp;

#ifdef DEBUG_SPECTRAL_SCAN
		wma_debug("len=%d, tsf=0x%08x, rssi = 0x%x/0x%x/0x%x/0x%x, comb rssi = 0x%x, phycode=%d",
				ev->hdr.buf_len,
				ev->hdr.tsf_timestamp,
				ev->hdr.rssi_chain0,
				ev->hdr.rssi_chain1,
				ev->hdr.rssi_chain2,
				ev->hdr.rssi_chain3,
				WMI_UNIFIED_RSSI_COMB_GET(&ev->hdr),
					  phyerr->phy_err_code);

		/*
		 * For now, unroll this loop - the chain 'value' field isn't
		 * a variable but glued together into a macro field definition.
		 * Grr. :-)
		 */
		wma_debug("chain 0: raw=0x%08x; pri20=%d sec20=%d sec40=%d sec80=%d",
				ev->hdr.rssi_chain0,
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 0, PRI20),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 0, SEC20),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 0, SEC40),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 0, SEC80));

		wma_debug("chain 1: raw=0x%08x: pri20=%d sec20=%d sec40=%d sec80=%d",
				ev->hdr.rssi_chain1,
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 1, PRI20),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 1, SEC20),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 1, SEC40),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 1, SEC80));

		wma_debug("chain 2: raw=0x%08x: pri20=%d sec20=%d sec40=%d sec80=%d",
				ev->hdr.rssi_chain2,
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 2, PRI20),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 2, SEC20),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 2, SEC40),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 2, SEC80));

		wma_debug("chain 3: raw=0x%08x: pri20=%d sec20=%d sec40=%d sec80=%d",
				ev->hdr.rssi_chain3,
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 3, PRI20),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 3, SEC20),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 3, SEC40),
				WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 3, SEC80));


		wma_debug("freq_info_1=0x%08x, freq_info_2=0x%08x",
			   ev->hdr.freq_info_1, ev->hdr.freq_info_2);

		/*
		 * The NF chain values are signed and are negative - hence
		 * the cast evilness.
		 */
		wma_debug("nfval[1]=0x%08x, nfval[2]=0x%08x, nf=%d/%d/%d/%d, freq1=%d, freq2=%d, cw=%d",
				ev->hdr.nf_list_1,
				ev->hdr.nf_list_2,
				(int) WMI_UNIFIED_NF_CHAIN_GET(&ev->hdr, 0),
				(int) WMI_UNIFIED_NF_CHAIN_GET(&ev->hdr, 1),
				(int) WMI_UNIFIED_NF_CHAIN_GET(&ev->hdr, 2),
				(int) WMI_UNIFIED_NF_CHAIN_GET(&ev->hdr, 3),
				WMI_UNIFIED_FREQ_INFO_GET(&ev->hdr, 1),
				WMI_UNIFIED_FREQ_INFO_GET(&ev->hdr, 2),
				WMI_UNIFIED_CHWIDTH_GET(&ev->hdr));
#endif

		/*
		 * If required, pass spectral events to the spectral module
		 */
		if (ev->hdr.buf_len > 0) {

			/* Initialize the NF values to Zero. */
			phyerr->rf_info.noise_floor[0] =
			    WMI_UNIFIED_NF_CHAIN_GET(&ev->hdr, 0);
			phyerr->rf_info.noise_floor[1] =
			    WMI_UNIFIED_NF_CHAIN_GET(&ev->hdr, 1);
			phyerr->rf_info.noise_floor[2] =
			    WMI_UNIFIED_NF_CHAIN_GET(&ev->hdr, 2);
			phyerr->rf_info.noise_floor[3] =
			    WMI_UNIFIED_NF_CHAIN_GET(&ev->hdr, 3);

			/* populate the rf info */
			phyerr->rf_info.rssi_comb =
			    WMI_UNIFIED_RSSI_COMB_GET(&ev->hdr);

			/* Need to unroll loop due to macro
			 * constraints chain 0
			 */
			phyerr->rf_info.pc_rssi_info[0].rssi_pri20 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 0, PRI20);
			phyerr->rf_info.pc_rssi_info[0].rssi_sec20 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 0, SEC20);
			phyerr->rf_info.pc_rssi_info[0].rssi_sec40 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 0, SEC40);
			phyerr->rf_info.pc_rssi_info[0].rssi_sec80 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 0, SEC80);

			/* chain 1 */
			phyerr->rf_info.pc_rssi_info[1].rssi_pri20 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 1, PRI20);
			phyerr->rf_info.pc_rssi_info[1].rssi_sec20 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 1, SEC20);
			phyerr->rf_info.pc_rssi_info[1].rssi_sec40 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 1, SEC40);
			phyerr->rf_info.pc_rssi_info[1].rssi_sec80 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 1, SEC80);

			/* chain 2 */
			phyerr->rf_info.pc_rssi_info[2].rssi_pri20 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 2, PRI20);
			phyerr->rf_info.pc_rssi_info[2].rssi_sec20 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 2, SEC20);
			phyerr->rf_info.pc_rssi_info[2].rssi_sec40 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 2, SEC40);
			phyerr->rf_info.pc_rssi_info[2].rssi_sec80 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 2, SEC80);

			/* chain 3 */
			phyerr->rf_info.pc_rssi_info[3].rssi_pri20 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 3, PRI20);
			phyerr->rf_info.pc_rssi_info[3].rssi_sec20 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 3, SEC20);
			phyerr->rf_info.pc_rssi_info[3].rssi_sec40 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 3, SEC40);
			phyerr->rf_info.pc_rssi_info[3].rssi_sec80 =
			WMI_UNIFIED_RSSI_CHAN_GET(&ev->hdr, 3, SEC80);

			phyerr->chan_info.center_freq1 =
			    WMI_UNIFIED_FREQ_INFO_GET(&ev->hdr, 1);
			phyerr->chan_info.center_freq2 =
			    WMI_UNIFIED_FREQ_INFO_GET(&ev->hdr, 2);

		}

		/*
		 * Advance the buffer pointer to the next PHY error.
		 * buflen is the length of this payload, so we need to
		 * advance past the current header _AND_ the payload.
		 */
		 n += sizeof(*ev) + ev->hdr.buf_len;
	}
	*buf_offset += n;

	return QDF_STATUS_SUCCESS;
}

/**
 * spectral_phyerr_event_handler() - spectral phyerr event handler
 * @handle: wma handle
 * @data: data buffer
 * @datalen: buffer length
 *
 * Return:  QDF_STATUS
 */
static QDF_STATUS spectral_phyerr_event_handler(void *handle,
						uint8_t *data,
						uint32_t datalen)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint16_t buf_offset, event_buf_len = 0;
	wmi_single_phyerr_rx_event *ev;
	wmi_host_phyerr_t phyerr;
	struct target_if_spectral_rfqual_info rfqual_info;
	struct target_if_spectral_chan_info chan_info;
	struct target_if_spectral_acs_stats acs_stats;

	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	memset(&phyerr, 0, sizeof(wmi_host_phyerr_t));
	status = wmi_extract_comb_phyerr(wma->wmi_handle, data, datalen,
					 &buf_offset, &phyerr);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("extract comb phyerr failed");
		return QDF_STATUS_E_FAILURE;
	}

	ev = (wmi_single_phyerr_rx_event *)phyerr.bufp;
	event_buf_len = phyerr.buf_len;
	/* Loop over the bufp, extracting out phyerrors */
	buf_offset = 0;
	while (buf_offset < event_buf_len) {
		if (wma_extract_single_phyerr_spectral(handle, ev,
			event_buf_len, &buf_offset, &phyerr)) {
			wma_err("extract single phy err failed");
			return QDF_STATUS_E_FAILURE;
		}

		if (phyerr.buf_len > 0) {
			if (sizeof(phyerr.rf_info) > sizeof(rfqual_info))
				qdf_mem_copy(&rfqual_info, &phyerr.rf_info,
						sizeof(rfqual_info));
			else
				qdf_mem_copy(&rfqual_info, &phyerr.rf_info,
						sizeof(phyerr.rf_info));

			if (sizeof(phyerr.chan_info) > sizeof(chan_info))
				qdf_mem_copy(&chan_info, &phyerr.chan_info,
						sizeof(chan_info));
			else
				qdf_mem_copy(&chan_info, &phyerr.chan_info,
						sizeof(phyerr.chan_info));

			target_if_spectral_process_phyerr(wma->pdev, phyerr.bufp,
							phyerr.buf_len,
							&rfqual_info,
							&chan_info,
							phyerr.tsf64,
							&acs_stats);
		}
	}

	return status;
}
#else
static QDF_STATUS
wma_extract_single_phyerr_spectral(void *handle, void *evt_buf,
				   uint16_t datalen,
				   uint16_t *buf_offset,
				   wmi_host_phyerr_t *phyerr)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS spectral_phyerr_event_handler(void *handle,
					uint8_t *data, uint32_t datalen)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dfs_phyerr_event_handler() - dfs phyerr event handler
 * @handle: wma handle
 * @data: data buffer
 * @datalen: buffer length
 * @fulltsf: 64 bit event TSF
 *
 * Function to process DFS phy errors.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dfs_phyerr_event_handler(tp_wma_handle handle,
					   uint8_t *data,
					   uint32_t datalen,
					   uint64_t fulltsf)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
	wmi_host_phyerr_t phyerr;
	int8_t rssi_comb;
	uint16_t buf_offset;

	if (!handle->psoc) {
		wma_err("psoc is null");
		return QDF_STATUS_E_INVAL;
	}

	dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(handle->psoc);
	if (!dfs_rx_ops) {
		wma_err("dfs_rx_ops is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!dfs_rx_ops->dfs_process_phyerr) {
		wma_err("dfs_process_phyerr handler is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!handle->pdev) {
		wma_err("pdev is null");
		return -EINVAL;
	}

	buf_offset = 0;
	while (buf_offset < datalen) {
		status = wmi_extract_single_phyerr(handle->wmi_handle, data, datalen,
						   &buf_offset, &phyerr);
		if (QDF_IS_STATUS_ERROR(status)) {
			/* wmi_extract_single_phyerr has logs */
			return status;
		}

		rssi_comb = phyerr.rf_info.rssi_comb & 0xFF;
		if (phyerr.buf_len > 0)
			dfs_rx_ops->dfs_process_phyerr(handle->pdev,
						       &phyerr.bufp[0],
						       phyerr.buf_len,
						       rssi_comb,
						       rssi_comb,
						       phyerr.tsf_timestamp,
						       fulltsf);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_unified_phyerr_rx_event_handler() - phyerr event handler
 * @handle: wma handle
 * @data: data buffer
 * @datalen: buffer length
 *
 * WMI Handler for WMI_PHYERR_EVENTID event from firmware.
 * This handler is currently handling DFS and spectral scan
 * phy errors.
 *
 * Return: 0 for success, other value for failure
 */
static int wma_unified_phyerr_rx_event_handler(void *handle,
					       uint8_t *data,
					       uint32_t datalen)
{
	/* phyerr handling is moved to cmn project
	 * As WIN still uses handler registration in non-cmn code.
	 * need complete testing of non offloaded DFS code before we enable
	 * it in cmn code.
	 **/
	tp_wma_handle wma = (tp_wma_handle) handle;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	wmi_host_phyerr_t phyerr;
	uint16_t buf_offset = 0;
	wmi_single_phyerr_rx_event *ev;
	uint16_t event_buf_len = 0;
	wmi_host_phyerr_t phyerr2;
	bool spectralscan = false;

	if (!wma) {
		wma_err("wma handle is null");
		return -EINVAL;
	}

	/* sanity check on data length */
	status = wmi_extract_comb_phyerr(wma->wmi_handle, data, datalen,
					 &buf_offset, &phyerr);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("extract phyerr failed: %d", status);
		return qdf_status_to_os_return(status);
	}
	ev = (wmi_single_phyerr_rx_event *)phyerr.bufp;
	event_buf_len = phyerr.buf_len;
	/* Loop over the bufp, extracting out phyerrors */
	buf_offset = 0;
	while (ev && (buf_offset < event_buf_len)) {
		if (wma_extract_single_phyerr_spectral(handle, ev,
						       event_buf_len,
						       &buf_offset,
						       &phyerr2)) {
			wma_err("extract single phy err failed");
			return qdf_status_to_os_return(QDF_STATUS_E_FAILURE);
		}
		if ((buf_offset != 0) && (phyerr2.phy_err_code == 0x26 ||
					  phyerr2.phy_err_code == 0x24)) {
			spectralscan = true;
		} else {
			break;
		}
	}
	if (spectralscan) {
		status = spectral_phyerr_event_handler(wma, data, datalen);
		return qdf_status_to_os_return(status);
	}
	/* handle different PHY Error conditions */
	if (((phyerr.phy_err_mask0 & (WMI_PHY_ERROR_MASK0_RADAR |
	    WMI_PHY_ERROR_MASK0_FALSE_RADAR_EXT |
	    WMI_PHY_ERROR_MASK0_SPECTRAL_SCAN)) == 0)) {
		wma_debug("Unknown phy error event");
		return -EINVAL;
	}

	/* Handle Spectral or DFS PHY Error */
	if (phyerr.phy_err_mask0 & (WMI_PHY_ERROR_MASK0_RADAR |
	    WMI_PHY_ERROR_MASK0_FALSE_RADAR_EXT)) {
		if (wma->is_dfs_offloaded) {
			wma_debug("Unexpected phy error, dfs offloaded");
			return -EINVAL;
		}
		status = dfs_phyerr_event_handler(wma,
						  phyerr.bufp,
						  phyerr.buf_len,
						  phyerr.tsf64);
	} else if (phyerr.phy_err_mask0 & (WMI_PHY_ERROR_MASK0_SPECTRAL_SCAN |
		   WMI_PHY_ERROR_MASK0_FALSE_RADAR_EXT)) {
		status = spectral_phyerr_event_handler(wma, data, datalen);
	}

	return qdf_status_to_os_return(status);
}

void wma_vdev_init(struct wma_txrx_node *vdev)
{
	qdf_wake_lock_create(&vdev->vdev_set_key_wakelock, "vdev_set_key");
	qdf_runtime_lock_init(&vdev->vdev_set_key_runtime_wakelock);
	vdev->is_waiting_for_key = false;
}

void wma_vdev_deinit(struct wma_txrx_node *vdev)
{
	struct beacon_info *bcn;
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	/* validate the wma_handle */
	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return;
	}

	bcn = vdev->beacon;
	if (bcn) {
		if (bcn->dma_mapped)
			qdf_nbuf_unmap_single(wma_handle->qdf_dev,
				bcn->buf, QDF_DMA_TO_DEVICE);
		qdf_nbuf_free(bcn->buf);
		qdf_mem_free(bcn);
		vdev->beacon = NULL;
	}

	if (vdev->vdev_active == true)
		vdev->vdev_active = false;

	if (vdev->addBssStaContext) {
		qdf_mem_free(vdev->addBssStaContext);
		vdev->addBssStaContext = NULL;
	}

	if (vdev->psnr_req) {
		qdf_mem_free(vdev->psnr_req);
		vdev->psnr_req = NULL;
	}

	if (vdev->rcpi_req) {
		qdf_mem_free(vdev->rcpi_req);
		vdev->rcpi_req = NULL;
	}

	if (vdev->roam_scan_stats_req) {
		struct sir_roam_scan_stats *req;

		req = vdev->roam_scan_stats_req;
		vdev->roam_scan_stats_req = NULL;
		qdf_mem_free(req);
	}

	if (vdev->roam_synch_frame_ind.bcn_probe_rsp) {
		qdf_mem_free(vdev->roam_synch_frame_ind.bcn_probe_rsp);
		vdev->roam_synch_frame_ind.bcn_probe_rsp = NULL;
	}

	if (vdev->roam_synch_frame_ind.reassoc_req) {
		qdf_mem_free(vdev->roam_synch_frame_ind.reassoc_req);
		vdev->roam_synch_frame_ind.reassoc_req = NULL;
	}

	if (vdev->roam_synch_frame_ind.reassoc_rsp) {
		qdf_mem_free(vdev->roam_synch_frame_ind.reassoc_rsp);
		vdev->roam_synch_frame_ind.reassoc_rsp = NULL;
	}

	if (vdev->plink_status_req) {
		qdf_mem_free(vdev->plink_status_req);
		vdev->plink_status_req = NULL;
	}

	qdf_runtime_lock_deinit(&vdev->vdev_set_key_runtime_wakelock);
	qdf_wake_lock_destroy(&vdev->vdev_set_key_wakelock);
	vdev->is_waiting_for_key = false;
}

/**
 * wma_wmi_stop() - generic function to block WMI commands
 * @return: None
 */
void wma_wmi_stop(void)
{
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if ((!wma_handle) || (!wma_handle->wmi_handle)) {
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_INFO,
			  "wma_handle or wmi_handle is NULL\n");
		return;
	}
	wmi_stop(wma_handle->wmi_handle);
}

#ifdef WLAN_WMI_BCN
static QDF_STATUS
wma_register_swba_events(wmi_unified_t wmi_handle)
{
	QDF_STATUS status;

	status = wmi_unified_register_event_handler(wmi_handle,
						    wmi_host_swba_event_id,
						    wma_beacon_swba_handler,
						    WMA_RX_SERIALIZER_CTX);

	return status;
}
#else
static QDF_STATUS wma_register_swba_events(wmi_unified_t wmi_handle)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef FEATURE_WLAN_APF
static void wma_register_apf_events(tp_wma_handle wma_handle)
{
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_INFO,
			  "wma_handle is NULL\n");
		return;
	}

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_apf_capability_info_event_id,
					   wma_get_apf_caps_event_handler,
					   WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_apf_get_vdev_work_memory_resp_event_id,
				wma_apf_read_work_memory_event_handler,
				WMA_RX_SERIALIZER_CTX);
}
#else /* FEATURE_WLAN_APF */
static void wma_register_apf_events(tp_wma_handle wma_handle)
{
}
#endif /* FEATURE_WLAN_APF */

#ifdef WLAN_FEATURE_MOTION_DETECTION
/**
 * wma_register_md_events - Register motion detection event handlers
 * @wma_handle: wma handle
 * Return: None
 */
static void wma_register_md_events(tp_wma_handle wma_handle)
{
	if (!wma_handle) {
		QDF_TRACE(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_INFO,
			  "wma_handle is NULL\n");
		return;
	}

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_motion_det_host_eventid,
					   wma_motion_det_host_event_handler,
					   WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(
				wma_handle->wmi_handle,
				wmi_motion_det_base_line_host_eventid,
				wma_motion_det_base_line_host_event_handler,
				WMA_RX_SERIALIZER_CTX);
}
#else /* WLAN_FEATURE_MOTION_DETECTION */
/**
 * wma_register_md_events - Register motion detection event handlers
 * @wma_handle: wma handle
 * Return: None
 */
static void wma_register_md_events(tp_wma_handle wma_handle)
{
}
#endif /* WLAN_FEATURE_MOTION_DETECTION */

#ifdef FEATURE_WLM_STATS
static void wma_register_wlm_stats_events(tp_wma_handle wma_handle)
{
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_wlm_stats_event_id,
					   wma_wlm_stats_rsp,
					   WMA_RX_SERIALIZER_CTX);
}
#else /* FEATURE_WLM_STATS */
static void wma_register_wlm_stats_events(tp_wma_handle wma_handle)
{
}
#endif /* FEATURE_WLM_STATS */

struct wlan_objmgr_psoc *wma_get_psoc_from_scn_handle(void *scn_handle)
{
	tp_wma_handle wma_handle;

	if (!scn_handle) {
		wma_err("invalid scn handle");
		return NULL;
	}
	wma_handle = (tp_wma_handle)scn_handle;

	return wma_handle->psoc;
}

void wma_get_fw_phy_mode_for_freq_cb(uint32_t freq, uint32_t chan_width,
				     uint32_t *phy_mode)
{
	uint32_t dot11_mode;
	enum wlan_phymode host_phy_mode;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("MAC context is NULL");
		*phy_mode = WLAN_PHYMODE_AUTO;
		return;
	}

	dot11_mode = mac->mlme_cfg->dot11_mode.dot11_mode;

	/* Update invalid dot11 modes to valid dot11 modes */
	if (WLAN_REG_IS_24GHZ_CH_FREQ(freq) &&
	    dot11_mode == MLME_DOT11_MODE_11A)
		dot11_mode = MLME_DOT11_MODE_11G;

	if (WLAN_REG_IS_5GHZ_CH_FREQ(freq) &&
	    (dot11_mode == MLME_DOT11_MODE_11B ||
	     dot11_mode == MLME_DOT11_MODE_11G ||
	     dot11_mode == MLME_DOT11_MODE_11G_ONLY))
		dot11_mode = MLME_DOT11_MODE_11A;

	host_phy_mode = wma_chan_phy_mode(freq, chan_width, dot11_mode);
	*phy_mode = wma_host_to_fw_phymode(host_phy_mode);
}

void wma_get_phy_mode_cb(qdf_freq_t freq, uint32_t chan_width,
			 enum wlan_phymode *phy_mode)
{
	uint32_t dot11_mode;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("MAC context is NULL");
		*phy_mode = WLAN_PHYMODE_AUTO;
		return;
	}

	dot11_mode = mac->mlme_cfg->dot11_mode.dot11_mode;
	*phy_mode = wma_chan_phy_mode(freq, chan_width, dot11_mode);
}

#ifdef WLAN_FEATURE_NAN
static void
wma_register_nan_callbacks(tp_wma_handle wma_handle)
{
	struct nan_callbacks cb_obj = {0};

	cb_obj.update_ndi_conn = wma_ndi_update_connection_info;

	ucfg_nan_register_wma_callbacks(wma_handle->psoc, &cb_obj);
}
#else
static void wma_register_nan_callbacks(tp_wma_handle wma_handle)
{
}
#endif

#ifdef WLAN_FEATURE_PKT_CAPTURE
static void
wma_register_pkt_capture_callbacks(tp_wma_handle wma_handle)
{
	struct pkt_capture_callbacks cb_obj = {0};

	cb_obj.get_rmf_status = wma_get_rmf_status;

	ucfg_pkt_capture_register_wma_callbacks(wma_handle->psoc, &cb_obj);
}
#else
static inline void
wma_register_pkt_capture_callbacks(tp_wma_handle wma_handle)
{
}
#endif

#ifdef TRACE_RECORD
static void wma_trace_dump(void *mac_ctx, tp_qdf_trace_record record,
			   uint16_t rec_index)
{
	/*
	 * This is dummy handler registered to qdf_trace as wma module wants to
	 * insert trace records in qdf trace global record table but qdf_trace
	 * does not allow to insert the trace records in the global record
	 * table if a module is not registered with the qdf trace.
	 */
}

static void wma_trace_init(void)
{
	qdf_trace_register(QDF_MODULE_ID_WMA, &wma_trace_dump);
}
#else
static inline void wma_trace_init(void)
{
}
#endif

#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
static void wma_get_service_cap_club_get_sta_in_ll_stats_req(
					struct wmi_unified *wmi_handle,
					struct wma_tgt_services *cfg)
{
	cfg->is_get_station_clubbed_in_ll_stats_req =
		wmi_service_enabled(wmi_handle,
				    wmi_service_get_station_in_ll_stats_req);
}
#else
static void wma_get_service_cap_club_get_sta_in_ll_stats_req(
					struct wmi_unified *wmi_handle,
					struct wma_tgt_services *cfg)
{
}
#endif /* FEATURE_CLUB_LL_STATS_AND_GET_STATION */

/**
 * wma_open() - Allocate wma context and initialize it.
 * @cds_context:  cds context
 * @wma_tgt_cfg_cb: tgt config callback fun
 * @radar_ind_cb: dfs radar indication callback
 * @cds_cfg:  mac parameters
 *
 * Return: 0 on success, errno on failure
 */
QDF_STATUS wma_open(struct wlan_objmgr_psoc *psoc,
		    wma_tgt_cfg_cb tgt_cfg_cb,
		    struct cds_config_info *cds_cfg,
		    uint32_t target_type)
{
	tp_wma_handle wma_handle;
	HTC_HANDLE htc_handle;
	qdf_device_t qdf_dev;
	void *wmi_handle;
	QDF_STATUS qdf_status;
	struct wmi_unified_attach_params *params;
	struct policy_mgr_wma_cbacks wma_cbacks;
	struct target_psoc_info *tgt_psoc_info;
	int i;
	bool val = 0;
	void *cds_context;
	target_resource_config *wlan_res_cfg;
	uint32_t self_gen_frm_pwr = 0;

	wma_debug("Enter");

	cds_context = cds_get_global_context();
	if (!cds_context) {
		wma_err("Invalid CDS context");
		return QDF_STATUS_E_INVAL;
	}

	g_wmi_version_info.major = __WMI_VER_MAJOR_;
	g_wmi_version_info.minor = __WMI_VER_MINOR_;
	g_wmi_version_info.revision = __WMI_REVISION_;

	qdf_dev = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);
	htc_handle = cds_get_context(QDF_MODULE_ID_HTC);

	if (!htc_handle) {
		wma_err("Invalid HTC handle");
		return QDF_STATUS_E_INVAL;
	}

	/* Alloc memory for WMA Context */
	qdf_status = cds_alloc_context(QDF_MODULE_ID_WMA,
				       (void **)&wma_handle,
				       sizeof(*wma_handle));

	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("Memory allocation failed for wma_handle");
		return qdf_status;
	}

	qdf_mem_zero(wma_handle, sizeof(t_wma_handle));

	if (target_if_alloc_psoc_tgt_info(psoc)) {
		wma_err("target psoc info allocation failed");
		qdf_status = QDF_STATUS_E_NOMEM;
		goto err_free_wma_handle;
	}

	if (cds_get_conparam() != QDF_GLOBAL_FTM_MODE) {
#ifdef FEATURE_WLAN_EXTSCAN
		qdf_wake_lock_create(&wma_handle->extscan_wake_lock,
					"wlan_extscan_wl");
#endif /* FEATURE_WLAN_EXTSCAN */
		qdf_wake_lock_create(&wma_handle->wow_wake_lock,
			"wlan_wow_wl");
		qdf_wake_lock_create(&wma_handle->wow_auth_req_wl,
			"wlan_auth_req_wl");
		qdf_wake_lock_create(&wma_handle->wow_assoc_req_wl,
			"wlan_assoc_req_wl");
		qdf_wake_lock_create(&wma_handle->wow_deauth_rec_wl,
			"wlan_deauth_rec_wl");
		qdf_wake_lock_create(&wma_handle->wow_disassoc_rec_wl,
			"wlan_disassoc_rec_wl");
		qdf_wake_lock_create(&wma_handle->wow_ap_assoc_lost_wl,
			"wlan_ap_assoc_lost_wl");
		qdf_wake_lock_create(&wma_handle->wow_auto_shutdown_wl,
			"wlan_auto_shutdown_wl");
		qdf_wake_lock_create(&wma_handle->roam_ho_wl,
			"wlan_roam_ho_wl");
		qdf_wake_lock_create(&wma_handle->roam_preauth_wl,
				     "wlan_roam_preauth_wl");
		qdf_wake_lock_create(&wma_handle->probe_req_wps_wl,
				     "wlan_probe_req_wps_wl");
	}

	qdf_status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_LEGACY_WMA_ID);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		wma_err("PSOC get_ref fails");
		goto err_get_psoc_ref;
	}
	wma_handle->psoc = psoc;

	wma_target_if_open(wma_handle);

	/*
	 * Allocate locally used params with its rx_ops member,
	 * and free it immediately after used.
	 */
	params = qdf_mem_malloc(sizeof(*params));
	if (!params) {
		qdf_status = QDF_STATUS_E_NOMEM;
		goto err_wma_handle;
	}

	params->osdev = NULL;
	params->target_type = WMI_TLV_TARGET;
	params->use_cookie = false;
	params->psoc = psoc;
	params->max_commands = WMI_MAX_CMDS;

	/* initialize tlv attach */
	wmi_tlv_init();

	/* attach the wmi */
	wmi_handle = wmi_unified_attach(wma_handle, params);
	qdf_mem_free(params);
	if (!wmi_handle) {
		wma_err("failed to attach WMI");
		qdf_status = QDF_STATUS_E_NOMEM;
		goto err_wma_handle;
	}

	target_if_register_legacy_service_ready_cb(
					wma_legacy_service_ready_event_handler);

	wma_nofl_alert("WMA --> wmi_unified_attach - success");

	/* store the wmi handle in tgt_if_handle */
	tgt_psoc_info = wlan_psoc_get_tgt_if_handle(psoc);

	target_psoc_set_target_type(tgt_psoc_info, target_type);
	/* Save the WMI & HTC handle */
	target_psoc_set_wmi_hdl(tgt_psoc_info, wmi_handle);
	wma_handle->wmi_handle = wmi_handle;
	target_psoc_set_htc_hdl(tgt_psoc_info, htc_handle);
	wma_handle->cds_context = cds_context;
	wma_handle->qdf_dev = qdf_dev;
	wma_handle->enable_tx_compl_tsf64 =
			cds_cfg->enable_tx_compl_tsf64;

	/* Register Converged Event handlers */
	init_deinit_register_tgt_psoc_ev_handlers(psoc);

	/* Initialize max_no_of_peers for wma_get_number_of_peers_supported() */
	cds_cfg->max_station = wma_init_max_no_of_peers(wma_handle,
							cds_cfg->max_station);

	wlan_mlme_set_assoc_sta_limit(psoc, cds_cfg->max_station);

	/* initialize default target config */
	wlan_res_cfg = target_psoc_get_wlan_res_cfg(tgt_psoc_info);
	if (!wlan_res_cfg) {
		wma_err("wlan_res_cfg is null");
		qdf_status = QDF_STATUS_E_NOMEM;
		goto err_wma_handle;
	}

	wma_set_default_tgt_config(wma_handle, wlan_res_cfg, cds_cfg);

	qdf_status = wlan_mlme_get_tx_chainmask_cck(psoc, &val);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to get tx_chainmask_cck");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto err_wma_handle;
	}
	wma_handle->tx_chain_mask_cck = val;

	qdf_status = wlan_mlme_get_self_gen_frm_pwr(psoc, &self_gen_frm_pwr);
	if (qdf_status != QDF_STATUS_SUCCESS)
		wma_err("Failed to get self_gen_frm_pwr");
	wma_handle->self_gen_frm_pwr = self_gen_frm_pwr;

	cds_cfg->max_bssid = WLAN_MAX_VDEVS;

	wma_handle->max_station = cds_cfg->max_station;
	wma_handle->max_bssid = cds_cfg->max_bssid;
	wma_handle->enable_mc_list =
		ucfg_pmo_is_mc_addr_list_enabled(wma_handle->psoc);
	wma_handle->active_uc_apf_mode =
		ucfg_pmo_get_active_uc_apf_mode(wma_handle->psoc);
	wma_handle->active_mc_bc_apf_mode =
		ucfg_pmo_get_active_mc_bc_apf_mode(wma_handle->psoc);
	wma_handle->link_stats_results = NULL;
#ifdef WLAN_FEATURE_LPSS
	wma_handle->is_lpass_enabled = cds_cfg->is_lpass_enabled;
#endif
	wma_handle->interfaces = qdf_mem_malloc(sizeof(struct wma_txrx_node) *
						wma_handle->max_bssid);
	if (!wma_handle->interfaces) {
		qdf_status = QDF_STATUS_E_NOMEM;
		goto err_scn_context;
	}

	for (i = 0; i < wma_handle->max_bssid; ++i)
		wma_vdev_init(&wma_handle->interfaces[i]);

	/* Register the debug print event handler */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					wmi_debug_print_event_id,
					wma_unified_debug_print_event_handler,
					WMA_RX_SERIALIZER_CTX);
	/* Register profiling event Handler */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					wmi_wlan_profile_data_event_id,
					wma_profile_data_report_event_handler,
					WMA_RX_SERIALIZER_CTX);

	wma_handle->tgt_cfg_update_cb = tgt_cfg_cb;
	wma_handle->old_hw_mode_index = WMA_DEFAULT_HW_MODE_INDEX;
	wma_handle->new_hw_mode_index = WMA_DEFAULT_HW_MODE_INDEX;
	wma_handle->saved_chan.num_channels = 0;
	wma_handle->fw_timeout_crash = cds_cfg->fw_timeout_crash;

	qdf_status = qdf_mc_timer_init(&wma_handle->service_ready_ext_timer,
					QDF_TIMER_TYPE_SW,
					wma_service_ready_ext_evt_timeout,
					wma_handle);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		wma_err("Failed to initialize service ready ext timeout");
		goto err_event_init;
	}

	qdf_status = qdf_event_create(&wma_handle->target_suspend);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("target suspend event initialization failed");
		goto err_event_init;
	}

	/* Init Tx Frame Complete event */
	qdf_status = qdf_event_create(&wma_handle->tx_frm_download_comp_event);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		wma_err("failed to init tx_frm_download_comp_event");
		goto err_event_init;
	}

	/* Init tx queue empty check event */
	qdf_status = qdf_event_create(&wma_handle->tx_queue_empty_event);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		wma_err("failed to init tx_queue_empty_event");
		goto err_event_init;
	}

	qdf_status = cds_shutdown_notifier_register(wma_shutdown_notifier_cb,
						    wma_handle);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("Shutdown notifier register failed: %d", qdf_status);
		goto err_event_init;
	}

	qdf_status = qdf_event_create(&wma_handle->runtime_suspend);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("runtime_suspend event initialization failed");
		goto err_event_init;
	}

	qdf_status = qdf_event_create(&wma_handle->recovery_event);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("recovery event initialization failed");
		goto err_event_init;
	}

	qdf_list_create(&wma_handle->wma_hold_req_queue,
		      MAX_ENTRY_HOLD_REQ_QUEUE);
	qdf_spinlock_create(&wma_handle->wma_hold_req_q_lock);
	qdf_atomic_init(&wma_handle->is_wow_bus_suspended);

	/* register for STA kickout function */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_peer_sta_kickout_event_id,
					   wma_peer_sta_kickout_event_handler,
					   WMA_RX_SERIALIZER_CTX);

	/* register for stats response event */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_get_arp_stats_req_id,
					   wma_get_arp_stats_handler,
					   WMA_RX_SERIALIZER_CTX);

	/* register for fw state response event */
	wma_register_fw_state_events(wma_handle->wmi_handle);

#ifdef WLAN_POWER_DEBUG
	/* register for Chip Power stats event */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_pdev_chip_power_stats_event_id,
				wma_unified_power_debug_stats_event_handler,
				WMA_RX_SERIALIZER_CTX);
#endif
#ifdef WLAN_FEATURE_BEACON_RECEPTION_STATS
	/* register for beacon stats event */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_vdev_bcn_reception_stats_event_id,
				wma_unified_beacon_debug_stats_event_handler,
				WMA_RX_SERIALIZER_CTX);
#endif

#if defined(CLD_PM_QOS) && defined(WLAN_FEATURE_LL_MODE)
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_vdev_bcn_latency_event_id,
					   wma_vdev_bcn_latency_event_handler,
					   WMA_RX_SERIALIZER_CTX);
#endif
	/* register for linkspeed response event */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_peer_estimated_linkspeed_event_id,
					   wma_link_speed_event_handler,
					   WMA_RX_SERIALIZER_CTX);

#ifdef FEATURE_OEM_DATA_SUPPORT
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_oem_response_event_id,
					   wma_oem_data_response_handler,
					   WMA_RX_SERIALIZER_CTX);
#endif /* FEATURE_OEM_DATA_SUPPORT */

	/* Register peer change event handler */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_peer_state_event_id,
					   wma_peer_state_change_event_handler,
					   WMA_RX_WORK_CTX);

	/* Register beacon tx complete event id. The event is required
	 * for sending channel switch announcement frames
	 */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					wmi_offload_bcn_tx_status_event_id,
					wma_unified_bcntx_status_event_handler,
					WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_update_vdev_rate_stats_event_id,
					   wma_link_status_event_handler,
					   WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_roam_scan_stats_event_id,
					   wma_roam_scan_stats_event_handler,
					   WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_pdev_cold_boot_cal_event_id,
					   wma_cold_boot_cal_event_handler,
					   WMA_RX_WORK_CTX);

#ifdef FEATURE_OEM_DATA
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_oem_data_event_id,
					   wma_oem_event_handler,
					   WMA_RX_WORK_CTX);
#endif

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
	/* Register event handler for processing Link Layer Stats
	 * response from the FW
	 */
	wma_register_ll_stats_event_handler(wma_handle);

#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

	wmi_set_tgt_assert(wma_handle->wmi_handle,
			   cds_cfg->force_target_assert_enabled);
	/* Firmware debug log */
	qdf_status = dbglog_init(wma_handle->wmi_handle);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("Firmware Dbglog initialization failed");
		goto err_dbglog_init;
	}

	/*
	 * Update Powersave mode
	 * 1 - Legacy Powersave + Deepsleep Disabled
	 * 2 - QPower + Deepsleep Disabled
	 * 3 - Legacy Powersave + Deepsleep Enabled
	 * 4 - QPower + Deepsleep Enabled
	 */
	wma_handle->powersave_mode =
			ucfg_pmo_power_save_offload_enabled(wma_handle->psoc);
	wma_handle->staMaxLIModDtim = cds_cfg->sta_maxlimod_dtim;
	wma_handle->staModDtim = ucfg_pmo_get_sta_mod_dtim(wma_handle->psoc);
	wma_handle->staDynamicDtim =
			ucfg_pmo_get_sta_dynamic_dtim(wma_handle->psoc);

	/* register for install key completion event */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_vdev_install_key_complete_event_id,
				wma_vdev_install_key_complete_event_handler,
				WMA_RX_SERIALIZER_CTX);

#ifdef WLAN_FEATURE_STATS_EXT
	/* register for extended stats event */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_stats_ext_event_id,
					   wma_stats_ext_event_handler,
					   WMA_RX_SERIALIZER_CTX);
#endif /* WLAN_FEATURE_STATS_EXT */
#ifdef FEATURE_WLAN_EXTSCAN
	wma_register_extscan_event_handler(wma_handle);
#endif /* WLAN_FEATURE_STATS_EXT */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_roam_synch_event_id,
					   wma_roam_synch_event_handler,
					   WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				   wmi_roam_synch_frame_event_id,
				   wma_roam_synch_frame_event_handler,
				   WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_roam_auth_offload_event_id,
					   wma_roam_auth_offload_event_handler,
					   WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_roam_stats_event_id,
					   wma_roam_stats_event_handler,
					   WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_roam_scan_chan_list_id,
					   wma_roam_scan_chan_list_event_handler,
					   WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_roam_blacklist_event_id,
					   wma_handle_btm_blacklist_event,
					   WMA_RX_SERIALIZER_CTX);

	wma_register_pmkid_req_event_handler(wma_handle);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					wmi_vdev_disconnect_event_id,
					wma_roam_vdev_disconnect_event_handler,
					WMA_RX_SERIALIZER_CTX);

#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_rssi_breach_event_id,
				wma_rssi_breached_event_handler,
				WMA_RX_SERIALIZER_CTX);

	qdf_wake_lock_create(&wma_handle->wmi_cmd_rsp_wake_lock,
					"wlan_fw_rsp_wakelock");
	qdf_runtime_lock_init(&wma_handle->wmi_cmd_rsp_runtime_lock);
	qdf_runtime_lock_init(&wma_handle->sap_prevent_runtime_pm_lock);

	/* Register peer assoc conf event handler */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_peer_assoc_conf_event_id,
					   wma_peer_assoc_conf_handler,
					   WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_peer_create_conf_event_id,
					   wma_peer_create_confirm_handler,
					   WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_peer_delete_response_event_id,
					   wma_peer_delete_handler,
					   WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_chan_info_event_id,
					   wma_chan_info_event_handler,
					   WMA_RX_SERIALIZER_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_dbg_mesg_flush_complete_event_id,
				wma_flush_complete_evt_handler,
				WMA_RX_WORK_CTX);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_report_rx_aggr_failure_event_id,
				wma_rx_aggr_failure_event_handler,
				WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(
				wma_handle->wmi_handle,
				wmi_coex_report_antenna_isolation_event_id,
				wma_antenna_isolation_event_handler,
				WMA_RX_SERIALIZER_CTX);

	wma_handle->ito_repeat_count = cds_cfg->ito_repeat_count;
	wma_handle->bandcapability = cds_cfg->bandcapability;

	/* Register PWR_SAVE_FAIL event only in case of recovery(1) */
	if (ucfg_pmo_get_auto_power_fail_mode(wma_handle->psoc) ==
	    PMO_FW_TO_SEND_WOW_IND_ON_PWR_FAILURE) {
		wmi_unified_register_event_handler(wma_handle->wmi_handle,
			wmi_pdev_chip_pwr_save_failure_detect_event_id,
			wma_chip_power_save_failure_detected_handler,
			WMA_RX_WORK_CTX);
	}

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_pdev_div_rssi_antid_event_id,
				wma_pdev_div_info_evt_handler,
				WMA_RX_WORK_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   wmi_get_ani_level_event_id,
					   wma_get_ani_level_evt_handler,
					   WMA_RX_WORK_CTX);

	wma_register_debug_callback();
	wifi_pos_register_get_phy_mode_cb(wma_handle->psoc,
					  wma_get_phy_mode_cb);
	wifi_pos_register_get_fw_phy_mode_for_freq_cb(
					wma_handle->psoc,
					wma_get_fw_phy_mode_for_freq_cb);

	/* Register callback with PMO so PMO can update the vdev pause bitmap*/
	pmo_register_pause_bitmap_notifier(wma_handle->psoc,
		wma_vdev_update_pause_bitmap);
	pmo_register_get_pause_bitmap(wma_handle->psoc,
		wma_vdev_get_pause_bitmap);
	pmo_register_is_device_in_low_pwr_mode(wma_handle->psoc,
		wma_vdev_is_device_in_low_pwr_mode);
	pmo_register_get_dtim_period_callback(wma_handle->psoc,
					      wma_vdev_get_dtim_period);
	pmo_register_get_beacon_interval_callback(wma_handle->psoc,
						  wma_vdev_get_beacon_interval);
	wma_cbacks.wma_get_connection_info = wma_get_connection_info;
	wma_register_nan_callbacks(wma_handle);
	wma_register_pkt_capture_callbacks(wma_handle);
	qdf_status = policy_mgr_register_wma_cb(wma_handle->psoc, &wma_cbacks);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		wma_err("Failed to register wma cb with Policy Manager");
	}

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			wmi_phyerr_event_id,
			wma_unified_phyerr_rx_event_handler,
			WMA_RX_WORK_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			wmi_sap_obss_detection_report_event_id,
			wma_vdev_obss_detection_info_handler,
			WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			wmi_obss_color_collision_report_event_id,
			wma_vdev_bss_color_collision_info_handler,
			WMA_RX_WORK_CTX);

	wma_register_twt_events(wma_handle);

	wma_register_apf_events(wma_handle);
	wma_register_md_events(wma_handle);
	wma_register_wlm_stats_events(wma_handle);
	wma_register_mws_coex_events(wma_handle);
	wma_trace_init();
	return QDF_STATUS_SUCCESS;

err_dbglog_init:
	qdf_wake_lock_destroy(&wma_handle->wmi_cmd_rsp_wake_lock);
	qdf_runtime_lock_deinit(&wma_handle->sap_prevent_runtime_pm_lock);
	qdf_runtime_lock_deinit(&wma_handle->wmi_cmd_rsp_runtime_lock);
	qdf_spinlock_destroy(&wma_handle->wma_hold_req_q_lock);
err_event_init:
	wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
					     wmi_debug_print_event_id);

	for (i = 0; i < wma_handle->max_bssid; ++i)
		wma_vdev_deinit(&wma_handle->interfaces[i]);

	qdf_mem_free(wma_handle->interfaces);

err_scn_context:
	qdf_mem_free(((struct cds_context *) cds_context)->cfg_ctx);
	((struct cds_context *)cds_context)->cfg_ctx = NULL;
	qdf_mem_free(wmi_handle);

err_wma_handle:
	wlan_objmgr_psoc_release_ref(psoc, WLAN_LEGACY_WMA_ID);
err_get_psoc_ref:
	target_if_free_psoc_tgt_info(psoc);
	if (cds_get_conparam() != QDF_GLOBAL_FTM_MODE) {
#ifdef FEATURE_WLAN_EXTSCAN
		qdf_wake_lock_destroy(&wma_handle->extscan_wake_lock);
#endif /* FEATURE_WLAN_EXTSCAN */
		qdf_wake_lock_destroy(&wma_handle->wow_wake_lock);
		qdf_wake_lock_destroy(&wma_handle->wow_auth_req_wl);
		qdf_wake_lock_destroy(&wma_handle->wow_assoc_req_wl);
		qdf_wake_lock_destroy(&wma_handle->wow_deauth_rec_wl);
		qdf_wake_lock_destroy(&wma_handle->wow_disassoc_rec_wl);
		qdf_wake_lock_destroy(&wma_handle->wow_ap_assoc_lost_wl);
		qdf_wake_lock_destroy(&wma_handle->wow_auto_shutdown_wl);
		qdf_wake_lock_destroy(&wma_handle->roam_ho_wl);
		qdf_wake_lock_destroy(&wma_handle->roam_preauth_wl);
		qdf_wake_lock_destroy(&wma_handle->probe_req_wps_wl);
	}
err_free_wma_handle:
	cds_free_context(QDF_MODULE_ID_WMA, wma_handle);

	wma_debug("Exit");

	return qdf_status;
}

/**
 * wma_pre_start() - wma pre start
 *
 * Return: 0 on success, errno on failure
 */
QDF_STATUS wma_pre_start(void)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma_handle;
	void *htc_handle;

	wma_debug("Enter");

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	/* Validate the wma_handle */
	if (!wma_handle) {
		wma_err("invalid wma handle");
		qdf_status = QDF_STATUS_E_INVAL;
		goto end;
	}

	htc_handle = lmac_get_htc_hdl(wma_handle->psoc);
	if (!htc_handle) {
		wma_err("invalid htc handle");
		qdf_status = QDF_STATUS_E_INVAL;
		goto end;
	}

	/* Open endpoint for ctrl path - WMI <--> HTC */
	qdf_status = wmi_unified_connect_htc_service(wma_handle->wmi_handle,
						     htc_handle);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("wmi_unified_connect_htc_service");
		if (!cds_is_fw_down())
			QDF_BUG(0);

		qdf_status = QDF_STATUS_E_FAULT;
		goto end;
	}

	/* Open endpoint for wmi diag path */
	qdf_status = wmi_diag_connect_pdev_htc_service(wma_handle->wmi_handle,
						       htc_handle);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("wmi_diag_connect_pdev_htc_service");
		if (!cds_is_fw_down())
			QDF_BUG(0);

		qdf_status = QDF_STATUS_E_FAULT;
		goto end;
	}

	wma_debug("WMA --> wmi_unified_connect_htc_service - success");

end:
	wma_debug("Exit");
	return qdf_status;
}

void wma_send_msg_by_priority(tp_wma_handle wma_handle, uint16_t msg_type,
		 void *body_ptr, uint32_t body_val, bool is_high_priority)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	msg.type = msg_type;
	msg.bodyval = body_val;
	msg.bodyptr = body_ptr;
	msg.flush_callback = wma_discard_fw_event;

	status = scheduler_post_msg_by_priority(QDF_MODULE_ID_PE,
					       &msg, is_high_priority);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		if (body_ptr)
			qdf_mem_free(body_ptr);
	}
}


void wma_send_msg(tp_wma_handle wma_handle, uint16_t msg_type,
			 void *body_ptr, uint32_t body_val)
{
	wma_send_msg_by_priority(wma_handle, msg_type,
				body_ptr, body_val, false);
}

void wma_send_msg_high_priority(tp_wma_handle wma_handle, uint16_t msg_type,
			 void *body_ptr, uint32_t body_val)
{
	wma_send_msg_by_priority(wma_handle, msg_type,
				body_ptr, body_val, true);
}

/**
 * wma_set_base_macaddr_indicate() - set base mac address in fw
 * @wma_handle: wma handle
 * @customAddr: base mac address
 *
 * Return: 0 for success or error code
 */
static int wma_set_base_macaddr_indicate(tp_wma_handle wma_handle,
					 tSirMacAddr *customAddr)
{
	int err;

	err = wmi_unified_set_base_macaddr_indicate_cmd(wma_handle->wmi_handle,
				     (uint8_t *)customAddr);
	if (err)
		return -EIO;
	wma_debug("Base MAC Addr: " QDF_MAC_ADDR_FMT,
		 QDF_MAC_ADDR_REF((*customAddr)));

	return 0;
}

/**
 * wma_log_supported_evt_handler() - Enable/Disable FW diag/log events
 * @handle: WMA handle
 * @event:  Event received from FW
 * @len:    Length of the event
 *
 * Enables the low frequency events and disables the high frequency
 * events. Bit 17 indicates if the event if low/high frequency.
 * 1 - high frequency, 0 - low frequency
 *
 * Return: 0 on successfully enabling/disabling the events
 */
static int wma_log_supported_evt_handler(void *handle,
		uint8_t *event,
		uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;

	if (wmi_unified_log_supported_evt_cmd(wma->wmi_handle,
				event, len))
		return -EINVAL;

	return 0;
}

/**
 * wma_pdev_set_hw_mode_resp_evt_handler() - Set HW mode resp evt handler
 * @handle: WMI handle
 * @event:  Event recevied from FW
 * @len:    Length of the event
 *
 * Event handler for WMI_PDEV_SET_HW_MODE_RESP_EVENTID that is sent to host
 * driver in response to a WMI_PDEV_SET_HW_MODE_CMDID being sent to WLAN
 * firmware
 *
 * Return: QDF_STATUS
 */
static int wma_pdev_set_hw_mode_resp_evt_handler(void *handle,
		uint8_t *event,
		uint32_t len)
{
	WMI_PDEV_SET_HW_MODE_RESP_EVENTID_param_tlvs *param_buf;
	wmi_pdev_set_hw_mode_response_event_fixed_param *wmi_event;
	wmi_pdev_set_hw_mode_response_vdev_mac_entry *vdev_mac_entry;
	uint32_t i;
	struct sir_set_hw_mode_resp *hw_mode_resp;
	tp_wma_handle wma = (tp_wma_handle) handle;

	if (!wma) {
		wma_err("Invalid WMA handle");
		/* Since WMA handle itself is NULL, we cannot send fail
		 * response back to LIM here
		 */
		return QDF_STATUS_E_NULL_VALUE;
	}

	wma_release_wakelock(&wma->wmi_cmd_rsp_wake_lock);
	wma_remove_req(wma, 0, WMA_PDEV_SET_HW_MODE_RESP);

	hw_mode_resp = qdf_mem_malloc(sizeof(*hw_mode_resp));
	if (!hw_mode_resp) {
		/* Since this memory allocation itself failed, we cannot
		 * send fail response back to LIM here
		 */
		return QDF_STATUS_E_NULL_VALUE;
	}

	param_buf = (WMI_PDEV_SET_HW_MODE_RESP_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		wma_err("Invalid WMI_PDEV_SET_HW_MODE_RESP_EVENTID event");
		/* Need to send response back to upper layer to free
		 * active command list
		 */
		goto fail;
	}
	if (param_buf->fixed_param->num_vdev_mac_entries >=
						MAX_VDEV_SUPPORTED) {
		wma_err("num_vdev_mac_entries crossed max value");
		goto fail;
	}

	wmi_event = param_buf->fixed_param;
	if (wmi_event->num_vdev_mac_entries >
	    param_buf->num_wmi_pdev_set_hw_mode_response_vdev_mac_mapping) {
		wma_err("Invalid num_vdev_mac_entries: %d",
			wmi_event->num_vdev_mac_entries);
		goto fail;
	}
	hw_mode_resp->status = wmi_event->status;
	hw_mode_resp->cfgd_hw_mode_index = wmi_event->cfgd_hw_mode_index;
	hw_mode_resp->num_vdev_mac_entries = wmi_event->num_vdev_mac_entries;

	wma_debug("status:%d cfgd_hw_mode_index:%d num_vdev_mac_entries:%d",
			wmi_event->status,
			wmi_event->cfgd_hw_mode_index,
			wmi_event->num_vdev_mac_entries);
	vdev_mac_entry =
		param_buf->wmi_pdev_set_hw_mode_response_vdev_mac_mapping;

	/* Store the vdev-mac map in WMA and prepare to send to PE  */
	for (i = 0; i < wmi_event->num_vdev_mac_entries; i++) {
		uint32_t vdev_id, mac_id, pdev_id;

		vdev_id = vdev_mac_entry[i].vdev_id;
		pdev_id = vdev_mac_entry[i].pdev_id;
		if (pdev_id == OL_TXRX_PDEV_ID) {
			wma_err("soc level id received for mac id");
			goto fail;
		}
		if (vdev_id >= wma->max_bssid) {
			wma_err("vdev_id: %d is invalid, max_bssid: %d",
				vdev_id, wma->max_bssid);
			goto fail;
		}

		mac_id = WMA_PDEV_TO_MAC_MAP(vdev_mac_entry[i].pdev_id);

		wma_debug("vdev_id:%d mac_id:%d", vdev_id, mac_id);

		hw_mode_resp->vdev_mac_map[i].vdev_id = vdev_id;
		hw_mode_resp->vdev_mac_map[i].mac_id = mac_id;
		wma_update_intf_hw_mode_params(vdev_id, mac_id,
				wmi_event->cfgd_hw_mode_index);
	}

	if (hw_mode_resp->status == SET_HW_MODE_STATUS_OK) {
		if (WMA_DEFAULT_HW_MODE_INDEX == wma->new_hw_mode_index) {
			wma->new_hw_mode_index = wmi_event->cfgd_hw_mode_index;
		} else {
			wma->old_hw_mode_index = wma->new_hw_mode_index;
			wma->new_hw_mode_index = wmi_event->cfgd_hw_mode_index;
		}
		policy_mgr_update_hw_mode_index(wma->psoc,
		wmi_event->cfgd_hw_mode_index);
	}

	wma_debug("Updated: old_hw_mode_index:%d new_hw_mode_index:%d",
		 wma->old_hw_mode_index, wma->new_hw_mode_index);

	wma_send_msg(wma, SIR_HAL_PDEV_SET_HW_MODE_RESP,
		     (void *) hw_mode_resp, 0);

	return QDF_STATUS_SUCCESS;

fail:
	wma_err("Sending fail response to LIM");
	hw_mode_resp->status = SET_HW_MODE_STATUS_ECANCELED;
	hw_mode_resp->cfgd_hw_mode_index = 0;
	hw_mode_resp->num_vdev_mac_entries = 0;
	wma_send_msg(wma, SIR_HAL_PDEV_SET_HW_MODE_RESP,
			(void *) hw_mode_resp, 0);

	return QDF_STATUS_E_FAILURE;
}

/**
 * wma_process_pdev_hw_mode_trans_ind() - Process HW mode transition info
 *
 * @handle: WMA handle
 * @fixed_param: Event fixed parameters
 * @vdev_mac_entry - vdev mac entry
 * @hw_mode_trans_ind - Buffer to store parsed information
 *
 * Parses fixed_param, vdev_mac_entry and fills in the information into
 * hw_mode_trans_ind and wma
 *
 * Return: None
 */
void wma_process_pdev_hw_mode_trans_ind(void *handle,
	wmi_pdev_hw_mode_transition_event_fixed_param *fixed_param,
	wmi_pdev_set_hw_mode_response_vdev_mac_entry *vdev_mac_entry,
	struct sir_hw_mode_trans_ind *hw_mode_trans_ind)
{
	uint32_t i;
	tp_wma_handle wma = (tp_wma_handle) handle;

	if (fixed_param->num_vdev_mac_entries > MAX_VDEV_SUPPORTED) {
		wma_err("Number of Vdev mac entries %d exceeded max vdev supported %d",
			fixed_param->num_vdev_mac_entries,
			MAX_VDEV_SUPPORTED);
		return;
	}
	hw_mode_trans_ind->old_hw_mode_index = fixed_param->old_hw_mode_index;
	hw_mode_trans_ind->new_hw_mode_index = fixed_param->new_hw_mode_index;
	hw_mode_trans_ind->num_vdev_mac_entries =
					fixed_param->num_vdev_mac_entries;
	wma_debug("old_hw_mode_index:%d new_hw_mode_index:%d entries=%d",
		fixed_param->old_hw_mode_index,
		fixed_param->new_hw_mode_index,
		fixed_param->num_vdev_mac_entries);

	if (!vdev_mac_entry) {
		wma_err("Invalid vdev_mac_entry");
		return;
	}

	/* Store the vdev-mac map in WMA and send to policy manager */
	for (i = 0; i < fixed_param->num_vdev_mac_entries; i++) {
		uint32_t vdev_id, mac_id, pdev_id;

		vdev_id = vdev_mac_entry[i].vdev_id;
		pdev_id = vdev_mac_entry[i].pdev_id;

		if (pdev_id == OL_TXRX_PDEV_ID) {
			wma_err("soc level id received for mac id");
			return;
		}
		if (vdev_id >= wma->max_bssid) {
			wma_err("vdev_id: %d is invalid, max_bssid: %d",
			        vdev_id, wma->max_bssid);
			return;
		}

		mac_id = WMA_PDEV_TO_MAC_MAP(vdev_mac_entry[i].pdev_id);

		wma_debug("vdev_id:%d mac_id:%d", vdev_id, mac_id);

		hw_mode_trans_ind->vdev_mac_map[i].vdev_id = vdev_id;
		hw_mode_trans_ind->vdev_mac_map[i].mac_id = mac_id;
		wma_update_intf_hw_mode_params(vdev_id, mac_id,
				fixed_param->new_hw_mode_index);
	}
	wma->old_hw_mode_index = fixed_param->old_hw_mode_index;
	wma->new_hw_mode_index = fixed_param->new_hw_mode_index;
	policy_mgr_update_new_hw_mode_index(wma->psoc,
		fixed_param->new_hw_mode_index);
	policy_mgr_update_old_hw_mode_index(wma->psoc,
		fixed_param->old_hw_mode_index);

	wma_debug("Updated: old_hw_mode_index:%d new_hw_mode_index:%d",
		 wma->old_hw_mode_index, wma->new_hw_mode_index);
}

/**
 * wma_pdev_hw_mode_transition_evt_handler() - HW mode transition evt handler
 * @handle: WMI handle
 * @event:  Event recevied from FW
 * @len:    Length of the event
 *
 * Event handler for WMI_PDEV_HW_MODE_TRANSITION_EVENTID that indicates an
 * asynchronous hardware mode transition. This event notifies the host driver
 * that firmware independently changed the hardware mode for some reason, such
 * as Coex, LFR 3.0, etc
 *
 * Return: Success on receiving valid params from FW
 */
static int wma_pdev_hw_mode_transition_evt_handler(void *handle,
		uint8_t *event,
		uint32_t len)
{
	WMI_PDEV_HW_MODE_TRANSITION_EVENTID_param_tlvs *param_buf;
	wmi_pdev_hw_mode_transition_event_fixed_param *wmi_event;
	wmi_pdev_set_hw_mode_response_vdev_mac_entry *vdev_mac_entry;
	struct sir_hw_mode_trans_ind *hw_mode_trans_ind;
	tp_wma_handle wma = (tp_wma_handle) handle;

	if (!wma) {
		/* This is an async event. So, not sending any event to LIM */
		wma_err("Invalid WMA handle");
		return QDF_STATUS_E_NULL_VALUE;
	}

	param_buf = (WMI_PDEV_HW_MODE_TRANSITION_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		/* This is an async event. So, not sending any event to LIM */
		wma_err("Invalid WMI_PDEV_HW_MODE_TRANSITION_EVENTID event");
		return QDF_STATUS_E_FAILURE;
	}

	if (param_buf->fixed_param->num_vdev_mac_entries > MAX_VDEV_SUPPORTED) {
		wma_err("num_vdev_mac_entries: %d crossed max value: %d",
			param_buf->fixed_param->num_vdev_mac_entries,
			MAX_VDEV_SUPPORTED);
		return QDF_STATUS_E_FAILURE;
	}

	hw_mode_trans_ind = qdf_mem_malloc(sizeof(*hw_mode_trans_ind));
	if (!hw_mode_trans_ind)
		return QDF_STATUS_E_NOMEM;

	wmi_event = param_buf->fixed_param;
	vdev_mac_entry =
		param_buf->wmi_pdev_set_hw_mode_response_vdev_mac_mapping;
	if (wmi_event->num_vdev_mac_entries >
	    param_buf->num_wmi_pdev_set_hw_mode_response_vdev_mac_mapping) {
		wma_err("Invalid num_vdev_mac_entries: %d",
			wmi_event->num_vdev_mac_entries);
		qdf_mem_free(hw_mode_trans_ind);
		return -EINVAL;
	}
	wma_process_pdev_hw_mode_trans_ind(wma, wmi_event, vdev_mac_entry,
		hw_mode_trans_ind);
	/* Pass the message to PE */
	wma_send_msg(wma, SIR_HAL_PDEV_HW_MODE_TRANS_IND,
		     (void *) hw_mode_trans_ind, 0);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_pdev_set_dual_mode_config_resp_evt_handler() - Dual mode evt handler
 * @handle: WMI handle
 * @event:  Event received from FW
 * @len:    Length of the event
 *
 * Notifies the host driver of the completion or failure of a
 * WMI_PDEV_SET_MAC_CONFIG_CMDID command. This event would be returned to
 * the host driver once the firmware has completed a reconfiguration of the Scan
 * and FW mode configuration. This changes could include entering or leaving a
 * dual mac configuration for either scan and/or more permanent firmware mode.
 *
 * Return: Success on receiving valid params from FW
 */
static int wma_pdev_set_dual_mode_config_resp_evt_handler(void *handle,
		uint8_t *event,
		uint32_t len)
{
	WMI_PDEV_SET_MAC_CONFIG_RESP_EVENTID_param_tlvs *param_buf;
	wmi_pdev_set_mac_config_response_event_fixed_param *wmi_event;
	tp_wma_handle wma = (tp_wma_handle) handle;
	struct sir_dual_mac_config_resp *dual_mac_cfg_resp;

	if (!wma) {
		wma_err("Invalid WMA handle");
		/* Since the WMA handle is NULL, we cannot send resp to LIM.
		 * So, returning from here.
		 */
		return QDF_STATUS_E_NULL_VALUE;
	}
	wma_release_wakelock(&wma->wmi_cmd_rsp_wake_lock);
	wma_remove_req(wma, 0, WMA_PDEV_MAC_CFG_RESP);

	dual_mac_cfg_resp = qdf_mem_malloc(sizeof(*dual_mac_cfg_resp));
	if (!dual_mac_cfg_resp)
		/* Since the mem alloc failed, we cannot send resp to LIM.
		 * So, returning from here.
		 */
		return QDF_STATUS_E_NULL_VALUE;

	param_buf = (WMI_PDEV_SET_MAC_CONFIG_RESP_EVENTID_param_tlvs *)
		event;
	if (!param_buf) {
		wma_err("Invalid event");
		goto fail;
	}

	wmi_event = param_buf->fixed_param;
	wma_debug("status: %d", wmi_event->status);
	dual_mac_cfg_resp->status = wmi_event->status;

	if (SET_HW_MODE_STATUS_OK == dual_mac_cfg_resp->status) {
		policy_mgr_update_dbs_scan_config(wma->psoc);
		policy_mgr_update_dbs_fw_config(wma->psoc);
	}

	/* Pass the message to PE */
	wma_send_msg(wma, SIR_HAL_PDEV_MAC_CFG_RESP,
			(void *) dual_mac_cfg_resp, 0);

	return QDF_STATUS_SUCCESS;

fail:
	wma_err("Sending fail response to LIM");
	dual_mac_cfg_resp->status = SET_HW_MODE_STATUS_ECANCELED;
	wma_send_msg(wma, SIR_HAL_PDEV_MAC_CFG_RESP,
			(void *) dual_mac_cfg_resp, 0);

	return QDF_STATUS_E_FAILURE;

}

/**
 * wma_send_time_stamp_sync_cmd() - timer callback send timestamp to
 * firmware to sync with host.
 * @wma_handle: wma handle
 *
 * Return: void
 */
static void wma_send_time_stamp_sync_cmd(void *data)
{
	tp_wma_handle wma_handle;
	QDF_STATUS qdf_status;

	wma_handle = (tp_wma_handle) data;

	wmi_send_time_stamp_sync_cmd_tlv(wma_handle->wmi_handle);

	/* Start/Restart the timer */
	qdf_status = qdf_mc_timer_start(&wma_handle->wma_fw_time_sync_timer,
				       WMA_FW_TIME_SYNC_TIMER);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		wma_err("Failed to start the firmware time sync timer");
}

#ifdef WLAN_CONV_SPECTRAL_ENABLE
static void wma_register_spectral_cmds(tp_wma_handle wma_handle)
{
	struct spectral_wmi_ops cmd_ops;

	cmd_ops.wmi_spectral_configure_cmd_send =
			wmi_unified_vdev_spectral_configure_cmd_send;
	cmd_ops.wmi_spectral_enable_cmd_send =
			wmi_unified_vdev_spectral_enable_cmd_send;
	wlan_register_spectral_wmi_ops(wma_handle->psoc, &cmd_ops);
}
#else
static void wma_register_spectral_cmds(tp_wma_handle wma_handle)
{
}
#endif
/**
 * wma_start() - wma start function.
 *               Initialize event handlers and timers.
 *
 * Return: 0 on success, QDF Error on failure
 */
QDF_STATUS wma_start(void)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma_handle;
	int status;
	struct wmi_unified *wmi_handle;
	struct mac_context *mac = NULL;

	wma_debug("Enter");

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	/* validate the wma_handle */
	if (!wma_handle) {
		wma_err("Invalid wma handle");
		qdf_status = QDF_STATUS_E_INVAL;
		goto end;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(wma_handle->psoc);
	if (!wmi_handle) {
		wma_err("Invalid wmi handle");
		qdf_status = QDF_STATUS_E_INVAL;
		goto end;
	}

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		wma_err("Invalid mac context");
		goto end;
	}

	status = wmi_unified_register_event_handler(wmi_handle,
						    wmi_roam_event_id,
						    wma_roam_event_callback,
						    WMA_RX_SERIALIZER_CTX);
	if (0 != status) {
		wma_err("Failed to register Roam callback");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	status = wmi_unified_register_event_handler(wmi_handle,
						    wmi_wow_wakeup_host_event_id,
						    wma_wow_wakeup_host_event,
						    WMA_RX_TASKLET_CTX);
	if (status) {
		wma_err("Failed to register wow wakeup host event handler");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	if (wma_d0_wow_is_supported()) {
		status = wmi_unified_register_event_handler(
				wmi_handle,
				wmi_d0_wow_disable_ack_event_id,
				wma_d0_wow_disable_ack_event,
				WMA_RX_TASKLET_CTX);
		if (status) {
			wma_err("Failed to register d0wow disable ack event handler");
			qdf_status = QDF_STATUS_E_FAILURE;
			goto end;
		}
	}

	status = wmi_unified_register_event_handler(wmi_handle,
				wmi_pdev_resume_event_id,
				wma_pdev_resume_event_handler,
				WMA_RX_TASKLET_CTX);
	if (status) {
		wma_err("Failed to register PDEV resume event handler");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}
#if defined(QCA_LL_LEGACY_TX_FLOW_CONTROL) || \
	defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(CONFIG_HL_SUPPORT)
	wma_debug("MCC TX Pause Event Handler register");
	status = wmi_unified_register_event_handler(wmi_handle,
					wmi_tx_pause_event_id,
					wma_mcc_vdev_tx_pause_evt_handler,
					WMA_RX_TASKLET_CTX);
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

	wma_debug("Registering SAR2 response handler");
	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
						wmi_wlan_sar2_result_event_id,
						wma_sar_rsp_evt_handler,
						WMA_RX_SERIALIZER_CTX);
	if (status) {
		wma_err("Failed to register sar response event cb");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	wma_debug("Registering auto shutdown handler");
	status = wmi_unified_register_event_handler(wmi_handle,
						wmi_host_auto_shutdown_event_id,
						wma_auto_shutdown_event_handler,
						WMA_RX_SERIALIZER_CTX);
	if (status) {
		wma_err("Failed to register WMI Auto shutdown event handler");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}
#endif /* FEATURE_WLAN_AUTO_SHUTDOWN */
	status = wmi_unified_register_event_handler(wmi_handle,
						wmi_thermal_mgmt_event_id,
						wma_thermal_mgmt_evt_handler,
						WMA_RX_SERIALIZER_CTX);
	if (status) {
		wma_err("Failed to register thermal mitigation event cb");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	status = wma_ocb_register_callbacks(wma_handle);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		wma_err("Failed to register OCB callbacks");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	qdf_status = QDF_STATUS_SUCCESS;

#ifdef QCA_WIFI_FTM
	/*
	 * Tx mgmt attach requires TXRX context which is not created
	 * in FTM mode. So skip the TX mgmt attach.
	 */
	if (cds_get_conparam() == QDF_GLOBAL_FTM_MODE)
		goto end;
#endif /* QCA_WIFI_FTM */

	qdf_status = wma_tx_attach(wma_handle);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to register tx management");
		goto end;
	}

	if (!mac->mlme_cfg->gen.enable_remove_time_stamp_sync_cmd &&
	    cds_get_conparam() != QDF_GLOBAL_FTM_MODE) {
		/* Initialize firmware time stamp sync timer */
		qdf_status = qdf_mc_timer_init(
					&wma_handle->wma_fw_time_sync_timer,
					QDF_TIMER_TYPE_SW,
					wma_send_time_stamp_sync_cmd,
					wma_handle);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			wma_err("Failed to init fw time sync timer");
			goto end;
		}

		/* Start firmware time stamp sync timer */
		wma_send_time_stamp_sync_cmd(wma_handle);
	}
	/* Initialize log completion timeout */
	qdf_status = qdf_mc_timer_init(&wma_handle->log_completion_timer,
			QDF_TIMER_TYPE_SW,
			wma_log_completion_timeout,
			wma_handle);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to initialize log completion timeout");
		goto end;
	}

	status = wma_fips_register_event_handlers(wma_handle);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		wma_err("Failed to register FIPS event handler");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	status = wma_sar_register_event_handlers(wma_handle);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		wma_err("Failed to register SAR event handlers");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	/* Initialize the get temperature event handler */
	status = wmi_unified_register_event_handler(wmi_handle,
					wmi_pdev_temperature_event_id,
					wma_pdev_temperature_evt_handler,
					WMA_RX_SERIALIZER_CTX);
	if (status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to register get_temperature event cb");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	status = wmi_unified_register_event_handler(wmi_handle,
						wmi_vdev_tsf_report_event_id,
						wma_vdev_tsf_handler,
						WMA_RX_SERIALIZER_CTX);
	if (0 != status) {
		wma_err("Failed to register tsf callback");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	/* Initialize the wma_pdev_set_hw_mode_resp_evt_handler event handler */
	status = wmi_unified_register_event_handler(wmi_handle,
			wmi_pdev_set_hw_mode_rsp_event_id,
			wma_pdev_set_hw_mode_resp_evt_handler,
			WMA_RX_SERIALIZER_CTX);
	if (status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to register set hw mode resp event cb");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	/* Initialize the WMI_SOC_HW_MODE_TRANSITION_EVENTID event handler */
	status = wmi_unified_register_event_handler(wmi_handle,
			wmi_pdev_hw_mode_transition_event_id,
			wma_pdev_hw_mode_transition_evt_handler,
			WMA_RX_SERIALIZER_CTX);
	if (status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to register hw mode transition event cb");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	/* Initialize the set dual mac configuration event handler */
	status = wmi_unified_register_event_handler(wmi_handle,
			wmi_pdev_set_mac_config_resp_event_id,
			wma_pdev_set_dual_mode_config_resp_evt_handler,
			WMA_RX_SERIALIZER_CTX);
	if (status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to register hw mode transition event cb");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	status = wmi_unified_register_event_handler(wmi_handle,
			wmi_coex_bt_activity_event_id,
			wma_wlan_bt_activity_evt_handler,
			WMA_RX_SERIALIZER_CTX);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		wma_err("Failed to register coex bt activity event handler");
		qdf_status = QDF_STATUS_E_FAILURE;
		goto end;
	}
	wma_register_spectral_cmds(wma_handle);

end:
	wma_debug("Exit");
	return qdf_status;
}

QDF_STATUS wma_stop(void)
{
	tp_wma_handle wma_handle;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	int i;
	struct mac_context *mac = NULL;
	struct wlan_objmgr_vdev *vdev;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	wma_debug("Enter");
	/* validate the wma_handle */
	if (!wma_handle) {
		wma_err("Invalid handle");
		qdf_status = QDF_STATUS_E_INVAL;
		goto end;
	}
	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		wma_err("Invalid mac context");
		goto end;
	}
#ifdef QCA_WIFI_FTM
	/*
	 * Tx mgmt detach requires TXRX context which is not created
	 * in FTM mode. So skip the TX mgmt detach.
	 */
	if (cds_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		qdf_status = QDF_STATUS_SUCCESS;
		goto end;
	}
#endif /* QCA_WIFI_FTM */

	if (wma_handle->ack_work_ctx) {
		cds_flush_work(&wma_handle->ack_work_ctx->ack_cmp_work);
		qdf_mem_free(wma_handle->ack_work_ctx);
		wma_handle->ack_work_ctx = NULL;
	}

	/* Destroy the timer for log completion */
	qdf_status = qdf_mc_timer_destroy(&wma_handle->log_completion_timer);
	if (qdf_status != QDF_STATUS_SUCCESS)
		wma_err("Failed to destroy the log completion timer");
	/* clean up ll-queue for all vdev */
	for (i = 0; i < wma_handle->max_bssid; i++) {
		vdev = wma_handle->interfaces[i].vdev;
		if (!vdev)
			continue;

		if (wma_is_vdev_up(i))
			cdp_fc_vdev_flush(cds_get_context(QDF_MODULE_ID_SOC),
					  i);
	}

	if (!mac->mlme_cfg->gen.enable_remove_time_stamp_sync_cmd &&
	    cds_get_conparam() != QDF_GLOBAL_FTM_MODE) {
		/* Destroy firmware time stamp sync timer */
		qdf_status = qdf_mc_timer_destroy(
				&wma_handle->wma_fw_time_sync_timer);
		if (QDF_IS_STATUS_ERROR(qdf_status))
			wma_err("Failed to destroy fw sync timer");
	}

	qdf_status = wma_tx_detach(wma_handle);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to deregister tx management");
		goto end;
	}

end:
	wma_debug("Exit");
	return qdf_status;
}

/**
 * wma_wmi_service_close() - close wma wmi service interface.
 *
 * Return: 0 on success, QDF Error on failure
 */
QDF_STATUS wma_wmi_service_close(void)
{
	void *cds_ctx;
	tp_wma_handle wma_handle;
	uint8_t i;

	wma_debug("Enter");

	cds_ctx = cds_get_global_context();
	if (!cds_ctx) {
		wma_err("Invalid CDS context");
		return QDF_STATUS_E_INVAL;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	/* validate the wma_handle */
	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return QDF_STATUS_E_INVAL;
	}

	/* validate the wmi handle */
	if (!wma_handle->wmi_handle) {
		wma_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	/* dettach the wmi serice */
	wma_debug("calling wmi_unified_detach");
	wmi_unified_detach(wma_handle->wmi_handle);
	wma_handle->wmi_handle = NULL;

	for (i = 0; i < wma_handle->max_bssid; i++)
		wma_vdev_deinit(&wma_handle->interfaces[i]);

	qdf_mem_free(wma_handle->interfaces);

	/* free the wma_handle */
	cds_free_context(QDF_MODULE_ID_WMA, wma_handle);

	if (((struct cds_context *)cds_ctx)->cfg_ctx)
		qdf_mem_free(((struct cds_context *)cds_ctx)->cfg_ctx);
	((struct cds_context *)cds_ctx)->cfg_ctx = NULL;
	wma_debug("Exit");
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_wmi_work_close() - close the work queue items associated with WMI
 *
 * This function closes work queue items associated with WMI, but not fully
 * closes WMI service.
 *
 * Return: QDF_STATUS_SUCCESS if work close is successful. Otherwise
 *	proper error codes.
 */
QDF_STATUS wma_wmi_work_close(void)
{
	tp_wma_handle wma_handle;

	wma_debug("Enter");

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	/* validate the wma_handle */
	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return QDF_STATUS_E_INVAL;
	}

	/* validate the wmi handle */
	if (!wma_handle->wmi_handle) {
		wma_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	/* remove the wmi work */
	wma_debug("calling wmi_unified_remove_work");
	wmi_unified_remove_work(wma_handle->wmi_handle);

	wma_debug("Exit");
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_close() - wma close function.
 *               cleanup resources attached with wma.
 *
 * Return: 0 on success, QDF Error on failure
 */
QDF_STATUS wma_close(void)
{
	tp_wma_handle wma_handle;
	struct target_psoc_info *tgt_psoc_info;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	wma_debug("Enter");

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	/* validate the wma_handle */
	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return QDF_STATUS_E_INVAL;
	}

	/* validate the wmi handle */
	if (!wma_handle->wmi_handle) {
		wma_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	/* Free DBS list */
	if (wma_handle->hw_mode.hw_mode_list) {
		qdf_mem_free(wma_handle->hw_mode.hw_mode_list);
		wma_handle->hw_mode.hw_mode_list = NULL;
		wma_debug("DBS list is freed");
	}

	if (cds_get_conparam() != QDF_GLOBAL_FTM_MODE) {
#ifdef FEATURE_WLAN_EXTSCAN
		qdf_wake_lock_destroy(&wma_handle->extscan_wake_lock);
#endif /* FEATURE_WLAN_EXTSCAN */
		qdf_wake_lock_destroy(&wma_handle->wow_wake_lock);
		qdf_wake_lock_destroy(&wma_handle->wow_auth_req_wl);
		qdf_wake_lock_destroy(&wma_handle->wow_assoc_req_wl);
		qdf_wake_lock_destroy(&wma_handle->wow_deauth_rec_wl);
		qdf_wake_lock_destroy(&wma_handle->wow_disassoc_rec_wl);
		qdf_wake_lock_destroy(&wma_handle->wow_ap_assoc_lost_wl);
		qdf_wake_lock_destroy(&wma_handle->wow_auto_shutdown_wl);
		qdf_wake_lock_destroy(&wma_handle->roam_ho_wl);
		qdf_wake_lock_destroy(&wma_handle->roam_preauth_wl);
		qdf_wake_lock_destroy(&wma_handle->probe_req_wps_wl);
	}

	/* unregister Firmware debug log */
	qdf_status = dbglog_deinit(wma_handle->wmi_handle);
	if (qdf_status != QDF_STATUS_SUCCESS)
		wma_err("dbglog_deinit failed");

	qdf_status = qdf_mc_timer_destroy(&wma_handle->service_ready_ext_timer);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		wma_err("Failed to destroy service ready ext event timer");

	qdf_event_destroy(&wma_handle->target_suspend);
	qdf_event_destroy(&wma_handle->runtime_suspend);
	qdf_event_destroy(&wma_handle->recovery_event);
	qdf_event_destroy(&wma_handle->tx_frm_download_comp_event);
	qdf_event_destroy(&wma_handle->tx_queue_empty_event);
	wma_cleanup_hold_req(wma_handle);
	qdf_wake_lock_destroy(&wma_handle->wmi_cmd_rsp_wake_lock);
	qdf_runtime_lock_deinit(&wma_handle->sap_prevent_runtime_pm_lock);
	qdf_runtime_lock_deinit(&wma_handle->wmi_cmd_rsp_runtime_lock);
	qdf_spinlock_destroy(&wma_handle->wma_hold_req_q_lock);

	if (wma_handle->pGetRssiReq) {
		qdf_mem_free(wma_handle->pGetRssiReq);
		wma_handle->pGetRssiReq = NULL;
	}

	wma_unified_radio_tx_mem_free(wma_handle);

	if (wma_handle->pdev) {
		wlan_objmgr_pdev_release_ref(wma_handle->pdev,
				WLAN_LEGACY_WMA_ID);
		wma_handle->pdev = NULL;
	}

	pmo_unregister_get_beacon_interval_callback(wma_handle->psoc);
	pmo_unregister_get_dtim_period_callback(wma_handle->psoc);
	pmo_unregister_is_device_in_low_pwr_mode(wma_handle->psoc);
	pmo_unregister_get_pause_bitmap(wma_handle->psoc);
	pmo_unregister_pause_bitmap_notifier(wma_handle->psoc);

	tgt_psoc_info = wlan_psoc_get_tgt_if_handle(wma_handle->psoc);
	init_deinit_free_num_units(wma_handle->psoc, tgt_psoc_info);
	target_if_free_psoc_tgt_info(wma_handle->psoc);

	wlan_objmgr_psoc_release_ref(wma_handle->psoc, WLAN_LEGACY_WMA_ID);
	wma_handle->psoc = NULL;

	wma_debug("Exit");
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_update_fw_config() - update fw configuration
 * @psoc: psoc to query configuration from
 * @tgt_hdl: target capability info
 *
 * Return: none
 */
static void wma_update_fw_config(struct wlan_objmgr_psoc *psoc,
				 struct target_psoc_info *tgt_hdl)
{
	target_resource_config *cfg = &tgt_hdl->info.wlan_res_cfg;

	/* Override the no. of max fragments as per platform configuration */
	cfg->max_frag_entries =	QDF_MIN(QCA_OL_11AC_TX_MAX_FRAGS,
					target_if_get_max_frag_entry(tgt_hdl));
	target_if_set_max_frag_entry(tgt_hdl, cfg->max_frag_entries);

	cfg->num_wow_filters = ucfg_pmo_get_num_wow_filters(psoc);
	cfg->apf_instruction_size = ucfg_pmo_get_apf_instruction_size(psoc);
	cfg->num_packet_filters = ucfg_pmo_get_num_packet_filters(psoc);
}

/**
 * wma_set_tx_partition_base() - set TX MSDU ID partition base for IPA
 * @value:  TX MSDU ID partition base
 *
 * Return: none
 */
#ifdef IPA_OFFLOAD
static void wma_set_tx_partition_base(uint32_t value)
{
	cdp_ipa_set_uc_tx_partition_base(
			cds_get_context(QDF_MODULE_ID_SOC),
			(struct cdp_cfg *)cds_get_context(QDF_MODULE_ID_CFG),
			value);
	wma_debug("TX_MSDU_ID_PARTITION=%d", value);
}
#else
static void wma_set_tx_partition_base(uint32_t value)
{
}
#endif

#ifdef WLAN_FEATURE_IGMP_OFFLOAD
/**
 * wma_get_igmp_offload_enable() - update tgt service with igmp offload support
 * @wmi_handle: Unified wmi handle
 * @cfg: target services
 *
 * Return: none
 */
static inline void
wma_get_igmp_offload_enable(struct wmi_unified *wmi_handle,
			    struct wma_tgt_services *cfg)
{
	cfg->igmp_offload_enable = wmi_service_enabled(
					wmi_handle,
					wmi_service_igmp_offload_support);
}
#else
static inline void
wma_get_igmp_offload_enable(struct wmi_unified *wmi_handle,
			    struct wma_tgt_services *cfg)
{}
#endif

#ifdef WLAN_FEATURE_11AX
#ifdef FEATURE_WLAN_TDLS
/**
 * wma_get_tdls_ax_support() - update tgt service with service tdls ax support
 * @wmi_handle: Unified wmi handle
 * @cfg: target services
 *
 * Return: none
 */
static inline void
wma_get_tdls_ax_support(struct wmi_unified *wmi_handle,
			struct wma_tgt_services *cfg)
{
	cfg->en_tdls_11ax_support = wmi_service_enabled(
						wmi_handle,
						wmi_service_tdls_ax_support);
}
#else
static inline void
wma_get_tdls_ax_support(struct wmi_unified *wmi_handle,
			struct wma_tgt_services *cfg)
{}
#endif
#else
static inline void
wma_get_tdls_ax_support(struct wmi_unified *wmi_handle,
			struct wma_tgt_services *cfg)
{}
#endif

/**
 * wma_update_target_services() - update target services from wma handle
 * @wmi_handle: Unified wmi handle
 * @cfg: target services
 *
 * Return: none
 */
static inline void wma_update_target_services(struct wmi_unified *wmi_handle,
					      struct wma_tgt_services *cfg)
{
	/* STA power save */
	cfg->sta_power_save = wmi_service_enabled(wmi_handle,
						     wmi_service_sta_pwrsave);

	/* Enable UAPSD */
	cfg->uapsd = wmi_service_enabled(wmi_handle,
					    wmi_service_ap_uapsd);

	/* Update AP DFS service */
	cfg->ap_dfs = wmi_service_enabled(wmi_handle,
					     wmi_service_ap_dfs);

	/* Enable 11AC */
	cfg->en_11ac = wmi_service_enabled(wmi_handle,
					      wmi_service_11ac);
	if (cfg->en_11ac)
		g_fw_wlan_feat_caps |= (1 << DOT11AC);

	/* Proactive ARP response */
	g_fw_wlan_feat_caps |= (1 << WLAN_PERIODIC_TX_PTRN);

	/* Enable WOW */
	g_fw_wlan_feat_caps |= (1 << WOW);

	/* ARP offload */
	cfg->arp_offload = wmi_service_enabled(wmi_handle,
						  wmi_service_arpns_offload);

	/* Adaptive early-rx */
	cfg->early_rx = wmi_service_enabled(wmi_handle,
					       wmi_service_early_rx);
#ifdef FEATURE_WLAN_SCAN_PNO
	/* PNO offload */
	if (wmi_service_enabled(wmi_handle, wmi_service_nlo)) {
		cfg->pno_offload = true;
		g_fw_wlan_feat_caps |= (1 << PNO);
	}
#endif /* FEATURE_WLAN_SCAN_PNO */

#ifdef FEATURE_WLAN_EXTSCAN
	if (wmi_service_enabled(wmi_handle, wmi_service_extscan))
		g_fw_wlan_feat_caps |= (1 << EXTENDED_SCAN);
#endif /* FEATURE_WLAN_EXTSCAN */
	cfg->lte_coex_ant_share = wmi_service_enabled(wmi_handle,
					wmi_service_lte_ant_share_support);
#ifdef FEATURE_WLAN_TDLS
	/* Enable TDLS */
	if (wmi_service_enabled(wmi_handle, wmi_service_tdls)) {
		cfg->en_tdls = 1;
		g_fw_wlan_feat_caps |= (1 << TDLS);
	}
	/* Enable advanced TDLS features */
	if (wmi_service_enabled(wmi_handle, wmi_service_tdls_offchan)) {
		cfg->en_tdls_offchan = 1;
		g_fw_wlan_feat_caps |= (1 << TDLS_OFF_CHANNEL);
	}

	cfg->en_tdls_uapsd_buf_sta =
		wmi_service_enabled(wmi_handle,
				       wmi_service_tdls_uapsd_buffer_sta);
	cfg->en_tdls_uapsd_sleep_sta =
		wmi_service_enabled(wmi_handle,
				       wmi_service_tdls_uapsd_sleep_sta);
#endif /* FEATURE_WLAN_TDLS */
	if (wmi_service_enabled
		    (wmi_handle, wmi_service_beacon_offload))
		cfg->beacon_offload = true;
	if (wmi_service_enabled
		    (wmi_handle, wmi_service_sta_pmf_offload))
		cfg->pmf_offload = true;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	/* Enable Roam Offload */
	cfg->en_roam_offload = wmi_service_enabled(wmi_handle,
					      wmi_service_roam_ho_offload);
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
#ifdef WLAN_FEATURE_NAN
	if (wmi_service_enabled(wmi_handle, wmi_service_nan))
		g_fw_wlan_feat_caps |= (1 << NAN);
#endif /* WLAN_FEATURE_NAN */

	if (wmi_service_enabled(wmi_handle, wmi_service_rtt))
		g_fw_wlan_feat_caps |= (1 << RTT);

	if (wmi_service_enabled(wmi_handle,
			wmi_service_tx_msdu_id_new_partition_support)) {
		wma_set_tx_partition_base(HTT_TX_IPA_NEW_MSDU_ID_SPACE_BEGIN);
	} else {
		wma_set_tx_partition_base(HTT_TX_IPA_MSDU_ID_SPACE_BEGIN);
	}

	wma_he_update_tgt_services(wmi_handle, cfg);

	cfg->get_peer_info_enabled =
		wmi_service_enabled(wmi_handle,
				       wmi_service_peer_stats_info);
	if (wmi_service_enabled(wmi_handle, wmi_service_fils_support))
		cfg->is_fils_roaming_supported = true;

	if (wmi_service_enabled(wmi_handle, wmi_service_mawc_support))
		cfg->is_fw_mawc_capable = true;

	if (wmi_service_enabled(wmi_handle,
				wmi_service_11k_neighbour_report_support))
		cfg->is_11k_offload_supported = true;

	if (wmi_service_enabled(wmi_handle, wmi_service_twt_requestor))
		cfg->twt_requestor = true;
	if (wmi_service_enabled(wmi_handle, wmi_service_twt_responder))
		cfg->twt_responder = true;
	if (wmi_service_enabled(wmi_handle, wmi_service_obss_scan))
		cfg->obss_scan_offload = true;
	if (wmi_service_enabled(wmi_handle, wmi_service_beacon_reception_stats))
		cfg->bcn_reception_stats = true;

	if (wmi_service_enabled(wmi_handle, wmi_service_vdev_latency_config))
		g_fw_wlan_feat_caps |= (1 << VDEV_LATENCY_CONFIG);
	if (wmi_service_enabled(wmi_handle,
				wmi_roam_scan_chan_list_to_host_support))
		cfg->is_roam_scan_ch_to_host = true;

	cfg->ll_stats_per_chan_rx_tx_time =
		wmi_service_enabled(wmi_handle,
				    wmi_service_ll_stats_per_chan_rx_tx_time);

	wma_get_service_cap_club_get_sta_in_ll_stats_req(wmi_handle, cfg);

	wma_get_igmp_offload_enable(wmi_handle, cfg);
	wma_get_tdls_ax_support(wmi_handle, cfg);
}

/**
 * wma_update_target_ht_cap() - update ht capabality from wma handle
 * @tgt_hdl: pointer to structure target_psoc_info
 * @cfg: ht capability
 *
 * Return: none
 */
static inline void
wma_update_target_ht_cap(struct target_psoc_info *tgt_hdl,
			 struct wma_tgt_ht_cap *cfg)
{
	int ht_cap_info;

	ht_cap_info = target_if_get_ht_cap_info(tgt_hdl);
	/* RX STBC */
	cfg->ht_rx_stbc = !!(ht_cap_info & WMI_HT_CAP_RX_STBC);

	/* TX STBC */
	cfg->ht_tx_stbc = !!(ht_cap_info & WMI_HT_CAP_TX_STBC);

	/* MPDU density */
	cfg->mpdu_density = ht_cap_info & WMI_HT_CAP_MPDU_DENSITY;

	/* HT RX LDPC */
	cfg->ht_rx_ldpc = !!(ht_cap_info & WMI_HT_CAP_LDPC);

	/* HT SGI */
	cfg->ht_sgi_20 = !!(ht_cap_info & WMI_HT_CAP_HT20_SGI);

	cfg->ht_sgi_40 = !!(ht_cap_info & WMI_HT_CAP_HT40_SGI);

	/* RF chains */
	cfg->num_rf_chains = target_if_get_num_rf_chains(tgt_hdl);

	wma_nofl_debug("ht_cap_info - %x ht_rx_stbc - %d, ht_tx_stbc - %d\n"
		 "mpdu_density - %d ht_rx_ldpc - %d ht_sgi_20 - %d\n"
		 "ht_sgi_40 - %d num_rf_chains - %d",
		 ht_cap_info,
		 cfg->ht_rx_stbc, cfg->ht_tx_stbc, cfg->mpdu_density,
		 cfg->ht_rx_ldpc, cfg->ht_sgi_20, cfg->ht_sgi_40,
		 cfg->num_rf_chains);

}

/**
 * wma_update_target_vht_cap() - update vht capabality from wma handle
 * @tgt_hdl: pointer to structure target_psoc_info
 * @cfg: vht capabality
 *
 * Return: none
 */
static inline void
wma_update_target_vht_cap(struct target_psoc_info *tgt_hdl,
			  struct wma_tgt_vht_cap *cfg)
{
	int vht_cap_info = target_if_get_vht_cap_info(tgt_hdl);

	if (vht_cap_info & WMI_VHT_CAP_MAX_MPDU_LEN_11454)
		cfg->vht_max_mpdu = WMI_VHT_CAP_MAX_MPDU_LEN_11454;
	else if (vht_cap_info & WMI_VHT_CAP_MAX_MPDU_LEN_7935)
		cfg->vht_max_mpdu = WMI_VHT_CAP_MAX_MPDU_LEN_7935;
	else
		cfg->vht_max_mpdu = 0;


	if (vht_cap_info & WMI_VHT_CAP_CH_WIDTH_80P80_160MHZ) {
		cfg->supp_chan_width = 1 << eHT_CHANNEL_WIDTH_80P80MHZ;
		cfg->supp_chan_width |= 1 << eHT_CHANNEL_WIDTH_160MHZ;
	} else if (vht_cap_info & WMI_VHT_CAP_CH_WIDTH_160MHZ) {
		cfg->supp_chan_width = 1 << eHT_CHANNEL_WIDTH_160MHZ;
	} else {
		cfg->supp_chan_width = 1 << eHT_CHANNEL_WIDTH_80MHZ;
	}

	cfg->vht_rx_ldpc = vht_cap_info & WMI_VHT_CAP_RX_LDPC;

	cfg->vht_short_gi_80 = vht_cap_info & WMI_VHT_CAP_SGI_80MHZ;
	cfg->vht_short_gi_160 = vht_cap_info & WMI_VHT_CAP_SGI_160MHZ;

	cfg->vht_tx_stbc = vht_cap_info & WMI_VHT_CAP_TX_STBC;

	cfg->vht_rx_stbc =
		(vht_cap_info & WMI_VHT_CAP_RX_STBC_1SS) |
		(vht_cap_info & WMI_VHT_CAP_RX_STBC_2SS) |
		(vht_cap_info & WMI_VHT_CAP_RX_STBC_3SS);

	cfg->vht_max_ampdu_len_exp = (vht_cap_info &
				      WMI_VHT_CAP_MAX_AMPDU_LEN_EXP)
				     >> WMI_VHT_CAP_MAX_AMPDU_LEN_EXP_SHIFT;

	cfg->vht_su_bformer = vht_cap_info & WMI_VHT_CAP_SU_BFORMER;

	cfg->vht_su_bformee = vht_cap_info & WMI_VHT_CAP_SU_BFORMEE;

	cfg->vht_mu_bformer = vht_cap_info & WMI_VHT_CAP_MU_BFORMER;

	cfg->vht_mu_bformee = vht_cap_info & WMI_VHT_CAP_MU_BFORMEE;

	cfg->vht_txop_ps = vht_cap_info & WMI_VHT_CAP_TXOP_PS;

	wma_nofl_debug("max_mpdu %d supp_chan_width %x rx_ldpc %x\n"
		 "short_gi_80 %x tx_stbc %x rx_stbc %x txop_ps %x\n"
		 "su_bformee %x mu_bformee %x max_ampdu_len_exp %d",
		 cfg->vht_max_mpdu, cfg->supp_chan_width, cfg->vht_rx_ldpc,
		 cfg->vht_short_gi_80, cfg->vht_tx_stbc, cfg->vht_rx_stbc,
		 cfg->vht_txop_ps, cfg->vht_su_bformee, cfg->vht_mu_bformee,
		 cfg->vht_max_ampdu_len_exp);
}

/**
 * wma_update_supported_bands() - update supported bands from service ready ext
 * @supported_bands: Supported band given by FW through service ready ext params
 * @new_supported_bands: New supported band which needs to be updated by
 *			 this API which WMA layer understands
 *
 * This API will convert FW given supported band to enum which WMA layer
 * understands
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wma_update_supported_bands(
			WLAN_BAND_CAPABILITY supported_bands,
			WMI_PHY_CAPABILITY *new_supported_bands)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!new_supported_bands) {
		wma_err("NULL new supported band variable");
		return QDF_STATUS_E_FAILURE;
	}
	switch (supported_bands) {
	case WLAN_2G_CAPABILITY:
		*new_supported_bands |= WMI_11G_CAPABILITY;
		break;
	case WLAN_5G_CAPABILITY:
		*new_supported_bands |= WMI_11A_CAPABILITY;
		break;
	default:
		wma_err("wrong supported band");
		status = QDF_STATUS_E_FAILURE;
		break;
	}
	return status;
}

/**
 * wma_derive_ext_ht_cap() - Derive HT caps based on given value
 * @ht_cap: given pointer to HT caps which needs to be updated
 * @tx_chain: given tx chainmask value
 * @rx_chain: given rx chainmask value
 * @value: new HT cap info provided in form of bitmask
 *
 * This function takes the value provided in form of bitmask and decodes
 * it. After decoding, what ever value it gets, it takes the union(max) or
 * intersection(min) with previously derived values.
 *
 * Return: none
 *
 */
static void wma_derive_ext_ht_cap(
			struct wma_tgt_ht_cap *ht_cap, uint32_t value,
			uint32_t tx_chain, uint32_t rx_chain)
{
	struct wma_tgt_ht_cap tmp = {0};

	if (!ht_cap)
		return;

	if (!qdf_mem_cmp(ht_cap, &tmp, sizeof(struct wma_tgt_ht_cap))) {
		ht_cap->ht_rx_stbc = (!!(value & WMI_HT_CAP_RX_STBC));
		ht_cap->ht_tx_stbc = (!!(value & WMI_HT_CAP_TX_STBC));
		ht_cap->mpdu_density = (!!(value & WMI_HT_CAP_MPDU_DENSITY));
		ht_cap->ht_rx_ldpc = (!!(value & WMI_HT_CAP_RX_LDPC));
		ht_cap->ht_sgi_20 = (!!(value & WMI_HT_CAP_HT20_SGI));
		ht_cap->ht_sgi_40 = (!!(value & WMI_HT_CAP_HT40_SGI));
		ht_cap->num_rf_chains =
			QDF_MAX(wma_get_num_of_setbits_from_bitmask(tx_chain),
				wma_get_num_of_setbits_from_bitmask(rx_chain));
	} else {
		ht_cap->ht_rx_stbc = QDF_MIN(ht_cap->ht_rx_stbc,
					(!!(value & WMI_HT_CAP_RX_STBC)));
		ht_cap->ht_tx_stbc = QDF_MAX(ht_cap->ht_tx_stbc,
					(!!(value & WMI_HT_CAP_TX_STBC)));
		ht_cap->mpdu_density = QDF_MIN(ht_cap->mpdu_density,
					(!!(value & WMI_HT_CAP_MPDU_DENSITY)));
		ht_cap->ht_rx_ldpc = QDF_MIN(ht_cap->ht_rx_ldpc,
					(!!(value & WMI_HT_CAP_RX_LDPC)));
		ht_cap->ht_sgi_20 = QDF_MIN(ht_cap->ht_sgi_20,
					(!!(value & WMI_HT_CAP_HT20_SGI)));
		ht_cap->ht_sgi_40 = QDF_MIN(ht_cap->ht_sgi_40,
					(!!(value & WMI_HT_CAP_HT40_SGI)));
		ht_cap->num_rf_chains =
			QDF_MAX(ht_cap->num_rf_chains,
				QDF_MAX(wma_get_num_of_setbits_from_bitmask(
								tx_chain),
					wma_get_num_of_setbits_from_bitmask(
								rx_chain)));
	}
}

/**
 * wma_update_target_ext_ht_cap() - Update HT caps with given extended cap
 * @tgt_hdl - target psoc information
 * @ht_cap: HT cap structure to be filled
 *
 * This function loop through each hardware mode and for each hardware mode
 * again it loop through each MAC/PHY and pull the caps 2G and 5G specific
 * HT caps and derives the final cap.
 *
 * Return: none
 *
 */
static void wma_update_target_ext_ht_cap(struct target_psoc_info *tgt_hdl,
					 struct wma_tgt_ht_cap *ht_cap)
{
	int i, total_mac_phy_cnt;
	uint32_t ht_2g, ht_5g;
	struct wma_tgt_ht_cap tmp_ht_cap = {0}, tmp_cap = {0};
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap;
	int num_hw_modes;

	total_mac_phy_cnt = target_psoc_get_total_mac_phy_cnt(tgt_hdl);
	num_hw_modes = target_psoc_get_num_hw_modes(tgt_hdl);
	mac_phy_cap = target_psoc_get_mac_phy_cap(tgt_hdl);

	if (!mac_phy_cap) {
		wma_err("Invalid MAC PHY capabilities handle");
		return;
	}

	/*
	 * for legacy device extended cap might not even come, so in that case
	 * don't overwrite legacy values
	 */
	if (!num_hw_modes) {
		wma_debug("No extended HT cap for current SOC");
		return;
	}

	for (i = 0; i < total_mac_phy_cnt; i++) {
		ht_2g = mac_phy_cap[i].ht_cap_info_2G;
		ht_5g = mac_phy_cap[i].ht_cap_info_5G;
		if (ht_2g)
			wma_derive_ext_ht_cap(&tmp_ht_cap,
					ht_2g,
					mac_phy_cap[i].tx_chain_mask_2G,
					mac_phy_cap[i].rx_chain_mask_2G);
		if (ht_5g)
			wma_derive_ext_ht_cap(&tmp_ht_cap,
					ht_5g,
					mac_phy_cap[i].tx_chain_mask_5G,
					mac_phy_cap[i].rx_chain_mask_5G);
	}

	if (qdf_mem_cmp(&tmp_cap, &tmp_ht_cap,
				sizeof(struct wma_tgt_ht_cap))) {
		qdf_mem_copy(ht_cap, &tmp_ht_cap,
				sizeof(struct wma_tgt_ht_cap));
	}

	wma_nofl_debug("[ext ht cap] ht_rx_stbc - %d, ht_tx_stbc - %d\n"
			"mpdu_density - %d ht_rx_ldpc - %d ht_sgi_20 - %d\n"
			"ht_sgi_40 - %d num_rf_chains - %d",
			ht_cap->ht_rx_stbc, ht_cap->ht_tx_stbc,
			ht_cap->mpdu_density, ht_cap->ht_rx_ldpc,
			ht_cap->ht_sgi_20, ht_cap->ht_sgi_40,
			ht_cap->num_rf_chains);
}

/**
 * wma_derive_ext_vht_cap() - Derive VHT caps based on given value
 * @vht_cap: pointer to given VHT caps to be filled
 * @value: new VHT cap info provided in form of bitmask
 *
 * This function takes the value provided in form of bitmask and decodes
 * it. After decoding, what ever value it gets, it takes the union(max) or
 * intersection(min) with previously derived values.
 *
 * Return: none
 *
 */
static void wma_derive_ext_vht_cap(
			struct wma_tgt_vht_cap *vht_cap, uint32_t value)
{
	struct wma_tgt_vht_cap tmp_cap = {0};
	uint32_t tmp = 0;

	if (!vht_cap)
		return;

	if (!qdf_mem_cmp(vht_cap, &tmp_cap,
				sizeof(struct wma_tgt_vht_cap))) {
		if (value & WMI_VHT_CAP_MAX_MPDU_LEN_11454)
			vht_cap->vht_max_mpdu = WMI_VHT_CAP_MAX_MPDU_LEN_11454;
		else if (value & WMI_VHT_CAP_MAX_MPDU_LEN_7935)
			vht_cap->vht_max_mpdu = WMI_VHT_CAP_MAX_MPDU_LEN_7935;
		else
			vht_cap->vht_max_mpdu = 0;

		if (value & WMI_VHT_CAP_CH_WIDTH_80P80_160MHZ) {
			vht_cap->supp_chan_width =
				1 << eHT_CHANNEL_WIDTH_80P80MHZ;
			vht_cap->supp_chan_width |=
				1 << eHT_CHANNEL_WIDTH_160MHZ;
		} else if (value & WMI_VHT_CAP_CH_WIDTH_160MHZ) {
			vht_cap->supp_chan_width =
				1 << eHT_CHANNEL_WIDTH_160MHZ;
		} else {
			vht_cap->supp_chan_width = 1 << eHT_CHANNEL_WIDTH_80MHZ;
		}
		vht_cap->vht_rx_ldpc = value & WMI_VHT_CAP_RX_LDPC;
		vht_cap->vht_short_gi_80 = value & WMI_VHT_CAP_SGI_80MHZ;
		vht_cap->vht_short_gi_160 = value & WMI_VHT_CAP_SGI_160MHZ;
		vht_cap->vht_tx_stbc = value & WMI_VHT_CAP_TX_STBC;
		vht_cap->vht_rx_stbc =
			(value & WMI_VHT_CAP_RX_STBC_1SS) |
			(value & WMI_VHT_CAP_RX_STBC_2SS) |
			(value & WMI_VHT_CAP_RX_STBC_3SS);
		vht_cap->vht_max_ampdu_len_exp =
			(value & WMI_VHT_CAP_MAX_AMPDU_LEN_EXP) >>
				WMI_VHT_CAP_MAX_AMPDU_LEN_EXP_SHIFT;
		vht_cap->vht_su_bformer = value & WMI_VHT_CAP_SU_BFORMER;
		vht_cap->vht_su_bformee = value & WMI_VHT_CAP_SU_BFORMEE;
		vht_cap->vht_mu_bformer = value & WMI_VHT_CAP_MU_BFORMER;
		vht_cap->vht_mu_bformee = value & WMI_VHT_CAP_MU_BFORMEE;
		vht_cap->vht_txop_ps = value & WMI_VHT_CAP_TXOP_PS;
	} else {
		if (value & WMI_VHT_CAP_MAX_MPDU_LEN_11454)
			tmp = WMI_VHT_CAP_MAX_MPDU_LEN_11454;
		else if (value & WMI_VHT_CAP_MAX_MPDU_LEN_7935)
			tmp = WMI_VHT_CAP_MAX_MPDU_LEN_7935;
		else
			tmp = 0;
		vht_cap->vht_max_mpdu = QDF_MIN(vht_cap->vht_max_mpdu, tmp);

		if ((value & WMI_VHT_CAP_CH_WIDTH_80P80_160MHZ)) {
			tmp = (1 << eHT_CHANNEL_WIDTH_80P80MHZ) |
				(1 << eHT_CHANNEL_WIDTH_160MHZ);
		} else if (value & WMI_VHT_CAP_CH_WIDTH_160MHZ) {
			tmp = 1 << eHT_CHANNEL_WIDTH_160MHZ;
		} else {
			tmp = 1 << eHT_CHANNEL_WIDTH_80MHZ;
		}
		vht_cap->supp_chan_width =
			QDF_MAX(vht_cap->supp_chan_width, tmp);
		vht_cap->vht_rx_ldpc = QDF_MIN(vht_cap->vht_rx_ldpc,
						value & WMI_VHT_CAP_RX_LDPC);
		vht_cap->vht_short_gi_80 = QDF_MAX(vht_cap->vht_short_gi_80,
						value & WMI_VHT_CAP_SGI_80MHZ);
		vht_cap->vht_short_gi_160 = QDF_MAX(vht_cap->vht_short_gi_160,
						value & WMI_VHT_CAP_SGI_160MHZ);
		vht_cap->vht_tx_stbc = QDF_MAX(vht_cap->vht_tx_stbc,
						value & WMI_VHT_CAP_TX_STBC);
		vht_cap->vht_rx_stbc = QDF_MIN(vht_cap->vht_rx_stbc,
					(value & WMI_VHT_CAP_RX_STBC_1SS) |
					(value & WMI_VHT_CAP_RX_STBC_2SS) |
					(value & WMI_VHT_CAP_RX_STBC_3SS));
		vht_cap->vht_max_ampdu_len_exp =
			QDF_MIN(vht_cap->vht_max_ampdu_len_exp,
				(value & WMI_VHT_CAP_MAX_AMPDU_LEN_EXP) >>
					WMI_VHT_CAP_MAX_AMPDU_LEN_EXP_SHIFT);
		vht_cap->vht_su_bformer = QDF_MAX(vht_cap->vht_su_bformer,
						value & WMI_VHT_CAP_SU_BFORMER);
		vht_cap->vht_su_bformee = QDF_MAX(vht_cap->vht_su_bformee,
						value & WMI_VHT_CAP_SU_BFORMEE);
		vht_cap->vht_mu_bformer = QDF_MAX(vht_cap->vht_mu_bformer,
						value & WMI_VHT_CAP_MU_BFORMER);
		vht_cap->vht_mu_bformee = QDF_MAX(vht_cap->vht_mu_bformee,
						value & WMI_VHT_CAP_MU_BFORMEE);
		vht_cap->vht_txop_ps = QDF_MIN(vht_cap->vht_txop_ps,
						value & WMI_VHT_CAP_TXOP_PS);
	}
}

/**
 * wma_update_target_ext_vht_cap() - Update VHT caps with given extended cap
 * @tgt_hdl - target psoc information
 * @vht_cap: VHT cap structure to be filled
 *
 * This function loop through each hardware mode and for each hardware mode
 * again it loop through each MAC/PHY and pull the caps 2G and 5G specific
 * VHT caps and derives the final cap.
 *
 * Return: none
 *
 */
static void wma_update_target_ext_vht_cap(struct target_psoc_info *tgt_hdl,
					  struct wma_tgt_vht_cap *vht_cap)
{
	int i, num_hw_modes, total_mac_phy_cnt;
	uint32_t vht_cap_info_2g, vht_cap_info_5g;
	struct wma_tgt_vht_cap tmp_vht_cap = {0}, tmp_cap = {0};
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap;
	uint32_t vht_mcs_10_11_supp = 0;

	total_mac_phy_cnt = target_psoc_get_total_mac_phy_cnt(tgt_hdl);
	num_hw_modes = target_psoc_get_num_hw_modes(tgt_hdl);

	mac_phy_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
	if (!mac_phy_cap) {
		wma_err("Invalid MAC PHY capabilities handle");
		return;
	}

	/*
	 * for legacy device extended cap might not even come, so in that case
	 * don't overwrite legacy values
	 */
	if (!num_hw_modes) {
		wma_debug("No extended VHT cap for current SOC");
		return;
	}

	for (i = 0; i < total_mac_phy_cnt; i++) {
		vht_cap_info_2g = mac_phy_cap[i].vht_cap_info_2G;
		vht_cap_info_5g = mac_phy_cap[i].vht_cap_info_5G;
		if (vht_cap_info_2g)
			wma_derive_ext_vht_cap(&tmp_vht_cap,
					vht_cap_info_2g);
		if (vht_cap_info_5g)
			wma_derive_ext_vht_cap(&tmp_vht_cap,
					vht_cap_info_5g);
		if (WMI_GET_BITS(mac_phy_cap[i].vht_supp_mcs_5G, 16, 2) &&
		    WMI_VHT_MCS_NOTIFY_EXT_SS_GET(mac_phy_cap[i].
			    vht_supp_mcs_5G))
			vht_mcs_10_11_supp = 1;
		if (WMI_GET_BITS(mac_phy_cap[i].vht_supp_mcs_2G, 16, 2) &&
		    WMI_VHT_MCS_NOTIFY_EXT_SS_GET(mac_phy_cap[i].
			    vht_supp_mcs_2G))
			vht_mcs_10_11_supp = 1;
	}

	if (qdf_mem_cmp(&tmp_cap, &tmp_vht_cap,
				sizeof(struct wma_tgt_vht_cap))) {
			qdf_mem_copy(vht_cap, &tmp_vht_cap,
					sizeof(struct wma_tgt_vht_cap));
	}
	vht_cap->vht_mcs_10_11_supp = vht_mcs_10_11_supp;
	wma_nofl_debug("[ext vhtcap] max_mpdu %d supp_chan_width %x rx_ldpc %x\n"
		"short_gi_80 %x tx_stbc %x rx_stbc %x txop_ps %x\n"
		"su_bformee %x mu_bformee %x max_ampdu_len_exp %d\n"
		"vht_mcs_10_11_supp %d",
		vht_cap->vht_max_mpdu, vht_cap->supp_chan_width,
		vht_cap->vht_rx_ldpc, vht_cap->vht_short_gi_80,
		vht_cap->vht_tx_stbc, vht_cap->vht_rx_stbc,
		vht_cap->vht_txop_ps, vht_cap->vht_su_bformee,
		vht_cap->vht_mu_bformee, vht_cap->vht_max_ampdu_len_exp,
		vht_cap->vht_mcs_10_11_supp);
}

static void
wma_update_sar_version(struct wlan_psoc_host_service_ext_param *param,
		       struct wma_tgt_cfg *cfg)
{
	cfg->sar_version = param ? param->sar_version : SAR_VERSION_1;
}

/**
 * wma_update_hdd_band_cap() - update band cap which hdd understands
 * @supported_band: supported band which has been given by FW
 * @tgt_cfg: target configuration to be updated
 * @psoc: psoc ptr
 *
 * Convert WMA given supported band to enum which HDD understands
 *
 * Return: None
 */
static void wma_update_hdd_band_cap(WMI_PHY_CAPABILITY supported_band,
				    struct wma_tgt_cfg *tgt_cfg,
				    struct wlan_objmgr_psoc *psoc)
{
	switch (supported_band) {
	case WMI_11G_CAPABILITY:
	case WMI_11NG_CAPABILITY:
		tgt_cfg->band_cap = BIT(REG_BAND_2G);
		break;
	case WMI_11A_CAPABILITY:
	case WMI_11NA_CAPABILITY:
	case WMI_11AC_CAPABILITY:
		tgt_cfg->band_cap = BIT(REG_BAND_5G);
		break;
	case WMI_11AG_CAPABILITY:
	case WMI_11NAG_CAPABILITY:
	case WMI_11AX_CAPABILITY:
		tgt_cfg->band_cap = (BIT(REG_BAND_2G) | BIT(REG_BAND_5G));
		if (wlan_reg_is_6ghz_supported(psoc))
			tgt_cfg->band_cap |= BIT(REG_BAND_6G);
		break;
	default:
		tgt_cfg->band_cap = (BIT(REG_BAND_2G) |
				     BIT(REG_BAND_5G) |
				     BIT(REG_BAND_6G));
	}
}

/**
 * wma_update_obss_detection_support() - update obss detection offload support
 * @wh: wma handle
 * @tgt_cfg: target configuration to be updated
 *
 * Update obss detection offload support based on service bit.
 *
 * Return: None
 */
static void wma_update_obss_detection_support(tp_wma_handle wh,
					      struct wma_tgt_cfg *tgt_cfg)
{
	if (wmi_service_enabled(wh->wmi_handle,
				wmi_service_ap_obss_detection_offload))
		tgt_cfg->obss_detection_offloaded = true;
	else
		tgt_cfg->obss_detection_offloaded = false;
}

/**
 * wma_update_obss_color_collision_support() - update obss color collision
 *   offload support
 * @wh: wma handle
 * @tgt_cfg: target configuration to be updated
 *
 * Update obss color collision offload support based on service bit.
 *
 * Return: None
 */
static void wma_update_obss_color_collision_support(tp_wma_handle wh,
						    struct wma_tgt_cfg *tgt_cfg)
{
	if (wmi_service_enabled(wh->wmi_handle, wmi_service_bss_color_offload))
		tgt_cfg->obss_color_collision_offloaded = true;
	else
		tgt_cfg->obss_color_collision_offloaded = false;
}

/**
 * wma_update_restricted_80p80_bw_support() - update restricted 80+80 supprot
 * @wh: wma handle
 * @tgt_cfg: target configuration to be updated
 *
 * Update restricted 80+80MHz (165MHz) BW support based on service bit.
 *
 * Return: None
 */
static void wma_update_restricted_80p80_bw_support(tp_wma_handle wh,
						   struct wma_tgt_cfg *tgt_cfg)
{
	if (wmi_service_enabled(wh->wmi_handle,
				wmi_service_bw_165mhz_support))
		tgt_cfg->restricted_80p80_bw_supp = true;
	else
		tgt_cfg->restricted_80p80_bw_supp = false;
}

#ifdef WLAN_SUPPORT_GREEN_AP
static void wma_green_ap_register_handlers(tp_wma_handle wma_handle)
{
	if (WMI_SERVICE_IS_ENABLED(wma_handle->wmi_service_bitmap,
				   WMI_SERVICE_EGAP))
		target_if_green_ap_register_egap_event_handler(
					wma_handle->pdev);

}
#else
static void wma_green_ap_register_handlers(tp_wma_handle wma_handle)
{
}
#endif

#ifdef WLAN_FEATURE_NAN
static void wma_update_nan_target_caps(tp_wma_handle wma_handle,
				       struct wma_tgt_cfg *tgt_cfg)
{
	if (wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_nan_disable_support))
		tgt_cfg->nan_caps.nan_conc_control = 1;

	if (wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_nan_dbs_support))
		tgt_cfg->nan_caps.nan_dbs_supported = 1;

	if (wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_ndi_dbs_support))
		tgt_cfg->nan_caps.ndi_dbs_supported = 1;

	if (wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_nan_sap_support))
		tgt_cfg->nan_caps.nan_sap_supported = 1;

	if (wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_ndi_sap_support))
		tgt_cfg->nan_caps.ndi_sap_supported = 1;

	if (wmi_service_enabled(wma_handle->wmi_handle, wmi_service_nan_vdev))
		tgt_cfg->nan_caps.nan_vdev_allowed = 1;

	if (wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_sta_nan_ndi_four_port))
		tgt_cfg->nan_caps.sta_nan_ndi_ndi_allowed = 1;

	if (wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_ndi_txbf_support))
		tgt_cfg->nan_caps.ndi_txbf_supported = 1;
}
#else
static void wma_update_nan_target_caps(tp_wma_handle wma_handle,
				       struct wma_tgt_cfg *tgt_cfg)
{
}
#endif

static uint8_t
wma_convert_chainmask_to_chain(uint8_t chainmask)
{
	uint8_t num_chains = 0;

	while (chainmask) {
		chainmask &= (chainmask - 1);
		num_chains++;
	}

	return num_chains;
}

static void
wma_fill_chain_cfg(struct target_psoc_info *tgt_hdl,
		   uint8_t phy)
{
	struct mac_context *mac_ctx;
	uint8_t num_chain;
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap =
						tgt_hdl->info.mac_phy_cap;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		wma_err("fill chain cfg failed as mac_ctx is NULL");
		return;
	}

	num_chain = wma_convert_chainmask_to_chain(mac_phy_cap[phy].
						   tx_chain_mask_2G);

	if (num_chain > mac_ctx->fw_chain_cfg.max_tx_chains_2g)
		mac_ctx->fw_chain_cfg.max_tx_chains_2g = num_chain;

	num_chain = wma_convert_chainmask_to_chain(mac_phy_cap[phy].
						   tx_chain_mask_5G);

	if (num_chain > mac_ctx->fw_chain_cfg.max_tx_chains_5g)
		mac_ctx->fw_chain_cfg.max_tx_chains_5g = num_chain;

	num_chain = wma_convert_chainmask_to_chain(mac_phy_cap[phy].
						   rx_chain_mask_2G);

	if (num_chain > mac_ctx->fw_chain_cfg.max_rx_chains_2g)
		mac_ctx->fw_chain_cfg.max_rx_chains_2g = num_chain;

	num_chain = wma_convert_chainmask_to_chain(mac_phy_cap[phy].
						   rx_chain_mask_5G);

	if (num_chain > mac_ctx->fw_chain_cfg.max_rx_chains_5g)
		mac_ctx->fw_chain_cfg.max_rx_chains_5g = num_chain;
}

static void wma_update_mlme_related_tgt_caps(struct wlan_objmgr_psoc *psoc,
					     struct wmi_unified *wmi_handle)
{
	struct mlme_tgt_caps mlme_tgt_cfg;

	mlme_tgt_cfg.data_stall_recovery_fw_support =
		wmi_service_enabled(wmi_handle,
				    wmi_service_data_stall_recovery_support);

	mlme_tgt_cfg.bigtk_support =
		wmi_service_enabled(wmi_handle, wmi_beacon_protection_support);

	mlme_tgt_cfg.stop_all_host_scan_support =
		wmi_service_enabled(wmi_handle,
				    wmi_service_host_scan_stop_vdev_all);
	mlme_tgt_cfg.dual_sta_roam_fw_support =
		wmi_service_enabled(wmi_handle,
				    wmi_service_dual_sta_roam_support);

	mlme_tgt_cfg.ocv_support =
		wmi_service_enabled(wmi_handle,
				    wmi_service_ocv_support);

	wma_debug("beacon protection support %d, ocv support %d",
		  mlme_tgt_cfg.bigtk_support, mlme_tgt_cfg.ocv_support);

	/* Call this at last only after filling all the tgt caps */
	wlan_mlme_update_cfg_with_tgt_caps(psoc, &mlme_tgt_cfg);
}

static bool
wma_is_dbs_mandatory(struct wlan_objmgr_psoc *psoc,
		     struct target_psoc_info *tgt_hdl)
{
	uint8_t i, total_mac_phy_cnt;
	struct wlan_psoc_host_mac_phy_caps *mac_cap, *mac_phy_cap;
	uint8_t supported_band = 0;

	if (!policy_mgr_find_if_fw_supports_dbs(psoc) ||
	    !policy_mgr_find_if_hwlist_has_dbs(psoc)) {
		wma_debug("DBS is not mandatory");
		return false;
	}

	total_mac_phy_cnt = target_psoc_get_total_mac_phy_cnt(tgt_hdl);
	mac_phy_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
	if (!mac_phy_cap) {
		wma_err("Invalid MAC PHY capabilities handle");
		return false;
	}


	for (i = 0; i < total_mac_phy_cnt; i++) {
		mac_cap = &mac_phy_cap[i];
		if (mac_cap && (mac_cap->phy_id == 0))
			supported_band |= mac_cap->supported_bands;
	}

	/* If Mac0 supports both the bands then DBS is not mandatory */
	if (supported_band & WLAN_2G_CAPABILITY &&
	    supported_band & WLAN_5G_CAPABILITY) {
		wma_debug("Mac0 supports both bands DBS is optional");
		return false;
	}

	wma_info("MAC0 does not support both bands %d DBS is mandatory",
		 supported_band);

	return true;
}

/**
 * wma_update_hdd_cfg() - update HDD config
 * @wma_handle: wma handle
 *
 * Return: Zero on success err number on failure
 */
static int wma_update_hdd_cfg(tp_wma_handle wma_handle)
{
	struct wma_tgt_cfg tgt_cfg;
	void *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	target_resource_config *wlan_res_cfg;
	struct wlan_psoc_host_service_ext_param *service_ext_param;
	struct target_psoc_info *tgt_hdl;
	struct wmi_unified *wmi_handle;
	uint8_t i;
	int ret;

	wma_debug("Enter");

	tgt_hdl = wlan_psoc_get_tgt_if_handle(wma_handle->psoc);
	if (!tgt_hdl) {
		wma_err("target psoc info is NULL");
		return -EINVAL;
	}

	wlan_res_cfg = target_psoc_get_wlan_res_cfg(tgt_hdl);
	if (!wlan_res_cfg) {
		wma_err("wlan_res_cfg is null");
		return -EINVAL;
	}

	service_ext_param =
			target_psoc_get_service_ext_param(tgt_hdl);
	wmi_handle = get_wmi_unified_hdl_from_psoc(wma_handle->psoc);
	if (!wmi_handle) {
		wma_err("wmi handle is NULL");
		return -EINVAL;
	}

	wma_update_mlme_related_tgt_caps(wma_handle->psoc, wmi_handle);

	if (wmi_service_enabled(wmi_handle, wmi_service_peer_create_conf))
		wlan_psoc_nif_fw_ext_cap_set(wma_handle->psoc,
					     WLAN_SOC_F_PEER_CREATE_RESP);

	qdf_mem_zero(&tgt_cfg, sizeof(struct wma_tgt_cfg));

	tgt_cfg.sub_20_support = wma_handle->sub_20_support;
	tgt_cfg.reg_domain = wma_handle->reg_cap.eeprom_rd;
	tgt_cfg.eeprom_rd_ext = wma_handle->reg_cap.eeprom_rd_ext;

	tgt_cfg.max_intf_count = wlan_res_cfg->num_vdevs;

	qdf_mem_copy(tgt_cfg.hw_macaddr.bytes, wma_handle->hwaddr,
		     ATH_MAC_LEN);

	wma_update_target_services(wmi_handle, &tgt_cfg.services);
	wma_update_target_ht_cap(tgt_hdl, &tgt_cfg.ht_cap);
	wma_update_target_vht_cap(tgt_hdl, &tgt_cfg.vht_cap);
	/*
	 * This will overwrite the structure filled by wma_update_target_ht_cap
	 * and wma_update_target_vht_cap APIs.
	 */
	wma_update_target_ext_ht_cap(tgt_hdl, &tgt_cfg.ht_cap);
	wma_update_target_ext_vht_cap(tgt_hdl, &tgt_cfg.vht_cap);

	wma_update_target_ext_he_cap(tgt_hdl, &tgt_cfg);

	tgt_cfg.target_fw_version = target_if_get_fw_version(tgt_hdl);
	if (service_ext_param)
		tgt_cfg.target_fw_vers_ext =
				service_ext_param->fw_build_vers_ext;

	tgt_cfg.hw_bd_id = wma_handle->hw_bd_id;
	tgt_cfg.hw_bd_info.bdf_version = wma_handle->hw_bd_info[BDF_VERSION];
	tgt_cfg.hw_bd_info.ref_design_id =
		wma_handle->hw_bd_info[REF_DESIGN_ID];
	tgt_cfg.hw_bd_info.customer_id = wma_handle->hw_bd_info[CUSTOMER_ID];
	tgt_cfg.hw_bd_info.project_id = wma_handle->hw_bd_info[PROJECT_ID];
	tgt_cfg.hw_bd_info.board_data_rev =
		wma_handle->hw_bd_info[BOARD_DATA_REV];

#ifdef WLAN_FEATURE_LPSS
	tgt_cfg.lpss_support = wma_handle->lpss_support;
#endif /* WLAN_FEATURE_LPSS */
	tgt_cfg.ap_arpns_support = wma_handle->ap_arpns_support;
	tgt_cfg.dfs_cac_offload = wma_handle->is_dfs_offloaded;
	tgt_cfg.rcpi_enabled = wma_handle->rcpi_enabled;
	wma_update_hdd_band_cap(target_if_get_phy_capability(tgt_hdl),
				&tgt_cfg, wma_handle->psoc);
	wma_update_sar_version(service_ext_param, &tgt_cfg);
	tgt_cfg.fine_time_measurement_cap =
		target_if_get_wmi_fw_sub_feat_caps(tgt_hdl);
	tgt_cfg.wmi_max_len = wmi_get_max_msg_len(wma_handle->wmi_handle)
			      - WMI_TLV_HEADROOM;
	tgt_cfg.tx_bfee_8ss_enabled = wma_handle->tx_bfee_8ss_enabled;
	tgt_cfg.dynamic_nss_chains_support =
				wma_handle->dynamic_nss_chains_support;
	wma_update_obss_detection_support(wma_handle, &tgt_cfg);
	wma_update_obss_color_collision_support(wma_handle, &tgt_cfg);
	wma_update_hdd_cfg_ndp(wma_handle, &tgt_cfg);
	wma_update_nan_target_caps(wma_handle, &tgt_cfg);
	wma_update_bcast_twt_support(wma_handle, &tgt_cfg);
	wma_update_twt_tgt_cap(wma_handle, &tgt_cfg);
	wma_update_restricted_80p80_bw_support(wma_handle, &tgt_cfg);
	/* Take the max of chains supported by FW, which will limit nss */
	for (i = 0; i < tgt_hdl->info.total_mac_phy_cnt; i++)
		wma_fill_chain_cfg(tgt_hdl, i);

	ret = wma_handle->tgt_cfg_update_cb(hdd_ctx, &tgt_cfg);
	if (ret)
		return -EINVAL;

	wma_green_ap_register_handlers(wma_handle);

	return ret;
}

/**
 * wma_dump_dbs_hw_mode() - Print the DBS HW modes
 * @wma_handle: WMA handle
 *
 * Prints the DBS HW modes sent by the FW as part
 * of WMI ready event
 *
 * Return: None
 */
static void wma_dump_dbs_hw_mode(tp_wma_handle wma_handle)
{
	uint32_t i, param;

	if (!wma_handle) {
		wma_err("Invalid WMA handle");
		return;
	}

	for (i = 0; i < wma_handle->num_dbs_hw_modes; i++) {
		param = wma_handle->hw_mode.hw_mode_list[i];
		wma_debug("[%d]-MAC0: tx_ss:%d rx_ss:%d bw_idx:%d",
			i,
			WMA_HW_MODE_MAC0_TX_STREAMS_GET(param),
			WMA_HW_MODE_MAC0_RX_STREAMS_GET(param),
			WMA_HW_MODE_MAC0_BANDWIDTH_GET(param));
		wma_debug("[%d]-MAC1: tx_ss:%d rx_ss:%d bw_idx:%d",
			i,
			WMA_HW_MODE_MAC1_TX_STREAMS_GET(param),
			WMA_HW_MODE_MAC1_RX_STREAMS_GET(param),
			WMA_HW_MODE_MAC1_BANDWIDTH_GET(param));
		wma_debug("[%d] DBS:%d SBS:%d", i,
			WMA_HW_MODE_DBS_MODE_GET(param),
			WMA_HW_MODE_SBS_MODE_GET(param));
	}
	policy_mgr_dump_dbs_hw_mode(wma_handle->psoc);
}

/**
 * wma_init_scan_fw_mode_config() - Initialize scan/fw mode config
 * @psoc: Object manager psoc
 * @scan_config: Scam mode configuration
 * @fw_config: FW mode configuration
 *
 * Enables all the valid bits of concurrent_scan_config_bits and
 * fw_mode_config_bits.
 *
 * Return: None
 */
static void wma_init_scan_fw_mode_config(struct wlan_objmgr_psoc *psoc,
					 uint32_t scan_config,
					 uint32_t fw_config)
{
	wma_debug("Enter");

	if (!psoc) {
		wma_err("obj psoc is NULL");
		return;
	}

	policy_mgr_init_dbs_config(psoc, scan_config, fw_config);

	wma_debug("Exit");
}

static void wma_set_pmo_caps(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	tp_wma_handle wma;
	struct pmo_device_caps caps;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("wma handler is null");
		return;
	}

	caps.arp_ns_offload =
		wmi_service_enabled(wma->wmi_handle, wmi_service_arpns_offload);
	caps.apf =
		wmi_service_enabled(wma->wmi_handle, wmi_service_apf_offload);
	caps.packet_filter =
		wmi_service_enabled(wma->wmi_handle,
				    wmi_service_packet_filter_offload);
	caps.unified_wow =
		wmi_service_enabled(wma->wmi_handle,
				    wmi_service_unified_wow_capability);
	caps.li_offload =
		wmi_service_enabled(wma->wmi_handle,
				    wmi_service_listen_interval_offload_support
				    );

	status = ucfg_pmo_psoc_set_caps(psoc, &caps);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to set PMO capabilities; status:%d", status);
}

/**
 * wma_set_mlme_caps() - Populate the MLME related target capabilities to the
 * mlme component
 * @psoc: Pointer to psoc object
 *
 * Return: None
 */
static void wma_set_mlme_caps(struct wlan_objmgr_psoc *psoc)
{
	tp_wma_handle wma;
	bool tgt_cap;
	uint32_t akm_bitmap = 0;
	QDF_STATUS status;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("wma handler is null");
		return;
	}

	tgt_cap = wmi_service_enabled(wma->wmi_handle,
				      wmi_service_adaptive_11r_support);

	status = ucfg_mlme_set_tgt_adaptive_11r_cap(psoc, tgt_cap);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to set adaptive 11r cap");

	tgt_cap = wmi_service_enabled(wma->wmi_handle,
				      wmi_service_wpa3_ft_sae_support);
	if (tgt_cap)
		 akm_bitmap |= (1 << AKM_FT_SAE);

	tgt_cap = wmi_service_enabled(wma->wmi_handle,
				      wmi_service_wpa3_ft_suite_b_support);
	if (tgt_cap)
		akm_bitmap |= (1 << AKM_FT_SUITEB_SHA384);

	tgt_cap = wmi_service_enabled(wma->wmi_handle,
				      wmi_service_ft_fils);
	if (tgt_cap)
		akm_bitmap |= (1 << AKM_FT_FILS);

	tgt_cap = wmi_service_enabled(wma->wmi_handle,
				      wmi_service_owe_roam_support);
	if (tgt_cap)
		akm_bitmap |= (1 << AKM_OWE);

	tgt_cap = wmi_service_enabled(wma->wmi_handle,
				      wmi_service_sae_roam_support);
	if (tgt_cap)
		akm_bitmap |= (1 << AKM_SAE);

	tgt_cap = wmi_service_enabled(wma->wmi_handle,
				      wmi_service_suiteb_roam_support);
	if (tgt_cap)
		akm_bitmap |= (1 << AKM_SUITEB);

	status = mlme_set_tgt_wpa3_roam_cap(psoc, akm_bitmap);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to set sae roam support");
}

#ifdef THERMAL_STATS_SUPPORT
static void wma_set_thermal_stats_fw_cap(tp_wma_handle wma,
					 struct wlan_fwol_capability_info *cap)
{
	cap->fw_thermal_stats_cap = wmi_service_enabled(wma->wmi_handle,
				wmi_service_thermal_stats_temp_range_supported);
}
#else
static void wma_set_thermal_stats_fw_cap(tp_wma_handle wma,
					 struct wlan_fwol_capability_info *cap)
{
}
#endif

/**
 * wma_set_fwol_caps() - Populate fwol component related capabilities
 *			 to the fwol component
 *
 * @psoc: Pointer to psoc object
 *
 * Return: None
 */
static void wma_set_fwol_caps(struct wlan_objmgr_psoc *psoc)
{
	tp_wma_handle wma;
	struct wlan_fwol_capability_info cap_info;
	wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err_rl("wma Null");
		return;
	}
	if (!psoc) {
		wma_err_rl("psoc Null");
		return;
	}

	wma_set_thermal_stats_fw_cap(wma, &cap_info);
	ucfg_fwol_update_fw_cap_info(psoc, &cap_info);
}

static void wma_set_component_caps(struct wlan_objmgr_psoc *psoc)
{
	wma_set_pmo_caps(psoc);
	wma_set_mlme_caps(psoc);
	wma_set_fwol_caps(psoc);
}

#if defined(WLAN_FEATURE_GTK_OFFLOAD) && defined(WLAN_POWER_MANAGEMENT_OFFLOAD)
static QDF_STATUS wma_register_gtk_offload_event(tp_wma_handle wma_handle)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!wma_handle) {
		wma_err("wma_handle passed is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_gtk_offload)) {
		status = wmi_unified_register_event_handler(
					wma_handle->wmi_handle,
					wmi_gtk_offload_status_event_id,
					target_if_pmo_gtk_offload_status_event,
					WMA_RX_WORK_CTX);
	}
	return status;
}
#else
static QDF_STATUS wma_register_gtk_offload_event(tp_wma_handle wma_handle)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_GTK_OFFLOAD && WLAN_POWER_MANAGEMENT_OFFLOAD */

/**
 * wma_rx_service_ready_event() - event handler to process
 *                                wmi rx sevice ready event.
 * @handle: wma handle
 * @cmd_param_info: command params info
 *
 * Return: none
 */
int wma_rx_service_ready_event(void *handle, uint8_t *cmd_param_info,
			       uint32_t length)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_SERVICE_READY_EVENTID_param_tlvs *param_buf;
	wmi_service_ready_event_fixed_param *ev;
	QDF_STATUS status;
	uint32_t *ev_wlan_dbs_hw_mode_list;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct target_psoc_info *tgt_hdl;
	struct wlan_psoc_target_capability_info *tgt_cap_info;
	target_resource_config *wlan_res_cfg;
	struct wmi_unified *wmi_handle;
	uint32_t *service_bitmap;

	wma_debug("Enter");

	if (!handle) {
		wma_err("wma_handle passed is NULL");
		return -EINVAL;
	}

	tgt_hdl = wlan_psoc_get_tgt_if_handle(wma_handle->psoc);
	if (!tgt_hdl) {
		wma_err("target psoc info is NULL");
		return -EINVAL;
	}

	wlan_res_cfg = target_psoc_get_wlan_res_cfg(tgt_hdl);
	tgt_cap_info = target_psoc_get_target_caps(tgt_hdl);
	service_bitmap = target_psoc_get_service_bitmap(tgt_hdl);

	param_buf = (WMI_SERVICE_READY_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid arguments");
		return -EINVAL;
	}

	ev = param_buf->fixed_param;
	if (!ev) {
		wma_err("Invalid buffer");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(wma_handle->psoc);
	if (!wmi_handle) {
		wma_err("wmi handle is NULL");
		return -EINVAL;
	}

	wma_debug("WMA <-- WMI_SERVICE_READY_EVENTID");

	if (ev->num_dbs_hw_modes > param_buf->num_wlan_dbs_hw_mode_list) {
		wma_err("FW dbs_hw_mode entry %d more than value %d in TLV hdr",
			ev->num_dbs_hw_modes,
			param_buf->num_wlan_dbs_hw_mode_list);
		return -EINVAL;
	}

	wma_handle->num_dbs_hw_modes = ev->num_dbs_hw_modes;
	ev_wlan_dbs_hw_mode_list = param_buf->wlan_dbs_hw_mode_list;

	/* Continuing with the rest of the processing,
	 * even if memory allocation fails
	 */
	wma_handle->hw_mode.hw_mode_list =
		qdf_mem_malloc(sizeof(*wma_handle->hw_mode.hw_mode_list) *
				wma_handle->num_dbs_hw_modes);

	if (wma_handle->hw_mode.hw_mode_list)
		qdf_mem_copy(wma_handle->hw_mode.hw_mode_list,
			     ev_wlan_dbs_hw_mode_list,
			     (sizeof(*wma_handle->hw_mode.hw_mode_list) *
			      wma_handle->num_dbs_hw_modes));

	policy_mgr_init_dbs_hw_mode(wma_handle->psoc,
	ev->num_dbs_hw_modes, ev_wlan_dbs_hw_mode_list);
	wma_dump_dbs_hw_mode(wma_handle);

	/* Initializes the fw_mode and scan_config to zero.
	 * If ext service ready event is present it will set
	 * the actual values of these two params.
	 * This is to ensure that no garbage values would be
	 * present in the absence of ext service ready event.
	 */
	wma_init_scan_fw_mode_config(wma_handle->psoc, 0, 0);

	qdf_mem_copy(&wma_handle->reg_cap, param_buf->hal_reg_capabilities,
				 sizeof(HAL_REG_CAPABILITIES));

	wma_handle->vht_supp_mcs = ev->vht_supp_mcs;

	wma_handle->new_hw_mode_index = tgt_cap_info->default_dbs_hw_mode_index;
	policy_mgr_update_new_hw_mode_index(wma_handle->psoc,
	tgt_cap_info->default_dbs_hw_mode_index);

	wma_debug("Firmware default hw mode index : %d",
		 tgt_cap_info->default_dbs_hw_mode_index);
	wma_info("Firmware build version : %08x",
		 ev->fw_build_vers);
	wma_debug("FW fine time meas cap: 0x%x",
		 tgt_cap_info->wmi_fw_sub_feat_caps);

	wma_handle->hw_bd_id = ev->hw_bd_id;

	wma_handle->hw_bd_info[BDF_VERSION] =
		WMI_GET_BDF_VERSION(ev->hw_bd_info);
	wma_handle->hw_bd_info[REF_DESIGN_ID] =
		WMI_GET_REF_DESIGN(ev->hw_bd_info);
	wma_handle->hw_bd_info[CUSTOMER_ID] =
		WMI_GET_CUSTOMER_ID(ev->hw_bd_info);
	wma_handle->hw_bd_info[PROJECT_ID] =
		WMI_GET_PROJECT_ID(ev->hw_bd_info);
	wma_handle->hw_bd_info[BOARD_DATA_REV] =
		WMI_GET_BOARD_DATA_REV(ev->hw_bd_info);

	wma_info("Board id: %x, Board version: %x %x %x %x %x",
		 wma_handle->hw_bd_id,
		 wma_handle->hw_bd_info[BDF_VERSION],
		 wma_handle->hw_bd_info[REF_DESIGN_ID],
		 wma_handle->hw_bd_info[CUSTOMER_ID],
		 wma_handle->hw_bd_info[PROJECT_ID],
		 wma_handle->hw_bd_info[BOARD_DATA_REV]);

	/* wmi service is ready */
	qdf_mem_copy(wma_handle->wmi_service_bitmap,
		     service_bitmap,
		     sizeof(wma_handle->wmi_service_bitmap));

	cdp_cfg_tx_set_is_mgmt_over_wmi_enabled(soc,
		wmi_service_enabled(wmi_handle, wmi_service_mgmt_tx_wmi));
	cdp_set_desc_global_pool_size(soc, ev->num_msdu_desc);
	/* SWBA event handler for beacon transmission */
	status = wma_register_swba_events(wma_handle->wmi_handle);

	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to register swba beacon event cb");
		goto free_hw_mode_list;
	}
#ifdef WLAN_FEATURE_LPSS
	wma_handle->lpss_support =
		wmi_service_enabled(wmi_handle, wmi_service_lpass);
#endif /* WLAN_FEATURE_LPSS */

	/*
	 * This Service bit is added to check for ARP/NS Offload
	 * support for LL/HL targets
	 */
	wma_handle->ap_arpns_support =
		wmi_service_enabled(wmi_handle, wmi_service_ap_arpns_offload);

	if (wmi_service_enabled(wmi_handle, wmi_service_csa_offload)) {
		wma_debug("FW support CSA offload capability");
		status = wmi_unified_register_event_handler(
						wmi_handle,
						wmi_csa_handling_event_id,
						wma_csa_offload_handler,
						WMA_RX_SERIALIZER_CTX);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("Failed to register CSA offload event cb");
			goto free_hw_mode_list;
		}
	}

	if (wmi_service_enabled(wmi_handle, wmi_service_mgmt_tx_wmi)) {
		wma_debug("Firmware supports management TX over WMI,use WMI interface instead of HTT for management Tx");
		/*
		 * Register Tx completion event handler for MGMT Tx over WMI
		 * case
		 */
		status = wmi_unified_register_event_handler(
					wmi_handle,
					wmi_mgmt_tx_completion_event_id,
					wma_mgmt_tx_completion_handler,
					WMA_RX_SERIALIZER_CTX);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("Failed to register MGMT over WMI completion handler");
			goto free_hw_mode_list;
		}

		status = wmi_unified_register_event_handler(
				wmi_handle,
				wmi_mgmt_tx_bundle_completion_event_id,
				wma_mgmt_tx_bundle_completion_handler,
				WMA_RX_SERIALIZER_CTX);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("Failed to register MGMT over WMI completion handler");
			goto free_hw_mode_list;
		}

	} else {
		wma_err("FW doesnot support WMI_SERVICE_MGMT_TX_WMI, Use HTT interface for Management Tx");
	}

	status = wma_register_gtk_offload_event(wma_handle);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to register GTK offload event cb");
		goto free_hw_mode_list;
	}

	status = wmi_unified_register_event_handler(wmi_handle,
				wmi_tbttoffset_update_event_id,
				wma_tbttoffset_update_event_handler,
				WMA_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to register WMI_TBTTOFFSET_UPDATE_EVENTID callback");
		goto free_hw_mode_list;
	}

	if (wmi_service_enabled(wma_handle->wmi_handle,
				   wmi_service_rcpi_support)) {
		/* register for rcpi response event */
		status = wmi_unified_register_event_handler(
							wmi_handle,
							wmi_update_rcpi_event_id,
							wma_rcpi_event_handler,
							WMA_RX_SERIALIZER_CTX);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("Failed to register RCPI event handler");
			goto free_hw_mode_list;
		}
		wma_handle->rcpi_enabled = true;
	}

	/* mac_id is replaced with pdev_id in converged firmware to have
	 * multi-radio support. In order to maintain backward compatibility
	 * with old fw, host needs to check WMI_SERVICE_DEPRECATED_REPLACE
	 * in service bitmap from FW and host needs to set use_pdev_id in
	 * wmi_resource_config to true. If WMI_SERVICE_DEPRECATED_REPLACE
	 * service is not set, then host shall not expect MAC ID from FW in
	 * VDEV START RESPONSE event and host shall use PDEV ID.
	 */
	if (wmi_service_enabled(wmi_handle, wmi_service_deprecated_replace))
		wlan_res_cfg->use_pdev_id = true;
	else
		wlan_res_cfg->use_pdev_id = false;

	wlan_res_cfg->max_num_dbs_scan_duty_cycle = CDS_DBS_SCAN_CLIENTS_MAX;

	/* Initialize the log supported event handler */
	status = wmi_unified_register_event_handler(wmi_handle,
			wmi_diag_event_id_log_supported_event_id,
			wma_log_supported_evt_handler,
			WMA_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to register log supported event cb");
		goto free_hw_mode_list;
	}

	cdp_mark_first_wakeup_packet(
		soc, OL_TXRX_PDEV_ID,
		wmi_service_enabled(wmi_handle,
				    wmi_service_mark_first_wakeup_packet));
	wma_handle->is_dfs_offloaded =
		wmi_service_enabled(wmi_handle,
				    wmi_service_dfs_phyerr_offload);

	wma_handle->nan_datapath_enabled =
		wmi_service_enabled(wma_handle->wmi_handle,
				    wmi_service_nan_data);

	wma_handle->fw_therm_throt_support =
		wmi_service_enabled(wma_handle->wmi_handle,
				    wmi_service_tt);

	wma_set_component_caps(wma_handle->psoc);

	wma_update_fw_config(wma_handle->psoc, tgt_hdl);

	status = wmi_unified_save_fw_version_cmd(wmi_handle, param_buf);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to send WMI_INIT_CMDID command");
		goto free_hw_mode_list;
	}

	if (wmi_service_enabled(wmi_handle, wmi_service_ext_msg)) {
		status = qdf_mc_timer_start(
				&wma_handle->service_ready_ext_timer,
				WMA_SERVICE_READY_EXT_TIMEOUT);
		if (QDF_IS_STATUS_ERROR(status))
			wma_err("Failed to start the service ready ext timer");
	}
	wma_handle->tx_bfee_8ss_enabled =
		wmi_service_enabled(wmi_handle, wmi_service_8ss_tx_bfee);

	wma_handle->dynamic_nss_chains_support = wmi_service_enabled(wmi_handle,
					wmi_service_per_vdev_chain_support);
	target_psoc_set_num_radios(tgt_hdl, 1);

	return 0;

free_hw_mode_list:
	if (wma_handle->hw_mode.hw_mode_list) {
		qdf_mem_free(wma_handle->hw_mode.hw_mode_list);
		wma_handle->hw_mode.hw_mode_list = NULL;
		wma_debug("DBS list is freed");
	}

	return -EINVAL;

}

/**
 * wma_get_caps_for_phyidx_hwmode() - to fetch caps for given hw mode and band
 * @caps_per_phy: Pointer to capabilities structure which needs to be filled
 * @hw_mode: Provided hardware mode
 * @band: Provide band i.e. 2G or 5G
 *
 * This API finds cap which suitable for provided hw mode and band. If user
 * is provides some invalid hw mode then it will automatically falls back to
 * default hw mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_get_caps_for_phyidx_hwmode(struct wma_caps_per_phy *caps_per_phy,
		enum hw_mode_dbs_capab hw_mode, enum cds_band_type band)
{
	t_wma_handle *wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	struct target_psoc_info *tgt_hdl;
	int ht_cap_info, vht_cap_info;
	uint8_t our_hw_mode = hw_mode, num_hw_modes;
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap;
	struct wlan_psoc_target_capability_info *tgt_cap_info;
	uint8_t total_mac_phy_cnt, i;

	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return QDF_STATUS_E_FAILURE;
	}

	tgt_hdl = wlan_psoc_get_tgt_if_handle(wma_handle->psoc);
	if (!tgt_hdl) {
		wma_err("target psoc info is NULL");
		return -EINVAL;
	}
	if (!caps_per_phy) {
		wma_err("Invalid caps pointer");
		return QDF_STATUS_E_FAILURE;
	}

	ht_cap_info = target_if_get_ht_cap_info(tgt_hdl);
	vht_cap_info = target_if_get_vht_cap_info(tgt_hdl);
	num_hw_modes = target_psoc_get_num_hw_modes(tgt_hdl);
	mac_phy_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
	tgt_cap_info = target_psoc_get_target_caps(tgt_hdl);

	if (!mac_phy_cap) {
		wma_err("Invalid MAC PHY capabilities handle");
		return QDF_STATUS_E_FAILURE;
	}

	if (!tgt_cap_info) {
		wma_err("Invalid target capabilities handle");
		return QDF_STATUS_E_FAILURE;
	}

	if (!num_hw_modes) {
		wma_debug("Invalid number of hw modes, use legacy HT/VHT caps");
		caps_per_phy->ht_2g = ht_cap_info;
		caps_per_phy->ht_5g = ht_cap_info;
		caps_per_phy->vht_2g = vht_cap_info;
		caps_per_phy->vht_5g = vht_cap_info;
		/* legacy platform doesn't support HE IE */
		caps_per_phy->he_2g[0] = 0;
		caps_per_phy->he_2g[1] = 0;
		caps_per_phy->he_5g[0] = 0;
		caps_per_phy->he_5g[1] = 0;
		caps_per_phy->tx_chain_mask_2G =
			EXTRACT_TX_CHAIN_MASK_2G(tgt_cap_info->txrx_chainmask);
		caps_per_phy->rx_chain_mask_2G =
			EXTRACT_RX_CHAIN_MASK_2G(tgt_cap_info->txrx_chainmask);
		caps_per_phy->tx_chain_mask_5G =
			EXTRACT_TX_CHAIN_MASK_5G(tgt_cap_info->txrx_chainmask);
		caps_per_phy->rx_chain_mask_5G =
			EXTRACT_RX_CHAIN_MASK_5G(tgt_cap_info->txrx_chainmask);

		return QDF_STATUS_SUCCESS;
	}

	if (!policy_mgr_is_dbs_enable(wma_handle->psoc))
		our_hw_mode = HW_MODE_DBS_NONE;

	total_mac_phy_cnt = target_psoc_get_total_mac_phy_cnt(tgt_hdl);
	for (i = 0; i < total_mac_phy_cnt; i++) {
		if (our_hw_mode == HW_MODE_DBS &&
		    mac_phy_cap[i].hw_mode_config_type != WMI_HW_MODE_DBS)
			continue;

		if ((band == CDS_BAND_2GHZ || band == CDS_BAND_ALL) &&
		    (WLAN_2G_CAPABILITY & mac_phy_cap[i].supported_bands) &&
		    !caps_per_phy->tx_chain_mask_2G) {
			caps_per_phy->ht_2g = mac_phy_cap[i].ht_cap_info_2G;
			caps_per_phy->vht_2g = mac_phy_cap[i].vht_cap_info_2G;
			qdf_mem_copy(caps_per_phy->he_2g,
				     mac_phy_cap[i].he_cap_info_2G,
				     sizeof(caps_per_phy->he_2g));

			caps_per_phy->tx_chain_mask_2G =
					mac_phy_cap[i].tx_chain_mask_2G;
			caps_per_phy->rx_chain_mask_2G =
					mac_phy_cap[i].rx_chain_mask_2G;

			wma_debug("Select 2G capable phyid[%d] chain %d %d ht 0x%x vht 0x%x",
				 i,
				 caps_per_phy->tx_chain_mask_2G,
				 caps_per_phy->rx_chain_mask_2G,
				 caps_per_phy->ht_2g,
				 caps_per_phy->vht_2g);
		}
		if ((band == CDS_BAND_5GHZ || band == CDS_BAND_ALL) &&
		    (WLAN_5G_CAPABILITY & mac_phy_cap[i].supported_bands) &&
		    !caps_per_phy->tx_chain_mask_5G) {
			caps_per_phy->ht_5g = mac_phy_cap[i].ht_cap_info_5G;
			caps_per_phy->vht_5g = mac_phy_cap[i].vht_cap_info_5G;
			qdf_mem_copy(caps_per_phy->he_5g,
				     mac_phy_cap[i].he_cap_info_5G,
				     sizeof(caps_per_phy->he_5g));

			caps_per_phy->tx_chain_mask_5G =
					mac_phy_cap[i].tx_chain_mask_5G;
			caps_per_phy->rx_chain_mask_5G =
					mac_phy_cap[i].rx_chain_mask_5G;

			wma_debug("Select 5G capable phyid[%d] chain %d %d ht 0x%x vht 0x%x",
				 i,
				 caps_per_phy->tx_chain_mask_5G,
				 caps_per_phy->rx_chain_mask_5G,
				 caps_per_phy->ht_5g,
				 caps_per_phy->vht_5g);
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_is_rx_ldpc_supported_for_channel() - to find out if ldpc is supported
 *
 * @ch_freq: Channel freq for which it needs to check if rx ldpc is enabled
 *
 * This API takes channel number as argument and takes default hw mode as DBS
 * to check if rx LDPC support is enabled for that channel or no
 */
bool wma_is_rx_ldpc_supported_for_channel(uint32_t ch_freq)
{
	t_wma_handle *wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	struct target_psoc_info *tgt_hdl;
	struct wma_caps_per_phy caps_per_phy = {0};
	enum cds_band_type band;
	bool status;
	uint8_t num_hw_modes;

	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return false;
	}

	tgt_hdl = wlan_psoc_get_tgt_if_handle(wma_handle->psoc);
	if (!tgt_hdl) {
		wma_err("Target handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	num_hw_modes = target_psoc_get_num_hw_modes(tgt_hdl);

	if (!WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq))
		band = CDS_BAND_5GHZ;
	else
		band = CDS_BAND_2GHZ;

	if (QDF_STATUS_SUCCESS != wma_get_caps_for_phyidx_hwmode(
						&caps_per_phy,
						HW_MODE_DBS, band)) {
		return false;
	}

	/*
	 * Legacy platforms like Rome set WMI_HT_CAP_LDPC to specify RX LDPC
	 * capability. But new platforms like Helium set WMI_HT_CAP_RX_LDPC
	 * instead.
	 */
	if (0 == num_hw_modes) {
		status = (!!(caps_per_phy.ht_2g & WMI_HT_CAP_LDPC));
	} else {
		if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq))
			status = (!!(caps_per_phy.ht_2g & WMI_HT_CAP_RX_LDPC));
		else
			status = (!!(caps_per_phy.ht_5g & WMI_HT_CAP_RX_LDPC));
	}

	return status;
}

/**
 * wma_print_mac_phy_capabilities() - Prints MAC PHY capabilities
 * @cap: pointer to WMI_MAC_PHY_CAPABILITIES
 * @index: MAC_PHY index
 *
 * Return: none
 */
static void wma_print_mac_phy_capabilities(struct wlan_psoc_host_mac_phy_caps
					   *cap, int index)
{
	uint32_t mac_2G[PSOC_HOST_MAX_MAC_SIZE];
	uint32_t mac_5G[PSOC_HOST_MAX_MAC_SIZE];
	uint32_t phy_2G[WMI_MAX_HECAP_PHY_SIZE];
	uint32_t phy_5G[WMI_MAX_HECAP_PHY_SIZE];
	struct wlan_psoc_host_ppe_threshold ppet_2G, ppet_5G;

	wma_nofl_debug("\t: index [%d]", index);
	wma_nofl_debug("\t: cap for hw_mode_id[%d]", cap->hw_mode_id);
	wma_nofl_debug("\t: pdev_id[%d]", cap->pdev_id);
	wma_nofl_debug("\t: phy_id[%d]", cap->phy_id);
	wma_nofl_debug("\t: hw_mode_config_type[%d]", cap->hw_mode_config_type);
	wma_nofl_debug("\t: supports_11b[%d]", cap->supports_11b);
	wma_nofl_debug("\t: supports_11g[%d]", cap->supports_11g);
	wma_nofl_debug("\t: supports_11a[%d]", cap->supports_11a);
	wma_nofl_debug("\t: supports_11n[%d]", cap->supports_11n);
	wma_nofl_debug("\t: supports_11ac[%d]", cap->supports_11ac);
	wma_nofl_debug("\t: supports_11ax[%d]", cap->supports_11ax);
	wma_nofl_debug("\t: supported_bands[%d]", cap->supported_bands);
	wma_nofl_debug("\t: ampdu_density[%d]", cap->ampdu_density);
	wma_nofl_debug("\t: max_bw_supported_2G[%d]", cap->max_bw_supported_2G);
	wma_nofl_debug("\t: ht_cap_info_2G[%d]", cap->ht_cap_info_2G);
	wma_nofl_debug("\t: vht_cap_info_2G[0x%0X]", cap->vht_cap_info_2G);
	wma_nofl_debug("\t: vht_supp_mcs_2G[0x%0X]", cap->vht_supp_mcs_2G);
	wma_nofl_debug("\t: tx_chain_mask_2G[%d]", cap->tx_chain_mask_2G);
	wma_nofl_debug("\t: rx_chain_mask_2G[%d]", cap->rx_chain_mask_2G);
	wma_nofl_debug("\t: max_bw_supported_5G[%d]", cap->max_bw_supported_5G);
	wma_nofl_debug("\t: ht_cap_info_5G[%d]", cap->ht_cap_info_5G);
	wma_nofl_debug("\t: vht_cap_info_5G[0x%0X]", cap->vht_cap_info_5G);
	wma_nofl_debug("\t: vht_supp_mcs_5G[0x%0X]", cap->vht_supp_mcs_5G);
	wma_nofl_debug("\t: tx_chain_mask_5G[%d]", cap->tx_chain_mask_5G);
	wma_nofl_debug("\t: rx_chain_mask_5G[%d]", cap->rx_chain_mask_5G);
	wma_nofl_debug("\t: he_cap_info_2G[0][%08x]", cap->he_cap_info_2G[0]);
	wma_nofl_debug("\t: he_cap_info_2G[1][%08x]", cap->he_cap_info_2G[1]);
	wma_nofl_debug("\t: he_supp_mcs_2G[%08x]", cap->he_supp_mcs_2G);
	wma_nofl_debug("\t: he_cap_info_5G[0][%08x]", cap->he_cap_info_5G[0]);
	wma_nofl_debug("\t: he_cap_info_5G[1][%08x]", cap->he_cap_info_5G[1]);
	wma_nofl_debug("\t: he_supp_mcs_5G[%08x]", cap->he_supp_mcs_5G);
	qdf_mem_copy(mac_2G, cap->he_cap_info_2G, sizeof(mac_2G));
	qdf_mem_copy(mac_5G, cap->he_cap_info_5G, sizeof(mac_5G));
	qdf_mem_copy(phy_2G, cap->he_cap_phy_info_2G,
		     WMI_MAX_HECAP_PHY_SIZE * 4);
	qdf_mem_copy(phy_5G, cap->he_cap_phy_info_5G,
		     WMI_MAX_HECAP_PHY_SIZE * 4);
	ppet_2G = cap->he_ppet2G;
	ppet_5G = cap->he_ppet5G;

	wma_print_he_mac_cap_w1(mac_2G[0]);
	wma_print_he_mac_cap_w2(mac_2G[1]);
	wma_print_he_phy_cap(phy_2G);
	wma_print_he_ppet(&ppet_2G);
	wma_print_he_mac_cap_w1(mac_5G[0]);
	wma_print_he_mac_cap_w1(mac_5G[1]);
	wma_print_he_phy_cap(phy_5G);
	wma_print_he_ppet(&ppet_5G);
}

/**
 * wma_print_populate_soc_caps() - Prints all the caps populated per hw mode
 * @tgt_info: target related info
 *
 * This function prints all the caps populater per hw mode and per PHY
 *
 * Return: none
 */
static void wma_print_populate_soc_caps(struct target_psoc_info *tgt_hdl)
{
	int i, num_hw_modes, total_mac_phy_cnt;
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap, *tmp;

	num_hw_modes = target_psoc_get_num_hw_modes(tgt_hdl);
	total_mac_phy_cnt = target_psoc_get_total_mac_phy_cnt(tgt_hdl);

	/* print number of hw modes */
	wma_debug("num of hw modes [%d]", num_hw_modes);
	wma_debug("num mac_phy_cnt [%d]", total_mac_phy_cnt);

	mac_phy_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
	if (!mac_phy_cap) {
		wma_err("Invalid MAC PHY capabilities handle");
		return;
	}

	wma_debug("<====== HW mode cap printing starts ======>");
	/* print cap of each hw mode */
	for (i = 0; i < total_mac_phy_cnt; i++) {
		if (&mac_phy_cap[i]) {
			wma_nofl_debug("====>: hw mode id[%d], phy id[%d]",
				 mac_phy_cap[i].hw_mode_id,
				 mac_phy_cap[i].phy_id);
			tmp = &mac_phy_cap[i];
			wma_print_mac_phy_capabilities(tmp, i);
		}
	}
	wma_debug("<====== HW mode cap printing ends ======>\n");
}

/**
 * wma_map_wmi_channel_width_to_hw_mode_bw() - returns bandwidth
 * in terms of hw_mode_bandwidth
 * @width: bandwidth in terms of wmi_channel_width
 *
 * This function returns the bandwidth in terms of hw_mode_bandwidth.
 *
 * Return: BW in terms of hw_mode_bandwidth.
 */
static enum hw_mode_bandwidth wma_map_wmi_channel_width_to_hw_mode_bw(
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

/**
 * wma_get_hw_mode_params() - get TX-RX stream and bandwidth
 * supported from the capabilities.
 * @caps: PHY capability
 * @info: param to store TX-RX stream and BW information
 *
 * This function will calculate TX-RX stream and bandwidth supported
 * as per the PHY capability, and assign to mac_ss_bw_info.
 *
 * Return: none
 */
static void wma_get_hw_mode_params(struct wlan_psoc_host_mac_phy_caps *caps,
			struct mac_ss_bw_info *info)
{
	if (!caps) {
		wma_err("Invalid capabilities");
		return;
	}

	info->mac_tx_stream = wma_get_num_of_setbits_from_bitmask(
				QDF_MAX(caps->tx_chain_mask_2G,
					caps->tx_chain_mask_5G));
	info->mac_rx_stream = wma_get_num_of_setbits_from_bitmask(
				QDF_MAX(caps->rx_chain_mask_2G,
					caps->rx_chain_mask_5G));
	info->mac_bw = wma_map_wmi_channel_width_to_hw_mode_bw(
				QDF_MAX(caps->max_bw_supported_2G,
					caps->max_bw_supported_5G));
}

/**
 * wma_set_hw_mode_params() - sets TX-RX stream, bandwidth and
 * DBS in hw_mode_list
 * @wma_handle: pointer to wma global structure
 * @mac0_ss_bw_info: TX-RX streams, BW for MAC0
 * @mac1_ss_bw_info: TX-RX streams, BW for MAC1
 * @pos: refers to hw_mode_index
 * @dbs_mode: dbs_mode for the dbs_hw_mode
 * @sbs_mode: sbs_mode for the sbs_hw_mode
 *
 * This function sets TX-RX stream, bandwidth and DBS mode in
 * hw_mode_list.
 *
 * Return: none
 */
static void wma_set_hw_mode_params(t_wma_handle *wma_handle,
			struct mac_ss_bw_info mac0_ss_bw_info,
			struct mac_ss_bw_info mac1_ss_bw_info,
			uint32_t pos, uint32_t dbs_mode,
			uint32_t sbs_mode)
{
	WMA_HW_MODE_MAC0_TX_STREAMS_SET(
		wma_handle->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_tx_stream);
	WMA_HW_MODE_MAC0_RX_STREAMS_SET(
		wma_handle->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_rx_stream);
	WMA_HW_MODE_MAC0_BANDWIDTH_SET(
		wma_handle->hw_mode.hw_mode_list[pos],
		mac0_ss_bw_info.mac_bw);
	WMA_HW_MODE_MAC1_TX_STREAMS_SET(
		wma_handle->hw_mode.hw_mode_list[pos],
		mac1_ss_bw_info.mac_tx_stream);
	WMA_HW_MODE_MAC1_RX_STREAMS_SET(
		wma_handle->hw_mode.hw_mode_list[pos],
		mac1_ss_bw_info.mac_rx_stream);
	WMA_HW_MODE_MAC1_BANDWIDTH_SET(
		wma_handle->hw_mode.hw_mode_list[pos],
		mac1_ss_bw_info.mac_bw);
	WMA_HW_MODE_DBS_MODE_SET(
		wma_handle->hw_mode.hw_mode_list[pos],
		dbs_mode);
	WMA_HW_MODE_AGILE_DFS_SET(
		wma_handle->hw_mode.hw_mode_list[pos],
		HW_MODE_AGILE_DFS_NONE);
	WMA_HW_MODE_SBS_MODE_SET(
		wma_handle->hw_mode.hw_mode_list[pos],
		sbs_mode);
}

/**
 * wma_update_hw_mode_list() - updates hw_mode_list
 * @wma_handle: pointer to wma global structure
 * @tgt_hdl - target psoc information
 *
 * This function updates hw_mode_list with tx_streams, rx_streams,
 * bandwidth, dbs and agile dfs for each hw_mode.
 *
 * Returns: 0 for success else failure.
 */
static QDF_STATUS wma_update_hw_mode_list(t_wma_handle *wma_handle,
					  struct target_psoc_info *tgt_hdl)
{
	struct wlan_psoc_host_mac_phy_caps *tmp, *mac_phy_cap;
	uint32_t i, hw_config_type, j = 0;
	uint32_t dbs_mode, sbs_mode;
	struct mac_ss_bw_info mac0_ss_bw_info = {0};
	struct mac_ss_bw_info mac1_ss_bw_info = {0};
	WMI_PHY_CAPABILITY new_supported_band = 0;
	bool supported_band_update_failure = false;
	struct wlan_psoc_target_capability_info *tgt_cap_info;
	int num_hw_modes;

	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return QDF_STATUS_E_FAILURE;
	}

	num_hw_modes = target_psoc_get_num_hw_modes(tgt_hdl);
	mac_phy_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
	tgt_cap_info = target_psoc_get_target_caps(tgt_hdl);

	if (!mac_phy_cap) {
		wma_err("mac_phy_cap Null");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * This list was updated as part of service ready event. Re-populate
	 * HW mode list from the device capabilities.
	 */
	if (wma_handle->hw_mode.hw_mode_list) {
		qdf_mem_free(wma_handle->hw_mode.hw_mode_list);
		wma_handle->hw_mode.hw_mode_list = NULL;
		wma_debug("DBS list is freed");
	}

	wma_handle->hw_mode.hw_mode_list =
		qdf_mem_malloc(sizeof(*wma_handle->hw_mode.hw_mode_list) *
			       num_hw_modes);
	if (!wma_handle->hw_mode.hw_mode_list) {
		wma_handle->num_dbs_hw_modes = 0;
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("Updated HW mode list: Num modes:%d",
		 num_hw_modes);

	wma_handle->num_dbs_hw_modes = num_hw_modes;
	for (i = 0; i < num_hw_modes; i++) {
		/* Update for MAC0 */
		tmp = &mac_phy_cap[j++];
		wma_get_hw_mode_params(tmp, &mac0_ss_bw_info);
		hw_config_type = tmp->hw_mode_config_type;
		dbs_mode = HW_MODE_DBS_NONE;
		sbs_mode = HW_MODE_SBS_NONE;
		mac1_ss_bw_info.mac_tx_stream = 0;
		mac1_ss_bw_info.mac_rx_stream = 0;
		mac1_ss_bw_info.mac_bw = 0;
		if (wma_update_supported_bands(tmp->supported_bands,
						&new_supported_band)
		   != QDF_STATUS_SUCCESS)
			supported_band_update_failure = true;

		/* SBS and DBS have dual MAC. Upto 2 MACs are considered. */
		if ((hw_config_type == WMI_HW_MODE_DBS) ||
		    (hw_config_type == WMI_HW_MODE_SBS_PASSIVE) ||
		    (hw_config_type == WMI_HW_MODE_SBS)) {
			/* Update for MAC1 */
			tmp = &mac_phy_cap[j++];
			wma_get_hw_mode_params(tmp, &mac1_ss_bw_info);
			if (hw_config_type == WMI_HW_MODE_DBS)
				dbs_mode = HW_MODE_DBS;
			if ((hw_config_type == WMI_HW_MODE_SBS_PASSIVE) ||
			    (hw_config_type == WMI_HW_MODE_SBS))
				sbs_mode = HW_MODE_SBS;
			if (QDF_STATUS_SUCCESS !=
			wma_update_supported_bands(tmp->supported_bands,
						&new_supported_band))
				supported_band_update_failure = true;
		}

		/* Updating HW mode list */
		wma_set_hw_mode_params(wma_handle, mac0_ss_bw_info,
				       mac1_ss_bw_info, i, dbs_mode,
				       sbs_mode);
	}

	/* overwrite phy_capability which we got from service ready event */
	if (!supported_band_update_failure) {
		wma_debug("updating supported band from old[%d] to new[%d]",
			 target_if_get_phy_capability(tgt_hdl),
			 new_supported_band);
		target_if_set_phy_capability(tgt_hdl, new_supported_band);
	}

	if (QDF_STATUS_SUCCESS !=
			policy_mgr_update_hw_mode_list(wma_handle->psoc,
						       tgt_hdl))
		wma_err("failed to update policy manager");
	wma_dump_dbs_hw_mode(wma_handle);
	return QDF_STATUS_SUCCESS;
}

static void wma_init_wifi_pos_dma_rings(t_wma_handle *wma_handle,
					uint8_t num_mac, void *buf)
{
	struct hif_opaque_softc *hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	void *hal_soc;

	if (!hif_ctx) {
		wma_err("invalid hif context");
		return;
	}

	hal_soc = hif_get_hal_handle(hif_ctx);

	wifi_pos_init_cir_cfr_rings(wma_handle->psoc, hal_soc, num_mac, buf);
}

/**
 * wma_populate_soc_caps() - populate entire SOC's capabilities
 * @wma_handle: pointer to wma global structure
 * @tgt_hdl: target psoc information
 * @param_buf: pointer to param of service ready extension event from fw
 *
 * This API populates all capabilities of entire SOC. For example,
 * how many number of hw modes are supported by this SOC, what are the
 * capabilities of each phy per hw mode, what are HAL reg capabilities per
 * phy.
 *
 * Return: none
 */
static void wma_populate_soc_caps(t_wma_handle *wma_handle,
				  struct target_psoc_info *tgt_hdl,
			WMI_SERVICE_READY_EXT_EVENTID_param_tlvs *param_buf)
{

	wma_debug("Enter");

	wma_init_wifi_pos_dma_rings(wma_handle,
				    param_buf->num_oem_dma_ring_caps,
				    param_buf->oem_dma_ring_caps);

	wma_print_populate_soc_caps(tgt_hdl);
	wma_debug("Exit");
}

/**
 * wma_init_dbr_params() - init dbr params
 * @wma_handle: pointer to wma global structure
 *
 * This API initializes params of direct buffer rx component.
 *
 * Return: none
 */
#ifdef DIRECT_BUF_RX_ENABLE
static void wma_init_dbr_params(t_wma_handle *wma_handle)
{
	struct hif_opaque_softc *hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	void *hal_soc;

	if (!hif_ctx) {
		wma_err("invalid hif context");
		return;
	}

	hal_soc = hif_get_hal_handle(hif_ctx);
	direct_buf_rx_target_attach(wma_handle->psoc, hal_soc,
				    wma_handle->qdf_dev);
}
#else
static inline void wma_init_dbr_params(t_wma_handle *wma_handle)
{
}
#endif

/**
 * wma_set_coex_res_cfg() - Set target COEX resource configuration.
 * @wma_handle: pointer to wma global structure
 * @wlan_res_cfg: Pointer to target resource configuration
 *
 * Return: none
 */
#ifdef FEATURE_COEX_CONFIG
static void wma_set_coex_res_cfg(t_wma_handle *wma_handle,
				 struct wmi_unified *wmi_handle,
				 target_resource_config *wlan_res_cfg)
{
	if (cfg_get(wma_handle->psoc, CFG_THREE_WAY_COEX_CONFIG_LEGACY) &&
	    wmi_service_enabled(wmi_handle,
				wmi_service_three_way_coex_config_legacy)) {
		wlan_res_cfg->three_way_coex_config_legacy_en = true;
	} else {
		wlan_res_cfg->three_way_coex_config_legacy_en = false;
	}
}
#else
static void wma_set_coex_res_cfg(t_wma_handle *wma_handle,
				 struct wmi_unified *wmi_handle,
				 target_resource_config *wlan_res_cfg)
{
}
#endif

/**
 * wma_rx_service_ready_ext_event() - evt handler for sevice ready ext event.
 * @handle: wma handle
 * @event: params of the service ready extended event
 * @length: param length
 *
 * Return: none
 */
int wma_rx_service_ready_ext_event(void *handle, uint8_t *event,
					uint32_t length)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_SERVICE_READY_EXT_EVENTID_param_tlvs *param_buf;
	wmi_service_ready_ext_event_fixed_param *ev;
	QDF_STATUS ret;
	struct target_psoc_info *tgt_hdl;
	struct wlan_psoc_target_capability_info *tgt_cap_info;
	uint32_t conc_scan_config_bits, fw_config_bits;
	struct wmi_unified *wmi_handle;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	target_resource_config *wlan_res_cfg;
	uint8_t sta_sap_scc_on_dfs_chnl;

	wma_debug("Enter");

	if (!wma_handle) {
		wma_err("Invalid WMA handle");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(wma_handle->psoc);

	tgt_hdl = wlan_psoc_get_tgt_if_handle(wma_handle->psoc);
	if (!tgt_hdl) {
		wma_err("target psoc info is NULL");
		return -EINVAL;
	}

	wlan_res_cfg = target_psoc_get_wlan_res_cfg(tgt_hdl);
	param_buf = (WMI_SERVICE_READY_EXT_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		wma_err("Invalid event");
		return -EINVAL;
	}

	ev = param_buf->fixed_param;
	if (!ev) {
		wma_err("Invalid buffer");
		return -EINVAL;
	}

	wma_debug("WMA <-- WMI_SERVICE_READY_EXT_EVENTID");

	tgt_cap_info = target_psoc_get_target_caps(tgt_hdl);
	fw_config_bits = target_if_get_fw_config_bits(tgt_hdl);
	conc_scan_config_bits = target_if_get_conc_scan_config_bits(tgt_hdl);

	wma_debug("Defaults: scan config:%x FW mode config:%x",
		 conc_scan_config_bits, fw_config_bits);

	ret = qdf_mc_timer_stop(&wma_handle->service_ready_ext_timer);
	if (!QDF_IS_STATUS_SUCCESS(ret)) {
		wma_err("Failed to stop the service ready ext timer");
		return -EINVAL;
	}
	wma_populate_soc_caps(wma_handle, tgt_hdl, param_buf);

	ret = wma_update_hw_mode_list(wma_handle, tgt_hdl);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Failed to update hw mode list");
		return -EINVAL;
	}

	wma_debug("WMA --> WMI_INIT_CMDID");

	if (wma_is_dbs_mandatory(wma_handle->psoc, tgt_hdl) &&
	   (policy_mgr_is_dual_mac_disabled_in_ini(wma_handle->psoc))) {
		policy_mgr_set_dual_mac_feature(wma_handle->psoc,
				ENABLE_DBS_CXN_AND_DISABLE_SIMULTANEOUS_SCAN);
		policy_mgr_set_ch_select_plcy(wma_handle->psoc,
					      POLICY_MGR_CH_SELECT_POLICY_DEF);
	}
	wma_init_scan_fw_mode_config(wma_handle->psoc, conc_scan_config_bits,
				     fw_config_bits);

	policy_mgr_get_sta_sap_scc_on_dfs_chnl(wma_handle->psoc,
					       &sta_sap_scc_on_dfs_chnl);

	/*
	 * For non-dbs HW, disallow sta+sap on DFS channel as if SAP comes
	 * on DFS master mode enable (sta_sap_scc_on_dfs_chnl = 2), scan will
	 * be disabled and STA cannot connect to any other channel
	 */
	if (!policy_mgr_is_hw_dbs_capable(wma_handle->psoc) &&
	    sta_sap_scc_on_dfs_chnl == 2)
		policy_mgr_set_sta_sap_scc_on_dfs_chnl(wma_handle->psoc, 1);

	target_psoc_set_num_radios(tgt_hdl, 1);

	if (wmi_service_enabled(wmi_handle,
				wmi_service_new_htt_msg_format)) {
		cdp_cfg_set_new_htt_msg_format(soc, 1);
		wlan_res_cfg->new_htt_msg_format = true;
	} else {
		cdp_cfg_set_new_htt_msg_format(soc, 0);
		wlan_res_cfg->new_htt_msg_format = false;
	}

	if (QDF_GLOBAL_FTM_MODE  != cds_get_conparam() &&
	    ucfg_mlme_get_peer_unmap_conf(wma_handle->psoc) &&
	    wmi_service_enabled(wmi_handle,
				wmi_service_peer_unmap_cnf_support)) {
		wlan_res_cfg->peer_unmap_conf_support = true;
		cdp_cfg_set_peer_unmap_conf_support(soc, true);
	} else {
		wlan_res_cfg->peer_unmap_conf_support = false;
		cdp_cfg_set_peer_unmap_conf_support(soc, false);
	}

	if (wma_handle->enable_tx_compl_tsf64 &&
	    wmi_service_enabled(wmi_handle,
				wmi_service_tx_compl_tsf64)) {
		wlan_res_cfg->tstamp64_en = true;
		cdp_cfg_set_tx_compl_tsf64(soc, true);
	} else {
		wlan_res_cfg->tstamp64_en = false;
		cdp_cfg_set_tx_compl_tsf64(soc, false);
	}

	if (ucfg_is_ftm_time_sync_enable(wma_handle->psoc) &&
	    wmi_service_enabled(wmi_handle, wmi_service_time_sync_ftm)) {
		wlan_res_cfg->time_sync_ftm = true;
		ucfg_ftm_time_sync_set_enable(wma_handle->psoc, true);
	} else {
		wlan_res_cfg->time_sync_ftm = false;
		ucfg_ftm_time_sync_set_enable(wma_handle->psoc, false);
	}

	if (wmi_service_enabled(wma_handle->wmi_handle, wmi_service_nan_vdev))
		ucfg_nan_set_vdev_creation_supp_by_fw(wma_handle->psoc, true);

	/* Change default hw mode as below kind of target will only be
	 * sending single HW mode
	 */
	if (!wmi_service_enabled(wmi_handle,
				 wmi_service_dual_band_simultaneous_support))
		wma_handle->new_hw_mode_index =
				tgt_cap_info->default_dbs_hw_mode_index;

	/*
	 * Firmware can accommodate maximum 4 vdevs and the ini gNumVdevs
	 * indicates the same.
	 * If host driver is going to create vdev for NAN, it indicates
	 * the total no.of vdevs supported to firmware which includes the
	 * NAN vdev.
	 * If firmware is going to create NAN discovery vdev, host should
	 * indicate 3 vdevs and firmware shall add 1 vdev for NAN. So decrement
	 * the num_vdevs by 1.
	 */
	if (ucfg_nan_is_vdev_creation_allowed(wma_handle->psoc)) {
		wlan_res_cfg->nan_separate_iface_support = true;
	} else {
		wlan_res_cfg->num_vdevs--;
		wma_update_num_peers_tids(wma_handle, wlan_res_cfg);
	}

	if ((ucfg_pkt_capture_get_mode(wma_handle->psoc) !=
						PACKET_CAPTURE_MODE_DISABLE) &&
	    wmi_service_enabled(wmi_handle,
				wmi_service_packet_capture_support))
		wlan_res_cfg->pktcapture_support = true;
	else
		wlan_res_cfg->pktcapture_support = false;

	if (wmi_service_enabled(wmi_handle,
				wmi_service_sae_eapol_offload_support))
		wlan_res_cfg->sae_eapol_offload = true;
	else
		wlan_res_cfg->sae_eapol_offload = false;

	wma_debug("num_vdevs: %u", wlan_res_cfg->num_vdevs);

	wma_init_dbr_params(wma_handle);

	wma_set_coex_res_cfg(wma_handle, wmi_handle, wlan_res_cfg);

	return 0;
}

/**
 * wma_rx_ready_event() - event handler to process
 *                        wmi rx ready event.
 * @handle: wma handle
 * @cmd_param_info: command params info
 * @length: param length
 *
 * Return: none
 */
int wma_rx_ready_event(void *handle, uint8_t *cmd_param_info,
					uint32_t length)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_READY_EVENTID_param_tlvs *param_buf = NULL;
	wmi_ready_event_fixed_param *ev = NULL;
	int ret;

	wma_debug("Enter");

	param_buf = (WMI_READY_EVENTID_param_tlvs *) cmd_param_info;
	if (!(wma_handle && param_buf)) {
		wma_err("Invalid arguments");
		QDF_ASSERT(0);
		return -EINVAL;
	}

	wma_debug("WMA <-- WMI_READY_EVENTID");

	ev = param_buf->fixed_param;
	/* Indicate to the waiting thread that the ready
	 * event was received
	 */
	wma_handle->sub_20_support =
		wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_half_rate_quarter_rate_support);
	wma_handle->wmi_ready = true;
	wma_handle->wlan_init_status = ev->status;

	if (wma_handle->is_dfs_offloaded)
		wmi_unified_dfs_phyerr_offload_en_cmd(
				wma_handle->wmi_handle, 0);
	/* copy the mac addr */
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&ev->mac_addr, wma_handle->myaddr);
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&ev->mac_addr, wma_handle->hwaddr);
	ret = wma_update_hdd_cfg(wma_handle);
	if (ret)
		return ret;

	wma_debug("Exit");

	return 0;
}

/**
 * wma_wait_for_ready_event() - wait for wma ready event
 * @handle: wma handle
 *
 * Return: 0 for success or QDF error
 */
QDF_STATUS wma_wait_for_ready_event(WMA_HANDLE handle)
{
	tp_wma_handle wma_handle = (tp_wma_handle)handle;
	QDF_STATUS status;
	struct target_psoc_info *tgt_hdl;

	tgt_hdl = wlan_psoc_get_tgt_if_handle(wma_handle->psoc);
	if (!tgt_hdl) {
		wma_err("target psoc info is NULL");
		return QDF_STATUS_E_INVAL;
	}

	status = qdf_wait_for_event_completion(&tgt_hdl->info.event,
					       WMA_READY_EVENTID_TIMEOUT);
	if (!tgt_hdl->info.wmi_ready) {
		wma_err("Error in pdev creation");
		if (!cds_is_driver_recovering() || !cds_is_fw_down())
			QDF_DEBUG_PANIC("FW ready event timed out");
		return QDF_STATUS_E_INVAL;
	}

	if (status == QDF_STATUS_E_TIMEOUT)
		wma_err("Timeout waiting for FW ready event");
	else if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to wait for FW ready event; status:%u", status);
	else
		wma_info("FW ready event received");

	return status;
}

/**
 * wma_set_ppsconfig() - set pps config in fw
 * @vdev_id: vdev id
 * @pps_param: pps params
 * @val : param value
 *
 * Return: 0 for success or QDF error
 */
QDF_STATUS wma_set_ppsconfig(uint8_t vdev_id, uint16_t pps_param,
				    int val)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	int ret = -EIO;
	uint32_t pps_val;

	if (!wma) {
		wma_err("Failed to get wma");
		return QDF_STATUS_E_INVAL;
	}

	switch (pps_param) {
	case WMA_VHT_PPS_PAID_MATCH:
		pps_val = ((val << 31) & 0xffff0000) |
			  (PKT_PWR_SAVE_PAID_MATCH & 0xffff);
		goto pkt_pwr_save_config;
	case WMA_VHT_PPS_GID_MATCH:
		pps_val = ((val << 31) & 0xffff0000) |
			  (PKT_PWR_SAVE_GID_MATCH & 0xffff);
		goto pkt_pwr_save_config;
	case WMA_VHT_PPS_DELIM_CRC_FAIL:
		pps_val = ((val << 31) & 0xffff0000) |
			  (PKT_PWR_SAVE_DELIM_CRC_FAIL & 0xffff);
		goto pkt_pwr_save_config;

		/* Enable the code below as and when the functionality
		 * is supported/added in host.
		 */
#ifdef NOT_YET
	case WMA_VHT_PPS_EARLY_TIM_CLEAR:
		pps_val = ((val << 31) & 0xffff0000) |
			  (PKT_PWR_SAVE_EARLY_TIM_CLEAR & 0xffff);
		goto pkt_pwr_save_config;
	case WMA_VHT_PPS_EARLY_DTIM_CLEAR:
		pps_val = ((val << 31) & 0xffff0000) |
			  (PKT_PWR_SAVE_EARLY_DTIM_CLEAR & 0xffff);
		goto pkt_pwr_save_config;
	case WMA_VHT_PPS_EOF_PAD_DELIM:
		pps_val = ((val << 31) & 0xffff0000) |
			  (PKT_PWR_SAVE_EOF_PAD_DELIM & 0xffff);
		goto pkt_pwr_save_config;
	case WMA_VHT_PPS_MACADDR_MISMATCH:
		pps_val = ((val << 31) & 0xffff0000) |
			  (PKT_PWR_SAVE_MACADDR_MISMATCH & 0xffff);
		goto pkt_pwr_save_config;
	case WMA_VHT_PPS_GID_NSTS_ZERO:
		pps_val = ((val << 31) & 0xffff0000) |
			  (PKT_PWR_SAVE_GID_NSTS_ZERO & 0xffff);
		goto pkt_pwr_save_config;
	case WMA_VHT_PPS_RSSI_CHECK:
		pps_val = ((val << 31) & 0xffff0000) |
			  (PKT_PWR_SAVE_RSSI_CHECK & 0xffff);
		goto pkt_pwr_save_config;
#endif /* NOT_YET */
pkt_pwr_save_config:
		wma_debug("vdev_id:%d val:0x%x pps_val:0x%x", vdev_id,
			 val, pps_val);
		ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_PACKET_POWERSAVE,
					      pps_val);
		break;
	default:
		wma_err("INVALID PPS CONFIG");
	}

	return (ret) ? QDF_STATUS_E_FAILURE : QDF_STATUS_SUCCESS;
}

/**
 * wma_process_set_mas() - Function to enable/disable MAS
 * @wma:	Pointer to WMA handle
 * @mas_val:	1-Enable MAS, 0-Disable MAS
 *
 * This function enables/disables the MAS value
 *
 * Return: QDF_SUCCESS for success otherwise failure
 */
static QDF_STATUS wma_process_set_mas(tp_wma_handle wma,
				      uint32_t *mas_val)
{
	uint32_t val;

	if (!wma || !mas_val) {
		wma_err("Invalid input to enable/disable MAS");
		return QDF_STATUS_E_FAILURE;
	}

	val = (*mas_val);

	if (QDF_STATUS_SUCCESS !=
			wma_set_enable_disable_mcc_adaptive_scheduler(val)) {
		wma_err("Unable to enable/disable MAS");
		return QDF_STATUS_E_FAILURE;
	}
	wma_debug("Value is %d", val);
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_process_set_miracast() - Function to set miracast value in WMA
 * @wma:		Pointer to WMA handle
 * @miracast_val:	0-Disabled,1-Source,2-Sink
 *
 * This function stores the miracast value in WMA
 *
 * Return: QDF_SUCCESS for success otherwise failure
 *
 */
static QDF_STATUS wma_process_set_miracast(tp_wma_handle wma,
					   uint32_t *miracast_val)
{
	if (!wma || !miracast_val) {
		wma_err("Invalid input to store miracast value");
		return QDF_STATUS_E_FAILURE;
	}

	wma->miracast_value = *miracast_val;
	wma_debug("Miracast value is %d", wma->miracast_value);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_config_stats_factor() - Function to configure stats avg. factor
 * @wma:  pointer to WMA handle
 * @avg_factor:	stats. avg. factor passed down by userspace
 *
 * This function configures the avg. stats value in firmware
 *
 * Return: QDF_STATUS_SUCCESS for success otherwise failure
 *
 */
static QDF_STATUS wma_config_stats_factor(tp_wma_handle wma,
				      struct sir_stats_avg_factor *avg_factor)
{
	QDF_STATUS ret;

	if (!wma || !avg_factor) {
		wma_err("Invalid input of stats avg factor");
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma->wmi_handle,
					    avg_factor->vdev_id,
					    WMI_VDEV_PARAM_STATS_AVG_FACTOR,
					    avg_factor->stats_avg_factor);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("failed to set avg_factor for vdev_id %d",
			 avg_factor->vdev_id);
	}

	wma_debug("Set stats_avg_factor %d for vdev_id %d",
		 avg_factor->stats_avg_factor, avg_factor->vdev_id);

	return ret;
}

/**
 * wma_config_guard_time() - Function to set guard time in firmware
 * @wma:  pointer to WMA handle
 * @guard_time:  guard time passed down by userspace
 *
 * This function configures the guard time in firmware
 *
 * Return: QDF_STATUS_SUCCESS for success otherwise failure
 *
 */
static QDF_STATUS wma_config_guard_time(tp_wma_handle wma,
				   struct sir_guard_time_request *guard_time)
{
	QDF_STATUS ret;

	if (!wma || !guard_time) {
		wma_err("Invalid input of guard time");
		return QDF_STATUS_E_FAILURE;
	}

	ret = wma_vdev_set_param(wma->wmi_handle,
					      guard_time->vdev_id,
					      WMI_VDEV_PARAM_RX_LEAK_WINDOW,
					      guard_time->guard_time);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("failed to set guard time for vdev_id %d",
			 guard_time->vdev_id);
	}

	wma_debug("Set guard time %d for vdev_id %d",
		 guard_time->guard_time, guard_time->vdev_id);

	return ret;
}

/**
 * wma_enable_specific_fw_logs() - Start/Stop logging of diag event/log id
 * @wma_handle: WMA handle
 * @start_log: Start logging related parameters
 *
 * Send the command to the FW based on which specific logging of diag
 * event/log id can be started/stopped
 *
 * Return: None
 */
static void wma_enable_specific_fw_logs(tp_wma_handle wma_handle,
					struct sir_wifi_start_log *start_log)
{

	if (!start_log) {
		wma_err("start_log pointer is NULL");
		return;
	}
	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return;
	}

	if (!((start_log->ring_id == RING_ID_CONNECTIVITY) ||
			(start_log->ring_id == RING_ID_FIRMWARE_DEBUG))) {
		wma_debug("Not connectivity or fw debug ring: %d",
				start_log->ring_id);
		return;
	}

	wmi_unified_enable_specific_fw_logs_cmd(wma_handle->wmi_handle,
				(struct wmi_wifi_start_log *)start_log);
}

#define MEGABYTE	(1024 * 1024)
/**
 * wma_set_wifi_start_packet_stats() - Start/stop packet stats
 * @wma_handle: WMA handle
 * @start_log: Struture containing the start wifi logger params
 *
 * This function is used to send the WMA commands to start/stop logging
 * of per packet statistics
 *
 * Return: None
 *
 */
#if !defined(FEATURE_PKTLOG) || defined(REMOVE_PKT_LOG)
static void wma_set_wifi_start_packet_stats(void *wma_handle,
					struct sir_wifi_start_log *start_log)
{
}

#else
static void wma_set_wifi_start_packet_stats(void *wma_handle,
					struct sir_wifi_start_log *start_log)
{
	struct hif_opaque_softc *scn;
	uint32_t log_state;

	if (!start_log) {
		wma_err("start_log pointer is NULL");
		return;
	}
	if (!wma_handle) {
		wma_err("Invalid wma handle");
		return;
	}

	/* No need to register for ring IDs other than packet stats */
	if (start_log->ring_id != RING_ID_PER_PACKET_STATS) {
		wma_debug("Ring id is not for per packet stats: %d",
			 start_log->ring_id);
		return;
	}

	scn = cds_get_context(QDF_MODULE_ID_HIF);
	if (!scn) {
		wma_err("Invalid HIF handle");
		return;
	}

#ifdef HELIUMPLUS
	log_state = ATH_PKTLOG_ANI | ATH_PKTLOG_RCUPDATE | ATH_PKTLOG_RCFIND |
		ATH_PKTLOG_RX | ATH_PKTLOG_TX |
		ATH_PKTLOG_TEXT | ATH_PKTLOG_SW_EVENT;
#elif defined(QCA_WIFI_QCA6390) || defined(QCA_WIFI_QCA6490) || \
      defined(QCA_WIFI_QCA6750)
	log_state = ATH_PKTLOG_RCFIND | ATH_PKTLOG_RCUPDATE |
		    ATH_PKTLOG_TX | ATH_PKTLOG_LITE_T2H |
		    ATH_PKTLOG_SW_EVENT | ATH_PKTLOG_RX;
#elif defined(QCA_WIFI_QCA6290)
	log_state = ATH_PKTLOG_LITE_RX | ATH_PKTLOG_LITE_T2H;
#else
	wma_debug("Packet log Not supported");
	log_state = 0;
#endif
	if (start_log->size != 0) {
		pktlog_setsize(scn, start_log->size * MEGABYTE);
		return;
	} else if (start_log->is_pktlog_buff_clear == true) {
		pktlog_clearbuff(scn, start_log->is_pktlog_buff_clear);
		return;
	}

	if (start_log->verbose_level == WLAN_LOG_LEVEL_ACTIVE) {
		pktlog_enable(scn, log_state, start_log->ini_triggered,
			      start_log->user_triggered,
			      start_log->is_iwpriv_command);
		wma_debug("Enabling per packet stats");
	} else {
		pktlog_enable(scn, 0, start_log->ini_triggered,
				start_log->user_triggered,
				start_log->is_iwpriv_command);
		wma_debug("Disabling per packet stats");
	}
}
#endif

/**
 * wma_send_flush_logs_to_fw() - Send log flush command to FW
 * @wma_handle: WMI handle
 *
 * This function is used to send the flush command to the FW,
 * that will flush the fw logs that are residue in the FW
 *
 * Return: None
 */
void wma_send_flush_logs_to_fw(tp_wma_handle wma_handle)
{
	QDF_STATUS status;

	status = wmi_unified_flush_logs_to_fw_cmd(wma_handle->wmi_handle);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	status = qdf_mc_timer_start(&wma_handle->log_completion_timer,
				    WMA_LOG_COMPLETION_TIMER);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to start the log completion timer");
}

/**
 * wma_update_tx_fail_cnt_th() - Set threshold for TX pkt fail
 * @wma_handle: WMA handle
 * @tx_fail_cnt_th: sme_tx_fail_cnt_threshold parameter
 *
 * This function is used to set Tx pkt fail count threshold,
 * FW will do disconnect with station once this threshold is reached.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wma_update_tx_fail_cnt_th(tp_wma_handle wma,
			struct sme_tx_fail_cnt_threshold *tx_fail_cnt_th)
{
	u_int8_t vdev_id;
	u_int32_t tx_fail_disconn_th;
	int ret = -EIO;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue Tx pkt fail count threshold");
		return QDF_STATUS_E_INVAL;
	}
	vdev_id = tx_fail_cnt_th->session_id;
	tx_fail_disconn_th = tx_fail_cnt_th->tx_fail_cnt_threshold;
	wma_debug("Set TX pkt fail count threshold  vdevId %d count %d",
			vdev_id, tx_fail_disconn_th);


	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_DISCONNECT_TH,
			tx_fail_disconn_th);

	if (ret) {
		wma_err("Failed to send TX pkt fail count threshold command");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_update_short_retry_limit() - Set retry limit for short frames
 * @wma_handle: WMA handle
 * @short_retry_limit_th: retry limir count for Short frames.
 *
 * This function is used to configure the transmission retry limit at which
 * short frames needs to be retry.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wma_update_short_retry_limit(tp_wma_handle wma,
		struct sme_short_retry_limit *short_retry_limit_th)
{
	uint8_t vdev_id;
	uint32_t short_retry_limit;
	int ret;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue short retry limit threshold");
		return QDF_STATUS_E_INVAL;
	}
	vdev_id = short_retry_limit_th->session_id;
	short_retry_limit = short_retry_limit_th->short_retry_limit;
	wma_debug("Set short retry limit threshold  vdevId %d count %d",
		vdev_id, short_retry_limit);

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
		WMI_VDEV_PARAM_NON_AGG_SW_RETRY_TH,
		short_retry_limit);

	if (ret) {
		wma_err("Failed to send short limit threshold command");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_update_long_retry_limit() - Set retry limit for long frames
 * @wma_handle: WMA handle
 * @long_retry_limit_th: retry limir count for long frames
 *
 * This function is used to configure the transmission retry limit at which
 * long frames needs to be retry
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wma_update_long_retry_limit(tp_wma_handle wma,
		struct sme_long_retry_limit  *long_retry_limit_th)
{
	uint8_t vdev_id;
	uint32_t long_retry_limit;
	int ret;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue long retry limit threshold");
		return QDF_STATUS_E_INVAL;
	}
	vdev_id = long_retry_limit_th->session_id;
	long_retry_limit = long_retry_limit_th->long_retry_limit;
	wma_debug("Set TX pkt fail count threshold  vdevId %d count %d",
		vdev_id, long_retry_limit);

	ret  = wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_AGG_SW_RETRY_TH,
			long_retry_limit);

	if (ret) {
		wma_err("Failed to send long limit threshold command");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/*
 * wma_update_sta_inactivity_timeout() - Set sta_inactivity_timeout to fw
 * @wma_handle: WMA handle
 * @sta_inactivity_timer: sme_sta_inactivity_timeout
 *
 * This function is used to set sta_inactivity_timeout.
 * If a station does not send anything in sta_inactivity_timeout seconds, an
 * empty data frame is sent to it in order to verify whether it is
 * still in range. If this frame is not ACKed, the station will be
 * disassociated and then deauthenticated.
 *
 * Return: None
 */
void wma_update_sta_inactivity_timeout(tp_wma_handle wma,
		struct sme_sta_inactivity_timeout  *sta_inactivity_timer)
{
	uint8_t vdev_id;
	uint32_t max_unresponsive_time;
	uint32_t min_inactive_time, max_inactive_time;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue sta_inactivity_timeout");
		return;
	}
	vdev_id = sta_inactivity_timer->session_id;
	max_unresponsive_time = sta_inactivity_timer->sta_inactivity_timeout;
	max_inactive_time = max_unresponsive_time * TWO_THIRD;
	min_inactive_time = max_unresponsive_time - max_inactive_time;

	if (wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_AP_KEEPALIVE_MIN_IDLE_INACTIVE_TIME_SECS,
			min_inactive_time))
		wma_err("Failed to Set AP MIN IDLE INACTIVE TIME");

	if (wma_vdev_set_param(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_AP_KEEPALIVE_MAX_IDLE_INACTIVE_TIME_SECS,
			max_inactive_time))
		wma_err("Failed to Set AP MAX IDLE INACTIVE TIME");

	if (wma_vdev_set_param(wma->wmi_handle, vdev_id,
		WMI_VDEV_PARAM_AP_KEEPALIVE_MAX_UNRESPONSIVE_TIME_SECS,
		max_unresponsive_time))
		wma_err("Failed to Set MAX UNRESPONSIVE TIME");

	wma_debug("vdev_id:%d min_inactive_time: %u max_inactive_time: %u max_unresponsive_time: %u",
			vdev_id,
			min_inactive_time, max_inactive_time,
			max_unresponsive_time);
}

#ifdef WLAN_FEATURE_WOW_PULSE


#define WMI_WOW_HOSTWAKEUP_GPIO_CMD_FIXED_PARAM \
WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMD_fixed_param


#define WMITLV_TAG_HOSTWAKEUP_GPIO_CMD_FIXED_PARAM \
WMITLV_TAG_STRUC_wmi_wow_hostwakeup_gpio_pin_pattern_config_cmd_fixed_param

/**
 * wma_send_wow_pulse_cmd() - send wmi cmd of wow pulse cmd
 * information to fw.
 * @wma_handle: wma handler
 * @udp_response: wow_pulse_mode pointer
 *
 * Return: Return QDF_STATUS
 */
static QDF_STATUS wma_send_wow_pulse_cmd(tp_wma_handle wma_handle,
					struct wow_pulse_mode *wow_pulse_cmd)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	wmi_buf_t buf;
	WMI_WOW_HOSTWAKEUP_GPIO_CMD_FIXED_PARAM *cmd;
	u_int16_t len;

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (WMI_WOW_HOSTWAKEUP_GPIO_CMD_FIXED_PARAM *)wmi_buf_data(buf);
	qdf_mem_zero(cmd, len);

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_HOSTWAKEUP_GPIO_CMD_FIXED_PARAM,
		WMITLV_GET_STRUCT_TLVLEN(
			WMI_WOW_HOSTWAKEUP_GPIO_CMD_FIXED_PARAM));

	cmd->enable = wow_pulse_cmd->wow_pulse_enable;
	cmd->pin = wow_pulse_cmd->wow_pulse_pin;
	cmd->interval_low = wow_pulse_cmd->wow_pulse_interval_low;
	cmd->interval_high = wow_pulse_cmd->wow_pulse_interval_high;
	cmd->repeat_cnt = wow_pulse_cmd->wow_pulse_repeat_count;
	cmd->init_state = wow_pulse_cmd->wow_pulse_init_state;

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
		WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMDID)) {
		wmi_buf_free(buf);
		status = QDF_STATUS_E_FAILURE;
	}

	wma_debug("Exit");
	return status;
}

#undef WMI_WOW_HOSTWAKEUP_GPIO_CMD_FIXED_PARAM
#undef WMITLV_TAG_HOSTWAKEUP_GPIO_CMD_FIXED_PARAM
#undef WMI_WOW_PULSE_REPEAT_CNT

#else
static inline QDF_STATUS wma_send_wow_pulse_cmd(tp_wma_handle wma_handle,
					struct wow_pulse_mode *wow_pulse_cmd)
{
	return QDF_STATUS_E_FAILURE;
}
#endif


/**
 * wma_process_power_debug_stats_req() - Process the Chip Power stats collect
 * request and pass the Power stats request to Fw
 * @wma_handle: WMA handle
 *
 * Return: QDF_STATUS
 */
#ifdef WLAN_POWER_DEBUG
static QDF_STATUS wma_process_power_debug_stats_req(tp_wma_handle wma_handle)
{
	wmi_pdev_get_chip_power_stats_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	int ret;

	if (!wma_handle) {
		wma_err("input pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_pdev_get_chip_power_stats_cmd_fixed_param *) buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_get_chip_power_stats_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_pdev_get_chip_power_stats_cmd_fixed_param));
	cmd->pdev_id = 0;

	wma_debug("POWER_DEBUG_STATS - Get Request Params; Pdev id - %d",
			cmd->pdev_id);
	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
			WMI_PDEV_GET_CHIP_POWER_STATS_CMDID);
	if (ret) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS wma_process_power_debug_stats_req(tp_wma_handle wma_handle)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#ifdef WLAN_FEATURE_BEACON_RECEPTION_STATS
static QDF_STATUS wma_process_beacon_debug_stats_req(tp_wma_handle wma_handle,
						     uint32_t *vdev_id)
{
	wmi_vdev_get_bcn_recv_stats_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	int ret;

	wma_debug("Enter");
	if (!wma_handle) {
		wma_err("input pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (u_int8_t *)wmi_buf_data(buf);
	cmd = (wmi_vdev_get_bcn_recv_stats_cmd_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_get_bcn_recv_stats_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_vdev_get_bcn_recv_stats_cmd_fixed_param));
	cmd->vdev_id = *vdev_id;

	wma_debug("BEACON_DEBUG_STATS - Get Request Params; vdev id - %d",
		 cmd->vdev_id);
	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				   WMI_VDEV_GET_BCN_RECEPTION_STATS_CMDID);
	if (ret) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("Exit");
	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS wma_process_beacon_debug_stats_req(tp_wma_handle wma_handle,
						     uint32_t *vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wma_set_arp_req_stats() - process set arp stats request command to fw
 * @wma_handle: WMA handle
 * @req_buf: set srp stats request buffer
 *
 * Return: None
 */
static void wma_set_arp_req_stats(WMA_HANDLE handle,
				  struct set_arp_stats_params *req_buf)
{
	QDF_STATUS status;
	struct set_arp_stats *arp_stats;
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct wlan_objmgr_vdev *vdev;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, cannot send per roam config");
		return;
	}

	if (!wma_is_vdev_valid(req_buf->vdev_id)) {
		wma_err("vdev id:%d is not active", req_buf->vdev_id);
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(wma_handle->psoc,
						    req_buf->vdev_id,
						    WLAN_LEGACY_WMA_ID);
	if (!vdev) {
		wma_err("Can't get vdev by vdev_id:%d", req_buf->vdev_id);
		return;
	}

	if (!wma_is_vdev_up(req_buf->vdev_id)) {
		wma_debug("vdev id:%d is not started", req_buf->vdev_id);
		goto release_ref;
	}

	arp_stats = (struct set_arp_stats *)req_buf;
	status = wmi_unified_set_arp_stats_req(wma_handle->wmi_handle,
					       arp_stats);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("failed to set arp stats to FW");

release_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_WMA_ID);
}

/**
 * wma_get_arp_req_stats() - process get arp stats request command to fw
 * @wma_handle: WMA handle
 * @req_buf: get srp stats request buffer
 *
 * Return: None
 */
static void wma_get_arp_req_stats(WMA_HANDLE handle,
				  struct get_arp_stats_params *req_buf)
{
	QDF_STATUS status;
	struct get_arp_stats *arp_stats;
	tp_wma_handle wma_handle = (tp_wma_handle) handle;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, cannot send per roam config");
		return;
	}
	if (!wma_is_vdev_valid(req_buf->vdev_id)) {
		wma_err("vdev id:%d is not active", req_buf->vdev_id);
		return;
	}

	arp_stats = (struct get_arp_stats *)req_buf;
	status = wmi_unified_get_arp_stats_req(wma_handle->wmi_handle,
					       arp_stats);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("failed to send get arp stats to FW");
}

/**
 * wma_set_del_pmkid_cache() - API to set/delete PMKID cache entry in fw
 * @handle: WMA handle
 * @pmk_cache: PMK cache entry
 *
 * Return: None
 */
static void wma_set_del_pmkid_cache(WMA_HANDLE handle,
				    struct wmi_unified_pmk_cache *pmk_cache)
{
	QDF_STATUS status;
	tp_wma_handle wma_handle = (tp_wma_handle) handle;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, cannot send set del pmkid");
		return;
	}

	status = wmi_unified_set_del_pmkid_cache(wma_handle->wmi_handle,
						 pmk_cache);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("failed to send set/del pmkid cmd to fw");
}

/**
 * wma_send_invoke_neighbor_report() - API to send invoke neighbor report
 * command to fw
 *
 * @handle: WMA handle
 * @params: Pointer to invoke neighbor report params
 *
 * Return: None
 */
static
void wma_send_invoke_neighbor_report(WMA_HANDLE handle,
			struct wmi_invoke_neighbor_report_params *params)
{
	QDF_STATUS status;
	tp_wma_handle wma_handle = (tp_wma_handle) handle;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, cannot send invoke neighbor report");
		return;
	}

	status = wmi_unified_invoke_neighbor_report_cmd(wma_handle->wmi_handle,
							params);

	if (status != QDF_STATUS_SUCCESS)
		wma_err("failed to send invoke neighbor report command");
}

QDF_STATUS wma_set_rx_reorder_timeout_val(tp_wma_handle wma_handle,
	struct sir_set_rx_reorder_timeout_val *reorder_timeout)
{
	wmi_pdev_set_reorder_timeout_val_cmd_fixed_param *cmd;
	uint32_t len;
	wmi_buf_t buf;
	int ret;

	if (!reorder_timeout) {
		wma_err("invalid pointer");
		return QDF_STATUS_E_INVAL;
	}

	if (!wma_handle) {
		wma_err("WMA context is invalid!");
		return QDF_STATUS_E_INVAL;
	}
	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_pdev_set_reorder_timeout_val_cmd_fixed_param *)
		wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_pdev_set_reorder_timeout_val_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN(wmi_pdev_set_reorder_timeout_val_cmd_fixed_param));

	memcpy(cmd->rx_timeout_pri, reorder_timeout->rx_timeout_pri,
		sizeof(reorder_timeout->rx_timeout_pri));

	wma_debug("rx aggr record timeout: VO: %d, VI: %d, BE: %d, BK: %d",
		cmd->rx_timeout_pri[0], cmd->rx_timeout_pri[1],
		cmd->rx_timeout_pri[2], cmd->rx_timeout_pri[3]);

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
			WMI_PDEV_SET_REORDER_TIMEOUT_VAL_CMDID);
	if (ret) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_set_rx_blocksize(tp_wma_handle wma_handle,
	struct sir_peer_set_rx_blocksize *peer_rx_blocksize)
{
	wmi_peer_set_rx_blocksize_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int ret;

	if (!peer_rx_blocksize) {
		wma_err("invalid pointer");
		return QDF_STATUS_E_INVAL;
	}

	if (!wma_handle) {
		wma_err("WMA context is invalid!");
		return QDF_STATUS_E_INVAL;
	}

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_peer_set_rx_blocksize_cmd_fixed_param *) buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_peer_set_rx_blocksize_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN(wmi_peer_set_rx_blocksize_cmd_fixed_param));

	cmd->vdev_id = peer_rx_blocksize->vdev_id;
	cmd->rx_block_ack_win_limit =
		peer_rx_blocksize->rx_block_ack_win_limit;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_rx_blocksize->peer_macaddr.bytes,
		&cmd->peer_macaddr);

	wma_debug("rx aggr blocksize: %d", cmd->rx_block_ack_win_limit);

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
			WMI_PEER_SET_RX_BLOCKSIZE_CMDID);
	if (ret) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_get_chain_rssi(tp_wma_handle wma_handle,
		struct get_chain_rssi_req_params *req_params)
{
	wmi_pdev_div_get_rssi_antid_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t len = sizeof(wmi_pdev_div_get_rssi_antid_fixed_param);
	u_int8_t *buf_ptr;

	if (!wma_handle) {
		wma_err("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_INVAL;
	}

	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (u_int8_t *)wmi_buf_data(wmi_buf);

	cmd = (wmi_pdev_div_get_rssi_antid_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_pdev_div_get_rssi_antid_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
		wmi_pdev_div_get_rssi_antid_fixed_param));
	cmd->pdev_id = 0;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(req_params->peer_macaddr.bytes,
				&cmd->macaddr);

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
				 WMI_PDEV_DIV_GET_RSSI_ANTID_CMDID)) {
		wmi_buf_free(wmi_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_FEATURE_FILS_SK)
/**
 * wma_roam_scan_send_hlp() - API to send HLP IE info to fw
 * @wma_handle: WMA handle
 * @req: HLP params
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wma_roam_scan_send_hlp(tp_wma_handle wma_handle,
					 struct hlp_params *req)
{
	struct hlp_params *params;
	QDF_STATUS status;

	params = qdf_mem_malloc(sizeof(*params));
	if (!params)
		return QDF_STATUS_E_NOMEM;

	params->vdev_id = req->vdev_id;
	params->hlp_ie_len = req->hlp_ie_len;
	qdf_mem_copy(params->hlp_ie, req->hlp_ie, req->hlp_ie_len);
	status = wmi_unified_roam_send_hlp_cmd(wma_handle->wmi_handle, params);

	wma_debug("Send HLP status %d vdev id %d", status, params->vdev_id);
	qdf_trace_hex_dump(QDF_MODULE_ID_WMI, QDF_TRACE_LEVEL_DEBUG,
				params->hlp_ie, 10);

	qdf_mem_free(params);
	return status;
}
#else
static QDF_STATUS wma_roam_scan_send_hlp(tp_wma_handle wma_handle,
					 struct hlp_params *req)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wma_process_set_limit_off_chan() - set limit off chanel parameters
 * @wma_handle: pointer to wma handle
 * @param: pointer to sir_limit_off_chan
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
static QDF_STATUS wma_process_limit_off_chan(tp_wma_handle wma_handle,
	struct sir_limit_off_chan *param)
{
	int32_t err;
	struct wmi_limit_off_chan_param limit_off_chan_param;

	if (param->vdev_id >= wma_handle->max_bssid) {
		wma_err("Invalid vdev_id: %d", param->vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	if (!wma_is_vdev_up(param->vdev_id)) {
		wma_debug("vdev %d is not up skipping limit_off_chan_param",
			 param->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	limit_off_chan_param.vdev_id = param->vdev_id;
	limit_off_chan_param.status = param->is_tos_active;
	limit_off_chan_param.max_offchan_time = param->max_off_chan_time;
	limit_off_chan_param.rest_time = param->rest_time;
	limit_off_chan_param.skip_dfs_chans = param->skip_dfs_chans;

	err = wmi_unified_send_limit_off_chan_cmd(wma_handle->wmi_handle,
			&limit_off_chan_param);
	if (err) {
		wma_err("failed to set limit off chan cmd");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS wma_process_obss_color_collision_req(tp_wma_handle wma_handle,
		struct wmi_obss_color_collision_cfg_param *cfg)
{
	QDF_STATUS status;

	if (cfg->vdev_id >= wma_handle->max_bssid) {
		wma_err("Invalid vdev_id: %d", cfg->vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	if (!wma_is_vdev_up(cfg->vdev_id)) {
		wma_err("vdev %d is not up skipping obss color collision req",
			 cfg->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_send_obss_color_collision_cfg_cmd(wma_handle->
							       wmi_handle, cfg);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to send obss color collision cfg");

	return status;
}

/**
 * wma_send_obss_detection_cfg() - send obss detection cfg to firmware
 * @wma_handle: pointer to wma handle
 * @cfg: obss detection configuration
 *
 * Send obss detection configuration to firmware.
 *
 * Return: None
 */
static void wma_send_obss_detection_cfg(tp_wma_handle wma_handle,
					struct wmi_obss_detection_cfg_param
					*cfg)
{
	QDF_STATUS status;

	if (cfg->vdev_id >= wma_handle->max_bssid) {
		wma_err("Invalid vdev_id: %d", cfg->vdev_id);
		return;
	}
	if (!wma_is_vdev_up(cfg->vdev_id)) {
		wma_err("vdev %d is not up skipping obss detection req",
			 cfg->vdev_id);
		return;
	}

	status = wmi_unified_send_obss_detection_cfg_cmd(wma_handle->wmi_handle,
							 cfg);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to send obss detection cfg");

	return;
}

#ifdef WLAN_FEATURE_MOTION_DETECTION
/**
 * wma_motion_det_host_event_handler - motion detection event handler
 * @handle: WMA global handle
 * @event: motion detection event
 * @len: Length of cmd
 *
 * Call motion detection event callback handler
 *
 * Return: 0 on success, else error on failure
 */
int wma_motion_det_host_event_handler(void *handle, uint8_t *event,
				      uint32_t len)
{
	wmi_motion_det_event *motion_det_event_hdr;
	WMI_MOTION_DET_HOST_EVENTID_param_tlvs *param_buf =
			(WMI_MOTION_DET_HOST_EVENTID_param_tlvs *)event;
	struct sir_md_evt *md_event;
	struct mac_context *pmac = (struct mac_context *)cds_get_context(
				    QDF_MODULE_ID_PE);

	if (!param_buf) {
		wma_er("Invalid motion det host event buffer");
		return -EINVAL;
	}

	if (!pmac || !pmac->sme.md_host_evt_cb) {
		wma_err("Invalid motion detect callback");
		return -EINVAL;
	}

	motion_det_event_hdr = param_buf->fixed_param;
	wma_alert("motion detect host event received, vdev_id=%d, status=%d",
		 motion_det_event_hdr->vdev_id, motion_det_event_hdr->status);

	md_event = qdf_mem_malloc(sizeof(*md_event));
	if (!md_event)
		return -ENOMEM;

	md_event->vdev_id = motion_det_event_hdr->vdev_id;
	md_event->status = motion_det_event_hdr->status;

	pmac->sme.md_host_evt_cb(pmac->sme.md_ctx, md_event);

	qdf_mem_free(md_event);
	return 0;
}

/**
 * wma_motion_det_base_line_host_event_handler - md baselining event handler
 * @handle: WMA global handle
 * @event: motion detection baselining event
 * @len: Length of cmd
 *
 * Return: 0 on success, else error on failure
 */
int wma_motion_det_base_line_host_event_handler(void *handle,
						uint8_t *event, uint32_t len)
{
	wmi_motion_det_base_line_event *motion_det_base_line_event_hdr;
	WMI_MOTION_DET_BASE_LINE_HOST_EVENTID_param_tlvs *param_buf =
		(WMI_MOTION_DET_BASE_LINE_HOST_EVENTID_param_tlvs *)event;
	struct sir_md_bl_evt *md_bl_event;
	struct mac_context *pmac = (struct mac_context *)cds_get_context(
				    QDF_MODULE_ID_PE);

	if (!param_buf) {
		wma_err("Invalid motion detection base line event buffer");
		return -EINVAL;
	}

	if (!pmac || !pmac->sme.md_bl_evt_cb) {
		wma_err("Invalid motion detection base line callback");
		return -EINVAL;
	}

	motion_det_base_line_event_hdr = param_buf->fixed_param;
	wma_alert("motion detection base line event received, vdev_id=%d",
		 motion_det_base_line_event_hdr->vdev_id);
	wma_alert("baseline_value=%d bl_max_corr_resv=%d bl_min_corr_resv=%d",
		 motion_det_base_line_event_hdr->bl_baseline_value,
		 motion_det_base_line_event_hdr->bl_max_corr_reserved,
		 motion_det_base_line_event_hdr->bl_min_corr_reserved);

	md_bl_event = qdf_mem_malloc(sizeof(*md_bl_event));
	if (!md_bl_event)
		return -ENOMEM;

	md_bl_event->vdev_id = motion_det_base_line_event_hdr->vdev_id;
	md_bl_event->bl_baseline_value =
			motion_det_base_line_event_hdr->bl_baseline_value;
	md_bl_event->bl_max_corr_reserved =
			motion_det_base_line_event_hdr->bl_max_corr_reserved;
	md_bl_event->bl_min_corr_reserved =
			motion_det_base_line_event_hdr->bl_min_corr_reserved;

	pmac->sme.md_bl_evt_cb(pmac->sme.md_ctx, md_bl_event);

	qdf_mem_free(md_bl_event);
	return 0;
}

/**
 * wma_set_motion_det_config - Sends motion detection configuration wmi cmd
 * @wma_handle: WMA global handle
 * @motion_det_cfg: motion detection configuration
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_ERROR on error
 */
static QDF_STATUS wma_set_motion_det_config(
				tp_wma_handle wma_handle,
				struct sme_motion_det_cfg *motion_det_cfg)
{
	wmi_motion_det_config_params_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int err;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, sizeof(*cmd));
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_motion_det_config_params_cmd_fixed_param *)wmi_buf_data(buf);
	qdf_mem_zero(cmd, sizeof(*cmd));

	WMITLV_SET_HDR(
		&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_motion_det_config_params_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_motion_det_config_params_cmd_fixed_param));
	cmd->vdev_id = motion_det_cfg->vdev_id;
	cmd->time_t1 = motion_det_cfg->time_t1;
	cmd->time_t2 = motion_det_cfg->time_t2;
	cmd->n1 = motion_det_cfg->n1;
	cmd->n2 = motion_det_cfg->n2;
	cmd->time_t1_gap = motion_det_cfg->time_t1_gap;
	cmd->time_t2_gap = motion_det_cfg->time_t2_gap;
	cmd->coarse_K = motion_det_cfg->coarse_K;
	cmd->fine_K = motion_det_cfg->fine_K;
	cmd->coarse_Q = motion_det_cfg->coarse_Q;
	cmd->fine_Q = motion_det_cfg->fine_Q;
	cmd->md_coarse_thr_high = motion_det_cfg->md_coarse_thr_high;
	cmd->md_fine_thr_high = motion_det_cfg->md_fine_thr_high;
	cmd->md_coarse_thr_low = motion_det_cfg->md_coarse_thr_low;
	cmd->md_fine_thr_low = motion_det_cfg->md_fine_thr_low;

	err = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, sizeof(*cmd),
				   WMI_MOTION_DET_CONFIG_PARAM_CMDID);
	if (err) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}
	wma_nofl_alert("Set motion_det_config to vdevId %d\n"
		 "time_t1 %d\n"
		 "time_t2 %d\n"
		 "n1 %d\n"
		 "n2 %d\n"
		 "time_t1_gap %d\n"
		 "time_t2_gap %d\n"
		 "coarse_K %d\n"
		 "fine_K %d\n"
		 "coarse_Q %d\n"
		 "fine_Q %d\n"
		 "md_coarse_thr_high %d\n"
		 "md_fine_thr_high %d\n"
		 "md_coarse_thr_low %d\n"
		 "md_fine_thr_low %d\n",
		 motion_det_cfg->vdev_id,
		 motion_det_cfg->time_t1,
		 motion_det_cfg->time_t2,
		 motion_det_cfg->n1,
		 motion_det_cfg->n2,
		 motion_det_cfg->time_t1_gap,
		 motion_det_cfg->time_t2_gap,
		 motion_det_cfg->coarse_K,
		 motion_det_cfg->fine_K,
		 motion_det_cfg->coarse_Q,
		 motion_det_cfg->fine_Q,
		 motion_det_cfg->md_coarse_thr_high,
		 motion_det_cfg->md_fine_thr_high,
		 motion_det_cfg->md_coarse_thr_low,
		 motion_det_cfg->md_fine_thr_low);
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_set_motion_det_enable - Sends motion detection start/stop wmi cmd
 * @wma_handle: WMA global handle
 * @md_en: motion detection start/stop
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_ERROR on error
 */
static QDF_STATUS wma_set_motion_det_enable(tp_wma_handle wma_handle,
					    struct sme_motion_det_en *md_en)
{
	wmi_motion_det_start_stop_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int err;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, sizeof(*cmd));
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_motion_det_start_stop_cmd_fixed_param *)wmi_buf_data(buf);
	qdf_mem_zero(cmd, sizeof(*cmd));

	WMITLV_SET_HDR(
		&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_motion_det_start_stop_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_motion_det_start_stop_cmd_fixed_param));
	cmd->vdev_id = md_en->vdev_id;
	cmd->enable = md_en->enable;

	err = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, sizeof(*cmd),
				   WMI_MOTION_DET_START_STOP_CMDID);
	if (err) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}
	wma_alert("Set motion_det_enable to vdevId %d %d", md_en->vdev_id,
		 md_en->enable);
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_set_motion_det_base_line_config - Sends md baselining cfg wmi cmd
 * @wma_handle: WMA global handle
 * @md_base_line_cfg: md baselining configuration
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_ERROR on error
 */
static QDF_STATUS wma_set_motion_det_base_line_config(
		tp_wma_handle wma_handle,
		struct sme_motion_det_base_line_cfg *md_base_line_cfg)
{
	wmi_motion_det_base_line_config_params_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int err;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, sizeof(*cmd));
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_motion_det_base_line_config_params_cmd_fixed_param *)
		wmi_buf_data(buf);
	qdf_mem_zero(cmd, sizeof(*cmd));

	WMITLV_SET_HDR(
	&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_motion_det_base_line_config_params_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN(
		wmi_motion_det_base_line_config_params_cmd_fixed_param));

	cmd->vdev_id = md_base_line_cfg->vdev_id;
	cmd->bl_time_t = md_base_line_cfg->bl_time_t;
	cmd->bl_packet_gap = md_base_line_cfg->bl_packet_gap;
	cmd->bl_n = md_base_line_cfg->bl_n;
	cmd->bl_num_meas = md_base_line_cfg->bl_num_meas;

	err = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,	sizeof(*cmd),
				   WMI_MOTION_DET_BASE_LINE_CONFIG_PARAM_CMDID);
	if (err) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}
	wma_nofl_alert("Set motion_det_baseline_config to vdevId %d\n"
		 "bl_time_t %d\n"
		 "bl_packet_gap %d\n"
		 "bl_n %d\n"
		 "bl_num_meas %d\n",
		 md_base_line_cfg->vdev_id,
		 md_base_line_cfg->bl_time_t,
		 md_base_line_cfg->bl_packet_gap,
		 md_base_line_cfg->bl_n,
		 md_base_line_cfg->bl_num_meas);
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_set_motion_det_base_line_enable - Sends md baselining start/stop wmi cmd
 * @wma_handle: WMA global handle
 * @md_base_line_en: motion detection baselining start/stop
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_ERROR on error
 */
static QDF_STATUS wma_set_motion_det_base_line_enable(
			tp_wma_handle wma_handle,
			struct sme_motion_det_base_line_en *md_base_line_en)
{
	wmi_motion_det_base_line_start_stop_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int err;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, sizeof(*cmd));
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_motion_det_base_line_start_stop_cmd_fixed_param *)
		wmi_buf_data(buf);
	qdf_mem_zero(cmd, sizeof(*cmd));

	WMITLV_SET_HDR(
	&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_motion_det_base_line_start_stop_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN(
		wmi_motion_det_base_line_start_stop_cmd_fixed_param));

	cmd->vdev_id = md_base_line_en->vdev_id;
	cmd->enable = md_base_line_en->enable;

	err = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, sizeof(*cmd),
				   WMI_MOTION_DET_BASE_LINE_START_STOP_CMDID);
	if (err) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}
	wma_alert("Set motion_det_base_line_enable to vdevId %d enable %d",
		 md_base_line_en->vdev_id, md_base_line_en->enable);
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_MOTION_DETECTION */

/**
 * wma_mc_process_msg() - process wma messages and call appropriate function.
 * @msg: message
 *
 * Return: QDF_SUCCESS for success otherwise failure
 */
static QDF_STATUS wma_mc_process_msg(struct scheduler_msg *msg)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma_handle;

	if (!msg) {
		wma_err("msg is NULL");
		QDF_ASSERT(0);
		qdf_status = QDF_STATUS_E_INVAL;
		goto end;
	}

	wma_debug("msg->type = %x %s", msg->type,
		 mac_trace_get_wma_msg_string(msg->type));

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		wma_err("wma_handle is NULL");
		QDF_ASSERT(0);
		qdf_mem_free(msg->bodyptr);
		qdf_status = QDF_STATUS_E_INVAL;
		goto end;
	}

	switch (msg->type) {
#ifdef FEATURE_WLAN_ESE
	case WMA_TSM_STATS_REQ:
		wma_debug("McThread: WMA_TSM_STATS_REQ");
		wma_process_tsm_stats_req(wma_handle, (void *)msg->bodyptr);
		break;
#endif /* FEATURE_WLAN_ESE */
	case WMA_UPDATE_CHAN_LIST_REQ:
		wma_update_channel_list(wma_handle,
					(tSirUpdateChanList *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_ADD_STA_REQ:
		wma_add_sta(wma_handle, (tpAddStaParams) msg->bodyptr);
		break;
	case WMA_SEND_PEER_UNMAP_CONF:
		wma_peer_unmap_conf_send(
			wma_handle,
			(struct send_peer_unmap_conf_params *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_DELETE_STA_REQ:
		wma_delete_sta(wma_handle, (tpDeleteStaParams) msg->bodyptr);
		break;
	case WMA_DELETE_BSS_HO_FAIL_REQ:
		wma_delete_bss_ho_fail(wma_handle, msg->bodyval);
		break;
	case WMA_DELETE_BSS_REQ:
		wma_delete_bss(wma_handle, msg->bodyval);
		break;
	case WMA_UPDATE_EDCA_PROFILE_IND:
		wma_process_update_edca_param_req(wma_handle,
						  (tEdcaParams *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SEND_BEACON_REQ:
		wma_send_beacon(wma_handle, (tpSendbeaconParams) msg->bodyptr);
		break;
	case WMA_SEND_AP_VDEV_UP:
		wma_set_ap_vdev_up(wma_handle, msg->bodyval);
		break;
	case WMA_SEND_PROBE_RSP_TMPL:
		wma_send_probe_rsp_tmpl(wma_handle,
					(tpSendProbeRespParams) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_CLI_SET_CMD:
		wma_process_cli_set_cmd(wma_handle,
					(wma_cli_set_cmd_t *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_PDEV_IE_REQ:
		wma_process_set_pdev_ie_req(wma_handle,
				(struct set_ie_param *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#if !defined(REMOVE_PKT_LOG) && defined(FEATURE_PKTLOG)
	case WMA_PKTLOG_ENABLE_REQ:
		wma_pktlog_wmi_send_cmd(wma_handle,
			(struct ath_pktlog_wmi_params *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* REMOVE_PKT_LOG */
	case WMA_ENABLE_UAPSD_REQ:
		wma_enable_uapsd_mode(wma_handle,
				      (tpEnableUapsdParams) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_DISABLE_UAPSD_REQ:
		wma_disable_uapsd_mode(wma_handle,
				       (tpDisableUapsdParams) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_DTIM_PERIOD:
		wma_set_dtim_period(wma_handle,
				    (struct set_dtim_params *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_TX_POWER_REQ:
		wma_set_tx_power(wma_handle, (tpMaxTxPowerParams) msg->bodyptr);
		break;
	case WMA_SET_MAX_TX_POWER_REQ:
		wma_set_max_tx_power(wma_handle,
				     (tpMaxTxPowerParams) msg->bodyptr);
		break;
	case WMA_SET_KEEP_ALIVE:
		wma_set_keepalive_req(wma_handle, msg->bodyptr);
		break;
#ifdef FEATURE_WLAN_ESE
	case WMA_SET_PLM_REQ:
		wma_config_plm(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif

	case WMA_UPDATE_OP_MODE:
		wma_process_update_opmode(wma_handle,
					  (tUpdateVHTOpMode *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_UPDATE_RX_NSS:
		wma_process_update_rx_nss(wma_handle,
					  (tUpdateRxNss *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_UPDATE_MEMBERSHIP:
		wma_process_update_membership(wma_handle,
			(tUpdateMembership *) msg->bodyptr);
		break;
	case WMA_UPDATE_USERPOS:
		wma_process_update_userpos(wma_handle,
					   (tUpdateUserPos *) msg->bodyptr);
		break;
	case WMA_UPDATE_BEACON_IND:
		wma_process_update_beacon_params(wma_handle,
			(tUpdateBeaconParams *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;

	case WMA_ADD_TS_REQ:
		wma_add_ts_req(wma_handle, msg->bodyptr);
		break;

	case WMA_DEL_TS_REQ:
		wma_del_ts_req(wma_handle, msg->bodyptr);
		break;

	case WMA_AGGR_QOS_REQ:
		wma_aggr_qos_req(wma_handle, msg->bodyptr);
		break;

	case WMA_8023_MULTICAST_LIST_REQ:
		wma_process_mcbc_set_filter_req(wma_handle,
				(tpSirRcvFltMcAddrList) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;

	case WMA_ROAM_PRE_AUTH_STATUS:
		wma_send_roam_preauth_status(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;

	case WMA_ROAM_SYNC_TIMEOUT:
		wma_handle_roam_sync_timeout(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_RATE_UPDATE_IND:
		wma_process_rate_update_indicate(wma_handle,
				(tSirRateUpdateInd *) msg->bodyptr);
		break;

#ifdef FEATURE_WLAN_TDLS
	case WMA_UPDATE_TDLS_PEER_STATE:
		wma_update_tdls_peer_state(wma_handle, msg->bodyptr);
		break;
#endif /* FEATURE_WLAN_TDLS */
	case WMA_ADD_PERIODIC_TX_PTRN_IND:
		wma_process_add_periodic_tx_ptrn_ind(wma_handle,
				(tSirAddPeriodicTxPtrn *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_DEL_PERIODIC_TX_PTRN_IND:
		wma_process_del_periodic_tx_ptrn_ind(wma_handle,
				(tSirDelPeriodicTxPtrn *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_TX_POWER_LIMIT:
		wma_process_tx_power_limits(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SEND_ADDBA_REQ:
		wma_process_send_addba_req(wma_handle,
				(struct send_add_ba_req *)msg->bodyptr);
		break;

#ifdef FEATURE_WLAN_CH_AVOID
	case WMA_CH_AVOID_UPDATE_REQ:
		wma_process_ch_avoid_update_req(wma_handle,
				(tSirChAvoidUpdateReq *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* FEATURE_WLAN_CH_AVOID */
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	case WMA_SET_AUTO_SHUTDOWN_TIMER_REQ:
		wma_set_auto_shutdown_timer_req(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* FEATURE_WLAN_AUTO_SHUTDOWN */
	case WMA_DHCP_START_IND:
	case WMA_DHCP_STOP_IND:
		wma_process_dhcp_ind(wma_handle, (tAniDHCPInd *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_INIT_THERMAL_INFO_CMD:
		wma_process_init_thermal_info(wma_handle,
					      (t_thermal_mgmt *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;

	case WMA_SET_THERMAL_LEVEL:
		wma_process_set_thermal_level(wma_handle, msg->bodyval);
		break;
#ifdef CONFIG_HL_SUPPORT
	case WMA_INIT_BAD_PEER_TX_CTL_INFO_CMD:
		wma_process_init_bad_peer_tx_ctl_info(
			wma_handle,
			(struct t_bad_peer_txtcl_config *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
			break;
#endif
	case WMA_SET_MIMOPS_REQ:
		wma_process_set_mimops_req(wma_handle,
					   (tSetMIMOPS *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_SAP_INTRABSS_DIS:
		wma_set_vdev_intrabss_fwd(wma_handle,
					  (tDisableIntraBssFwd *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_GET_ISOLATION:
		wma_get_isolation(wma_handle);
		break;
	case WMA_MODEM_POWER_STATE_IND:
		wma_notify_modem_power_state(wma_handle,
				(tSirModemPowerStateInd *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#ifdef WLAN_FEATURE_STATS_EXT
	case WMA_STATS_EXT_REQUEST:
		wma_stats_ext_req(wma_handle,
				  (tpStatsExtRequest) (msg->bodyptr));
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* WLAN_FEATURE_STATS_EXT */
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
	case WMA_WLAN_EXT_WOW:
		wma_enable_ext_wow(wma_handle,
				   (tSirExtWoWParams *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_WLAN_SET_APP_TYPE1_PARAMS:
		wma_set_app_type1_params_in_fw(wma_handle,
				(tSirAppType1Params *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_WLAN_SET_APP_TYPE2_PARAMS:
		wma_set_app_type2_params_in_fw(wma_handle,
				(tSirAppType2Params *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* WLAN_FEATURE_EXTWOW_SUPPORT */
#ifdef FEATURE_WLAN_EXTSCAN
	case WMA_EXTSCAN_START_REQ:
		wma_start_extscan(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_EXTSCAN_STOP_REQ:
		wma_stop_extscan(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_EXTSCAN_SET_BSSID_HOTLIST_REQ:
		wma_extscan_start_hotlist_monitor(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_EXTSCAN_RESET_BSSID_HOTLIST_REQ:
		wma_extscan_stop_hotlist_monitor(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_EXTSCAN_SET_SIGNF_CHANGE_REQ:
		wma_extscan_start_change_monitor(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_EXTSCAN_RESET_SIGNF_CHANGE_REQ:
		wma_extscan_stop_change_monitor(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_EXTSCAN_GET_CACHED_RESULTS_REQ:
		wma_extscan_get_cached_results(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_EXTSCAN_GET_CAPABILITIES_REQ:
		wma_extscan_get_capabilities(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_EPNO_LIST_REQ:
		wma_set_epno_network_list(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_PASSPOINT_LIST_REQ:
		/* Issue reset passpoint network list first and clear
		 * the entries
		 */
		wma_reset_passpoint_network_list(wma_handle, msg->bodyptr);

		wma_set_passpoint_network_list(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_RESET_PASSPOINT_LIST_REQ:
		wma_reset_passpoint_network_list(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* FEATURE_WLAN_EXTSCAN */
	case WMA_SET_SCAN_MAC_OUI_REQ:
		wma_scan_probe_setoui(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
	case WMA_LINK_LAYER_STATS_CLEAR_REQ:
		wma_process_ll_stats_clear_req(wma_handle,
			(tpSirLLStatsClearReq) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_LINK_LAYER_STATS_SET_REQ:
		wma_process_ll_stats_set_req(wma_handle,
					     (tpSirLLStatsSetReq) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_LINK_LAYER_STATS_GET_REQ:
		wma_process_ll_stats_get_req(wma_handle,
					     (tpSirLLStatsGetReq) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WDA_LINK_LAYER_STATS_SET_THRESHOLD:
		wma_config_stats_ext_threshold(wma_handle,
			(struct sir_ll_ext_stats_threshold *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	case WMA_ROAM_OFFLOAD_SYNCH_FAIL:
		wma_process_roam_synch_fail(wma_handle,
			(struct roam_offload_synch_fail *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_ROAM_INVOKE:
		wma_process_roam_invoke(wma_handle,
			(struct wma_roam_invoke_cmd *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
	case SIR_HAL_SET_BASE_MACADDR_IND:
		wma_set_base_macaddr_indicate(wma_handle,
					      (tSirMacAddr *) msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_LINK_STATUS_GET_REQ:
		wma_process_link_status_req(wma_handle,
					    (tAniGetLinkStatus *) msg->bodyptr);
		break;
	case WMA_GET_TEMPERATURE_REQ:
		wma_get_temperature(wma_handle);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_TSF_GPIO_PIN:
		wma_set_tsf_gpio_pin(wma_handle, msg->bodyval);
		break;

#ifdef DHCP_SERVER_OFFLOAD
	case WMA_SET_DHCP_SERVER_OFFLOAD_CMD:
		wma_process_dhcpserver_offload(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* DHCP_SERVER_OFFLOAD */
#ifdef WLAN_FEATURE_GPIO_LED_FLASHING
	case WMA_LED_FLASHING_REQ:
		wma_set_led_flashing(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* WLAN_FEATURE_GPIO_LED_FLASHING */
	case SIR_HAL_SET_MAS:
		wma_process_set_mas(wma_handle,
				(uint32_t *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_SET_MIRACAST:
		wma_process_set_miracast(wma_handle,
				(uint32_t *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_CONFIG_STATS_FACTOR:
		wma_config_stats_factor(wma_handle,
					(struct sir_stats_avg_factor *)
					msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_CONFIG_GUARD_TIME:
		wma_config_guard_time(wma_handle,
				      (struct sir_guard_time_request *)
				      msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_START_STOP_LOGGING:
		wma_set_wifi_start_packet_stats(wma_handle,
				(struct sir_wifi_start_log *)msg->bodyptr);
		wma_enable_specific_fw_logs(wma_handle,
				(struct sir_wifi_start_log *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_FLUSH_LOG_TO_FW:
		wma_send_flush_logs_to_fw(wma_handle);
		/* Body ptr is NULL here */
		break;
	case WMA_SET_RSSI_MONITOR_REQ:
		wma_set_rssi_monitoring(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_SET_PCL_TO_FW:
		wma_send_set_pcl_cmd(wma_handle,
				     (struct set_pcl_req *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_PDEV_SET_HW_MODE:
		wma_send_pdev_set_hw_mode_cmd(wma_handle,
				(struct policy_mgr_hw_mode *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_WISA_PARAMS:
		wma_set_wisa_params(wma_handle,
			(struct sir_wisa_params *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_PDEV_DUAL_MAC_CFG_REQ:
		wma_send_pdev_set_dual_mac_config(wma_handle,
				(struct policy_mgr_dual_mac_config *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_IE_INFO:
		wma_process_set_ie_info(wma_handle,
			(struct vdev_ie_info *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_CFG_VENDOR_ACTION_TB_PPDU:
		wma_process_cfg_action_frm_tb_ppdu(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_SOC_ANTENNA_MODE_REQ:
		wma_send_pdev_set_antenna_mode(wma_handle,
			(struct sir_antenna_mode_param *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_LRO_CONFIG_CMD:
		wma_lro_config_cmd(wma_handle,
			(struct cdp_lro_hash_config *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_GW_PARAM_UPDATE_REQ:
		wma_set_gateway_params(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_ADAPT_DWELLTIME_CONF_PARAMS:
		wma_send_adapt_dwelltime_params(wma_handle,
			(struct adaptive_dwelltime_params *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_HT40_OBSS_SCAN_IND:
		wma_send_ht40_obss_scanind(wma_handle,
			(struct obss_ht40_scanind *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_ADD_BCN_FILTER_CMDID:
		wma_add_beacon_filter(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_REMOVE_BCN_FILTER_CMDID:
		wma_remove_beacon_filter(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WDA_APF_GET_CAPABILITIES_REQ:
		wma_get_apf_capabilities(wma_handle);
		break;
	case SIR_HAL_POWER_DBG_CMD:
		wma_process_hal_pwr_dbg_cmd(wma_handle,
					    msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SEND_FREQ_RANGE_CONTROL_IND:
		wma_enable_disable_caevent_ind(wma_handle, msg->bodyval);
		break;
	case SIR_HAL_UPDATE_TX_FAIL_CNT_TH:
		wma_update_tx_fail_cnt_th(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_LONG_RETRY_LIMIT_CNT:
		wma_update_long_retry_limit(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_SHORT_RETRY_LIMIT_CNT:
		wma_update_short_retry_limit(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_POWER_DEBUG_STATS_REQ:
		wma_process_power_debug_stats_req(wma_handle);
		break;
	case WMA_BEACON_DEBUG_STATS_REQ:
		wma_process_beacon_debug_stats_req(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_GET_RCPI_REQ:
		wma_get_rcpi_req(wma_handle,
				 (struct sme_rcpi_req *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_WOW_PULSE_CMD:
		wma_send_wow_pulse_cmd(wma_handle,
			(struct wow_pulse_mode *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_DBS_SCAN_SEL_CONF_PARAMS:
		wma_send_dbs_scan_selection_params(wma_handle,
			(struct wmi_dbs_scan_sel_params *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_ARP_STATS_REQ:
		wma_set_arp_req_stats(wma_handle,
			(struct set_arp_stats_params *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_GET_ARP_STATS_REQ:
		wma_get_arp_req_stats(wma_handle,
			(struct get_arp_stats_params *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case SIR_HAL_SET_DEL_PMKID_CACHE:
		wma_set_del_pmkid_cache(wma_handle, msg->bodyptr);
		if (msg->bodyptr) {
			qdf_mem_zero(msg->bodyptr,
				     sizeof(struct wmi_unified_pmk_cache));
			qdf_mem_free(msg->bodyptr);
		}
		break;
	case SIR_HAL_HLP_IE_INFO:
		wma_roam_scan_send_hlp(wma_handle,
			(struct hlp_params *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_LIMIT_OFF_CHAN:
		wma_process_limit_off_chan(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_OBSS_DETECTION_REQ:
		wma_send_obss_detection_cfg(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_INVOKE_NEIGHBOR_REPORT:
		wma_send_invoke_neighbor_report(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_OBSS_COLOR_COLLISION_REQ:
		wma_process_obss_color_collision_req(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_GET_ROAM_SCAN_STATS:
		wma_get_roam_scan_stats(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#ifdef WLAN_FEATURE_MOTION_DETECTION
	case WMA_SET_MOTION_DET_CONFIG:
		wma_set_motion_det_config(
			wma_handle,
			(struct sme_motion_det_cfg *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_MOTION_DET_ENABLE:
		wma_set_motion_det_enable(
			wma_handle,
			(struct sme_motion_det_en *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_MOTION_DET_BASE_LINE_CONFIG:
		wma_set_motion_det_base_line_config(
			wma_handle,
			(struct sme_motion_det_base_line_cfg *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_MOTION_DET_BASE_LINE_ENABLE:
		wma_set_motion_det_base_line_enable(
			wma_handle,
			(struct sme_motion_det_base_line_en *)msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* WLAN_FEATURE_MOTION_DETECTION */
#ifdef FW_THERMAL_THROTTLE_SUPPORT
	case WMA_SET_THERMAL_THROTTLE_CFG:
		if (!wma_handle->thermal_mgmt_info.thermalMgmtEnabled)
			wmi_unified_thermal_mitigation_param_cmd_send(
					wma_handle->wmi_handle, msg->bodyptr);
		else
			qdf_status = QDF_STATUS_E_INVAL;
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_SET_THERMAL_MGMT:
		if (!wma_handle->thermal_mgmt_info.thermalMgmtEnabled)
			wma_set_thermal_mgmt(
				wma_handle,
				*((t_thermal_cmd_params *)msg->bodyptr));
		else
			qdf_status = QDF_STATUS_E_INVAL;
		qdf_mem_free(msg->bodyptr);
		break;
#endif /* FW_THERMAL_THROTTLE_SUPPORT */
#ifdef WLAN_MWS_INFO_DEBUGFS
	case WMA_GET_MWS_COEX_INFO_REQ:
		wma_get_mws_coex_info_req(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
#endif
	case WMA_ROAM_SCAN_CH_REQ:
		wma_get_roam_scan_ch(wma_handle->wmi_handle, msg->bodyval);
		break;
	case WMA_TWT_ADD_DIALOG_REQUEST:
		wma_twt_process_add_dialog(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_TWT_DEL_DIALOG_REQUEST:
		wma_twt_process_del_dialog(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_TWT_PAUSE_DIALOG_REQUEST:
		wma_twt_process_pause_dialog(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_TWT_RESUME_DIALOG_REQUEST:
		wma_twt_process_resume_dialog(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case WMA_TWT_NUDGE_DIALOG_REQUEST:
		wma_twt_process_nudge_dialog(wma_handle, msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	default:
		wma_debug("Unhandled WMA message of type %d", msg->type);
		if (msg->bodyptr)
			qdf_mem_free(msg->bodyptr);
	}
end:
	return qdf_status;
}

QDF_STATUS wma_mc_process_handler(struct scheduler_msg *msg)
{
	return wma_mc_process_msg(msg);
}

/**
 * wma_log_completion_timeout() - Log completion timeout
 * @data: Timeout handler data
 *
 * This function is called when log completion timer expires
 *
 * Return: None
 */
void wma_log_completion_timeout(void *data)
{
	tp_wma_handle wma_handle;

	wma_debug("Timeout occurred for log completion command");

	wma_handle = (tp_wma_handle) data;
	if (!wma_handle)
		wma_err("Invalid WMA handle");

	/* Though we did not receive any event from FW,
	 * we can flush whatever logs we have with us
	 */
	cds_logging_set_fw_flush_complete();
}

/**
 * wma_map_pcl_weights() - Map PCL weights
 * @pcl_weight: Internal PCL weights
 *
 * Maps the internal weights of PCL to the weights needed by FW
 *
 * Return: Mapped channel weight of type wmi_pcl_chan_weight
 */
wmi_pcl_chan_weight wma_map_pcl_weights(uint32_t pcl_weight)
{
	switch (pcl_weight) {
	case WEIGHT_OF_GROUP1_PCL_CHANNELS:
		return WMI_PCL_WEIGHT_VERY_HIGH;
	case WEIGHT_OF_GROUP2_PCL_CHANNELS:
		return WMI_PCL_WEIGHT_HIGH;
	case WEIGHT_OF_GROUP3_PCL_CHANNELS:
		return WMI_PCL_WEIGHT_MEDIUM;
	case WEIGHT_OF_NON_PCL_CHANNELS:
		return WMI_PCL_WEIGHT_LOW;
	default:
		return WMI_PCL_WEIGHT_DISALLOW;
	}
}

/**
 * wma_send_set_pcl_cmd() - Send WMI_SOC_SET_PCL_CMDID to FW
 * @wma_handle: WMA handle
 * @msg: PCL structure containing the PCL and the number of channels
 *
 * WMI_PDEV_SET_PCL_CMDID provides a Preferred Channel List (PCL) to the WLAN
 * firmware. The DBS Manager is the consumer of this information in the WLAN
 * firmware. The channel list will be used when a Virtual DEVice (VDEV) needs
 * to migrate to a new channel without host driver involvement. An example of
 * this behavior is Legacy Fast Roaming (LFR 3.0). Generally, the host will
 * manage the channel selection without firmware involvement.
 *
 * WMI_PDEV_SET_PCL_CMDID will carry only the weight list and not the actual
 * channel list. The weights corresponds to the channels sent in
 * WMI_SCAN_CHAN_LIST_CMDID. The channels from PCL would be having a higher
 * weightage compared to the non PCL channels.
 *
 * Return: Success if the cmd is sent successfully to the firmware
 */
QDF_STATUS wma_send_set_pcl_cmd(tp_wma_handle wma_handle,
				struct set_pcl_req *msg)
{
	uint32_t i;
	QDF_STATUS status;

	if (!wma_handle) {
		wma_err("WMA handle is NULL. Cannot issue command");
		return QDF_STATUS_E_NULL_VALUE;
	}

	/*
	 * if vdev_id is WLAN_UMAC_VDEV_ID_MAX, then roaming is enabled on
	 * only one sta, so PDEV PCL command needs to be sent.
	 * If a valid vdev id is present, then vdev pcl command needs to be
	 * sent.
	 */
	if (msg->vdev_id != WLAN_UMAC_VDEV_ID_MAX)
		return wlan_cm_roam_send_set_vdev_pcl(wma_handle->psoc, msg);

	for (i = 0; i < wma_handle->saved_chan.num_channels; i++) {
		msg->chan_weights.saved_chan_list[i] =
					wma_handle->saved_chan.ch_freq_list[i];
	}

	msg->chan_weights.saved_num_chan = wma_handle->saved_chan.num_channels;
	status = policy_mgr_get_valid_chan_weights(wma_handle->psoc,
		(struct policy_mgr_pcl_chan_weights *)&msg->chan_weights,
		PM_STA_MODE);

	for (i = 0; i < msg->chan_weights.saved_num_chan; i++) {
		msg->chan_weights.weighed_valid_list[i] =
			wma_map_pcl_weights(
				msg->chan_weights.weighed_valid_list[i]);
		/* Dont allow roaming on 2G when 5G_ONLY configured */
		if (((wma_handle->bandcapability == BAND_5G) ||
		    (msg->band_mask == BIT(REG_BAND_5G))) &&
		    (WLAN_REG_IS_24GHZ_CH_FREQ(
		    msg->chan_weights.saved_chan_list[i]))) {
			msg->chan_weights.weighed_valid_list[i] =
				WEIGHT_OF_DISALLOWED_CHANNELS;
		}
		if (msg->band_mask == BIT(REG_BAND_2G) &&
		    !WLAN_REG_IS_24GHZ_CH_FREQ(
		    msg->chan_weights.saved_chan_list[i]))
			msg->chan_weights.weighed_valid_list[i] =
				WEIGHT_OF_DISALLOWED_CHANNELS;
	}

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		wma_err("Error in creating weighed pcl");
		return status;
	}
	wma_debug("Dump channel list send to wmi");
	policy_mgr_dump_channel_list(msg->chan_weights.saved_num_chan,
				     msg->chan_weights.saved_chan_list,
				     msg->chan_weights.weighed_valid_list);

	if (wmi_unified_pdev_set_pcl_cmd(wma_handle->wmi_handle,
					 &msg->chan_weights))
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_send_pdev_set_hw_mode_cmd() - Send WMI_PDEV_SET_HW_MODE_CMDID to FW
 * @wma_handle: WMA handle
 * @msg: Structure containing the following parameters
 *
 * - hw_mode_index: The HW_Mode field is a enumerated type that is selected
 * from the HW_Mode table, which is returned in the WMI_SERVICE_READY_EVENTID.
 *
 * Provides notification to the WLAN firmware that host driver is requesting a
 * HardWare (HW) Mode change. This command is needed to support iHelium in the
 * configurations that include the Dual Band Simultaneous (DBS) feature.
 *
 * Return: Success if the cmd is sent successfully to the firmware
 */
QDF_STATUS wma_send_pdev_set_hw_mode_cmd(tp_wma_handle wma_handle,
					 struct policy_mgr_hw_mode *msg)
{
	struct sir_set_hw_mode_resp *param;
	struct wma_target_req *timeout_msg;

	if (!wma_handle) {
		wma_err("WMA handle is NULL. Cannot issue command");
		/* Handle is NULL. Will not be able to send failure
		 * response as well
		 */
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!msg) {
		wma_err("Set HW mode param is NULL");
		/* Lets try to free the active command list */
		goto fail;
	}

	wma_acquire_wakelock(&wma_handle->wmi_cmd_rsp_wake_lock,
			     WMA_VDEV_HW_MODE_REQUEST_TIMEOUT);
	if (wmi_unified_soc_set_hw_mode_cmd(wma_handle->wmi_handle,
					    msg->hw_mode_index)) {
		wma_release_wakelock(&wma_handle->wmi_cmd_rsp_wake_lock);
		goto fail;
	}
	timeout_msg = wma_fill_hold_req(wma_handle, 0,
			SIR_HAL_PDEV_SET_HW_MODE,
			WMA_PDEV_SET_HW_MODE_RESP, NULL,
			WMA_VDEV_HW_MODE_REQUEST_TIMEOUT - 1);
	if (!timeout_msg) {
		wma_err("Failed to allocate request for SIR_HAL_PDEV_SET_HW_MODE");
		wma_remove_req(wma_handle, 0, WMA_PDEV_SET_HW_MODE_RESP);
	}

	return QDF_STATUS_SUCCESS;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return QDF_STATUS_E_NULL_VALUE;

	param->status = SET_HW_MODE_STATUS_ECANCELED;
	param->cfgd_hw_mode_index = 0;
	param->num_vdev_mac_entries = 0;
	wma_debug("Sending HW mode fail response to LIM");
	wma_send_msg(wma_handle, SIR_HAL_PDEV_SET_HW_MODE_RESP,
			(void *) param, 0);
	return QDF_STATUS_E_FAILURE;
}

/**
 * wma_send_pdev_set_dual_mac_config() - Set dual mac config to FW
 * @wma_handle: WMA handle
 * @msg: Dual MAC config parameters
 *
 * Configures WLAN firmware with the dual MAC features
 *
 * Return: QDF_STATUS. 0 on success.
 */
QDF_STATUS wma_send_pdev_set_dual_mac_config(tp_wma_handle wma_handle,
		struct policy_mgr_dual_mac_config *msg)
{
	QDF_STATUS status;
	struct wma_target_req *req_msg;
	struct sir_dual_mac_config_resp *resp;

	if (!wma_handle) {
		wma_err("WMA handle is NULL. Cannot issue command");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!msg) {
		wma_err("Set dual mode config is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	req_msg = wma_fill_hold_req(wma_handle, 0,
				    SIR_HAL_PDEV_DUAL_MAC_CFG_REQ,
				    WMA_PDEV_MAC_CFG_RESP, NULL,
				    WMA_VDEV_DUAL_MAC_CFG_TIMEOUT);
	if (!req_msg) {
		wma_err("Failed to allocate request for SIR_HAL_PDEV_DUAL_MAC_CFG_REQ");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * aquire the wake lock here and release it in response handler function
	 * In error condition, release the wake lock right away
	 */
	wma_acquire_wakelock(&wma_handle->wmi_cmd_rsp_wake_lock,
			     WMA_VDEV_PLCY_MGR_WAKE_LOCK_TIMEOUT);
	status = wmi_unified_pdev_set_dual_mac_config_cmd(
				wma_handle->wmi_handle,
				(struct policy_mgr_dual_mac_config *)msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to send WMI_PDEV_SET_DUAL_MAC_CONFIG_CMDID: %d",
			status);
		wma_release_wakelock(&wma_handle->wmi_cmd_rsp_wake_lock);
		wma_remove_req(wma_handle, 0, WMA_PDEV_MAC_CFG_RESP);
		goto fail;
	}
	policy_mgr_update_dbs_req_config(wma_handle->psoc,
	msg->scan_config, msg->fw_mode_config);

	return QDF_STATUS_SUCCESS;

fail:
	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp)
		return QDF_STATUS_E_NULL_VALUE;

	resp->status = SET_HW_MODE_STATUS_ECANCELED;
	wma_debug("Sending failure response to LIM");
	wma_send_msg(wma_handle, SIR_HAL_PDEV_MAC_CFG_RESP, (void *) resp, 0);

	return QDF_STATUS_E_FAILURE;
}

/**
 * wma_send_pdev_set_antenna_mode() - Set antenna mode to FW
 * @wma_handle: WMA handle
 * @msg: Antenna mode parameters
 *
 * Send WMI_PDEV_SET_ANTENNA_MODE_CMDID to FW requesting to
 * modify the number of TX/RX chains from host
 *
 * Return: QDF_STATUS. 0 on success.
 */
QDF_STATUS wma_send_pdev_set_antenna_mode(tp_wma_handle wma_handle,
		struct sir_antenna_mode_param *msg)
{
	wmi_pdev_set_antenna_mode_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct sir_antenna_mode_resp *param;

	if (!wma_handle) {
		wma_err("WMA handle is NULL. Cannot issue command");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!msg) {
		wma_err("Set antenna mode param is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	len = sizeof(*cmd);

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		status = QDF_STATUS_E_NOMEM;
		goto resp;
	}

	cmd = (wmi_pdev_set_antenna_mode_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_pdev_set_antenna_mode_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_pdev_set_antenna_mode_cmd_fixed_param));

	cmd->pdev_id = OL_TXRX_PDEV_ID;
	/* Bits 0-15 is num of RX chains 16-31 is num of TX chains */
	cmd->num_txrx_chains = msg->num_rx_chains;
	cmd->num_txrx_chains |= (msg->num_tx_chains << 16);

	wma_debug("Num of chains TX: %d RX: %d txrx_chains: 0x%x",
		 msg->num_tx_chains,
		 msg->num_rx_chains, cmd->num_txrx_chains);

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				 WMI_PDEV_SET_ANTENNA_MODE_CMDID)) {
		wmi_buf_free(buf);
		status = QDF_STATUS_E_FAILURE;
		goto resp;
	}
	status = QDF_STATUS_SUCCESS;

resp:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return QDF_STATUS_E_NOMEM;

	param->status = (status) ?
		SET_ANTENNA_MODE_STATUS_ECANCELED :
		SET_ANTENNA_MODE_STATUS_OK;
	wma_debug("Send antenna mode resp to LIM status: %d",
		  param->status);
	wma_send_msg(wma_handle, SIR_HAL_SOC_ANTENNA_MODE_RESP,
			(void *) param, 0);
	return status;
}

/**
 * wma_crash_inject() - sends command to FW to simulate crash
 * @wma_handle:         pointer of WMA context
 * @type:               subtype of the command
 * @delay_time_ms:      time in milliseconds for FW to delay the crash
 *
 * This function will send a command to FW in order to simulate different
 * kinds of FW crashes.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_crash_inject(WMA_HANDLE wma_handle, uint32_t type,
			    uint32_t delay_time_ms)
{
	struct crash_inject param;
	tp_wma_handle wma = (tp_wma_handle)wma_handle;

	param.type = type;
	param.delay_time_ms = delay_time_ms;
	return wmi_crash_inject(wma->wmi_handle, &param);
}

#ifdef RECEIVE_OFFLOAD
int wma_lro_init(struct cdp_lro_hash_config *lro_config)
{
	struct scheduler_msg msg = {0};
	struct cdp_lro_hash_config *iwcmd;

	iwcmd = qdf_mem_malloc(sizeof(*iwcmd));
	if (!iwcmd)
		return -ENOMEM;

	*iwcmd = *lro_config;

	msg.type = WMA_LRO_CONFIG_CMD;
	msg.reserved = 0;
	msg.bodyptr = iwcmd;

	if (QDF_STATUS_SUCCESS !=
		scheduler_post_message(QDF_MODULE_ID_WMA,
				       QDF_MODULE_ID_WMA,
				       QDF_MODULE_ID_WMA, &msg)) {
		qdf_mem_free(iwcmd);
		return -EAGAIN;
	}

	wma_debug("sending the LRO configuration to the fw");
	return 0;
}
#endif

QDF_STATUS wma_configure_smps_params(uint32_t vdev_id, uint32_t param_id,
							uint32_t param_val)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	int smps_cmd_value;
	int status = QDF_STATUS_E_INVAL;

	if (!wma) {
		wma_err("Failed to get wma");
		return status;
	}

	smps_cmd_value = param_id << WMI_SMPS_PARAM_VALUE_S;
	smps_cmd_value = smps_cmd_value | param_val;

	status = wma_set_smps_params(wma, vdev_id, smps_cmd_value);
	if (status)
		wma_err("Failed to set SMPS Param");

	return status;
}


/**
 * wma_config_bmiss_bcnt_params() - set bmiss config parameters
 * @vdev_id: virtual device for the command
 * @first_cnt: bmiss first value
 * @final_cnt: bmiss final value
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS wma_config_bmiss_bcnt_params(uint32_t vdev_id, uint32_t first_cnt,
		uint32_t final_cnt)
{
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	int status = QDF_STATUS_E_INVAL;

	if (!wma_handle) {
		wma_err("Failed to get wma");
		return status;
	}

	status = wma_roam_scan_bmiss_cnt(wma_handle, first_cnt, final_cnt,
			vdev_id);

	if (status)
		wma_err("Failed to set Bmiss Param");

	return status;
}

QDF_STATUS wma_get_rx_chainmask(uint8_t pdev_id, uint32_t *chainmask_2g,
				uint32_t *chainmask_5g)
{
	struct wlan_psoc_host_mac_phy_caps *mac_phy_cap;
	uint8_t total_mac_phy_cnt, idx;
	struct target_psoc_info *tgt_hdl;
	uint32_t hw_mode_idx = 0, num_hw_modes = 0;

	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		wma_err("wma_handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	tgt_hdl = wlan_psoc_get_tgt_if_handle(wma_handle->psoc);
	if (!tgt_hdl) {
		wma_err("target psoc info is NULL");
		return QDF_STATUS_E_INVAL;
	}

	total_mac_phy_cnt = target_psoc_get_total_mac_phy_cnt(tgt_hdl);
	num_hw_modes = target_psoc_get_num_hw_modes(tgt_hdl);
	if (total_mac_phy_cnt <= pdev_id) {
		wma_err("mac phy cnt %d, pdev id %d",
			total_mac_phy_cnt, pdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if ((wma_handle->new_hw_mode_index != WMA_DEFAULT_HW_MODE_INDEX) &&
	    (wma_handle->new_hw_mode_index <= num_hw_modes))
		hw_mode_idx = wma_handle->new_hw_mode_index;
	mac_phy_cap = target_psoc_get_mac_phy_cap(tgt_hdl);
	if (!mac_phy_cap) {
		wma_err("Invalid MAC PHY capabilities handle");
		return QDF_STATUS_E_FAILURE;
	}
	for (idx = 0; idx < total_mac_phy_cnt; idx++) {
		if (mac_phy_cap[idx].hw_mode_id != hw_mode_idx)
			continue;
		if (mac_phy_cap[idx].supported_bands & WLAN_2G_CAPABILITY)
			*chainmask_2g = mac_phy_cap[idx].rx_chain_mask_2G;
		if (mac_phy_cap[idx].supported_bands & WLAN_5G_CAPABILITY)
			*chainmask_5g = mac_phy_cap[idx].rx_chain_mask_5G;
	}
	wma_debug("pdev id: %d, hw_mode_idx: %d, rx chainmask 2g:%d, 5g:%d",
		 pdev_id, hw_mode_idx, *chainmask_2g, *chainmask_5g);

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_ANI_LEVEL_REQUEST
QDF_STATUS wma_send_ani_level_request(tp_wma_handle wma_handle,
				      uint32_t *freqs, uint8_t num_freqs)
{
	return wmi_unified_ani_level_cmd_send(wma_handle->wmi_handle, freqs,
					      num_freqs);
}
#endif
