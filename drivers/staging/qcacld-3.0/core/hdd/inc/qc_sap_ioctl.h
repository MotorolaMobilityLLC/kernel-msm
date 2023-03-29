/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

#ifndef _QC_SAP_IOCTL_H_
#define _QC_SAP_IOCTL_H_

/*
 * QCSAP ioctls.
 */

/*
 * Channel List Info
 */

struct channel_list_info {
	uint8_t num_channels;
	uint8_t channels[NUM_CHANNELS];
};

#ifdef __linux__
/*
 * Wireless Extensions API, private ioctl interfaces.
 *
 * NB: Even-numbered ioctl numbers have set semantics and are privileged!
 *     (regardless of the incorrect comment in wireless.h!)
 */

#define QCSAP_IOCTL_SETPARAM          (SIOCIWFIRSTPRIV + 0)
#define QCSAP_IOCTL_GETPARAM          (SIOCIWFIRSTPRIV + 1)
/* (SIOCIWFIRSTPRIV+2) is unused */
#define QCSAP_IOCTL_SET_NONE_GET_THREE (SIOCIWFIRSTPRIV + 3)
#define WE_GET_TSF 1
#define QCSAP_IOCTL_GET_STAWPAIE      (SIOCIWFIRSTPRIV + 4)
#define QCSAP_IOCTL_STOPBSS           (SIOCIWFIRSTPRIV + 6)
#define QCSAP_IOCTL_VERSION           (SIOCIWFIRSTPRIV + 7)
/* (SIOCIWFIRSTPRIV + 8) is unused */
#define QCSAP_IOCTL_GET_CHANNEL       (SIOCIWFIRSTPRIV + 9)
#define QCSAP_IOCTL_ASSOC_STA_MACADDR (SIOCIWFIRSTPRIV + 10)
#define QCSAP_IOCTL_DISASSOC_STA      (SIOCIWFIRSTPRIV + 11)
#define QCSAP_IOCTL_SET_PKTLOG        (SIOCIWFIRSTPRIV + 12)

/* Private ioctls and their sub-ioctls */
#define QCSAP_PRIV_GET_CHAR_SET_NONE   (SIOCIWFIRSTPRIV + 13)
#define QCSAP_GET_STATS 1
#define QCSAP_LIST_FW_PROFILE 2

/* (SIOCIWFIRSTPRIV + 14) is unused */

#define QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE (SIOCIWFIRSTPRIV + 15)
#define WE_SET_WLAN_DBG 1
#define WE_SET_DP_TRACE 2
#define QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE (SIOCIWFIRSTPRIV + 16)
#define WE_UNIT_TEST_CMD   7
/*
 * <ioctl>
 * ch_avoid - unit test SAP channel avoidance
 *
 * @INPUT: chan avoid ranges
 *
 * @OUTPUT: none
 *
 * This IOCTL is used to fake a channel avoidance event.
 * To test SAP/GO chan switch during chan avoid event process.
 *
 * @E.g: iwpriv wlan0 ch_avoid 2452 2462
 *
 * Supported Feature: SAP chan avoidance.
 *
 * Usage: Internal
 *
 * </ioctl>
 */
#define WE_SET_CHAN_AVOID 21

#define WE_SET_THERMAL_THROTTLE_CFG     27

#define WE_P2P_NOA_CMD  2

#define QCSAP_IOCTL_MODIFY_ACL          (SIOCIWFIRSTPRIV + 18)
#define QCSAP_IOCTL_GET_CHANNEL_LIST    (SIOCIWFIRSTPRIV + 19)
#define QCSAP_IOCTL_SET_TX_POWER        (SIOCIWFIRSTPRIV + 20)
#define QCSAP_IOCTL_GET_STA_INFO        (SIOCIWFIRSTPRIV + 21)
#define QCSAP_IOCTL_SET_MAX_TX_POWER    (SIOCIWFIRSTPRIV + 22)
#define QCSAP_IOCTL_GET_INI_CFG         (SIOCIWFIRSTPRIV + 25)

#define QCSAP_IOCTL_SET_TWO_INT_GET_NONE (SIOCIWFIRSTPRIV + 28)
/* QCSAP_IOCTL_SET_TWO_INT_GET_NONE sub commands */
#define QCSAP_IOCTL_SET_FW_CRASH_INJECT 1
#define QCSAP_IOCTL_DUMP_DP_TRACE_LEVEL 2
#define QCSAP_ENABLE_FW_PROFILE          3
#define QCSAP_SET_FW_PROFILE_HIST_INTVL  4
/* Private sub-ioctl for initiating WoW suspend without Apps suspend */
#define QCSAP_SET_WLAN_SUSPEND  5
#define QCSAP_SET_WLAN_RESUME   6
#define QCSAP_SET_BA_AGEING_TIMEOUT 7

#define QCSAP_IOCTL_PRIV_GET_RSSI       (SIOCIWFIRSTPRIV + 29)
#define QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED (SIOCIWFIRSTPRIV + 31)
#define QCSAP_IOCTL_GET_BA_AGEING_TIMEOUT (SIOCIWFIRSTPRIV + 32)

#define MAX_VAR_ARGS         7

#define QCSAP_IOCTL_MAX_STR_LEN 1024

#define RC_2_RATE_IDX(_rc)              ((_rc) & 0x7)
#define HT_RC_2_STREAMS(_rc)            ((((_rc) & 0x78) >> 3) + 1)

#define RC_2_RATE_IDX_11AC(_rc)         ((_rc) & 0xf)
#define HT_RC_2_STREAMS_11AC(_rc)       ((((_rc) & 0x30) >> 4) + 1)

#define RC_2_RATE_IDX_11AX(_rc)         ((_rc) & 0x1f)
#define HT_RC_2_STREAMS_11AX(_rc)       (((_rc) >> 5) & 0x7)

/*
 * <ioctl>
 * setRadar - simulate a radar event
 *
 * @INPUT: None
 *
 * @OUTPUT: None
 *
 * This IOCTL is used to simulate a radar event, state machines for
 * SAP will behave as same way in which a radar event is reported by WMA
 *
 * @E.g: iwpriv wlan0 setRadar
 *
 * Supported Feature: DFS
 *
 * Usage: Internal
 *
 * </ioctl>
 */

/*
 * <ioctl>
 * setRadarDbg - enable/disable radar specific logs
 *
 * @INPUT: 1/0
 *
 * @OUTPUT: None
 *
 * This IOCTL is enable radar phyerror info in wma
 *
 * @E.g: iwpriv wlan0 setRadarDbg <enable>
 *  iwpriv wlan0 setRadarDbg 1
 *
 * Supported Feature: DFS
 *
 * Usage: Internal
 *
 * </ioctl>
 */
enum {
	QCSAP_PARAM_MAX_ASSOC = 1,
	QCSAP_PARAM_GET_WLAN_DBG,
	QCSAP_PARAM_CLR_ACL = 4,
	QCSAP_PARAM_ACL_MODE,
	QCSAP_PARAM_HIDE_SSID,
	QCSAP_PARAM_SET_MC_RATE,
	QCSAP_PARAM_SET_TXRX_FW_STATS,
	QCSAP_PARAM_SET_MCC_CHANNEL_LATENCY,
	QCSAP_PARAM_SET_MCC_CHANNEL_QUOTA,
	QCSAP_DBGLOG_LOG_LEVEL,
	QCSAP_DBGLOG_VAP_ENABLE,
	QCSAP_DBGLOG_VAP_DISABLE,
	QCSAP_DBGLOG_MODULE_ENABLE,
	QCSAP_DBGLOG_MODULE_DISABLE,
	QCSAP_DBGLOG_MOD_LOG_LEVEL,
	QCSAP_DBGLOG_TYPE,
	QCSAP_DBGLOG_REPORT_ENABLE,
	QCASAP_TXRX_FWSTATS_RESET,
	QCSAP_PARAM_RTSCTS,
	QCASAP_SET_11N_RATE,
	QCASAP_SET_VHT_RATE,
	QCASAP_SHORT_GI,
	QCSAP_SET_AMPDU,
	QCSAP_SET_AMSDU,
	QCSAP_GTX_HT_MCS,
	QCSAP_GTX_VHT_MCS,
	QCSAP_GTX_USRCFG,
	QCSAP_GTX_THRE,
	QCSAP_GTX_MARGIN,
	QCSAP_GTX_STEP,
	QCSAP_GTX_MINTPC,
	QCSAP_GTX_BWMASK,
	QCASAP_SET_TM_LEVEL,
	QCASAP_SET_DFS_IGNORE_CAC,
	QCASAP_GET_DFS_NOL,
	QCASAP_SET_DFS_NOL,
	QCSAP_PARAM_SET_CHANNEL_CHANGE,
	QCASAP_SET_DFS_TARGET_CHNL,
	QCASAP_SET_RADAR_CMD,
	QCSAP_GET_ACL,
	QCASAP_TX_CHAINMASK_CMD,
	QCASAP_RX_CHAINMASK_CMD,
	QCASAP_NSS_CMD,
	QCSAP_IPA_UC_STAT,
	QCASAP_SET_PHYMODE,
	QCASAP_GET_TEMP_CMD,
	QCASAP_DUMP_STATS,
	QCASAP_CLEAR_STATS,
	QCASAP_SET_RADAR_DBG,
	QCSAP_GET_FW_PROFILE_DATA,
	QCSAP_START_FW_PROFILING,
	QCSAP_CAP_TSF,
	QCSAP_GET_TSF,
	QCSAP_PARAM_CONC_SYSTEM_PREF,
	QCASAP_PARAM_LDPC,
	QCASAP_PARAM_TX_STBC,
	QCASAP_PARAM_RX_STBC,
	QCSAP_PARAM_CHAN_WIDTH,
	QCSAP_PARAM_SET_TXRX_STATS,
	QCASAP_SET_11AX_RATE,
	QCASAP_SET_PEER_RATE, /* Not Supported */
	QCASAP_PARAM_DCM,
	QCASAP_PARAM_RANGE_EXT,
	QCSAP_SET_DEFAULT_AMPDU,
	QCSAP_ENABLE_RTS_BURSTING,
	QCASAP_SET_HE_BSS_COLOR,
	QCSAP_SET_BTCOEX_MODE,
	QCSAP_SET_BTCOEX_LOW_RSSI_THRESHOLD,
};

int iw_get_channel_list_with_cc(struct net_device *dev,
				mac_handle_t mac_handle,
				struct iw_request_info *info,
				union iwreq_data *wrqu,
				char *extra);

#endif /* __linux__ */

#endif /*_QC_SAP_IOCTL_H_*/
