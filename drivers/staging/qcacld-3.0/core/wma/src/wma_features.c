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
 *  DOC:    wma_features.c
 *  This file contains different features related functions like WoW,
 *  Offloads, TDLS etc.
 */

/* Header files */

#include "cds_ieee80211_common.h"	/* ieee80211_frame */
#include "wma.h"
#include "wma_api.h"
#include "cds_api.h"
#include "wmi_unified_api.h"
#include "wlan_qct_sys.h"
#include "wni_api.h"
#include "ani_global.h"
#include "wmi_unified.h"
#include "wni_cfg.h"
#include <cdp_txrx_tx_delay.h>
#include <cdp_txrx_peer_ops.h>

#include "qdf_nbuf.h"
#include "qdf_types.h"
#include "qdf_mem.h"
#include "qdf_util.h"

#include "wma_types.h"
#include "lim_api.h"
#include "lim_session_utils.h"
#include "cfg_ucfg_api.h"
#include "cds_utils.h"
#include "cfg_qos.h"
#if !defined(REMOVE_PKT_LOG)
#include "pktlog_ac.h"
#endif /* REMOVE_PKT_LOG */

#include "dbglog_host.h"
#include "csr_api.h"
#include "ol_fw.h"

#include "wma_internal.h"
#include "wma_nan_datapath.h"
#include <cdp_txrx_handle.h>
#include "wlan_pmo_ucfg_api.h"
#include <target_if_scan.h>
#include "wlan_reg_services_api.h"
#include "wlan_roam_debug.h"
#include <wlan_cp_stats_mc_ucfg_api.h>
#ifdef WLAN_FEATURE_NAN
#include "target_if_nan.h"
#endif
#include "wlan_scan_api.h"
#include <wlan_crypto_global_api.h>
#include "cdp_txrx_host_stats.h"

/**
 * WMA_SET_VDEV_IE_SOURCE_HOST - Flag to identify the source of VDEV SET IE
 * command. The value is 0x0 for the VDEV SET IE WMI commands from mobile
 * MCL platform.
 */
#define WMA_SET_VDEV_IE_SOURCE_HOST 0x0

/*
 * Max AMPDU Tx Aggr supported size
 */
#define ADDBA_TXAGGR_SIZE_HELIUM 64
#define ADDBA_TXAGGR_SIZE_LITHIUM 256

static bool is_wakeup_event_console_logs_enabled;

void wma_set_wakeup_logs_to_console(bool value)
{
	is_wakeup_event_console_logs_enabled = value;
}

#if defined(FEATURE_WLAN_DIAG_SUPPORT)
/**
 * qdf_wma_wow_wakeup_stats_event()- send wow wakeup stats
 * @tp_wma_handle wma: WOW wakeup packet counter
 *
 * This function sends wow wakeup stats diag event
 *
 * Return: void.
 */
static inline void qdf_wma_wow_wakeup_stats_event(tp_wma_handle wma)
{
	QDF_STATUS status;
	struct wake_lock_stats stats = {0};

	WLAN_HOST_DIAG_EVENT_DEF(wow_stats,
	struct host_event_wlan_powersave_wow_stats);

	status = ucfg_mc_cp_stats_get_psoc_wake_lock_stats(wma->psoc, &stats);
	if (QDF_IS_STATUS_ERROR(status))
		return;
	qdf_mem_zero(&wow_stats, sizeof(wow_stats));

	wow_stats.wow_bcast_wake_up_count = stats.bcast_wake_up_count;
	wow_stats.wow_ipv4_mcast_wake_up_count = stats.ipv4_mcast_wake_up_count;
	wow_stats.wow_ipv6_mcast_wake_up_count = stats.ipv6_mcast_wake_up_count;
	wow_stats.wow_ipv6_mcast_ra_stats = stats.ipv6_mcast_ra_stats;
	wow_stats.wow_ipv6_mcast_ns_stats = stats.ipv6_mcast_ns_stats;
	wow_stats.wow_ipv6_mcast_na_stats = stats.ipv6_mcast_na_stats;
	wow_stats.wow_pno_match_wake_up_count = stats.pno_match_wake_up_count;
	wow_stats.wow_pno_complete_wake_up_count =
				stats.pno_complete_wake_up_count;
	wow_stats.wow_gscan_wake_up_count = stats.gscan_wake_up_count;
	wow_stats.wow_low_rssi_wake_up_count = stats.low_rssi_wake_up_count;
	wow_stats.wow_rssi_breach_wake_up_count =
				stats.rssi_breach_wake_up_count;
	wow_stats.wow_icmpv4_count = stats.icmpv4_count;
	wow_stats.wow_icmpv6_count = stats.icmpv6_count;
	wow_stats.wow_oem_response_wake_up_count =
				stats.oem_response_wake_up_count;

	WLAN_HOST_DIAG_EVENT_REPORT(&wow_stats, EVENT_WLAN_POWERSAVE_WOW_STATS);
}
#else
static inline void qdf_wma_wow_wakeup_stats_event(tp_wma_handle wma)
{
	return;
}
#endif

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
/**
 * wma_wake_reason_auto_shutdown() - to post auto shutdown event to sme
 *
 * Return: 0 for success or error code
 */
static int wma_wake_reason_auto_shutdown(void)
{
	QDF_STATUS qdf_status;
	struct scheduler_msg sme_msg = { 0 };

	sme_msg.type = eWNI_SME_AUTO_SHUTDOWN_IND;
	qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		wma_err("Fail to post eWNI_SME_AUTO_SHUTDOWN_IND msg to SME");

	return qdf_status_to_os_return(qdf_status);
}
#else
static inline int wma_wake_reason_auto_shutdown(void)
{
	return 0;
}
#endif /* FEATURE_WLAN_AUTO_SHUTDOWN */

#ifdef FEATURE_WLAN_SCAN_PNO
static int wma_wake_reason_nlod(t_wma_handle *wma, uint8_t vdev_id)
{
	wmi_nlo_event nlo_event = { .vdev_id = vdev_id };
	WMI_NLO_MATCH_EVENTID_param_tlvs param = { .fixed_param = &nlo_event };

	return target_if_nlo_match_event_handler(wma, (uint8_t *)&param,
						 sizeof(param));
}
#else
static inline int wma_wake_reason_nlod(t_wma_handle *wma, uint8_t vdev_id)
{
	return 0;
}
#endif /* FEATURE_WLAN_SCAN_PNO */

#ifdef WLAN_FEATURE_NAN
/**
 * wma_nan_rsp_handler_callback() - call NAN Discovery event handler
 * @handle: wma handle
 * @event: event buffer
 * @len: buffer length
 *
 * Return: 0 for success or error code
 */
static int wma_nan_rsp_handler_callback(void *handle, uint8_t *event,
					uint32_t len)
{
	return target_if_nan_rsp_handler(handle, event, len);
}
#else
static inline int wma_nan_rsp_handler_callback(void *handle, uint8_t *event,
					       uint32_t len)
{
	return 0;
}
#endif

/**
 * wma_get_snr() - get RSSI from fw
 * @psnr_req: request params
 *
 * Return: QDF status
 */
QDF_STATUS wma_get_snr(tAniGetSnrReq *psnr_req)
{
	tAniGetSnrReq *psnr_req_bkp;
	tp_wma_handle wma_handle = NULL;
	struct wma_txrx_node *intr;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		wma_err("Failed to get wma_handle");
		return QDF_STATUS_E_FAULT;
	}

	intr = &wma_handle->interfaces[psnr_req->sessionId];
	/* command is in progress */
	if (intr->psnr_req) {
		wma_err("previous snr request is pending");
		return QDF_STATUS_SUCCESS;
	}

	psnr_req_bkp = qdf_mem_malloc(sizeof(tAniGetSnrReq));
	if (!psnr_req_bkp)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_zero(psnr_req_bkp, sizeof(tAniGetSnrReq));
	psnr_req_bkp->pDevContext = psnr_req->pDevContext;
	psnr_req_bkp->snrCallback = psnr_req->snrCallback;
	intr->psnr_req = (void *)psnr_req_bkp;

	if (wmi_unified_snr_cmd(wma_handle->wmi_handle,
				 psnr_req->sessionId)) {
		wma_err("Failed to send host stats request to fw");
		qdf_mem_free(psnr_req_bkp);
		intr->psnr_req = NULL;
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void wma_get_rx_retry_cnt(struct mac_context *mac, uint8_t vdev_id,
			  uint8_t *mac_addr)
{
	struct cdp_peer_stats *peer_stats;
	QDF_STATUS status;

	peer_stats = qdf_mem_malloc(sizeof(*peer_stats));
	if (!peer_stats)
		return;

	status = cdp_host_get_peer_stats(cds_get_context(QDF_MODULE_ID_SOC),
					 vdev_id, mac_addr, peer_stats);

	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to get peer stats");
		goto exit;
	}

	mac->rx_retry_cnt = peer_stats->rx.rx_retries;
	wma_debug("Rx retry count %d, Peer" QDF_MAC_ADDR_FMT, mac->rx_retry_cnt,
		  QDF_MAC_ADDR_REF(mac_addr));

exit:
	qdf_mem_free(peer_stats);
}

/**
 * wma_process_link_status_req() - process link status request from UMAC
 * @wma: wma handle
 * @pGetLinkStatus: get link params
 *
 * Return: none
 */
void wma_process_link_status_req(tp_wma_handle wma,
				 tAniGetLinkStatus *pGetLinkStatus)
{
	struct link_status_params cmd = {0};
	struct wma_txrx_node *iface =
		&wma->interfaces[pGetLinkStatus->sessionId];

	if (iface->plink_status_req) {
		wma_err("previous link status request is pending,deleting the new request");
		qdf_mem_free(pGetLinkStatus);
		return;
	}

	iface->plink_status_req = pGetLinkStatus;
	cmd.vdev_id = pGetLinkStatus->sessionId;
	if (wmi_unified_link_status_req_cmd(wma->wmi_handle, &cmd)) {
		wma_err("Failed to send WMI link  status request to fw");
		iface->plink_status_req = NULL;
		goto end;
	}

	return;

end:
	wma_post_link_status(pGetLinkStatus, LINK_STATUS_LEGACY);
}

#ifdef WLAN_FEATURE_TSF
/**
 * wma_vdev_tsf_handler() - handle tsf event indicated by FW
 * @handle: wma context
 * @data: event buffer
 * @data len: length of event buffer
 *
 * Return: 0 on success
 */
int wma_vdev_tsf_handler(void *handle, uint8_t *data, uint32_t data_len)
{
	struct scheduler_msg tsf_msg = {0};
	WMI_VDEV_TSF_REPORT_EVENTID_param_tlvs *param_buf;
	wmi_vdev_tsf_report_event_fixed_param *tsf_event;
	struct stsf *ptsf;

	if (!data) {
		wma_err("invalid pointer");
		return -EINVAL;
	}
	ptsf = qdf_mem_malloc(sizeof(*ptsf));
	if (!ptsf)
		return -ENOMEM;

	param_buf = (WMI_VDEV_TSF_REPORT_EVENTID_param_tlvs *)data;
	tsf_event = param_buf->fixed_param;

	ptsf->vdev_id = tsf_event->vdev_id;
	ptsf->tsf_low = tsf_event->tsf_low;
	ptsf->tsf_high = tsf_event->tsf_high;
	ptsf->soc_timer_low = tsf_event->qtimer_low;
	ptsf->soc_timer_high = tsf_event->qtimer_high;
	ptsf->global_tsf_low = tsf_event->wlan_global_tsf_low;
	ptsf->global_tsf_high = tsf_event->wlan_global_tsf_high;
	wma_nofl_debug("receive WMI_VDEV_TSF_REPORT_EVENTID on %d, tsf: %d %d",
		       ptsf->vdev_id, ptsf->tsf_low, ptsf->tsf_high);

	wma_nofl_debug("g_tsf: %d %d; soc_timer: %d %d",
		       ptsf->global_tsf_low, ptsf->global_tsf_high,
			   ptsf->soc_timer_low, ptsf->soc_timer_high);
	tsf_msg.type = eWNI_SME_TSF_EVENT;
	tsf_msg.bodyptr = ptsf;
	tsf_msg.bodyval = 0;

	if (QDF_STATUS_SUCCESS !=
		scheduler_post_message(QDF_MODULE_ID_WMA,
				       QDF_MODULE_ID_SME,
				       QDF_MODULE_ID_SME, &tsf_msg)) {
		qdf_mem_free(ptsf);
		return -EINVAL;
	}
	return 0;
}

#if defined(QCA_WIFI_3_0) || defined(WLAN_FEATURE_TSF_TIMER_SYNC)
#define TSF_FW_ACTION_CMD TSF_TSTAMP_QTIMER_CAPTURE_REQ
#else
#define TSF_FW_ACTION_CMD TSF_TSTAMP_CAPTURE_REQ
#endif
/**
 * wma_capture_tsf() - send wmi to fw to capture tsf
 * @wma_handle: wma handler
 * @vdev_id: vdev id
 *
 * Return: wmi send state
 */
QDF_STATUS wma_capture_tsf(tp_wma_handle wma_handle, uint32_t vdev_id)
{
	QDF_STATUS status;
	wmi_buf_t buf;
	wmi_vdev_tsf_tstamp_action_cmd_fixed_param *cmd;
	int len = sizeof(*cmd);

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_vdev_tsf_tstamp_action_cmd_fixed_param *) wmi_buf_data(buf);
	cmd->vdev_id = vdev_id;
	cmd->tsf_action = TSF_FW_ACTION_CMD;
	wma_debug("vdev_id %u, tsf_cmd: %d", cmd->vdev_id, cmd->tsf_action);

	WMITLV_SET_HDR(&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_vdev_tsf_tstamp_action_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN(
	wmi_vdev_tsf_tstamp_action_cmd_fixed_param));

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				      WMI_VDEV_TSF_TSTAMP_ACTION_CMDID);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);

	return status;
}

/**
 * wma_reset_tsf_gpio() - send wmi to fw to reset GPIO
 * @wma_handle: wma handler
 * @vdev_id: vdev id
 *
 * Return: wmi send state
 */
QDF_STATUS wma_reset_tsf_gpio(tp_wma_handle wma_handle, uint32_t vdev_id)
{
	QDF_STATUS status;
	wmi_buf_t buf;
	wmi_vdev_tsf_tstamp_action_cmd_fixed_param *cmd;
	int len = sizeof(*cmd);
	uint8_t *buf_ptr;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *) wmi_buf_data(buf);
	cmd = (wmi_vdev_tsf_tstamp_action_cmd_fixed_param *) buf_ptr;
	cmd->vdev_id = vdev_id;
	cmd->tsf_action = TSF_TSTAMP_CAPTURE_RESET;

	wma_debug("vdev_id %u, TSF_TSTAMP_CAPTURE_RESET", cmd->vdev_id);

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_vdev_tsf_tstamp_action_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
				wmi_vdev_tsf_tstamp_action_cmd_fixed_param));

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				      WMI_VDEV_TSF_TSTAMP_ACTION_CMDID);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);

	return status;
}

/**
 * wma_set_tsf_gpio_pin() - send wmi cmd to configure gpio pin
 * @handle: wma handler
 * @pin: GPIO pin id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_set_tsf_gpio_pin(WMA_HANDLE handle, uint32_t pin)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	struct pdev_params pdev_param = {0};
	int32_t ret;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not set gpio");
		return QDF_STATUS_E_INVAL;
	}

	wma_debug("set tsf gpio pin: %d", pin);

	pdev_param.param_id = WMI_PDEV_PARAM_WNTS_CONFIG;
	pdev_param.param_value = pin;
	ret = wmi_unified_pdev_param_send(wma->wmi_handle,
					 &pdev_param,
					 WMA_WILDCARD_PDEV_ID);
	if (ret) {
		wma_err("Failed to set tsf gpio pin (status=%d)", ret);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wma_set_wisa_params(): Set WISA features related params in FW
 * @wma_handle: WMA handle
 * @wisa: Pointer to WISA param struct
 *
 * Return: CDF status
 */
QDF_STATUS wma_set_wisa_params(tp_wma_handle wma_handle,
				struct sir_wisa_params *wisa)
{
	QDF_STATUS status;
	wmi_buf_t buf;
	wmi_vdev_wisa_cmd_fixed_param *cmd;
	int len = sizeof(*cmd);

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_vdev_wisa_cmd_fixed_param *) wmi_buf_data(buf);
	cmd->wisa_mode = wisa->mode;
	cmd->vdev_id = wisa->vdev_id;

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_vdev_wisa_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
				wmi_vdev_wisa_cmd_fixed_param));

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				      WMI_VDEV_WISA_CMDID);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);

	return status;
}

/**
 * wma_process_dhcp_ind() - process dhcp indication from SME
 * @wma_handle: wma handle
 * @ta_dhcp_ind: DHCP indication
 *
 * Return: QDF Status
 */
QDF_STATUS wma_process_dhcp_ind(WMA_HANDLE handle,
				tAniDHCPInd *ta_dhcp_ind)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	uint8_t vdev_id;
	wmi_peer_set_param_cmd_fixed_param peer_set_param_fp = {0};

	if (!wma_handle) {
		wma_err("wma_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!ta_dhcp_ind) {
		wma_err("DHCP indication is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (wma_find_vdev_id_by_addr(wma_handle,
				     ta_dhcp_ind->adapterMacAddr.bytes,
				     &vdev_id)) {
		wma_err("Failed to find vdev id for DHCP indication");
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("WMA --> WMI_PEER_SET_PARAM triggered by DHCP, msgType=%s, device_mode=%d, macAddr=" QDF_MAC_ADDR_FMT,
		ta_dhcp_ind->msgType == WMA_DHCP_START_IND ?
		"WMA_DHCP_START_IND" : "WMA_DHCP_STOP_IND",
		ta_dhcp_ind->device_mode,
		QDF_MAC_ADDR_REF(ta_dhcp_ind->peerMacAddr.bytes));

	/* fill in values */
	peer_set_param_fp.vdev_id = vdev_id;
	peer_set_param_fp.param_id = WMI_PEER_CRIT_PROTO_HINT_ENABLED;
	if (WMA_DHCP_START_IND == ta_dhcp_ind->msgType)
		peer_set_param_fp.param_value = 1;
	else
		peer_set_param_fp.param_value = 0;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(ta_dhcp_ind->peerMacAddr.bytes,
				   &peer_set_param_fp.peer_macaddr);

	return wmi_unified_process_dhcp_ind(wma_handle->wmi_handle,
					    &peer_set_param_fp);
}

enum wlan_phymode wma_chan_phy_mode(uint32_t freq, enum phy_ch_width chan_width,
				    uint8_t dot11_mode)
{
	enum wlan_phymode phymode = WLAN_PHYMODE_AUTO;
	uint16_t bw_val = wlan_reg_get_bw_value(chan_width);
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err("wma_handle is NULL");
		return WLAN_PHYMODE_AUTO;
	}

	if (chan_width >= CH_WIDTH_INVALID) {
		wma_err_rl("Invalid channel width %d", chan_width);
		return WLAN_PHYMODE_AUTO;
	}

	if (wlan_reg_is_24ghz_ch_freq(freq)) {
		if (((CH_WIDTH_5MHZ == chan_width) ||
		     (CH_WIDTH_10MHZ == chan_width)) &&
		    ((MLME_DOT11_MODE_11B == dot11_mode) ||
		     (MLME_DOT11_MODE_11G == dot11_mode) ||
		     (MLME_DOT11_MODE_11N == dot11_mode) ||
		     (MLME_DOT11_MODE_ALL == dot11_mode) ||
		     (MLME_DOT11_MODE_11AC == dot11_mode) ||
		     (MLME_DOT11_MODE_11AX == dot11_mode)))
			phymode = WLAN_PHYMODE_11G;
		else {
			switch (dot11_mode) {
			case MLME_DOT11_MODE_11B:
				if ((bw_val == 20) || (bw_val == 40))
					phymode = WLAN_PHYMODE_11B;
				break;
			case MLME_DOT11_MODE_11G:
				if ((bw_val == 20) || (bw_val == 40))
					phymode = WLAN_PHYMODE_11G;
				break;
			case MLME_DOT11_MODE_11G_ONLY:
				if ((bw_val == 20) || (bw_val == 40))
					phymode = WLAN_PHYMODE_11G_ONLY;
				break;
			case MLME_DOT11_MODE_11N:
			case MLME_DOT11_MODE_11N_ONLY:
				if (bw_val == 20)
					phymode = WLAN_PHYMODE_11NG_HT20;
				else if (bw_val == 40)
					phymode = WLAN_PHYMODE_11NG_HT40;
				break;
			case MLME_DOT11_MODE_ALL:
			case MLME_DOT11_MODE_11AC:
			case MLME_DOT11_MODE_11AC_ONLY:
				if (bw_val == 20)
					phymode = WLAN_PHYMODE_11AC_VHT20_2G;
				else if (bw_val == 40)
					phymode = WLAN_PHYMODE_11AC_VHT40_2G;
				break;
			case MLME_DOT11_MODE_11AX:
			case MLME_DOT11_MODE_11AX_ONLY:
				if (20 == bw_val)
					phymode = WLAN_PHYMODE_11AXG_HE20;
				else if (40 == bw_val)
					phymode = WLAN_PHYMODE_11AXG_HE40;
				break;
			default:
				break;
			}
		}
	} else if (wlan_reg_is_dsrc_freq(freq))
		phymode = WLAN_PHYMODE_11A;
	else {
		if (((CH_WIDTH_5MHZ == chan_width) ||
		     (CH_WIDTH_10MHZ == chan_width)) &&
		    ((MLME_DOT11_MODE_11A == dot11_mode) ||
		     (MLME_DOT11_MODE_11N == dot11_mode) ||
		     (MLME_DOT11_MODE_ALL == dot11_mode) ||
		     (MLME_DOT11_MODE_11AC == dot11_mode) ||
		     (MLME_DOT11_MODE_11AX == dot11_mode)))
			phymode = WLAN_PHYMODE_11A;
		else {
			switch (dot11_mode) {
			case MLME_DOT11_MODE_11A:
				if (0 < bw_val)
					phymode = WLAN_PHYMODE_11A;
				break;
			case MLME_DOT11_MODE_11N:
			case MLME_DOT11_MODE_11N_ONLY:
				if (bw_val == 20)
					phymode = WLAN_PHYMODE_11NA_HT20;
				else if (40 <= bw_val)
					phymode = WLAN_PHYMODE_11NA_HT40;
				break;
			case MLME_DOT11_MODE_ALL:
			case MLME_DOT11_MODE_11AC:
			case MLME_DOT11_MODE_11AC_ONLY:
				if (bw_val == 20)
					phymode = WLAN_PHYMODE_11AC_VHT20;
				else if (bw_val == 40)
					phymode = WLAN_PHYMODE_11AC_VHT40;
				else if (bw_val == 80)
					phymode = WLAN_PHYMODE_11AC_VHT80;
				else if (chan_width == CH_WIDTH_160MHZ)
					phymode = WLAN_PHYMODE_11AC_VHT160;
				else if (chan_width == CH_WIDTH_80P80MHZ)
					phymode = WLAN_PHYMODE_11AC_VHT80_80;
				break;
			case MLME_DOT11_MODE_11AX:
			case MLME_DOT11_MODE_11AX_ONLY:
				if (20 == bw_val)
					phymode = WLAN_PHYMODE_11AXA_HE20;
				else if (40 == bw_val)
					phymode = WLAN_PHYMODE_11AXA_HE40;
				else if (80 == bw_val)
					phymode = WLAN_PHYMODE_11AXA_HE80;
				else if (CH_WIDTH_160MHZ == chan_width)
					phymode = WLAN_PHYMODE_11AXA_HE160;
				else if (CH_WIDTH_80P80MHZ == chan_width)
					phymode = WLAN_PHYMODE_11AXA_HE80_80;
				break;
			default:
				break;
			}
		}
	}

	wma_debug("phymode %d freq %d ch_width %d dot11_mode %d",
		 phymode, freq, chan_width, dot11_mode);

	QDF_ASSERT(phymode != WLAN_PHYMODE_AUTO);
	return phymode;
}

/**
 * wma_get_link_speed() -send command to get linkspeed
 * @handle: wma handle
 * @pLinkSpeed: link speed info
 *
 * Return: QDF status
 */
QDF_STATUS wma_get_link_speed(WMA_HANDLE handle,
			      struct link_speed_info *pLinkSpeed)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_mac_addr peer_macaddr;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, can not issue get link speed cmd");
		return QDF_STATUS_E_INVAL;
	}
	if (!wmi_service_enabled(wma_handle->wmi_handle,
				    wmi_service_estimate_linkspeed)) {
		wma_err("Linkspeed feature bit not enabled Sending value 0 as link speed");
		wma_send_link_speed(0);
		return QDF_STATUS_E_FAILURE;
	}
	/* Copy the peer macaddress to the wma buffer */
	WMI_CHAR_ARRAY_TO_MAC_ADDR(pLinkSpeed->peer_macaddr.bytes,
				   &peer_macaddr);
	wma_debug("pLinkSpeed->peerMacAddr: "QDF_MAC_ADDR_FMT", peer_macaddr.mac_addr31to0: 0x%x, peer_macaddr.mac_addr47to32: 0x%x",
		 QDF_MAC_ADDR_REF(pLinkSpeed->peer_macaddr.bytes),
		 peer_macaddr.mac_addr31to0,
		 peer_macaddr.mac_addr47to32);
	if (wmi_unified_get_link_speed_cmd(wma_handle->wmi_handle,
					peer_macaddr)) {
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_get_isolation(tp_wma_handle wma)
{
	wmi_coex_get_antenna_isolation_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t  len;
	uint8_t *buf_ptr;

	wma_debug("get isolation");

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue get isolation");
		return QDF_STATUS_E_INVAL;
	}

	len  = sizeof(wmi_coex_get_antenna_isolation_cmd_fixed_param);
	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf) {
		wma_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}
	buf_ptr = (uint8_t *)wmi_buf_data(wmi_buf);

	cmd = (wmi_coex_get_antenna_isolation_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(
	&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_coex_get_antenna_isolation_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN(
	wmi_coex_get_antenna_isolation_cmd_fixed_param));

	if (wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
				 WMI_COEX_GET_ANTENNA_ISOLATION_CMDID)) {
		wma_err("Failed to get isolation request from fw");
		wmi_buf_free(wmi_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_add_beacon_filter() - Issue WMI command to set beacon filter
 * @wma: wma handler
 * @filter_params: beacon_filter_param to set
 *
 * Return: Return QDF_STATUS
 */
QDF_STATUS wma_add_beacon_filter(WMA_HANDLE handle,
				struct beacon_filter_param *filter_params)
{
	int i;
	wmi_buf_t wmi_buf;
	u_int8_t *buf;
	A_UINT32 *ie_map;
	int ret;
	struct wma_txrx_node *iface;
	tp_wma_handle wma = (tp_wma_handle) handle;

	wmi_add_bcn_filter_cmd_fixed_param *cmd;
	int len = sizeof(wmi_add_bcn_filter_cmd_fixed_param);

	len += WMI_TLV_HDR_SIZE;
	len += BCN_FLT_MAX_ELEMS_IE_LIST*sizeof(A_UINT32);

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue set beacon filter");
		return QDF_STATUS_E_INVAL;
	}

	iface = &wma->interfaces[filter_params->vdev_id];
	qdf_mem_copy(&iface->beacon_filter, filter_params,
			sizeof(struct beacon_filter_param));
	iface->beacon_filter_enabled = true;

	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf)
		return QDF_STATUS_E_NOMEM;

	buf = (u_int8_t *) wmi_buf_data(wmi_buf);

	cmd = (wmi_add_bcn_filter_cmd_fixed_param *)wmi_buf_data(wmi_buf);
	cmd->vdev_id = filter_params->vdev_id;

	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_add_bcn_filter_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_add_bcn_filter_cmd_fixed_param));

	buf += sizeof(wmi_add_bcn_filter_cmd_fixed_param);

	WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_UINT32,
			(BCN_FLT_MAX_ELEMS_IE_LIST * sizeof(u_int32_t)));

	ie_map = (A_UINT32 *)(buf + WMI_TLV_HDR_SIZE);
	for (i = 0; i < BCN_FLT_MAX_ELEMS_IE_LIST; i++) {
		ie_map[i] = filter_params->ie_map[i];
		wma_debug("beacon filter ie map = %u", ie_map[i]);
	}

	ret = wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
			WMI_ADD_BCN_FILTER_CMDID);
	if (ret) {
		wmi_buf_free(wmi_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
* wma_remove_beacon_filter() - Issue WMI command to remove beacon filter
* @wma: wma handler
* @filter_params: beacon_filter_params
*
* Return: Return QDF_STATUS
*/
QDF_STATUS wma_remove_beacon_filter(WMA_HANDLE handle,
				struct beacon_filter_param *filter_params)
{
	wmi_buf_t buf;
	tp_wma_handle wma = (tp_wma_handle) handle;
	wmi_rmv_bcn_filter_cmd_fixed_param *cmd;
	int len = sizeof(wmi_rmv_bcn_filter_cmd_fixed_param);
	int ret;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, cannot issue remove beacon filter");
		return QDF_STATUS_E_INVAL;
	}

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_rmv_bcn_filter_cmd_fixed_param *)wmi_buf_data(buf);
	cmd->vdev_id = filter_params->vdev_id;

	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_rmv_bcn_filter_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_rmv_bcn_filter_cmd_fixed_param));

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
			WMI_RMV_BCN_FILTER_CMDID);
	if (ret) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_send_adapt_dwelltime_params() - send adaptive dwelltime configuration
 * params to firmware
 * @wma_handle:	 wma handler
 * @dwelltime_params: pointer to dwelltime_params
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF failure reason code for failure
 */
QDF_STATUS wma_send_adapt_dwelltime_params(WMA_HANDLE handle,
			struct adaptive_dwelltime_params *dwelltime_params)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct wmi_adaptive_dwelltime_params wmi_param = {0};
	int32_t err;

	wmi_param.is_enabled = dwelltime_params->is_enabled;
	wmi_param.dwelltime_mode = dwelltime_params->dwelltime_mode;
	wmi_param.lpf_weight = dwelltime_params->lpf_weight;
	wmi_param.passive_mon_intval = dwelltime_params->passive_mon_intval;
	wmi_param.wifi_act_threshold = dwelltime_params->wifi_act_threshold;
	err = wmi_unified_send_adapt_dwelltime_params_cmd(wma_handle->
					wmi_handle, &wmi_param);
	if (err)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_send_dbs_scan_selection_params(WMA_HANDLE handle,
			struct wmi_dbs_scan_sel_params *dbs_scan_params)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	int32_t err;

	err = wmi_unified_send_dbs_scan_sel_params_cmd(wma_handle->
					wmi_handle, dbs_scan_params);
	if (err)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_unified_fw_profiling_cmd() - send FW profiling cmd to WLAN FW
 * @wma: wma handle
 * @cmd: Profiling command index
 * @value1: parameter1 value
 * @value2: parameter2 value
 *
 * Return: 0 for success else error code
 */
QDF_STATUS wma_unified_fw_profiling_cmd(wmi_unified_t wmi_handle,
			uint32_t cmd, uint32_t value1, uint32_t value2)
{
	int ret;

	ret = wmi_unified_fw_profiling_data_cmd(wmi_handle, cmd,
			value1, value2);
	if (ret) {
		wma_err("enable cmd Failed for id %d value %d",	value1, value2);
		return ret;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_wow_set_wake_time() - set timer pattern tlv, so that firmware will wake
 * up host after specified time is elapsed
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 * @cookie: value to identify reason why host set up wake call.
 * @time: time in ms
 *
 * Return: QDF status
 */
static QDF_STATUS wma_wow_set_wake_time(WMA_HANDLE wma_handle, uint8_t vdev_id,
					uint32_t cookie, uint32_t time)
{
	int ret;
	tp_wma_handle wma = (tp_wma_handle)wma_handle;

	wma_debug("send timer patter with time: %d and vdev = %d to fw",
		 time, vdev_id);
	ret = wmi_unified_wow_timer_pattern_cmd(wma->wmi_handle, vdev_id,
						cookie, time);
	if (ret) {
		wma_err("Failed to send timer patter to fw");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD
/**
 * wma_check_and_set_wake_timer(): checks all interfaces and if any interface
 * has install_key pending, sets timer pattern in fw to wake up host after
 * specified time has elapsed.
 * @wma: wma handle
 * @time: time after which host wants to be awaken.
 *
 * Return: None
 */
void wma_check_and_set_wake_timer(uint32_t time)
{
	int i;
	struct wma_txrx_node *iface;
	bool is_set_key_in_progress = false;
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err("WMA is closed");
		return;
	}

	if (!wmi_service_enabled(wma->wmi_handle,
		wmi_service_wow_wakeup_by_timer_pattern)) {
		wma_debug("TIME_PATTERN is not enabled");
		return;
	}

	for (i = 0; i < wma->max_bssid; i++) {
		iface = &wma->interfaces[i];
		if (iface->vdev_active && iface->is_waiting_for_key) {
			/*
			 * right now cookie is dont care, since FW disregards
			 * that.
			 */
			is_set_key_in_progress = true;
			wma_wow_set_wake_time((WMA_HANDLE)wma, i, 0, time);
			break;
		}
	}

	if (!is_set_key_in_progress)
		wma_debug("set key not in progress for any vdev");
}

/**
 * wma_unified_csa_offload_enable() - sen CSA offload enable command
 * @wma: wma handle
 * @vdev_id: vdev id
 *
 * Return: 0 for success or error code
 */
int wma_unified_csa_offload_enable(tp_wma_handle wma, uint8_t vdev_id)
{
	if (wmi_unified_csa_offload_enable(wma->wmi_handle,
				 vdev_id)) {
		wma_alert("Failed to send CSA offload enable command");
		return -EIO;
	}

	return 0;
}
#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

static uint8_t *
wma_parse_ch_switch_wrapper_ie(uint8_t *ch_wr_ie, uint8_t sub_ele_id)
{
	uint8_t len = 0, sub_ele_len = 0;
	struct ie_header *ele;

	ele = (struct ie_header *)ch_wr_ie;
	if (ele->ie_id != WLAN_ELEMID_CHAN_SWITCH_WRAP ||
	    ele->ie_len == 0)
		return NULL;

	len = ele->ie_len;
	ele = (struct ie_header *)(ch_wr_ie + sizeof(struct ie_header));

	while (len > 0) {
		sub_ele_len = sizeof(struct ie_header) + ele->ie_len;
		len -= sub_ele_len;
		if (ele->ie_id == sub_ele_id)
			return (uint8_t *)ele;

		ele = (struct ie_header *)((uint8_t *)ele + sub_ele_len);
	}

	return NULL;
}

/**
 * wma_csa_offload_handler() - CSA event handler
 * @handle: wma handle
 * @event: event buffer
 * @len: buffer length
 *
 * This event is sent by firmware when it receives CSA IE.
 *
 * Return: 0 for success or error code
 */
int wma_csa_offload_handler(void *handle, uint8_t *event, uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_CSA_HANDLING_EVENTID_param_tlvs *param_buf;
	wmi_csa_event_fixed_param *csa_event;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	uint8_t vdev_id = 0;
	uint8_t cur_chan = 0;
	struct ieee80211_channelswitch_ie *csa_ie;
	struct csa_offload_params *csa_offload_event;
	struct ieee80211_extendedchannelswitch_ie *xcsa_ie;
	struct ieee80211_ie_wide_bw_switch *wb_ie;
	struct wma_txrx_node *intr = wma->interfaces;

	param_buf = (WMI_CSA_HANDLING_EVENTID_param_tlvs *) event;

	wma_debug("Enter");
	if (!param_buf) {
		wma_err("Invalid csa event buffer");
		return -EINVAL;
	}
	csa_event = param_buf->fixed_param;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&csa_event->i_addr2, &bssid[0]);

	if (wma_find_vdev_id_by_bssid(wma, bssid, &vdev_id)) {
		wma_err("Invalid bssid received");
		return -EINVAL;
	}

	csa_offload_event = qdf_mem_malloc(sizeof(*csa_offload_event));
	if (!csa_offload_event)
		return -EINVAL;

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(wma->psoc, vdev_id) ||
	    wma->interfaces[vdev_id].roaming_in_progress) {
		wma_err("Roaming in progress for vdev %d, ignore csa event",
			 vdev_id);
		qdf_mem_free(csa_offload_event);
		return -EINVAL;
	}

	qdf_mem_zero(csa_offload_event, sizeof(*csa_offload_event));
	qdf_mem_copy(csa_offload_event->bssId, &bssid, QDF_MAC_ADDR_SIZE);

	if (csa_event->ies_present_flag & WMI_CSA_IE_PRESENT) {
		csa_ie = (struct ieee80211_channelswitch_ie *)
						(&csa_event->csa_ie[0]);
		csa_offload_event->channel = csa_ie->newchannel;
		csa_offload_event->csa_chan_freq =
			wlan_reg_legacy_chan_to_freq(wma->pdev,
						     csa_ie->newchannel);
		csa_offload_event->switch_mode = csa_ie->switchmode;
	} else if (csa_event->ies_present_flag & WMI_XCSA_IE_PRESENT) {
		xcsa_ie = (struct ieee80211_extendedchannelswitch_ie *)
						(&csa_event->xcsa_ie[0]);
		csa_offload_event->channel = xcsa_ie->newchannel;
		csa_offload_event->switch_mode = xcsa_ie->switchmode;
		csa_offload_event->new_op_class = xcsa_ie->newClass;
		if (wlan_reg_is_6ghz_op_class(wma->pdev, xcsa_ie->newClass)) {
			csa_offload_event->csa_chan_freq =
				wlan_reg_chan_band_to_freq
					(wma->pdev, xcsa_ie->newchannel,
					 BIT(REG_BAND_6G));
		} else {
			csa_offload_event->csa_chan_freq =
				wlan_reg_legacy_chan_to_freq
					(wma->pdev, xcsa_ie->newchannel);
		}
	} else {
		wma_err("CSA Event error: No CSA IE present");
		qdf_mem_free(csa_offload_event);
		return -EINVAL;
	}

	if (csa_event->ies_present_flag & WMI_WBW_IE_PRESENT) {
		wb_ie = (struct ieee80211_ie_wide_bw_switch *)
						(&csa_event->wb_ie[0]);
		csa_offload_event->new_ch_width = wb_ie->new_ch_width;
		csa_offload_event->new_ch_freq_seg1 = wb_ie->new_ch_freq_seg1;
		csa_offload_event->new_ch_freq_seg2 = wb_ie->new_ch_freq_seg2;
	} else if (csa_event->ies_present_flag &
		   WMI_CSWRAP_IE_EXTENDED_PRESENT) {
		wb_ie = (struct ieee80211_ie_wide_bw_switch *)
				wma_parse_ch_switch_wrapper_ie(
				(uint8_t *)&csa_event->cswrap_ie_extended,
				WLAN_ELEMID_WIDE_BAND_CHAN_SWITCH);
		if (wb_ie) {
			csa_offload_event->new_ch_width = wb_ie->new_ch_width;
			csa_offload_event->new_ch_freq_seg1 =
						wb_ie->new_ch_freq_seg1;
			csa_offload_event->new_ch_freq_seg2 =
						wb_ie->new_ch_freq_seg2;
			csa_event->ies_present_flag |= WMI_WBW_IE_PRESENT;
		}
	}

	csa_offload_event->ies_present_flag = csa_event->ies_present_flag;

	wma_debug("CSA: BSSID "QDF_MAC_ADDR_FMT" chan %d freq %d flag 0x%x width = %d freq1 = %d freq2 = %d op class = %d",
		 QDF_MAC_ADDR_REF(csa_offload_event->bssId),
		 csa_offload_event->channel,
		 csa_offload_event->csa_chan_freq,
		 csa_event->ies_present_flag,
		 csa_offload_event->new_ch_width,
		 csa_offload_event->new_ch_freq_seg1,
		 csa_offload_event->new_ch_freq_seg2,
		 csa_offload_event->new_op_class);

	cur_chan = cds_freq_to_chan(intr[vdev_id].ch_freq);
	/*
	 * basic sanity check: requested channel should not be 0
	 * and equal to home channel
	 */
	if (0 == csa_offload_event->channel) {
		wma_err("CSA Event with channel %d. Ignore !!",
			 csa_offload_event->channel);
		qdf_mem_free(csa_offload_event);
		return -EINVAL;
	}

	wma_send_msg(wma, WMA_CSA_OFFLOAD_EVENT, (void *)csa_offload_event, 0);
	return 0;
}

#ifdef FEATURE_OEM_DATA_SUPPORT
/**
 * wma_oem_data_response_handler() - OEM data response event handler
 * @handle: wma handle
 * @datap: data ptr
 * @len: data length
 *
 * Return: 0 for success or error code
 */
int wma_oem_data_response_handler(void *handle,
				  uint8_t *datap, uint32_t len)
{
	WMI_OEM_RESPONSE_EVENTID_param_tlvs *param_buf;
	uint8_t *data;
	uint32_t datalen;
	struct oem_data_rsp *oem_rsp;
	struct mac_context *pmac = cds_get_context(QDF_MODULE_ID_PE);

	if (!pmac) {
		wma_err("Invalid pmac");
		return -EINVAL;
	}

	if (!pmac->sme.oem_data_rsp_callback) {
		wma_err("Callback not registered");
		return -EINVAL;
	}

	param_buf = (WMI_OEM_RESPONSE_EVENTID_param_tlvs *) datap;
	if (!param_buf) {
		wma_err("Received NULL buf ptr from FW");
		return -ENOMEM;
	}

	data = param_buf->data;
	datalen = param_buf->num_data;

	if (!data) {
		wma_err("Received NULL data from FW");
		return -EINVAL;
	}

	if (datalen > OEM_DATA_RSP_SIZE) {
		wma_err("Received data len %d exceeds max value %d",
			 datalen, OEM_DATA_RSP_SIZE);
		return -EINVAL;
	}

	oem_rsp = qdf_mem_malloc(sizeof(*oem_rsp));
	if (!oem_rsp)
		return -ENOMEM;

	oem_rsp->rsp_len = datalen;
	if (oem_rsp->rsp_len) {
		oem_rsp->data = qdf_mem_malloc(oem_rsp->rsp_len);
		if (!oem_rsp->data) {
			qdf_mem_free(oem_rsp);
			return -ENOMEM;
		}
	} else {
		wma_err("Invalid rsp length: %d", oem_rsp->rsp_len);
		qdf_mem_free(oem_rsp);
		return -EINVAL;
	}

	qdf_mem_copy(oem_rsp->data, data, datalen);

	wma_debug("Sending OEM_DATA_RSP(len: %d) to upper layer", datalen);

	pmac->sme.oem_data_rsp_callback(oem_rsp);

	if (oem_rsp->data)
		qdf_mem_free(oem_rsp->data);
	qdf_mem_free(oem_rsp);

	return 0;
}

QDF_STATUS wma_start_oem_req_cmd(tp_wma_handle wma_handle,
				 struct oem_data_req *oem_data_req)
{
	QDF_STATUS ret;

	wma_debug("Send OEM Data Request to target");

	if (!oem_data_req || !oem_data_req->data) {
		wma_err("oem_data_req is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA - closed, can not send Oem data request cmd");
		qdf_mem_free(oem_data_req->data);
		return QDF_STATUS_E_INVAL;
	}

	/* legacy api, for oem data request case */
	ret = wmi_unified_start_oem_data_cmd(wma_handle->wmi_handle,
					     oem_data_req->data_len,
					     oem_data_req->data);

	if (!QDF_IS_STATUS_SUCCESS(ret))
		wma_err("wmi cmd send failed");

	return ret;
}
#endif /* FEATURE_OEM_DATA_SUPPORT */

#ifdef FEATURE_OEM_DATA
QDF_STATUS wma_start_oem_data_cmd(tp_wma_handle wma_handle,
				  struct oem_data *oem_data)
{
	QDF_STATUS ret;

	wma_debug("Send OEM Data to target");

	if (!oem_data || !oem_data->data) {
		wma_err("oem_data is null");
		return QDF_STATUS_E_INVAL;
	}

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA - closed");
		return QDF_STATUS_E_INVAL;
	}

	/* common api, for oem data command case */
	ret = wmi_unified_start_oemv2_data_cmd(wma_handle->wmi_handle,
					       oem_data);
	if (!QDF_IS_STATUS_SUCCESS(ret))
		wma_err("call start wmi cmd failed");

	return ret;
}
#endif

#if !defined(REMOVE_PKT_LOG) && defined(FEATURE_PKTLOG)
/**
 * wma_pktlog_wmi_send_cmd() - send pktlog enable/disable command to target
 * @handle: wma handle
 * @params: pktlog params
 *
 * Return: QDF status
 */
QDF_STATUS wma_pktlog_wmi_send_cmd(WMA_HANDLE handle,
				   struct ath_pktlog_wmi_params *params)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	int ret;

	ret = wmi_unified_pktlog_wmi_send_cmd(wma_handle->wmi_handle,
			params->pktlog_event,
			params->cmd_id, params->user_triggered);
	if (ret)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}
#endif /* !REMOVE_PKT_LOG && FEATURE_PKTLOG */

/**
 * wma_wow_wake_reason_str() -  Converts wow wakeup reason code to text format
 * @wake_reason - WOW wake reason
 *
 * Return: reason code in string format
 */
static const uint8_t *wma_wow_wake_reason_str(A_INT32 wake_reason)
{
	switch (wake_reason) {
	case WOW_REASON_UNSPECIFIED:
		return "UNSPECIFIED";
	case WOW_REASON_NLOD:
		return "NLOD";
	case WOW_REASON_AP_ASSOC_LOST:
		return "AP_ASSOC_LOST";
	case WOW_REASON_LOW_RSSI:
		return "LOW_RSSI";
	case WOW_REASON_DEAUTH_RECVD:
		return "DEAUTH_RECVD";
	case WOW_REASON_DISASSOC_RECVD:
		return "DISASSOC_RECVD";
	case WOW_REASON_GTK_HS_ERR:
		return "GTK_HS_ERR";
	case WOW_REASON_EAP_REQ:
		return "EAP_REQ";
	case WOW_REASON_FOURWAY_HS_RECV:
		return "FOURWAY_HS_RECV";
	case WOW_REASON_TIMER_INTR_RECV:
		return "TIMER_INTR_RECV";
	case WOW_REASON_PATTERN_MATCH_FOUND:
		return "PATTERN_MATCH_FOUND";
	case WOW_REASON_RECV_MAGIC_PATTERN:
		return "RECV_MAGIC_PATTERN";
	case WOW_REASON_P2P_DISC:
		return "P2P_DISC";
	case WOW_REASON_WLAN_HB:
		return "WLAN_HB";
	case WOW_REASON_CSA_EVENT:
		return "CSA_EVENT";
	case WOW_REASON_PROBE_REQ_WPS_IE_RECV:
		return "PROBE_REQ_WPS_IE_RECV";
	case WOW_REASON_AUTH_REQ_RECV:
		return "AUTH_REQ_RECV";
	case WOW_REASON_ASSOC_REQ_RECV:
		return "ASSOC_REQ_RECV";
	case WOW_REASON_HTT_EVENT:
		return "HTT_EVENT";
	case WOW_REASON_RA_MATCH:
		return "RA_MATCH";
	case WOW_REASON_HOST_AUTO_SHUTDOWN:
		return "HOST_AUTO_SHUTDOWN";
	case WOW_REASON_IOAC_MAGIC_EVENT:
		return "IOAC_MAGIC_EVENT";
	case WOW_REASON_IOAC_SHORT_EVENT:
		return "IOAC_SHORT_EVENT";
	case WOW_REASON_IOAC_EXTEND_EVENT:
		return "IOAC_EXTEND_EVENT";
	case WOW_REASON_IOAC_TIMER_EVENT:
		return "IOAC_TIMER_EVENT";
	case WOW_REASON_ROAM_HO:
		return "ROAM_HO";
	case WOW_REASON_ROAM_PREAUTH_START:
		return "ROAM_PREAUTH_START_EVENT";
	case WOW_REASON_DFS_PHYERR_RADADR_EVENT:
		return "DFS_PHYERR_RADADR_EVENT";
	case WOW_REASON_BEACON_RECV:
		return "BEACON_RECV";
	case WOW_REASON_CLIENT_KICKOUT_EVENT:
		return "CLIENT_KICKOUT_EVENT";
	case WOW_REASON_NAN_EVENT:
		return "NAN_EVENT";
	case WOW_REASON_EXTSCAN:
		return "EXTSCAN";
	case WOW_REASON_RSSI_BREACH_EVENT:
		return "RSSI_BREACH_EVENT";
	case WOW_REASON_IOAC_REV_KA_FAIL_EVENT:
		return "IOAC_REV_KA_FAIL_EVENT";
	case WOW_REASON_IOAC_SOCK_EVENT:
		return "IOAC_SOCK_EVENT";
	case WOW_REASON_NLO_SCAN_COMPLETE:
		return "NLO_SCAN_COMPLETE";
	case WOW_REASON_PACKET_FILTER_MATCH:
		return "PACKET_FILTER_MATCH";
	case WOW_REASON_ASSOC_RES_RECV:
		return "ASSOC_RES_RECV";
	case WOW_REASON_REASSOC_REQ_RECV:
		return "REASSOC_REQ_RECV";
	case WOW_REASON_REASSOC_RES_RECV:
		return "REASSOC_RES_RECV";
	case WOW_REASON_ACTION_FRAME_RECV:
		return "ACTION_FRAME_RECV";
	case WOW_REASON_BPF_ALLOW:
		return "BPF_ALLOW";
	case WOW_REASON_NAN_DATA:
		return "NAN_DATA";
	case WOW_REASON_OEM_RESPONSE_EVENT:
		return "OEM_RESPONSE_EVENT";
	case WOW_REASON_TDLS_CONN_TRACKER_EVENT:
		return "TDLS_CONN_TRACKER_EVENT";
	case WOW_REASON_CRITICAL_LOG:
		return "CRITICAL_LOG";
	case WOW_REASON_P2P_LISTEN_OFFLOAD:
		return "P2P_LISTEN_OFFLOAD";
	case WOW_REASON_NAN_EVENT_WAKE_HOST:
		return "NAN_EVENT_WAKE_HOST";
	case WOW_REASON_DEBUG_TEST:
		return "DEBUG_TEST";
	case WOW_REASON_CHIP_POWER_FAILURE_DETECT:
		return "CHIP_POWER_FAILURE_DETECT";
	case WOW_REASON_11D_SCAN:
		return "11D_SCAN";
	case WOW_REASON_SAP_OBSS_DETECTION:
		return "SAP_OBSS_DETECTION";
	case WOW_REASON_BSS_COLOR_COLLISION_DETECT:
		return "BSS_COLOR_COLLISION_DETECT";
#ifdef WLAN_FEATURE_MOTION_DETECTION
	case WOW_REASON_WLAN_MD:
		return "MOTION_DETECT";
	case WOW_REASON_WLAN_BL:
		return "MOTION_DETECT_BASELINE";
#endif /* WLAN_FEATURE_MOTION_DETECTION */
	case WOW_REASON_PAGE_FAULT:
		return "PAGE_FAULT";
	case WOW_REASON_ROAM_PMKID_REQUEST:
		return "ROAM_PMKID_REQUEST";
	case WOW_REASON_VDEV_DISCONNECT:
		return "VDEV_DISCONNECT";
	case WOW_REASON_LOCAL_DATA_UC_DROP:
		return "LOCAL_DATA_UC_DROP";
	case WOW_REASON_FATAL_EVENT_WAKE:
		return "FATAL_EVENT_WAKE";
	case WOW_REASON_GENERIC_WAKE:
		return "GENERIC_WAKE";
	case WOW_REASON_TWT:
		return "TWT Event";
	case WOW_REASON_ROAM_STATS:
		return "ROAM_STATS";
	default:
		return "unknown";
	}
}

static bool wma_wow_reason_has_stats(enum wake_reason_e reason)
{
	switch (reason) {
	case WOW_REASON_ASSOC_REQ_RECV:
	case WOW_REASON_DISASSOC_RECVD:
	case WOW_REASON_ASSOC_RES_RECV:
	case WOW_REASON_REASSOC_REQ_RECV:
	case WOW_REASON_REASSOC_RES_RECV:
	case WOW_REASON_AUTH_REQ_RECV:
	case WOW_REASON_DEAUTH_RECVD:
	case WOW_REASON_ACTION_FRAME_RECV:
	case WOW_REASON_BPF_ALLOW:
	case WOW_REASON_PATTERN_MATCH_FOUND:
	case WOW_REASON_PACKET_FILTER_MATCH:
	case WOW_REASON_RA_MATCH:
	case WOW_REASON_NLOD:
	case WOW_REASON_NLO_SCAN_COMPLETE:
	case WOW_REASON_LOW_RSSI:
	case WOW_REASON_EXTSCAN:
	case WOW_REASON_RSSI_BREACH_EVENT:
	case WOW_REASON_OEM_RESPONSE_EVENT:
	case WOW_REASON_CHIP_POWER_FAILURE_DETECT:
	case WOW_REASON_11D_SCAN:
	case WOW_REASON_LOCAL_DATA_UC_DROP:
	case WOW_REASON_FATAL_EVENT_WAKE:
		return true;
#ifdef WLAN_FEATURE_MOTION_DETECTION
	case WOW_REASON_WLAN_MD:
	case WOW_REASON_WLAN_BL:
		return true;
#endif /* WLAN_FEATURE_MOTION_DETECTION */
	default:
		return false;
	}
}

static void wma_inc_wow_stats(t_wma_handle *wma,
			      WOW_EVENT_INFO_fixed_param *wake_info)
{
	ucfg_mc_cp_stats_inc_wake_lock_stats(wma->psoc,
					     wake_info->vdev_id,
					     wake_info->wake_reason);
}

static void wma_wow_stats_display(struct wake_lock_stats *stats)
{
	wma_conditional_log(is_wakeup_event_console_logs_enabled,
			    "WLAN wake reason counters:");
	wma_conditional_log(is_wakeup_event_console_logs_enabled,
			    "uc:%d bc:%d v4_mc:%d v6_mc:%d ra:%d ns:%d na:%d "
			    "icmp:%d icmpv6:%d",
			    stats->ucast_wake_up_count,
			    stats->bcast_wake_up_count,
			    stats->ipv4_mcast_wake_up_count,
			    stats->ipv6_mcast_wake_up_count,
			    stats->ipv6_mcast_ra_stats,
			    stats->ipv6_mcast_ns_stats,
			    stats->ipv6_mcast_na_stats,
			    stats->icmpv4_count,
			    stats->icmpv6_count);

	wma_conditional_log(is_wakeup_event_console_logs_enabled,
			    "assoc:%d disassoc:%d assoc_resp:%d reassoc:%d "
			    "reassoc_resp:%d auth:%d deauth:%d action:%d",
			    stats->mgmt_assoc,
			    stats->mgmt_disassoc,
			    stats->mgmt_assoc_resp,
			    stats->mgmt_reassoc,
			    stats->mgmt_reassoc_resp,
			    stats->mgmt_auth,
			    stats->mgmt_deauth,
			    stats->mgmt_action);

	wma_conditional_log(is_wakeup_event_console_logs_enabled,
			    "pno_match:%d pno_complete:%d gscan:%d low_rssi:%d"
			    " rssi_breach:%d oem:%d ucdrop:%d scan_11d:%d"
			    " fatal_event:%d",
			    stats->pno_match_wake_up_count,
			    stats->pno_complete_wake_up_count,
			    stats->gscan_wake_up_count,
			    stats->low_rssi_wake_up_count,
			    stats->rssi_breach_wake_up_count,
			    stats->oem_response_wake_up_count,
			    stats->uc_drop_wake_up_count,
			    stats->scan_11d,
			    stats->fatal_event_wake_up_count);
}

static void wma_print_wow_stats(t_wma_handle *wma,
				WOW_EVENT_INFO_fixed_param *wake_info)
{
	struct wlan_objmgr_vdev *vdev;
	struct wake_lock_stats stats = {0};

	if (!wma_wow_reason_has_stats(wake_info->wake_reason))
		return;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(wma->psoc,
						    wake_info->vdev_id,
						    WLAN_LEGACY_WMA_ID);
	if (!vdev) {
		wma_err("vdev_id: %d, failed to get vdev from psoc",
			wake_info->vdev_id);
		return;
	}

	ucfg_mc_cp_stats_get_vdev_wake_lock_stats(vdev, &stats);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_WMA_ID);
	wma_wow_stats_display(&stats);
}

#ifdef FEATURE_WLAN_EXTSCAN
/**
 * wma_extscan_get_eventid_from_tlvtag() - map tlv tag to corresponding event id
 * @tag: WMI TLV tag
 *
 * Return:
 *	0 if TLV tag is invalid
 *	else return corresponding WMI event id
 */
static int wma_extscan_get_eventid_from_tlvtag(uint32_t tag)
{
	uint32_t event_id;

	switch (tag) {
	case WMITLV_TAG_STRUC_wmi_extscan_start_stop_event_fixed_param:
		event_id = WMI_EXTSCAN_START_STOP_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_operation_event_fixed_param:
		event_id = WMI_EXTSCAN_OPERATION_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_table_usage_event_fixed_param:
		event_id = WMI_EXTSCAN_TABLE_USAGE_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_cached_results_event_fixed_param:
		event_id = WMI_EXTSCAN_CACHED_RESULTS_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_wlan_change_results_event_fixed_param:
		event_id = WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_hotlist_match_event_fixed_param:
		event_id = WMI_EXTSCAN_HOTLIST_MATCH_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_capabilities_event_fixed_param:
		event_id = WMI_EXTSCAN_CAPABILITIES_EVENTID;
		break;

	default:
		event_id = 0;
		wma_err("Unknown tag: %d", tag);
		break;
	}

	wma_info("For tag %d WMI event 0x%x", tag, event_id);
	return event_id;
}
#else
static int wma_extscan_get_eventid_from_tlvtag(uint32_t tag)
{
	return 0;
}
#endif

/**
 * wow_get_wmi_eventid() - map reason or tlv tag to corresponding event id
 * @tag: WMI TLV tag
 * @reason: WOW reason
 *
 * WOW reason type is primarily used to find the ID. If there could be
 * multiple events that can be sent as a WOW event with same reason
 * then tlv tag is used to identify the corresponding event.
 *
 * Return:
 *      0 if TLV tag/reason is invalid
 *      else return corresponding WMI event id
 */
static int wow_get_wmi_eventid(int32_t reason, uint32_t tag)
{
	int event_id;

	switch (reason) {
	case WOW_REASON_AP_ASSOC_LOST:
		event_id = WMI_ROAM_EVENTID;
		break;
	case WOW_REASON_NLO_SCAN_COMPLETE:
		event_id = WMI_NLO_SCAN_COMPLETE_EVENTID;
		break;
	case WOW_REASON_CSA_EVENT:
		event_id = WMI_CSA_HANDLING_EVENTID;
		break;
	case WOW_REASON_LOW_RSSI:
		event_id = WMI_ROAM_EVENTID;
		break;
	case WOW_REASON_CLIENT_KICKOUT_EVENT:
		event_id = WMI_PEER_STA_KICKOUT_EVENTID;
		break;
	case WOW_REASON_EXTSCAN:
		event_id = wma_extscan_get_eventid_from_tlvtag(tag);
		break;
	case WOW_REASON_RSSI_BREACH_EVENT:
		event_id = WMI_RSSI_BREACH_EVENTID;
		break;
	case WOW_REASON_NAN_EVENT:
		event_id = WMI_NAN_EVENTID;
		break;
	case WOW_REASON_NAN_DATA:
		event_id = wma_ndp_get_eventid_from_tlvtag(tag);
		break;
	case WOW_REASON_TDLS_CONN_TRACKER_EVENT:
		event_id = WOW_TDLS_CONN_TRACKER_EVENT;
		break;
	case WOW_REASON_ROAM_HO:
		event_id = WMI_ROAM_EVENTID;
		break;
	case WOW_REASON_11D_SCAN:
		event_id = WMI_11D_NEW_COUNTRY_EVENTID;
		break;
	case WOW_ROAM_PREAUTH_START_EVENT:
		event_id = WMI_ROAM_PREAUTH_STATUS_CMDID;
		break;
	case WOW_REASON_ROAM_PMKID_REQUEST:
		event_id = WMI_ROAM_PMKID_REQUEST_EVENTID;
		break;
	case WOW_REASON_VDEV_DISCONNECT:
		event_id = WMI_VDEV_DISCONNECT_EVENTID;
		break;
	default:
		wma_debug("No Event Id for WOW reason %s(%d)",
			 wma_wow_wake_reason_str(reason), reason);
		event_id = 0;
		break;
	}
	wlan_roam_debug_log(WMA_INVALID_VDEV_ID, DEBUG_WOW_REASON,
			    DEBUG_INVALID_PEER_ID, NULL, NULL,
			    reason, event_id);

	return event_id;
}

/**
 * is_piggybacked_event() - Returns true if the given wake reason indicates
 *	there will be piggybacked TLV event data
 * @reason: WOW reason
 *
 * There are three types of WoW event payloads: none, piggybacked event, and
 * network packet. This function returns true for wake reasons that fall into
 * the piggybacked event case.
 *
 * Return: true for piggybacked event data
 */
static bool is_piggybacked_event(int32_t reason)
{
	switch (reason) {
	case WOW_REASON_AP_ASSOC_LOST:
	case WOW_REASON_NLO_SCAN_COMPLETE:
	case WOW_REASON_CSA_EVENT:
	case WOW_REASON_LOW_RSSI:
	case WOW_REASON_CLIENT_KICKOUT_EVENT:
	case WOW_REASON_EXTSCAN:
	case WOW_REASON_RSSI_BREACH_EVENT:
	case WOW_REASON_NAN_EVENT:
	case WOW_REASON_NAN_DATA:
	case WOW_REASON_TDLS_CONN_TRACKER_EVENT:
	case WOW_REASON_ROAM_HO:
	case WOW_REASON_ROAM_PMKID_REQUEST:
	case WOW_REASON_VDEV_DISCONNECT:
	case WOW_REASON_TWT:
		return true;
	default:
		return false;
	}
}

/**
 * wma_pkt_proto_subtype_to_string() - to convert proto subtype
 *         of data packet to string.
 * @proto_subtype: proto subtype for data packet
 *
 * This function returns the string for the proto subtype of
 * data packet.
 *
 * Return: string for proto subtype for data packet
 */
static const char *
wma_pkt_proto_subtype_to_string(enum qdf_proto_subtype proto_subtype)
{
	switch (proto_subtype) {
	case QDF_PROTO_EAPOL_M1:
		return "EAPOL M1";
	case QDF_PROTO_EAPOL_M2:
		return "EAPOL M2";
	case QDF_PROTO_EAPOL_M3:
		return "EAPOL M3";
	case QDF_PROTO_EAPOL_M4:
		return "EAPOL M4";
	case QDF_PROTO_DHCP_DISCOVER:
		return "DHCP DISCOVER";
	case QDF_PROTO_DHCP_REQUEST:
		return "DHCP REQUEST";
	case QDF_PROTO_DHCP_OFFER:
		return "DHCP OFFER";
	case QDF_PROTO_DHCP_ACK:
		return "DHCP ACK";
	case QDF_PROTO_DHCP_NACK:
		return "DHCP NACK";
	case QDF_PROTO_DHCP_RELEASE:
		return "DHCP RELEASE";
	case QDF_PROTO_DHCP_INFORM:
		return "DHCP INFORM";
	case QDF_PROTO_DHCP_DECLINE:
		return "DHCP DECLINE";
	case QDF_PROTO_ARP_REQ:
		return "ARP REQUEST";
	case QDF_PROTO_ARP_RES:
		return "ARP RESPONSE";
	case QDF_PROTO_ICMP_REQ:
		return "ICMP REQUEST";
	case QDF_PROTO_ICMP_RES:
		return "ICMP RESPONSE";
	case QDF_PROTO_ICMPV6_REQ:
		return "ICMPV6 REQUEST";
	case QDF_PROTO_ICMPV6_RES:
		return "ICMPV6 RESPONSE";
	case QDF_PROTO_ICMPV6_RS:
		return "ICMPV6 RS";
	case QDF_PROTO_ICMPV6_RA:
		return "ICMPV6 RA";
	case QDF_PROTO_ICMPV6_NS:
		return "ICMPV6 NS";
	case QDF_PROTO_ICMPV6_NA:
		return "ICMPV6 NA";
	case QDF_PROTO_IPV4_UDP:
		return "IPV4 UDP Packet";
	case QDF_PROTO_IPV4_TCP:
		return "IPV4 TCP Packet";
	case QDF_PROTO_IPV6_UDP:
		return "IPV6 UDP Packet";
	case QDF_PROTO_IPV6_TCP:
		return "IPV6 TCP Packet";
	default:
		return NULL;
	}
}

/**
 * wma_wow_get_pkt_proto_subtype() - get the proto subtype of the packet.
 * @data: Pointer to the packet data buffer
 * @len: length of the packet data buffer
 *
 * Return: proto subtype of the packet.
 */
static enum qdf_proto_subtype
wma_wow_get_pkt_proto_subtype(uint8_t *data, uint32_t len)
{
	uint16_t eth_type;
	uint8_t proto_type;

	if (len < QDF_NBUF_TRAC_ETH_TYPE_OFFSET + 2) {
		wma_err("Malformed ethernet packet: length %u < %d",
			len, QDF_NBUF_TRAC_ETH_TYPE_OFFSET + 2);
		return QDF_PROTO_INVALID;
	}

	eth_type = *(uint16_t *)(data + QDF_NBUF_TRAC_ETH_TYPE_OFFSET);
	eth_type = qdf_cpu_to_be16(eth_type);

	wma_debug("Ether Type: 0x%04x", eth_type);
	switch (eth_type) {
	case QDF_NBUF_TRAC_EAPOL_ETH_TYPE:
		if (len < WMA_EAPOL_SUBTYPE_GET_MIN_LEN)
			return QDF_PROTO_INVALID;

		wma_debug("EAPOL Packet");
		return qdf_nbuf_data_get_eapol_subtype(data);

	case QDF_NBUF_TRAC_ARP_ETH_TYPE:
		if (len < WMA_ARP_SUBTYPE_GET_MIN_LEN)
			return QDF_PROTO_INVALID;

		wma_debug("ARP Packet");
		return qdf_nbuf_data_get_arp_subtype(data);

	case QDF_NBUF_TRAC_IPV4_ETH_TYPE:
		if (len < WMA_IPV4_PROTO_GET_MIN_LEN)
			return QDF_PROTO_INVALID;

		wma_debug("IPV4 Packet");

		proto_type = qdf_nbuf_data_get_ipv4_proto(data);
		wma_debug("IPV4_proto_type: %u", proto_type);

		switch (proto_type) {
		case QDF_NBUF_TRAC_ICMP_TYPE:
			if (len < WMA_ICMP_SUBTYPE_GET_MIN_LEN)
				return QDF_PROTO_INVALID;

			wma_debug("ICMP Packet");
			return qdf_nbuf_data_get_icmp_subtype(data);

		case QDF_NBUF_TRAC_UDP_TYPE:
			if (len < WMA_IS_DHCP_GET_MIN_LEN)
				return QDF_PROTO_IPV4_UDP;

			if (!qdf_nbuf_data_is_ipv4_dhcp_pkt(data))
				return QDF_PROTO_IPV4_UDP;

			if (len < WMA_DHCP_SUBTYPE_GET_MIN_LEN)
				return QDF_PROTO_INVALID;

			wma_debug("DHCP Packet");
			return qdf_nbuf_data_get_dhcp_subtype(data);

		case QDF_NBUF_TRAC_TCP_TYPE:
			return QDF_PROTO_IPV4_TCP;

		default:
			return QDF_PROTO_INVALID;
		}

	case QDF_NBUF_TRAC_IPV6_ETH_TYPE:
		if (len < WMA_IPV6_PROTO_GET_MIN_LEN)
			return QDF_PROTO_INVALID;

		wma_debug("IPV6 Packet");

		proto_type = qdf_nbuf_data_get_ipv6_proto(data);
		wma_debug("IPV6_proto_type: %u", proto_type);

		switch (proto_type) {
		case QDF_NBUF_TRAC_ICMPV6_TYPE:
			if (len < WMA_ICMPV6_SUBTYPE_GET_MIN_LEN)
				return QDF_PROTO_INVALID;

			wma_debug("ICMPV6 Packet");
			return qdf_nbuf_data_get_icmpv6_subtype(data);

		case QDF_NBUF_TRAC_UDP_TYPE:
			return QDF_PROTO_IPV6_UDP;

		case QDF_NBUF_TRAC_TCP_TYPE:
			return QDF_PROTO_IPV6_TCP;

		default:
			return QDF_PROTO_INVALID;
		}

	default:
		return QDF_PROTO_INVALID;
	}
}

static void wma_log_pkt_eapol(uint8_t *data, uint32_t length)
{
	uint16_t pkt_len, key_len;

	if (length < WMA_EAPOL_INFO_GET_MIN_LEN)
		return;

	pkt_len = *(uint16_t *)(data + EAPOL_PKT_LEN_OFFSET);
	key_len = *(uint16_t *)(data + EAPOL_KEY_LEN_OFFSET);
	wma_debug("Pkt_len: %u, Key_len: %u",
		 qdf_cpu_to_be16(pkt_len), qdf_cpu_to_be16(key_len));
}

static void wma_log_pkt_dhcp(uint8_t *data, uint32_t length)
{
	uint16_t pkt_len;
	uint32_t trans_id;

	if (length < WMA_DHCP_INFO_GET_MIN_LEN)
		return;

	pkt_len = *(uint16_t *)(data + DHCP_PKT_LEN_OFFSET);
	trans_id = *(uint32_t *)(data + DHCP_TRANSACTION_ID_OFFSET);
	wma_debug("Pkt_len: %u, Transaction_id: %u",
		 qdf_cpu_to_be16(pkt_len), qdf_cpu_to_be16(trans_id));
}

static void wma_log_pkt_icmpv4(uint8_t *data, uint32_t length)
{
	uint16_t pkt_len, seq_num;

	if (length < WMA_IPV4_PKT_INFO_GET_MIN_LEN)
		return;

	pkt_len = *(uint16_t *)(data + IPV4_PKT_LEN_OFFSET);
	seq_num = *(uint16_t *)(data + ICMP_SEQ_NUM_OFFSET);
	wma_debug("Pkt_len: %u, Seq_num: %u",
		 qdf_cpu_to_be16(pkt_len), qdf_cpu_to_be16(seq_num));
}

static void wma_log_pkt_icmpv6(uint8_t *data, uint32_t length)
{
	uint16_t pkt_len, seq_num;

	if (length < WMA_IPV6_PKT_INFO_GET_MIN_LEN)
		return;

	pkt_len = *(uint16_t *)(data + IPV6_PKT_LEN_OFFSET);
	seq_num = *(uint16_t *)(data + ICMPV6_SEQ_NUM_OFFSET);
	wma_debug("Pkt_len: %u, Seq_num: %u",
		 qdf_cpu_to_be16(pkt_len), qdf_cpu_to_be16(seq_num));
}

static void wma_log_pkt_ipv4(uint8_t *data, uint32_t length)
{
	uint16_t pkt_len, src_port, dst_port;
	char *ip_addr;

	if (length < WMA_IPV4_PKT_INFO_GET_MIN_LEN)
		return;

	pkt_len = *(uint16_t *)(data + IPV4_PKT_LEN_OFFSET);
	ip_addr = (char *)(data + IPV4_SRC_ADDR_OFFSET);
	wma_nofl_debug("src addr %d:%d:%d:%d", ip_addr[0], ip_addr[1],
		      ip_addr[2], ip_addr[3]);
	ip_addr = (char *)(data + IPV4_DST_ADDR_OFFSET);
	wma_nofl_debug("dst addr %d:%d:%d:%d", ip_addr[0], ip_addr[1],
		      ip_addr[2], ip_addr[3]);
	src_port = *(uint16_t *)(data + IPV4_SRC_PORT_OFFSET);
	dst_port = *(uint16_t *)(data + IPV4_DST_PORT_OFFSET);
	wma_info("Pkt_len: %u, src_port: %u, dst_port: %u",
		qdf_cpu_to_be16(pkt_len),
		qdf_cpu_to_be16(src_port),
		qdf_cpu_to_be16(dst_port));
}

static void wma_log_pkt_ipv6(uint8_t *data, uint32_t length)
{
	uint16_t pkt_len, src_port, dst_port;
	char *ip_addr;

	if (length < WMA_IPV6_PKT_INFO_GET_MIN_LEN)
		return;

	pkt_len = *(uint16_t *)(data + IPV6_PKT_LEN_OFFSET);
	ip_addr = (char *)(data + IPV6_SRC_ADDR_OFFSET);
	wma_nofl_debug("src addr "IPV6_ADDR_STR, ip_addr[0],
		 ip_addr[1], ip_addr[2], ip_addr[3], ip_addr[4],
		 ip_addr[5], ip_addr[6], ip_addr[7], ip_addr[8],
		 ip_addr[9], ip_addr[10], ip_addr[11],
		 ip_addr[12], ip_addr[13], ip_addr[14],
		 ip_addr[15]);
	ip_addr = (char *)(data + IPV6_DST_ADDR_OFFSET);
	wma_nofl_debug("dst addr "IPV6_ADDR_STR, ip_addr[0],
		 ip_addr[1], ip_addr[2], ip_addr[3], ip_addr[4],
		 ip_addr[5], ip_addr[6], ip_addr[7], ip_addr[8],
		 ip_addr[9], ip_addr[10], ip_addr[11],
		 ip_addr[12], ip_addr[13], ip_addr[14],
		 ip_addr[15]);
	src_port = *(uint16_t *)(data + IPV6_SRC_PORT_OFFSET);
	dst_port = *(uint16_t *)(data + IPV6_DST_PORT_OFFSET);
	wma_info("Pkt_len: %u, src_port: %u, dst_port: %u",
		 qdf_cpu_to_be16(pkt_len),
		 qdf_cpu_to_be16(src_port),
		 qdf_cpu_to_be16(dst_port));
}

static void wma_log_pkt_tcpv4(uint8_t *data, uint32_t length)
{
	uint32_t seq_num;

	if (length < WMA_IPV4_PKT_INFO_GET_MIN_LEN)
		return;

	seq_num = *(uint32_t *)(data + IPV4_TCP_SEQ_NUM_OFFSET);
	wma_debug("TCP_seq_num: %u", qdf_cpu_to_be16(seq_num));
}

static void wma_log_pkt_tcpv6(uint8_t *data, uint32_t length)
{
	uint32_t seq_num;

	if (length < WMA_IPV6_PKT_INFO_GET_MIN_LEN)
		return;

	seq_num = *(uint32_t *)(data + IPV6_TCP_SEQ_NUM_OFFSET);
	wma_debug("TCP_seq_num: %u", qdf_cpu_to_be16(seq_num));
}

static void wma_wow_inc_wake_lock_stats_by_dst_addr(t_wma_handle *wma,
						    uint8_t vdev_id,
						    uint8_t *dest_mac)
{
	ucfg_mc_cp_stats_inc_wake_lock_stats_by_dst_addr(wma->psoc,
							 vdev_id,
							 dest_mac);
}

static void wma_wow_inc_wake_lock_stats_by_protocol(t_wma_handle *wma,
			uint8_t vdev_id, enum qdf_proto_subtype proto_subtype)
{
	ucfg_mc_cp_stats_inc_wake_lock_stats_by_protocol(wma->psoc,
							 vdev_id,
							 proto_subtype);
}

/**
 * wma_wow_parse_data_pkt() - API to parse data buffer for data
 *    packet that resulted in WOW wakeup.
 * @stats: per-vdev stats for tracking packet types
 * @data: Pointer to data buffer
 * @length: data buffer length
 *
 * This function parses the data buffer received (first few bytes of
 * skb->data) to get information like src mac addr, dst mac addr, packet
 * len, seq_num, etc. It also increments stats for different packet types.
 *
 * Return: void
 */
static void wma_wow_parse_data_pkt(t_wma_handle *wma,
				   uint8_t vdev_id, uint8_t *data,
				   uint32_t length)
{
	uint8_t *src_mac;
	uint8_t *dest_mac;
	const char *proto_subtype_name;
	enum qdf_proto_subtype proto_subtype;

	wma_debug("packet length: %u", length);
	if (length < QDF_NBUF_TRAC_IPV4_OFFSET)
		return;

	src_mac = data + QDF_NBUF_SRC_MAC_OFFSET;
	dest_mac = data + QDF_NBUF_DEST_MAC_OFFSET;
	wma_conditional_log(is_wakeup_event_console_logs_enabled,
			    "Src_mac: " QDF_MAC_ADDR_FMT ", Dst_mac: "
			    QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(src_mac),
			    QDF_MAC_ADDR_REF(dest_mac));

	wma_wow_inc_wake_lock_stats_by_dst_addr(wma, vdev_id, dest_mac);

	proto_subtype = wma_wow_get_pkt_proto_subtype(data, length);
	proto_subtype_name = wma_pkt_proto_subtype_to_string(proto_subtype);
	if (proto_subtype_name)
		wma_conditional_log(is_wakeup_event_console_logs_enabled,
				    "WOW Wakeup: %s rcvd", proto_subtype_name);

	switch (proto_subtype) {
	case QDF_PROTO_EAPOL_M1:
	case QDF_PROTO_EAPOL_M2:
	case QDF_PROTO_EAPOL_M3:
	case QDF_PROTO_EAPOL_M4:
		wma_log_pkt_eapol(data, length);
		break;

	case QDF_PROTO_DHCP_DISCOVER:
	case QDF_PROTO_DHCP_REQUEST:
	case QDF_PROTO_DHCP_OFFER:
	case QDF_PROTO_DHCP_ACK:
	case QDF_PROTO_DHCP_NACK:
	case QDF_PROTO_DHCP_RELEASE:
	case QDF_PROTO_DHCP_INFORM:
	case QDF_PROTO_DHCP_DECLINE:
		wma_log_pkt_dhcp(data, length);
		break;

	case QDF_PROTO_ICMP_REQ:
	case QDF_PROTO_ICMP_RES:
		wma_wow_inc_wake_lock_stats_by_protocol(wma, vdev_id,
							proto_subtype);
		wma_log_pkt_icmpv4(data, length);
		break;

	case QDF_PROTO_ICMPV6_REQ:
	case QDF_PROTO_ICMPV6_RES:
	case QDF_PROTO_ICMPV6_RS:
	case QDF_PROTO_ICMPV6_RA:
	case QDF_PROTO_ICMPV6_NS:
	case QDF_PROTO_ICMPV6_NA:
		wma_wow_inc_wake_lock_stats_by_protocol(wma, vdev_id,
							proto_subtype);
		wma_log_pkt_icmpv6(data, length);
		break;

	case QDF_PROTO_IPV4_UDP:
		wma_log_pkt_ipv4(data, length);
		break;
	case QDF_PROTO_IPV4_TCP:
		wma_log_pkt_ipv4(data, length);
		wma_log_pkt_tcpv4(data, length);
		break;

	case QDF_PROTO_IPV6_UDP:
		wma_log_pkt_ipv6(data, length);
		break;
	case QDF_PROTO_IPV6_TCP:
		wma_log_pkt_ipv6(data, length);
		wma_log_pkt_tcpv6(data, length);
		break;
	default:
		break;
	}
}

/**
 * wma_wow_dump_mgmt_buffer() - API to parse data buffer for mgmt.
 *    packet that resulted in WOW wakeup.
 * @wow_packet_buffer: Pointer to data buffer
 * @buf_len: length of data buffer
 *
 * This function parses the data buffer received (802.11 header)
 * to get information like src mac addr, dst mac addr, seq_num,
 * frag_num, etc.
 *
 * Return: void
 */
static void wma_wow_dump_mgmt_buffer(uint8_t *wow_packet_buffer,
				     uint32_t buf_len)
{
	struct ieee80211_frame_addr4 *wh;

	wma_debug("wow_buf_pkt_len: %u", buf_len);
	wh = (struct ieee80211_frame_addr4 *)
		(wow_packet_buffer);
	if (buf_len >= sizeof(struct ieee80211_frame)) {
		uint8_t to_from_ds, frag_num;
		uint32_t seq_num;

		wma_conditional_log(is_wakeup_event_console_logs_enabled,
				    "RA: " QDF_MAC_ADDR_FMT " TA: "
				    QDF_MAC_ADDR_FMT,
				    QDF_MAC_ADDR_REF(wh->i_addr1),
				    QDF_MAC_ADDR_REF(wh->i_addr2));

		wma_conditional_log(is_wakeup_event_console_logs_enabled,
				    "TO_DS: %u, FROM_DS: %u",
				    wh->i_fc[1] & IEEE80211_FC1_DIR_TODS,
				    wh->i_fc[1] & IEEE80211_FC1_DIR_FROMDS);

		to_from_ds = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

		switch (to_from_ds) {
		case IEEE80211_FC1_DIR_NODS:
			wma_conditional_log(
				is_wakeup_event_console_logs_enabled,
				"BSSID: " QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(wh->i_addr3));
			break;
		case IEEE80211_FC1_DIR_TODS:
			wma_conditional_log(
				is_wakeup_event_console_logs_enabled,
				"DA: " QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(wh->i_addr3));
			break;
		case IEEE80211_FC1_DIR_FROMDS:
			wma_conditional_log(
				is_wakeup_event_console_logs_enabled,
				"SA: " QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(wh->i_addr3));
			break;
		case IEEE80211_FC1_DIR_DSTODS:
			if (buf_len >= sizeof(struct ieee80211_frame_addr4))
				wma_conditional_log(
					is_wakeup_event_console_logs_enabled,
					"DA: " QDF_MAC_ADDR_FMT " SA: "
					QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(wh->i_addr3),
					QDF_MAC_ADDR_REF(wh->i_addr4));
			break;
		}

		seq_num = (((*(uint16_t *)wh->i_seq) &
				IEEE80211_SEQ_SEQ_MASK) >>
				IEEE80211_SEQ_SEQ_SHIFT);
		frag_num = (((*(uint16_t *)wh->i_seq) &
				IEEE80211_SEQ_FRAG_MASK) >>
				IEEE80211_SEQ_FRAG_SHIFT);

		wma_conditional_log(is_wakeup_event_console_logs_enabled,
				    "SEQ_NUM: %u, FRAG_NUM: %u", seq_num,
				    frag_num);
	} else {
		wma_err("Insufficient buffer length for mgmt. packet");
	}
}

/**
 * wma_acquire_wakelock() - conditionally aquires a wakelock base on wake reason
 * @wma: the wma handle with the wakelocks to aquire
 * @wake_reason: wow wakeup reason
 *
 * Return: None
 */
static void wma_acquire_wow_wakelock(t_wma_handle *wma, int wake_reason)
{
	qdf_wake_lock_t *wl;
	uint32_t ms;

	switch (wake_reason) {
	case WOW_REASON_AUTH_REQ_RECV:
		wl = &wma->wow_auth_req_wl;
		ms = WMA_AUTH_REQ_RECV_WAKE_LOCK_TIMEOUT;
		break;
	case WOW_REASON_ASSOC_REQ_RECV:
		wl = &wma->wow_assoc_req_wl;
		ms = WMA_ASSOC_REQ_RECV_WAKE_LOCK_DURATION;
		break;
	case WOW_REASON_DEAUTH_RECVD:
		wl = &wma->wow_deauth_rec_wl;
		ms = WMA_DEAUTH_RECV_WAKE_LOCK_DURATION;
		break;
	case WOW_REASON_DISASSOC_RECVD:
		wl = &wma->wow_disassoc_rec_wl;
		ms = WMA_DISASSOC_RECV_WAKE_LOCK_DURATION;
		break;
	case WOW_REASON_AP_ASSOC_LOST:
		wl = &wma->wow_ap_assoc_lost_wl;
		ms = WMA_BMISS_EVENT_WAKE_LOCK_DURATION;
		break;
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	case WOW_REASON_HOST_AUTO_SHUTDOWN:
		wl = &wma->wow_auto_shutdown_wl;
		ms = WMA_AUTO_SHUTDOWN_WAKE_LOCK_DURATION;
		break;
#endif
	case WOW_REASON_ROAM_HO:
		wl = &wma->roam_ho_wl;
		ms = WMA_ROAM_HO_WAKE_LOCK_DURATION;
		break;
	case WOW_REASON_ROAM_PREAUTH_START:
		wl = &wma->roam_preauth_wl;
		ms = WMA_ROAM_PREAUTH_WAKE_LOCK_DURATION;
		break;
	case WOW_REASON_PROBE_REQ_WPS_IE_RECV:
		wl = &wma->probe_req_wps_wl;
		ms = WMA_REASON_PROBE_REQ_WPS_IE_RECV_DURATION;
		break;
	default:
		return;
	}

	wma_alert("Holding %d msec wake_lock", ms);
	cds_host_diag_log_work(wl, ms, WIFI_POWER_EVENT_WAKELOCK_WOW);
	qdf_wake_lock_timeout_acquire(wl, ms);
}

/**
 * wma_wake_reason_ap_assoc_lost() - WOW_REASON_AP_ASSOC_LOST handler
 * @wma: Pointer to wma handle
 * @event: pointer to piggybacked WMI_ROAM_EVENTID_param_tlvs buffer
 * @len: length of the event buffer
 *
 * Return: Errno
 */
static int
wma_wake_reason_ap_assoc_lost(t_wma_handle *wma, void *event, uint32_t len)
{
	WMI_ROAM_EVENTID_param_tlvs *event_param;
	wmi_roam_event_fixed_param *roam_event;

	event_param = event;
	if (!event_param) {
		wma_err("AP Assoc Lost event data is null");
		return -EINVAL;
	}

	roam_event = event_param->fixed_param;
	wma_alert("Beacon miss indication on vdev %d", roam_event->vdev_id);

	wma_beacon_miss_handler(wma, roam_event->vdev_id, roam_event->rssi);

	return 0;
}

static const char *wma_vdev_type_str(uint32_t vdev_type)
{
	switch (vdev_type) {
	case WMI_VDEV_TYPE_AP:
		return "AP";
	case WMI_VDEV_TYPE_STA:
		return "STA";
	case WMI_VDEV_TYPE_IBSS:
		return "IBSS";
	case WMI_VDEV_TYPE_MONITOR:
		return "MONITOR";
	case WMI_VDEV_TYPE_NAN:
		return "NAN";
	case WMI_VDEV_TYPE_OCB:
		return "OCB";
	case WMI_VDEV_TYPE_NDI:
		return "NDI";
	default:
		return "unknown";
	}
}

static int wma_wake_event_packet(
	t_wma_handle *wma,
	WMI_WOW_WAKEUP_HOST_EVENTID_param_tlvs *event_param,
	uint32_t length)
{
	WOW_EVENT_INFO_fixed_param *wake_info;
	struct wma_txrx_node *vdev;
	uint8_t *packet;
	uint32_t packet_len;

	if (event_param->num_wow_packet_buffer <= 4) {
		wma_err("Invalid wow packet buffer from firmware %u",
			 event_param->num_wow_packet_buffer);
		return -EINVAL;
	}
	/* first 4 bytes are the length, followed by the buffer */
	packet_len = *(uint32_t *)event_param->wow_packet_buffer;
	packet = event_param->wow_packet_buffer + 4;

	if (!packet_len) {
		wma_err("Wake event packet is empty");
		return 0;
	}

	if (packet_len > (event_param->num_wow_packet_buffer - 4)) {
		wma_err("Invalid packet_len from firmware, packet_len: %u, num_wow_packet_buffer: %u",
			 packet_len,
			 event_param->num_wow_packet_buffer);
		return -EINVAL;
	}

	wake_info = event_param->fixed_param;

	switch (wake_info->wake_reason) {
	case WOW_REASON_AUTH_REQ_RECV:
	case WOW_REASON_ASSOC_REQ_RECV:
	case WOW_REASON_DEAUTH_RECVD:
	case WOW_REASON_DISASSOC_RECVD:
	case WOW_REASON_ASSOC_RES_RECV:
	case WOW_REASON_REASSOC_REQ_RECV:
	case WOW_REASON_REASSOC_RES_RECV:
	case WOW_REASON_BEACON_RECV:
	case WOW_REASON_ACTION_FRAME_RECV:
		/* management frame case */
		wma_wow_dump_mgmt_buffer(packet, packet_len);
		break;

	case WOW_REASON_BPF_ALLOW:
	case WOW_REASON_PATTERN_MATCH_FOUND:
	case WOW_REASON_RA_MATCH:
	case WOW_REASON_RECV_MAGIC_PATTERN:
	case WOW_REASON_PACKET_FILTER_MATCH:
		wma_debug("Wake event packet:");
		qdf_trace_hex_dump(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_DEBUG,
				   packet, packet_len);

		vdev = &wma->interfaces[wake_info->vdev_id];
		wma_wow_parse_data_pkt(wma, wake_info->vdev_id,
				       packet, packet_len);
		break;

	case WOW_REASON_PAGE_FAULT:
		/*
		 * In case PAGE_FAULT occurs on non-DRV platform,
		 * dump event buffer which contains more info regarding
		 * current page fault.
		 */
		wma_info("PAGE_FAULT occurs during suspend: packet_len %u",
			 packet_len);
		qdf_trace_hex_dump(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_INFO,
				   packet, packet_len);
		break;

	default:
		wma_err("Wake reason %s is not a packet event",
			 wma_wow_wake_reason_str(wake_info->wake_reason));
		return -EINVAL;
	}

	return 0;
}

static int wma_wake_event_no_payload(
	t_wma_handle *wma,
	WMI_WOW_WAKEUP_HOST_EVENTID_param_tlvs *event_param,
	uint32_t length)
{
	WOW_EVENT_INFO_fixed_param *wake_info = event_param->fixed_param;

	switch (wake_info->wake_reason) {
	case WOW_REASON_HOST_AUTO_SHUTDOWN:
		return wma_wake_reason_auto_shutdown();

	case WOW_REASON_NLOD:
		return wma_wake_reason_nlod(wma, wake_info->vdev_id);

	case WOW_REASON_GENERIC_WAKE:
	case WOW_REASON_ROAM_STATS:
		wma_info("Wake reason %s",
			 wma_wow_wake_reason_str(wake_info->wake_reason));
		return 0;

	default:
		return 0;
	}
}

static int wma_wake_event_piggybacked(
	t_wma_handle *wma,
	WMI_WOW_WAKEUP_HOST_EVENTID_param_tlvs *event_param,
	uint32_t length)
{
	int errno = 0;
	void *pb_event;
	uint32_t pb_event_len;
	uint32_t wake_reason;
	uint32_t event_id;
	uint8_t *bssid;
	tpDeleteStaContext del_sta_ctx;

	/*
	 * There are "normal" cases where a wake reason that usually contains a
	 * piggybacked event is empty. In these cases we just want to wake up,
	 * and no action is needed. Bail out now if that is the case.
	 */
	if (!event_param->wow_packet_buffer ||
	    event_param->num_wow_packet_buffer <= 4) {
		wma_err("Invalid wow packet buffer from firmware %u",
			 event_param->num_wow_packet_buffer);
		return 0;
	}

	bssid = wma_get_vdev_bssid
		(wma->interfaces[event_param->fixed_param->vdev_id].vdev);
	if (!bssid) {
		wma_err("Failed to get bssid for vdev_%d",
			event_param->fixed_param->vdev_id);
		return 0;
	}
	wake_reason = event_param->fixed_param->wake_reason;

	/* parse piggybacked event from param buffer */
	{
		int ret_code;
		uint8_t *pb_event_buf;
		uint32_t tag;

		/* first 4 bytes are the length, followed by the buffer */
		pb_event_len = *(uint32_t *)event_param->wow_packet_buffer;
		if (pb_event_len > (event_param->num_wow_packet_buffer - 4)) {
			wma_err("Invalid pb_event_len from firmware, pb_event_len: %u, num_wow_packet_buffer: %u",
				 pb_event_len,
				 event_param->num_wow_packet_buffer);
			return -EINVAL;
		}
		pb_event_buf = event_param->wow_packet_buffer + 4;

		wma_debug("piggybacked event buffer:");
		qdf_trace_hex_dump(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_DEBUG,
				   pb_event_buf, pb_event_len);

		tag = WMITLV_GET_TLVTAG(WMITLV_GET_HDR(pb_event_buf));
		event_id = wow_get_wmi_eventid(wake_reason, tag);
		if (!event_id) {
			wma_err("Unable to find Event Id");
			return -EINVAL;
		}

		ret_code = wmitlv_check_and_pad_event_tlvs(wma, pb_event_buf,
							   pb_event_len,
							   event_id, &pb_event);
		if (ret_code) {
			wma_err("Bad TLVs; len:%d, event_id:%d, status:%d",
				 pb_event_len, event_id, ret_code);
			return -EINVAL;
		}
	}

	switch (wake_reason) {
	case WOW_REASON_AP_ASSOC_LOST:
		errno = wma_wake_reason_ap_assoc_lost(wma, pb_event,
						      pb_event_len);
		break;

#ifdef FEATURE_WLAN_SCAN_PNO
	case WOW_REASON_NLO_SCAN_COMPLETE:
		errno = target_if_nlo_complete_handler(wma, pb_event,
						       pb_event_len);
		break;
#endif /* FEATURE_WLAN_SCAN_PNO */

	case WOW_REASON_CSA_EVENT:
		errno = wma_csa_offload_handler(wma, pb_event, pb_event_len);
		break;

	/*
	 * WOW_REASON_LOW_RSSI is used for following roaming events -
	 * WMI_ROAM_REASON_BETTER_AP, WMI_ROAM_REASON_BMISS,
	 * WMI_ROAM_REASON_SUITABLE_AP will be handled by
	 * wma_roam_event_callback().
	 * WOW_REASON_ROAM_HO is associated with
	 * WMI_ROAM_REASON_HO_FAILED event and it will be handled by
	 * wma_roam_event_callback().
	 */
	case WOW_REASON_LOW_RSSI:
	case WOW_REASON_ROAM_HO:
		wlan_roam_debug_log(event_param->fixed_param->vdev_id,
				    DEBUG_WOW_ROAM_EVENT,
				    DEBUG_INVALID_PEER_ID,
				    NULL, NULL, wake_reason,
				    pb_event_len);
		if (pb_event_len > 0) {
			errno = wma_roam_event_callback(wma, pb_event,
							pb_event_len);
		} else {
			/*
			 * No wow_packet_buffer means a better AP beacon
			 * will follow in a later event.
			 */
			wma_debug("Host woken up because of better AP beacon");
		}
		break;

	case WOW_REASON_CLIENT_KICKOUT_EVENT:
		errno = wma_peer_sta_kickout_event_handler(wma, pb_event,
							   pb_event_len);
		break;

#ifdef FEATURE_WLAN_EXTSCAN
	case WOW_REASON_EXTSCAN:
		errno = wma_extscan_wow_event_callback(wma, pb_event,
						       pb_event_len);
		break;
#endif

	case WOW_REASON_RSSI_BREACH_EVENT:
		errno = wma_rssi_breached_event_handler(wma, pb_event,
							pb_event_len);
		break;

	case WOW_REASON_NAN_EVENT:
		errno = wma_nan_rsp_handler_callback(wma, pb_event,
						     pb_event_len);
		break;

	case WOW_REASON_NAN_DATA:
		errno = wma_ndp_wow_event_callback(wma, pb_event, pb_event_len,
						   event_id);
		break;

#ifdef FEATURE_WLAN_TDLS
	case WOW_REASON_TDLS_CONN_TRACKER_EVENT:
		errno = wma_tdls_event_handler(wma, pb_event, pb_event_len);
		break;
#endif

	case WOW_REASON_TIMER_INTR_RECV:
		/*
		 * Right now firmware is not returning any cookie host has
		 * programmed. So do not check for cookie.
		 */
		wma_err("WOW_REASON_TIMER_INTR_RECV received, indicating key exchange did not finish. Initiate disconnect");
		del_sta_ctx = qdf_mem_malloc(sizeof(*del_sta_ctx));
		if (!del_sta_ctx)
			break;

		del_sta_ctx->is_tdls = false;
		del_sta_ctx->vdev_id = event_param->fixed_param->vdev_id;
		qdf_mem_copy(del_sta_ctx->addr2, bssid, QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(del_sta_ctx->bssId, bssid, QDF_MAC_ADDR_SIZE);
		del_sta_ctx->reasonCode = HAL_DEL_STA_REASON_CODE_KEEP_ALIVE;
		wma_send_msg(wma, SIR_LIM_DELETE_STA_CONTEXT_IND, del_sta_ctx,
			     0);
		break;
	case WOW_REASON_ROAM_PMKID_REQUEST:
		wma_debug("Host woken up because of PMKID request event");
		errno = wma_roam_pmkid_request_event_handler(wma, pb_event,
							     pb_event_len);
		break;
	case WOW_REASON_VDEV_DISCONNECT:
		wma_debug("Host woken up because of vdev disconnect event");
		errno = wma_roam_vdev_disconnect_event_handler(wma, pb_event,
							       pb_event_len);
		break;
	default:
		wma_err("Wake reason %s(%u) is not a piggybacked event",
			 wma_wow_wake_reason_str(wake_reason), wake_reason);
		errno = -EINVAL;
		break;
	}

	wmitlv_free_allocated_event_tlvs(event_id, &pb_event);

	return errno;
}

static void wma_debug_assert_page_fault_wakeup(uint32_t reason)
{
	/* During DRV if page fault wake up then assert */
	if ((WOW_REASON_PAGE_FAULT == reason) && (qdf_is_drv_connected()))
		QDF_DEBUG_PANIC("Unexpected page fault wake up detected during DRV wow");
}

static void wma_wake_event_log_reason(t_wma_handle *wma,
				      WOW_EVENT_INFO_fixed_param *wake_info)
{
	struct wma_txrx_node *vdev;

	/* "Unspecified" means APPS triggered wake, else firmware triggered */
	if (wake_info->wake_reason != WOW_REASON_UNSPECIFIED) {
		vdev = &wma->interfaces[wake_info->vdev_id];
		wma_nofl_alert("WLAN triggered wakeup: %s (%d), vdev: %d (%s)",
			      wma_wow_wake_reason_str(wake_info->wake_reason),
			      wake_info->wake_reason,
			      wake_info->vdev_id,
			      wma_vdev_type_str(vdev->type));
		wma_debug_assert_page_fault_wakeup(wake_info->wake_reason);
	} else if (!wmi_get_runtime_pm_inprogress(wma->wmi_handle)) {
		wma_nofl_alert("Non-WLAN triggered wakeup: %s (%d)",
			      wma_wow_wake_reason_str(wake_info->wake_reason),
			      wake_info->wake_reason);
	}

	qdf_wow_wakeup_host_event(wake_info->wake_reason);
	qdf_wma_wow_wakeup_stats_event(wma);
}

/**
 * wma_wow_wakeup_host_event() - wakeup host event handler
 * @handle: wma handle
 * @event: event data
 * @len: buffer length
 *
 * Handler to catch wow wakeup host event. This event will have
 * reason why the firmware has woken the host.
 *
 * Return: Errno
 */
int wma_wow_wakeup_host_event(void *handle, uint8_t *event, uint32_t len)
{
	int errno;
	t_wma_handle *wma = handle;
	WMI_WOW_WAKEUP_HOST_EVENTID_param_tlvs *event_param;
	WOW_EVENT_INFO_fixed_param *wake_info;

	event_param = (WMI_WOW_WAKEUP_HOST_EVENTID_param_tlvs *)event;
	if (!event_param) {
		wma_err("Wake event data is null");
		return -EINVAL;
	}

	wake_info = event_param->fixed_param;

	if (wake_info->vdev_id >= wma->max_bssid) {
		wma_err("received invalid vdev_id %d", wake_info->vdev_id);
		return -EINVAL;
	}

	wma_wake_event_log_reason(wma, wake_info);

	ucfg_pmo_psoc_wakeup_host_event_received(wma->psoc);

	wma_print_wow_stats(wma, wake_info);
	/* split based on payload type */
	if (is_piggybacked_event(wake_info->wake_reason))
		errno = wma_wake_event_piggybacked(wma, event_param, len);
	else if (event_param->wow_packet_buffer)
		errno = wma_wake_event_packet(wma, event_param, len);
	else
		errno = wma_wake_event_no_payload(wma, event_param, len);

	wma_inc_wow_stats(wma, wake_info);
	wma_print_wow_stats(wma, wake_info);
	wma_acquire_wow_wakelock(wma, wake_info->wake_reason);

	return errno;
}

#ifdef FEATURE_WLAN_D0WOW
/**
 * wma_d0_wow_disable_ack_event() - wakeup host event handler
 * @handle: wma handle
 * @event: event data
 * @len: buffer length
 *
 * Handler to catch D0-WOW disable ACK event.  This event will have
 * reason why the firmware has woken the host.
 * This is for backward compatible with cld2.0.
 *
 * Return: 0 for success or error
 */
int wma_d0_wow_disable_ack_event(void *handle, uint8_t *event, uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_D0_WOW_DISABLE_ACK_EVENTID_param_tlvs *param_buf;
	wmi_d0_wow_disable_ack_event_fixed_param *resp_data;

	param_buf = (WMI_D0_WOW_DISABLE_ACK_EVENTID_param_tlvs *)event;
	if (!param_buf) {
		wma_err("Invalid D0-WOW disable ACK event buffer!");
		return -EINVAL;
	}

	resp_data = param_buf->fixed_param;

	ucfg_pmo_psoc_wakeup_host_event_received(wma->psoc);

	wma_debug("Received D0-WOW disable ACK");

	return 0;
}
#else
int wma_d0_wow_disable_ack_event(void *handle, uint8_t *event, uint32_t len)
{
	return 0;
}
#endif

/**
 * wma_pdev_resume_event_handler() - PDEV resume event handler
 * @handle: wma handle
 * @event: event data
 * @len: buffer length
 *
 * Return: 0 for success or error
 */
int wma_pdev_resume_event_handler(void *handle, uint8_t *event, uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;

	wma_nofl_alert("Received PDEV resume event");

	ucfg_pmo_psoc_wakeup_host_event_received(wma->psoc);

	return 0;
}

/**
 * wma_del_ts_req() - send DELTS request to fw
 * @wma: wma handle
 * @msg: delts params
 *
 * Return: none
 */
void wma_del_ts_req(tp_wma_handle wma, struct del_ts_params *msg)
{
	if (!wma_is_vdev_valid(msg->sessionId)) {
		wma_err("vdev id:%d is not active ", msg->sessionId);
		qdf_mem_free(msg);
		return;
	}
	if (wmi_unified_del_ts_cmd(wma->wmi_handle,
				 msg->sessionId,
				 TID_TO_WME_AC(msg->userPrio))) {
		wma_alert("Failed to send vdev DELTS command");
	}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (msg->setRICparams == true)
		wma_set_ric_req(wma, msg, false);
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
	qdf_mem_free(msg);
}

void wma_aggr_qos_req(tp_wma_handle wma,
		      struct aggr_add_ts_param *aggr_qos_rsp_msg)
{
	if (!wma_is_vdev_valid(aggr_qos_rsp_msg->vdev_id)) {
		wma_err("vdev id:%d is not active ",
			 aggr_qos_rsp_msg->vdev_id);
		return;
	}
	wmi_unified_aggr_qos_cmd(wma->wmi_handle, aggr_qos_rsp_msg);
	/* send response to upper layers from here only. */
	wma_send_msg_high_priority(wma, WMA_AGGR_QOS_RSP, aggr_qos_rsp_msg, 0);
}

#ifdef FEATURE_WLAN_ESE
/**
 * wma_set_tsm_interval() - Set TSM interval
 * @req: pointer to ADDTS request
 *
 * Return: QDF_STATUS_E_FAILURE or QDF_STATUS_SUCCESS
 */
static QDF_STATUS wma_set_tsm_interval(struct add_ts_param *req)
{
	/*
	 * msmt_interval is in unit called TU (1 TU = 1024 us)
	 * max value of msmt_interval cannot make resulting
	 * interval_milliseconds overflow 32 bit
	 *
	 */
	uint32_t interval_milliseconds;

	interval_milliseconds = (req->tsm_interval * 1024) / 1000;

	cdp_tx_set_compute_interval(cds_get_context(QDF_MODULE_ID_SOC),
			WMI_PDEV_ID_SOC,
			interval_milliseconds);
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS wma_set_tsm_interval(struct add_ts_param *req)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_WLAN_ESE */

/**
 * wma_add_ts_req() - send ADDTS request to fw
 * @wma: wma handle
 * @msg: ADDTS params
 *
 * Return: none
 */
void wma_add_ts_req(tp_wma_handle wma, struct add_ts_param *msg)
{
	struct add_ts_param cmd = {0};

	msg->status = QDF_STATUS_SUCCESS;
	if (wma_set_tsm_interval(msg) == QDF_STATUS_SUCCESS) {

		cmd.vdev_id = msg->vdev_id;
		cmd.tspec.tsinfo.traffic.userPrio =
			TID_TO_WME_AC(msg->tspec.tsinfo.traffic.userPrio);
		cmd.tspec.mediumTime = msg->tspec.mediumTime;
		if (wmi_unified_add_ts_cmd(wma->wmi_handle, &cmd))
			msg->status = QDF_STATUS_E_FAILURE;

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
		if (msg->set_ric_params)
			wma_set_ric_req(wma, msg, true);
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

	}
	wma_send_msg_high_priority(wma, WMA_ADD_TS_RSP, msg, 0);
}

#ifdef FEATURE_WLAN_ESE

#define TSM_DELAY_HISTROGRAM_BINS 4
/**
 * wma_process_tsm_stats_req() - process tsm stats request
 * @wma_handler - handle to wma
 * @pTsmStatsMsg - TSM stats struct that needs to be populated and
 *         passed in message.
 *
 * A parallel function to WMA_ProcessTsmStatsReq for pronto. This
 * function fetches stats from data path APIs and post
 * WMA_TSM_STATS_RSP msg back to LIM.
 *
 * Return: QDF status
 */
QDF_STATUS wma_process_tsm_stats_req(tp_wma_handle wma_handler,
				     void *pTsmStatsMsg)
{
	uint8_t counter;
	uint32_t queue_delay_microsec = 0;
	uint32_t tx_delay_microsec = 0;
	uint16_t packet_count = 0;
	uint16_t packet_loss_count = 0;
	tpAniTrafStrmMetrics pTsmMetric = NULL;
	tpAniGetTsmStatsReq pStats = (tpAniGetTsmStatsReq) pTsmStatsMsg;
	tpAniGetTsmStatsRsp pTsmRspParams = NULL;
	int tid = pStats->tid;
	/*
	 * The number of histrogram bin report by data path api are different
	 * than required by TSM, hence different (6) size array used
	 */
	uint16_t bin_values[QCA_TX_DELAY_HIST_REPORT_BINS] = { 0, };
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	/* get required values from data path APIs */
	cdp_tx_delay(soc,
		WMI_PDEV_ID_SOC,
		&queue_delay_microsec,
		&tx_delay_microsec, tid);
	cdp_tx_delay_hist(soc,
		WMI_PDEV_ID_SOC,
		bin_values, tid);
	cdp_tx_packet_count(soc,
		WMI_PDEV_ID_SOC,
		&packet_count,
		&packet_loss_count, tid);

	pTsmRspParams = qdf_mem_malloc(sizeof(*pTsmRspParams));
	if (!pTsmRspParams) {
		QDF_ASSERT(0);
		qdf_mem_free(pTsmStatsMsg);
		return QDF_STATUS_E_NOMEM;
	}

	qdf_copy_macaddr(&pTsmRspParams->bssid, &pStats->bssId);
	pTsmRspParams->rc = QDF_STATUS_E_FAILURE;
	pTsmRspParams->tsmStatsReq = pStats;
	pTsmMetric = &pTsmRspParams->tsmMetrics;
	/* populate pTsmMetric */
	pTsmMetric->UplinkPktQueueDly = queue_delay_microsec;
	/* store only required number of bin values */
	for (counter = 0; counter < TSM_DELAY_HISTROGRAM_BINS; counter++) {
		pTsmMetric->UplinkPktQueueDlyHist[counter] =
			bin_values[counter];
	}
	pTsmMetric->UplinkPktTxDly = tx_delay_microsec;
	pTsmMetric->UplinkPktLoss = packet_loss_count;
	pTsmMetric->UplinkPktCount = packet_count;

	/*
	 * No need to populate roaming delay and roaming count as they are
	 * being populated just before sending IAPP frame out
	 */
	/* post this message to LIM/PE */
	wma_send_msg(wma_handler, WMA_TSM_STATS_RSP, (void *)pTsmRspParams, 0);
	return QDF_STATUS_SUCCESS;
}

#endif /* FEATURE_WLAN_ESE */

/**
 * wma_process_mcbc_set_filter_req() - process mcbc set filter request
 * @wma_handle: wma handle
 * @mcbc_param: mcbc params
 *
 * Return: QDF status
 */
QDF_STATUS wma_process_mcbc_set_filter_req(tp_wma_handle wma_handle,
					   tSirRcvFltMcAddrList *mcbc_param)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_process_add_periodic_tx_ptrn_ind() - add periodic tx pattern
 * @handle: wma handle
 * @pattern: tx pattern params
 *
 * Return: QDF status
 */
QDF_STATUS wma_process_add_periodic_tx_ptrn_ind(WMA_HANDLE handle,
						tSirAddPeriodicTxPtrn *pattern)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct periodic_tx_pattern *params_ptr;
	uint8_t vdev_id;
	QDF_STATUS status;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, can not issue fw add pattern cmd");
		return QDF_STATUS_E_INVAL;
	}

	if (wma_find_vdev_id_by_addr(wma_handle,
				     pattern->mac_address.bytes,
				     &vdev_id)) {
		wma_err("Failed to find vdev id for "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(pattern->mac_address.bytes));
		return QDF_STATUS_E_INVAL;
	}

	params_ptr = qdf_mem_malloc(sizeof(*params_ptr));
	if (!params_ptr)
		return QDF_STATUS_E_NOMEM;

	params_ptr->ucPtrnId = pattern->ucPtrnId;
	params_ptr->ucPtrnSize = pattern->ucPtrnSize;
	params_ptr->usPtrnIntervalMs = pattern->usPtrnIntervalMs;
	qdf_mem_copy(&params_ptr->mac_address, &pattern->mac_address,
		     sizeof(struct qdf_mac_addr));
	qdf_mem_copy(params_ptr->ucPattern, pattern->ucPattern,
		     params_ptr->ucPtrnSize);

	status = wmi_unified_process_add_periodic_tx_ptrn_cmd(
				wma_handle->wmi_handle, params_ptr, vdev_id);

	qdf_mem_free(params_ptr);
	return status;
}

/**
 * wma_process_del_periodic_tx_ptrn_ind - del periodic tx ptrn
 * @handle: wma handle
 * @pDelPeriodicTxPtrnParams: tx ptrn params
 *
 * Return: QDF status
 */
QDF_STATUS wma_process_del_periodic_tx_ptrn_ind(WMA_HANDLE handle,
						tSirDelPeriodicTxPtrn *
						pDelPeriodicTxPtrnParams)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	uint8_t vdev_id;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, can not issue Del Pattern cmd");
		return QDF_STATUS_E_INVAL;
	}

	if (wma_find_vdev_id_by_addr(
			wma_handle,
			pDelPeriodicTxPtrnParams->mac_address.bytes,
			&vdev_id)) {
		wma_err("Failed to find vdev id for "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(pDelPeriodicTxPtrnParams->mac_address.bytes));
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_process_del_periodic_tx_ptrn_cmd(
				wma_handle->wmi_handle, vdev_id,
				pDelPeriodicTxPtrnParams->ucPtrnId);
}

#ifdef WLAN_FEATURE_STATS_EXT
QDF_STATUS wma_stats_ext_req(void *wma_ptr, tpStatsExtRequest preq)
{
	tp_wma_handle wma = (tp_wma_handle) wma_ptr;
	struct stats_ext_params *params;
	size_t params_len;
	QDF_STATUS status;

	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	params_len = sizeof(*params) + preq->request_data_len;
	params = qdf_mem_malloc(params_len);
	if (!params)
		return QDF_STATUS_E_NOMEM;

	params->vdev_id = preq->vdev_id;
	params->request_data_len = preq->request_data_len;
	if (preq->request_data_len > 0)
		qdf_mem_copy(params->request_data, preq->request_data,
			     params->request_data_len);

	status = wmi_unified_stats_ext_req_cmd(wma->wmi_handle, params);
	qdf_mem_free(params);

	return status;
}

#endif /* WLAN_FEATURE_STATS_EXT */

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
/**
 * wma_send_status_of_ext_wow() - send ext wow status to SME
 * @wma: wma handle
 * @status: status
 *
 * Return: none
 */
static void wma_send_status_of_ext_wow(tp_wma_handle wma, bool status)
{
	tSirReadyToExtWoWInd *ready_to_extwow;
	QDF_STATUS vstatus;
	struct scheduler_msg message = {0};
	uint8_t len;

	wma_debug("Posting ready to suspend indication to umac");

	len = sizeof(tSirReadyToExtWoWInd);
	ready_to_extwow = qdf_mem_malloc(len);
	if (!ready_to_extwow)
		return;

	ready_to_extwow->mesgType = eWNI_SME_READY_TO_EXTWOW_IND;
	ready_to_extwow->mesgLen = len;
	ready_to_extwow->status = status;

	message.type = eWNI_SME_READY_TO_EXTWOW_IND;
	message.bodyptr = (void *)ready_to_extwow;
	message.bodyval = 0;

	vstatus = scheduler_post_message(QDF_MODULE_ID_WMA,
					 QDF_MODULE_ID_SME,
					 QDF_MODULE_ID_SME, &message);
	if (vstatus != QDF_STATUS_SUCCESS)
		qdf_mem_free(ready_to_extwow);
}

/**
 * wma_enable_ext_wow() - enable ext wow in fw
 * @wma: wma handle
 * @params: ext wow params
 *
 * Return:0 for success or error code
 */
QDF_STATUS wma_enable_ext_wow(tp_wma_handle wma, tpSirExtWoWParams params)
{
	struct ext_wow_params wow_params = {0};
	QDF_STATUS status;

	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	wow_params.vdev_id = params->vdev_id;
	wow_params.type = (enum wmi_ext_wow_type) params->type;
	wow_params.wakeup_pin_num = params->wakeup_pin_num;

	status = wmi_unified_enable_ext_wow_cmd(wma->wmi_handle,
				&wow_params);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	wma_send_status_of_ext_wow(wma, true);
	return status;

}

/**
 * wma_set_app_type1_params_in_fw() - set app type1 params in fw
 * @wma: wma handle
 * @appType1Params: app type1 params
 *
 * Return: QDF status
 */
int wma_set_app_type1_params_in_fw(tp_wma_handle wma,
				   tpSirAppType1Params appType1Params)
{
	int ret;

	ret = wmi_unified_app_type1_params_in_fw_cmd(wma->wmi_handle,
				   (struct app_type1_params *)appType1Params);
	if (ret) {
		wma_err("Failed to set APP TYPE1 PARAMS");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_set_app_type2_params_in_fw() - set app type2 params in fw
 * @wma: wma handle
 * @appType2Params: app type2 params
 *
 * Return: QDF status
 */
QDF_STATUS wma_set_app_type2_params_in_fw(tp_wma_handle wma,
					  tpSirAppType2Params appType2Params)
{
	struct app_type2_params params = {0};

	if (!wma) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	params.vdev_id = appType2Params->vdev_id;
	params.rc4_key_len = appType2Params->rc4_key_len;
	qdf_mem_copy(params.rc4_key, appType2Params->rc4_key, 16);
	params.ip_id = appType2Params->ip_id;
	params.ip_device_ip = appType2Params->ip_device_ip;
	params.ip_server_ip = appType2Params->ip_server_ip;
	params.tcp_src_port = appType2Params->tcp_src_port;
	params.tcp_dst_port = appType2Params->tcp_dst_port;
	params.tcp_seq = appType2Params->tcp_seq;
	params.tcp_ack_seq = appType2Params->tcp_ack_seq;
	params.keepalive_init = appType2Params->keepalive_init;
	params.keepalive_min = appType2Params->keepalive_min;
	params.keepalive_max = appType2Params->keepalive_max;
	params.keepalive_inc = appType2Params->keepalive_inc;
	params.tcp_tx_timeout_val = appType2Params->tcp_tx_timeout_val;
	params.tcp_rx_timeout_val = appType2Params->tcp_rx_timeout_val;
	qdf_mem_copy(&params.gateway_mac, &appType2Params->gateway_mac,
			sizeof(struct qdf_mac_addr));

	return wmi_unified_set_app_type2_params_in_fw_cmd(wma->wmi_handle,
							&params);
}
#endif /* WLAN_FEATURE_EXTWOW_SUPPORT */

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
/**
 * wma_auto_shutdown_event_handler() - process auto shutdown timer trigger
 * @handle: wma handle
 * @event: event buffer
 * @len: buffer length
 *
 * Return: 0 for success or error code
 */
int wma_auto_shutdown_event_handler(void *handle, uint8_t *event,
				    uint32_t len)
{
	wmi_host_auto_shutdown_event_fixed_param *wmi_auto_sh_evt;
	WMI_HOST_AUTO_SHUTDOWN_EVENTID_param_tlvs *param_buf =
		(WMI_HOST_AUTO_SHUTDOWN_EVENTID_param_tlvs *)
		event;

	if (!param_buf || !param_buf->fixed_param) {
		wma_err("Invalid Auto shutdown timer evt");
		return -EINVAL;
	}

	wmi_auto_sh_evt = param_buf->fixed_param;

	if (wmi_auto_sh_evt->shutdown_reason
	    != WMI_HOST_AUTO_SHUTDOWN_REASON_TIMER_EXPIRY) {
		wma_err("Invalid Auto shutdown timer evt");
		return -EINVAL;
	}

	wma_debug("Auto Shutdown Evt: %d", wmi_auto_sh_evt->shutdown_reason);
	return wma_wake_reason_auto_shutdown();
}

QDF_STATUS
wma_set_auto_shutdown_timer_req(tp_wma_handle wma_handle,
				struct auto_shutdown_cmd *auto_sh_cmd)
{
	if (!auto_sh_cmd) {
		wma_err("Invalid Autoshutdown cfg cmd");
		return QDF_STATUS_E_FAILURE;
	}

	return wmi_unified_set_auto_shutdown_timer_cmd(wma_handle->wmi_handle,
						       auto_sh_cmd->timer_val);
}
#endif /* FEATURE_WLAN_AUTO_SHUTDOWN */

#ifdef DHCP_SERVER_OFFLOAD
QDF_STATUS
wma_process_dhcpserver_offload(tp_wma_handle wma_handle,
			       struct dhcp_offload_info_params *params)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle;

	if (!wma_handle) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	wmi_handle = wma_handle->wmi_handle;
	status = wmi_unified_process_dhcpserver_offload_cmd(wmi_handle,
							    params);
	wma_debug("Set dhcp server offload to vdev %d status %d",
		 params->vdev_id, status);

	return status;
}
#endif /* DHCP_SERVER_OFFLOAD */

#ifdef WLAN_FEATURE_GPIO_LED_FLASHING
/**
 * wma_set_led_flashing() - set led flashing in fw
 * @wma_handle: wma handle
 * @flashing: flashing request
 *
 * Return: QDF status
 */
QDF_STATUS wma_set_led_flashing(tp_wma_handle wma_handle,
				struct flashing_req_params *flashing)
{
	QDF_STATUS status;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_INVAL;
	}
	if (!flashing) {
		wma_err("invalid parameter: flashing");
		return QDF_STATUS_E_INVAL;
	}
	status = wmi_unified_set_led_flashing_cmd(wma_handle->wmi_handle,
						  flashing);
	return status;
}
#endif /* WLAN_FEATURE_GPIO_LED_FLASHING */

int wma_sar_rsp_evt_handler(ol_scn_t handle, uint8_t *event, uint32_t len)
{
	tp_wma_handle wma_handle;
	wmi_unified_t wmi_handle;
	QDF_STATUS status;

	wma_debug("handle:%pK event:%pK len:%u", handle, event, len);

	wma_handle = handle;
	if (!wma_handle) {
		wma_err("NULL wma_handle");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("NULL wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_extract_sar2_result_event(wmi_handle,
						       event, len);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Event extract failure: %d", status);
		return -EINVAL;
	}

	return 0;
}

#ifdef FEATURE_WLAN_CH_AVOID
/**
 * wma_process_ch_avoid_update_req() - handles channel avoid update request
 * @wma_handle: wma handle
 * @ch_avoid_update_req: channel avoid update params
 *
 * Return: QDF status
 */
QDF_STATUS wma_process_ch_avoid_update_req(tp_wma_handle wma_handle,
					   tSirChAvoidUpdateReq *
					   ch_avoid_update_req)
{
	QDF_STATUS status;

	if (!wma_handle) {
		wma_err("wma handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (!ch_avoid_update_req) {
		wma_err("ch_avoid_update_req is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("WMA --> WMI_CHAN_AVOID_UPDATE");

	status = wmi_unified_process_ch_avoid_update_cmd(
					wma_handle->wmi_handle);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	wma_debug("WMA --> WMI_CHAN_AVOID_UPDATE sent through WMI");
	return status;
}
#endif

void wma_send_regdomain_info_to_fw(uint32_t reg_dmn, uint16_t regdmn2G,
				   uint16_t regdmn5G, uint8_t ctl2G,
				   uint8_t ctl5G)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	int32_t cck_mask_val = 0;
	struct pdev_params pdev_param = {0};
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	wma_debug("reg_dmn: %d regdmn2g: %d regdmn5g :%d ctl2g: %d ctl5g: %d",
		 reg_dmn, regdmn2G, regdmn5G, ctl2G, ctl5G);

	if (!wma) {
		wma_err("wma context is NULL");
		return;
	}

	status = wmi_unified_send_regdomain_info_to_fw_cmd(wma->wmi_handle,
			reg_dmn, regdmn2G, regdmn5G, ctl2G, ctl5G);
	if (status == QDF_STATUS_E_NOMEM)
		return;

	if ((((reg_dmn & ~CTRY_FLAG) == CTRY_JAPAN15) ||
	     ((reg_dmn & ~CTRY_FLAG) == CTRY_KOREA_ROC)) &&
	    (true == wma->tx_chain_mask_cck))
		cck_mask_val = 1;

	cck_mask_val |= (wma->self_gen_frm_pwr << 16);
	pdev_param.param_id = WMI_PDEV_PARAM_TX_CHAIN_MASK_CCK;
	pdev_param.param_value = cck_mask_val;
	ret = wmi_unified_pdev_param_send(wma->wmi_handle,
					 &pdev_param,
					 WMA_WILDCARD_PDEV_ID);

	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("failed to set PDEV tx_chain_mask_cck %d", ret);
}

#ifdef FEATURE_WLAN_TDLS
/**
 * wma_tdls_event_handler() - handle TDLS event
 * @handle: wma handle
 * @event: event buffer
 * @len: buffer length
 *
 * Return: 0 for success or error code
 */
int wma_tdls_event_handler(void *handle, uint8_t *event, uint32_t len)
{
	/* TODO update with target rx ops */
	return 0;
}

/**
 * wma_update_tdls_peer_state() - update TDLS peer state
 * @handle: wma handle
 * @peer_state: TDLS peer state params
 *
 * Return: 0 for success or error code
 */
int wma_update_tdls_peer_state(WMA_HANDLE handle,
			       struct tdls_peer_update_state *peer_state)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	uint32_t i;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct tdls_peer_params *peer_cap;
	int ret = 0;
	uint32_t *ch_mhz = NULL;
	size_t ch_mhz_len;
	uint8_t chan_id;
	bool restore_last_peer = false;
	QDF_STATUS qdf_status;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, can not issue cmd");
		ret = -EINVAL;
		goto end_tdls_peer_state;
	}

	if (!soc) {
		wma_err("SOC context is NULL");
		ret = -EINVAL;
		goto end_tdls_peer_state;
	}

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(wma_handle->psoc,
					   peer_state->vdev_id)) {
		wma_err("roaming in progress, reject peer update cmd!");
		ret = -EPERM;
		goto end_tdls_peer_state;
	}


	if (!wma_objmgr_peer_exist(wma_handle,
				   peer_state->peer_macaddr, NULL)) {
		wma_err("peer:" QDF_MAC_ADDR_FMT "doesn't exist",
			QDF_MAC_ADDR_REF(peer_state->peer_macaddr));
		ret = -EINVAL;
		goto end_tdls_peer_state;
	}

	peer_cap = &peer_state->peer_cap;

	/* peer capability info is valid only when peer state is connected */
	if (TDLS_PEER_STATE_CONNECTED != peer_state->peer_state)
		qdf_mem_zero(peer_cap, sizeof(*peer_cap));

	if (peer_cap->peer_chanlen) {
		ch_mhz_len = sizeof(*ch_mhz) * peer_cap->peer_chanlen;
		ch_mhz = qdf_mem_malloc(ch_mhz_len);
		if (!ch_mhz) {
			ret = -ENOMEM;
			goto end_tdls_peer_state;
		}

		for (i = 0; i < peer_cap->peer_chanlen; ++i) {
			chan_id = peer_cap->peer_chan[i].chan_id;
			ch_mhz[i] = cds_chan_to_freq(chan_id);
		}
	}

	cdp_peer_set_tdls_offchan_enabled(soc, peer_state->vdev_id,
					  peer_state->peer_macaddr,
					  !!peer_cap->peer_off_chan_support);

	if (wmi_unified_update_tdls_peer_state_cmd(wma_handle->wmi_handle,
						   peer_state,
						   ch_mhz)) {
		wma_err("failed to send tdls peer update state command");
		ret = -EIO;
		/* Fall through to delete TDLS peer for teardown */
	}

	/* in case of teardown, remove peer from fw */
	if (TDLS_PEER_STATE_TEARDOWN == peer_state->peer_state) {
		restore_last_peer = cdp_peer_is_vdev_restore_last_peer(
						soc,
						peer_state->vdev_id,
						peer_state->peer_macaddr);

		wma_debug("calling wma_remove_peer for peer " QDF_MAC_ADDR_FMT
			 " vdevId: %d",
			 QDF_MAC_ADDR_REF(peer_state->peer_macaddr),
			 peer_state->vdev_id);
		qdf_status = wma_remove_peer(wma_handle,
					     peer_state->peer_macaddr,
					     peer_state->vdev_id, false);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			wma_err("wma_remove_peer failed");
			ret = -EINVAL;
		}
		cdp_peer_update_last_real_peer(soc, WMI_PDEV_ID_SOC,
					       peer_state->vdev_id,
					       restore_last_peer);
	}

	if (TDLS_PEER_STATE_CONNECTED == peer_state->peer_state) {
		cdp_peer_state_update(soc, peer_state->peer_macaddr,
				      OL_TXRX_PEER_STATE_AUTH);
	}

end_tdls_peer_state:
	if (ch_mhz)
		qdf_mem_free(ch_mhz);
	if (peer_state)
		qdf_mem_free(peer_state);
	return ret;
}
#endif /* FEATURE_WLAN_TDLS */

/*
 * wma_process_cfg_action_frm_tb_ppdu() - action frame TB PPDU cfg to firmware
 * @wma:                Pointer to WMA handle
 * @cfg_info:       Pointer for cfg info
 *
 * Return: QDF_STATUS_SUCCESS for success otherwise failure
 *
 */
QDF_STATUS wma_process_cfg_action_frm_tb_ppdu(tp_wma_handle wma,
				   struct cfg_action_frm_tb_ppdu *cfg_info)
{
	struct cfg_action_frm_tb_ppdu_param cmd = {0};

	if (!wma) {
		wma_err("WMA pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	cmd.frm_len = cfg_info->frm_len;
	cmd.cfg = cfg_info->cfg;
	cmd.data = cfg_info->data;

	wma_debug("cfg: %d, frm_len: %d", cfg_info->cfg, cfg_info->frm_len);

	return wmi_unified_cfg_action_frm_tb_ppdu_cmd(wma->wmi_handle, &cmd);
}


/*
 * wma_process_set_ie_info() - Function to send IE info to firmware
 * @wma:                Pointer to WMA handle
 * @ie_data:       Pointer for ie data
 *
 * This function sends IE information to firmware
 *
 * Return: QDF_STATUS_SUCCESS for success otherwise failure
 *
 */
QDF_STATUS wma_process_set_ie_info(tp_wma_handle wma,
				   struct vdev_ie_info *ie_info)
{
	struct wma_txrx_node *interface;
	struct vdev_ie_info_param cmd = {0};

	if (!ie_info || !wma) {
		wma_err("input pointer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/* Validate the input */
	if (ie_info->length  <= 0) {
		wma_err("Invalid IE length");
		return QDF_STATUS_E_INVAL;
	}

	if (!wma_is_vdev_valid(ie_info->vdev_id)) {
		wma_err("vdev_id: %d is not active", ie_info->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	interface = &wma->interfaces[ie_info->vdev_id];
	cmd.vdev_id = ie_info->vdev_id;
	cmd.ie_id = ie_info->ie_id;
	cmd.length = ie_info->length;
	cmd.band = ie_info->band;
	cmd.data = ie_info->data;
	cmd.ie_source = WMA_SET_VDEV_IE_SOURCE_HOST;

	wma_debug("vdev id: %d, ie_id: %d, band: %d, len: %d",
		 ie_info->vdev_id, ie_info->ie_id, ie_info->band,
		 ie_info->length);

	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_WMA, QDF_TRACE_LEVEL_DEBUG,
		ie_info->data, ie_info->length);

	return wmi_unified_process_set_ie_info_cmd(wma->wmi_handle, &cmd);
}

#ifdef FEATURE_WLAN_APF
/**
 *  wma_get_apf_caps_event_handler() - Event handler for get apf capability
 *  @handle: WMA global handle
 *  @cmd_param_info: command event data
 *  @len: Length of @cmd_param_info
 *
 *  Return: 0 on Success or Errno on failure
 */
int wma_get_apf_caps_event_handler(void *handle, u_int8_t *cmd_param_info,
				   u_int32_t len)
{
	WMI_BPF_CAPABILIY_INFO_EVENTID_param_tlvs  *param_buf;
	wmi_bpf_capability_info_evt_fixed_param *event;
	struct sir_apf_get_offload *apf_get_offload;
	struct mac_context *pmac = (struct mac_context *)cds_get_context(
				QDF_MODULE_ID_PE);

	if (!pmac) {
		wma_err("Invalid pmac");
		return -EINVAL;
	}
	if (!pmac->sme.apf_get_offload_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}

	param_buf = (WMI_BPF_CAPABILIY_INFO_EVENTID_param_tlvs *)cmd_param_info;
	event = param_buf->fixed_param;
	apf_get_offload = qdf_mem_malloc(sizeof(*apf_get_offload));
	if (!apf_get_offload)
		return -ENOMEM;

	apf_get_offload->apf_version = event->bpf_version;
	apf_get_offload->max_apf_filters = event->max_bpf_filters;
	apf_get_offload->max_bytes_for_apf_inst =
			event->max_bytes_for_bpf_inst;
	wma_debug("APF capabilities version: %d max apf filter size: %d",
		 apf_get_offload->apf_version,
		 apf_get_offload->max_bytes_for_apf_inst);

	wma_debug("sending apf capabilities event to hdd");
	pmac->sme.apf_get_offload_cb(pmac->sme.apf_get_offload_context,
				     apf_get_offload);
	qdf_mem_free(apf_get_offload);
	return 0;
}

QDF_STATUS wma_get_apf_capabilities(tp_wma_handle wma)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	wmi_bpf_get_capability_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t   len;
	u_int8_t *buf_ptr;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue get APF capab");
		return QDF_STATUS_E_INVAL;
	}

	if (!wmi_service_enabled(wma->wmi_handle, wmi_service_apf_offload)) {
		wma_err("APF cababilities feature bit not enabled");
		return QDF_STATUS_E_FAILURE;
	}

	len = sizeof(*cmd);
	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);
	cmd = (wmi_bpf_get_capability_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_bpf_get_capability_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
		wmi_bpf_get_capability_cmd_fixed_param));

	if (wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
				 WMI_BPF_GET_CAPABILITY_CMDID)) {
		wmi_buf_free(wmi_buf);
		return QDF_STATUS_E_FAILURE;
	}
	return status;
}

QDF_STATUS wma_set_apf_instructions(tp_wma_handle wma,
				    struct sir_apf_set_offload *apf_set_offload)
{
	wmi_bpf_set_vdev_instructions_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t   len = 0, len_aligned = 0;
	u_int8_t *buf_ptr;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue set APF capability");
		return QDF_STATUS_E_INVAL;
	}

	if (!wmi_service_enabled(wma->wmi_handle,
		wmi_service_apf_offload)) {
		wma_err("APF offload feature Disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	if (!apf_set_offload) {
		wma_err("Invalid APF instruction request");
		return QDF_STATUS_E_INVAL;
	}

	if (apf_set_offload->session_id >= wma->max_bssid) {
		wma_err("Invalid vdev_id: %d", apf_set_offload->session_id);
		return QDF_STATUS_E_INVAL;
	}

	if (!wma_is_vdev_up(apf_set_offload->session_id)) {
		wma_err("vdev %d is not up skipping APF offload",
			 apf_set_offload->session_id);
		return QDF_STATUS_E_INVAL;
	}

	if (apf_set_offload->total_length) {
		len_aligned = roundup(apf_set_offload->current_length,
					sizeof(A_UINT32));
		len = len_aligned + WMI_TLV_HDR_SIZE;
	}

	len += sizeof(*cmd);
	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);
	cmd = (wmi_bpf_set_vdev_instructions_cmd_fixed_param *) buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_bpf_set_vdev_instructions_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_bpf_set_vdev_instructions_cmd_fixed_param));
	cmd->vdev_id = apf_set_offload->session_id;
	cmd->filter_id = apf_set_offload->filter_id;
	cmd->total_length = apf_set_offload->total_length;
	cmd->current_offset = apf_set_offload->current_offset;
	cmd->current_length = apf_set_offload->current_length;

	if (apf_set_offload->total_length) {
		buf_ptr +=
			sizeof(wmi_bpf_set_vdev_instructions_cmd_fixed_param);
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, len_aligned);
		buf_ptr += WMI_TLV_HDR_SIZE;
		qdf_mem_copy(buf_ptr, apf_set_offload->program,
			     apf_set_offload->current_length);
	}

	if (wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
				 WMI_BPF_SET_VDEV_INSTRUCTIONS_CMDID)) {
		wmi_buf_free(wmi_buf);
		return QDF_STATUS_E_FAILURE;
	}
	wma_debug("APF offload enabled in fw");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_send_apf_enable_cmd(WMA_HANDLE handle, uint8_t vdev_id,
				   bool apf_enable)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma = (tp_wma_handle) handle;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue get APF capab");
		return QDF_STATUS_E_INVAL;
	}

	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
		WMI_SERVICE_BPF_OFFLOAD)) {
		wma_err("APF cababilities feature bit not enabled");
		return QDF_STATUS_E_FAILURE;
	}

	status = wmi_unified_send_apf_enable_cmd(wma->wmi_handle, vdev_id,
						 apf_enable);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to send apf enable/disable cmd");
		return QDF_STATUS_E_FAILURE;
	}

	if (apf_enable)
		wma_debug("Sent APF Enable on vdevid: %d", vdev_id);
	else
		wma_debug("Sent APF Disable on vdevid: %d", vdev_id);

	return status;
}

QDF_STATUS
wma_send_apf_write_work_memory_cmd(WMA_HANDLE handle,
				   struct wmi_apf_write_memory_params
								*write_params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma = (tp_wma_handle) handle;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue write APF mem");
		return QDF_STATUS_E_INVAL;
	}

	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
		WMI_SERVICE_BPF_OFFLOAD)) {
		wma_err("APF cababilities feature bit not enabled");
		return QDF_STATUS_E_FAILURE;
	}

	if (wmi_unified_send_apf_write_work_memory_cmd(wma->wmi_handle,
						       write_params)) {
		wma_err("Failed to send APF write mem command");
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("Sent APF wite mem on vdevid: %d", write_params->vdev_id);
	return status;
}

int wma_apf_read_work_memory_event_handler(void *handle, uint8_t *evt_buf,
					   uint32_t len)
{
	tp_wma_handle wma_handle;
	wmi_unified_t wmi_handle;
	struct wmi_apf_read_memory_resp_event_params evt_params = {0};
	QDF_STATUS status;
	struct mac_context *pmac = cds_get_context(QDF_MODULE_ID_PE);

	wma_debug("handle:%pK event:%pK len:%u", handle, evt_buf, len);

	wma_handle = handle;
	if (!wma_handle) {
		wma_err("NULL wma_handle");
		return -EINVAL;
	}

	wmi_handle = wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("NULL wmi_handle");
		return -EINVAL;
	}

	if (!pmac) {
		wma_err("Invalid pmac");
		return -EINVAL;
	}

	if (!pmac->sme.apf_read_mem_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}

	status = wmi_extract_apf_read_memory_resp_event(wmi_handle,
						evt_buf, &evt_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Event extract failure: %d", status);
		return -EINVAL;
	}

	pmac->sme.apf_read_mem_cb(pmac->hdd_handle, &evt_params);

	return 0;
}

QDF_STATUS wma_send_apf_read_work_memory_cmd(WMA_HANDLE handle,
					     struct wmi_apf_read_memory_params
								  *read_params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma = (tp_wma_handle) handle;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue read APF memory");
		return QDF_STATUS_E_INVAL;
	}

	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
		WMI_SERVICE_BPF_OFFLOAD)) {
		wma_err("APF cababilities feature bit not enabled");
		return QDF_STATUS_E_FAILURE;
	}

	if (wmi_unified_send_apf_read_work_memory_cmd(wma->wmi_handle,
						      read_params)) {
		wma_err("Failed to send APF read memory command");
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("Sent APF read memory on vdevid: %d", read_params->vdev_id);
	return status;
}
#endif /* FEATURE_WLAN_APF */

QDF_STATUS wma_set_tx_rx_aggr_size(uint8_t vdev_id,
				   uint32_t tx_size,
				   uint32_t rx_size,
				   wmi_vdev_custom_aggr_type_t aggr_type)
{
	tp_wma_handle wma_handle;
	struct wma_txrx_node *intr;
	wmi_vdev_set_custom_aggr_size_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int ret;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		wma_err("WMA context is invalid!");
		return QDF_STATUS_E_INVAL;
	}

	intr = wma_handle->interfaces;
	if (!intr) {
		wma_err("WMA interface is invalid!");
		return QDF_STATUS_E_INVAL;
	}

	if (aggr_type == WMI_VDEV_CUSTOM_AGGR_TYPE_AMPDU) {
		intr[vdev_id].config.tx_ampdu = tx_size;
		intr[vdev_id].config.rx_ampdu = rx_size;
	} else {
		intr[vdev_id].config.tx_amsdu = tx_size;
		intr[vdev_id].config.rx_amsdu = rx_size;
	}

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_vdev_set_custom_aggr_size_cmd_fixed_param *) buf_ptr;
	qdf_mem_zero(cmd, sizeof(*cmd));

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_vdev_set_custom_aggr_size_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_vdev_set_custom_aggr_size_cmd_fixed_param));

	if (wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_ampdu_tx_buf_size_256_support)) {
		cmd->enable_bitmap |= (0x1 << 6);
		if (!(tx_size <= ADDBA_TXAGGR_SIZE_LITHIUM)) {
			wma_err("Invalid AMPDU Size");
			return QDF_STATUS_E_INVAL;
		}
	} else if (tx_size == ADDBA_TXAGGR_SIZE_LITHIUM) {
		tx_size = ADDBA_TXAGGR_SIZE_HELIUM;
	} else if (!(tx_size <= ADDBA_TXAGGR_SIZE_HELIUM)) {
		wma_err("Invalid AMPDU Size");
		return QDF_STATUS_E_INVAL;
	}

	cmd->vdev_id = vdev_id;
	cmd->tx_aggr_size = tx_size;
	cmd->rx_aggr_size = rx_size;
	/* bit 2 (aggr_type): TX Aggregation Type (0=A-MPDU, 1=A-MSDU) */
	if (aggr_type == WMI_VDEV_CUSTOM_AGGR_TYPE_AMSDU)
		cmd->enable_bitmap |= 0x04;

	wma_debug("tx aggr: %d rx aggr: %d vdev: %d enable_bitmap %d",
		 cmd->tx_aggr_size, cmd->rx_aggr_size, cmd->vdev_id,
		 cmd->enable_bitmap);

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				WMI_VDEV_SET_CUSTOM_AGGR_SIZE_CMDID);
	if (ret) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_set_tx_rx_aggr_size_per_ac(WMA_HANDLE handle,
					  uint8_t vdev_id,
					  struct wlan_mlme_qos *qos_aggr,
					  wmi_vdev_custom_aggr_type_t aggr_type)
{
	wmi_vdev_set_custom_aggr_size_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int ret;
	int queue_num;
	uint32_t tx_aggr_size[4];
	tp_wma_handle wma_handle = (tp_wma_handle)handle;

	if (!wma_handle) {
		wma_err("WMA context is invald!");
		return QDF_STATUS_E_INVAL;
	}

	tx_aggr_size[0] = qos_aggr->tx_aggregation_size_be;
	tx_aggr_size[1] = qos_aggr->tx_aggregation_size_bk;
	tx_aggr_size[2] = qos_aggr->tx_aggregation_size_vi;
	tx_aggr_size[3] = qos_aggr->tx_aggregation_size_vo;

	for (queue_num = 0; queue_num < 4; queue_num++) {
		if (tx_aggr_size[queue_num] == 0)
			continue;

		len = sizeof(*cmd);
		buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
		if (!buf)
			return QDF_STATUS_E_NOMEM;

		buf_ptr = (u_int8_t *)wmi_buf_data(buf);
		cmd = (wmi_vdev_set_custom_aggr_size_cmd_fixed_param *)buf_ptr;
		qdf_mem_zero(cmd, sizeof(*cmd));

		WMITLV_SET_HDR(&cmd->tlv_header,
			       WMITLV_TAG_STRUC_wmi_vdev_set_custom_aggr_size_cmd_fixed_param,
			       WMITLV_GET_STRUCT_TLVLEN(
					wmi_vdev_set_custom_aggr_size_cmd_fixed_param));

		cmd->vdev_id = vdev_id;
		cmd->rx_aggr_size = qos_aggr->rx_aggregation_size;
		cmd->tx_aggr_size = tx_aggr_size[queue_num];

		if (wmi_service_enabled(wma_handle->wmi_handle,
					wmi_service_ampdu_tx_buf_size_256_support)) {
			cmd->enable_bitmap |= (0x1 << 6);
			if (!(tx_aggr_size[queue_num] <= ADDBA_TXAGGR_SIZE_LITHIUM)) {
				wma_err("Invalid AMPDU Size");
				return QDF_STATUS_E_INVAL;
			}
		} else if (tx_aggr_size[queue_num] == ADDBA_TXAGGR_SIZE_LITHIUM) {
			tx_aggr_size[queue_num] = ADDBA_TXAGGR_SIZE_HELIUM;
		} else if (!(tx_aggr_size[queue_num] <= ADDBA_TXAGGR_SIZE_HELIUM)) {
			wma_err("Invalid AMPDU Size");
			return QDF_STATUS_E_INVAL;
		}

		/* bit 5: tx_ac_enable, if set, ac bitmap is valid. */
		cmd->enable_bitmap |= 0x20 | queue_num;
		/* bit 2 (aggr_type): TX Aggregation Type (0=A-MPDU, 1=A-MSDU) */
		if (aggr_type == WMI_VDEV_CUSTOM_AGGR_TYPE_AMSDU)
			cmd->enable_bitmap |= 0x04;

		wma_debug("queue_num: %d, tx aggr: %d rx aggr: %d vdev: %d, bitmap: %d",
			 queue_num, cmd->tx_aggr_size,
			 cmd->rx_aggr_size, cmd->vdev_id,
			 cmd->enable_bitmap);

		ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
					WMI_VDEV_SET_CUSTOM_AGGR_SIZE_CMDID);
		if (ret) {
			wmi_buf_free(buf);
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS wma_set_sw_retry_by_qos(
	tp_wma_handle handle, uint8_t vdev_id,
	wmi_vdev_custom_sw_retry_type_t retry_type,
	wmi_traffic_ac ac_type,
	uint32_t sw_retry)
{
	wmi_vdev_set_custom_sw_retry_th_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int ret;

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(handle->wmi_handle, len);

	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (u_int8_t *)wmi_buf_data(buf);
	cmd = (wmi_vdev_set_custom_sw_retry_th_cmd_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_set_custom_sw_retry_th_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
		       wmi_vdev_set_custom_sw_retry_th_cmd_fixed_param));

	cmd->vdev_id = vdev_id;
	cmd->ac_type = ac_type;
	cmd->sw_retry_type = retry_type;
	cmd->sw_retry_th = sw_retry;

	wma_debug("ac_type: %d re_type: %d threshold: %d vid: %d",
		  cmd->ac_type, cmd->sw_retry_type,
		  cmd->sw_retry_th, cmd->vdev_id);

	ret = wmi_unified_cmd_send(handle->wmi_handle,
				   buf, len,
				   WMI_VDEV_SET_CUSTOM_SW_RETRY_TH_CMDID);

	if (ret) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_set_vdev_sw_retry_th(uint8_t vdev_id, uint8_t sw_retry_count,
				    wmi_vdev_custom_sw_retry_type_t retry_type)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma_handle;
	uint32_t queue_num;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle)
		return QDF_STATUS_E_FAILURE;


	for (queue_num = 0; queue_num < WMI_AC_MAX; queue_num++) {
		if (sw_retry_count == 0)
			continue;

		status = wma_set_sw_retry_by_qos(wma_handle,
						 vdev_id,
						 retry_type,
						 queue_num,
						 sw_retry_count);

		if (QDF_IS_STATUS_ERROR(status))
			return status;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_set_sw_retry_threshold_per_ac(WMA_HANDLE handle,
					     uint8_t vdev_id,
					     struct wlan_mlme_qos *qos_aggr)
{
	QDF_STATUS ret;
	int retry_type, queue_num;
	uint32_t tx_sw_retry[WMI_VDEV_CUSTOM_SW_RETRY_TYPE_MAX][WMI_AC_MAX];
	uint32_t sw_retry;
	tp_wma_handle wma_handle = (tp_wma_handle)handle;

	if (!wma_handle) {
		wma_err("WMA context is invalid!");
		return QDF_STATUS_E_INVAL;
	}

	tx_sw_retry[WMI_VDEV_CUSTOM_SW_RETRY_TYPE_AGGR][WMI_AC_BE] =
		qos_aggr->tx_aggr_sw_retry_threshold_be;
	tx_sw_retry[WMI_VDEV_CUSTOM_SW_RETRY_TYPE_AGGR][WMI_AC_BK] =
		qos_aggr->tx_aggr_sw_retry_threshold_bk;
	tx_sw_retry[WMI_VDEV_CUSTOM_SW_RETRY_TYPE_AGGR][WMI_AC_VI] =
		qos_aggr->tx_aggr_sw_retry_threshold_vi;
	tx_sw_retry[WMI_VDEV_CUSTOM_SW_RETRY_TYPE_AGGR][WMI_AC_VO] =
		qos_aggr->tx_aggr_sw_retry_threshold_vo;

	tx_sw_retry[WMI_VDEV_CUSTOM_SW_RETRY_TYPE_NONAGGR][WMI_AC_BE] =
		qos_aggr->tx_non_aggr_sw_retry_threshold_be;
	tx_sw_retry[WMI_VDEV_CUSTOM_SW_RETRY_TYPE_NONAGGR][WMI_AC_BK] =
		qos_aggr->tx_non_aggr_sw_retry_threshold_bk;
	tx_sw_retry[WMI_VDEV_CUSTOM_SW_RETRY_TYPE_NONAGGR][WMI_AC_VI] =
		qos_aggr->tx_non_aggr_sw_retry_threshold_vi;
	tx_sw_retry[WMI_VDEV_CUSTOM_SW_RETRY_TYPE_NONAGGR][WMI_AC_VO] =
		qos_aggr->tx_non_aggr_sw_retry_threshold_vo;

	retry_type = WMI_VDEV_CUSTOM_SW_RETRY_TYPE_NONAGGR;
	while (retry_type < WMI_VDEV_CUSTOM_SW_RETRY_TYPE_MAX) {
		for (queue_num = 0; queue_num < WMI_AC_MAX; queue_num++) {
			if (tx_sw_retry[retry_type][queue_num] == 0)
				continue;

			sw_retry = tx_sw_retry[retry_type][queue_num];
			ret = wma_set_sw_retry_by_qos(wma_handle,
						      vdev_id,
						      retry_type,
						      queue_num,
						      sw_retry);

			if (QDF_IS_STATUS_ERROR(ret))
				return ret;
		}
		retry_type++;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_set_sw_retry_threshold(uint8_t vdev_id, uint32_t retry,
				      uint32_t param_id)
{
	uint32_t max, min;
	uint32_t ret;

	if (param_id == WMI_PDEV_PARAM_AGG_SW_RETRY_TH) {
		max = cfg_max(CFG_TX_AGGR_SW_RETRY);
		min = cfg_min(CFG_TX_AGGR_SW_RETRY);
	} else {
		max = cfg_max(CFG_TX_NON_AGGR_SW_RETRY);
		min = cfg_min(CFG_TX_NON_AGGR_SW_RETRY);
	}

	retry = (retry > max) ? max : retry;
	retry = (retry < min) ? min : retry;

	ret = wma_cli_set_command(vdev_id, param_id, retry, PDEV_CMD);
	if (ret)
		return QDF_STATUS_E_IO;

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_process_fw_test_cmd() - send unit test command to fw.
 * @handle: wma handle
 * @wma_fwtest: fw test command
 *
 * This function send fw test command to fw.
 *
 * Return: none
 */
QDF_STATUS wma_process_fw_test_cmd(WMA_HANDLE handle,
				   struct set_fwtest_params *wma_fwtest)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;

	if (!wma_handle || !wma_handle->wmi_handle) {
		wma_err("WMA is closed, can not issue fw test cmd");
		return QDF_STATUS_E_FAILURE;
	}

	if (wmi_unified_fw_test_cmd(wma_handle->wmi_handle,
				    (struct set_fwtest_params *)wma_fwtest)) {
		wma_err("Failed to issue fw test cmd");
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_enable_disable_caevent_ind() - Issue WMI command to enable or
 * disable ca event indication
 * @wma: wma handler
 * @val: boolean value true or false
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_enable_disable_caevent_ind(tp_wma_handle wma, uint8_t val)
{
	WMI_CHAN_AVOID_RPT_ALLOW_CMD_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint8_t *buf_ptr;
	uint32_t len;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue set/clear CA");
		return QDF_STATUS_E_INVAL;
	}

	len = sizeof(*cmd);
	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *) wmi_buf_data(wmi_buf);
	cmd = (WMI_CHAN_AVOID_RPT_ALLOW_CMD_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_WMI_CHAN_AVOID_RPT_ALLOW_CMD_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
				WMI_CHAN_AVOID_RPT_ALLOW_CMD_fixed_param));
	cmd->rpt_allow = val;
	if (wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
				WMI_CHAN_AVOID_RPT_ALLOW_CMDID)) {
		wmi_buf_free(wmi_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static wma_sar_cb sar_callback;
static void *sar_context;

static int wma_sar_event_handler(void *handle, uint8_t *evt_buf, uint32_t len)
{
	tp_wma_handle wma_handle;
	wmi_unified_t wmi_handle;
	struct sar_limit_event *event;
	wma_sar_cb callback;
	QDF_STATUS status;

	wma_info("handle:%pK event:%pK len:%u", handle, evt_buf, len);

	wma_handle = handle;
	if (!wma_handle) {
		wma_err("NULL wma_handle");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("NULL wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	event = qdf_mem_malloc(sizeof(*event));
	if (!event)
		return QDF_STATUS_E_NOMEM;

	status = wmi_unified_extract_sar_limit_event(wmi_handle,
						     evt_buf, event);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Event extract failure: %d", status);
		qdf_mem_free(event);
		return QDF_STATUS_E_INVAL;
	}

	callback = sar_callback;
	sar_callback = NULL;
	if (callback)
		callback(sar_context, event);

	qdf_mem_free(event);

	return 0;
}

QDF_STATUS wma_sar_register_event_handlers(WMA_HANDLE handle)
{
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;

	if (!wma_handle) {
		wma_err("NULL wma_handle");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("NULL wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_register_event_handler(wmi_handle,
						  wmi_sar_get_limits_event_id,
						  wma_sar_event_handler,
						  WMA_RX_WORK_CTX);
}

QDF_STATUS wma_get_sar_limit(WMA_HANDLE handle,
			     wma_sar_cb callback, void *context)
{
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;
	QDF_STATUS status;

	if (!wma_handle) {
		wma_err("NULL wma_handle");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("NULL wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	sar_callback = callback;
	sar_context = context;
	status = wmi_unified_get_sar_limit_cmd(wmi_handle);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("wmi_unified_get_sar_limit_cmd() error: %u",
			 status);
		sar_callback = NULL;
	}

	return status;
}

QDF_STATUS wma_set_sar_limit(WMA_HANDLE handle,
		struct sar_limit_cmd_params *sar_limit_params)
{
	int ret;
	tp_wma_handle wma = (tp_wma_handle) handle;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue set sar limit msg");
		return QDF_STATUS_E_INVAL;
	}

	if (!sar_limit_params) {
		wma_err("set sar limit ptr NULL");
		return QDF_STATUS_E_INVAL;
	}

	ret = wmi_unified_send_sar_limit_cmd(wma->wmi_handle,
				sar_limit_params);

	return ret;
}

QDF_STATUS wma_send_coex_config_cmd(WMA_HANDLE wma_handle,
				    struct coex_config_params *coex_cfg_params)
{
	tp_wma_handle wma = (tp_wma_handle)wma_handle;

	if (!wma || !wma->wmi_handle) {
		wma_err("WMA is closed, can not issue coex config command");
		return QDF_STATUS_E_INVAL;
	}

	if (!coex_cfg_params) {
		wma_err("coex cfg params ptr NULL");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_send_coex_config_cmd(wma->wmi_handle,
					       coex_cfg_params);
}

/**
 * wma_get_arp_stats_handler() - handle arp stats data
 * indicated by FW
 * @handle: wma context
 * @data: event buffer
 * @data len: length of event buffer
 *
 * Return: 0 on success
 */
int wma_get_arp_stats_handler(void *handle, uint8_t *data,
			uint32_t data_len)
{
	WMI_VDEV_GET_ARP_STAT_EVENTID_param_tlvs *param_buf;
	wmi_vdev_get_arp_stats_event_fixed_param *data_event;
	wmi_vdev_get_connectivity_check_stats *connect_stats_event;
	uint8_t *buf_ptr;
	struct rsp_stats rsp = {0};
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("Invalid mac context");
		return -EINVAL;
	}

	if (!mac->sme.get_arp_stats_cb) {
		wma_err("Callback not registered");
		return -EINVAL;
	}

	if (!data) {
		wma_err("invalid pointer");
		return -EINVAL;
	}
	param_buf = (WMI_VDEV_GET_ARP_STAT_EVENTID_param_tlvs *)data;
	if (!param_buf) {
		wma_err("Invalid get arp stats event");
		return -EINVAL;
	}
	data_event = param_buf->fixed_param;
	if (!data_event) {
		wma_err("Invalid get arp stats data event");
		return -EINVAL;
	}
	rsp.arp_req_enqueue = data_event->arp_req_enqueue;
	rsp.vdev_id = data_event->vdev_id;
	rsp.arp_req_tx_success = data_event->arp_req_tx_success;
	rsp.arp_req_tx_failure = data_event->arp_req_tx_failure;
	rsp.arp_rsp_recvd = data_event->arp_rsp_recvd;
	rsp.out_of_order_arp_rsp_drop_cnt =
		data_event->out_of_order_arp_rsp_drop_cnt;
	rsp.dad_detected = data_event->dad_detected;
	rsp.connect_status = data_event->connect_status;
	rsp.ba_session_establishment_status =
		data_event->ba_session_establishment_status;

	buf_ptr = (uint8_t *)data_event;
	buf_ptr = buf_ptr + sizeof(wmi_vdev_get_arp_stats_event_fixed_param) +
		  WMI_TLV_HDR_SIZE;
	connect_stats_event = (wmi_vdev_get_connectivity_check_stats *)buf_ptr;

	if (((connect_stats_event->tlv_header & 0xFFFF0000) >> 16 ==
	      WMITLV_TAG_STRUC_wmi_vdev_get_connectivity_check_stats)) {
		rsp.connect_stats_present = true;
		rsp.tcp_ack_recvd = connect_stats_event->tcp_ack_recvd;
		rsp.icmpv4_rsp_recvd = connect_stats_event->icmpv4_rsp_recvd;
		wma_debug("tcp_ack_recvd %d icmpv4_rsp_recvd %d",
			connect_stats_event->tcp_ack_recvd,
			connect_stats_event->icmpv4_rsp_recvd);
	}

	mac->sme.get_arp_stats_cb(mac->hdd_handle, &rsp,
				  mac->sme.get_arp_stats_context);

	return 0;
}

/**
 * wma_unified_power_debug_stats_event_handler() - WMA handler function to
 * handle Power stats event from firmware
 * @handle: Pointer to wma handle
 * @cmd_param_info: Pointer to Power stats event TLV
 * @len: Length of the cmd_param_info
 *
 * Return: 0 on success, error number otherwise
 */
 #ifdef WLAN_POWER_DEBUG
int wma_unified_power_debug_stats_event_handler(void *handle,
			uint8_t *cmd_param_info, uint32_t len)
{
	WMI_PDEV_CHIP_POWER_STATS_EVENTID_param_tlvs *param_tlvs;
	struct power_stats_response *power_stats_results;
	wmi_pdev_chip_power_stats_event_fixed_param *param_buf;
	uint32_t power_stats_len, stats_registers_len, *debug_registers;

	struct mac_context *mac = (struct mac_context *)cds_get_context(QDF_MODULE_ID_PE);

	param_tlvs =
		(WMI_PDEV_CHIP_POWER_STATS_EVENTID_param_tlvs *) cmd_param_info;

	param_buf = (wmi_pdev_chip_power_stats_event_fixed_param *)
		param_tlvs->fixed_param;
	if (!mac) {
		wma_debug("NULL mac ptr");
		return -EINVAL;
	}

	if (!param_buf) {
		wma_debug("NULL power stats event fixed param");
		return -EINVAL;
	}

	if (param_buf->num_debug_register > ((WMI_SVC_MSG_MAX_SIZE -
		sizeof(wmi_pdev_chip_power_stats_event_fixed_param)) /
		sizeof(uint32_t)) ||
	    param_buf->num_debug_register > param_tlvs->num_debug_registers) {
		wma_err("excess payload: LEN num_debug_register:%u",
			param_buf->num_debug_register);
		return -EINVAL;
	}
	debug_registers = param_tlvs->debug_registers;
	stats_registers_len =
		(sizeof(uint32_t) * param_buf->num_debug_register);
	power_stats_len = stats_registers_len + sizeof(*power_stats_results);
	power_stats_results = qdf_mem_malloc(power_stats_len);
	if (!power_stats_results)
		return -ENOMEM;

	wma_debug("Cumulative sleep time %d cumulative total on time %d deep sleep enter counter %d last deep sleep enter tstamp ts %d debug registers fmt %d num debug register %d",
			param_buf->cumulative_sleep_time_ms,
			param_buf->cumulative_total_on_time_ms,
			param_buf->deep_sleep_enter_counter,
			param_buf->last_deep_sleep_enter_tstamp_ms,
			param_buf->debug_register_fmt,
			param_buf->num_debug_register);

	power_stats_results->cumulative_sleep_time_ms
		= param_buf->cumulative_sleep_time_ms;
	power_stats_results->cumulative_total_on_time_ms
		= param_buf->cumulative_total_on_time_ms;
	power_stats_results->deep_sleep_enter_counter
		= param_buf->deep_sleep_enter_counter;
	power_stats_results->last_deep_sleep_enter_tstamp_ms
		= param_buf->last_deep_sleep_enter_tstamp_ms;
	power_stats_results->debug_register_fmt
		= param_buf->debug_register_fmt;
	power_stats_results->num_debug_register
		= param_buf->num_debug_register;

	power_stats_results->debug_registers
		= (uint32_t *)(power_stats_results + 1);

	qdf_mem_copy(power_stats_results->debug_registers,
			debug_registers, stats_registers_len);
	if (mac->sme.sme_power_debug_stats_callback)
		mac->sme.sme_power_debug_stats_callback(mac,
							power_stats_results);

	qdf_mem_free(power_stats_results);
	return 0;
}
#endif

#ifdef WLAN_FEATURE_BEACON_RECEPTION_STATS
int wma_unified_beacon_debug_stats_event_handler(void *handle,
						 uint8_t *cmd_param_info,
						 uint32_t len)
{
	WMI_VDEV_BCN_RECEPTION_STATS_EVENTID_param_tlvs *param_tlvs;
	struct bcn_reception_stats_rsp *bcn_reception_stats;
	wmi_vdev_bcn_recv_stats_fixed_param *param_buf;
	struct mac_context *mac =
			(struct mac_context *)cds_get_context(QDF_MODULE_ID_PE);

	param_tlvs =
	   (WMI_VDEV_BCN_RECEPTION_STATS_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_tlvs) {
		wma_err("Invalid stats event");
		return -EINVAL;
	}

	param_buf = (wmi_vdev_bcn_recv_stats_fixed_param *)
		param_tlvs->fixed_param;
	if (!param_buf || !mac || !mac->sme.beacon_stats_resp_callback) {
		wma_debug("NULL mac ptr or HDD callback is null");
		return -EINVAL;
	}

	if (!param_buf) {
		wma_debug("NULL beacon stats event fixed param");
		return -EINVAL;
	}

	bcn_reception_stats = qdf_mem_malloc(sizeof(*bcn_reception_stats));
	if (!bcn_reception_stats)
		return -ENOMEM;

	bcn_reception_stats->total_bcn_cnt = param_buf->total_bcn_cnt;
	bcn_reception_stats->total_bmiss_cnt = param_buf->total_bmiss_cnt;
	bcn_reception_stats->vdev_id = param_buf->vdev_id;

	wma_debug("Total beacon count %d total beacon miss count %d vdev_id %d",
		 param_buf->total_bcn_cnt,
		 param_buf->total_bmiss_cnt,
		 param_buf->vdev_id);

	qdf_mem_copy(bcn_reception_stats->bmiss_bitmap,
		     param_buf->bmiss_bitmap,
		     MAX_BCNMISS_BITMAP * sizeof(uint32_t));

	mac->sme.beacon_stats_resp_callback(bcn_reception_stats,
			mac->sme.beacon_stats_context);
	qdf_mem_free(bcn_reception_stats);
	return 0;
}
#else
int wma_unified_beacon_debug_stats_event_handler(void *handle,
						 uint8_t *cmd_param_info,
						  uint32_t len)
{
	return 0;
}
#endif

#if defined(CLD_PM_QOS) && defined(WLAN_FEATURE_LL_MODE)
int
wma_vdev_bcn_latency_event_handler(void *handle,
				   uint8_t *event_info,
				   uint32_t len)
{
	WMI_VDEV_BCN_LATENCY_EVENTID_param_tlvs *param_buf = NULL;
	wmi_vdev_bcn_latency_fixed_param *bcn_latency = NULL;
	struct mac_context *mac =
			(struct mac_context *)cds_get_context(QDF_MODULE_ID_PE);
	uint32_t latency_level;

	param_buf = (WMI_VDEV_BCN_LATENCY_EVENTID_param_tlvs *)event_info;
	if (!param_buf) {
		wma_err("Invalid bcn latency event");
		return -EINVAL;
	}

	bcn_latency = param_buf->fixed_param;
	if (!bcn_latency) {
		wma_debug("beacon latency event fixed param is NULL");
		return -EINVAL;
	}

	/* Map the latency value to the level which host expects
	 * 1 - normal, 2 - moderate, 3 - low, 4 - ultralow
	 */
	latency_level = bcn_latency->latency_level + 1;
	if (latency_level < 1 || latency_level > 4) {
		wma_debug("invalid beacon latency level value");
		return -EINVAL;
	}

	/* Call the registered sme callback */
	mac->sme.beacon_latency_event_cb(latency_level);

	return 0;
}
#endif

int wma_chan_info_event_handler(void *handle, uint8_t *event_buf,
				uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_CHAN_INFO_EVENTID_param_tlvs *param_buf;
	wmi_chan_info_event_fixed_param *event;
	struct scan_chan_info buf;
	struct mac_context *mac = NULL;
	struct lim_channel_status *channel_status;
	bool snr_monitor_enabled;
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE mode;

	if (wma && wma->cds_context)
		mac = (struct mac_context *)cds_get_context(QDF_MODULE_ID_PE);

	if (!mac) {
		wma_err("Invalid mac context");
		return -EINVAL;
	}

	param_buf = (WMI_CHAN_INFO_EVENTID_param_tlvs *)event_buf;
	if (!param_buf)  {
		wma_err("Invalid chan info event buffer");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	if (!event) {
		wma_err("Invalid fixed param");
		return -EINVAL;
	}

	snr_monitor_enabled = wlan_scan_is_snr_monitor_enabled(mac->psoc);
	if (snr_monitor_enabled && mac->chan_info_cb) {
		buf.tx_frame_count = event->tx_frame_cnt;
		buf.clock_freq = event->mac_clk_mhz;
		buf.cmd_flag = event->cmd_flags;
		buf.freq = event->freq;
		buf.noise_floor = event->noise_floor;
		buf.cycle_count = event->cycle_count;
		buf.rx_clear_count = event->rx_clear_count;
		mac->chan_info_cb(&buf);
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(wma->psoc, event->vdev_id,
						    WLAN_LEGACY_WMA_ID);

	if (!vdev) {
		wma_err("vdev not found for vdev %d", event->vdev_id);
		return -EINVAL;
	}
	mode = wlan_vdev_mlme_get_opmode(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_WMA_ID);

	if (mac->sap.acs_with_more_param && mode == QDF_SAP_MODE) {
		channel_status = qdf_mem_malloc(sizeof(*channel_status));
		if (!channel_status)
			return -ENOMEM;

		wma_debug("freq %d nf %d rxcnt %u cyccnt %u tx_r %d tx_t %d",
			  event->freq, event->noise_floor,
			  event->rx_clear_count, event->cycle_count,
			  event->chan_tx_pwr_range, event->chan_tx_pwr_tp);

		channel_status->channelfreq = event->freq;
		channel_status->noise_floor = event->noise_floor;
		channel_status->rx_clear_count =
			 event->rx_clear_count;
		channel_status->cycle_count = event->cycle_count;
		channel_status->chan_tx_pwr_range =
			 event->chan_tx_pwr_range;
		channel_status->chan_tx_pwr_throughput =
			 event->chan_tx_pwr_tp;
		channel_status->rx_frame_count =
			 event->rx_frame_count;
		channel_status->bss_rx_cycle_count =
			event->my_bss_rx_cycle_count;
		channel_status->rx_11b_mode_data_duration =
			event->rx_11b_mode_data_duration;
		channel_status->tx_frame_count = event->tx_frame_cnt;
		channel_status->mac_clk_mhz = event->mac_clk_mhz;
		channel_status->channel_id =
			cds_freq_to_chan(event->freq);
		channel_status->cmd_flags =
			event->cmd_flags;

		wma_send_msg(handle, WMA_RX_CHN_STATUS_EVENT,
			     (void *)channel_status, 0);
	}

	return 0;
}

int wma_rx_aggr_failure_event_handler(void *handle, u_int8_t *event_buf,
							u_int32_t len)
{
	WMI_REPORT_RX_AGGR_FAILURE_EVENTID_param_tlvs *param_buf;
	struct sir_sme_rx_aggr_hole_ind *rx_aggr_hole_event;
	wmi_rx_aggr_failure_event_fixed_param *rx_aggr_failure_info;
	wmi_rx_aggr_failure_info *hole_info;
	uint32_t i, alloc_len;
	struct mac_context *mac;

	mac = (struct mac_context *)cds_get_context(QDF_MODULE_ID_PE);
	if (!mac || !mac->sme.stats_ext2_cb) {
		wma_debug("NULL mac ptr or HDD callback is null");
		return -EINVAL;
	}

	param_buf = (WMI_REPORT_RX_AGGR_FAILURE_EVENTID_param_tlvs *)event_buf;
	if (!param_buf) {
		wma_err("Invalid stats ext event buf");
		return -EINVAL;
	}

	rx_aggr_failure_info = param_buf->fixed_param;
	hole_info = param_buf->failure_info;

	if (rx_aggr_failure_info->num_failure_info > ((WMI_SVC_MSG_MAX_SIZE -
	    sizeof(*rx_aggr_hole_event)) /
	    sizeof(rx_aggr_hole_event->hole_info_array[0]))) {
		wma_err("Excess data from WMI num_failure_info %d",
			rx_aggr_failure_info->num_failure_info);
		return -EINVAL;
	}

	alloc_len = sizeof(*rx_aggr_hole_event) +
		(rx_aggr_failure_info->num_failure_info)*
		sizeof(rx_aggr_hole_event->hole_info_array[0]);
	rx_aggr_hole_event = qdf_mem_malloc(alloc_len);
	if (!rx_aggr_hole_event)
		return -ENOMEM;

	rx_aggr_hole_event->hole_cnt = rx_aggr_failure_info->num_failure_info;
	if (rx_aggr_hole_event->hole_cnt > param_buf->num_failure_info) {
		wma_err("Invalid no of hole count: %d",
			rx_aggr_hole_event->hole_cnt);
		qdf_mem_free(rx_aggr_hole_event);
		return -EINVAL;
	}
	wma_debug("aggr holes_sum: %d\n",
		rx_aggr_failure_info->num_failure_info);
	for (i = 0; i < rx_aggr_hole_event->hole_cnt; i++) {
		rx_aggr_hole_event->hole_info_array[i] =
			hole_info->end_seq - hole_info->start_seq + 1;
		wma_nofl_debug("aggr_index: %d\tstart_seq: %d\tend_seq: %d\t"
			"hole_info: %d mpdu lost",
			i, hole_info->start_seq, hole_info->end_seq,
			rx_aggr_hole_event->hole_info_array[i]);
		hole_info++;
	}

	mac->sme.stats_ext2_cb(mac->hdd_handle, rx_aggr_hole_event);
	qdf_mem_free(rx_aggr_hole_event);

	return 0;
}

int wma_wlan_bt_activity_evt_handler(void *handle, uint8_t *event, uint32_t len)
{
	wmi_coex_bt_activity_event_fixed_param *fixed_param;
	WMI_WLAN_COEX_BT_ACTIVITY_EVENTID_param_tlvs *param_buf =
		(WMI_WLAN_COEX_BT_ACTIVITY_EVENTID_param_tlvs *)event;
	struct scheduler_msg sme_msg = {0};
	QDF_STATUS qdf_status;

	if (!param_buf) {
		wma_err("Invalid BT activity event buffer");
		return -EINVAL;
	}

	fixed_param = param_buf->fixed_param;
	if (!fixed_param) {
		wma_err("Invalid BT activity event fixed param buffer");
		return -EINVAL;
	}

	wma_info("Received BT activity event %u",
		fixed_param->coex_profile_evt);

	sme_msg.type = eWNI_SME_BT_ACTIVITY_INFO_IND;
	sme_msg.bodyptr = NULL;
	sme_msg.bodyval = fixed_param->coex_profile_evt;

	qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &sme_msg);
	if (QDF_IS_STATUS_ERROR(qdf_status))
		return -EINVAL;

	return 0;
}

int wma_pdev_div_info_evt_handler(void *handle, u_int8_t *event_buf,
	u_int32_t len)
{
	WMI_PDEV_DIV_RSSI_ANTID_EVENTID_param_tlvs *param_buf;
	wmi_pdev_div_rssi_antid_event_fixed_param *event;
	struct chain_rssi_result chain_rssi_result;
	u_int32_t i;
	u_int8_t macaddr[QDF_MAC_ADDR_SIZE];
	tp_wma_handle wma = (tp_wma_handle)handle;

	struct mac_context *pmac = (struct mac_context *)cds_get_context(
					QDF_MODULE_ID_PE);
	if (!pmac || !wma) {
		wma_err("Invalid pmac or wma");
		return -EINVAL;
	}

	if (!pmac->sme.get_chain_rssi_cb) {
		wma_err("Invalid get_chain_rssi_cb");
		return -EINVAL;
	}
	param_buf = (WMI_PDEV_DIV_RSSI_ANTID_EVENTID_param_tlvs *) event_buf;
	if (!param_buf) {
		wma_err("Invalid rssi antid event buffer");
		return -EINVAL;
	}

	event = param_buf->fixed_param;
	if (!event) {
		wma_err("Invalid fixed param");
		return -EINVAL;
	}

	if (event->num_chains_valid > CHAIN_MAX_NUM) {
		wma_err("Invalid num of chains");
		return -EINVAL;
	}

	qdf_mem_zero(&chain_rssi_result, sizeof(chain_rssi_result));

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&event->macaddr, macaddr);
	wma_debug("macaddr: " QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(macaddr));

	wma_debug("num_chains_valid: %d", event->num_chains_valid);
	chain_rssi_result.num_chains_valid = event->num_chains_valid;

	qdf_mem_copy(chain_rssi_result.chain_rssi, event->chain_rssi,
		     sizeof(event->chain_rssi));

	qdf_mem_copy(chain_rssi_result.chain_evm, event->chain_evm,
		     sizeof(event->chain_evm));

	qdf_mem_copy(chain_rssi_result.ant_id, event->ant_id,
		     sizeof(event->ant_id));

	for (i = 0; i < chain_rssi_result.num_chains_valid; i++) {
		wma_nofl_debug("chain_rssi: %d, chain_evm: %d,ant_id: %d",
			 chain_rssi_result.chain_rssi[i],
			 chain_rssi_result.chain_evm[i],
			 chain_rssi_result.ant_id[i]);

		if (!wmi_service_enabled(wma->wmi_handle,
					 wmi_service_hw_db2dbm_support)) {
			if (chain_rssi_result.chain_rssi[i] !=
			    WMA_INVALID_PER_CHAIN_SNR)
				chain_rssi_result.chain_rssi[i] +=
						WMA_TGT_NOISE_FLOOR_DBM;
			else
				chain_rssi_result.chain_rssi[i] =
						WMA_INVALID_PER_CHAIN_RSSI;
		}
	}

	pmac->sme.get_chain_rssi_cb(pmac->sme.get_chain_rssi_context,
				&chain_rssi_result);

	return 0;
}

int wma_vdev_obss_detection_info_handler(void *handle, uint8_t *event,
					 uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	struct wmi_obss_detect_info *obss_detection;
	QDF_STATUS status;

	if (!event) {
		wma_err("Invalid obss_detection_info event buffer");
		return -EINVAL;
	}

	obss_detection = qdf_mem_malloc(sizeof(*obss_detection));
	if (!obss_detection)
		return -ENOMEM;

	status = wmi_unified_extract_obss_detection_info(wma->wmi_handle,
							 event, obss_detection);

	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to extract obss info");
		qdf_mem_free(obss_detection);
		return -EINVAL;
	}

	if (!wma_is_vdev_valid(obss_detection->vdev_id)) {
		wma_err("Invalid vdev id %d", obss_detection->vdev_id);
		qdf_mem_free(obss_detection);
		return -EINVAL;
	}

	wma_send_msg(wma, WMA_OBSS_DETECTION_INFO, obss_detection, 0);

	return 0;
}

static void wma_send_set_key_rsp(uint8_t vdev_id, bool pairwise,
				 uint8_t key_index)
{
	tSetStaKeyParams *key_info_uc;
	tSetBssKeyParams *key_info_mc;
	struct wlan_crypto_key *crypto_key;
	struct wlan_objmgr_vdev *vdev;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct qdf_mac_addr bcast_mac = QDF_MAC_ADDR_BCAST_INIT;

	if (!wma) {
		wma_err("WMA context does not exist");
		return;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(wma->psoc,
						    vdev_id,
						    WLAN_LEGACY_WMA_ID);
	if (!vdev) {
		wma_err("VDEV object not found");
		return;
	}
	crypto_key = wlan_crypto_get_key(vdev, key_index);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_WMA_ID);
	if (!crypto_key) {
		wma_debug("crypto_key not found");
		return;
	}

	if (pairwise) {
		key_info_uc = qdf_mem_malloc(sizeof(*key_info_uc));
		if (!key_info_uc)
			return;
		key_info_uc->vdev_id = vdev_id;
		key_info_uc->status = QDF_STATUS_SUCCESS;
		key_info_uc->key[0].keyLength = crypto_key->keylen;
		qdf_mem_copy(&key_info_uc->macaddr, &crypto_key->macaddr,
			     QDF_MAC_ADDR_SIZE);
		wma_send_msg_high_priority(wma, WMA_SET_STAKEY_RSP,
					   key_info_uc, 0);
	} else {
		key_info_mc = qdf_mem_malloc(sizeof(*key_info_mc));
		if (!key_info_mc)
			return;
		key_info_mc->vdev_id = vdev_id;
		key_info_mc->status = QDF_STATUS_SUCCESS;
		key_info_mc->key[0].keyLength = crypto_key->keylen;
		qdf_mem_copy(&key_info_mc->macaddr, &bcast_mac,
			     QDF_MAC_ADDR_SIZE);
		wma_send_msg_high_priority(wma, WMA_SET_BSSKEY_RSP,
					   key_info_mc, 0);
	}
}

static uint32_t wma_cipher_to_cap(enum wlan_crypto_cipher_type cipher)
{
	switch (cipher) {
	case WLAN_CRYPTO_CIPHER_WEP:  return WLAN_CRYPTO_CAP_WEP;
	case WLAN_CRYPTO_CIPHER_WEP_40:  return WLAN_CRYPTO_CAP_WEP;
	case WLAN_CRYPTO_CIPHER_WEP_104:  return WLAN_CRYPTO_CAP_WEP;
	case WLAN_CRYPTO_CIPHER_AES_OCB:  return WLAN_CRYPTO_CAP_AES;
	case WLAN_CRYPTO_CIPHER_AES_CCM:  return WLAN_CRYPTO_CAP_AES;
	case WLAN_CRYPTO_CIPHER_AES_CCM_256:  return WLAN_CRYPTO_CAP_AES;
	case WLAN_CRYPTO_CIPHER_AES_GCM:  return WLAN_CRYPTO_CAP_AES;
	case WLAN_CRYPTO_CIPHER_AES_GCM_256:  return WLAN_CRYPTO_CAP_AES;
	case WLAN_CRYPTO_CIPHER_CKIP: return WLAN_CRYPTO_CAP_CKIP;
	case WLAN_CRYPTO_CIPHER_TKIP: return WLAN_CRYPTO_CAP_TKIP_MIC;
	case WLAN_CRYPTO_CIPHER_WAPI_SMS4: return WLAN_CRYPTO_CAP_WAPI_SMS4;
	case WLAN_CRYPTO_CIPHER_WAPI_GCM4: return WLAN_CRYPTO_CAP_WAPI_GCM4;
	case WLAN_CRYPTO_CIPHER_FILS_AEAD: return WLAN_CRYPTO_CAP_FILS_AEAD;
	default: return 0;
	}
}

void wma_set_peer_ucast_cipher(uint8_t *mac_addr,
			       enum wlan_crypto_cipher_type cipher)
{
	int32_t set_val = 0, cipher_cap;
	struct wlan_objmgr_peer *peer;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err("wma context is NULL");
		return;
	}

	peer = wlan_objmgr_get_peer(wma->psoc,
				    wlan_objmgr_pdev_get_pdev_id(wma->pdev),
				    mac_addr, WLAN_LEGACY_WMA_ID);
	if (!peer) {
		wma_err("Peer of peer_mac "QDF_MAC_ADDR_FMT" not found",
			QDF_MAC_ADDR_REF(mac_addr));
		return;
	}
	cipher_cap = wma_cipher_to_cap(cipher);
	MLME_SET_BIT(set_val, cipher_cap);
	wlan_crypto_set_peer_param(peer, WLAN_CRYPTO_PARAM_CIPHER_CAP,
				   set_val);
	set_val = 0;
	MLME_SET_BIT(set_val, cipher);
	wlan_crypto_set_peer_param(peer, WLAN_CRYPTO_PARAM_UCAST_CIPHER,
				   set_val);
	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);

	wma_debug("Set unicast cipher %x and cap %x for "QDF_MAC_ADDR_FMT,
		  1 << cipher, 1 << cipher_cap,
		  QDF_MAC_ADDR_REF(mac_addr));
}

void wma_update_set_key(uint8_t session_id, bool pairwise,
			uint8_t key_index,
			enum wlan_crypto_cipher_type cipher_type)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *iface;

	if (!wma) {
		wma_err("Invalid WMA context");
		return;
	}
	iface = &wma->interfaces[session_id];
	if (!iface) {
		wma_err("iface not found for session id %d", session_id);
		return;
	}

	if (iface)
		iface->is_waiting_for_key = false;

	if (!pairwise && iface) {
		/* Its GTK release the wake lock */
		wma_debug("Release set key wake lock");
		qdf_runtime_pm_allow_suspend(
				&iface->vdev_set_key_runtime_wakelock);
		wma_release_wakelock(&iface->vdev_set_key_wakelock);
	}

	wma_send_set_key_rsp(session_id, pairwise, key_index);
}

int wma_vdev_bss_color_collision_info_handler(void *handle,
					      uint8_t *event,
					      uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	struct wmi_obss_color_collision_info *obss_color_info;
	QDF_STATUS status;

	if (!event) {
		wma_err("Invalid obss_color_collision event buffer");
		return -EINVAL;
	}

	obss_color_info = qdf_mem_malloc(sizeof(*obss_color_info));
	if (!obss_color_info)
		return -ENOMEM;

	status = wmi_unified_extract_obss_color_collision_info(wma->wmi_handle,
							       event,
							       obss_color_info);

	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to extract obss color info");
		qdf_mem_free(obss_color_info);
		return -EINVAL;
	}

	if (!wma_is_vdev_valid(obss_color_info->vdev_id)) {
		wma_err("Invalid vdev id %d", obss_color_info->vdev_id);
		qdf_mem_free(obss_color_info);
		return -EINVAL;
	}

	wma_send_msg(wma, WMA_OBSS_COLOR_COLLISION_INFO, obss_color_info, 0);

	return 0;
}

#ifdef FEATURE_ANI_LEVEL_REQUEST
int wma_get_ani_level_evt_handler(void *handle, uint8_t *event_buf,
				  uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	struct wmi_host_ani_level_event *ani = NULL;
	uint32_t num_freqs = 0;
	QDF_STATUS status;
	struct mac_context *pmac;
	int ret = 0;

	pmac = (struct mac_context *)cds_get_context(QDF_MODULE_ID_PE);
	if (!pmac || !wma) {
		wma_err("Invalid pmac or wma");
		return -EINVAL;
	}

	status = wmi_unified_extract_ani_level(wma->wmi_handle, event_buf,
					       &ani, &num_freqs);

	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to extract ani level");
		return -EINVAL;
	}

	if (!pmac->ani_params.ani_level_cb) {
		wma_err("Invalid ani_level_cb");
		ret = -EINVAL;
		goto free;
	}

	pmac->ani_params.ani_level_cb(ani, num_freqs,
				      pmac->ani_params.context);

free:
	qdf_mem_free(ani);
	return ret;
}
#else
int wma_get_ani_level_evt_handler(void *handle, uint8_t *event_buf,
				  uint32_t len)
{
	return 0;
}
#endif

