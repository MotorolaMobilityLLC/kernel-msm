/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */


/** ------------------------------------------------------------------------ *
    ------------------------------------------------------------------------ *


    \file wlan_hdd_wext.c

    \brief Airgo Linux Wireless Extensions Common Control Plane Types and
    interfaces.

    $Id: wlan_hdd_wext.c,v 1.34 2007/04/14 01:49:23 jimz Exp jimz $This file defines all of the types that are utilized by the CCP module
    of the "Portable" HDD.   This file also includes the underlying Linux
    Wireless Extensions Data types referred to by CCP.

  ======================================================================== */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include <macTrace.h>
#include <wlan_hdd_includes.h>
#include <wlan_btc_svc.h>
#include <wlan_nlink_common.h>
#ifdef WLAN_BTAMP_FEATURE
#include <bap_hdd_main.h>
#endif
#include <vos_api.h>
#include <net/arp.h>
#include "ccmApi.h"
#include "sirParams.h"
#include "csrApi.h"
#include "csrInsideApi.h"
#if defined WLAN_FEATURE_VOWIFI
#include "smeRrmInternal.h"
#endif
#include <aniGlobal.h>
#include "dot11f.h"
#include <wlan_hdd_wowl.h>
#include <wlan_hdd_cfg.h>
#include <wlan_hdd_wmm.h>
#include "utilsApi.h"
#include "wlan_hdd_p2p.h"
#ifdef FEATURE_WLAN_TDLS
#include "wlan_hdd_tdls.h"
#endif

#include "ieee80211_common.h"
#include "ol_if_athvar.h"
#include "dbglog_host.h"
#include "wma.h"

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include "wlan_hdd_power.h"
#include "qwlan_version.h"
#include "wlan_hdd_host_offload.h"
#include "wlan_hdd_keep_alive.h"
#ifdef WLAN_FEATURE_PACKET_FILTERING
#include "wlan_hdd_packet_filtering.h"
#endif

#include <linux/wireless.h>
#include <net/cfg80211.h>
#include "wlan_qct_tl.h"

#include "wlan_hdd_misc.h"
#include "bap_hdd_misc.h"

#include "wlan_hdd_dev_pwr.h"
#include "qc_sap_ioctl.h"
#include "sme_Api.h"
#include "wlan_qct_wda.h"
#include "vos_trace.h"
#include "wlan_hdd_assoc.h"

#ifdef QCA_PKT_PROTO_TRACE
#include "vos_packet.h"
#endif /* QCA_PKT_PROTO_TRACE */

#ifdef CONFIG_HAS_EARLYSUSPEND
#include "wlan_hdd_cfg80211.h"
#endif

#ifdef FEATURE_OEM_DATA_SUPPORT
#define MAX_OEM_DATA_RSP_LEN            2047
#endif

#define HDD_FINISH_ULA_TIME_OUT         800
#define HDD_SET_MCBC_FILTERS_TO_FW      1
#define HDD_DELETE_MCBC_FILTERS_FROM_FW 0

extern int wlan_hdd_cfg80211_update_band(struct wiphy *wiphy, eCsrBand eBand);
static int ioctl_debug;
module_param(ioctl_debug, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

/* To Validate Channel against the Frequency and Vice-Versa */
static const hdd_freq_chan_map_t freq_chan_map[] = { {2412, 1}, {2417, 2},
        {2422, 3}, {2427, 4}, {2432, 5}, {2437, 6}, {2442, 7}, {2447, 8},
        {2452, 9}, {2457, 10}, {2462, 11}, {2467 ,12}, {2472, 13},
        {2484, 14}, {4920, 240}, {4940, 244}, {4960, 248}, {4980, 252},
        {5040, 208}, {5060, 212}, {5080, 216}, {5180, 36}, {5200, 40}, {5220, 44},
        {5240, 48}, {5260, 52}, {5280, 56}, {5300, 60}, {5320, 64}, {5500, 100},
        {5520, 104}, {5540, 108}, {5560, 112}, {5580, 116}, {5600, 120},
        {5620, 124}, {5640, 128}, {5660, 132}, {5680, 136}, {5700, 140},
        {5720, 144}, {5745, 149}, {5765, 153}, {5785, 157}, {5805, 161},
        {5825, 165} };

#define FREQ_CHAN_MAP_TABLE_SIZE (sizeof(freq_chan_map)/sizeof(freq_chan_map[0]))

#define RC_2_RATE_IDX(_rc)        ((_rc) & 0x7)
#define HT_RC_2_STREAMS(_rc)    ((((_rc) & 0x78) >> 3) + 1)

#define RC_2_RATE_IDX_11AC(_rc)        ((_rc) & 0xf)
#define HT_RC_2_STREAMS_11AC(_rc)    ((((_rc) & 0x30) >> 4) + 1)

/* Private ioctls and their sub-ioctls */
#define WLAN_PRIV_SET_INT_GET_NONE    (SIOCIWFIRSTPRIV + 0)
#define WE_SET_11D_STATE     1
#define WE_WOWL              2
#define WE_SET_POWER         3
#define WE_SET_MAX_ASSOC     4
#define WE_SET_SAP_AUTO_CHANNEL_SELECTION     5
#define WE_SET_DATA_INACTIVITY_TO  6
#define WE_SET_MAX_TX_POWER  7
#define WE_SET_HIGHER_DTIM_TRANSITION   8
#define WE_SET_TM_LEVEL      9
#define WE_SET_PHYMODE       10
#define WE_SET_NSS           11
#define WE_SET_LDPC          12
#define WE_SET_TX_STBC       13
#define WE_SET_RX_STBC       14
#define WE_SET_SHORT_GI      15
#define WE_SET_RTSCTS        16
#define WE_SET_CHWIDTH       17
#define WE_SET_ANI_EN_DIS    18
#define WE_SET_ANI_POLL_PERIOD    19
#define WE_SET_ANI_LISTEN_PERIOD  20
#define WE_SET_ANI_OFDM_LEVEL     21
#define WE_SET_ANI_CCK_LEVEL      22
#define WE_SET_DYNAMIC_BW         23
#define WE_SET_TX_CHAINMASK  24
#define WE_SET_RX_CHAINMASK  25
#define WE_SET_11N_RATE      26
#define WE_SET_AMPDU         27
#define WE_SET_AMSDU         28
#define WE_SET_TXPOW_2G      29
#define WE_SET_TXPOW_5G      30
/* Private ioctl for firmware debug log */
#define WE_DBGLOG_LOG_LEVEL             31
#define WE_DBGLOG_VAP_ENABLE            32
#define WE_DBGLOG_VAP_DISABLE           33
#define WE_DBGLOG_MODULE_ENABLE         34
#define WE_DBGLOG_MODULE_DISABLE        35
#define WE_DBGLOG_MOD_LOG_LEVEL         36
#define WE_DBGLOG_TYPE                  37
#define WE_SET_TXRX_FWSTATS             38
#define WE_SET_VHT_RATE                 39
#define WE_DBGLOG_REPORT_ENABLE         40
#define WE_TXRX_FWSTATS_RESET           41
#define WE_SET_MAX_TX_POWER_2_4   42
#define WE_SET_MAX_TX_POWER_5_0   43
#define WE_SET_POWER_GATING       44
/* Private ioctl for packet power save */
#define  WE_PPS_PAID_MATCH              45
#define  WE_PPS_GID_MATCH               46
#define  WE_PPS_EARLY_TIM_CLEAR         47
#define  WE_PPS_EARLY_DTIM_CLEAR        48
#define  WE_PPS_EOF_PAD_DELIM           49
#define  WE_PPS_MACADDR_MISMATCH        50
#define  WE_PPS_DELIM_CRC_FAIL          51
#define  WE_PPS_GID_NSTS_ZERO           52
#define  WE_PPS_RSSI_CHECK              53
/* 54 is unused */
#define WE_SET_HTSMPS                   55
/* Private ioctl for QPower */
#define WE_SET_QPOWER_MAX_PSPOLL_COUNT            56
#define WE_SET_QPOWER_MAX_TX_BEFORE_WAKE          57
#define WE_SET_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL   58
#define WE_SET_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL 59

#define WE_SET_BURST_ENABLE             60
#define WE_SET_BURST_DUR                61
/* GTX Commands */
#define WE_SET_GTX_HT_MCS               62
#define WE_SET_GTX_VHT_MCS              63
#define WE_SET_GTX_USRCFG               64
#define WE_SET_GTX_THRE                 65
#define WE_SET_GTX_MARGIN               66
#define WE_SET_GTX_STEP                 67
#define WE_SET_GTX_MINTPC               68
#define WE_SET_GTX_BWMASK               69
/* Private ioctl to configure MCC home channels time quota and latency */
#define WE_MCC_CONFIG_LATENCY           70
#define WE_MCC_CONFIG_QUOTA             71
/* Private IOCTL for debug connection issues */
#define WE_SET_DEBUG_LOG                72
#define  WE_SET_SCAN_BAND_PREFERENCE    73
#ifdef WE_SET_TX_POWER
#undef WE_SET_TX_POWER
#endif
#define WE_SET_TX_POWER                 74
/* Private ioctl for earlyrx power save feature */
#define WE_SET_EARLY_RX_ADJUST_ENABLE         75
#define WE_SET_EARLY_RX_TGT_BMISS_NUM         76
#define WE_SET_EARLY_RX_BMISS_SAMPLE_CYCLE    77
#define WE_SET_EARLY_RX_SLOP_STEP             78
#define WE_SET_EARLY_RX_INIT_SLOP             79
#define WE_SET_EARLY_RX_ADJUST_PAUSE          80
#define WE_SET_MC_RATE                        81
#define WE_SET_EARLY_RX_DRIFT_SAMPLE          82
/* Private ioctl for packet power save */
#define WE_PPS_5G_EBT                         83

/* Private ioctls and their sub-ioctls */
#define WLAN_PRIV_SET_NONE_GET_INT    (SIOCIWFIRSTPRIV + 1)
#define WE_GET_11D_STATE     1
#define WE_IBSS_STATUS       2

#define WE_GET_WLAN_DBG      4
#define WE_GET_MAX_ASSOC     6
/* 7 is unused */
#define WE_GET_SAP_AUTO_CHANNEL_SELECTION 8
#define WE_GET_CONCURRENCY_MODE 9
#define WE_GET_NSS           11
#define WE_GET_LDPC          12
#define WE_GET_TX_STBC       13
#define WE_GET_RX_STBC       14
#define WE_GET_SHORT_GI      15
#define WE_GET_RTSCTS        16
#define WE_GET_CHWIDTH       17
#define WE_GET_ANI_EN_DIS    18
#define WE_GET_ANI_POLL_PERIOD    19
#define WE_GET_ANI_LISTEN_PERIOD  20
#define WE_GET_ANI_OFDM_LEVEL     21
#define WE_GET_ANI_CCK_LEVEL      22
#define WE_GET_DYNAMIC_BW         23
#define WE_GET_TX_CHAINMASK  24
#define WE_GET_RX_CHAINMASK  25
#define WE_GET_11N_RATE      26
#define WE_GET_AMPDU         27
#define WE_GET_AMSDU         28
#define WE_GET_TXPOW_2G      29
#define WE_GET_TXPOW_5G      30
#define WE_GET_POWER_GATING  31
#define WE_GET_PPS_PAID_MATCH           32
#define WE_GET_PPS_GID_MATCH            33
#define WE_GET_PPS_EARLY_TIM_CLEAR      34
#define WE_GET_PPS_EARLY_DTIM_CLEAR     35
#define WE_GET_PPS_EOF_PAD_DELIM        36
#define WE_GET_PPS_MACADDR_MISMATCH     37
#define WE_GET_PPS_DELIM_CRC_FAIL       38
#define WE_GET_PPS_GID_NSTS_ZERO        39
#define WE_GET_PPS_RSSI_CHECK           40
/* Private ioctl for QPower */
#define WE_GET_QPOWER_MAX_PSPOLL_COUNT            41
#define WE_GET_QPOWER_MAX_TX_BEFORE_WAKE          42
#define WE_GET_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL   43
#define WE_GET_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL 44
#define WE_GET_BURST_ENABLE             45
#define WE_GET_BURST_DUR                46
/* GTX Commands */
#define WE_GET_GTX_HT_MCS               47
#define WE_GET_GTX_VHT_MCS              48
#define WE_GET_GTX_USRCFG               49
#define WE_GET_GTX_THRE                 50
#define WE_GET_GTX_MARGIN               51
#define WE_GET_GTX_STEP                 52
#define WE_GET_GTX_MINTPC               53
#define WE_GET_GTX_BWMASK               54
#define WE_GET_SCAN_BAND_PREFERENCE     55

/* Private ioctls and their sub-ioctls */
#define WLAN_PRIV_SET_INT_GET_INT     (SIOCIWFIRSTPRIV + 2)

/* Private ioctls and their sub-ioctls */
#define WLAN_PRIV_SET_CHAR_GET_NONE   (SIOCIWFIRSTPRIV + 3)
#define WE_WOWL_ADD_PTRN     1
#define WE_WOWL_DEL_PTRN     2
#if defined WLAN_FEATURE_VOWIFI
#define WE_NEIGHBOR_REPORT_REQUEST 3
#endif
#define WE_SET_AP_WPS_IE     4  //This is called in station mode to set probe rsp ie.
#define WE_SET_CONFIG        5

/* Private ioctls and their sub-ioctls */
#define WLAN_PRIV_SET_THREE_INT_GET_NONE   (SIOCIWFIRSTPRIV + 4)
#define WE_SET_WLAN_DBG      1
/* 2 is unused */
#define WE_SET_SAP_CHANNELS  3

/* Private ioctls and their sub-ioctls */
#define WLAN_PRIV_GET_CHAR_SET_NONE   (SIOCIWFIRSTPRIV + 5)
#define WE_WLAN_VERSION      1
#define WE_GET_STATS         2
#define WE_GET_CFG           3
#define WE_GET_WMM_STATUS    4
#define WE_GET_CHANNEL_LIST  5
#ifdef WLAN_FEATURE_11AC
#define WE_GET_RSSI          6
#endif
#define WE_GET_ROAM_RSSI     7
#ifdef FEATURE_WLAN_TDLS
#define WE_GET_TDLS_PEERS    8
#endif
#ifdef WLAN_FEATURE_11W
#define WE_GET_11W_INFO      9
#endif
#define WE_GET_STATES        10
#define WE_GET_PHYMODE       12
#ifdef FEATURE_OEM_DATA_SUPPORT
#define WE_GET_OEM_DATA_CAP  13
#endif

/* Private ioctls and their sub-ioctls */
#define WLAN_PRIV_SET_NONE_GET_NONE   (SIOCIWFIRSTPRIV + 6)
#define WE_CLEAR_STATS       1
#ifdef WLAN_BTAMP_FEATURE
#define WE_ENABLE_AMP        4
#define WE_DISABLE_AMP       5
#endif /* WLAN_BTAMP_FEATURE */
#define WE_ENABLE_DXE_STALL_DETECT 6
#define WE_DISPLAY_DXE_SNAP_SHOT   7
#define WE_SET_REASSOC_TRIGGER     8
#define WE_DISPLAY_DATAPATH_SNAP_SHOT    9
#define WE_DUMP_AGC_START          11
#define WE_DUMP_AGC                12
#define WE_DUMP_CHANINFO_START     13
#define WE_DUMP_CHANINFO           14
#define WE_DUMP_WATCHDOG           15
#ifdef CONFIG_ATH_PCIE_ACCESS_DEBUG
#define WE_DUMP_PCIE_LOG           16
#endif
#define WE_GET_RECOVERY_STAT       17

/* Private ioctls and their sub-ioctls */
#define WLAN_PRIV_SET_VAR_INT_GET_NONE   (SIOCIWFIRSTPRIV + 7)
#define WE_LOG_DUMP_CMD      1

#define WE_P2P_NOA_CMD       2
//IOCTL to configure MCC params
#define WE_MCC_CONFIG_CREDENTIAL 3
#define WE_MCC_CONFIG_PARAMS  4

#ifdef FEATURE_WLAN_TDLS
#define WE_TDLS_CONFIG_PARAMS   5
#endif

#define WE_UNIT_TEST_CMD   7

#define WE_MTRACE_DUMP_CMD    8
#define WE_MTRACE_SELECTIVE_MODULE_LOG_ENABLE_CMD    9

#ifdef FEATURE_WLAN_TDLS
#undef  MAX_VAR_ARGS
#define MAX_VAR_ARGS         11
#else
#define MAX_VAR_ARGS         7
#endif


/* Private ioctls (with no sub-ioctls) */
/* note that they must be odd so that they have "get" semantics */
#define WLAN_PRIV_ADD_TSPEC (SIOCIWFIRSTPRIV +  9)
#define WLAN_PRIV_DEL_TSPEC (SIOCIWFIRSTPRIV + 11)
#define WLAN_PRIV_GET_TSPEC (SIOCIWFIRSTPRIV + 13)

/* (SIOCIWFIRSTPRIV + 8)  is currently unused */
/* (SIOCIWFIRSTPRIV + 16) is currently unused */
/* (SIOCIWFIRSTPRIV + 10) is currently unused */
/* (SIOCIWFIRSTPRIV + 12) is currently unused */
/* (SIOCIWFIRSTPRIV + 14) is currently unused */
/* (SIOCIWFIRSTPRIV + 15) is currently unused */

#ifdef FEATURE_OEM_DATA_SUPPORT
/* Private ioctls for setting the measurement configuration */
#define WLAN_PRIV_SET_OEM_DATA_REQ (SIOCIWFIRSTPRIV + 17)
#define WLAN_PRIV_GET_OEM_DATA_RSP (SIOCIWFIRSTPRIV + 19)
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
#define WLAN_PRIV_SET_FTIES             (SIOCIWFIRSTPRIV + 20)
#endif

/* Private ioctl for setting the host offload feature */
#define WLAN_PRIV_SET_HOST_OFFLOAD (SIOCIWFIRSTPRIV + 18)

/* Private ioctl to get the statistics */
#define WLAN_GET_WLAN_STATISTICS (SIOCIWFIRSTPRIV + 21)

/* Private ioctl to set the Keep Alive Params */
#define WLAN_SET_KEEPALIVE_PARAMS (SIOCIWFIRSTPRIV + 22)
#ifdef WLAN_FEATURE_PACKET_FILTERING
/* Private ioctl to set the Packet Filtering Params */
#define WLAN_SET_PACKET_FILTER_PARAMS (SIOCIWFIRSTPRIV + 23)
#endif

#ifdef FEATURE_WLAN_SCAN_PNO
/* Private ioctl to get the statistics */
#define WLAN_SET_PNO (SIOCIWFIRSTPRIV + 24)
#endif

#define WLAN_SET_BAND_CONFIG  (SIOCIWFIRSTPRIV + 25)  /*Don't change this number*/

#define WLAN_PRIV_SET_MCBC_FILTER    (SIOCIWFIRSTPRIV + 26)
#define WLAN_PRIV_CLEAR_MCBC_FILTER  (SIOCIWFIRSTPRIV + 27)
/* Private ioctl to trigger reassociation */

#define WLAN_SET_POWER_PARAMS        (SIOCIWFIRSTPRIV + 29)

#define WLAN_GET_LINK_SPEED          (SIOCIWFIRSTPRIV + 31)

/* Private ioctls and their sub-ioctls */
#define WLAN_PRIV_SET_TWO_INT_GET_NONE   (SIOCIWFIRSTPRIV + 28)
#define WE_SET_SMPS_PARAM    1
#ifdef DEBUG
#define WE_SET_FW_CRASH_INJECT    2
#endif

#define WLAN_STATS_INVALID            0
#define WLAN_STATS_RETRY_CNT          1
#define WLAN_STATS_MUL_RETRY_CNT      2
#define WLAN_STATS_TX_FRM_CNT         3
#define WLAN_STATS_RX_FRM_CNT         4
#define WLAN_STATS_FRM_DUP_CNT        5
#define WLAN_STATS_FAIL_CNT           6
#define WLAN_STATS_RTS_FAIL_CNT       7
#define WLAN_STATS_ACK_FAIL_CNT       8
#define WLAN_STATS_RTS_SUC_CNT        9
#define WLAN_STATS_RX_DISCARD_CNT     10
#define WLAN_STATS_RX_ERROR_CNT       11
#define WLAN_STATS_TX_BYTE_CNT        12

#define WLAN_STATS_RX_BYTE_CNT        13
#define WLAN_STATS_RX_RATE            14
#define WLAN_STATS_TX_RATE            15

#define WLAN_STATS_RX_UC_BYTE_CNT     16
#define WLAN_STATS_RX_MC_BYTE_CNT     17
#define WLAN_STATS_RX_BC_BYTE_CNT     18
#define WLAN_STATS_TX_UC_BYTE_CNT     19
#define WLAN_STATS_TX_MC_BYTE_CNT     20
#define WLAN_STATS_TX_BC_BYTE_CNT     21

#define FILL_TLV(__p, __type, __size, __val, __tlen) do {           \
        if ((__tlen + __size + 2) < WE_MAX_STR_LEN)                 \
        {                                                           \
            *__p++ = __type;                                        \
            *__p++ = __size;                                        \
            memcpy(__p, __val, __size);                             \
            __p += __size;                                          \
            __tlen += __size + 2;                                   \
        }                                                           \
        else                                                        \
        {                                                           \
            hddLog(VOS_TRACE_LEVEL_ERROR, "FILL_TLV Failed!!!");  \
        }                                                           \
    } while(0);

#define VERSION_VALUE_MAX_LEN 32

#define TX_PER_TRACKING_DEFAULT_RATIO             5
#define TX_PER_TRACKING_MAX_RATIO                10
#define TX_PER_TRACKING_DEFAULT_WATERMARK         5

#define WLAN_ADAPTER 0
#define P2P_ADAPTER  1

/*MCC Configuration parameters */
enum {
    MCC_SCHEDULE_TIME_SLICE_CFG_PARAM = 1,
    MCC_MAX_NULL_SEND_TIME_CFG_PARAM,
    MCC_TX_EARLY_STOP_TIME_CFG_PARAM,
    MCC_RX_DRAIN_TIME_CFG_PARAM,
    MCC_CHANNEL_SWITCH_TIME_CFG_PARAM,
    MCC_MIN_CHANNEL_TIME_CFG_PARAM,
    MCC_PARK_BEFORE_TBTT_CFG_PARAM,
    MCC_MIN_AFTER_DTIM_CFG_PARAM,
    MCC_TOO_CLOSE_MARGIN_CFG_PARAM,
};

static const struct qwlan_hw qwlan_hw_list[] = {
    {
        .id = AR6320_REV1_VERSION,
        .subid = 0,
        .name = "QCA6174_REV1",
    },
    {
        .id = AR6320_REV1_1_VERSION,
        .subid = 0x1,
        .name = "QCA6174_REV1_1",
    },
    {
        .id = AR6320_REV1_3_VERSION,
        .subid = 0x2,
        .name = "QCA6174_REV1_3",
    },
    {
        .id = AR6320_REV2_1_VERSION,
        .subid = 0x4,
        .name = "QCA6174_REV2_1",
    },
    {
        .id = AR6320_REV2_1_VERSION,
        .subid = 0x5,
        .name = "QCA6174_REV2_2",
    },
    {
        .id = AR6320_REV3_VERSION,
        .subid = 0x6,
        .name = "QCA6174_REV2.3",
    },
    {
        .id = AR6320_REV3_VERSION,
        .subid = 0x8,
        .name = "QCA6174_REV3",
    },
    {
        .id = AR6320_REV3_VERSION,
        .subid = 0x9,
        .name = "QCA6174_REV3_1",
    },
    {
        .id = AR6320_REV3_2_VERSION,
        .subid = 0xA,
        .name = "QCA6174_REV3_2",
    }
};

int hdd_validate_mcc_config(hdd_adapter_t *pAdapter, v_UINT_t staId,
                                v_UINT_t arg1, v_UINT_t arg2, v_UINT_t arg3);

#ifdef WLAN_FEATURE_PACKET_FILTERING
int wlan_hdd_set_filter(hdd_context_t *pHddCtx, tpPacketFilterCfg pRequest,
                           v_U8_t sessionId);
#endif

/**---------------------------------------------------------------------------

  \brief mem_alloc_copy_from_user_helper -

   Helper function to allocate buffer and copy user data.

  \param  - wrqu - Pointer to IOCTL Data.
            len  - size

  \return - On Success pointer to buffer, On failure NULL

  --------------------------------------------------------------------------*/
void *mem_alloc_copy_from_user_helper(const void *wrqu_data, size_t len)
{
    u8 *ptr = NULL;

  /* in order to protect the code, an extra byte is post appended to the buffer
   * and the null termination is added.  However, when allocating (len+1) byte
   * of memory, we need to make sure that there is no uint overflow when doing
   * addition. In theory check len < UINT_MAX protects the uint overflow. For
   * wlan private ioctl, the buffer size is much less than UINT_MAX, as a good
   * guess, now, it is assumed that the private command buffer size is no
   * greater than 4K (4096 bytes). So we use 4096 as the upper boundary for now.
   */
    if (len > MAX_USER_COMMAND_SIZE)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "Invalid length");
        return NULL;
    }


    ptr = kmalloc(len + 1, GFP_KERNEL);
    if (NULL == ptr)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "unable to allocate memory");
        return NULL;
    }

    if (copy_from_user(ptr, wrqu_data, len))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
        kfree(ptr);
        return NULL;
    }
    ptr[len] = '\0';
    return ptr;
}

/**---------------------------------------------------------------------------

  \brief hdd_priv_get_data -

   Helper function to get compatible struct iw_point passed to ioctl

  \param  - p_priv_data - pointer to iw_point struct to be filled
            wrqu - Pointer to IOCTL Data received from user space

  \return - 0 if p_priv_data successfully filled
            error otherwise

  --------------------------------------------------------------------------*/
int hdd_priv_get_data(struct iw_point *p_priv_data,
                      union iwreq_data *wrqu)
{
   if ((NULL == p_priv_data) || (NULL == wrqu)) {
      return -EINVAL;
   }

#ifdef CONFIG_COMPAT
   if (is_compat_task()) {
      struct compat_iw_point *p_compat_priv_data;

      /* Compat task: typecast to compat structure and copy the members. */
      p_compat_priv_data = (struct compat_iw_point *) &wrqu->data;

      p_priv_data->pointer = compat_ptr(p_compat_priv_data->pointer);
      p_priv_data->length  = p_compat_priv_data->length;
      p_priv_data->flags   = p_compat_priv_data->flags;
   } else {
#endif /* #ifdef CONFIG_COMPAT */

      /* Non compat task: directly copy the structure. */
      memcpy(p_priv_data, &wrqu->data, sizeof(struct iw_point));

#ifdef CONFIG_COMPAT
   }
#endif /* #ifdef CONFIG_COMPAT */

   return 0;
}


/**---------------------------------------------------------------------------

  \brief hdd_wlan_get_stats -

   Helper function to get stats

  \param  - pAdapter Pointer to the adapter.
            wrqu - Pointer to IOCTL REQUEST Data.
            extra - Pointer to char


  \return - none

  --------------------------------------------------------------------------*/
void hdd_wlan_get_stats(hdd_adapter_t *pAdapter, v_U16_t *length,
                        char *buffer, v_U16_t buf_len)
{
    hdd_tx_rx_stats_t *pStats = &pAdapter->hdd_stats.hddTxRxStats;

    snprintf(buffer, buf_len,
        "\nTransmit"
        "\ncalled %u, dropped %u,"
        "\n      dropped BK %u, BE %u, VI %u, VO %u"
        "\n   classified BK %u, BE %u, VI %u, VO %u"
        "\ncompleted %u,"
        "\n\nReceive"
        "\nchains %u, packets %u, dropped %u, delivered %u, refused %u"
        "\n"
        "\nNetQueue State %s"
        "\ndisable count %u, enable count %u"
        "\n\nTX_FLOW"
        "\nCurrent status %s"
        "\ntx-flow timer start count %u"
        "\npause count %u, unpause count %u",
        pStats->txXmitCalled,
        pStats->txXmitDropped,

        pStats->txXmitDroppedAC[WLANTL_AC_BK],
        pStats->txXmitDroppedAC[WLANTL_AC_BE],
        pStats->txXmitDroppedAC[WLANTL_AC_VI],
        pStats->txXmitDroppedAC[WLANTL_AC_VO],

        pStats->txXmitClassifiedAC[WLANTL_AC_BK],
        pStats->txXmitClassifiedAC[WLANTL_AC_BE],
        pStats->txXmitClassifiedAC[WLANTL_AC_VI],
        pStats->txXmitClassifiedAC[WLANTL_AC_VO],

        pStats->txCompleted,

        pStats->rxChains,
        pStats->rxPackets,
        pStats->rxDropped,
        pStats->rxDelivered,
        pStats->rxRefused,
        (pStats->netq_state_off == TRUE ? "OFF": "ON"),
        pStats->netq_disable_cnt,
        pStats->netq_enable_cnt,
        (pStats->is_txflow_paused == TRUE ? "paused": "unpaused"),
        pStats->txflow_timer_cnt,
        pStats->txflow_pause_cnt,
        pStats->txflow_unpause_cnt
        );
    *length = strlen(buffer) + 1;
}


/**---------------------------------------------------------------------------

  \brief hdd_wlan_get_version() -

   This function use to get Wlan Driver, Firmware, & Hardware Version.

  \param  - pAdapter Pointer to the adapter.
            wrqu - Pointer to IOCTL REQUEST Data.
            extra - Pointer to char

  \return - none

  --------------------------------------------------------------------------*/
void hdd_wlan_get_version(hdd_adapter_t *pAdapter, union iwreq_data *wrqu,
                          char *extra)
{
    tSirVersionString wcnss_SW_version;
    const char *pSWversion;
    const char *pHWversion;
    v_U32_t MSPId = 0, mSPId = 0, SIId = 0, CRMId = 0;

    hdd_context_t *pHddContext;
    int i = 0;

    pHddContext = WLAN_HDD_GET_CTX(pAdapter);
    if (!pHddContext) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "%s:Invalid context, HDD context is null", __func__);
        goto error;
    }

    snprintf(wcnss_SW_version, sizeof(tSirVersionString), "%08x",
        pHddContext->target_fw_version);

    pSWversion = wcnss_SW_version;
    MSPId = (pHddContext->target_fw_version & 0xf0000000) >> 28;
    mSPId = (pHddContext->target_fw_version & 0xf000000) >> 24;
    SIId = (pHddContext->target_fw_version & 0xf00000) >> 20;
    CRMId = pHddContext->target_fw_version & 0x7fff;

    for (i = 0; i < ARRAY_SIZE(qwlan_hw_list); i++) {
        if (pHddContext->target_hw_version == qwlan_hw_list[i].id &&
            pHddContext->target_hw_revision == qwlan_hw_list[i].subid) {
            pHWversion = qwlan_hw_list[i].name;
            break;
        }
    }

    if (i == ARRAY_SIZE(qwlan_hw_list))
        pHWversion = "Unknown";
    pHddContext->target_hw_name = pHWversion;

    if (wrqu) {
        wrqu->data.length = scnprintf(extra, WE_MAX_STR_LEN,
                                     "Host SW:%s, FW:%d.%d.%d.%d, HW:%s",
                                     QWLAN_VERSIONSTR,
                                     MSPId,
                                     mSPId,
                                     SIId,
                                     CRMId,
                                     pHWversion);
    } else {
        pr_info("Host SW:%s, FW:%d.%d.%d.%d, HW:%s\n",
                QWLAN_VERSIONSTR,
                MSPId,
                mSPId,
                SIId,
                CRMId,
                pHWversion);
    }
error:
    return;
}

int hdd_wlan_get_rts_threshold(hdd_adapter_t *pAdapter, union iwreq_data *wrqu)
{
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    v_U32_t threshold = 0,status = 0;

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                "%s:LOGP in Progress. Ignore!!!",__func__);
      return status;
    }

    if ( eHAL_STATUS_SUCCESS !=
                     ccmCfgGetInt(hHal, WNI_CFG_RTS_THRESHOLD, &threshold) )
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                      FL("failed to get ini parameter, WNI_CFG_RTS_THRESHOLD"));
       return -EIO;
    }
    wrqu->rts.value = threshold;

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                 ("Rts-Threshold=%d!!"), wrqu->rts.value);

    EXIT();

    return 0;
}
int hdd_wlan_get_frag_threshold(hdd_adapter_t *pAdapter, union iwreq_data *wrqu)
{
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    v_U32_t threshold = 0,status = 0;

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s:LOGP in Progress. Ignore!!!",__func__);
      return status;
    }

    if ( ccmCfgGetInt(hHal, WNI_CFG_FRAGMENTATION_THRESHOLD, &threshold)
                                                != eHAL_STATUS_SUCCESS )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                      FL("failed to get ini parameter, WNI_CFG_FRAGMENTATION_THRESHOLD"));
        return -EIO;
    }
    wrqu->frag.value = threshold;

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                               ("Frag-Threshold=%d!!"), wrqu->frag.value);

    EXIT();

    return 0;
}

int hdd_wlan_get_freq(v_U32_t channel, v_U32_t *pfreq)
{
    int i;
    if (channel > 0)
    {
        for (i=0; i < FREQ_CHAN_MAP_TABLE_SIZE; i++)
        {
            if (channel == freq_chan_map[i].chan)
            {
                *pfreq = freq_chan_map[i].freq;
                return 1;
            }
        }
    }
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                               ("Invalid channel no=%d!!"), channel);
    return -EINVAL;
}

static v_BOOL_t
hdd_IsAuthTypeRSN( tHalHandle halHandle, eCsrAuthType authType)
{
    v_BOOL_t rsnType = VOS_FALSE;
    // is the authType supported?
    switch (authType)
    {
        case eCSR_AUTH_TYPE_NONE:    //never used
            rsnType = eANI_BOOLEAN_FALSE;
            break;
        // MAC layer authentication types
        case eCSR_AUTH_TYPE_OPEN_SYSTEM:
            rsnType = eANI_BOOLEAN_FALSE;
            break;
        case eCSR_AUTH_TYPE_SHARED_KEY:
            rsnType = eANI_BOOLEAN_FALSE;
            break;
        case eCSR_AUTH_TYPE_AUTOSWITCH:
            rsnType = eANI_BOOLEAN_FALSE;
            break;

        // Upper layer authentication types
        case eCSR_AUTH_TYPE_WPA:
            rsnType = eANI_BOOLEAN_TRUE;
            break;
        case eCSR_AUTH_TYPE_WPA_PSK:
            rsnType = eANI_BOOLEAN_TRUE;
            break;
        case eCSR_AUTH_TYPE_WPA_NONE:
            rsnType = eANI_BOOLEAN_TRUE;
            break;
#ifdef WLAN_FEATURE_VOWIFI_11R
        case eCSR_AUTH_TYPE_FT_RSN:
#endif
        case eCSR_AUTH_TYPE_RSN:
            rsnType = eANI_BOOLEAN_TRUE;
            break;
#ifdef WLAN_FEATURE_VOWIFI_11R
        case eCSR_AUTH_TYPE_FT_RSN_PSK:
#endif
        case eCSR_AUTH_TYPE_RSN_PSK:
#ifdef WLAN_FEATURE_11W
        case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
        case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
#endif
            rsnType = eANI_BOOLEAN_TRUE;
            break;
        //case eCSR_AUTH_TYPE_FAILED:
        case eCSR_AUTH_TYPE_UNKNOWN:
            rsnType = eANI_BOOLEAN_FALSE;
            break;
        default:
            hddLog(LOGE, FL("%s called with unknown authType - default to Open, None"),
                   __func__);
            rsnType = eANI_BOOLEAN_FALSE;
            break;
    }
    hddLog(LOGE, FL("%s called with authType: %d, returned: %d"),
                                             __func__, authType, rsnType);
    return rsnType;
}

static void hdd_GetRssiCB( v_S7_t rssi, tANI_U32 staId, void *pContext )
{
   struct statsContext *pStatsContext;
   hdd_adapter_t *pAdapter;

   if (ioctl_debug)
   {
      pr_info("%s: rssi [%d] STA [%d] pContext [%p]\n",
              __func__, (int)rssi, (int)staId, pContext);
   }

   if (NULL == pContext)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, pContext [%p]",
             __func__, pContext);
      return;
   }

   pStatsContext = pContext;
   pAdapter      = pStatsContext->pAdapter;

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
   spin_lock(&hdd_context_lock);

   if ((NULL == pAdapter) || (RSSI_CONTEXT_MAGIC != pStatsContext->magic))
   {
      /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, pAdapter [%p] magic [%08x]",
              __func__, pAdapter, pStatsContext->magic);
      if (ioctl_debug)
      {
         pr_info("%s: Invalid context, pAdapter [%p] magic [%08x]\n",
                 __func__, pAdapter, pStatsContext->magic);
      }
      return;
   }

   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pStatsContext->magic = 0;

   /* copy over the rssi */
   pAdapter->rssi = rssi;

   /* notify the caller */
   complete(&pStatsContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}

static void hdd_GetSnrCB(tANI_S8 snr, tANI_U32 staId, void *pContext)
{
   struct statsContext *pStatsContext;
   hdd_adapter_t *pAdapter;

   if (ioctl_debug)
   {
      pr_info("%s: snr [%d] STA [%d] pContext [%p]\n",
              __func__, (int)snr, (int)staId, pContext);
   }

   if (NULL == pContext)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, pContext [%p]",
             __func__, pContext);
      return;
   }

   pStatsContext = pContext;
   pAdapter      = pStatsContext->pAdapter;

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
   spin_lock(&hdd_context_lock);

   if ((NULL == pAdapter) || (SNR_CONTEXT_MAGIC != pStatsContext->magic))
   {
      /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, pAdapter [%p] magic [%08x]",
              __func__, pAdapter, pStatsContext->magic);
      if (ioctl_debug)
      {
         pr_info("%s: Invalid context, pAdapter [%p] magic [%08x]\n",
                 __func__, pAdapter, pStatsContext->magic);
      }
      return;
   }

   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pStatsContext->magic = 0;

   /* copy over the snr */
   pAdapter->snr = snr;

   /* notify the caller */
   complete(&pStatsContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}

VOS_STATUS wlan_hdd_get_rssi(hdd_adapter_t *pAdapter, v_S7_t *rssi_value)
{
   struct statsContext context;
   hdd_context_t *pHddCtx;
   hdd_station_ctx_t *pHddStaCtx;
   eHalStatus hstatus;
   unsigned long rc;

   if (NULL == pAdapter)
   {
       hddLog(VOS_TRACE_LEVEL_WARN,
              "%s: Invalid context, pAdapter", __func__);
       return VOS_STATUS_E_FAULT;
   }
   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s:LOGP in Progress. Ignore!!!",__func__);
       /* return a cached value */
       *rssi_value = pAdapter->rssi;
       return VOS_STATUS_SUCCESS;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = RSSI_CONTEXT_MAGIC;

   hstatus = sme_GetRssi(pHddCtx->hHal, hdd_GetRssiCB,
                         pHddStaCtx->conn_info.staId[ 0 ],
                         pHddStaCtx->conn_info.bssId, pAdapter->rssi,
                         &context, pHddCtx->pvosContext);
   if (eHAL_STATUS_SUCCESS != hstatus)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Unable to retrieve RSSI",
              __func__);
       /* we'll returned a cached value below */
   }
   else
   {
       /* request was sent -- wait for the response */
       rc = wait_for_completion_timeout(&context.completion,
                                    msecs_to_jiffies(WLAN_WAIT_TIME_STATS));
       if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
              FL("SME timed out while retrieving RSSI"));
          /* we'll now returned a cached value below */
       }
   }

   /* either we never sent a request, we sent a request and received a
      response or we sent a request and timed out.  if we never sent a
      request or if we sent a request and got a response, we want to
      clear the magic out of paranoia.  if we timed out there is a
      race condition such that the callback function could be
      executing at the same time we are. of primary concern is if the
      callback function had already verified the "magic" but had not
      yet set the completion variable when a timeout occurred. we
      serialize these activities by invalidating the magic while
      holding a shared spinlock which will cause us to block if the
      callback is currently executing */
   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);

   *rssi_value = pAdapter->rssi;

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS wlan_hdd_get_snr(hdd_adapter_t *pAdapter, v_S7_t *snr)
{
   struct statsContext context;
   hdd_context_t *pHddCtx;
   hdd_station_ctx_t *pHddStaCtx;
   eHalStatus hstatus;
   unsigned long rc;
   int valid;

   if (NULL == pAdapter)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: Invalid context, pAdapter", __func__);
       return VOS_STATUS_E_FAULT;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

   valid = wlan_hdd_validate_context(pHddCtx);
   if (0 != valid)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
       return VOS_STATUS_E_FAULT;
   }

   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = SNR_CONTEXT_MAGIC;

   hstatus = sme_GetSnr(pHddCtx->hHal, hdd_GetSnrCB,
                         pHddStaCtx->conn_info.staId[ 0 ],
                         pHddStaCtx->conn_info.bssId,
                         &context);
   if (eHAL_STATUS_SUCCESS != hstatus)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Unable to retrieve RSSI",
              __func__);
       /* we'll returned a cached value below */
   }
   else
   {
       /* request was sent -- wait for the response */
       rc = wait_for_completion_timeout(&context.completion,
                                    msecs_to_jiffies(WLAN_WAIT_TIME_STATS));
       if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
              FL("SME timed out while retrieving SNR"));
          /* we'll now returned a cached value below */
       }
   }

   /* either we never sent a request, we sent a request and received a
      response or we sent a request and timed out.  if we never sent a
      request or if we sent a request and got a response, we want to
      clear the magic out of paranoia.  if we timed out there is a
      race condition such that the callback function could be
      executing at the same time we are. of primary concern is if the
      callback function had already verified the "magic" but had not
      yet set the completion variable when a timeout occurred. we
      serialize these activities by invalidating the magic while
      holding a shared spinlock which will cause us to block if the
      callback is currently executing */
   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);

   *snr = pAdapter->snr;

   return VOS_STATUS_SUCCESS;
}
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)

static void hdd_GetRoamRssiCB( v_S7_t rssi, tANI_U32 staId, void *pContext )
{
   struct statsContext *pStatsContext;
   hdd_adapter_t *pAdapter;
   if (ioctl_debug)
   {
      pr_info("%s: rssi [%d] STA [%d] pContext [%p]\n",
              __func__, (int)rssi, (int)staId, pContext);
   }

   if (NULL == pContext)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, pContext [%p]",
             __func__, pContext);
      return;
   }

   pStatsContext = pContext;
   pAdapter      = pStatsContext->pAdapter;

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
   spin_lock(&hdd_context_lock);

   if ((NULL == pAdapter) || (RSSI_CONTEXT_MAGIC != pStatsContext->magic))
   {
      /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, pAdapter [%p] magic [%08x]",
              __func__, pAdapter, pStatsContext->magic);
      if (ioctl_debug)
      {
         pr_info("%s: Invalid context, pAdapter [%p] magic [%08x]\n",
                 __func__, pAdapter, pStatsContext->magic);
      }
      return;
   }

   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pStatsContext->magic = 0;

   /* copy over the rssi */
   pAdapter->rssi = rssi;

   /* notify the caller */
   complete(&pStatsContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}



VOS_STATUS wlan_hdd_get_roam_rssi(hdd_adapter_t *pAdapter, v_S7_t *rssi_value)
{
   struct statsContext context;
   hdd_context_t *pHddCtx = NULL;
   hdd_station_ctx_t *pHddStaCtx = NULL;
   eHalStatus hstatus;
   unsigned long rc;

   if (NULL == pAdapter)
   {
       hddLog(VOS_TRACE_LEVEL_WARN,
              "%s: Invalid context, pAdapter", __func__);
       return VOS_STATUS_E_FAULT;
   }
   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s:LOGP in Progress. Ignore!!!",__func__);
       /* return a cached value */
       *rssi_value = pAdapter->rssi;
       return VOS_STATUS_SUCCESS;
   }

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   if(eConnectionState_Associated != pHddStaCtx->conn_info.connState)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s:Not associated!",__func__);
       /* return a cached value */
       *rssi_value = 0;
       return VOS_STATUS_SUCCESS;
   }
   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = RSSI_CONTEXT_MAGIC;

   hstatus = sme_GetRoamRssi(pHddCtx->hHal, hdd_GetRoamRssiCB,
                         pHddStaCtx->conn_info.staId[ 0 ],
                         pHddStaCtx->conn_info.bssId,
                         &context, pHddCtx->pvosContext);
   if (eHAL_STATUS_SUCCESS != hstatus)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Unable to retrieve RSSI",
              __func__);
       /* we'll returned a cached value below */
   }
   else
   {
       /* request was sent -- wait for the response */
       rc = wait_for_completion_timeout(&context.completion,
                                    msecs_to_jiffies(WLAN_WAIT_TIME_STATS));
       if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
              FL("SME timed out while retrieving RSSI"));
          /* we'll now returned a cached value below */
       }
   }

   /* either we never sent a request, we sent a request and received a
      response or we sent a request and timed out.  if we never sent a
      request or if we sent a request and got a response, we want to
      clear the magic out of paranoia.  if we timed out there is a
      race condition such that the callback function could be
      executing at the same time we are. of primary concern is if the
      callback function had already verified the "magic" but had not
      yet set the completion variable when a timeout occurred. we
      serialize these activities by invalidating the magic while
      holding a shared spinlock which will cause us to block if the
      callback is currently executing */
   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);

   *rssi_value = pAdapter->rssi;

   return VOS_STATUS_SUCCESS;
}
#endif


void hdd_StatisticsCB( void *pStats, void *pContext )
{
   hdd_adapter_t             *pAdapter      = (hdd_adapter_t *)pContext;
   hdd_stats_t               *pStatsCache   = NULL;
   hdd_wext_state_t *pWextState;
   VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

   tCsrSummaryStatsInfo      *pSummaryStats = NULL;
   tCsrGlobalClassAStatsInfo *pClassAStats  = NULL;
   tCsrGlobalClassBStatsInfo *pClassBStats  = NULL;
   tCsrGlobalClassCStatsInfo *pClassCStats  = NULL;
   tCsrGlobalClassDStatsInfo *pClassDStats  = NULL;
   tCsrPerStaStatsInfo       *pPerStaStats  = NULL;

   if (pAdapter!= NULL)
     pStatsCache = &pAdapter->hdd_stats;


   pSummaryStats = (tCsrSummaryStatsInfo *)pStats;
   pClassAStats  = (tCsrGlobalClassAStatsInfo *)( pSummaryStats + 1 );
   pClassBStats  = (tCsrGlobalClassBStatsInfo *)( pClassAStats + 1 );
   pClassCStats  = (tCsrGlobalClassCStatsInfo *)( pClassBStats + 1 );
   pClassDStats  = (tCsrGlobalClassDStatsInfo *)( pClassCStats + 1 );
   pPerStaStats  = (tCsrPerStaStatsInfo *)( pClassDStats + 1 );

   if (pStatsCache!=NULL)
   {
      // and copy the stats into the cache we keep in the adapter instance structure
      vos_mem_copy( &pStatsCache->summary_stat, pSummaryStats, sizeof( pStatsCache->summary_stat ) );
      vos_mem_copy( &pStatsCache->ClassA_stat, pClassAStats, sizeof( pStatsCache->ClassA_stat ) );
      vos_mem_copy( &pStatsCache->ClassB_stat, pClassBStats, sizeof( pStatsCache->ClassB_stat ) );
      vos_mem_copy( &pStatsCache->ClassC_stat, pClassCStats, sizeof( pStatsCache->ClassC_stat ) );
      vos_mem_copy( &pStatsCache->ClassD_stat, pClassDStats, sizeof( pStatsCache->ClassD_stat ) );
      vos_mem_copy( &pStatsCache->perStaStats, pPerStaStats, sizeof( pStatsCache->perStaStats ) );
   }

    if (pAdapter) {
        pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
        vos_status = vos_event_set(&pWextState->vosevent);
        if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
           hddLog(LOGE, FL("vos_event_set failed"));
           return;
        }
    }
}

void ccmCfgSetCallback(tHalHandle halHandle, tANI_S32 result)
{
   v_CONTEXT_t pVosContext;
   hdd_context_t *pHddCtx;
   VOS_STATUS hdd_reconnect_all_adapters( hdd_context_t *pHddCtx );
#if 0
   hdd_wext_state_t *pWextState;
   v_U32_t roamId;
#endif

   ENTER();

   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS,NULL);

   pHddCtx = (hdd_context_t*) vos_get_context(VOS_MODULE_ID_HDD,pVosContext);
   if (NULL == pHddCtx)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid pHddCtx", __func__);
      return;
   }
#if 0
   pWextState = pAdapter->pWextState;
#endif

   if (WNI_CFG_NEED_RESTART == result || WNI_CFG_NEED_RELOAD == result)
   {
      //TODO Verify is this is really used. If yes need to fix it.
      hdd_reconnect_all_adapters( pHddCtx );
#if 0
      pAdapter->conn_info.connState = eConnectionState_NotConnected;
      INIT_COMPLETION(pAdapter->disconnect_comp_var);
      vosStatus = sme_RoamDisconnect(halHandle, pAdapter->sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED);

      if(VOS_STATUS_SUCCESS == vosStatus)
          wait_for_completion_interruptible_timeout(&pAdapter->disconnect_comp_var,
                     msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));

      sme_RoamConnect(halHandle,
                     pAdapter->sessionId, &(pWextState->roamProfile),
                     &roamId);
#endif
   }

   EXIT();

}

void hdd_clearRoamProfileIe( hdd_adapter_t *pAdapter)
{
   int i = 0;
   hdd_wext_state_t *pWextState= WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

   /* clear WPA/RSN/WSC IE information in the profile */
   pWextState->roamProfile.nWPAReqIELength = 0;
   pWextState->roamProfile.pWPAReqIE = (tANI_U8 *)NULL;
   pWextState->roamProfile.nRSNReqIELength = 0;
   pWextState->roamProfile.pRSNReqIE = (tANI_U8 *)NULL;

#ifdef FEATURE_WLAN_WAPI
   pWextState->roamProfile.nWAPIReqIELength = 0;
   pWextState->roamProfile.pWAPIReqIE = (tANI_U8 *)NULL;
#endif

   pWextState->roamProfile.bWPSAssociation = VOS_FALSE;
   pWextState->roamProfile.bOSENAssociation = VOS_FALSE;
   pWextState->roamProfile.pAddIEScan = (tANI_U8 *)NULL;
   pWextState->roamProfile.nAddIEScanLength = 0;
   pWextState->roamProfile.pAddIEAssoc = (tANI_U8 *)NULL;
   pWextState->roamProfile.nAddIEAssocLength = 0;

   pWextState->roamProfile.EncryptionType.numEntries = 1;
   pWextState->roamProfile.EncryptionType.encryptionType[0]
                                                     = eCSR_ENCRYPT_TYPE_NONE;

   pWextState->roamProfile.mcEncryptionType.numEntries = 1;
   pWextState->roamProfile.mcEncryptionType.encryptionType[0]
                                                     = eCSR_ENCRYPT_TYPE_NONE;

   pWextState->roamProfile.AuthType.numEntries = 1;
   pWextState->roamProfile.AuthType.authType[0] = eCSR_AUTH_TYPE_OPEN_SYSTEM;

#ifdef WLAN_FEATURE_11W
   pWextState->roamProfile.MFPEnabled = eANI_BOOLEAN_FALSE;
   pWextState->roamProfile.MFPRequired = 0;
   pWextState->roamProfile.MFPCapable = 0;
#endif

   pWextState->authKeyMgmt = 0;

   for (i=0; i < CSR_MAX_NUM_KEY; i++)
   {
      if (pWextState->roamProfile.Keys.KeyMaterial[i])
      {
         pWextState->roamProfile.Keys.KeyLength[i] = 0;
      }
   }
#ifdef FEATURE_WLAN_WAPI
   pAdapter->wapi_info.wapiAuthMode = WAPI_AUTH_MODE_OPEN;
   pAdapter->wapi_info.nWapiMode = 0;
#endif

   vos_mem_zero((void *)(pWextState->req_bssId), VOS_MAC_ADDR_SIZE);

}

void wlan_hdd_ula_done_cb(v_VOID_t *callbackContext)
{
    hdd_adapter_t *pAdapter = (hdd_adapter_t*)callbackContext;

    if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Invalid pAdapter magic", __func__);
    }
    else
    {
        complete(&pAdapter->ula_complete);
    }
}

VOS_STATUS wlan_hdd_check_ula_done(hdd_adapter_t *pAdapter)
{
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    VOS_STATUS vos_status;
    unsigned long rc;

    if (VOS_FALSE == pHddStaCtx->conn_info.uIsAuthenticated)
    {
        INIT_COMPLETION(pAdapter->ula_complete);

        /*To avoid race condition between the set key and the last EAPOL
          packet, notify TL to finish upper layer authentication incase if the
          last EAPOL packet pending in the TL queue.*/
        vos_status = WLANTL_Finish_ULA(wlan_hdd_ula_done_cb, pAdapter);

        if ( vos_status != VOS_STATUS_SUCCESS )
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "[%4d] WLANTL_Finish_ULA returned ERROR status= %d",
                   __LINE__, vos_status );
            return vos_status;

        }

        rc = wait_for_completion_timeout(&pAdapter->ula_complete,
                                    msecs_to_jiffies(HDD_FINISH_ULA_TIME_OUT));
        if (rc <= 0)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      FL("failure wait on ULA to complete %ld"), rc);
            /* we'll still fall through and return success since the
             * connection may still get established but is just taking
             * too long for us to wait */
        }
    }
    return VOS_STATUS_SUCCESS;
}

v_U8_t* wlan_hdd_get_vendor_oui_ie_ptr(v_U8_t *oui, v_U8_t oui_size, v_U8_t *ie, int ie_len)
{

    int left = ie_len;
    v_U8_t *ptr = ie;
    v_U8_t elem_id,elem_len;
    v_U8_t eid = 0xDD;

    if ( NULL == ie || 0 == ie_len )
       return NULL;

    while(left >= 2)
    {
        elem_id  = ptr[0];
        elem_len = ptr[1];
        left -= 2;
        if(elem_len > left)
        {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                   FL("****Invalid IEs eid = %d elem_len=%d left=%d*****"),
                    eid,elem_len,left);
            return NULL;
        }
        if (elem_id == eid)
        {
            if(memcmp( &ptr[2], oui, oui_size)==0)
                return ptr;
        }

        left -= elem_len;
        ptr += (elem_len + 2);
    }
    return NULL;
}

/**
 * hdd_get_ldpc() - Get adapter LDPC
 * @adapter: adapter being queried
 * @value: where to store the value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_get_ldpc(hdd_adapter_t *adapter, int *value)
{
	tHalHandle hal = WLAN_HDD_GET_HAL_CTX(adapter);
	int ret;

	ENTER();
	ret = sme_GetHTConfig(hal, adapter->sessionId,
			      WNI_CFG_HT_CAP_INFO_ADVANCE_CODING);
	if (ret < 0) {
		hddLog(LOGE, FL("Failed to get LDPC value"));
	} else {
		*value = ret;
		ret = 0;
	}
	return ret;
}

/**
 * hdd_set_ldpc() - Set adapter LDPC
 * @adapter: adapter being modified
 * @value: new LDPC value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_ldpc(hdd_adapter_t *adapter, int value)
{
	tHalHandle hal = WLAN_HDD_GET_HAL_CTX(adapter);
	int ret;

	hddLog(LOG1, FL("%d"), value);
	if (value) {
		/* make sure HT capabilities allow this */
		eHalStatus status;
		uint32_t cfg_value;
		union {
			uint16_t cfg_value16;
			tSirMacHTCapabilityInfo ht_cap_info;
		} u;

		status = ccmCfgGetInt(hal, WNI_CFG_HT_CAP_INFO, &cfg_value);
		if (eHAL_STATUS_SUCCESS != status) {
			hddLog(LOGE, FL("Failed to get HT capability info"));
			return -EIO;
		}
		u.cfg_value16 = cfg_value & 0xFFFF;
		if (!u.ht_cap_info.advCodingCap) {
			hddLog(LOGE, FL("LDCP not supported"));
			return -EINVAL;
		}
	}

	ret = sme_UpdateHTConfig(hal, adapter->sessionId,
				 WNI_CFG_HT_CAP_INFO_ADVANCE_CODING,
				 value);
	if (ret)
		hddLog(LOGE, FL("Failed to set LDPC value"));

	return ret;
}

/**
 * hdd_get_tx_stbc() - Get adapter TX STBC
 * @adapter: adapter being queried
 * @value: where to store the value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_get_tx_stbc(hdd_adapter_t *adapter, int *value)
{
	tHalHandle hal = WLAN_HDD_GET_HAL_CTX(adapter);
	int ret;

	ENTER();
	ret = sme_GetHTConfig(hal, adapter->sessionId,
			      WNI_CFG_HT_CAP_INFO_TX_STBC);
	if (ret < 0) {
		hddLog(LOGE, FL("Failed to get TX STBC value"));
	} else {
		*value = ret;
		ret = 0;
	}

	return ret;
}

/**
 * hdd_set_tx_stbc() - Set adapter TX STBC
 * @adapter: adapter being modified
 * @value: new TX STBC value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_tx_stbc(hdd_adapter_t *adapter, int value)
{
	tHalHandle hal = WLAN_HDD_GET_HAL_CTX(adapter);
	int ret;

	hddLog(LOG1, FL("%d"), value);
	if (value) {
		/* make sure HT capabilities allow this */
		eHalStatus status;
		uint32_t cfg_value;
		union {
			uint16_t cfg_value16;
			tSirMacHTCapabilityInfo ht_cap_info;
		} u;

		status = ccmCfgGetInt(hal, WNI_CFG_HT_CAP_INFO, &cfg_value);
		if (eHAL_STATUS_SUCCESS != status) {
			hddLog(LOGE, FL("Failed to get HT capability info"));
			return -EIO;
		}
		u.cfg_value16 = cfg_value & 0xFFFF;
		if (!u.ht_cap_info.txSTBC) {
			hddLog(LOGE, FL("TX STBC not supported"));
			return -EINVAL;
		}
	}
	ret = sme_UpdateHTConfig(hal, adapter->sessionId,
				 WNI_CFG_HT_CAP_INFO_TX_STBC,
				 value);
	if (ret)
		hddLog(LOGE, FL("Failed to set TX STBC value"));

	return ret;
}

/**
 * hdd_get_rx_stbc() - Get adapter RX STBC
 * @adapter: adapter being queried
 * @value: where to store the value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_get_rx_stbc(hdd_adapter_t *adapter, int *value)
{
	tHalHandle hal = WLAN_HDD_GET_HAL_CTX(adapter);
	int ret;

	ENTER();
	ret = sme_GetHTConfig(hal, adapter->sessionId,
			      WNI_CFG_HT_CAP_INFO_RX_STBC);
	if (ret < 0) {
		hddLog(LOGE, FL("Failed to get RX STBC value"));
	} else {
		*value = ret;
		ret = 0;
	}

	return ret;
}

/**
 * hdd_set_rx_stbc() - Set adapter RX STBC
 * @adapter: adapter being modified
 * @value: new RX STBC value
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_rx_stbc(hdd_adapter_t *adapter, int value)
{
	tHalHandle hal = WLAN_HDD_GET_HAL_CTX(adapter);
	int ret;

	hddLog(LOG1, FL("%d"), value);
	if (value) {
		/* make sure HT capabilities allow this */
		eHalStatus status;
		uint32_t cfg_value;
		union {
			uint16_t cfg_value16;
			tSirMacHTCapabilityInfo ht_cap_info;
		} u;

		status = ccmCfgGetInt(hal, WNI_CFG_HT_CAP_INFO, &cfg_value);
		if (eHAL_STATUS_SUCCESS != status) {
			hddLog(LOGE, FL("Failed to get HT capability info"));
			return -EIO;
		}
		u.cfg_value16 = cfg_value & 0xFFFF;
		if (!u.ht_cap_info.rxSTBC) {
			hddLog(LOGE, FL("RX STBC not supported"));
			return -EINVAL;
		}
	}
	ret = sme_UpdateHTConfig(hal, adapter->sessionId,
				 WNI_CFG_HT_CAP_INFO_RX_STBC,
				 value);
	if (ret)
		hddLog(LOGE, FL("Failed to set RX STBC value"));

	return ret;
}

static int iw_set_commit(struct net_device *dev, struct iw_request_info *info,
                         union iwreq_data *wrqu, char *extra)
{
    hddLog( LOG1, "In %s", __func__);
    /* Do nothing for now */
    return 0;
}

static int iw_get_name(struct net_device *dev,
                       struct iw_request_info *info,
                       char *wrqu, char *extra)
{

    ENTER();
    strlcpy(wrqu, "Qcom:802.11n", IFNAMSIZ);
    EXIT();
    return 0;
}

/**
 * __iw_set_mode() - SIOCSIWMODE ioctl handler
 * @dev: device upon which the ioctl was received
 * @info: ioctl request information
 * @wrqu: ioctl request data
 * @extra: ioctl extra data
 *
 * Return: 0 on success, non-zero on error
 */
static int __iw_set_mode(struct net_device *dev, struct iw_request_info *info,
			 union iwreq_data *wrqu, char *extra)
{
    hdd_wext_state_t         *pWextState;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tCsrRoamProfile          *pRoamProfile;
    eCsrRoamBssType          LastBSSType;
    eMib_dot11DesiredBssType connectedBssType;
    hdd_config_t             *pConfig;
    struct wireless_dev      *wdev;

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                 "%s:LOGP in Progress. Ignore!!!", __func__);
       return 0;
    }

    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    wdev = dev->ieee80211_ptr;
    pRoamProfile = &pWextState->roamProfile;
    LastBSSType = pRoamProfile->BSSType;

    hddLog(LOG1, "%s Old Bss type = %d", __func__, LastBSSType);

    switch (wrqu->mode)
    {
    case IW_MODE_ADHOC:
        hddLog(LOG1, "%s Setting AP Mode as IW_MODE_ADHOC", __func__);
        pRoamProfile->BSSType = eCSR_BSS_TYPE_START_IBSS;
        // Set the phymode correctly for IBSS.
        pConfig  = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini;
        pWextState->roamProfile.phyMode = hdd_cfg_xlate_to_csr_phy_mode(pConfig->dot11Mode);
        pAdapter->device_mode = WLAN_HDD_IBSS;
        wdev->iftype = NL80211_IFTYPE_ADHOC;
        break;
    case IW_MODE_INFRA:
        hddLog(LOG1, "%s Setting AP Mode as IW_MODE_INFRA", __func__);
        pRoamProfile->BSSType = eCSR_BSS_TYPE_INFRASTRUCTURE;
        wdev->iftype = NL80211_IFTYPE_STATION;
        break;
    case IW_MODE_AUTO:
        hddLog(LOG1, "%s Setting AP Mode as IW_MODE_AUTO", __func__);
        pRoamProfile->BSSType = eCSR_BSS_TYPE_ANY;
        break;
    default:
        hddLog(LOGE, "%s Unknown AP Mode value %d ", __func__, wrqu->mode);
        return -EOPNOTSUPP;
    }

    if ( LastBSSType != pRoamProfile->BSSType )
    {
        //the BSS mode changed
        // We need to issue disconnect if connected or in IBSS disconnect state
        if ( hdd_connGetConnectedBssType( WLAN_HDD_GET_STATION_CTX_PTR(pAdapter), &connectedBssType ) ||
             ( eCSR_BSS_TYPE_START_IBSS == LastBSSType ) )
        {
            VOS_STATUS vosStatus;
            // need to issue a disconnect to CSR.
            INIT_COMPLETION(pAdapter->disconnect_comp_var);
            vosStatus = sme_RoamDisconnect( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                          pAdapter->sessionId,
                                          eCSR_DISCONNECT_REASON_IBSS_LEAVE );
            if(VOS_STATUS_SUCCESS == vosStatus)
            {
                 unsigned long rc;
                 rc = wait_for_completion_timeout(
                                  &pAdapter->disconnect_comp_var,
                                    msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
                 if (!rc)
                     hddLog(VOS_TRACE_LEVEL_ERROR,
                            FL("failed wait on disconnect_comp_var"));
            }
        }
    }

    EXIT();
    return 0;
}

/**
 * iw_set_mode() - SSR wrapper for __iw_set_mode()
 * @dev: pointer to net_device
 * @info: pointer to iw_request_info
 * @wrqu: pointer to iwreq_data
 * @extra: pointer to extra ioctl payload
 *
 * Return: 0 on success, error number otherwise
 */
static int iw_set_mode(struct net_device *dev, struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __iw_set_mode(dev, info, wrqu, extra);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * __iw_get_mode() - SIOCGIWMODE ioctl handler
 * @dev: device upon which the ioctl was received
 * @info: ioctl request information
 * @wrqu: ioctl request data
 * @extra: ioctl extra data
 *
 * Return: 0 on success, non-zero on error
 */
static int
__iw_get_mode(struct net_device *dev, struct iw_request_info *info,
	      union iwreq_data *wrqu, char *extra)
{
    hdd_wext_state_t *pWextState;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                 "%s:LOGP in Progress. Ignore!!!", __func__);
       return 0;
    }

    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

    switch (pWextState->roamProfile.BSSType) {
    case eCSR_BSS_TYPE_INFRASTRUCTURE:
        hddLog(LOG1, FL("returns IW_MODE_INFRA"));
        wrqu->mode = IW_MODE_INFRA;
        break;
    case eCSR_BSS_TYPE_IBSS:
    case eCSR_BSS_TYPE_START_IBSS:
        hddLog(LOG1, FL("returns IW_MODE_ADHOC"));
        wrqu->mode = IW_MODE_ADHOC;
        break;
    case eCSR_BSS_TYPE_ANY:
    default:
        hddLog(LOG1, FL("returns IW_MODE_AUTO"));
        wrqu->mode = IW_MODE_AUTO;
        break;
    }

    EXIT();
    return 0;
}

/**
 * iw_get_mode() - SSR wrapper for __iw_get_mode()
 * @dev: pointer to net_device
 * @info: pointer to iw_request_info
 * @wrqu: pointer to iwreq_data
 * @extra: pointer to extra ioctl payload
 *
 * Return: 0 on success, error number otherwise
 */
static int iw_get_mode(struct net_device *dev, struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __iw_get_mode(dev, info, wrqu, extra);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * __iw_set_freq() - SIOCSIWFREQ ioctl handler
 * @dev: device upon which the ioctl was received
 * @info: ioctl request information
 * @wrqu: ioctl request data
 * @extra: ioctl extra data
 *
 * Return: 0 on success, non-zero on error
 */
static int __iw_set_freq(struct net_device *dev, struct iw_request_info *info,
			 union iwreq_data *wrqu, char *extra)
{
    v_U32_t numChans = 0;
    v_U8_t validChan[WNI_CFG_VALID_CHANNEL_LIST_LEN];
    v_U32_t indx = 0;
    v_U32_t status = 0;

    hdd_wext_state_t *pWextState;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    tCsrRoamProfile * pRoamProfile;
    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:LOGP in Progress. Ignore!!!",__func__);
       return status;
    }

    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

    pRoamProfile = &pWextState->roamProfile;

    hddLog(LOG1,"setCHANNEL ioctl");

    /* Link is up then return cant set channel*/
    if(eConnectionState_IbssConnected == pHddStaCtx->conn_info.connState ||
       eConnectionState_Associated == pHddStaCtx->conn_info.connState)
    {
        hddLog( LOGE, "IBSS Associated");
        return -EOPNOTSUPP;
    }

    /* Settings by Frequency as input */
    if((wrqu->freq.e == 1) && (wrqu->freq.m >= (tANI_U32)2.412e8) &&
                            (wrqu->freq.m <= (tANI_U32)5.825e8))
    {
        tANI_U32 freq = wrqu->freq.m / 100000;

        while ((indx <  FREQ_CHAN_MAP_TABLE_SIZE) && (freq != freq_chan_map[indx].freq))
            indx++;
        if (indx >= FREQ_CHAN_MAP_TABLE_SIZE)
        {
            return -EINVAL;
        }
        wrqu->freq.e = 0;
        wrqu->freq.m = freq_chan_map[indx].chan;

    }

    if (wrqu->freq.e == 0)
    {
        if((wrqu->freq.m < WNI_CFG_CURRENT_CHANNEL_STAMIN) ||
                        (wrqu->freq.m > WNI_CFG_CURRENT_CHANNEL_STAMAX))
        {
            hddLog(LOG1,"%s: Channel [%d] is outside valid range from %d to %d",
                __func__, wrqu->freq.m, WNI_CFG_CURRENT_CHANNEL_STAMIN,
                    WNI_CFG_CURRENT_CHANNEL_STAMAX);
             return -EINVAL;
        }

        numChans = WNI_CFG_VALID_CHANNEL_LIST_LEN;

        if (ccmCfgGetStr(hHal, WNI_CFG_VALID_CHANNEL_LIST,
                validChan, &numChans) != eHAL_STATUS_SUCCESS){
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
               FL("failed to get ini parameter, WNI_CFG_VALID_CHANNEL_LIST"));
            return -EIO;
        }

        for (indx = 0; indx < numChans; indx++) {
            if (wrqu->freq.m == validChan[indx]){
                break;
            }
        }
    }
    else{

        return -EINVAL;
    }

    if(indx >= numChans)
    {
        return -EINVAL;
    }

    /* Set the Operational Channel */
    numChans = pRoamProfile->ChannelInfo.numOfChannels = 1;
    pHddStaCtx->conn_info.operationChannel = wrqu->freq.m;
    pRoamProfile->ChannelInfo.ChannelList = &pHddStaCtx->conn_info.operationChannel;

    hddLog(LOG1,"pRoamProfile->operationChannel  = %d", wrqu->freq.m);

    EXIT();

    return status;
}

/**
 * iw_set_freq() - SSR wrapper for __iw_set_freq()
 * @dev: pointer to net_device
 * @info: pointer to iw_request_info
 * @wrqu: pointer to iwreq_data
 * @extra: pointer to extra ioctl payload
 *
 * Return: 0 on success, error number otherwise
 */
static int iw_set_freq(struct net_device *dev, struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __iw_set_freq(dev, info, wrqu, extra);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * __iw_get_freq() - SIOCGIWFREQ ioctl handler
 * @dev: device upon which the ioctl was received
 * @info: ioctl request information
 * @fwrq: ioctl frequency data
 * @extra: ioctl extra data
 *
 * Return: 0 on success, non-zero on error
 */
static int __iw_get_freq(struct net_device *dev, struct iw_request_info *info,
			 struct iw_freq *fwrq, char *extra)
{
   v_U32_t status = FALSE, channel = 0, freq = 0;
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   tHalHandle hHal;
   hdd_wext_state_t *pWextState;
   tCsrRoamProfile * pRoamProfile;
   hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   ENTER();

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:LOGP in Progress. Ignore!!!",__func__);
      return status;
   }

   pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
   hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);

   pRoamProfile = &pWextState->roamProfile;

   if( pHddStaCtx->conn_info.connState== eConnectionState_Associated )
   {
       if (sme_GetOperationChannel(hHal, &channel, pAdapter->sessionId) != eHAL_STATUS_SUCCESS)
       {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       FL("failed to get operating channel %u"), pAdapter->sessionId);
           return -EIO;
       }
       else
       {
           status = hdd_wlan_get_freq(channel, &freq);
           if( TRUE == status )
           {
               /* Set Exponent parameter as 6 (MHZ) in struct iw_freq
                * iwlist & iwconfig command shows frequency into proper
                * format (2.412 GHz instead of 246.2 MHz)*/
               fwrq->m = freq;
               fwrq->e = MHZ;
           }
       }
    }
    else
    {
       /* Set Exponent parameter as 6 (MHZ) in struct iw_freq
        * iwlist & iwconfig command shows frequency into proper
        * format (2.412 GHz instead of 246.2 MHz)*/
       fwrq->m = 0;
       fwrq->e = MHZ;
    }
   return 0;
}

/**
 * iw_get_freq() - SSR wrapper for __iw_get_freq()
 * @dev: pointer to net_device
 * @info: pointer to iw_request_info
 * @fwrq: pointer to frequency data
 * @extra: pointer to extra ioctl payload
 *
 * Return: 0 on success, error number otherwise
 */
static int iw_get_freq(struct net_device *dev, struct iw_request_info *info,
		       struct iw_freq *fwrq, char *extra)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __iw_get_freq(dev, info, fwrq, extra);
	vos_ssr_unprotect(__func__);

	return ret;
}

static int iw_get_tx_power(struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu, char *extra)
{

   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   if (pHddCtx->isLogpInProgress)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
              "%s:LOGP in Progress. Ignore!!!",__func__);
      return -EBUSY;
   }

   if(eConnectionState_Associated != pHddStaCtx->conn_info.connState)
   {
      wrqu->txpower.value = 0;
      return 0;
   }
   wlan_hdd_get_classAstats(pAdapter);
   wrqu->txpower.value = pAdapter->hdd_stats.ClassA_stat.max_pwr;

   return 0;
}

static int iw_set_tx_power(struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:LOGP in Progress. Ignore!!!",__func__);
       return 0;
    }

    ENTER();

    if ( ccmCfgSetInt(hHal, WNI_CFG_CURRENT_TX_POWER_LEVEL, wrqu->txpower.value, ccmCfgSetCallback, eANI_BOOLEAN_TRUE) != eHAL_STATUS_SUCCESS )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("failed to set ini parameter, WNI_CFG_CURRENT_TX_POWER_LEVEL"));
        return -EIO;
    }

    EXIT();

    return 0;
}

static int iw_get_bitrate(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
   VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
   eHalStatus status = eHAL_STATUS_SUCCESS;
   hdd_wext_state_t *pWextState;
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   ENTER();

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:LOGP in Progress. Ignore!!!",__func__);
      return status;
   }

   if(eConnectionState_Associated != pHddStaCtx->conn_info.connState) {
        wrqu->bitrate.value = 0;
   }
   else {
      status = sme_GetStatistics( WLAN_HDD_GET_HAL_CTX(pAdapter), eCSR_HDD,
                               SME_SUMMARY_STATS       |
                               SME_GLOBAL_CLASSA_STATS |
                               SME_GLOBAL_CLASSB_STATS |
                               SME_GLOBAL_CLASSC_STATS |
                               SME_GLOBAL_CLASSD_STATS |
                               SME_PER_STA_STATS,
                               hdd_StatisticsCB, 0, FALSE,
                               pHddStaCtx->conn_info.staId[0], pAdapter,
                               pAdapter->sessionId );

      if(eHAL_STATUS_SUCCESS != status)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Unable to retrieve statistics",
                __func__);
         return status;
      }

      pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

      vos_status = vos_wait_single_event(&pWextState->vosevent, WLAN_WAIT_TIME_STATS);

      if (!VOS_IS_STATUS_SUCCESS(vos_status))
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: SME timeout while retrieving statistics",
                __func__);
         return VOS_STATUS_E_FAILURE;
      }

      wrqu->bitrate.value = pAdapter->hdd_stats.ClassA_stat.tx_rate*500*1000;
   }

   EXIT();

   return vos_status;
}
/* ccm call back function */

static int iw_set_bitrate(struct net_device *dev,
                          struct iw_request_info *info,
                          union iwreq_data *wrqu,
                          char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_wext_state_t *pWextState;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    v_U8_t supp_rates[WNI_CFG_SUPPORTED_RATES_11A_LEN];
    v_U32_t a_len = WNI_CFG_SUPPORTED_RATES_11A_LEN;
    v_U32_t b_len = WNI_CFG_SUPPORTED_RATES_11B_LEN;
    v_U32_t i, rate;
    v_U32_t valid_rate = FALSE, active_phy_mode = 0;

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:LOGP in Progress. Ignore!!!",__func__);
       return 0;
    }

    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

    if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
    {
        return -ENXIO ;
    }

    rate = wrqu->bitrate.value;

    if (rate == -1)
    {
        rate = WNI_CFG_FIXED_RATE_AUTO;
        valid_rate = TRUE;
    }
    else if (ccmCfgGetInt(WLAN_HDD_GET_HAL_CTX(pAdapter),
                        WNI_CFG_DOT11_MODE, &active_phy_mode) == eHAL_STATUS_SUCCESS)
    {
        if (active_phy_mode == WNI_CFG_DOT11_MODE_11A || active_phy_mode == WNI_CFG_DOT11_MODE_11G
            || active_phy_mode == WNI_CFG_DOT11_MODE_11B)
        {
            if ((ccmCfgGetStr(WLAN_HDD_GET_HAL_CTX(pAdapter),
                        WNI_CFG_SUPPORTED_RATES_11A,
                        supp_rates, &a_len) == eHAL_STATUS_SUCCESS) &&
                (ccmCfgGetStr(WLAN_HDD_GET_HAL_CTX(pAdapter),
                        WNI_CFG_SUPPORTED_RATES_11B,
                        supp_rates, &b_len) == eHAL_STATUS_SUCCESS))
            {
                for (i = 0; i < (b_len + a_len); ++i)
                {
                    /* supported rates returned is double the actual rate so we divide it by 2 */
                    if ((supp_rates[i]&0x7F)/2 == rate)
                    {
                        valid_rate = TRUE;
                        rate = i + WNI_CFG_FIXED_RATE_1MBPS;
                        break;
                    }
                }
            }
        }
    }
    if (valid_rate != TRUE)
    {
        return -EINVAL;
    }
    if (ccmCfgSetInt(WLAN_HDD_GET_HAL_CTX(pAdapter),
                     WNI_CFG_FIXED_RATE, rate,
                     ccmCfgSetCallback,eANI_BOOLEAN_FALSE) != eHAL_STATUS_SUCCESS)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("failed to set ini parameter, WNI_CFG_FIXED_RATE"));
        return -EIO;
    }
    return 0;
}


static int iw_set_genie(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    u_int8_t *genie = NULL;
    u_int8_t *base_genie = NULL;
    v_U16_t remLen;
    int ret = 0;

   ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return 0;
    }

    if (!wrqu->data.length) {
        hdd_clearRoamProfileIe(pAdapter);
        EXIT();
        return 0;
    }

    base_genie = mem_alloc_copy_from_user_helper(wrqu->data.pointer,
                                                 wrqu->data.length);
    if (NULL == base_genie)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "mem_alloc_copy_from_user_helper fail");
        return -ENOMEM;
    }

    genie = base_genie;

    remLen = wrqu->data.length;

    hddLog(LOG1,"iw_set_genie ioctl IE[0x%X], LEN[%d]", genie[0], genie[1]);

    /* clear any previous genIE before this call */
    memset( &pWextState->genIE, 0, sizeof(pWextState->genIE) );

    while (remLen >= 2)
   {
        v_U16_t eLen = 0;
        v_U8_t elementId;
        elementId = *genie++;
        eLen  = *genie++;
        remLen -= 2;

        hddLog(VOS_TRACE_LEVEL_INFO, "%s: IE[0x%X], LEN[%d]",
            __func__, elementId, eLen);

        switch ( elementId )
         {
            case IE_EID_VENDOR:
                if ((IE_LEN_SIZE+IE_EID_SIZE+IE_VENDOR_OUI_SIZE) > eLen) /* should have at least OUI */
                {
                    ret = -EINVAL;
                    goto exit;
                }

                if (0 == memcmp(&genie[0], "\x00\x50\xf2\x04", 4))
                {
                    v_U16_t curGenIELen = pWextState->genIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set WPS OUI(%02x %02x %02x %02x) IE(len %d)",
                            __func__, genie[0], genie[1], genie[2], genie[3], eLen + 2);

                    if( SIR_MAC_MAX_IE_LENGTH < (pWextState->genIE.length + eLen) )
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate genIE. "
                                                      "Need bigger buffer space");
                       VOS_ASSERT(0);
                       ret = -EINVAL;
                       goto exit;
                    }
                    // save to Additional IE ; it should be accumulated to handle WPS IE + other IE
                    memcpy( pWextState->genIE.addIEdata + curGenIELen, genie - 2, eLen + 2);
                    pWextState->genIE.length += eLen + 2;
                }
                else if (0 == memcmp(&genie[0], "\x00\x50\xf2", 3))
                {
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set WPA IE (len %d)",__func__, eLen + 2);
                    if ((eLen + 2) > (sizeof(pWextState->WPARSNIE)))
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate genIE. "
                                                      "Need bigger buffer space");
                       ret = -EINVAL;
                       VOS_ASSERT(0);
                       goto exit;
                    }
                    memset( pWextState->WPARSNIE, 0, MAX_WPA_RSN_IE_LEN );
                    memcpy( pWextState->WPARSNIE, genie - 2, (eLen + 2));
                    pWextState->roamProfile.pWPAReqIE = pWextState->WPARSNIE;
                    pWextState->roamProfile.nWPAReqIELength = eLen + 2;
                }
                else /* any vendorId except WPA IE should be accumulated to genIE */
                {
                    v_U16_t curGenIELen = pWextState->genIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set OUI(%02x %02x %02x %02x) IE(len %d)",
                            __func__, genie[0], genie[1], genie[2], genie[3], eLen + 2);

                    if( SIR_MAC_MAX_IE_LENGTH < (pWextState->genIE.length + eLen) )
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate genIE. "
                                                      "Need bigger buffer space");
                       VOS_ASSERT(0);
                       ret = -ENOMEM;
                       goto exit;
                    }
                    // save to Additional IE ; it should be accumulated to handle WPS IE + other IE
                    memcpy( pWextState->genIE.addIEdata + curGenIELen, genie - 2, eLen + 2);
                    pWextState->genIE.length += eLen + 2;
                }
              break;
         case DOT11F_EID_RSN:
                hddLog (LOG1, "%s Set RSN IE (len %d)",__func__, eLen+2);
                if ((eLen + 2) > (sizeof(pWextState->WPARSNIE)))
                {
                    hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate genIE. "
                                                  "Need bigger buffer space");
                    ret = -EINVAL;
                    VOS_ASSERT(0);
                    goto exit;
                }
                memset( pWextState->WPARSNIE, 0, MAX_WPA_RSN_IE_LEN );
                memcpy( pWextState->WPARSNIE, genie - 2, (eLen + 2));
                pWextState->roamProfile.pRSNReqIE = pWextState->WPARSNIE;
                pWextState->roamProfile.nRSNReqIELength = eLen + 2;
              break;

         default:
                hddLog (LOGE, "%s Set UNKNOWN IE %X",__func__, elementId);
                goto exit;
    }
        genie += eLen;
        remLen -= eLen;
    }
exit:
    EXIT();
    kfree(base_genie);
    return ret;
}

static int iw_get_genie(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu,
                        char *extra)
{
    hdd_wext_state_t *pWextState;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    eHalStatus status;
    v_U32_t length = DOT11F_IE_RSN_MAX_LEN;
    v_U8_t genIeBytes[DOT11F_IE_RSN_MAX_LEN];

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:LOGP in Progress. Ignore!!!",__func__);
       return 0;
    }


    hddLog(LOG1,"getGEN_IE ioctl");

    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

    if( pHddStaCtx->conn_info.connState == eConnectionState_NotConnected)
    {
        return -ENXIO;
    }

    // Return something ONLY if we are associated with an RSN or WPA network
    if ( VOS_TRUE != hdd_IsAuthTypeRSN(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                                pWextState->roamProfile.negotiatedAuthType))
    {
        return -ENXIO;
    }

    // Actually retrieve the RSN IE from CSR.  (We previously sent it down in the CSR Roam Profile.)
    status = csrRoamGetWpaRsnReqIE(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                   pAdapter->sessionId,
                                   &length,
                                   genIeBytes);
    length = VOS_MIN((u_int16_t) length, DOT11F_IE_RSN_MAX_LEN);
    if (wrqu->data.length < length)
    {
        hddLog(LOG1, "%s: failed to copy data to user buffer", __func__);
        return -EFAULT;
    }
    vos_mem_copy( extra, (v_VOID_t*)genIeBytes, length);
    wrqu->data.length = length;

    hddLog(LOG1,"%s: RSN IE of %d bytes returned", __func__, wrqu->data.length );

    EXIT();

    return 0;
}

static int iw_get_encode(struct net_device *dev,
                       struct iw_request_info *info,
                       struct iw_point *dwrq, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    tCsrRoamProfile *pRoamProfile = &(pWextState->roamProfile);
    int keyId;
    eCsrAuthType authType = eCSR_AUTH_TYPE_NONE;
    int i;

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:LOGP in Progress. Ignore!!!",__func__);
       return 0;
    }

    keyId = pRoamProfile->Keys.defaultIndex;

    if(keyId < 0 || keyId >= MAX_WEP_KEYS)
    {
        hddLog(LOG1,"%s: Invalid keyId : %d", __func__, keyId);
        return -EINVAL;
    }

    if(pRoamProfile->Keys.KeyLength[keyId] > 0)
    {
        dwrq->flags |= IW_ENCODE_ENABLED;
        dwrq->length = pRoamProfile->Keys.KeyLength[keyId];
        vos_mem_copy(extra,&(pRoamProfile->Keys.KeyMaterial[keyId][0]),pRoamProfile->Keys.KeyLength[keyId]);

        dwrq->flags |= (keyId + 1);

    }
    else
    {
        dwrq->flags |= IW_ENCODE_DISABLED;
    }

    for(i=0; i < MAX_WEP_KEYS; i++)
    {
        if(pRoamProfile->Keys.KeyMaterial[i] == NULL)
        {
            continue;
        }
        else
        {
            break;
        }
    }

    if(MAX_WEP_KEYS == i)
    {
        dwrq->flags |= IW_ENCODE_NOKEY;
    }

    authType = ((hdd_station_ctx_t*)WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.authType;

    if(eCSR_AUTH_TYPE_OPEN_SYSTEM == authType)
    {
        dwrq->flags |= IW_ENCODE_OPEN;
    }
    else
    {
        dwrq->flags |= IW_ENCODE_RESTRICTED;
    }
    EXIT();
    return 0;
}

#define PAE_ROLE_AUTHENTICATOR 1 // =1 for authenticator,
#define PAE_ROLE_SUPPLICANT 0 // =0 for supplicant


/*
 * This function sends a single 'key' to LIM at all time.
 */

static int iw_get_rts_threshold(struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   v_U32_t status = 0;

   status = hdd_wlan_get_rts_threshold(pAdapter,wrqu);

   return status;
}

static int iw_set_rts_threshold(struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
            "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EAGAIN;
    }
    if ( wrqu->rts.value < WNI_CFG_RTS_THRESHOLD_STAMIN || wrqu->rts.value > WNI_CFG_RTS_THRESHOLD_STAMAX )
    {
        return -EINVAL;
    }

    if ( ccmCfgSetInt(hHal, WNI_CFG_RTS_THRESHOLD, wrqu->rts.value, ccmCfgSetCallback, eANI_BOOLEAN_TRUE) != eHAL_STATUS_SUCCESS )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("failed to set ini parameter, WNI_CFG_RTS_THRESHOLD"));
        return -EIO;
    }

    EXIT();

    return 0;
}

static int iw_get_frag_threshold(struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    v_U32_t status = 0;

    status = hdd_wlan_get_frag_threshold(pAdapter,wrqu);

    return status;
}

static int iw_set_frag_threshold(struct net_device *dev,
             struct iw_request_info *info,
                 union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);

   ENTER();

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
       return -EBUSY;
    }
    if ( wrqu->frag.value < WNI_CFG_FRAGMENTATION_THRESHOLD_STAMIN || wrqu->frag.value > WNI_CFG_FRAGMENTATION_THRESHOLD_STAMAX )
    {
        return -EINVAL;
    }

    if ( ccmCfgSetInt(hHal, WNI_CFG_FRAGMENTATION_THRESHOLD, wrqu->frag.value, ccmCfgSetCallback, eANI_BOOLEAN_TRUE) != eHAL_STATUS_SUCCESS )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("failed to set ini parameter, WNI_CFG_FRAGMENTATION_THRESHOLD"));
        return -EIO;
    }

   EXIT();

   return 0;
}

static int iw_get_power_mode(struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu, char *extra)
{
   ENTER();
   return -EOPNOTSUPP;
}

static int iw_set_power_mode(struct net_device *dev,
                             struct iw_request_info *info,
                             union iwreq_data *wrqu, char *extra)
{
    ENTER();
    return -EOPNOTSUPP;
}

/**
 * __iw_get_range() - SIOCGIWRANGE ioctl handler
 * @dev: device upon which the ioctl was received
 * @info: ioctl request information
 * @wrqu: ioctl request data
 * @extra: ioctl extra data
 *
 * Return: 0 on success, non-zero on error
 */
static int __iw_get_range(struct net_device *dev, struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
   struct iw_range *range = (struct iw_range *) extra;

   v_U8_t channels[WNI_CFG_VALID_CHANNEL_LIST_LEN];

   v_U32_t num_channels = sizeof(channels);
   v_U8_t supp_rates[WNI_CFG_SUPPORTED_RATES_11A_LEN];
   v_U32_t a_len;
   v_U32_t b_len;
   v_U32_t active_phy_mode = 0;
   v_U8_t index = 0, i;

   ENTER();

   wrqu->data.length = sizeof(struct iw_range);
   memset(range, 0, sizeof(struct iw_range));

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
      return -EBUSY;
   }

   /*Get the phy mode*/
   if (ccmCfgGetInt(hHal,
                  WNI_CFG_DOT11_MODE, &active_phy_mode) == eHAL_STATUS_SUCCESS)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "active_phy_mode = %d", active_phy_mode);

      if (active_phy_mode == WNI_CFG_DOT11_MODE_11A || active_phy_mode == WNI_CFG_DOT11_MODE_11G)
      {
         /*Get the supported rates for 11G band*/
         a_len = WNI_CFG_SUPPORTED_RATES_11A_LEN;
         if (ccmCfgGetStr(hHal,
                          WNI_CFG_SUPPORTED_RATES_11A,
                          supp_rates, &a_len) == eHAL_STATUS_SUCCESS)
         {
            if (a_len > WNI_CFG_SUPPORTED_RATES_11A_LEN)
            {
               a_len = WNI_CFG_SUPPORTED_RATES_11A_LEN;
            }
            for (i = 0; i < a_len; i++)
            {
               range->bitrate[i] = ((supp_rates[i] & 0x7F) / 2) * 1000000;
            }
            range->num_bitrates = a_len;
         }
         else
         {
            return -EIO;
         }
      }
      else if (active_phy_mode == WNI_CFG_DOT11_MODE_11B)
      {
         /*Get the supported rates for 11B band*/
         b_len = WNI_CFG_SUPPORTED_RATES_11B_LEN;
         if (ccmCfgGetStr(hHal,
                          WNI_CFG_SUPPORTED_RATES_11B,
                          supp_rates, &b_len) == eHAL_STATUS_SUCCESS)
         {
            if (b_len > WNI_CFG_SUPPORTED_RATES_11B_LEN)
            {
               b_len = WNI_CFG_SUPPORTED_RATES_11B_LEN;
            }
            for (i = 0; i < b_len; i++)
            {
               range->bitrate[i] = ((supp_rates[i] & 0x7F) / 2) * 1000000;
            }
            range->num_bitrates = b_len;
         }
         else
         {
            return -EIO;
         }
      }
   }

   range->max_rts = WNI_CFG_RTS_THRESHOLD_STAMAX;
   range->min_frag = WNI_CFG_FRAGMENTATION_THRESHOLD_STAMIN;
   range->max_frag = WNI_CFG_FRAGMENTATION_THRESHOLD_STAMAX;

   range->encoding_size[0] = 5;
   range->encoding_size[1] = 13;
   range->num_encoding_sizes = 2;
   range->max_encoding_tokens = MAX_WEP_KEYS;

   // we support through Wireless Extensions 22
   range->we_version_compiled = WIRELESS_EXT;
   range->we_version_source = 22;

   /*Supported Channels and Frequencies*/
   if (ccmCfgGetStr((hHal), WNI_CFG_VALID_CHANNEL_LIST, channels, &num_channels) != eHAL_STATUS_SUCCESS)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
               FL("failed to get ini parameter, WNI_CFG_VALID_CHANNEL_LIST"));
      return -EIO;
   }
   if (num_channels > IW_MAX_FREQUENCIES)
   {
      num_channels = IW_MAX_FREQUENCIES;
   }

   range->num_channels = num_channels;
   range->num_frequency = num_channels;

   for (index=0; index < num_channels; index++)
   {
      v_U32_t frq_indx = 0;

      range->freq[index].i = channels[index];
      while (frq_indx <  FREQ_CHAN_MAP_TABLE_SIZE)
      {
           if(channels[index] == freq_chan_map[frq_indx].chan)
           {
             range->freq[index].m = freq_chan_map[frq_indx].freq * 100000;
             range->freq[index].e = 1;
             break;
           }
           frq_indx++;
      }
   }

   /* Event capability (kernel + driver) */
   range->event_capa[0] = (IW_EVENT_CAPA_K_0 |
                    IW_EVENT_CAPA_MASK(SIOCGIWAP) |
                    IW_EVENT_CAPA_MASK(SIOCGIWSCAN));
   range->event_capa[1] = IW_EVENT_CAPA_K_1;

   /*Encryption capability*/
   range->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 |
                IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;

   /* Txpower capability */
   range->txpower_capa = IW_TXPOW_MWATT;

   /*Scanning capability*/
   #if WIRELESS_EXT >= 22
   range->scan_capa = IW_SCAN_CAPA_ESSID | IW_SCAN_CAPA_TYPE | IW_SCAN_CAPA_CHANNEL;
   #endif

   EXIT();
   return 0;
}

/**
 * iw_get_range() - SSR wrapper for __iw_get_range()
 * @dev: pointer to net_device
 * @info: pointer to iw_request_info
 * @wrqu: pointer to iwreq_data
 * @extra: pointer to extra ioctl payload
 *
 * Return: 0 on success, error number otherwise
 */
static int iw_get_range(struct net_device *dev, struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __iw_get_range(dev, info, wrqu, extra);
	vos_ssr_unprotect(__func__);

	return ret;
}

/* Callback function registered with PMC to know status of PMC request */
static void iw_power_callback_fn (void *pContext, eHalStatus status)
{
   struct statsContext *pStatsContext;

   if (NULL == pContext)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
            "%s: Bad param, pContext [%p]",
              __func__, pContext);
       return;
   }

   pStatsContext = (struct statsContext *)pContext;

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
   spin_lock(&hdd_context_lock);

   if (POWER_CONTEXT_MAGIC != pStatsContext->magic)
   {
       /* the caller presumably timed out so there is nothing we can do */
       spin_unlock(&hdd_context_lock);
       hddLog(VOS_TRACE_LEVEL_WARN,
              "%s: Invalid context, magic [%08x]",
              __func__, pStatsContext->magic);

       if (ioctl_debug)
       {
           pr_info("%s: Invalid context, magic [%08x]\n",
                   __func__, pStatsContext->magic);
       }
       return;
  }

  /* context is valid so caller is still waiting */

  /* paranoia: invalidate the magic */
  pStatsContext->magic = 0;

  /* notify the caller */
  complete(&pStatsContext->completion);

  /* serialization is complete */
  spin_unlock(&hdd_context_lock);
}

/* Callback function for tx per hit */
void hdd_tx_per_hit_cb (void *pCallbackContext)
{
    hdd_adapter_t *pAdapter = (hdd_adapter_t *)pCallbackContext;
    unsigned char tx_fail[16];
    union iwreq_data wrqu;

    if (NULL == pAdapter || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic))
    {
        hddLog(LOGE, "hdd_tx_per_hit_cb: pAdapter is NULL");
        return;
    }
    memset(&wrqu, 0, sizeof(wrqu));
    wrqu.data.length = strlcpy(tx_fail, "TX_FAIL", sizeof(tx_fail));
    wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, tx_fail);
}

void hdd_GetClassA_statisticsCB(void *pStats, void *pContext)
{
   struct statsContext *pStatsContext;
   tCsrGlobalClassAStatsInfo *pClassAStats;
   hdd_adapter_t *pAdapter;

   if (ioctl_debug)
   {
      pr_info("%s: pStats [%p] pContext [%p]\n",
              __func__, pStats, pContext);
   }

   if ((NULL == pStats) || (NULL == pContext))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, pStats [%p] pContext [%p]",
              __func__, pStats, pContext);
      return;
   }

   pClassAStats  = pStats;
   pStatsContext = pContext;
   pAdapter      = pStatsContext->pAdapter;

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
   spin_lock(&hdd_context_lock);

   if ((NULL == pAdapter) || (STATS_CONTEXT_MAGIC != pStatsContext->magic))
   {
      /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, pAdapter [%p] magic [%08x]",
              __func__, pAdapter, pStatsContext->magic);
      if (ioctl_debug)
      {
         pr_info("%s: Invalid context, pAdapter [%p] magic [%08x]\n",
                 __func__, pAdapter, pStatsContext->magic);
      }
      return;
   }

   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pStatsContext->magic = 0;

   /* copy over the stats. do so as a struct copy */
   pAdapter->hdd_stats.ClassA_stat = *pClassAStats;

   /* notify the caller */
   complete(&pStatsContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}

void hdd_GetLink_SpeedCB(tSirLinkSpeedInfo *pLinkSpeed, void *pContext)
{
   struct linkspeedContext *pLinkSpeedContext;
   hdd_adapter_t *pAdapter;

   if ((NULL == pLinkSpeed) || (NULL == pContext))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, pLinkSpeed [%p] pContext [%p]",
             __func__, pLinkSpeed, pContext);
      return;
   }
   spin_lock(&hdd_context_lock);
   pLinkSpeedContext = pContext;
   pAdapter      = pLinkSpeedContext->pAdapter;

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */

   if ((NULL == pAdapter) || (LINK_CONTEXT_MAGIC != pLinkSpeedContext->magic))
   {
       /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, pAdapter [%p] magic [%08x]",
              __func__, pAdapter, pLinkSpeedContext->magic);
      if (ioctl_debug)
      {
         pr_info("%s: Invalid context, pAdapter [%p] magic [%08x]\n",
                 __func__, pAdapter, pLinkSpeedContext->magic);
      }
      return;
   }
   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pLinkSpeedContext->magic = 0;

   /* copy over the stats. do so as a struct copy */
   pAdapter->ls_stats = *pLinkSpeed;

   /* notify the caller */
   complete(&pLinkSpeedContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}

VOS_STATUS  wlan_hdd_get_classAstats(hdd_adapter_t *pAdapter)
{
   hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   eHalStatus hstatus;
   unsigned long rc;
   struct statsContext context;

   if (NULL == pAdapter)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s: pAdapter is NULL", __func__);
       return VOS_STATUS_E_FAULT;
   }
   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s:LOGP in Progress. Ignore!!!",__func__);
       return VOS_STATUS_SUCCESS;
   }

   /* we are connected
   prepare our callback context */
   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = STATS_CONTEXT_MAGIC;
   /* query only for Class A statistics (which include link speed) */
   hstatus = sme_GetStatistics( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                  eCSR_HDD,
                                  SME_GLOBAL_CLASSA_STATS,
                                  hdd_GetClassA_statisticsCB,
                                  0, // not periodic
                                  FALSE, //non-cached results
                                  pHddStaCtx->conn_info.staId[0],
                                  &context,
                                  pAdapter->sessionId );
   if (eHAL_STATUS_SUCCESS != hstatus)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: Unable to retrieve Class A statistics",
               __func__);
       /* we'll returned a cached value below */
   }
   else
   {
       /* request was sent -- wait for the response */
       rc = wait_for_completion_timeout(&context.completion,
                                msecs_to_jiffies(WLAN_WAIT_TIME_STATS));
       if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
              FL("SME timed out while retrieving Class A statistics"));
      }
   }

   /* either we never sent a request, we sent a request and received a
      response or we sent a request and timed out.  if we never sent a
      request or if we sent a request and got a response, we want to
      clear the magic out of paranoia.  if we timed out there is a
      race condition such that the callback function could be
      executing at the same time we are. of primary concern is if the
      callback function had already verified the "magic" but had not
      yet set the completion variable when a timeout occurred. we
      serialize these activities by invalidating the magic while
      holding a shared spinlock which will cause us to block if the
      callback is currently executing */
   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);

   /* either callback updated pAdapter stats or it has cached data */
   return VOS_STATUS_SUCCESS;
}

static void hdd_get_station_statisticsCB(void *pStats, void *pContext)
{
   struct statsContext *pStatsContext;
   tCsrSummaryStatsInfo      *pSummaryStats;
   tCsrGlobalClassAStatsInfo *pClassAStats;
   hdd_adapter_t *pAdapter;

   if (ioctl_debug)
   {
      pr_info("%s: pStats [%p] pContext [%p]\n",
              __func__, pStats, pContext);
   }

   if ((NULL == pStats) || (NULL == pContext))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Bad param, pStats [%p] pContext [%p]",
             __func__, pStats, pContext);
      return;
   }

   /* there is a race condition that exists between this callback
      function and the caller since the caller could time out either
      before or while this code is executing.  we use a spinlock to
      serialize these actions */
   spin_lock(&hdd_context_lock);

   pSummaryStats = (tCsrSummaryStatsInfo *)pStats;
   pClassAStats  = (tCsrGlobalClassAStatsInfo *)( pSummaryStats + 1 );
   pStatsContext = pContext;
   pAdapter      = pStatsContext->pAdapter;
   if ((NULL == pAdapter) || (STATS_CONTEXT_MAGIC != pStatsContext->magic))
   {
      /* the caller presumably timed out so there is nothing we can do */
      spin_unlock(&hdd_context_lock);
      hddLog(VOS_TRACE_LEVEL_WARN,
             "%s: Invalid context, pAdapter [%p] magic [%08x]",
             __func__, pAdapter, pStatsContext->magic);
      if (ioctl_debug)
      {
         pr_info("%s: Invalid context, pAdapter [%p] magic [%08x]\n",
                 __func__, pAdapter, pStatsContext->magic);
      }
      return;
   }

   /* context is valid so caller is still waiting */

   /* paranoia: invalidate the magic */
   pStatsContext->magic = 0;

   /* copy over the stats. do so as a struct copy */
   pAdapter->hdd_stats.summary_stat = *pSummaryStats;
   pAdapter->hdd_stats.ClassA_stat = *pClassAStats;

   /* notify the caller */
   complete(&pStatsContext->completion);

   /* serialization is complete */
   spin_unlock(&hdd_context_lock);
}

VOS_STATUS  wlan_hdd_get_station_stats(hdd_adapter_t *pAdapter)
{
   hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   eHalStatus hstatus;
   unsigned long rc;
   struct statsContext context;

   if (NULL == pAdapter)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s: pAdapter is NULL", __func__);
       return VOS_STATUS_SUCCESS;
   }

   /* we are connected
   prepare our callback context */
   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = STATS_CONTEXT_MAGIC;

   /* query only for Summary & Class A statistics */
   hstatus = sme_GetStatistics(WLAN_HDD_GET_HAL_CTX(pAdapter),
                               eCSR_HDD,
                               SME_SUMMARY_STATS |
                               SME_GLOBAL_CLASSA_STATS,
                               hdd_get_station_statisticsCB,
                               0, // not periodic
                               FALSE, //non-cached results
                               pHddStaCtx->conn_info.staId[0],
                               &context,
                               pAdapter->sessionId);
   if (eHAL_STATUS_SUCCESS != hstatus)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Unable to retrieve statistics",
             __func__);
      /* we'll return with cached values */
   }
   else
   {
      /* request was sent -- wait for the response */
      rc = wait_for_completion_timeout(&context.completion,
                           msecs_to_jiffies(WLAN_WAIT_TIME_STATS));

      if (!rc) {
          hddLog(VOS_TRACE_LEVEL_ERROR,
              FL("SME timed out while retrieving statistics"));
      }
   }

   /* either we never sent a request, we sent a request and received a
      response or we sent a request and timed out.  if we never sent a
      request or if we sent a request and got a response, we want to
      clear the magic out of paranoia.  if we timed out there is a
      race condition such that the callback function could be
      executing at the same time we are. of primary concern is if the
      callback function had already verified the "magic" but had not
      yet set the completion variable when a timeout occurred. we
      serialize these activities by invalidating the magic while
      holding a shared spinlock which will cause us to block if the
      callback is currently executing */
   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);

   /* either callback updated pAdapter stats or it has cached data */
   return VOS_STATUS_SUCCESS;
}


/*
 * Support for the LINKSPEED private command
 * Per the WiFi framework the response must be of the form
 *         "LinkSpeed xx"
 */
static int iw_get_linkspeed(struct net_device *dev,
                            struct iw_request_info *info,
                            union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *pHddCtx;
   hdd_station_ctx_t *pHddStaCtx;
   char *pLinkSpeed = (char*)extra;
   int len = sizeof(v_U32_t) + 1;
   v_U32_t link_speed = 0;
   VOS_STATUS status;
   int rc, valid;
   tSirMacAddr bssid;

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   valid = wlan_hdd_validate_context(pHddCtx);

   if (0 != valid)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
       return valid;
   }
   vos_mem_copy(bssid, pHddStaCtx->conn_info.bssId, VOS_MAC_ADDR_SIZE);

   if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
   {
      /* we are not connected so we don't have a classAstats */
      link_speed = 0;
   }
   else
   {
       status = wlan_hdd_get_linkspeed_for_peermac(pAdapter, bssid);
       if (!VOS_IS_STATUS_SUCCESS(status ))
       {
          hddLog(VOS_TRACE_LEVEL_ERROR, FL("Unable to retrieve SME linkspeed"));
          return -EINVAL;
       }
       link_speed = pAdapter->ls_stats.estLinkSpeed;
       /* linkspeed in units of 500 kbps */
       link_speed = link_speed / 500;
   }
   wrqu->data.length = len;
   /* return the linkspeed in the format required by the WiFi Framework */
   rc = snprintf(pLinkSpeed, len, "%u", link_speed);
   if ((rc < 0) || (rc >= len))
   {
       /* encoding or length error? */
       hddLog(VOS_TRACE_LEVEL_ERROR,FL("Unable to encode link speed"));
       return -EIO;
   }

  /* a value is being successfully returned */
   return rc;
}

/*
 * Helper function to return correct value for WLAN_GET_LINK_SPEED
 *
 */
static int iw_get_linkspeed_priv(struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu, char *extra)
{
    int rc;

    rc = iw_get_linkspeed(dev, info, wrqu, extra);

    if (rc < 0)
       return rc;

    /* a value is being successfully returned */
    return 0;
}

/*
 * Support for the RSSI & RSSI-APPROX private commands
 * Per the WiFi framework the response must be of the form
 *         "<ssid> rssi <xx>"
 * unless we are not associated, in which case the response is
 *         "OK"
 */
static int iw_get_rssi(struct net_device *dev,
                       struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   char *cmd = extra;
   int len = wrqu->data.length;
   v_S7_t s7Rssi = 0;
   hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   int ssidlen = pHddStaCtx->conn_info.SSID.SSID.length;
   VOS_STATUS vosStatus;
   int rc;

   if ((eConnectionState_Associated != pHddStaCtx->conn_info.connState) ||
       (0 == ssidlen) || (ssidlen >= len))
   {
      /* we are not connected or our SSID is too long
         so we cannot report an rssi */
      rc = scnprintf(cmd, len, "OK");
   }
   else
   {
      /* we are connected with a valid SSID
         so we can write the SSID into the return buffer
         (note that it is not NUL-terminated) */
      memcpy(cmd, pHddStaCtx->conn_info.SSID.SSID.ssId, ssidlen );

      vosStatus = wlan_hdd_get_rssi(pAdapter, &s7Rssi);

      if (VOS_STATUS_SUCCESS == vosStatus)
      {
          /* append the rssi to the ssid in the format required by
             the WiFI Framework */
          rc = scnprintf(&cmd[ssidlen], len - ssidlen, " rssi %d", s7Rssi);
          rc += ssidlen;
      }
      else
      {
          rc = -1;
      }
   }

   /* verify that we wrote a valid response */
   if ((rc < 0) || (rc >= len))
   {
      // encoding or length error?
      hddLog(VOS_TRACE_LEVEL_ERROR,
             "%s: Unable to encode RSSI, got [%s]",
             __func__, cmd);
      return -EIO;
   }

   /* a value is being successfully returned */
   return rc;
}

/*
 * Support for SoftAP channel range private command
 */
static int iw_softap_set_channel_range( struct net_device *dev,
                                        int startChannel,
                                        int endChannel,
                                        int band)
{
    VOS_STATUS status;
    int ret = 0;
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);

    if (!capable(CAP_NET_ADMIN))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("permission check failed"));
        return -EPERM;
    }

    status = WLANSAP_SetChannelRange(hHal, startChannel, endChannel, band);

    if (VOS_STATUS_SUCCESS != status)
    {
        ret = -EINVAL;
    }
    pHddCtx->is_dynamic_channel_range_set = 1;
    return ret;
}

VOS_STATUS  wlan_hdd_enter_bmps(hdd_adapter_t *pAdapter, int mode)
{
   struct statsContext context;
   eHalStatus status;
   hdd_context_t *pHddCtx;

   if (NULL == pAdapter)
   {
       hddLog(VOS_TRACE_LEVEL_FATAL, "Adapter NULL");
       return VOS_STATUS_E_FAULT;
   }

   hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "power mode=%d", mode);
   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   if (pHddCtx->isLogpInProgress) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s:LOGP in Progress. Ignore!!!", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   init_completion(&context.completion);

   context.pAdapter = pAdapter;
   context.magic = POWER_CONTEXT_MAGIC;

   if (DRIVER_POWER_MODE_ACTIVE == mode)
   {
       hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s:Wlan driver Entering "
               "Full Power", __func__);
       status = sme_RequestFullPower(WLAN_HDD_GET_HAL_CTX(pAdapter),
                       iw_power_callback_fn, &context,
                       eSME_FULL_PWR_NEEDED_BY_HDD);
       // Enter Full power command received from GUI this means we are disconnected
       // Set PMC remainInPowerActiveTillDHCP flag to disable auto BMPS entry by PMC
       sme_SetDHCPTillPowerActiveFlag(pHddCtx->hHal, TRUE);
       if (eHAL_STATUS_PMC_PENDING == status)
       {
           unsigned long rc;
           /* request was sent -- wait for the response */
           rc = wait_for_completion_timeout(
                   &context.completion,
                   msecs_to_jiffies(WLAN_WAIT_TIME_POWER));

           if (!rc) {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("SME timed out while requesting full power"));
           }
       }
   }
   else if (DRIVER_POWER_MODE_AUTO == mode)
   {
       if (pHddCtx->cfg_ini->fIsBmpsEnabled)
       {
           hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s:Wlan driver Entering Bmps ",
                  __func__);
           // Enter BMPS command received from GUI this means DHCP is completed
           // Clear PMC remainInPowerActiveTillDHCP flag to enable auto BMPS entry
           sme_SetDHCPTillPowerActiveFlag(WLAN_HDD_GET_HAL_CTX(pAdapter),
                    FALSE);
           status = sme_RequestBmps(WLAN_HDD_GET_HAL_CTX(pAdapter),
                           iw_power_callback_fn, &context);
           if (eHAL_STATUS_PMC_PENDING == status)
           {
               unsigned long rc;
               /* request was sent -- wait for the response */
               rc = wait_for_completion_timeout(
                           &context.completion,
                           msecs_to_jiffies(WLAN_WAIT_TIME_POWER));
               if (!rc) {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                       FL("SME timed out while requesting BMPS"));
               }
           }
       }
       else
       {
           hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "BMPS is not "
                   "enabled in the cfg");
       }
   }

   /* either we never sent a request, we sent a request and received a
      response or we sent a request and timed out.  if we never sent a
      request or if we sent a request and got a response, we want to
      clear the magic out of paranoia.  if we timed out there is a
      race condition such that the callback function could be
      executing at the same time we are. of primary concern is if the
      callback function had already verified the "magic" but had not
      yet set the completion variable when a timeout occurred. we
      serialize these activities by invalidating the magic while
      holding a shared spinlock which will cause us to block if the
      callback is currently executing */
   spin_lock(&hdd_context_lock);
   context.magic = 0;
   spin_unlock(&hdd_context_lock);

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS  wlan_hdd_set_powersave(hdd_adapter_t *pAdapter, int mode)
{
   hdd_context_t *pHddCtx;

   if (NULL == pAdapter)
   {
       hddLog(VOS_TRACE_LEVEL_FATAL, "Adapter NULL");
       return VOS_STATUS_E_FAULT;
   }

   hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "power mode=%d", mode);

   pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

   if (DRIVER_POWER_MODE_ACTIVE == mode)
   {
       hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s:Wlan driver Entering "
               "Full Power", __func__);

       /*
        * Enter Full power command received from GUI
        * this means we are disconnected
        */
       sme_PsOffloadDisablePowerSave(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                     pAdapter->sessionId);
   }
   else if (DRIVER_POWER_MODE_AUTO == mode)
   {
       if (pHddCtx->cfg_ini->fIsBmpsEnabled)
       {
           hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s:Wlan driver Entering Bmps ",
                  __func__);

           /*
            * Enter BMPS command received from GUI
            * this means DHCP is completed
            */
           sme_PsOffloadEnablePowerSave(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                        pAdapter->sessionId);
       }
       else
       {
           hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "BMPS is not "
                   "enabled in the cfg");
       }
   }
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS wlan_hdd_exit_lowpower(hdd_context_t *pHddCtx,
                                       hdd_adapter_t *pAdapter)
{
   VOS_STATUS vos_Status;

   if ((NULL == pAdapter) || (NULL == pHddCtx))
   {
       hddLog(VOS_TRACE_LEVEL_FATAL, "Invalid pointer");
       return VOS_STATUS_E_FAULT;
   }

   /**Exit from Deep sleep or standby if we get the driver
   START cmd from android GUI
    */
   if (WLAN_MAP_DRIVER_STOP_TO_STANDBY == pHddCtx->cfg_ini->nEnableDriverStop)
   {
       hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: WLAN being exit "
              "from Stand by",__func__);
       vos_Status = hdd_exit_standby(pHddCtx);
   }
   else if (eHDD_SUSPEND_DEEP_SLEEP == pHddCtx->hdd_ps_state)
   {
       hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: WLAN being exit "
              "from deep sleep",__func__);
       vos_Status = hdd_exit_deep_sleep(pHddCtx, pAdapter);
   }
   else
   {
       hddLog(VOS_TRACE_LEVEL_WARN, "%s: Not in standby or deep sleep. "
               "Ignore start cmd %d", __func__, pHddCtx->hdd_ps_state);
       vos_Status = VOS_STATUS_SUCCESS;
   }

   return vos_Status;
}

VOS_STATUS wlan_hdd_enter_lowpower(hdd_context_t *pHddCtx)
{
   VOS_STATUS vos_Status = VOS_STATUS_E_FAILURE;

   if (NULL == pHddCtx)
   {
        hddLog(VOS_TRACE_LEVEL_ERROR, "HDD context NULL");
        return VOS_STATUS_E_FAULT;
   }

   if (WLAN_MAP_DRIVER_STOP_TO_STANDBY == pHddCtx->cfg_ini->nEnableDriverStop)
   {
      //Execute standby procedure.
      //Executing standby procedure will cause the STA to
      //disassociate first and then the chip will be put into standby.
      hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "Wlan driver entering Stand by mode");
      vos_Status  = hdd_enter_standby(pHddCtx);
   }
   else if (WLAN_MAP_DRIVER_STOP_TO_DEEP_SLEEP ==
            pHddCtx->cfg_ini->nEnableDriverStop)
   {
       //Execute deep sleep procedure
       hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "Wlan driver entering "
               "deep sleep mode");
       //Deep sleep not supported
       vos_Status  = hdd_enter_standby(pHddCtx);
   }
   else
   {
       hddLog(VOS_TRACE_LEVEL_INFO_LOW, "%s: Driver stop is not enabled %d",
           __func__, pHddCtx->cfg_ini->nEnableDriverStop);
       vos_Status = VOS_STATUS_SUCCESS;
   }

   return vos_Status;
}


void* wlan_hdd_change_country_code_callback(void *pAdapter)
{

    hdd_adapter_t *call_back_pAdapter = pAdapter;
    complete(&call_back_pAdapter->change_country_code);

    return NULL;
}

/**
 * __iw_set_priv() - SIOCSIWPRIV ioctl handler
 * @dev: device upon which the ioctl was received
 * @info: ioctl request information
 * @wrqu: ioctl request data
 * @extra: ioctl extra data
 *
 * Return: 0 on success, non-zero on error
 */
static int __iw_set_priv(struct net_device *dev, struct iw_request_info *info,
			 union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    char *cmd = NULL;
    int cmd_len = wrqu->data.length;
    int ret = 0;
    int rc = 0;
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    ENTER();
    cmd = mem_alloc_copy_from_user_helper(wrqu->data.pointer,
                                          wrqu->data.length);
    if (NULL == cmd)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "mem_alloc_copy_from_user_helper fail");
        return -ENOMEM;
    }

    if (ioctl_debug)
    {
       pr_info("%s: req [%s] len [%d]\n", __func__, cmd, cmd_len);
    }

    hddLog(VOS_TRACE_LEVEL_INFO_MED,
           "%s: ***Received %s cmd from Wi-Fi GUI***", __func__, cmd);

    if (pHddCtx->isLogpInProgress) {
        if (ioctl_debug)
        {
            pr_info("%s: RESTART in progress\n", __func__);
        }

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                 "%s:LOGP in Progress. Ignore!!!",__func__);
        kfree(cmd);
        return -EBUSY;
    }

    if (strncmp(cmd, "CSCAN", 5) == 0 )
    {
       if (eHAL_STATUS_SUCCESS != iw_set_cscan(dev, info, wrqu, cmd)) {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "%s: Error in iw_set_scan!", __func__);
          rc = -EINVAL;
       }
    }
    else if( strcasecmp(cmd, "start") == 0 ) {

        hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "Start command");
        /*Exit from Deep sleep or standby if we get the driver START cmd from android GUI*/

        vos_status = wlan_hdd_exit_lowpower(pHddCtx, pAdapter);
        if (vos_status == VOS_STATUS_SUCCESS)
        {
            union iwreq_data wrqu;
            char buf[10];

            memset(&wrqu, 0, sizeof(wrqu));
            wrqu.data.length = strlcpy(buf, "START", sizeof(buf));
            wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, buf);
        }
        else
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: START CMD Status %d", __func__, vos_status);
            rc = -EIO;
        }
        goto done;
    }
    else if( strcasecmp(cmd, "stop") == 0 )
    {
        union iwreq_data wrqu;
        char buf[10];

        hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "Stop command");

        wlan_hdd_enter_lowpower(pHddCtx);
        memset(&wrqu, 0, sizeof(wrqu));
        wrqu.data.length = strlcpy(buf, "STOP", sizeof(buf));
        wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, buf);
        goto done;
    }
    else if (strcasecmp(cmd, "macaddr") == 0)
    {
        ret = snprintf(cmd, cmd_len, "Macaddr = " MAC_ADDRESS_STR,
                       MAC_ADDR_ARRAY(pAdapter->macAddressCurrent.bytes));
    }
    else if (strcasecmp(cmd, "scan-active") == 0)
    {
        hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
        pHddCtx->ioctl_scan_mode = eSIR_ACTIVE_SCAN;
        ret = snprintf(cmd, cmd_len, "OK");
    }
    else if (strcasecmp(cmd, "scan-passive") == 0)
    {
        hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
        pHddCtx->ioctl_scan_mode = eSIR_PASSIVE_SCAN;
        ret = snprintf(cmd, cmd_len, "OK");
    }
    else if( strcasecmp(cmd, "scan-mode") == 0 )
    {
        ret = snprintf(cmd, cmd_len, "ScanMode = %u", pAdapter->scan_info.scan_mode);
    }
    else if( strcasecmp(cmd, "linkspeed") == 0 )
    {
        ret = iw_get_linkspeed(dev, info, wrqu, cmd);
    }
    else if( strncasecmp(cmd, "COUNTRY", 7) == 0 ) {
        char *country_code;
        unsigned long rc;
        eHalStatus eHal_status;

        country_code =  cmd + 8;

        init_completion(&pAdapter->change_country_code);

        eHal_status = sme_ChangeCountryCode(pHddCtx->hHal,
                                            (void *)(tSmeChangeCountryCallback)wlan_hdd_change_country_code_callback,
                                            country_code,
                                            pAdapter,
                                            pHddCtx->pvosContext,
                                            eSIR_TRUE,
                                            eSIR_TRUE);

        /* Wait for completion */
        rc = wait_for_completion_timeout(&pAdapter->change_country_code,
                                  msecs_to_jiffies(WLAN_WAIT_TIME_STATS));

        if (!rc) {
            hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("SME timedout while setting country code"));
        }

        if (eHAL_STATUS_SUCCESS != eHal_status)
        {
            VOS_TRACE( VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                       "%s: SME Change Country code fail", __func__);
            kfree(cmd);
            return -EIO;
        }
    }
    else if( strncasecmp(cmd, "rssi", 4) == 0 )
    {
        ret = iw_get_rssi(dev, info, wrqu, cmd);
    }
    else if( strncasecmp(cmd, "powermode", 9) == 0 ) {
        int mode;
        char *ptr;

        if (9 < cmd_len)
        {
            ptr = (char*)(cmd + 9);

        }else{
              VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                        "CMD LENGTH %d is not correct",cmd_len);
              kfree(cmd);
              return -EINVAL;
        }

        if (1 != sscanf(ptr,"%d",&mode))
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "powermode input %s is not correct",ptr);
            kfree(cmd);
            return -EIO;
        }

        if(!pHddCtx->cfg_ini->enablePowersaveOffload)
            wlan_hdd_enter_bmps(pAdapter, mode);
        else
            wlan_hdd_set_powersave(pAdapter, mode);
    }
    else if (strncasecmp(cmd, "getpower", 8) == 0 ) {
        v_U32_t pmc_state;
        v_U16_t value;

        pmc_state = pmcGetPmcState(WLAN_HDD_GET_HAL_CTX(pAdapter));
        if(pmc_state == BMPS) {
           value = DRIVER_POWER_MODE_AUTO;
        }
        else {
           value = DRIVER_POWER_MODE_ACTIVE;
        }
        ret = snprintf(cmd, cmd_len, "powermode = %u", value);
    }
    else if( strncasecmp(cmd, "btcoexmode", 10) == 0 ) {
        hddLog( VOS_TRACE_LEVEL_INFO, "btcoexmode");
        /*TODO: set the btcoexmode*/
    }
    else if( strcasecmp(cmd, "btcoexstat") == 0 ) {

        hddLog(VOS_TRACE_LEVEL_INFO, "BtCoex Status");
        /*TODO: Return the btcoex status*/
    }
    else if( strcasecmp(cmd, "rxfilter-start") == 0 ) {

        hddLog(VOS_TRACE_LEVEL_INFO, "Rx Data Filter Start command");

        /*TODO: Enable Rx data Filter*/
    }
    else if( strcasecmp(cmd, "rxfilter-stop") == 0 ) {

        hddLog(VOS_TRACE_LEVEL_INFO, "Rx Data Filter Stop command");

        /*TODO: Disable Rx data Filter*/
    }
    else if( strcasecmp(cmd, "rxfilter-statistics") == 0 ) {

        hddLog( VOS_TRACE_LEVEL_INFO, "Rx Data Filter Statistics command");
        /*TODO: rxfilter-statistics*/
    }
    else if( strncasecmp(cmd, "rxfilter-add", 12) == 0 ) {

        hddLog( VOS_TRACE_LEVEL_INFO, "rxfilter-add");
        /*TODO: rxfilter-add*/
    }
    else if( strncasecmp(cmd, "rxfilter-remove",15) == 0 ) {

        hddLog( VOS_TRACE_LEVEL_INFO, "rxfilter-remove");
        /*TODO: rxfilter-remove*/
    }
#ifdef FEATURE_WLAN_SCAN_PNO
    else if( strncasecmp(cmd, "pnosetup", 8) == 0 ) {
        hddLog( VOS_TRACE_LEVEL_INFO, "pnosetup");
        /*TODO: support pnosetup*/
    }
    else if( strncasecmp(cmd, "pnoforce", 8) == 0 ) {
        hddLog( VOS_TRACE_LEVEL_INFO, "pnoforce");
        /*TODO: support pnoforce*/
    }
    else if( strncasecmp(cmd, "pno",3) == 0 ) {

        hddLog( VOS_TRACE_LEVEL_INFO, "pno");
        vos_status = iw_set_pno(dev, info, wrqu, cmd, 3);
        kfree(cmd);
        return (vos_status == VOS_STATUS_SUCCESS) ? 0 : -EINVAL;
    }
    else if( strncasecmp(cmd, "rssifilter",10) == 0 ) {
        hddLog( VOS_TRACE_LEVEL_INFO, "rssifilter");
        vos_status = iw_set_rssi_filter(dev, info, wrqu, cmd, 10);
        kfree(cmd);
        return (vos_status == VOS_STATUS_SUCCESS) ? 0 : -EINVAL;
    }
#endif /*FEATURE_WLAN_SCAN_PNO*/
    else if( strncasecmp(cmd, "powerparams",11) == 0 ) {
      hddLog( VOS_TRACE_LEVEL_INFO, "powerparams");
      vos_status = iw_set_power_params(dev, info, wrqu, cmd, 11);
      kfree(cmd);
      return (vos_status == VOS_STATUS_SUCCESS) ? 0 : -EINVAL;
    }
    else if( 0 == strncasecmp(cmd, "CONFIG-TX-TRACKING", 18) ) {
        tSirTxPerTrackingParam tTxPerTrackingParam;
        char *ptr;

        if (18 < cmd_len)
        {
           ptr = (char*)(cmd + 18);
        }else{
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         "CMD LENGTH %d is not correct",cmd_len);
               kfree(cmd);
               return -EINVAL;
        }

        if (4 != sscanf(ptr,"%hhu %hhu %hhu %u",
                        &(tTxPerTrackingParam.ucTxPerTrackingEnable),
                        &(tTxPerTrackingParam.ucTxPerTrackingPeriod),
                        &(tTxPerTrackingParam.ucTxPerTrackingRatio),
                        &(tTxPerTrackingParam.uTxPerTrackingWatermark)))
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "CONFIG-TX-TRACKING %s input is not correct",ptr);
                      kfree(cmd);
                      return -EIO;
        }

        // parameters checking
        // period has to be larger than 0
        if (0 == tTxPerTrackingParam.ucTxPerTrackingPeriod)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN, "Period input is not correct");
            kfree(cmd);
            return -EIO;
        }

        // use default value 5 is the input is not reasonable. in unit of 10%
        if ((tTxPerTrackingParam.ucTxPerTrackingRatio > TX_PER_TRACKING_MAX_RATIO) || (0 == tTxPerTrackingParam.ucTxPerTrackingRatio))
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN, "Ratio input is not good. use default 5");
            tTxPerTrackingParam.ucTxPerTrackingRatio = TX_PER_TRACKING_DEFAULT_RATIO;
        }

        // default is 5
        if (0 == tTxPerTrackingParam.uTxPerTrackingWatermark)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN, "Tx Packet number input is not good. use default 5");
            tTxPerTrackingParam.uTxPerTrackingWatermark = TX_PER_TRACKING_DEFAULT_WATERMARK;
        }

        if (eHAL_STATUS_SUCCESS !=
            sme_SetTxPerTracking(pHddCtx->hHal,
                                 hdd_tx_per_hit_cb,
                                 (void*)pAdapter, &tTxPerTrackingParam)) {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN, "Set Tx PER Tracking Failed!");
            rc = -EIO;
        }
    }
    else {
        hddLog( VOS_TRACE_LEVEL_WARN, "%s: Unsupported GUI command %s",
                __func__, cmd);
    }
done:
    /* many of the commands write information back into the command
       string using snprintf().  check the return value here in one
       place */
    if ((ret < 0) || (ret >= cmd_len))
    {
       /* there was an encoding error or overflow */
       rc = -EINVAL;
    }
    else if (ret > 0)
    {
       if (copy_to_user(wrqu->data.pointer, cmd, ret))
       {
          hddLog(VOS_TRACE_LEVEL_ERROR,
                 "%s: failed to copy data to user buffer", __func__);
          kfree(cmd);
          return -EFAULT;
       }
       wrqu->data.length = ret;
    }

    if (ioctl_debug)
    {
       pr_info("%s: rsp [%s] len [%d] status %d\n",
               __func__, cmd, wrqu->data.length, rc);
    }
    kfree(cmd);
    return rc;
}

/**
 * iw_set_priv() - SSR wrapper for __iw_set_priv()
 * @dev: pointer to net_device
 * @info: pointer to iw_request_info
 * @wrqu: pointer to iwreq_data
 * @extra: pointer to extra ioctl payload
 *
 * Return: 0 on success, error number otherwise
 */
static int iw_set_priv(struct net_device *dev, struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __iw_set_priv(dev, info, wrqu, extra);
	vos_ssr_unprotect(__func__);

	return ret;
}

static int iw_set_nick(struct net_device *dev,
                       struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
   ENTER();
   return 0;
}

static int iw_get_nick(struct net_device *dev,
                       struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
   ENTER();
   return 0;
}

static struct iw_statistics *get_wireless_stats(struct net_device *dev)
{
   ENTER();
   return NULL;
}

static int iw_set_encode(struct net_device *dev,struct iw_request_info *info,
                        union iwreq_data *wrqu,char *extra)

{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   hdd_wext_state_t  *pWextState =  WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
   struct iw_point *encoderq = &(wrqu->encoding);
   v_U32_t keyId;
   v_U8_t key_length;
   eCsrEncryptionType encryptionType = eCSR_ENCRYPT_TYPE_NONE;
   v_BOOL_t fKeyPresent = 0;
   int i;
   eHalStatus status = eHAL_STATUS_SUCCESS;


   ENTER();

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!",__func__);
        return 0;
   }


   keyId = encoderq->flags & IW_ENCODE_INDEX;

   if(keyId)
   {
       if(keyId > MAX_WEP_KEYS)
       {
           return -EINVAL;
       }

       fKeyPresent = 1;
       keyId--;
   }
   else
   {
       fKeyPresent = 0;
   }


   if(wrqu->data.flags & IW_ENCODE_DISABLED)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "****iwconfig wlan0 key off*****");
       if(!fKeyPresent) {

          for(i=0;i < CSR_MAX_NUM_KEY; i++) {

             if(pWextState->roamProfile.Keys.KeyMaterial[i])
                pWextState->roamProfile.Keys.KeyLength[i] = 0;
          }
       }
       pHddStaCtx->conn_info.authType =  eCSR_AUTH_TYPE_OPEN_SYSTEM;
       pWextState->wpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
       pWextState->roamProfile.EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
       pWextState->roamProfile.mcEncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;

       pHddStaCtx->conn_info.ucEncryptionType = eCSR_ENCRYPT_TYPE_NONE;
       pHddStaCtx->conn_info.mcEncryptionType = eCSR_ENCRYPT_TYPE_NONE;

       if(eConnectionState_Associated == pHddStaCtx->conn_info.connState)
       {
           INIT_COMPLETION(pAdapter->disconnect_comp_var);
           status = sme_RoamDisconnect( WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED );
           if(eHAL_STATUS_SUCCESS == status)
           {
                 unsigned long rc;
                 rc = wait_for_completion_timeout(
                              &pAdapter->disconnect_comp_var,
                               msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
                if (!rc)
                     hddLog(VOS_TRACE_LEVEL_ERROR,
                            FL("failed wait on disconnect_comp_var"));
           }
       }

       return status;

   }

   if (wrqu->data.flags & (IW_ENCODE_OPEN | IW_ENCODE_RESTRICTED))
   {
      hddLog(VOS_TRACE_LEVEL_INFO, "iwconfig wlan0 key on");

      pHddStaCtx->conn_info.authType = (encoderq->flags & IW_ENCODE_RESTRICTED) ? eCSR_AUTH_TYPE_SHARED_KEY : eCSR_AUTH_TYPE_OPEN_SYSTEM;

   }


   if(wrqu->data.length > 0)
   {
       hddLog(VOS_TRACE_LEVEL_INFO, "%s : wrqu->data.length : %d",__func__,wrqu->data.length);

       key_length = wrqu->data.length;

       /* IW_ENCODING_TOKEN_MAX is the value that is set for wrqu->data.length by iwconfig.c when 'iwconfig wlan0 key on' is issued.*/

       if(5 == key_length)
       {
           hddLog(VOS_TRACE_LEVEL_INFO, "%s: Call with WEP40,key_len=%d",__func__,key_length);

           if((IW_AUTH_KEY_MGMT_802_1X == pWextState->authKeyMgmt) && (eCSR_AUTH_TYPE_OPEN_SYSTEM == pHddStaCtx->conn_info.authType))
           {
               encryptionType = eCSR_ENCRYPT_TYPE_WEP40;
           }
           else
           {
               encryptionType = eCSR_ENCRYPT_TYPE_WEP40_STATICKEY;
           }
       }
       else if(13 == key_length)
       {
           hddLog(VOS_TRACE_LEVEL_INFO, "%s:Call with WEP104,key_len:%d",__func__,key_length);

           if((IW_AUTH_KEY_MGMT_802_1X == pWextState->authKeyMgmt) && (eCSR_AUTH_TYPE_OPEN_SYSTEM == pHddStaCtx->conn_info.authType))
           {
               encryptionType = eCSR_ENCRYPT_TYPE_WEP104;
           }
           else
           {
               encryptionType = eCSR_ENCRYPT_TYPE_WEP104_STATICKEY;
           }
       }
       else
       {
           hddLog(VOS_TRACE_LEVEL_WARN, "%s: Invalid WEP key length :%d",
                  __func__, key_length);
           return -EINVAL;
       }

       pHddStaCtx->conn_info.ucEncryptionType = encryptionType;
       pHddStaCtx->conn_info.mcEncryptionType = encryptionType;
       pWextState->roamProfile.EncryptionType.numEntries = 1;
       pWextState->roamProfile.EncryptionType.encryptionType[0] = encryptionType;
       pWextState->roamProfile.mcEncryptionType.numEntries = 1;
       pWextState->roamProfile.mcEncryptionType.encryptionType[0] = encryptionType;

       if((eConnectionState_NotConnected == pHddStaCtx->conn_info.connState) &&
            ((eCSR_AUTH_TYPE_OPEN_SYSTEM == pHddStaCtx->conn_info.authType) ||
              (eCSR_AUTH_TYPE_SHARED_KEY == pHddStaCtx->conn_info.authType)))
       {

          vos_mem_copy(&pWextState->roamProfile.Keys.KeyMaterial[keyId][0],extra,key_length);

          pWextState->roamProfile.Keys.KeyLength[keyId] = (v_U8_t)key_length;
          pWextState->roamProfile.Keys.defaultIndex = (v_U8_t)keyId;

          return status;
       }
   }

   return 0;
}

static int iw_get_encodeext(struct net_device *dev,
               struct iw_request_info *info,
               struct iw_point *dwrq,
               char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_wext_state_t  *pWextState =  WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    tCsrRoamProfile *pRoamProfile = &(pWextState->roamProfile);
    int keyId;
    eCsrEncryptionType encryptionType = eCSR_ENCRYPT_TYPE_NONE;
    eCsrAuthType authType = eCSR_AUTH_TYPE_NONE;
    int i;

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
              "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EBUSY;
    }

    keyId = pRoamProfile->Keys.defaultIndex;

    if(keyId < 0 || keyId >= MAX_WEP_KEYS)
    {
        hddLog(LOG1,"%s: Invalid keyId : %d", __func__, keyId);
        return -EINVAL;
    }

    if(pRoamProfile->Keys.KeyLength[keyId] > 0)
    {
        dwrq->flags |= IW_ENCODE_ENABLED;
        dwrq->length = pRoamProfile->Keys.KeyLength[keyId];
        vos_mem_copy(extra, &(pRoamProfile->Keys.KeyMaterial[keyId][0]),
                     pRoamProfile->Keys.KeyLength[keyId]);
    }
    else
    {
        dwrq->flags |= IW_ENCODE_DISABLED;
    }

    for(i=0; i < MAX_WEP_KEYS; i++)
    {
        if(pRoamProfile->Keys.KeyMaterial[i] == NULL)
        {
            continue;
        }
        else
        {
            break;
        }
    }

    if(MAX_WEP_KEYS == i)
    {
        dwrq->flags |= IW_ENCODE_NOKEY;
    }
    else
    {
        dwrq->flags |= IW_ENCODE_ENABLED;
    }

    encryptionType = pRoamProfile->EncryptionType.encryptionType[0];

    if(eCSR_ENCRYPT_TYPE_NONE == encryptionType)
    {
        dwrq->flags |= IW_ENCODE_DISABLED;
    }

    authType = (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.authType;

    if(IW_AUTH_ALG_OPEN_SYSTEM == authType)
    {
        dwrq->flags |= IW_ENCODE_OPEN;
    }
    else
    {
        dwrq->flags |= IW_ENCODE_RESTRICTED;
    }
    EXIT();
    return 0;

}

static int iw_set_encodeext(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_wext_state_t  *pWextState =  WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    eHalStatus halStatus= eHAL_STATUS_SUCCESS;

    tCsrRoamProfile *pRoamProfile = &pWextState->roamProfile;
    v_U32_t status = 0;

    struct iw_encode_ext *ext = (struct iw_encode_ext*)extra;

    v_U8_t groupmacaddr[VOS_MAC_ADDR_SIZE] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    int key_index;
    struct iw_point *encoding = &wrqu->encoding;
    tCsrRoamSetKey  setKey;
    v_U32_t  roamId= 0xFF;
    VOS_STATUS vos_status;

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!",__func__);
        return 0;
    }

    key_index = encoding->flags & IW_ENCODE_INDEX;

    if(key_index > 0) {

         /*Convert from 1-based to 0-based keying*/
        key_index--;
    }
    if(!ext->key_len) {

      /* Set the encryption type to NONE */
       pRoamProfile->EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
       return status;
    }

    if(eConnectionState_NotConnected == pHddStaCtx->conn_info.connState &&
                                                  (IW_ENCODE_ALG_WEP == ext->alg))
    {
       if(IW_AUTH_KEY_MGMT_802_1X == pWextState->authKeyMgmt) {

          VOS_TRACE (VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     ("Invalid Configuration:%s"),__func__);
          return -EINVAL;
       }
       else {
         /*Static wep, update the roam profile with the keys */
          if(ext->key && (ext->key_len <= eCSR_SECURITY_WEP_KEYSIZE_MAX_BYTES) &&
                                                               key_index < CSR_MAX_NUM_KEY) {
             vos_mem_copy(&pRoamProfile->Keys.KeyMaterial[key_index][0],ext->key,ext->key_len);
             pRoamProfile->Keys.KeyLength[key_index] = (v_U8_t)ext->key_len;

             if(ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
                pRoamProfile->Keys.defaultIndex = (v_U8_t)key_index;

          }
       }
       return status;
    }

    vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));

    setKey.keyId = key_index;
    setKey.keyLength = ext->key_len;

    if(ext->key_len <= CSR_MAX_KEY_LEN) {
       vos_mem_copy(&setKey.Key[0],ext->key,ext->key_len);
    }

    if(ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
      /*Key direction for group is RX only*/
       setKey.keyDirection = eSIR_RX_ONLY;
       vos_mem_copy(setKey.peerMac,groupmacaddr, VOS_MAC_ADDR_SIZE);
    }
    else {

       setKey.keyDirection =  eSIR_TX_RX;
       vos_mem_copy(setKey.peerMac,ext->addr.sa_data, VOS_MAC_ADDR_SIZE);
    }

    /*For supplicant pae role is zero*/
    setKey.paeRole = 0;

    switch(ext->alg)
    {
       case IW_ENCODE_ALG_NONE:
         setKey.encType = eCSR_ENCRYPT_TYPE_NONE;
         break;

       case IW_ENCODE_ALG_WEP:
         setKey.encType = (ext->key_len== 5) ? eCSR_ENCRYPT_TYPE_WEP40:eCSR_ENCRYPT_TYPE_WEP104;
         break;

       case IW_ENCODE_ALG_TKIP:
       {
          v_U8_t *pKey = &setKey.Key[0];

          setKey.encType = eCSR_ENCRYPT_TYPE_TKIP;

          vos_mem_zero(pKey, CSR_MAX_KEY_LEN);

          /*Supplicant sends the 32bytes key in this order

                |--------------|----------|----------|
                |   Tk1        |TX-MIC    |  RX Mic  |
                |--------------|----------|----------|
                <---16bytes---><--8bytes--><--8bytes-->

                */
          /*Sme expects the 32 bytes key to be in the below order

                |--------------|----------|----------|
                |   Tk1        |RX-MIC    |  TX Mic  |
                |--------------|----------|----------|
                <---16bytes---><--8bytes--><--8bytes-->
               */
          /* Copy the Temporal Key 1 (TK1) */
          vos_mem_copy(pKey,ext->key,16);

         /*Copy the rx mic first*/
          vos_mem_copy(&pKey[16],&ext->key[24],8);

         /*Copy the tx mic */
          vos_mem_copy(&pKey[24],&ext->key[16],8);

       }
       break;

       case IW_ENCODE_ALG_CCMP:
          setKey.encType = eCSR_ENCRYPT_TYPE_AES;
          break;

#ifdef FEATURE_WLAN_ESE
#define IW_ENCODE_ALG_KRK 6
       case IW_ENCODE_ALG_KRK:
          setKey.encType = eCSR_ENCRYPT_TYPE_KRK;
          break;
#endif  /* FEATURE_WLAN_ESE */

       default:
          setKey.encType = eCSR_ENCRYPT_TYPE_NONE;
          break;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
          ("%s:cipher_alg:%d key_len[%d] *pEncryptionType :%d"),__func__,(int)ext->alg,(int)ext->key_len,setKey.encType);

#ifdef WLAN_FEATURE_VOWIFI_11R
    /* The supplicant may attempt to set the PTK once pre-authentication
       is done. Save the key in the UMAC and include it in the ADD
       BSS request */
    halStatus = sme_FTUpdateKey(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                pAdapter->sessionId, &setKey);
    if ( halStatus == eHAL_STATUS_FT_PREAUTH_KEY_SUCCESS )
    {
        hddLog(VOS_TRACE_LEVEL_INFO_MED,
               "%s: Update PreAuth Key success", __func__);
        return 0;
    }
    else if ( halStatus == eHAL_STATUS_FT_PREAUTH_KEY_FAILED )
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: Update PreAuth Key failed", __func__);
        return -EINVAL;
    }
#endif /* WLAN_FEATURE_VOWIFI_11R */

    pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_SETTING_KEY;

    vos_status = wlan_hdd_check_ula_done(pAdapter);
    if ( vos_status != VOS_STATUS_SUCCESS )
    {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "[%4d] wlan_hdd_check_ula_done returned ERROR status= %d",
                   __LINE__, vos_status );

       pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
    }

    halStatus = sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),pAdapter->sessionId, &setKey, &roamId );

    if ( halStatus != eHAL_STATUS_SUCCESS )
    {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "[%4d] sme_RoamSetKey returned ERROR status= %d",
                   __LINE__, halStatus );

       pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
    }

   return halStatus;
}

static int iw_set_retry(struct net_device *dev, struct iw_request_info *info,
           union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);

   ENTER();

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s:LOGP in Progress. Ignore!!!", __func__);
      return -EBUSY;
   }

   if(wrqu->retry.value < WNI_CFG_LONG_RETRY_LIMIT_STAMIN ||
       wrqu->retry.value > WNI_CFG_LONG_RETRY_LIMIT_STAMAX) {

      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("Invalid Retry-Limit=%d!!"),wrqu->retry.value);

      return -EINVAL;
   }

   if(wrqu->retry.flags & IW_RETRY_LIMIT) {

       if((wrqu->retry.flags & IW_RETRY_LONG))
       {
          if ( ccmCfgSetInt(hHal, WNI_CFG_LONG_RETRY_LIMIT, wrqu->retry.value, ccmCfgSetCallback, eANI_BOOLEAN_TRUE) != eHAL_STATUS_SUCCESS )
          {
             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL("failed to set ini parameter, WNI_CFG_LONG_RETRY_LIMIT"));
             return -EIO;
          }
       }
       else if((wrqu->retry.flags & IW_RETRY_SHORT))
       {
          if ( ccmCfgSetInt(hHal, WNI_CFG_SHORT_RETRY_LIMIT, wrqu->retry.value, ccmCfgSetCallback, eANI_BOOLEAN_TRUE) != eHAL_STATUS_SUCCESS )
          {
             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               FL("failed to set ini parameter, WNI_CFG_LONG_RETRY_LIMIT"));
             return -EIO;
          }
       }
   }
   else
   {
       return -EOPNOTSUPP;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, ("Set Retry-Limit=%d!!"),wrqu->retry.value);

   EXIT();

   return 0;

}

static int iw_get_retry(struct net_device *dev, struct iw_request_info *info,
           union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
   v_U32_t retry = 0;

   ENTER();

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
              "%s:LOGP in Progress. Ignore!!!", __func__);
      return -EBUSY;
   }

   if((wrqu->retry.flags & IW_RETRY_LONG))
   {
      wrqu->retry.flags = IW_RETRY_LIMIT | IW_RETRY_LONG;

      if ( ccmCfgGetInt(hHal, WNI_CFG_LONG_RETRY_LIMIT, &retry) != eHAL_STATUS_SUCCESS )
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                      FL("failed to get ini parameter, WNI_CFG_LONG_RETRY_LIMIT"));
         return -EIO;
      }

      wrqu->retry.value = retry;
   }
   else if ((wrqu->retry.flags & IW_RETRY_SHORT))
   {
      wrqu->retry.flags = IW_RETRY_LIMIT | IW_RETRY_SHORT;

      if ( ccmCfgGetInt(hHal, WNI_CFG_SHORT_RETRY_LIMIT, &retry) != eHAL_STATUS_SUCCESS )
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                      FL("failed to get ini parameter, WNI_CFG_LONG_RETRY_LIMIT"));
         return -EIO;
      }

      wrqu->retry.value = retry;
   }
   else {
      return -EOPNOTSUPP;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, ("Retry-Limit=%d!!"),retry);

   EXIT();

   return 0;
}

static int iw_set_mlme(struct net_device *dev,
                       struct iw_request_info *info,
                       union iwreq_data *wrqu,
                       char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    struct iw_mlme *mlme = (struct iw_mlme *)extra;
    eHalStatus status = eHAL_STATUS_SUCCESS;

    ENTER();

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!",__func__);
        return 0;
    }

    //reason_code is unused. By default it is set to eCSR_DISCONNECT_REASON_UNSPECIFIED
    switch (mlme->cmd) {
        case IW_MLME_DISASSOC:
        case IW_MLME_DEAUTH:

            if( pHddStaCtx->conn_info.connState == eConnectionState_Associated )
            {
                eCsrRoamDisconnectReason reason = eCSR_DISCONNECT_REASON_UNSPECIFIED;

                if( mlme->reason_code == HDD_REASON_MICHAEL_MIC_FAILURE )
                    reason = eCSR_DISCONNECT_REASON_MIC_ERROR;

                INIT_COMPLETION(pAdapter->disconnect_comp_var);
                status = sme_RoamDisconnect( WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId,reason);

                if(eHAL_STATUS_SUCCESS == status)
                {
                    unsigned long rc;
                    rc = wait_for_completion_timeout(
                                   &pAdapter->disconnect_comp_var,
                                   msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
                    if (!rc)
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                            FL("failed wait on disconnect_comp_var"));
                }
                else
                    hddLog(LOGE,"%s %d Command Disassociate/Deauthenticate : csrRoamDisconnect failure returned %d",
                       __func__, (int)mlme->cmd, (int)status );

                /* Resetting authKeyMgmt */
                (WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter))->authKeyMgmt = 0;

                netif_tx_disable(dev);
                netif_carrier_off(dev);
                pAdapter->hdd_stats.hddTxRxStats.netq_disable_cnt++;
                pAdapter->hdd_stats.hddTxRxStats.netq_state_off = TRUE;

            }
            else
            {
                hddLog(LOGE,"%s %d Command Disassociate/Deauthenticate called but station is not in associated state", __func__, (int)mlme->cmd );
            }
            break;
        default:
            hddLog(LOGE,"%s %d Command should be Disassociate/Deauthenticate", __func__, (int)mlme->cmd );
            return -EINVAL;
    }//end of switch

    EXIT();

    return status;

}

int process_wma_set_command(int sessid, int paramid,
                                   int sval, int vpdev)
{
    int ret = 0;
    vos_msg_t msg = {0};

    wda_cli_set_cmd_t *iwcmd = (wda_cli_set_cmd_t *)vos_mem_malloc(
                                sizeof(wda_cli_set_cmd_t));
    if (NULL == iwcmd) {
       hddLog(VOS_TRACE_LEVEL_FATAL, "%s: vos_mem_alloc failed", __func__);
       return -EINVAL;
    }
    vos_mem_zero((void *)iwcmd, sizeof(wda_cli_set_cmd_t));
    iwcmd->param_value = sval;
    iwcmd->param_vdev_id = sessid;
    iwcmd->param_id = paramid;
    iwcmd->param_vp_dev = vpdev;
    msg.type = WDA_CLI_SET_CMD;
    msg.reserved = 0;
    msg.bodyptr = (void *)iwcmd;
    if (VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA,
                                                  &msg)) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s: "
                 "Not able to post wda_cli_set_cmd message to WDA",
                 __func__);
       vos_mem_free(iwcmd);
       ret = -EIO;
    }
    return ret;
}

int process_wma_set_command_twoargs(int sessid, int paramid,
                                    int sval, int ssecval, int vpdev)
{
    int ret = 0;
    vos_msg_t msg = {0};
    wda_cli_set_cmd_t *iwcmd = vos_mem_malloc(sizeof(*iwcmd));

    if (NULL == iwcmd) {
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: vos_mem_alloc failed!", __func__);
        ret = -EINVAL;
        goto out;
    }

    vos_mem_zero(iwcmd, sizeof(*iwcmd));
    iwcmd->param_value = sval;
    iwcmd->param_sec_value = ssecval;
    iwcmd->param_vdev_id = sessid;
    iwcmd->param_id = paramid;
    iwcmd->param_vp_dev = vpdev;
    msg.type = WDA_CLI_SET_CMD;
    msg.reserved = 0;
    msg.bodyptr = iwcmd;

    if (VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA, &msg)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "%s: "
                  "Not able to post wda_cli_set_cmd message to WDA", __func__);
        vos_mem_free(iwcmd);
        ret = -EIO;
    }

out:
    return ret;
}

int wlan_hdd_update_phymode(struct net_device *net, tHalHandle hal,
                                   int new_phymode,
                                   hdd_context_t *phddctx)
{
#ifdef QCA_HT_2040_COEX
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(net);
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
#endif
    v_BOOL_t band_24 = VOS_FALSE, band_5g = VOS_FALSE;
    v_BOOL_t ch_bond24 = VOS_FALSE, ch_bond5g = VOS_FALSE;
    tSmeConfigParams smeconfig;
    tANI_U32 chwidth = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
#ifdef WLAN_FEATURE_11AC
    tANI_U32 vhtchanwidth;
#endif
    eCsrPhyMode phymode = -EIO, old_phymode;
    eCsrBand curr_band = eCSR_BAND_ALL;

    old_phymode = sme_GetPhyMode(hal);

    if (WNI_CFG_CHANNEL_BONDING_MODE_DISABLE !=
        sme_GetCBPhyStateFromCBIniValue(
                                   phddctx->cfg_ini->nChannelBondingMode24GHz))
            ch_bond24 = VOS_TRUE;

    if (WNI_CFG_CHANNEL_BONDING_MODE_DISABLE !=
        sme_GetCBPhyStateFromCBIniValue(
                                    phddctx->cfg_ini->nChannelBondingMode5GHz))
            ch_bond5g = VOS_TRUE;

    if (phddctx->cfg_ini->nBandCapability == eCSR_BAND_ALL) {
        band_24 = band_5g = VOS_TRUE;
    } else if (phddctx->cfg_ini->nBandCapability == eCSR_BAND_24) {
        band_24 = VOS_TRUE;
    } else if (phddctx->cfg_ini->nBandCapability == eCSR_BAND_5G) {
        band_5g = VOS_TRUE;
    }

    vhtchanwidth = phddctx->cfg_ini->vhtChannelWidth;
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_DEBUG, ("ch_bond24=%d "
              "ch_bond5g=%d band_24=%d band_5g=%d VHT_ch_width=%u"), ch_bond24,
              ch_bond5g, band_24, band_5g, vhtchanwidth);
    switch (new_phymode) {
    case IEEE80211_MODE_11A:
         if (band_5g) {
             sme_SetPhyMode(hal, eCSR_DOT11_MODE_11a);
             if ((hdd_setBand(net, WLAN_HDD_UI_BAND_5_GHZ) == 0))
                 phymode = eCSR_DOT11_MODE_11a;
             else {
                 sme_SetPhyMode(hal, old_phymode);
                 return -EIO;
             }
         }
         break;
    case IEEE80211_MODE_11B:
         if (band_24) {
             sme_SetPhyMode(hal, eCSR_DOT11_MODE_11b);
             if ((hdd_setBand(net, WLAN_HDD_UI_BAND_2_4_GHZ) == 0))
                 phymode = eCSR_DOT11_MODE_11b;
             else {
                 sme_SetPhyMode(hal, old_phymode);
                 return -EIO;
             }
         }
         break;
    case IEEE80211_MODE_11G:
         if (band_24) {
             sme_SetPhyMode(hal, eCSR_DOT11_MODE_11g);
             if ((hdd_setBand(net, WLAN_HDD_UI_BAND_2_4_GHZ) == 0))
                 phymode = eCSR_DOT11_MODE_11g;
             else {
                 sme_SetPhyMode(hal, old_phymode);
                 return -EIO;
             }
         }
         break;
    /*
     * UMAC doesn't have option to set MODE_11NA/MODE_11NG as phymode
     * so setting phymode as eCSR_DOT11_MODE_11n and updating the band
     * and channel bonding in configuration to reflect MODE_11NA/MODE_11NG
     */
    case IEEE80211_MODE_11NA_HT20:
         if (band_5g) {
             sme_SetPhyMode(hal, eCSR_DOT11_MODE_11n);
             if ((hdd_setBand(net, WLAN_HDD_UI_BAND_5_GHZ) == 0)) {
                 phymode = eCSR_DOT11_MODE_11n;
                 chwidth = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
                 curr_band = eCSR_BAND_5G;
             } else {
                 sme_SetPhyMode(hal, old_phymode);
                 return -EIO;
             }
         }
         break;
    case IEEE80211_MODE_11NA_HT40:
         if (band_5g && ch_bond5g) {
             sme_SetPhyMode(hal, eCSR_DOT11_MODE_11n);
             if ((hdd_setBand(net, WLAN_HDD_UI_BAND_5_GHZ) == 0)) {
                 phymode = eCSR_DOT11_MODE_11n;
                 chwidth = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;
                 curr_band = eCSR_BAND_5G;
             } else {
                 sme_SetPhyMode(hal, old_phymode);
                 return -EIO;
             }
         }
         break;
    case IEEE80211_MODE_11NG_HT20:
         if (band_24) {
             sme_SetPhyMode(hal, eCSR_DOT11_MODE_11n);
             if ((hdd_setBand(net, WLAN_HDD_UI_BAND_2_4_GHZ) == 0)) {
                 phymode = eCSR_DOT11_MODE_11n;
                 chwidth = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
                 curr_band = eCSR_BAND_24;
             } else {
                 sme_SetPhyMode(hal, old_phymode);
                 return -EIO;
             }
         }
         break;
    case IEEE80211_MODE_11NG_HT40:
         if (band_24 && ch_bond24) {
             sme_SetPhyMode(hal, eCSR_DOT11_MODE_11n);
             if ((hdd_setBand(net, WLAN_HDD_UI_BAND_2_4_GHZ) == 0)) {
                 phymode = eCSR_DOT11_MODE_11n;
                 chwidth = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;
                 curr_band = eCSR_BAND_24;
             } else {
                 sme_SetPhyMode(hal, old_phymode);
                 return -EIO;
             }
         }
         break;
#ifdef WLAN_FEATURE_11AC
    case IEEE80211_MODE_11AC_VHT20:
    case IEEE80211_MODE_11AC_VHT40:
    case IEEE80211_MODE_11AC_VHT80:
         if (band_5g) {
             sme_SetPhyMode(hal, eCSR_DOT11_MODE_11ac);
             if ((hdd_setBand(net, WLAN_HDD_UI_BAND_5_GHZ) == 0)) {
                 phymode = eCSR_DOT11_MODE_11ac;
             } else {
                 sme_SetPhyMode(hal, old_phymode);
                 return -EIO;
             }
         }
         break;
#endif
    case IEEE80211_MODE_2G_AUTO:
         sme_SetPhyMode(hal, eCSR_DOT11_MODE_AUTO);
         if ((hdd_setBand(net, WLAN_HDD_UI_BAND_2_4_GHZ) == 0)) {
             phymode = eCSR_DOT11_MODE_AUTO;
             chwidth = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;
             curr_band = eCSR_BAND_24;
         } else {
             sme_SetPhyMode(hal, old_phymode);
             return -EIO;
         }
         break;
    case IEEE80211_MODE_5G_AUTO:
         sme_SetPhyMode(hal, eCSR_DOT11_MODE_AUTO);
         if ((hdd_setBand(net, WLAN_HDD_UI_BAND_5_GHZ) == 0)) {
             phymode = eCSR_DOT11_MODE_AUTO;
             chwidth = WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;
             curr_band = eCSR_BAND_5G;
         } else {
             sme_SetPhyMode(hal, old_phymode);
             return -EIO;
         }
         break;
    default:
         return -EIO;
    }

#ifdef WLAN_FEATURE_11AC
    switch (new_phymode) {
    case IEEE80211_MODE_11AC_VHT20:
        vhtchanwidth = eHT_CHANNEL_WIDTH_20MHZ;
    case IEEE80211_MODE_11AC_VHT40:
        vhtchanwidth = eHT_CHANNEL_WIDTH_40MHZ;
    case IEEE80211_MODE_11AC_VHT80:
        vhtchanwidth = eHT_CHANNEL_WIDTH_80MHZ;
    default:
        vhtchanwidth = phddctx->cfg_ini->vhtChannelWidth;
    }
#endif

    if (phymode != -EIO) {
        sme_GetConfigParam(hal, &smeconfig);
        smeconfig.csrConfig.phyMode = phymode;
        if (phymode == eCSR_DOT11_MODE_11n &&
                             chwidth == WNI_CFG_CHANNEL_BONDING_MODE_DISABLE) {
            if (curr_band == eCSR_BAND_24)
                smeconfig.csrConfig.channelBondingMode24GHz =
                                          WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
            else
                smeconfig.csrConfig.channelBondingMode5GHz =
                                          WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;

#ifdef QCA_HT_2040_COEX
            smeconfig.csrConfig.obssEnabled = eANI_BOOLEAN_FALSE;
            halStatus = sme_SetHT2040Mode(hal, pAdapter->sessionId,
                                      eHT_CHAN_HT20, eANI_BOOLEAN_FALSE);
            if (halStatus == eHAL_STATUS_FAILURE) {
                hddLog(LOGE, FL("Failed to disable OBSS"));
                return -EIO;
            }
#endif
        } else if (phymode == eCSR_DOT11_MODE_11n &&
                              chwidth == WNI_CFG_CHANNEL_BONDING_MODE_ENABLE) {
            if (curr_band == eCSR_BAND_24)
                smeconfig.csrConfig.channelBondingMode24GHz =
                                    phddctx->cfg_ini->nChannelBondingMode24GHz;
            else
                smeconfig.csrConfig.channelBondingMode5GHz =
                                    phddctx->cfg_ini->nChannelBondingMode5GHz;

#ifdef QCA_HT_2040_COEX
            if (phddctx->cfg_ini->ht2040CoexEnabled) {
                smeconfig.csrConfig.obssEnabled = eANI_BOOLEAN_TRUE;
                halStatus = sme_SetHT2040Mode(hal, pAdapter->sessionId,
                                      eHT_CHAN_HT20, eANI_BOOLEAN_TRUE);
                if (halStatus == eHAL_STATUS_FAILURE) {
                    hddLog(LOGE, FL("Failed to enable OBSS"));
                    return -EIO;
                }
            }
#endif
        }
#ifdef WLAN_FEATURE_11AC
        smeconfig.csrConfig.nVhtChannelWidth = vhtchanwidth;
#endif

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                                  "SET PHY MODE=%d",
                                                  smeconfig.csrConfig.phyMode);
        sme_UpdateConfig(hal, &smeconfig);
    }

    return 0;
}

/* set param sub-ioctls */
static int iw_setint_getnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_wext_state_t  *pWextState =  WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    hdd_context_t     *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tSmeConfigParams smeConfig;
    int *value = (int *)extra;
    int sub_cmd = value[0];
    int set_value = value[1];
    int ret = 0; /* success */
    int enable_pbm, enable_mp;

#ifdef CONFIG_HAS_EARLYSUSPEND
    v_U8_t nEnableSuspendOld;
#endif
    INIT_COMPLETION(pWextState->completion_var);
    memset(&smeConfig, 0x00, sizeof(smeConfig));

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EBUSY;
    }

    switch(sub_cmd)
    {
        case WE_SET_11D_STATE:
        {
            if((ENABLE_11D == set_value) || (DISABLE_11D == set_value)) {

                sme_GetConfigParam(hHal, &smeConfig);
                smeConfig.csrConfig.Is11dSupportEnabled = (v_BOOL_t)set_value;

                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                        ("11D state=%d!!"),
                        smeConfig.csrConfig.Is11dSupportEnabled);

                sme_UpdateConfig(hHal, &smeConfig);
            }
            else {
               return -EINVAL;
            }
            break;
        }

        case WE_WOWL:
        {
           switch (set_value)
           {
              case 0x00:
                 hdd_exit_wowl(pAdapter);
                 break;
              case 0x01:
              case 0x02:
              case 0x03:
                 enable_mp =  (set_value & 0x01) ? 1 : 0;
                 enable_pbm = (set_value & 0x02) ? 1 : 0;
                 hddLog(LOGE, "magic packet ? = %s pattern byte matching ? = %s",
                     (enable_mp ? "YES":"NO"), (enable_pbm ? "YES":"NO"));
                 hdd_enter_wowl(pAdapter, enable_mp, enable_pbm);
                 break;
              default:
                 hddLog(LOGE, "Invalid arg  %d in WE_WOWL IOCTL", set_value);
                 ret = -EINVAL;
                 break;
           }

           break;
        }
        case WE_SET_POWER:
        {
           switch (set_value)
           {
              case  0: //Full Power
              {
                 struct statsContext context;
                 eHalStatus status;

                 init_completion(&context.completion);

                 context.pAdapter = pAdapter;
                 context.magic = POWER_CONTEXT_MAGIC;

                 status = sme_RequestFullPower(WLAN_HDD_GET_HAL_CTX(pAdapter),
                              iw_power_callback_fn, &context,
                              eSME_FULL_PWR_NEEDED_BY_HDD);
                 if (eHAL_STATUS_PMC_PENDING == status)
                 {
                    unsigned long rc;
                    rc = wait_for_completion_timeout(
                                &context.completion,
                                 msecs_to_jiffies(WLAN_WAIT_TIME_POWER));

                    if (!rc) {
                       hddLog(VOS_TRACE_LEVEL_ERROR,
                           FL("SME timed out while requesting full power"));
                    }
                 }
                 /* either we have a response or we timed out.  if we timed
                    out there is a race condition such that the callback
                    function could be executing at the same time we are. of
                    primary concern is if the callback function had already
                    verified the "magic" but had not yet set the completion
                    variable when a timeout occurred. we serialize these
                    activities by invalidating the magic while holding a
                    shared spinlock which will cause us to block if the
                    callback is currently executing */
                 spin_lock(&hdd_context_lock);
                 context.magic = 0;
                 spin_unlock(&hdd_context_lock);

                 hddLog(LOGE, "iwpriv Full Power completed");
                 break;
              }
              case  1: //Enable BMPS
                 sme_EnablePowerSave(hHal, ePMC_BEACON_MODE_POWER_SAVE);
                 break;
              case  2: //Disable BMPS
                 sme_DisablePowerSave(hHal, ePMC_BEACON_MODE_POWER_SAVE);
                 break;
              case  3: //Request Bmps
              {
                 struct statsContext context;
                 eHalStatus status;

                 init_completion(&context.completion);

                 context.pAdapter = pAdapter;
                 context.magic = POWER_CONTEXT_MAGIC;

                 status = sme_RequestBmps(WLAN_HDD_GET_HAL_CTX(pAdapter),
                           iw_power_callback_fn, &context);
                 if (eHAL_STATUS_PMC_PENDING == status)
                 {
                    unsigned long rc;
                    rc = wait_for_completion_timeout(
                              &context.completion,
                              msecs_to_jiffies(WLAN_WAIT_TIME_POWER));
                    if (!rc) {
                       hddLog(VOS_TRACE_LEVEL_ERROR,
                           FL("SME timed out while requesting BMPS"));
                    }
                 }
                 /* either we have a response or we timed out.  if we
                    timed out there is a race condition such that the
                    callback function could be executing at the same
                    time we are. of primary concern is if the callback
                    function had already verified the "magic" but had
                    not yet set the completion variable when a timeout
                    occurred. we serialize these activities by
                    invalidating the magic while holding a shared
                    spinlock which will cause us to block if the
                    callback is currently executing */
                 spin_lock(&hdd_context_lock);
                 context.magic = 0;
                 spin_unlock(&hdd_context_lock);

                 hddLog(LOGE, "iwpriv Request BMPS completed");
                 break;
              }
              case  4: //Enable IMPS
                 sme_EnablePowerSave(hHal, ePMC_IDLE_MODE_POWER_SAVE);
                 break;
              case  5: //Disable IMPS
                 sme_DisablePowerSave(hHal, ePMC_IDLE_MODE_POWER_SAVE);
                 break;
              case  6: //Enable Standby
                 sme_EnablePowerSave(hHal, ePMC_STANDBY_MODE_POWER_SAVE);
                 break;
              case  7: //Disable Standby
                 sme_DisablePowerSave(hHal, ePMC_STANDBY_MODE_POWER_SAVE);
                 break;
              case  8: //Request Standby
#ifdef CONFIG_HAS_EARLYSUSPEND
#endif
                 break;
              case  9: //Start Auto Bmps Timer
                 sme_StartAutoBmpsTimer(hHal);
                 break;
              case  10://Stop Auto BMPS Timer
                 sme_StopAutoBmpsTimer(hHal);
                 break;
#ifdef CONFIG_HAS_EARLYSUSPEND
              case  11://suspend to standby
#ifdef CONFIG_HAS_EARLYSUSPEND
                 nEnableSuspendOld = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->nEnableSuspend;
                 (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->nEnableSuspend = 1;
                 (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->nEnableSuspend = nEnableSuspendOld;
#endif
                 break;
              case  12://suspend to deep sleep
#ifdef CONFIG_HAS_EARLYSUSPEND
                 nEnableSuspendOld = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->nEnableSuspend;
                 (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->nEnableSuspend = 2;
                 (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->nEnableSuspend = nEnableSuspendOld;
#endif
                 break;
              case  13://resume from suspend
#ifdef CONFIG_HAS_EARLYSUSPEND
#endif
                 break;
#endif
              case  14://reset wlan (power down/power up)
                 break;
              default:
                 hddLog(LOGE, "Invalid arg  %d in WE_SET_POWER IOCTL", set_value);
                 ret = -EINVAL;
                 break;
           }
           break;
        }

        case WE_SET_MAX_ASSOC:
        {
            if ((WNI_CFG_ASSOC_STA_LIMIT_STAMIN > set_value) ||
                (WNI_CFG_ASSOC_STA_LIMIT_STAMAX < set_value))
            {
                ret = -EINVAL;
            }
            else if ( ccmCfgSetInt(hHal, WNI_CFG_ASSOC_STA_LIMIT,
                                   set_value, NULL, eANI_BOOLEAN_FALSE)
                      != eHAL_STATUS_SUCCESS )
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("failed to set ini parameter, WNI_CFG_ASSOC_STA_LIMIT"));
                ret = -EIO;
            }
            break;
        }

        case WE_SET_SAP_AUTO_CHANNEL_SELECTION:
        {
            if (set_value == 0 || set_value == 1)
            {
#ifdef WLAN_FEATURE_MBSSID
                pAdapter->sap_dyn_ini_cfg.apAutoChannelSelection = set_value;
#else
                (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->apAutoChannelSelection
                                                                 = set_value;
#endif
            }
            else
            {
                 hddLog(LOGE,
                    "Invalid arg %d in WE_SET_SAP_AUTO_CHANNEL_SELECTION IOCTL",
                    set_value);
                 ret = -EINVAL;
            }
            break;
         }

        case  WE_SET_DATA_INACTIVITY_TO:
        {
           if  ((set_value < CFG_DATA_INACTIVITY_TIMEOUT_MIN) ||
                (set_value > CFG_DATA_INACTIVITY_TIMEOUT_MAX) ||
                (ccmCfgSetInt((WLAN_HDD_GET_CTX(pAdapter))->hHal,
                    WNI_CFG_PS_DATA_INACTIVITY_TIMEOUT,
                    set_value,
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE))
           {
               hddLog(LOGE,"Failure: Could not pass on "
                "WNI_CFG_PS_DATA_INACTIVITY_TIMEOUT configuration info "
                "to CCM");
               ret = -EINVAL;
           }
           break;
        }
        case WE_SET_MC_RATE:
        {
            ret = wlan_hdd_set_mc_rate(pAdapter, set_value);
            break;
        }
        case WE_SET_TX_POWER:
        {
           tSirMacAddr bssid;

           vos_mem_copy(bssid, pHddStaCtx->conn_info.bssId, VOS_MAC_ADDR_SIZE);
           if ( sme_SetTxPower(hHal, pAdapter->sessionId, bssid,
                              pAdapter->device_mode, set_value) !=
                              eHAL_STATUS_SUCCESS )
           {
              hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Setting tx power failed",
                     __func__);
              return -EIO;
           }
           break;
        }
        case WE_SET_MAX_TX_POWER:
        {
           tSirMacAddr bssid;
           tSirMacAddr selfMac;

           hddLog(VOS_TRACE_LEVEL_INFO, "%s: Setting maximum tx power %d dBm",
                  __func__, set_value);
           vos_mem_copy(bssid, pHddStaCtx->conn_info.bssId,
                 VOS_MAC_ADDR_SIZE);
           vos_mem_copy(selfMac, pHddStaCtx->conn_info.bssId,
                 VOS_MAC_ADDR_SIZE);

           if( sme_SetMaxTxPower(hHal, bssid, selfMac, set_value) !=
               eHAL_STATUS_SUCCESS )
           {
              hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Setting maximum tx power failed",
              __func__);
              return -EIO;
           }

           break;
        }
        case WE_SET_MAX_TX_POWER_2_4:
        {
           hddLog(VOS_TRACE_LEVEL_INFO,
                  "%s: Setting maximum tx power %d dBm for 2.4 GHz band",
                  __func__, set_value);
           if (sme_SetMaxTxPowerPerBand(eCSR_BAND_24, set_value) !=
                                        eHAL_STATUS_SUCCESS)
           {
              hddLog(VOS_TRACE_LEVEL_ERROR,
                     "%s: Setting maximum tx power failed for 2.4 GHz band",
                     __func__);
              return -EIO;
           }

           break;
        }
        case WE_SET_MAX_TX_POWER_5_0:
        {
           hddLog(VOS_TRACE_LEVEL_INFO,
                  "%s: Setting maximum tx power %d dBm for 5.0 GHz band",
                  __func__, set_value);
           if (sme_SetMaxTxPowerPerBand(eCSR_BAND_5G, set_value) !=
                                        eHAL_STATUS_SUCCESS)
           {
              hddLog(VOS_TRACE_LEVEL_ERROR,
                     "%s: Setting maximum tx power failed for 5.0 GHz band",
                     __func__);
              return -EIO;
           }

           break;
        }
        case WE_SET_HIGHER_DTIM_TRANSITION:
        {
            if(!((set_value == eANI_BOOLEAN_FALSE) ||
                          (set_value == eANI_BOOLEAN_TRUE)))
            {
                hddLog(LOGE, "Dynamic DTIM Incorrect data:%d", set_value);
                ret = -EINVAL;
            }
            else
            {
                if(pAdapter->higherDtimTransition != set_value)
                {
                    pAdapter->higherDtimTransition = set_value;
                    hddLog(LOG1, "%s: higherDtimTransition set to :%d", __func__, pAdapter->higherDtimTransition);
                }
            }

           break;
        }

        case WE_SET_TM_LEVEL:
        {
           hddLog(VOS_TRACE_LEVEL_INFO, "Set Thermal Mitigation Level %d",
                  set_value);
           (void)sme_SetThermalLevel(hHal, set_value);
           break;
        }

        case WE_SET_PHYMODE:
        {
           hdd_context_t *phddctx = WLAN_HDD_GET_CTX(pAdapter);

           ret = wlan_hdd_update_phymode(dev, hHal, set_value, phddctx);
           break;
        }

        case WE_SET_NSS:
        {
           hddLog(LOG1, "Set NSS = %d", set_value);
           if ((set_value > 2) || (set_value <= 0)) {
               hddLog(LOGE, "NSS greater than 2 not supported");
               ret = -EINVAL;
           } else {
               if (VOS_STATUS_SUCCESS !=
                     hdd_update_nss(WLAN_HDD_GET_CTX(pAdapter), set_value))
                   ret = -EINVAL;
           }
           break;
        }

        case WE_SET_GTX_HT_MCS:
        {
           hddLog(LOG1, "WMI_VDEV_PARAM_GTX_HT_MCS %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_HT_MCS,
                                         set_value, GTX_CMD);
           break;
        }

        case WE_SET_GTX_VHT_MCS:
        {
           hddLog(LOG1, "WMI_VDEV_PARAM_GTX_VHT_MCS %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_VHT_MCS,
                                         set_value, GTX_CMD);
           break;
        }

       case WE_SET_GTX_USRCFG:
        {
           hddLog(LOG1, "WMI_VDEV_PARAM_GTX_USR_CFG %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_USR_CFG,
                                         set_value, GTX_CMD);
           break;
        }

        case WE_SET_GTX_THRE:
        {
           hddLog(LOG1, "WMI_VDEV_PARAM_GTX_THRE %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_THRE,
                                         set_value, GTX_CMD);
           break;
        }

        case WE_SET_GTX_MARGIN:
        {
           hddLog(LOG1, "WMI_VDEV_PARAM_GTX_MARGIN %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_MARGIN,
                                         set_value, GTX_CMD);
           break;
        }

        case WE_SET_GTX_STEP:
        {
           hddLog(LOG1, "WMI_VDEV_PARAM_GTX_STEP %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_STEP,
                                         set_value, GTX_CMD);
           break;
        }

        case WE_SET_GTX_MINTPC:
        {
           hddLog(LOG1, "WMI_VDEV_PARAM_GTX_MINTPC %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_MINTPC,
                                         set_value, GTX_CMD);
           break;
        }

        case WE_SET_GTX_BWMASK:
        {
           hddLog(LOG1, "WMI_VDEV_PARAM_GTX_BWMASK %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_GTX_BW_MASK,
                                         set_value, GTX_CMD);
           break;
        }

        case WE_SET_LDPC:
           ret = hdd_set_ldpc(pAdapter, set_value);
           break;

        case WE_SET_TX_STBC:
           ret = hdd_set_tx_stbc(pAdapter, set_value);
           break;

        case WE_SET_RX_STBC:
           ret = hdd_set_rx_stbc(pAdapter, set_value);
           break;

        case WE_SET_SHORT_GI:
        {
           hddLog(LOG1, "WMI_VDEV_PARAM_SGI val %d", set_value);
           ret = sme_UpdateHTConfig(hHal, pAdapter->sessionId,
                                   WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ,
                                   set_value);
           if (ret)
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "Failed to set ShortGI value");
           break;
        }

        case WE_SET_RTSCTS:
        {
           u_int32_t value;

           hddLog(LOG1, "WMI_VDEV_PARAM_ENABLE_RTSCTS val 0x%x", set_value);

           if ((set_value & HDD_RTSCTS_EN_MASK) == HDD_RTSCTS_ENABLE)
               value = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->RTSThreshold;
           else if (((set_value & HDD_RTSCTS_EN_MASK) == 0) ||
               ((set_value & HDD_RTSCTS_EN_MASK) == HDD_CTS_ENABLE))
               value = WNI_CFG_RTS_THRESHOLD_STAMAX;
           else
               return -EIO;

           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_ENABLE_RTSCTS,
                                         set_value, VDEV_CMD);
           if (!ret) {
               if (ccmCfgSetInt(hHal, WNI_CFG_RTS_THRESHOLD, value,
                   ccmCfgSetCallback, eANI_BOOLEAN_TRUE) !=
                                                         eHAL_STATUS_SUCCESS) {
                   hddLog(LOGE, "FAILED TO SET RTSCTS");
                   return -EIO;
               }
           }

           break;
        }

        case WE_SET_CHWIDTH:
        {
           bool chwidth = false;
           hdd_context_t *phddctx = WLAN_HDD_GET_CTX(pAdapter);
           /*updating channel bonding only on 5Ghz*/
           hddLog(LOG1, "WMI_VDEV_PARAM_CHWIDTH val %d", set_value);
           if (set_value > eHT_CHANNEL_WIDTH_80MHZ) {
                hddLog(LOGE, "Invalid channel width 0->20 1->40 2->80");
                return -EINVAL;
           }

           if ((WNI_CFG_CHANNEL_BONDING_MODE_DISABLE !=
               csrConvertCBIniValueToPhyCBState(
                       phddctx->cfg_ini->nChannelBondingMode5GHz)))
               chwidth = true;

           sme_GetConfigParam(hHal, &smeConfig);
           switch (set_value) {
           case eHT_CHANNEL_WIDTH_20MHZ:
                smeConfig.csrConfig.channelBondingMode5GHz =
                                          WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
                break;
           case eHT_CHANNEL_WIDTH_40MHZ:
                if (chwidth)
                    smeConfig.csrConfig.channelBondingMode5GHz =
                                     phddctx->cfg_ini->nChannelBondingMode5GHz;
                else
                    return -EINVAL;

                break;
#ifdef WLAN_FEATURE_11AC
           case eHT_CHANNEL_WIDTH_80MHZ:
                if (chwidth)
                    smeConfig.csrConfig.channelBondingMode5GHz =
                                     phddctx->cfg_ini->nChannelBondingMode5GHz;
                else
                    return -EINVAL;

                break;
#endif

           default:
                return -EINVAL;
           }

           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_CHWIDTH,
                                         set_value, VDEV_CMD);
           if (!ret)
               sme_UpdateConfig(hHal, &smeConfig);

           break;
        }

        case WE_SET_ANI_EN_DIS:
        {
           hddLog(LOG1, "WMI_PDEV_PARAM_ANI_ENABLE val %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_ANI_ENABLE,
                                         set_value, PDEV_CMD);
           break;
        }

        case WE_SET_ANI_POLL_PERIOD:
        {
           hddLog(LOG1, "WMI_PDEV_PARAM_ANI_POLL_PERIOD val %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_ANI_POLL_PERIOD,
                                         set_value, PDEV_CMD);
           break;
        }

        case WE_SET_ANI_LISTEN_PERIOD:
        {
           hddLog(LOG1, "WMI_PDEV_PARAM_ANI_LISTEN_PERIOD val %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_ANI_LISTEN_PERIOD,
                                         set_value, PDEV_CMD);
           break;
        }

        case WE_SET_ANI_OFDM_LEVEL:
        {
           hddLog(LOG1, "WMI_PDEV_PARAM_ANI_OFDM_LEVEL val %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_ANI_OFDM_LEVEL,
                                         set_value, PDEV_CMD);
           break;
        }

        case WE_SET_ANI_CCK_LEVEL:
        {
           hddLog(LOG1, "WMI_PDEV_PARAM_ANI_CCK_LEVEL val %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_ANI_CCK_LEVEL,
                                         set_value, PDEV_CMD);
           break;
        }

        case WE_SET_DYNAMIC_BW:
        {
           hddLog(LOG1, "WMI_PDEV_PARAM_DYNAMIC_BW val %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_DYNAMIC_BW,
                                         set_value, PDEV_CMD);
           break;
        }

        case WE_SET_11N_RATE:
        {
           u_int8_t preamble = 0, nss = 0, rix = 0;
           hddLog(LOG1, "WMI_VDEV_PARAM_FIXED_RATE val %d", set_value);

           if (set_value != 0xff) {
               rix = RC_2_RATE_IDX(set_value);
               if (set_value & 0x80) {
                   preamble = WMI_RATE_PREAMBLE_HT;
                   nss = HT_RC_2_STREAMS(set_value) -1;
               } else {
                   nss = 0;
                   rix = RC_2_RATE_IDX(set_value);
                   if (set_value & 0x10) {
                       preamble = WMI_RATE_PREAMBLE_CCK;
                       /* Enable Short preamble always for CCK except 1mbps */
                       if(rix != 0x3)
                           rix |= 0x4;
                   } else
                       preamble = WMI_RATE_PREAMBLE_OFDM;
               }
               set_value = (preamble << 6) | (nss << 4) | rix;
           }
           hddLog(LOG1, "WMI_VDEV_PARAM_FIXED_RATE val %d rix %d preamble %x\
                  nss %d", set_value, rix, preamble, nss);

           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_FIXED_RATE,
                                         set_value, VDEV_CMD);
           break;
         }

        case WE_SET_VHT_RATE:
        {
           u_int8_t preamble = 0, nss = 0, rix = 0;

           if (set_value != 0xff) {
               rix = RC_2_RATE_IDX_11AC(set_value);
               preamble = WMI_RATE_PREAMBLE_VHT;
               nss = HT_RC_2_STREAMS_11AC(set_value) -1;

               set_value = (preamble << 6) | (nss << 4) | rix;
           }
           hddLog(LOG1, "WMI_VDEV_PARAM_FIXED_RATE val %d rix %d preamble %x\
                  nss %d", set_value, rix, preamble, nss);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_FIXED_RATE,
                                         set_value, VDEV_CMD);
           break;
         }

         case WE_SET_AMPDU:
         {
            hddLog(LOG1, "SET AMPDU val %d", set_value);
            ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)GEN_VDEV_PARAM_AMPDU,
                                          set_value, GEN_CMD);
            break;
         }

         case WE_SET_AMSDU:
         {
            hddLog(LOG1, "SET AMSDU val %d", set_value);
            ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)GEN_VDEV_PARAM_AMSDU,
                                          set_value, GEN_CMD);
            break;
         }

         case WE_SET_BURST_ENABLE:
         {
            hddLog(LOG1, "SET Burst enable val %d", set_value);
            if ((set_value == 0) || (set_value == 1)) {
                ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)WMI_PDEV_PARAM_BURST_ENABLE,
                                          set_value, PDEV_CMD);
            }
            else
                ret = -EINVAL;
            break;
         }
         case WE_SET_BURST_DUR:
         {
            hddLog(LOG1, "SET Burst duration val %d", set_value);
            if ((set_value > 0) && (set_value <= 8192)) {
                ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)WMI_PDEV_PARAM_BURST_DUR,
                                          set_value, PDEV_CMD);
            }
            else
                ret = -EINVAL;
            break;
         }

         case WE_SET_TX_CHAINMASK:
         {
            hddLog(LOG1, "WMI_PDEV_PARAM_TX_CHAIN_MASK val %d", set_value);
            ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)WMI_PDEV_PARAM_TX_CHAIN_MASK,
                                          set_value, PDEV_CMD);
            break;
         }

         case WE_SET_RX_CHAINMASK:
         {
             hddLog(LOG1, "WMI_PDEV_PARAM_RX_CHAIN_MASK val %d", set_value);
             ret = process_wma_set_command((int)pAdapter->sessionId,
                                           (int)WMI_PDEV_PARAM_RX_CHAIN_MASK,
                                           set_value, PDEV_CMD);
             break;
         }

         case WE_SET_TXPOW_2G:
         {
             hddLog(LOG1, "WMI_PDEV_PARAM_TXPOWER_LIMIT2G val %d", set_value);
             ret = process_wma_set_command((int)pAdapter->sessionId,
                                           (int)WMI_PDEV_PARAM_TXPOWER_LIMIT2G,
                                           set_value, PDEV_CMD);
             break;
         }

         case WE_SET_TXPOW_5G:
         {
             hddLog(LOG1, "WMI_PDEV_PARAM_TXPOWER_LIMIT5G val %d", set_value);
             ret = process_wma_set_command((int)pAdapter->sessionId,
                                           (int)WMI_PDEV_PARAM_TXPOWER_LIMIT5G,
                                           set_value, PDEV_CMD);
             break;
         }

         case WE_SET_POWER_GATING:
         {
              hddLog(LOG1, "WMI_PDEV_PARAM_POWER_GATING_SLEEP val %d",
                     set_value);
              ret = process_wma_set_command((int)pAdapter->sessionId,
                                        (int)WMI_PDEV_PARAM_POWER_GATING_SLEEP,
                                        (set_value)? true:false, PDEV_CMD);
              break;
         }

         /* Firmware debug log */
         case WE_DBGLOG_LOG_LEVEL:
         {
              hddLog(LOG1, "WE_DBGLOG_LOG_LEVEL val %d", set_value);
              pHddCtx->fw_log_settings.dl_loglevel = set_value;
              ret = process_wma_set_command((int)pAdapter->sessionId,
                                           (int)WMI_DBGLOG_LOG_LEVEL,
                                           set_value, DBG_CMD);
              break;
         }

         case WE_DBGLOG_VAP_ENABLE:
         {
              hddLog(LOG1, "WE_DBGLOG_VAP_ENABLE val %d", set_value);
              ret = process_wma_set_command((int)pAdapter->sessionId,
                                           (int)WMI_DBGLOG_VAP_ENABLE,
                                           set_value, DBG_CMD);
              break;
         }

         case WE_DBGLOG_VAP_DISABLE:
         {
              hddLog(LOG1, "WE_DBGLOG_VAP_DISABLE val %d", set_value);
              ret = process_wma_set_command((int)pAdapter->sessionId,
                                           (int)WMI_DBGLOG_VAP_DISABLE,
                                           set_value, DBG_CMD);
              break;
         }

         case WE_DBGLOG_MODULE_ENABLE:
         {
              hddLog(LOG1, "WE_DBGLOG_MODULE_ENABLE val %d", set_value);
              pHddCtx->fw_log_settings.enable = set_value;
              ret = process_wma_set_command((int)pAdapter->sessionId,
                                           (int)WMI_DBGLOG_MODULE_ENABLE,
                                           set_value, DBG_CMD);
              break;
         }

         case WE_DBGLOG_MODULE_DISABLE:
         {
              hddLog(LOG1, "WE_DBGLOG_MODULE_DISABLE val %d", set_value);
              pHddCtx->fw_log_settings.enable = set_value;
              ret = process_wma_set_command((int)pAdapter->sessionId,
                                           (int)WMI_DBGLOG_MODULE_DISABLE,
                                           set_value, DBG_CMD);
              break;
         }
         case WE_DBGLOG_MOD_LOG_LEVEL:
         {
              hddLog(LOG1, "WE_DBGLOG_MOD_LOG_LEVEL val %d", set_value);

              if (pHddCtx->fw_log_settings.index >= MAX_MOD_LOGLEVEL) {
                 pHddCtx->fw_log_settings.index = 0;
              }

              pHddCtx->fw_log_settings.dl_mod_loglevel[pHddCtx->
                                        fw_log_settings.index] = set_value;
              pHddCtx->fw_log_settings.index++;

              ret = process_wma_set_command((int)pAdapter->sessionId,
                                           (int)WMI_DBGLOG_MOD_LOG_LEVEL,
                                            set_value, DBG_CMD);
              break;
         }

         case WE_DBGLOG_TYPE:
         {
              hddLog(LOG1, "WE_DBGLOG_TYPE val %d", set_value);
              pHddCtx->fw_log_settings.dl_type = set_value;
              ret = process_wma_set_command((int)pAdapter->sessionId,
                                           (int)WMI_DBGLOG_TYPE,
                                           set_value, DBG_CMD);
              break;
         }
        case WE_DBGLOG_REPORT_ENABLE:
        {
             hddLog(LOG1, "WE_DBGLOG_REPORT_ENABLE val %d", set_value);
             pHddCtx->fw_log_settings.dl_report = set_value;
             ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)WMI_DBGLOG_REPORT_ENABLE,
                                          set_value, DBG_CMD);
             break;
        }

	case WE_SET_TXRX_FWSTATS:
	{
           hddLog(LOG1, "WE_SET_TXRX_FWSTATS val %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
			   (int)WMA_VDEV_TXRX_FWSTATS_ENABLE_CMDID,
			   set_value, VDEV_CMD);
	   break;
	}

	case WE_TXRX_FWSTATS_RESET:
	{
           hddLog(LOG1, "WE_TXRX_FWSTATS_RESET val %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
			   (int)WMA_VDEV_TXRX_FWSTATS_RESET_CMDID,
			   set_value, VDEV_CMD);
	   break;
	}

	case WE_PPS_PAID_MATCH:
        {
           if(pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
              return EINVAL;

           hddLog(LOG1, "WMI_VDEV_PPS_PAID_MATCH val %d ", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                           (int)WMI_VDEV_PPS_PAID_MATCH,
                           set_value, PPS_CMD);
           break;
        }

	case WE_PPS_GID_MATCH:
	{
	   if(pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
		return EINVAL;
	   hddLog(LOG1, "WMI_VDEV_PPS_GID_MATCH val %d ", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                           (int)WMI_VDEV_PPS_GID_MATCH,
                           set_value, PPS_CMD);
           break;
        }

	case WE_PPS_EARLY_TIM_CLEAR:
	{
           if(pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
               return EINVAL;
           hddLog(LOG1, " WMI_VDEV_PPS_EARLY_TIM_CLEAR val %d ", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                           (int)WMI_VDEV_PPS_EARLY_TIM_CLEAR,
                           set_value, PPS_CMD);
           break;
        }

	case WE_PPS_EARLY_DTIM_CLEAR:
        {
           if(pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
              return EINVAL;
           hddLog(LOG1, "WMI_VDEV_PPS_EARLY_DTIM_CLEAR val %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                           (int)WMI_VDEV_PPS_EARLY_DTIM_CLEAR,
                           set_value, PPS_CMD);
           break;
        }

	case WE_PPS_EOF_PAD_DELIM:
        {
           if(pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
               return EINVAL;
           hddLog(LOG1, "WMI_VDEV_PPS_EOF_PAD_DELIM val %d ", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                           (int)WMI_VDEV_PPS_EOF_PAD_DELIM,
                           set_value, PPS_CMD);
           break;
        }

	case WE_PPS_MACADDR_MISMATCH:
        {
           if(pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
              return EINVAL;
           hddLog(LOG1, "WMI_VDEV_PPS_MACADDR_MISMATCH val %d ", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                           (int)WMI_VDEV_PPS_MACADDR_MISMATCH,
                           set_value, PPS_CMD);
           break;
        }

	case WE_PPS_DELIM_CRC_FAIL:
        {
           if(pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
              return EINVAL;
           hddLog(LOG1, "WMI_VDEV_PPS_DELIM_CRC_FAIL val %d ", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                           (int)WMI_VDEV_PPS_DELIM_CRC_FAIL,
                           set_value, PPS_CMD);
           break;
        }


	case WE_PPS_GID_NSTS_ZERO:
        {
           if(pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
               return EINVAL;
           hddLog(LOG1, "WMI_VDEV_PPS_GID_NSTS_ZERO val %d ", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                           (int)WMI_VDEV_PPS_GID_NSTS_ZERO,
                           set_value, PPS_CMD);
           break;
        }


	case WE_PPS_RSSI_CHECK:
        {
           if(pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
               return EINVAL;
           hddLog(LOG1, "WMI_VDEV_PPS_RSSI_CHECK val %d ", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                           (int)WMI_VDEV_PPS_RSSI_CHECK,
                           set_value, PPS_CMD);
           break;
        }

        case WE_PPS_5G_EBT:
        {
           if (pAdapter->device_mode != WLAN_HDD_INFRA_STATION)
              return -EINVAL;

           hddLog(LOG1, "WMI_VDEV_PPS_5G_EBT val %d", set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                           (int)WMI_VDEV_PPS_5G_EBT,
                           set_value, PPS_CMD);
           break;
        }

        case WE_SET_HTSMPS:
        {
            hddLog(LOG1, "WE_SET_HTSMPS val %d", set_value);
            ret = process_wma_set_command((int)pAdapter->sessionId,
                            (int)WMI_STA_SMPS_FORCE_MODE_CMDID,
                             set_value, VDEV_CMD);
            break;
        }


        case WE_SET_QPOWER_MAX_PSPOLL_COUNT:
        {
               hddLog(LOG1, "WE_SET_QPOWER_MAX_PSPOLL_COUNT val %d",
                      set_value);
	       ret = process_wma_set_command((int)pAdapter->sessionId,
                                 (int)WMI_STA_PS_PARAM_QPOWER_PSPOLL_COUNT,
                                 set_value, QPOWER_CMD);
               break;
        }

        case WE_SET_QPOWER_MAX_TX_BEFORE_WAKE:
        {
           hddLog(LOG1, "WE_SET_QPOWER_MAX_TX_BEFORE_WAKE val %d",
                  set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                         (int)WMI_STA_PS_PARAM_QPOWER_MAX_TX_BEFORE_WAKE,
                         set_value, QPOWER_CMD);
           break;
        }

        case WE_SET_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL:
        {
           hddLog(LOG1, "WE_SET_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL val %d",
                  set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                        (int)WMI_STA_PS_PARAM_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL,
                        set_value, QPOWER_CMD);
           break;
        }

        case WE_SET_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL:
        {
           hddLog(LOG1, "WE_SET_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL val %d",
                  set_value);
           ret = process_wma_set_command((int)pAdapter->sessionId,
                      (int)WMI_STA_PS_PARAM_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL,
                      set_value, QPOWER_CMD);
           break;
        }
        case WE_SET_SCAN_BAND_PREFERENCE:
        {
           if(pAdapter->device_mode != WLAN_HDD_INFRA_STATION) {
               ret = -EINVAL;
               break;
           }
           hddLog(LOG1, "WE_SET_BAND_PREFERRENCE val %d ", set_value);

           if (eCSR_BAND_ALL == set_value ||
                  eCSR_BAND_24 == set_value || eCSR_BAND_5G == set_value) {
               sme_GetConfigParam(hHal, &smeConfig);
               smeConfig.csrConfig.scanBandPreference = set_value;

               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                         "set band scan preference = %d",
                         smeConfig.csrConfig.scanBandPreference);

               sme_UpdateConfig(hHal, &smeConfig);
           }
           else {
               ret = -EINVAL;
           }
           break;

        }

        case WE_MCC_CONFIG_LATENCY:
        {
            tVOS_CONCURRENCY_MODE concurrent_state = 0;
            v_U8_t first_adapter_operating_channel = 0;
            int ret = 0; /* success */

            hddLog(LOG1, "iwpriv cmd to set MCC latency with val %dms",
                    set_value);
            /**
             * Check if concurrency mode is active.
             * Need to modify this code to support MCC modes other than STA/P2P
             */
            concurrent_state = hdd_get_concurrency_mode();
            if ((concurrent_state == (VOS_STA | VOS_P2P_CLIENT)) ||
                (concurrent_state == (VOS_STA | VOS_P2P_GO)))
            {
                hddLog(LOG1, "STA & P2P are both enabled");
                /**
                 * The channel number and latency are formatted in
                 * a bit vector then passed on to WMA layer.
                 +**********************************************+
                 |bits 31-16 |      bits 15-8    |  bits 7-0    |
                 |  Unused   | latency - Chan. 1 |  channel no. |
                 +**********************************************+
                 */
                /* Get the operating channel of the designated vdev */
                first_adapter_operating_channel =
                            hdd_get_operating_channel
                            (
                                pAdapter->pHddCtx,
                                pAdapter->device_mode
                            );
                /* Move the time latency for the adapter to bits 15-8 */
                set_value = set_value << 8;
                /* Store the channel number at bits 7-0 of the bit vector */
                set_value = set_value | first_adapter_operating_channel;
                /* Send command to WMA */
                ret = process_wma_set_command((int)pAdapter->sessionId,
                        (int)WMA_VDEV_MCC_SET_TIME_LATENCY,
                        set_value, VDEV_CMD);
            }
            else
            {
                hddLog(LOG1, "%s: MCC is not active. Exit w/o setting latency",
                       __func__);
            }
            break;
        }

        case WE_MCC_CONFIG_QUOTA:
        {
            v_U8_t first_adapter_operating_channel = 0;
            v_U8_t second_adapter_opertaing_channel = 0;
            hdd_adapter_t *staAdapter = NULL;
            int ret = 0; /* success */

            tVOS_CONCURRENCY_MODE concurrent_state = hdd_get_concurrency_mode();
            hddLog(LOG1, "iwpriv cmd to set MCC quota with val %dms",
                set_value);
            /**
              * Check if concurrency mode is active.
              * Need to modify this code to support MCC modes other than STA/P2P
              */
            if ((concurrent_state == (VOS_STA | VOS_P2P_CLIENT)) ||
                (concurrent_state == (VOS_STA | VOS_P2P_GO)))
            {
                hddLog(LOG1, "STA & P2P are both enabled");
                /**
                 * The channel numbers for both adapters and the time
                 * quota for the 1st adapter, i.e., one specified in cmd
                 * are formatted as a bit vector then passed on to WMA
                 +************************************************************+
                 |bit 31-24  | bit 23-16  |   bits 15-8   |   bits 7-0        |
                 |  Unused   | Quota for  | chan. # for   |   chan. # for     |
                 |           | 1st chan.  | 1st chan.     |   2nd chan.       |
                 +************************************************************+
                 */
                /* Get the operating channel of the specified vdev */
                first_adapter_operating_channel =
                            hdd_get_operating_channel
                            (
                                pAdapter->pHddCtx,
                                pAdapter->device_mode
                            );
                hddLog(LOG1, "1st channel No.:%d and quota:%dms",
                        first_adapter_operating_channel, set_value);
                /* Move the time quota for first channel to bits 15-8 */
                set_value = set_value << 8;
                /** Store the channel number of 1st channel at bits 7-0
                 * of the bit vector
                 */
                set_value = set_value | first_adapter_operating_channel;
                /* Find out the 2nd MCC adapter and its operating channel */
                if (pAdapter->device_mode == WLAN_HDD_INFRA_STATION)
                {
                    /* iwpriv cmd was issued on wlan0; get p2p0 vdev channel */
                    if ((concurrent_state & VOS_P2P_CLIENT) != 0)
                    {
                        /* The 2nd MCC vdev is P2P client */
                        staAdapter = hdd_get_adapter(pAdapter->pHddCtx,
                                        WLAN_HDD_P2P_CLIENT);
                    } else
                    {
                        /* The 2nd MCC vdev is P2P GO */
                        staAdapter = hdd_get_adapter(pAdapter->pHddCtx,
                                        WLAN_HDD_P2P_GO);
                    }
                }
                else
                {
                    /* iwpriv cmd was issued on p2p0; get wlan0 vdev channel */
                    staAdapter = hdd_get_adapter(pAdapter->pHddCtx,
                                     WLAN_HDD_INFRA_STATION);
                }
                if (staAdapter != NULL)
                {
                    second_adapter_opertaing_channel =
                            hdd_get_operating_channel
                            (
                            staAdapter->pHddCtx,
                            staAdapter->device_mode
                            );
                    hddLog(LOG1, "2nd vdev channel No. is:%d",
                            second_adapter_opertaing_channel);
                    /** Now move the time quota and channel number of the
                     * 1st adapter to bits 23-16 and bits 15-8 of the bit
                     * vector, respectively.
                     */
                    set_value = set_value << 8;
                    /* Store the channel number for 2nd MCC vdev at bits
                     * 7-0 of set_value
                     */
                    set_value = set_value | second_adapter_opertaing_channel;
                    ret = process_wma_set_command((int)pAdapter->sessionId,
                            (int)WMA_VDEV_MCC_SET_TIME_QUOTA,
                            set_value, VDEV_CMD);
                }
                else
                {
                    hddLog(LOGE, "NULL adapter handle. Exit");
                }
            }
            else
            {
                hddLog(LOG1, "%s: MCC is not active. Exit w/o setting latency",
                        __func__);
            }
            break;
        }
       case WE_SET_DEBUG_LOG:
       {
           hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
#ifdef QCA_PKT_PROTO_TRACE
           /* Trace buffer dump only */
           if (VOS_PKT_TRAC_DUMP_CMD == set_value)
           {
              vos_pkt_trace_buf_dump();
              break;
           }
#endif /* QCA_PKT_PROTO_TRACE */
           pHddCtx->cfg_ini->gEnableDebugLog = set_value;
           sme_UpdateConnectDebug(pHddCtx->hHal, set_value);
           break;
       }
       case WE_SET_EARLY_RX_ADJUST_ENABLE:
       {
            hddLog(LOG1, "SET early_rx enable val %d", set_value);
            if ((set_value == 0) || (set_value == 1)) {
                 ret = process_wma_set_command((int)pAdapter->sessionId,
                                (int)WMI_VDEV_PARAM_EARLY_RX_ADJUST_ENABLE,
                                set_value, VDEV_CMD);
            }
            else
              ret = -EINVAL;
            break;
       }
       case WE_SET_EARLY_RX_TGT_BMISS_NUM:
       {
            hddLog(LOG1, "SET early_rx bmiss val %d", set_value);
            ret = process_wma_set_command((int)pAdapter->sessionId,
                            (int)WMI_VDEV_PARAM_EARLY_RX_TGT_BMISS_NUM,
                            set_value, VDEV_CMD);
            break;
       }
       case WE_SET_EARLY_RX_BMISS_SAMPLE_CYCLE:
       {
            hddLog(LOG1, "SET early_rx bmiss sample cycle %d", set_value);
            ret = process_wma_set_command((int)pAdapter->sessionId,
                            (int)WMI_VDEV_PARAM_EARLY_RX_BMISS_SAMPLE_CYCLE,
                            set_value, VDEV_CMD);
            break;
       }
       case WE_SET_EARLY_RX_SLOP_STEP:
       {
            hddLog(LOG1, "SET early_rx bmiss slop step val %d", set_value);
            ret = process_wma_set_command((int)pAdapter->sessionId,
                            (int)WMI_VDEV_PARAM_EARLY_RX_SLOP_STEP,
                            set_value, VDEV_CMD);
            break;
       }
       case WE_SET_EARLY_RX_INIT_SLOP:
       {
            hddLog(LOG1, "SET early_rx init slop step val %d", set_value);
            ret = process_wma_set_command((int)pAdapter->sessionId,
                            (int)WMI_VDEV_PARAM_EARLY_RX_INIT_SLOP,
                            set_value, VDEV_CMD);
            break;
       }
       case WE_SET_EARLY_RX_ADJUST_PAUSE:
       {
            hddLog(LOG1, "SET early_rx adjust pause %d", set_value);
            if ((set_value == 0) || (set_value == 1)) {
                 ret = process_wma_set_command((int)pAdapter->sessionId,
                                (int)WMI_VDEV_PARAM_EARLY_RX_ADJUST_PAUSE,
                                set_value, VDEV_CMD);
            }
            else
              ret = -EINVAL;
            break;
       }
       case WE_SET_EARLY_RX_DRIFT_SAMPLE:
       {
            hddLog(LOG1, "SET early_rx drift sample %d", set_value);
            ret = process_wma_set_command((int)pAdapter->sessionId,
                            (int)WMI_VDEV_PARAM_EARLY_RX_DRIFT_SAMPLE,
                            set_value, VDEV_CMD);
            break;
       }
        default:
        {
           hddLog(LOGE, "%s: Invalid sub command %d", __func__, sub_cmd);
           ret = -EINVAL;
           break;
        }
    }
    return ret;
}

/* set param sub-ioctls */
static int iw_setchar_getnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    VOS_STATUS vstatus;
    int sub_cmd;
    int ret = 0; /* success */
    char *pBuffer = NULL;
    hdd_adapter_t *pAdapter = (netdev_priv(dev));
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
#ifdef WLAN_FEATURE_VOWIFI
    hdd_config_t  *pConfig = pHddCtx->cfg_ini;
#endif /* WLAN_FEATURE_VOWIFI */
    struct iw_point s_priv_data;

    if (!capable(CAP_NET_ADMIN))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("permission check failed"));
        return -EPERM;
    }

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EBUSY;
    }

    if (!capable(CAP_NET_ADMIN)){
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("permission check failed"));
        return -EPERM;
    }

    /* helper function to get iwreq_data with compat handling. */
    if (hdd_priv_get_data(&s_priv_data, wrqu)) {
       return -EINVAL;
    }

    /* make sure all params are correctly passed to function */
    if ((NULL == s_priv_data.pointer) || (0 == s_priv_data.length)) {
       return -EINVAL;
    }

    sub_cmd = s_priv_data.flags;

    /* ODD number is used for set, copy data using copy_from_user */
    pBuffer = mem_alloc_copy_from_user_helper(s_priv_data.pointer,
                                              s_priv_data.length);
    if (NULL == pBuffer)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "mem_alloc_copy_from_user_helper fail");
        return -ENOMEM;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s: Received length %d", __func__, s_priv_data.length);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s: Received data %s", __func__, pBuffer);

    switch(sub_cmd)
    {
       case WE_WOWL_ADD_PTRN:
          VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "ADD_PTRN");
          hdd_add_wowl_ptrn(pAdapter, pBuffer);
          break;
       case WE_WOWL_DEL_PTRN:
          VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "DEL_PTRN");
          hdd_del_wowl_ptrn(pAdapter, pBuffer);
          break;
#if defined WLAN_FEATURE_VOWIFI
       case WE_NEIGHBOR_REPORT_REQUEST:
          {
             tRrmNeighborReq neighborReq;
             tRrmNeighborRspCallbackInfo callbackInfo;

             if (pConfig->fRrmEnable)
             {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "Neighbor Request");
                neighborReq.no_ssid = (s_priv_data.length - 1) ? false : true ;
                if( !neighborReq.no_ssid )
                {
                   neighborReq.ssid.length = (s_priv_data.length - 1) > 32 ?
                      32 : (s_priv_data.length - 1);
                   vos_mem_copy(neighborReq.ssid.ssId, pBuffer,
                                 neighborReq.ssid.length);
                }

                callbackInfo.neighborRspCallback = NULL;
                callbackInfo.neighborRspCallbackContext = NULL;
                callbackInfo.timeout = 5000;   //5 seconds
                sme_NeighborReportRequest(  WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId, &neighborReq, &callbackInfo );
             }
             else
             {
                hddLog(LOGE, "%s: Ignoring neighbor request as RRM is not enabled", __func__);
                ret = -EINVAL;
             }
          }
          break;
#endif
       case WE_SET_AP_WPS_IE:
          hddLog( LOGE, "Received WE_SET_AP_WPS_IE" );
          sme_updateP2pIe(WLAN_HDD_GET_HAL_CTX(pAdapter), pBuffer,
                           s_priv_data.length);
          break;
       case WE_SET_CONFIG:
          vstatus = hdd_execute_global_config_command(pHddCtx, pBuffer);
#ifdef WLAN_FEATURE_MBSSID
          if (vstatus == VOS_STATUS_E_PERM) {
              vstatus = hdd_execute_sap_dyn_config_command(pAdapter, pBuffer);
              if (vstatus == VOS_STATUS_SUCCESS)
                  VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                             "%s: Stored in Dynamic SAP ini config", __func__);

          }
#endif
          if (VOS_STATUS_SUCCESS != vstatus)
          {
             ret = -EINVAL;
          }
          break;
       default:
       {
           hddLog(LOGE, "%s: Invalid sub command %d", __func__, sub_cmd);
           ret = -EINVAL;
           break;
       }
    }
    kfree(pBuffer);
    return ret;
}

/* get param sub-ioctls */
static int iw_setnone_getint(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    int *value = (int *)extra;
    int ret = 0; /* success */
    hdd_context_t *wmahddCtxt = WLAN_HDD_GET_CTX(pAdapter);
    void *wmapvosContext = wmahddCtxt->pvosContext;
    tSmeConfigParams smeConfig;

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EBUSY;
    }

    switch (value[0])
    {
        case WE_GET_11D_STATE:
        {
           sme_GetConfigParam(hHal,&smeConfig);

           *value = smeConfig.csrConfig.Is11dSupportEnabled;

            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, ("11D state=%d!!"),*value);

           break;
        }

        case WE_IBSS_STATUS:
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "****Return IBSS Status*****");
           break;

        case WE_GET_WLAN_DBG:
        {
           vos_trace_display();
           *value = 0;
           break;
        }
        case WE_GET_MAX_ASSOC:
        {
            if (ccmCfgGetInt(hHal, WNI_CFG_ASSOC_STA_LIMIT, (tANI_U32 *)value) != eHAL_STATUS_SUCCESS)
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                      FL("failed to get ini parameter, WNI_CFG_ASSOC_STA_LIMIT"));
                ret = -EIO;
            }
            break;
        }

        case WE_GET_SAP_AUTO_CHANNEL_SELECTION:
        {
#ifdef WLAN_FEATURE_MBSSID
            *value = pAdapter->sap_dyn_ini_cfg.apAutoChannelSelection;
#else
            *value = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->apAutoChannelSelection;
#endif
            break;
        }
        case WE_GET_CONCURRENCY_MODE:
        {
           *value = hdd_get_concurrency_mode ( );

           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, ("concurrency mode=%d"),*value);
           break;
        }

        case WE_GET_NSS:
        {
           sme_GetConfigParam(hHal, &smeConfig);
           *value = (smeConfig.csrConfig.enable2x2 == 0) ? 1 : 2;
           hddLog(LOG1, "GET_NSS: Current NSS:%d", *value);
           break;
        }

        case WE_GET_GTX_HT_MCS:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_HT_MCS");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_HT_MCS,
                                        GTX_CMD);
           break;
        }

        case WE_GET_GTX_VHT_MCS:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_VHT_MCS");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_VHT_MCS,
                                        GTX_CMD);
           break;
        }

        case WE_GET_GTX_USRCFG:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_USR_CFG");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_USR_CFG,
                                        GTX_CMD);
           break;
        }

        case WE_GET_GTX_THRE:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_THRE");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_THRE,
                                        GTX_CMD);
           break;
        }

        case WE_GET_GTX_MARGIN:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_MARGIN");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_MARGIN,
                                        GTX_CMD);
           break;
        }

        case WE_GET_GTX_STEP:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_STEP");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_STEP,
                                        GTX_CMD);
           break;
        }

        case WE_GET_GTX_MINTPC:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_MINTPC");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_MINTPC,
                                        GTX_CMD);
           break;
        }

        case WE_GET_GTX_BWMASK:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_GTX_BW_MASK");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_GTX_BW_MASK,
                                        GTX_CMD);
           break;
        }

        case WE_GET_LDPC:
           ret = hdd_get_ldpc(pAdapter, value);
           break;

        case WE_GET_TX_STBC:
           ret = hdd_get_tx_stbc(pAdapter, value);
           break;

        case WE_GET_RX_STBC:
           ret = hdd_get_rx_stbc(pAdapter, value);
           break;

        case WE_GET_SHORT_GI:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_SGI");
           *value = sme_GetHTConfig(hHal, pAdapter->sessionId,
                                           WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ);
           break;
        }

        case WE_GET_RTSCTS:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_ENABLE_RTSCTS");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_ENABLE_RTSCTS,
                                        VDEV_CMD);
           break;
        }

        case WE_GET_CHWIDTH:
        {
           hddLog(LOG1, "GET WMI_VDEV_PARAM_CHWIDTH");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_VDEV_PARAM_CHWIDTH,
                                        VDEV_CMD);
           break;
        }

        case WE_GET_ANI_EN_DIS:
        {
           hddLog(LOG1, "GET WMI_PDEV_PARAM_ANI_ENABLE");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_PDEV_PARAM_ANI_ENABLE,
                                        PDEV_CMD);
           break;
        }

        case WE_GET_ANI_POLL_PERIOD:
        {
           hddLog(LOG1, "GET WMI_PDEV_PARAM_ANI_POLL_PERIOD");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_PDEV_PARAM_ANI_POLL_PERIOD,
                                        PDEV_CMD);
           break;
        }

        case WE_GET_ANI_LISTEN_PERIOD:
        {
           hddLog(LOG1, "GET WMI_PDEV_PARAM_ANI_LISTEN_PERIOD");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_PDEV_PARAM_ANI_LISTEN_PERIOD,
                                        PDEV_CMD);
           break;
        }

        case WE_GET_ANI_OFDM_LEVEL:
        {
           hddLog(LOG1, "GET WMI_PDEV_PARAM_ANI_OFDM_LEVEL");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_PDEV_PARAM_ANI_OFDM_LEVEL,
                                        PDEV_CMD);
           break;
        }

        case WE_GET_ANI_CCK_LEVEL:
        {
           hddLog(LOG1, "GET WMI_PDEV_PARAM_ANI_CCK_LEVEL");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_PDEV_PARAM_ANI_CCK_LEVEL,
                                        PDEV_CMD);
           break;
        }

        case WE_GET_DYNAMIC_BW:
        {
           hddLog(LOG1, "GET WMI_PDEV_PARAM_ANI_CCK_LEVEL");
           *value = wma_cli_get_command(wmapvosContext,
                                        (int)pAdapter->sessionId,
                                        (int)WMI_PDEV_PARAM_DYNAMIC_BW,
                                        PDEV_CMD);
           break;
        }

        case WE_GET_11N_RATE:
        {
            hddLog(LOG1, "GET WMI_VDEV_PARAM_FIXED_RATE");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PARAM_FIXED_RATE,
                                         VDEV_CMD);
            break;
        }

        case WE_GET_AMPDU:
        {
            hddLog(LOG1, "GET AMPDU");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)GEN_VDEV_PARAM_AMPDU,
                                         GEN_CMD);
            break;
        }

        case WE_GET_AMSDU:
        {
            hddLog(LOG1, "GET AMSDU");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)GEN_VDEV_PARAM_AMSDU,
                                         GEN_CMD);
            break;
        }

        case WE_GET_BURST_ENABLE:
        {
            hddLog(LOG1, "GET Burst enable value");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_BURST_ENABLE,
                                         PDEV_CMD);
            break;
        }
        case WE_GET_BURST_DUR:
        {
            hddLog(LOG1, "GET Burst Duration value");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_BURST_DUR,
                                         PDEV_CMD);
            break;
        }

        case WE_GET_TX_CHAINMASK:
        {
            hddLog(LOG1, "GET WMI_PDEV_PARAM_TX_CHAIN_MASK");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_TX_CHAIN_MASK,
                                         PDEV_CMD);
            break;
        }

        case WE_GET_RX_CHAINMASK:
        {
            hddLog(LOG1, "GET WMI_PDEV_PARAM_RX_CHAIN_MASK");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_RX_CHAIN_MASK,
                                         PDEV_CMD);
            break;
        }

        case WE_GET_TXPOW_2G:
        {
            tANI_U32 txpow2g = 0;
            tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
            hddLog(LOG1, "GET WMI_PDEV_PARAM_TXPOWER_LIMIT2G");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_TXPOWER_LIMIT2G,
                                         PDEV_CMD);
            if ( eHAL_STATUS_SUCCESS != ccmCfgGetInt(hHal,
                      WNI_CFG_CURRENT_TX_POWER_LEVEL, &txpow2g) )
            {
                 return -EIO;
            }
            hddLog(LOG1, "2G tx_power %d", txpow2g);
            break;
        }

        case WE_GET_TXPOW_5G:
        {
            tANI_U32 txpow5g = 0;
            tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
            hddLog(LOG1, "GET WMI_PDEV_PARAM_TXPOWER_LIMIT5G");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_PDEV_PARAM_TXPOWER_LIMIT5G,
                                         PDEV_CMD);
            if ( eHAL_STATUS_SUCCESS != ccmCfgGetInt(hHal,
                      WNI_CFG_CURRENT_TX_POWER_LEVEL, &txpow5g) )
            {
                 return -EIO;
            }
            hddLog(LOG1, "5G tx_power %d", txpow5g);
            break;
        }

        case WE_GET_POWER_GATING:
        {
            hddLog(LOG1, "GET WMI_PDEV_PARAM_POWER_GATING_SLEEP");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                        (int)WMI_PDEV_PARAM_POWER_GATING_SLEEP,
                                        PDEV_CMD);
            break;
        }

	case WE_GET_PPS_PAID_MATCH:
        {
            hddLog(LOG1, "GET WMI_VDEV_PPS_PAID_MATCH");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PPS_PAID_MATCH,
                                         PPS_CMD);
            break;
        }

	case WE_GET_PPS_GID_MATCH:
        {
            hddLog(LOG1, "GET WMI_VDEV_PPS_GID_MATCH");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PPS_GID_MATCH,
                                         PPS_CMD);
            break;
	}

	case WE_GET_PPS_EARLY_TIM_CLEAR:
        {
            hddLog(LOG1, "GET WMI_VDEV_PPS_EARLY_TIM_CLEAR");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PPS_EARLY_TIM_CLEAR,
                                         PPS_CMD);
            break;
	}

	case WE_GET_PPS_EARLY_DTIM_CLEAR:
        {
            hddLog(LOG1, "GET WMI_VDEV_PPS_EARLY_DTIM_CLEAR");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PPS_EARLY_DTIM_CLEAR,
                                         PPS_CMD);
            break;
	}

	case WE_GET_PPS_EOF_PAD_DELIM:
        {
            hddLog(LOG1, "GET WMI_VDEV_PPS_EOF_PAD_DELIM");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PPS_EOF_PAD_DELIM,
                                         PPS_CMD);
            break;
	}

	case WE_GET_PPS_MACADDR_MISMATCH:
        {
            hddLog(LOG1, "GET WMI_VDEV_PPS_MACADDR_MISMATCH");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PPS_MACADDR_MISMATCH,
                                         PPS_CMD);
            break;
	}

	case WE_GET_PPS_DELIM_CRC_FAIL:
        {
            hddLog(LOG1, "GET WMI_VDEV_PPS_DELIM_CRC_FAIL");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PPS_DELIM_CRC_FAIL,
                                         PPS_CMD);
            break;
	}

	case WE_GET_PPS_GID_NSTS_ZERO:
        {
            hddLog(LOG1, "GET WMI_VDEV_PPS_GID_NSTS_ZERO");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PPS_GID_NSTS_ZERO,
                                         PPS_CMD);
            break;
	}

	case WE_GET_PPS_RSSI_CHECK:
	{

            hddLog(LOG1, "GET WMI_VDEV_PPS_RSSI_CHECK");
            *value = wma_cli_get_command(wmapvosContext,
                                         (int)pAdapter->sessionId,
                                         (int)WMI_VDEV_PPS_RSSI_CHECK,
                                         PPS_CMD);
            break;
	}

        case WE_GET_QPOWER_MAX_PSPOLL_COUNT:
        {
               hddLog(LOG1, "WE_GET_QPOWER_MAX_PSPOLL_COUNT");
               *value = wma_cli_get_command(wmapvosContext,
                                      (int)pAdapter->sessionId,
                                      (int)WMI_STA_PS_PARAM_QPOWER_PSPOLL_COUNT,
                                      QPOWER_CMD);
               break;
        }

        case WE_GET_QPOWER_MAX_TX_BEFORE_WAKE:
        {
           hddLog(LOG1, "WE_GET_QPOWER_MAX_TX_BEFORE_WAKE");
           *value = wma_cli_get_command(wmapvosContext,
                              (int)pAdapter->sessionId,
                              (int)WMI_STA_PS_PARAM_QPOWER_MAX_TX_BEFORE_WAKE,
                              QPOWER_CMD);
           break;
        }

        case WE_GET_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL:
        {
           hddLog(LOG1, "WE_GET_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL");
           *value = wma_cli_get_command(wmapvosContext,
                      (int)pAdapter->sessionId,
                      (int)WMI_STA_PS_PARAM_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL,
                      QPOWER_CMD);
           break;
        }

        case WE_GET_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL:
        {
           hddLog(LOG1, "WE_GET_QPOWER_MAX_PSPOLL_COUNT");
           *value = wma_cli_get_command(wmapvosContext,
                      (int)pAdapter->sessionId,
                      (int)WMI_STA_PS_PARAM_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL,
                      QPOWER_CMD);
           break;
        }

        case WE_GET_SCAN_BAND_PREFERENCE:
        {
            sme_GetConfigParam(hHal, &smeConfig);
            *value = smeConfig.csrConfig.scanBandPreference;

            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "scanBandPreference = %d", *value);
            break;
        }

        default:
        {
           hddLog(LOGE, "Invalid IOCTL get_value command %d", value[0]);
           break;
        }
    }

    return ret;
}

/* set param sub-ioctls */
int iw_set_three_ints_getnone(struct net_device *dev,
                              struct iw_request_info *info,
                              union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    int *value = (int *)extra;
    int sub_cmd = value[0];
    int ret = 0;

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EBUSY;
    }

    if (!capable(CAP_NET_ADMIN)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("permission check failed"));
        return -EPERM;
    }

    switch(sub_cmd) {

    case WE_SET_WLAN_DBG:
       vos_trace_setValue( value[1], value[2], value[3]);
       break;

    case WE_SET_SAP_CHANNELS:
       ret = iw_softap_set_channel_range( dev, value[1], value[2], value[3]);
       break;

    default:
       hddLog(LOGE, "%s: Invalid IOCTL command %d", __func__, sub_cmd );
       break;

    }
    return ret;
}

static int iw_get_char_setnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    int sub_cmd = wrqu->data.flags;
#ifdef WLAN_FEATURE_11W
    hdd_wext_state_t *pWextState;
#endif

#ifdef WLAN_FEATURE_11W
    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
#endif

    if (NULL == WLAN_HDD_GET_CTX(pAdapter))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                        "%s: HDD Context is NULL!", __func__);

        return -EINVAL;
    }

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EBUSY;
    }

    switch(sub_cmd)
    {
        case WE_WLAN_VERSION:
        {
            hdd_wlan_get_version(pAdapter, wrqu, extra);
            break;
        }

        case WE_GET_STATS:
        {
            hdd_wlan_get_stats(pAdapter, &(wrqu->data.length),
                               extra, WE_MAX_STR_LEN);
            break;
        }

/* The case prints the current state of the HDD, SME, CSR, PE, TL
   *it can be extended for WDI Global State as well.
   *And currently it only checks P2P_CLIENT adapter.
   *P2P_DEVICE and P2P_GO have not been added as of now.
*/
        case WE_GET_STATES:
        {
            int buf = 0, len = 0;
            int adapter_num = 0;
            int count = 0, check = 1;

            tANI_U16 tlState;
            tHalHandle hHal = NULL;
            tpAniSirGlobal pMac = NULL;
            hdd_station_ctx_t *pHddStaCtx = NULL;

            hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
            hdd_adapter_t *useAdapter = NULL;

            /* Print wlan0 or p2p0 states based on the adapter_num
              *by using the correct adapter
            */
            while ( adapter_num < 2 )
            {
                if ( WLAN_ADAPTER == adapter_num )
                {
                    useAdapter = pAdapter;
                    buf = scnprintf(extra + len, WE_MAX_STR_LEN - len,
                            "\n\n wlan0 States:-");
                    len += buf;
                }
                else if ( P2P_ADAPTER == adapter_num )
                {
                    buf = scnprintf(extra + len, WE_MAX_STR_LEN - len,
                            "\n\n p2p0 States:-");
                    len += buf;

                    if( !pHddCtx )
                    {
                        buf = scnprintf(extra + len, WE_MAX_STR_LEN - len,
                                "\n pHddCtx is NULL");
                        len += buf;
                        break;
                    }

                    /*Printing p2p0 states only in the case when the device is
                      configured as a p2p_client*/
                    useAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_P2P_CLIENT);
                    if ( !useAdapter )
                    {
                        buf = scnprintf(extra + len, WE_MAX_STR_LEN - len,
                                "\n Device not configured as P2P_CLIENT.");
                        len += buf;
                        break;
                    }
                }

                hHal = WLAN_HDD_GET_HAL_CTX( useAdapter );
                if (!hHal) {
                    buf = scnprintf(extra + len,  WE_MAX_STR_LEN - len,
                            "\n pMac is NULL");
                    len += buf;
                    break;
                }
                pMac = PMAC_STRUCT( hHal );
                if (!pMac) {
                    buf = scnprintf(extra + len,  WE_MAX_STR_LEN - len,
                            "\n pMac is NULL");
                    len += buf;
                    break;
                }
                pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(useAdapter);

                tlState = smeGetTLSTAState(hHal, pHddStaCtx->conn_info.staId[0]);

                buf = scnprintf(extra + len, WE_MAX_STR_LEN - len,
                        "\n HDD Conn State - %s "
                        "\n \n SME State:"
                        "\n Neighbour Roam State - %s"
                        "\n CSR State - %s"
                        "\n CSR Substate - %s"
                        "\n \n TL STA %d State: %s",
                        macTraceGetHDDWlanConnState(
                                pHddStaCtx->conn_info.connState),
                        macTraceGetNeighbourRoamState(
                              sme_getNeighborRoamState(hHal,
                                  useAdapter->sessionId)),
                        macTraceGetcsrRoamState(
                              sme_getCurrentRoamState(hHal,
                                  useAdapter->sessionId)),
                        macTraceGetcsrRoamSubState(
                              sme_getCurrentRoamSubState(hHal,
                                  useAdapter->sessionId)),
                        pHddStaCtx->conn_info.staId[0],
                        macTraceGetTLState(tlState)
                        );
                len += buf;
                adapter_num++;
            }

            if (hHal) {
                /* Printing Lim State starting with global lim states */
                buf = scnprintf(extra + len, WE_MAX_STR_LEN - len,
                        "\n \n LIM STATES:-"
                        "\n Global Sme State - %s "\
                        "\n Global mlm State - %s "\
                        "\n",
                        macTraceGetLimSmeState(sme_getLimSmeState(hHal)),
                        macTraceGetLimMlmState(sme_getLimSmeState(hHal))
                        );
                len += buf;

                /* Printing the PE Sme and Mlm states for valid lim sessions */
                while (check < 3 && count < 255) {
                    if (sme_IsLimSessionValid(hHal, count)) {
                        buf = scnprintf(extra + len, WE_MAX_STR_LEN - len,
                            "\n Lim Valid Session %d:-"
                            "\n PE Sme State - %s "
                            "\n PE Mlm State - %s "
                            "\n",
                            check,
                            macTraceGetLimSmeState(sme_getLimSmeSessionState(
                                                                  hHal, count)),
                            macTraceGetLimMlmState(sme_getLimMlmSessionState(
                                                                  hHal, count))
                            );

                        len += buf;
                        check++;
                    }
                    count++;
                }
            }

            wrqu->data.length = strlen(extra)+1;
            break;
        }

        case WE_GET_CFG:
        {
#ifdef WLAN_FEATURE_MBSSID
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: Printing Adapter MBSSID SAP Dyn INI Config", __func__);
            hdd_cfg_get_sap_dyn_config(pAdapter,
                                      extra, QCSAP_IOCTL_MAX_STR_LEN);
            /* Overwrite extra buffer with global ini config if need to return
             * in buf
             */
#endif
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                               "%s: Printing CLD global INI Config", __func__);
            hdd_cfg_get_global_config(WLAN_HDD_GET_CTX(pAdapter), extra,
                                      QCSAP_IOCTL_MAX_STR_LEN);
            wrqu->data.length = strlen(extra)+1;
            break;
        }
#ifdef WLAN_FEATURE_11AC
        case WE_GET_RSSI:
        {
            v_S7_t s7Rssi = 0;
            wlan_hdd_get_rssi(pAdapter, &s7Rssi);
            snprintf(extra, WE_MAX_STR_LEN, "rssi=%d",s7Rssi);
            wrqu->data.length = strlen(extra)+1;
            break;
        }
#endif

#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
        case WE_GET_ROAM_RSSI:
        {
            v_S7_t s7Rssi = 0;
            wlan_hdd_get_roam_rssi(pAdapter, &s7Rssi);
            snprintf(extra, WE_MAX_STR_LEN, "rssi=%d", s7Rssi);
            wrqu->data.length = strlen(extra)+1;
            break;
        }
#endif
        case WE_GET_WMM_STATUS:
        {
            snprintf(extra, WE_MAX_STR_LEN,
                    "\nDir: 0=up, 1=down, 3=both\n"
                    "|------------------------|\n"
                    "|AC | ACM |Admitted| Dir |\n"
                    "|------------------------|\n"
                    "|VO |  %d  |  %3s   |  %d  |\n"
                    "|VI |  %d  |  %3s   |  %d  |\n"
                    "|BE |  %d  |  %3s   |  %d  |\n"
                    "|BK |  %d  |  %3s   |  %d  |\n"
                    "|------------------------|\n",
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_VO].wmmAcAccessRequired,
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_VO].wmmAcAccessAllowed?"YES":"NO",
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_VO].wmmAcTspecInfo.ts_info.direction,
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_VI].wmmAcAccessRequired,
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_VI].wmmAcAccessAllowed?"YES":"NO",
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_VI].wmmAcTspecInfo.ts_info.direction,
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_BE].wmmAcAccessRequired,
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_BE].wmmAcAccessAllowed?"YES":"NO",
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_BE].wmmAcTspecInfo.ts_info.direction,
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_BK].wmmAcAccessRequired,
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_BK].wmmAcAccessAllowed?"YES":"NO",
                    pAdapter->hddWmmStatus.wmmAcStatus[WLANTL_AC_BK].wmmAcTspecInfo.ts_info.direction);


            wrqu->data.length = strlen(extra)+1;
            break;
        }
        case WE_GET_CHANNEL_LIST:
        {
            VOS_STATUS status;
            v_U8_t i, len;
            char* buf ;

            tChannelListInfo channel_list;

            memset(&channel_list, 0, sizeof(channel_list));
            status = iw_softap_get_channel_list(dev, info, wrqu, (char *)&channel_list);
            if ( !VOS_IS_STATUS_SUCCESS( status ) )
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s GetChannelList Failed!!!", __func__);
                return -EINVAL;
            }
            buf = extra;
            /**
             * Maximum channels = WNI_CFG_VALID_CHANNEL_LIST_LEN. Maximum buffer
             * needed = 5 * number of channels. Check if sufficient
             * buffer is available and then proceed to fill the buffer.
             */
            if(WE_MAX_STR_LEN < (5 * WNI_CFG_VALID_CHANNEL_LIST_LEN))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s Insufficient Buffer to populate channel list",
                            __func__);
                return -EINVAL;
            }
            len = scnprintf(buf, WE_MAX_STR_LEN, "%u ",
                    channel_list.num_channels);
            for(i = 0 ; i < channel_list.num_channels; i++)
            {
                len += scnprintf(buf + len, WE_MAX_STR_LEN - len,
                               "%u ", channel_list.channels[i]);
            }
            wrqu->data.length = strlen(extra)+1;

            break;
        }
#ifdef FEATURE_WLAN_TDLS
        case WE_GET_TDLS_PEERS:
        {
            wrqu->data.length = wlan_hdd_tdls_get_all_peers(pAdapter, extra, WE_MAX_STR_LEN)+1;
            break;
        }
#endif
#ifdef WLAN_FEATURE_11W
       case WE_GET_11W_INFO:
       {
           hddLog(LOGE, "WE_GET_11W_ENABLED = %d",  pWextState->roamProfile.MFPEnabled );

           snprintf(extra, WE_MAX_STR_LEN,
                    "\n BSSID %02X:%02X:%02X:%02X:%02X:%02X, Is PMF Assoc? %d"
                    "\n Number of Unprotected Disassocs %d"
                    "\n Number of Unprotected Deauths %d",
                    (*pWextState->roamProfile.BSSIDs.bssid)[0], (*pWextState->roamProfile.BSSIDs.bssid)[1],
                    (*pWextState->roamProfile.BSSIDs.bssid)[2], (*pWextState->roamProfile.BSSIDs.bssid)[3],
                    (*pWextState->roamProfile.BSSIDs.bssid)[4], (*pWextState->roamProfile.BSSIDs.bssid)[5],
                    pWextState->roamProfile.MFPEnabled, pAdapter->hdd_stats.hddPmfStats.numUnprotDisassocRx,
                    pAdapter->hdd_stats.hddPmfStats.numUnprotDeauthRx);

           wrqu->data.length = strlen(extra)+1;
           break;
       }
#endif
        case WE_GET_PHYMODE:
        {
           v_BOOL_t ch_bond24 = VOS_FALSE, ch_bond5g = VOS_FALSE;
           hdd_context_t *hddctx = WLAN_HDD_GET_CTX(pAdapter);
           tHalHandle hal = WLAN_HDD_GET_HAL_CTX(pAdapter);
           eCsrPhyMode phymode;
           eCsrBand currBand;
           tSmeConfigParams smeconfig;

           sme_GetConfigParam(hal, &smeconfig);
           if (WNI_CFG_CHANNEL_BONDING_MODE_DISABLE !=
                                  smeconfig.csrConfig.channelBondingMode24GHz)
               ch_bond24 = VOS_TRUE;

           if (WNI_CFG_CHANNEL_BONDING_MODE_DISABLE !=
                                  smeconfig.csrConfig.channelBondingMode5GHz)
               ch_bond5g = VOS_TRUE;

           phymode = sme_GetPhyMode(hal);
           if ((eHAL_STATUS_SUCCESS != sme_GetFreqBand(hal, &currBand))) {
               VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                         "%s: Failed to get current band config",
                         __func__);
               return -EIO;
           }


           switch (phymode) {
           case eCSR_DOT11_MODE_AUTO:
                snprintf(extra, WE_MAX_STR_LEN, "AUTO MODE");
                break;
           case eCSR_DOT11_MODE_TAURUS:
           case eCSR_DOT11_MODE_POLARIS:
           case eCSR_DOT11_MODE_TAURUS_ONLY:
           case eCSR_DOT11_MODE_TITAN:
           case eCSR_DOT11_MODE_11n:
           case eCSR_DOT11_MODE_11n_ONLY:
                if (currBand == eCSR_BAND_24) {
                    if (ch_bond24)
                        snprintf(extra, WE_MAX_STR_LEN, "11NGHT40");
                    else
                        snprintf(extra, WE_MAX_STR_LEN, "11NGHT20");
                }
                else if(currBand == eCSR_BAND_5G) {
                    if (ch_bond5g)
                        snprintf(extra, WE_MAX_STR_LEN, "11NAHT40");
                    else
                        snprintf(extra, WE_MAX_STR_LEN, "11NAHT20");
                } else {
                    snprintf(extra, WE_MAX_STR_LEN, "11N");
                }
                break;
           case eCSR_DOT11_MODE_abg:
                snprintf(extra, WE_MAX_STR_LEN, "11ABG");
                break;
           case eCSR_DOT11_MODE_11a:
           case eCSR_DOT11_MODE_11a_ONLY:
                snprintf(extra, WE_MAX_STR_LEN, "11A");
                break;
           case eCSR_DOT11_MODE_11b:
           case eCSR_DOT11_MODE_11b_ONLY:
                snprintf(extra, WE_MAX_STR_LEN, "11B");
                break;
           case eCSR_DOT11_MODE_11g:
           case eCSR_DOT11_MODE_11g_ONLY:
                snprintf(extra, WE_MAX_STR_LEN, "11G");
                break;
#ifdef WLAN_FEATURE_11AC
           case eCSR_DOT11_MODE_11ac:
           case eCSR_DOT11_MODE_11ac_ONLY:
                if (hddctx->cfg_ini->vhtChannelWidth ==
                                                       eHT_CHANNEL_WIDTH_20MHZ)
                    snprintf(extra, WE_MAX_STR_LEN, "11ACVHT20");
                else if (hddctx->cfg_ini->vhtChannelWidth ==
                                                       eHT_CHANNEL_WIDTH_40MHZ)
                    snprintf(extra, WE_MAX_STR_LEN, "11ACVHT40");
                else if (hddctx->cfg_ini->vhtChannelWidth ==
                                                       eHT_CHANNEL_WIDTH_80MHZ)
                    snprintf(extra, WE_MAX_STR_LEN, "11ACVHT80");
                else if (hddctx->cfg_ini->vhtChannelWidth ==
                                                       eHT_CHANNEL_WIDTH_160MHZ)
                    snprintf(extra, WE_MAX_STR_LEN, "11ACVHT160");
                break;
#endif
           }

           wrqu->data.length = strlen(extra)+1;
           break;
        }

#ifdef FEATURE_OEM_DATA_SUPPORT
        case WE_GET_OEM_DATA_CAP:
        {
            return iw_get_oem_data_cap(dev, info, wrqu, extra);
        }
#endif /* FEATURE_OEM_DATA_SUPPORT */
        default:
        {
            hddLog(LOGE, "%s: Invalid IOCTL command %d", __func__, sub_cmd );
            break;
        }
    }

    return 0;
}

/*  action sub-ioctls */
static int iw_setnone_getnone(struct net_device *dev, struct iw_request_info *info,
                       union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    int ret = 0; /* success */
    int sub_cmd;

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
        msleep(1000);
        return -EBUSY;
    }

#ifdef CONFIG_COMPAT
    /* this ioctl is a special case where a sub-ioctl is used and both
     * the number of get and set args is 0.  in this specific case the
     * logic in iwpriv places the sub_cmd in the data.flags portion of
     * the iwreq.  unfortunately the location of this field will be
     * different between 32-bit and 64-bit user space, and the standard
     * compat support in the kernel does not handle this case.  so we
     * need to explicitly handle it here. */
    if (is_compat_task()) {
        struct compat_iw_point *compat_iw_point =
            (struct compat_iw_point *) &wrqu->data;
        sub_cmd = compat_iw_point->flags;
    } else {
        sub_cmd = wrqu->data.flags;
    }
#else
    sub_cmd = wrqu->data.flags;
#endif

    switch (sub_cmd)
    {
        case WE_CLEAR_STATS:
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"%s: clearing", __func__);
            memset(&pAdapter->stats, 0, sizeof(pAdapter->stats));
            memset(&pAdapter->hdd_stats, 0, sizeof(pAdapter->hdd_stats));
            break;
        }

        case WE_GET_RECOVERY_STAT:
        {
            tHalHandle hal = WLAN_HDD_GET_HAL_CTX(pAdapter);
            sme_getRecoveryStats(hal);
            break;
        }

#ifdef WLAN_BTAMP_FEATURE
        case WE_ENABLE_AMP:
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"%s: enabling AMP", __func__);
            WLANBAP_RegisterWithHCI(pAdapter);
            break;
        }
        case WE_DISABLE_AMP:
        {
            hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
            VOS_STATUS status;

            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"%s: disabling AMP", __func__);

            pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
            status = WLANBAP_StopAmp();
            if(VOS_STATUS_SUCCESS != status )
            {
               pHddCtx->isAmpAllowed = VOS_TRUE;
               hddLog(VOS_TRACE_LEVEL_FATAL,
                      "%s: Failed to stop AMP", __func__);
            }
            else
            {
               //a state m/c implementation in PAL is TBD to avoid this delay
               msleep(500);
               pHddCtx->isAmpAllowed = VOS_FALSE;
               WLANBAP_DeregisterFromHCI();
            }

            break;
        }
#endif
        case WE_ENABLE_DXE_STALL_DETECT:
        {
            tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
            sme_transportDebug(hHal, VOS_FALSE, VOS_TRUE);
            break;
        }
        case WE_DISPLAY_DXE_SNAP_SHOT:
        {
            tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
            sme_transportDebug(hHal, VOS_TRUE, VOS_FALSE);
            break;
        }
        case WE_DISPLAY_DATAPATH_SNAP_SHOT:
        {
            hddLog(LOGE, "%s: called %d",__func__, sub_cmd);
            hdd_wmm_tx_snapshot(pAdapter);
            WLANTL_TLDebugMessage(VOS_TRUE);
            break;
        }
        case WE_SET_REASSOC_TRIGGER:
        {
            hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
            tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
            v_U32_t roamId = 0;
            tCsrRoamModifyProfileFields modProfileFields;
            sme_GetModifyProfileFields(hHal, pAdapter->sessionId,
                                       &modProfileFields);
            sme_RoamReassoc(hHal, pAdapter->sessionId,
                            NULL, modProfileFields, &roamId, 1);
            return 0;
        }

        case WE_DUMP_AGC_START:
        {
            hddLog(LOG1, "WE_DUMP_AGC_START");
            ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)GEN_PARAM_DUMP_AGC_START,
                                          0, GEN_CMD);
            break;
        }
        case WE_DUMP_AGC:
        {
            hddLog(LOG1, "WE_DUMP_AGC");
            ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)GEN_PARAM_DUMP_AGC,
                                          0, GEN_CMD);
            break;
        }

        case WE_DUMP_CHANINFO_START:
        {
            hddLog(LOG1, "WE_DUMP_CHANINFO_START");
            ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)GEN_PARAM_DUMP_CHANINFO_START,
                                          0, GEN_CMD);
            break;
        }
        case WE_DUMP_CHANINFO:
        {
            hddLog(LOG1, "WE_DUMP_CHANINFO_START");
            ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)GEN_PARAM_DUMP_CHANINFO,
                                          0, GEN_CMD);
            break;
        }
        case WE_DUMP_WATCHDOG:
        {
            hddLog(LOG1, "WE_DUMP_WATCHDOG");
            ret = process_wma_set_command((int)pAdapter->sessionId,
                                          (int)GEN_PARAM_DUMP_WATCHDOG,
                                          0, GEN_CMD);
            break;
        }
#ifdef CONFIG_ATH_PCIE_ACCESS_DEBUG
        case WE_DUMP_PCIE_LOG:
        {
          hddLog(LOGE, "WE_DUMP_PCIE_LOG");
          ret = process_wma_set_command((int) pAdapter->sessionId,
                                        (int) GEN_PARAM_DUMP_PCIE_ACCESS_LOG,
                                        0, GEN_CMD);
          break;
        }
#endif
        default:
        {
            hddLog(LOGE, "%s: unknown ioctl %d", __func__, sub_cmd);
            break;
        }
    }

    return ret;
}

void hdd_wmm_tx_snapshot(hdd_adapter_t *pAdapter)
{
    /*
     * Function to display HDD WMM information
     * for Tx Queues.
     * Prints global as well as per client depending
     * whether the clients are registered or not.
     */
    int i = 0, j = 0;
    for ( i=0; i< NUM_TX_QUEUES; i++)
    {
        spin_lock_bh(&pAdapter->wmm_tx_queue[i].lock);
        hddLog(LOGE, "HDD WMM TxQueue Info For AC: %d Count: %d PrevAdress:%p, NextAddress:%p",
               i, pAdapter->wmm_tx_queue[i].count,
               pAdapter->wmm_tx_queue[i].anchor.prev, pAdapter->wmm_tx_queue[i].anchor.next);
        spin_unlock_bh(&pAdapter->wmm_tx_queue[i].lock);
    }

    for(i =0; i<WLAN_MAX_STA_COUNT; i++)
    {
        if(pAdapter->aStaInfo[i].isUsed)
        {
             hddLog(LOGE, "******STAIndex: %d*********", i);
             for ( j=0; j< NUM_TX_QUEUES; j++)
             {
                spin_lock_bh(&pAdapter->aStaInfo[i].wmm_tx_queue[j].lock);
                hddLog(LOGE, "HDD TxQueue Info For AC: %d Count: %d PrevAdress:%p, NextAddress:%p",
                       j, pAdapter->aStaInfo[i].wmm_tx_queue[j].count,
                       pAdapter->aStaInfo[i].wmm_tx_queue[j].anchor.prev,
                       pAdapter->aStaInfo[i].wmm_tx_queue[j].anchor.next);
                spin_unlock_bh(&pAdapter->aStaInfo[i].wmm_tx_queue[j].lock);
             }
        }
    }

}

static int __iw_set_var_ints_getnone(struct net_device *dev,
                                     struct iw_request_info *info,
                                     union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    int sub_cmd;
    int apps_args[MAX_VAR_ARGS] = {0};
    int num_args;
    hdd_station_ctx_t *pStaCtx = NULL ;
    hdd_ap_ctx_t  *pAPCtx = NULL;
    int cmd = 0;
    int staId = 0;
    struct iw_point s_priv_data;

    if (!capable(CAP_NET_ADMIN)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                FL("permission check failed"));
        return -EPERM;
    }
    /* helper function to get iwreq_data with compat handling. */
    if (hdd_priv_get_data(&s_priv_data, wrqu)) {
       return -EINVAL;
    }

    if (NULL == s_priv_data.pointer) {
       return -EINVAL;
    }

    sub_cmd = s_priv_data.flags;
    num_args = s_priv_data.length;

    hddLog(LOG1, "%s: Received length %d", __func__, s_priv_data.length);

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EBUSY;
    }

    if (num_args > MAX_VAR_ARGS)
    {
       num_args = MAX_VAR_ARGS;
    }

    /* ODD number is used for set, copy data using copy_from_user */
    if (copy_from_user(apps_args, s_priv_data.pointer,
                       (sizeof(int)) * num_args)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to copy data to user buffer", __func__);
        return -EFAULT;
    }

    if(( sub_cmd == WE_MCC_CONFIG_CREDENTIAL ) ||
        (sub_cmd == WE_MCC_CONFIG_PARAMS ))
    {
        if(( pAdapter->device_mode == WLAN_HDD_INFRA_STATION )||
           ( pAdapter->device_mode == WLAN_HDD_P2P_CLIENT ))
        {
            pStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
            staId = pStaCtx->conn_info.staId[0];
        }
        else if (( pAdapter->device_mode == WLAN_HDD_P2P_GO ) ||
                 ( pAdapter->device_mode == WLAN_HDD_SOFTAP ))
        {
            pAPCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
            staId = pAPCtx->uBCStaId;
        }
        else
        {
            hddLog(LOGE, "%s: Device mode %d not recognised", __FUNCTION__, pAdapter->device_mode);
            return 0;
        }
    }

    switch (sub_cmd)
    {
        case WE_LOG_DUMP_CMD:
            {
                hddLog(LOG1, "%s: LOG_DUMP %d arg1 %d arg2 %d arg3 %d arg4 %d",
                        __func__, apps_args[0], apps_args[1], apps_args[2],
                        apps_args[3], apps_args[4]);

                logPrintf(hHal, apps_args[0], apps_args[1], apps_args[2],
                        apps_args[3], apps_args[4]);

            }
            break;

        case WE_P2P_NOA_CMD:
            {
                p2p_app_setP2pPs_t p2pNoA;

                p2pNoA.opp_ps = apps_args[0];
                p2pNoA.ctWindow = apps_args[1];
                p2pNoA.duration = apps_args[2];
                p2pNoA.interval  = apps_args[3];
                p2pNoA.count = apps_args[4];
                p2pNoA.single_noa_duration = apps_args[5];
                p2pNoA.psSelection = apps_args[6];

                hddLog(LOG1, "%s: P2P_NOA_ATTR:oppPS %d ctWindow %d duration %d "
                       "interval %d count %d single noa duration %d PsSelection %x",
                       __func__, apps_args[0], apps_args[1], apps_args[2],
                       apps_args[3], apps_args[4], apps_args[5], apps_args[6]);

                hdd_setP2pPs(dev, &p2pNoA);

            }
            break;

        case WE_MTRACE_SELECTIVE_MODULE_LOG_ENABLE_CMD:
            {
                hddLog(LOG1, "%s: SELECTIVE_MODULE_LOG %d arg1 %d arg2",
                        __func__, apps_args[0], apps_args[1]);
                vosTraceEnable(apps_args[0], apps_args[1]);
            }
            break;

        case WE_MTRACE_DUMP_CMD:
            {
                hddLog(LOG1, "%s: MTRACE_DUMP code %d session %d count %d "
                       "bitmask_of_module %d ",
                        __func__, apps_args[0], apps_args[1], apps_args[2],
                        apps_args[3]);
                vosTraceDumpAll((void*)hHal , apps_args[0], apps_args[1],
                                apps_args[2], apps_args[3]);

            }
            break;

        case WE_MCC_CONFIG_CREDENTIAL :
            {
                cmd = 287; //Command should be updated if there is any change
                           // in the Riva dump command
                if((apps_args[0] >= 40 ) && (apps_args[0] <= 160 ))
                {
                    logPrintf(hHal, cmd, staId, apps_args[0], apps_args[1], apps_args[2]);
                }
                else
                {
                     hddLog(LOGE, "%s : Enter valid MccCredential value between MIN :40 and MAX:160", __func__);
                     return 0;
                }
            }
            break;

        case WE_MCC_CONFIG_PARAMS :
            {
                cmd = 288; //command Should be updated if there is any change
                           // in the Riva dump command
                 hdd_validate_mcc_config(pAdapter, staId, apps_args[0], apps_args[1],apps_args[2]);
            }
        break;

#ifdef FEATURE_WLAN_TDLS
        case WE_TDLS_CONFIG_PARAMS :
            {
                tdls_config_params_t tdlsParams;

                tdlsParams.tdls                    = apps_args[0];
                tdlsParams.tx_period_t             = apps_args[1];
                tdlsParams.tx_packet_n             = apps_args[2];
                tdlsParams.discovery_period_t      = apps_args[3];
                tdlsParams.discovery_tries_n       = apps_args[4];
                tdlsParams.idle_timeout_t          = apps_args[5];
                tdlsParams.idle_packet_n           = apps_args[6];
                tdlsParams.rssi_hysteresis         = apps_args[7];
                tdlsParams.rssi_trigger_threshold  = apps_args[8];
                tdlsParams.rssi_teardown_threshold = apps_args[9];
                tdlsParams.rssi_delta              = apps_args[10];

                wlan_hdd_tdls_set_params(dev, &tdlsParams);
            }
        break;
#endif
        case WE_UNIT_TEST_CMD :
            {
                t_wma_unit_test_cmd *unitTestArgs;
                vos_msg_t msg = {0};
                int i, j;
                if ((apps_args[0] < WLAN_MODULE_ID_MIN) ||
                               (apps_args[0] >= WLAN_MODULE_ID_MAX)) {
                    hddLog(LOGE, FL("Invalid MODULE ID %d"), apps_args[0]);
                    return -EINVAL;
                }
                if (apps_args[1] > (WMA_MAX_NUM_ARGS)) {
                    hddLog(LOGE, FL("Too Many args %d"), apps_args[1]);
                    return -EINVAL;
                }
                unitTestArgs = vos_mem_malloc(sizeof(*unitTestArgs));
                if (NULL == unitTestArgs) {
                    hddLog(LOGE,
                      FL("vos_mem_alloc failed for unitTestArgs"));
                    return -ENOMEM;
                }
                unitTestArgs->vdev_id            = (int)pAdapter->sessionId;
                unitTestArgs->module_id          = apps_args[0];
                unitTestArgs->num_args           = apps_args[1];
                for (i = 0, j = 2; i < unitTestArgs->num_args; i++, j++) {
                    unitTestArgs->args[i] = apps_args[j];
                }
                msg.type = SIR_HAL_UNIT_TEST_CMD;
                msg.reserved = 0;
                msg.bodyptr = unitTestArgs;
                if (VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA,
                                                                    &msg)) {
                    vos_mem_free(unitTestArgs);
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                        FL("Not able to post UNIT_TEST_CMD message to WDA"));
                    return -EINVAL;
                }
            }
        break;
        default:
            {
                hddLog(LOGE, FL("Invalid IOCTL command %d"), sub_cmd );
            }
            break;
    }

    return 0;
}


int iw_set_var_ints_getnone(struct net_device *dev,
                            struct iw_request_info *info,
                            union iwreq_data *wrqu, char *extra)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __iw_set_var_ints_getnone(dev, info, wrqu, extra);
   vos_ssr_unprotect(__func__);
   return ret;
}


static int iw_add_tspec(struct net_device *dev, struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   hdd_wlan_wmm_status_e *pStatus = (hdd_wlan_wmm_status_e *)extra;
   int params[HDD_WLAN_WMM_PARAM_COUNT];
   sme_QosWmmTspecInfo tSpec;
   v_U32_t handle;
   struct iw_point s_priv_data;

   /*
    * Make sure the application is sufficiently privileged
    * note that the kernel will do this for "set" ioctls, but since
    * this ioctl wants to return status to user space it must be
    * defined as a "get" ioctl.
    */
   if (!capable(CAP_NET_ADMIN))
   {
      return -EPERM;
   }

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
      return -EBUSY;
   }

   // we must be associated in order to add a tspec
   if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
   {
      *pStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
      return 0;
   }

   // since we are defined to be a "get" ioctl, and since the number
   // of params exceeds the number of params that wireless extensions
   // will pass down in the iwreq_data, we must copy the "set" params.
   // We must handle the compat for iwreq_data in 32U/64K environment.

   // helper function to get iwreq_data with compat handling.
   if (hdd_priv_get_data(&s_priv_data, wrqu)) {
      *pStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
      return 0;
   }

   // make sure all params are correctly passed to function
   if ((NULL == s_priv_data.pointer) ||
       (HDD_WLAN_WMM_PARAM_COUNT != s_priv_data.length)) {
      *pStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
      return 0;
   }

   // from user space ourselves
   if (copy_from_user(&params, s_priv_data.pointer, sizeof(params))) {
      // hmmm, can't get them
      return -EIO;
   }

   // clear the tspec
   memset(&tSpec, 0, sizeof(tSpec));

   // validate the handle
   handle = params[HDD_WLAN_WMM_PARAM_HANDLE];
   if (HDD_WMM_HANDLE_IMPLICIT == handle)
   {
      // that one is reserved
      *pStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
      return 0;
   }

   // validate the TID
   if (params[HDD_WLAN_WMM_PARAM_TID] > 7)
   {
      // out of range
      *pStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
      return 0;
   }
   tSpec.ts_info.tid = params[HDD_WLAN_WMM_PARAM_TID];

   // validate the direction
   switch (params[HDD_WLAN_WMM_PARAM_DIRECTION])
   {
   case HDD_WLAN_WMM_DIRECTION_UPSTREAM:
      tSpec.ts_info.direction = SME_QOS_WMM_TS_DIR_UPLINK;
      break;

   case HDD_WLAN_WMM_DIRECTION_DOWNSTREAM:
      tSpec.ts_info.direction = SME_QOS_WMM_TS_DIR_DOWNLINK;
      break;

   case HDD_WLAN_WMM_DIRECTION_BIDIRECTIONAL:
      tSpec.ts_info.direction = SME_QOS_WMM_TS_DIR_BOTH;
      break;

   default:
      // unknown
      *pStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
      return 0;
   }

   tSpec.ts_info.psb = params[HDD_WLAN_WMM_PARAM_APSD];

   // validate the user priority
   if (params[HDD_WLAN_WMM_PARAM_USER_PRIORITY] >= SME_QOS_WMM_UP_MAX)
   {
      // out of range
      *pStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
      return 0;
   }
   tSpec.ts_info.up = params[HDD_WLAN_WMM_PARAM_USER_PRIORITY];
   if(0 > tSpec.ts_info.up || SME_QOS_WMM_UP_MAX < tSpec.ts_info.up)
   {
   hddLog(VOS_TRACE_LEVEL_ERROR,"***ts_info.up out of bounds***");
   return 0;
   }

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
             "%s:TS_INFO PSB %d UP %d !!!", __func__,
             tSpec.ts_info.psb, tSpec.ts_info.up);

   tSpec.nominal_msdu_size = params[HDD_WLAN_WMM_PARAM_NOMINAL_MSDU_SIZE];
   tSpec.maximum_msdu_size = params[HDD_WLAN_WMM_PARAM_MAXIMUM_MSDU_SIZE];
   tSpec.min_data_rate = params[HDD_WLAN_WMM_PARAM_MINIMUM_DATA_RATE];
   tSpec.mean_data_rate = params[HDD_WLAN_WMM_PARAM_MEAN_DATA_RATE];
   tSpec.peak_data_rate = params[HDD_WLAN_WMM_PARAM_PEAK_DATA_RATE];
   tSpec.max_burst_size = params[HDD_WLAN_WMM_PARAM_MAX_BURST_SIZE];
   tSpec.min_phy_rate = params[HDD_WLAN_WMM_PARAM_MINIMUM_PHY_RATE];
   tSpec.surplus_bw_allowance = params[HDD_WLAN_WMM_PARAM_SURPLUS_BANDWIDTH_ALLOWANCE];
   tSpec.min_service_interval = params[HDD_WLAN_WMM_PARAM_SERVICE_INTERVAL];
   tSpec.max_service_interval = params[HDD_WLAN_WMM_PARAM_MAX_SERVICE_INTERVAL];
   tSpec.suspension_interval = params[HDD_WLAN_WMM_PARAM_SUSPENSION_INTERVAL];
   tSpec.inactivity_interval = params[HDD_WLAN_WMM_PARAM_INACTIVITY_INTERVAL];

   tSpec.ts_info.burst_size_defn = params[HDD_WLAN_WMM_PARAM_BURST_SIZE_DEFN];

   // validate the ts info ack policy
   switch (params[HDD_WLAN_WMM_PARAM_ACK_POLICY])
   {
   case HDD_WLAN_WMM_TS_INFO_ACK_POLICY_NORMAL_ACK:
      tSpec.ts_info.ack_policy = SME_QOS_WMM_TS_ACK_POLICY_NORMAL_ACK;
      break;

   case HDD_WLAN_WMM_TS_INFO_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK:
      tSpec.ts_info.ack_policy = SME_QOS_WMM_TS_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK;
      break;

   default:
      // unknown
      *pStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
      return 0;
   }

   *pStatus = hdd_wmm_addts(pAdapter, handle, &tSpec);
   return 0;
}


static int iw_del_tspec(struct net_device *dev, struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   int *params = (int *)extra;
   hdd_wlan_wmm_status_e *pStatus = (hdd_wlan_wmm_status_e *)extra;
   v_U32_t handle;

   /*
    * Make sure the application is sufficiently privileged
    * note that the kernel will do this for "set" ioctls, but since
    * this ioctl wants to return status to user space it must be
    * defined as a "get" ioctl.
    */
   if (!capable(CAP_NET_ADMIN))
   {
      return -EPERM;
   }

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
      return -EBUSY;
   }

   // although we are defined to be a "get" ioctl, the params we require
   // will fit in the iwreq_data, therefore unlike iw_add_tspec() there
   // is no need to copy the params from user space

   // validate the handle
   handle = params[HDD_WLAN_WMM_PARAM_HANDLE];
   if (HDD_WMM_HANDLE_IMPLICIT == handle)
   {
      // that one is reserved
      *pStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
      return 0;
   }

   *pStatus = hdd_wmm_delts(pAdapter, handle);
   return 0;
}


static int iw_get_tspec(struct net_device *dev, struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   int *params = (int *)extra;
   hdd_wlan_wmm_status_e *pStatus = (hdd_wlan_wmm_status_e *)extra;
   v_U32_t handle;

   // although we are defined to be a "get" ioctl, the params we require
   // will fit in the iwreq_data, therefore unlike iw_add_tspec() there
   // is no need to copy the params from user space

   // validate the handle
   handle = params[HDD_WLAN_WMM_PARAM_HANDLE];
   if (HDD_WMM_HANDLE_IMPLICIT == handle)
   {
      // that one is reserved
      *pStatus = HDD_WLAN_WMM_STATUS_SETUP_FAILED_BAD_PARAM;
      return 0;
   }

   if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
      return -EBUSY;
   }

   *pStatus = hdd_wmm_checkts(pAdapter, handle);
   return 0;
}

#ifdef WLAN_FEATURE_VOWIFI_11R
//
//
// Each time the supplicant has the auth_request or reassoc request
// IEs ready. This is pushed to the driver. The driver will inturn use
// it to send out the auth req and reassoc req for 11r FT Assoc.
//
static int iw_set_fties(struct net_device *dev, struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    //v_CONTEXT_t pVosContext;

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
       return -EBUSY;
    }
    if (!wrqu->data.length)
    {
        hddLog(LOGE, FL("called with 0 length IEs"));
        return -EINVAL;
    }
    if (wrqu->data.pointer == NULL)
    {
        hddLog(LOGE, FL("called with NULL IE"));
        return -EINVAL;
    }

    // Added for debug on reception of Re-assoc Req.
    if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
    {
        hddLog(LOGE, FL("Called with Ie of length = %d when not associated"),
            wrqu->data.length);
        hddLog(LOGE, FL("Should be Re-assoc Req IEs"));
    }

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    hddLog(LOG1, FL("%s called with Ie of length = %d"), __func__, wrqu->data.length);
#endif

    // Pass the received FT IEs to SME
    sme_SetFTIEs( WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId, extra,
        wrqu->data.length);

    return 0;
}
#endif

static int iw_set_dynamic_mcbc_filter(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tpRcvFltMcAddrList pRequest = (tpRcvFltMcAddrList)extra;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tpSirWlanSetRxpFilters wlanRxpFilterParam;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    tpSirRcvFltMcAddrList mc_addr_list_ptr;
    int idx;
    eHalStatus ret_val;

    if (pHddCtx->isLogpInProgress)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
       return -EBUSY;
    }
    if ((HDD_MULTICAST_FILTER_LIST == pRequest->mcastBcastFilterSetting) ||
        (HDD_MULTICAST_FILTER_LIST_CLEAR == pRequest->mcastBcastFilterSetting))
    {
#ifdef WLAN_FEATURE_PACKET_FILTERING

        mc_addr_list_ptr = vos_mem_malloc(sizeof(tSirRcvFltMcAddrList));
        if (NULL == mc_addr_list_ptr)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: vos_mem_alloc failed", __func__);
            return -ENOMEM;
        }

        mc_addr_list_ptr->ulMulticastAddrCnt = pRequest->mcast_addr_cnt;

        if (mc_addr_list_ptr->ulMulticastAddrCnt > HDD_MAX_NUM_MULTICAST_ADDRESS)
            mc_addr_list_ptr->ulMulticastAddrCnt = HDD_MAX_NUM_MULTICAST_ADDRESS;

        hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s MC Addr List Cnt %d", __func__,
               mc_addr_list_ptr->ulMulticastAddrCnt);

        for (idx = 0; idx < mc_addr_list_ptr->ulMulticastAddrCnt; idx++)
        {
            memcpy(&mc_addr_list_ptr->multicastAddr[idx],
                   pRequest->multicastAddr[idx], HDD_WLAN_MAC_ADDR_LEN);

            hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s MC Addr for Idx %d ="MAC_ADDRESS_STR, __func__,
                   idx, MAC_ADDR_ARRAY(mc_addr_list_ptr->multicastAddr[idx]));
        }

        if (HDD_MULTICAST_FILTER_LIST_CLEAR == pRequest->mcastBcastFilterSetting)
            mc_addr_list_ptr->action = HDD_DELETE_MCBC_FILTERS_FROM_FW; //clear
        else
            mc_addr_list_ptr->action = HDD_SET_MCBC_FILTERS_TO_FW; //set

        ret_val = sme_8023MulticastList(hHal, pAdapter->sessionId, mc_addr_list_ptr);
        vos_mem_free(mc_addr_list_ptr);
        if (eHAL_STATUS_SUCCESS != ret_val)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failure to Set MC Address List",
                   __func__);
            return -EINVAL;
        }
#endif //WLAN_FEATURE_PACKET_FILTERING
    }
    else
    {

        hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: Set MC BC Filter Config request: %d suspend %d",
               __func__, pRequest->mcastBcastFilterSetting,
               pHddCtx->hdd_wlan_suspended);

        pHddCtx->configuredMcastBcastFilter = pRequest->mcastBcastFilterSetting;

        if (pHddCtx->hdd_wlan_suspended)
        {
            wlanRxpFilterParam = vos_mem_malloc(sizeof(tSirWlanSetRxpFilters));
            if (NULL == wlanRxpFilterParam)
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                       "%s: vos_mem_alloc failed", __func__);
                return -EINVAL;
            }

            wlanRxpFilterParam->configuredMcstBcstFilterSetting =
                pRequest->mcastBcastFilterSetting;
            wlanRxpFilterParam->setMcstBcstFilter = TRUE;

            hdd_conf_hostoffload(pAdapter, TRUE);
            wlanRxpFilterParam->configuredMcstBcstFilterSetting =
                                pHddCtx->configuredMcastBcastFilter;

            hddLog(VOS_TRACE_LEVEL_INFO, "%s:MC/BC changed Req %d Set %d En %d",
                   __func__,
                   pHddCtx->configuredMcastBcastFilter,
                   wlanRxpFilterParam->configuredMcstBcstFilterSetting,
                   wlanRxpFilterParam->setMcstBcstFilter);

            if (eHAL_STATUS_SUCCESS !=
                    sme_ConfigureRxpFilter(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                           wlanRxpFilterParam))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                       "%s: Failure to execute set HW MC/BC Filter request",
                       __func__);
                vos_mem_free(wlanRxpFilterParam);
                return -EINVAL;
            }

        }
    }

    return 0;
}

static int iw_clear_dynamic_mcbc_filter(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tpSirWlanSetRxpFilters wlanRxpFilterParam;
    hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: ", __func__);

    if (!capable(CAP_NET_ADMIN)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("permission check failed"));
        return -EPERM;
    }

    //Reset the filter to INI value as we have to clear the dynamic filter
    pHddCtx->configuredMcastBcastFilter = pHddCtx->cfg_ini->mcastBcastFilterSetting;

    //Configure FW with new setting
    if (pHddCtx->hdd_wlan_suspended)
    {
        wlanRxpFilterParam = vos_mem_malloc(sizeof(tSirWlanSetRxpFilters));
        if (NULL == wlanRxpFilterParam)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: vos_mem_alloc failed", __func__);
            return -EINVAL;
        }

        wlanRxpFilterParam->configuredMcstBcstFilterSetting =
            pHddCtx->configuredMcastBcastFilter;
        wlanRxpFilterParam->setMcstBcstFilter = TRUE;

        hdd_conf_hostoffload(pAdapter, TRUE);
        wlanRxpFilterParam->configuredMcstBcstFilterSetting =
                            pHddCtx->configuredMcastBcastFilter;

        if (eHAL_STATUS_SUCCESS !=
                  sme_ConfigureRxpFilter(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                         wlanRxpFilterParam))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: Failure to execute set HW MC/BC Filter request",
                   __func__);
            vos_mem_free(wlanRxpFilterParam);
            return -EINVAL;
        }
    }
    return 0;
}

static int iw_set_host_offload(struct net_device *dev, struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tpHostOffloadRequest pRequest = (tpHostOffloadRequest) extra;
    tSirHostOffloadReq offloadRequest;

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
       return -EBUSY;
    }

    if (!hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter)))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP dev is not in CONNECTED state, ignore!!!", __func__);
       return -EINVAL;
    }

    /* Debug display of request components. */
    switch (pRequest->offloadType)
    {
        case WLAN_IPV4_ARP_REPLY_OFFLOAD:
            hddLog(VOS_TRACE_LEVEL_WARN, "%s: Host offload request: ARP reply", __func__);
            switch (pRequest->enableOrDisable)
            {
                case WLAN_OFFLOAD_DISABLE:
                    hddLog(VOS_TRACE_LEVEL_WARN, "   disable");
                    break;
                case WLAN_OFFLOAD_ARP_AND_BC_FILTER_ENABLE:
                    hddLog(VOS_TRACE_LEVEL_WARN, "   BC Filtering enable");
                case WLAN_OFFLOAD_ENABLE:
                    hddLog(VOS_TRACE_LEVEL_WARN, "   ARP offload enable");
                    hddLog(VOS_TRACE_LEVEL_WARN, "   IP address: %d.%d.%d.%d",
                            pRequest->params.hostIpv4Addr[0], pRequest->params.hostIpv4Addr[1],
                            pRequest->params.hostIpv4Addr[2], pRequest->params.hostIpv4Addr[3]);
            }
            break;

    case WLAN_IPV6_NEIGHBOR_DISCOVERY_OFFLOAD:
        hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: Host offload request: neighbor discovery",
               __func__);
        switch (pRequest->enableOrDisable)
        {
        case WLAN_OFFLOAD_DISABLE:
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "   disable");
            break;
        case WLAN_OFFLOAD_ENABLE:
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "   enable");
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "   IP address: %x:%x:%x:%x:%x:%x:%x:%x",
                   *(v_U16_t *)(pRequest->params.hostIpv6Addr),
                   *(v_U16_t *)(pRequest->params.hostIpv6Addr + 2),
                   *(v_U16_t *)(pRequest->params.hostIpv6Addr + 4),
                   *(v_U16_t *)(pRequest->params.hostIpv6Addr + 6),
                   *(v_U16_t *)(pRequest->params.hostIpv6Addr + 8),
                   *(v_U16_t *)(pRequest->params.hostIpv6Addr + 10),
                   *(v_U16_t *)(pRequest->params.hostIpv6Addr + 12),
                   *(v_U16_t *)(pRequest->params.hostIpv6Addr + 14));
        }
    }

    /* Execute offload request. The reason that we can copy the request information
       from the ioctl structure to the SME structure is that they are laid out
       exactly the same.  Otherwise, each piece of information would have to be
       copied individually. */
    memcpy(&offloadRequest, pRequest, wrqu->data.length);
    if (eHAL_STATUS_SUCCESS != sme_SetHostOffload(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                        pAdapter->sessionId, &offloadRequest))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failure to execute host offload request",
               __func__);
        return -EINVAL;
    }

    return 0;
}

static int iw_set_keepalive_params(struct net_device *dev, struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tpKeepAliveRequest pRequest = (tpKeepAliveRequest) extra;
    tSirKeepAliveReq keepaliveRequest;

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return 0;
    }

    if (pRequest->timePeriod > WNI_CFG_INFRA_STA_KEEP_ALIVE_PERIOD_STAMAX) {
        hddLog(LOGE, FL("Value of timePeriod exceed Max limit %d"),
               pRequest->timePeriod);
        return -EINVAL;
    }


    /* Debug display of request components. */
    hddLog(VOS_TRACE_LEVEL_INFO,
           "%s: Set Keep Alive Request : TimePeriod %d size %zu",
           __func__, pRequest->timePeriod, sizeof(tKeepAliveRequest));

      switch (pRequest->packetType)
      {
        case WLAN_KEEP_ALIVE_NULL_PKT:
            hddLog(VOS_TRACE_LEVEL_WARN, "%s: Keep Alive Request: Tx NULL", __func__);
            break;

        case WLAN_KEEP_ALIVE_UNSOLICIT_ARP_RSP:
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: Keep Alive Request: Tx UnSolicited ARP RSP",
               __func__);

            hddLog(VOS_TRACE_LEVEL_WARN, "  Host IP address: %d.%d.%d.%d",
            pRequest->hostIpv4Addr[0], pRequest->hostIpv4Addr[1],
            pRequest->hostIpv4Addr[2], pRequest->hostIpv4Addr[3]);

            hddLog(VOS_TRACE_LEVEL_WARN, "  Dest IP address: %d.%d.%d.%d",
            pRequest->destIpv4Addr[0], pRequest->destIpv4Addr[1],
            pRequest->destIpv4Addr[2], pRequest->destIpv4Addr[3]);

            hddLog(VOS_TRACE_LEVEL_WARN, "  Dest MAC address: %d:%d:%d:%d:%d:%d",
            pRequest->destMacAddr[0], pRequest->destMacAddr[1],
            pRequest->destMacAddr[2], pRequest->destMacAddr[3],
            pRequest->destMacAddr[4], pRequest->destMacAddr[5]);
            break;
      }

    /* Execute keep alive request. The reason that we can copy the request information
       from the ioctl structure to the SME structure is that they are laid out
       exactly the same.  Otherwise, each piece of information would have to be
       copied individually. */
       memcpy(&keepaliveRequest, pRequest, wrqu->data.length);

       hddLog(VOS_TRACE_LEVEL_ERROR, "set Keep: TP before SME %d", keepaliveRequest.timePeriod);

    if (eHAL_STATUS_SUCCESS != sme_SetKeepAlive(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                        pAdapter->sessionId, &keepaliveRequest))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failure to execute Keep Alive",
               __func__);
        return -EINVAL;
    }

    return 0;
}

#ifdef WLAN_FEATURE_PACKET_FILTERING
int wlan_hdd_set_filter(hdd_context_t *pHddCtx, tpPacketFilterCfg pRequest,
                            tANI_U8 sessionId)
{
    tSirRcvPktFilterCfgType    packetFilterSetReq = {0};
    tSirRcvFltPktClearParam    packetFilterClrReq = {0};
    int i=0;

    if (pHddCtx->cfg_ini->disablePacketFilter)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Packet Filtering Disabled. Returning ",
                __func__ );
        return 0;
    }
    if (pHddCtx->isLogpInProgress)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
       return -EBUSY;
    }
    /* Debug display of request components. */
    hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Packet Filter Request : FA %d params %d",
            __func__, pRequest->filterAction, pRequest->numParams);

    switch (pRequest->filterAction)
    {
        case HDD_RCV_FILTER_SET:
            hddLog(VOS_TRACE_LEVEL_INFO, "%s: Set Packet Filter Request for Id: %d",
                    __func__, pRequest->filterId);

            packetFilterSetReq.filterId = pRequest->filterId;
            if ( pRequest->numParams >= HDD_MAX_CMP_PER_PACKET_FILTER)
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Number of Params exceed Max limit %d",
                        __func__, pRequest->numParams);
                return -EINVAL;
            }
            packetFilterSetReq.numFieldParams = pRequest->numParams;
            packetFilterSetReq.coalesceTime = 0;
            packetFilterSetReq.filterType = 1;
            for (i=0; i < pRequest->numParams; i++)
            {
                packetFilterSetReq.paramsData[i].protocolLayer = pRequest->paramsData[i].protocolLayer;
                packetFilterSetReq.paramsData[i].cmpFlag = pRequest->paramsData[i].cmpFlag;
                packetFilterSetReq.paramsData[i].dataOffset = pRequest->paramsData[i].dataOffset;
                packetFilterSetReq.paramsData[i].dataLength = pRequest->paramsData[i].dataLength;
                packetFilterSetReq.paramsData[i].reserved = 0;

                hddLog(VOS_TRACE_LEVEL_INFO, "Proto %d Comp Flag %d Filter Type %d",
                        pRequest->paramsData[i].protocolLayer, pRequest->paramsData[i].cmpFlag,
                        packetFilterSetReq.filterType);

                hddLog(VOS_TRACE_LEVEL_INFO, "Data Offset %d Data Len %d",
                        pRequest->paramsData[i].dataOffset, pRequest->paramsData[i].dataLength);
                if ((sizeof(packetFilterSetReq.paramsData[i].compareData)) <
                           (pRequest->paramsData[i].dataLength))
                    return -EINVAL;

                memcpy(&packetFilterSetReq.paramsData[i].compareData,
                        pRequest->paramsData[i].compareData, pRequest->paramsData[i].dataLength);
                memcpy(&packetFilterSetReq.paramsData[i].dataMask,
                        pRequest->paramsData[i].dataMask, pRequest->paramsData[i].dataLength);

                hddLog(VOS_TRACE_LEVEL_INFO, "CData %d CData %d CData %d CData %d CData %d CData %d",
                        pRequest->paramsData[i].compareData[0], pRequest->paramsData[i].compareData[1],
                        pRequest->paramsData[i].compareData[2], pRequest->paramsData[i].compareData[3],
                        pRequest->paramsData[i].compareData[4], pRequest->paramsData[i].compareData[5]);

                hddLog(VOS_TRACE_LEVEL_INFO, "MData %d MData %d MData %d MData %d MData %d MData %d",
                        pRequest->paramsData[i].dataMask[0], pRequest->paramsData[i].dataMask[1],
                        pRequest->paramsData[i].dataMask[2], pRequest->paramsData[i].dataMask[3],
                        pRequest->paramsData[i].dataMask[4], pRequest->paramsData[i].dataMask[5]);
            }

            if (eHAL_STATUS_SUCCESS != sme_ReceiveFilterSetFilter(pHddCtx->hHal, &packetFilterSetReq, sessionId))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failure to execute Set Filter",
                        __func__);
                return -EINVAL;
            }

            break;

        case HDD_RCV_FILTER_CLEAR:

            hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: Clear Packet Filter Request for Id: %d",
                    __func__, pRequest->filterId);
            packetFilterClrReq.filterId = pRequest->filterId;
            if (eHAL_STATUS_SUCCESS != sme_ReceiveFilterClearFilter(pHddCtx->hHal, &packetFilterClrReq, sessionId))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Failure to execute Clear Filter",
                        __func__);
                return -EINVAL;
            }
            break;

        default :
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: Packet Filter Request: Invalid %d",
                    __func__, pRequest->filterAction);
            return -EINVAL;
    }
    return 0;
}

int wlan_hdd_setIPv6Filter(hdd_context_t *pHddCtx, tANI_U8 filterType,
                           tANI_U8 sessionId)
{
    tSirRcvPktFilterCfgType    packetFilterSetReq = {0};
    tSirRcvFltPktClearParam    packetFilterClrReq = {0};

    if (NULL == pHddCtx)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL(" NULL HDD Context Passed"));
        return -EINVAL;
    }

    if (pHddCtx->isLogpInProgress)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
       return -EBUSY;
    }

    if (pHddCtx->cfg_ini->disablePacketFilter)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Packet Filtering Disabled. Returning ",
                __func__ );
        return -EINVAL;
    }

    switch (filterType)
    {
        /* For setting IPV6 MC and UC Filter we need to configure
         * 2 filters, one for MC and one for UC.
         * The Filter ID shouldn't be swapped, which results in making
         * UC Filter ineffective.
         * We have hard coded all the values
         *
         * Reason for a separate UC filter is because, driver need to
         * specify the FW that the specific filter is for unicast
         * otherwise FW will not pass the unicast frames by default
         * through the filter. This is required to avoid any performance
         * hits when no unicast filter is set and only MC/BC are set.
         * The way driver informs host is by using the MAC protocol
         * layer, CMP flag set to MAX, CMP Data set to 1.
         */

    case HDD_FILTER_IPV6_MC_UC:
        /* Setting IPV6 MC Filter below
         */
        packetFilterSetReq.filterType = HDD_RCV_FILTER_SET;
        packetFilterSetReq.filterId = HDD_FILTER_ID_IPV6_MC;
        packetFilterSetReq.numFieldParams = 2;
        packetFilterSetReq.paramsData[0].protocolLayer =
                                         HDD_FILTER_PROTO_TYPE_MAC;
        packetFilterSetReq.paramsData[0].cmpFlag =
                                         HDD_FILTER_CMP_TYPE_NOT_EQUAL;
        packetFilterSetReq.paramsData[0].dataOffset =
                                         WLAN_HDD_80211_FRM_DA_OFFSET;
        packetFilterSetReq.paramsData[0].dataLength = 1;
        packetFilterSetReq.paramsData[0].compareData[0] =
                                         HDD_IPV6_MC_CMP_DATA;

        packetFilterSetReq.paramsData[1].protocolLayer =
                                         HDD_FILTER_PROTO_TYPE_ARP;
        packetFilterSetReq.paramsData[1].cmpFlag =
                                         HDD_FILTER_CMP_TYPE_NOT_EQUAL;
        packetFilterSetReq.paramsData[1].dataOffset = ETH_ALEN;
        packetFilterSetReq.paramsData[1].dataLength = 2;
        packetFilterSetReq.paramsData[1].compareData[0] =
                                         HDD_IPV6_CMP_DATA_0;
        packetFilterSetReq.paramsData[1].compareData[1] =
                                         HDD_IPV6_CMP_DATA_1;


        if (eHAL_STATUS_SUCCESS != sme_ReceiveFilterSetFilter(pHddCtx->hHal,
                                    &packetFilterSetReq, sessionId))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Failure to execute Set IPv6 Multicast Filter",
                    __func__);
            return -EINVAL;
        }

        memset( &packetFilterSetReq, 0, sizeof(tSirRcvPktFilterCfgType));

        /*
         * Setting IPV6 UC Filter below
         */
        packetFilterSetReq.filterType = HDD_RCV_FILTER_SET;
        packetFilterSetReq.filterId = HDD_FILTER_ID_IPV6_UC;
        packetFilterSetReq.numFieldParams = 2;
        packetFilterSetReq.paramsData[0].protocolLayer =
                                         HDD_FILTER_PROTO_TYPE_MAC;
        packetFilterSetReq.paramsData[0].cmpFlag =
                                         HDD_FILTER_CMP_TYPE_MAX;
        packetFilterSetReq.paramsData[0].dataOffset = 0;
        packetFilterSetReq.paramsData[0].dataLength = 1;
        packetFilterSetReq.paramsData[0].compareData[0] =
                                         HDD_IPV6_UC_CMP_DATA;

        packetFilterSetReq.paramsData[1].protocolLayer =
                                         HDD_FILTER_PROTO_TYPE_ARP;
        packetFilterSetReq.paramsData[1].cmpFlag =
                                         HDD_FILTER_CMP_TYPE_NOT_EQUAL;
        packetFilterSetReq.paramsData[1].dataOffset = ETH_ALEN;
        packetFilterSetReq.paramsData[1].dataLength = 2;
        packetFilterSetReq.paramsData[1].compareData[0] =
                                         HDD_IPV6_CMP_DATA_0;
        packetFilterSetReq.paramsData[1].compareData[1] =
                                         HDD_IPV6_CMP_DATA_1;

        if (eHAL_STATUS_SUCCESS != sme_ReceiveFilterSetFilter(pHddCtx->hHal,
                                    &packetFilterSetReq, sessionId))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Failure to execute Set IPv6 Unicast Filter",
                    __func__);
            return -EINVAL;
        }

        break;

    case HDD_FILTER_IPV6_MC:
        /*
         * IPV6 UC Filter might be already set,
         * clear the UC Filter. As the Filter
         * IDs are static, we can directly clear it.
         */
        packetFilterSetReq.filterType = HDD_RCV_FILTER_SET;
        packetFilterClrReq.filterId = HDD_FILTER_ID_IPV6_UC;
        if (eHAL_STATUS_SUCCESS != sme_ReceiveFilterClearFilter(pHddCtx->hHal,
                                    &packetFilterClrReq, sessionId))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Failure to execute Clear IPv6 Unicast Filter",
                    __func__);
            return -EINVAL;
        }

        /*
         * Setting IPV6 MC Filter below
         */
        packetFilterSetReq.filterId = HDD_FILTER_ID_IPV6_MC;
        packetFilterSetReq.numFieldParams = 2;
        packetFilterSetReq.paramsData[0].protocolLayer =
                                         HDD_FILTER_PROTO_TYPE_MAC;
        packetFilterSetReq.paramsData[0].cmpFlag =
                                         HDD_FILTER_CMP_TYPE_NOT_EQUAL;
        packetFilterSetReq.paramsData[0].dataOffset =
                                         WLAN_HDD_80211_FRM_DA_OFFSET;
        packetFilterSetReq.paramsData[0].dataLength = 1;
        packetFilterSetReq.paramsData[0].compareData[0] =
                                         HDD_IPV6_MC_CMP_DATA;

        packetFilterSetReq.paramsData[1].protocolLayer =
                                         HDD_FILTER_PROTO_TYPE_ARP;
        packetFilterSetReq.paramsData[1].cmpFlag =
                                         HDD_FILTER_CMP_TYPE_NOT_EQUAL;
        packetFilterSetReq.paramsData[1].dataOffset = ETH_ALEN;
        packetFilterSetReq.paramsData[1].dataLength = 2;
        packetFilterSetReq.paramsData[1].compareData[0] =
                                         HDD_IPV6_CMP_DATA_0;
        packetFilterSetReq.paramsData[1].compareData[1] =
                                         HDD_IPV6_CMP_DATA_1;


        if (eHAL_STATUS_SUCCESS != sme_ReceiveFilterSetFilter(pHddCtx->hHal,
                                    &packetFilterSetReq, sessionId))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Failure to execute Set IPv6 Multicast Filter",
                    __func__);
            return -EINVAL;
        }
        break;

    default :
        hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: Packet Filter Request: Invalid",
                __func__);
        return -EINVAL;
    }
    return 0;
}

void wlan_hdd_set_mc_addr_list(hdd_adapter_t *pAdapter, v_U8_t set)
{
    v_U8_t i;
    tpSirRcvFltMcAddrList pMulticastAddrs = NULL;
    tHalHandle hHal = NULL;
    hdd_context_t* pHddCtx = (hdd_context_t*)pAdapter->pHddCtx;

    if (NULL == pHddCtx)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD CTX is NULL"));
        return;
    }

    hHal = pHddCtx->hHal;

    if (NULL == hHal)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HAL Handle is NULL"));
        return;
    }

    /* Check if INI is enabled or not, other wise just return
     */
    if (pHddCtx->cfg_ini->fEnableMCAddrList)
    {
        pMulticastAddrs = vos_mem_malloc(sizeof(tSirRcvFltMcAddrList));
        if (NULL == pMulticastAddrs)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("Could not allocate Memory"));
            return;
        }
        vos_mem_zero(pMulticastAddrs, sizeof(tSirRcvFltMcAddrList));
        pMulticastAddrs->action = set;

        if (set)
        {
            /* Following pre-conditions should be satisfied before we
             * configure the MC address list.
             */
            if (((pAdapter->device_mode == WLAN_HDD_INFRA_STATION) ||
               (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT))
               && pAdapter->mc_addr_list.mc_cnt
               && (eConnectionState_Associated ==
               (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState))
            {
                pMulticastAddrs->ulMulticastAddrCnt =
                                 pAdapter->mc_addr_list.mc_cnt;
                for (i = 0; i < pAdapter->mc_addr_list.mc_cnt; i++)
                {
                    memcpy(pMulticastAddrs->multicastAddr[i],
                           pAdapter->mc_addr_list.addr[i],
                           sizeof(pAdapter->mc_addr_list.addr[i]));
                    hddLog(VOS_TRACE_LEVEL_INFO,
                            "%s: %s multicast filter: addr ="
                            MAC_ADDRESS_STR,
                            __func__, set ? "setting" : "clearing",
                            MAC_ADDR_ARRAY(pMulticastAddrs->multicastAddr[i]));
                }
                /* Set multicast filter */
                sme_8023MulticastList(hHal, pAdapter->sessionId,
                                      pMulticastAddrs);
            }
        }
        else
        {
            /* Need to clear only if it was previously configured
             */
            if (pAdapter->mc_addr_list.isFilterApplied)
            {
                pMulticastAddrs->ulMulticastAddrCnt =
                                 pAdapter->mc_addr_list.mc_cnt;
                for (i = 0; i < pAdapter->mc_addr_list.mc_cnt; i++) {
                    memcpy(pMulticastAddrs->multicastAddr[i],
                           pAdapter->mc_addr_list.addr[i],
                           sizeof(pAdapter->mc_addr_list.addr[i]));
                }
                sme_8023MulticastList(hHal, pAdapter->sessionId,
                                      pMulticastAddrs);
            }

        }
        /* MAddrCnt is MulticastAddrCnt */
        hddLog(VOS_TRACE_LEVEL_INFO, "smeSessionId:%d; set:%d; MCAdddrCnt :%d",
              pAdapter->sessionId, set, pMulticastAddrs->ulMulticastAddrCnt);

        pAdapter->mc_addr_list.isFilterApplied = set ? TRUE : FALSE;
        vos_mem_free(pMulticastAddrs);
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_INFO,
                FL("gMCAddrListEnable is not enabled in INI"));
    }
    return;
}

static int iw_set_packet_filter_params(struct net_device *dev,
                                       struct iw_request_info *info,
                                       union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tpPacketFilterCfg pRequest = NULL;
    int ret;
    struct iw_point s_priv_data;

    if (!capable(CAP_NET_ADMIN)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("permission check failed"));
        return -EPERM;
    }

    if (hdd_priv_get_data(&s_priv_data, wrqu)) {
       return -EINVAL;
    }

    if ((NULL == s_priv_data.pointer) || (0 == s_priv_data.length)) {
       return -EINVAL;
    }

    /* ODD number is used for set, copy data using copy_from_user */
    pRequest = mem_alloc_copy_from_user_helper(s_priv_data.pointer,
                                               s_priv_data.length);
    if (NULL == pRequest) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "mem_alloc_copy_from_user_helper fail");
        return -ENOMEM;
    }

    ret = wlan_hdd_set_filter(WLAN_HDD_GET_CTX(pAdapter), pRequest,
                              pAdapter->sessionId);
    kfree(pRequest);

    return ret;
}
#endif
static int iw_get_statistics(struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu, char *extra)
{

  VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
  eHalStatus status = eHAL_STATUS_SUCCESS;
  hdd_wext_state_t *pWextState;
  hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
  hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
  char *p = extra;
  int tlen = 0;
  tCsrSummaryStatsInfo *pStats = &(pAdapter->hdd_stats.summary_stat);

  tCsrGlobalClassAStatsInfo *aStats = &(pAdapter->hdd_stats.ClassA_stat);
  tCsrGlobalClassDStatsInfo *dStats = &(pAdapter->hdd_stats.ClassD_stat);

  ENTER();

  if (pHddCtx->isLogpInProgress) {
     VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL, "%s:LOGP in Progress. Ignore!!!",__func__);
     return -EINVAL;
  }

  if (eConnectionState_Associated != (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState) {

     wrqu->txpower.value = 0;
  }
  else {
    status = sme_GetStatistics( pHddCtx->hHal, eCSR_HDD,
                       SME_SUMMARY_STATS      |
                       SME_GLOBAL_CLASSA_STATS |
                       SME_GLOBAL_CLASSB_STATS |
                       SME_GLOBAL_CLASSC_STATS |
                       SME_GLOBAL_CLASSD_STATS |
                       SME_PER_STA_STATS,
                       hdd_StatisticsCB, 0, FALSE,
                       (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                       pAdapter,
                       pAdapter->sessionId );

    if (eHAL_STATUS_SUCCESS != status)
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: Unable to retrieve SME statistics",
              __func__);
        return -EINVAL;
    }

    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

    vos_status = vos_wait_single_event(&pWextState->vosevent, WLAN_WAIT_TIME_STATS);
    if (!VOS_IS_STATUS_SUCCESS(vos_status))
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: SME timeout while retrieving statistics",
              __func__);
       /*Remove the SME statistics list by passing NULL in callback argument*/
       status = sme_GetStatistics( pHddCtx->hHal, eCSR_HDD,
                       SME_SUMMARY_STATS      |
                       SME_GLOBAL_CLASSA_STATS |
                       SME_GLOBAL_CLASSB_STATS |
                       SME_GLOBAL_CLASSC_STATS |
                       SME_GLOBAL_CLASSD_STATS |
                       SME_PER_STA_STATS,
                       NULL, 0, FALSE,
                       (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.staId[0],
                       pAdapter,
                       pAdapter->sessionId );

       return -EINVAL;
    }
    FILL_TLV(p, (tANI_U8)WLAN_STATS_RETRY_CNT,
              (tANI_U8) sizeof (pStats->retry_cnt),
              (char*) &(pStats->retry_cnt[0]),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_MUL_RETRY_CNT,
              (tANI_U8) sizeof (pStats->multiple_retry_cnt),
              (char*) &(pStats->multiple_retry_cnt[0]),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_TX_FRM_CNT,
              (tANI_U8) sizeof (pStats->tx_frm_cnt),
              (char*) &(pStats->tx_frm_cnt[0]),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_RX_FRM_CNT,
              (tANI_U8) sizeof (pStats->rx_frm_cnt),
              (char*) &(pStats->rx_frm_cnt),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_FRM_DUP_CNT,
              (tANI_U8) sizeof (pStats->frm_dup_cnt),
              (char*) &(pStats->frm_dup_cnt),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_FAIL_CNT,
              (tANI_U8) sizeof (pStats->fail_cnt),
              (char*) &(pStats->fail_cnt[0]),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_RTS_FAIL_CNT,
              (tANI_U8) sizeof (pStats->rts_fail_cnt),
              (char*) &(pStats->rts_fail_cnt),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_ACK_FAIL_CNT,
              (tANI_U8) sizeof (pStats->ack_fail_cnt),
              (char*) &(pStats->ack_fail_cnt),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_RTS_SUC_CNT,
              (tANI_U8) sizeof (pStats->rts_succ_cnt),
              (char*) &(pStats->rts_succ_cnt),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_RX_DISCARD_CNT,
              (tANI_U8) sizeof (pStats->rx_discard_cnt),
              (char*) &(pStats->rx_discard_cnt),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_RX_ERROR_CNT,
              (tANI_U8) sizeof (pStats->rx_error_cnt),
              (char*) &(pStats->rx_error_cnt),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_TX_BYTE_CNT,
              (tANI_U8) sizeof (dStats->tx_uc_byte_cnt[0]),
              (char*) &(dStats->tx_uc_byte_cnt[0]),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_RX_BYTE_CNT,
              (tANI_U8) sizeof (dStats->rx_byte_cnt),
              (char*) &(dStats->rx_byte_cnt),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_RX_RATE,
              (tANI_U8) sizeof (dStats->rx_rate),
              (char*) &(dStats->rx_rate),
              tlen);

    /* Transmit rate, in units of 500 kbit/sec */
    FILL_TLV(p, (tANI_U8)WLAN_STATS_TX_RATE,
              (tANI_U8) sizeof (aStats->tx_rate),
              (char*) &(aStats->tx_rate),
              tlen);

    FILL_TLV(p, (tANI_U8)WLAN_STATS_RX_UC_BYTE_CNT,
              (tANI_U8) sizeof (dStats->rx_uc_byte_cnt[0]),
              (char*) &(dStats->rx_uc_byte_cnt[0]),
              tlen);
    FILL_TLV(p, (tANI_U8)WLAN_STATS_RX_MC_BYTE_CNT,
              (tANI_U8) sizeof (dStats->rx_mc_byte_cnt),
              (char*) &(dStats->rx_mc_byte_cnt),
              tlen);
    FILL_TLV(p, (tANI_U8)WLAN_STATS_RX_BC_BYTE_CNT,
              (tANI_U8) sizeof (dStats->rx_bc_byte_cnt),
              (char*) &(dStats->rx_bc_byte_cnt),
              tlen);
    FILL_TLV(p, (tANI_U8)WLAN_STATS_TX_UC_BYTE_CNT,
              (tANI_U8) sizeof (dStats->tx_uc_byte_cnt[0]),
              (char*) &(dStats->tx_uc_byte_cnt[0]),
              tlen);
    FILL_TLV(p, (tANI_U8)WLAN_STATS_TX_MC_BYTE_CNT,
              (tANI_U8) sizeof (dStats->tx_mc_byte_cnt),
              (char*) &(dStats->tx_mc_byte_cnt),
              tlen);
    FILL_TLV(p, (tANI_U8)WLAN_STATS_TX_BC_BYTE_CNT,
              (tANI_U8) sizeof (dStats->tx_bc_byte_cnt),
              (char*) &(dStats->tx_bc_byte_cnt),
              tlen);

    wrqu->data.length = tlen;

  }

  EXIT();

  return 0;
}


#ifdef FEATURE_WLAN_SCAN_PNO

/*Max Len for PNO notification*/
#define MAX_PNO_NOTIFY_LEN 100
void found_pref_network_cb (void *callbackContext,
                              tSirPrefNetworkFoundInd *pPrefNetworkFoundInd)
{
  hdd_adapter_t* pAdapter = (hdd_adapter_t*)callbackContext;
  union iwreq_data wrqu;
  char buf[MAX_PNO_NOTIFY_LEN+1];

  hddLog(VOS_TRACE_LEVEL_WARN, "A preferred network was found: %s with rssi: -%d",
         pPrefNetworkFoundInd->ssId.ssId, pPrefNetworkFoundInd->rssi);

  // create the event
  memset(&wrqu, 0, sizeof(wrqu));
  memset(buf, 0, sizeof(buf));

  snprintf(buf, MAX_PNO_NOTIFY_LEN, "QCOM: Found preferred network: %s with RSSI of -%u",
           pPrefNetworkFoundInd->ssId.ssId,
          (unsigned int)pPrefNetworkFoundInd->rssi);

  wrqu.data.pointer = buf;
  wrqu.data.length = strlen(buf);

  // send the event

  wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, buf);

}


/*string based input*/
VOS_STATUS iw_set_pno(struct net_device *dev, struct iw_request_info *info,
                      union iwreq_data *wrqu, char *extra, int nOffset)
{
  hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
  /* pnoRequest is a large struct, so we make it static to avoid stack
     overflow.  This API is only invoked via ioctl, so it is
     serialized by the kernel rtnl_lock and hence does not need to be
     reentrant */
  static tSirPNOScanReq pnoRequest;
  char *ptr;
  v_U8_t i,j, ucParams, ucMode;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "PNO data len %d data %s",
            wrqu->data.length,
            extra);

  if (wrqu->data.length <= nOffset )
  {
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN, "PNO input is not correct");
    return VOS_STATUS_E_FAILURE;
  }

  pnoRequest.enable = 0;
  pnoRequest.ucNetworksCount = 0;
  /*-----------------------------------------------------------------------
    Input is string based and expected to be like this:

    <enabled> <netw_count>
    for each network:
    <ssid_len> <ssid> <authentication> <encryption>
    <ch_num> <channel_list optional> <bcast_type> <rssi_threshold>
    <scan_timers> <scan_time> <scan_repeat> <scan_time> <scan_repeat>

    e.g:
    1 2 4 test 0 0 3 1 6 11 2 40 5 test2 4 4 6 1 2 3 4 5 6 1 0 2 5 2 300 0

    this translates into:
    -----------------------------
    enable PNO
    look for 2 networks:
    test - with authentication type 0 and encryption type 0,
    that can be found on 3 channels: 1 6 and 11 ,
    SSID bcast type is unknown (directed probe will be sent if AP not found)
    and must meet -40dBm RSSI

    test2 - with auth and encryption type 4/4
    that can be found on 6 channels 1, 2, 3, 4, 5 and 6
    bcast type is non-bcast (directed probe will be sent)
    and must not meet any RSSI threshold

    scan every 5 seconds 2 times, scan every 300 seconds until stopped
  -----------------------------------------------------------------------*/
  ptr = extra + nOffset;

  if (1 != sscanf(ptr,"%hhu%n", &(pnoRequest.enable), &nOffset))
  {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "PNO enable input is not valid %s",ptr);
      return VOS_STATUS_E_FAILURE;
  }

  if ( 0 == pnoRequest.enable )
  {
    /*Disable PNO*/
    memset(&pnoRequest, 0, sizeof(pnoRequest));
    sme_SetPreferredNetworkList(WLAN_HDD_GET_HAL_CTX(pAdapter), &pnoRequest,
                                pAdapter->sessionId,
                                found_pref_network_cb, pAdapter);
    return VOS_STATUS_SUCCESS;
  }

  ptr += nOffset;

  if (1 != sscanf(ptr,"%hhu %n", &(pnoRequest.ucNetworksCount), &nOffset))
  {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "PNO count input not valid %s",ptr);
      return VOS_STATUS_E_FAILURE;

  }

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "PNO enable %d networks count %d offset %d",
            pnoRequest.enable,
            pnoRequest.ucNetworksCount,
            nOffset);

  /* Parameters checking:
      ucNetworksCount has to be larger than 0*/
  if (( 0 == pnoRequest.ucNetworksCount ) ||
      ( pnoRequest.ucNetworksCount > SIR_PNO_MAX_SUPP_NETWORKS ))
  {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN, "Network input is not correct");
      return VOS_STATUS_E_FAILURE;
  }

  ptr += nOffset;

  for ( i = 0; i < pnoRequest.ucNetworksCount; i++ )
  {

    pnoRequest.aNetworks[i].ssId.length = 0;

    ucParams = sscanf(ptr,"%hhu %n",
                      &(pnoRequest.aNetworks[i].ssId.length),&nOffset);

    if (1 != ucParams)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "PNO ssid length input is not valid %s",ptr);
        return VOS_STATUS_E_FAILURE;
    }

    if (( 0 == pnoRequest.aNetworks[i].ssId.length ) ||
        ( pnoRequest.aNetworks[i].ssId.length > 32 ) )
    {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "SSID Len %d is not correct for network %d",
                pnoRequest.aNetworks[i].ssId.length, i);
      return VOS_STATUS_E_FAILURE;
    }

    /*Advance to SSID*/
    ptr += nOffset;

    memcpy(pnoRequest.aNetworks[i].ssId.ssId, ptr,
           pnoRequest.aNetworks[i].ssId.length);
    ptr += pnoRequest.aNetworks[i].ssId.length;

    ucParams = sscanf(ptr,"%u %u %hhu %n",
                      &(pnoRequest.aNetworks[i].authentication),
                      &(pnoRequest.aNetworks[i].encryption),
                      &(pnoRequest.aNetworks[i].ucChannelCount),
                      &nOffset);

    if ( 3 != ucParams )
    {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                "Incorrect cmd %s",ptr);
      return VOS_STATUS_E_FAILURE;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "PNO len %d ssid 0x%08x%08x%08x%08x%08x%08x%08x%08x"
              "auth %d encry %d channel count %d offset %d",
              pnoRequest.aNetworks[i].ssId.length,
              *((v_U32_t *) &pnoRequest.aNetworks[i].ssId.ssId[0]),
              *((v_U32_t *) &pnoRequest.aNetworks[i].ssId.ssId[4]),
              *((v_U32_t *) &pnoRequest.aNetworks[i].ssId.ssId[8]),
              *((v_U32_t *) &pnoRequest.aNetworks[i].ssId.ssId[12]),
              *((v_U32_t *) &pnoRequest.aNetworks[i].ssId.ssId[16]),
              *((v_U32_t *) &pnoRequest.aNetworks[i].ssId.ssId[20]),
              *((v_U32_t *) &pnoRequest.aNetworks[i].ssId.ssId[24]),
              *((v_U32_t *) &pnoRequest.aNetworks[i].ssId.ssId[28]),
              pnoRequest.aNetworks[i].authentication,
              pnoRequest.aNetworks[i].encryption,
              pnoRequest.aNetworks[i].ucChannelCount,
              nOffset );

    /*Advance to channel list*/
    ptr += nOffset;

    if (SIR_PNO_MAX_NETW_CHANNELS < pnoRequest.aNetworks[i].ucChannelCount)
    {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                "Incorrect number of channels");
      return VOS_STATUS_E_FAILURE;
    }

    if ( 0 !=  pnoRequest.aNetworks[i].ucChannelCount)
    {
      for ( j = 0; j < pnoRequest.aNetworks[i].ucChannelCount; j++)
      {
           if (1 != sscanf(ptr,"%hhu %n",
                           &(pnoRequest.aNetworks[i].aChannels[j]),
                           &nOffset))
            {    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           "PNO network channel input is not valid %s",ptr);
                  return VOS_STATUS_E_FAILURE;
            }
            /*Advance to next channel number*/
            ptr += nOffset;
      }
    }

    if (1 != sscanf(ptr,"%u %n",
                    &(pnoRequest.aNetworks[i].bcastNetwType),
                    &nOffset))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "PNO broadcast network type input is not valid %s",ptr);
        return VOS_STATUS_E_FAILURE;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "PNO bcastNetwType %d offset %d",
            pnoRequest.aNetworks[i].bcastNetwType,
            nOffset );

    /*Advance to rssi Threshold*/
    ptr += nOffset;
    if (1 != sscanf(ptr,"%d %n",
                    &(pnoRequest.aNetworks[i].rssiThreshold),
                    &nOffset))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "PNO rssi threshold input is not valid %s",ptr);
        return VOS_STATUS_E_FAILURE;
    }
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "PNO rssi %d offset %d",
            pnoRequest.aNetworks[i].rssiThreshold,
            nOffset );
    /*Advance to next network*/
    ptr += nOffset;
  }/*For ucNetworkCount*/

  ucParams = sscanf(ptr,"%hhu %n",&(ucMode), &nOffset);

  pnoRequest.modePNO = ucMode;
  /*for LA we just expose suspend option*/
  if (( 1 != ucParams )||(  ucMode >= SIR_PNO_MODE_MAX ))
  {
     pnoRequest.modePNO = SIR_PNO_MODE_ON_SUSPEND;
  }

  sme_SetPreferredNetworkList(WLAN_HDD_GET_HAL_CTX(pAdapter), &pnoRequest,
                                pAdapter->sessionId,
                                found_pref_network_cb, pAdapter);

  return VOS_STATUS_SUCCESS;
}/*iw_set_pno*/

VOS_STATUS iw_set_rssi_filter(struct net_device *dev, struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra, int nOffset)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    v_U8_t rssiThreshold = 0;
    v_U8_t nRead;

    nRead = sscanf(extra + nOffset,"%hhu",
           &rssiThreshold);

    if ( 1 != nRead )
    {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                "Incorrect format");
      return VOS_STATUS_E_FAILURE;
    }

    sme_SetRSSIFilter(WLAN_HDD_GET_HAL_CTX(pAdapter), rssiThreshold);
    return VOS_STATUS_SUCCESS;
}


static int iw_set_pno_priv(struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "Set PNO Private");

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
       return -EBUSY;
    }
    return iw_set_pno(dev,info,wrqu,extra,0);
}
#endif /*FEATURE_WLAN_SCAN_PNO*/

//Common function to SetBand
int hdd_setBand(struct net_device *dev, u8 ui_band)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    eCsrBand band;

    VOS_STATUS status;
    hdd_context_t *pHddCtx;
    hdd_adapter_list_node_t *pAdapterNode, *pNext;
    eCsrBand currBand = eCSR_BAND_MAX;
    eCsrBand connectedBand;

    pAdapterNode = NULL;
    pNext = NULL;
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    switch(ui_band)
    {
        case WLAN_HDD_UI_BAND_AUTO:
             band = eCSR_BAND_ALL;
        break;
        case WLAN_HDD_UI_BAND_5_GHZ:
            band = eCSR_BAND_5G;
        break;
        case WLAN_HDD_UI_BAND_2_4_GHZ:
            band = eCSR_BAND_24;
        break;
        default:
            band = eCSR_BAND_MAX;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: change band to %u",
                __func__, band);

    if (band == eCSR_BAND_MAX)
    {
        /* Received change band request with invalid band value */
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: Invalid band value %u", __func__, ui_band);
        return -EINVAL;
    }

    if ((band == eCSR_BAND_24 && pHddCtx->cfg_ini->nBandCapability == 2) ||
        (band == eCSR_BAND_5G && pHddCtx->cfg_ini->nBandCapability == 1)) {
        hddLog(LOGP, FL("band value %u violate INI settings %u"),
               band, pHddCtx->cfg_ini->nBandCapability);
        return -EIO;
    }

    if (band == eCSR_BAND_ALL) {
        hddLog(LOG1,
               FL("Auto band received. Setting band same as ini value %d"),
               pHddCtx->cfg_ini->nBandCapability);
        band = pHddCtx->cfg_ini->nBandCapability;
    }

    if (eHAL_STATUS_SUCCESS != sme_GetFreqBand(hHal, &currBand))
    {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s: Failed to get current band config",
                 __func__);
         return -EIO;
    }

    if (currBand != band)
    {
        /* Change band request received.
         * Abort pending scan requests, flush the existing scan results,
         * and change the band capability
         */
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s: Current band value = %u, new setting %u ",
                 __func__, currBand, band);

        status =  hdd_get_front_adapter(pHddCtx, &pAdapterNode);
        while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
        {
            pAdapter = pAdapterNode->pAdapter;
            hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
            hdd_abort_mac_scan(pHddCtx, pAdapter->sessionId,
                           eCSR_SCAN_ABORT_DUE_TO_BAND_CHANGE);
            connectedBand =
                hdd_connGetConnectedBand(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter));

            /* Handling is done only for STA and P2P */
            if ( band != eCSR_BAND_ALL &&
                 ((pAdapter->device_mode == WLAN_HDD_INFRA_STATION) ||
                 (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)) &&
                 (hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))) &&
                 (connectedBand != band))
            {
                 eHalStatus status = eHAL_STATUS_SUCCESS;
                 long lrc;

                 /* STA already connected on current band, So issue disconnect
                  * first, then change the band*/

                 hddLog(LOG1,
                        FL("STA Device mode (%d) connected band %u, Changing band to %u, Issuing Disconnect"),
                        pAdapter->device_mode, currBand, band);

                 INIT_COMPLETION(pAdapter->disconnect_comp_var);

                 status = sme_RoamDisconnect( WLAN_HDD_GET_HAL_CTX(pAdapter),
                 pAdapter->sessionId, eCSR_DISCONNECT_REASON_UNSPECIFIED);

                 if ( eHAL_STATUS_SUCCESS != status)
                 {
                     hddLog(VOS_TRACE_LEVEL_ERROR,
                             "%s csrRoamDisconnect failure, returned %d",
                               __func__, (int)status );
                     return -EINVAL;
                 }

                 lrc = wait_for_completion_timeout(
                         &pAdapter->disconnect_comp_var,
                         msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));

                 if (lrc == 0) {
                    hddLog(VOS_TRACE_LEVEL_ERROR,"%s:Timeout while waiting for csrRoamDisconnect",
                                __func__);
                    return -ETIMEDOUT ;
                 }
            }

            sme_ScanFlushResult(hHal, pAdapter->sessionId);

            status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
            pAdapterNode = pNext;
        }

        if (eHAL_STATUS_SUCCESS != sme_SetFreqBand(hHal, pAdapter->sessionId,
                                                   band)) {
             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                     FL("Failed to set the band value to %u"), band);
             return -EINVAL;
        }
        wlan_hdd_cfg80211_update_band(pHddCtx->wiphy, (eCsrBand)band);
    }
    return 0;
}

int hdd_setBand_helper(struct net_device *dev, const char *command)
{
    u8 band;
    int ret;

    /* Convert the band value from ascii to integer */
    command += WLAN_HDD_UI_SET_BAND_VALUE_OFFSET;
    ret = kstrtou8(command, 10, &band);
    if (ret < 0) {
        hddLog(LOGE, FL("kstrtou8 failed"));
        return -EINVAL;
    }

    return hdd_setBand(dev, band);
}

static int iw_set_band_config(struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    int *value = (int *)extra;

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: ", __func__);

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EBUSY;
    }

    if (!capable(CAP_NET_ADMIN)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("permission check failed"));
        return -EPERM;
    }

    return hdd_setBand(dev, value[0]);
}

static int iw_set_power_params_priv(struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu, char *extra)
{
  int ret;
  char *ptr;
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "Set power params Private");

  if (!capable(CAP_NET_ADMIN)) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("permission check failed"));
      return -EPERM;
  }
  /* ODD number is used for set, copy data using copy_from_user */
  ptr = mem_alloc_copy_from_user_helper(wrqu->data.pointer,
                                        wrqu->data.length);
  if (NULL == ptr)
  {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "mem_alloc_copy_from_user_helper fail");
      return -ENOMEM;
  }

  ret = iw_set_power_params(dev, info, wrqu, ptr, 0);
  kfree(ptr);
  return ret;
}



/*string based input*/
VOS_STATUS iw_set_power_params(struct net_device *dev, struct iw_request_info *info,
                      union iwreq_data *wrqu, char *extra, int nOffset)
{
  hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
  tSirSetPowerParamsReq powerRequest;
  char *ptr;
  v_U8_t  ucType;
  v_U32_t  uTotalSize, uValue;
  /*- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "Power Params data len %d data %s",
            wrqu->data.length,
            extra);

  if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress)
  {
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
    return -EBUSY;
  }

  if (wrqu->data.length <= nOffset )
  {
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN, "set power param input is not correct");
    return VOS_STATUS_E_FAILURE;
  }

  uTotalSize = wrqu->data.length - nOffset;

  /*-----------------------------------------------------------------------
    Input is string based and expected to be like this:

    <param_type> <param_value> <param_type> <param_value> ...

    e.g:
    1 2 2 3 3 0 4 1 5 1

    e.g. setting just a few:
    1 2 4 1

    parameter types:
    -----------------------------
    1 - Ignore DTIM
    2 - Listen Interval
    3 - Broadcast Multicast Filter
    4 - Beacon Early Termination
    5 - Beacon Early Termination Interval
  -----------------------------------------------------------------------*/
  powerRequest.uIgnoreDTIM       = SIR_NOCHANGE_POWER_VALUE;
  powerRequest.uListenInterval   = SIR_NOCHANGE_POWER_VALUE;
  powerRequest.uBcastMcastFilter = SIR_NOCHANGE_POWER_VALUE;
  powerRequest.uEnableBET        = SIR_NOCHANGE_POWER_VALUE;
  powerRequest.uBETInterval      = SIR_NOCHANGE_POWER_VALUE;

  ptr = extra + nOffset;

  while ( uTotalSize )
  {
    if (1 != sscanf(ptr,"%hhu %n", &(ucType), &nOffset))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "Invalid input parameter type %s",ptr);
         return VOS_STATUS_E_FAILURE;
    }

    uTotalSize -= nOffset;

    if (!uTotalSize)
    {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid input parameter type : %d with no value at offset %d",
                ucType, nOffset);
      return VOS_STATUS_E_FAILURE;
    }

    ptr += nOffset;

    if (1 != sscanf(ptr,"%u %n", &(uValue), &nOffset))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "Invalid input parameter value %s",ptr);
         return VOS_STATUS_E_FAILURE;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "Power request parameter %d value %d offset %d",
              ucType, uValue, nOffset);

    switch (ucType)
    {
      case eSIR_IGNORE_DTIM:
      powerRequest.uIgnoreDTIM       = uValue;
      break;
      case eSIR_LISTEN_INTERVAL:
      powerRequest.uListenInterval   = uValue;
      break;
      case eSIR_MCAST_BCAST_FILTER:
      powerRequest.uBcastMcastFilter = uValue;
      break;
      case eSIR_ENABLE_BET:
      powerRequest.uEnableBET        = uValue;
      break;
      case eSIR_BET_INTERVAL:
      powerRequest.uBETInterval      = uValue;
      break;
      default:
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Invalid input parameter type : %d with value: %d at offset %d",
                ucType, uValue,  nOffset);
      return VOS_STATUS_E_FAILURE;
    }

    uTotalSize -= nOffset;
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "Power request parameter %d Total size",
              uTotalSize);
    ptr += nOffset;
    /* This is added for dynamic Tele LI enable (0xF1) /disable (0xF0)*/
    if(!(uTotalSize - nOffset) &&
       (powerRequest.uListenInterval != SIR_NOCHANGE_POWER_VALUE))
    {
        uTotalSize = 0;
    }

  }/*Go for as long as we have a valid string*/

  /* put the device into full power*/
  wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_ACTIVE);

  /* Apply the power save params*/
  sme_SetPowerParams( WLAN_HDD_GET_HAL_CTX(pAdapter), &powerRequest, FALSE);

  /* put the device back to power save*/
  wlan_hdd_enter_bmps(pAdapter, DRIVER_POWER_MODE_AUTO);

  return VOS_STATUS_SUCCESS;
}/*iw_set_power_params*/

int iw_set_two_ints_getnone(struct net_device *dev,
                            struct iw_request_info *info,
                            union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    int *value = (int *)extra;
    int sub_cmd = value[0];
    int ret = 0;

    if ((WLAN_HDD_GET_CTX(pAdapter))->isLogpInProgress) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                                  "%s:LOGP in Progress. Ignore!!!", __func__);
        return -EBUSY;
    }

    switch(sub_cmd) {
    case WE_SET_SMPS_PARAM:
        hddLog(LOG1, "WE_SET_SMPS_PARAM val %d %d", value[1], value[2]);
        ret = process_wma_set_command((int)pAdapter->sessionId,
                       (int)WMI_STA_SMPS_PARAM_CMDID,
                       value[1] << WMA_SMPS_PARAM_VALUE_S | value[2], VDEV_CMD);
        break;
#ifdef DEBUG
    case WE_SET_FW_CRASH_INJECT:
        hddLog(LOGE, "WE_SET_FW_CRASH_INJECT: %d %d", value[1], value[2]);
        pr_err("SSR is triggered by iwpriv CRASH_INJECT: %d %d\n",
                                                value[1], value[2]);
        ret = process_wma_set_command_twoargs((int) pAdapter->sessionId,
                                              (int) GEN_PARAM_CRASH_INJECT,
                                              value[1], value[2], GEN_CMD);
        break;
#endif
    default:
        hddLog(LOGE, "%s: Invalid IOCTL command %d", __func__, sub_cmd);
        break;
    }

    return ret;
}


// Define the Wireless Extensions to the Linux Network Device structure
// A number of these routines are NULL (meaning they are not implemented.)

static const iw_handler      we_handler[] =
{
   (iw_handler) iw_set_commit,      /* SIOCSIWCOMMIT */
   (iw_handler) iw_get_name,      /* SIOCGIWNAME */
   (iw_handler) NULL,            /* SIOCSIWNWID */
   (iw_handler) NULL,            /* SIOCGIWNWID */
   (iw_handler) iw_set_freq,      /* SIOCSIWFREQ */
   (iw_handler) iw_get_freq,      /* SIOCGIWFREQ */
   (iw_handler) iw_set_mode,      /* SIOCSIWMODE */
   (iw_handler) iw_get_mode,      /* SIOCGIWMODE */
   (iw_handler) NULL,              /* SIOCSIWSENS */
   (iw_handler) NULL,              /* SIOCGIWSENS */
   (iw_handler) NULL,             /* SIOCSIWRANGE */
   (iw_handler) iw_get_range,      /* SIOCGIWRANGE */
   (iw_handler) iw_set_priv,       /* SIOCSIWPRIV */
   (iw_handler) NULL,             /* SIOCGIWPRIV */
   (iw_handler) NULL,             /* SIOCSIWSTATS */
   (iw_handler) NULL,             /* SIOCGIWSTATS */
   iw_handler_set_spy,             /* SIOCSIWSPY */
   iw_handler_get_spy,             /* SIOCGIWSPY */
   iw_handler_set_thrspy,         /* SIOCSIWTHRSPY */
   iw_handler_get_thrspy,         /* SIOCGIWTHRSPY */
   (iw_handler) iw_set_ap_address,   /* SIOCSIWAP */
   (iw_handler) iw_get_ap_address,   /* SIOCGIWAP */
   (iw_handler) iw_set_mlme,              /* SIOCSIWMLME */
   (iw_handler) NULL,              /* SIOCGIWAPLIST */
   (iw_handler) iw_set_scan,      /* SIOCSIWSCAN */
   (iw_handler) iw_get_scan,      /* SIOCGIWSCAN */
   (iw_handler) iw_set_essid,      /* SIOCSIWESSID */
   (iw_handler) iw_get_essid,      /* SIOCGIWESSID */
   (iw_handler) iw_set_nick,      /* SIOCSIWNICKN */
   (iw_handler) iw_get_nick,      /* SIOCGIWNICKN */
   (iw_handler) NULL,             /* -- hole -- */
   (iw_handler) NULL,             /* -- hole -- */
   (iw_handler) iw_set_bitrate,   /* SIOCSIWRATE */
   (iw_handler) iw_get_bitrate,   /* SIOCGIWRATE */
   (iw_handler) iw_set_rts_threshold,/* SIOCSIWRTS */
   (iw_handler) iw_get_rts_threshold,/* SIOCGIWRTS */
   (iw_handler) iw_set_frag_threshold,   /* SIOCSIWFRAG */
   (iw_handler) iw_get_frag_threshold,   /* SIOCGIWFRAG */
   (iw_handler) iw_set_tx_power,      /* SIOCSIWTXPOW */
   (iw_handler) iw_get_tx_power,      /* SIOCGIWTXPOW */
   (iw_handler) iw_set_retry,          /* SIOCSIWRETRY */
   (iw_handler) iw_get_retry,          /* SIOCGIWRETRY */
   (iw_handler) iw_set_encode,          /* SIOCSIWENCODE */
   (iw_handler) iw_get_encode,          /* SIOCGIWENCODE */
   (iw_handler) iw_set_power_mode,      /* SIOCSIWPOWER */
   (iw_handler) iw_get_power_mode,      /* SIOCGIWPOWER */
   (iw_handler) NULL,                 /* -- hole -- */
   (iw_handler) NULL,                /* -- hole -- */
   (iw_handler) iw_set_genie,      /* SIOCSIWGENIE */
   (iw_handler) iw_get_genie,      /* SIOCGIWGENIE */
   (iw_handler) iw_set_auth,      /* SIOCSIWAUTH */
   (iw_handler) iw_get_auth,      /* SIOCGIWAUTH */
   (iw_handler) iw_set_encodeext,   /* SIOCSIWENCODEEXT */
   (iw_handler) iw_get_encodeext,   /* SIOCGIWENCODEEXT */
   (iw_handler) NULL,         /* SIOCSIWPMKSA */
};

static const iw_handler we_private[] = {

   [WLAN_PRIV_SET_INT_GET_NONE      - SIOCIWFIRSTPRIV]   = iw_setint_getnone,  //set priv ioctl
   [WLAN_PRIV_SET_NONE_GET_INT      - SIOCIWFIRSTPRIV]   = iw_setnone_getint,  //get priv ioctl
   [WLAN_PRIV_SET_CHAR_GET_NONE     - SIOCIWFIRSTPRIV]   = iw_setchar_getnone, //get priv ioctl
   [WLAN_PRIV_SET_THREE_INT_GET_NONE - SIOCIWFIRSTPRIV]  = iw_set_three_ints_getnone,
   [WLAN_PRIV_GET_CHAR_SET_NONE      - SIOCIWFIRSTPRIV]  = iw_get_char_setnone,
   [WLAN_PRIV_SET_NONE_GET_NONE     - SIOCIWFIRSTPRIV]   = iw_setnone_getnone, //action priv ioctl
   [WLAN_PRIV_SET_VAR_INT_GET_NONE  - SIOCIWFIRSTPRIV]   = iw_set_var_ints_getnone,
   [WLAN_PRIV_ADD_TSPEC             - SIOCIWFIRSTPRIV]   = iw_add_tspec,
   [WLAN_PRIV_DEL_TSPEC             - SIOCIWFIRSTPRIV]   = iw_del_tspec,
   [WLAN_PRIV_GET_TSPEC             - SIOCIWFIRSTPRIV]   = iw_get_tspec,
#ifdef FEATURE_OEM_DATA_SUPPORT
   [WLAN_PRIV_SET_OEM_DATA_REQ - SIOCIWFIRSTPRIV] = iw_set_oem_data_req, //oem data req Specifc
   [WLAN_PRIV_GET_OEM_DATA_RSP - SIOCIWFIRSTPRIV] = iw_get_oem_data_rsp, //oem data req Specifc
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
   [WLAN_PRIV_SET_FTIES                 - SIOCIWFIRSTPRIV]   = iw_set_fties,
#endif
   [WLAN_PRIV_SET_HOST_OFFLOAD          - SIOCIWFIRSTPRIV]   = iw_set_host_offload,
   [WLAN_GET_WLAN_STATISTICS            - SIOCIWFIRSTPRIV]   = iw_get_statistics,
   [WLAN_SET_KEEPALIVE_PARAMS           - SIOCIWFIRSTPRIV]   = iw_set_keepalive_params
#ifdef WLAN_FEATURE_PACKET_FILTERING
   ,
   [WLAN_SET_PACKET_FILTER_PARAMS       - SIOCIWFIRSTPRIV]   = iw_set_packet_filter_params
#endif
#ifdef FEATURE_WLAN_SCAN_PNO
   ,
   [WLAN_SET_PNO                        - SIOCIWFIRSTPRIV]   = iw_set_pno_priv
#endif
   ,
   [WLAN_SET_BAND_CONFIG                - SIOCIWFIRSTPRIV]   = iw_set_band_config,
   [WLAN_PRIV_SET_MCBC_FILTER           - SIOCIWFIRSTPRIV]   = iw_set_dynamic_mcbc_filter,
   [WLAN_PRIV_CLEAR_MCBC_FILTER         - SIOCIWFIRSTPRIV]   = iw_clear_dynamic_mcbc_filter,
   [WLAN_SET_POWER_PARAMS               - SIOCIWFIRSTPRIV]   = iw_set_power_params_priv,
   [WLAN_GET_LINK_SPEED                 - SIOCIWFIRSTPRIV]   = iw_get_linkspeed_priv,
   [WLAN_PRIV_SET_TWO_INT_GET_NONE      - SIOCIWFIRSTPRIV]   = iw_set_two_ints_getnone,
};

/*Maximum command length can be only 15 */
static const struct iw_priv_args we_private_args[] = {

    /* handlers for main ioctl */
    {   WLAN_PRIV_SET_INT_GET_NONE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "" },

    /* handlers for sub-ioctl */
    {   WE_SET_11D_STATE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set11Dstate" },

    {   WE_WOWL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "wowl" },

    {   WE_SET_POWER,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setPower" },

    {   WE_SET_MAX_ASSOC,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setMaxAssoc" },

    {   WE_SET_SAP_AUTO_CHANNEL_SELECTION,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setAutoChannel" },

    {   WE_SET_DATA_INACTIVITY_TO,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "inactivityTO" },

    {   WE_SET_MAX_TX_POWER,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setMaxTxPower" },

    {   WE_SET_TX_POWER,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTxPower" },

    {   WE_SET_MC_RATE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setMcRate" },

    {   WE_SET_MAX_TX_POWER_2_4,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTxMaxPower2G" },

    {   WE_SET_MAX_TX_POWER_5_0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTxMaxPower5G" },

    /* SAP has TxMax whereas STA has MaxTx, adding TxMax for STA
     * as well to keep same syntax as in SAP. Now onwards, STA
     * will support both */
    {   WE_SET_MAX_TX_POWER,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTxMaxPower" },

    /* set Higher DTIM Transition (DTIM1 to DTIM3)
     * 1 = enable and 0 = disable */
    {
        WE_SET_HIGHER_DTIM_TRANSITION,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setHDtimTransn" },

    {   WE_SET_TM_LEVEL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTmLevel" },

    {   WE_SET_PHYMODE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "setphymode" },

    {   WE_SET_NSS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "nss" },

    {   WE_SET_LDPC,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "ldpc" },

    {   WE_SET_TX_STBC,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "tx_stbc" },

    {   WE_SET_RX_STBC,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "rx_stbc" },

    {   WE_SET_SHORT_GI,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "shortgi" },

    {   WE_SET_RTSCTS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "enablertscts" },

    {   WE_SET_CHWIDTH,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "chwidth" },

    {   WE_SET_ANI_EN_DIS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "anienable" },

    {   WE_SET_ANI_POLL_PERIOD,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "aniplen" },

    {   WE_SET_ANI_LISTEN_PERIOD,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "anilislen" },

    {   WE_SET_ANI_OFDM_LEVEL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "aniofdmlvl" },

    {   WE_SET_ANI_CCK_LEVEL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "aniccklvl" },

    {   WE_SET_DYNAMIC_BW,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "cwmenable" },

    {  WE_SET_GTX_HT_MCS,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxHTMcs" },

    {  WE_SET_GTX_VHT_MCS,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxVHTMcs" },

    {  WE_SET_GTX_USRCFG,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxUsrCfg" },

    {  WE_SET_GTX_THRE,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxThre" },

    {  WE_SET_GTX_MARGIN,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxMargin" },

    {  WE_SET_GTX_STEP,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxStep" },

    {  WE_SET_GTX_MINTPC,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxMinTpc" },

    {  WE_SET_GTX_BWMASK,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "gtxBWMask" },

    {  WE_SET_TX_CHAINMASK,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "txchainmask" },

    {  WE_SET_RX_CHAINMASK,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "rxchainmask" },

    {   WE_SET_11N_RATE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set11NRates" },

    {   WE_SET_VHT_RATE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "set11ACRates" },

    {   WE_SET_AMPDU,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "ampdu" },

    {   WE_SET_AMSDU,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "amsdu" },

    {   WE_SET_BURST_ENABLE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "burst_enable" },

    {   WE_SET_BURST_DUR,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "burst_dur" },

    {   WE_SET_TXPOW_2G,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "txpow2g" },

    {   WE_SET_TXPOW_5G,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "txpow5g" },

    {   WE_SET_POWER_GATING,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "pwrgating" },

    /* Sub-cmds DBGLOG specific commands */
    {   WE_DBGLOG_LOG_LEVEL ,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_loglevel" },

    {   WE_DBGLOG_VAP_ENABLE ,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_vapon" },

    {   WE_DBGLOG_VAP_DISABLE ,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_vapoff" },

    {   WE_DBGLOG_MODULE_ENABLE ,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_modon" },

    {   WE_DBGLOG_MODULE_DISABLE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_modoff" },

    {   WE_DBGLOG_MOD_LOG_LEVEL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_mod_loglevel" },

    {   WE_DBGLOG_TYPE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_type" },
    {   WE_DBGLOG_REPORT_ENABLE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "dl_report" },

    {   WE_SET_TXRX_FWSTATS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "txrx_fw_stats" },

    {   WE_TXRX_FWSTATS_RESET,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "txrx_fw_st_rst" },

    {   WE_PPS_PAID_MATCH,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "paid_match" },


    {   WE_PPS_GID_MATCH,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "gid_match" },


    {   WE_PPS_EARLY_TIM_CLEAR,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "tim_clear" },


    {   WE_PPS_EARLY_DTIM_CLEAR,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "dtim_clear" },


    {   WE_PPS_EOF_PAD_DELIM,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "eof_delim" },


    {   WE_PPS_MACADDR_MISMATCH,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "mac_match" },


    {   WE_PPS_DELIM_CRC_FAIL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "delim_fail" },


    {   WE_PPS_GID_NSTS_ZERO,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "nsts_zero" },


    {   WE_PPS_RSSI_CHECK,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "rssi_chk" },

    {   WE_PPS_5G_EBT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "5g_ebt" },

    {   WE_SET_HTSMPS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "htsmps" },


    {   WE_SET_QPOWER_MAX_PSPOLL_COUNT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "set_qpspollcnt" },

    {   WE_SET_QPOWER_MAX_TX_BEFORE_WAKE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "set_qtxwake" },

    {   WE_SET_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "set_qwakeintv" },

    {   WE_SET_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "set_qnodatapoll" },

    /* handlers for MCC time quota and latency sub ioctls */
    {   WE_MCC_CONFIG_LATENCY,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "setMccLatency" },

    {   WE_MCC_CONFIG_QUOTA,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "setMccQuota" },

    {   WE_SET_DEBUG_LOG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "setDbgLvl" },

    {   WE_SET_SCAN_BAND_PREFERENCE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "set_scan_pref" },
    /* handlers for early_rx power save */
    {   WE_SET_EARLY_RX_ADJUST_ENABLE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "erx_enable" },

    {   WE_SET_EARLY_RX_TGT_BMISS_NUM,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "erx_bmiss_val" },

    {   WE_SET_EARLY_RX_BMISS_SAMPLE_CYCLE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "erx_bmiss_smpl" },

    {   WE_SET_EARLY_RX_SLOP_STEP,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "erx_slop_step" },

    {   WE_SET_EARLY_RX_INIT_SLOP,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "erx_init_slop" },

    {   WE_SET_EARLY_RX_ADJUST_PAUSE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "erx_adj_pause" },

    {   WE_SET_EARLY_RX_DRIFT_SAMPLE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0, "erx_dri_sample" },

    {   WLAN_PRIV_SET_NONE_GET_INT,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "" },

    /* handlers for sub-ioctl */
    {   WE_GET_11D_STATE,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get11Dstate" },

    {   WE_IBSS_STATUS,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "getAdhocStatus" },

    {   WE_GET_WLAN_DBG,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "getwlandbg" },

    {   WE_GET_MAX_ASSOC,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "getMaxAssoc" },

    {   WE_GET_SAP_AUTO_CHANNEL_SELECTION,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "getAutoChannel" },

    {   WE_GET_CONCURRENCY_MODE,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "getconcurrency" },

    {   WE_GET_NSS,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_nss" },

    {   WE_GET_LDPC,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_ldpc" },

    {   WE_GET_TX_STBC,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_tx_stbc" },

    {   WE_GET_RX_STBC,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_rx_stbc" },

    {   WE_GET_SHORT_GI,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_shortgi" },

    {   WE_GET_RTSCTS,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_rtscts" },

    {   WE_GET_CHWIDTH,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_chwidth" },

    {   WE_GET_ANI_EN_DIS,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_anienable" },

    {   WE_GET_ANI_POLL_PERIOD,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_aniplen" },

    {   WE_GET_ANI_LISTEN_PERIOD,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_anilislen" },

    {   WE_GET_ANI_OFDM_LEVEL,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_aniofdmlvl" },

    {   WE_GET_ANI_CCK_LEVEL,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_aniccklvl" },

    {   WE_GET_DYNAMIC_BW,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_cwmenable" },

    {  WE_GET_GTX_HT_MCS,
       0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_gtxHTMcs" },

    {  WE_GET_GTX_VHT_MCS,
       0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_gtxVHTMcs" },

    {  WE_GET_GTX_USRCFG,
        0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_gtxUsrCfg" },

    {  WE_GET_GTX_THRE,
        0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_gtxThre" },

    {  WE_GET_GTX_MARGIN,
        0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_gtxMargin" },

    {  WE_GET_GTX_STEP,
        0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_gtxStep" },

    {  WE_GET_GTX_MINTPC,
        0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_gtxMinTpc" },

    {  WE_GET_GTX_BWMASK,
        0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_gtxBWMask" },

    {   WE_GET_TX_CHAINMASK,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_txchainmask" },

    {   WE_GET_RX_CHAINMASK,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_rxchainmask" },

    {   WE_GET_11N_RATE,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_11nrate" },

    {   WE_GET_AMPDU,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_ampdu" },

    {   WE_GET_AMSDU,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_amsdu" },

    {   WE_GET_BURST_ENABLE,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_burst_en" },

    {   WE_GET_BURST_DUR,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_burst_dur" },

    {   WE_GET_TXPOW_2G,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_txpow2g" },

    {   WE_GET_TXPOW_5G,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_txpow5g" },

    {   WE_GET_POWER_GATING,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_pwrgating" },

    {   WE_GET_PPS_PAID_MATCH,
	0,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"get_paid_match"},


    {   WE_GET_PPS_GID_MATCH,
	0,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"get_gid_match"},


    {   WE_GET_PPS_EARLY_TIM_CLEAR,
	0,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"get_tim_clear"},


    {   WE_GET_PPS_EARLY_DTIM_CLEAR,
	0,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"get_dtim_clear"},


    {   WE_GET_PPS_EOF_PAD_DELIM,
	0,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"get_eof_delim"},


    {   WE_GET_PPS_MACADDR_MISMATCH,
	0,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"get_mac_match"},


    {   WE_GET_PPS_DELIM_CRC_FAIL,
	0,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"get_delim_fail"},


    {   WE_GET_PPS_GID_NSTS_ZERO,
	0,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"get_nsts_zero"},


    {   WE_GET_PPS_RSSI_CHECK,
	0,
	IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
	"get_rssi_chk"},

    {   WE_GET_QPOWER_MAX_PSPOLL_COUNT,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_qpspollcnt"},

    {   WE_GET_QPOWER_MAX_TX_BEFORE_WAKE,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_qtxwake"},

    {   WE_GET_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_qwakeintv"},

    {   WE_GET_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_qnodatapoll"},

    {   WE_GET_SCAN_BAND_PREFERENCE,
        0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "get_scan_pref"},

    /* handlers for main ioctl */
    {   WLAN_PRIV_SET_CHAR_GET_NONE,
        IW_PRIV_TYPE_CHAR| 512,
        0,
        "" },

    /* handlers for sub-ioctl */
    {   WE_WOWL_ADD_PTRN,
        IW_PRIV_TYPE_CHAR| 512,
        0,
        "wowlAddPtrn" },

    {   WE_WOWL_DEL_PTRN,
        IW_PRIV_TYPE_CHAR| 512,
        0,
        "wowlDelPtrn" },

#if defined WLAN_FEATURE_VOWIFI
    /* handlers for sub-ioctl */
    {   WE_NEIGHBOR_REPORT_REQUEST,
        IW_PRIV_TYPE_CHAR | 512,
        0,
        "neighbor" },
#endif
    {   WE_SET_AP_WPS_IE,
        IW_PRIV_TYPE_CHAR| 512,
        0,
        "set_ap_wps_ie" },

    {   WE_SET_CONFIG,
        IW_PRIV_TYPE_CHAR| 512,
        0,
        "setConfig" },

    /* handlers for main ioctl */
    {   WLAN_PRIV_SET_THREE_INT_GET_NONE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
        0,
        "" },

    /* handlers for sub-ioctl */
    {   WE_SET_WLAN_DBG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
        0,
        "setwlandbg" },

    {   WE_SET_SAP_CHANNELS,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
        0,
        "setsapchannels" },

    /* handlers for main ioctl */
    {   WLAN_PRIV_GET_CHAR_SET_NONE,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "" },

    /* handlers for sub-ioctl */
    {   WE_WLAN_VERSION,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "version" },
    {   WE_GET_STATS,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getStats" },
    {   WE_GET_STATES,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getHostStates" },
    {   WE_GET_CFG,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getConfig" },
#ifdef WLAN_FEATURE_11AC
    {   WE_GET_RSSI,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getRSSI" },
#endif
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
    {   WE_GET_ROAM_RSSI,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getRoamRSSI" },
#endif
    {   WE_GET_WMM_STATUS,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getWmmStatus" },
    {
        WE_GET_CHANNEL_LIST,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getChannelList" },
#ifdef FEATURE_WLAN_TDLS
    {
        WE_GET_TDLS_PEERS,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getTdlsPeers" },
#endif
#ifdef WLAN_FEATURE_11W
    {
        WE_GET_11W_INFO,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getPMFInfo" },
#endif
    {   WE_GET_PHYMODE,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getphymode" },
#ifdef FEATURE_OEM_DATA_SUPPORT
    {   WE_GET_OEM_DATA_CAP,
        0,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        "getOemDataCap" },
#endif /* FEATURE_OEM_DATA_SUPPORT */

    /* handlers for main ioctl */
    {   WLAN_PRIV_SET_NONE_GET_NONE,
        0,
        0,
        "" },

    /* handlers for sub-ioctl */
    {   WE_CLEAR_STATS,
        0,
        0,
        "clearStats" },
    {   WE_GET_RECOVERY_STAT,
        0,
        0,
        "getRecoverStat" },
#ifdef WLAN_BTAMP_FEATURE
    {   WE_ENABLE_AMP,
        0,
        0,
        "enableAMP" },
    {   WE_DISABLE_AMP,
        0,
        0,
        "disableAMP" },
#endif /* WLAN_BTAMP_FEATURE */
    {   WE_ENABLE_DXE_STALL_DETECT,
        0,
        0,
        "dxeStallDetect" },
    {   WE_DISPLAY_DXE_SNAP_SHOT,
        0,
        0,
        "dxeSnapshot" },
    {   WE_DISPLAY_DATAPATH_SNAP_SHOT,
        0,
        0,
        "dataSnapshot"},
    {
        WE_SET_REASSOC_TRIGGER,
        0,
        0,
        "reassoc" },
    {   WE_DUMP_AGC_START,
        0,
        0,
        "dump_agc_start" },

    {   WE_DUMP_AGC,
        0,
        0,
        "dump_agc" },

    {   WE_DUMP_CHANINFO_START,
        0,
        0,
        "dump_chninfo_en" },

    {   WE_DUMP_CHANINFO,
        0,
        0,
        "dump_chninfo" },

    {   WE_DUMP_WATCHDOG,
        0,
        0,
        "dump_watchdog" },
#ifdef CONFIG_ATH_PCIE_ACCESS_DEBUG
    {   WE_DUMP_PCIE_LOG,
        0,
        0,
        "dump_pcie_log" },
#endif
    /* handlers for main ioctl */
    {   WLAN_PRIV_SET_VAR_INT_GET_NONE,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "" },

    /* handlers for sub-ioctl */
    {   WE_LOG_DUMP_CMD,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "dump" },

    /* handlers for sub-ioctl */
    {   WE_MTRACE_SELECTIVE_MODULE_LOG_ENABLE_CMD,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "setdumplog" },

    {   WE_MTRACE_DUMP_CMD,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "dumplog" },

    /* handlers for sub ioctl */
   {
       WE_MCC_CONFIG_CREDENTIAL,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0,
       "setMccCrdnl" },

    /* handlers for sub ioctl */
   {
       WE_MCC_CONFIG_PARAMS,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0,
       "setMccConfig" },

#ifdef FEATURE_WLAN_TDLS
    /* handlers for sub ioctl */
   {
       WE_TDLS_CONFIG_PARAMS,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0,
       "setTdlsConfig" },
#endif
    {
        WE_UNIT_TEST_CMD,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "setUnitTestCmd" },

    /* handlers for main ioctl */
    {   WLAN_PRIV_ADD_TSPEC,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | HDD_WLAN_WMM_PARAM_COUNT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "addTspec" },

    /* handlers for main ioctl */
    {   WLAN_PRIV_DEL_TSPEC,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "delTspec" },

    /* handlers for main ioctl */
    {   WLAN_PRIV_GET_TSPEC,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        "getTspec" },

#ifdef FEATURE_OEM_DATA_SUPPORT
    /* handlers for main ioctl - OEM DATA */
    {
        WLAN_PRIV_SET_OEM_DATA_REQ,
        IW_PRIV_TYPE_BYTE | sizeof(struct iw_oem_data_req) | IW_PRIV_SIZE_FIXED,
        0,
        "set_oem_data_req" },

    /* handlers for main ioctl - OEM DATA */
    {
        WLAN_PRIV_GET_OEM_DATA_RSP,
        0,
        IW_PRIV_TYPE_BYTE | MAX_OEM_DATA_RSP_LEN,
        "get_oem_data_rsp" },
#endif

    /* handlers for main ioctl - host offload */
    {
        WLAN_PRIV_SET_HOST_OFFLOAD,
        IW_PRIV_TYPE_BYTE | sizeof(tHostOffloadRequest),
        0,
        "setHostOffload" },

    {
        WLAN_GET_WLAN_STATISTICS,
        0,
        IW_PRIV_TYPE_BYTE | WE_MAX_STR_LEN,
        "getWlanStats" },

    {
        WLAN_SET_KEEPALIVE_PARAMS,
        IW_PRIV_TYPE_BYTE  | WE_MAX_STR_LEN,
        0,
        "setKeepAlive" },
#ifdef WLAN_FEATURE_PACKET_FILTERING
    {
        WLAN_SET_PACKET_FILTER_PARAMS,
        IW_PRIV_TYPE_BYTE  | sizeof(tPacketFilterCfg),
        0,
        "setPktFilter" },
#endif
#ifdef FEATURE_WLAN_SCAN_PNO
    {
        WLAN_SET_PNO,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        0,
        "setpno" },
#endif
    {
        WLAN_SET_BAND_CONFIG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
        0,
        "SETBAND" },
    /* handlers for dynamic MC BC ioctl */
    {
        WLAN_PRIV_SET_MCBC_FILTER,
        IW_PRIV_TYPE_BYTE | sizeof(tRcvFltMcAddrList),
        0,
        "setMCBCFilter" },
    {
        WLAN_PRIV_CLEAR_MCBC_FILTER,
        0,
        0,
        "clearMCBCFilter" },
    {
        WLAN_SET_POWER_PARAMS,
        IW_PRIV_TYPE_CHAR| WE_MAX_STR_LEN,
        0,
        "setpowerparams" },
    {
        WLAN_GET_LINK_SPEED,
        IW_PRIV_TYPE_CHAR | 18,
        IW_PRIV_TYPE_CHAR | 5, "getLinkSpeed" },

    /* handlers for main ioctl */
    {   WLAN_PRIV_SET_TWO_INT_GET_NONE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
        0,
        "" },
    {   WE_SET_SMPS_PARAM,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
        0, "set_smps_param" },
#ifdef DEBUG
    {   WE_SET_FW_CRASH_INJECT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
        0, "crash_inject" },
#endif
};



const struct iw_handler_def we_handler_def = {
   .num_standard     = sizeof(we_handler) / sizeof(we_handler[0]),
   .num_private      = sizeof(we_private) / sizeof(we_private[0]),
   .num_private_args = sizeof(we_private_args) / sizeof(we_private_args[0]),

   .standard         = (iw_handler *)we_handler,
   .private          = (iw_handler *)we_private,
   .private_args     = we_private_args,
   .get_wireless_stats = get_wireless_stats,
};

int hdd_validate_mcc_config(hdd_adapter_t *pAdapter, v_UINT_t staId, v_UINT_t arg1, v_UINT_t arg2, v_UINT_t arg3)
{
    v_U32_t  cmd = 288; //Command to RIVA
    hdd_context_t *pHddCtx = NULL;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    /*
     *configMccParam : specify the bit which needs to be modified
     *allowed to update based on wlan_qcom_cfg.ini
     * configuration
     * Bit 0 : SCHEDULE_TIME_SLICE   MIN : 5 MAX : 20
     * Bit 1 : MAX_NULL_SEND_TIME    MIN : 1 MAX : 10
     * Bit 2 : TX_EARLY_STOP_TIME    MIN : 1 MAX : 10
     * Bit 3 : RX_DRAIN_TIME         MIN : 1 MAX : 10
     * Bit 4 : CHANNEL_SWITCH_TIME   MIN : 1 MAX : 20
     * Bit 5 : MIN_CHANNEL_TIME      MIN : 5 MAX : 20
     * Bit 6 : PARK_BEFORE_TBTT      MIN : 1 MAX :  5
     * Bit 7 : MIN_AFTER_DTIM        MIN : 5 MAX : 15
     * Bit 8 : TOO_CLOSE_MARGIN      MIN : 1 MAX :  3
     * Bit 9 : Reserved
     */
    switch (arg1)
    {
        //Update MCC SCHEDULE_TIME_SLICE parameter
        case MCC_SCHEDULE_TIME_SLICE_CFG_PARAM :
            if( pHddCtx->cfg_ini->configMccParam & 0x0001)
            {
                if((arg2 >= 5) && (arg2 <= 20))
                {
                    logPrintf(hHal, cmd, staId, arg1, arg2, arg3);
                }
                else
                {
                    hddLog(LOGE, "%s : Enter a valid MCC configuration value", __FUNCTION__);
                    return 0;
                }
            }
            break;

        //Update MCC MAX_NULL_SEND_TIME parameter
        case MCC_MAX_NULL_SEND_TIME_CFG_PARAM :
            if( pHddCtx->cfg_ini->configMccParam & 0x0002)
            {
                if((arg2 >= 1) && (arg2 <= 10))
                {
                    logPrintf(hHal, cmd, staId, arg1, arg2, arg3);
                }
                else
                {
                    hddLog(LOGE, "%s : Enter a valid MCC configuration value", __FUNCTION__);
                    return 0;
                }
            }
            break;

        //Update MCC TX_EARLY_STOP_TIME parameter
        case MCC_TX_EARLY_STOP_TIME_CFG_PARAM :
            if( pHddCtx->cfg_ini->configMccParam & 0x0004)
            {
                if((arg2 >= 1) && (arg2 <= 10))
                {
                    logPrintf(hHal, cmd, staId, arg1, arg2, arg3);
                }
                else
                {
                    hddLog(LOGE, "%s : Enter a valid MCC configuration value", __FUNCTION__);
                    return 0;
                }
            }
            break;

        //Update MCC RX_DRAIN_TIME parameter
        case MCC_RX_DRAIN_TIME_CFG_PARAM :
            if( pHddCtx->cfg_ini->configMccParam & 0x0008)
            {
                if((arg2 >= 1) && (arg2 <= 10))
                {
                    logPrintf(hHal, cmd, staId, arg1, arg2, arg3);
                }
                else
                {
                    hddLog(LOGE, "%s : Enter a valid MCC configuration value", __FUNCTION__);
                    return 0;
                }
            }
        break;

        //Update MCC CHANNEL_SWITCH_TIME parameter
        case MCC_CHANNEL_SWITCH_TIME_CFG_PARAM :
            if( pHddCtx->cfg_ini->configMccParam & 0x0010)
            {
                if((arg2 >= 1) && (arg2 <= 20))
                {
                    logPrintf(hHal, cmd, staId, arg1, arg2, arg3);
                }
                else
                {
                    hddLog(LOGE, "%s : Enter a valid MCC configuration value", __FUNCTION__);
                    return 0;
                }
            }
            break;

        //Update MCC MIN_CHANNEL_TIME parameter
        case MCC_MIN_CHANNEL_TIME_CFG_PARAM :
            if( pHddCtx->cfg_ini->configMccParam & 0x0020)
            {
                if((arg2 >= 5) && (arg2 <= 20))
                {
                    logPrintf(hHal, cmd, staId, arg1, arg2, arg3);
                }
                else
                {
                    hddLog(LOGE, "%s : Enter a valid MCC configuration value", __FUNCTION__);
                    return 0;
                }
            }
            break;

        //Update MCC PARK_BEFORE_TBTT parameter
        case MCC_PARK_BEFORE_TBTT_CFG_PARAM :
            if( pHddCtx->cfg_ini->configMccParam & 0x0040)
            {
                if((arg2 >= 1) && (arg2 <= 5))
                {
                    logPrintf(hHal, cmd, staId, arg1, arg2, arg3);
                }
                else
                {
                    hddLog(LOGE, "%s : Enter a valid MCC configuration value", __FUNCTION__);
                    return 0;
                }
            }
            break;

        //Update MCC MIN_AFTER_DTIM parameter
        case MCC_MIN_AFTER_DTIM_CFG_PARAM :
            if( pHddCtx->cfg_ini->configMccParam & 0x0080)
            {
                if((arg2 >= 5) && (arg2 <= 15))
                {
                    logPrintf(hHal, cmd, staId, arg1, arg2, arg3);
                }
                else
                {
                    hddLog(LOGE, "%s : Enter a valid MCC configuration value", __FUNCTION__);
                    return 0;
                }
            }
            break;

        //Update MCC TOO_CLOSE_MARGIN parameter
        case MCC_TOO_CLOSE_MARGIN_CFG_PARAM :
            if( pHddCtx->cfg_ini->configMccParam & 0x0100)
            {
                if((arg2 >= 1) && (arg2 <= 3))
                {
                    logPrintf(hHal, cmd, staId, arg1, arg2, arg3);
                }
                else
                {
                    hddLog(LOGE, "%s : Enter a valid MCC configuration value", __FUNCTION__);
                    return 0;
                }
            }
            break;

        default :
            hddLog(LOGE, FL("Unknown / Not allowed to configure parameter : %d"), arg1);
            break;
    }
    return 0;
}

int hdd_set_wext(hdd_adapter_t *pAdapter)
{
    hdd_wext_state_t *pwextBuf;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    pwextBuf = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

    // Now configure the roaming profile links. To SSID and bssid.
    pwextBuf->roamProfile.SSIDs.numOfSSIDs = 0;
    pwextBuf->roamProfile.SSIDs.SSIDList = &pHddStaCtx->conn_info.SSID;

    pwextBuf->roamProfile.BSSIDs.numOfBSSIDs = 0;
    pwextBuf->roamProfile.BSSIDs.bssid = &pHddStaCtx->conn_info.bssId;

    /*Set the numOfChannels to zero to scan all the channels*/
    pwextBuf->roamProfile.ChannelInfo.numOfChannels = 0;
    pwextBuf->roamProfile.ChannelInfo.ChannelList = NULL;

    /* Default is no encryption */
    pwextBuf->roamProfile.EncryptionType.numEntries = 1;
    pwextBuf->roamProfile.EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;

    pwextBuf->roamProfile.mcEncryptionType.numEntries = 1;
    pwextBuf->roamProfile.mcEncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;

    pwextBuf->roamProfile.BSSType = eCSR_BSS_TYPE_INFRASTRUCTURE;

    /* Default is no authentication */
    pwextBuf->roamProfile.AuthType.numEntries = 1;
    pwextBuf->roamProfile.AuthType.authType[0] = eCSR_AUTH_TYPE_OPEN_SYSTEM;

    pwextBuf->roamProfile.phyMode = eCSR_DOT11_MODE_TAURUS;
    pwextBuf->wpaVersion = IW_AUTH_WPA_VERSION_DISABLED;

    /*Set the default scan mode*/
    pAdapter->scan_info.scan_mode = eSIR_ACTIVE_SCAN;

    hdd_clearRoamProfileIe(pAdapter);

    return VOS_STATUS_SUCCESS;

    }

int hdd_register_wext(struct net_device *dev)
    {
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_wext_state_t *pwextBuf = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    VOS_STATUS status;

   ENTER();

    // Zero the memory.  This zeros the profile structure.
   memset(pwextBuf, 0,sizeof(hdd_wext_state_t));

    init_completion(&(WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter))->completion_var);


    status = hdd_set_wext(pAdapter);

    if(!VOS_IS_STATUS_SUCCESS(status)) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: hdd_set_wext failed!!"));
        return eHAL_STATUS_FAILURE;
    }

    if (!VOS_IS_STATUS_SUCCESS(vos_event_init(&pwextBuf->vosevent)))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: HDD vos event init failed!!"));
        return eHAL_STATUS_FAILURE;
    }

    if (!VOS_IS_STATUS_SUCCESS(vos_event_init(&pwextBuf->scanevent)))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: HDD scan event init failed!!"));
        return eHAL_STATUS_FAILURE;
    }

    // Register as a wireless device
    dev->wireless_handlers = (struct iw_handler_def *)&we_handler_def;

    EXIT();
    return 0;
}

int hdd_UnregisterWext(struct net_device *dev)
{
	hddLog(LOG1, FL("dev(%p)"), dev);

	if (dev != NULL) {
		rtnl_lock();
		dev->wireless_handlers = NULL;
		rtnl_unlock();
	}

	return 0;
}
