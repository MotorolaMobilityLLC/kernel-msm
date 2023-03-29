/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_QCT_WMA_H
#define WLAN_QCT_WMA_H

#include "ani_global.h"

#include "wma_api.h"
#include "wma_tgt_cfg.h"
#include "i_cds_packet.h"

#define IS_FEATURE_SUPPORTED_BY_FW(feat_enum_value) \
				wma_get_fw_wlan_feat_caps(feat_enum_value)

#define DPU_FEEDBACK_UNPROTECTED_ERROR 0x0F

#define WMA_GET_RX_MAC_HEADER(pRxMeta) \
	(tpSirMacMgmtHdr)(((t_packetmeta *)pRxMeta)->mpdu_hdr_ptr)

#define WMA_GET_RX_MPDUHEADER3A(pRxMeta) \
	(tpSirMacDataHdr3a)(((t_packetmeta *)pRxMeta)->mpdu_hdr_ptr)

#define WMA_GET_RX_MPDU_HEADER_LEN(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->mpdu_hdr_len)

#define WMA_GET_RX_MPDU_LEN(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->mpdu_len)

#define WMA_GET_RX_PAYLOAD_LEN(pRxMeta)	\
	(((t_packetmeta *)pRxMeta)->mpdu_data_len)

#define WMA_GET_RX_TSF_DELTA(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->tsf_delta)

#define WMA_GET_RX_MAC_RATE_IDX(pRxMeta) 0

#define WMA_GET_RX_MPDU_DATA(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->mpdu_data_ptr)

#define WMA_GET_RX_UNKNOWN_UCAST(pRxMeta) 0

#define WMA_GET_RX_FREQ(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->frequency)

#define WMA_GET_RX_FT_DONE(pRxMeta) 0

#define WMA_GET_RX_DPU_FEEDBACK(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->dpuFeedback)

#define WMA_GET_RX_BEACON_SENT(pRxMeta) 0

#define WMA_GET_RX_TSF_LATER(pRxMeta) 0

#define WMA_GET_RX_TIMESTAMP(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->timestamp)

#define WMA_GET_OFFLOADSCANLEARN(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->offloadScanLearn)
#define WMA_GET_ROAMCANDIDATEIND(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->roamCandidateInd)
#define WMA_GET_SESSIONID(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->session_id)
#define WMA_GET_SCAN_SRC(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->scan_src)

#ifdef FEATURE_WLAN_EXTSCAN
#define WMA_IS_EXTSCAN_SCAN_SRC(pRxMeta) \
	((((t_packetmeta *)pRxMeta)->scan_src) == WMI_MGMT_RX_HDR_EXTSCAN)
#define WMA_IS_EPNO_SCAN_SRC(pRxMeta) \
	((((t_packetmeta *)pRxMeta)->scan_src) & WMI_MGMT_RX_HDR_ENLO)
#endif /* FEATURE_WLAN_EXTSCAN */

#define WMA_GET_RX_SNR(pRxMeta)	\
	(((t_packetmeta *)pRxMeta)->snr)

#define WMA_GET_RX_RFBAND(pRxMeta) 0

#define WMA_MAX_TXPOWER_INVALID        127
/* rssi value normalized to noise floor of -96 dBm */
#define WMA_GET_RX_RSSI_NORMALIZED(pRxMeta) \
		       (((t_packetmeta *)pRxMeta)->rssi)

/* raw rssi based on actual noise floor in hardware */
#define WMA_GET_RX_RSSI_RAW(pRxMeta) \
		       (((t_packetmeta *)pRxMeta)->rssi_raw)

/*
 * the repeat_cnt is reserved by FW team, the current value
 * is always 0xffffffff
 */
#define WMI_WOW_PULSE_REPEAT_CNT 0xffffffff


/* WMA Messages */
enum wmamsgtype {
	WMA_MSG_TYPES_BEGIN = SIR_HAL_MSG_TYPES_BEGIN,
	WMA_ITC_MSG_TYPES_BEGIN = SIR_HAL_ITC_MSG_TYPES_BEGIN,
	WMA_RADAR_DETECTED_IND = SIR_HAL_RADAR_DETECTED_IND,

	WMA_ADD_STA_REQ = SIR_HAL_ADD_STA_REQ,
	WMA_ADD_STA_RSP = SIR_HAL_ADD_STA_RSP,
	WMA_DELETE_STA_REQ = SIR_HAL_DELETE_STA_REQ,
	WMA_DELETE_STA_RSP =  SIR_HAL_DELETE_STA_RSP,
	WMA_ADD_BSS_REQ = SIR_HAL_ADD_BSS_REQ,
	WMA_DELETE_BSS_REQ = SIR_HAL_DELETE_BSS_REQ,
	WMA_DELETE_BSS_HO_FAIL_REQ = SIR_HAL_DELETE_BSS_HO_FAIL_REQ,
	WMA_DELETE_BSS_RSP = SIR_HAL_DELETE_BSS_RSP,
	WMA_DELETE_BSS_HO_FAIL_RSP = SIR_HAL_DELETE_BSS_HO_FAIL_RSP,
	WMA_SEND_BEACON_REQ = SIR_HAL_SEND_BEACON_REQ,
	WMA_SEND_BCN_RSP = SIR_HAL_SEND_BCN_RSP,
	WMA_SEND_PROBE_RSP_TMPL = SIR_HAL_SEND_PROBE_RSP_TMPL,
	WMA_ROAM_BLACLIST_MSG = SIR_HAL_ROAM_BLACKLIST_MSG,
	WMA_SEND_PEER_UNMAP_CONF = SIR_HAL_SEND_PEER_UNMAP_CONF,

	WMA_SET_BSSKEY_RSP = SIR_HAL_SET_BSSKEY_RSP,
	WMA_SET_STAKEY_RSP = SIR_HAL_SET_STAKEY_RSP,
	WMA_UPDATE_EDCA_PROFILE_IND = SIR_HAL_UPDATE_EDCA_PROFILE_IND,

	WMA_UPDATE_BEACON_IND = SIR_HAL_UPDATE_BEACON_IND,
	WMA_CHNL_SWITCH_REQ = SIR_HAL_CHNL_SWITCH_REQ,
	WMA_ADD_TS_REQ = SIR_HAL_ADD_TS_REQ,
	WMA_DEL_TS_REQ = SIR_HAL_DEL_TS_REQ,

	WMA_MISSED_BEACON_IND = SIR_HAL_MISSED_BEACON_IND,

	WMA_SWITCH_CHANNEL_RSP = SIR_HAL_SWITCH_CHANNEL_RSP,
	WMA_P2P_NOA_ATTR_IND = SIR_HAL_P2P_NOA_ATTR_IND,
	WMA_PWR_SAVE_CFG = SIR_HAL_PWR_SAVE_CFG,

	WMA_IBSS_STA_ADD = SIR_HAL_IBSS_STA_ADD,
	WMA_TIMER_ADJUST_ADAPTIVE_THRESHOLD_IND =
				SIR_HAL_TIMER_ADJUST_ADAPTIVE_THRESHOLD_IND,
	WMA_SET_LINK_STATE = SIR_HAL_SET_LINK_STATE,
	WMA_SET_STA_BCASTKEY_RSP = SIR_HAL_SET_STA_BCASTKEY_RSP,
	WMA_ADD_TS_RSP = SIR_HAL_ADD_TS_RSP,
	WMA_DPU_MIC_ERROR = SIR_HAL_DPU_MIC_ERROR,
	WMA_TIMER_CHIP_MONITOR_TIMEOUT = SIR_HAL_TIMER_CHIP_MONITOR_TIMEOUT,
	WMA_TIMER_TRAFFIC_ACTIVITY_REQ = SIR_HAL_TIMER_TRAFFIC_ACTIVITY_REQ,
	WMA_TIMER_ADC_RSSI_STATS = SIR_HAL_TIMER_ADC_RSSI_STATS,
	WMA_TIMER_TRAFFIC_STATS_IND = SIR_HAL_TRAFFIC_STATS_IND,
#ifdef WLAN_FEATURE_11W
	WMA_EXCLUDE_UNENCRYPTED_IND = SIR_HAL_EXCLUDE_UNENCRYPTED_IND,
#endif

#ifdef FEATURE_WLAN_ESE
	WMA_TSM_STATS_REQ = SIR_HAL_TSM_STATS_REQ,
	WMA_TSM_STATS_RSP = SIR_HAL_TSM_STATS_RSP,
#endif

	WMA_ROAM_SCAN_CH_REQ = SIR_HAL_ROAM_SCAN_CH_REQ,

	WMA_HT40_OBSS_SCAN_IND = SIR_HAL_HT40_OBSS_SCAN_IND,

	WMA_SET_MIMOPS_REQ = SIR_HAL_SET_MIMOPS_REQ,
	WMA_SET_MIMOPS_RSP = SIR_HAL_SET_MIMOPS_RSP,
	WMA_SYS_READY_IND =  SIR_HAL_SYS_READY_IND,
	WMA_SET_TX_POWER_REQ = SIR_HAL_SET_TX_POWER_REQ,
	WMA_SET_TX_POWER_RSP = SIR_HAL_SET_TX_POWER_RSP,
	WMA_GET_TX_POWER_REQ = SIR_HAL_GET_TX_POWER_REQ,

	WMA_ENABLE_UAPSD_REQ = SIR_HAL_ENABLE_UAPSD_REQ,
	WMA_DISABLE_UAPSD_REQ = SIR_HAL_DISABLE_UAPSD_REQ,

	WMA_SET_KEY_DONE = SIR_HAL_SET_KEY_DONE,


	/* PE <-> HAL BTC messages */
	WMA_BTC_SET_CFG = SIR_HAL_BTC_SET_CFG,
	WMA_HANDLE_FW_MBOX_RSP = SIR_HAL_HANDLE_FW_MBOX_RSP,

	WMA_SET_MAX_TX_POWER_REQ = SIR_HAL_SET_MAX_TX_POWER_REQ,
	WMA_SET_MAX_TX_POWER_RSP = SIR_HAL_SET_MAX_TX_POWER_RSP,
	WMA_SET_DTIM_PERIOD = SIR_HAL_SET_DTIM_PERIOD,

	WMA_SET_MAX_TX_POWER_PER_BAND_REQ =
	SIR_HAL_SET_MAX_TX_POWER_PER_BAND_REQ,

	/* PE <-> HAL Host Offload message */
	WMA_SET_HOST_OFFLOAD = SIR_HAL_SET_HOST_OFFLOAD,

	/* PE <-> HAL Keep Alive message */
	WMA_SET_KEEP_ALIVE = SIR_HAL_SET_KEEP_ALIVE,

#ifdef WLAN_NS_OFFLOAD
	WMA_SET_NS_OFFLOAD = SIR_HAL_SET_NS_OFFLOAD,
#endif /* WLAN_NS_OFFLOAD */

#ifdef FEATURE_WLAN_TDLS
	WMA_SET_TDLS_LINK_ESTABLISH_REQ = SIR_HAL_TDLS_LINK_ESTABLISH_REQ,
	WMA_SET_TDLS_LINK_ESTABLISH_REQ_RSP =
				SIR_HAL_TDLS_LINK_ESTABLISH_REQ_RSP,
#endif

	WMA_WLAN_SUSPEND_IND = SIR_HAL_WLAN_SUSPEND_IND,
	WMA_WLAN_RESUME_REQ = SIR_HAL_WLAN_RESUME_REQ,
	WMA_MSG_TYPES_END = SIR_HAL_MSG_TYPES_END,

	WMA_AGGR_QOS_REQ = SIR_HAL_AGGR_QOS_REQ,
	WMA_AGGR_QOS_RSP = SIR_HAL_AGGR_QOS_RSP,

	WMA_CSA_OFFLOAD_EVENT = SIR_CSA_OFFLOAD_EVENT,

#ifdef FEATURE_WLAN_ESE
	WMA_SET_PLM_REQ = SIR_HAL_SET_PLM_REQ,
#endif

	WMA_ROAM_PRE_AUTH_STATUS = SIR_HAL_ROAM_PRE_AUTH_STATUS_IND,

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	WMA_ROAM_OFFLOAD_SYNCH_IND = SIR_HAL_ROAM_OFFLOAD_SYNCH_IND,
	WMA_ROAM_OFFLOAD_SYNCH_FAIL = SIR_HAL_ROAM_OFFLOAD_SYNCH_FAIL,
#endif

	WMA_8023_MULTICAST_LIST_REQ = SIR_HAL_8023_MULTICAST_LIST_REQ,

#ifdef WLAN_FEATURE_PACKET_FILTERING
	WMA_RECEIVE_FILTER_SET_FILTER_REQ =
				SIR_HAL_RECEIVE_FILTER_SET_FILTER_REQ,
	WMA_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ =
			SIR_HAL_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ,
	WMA_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP =
			SIR_HAL_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP,
	WMA_RECEIVE_FILTER_CLEAR_FILTER_REQ =
				SIR_HAL_RECEIVE_FILTER_CLEAR_FILTER_REQ,
#endif /* WLAN_FEATURE_PACKET_FILTERING */

	WMA_DHCP_START_IND = SIR_HAL_DHCP_START_IND,
	WMA_DHCP_STOP_IND = SIR_HAL_DHCP_STOP_IND,

#ifdef WLAN_FEATURE_GTK_OFFLOAD
	WMA_GTK_OFFLOAD_REQ = SIR_HAL_GTK_OFFLOAD_REQ,
	WMA_GTK_OFFLOAD_GETINFO_REQ = SIR_HAL_GTK_OFFLOAD_GETINFO_REQ,
	WMA_GTK_OFFLOAD_GETINFO_RSP = SIR_HAL_GTK_OFFLOAD_GETINFO_RSP,
#endif /* WLAN_FEATURE_GTK_OFFLOAD */

	WMA_SET_TM_LEVEL_REQ = SIR_HAL_SET_TM_LEVEL_REQ,

	WMA_UPDATE_OP_MODE = SIR_HAL_UPDATE_OP_MODE,
	WMA_UPDATE_RX_NSS = SIR_HAL_UPDATE_RX_NSS,
	WMA_UPDATE_MEMBERSHIP = SIR_HAL_UPDATE_MEMBERSHIP,
	WMA_UPDATE_USERPOS = SIR_HAL_UPDATE_USERPOS,

#ifdef WLAN_FEATURE_NAN
	WMA_NAN_REQUEST = SIR_HAL_NAN_REQUEST,
#endif

	WMA_START_SCAN_OFFLOAD_REQ = SIR_HAL_START_SCAN_OFFLOAD_REQ,
	WMA_STOP_SCAN_OFFLOAD_REQ = SIR_HAL_STOP_SCAN_OFFLOAD_REQ,
	WMA_UPDATE_CHAN_LIST_REQ = SIR_HAL_UPDATE_CHAN_LIST_REQ,
	WMA_RX_SCAN_EVENT = SIR_HAL_RX_SCAN_EVENT,
	WMA_RX_CHN_STATUS_EVENT = SIR_HAL_RX_CHN_STATUS_EVENT,

	WMA_CLI_SET_CMD = SIR_HAL_CLI_SET_CMD,

#ifndef REMOVE_PKT_LOG
	WMA_PKTLOG_ENABLE_REQ = SIR_HAL_PKTLOG_ENABLE_REQ,
#endif

#ifdef FEATURE_WLAN_LPHB
	WMA_LPHB_CONF_REQ =  SIR_HAL_LPHB_CONF_IND,
#endif /* FEATURE_WLAN_LPHB */

#ifdef FEATURE_WLAN_CH_AVOID
	WMA_CH_AVOID_UPDATE_REQ = SIR_HAL_CH_AVOID_UPDATE_REQ,
#endif /* FEATURE_WLAN_CH_AVOID */

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	WMA_SET_AUTO_SHUTDOWN_TIMER_REQ = SIR_HAL_SET_AUTO_SHUTDOWN_TIMER_REQ,
#endif

	WMA_ADD_PERIODIC_TX_PTRN_IND = SIR_HAL_ADD_PERIODIC_TX_PTRN_IND,
	WMA_DEL_PERIODIC_TX_PTRN_IND = SIR_HAL_DEL_PERIODIC_TX_PTRN_IND,

	WMA_TX_POWER_LIMIT = SIR_HAL_SET_TX_POWER_LIMIT,

	WMA_RATE_UPDATE_IND = SIR_HAL_RATE_UPDATE_IND,

	WMA_SEND_ADDBA_REQ = SIR_HAL_SEND_ADDBA_REQ,
	WMA_INIT_THERMAL_INFO_CMD = SIR_HAL_INIT_THERMAL_INFO_CMD,
	WMA_SET_THERMAL_LEVEL = SIR_HAL_SET_THERMAL_LEVEL,

	WMA_INIT_BAD_PEER_TX_CTL_INFO_CMD = SIR_HAL_BAD_PEER_TX_CTL_INI_CMD,

#ifdef FEATURE_WLAN_TDLS
	WMA_UPDATE_TDLS_PEER_STATE = SIR_HAL_UPDATE_TDLS_PEER_STATE,
	WMA_TDLS_SHOULD_DISCOVER_CMD = SIR_HAL_TDLS_SHOULD_DISCOVER,
	WMA_TDLS_SHOULD_TEARDOWN_CMD = SIR_HAL_TDLS_SHOULD_TEARDOWN,
	WMA_TDLS_PEER_DISCONNECTED_CMD = SIR_HAL_TDLS_PEER_DISCONNECTED,
#endif
	WMA_SET_SAP_INTRABSS_DIS = SIR_HAL_SET_SAP_INTRABSS_DIS,

/* Message to indicate beacon tx completion after beacon template update
 * beacon offload case
 */
	WMA_DFS_BEACON_TX_SUCCESS_IND = SIR_HAL_BEACON_TX_SUCCESS_IND,
	WMA_DISASSOC_TX_COMP = SIR_HAL_DISASSOC_TX_COMP,
	WMA_DEAUTH_TX_COMP = SIR_HAL_DEAUTH_TX_COMP,

	WMA_GET_ISOLATION = SIR_HAL_GET_ISOLATION,

	WMA_MODEM_POWER_STATE_IND = SIR_HAL_MODEM_POWER_STATE_IND,

#ifdef WLAN_FEATURE_STATS_EXT
	WMA_STATS_EXT_REQUEST = SIR_HAL_STATS_EXT_REQUEST,
#endif

	WMA_GET_TEMPERATURE_REQ = SIR_HAL_GET_TEMPERATURE_REQ,
	WMA_SET_WISA_PARAMS = SIR_HAL_SET_WISA_PARAMS,

#ifdef FEATURE_WLAN_EXTSCAN
	WMA_EXTSCAN_GET_CAPABILITIES_REQ = SIR_HAL_EXTSCAN_GET_CAPABILITIES_REQ,
	WMA_EXTSCAN_START_REQ = SIR_HAL_EXTSCAN_START_REQ,
	WMA_EXTSCAN_STOP_REQ = SIR_HAL_EXTSCAN_STOP_REQ,
	WMA_EXTSCAN_SET_BSSID_HOTLIST_REQ =
				SIR_HAL_EXTSCAN_SET_BSS_HOTLIST_REQ,
	WMA_EXTSCAN_RESET_BSSID_HOTLIST_REQ =
				SIR_HAL_EXTSCAN_RESET_BSS_HOTLIST_REQ,
	WMA_EXTSCAN_SET_SIGNF_CHANGE_REQ = SIR_HAL_EXTSCAN_SET_SIGNF_CHANGE_REQ,
	WMA_EXTSCAN_RESET_SIGNF_CHANGE_REQ =
				SIR_HAL_EXTSCAN_RESET_SIGNF_CHANGE_REQ,
	WMA_EXTSCAN_GET_CACHED_RESULTS_REQ =
				SIR_HAL_EXTSCAN_GET_CACHED_RESULTS_REQ,
	WMA_SET_EPNO_LIST_REQ = SIR_HAL_SET_EPNO_LIST_REQ,
	WMA_SET_PASSPOINT_LIST_REQ = SIR_HAL_SET_PASSPOINT_LIST_REQ,
	WMA_RESET_PASSPOINT_LIST_REQ = SIR_HAL_RESET_PASSPOINT_LIST_REQ,
#endif  /* FEATURE_WLAN_EXTSCAN */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
	WMA_LINK_LAYER_STATS_CLEAR_REQ = SIR_HAL_LL_STATS_CLEAR_REQ,
	WMA_LINK_LAYER_STATS_SET_REQ = SIR_HAL_LL_STATS_SET_REQ,
	WMA_LINK_LAYER_STATS_GET_REQ = SIR_HAL_LL_STATS_GET_REQ,
	WMA_LINK_LAYER_STATS_RESULTS_RSP = SIR_HAL_LL_STATS_RESULTS_RSP,
	WDA_LINK_LAYER_STATS_SET_THRESHOLD = SIR_HAL_LL_STATS_EXT_SET_THRESHOLD,
#endif  /* WLAN_FEATURE_LINK_LAYER_STATS */

	WMA_LINK_STATUS_GET_REQ = SIR_HAL_LINK_STATUS_GET_REQ,

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
	WMA_WLAN_EXT_WOW = SIR_HAL_CONFIG_EXT_WOW,
	WMA_WLAN_SET_APP_TYPE1_PARAMS = SIR_HAL_CONFIG_APP_TYPE1_PARAMS,
	WMA_WLAN_SET_APP_TYPE2_PARAMS = SIR_HAL_CONFIG_APP_TYPE2_PARAMS,
#endif

	WMA_SET_SCAN_MAC_OUI_REQ = SIR_HAL_SET_SCAN_MAC_OUI_REQ,
	WMA_TSF_GPIO_PIN = SIR_HAL_TSF_GPIO_PIN_REQ,

#ifdef DHCP_SERVER_OFFLOAD
	WMA_SET_DHCP_SERVER_OFFLOAD_CMD = SIR_HAL_SET_DHCP_SERVER_OFFLOAD,
#endif  /* DHCP_SERVER_OFFLOAD */

#ifdef WLAN_FEATURE_GPIO_LED_FLASHING
	WMA_LED_FLASHING_REQ = SIR_HAL_LED_FLASHING_REQ,
#endif

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	WMA_UPDATE_Q2Q_IE_IND = SIR_HAL_UPDATE_Q2Q_IE_IND,
#endif  /* FEATURE_AP_MCC_CH_AVOIDANCE */
	WMA_SET_RSSI_MONITOR_REQ = SIR_HAL_SET_RSSI_MONITOR_REQ,

	WMA_OCB_SET_CONFIG_CMD = SIR_HAL_OCB_SET_CONFIG_CMD,
	WMA_OCB_SET_UTC_TIME_CMD = SIR_HAL_OCB_SET_UTC_TIME_CMD,
	WMA_OCB_START_TIMING_ADVERT_CMD = SIR_HAL_OCB_START_TIMING_ADVERT_CMD,
	WMA_OCB_STOP_TIMING_ADVERT_CMD = SIR_HAL_OCB_STOP_TIMING_ADVERT_CMD,
	WMA_OCB_GET_TSF_TIMER_CMD = SIR_HAL_OCB_GET_TSF_TIMER_CMD,
	WMA_DCC_GET_STATS_CMD = SIR_HAL_DCC_GET_STATS_CMD,
	WMA_DCC_CLEAR_STATS_CMD = SIR_HAL_DCC_CLEAR_STATS_CMD,
	WMA_DCC_UPDATE_NDL_CMD = SIR_HAL_DCC_UPDATE_NDL_CMD,
	WMA_SET_IE_INFO = SIR_HAL_SET_IE_INFO,

	WMA_LRO_CONFIG_CMD = SIR_HAL_LRO_CONFIG_CMD,
	WMA_GW_PARAM_UPDATE_REQ = SIR_HAL_GATEWAY_PARAM_UPDATE_REQ,
	WMA_ADD_BCN_FILTER_CMDID = SIR_HAL_ADD_BCN_FILTER_CMDID,
	WMA_REMOVE_BCN_FILTER_CMDID = SIR_HAL_REMOVE_BCN_FILTER_CMDID,
	WMA_SET_ADAPT_DWELLTIME_CONF_PARAMS =
					SIR_HAL_SET_ADAPT_DWELLTIME_PARAMS,

	WDA_APF_GET_CAPABILITIES_REQ = SIR_HAL_APF_GET_CAPABILITIES_REQ,
	WMA_ROAM_SYNC_TIMEOUT = SIR_HAL_WMA_ROAM_SYNC_TIMEOUT,

	WMA_SET_PDEV_IE_REQ = SIR_HAL_SET_PDEV_IE_REQ,
	WMA_SEND_FREQ_RANGE_CONTROL_IND = SIR_HAL_SEND_FREQ_RANGE_CONTROL_IND,
	WMA_POWER_DEBUG_STATS_REQ = SIR_HAL_POWER_DEBUG_STATS_REQ,
	WMA_BEACON_DEBUG_STATS_REQ = SIR_HAL_BEACON_DEBUG_STATS_REQ,
	WMA_GET_RCPI_REQ = SIR_HAL_GET_RCPI_REQ,
	WMA_ROAM_BLACKLIST_MSG = SIR_HAL_ROAM_BLACKLIST_MSG,
	WMA_SET_DBS_SCAN_SEL_CONF_PARAMS = SIR_HAL_SET_DBS_SCAN_SEL_PARAMS,

	WMA_SET_WOW_PULSE_CMD = SIR_HAL_SET_WOW_PULSE_CMD,

	WMA_SEND_AP_VDEV_UP = SIR_HAL_SEND_AP_VDEV_UP,

	WMA_SET_ARP_STATS_REQ = SIR_HAL_SET_ARP_STATS_REQ,
	WMA_GET_ARP_STATS_REQ = SIR_HAL_GET_ARP_STATS_REQ,
	WMA_SET_LIMIT_OFF_CHAN = SIR_HAL_SET_LIMIT_OFF_CHAN,
	WMA_OBSS_DETECTION_REQ = SIR_HAL_OBSS_DETECTION_REQ,
	WMA_OBSS_DETECTION_INFO = SIR_HAL_OBSS_DETECTION_INFO,
	WMA_INVOKE_NEIGHBOR_REPORT = SIR_HAL_INVOKE_NEIGHBOR_REPORT,
	WMA_OBSS_COLOR_COLLISION_REQ = SIR_HAL_OBSS_COLOR_COLLISION_REQ,
	WMA_OBSS_COLOR_COLLISION_INFO = SIR_HAL_OBSS_COLOR_COLLISION_INFO,
	WMA_CFG_VENDOR_ACTION_TB_PPDU = SIR_HAL_CFG_VENDOR_ACTION_TB_PPDU,
	WMA_GET_ROAM_SCAN_STATS = SIR_HAL_GET_ROAM_SCAN_STATS,

#ifdef WLAN_FEATURE_MOTION_DETECTION
	WMA_SET_MOTION_DET_CONFIG = SIR_HAL_SET_MOTION_DET_CONFIG,
	WMA_SET_MOTION_DET_ENABLE = SIR_HAL_SET_MOTION_DET_ENABLE,
	WMA_SET_MOTION_DET_BASE_LINE_CONFIG =
				SIR_HAL_SET_MOTION_DET_BASE_LINE_CONFIG,
	WMA_SET_MOTION_DET_BASE_LINE_ENABLE =
				SIR_HAL_SET_MOTION_DET_BASE_LINE_ENABLE,
#endif  /* WLAN_FEATURE_MOTION_DETECTION */

#ifdef FW_THERMAL_THROTTLE_SUPPORT
	WMA_SET_THERMAL_THROTTLE_CFG = SIR_HAL_SET_THERMAL_THROTTLE_CFG,
	WMA_SET_THERMAL_MGMT = SIR_HAL_SET_THERMAL_MGMT,
#endif  /*FW_THERMAL_THROTTLE_SUPPORT */

#ifdef WLAN_MWS_INFO_DEBUGFS
	WMA_GET_MWS_COEX_INFO_REQ = SIR_HAL_GET_MWS_COEX_INFO_REQ,
#endif

	WMA_TWT_ADD_DIALOG_REQUEST = SIR_HAL_TWT_ADD_DIALOG_REQUEST,
	WMA_TWT_DEL_DIALOG_REQUEST = SIR_HAL_TWT_DEL_DIALOG_REQUEST,
	WMA_TWT_PAUSE_DIALOG_REQUEST = SIR_HAL_TWT_PAUSE_DIALOG_REQUEST,
	WMA_TWT_RESUME_DIALOG_REQUEST =  SIR_HAL_TWT_RESUME_DIALOG_REQUEST,
	WMA_PEER_CREATE_REQ = SIR_HAL_PEER_CREATE_REQ,
	WMA_TWT_NUDGE_DIALOG_REQUEST = SIR_HAL_TWT_NUDGE_DIALOG_REQUEST,
};

/* Bit 6 will be used to control BD rate for Management frames */
#define HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME 0x40

#define wma_tx_frame(hHal, pFrmBuf, frmLen, frmType, txDir, tid, pCompFunc, \
		   pData, txFlag, sessionid, channel_freq, rid, peer_rssi) \
	(QDF_STATUS)( wma_tx_packet( \
		      cds_get_context(QDF_MODULE_ID_WMA), \
		      (pFrmBuf), \
		      (frmLen), \
		      (frmType), \
		      (txDir), \
		      (tid), \
		      (pCompFunc), \
		      (pData), \
		      (NULL), \
		      (txFlag), \
		      (sessionid), \
		      (false), \
		      (channel_freq), \
		      (rid), \
		      (peer_rssi)))

#define wma_tx_frameWithTxComplete(hHal, pFrmBuf, frmLen, frmType, txDir, tid, \
	 pCompFunc, pData, pCBackFnTxComp, txFlag, sessionid, tdlsflag, \
	 channel_freq, rid, peer_rssi) \
	(QDF_STATUS)( wma_tx_packet( \
		      cds_get_context(QDF_MODULE_ID_WMA), \
		      (pFrmBuf), \
		      (frmLen), \
		      (frmType), \
		      (txDir), \
		      (tid), \
		      (pCompFunc), \
		      (pData), \
		      (pCBackFnTxComp), \
		      (txFlag), \
		      (sessionid), \
		      (tdlsflag), \
		      (channel_freq), \
		      (rid), \
		      (peer_rssi)))

/**
 * struct sUapsd_Params - Powersave Offload Changes
 * @bkDeliveryEnabled: BK delivery enabled flag
 * @beDeliveryEnabled: BE delivery enabled flag
 * @viDeliveryEnabled: VI delivery enabled flag
 * @voDeliveryEnabled: VO delivery enabled flag
 * @bkTriggerEnabled: BK trigger enabled flag
 * @beTriggerEnabled: BE trigger enabled flag
 * @viTriggerEnabled: VI trigger enabled flag
 * @voTriggerEnabled: VO trigger enabled flag
 */
typedef struct sUapsd_Params {
	uint8_t bkDeliveryEnabled:1;
	uint8_t beDeliveryEnabled:1;
	uint8_t viDeliveryEnabled:1;
	uint8_t voDeliveryEnabled:1;
	uint8_t bkTriggerEnabled:1;
	uint8_t beTriggerEnabled:1;
	uint8_t viTriggerEnabled:1;
	uint8_t voTriggerEnabled:1;
	bool enable_ps;
} tUapsd_Params, *tpUapsd_Params;

/**
 * struct sEnablePsParams - Enable PowerSave Params
 * @psSetting: power save setting
 * @uapsdParams: UAPSD Parameters
 * @sessionid: sme session id / vdev id
 */
typedef struct sEnablePsParams {
	tSirAddonPsReq psSetting;
	tUapsd_Params uapsdParams;
	uint32_t sessionid;
} tEnablePsParams, *tpEnablePsParams;

/**
 * struct sDisablePsParams - Disable PowerSave Params
 * @psSetting: power save setting
 * @sessionid: sme session id / vdev id
 */
typedef struct sDisablePsParams {
	tSirAddonPsReq psSetting;
	uint32_t sessionid;
} tDisablePsParams, *tpDisablePsParams;

/**
 * struct sEnableUapsdParams - Enable Uapsd Params
 * @uapsdParams: UAPSD parameters
 * @bssid: mac address
 * @sessionid: sme session id/ vdev id
 * @status: success/failure
 */
typedef struct sEnableUapsdParams {
	tUapsd_Params uapsdParams;
	tSirMacAddr bssid;
	uint32_t sessionid;
	uint32_t status;
} tEnableUapsdParams, *tpEnableUapsdParams;

/**
 * struct sDisableUapsdParams - Disable Uapsd Params
 * @bssid: mac address
 * @sessionid: sme session id/ vdev id
 * @status: success/failure
 */
typedef struct sDisableUapsdParams {
	tSirMacAddr bssid;
	uint32_t sessionid;
	uint32_t status;
} tDisableUapsdParams, *tpDisableUapsdParams;

/**
 * wma_tx_dwnld_comp_callback - callback function for TX dwnld complete
 * @context: global mac pointer
 * @buf: buffer
 * @bFreeData: to free/not free the buffer
 *
 * callback function for mgmt tx download completion.
 *
 * Return: QDF_STATUS_SUCCESS in case of success
 */
typedef QDF_STATUS (*wma_tx_dwnld_comp_callback)(void *context, qdf_nbuf_t buf,
				 bool bFreeData);

/**
 * wma_tx_ota_comp_callback - callback function for TX complete
 * @context: global mac pointer
 * @buf: buffer
 * @status: tx completion status
 * @params: tx completion params
 *
 * callback function for mgmt tx ota completion.
 *
 * Return: QDF_STATUS_SUCCESS in case of success
 */
typedef QDF_STATUS (*wma_tx_ota_comp_callback)(void *context, qdf_nbuf_t buf,
				      uint32_t status, void *params);

/* generic callback for updating parameters from target to HDD */
typedef int (*wma_tgt_cfg_cb)(hdd_handle_t handle, struct wma_tgt_cfg *cfg);

/**
 * struct wma_cli_set_cmd_t - set command parameters
 * @param_id: parameter id
 * @param_value: parameter value
 * @param_sec_value: parameter sec value
 * @param_vdev_id: parameter vdev id
 * @param_vp_dev: is it per vdev/pdev
 */
typedef struct {
	uint32_t param_id;
	uint32_t param_value;
	uint32_t param_sec_value;
	uint32_t param_vdev_id;
	uint32_t param_vp_dev;
} wma_cli_set_cmd_t;

enum rateid {
	RATEID_1MBPS = 0,
	RATEID_2MBPS,
	RATEID_5_5MBPS,
	RATEID_11MBPS,
	RATEID_6MBPS,
	RATEID_9MBPS,
	RATEID_12MBPS,
	RATEID_18MBPS,
	RATEID_24MBPS,
	RATEID_36MBPS,
	RATEID_48MBPS = 10,
	RATEID_54MBPS,
	RATEID_DEFAULT
};

QDF_STATUS wma_post_ctrl_msg(struct mac_context *mac, struct scheduler_msg *pMsg);

QDF_STATUS u_mac_post_ctrl_msg(void *pSirGlobal, tSirMbMsg *pMb);

QDF_STATUS wma_set_idle_ps_config(void *wma_ptr, uint32_t idle_ps);
QDF_STATUS wma_get_snr(tAniGetSnrReq *psnr_req);

/**
 * wma_get_rx_retry_cnt() - API to get rx retry count from data path
 * @mac: pointer to mac context
 * @vdev_id: vdev id
 * @mac_addr: mac address of the remote station
 *
 * Return: none
 */
void wma_get_rx_retry_cnt(struct mac_context *mac, uint8_t vdev_id,
			  uint8_t *mac_addr);

/**
 * wma_set_wlm_latency_level() - set latency level to FW
 * @wma_ptr: wma handle
 * @latency_params: latency params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_set_wlm_latency_level(void *wma_ptr,
			struct wlm_latency_level_param *latency_params);

QDF_STATUS
wma_ds_peek_rx_packet_info
	(cds_pkt_t *vosDataBuff, void **ppRxHeader, bool bSwap);

/**
 * wma_tx_abort() - abort tx
 * @vdev_id: vdev id
 *
 * In case of deauth host abort transmitting packet.
 *
 * Return: none
 */
void wma_tx_abort(uint8_t vdev_id);

/**
 * wma_tx_packet() - Sends Tx Frame to TxRx
 * @wma_context: wma context
 * @tx_frame: frame buffer
 * @frmLen: frame length
 * @frmType: frame type
 * @txDir: tx diection
 * @tid: TID
 * @tx_frm_download_comp_cb: tx download callback handler
 * @pData: tx packet
 * @tx_frm_ota_comp_cb: OTA complition handler
 * @tx_flag: tx flag
 * @vdev_id: vdev id
 * @tdls_flag: tdls flag
 * @channel_freq: channel frequency
 * @rid: rate id
 * @peer_rssi: peer RSSI value
 *
 * This function sends the frame corresponding to the
 * given vdev id.
 * This is blocking call till the downloading of frame is complete.
 *
 * Return: QDF status
 */
QDF_STATUS wma_tx_packet(void *wma_context, void *tx_frame, uint16_t frmLen,
			 eFrameType frmType, eFrameTxDir txDir, uint8_t tid,
			 wma_tx_dwnld_comp_callback tx_frm_download_comp_cb,
			 void *pData,
			 wma_tx_ota_comp_callback tx_frm_ota_comp_cb,
			 uint8_t tx_flag, uint8_t vdev_id, bool tdls_flag,
			 uint16_t channel_freq, enum rateid rid,
			 int8_t peer_rssi);

/**
 * wma_open() - Allocate wma context and initialize it.
 * @psoc: Psoc pointer
 * @pTgtUpdCB: tgt config update callback fun
 * @cds_cfg:  mac parameters
 * @target_type: Target type
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_open(struct wlan_objmgr_psoc *psoc,
		    wma_tgt_cfg_cb pTgtUpdCB, struct cds_config_info *cds_cfg,
		    uint32_t target_type);

/**
 * wma_vdev_init() - initialize a wma vdev
 * @vdev: the vdev to initialize
 *
 * Return: None
 */
void wma_vdev_init(struct wma_txrx_node *vdev);

/**
 * wma_vdev_deinit() - de-initialize a wma vdev
 * @vdev: the vdev to de-initialize
 *
 * Return: None
 */
void wma_vdev_deinit(struct wma_txrx_node *vdev);

QDF_STATUS wma_register_mgmt_frm_client(void);

QDF_STATUS wma_de_register_mgmt_frm_client(void);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS wma_register_roaming_callbacks(
		csr_roam_synch_fn_t csr_roam_synch_cb,
		QDF_STATUS (*csr_roam_auth_event_handle_cb)(
			struct mac_context *mac,
			uint8_t vdev_id,
			struct qdf_mac_addr bssid),
		pe_roam_synch_fn_t pe_roam_synch_cb,
		QDF_STATUS (*pe_disconnect_cb) (struct mac_context *mac,
			uint8_t vdev_id,
			uint8_t *deauth_disassoc_frame,
			uint16_t deauth_disassoc_frame_len,
			uint16_t reason_code),
		csr_roam_pmkid_req_fn_t csr_roam_pmkid_req_cb);
#else
static inline QDF_STATUS wma_register_roaming_callbacks(
		csr_roam_synch_fn_t csr_roam_synch_cb,
		QDF_STATUS (*csr_roam_auth_event_handle_cb)(
			struct mac_context *mac,
			uint8_t vdev_id,
			struct qdf_mac_addr bssid),
		pe_roam_synch_fn_t pe_roam_synch_cb,
		QDF_STATUS (*pe_disconnect_cb) (struct mac_context *mac,
			uint8_t vdev_id,
			uint8_t *deauth_disassoc_frame,
			uint16_t deauth_disassoc_frame_len,
			uint16_t reason_code),
		csr_roam_pmkid_req_fn_t csr_roam_pmkid_req_cb)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

#endif
