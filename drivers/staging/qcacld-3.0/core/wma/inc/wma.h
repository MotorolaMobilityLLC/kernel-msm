/*
 * Copyright (c) 2013-2020 The Linux Foundation. All rights reserved.
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

#ifndef WMA_H
#define WMA_H

#include "a_types.h"
#include "qdf_types.h"
#include "osapi_linux.h"
#include "htc_packet.h"
#include "i_qdf_event.h"
#include "wmi_services.h"
#include "wmi_unified.h"
#include "wmi_version.h"
#include "qdf_types.h"
#include "qdf_status.h"
#include "cds_sched.h"
#include "cds_config.h"
#include "sir_mac_prot_def.h"
#include "wma_types.h"
#include <linux/workqueue.h>
#include "utils_api.h"
#include "lim_types.h"
#include "wmi_unified_api.h"
#include "cdp_txrx_cmn.h"
#include "dbglog.h"
#include "cds_ieee80211_common.h"
#include "wlan_objmgr_psoc_obj.h"
#include <cdp_txrx_handle.h>
#include <wlan_policy_mgr_api.h>
#include "wma_api.h"
#include "wmi_unified_param.h"
#include "wmi.h"
#include "wlan_cm_roam_public_struct.h"

/* Platform specific configuration for max. no. of fragments */
#define QCA_OL_11AC_TX_MAX_FRAGS            2

/* Private */

#define WMA_READY_EVENTID_TIMEOUT          6000
#define WMA_SERVICE_READY_EXT_TIMEOUT      6000
#define NAN_CLUSTER_ID_BYTES               4

#define WMA_CRASH_INJECT_TIMEOUT           5000

/* MAC ID to PDEV ID mapping is as given below
 * MAC_ID           PDEV_ID
 * 0                    1
 * 1                    2
 * SOC Level            WMI_PDEV_ID_SOC
 */
#define WMA_MAC_TO_PDEV_MAP(x) ((x) + (1))
#define WMA_PDEV_TO_MAC_MAP(x) ((x) - (1))

#define MAX_PRINT_FAILURE_CNT 50

#define WMA_INVALID_VDEV_ID                             0xFF

#define wma_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_WMA, params)
#define wma_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_WMA, params)
#define wma_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_WMA, params)
#define wma_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_WMA, params)
#define wma_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_WMA, params)
#define wma_debug_rl(params...) QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_WMA, params)
#define wma_err_rl(params...) QDF_TRACE_ERROR_RL(QDF_MODULE_ID_WMA, params)

#define wma_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_WMA, params)
#define wma_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_WMA, params)
#define wma_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_WMA, params)
#define wma_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_WMA, params)
#define wma_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_WMA, params)

#define wma_conditional_log(is_console_log_enabled, params...) \
	do { \
		if (is_console_log_enabled) \
			wma_info(params); \
		else \
			wma_debug(params); \
	} while (0)

#define WMA_WILDCARD_PDEV_ID 0x0

#define WMA_HW_DEF_SCAN_MAX_DURATION      30000 /* 30 secs */

#define WMA_EAPOL_SUBTYPE_GET_MIN_LEN     21
#define WMA_EAPOL_INFO_GET_MIN_LEN        23
#define WMA_IS_DHCP_GET_MIN_LEN           38
#define WMA_DHCP_SUBTYPE_GET_MIN_LEN      0x11D
#define WMA_DHCP_INFO_GET_MIN_LEN         50
#define WMA_ARP_SUBTYPE_GET_MIN_LEN       22
#define WMA_IPV4_PROTO_GET_MIN_LEN        24
#define WMA_IPV4_PKT_INFO_GET_MIN_LEN     42
#define WMA_ICMP_SUBTYPE_GET_MIN_LEN      35
#define WMA_IPV6_PROTO_GET_MIN_LEN        21
#define WMA_IPV6_PKT_INFO_GET_MIN_LEN     62
#define WMA_ICMPV6_SUBTYPE_GET_MIN_LEN    55

/* Beacon tx rate */
#define WMA_BEACON_TX_RATE_1_M            10
#define WMA_BEACON_TX_RATE_2_M            20
#define WMA_BEACON_TX_RATE_5_5_M          55
#define WMA_BEACON_TX_RATE_11_M           110
#define WMA_BEACON_TX_RATE_6_M            60
#define WMA_BEACON_TX_RATE_9_M            90
#define WMA_BEACON_TX_RATE_12_M           120
#define WMA_BEACON_TX_RATE_18_M           180
#define WMA_BEACON_TX_RATE_24_M           240
#define WMA_BEACON_TX_RATE_36_M           360
#define WMA_BEACON_TX_RATE_48_M           480
#define WMA_BEACON_TX_RATE_54_M           540

/* Roaming default values
 * All time and period values are in milliseconds.
 * All rssi values are in dB except for WMA_NOISE_FLOOR_DBM_DEFAULT.
 */

#define WMA_ROAM_SCAN_CHANNEL_SWITCH_TIME    (4)
#define WMA_NOISE_FLOOR_DBM_DEFAULT          (-96)
#define WMA_RSSI_MIN_VALUE                   (-128)
#define WMA_RSSI_MAX_VALUE                   (127)
#define WMA_ROAM_RSSI_DIFF_DEFAULT           (5)
#define WMA_ROAM_DWELL_TIME_ACTIVE_DEFAULT   (100)
#define WMA_ROAM_DWELL_TIME_PASSIVE_DEFAULT  (110)
#define WMA_ROAM_MIN_REST_TIME_DEFAULT       (50)
#define WMA_ROAM_MAX_REST_TIME_DEFAULT       (500)

#define WMA_INVALID_KEY_IDX     0xff

#define WMA_MAX_RF_CHAINS(x)    ((1 << x) - 1)
#define WMA_MIN_RF_CHAINS               (1)
#define WMA_MAX_NSS               (2)

#define WMA_NOA_IE_SIZE(num_desc) (2 + (13 * (num_desc)))
#define WMA_MAX_NOA_DESCRIPTORS 4

#define WMA_TIM_SUPPORTED_PVB_LENGTH ((HAL_NUM_STA / 8) + 1)

#define WMA_BSS_STATUS_STARTED 0x1
#define WMA_BSS_STATUS_STOPPED 0x2

#define WMA_PEER_ASSOC_CNF_START 0x01
#define WMA_PEER_ASSOC_TIMEOUT SIR_PEER_ASSOC_TIMEOUT

#define WMA_DELETE_STA_RSP_START 0x02
#define WMA_DELETE_STA_TIMEOUT SIR_DELETE_STA_TIMEOUT

#define WMA_DEL_P2P_SELF_STA_RSP_START 0x03
#define WMA_SET_LINK_PEER_RSP 0x04
#define WMA_DELETE_PEER_RSP 0x05

#define WMA_PDEV_SET_HW_MODE_RESP 0x06
#define WMA_PDEV_MAC_CFG_RESP 0x07

#define WMA_PEER_CREATE_RESPONSE 0x08
#define WMA_PEER_CREATE_RESPONSE_TIMEOUT SIR_PEER_CREATE_RESPONSE_TIMEOUT

/* FW response timeout values in milli seconds */
#define WMA_VDEV_PLCY_MGR_TIMEOUT        SIR_VDEV_PLCY_MGR_TIMEOUT
#define WMA_VDEV_HW_MODE_REQUEST_TIMEOUT WMA_VDEV_PLCY_MGR_TIMEOUT
#define WMA_VDEV_DUAL_MAC_CFG_TIMEOUT    WMA_VDEV_PLCY_MGR_TIMEOUT
#define WMA_VDEV_PLCY_MGR_WAKE_LOCK_TIMEOUT \
	(WMA_VDEV_PLCY_MGR_TIMEOUT + 500)


#define WMA_VDEV_SET_KEY_WAKELOCK_TIMEOUT	WAKELOCK_DURATION_RECOMMENDED

#define WMA_TX_Q_RECHECK_TIMER_WAIT      2      /* 2 ms */
#define WMA_MAX_NUM_ARGS 8

#define WMA_SMPS_PARAM_VALUE_S 29

/*
 * Setting the Tx Comp Timeout to 1 secs.
 * TODO: Need to Revist the Timing
 */
#define WMA_TX_FRAME_COMPLETE_TIMEOUT  1000
#define WMA_TX_FRAME_BUFFER_NO_FREE    0
#define WMA_TX_FRAME_BUFFER_FREE       1

/*
 * TODO: Add WMI_CMD_ID_MAX as part of WMI_CMD_ID
 * instead of assigning it to the last valid wmi
 * cmd+1 to avoid updating this when a command is
 * added/deleted.
 */
#define WMI_CMDID_MAX (WMI_TXBF_CMDID + 1)

#define WMA_NLO_FREQ_THRESH          1000       /* in MHz */
#define WMA_MSEC_TO_USEC(msec)	     (msec * 1000) /* msec to usec */

#define WMA_AUTH_REQ_RECV_WAKE_LOCK_TIMEOUT     WAKELOCK_DURATION_RECOMMENDED
#define WMA_ASSOC_REQ_RECV_WAKE_LOCK_DURATION   WAKELOCK_DURATION_RECOMMENDED
#define WMA_DEAUTH_RECV_WAKE_LOCK_DURATION      WAKELOCK_DURATION_RECOMMENDED
#define WMA_DISASSOC_RECV_WAKE_LOCK_DURATION    WAKELOCK_DURATION_RECOMMENDED
#define WMA_ROAM_HO_WAKE_LOCK_DURATION          (500)          /* in msec */
#define WMA_ROAM_PREAUTH_WAKE_LOCK_DURATION     (2 * 1000)

#define WMA_REASON_PROBE_REQ_WPS_IE_RECV_DURATION     (3 * 1000)

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
#define WMA_AUTO_SHUTDOWN_WAKE_LOCK_DURATION    WAKELOCK_DURATION_RECOMMENDED
#endif
#define WMA_BMISS_EVENT_WAKE_LOCK_DURATION      WAKELOCK_DURATION_RECOMMENDED
#define WMA_FW_RSP_EVENT_WAKE_LOCK_DURATION     WAKELOCK_DURATION_MAX

#define WMA_TXMIC_LEN 8
#define WMA_RXMIC_LEN 8
#define WMA_IV_KEY_LEN 16

/*
 * Length = (2 octets for Index and CTWin/Opp PS) and
 * (13 octets for each NOA Descriptors)
 */

#define WMA_P2P_NOA_IE_OPP_PS_SET (0x80)
#define WMA_P2P_NOA_IE_CTWIN_MASK (0x7F)

#define WMA_P2P_IE_ID 0xdd
#define WMA_P2P_WFA_OUI { 0x50, 0x6f, 0x9a }
#define WMA_P2P_WFA_VER 0x09    /* ver 1.0 */

/* P2P Sub element definitions (according to table 5 of Wifi's P2P spec) */
#define WMA_P2P_SUB_ELEMENT_STATUS                    0
#define WMA_P2P_SUB_ELEMENT_MINOR_REASON              1
#define WMA_P2P_SUB_ELEMENT_CAPABILITY                2
#define WMA_P2P_SUB_ELEMENT_DEVICE_ID                 3
#define WMA_P2P_SUB_ELEMENT_GO_INTENT                 4
#define WMA_P2P_SUB_ELEMENT_CONFIGURATION_TIMEOUT     5
#define WMA_P2P_SUB_ELEMENT_LISTEN_CHANNEL            6
#define WMA_P2P_SUB_ELEMENT_GROUP_BSSID               7
#define WMA_P2P_SUB_ELEMENT_EXTENDED_LISTEN_TIMING    8
#define WMA_P2P_SUB_ELEMENT_INTENDED_INTERFACE_ADDR   9
#define WMA_P2P_SUB_ELEMENT_MANAGEABILITY             10
#define WMA_P2P_SUB_ELEMENT_CHANNEL_LIST              11
#define WMA_P2P_SUB_ELEMENT_NOA                       12
#define WMA_P2P_SUB_ELEMENT_DEVICE_INFO               13
#define WMA_P2P_SUB_ELEMENT_GROUP_INFO                14
#define WMA_P2P_SUB_ELEMENT_GROUP_ID                  15
#define WMA_P2P_SUB_ELEMENT_INTERFACE                 16
#define WMA_P2P_SUB_ELEMENT_OP_CHANNEL                17
#define WMA_P2P_SUB_ELEMENT_INVITATION_FLAGS          18
#define WMA_P2P_SUB_ELEMENT_VENDOR                    221

/* Macros for handling unaligned memory accesses */
#define P2PIE_PUT_LE16(a, val)		\
	do {			\
		(a)[1] = ((uint16_t) (val)) >> 8;	\
		(a)[0] = ((uint16_t) (val)) & 0xff;	\
	} while (0)

#define P2PIE_PUT_LE32(a, val)			\
	do {				\
		(a)[3] = (uint8_t) ((((uint32_t) (val)) >> 24) & 0xff);	\
		(a)[2] = (uint8_t) ((((uint32_t) (val)) >> 16) & 0xff);	\
		(a)[1] = (uint8_t) ((((uint32_t) (val)) >> 8) & 0xff);	\
		(a)[0] = (uint8_t) (((uint32_t) (val)) & 0xff);	    \
	} while (0)


#define WMA_DEFAULT_MAX_PSPOLL_BEFORE_WAKE 1

#define WMA_VHT_PPS_PAID_MATCH 1
#define WMA_VHT_PPS_GID_MATCH 2
#define WMA_VHT_PPS_DELIM_CRC_FAIL 3

#define WMA_DEFAULT_HW_MODE_INDEX 0xFFFF
#define TWO_THIRD (2/3)

/**
 * WMA hardware mode list bit-mask definitions.
 * Bits 4:0, 31:29 are unused.
 *
 * The below definitions are added corresponding to WMI DBS HW mode
 * list to make it independent of firmware changes for WMI definitions.
 * Currently these definitions have dependency with BIT positions of
 * the existing WMI macros. Thus, if the BIT positions are changed for
 * WMI macros, then these macros' BIT definitions are also need to be
 * changed.
 */
#define WMA_HW_MODE_MAC0_TX_STREAMS_BITPOS  (28)
#define WMA_HW_MODE_MAC0_RX_STREAMS_BITPOS  (24)
#define WMA_HW_MODE_MAC1_TX_STREAMS_BITPOS  (20)
#define WMA_HW_MODE_MAC1_RX_STREAMS_BITPOS  (16)
#define WMA_HW_MODE_MAC0_BANDWIDTH_BITPOS   (12)
#define WMA_HW_MODE_MAC1_BANDWIDTH_BITPOS   (8)
#define WMA_HW_MODE_DBS_MODE_BITPOS         (7)
#define WMA_HW_MODE_AGILE_DFS_MODE_BITPOS   (6)
#define WMA_HW_MODE_SBS_MODE_BITPOS         (5)

#define WMA_HW_MODE_MAC0_TX_STREAMS_MASK    \
			(0xf << WMA_HW_MODE_MAC0_TX_STREAMS_BITPOS)
#define WMA_HW_MODE_MAC0_RX_STREAMS_MASK    \
			(0xf << WMA_HW_MODE_MAC0_RX_STREAMS_BITPOS)
#define WMA_HW_MODE_MAC1_TX_STREAMS_MASK    \
			(0xf << WMA_HW_MODE_MAC1_TX_STREAMS_BITPOS)
#define WMA_HW_MODE_MAC1_RX_STREAMS_MASK    \
			(0xf << WMA_HW_MODE_MAC1_RX_STREAMS_BITPOS)
#define WMA_HW_MODE_MAC0_BANDWIDTH_MASK     \
			(0xf << WMA_HW_MODE_MAC0_BANDWIDTH_BITPOS)
#define WMA_HW_MODE_MAC1_BANDWIDTH_MASK     \
			(0xf << WMA_HW_MODE_MAC1_BANDWIDTH_BITPOS)
#define WMA_HW_MODE_DBS_MODE_MASK           \
			(0x1 << WMA_HW_MODE_DBS_MODE_BITPOS)
#define WMA_HW_MODE_AGILE_DFS_MODE_MASK     \
			(0x1 << WMA_HW_MODE_AGILE_DFS_MODE_BITPOS)
#define WMA_HW_MODE_SBS_MODE_MASK           \
			(0x1 << WMA_HW_MODE_SBS_MODE_BITPOS)

#define WMA_HW_MODE_MAC0_TX_STREAMS_SET(hw_mode, value) \
	WMI_SET_BITS(hw_mode, WMA_HW_MODE_MAC0_TX_STREAMS_BITPOS, 4, value)
#define WMA_HW_MODE_MAC0_RX_STREAMS_SET(hw_mode, value) \
	WMI_SET_BITS(hw_mode, WMA_HW_MODE_MAC0_RX_STREAMS_BITPOS, 4, value)
#define WMA_HW_MODE_MAC1_TX_STREAMS_SET(hw_mode, value) \
	WMI_SET_BITS(hw_mode, WMA_HW_MODE_MAC1_TX_STREAMS_BITPOS, 4, value)
#define WMA_HW_MODE_MAC1_RX_STREAMS_SET(hw_mode, value) \
	WMI_SET_BITS(hw_mode, WMA_HW_MODE_MAC1_RX_STREAMS_BITPOS, 4, value)
#define WMA_HW_MODE_MAC0_BANDWIDTH_SET(hw_mode, value)  \
	WMI_SET_BITS(hw_mode, WMA_HW_MODE_MAC0_BANDWIDTH_BITPOS, 4, value)
#define WMA_HW_MODE_MAC1_BANDWIDTH_SET(hw_mode, value)  \
	WMI_SET_BITS(hw_mode, WMA_HW_MODE_MAC1_BANDWIDTH_BITPOS, 4, value)
#define WMA_HW_MODE_DBS_MODE_SET(hw_mode, value)        \
	WMI_SET_BITS(hw_mode, WMA_HW_MODE_DBS_MODE_BITPOS, 1, value)
#define WMA_HW_MODE_AGILE_DFS_SET(hw_mode, value)       \
	WMI_SET_BITS(hw_mode, WMA_HW_MODE_AGILE_DFS_MODE_BITPOS, 1, value)
#define WMA_HW_MODE_SBS_MODE_SET(hw_mode, value)        \
	WMI_SET_BITS(hw_mode, WMA_HW_MODE_SBS_MODE_BITPOS, 1, value)

#define WMA_HW_MODE_MAC0_TX_STREAMS_GET(hw_mode)                \
	((hw_mode & WMA_HW_MODE_MAC0_TX_STREAMS_MASK) >>        \
		WMA_HW_MODE_MAC0_TX_STREAMS_BITPOS)
#define WMA_HW_MODE_MAC0_RX_STREAMS_GET(hw_mode)                \
	((hw_mode & WMA_HW_MODE_MAC0_RX_STREAMS_MASK) >>        \
		WMA_HW_MODE_MAC0_RX_STREAMS_BITPOS)
#define WMA_HW_MODE_MAC1_TX_STREAMS_GET(hw_mode)                \
	((hw_mode & WMA_HW_MODE_MAC1_TX_STREAMS_MASK) >>        \
		WMA_HW_MODE_MAC1_TX_STREAMS_BITPOS)
#define WMA_HW_MODE_MAC1_RX_STREAMS_GET(hw_mode)                \
	((hw_mode & WMA_HW_MODE_MAC1_RX_STREAMS_MASK) >>        \
		WMA_HW_MODE_MAC1_RX_STREAMS_BITPOS)
#define WMA_HW_MODE_MAC0_BANDWIDTH_GET(hw_mode)                 \
	((hw_mode & WMA_HW_MODE_MAC0_BANDWIDTH_MASK) >>         \
		WMA_HW_MODE_MAC0_BANDWIDTH_BITPOS)
#define WMA_HW_MODE_MAC1_BANDWIDTH_GET(hw_mode)                 \
	((hw_mode & WMA_HW_MODE_MAC1_BANDWIDTH_MASK) >>         \
		WMA_HW_MODE_MAC1_BANDWIDTH_BITPOS)
#define WMA_HW_MODE_DBS_MODE_GET(hw_mode)                       \
	((hw_mode & WMA_HW_MODE_DBS_MODE_MASK) >>               \
		WMA_HW_MODE_DBS_MODE_BITPOS)
#define WMA_HW_MODE_AGILE_DFS_GET(hw_mode)                      \
	((hw_mode & WMA_HW_MODE_AGILE_DFS_MODE_MASK) >>         \
		WMA_HW_MODE_AGILE_DFS_MODE_BITPOS)
#define WMA_HW_MODE_SBS_MODE_GET(hw_mode)                       \
	((hw_mode & WMA_HW_MODE_SBS_MODE_MASK) >>               \
		WMA_HW_MODE_SBS_MODE_BITPOS)

/*
 * Extract 2G or 5G tx/rx chainmask
 * format of txrx_chainmask (from wmi_service_ready_event_fixed_param):
 *    [7:0]   - 2G band tx chain mask
 *    [15:8]  - 2G band rx chain mask
 *    [23:16] - 5G band tx chain mask
 *    [31:24] - 5G band rx chain mask
 */
#define EXTRACT_TX_CHAIN_MASK_2G(chainmask) ((chainmask) & 0xFF)
#define EXTRACT_RX_CHAIN_MASK_2G(chainmask) (((chainmask) >> 8) & 0xFF)
#define EXTRACT_TX_CHAIN_MASK_5G(chainmask) (((chainmask) >> 16) & 0xFF)
#define EXTRACT_RX_CHAIN_MASK_5G(chainmask) (((chainmask) >> 24) & 0xFF)

/*
 * PROBE_REQ_TX_DELAY
 * param to specify probe request Tx delay for scans triggered on this VDEV
 */
#define PROBE_REQ_TX_DELAY 10

/* PROBE_REQ_TX_TIME_GAP
 * param to specify the time gap between each set of probe request transmission.
 * The number of probe requests in each set depends on the ssid_list and,
 * bssid_list in the scan request. This parameter will get applied only,
 * for the scans triggered on this VDEV.
 */
#define PROBE_REQ_TX_TIME_GAP 20

/**
 * enum wma_rx_exec_ctx - wma rx execution context
 * @WMA_RX_WORK_CTX: work queue context execution
 * @WMA_RX_TASKLET_CTX: tasklet context execution
 * @WMA_RX_SERIALIZER_CTX: MC thread context execution
 *
 */
enum wma_rx_exec_ctx {
	WMA_RX_WORK_CTX = WMI_RX_WORK_CTX,
	WMA_RX_TASKLET_CTX = WMI_RX_TASKLET_CTX,
	WMA_RX_SERIALIZER_CTX = WMI_RX_SERIALIZER_CTX,
};

/**
 * struct beacon_info - structure to store beacon template
 * @buf: skb ptr
 * @len: length
 * @dma_mapped: is it dma mapped or not
 * @tim_ie_offset: TIM IE offset
 * @dtim_count: DTIM count
 * @seq_no: sequence no
 * @noa_sub_ie: NOA sub IE
 * @noa_sub_ie_len: NOA sub IE length
 * @noa_ie: NOA IE
 * @p2p_ie_offset: p2p IE offset
 * @lock: lock
 */
struct beacon_info {
	qdf_nbuf_t buf;
	uint32_t len;
	uint8_t dma_mapped;
	uint32_t tim_ie_offset;
	uint8_t dtim_count;
	uint16_t seq_no;
	uint8_t noa_sub_ie[2 + WMA_NOA_IE_SIZE(WMA_MAX_NOA_DESCRIPTORS)];
	uint16_t noa_sub_ie_len;
	uint8_t *noa_ie;
	uint16_t p2p_ie_offset;
	qdf_spinlock_t lock;
};

/**
 * struct beacon_tim_ie - structure to store TIM IE of beacon
 * @tim_ie: tim ie
 * @tim_len: tim ie length
 * @dtim_count: dtim count
 * @dtim_period: dtim period
 * @tim_bitctl: tim bit control
 * @tim_bitmap: tim bitmap
 */
struct beacon_tim_ie {
	uint8_t tim_ie;
	uint8_t tim_len;
	uint8_t dtim_count;
	uint8_t dtim_period;
	uint8_t tim_bitctl;
	uint8_t tim_bitmap[1];
} __ATTRIB_PACK;

/**
 * struct pps - packet power save parameter
 * @paid_match_enable: paid match enable
 * @gid_match_enable: gid match enable
 * @tim_clear: time clear
 * @dtim_clear: dtim clear
 * @eof_delim: eof delim
 * @mac_match: mac match
 * @delim_fail: delim fail
 * @nsts_zero: nsts zero
 * @rssi_chk: RSSI check
 * @ebt_5g: ebt 5GHz
 */
struct pps {
	bool paid_match_enable;
	bool gid_match_enable;
	bool tim_clear;
	bool dtim_clear;
	bool eof_delim;
	bool mac_match;
	bool delim_fail;
	bool nsts_zero;
	bool rssi_chk;
	bool ebt_5g;
};

/**
 * struct qpower_params - qpower related parameters
 * @max_ps_poll_cnt: max ps poll count
 * @max_tx_before_wake: max tx before wake
 * @spec_ps_poll_wake_interval: ps poll wake interval
 * @max_spec_nodata_ps_poll: no data ps poll
 */
struct qpower_params {
	uint32_t max_ps_poll_cnt;
	uint32_t max_tx_before_wake;
	uint32_t spec_ps_poll_wake_interval;
	uint32_t max_spec_nodata_ps_poll;
};


/**
 * struct gtx_config_t - GTX config
 * @gtxRTMask: for HT and VHT rate masks
 * @gtxUsrcfg: host request for GTX mask
 * @gtxPERThreshold: PER Threshold (default: 10%)
 * @gtxPERMargin: PER margin (default: 2%)
 * @gtxTPCstep: TCP step (default: 1)
 * @gtxTPCMin: TCP min (default: 5)
 * @gtxBWMask: BW mask (20/40/80/160 Mhz)
 */
typedef struct {
	uint32_t gtxRTMask[2];
	uint32_t gtxUsrcfg;
	uint32_t gtxPERThreshold;
	uint32_t gtxPERMargin;
	uint32_t gtxTPCstep;
	uint32_t gtxTPCMin;
	uint32_t gtxBWMask;
} gtx_config_t;

/**
 * struct pdev_cli_config_t - store pdev parameters
 * @ani_enable: ANI is enabled/disable on target
 * @ani_poll_len: store ANI polling period
 * @ani_listen_len: store ANI listening period
 * @ani_ofdm_level: store ANI OFDM immunity level
 * @ani_cck_level: store ANI CCK immunity level
 * @cwmenable: Dynamic bw is enable/disable in fw
 * @txchainmask: tx chain mask
 * @rxchainmask: rx chain mask
 * @txpow2g: tx power limit for 2GHz
 * @txpow5g: tx power limit for 5GHz
 *
 * This structure stores pdev parameters.
 * Some of these parameters are set in fw and some
 * parameters are only maintained in host.
 */
typedef struct {
	uint32_t ani_enable;
	uint32_t ani_poll_len;
	uint32_t ani_listen_len;
	uint32_t ani_ofdm_level;
	uint32_t ani_cck_level;
	uint32_t cwmenable;
	uint32_t cts_cbw;
	uint32_t txchainmask;
	uint32_t rxchainmask;
	uint32_t txpow2g;
	uint32_t txpow5g;
} pdev_cli_config_t;

/**
 * struct vdev_cli_config_t - store vdev parameters
 * @nss: nss width
 * @ldpc: is ldpc is enable/disable
 * @tx_stbc: TX STBC is enable/disable
 * @rx_stbc: RX STBC is enable/disable
 * @shortgi: short gi is enable/disable
 * @rtscts_en: RTS/CTS is enable/disable
 * @chwidth: channel width
 * @tx_rate: tx rate
 * @ampdu: ampdu size
 * @amsdu: amsdu size
 * @erx_adjust: enable/disable early rx enable
 * @erx_bmiss_num: target bmiss number per sample
 * @erx_bmiss_cycle: sample cycle
 * @erx_slop_step: slop_step value
 * @erx_init_slop: init slop
 * @erx_adj_pause: pause adjust enable/disable
 * @erx_dri_sample: enable/disable drift sample
 * @pps_params: packet power save parameters
 * @qpower_params: qpower parameters
 * @gtx_info: GTX offload info
 * @dcm: DCM enable/disable
 * @range_ext: HE range extension enable/disable
 * @tx_ampdu: tx ampdu size
 * @rx_ampdu: rx ampdu size
 * @tx_amsdu: tx amsdu size
 * @rx_amsdu: rx amsdu size
 *
 * This structure stores vdev parameters.
 * Some of these parameters are set in fw and some
 * parameters are only maintained in host.
 */
typedef struct {
	uint32_t nss;
	uint32_t ldpc;
	uint32_t tx_stbc;
	uint32_t rx_stbc;
	uint32_t shortgi;
	uint32_t rtscts_en;
	uint32_t chwidth;
	uint32_t tx_rate;
	uint32_t ampdu;
	uint32_t amsdu;
	uint32_t erx_adjust;
	uint32_t erx_bmiss_num;
	uint32_t erx_bmiss_cycle;
	uint32_t erx_slop_step;
	uint32_t erx_init_slop;
	uint32_t erx_adj_pause;
	uint32_t erx_dri_sample;
	struct pps pps_params;
	struct qpower_params qpower_params;
	gtx_config_t gtx_info;
#ifdef WLAN_FEATURE_11AX
	uint8_t dcm;
	uint8_t range_ext;
#endif
	uint32_t tx_ampdu;
	uint32_t rx_ampdu;
	uint32_t tx_amsdu;
	uint32_t rx_amsdu;
} vdev_cli_config_t;

/**
 * struct wma_version_info - Store wmi version info
 * @major: wmi major version
 * @minor: wmi minor version
 * @revision: wmi revision number
 */
struct wma_version_info {
	u_int32_t major;
	u_int32_t minor;
	u_int32_t revision;
};

struct roam_synch_frame_ind {
	uint32_t bcn_probe_rsp_len;
	uint8_t *bcn_probe_rsp;
	uint8_t is_beacon;
	uint32_t reassoc_req_len;
	uint8_t *reassoc_req;
	uint32_t reassoc_rsp_len;
	uint8_t *reassoc_rsp;
};

/* Max number of invalid peer entries */
#define INVALID_PEER_MAX_NUM 5

/**
 * struct wma_invalid_peer_params - stores invalid peer entries
 * @rx_macaddr: store mac addr of invalid peer
 */
struct wma_invalid_peer_params {
	uint8_t rx_macaddr[QDF_MAC_ADDR_SIZE];
};

/**
 * struct wma_txrx_node - txrx node
 * @vdev: pointer to vdev object
 * @beacon: beacon info
 * @config: per vdev config parameters
 * @scan_info: scan info
 * @type: type
 * @sub_type: sub type
 * @nlo_match_evt_received: is nlo match event received or not
 * @pno_in_progress: is pno in progress or not
 * @plm_in_progress: is plm in progress or not
 * @beaconInterval: beacon interval
 * @llbCoexist: 11b coexist
 * @shortSlotTimeSupported: is short slot time supported or not
 * @dtimPeriod: DTIM period
 * @chan_width: channel bandwidth
 * @vdev_up: is vdev up or not
 * @tsfadjust: TSF adjust
 * @addBssStaContext: add bss context
 * @aid: association id
 * @rmfEnabled: Robust Management Frame (RMF) enabled/disabled
 * @uapsd_cached_val: uapsd cached value
 * @stats_rsp: stats response
 * @del_staself_req: delete sta self request
 * @bss_status: bss status
 * @nss: nss value
 * @is_channel_switch: is channel switch
 * @pause_bitmap: pause bitmap
 * @nwType: network type (802.11a/b/g/n/ac)
 * @ps_enabled: is powersave enable/disable
 * @peer_count: peer count
 * @roam_synch_in_progress: flag is in progress or not
 * @plink_status_req: link status request
 * @psnr_req: snr request
 * @tx_streams: number of tx streams can be used by the vdev
 * @mac_id: the mac on which vdev is on
 * @arp_offload_req: cached arp offload request
 * @ns_offload_req: cached ns offload request
 * @rcpi_req: rcpi request
 * @in_bmps: Whether bmps for this interface has been enabled
 * @vdev_set_key_wakelock: wakelock to protect vdev set key op with firmware
 * @vdev_set_key_runtime_wakelock: runtime pm wakelock for set key
 * @ch_freq: channel frequency
 * @roam_scan_stats_req: cached roam scan stats request
 * @wma_invalid_peer_params: structure storing invalid peer params
 * @invalid_peer_idx: invalid peer index
 * It stores parameters per vdev in wma.
 */
struct wma_txrx_node {
	struct wlan_objmgr_vdev *vdev;
	struct beacon_info *beacon;
	vdev_cli_config_t config;
	uint32_t type;
	uint32_t sub_type;
#ifdef FEATURE_WLAN_ESE
	bool plm_in_progress;
#endif
	tSirMacBeaconInterval beaconInterval;
	uint8_t llbCoexist;
	uint8_t shortSlotTimeSupported;
	uint8_t dtimPeriod;
	enum phy_ch_width chan_width;
	bool vdev_active;
	uint64_t tsfadjust;
	tAddStaParams *addBssStaContext;
	uint16_t aid;
	uint8_t rmfEnabled;
	uint32_t uapsd_cached_val;
	void *del_staself_req;
	bool is_del_sta_defered;
	qdf_atomic_t bss_status;
	enum tx_rate_info rate_flags;
	uint8_t nss;
	uint16_t pause_bitmap;
	uint32_t nwType;
	uint32_t peer_count;
	void *plink_status_req;
	void *psnr_req;
#ifdef FEATURE_WLAN_EXTSCAN
	bool extscan_in_progress;
#endif
	uint32_t tx_streams;
	uint32_t mac_id;
	bool roaming_in_progress;
	int32_t roam_synch_delay;
	struct sme_rcpi_req *rcpi_req;
	bool in_bmps;
	struct beacon_filter_param beacon_filter;
	bool beacon_filter_enabled;
	qdf_wake_lock_t vdev_set_key_wakelock;
	qdf_runtime_lock_t vdev_set_key_runtime_wakelock;
	struct roam_synch_frame_ind roam_synch_frame_ind;
	bool is_waiting_for_key;
	uint32_t ch_freq;
	uint16_t ch_flagext;
	struct sir_roam_scan_stats *roam_scan_stats_req;
	struct wma_invalid_peer_params invalid_peers[INVALID_PEER_MAX_NUM];
	uint8_t invalid_peer_idx;
};

/**
 * struct mac_ss_bw_info - hw_mode_list PHY/MAC params for each MAC
 * @mac_tx_stream: Max TX stream
 * @mac_rx_stream: Max RX stream
 * @mac_bw: Max bandwidth
 */
struct mac_ss_bw_info {
	uint32_t mac_tx_stream;
	uint32_t mac_rx_stream;
	uint32_t mac_bw;
};

/**
 * struct wma_ini_config - Structure to hold wma ini configuration
 * @max_no_of_peers: Max Number of supported
 *
 * Placeholder for WMA ini parameters.
 */
struct wma_ini_config {
	uint8_t max_no_of_peers;
};

/**
 * struct wma_valid_channels - Channel details part of WMI_SCAN_CHAN_LIST_CMDID
 * @num_channels: Number of channels
 * @ch_freq_list: Channel Frequency list
 */
struct wma_valid_channels {
	uint8_t num_channels;
	uint32_t ch_freq_list[NUM_CHANNELS];
};

#ifdef FEATURE_WLM_STATS
/**
 * struct wma_wlm_stats_data - Data required to be used to send WLM req
 * @wlm_stats_max_size: Buffer size provided by userspace
 * @wlm_stats_cookie: Cookie to retrieve WLM req data
 * @wlm_stats_callback: Callback to be used to send WLM response
 */
struct wma_wlm_stats_data {
	uint32_t wlm_stats_max_size;
	void *wlm_stats_cookie;
	wma_wlm_stats_cb wlm_stats_callback;
};
#endif

/**
 * struct t_wma_handle - wma context
 * @wmi_handle: wmi handle
 * @cds_context: cds handle
 * @mac_context: mac context
 * @psoc: psoc context
 * @pdev: physical device global object
 * @target_suspend: target suspend event
 * @recovery_event: wma FW recovery event
 * @max_station: max stations
 * @max_bssid: max bssid
 * @myaddr: current mac address
 * @hwaddr: mac address from EEPROM
 * @lpss_support: LPSS feature is supported in target or not
 * @wmi_ready: wmi status flag
 * @wlan_init_status: wlan init status
 * @qdf_dev: qdf device
 * @wmi_service_bitmap: wmi services bitmap received from Target
 * @wmi_service_ext_bitmap: extended wmi services bitmap received from Target
 * @tx_frm_download_comp_cb: Tx Frame Compl Cb registered by umac
 * @tx_frm_download_comp_event: Event to wait for tx download completion
 * @tx_queue_empty_event: Dummy event to wait for draining MSDUs left
 *   in hardware tx queue and before requesting VDEV_STOP. Nobody will
 *   set this and wait will timeout, and code will poll the pending tx
 *   descriptors number to be zero.
 * @umac_ota_ack_cb: Ack Complete Callback registered by umac
 * @umac_data_ota_ack_cb: ack complete callback
 * @last_umac_data_ota_timestamp: timestamp when OTA of last umac data
 *   was done
 * @last_umac_data_nbuf: cache nbuf ptr for the last umac data buf
 * @tgt_cfg_update_cb: configuration update callback
 * @reg_cap: regulatory capablities
 * @scan_id: scan id
 * @interfaces: txrx nodes(per vdev)
 * @pdevconfig: pdev related configrations
 * @wma_hold_req_queue: Queue use to serialize requests to firmware
 * @wma_hold_req_q_lock: Mutex for @wma_hold_req_queue
 * @vht_supp_mcs: VHT supported MCS
 * @is_fw_assert: is fw asserted
 * @ack_work_ctx: Context for deferred processing of TX ACK
 * @powersave_mode: power save mode
 * @pGetRssiReq: get RSSI request
 * @get_one_peer_info: When a "get peer info" request is active, is
 *   the request for a single peer?
 * @peer_macaddr: When @get_one_peer_info is true, the peer's mac address
 * @thermal_mgmt_info: Thermal mitigation related info
 * @enable_mc_list: To Check if Multicast list filtering is enabled in FW
 * @hddTxFailCb: tx fail indication callback
 * @extscan_wake_lock: extscan wake lock
 * @wow_wake_lock: wow wake lock
 * @wow_auth_req_wl: wow wake lock for auth req
 * @wow_assoc_req_wl: wow wake lock for assoc req
 * @wow_deauth_rec_wl: wow wake lock for deauth req
 * @wow_disassoc_rec_wl: wow wake lock for disassoc req
 * @wow_ap_assoc_lost_wl: wow wake lock for assoc lost req
 * @wow_auto_shutdown_wl: wow wake lock for shutdown req
 * @roam_ho_wl: wake lock for roam handoff req
 * @wow_nack: wow negative ack flag
 * @is_wow_bus_suspended: is wow bus suspended flag
 * @IsRArateLimitEnabled: RA rate limiti s enabled or not
 * @RArateLimitInterval: RA rate limit interval
 * @is_lpass_enabled: Flag to indicate if LPASS feature is enabled or not
 * @staMaxLIModDtim: station max listen interval
 * @staModDtim: station mode DTIM
 * @staDynamicDtim: station dynamic DTIM
 * @hw_bd_id: hardware board id
 * @hw_bd_info: hardware board info
 * @miracast_value: miracast value
 * @log_completion_timer: log completion timer
 * @num_dbs_hw_modes: Number of HW modes supported by the FW
 * @hw_mode: DBS HW mode list
 * @old_hw_mode_index: Previous configured HW mode index
 * @new_hw_mode_index: Current configured HW mode index
 * @peer_authorized_cb: peer authorized hdd callback
 * @ocb_config_req: OCB request context
 * @self_gen_frm_pwr: Self-generated frame power
 * @tx_chain_mask_cck: Is the CCK tx chain mask enabled
 * @service_ready_ext_timer: Timer for service ready extended.  Note
 *   this is a a timer instead of wait event because on receiving the
 *   service ready event, we will be waiting on the MC thread for the
 *   service extended ready event which is also processed in MC
 *   thread.  This leads to MC thread being stuck. Alternative was to
 *   process these events in tasklet/workqueue context. But, this
 *   leads to race conditions when the events are processed in two
 *   different context. So, processing ready event and extended ready
 *   event in the serialized MC thread context with a timer.
 * @csr_roam_synch_cb: CSR callback for firmware Roam Sync events
 * @pe_roam_synch_cb: pe callback for firmware Roam Sync events
 * @csr_roam_auth_event_handle_cb: CSR callback for target authentication
 * offload event.
 * @wmi_cmd_rsp_wake_lock: wmi command response wake lock
 * @wmi_cmd_rsp_runtime_lock: wmi command response bus lock
 * @active_uc_apf_mode: Setting that determines how APF is applied in
 *   active mode for uc packets
 * @active_mc_bc_apf_mode: Setting that determines how APF is applied in
 *   active mode for MC/BC packets
 * @ini_config: Initial configuration from upper layer
 * @saved_chan: saved channel list sent as part of
 *   WMI_SCAN_CHAN_LIST_CMDID
 * @nan_datapath_enabled: Is NAN datapath support enabled in firmware?
 * @fw_timeout_crash: Should firmware be reset upon response timeout?
 * @sub_20_support: Does target support sub-20MHz bandwidth (aka
 *   half-rate and quarter-rate)?
 * @is_dfs_offloaded: Is dfs and cac timer offloaded?
 * @wma_mgmt_tx_packetdump_cb: Callback function for TX packet dump
 * @wma_mgmt_rx_packetdump_cb: Callback function for RX packet dump
 * @rcpi_enabled: Is RCPI enabled?
 * @link_stats_results: Structure for handing link stats from firmware
 * @tx_fail_cnt: Number of TX failures
 * @wlm_data: Data required for WLM req and resp handling
 * @he_cap: 802.11ax capabilities
 * @bandcapability: band capability configured through ini
 * @tx_bfee_8ss_enabled: Is Tx Beamformee support for 8x8 enabled?
 * @in_imps: Is device in Idle Mode Power Save?
 * @dynamic_nss_chains_update: per vdev nss, chains update
 * @ito_repeat_count: Indicates ito repeated count
 * @wma_fw_time_sync_timer: timer used for firmware time sync
 * * @fw_therm_throt_support: FW Supports thermal throttling?
 *
 * This structure is the global wma context.  It contains global wma
 * module parameters and handles of other modules.

 */
typedef struct {
	void *wmi_handle;
	void *cds_context;
	struct mac_context *mac_context;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	qdf_event_t target_suspend;
	qdf_event_t runtime_suspend;
	qdf_event_t recovery_event;
	uint16_t max_station;
	uint16_t max_bssid;
	uint8_t myaddr[QDF_MAC_ADDR_SIZE];
	uint8_t hwaddr[QDF_MAC_ADDR_SIZE];
#ifdef WLAN_FEATURE_LPSS
	uint8_t lpss_support;
#endif
	uint8_t ap_arpns_support;
	bool wmi_ready;
	uint32_t wlan_init_status;
	qdf_device_t qdf_dev;
	uint32_t wmi_service_bitmap[WMI_SERVICE_BM_SIZE];
	uint32_t wmi_service_ext_bitmap[WMI_SERVICE_SEGMENT_BM_SIZE32];
	wma_tx_dwnld_comp_callback tx_frm_download_comp_cb;
	qdf_event_t tx_frm_download_comp_event;
	qdf_event_t tx_queue_empty_event;
	wma_tx_ota_comp_callback umac_data_ota_ack_cb;
	unsigned long last_umac_data_ota_timestamp;
	qdf_nbuf_t last_umac_data_nbuf;
	wma_tgt_cfg_cb tgt_cfg_update_cb;
	HAL_REG_CAPABILITIES reg_cap;
	uint32_t scan_id;
	struct wma_txrx_node *interfaces;
	pdev_cli_config_t pdevconfig;
	qdf_list_t wma_hold_req_queue;
	qdf_spinlock_t wma_hold_req_q_lock;
	uint32_t vht_supp_mcs;
	uint8_t is_fw_assert;
	struct wma_tx_ack_work_ctx *ack_work_ctx;
	uint8_t powersave_mode;
	void *pGetRssiReq;
	bool get_one_peer_info;
	struct qdf_mac_addr peer_macaddr;
	t_thermal_mgmt thermal_mgmt_info;
	bool enable_mc_list;
#ifdef FEATURE_WLAN_EXTSCAN
	qdf_wake_lock_t extscan_wake_lock;
#endif
	qdf_wake_lock_t wow_wake_lock;
	qdf_wake_lock_t wow_auth_req_wl;
	qdf_wake_lock_t wow_assoc_req_wl;
	qdf_wake_lock_t wow_deauth_rec_wl;
	qdf_wake_lock_t wow_disassoc_rec_wl;
	qdf_wake_lock_t wow_ap_assoc_lost_wl;
	qdf_wake_lock_t wow_auto_shutdown_wl;
	qdf_wake_lock_t roam_ho_wl;
	qdf_wake_lock_t roam_preauth_wl;
	qdf_wake_lock_t probe_req_wps_wl;
	int wow_nack;
	qdf_atomic_t is_wow_bus_suspended;
#ifdef WLAN_FEATURE_LPSS
	bool is_lpass_enabled;
#endif
	uint8_t staMaxLIModDtim;
	uint8_t staModDtim;
	uint8_t staDynamicDtim;
	uint32_t hw_bd_id;
	uint32_t hw_bd_info[HW_BD_INFO_SIZE];
	uint32_t miracast_value;
	qdf_mc_timer_t log_completion_timer;
	uint32_t num_dbs_hw_modes;
	struct dbs_hw_mode_info hw_mode;
	uint32_t old_hw_mode_index;
	uint32_t new_hw_mode_index;
	wma_peer_authorized_fp peer_authorized_cb;
	struct sir_ocb_config *ocb_config_req;
	uint16_t self_gen_frm_pwr;
	bool tx_chain_mask_cck;
	qdf_mc_timer_t service_ready_ext_timer;

	QDF_STATUS (*csr_roam_synch_cb)(struct mac_context *mac,
		struct roam_offload_synch_ind *roam_synch_data,
		struct bss_description *bss_desc_ptr,
		enum sir_roam_op_code reason);
	QDF_STATUS (*csr_roam_auth_event_handle_cb)(struct mac_context *mac,
						    uint8_t vdev_id,
						    struct qdf_mac_addr bssid);
	QDF_STATUS (*pe_roam_synch_cb)(struct mac_context *mac,
		struct roam_offload_synch_ind *roam_synch_data,
		struct bss_description *bss_desc_ptr,
		enum sir_roam_op_code reason);
	QDF_STATUS (*pe_disconnect_cb) (struct mac_context *mac,
					uint8_t vdev_id,
					uint8_t *deauth_disassoc_frame,
					uint16_t deauth_disassoc_frame_len,
					uint16_t reason_code);
	QDF_STATUS (*csr_roam_pmkid_req_cb)(uint8_t vdev_id,
		struct roam_pmkid_req_event *bss_list);
	qdf_wake_lock_t wmi_cmd_rsp_wake_lock;
	qdf_runtime_lock_t wmi_cmd_rsp_runtime_lock;
	qdf_runtime_lock_t sap_prevent_runtime_pm_lock;
	enum active_apf_mode active_uc_apf_mode;
	enum active_apf_mode active_mc_bc_apf_mode;
	struct wma_ini_config ini_config;
	struct wma_valid_channels saved_chan;
	bool nan_datapath_enabled;
	bool fw_timeout_crash;
	bool sub_20_support;
	bool is_dfs_offloaded;
	ol_txrx_pktdump_cb wma_mgmt_tx_packetdump_cb;
	ol_txrx_pktdump_cb wma_mgmt_rx_packetdump_cb;
	bool rcpi_enabled;
	tSirLLStatsResults *link_stats_results;
	uint64_t tx_fail_cnt;
#ifdef FEATURE_WLM_STATS
	struct wma_wlm_stats_data wlm_data;
#endif
#ifdef WLAN_FEATURE_11AX
	struct he_capability he_cap;
#endif
	uint8_t bandcapability;
	bool tx_bfee_8ss_enabled;
	bool in_imps;
	bool dynamic_nss_chains_support;
	uint8_t  ito_repeat_count;
	qdf_mc_timer_t wma_fw_time_sync_timer;
	bool fw_therm_throt_support;
	bool enable_tx_compl_tsf64;
} t_wma_handle, *tp_wma_handle;

/**
 * wma_validate_handle() - Validate WMA handle
 * @wma_handle: wma handle
 *
 * Return: errno if WMA handle is NULL; 0 otherwise
 */
#define wma_validate_handle(wma_handle) \
        __wma_validate_handle(wma_handle, __func__)
int __wma_validate_handle(tp_wma_handle wma_handle, const char *func);

/**
 * wma_vdev_nss_chain_params_send() - send vdev nss chain params to fw.
 * @vdev_id: vdev_id
 * @user_cfg: pointer to the params structure
 *
 * This function sends nss chain params to the fw
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE on error
 */
QDF_STATUS
wma_vdev_nss_chain_params_send(uint8_t vdev_id,
			       struct wlan_mlme_nss_chains *user_cfg);

/**
 * wma_send_regdomain_info_to_fw() - send regdomain info to fw
 * @reg_dmn: reg domain
 * @regdmn2G: 2G reg domain
 * @regdmn5G: 5G reg domain
 * @ctl2G: 2G test limit
 * @ctl5G: 5G test limit
 *
 * Return: none
 */
void wma_send_regdomain_info_to_fw(uint32_t reg_dmn, uint16_t regdmn2G,
				   uint16_t regdmn5G, uint8_t ctl2G,
				   uint8_t ctl5G);
/**
 * enum frame_index - Frame index
 * @GENERIC_NODOWNLD_NOACK_COMP_INDEX: Frame index for no download comp no ack
 * @GENERIC_DOWNLD_COMP_NOACK_COMP_INDEX: Frame index for download comp no ack
 * @GENERIC_DOWNLD_COMP_ACK_COMP_INDEX: Frame index for download comp and ack
 * @GENERIC_NODOWLOAD_ACK_COMP_INDEX: Frame index for no download comp and ack
 * @FRAME_INDEX_MAX: maximum frame index
 */
enum frame_index {
	GENERIC_NODOWNLD_NOACK_COMP_INDEX,
	GENERIC_DOWNLD_COMP_NOACK_COMP_INDEX,
	GENERIC_DOWNLD_COMP_ACK_COMP_INDEX,
	GENERIC_NODOWLOAD_ACK_COMP_INDEX,
	FRAME_INDEX_MAX
};

/**
 * struct wma_tx_ack_work_ctx - tx ack work context
 * @wma_handle: wma handle
 * @sub_type: sub type
 * @status: status
 * @ack_cmp_work: work structure
 */
struct wma_tx_ack_work_ctx {
	tp_wma_handle wma_handle;
	uint16_t sub_type;
	int32_t status;
	qdf_work_t ack_cmp_work;
};

/**
 * struct wma_target_req - target request parameters
 * @event_timeout: event timeout
 * @node: list
 * @user_data: user data
 * @msg_type: message type
 * @vdev_id: vdev id
 * @type: type
 */
struct wma_target_req {
	qdf_mc_timer_t event_timeout;
	qdf_list_node_t node;
	void *user_data;
	uint32_t msg_type;
	uint8_t vdev_id;
	uint8_t type;
};

/**
 * struct wma_set_key_params - set key parameters
 * @vdev_id: vdev id
 * @def_key_idx: used to see if we have to read the key from cfg
 * @key_len: key length
 * @peer_mac: peer mac address
 * @singl_tid_rc: 1=Single TID based Replay Count, 0=Per TID based RC
 * @key_type: key type
 * @key_idx: key index
 * @unicast: unicast flag
 * @key_data: key data
 */
struct wma_set_key_params {
	uint8_t vdev_id;
	/* def_key_idx can be used to see if we have to read the key from cfg */
	uint32_t def_key_idx;
	uint16_t key_len;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	uint8_t singl_tid_rc;
	enum eAniEdType key_type;
	uint32_t key_idx;
	bool unicast;
	uint8_t key_data[SIR_MAC_MAX_KEY_LENGTH];
	uint8_t key_rsc[WLAN_CRYPTO_RSC_SIZE];
};

/**
 * struct t_thermal_cmd_params - thermal command parameters
 * @minTemp: minimum temprature
 * @maxTemp: maximum temprature
 * @thermalEnable: thermal enable
 * @thermal_action: thermal action
 */
typedef struct {
	uint16_t minTemp;
	uint16_t maxTemp;
	uint8_t thermalEnable;
	enum thermal_mgmt_action_code thermal_action;
} t_thermal_cmd_params, *tp_thermal_cmd_params;

/**
 * enum wma_cfg_cmd_id - wma cmd ids
 * @WMA_VDEV_TXRX_FWSTATS_ENABLE_CMDID: txrx firmware stats enable command
 * @WMA_VDEV_TXRX_FWSTATS_RESET_CMDID: txrx firmware stats reset command
 * @WMA_VDEV_MCC_SET_TIME_LATENCY: set MCC latency time
 * @WMA_VDEV_MCC_SET_TIME_QUOTA: set MCC time quota
 * @WMA_VDEV_TXRX_GET_IPA_UC_FW_STATS_CMDID: get IPA microcontroller fw stats
 * @WMA_VDEV_TXRX_GET_IPA_UC_SHARING_STATS_CMDID: get IPA uC wifi-sharing stats
 * @WMA_VDEV_TXRX_SET_IPA_UC_QUOTA_CMDID: set IPA uC quota limit
 *
 * wma command ids for configuration request which
 * does not involve sending a wmi command.
 */
enum wma_cfg_cmd_id {
	WMA_VDEV_TXRX_FWSTATS_ENABLE_CMDID = WMI_CMDID_MAX,
	WMA_VDEV_TXRX_FWSTATS_RESET_CMDID,
	WMA_VDEV_MCC_SET_TIME_LATENCY,
	WMA_VDEV_MCC_SET_TIME_QUOTA,
	WMA_VDEV_TXRX_GET_IPA_UC_FW_STATS_CMDID,
	WMA_VDEV_TXRX_GET_IPA_UC_SHARING_STATS_CMDID,
	WMA_VDEV_TXRX_SET_IPA_UC_QUOTA_CMDID,
	WMA_CMD_ID_MAX
};

/**
 * struct wma_trigger_uapsd_params - trigger uapsd parameters
 * @wmm_ac: wmm access category
 * @user_priority: user priority
 * @service_interval: service interval
 * @suspend_interval: suspend interval
 * @delay_interval: delay interval
 */
typedef struct wma_trigger_uapsd_params {
	uint32_t wmm_ac;
	uint32_t user_priority;
	uint32_t service_interval;
	uint32_t suspend_interval;
	uint32_t delay_interval;
} t_wma_trigger_uapsd_params, *tp_wma_trigger_uapsd_params;

/**
 * enum uapsd_peer_param_max_sp - U-APSD maximum service period of peer station
 * @UAPSD_MAX_SP_LEN_UNLIMITED: unlimited max service period
 * @UAPSD_MAX_SP_LEN_2: max service period = 2
 * @UAPSD_MAX_SP_LEN_4: max service period = 4
 * @UAPSD_MAX_SP_LEN_6: max service period = 6
 */
enum uapsd_peer_param_max_sp {
	UAPSD_MAX_SP_LEN_UNLIMITED = 0,
	UAPSD_MAX_SP_LEN_2 = 2,
	UAPSD_MAX_SP_LEN_4 = 4,
	UAPSD_MAX_SP_LEN_6 = 6
};

/**
 * enum uapsd_peer_param_enabled_ac - U-APSD Enabled AC's of peer station
 * @UAPSD_VO_ENABLED: enable uapsd for voice
 * @UAPSD_VI_ENABLED: enable uapsd for video
 * @UAPSD_BK_ENABLED: enable uapsd for background
 * @UAPSD_BE_ENABLED: enable uapsd for best effort
 */
enum uapsd_peer_param_enabled_ac {
	UAPSD_VO_ENABLED = 0x01,
	UAPSD_VI_ENABLED = 0x02,
	UAPSD_BK_ENABLED = 0x04,
	UAPSD_BE_ENABLED = 0x08
};

/**
 * enum profile_id_t - Firmware profiling index
 * @PROF_CPU_IDLE: cpu idle profile
 * @PROF_PPDU_PROC: ppdu processing profile
 * @PROF_PPDU_POST: ppdu post profile
 * @PROF_HTT_TX_INPUT: htt tx input profile
 * @PROF_MSDU_ENQ: msdu enqueue profile
 * @PROF_PPDU_POST_HAL: ppdu post profile
 * @PROF_COMPUTE_TX_TIME: tx time profile
 * @PROF_MAX_ID: max profile index
 */
enum profile_id_t {
	PROF_CPU_IDLE,
	PROF_PPDU_PROC,
	PROF_PPDU_POST,
	PROF_HTT_TX_INPUT,
	PROF_MSDU_ENQ,
	PROF_PPDU_POST_HAL,
	PROF_COMPUTE_TX_TIME,
	PROF_MAX_ID,
};

/**
 * struct p2p_ie - P2P IE structural definition.
 * @p2p_id: p2p id
 * @p2p_len: p2p length
 * @p2p_oui: p2p OUI
 * @p2p_oui_type: p2p OUI type
 */
struct p2p_ie {
	uint8_t p2p_id;
	uint8_t p2p_len;
	uint8_t p2p_oui[3];
	uint8_t p2p_oui_type;
} __packed;

/**
 * struct p2p_noa_descriptor - noa descriptor
 * @type_count: 255: continuous schedule, 0: reserved
 * @duration: Absent period duration in micro seconds
 * @interval: Absent period interval in micro seconds
 * @start_time: 32 bit tsf time when in starts
 */
struct p2p_noa_descriptor {
	uint8_t type_count;
	uint32_t duration;
	uint32_t interval;
	uint32_t start_time;
} __packed;

/**
 * struct p2p_sub_element_noa - p2p noa element
 * @p2p_sub_id: p2p sub id
 * @p2p_sub_len: p2p sub length
 * @index: identifies instance of NOA su element
 * @oppPS: oppPS state of the AP
 * @ctwindow: ctwindow in TUs
 * @num_descriptors: number of NOA descriptors
 * @noa_descriptors: noa descriptors
 */
struct p2p_sub_element_noa {
	uint8_t p2p_sub_id;
	uint8_t p2p_sub_len;
	uint8_t index;          /* identifies instance of NOA su element */
	uint8_t oppPS:1,        /* oppPS state of the AP */
		ctwindow:7;     /* ctwindow in TUs */
	uint8_t num_descriptors;        /* number of NOA descriptors */
	struct p2p_noa_descriptor noa_descriptors[WMA_MAX_NOA_DESCRIPTORS];
};

/**
 * struct wma_decap_info_t - decapsulation info
 * @hdr: header
 * @hdr_len: header length
 */
struct wma_decap_info_t {
	uint8_t hdr[sizeof(struct ieee80211_qosframe_addr4)];
	int32_t hdr_len;
};

/**
 * enum packet_power_save - packet power save params
 * @WMI_VDEV_PPS_PAID_MATCH: paid match param
 * @WMI_VDEV_PPS_GID_MATCH: gid match param
 * @WMI_VDEV_PPS_EARLY_TIM_CLEAR: early tim clear param
 * @WMI_VDEV_PPS_EARLY_DTIM_CLEAR: early dtim clear param
 * @WMI_VDEV_PPS_EOF_PAD_DELIM: eof pad delim param
 * @WMI_VDEV_PPS_MACADDR_MISMATCH: macaddr mismatch param
 * @WMI_VDEV_PPS_DELIM_CRC_FAIL: delim CRC fail param
 * @WMI_VDEV_PPS_GID_NSTS_ZERO: gid nsts zero param
 * @WMI_VDEV_PPS_RSSI_CHECK: RSSI check param
 * @WMI_VDEV_PPS_5G_EBT: 5G ebt param
 */
typedef enum {
	WMI_VDEV_PPS_PAID_MATCH = 0,
	WMI_VDEV_PPS_GID_MATCH = 1,
	WMI_VDEV_PPS_EARLY_TIM_CLEAR = 2,
	WMI_VDEV_PPS_EARLY_DTIM_CLEAR = 3,
	WMI_VDEV_PPS_EOF_PAD_DELIM = 4,
	WMI_VDEV_PPS_MACADDR_MISMATCH = 5,
	WMI_VDEV_PPS_DELIM_CRC_FAIL = 6,
	WMI_VDEV_PPS_GID_NSTS_ZERO = 7,
	WMI_VDEV_PPS_RSSI_CHECK = 8,
	WMI_VDEV_VHT_SET_GID_MGMT = 9,
	WMI_VDEV_PPS_5G_EBT = 10
} packet_power_save;

/**
 * enum green_tx_param - green tx parameters
 * @WMI_VDEV_PARAM_GTX_HT_MCS: ht mcs param
 * @WMI_VDEV_PARAM_GTX_VHT_MCS: vht mcs param
 * @WMI_VDEV_PARAM_GTX_USR_CFG: user cfg param
 * @WMI_VDEV_PARAM_GTX_THRE: thre param
 * @WMI_VDEV_PARAM_GTX_MARGIN: green tx margin param
 * @WMI_VDEV_PARAM_GTX_STEP: green tx step param
 * @WMI_VDEV_PARAM_GTX_MINTPC: mintpc param
 * @WMI_VDEV_PARAM_GTX_BW_MASK: bandwidth mask
 */
typedef enum {
	WMI_VDEV_PARAM_GTX_HT_MCS,
	WMI_VDEV_PARAM_GTX_VHT_MCS,
	WMI_VDEV_PARAM_GTX_USR_CFG,
	WMI_VDEV_PARAM_GTX_THRE,
	WMI_VDEV_PARAM_GTX_MARGIN,
	WMI_VDEV_PARAM_GTX_STEP,
	WMI_VDEV_PARAM_GTX_MINTPC,
	WMI_VDEV_PARAM_GTX_BW_MASK,
} green_tx_param;

/**
 * enum uapsd_ac - U-APSD Access Categories
 * @UAPSD_BE: best effort
 * @UAPSD_BK: back ground
 * @UAPSD_VI: video
 * @UAPSD_VO: voice
 */
enum uapsd_ac {
	UAPSD_BE,
	UAPSD_BK,
	UAPSD_VI,
	UAPSD_VO
};

/**
 * wma_disable_uapsd_per_ac() - disable uapsd per ac
 * @wmi_handle: wma handle
 * @vdev_id: vdev id
 * @ac: access category
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_disable_uapsd_per_ac(tp_wma_handle wma_handle,
				    uint32_t vdev_id, enum uapsd_ac ac);

/**
 * enum uapsd_up - U-APSD User Priorities
 * @UAPSD_UP_BE: best effort
 * @UAPSD_UP_BK: back ground
 * @UAPSD_UP_RESV: reserve
 * @UAPSD_UP_EE: Excellent Effort
 * @UAPSD_UP_CL: Critical Applications
 * @UAPSD_UP_VI: video
 * @UAPSD_UP_VO: voice
 * @UAPSD_UP_NC: Network Control
 */
enum uapsd_up {
	UAPSD_UP_BE,
	UAPSD_UP_BK,
	UAPSD_UP_RESV,
	UAPSD_UP_EE,
	UAPSD_UP_CL,
	UAPSD_UP_VI,
	UAPSD_UP_VO,
	UAPSD_UP_NC,
	UAPSD_UP_MAX
};

/**
 * struct wma_roam_invoke_cmd - roam invoke command
 * @vdev_id: vdev id
 * @bssid: mac address
 * @ch_freq: channel frequency
 * @frame_len: frame length, includs mac header, fixed params and ies
 * @frame_buf: buffer contaning probe response or beacon
 * @is_same_bssid: flag to indicate if roaming is requested for same bssid
 * @forced_roaming: Roaming to be done without giving bssid, and channel.
 */
struct wma_roam_invoke_cmd {
	uint32_t vdev_id;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	uint32_t ch_freq;
	uint32_t frame_len;
	uint8_t *frame_buf;
	uint8_t is_same_bssid;
	bool forced_roaming;
};

A_UINT32 e_csr_auth_type_to_rsn_authmode(enum csr_akm_type authtype,
					 eCsrEncryptionType encr);
A_UINT32 e_csr_encryption_type_to_rsn_cipherset(eCsrEncryptionType encr);

/**
 * wma_trigger_uapsd_params() - set trigger uapsd parameter
 * @wmi_handle: wma handle
 * @vdev_id: vdev id
 * @trigger_uapsd_params: trigger uapsd parameters
 *
 * This function sets the trigger uapsd
 * params such as service interval, delay
 * interval and suspend interval which
 * will be used by the firmware to send
 * trigger frames periodically when there
 * is no traffic on the transmit side.
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_trigger_uapsd_params(tp_wma_handle wma_handle, uint32_t vdev_id,
				    tp_wma_trigger_uapsd_params
				    trigger_uapsd_params);

void wma_send_flush_logs_to_fw(tp_wma_handle wma_handle);
void wma_log_completion_timeout(void *data);

#ifdef FEATURE_RSSI_MONITOR
/**
 * wma_set_rssi_monitoring() - set rssi monitoring
 * @handle: WMA handle
 * @req: rssi monitoring request structure
 *
 * This function takes the incoming @req and sends it down to the
 * firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_set_rssi_monitoring(tp_wma_handle wma,
				   struct rssi_monitor_param *req);
#else /* FEATURE_RSSI_MONITOR */
static inline
QDF_STATUS wma_set_rssi_monitoring(tp_wma_handle wma,
				   struct rssi_monitor_param *req)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_RSSI_MONITOR */

/**
 * wma_map_pcl_weights  - Map WMA pcl weights to wmi pcl weights
 * @pcl_weight: Input PCL weight to be converted to wmi format
 *
 * Return: wmi_pcl_chan_weight
 */
wmi_pcl_chan_weight wma_map_pcl_weights(uint32_t pcl_weight);

QDF_STATUS wma_send_set_pcl_cmd(tp_wma_handle wma_handle,
				struct set_pcl_req *msg);

QDF_STATUS wma_send_pdev_set_hw_mode_cmd(tp_wma_handle wma_handle,
		struct policy_mgr_hw_mode *msg);

QDF_STATUS wma_send_pdev_set_dual_mac_config(tp_wma_handle wma_handle,
		struct policy_mgr_dual_mac_config *msg);
QDF_STATUS wma_send_pdev_set_antenna_mode(tp_wma_handle wma_handle,
		struct sir_antenna_mode_param *msg);

struct wma_target_req *wma_fill_hold_req(tp_wma_handle wma,
				    uint8_t vdev_id, uint32_t msg_type,
				    uint8_t type, void *params,
				    uint32_t timeout);

int wma_mgmt_tx_completion_handler(void *handle, uint8_t *cmpl_event_params,
				   uint32_t len);
int wma_mgmt_tx_bundle_completion_handler(void *handle,
	uint8_t *cmpl_event_params, uint32_t len);
uint32_t wma_get_vht_ch_width(void);
QDF_STATUS
wma_config_debug_module_cmd(wmi_unified_t wmi_handle, A_UINT32 param,
		A_UINT32 val, A_UINT32 *module_id_bitmap,
		A_UINT32 bitmap_len);

#ifdef FEATURE_LFR_SUBNET_DETECTION
/**
 * wma_set_gateway_params() - set gateway parameters
 * @wma: WMA handle
 * @req: gateway update request parameter structure
 *
 * This function takes the incoming @req and sends it down to the
 * firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_set_gateway_params(tp_wma_handle wma,
				  struct gateway_update_req_param *req);
#else
static inline
QDF_STATUS wma_set_gateway_params(tp_wma_handle wma,
				  struct gateway_update_req_param *req)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_LFR_SUBNET_DETECTION */

QDF_STATUS wma_lro_config_cmd(void *handle,
	 struct cdp_lro_hash_config *wma_lro_cmd);

QDF_STATUS wma_ht40_stop_obss_scan(tp_wma_handle wma_handle,
				int32_t vdev_id);

QDF_STATUS wma_process_fw_test_cmd(WMA_HANDLE handle,
				   struct set_fwtest_params *wma_fwtest);

QDF_STATUS wma_send_ht40_obss_scanind(tp_wma_handle wma,
	struct obss_ht40_scanind *req);

uint32_t wma_get_num_of_setbits_from_bitmask(uint32_t mask);

#ifdef FEATURE_WLAN_APF
/**
 *  wma_get_apf_caps_event_handler() - Event handler for get apf capability
 *  @handle: WMA global handle
 *  @cmd_param_info: command event data
 *  @len: Length of @cmd_param_info
 *
 *  Return: 0 on Success or Errno on failure
 */
int wma_get_apf_caps_event_handler(void *handle,
				   u_int8_t *cmd_param_info,
				   u_int32_t len);

/**
 * wma_get_apf_capabilities - Send get apf capability to firmware
 * @wma_handle: wma handle
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS wma_get_apf_capabilities(tp_wma_handle wma);

/**
 *  wma_set_apf_instructions - Set apf instructions to firmware
 *  @wma: wma handle
 *  @apf_set_offload: APF offload information to set to firmware
 *
 *  Return: QDF_STATUS enumeration
 */
QDF_STATUS
wma_set_apf_instructions(tp_wma_handle wma,
			 struct sir_apf_set_offload *apf_set_offload);

/**
 * wma_send_apf_enable_cmd - Send apf enable/disable cmd
 * @wma_handle: wma handle
 * @vdev_id: vdev id
 * @apf_enable: true: Enable APF Int., false: Disable APF Int.
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS wma_send_apf_enable_cmd(WMA_HANDLE handle, uint8_t vdev_id,
				   bool apf_enable);

/**
 * wma_send_apf_write_work_memory_cmd - Command to write into the apf work
 * memory
 * @wma_handle: wma handle
 * @write_params: APF parameters for the write operation
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS
wma_send_apf_write_work_memory_cmd(WMA_HANDLE handle,
				   struct wmi_apf_write_memory_params
								*write_params);

/**
 * wma_send_apf_read_work_memory_cmd - Command to get part of apf work memory
 * @wma_handle: wma handle
 * @callback: HDD callback to receive apf get mem event
 * @context: Context for the HDD callback
 * @read_params: APF parameters for the get operation
 *
 * Return: QDF_STATUS enumeration.
 */
QDF_STATUS
wma_send_apf_read_work_memory_cmd(WMA_HANDLE handle,
				  struct wmi_apf_read_memory_params
								*read_params);

/**
 * wma_apf_read_work_memory_event_handler - Event handler for get apf mem
 * operation
 * @handle: wma handle
 * @evt_buf: Buffer pointer to the event
 * @len: Length of the event buffer
 *
 * Return: status.
 */
int wma_apf_read_work_memory_event_handler(void *handle, uint8_t *evt_buf,
					   uint32_t len);
#else /* FEATURE_WLAN_APF */
static inline QDF_STATUS wma_get_apf_capabilities(tp_wma_handle wma)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wma_set_apf_instructions(tp_wma_handle wma,
			 struct sir_apf_set_offload *apf_set_offload)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_WLAN_APF */

void wma_process_set_pdev_ie_req(tp_wma_handle wma,
		struct set_ie_param *ie_params);
void wma_process_set_pdev_ht_ie_req(tp_wma_handle wma,
		struct set_ie_param *ie_params);
void wma_process_set_pdev_vht_ie_req(tp_wma_handle wma,
		struct set_ie_param *ie_params);

QDF_STATUS wma_remove_peer(tp_wma_handle wma, uint8_t *mac_addr,
			   uint8_t vdev_id, bool no_fw_peer_delete);

/**
 * wma_create_peer() - Call wma_add_peer() to send peer create command to fw
 * and setup cdp peer
 * @wma: wma handle
 * @peer_addr: peer mac address
 * @peer_type: peer type
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_create_peer(tp_wma_handle wma,
			   uint8_t peer_addr[QDF_MAC_ADDR_SIZE],
			   u_int32_t peer_type, u_int8_t vdev_id);

QDF_STATUS wma_peer_unmap_conf_cb(uint8_t vdev_id,
				  uint32_t peer_id_cnt,
				  uint16_t *peer_id_list);

bool wma_objmgr_peer_exist(tp_wma_handle wma,
			   uint8_t *peer_addr, uint8_t *peer_vdev_id);

/**
 * wma_get_cca_stats() - send request to fw to get CCA
 * @wmi_hdl: wma handle
 * @vdev_id: vdev id
 *
 * Return: QDF status
 */
QDF_STATUS wma_get_cca_stats(tp_wma_handle wma_handle,
				uint8_t vdev_id);

struct wma_ini_config *wma_get_ini_handle(tp_wma_handle wma_handle);

/**
 * wma_chan_phy__mode() - get host phymode for channel
 * @freq: channel freq
 * @chan_width: maximum channel width possible
 * @dot11_mode: maximum phy_mode possible
 *
 * Return: return host phymode
 */
enum wlan_phymode wma_chan_phy_mode(uint32_t freq, enum phy_ch_width chan_width,
				    uint8_t dot11_mode);

/**
 * wma_host_to_fw_phymode() - convert host to fw phymode
 * @host_phymode: phymode to convert
 *
 * Return: one of the values defined in enum WMI_HOST_WLAN_PHY_MODE;
 *         or WMI_HOST_MODE_UNKNOWN if the conversion fails
 */
WMI_HOST_WLAN_PHY_MODE wma_host_to_fw_phymode(enum wlan_phymode host_phymode);

/**
 * wma_fw_to_host_phymode() - convert fw to host phymode
 * @phymode: phymode to convert
 *
 * Return: one of the values defined in enum wlan_phymode;
 *         or WLAN_PHYMODE_AUTO if the conversion fails
 */
enum wlan_phymode wma_fw_to_host_phymode(WMI_HOST_WLAN_PHY_MODE phymode);


#ifdef FEATURE_OEM_DATA_SUPPORT
/**
 * wma_start_oem_req_cmd() - send oem request command to fw
 * @wma_handle: wma handle
 * @oem_data_req: the pointer of oem req buf
 *
 * Return: QDF status
 */
QDF_STATUS wma_start_oem_req_cmd(tp_wma_handle wma_handle,
				 struct oem_data_req *oem_data_req);
#endif

#ifdef FEATURE_OEM_DATA
/**
 * wma_start_oem_data_cmd() - send oem data command to fw
 * @wma_handle: wma handle
 * @oem_data: the pointer of oem data buf
 *
 * Return: QDF status
 */
QDF_STATUS wma_start_oem_data_cmd(tp_wma_handle wma_handle,
				  struct oem_data *oem_data);
#endif

QDF_STATUS wma_enable_disable_caevent_ind(tp_wma_handle wma_handle,
				uint8_t val);
void wma_register_packetdump_callback(
		ol_txrx_pktdump_cb wma_mgmt_tx_packetdump_cb,
		ol_txrx_pktdump_cb wma_mgmt_rx_packetdump_cb);
void wma_deregister_packetdump_callback(void);
void wma_update_sta_inactivity_timeout(tp_wma_handle wma,
		struct sme_sta_inactivity_timeout  *sta_inactivity_timer);

/**
 * wma_form_rx_packet() - form rx cds packet
 * @buf: buffer
 * @mgmt_rx_params: mgmt rx params
 * @rx_pkt: cds packet
 *
 * This functions forms a cds packet from the rx mgmt frame received.
 *
 * Return: 0 for success or error code
 */
int wma_form_rx_packet(qdf_nbuf_t buf,
			struct mgmt_rx_event_params *mgmt_rx_params,
			cds_pkt_t *rx_pkt);

/**
 * wma_mgmt_unified_cmd_send() - send the mgmt tx packet
 * @vdev: objmgr vdev
 * @buf: buffer
 * @desc_id: desc id
 * @mgmt_tx_params: mgmt rx params
 *
 * This functions sends mgmt tx packet to WMI layer.
 *
 * Return: 0 for success or error code
 */
QDF_STATUS wma_mgmt_unified_cmd_send(struct wlan_objmgr_vdev *vdev,
				qdf_nbuf_t buf, uint32_t desc_id,
				void *mgmt_tx_params);

#ifndef CONFIG_HL_SUPPORT
/**
 * wma_mgmt_nbuf_unmap_cb() - dma unmap for pending mgmt pkts
 * @pdev: objmgr pdev
 * @buf: buffer
 *
 * This function does the dma unmap of the pending mgmt packet cleanup
 *
 * Return: None
 */
void wma_mgmt_nbuf_unmap_cb(struct wlan_objmgr_pdev *pdev,
			    qdf_nbuf_t buf);
/**
 * wma_mgmt_nbuf_unmap_cb() - dma unmap for pending mgmt pkts
 * @pdev: objmgr pdev
 * @buf: buffer
 *
 * This is a cb function drains all mgmt packets of a vdev.
 * This is called in event of target going down without sending completions.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_mgmt_frame_fill_peer_cb(struct wlan_objmgr_peer *peer,
				       qdf_nbuf_t buf);
#else
static inline void wma_mgmt_nbuf_unmap_cb(struct wlan_objmgr_pdev *pdev,
					  qdf_nbuf_t buf)
{}
static inline QDF_STATUS wma_mgmt_frame_fill_peer_cb(struct wlan_objmgr_peer *peer,
						     qdf_nbuf_t buf)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * wma_chan_info_event_handler() - chan info event handler
 * @handle: wma handle
 * @event_buf: event handler data
 * @len: length of @event_buf
 *
 * this function will handle the WMI_CHAN_INFO_EVENTID
 *
 * Return: int
 */
int wma_chan_info_event_handler(void *handle, uint8_t *event_buf,
						uint32_t len);

/**
 * wma_update_vdev_pause_bitmap() - update vdev pause bitmap
 * @vdev_id: the Id of the vdev to configure
 * @value: value pause bitmap value
 *
 * Return: None
 */
static inline
void wma_vdev_update_pause_bitmap(uint8_t vdev_id, uint16_t value)
{
	tp_wma_handle wma = (tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *iface;

	if (!wma) {
		wma_err("WMA context is invald!");
		return;
	}

	if (vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev_id: %d", vdev_id);
		return;
	}

	iface = &wma->interfaces[vdev_id];

	if (!iface) {
		wma_err("Interface is NULL");
		return;
	}

	iface->pause_bitmap = value;
}

/**
 * wma_vdev_get_pause_bitmap() - Get vdev pause bitmap
 * @vdev_id: the Id of the vdev to configure
 *
 * Return: Vdev pause bitmap value else 0 on error
 */
static inline
uint16_t wma_vdev_get_pause_bitmap(uint8_t vdev_id)
{
	tp_wma_handle wma = (tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *iface;

	if (!wma) {
		wma_err("WMA context is invald!");
		return 0;
	}

	iface = &wma->interfaces[vdev_id];

	if (!iface) {
		wma_err("Interface is NULL");
		return 0;
	}

	return iface->pause_bitmap;
}

/**
 * wma_vdev_is_device_in_low_pwr_mode - is device in power save mode
 * @vdev_id: the Id of the vdev to configure
 *
 * Return: true if device is in low power mode else false
 */
static inline bool wma_vdev_is_device_in_low_pwr_mode(uint8_t vdev_id)
{
	tp_wma_handle wma = (tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *iface;

	if (!wma) {
		wma_err("WMA context is invalid!");
		return 0;
	}

	iface = &wma->interfaces[vdev_id];

	if (!iface) {
		wma_err("Interface is NULL");
		return 0;
	}

	return iface->in_bmps || wma->in_imps;
}

/**
 * wma_vdev_get_dtim_period - Get dtim period value from mlme
 * @vdev_id: vdev index number
 * @value: pointer to the value to fill out
 *
 * Note caller must verify return status before using value
 *
 * Return: QDF_STATUS_SUCCESS when fetched a valid value from cfg else
 * QDF_STATUS_E_FAILURE
 */
static inline
QDF_STATUS wma_vdev_get_dtim_period(uint8_t vdev_id, uint8_t *value)
{
	tp_wma_handle wma = (tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *iface;
	/* set value to zero */
	*value = 0;

	if (!wma)
		return QDF_STATUS_E_FAILURE;

	iface = &wma->interfaces[vdev_id];

	if (!iface)
		return QDF_STATUS_E_FAILURE;

	*value = iface->dtimPeriod;
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_vdev_get_beacon_interval - Get beacon interval from mlme
 * @vdev_id: vdev index number
 * @value: pointer to the value to fill out
 *
 * Note caller must verify return status before using value
 *
 * Return: QDF_STATUS_SUCCESS when fetched a valid value from cfg else
 * QDF_STATUS_E_FAILURE
 */
static inline
QDF_STATUS wma_vdev_get_beacon_interval(uint8_t  vdev_id, uint16_t *value)
{
	tp_wma_handle wma = (tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *iface;
	/* set value to zero */
	*value = 0;

	if (!wma)
		return QDF_STATUS_E_FAILURE;

	iface = &wma->interfaces[vdev_id];

	if (!iface)
		return QDF_STATUS_E_FAILURE;

	*value = iface->beaconInterval;
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_get_vdev_rate_flag - Get beacon rate flag from mlme
 * @vdev_id: vdev index number
 * @rate_flag: pointer to the value to fill out
 *
 * Note caller must verify return status before using value
 *
 * Return: QDF_STATUS_SUCCESS when fetched a valid value from mlme else
 * QDF_STATUS_E_FAILURE
 */
static inline QDF_STATUS
wma_get_vdev_rate_flag(struct wlan_objmgr_vdev *vdev, uint32_t *rate_flag)
{
	struct vdev_mlme_obj *mlme_obj;

	if (!vdev) {
		wma_err("vdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!mlme_obj) {
		wma_err("ailed to get mlme_obj");
		return QDF_STATUS_E_FAILURE;
	}

	*rate_flag = mlme_obj->mgmt.rate_info.rate_flags;
	return QDF_STATUS_SUCCESS;
}

/**
 * wma_vdev_set_pause_bit() - Set a bit in vdev pause bitmap
 * @vdev_id: the Id of the vdev to configure
 * @bit_pos: set bit position in pause bitmap
 *
 * Return: None
 */
static inline
void wma_vdev_set_pause_bit(uint8_t vdev_id, wmi_tx_pause_type bit_pos)
{
	tp_wma_handle wma = (tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *iface;

	if (!wma) {
		wma_err("WMA context is invalid!");
		return;
	}

	iface = &wma->interfaces[vdev_id];

	if (!iface) {
		wma_err("Interface is NULL");
		return;
	}

	iface->pause_bitmap |= (1 << bit_pos);
}

/**
 * wma_vdev_clear_pause_bit() - Clear a bit from vdev pause bitmap
 * @vdev_id: the Id of the vdev to configure
 * @bit_pos: set bit position in pause bitmap
 *
 * Return: None
 */
static inline
void wma_vdev_clear_pause_bit(uint8_t vdev_id, wmi_tx_pause_type bit_pos)
{
	tp_wma_handle wma = (tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *iface;

	if (!wma) {
		wma_err("WMA context is invalid!");
		return;
	}

	iface = &wma->interfaces[vdev_id];

	if (!iface) {
		wma_err("Interface is NULL");
		return;
	}

	iface->pause_bitmap &= ~(1 << bit_pos);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wma_send_roam_preauth_status() - Send the preauth status to wmi
 * @handle: WMA handle
 * @roam_req: Pointer to wmi_roam_auth_status_params from sae
 *
 * Return: None
 */
void
wma_send_roam_preauth_status(tp_wma_handle wma_handle,
			     struct wmi_roam_auth_status_params *params);
#else
static inline void
wma_send_roam_preauth_status(tp_wma_handle wma_handle,
			     struct wmi_roam_auth_status_params *params)
{}
#endif

/**
 * wma_handle_roam_sync_timeout() - Update roaming status at wma layer
 * @wma_handle: wma handle
 * @info: Info for roaming start timer
 *
 * This function gets called in case of roaming offload timer get expired
 *
 * Return: None
 */
void wma_handle_roam_sync_timeout(tp_wma_handle wma_handle,
				  struct roam_sync_timeout_timer_info *info);

#ifdef WMI_INTERFACE_EVENT_LOGGING
static inline void wma_print_wmi_cmd_log(uint32_t count,
					 qdf_abstract_print *print,
					 void *print_priv)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (wma) {
		print(print_priv, "Command Log (count %u)", count);
		wmi_print_cmd_log(wma->wmi_handle, count, print, print_priv);
	}
}

static inline void wma_print_wmi_cmd_tx_cmp_log(uint32_t count,
						qdf_abstract_print *print,
						void *print_priv)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (wma) {
		print(print_priv, "Command Tx Complete Log (count %u)", count);
		wmi_print_cmd_tx_cmp_log(wma->wmi_handle, count, print,
					 print_priv);
	}
}

static inline void wma_print_wmi_mgmt_cmd_log(uint32_t count,
					      qdf_abstract_print *print,
					      void *print_priv)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (wma) {
		print(print_priv, "Management Command Log (count %u)", count);
		wmi_print_mgmt_cmd_log(wma->wmi_handle, count, print,
				       print_priv);
	}
}

static inline void wma_print_wmi_mgmt_cmd_tx_cmp_log(uint32_t count,
						     qdf_abstract_print *print,
						     void *print_priv)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (wma) {
		print(print_priv,
		"Management Command Tx Complete Log (count %u)", count);
		wmi_print_mgmt_cmd_tx_cmp_log(wma->wmi_handle, count, print,
					      print_priv);
	}
}

static inline void wma_print_wmi_event_log(uint32_t count,
					   qdf_abstract_print *print,
					   void *print_priv)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (wma) {
		print(print_priv, "Event Log (count %u)", count);
		wmi_print_event_log(wma->wmi_handle, count, print, print_priv);
	}
}

static inline void wma_print_wmi_rx_event_log(uint32_t count,
					      qdf_abstract_print *print,
					      void *print_priv)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (wma) {
		print(print_priv, "Rx Event Log (count %u)", count);
		wmi_print_rx_event_log(wma->wmi_handle, count, print,
				       print_priv);
	}
}

static inline void wma_print_wmi_mgmt_event_log(uint32_t count,
						qdf_abstract_print *print,
						void *print_priv)
{
	t_wma_handle *wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (wma) {
		print(print_priv, "Management Event Log (count %u)", count);
		wmi_print_mgmt_event_log(wma->wmi_handle, count, print,
					 print_priv);
	}
}
#else

static inline void wma_print_wmi_cmd_log(uint32_t count,
					 qdf_abstract_print *print,
					 void *print_priv)
{
}

static inline void wma_print_wmi_cmd_tx_cmp_log(uint32_t count,
						qdf_abstract_print *print,
						void *print_priv)
{
}

static inline void wma_print_wmi_mgmt_cmd_log(uint32_t count,
					      qdf_abstract_print *print,
					      void *print_priv)
{
}

static inline void wma_print_wmi_mgmt_cmd_tx_cmp_log(uint32_t count,
						     qdf_abstract_print *print,
						     void *print_priv)
{
}

static inline void wma_print_wmi_event_log(uint32_t count,
					   qdf_abstract_print *print,
					   void *print_priv)
{
}

static inline void wma_print_wmi_rx_event_log(uint32_t count,
					      qdf_abstract_print *print,
					      void *print_priv)
{
}

static inline void wma_print_wmi_mgmt_event_log(uint32_t count,
						qdf_abstract_print *print,
						void *print_priv)
{
}
#endif /* WMI_INTERFACE_EVENT_LOGGING */

/**
 * wma_set_rx_reorder_timeout_val() - set rx recorder timeout value
 * @wma_handle: pointer to wma handle
 * @reorder_timeout: rx reorder timeout value
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_set_rx_reorder_timeout_val(tp_wma_handle wma_handle,
	struct sir_set_rx_reorder_timeout_val *reorder_timeout);

/**
 * wma_set_rx_blocksize() - set rx blocksize
 * @wma_handle: pointer to wma handle
 * @peer_rx_blocksize: rx blocksize for peer mac
 *
 * Return: QDF_STATUS_SUCCESS for success or error code.
 */
QDF_STATUS wma_set_rx_blocksize(tp_wma_handle wma_handle,
	struct sir_peer_set_rx_blocksize *peer_rx_blocksize);
/**
 * wma_configure_smps_params() - Configures the smps parameters to set
 * @vdev_id: Virtual device for the command
 * @param_id: SMPS parameter ID
 * @param_val: Value to be set for the parameter
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS wma_configure_smps_params(uint32_t vdev_id, uint32_t param_id,
							uint32_t param_val);

/*
 * wma_chip_power_save_failure_detected_handler() - chip pwr save fail detected
 * event handler
 * @handle: wma handle
 * @cmd_param_info: event handler data
 * @len: length of @cmd_param_info
 *
 * Return: QDF_STATUS_SUCCESS on success; error code otherwise
 */
int wma_chip_power_save_failure_detected_handler(void *handle,
						 uint8_t *cmd_param_info,
						 uint32_t len);

/**
 * wma_get_chain_rssi() - send wmi cmd to get chain rssi
 * @wma_handle: wma handler
 * @req_params: requset params
 *
 * Return: Return QDF_STATUS
 */
QDF_STATUS wma_get_chain_rssi(tp_wma_handle wma_handle,
		struct get_chain_rssi_req_params *req_params);

/**
 * wma_config_bmiss_bcnt_params() - set bmiss config parameters
 * @vdev_id: virtual device for the command
 * @first_cnt: bmiss first value
 * @final_cnt: bmiss final value
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS wma_config_bmiss_bcnt_params(uint32_t vdev_id, uint32_t first_cnt,
		uint32_t final_cnt);

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD
/**
 * wma_check_and_set_wake_timer(): checks all interfaces and if any interface
 * has install_key pending, sets timer pattern in fw to wake up host after
 * specified time has elapsed.
 * @time: time after which host wants to be awaken.
 *
 * Return: None
 */
void wma_check_and_set_wake_timer(uint32_t time);
#endif

/**
 * wma_delete_invalid_peer_entries() - Delete invalid peer entries stored
 * @vdev_id: virtual interface id
 * @peer_mac_addr: Peer MAC address
 *
 * Removes the invalid peer mac entry from wma node
 */
void wma_delete_invalid_peer_entries(uint8_t vdev_id, uint8_t *peer_mac_addr);

/**
 * wma_rx_invalid_peer_ind() - the callback for DP to notify WMA layer
 * invalid peer data is received, this function will send message to
 * lim module.
 * @vdev_id: virtual device ID
 * @wh: Pointer to 802.11 frame header
 *
 * Return: 0 for success or non-zero on failure
 */
uint8_t wma_rx_invalid_peer_ind(uint8_t vdev_id, void *wh);

/**
 * wma_dp_send_delba_ind() - the callback for DP to notify WMA layer
 * to del ba of rx
 * @vdev_id: vdev id
 * @peer_macaddr: peer mac address
 * @tid: tid of rx
 * @reason_code: reason code
 * @cdp_rcode: CDP reason code for sending DELBA
 *
 * Return: 0 for success or non-zero on failure
 */
int wma_dp_send_delba_ind(uint8_t vdev_id,
			  uint8_t *peer_macaddr,
			  uint8_t tid,
			  uint8_t reason_code,
			  enum cdp_delba_rcode cdp_rcode);

/**
 * is_roam_inprogress() - Is vdev in progress
 * @vdev_id: vdev of interest
 *
 * Return: true if roaming, false otherwise
 */
bool wma_is_roam_in_progress(uint32_t vdev_id);

/**
 * wma_get_psoc_from_scn_handle() - API to get psoc from scn handle
 * @scn_handle: opaque wma handle
 *
 * API to get psoc from scn handle
 *
 * Return: psoc context or null in case of failure
 */
struct wlan_objmgr_psoc *wma_get_psoc_from_scn_handle(void *scn_handle);

/**
 * wma_set_peer_ucast_cipher() - Update unicast cipher fof the peer
 * @mac_addr: peer mac address
 * @cipher: peer cipher type
 *
 * Return: None
 */
void wma_set_peer_ucast_cipher(uint8_t *mac_addr,
			       enum wlan_crypto_cipher_type cipher);

/**
 * wma_update_set_key() - Update WMA layer for set key
 * @session_id: vdev session identifier
 * @pairwise: denotes if it is pairwise or group key
 * @key_index: Key Index
 * @cipher_type: cipher type being used for the encryption/decryption
 *
 * Return: None
 */
void wma_update_set_key(uint8_t session_id, bool pairwise,
			uint8_t key_index,
			enum wlan_crypto_cipher_type cipher_type);

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

int wma_motion_det_host_event_handler(void *handle, u_int8_t *event,
				      u_int32_t len);

/**
 * wma_motion_det_base_line_host_event_handler - md baselining event handler
 * @handle: WMA global handle
 * @event: motion detection baselining event
 * @len: Length of cmd
 *
 * Return: 0 on success, else error on failure
 */
int wma_motion_det_base_line_host_event_handler(void *handle, u_int8_t *event,
						u_int32_t len);
#endif /* WLAN_FEATURE_MOTION_DETECTION */

/**
 * wma_add_bss_peer_sta() - creat bss peer when sta connect
 * @vdev_id: vdev id
 * @bssid: AP bssid
 * @roam_sync: if roam sync is in progress
 * @is_resp_required: Peer create response is expected from firmware.
 * This flag will be set to true for initial connection and false for
 * LFR2 case.
 *
 * Return: 0 on success, else error on failure
 */
QDF_STATUS wma_add_bss_peer_sta(uint8_t vdev_id, uint8_t *bssid,
				bool is_resp_required);

/**
 * wma_send_vdev_stop() - WMA api to send vdev stop to fw
 * @vdev_id: vdev id
 *
 * Return: 0 on success, else error on failure
 */
QDF_STATUS wma_send_vdev_stop(uint8_t vdev_id);

/**
 * wma_pre_assoc_req() - wma pre assoc req when sta connect
 * @add_bss: add bss param
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_pre_assoc_req(struct bss_params *add_bss);

/**
 * wma_add_bss_lfr3() - add bss during LFR3 offload roaming
 * @wma: wma handler
 * @add_bss: add bss param
 *
 * Return: None
 */
void wma_add_bss_lfr3(tp_wma_handle wma, struct bss_params *add_bss);

#ifdef WLAN_FEATURE_HOST_ROAM
/**
 * wma_add_bss_lfr2_vdev_start() - add bss and start vdev during host roaming
 * @vdev: vdev in object manager
 * @add_bss: add bss param
 *
 * Return: None
 */
QDF_STATUS wma_add_bss_lfr2_vdev_start(struct wlan_objmgr_vdev *vdev,
				       struct bss_params *add_bss);
#endif

/**
 * wma_send_peer_assoc_req() - wma send peer assoc req when sta connect
 * @add_bss: add bss param
 *
 * Return: None
 */
QDF_STATUS wma_send_peer_assoc_req(struct bss_params *add_bss);

/**
 * wma_get_rx_chainmask() - API to get rx chainmask from mac phy capability
 * @pdev_id: pdev id
 * @chainmask_2g: pointer to return 2g chainmask
 * @chainmask_5g: pointer to return 5g chainmask
 *
 * API to get rx chainmask from mac phy capability directly.
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS wma_get_rx_chainmask(uint8_t pdev_id, uint32_t *chainmask_2g,
				uint32_t *chainmask_5g);

/**
 * wma_handle_channel_switch_resp() - handle channel switch resp
 * @wma: wma handle
 * @rsp: response for channel switch
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_handle_channel_switch_resp(tp_wma_handle wma,
					  struct vdev_start_response *rsp);

/**
 * wma_pre_chan_switch_setup() - handler before channel switch vdev start
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_pre_chan_switch_setup(uint8_t vdev_id);

/**
 * wma_post_chan_switch_setup() - handler after channel switch vdev start
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_post_chan_switch_setup(uint8_t vdev_id);

/**
 * wma_vdev_pre_start() - prepare vdev start
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_vdev_pre_start(uint8_t vdev_id, bool restart);

/**
 * wma_remove_bss_peer_on_vdev_start_failure() - remove the bss peers in case of
 * vdev start request failure
 * @wma: wma handle.
 * @vdev_id: vdev id
 *
 * This API deletes the BSS peer created during ADD BSS in case of ADD BSS
 * request sent to the FW fails.
 *
 * Return: None;
 */
void wma_remove_bss_peer_on_vdev_start_failure(tp_wma_handle wma,
					       uint8_t vdev_id);

/**
 * wma_send_add_bss_resp() - send add bss failure
 * @wma: wma handle.
 * @vdev_id: vdev id
 * @status: status
 *
 * Return: None
 */
void wma_send_add_bss_resp(tp_wma_handle wma, uint8_t vdev_id,
			   QDF_STATUS status);

/**
 * wma_post_vdev_start_setup() - wma post vdev start handler
 * @wma: wma handle.
 * @vdev_id: vdev id
 *
 * Return: Success or Failure status
 */
QDF_STATUS wma_post_vdev_start_setup(uint8_t vdev_id);

/**
 * wma_pre_vdev_start_setup() - wma pre vdev start handler
 * @wma: wma handle.
 * @vdev_id: vdev id
 * @addbss_param: bss param
 *
 * Return: Success or Failure status
 */
QDF_STATUS wma_pre_vdev_start_setup(uint8_t vdev_id,
				    struct bss_params *add_bss);

#ifdef FEATURE_ANI_LEVEL_REQUEST
/**
 * wma_send_ani_level_request() - Send get ani level cmd to WMI
 * @wma_handle: wma handle.
 * @freqs: pointer to channels for which ANI level has to be retrieved
 * @num_freqs: number of channels in the above parameter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_send_ani_level_request(tp_wma_handle wma_handle,
				      uint32_t *freqs, uint8_t num_freqs);
#endif /* FEATURE_ANI_LEVEL_REQUEST */

/**
 * wma_vdev_detach() - send vdev delete command to fw
 * @wma_handle: wma handle
 * @pdel_vdev_req_param: del vdev params
 *
 * Return: QDF status
 */
QDF_STATUS wma_vdev_detach(struct del_vdev_params *pdel_vdev_req_param);
#endif

