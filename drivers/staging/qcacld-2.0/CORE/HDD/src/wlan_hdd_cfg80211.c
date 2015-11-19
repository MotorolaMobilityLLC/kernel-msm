/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

/**========================================================================

  \file  wlan_hdd_cfg80211.c

  \brief WLAN Host Device Driver implementation

  ========================================================================*/

/**=========================================================================

  EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$   $DateTime: $ $Author: $


  when        who            what, where, why
  --------    ---            --------------------------------------------------------
 21/12/09     Ashwani        Created module.

 07/06/10     Kumar Deepak   Implemented cfg80211 callbacks for ANDROID
              Ganesh K
  ==========================================================================*/


#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <wlan_hdd_includes.h>
#include <net/arp.h>
#include <net/cfg80211.h>
#include <vos_trace.h>
#ifdef CONFIG_CNSS
#include <net/cnss.h>
#endif
#include <linux/wireless.h>
#include <wlan_hdd_wowl.h>
#include <aniGlobal.h>
#include "ccmApi.h"
#include "sirParams.h"
#include "dot11f.h"
#include "wlan_hdd_assoc.h"
#include "wlan_hdd_wext.h"
#include "sme_Api.h"
#include "wlan_hdd_p2p.h"
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_hostapd.h"
#include "wlan_hdd_softap_tx_rx.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_assoc.h"
#include "wlan_hdd_power.h"
#include "wlan_hdd_trace.h"
#include "vos_types.h"
#include "vos_trace.h"
#include "vos_utils.h"
#ifdef WLAN_BTAMP_FEATURE
#include "bap_hdd_misc.h"
#endif
#include "vos_sched.h"
#include <qc_sap_ioctl.h>
#include "wlan_hdd_tdls.h"
#include "wlan_hdd_wmm.h"
#include "wlan_qct_wda.h"
#include "wlan_nv.h"
#include "wlan_hdd_dev_pwr.h"
#ifdef CONFIG_CNSS
#include <net/cnss.h>
#endif
#include "wlan_hdd_misc.h"
#ifdef WLAN_FEATURE_NAN
#include "nan_Api.h"
#include "wlan_hdd_nan.h"
#endif
#ifdef IPA_OFFLOAD
#include <wlan_hdd_ipa.h>
#endif
#include "qwlan_version.h"

#include "wlan_hdd_memdump.h"

#include "wlan_logging_sock_svc.h"

#define g_mode_rates_size (12)
#define a_mode_rates_size (8)
#define FREQ_BASE_80211G          (2407)
#define FREQ_BAND_DIFF_80211G     (5)
#define MAX_SCAN_SSID 10
#define MAX_PENDING_LOG 5
#define MAX_HT_MCS_IDX 8
#define MAX_VHT_MCS_IDX 10
#define INVALID_MCS_IDX 255
#define GET_IE_LEN_IN_BSS_DESC(lenInBss) ( lenInBss + sizeof(lenInBss) - \
        ((uintptr_t)OFFSET_OF( tSirBssDescription, ieFields)))

#define HDD_WAKE_LOCK_SCAN_DURATION       (5 * 1000) /* in msec */

/* For IBSS, enable obss, fromllb, overlapOBSS & overlapFromllb protection
   check. The bit map is defined in:

    typedef struct sCfgProtection
    {
        tANI_U32 overlapFromlla:1;
        tANI_U32 overlapFromllb:1;
        tANI_U32 overlapFromllg:1;
        tANI_U32 overlapHt20:1;
        tANI_U32 overlapNonGf:1;
        tANI_U32 overlapLsigTxop:1;
        tANI_U32 overlapRifs:1;
        tANI_U32 overlapOBSS:1;
        tANI_U32 fromlla:1;
        tANI_U32 fromllb:1;
        tANI_U32 fromllg:1;
        tANI_U32 ht20:1;
        tANI_U32 nonGf:1;
        tANI_U32 lsigTxop:1;
        tANI_U32 rifs:1;
        tANI_U32 obss:1;
    }tCfgProtection, *tpCfgProtection;

*/
#define IBSS_CFG_PROTECTION_ENABLE_MASK 0x8282

#define HDD2GHZCHAN(freq, chan, flag)   {     \
    .band =  IEEE80211_BAND_2GHZ, \
    .center_freq = (freq), \
    .hw_value = (chan),\
    .flags = (flag), \
    .max_antenna_gain = 0 ,\
    .max_power = 30, \
}

#define HDD5GHZCHAN(freq, chan, flag)   {     \
    .band =  IEEE80211_BAND_5GHZ, \
    .center_freq = (freq), \
    .hw_value = (chan),\
    .flags = (flag), \
    .max_antenna_gain = 0 ,\
    .max_power = 30, \
}

#define HDD_G_MODE_RATETAB(rate, rate_id, flag)\
{\
    .bitrate = rate, \
    .hw_value = rate_id, \
    .flags = flag, \
}

#ifndef WLAN_FEATURE_TDLS_DEBUG
#define TDLS_LOG_LEVEL VOS_TRACE_LEVEL_INFO
#else
#define TDLS_LOG_LEVEL VOS_TRACE_LEVEL_ERROR
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
#define WLAN_AKM_SUITE_FT_8021X         0x000FAC03
#define WLAN_AKM_SUITE_FT_PSK           0x000FAC04
#endif

#define HDD_CHANNEL_14 14
#define WLAN_HDD_MAX_FEATURE_SET   8

#ifdef FEATURE_WLAN_EXTSCAN
/*
 * Used to allocate the size of 4096 for the EXTScan NL data.
 * The size of 4096 is considered assuming that all data per
 * respective event fit with in the limit.Please take a call
 * on the limit based on the data requirements.
 */

#define EXTSCAN_EVENT_BUF_SIZE 4096
#endif

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
/*
 * Used to allocate the size of 4096 for the link layer stats.
 * The size of 4096 is considered assuming that all data per
 * respective event fit with in the limit.Please take a call
 * on the limit based on the data requirements on link layer
 * statistics.
 */
#define LL_STATS_EVENT_BUF_SIZE 4096
#endif

/* EXT TDLS */
/*
 * Used to allocate the size of 4096 for the TDLS.
 * The size of 4096 is considered assuming that all data per
 * respective event fit with in the limit.Please take a call
 * on the limit based on the data requirements on link layer
 * statistics.
 */
#define EXTTDLS_EVENT_BUF_SIZE 4096

static const u32 hdd_cipher_suites[] =
{
    WLAN_CIPHER_SUITE_WEP40,
    WLAN_CIPHER_SUITE_WEP104,
    WLAN_CIPHER_SUITE_TKIP,
#ifdef FEATURE_WLAN_ESE
#define WLAN_CIPHER_SUITE_BTK 0x004096fe /* use for BTK */
#define WLAN_CIPHER_SUITE_KRK 0x004096ff /* use for KRK */
    WLAN_CIPHER_SUITE_BTK,
    WLAN_CIPHER_SUITE_KRK,
    WLAN_CIPHER_SUITE_CCMP,
#else
    WLAN_CIPHER_SUITE_CCMP,
#endif
#ifdef FEATURE_WLAN_WAPI
    WLAN_CIPHER_SUITE_SMS4,
#endif
#ifdef WLAN_FEATURE_11W
    WLAN_CIPHER_SUITE_AES_CMAC,
#endif
};

static struct ieee80211_channel hdd_channels_2_4_GHZ[] =
{
    HDD2GHZCHAN(2412, 1, 0) ,
    HDD2GHZCHAN(2417, 2, 0) ,
    HDD2GHZCHAN(2422, 3, 0) ,
    HDD2GHZCHAN(2427, 4, 0) ,
    HDD2GHZCHAN(2432, 5, 0) ,
    HDD2GHZCHAN(2437, 6, 0) ,
    HDD2GHZCHAN(2442, 7, 0) ,
    HDD2GHZCHAN(2447, 8, 0) ,
    HDD2GHZCHAN(2452, 9, 0) ,
    HDD2GHZCHAN(2457, 10, 0) ,
    HDD2GHZCHAN(2462, 11, 0) ,
    HDD2GHZCHAN(2467, 12, 0) ,
    HDD2GHZCHAN(2472, 13, 0) ,
    HDD2GHZCHAN(2484, 14, 0) ,
};

static struct ieee80211_channel hdd_social_channels_2_4_GHZ[] =
{
    HDD2GHZCHAN(2412, 1, 0) ,
    HDD2GHZCHAN(2437, 6, 0) ,
    HDD2GHZCHAN(2462, 11, 0) ,
};

static struct ieee80211_channel hdd_channels_5_GHZ[] =
{
    HDD5GHZCHAN(4920, 240, 0) ,
    HDD5GHZCHAN(4940, 244, 0) ,
    HDD5GHZCHAN(4960, 248, 0) ,
    HDD5GHZCHAN(4980, 252, 0) ,
    HDD5GHZCHAN(5040, 208, 0) ,
    HDD5GHZCHAN(5060, 212, 0) ,
    HDD5GHZCHAN(5080, 216, 0) ,
    HDD5GHZCHAN(5180, 36, 0) ,
    HDD5GHZCHAN(5200, 40, 0) ,
    HDD5GHZCHAN(5220, 44, 0) ,
    HDD5GHZCHAN(5240, 48, 0) ,
    HDD5GHZCHAN(5260, 52, 0) ,
    HDD5GHZCHAN(5280, 56, 0) ,
    HDD5GHZCHAN(5300, 60, 0) ,
    HDD5GHZCHAN(5320, 64, 0) ,
    HDD5GHZCHAN(5500,100, 0) ,
    HDD5GHZCHAN(5520,104, 0) ,
    HDD5GHZCHAN(5540,108, 0) ,
    HDD5GHZCHAN(5560,112, 0) ,
    HDD5GHZCHAN(5580,116, 0) ,
    HDD5GHZCHAN(5600,120, 0) ,
    HDD5GHZCHAN(5620,124, 0) ,
    HDD5GHZCHAN(5640,128, 0) ,
    HDD5GHZCHAN(5660,132, 0) ,
    HDD5GHZCHAN(5680,136, 0) ,
    HDD5GHZCHAN(5700,140, 0) ,
#ifdef FEATURE_WLAN_CH144
    HDD5GHZCHAN(5720,144, 0) ,
#endif /* FEATURE_WLAN_CH144 */
    HDD5GHZCHAN(5745,149, 0) ,
    HDD5GHZCHAN(5765,153, 0) ,
    HDD5GHZCHAN(5785,157, 0) ,
    HDD5GHZCHAN(5805,161, 0) ,
    HDD5GHZCHAN(5825,165, 0) ,
};

static struct ieee80211_rate g_mode_rates[] =
{
    HDD_G_MODE_RATETAB(10, 0x1, 0),
    HDD_G_MODE_RATETAB(20, 0x2, 0),
    HDD_G_MODE_RATETAB(55, 0x4, 0),
    HDD_G_MODE_RATETAB(110, 0x8, 0),
    HDD_G_MODE_RATETAB(60, 0x10, 0),
    HDD_G_MODE_RATETAB(90, 0x20, 0),
    HDD_G_MODE_RATETAB(120, 0x40, 0),
    HDD_G_MODE_RATETAB(180, 0x80, 0),
    HDD_G_MODE_RATETAB(240, 0x100, 0),
    HDD_G_MODE_RATETAB(360, 0x200, 0),
    HDD_G_MODE_RATETAB(480, 0x400, 0),
    HDD_G_MODE_RATETAB(540, 0x800, 0),
};

static struct ieee80211_rate a_mode_rates[] =
{
    HDD_G_MODE_RATETAB(60, 0x10, 0),
    HDD_G_MODE_RATETAB(90, 0x20, 0),
    HDD_G_MODE_RATETAB(120, 0x40, 0),
    HDD_G_MODE_RATETAB(180, 0x80, 0),
    HDD_G_MODE_RATETAB(240, 0x100, 0),
    HDD_G_MODE_RATETAB(360, 0x200, 0),
    HDD_G_MODE_RATETAB(480, 0x400, 0),
    HDD_G_MODE_RATETAB(540, 0x800, 0),
};

static struct ieee80211_supported_band wlan_hdd_band_2_4_GHZ =
{
    .channels = hdd_channels_2_4_GHZ,
    .n_channels = ARRAY_SIZE(hdd_channels_2_4_GHZ),
    .band       = IEEE80211_BAND_2GHZ,
    .bitrates = g_mode_rates,
    .n_bitrates = g_mode_rates_size,
    .ht_cap.ht_supported   = 1,
    .ht_cap.cap            =  IEEE80211_HT_CAP_SGI_20
                            | IEEE80211_HT_CAP_GRN_FLD
                            | IEEE80211_HT_CAP_DSSSCCK40
                            | IEEE80211_HT_CAP_LSIG_TXOP_PROT
                            | IEEE80211_HT_CAP_SGI_40
                            | IEEE80211_HT_CAP_SUP_WIDTH_20_40,
    .ht_cap.ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,
    .ht_cap.ampdu_density  = IEEE80211_HT_MPDU_DENSITY_16,
    .ht_cap.mcs.rx_mask    = { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    .ht_cap.mcs.rx_highest = cpu_to_le16( 72 ),
    .ht_cap.mcs.tx_params  = IEEE80211_HT_MCS_TX_DEFINED,
    .vht_cap.cap = IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454
                            | IEEE80211_VHT_CAP_SHORT_GI_80
                            | IEEE80211_VHT_CAP_TXSTBC
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,4,0))
                            | (IEEE80211_VHT_CAP_RXSTBC_MASK &
                              ( IEEE80211_VHT_CAP_RXSTBC_1
                              | IEEE80211_VHT_CAP_RXSTBC_2))
#endif
                            | IEEE80211_VHT_CAP_RXLDPC,
};

static struct ieee80211_supported_band wlan_hdd_band_p2p_2_4_GHZ =
{
    .channels = hdd_social_channels_2_4_GHZ,
    .n_channels = ARRAY_SIZE(hdd_social_channels_2_4_GHZ),
    .band       = IEEE80211_BAND_2GHZ,
    .bitrates = g_mode_rates,
    .n_bitrates = g_mode_rates_size,
    .ht_cap.ht_supported   = 1,
    .ht_cap.cap            =  IEEE80211_HT_CAP_SGI_20
                            | IEEE80211_HT_CAP_GRN_FLD
                            | IEEE80211_HT_CAP_DSSSCCK40
                            | IEEE80211_HT_CAP_LSIG_TXOP_PROT,
    .ht_cap.ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,
    .ht_cap.ampdu_density  = IEEE80211_HT_MPDU_DENSITY_16,
    .ht_cap.mcs.rx_mask    = { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    .ht_cap.mcs.rx_highest = cpu_to_le16( 72 ),
    .ht_cap.mcs.tx_params  = IEEE80211_HT_MCS_TX_DEFINED,
};

static struct ieee80211_supported_band wlan_hdd_band_5_GHZ =
{
    .channels = hdd_channels_5_GHZ,
    .n_channels = ARRAY_SIZE(hdd_channels_5_GHZ),
    .band     = IEEE80211_BAND_5GHZ,
    .bitrates = a_mode_rates,
    .n_bitrates = a_mode_rates_size,
    .ht_cap.ht_supported   = 1,
    .ht_cap.cap            =  IEEE80211_HT_CAP_SGI_20
                            | IEEE80211_HT_CAP_GRN_FLD
                            | IEEE80211_HT_CAP_DSSSCCK40
                            | IEEE80211_HT_CAP_LSIG_TXOP_PROT
                            | IEEE80211_HT_CAP_SGI_40
                            | IEEE80211_HT_CAP_SUP_WIDTH_20_40,
    .ht_cap.ampdu_factor   = IEEE80211_HT_MAX_AMPDU_64K,
    .ht_cap.ampdu_density  = IEEE80211_HT_MPDU_DENSITY_16,
    .ht_cap.mcs.rx_mask    = { 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, },
    .ht_cap.mcs.rx_highest = cpu_to_le16( 72 ),
    .ht_cap.mcs.tx_params  = IEEE80211_HT_MCS_TX_DEFINED,
    .vht_cap.vht_supported = 1,
    .vht_cap.cap = IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454
                 | IEEE80211_VHT_CAP_SHORT_GI_80
                 | IEEE80211_VHT_CAP_TXSTBC
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,4,0))
                 | (IEEE80211_VHT_CAP_RXSTBC_MASK &
                   ( IEEE80211_VHT_CAP_RXSTBC_1
                   | IEEE80211_VHT_CAP_RXSTBC_2))
#endif
                 | IEEE80211_VHT_CAP_RXLDPC
};

/* This structure contain information what kind of frame are expected in
     TX/RX direction for each kind of interface */
static const struct ieee80211_txrx_stypes
wlan_hdd_txrx_stypes[NUM_NL80211_IFTYPES] = {
    [NL80211_IFTYPE_STATION] = {
        .tx = 0xffff,
        .rx = BIT(SIR_MAC_MGMT_ACTION) |
            BIT(SIR_MAC_MGMT_PROBE_REQ),
    },
    [NL80211_IFTYPE_AP] = {
        .tx = 0xffff,
        .rx = BIT(SIR_MAC_MGMT_ASSOC_REQ) |
            BIT(SIR_MAC_MGMT_REASSOC_REQ) |
            BIT(SIR_MAC_MGMT_PROBE_REQ) |
            BIT(SIR_MAC_MGMT_DISASSOC) |
            BIT(SIR_MAC_MGMT_AUTH) |
            BIT(SIR_MAC_MGMT_DEAUTH) |
            BIT(SIR_MAC_MGMT_ACTION),
    },
    [NL80211_IFTYPE_ADHOC] = {
        .tx = 0xffff,
        .rx = BIT(SIR_MAC_MGMT_ASSOC_REQ) |
            BIT(SIR_MAC_MGMT_REASSOC_REQ) |
            BIT(SIR_MAC_MGMT_PROBE_REQ) |
            BIT(SIR_MAC_MGMT_DISASSOC) |
            BIT(SIR_MAC_MGMT_AUTH) |
            BIT(SIR_MAC_MGMT_DEAUTH) |
            BIT(SIR_MAC_MGMT_ACTION),
    },
    [NL80211_IFTYPE_P2P_CLIENT] = {
        .tx = 0xffff,
        .rx = BIT(SIR_MAC_MGMT_ACTION) |
            BIT(SIR_MAC_MGMT_PROBE_REQ),
    },
    [NL80211_IFTYPE_P2P_GO] = {
        /* This is also same as for SoftAP */
        .tx = 0xffff,
        .rx = BIT(SIR_MAC_MGMT_ASSOC_REQ) |
            BIT(SIR_MAC_MGMT_REASSOC_REQ) |
            BIT(SIR_MAC_MGMT_PROBE_REQ) |
            BIT(SIR_MAC_MGMT_DISASSOC) |
            BIT(SIR_MAC_MGMT_AUTH) |
            BIT(SIR_MAC_MGMT_DEAUTH) |
            BIT(SIR_MAC_MGMT_ACTION),
    },
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
/* Interface limits and combinations registered by the driver */

/* STA ( + STA ) combination */
static const struct ieee80211_iface_limit
wlan_hdd_sta_iface_limit[] = {
   {
      .max = 3, /* p2p0 is a STA as well */
      .types = BIT(NL80211_IFTYPE_STATION),
   },
};

/* ADHOC (IBSS) limit */
static const struct ieee80211_iface_limit
wlan_hdd_adhoc_iface_limit[] = {
   {
      .max = 1,
      .types = BIT(NL80211_IFTYPE_STATION),
   },
   {
      .max = 1,
      .types = BIT(NL80211_IFTYPE_ADHOC),
   },
};

/* AP ( + AP ) combination */
static const struct ieee80211_iface_limit
wlan_hdd_ap_iface_limit[] = {
   {
      .max = (VOS_MAX_NO_OF_SAP_MODE +
              SAP_MAX_OBSS_STA_CNT),
      .types = BIT(NL80211_IFTYPE_AP),
   },
};

/* P2P limit */
static const struct ieee80211_iface_limit
wlan_hdd_p2p_iface_limit[] = {
   {
      .max = 1,
      .types = BIT(NL80211_IFTYPE_P2P_CLIENT),
   },
   {
      .max = 1,
      .types = BIT(NL80211_IFTYPE_P2P_GO),
   },
};

static const struct ieee80211_iface_limit
wlan_hdd_sta_ap_iface_limit[] = {
    {
        /* We need 1 extra STA interface for OBSS scan when SAP starts
         * with HT40 in STA+SAP concurrency mode
         */
        .max = (1 + SAP_MAX_OBSS_STA_CNT),
        .types = BIT(NL80211_IFTYPE_STATION),
    },
    {
        .max = VOS_MAX_NO_OF_SAP_MODE,
        .types = BIT(NL80211_IFTYPE_AP),
    },
};

/* STA + P2P combination */
static const struct ieee80211_iface_limit
wlan_hdd_sta_p2p_iface_limit[] = {
   {
      /* One reserved for dedicated P2PDEV usage */
      .max = 2,
      .types = BIT(NL80211_IFTYPE_STATION)
   },
   {
      /* Support for two identical (GO + GO or CLI + CLI)
       * or dissimilar (GO + CLI) P2P interfaces
       */
      .max = 2,
      .types = BIT(NL80211_IFTYPE_P2P_GO) |
               BIT(NL80211_IFTYPE_P2P_CLIENT),
   },
};

static struct ieee80211_iface_combination
wlan_hdd_iface_combination[] = {
   /* STA */
   {
      .limits = wlan_hdd_sta_iface_limit,
      .num_different_channels = 2,
      .max_interfaces = 3,
      .n_limits = ARRAY_SIZE(wlan_hdd_sta_iface_limit),
   },
   /* ADHOC */
   {
      .limits = wlan_hdd_adhoc_iface_limit,
      .num_different_channels = 1,
      .max_interfaces = 2,
      .n_limits = ARRAY_SIZE(wlan_hdd_adhoc_iface_limit),
   },
   /* AP */
   {
      .limits = wlan_hdd_ap_iface_limit,
      .num_different_channels = 2,
      .max_interfaces = (SAP_MAX_OBSS_STA_CNT +
                         VOS_MAX_NO_OF_SAP_MODE),
      .n_limits = ARRAY_SIZE(wlan_hdd_ap_iface_limit),
   },
   /* P2P */
   {
      .limits = wlan_hdd_p2p_iface_limit,
      .num_different_channels = 2,
      .max_interfaces = 2,
      .n_limits = ARRAY_SIZE(wlan_hdd_p2p_iface_limit),
   },
   /* STA + AP */
   {
      .limits = wlan_hdd_sta_ap_iface_limit,
      .num_different_channels = 2,
      .max_interfaces = (1 + SAP_MAX_OBSS_STA_CNT +
                         VOS_MAX_NO_OF_SAP_MODE),
      .n_limits = ARRAY_SIZE(wlan_hdd_sta_ap_iface_limit),
      .beacon_int_infra_match = true,
   },
   /* STA + P2P */
   {
      .limits = wlan_hdd_sta_p2p_iface_limit,
      .num_different_channels = 2,
      /* one interface reserved for P2PDEV dedicated usage */
      .max_interfaces = 4,
      .n_limits = ARRAY_SIZE(wlan_hdd_sta_p2p_iface_limit),
      .beacon_int_infra_match = true,
   },
};
#endif //(LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))


static struct cfg80211_ops wlan_hdd_cfg80211_ops;

/* Data rate 100KBPS based on IE Index */
struct index_data_rate_type
{
   v_U8_t   beacon_rate_index;
   v_U16_t  supported_rate[4];
};

/* 11B, 11G Rate table include Basic rate and Extended rate
   The IDX field is the rate index
   The HI field is the rate when RSSI is strong or being ignored
    (in this case we report actual rate)
   The MID field is the rate when RSSI is moderate
    (in this case we cap 11b rates at 5.5 and 11g rates at 24)
   The LO field is the rate when RSSI is low
    (in this case we don't report rates, actual current rate used)
 */
static const struct
{
   v_U8_t   beacon_rate_index;
   v_U16_t  supported_rate[4];
} supported_data_rate[] =
{
/* IDX     HI  HM  LM LO (RSSI-based index */
   {2,   { 10,  10, 10, 0}},
   {4,   { 20,  20, 10, 0}},
   {11,  { 55,  20, 10, 0}},
   {12,  { 60,  55, 20, 0}},
   {18,  { 90,  55, 20, 0}},
   {22,  {110,  55, 20, 0}},
   {24,  {120,  90, 60, 0}},
   {36,  {180, 120, 60, 0}},
   {44,  {220, 180, 60, 0}},
   {48,  {240, 180, 90, 0}},
   {66,  {330, 180, 90, 0}},
   {72,  {360, 240, 90, 0}},
   {96,  {480, 240, 120, 0}},
   {108, {540, 240, 120, 0}}
};

/* MCS Based rate table */
/* HT MCS parameters with Nss = 1 */
static struct index_data_rate_type supported_mcs_rate_nss1[] =
{
/* MCS  L20   L40   S20  S40 */
   {0,  {65,  135,  72,  150}},
   {1,  {130, 270,  144, 300}},
   {2,  {195, 405,  217, 450}},
   {3,  {260, 540,  289, 600}},
   {4,  {390, 810,  433, 900}},
   {5,  {520, 1080, 578, 1200}},
   {6,  {585, 1215, 650, 1350}},
   {7,  {650, 1350, 722, 1500}}
};
/* HT MCS parameters with Nss = 2 */
static struct index_data_rate_type supported_mcs_rate_nss2[] =
{
/* MCS  L20    L40   S20   S40 */
   {0,  {130,  270,  144,  300}},
   {1,  {260,  540,  289,  600}},
   {2,  {390,  810,  433,  900}},
   {3,  {520,  1080, 578,  1200}},
   {4,  {780,  1620, 867,  1800}},
   {5,  {1040, 2160, 1156, 2400}},
   {6,  {1170, 2430, 1300, 2700}},
   {7,  {1300, 2700, 1444, 3000}}
};

#ifdef WLAN_FEATURE_11AC

#define DATA_RATE_11AC_MCS_MASK    0x03

struct index_vht_data_rate_type
{
   v_U8_t   beacon_rate_index;
   v_U16_t  supported_VHT80_rate[2];
   v_U16_t  supported_VHT40_rate[2];
   v_U16_t  supported_VHT20_rate[2];
};

typedef enum
{
   DATA_RATE_11AC_MAX_MCS_7,
   DATA_RATE_11AC_MAX_MCS_8,
   DATA_RATE_11AC_MAX_MCS_9,
   DATA_RATE_11AC_MAX_MCS_NA
} eDataRate11ACMaxMcs;

/* SSID broadcast type */
typedef enum eSSIDBcastType
{
  eBCAST_UNKNOWN      = 0,
  eBCAST_NORMAL       = 1,
  eBCAST_HIDDEN       = 2,
} tSSIDBcastType;

/* MCS Based VHT rate table */
/* MCS parameters with Nss = 1*/
static struct index_vht_data_rate_type supported_vht_mcs_rate_nss1[] =
{
/* MCS  L80    S80     L40   S40    L20   S40*/
   {0,  {293,  325},  {135,  150},  {65,   72}},
   {1,  {585,  650},  {270,  300},  {130,  144}},
   {2,  {878,  975},  {405,  450},  {195,  217}},
   {3,  {1170, 1300}, {540,  600},  {260,  289}},
   {4,  {1755, 1950}, {810,  900},  {390,  433}},
   {5,  {2340, 2600}, {1080, 1200}, {520,  578}},
   {6,  {2633, 2925}, {1215, 1350}, {585,  650}},
   {7,  {2925, 3250}, {1350, 1500}, {650,  722}},
   {8,  {3510, 3900}, {1620, 1800}, {780,  867}},
   {9,  {3900, 4333}, {1800, 2000}, {780,  867}}
};

/*MCS parameters with Nss = 2*/
static struct index_vht_data_rate_type supported_vht_mcs_rate_nss2[] =
{
/* MCS  L80    S80     L40   S40    L20   S40*/
   {0,  {585,  650},  {270,  300},  {130,  144}},
   {1,  {1170, 1300}, {540,  600},  {260,  289}},
   {2,  {1755, 1950}, {810,  900},  {390,  433}},
   {3,  {2340, 2600}, {1080, 1200}, {520,  578}},
   {4,  {3510, 3900}, {1620, 1800}, {780,  867}},
   {5,  {4680, 5200}, {2160, 2400}, {1040, 1156}},
   {6,  {5265, 5850}, {2430, 2700}, {1170, 1300}},
   {7,  {5850, 6500}, {2700, 3000}, {1300, 1444}},
   {8,  {7020, 7800}, {3240, 3600}, {1560, 1733}},
   {9,  {7800, 8667}, {3600, 4000}, {1560, 1733}}
};
#endif /* WLAN_FEATURE_11AC */

/* Array index points to MCS and array value points respective rssi */
static int rssiMcsTbl[][10] =
{
/*MCS 0   1     2   3    4    5    6    7    8    9*/
   {-82, -79, -77, -74, -70, -66, -65, -64, -59, -57}, //20
   {-79, -76, -74, -71, -67, -63, -62, -61, -56, -54}, //40
   {-76, -73, -71, -68, -64, -60, -59, -58, -53, -51}  //80
};

extern struct net_device_ops net_ops_struct;

#ifdef WLAN_NL80211_TESTMODE
enum wlan_hdd_tm_attr
{
    WLAN_HDD_TM_ATTR_INVALID = 0,
    WLAN_HDD_TM_ATTR_CMD     = 1,
    WLAN_HDD_TM_ATTR_DATA    = 2,
    WLAN_HDD_TM_ATTR_STREAM_ID = 3,
    WLAN_HDD_TM_ATTR_TYPE    = 4,
    /* keep last */
    WLAN_HDD_TM_ATTR_AFTER_LAST,
    WLAN_HDD_TM_ATTR_MAX       = WLAN_HDD_TM_ATTR_AFTER_LAST - 1,
};

enum wlan_hdd_tm_cmd
{
    WLAN_HDD_TM_CMD_WLAN_FTM   = 0,
    WLAN_HDD_TM_CMD_WLAN_HB    = 1,
};

#define WLAN_HDD_TM_DATA_MAX_LEN    5000

static const struct nla_policy wlan_hdd_tm_policy[WLAN_HDD_TM_ATTR_MAX + 1] =
{
    [WLAN_HDD_TM_ATTR_CMD]        = { .type = NLA_U32 },
    [WLAN_HDD_TM_ATTR_DATA]       = { .type = NLA_BINARY,
                                    .len = WLAN_HDD_TM_DATA_MAX_LEN },
};
#endif /* WLAN_NL80211_TESTMODE */

#ifdef FEATURE_WLAN_EXTSCAN

static const struct nla_policy
wlan_hdd_extscan_config_policy[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1] =
{
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_VALID_CHANNELS_CONFIG_PARAM_WIFI_BAND] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CHANNEL] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_DWELL_TIME] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_PASSIVE] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CLASS] = { .type = NLA_U8 },

    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_INDEX] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_BAND] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_PERIOD] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_REPORT_EVENTS] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_BASE_PERIOD] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_MAX_AP_PER_SCAN] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD_PERCENT] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD_NUM_SCANS] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_NUM_BUCKETS] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_FLUSH] = { .type = NLA_U8 },

    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_MAX] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID] = { .type = NLA_UNSPEC },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW] = { .type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH] = { .type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_CHANNEL] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_NUM_AP] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_RSSI_SAMPLE_SIZE] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_LOST_AP_SAMPLE_SIZE] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_MIN_BREACHING] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_NUM_AP] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_NUM_NETWORKS] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_SSID] = { .type = NLA_BINARY,
							.len = IEEE80211_MAX_SSID_LEN },
    [QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_RSSI_THRESHOLD] = { .type = NLA_S8 },
    [QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_FLAGS] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_AUTH_BIT] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_SSID] = { .type = NLA_BINARY,
							.len = IEEE80211_MAX_SSID_LEN + 1 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_HOTLIST_PARAMS_LOST_SSID_SAMPLE_SIZE] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_HOTLIST_PARAMS_NUM_SSID] = { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_BAND] = { .type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_RSSI_LOW] = { .type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_RSSI_HIGH] = { .type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_CONFIGURATION_FLAGS] = { .type = NLA_U32 },
};

static const struct nla_policy
wlan_hdd_extscan_results_policy[QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_MAX + 1] =
{
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BEACON_PERIOD] = { .type = NLA_U16 },
    [QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CAPABILITY] = { .type = NLA_U16 },
};


#endif /* FEATURE_WLAN_EXTSCAN */

#if defined(FEATURE_WLAN_CH_AVOID) || defined(FEATURE_WLAN_FORCE_SAP_SCC)
/*
 * FUNCTION: wlan_hdd_send_avoid_freq_event
 * This is called when wlan driver needs to send vendor specific
 * avoid frequency range event to user space
 */
int wlan_hdd_send_avoid_freq_event(hdd_context_t *pHddCtx,
                                   tHddAvoidFreqList *pAvoidFreqList)
{
    struct sk_buff *vendor_event;

    ENTER();

    if (!pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is null", __func__);
        return -1;
    }

    if (!pAvoidFreqList)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: pAvoidFreqList is null", __func__);
        return -1;
    }

    vendor_event = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                              sizeof(tHddAvoidFreqList),
                              QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY_INDEX,
                              GFP_KERNEL);
    if (!vendor_event)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: cfg80211_vendor_event_alloc failed", __func__);
        return -1;
    }

    memcpy(skb_put(vendor_event, sizeof(tHddAvoidFreqList)),
                   (void *)pAvoidFreqList, sizeof(tHddAvoidFreqList));

    cfg80211_vendor_event(vendor_event, GFP_KERNEL);

    EXIT();
    return 0;
}
#endif /* FEATURE_WLAN_CH_AVOID || FEATURE_WLAN_FORCE_SAP_SCC */

#ifdef WLAN_FEATURE_NAN
/*
 * FUNCTION: wlan_hdd_cfg80211_nan_request
 * This is called when wlan driver needs to send vendor specific
 * nan request event.
 */
static int wlan_hdd_cfg80211_nan_request(struct wiphy *wiphy,
                                         struct wireless_dev *wdev,
                                         const void *data,
                                         int data_len)
{
    tNanRequestReq nan_req;
    VOS_STATUS status;
    int ret_val = -1;

    nan_req.request_data_len = data_len;
    nan_req.request_data = (void *)data;

    status = sme_NanRequest(&nan_req);
    if (VOS_STATUS_SUCCESS == status) {
        ret_val = 0;
    }
    return ret_val;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_nan_callback
 * This is a callback function and it gets called
 * when we need to report nan response event to
 * upper layers.
 */
static void wlan_hdd_cfg80211_nan_callback(void* ctx, tSirNanEvent* msg)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)ctx;
    struct sk_buff *vendor_event;
    int status;
    tSirNanEvent *data;

    if (NULL == msg) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL(" msg received here is null"));
        return;
    }
    data = msg;

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("HDD context is not valid"));
        return;
    }

    vendor_event = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                                   data->event_data_len +
                                   NLMSG_HDRLEN,
                                   QCA_NL80211_VENDOR_SUBCMD_NAN_INDEX,
                                   GFP_KERNEL);

    if (!vendor_event) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NAN,
                data->event_data_len, data->event_data)) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   FL("QCA_WLAN_VENDOR_ATTR_NAN put fail"));
        kfree_skb(vendor_event);
        return;
    }
    cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}

/*
 * FUNCTION: wlan_hdd_cfg80211_nan_init
 * This function is called to register the callback to sme layer
 */
void wlan_hdd_cfg80211_nan_init(hdd_context_t *pHddCtx)
{
    sme_NanRegisterCallback(pHddCtx->hHal, wlan_hdd_cfg80211_nan_callback);
}

#endif

/* vendor specific events */
static const struct nl80211_vendor_cmd_info wlan_hdd_cfg80211_vendor_events[] =
{
#ifdef FEATURE_WLAN_CH_AVOID
    [QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY
    },
#endif /* FEATURE_WLAN_CH_AVOID */

#ifdef WLAN_FEATURE_NAN
    [QCA_NL80211_VENDOR_SUBCMD_NAN_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_NAN
    },
#endif

#ifdef WLAN_FEATURE_STATS_EXT
    [QCA_NL80211_VENDOR_SUBCMD_STATS_EXT_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_STATS_EXT
    },
#endif /* WLAN_FEATURE_STATS_EXT */
#ifdef FEATURE_WLAN_EXTSCAN
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_START_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_START
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_STOP_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_STOP
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CAPABILITIES_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CAPABILITIES
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CACHED_RESULTS_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CACHED_RESULTS
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_RESULTS_AVAILABLE_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_RESULTS_AVAILABLE
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_FULL_SCAN_RESULT_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_FULL_SCAN_RESULT
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_EVENT_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_EVENT
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_FOUND_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_FOUND
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_BSSID_HOTLIST_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_BSSID_HOTLIST
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_BSSID_HOTLIST_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_BSSID_HOTLIST
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SIGNIFICANT_CHANGE_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SIGNIFICANT_CHANGE
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SIGNIFICANT_CHANGE_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SIGNIFICANT_CHANGE
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SIGNIFICANT_CHANGE_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SIGNIFICANT_CHANGE
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_SSID_FOUND_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_SSID_FOUND
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_SSID_LOST_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_SSID_LOST
    },
#endif /* FEATURE_WLAN_EXTSCAN */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
    [QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET_INDEX] =  {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET
    },
    [QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET_INDEX] =  {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET
    },
    [QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR_INDEX] =  {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR
    },
    [QCA_NL80211_VENDOR_SUBCMD_LL_RADIO_STATS_INDEX] =  {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_RADIO_RESULTS
    },
    [QCA_NL80211_VENDOR_SUBCMD_LL_IFACE_STATS_INDEX] =  {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_IFACE_RESULTS
    },
    [QCA_NL80211_VENDOR_SUBCMD_LL_PEER_INFO_STATS_INDEX] =  {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_PEERS_RESULTS
    },
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */
/* EXT TDLS */
    [QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE_CHANGE_INDEX] =  {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE
    },
#ifdef FEATURE_WLAN_EXTSCAN
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_NETWORK_FOUND_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_NETWORK_FOUND
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_PASSPOINT_NETWORK_FOUND_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_PASSPOINT_NETWORK_FOUND
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SSID_HOTLIST_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SSID_HOTLIST
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SSID_HOTLIST_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SSID_HOTLIST
    },
    [QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_LOST_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_LOST
    },
#endif /* FEATURE_WLAN_EXTSCAN */
#ifdef WLAN_FEATURE_MEMDUMP
    [QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_MEMORY_DUMP_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_MEMORY_DUMP
    },
#endif /* WLAN_FEATURE_MEMDUMP */
    [QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI_INDEX] = {
        .vendor_id = QCA_NL80211_VENDOR_ID,
        .subcmd = QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI
    },
};

static int is_driver_dfs_capable(struct wiphy *wiphy,
                                 struct wireless_dev *wdev,
                                 const void *data,
                                 int data_len)
{
    u32 dfs_capability = 0;
    struct sk_buff *temp_skbuff;
    int ret_val;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,4,0))
    dfs_capability = !!(wiphy->flags & WIPHY_FLAG_DFS_OFFLOAD);
#endif

    temp_skbuff = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32) +
                                                      NLMSG_HDRLEN);

    if (temp_skbuff != NULL)
    {

        ret_val = nla_put_u32(temp_skbuff, QCA_WLAN_VENDOR_ATTR_DFS,
                              dfs_capability);
        if (ret_val)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: QCA_WLAN_VENDOR_ATTR_DFS put fail", __func__);
            kfree_skb(temp_skbuff);

            return ret_val;
        }

        return cfg80211_vendor_cmd_reply(temp_skbuff);
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s: dfs capability: buffer alloc fail", __func__);
    return -1;

}

static int
__wlan_hdd_cfg80211_get_supported_features(struct wiphy *wiphy,
                                         struct wireless_dev *wdev,
                                         const void *data,
                                         int data_len)
{
    hdd_context_t *pHddCtx      = wiphy_priv(wiphy);
    struct sk_buff *skb         = NULL;
    tANI_U32       fset         = 0;
    int ret;

    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {
        hddLog(LOGE, FL("Hdd context not valid"));
        return -EINVAL;
    }

    if (wiphy->interface_modes & BIT(NL80211_IFTYPE_STATION)) {
        hddLog(LOG1, FL("Infra Station mode is supported by driver"));
        fset |= WIFI_FEATURE_INFRA;
    }

    if (TRUE == hdd_is_5g_supported(pHddCtx)) {
        hddLog(LOG1, FL("INFRA_5G is supported by firmware"));
        fset |= WIFI_FEATURE_INFRA_5G;
    }

#ifdef WLAN_FEATURE_P2P
    if ((wiphy->interface_modes & BIT(NL80211_IFTYPE_P2P_CLIENT)) &&
        (wiphy->interface_modes & BIT(NL80211_IFTYPE_P2P_GO))) {
        hddLog(LOG1, FL("WiFi-Direct is supported by driver"));
        fset |= WIFI_FEATURE_P2P;
    }
#endif

    /* Soft-AP is supported currently by default */
    fset |= WIFI_FEATURE_SOFT_AP;

    /* HOTSPOT is a supplicant feature, enable it by default */
    fset |= WIFI_FEATURE_HOTSPOT;

#ifdef FEATURE_WLAN_EXTSCAN
    if (sme_IsFeatureSupportedByFW(EXTENDED_SCAN)) {
        hddLog(LOG1, FL("EXTScan is supported by firmware"));
        fset |= WIFI_FEATURE_EXTSCAN | WIFI_FEATURE_HAL_EPNO;
    }
#endif

#ifdef WLAN_FEATURE_NAN
    if (sme_IsFeatureSupportedByFW(NAN)) {
        hddLog(LOG1, FL("NAN is supported by firmware"));
        fset |= WIFI_FEATURE_NAN;
    }
#endif

    if (sme_IsFeatureSupportedByFW(RTT)) {
        hddLog(LOG1, FL("RTT is supported by firmware"));
        fset |= WIFI_FEATURE_D2D_RTT;
        fset |= WIFI_FEATURE_D2AP_RTT;
    }

#ifdef FEATURE_WLAN_SCAN_PNO
    if (pHddCtx->cfg_ini->configPNOScanSupport &&
        sme_IsFeatureSupportedByFW(PNO)) {
        hddLog(LOG1, FL("PNO is supported by firmware"));
        fset |= WIFI_FEATURE_PNO;
    }
#endif

    /* STA+STA is supported currently by default */
    fset |= WIFI_FEATURE_ADDITIONAL_STA;

#ifdef FEATURE_WLAN_TDLS
    if ((TRUE == pHddCtx->cfg_ini->fEnableTDLSSupport) &&
        sme_IsFeatureSupportedByFW(TDLS)) {
        hddLog(LOG1, FL("TDLS is supported by firmware"));
        fset |= WIFI_FEATURE_TDLS;
    }

    if (sme_IsFeatureSupportedByFW(TDLS) &&
       (TRUE == pHddCtx->cfg_ini->fEnableTDLSOffChannel) &&
       sme_IsFeatureSupportedByFW(TDLS_OFF_CHANNEL)) {
        hddLog(LOG1, FL("TDLS off-channel is supported by firmware"));
        fset |= WIFI_FEATURE_TDLS_OFFCHANNEL;
    }
#endif

#ifdef WLAN_AP_STA_CONCURRENCY
    /* AP+STA concurrency is supported currently by default */
    fset |= WIFI_FEATURE_AP_STA;
#endif
    fset |= WIFI_FEATURE_RSSI_MONITOR;

    if (hdd_link_layer_stats_supported())
        fset |= WIFI_FEATURE_LINK_LAYER_STATS;

    skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(fset) +
                                              NLMSG_HDRLEN);

    if (!skb) {
        hddLog(LOGE, FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
        return -EINVAL;
    }
    hddLog(LOG1, FL("Supported Features : 0x%x"), fset);

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_FEATURE_SET, fset)) {
        hddLog(LOGE, FL("nla put fail"));
        goto nla_put_failure;
    }

    return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
    kfree_skb(skb);
    return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_get_supported_features() - get supported features
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_get_supported_features(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_supported_features(wiphy, wdev,
						data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

static int
wlan_hdd_cfg80211_set_scanning_mac_oui(struct wiphy *wiphy,
                                       struct wireless_dev *wdev,
                                       const void *data,
                                       int data_len)
{
    tpSirScanMacOui pReqMsg   = NULL;
    hdd_context_t *pHddCtx    = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_MAX + 1];
    eHalStatus status;
    int ret;

    ENTER();

    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {
       hddLog(LOGE, FL("HDD context is not valid"));
       return ret;
    }

    if (FALSE == pHddCtx->cfg_ini->enable_mac_spoofing) {
        hddLog(LOGW, FL("MAC address spoofing is not enabled"));
        return -ENOTSUPP;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI_MAX,
                    data, data_len,
                    NULL)) {
        hddLog(LOGE, FL("Invalid ATTR"));
        return -EINVAL;
    }

    pReqMsg = vos_mem_malloc(sizeof(*pReqMsg));
    if (!pReqMsg) {
        hddLog(LOGE, FL("vos_mem_malloc failed"));
        return -ENOMEM;
    }

    /* Parse and fetch oui */
    if (!tb[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI]) {
        hddLog(LOGE, FL("attr mac oui failed"));
        goto fail;
    }

    nla_memcpy(&pReqMsg->oui[0],
            tb[QCA_WLAN_VENDOR_ATTR_SET_SCANNING_MAC_OUI],
            sizeof(pReqMsg->oui));

    hddLog(LOG1, FL("Oui (%02x:%02x:%02x)"), pReqMsg->oui[0], pReqMsg->oui[1],
                 pReqMsg->oui[2]);

    status = sme_SetScanningMacOui(pHddCtx->hHal, pReqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                    FL("sme_SetScanningMacOui failed(err=%d)"), status);
        goto fail;
    }

    return 0;

fail:
    vos_mem_free(pReqMsg);
    return -EINVAL;
}

static int
__wlan_hdd_cfg80211_get_concurrency_matrix(struct wiphy *wiphy,
                                         struct wireless_dev *wdev,
                                         const void *data,
                                         int data_len)
{
    uint32_t feature_set_matrix[WLAN_HDD_MAX_FEATURE_SET] = {0};
    uint8_t i, feature_sets, max_feature_sets;
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_MAX + 1];
    struct sk_buff *reply_skb;
    hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
    int ret;

    ENTER();

    ret = wlan_hdd_validate_context(hdd_ctx);
    if (0 != ret) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return ret;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_MAX,
                  data, data_len, NULL)) {
        hddLog(LOGE, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch max feature set */
    if (!tb[QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_CONFIG_PARAM_SET_SIZE_MAX]) {
        hddLog(LOGE, FL("Attr max feature set size failed"));
        return -EINVAL;
    }
    max_feature_sets = nla_get_u32(
     tb[QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_CONFIG_PARAM_SET_SIZE_MAX]);
    hddLog(LOG1, FL("Max feature set size (%d)"), max_feature_sets);

    /* Fill feature combination matrix */
    feature_sets = 0;
    if (feature_sets >= WLAN_HDD_MAX_FEATURE_SET) goto max_buffer_err;
    feature_set_matrix[feature_sets++] = WIFI_FEATURE_INFRA |
                                         WIFI_FEATURE_P2P;

    /* Add more feature combinations here */

    feature_sets = VOS_MIN(feature_sets, max_feature_sets);
    hddLog(LOG1, FL("Number of feature sets (%d)"), feature_sets);
    hddLog(LOG1, "Feature set matrix");
    for (i = 0; i < feature_sets; i++)
        hddLog(LOG1, "[%d] 0x%02X", i, feature_set_matrix[i]);

    reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32) +
                                                    sizeof(u32) * feature_sets +
                                                    NLMSG_HDRLEN);

    if (reply_skb) {
        if (nla_put_u32(reply_skb,
                        QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_RESULTS_SET_SIZE,
                        feature_sets) ||
            nla_put(reply_skb,
                    QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_RESULTS_SET,
                    sizeof(u32) * feature_sets, feature_set_matrix)) {
            hddLog(LOGE, FL("nla put fail"));
            kfree_skb(reply_skb);
            return -EINVAL;
        }

        return cfg80211_vendor_cmd_reply(reply_skb);
    }
    hddLog(LOGE, FL("Feature set matrix: buffer alloc fail"));
    return -ENOMEM;

max_buffer_err:
    hddLog(LOGE, FL("Feature set max buffer size reached. feature_sets(%d) max(%d)"),
           feature_sets, WLAN_HDD_MAX_FEATURE_SET);
    return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_get_concurrency_matrix() - get concurrency matrix
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_get_concurrency_matrix(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_get_concurrency_matrix(wiphy, wdev, data,
                                                     data_len);
	vos_ssr_unprotect(__func__);
	return ret;
}

static int
__wlan_hdd_cfg80211_set_ext_roam_params(struct wiphy *wiphy,
                                   struct wireless_dev *wdev,
                                   const void *data,
                                   int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);
	uint8_t session_id;
	struct roam_ext_params roam_params;
	uint32_t cmd_type, req_id;
	struct nlattr *curr_attr;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX + 1];
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX + 1];
	int rem, i;
	uint32_t buf_len = 0;
	int ret;

	ret = wlan_hdd_validate_context(pHddCtx);
	if (0 != ret) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX,
		data, data_len,
		NULL)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}
	/* Parse and fetch Command Type*/
	if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_SUBCMD]) {
		hddLog(LOGE, FL("roam cmd type failed"));
		goto fail;
	}
	session_id = pAdapter->sessionId;
	vos_mem_set(&roam_params, sizeof(roam_params),0);
	cmd_type = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_ROAMING_SUBCMD]);
	if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_REQ_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		goto fail;
	}
	req_id = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_ROAMING_REQ_ID]);
	hddLog(VOS_TRACE_LEVEL_DEBUG, FL("Req Id (%d)"), req_id);
	hddLog(VOS_TRACE_LEVEL_DEBUG, FL("Cmd Type (%d)"), cmd_type);
	switch(cmd_type) {
	case QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_SSID_WHITE_LIST:
		i = 0;
		nla_for_each_nested(curr_attr,
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID_LIST],
			rem) {
			if (nla_parse(tb2,
				QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_MAX,
				nla_data(curr_attr), nla_len(curr_attr),
				NULL)) {
				hddLog(LOGE, FL("nla_parse failed"));
				goto fail;
			}
			/* Parse and Fetch allowed SSID list*/
			if (!tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID]) {
				hddLog(LOGE, FL("attr allowed ssid failed"));
				goto fail;
			}
			buf_len = nla_len(tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID]);
			/*
			 * Upper Layers include a null termination character.
			 * Check for the actual permissible length of SSID and
			 * also ensure not to copy the NULL termination
			 * character to the driver buffer.
			 */
			if (buf_len && (i < MAX_SSID_ALLOWED_LIST) &&
				((buf_len - 1) <= SIR_MAC_MAX_SSID_LENGTH)) {
				nla_memcpy(roam_params.ssid_allowed_list[i].ssId,
					tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_WHITE_LIST_SSID],
					buf_len - 1);
				roam_params.ssid_allowed_list[i].length =
					buf_len - 1;
				hddLog(VOS_TRACE_LEVEL_DEBUG,
					FL("SSID[%d]: %.*s,length = %d"), i,
					roam_params.ssid_allowed_list[i].length,
					roam_params.ssid_allowed_list[i].ssId,
					roam_params.ssid_allowed_list[i].length);
				i++;
			}
			else {
				hddLog(LOGE, FL("Invalid SSID len %d,idx %d"),
					buf_len, i);
			}
		}
		roam_params.num_ssid_allowed_list = i;
		hddLog(VOS_TRACE_LEVEL_DEBUG, FL("Num of Allowed SSID %d"),
			roam_params.num_ssid_allowed_list);
		sme_update_roam_params(pHddCtx->hHal, session_id,
				roam_params, REASON_ROAM_SET_SSID_ALLOWED);
		break;
	case QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_SET_EXTSCAN_ROAM_PARAMS:
		/* Parse and fetch 5G Boost Threshold */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_THRESHOLD]) {
			hddLog(LOGE, FL("5G boost threshold failed"));
			goto fail;
		}
		roam_params.raise_rssi_thresh_5g = nla_get_s32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_THRESHOLD]);
		hddLog(VOS_TRACE_LEVEL_DEBUG,
			FL("5G Boost Threshold (%d)"),
			roam_params.raise_rssi_thresh_5g);
		/* Parse and fetch 5G Penalty Threshold */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_THRESHOLD]) {
			hddLog(LOGE, FL("5G penalty threshold failed"));
			goto fail;
		}
		roam_params.drop_rssi_thresh_5g = nla_get_s32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_THRESHOLD]);
		hddLog(VOS_TRACE_LEVEL_DEBUG,
			FL("5G Penalty Threshold (%d)"),
			roam_params.drop_rssi_thresh_5g);
		/* Parse and fetch 5G Boost Factor */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_FACTOR]) {
			hddLog(LOGE, FL("5G boost Factor failed"));
			goto fail;
		}
		roam_params.raise_factor_5g = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_BOOST_FACTOR]);
		hddLog(VOS_TRACE_LEVEL_DEBUG, FL("5G Boost Factor (%d)"),
			roam_params.raise_factor_5g);
		/* Parse and fetch 5G Penalty factor */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_FACTOR]) {
			hddLog(LOGE, FL("5G Penalty Factor failed"));
			goto fail;
		}
		roam_params.drop_factor_5g = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_PENALTY_FACTOR]);
		hddLog(VOS_TRACE_LEVEL_DEBUG,
			FL("5G Penalty factor (%d)"),
			roam_params.drop_factor_5g);
		/* Parse and fetch 5G Max Boost */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_MAX_BOOST]) {
			hddLog(LOGE, FL("5G Max Boost failed"));
			goto fail;
		}
		roam_params.max_raise_rssi_5g = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_A_BAND_MAX_BOOST]);
		hddLog(VOS_TRACE_LEVEL_DEBUG, FL("5G Max Boost (%d)"),
			roam_params.max_raise_rssi_5g);
		/* Parse and fetch Rssi Diff */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_LAZY_ROAM_HISTERESYS]) {
			hddLog(LOGE, FL("Rssi Diff failed"));
			goto fail;
		}
		roam_params.rssi_diff = nla_get_s32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_LAZY_ROAM_HISTERESYS]);
		hddLog(VOS_TRACE_LEVEL_DEBUG, FL("RSSI Diff (%d)"),
			roam_params.rssi_diff);
		/* Parse and fetch Good Rssi Threshold */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALERT_ROAM_RSSI_TRIGGER]) {
			hddLog(LOGE, FL("Alert Rssi Threshold failed"));
			goto fail;
		}
		roam_params.alert_rssi_threshold = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_ALERT_ROAM_RSSI_TRIGGER]);
		hddLog(VOS_TRACE_LEVEL_DEBUG,
			FL("Alert RSSI Threshold (%d)"),
			roam_params.alert_rssi_threshold);
		sme_update_roam_params(pHddCtx->hHal, session_id,
			roam_params,
			REASON_ROAM_EXT_SCAN_PARAMS_CHANGED);
		break;
	case QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_SET_LAZY_ROAM:
		/* Parse and fetch Activate Good Rssi Roam */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_ENABLE]) {
			hddLog(LOGE, FL("Activate Good Rssi Roam failed"));
			goto fail;
		}
		roam_params.good_rssi_roam = nla_get_s32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_ENABLE]);
		hddLog(VOS_TRACE_LEVEL_DEBUG,
			FL("Activate Good Rssi Roam (%d)"),
			roam_params.good_rssi_roam);
		sme_update_roam_params(pHddCtx->hHal, session_id,
			roam_params, REASON_ROAM_GOOD_RSSI_CHANGED);
		break;
	case QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_SET_BSSID_PREFS:
		/* Parse and fetch number of preferred BSSID */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_NUM_BSSID]) {
			hddLog(LOGE, FL("attr num of preferred bssid failed"));
			goto fail;
		}
		roam_params.num_bssid_favored = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_NUM_BSSID]);
		hddLog(VOS_TRACE_LEVEL_DEBUG,
			FL("Num of Preferred BSSID (%d)"),
			roam_params.num_bssid_favored);
		i = 0;
		nla_for_each_nested(curr_attr,
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PREFS],
			rem) {
			if (nla_parse(tb2,
				QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX,
				nla_data(curr_attr), nla_len(curr_attr),
				NULL)) {
				hddLog(LOGE, FL("nla_parse failed"));
				goto fail;
			}
			/* Parse and fetch MAC address */
			if (!tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_BSSID]) {
				hddLog(LOGE, FL("attr mac address failed"));
				goto fail;
			}
			nla_memcpy(roam_params.bssid_favored[i],
				tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_BSSID],
				sizeof(tSirMacAddr));
			hddLog(VOS_TRACE_LEVEL_DEBUG, MAC_ADDRESS_STR,
				MAC_ADDR_ARRAY(roam_params.bssid_favored[i]));
			/* Parse and fetch preference factor*/
			if (!tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_RSSI_MODIFIER]) {
				hddLog(LOGE, FL("BSSID Preference score failed"));
				goto fail;
			}
			roam_params.bssid_favored_factor[i] = nla_get_u32(
				tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_LAZY_ROAM_RSSI_MODIFIER]);
			hddLog(VOS_TRACE_LEVEL_DEBUG,
				FL("BSSID Preference score (%d)"),
				roam_params.bssid_favored_factor[i]);
			i++;
		}
		sme_update_roam_params(pHddCtx->hHal, session_id,
			roam_params, REASON_ROAM_SET_FAVORED_BSSID);
		break;
	case QCA_WLAN_VENDOR_ATTR_ROAM_SUBCMD_SET_BLACKLIST_BSSID:
		/* Parse and fetch number of blacklist BSSID */
		if (!tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_NUM_BSSID]) {
			hddLog(LOGE, FL("attr num of blacklist bssid failed"));
			goto fail;
		}
		roam_params.num_bssid_avoid_list = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_NUM_BSSID]);
		hddLog(VOS_TRACE_LEVEL_DEBUG,
			FL("Num of blacklist BSSID (%d)"),
			roam_params.num_bssid_avoid_list);
		i = 0;
		nla_for_each_nested(curr_attr,
			tb[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS],
			rem) {
			if (nla_parse(tb2,
				QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_MAX,
				nla_data(curr_attr), nla_len(curr_attr),
				NULL)) {
				hddLog(LOGE, FL("nla_parse failed"));
				goto fail;
			}
			/* Parse and fetch MAC address */
			if (!tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_BSSID]) {
				hddLog(LOGE, FL("attr blacklist addr failed"));
				goto fail;
			}
			nla_memcpy(roam_params.bssid_avoid_list[i],
				tb2[QCA_WLAN_VENDOR_ATTR_ROAMING_PARAM_SET_BSSID_PARAMS_BSSID],
				sizeof(tSirMacAddr));
			hddLog(VOS_TRACE_LEVEL_DEBUG, MAC_ADDRESS_STR,
				MAC_ADDR_ARRAY(
				roam_params.bssid_avoid_list[i]));
			i++;
		}
		sme_update_roam_params(pHddCtx->hHal, session_id,
			roam_params, REASON_ROAM_SET_BLACKLIST_BSSID);
		break;
	}
	return 0;
fail:
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_set_ext_roam_params() - set ext scan roam params
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_set_ext_roam_params(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data,
				int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_ext_roam_params(wiphy, wdev,
							data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

#ifdef WLAN_FEATURE_STATS_EXT
static int wlan_hdd_cfg80211_stats_ext_request(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        const void *data,
                                        int data_len)
{
    tStatsExtRequestReq stats_ext_req;
    struct net_device *dev = wdev->netdev;
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    int ret_val = -1;
    eHalStatus status;

    stats_ext_req.request_data_len = data_len;
    stats_ext_req.request_data = (void *)data;

    status = sme_StatsExtRequest(pAdapter->sessionId, &stats_ext_req);

    if (eHAL_STATUS_SUCCESS == status)
        ret_val = 0;

    return ret_val;
}

static void wlan_hdd_cfg80211_stats_ext_callback(void* ctx, tStatsExtEvent* msg)
{

    hdd_context_t *pHddCtx = (hdd_context_t *)ctx;
    struct sk_buff *vendor_event;
    int status;
    int ret_val;
    tStatsExtEvent *data = msg;
    hdd_adapter_t *pAdapter = NULL;

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return;
    }

    pAdapter = hdd_get_adapter_by_vdev( pHddCtx, data->vdev_id);

    if (NULL == pAdapter)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: vdev_id %d does not exist with host",
                  __func__, data->vdev_id);
        return;
    }


    vendor_event = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                                               data->event_data_len +
                                               sizeof(tANI_U32) +
                                               NLMSG_HDRLEN + NLMSG_HDRLEN,
                                               QCA_NL80211_VENDOR_SUBCMD_STATS_EXT_INDEX,
                                               GFP_KERNEL);

    if (!vendor_event)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: cfg80211_vendor_event_alloc failed", __func__);
        return;
    }

    ret_val = nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_IFINDEX,
                          pAdapter->dev->ifindex);
    if (ret_val)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: QCA_WLAN_VENDOR_ATTR_IFINDEX put fail", __func__);
        kfree_skb(vendor_event);

        return;
    }


    ret_val = nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_STATS_EXT,
                      data->event_data_len, data->event_data);

    if (ret_val)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: QCA_WLAN_VENDOR_ATTR_STATS_EXT put fail", __func__);
        kfree_skb(vendor_event);

        return;
    }

    cfg80211_vendor_event(vendor_event, GFP_KERNEL);

}


void wlan_hdd_cfg80211_stats_ext_init(hdd_context_t *pHddCtx)
{
    sme_StatsExtRegisterCallback(pHddCtx->hHal,
                                 wlan_hdd_cfg80211_stats_ext_callback);
}

#endif

#ifdef FEATURE_WLAN_EXTSCAN

/*
 * define short names for the global vendor params
 * used by wlan_hdd_send_ext_scan_capability()
 */
#define PARAM_REQUEST_ID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID
#define PARAM_STATUS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_STATUS
#define MAX_SCAN_CACHE_SIZE \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SCAN_CACHE_SIZE
#define MAX_SCAN_BUCKETS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SCAN_BUCKETS
#define MAX_AP_CACHE_PER_SCAN \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_AP_CACHE_PER_SCAN
#define MAX_RSSI_SAMPLE_SIZE \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_RSSI_SAMPLE_SIZE
#define MAX_SCAN_RPT_THRHOLD \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SCAN_REPORTING_THRESHOLD
#define MAX_HOTLIST_BSSIDS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_HOTLIST_BSSIDS
#define MAX_SIGNIFICANT_WIFI_CHANGE_APS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SIGNIFICANT_WIFI_CHANGE_APS
#define MAX_BSSID_HISTORY_ENTRIES \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_BSSID_HISTORY_ENTRIES
#define MAX_HOTLIST_SSIDS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_HOTLIST_SSIDS
#define MAX_NUM_EPNO_NETS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_NUM_EPNO_NETS
#define MAX_NUM_EPNO_NETS_BY_SSID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_NUM_EPNO_NETS_BY_SSID
#define MAX_NUM_WHITELISTED_SSID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_NUM_WHITELISTED_SSID

/**
 * wlan_hdd_send_ext_scan_capability - send ext scan capability to user space
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_send_ext_scan_capability(hdd_context_t *hdd_ctx)
{
	int ret;
	struct sk_buff *skb;
	struct ext_scan_capabilities_response *data;
	uint32_t nl_buf_len;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret) {
		hddLog(LOGE, FL("hdd_context is invalid"));
		return ret;
	}

	data = &(hdd_ctx->ext_scan_context.capability_response);

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len += (sizeof(data->requestId) + NLA_HDRLEN) +
	(sizeof(data->status) + NLA_HDRLEN) +
	(sizeof(data->max_scan_cache_size) + NLA_HDRLEN) +
	(sizeof(data->max_scan_buckets) + NLA_HDRLEN) +
	(sizeof(data->max_ap_cache_per_scan) + NLA_HDRLEN) +
	(sizeof(data->max_rssi_sample_size) + NLA_HDRLEN) +
	(sizeof(data->max_scan_reporting_threshold) + NLA_HDRLEN) +
	(sizeof(data->max_hotlist_bssids) + NLA_HDRLEN) +
	(sizeof(data->max_significant_wifi_change_aps) + NLA_HDRLEN) +
	(sizeof(data->max_bssid_history_entries) + NLA_HDRLEN) +
	(sizeof(data->max_hotlist_ssids) + NLA_HDRLEN) +
	(sizeof(data->max_number_epno_networks) + NLA_HDRLEN) +
	(sizeof(data->max_number_epno_networks_by_ssid) + NLA_HDRLEN) +
	(sizeof(data->max_number_of_white_listed_ssid) + NLA_HDRLEN);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);

	if (!skb) {
		hddLog(LOGE, FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
		return -ENOMEM;
	}

	hddLog(LOG1, "Req Id (%u)", data->requestId);
	hddLog(LOG1, "Status (%u)", data->status);
	hddLog(LOG1, "Scan cache size (%u)", data->max_scan_cache_size);
	hddLog(LOG1, "Scan buckets (%u)", data->max_scan_buckets);
	hddLog(LOG1, "Max AP per scan (%u)", data->max_ap_cache_per_scan);
	hddLog(LOG1, "max_rssi_sample_size (%u)",
					data->max_rssi_sample_size);
	hddLog(LOG1, "max_scan_reporting_threshold (%u)",
					data->max_scan_reporting_threshold);
	hddLog(LOG1, "max_hotlist_bssids (%u)", data->max_hotlist_bssids);
	hddLog(LOG1, "max_significant_wifi_change_aps (%u)",
					data->max_significant_wifi_change_aps);
	hddLog(LOG1, "max_bssid_history_entries (%u)",
					data->max_bssid_history_entries);
	hddLog(LOG1, "max_hotlist_ssids (%u)", data->max_hotlist_ssids);
	hddLog(LOG1, "max_number_epno_networks (%u)",
					data->max_number_epno_networks);
	hddLog(LOG1, "max_number_epno_networks_by_ssid (%u)",
					data->max_number_epno_networks_by_ssid);
	hddLog(LOG1, "max_number_of_white_listed_ssid (%u)",
					data->max_number_of_white_listed_ssid);

	if (nla_put_u32(skb, PARAM_REQUEST_ID, data->requestId) ||
	    nla_put_u32(skb, PARAM_STATUS, data->status) ||
	    nla_put_u32(skb, MAX_SCAN_CACHE_SIZE, data->max_scan_cache_size) ||
	    nla_put_u32(skb, MAX_SCAN_BUCKETS, data->max_scan_buckets) ||
	    nla_put_u32(skb, MAX_AP_CACHE_PER_SCAN,
			data->max_ap_cache_per_scan) ||
	    nla_put_u32(skb, MAX_RSSI_SAMPLE_SIZE,
			data->max_rssi_sample_size) ||
	    nla_put_u32(skb, MAX_SCAN_RPT_THRHOLD,
			data->max_scan_reporting_threshold) ||
	    nla_put_u32(skb, MAX_HOTLIST_BSSIDS, data->max_hotlist_bssids) ||
	    nla_put_u32(skb, MAX_SIGNIFICANT_WIFI_CHANGE_APS,
			data->max_significant_wifi_change_aps) ||
	    nla_put_u32(skb, MAX_BSSID_HISTORY_ENTRIES,
			data->max_bssid_history_entries) ||
	    nla_put_u32(skb, MAX_HOTLIST_SSIDS,	data->max_hotlist_ssids) ||
	    nla_put_u32(skb, MAX_NUM_EPNO_NETS,
			data->max_number_epno_networks) ||
	    nla_put_u32(skb, MAX_NUM_EPNO_NETS_BY_SSID,
			data->max_number_epno_networks_by_ssid) ||
	    nla_put_u32(skb, MAX_NUM_WHITELISTED_SSID,
			data->max_number_of_white_listed_ssid)) {
		hddLog(LOGE, FL("nla put fail"));
		goto nla_put_failure;
	}

	cfg80211_vendor_cmd_reply(skb);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -EINVAL;
}
/*
 * done with short names for the global vendor params
 * used by wlan_hdd_send_ext_scan_capability()
 */
#undef PARAM_REQUEST_ID
#undef PARAM_STATUS
#undef MAX_SCAN_CACHE_SIZE
#undef MAX_SCAN_BUCKETS
#undef MAX_AP_CACHE_PER_SCAN
#undef MAX_RSSI_SAMPLE_SIZE
#undef MAX_SCAN_RPT_THRHOLD
#undef MAX_HOTLIST_BSSIDS
#undef MAX_SIGNIFICANT_WIFI_CHANGE_APS
#undef MAX_BSSID_HISTORY_ENTRIES
#undef MAX_HOTLIST_SSIDS
#undef MAX_NUM_EPNO_NETS
#undef MAX_NUM_EPNO_NETS_BY_SSID
#undef MAX_NUM_WHITELISTED_SSID

static int __wlan_hdd_cfg80211_extscan_get_capabilities(struct wiphy *wiphy,
                                                struct wireless_dev *wdev,
                                                const void *data,
                                                int data_len)
{
    int ret;
    unsigned long rc;
    struct hdd_ext_scan_context *context;
    tpSirGetExtScanCapabilitiesReqParams pReqMsg = NULL;
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                     = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    eHalStatus status;

    ENTER();

    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, data_len,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    pReqMsg = vos_mem_malloc(sizeof(*pReqMsg));
    if (!pReqMsg) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("vos_mem_malloc failed"));
        return -ENOMEM;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr request id failed"));
        goto fail;
    }

    pReqMsg->requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Req Id %d"), pReqMsg->requestId);

    pReqMsg->sessionId = pAdapter->sessionId;
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Session Id %d"), pReqMsg->sessionId);

    spin_lock(&hdd_context_lock);
    context = &pHddCtx->ext_scan_context;
    context->request_id = pReqMsg->requestId;
    INIT_COMPLETION(context->response_event);
    spin_unlock(&hdd_context_lock);


    status = sme_ExtScanGetCapabilities(pHddCtx->hHal, pReqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("sme_ExtScanGetCapabilities failed(err=%d)"), status);
        goto fail;
    }

    rc = wait_for_completion_timeout(&context->response_event,
             msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));
    if (!rc) {
        hddLog(LOGE, FL("Target response timed out"));
        return -ETIMEDOUT;
    }

    ret = wlan_hdd_send_ext_scan_capability(pHddCtx);
    if (ret)
        hddLog(LOGE, FL("Failed to send ext scan capability to user space"));

    return ret;

fail:
    vos_mem_free(pReqMsg);
    return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_extscan_get_capabilities() - get ext scan capabilities
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_extscan_get_capabilities(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_get_capabilities(wiphy, wdev, data,
		data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}



/*
 * define short names for the global vendor params
 * used by wlan_hdd_cfg80211_extscan_get_cached_results()
 */
#define PARAM_MAX \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX
#define PARAM_REQUEST_ID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID
#define PARAM_FLUSH \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_FLUSH
/**
 * __wlan_hdd_cfg80211_extscan_get_cached_results() - extscan get cached results
 * @wiphy: wiphy pointer
 * @wdev: pointer to struct wireless_dev
 * @data: pointer to incoming NL vendor data
 * @data_len: length of @data
 *
 * This function parses the incoming NL vendor command data attributes and
 * invokes the SME Api and blocks on a completion variable.
 * Each WMI event with cached scan results data chunk results in
 * function call wlan_hdd_cfg80211_extscan_cached_results_ind and each
 * data chunk is sent up the layer in cfg80211_vendor_cmd_alloc_reply_skb.
 *
 * If timeout happens before receiving all of the data, this function sets
 * a context variable @ignore_cached_results to %true, all of the next data
 * chunks are checked against this variable and dropped.
 *
 * Return: 0 on success; error number otherwise.
 */
static int __wlan_hdd_cfg80211_extscan_get_cached_results(struct wiphy *wiphy,
                                                  struct wireless_dev *wdev,
                                                  const void *data,
                                                  int data_len)
{
	tpSirExtScanGetCachedResultsReqParams pReqMsg = NULL;
	struct net_device *dev                      = wdev->netdev;
	hdd_adapter_t *pAdapter                     = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx                      = wiphy_priv(wiphy);
	struct hdd_ext_scan_context *context;
	struct nlattr *tb[PARAM_MAX + 1];
	eHalStatus status;
	int retval = 0;
	unsigned long rc;

	ENTER();

	status = wlan_hdd_validate_context(pHddCtx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	if (nla_parse(tb, PARAM_MAX, data, data_len,
			wlan_hdd_extscan_config_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	pReqMsg = vos_mem_malloc(sizeof(*pReqMsg));
	if (!pReqMsg) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}

	/* Parse and fetch request Id */
	if (!tb[PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		goto fail;
	}

	pReqMsg->requestId = nla_get_u32(tb[PARAM_REQUEST_ID]);
	pReqMsg->sessionId = pAdapter->sessionId;
	hddLog(LOG1, FL("Req Id %u Session Id %d"),
		pReqMsg->requestId, pReqMsg->sessionId);

	/* Parse and fetch flush parameter */
	if (!tb[PARAM_FLUSH]) {
		hddLog(LOGE, FL("attr flush failed"));
		goto fail;
	}
	pReqMsg->flush = nla_get_u8(tb[PARAM_FLUSH]);
	hddLog(LOG1, FL("Flush %d"), pReqMsg->flush);

	spin_lock(&hdd_context_lock);
	context = &pHddCtx->ext_scan_context;
	context->request_id = pReqMsg->requestId;
	context->ignore_cached_results = false;
	INIT_COMPLETION(context->response_event);
	spin_unlock(&hdd_context_lock);

	status = sme_getCachedResults(pHddCtx->hHal, pReqMsg);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE,
			FL("sme_getCachedResults failed(err=%d)"), status);
		goto fail;
	}

	rc = wait_for_completion_timeout(&context->response_event,
			msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));
	if (!rc) {
		hddLog(LOGE, FL("Target response timed out"));
		retval = -ETIMEDOUT;
		spin_lock(&hdd_context_lock);
		context->ignore_cached_results = true;
		spin_unlock(&hdd_context_lock);
	} else {
		spin_lock(&hdd_context_lock);
		retval = context->response_status;
		spin_unlock(&hdd_context_lock);
	}

	return retval;

fail:
	vos_mem_free(pReqMsg);
	return -EINVAL;
}
/*
 * done with short names for the global vendor params
 * used by wlan_hdd_cfg80211_extscan_get_cached_results()
 */
#undef PARAM_MAX
#undef PARAM_REQUEST_ID
#undef PARAM_FLUSH

/**
 * wlan_hdd_cfg80211_extscan_get_cached_results() - extscan get cached results
 * @wiphy: wiphy pointer
 * @wdev: pointer to struct wireless_dev
 * @data: pointer to incoming NL vendor data
 * @data_len: length of @data
 *
 * This function parses the incoming NL vendor command data attributes and
 * invokes the SME Api and blocks on a completion variable.
 * Each WMI event with cached scan results data chunk results in
 * function call wlan_hdd_cfg80211_extscan_cached_results_ind and each
 * data chunk is sent up the layer in cfg80211_vendor_cmd_alloc_reply_skb.
 *
 * If timeout happens before receiving all of the data, this function sets
 * a context variable @ignore_cached_results to %true, all of the next data
 * chunks are checked against this variable and dropped.
 *
 * Return: 0 on success; error number otherwise.
 */
static int wlan_hdd_cfg80211_extscan_get_cached_results(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_get_cached_results(wiphy, wdev, data,
								data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}


static int __wlan_hdd_cfg80211_extscan_set_bssid_hotlist(struct wiphy *wiphy,
                                                 struct wireless_dev *wdev,
                                                 const void *data,
                                                 int data_len)
{
    tpSirExtScanSetBssidHotListReqParams pReqMsg = NULL;
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                     = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr *apTh;
    struct hdd_ext_scan_context *context;
    uint32_t request_id;
    eHalStatus status;
    tANI_U8 i;
    int rem, retval;
    unsigned long rc;

    ENTER();

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, data_len,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    pReqMsg = vos_mem_malloc(sizeof(*pReqMsg));
    if (!pReqMsg) {
        hddLog(LOGE, FL("vos_mem_malloc failed"));
        return -ENOMEM;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(LOGE, FL("attr request id failed"));
        goto fail;
    }

    pReqMsg->requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(LOG1, FL("Req Id %d"), pReqMsg->requestId);

    /* Parse and fetch number of APs */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_NUM_AP]) {
        hddLog(LOGE, FL("attr number of AP failed"));
        goto fail;
    }
    pReqMsg->numAp = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_NUM_AP]);
    hddLog(LOG1, FL("Number of AP %d"), pReqMsg->numAp);

    /* Parse and fetch lost ap sample size */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_LOST_AP_SAMPLE_SIZE]) {
        hddLog(LOGE, FL("attr lost ap sample size failed"));
        goto fail;
    }

    pReqMsg->lost_ap_sample_size = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_LOST_AP_SAMPLE_SIZE]);
    hddLog(LOG1, FL("Lost ap sample size %d"), pReqMsg->lost_ap_sample_size);

    pReqMsg->sessionId = pAdapter->sessionId;
    hddLog(LOG1, FL("Session Id %d"), pReqMsg->sessionId);

    i = 0;
    nla_for_each_nested(apTh,
                tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM], rem) {
        if (nla_parse(tb2, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                nla_data(apTh), nla_len(apTh),
                wlan_hdd_extscan_config_policy)) {
            hddLog(LOGE, FL("nla_parse failed"));
            goto fail;
        }

        /* Parse and fetch MAC address */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID]) {
            hddLog(LOGE, FL("attr mac address failed"));
            goto fail;
        }
        nla_memcpy(pReqMsg->ap[i].bssid,
                tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID],
                sizeof(tSirMacAddr));
        hddLog(LOG1, MAC_ADDRESS_STR,
               MAC_ADDR_ARRAY(pReqMsg->ap[i].bssid));

        /* Parse and fetch low RSSI */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW]) {
            hddLog(LOGE, FL("attr low RSSI failed"));
            goto fail;
        }
        pReqMsg->ap[i].low = nla_get_s32(
             tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW]);
        hddLog(LOG1, FL("RSSI low %d"), pReqMsg->ap[i].low);

        /* Parse and fetch high RSSI */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH]) {
            hddLog(LOGE, FL("attr high RSSI failed"));
            goto fail;
        }
        pReqMsg->ap[i].high = nla_get_s32(
            tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH]);
        hddLog(LOG1, FL("RSSI High %d"), pReqMsg->ap[i].high);
        i++;
    }

    context = &pHddCtx->ext_scan_context;
    spin_lock(&hdd_context_lock);
    INIT_COMPLETION(context->response_event);
    context->request_id = request_id = pReqMsg->requestId;
    spin_unlock(&hdd_context_lock);

    status = sme_SetBssHotlist(pHddCtx->hHal, pReqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(LOGE, FL("sme_SetBssHotlist failed(err=%d)"), status);
        goto fail;
    }

    /* request was sent -- wait for the response */
    rc = wait_for_completion_timeout(&context->response_event,
                                     msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));

    if (!rc) {
       hddLog(LOGE, FL("sme_SetBssHotlist timed out"));
       retval = -ETIMEDOUT;
    } else {
       spin_lock(&hdd_context_lock);
       if (context->request_id == request_id)
          retval = context->response_status;
       else
          retval = -EINVAL;
       spin_unlock(&hdd_context_lock);
    }

    return retval;

fail:
    vos_mem_free(pReqMsg);
    return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_extscan_set_bssid_hotlist() - set ext scan bssid hotlist
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_cfg80211_extscan_set_bssid_hotlist(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_set_bssid_hotlist(wiphy, wdev, data,
					data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}


/*
 * define short names for the global vendor params
 * used by wlan_hdd_cfg80211_extscan_set_ssid_hotlist()
 */
#define PARAM_MAX \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX
#define PARAM_REQUEST_ID \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID
#define PARAMS_LOST_SSID_SAMPLE_SIZE \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_HOTLIST_PARAMS_LOST_SSID_SAMPLE_SIZE
#define PARAMS_NUM_SSID \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_HOTLIST_PARAMS_NUM_SSID
#define THRESHOLD_PARAM \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM
#define PARAM_SSID \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_SSID
#define PARAM_BAND \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_BAND
#define PARAM_RSSI_LOW \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_RSSI_LOW
#define PARAM_RSSI_HIGH \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_RSSI_HIGH

/**
 * __wlan_hdd_cfg80211_extscan_set_ssid_hotlist() - set ssid hot list
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_extscan_set_ssid_hotlist(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	struct sir_set_ssid_hotlist_request *request;
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[PARAM_MAX + 1];
	struct nlattr *tb2[PARAM_MAX + 1];
	struct nlattr *ssids;
	struct hdd_ext_scan_context *context;
	uint32_t request_id;
	char ssid_string[SIR_MAC_MAX_SSID_LENGTH + 1];
	int ssid_len;
	eHalStatus status;
	int i, rem, retval;
	unsigned long rc;

	ENTER();

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (0 != retval) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	if (nla_parse(tb, PARAM_MAX,
		      data, data_len,
		      wlan_hdd_extscan_config_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	request = vos_mem_malloc(sizeof(*request));
	if (!request) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}

	/* Parse and fetch request Id */
	if (!tb[PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		goto fail;
	}

	request->request_id = nla_get_u32(tb[PARAM_REQUEST_ID]);
	hddLog(LOG1, FL("Request Id %d"), request->request_id);

	/* Parse and fetch lost SSID sample size */
	if (!tb[PARAMS_LOST_SSID_SAMPLE_SIZE]) {
		hddLog(LOGE, FL("attr number of Ssid failed"));
		goto fail;
	}
	request->lost_ssid_sample_size =
		nla_get_u32(tb[PARAMS_LOST_SSID_SAMPLE_SIZE]);
	hddLog(LOG1, FL("Lost SSID Sample Size %d"),
	       request->lost_ssid_sample_size);

	/* Parse and fetch number of hotlist SSID */
	if (!tb[PARAMS_NUM_SSID]) {
		hddLog(LOGE, FL("attr number of Ssid failed"));
		goto fail;
	}
	request->ssid_count = nla_get_u32(tb[PARAMS_NUM_SSID]);
	hddLog(LOG1, FL("Number of SSID %d"), request->ssid_count);

	request->session_id = adapter->sessionId;
	hddLog(LOG1, FL("Session Id (%d)"), request->session_id);

	i = 0;
	nla_for_each_nested(ssids, tb[THRESHOLD_PARAM], rem) {
		if (i >= WLAN_EXTSCAN_MAX_HOTLIST_SSIDS) {
			hddLog(LOGE,
			       FL("Too Many SSIDs, %d exceeds %d"),
			       i, WLAN_EXTSCAN_MAX_HOTLIST_SSIDS);
			break;
		}
		if (nla_parse(tb2, PARAM_MAX,
			      nla_data(ssids), nla_len(ssids),
			      wlan_hdd_extscan_config_policy)) {
			hddLog(LOGE, FL("nla_parse failed"));
			goto fail;
		}

		/* Parse and fetch SSID */
		if (!tb2[PARAM_SSID]) {
			hddLog(LOGE, FL("attr ssid failed"));
			goto fail;
		}
		nla_memcpy(ssid_string,
			   tb2[PARAM_SSID],
			   sizeof(ssid_string));
		hddLog(LOG1, FL("SSID %s"),
		       ssid_string);
		ssid_len = strlen(ssid_string);
		memcpy(request->ssids[i].ssid.ssId, ssid_string, ssid_len);
		request->ssids[i].ssid.length = ssid_len;

		/* Parse and fetch low RSSI */
		if (!tb2[PARAM_BAND]) {
			hddLog(LOGE, FL("attr band failed"));
			goto fail;
		}
		request->ssids[i].band = nla_get_u8(tb2[PARAM_BAND]);
		hddLog(LOG1, FL("band %d"), request->ssids[i].band);

		/* Parse and fetch low RSSI */
		if (!tb2[PARAM_RSSI_LOW]) {
			hddLog(LOGE, FL("attr low RSSI failed"));
			goto fail;
		}
		request->ssids[i].rssi_low = nla_get_s32(tb2[PARAM_RSSI_LOW]);
		hddLog(LOG1, FL("RSSI low %d"), request->ssids[i].rssi_low);

		/* Parse and fetch high RSSI */
		if (!tb2[PARAM_RSSI_HIGH]) {
			hddLog(LOGE, FL("attr high RSSI failed"));
			goto fail;
		}
		request->ssids[i].rssi_high = nla_get_u32(tb2[PARAM_RSSI_HIGH]);
		hddLog(LOG1, FL("RSSI high %d"), request->ssids[i].rssi_high);
		i++;
	}

	context = &hdd_ctx->ext_scan_context;
	spin_lock(&hdd_context_lock);
	INIT_COMPLETION(context->response_event);
	context->request_id = request_id = request->request_id;
	spin_unlock(&hdd_context_lock);

	status = sme_set_ssid_hotlist(hdd_ctx->hHal, request);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE,
		       FL("sme_set_ssid_hotlist failed(err=%d)"), status);
		goto fail;
	}

	vos_mem_free(request);

	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout(&context->response_event,
					 msecs_to_jiffies
						(WLAN_WAIT_TIME_EXTSCAN));
	if (!rc) {
		hddLog(LOGE, FL("sme_set_ssid_hotlist timed out"));
		retval = -ETIMEDOUT;
	} else {
		spin_lock(&hdd_context_lock);
		if (context->request_id == request_id)
			retval = context->response_status;
		else
			retval = -EINVAL;
		spin_unlock(&hdd_context_lock);
	}

	return retval;

fail:
	vos_mem_free(request);
	return -EINVAL;
}

/*
 * done with short names for the global vendor params
 * used by wlan_hdd_cfg80211_extscan_set_ssid_hotlist()
 */
#undef PARAM_MAX
#undef PARAM_REQUEST_ID
#undef PARAMS_NUM_SSID
#undef THRESHOLD_PARAM
#undef PARAM_SSID
#undef PARAM_BAND
#undef PARAM_RSSI_LOW
#undef PARAM_RSSI_HIGH

/**
 * wlan_hdd_cfg80211_extscan_set_ssid_hotlist() - set ssid hot list
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
wlan_hdd_cfg80211_extscan_set_ssid_hotlist(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_set_ssid_hotlist(wiphy, wdev, data,
							data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

static int __wlan_hdd_cfg80211_extscan_set_significant_change(
                                        struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        const void *data,
                                        int data_len)
{
    tpSirExtScanSetSigChangeReqParams pReqMsg = NULL;
    struct net_device *dev                  = wdev->netdev;
    hdd_adapter_t *pAdapter                 = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                  = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct nlattr *apTh;
    struct hdd_ext_scan_context *context;
    uint32_t request_id;
    eHalStatus status;
    tANI_U8 i;
    int rem;
    int retval;
    unsigned long rc;

    ENTER();

    retval = wlan_hdd_validate_context(pHddCtx);
    if (0 != retval) {
       hddLog(LOGE, FL("HDD context is not valid"));
       return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, data_len,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(LOGE, FL("Invalid ATTR"));
        return -EINVAL;
    }

    pReqMsg = vos_mem_malloc(sizeof(*pReqMsg));
    if (!pReqMsg) {
        hddLog(LOGE, FL("vos_mem_malloc failed"));
        return -ENOMEM;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(LOGE, FL("attr request id failed"));
        goto fail;
    }

    pReqMsg->requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(LOG1, FL("Req Id %d"), pReqMsg->requestId);

    /* Parse and fetch RSSI sample size */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_RSSI_SAMPLE_SIZE])
    {
        hddLog(LOGE, FL("attr RSSI sample size failed"));
        goto fail;
    }
    pReqMsg->rssiSampleSize = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_RSSI_SAMPLE_SIZE]);
    hddLog(LOG1, FL("RSSI sample size %u"), pReqMsg->rssiSampleSize);

    /* Parse and fetch lost AP sample size */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_LOST_AP_SAMPLE_SIZE])
    {
        hddLog(LOGE, FL("attr lost AP sample size failed"));
        goto fail;
    }
    pReqMsg->lostApSampleSize = nla_get_u32(
       tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_LOST_AP_SAMPLE_SIZE]);
    hddLog(LOG1, FL("Lost AP sample size %u"), pReqMsg->lostApSampleSize);

    /* Parse and fetch AP min breaching */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_MIN_BREACHING])
    {
        hddLog(LOGE, FL("attr AP min breaching"));
        goto fail;
    }
    pReqMsg->minBreaching = nla_get_u32(
     tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_MIN_BREACHING]);
    hddLog(LOG1, FL("AP min breaching %u"), pReqMsg->minBreaching);

    /* Parse and fetch number of APs */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_NUM_AP]) {
        hddLog(LOGE, FL("attr number of AP failed"));
        goto fail;
    }
    pReqMsg->numAp = nla_get_u32(
            tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_NUM_AP]);

    pReqMsg->sessionId = pAdapter->sessionId;
    hddLog(LOG1, FL("Number of AP %d Session Id %d"), pReqMsg->numAp,
           pReqMsg->sessionId);

    i = 0;
    nla_for_each_nested(apTh,
                tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM], rem) {
        if (nla_parse(tb2,
                QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                nla_data(apTh), nla_len(apTh),
                wlan_hdd_extscan_config_policy)) {
            hddLog(LOGE, FL("nla_parse failed"));
            goto fail;
        }

        /* Parse and fetch MAC address */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID]) {
            hddLog(LOGE, FL("attr mac address failed"));
            goto fail;
        }
        nla_memcpy(pReqMsg->ap[i].bssid,
                tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID],
                sizeof(tSirMacAddr));
        hddLog(LOG1, MAC_ADDRESS_STR,
               MAC_ADDR_ARRAY(pReqMsg->ap[i].bssid));

        /* Parse and fetch low RSSI */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW]) {
            hddLog(LOGE, FL("attr low RSSI failed"));
            goto fail;
        }
        pReqMsg->ap[i].low = nla_get_s32(
             tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW]);
        hddLog(LOG1, FL("RSSI low %d"), pReqMsg->ap[i].low);

        /* Parse and fetch high RSSI */
        if (!tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH]) {
            hddLog(LOGE, FL("attr high RSSI failed"));
            goto fail;
        }
        pReqMsg->ap[i].high = nla_get_s32(
            tb2[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH]);
        hddLog(LOG1, FL("RSSI High %d"), pReqMsg->ap[i].high);

        i++;
    }

    context = &pHddCtx->ext_scan_context;
    spin_lock(&hdd_context_lock);
    INIT_COMPLETION(context->response_event);
    context->request_id = request_id = pReqMsg->requestId;
    spin_unlock(&hdd_context_lock);

    status = sme_SetSignificantChange(pHddCtx->hHal, pReqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(LOGE, FL("sme_SetSignificantChange failed(err=%d)"), status);
        goto fail;
    }

    /* request was sent -- wait for the response */
    rc = wait_for_completion_timeout(&context->response_event,
                                     msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));

    if (!rc) {
       hddLog(LOGE, FL("sme_SetSignificantChange timed out"));
       retval = -ETIMEDOUT;
    } else {
       spin_lock(&hdd_context_lock);
       if (context->request_id == request_id)
          retval = context->response_status;
       else
          retval = -EINVAL;
       spin_unlock(&hdd_context_lock);
    }

    return retval;

fail:
    vos_mem_free(pReqMsg);
    return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_extscan_set_significant_change() - set significant change
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_hdd_cfg80211_extscan_set_significant_change(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_set_significant_change(wiphy, wdev,
					data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

static int __wlan_hdd_cfg80211_extscan_get_valid_channels(struct wiphy *wiphy,
                                                     struct wireless_dev *wdev,
                                                     const void *data,
                                                     int data_len)
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    struct net_device *dev = wdev->netdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    uint32_t chan_list[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
    uint8_t num_channels  = 0;
    uint8_t num_chan_new  = 0;
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    tANI_U32 requestId, maxChannels;
    tWifiBand wifiBand;
    eHalStatus status;
    struct sk_buff *reply_skb;
    tANI_U8 i, j, k;
    int retval;

    ENTER();

    retval = wlan_hdd_validate_context(pHddCtx);
    if (0 != retval) {
       hddLog(LOGE, FL("HDD context is not valid"));
       return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                  data, data_len,
                  wlan_hdd_extscan_config_policy)) {
        hddLog(LOGE, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(LOGE, FL("attr request id failed"));
        return -EINVAL;
    }
    requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);
    hddLog(LOG1, FL("Req Id %d"), requestId);

    /* Parse and fetch wifi band */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_VALID_CHANNELS_CONFIG_PARAM_WIFI_BAND]) {
        hddLog(LOGE, FL("attr wifi band failed"));
        return -EINVAL;
    }
    wifiBand = nla_get_u32(
     tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_VALID_CHANNELS_CONFIG_PARAM_WIFI_BAND]);
    hddLog(LOG1, FL("Wifi band %d"), wifiBand);

    /* Parse and fetch max channels */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_VALID_CHANNELS_CONFIG_PARAM_MAX_CHANNELS]) {
        hddLog(LOGE, FL("attr max channels failed"));
        return -EINVAL;
    }
    maxChannels = nla_get_u32(
     tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_VALID_CHANNELS_CONFIG_PARAM_MAX_CHANNELS]);
    hddLog(LOG1, FL("Max channels %d"), maxChannels);

    status = sme_GetValidChannelsByBand((tHalHandle)(pHddCtx->hHal),
                                        wifiBand, chan_list,
                                        &num_channels);
    if (eHAL_STATUS_SUCCESS != status) {
        hddLog(LOGE,
           FL("sme_GetValidChannelsByBand failed (err=%d)"), status);
        return -EINVAL;
    }

    num_channels = VOS_MIN(num_channels, maxChannels);

    num_chan_new = num_channels;

    /* remove the indoor only channels if iface is SAP */
    if ((WLAN_HDD_SOFTAP == pAdapter->device_mode) ||
        !strncmp(hdd_get_fwpath(), "ap", 2)) {
        num_chan_new = 0;
        for (i = 0; i < num_channels; i++)
            for (j = 0; j < IEEE80211_NUM_BANDS; j++) {
                if (wiphy->bands[j] == NULL)
                    continue;
                for (k = 0; k < wiphy->bands[j]->n_channels; k++) {
                    if ((chan_list[i] ==
                         wiphy->bands[j]->channels[k].center_freq) &&
                        (!(wiphy->bands[j]->channels[k].flags &
                           IEEE80211_CHAN_INDOOR_ONLY))) {
                        chan_list[num_chan_new] = chan_list[i];
                        num_chan_new++;
                    }
                }
            }
    }

    hddLog(LOG1, FL("Number of channels %d"), num_chan_new);
    for (i = 0; i < num_chan_new; i++)
        hddLog(LOG1, "Channel: %u ", chan_list[i]);

    reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32) +
                                                    sizeof(u32) * num_chan_new +
                                                    NLMSG_HDRLEN);

    if (reply_skb) {
        if (nla_put_u32(reply_skb,
                        QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_CHANNELS,
                        num_chan_new) ||
            nla_put(reply_skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CHANNELS,
                    sizeof(u32) * num_chan_new, chan_list)) {
            hddLog(LOGE, FL("nla put fail"));
            kfree_skb(reply_skb);
            return -EINVAL;
        }

        return cfg80211_vendor_cmd_reply(reply_skb);
    }
    hddLog(LOGE, FL("valid channels: buffer alloc fail"));
    return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_extscan_get_valid_channels() - get ext scan valid channels
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_hdd_cfg80211_extscan_get_valid_channels(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_get_valid_channels(wiphy, wdev, data,
			data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * hdd_extscan_update_dwell_time_limits() - update dwell times
 * @req_msg: Pointer to request message
 * @bkt_idx: Index of current bucket being processed
 * @active_min: minimum active dwell time
 * @active_max: maximum active dwell time
 * @passive_min: minimum passive dwell time
 * @passive_max: maximum passive dwell time
 *
 * Return: none
 */
static void hdd_extscan_update_dwell_time_limits(
			tpSirWifiScanCmdReqParams req_msg, uint32_t bkt_idx,
			uint32_t active_min, uint32_t active_max,
			uint32_t passive_min, uint32_t passive_max)
{
	/* update per-bucket dwell times */
	if (req_msg->buckets[bkt_idx].min_dwell_time_active >
			active_min) {
		req_msg->buckets[bkt_idx].min_dwell_time_active =
			active_min;
	}
	if (req_msg->buckets[bkt_idx].max_dwell_time_active <
			active_max) {
		req_msg->buckets[bkt_idx].max_dwell_time_active =
			active_max;
	}
	if (req_msg->buckets[bkt_idx].min_dwell_time_passive >
			passive_min) {
		req_msg->buckets[bkt_idx].min_dwell_time_passive =
			passive_min;
	}
	if (req_msg->buckets[bkt_idx].max_dwell_time_passive <
			passive_max) {
		req_msg->buckets[bkt_idx].max_dwell_time_passive =
			passive_max;
	}
	/* update dwell-time across all buckets */
	if (req_msg->min_dwell_time_active >
			req_msg->buckets[bkt_idx].min_dwell_time_active) {
		req_msg->min_dwell_time_active =
			req_msg->buckets[bkt_idx].min_dwell_time_active;
	}
	if (req_msg->max_dwell_time_active <
			req_msg->buckets[bkt_idx].max_dwell_time_active) {
		req_msg->max_dwell_time_active =
			req_msg->buckets[bkt_idx].max_dwell_time_active;
	}
	if (req_msg->min_dwell_time_passive >
			req_msg->buckets[bkt_idx].min_dwell_time_passive) {
		req_msg->min_dwell_time_passive =
			req_msg->buckets[bkt_idx].min_dwell_time_passive;
	}
	if (req_msg->max_dwell_time_passive >
			req_msg->buckets[bkt_idx].max_dwell_time_passive) {
		req_msg->max_dwell_time_passive =
			req_msg->buckets[bkt_idx].max_dwell_time_passive;
	}
}

/**
 * hdd_extscan_channel_max_reached() - channel max reached
 * @req: extscan request structure
 * @total_channels: total number of channels
 *
 * Return: true if total channels reached max, false otherwise
 */
static bool hdd_extscan_channel_max_reached(tSirWifiScanCmdReqParams *req,
					    uint8_t total_channels)
{
	if (total_channels == WLAN_EXTSCAN_MAX_CHANNELS) {
		hddLog(LOGW,
			FL("max #of channels %d reached, taking only first %d bucket(s)"),
			total_channels, req->numBuckets);
		return true;
	}
	return false;
}

static int hdd_extscan_start_fill_bucket_channel_spec(
			hdd_context_t *pHddCtx,
			tpSirWifiScanCmdReqParams pReqMsg,
			struct nlattr **tb)
{
	struct nlattr *bucket[
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
	struct nlattr *channel[
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
	struct nlattr *buckets;
	struct nlattr *channels;
	int rem1, rem2;
	eHalStatus status;
	uint8_t bktIndex, j, numChannels, total_channels = 0;
	uint32_t chanList[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};

	uint32_t min_dwell_time_active_bucket =
		pHddCtx->cfg_ini->extscan_active_max_chn_time;
	uint32_t max_dwell_time_active_bucket =
		pHddCtx->cfg_ini->extscan_active_max_chn_time;
	uint32_t min_dwell_time_passive_bucket =
		pHddCtx->cfg_ini->extscan_passive_max_chn_time;
	uint32_t max_dwell_time_passive_bucket =
		pHddCtx->cfg_ini->extscan_passive_max_chn_time;

	bktIndex = 0;
	pReqMsg->min_dwell_time_active =
		pReqMsg->max_dwell_time_active =
			pHddCtx->cfg_ini->extscan_active_max_chn_time;

	pReqMsg->min_dwell_time_passive =
		pReqMsg->max_dwell_time_passive =
			pHddCtx->cfg_ini->extscan_passive_max_chn_time;
	pReqMsg->numBuckets = 0;

	nla_for_each_nested(buckets,
			tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC], rem1) {
		if (nla_parse(bucket,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
			nla_data(buckets), nla_len(buckets), NULL)) {
			hddLog(LOGE, FL("nla_parse failed"));
			return -EINVAL;
		}

		/* Parse and fetch bucket spec */
		if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_INDEX]) {
			hddLog(LOGE, FL("attr bucket index failed"));
			return -EINVAL;
		}
		pReqMsg->buckets[bktIndex].bucket = nla_get_u8(
			bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_INDEX]);
		hddLog(LOG1, FL("Bucket spec Index %d"),
				pReqMsg->buckets[bktIndex].bucket);

		/* Parse and fetch wifi band */
		if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_BAND]) {
			hddLog(LOGE, FL("attr wifi band failed"));
			return -EINVAL;
		}
		pReqMsg->buckets[bktIndex].band = nla_get_u8(
			bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_BAND]);
		hddLog(LOG1, FL("Wifi band %d"),
			pReqMsg->buckets[bktIndex].band);

		/* Parse and fetch period */
		if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_PERIOD]) {
			hddLog(LOGE, FL("attr period failed"));
			return -EINVAL;
		}
		pReqMsg->buckets[bktIndex].period = nla_get_u32(
		bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_PERIOD]);
		hddLog(LOG1, FL("period %d"),
			pReqMsg->buckets[bktIndex].period);

		/* Parse and fetch report events */
		if (!bucket[
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_REPORT_EVENTS]) {
			hddLog(LOGE, FL("attr report events failed"));
			return -EINVAL;
		}
		pReqMsg->buckets[bktIndex].reportEvents = nla_get_u8(
			bucket[
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_REPORT_EVENTS]);
		hddLog(LOG1, FL("report events %d"),
				pReqMsg->buckets[bktIndex].reportEvents);

		/* Parse and fetch max period */
		if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_MAX_PERIOD]) {
			hddLog(LOGE, FL("attr max period failed"));
			return -EINVAL;
	        }
		pReqMsg->buckets[bktIndex].max_period = nla_get_u32(
			bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_MAX_PERIOD]);
		hddLog(LOG1, FL("max period %u"),
			pReqMsg->buckets[bktIndex].max_period);

		/* Parse and fetch exponent */
		if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_EXPONENT]) {
			hddLog(LOGE, FL("attr exponent failed"));
			return -EINVAL;
		}
		pReqMsg->buckets[bktIndex].exponent = nla_get_u32(
			bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_EXPONENT]);
		hddLog(LOG1, FL("exponent %u"),
			pReqMsg->buckets[bktIndex].exponent);

		/* Parse and fetch step count */
		if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_STEP_COUNT]) {
			hddLog(LOGE, FL("attr step count failed"));
			return -EINVAL;
		}
		pReqMsg->buckets[bktIndex].step_count = nla_get_u32(
			bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_STEP_COUNT]);
		hddLog(LOG1, FL("Step count %u"),
			pReqMsg->buckets[bktIndex].step_count);

		/* start with known good values for bucket dwell times */
		pReqMsg->buckets[bktIndex].min_dwell_time_active =
		pReqMsg->buckets[bktIndex].max_dwell_time_active =
			pHddCtx->cfg_ini->extscan_active_max_chn_time;

		pReqMsg->buckets[bktIndex].min_dwell_time_passive =
		pReqMsg->buckets[bktIndex].max_dwell_time_passive =
			pHddCtx->cfg_ini->extscan_passive_max_chn_time;

		/* Framework shall pass the channel list if the input WiFi band is
		 * WIFI_BAND_UNSPECIFIED.
		 * If the input WiFi band is specified (any value other than
		 * WIFI_BAND_UNSPECIFIED) then driver populates the channel list
		 */
		if (pReqMsg->buckets[bktIndex].band != WIFI_BAND_UNSPECIFIED) {
			if (hdd_extscan_channel_max_reached(pReqMsg,
							    total_channels))
				return 0;

			numChannels = 0;
			hddLog(LOG1, "WiFi band is specified, driver to fill channel list");
			status = sme_GetValidChannelsByBand(pHddCtx->hHal,
						pReqMsg->buckets[bktIndex].band,
						chanList, &numChannels);
			if (!HAL_STATUS_SUCCESS(status)) {
				hddLog(LOGE,
				       FL("sme_GetValidChannelsByBand failed (err=%d)"),
				       status);
				return -EINVAL;
			}
			hddLog(LOG1, FL("before trimming, num_channels: %d"),
				numChannels);

			pReqMsg->buckets[bktIndex].numChannels =
				VOS_MIN(numChannels,
					(WLAN_EXTSCAN_MAX_CHANNELS - total_channels));
			hddLog(LOG1,
				FL("Adj Num channels/bucket: %d total_channels: %d"),
				pReqMsg->buckets[bktIndex].numChannels,
				total_channels);

			total_channels += pReqMsg->buckets[bktIndex].numChannels;

			for (j = 0; j < pReqMsg->buckets[bktIndex].numChannels;
				j++) {
				pReqMsg->buckets[bktIndex].channels[j].channel =
							chanList[j];
				pReqMsg->buckets[bktIndex].channels[j].
							chnlClass = 0;
				if (CSR_IS_CHANNEL_DFS(
					vos_freq_to_chan(chanList[j]))) {
					pReqMsg->buckets[bktIndex].channels[j].
								passive = 1;
					pReqMsg->buckets[bktIndex].channels[j].
					dwellTimeMs =
						pHddCtx->cfg_ini->
						extscan_passive_max_chn_time;
					/* reconfigure per-bucket dwell time */
					if (min_dwell_time_passive_bucket >
							pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs) {
						min_dwell_time_passive_bucket =
							pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs;
					}
					if (max_dwell_time_passive_bucket <
							pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs) {
						max_dwell_time_passive_bucket =
							pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs;
					}
				} else {
					pReqMsg->buckets[bktIndex].channels[j].
						passive = 0;
					pReqMsg->buckets[bktIndex].channels[j].
						dwellTimeMs =
						pHddCtx->cfg_ini->extscan_active_max_chn_time;
					/* reconfigure per-bucket dwell times */
					if (min_dwell_time_active_bucket >
							pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs) {
						min_dwell_time_active_bucket =
							pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs;
					}
					if (max_dwell_time_active_bucket <
							pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs) {
						max_dwell_time_active_bucket =
							pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs;
					}
				}

				hddLog(LOG1,
					"Channel %u Passive %u Dwell time %u ms Class %u",
					pReqMsg->buckets[bktIndex].channels[j].channel,
					pReqMsg->buckets[bktIndex].channels[j].passive,
					pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs,
					pReqMsg->buckets[bktIndex].channels[j].chnlClass);
			}

			hdd_extscan_update_dwell_time_limits(
					pReqMsg, bktIndex,
					min_dwell_time_active_bucket,
					max_dwell_time_active_bucket,
					min_dwell_time_passive_bucket,
					max_dwell_time_passive_bucket);

			hddLog(LOG1, FL("bktIndex:%d actv_min:%d actv_max:%d pass_min:%d pass_max:%d"),
					bktIndex,
					pReqMsg->buckets[bktIndex].min_dwell_time_active,
					pReqMsg->buckets[bktIndex].max_dwell_time_active,
					pReqMsg->buckets[bktIndex].min_dwell_time_passive,
					pReqMsg->buckets[bktIndex].max_dwell_time_passive);

			bktIndex++;
			pReqMsg->numBuckets++;
			continue;
		}

		/* Parse and fetch number of channels */
		if (!bucket[
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS]) {
			hddLog(LOGE, FL("attr num channels failed"));
			return -EINVAL;
		}

		pReqMsg->buckets[bktIndex].numChannels =
		nla_get_u32(bucket[
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS]);
		hddLog(LOG1, FL("before trimming: num channels %d"),
				pReqMsg->buckets[bktIndex].numChannels);
		pReqMsg->buckets[bktIndex].numChannels =
			VOS_MIN(pReqMsg->buckets[bktIndex].numChannels,
				(WLAN_EXTSCAN_MAX_CHANNELS - total_channels));
		hddLog(LOG1,
			FL("Num channels/bucket: %d total_channels: %d"),
			pReqMsg->buckets[bktIndex].numChannels,
			total_channels);
		if (hdd_extscan_channel_max_reached(pReqMsg, total_channels))
			return 0;

		if (!bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC]) {
			hddLog(LOGE, FL("attr channel spec failed"));
			return -EINVAL;
		}

		j = 0;
		nla_for_each_nested(channels,
			bucket[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC], rem2) {
			if ((j >= pReqMsg->buckets[bktIndex].numChannels) ||
			    hdd_extscan_channel_max_reached(pReqMsg,
							    total_channels))
				break;

			if (nla_parse(channel,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
				nla_data(channels), nla_len(channels),
				wlan_hdd_extscan_config_policy)) {
				hddLog(LOGE, FL("nla_parse failed"));
				return -EINVAL;
			}

			/* Parse and fetch channel */
			if (!channel[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CHANNEL]) {
				hddLog(LOGE, FL("attr channel failed"));
				return -EINVAL;
			}
			pReqMsg->buckets[bktIndex].channels[j].channel =
				nla_get_u32(channel[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CHANNEL]);
			hddLog(LOG1, FL("channel %u"),
				pReqMsg->buckets[bktIndex].channels[j].channel);

			/* Parse and fetch dwell time */
			if (!channel[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_DWELL_TIME]) {
				hddLog(LOGE, FL("attr dwelltime failed"));
				return -EINVAL;
			}
			pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs =
				nla_get_u32(channel[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_DWELL_TIME]);

			/* Override dwell time if required */
			if (pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs <
				pHddCtx->cfg_ini->extscan_active_min_chn_time ||
				pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs >
				pHddCtx->cfg_ini->extscan_active_max_chn_time) {
				hddLog(LOG1,
					FL("WiFi band is unspecified, dwellTime:%d"),
					pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs);

				if (CSR_IS_CHANNEL_DFS(
					vos_freq_to_chan(
						pReqMsg->buckets[bktIndex].channels[j].channel))) {
					pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs =
						pHddCtx->cfg_ini->extscan_passive_max_chn_time;
				} else {
					pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs =
						pHddCtx->cfg_ini->extscan_active_max_chn_time;
				}
			}

			hddLog(LOG1, FL("New Dwell time (%u ms)"),
				pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs);

			if (CSR_IS_CHANNEL_DFS(
						vos_freq_to_chan(
						pReqMsg->buckets[bktIndex].channels[j].channel))) {
				if(min_dwell_time_passive_bucket >
						pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs) {
					min_dwell_time_passive_bucket =
						pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs;
				}
				if(max_dwell_time_passive_bucket <
						pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs) {
					max_dwell_time_passive_bucket =
						pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs;
				}
			} else {
				if(min_dwell_time_active_bucket >
						pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs) {
					min_dwell_time_active_bucket =
						pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs;
				}
				if(max_dwell_time_active_bucket <
						pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs) {
					max_dwell_time_active_bucket =
						pReqMsg->buckets[bktIndex].channels[j].dwellTimeMs;
				}
			}

			/* Parse and fetch channel spec passive */
			if (!channel[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_PASSIVE]) {
				hddLog(LOGE,
					FL("attr channel spec passive failed"));
				return -EINVAL;
			}
			pReqMsg->buckets[bktIndex].channels[j].passive =
				nla_get_u8(channel[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_PASSIVE]);
			hddLog(LOG1, FL("Chnl spec passive %u"),
				pReqMsg->buckets[bktIndex].channels[j].passive);

			/* Override scan type if required */
			if (CSR_IS_CHANNEL_DFS(
				vos_freq_to_chan(
					pReqMsg->buckets[bktIndex].channels[j].channel))) {
				pReqMsg->buckets[bktIndex].channels[j].passive = TRUE;
			} else {
				pReqMsg->buckets[bktIndex].channels[j].passive = FALSE;
			}

			j++;
			total_channels++;
		}

		hdd_extscan_update_dwell_time_limits(
					pReqMsg, bktIndex,
					min_dwell_time_active_bucket,
					max_dwell_time_active_bucket,
					min_dwell_time_passive_bucket,
					max_dwell_time_passive_bucket);

		hddLog(LOG1, FL("bktIndex:%d actv_min:%d actv_max:%d pass_min:%d pass_max:%d"),
				bktIndex,
				pReqMsg->buckets[bktIndex].min_dwell_time_active,
				pReqMsg->buckets[bktIndex].max_dwell_time_active,
				pReqMsg->buckets[bktIndex].min_dwell_time_passive,
				pReqMsg->buckets[bktIndex].max_dwell_time_passive);

		bktIndex++;
		pReqMsg->numBuckets++;
	}

	hddLog(LOG1, FL("Global: actv_min:%d actv_max:%d pass_min:%d pass_max:%d"),
				pReqMsg->min_dwell_time_active,
				pReqMsg->max_dwell_time_active,
				pReqMsg->min_dwell_time_passive,
				pReqMsg->max_dwell_time_passive);

	return 0;
}

/*
 * hdd_extscan_map_usr_drv_config_flags() - map userspace to driver config flags
 * @config_flags - [input] configuration flags.
 *
 * This function maps user space received configuration flags to
 * driver representation.
 *
 * Return: configuration flags
 */
static uint32_t hdd_extscan_map_usr_drv_config_flags(uint32_t config_flags)
{
	uint32_t configuration_flags = 0;

	if (config_flags & EXTSCAN_LP_EXTENDED_BATCHING)
		configuration_flags |= EXTSCAN_LP_EXTENDED_BATCHING;

	return configuration_flags;
}

/*
 * define short names for the global vendor params
 * used by wlan_hdd_cfg80211_extscan_start()
 */
#define PARAM_MAX \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX
#define PARAM_REQUEST_ID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID
#define PARAM_BASE_PERIOD \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_BASE_PERIOD
#define PARAM_MAX_AP_PER_SCAN \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_MAX_AP_PER_SCAN
#define PARAM_RPT_THRHLD_PERCENT \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD_PERCENT
#define PARAM_RPT_THRHLD_NUM_SCANS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD_NUM_SCANS
#define PARAM_NUM_BUCKETS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_NUM_BUCKETS
#define PARAM_CONFIG_FLAGS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_CONFIGURATION_FLAGS

/**
 * __wlan_hdd_cfg80211_extscan_start() - start extscan
 * @wiphy: Pointer to wireless phy.
 * @wdev: Pointer to wireless device.
 * @data: Pointer to input data.
 * @data_len: Length of @data.
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_extscan_start(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	tpSirWifiScanCmdReqParams pReqMsg = NULL;
	struct net_device *dev            = wdev->netdev;
	hdd_adapter_t *pAdapter           = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx            = wiphy_priv(wiphy);
	struct nlattr *tb[PARAM_MAX + 1];
	struct hdd_ext_scan_context *context;
	uint32_t request_id, num_buckets;
	eHalStatus status;
	int retval;
	unsigned long rc;

	ENTER();

	retval = wlan_hdd_validate_context(pHddCtx);
	if (0 != retval) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	if (nla_parse(tb, PARAM_MAX, data, data_len,
		wlan_hdd_extscan_config_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	pReqMsg = vos_mem_malloc(sizeof(*pReqMsg));
	if (!pReqMsg) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}

	/* Parse and fetch request Id */
	if (!tb[PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		goto fail;
	}

	pReqMsg->requestId = nla_get_u32(tb[PARAM_REQUEST_ID]);
	pReqMsg->sessionId = pAdapter->sessionId;
	hddLog(LOG1, FL("Req Id %d Session Id %d"),
		pReqMsg->requestId, pReqMsg->sessionId);

	/* Parse and fetch base period */
	if (!tb[PARAM_BASE_PERIOD]) {
		hddLog(LOGE, FL("attr base period failed"));
		goto fail;
	}
	pReqMsg->basePeriod = nla_get_u32(tb[PARAM_BASE_PERIOD]);
	hddLog(LOG1, FL("Base Period %d"), pReqMsg->basePeriod);

	/* Parse and fetch max AP per scan */
	if (!tb[PARAM_MAX_AP_PER_SCAN]) {
		hddLog(LOGE, FL("attr max_ap_per_scan failed"));
		goto fail;
	}
	pReqMsg->maxAPperScan = nla_get_u32(tb[PARAM_MAX_AP_PER_SCAN]);
	hddLog(LOG1, FL("Max AP per Scan %d"), pReqMsg->maxAPperScan);

	/* Parse and fetch report threshold percent */
	if (!tb[PARAM_RPT_THRHLD_PERCENT]) {
		hddLog(LOGE, FL("attr report_threshold percent failed"));
		goto fail;
	}
	pReqMsg->report_threshold_percent = nla_get_u8(
		tb[PARAM_RPT_THRHLD_PERCENT]);
	hddLog(LOG1, FL("Report Threshold percent %d"),
		pReqMsg->report_threshold_percent);

	/* Parse and fetch report threshold num scans */
	if (!tb[PARAM_RPT_THRHLD_NUM_SCANS]) {
		hddLog(LOGE, FL("attr report_threshold num scans failed"));
		goto fail;
	}
	pReqMsg->report_threshold_num_scans = nla_get_u8(
		tb[PARAM_RPT_THRHLD_NUM_SCANS]);
	hddLog(LOG1, FL("Report Threshold num scans %d"),
		pReqMsg->report_threshold_num_scans);

	/* Parse and fetch number of buckets */
	if (!tb[PARAM_NUM_BUCKETS]) {
		hddLog(LOGE, FL("attr number of buckets failed"));
		goto fail;
	}
	num_buckets = nla_get_u8(tb[PARAM_NUM_BUCKETS]);
	if (num_buckets > WLAN_EXTSCAN_MAX_BUCKETS) {
		hddLog(LOGW,
			FL("Exceeded MAX number of buckets: %d"),
			WLAN_EXTSCAN_MAX_BUCKETS);
	}
	hddLog(LOG1, FL("Input: Number of Buckets %d"), num_buckets);

	/* This is optional attribute, if not present set it to 0 */
	if (!tb[PARAM_CONFIG_FLAGS])
		pReqMsg->configuration_flags = 0;
	else
		pReqMsg->configuration_flags =
			hdd_extscan_map_usr_drv_config_flags(
				nla_get_u32(tb[PARAM_CONFIG_FLAGS]));

	hddLog(LOG1, FL("Configuration flags: %u"),
				pReqMsg->configuration_flags);

	if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC]) {
		hddLog(LOGE, FL("attr bucket spec failed"));
		goto fail;
	}

	if (hdd_extscan_start_fill_bucket_channel_spec(pHddCtx, pReqMsg, tb))
		goto fail;

	context = &pHddCtx->ext_scan_context;
	spin_lock(&hdd_context_lock);
	INIT_COMPLETION(context->response_event);
	context->request_id = request_id = pReqMsg->requestId;
	spin_unlock(&hdd_context_lock);

	status = sme_ExtScanStart(pHddCtx->hHal, pReqMsg);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE,
			FL("sme_ExtScanStart failed(err=%d)"), status);
		goto fail;
	}

	pHddCtx->ext_scan_start_since_boot = vos_get_monotonic_boottime();
	hddLog(LOG1, FL("Timestamp since boot: %llu"),
			pHddCtx->ext_scan_start_since_boot);

	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout(&context->response_event,
				msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));

	if (!rc) {
		hddLog(LOGE, FL("sme_ExtScanStart timed out"));
		retval = -ETIMEDOUT;
	} else {
		spin_lock(&hdd_context_lock);
		if (context->request_id == request_id)
			retval = context->response_status;
		else
			retval = -EINVAL;
		spin_unlock(&hdd_context_lock);
	}

	return retval;

fail:
	vos_mem_free(pReqMsg);
	return -EINVAL;
}
/*
 * done with short names for the global vendor params
 * used by wlan_hdd_cfg80211_extscan_start()
 */
#undef PARAM_MAX
#undef PARAM_REQUEST_ID
#undef PARAM_BASE_PERIOD
#undef PARAMS_MAX_AP_PER_SCAN
#undef PARAMS_RPT_THRHLD_PERCENT
#undef PARAMS_RPT_THRHLD_NUM_SCANS
#undef PARAMS_NUM_BUCKETS
#undef PARAM_CONFIG_FLAGS

/**
 * wlan_hdd_cfg80211_extscan_start() - start extscan
 * @wiphy: Pointer to wireless phy.
 * @wdev: Pointer to wireless device.
 * @data: Pointer to input data.
 * @data_len: Length of @data.
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_hdd_cfg80211_extscan_start(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_start(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/*
 * define short names for the global vendor params
 * used by wlan_hdd_cfg80211_extscan_stop()
 */
#define PARAM_MAX \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX
#define PARAM_REQUEST_ID \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID

/**
 * wlan_hdd_cfg80211_extscan_stop() - stop extscan
 * @wiphy: Pointer to wireless phy.
 * @wdev: Pointer to wireless device.
 * @data: Pointer to input data.
 * @data_len: Length of @data.
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_extscan_stop(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data,
					  int data_len)
{
	tpSirExtScanStopReqParams pReqMsg = NULL;
	struct net_device *dev            = wdev->netdev;
	hdd_adapter_t *pAdapter           = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx            = wiphy_priv(wiphy);
	struct hdd_ext_scan_context *context;
	struct nlattr *tb[PARAM_MAX + 1];
	uint32_t request_id;
	eHalStatus status;
	int retval;
	unsigned long rc;

	ENTER();

	retval = wlan_hdd_validate_context(pHddCtx);
	if (0 != retval) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	if (nla_parse(tb, PARAM_MAX, data, data_len,
			wlan_hdd_extscan_config_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	pReqMsg = vos_mem_malloc(sizeof(*pReqMsg));
	if (!pReqMsg) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}

	/* Parse and fetch request Id */
	if (!tb[PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		goto fail;
	}

	pReqMsg->requestId = nla_get_u32(tb[PARAM_REQUEST_ID]);
	pReqMsg->sessionId = pAdapter->sessionId;
	hddLog(LOG1, FL("Req Id %d Session Id %d"),
		pReqMsg->requestId, pReqMsg->sessionId);

	context = &pHddCtx->ext_scan_context;
	spin_lock(&hdd_context_lock);
	INIT_COMPLETION(context->response_event);
	context->request_id = request_id = pReqMsg->requestId;
	spin_unlock(&hdd_context_lock);

	status = sme_ExtScanStop(pHddCtx->hHal, pReqMsg);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE,
			FL("sme_ExtScanStop failed(err=%d)"), status);
		goto fail;
	}

	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout(&context->response_event,
				msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));

	if (!rc) {
		hddLog(LOGE, FL("sme_ExtScanStop timed out"));
		retval = -ETIMEDOUT;
	} else {
		spin_lock(&hdd_context_lock);
		if (context->request_id == request_id)
			retval = context->response_status;
		else
			retval = -EINVAL;
		spin_unlock(&hdd_context_lock);
	}

	return retval;

fail:
    vos_mem_free(pReqMsg);
    return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_extscan_stop() - stop extscan
 * @wiphy: Pointer to wireless phy.
 * @wdev: Pointer to wireless device.
 * @data: Pointer to input data.
 * @data_len: Length of @data.
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_hdd_cfg80211_extscan_stop(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_stop(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/*
 * done with short names for the global vendor params
 * used by wlan_hdd_cfg80211_extscan_stop()
 */
#undef PARAM_MAX
#undef PARAM_REQUEST_ID

static int __wlan_hdd_cfg80211_extscan_reset_bssid_hotlist(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        const void *data,
                                        int data_len)
{
    tpSirExtScanResetBssidHotlistReqParams pReqMsg = NULL;
    struct net_device *dev                       = wdev->netdev;
    hdd_adapter_t *pAdapter                      = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                       = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct hdd_ext_scan_context *context;
    uint32_t request_id;
    eHalStatus status;
    int retval;
    unsigned long rc;

    ENTER();

    retval = wlan_hdd_validate_context(pHddCtx);
    if (0 != retval) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, data_len,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(LOGE, FL("Invalid ATTR"));
        return -EINVAL;
    }

    pReqMsg = vos_mem_malloc(sizeof(*pReqMsg));
    if (!pReqMsg) {
        hddLog(LOGE, FL("vos_mem_malloc failed"));
        return -ENOMEM;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(LOGE, FL("attr request id failed"));
        goto fail;
    }

    pReqMsg->requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);

    pReqMsg->sessionId = pAdapter->sessionId;
    hddLog(LOG1, FL("Req Id %d Session Id %d"),
                 pReqMsg->requestId, pReqMsg->sessionId);

    context = &pHddCtx->ext_scan_context;
    spin_lock(&hdd_context_lock);
    INIT_COMPLETION(context->response_event);
    context->request_id = request_id = pReqMsg->requestId;
    spin_unlock(&hdd_context_lock);

    status = sme_ResetBssHotlist(pHddCtx->hHal, pReqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(LOGE, FL("sme_ResetBssHotlist failed(err=%d)"), status);
        goto fail;
    }

    /* request was sent -- wait for the response */
    rc = wait_for_completion_timeout(&context->response_event,
                                     msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));
    if (!rc) {
       hddLog(LOGE, FL("sme_ResetBssHotlist timed out"));
       retval = -ETIMEDOUT;
    } else {
       spin_lock(&hdd_context_lock);
       if (context->request_id == request_id)
          retval = context->response_status;
       else
          retval = -EINVAL;
       spin_unlock(&hdd_context_lock);
    }

    return retval;

fail:
    vos_mem_free(pReqMsg);
    return -EINVAL;
}

/*
 * define short names for the global vendor params
 * used by wlan_hdd_cfg80211_extscan_reset_ssid_hotlist()
 */
#define PARAM_MAX \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX
#define PARAM_REQUEST_ID \
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID

/**
 * wlan_hdd_cfg80211_extscan_reset_bssid_hotlist() - reset bssid hot list
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int wlan_hdd_cfg80211_extscan_reset_bssid_hotlist(struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_reset_bssid_hotlist(wiphy, wdev,
								data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_extscan_reset_ssid_hotlist() - reset ssid hot list
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_extscan_reset_ssid_hotlist(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	struct sir_set_ssid_hotlist_request *request;
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[PARAM_MAX + 1];
	struct hdd_ext_scan_context *context;
	uint32_t request_id;
	eHalStatus status;
	int retval;
	unsigned long rc;

	ENTER();

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (0 != retval) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	if (nla_parse(tb, PARAM_MAX,
		      data, data_len,
		      wlan_hdd_extscan_config_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	request = vos_mem_malloc(sizeof(*request));
	if (!request) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}

	/* Parse and fetch request Id */
	if (!tb[PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		goto fail;
	}

	request->request_id = nla_get_u32(tb[PARAM_REQUEST_ID]);
	request->session_id = adapter->sessionId;
	hddLog(LOG1, FL("Request Id %d Session Id %d"), request->request_id,
		request->session_id);

	request->lost_ssid_sample_size = 0;
	request->ssid_count = 0;

	context = &hdd_ctx->ext_scan_context;
	spin_lock(&hdd_context_lock);
	INIT_COMPLETION(context->response_event);
	context->request_id = request_id = request->request_id;
	spin_unlock(&hdd_context_lock);

	status = sme_set_ssid_hotlist(hdd_ctx->hHal, request);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE,
		       FL("sme_reset_ssid_hotlist failed(err=%d)"), status);
		goto fail;
	}

	vos_mem_free(request);

	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout(&context->response_event,
					 msecs_to_jiffies
						(WLAN_WAIT_TIME_EXTSCAN));
	if (!rc) {
		hddLog(LOGE, FL("sme_reset_ssid_hotlist timed out"));
		retval = -ETIMEDOUT;
	} else {
		spin_lock(&hdd_context_lock);
		if (context->request_id == request_id)
			retval = context->response_status;
		else
			retval = -EINVAL;
		spin_unlock(&hdd_context_lock);
	}

	return retval;

fail:
	vos_mem_free(request);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_extscan_reset_ssid_hotlist() - reset ssid hot list
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
wlan_hdd_cfg80211_extscan_reset_ssid_hotlist(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_reset_ssid_hotlist(wiphy, wdev,
							data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}
/*
 * done with short names for the global vendor params
 * used by wlan_hdd_cfg80211_extscan_reset_ssid_hotlist()
 */
#undef PARAM_MAX
#undef PARAM_REQUEST_ID

static int __wlan_hdd_cfg80211_extscan_reset_significant_change(
                                        struct wiphy *wiphy,
                                        struct wireless_dev *wdev,
                                        const void *data,
                                        int data_len)
{
    tpSirExtScanResetSignificantChangeReqParams pReqMsg = NULL;
    struct net_device *dev                            = wdev->netdev;
    hdd_adapter_t *pAdapter                           = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                            = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
    struct hdd_ext_scan_context *context;
    uint32_t request_id;
    eHalStatus status;
    int retval;
    unsigned long rc;

    ENTER();

    retval = wlan_hdd_validate_context(pHddCtx);
    if (0 != retval) {
         hddLog(LOGE, FL("HDD context is not valid"));
         return -EINVAL;
    }


    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
                    data, data_len,
                    wlan_hdd_extscan_config_policy)) {
        hddLog(LOGE, FL("Invalid ATTR"));
        return -EINVAL;
    }

    pReqMsg = vos_mem_malloc(sizeof(*pReqMsg));
    if (!pReqMsg) {
        hddLog(LOGE, FL("vos_mem_malloc failed"));
        return -ENOMEM;
    }

    /* Parse and fetch request Id */
    if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
        hddLog(LOGE, FL("attr request id failed"));
        goto fail;
    }

    pReqMsg->requestId = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);

    pReqMsg->sessionId = pAdapter->sessionId;
    hddLog(LOG1, FL("Req Id %d Session Id %d"),
           pReqMsg->requestId, pReqMsg->sessionId);

    context = &pHddCtx->ext_scan_context;
    spin_lock(&hdd_context_lock);
    INIT_COMPLETION(context->response_event);
    context->request_id = request_id = pReqMsg->requestId;
    spin_unlock(&hdd_context_lock);

    status = sme_ResetSignificantChange(pHddCtx->hHal, pReqMsg);
    if (!HAL_STATUS_SUCCESS(status)) {
        hddLog(LOGE, FL("sme_ResetSignificantChange failed(err=%d)"), status);
        goto fail;
    }

    /* request was sent -- wait for the response */
    rc = wait_for_completion_timeout(&context->response_event,
                                     msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));

    if (!rc) {
       hddLog(LOGE, FL("sme_ResetSignificantChange timed out"));
       retval = -ETIMEDOUT;
    } else {
       spin_lock(&hdd_context_lock);
       if (context->request_id == request_id)
          retval = context->response_status;
       else
          retval = -EINVAL;
       spin_unlock(&hdd_context_lock);
    }

    return retval;

fail:
    vos_mem_free(pReqMsg);
    return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_extscan_reset_significant_change() - reset significant
 *							change
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static
int wlan_hdd_cfg80211_extscan_reset_significant_change(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_extscan_reset_significant_change(wiphy, wdev,
						data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}


/**
 * hdd_extscan_epno_fill_network_list() - epno fill network list
 * @hddctx: HDD context
 * @req_msg: request message
 * @tb: vendor attribute table
 *
 * This function reads the network block NL vendor attributes from %tb and
 * fill in the epno request message.
 *
 * Return: 0 on success, error number otherwise
 */
static int hdd_extscan_epno_fill_network_list(
			hdd_context_t *hddctx,
			struct wifi_epno_params *req_msg,
			struct nlattr **tb)
{
	struct nlattr *network[
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX + 1];
	struct nlattr *networks;
	int rem1, ssid_len;
	uint8_t index, *ssid;

	index = 0;
	nla_for_each_nested(networks,
		tb[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORKS_LIST],
		rem1) {
		if (nla_parse(network,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_MAX,
			nla_data(networks), nla_len(networks), NULL)) {
			hddLog(LOGE, FL("nla_parse failed"));
			return -EINVAL;
		}

		/* Parse and fetch ssid */
		if (!network[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_SSID]) {
			hddLog(LOGE, FL("attr network ssid failed"));
			return -EINVAL;
		}
		ssid_len = nla_len(
			network[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_SSID]);

		/* Decrement by 1, don't count null character */
		ssid_len--;

		req_msg->networks[index].ssid.length = ssid_len;
		hddLog(LOG1, FL("network ssid length %d"), ssid_len);
		ssid = nla_data(network[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_SSID]);
		vos_mem_copy(req_msg->networks[index].ssid.ssId, ssid, ssid_len);
		hddLog(LOG1, FL("Ssid: %.*s"),
			req_msg->networks[index].ssid.length,
			req_msg->networks[index].ssid.ssId);

		/* Parse and fetch rssi threshold */
		if (!network[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_RSSI_THRESHOLD]) {
			hddLog(LOGE, FL("attr rssi threshold failed"));
			return -EINVAL;
		}
		req_msg->networks[index].rssi_threshold = nla_get_s8(
			network[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_RSSI_THRESHOLD]);
		hddLog(LOG1, FL("rssi threshold %d"),
			req_msg->networks[index].rssi_threshold);

		/* Parse and fetch epno flags */
		if (!network[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_FLAGS]) {
			hddLog(LOGE, FL("attr epno flags failed"));
			return -EINVAL;
		}
		req_msg->networks[index].flags = nla_get_u8(
			network[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_FLAGS]);
		hddLog(LOG1, FL("flags %u"), req_msg->networks[index].flags);

		/* Parse and fetch auth bit */
		if (!network[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_AUTH_BIT]) {
			hddLog(LOGE, FL("attr auth bit failed"));
			return -EINVAL;
		}
		req_msg->networks[index].auth_bit_field = nla_get_u8(
			network[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_AUTH_BIT]);
		hddLog(LOG1, FL("auth bit %u"),
			req_msg->networks[index].auth_bit_field);

		index++;
	}
	return 0;
}

/**
 * wlan_hdd_cfg80211_set_epno_list() - epno set network list
 * @wiphy: wiphy
 * @wdev: pointer to wireless dev
 * @data: data pointer
 * @data_len: data length
 *
 * This function reads the NL vendor attributes from %tb and
 * fill in the epno request message.
 *
 * Return: 0 on success, error number otherwise
 */
static int wlan_hdd_cfg80211_set_epno_list(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	struct wifi_epno_params *req_msg = NULL;
	struct net_device *dev           = wdev->netdev;
	hdd_adapter_t *adapter           = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx           = wiphy_priv(wiphy);
	struct nlattr *tb[
		QCA_WLAN_VENDOR_ATTR_PNO_MAX + 1];
	eHalStatus status;
	uint32_t num_networks, len;

	ENTER();

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PNO_MAX,
		data, data_len,
		wlan_hdd_extscan_config_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	/* Parse and fetch number of networks */
	if (!tb[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_NUM_NETWORKS]) {
		hddLog(LOGE, FL("attr num networks failed"));
		return -EINVAL;
	}
	num_networks = nla_get_u32(
              tb[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_NUM_NETWORKS]);
	hddLog(LOG1, FL("num networks %u"), num_networks);

	len = sizeof(*req_msg) +
		(num_networks * sizeof(struct wifi_epno_network));
	req_msg = vos_mem_malloc(len);
	if (!req_msg) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}
	vos_mem_zero(req_msg, len);
	req_msg->num_networks = num_networks;

	/* Parse and fetch request Id */
	if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		goto fail;
	}
	req_msg->request_id = nla_get_u32(
	    tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);

	req_msg->session_id = adapter->sessionId;
	hddLog(LOG1, FL("Req Id %u Session Id %d"),
		req_msg->request_id, req_msg->session_id);

	if (hdd_extscan_epno_fill_network_list(hdd_ctx, req_msg, tb))
		goto fail;

	status = sme_set_epno_list(hdd_ctx->hHal, req_msg);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE, FL("sme_set_epno_list failed(err=%d)"), status);
		goto fail;
	}

	EXIT();
	vos_mem_free(req_msg);
	return 0;

fail:
	vos_mem_free(req_msg);
	return -EINVAL;
}

/**
 * hdd_extscan_passpoint_fill_network_list() - passpoint fill network list
 * @hddctx: HDD context
 * @req_msg: request message
 * @tb: vendor attribute table
 *
 * This function reads the network block NL vendor attributes from %tb and
 * fill in the passpoint request message.
 *
 * Return: 0 on success, error number otherwise
 */
static int hdd_extscan_passpoint_fill_network_list(
			hdd_context_t *hddctx,
			struct wifi_passpoint_req *req_msg,
			struct nlattr **tb)
{
	struct nlattr *network[QCA_WLAN_VENDOR_ATTR_PNO_MAX + 1];
	struct nlattr *networks;
	int rem1, len;
	uint8_t index;

	index = 0;
	nla_for_each_nested(networks,
		tb[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_LIST_PARAM_NETWORK_ARRAY],
		rem1) {
		if (nla_parse(network,
			QCA_WLAN_VENDOR_ATTR_PNO_MAX,
			nla_data(networks), nla_len(networks), NULL)) {
			hddLog(LOGE, FL("nla_parse failed"));
			return -EINVAL;
		}

		/* Parse and fetch identifier */
		if (!network[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ID]) {
			hddLog(LOGE, FL("attr passpoint id failed"));
			return -EINVAL;
		}
		req_msg->networks[index].id = nla_get_u32(
			network[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ID]);
		hddLog(LOG1, FL("Id %u"), req_msg->networks[index].id);

		/* Parse and fetch realm */
		if (!network[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_REALM]) {
			hddLog(LOGE, FL("attr realm failed"));
			return -EINVAL;
		}
		len = nla_len(
			network[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_REALM]);
		if (len < 0 || len > SIR_PASSPOINT_REALM_LEN) {
			hddLog(LOGE, FL("Invalid realm size %d"), len);
			return -EINVAL;
		}
		vos_mem_copy(req_msg->networks[index].realm,
				nla_data(network[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_REALM]),
				len);
		hddLog(LOG1, FL("realm len %d"), len);
		hddLog(LOG1, FL("realm: %s"), req_msg->networks[index].realm);

		/* Parse and fetch roaming consortium ids */
		if (!network[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ROAM_CNSRTM_ID]) {
			hddLog(LOGE, FL("attr roaming consortium ids failed"));
			return -EINVAL;
		}
		nla_memcpy(&req_msg->networks[index].roaming_consortium_ids,
			network[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ROAM_CNSRTM_ID],
			sizeof(req_msg->networks[0].roaming_consortium_ids));
		hddLog(LOG1, FL("roaming consortium ids"));
		VOS_TRACE_HEX_DUMP(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
				req_msg->networks[index].roaming_consortium_ids,
				sizeof(req_msg->networks[0].roaming_consortium_ids));

		/* Parse and fetch plmn */
		if (!network[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ROAM_PLMN]) {
			hddLog(LOGE, FL("attr plmn failed"));
			return -EINVAL;
		}
		nla_memcpy(&req_msg->networks[index].plmn,
			network[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ROAM_PLMN],
			SIR_PASSPOINT_PLMN_LEN);
		hddLog(LOG1, FL("plmn %02x:%02x:%02x"),
			req_msg->networks[index].plmn[0],
			req_msg->networks[index].plmn[1],
			req_msg->networks[index].plmn[2]);

		index++;
	}
	return 0;
}

/**
 * wlan_hdd_cfg80211_set_passpoint_list() - set passpoint network list
 * @wiphy: wiphy
 * @wdev: pointer to wireless dev
 * @data: data pointer
 * @data_len: data length
 *
 * This function reads the NL vendor attributes from %tb and
 * fill in the passpoint request message.
 *
 * Return: 0 on success, error number otherwise
 */
static int wlan_hdd_cfg80211_set_passpoint_list(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct wifi_passpoint_req *req_msg = NULL;
	struct net_device *dev             = wdev->netdev;
	hdd_adapter_t *adapter             = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx             = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PNO_MAX + 1];
	eHalStatus status;
	uint32_t num_networks = 0;

	ENTER();

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PNO_MAX, data, data_len,
		wlan_hdd_extscan_config_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	/* Parse and fetch number of networks */
	if (!tb[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_LIST_PARAM_NUM]) {
		hddLog(LOGE, FL("attr num networks failed"));
		return -EINVAL;
	}
	num_networks = nla_get_u32(
		tb[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_LIST_PARAM_NUM]);
	hddLog(LOG1, FL("num networks %u"), num_networks);

	req_msg = vos_mem_malloc(sizeof(*req_msg) +
			(num_networks * sizeof(req_msg->networks[0])));
	if (!req_msg) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}
	req_msg->num_networks = num_networks;

	/* Parse and fetch request Id */
	if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		goto fail;
	}
	req_msg->request_id = nla_get_u32(
	    tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);

	req_msg->session_id = adapter->sessionId;
	hddLog(LOG1, FL("Req Id %u Session Id %d"), req_msg->request_id,
			req_msg->session_id);

	if (hdd_extscan_passpoint_fill_network_list(hdd_ctx, req_msg, tb))
		goto fail;

	status = sme_set_passpoint_list(hdd_ctx->hHal, req_msg);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE,
			FL("sme_set_passpoint_list failed(err=%d)"), status);
		goto fail;
	}

	EXIT();
	vos_mem_free(req_msg);
	return 0;

fail:
	vos_mem_free(req_msg);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_reset_passpoint_list() - reset passpoint network list
 * @wiphy: wiphy
 * @wdev: pointer to wireless dev
 * @data: data pointer
 * @data_len: data length
 *
 * This function resets passpoint networks list
 *
 * Return: 0 on success, error number otherwise
 */
static int wlan_hdd_cfg80211_reset_passpoint_list(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct wifi_passpoint_req *req_msg = NULL;
	struct net_device *dev             = wdev->netdev;
	hdd_adapter_t *adapter             = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx             = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PNO_MAX + 1];
	eHalStatus status;

	ENTER();

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PNO_MAX, data, data_len,
		wlan_hdd_extscan_config_policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	req_msg = vos_mem_malloc(sizeof(*req_msg));
	if (!req_msg) {
		hddLog(LOGE, FL("vos_mem_malloc failed"));
		return -ENOMEM;
	}

	/* Parse and fetch request Id */
	if (!tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		goto fail;
	}
	req_msg->request_id = nla_get_u32(
	    tb[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID]);

	req_msg->session_id = adapter->sessionId;
	hddLog(LOG1, FL("Req Id %u Session Id %d"),
			req_msg->request_id, req_msg->session_id);

	status = sme_reset_passpoint_list(hdd_ctx->hHal, req_msg);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE,
			FL("sme_reset_passpoint_list failed(err=%d)"), status);
		goto fail;
	}

	EXIT();
	vos_mem_free(req_msg);
	return 0;

fail:
	vos_mem_free(req_msg);
	return -EINVAL;
}

#endif /* FEATURE_WLAN_EXTSCAN */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

static bool put_wifi_rate_stat( tpSirWifiRateStat stats,
                                struct sk_buff *vendor_event)
{
    if (nla_put_u8(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_PREAMBLE,
                   stats->rate.preamble)  ||
        nla_put_u8(vendor_event,
                   QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_NSS,
                   stats->rate.nss)       ||
        nla_put_u8(vendor_event,
                   QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BW,
                   stats->rate.bw)        ||
        nla_put_u8(vendor_event,
                   QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MCS_INDEX,
                   stats->rate.rateMcsIdx) ||
        nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BIT_RATE,
                    stats->rate.bitrate )   ||
        nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_TX_MPDU,
                    stats->txMpdu )    ||
        nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RX_MPDU,
                    stats->rxMpdu )     ||
        nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MPDU_LOST,
                    stats->mpduLost )  ||
        nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES,
                    stats->retries)     ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_SHORT,
                    stats->retriesShort )   ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_LONG,
                    stats->retriesLong))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("QCA_WLAN_VENDOR_ATTR put fail"));
        return FALSE;
    }

    return TRUE;
}

static bool put_wifi_peer_info( tpSirWifiPeerInfo stats,
                               struct sk_buff *vendor_event)
{
    u32 i = 0;
    tpSirWifiRateStat pRateStats;

    if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_TYPE,
                    stats->type) ||
        nla_put(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_MAC_ADDRESS,
                VOS_MAC_ADDR_SIZE, &stats->peerMacAddress[0]) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_CAPABILITIES,
                    stats->capabilities) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES,
                    stats->numRate))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("QCA_WLAN_VENDOR_ATTR put fail"));
        goto error;
    }

    if (stats->numRate)
    {
        struct nlattr *rateInfo;
        struct nlattr *rates;

        rateInfo = nla_nest_start(vendor_event,
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_RATE_INFO);
        if (rateInfo == NULL)
            goto error;

        for (i = 0; i < stats->numRate; i++)
        {
            pRateStats = (tpSirWifiRateStat )((uint8 *)
                                          stats->rateStats +
                                          (i * sizeof(tSirWifiRateStat)));
            rates = nla_nest_start(vendor_event, i);
            if (rates == NULL)
                goto error;

            if (FALSE == put_wifi_rate_stat(pRateStats, vendor_event))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                       FL("QCA_WLAN_VENDOR_ATTR put fail"));
                return FALSE;
            }
            nla_nest_end(vendor_event, rates);
        }
        nla_nest_end(vendor_event, rateInfo);
    }

    return TRUE;
error:
    return FALSE;
}

static bool put_wifi_wmm_ac_stat( tpSirWifiWmmAcStat stats,
                                  struct sk_buff *vendor_event)
{
    if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_AC,
                    stats->ac ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MPDU,
                    stats->txMpdu ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MPDU,
                    stats->rxMpdu ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MCAST,
                    stats->txMcast ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MCAST,
                    stats->rxMcast ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_AMPDU,
                    stats->rxAmpdu ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_AMPDU,
                    stats->txAmpdu ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_MPDU_LOST,
                    stats->mpduLost )||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES,
                    stats->retries ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_SHORT,
                    stats->retriesShort ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_LONG,
                    stats->retriesLong ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MIN,
                    stats->contentionTimeMin ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MAX,
                    stats->contentionTimeMax ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_AVG,
                    stats->contentionTimeAvg ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_NUM_SAMPLES,
                    stats->contentionNumSamples ))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("QCA_WLAN_VENDOR_ATTR put fail") );
        return FALSE;
    }

    return TRUE;
}

static bool put_wifi_interface_info(tpSirWifiInterfaceInfo stats,
                                    struct sk_buff *vendor_event)
{
    if (nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MODE,
                    stats->mode ) ||
        nla_put(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MAC_ADDR,
                VOS_MAC_ADDR_SIZE, stats->macAddr) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_STATE,
                    stats->state ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_ROAMING,
                    stats->roaming ) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_CAPABILITIES,
                    stats->capabilities ) ||
        nla_put(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_SSID,
                strlen(stats->ssid), stats->ssid) ||
        nla_put(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_BSSID,
                VOS_MAC_ADDR_SIZE, stats->bssid) ||
        nla_put(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_AP_COUNTRY_STR,
                WNI_CFG_COUNTRY_CODE_LEN, stats->apCountryStr) ||
        nla_put(vendor_event,
                QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_COUNTRY_STR,
                WNI_CFG_COUNTRY_CODE_LEN, stats->countryStr))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("QCA_WLAN_VENDOR_ATTR put fail") );
        return FALSE;
    }

    return TRUE;
}

static bool put_wifi_iface_stats(tpSirWifiIfaceStat pWifiIfaceStat,
                                 u32 num_peers,
                                 struct sk_buff *vendor_event)
{
    int i = 0;
    struct nlattr *wmmInfo;
    struct nlattr *wmmStats;
    u64 average_tsf_offset;

    if (FALSE == put_wifi_interface_info(
            &pWifiIfaceStat->info,
            vendor_event))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("QCA_WLAN_VENDOR_ATTR put fail") );
        return FALSE;

    }

    average_tsf_offset =  pWifiIfaceStat->avg_bcn_spread_offset_high;
    average_tsf_offset =  (average_tsf_offset << 32) |
        pWifiIfaceStat->avg_bcn_spread_offset_low ;

    if (nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE_IFACE) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS,
                    num_peers) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX,
                    pWifiIfaceStat->beaconRx) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX,
                    pWifiIfaceStat->mgmtRx) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX,
                    pWifiIfaceStat->mgmtActionRx) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX,
                    pWifiIfaceStat->mgmtActionTx) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT,
                    pWifiIfaceStat->rssiMgmt) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA,
                    pWifiIfaceStat->rssiData) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK,
                    pWifiIfaceStat->rssiAck) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_DETECTED,
                    pWifiIfaceStat->is_leaky_ap) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_AVG_NUM_FRAMES_LEAKED,
                    pWifiIfaceStat->avg_rx_frms_leaked) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_GUARD_TIME,
                    pWifiIfaceStat->rx_leak_window) ||
        nla_put_u64(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_AVERAGE_TSF_OFFSET,
                    average_tsf_offset))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("QCA_WLAN_VENDOR_ATTR put fail"));
        return FALSE;
    }

    wmmInfo = nla_nest_start(vendor_event,
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO);
    if (wmmInfo == NULL)
        return FALSE;

    for (i = 0; i < WIFI_AC_MAX; i++)
    {
        wmmStats = nla_nest_start(vendor_event, i);
        if (wmmStats == NULL)
            return FALSE;

        if (FALSE == put_wifi_wmm_ac_stat(
                &pWifiIfaceStat->AccessclassStats[i],
                vendor_event))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   FL("put_wifi_wmm_ac_stat Fail"));
            return FALSE;
        }

        nla_nest_end(vendor_event, wmmStats);
    }
    nla_nest_end(vendor_event, wmmInfo);
    return TRUE;
}

static tSirWifiInterfaceMode
hdd_map_device_to_ll_iface_mode ( int deviceMode )
{
    switch (deviceMode)
    {
    case  WLAN_HDD_INFRA_STATION:
        return WIFI_INTERFACE_STA;
    case  WLAN_HDD_SOFTAP:
        return WIFI_INTERFACE_SOFTAP;
    case  WLAN_HDD_P2P_CLIENT:
        return WIFI_INTERFACE_P2P_CLIENT;
    case  WLAN_HDD_P2P_GO:
        return WIFI_INTERFACE_P2P_GO;
    case  WLAN_HDD_IBSS:
        return WIFI_INTERFACE_IBSS;
    default:
        /* Return Interface Mode as STA for all the unsupported modes */
        return WIFI_INTERFACE_STA;
    }
}

static bool hdd_get_interface_info(hdd_adapter_t *pAdapter,
                           tpSirWifiInterfaceInfo pInfo)
{
    v_U8_t *staMac = NULL;
    hdd_station_ctx_t *pHddStaCtx;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    pInfo->mode = hdd_map_device_to_ll_iface_mode(pAdapter->device_mode);

    vos_mem_copy(pInfo->macAddr,
        pAdapter->macAddressCurrent.bytes, sizeof(v_MACADDR_t));

    if (((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
         (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) ||
         (WLAN_HDD_P2P_DEVICE == pAdapter->device_mode)))
    {
        pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
        if (eConnectionState_NotConnected == pHddStaCtx->conn_info.connState)
        {
            pInfo->state = WIFI_DISCONNECTED;
        }
        if (eConnectionState_Connecting == pHddStaCtx->conn_info.connState)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: Session ID %d, Connection is in progress", __func__,
                   pAdapter->sessionId);
            pInfo->state = WIFI_ASSOCIATING;
        }
        if ((eConnectionState_Associated == pHddStaCtx->conn_info.connState) &&
            (VOS_FALSE == pHddStaCtx->conn_info.uIsAuthenticated))
        {
            staMac = (v_U8_t *) &(pAdapter->macAddressCurrent.bytes[0]);
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: client " MAC_ADDRESS_STR
                   " is in the middle of WPS/EAPOL exchange.", __func__,
                   MAC_ADDR_ARRAY(staMac));
            pInfo->state = WIFI_AUTHENTICATING;
        }
        if (eConnectionState_Associated == pHddStaCtx->conn_info.connState)
        {
            pInfo->state = WIFI_ASSOCIATED;
            vos_mem_copy(pInfo->bssid,
                         &pHddStaCtx->conn_info.bssId, VOS_MAC_ADDR_SIZE);
            vos_mem_copy(pInfo->ssid,
                         pHddStaCtx->conn_info.SSID.SSID.ssId,
                         pHddStaCtx->conn_info.SSID.SSID.length);
            /*
             * NULL Terminate the string
             */
            pInfo->ssid[pHddStaCtx->conn_info.SSID.SSID.length] = 0;
        }
    }

    vos_mem_copy(pInfo->countryStr,
        pMac->scan.countryCodeCurrent, WNI_CFG_COUNTRY_CODE_LEN);

    vos_mem_copy(pInfo->apCountryStr,
                 pMac->scan.countryCodeCurrent, WNI_CFG_COUNTRY_CODE_LEN);

    return TRUE;
}

/*
 * hdd_link_layer_process_peer_stats () - This function is called after
 * receiving Link Layer Peer statistics from FW.This function converts
 * the firmware data to the NL data and sends the same to the kernel/upper
 * layers.
 */
static void hdd_link_layer_process_peer_stats(hdd_adapter_t *pAdapter,
                                              u32 more_data,
                                              tpSirWifiPeerStat pData)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tpSirWifiRateStat   pWifiRateStat;
    tpSirWifiPeerStat   pWifiPeerStat;
    tpSirWifiPeerInfo   pWifiPeerInfo;
    struct sk_buff *vendor_event;
    int status, i, j;
    struct nlattr *peers;
    int numRate;

    pWifiPeerStat = pData;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_PEER_ALL : numPeers %u, more data = %u",
           pWifiPeerStat->numPeers,
           more_data);

    for (i = 0; i < pWifiPeerStat->numPeers; i++)
    {
        pWifiPeerInfo = (tpSirWifiPeerInfo)
            ((uint8 *)pWifiPeerStat->peerInfo +
             ( i * sizeof(tSirWifiPeerInfo)));

        hddLog(VOS_TRACE_LEVEL_INFO,
               " %d) LL_STATS Channel Stats "
               " Peer Type %u "
               " peerMacAddress  %pM "
               " capabilities 0x%x "
               " numRate %u ",
               i,
               pWifiPeerInfo->type,
               pWifiPeerInfo->peerMacAddress,
               pWifiPeerInfo->capabilities,
               pWifiPeerInfo->numRate);
        {
            for (j = 0; j < pWifiPeerInfo->numRate; j++)
            {
                pWifiRateStat = (tpSirWifiRateStat)
                    ((tANI_U8 *) pWifiPeerInfo->rateStats +
                     ( j * sizeof(tSirWifiRateStat)));

                hddLog(VOS_TRACE_LEVEL_INFO,
                       "   peer Rate Stats "
                       "   preamble  %u "
                       "   nss %u "
                       "   bw %u "
                       "   rateMcsIdx  %u "
                       "   reserved %u "
                       "   bitrate %u "
                       "   txMpdu %u "
                       "   rxMpdu %u "
                       "   mpduLost %u "
                       "   retries %u "
                       "   retriesShort %u "
                       "   retriesLong %u",
                       pWifiRateStat->rate.preamble,
                       pWifiRateStat->rate.nss,
                       pWifiRateStat->rate.bw,
                       pWifiRateStat->rate.rateMcsIdx,
                       pWifiRateStat->rate.reserved,
                       pWifiRateStat->rate.bitrate,
                       pWifiRateStat->txMpdu,
                       pWifiRateStat->rxMpdu,
                       pWifiRateStat->mpduLost,
                       pWifiRateStat->retries,
                       pWifiRateStat->retriesShort,
                       pWifiRateStat->retriesLong);
            }
        }
    }

    /*
     * Allocate a size of 4096 for the peer stats comprising
     * each of size = sizeof (tSirWifiPeerInfo) + numRate *
     * sizeof (tSirWifiRateStat).Each field is put with an
     * NL attribute.The size of 4096 is considered assuming
     * that number of rates shall not exceed beyond 50 with
     * the sizeof (tSirWifiRateStat) being 32.
     */
    vendor_event = cfg80211_vendor_cmd_alloc_reply_skb(pHddCtx->wiphy,
                       LL_STATS_EVENT_BUF_SIZE);

    if (!vendor_event)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: cfg80211_vendor_cmd_alloc_reply_skb failed",
               __func__);
        return;
    }

    if (nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE_PEER) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RESULTS_MORE_DATA,
                    more_data) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS,
                    pWifiPeerStat->numPeers))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: QCA_WLAN_VENDOR_ATTR put fail", __func__);

        kfree_skb(vendor_event);
        return;
    }


    pWifiPeerInfo = (tpSirWifiPeerInfo) ((uint8 *)
                                         pWifiPeerStat->peerInfo);

    if (pWifiPeerStat->numPeers)
    {
        struct nlattr *peerInfo;
        peerInfo = nla_nest_start(vendor_event,
                              QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO);
        if (peerInfo == NULL) {
            hddLog(LOGE, FL("nla_nest_start failed"));
            kfree_skb(vendor_event);
            return;
        }

        for (i = 1; i <= pWifiPeerStat->numPeers; i++)
        {
            peers = nla_nest_start(vendor_event, i);
            if (peers == NULL) {
                hddLog(LOGE, FL("nla_nest_start failed"));
                kfree_skb(vendor_event);
                return;
            }

            numRate = pWifiPeerInfo->numRate;

            if (FALSE == put_wifi_peer_info(
                    pWifiPeerInfo, vendor_event))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, FL("put_wifi_peer_info fail"));
                kfree_skb(vendor_event);
                return;
            }

            pWifiPeerInfo = (tpSirWifiPeerInfo) ((uint8 *)
                                         pWifiPeerStat->peerInfo +
                                         (i * sizeof(tSirWifiPeerInfo)) +
                                         (numRate * sizeof (tSirWifiRateStat)));
            nla_nest_end(vendor_event, peers);
        }
        nla_nest_end(vendor_event, peerInfo);
    }
    cfg80211_vendor_cmd_reply(vendor_event);
    return;
}

/*
 * hdd_link_layer_process_iface_stats () - This function is called after
 * receiving Link Layer Interface statistics from FW.This function converts
 * the firmware data to the NL data and sends the same to the kernel/upper
 * layers.
 */
static void hdd_link_layer_process_iface_stats(hdd_adapter_t *pAdapter,
                                               tpSirWifiIfaceStat pData,
                                               u32 num_peers)
{
    tpSirWifiIfaceStat  pWifiIfaceStat;
    struct sk_buff *vendor_event;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    int status;
    int i;

    pWifiIfaceStat = pData;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid") );
        return;
    }

    /*
     * Allocate a size of 4096 for the interface stats comprising
     * sizeof (tpSirWifiIfaceStat).The size of 4096 is considered
     * assuming that all these fit with in the limit.Please take
     * a call on the limit based on the data requirements on
     * interface statistics.
     */
    vendor_event = cfg80211_vendor_cmd_alloc_reply_skb(pHddCtx->wiphy,
                        LL_STATS_EVENT_BUF_SIZE);

    if (!vendor_event)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("cfg80211_vendor_cmd_alloc_reply_skb failed") );
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "WMI_LINK_STATS_IFACE Data");

    if (FALSE == hdd_get_interface_info(pAdapter,
                                        &pWifiIfaceStat->info))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("hdd_get_interface_info get fail"));
        kfree_skb(vendor_event);
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
           " Num peers %u "
           "LL_STATS_IFACE: "
           " Mode %u "
           " MAC %pM "
           " State %u "
           " Roaming %u "
           " capabilities 0x%x "
           " SSID %s "
           " BSSID %pM",
           num_peers,
           pWifiIfaceStat->info.mode,
           pWifiIfaceStat->info.macAddr,
           pWifiIfaceStat->info.state,
           pWifiIfaceStat->info.roaming,
           pWifiIfaceStat->info.capabilities,
           pWifiIfaceStat->info.ssid,
           pWifiIfaceStat->info.bssid);

    hddLog(VOS_TRACE_LEVEL_INFO,
           " AP country str: %c%c%c",
           pWifiIfaceStat->info.apCountryStr[0],
           pWifiIfaceStat->info.apCountryStr[1],
           pWifiIfaceStat->info.apCountryStr[2]);

    hddLog(VOS_TRACE_LEVEL_INFO,
           " Country Str Association: %c%c%c",
           pWifiIfaceStat->info.countryStr[0],
           pWifiIfaceStat->info.countryStr[1],
           pWifiIfaceStat->info.countryStr[2]);

    hddLog(VOS_TRACE_LEVEL_INFO,
           " beaconRx %u "
           " mgmtRx %u "
           " mgmtActionRx  %u "
           " mgmtActionTx %u "
           " rssiMgmt %u "
           " rssiData %u "
           " rssiAck %u "
           " avg_bcn_spread_offset_high %u"
           " avg_bcn_spread_offset_low %u"
           " is leaky_ap %u"
           " avg_rx_frms_leaked %u"
           " rx_leak_window %u",
           pWifiIfaceStat->beaconRx,
           pWifiIfaceStat->mgmtRx,
           pWifiIfaceStat->mgmtActionRx,
           pWifiIfaceStat->mgmtActionTx,
           pWifiIfaceStat->rssiMgmt,
           pWifiIfaceStat->rssiData,
           pWifiIfaceStat->rssiAck,
           pWifiIfaceStat->avg_bcn_spread_offset_high,
           pWifiIfaceStat->avg_bcn_spread_offset_low,
           pWifiIfaceStat->is_leaky_ap,
           pWifiIfaceStat->avg_rx_frms_leaked,
           pWifiIfaceStat->rx_leak_window);

    for (i = 0; i < WIFI_AC_MAX; i++)
    {
        hddLog(VOS_TRACE_LEVEL_INFO,

               " %d) LL_STATS IFACE: "
               " ac:  %u  txMpdu: %u "
               " rxMpdu: %u txMcast: %u "
               " rxMcast: %u  rxAmpdu: %u "
               " txAmpdu:  %u  mpduLost: %u "
               " retries: %u  retriesShort: %u "
               " retriesLong: %u  contentionTimeMin: %u "
               " contentionTimeMax: %u  contentionTimeAvg: %u "
               " contentionNumSamples: %u",
               i,
               pWifiIfaceStat->AccessclassStats[i].ac,
               pWifiIfaceStat->AccessclassStats[i].txMpdu,
               pWifiIfaceStat->AccessclassStats[i].rxMpdu,
               pWifiIfaceStat->AccessclassStats[i].txMcast,
               pWifiIfaceStat->AccessclassStats[i].rxMcast,
               pWifiIfaceStat->AccessclassStats[i].rxAmpdu,
               pWifiIfaceStat->AccessclassStats[i].txAmpdu,
               pWifiIfaceStat->AccessclassStats[i].mpduLost,
               pWifiIfaceStat->AccessclassStats[i].retries,
               pWifiIfaceStat->AccessclassStats[i].retriesShort,
               pWifiIfaceStat->AccessclassStats[i].retriesLong,
               pWifiIfaceStat->AccessclassStats[i].contentionTimeMin,
               pWifiIfaceStat->AccessclassStats[i].contentionTimeMax,
               pWifiIfaceStat->AccessclassStats[i].contentionTimeAvg,
               pWifiIfaceStat->AccessclassStats[i].contentionNumSamples);
    }

    if (FALSE == put_wifi_iface_stats(pWifiIfaceStat, num_peers, vendor_event)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("put_wifi_iface_stats fail"));
        kfree_skb(vendor_event);
        return;
    }

    cfg80211_vendor_cmd_reply(vendor_event);
    return;
}

/*
 * hdd_link_layer_process_radio_stats () - This function is called after
 * receiving Link Layer Radio statistics from FW.This function converts
 * the firmware data to the NL data and sends the same to the kernel/upper
 * layers.
 */
static void hdd_link_layer_process_radio_stats(hdd_adapter_t *pAdapter,
                                               u32 more_data,
                                               tpSirWifiRadioStat pData,
                                               u32 num_radio)
{
    int status, i;
    tpSirWifiRadioStat  pWifiRadioStat;
    tpSirWifiChannelStats pWifiChannelStats;
    struct sk_buff *vendor_event;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    pWifiRadioStat = pData;
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_RADIO"
           " number of radios = %u"
           " radio is %d onTime is %u"
           " txTime is %u  rxTime is %u"
           " onTimeScan is %u  onTimeNbd is %u"
           " onTimeGscan is %u onTimeRoamScan is %u"
           " onTimePnoScan is %u  onTimeHs20 is %u"
           " numChannels is %u",
           num_radio,
           pWifiRadioStat->radio,
           pWifiRadioStat->onTime,
           pWifiRadioStat->txTime,
           pWifiRadioStat->rxTime,
           pWifiRadioStat->onTimeScan,
           pWifiRadioStat->onTimeNbd,
           pWifiRadioStat->onTimeGscan,
           pWifiRadioStat->onTimeRoamScan,
           pWifiRadioStat->onTimePnoScan,
           pWifiRadioStat->onTimeHs20,
           pWifiRadioStat->numChannels);

    /*
     * Allocate a size of 4096 for the Radio stats comprising
     * sizeof (tSirWifiRadioStat) + numChannels * sizeof
     * (tSirWifiChannelStats).Each channel data is put with an
     * NL attribute.The size of 4096 is considered assuming that
     * number of channels shall not exceed beyond  60 with the
     * sizeof (tSirWifiChannelStats) being 24 bytes.
     */

    vendor_event = cfg80211_vendor_cmd_alloc_reply_skb(pHddCtx->wiphy,
                       LL_STATS_EVENT_BUF_SIZE);

    if (!vendor_event)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
        return;
    }

    if (nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE_RADIO) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RESULTS_MORE_DATA,
                    more_data)                  ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_NUM_RADIOS,
                    num_radio)                  ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID,
                    pWifiRadioStat->radio)      ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME,
                    pWifiRadioStat->onTime)     ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME,
                    pWifiRadioStat->txTime)     ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_RX_TIME,
                    pWifiRadioStat->rxTime)     ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_SCAN,
                    pWifiRadioStat->onTimeScan) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_NBD,
                    pWifiRadioStat->onTimeNbd)  ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_GSCAN,
                    pWifiRadioStat->onTimeGscan)||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_ROAM_SCAN,
                    pWifiRadioStat->onTimeRoamScan) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_PNO_SCAN,
                    pWifiRadioStat->onTimePnoScan) ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_HS20,
                    pWifiRadioStat->onTimeHs20)    ||
        nla_put_u32(vendor_event,
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS,
                    pWifiRadioStat->numChannels))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("QCA_WLAN_VENDOR_ATTR put fail"));

        kfree_skb(vendor_event);
        return ;
    }

    if (pWifiRadioStat->numChannels)
    {
        struct nlattr *chList;
        struct nlattr *chInfo;

        chList = nla_nest_start(vendor_event,
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO);
        if (chList == NULL) {
            hddLog(LOGE, FL("nla_nest_start failed"));
            kfree_skb(vendor_event);
            return;
        }

        for (i = 0; i < pWifiRadioStat->numChannels; i++)
        {
            pWifiChannelStats = (tpSirWifiChannelStats) ((uint8*)
                                             pWifiRadioStat->channels +
                                            (i * sizeof(tSirWifiChannelStats)));

            hddLog(VOS_TRACE_LEVEL_INFO,
                   " %d) Channel Info"
                   "  width is %u "
                   "  CenterFreq %u "
                   "  CenterFreq0 %u "
                   "  CenterFreq1 %u "
                   "  onTime %u "
                   "  ccaBusyTime %u",
                   i,
                   pWifiChannelStats->channel.width,
                   pWifiChannelStats->channel.centerFreq,
                   pWifiChannelStats->channel.centerFreq0,
                   pWifiChannelStats->channel.centerFreq1,
                   pWifiChannelStats->onTime,
                   pWifiChannelStats->ccaBusyTime);

            chInfo = nla_nest_start(vendor_event, i);
            if (chInfo == NULL) {
                hddLog(LOGE, FL("nla_nest_start failed"));
                kfree_skb(vendor_event);
                return;
            }

            if (nla_put_u32(vendor_event,
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_WIDTH,
                        pWifiChannelStats->channel.width) ||
                nla_put_u32(vendor_event,
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ,
                        pWifiChannelStats->channel.centerFreq) ||
                nla_put_u32(vendor_event,
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ0,
                        pWifiChannelStats->channel.centerFreq0)  ||
                nla_put_u32(vendor_event,
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ1,
                        pWifiChannelStats->channel.centerFreq1)    ||
                nla_put_u32(vendor_event,
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_ON_TIME,
                        pWifiChannelStats->onTime)  ||
                nla_put_u32(vendor_event,
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_CCA_BUSY_TIME,
                        pWifiChannelStats->ccaBusyTime))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla_put failed"));
                kfree_skb(vendor_event);
                return ;
            }
            nla_nest_end(vendor_event, chInfo);
        }
        nla_nest_end(vendor_event, chList);
    }
    cfg80211_vendor_cmd_reply(vendor_event);
    return;
}

/*
 * wlan_hdd_cfg80211_link_layer_stats_callback () - This function is called
 * after receiving Link Layer indications from FW.This callback converts the
 * firmware data to the NL data and send the same to the kernel/upper layers.
 */
static void wlan_hdd_cfg80211_link_layer_stats_callback(void *ctx,
                                                        int indType,
                                                        void *pRsp)
{
    hdd_adapter_t *pAdapter = NULL;
    struct hdd_ll_stats_context *context;
    hdd_context_t *pHddCtx = ctx;
    tpSirLLStatsResults linkLayerStatsResults = (tpSirLLStatsResults)pRsp;
    int status;

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return;
    }

    pAdapter = hdd_get_adapter_by_vdev(pHddCtx,
                                       linkLayerStatsResults->ifaceId);

    if (NULL == pAdapter)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: vdev_id %d does not exist with host",
                  __func__, linkLayerStatsResults->ifaceId);
        return;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "%s: Link Layer Indication indType: %d", __func__, indType);

    switch (indType)
    {
    case SIR_HAL_LL_STATS_RESULTS_RSP:
        {
            hddLog(VOS_TRACE_LEVEL_INFO,
                    FL("RESPONSE SIR_HAL_LL_STATS_RESULTS_RSP"));
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "LL_STATS RESULTS RESPONSE paramID = 0x%x",
                   linkLayerStatsResults->paramId);
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "LL_STATS RESULTS RESPONSE ifaceId = %u",
                    linkLayerStatsResults->ifaceId);
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "LL_STATS RESULTS RESPONSE respId = %u",
                    linkLayerStatsResults->rspId);
            hddLog(VOS_TRACE_LEVEL_INFO,
                   "LL_STATS RESULTS RESPONSE more data = %u",
                   linkLayerStatsResults->moreResultToFollow);
            hddLog(VOS_TRACE_LEVEL_INFO,
                   "LL_STATS RESULTS RESPONSE num radio = %u",
                   linkLayerStatsResults->num_radio);
            hddLog(VOS_TRACE_LEVEL_INFO,
                   "LL_STATS RESULTS RESPONSE result = %p",
                   linkLayerStatsResults->results);

            spin_lock(&hdd_context_lock);
            context = &pHddCtx->ll_stats_context;
            /* validate response received from target */
            if ((context->request_id != linkLayerStatsResults->rspId) ||
                !(context->request_bitmap & linkLayerStatsResults->paramId)) {
                 spin_unlock(&hdd_context_lock);
                 hddLog(LOGE,
                     FL("Error : Request id %d response id %d request bitmap 0x%x response bitmap 0x%x"),
                     context->request_id, linkLayerStatsResults->rspId,
                     context->request_bitmap, linkLayerStatsResults->paramId);
                 return;
            }
            spin_unlock(&hdd_context_lock);

            if (linkLayerStatsResults->paramId & WMI_LINK_STATS_RADIO )
            {
                hdd_link_layer_process_radio_stats(pAdapter,
                                   linkLayerStatsResults->moreResultToFollow,
                                   (tpSirWifiRadioStat)
                                   linkLayerStatsResults->results,
                                   linkLayerStatsResults->num_radio);

                spin_lock(&hdd_context_lock);
                if (!linkLayerStatsResults->moreResultToFollow)
                     context->request_bitmap &= ~(WMI_LINK_STATS_RADIO);
                spin_unlock(&hdd_context_lock);

            }
            else if (linkLayerStatsResults->paramId & WMI_LINK_STATS_IFACE )
            {
                hdd_link_layer_process_iface_stats(pAdapter,
                                   (tpSirWifiIfaceStat)
                                   linkLayerStatsResults->results,
                                   linkLayerStatsResults->num_peers);

                spin_lock(&hdd_context_lock);
                /* Firmware doesn't send peerstats event if no peers are
                 * connected. HDD should not wait for any peerstats in this case
                 * and return the status to middlewre after receiving iface
                 * stats
                 */
                if (!linkLayerStatsResults->num_peers)
                     context->request_bitmap &= ~(WMI_LINK_STATS_ALL_PEER);
                context->request_bitmap &= ~(WMI_LINK_STATS_IFACE);
                spin_unlock(&hdd_context_lock);

            }
            else if (linkLayerStatsResults->paramId & WMI_LINK_STATS_ALL_PEER )
            {
                hdd_link_layer_process_peer_stats(pAdapter,
                                   linkLayerStatsResults->moreResultToFollow,
                                   (tpSirWifiPeerStat)
                                   linkLayerStatsResults->results);

                spin_lock(&hdd_context_lock);
                if (!linkLayerStatsResults->moreResultToFollow)
                     context->request_bitmap &= ~(WMI_LINK_STATS_ALL_PEER);
                spin_unlock(&hdd_context_lock);

            }
            else
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        FL("INVALID LL_STATS_NOTIFY RESPONSE ***********"));
            }

            spin_lock(&hdd_context_lock);
            /* complete response event if all requests bitmaps are cleared */
            if (0 == context->request_bitmap)
                complete(&context->response_event);
            spin_unlock(&hdd_context_lock);

            break;
        }
        default:
            hddLog(VOS_TRACE_LEVEL_ERROR, "invalid event type %d", indType);
            break;
    }

    return;
}


void wlan_hdd_cfg80211_link_layer_stats_init(hdd_context_t *pHddCtx)
{
    sme_SetLinkLayerStatsIndCB(pHddCtx->hHal,
                                 wlan_hdd_cfg80211_link_layer_stats_callback);
}


const struct
nla_policy
qca_wlan_vendor_ll_set_policy[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD] =
    { .type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING] =
    { .type = NLA_U32 },
};

static int __wlan_hdd_cfg80211_ll_stats_set(struct wiphy *wiphy,
                                          struct wireless_dev *wdev,
                                          const void *data,
                                          int data_len)
{
    int status;
    struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX + 1];
    tSirLLStatsSetReq LinkLayerStatsSetReq;
    struct net_device *dev = wdev->netdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }

    if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX,
                  (struct nlattr *)data,
                  data_len, qca_wlan_vendor_ll_set_policy))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("maximum attribute not present"));
        return -EINVAL;
    }

    if (!tb_vendor
            [QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("MPDU size Not present"));
        return -EINVAL;
    }

    if (!tb_vendor[
         QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Stats Gathering Not Present"));
        return -EINVAL;
    }

    /* Shall take the request Id if the Upper layers pass. 1 For now.*/
    LinkLayerStatsSetReq.reqId = 1;

    LinkLayerStatsSetReq.mpduSizeThreshold =
        nla_get_u32(
            tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD]);

    LinkLayerStatsSetReq.aggressiveStatisticsGathering =
        nla_get_u32(
            tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING]);

    LinkLayerStatsSetReq.staId = pAdapter->sessionId;

    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_SET reqId = %d",
           LinkLayerStatsSetReq.reqId);
    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_SET staId = %d", LinkLayerStatsSetReq.staId);
    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_SET mpduSizeThreshold = %d",
           LinkLayerStatsSetReq.mpduSizeThreshold);
    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_SET aggressive Statistics Gathering  = %d",
           LinkLayerStatsSetReq.aggressiveStatisticsGathering);


    if (eHAL_STATUS_SUCCESS != sme_LLStatsSetReq(pHddCtx->hHal,
                                                 &LinkLayerStatsSetReq))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s:"
               "sme_LLStatsSetReq Failed", __func__);
        return -EINVAL;
    }

    pAdapter->isLinkLayerStatsSet = 1;

    return 0;
}

/**
 * wlan_hdd_cfg80211_ll_stats_set() - set ll stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 if success, non-zero for failure
 */
static int wlan_hdd_cfg80211_ll_stats_set(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_ll_stats_set(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

const struct
nla_policy
qca_wlan_vendor_ll_get_policy[QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX +1] =
{
    /* Unsigned 32bit value provided by the caller issuing the GET stats
     * command. When reporting
     * the stats results, the driver uses the same value to indicate
     * which GET request the results
     * correspond to.
     */
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID] = { .type = NLA_U32 },

    /* Unsigned 32bit value . bit mask to identify what statistics are
       requested for retrieval */
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK] = { .type = NLA_U32 }
};

static int __wlan_hdd_cfg80211_ll_stats_get(struct wiphy *wiphy,
                                          struct wireless_dev *wdev,
                                          const void *data,
                                          int data_len)
{
    unsigned long rc;
    struct hdd_ll_stats_context *context;
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX + 1];
    tSirLLStatsGetReq LinkLayerStatsGetReq;
    struct net_device *dev = wdev->netdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    int status;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL ;
    }

    if (!pAdapter->isLinkLayerStatsSet)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
               "%s: isLinkLayerStatsSet : %d",
               __func__, pAdapter->isLinkLayerStatsSet);
        return -EINVAL;
    }

    if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX,
                  (struct nlattr *)data,
                  data_len, qca_wlan_vendor_ll_get_policy))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("max attribute not present"));
        return -EINVAL;
    }

    if (!tb_vendor
            [QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Request Id Not present"));
        return -EINVAL;
    }

    if (!tb_vendor
        [QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Req Mask Not present"));
        return -EINVAL;
    }

    LinkLayerStatsGetReq.reqId =
        nla_get_u32(tb_vendor[
                         QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID]);
    LinkLayerStatsGetReq.paramIdMask =
        nla_get_u32(tb_vendor[
                         QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK]);

    LinkLayerStatsGetReq.staId = pAdapter->sessionId;

    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_GET reqId = %d", LinkLayerStatsGetReq.reqId);
    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_GET staId = %d", LinkLayerStatsGetReq.staId);
    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_GET paramIdMask = %d",
           LinkLayerStatsGetReq.paramIdMask);

    spin_lock(&hdd_context_lock);
    context = &pHddCtx->ll_stats_context;
    context->request_id = LinkLayerStatsGetReq.reqId;
    context->request_bitmap = LinkLayerStatsGetReq.paramIdMask;
    INIT_COMPLETION(context->response_event);
    spin_unlock(&hdd_context_lock);

    if (eHAL_STATUS_SUCCESS != sme_LLStatsGetReq(pHddCtx->hHal,
                                                &LinkLayerStatsGetReq))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s:"
               "sme_LLStatsGetReq Failed", __func__);
        return -EINVAL;
    }

    rc = wait_for_completion_timeout(&context->response_event,
             msecs_to_jiffies(WLAN_WAIT_TIME_LL_STATS));
    if (!rc) {
        hddLog(LOGE,
            FL("Target response timed out request id %d request bitmap 0x%x"),
            context->request_id, context->request_bitmap);
        return -ETIMEDOUT;
    }

    return 0;
}

/**
 * wlan_hdd_cfg80211_ll_stats_get() - get ll stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 if success, non-zero for failure
 */
static int wlan_hdd_cfg80211_ll_stats_get(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_ll_stats_get(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

const struct
nla_policy
qca_wlan_vendor_ll_clr_policy[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ] = {.type = NLA_U8 },
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK] = {.type = NLA_U32 },
    [QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP] = {.type = NLA_U8 },
};

static int __wlan_hdd_cfg80211_ll_stats_clear(struct wiphy *wiphy,
                                            struct wireless_dev *wdev,
                                            const void *data,
                                            int data_len)
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX + 1];
    tSirLLStatsClearReq LinkLayerStatsClearReq;
    struct net_device *dev = wdev->netdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    u32 statsClearReqMask;
    u8 stopReq;
    int status;
    struct sk_buff *temp_skbuff;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }

    if (!pAdapter->isLinkLayerStatsSet)
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
               "%s: isLinkLayerStatsSet : %d",
               __func__, pAdapter->isLinkLayerStatsSet);
        return -EINVAL;
    }

    if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX,
                  (struct nlattr *)data,
                  data_len, qca_wlan_vendor_ll_clr_policy))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("STATS_CLR_MAX is not present"));
        return -EINVAL;
    }

    if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK] ||
        !tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ])
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Error in LL_STATS CLR CONFIG PARA"));
        return -EINVAL;
    }

    statsClearReqMask = LinkLayerStatsClearReq.statsClearReqMask =
        nla_get_u32(
            tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK]);

    stopReq = LinkLayerStatsClearReq.stopReq =
        nla_get_u8(
            tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ]);

    /*
     * Shall take the request Id if the Upper layers pass. 1 For now.
     */
    LinkLayerStatsClearReq.reqId = 1;

    LinkLayerStatsClearReq.staId = pAdapter->sessionId;

    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_CLEAR reqId = %d", LinkLayerStatsClearReq.reqId);
    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_CLEAR staId = %d", LinkLayerStatsClearReq.staId);
    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_CLEAR statsClearReqMask = 0x%X",
           LinkLayerStatsClearReq.statsClearReqMask);
    hddLog(VOS_TRACE_LEVEL_INFO,
           "LL_STATS_CLEAR stopReq  = %d",
           LinkLayerStatsClearReq.stopReq);

    if (eHAL_STATUS_SUCCESS == sme_LLStatsClearReq(pHddCtx->hHal,
                                                   &LinkLayerStatsClearReq))
    {
        temp_skbuff = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
                                                          2 * sizeof(u32) +
                                                          2 * NLMSG_HDRLEN);
        if (temp_skbuff != NULL)
        {
            if (nla_put_u32(temp_skbuff,
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK,
                            statsClearReqMask) ||
                nla_put_u32(temp_skbuff,
                            QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP,
                            stopReq))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, FL("LL_STATS_CLR put fail"));
                kfree_skb(temp_skbuff);
                return -EINVAL;
            }

            /* If the ask is to stop the stats collection as part of clear
             * (stopReq = 1) , ensure that no further requests of get
             * go to the firmware by having isLinkLayerStatsSet set to 0.
             * However it the stopReq as part of the clear request is 0 ,
             * the request to get the statistics are honoured as in this
             * case the firmware is just asked to clear the statistics.
             */
            if (stopReq == 1)
                pAdapter->isLinkLayerStatsSet = 0;

            return cfg80211_vendor_cmd_reply(temp_skbuff);
        }
        return -ENOMEM;
    }

    return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_ll_stats_clear() - clear ll stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 if success, non-zero for failure
 */
static int wlan_hdd_cfg80211_ll_stats_clear(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_ll_stats_clear(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

#ifdef FEATURE_WLAN_TDLS
/* EXT TDLS */
static const struct nla_policy
wlan_hdd_tdls_config_enable_policy[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR] = {.type = NLA_UNSPEC },
    [QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_CHANNEL] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_GLOBAL_OPERATING_CLASS] =
                                                       {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX_LATENCY_MS] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MIN_BANDWIDTH_KBPS] = {.type = NLA_S32 },

};

static const struct nla_policy
wlan_hdd_tdls_config_disable_policy[QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAC_ADDR] = {.type = NLA_UNSPEC },

};

static const struct nla_policy
wlan_hdd_tdls_config_state_change_policy[
                    QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAC_ADDR] = {.type = NLA_UNSPEC },
    [QCA_WLAN_VENDOR_ATTR_TDLS_NEW_STATE] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_STATE_REASON] = {.type = NLA_S32 },

};

static const struct
nla_policy
qca_wlan_vendor_get_wifi_info_policy[
				QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX +1] = {
	[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION] = {.type = NLA_U8 },
};

/**
 * wlan_hdd_cfg80211_get_wifi_info() - Get the wifi driver related info
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This is called when wlan driver needs to send wifi driver related info
 * (driver/fw version) to the user space application upon request.
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_get_wifi_info(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data, int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX + 1];
	tSirVersionString version;
	uint32_t version_len;
	uint32_t major_spid = 0, minor_spid = 0, siid = 0, crmid = 0;
	uint8_t attr;
	int status;
	struct sk_buff *reply_skb = NULL;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	if (nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_WIFI_INFO_GET_MAX, data,
		      data_len, qca_wlan_vendor_get_wifi_info_policy)) {
		hddLog(LOGE, FL("WIFI_INFO_GET NL CMD parsing failed"));
		return -EINVAL;
	}

	if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION]) {
		hddLog(LOG1, FL("Rcvd req for Driver version"));
		strlcpy(version, QWLAN_VERSIONSTR, sizeof(version));
		attr = QCA_WLAN_VENDOR_ATTR_WIFI_INFO_DRIVER_VERSION;
	} else if (tb_vendor[QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION]) {
		hddLog(LOG1, FL("Rcvd req for FW version"));
		hdd_get_fw_version(hdd_ctx, &major_spid, &minor_spid, &siid,
				   &crmid);
		snprintf(version, sizeof(version), "%d:%d:%d:%d",
			 major_spid, minor_spid, siid, crmid);
		attr = QCA_WLAN_VENDOR_ATTR_WIFI_INFO_FIRMWARE_VERSION;
	} else {
		hddLog(LOGE, FL("Invalid attribute in get wifi info request"));
		return -EINVAL;
	}

	version_len = strlen(version);
	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
			version_len + NLA_HDRLEN + NLMSG_HDRLEN);
	if (!reply_skb) {
		hddLog(LOGE, FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
		return -ENOMEM;
	}

	if (nla_put(reply_skb, attr, version_len, version)) {
		hddLog(LOGE, FL("nla put fail"));
		kfree_skb(reply_skb);
		return -EINVAL;
	}

	return cfg80211_vendor_cmd_reply(reply_skb);
}

/**
 * wlan_hdd_cfg80211_get_logger_supp_feature() - Get the wifi logger features
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This is called by userspace to know the supported logger features
 *
 * Return:   Return the Success or Failure code.
 */
static int
wlan_hdd_cfg80211_get_logger_supp_feature(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data, int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	int status;
	uint32_t features;
	struct sk_buff *reply_skb = NULL;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	features = 0;

	if (hdd_is_memdump_supported())
		features |= WIFI_LOGGER_MEMORY_DUMP_SUPPORTED;
	features |= WIFI_LOGGER_PER_PACKET_TX_RX_STATUS_SUPPORTED;
	features |= WIFI_LOGGER_CONNECT_EVENT_SUPPORTED;
	features |= WIFI_LOGGER_WAKE_LOCK_SUPPORTED;

	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
			sizeof(uint32_t) + NLA_HDRLEN + NLMSG_HDRLEN);
	if (!reply_skb) {
		hddLog(LOGE, FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
		return -ENOMEM;
	}

	hddLog(LOG1, FL("Supported logger features: 0x%0x"), features);
	if (nla_put_u32(reply_skb, QCA_WLAN_VENDOR_ATTR_LOGGER_SUPPORTED,
				   features)) {
		hddLog(LOGE, FL("nla put fail"));
		kfree_skb(reply_skb);
		return -EINVAL;
	}

	return cfg80211_vendor_cmd_reply(reply_skb);
}

static const struct nla_policy
wlan_hdd_tdls_config_get_status_policy[
                     QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAX +1] =
{
    [QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR] = {.type = NLA_UNSPEC },
    [QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_STATE] = {.type = NLA_S32 },
    [QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_REASON] = {.type = NLA_S32 },

};
static int __wlan_hdd_cfg80211_exttdls_get_status(struct wiphy *wiphy,
                                                struct wireless_dev *wdev,
                                                const void *data,
                                                int data_len)
{
    uint8_t peer[6]         = {0};
    struct net_device *dev  = wdev->netdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx  = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAX + 1];
    eHalStatus ret;
    tANI_S32 state;
    tANI_S32 reason;
    struct sk_buff *skb = NULL;

    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {
        return -EINVAL;
    }
    if (pHddCtx->cfg_ini->fTDLSExternalControl == FALSE) {
        return -ENOTSUPP;
    }
    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAX,
                    data, data_len,
                    wlan_hdd_tdls_config_get_status_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid attribute"));
        return -EINVAL;
    }

    /* Parse and fetch mac address */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr mac addr failed"));
        return -EINVAL;
    }

    memcpy(peer, nla_data(
           tb[QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_MAC_ADDR]),
           sizeof(peer));
    hddLog(VOS_TRACE_LEVEL_INFO, FL(MAC_ADDRESS_STR),MAC_ADDR_ARRAY(peer));

    ret = wlan_hdd_tdls_get_status(pAdapter, peer, &state, &reason);

    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("get status Failed"));
        return -EINVAL;
    }
    skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
                                              2 * sizeof(int32_t) +
                                              NLMSG_HDRLEN);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
        return -EINVAL;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Reason (%d) Status (%d) tdls peer " MAC_ADDRESS_STR),
                                 reason,
                                 state,
                                 MAC_ADDR_ARRAY(peer));

    if (nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_STATE, state) ||
        nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_TDLS_GET_STATUS_REASON, reason)) {

        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    return cfg80211_vendor_cmd_reply(skb);

nla_put_failure:
    kfree_skb(skb);
    return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_exttdls_get_status() - get ext tdls status
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
static int wlan_hdd_cfg80211_exttdls_get_status(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_exttdls_get_status(wiphy, wdev, data,
							data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

static int wlan_hdd_cfg80211_exttdls_callback(tANI_U8* mac,
                                              tANI_S32 state,
                                              tANI_S32 reason,
                                              void *ctx)
{
    hdd_adapter_t* pAdapter       = (hdd_adapter_t*)ctx;
    hdd_context_t *pHddCtx        = WLAN_HDD_GET_CTX(pAdapter);
    struct sk_buff *skb           = NULL;

    if (wlan_hdd_validate_context(pHddCtx)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid "));
        return -EINVAL;
    }

    if (pHddCtx->cfg_ini->fTDLSExternalControl == FALSE) {
        return -ENOTSUPP;
    }
    skb = cfg80211_vendor_event_alloc(
                            pHddCtx->wiphy,
                            EXTTDLS_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                            QCA_NL80211_VENDOR_SUBCMD_TDLS_STATE_CHANGE_INDEX,
                            GFP_KERNEL);

    if (!skb) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                  FL("cfg80211_vendor_event_alloc failed"));
        return -EINVAL;
    }
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Entering "));
    hddLog(VOS_TRACE_LEVEL_INFO, "Reason (%d) Status (%d)", reason, state);
    hddLog(VOS_TRACE_LEVEL_WARN, "tdls peer " MAC_ADDRESS_STR,
                                          MAC_ADDR_ARRAY(mac));

    if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_TDLS_STATE_MAC_ADDR,
                                              VOS_MAC_ADDR_SIZE, mac) ||
        nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_TDLS_NEW_STATE, state) ||
        nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_TDLS_STATE_REASON, reason)
       ) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, GFP_KERNEL);
    return (0);

nla_put_failure:
    kfree_skb(skb);
    return -EINVAL;
}

static int __wlan_hdd_cfg80211_exttdls_enable(struct wiphy *wiphy,
                                            struct wireless_dev *wdev,
                                            const void *data,
                                            int data_len)
{
    uint8_t peer[6]                            = {0};
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                     = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX + 1];
    eHalStatus status;
    tdls_req_params_t   pReqMsg = {0};

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    if (pHddCtx->cfg_ini->fTDLSExternalControl == FALSE) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("TDLS External Control is not enabled"));
        return -ENOTSUPP;
    }
    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX,
                    data, data_len,
                    wlan_hdd_tdls_config_enable_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }

    /* Parse and fetch mac address */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr mac addr failed"));
        return -EINVAL;
    }

    memcpy(peer, nla_data(
                tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAC_ADDR]),
                sizeof(peer));
    hddLog(VOS_TRACE_LEVEL_INFO, FL(MAC_ADDRESS_STR),MAC_ADDR_ARRAY(peer));

    /* Parse and fetch channel */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_CHANNEL]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr channel failed"));
        return -EINVAL;
    }
    pReqMsg.channel = nla_get_s32(
         tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_CHANNEL]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Channel Num (%d)"), pReqMsg.channel);

    /* Parse and fetch global operating class */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_GLOBAL_OPERATING_CLASS]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr operating class failed"));
        return -EINVAL;
    }
    pReqMsg.global_operating_class = nla_get_s32(
        tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_GLOBAL_OPERATING_CLASS]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Operating class (%d)"),
                                     pReqMsg.global_operating_class);

    /* Parse and fetch latency ms */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX_LATENCY_MS]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr latency failed"));
        return -EINVAL;
    }
    pReqMsg.max_latency_ms = nla_get_s32(
        tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MAX_LATENCY_MS]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Latency (%d)"),
                                     pReqMsg.max_latency_ms);

    /* Parse and fetch required bandwidth kbps */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MIN_BANDWIDTH_KBPS]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr bandwidth failed"));
        return -EINVAL;
    }

    pReqMsg.min_bandwidth_kbps = nla_get_s32(
        tb[QCA_WLAN_VENDOR_ATTR_TDLS_ENABLE_MIN_BANDWIDTH_KBPS]);
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Bandwidth (%d)"),
                                     pReqMsg.min_bandwidth_kbps);

    return (wlan_hdd_tdls_extctrl_config_peer(pAdapter,
                                 peer,
                                 wlan_hdd_cfg80211_exttdls_callback,
                                 pReqMsg.channel,
                                 pReqMsg.max_latency_ms,
                                 pReqMsg.global_operating_class,
                                 pReqMsg.min_bandwidth_kbps));
}

/**
 * wlan_hdd_cfg80211_exttdls_enable() - enable ext tdls
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
static int wlan_hdd_cfg80211_exttdls_enable(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_exttdls_enable(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

static int __wlan_hdd_cfg80211_exttdls_disable(struct wiphy *wiphy,
                                             struct wireless_dev *wdev,
                                             const void *data,
                                             int data_len)
{
    u8 peer[6]                                 = {0};
    struct net_device *dev                     = wdev->netdev;
    hdd_adapter_t *pAdapter                    = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx                     = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAX + 1];
    eHalStatus status;

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }
    if (pHddCtx->cfg_ini->fTDLSExternalControl == FALSE) {

        return -ENOTSUPP;
    }
    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAX,
                    data, data_len,
                    wlan_hdd_tdls_config_disable_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Invalid ATTR"));
        return -EINVAL;
    }
    /* Parse and fetch mac address */
    if (!tb[QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAC_ADDR]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr mac addr failed"));
        return -EINVAL;
    }

    memcpy(peer, nla_data(
                tb[QCA_WLAN_VENDOR_ATTR_TDLS_DISABLE_MAC_ADDR]),
                sizeof(peer));
    hddLog(VOS_TRACE_LEVEL_INFO, FL(MAC_ADDRESS_STR),MAC_ADDR_ARRAY(peer));

    return (wlan_hdd_tdls_extctrl_deconfig_peer(pAdapter, peer));
}

/**
 * wlan_hdd_cfg80211_exttdls_disable() - disable ext tdls
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * Return:   Return the Success or Failure code.
 */
static int wlan_hdd_cfg80211_exttdls_disable(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_exttdls_disable(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

#endif


static const struct nla_policy
wlan_hdd_set_no_dfs_flag_config_policy[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX
                                       +1] =
{
    [QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG] = {.type = NLA_U32 },
};


static int wlan_hdd_cfg80211_disable_dfs_chan_scan(struct wiphy *wiphy,
                                                  struct wireless_dev *wdev,
                                                  const void *data,
                                                  int data_len)
{
    struct net_device *dev = wdev->netdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    hdd_context_t *pHddCtx  = wiphy_priv(wiphy);
    struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX + 1];
    eHalStatus status;
    int ret_val = -EPERM;
    u32 no_dfs_flag = 0;
    hdd_adapter_list_node_t *p_adapter_node = NULL, *p_next = NULL;
    hdd_adapter_t *p_adapter;
    VOS_STATUS vos_status;
    hdd_ap_ctx_t *p_ap_ctx;
    hdd_station_ctx_t *p_sta_ctx;

    if (wlan_hdd_validate_context(pHddCtx)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("HDD context is not valid"));
        return -EINVAL;
    }

    if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG_MAX,
                    data, data_len,
                    wlan_hdd_set_no_dfs_flag_config_policy)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("invalid attr"));
        return -EINVAL;
    }

    if (!tb[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG]) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("attr dfs flag failed"));
        return -EINVAL;
    }

    no_dfs_flag = nla_get_u32(
        tb[QCA_WLAN_VENDOR_ATTR_SET_NO_DFS_FLAG]);

    hddLog(VOS_TRACE_LEVEL_INFO, FL(" DFS flag = %d"),
           no_dfs_flag);

    if (no_dfs_flag > 1) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("invalid value of dfs flag"));
        return -EINVAL;
    }

    if (no_dfs_flag == pHddCtx->cfg_ini->enableDFSChnlScan) {
        if (no_dfs_flag) {

            vos_status = hdd_get_front_adapter( pHddCtx, &p_adapter_node);
            while ((NULL != p_adapter_node) &&
                   (VOS_STATUS_SUCCESS == vos_status))
            {
                p_adapter = p_adapter_node->pAdapter;

                if (WLAN_HDD_SOFTAP == p_adapter->device_mode) {
                    p_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(p_adapter);

                    /* if there is SAP already running on DFS channel,
                       do not disable scan on dfs channels. Note that with
                       SAP on DFS, there cannot be conurrency on single
                       radio. But then we can have multiple radios !!!!! */
                    if (NV_CHANNEL_DFS ==
                        vos_nv_getChannelEnabledState(
                            p_ap_ctx->operatingChannel)) {
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                               FL("SAP running on DFS channel"));
                        return -EOPNOTSUPP;
                    }
                }

                if (WLAN_HDD_INFRA_STATION == p_adapter->device_mode) {
                    p_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(p_adapter);

                    /* if STA is already connected on DFS channel,
                       do not disable scan on dfs channels */
                    if (hdd_connIsConnected(p_sta_ctx) &&
                        (NV_CHANNEL_DFS ==
                         vos_nv_getChannelEnabledState(
                             p_sta_ctx->conn_info.operationChannel))) {
                        hddLog(VOS_TRACE_LEVEL_ERROR,
                               FL("client connected on DFS channel"));
                        return -EOPNOTSUPP;
                    }
                }

                vos_status = hdd_get_next_adapter(pHddCtx, p_adapter_node,
                                                  &p_next);
                p_adapter_node = p_next;
            }
        }

        pHddCtx->cfg_ini->enableDFSChnlScan = !no_dfs_flag;

        hdd_abort_mac_scan_all_adapters(pHddCtx);

        /* call the SME API to tunnel down the new channel list
           to the firmware  */
        status = sme_handle_dfs_chan_scan(hHal,
                                          pHddCtx->cfg_ini->enableDFSChnlScan);

        if (eHAL_STATUS_SUCCESS == status) {
            ret_val = 0;

            /* Clear the SME scan cache also. Note that the clearing of scan
             * results is independent of session; so no need to iterate over
             * all sessions
             */
            status = sme_ScanFlushResult(hHal, pAdapter->sessionId);
            if (eHAL_STATUS_SUCCESS != status)
                ret_val = -EPERM;
        }
    } else {
        hddLog(VOS_TRACE_LEVEL_INFO, FL(" the DFS flag has not changed"));
        ret_val = 0;
    }

    return ret_val;
}

static const struct nla_policy
wlan_hdd_wifi_config_policy[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX
                            +1] =
{
	[QCA_WLAN_VENDOR_ATTR_CONFIG_MODULATED_DTIM] = {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_STATS_AVG_FACTOR] = {.type = NLA_U16 },
	[QCA_WLAN_VENDOR_ATTR_CONFIG_GUARD_TIME] = {.type = NLA_U32 },
};


/**
 * wlan_hdd_cfg80211_wifi_configuration_set() - Wifi configuration
 * vendor command
 *
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Handles QCA_WLAN_VENDOR_ATTR_CONFIG_MAX.
 *
 * Return: EOK or other error codes.
 */
static int wlan_hdd_cfg80211_wifi_configuration_set(struct wiphy *wiphy,
						    struct wireless_dev *wdev,
						    const void *data,
						    int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx  = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MAX + 1];
	int ret_val = 0;
	u32 modulated_dtim;
	u16 stats_avg_factor;
	u32 guard_time;
	eHalStatus status;

	ret_val = wlan_hdd_validate_context(pHddCtx);
	if (ret_val) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return ret_val;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_CONFIG_MAX,
		      data, data_len,
		      wlan_hdd_wifi_config_policy)) {
		hddLog(LOGE, FL("invalid attr"));
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MODULATED_DTIM]) {
		modulated_dtim = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_CONFIG_MODULATED_DTIM]);

		status = sme_configure_modulated_dtim(pHddCtx->hHal,
							pAdapter->sessionId,
							modulated_dtim);

		if (eHAL_STATUS_SUCCESS != status)
			ret_val = -EPERM;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_STATS_AVG_FACTOR]) {
		stats_avg_factor = nla_get_u16(
			tb[QCA_WLAN_VENDOR_ATTR_CONFIG_STATS_AVG_FACTOR]);
		status = sme_configure_stats_avg_factor(pHddCtx->hHal,
							pAdapter->sessionId,
							stats_avg_factor);

		if (eHAL_STATUS_SUCCESS != status)
			ret_val = -EPERM;
	}


	if (tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GUARD_TIME]) {
		guard_time = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_CONFIG_GUARD_TIME]);
		status = sme_configure_guard_time(pHddCtx->hHal,
						  pAdapter->sessionId,
						  guard_time);

		if (eHAL_STATUS_SUCCESS != status)
			ret_val = -EPERM;
	}

	return ret_val;
}

#ifdef FEATURE_WLAN_TDLS

/* TDLS capabilities params */
#define PARAM_MAX_TDLS_SESSION \
		QCA_WLAN_VENDOR_ATTR_TDLS_GET_CAPS_MAX_CONC_SESSIONS
#define PARAM_TDLS_FEATURE_SUPPORT \
		QCA_WLAN_VENDOR_ATTR_TDLS_GET_CAPS_FEATURES_SUPPORTED

/**
 * wlan_hdd_cfg80211_get_tdls_capabilities() - Provide TDLS Capabilites.
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function provides TDLS capabilities
 *
 * Return: 0 on success and errno on failure
 */
static int wlan_hdd_cfg80211_get_tdls_capabilities(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	eHalStatus status;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct sk_buff *skb = NULL;
	uint32_t set = 0;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (eHAL_STATUS_SUCCESS != status) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, (2 * sizeof(u32)) +
						   NLMSG_HDRLEN);
	if (!skb) {
		hddLog(LOGE, FL("cfg80211_vendor_cmd_alloc_reply_skb failed"));
		goto fail;
	}

	if (FALSE == hdd_ctx->cfg_ini->fEnableTDLSSupport) {
		hddLog(LOGE,
			FL("TDLS feature not Enabled or Not supported in FW"));
		if (nla_put_u32(skb, PARAM_MAX_TDLS_SESSION, 0) ||
			nla_put_u32(skb, PARAM_TDLS_FEATURE_SUPPORT, 0)) {
			hddLog(LOGE, FL("nla put fail"));
			goto fail;
		}
	} else {
		set = set | WIFI_TDLS_SUPPORT;
		set = set | (hdd_ctx->cfg_ini->fTDLSExternalControl ?
					WIFI_TDLS_EXTERNAL_CONTROL_SUPPORT : 0);
		set = set | (hdd_ctx->cfg_ini->fEnableTDLSOffChannel ?
					WIIF_TDLS_OFFCHANNEL_SUPPORT : 0);
		hddLog(LOG1, FL("TDLS Feature supported value %x"), set);
		if (nla_put_u32(skb, PARAM_MAX_TDLS_SESSION,
				 hdd_ctx->max_num_tdls_sta) ||
			nla_put_u32(skb, PARAM_TDLS_FEATURE_SUPPORT,
				 set)) {
			hddLog(LOGE, FL("nla put fail"));
			goto fail;
		}
	}
	return cfg80211_vendor_cmd_reply(skb);
fail:
	if (skb)
		kfree_skb(skb);
	return -EINVAL;
}

#endif

static const struct
nla_policy
qca_wlan_vendor_wifi_logger_start_policy
[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_RING_ID]
		= {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_VERBOSE_LEVEL]
		= {.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_FLAGS]
		= {.type = NLA_U32 },
};

/**
 * __wlan_hdd_cfg80211_wifi_logger_start() - This function is used to enable
 * or disable the collection of packet statistics from the firmware
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function is used to enable or disable the collection of packet
 * statistics from the firmware
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_wifi_logger_start(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	eHalStatus status;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_MAX + 1];
	struct sir_wifi_start_log start_log;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_START_MAX,
		      data, data_len,
		      qca_wlan_vendor_wifi_logger_start_policy)) {
		hddLog(LOGE, FL("Invalid attribute"));
		return -EINVAL;
	}

	/* Parse and fetch ring id */
	if (!tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_RING_ID]) {
		hddLog(LOGE, FL("attr ATTR failed"));
		return -EINVAL;
	}
	start_log.ring_id = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_RING_ID]);
	hddLog(LOGE, FL("Ring ID=%d"), start_log.ring_id);

	/* Parse and fetch verbose level */
	if (!tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_VERBOSE_LEVEL]) {
		hddLog(LOGE, FL("attr verbose_level failed"));
		return -EINVAL;
	}
	start_log.verbose_level = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_VERBOSE_LEVEL]);
	hddLog(LOGE, FL("verbose_level=%d"), start_log.verbose_level);

	/* Parse and fetch flag */
	if (!tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_FLAGS]) {
		hddLog(LOGE, FL("attr flag failed"));
		return -EINVAL;
	}
	start_log.flag = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_FLAGS]);
	hddLog(LOGE, FL("flag=%d"), start_log.flag);

	vos_set_ring_log_level(start_log.ring_id, start_log.verbose_level);

	if (start_log.ring_id == RING_ID_WAKELOCK) {
		/* Start/stop wakelock events */
		if (start_log.verbose_level > WLAN_LOG_LEVEL_OFF)
			vos_set_wakelock_logging(true);
		else
			vos_set_wakelock_logging(false);
		return 0;
	}

	status = sme_wifi_start_logger(hdd_ctx->hHal, start_log);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE, FL("sme_wifi_start_logger failed(err=%d)"),
			status);
		return -EINVAL;
	}
	return 0;
}

/**
 * wlan_hdd_cfg80211_wifi_logger_start() - Wrapper function used to enable
 * or disable the collection of packet statistics from the firmware
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function is used to enable or disable the collection of packet
 * statistics from the firmware
 *
 * Return: 0 on success and errno on failure
 */
static int wlan_hdd_cfg80211_wifi_logger_start(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_wifi_logger_start(wiphy,
			wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

static const struct
nla_policy
qca_wlan_vendor_wifi_logger_get_ring_data_policy
[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_ID]
		= {.type = NLA_U32 },
};

/**
 * __wlan_hdd_cfg80211_wifi_logger_get_ring_data() - Flush per packet stats
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function is used to flush or retrieve the per packet statistics from
 * the driver
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_wifi_logger_get_ring_data(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	eHalStatus status;
	VOS_STATUS ret;
	uint32_t ring_id;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb
		    [QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_MAX + 1];

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_MAX,
			data, data_len,
			qca_wlan_vendor_wifi_logger_get_ring_data_policy)) {
		hddLog(LOGE, FL("Invalid attribute"));
		return -EINVAL;
	}

	/* Parse and fetch ring id */
	if (!tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_ID]) {
		hddLog(LOGE, FL("attr ATTR failed"));
		return -EINVAL;
	}

	ring_id = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_WIFI_LOGGER_GET_RING_DATA_ID]);

	if (ring_id == RING_ID_PER_PACKET_STATS) {
		wlan_logging_set_per_pkt_stats();
		hddLog(LOG1, FL("Flushing/Retrieving packet stats"));
	}

	hddLog(LOG1, FL("Bug report triggered by framework"));

	ret = vos_flush_logs(WLAN_LOG_TYPE_NON_FATAL,
			     WLAN_LOG_INDICATOR_FRAMEWORK,
			     WLAN_LOG_REASON_CODE_UNUSED);
	if (VOS_STATUS_SUCCESS != ret) {
		hddLog(LOGE, FL("Failed to trigger bug report"));
		return -EINVAL;
	}

	return 0;
}

/**
 * wlan_hdd_cfg80211_wifi_logger_get_ring_data() - Wrapper to flush packet stats
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function is used to flush or retrieve the per packet statistics from
 * the driver
 *
 * Return: 0 on success and errno on failure
 */
static int wlan_hdd_cfg80211_wifi_logger_get_ring_data(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_wifi_logger_get_ring_data(wiphy,
			wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

#ifdef WLAN_FEATURE_OFFLOAD_PACKETS
/**
 * hdd_map_req_id_to_pattern_id() - map request id to pattern id
 * @hdd_ctx: HDD context
 * @request_id: [input] request id
 * @pattern_id: [output] pattern id
 *
 * This function loops through request id to pattern id array
 * if the slot is available, store the request id and return pattern id
 * if entry exists, return the pattern id
 *
 * Return: 0 on success and errno on failure
 */
static int hdd_map_req_id_to_pattern_id(hdd_context_t *hdd_ctx,
					  uint32_t request_id,
					  uint8_t *pattern_id)
{
	uint32_t i;

	mutex_lock(&hdd_ctx->op_ctx.op_lock);
	for (i = 0; i < MAXNUM_PERIODIC_TX_PTRNS; i++) {
		if (hdd_ctx->op_ctx.op_table[i].request_id == MAX_REQUEST_ID) {
			hdd_ctx->op_ctx.op_table[i].request_id = request_id;
			*pattern_id = hdd_ctx->op_ctx.op_table[i].pattern_id;
			mutex_unlock(&hdd_ctx->op_ctx.op_lock);
			return 0;
		} else if (hdd_ctx->op_ctx.op_table[i].request_id ==
					request_id) {
			*pattern_id = hdd_ctx->op_ctx.op_table[i].pattern_id;
			mutex_unlock(&hdd_ctx->op_ctx.op_lock);
			return 0;
		}
	}
	mutex_unlock(&hdd_ctx->op_ctx.op_lock);
	return -EINVAL;
}

/**
 * hdd_unmap_req_id_to_pattern_id() - unmap request id to pattern id
 * @hdd_ctx: HDD context
 * @request_id: [input] request id
 * @pattern_id: [output] pattern id
 *
 * This function loops through request id to pattern id array
 * reset request id to 0 (slot available again) and
 * return pattern id
 *
 * Return: 0 on success and errno on failure
 */
static int hdd_unmap_req_id_to_pattern_id(hdd_context_t *hdd_ctx,
					  uint32_t request_id,
					  uint8_t *pattern_id)
{
	uint32_t i;

	mutex_lock(&hdd_ctx->op_ctx.op_lock);
	for (i = 0; i < MAXNUM_PERIODIC_TX_PTRNS; i++) {
		if (hdd_ctx->op_ctx.op_table[i].request_id == request_id) {
			hdd_ctx->op_ctx.op_table[i].request_id = MAX_REQUEST_ID;
			*pattern_id = hdd_ctx->op_ctx.op_table[i].pattern_id;
			mutex_unlock(&hdd_ctx->op_ctx.op_lock);
			return 0;
		}
	}
	mutex_unlock(&hdd_ctx->op_ctx.op_lock);
	return -EINVAL;
}


/*
 * define short names for the global vendor params
 * used by __wlan_hdd_cfg80211_offloaded_packets()
 */
#define PARAM_MAX QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_MAX
#define PARAM_REQUEST_ID \
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_REQUEST_ID
#define PARAM_CONTROL \
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_SENDING_CONTROL
#define PARAM_IP_PACKET \
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_IP_PACKET_DATA
#define PARAM_SRC_MAC_ADDR \
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_SRC_MAC_ADDR
#define PARAM_DST_MAC_ADDR \
		QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_DST_MAC_ADDR
#define PARAM_PERIOD QCA_WLAN_VENDOR_ATTR_OFFLOADED_PACKETS_PERIOD

/**
 * wlan_hdd_add_tx_ptrn() - add tx pattern
 * @adapter: adapter pointer
 * @hdd_ctx: hdd context
 * @tb: nl attributes
 *
 * This function reads the NL attributes and forms a AddTxPtrn message
 * posts it to SME.
 *
 */
static int
wlan_hdd_add_tx_ptrn(hdd_adapter_t *adapter, hdd_context_t *hdd_ctx,
			struct nlattr **tb)
{
	struct sSirAddPeriodicTxPtrn *add_req;
	eHalStatus status;
	uint32_t request_id, ret, len;
	uint8_t pattern_id = 0;
	v_MACADDR_t dst_addr;
	uint16_t eth_type = htons(ETH_P_IP);

	if (!hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
		hddLog(LOGE, FL("Not in Connected state!"));
		return -ENOTSUPP;
	}

	add_req = vos_mem_malloc(sizeof(*add_req));
	if (!add_req) {
		hddLog(LOGE, FL("memory allocation failed"));
		return -ENOMEM;
	}

	/* Parse and fetch request Id */
	if (!tb[PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		goto fail;
	}

	request_id = nla_get_u32(tb[PARAM_REQUEST_ID]);
	if (request_id == MAX_REQUEST_ID) {
		hddLog(LOGE, FL("request_id cannot be MAX"));
		return -EINVAL;
	}

	hddLog(LOG1, FL("Request Id: %u"), request_id);

	if (!tb[PARAM_PERIOD]) {
		hddLog(LOGE, FL("attr period failed"));
		goto fail;
	}
	add_req->usPtrnIntervalMs = nla_get_u32(tb[PARAM_PERIOD]);
	hddLog(LOG1, FL("Period: %u ms"), add_req->usPtrnIntervalMs);
	if (add_req->usPtrnIntervalMs == 0) {
		hddLog(LOGE, FL("Invalid interval zero, return failure"));
		goto fail;
	}

	if (!tb[PARAM_SRC_MAC_ADDR]) {
		hddLog(LOGE, FL("attr source mac address failed"));
		goto fail;
	}
	nla_memcpy(add_req->macAddress, tb[PARAM_SRC_MAC_ADDR],
			VOS_MAC_ADDR_SIZE);
	hddLog(LOG1, "input src mac address: "MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(add_req->macAddress));

	if (memcmp(add_req->macAddress, adapter->macAddressCurrent.bytes,
		VOS_MAC_ADDR_SIZE)) {
		hddLog(LOGE, FL("input src mac address and connected ap bssid are different"));
		goto fail;
	}

	if (!tb[PARAM_DST_MAC_ADDR]) {
		hddLog(LOGE, FL("attr dst mac address failed"));
		goto fail;
	}
	nla_memcpy(dst_addr.bytes, tb[PARAM_DST_MAC_ADDR], VOS_MAC_ADDR_SIZE);
	hddLog(LOG1, "input dst mac address: "MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(dst_addr.bytes));

	if (!tb[PARAM_IP_PACKET]) {
		hddLog(LOGE, FL("attr ip packet failed"));
		goto fail;
	}
	add_req->ucPtrnSize = nla_len(tb[PARAM_IP_PACKET]);
	hddLog(LOG1, FL("IP packet len: %u"), add_req->ucPtrnSize);

	if (add_req->ucPtrnSize < 0 ||
		add_req->ucPtrnSize > (PERIODIC_TX_PTRN_MAX_SIZE -
					HDD_ETH_HEADER_LEN)) {
		hddLog(LOGE, FL("Invalid IP packet len: %d"),
				add_req->ucPtrnSize);
		goto fail;
	}

	len = 0;
	vos_mem_copy(&add_req->ucPattern[0], dst_addr.bytes, VOS_MAC_ADDR_SIZE);
	len += VOS_MAC_ADDR_SIZE;
	vos_mem_copy(&add_req->ucPattern[len], add_req->macAddress,
			VOS_MAC_ADDR_SIZE);
	len += VOS_MAC_ADDR_SIZE;
	vos_mem_copy(&add_req->ucPattern[len], &eth_type, 2);
	len += 2;

	/*
	 * This is the IP packet, add 14 bytes Ethernet (802.3) header
	 * ------------------------------------------------------------
	 * | 14 bytes Ethernet (802.3) header | IP header and payload |
	 * ------------------------------------------------------------
	 */
	vos_mem_copy(&add_req->ucPattern[len],
			nla_data(tb[PARAM_IP_PACKET]),
			add_req->ucPtrnSize);
	add_req->ucPtrnSize += len;

	ret = hdd_map_req_id_to_pattern_id(hdd_ctx, request_id, &pattern_id);
	if (ret) {
		hddLog(LOGW, FL("req id to pattern id failed (ret=%d)"), ret);
		goto fail;
	}
	add_req->ucPtrnId = pattern_id;
	hddLog(LOG1, FL("pattern id: %d"), add_req->ucPtrnId);

	status = sme_AddPeriodicTxPtrn(hdd_ctx->hHal, add_req);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE,
			FL("sme_AddPeriodicTxPtrn failed (err=%d)"), status);
		goto fail;
	}

	EXIT();
	vos_mem_free(add_req);
	return 0;

fail:
	vos_mem_free(add_req);
	return -EINVAL;
}

/**
 * wlan_hdd_del_tx_ptrn() - delete tx pattern
 * @adapter: adapter pointer
 * @hdd_ctx: hdd context
 * @tb: nl attributes
 *
 * This function reads the NL attributes and forms a DelTxPtrn message
 * posts it to SME.
 *
 */
static int
wlan_hdd_del_tx_ptrn(hdd_adapter_t *adapter, hdd_context_t *hdd_ctx,
			struct nlattr **tb)
{
	struct sSirDelPeriodicTxPtrn *del_req;
	eHalStatus status;
	uint32_t request_id, ret;
	uint8_t pattern_id = 0;

	/* Parse and fetch request Id */
	if (!tb[PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		return -EINVAL;
	}

	request_id = nla_get_u32(tb[PARAM_REQUEST_ID]);
	if (request_id == MAX_REQUEST_ID) {
		hddLog(LOGE, FL("request_id cannot be MAX"));
		return -EINVAL;
	}

	ret = hdd_unmap_req_id_to_pattern_id(hdd_ctx, request_id, &pattern_id);
	if (ret) {
		hddLog(LOGW, FL("req id to pattern id failed (ret=%d)"), ret);
		return -EINVAL;
	}

	del_req = vos_mem_malloc(sizeof(*del_req));
	if (!del_req) {
		hddLog(LOGE, FL("memory allocation failed"));
		return -ENOMEM;
	}

	vos_mem_copy(del_req->macAddress, adapter->macAddressCurrent.bytes,
			VOS_MAC_ADDR_SIZE);
	hddLog(LOG1, MAC_ADDRESS_STR, MAC_ADDR_ARRAY(del_req->macAddress));
	del_req->ucPtrnId = pattern_id;
	hddLog(LOG1, FL("Request Id: %u Pattern id: %d"),
			 request_id, del_req->ucPtrnId);

	status = sme_DelPeriodicTxPtrn(hdd_ctx->hHal, del_req);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE,
			FL("sme_DelPeriodicTxPtrn failed (err=%d)"), status);
		goto fail;
	}

	EXIT();
	vos_mem_free(del_req);
	return 0;

fail:
	vos_mem_free(del_req);
	return -EINVAL;
}


/**
 * __wlan_hdd_cfg80211_offloaded_packets() - send offloaded packets
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_offloaded_packets(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[PARAM_MAX + 1];
	uint8_t control;
	int ret;
	static const struct nla_policy policy[PARAM_MAX + 1] = {
			[PARAM_REQUEST_ID] = { .type = NLA_U32 },
			[PARAM_CONTROL] = { .type = NLA_U32 },
			[PARAM_SRC_MAC_ADDR] = { .type = NLA_BINARY,
						.len = VOS_MAC_ADDR_SIZE },
			[PARAM_DST_MAC_ADDR] = { .type = NLA_BINARY,
						.len = VOS_MAC_ADDR_SIZE },
			[PARAM_PERIOD] = { .type = NLA_U32 },
	};

	ENTER();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return ret;
	}

	if (!sme_IsFeatureSupportedByFW(WLAN_PERIODIC_TX_PTRN)) {
		hddLog(LOGE,
			FL("Periodic Tx Pattern Offload feature is not supported in FW!"));
		return -ENOTSUPP;
	}

	if (nla_parse(tb, PARAM_MAX, data, data_len, policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	if (!tb[PARAM_CONTROL]) {
		hddLog(LOGE, FL("attr control failed"));
		return -EINVAL;
	}
	control = nla_get_u32(tb[PARAM_CONTROL]);
	hddLog(LOG1, FL("Control: %d"), control);

	if (control == WLAN_START_OFFLOADED_PACKETS)
		return wlan_hdd_add_tx_ptrn(adapter, hdd_ctx, tb);
	else if (control == WLAN_STOP_OFFLOADED_PACKETS)
		return wlan_hdd_del_tx_ptrn(adapter, hdd_ctx, tb);
	else {
		hddLog(LOGE, FL("Invalid control: %d"), control);
		return -EINVAL;
	}
}

/*
 * done with short names for the global vendor params
 * used by __wlan_hdd_cfg80211_offloaded_packets()
 */
#undef PARAM_MAX
#undef PARAM_REQUEST_ID
#undef PARAM_CONTROL
#undef PARAM_IP_PACKET
#undef PARAM_SRC_MAC_ADDR
#undef PARAM_DST_MAC_ADDR
#undef PARAM_PERIOD

/**
 * wlan_hdd_cfg80211_offloaded_packets() - Wrapper to offload packets
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */
static int wlan_hdd_cfg80211_offloaded_packets(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_offloaded_packets(wiphy,
					wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}
#endif

/*
 * define short names for the global vendor params
 * used by __wlan_hdd_cfg80211_monitor_rssi()
 */
#define PARAM_MAX QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX
#define PARAM_REQUEST_ID QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_REQUEST_ID
#define PARAM_CONTROL QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CONTROL
#define PARAM_MIN_RSSI QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MIN_RSSI
#define PARAM_MAX_RSSI QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX_RSSI

/**
 * __wlan_hdd_cfg80211_monitor_rssi() - monitor rssi
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_monitor_rssi(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data,
				 int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[PARAM_MAX + 1];
	struct rssi_monitor_req req;
	eHalStatus status;
	int ret;
	uint32_t control;
	static const struct nla_policy policy[PARAM_MAX + 1] = {
			[PARAM_REQUEST_ID] = { .type = NLA_U32 },
			[PARAM_CONTROL] = { .type = NLA_U32 },
			[PARAM_MIN_RSSI] = { .type = NLA_S8 },
			[PARAM_MAX_RSSI] = { .type = NLA_S8 },
	};

	ENTER();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret) {
		hddLog(LOGE, FL("HDD context is not valid"));
		return -EINVAL;
	}

	if (!hdd_connIsConnected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
		hddLog(LOGE, FL("Not in Connected state!"));
		return -ENOTSUPP;
	}

	if (nla_parse(tb, PARAM_MAX, data, data_len, policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	if (!tb[PARAM_REQUEST_ID]) {
		hddLog(LOGE, FL("attr request id failed"));
		return -EINVAL;
	}

	if (!tb[PARAM_CONTROL]) {
		hddLog(LOGE, FL("attr control failed"));
		return -EINVAL;
	}

	req.request_id = nla_get_u32(tb[PARAM_REQUEST_ID]);
	req.session_id = adapter->sessionId;
	control = nla_get_u32(tb[PARAM_CONTROL]);

	if (control == QCA_WLAN_RSSI_MONITORING_START) {
		req.control = true;
		if (!tb[PARAM_MIN_RSSI]) {
			hddLog(LOGE, FL("attr min rssi failed"));
			return -EINVAL;
		}

		if (!tb[PARAM_MAX_RSSI]) {
			hddLog(LOGE, FL("attr max rssi failed"));
			return -EINVAL;
		}

		req.min_rssi = nla_get_s8(tb[PARAM_MIN_RSSI]);
		req.max_rssi = nla_get_s8(tb[PARAM_MAX_RSSI]);

		if (!(req.min_rssi < req.max_rssi)) {
			hddLog(LOGW, FL("min_rssi: %d must be less than max_rssi: %d"),
					req.min_rssi, req.max_rssi);
			return -EINVAL;
		}
		hddLog(LOG1, FL("Min_rssi: %d Max_rssi: %d"),
			req.min_rssi, req.max_rssi);

	} else if (control == QCA_WLAN_RSSI_MONITORING_STOP)
		req.control = false;
	else {
		hddLog(LOGE, FL("Invalid control cmd: %d"), control);
		return -EINVAL;
	}
	hddLog(LOG1, FL("Request Id: %u Session_id: %d Control: %d"),
			req.request_id, req.session_id, req.control);

	status = sme_set_rssi_monitoring(hdd_ctx->hHal, &req);
	if (!HAL_STATUS_SUCCESS(status)) {
		hddLog(LOGE,
			FL("sme_set_rssi_monitoring failed(err=%d)"), status);
		return -EINVAL;
	}

	return 0;
}

/*
 * done with short names for the global vendor params
 * used by __wlan_hdd_cfg80211_monitor_rssi()
 */
#undef PARAM_MAX
#undef PARAM_CONTROL
#undef PARAM_REQUEST_ID
#undef PARAM_MAX_RSSI
#undef PARAM_MIN_RSSI

/**
 * wlan_hdd_cfg80211_monitor_rssi() - SSR wrapper to rssi monitoring
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */
static int
wlan_hdd_cfg80211_monitor_rssi(struct wiphy *wiphy, struct wireless_dev *wdev,
			       const void *data, int data_len)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_monitor_rssi(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

/**
 * hdd_rssi_threshold_breached() - rssi breached NL event
 * @hddctx: HDD context
 * @data: rssi breached event data
 *
 * This function reads the rssi breached event %data and fill in the skb with
 * NL attributes and send up the NL event.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
void hdd_rssi_threshold_breached(void *hddctx,
				 struct rssi_breach_event *data)
{
	hdd_context_t *hdd_ctx  = hddctx;
	struct sk_buff *skb;
	int flags = vos_get_gfp_flags();

	ENTER();

	if (wlan_hdd_validate_context(hdd_ctx) || !data) {
		hddLog(LOGE, FL("HDD context is invalid or data(%p) is null"),
			data);
		return;
	}

	skb = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
				  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
				  QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI_INDEX,
				  flags);

	if (!skb) {
		hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
		return;
	}

	hddLog(LOG1, "Req Id: %u Current rssi: %d",
			data->request_id, data->curr_rssi);
	hddLog(LOG1, "Current BSSID: "MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(data->curr_bssid.bytes));

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_REQUEST_ID,
		data->request_id) ||
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CUR_BSSID,
		sizeof(data->curr_bssid), data->curr_bssid.bytes) ||
	    nla_put_s8(skb, QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CUR_RSSI,
		data->curr_rssi)) {
		hddLog(LOGE, FL("nla put fail"));
		goto fail;
	}

	cfg80211_vendor_event(skb, flags);
	return;

fail:
	kfree_skb(skb);
	return;
}

/**
 * __wlan_hdd_cfg80211_setband() - set band
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_setband(struct wiphy *wiphy,
			    struct wireless_dev *wdev,
			    const void *data, int data_len)
{
	struct net_device *dev = wdev->netdev;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_MAX + 1];
	int ret;
	static const struct nla_policy policy[QCA_WLAN_VENDOR_ATTR_MAX + 1]
		= {[QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE] = { .type = NLA_U32 } };

	ENTER();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_MAX, data, data_len, policy)) {
		hddLog(LOGE, FL("Invalid ATTR"));
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE]) {
		hddLog(LOGE, FL("attr SETBAND_VALUE failed"));
		return -EINVAL;
	}

	return hdd_setBand(dev,
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_SETBAND_VALUE]));
}

/**
 * wlan_hdd_cfg80211_setband() - Wrapper to setband
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */
static int wlan_hdd_cfg80211_setband(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data, int data_len)
{
	int ret = 0;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_setband(wiphy, wdev, data, data_len);
	vos_ssr_unprotect(__func__);

	return ret;
}

const struct wiphy_vendor_command hdd_wiphy_vendor_commands[] =
{
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)is_driver_dfs_capable
    },

#ifdef WLAN_FEATURE_NAN
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_NAN,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_nan_request
    },
#endif

#ifdef WLAN_FEATURE_STATS_EXT
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_STATS_EXT,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_stats_ext_request
    },
#endif
#ifdef FEATURE_WLAN_EXTSCAN
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_START,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_extscan_start
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_STOP,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_extscan_stop
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_VALID_CHANNELS,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = (void *)wlan_hdd_cfg80211_extscan_get_valid_channels
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CAPABILITIES,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_extscan_get_capabilities
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_GET_CACHED_RESULTS,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_extscan_get_cached_results
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_BSSID_HOTLIST,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_extscan_set_bssid_hotlist
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_BSSID_HOTLIST,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_extscan_reset_bssid_hotlist
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SIGNIFICANT_CHANGE,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_extscan_set_significant_change
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SIGNIFICANT_CHANGE,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_extscan_reset_significant_change
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_SET_LIST,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_set_epno_list
    },
#endif /* FEATURE_WLAN_EXTSCAN */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_ll_stats_clear
    },

    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_ll_stats_set
    },

    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_ll_stats_get
    },
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */
/* EXT TDLS */
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_ENABLE,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_exttdls_enable
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_DISABLE,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = (void *)wlan_hdd_cfg80211_exttdls_disable
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_STATUS,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = (void *)wlan_hdd_cfg80211_exttdls_get_status
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_SUPPORTED_FEATURES,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = (void *)wlan_hdd_cfg80211_get_supported_features
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SCANNING_MAC_OUI,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = (void *)wlan_hdd_cfg80211_set_scanning_mac_oui
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_NO_DFS_FLAG,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = (void *)wlan_hdd_cfg80211_disable_dfs_chan_scan
     },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_CONCURRENCY_MATRIX,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = (void *)wlan_hdd_cfg80211_get_concurrency_matrix
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_wifi_configuration_set
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_ROAM,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_hdd_cfg80211_set_ext_roam_params
    },
#ifdef FEATURE_WLAN_EXTSCAN
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_SET_PASSPOINT_LIST,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_set_passpoint_list
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_RESET_PASSPOINT_LIST,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_reset_passpoint_list
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SET_SSID_HOTLIST,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_extscan_set_ssid_hotlist
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_RESET_SSID_HOTLIST,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_extscan_reset_ssid_hotlist
    },
#endif /* FEATURE_WLAN_EXTSCAN */
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_WIFI_INFO,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV,
        .doit = wlan_hdd_cfg80211_get_wifi_info
    },
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_LOGGER_FEATURE_SET,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_get_logger_supp_feature
    },

#ifdef WLAN_FEATURE_MEMDUMP
    {
        .info.vendor_id = QCA_NL80211_VENDOR_ID,
        .info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_MEMORY_DUMP,
        .flags = WIPHY_VENDOR_CMD_NEED_WDEV |
                 WIPHY_VENDOR_CMD_NEED_NETDEV |
                 WIPHY_VENDOR_CMD_NEED_RUNNING,
        .doit = wlan_hdd_cfg80211_get_fw_mem_dump
    },
#endif /* WLAN_FEATURE_MEMDUMP */
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_WIFI_LOGGER_START,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_wifi_logger_start
	},
#ifdef FEATURE_WLAN_TDLS
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_TDLS_GET_CAPABILITIES,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_get_tdls_capabilities
	},
#endif
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_RING_DATA,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV,
		.doit = wlan_hdd_cfg80211_wifi_logger_get_ring_data
	},
#ifdef WLAN_FEATURE_OFFLOAD_PACKETS
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OFFLOADED_PACKETS,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_offloaded_packets
	},
#endif
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			WIPHY_VENDOR_CMD_NEED_NETDEV |
			WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_monitor_rssi
	},
	{
		.info.vendor_id = QCA_NL80211_VENDOR_ID,
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SETBAND,
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |
			 WIPHY_VENDOR_CMD_NEED_NETDEV |
			 WIPHY_VENDOR_CMD_NEED_RUNNING,
		.doit = wlan_hdd_cfg80211_setband
	},
};


/*
 * FUNCTION: wlan_hdd_cfg80211_wiphy_alloc
 * This function is called by hdd_wlan_startup()
 * during initialization.
 * This function is used to allocate wiphy structure.
 */
struct wiphy *wlan_hdd_cfg80211_wiphy_alloc(int priv_size)
{
    struct wiphy *wiphy;
    ENTER();

    /*
     *   Create wiphy device
     */
    wiphy = wiphy_new(&wlan_hdd_cfg80211_ops, priv_size);

    if (!wiphy)
    {
        /* Print error and jump into err label and free the memory */
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: wiphy init failed", __func__);
        return NULL;
    }

    return wiphy;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_update_band
 * This function is called from the supplicant through a
 * private ioctl to change the band value
 */
int wlan_hdd_cfg80211_update_band(struct wiphy *wiphy, eCsrBand eBand)
{
    int i, j;
    eNVChannelEnabledType channelEnabledState;

    ENTER();

    for (i = 0; i < IEEE80211_NUM_BANDS; i++)
    {

        if (NULL == wiphy->bands[i])
        {
           hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy->bands[i] is NULL, i = %d",
                  __func__, i);
           continue;
        }

        for (j = 0; j < wiphy->bands[i]->n_channels; j++)
        {
            struct ieee80211_supported_band *band = wiphy->bands[i];

            channelEnabledState = vos_nv_getChannelEnabledState(
                                  band->channels[j].hw_value);

            if (IEEE80211_BAND_2GHZ == i && eCSR_BAND_5G == eBand) // 5G only
            {
#ifdef WLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY
                // Enable Social channels for P2P
                if (WLAN_HDD_IS_SOCIAL_CHANNEL(band->channels[j].center_freq) &&
                    NV_CHANNEL_ENABLE == channelEnabledState)
                    band->channels[j].flags &= ~IEEE80211_CHAN_DISABLED;
                else
#endif
                    band->channels[j].flags |= IEEE80211_CHAN_DISABLED;
                continue;
            }
            else if (IEEE80211_BAND_5GHZ == i && eCSR_BAND_24 == eBand) // 2G only
            {
                band->channels[j].flags |= IEEE80211_CHAN_DISABLED;
                continue;
            }

            if (NV_CHANNEL_DISABLE == channelEnabledState ||
                NV_CHANNEL_INVALID == channelEnabledState)
            {
                band->channels[j].flags |= IEEE80211_CHAN_DISABLED;
            }
            else if (NV_CHANNEL_DFS == channelEnabledState)
            {
                band->channels[j].flags &= ~IEEE80211_CHAN_DISABLED;
                band->channels[j].flags |= IEEE80211_CHAN_RADAR;
            }
            else
            {
                band->channels[j].flags &= ~(IEEE80211_CHAN_DISABLED
                                             |IEEE80211_CHAN_RADAR);
            }
        }
    }
    return 0;
}
/*
 * FUNCTION: wlan_hdd_cfg80211_init
 * This function is called by hdd_wlan_startup()
 * during initialization.
 * This function is used to initialize and register wiphy structure.
 */
int wlan_hdd_cfg80211_init(struct device *dev,
                               struct wiphy *wiphy,
                               hdd_config_t *pCfg
                               )
{
    int i, j;
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);

    ENTER();

    /* Now bind the underlying wlan device with wiphy */
    set_wiphy_dev(wiphy, dev);

    wiphy->mgmt_stypes = wlan_hdd_txrx_stypes;


    /* This will disable updating of NL channels from passive to
     * active if a beacon is received on passive channel. */
    wiphy->flags |=   WIPHY_FLAG_DISABLE_BEACON_HINTS;


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
    wiphy->flags |= WIPHY_FLAG_HAVE_AP_SME
                 |  WIPHY_FLAG_AP_PROBE_RESP_OFFLOAD
                 |  WIPHY_FLAG_HAS_REMAIN_ON_CHANNEL
#ifdef FEATURE_WLAN_STA_4ADDR_SCHEME
                 |  WIPHY_FLAG_4ADDR_STATION
#endif
                    | WIPHY_FLAG_OFFCHAN_TX;
    wiphy->country_ie_pref = NL80211_COUNTRY_IE_IGNORE_CORE;
#endif

    wiphy->wowlan.flags = WIPHY_WOWLAN_MAGIC_PKT;
    wiphy->wowlan.n_patterns = WOWL_MAX_PTRNS_ALLOWED;
    wiphy->wowlan.pattern_min_len = 1;
    wiphy->wowlan.pattern_max_len = WOWL_PTRN_MAX_SIZE;

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
    if (pCfg->isFastTransitionEnabled
#ifdef FEATURE_WLAN_LFR
       || pCfg->isFastRoamIniFeatureEnabled
#endif
#ifdef FEATURE_WLAN_ESE
       || pCfg->isEseIniFeatureEnabled
#endif
    )
    {
        wiphy->flags |= WIPHY_FLAG_SUPPORTS_FW_ROAM;
    }
#endif
#ifdef FEATURE_WLAN_TDLS
    wiphy->flags |= WIPHY_FLAG_SUPPORTS_TDLS
                 |  WIPHY_FLAG_TDLS_EXTERNAL_SETUP;
#endif

    wiphy->features |= NL80211_FEATURE_HT_IBSS;

#ifdef FEATURE_WLAN_SCAN_PNO
    if (pCfg->configPNOScanSupport)
    {
        wiphy->flags |= WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
        wiphy->max_sched_scan_ssids = SIR_PNO_MAX_SUPP_NETWORKS;
        wiphy->max_match_sets       = SIR_PNO_MAX_SUPP_NETWORKS;
        wiphy->max_sched_scan_ie_len = SIR_MAC_MAX_IE_LENGTH;
    }
#endif/*FEATURE_WLAN_SCAN_PNO*/

#if  defined QCA_WIFI_FTM
    if (vos_get_conparam() != VOS_FTM_MODE) {
#endif

    /* even with WIPHY_FLAG_CUSTOM_REGULATORY,
       driver can still register regulatory callback and
       it will get regulatory settings in wiphy->band[], but
       driver need to determine what to do with both
       regulatory settings */

    wiphy->reg_notifier = wlan_hdd_linux_reg_notifier;

#if  defined QCA_WIFI_FTM
    }
#endif

    wiphy->max_scan_ssids = MAX_SCAN_SSID;

    wiphy->max_scan_ie_len = SIR_MAC_MAX_ADD_IE_LENGTH;

    wiphy->max_acl_mac_addrs = MAX_ACL_MAC_ADDRESS;

    /* Supports STATION & AD-HOC modes right now */
    wiphy->interface_modes =   BIT(NL80211_IFTYPE_STATION)
                             | BIT(NL80211_IFTYPE_ADHOC)
                             | BIT(NL80211_IFTYPE_P2P_CLIENT)
                             | BIT(NL80211_IFTYPE_P2P_GO)
                             | BIT(NL80211_IFTYPE_AP);

    if( pCfg->advertiseConcurrentOperation )
    {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
       if( pCfg->enableMCC ) {
          int i;
          for (i = 0; i < ARRAY_SIZE(wlan_hdd_iface_combination); i++) {
             if( !pCfg->allowMCCGODiffBI )
                wlan_hdd_iface_combination[i].beacon_int_infra_match = true;
          }
       }
       wiphy->n_iface_combinations = ARRAY_SIZE(wlan_hdd_iface_combination);
       wiphy->iface_combinations = wlan_hdd_iface_combination;
#endif
    }

    /* Before registering we need to update the HT capability based
     * on ini values */
    if( !pCfg->ShortGI20MhzEnable )
    {
        wlan_hdd_band_2_4_GHZ.ht_cap.cap     &= ~IEEE80211_HT_CAP_SGI_20;
        wlan_hdd_band_5_GHZ.ht_cap.cap       &= ~IEEE80211_HT_CAP_SGI_20;
        wlan_hdd_band_p2p_2_4_GHZ.ht_cap.cap &= ~IEEE80211_HT_CAP_SGI_20;
    }

    if( !pCfg->ShortGI40MhzEnable )
    {
        wlan_hdd_band_5_GHZ.ht_cap.cap       &= ~IEEE80211_HT_CAP_SGI_40;
    }

    if( !pCfg->nChannelBondingMode5GHz )
    {
        wlan_hdd_band_5_GHZ.ht_cap.cap &= ~IEEE80211_HT_CAP_SUP_WIDTH_20_40;
    }

   wiphy->bands[IEEE80211_BAND_2GHZ] = &wlan_hdd_band_2_4_GHZ;
   if (true == hdd_is_5g_supported(pHddCtx))
   {
       wiphy->bands[IEEE80211_BAND_5GHZ] = &wlan_hdd_band_5_GHZ;
   }

   for (i = 0; i < IEEE80211_NUM_BANDS; i++)
   {

       if (NULL == wiphy->bands[i])
       {
          hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy->bands[i] is NULL, i = %d",
                 __func__, i);
          continue;
       }

       for (j = 0; j < wiphy->bands[i]->n_channels; j++)
       {
           struct ieee80211_supported_band *band = wiphy->bands[i];

           if (IEEE80211_BAND_2GHZ == i && eCSR_BAND_5G == pCfg->nBandCapability) // 5G only
           {
#ifdef WLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY
               // Enable social channels for P2P
               if (WLAN_HDD_IS_SOCIAL_CHANNEL(band->channels[j].center_freq))
                   band->channels[j].flags &= ~IEEE80211_CHAN_DISABLED;
               else
#endif
                   band->channels[j].flags |= IEEE80211_CHAN_DISABLED;
               continue;
           }
           else if (IEEE80211_BAND_5GHZ == i && eCSR_BAND_24 == pCfg->nBandCapability) // 2G only
           {
               band->channels[j].flags |= IEEE80211_CHAN_DISABLED;
               continue;
           }
       }
    }
    /*Initialise the supported cipher suite details*/
    wiphy->cipher_suites = hdd_cipher_suites;
    wiphy->n_cipher_suites = ARRAY_SIZE(hdd_cipher_suites);

    /*signal strength in mBm (100*dBm) */
    wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;
    wiphy->max_remain_on_channel_duration = 1000;
    wiphy->n_vendor_commands = ARRAY_SIZE(hdd_wiphy_vendor_commands);
    wiphy->vendor_commands = hdd_wiphy_vendor_commands;

    wiphy->vendor_events = wlan_hdd_cfg80211_vendor_events;
    wiphy->n_vendor_events = ARRAY_SIZE(wlan_hdd_cfg80211_vendor_events);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,4,0))
    if (pCfg->enableDFSMasterCap) {
        wiphy->flags |= WIPHY_FLAG_DFS_OFFLOAD;
    }
#endif

    wiphy->max_ap_assoc_sta = pCfg->maxNumberOfPeers;

#ifdef QCA_HT_2040_COEX
    if (pCfg->ht2040CoexEnabled)
        wiphy->features |= NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE;
#endif

#ifdef NL80211_KEY_LEN_PMK /* kernel supports key mgmt offload */
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    if (pCfg->isRoamOffloadEnabled) {
        wiphy->flags |= WIPHY_FLAG_HAS_KEY_MGMT_OFFLOAD;
        wiphy->key_mgmt_offload_support |=
                         NL80211_KEY_MGMT_OFFLOAD_SUPPORT_PSK;
        wiphy->key_mgmt_offload_support |=
                         NL80211_KEY_MGMT_OFFLOAD_SUPPORT_FT_PSK;
        wiphy->key_mgmt_offload_support |=
                         NL80211_KEY_MGMT_OFFLOAD_SUPPORT_PMKSA;
        wiphy->key_mgmt_offload_support |=
                         NL80211_KEY_MGMT_OFFLOAD_SUPPORT_FT_802_1X;
        wiphy->key_derive_offload_support |=
                         NL80211_KEY_DERIVE_OFFLOAD_SUPPORT_IGTK;
        wiphy->key_derive_offload_support |=
                         NL80211_KEY_DERIVE_OFFLOAD_SUPPORT_SHA256;
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_DEBUG,
            "%s: LFR3:Driver key mgmt offload capability flags %x",
                         __func__,wiphy->key_mgmt_offload_support);
    }
#endif
#endif

    EXIT();
    return 0;
}

/*
 * In this function, wiphy structure is updated after VOSS
 * initialization. In wlan_hdd_cfg80211_init, only the
 * default values will be initialized. The final initialization
 * of all required members can be done here.
 */
void wlan_hdd_update_wiphy(struct wiphy *wiphy,
                           hdd_config_t *pCfg)
{
    wiphy->max_ap_assoc_sta = pCfg->maxNumberOfPeers;
}

/* In this function we are registering wiphy. */
int wlan_hdd_cfg80211_register(struct wiphy *wiphy)
{
    ENTER();
 /* Register our wiphy dev with cfg80211 */
    if (0 > wiphy_register(wiphy))
    {
        /* print error */
        hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy register failed", __func__);
        return -EIO;
    }

    EXIT();
    return 0;
}

/*
  HDD function to update wiphy capability based on target offload status.

  wlan_hdd_cfg80211_init() does initialization of all wiphy related
  capability even before downloading firmware to the target. In discrete
  case, host will get know certain offload capability (say sched_scan
  caps) only after downloading firmware to the target and target boots up.
  This function is used to override setting done in wlan_hdd_cfg80211_init()
  based on target capability.
*/
void wlan_hdd_cfg80211_update_wiphy_caps(struct wiphy *wiphy)
{
#ifdef FEATURE_WLAN_SCAN_PNO
   hdd_context_t *pHddCtx = wiphy_priv(wiphy);
   hdd_config_t *pCfg = pHddCtx->cfg_ini;

   /* wlan_hdd_cfg80211_init() sets sched_scan caps already in wiphy before
    * control comes here. Here just we need to clear it if firmware doesn't
    * have PNO support. */
   if (!pCfg->PnoOffload) {
       wiphy->flags &= ~WIPHY_FLAG_SUPPORTS_SCHED_SCAN;
       wiphy->max_sched_scan_ssids = 0;
       wiphy->max_match_sets = 0;
       wiphy->max_sched_scan_ie_len = 0;
   }
#endif
}

/*
 * In this function we are updating channel list when,
 * regulatory domain is FCC and country code is US.
 * Here In FCC standard 5GHz UNII-1 Bands are indoor only.
 * As per FCC smart phone is not a indoor device.
 * GO should not operate on indoor channels.
 */
void wlan_hdd_cfg80211_update_reg_info(struct wiphy *wiphy)
{
    int j;
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    tANI_U8 defaultCountryCode[3] = SME_INVALID_COUNTRY_CODE;
    /* Default country code from NV at the time of wiphy initialization. */
    if (eHAL_STATUS_SUCCESS != sme_GetDefaultCountryCodeFrmNv(pHddCtx->hHal,
                                  &defaultCountryCode[0]))
    {
       hddLog(LOGE, FL("Failed to get default country code from NV"));
    }
    if ((defaultCountryCode[0]== 'U') && (defaultCountryCode[1]=='S'))
    {
       if (NULL == wiphy->bands[IEEE80211_BAND_5GHZ])
       {
          hddLog(VOS_TRACE_LEVEL_ERROR,"%s: wiphy->bands[IEEE80211_BAND_5GHZ] is NULL",__func__ );
          return;
       }
       for (j = 0; j < wiphy->bands[IEEE80211_BAND_5GHZ]->n_channels; j++)
       {
          struct ieee80211_supported_band *band = wiphy->bands[IEEE80211_BAND_5GHZ];
          // Mark UNII -1 band channel as passive
          if (WLAN_HDD_CHANNEL_IN_UNII_1_BAND(band->channels[j].center_freq))
             band->channels[j].flags |= IEEE80211_CHAN_PASSIVE_SCAN;
       }
    }
}

/* This function registers for all frame which supplicant is interested in */
void wlan_hdd_cfg80211_register_frames(hdd_adapter_t* pAdapter)
{
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    /* Register for all P2P action, public action etc frames */
    v_U16_t type = (SIR_MAC_MGMT_FRAME << 2) | ( SIR_MAC_MGMT_ACTION << 4);

    ENTER();

   /* Right now we are registering these frame when driver is getting
      initialized. Once we will move to 2.6.37 kernel, in which we have
      frame register ops, we will move this code as a part of that */
    /* GAS Initial Request */
    sme_RegisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)GAS_INITIAL_REQ, GAS_INITIAL_REQ_SIZE );

    /* GAS Initial Response */
    sme_RegisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)GAS_INITIAL_RSP, GAS_INITIAL_RSP_SIZE );

    /* GAS Comeback Request */
    sme_RegisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)GAS_COMEBACK_REQ, GAS_COMEBACK_REQ_SIZE );

    /* GAS Comeback Response */
    sme_RegisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)GAS_COMEBACK_RSP, GAS_COMEBACK_RSP_SIZE );

    /* P2P Public Action */
    sme_RegisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)P2P_PUBLIC_ACTION_FRAME,
                                  P2P_PUBLIC_ACTION_FRAME_SIZE );

    /* P2P Action */
    sme_RegisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)P2P_ACTION_FRAME,
                                  P2P_ACTION_FRAME_SIZE );

    /* WNM BSS Transition Request frame */
    sme_RegisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)WNM_BSS_ACTION_FRAME,
                                  WNM_BSS_ACTION_FRAME_SIZE );

    /* WNM-Notification */
    sme_RegisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)WNM_NOTIFICATION_FRAME,
                                  WNM_NOTIFICATION_FRAME_SIZE );
}

void wlan_hdd_cfg80211_deregister_frames(hdd_adapter_t* pAdapter)
{
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    /* Register for all P2P action, public action etc frames */
    v_U16_t type = (SIR_MAC_MGMT_FRAME << 2) | ( SIR_MAC_MGMT_ACTION << 4);

    ENTER();

   /* Right now we are registering these frame when driver is getting
      initialized. Once we will move to 2.6.37 kernel, in which we have
      frame register ops, we will move this code as a part of that */
    /* GAS Initial Request */

    sme_DeregisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                          (v_U8_t*)GAS_INITIAL_REQ, GAS_INITIAL_REQ_SIZE );

    /* GAS Initial Response */
    sme_DeregisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)GAS_INITIAL_RSP, GAS_INITIAL_RSP_SIZE );

    /* GAS Comeback Request */
    sme_DeregisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)GAS_COMEBACK_REQ, GAS_COMEBACK_REQ_SIZE );

    /* GAS Comeback Response */
    sme_DeregisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)GAS_COMEBACK_RSP, GAS_COMEBACK_RSP_SIZE );

    /* P2P Public Action */
    sme_DeregisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)P2P_PUBLIC_ACTION_FRAME,
                                  P2P_PUBLIC_ACTION_FRAME_SIZE );

    /* P2P Action */
    sme_DeregisterMgmtFrame(hHal, HDD_SESSION_ID_ANY, type,
                         (v_U8_t*)P2P_ACTION_FRAME,
                                  P2P_ACTION_FRAME_SIZE );

    /* WNM-Notification */
    sme_DeregisterMgmtFrame(hHal, pAdapter->sessionId, type,
                         (v_U8_t*)WNM_NOTIFICATION_FRAME,
                                  WNM_NOTIFICATION_FRAME_SIZE );
}

#ifdef FEATURE_WLAN_WAPI
void wlan_hdd_cfg80211_set_key_wapi(hdd_adapter_t* pAdapter, u8 key_index,
                                     const u8 *mac_addr, u8 *key , int key_Len)
{
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    tCsrRoamSetKey  setKey;
    v_BOOL_t isConnected = TRUE;
    int status = 0;
    v_U32_t roamId= 0xFF;
    tANI_U8 *pKeyPtr = NULL;
    int n = 0;

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %d",
                                        __func__,pAdapter->device_mode);

    vos_mem_zero(&setKey, sizeof(tCsrRoamSetKey));
    setKey.keyId = key_index; // Store Key ID
    setKey.encType  = eCSR_ENCRYPT_TYPE_WPI; // SET WAPI Encryption
    setKey.keyDirection = eSIR_TX_RX;  /* Key Direction both TX and RX */
    setKey.paeRole = 0 ; // the PAE role
    if (!mac_addr || is_broadcast_ether_addr(mac_addr))
    {
        vos_set_macaddr_broadcast( (v_MACADDR_t *)setKey.peerMac );
    }
    else
    {
        vos_mem_copy(setKey.peerMac, mac_addr, VOS_MAC_ADDR_SIZE);
    }
    setKey.keyLength = key_Len;
    pKeyPtr = setKey.Key;
    memcpy( pKeyPtr, key, key_Len);

    hddLog(VOS_TRACE_LEVEL_INFO,"%s: WAPI KEY LENGTH:0x%04x",
                                            __func__, key_Len);
    for (n = 0 ; n < key_Len; n++)
        hddLog(VOS_TRACE_LEVEL_INFO, "%s WAPI KEY Data[%d]:%02x ",
                                           __func__,n,setKey.Key[n]);

    pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_SETTING_KEY;
    if ( isConnected )
    {
        status= sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
                             pAdapter->sessionId, &setKey, &roamId );
    }
    if ( status != 0 )
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "[%4d] sme_RoamSetKey returned ERROR status= %d",
                                                __LINE__, status );
        pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
    }
}
#endif /* FEATURE_WLAN_WAPI*/

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
int wlan_hdd_cfg80211_alloc_new_beacon(hdd_adapter_t *pAdapter,
                                       beacon_data_t **ppBeacon,
                                       struct beacon_parameters *params)
#else
int wlan_hdd_cfg80211_alloc_new_beacon(hdd_adapter_t *pAdapter,
                                       beacon_data_t **ppBeacon,
                                       struct cfg80211_beacon_data *params,
                                       int dtim_period)
#endif
{
    int size;
    beacon_data_t *beacon = NULL;
    beacon_data_t *old = NULL;
    int head_len,tail_len;

    ENTER();
    if (params->head && !params->head_len)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("head_len is NULL"));
        return -EINVAL;
    }

    old = pAdapter->sessionCtx.ap.beacon;

    if (!params->head && !old)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("session(%d) old and new heads points to NULL"),
                     pAdapter->sessionId);
        return -EINVAL;
    }

    if(params->head)
        head_len = params->head_len;
    else
        head_len = old->head_len;

    if(params->tail || !old)
        tail_len = params->tail_len;
    else
        tail_len = old->tail_len;

    size = sizeof(beacon_data_t) + head_len + tail_len;

    beacon = kzalloc(size, GFP_KERNEL);

    if( beacon == NULL )
    {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 FL("Mem allocation for beacon failed"));
       return -ENOMEM;
    }

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
    if(params->dtim_period || !old )
        beacon->dtim_period = params->dtim_period;
    else
        beacon->dtim_period = old->dtim_period;
#else
    if(dtim_period || !old )
        beacon->dtim_period = dtim_period;
    else
        beacon->dtim_period = old->dtim_period;
#endif

    beacon->head = ((u8 *) beacon) + sizeof(beacon_data_t);
    beacon->tail = beacon->head + head_len;
    beacon->head_len = head_len;
    beacon->tail_len = tail_len;

    if(params->head) {
        memcpy (beacon->head,params->head,beacon->head_len);
    }
    else {
        if(old)
            memcpy (beacon->head,old->head,beacon->head_len);
    }

    if(params->tail) {
        memcpy (beacon->tail,params->tail,beacon->tail_len);
    }
    else {
       if(old)
           memcpy (beacon->tail,old->tail,beacon->tail_len);
    }

    *ppBeacon = beacon;

    kfree(old);

    return 0;

}

v_U8_t* wlan_hdd_cfg80211_get_ie_ptr(v_U8_t *pIes, int length, v_U8_t eid)
{
    int left = length;
    v_U8_t *ptr = pIes;
    v_U8_t elem_id,elem_len;

    while(left >= 2)
    {
        elem_id  =  ptr[0];
        elem_len =  ptr[1];
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
            return ptr;
        }

        left -= elem_len;
        ptr += (elem_len + 2);
    }
    return NULL;
}

/* Check if rate is 11g rate or not */
static int wlan_hdd_rate_is_11g(u8 rate)
{
    static const u8 gRateArray[8] = {12, 18, 24, 36, 48, 72, 96, 108}; /* actual rate * 2 */
    u8 i;
    for (i = 0; i < 8; i++)
    {
        if(rate == gRateArray[i])
            return TRUE;
    }
    return FALSE;
}

/* Check for 11g rate and set proper 11g only mode */
static void wlan_hdd_check_11gmode(u8 *pIe, u8* require_ht,
                     u8* pCheckRatesfor11g, eSapPhyMode* pSapHw_mode)
{
    u8 i, num_rates = pIe[0];

    pIe += 1;
    for ( i = 0; i < num_rates; i++)
    {
        if( *pCheckRatesfor11g && (TRUE == wlan_hdd_rate_is_11g(pIe[i] & RATE_MASK)))
        {
            /* If rate set have 11g rate than change the mode to 11G */
            *pSapHw_mode = eSAP_DOT11_MODE_11g;
            if (pIe[i] & BASIC_RATE_MASK)
            {
                /* If we have 11g rate as  basic rate, it means mode
                   is 11g only mode.
                 */
               *pSapHw_mode = eSAP_DOT11_MODE_11g_ONLY;
               *pCheckRatesfor11g = FALSE;
            }
        }
        else if((BASIC_RATE_MASK | WLAN_BSS_MEMBERSHIP_SELECTOR_HT_PHY) == pIe[i])
        {
            *require_ht = TRUE;
        }
    }
    return;
}

static void wlan_hdd_set_sapHwmode(hdd_adapter_t *pHostapdAdapter)
{
    tsap_Config_t *pConfig = &pHostapdAdapter->sessionCtx.ap.sapConfig;
    beacon_data_t *pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;
    struct ieee80211_mgmt *pMgmt_frame = (struct ieee80211_mgmt*)pBeacon->head;
    u8 checkRatesfor11g = TRUE;
    u8 require_ht = FALSE;
    u8 *pIe=NULL;

    pConfig->SapHw_mode= eSAP_DOT11_MODE_11b;

    pIe = wlan_hdd_cfg80211_get_ie_ptr(&pMgmt_frame->u.beacon.variable[0],
                                       pBeacon->head_len, WLAN_EID_SUPP_RATES);
    if (pIe != NULL)
    {
        pIe += 1;
        wlan_hdd_check_11gmode(pIe, &require_ht, &checkRatesfor11g,
                               &pConfig->SapHw_mode);
    }

    pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
                                WLAN_EID_EXT_SUPP_RATES);
    if (pIe != NULL)
    {

        pIe += 1;
        wlan_hdd_check_11gmode(pIe, &require_ht, &checkRatesfor11g,
                               &pConfig->SapHw_mode);
    }

    if( pConfig->channel > 14 )
    {
        pConfig->SapHw_mode= eSAP_DOT11_MODE_11a;
    }

    pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
                                       WLAN_EID_HT_CAPABILITY);

    if(pIe)
    {
        pConfig->SapHw_mode= eSAP_DOT11_MODE_11n;
        if(require_ht)
            pConfig->SapHw_mode= eSAP_DOT11_MODE_11n_ONLY;
    }
}

static int wlan_hdd_add_ie(hdd_adapter_t* pHostapdAdapter, v_U8_t *genie,
                              v_U8_t *total_ielen, v_U8_t *oui, v_U8_t oui_size)
{
    v_U16_t ielen = 0;
    v_U8_t *pIe = NULL;
    beacon_data_t *pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;

    pIe = wlan_hdd_get_vendor_oui_ie_ptr(oui, oui_size,
                                          pBeacon->tail, pBeacon->tail_len);

    if (pIe)
    {
        ielen = pIe[1] + 2;
        if ((*total_ielen + ielen) <= MAX_GENIE_LEN)
        {
            vos_mem_copy(&genie[*total_ielen], pIe, ielen);
        }
        else
        {
            hddLog( VOS_TRACE_LEVEL_ERROR, "**Ie Length is too big***");
            return -EINVAL;
        }
        *total_ielen += ielen;
    }
    return 0;
}

static void wlan_hdd_add_hostapd_conf_vsie(hdd_adapter_t* pHostapdAdapter,
                                           v_U8_t *genie, v_U8_t *total_ielen)
{
    beacon_data_t *pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;
    int left = pBeacon->tail_len;
    v_U8_t *ptr = pBeacon->tail;
    v_U8_t elem_id, elem_len;
    v_U16_t ielen = 0;

    if ( NULL == ptr || 0 == left )
        return;

    while (left >= 2)
    {
        elem_id  = ptr[0];
        elem_len = ptr[1];
        left -= 2;
        if (elem_len > left)
        {
            hddLog( VOS_TRACE_LEVEL_ERROR,
                    "****Invalid IEs eid = %d elem_len=%d left=%d*****",
                    elem_id, elem_len, left);
            return;
        }
        if (IE_EID_VENDOR == elem_id)
        {
            /* skipping the VSIE's which we don't want to include or
             * it will be included by existing code
             */
            if ((memcmp( &ptr[2], WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE) != 0 ) &&
#ifdef WLAN_FEATURE_WFD
               (memcmp( &ptr[2], WFD_OUI_TYPE, WFD_OUI_TYPE_SIZE) != 0) &&
#endif
               (memcmp( &ptr[2], WHITELIST_OUI_TYPE, WPA_OUI_TYPE_SIZE) != 0) &&
               (memcmp( &ptr[2], BLACKLIST_OUI_TYPE, WPA_OUI_TYPE_SIZE) != 0) &&
               (memcmp( &ptr[2], "\x00\x50\xf2\x02", WPA_OUI_TYPE_SIZE) != 0) &&
               (memcmp( &ptr[2], WPA_OUI_TYPE, WPA_OUI_TYPE_SIZE) != 0) &&
               (memcmp( &ptr[2], P2P_OUI_TYPE, P2P_OUI_TYPE_SIZE) != 0))
            {
               ielen = ptr[1] + 2;
               if ((*total_ielen + ielen) <= MAX_GENIE_LEN)
               {
                   vos_mem_copy(&genie[*total_ielen], ptr, ielen);
                   *total_ielen += ielen;
               }
               else
               {
                   hddLog( VOS_TRACE_LEVEL_ERROR,
                           "IE Length is too big "
                           "IEs eid=%d elem_len=%d total_ie_lent=%d",
                           elem_id, elem_len, *total_ielen);
               }
            }
        }

        left -= elem_len;
        ptr += (elem_len + 2);
    }
    return;
}

static void wlan_hdd_add_extra_ie(hdd_adapter_t* pHostapdAdapter,
                                           v_U8_t *genie, v_U8_t *total_ielen,
                                           v_U8_t temp_ie_id)
{
    beacon_data_t *pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;
    int left = pBeacon->tail_len;
    v_U8_t *ptr = pBeacon->tail;
    v_U8_t elem_id, elem_len;
    v_U16_t ielen = 0;

    if ( NULL == ptr || 0 == left )
        return;

    while (left >= 2)
    {
        elem_id  = ptr[0];
        elem_len = ptr[1];
        left -= 2;
        if (elem_len > left)
        {
            hddLog( VOS_TRACE_LEVEL_ERROR,
                    "****Invalid IEs eid = %d elem_len=%d left=%d*****",
                    elem_id, elem_len, left);
            return;
        }

        if (temp_ie_id == elem_id)
        {
            ielen = ptr[1] + 2;
            if ((*total_ielen + ielen) <= MAX_GENIE_LEN)
            {
                vos_mem_copy(&genie[*total_ielen], ptr, ielen);
                *total_ielen += ielen;
            }
            else
            {
                hddLog( VOS_TRACE_LEVEL_ERROR,
                           "IE Length is too big "
                           "IEs eid=%d elem_len=%d total_ie_lent=%d",
                           elem_id, elem_len, *total_ielen);
            }
        }

        left -= elem_len;
        ptr += (elem_len + 2);
    }
    return;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static int wlan_hdd_cfg80211_update_apies(hdd_adapter_t* pHostapdAdapter,
                            struct beacon_parameters *params)
#else
static int wlan_hdd_cfg80211_update_apies(hdd_adapter_t* pHostapdAdapter,
                                     struct cfg80211_beacon_data *params)
#endif
{
    v_U8_t *genie;
    v_U8_t total_ielen = 0;
    int ret = 0;
    tsap_Config_t *pConfig;
    tSirUpdateIE   updateIE;

    pConfig = &pHostapdAdapter->sessionCtx.ap.sapConfig;

    genie = vos_mem_malloc(MAX_GENIE_LEN);

    if(genie == NULL) {

        return -ENOMEM;
    }

    if (0 != wlan_hdd_add_ie(pHostapdAdapter, genie,
                              &total_ielen, WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE))
    {
        hddLog(LOGE, FL("Adding WPS IE failed"));
         ret = -EINVAL;
         goto done;
    }

#ifdef WLAN_FEATURE_WFD
    if (0 != wlan_hdd_add_ie(pHostapdAdapter, genie,
                              &total_ielen, WFD_OUI_TYPE, WFD_OUI_TYPE_SIZE))
    {
        hddLog(LOGE, FL("Adding WFD IE failed"));
         ret = -EINVAL;
         goto done;
    }
#endif

    if (0 != wlan_hdd_add_ie(pHostapdAdapter, genie,
                              &total_ielen, P2P_OUI_TYPE, P2P_OUI_TYPE_SIZE))
    {
       hddLog(LOGE, FL("Adding P2P IE failed"));
         ret = -EINVAL;
         goto done;
    }

#ifdef FEATURE_WLAN_WAPI
    if (WLAN_HDD_SOFTAP == pHostapdAdapter->device_mode)
    {
        wlan_hdd_add_extra_ie(pHostapdAdapter, genie, &total_ielen,
                WLAN_EID_WAPI);
    }
#endif

    if (WLAN_HDD_SOFTAP == pHostapdAdapter->device_mode)
    {
        wlan_hdd_add_hostapd_conf_vsie(pHostapdAdapter, genie, &total_ielen);
    }

#ifdef QCA_HT_2040_COEX
    if (WLAN_HDD_SOFTAP == pHostapdAdapter->device_mode) {
        tSmeConfigParams smeConfig;

        vos_mem_zero(&smeConfig, sizeof(smeConfig));
        sme_GetConfigParam(WLAN_HDD_GET_HAL_CTX(pHostapdAdapter), &smeConfig);
        if (smeConfig.csrConfig.obssEnabled)
            wlan_hdd_add_extra_ie(pHostapdAdapter, genie, &total_ielen,
                    WLAN_EID_OVERLAP_BSS_SCAN_PARAM);
    }
#endif

    vos_mem_copy(updateIE.bssid, pHostapdAdapter->macAddressCurrent.bytes,
                   sizeof(tSirMacAddr));
    updateIE.smeSessionId =  pHostapdAdapter->sessionId;

    if (test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) {
        updateIE.ieBufferlength = total_ielen;
        updateIE.pAdditionIEBuffer = genie;
        updateIE.append = VOS_FALSE;
        updateIE.notify = VOS_TRUE;
        if (sme_UpdateAddIE(WLAN_HDD_GET_HAL_CTX(pHostapdAdapter),
                &updateIE, eUPDATE_IE_PROBE_BCN) == eHAL_STATUS_FAILURE) {
            hddLog(LOGE, FL("Could not pass on Add Ie probe beacon data"));
            ret = -EINVAL;
            goto done;
        }
        WLANSAP_ResetSapConfigAddIE(pConfig , eUPDATE_IE_PROBE_BCN);
    } else {
        WLANSAP_UpdateSapConfigAddIE(pConfig,
                                     genie,
                                     total_ielen,
                                     eUPDATE_IE_PROBE_BCN);
    }

    /* Added for Probe Response IE */
    if (test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) {
        updateIE.ieBufferlength = params->proberesp_ies_len;
        updateIE.pAdditionIEBuffer = (tANI_U8*)params->proberesp_ies;
        updateIE.append = VOS_FALSE;
        updateIE.notify = VOS_FALSE;
        if (sme_UpdateAddIE(WLAN_HDD_GET_HAL_CTX(pHostapdAdapter),
                  &updateIE, eUPDATE_IE_PROBE_RESP) == eHAL_STATUS_FAILURE) {
            hddLog(LOGE, FL("Could not pass on PROBE_RESP add Ie data"));
            ret = -EINVAL;
            goto done;
        }
        WLANSAP_ResetSapConfigAddIE(pConfig, eUPDATE_IE_PROBE_RESP);
    } else {
        WLANSAP_UpdateSapConfigAddIE(pConfig,
                             params->proberesp_ies,
                             params->proberesp_ies_len,
                             eUPDATE_IE_PROBE_RESP);
    }

    /* Assoc resp Add ie Data */
    if (test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) {
        updateIE.ieBufferlength = params->assocresp_ies_len;
        updateIE.pAdditionIEBuffer = (tANI_U8*)params->assocresp_ies;
        updateIE.append = VOS_FALSE;
        updateIE.notify = VOS_FALSE;
        if (sme_UpdateAddIE(WLAN_HDD_GET_HAL_CTX(pHostapdAdapter),
                &updateIE, eUPDATE_IE_ASSOC_RESP) == eHAL_STATUS_FAILURE) {
            hddLog(LOGE, FL("Could not pass on Add Ie Assoc Response data"));
            ret = -EINVAL;
            goto done;
        }
        WLANSAP_ResetSapConfigAddIE(pConfig, eUPDATE_IE_ASSOC_RESP);
    } else {
        WLANSAP_UpdateSapConfigAddIE(pConfig,
                         params->assocresp_ies,
                         params->assocresp_ies_len,
                         eUPDATE_IE_ASSOC_RESP);
    }

done:
    vos_mem_free(genie);
    return ret;
}

/*
 * FUNCTION: wlan_hdd_validate_operation_channel
 * called by wlan_hdd_cfg80211_start_bss() and
 * wlan_hdd_cfg80211_set_channel()
 * This function validates whether given channel is part of valid
 * channel list.
 */
VOS_STATUS wlan_hdd_validate_operation_channel(hdd_adapter_t *pAdapter,int channel)
{

    v_U32_t num_ch = 0;
    u8 valid_ch[WNI_CFG_VALID_CHANNEL_LIST_LEN];
    u32 indx = 0;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    v_U8_t fValidChannel = FALSE, count = 0;
    hdd_config_t *hdd_pConfig_ini= (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini;

    num_ch = WNI_CFG_VALID_CHANNEL_LIST_LEN;

    if ( hdd_pConfig_ini->sapAllowAllChannel)
    {
         /* Validate the channel */
        for (count = RF_CHAN_1 ; count <= RF_CHAN_165 ; count++)
        {
            if ( channel == rfChannels[count].channelNum )
            {
                fValidChannel = TRUE;
                break;
            }
        }
        if (fValidChannel != TRUE)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Invalid Channel [%d]", __func__, channel);
            return VOS_STATUS_E_FAILURE;
        }
    }
    else
    {
        if (0 != ccmCfgGetStr(hHal, WNI_CFG_VALID_CHANNEL_LIST,
            valid_ch, &num_ch))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: failed to get valid channel list", __func__);
                return VOS_STATUS_E_FAILURE;
            }
        for (indx = 0; indx < num_ch; indx++)
        {
            if (channel == valid_ch[indx])
            {
                break;
            }
         }

        if (indx >= num_ch)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Invalid Channel [%d]", __func__, channel);
            return VOS_STATUS_E_FAILURE;
        }
    }
    return VOS_STATUS_SUCCESS;

}

/**
 * FUNCTION: wlan_hdd_cfg80211_set_channel
 * This function is used to set the channel number
 */
static int wlan_hdd_cfg80211_set_channel( struct wiphy *wiphy, struct net_device *dev,
                                   struct ieee80211_channel *chan,
                                   enum nl80211_channel_type channel_type
                                 )
{
    hdd_adapter_t *pAdapter = NULL;
    v_U32_t num_ch = 0;
    int channel = 0;
    int freq = chan->center_freq; /* freq is in MHZ */
    hdd_context_t *pHddCtx;
    int status;
#ifdef QCA_HT_2040_COEX
    tSmeConfigParams smeConfig;
#endif

    ENTER();


    if( NULL == dev )
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Called with dev = NULL.", __func__);
        return -ENODEV;
    }
    pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_CHANNEL, pAdapter->sessionId,
                     channel_type ));
    hddLog(VOS_TRACE_LEVEL_INFO,
                "%s: device_mode = %d  freq = %d",__func__,
                            pAdapter->device_mode, chan->center_freq);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    /*
     * Do freq to chan conversion
     * TODO: for 11a
     */

    channel = ieee80211_frequency_to_channel(freq);

    /* Check freq range */
    if ((WNI_CFG_CURRENT_CHANNEL_STAMIN > channel) ||
            (WNI_CFG_CURRENT_CHANNEL_STAMAX < channel))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Channel [%d] is outside valid range from %d to %d",
                __func__, channel, WNI_CFG_CURRENT_CHANNEL_STAMIN,
                WNI_CFG_CURRENT_CHANNEL_STAMAX);
        return -EINVAL;
    }

    num_ch = WNI_CFG_VALID_CHANNEL_LIST_LEN;

    if ((WLAN_HDD_SOFTAP != pAdapter->device_mode) &&
       (WLAN_HDD_P2P_GO != pAdapter->device_mode))
    {
        if(VOS_STATUS_SUCCESS != wlan_hdd_validate_operation_channel(pAdapter,channel))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Invalid Channel [%d]", __func__, channel);
            return -EINVAL;
        }
        hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: set channel to [%d] for device mode =%d",
                          __func__, channel,pAdapter->device_mode);
    }
    if( (pAdapter->device_mode == WLAN_HDD_INFRA_STATION)
     || (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)
      )
    {
        hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
        tCsrRoamProfile * pRoamProfile = &pWextState->roamProfile;
        hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

        if (eConnectionState_IbssConnected == pHddStaCtx->conn_info.connState)
        {
           /* Link is up then return cant set channel*/
            hddLog( VOS_TRACE_LEVEL_ERROR,
                   "%s: IBSS Associated, can't set the channel", __func__);
            return -EINVAL;
        }

        num_ch = pRoamProfile->ChannelInfo.numOfChannels = 1;
        pHddStaCtx->conn_info.operationChannel = channel;
        pRoamProfile->ChannelInfo.ChannelList =
            &pHddStaCtx->conn_info.operationChannel;
    }
    else if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
        ||   (pAdapter->device_mode == WLAN_HDD_P2P_GO)
            )
    {
        if (WLAN_HDD_P2P_GO == pAdapter->device_mode)
        {
            if(VOS_STATUS_SUCCESS !=
                       wlan_hdd_validate_operation_channel(pAdapter,channel))
            {
               hddLog(VOS_TRACE_LEVEL_ERROR,
                      "%s: Invalid Channel [%d]", __func__, channel);
               return -EINVAL;
            }
            (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->sapConfig.channel = channel;
        }
        else if ( WLAN_HDD_SOFTAP == pAdapter->device_mode )
        {

            /* If auto channel selection is configured as enable/ 1 then ignore
            channel set by supplicant
            */
#ifdef WLAN_FEATURE_MBSSID
            if (pAdapter->sap_dyn_ini_cfg.apAutoChannelSelection)
#else
            hdd_config_t *cfg_param = (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini;
            if ( cfg_param->apAutoChannelSelection )
#endif
            {
                (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->sapConfig.channel =
                                                          AUTO_CHANNEL_SELECT;
                hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                       "%s: set channel to auto channel (0) for device mode =%d",
                       __func__, pAdapter->device_mode);
            }
            else
            {
                if(VOS_STATUS_SUCCESS !=
                         wlan_hdd_validate_operation_channel(pAdapter,channel))
                {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                          "%s: Invalid Channel [%d]", __func__, channel);
                   return -EINVAL;
                }
                (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->sapConfig.channel = channel;
            }

#ifdef QCA_HT_2040_COEX
            /* set channel bonding mode for 2.4G */
            if ( channel <= 14 )
            {
                switch (channel_type)
                {
                case NL80211_CHAN_HT20:
                case NL80211_CHAN_NO_HT:
                    sme_SetPhyCBMode24G(pHddCtx->hHal,
                                        PHY_SINGLE_CHANNEL_CENTERED);
                    break;

                case NL80211_CHAN_HT40MINUS:
                    sme_SetPhyCBMode24G(pHddCtx->hHal,
                                        PHY_DOUBLE_CHANNEL_HIGH_PRIMARY);
                    break;
                case NL80211_CHAN_HT40PLUS:
                    sme_SetPhyCBMode24G(pHddCtx->hHal,
                                        PHY_DOUBLE_CHANNEL_LOW_PRIMARY);
                    break;

                default:
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                              "%s:Error!!! Invalid HT20/40 mode !",
                              __func__);
                    return -EINVAL;
                }
            }

            vos_mem_zero(&smeConfig, sizeof(smeConfig));
            sme_GetConfigParam(pHddCtx->hHal, &smeConfig);
            if (NL80211_CHAN_NO_HT == channel_type)
                smeConfig.csrConfig.obssEnabled = VOS_FALSE;
            else
                smeConfig.csrConfig.obssEnabled = VOS_TRUE;
            sme_UpdateConfig (pHddCtx->hHal, &smeConfig);

#endif
        }
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_FATAL,
               "%s: Invalid device mode failed to set valid channel", __func__);
        return -EINVAL;
    }
    EXIT();
    return status;
}

/*
 * FUNCTION: wlan_hdd_set_acs_allowed_channels
 * set only allowed channel for ACS
 * input channel list is a string with comma separated
 * channel number, the first number is the total number
 * of channels specified. e.g. 4,1,6,9,36
 */
static void wlan_hdd_set_acs_allowed_channels(
                                             char *acsAllowedChnls,
                                             char *acsSapChnlList,
                                             int length)
{
    char *p;

    /* a white space is required at the beginning of the
       string to be properly parsed by function
       sapSetPreferredChannel later */
    strlcpy(acsSapChnlList, " ", length);
    strlcat(acsSapChnlList, acsAllowedChnls, length);
    p = acsSapChnlList;
    while (*p)
    {
        /* looking for comma, replace it with white space */
        if (*p == ',')
            *p = ' ';

        p++;
    }

    return;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static int wlan_hdd_cfg80211_start_bss(hdd_adapter_t *pHostapdAdapter,
                            struct beacon_parameters *params)
#else
static int wlan_hdd_cfg80211_start_bss(hdd_adapter_t *pHostapdAdapter,
                                       struct cfg80211_beacon_data *params,
                                       const u8 *ssid, size_t ssid_len,
                                       enum nl80211_hidden_ssid hidden_ssid)
#endif
{
    tsap_Config_t *pConfig;
    beacon_data_t *pBeacon = NULL;
    struct ieee80211_mgmt *pMgmt_frame;
    v_U8_t *pIe=NULL;
    v_U16_t capab_info;
    eCsrAuthType RSNAuthType;
    eCsrEncryptionType RSNEncryptType;
    eCsrEncryptionType mcRSNEncryptType;
    int status = VOS_STATUS_SUCCESS;
    tpWLAN_SAPEventCB pSapEventCallback;
    hdd_hostapd_state_t *pHostapdState;
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    struct qc_mac_acl_entry *acl_entry = NULL;
    v_SINT_t i;
    hdd_config_t *iniConfig;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
#ifdef WLAN_FEATURE_11AC
    v_BOOL_t sapForce11ACFor11n =
#ifdef WLAN_FEATURE_MBSSID
             pHostapdAdapter->sap_dyn_ini_cfg.apForce11ACFor11n;
#else
             pHddCtx->cfg_ini->apForce11ACFor11n;
#endif
#endif
    tSmeConfigParams *psmeConfig;
    v_BOOL_t MFPCapable =  VOS_FALSE;
    v_BOOL_t MFPRequired =  VOS_FALSE;
    u_int16_t prev_rsn_length = 0;
    ENTER();

    iniConfig = pHddCtx->cfg_ini;
    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);

    pConfig = &pHostapdAdapter->sessionCtx.ap.sapConfig;

    pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;

    pMgmt_frame = (struct ieee80211_mgmt*)pBeacon->head;

    pConfig->beacon_int =  pMgmt_frame->u.beacon.beacon_int;

    pConfig->disableDFSChSwitch = iniConfig->disableDFSChSwitch;

    //channel is already set in the set_channel Call back
    //pConfig->channel = pCommitConfig->channel;

    /*Protection parameter to enable or disable*/
    pConfig->protEnabled =
           (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apProtEnabled;

    pConfig->dtim_period = pBeacon->dtim_period;

    hddLog(VOS_TRACE_LEVEL_INFO_HIGH,"****pConfig->dtim_period=%d***",
                                      pConfig->dtim_period);

    pConfig->enOverLapCh = !!pHddCtx->cfg_ini->gEnableOverLapCh;
#ifdef WLAN_FEATURE_MBSSID
    if (strlen(pHostapdAdapter->sap_dyn_ini_cfg.acsAllowedChnls) > 0 )
#else
    if (strlen(iniConfig->acsAllowedChnls) > 0)
#endif
    {
        wlan_hdd_set_acs_allowed_channels(
#ifdef WLAN_FEATURE_MBSSID
                               pHostapdAdapter->sap_dyn_ini_cfg.acsAllowedChnls,
#else
                               iniConfig->acsAllowedChnls,
#endif
                               pConfig->acsAllowedChnls,
                               sizeof(pConfig->acsAllowedChnls));
    }

    if (pHostapdAdapter->device_mode == WLAN_HDD_SOFTAP)
    {
        pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
                                       WLAN_EID_COUNTRY);
        if(memcmp(pHddCtx->cfg_ini->apCntryCode, CFG_AP_COUNTRY_CODE_DEFAULT, 3) != 0)
        {
           tANI_BOOLEAN restartNeeded;
           pConfig->ieee80211d = 1;
           vos_mem_copy(pConfig->countryCode, pHddCtx->cfg_ini->apCntryCode, 3);
           sme_setRegInfo(hHal, pConfig->countryCode);
           sme_ResetCountryCodeInformation(hHal, &restartNeeded);
        }
        else if(pIe)
        {
            tANI_BOOLEAN restartNeeded;
            pConfig->ieee80211d = 1;
            vos_mem_copy(pConfig->countryCode, &pIe[2], 3);
            sme_setRegInfo(hHal, pConfig->countryCode);
            sme_ResetCountryCodeInformation(hHal, &restartNeeded);
        }
        else
        {
            pConfig->countryCode[0] = pHddCtx->reg.alpha2[0];
            pConfig->countryCode[1] = pHddCtx->reg.alpha2[1];
            pConfig->ieee80211d = 0;
        }

#ifdef WLAN_FEATURE_MBSSID
        if (!vos_concurrent_sap_sessions_running()) {
            /* Single AP Mode */
            if (VOS_IS_DFS_CH(pConfig->channel))
                pHddCtx->dev_dfs_cac_status = DFS_CAC_NEVER_DONE;
        } else {
            /* MBSSID Mode */
            hdd_adapter_t *con_sap_adapter;
            v_U16_t con_ch;

            con_sap_adapter = hdd_get_con_sap_adapter(pHostapdAdapter);
            if (con_sap_adapter) {
                /* we have active SAP running */
                /* Check if user configured channel is ACS or DFS */
                con_ch = con_sap_adapter->sessionCtx.ap.sapConfig.channel;
                /* For fixed channel or ACS, once primary AP chooses a DFS
                 * channel secondary AP should alway follow primary APs channel
                 * This is applicable for cases even when primary AP moves to
                 * non DFS channel after RADAR detection.
                 */
                if (con_ch == AUTO_CHANNEL_SELECT) {
                    con_ch = con_sap_adapter->sessionCtx.ap.operatingChannel;
                }

                if (VOS_IS_DFS_CH(con_ch)) {
                    /* AP-AP DFS: secondary AP has to follow primary AP's
                     * channel */
                    /* NOTE:Even if orig user configured channel is DFS, use the
                     * current operating channel since when RADAR detected orig
                     * configured channel will be in NOL and already running AP
                     * might have choosen another channel
                     */
                    con_ch = con_sap_adapter->sessionCtx.ap.operatingChannel;
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: Only SCC AP-AP DFS Permitted (ch=%d, con_ch=%d)",
                         __func__,
                         pConfig->channel,
                         con_ch);
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                        "Overriding guest AP's channel !!");
                    pConfig->channel = con_ch;
                }
            } else {
                /* We have idle AP interface (no active SAP running on it
                 * When one SAP is stopped then also this condition applies */
                if (VOS_IS_DFS_CH(pConfig->channel))
                    pHddCtx->dev_dfs_cac_status = DFS_CAC_NEVER_DONE;
            }
        }
#endif
        /*
        * If auto channel is configured i.e. channel is 0,
        * so skip channel validation.
        */
        if( AUTO_CHANNEL_SELECT != pConfig->channel )
        {
            if(VOS_STATUS_SUCCESS != wlan_hdd_validate_operation_channel(pHostapdAdapter,pConfig->channel))
            {
                 hddLog(VOS_TRACE_LEVEL_ERROR,
                         "%s: Invalid Channel [%d]", __func__, pConfig->channel);
                 return -EINVAL;
            }

            /* reject SAP if DFS channel scan is not allowed */
            if ((VOS_FALSE == pHddCtx->cfg_ini->enableDFSChnlScan) &&
                (NV_CHANNEL_DFS ==
                 vos_nv_getChannelEnabledState(pConfig->channel))) {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                       FL("not allowed to start SAP on DFS channel"));
                return -EOPNOTSUPP;
            }
        }
        else
        {
             if(1 != pHddCtx->is_dynamic_channel_range_set)
             {
#ifdef WLAN_FEATURE_MBSSID
                  WLANSAP_SetChannelRange(hHal,
                             pHostapdAdapter->sap_dyn_ini_cfg.apStartChannelNum,
                             pHostapdAdapter->sap_dyn_ini_cfg.apEndChannelNum,
                             pHostapdAdapter->sap_dyn_ini_cfg.apOperatingBand);
#else
                  hdd_config_t *hdd_pConfig=
                                   (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini;
                  WLANSAP_SetChannelRange(hHal,
                                          hdd_pConfig->apStartChannelNum,
                                          hdd_pConfig->apEndChannelNum,
                                          hdd_pConfig->apOperatingBand);
#endif
             }
                   pHddCtx->is_dynamic_channel_range_set = 0;
        }
        WLANSAP_Set_Dfs_Ignore_CAC(hHal, iniConfig->ignoreCAC);

        /*
         * Set the JAPAN W53 disabled INI param
         * in to SAP DFS for restricting these
         * channel from being picked during DFS
         * random channel selection.
         */
        WLANSAP_set_Dfs_Restrict_JapanW53(hHal,
                         iniConfig->gDisableDfsJapanW53);

        /*
         * Set the SAP Indoor/Outdoor preferred
         * operating channel location. This
         * prameter will restrict SAP picking
         * channel from only Indoor/outdoor
         * channels list only based up on the
         * this parameter.
         */
        WLANSAP_set_Dfs_Preferred_Channel_location(hHal,
                                     iniConfig->gSapPreferredChanLocation);
    }
    else
    {
        pConfig->ieee80211d = 0;
    }

    capab_info = pMgmt_frame->u.beacon.capab_info;

    pConfig->privacy = (pMgmt_frame->u.beacon.capab_info &
                        WLAN_CAPABILITY_PRIVACY) ? VOS_TRUE : VOS_FALSE;

    (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy = pConfig->privacy;

    /*Set wps station to configured*/
    pIe = wlan_hdd_get_wps_ie_ptr(pBeacon->tail, pBeacon->tail_len);

    if(pIe)
    {
        if(pIe[1] < (2 + WPS_OUI_TYPE_SIZE))
        {
            hddLog( VOS_TRACE_LEVEL_ERROR, "**Wps Ie Length is too small***");
            return -EINVAL;
        }
        else if(memcmp(&pIe[2], WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE) == 0)
        {
             hddLog( VOS_TRACE_LEVEL_INFO, "** WPS IE(len %d) ***", (pIe[1]+2));
             /* Check 15 bit of WPS IE as it contain information for wps state
              * WPS state
              */
              if(SAP_WPS_ENABLED_UNCONFIGURED == pIe[15])
              {
                  pConfig->wps_state = SAP_WPS_ENABLED_UNCONFIGURED;
              } else if(SAP_WPS_ENABLED_CONFIGURED == pIe[15])
              {
                  pConfig->wps_state = SAP_WPS_ENABLED_CONFIGURED;
              }
        }
    }
    else
    {
        pConfig->wps_state = SAP_WPS_DISABLED;
    }
    pConfig->fwdWPSPBCProbeReq  = 1; // Forward WPS PBC probe request frame up

    pConfig->RSNEncryptType = eCSR_ENCRYPT_TYPE_NONE;
    pConfig->mcRSNEncryptType = eCSR_ENCRYPT_TYPE_NONE;
    (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->ucEncryptType =
        eCSR_ENCRYPT_TYPE_NONE;

    pConfig->RSNWPAReqIELength = 0;
    memset(&pConfig->RSNWPAReqIE[0], 0, sizeof(pConfig->RSNWPAReqIE));
    pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
                                       WLAN_EID_RSN);
    if(pIe && pIe[1])
    {
        pConfig->RSNWPAReqIELength = pIe[1] + 2;
        if (pConfig->RSNWPAReqIELength < sizeof(pConfig->RSNWPAReqIE))
            memcpy(&pConfig->RSNWPAReqIE[0], pIe,
                                   pConfig->RSNWPAReqIELength);
        else
            hddLog(LOGE, "RSNWPA IE MAX Length exceeded; length =%d",
                                         pConfig->RSNWPAReqIELength);
        /* The actual processing may eventually be more extensive than
         * this. Right now, just consume any PMKIDs that are  sent in
         * by the app.
         * */
        status = hdd_softap_unpackIE(
                        vos_get_context( VOS_MODULE_ID_SME, pVosContext),
                        &RSNEncryptType,
                        &mcRSNEncryptType,
                        &RSNAuthType,
                        &MFPCapable,
                        &MFPRequired,
                        pConfig->RSNWPAReqIE[1]+2,
                        pConfig->RSNWPAReqIE );

        if( VOS_STATUS_SUCCESS == status )
        {
            /* Now copy over all the security attributes you have
             * parsed out
             * */
            pConfig->RSNEncryptType = RSNEncryptType; // Use the cipher type in the RSN IE
            pConfig->mcRSNEncryptType = mcRSNEncryptType;
            (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->ucEncryptType
                                                              = RSNEncryptType;
            hddLog( LOG1, FL("CSR AuthType = %d, "
                        "EncryptionType = %d mcEncryptionType = %d"),
                        RSNAuthType, RSNEncryptType, mcRSNEncryptType);
        }
    }

    pIe = wlan_hdd_get_vendor_oui_ie_ptr(WPA_OUI_TYPE, WPA_OUI_TYPE_SIZE,
                                         pBeacon->tail, pBeacon->tail_len);

    if(pIe && pIe[1] && (pIe[0] == DOT11F_EID_WPA))
    {
        if (pConfig->RSNWPAReqIE[0])
        {
            /*Mixed mode WPA/WPA2*/
            prev_rsn_length = pConfig->RSNWPAReqIELength;
            pConfig->RSNWPAReqIELength += pIe[1] + 2;
            if (pConfig->RSNWPAReqIELength < sizeof(pConfig->RSNWPAReqIE))
                memcpy(&pConfig->RSNWPAReqIE[0] + prev_rsn_length, pIe,
                                                            pIe[1] + 2);
            else
                hddLog(LOGE, "RSNWPA IE MAX Length exceeded; length =%d",
                                             pConfig->RSNWPAReqIELength);
        }
        else
        {
            pConfig->RSNWPAReqIELength = pIe[1] + 2;
            if (pConfig->RSNWPAReqIELength < sizeof(pConfig->RSNWPAReqIE))
                memcpy(&pConfig->RSNWPAReqIE[0], pIe,
                                       pConfig->RSNWPAReqIELength);
            else
               hddLog(LOGE, "RSNWPA IE MAX Length exceeded; length =%d",
                                            pConfig->RSNWPAReqIELength);
            status = hdd_softap_unpackIE(
                          vos_get_context( VOS_MODULE_ID_SME, pVosContext),
                          &RSNEncryptType,
                          &mcRSNEncryptType,
                          &RSNAuthType,
                          &MFPCapable,
                          &MFPRequired,
                          pConfig->RSNWPAReqIE[1]+2,
                          pConfig->RSNWPAReqIE );

            if( VOS_STATUS_SUCCESS == status )
            {
                /* Now copy over all the security attributes you have
                 * parsed out
                 * */
                pConfig->RSNEncryptType = RSNEncryptType; // Use the cipher type in the RSN IE
                pConfig->mcRSNEncryptType = mcRSNEncryptType;
                (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->ucEncryptType
                                                              = RSNEncryptType;
                hddLog( LOG1, FL("CSR AuthType = %d, "
                                "EncryptionType = %d mcEncryptionType = %d"),
                                RSNAuthType, RSNEncryptType, mcRSNEncryptType);
            }
        }
    }

    if (pConfig->RSNWPAReqIELength > sizeof(pConfig->RSNWPAReqIE)) {
        hddLog( VOS_TRACE_LEVEL_ERROR, "**RSNWPAReqIELength is too large***");
        return -EINVAL;
    }

    pConfig->SSIDinfo.ssidHidden = VOS_FALSE;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
    if (params->ssid != NULL)
    {
        vos_mem_copy(pConfig->SSIDinfo.ssid.ssId, params->ssid, params->ssid_len);
        pConfig->SSIDinfo.ssid.length = params->ssid_len;

        switch (params->hidden_ssid) {
        case NL80211_HIDDEN_SSID_NOT_IN_USE:
                hddLog(LOG1, "HIDDEN_SSID_NOT_IN_USE");
                pConfig->SSIDinfo.ssidHidden = eHIDDEN_SSID_NOT_IN_USE;
                break;
        case NL80211_HIDDEN_SSID_ZERO_LEN:
                hddLog(LOG1, "HIDDEN_SSID_ZERO_LEN");
                pConfig->SSIDinfo.ssidHidden = eHIDDEN_SSID_ZERO_LEN;
                break;
        case NL80211_HIDDEN_SSID_ZERO_CONTENTS:
                hddLog(LOG1, "HIDDEN_SSID_ZERO_CONTENTS");
                pConfig->SSIDinfo.ssidHidden = eHIDDEN_SSID_ZERO_CONTENTS;
                break;
        default:
                hddLog(LOGE, "Wrong hidden_ssid param %d", params->hidden_ssid);
                break;
        }
    }
#else
    if (ssid != NULL)
    {
        vos_mem_copy(pConfig->SSIDinfo.ssid.ssId, ssid, ssid_len);
        pConfig->SSIDinfo.ssid.length = ssid_len;

        switch (hidden_ssid) {
        case NL80211_HIDDEN_SSID_NOT_IN_USE:
                hddLog(LOG1, "HIDDEN_SSID_NOT_IN_USE");
                pConfig->SSIDinfo.ssidHidden = eHIDDEN_SSID_NOT_IN_USE;
                break;
        case NL80211_HIDDEN_SSID_ZERO_LEN:
                hddLog(LOG1, "HIDDEN_SSID_ZERO_LEN");
                pConfig->SSIDinfo.ssidHidden = eHIDDEN_SSID_ZERO_LEN;
                break;
        case NL80211_HIDDEN_SSID_ZERO_CONTENTS:
                hddLog(LOG1, "HIDDEN_SSID_ZERO_CONTENTS");
                pConfig->SSIDinfo.ssidHidden = eHIDDEN_SSID_ZERO_CONTENTS;
                break;
        default:
                hddLog(LOGE, "Wrong hidden_ssid param %d", hidden_ssid);
                break;
        }
    }
#endif

    vos_mem_copy(pConfig->self_macaddr.bytes,
               pHostapdAdapter->macAddressCurrent.bytes, sizeof(v_MACADDR_t));

    /* default value */
    pConfig->SapMacaddr_acl = eSAP_ACCEPT_UNLESS_DENIED;
    pConfig->num_accept_mac = 0;
    pConfig->num_deny_mac = 0;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
    pConfig->cc_switch_mode =
           (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->WlanMccToSccSwitchMode;
#endif

    pIe = wlan_hdd_get_vendor_oui_ie_ptr(BLACKLIST_OUI_TYPE, WPA_OUI_TYPE_SIZE,
                                         pBeacon->tail, pBeacon->tail_len);

    /* pIe for black list is following form:
            type    : 1 byte
            length  : 1 byte
            OUI     : 4 bytes
            acl type : 1 byte
            no of mac addr in black list: 1 byte
            list of mac_acl_entries: variable, 6 bytes per mac address + sizeof(int) for vlan id
    */
    if ((pIe != NULL) && (pIe[1] != 0))
    {
        pConfig->SapMacaddr_acl = pIe[6];
        pConfig->num_deny_mac   = pIe[7];
        hddLog(VOS_TRACE_LEVEL_INFO,"acl type = %d no deny mac = %d",
                                     pIe[6], pIe[7]);
        if (pConfig->num_deny_mac > MAX_ACL_MAC_ADDRESS)
            pConfig->num_deny_mac = MAX_ACL_MAC_ADDRESS;
        acl_entry = (struct qc_mac_acl_entry *)(pIe + 8);
        for (i = 0; i < pConfig->num_deny_mac; i++)
        {
            vos_mem_copy(&pConfig->deny_mac[i], acl_entry->addr, sizeof(qcmacaddr));
            acl_entry++;
        }
    }
    pIe = wlan_hdd_get_vendor_oui_ie_ptr(WHITELIST_OUI_TYPE, WPA_OUI_TYPE_SIZE,
                                         pBeacon->tail, pBeacon->tail_len);

    /* pIe for white list is following form:
            type    : 1 byte
            length  : 1 byte
            OUI     : 4 bytes
            acl type : 1 byte
            no of mac addr in white list: 1 byte
            list of mac_acl_entries: variable, 6 bytes per mac address + sizeof(int) for vlan id
    */
    if ((pIe != NULL) && (pIe[1] != 0))
    {
        pConfig->SapMacaddr_acl = pIe[6];
        pConfig->num_accept_mac   = pIe[7];
        hddLog(VOS_TRACE_LEVEL_INFO,"acl type = %d no accept mac = %d",
                                      pIe[6], pIe[7]);
        if (pConfig->num_accept_mac > MAX_ACL_MAC_ADDRESS)
            pConfig->num_accept_mac = MAX_ACL_MAC_ADDRESS;
        acl_entry = (struct qc_mac_acl_entry *)(pIe + 8);
        for (i = 0; i < pConfig->num_accept_mac; i++)
        {
            vos_mem_copy(&pConfig->accept_mac[i], acl_entry->addr, sizeof(qcmacaddr));
            acl_entry++;
        }
    }

    wlan_hdd_set_sapHwmode(pHostapdAdapter);

#ifdef WLAN_FEATURE_11AC
    /* Overwrite the hostapd setting for HW mode only for 11ac.
     * This is valid only if mode is set to 11n in hostapd and either AUTO or 11ac in .ini .
     * Otherwise, leave whatever is set in hostapd (a OR b OR g OR n mode) */
    if( ((pConfig->SapHw_mode == eSAP_DOT11_MODE_11n) ||
         (pConfig->SapHw_mode == eSAP_DOT11_MODE_11n_ONLY)) &&
         sapForce11ACFor11n &&
        (( (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->dot11Mode == eHDD_DOT11_MODE_AUTO ) ||
         ( (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->dot11Mode == eHDD_DOT11_MODE_11ac ) ||
         ( (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->dot11Mode == eHDD_DOT11_MODE_11ac_ONLY )) )
    {
        if ((WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->dot11Mode == eHDD_DOT11_MODE_11ac_ONLY)
            pConfig->SapHw_mode = eSAP_DOT11_MODE_11ac_ONLY;
        else
            pConfig->SapHw_mode = eSAP_DOT11_MODE_11ac;

        /* If ACS disable and selected channel <= 14
             OR
             ACS enabled and ACS operating band is chosen as 2.4
         AND
             VHT in 2.4G Disabled
         THEN
             Fallback to 11N mode
        */
        if ((((AUTO_CHANNEL_SELECT != pConfig->channel && pConfig->channel <= 14)
                || (AUTO_CHANNEL_SELECT == pConfig->channel &&
#ifdef WLAN_FEATURE_MBSSID
                pHostapdAdapter->sap_dyn_ini_cfg.apOperatingBand
#else
                iniConfig->apOperatingBand
#endif
                == eSAP_RF_SUBBAND_2_4_GHZ)) &&
            (WLAN_HDD_GET_CTX(pHostapdAdapter)->cfg_ini->enableVhtFor24GHzBand
                                                                 == FALSE)) ||
            (WLAN_HDD_GET_CTX(pHostapdAdapter)->isVHT80Allowed == FALSE))
        {
            pConfig->SapHw_mode = eSAP_DOT11_MODE_11n;
        }
    }
#endif

    if ( AUTO_CHANNEL_SELECT != pConfig->channel )
    {
        sme_SelectCBMode(hHal,
            sapConvertSapPhyModeToCsrPhyMode(pConfig->SapHw_mode),
            pConfig->channel);
    }
    // ht_capab is not what the name conveys,this is used for protection bitmap
    pConfig->ht_capab =
                 (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apProtection;

    if ( 0 != wlan_hdd_cfg80211_update_apies(pHostapdAdapter, params) )
    {
        hddLog(LOGE, FL("SAP Not able to set AP IEs"));
        WLANSAP_ResetSapConfigAddIE(pConfig, eUPDATE_IE_ALL);
        return -EINVAL;
    }

    //Uapsd Enabled Bit
    pConfig->UapsdEnable =
          (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apUapsdEnabled;
    //Enable OBSS protection
    pConfig->obssProtEnabled =
           (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apOBSSProtEnabled;

    if (pHostapdAdapter->device_mode == WLAN_HDD_SOFTAP) {
        pConfig->sap_dot11mc =
                (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->sap_dot11mc;
    } else { /* for P2P-Go case */
        pConfig->sap_dot11mc = 1;
    }
    hddLog(LOG1, FL("11MC Support Enabled : %d\n"),
           pConfig->sap_dot11mc);

#ifdef WLAN_FEATURE_11W
    pConfig->mfpCapable = MFPCapable;
    pConfig->mfpRequired = MFPRequired;
    hddLog(LOG1, FL("Soft AP MFP capable %d, MFP required %d\n"),
           pConfig->mfpCapable, pConfig->mfpRequired);
#endif

    hddLog(LOGW, FL("SOftAP macaddress : "MAC_ADDRESS_STR),
                 MAC_ADDR_ARRAY(pHostapdAdapter->macAddressCurrent.bytes));
    hddLog(LOGW,FL("ssid =%s, beaconint=%d, channel=%d"),
                    pConfig->SSIDinfo.ssid.ssId, (int)pConfig->beacon_int,
                    (int)pConfig->channel);
    hddLog(LOGW,FL("hw_mode=%x, privacy=%d, authType=%d"),
                    pConfig->SapHw_mode, pConfig->privacy,
                    pConfig->authType);
    hddLog(LOGW,FL("RSN/WPALen=%d, Uapsd = %d"),
                   (int)pConfig->RSNWPAReqIELength, pConfig->UapsdEnable);
    hddLog(LOGW,FL("ProtEnabled = %d, OBSSProtEnabled = %d"),
                    pConfig->protEnabled, pConfig->obssProtEnabled);

    if(test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags))
    {
        WLANSAP_ResetSapConfigAddIE(pConfig, eUPDATE_IE_ALL);
        //Bss already started. just return.
        //TODO Probably it should update some beacon params.
        hddLog( LOGE, "Bss Already started...Ignore the request");
        EXIT();
        return 0;
    }

    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_DEBUG, FL("Reached max concurrent connections"));
        return -EINVAL;
    }

    pConfig->persona = pHostapdAdapter->device_mode;

    psmeConfig = (tSmeConfigParams*) vos_mem_malloc(sizeof(tSmeConfigParams));
    if ( NULL != psmeConfig)
    {
#ifdef WLAN_FEATURE_MBSSID
        eHalStatus halStatus = eHAL_STATUS_FAILURE;
        sme_GetConfigParam(hHal, psmeConfig);
        psmeConfig->csrConfig.scanBandPreference =
                         pHostapdAdapter->sap_dyn_ini_cfg.acsScanBandPreference;
        halStatus = sme_UpdateConfig(pHddCtx->hHal, psmeConfig);
        if ( !HAL_STATUS_SUCCESS( halStatus )
                                    && pConfig->channel == AUTO_CHANNEL_SELECT )
        {
            hddLog(LOGE, "sme_UpdateConfig() for ACS Scan band pref Fail: %d",
                                                                     halStatus);
            return -EAGAIN;
        }
        pConfig->scanBandPreference = psmeConfig->csrConfig.scanBandPreference;
#else
        sme_GetConfigParam(hHal, psmeConfig);
        pConfig->scanBandPreference = psmeConfig->csrConfig.scanBandPreference;
#endif
        vos_mem_free(psmeConfig);
    }
#ifdef WLAN_FEATURE_MBSSID
    pConfig->acsBandSwitchThreshold =
                        pHostapdAdapter->sap_dyn_ini_cfg.acsBandSwitchThreshold;
    pConfig->apAutoChannelSelection =
                        pHostapdAdapter->sap_dyn_ini_cfg.apAutoChannelSelection;
    pConfig->apStartChannelNum =
                        pHostapdAdapter->sap_dyn_ini_cfg.apStartChannelNum;
    pConfig->apEndChannelNum =
                        pHostapdAdapter->sap_dyn_ini_cfg.apEndChannelNum;
    pConfig->apOperatingBand = pHostapdAdapter->sap_dyn_ini_cfg.apOperatingBand;
#else
    pConfig->acsBandSwitchThreshold = iniConfig->acsBandSwitchThreshold;
    pConfig->apAutoChannelSelection = iniConfig->apAutoChannelSelection;
    pConfig->apStartChannelNum = iniConfig->apStartChannelNum;
    pConfig->apEndChannelNum = iniConfig->apEndChannelNum;
    pConfig->apOperatingBand = iniConfig->apOperatingBand;
#endif

#if defined(FEATURE_WLAN_AP_AP_ACS_OPTIMIZE) && defined(WLAN_FEATURE_MBSSID)
    if (vos_concurrent_sap_sessions_running() &&
        pConfig->channel == AUTO_CHANNEL_SELECT) {
        hdd_adapter_t *con_sap_adapter;
        tsap_Config_t *con_sap_config = NULL;

        con_sap_adapter = hdd_get_con_sap_adapter(pHostapdAdapter);

        if (con_sap_adapter)
            con_sap_config = &con_sap_adapter->sessionCtx.ap.sapConfig;

        pConfig->skip_acs_scan_status = eSAP_DO_NEW_ACS_SCAN;

        hddLog(LOG1, FL("HDD_ACS_SKIP_STATUS = %d"),
            pHddCtx->skip_acs_scan_status);

        if (con_sap_config && con_sap_config->channel == AUTO_CHANNEL_SELECT &&
            pHddCtx->skip_acs_scan_status == eSAP_SKIP_ACS_SCAN) {

            hddLog(LOG1, FL("Operating Band: PriAP: %d SecAP: %d"),
               con_sap_config->apOperatingBand, pConfig->apOperatingBand);

            if (con_sap_config->apOperatingBand == 5 &&
                                              pConfig->apOperatingBand > 0) {
                pConfig->skip_acs_scan_status = eSAP_SKIP_ACS_SCAN;

            } else if (con_sap_config->apOperatingBand
                                               == pConfig->apOperatingBand) {
                v_U8_t con_sap_st_ch, con_sap_end_ch;
                v_U8_t cur_sap_st_ch, cur_sap_end_ch;
                v_U8_t bandStartChannel, bandEndChannel;

                con_sap_st_ch = con_sap_config->apStartChannelNum;
                con_sap_end_ch = con_sap_config->apEndChannelNum;
                cur_sap_st_ch = pConfig->apStartChannelNum;
                cur_sap_end_ch = pConfig->apEndChannelNum;

                WLANSAP_extend_to_acs_range(pConfig->apOperatingBand,
                                &cur_sap_st_ch, &cur_sap_end_ch,
                                &bandStartChannel, &bandEndChannel);

                WLANSAP_extend_to_acs_range(con_sap_config->apOperatingBand,
                                &con_sap_st_ch, &con_sap_end_ch,
                                &bandStartChannel, &bandEndChannel);

                if (con_sap_st_ch <= cur_sap_st_ch &&
                    con_sap_end_ch >= cur_sap_end_ch) {

                    pConfig->skip_acs_scan_status = eSAP_SKIP_ACS_SCAN;

                } else if (con_sap_st_ch >= cur_sap_st_ch &&
                    con_sap_end_ch >= cur_sap_end_ch) {

                    pConfig->skip_acs_scan_status = eSAP_DO_PAR_ACS_SCAN;

                    pConfig->skip_acs_scan_range1_stch = cur_sap_st_ch;
                    pConfig->skip_acs_scan_range1_endch = con_sap_st_ch - 1;
                    pConfig->skip_acs_scan_range2_stch = 0;
                    pConfig->skip_acs_scan_range2_endch = 0;

                } else if (con_sap_st_ch <= cur_sap_st_ch &&
                    con_sap_end_ch <= cur_sap_end_ch) {

                    pConfig->skip_acs_scan_status = eSAP_DO_PAR_ACS_SCAN;

                    pConfig->skip_acs_scan_range1_stch = con_sap_end_ch + 1;
                    pConfig->skip_acs_scan_range1_endch = cur_sap_end_ch;
                    pConfig->skip_acs_scan_range2_stch = 0;
                    pConfig->skip_acs_scan_range2_endch = 0;

                } else if (con_sap_st_ch >= cur_sap_st_ch &&
                    con_sap_end_ch <= cur_sap_end_ch) {

                    pConfig->skip_acs_scan_status = eSAP_DO_PAR_ACS_SCAN;

                    pConfig->skip_acs_scan_range1_stch = cur_sap_st_ch;
                    pConfig->skip_acs_scan_range1_endch = con_sap_st_ch - 1;
                    pConfig->skip_acs_scan_range2_stch = con_sap_end_ch;
                    pConfig->skip_acs_scan_range2_endch = cur_sap_end_ch + 1;

                } else
                    pConfig->skip_acs_scan_status = eSAP_DO_NEW_ACS_SCAN;

                hddLog(LOG1,
                       FL("SecAP ACS Skip = %d, ACS CH RANGE = %d-%d, %d-%d"),
                       pConfig->skip_acs_scan_status,
                       pConfig->skip_acs_scan_range1_stch,
                       pConfig->skip_acs_scan_range1_endch,
                       pConfig->skip_acs_scan_range2_stch,
                       pConfig->skip_acs_scan_range2_endch);
            }
        }
    }
#endif

    pSapEventCallback = hdd_hostapd_SAPEventCB;

    (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->dfs_cac_block_tx = VOS_TRUE;

    status = WLANSAP_StartBss(
#ifdef WLAN_FEATURE_MBSSID
                 WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
#else
                 pVosContext,
#endif
                 pSapEventCallback, pConfig, (v_PVOID_t)pHostapdAdapter->dev);
    if (!VOS_IS_STATUS_SUCCESS(status))
    {
        WLANSAP_ResetSapConfigAddIE(pConfig, eUPDATE_IE_ALL);
        hddLog(LOGE,FL("SAP Start Bss fail"));
        return -EINVAL;
    }

    hddLog(LOG1,
           FL("Waiting for Scan to complete(auto mode) and BSS to start"));

    status = vos_wait_single_event(&pHostapdState->vosEvent, 10000);

    WLANSAP_ResetSapConfigAddIE(pConfig, eUPDATE_IE_ALL);

    if (!VOS_IS_STATUS_SUCCESS(status))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 ("%s: ERROR: HDD vos wait for single_event failed!!"),
                 __func__);
        smeGetCommandQStatus(hHal);
        VOS_ASSERT(0);
        return -EINVAL;
    }

    /* Successfully started Bss update the state bit. */
    set_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
    wlan_hdd_incr_active_session(pHddCtx, pHostapdAdapter->device_mode);

#ifdef WLAN_FEATURE_P2P_DEBUG
    if (pHostapdAdapter->device_mode == WLAN_HDD_P2P_GO)
    {
         if(globalP2PConnectionStatus == P2P_GO_NEG_COMPLETED)
         {
             globalP2PConnectionStatus = P2P_GO_COMPLETED_STATE;
             hddLog(LOGE,"[P2P State] From Go nego completed to "
                         "Non-autonomous Group started");
         }
         else if(globalP2PConnectionStatus == P2P_NOT_ACTIVE)
         {
             globalP2PConnectionStatus = P2P_GO_COMPLETED_STATE;
             hddLog(LOGE,"[P2P State] From Inactive to "
                         "Autonomous Group started");
         }
    }
#endif

    pHostapdState->bCommit = TRUE;
    EXIT();

   return 0;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static int __wlan_hdd_cfg80211_add_beacon(struct wiphy *wiphy,
                                        struct net_device *dev,
                                        struct beacon_parameters *params)
{
    hdd_adapter_t  *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t  *pHddCtx;
    int status;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_ADD_BEACON,
                     pAdapter->sessionId, params->interval));
    hddLog(LOG2, FL("Device mode=%d"), pAdapter->device_mode);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return status;
    }

    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_DEBUG, FL("Reached max concurrent connections"));
        return -EINVAL;
    }

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP) ||
        (pAdapter->device_mode == WLAN_HDD_P2P_GO)) {
        beacon_data_t *old, *new;

        old = pAdapter->sessionCtx.ap.beacon;

        if (old) {
            hddLog(VOS_TRACE_LEVEL_WARN,
                   FL("already beacon info added to session(%d)"),
                      pAdapter->sessionId);
            return -EALREADY;
        }

        status = wlan_hdd_cfg80211_alloc_new_beacon(pAdapter,&new,params);
        if (status != VOS_STATUS_SUCCESS) {
             hddLog(VOS_TRACE_LEVEL_FATAL,
                    FL("Error!!! Allocating the new beacon"));
             return -EINVAL;
        }

        pAdapter->sessionCtx.ap.beacon = new;

        status = wlan_hdd_cfg80211_start_bss(pAdapter, params);
        if (0 != status) {
           pAdapter->sessionCtx.ap.beacon = NULL;
           kfree(new);
        }
    }

    EXIT();
    return status;
}

/**
 * wlan_hdd_cfg80211_add_beacon() - add beacon in sap mode
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 * @param: Pointer to beacon parameters
 *
 * Return: zero for success non-zero for failure
 */
static int wlan_hdd_cfg80211_add_beacon(struct wiphy *wiphy,
					struct net_device *dev,
					struct beacon_parameters *params)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_add_beacon(wiphy, dev, params);
	vos_ssr_unprotect(__func__);

	return ret;
}

static int __wlan_hdd_cfg80211_set_beacon(struct wiphy *wiphy,
                                        struct net_device *dev,
                                        struct beacon_parameters *params)
{
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t  *pHddCtx;
    int status;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_BEACON,
                     pAdapter->sessionId, pHddStaCtx->conn_info.authType));
    hddLog(LOG1, FL("Device_mode = %d"), pAdapter->device_mode);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return status;
    }

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP) ||
        (pAdapter->device_mode == WLAN_HDD_P2P_GO)) {
        beacon_data_t *old, *new;

        old = pAdapter->sessionCtx.ap.beacon;

        if (!old) {
            hddLog(LOGE,
                   FL("session(%d) old and new heads points to NULL"),
                   pAdapter->sessionId);
            return -ENOENT;
        }

        status = wlan_hdd_cfg80211_alloc_new_beacon(pAdapter,&new,params);

        if (status != VOS_STATUS_SUCCESS) {
            hddLog(VOS_TRACE_LEVEL_FATAL,
                   FL("Error!!! Allocating the new beacon"));
            return -EINVAL;
        }

        pAdapter->sessionCtx.ap.beacon = new;
        status = wlan_hdd_cfg80211_start_bss(pAdapter, params);
    }

    EXIT();
    return status;
}

/**
 * wlan_hdd_cfg80211_set_beacon() - set beacon in sap mode
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 * @param: Pointer to beacon parameters
 *
 * Return: zero for success non-zero for failure
 */
static int wlan_hdd_cfg80211_set_beacon(struct wiphy *wiphy,
					struct net_device *dev,
					struct beacon_parameters *params)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_set_beacon(wiphy, dev, params);
	vos_ssr_unprotect(__func__);

	return ret;
}

#endif //(LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
static int __wlan_hdd_cfg80211_del_beacon(struct wiphy *wiphy,
                                        struct net_device *dev)
#else
static int __wlan_hdd_cfg80211_stop_ap (struct wiphy *wiphy,
                                      struct net_device *dev)
#endif
{
    hdd_adapter_t  *pAdapter   = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t  *pHddCtx    = NULL;
    hdd_scaninfo_t *pScanInfo  = NULL;
    hdd_adapter_t  *staAdapter = NULL;
    VOS_STATUS      status     = VOS_STATUS_E_FAILURE;
    tSirUpdateIE    updateIE;
    beacon_data_t  *old;
    int             ret;
    unsigned long   rc;
    hdd_adapter_list_node_t *pAdapterNode = NULL;
    hdd_adapter_list_node_t *pNext        = NULL;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_STOP_AP,
                     pAdapter->sessionId, pAdapter->device_mode));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return ret;
    }

    if (!(pAdapter->device_mode == WLAN_HDD_SOFTAP ||
          pAdapter->device_mode == WLAN_HDD_P2P_GO)) {
        return -EOPNOTSUPP;
    }

    hddLog(LOG1, FL("Device_mode = %d"), pAdapter->device_mode);

    status = hdd_get_front_adapter (pHddCtx, &pAdapterNode);
    while (NULL != pAdapterNode && VOS_STATUS_SUCCESS == status) {
        staAdapter = pAdapterNode->pAdapter;

        if (WLAN_HDD_INFRA_STATION == staAdapter->device_mode ||
           (WLAN_HDD_P2P_CLIENT == staAdapter->device_mode) ||
           (WLAN_HDD_P2P_GO == staAdapter->device_mode)) {
            pScanInfo = &staAdapter->scan_info;

            if (pScanInfo && pScanInfo->mScanPending) {
               hddLog(LOG1, FL("Aborting pending scan for device mode:%d"),
                      staAdapter->device_mode);
               INIT_COMPLETION(pScanInfo->abortscan_event_var);
               hdd_abort_mac_scan(staAdapter->pHddCtx, staAdapter->sessionId,
                                  eCSR_SCAN_ABORT_DEFAULT);
               rc = wait_for_completion_timeout(
                     &pScanInfo->abortscan_event_var,
                     msecs_to_jiffies(WLAN_WAIT_TIME_ABORTSCAN));
               if (!rc) {
                   hddLog(LOGE,
                          FL("Timeout occurred while waiting for abortscan"));
                   VOS_ASSERT(pScanInfo->mScanPending);
               }
            }
        }

        status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
        pAdapterNode = pNext;
    }

    hdd_hostapd_stop(dev);

    old = pAdapter->sessionCtx.ap.beacon;
    if (!old) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("Session(%d) beacon data points to NULL"),
               pAdapter->sessionId);
        return -EINVAL;
    }

    hdd_cleanup_actionframe(pHddCtx, pAdapter);

    mutex_lock(&pHddCtx->sap_lock);
    if (test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags)) {
        hdd_hostapd_state_t *pHostapdState =
                    WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

        vos_event_reset(&pHostapdState->vosEvent);
#ifdef WLAN_FEATURE_MBSSID
        status = WLANSAP_StopBss(WLAN_HDD_GET_SAP_CTX_PTR(pAdapter));
#else
        status = WLANSAP_StopBss(pHddCtx->pvosContext);
#endif
        if (VOS_IS_STATUS_SUCCESS(status)) {
            status = vos_wait_single_event(&pHostapdState->vosEvent, 10000);

            if (!VOS_IS_STATUS_SUCCESS(status)) {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                       FL("HDD vos wait for single_event failed!!"));
                VOS_ASSERT(0);
            }
        }
        clear_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags);
        /* BSS stopped, clear the active sessions for this device mode */
        wlan_hdd_decr_active_session(pHddCtx, pAdapter->device_mode);

        pAdapter->sessionCtx.ap.beacon = NULL;
        kfree(old);
    }
    mutex_unlock(&pHddCtx->sap_lock);

    if (status != VOS_STATUS_SUCCESS) {
        hddLog(VOS_TRACE_LEVEL_FATAL, FL("Stopping the BSS"));
        return -EINVAL;
    }

    vos_mem_copy(updateIE.bssid, pAdapter->macAddressCurrent.bytes,
          sizeof(tSirMacAddr));
    updateIE.smeSessionId = pAdapter->sessionId;
    updateIE.ieBufferlength = 0;
    updateIE.pAdditionIEBuffer = NULL;
    updateIE.append = VOS_TRUE;
    updateIE.notify = VOS_TRUE;
    if (sme_UpdateAddIE(WLAN_HDD_GET_HAL_CTX(pAdapter),
              &updateIE, eUPDATE_IE_PROBE_BCN) == eHAL_STATUS_FAILURE) {
        hddLog(LOGE, FL("Could not pass on PROBE_RSP_BCN data to PE"));
    }

    if (sme_UpdateAddIE(WLAN_HDD_GET_HAL_CTX(pAdapter),
              &updateIE, eUPDATE_IE_ASSOC_RESP) == eHAL_STATUS_FAILURE) {
        hddLog(LOGE, FL("Could not pass on ASSOC_RSP data to PE"));
    }

    // Reset WNI_CFG_PROBE_RSP Flags
    wlan_hdd_reset_prob_rspies(pAdapter);

#ifdef WLAN_FEATURE_P2P_DEBUG
    if((pAdapter->device_mode == WLAN_HDD_P2P_GO) &&
       (globalP2PConnectionStatus == P2P_GO_COMPLETED_STATE)) {
        hddLog(LOGE,"[P2P State] From GO completed to Inactive state "
                    "GO got removed");
        globalP2PConnectionStatus = P2P_NOT_ACTIVE;
    }
#endif
    EXIT();
    return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0))
/**
 * wlan_hdd_cfg80211_del_beacon() - delete beacon in sap mode
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 *
 * Return: zero for success non-zero for failure
 */
static int wlan_hdd_cfg80211_del_beacon(struct wiphy *wiphy,
					struct net_device *dev)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_del_beacon(wiphy, dev);
	vos_ssr_unprotect(__func__);

	return ret;
}
#else
/**
 * wlan_hdd_cfg80211_stop_ap() - stop sap
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 *
 * Return: zero for success non-zero for failure
 */
static int wlan_hdd_cfg80211_stop_ap(struct wiphy *wiphy,
					struct net_device *dev)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_stop_ap(wiphy, dev);
	vos_ssr_unprotect(__func__);

	return ret;
}
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,3,0))
static int __wlan_hdd_cfg80211_start_ap(struct wiphy *wiphy,
                                      struct net_device *dev,
                                      struct cfg80211_ap_settings *params)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    int            status;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_START_AP, pAdapter->sessionId,
                     params-> beacon_interval));
    if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD adapter magic is invalid", __func__);
        return -ENODEV;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: pAdapter = %p, device mode = %d",
           __func__, pAdapter, pAdapter->device_mode);

    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_DEBUG, FL("Reached max concurrent connections"));
        return -EINVAL;
    }

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
      || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
       )
    {
        beacon_data_t  *old, *new;

        old = pAdapter->sessionCtx.ap.beacon;

        if (old)
            return -EALREADY;

        status = wlan_hdd_cfg80211_alloc_new_beacon(pAdapter, &new, &params->beacon, params->dtim_period);

        if (status != 0)
        {
             hddLog(VOS_TRACE_LEVEL_FATAL,
                   "%s:Error!!! Allocating the new beacon", __func__);
             return -EINVAL;
        }
        pAdapter->sessionCtx.ap.beacon = new;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
        wlan_hdd_cfg80211_set_channel(wiphy, dev,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0))
        params->channel, params->channel_type);
#else
        params->chandef.chan, cfg80211_get_chandef_type(&(params->chandef)));
#endif
#endif
        /* set authentication type */
        switch ( params->auth_type )
        {
        case  NL80211_AUTHTYPE_OPEN_SYSTEM:
            pAdapter->sessionCtx.ap.sapConfig.authType = eSAP_OPEN_SYSTEM;
            break;
        case NL80211_AUTHTYPE_SHARED_KEY:
            pAdapter->sessionCtx.ap.sapConfig.authType = eSAP_SHARED_KEY;
            break;
        default:
            pAdapter->sessionCtx.ap.sapConfig.authType = eSAP_AUTO_SWITCH;
        }

        status = wlan_hdd_cfg80211_start_bss(pAdapter, &params->beacon, params->ssid,
                                             params->ssid_len, params->hidden_ssid);
    }

    EXIT();
    return status;
}

/**
 * wlan_hdd_cfg80211_start_ap() - start sap
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 * @params: Pointer to start ap configuration parameters
 *
 * Return: zero for success non-zero for failure
 */
static int wlan_hdd_cfg80211_start_ap(struct wiphy *wiphy,
                                      struct net_device *dev,
                                      struct cfg80211_ap_settings *params)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_start_ap(wiphy, dev, params);
	vos_ssr_unprotect(__func__);

	return ret;
}
#endif

static int __wlan_hdd_cfg80211_change_beacon(struct wiphy *wiphy,
                                           struct net_device *dev,
                                           struct cfg80211_beacon_data *params)
{
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    beacon_data_t *old,*new;
    int status;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_CHANGE_BEACON,
                     pAdapter->sessionId, pAdapter->device_mode));
    hddLog(VOS_TRACE_LEVEL_INFO, FL("device_mode = %d"), pAdapter->device_mode);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return status;
    }

    if (!(pAdapter->device_mode == WLAN_HDD_SOFTAP ||
          pAdapter->device_mode == WLAN_HDD_P2P_GO)) {
        return -EOPNOTSUPP;
    }

    old = pAdapter->sessionCtx.ap.beacon;

    if (!old) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  FL("session(%d) beacon data points to NULL"),
                     pAdapter->sessionId);
        return -EINVAL;
    }

    status = wlan_hdd_cfg80211_alloc_new_beacon(pAdapter, &new, params, 0);

    if (status != VOS_STATUS_SUCCESS) {
        hddLog(VOS_TRACE_LEVEL_FATAL, FL("new beacon alloc failed"));
        return -EINVAL;
    }

    pAdapter->sessionCtx.ap.beacon = new;
    status = wlan_hdd_cfg80211_start_bss(pAdapter, params, NULL, 0, 0);

    EXIT();
    return status;
}

/**
 * wlan_hdd_cfg80211_change_beacon() - change beacon content in sap mode
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 * @params: Pointer to change beacon parameters
 *
 * Return: zero for success non-zero for failure
 */
static int wlan_hdd_cfg80211_change_beacon(struct wiphy *wiphy,
					struct net_device *dev,
					struct cfg80211_beacon_data *params)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_change_beacon(wiphy, dev, params);
	vos_ssr_unprotect(__func__);

	return ret;
}

static int __wlan_hdd_cfg80211_change_bss (struct wiphy *wiphy,
                                      struct net_device *dev,
                                      struct bss_parameters *params)
{
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    int ret = 0;
    eHalStatus halStatus;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_CHANGE_BSS,
                     pAdapter->sessionId, params->ap_isolate));
    hddLog(VOS_TRACE_LEVEL_INFO, FL("device_mode = %d, ap_isolate = %d"),
                               pAdapter->device_mode, params->ap_isolate);

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
       return ret;
    }

    if (!(pAdapter->device_mode == WLAN_HDD_SOFTAP ||
          pAdapter->device_mode == WLAN_HDD_P2P_GO)) {
        return -EOPNOTSUPP;
    }

    /* ap_isolate == -1 means that in change bss, upper layer doesn't
     * want to update this parameter */
    if (-1 != params->ap_isolate) {
        pAdapter->sessionCtx.ap.apDisableIntraBssFwd = !!params->ap_isolate;

        halStatus = sme_ApDisableIntraBssFwd(pHddCtx->hHal,
                    pAdapter->sessionId,
                    pAdapter->sessionCtx.ap.apDisableIntraBssFwd);
        if (!HAL_STATUS_SUCCESS(halStatus)) {
            ret = -EINVAL;
        }
    }

    EXIT();
    return ret;
}

static int
wlan_hdd_change_iface_to_adhoc(struct net_device *ndev,
                               tCsrRoamProfile *pRoamProfile,
                               enum nl80211_iftype type)
{
    hdd_adapter_t *pAdapter   = WLAN_HDD_GET_PRIV_PTR(ndev);
    hdd_context_t *pHddCtx    = WLAN_HDD_GET_CTX(pAdapter);
    hdd_config_t  *pConfig    = pHddCtx->cfg_ini;
    struct wireless_dev *wdev = ndev->ieee80211_ptr;

    pRoamProfile->BSSType = eCSR_BSS_TYPE_START_IBSS;
    pRoamProfile->phyMode = hdd_cfg_xlate_to_csr_phy_mode(pConfig->dot11Mode);
    pAdapter->device_mode = WLAN_HDD_IBSS;
    wdev->iftype = type;

    return 0;
}

static int wlan_hdd_change_iface_to_sta_mode(struct net_device *ndev,
                                                      enum nl80211_iftype type)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(ndev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    hdd_wext_state_t *wext;
    struct wireless_dev *wdev;
    VOS_STATUS status;

    ENTER();

    wdev = ndev->ieee80211_ptr;
    hdd_stop_adapter(pHddCtx, pAdapter, VOS_TRUE);
    hdd_deinit_adapter(pHddCtx, pAdapter, true);
    wdev->iftype = type;
    /*Check for sub-string p2p to confirm its a p2p interface*/
    if (NULL != strnstr(ndev->name, "p2p", 3)) {
        pAdapter->device_mode =
                        (type == NL80211_IFTYPE_STATION)?
                                     WLAN_HDD_P2P_DEVICE : WLAN_HDD_P2P_CLIENT;
    } else {
        pAdapter->device_mode =
                        (type == NL80211_IFTYPE_STATION) ?
                                  WLAN_HDD_INFRA_STATION : WLAN_HDD_P2P_CLIENT;
    }

    // set con_mode to STA only when no SAP concurrency mode
    if (!(hdd_get_concurrency_mode() & (VOS_SAP | VOS_P2P_GO)))
        hdd_set_conparam(0);
    pHddCtx->change_iface = type;
    memset(&pAdapter->sessionCtx, 0, sizeof(pAdapter->sessionCtx));
    hdd_set_station_ops(pAdapter->dev);
    status = hdd_init_station_mode(pAdapter);
    wext = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    wext->roamProfile.pAddIEScan = pAdapter->scan_info.scanAddIE.addIEdata;
    wext->roamProfile.nAddIEScanLength = pAdapter->scan_info.scanAddIE.length;
    EXIT();
    return status;
}

static int wlan_hdd_cfg80211_change_bss (struct wiphy *wiphy,
                                      struct net_device *dev,
                                      struct bss_parameters *params)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_change_bss(wiphy, dev, params);
    vos_ssr_unprotect(__func__);

    return ret;
}
/* FUNCTION: wlan_hdd_change_country_code_cd
*  to wait for country code completion
*/
void* wlan_hdd_change_country_code_cb(void *pAdapter)
{
    hdd_adapter_t *call_back_pAdapter = pAdapter;
    complete(&call_back_pAdapter->change_country_code);
    return NULL;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_change_iface
 * This function is used to set the interface type (INFRASTRUCTURE/ADHOC)
 */
static int __wlan_hdd_cfg80211_change_iface(struct wiphy *wiphy,
                                            struct net_device *ndev,
                                            enum nl80211_iftype type,
                                            u32 *flags,
                                            struct vif_params *params)
{
    struct wireless_dev *wdev;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(ndev);
    hdd_context_t *pHddCtx;
    tCsrRoamProfile *pRoamProfile = NULL;
    eCsrRoamBssType LastBSSType;
    hdd_config_t *pConfig = NULL;
    eMib_dot11DesiredBssType connectedBssType;
    unsigned long rc;
    VOS_STATUS vstatus;
    eHalStatus hstatus;
    int status;

    ENTER();

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return status;
    }

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_CHANGE_IFACE,
                     pAdapter->sessionId, type));

    hddLog(VOS_TRACE_LEVEL_INFO, FL("Device_mode = %d, IFTYPE = 0x%x"),
                                 pAdapter->device_mode, type);

    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_DEBUG, FL("Reached max concurrent connections"));
        return -EINVAL;
    }

    pConfig = pHddCtx->cfg_ini;
    wdev = ndev->ieee80211_ptr;

#ifdef WLAN_BTAMP_FEATURE
    if((NL80211_IFTYPE_P2P_CLIENT == type)||
       (NL80211_IFTYPE_ADHOC == type)||
       (NL80211_IFTYPE_AP == type)||
       (NL80211_IFTYPE_P2P_GO == type)) {
        pHddCtx->isAmpAllowed = VOS_FALSE;
        /* Stop AMP traffic */
        vstatus = WLANBAP_StopAmp();
        if (VOS_STATUS_SUCCESS != vstatus) {
            pHddCtx->isAmpAllowed = VOS_TRUE;
            hddLog(LOGP, FL("Failed to stop AMP"));
            return -EINVAL;
        }
    }
#endif /* WLAN_BTAMP_FEATURE */
    /* Reset the current device mode bit mask */
    wlan_hdd_clear_concurrency_mode(pHddCtx, pAdapter->device_mode);

    hdd_tdls_notify_mode_change(pAdapter, pHddCtx);

    if ((pAdapter->device_mode == WLAN_HDD_INFRA_STATION) ||
        (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT) ||
        (pAdapter->device_mode == WLAN_HDD_P2P_DEVICE)) {
        hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);

        pRoamProfile = &pWextState->roamProfile;
        LastBSSType = pRoamProfile->BSSType;

        switch (type) {
        case NL80211_IFTYPE_STATION:
        case NL80211_IFTYPE_P2P_CLIENT:
           vstatus = wlan_hdd_change_iface_to_sta_mode(ndev, type);
           if (vstatus != VOS_STATUS_SUCCESS)
               return -EINVAL;

#ifdef QCA_LL_TX_FLOW_CT
           if (pAdapter->tx_flow_timer_initialized == VOS_FALSE) {
              vos_timer_init(&pAdapter->tx_flow_control_timer,
                             VOS_TIMER_TYPE_SW,
                             hdd_tx_resume_timer_expired_handler,
                             pAdapter);
              pAdapter->tx_flow_timer_initialized = VOS_TRUE;
           }
           WLANTL_RegisterTXFlowControl(pHddCtx->pvosContext,
                            hdd_tx_resume_cb,
                            pAdapter->sessionId,
                           (void *)pAdapter);
#endif /* QCA_LL_TX_FLOW_CT */

           goto done;

        case NL80211_IFTYPE_ADHOC:
           hddLog(LOG1, FL("Setting interface Type to ADHOC"));
           wlan_hdd_change_iface_to_adhoc(ndev, pRoamProfile, type);
           break;

        case NL80211_IFTYPE_AP:
        case NL80211_IFTYPE_P2P_GO:
        {
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                   FL("Setting interface Type to %s"),
                   (type == NL80211_IFTYPE_AP) ? "SoftAP" : "P2pGo");

            /* Cancel any remain on channel for GO mode */
            if (NL80211_IFTYPE_P2P_GO == type) {
                wlan_hdd_cancel_existing_remain_on_channel(pAdapter);
            }

            if (NL80211_IFTYPE_AP == type) {
                /* As Loading WLAN Driver one interface being created for
                 * p2p device address. This will take one HW STA and the
                 * max number of clients that can connect to softAP will be
                 * reduced by one. so while changing the interface type to
                 * NL80211_IFTYPE_AP (SoftAP) remove p2p0 interface as it is
                 * not required in SoftAP mode.
                 */

                /* Get P2P Adapter */
                hdd_adapter_t  *pP2pAdapter = NULL;
                pP2pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_P2P_DEVICE);

                if (pP2pAdapter) {
                    hdd_stop_adapter(pHddCtx, pP2pAdapter, VOS_TRUE);
                    hdd_deinit_adapter(pHddCtx, pP2pAdapter, true);
                    hdd_close_adapter(pHddCtx, pP2pAdapter, VOS_TRUE);
                }
            }
            hdd_stop_adapter(pHddCtx, pAdapter, VOS_TRUE);

            /* De-init the adapter */
            hdd_deinit_adapter(pHddCtx, pAdapter, true);
            memset(&pAdapter->sessionCtx, 0, sizeof(pAdapter->sessionCtx));
            pAdapter->device_mode = (type == NL80211_IFTYPE_AP) ?
                                   WLAN_HDD_SOFTAP : WLAN_HDD_P2P_GO;

            /*
             * If Powersave Offload is enabled
             * Fw will take care incase of concurrency
             */
            if (!pHddCtx->cfg_ini->enablePowersaveOffload) {
                /* Disable BMPS and IMPS if enabled before starting Go */
                if (WLAN_HDD_P2P_GO == pAdapter->device_mode) {
                    if(VOS_STATUS_E_FAILURE ==
                           hdd_disable_bmps_imps(pHddCtx, WLAN_HDD_P2P_GO)) {
                           /* Fail to Exit BMPS */
                           VOS_ASSERT(0);
                    }
                }
            }

            if ((WLAN_HDD_SOFTAP == pAdapter->device_mode) &&
                (pConfig->apRandomBssidEnabled)) {
                /* To meet Android requirements create a randomized
                   MAC address of the form 02:1A:11:Fx:xx:xx */
                get_random_bytes(&ndev->dev_addr[3], 3);
                ndev->dev_addr[0] = 0x02;
                ndev->dev_addr[1] = 0x1A;
                ndev->dev_addr[2] = 0x11;
                ndev->dev_addr[3] |= 0xF0;
                memcpy(pAdapter->macAddressCurrent.bytes, ndev->dev_addr,
                       VOS_MAC_ADDR_SIZE);
                pr_info("wlan: Generated HotSpot BSSID "MAC_ADDRESS_STR"\n",
                        MAC_ADDR_ARRAY(ndev->dev_addr));
            }

            hdd_set_ap_ops(pAdapter->dev);

            /* This is for only SAP mode where users can
             * control country through ini.
             * P2P GO follows station country code
             * acquired during the STA scanning. */
            if ((NL80211_IFTYPE_AP == type) &&
               (memcmp(pConfig->apCntryCode,
                       CFG_AP_COUNTRY_CODE_DEFAULT, 3) != 0)) {
                hddLog(LOG1, FL("Setting country code from INI"));
                init_completion(&pAdapter->change_country_code);
                hstatus = sme_ChangeCountryCode(pHddCtx->hHal,
                                     (void *)(tSmeChangeCountryCallback)
                                      wlan_hdd_change_country_code_cb,
                                      pConfig->apCntryCode, pAdapter,
                                      pHddCtx->pvosContext,
                                      eSIR_FALSE, eSIR_TRUE);
                if (eHAL_STATUS_SUCCESS == hstatus) {
                    /* Wait for completion */
                    rc = wait_for_completion_timeout(
                                      &pAdapter->change_country_code,
                                      msecs_to_jiffies(WLAN_WAIT_TIME_COUNTRY));
                    if (!rc) {
                        hddLog(LOGE,
                               FL("SME Timed out while setting country code"));
                    }
                } else {
                    hddLog(LOGE, FL("SME Change Country code failed"));
                    return -EINVAL;
                }
            }

            vstatus = hdd_init_ap_mode(pAdapter);
            if (vstatus != VOS_STATUS_SUCCESS) {
                hddLog(LOGP, FL("Error initializing the ap mode"));
                return -EINVAL;
            }
            hdd_set_conparam(1);

#ifdef QCA_LL_TX_FLOW_CT
            if (pAdapter->tx_flow_timer_initialized == VOS_FALSE) {
               vos_timer_init(&pAdapter->tx_flow_control_timer,
                            VOS_TIMER_TYPE_SW,
                            hdd_softap_tx_resume_timer_expired_handler,
                            pAdapter);
               pAdapter->tx_flow_timer_initialized = VOS_TRUE;
            }
            WLANTL_RegisterTXFlowControl(pHddCtx->pvosContext,
                         hdd_softap_tx_resume_cb,
                         pAdapter->sessionId,
                         (void *)pAdapter);
#endif /* QCA_LL_TX_FLOW_CT */

            /* Interface type changed update in wiphy structure */
            if (wdev) {
                wdev->iftype = type;
                pHddCtx->change_iface = type;
            } else {
                hddLog(LOGE, FL("Wireless dev is NULL"));
                return -EINVAL;
            }
            goto done;
        }

        default:
            hddLog(LOGE, FL("Unsupported interface type (%d)"), type);
            return -EOPNOTSUPP;
        }
    } else if ((pAdapter->device_mode == WLAN_HDD_SOFTAP) ||
               (pAdapter->device_mode == WLAN_HDD_P2P_GO)) {
       switch (type) {
       case NL80211_IFTYPE_STATION:
       case NL80211_IFTYPE_P2P_CLIENT:
       case NL80211_IFTYPE_ADHOC:
          status = wlan_hdd_change_iface_to_sta_mode(ndev, type);
          if (status != VOS_STATUS_SUCCESS)
              return status;

#ifdef QCA_LL_TX_FLOW_CT
          if ((NL80211_IFTYPE_P2P_CLIENT == type) ||
              (NL80211_IFTYPE_STATION == type)) {
             if (pAdapter->tx_flow_timer_initialized == VOS_FALSE) {
                vos_timer_init(&pAdapter->tx_flow_control_timer,
                               VOS_TIMER_TYPE_SW,
                               hdd_tx_resume_timer_expired_handler,
                               pAdapter);
                pAdapter->tx_flow_timer_initialized = VOS_TRUE;
             }
             WLANTL_RegisterTXFlowControl(pHddCtx->pvosContext,
                               hdd_tx_resume_cb,
                               pAdapter->sessionId,
                              (void *)pAdapter);
          }
#endif /* QCA_LL_TX_FLOW_CT */

          /* FW will take care if PS offload is enabled. */
          if (pHddCtx->cfg_ini->enablePowersaveOffload)
              goto done;

          if (pHddCtx->hdd_wlan_suspended) {
                    hdd_set_pwrparams(pHddCtx);
          }
          hdd_enable_bmps_imps(pHddCtx);
          goto done;

        case NL80211_IFTYPE_AP:
        case NL80211_IFTYPE_P2P_GO:
           wdev->iftype = type;
           pAdapter->device_mode = (type == NL80211_IFTYPE_AP) ?
                                    WLAN_HDD_SOFTAP : WLAN_HDD_P2P_GO;
#ifdef QCA_LL_TX_FLOW_CT
           if (pAdapter->tx_flow_timer_initialized == VOS_FALSE) {
              vos_timer_init(&pAdapter->tx_flow_control_timer,
                             VOS_TIMER_TYPE_SW,
                             hdd_softap_tx_resume_timer_expired_handler,
                             pAdapter);
              pAdapter->tx_flow_timer_initialized = VOS_TRUE;
           }
           WLANTL_RegisterTXFlowControl(pHddCtx->pvosContext,
                         hdd_softap_tx_resume_cb,
                         pAdapter->sessionId,
                         (void *)pAdapter);
#endif /* QCA_LL_TX_FLOW_CT */
           goto done;

           default:
                hddLog(LOGE, FL("Unsupported interface type(%d)"), type);
                return -EOPNOTSUPP;
       }
    } else {
      hddLog(LOGE, FL("Unsupported device mode(%d)"), pAdapter->device_mode);
      return -EOPNOTSUPP;
    }

    if (LastBSSType != pRoamProfile->BSSType) {
        /* Interface type changed update in wiphy structure */
        wdev->iftype = type;

        /* The BSS mode changed, We need to issue disconnect
           if connected or in IBSS disconnect state */
        if (hdd_connGetConnectedBssType(
            WLAN_HDD_GET_STATION_CTX_PTR(pAdapter), &connectedBssType) ||
           (eCSR_BSS_TYPE_START_IBSS == LastBSSType)) {
            /* Need to issue a disconnect to CSR.*/
            INIT_COMPLETION(pAdapter->disconnect_comp_var);
            if (eHAL_STATUS_SUCCESS ==
                sme_RoamDisconnect(WLAN_HDD_GET_HAL_CTX(pAdapter),
                            pAdapter->sessionId,
                            eCSR_DISCONNECT_REASON_UNSPECIFIED)) {
                rc = wait_for_completion_timeout(
                              &pAdapter->disconnect_comp_var,
                              msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
                if (!rc) {
                    hddLog(LOGE,
                           FL("Wait on disconnect_comp_var failed"));
                 }
            }
        }
    }

done:
    /* Set bitmask based on updated value */
    wlan_hdd_set_concurrency_mode(pHddCtx, pAdapter->device_mode);

    /* Only STA mode support TM now
     * all other mode, TM feature should be disabled */
    if ((pHddCtx->cfg_ini->thermalMitigationEnable) &&
         (~VOS_STA & pHddCtx->concurrency_mode)) {
        hddDevTmLevelChangedHandler(pHddCtx->parent_dev, 0);
    }

#ifdef WLAN_BTAMP_FEATURE
    if ((NL80211_IFTYPE_STATION == type) && (pHddCtx->concurrency_mode <= 1) &&
       (pHddCtx->no_of_open_sessions[WLAN_HDD_INFRA_STATION] <= 1)) {
        /* We are ok to do AMP */
        pHddCtx->isAmpAllowed = VOS_TRUE;
    }
#endif /* WLAN_BTAMP_FEATURE */

#ifdef WLAN_FEATURE_LPSS
    wlan_hdd_send_all_scan_intf_info(pHddCtx);
#endif

    EXIT();
    return 0;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_change_iface
 * wrapper function to protect the actual implementation from SSR.
 */
static int wlan_hdd_cfg80211_change_iface(struct wiphy *wiphy,
                                          struct net_device *ndev,
                                          enum nl80211_iftype type,
                                          u32 *flags,
                                          struct vif_params *params)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_change_iface(wiphy, ndev, type, flags, params);
    vos_ssr_unprotect(__func__);

    return ret;
}

#ifdef FEATURE_WLAN_TDLS
static int wlan_hdd_tdls_add_station(struct wiphy *wiphy,
          struct net_device *dev, u8 *mac, bool update, tCsrStaParams *StaParams)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    VOS_STATUS status;
    hddTdlsPeer_t *pTdlsPeer;
    tANI_U16 numCurrTdlsPeers;
    unsigned long rc;
    long ret;

    ENTER();

    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
       return ret;
    }

    if ((eTDLS_SUPPORT_NOT_ENABLED == pHddCtx->tdls_mode) ||
        (eTDLS_SUPPORT_DISABLED == pHddCtx->tdls_mode))
    {
         VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                    "%s: TDLS mode is disabled OR not enabled in FW."
                    MAC_ADDRESS_STR " Request declined.",
                    __func__, MAC_ADDR_ARRAY(mac));
        return -ENOTSUPP;
    }

    pTdlsPeer = wlan_hdd_tdls_get_peer(pAdapter, mac);

    if ( NULL == pTdlsPeer ) {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
               "%s: " MAC_ADDRESS_STR " (update %d) not exist. return invalid",
               __func__, MAC_ADDR_ARRAY(mac), update);
        return -EINVAL;
    }

    /* in add station, we accept existing valid staId if there is */
    if ((0 == update) &&
        ((pTdlsPeer->link_status >= eTDLS_LINK_CONNECTING) ||
         (TDLS_STA_INDEX_VALID(pTdlsPeer->staId))))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                   "%s: " MAC_ADDRESS_STR
                   " link_status %d. staId %d. add station ignored.",
                   __func__, MAC_ADDR_ARRAY(mac), pTdlsPeer->link_status, pTdlsPeer->staId);
        return 0;
    }
    /* in change station, we accept only when staId is valid */
    if ((1 == update) &&
        ((pTdlsPeer->link_status > eTDLS_LINK_CONNECTING) ||
         (!TDLS_STA_INDEX_VALID(pTdlsPeer->staId))))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                   "%s: " MAC_ADDRESS_STR
                   " link status %d. staId %d. change station %s.",
                   __func__, MAC_ADDR_ARRAY(mac), pTdlsPeer->link_status, pTdlsPeer->staId,
                   (TDLS_STA_INDEX_VALID(pTdlsPeer->staId)) ? "ignored" : "declined");
        return (TDLS_STA_INDEX_VALID(pTdlsPeer->staId)) ? 0 : -EPERM;
    }

    /* when others are on-going, we want to change link_status to idle */
    if (NULL != wlan_hdd_tdls_is_progress(pHddCtx, mac, TRUE))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: " MAC_ADDRESS_STR
                   " TDLS setup is ongoing. Request declined.",
                   __func__, MAC_ADDR_ARRAY(mac));
        goto error;
    }

    /* first to check if we reached to maximum supported TDLS peer.
       TODO: for now, return -EPERM looks working fine,
       but need to check if any other errno fit into this category.*/
    numCurrTdlsPeers = wlan_hdd_tdlsConnectedPeers(pAdapter);
    if (pHddCtx->max_num_tdls_sta <= numCurrTdlsPeers)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: " MAC_ADDRESS_STR
                   " TDLS Max peer already connected. Request declined."
                   " Num of peers (%d), Max allowed (%d).",
                   __func__, MAC_ADDR_ARRAY(mac), numCurrTdlsPeers,
                   pHddCtx->max_num_tdls_sta);
        goto error;
    }
    else
    {
        hddTdlsPeer_t *pTdlsPeer;
        pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, mac, TRUE);
        if (pTdlsPeer && TDLS_IS_CONNECTED(pTdlsPeer))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: " MAC_ADDRESS_STR " already connected. Request declined.",
                       __func__, MAC_ADDR_ARRAY(mac));
            return -EPERM;
        }
    }
    if (0 == update)
        wlan_hdd_tdls_set_link_status(pAdapter,
                                      mac,
                                      eTDLS_LINK_CONNECTING,
                                      eTDLS_LINK_SUCCESS);

    /* debug code */
    if (NULL != StaParams)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                  "%s: TDLS Peer Parameters.", __func__);
        if(StaParams->htcap_present)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                      "ht_capa->cap_info: %0x", StaParams->HTCap.capInfo);
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                      "ht_capa->extended_capabilities: %0x",
                      StaParams->HTCap.extendedHtCapInfo);
        }
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                  "params->capability: %0x",StaParams->capability);
        VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                  "params->ext_capab_len: %0x",StaParams->extn_capability[0]);
        if(StaParams->vhtcap_present)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                      "rxMcsMap %x rxHighest %x txMcsMap %x txHighest %x",
                      StaParams->VHTCap.suppMcs.rxMcsMap, StaParams->VHTCap.suppMcs.rxHighest,
                      StaParams->VHTCap.suppMcs.txMcsMap, StaParams->VHTCap.suppMcs.txHighest);
        }
        {
            int i = 0;
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "Supported rates:");
            for (i = 0; i < sizeof(StaParams->supported_rates); i++)
               VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                          "[%d]: %x ", i, StaParams->supported_rates[i]);
        }
    }  /* end debug code */
    else if ((1 == update) && (NULL == StaParams))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s : update is true, but staParams is NULL. Error!", __func__);
        return -EPERM;
    }

    INIT_COMPLETION(pAdapter->tdls_add_station_comp);

    if (!update)
    {
        status = sme_AddTdlsPeerSta(WLAN_HDD_GET_HAL_CTX(pAdapter),
                pAdapter->sessionId, mac);
    }
    else
    {
        status = sme_ChangeTdlsPeerSta(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                       pAdapter->sessionId, mac, StaParams);
    }

     rc = wait_for_completion_timeout(&pAdapter->tdls_add_station_comp,
           msecs_to_jiffies(WAIT_TIME_TDLS_ADD_STA));

    if (!rc) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: timeout waiting for tdls add station indication",
                  __func__);
        return -EPERM;
    }

    if ( eHAL_STATUS_SUCCESS != pAdapter->tdlsAddStaStatus)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: Add Station is unsuccessful", __func__);
        return -EPERM;
    }

    return 0;

error:
    wlan_hdd_tdls_set_link_status(pAdapter,
                                  mac,
                                  eTDLS_LINK_IDLE,
                                  eTDLS_LINK_UNSPECIFIED);
    return -EPERM;

}
#endif

static bool wlan_hdd_is_duplicate_channel(tANI_U8 *arr,
                                          int index,
                                          tANI_U8 match)
{
    int i;
    for (i = 0; i < index; i++) {
        if (arr[i] == match)
            return TRUE;
    }
    return FALSE;
}

static int wlan_hdd_change_station(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         u8 *mac,
                                         struct station_parameters *params)
{
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_context_t *pHddCtx;
    hdd_station_ctx_t *pHddStaCtx;
    v_MACADDR_t STAMacAddress;
#ifdef FEATURE_WLAN_TDLS
    tCsrStaParams StaParams = {0};
    tANI_U8 isBufSta = 0;
    tANI_U8 isOffChannelSupported = 0;
#endif
    int ret;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CHANGE_STATION,
                     pAdapter->sessionId, params->listen_interval));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
       return ret;
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    vos_mem_copy(STAMacAddress.bytes, mac, sizeof(v_MACADDR_t));

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP) ||
        (pAdapter->device_mode == WLAN_HDD_P2P_GO)) {
        if (params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED)) {
            status = hdd_softap_change_STA_state( pAdapter, &STAMacAddress,
                                                  WLANTL_STA_AUTHENTICATED);

            if (status != VOS_STATUS_SUCCESS) {
                hddLog(VOS_TRACE_LEVEL_INFO,
                       FL("Not able to change TL state to AUTHENTICATED"));
                return -EINVAL;
            }
        }
    } else if ((pAdapter->device_mode == WLAN_HDD_INFRA_STATION) ||
               (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)) {
#ifdef FEATURE_WLAN_TDLS
        if (params->sta_flags_set & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
            StaParams.capability = params->capability;
            StaParams.uapsd_queues = params->uapsd_queues;
            StaParams.max_sp = params->max_sp;

            /* Convert (first channel , number of channels) tuple to
             * the total list of channels. This goes with the assumption
             * that if the first channel is < 14, then the next channels
             * are an incremental of 1 else an incremental of 4 till the number
             * of channels.
             */
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                      "%s: params->supported_channels_len: %d",
                      __func__, params->supported_channels_len);
            if (0 != params->supported_channels_len) {
                int i = 0, j = 0, k = 0, no_of_channels = 0;
                int num_unique_channels;
                int next;
                for (i = 0 ; i < params->supported_channels_len &&
                             j < SIR_MAC_MAX_SUPP_CHANNELS; i += 2) {
                    int wifi_chan_index;
                    if (!wlan_hdd_is_duplicate_channel(
                                            StaParams.supported_channels,
                                            j,
                                            params->supported_channels[i])){
                        StaParams.supported_channels[j] =
                                  params->supported_channels[i];
                    } else {
                        continue;
                    }
                    wifi_chan_index =
                        ((StaParams.supported_channels[j] <= HDD_CHANNEL_14 ) ? 1 : 4 );
                    no_of_channels = params->supported_channels[i + 1];

                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                              "%s: i: %d, j: %d, k: %d, StaParams.supported_channels[%d]: %d, wifi_chan_index: %d, no_of_channels: %d",
                              __func__, i, j, k, j,
                             StaParams.supported_channels[j],
                             wifi_chan_index,
                             no_of_channels);

                    for (k = 1; k <= no_of_channels &&
                                j <  SIR_MAC_MAX_SUPP_CHANNELS - 1; k++) {
                        next = StaParams.supported_channels[j] + wifi_chan_index;
                        if (!wlan_hdd_is_duplicate_channel(
                                             StaParams.supported_channels,
                                             j+1,
                                             next)){
                            StaParams.supported_channels[j + 1] = next;
                        } else {
                            continue;
                        }
                        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                  "%s: i: %d, j: %d, k: %d, StaParams.supported_channels[%d]: %d",
                                  __func__, i, j, k, j+1,
                                  StaParams.supported_channels[j+1]);
                        j += 1;
                    }
                }
                num_unique_channels = j+1;

                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                              "%s: Unique Channel List", __func__);
                for (i = 0; i < num_unique_channels; i++) {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                             "%s: StaParams.supported_channels[%d]: %d,",
                             __func__, i, StaParams.supported_channels[i]);
                }
                /* num of channels should not be more than max
                 * number of channels in 2.4GHz and 5GHz
                 */
                if (MAX_CHANNEL < num_unique_channels)
                    num_unique_channels = MAX_CHANNEL;

                StaParams.supported_channels_len = num_unique_channels;
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                          "%s: After removing duplcates StaParams.supported_channels_len: %d",
                          __func__, StaParams.supported_channels_len);
            }
            vos_mem_copy(StaParams.supported_oper_classes,
                         params->supported_oper_classes,
                         params->supported_oper_classes_len);
            StaParams.supported_oper_classes_len  =
                                             params->supported_oper_classes_len;

            if (0 != params->ext_capab_len)
                vos_mem_copy(StaParams.extn_capability, params->ext_capab,
                             sizeof(StaParams.extn_capability));

            if (NULL != params->ht_capa) {
                StaParams.htcap_present = 1;
                vos_mem_copy(&StaParams.HTCap, params->ht_capa, sizeof(tSirHTCap));
            }

            StaParams.supported_rates_len = params->supported_rates_len;

            /*
             * Note : The Maximum sizeof supported_rates sent by the Supplicant
             * is 32. The supported_rates array, for all the structures
             * propagating till Add Sta to the firmware has to be modified,
             * if the supplicant (ieee80211) is modified to send more rates.
             */

            /* To avoid Data Corruption, set to max length
               to SIR_MAC_MAX_SUPP_RATES */
            if (StaParams.supported_rates_len > SIR_MAC_MAX_SUPP_RATES)
                StaParams.supported_rates_len = SIR_MAC_MAX_SUPP_RATES;

            if (0 != StaParams.supported_rates_len) {
                int i = 0;
                vos_mem_copy(StaParams.supported_rates, params->supported_rates,
                             StaParams.supported_rates_len);
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                           "Supported Rates with Length %d", StaParams.supported_rates_len);
                for (i=0; i < StaParams.supported_rates_len; i++)
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                               "[%d]: %0x", i, StaParams.supported_rates[i]);
            }

            if (NULL != params->vht_capa) {
                StaParams.vhtcap_present = 1;
                vos_mem_copy(&StaParams.VHTCap, params->vht_capa, sizeof(tSirVHTCap));
            }

            if (0 != params->ext_capab_len ) {
                /*Define A Macro : TODO Sunil*/
                if ((1<<4) & StaParams.extn_capability[3]) {
                    isBufSta = 1;
                }
                /* TDLS Channel Switching Support */
                if ((1<<6) & StaParams.extn_capability[3]) {
                    isOffChannelSupported = 1;
                }
            }

            status = wlan_hdd_tdls_set_peer_caps(pAdapter, mac,
                                                  &StaParams, isBufSta,
                                                  isOffChannelSupported);
            if (VOS_STATUS_SUCCESS != status) {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                       FL("wlan_hdd_tdls_set_peer_caps failed!"));
                return -EINVAL;
            }

            status = wlan_hdd_tdls_add_station(wiphy, dev, mac, 1, &StaParams);
            if (VOS_STATUS_SUCCESS != status) {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                       FL("wlan_hdd_tdls_add_station failed!"));
                return -EINVAL;
            }
        }
#endif
    }
    EXIT();
    return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_add_key
 * This function is used to initialize the key information
 */
static int __wlan_hdd_cfg80211_add_key( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      u8 key_index, bool pairwise,
                                      const u8 *mac_addr,
                                      struct key_params *params
                                      )
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( ndev );
    tCsrRoamSetKey  setKey;
    u8 groupmacaddr[VOS_MAC_ADDR_SIZE] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    int status;
    v_U32_t roamId= 0xFF;
#ifndef WLAN_FEATURE_MBSSID
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
#endif
    hdd_hostapd_state_t *pHostapdState;
    eHalStatus halStatus;
    hdd_context_t *pHddCtx;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_ADD_KEY,
                     pAdapter->sessionId, params->key_len));
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %d",
            __func__, pAdapter->device_mode);

    if (CSR_MAX_NUM_KEY <= key_index)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid key index %d", __func__,
                key_index);

        return -EINVAL;
    }

    if (CSR_MAX_KEY_LEN < params->key_len)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid key length %d", __func__,
                params->key_len);

        return -EINVAL;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
           "%s: called with key index = %d & key length %d",
           __func__, key_index, params->key_len);

    /*extract key idx, key len and key*/
    vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));
    setKey.keyId = key_index;
    setKey.keyLength = params->key_len;
    vos_mem_copy(&setKey.Key[0],params->key, params->key_len);

    switch (params->cipher)
    {
        case WLAN_CIPHER_SUITE_WEP40:
            setKey.encType = eCSR_ENCRYPT_TYPE_WEP40_STATICKEY;
            break;

        case WLAN_CIPHER_SUITE_WEP104:
            setKey.encType = eCSR_ENCRYPT_TYPE_WEP104_STATICKEY;
            break;

        case WLAN_CIPHER_SUITE_TKIP:
            {
                u8 *pKey = &setKey.Key[0];
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
                vos_mem_copy(pKey, params->key, 16);

                /*Copy the rx mic first*/
                vos_mem_copy(&pKey[16], &params->key[24], 8);

                /*Copy the tx mic */
                vos_mem_copy(&pKey[24], &params->key[16], 8);


                break;
            }

        case WLAN_CIPHER_SUITE_CCMP:
            setKey.encType = eCSR_ENCRYPT_TYPE_AES;
            break;

#ifdef FEATURE_WLAN_WAPI
        case WLAN_CIPHER_SUITE_SMS4:
            {
                vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));
                wlan_hdd_cfg80211_set_key_wapi(pAdapter, key_index, mac_addr,
                        params->key, params->key_len);
                return 0;
            }
#endif

#ifdef FEATURE_WLAN_ESE
        case WLAN_CIPHER_SUITE_KRK:
            setKey.encType = eCSR_ENCRYPT_TYPE_KRK;
            break;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        case WLAN_CIPHER_SUITE_BTK:
            setKey.encType = eCSR_ENCRYPT_TYPE_BTK;
            break;
#endif
#endif

#ifdef WLAN_FEATURE_11W
        case WLAN_CIPHER_SUITE_AES_CMAC:
            setKey.encType = eCSR_ENCRYPT_TYPE_AES_CMAC;
            break;
#endif

        default:
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: unsupported cipher type %u",
                    __func__, params->cipher);
            return -EOPNOTSUPP;
    }

    hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: encryption type %d",
            __func__, setKey.encType);

    if (!pairwise)
    {
        /* set group key*/
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s- %d: setting Broadcast key",
                __func__, __LINE__);
        setKey.keyDirection = eSIR_RX_ONLY;
        vos_mem_copy(setKey.peerMac,groupmacaddr, VOS_MAC_ADDR_SIZE);
    }
    else
    {
        /* set pairwise key*/
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s- %d: setting pairwise key",
                __func__, __LINE__);
        setKey.keyDirection = eSIR_TX_RX;
        vos_mem_copy(setKey.peerMac, mac_addr, VOS_MAC_ADDR_SIZE);
    }
    if ((WLAN_HDD_IBSS == pAdapter->device_mode) && !pairwise)
    {
        /* if a key is already installed, block all subsequent ones */
        if (pAdapter->sessionCtx.station.ibss_enc_key_installed) {
            hddLog(VOS_TRACE_LEVEL_INFO_MED,
                   "%s: IBSS key installed already", __func__);
            return 0;
        }

        setKey.keyDirection = eSIR_TX_RX;
        /*Set the group key*/
        status = sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
            pAdapter->sessionId, &setKey, &roamId );

        if ( 0 != status )
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: sme_RoamSetKey failed, returned %d", __func__, status);
            return -EINVAL;
        }
        /*Save the keys here and call sme_RoamSetKey for setting
          the PTK after peer joins the IBSS network*/
        vos_mem_copy(&pAdapter->sessionCtx.station.ibss_enc_key,
                                    &setKey, sizeof(tCsrRoamSetKey));

        pAdapter->sessionCtx.station.ibss_enc_key_installed = 1;
        return status;
    }
    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP) ||
           (pAdapter->device_mode == WLAN_HDD_P2P_GO))
    {
        pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
        if( pHostapdState->bssState == BSS_START )
        {
#ifdef WLAN_FEATURE_MBSSID
            status = WLANSAP_SetKeySta( WLAN_HDD_GET_SAP_CTX_PTR(pAdapter),
                                        &setKey);
#else
            status = WLANSAP_SetKeySta( pVosContext, &setKey);
#endif
            if ( status != eHAL_STATUS_SUCCESS )
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                        "[%4d] WLANSAP_SetKeySta returned ERROR status= %d",
                        __LINE__, status );
            }
        }

        /* Saving WEP keys */
        else if( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY  == setKey.encType ||
                eCSR_ENCRYPT_TYPE_WEP104_STATICKEY  == setKey.encType  )
        {
            //Save the wep key in ap context. Issue setkey after the BSS is started.
            hdd_ap_ctx_t *pAPCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
            vos_mem_copy(&pAPCtx->wepKey[key_index], &setKey, sizeof(tCsrRoamSetKey));
        }
        else
        {
            //Save the key in ap context. Issue setkey after the BSS is started.
            hdd_ap_ctx_t *pAPCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
            vos_mem_copy(&pAPCtx->groupKey, &setKey, sizeof(tCsrRoamSetKey));
        }
    }
    else if ( (pAdapter->device_mode == WLAN_HDD_INFRA_STATION) ||
              (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT) )
    {
        hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
        hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

        if (!pairwise)
        {
            /* set group key*/
            if (pHddStaCtx->roam_info.deferKeyComplete)
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                           "%s- %d: Perform Set key Complete",
                           __func__, __LINE__);
                hdd_PerformRoamSetKeyComplete(pAdapter);
            }
        }

        pWextState->roamProfile.Keys.KeyLength[key_index] = (u8)params->key_len;

        pWextState->roamProfile.Keys.defaultIndex = key_index;


        vos_mem_copy(&pWextState->roamProfile.Keys.KeyMaterial[key_index][0],
                params->key, params->key_len);


        pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_SETTING_KEY;

        hddLog(VOS_TRACE_LEVEL_INFO_MED,
                "%s: set key for peerMac %2x:%2x:%2x:%2x:%2x:%2x, direction %d",
                __func__, setKey.peerMac[0], setKey.peerMac[1],
                setKey.peerMac[2], setKey.peerMac[3],
                setKey.peerMac[4], setKey.peerMac[5],
                setKey.keyDirection);


#ifdef WLAN_FEATURE_VOWIFI_11R
        /* The supplicant may attempt to set the PTK once pre-authentication
           is done. Save the key in the UMAC and include it in the ADD BSS
           request */
        halStatus = sme_FTUpdateKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
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

        /* issue set key request to SME*/
        status = sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
                pAdapter->sessionId, &setKey, &roamId );

        if ( 0 != status )
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: sme_RoamSetKey failed, returned %d", __func__, status);
            pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
            return -EINVAL;
        }


        /* in case of IBSS as there was no information available about WEP keys during
         * IBSS join, group key initialized with NULL key, so re-initialize group key
         * with correct value*/
        if ( (eCSR_BSS_TYPE_START_IBSS == pWextState->roamProfile.BSSType) &&
            !(  ( IW_AUTH_KEY_MGMT_802_1X
                    == (pWextState->authKeyMgmt & IW_AUTH_KEY_MGMT_802_1X))
                    && (eCSR_AUTH_TYPE_OPEN_SYSTEM == pHddStaCtx->conn_info.authType)
                 )
                &&
                (  (WLAN_CIPHER_SUITE_WEP40 == params->cipher)
                   || (WLAN_CIPHER_SUITE_WEP104 == params->cipher)
                )
           )
        {
            setKey.keyDirection = eSIR_RX_ONLY;
            vos_mem_copy(setKey.peerMac,groupmacaddr, VOS_MAC_ADDR_SIZE);

            hddLog(VOS_TRACE_LEVEL_INFO_MED,
                    "%s: set key peerMac %2x:%2x:%2x:%2x:%2x:%2x, direction %d",
                    __func__, setKey.peerMac[0], setKey.peerMac[1],
                    setKey.peerMac[2], setKey.peerMac[3],
                    setKey.peerMac[4], setKey.peerMac[5],
                    setKey.keyDirection);

            status = sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
                    pAdapter->sessionId, &setKey, &roamId );

            if ( 0 != status )
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: sme_RoamSetKey failed for group key (IBSS), returned %d",
                        __func__, status);
                pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
                return -EINVAL;
            }
        }
    }

    return 0;
}

static int wlan_hdd_cfg80211_add_key( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      u8 key_index, bool pairwise,
                                      const u8 *mac_addr,
                                      struct key_params *params
                                      )
{
    int ret;
    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_add_key(wiphy, ndev, key_index, pairwise,
                                      mac_addr, params);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_get_key
 * This function is used to get the key information
 */
static int __wlan_hdd_cfg80211_get_key(
                        struct wiphy *wiphy,
                        struct net_device *ndev,
                        u8 key_index, bool pairwise,
                        const u8 *mac_addr, void *cookie,
                        void (*callback)(void *cookie, struct key_params*)
                        )
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( ndev );
    hdd_wext_state_t *pWextState= WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    tCsrRoamProfile *pRoamProfile = &(pWextState->roamProfile);
    struct key_params params;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_GET_KEY,
                     pAdapter->sessionId, params.cipher));
    hddLog(VOS_TRACE_LEVEL_INFO, FL("Device_mode = %d"), pAdapter->device_mode);

    memset(&params, 0, sizeof(params));

    if (CSR_MAX_NUM_KEY <= key_index) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("invalid key index %d"), key_index);
        return -EINVAL;
    }

    switch (pRoamProfile->EncryptionType.encryptionType[0]) {
    case eCSR_ENCRYPT_TYPE_NONE:
         params.cipher = IW_AUTH_CIPHER_NONE;
         break;

    case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
    case eCSR_ENCRYPT_TYPE_WEP40:
         params.cipher = WLAN_CIPHER_SUITE_WEP40;
         break;

    case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
    case eCSR_ENCRYPT_TYPE_WEP104:
         params.cipher = WLAN_CIPHER_SUITE_WEP104;
         break;

    case eCSR_ENCRYPT_TYPE_TKIP:
         params.cipher = WLAN_CIPHER_SUITE_TKIP;
         break;

    case eCSR_ENCRYPT_TYPE_AES:
         params.cipher = WLAN_CIPHER_SUITE_AES_CMAC;
         break;

    default:
         params.cipher = IW_AUTH_CIPHER_NONE;
         break;
    }

    params.key_len = pRoamProfile->Keys.KeyLength[key_index];
    params.seq_len = 0;
    params.seq = NULL;
    params.key = &pRoamProfile->Keys.KeyMaterial[key_index][0];
    callback(cookie, &params);

    EXIT();
    return 0;
}

static int wlan_hdd_cfg80211_get_key(
                        struct wiphy *wiphy,
                        struct net_device *ndev,
                        u8 key_index, bool pairwise,
                        const u8 *mac_addr, void *cookie,
                        void (*callback)(void *cookie, struct key_params*)
                        )
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_get_key(wiphy, ndev, key_index, pairwise,
                                    mac_addr, cookie, callback);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_del_key
 * This function is used to delete the key information
 */
static int wlan_hdd_cfg80211_del_key( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      u8 key_index,
                                      bool pairwise,
                                      const u8 *mac_addr
                                    )
{
    int status = 0;

    //This code needs to be revisited. There is sme_removeKey API, we should
    //plan to use that. After the change to use correct index in setkey,
    //it is observed that this is invalidating peer
    //key index whenever re-key is done. This is affecting data link.
    //It should be ok to ignore del_key.
#if 0
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( ndev );
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;
    u8 groupmacaddr[VOS_MAC_ADDR_SIZE] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    tCsrRoamSetKey  setKey;
    v_U32_t roamId= 0xFF;

    ENTER();

    hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: device_mode = %d",
                                     __func__,pAdapter->device_mode);

    if (CSR_MAX_NUM_KEY <= key_index)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid key index %d", __func__,
                key_index);

        return -EINVAL;
    }

    vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));
    setKey.keyId = key_index;

    if (mac_addr)
        vos_mem_copy(setKey.peerMac, mac_addr, VOS_MAC_ADDR_SIZE);
    else
        vos_mem_copy(setKey.peerMac, groupmacaddr, VOS_MAC_ADDR_SIZE);

    setKey.encType = eCSR_ENCRYPT_TYPE_NONE;

    if ((pAdapter->device_mode == WLAN_HDD_SOFTAP)
      || (pAdapter->device_mode == WLAN_HDD_P2P_GO)
       )
    {

        hdd_hostapd_state_t *pHostapdState =
                                  WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);
        if( pHostapdState->bssState == BSS_START)
        {
#ifdef WLAN_FEATURE_MBSSID
            status = WLANSAP_SetKeySta( WLAN_HDD_GET_SAP_CTX_PTR(pAdapter),
                                        &setKey);
#else
            status = WLANSAP_SetKeySta( pVosContext, &setKey);
#endif

            if ( status != eHAL_STATUS_SUCCESS )
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "[%4d] WLANSAP_SetKeySta returned ERROR status= %d",
                     __LINE__, status );
            }
        }
    }
    else if ( (pAdapter->device_mode == WLAN_HDD_INFRA_STATION)
           || (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)
            )
    {
        hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

        pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_SETTING_KEY;

        hddLog(VOS_TRACE_LEVEL_INFO_MED,
                "%s: delete key for peerMac %2x:%2x:%2x:%2x:%2x:%2x",
                __func__, setKey.peerMac[0], setKey.peerMac[1],
                setKey.peerMac[2], setKey.peerMac[3],
                setKey.peerMac[4], setKey.peerMac[5]);
        if(pAdapter->sessionCtx.station.conn_info.connState ==
                                       eConnectionState_Associated)
        {
            status = sme_RoamSetKey( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                   pAdapter->sessionId, &setKey, &roamId );

            if ( 0 != status )
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: sme_RoamSetKey failure, returned %d",
                                                     __func__, status);
                pHddStaCtx->roam_info.roamingState = HDD_ROAM_STATE_NONE;
                return -EINVAL;
            }
        }
    }
#endif
    EXIT();
    return status;
}

#ifdef NL80211_KEY_LEN_PMK /* kernel supports key mgmt offload */
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static int wlan_hdd_cfg80211_key_mgmt_set_pmk(struct wiphy *wiphy,
                                              struct net_device *ndev,
                                              const u8 *pmk, size_t pmk_len)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(ndev);
    hdd_wext_state_t *pWextState;
    hdd_station_ctx_t *pHddStaCtx;

    ENTER();
    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    if (pmk) {
        tANI_U8 localPmk [SIR_ROAM_SCAN_PSK_SIZE];
        if ((pWextState->authKeyMgmt & IW_AUTH_KEY_MGMT_802_1X)
#ifdef FEATURE_WLAN_ESE
             && (!pWextState->isESEConnection)
#endif
           ) {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                       "%s: calling sme_RoamSetPSK_PMK \n", __func__);
            vos_mem_copy(localPmk, pmk, SIR_ROAM_SCAN_PSK_SIZE);
            sme_RoamSetPSK_PMK(WLAN_HDD_GET_HAL_CTX(pAdapter),
                               pAdapter->sessionId, localPmk, pmk_len);
            return VOS_STATUS_SUCCESS;
        }
    }
    return VOS_STATUS_SUCCESS;
}
#endif
#endif

/*
 * FUNCTION: __wlan_hdd_cfg80211_set_default_key
 * This function is used to set the default tx key index
 */
static int __wlan_hdd_cfg80211_set_default_key( struct wiphy *wiphy,
                                              struct net_device *ndev,
                                              u8 key_index,
                                              bool unicast, bool multicast)
{
    hdd_adapter_t     *pAdapter   = WLAN_HDD_GET_PRIV_PTR(ndev);
    hdd_wext_state_t  *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t     *pHddCtx;
    int status;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_DEFAULT_KEY,
                     pAdapter->sessionId, key_index));

    hddLog(LOG1, FL("Device_mode = %d key_index = %d"),
                 pAdapter->device_mode, key_index);

    if (CSR_MAX_NUM_KEY <= key_index) {
        hddLog(LOGE, FL("Invalid key index %d"), key_index);
        return -EINVAL;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return status;
    }

    if ((pAdapter->device_mode == WLAN_HDD_INFRA_STATION) ||
        (pAdapter->device_mode == WLAN_HDD_P2P_CLIENT)) {
        if ((eCSR_ENCRYPT_TYPE_TKIP !=
                pHddStaCtx->conn_info.ucEncryptionType) &&
            (eCSR_ENCRYPT_TYPE_AES !=
                pHddStaCtx->conn_info.ucEncryptionType)) {
            /* If default key index is not same as previous one,
             * then update the default key index */

            tCsrRoamSetKey  setKey;
            v_U32_t roamId= 0xFF;
            tCsrKeys *Keys = &pWextState->roamProfile.Keys;

            hddLog(LOG2, FL("Default tx key index %d"), key_index);

            Keys->defaultIndex = (u8)key_index;
            vos_mem_zero(&setKey, sizeof(tCsrRoamSetKey));
            setKey.keyId = key_index;
            setKey.keyLength = Keys->KeyLength[key_index];

            vos_mem_copy(&setKey.Key[0],
                    &Keys->KeyMaterial[key_index][0],
                    Keys->KeyLength[key_index]);

            setKey.keyDirection = eSIR_TX_RX;

            vos_mem_copy(setKey.peerMac,
                    &pHddStaCtx->conn_info.bssId[0], VOS_MAC_ADDR_SIZE);

            if (Keys->KeyLength[key_index] == CSR_WEP40_KEY_LEN &&
               pWextState->roamProfile.EncryptionType.encryptionType[0] ==
               eCSR_ENCRYPT_TYPE_WEP104) {
                /*
                 * In the case of dynamic wep supplicant hardcodes DWEP type
                 * to eCSR_ENCRYPT_TYPE_WEP104 even though ap is configured for
                 * WEP-40 encryption. In this case the key length is 5 but the
                 * encryption type is 104 hence checking the key length(5) and
                 * encryption type(104) and switching encryption type to 40.
                 */
                pWextState->roamProfile.EncryptionType.encryptionType[0] =
                   eCSR_ENCRYPT_TYPE_WEP40;
                pWextState->roamProfile.mcEncryptionType.encryptionType[0] =
                   eCSR_ENCRYPT_TYPE_WEP40;
            }

            setKey.encType =
                pWextState->roamProfile.EncryptionType.encryptionType[0];

            /* Issue set key request */
            status = sme_RoamSetKey(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                    pAdapter->sessionId, &setKey, &roamId);

            if (0 != status) {
                hddLog(LOGE, FL("sme_RoamSetKey failed, returned %d"), status);
                return -EINVAL;
            }
        }
    } else if ( WLAN_HDD_SOFTAP == pAdapter->device_mode ) {
        /* In SoftAp mode setting key direction for default mode */
        if ((eCSR_ENCRYPT_TYPE_TKIP !=
                pWextState->roamProfile.EncryptionType.encryptionType[0]) &&
             (eCSR_ENCRYPT_TYPE_AES !=
                pWextState->roamProfile.EncryptionType.encryptionType[0])) {
            /* Saving key direction for default key index to TX default */
            hdd_ap_ctx_t *pAPCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
            pAPCtx->wepKey[key_index].keyDirection = eSIR_TX_DEFAULT;
        }
    }

    EXIT();
    return status;
}

static int wlan_hdd_cfg80211_set_default_key( struct wiphy *wiphy,
                                              struct net_device *ndev,
                                              u8 key_index,
                                              bool unicast, bool multicast)
{
    int ret;
    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_set_default_key(wiphy, ndev, key_index, unicast,
                                              multicast);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_update_bss_list
 * This function is used to inform nl80211 interface that BSS might have
 * been lost.
 */
struct cfg80211_bss* wlan_hdd_cfg80211_update_bss_list(
   hdd_adapter_t *pAdapter, tCsrRoamInfo *pRoamInfo)
{
    struct net_device *dev = pAdapter->dev;
    struct wireless_dev *wdev = dev->ieee80211_ptr;
    struct wiphy *wiphy = wdev->wiphy;
    tSirBssDescription *pBssDesc = pRoamInfo->pBssDesc;
    int chan_no;
    unsigned int freq;
    struct ieee80211_channel *chan;
    struct cfg80211_bss *bss = NULL;

    ENTER();

    if (NULL == pBssDesc) {
        hddLog(LOGE, FL("pBssDesc is NULL"));
        return bss;
    }

    if (NULL == pRoamInfo->pProfile) {
        hddLog(LOGE, FL("Roam profile is NULL"));
        return bss;
    }

    chan_no = pBssDesc->channelId;

    if (chan_no <= ARRAY_SIZE(hdd_channels_2_4_GHZ)) {
        freq = ieee80211_channel_to_frequency(chan_no, IEEE80211_BAND_2GHZ);
    } else {
        freq = ieee80211_channel_to_frequency(chan_no, IEEE80211_BAND_5GHZ);
    }

    chan = __ieee80211_get_channel(wiphy, freq);

    if (!chan) {
       hddLog(LOGE, FL("chan pointer is NULL"));
       return NULL;
    }

    bss = cfg80211_get_bss(wiphy, chan, pBssDesc->bssId,
                           &pRoamInfo->pProfile->SSIDs.SSIDList->SSID.ssId[0],
                           pRoamInfo->pProfile->SSIDs.SSIDList->SSID.length,
                           WLAN_CAPABILITY_ESS, WLAN_CAPABILITY_ESS);
    if (bss == NULL) {
        hddLog(LOGE, FL("BSS not present"));
    } else {
        hddLog(LOG1, FL("cfg80211_unlink_bss called for BSSID "
               MAC_ADDRESS_STR), MAC_ADDR_ARRAY(pBssDesc->bssId));
        cfg80211_unlink_bss(wiphy, bss);
    }
    return bss;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_inform_bss_frame
 * This function is used to inform the BSS details to nl80211 interface.
 */
struct cfg80211_bss*
wlan_hdd_cfg80211_inform_bss_frame( hdd_adapter_t *pAdapter,
                                    tSirBssDescription *bss_desc
                                    )
{
    /*
      cfg80211_inform_bss() is not updating ie field of bss entry, if entry
      already exists in bss data base of cfg80211 for that particular BSS ID.
      Using cfg80211_inform_bss_frame to update the bss entry instead of
      cfg80211_inform_bss, But this call expects mgmt packet as input. As of
      now there is no possibility to get the mgmt(probe response) frame from PE,
      converting bss_desc to ieee80211_mgmt(probe response) and passing to
      cfg80211_inform_bss_frame.
     */
    struct net_device *dev = pAdapter->dev;
    struct wireless_dev *wdev = dev->ieee80211_ptr;
    struct wiphy *wiphy = wdev->wiphy;
    int chan_no = bss_desc->channelId;
#ifdef WLAN_ENABLE_AGEIE_ON_SCAN_RESULTS
    qcom_ie_age *qie_age = NULL;
    int ie_length = GET_IE_LEN_IN_BSS_DESC( bss_desc->length ) + sizeof(qcom_ie_age);
#else
    int ie_length = GET_IE_LEN_IN_BSS_DESC( bss_desc->length );
#endif
    const char *ie =
        ((ie_length != 0) ? (const char *)&bss_desc->ieFields: NULL);
    unsigned int freq;
    struct ieee80211_channel *chan;
    struct ieee80211_mgmt *mgmt = NULL;
    struct cfg80211_bss *bss_status = NULL;
    size_t frame_len = sizeof (struct ieee80211_mgmt) + ie_length;
    int rssi = 0;
    hdd_context_t *pHddCtx;
    int status;
#ifdef CONFIG_CNSS
    struct timespec ts;
#endif
    hdd_config_t *cfg_param = NULL;

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return NULL;
    }

    cfg_param = pHddCtx->cfg_ini;
    mgmt = kzalloc((sizeof (struct ieee80211_mgmt) + ie_length), GFP_KERNEL);
    if (!mgmt) {
        hddLog(LOGE, FL("memory allocation failed"));
        return NULL;
    }

    memcpy(mgmt->bssid, bss_desc->bssId, ETH_ALEN);

#ifdef CONFIG_CNSS
    /* Android does not want the time stamp from the frame.
       Instead it wants a monotonic increasing value */
    cnss_get_monotonic_boottime(&ts);
    mgmt->u.probe_resp.timestamp =
         ((u64)ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
#else
    /* keep old behavior for non-open source (for now) */
    memcpy(&mgmt->u.probe_resp.timestamp, bss_desc->timeStamp,
            sizeof (bss_desc->timeStamp));

#endif

    mgmt->u.probe_resp.beacon_int = bss_desc->beaconInterval;
    mgmt->u.probe_resp.capab_info = bss_desc->capabilityInfo;

#ifdef WLAN_ENABLE_AGEIE_ON_SCAN_RESULTS
    /* GPS Requirement: need age ie per entry. Using vendor specific. */
    /* Assuming this is the last IE, copy at the end */
    ie_length           -=sizeof(qcom_ie_age);
    qie_age =  (qcom_ie_age *)(mgmt->u.probe_resp.variable + ie_length);
    qie_age->element_id = QCOM_VENDOR_IE_ID;
    qie_age->len        = QCOM_VENDOR_IE_AGE_LEN;
    qie_age->oui_1      = QCOM_OUI1;
    qie_age->oui_2      = QCOM_OUI2;
    qie_age->oui_3      = QCOM_OUI3;
    qie_age->type       = QCOM_VENDOR_IE_AGE_TYPE;
    qie_age->age        = vos_timer_get_system_ticks() - bss_desc->nReceivedTime;
    qie_age->tsf_delta  = bss_desc->tsf_delta;
#endif

    memcpy(mgmt->u.probe_resp.variable, ie, ie_length);
    if (bss_desc->fProbeRsp) {
         mgmt->frame_control |=
                   (u16)(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_PROBE_RESP);
    } else {
         mgmt->frame_control |=
                   (u16)(IEEE80211_FTYPE_MGMT | IEEE80211_STYPE_BEACON);
    }

    if (chan_no <= ARRAY_SIZE(hdd_channels_2_4_GHZ) &&
        (wiphy->bands[IEEE80211_BAND_2GHZ] != NULL)) {
        freq = ieee80211_channel_to_frequency(chan_no, IEEE80211_BAND_2GHZ);
    } else if ((chan_no > ARRAY_SIZE(hdd_channels_2_4_GHZ)) &&
               (wiphy->bands[IEEE80211_BAND_5GHZ] != NULL)) {
        freq = ieee80211_channel_to_frequency(chan_no, IEEE80211_BAND_5GHZ);
    } else {
        hddLog(LOGE, FL("Invalid chan_no %d"), chan_no);
        kfree(mgmt);
        return NULL;
    }

    chan = __ieee80211_get_channel(wiphy, freq);
    /*
     * When the band is changed on the fly using the GUI, three things are done
     * 1. scan abort
     * 2. flush scan results from cache
     * 3. update the band with the new band user specified (refer to the
     * hdd_setBand_helper function) as part of the scan abort, message will be
     * queued to PE and we proceed with flushing and changing the band.
     * PE will stop the scanning further and report back the results what ever
     * it had till now by calling the call back function.
     * if the time between update band and scandone call back is sufficient
     * enough the band change reflects in SME, SME validates the channels
     * and discards the channels corresponding to previous band and calls back
     * with zero bss results. but if the time between band update and scan done
     * callback is very small then band change will not reflect in SME and SME
     * reports to HDD all the channels corresponding to previous band.this is
     * due to race condition.but those channels are invalid to the new band and
     * so this function __ieee80211_get_channel will return NULL.Each time we
     * report scan result with this pointer null warning kernel trace is printed
     * if the scan results contain large number of APs continuously kernel
     * warning trace is printed and it will lead to apps watch dog bark.
     * So drop the bss and continue to next bss.
     */
    if (chan == NULL) {
       hddLog(LOGE, FL("chan pointer is NULL"));
       kfree(mgmt);
       return NULL;
    }

    /* Based on .ini configuration, raw rssi can be reported for bss.
     * Raw rssi is typically used for estimating power.
     */

    rssi = (cfg_param->inform_bss_rssi_raw) ? bss_desc->rssi_raw :
                                              bss_desc->rssi;

    /* Supplicant takes the signal strength in terms of mBm(100*dBm) */
    rssi = (VOS_MIN(rssi, 0)) * 100;

    hddLog(LOG1, FL("BSSID: "MAC_ADDRESS_STR" Channel:%d RSSI:%d"),
           MAC_ADDR_ARRAY(mgmt->bssid), chan->center_freq, (int)(rssi/100));

    bss_status = cfg80211_inform_bss_frame(wiphy, chan, mgmt, frame_len, rssi,
                                           GFP_KERNEL);
    kfree(mgmt);
    return bss_status;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_update_bss_db
 * This function is used to update the BSS data base of CFG8011
 */
struct cfg80211_bss*
wlan_hdd_cfg80211_update_bss_db(hdd_adapter_t *pAdapter,
                                tCsrRoamInfo *pRoamInfo)
{
    tCsrRoamConnectedProfile roamProfile;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    struct cfg80211_bss *bss = NULL;

    ENTER();

    memset(&roamProfile, 0, sizeof(tCsrRoamConnectedProfile));
    sme_RoamGetConnectProfile(hHal, pAdapter->sessionId, &roamProfile);

    if (NULL != roamProfile.pBssDesc) {
        bss = wlan_hdd_cfg80211_inform_bss_frame(pAdapter,
                                                 roamProfile.pBssDesc);

        if (NULL == bss) {
            hddLog(LOG1, FL("wlan_hdd_cfg80211_inform_bss_frame return NULL"));
        }

        sme_RoamFreeConnectProfile(hHal, &roamProfile);
    } else {
        hddLog(LOGE, FL("roamProfile.pBssDesc is NULL"));
    }
    EXIT();
    return bss;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_update_bss
 */
static int wlan_hdd_cfg80211_update_bss( struct wiphy *wiphy,
                                         hdd_adapter_t *pAdapter
                                        )
{
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    tCsrScanResultInfo *pScanResult;
    eHalStatus status = 0;
    tScanResultHandle pResult;
    struct cfg80211_bss *bss_status = NULL;
    hdd_context_t *pHddCtx;
    int ret;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_UPDATE_BSS,
                     NO_SESSION, pAdapter->sessionId));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);
    if (0 != ret) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return ret;
    }

    /*
     * start getting scan results and populate cgf80211 BSS database
     */
    status = sme_ScanGetResult(hHal, pAdapter->sessionId, NULL, &pResult);

    /* no scan results */
    if (NULL == pResult) {
        hddLog(LOG1, FL("No scan result Status %d"), status);
        return status;
    }

    pScanResult = sme_ScanResultGetFirst(hHal, pResult);

    while (pScanResult) {
        /*
         * cfg80211_inform_bss() is not updating ie field of bss entry, if
         * entry already exists in bss data base of cfg80211 for that
         * particular BSS ID.  Using cfg80211_inform_bss_frame to update the
         * bss entry instead of cfg80211_inform_bss, But this call expects
         * mgmt packet as input. As of now there is no possibility to get
         * the mgmt(probe response) frame from PE, converting bss_desc to
         * ieee80211_mgmt(probe response) and passing to c
         * fg80211_inform_bss_frame.
         * */

        bss_status = wlan_hdd_cfg80211_inform_bss_frame(pAdapter,
                                                   &pScanResult->BssDescriptor);


        if (NULL == bss_status) {
            hddLog(LOG1, FL("NULL returned by cfg80211_inform_bss_frame"));
        } else {
            cfg80211_put_bss(
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
                             wiphy,
#endif
                             bss_status);
        }

        pScanResult = sme_ScanResultGetNext(hHal, pResult);
    }

    sme_ScanResultPurge(hHal, pResult);

    EXIT();
    return 0;
}

void
hddPrintPmkId(tANI_U8 *pmkId, tANI_U8 logLevel)
{
    VOS_TRACE(VOS_MODULE_ID_HDD, logLevel,
              "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
              pmkId[0], pmkId[1], pmkId[2], pmkId[3], pmkId[4],
              pmkId[5], pmkId[6], pmkId[7], pmkId[8], pmkId[9], pmkId[10],
              pmkId[11], pmkId[12], pmkId[13], pmkId[14], pmkId[15]);
} /****** end hddPrintPmkId() ******/

#define dump_pmkid(pMac, pmkid) \
    { \
        hddLog(VOS_TRACE_LEVEL_INFO, "PMKSA-ID:\t"); \
        hddPrintPmkId(pmkid, VOS_TRACE_LEVEL_INFO);\
    }

#if defined(FEATURE_WLAN_LFR) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
/*
 * FUNCTION: wlan_hdd_cfg80211_pmksa_candidate_notify
 * This function is used to notify the supplicant of a new PMKSA candidate.
 */
int wlan_hdd_cfg80211_pmksa_candidate_notify(
                    hdd_adapter_t *pAdapter, tCsrRoamInfo *pRoamInfo,
                    int index, bool preauth )
{
#ifdef FEATURE_WLAN_OKC
    struct net_device *dev = pAdapter->dev;
    hdd_context_t *pHddCtx = (hdd_context_t*)pAdapter->pHddCtx;

    ENTER();
    hddLog(LOG1, FL("is going to notify supplicant of:"));

    if (NULL == pRoamInfo) {
        hddLog(LOGP, FL("pRoamInfo is NULL"));
        return -EINVAL;
    }

    if (eANI_BOOLEAN_TRUE == hdd_is_okc_mode_enabled(pHddCtx)) {
        hddLog(VOS_TRACE_LEVEL_INFO, MAC_ADDRESS_STR,
               MAC_ADDR_ARRAY(pRoamInfo->bssid));
        cfg80211_pmksa_candidate_notify(dev, index, pRoamInfo->bssid,
                                        preauth, GFP_KERNEL);
    }
#endif  /* FEATURE_WLAN_OKC */
    return 0;
}
#endif //FEATURE_WLAN_LFR

#ifdef FEATURE_WLAN_LFR_METRICS
/*
 * FUNCTION: wlan_hdd_cfg80211_roam_metrics_preauth
 * 802.11r/LFR metrics reporting function to report preauth initiation
 *
 */
#define MAX_LFR_METRICS_EVENT_LENGTH 100
VOS_STATUS wlan_hdd_cfg80211_roam_metrics_preauth(hdd_adapter_t *pAdapter,
                                                  tCsrRoamInfo *pRoamInfo)
{
    unsigned char metrics_notification[MAX_LFR_METRICS_EVENT_LENGTH + 1];
    union iwreq_data wrqu;

    ENTER();

    if (NULL == pAdapter)
    {
        hddLog(LOGE, "%s: pAdapter is NULL!", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    /* create the event */
    memset(&wrqu, 0, sizeof(wrqu));
    memset(metrics_notification, 0, sizeof(metrics_notification));

    wrqu.data.pointer = metrics_notification;
    wrqu.data.length = scnprintf(metrics_notification,
        sizeof(metrics_notification), "QCOM: LFR_PREAUTH_INIT "
        MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pRoamInfo->bssid));

    wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, metrics_notification);

    EXIT();

    return VOS_STATUS_SUCCESS;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_roam_metrics_preauth_status
 * 802.11r/LFR metrics reporting function to report preauth completion
 * or failure
 */
VOS_STATUS wlan_hdd_cfg80211_roam_metrics_preauth_status(
    hdd_adapter_t *pAdapter, tCsrRoamInfo *pRoamInfo, bool preauth_status)
{
    unsigned char metrics_notification[MAX_LFR_METRICS_EVENT_LENGTH + 1];
    union iwreq_data wrqu;

    ENTER();

    if (NULL == pAdapter)
    {
        hddLog(LOGE, "%s: pAdapter is NULL!", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    /* create the event */
    memset(&wrqu, 0, sizeof(wrqu));
    memset(metrics_notification, 0, sizeof(metrics_notification));

    scnprintf(metrics_notification, sizeof(metrics_notification),
        "QCOM: LFR_PREAUTH_STATUS "MAC_ADDRESS_STR,
        MAC_ADDR_ARRAY(pRoamInfo->bssid));

    if (1 == preauth_status)
        strncat(metrics_notification, " TRUE", 5);
    else
        strncat(metrics_notification, " FALSE", 6);

    wrqu.data.pointer = metrics_notification;
    wrqu.data.length = strlen(metrics_notification);

    wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, metrics_notification);

    EXIT();

    return VOS_STATUS_SUCCESS;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_roam_metrics_handover
 * 802.11r/LFR metrics reporting function to report handover initiation
 *
 */
VOS_STATUS wlan_hdd_cfg80211_roam_metrics_handover(hdd_adapter_t * pAdapter,
                                                   tCsrRoamInfo *pRoamInfo)
{
    unsigned char metrics_notification[MAX_LFR_METRICS_EVENT_LENGTH + 1];
    union iwreq_data wrqu;

    ENTER();

    if (NULL == pAdapter)
    {
        hddLog(LOGE, "%s: pAdapter is NULL!", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    /* create the event */
    memset(&wrqu, 0, sizeof(wrqu));
    memset(metrics_notification, 0, sizeof(metrics_notification));

    wrqu.data.pointer = metrics_notification;
    wrqu.data.length = scnprintf(metrics_notification,
        sizeof(metrics_notification), "QCOM: LFR_PREAUTH_HANDOVER "
        MAC_ADDRESS_STR, MAC_ADDR_ARRAY(pRoamInfo->bssid));

    wireless_send_event(pAdapter->dev, IWEVCUSTOM, &wrqu, metrics_notification);

    EXIT();

    return VOS_STATUS_SUCCESS;
}
#endif

/*
 * FUNCTION: hdd_cfg80211_scan_done_callback
 * scanning callback function, called after finishing scan
 *
 */
static eHalStatus hdd_cfg80211_scan_done_callback(tHalHandle halHandle,
                                                  void *pContext,
                                                  tANI_U8 sessionId,
                                                  tANI_U32 scanId,
                                                  eCsrScanStatus status)
{
    struct net_device *dev = (struct net_device *) pContext;
    //struct wireless_dev *wdev = dev->ieee80211_ptr;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_scaninfo_t *pScanInfo = &pAdapter->scan_info;
    struct cfg80211_scan_request *req = NULL;
    bool aborted = false;
    unsigned long rc;
    int ret = 0;

    ENTER();

    hddLog(VOS_TRACE_LEVEL_INFO,
            "%s called with halHandle = %p, pContext = %p,"
            "scanID = %d, returned status = %d",
            __func__, halHandle, pContext, (int) scanId, (int) status);

    pScanInfo->mScanPendingCounter = 0;

    //Block on scan req completion variable. Can't wait forever though.
    rc = wait_for_completion_timeout(
                         &pScanInfo->scan_req_completion_event,
                         msecs_to_jiffies(WLAN_WAIT_TIME_SCAN_REQ));
    if (!rc) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s wait on scan_req_completion_event timed out", __func__);
       VOS_ASSERT(pScanInfo->mScanPending);
       goto allow_suspend;
    }

    if (pScanInfo->mScanPending != VOS_TRUE)
    {
        VOS_ASSERT(pScanInfo->mScanPending);
        goto allow_suspend;
    }

    /* Check the scanId */
    if (pScanInfo->scanId != scanId)
    {
        hddLog(VOS_TRACE_LEVEL_INFO,
                "%s called with mismatched scanId pScanInfo->scanId = %d "
                "scanId = %d", __func__, (int) pScanInfo->scanId,
                (int) scanId);
    }

    ret = wlan_hdd_cfg80211_update_bss((WLAN_HDD_GET_CTX(pAdapter))->wiphy,
                                        pAdapter);

    if (0 > ret)
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: NO SCAN result", __func__);


    /* If any client wait scan result through WEXT
     * send scan done event to client */
    if (pAdapter->scan_info.waitScanResult)
    {
        /* The other scan request waiting for current scan finish
         * Send event to notify current scan finished */
        if(WEXT_SCAN_PENDING_DELAY == pAdapter->scan_info.scan_pending_option)
        {
            vos_event_set(&pAdapter->scan_info.scan_finished_event);
        }
        /* Send notify to WEXT client */
        else if(WEXT_SCAN_PENDING_PIGGYBACK == pAdapter->scan_info.scan_pending_option)
        {
            struct net_device *dev = pAdapter->dev;
            union iwreq_data wrqu;
            int we_event;
            char *msg;

            memset(&wrqu, '\0', sizeof(wrqu));
            we_event = SIOCGIWSCAN;
            msg = NULL;
            wireless_send_event(dev, we_event, &wrqu, msg);
        }
    }
    pAdapter->scan_info.waitScanResult = FALSE;

    /* Get the Scan Req */
    req = pAdapter->request;

    if (!req)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "request is became NULL");
        pScanInfo->mScanPending = VOS_FALSE;
        goto allow_suspend;
    }

    pAdapter->request = NULL;
    /* Scan is no longer pending */
    pScanInfo->mScanPending = VOS_FALSE;

    /*
     * cfg80211_scan_done informing NL80211 about completion
     * of scanning
     */
    if (status == eCSR_SCAN_ABORT || status == eCSR_SCAN_FAILURE)
    {
         aborted = true;
    }
    cfg80211_scan_done(req, aborted);

    complete(&pScanInfo->abortscan_event_var);

allow_suspend:
    /* release the wake lock at the end of the scan*/
    hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_SCAN);
    /* Acquire wakelock to handle the case where APP's tries to suspend
     * immediately after the driver gets connect request(i.e after scan)
     * from supplicant, this result in app's is suspending and not able
     * to process the connect request to AP */
    hdd_prevent_suspend_timeout(1000, WIFI_POWER_EVENT_WAKELOCK_SCAN);
    /* Today Prevent/allow Runtime PM is tied with wakelocks, thus holding
     * a wakelock prevents runtime pm. In this specific case host held a
     * wakelock for 1sec to prevent APPS suspend, so supplicant can initiate
     * connection which can come asynchronously. Host is already preventing
     * runtime pm when connection in progress. In this case host can do
     * runtime suspend. Hence allow runtime suspend to happen.
     * The ideal approach is to decouple runtime pm from wakelocks and
     * identify cases where runtime pm should be prevented and allowed.
     * This is a fix done to address power issue where there is
     * rise is current cosumption for 1.8sec adding host 1sec wakelock
     * and 0.5sec auto suspend delay.
     */
    hdd_allow_runtime_suspend();

#ifdef FEATURE_WLAN_TDLS
    wlan_hdd_tdls_scan_done_callback(pAdapter);
#endif

    EXIT();
    return 0;
}

/*
 * FUNCTION: hdd_isConnectionInProgress
 * Go through each adapter and check if Connection is in progress
 *
 */
bool hdd_isConnectionInProgress(hdd_context_t *pHddCtx)
{
	hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
	hdd_station_ctx_t *pHddStaCtx = NULL;
	hdd_adapter_t *pAdapter = NULL;
	VOS_STATUS status = 0;
	v_U8_t staId = 0;
	v_U8_t *staMac = NULL;

	if (TRUE == pHddCtx->btCoexModeSet) {
		hddLog(LOG1, FL("BTCoex Mode operation in progress"));
		return true;
	}

	status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);

	while (NULL != pAdapterNode && VOS_STATUS_SUCCESS == status) {
		pAdapter = pAdapterNode->pAdapter;

		if (pAdapter) {
			hddLog(VOS_TRACE_LEVEL_INFO,
				"%s: Adapter with device mode %d exists",
				__func__, pAdapter->device_mode);
			if (((WLAN_HDD_INFRA_STATION ==
					pAdapter->device_mode) ||
				(WLAN_HDD_P2P_CLIENT ==
					pAdapter->device_mode) ||
				(WLAN_HDD_P2P_DEVICE ==
					pAdapter->device_mode)) &&
				(eConnectionState_Connecting ==
				(WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->
					conn_info.connState)) {
				hddLog(LOGE,
					FL("%p(%d) Connection is in progress"),
					WLAN_HDD_GET_STATION_CTX_PTR(pAdapter),
					pAdapter->sessionId);
				return true;
			}
			if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) &&
				smeNeighborRoamIsHandoffInProgress(
				WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId))
			{
				hddLog(VOS_TRACE_LEVEL_ERROR,
				"%s: %p(%d) Reassociation is in progress", __func__,
				WLAN_HDD_GET_STATION_CTX_PTR(pAdapter), pAdapter->sessionId);
				return VOS_TRUE;
			}
			if ((WLAN_HDD_INFRA_STATION ==
					pAdapter->device_mode) ||
				(WLAN_HDD_P2P_CLIENT ==
					pAdapter->device_mode) ||
				(WLAN_HDD_P2P_DEVICE ==
					pAdapter->device_mode)) {
				pHddStaCtx =
					WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
				if ((eConnectionState_Associated ==
					pHddStaCtx->conn_info.connState) &&
					(VOS_FALSE ==
						pHddStaCtx->conn_info.
							uIsAuthenticated)) {
					staMac = (v_U8_t *) &(pAdapter->
						macAddressCurrent.bytes[0]);
					hddLog(LOGE,
						FL("client " MAC_ADDRESS_STR " is in the middle of WPS/EAPOL exchange."),
						MAC_ADDR_ARRAY(staMac));
					return true;
				}
			} else if ((WLAN_HDD_SOFTAP == pAdapter->device_mode) ||
				   (WLAN_HDD_P2P_GO == pAdapter->device_mode)) {
				for (staId = 0; staId < WLAN_MAX_STA_COUNT;
					staId++) {
					if ((pAdapter->aStaInfo[staId].
							isUsed) &&
						(WLANTL_STA_CONNECTED ==
						 pAdapter->aStaInfo[staId].
								tlSTAState)) {
						staMac = (v_U8_t *) &(pAdapter->
							aStaInfo[staId].
							macAddrSTA.bytes[0]);

					hddLog(LOGE,
						FL("client " MAC_ADDRESS_STR " of SoftAP/P2P-GO is in the "
						"middle of WPS/EAPOL exchange."),
						MAC_ADDR_ARRAY(staMac));
					return true;
					}
				}
			}
		}
		status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
		pAdapterNode = pNext;
	}
	return false;
}

static void wlan_hdd_cfg80211_scan_block_cb(struct work_struct *work)
{
    hdd_adapter_t *adapter = container_of(work,
                                   hdd_adapter_t, scan_block_work);
    struct cfg80211_scan_request *request = NULL;
    if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
       VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD adapter context is invalid", __func__);
       return;
    }

    request = adapter->request;
    if (request) {
        request->n_ssids = 0;
        request->n_channels = 0;

        hddLog(LOGE,
                "%s:##In DFS Master mode. Scan aborted. Null result sent",
                 __func__);
        cfg80211_scan_done(request, true);
        adapter->request = NULL;
    }
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_scan
 * this scan respond to scan trigger and update cfg80211 scan database
 * later, scan dump command can be used to receive scan results
 */
int __wlan_hdd_cfg80211_scan( struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                            struct net_device *dev,
#endif
                            struct cfg80211_scan_request *request)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
   struct net_device *dev = request->wdev->netdev;
#endif
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
    hdd_wext_state_t *pwextBuf = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    hdd_config_t *cfg_param = NULL;
    tCsrScanRequest scanRequest;
    tANI_U8 *channelList = NULL, i;
    v_U32_t scanId = 0;
    int status;
    hdd_scaninfo_t *pScanInfo = NULL;
    v_U8_t* pP2pIe = NULL;
    hdd_adapter_t *con_sap_adapter;
    uint16_t con_dfs_ch;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SCAN,
                     pAdapter->sessionId, request->n_channels));

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: device_mode = %d",
           __func__, pAdapter->device_mode);

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    cfg_param = pHddCtx->cfg_ini;
    pScanInfo = &pAdapter->scan_info;

#ifdef WLAN_BTAMP_FEATURE
    //Scan not supported when AMP traffic is on.
    if (VOS_TRUE == WLANBAP_AmpSessionOn())
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: No scanning when AMP is on", __func__);
        return -EOPNOTSUPP;
    }
#endif
    //Scan on any other interface is not supported.
    if (pAdapter->device_mode == WLAN_HDD_SOFTAP)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: Not scanning on device_mode = %d",
               __func__, pAdapter->device_mode);
        return -EOPNOTSUPP;
    }
    /* Block All Scan during DFS operation and send null scan result */
    con_sap_adapter = hdd_get_con_sap_adapter(pAdapter);
    if (con_sap_adapter) {
        con_dfs_ch = con_sap_adapter->sessionCtx.ap.sapConfig.channel;
        if (con_dfs_ch == AUTO_CHANNEL_SELECT)
            con_dfs_ch = con_sap_adapter->sessionCtx.ap.operatingChannel;

        if (VOS_IS_DFS_CH(con_dfs_ch)) {
            /* Provide empty scan result during DFS operation since scanning
             * not supported during DFS. Reason is following case:
             * DFS is supported only in SCC for MBSSID Mode.
             * We shall not return EBUSY or ENOTSUPP as when Primary AP is
             * operating in DFS channel and secondary AP is started. Though we
             * force SCC in driver, the hostapd issues obss scan before
             * starting secAP. This results in MCC in DFS mode.
             * Thus we return null scan result. If we return scan failure
             * hostapd fails secondary AP startup.
             */
            pAdapter->request = request;

#ifdef CONFIG_CNSS
            cnss_init_work(&pAdapter->scan_block_work,
                                            wlan_hdd_cfg80211_scan_block_cb);
#else
            INIT_WORK(&pAdapter->scan_block_work,
                                            wlan_hdd_cfg80211_scan_block_cb);
#endif

            schedule_work(&pAdapter->scan_block_work);
            return 0;
        }
    }

    if (TRUE == pScanInfo->mScanPending)
    {
        if ( MAX_PENDING_LOG > pScanInfo->mScanPendingCounter++ )
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: mScanPending is TRUE", __func__);
        }
        return -EBUSY;
    }

    //Don't Allow Scan and return busy if Remain On
    //Channel and action frame is pending
    //Otherwise Cancel Remain On Channel and allow Scan
    //If no action frame pending
    if (0 != wlan_hdd_check_remain_on_channel(pAdapter))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Remain On Channel Pending", __func__);
        return -EBUSY;
    }
#ifdef FEATURE_WLAN_TDLS
    /* if tdls disagree scan right now, return immediately.
       tdls will schedule the scan when scan is allowed. (return SUCCESS)
       or will reject the scan if any TDLS is in progress. (return -EBUSY)
    */
    status = wlan_hdd_tdls_scan_callback (pAdapter,
                                          wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                                          dev,
#endif
                                          request);
    if (status <= 0)
    {
      if (!status)
          hddLog(VOS_TRACE_LEVEL_ERROR, "%s: TDLS in progress.scan rejected %d",
                 __func__, status);
      else
          hddLog(VOS_TRACE_LEVEL_ERROR, "%s: TDLS teardown is ongoing %d",
                 __func__, status);

        return status;
    }
#endif

    if (mutex_lock_interruptible(&pHddCtx->tmInfo.tmOperationLock))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD,VOS_TRACE_LEVEL_ERROR,
                  "%s: Acquire lock fail", __func__);
        return -EAGAIN;
    }
    if (TRUE == pHddCtx->tmInfo.tmAction.enterImps)
    {
        mutex_unlock(&pHddCtx->tmInfo.tmOperationLock);
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: MAX TM Level Scan not allowed", __func__);
        return -EBUSY;
    }
    mutex_unlock(&pHddCtx->tmInfo.tmOperationLock);

    /* Check if scan is allowed at this point of time.
     */
    if (hdd_isConnectionInProgress(pHddCtx)) {
        hddLog(LOGE, FL("Scan not allowed"));
        return -EBUSY;
    }

    vos_mem_zero( &scanRequest, sizeof(scanRequest));

    hddLog(VOS_TRACE_LEVEL_INFO, "scan request for ssid = %d",
           request->n_ssids);

    /* Even though supplicant doesn't provide any SSIDs, n_ssids is
     * set to 1.  Because of this, driver is assuming that this is not
     * wildcard scan and so is not aging out the scan results.
     */
    if ((request->ssids) && (request->n_ssids == 1) &&
                            ('\0' == request->ssids->ssid[0]))
    {
        request->n_ssids = 0;
    }

    if ((request->ssids) && (0 < request->n_ssids))
    {
        tCsrSSIDInfo *SsidInfo;
        int j;
        scanRequest.SSIDs.numOfSSIDs = request->n_ssids;
        /* Allocate num_ssid tCsrSSIDInfo structure */
        SsidInfo = scanRequest.SSIDs.SSIDList =
            vos_mem_malloc(request->n_ssids * sizeof(tCsrSSIDInfo));

        if (NULL == scanRequest.SSIDs.SSIDList)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: memory alloc failed SSIDInfo buffer", __func__);
            return -ENOMEM;
        }

        /* copy all the ssid's and their length */
        for (j = 0; j < request->n_ssids; j++, SsidInfo++)
        {
            /* get the ssid length */
            SsidInfo->SSID.length = request->ssids[j].ssid_len;
            vos_mem_copy(SsidInfo->SSID.ssId, &request->ssids[j].ssid[0],
                         SsidInfo->SSID.length);
            SsidInfo->SSID.ssId[SsidInfo->SSID.length] = '\0';
            hddLog(VOS_TRACE_LEVEL_INFO, "SSID number %d: %s",
                   j, SsidInfo->SSID.ssId);
        }
        /* set the scan type to active */
        scanRequest.scanType = eSIR_ACTIVE_SCAN;
    }
    else if (WLAN_HDD_P2P_GO == pAdapter->device_mode)
    {
        /* set the scan type to active */
        scanRequest.scanType = eSIR_ACTIVE_SCAN;
    }
    else
    {
        /* Set the scan type to default type, in this case it is ACTIVE */
        scanRequest.scanType = pHddCtx->ioctl_scan_mode;
    }
    scanRequest.minChnTime = cfg_param->nActiveMinChnTime;
    scanRequest.maxChnTime = cfg_param->nActiveMaxChnTime;

    /* set BSSType to default type */
    scanRequest.BSSType = eCSR_BSS_TYPE_ANY;

    if (MAX_CHANNEL < request->n_channels)
    {
       hddLog(VOS_TRACE_LEVEL_WARN, "No of Scan Channels exceeded limit: %d",
              request->n_channels);
       request->n_channels = MAX_CHANNEL;
    }

    hddLog(VOS_TRACE_LEVEL_INFO,
           "No of Scan Channels: %d", request->n_channels);

    if (request->n_channels)
    {
       char chList [(request->n_channels*5)+1];
       int len;
       channelList = vos_mem_malloc(request->n_channels);
       if (NULL == channelList)
       {
          hddLog(VOS_TRACE_LEVEL_ERROR,
                 "channelList memory alloc failed channelList");
          status = -ENOMEM;
          goto free_mem;
       }
       for (i = 0, len = 0; i < request->n_channels ; i++ )
       {
          channelList[i] = request->channels[i]->hw_value;
          len += snprintf(chList+len, 5, "%d ", channelList[i]);
       }

       hddLog(VOS_TRACE_LEVEL_INFO, "Channel-List: %s", chList);

    }
    scanRequest.ChannelInfo.numOfChannels = request->n_channels;
    scanRequest.ChannelInfo.ChannelList = channelList;

    /* set requestType to full scan */
    scanRequest.requestType = eCSR_SCAN_REQUEST_FULL_SCAN;

    /* Flush the scan results(only p2p beacons) for STA scan and P2P
     * search (Flush on both full  scan and social scan but not on single
     * channel scan).P2P  search happens on 3 social channels (1, 6, 11)
     */

    /* Supplicant does single channel scan after 8-way handshake
     * and in that case driver shouldn't flush scan results. If
     * driver flushes the scan results here and unfortunately if
     * the AP doesn't respond to our probe req then association
     * fails which is not desired
     */

    if (request->n_channels != WLAN_HDD_P2P_SINGLE_CHANNEL_SCAN)
    {
       hddLog(VOS_TRACE_LEVEL_DEBUG, "Flushing P2P Results");
       sme_ScanFlushP2PResult( WLAN_HDD_GET_HAL_CTX(pAdapter),
                               pAdapter->sessionId );
    }

    if (request->ie_len)
    {
       /* save this for future association (join requires this) */
       memset( &pScanInfo->scanAddIE, 0, sizeof(pScanInfo->scanAddIE) );
       memcpy( pScanInfo->scanAddIE.addIEdata, request->ie, request->ie_len);
       pScanInfo->scanAddIE.length = request->ie_len;

       if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
           (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) ||
           (WLAN_HDD_P2P_DEVICE == pAdapter->device_mode)
           )
       {
          pwextBuf->roamProfile.pAddIEScan = pScanInfo->scanAddIE.addIEdata;
          pwextBuf->roamProfile.nAddIEScanLength = pScanInfo->scanAddIE.length;
       }

       scanRequest.uIEFieldLen = pScanInfo->scanAddIE.length;
       scanRequest.pIEField = pScanInfo->scanAddIE.addIEdata;

       pP2pIe = wlan_hdd_get_p2p_ie_ptr((v_U8_t*)request->ie,
                                        request->ie_len);
       if (pP2pIe != NULL)
       {
#ifdef WLAN_FEATURE_P2P_DEBUG
          if (((globalP2PConnectionStatus == P2P_GO_NEG_COMPLETED) ||
               (globalP2PConnectionStatus == P2P_GO_NEG_PROCESS)) &&
              (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))
          {
             globalP2PConnectionStatus = P2P_CLIENT_CONNECTING_STATE_1;
             hddLog(VOS_TRACE_LEVEL_ERROR,
                    "[P2P State] Changing state from Go nego completed to Connection is started");
             hddLog(VOS_TRACE_LEVEL_ERROR,
                    "[P2P]P2P Scanning is started for 8way Handshake");
          }
          else if ((globalP2PConnectionStatus == P2P_CLIENT_DISCONNECTED_STATE) &&
                  (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))
          {
             globalP2PConnectionStatus = P2P_CLIENT_CONNECTING_STATE_2;
             hddLog(VOS_TRACE_LEVEL_ERROR,
                    "[P2P State] Changing state from Disconnected state to Connection is started");
             hddLog(VOS_TRACE_LEVEL_ERROR,
                    "[P2P]P2P Scanning is started for 4way Handshake");
          }
#endif

          /* no_cck will be set during p2p find to disable 11b rates */
          if (request->no_cck)
          {
             hddLog(VOS_TRACE_LEVEL_INFO,
                    "%s: This is a P2P Search", __func__);
             scanRequest.p2pSearch = 1;

             if (request->n_channels == WLAN_HDD_P2P_SOCIAL_CHANNELS)
             {
                /* set requestType to P2P Discovery */
                scanRequest.requestType = eCSR_SCAN_P2P_DISCOVERY;
             }

             /*
              * Skip Dfs Channel in case of P2P Search if it is set in
              * ini file
              */
             if (cfg_param->skipDfsChnlInP2pSearch)
             {
                scanRequest.skipDfsChnlInP2pSearch = 1;
             }
             else
             {
                scanRequest.skipDfsChnlInP2pSearch = 0;
             }

          }
       }
    }

    INIT_COMPLETION(pScanInfo->scan_req_completion_event);

    /* acquire the wakelock to avoid the apps suspend during the scan. To
     * address the following issues.
     * 1) Disconnected scenario: we are not allowing the suspend as WLAN is not in
     * BMPS/IMPS this result in android trying to suspend aggressively and backing off
     * for long time, this result in apps running at full power for long time.
     * 2) Connected scenario: If we allow the suspend during the scan, RIVA will
     * be stuck in full power because of resume BMPS
     */
    hdd_prevent_suspend_timeout(HDD_WAKE_LOCK_SCAN_DURATION,
                                WIFI_POWER_EVENT_WAKELOCK_SCAN);
    hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
           "requestType %d, scanType %d, minChnTime %d, maxChnTime %d,p2pSearch %d, skipDfsChnlIn P2pSearch %d",
           scanRequest.requestType, scanRequest.scanType,
           scanRequest.minChnTime, scanRequest.maxChnTime,
           scanRequest.p2pSearch, scanRequest.skipDfsChnlInP2pSearch);

    status = sme_ScanRequest( WLAN_HDD_GET_HAL_CTX(pAdapter),
                              pAdapter->sessionId, &scanRequest, &scanId,
                              &hdd_cfg80211_scan_done_callback, dev );

    if (eHAL_STATUS_SUCCESS != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: sme_ScanRequest returned error %d", __func__, status);
        complete(&pScanInfo->scan_req_completion_event);
        if(eHAL_STATUS_RESOURCES == status)
        {
           hddLog(VOS_TRACE_LEVEL_ERROR, "%s: HO is in progress.So defer the scan by informing busy",
                  __func__);
           status = -EBUSY;
        }
        else
        {
           status = -EIO;
        }

        hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_SCAN);
        goto free_mem;
    }

    pScanInfo->mScanPending = TRUE;
    pAdapter->request = request;
    pScanInfo->scanId = scanId;

    complete(&pScanInfo->scan_req_completion_event);

free_mem:
    if( scanRequest.SSIDs.SSIDList )
    {
        vos_mem_free(scanRequest.SSIDs.SSIDList);
    }

    if( channelList )
      vos_mem_free( channelList );

    EXIT();

    return status;
}

int wlan_hdd_cfg80211_scan( struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                            struct net_device *dev,
#endif
                            struct cfg80211_scan_request *request)
{
    int ret;

    vos_ssr_protect(__func__);
    ret =  __wlan_hdd_cfg80211_scan(wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                                   dev,
#endif
                                   request);
    vos_ssr_unprotect(__func__);

    return ret;
}

void hdd_select_cbmode( hdd_adapter_t *pAdapter,v_U8_t operationChannel)
{
    v_U8_t iniDot11Mode =
               (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->dot11Mode;
    eHddDot11Mode   hddDot11Mode = iniDot11Mode;

    hddLog(LOG1, FL("Channel Bonding Mode Selected is %u"),
           iniDot11Mode);
    switch ( iniDot11Mode )
    {
       case eHDD_DOT11_MODE_AUTO:
       case eHDD_DOT11_MODE_11ac:
       case eHDD_DOT11_MODE_11ac_ONLY:
#ifdef WLAN_FEATURE_11AC
          if (sme_IsFeatureSupportedByFW(DOT11AC))
              hddDot11Mode = eHDD_DOT11_MODE_11ac;
          else
              hddDot11Mode = eHDD_DOT11_MODE_11n;
#else
          hddDot11Mode = eHDD_DOT11_MODE_11n;
#endif
          break;
       case eHDD_DOT11_MODE_11n:
       case eHDD_DOT11_MODE_11n_ONLY:
          hddDot11Mode = eHDD_DOT11_MODE_11n;
          break;
       default:
          hddDot11Mode = iniDot11Mode;
          break;
    }
     /* This call decides required channel bonding mode */
    sme_SelectCBMode((WLAN_HDD_GET_CTX(pAdapter)->hHal),
                     hdd_cfg_xlate_to_csr_phy_mode(hddDot11Mode),
                     operationChannel);
}

/*
 * Time in msec
 * Time for complete association including DHCP
 */
#define WLAN_HDD_CONNECTION_TIME (30 * 1000)

/*
 * FUNCTION: wlan_hdd_cfg80211_connect_start
 * This function is used to start the association process
 */
int wlan_hdd_cfg80211_connect_start( hdd_adapter_t  *pAdapter,
        const u8 *ssid, size_t ssid_len, const u8 *bssid,
        const u8 *bssid_hint, u8 operatingChannel)
{
    int status = 0;
    hdd_wext_state_t *pWextState;
    hdd_context_t *pHddCtx;
    v_U32_t roamId;
    tCsrRoamProfile *pRoamProfile;
    eCsrAuthType RSNAuthType;

    ENTER();

    pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    status = wlan_hdd_validate_context(pHddCtx);
    if (status)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is not valid!", __func__);
        return status;
    }

    if (SIR_MAC_MAX_SSID_LENGTH < ssid_len)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: wrong SSID len", __func__);
        return -EINVAL;
    }

    pRoamProfile = &pWextState->roamProfile;

    if (pRoamProfile)
    {
        hdd_station_ctx_t *pHddStaCtx;
        pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

        if (HDD_WMM_USER_MODE_NO_QOS ==
                        (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->WmmMode)
        {
            /*QoS not enabled in cfg file*/
            pRoamProfile->uapsd_mask = 0;
        }
        else
        {
            /*QoS enabled, update uapsd mask from cfg file*/
            pRoamProfile->uapsd_mask =
                     (WLAN_HDD_GET_CTX(pAdapter))->cfg_ini->UapsdMask;
        }

        pRoamProfile->SSIDs.numOfSSIDs = 1;
        pRoamProfile->SSIDs.SSIDList->SSID.length = ssid_len;
        vos_mem_zero(pRoamProfile->SSIDs.SSIDList->SSID.ssId,
                sizeof(pRoamProfile->SSIDs.SSIDList->SSID.ssId));
        vos_mem_copy((void *)(pRoamProfile->SSIDs.SSIDList->SSID.ssId),
                ssid, ssid_len);

        if (bssid)
        {
            pRoamProfile->BSSIDs.numOfBSSIDs = 1;
            vos_mem_copy((void *)(pRoamProfile->BSSIDs.bssid), bssid,
                    VOS_MAC_ADDR_SIZE);
            /* Save BSSID in separate variable as well, as RoamProfile
               BSSID is getting zeroed out in the association process. And in
               case of join failure we should send valid BSSID to supplicant
             */
            vos_mem_copy((void *)(pWextState->req_bssId), bssid,
                    VOS_MAC_ADDR_SIZE);
        }
        else if (bssid_hint)
        {
            pRoamProfile->BSSIDs.numOfBSSIDs = 1;
            vos_mem_copy((void *)(pRoamProfile->BSSIDs.bssid), bssid_hint,
                    VOS_MAC_ADDR_SIZE);
            /* Save BSSID in separate variable as well, as RoamProfile
               BSSID is getting zeroed out in the association process. And in
               case of join failure we should send valid BSSID to supplicant
             */
            vos_mem_copy((void *)(pWextState->req_bssId), bssid_hint,
                    VOS_MAC_ADDR_SIZE);
            hddLog(LOGW, FL(" bssid_hint "MAC_ADDRESS_STR),
                   MAC_ADDR_ARRAY(bssid_hint));

        }
        else
        {
            vos_mem_zero((void *)(pRoamProfile->BSSIDs.bssid), VOS_MAC_ADDR_SIZE);
        }

        hddLog(LOG1, FL("Connect to SSID: %.*s operating Channel: %u"),
               pRoamProfile->SSIDs.SSIDList->SSID.length,
               pRoamProfile->SSIDs.SSIDList->SSID.ssId,
               operatingChannel);

        if ((IW_AUTH_WPA_VERSION_WPA == pWextState->wpaVersion) ||
                (IW_AUTH_WPA_VERSION_WPA2 == pWextState->wpaVersion))
        {
            /*set gen ie*/
            hdd_SetGENIEToCsr(pAdapter, &RSNAuthType);
            /*set auth*/
            hdd_set_csr_auth_type(pAdapter, RSNAuthType);
        }
#ifdef FEATURE_WLAN_WAPI
        if (pAdapter->wapi_info.nWapiMode)
        {
            hddLog(LOG1, "%s: Setting WAPI AUTH Type and Encryption Mode values", __func__);
            switch (pAdapter->wapi_info.wapiAuthMode)
            {
                case WAPI_AUTH_MODE_PSK:
                {
                    hddLog(LOG1, "%s: WAPI AUTH TYPE: PSK: %d", __func__,
                                                   pAdapter->wapi_info.wapiAuthMode);
                    pRoamProfile->AuthType.authType[0] = eCSR_AUTH_TYPE_WAPI_WAI_PSK;
                    break;
                }
                case WAPI_AUTH_MODE_CERT:
                {
                    hddLog(LOG1, "%s: WAPI AUTH TYPE: CERT: %d", __func__,
                                                    pAdapter->wapi_info.wapiAuthMode);
                    pRoamProfile->AuthType.authType[0] = eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE;
                    break;
                }
            } // End of switch
            if ( pAdapter->wapi_info.wapiAuthMode == WAPI_AUTH_MODE_PSK ||
                pAdapter->wapi_info.wapiAuthMode == WAPI_AUTH_MODE_CERT)
            {
                hddLog(LOG1, "%s: WAPI PAIRWISE/GROUP ENCRYPTION: WPI", __func__);
                pRoamProfile->AuthType.numEntries = 1;
                pRoamProfile->EncryptionType.numEntries = 1;
                pRoamProfile->EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_WPI;
                pRoamProfile->mcEncryptionType.numEntries = 1;
                pRoamProfile->mcEncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_WPI;
            }
        }
#endif /* FEATURE_WLAN_WAPI */
#ifdef WLAN_FEATURE_GTK_OFFLOAD
        /* Initializing gtkOffloadReqParams */
        if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
            (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))
        {
            memset(&pHddStaCtx->gtkOffloadReqParams, 0,
                  sizeof (tSirGtkOffloadParams));
            pHddStaCtx->gtkOffloadReqParams.ulFlags = GTK_OFFLOAD_DISABLE;
        }
#endif
        pRoamProfile->csrPersona = pAdapter->device_mode;

        if( operatingChannel )
        {
           pRoamProfile->ChannelInfo.ChannelList = &operatingChannel;
           pRoamProfile->ChannelInfo.numOfChannels = 1;
        }
        else
        {
            pRoamProfile->ChannelInfo.ChannelList = NULL;
            pRoamProfile->ChannelInfo.numOfChannels = 0;
        }
        if ( (WLAN_HDD_IBSS == pAdapter->device_mode) && operatingChannel)
        {
            /*
             * Need to post the IBSS power save parameters
             * to WMA. WMA will configure this parameters
             * to firmware if power save is enabled by the
             * firmware.
             */
            status = hdd_setIbssPowerSaveParams(pAdapter);

            if (VOS_STATUS_SUCCESS != status)
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                       "%s: Set IBSS Power Save Params Failed", __func__);
                return -EINVAL;
            }
            hdd_select_cbmode(pAdapter,operatingChannel);
        }

        /* change conn_state to connecting before sme_RoamConnect(), because sme_RoamConnect()
         * has a direct path to call hdd_smeRoamCallback(), which will change the conn_state
         * If direct path, conn_state will be accordingly changed to NotConnected or Associated
         * by either hdd_AssociationCompletionHandler() or hdd_DisConnectHandler() in sme_RoamCallback()
         * if sme_RomConnect is to be queued, Connecting state will remain until it is completed.
         */
        if (WLAN_HDD_INFRA_STATION == pAdapter->device_mode ||
            WLAN_HDD_P2P_CLIENT == pAdapter->device_mode)
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: Set HDD connState to eConnectionState_Connecting",
                   __func__);
            hdd_connSetConnectionState(pAdapter,
                                        eConnectionState_Connecting);
        }

        /* After 8-way handshake supplicant should give the scan command
         * in that it update the additional IEs, But because of scan
         * enhancements, the supplicant is not issuing the scan command now.
         * So the unicast frames which are sent from the host are not having
         * the additional IEs. If it is P2P CLIENT and there is no additional
         * IE present in roamProfile, then use the additional IE form scan_info
         */

        if ((pAdapter->device_mode == WLAN_HDD_P2P_CLIENT) &&
                (!pRoamProfile->pAddIEScan))
        {
            pRoamProfile->pAddIEScan = &pAdapter->scan_info.scanAddIE.addIEdata[0];
            pRoamProfile->nAddIEScanLength = pAdapter->scan_info.scanAddIE.length;
        }

        vos_runtime_pm_prevent_suspend_timeout(WLAN_HDD_CONNECTION_TIME);

        status = sme_RoamConnect( WLAN_HDD_GET_HAL_CTX(pAdapter),
                            pAdapter->sessionId, pRoamProfile, &roamId);

        if ((eHAL_STATUS_SUCCESS != status) &&
            (WLAN_HDD_INFRA_STATION == pAdapter->device_mode ||
             WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))

        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: sme_RoamConnect (session %d) failed with "
                                      "status %d. -> NotConnected", __func__, pAdapter->sessionId, status);
            /* change back to NotAssociated */
            hdd_connSetConnectionState(pAdapter,
                                       eConnectionState_NotConnected);
        }

        pRoamProfile->ChannelInfo.ChannelList = NULL;
        pRoamProfile->ChannelInfo.numOfChannels = 0;

    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: No valid Roam profile", __func__);
        return -EINVAL;
    }
    EXIT();
    return status;
}

/*
 * FUNCTION: wlan_hdd_set_cfg80211_auth_type
 * This function is used to set the authentication type (OPEN/SHARED).
 *
 */
static int wlan_hdd_cfg80211_set_auth_type(hdd_adapter_t *pAdapter,
        enum nl80211_auth_type auth_type)
{
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    ENTER();

    /*set authentication type*/
    switch (auth_type)
    {
        case NL80211_AUTHTYPE_AUTOMATIC:
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "%s: set authentication type to AUTOSWITCH", __func__);
            pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_AUTOSWITCH;
            break;

        case NL80211_AUTHTYPE_OPEN_SYSTEM:
#ifdef WLAN_FEATURE_VOWIFI_11R
        case NL80211_AUTHTYPE_FT:
#endif /* WLAN_FEATURE_VOWIFI_11R */
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "%s: set authentication type to OPEN", __func__);
            pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_OPEN_SYSTEM;
            break;

        case NL80211_AUTHTYPE_SHARED_KEY:
            hddLog(VOS_TRACE_LEVEL_INFO,
                    "%s: set authentication type to SHARED", __func__);
            pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_SHARED_KEY;
            break;
#ifdef FEATURE_WLAN_ESE
        case NL80211_AUTHTYPE_NETWORK_EAP:
            hddLog(VOS_TRACE_LEVEL_INFO,
                            "%s: set authentication type to CCKM WPA", __func__);
            pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_CCKM_WPA;//eCSR_AUTH_TYPE_CCKM_RSN needs to be handled as well if required.
            break;
#endif


        default:
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Unsupported authentication type %d", __func__,
                    auth_type);
            pHddStaCtx->conn_info.authType = eCSR_AUTH_TYPE_UNKNOWN;
            return -EINVAL;
    }

    pWextState->roamProfile.AuthType.authType[0] =
                                        pHddStaCtx->conn_info.authType;
    return 0;
}

/*
 * FUNCTION: wlan_hdd_set_akm_suite
 * This function is used to set the key mgmt type(PSK/8021x).
 *
 */
static int wlan_hdd_set_akm_suite( hdd_adapter_t *pAdapter,
                                   u32 key_mgmt
                                   )
{
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    ENTER();
    /* Should be in ieee802_11_defs.h */
#define WLAN_AKM_SUITE_8021X_SHA256 0x000FAC05
#define WLAN_AKM_SUITE_PSK_SHA256   0x000FAC06
    /*set key mgmt type*/
    switch(key_mgmt)
    {
        case WLAN_AKM_SUITE_PSK:
        case WLAN_AKM_SUITE_PSK_SHA256:
#ifdef WLAN_FEATURE_VOWIFI_11R
        case WLAN_AKM_SUITE_FT_PSK:
#endif
            hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting key mgmt type to PSK",
                    __func__);
            pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_PSK;
            break;

        case WLAN_AKM_SUITE_8021X_SHA256:
        case WLAN_AKM_SUITE_8021X:
#ifdef WLAN_FEATURE_VOWIFI_11R
        case WLAN_AKM_SUITE_FT_8021X:
#endif
            hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting key mgmt type to 8021x",
                    __func__);
            pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_802_1X;
            break;
#ifdef FEATURE_WLAN_ESE
#define WLAN_AKM_SUITE_CCKM         0x00409600 /* Should be in ieee802_11_defs.h */
#define IW_AUTH_KEY_MGMT_CCKM       8  /* Should be in linux/wireless.h */
        case WLAN_AKM_SUITE_CCKM:
            hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting key mgmt type to CCKM",
                            __func__);
            pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_CCKM;
            break;
#endif
#ifndef WLAN_AKM_SUITE_OSEN
#define WLAN_AKM_SUITE_OSEN         0x506f9a01 /* Should be in ieee802_11_defs.h */
#endif
        case WLAN_AKM_SUITE_OSEN:
            hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting key mgmt type to OSEN",
                            __func__);
            pWextState->authKeyMgmt |= IW_AUTH_KEY_MGMT_802_1X;
            break;

        default:
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Unsupported key mgmt type %d",
                    __func__, key_mgmt);
            return -EINVAL;

    }
    return 0;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_set_cipher
 * This function is used to set the encryption type
 * (NONE/WEP40/WEP104/TKIP/CCMP).
 */
static int wlan_hdd_cfg80211_set_cipher( hdd_adapter_t *pAdapter,
                                u32 cipher,
                                bool ucast
                                )
{
    eCsrEncryptionType encryptionType = eCSR_ENCRYPT_TYPE_NONE;
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    ENTER();

    if (!cipher)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: received cipher %d - considering none",
                __func__, cipher);
        encryptionType = eCSR_ENCRYPT_TYPE_NONE;
    }
    else
    {

        /*set encryption method*/
        switch (cipher)
        {
            case IW_AUTH_CIPHER_NONE:
                encryptionType = eCSR_ENCRYPT_TYPE_NONE;
                break;

            case WLAN_CIPHER_SUITE_WEP40:
                encryptionType = eCSR_ENCRYPT_TYPE_WEP40;
                break;

            case WLAN_CIPHER_SUITE_WEP104:
                encryptionType = eCSR_ENCRYPT_TYPE_WEP104;
                break;

            case WLAN_CIPHER_SUITE_TKIP:
                encryptionType = eCSR_ENCRYPT_TYPE_TKIP;
                break;

            case WLAN_CIPHER_SUITE_CCMP:
                encryptionType = eCSR_ENCRYPT_TYPE_AES;
                break;
#ifdef FEATURE_WLAN_WAPI
        case WLAN_CIPHER_SUITE_SMS4:
            encryptionType = eCSR_ENCRYPT_TYPE_WPI;
            break;
#endif

#ifdef FEATURE_WLAN_ESE
        case WLAN_CIPHER_SUITE_KRK:
            encryptionType = eCSR_ENCRYPT_TYPE_KRK;
            break;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        case WLAN_CIPHER_SUITE_BTK:
            encryptionType = eCSR_ENCRYPT_TYPE_BTK;
            break;
#endif
#endif
            default:
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Unsupported cipher type %d",
                        __func__, cipher);
                return -EOPNOTSUPP;
        }
    }

    if (ucast)
    {
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting unicast cipher type to %d",
                __func__, encryptionType);
        pHddStaCtx->conn_info.ucEncryptionType            = encryptionType;
        pWextState->roamProfile.EncryptionType.numEntries = 1;
        pWextState->roamProfile.EncryptionType.encryptionType[0] =
                                          encryptionType;
    }
    else
    {
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: setting mcast cipher type to %d",
                __func__, encryptionType);
        pHddStaCtx->conn_info.mcEncryptionType                       = encryptionType;
        pWextState->roamProfile.mcEncryptionType.numEntries        = 1;
        pWextState->roamProfile.mcEncryptionType.encryptionType[0] = encryptionType;
    }

    return 0;
}


/*
 * FUNCTION: wlan_hdd_cfg80211_set_ie
 * This function is used to parse WPA/RSN IE's.
 */
int wlan_hdd_cfg80211_set_ie( hdd_adapter_t *pAdapter,
                              u8 *ie,
                              size_t ie_len
                              )
{
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    u8 *genie = ie;
    v_U16_t remLen = ie_len;
#ifdef FEATURE_WLAN_WAPI
    v_U32_t akmsuite[MAX_NUM_AKM_SUITES];
    u16 *tmp;
    v_U16_t akmsuiteCount;
    int *akmlist;
#endif
    ENTER();

    /* clear previous assocAddIE */
    pWextState->assocAddIE.length = 0;
    pWextState->roamProfile.bWPSAssociation = VOS_FALSE;
    pWextState->roamProfile.bOSENAssociation = VOS_FALSE;

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
            case DOT11F_EID_WPA:
                if (4 > eLen) /* should have at least OUI which is 4 bytes so extra 2 bytes not needed */
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                              "%s: Invalid WPA IE", __func__);
                    return -EINVAL;
                }
                else if (0 == memcmp(&genie[0], "\x00\x50\xf2\x04", 4))
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set WPS IE(len %d)",
                            __func__, eLen + 2);

                    if (SIR_MAC_MAX_ADD_IE_LENGTH <
                            (pWextState->assocAddIE.length + eLen))
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE. "
                                                      "Need bigger buffer space");
                       VOS_ASSERT(0);
                       return -ENOMEM;
                    }
                    // WSC IE is saved to Additional IE ; it should be accumulated to handle WPS IE + P2P IE
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.bWPSAssociation = VOS_TRUE;
                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                }
                else if (0 == memcmp(&genie[0], "\x00\x50\xf2", 3))
                {
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set WPA IE (len %d)",__func__, eLen + 2);
                    memset( pWextState->WPARSNIE, 0, MAX_WPA_RSN_IE_LEN );
                    memcpy( pWextState->WPARSNIE, genie - 2, (eLen + 2) /*ie_len*/);
                    pWextState->roamProfile.pWPAReqIE = pWextState->WPARSNIE;
                    pWextState->roamProfile.nWPAReqIELength = eLen + 2;//ie_len;
                }
                else if ( (0 == memcmp(&genie[0], P2P_OUI_TYPE,
                                                         P2P_OUI_TYPE_SIZE)))
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set P2P IE(len %d)",
                            __func__, eLen + 2);

                    if (SIR_MAC_MAX_ADD_IE_LENGTH <
                            (pWextState->assocAddIE.length + eLen))
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE "
                                                      "Need bigger buffer space");
                       VOS_ASSERT(0);
                       return -ENOMEM;
                    }
                    // P2P IE is saved to Additional IE ; it should be accumulated to handle WPS IE + P2P IE
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                }
#ifdef WLAN_FEATURE_WFD
                else if ( (0 == memcmp(&genie[0], WFD_OUI_TYPE,
                                                         WFD_OUI_TYPE_SIZE))
                        /*Consider WFD IE, only for P2P Client */
                         && (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode) )
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set WFD IE(len %d)",
                            __func__, eLen + 2);

                    if (SIR_MAC_MAX_ADD_IE_LENGTH <
                            (pWextState->assocAddIE.length + eLen))
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE "
                                                      "Need bigger buffer space");
                       VOS_ASSERT(0);
                       return -ENOMEM;
                    }
                    // WFD IE is saved to Additional IE ; it should be accumulated to handle
                    // WPS IE + P2P IE + WFD IE
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                }
#endif
                /* Appending HS 2.0 Indication Element in Association Request */
                else if ( (0 == memcmp(&genie[0], HS20_OUI_TYPE,
                                       HS20_OUI_TYPE_SIZE)) )
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set HS20 IE(len %d)",
                            __func__, eLen + 2);

                    if (SIR_MAC_MAX_ADD_IE_LENGTH <
                            (pWextState->assocAddIE.length + eLen))
                    {
                        hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE "
                               "Need bigger buffer space");
                        VOS_ASSERT(0);
                        return -ENOMEM;
                    }
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                }
                 /* Appending OSEN Information  Element in Association Request */
                else if ( (0 == memcmp(&genie[0], OSEN_OUI_TYPE,
                                       OSEN_OUI_TYPE_SIZE)) )
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set OSEN IE(len %d)",
                            __func__, eLen + 2);

                    if ( SIR_MAC_MAX_IE_LENGTH < (pWextState->assocAddIE.length + eLen) ) {
                           hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE "
                               "Need bigger buffer space");
                        VOS_ASSERT(0);
                        return -ENOMEM;
                    }
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.bOSENAssociation = VOS_TRUE;
                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                }
                break;
            case DOT11F_EID_RSN:
                hddLog (VOS_TRACE_LEVEL_INFO, "%s Set RSN IE(len %d)",__func__, eLen + 2);
                memset( pWextState->WPARSNIE, 0, MAX_WPA_RSN_IE_LEN );
                memcpy( pWextState->WPARSNIE, genie - 2, (eLen + 2)/*ie_len*/);
                pWextState->roamProfile.pRSNReqIE = pWextState->WPARSNIE;
                pWextState->roamProfile.nRSNReqIELength = eLen + 2; //ie_len;
                break;
                /* Appending Extended Capabilities with Interworking bit set
                 * in Assoc Req.
                 *
                 * In assoc req this EXT Cap will only be taken into account if
                 * interworkingService bit is set to 1. Currently
                 * driver is only interested in interworkingService capability
                 * from supplicant. If in future any other EXT Cap info is
                 * required from supplicat, it needs to be handled while
                 * sending Assoc Req in LIM.
                 */
            case DOT11F_EID_EXTCAP:
                {
                    v_U16_t curAddIELen = pWextState->assocAddIE.length;
                    hddLog (VOS_TRACE_LEVEL_INFO, "%s Set Extended CAPS IE(len %d)",
                            __func__, eLen + 2);

                    if (SIR_MAC_MAX_ADD_IE_LENGTH <
                            (pWextState->assocAddIE.length + eLen))
                    {
                       hddLog(VOS_TRACE_LEVEL_FATAL, "Cannot accommodate assocAddIE "
                                                      "Need bigger buffer space");
                       VOS_ASSERT(0);
                       return -ENOMEM;
                    }
                    memcpy( pWextState->assocAddIE.addIEdata + curAddIELen, genie - 2, eLen + 2);
                    pWextState->assocAddIE.length += eLen + 2;

                    pWextState->roamProfile.pAddIEAssoc = pWextState->assocAddIE.addIEdata;
                    pWextState->roamProfile.nAddIEAssocLength = pWextState->assocAddIE.length;
                    break;
                }
#ifdef FEATURE_WLAN_WAPI
            case WLAN_EID_WAPI:
                pAdapter->wapi_info.nWapiMode = 1;   //Setting WAPI Mode to ON=1
                hddLog(VOS_TRACE_LEVEL_INFO, "WAPI MODE IS %u",
                                          pAdapter->wapi_info.nWapiMode);
                tmp = (u16 *)ie;
                tmp = tmp + 2; // Skip element Id and Len, Version
                akmsuiteCount = WPA_GET_LE16(tmp);
                tmp = tmp + 1;
                akmlist = (int *)(tmp);
                if(akmsuiteCount <= MAX_NUM_AKM_SUITES)
                {
                    memcpy(akmsuite, akmlist, (4*akmsuiteCount));
                }
                else
                {
                    hddLog(VOS_TRACE_LEVEL_FATAL, "Invalid akmSuite count");
                    VOS_ASSERT(0);
                    return -EINVAL;
                }

                if (WAPI_PSK_AKM_SUITE == akmsuite[0])
                {
                    hddLog(VOS_TRACE_LEVEL_INFO, "%s: WAPI AUTH MODE SET TO PSK",
                                                            __func__);
                    pAdapter->wapi_info.wapiAuthMode = WAPI_AUTH_MODE_PSK;
                }
                if (WAPI_CERT_AKM_SUITE == akmsuite[0])
                {
                    hddLog(VOS_TRACE_LEVEL_INFO, "%s: WAPI AUTH MODE SET TO CERTIFICATE",
                                                             __func__);
                    pAdapter->wapi_info.wapiAuthMode = WAPI_AUTH_MODE_CERT;
                }
                break;
#endif
            default:
                hddLog (VOS_TRACE_LEVEL_ERROR,
                        "%s Set UNKNOWN IE %X", __func__, elementId);
                /* when Unknown IE is received we should break and continue
                 * to the next IE in the buffer instead we were returning
                 * so changing this to break */
                break;
        }
        genie += eLen;
        remLen -= eLen;
    }
    EXIT();
    return 0;
}

/*
 * FUNCTION: hdd_isWPAIEPresent
 * Parse the received IE to find the WPA IE
 *
 */
static bool hdd_isWPAIEPresent(u8 *ie, u8 ie_len)
{
    v_U8_t eLen = 0;
    v_U16_t remLen = ie_len;
    v_U8_t elementId = 0;

    while (remLen >= 2)
    {
        elementId = *ie++;
        eLen  = *ie++;
        remLen -= 2;
        if (eLen > remLen)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: IE length is wrong %d", __func__, eLen);
            return FALSE;
        }
        if ((elementId == DOT11F_EID_WPA) && (remLen > 5))
        {
          /* OUI - 0x00 0X50 0XF2
             WPA Information Element - 0x01
             WPA version - 0x01*/
            if (0 == memcmp(&ie[0], "\x00\x50\xf2\x01\x01", 5))
               return TRUE;
        }
        ie += eLen;
        remLen -= eLen;
    }
    return FALSE;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_set_privacy
 * This function is used to initialize the security
 * parameters during connect operation.
 */
int wlan_hdd_cfg80211_set_privacy( hdd_adapter_t *pAdapter,
                                   struct cfg80211_connect_params *req
                                   )
{
    int status = 0;
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    ENTER();

    /*set wpa version*/
    pWextState->wpaVersion = IW_AUTH_WPA_VERSION_DISABLED;

    if (req->crypto.wpa_versions)
    {
        if (NL80211_WPA_VERSION_1 == req->crypto.wpa_versions)
        {
            pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA;
        }
        else if (NL80211_WPA_VERSION_2 == req->crypto.wpa_versions)
        {
            pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA2;
        }
    }

    hddLog(VOS_TRACE_LEVEL_INFO, "%s: set wpa version to %d", __func__,
            pWextState->wpaVersion);

    /*set authentication type*/
    status = wlan_hdd_cfg80211_set_auth_type(pAdapter, req->auth_type);

    if (0 > status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: failed to set authentication type ", __func__);
        return status;
    }

    /*set key mgmt type*/
    if (req->crypto.n_akm_suites)
    {
        status = wlan_hdd_set_akm_suite(pAdapter, req->crypto.akm_suites[0]);
        if (0 > status)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to set akm suite",
                    __func__);
            return status;
        }
    }

    /*set pairwise cipher type*/
    if (req->crypto.n_ciphers_pairwise)
    {
        status = wlan_hdd_cfg80211_set_cipher(pAdapter,
                                      req->crypto.ciphers_pairwise[0], true);
        if (0 > status)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: failed to set unicast cipher type", __func__);
            return status;
        }
    }
    else
    {
        /*Reset previous cipher suite to none*/
        status = wlan_hdd_cfg80211_set_cipher(pAdapter, 0, true);
        if (0 > status)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: failed to set unicast cipher type", __func__);
            return status;
        }
    }

    /*set group cipher type*/
    status = wlan_hdd_cfg80211_set_cipher(pAdapter, req->crypto.cipher_group,
                                                                       false);

    if (0 > status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to set mcast cipher type",
                __func__);
        return status;
    }

#ifdef WLAN_FEATURE_11W
    pWextState->roamProfile.MFPEnabled = (req->mfp == NL80211_MFP_REQUIRED);
#endif

    /* Parse WPA/RSN IE, and set the corresponding fields in Roam profile */
    if (req->ie_len)
    {
        status = wlan_hdd_cfg80211_set_ie(pAdapter, req->ie, req->ie_len);
        if ( 0 > status)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to parse the WPA/RSN IE",
                    __func__);
            return status;
        }
    }

    /*incase of WEP set default key information*/
    if (req->key && req->key_len)
    {
        if ( (WLAN_CIPHER_SUITE_WEP40 == req->crypto.ciphers_pairwise[0])
                || (WLAN_CIPHER_SUITE_WEP104 == req->crypto.ciphers_pairwise[0])
          )
        {
            if ( IW_AUTH_KEY_MGMT_802_1X
                    == (pWextState->authKeyMgmt & IW_AUTH_KEY_MGMT_802_1X  ))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Dynamic WEP not supported",
                        __func__);
                return -EOPNOTSUPP;
            }
            else
            {
                u8 key_len = req->key_len;
                u8 key_idx = req->key_idx;

                if ((eCSR_SECURITY_WEP_KEYSIZE_MAX_BYTES >= key_len)
                        && (CSR_MAX_NUM_KEY > key_idx)
                  )
                {
                    hddLog(VOS_TRACE_LEVEL_INFO,
                     "%s: setting default wep key, key_idx = %hu key_len %hu",
                            __func__, key_idx, key_len);
                    vos_mem_copy(
                       &pWextState->roamProfile.Keys.KeyMaterial[key_idx][0],
                                  req->key, key_len);
                    pWextState->roamProfile.Keys.KeyLength[key_idx] =
                                                               (u8)key_len;
                    pWextState->roamProfile.Keys.defaultIndex = (u8)key_idx;
                }
            }
        }
    }

    return status;
}

/*
 * FUNCTION: wlan_hdd_try_disconnect
 * This function is used to disconnect from previous
 * connection
 */
static int wlan_hdd_try_disconnect( hdd_adapter_t *pAdapter )
{
    unsigned long rc;
    hdd_station_ctx_t *pHddStaCtx;
    eMib_dot11DesiredBssType connectedBssType;

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    hdd_connGetConnectedBssType(pHddStaCtx,&connectedBssType );

    if((eMib_dot11DesiredBssType_independent == connectedBssType) ||
      (eConnectionState_Associated == pHddStaCtx->conn_info.connState) ||
      (eConnectionState_IbssConnected == pHddStaCtx->conn_info.connState))
    {
        /* Issue disconnect to CSR */
        INIT_COMPLETION(pAdapter->disconnect_comp_var);
        if( eHAL_STATUS_SUCCESS ==
              sme_RoamDisconnect( WLAN_HDD_GET_HAL_CTX(pAdapter),
                        pAdapter->sessionId,
                        eCSR_DISCONNECT_REASON_UNSPECIFIED ) )
        {
            rc = wait_for_completion_timeout(
                         &pAdapter->disconnect_comp_var,
                         msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
            if (!rc) {
                hddLog(LOGE, FL("Sme disconnect event timed out session Id %d"
                   " staDebugState %d"), pAdapter->sessionId,
                   pHddStaCtx->staDebugState);
                return -EALREADY;
            }
        }
    }
    else if(eConnectionState_Disconnecting == pHddStaCtx->conn_info.connState)
    {
        rc = wait_for_completion_timeout(
                     &pAdapter->disconnect_comp_var,
                     msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));
        if (!rc) {
            hddLog(LOGE, FL("Disconnect event timed out session Id %d"
               " staDebugState %d"), pAdapter->sessionId,
               pHddStaCtx->staDebugState);
            return -EALREADY;
        }
    }

    return 0;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_connect
 * This function is used to start the association process
 */
static int __wlan_hdd_cfg80211_connect( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      struct cfg80211_connect_params *req
                                      )
{
    int status;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( ndev );
    VOS_STATUS exitbmpsStatus = VOS_STATUS_E_INVAL;
    hdd_context_t *pHddCtx;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_CONNECT,
                     pAdapter->sessionId, pAdapter->device_mode));
    hddLog(VOS_TRACE_LEVEL_INFO,
             "%s: device_mode = %d",__func__,pAdapter->device_mode);

    if (pAdapter->device_mode != WLAN_HDD_INFRA_STATION &&
        pAdapter->device_mode != WLAN_HDD_P2P_CLIENT) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: device_mode is not supported", __func__);
        return -EINVAL;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (!pHddCtx) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is null", __func__);
        return -EINVAL;
    }

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    /* Supplicant indicate its decision to offload key management
     * by setting the third bit in flags in case of Secure connection
     * so if the supplicant does not support this then LFR3.0 shall
     * be disabled.if supplicant indicates support for offload
     * of key management then we shall enable LFR3.0.Note that
     * supplicant set the bit in flags means driver already indicated
     * its capability to handle the key management and LFR3.0 is
     * enabled in INI and FW also has the capability to handle
     * key management offload as part of LFR3.0
     */

#ifdef NL80211_KEY_LEN_PMK /* if kernel supports key mgmt offload */
    VOS_TRACE( VOS_MODULE_ID_HDD,
               VOS_TRACE_LEVEL_ERROR,
               "%s: LFR3:Supplicant association request flags %x",
               __func__, req->flags);
    if (!(req->flags & ASSOC_REQ_OFFLOAD_KEY_MGMT)) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
        FL("Supplicant does not support key mgmt offload for this AP"));
        sme_UpdateRoamKeyMgmtOffloadEnabled(pHddCtx->hHal,
                                            pAdapter->sessionId,
                                            FALSE);
    } else {
        sme_UpdateRoamKeyMgmtOffloadEnabled(pHddCtx->hHal,
                                            pAdapter->sessionId,
                                            TRUE);
    }
#else
    hddLog(VOS_TRACE_LEVEL_ERROR,
        FL("Kernel does not support key mgmt offload"));
    sme_UpdateRoamKeyMgmtOffloadEnabled(pHddCtx->hHal, pAdapter->sessionId, FALSE);
#endif /* #ifdef NL80211_KEY_LEN_PMK */

#endif
    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Reached max concurrent connections"));
        return -ECONNREFUSED;
    }

#if defined(FEATURE_WLAN_LFR) && defined(WLAN_FEATURE_ROAM_SCAN_OFFLOAD)
    wlan_hdd_disable_roaming(pAdapter);
#endif

#ifdef WLAN_BTAMP_FEATURE
    //Infra connect not supported when AMP traffic is on.
    if (VOS_TRUE == WLANBAP_AmpSessionOn()) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: No connection when AMP is on", __func__);
        return -ECONNREFUSED;
    }
#endif

    //If Device Mode is Station Concurrent Sessions Exit BMps
    //P2P Mode will be taken care in Open/close adapter
    if (!pHddCtx->cfg_ini->enablePowersaveOffload &&
        (WLAN_HDD_INFRA_STATION == pAdapter->device_mode) &&
        (vos_concurrent_open_sessions_running())) {
        exitbmpsStatus = hdd_disable_bmps_imps(pHddCtx,
                                               WLAN_HDD_INFRA_STATION);
    }

    /*Try disconnecting if already in connected state*/
    status = wlan_hdd_try_disconnect(pAdapter);
    if ( 0 > status) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Failed to disconnect the existing"
                " connection"));
        return -EALREADY;
    }

    /*initialise security parameters*/
    status = wlan_hdd_cfg80211_set_privacy(pAdapter, req);

    if (0 > status) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to set security params",
                __func__);
        return status;
    }

    if (req->channel) {
        status = wlan_hdd_cfg80211_connect_start(pAdapter, req->ssid,
                                                  req->ssid_len, req->bssid,
                          #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0))
                                                  req->bssid_hint,
                          #else
                                                  NULL,
                          #endif
                                                  req->channel->hw_value);
    } else {
        status = wlan_hdd_cfg80211_connect_start(pAdapter, req->ssid,
                                                  req->ssid_len, req->bssid,
                          #if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,15,0))
                                                  req->bssid_hint,
                          #else
                                                  NULL,
                          #endif
                                                  0);
    }

    if (0 != status) {
        //ReEnable BMPS if disabled
        // If PS offload is enabled, fw will take care of
// ps in cae of concurrency.
        if((VOS_STATUS_SUCCESS == exitbmpsStatus) &&
            (NULL != pHddCtx) && !pHddCtx->cfg_ini->enablePowersaveOffload) {
            if (pHddCtx->hdd_wlan_suspended) {
                hdd_set_pwrparams(pHddCtx);
            }
           //ReEnable Bmps and Imps back
           hdd_enable_bmps_imps(pHddCtx);
        }

        hddLog(VOS_TRACE_LEVEL_ERROR, FL("connect failed"));
        return status;
    }
#ifdef NL80211_KEY_LEN_PMK /* kernel supports key mgmt offload */
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    if ((eHAL_STATUS_SUCCESS == status) && (req->psk)) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: psk = %p", __func__, req->psk);
        /* since PMK is available only in cfg80211_connect(), we save it
         * even before we know connection succeeded or not */
        if (pHddCtx->cfg_ini->isRoamOffloadEnabled) {
             hdd_wext_state_t *pWextState =
                       WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
             tANI_U8 localPsk [SIR_ROAM_SCAN_PSK_SIZE];
             if ((pWextState->authKeyMgmt & IW_AUTH_KEY_MGMT_PSK)
#ifdef FEATURE_WLAN_ESE
                 && (!pWextState->isESEConnection)
#endif
                 ) {
                 hddLog(VOS_TRACE_LEVEL_ERROR, FL("calling sme_RoamSetPSK"));
                 vos_mem_copy(localPsk, req->psk, SIR_ROAM_SCAN_PSK_SIZE);
                 sme_RoamSetPSK_PMK(WLAN_HDD_GET_HAL_CTX(pAdapter),
                 pAdapter->sessionId, localPsk, SIR_ROAM_SCAN_PSK_SIZE);
             }
        }
    }
#endif
#endif
    pHddCtx->isAmpAllowed = VOS_FALSE;
    EXIT();
    return status;
}

static int wlan_hdd_cfg80211_connect( struct wiphy *wiphy,
                                      struct net_device *ndev,
                                      struct cfg80211_connect_params *req)
{
    int ret;
    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_connect(wiphy, ndev, req);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: wlan_hdd_disconnect
 * This function is used to issue a disconnect request to SME
 */
int wlan_hdd_disconnect( hdd_adapter_t *pAdapter, u16 reason )
{
    int status, result = 0;
    unsigned long rc;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    /*stop tx queues*/
    netif_tx_disable(pAdapter->dev);
    netif_carrier_off(pAdapter->dev);
    pHddCtx->isAmpAllowed = VOS_TRUE;
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: Set HDD connState to eConnectionState_Disconnecting",
                   __func__);
    pHddStaCtx->conn_info.connState = eConnectionState_Disconnecting;
    INIT_COMPLETION(pAdapter->disconnect_comp_var);

    /*issue disconnect*/

    status = sme_RoamDisconnect( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                 pAdapter->sessionId, reason);
    if (eHAL_STATUS_CMD_NOT_QUEUED == status) {
        hddLog(LOG1, FL("status = %d, already disconnected"), (int)status);
        goto disconnected;
    } else if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s csrRoamDisconnect failure, returned %d",
               __func__, (int)status );
        pHddStaCtx->staDebugState = status;
        result = -EINVAL;
        goto disconnected;
    }
    rc = wait_for_completion_timeout(
                &pAdapter->disconnect_comp_var,
                msecs_to_jiffies(WLAN_WAIT_TIME_DISCONNECT));

    if (!rc) {
       hddLog(VOS_TRACE_LEVEL_ERROR,
              "%s: Failed to disconnect, timed out", __func__);
       result = -ETIMEDOUT;
    }

disconnected:
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             FL("Set HDD connState to eConnectionState_NotConnected"));
    hdd_connSetConnectionState(pAdapter,
                                eConnectionState_NotConnected);

    return result;
}


/*
 * FUNCTION: __wlan_hdd_cfg80211_disconnect
 * This function is used to issue a disconnect request to SME
 */
static int __wlan_hdd_cfg80211_disconnect( struct wiphy *wiphy,
                                         struct net_device *dev,
                                         u16 reason
                                         )
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    int status;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
#ifdef FEATURE_WLAN_TDLS
    tANI_U8 staIdx;
#endif

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_DISCONNECT,
                     pAdapter->sessionId, reason));
    hddLog(VOS_TRACE_LEVEL_INFO, FL("device_mode (%d) reason code(%d)"),
                                 pAdapter->device_mode, reason);

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return status;
    }

    /* Issue disconnect request to SME, if station is in connected state */
    if ((pHddStaCtx->conn_info.connState == eConnectionState_Associated) ||
        (pHddStaCtx->conn_info.connState == eConnectionState_Connecting)) {
        eCsrRoamDisconnectReason reasonCode =
                                   eCSR_DISCONNECT_REASON_UNSPECIFIED;
        hdd_scaninfo_t *pScanInfo;
        switch (reason) {
        case WLAN_REASON_MIC_FAILURE:
             reasonCode = eCSR_DISCONNECT_REASON_MIC_ERROR;
             break;

        case WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY:
        case WLAN_REASON_DISASSOC_AP_BUSY:
        case WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA:
             reasonCode = eCSR_DISCONNECT_REASON_DISASSOC;
             break;

        case WLAN_REASON_PREV_AUTH_NOT_VALID:
        case WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA:
             reasonCode = eCSR_DISCONNECT_REASON_DEAUTH;
             break;

        case WLAN_REASON_DEAUTH_LEAVING:
             reasonCode = pHddCtx->cfg_ini->gEnableDeauthToDisassocMap ?
                 eCSR_DISCONNECT_REASON_STA_HAS_LEFT :
                 eCSR_DISCONNECT_REASON_DEAUTH;
             break;
        case WLAN_REASON_DISASSOC_STA_HAS_LEFT:
             reasonCode = eCSR_DISCONNECT_REASON_STA_HAS_LEFT;
             break;
        default:
             reasonCode = eCSR_DISCONNECT_REASON_UNSPECIFIED;
            break;
        }
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  FL("convert to internal reason %d to reasonCode %d"),
                  reason, reasonCode);
        pHddStaCtx->conn_info.connState = eConnectionState_NotConnected;
        pScanInfo =  &pAdapter->scan_info;
        if (pScanInfo->mScanPending) {
            hddLog(VOS_TRACE_LEVEL_INFO, "Disconnect is in progress, "
                          "Aborting Scan");
            hdd_abort_mac_scan(pHddCtx, pAdapter->sessionId,
                               eCSR_SCAN_ABORT_DEFAULT);
        }

        wlan_hdd_cleanup_remain_on_channel_ctx(pAdapter);
#ifdef FEATURE_WLAN_TDLS
        /* First clean up the tdls peers if any */
        for (staIdx = 0 ; staIdx < pHddCtx->max_num_tdls_sta; staIdx++) {
            if ((pHddCtx->tdlsConnInfo[staIdx].sessionId == pAdapter->sessionId) &&
                (pHddCtx->tdlsConnInfo[staIdx].staId)) {
                uint8 *mac;
                mac = pHddCtx->tdlsConnInfo[staIdx].peerMac.bytes;
                hddLog(TDLS_LOG_LEVEL,
                       "%s: call sme_DeleteTdlsPeerSta staId %d sessionId %d " MAC_ADDRESS_STR,
                       __func__, pHddCtx->tdlsConnInfo[staIdx].staId,
                        pAdapter->sessionId,
                        MAC_ADDR_ARRAY(mac));
                sme_DeleteTdlsPeerSta(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                      pAdapter->sessionId,
                                      mac);
            }
        }
#endif
        hddLog(LOG1, FL("Disconnecting with reasoncode:%u"), reasonCode);
        status = wlan_hdd_disconnect(pAdapter, reasonCode);
        if (0 != status) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("failure, returned %d"), status);
            return -EINVAL;
        }
    } else {
        hddLog(VOS_TRACE_LEVEL_ERROR,
               FL("unexpected cfg disconnect called while in state (%d)"),
               pHddStaCtx->conn_info.connState);
    }

    return status;
}

static int wlan_hdd_cfg80211_disconnect( struct wiphy *wiphy,
                                         struct net_device *dev,
                                         u16 reason
                                         )
{
    int ret;
    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_disconnect(wiphy, dev, reason);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_set_privacy_ibss
 * This function is used to initialize the security
 * settings in IBSS mode.
 */
static int wlan_hdd_cfg80211_set_privacy_ibss(
                                         hdd_adapter_t *pAdapter,
                                         struct cfg80211_ibss_params *params
                                         )
{
    int status = 0;
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    eCsrEncryptionType encryptionType = eCSR_ENCRYPT_TYPE_NONE;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    ENTER();

    pWextState->wpaVersion = IW_AUTH_WPA_VERSION_DISABLED;
    vos_mem_zero(&pHddStaCtx->ibss_enc_key, sizeof(tCsrRoamSetKey));
    pHddStaCtx->ibss_enc_key_installed = 0;

    if (params->ie_len && ( NULL != params->ie) )
    {
        if (wlan_hdd_cfg80211_get_ie_ptr (params->ie,
                            params->ie_len, WLAN_EID_RSN ))
        {
            pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA2;
            encryptionType = eCSR_ENCRYPT_TYPE_AES;
        }
        else if ( hdd_isWPAIEPresent (params->ie, params->ie_len ))
        {
            tDot11fIEWPA dot11WPAIE;
            tHalHandle halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);
            u8 *ie;

            memset(&dot11WPAIE, 0, sizeof(dot11WPAIE));
            ie = wlan_hdd_cfg80211_get_ie_ptr (params->ie,
                                     params->ie_len, DOT11F_EID_WPA);
            if ( NULL != ie )
            {
                pWextState->wpaVersion = IW_AUTH_WPA_VERSION_WPA;
                // Unpack the WPA IE
                //Skip past the EID byte and length byte - and four byte WiFi OUI
                dot11fUnpackIeWPA((tpAniSirGlobal) halHandle,
                                &ie[2+4],
                                ie[1] - 4,
                                &dot11WPAIE);
                /*Extract the multicast cipher, the encType for unicast
                               cipher for wpa-none is none*/
                encryptionType =
                  hdd_TranslateWPAToCsrEncryptionType(dot11WPAIE.multicast_cipher);
            }
        }

        status = wlan_hdd_cfg80211_set_ie(pAdapter, params->ie, params->ie_len);

        if (0 > status)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to parse WPA/RSN IE",
                    __func__);
            return status;
        }
    }

    pWextState->roamProfile.AuthType.authType[0] =
                                pHddStaCtx->conn_info.authType =
                                eCSR_AUTH_TYPE_OPEN_SYSTEM;

    if (params->privacy)
    {
        /* Security enabled IBSS, At this time there is no information available
         * about the security parameters, so initialise the encryption type to
         * eCSR_ENCRYPT_TYPE_WEP40_STATICKEY.
         * The correct security parameters will be updated later in
         * wlan_hdd_cfg80211_add_key */
        /* Hal expects encryption type to be set inorder
         *enable privacy bit in beacons */

        encryptionType = eCSR_ENCRYPT_TYPE_WEP40_STATICKEY;
    }
    VOS_TRACE (VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                          "encryptionType=%d", encryptionType);
    pHddStaCtx->conn_info.ucEncryptionType                   = encryptionType;
    pWextState->roamProfile.EncryptionType.numEntries        = 1;
    pWextState->roamProfile.EncryptionType.encryptionType[0] = encryptionType;
    return status;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_join_ibss
 * This function is used to create/join an IBSS
 */
static int __wlan_hdd_cfg80211_join_ibss(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         struct cfg80211_ibss_params *params)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    tCsrRoamProfile          *pRoamProfile;
    int status;
    bool alloc_bssid = VOS_FALSE;
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_JOIN_IBSS,
                     pAdapter->sessionId, pAdapter->device_mode));
    hddLog(VOS_TRACE_LEVEL_INFO,
                  "%s: device_mode = %d",__func__,pAdapter->device_mode);

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if (vos_max_concurrent_connections_reached()) {
        hddLog(VOS_TRACE_LEVEL_DEBUG, FL("Reached max concurrent connections"));
        return -ECONNREFUSED;
    }

    /*Try disconnecting if already in connected state*/
    status = wlan_hdd_try_disconnect(pAdapter);
    if ( 0 > status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Failed to disconnect the existing"
                " IBSS connection"));
        return -EALREADY;
    }

    pRoamProfile = &pWextState->roamProfile;

    if ( eCSR_BSS_TYPE_START_IBSS != pRoamProfile->BSSType )
    {
        hddLog (VOS_TRACE_LEVEL_ERROR,
                "%s Interface type is not set to IBSS", __func__);
        return -EINVAL;
    }

    /* enable selected protection checks in IBSS mode */
    pRoamProfile->cfg_protection = IBSS_CFG_PROTECTION_ENABLE_MASK;

    if (eHAL_STATUS_FAILURE == ccmCfgSetInt( pHddCtx->hHal,
                                             WNI_CFG_IBSS_ATIM_WIN_SIZE,
                                             pHddCtx->cfg_ini->ibssATIMWinSize,
                                             NULL,
                                             eANI_BOOLEAN_FALSE))
    {
        hddLog(LOGE,
               "%s: Could not pass on WNI_CFG_IBSS_ATIM_WIN_SIZE to CCM",
               __func__);
    }

    /* BSSID is provided by upper layers hence no need to AUTO generate */
    if (NULL != params->bssid) {
       if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IBSS_AUTO_BSSID, 0,
                        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE) {
           hddLog (VOS_TRACE_LEVEL_ERROR,
                   "%s:ccmCfgStInt faild for WNI_CFG_IBSS_AUTO_BSSID", __func__);
           return -EIO;
       }
    }
    else if(pHddCtx->cfg_ini->isCoalesingInIBSSAllowed == 0)
    {
        if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IBSS_AUTO_BSSID, 0,
                         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
        {
            hddLog (VOS_TRACE_LEVEL_ERROR,
                    "%s:ccmCfgStInt faild for WNI_CFG_IBSS_AUTO_BSSID", __func__);
            return -EIO;
        }
        params->bssid = vos_mem_malloc(VOS_MAC_ADDR_SIZE);
        if (!params->bssid) {
            hddLog(VOS_TRACE_LEVEL_ERROR, FL("Memory allocation failed"));
            return -ENOMEM;
        }
        vos_mem_copy((v_U8_t *)params->bssid,
                     (v_U8_t *)&pHddCtx->cfg_ini->IbssBssid.bytes[0],
                     VOS_MAC_ADDR_SIZE);
        alloc_bssid = VOS_TRUE;
    }
    if ((params->beacon_interval > CFG_BEACON_INTERVAL_MIN)
        && (params->beacon_interval <= CFG_BEACON_INTERVAL_MAX))
        pRoamProfile->beaconInterval = params->beacon_interval;
    else {
        pRoamProfile->beaconInterval = CFG_BEACON_INTERVAL_DEFAULT;
        hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                "%s: input beacon interval %d TU is invalid, use default %d TU",
                __func__, params->beacon_interval,
                pRoamProfile->beaconInterval);
    }

    /* Set Channel */
    if (NULL !=
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0))
                params->chandef.chan)
#else
                params->channel)
#endif
    {
        u8 channelNum;
        v_U32_t numChans = WNI_CFG_VALID_CHANNEL_LIST_LEN;
        v_U8_t validChan[WNI_CFG_VALID_CHANNEL_LIST_LEN];
        tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
        int indx;

        /* Get channel number */
        channelNum =
               ieee80211_frequency_to_channel(
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0))
                                            params->chandef.chan->center_freq);
#else
                                            params->channel->center_freq);
#endif

        if (0 != ccmCfgGetStr(hHal, WNI_CFG_VALID_CHANNEL_LIST,
                    validChan, &numChans))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: No valid channel list",
                    __func__);
            return -EOPNOTSUPP;
        }

        for (indx = 0; indx < numChans; indx++)
        {
            if (channelNum == validChan[indx])
            {
                break;
            }
        }
        if (indx >= numChans)
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Not valid Channel %d",
                    __func__, channelNum);
            return -EINVAL;
        }
        /* Set the Operational Channel */
        hddLog(VOS_TRACE_LEVEL_INFO_HIGH, "%s: set channel %d", __func__,
                channelNum);
        pRoamProfile->ChannelInfo.numOfChannels = 1;
        pHddStaCtx->conn_info.operationChannel = channelNum;
        pRoamProfile->ChannelInfo.ChannelList =
            &pHddStaCtx->conn_info.operationChannel;
    }

    /* Initialize security parameters */
    status = wlan_hdd_cfg80211_set_privacy_ibss(pAdapter, params);
    if (status < 0)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: failed to set security parameters",
                __func__);
        return status;
    }

    /* Issue connect start */
    status = wlan_hdd_cfg80211_connect_start(pAdapter, params->ssid,
            params->ssid_len, params->bssid, NULL,
            pHddStaCtx->conn_info.operationChannel);

    if (0 > status)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: connect failed", __func__);
        return status;
    }

    if (NULL != params->bssid &&
        pHddCtx->cfg_ini->isCoalesingInIBSSAllowed == 0 &&
        alloc_bssid == VOS_TRUE)
    {
        vos_mem_free(params->bssid);
    }
   return 0;
}

static int wlan_hdd_cfg80211_join_ibss(struct wiphy *wiphy,
                                       struct net_device *dev,
                                       struct cfg80211_ibss_params *params)
{
    int ret = 0;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_join_ibss(wiphy, dev, params);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_leave_ibss
 * This function is used to leave an IBSS
 */
static int __wlan_hdd_cfg80211_leave_ibss(struct wiphy *wiphy,
                                          struct net_device *dev)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
    tCsrRoamProfile *pRoamProfile;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    int status;
    eHalStatus hal_status;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_LEAVE_IBSS,
                     pAdapter->sessionId, eCSR_DISCONNECT_REASON_IBSS_LEAVE));
    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return status;
    }

    hddLog(VOS_TRACE_LEVEL_INFO, FL("device_mode = %d"), pAdapter->device_mode);
    if (NULL == pWextState) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("Data Storage Corruption"));
        return -EIO;
    }

    pRoamProfile = &pWextState->roamProfile;

    /* Issue disconnect only if interface type is set to IBSS */
    if (eCSR_BSS_TYPE_START_IBSS != pRoamProfile->BSSType) {
        hddLog (VOS_TRACE_LEVEL_ERROR, FL("BSS Type is not set to IBSS"));
        return -EINVAL;
    }

    /* Issue Disconnect request */
    INIT_COMPLETION(pAdapter->disconnect_comp_var);
    hal_status = sme_RoamDisconnect(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                    pAdapter->sessionId,
                                    eCSR_DISCONNECT_REASON_IBSS_LEAVE);
    if (!HAL_STATUS_SUCCESS(hal_status)) {
        hddLog(LOGE,
               FL("sme_RoamDisconnect failed hal_status(%d)"), hal_status);
        return -EAGAIN;
    }

    return 0;
}

static int wlan_hdd_cfg80211_leave_ibss(struct wiphy *wiphy,
                                        struct net_device *dev)
{
    int ret = 0;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_leave_ibss(wiphy, dev);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_set_wiphy_params
 * This function is used to set the phy parameters
 * (RTS Threshold/FRAG Threshold/Retry Count etc ...)
 */
static int __wlan_hdd_cfg80211_set_wiphy_params(struct wiphy *wiphy,
        u32 changed)
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    tHalHandle hHal = pHddCtx->hHal;
    int status;

    ENTER();
     MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                      TRACE_CODE_HDD_CFG80211_SET_WIPHY_PARAMS,
                      NO_SESSION, wiphy->rts_threshold));
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if (changed & WIPHY_PARAM_RTS_THRESHOLD)
    {
        u32 rts_threshold = (wiphy->rts_threshold == -1) ?
                               WNI_CFG_RTS_THRESHOLD_STAMAX :
                               wiphy->rts_threshold;

        if ((WNI_CFG_RTS_THRESHOLD_STAMIN > rts_threshold) ||
                (WNI_CFG_RTS_THRESHOLD_STAMAX < rts_threshold))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Invalid RTS Threshold value %u",
                    __func__, rts_threshold);
            return -EINVAL;
        }

        if (0 != ccmCfgSetInt(hHal, WNI_CFG_RTS_THRESHOLD,
                    rts_threshold, ccmCfgSetCallback,
                    eANI_BOOLEAN_TRUE))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: ccmCfgSetInt failed for rts_threshold value %u",
                    __func__, rts_threshold);
            return -EIO;
        }

        hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: set rts threshold %u", __func__,
                rts_threshold);
    }

    if (changed & WIPHY_PARAM_FRAG_THRESHOLD)
    {
        u16 frag_threshold = (wiphy->frag_threshold == -1) ?
                                WNI_CFG_FRAGMENTATION_THRESHOLD_STAMAX :
                                wiphy->frag_threshold;

        if ((WNI_CFG_FRAGMENTATION_THRESHOLD_STAMIN > frag_threshold)||
                (WNI_CFG_FRAGMENTATION_THRESHOLD_STAMAX < frag_threshold) )
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: Invalid frag_threshold value %hu", __func__,
                    frag_threshold);
            return -EINVAL;
        }

        if (0 != ccmCfgSetInt(hHal, WNI_CFG_FRAGMENTATION_THRESHOLD,
                    frag_threshold, ccmCfgSetCallback,
                    eANI_BOOLEAN_TRUE))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR,
                    "%s: ccmCfgSetInt failed for frag_threshold value %hu",
                    __func__, frag_threshold);
            return -EIO;
        }

        hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: set frag threshold %hu", __func__,
                frag_threshold);
    }

    if ((changed & WIPHY_PARAM_RETRY_SHORT)
            || (changed & WIPHY_PARAM_RETRY_LONG))
    {
        u8 retry_value = (changed & WIPHY_PARAM_RETRY_SHORT) ?
                         wiphy->retry_short :
                         wiphy->retry_long;

        if ((WNI_CFG_LONG_RETRY_LIMIT_STAMIN > retry_value) ||
                (WNI_CFG_LONG_RETRY_LIMIT_STAMAX < retry_value))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid Retry count %hu",
                    __func__, retry_value);
            return -EINVAL;
        }

        if (changed & WIPHY_PARAM_RETRY_SHORT)
        {
            if (0 != ccmCfgSetInt(hHal, WNI_CFG_LONG_RETRY_LIMIT,
                        retry_value, ccmCfgSetCallback,
                        eANI_BOOLEAN_TRUE))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: ccmCfgSetInt failed for long retry count %hu",
                        __func__, retry_value);
                return -EIO;
            }
            hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: set long retry count %hu",
                    __func__, retry_value);
        }
        else if (changed & WIPHY_PARAM_RETRY_SHORT)
        {
            if (0 != ccmCfgSetInt(hHal, WNI_CFG_SHORT_RETRY_LIMIT,
                        retry_value, ccmCfgSetCallback,
                        eANI_BOOLEAN_TRUE))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR,
                        "%s: ccmCfgSetInt failed for short retry count %hu",
                        __func__, retry_value);
                return -EIO;
            }
            hddLog(VOS_TRACE_LEVEL_INFO_MED, "%s: set short retry count %hu",
                    __func__, retry_value);
        }
    }


    return 0;
}

static int wlan_hdd_cfg80211_set_wiphy_params(struct wiphy *wiphy,
                                                                u32 changed)
{
     int ret;

     vos_ssr_protect(__func__);
     ret = __wlan_hdd_cfg80211_set_wiphy_params(wiphy, changed);
     vos_ssr_unprotect(__func__);

     return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_set_txpower
 * This function is used to set the txpower
 */
static int __wlan_hdd_cfg80211_set_txpower(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
        struct wireless_dev *wdev,
#endif
        enum nl80211_tx_power_setting type,
        int dbm)
{
    hdd_context_t *pHddCtx = (hdd_context_t*) wiphy_priv(wiphy);
    tHalHandle hHal = NULL;
    tSirMacAddr bssid = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    tSirMacAddr selfMac = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    int status;

    ENTER();
    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_TXPOWER,
                     NO_SESSION, type ));

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return status;
    }

    hHal = pHddCtx->hHal;

    if (0 != ccmCfgSetInt(hHal, WNI_CFG_CURRENT_TX_POWER_LEVEL,
                dbm, ccmCfgSetCallback,
                eANI_BOOLEAN_TRUE)) {
        hddLog(LOGE, FL("ccmCfgSetInt failed for tx power %hu"), dbm);
        return -EIO;
    }

    hddLog(VOS_TRACE_LEVEL_INFO_MED, FL("Set tx power level %d dbm"), dbm);

    switch (type) {
    /* Automatically determine transmit power */
    case NL80211_TX_POWER_AUTOMATIC:
    /* Fall through */
    case NL80211_TX_POWER_LIMITED: /* Limit TX power by the mBm parameter */
      if (sme_SetMaxTxPower(hHal, bssid, selfMac, dbm) != eHAL_STATUS_SUCCESS) {
          hddLog(LOGE, FL("Setting maximum tx power failed"));
          return -EIO;
      }
      break;

    case NL80211_TX_POWER_FIXED: /* Fix TX power to the mBm parameter */
       hddLog(LOGE, FL("NL80211_TX_POWER_FIXED not supported"));
       return -EOPNOTSUPP;
       break;

    default:
       hddLog(LOGE, FL("Invalid power setting type %d"), type);
       return -EIO;
    }

    EXIT();
    return 0;
}

static int wlan_hdd_cfg80211_set_txpower(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
        struct wireless_dev *wdev,
#endif
        enum nl80211_tx_power_setting type,
        int dbm)
{
    int ret;
    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_set_txpower(wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
                                        wdev,
#endif
                                        type,
                                        dbm);
   vos_ssr_unprotect(__func__);

   return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_get_txpower
 * This function is used to read the txpower
 */
static int wlan_hdd_cfg80211_get_txpower(struct wiphy *wiphy,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
                                         struct wireless_dev *wdev,
#endif
                                         int *dbm)
{

    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx = (hdd_context_t*) wiphy_priv(wiphy);
    int status;

    ENTER();

    status = wlan_hdd_validate_context(pHddCtx);
    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        *dbm = 0;
        return status;
    }

    pAdapter = hdd_get_adapter(pHddCtx, WLAN_HDD_INFRA_STATION);
    if (NULL == pAdapter) {
        hddLog(VOS_TRACE_LEVEL_FATAL, FL("pAdapter is NULL"));
        return -ENOENT;
    }

    wlan_hdd_get_classAstats(pAdapter);
    *dbm = pAdapter->hdd_stats.ClassA_stat.max_pwr;

    EXIT();
    return 0;
}


static int __wlan_hdd_cfg80211_get_station(struct wiphy *wiphy,
                                           struct net_device *dev,
                                           u8* mac, struct station_info *sinfo)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR( dev );
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    int ssidlen = pHddStaCtx->conn_info.SSID.SSID.length;
    tANI_U8 rate_flags;

    hdd_context_t *pHddCtx = (hdd_context_t*) wiphy_priv(wiphy);
    hdd_config_t  *pCfg    = pHddCtx->cfg_ini;

    tANI_U8  OperationalRates[CSR_DOT11_SUPPORTED_RATES_MAX];
    tANI_U32 ORLeng = CSR_DOT11_SUPPORTED_RATES_MAX;
    tANI_U8  ExtendedRates[CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX];
    tANI_U32 ERLeng = CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX;
    tANI_U8  MCSRates[SIZE_OF_BASIC_MCS_SET];
    tANI_U32 MCSLeng = SIZE_OF_BASIC_MCS_SET;
    tANI_U16 maxRate = 0;
    tANI_U16 myRate;
    tANI_U16 currentRate = 0;
    tANI_U8  maxSpeedMCS = 0;
    tANI_U8  maxMCSIdx = 0;
    tANI_U8  rateFlag = 1;
    tANI_U8  i, j, rssidx;
    tANI_U8  nss = 1;
    int status, mode = 0, maxHtIdx;
    struct index_vht_data_rate_type *supported_vht_mcs_rate;
    struct index_data_rate_type *supported_mcs_rate;

#ifdef WLAN_FEATURE_11AC
    tANI_U32 vht_mcs_map;
    eDataRate11ACMaxMcs vhtMaxMcs;
#endif /* WLAN_FEATURE_11AC */

    ENTER();

    if ((eConnectionState_Associated != pHddStaCtx->conn_info.connState) ||
            (0 == ssidlen))
    {
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: Not associated or"
                    " Invalid ssidlen, %d", __func__, ssidlen);
        /*To keep GUI happy*/
        return 0;
    }

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    wlan_hdd_get_rssi(pAdapter, &sinfo->signal);
    sinfo->filled |= STATION_INFO_SIGNAL;

#ifdef WLAN_FEATURE_LPSS
    if (!pAdapter->rssi_send) {
        pAdapter->rssi_send = VOS_TRUE;
        wlan_hdd_send_status_pkg(pAdapter, pHddStaCtx, 1, 1);
    }
#endif

    wlan_hdd_get_station_stats(pAdapter);
    rate_flags = pAdapter->hdd_stats.ClassA_stat.tx_rate_flags;

    //convert to the UI units of 100kbps
    myRate = pAdapter->hdd_stats.ClassA_stat.tx_rate * 5;
    if (!(rate_flags & eHAL_TX_RATE_LEGACY)) {
        nss = pAdapter->hdd_stats.ClassA_stat.rx_frag_cnt;

        if (eHDD_LINK_SPEED_REPORT_ACTUAL == pCfg->reportMaxLinkSpeed) {
            /* Get current rate flags if report actual */
            rate_flags = pAdapter->hdd_stats.ClassA_stat.promiscuous_rx_frag_cnt;
        }

        if (pAdapter->hdd_stats.ClassA_stat.mcs_index == INVALID_MCS_IDX) {
            rate_flags = eHAL_TX_RATE_LEGACY;
            pAdapter->hdd_stats.ClassA_stat.mcs_index = 0;
        }
    }
#ifdef LINKSPEED_DEBUG_ENABLED
    pr_info("RSSI %d, RLMS %u, rate %d, rssi high %d, rssi mid %d, rssi low %d, rate_flags 0x%x, MCS %d\n",
            sinfo->signal,
            pCfg->reportMaxLinkSpeed,
            myRate,
            (int) pCfg->linkSpeedRssiHigh,
            (int) pCfg->linkSpeedRssiMid,
            (int) pCfg->linkSpeedRssiLow,
            (int) rate_flags,
            (int) pAdapter->hdd_stats.ClassA_stat.mcs_index);
#endif //LINKSPEED_DEBUG_ENABLED

    if (eHDD_LINK_SPEED_REPORT_ACTUAL != pCfg->reportMaxLinkSpeed)
    {
        // we do not want to necessarily report the current speed
        if (eHDD_LINK_SPEED_REPORT_MAX == pCfg->reportMaxLinkSpeed)
        {
            // report the max possible speed
            rssidx = 0;
        }
        else if (eHDD_LINK_SPEED_REPORT_MAX_SCALED == pCfg->reportMaxLinkSpeed)
        {
            // report the max possible speed with RSSI scaling
            if (sinfo->signal >= pCfg->linkSpeedRssiHigh)
            {
                // report the max possible speed
                rssidx = 0;
            }
            else if (sinfo->signal >= pCfg->linkSpeedRssiMid)
            {
                // report middle speed
                rssidx = 1;
            }
            else if (sinfo->signal >= pCfg->linkSpeedRssiLow)
            {
                // report middle speed
                rssidx = 2;
            }
            else
            {
                // report actual speed
                rssidx = 3;
            }
        }
        else
        {
            // unknown, treat as eHDD_LINK_SPEED_REPORT_MAX
            hddLog(VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid value for reportMaxLinkSpeed: %u",
                    __func__, pCfg->reportMaxLinkSpeed);
            rssidx = 0;
        }

        maxRate = 0;

        /* Get Basic Rate Set */
        if (0 != ccmCfgGetStr(WLAN_HDD_GET_HAL_CTX(pAdapter), WNI_CFG_OPERATIONAL_RATE_SET,
                             OperationalRates, &ORLeng))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: ccm api returned failure", __func__);
            /*To keep GUI happy*/
            return 0;
        }

        for (i = 0; i < ORLeng; i++)
        {
            for (j = 0; j < (sizeof(supported_data_rate) / sizeof(supported_data_rate[0])); j ++)
            {
                /* Validate Rate Set */
                if (supported_data_rate[j].beacon_rate_index == (OperationalRates[i] & 0x7F))
                {
                    currentRate = supported_data_rate[j].supported_rate[rssidx];
                    break;
                }
            }
            /* Update MAX rate */
            maxRate = (currentRate > maxRate)?currentRate:maxRate;
        }

        /* Get Extended Rate Set */
        if (0 != ccmCfgGetStr(WLAN_HDD_GET_HAL_CTX(pAdapter), WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
                             ExtendedRates, &ERLeng))
        {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s: ccm api returned failure", __func__);
            /*To keep GUI happy*/
            return 0;
        }

        for (i = 0; i < ERLeng; i++)
        {
            for (j = 0; j < (sizeof(supported_data_rate) / sizeof(supported_data_rate[0])); j ++)
            {
                if (supported_data_rate[j].beacon_rate_index == (ExtendedRates[i] & 0x7F))
                {
                    currentRate = supported_data_rate[j].supported_rate[rssidx];
                    break;
                }
            }
            /* Update MAX rate */
            maxRate = (currentRate > maxRate)?currentRate:maxRate;
        }
        /* Get MCS Rate Set --
           Only if we are connected in non legacy mode and not reporting
           actual speed */
         if ((3 != rssidx) &&
              !(rate_flags & eHAL_TX_RATE_LEGACY))
        {
            if (0 != ccmCfgGetStr(WLAN_HDD_GET_HAL_CTX(pAdapter), WNI_CFG_CURRENT_MCS_SET,
                                 MCSRates, &MCSLeng))
            {
                hddLog(VOS_TRACE_LEVEL_ERROR, "%s: ccm api returned failure", __func__);
                /*To keep GUI happy*/
                return 0;
            }
            rateFlag = 0;
#ifdef WLAN_FEATURE_11AC
            supported_vht_mcs_rate = (struct index_vht_data_rate_type *)
                                      ((nss == 1)?
                                       &supported_vht_mcs_rate_nss1 :
                                       &supported_vht_mcs_rate_nss2);

            if (rate_flags & eHAL_TX_RATE_VHT80)
                mode = 2;
            else if ((rate_flags & eHAL_TX_RATE_VHT40) ||
                     (rate_flags & eHAL_TX_RATE_HT40))
                mode = 1;
            else
                mode = 0;

            /* VHT80 rate has separate rate table */
            if (rate_flags & (eHAL_TX_RATE_VHT20|eHAL_TX_RATE_VHT40|eHAL_TX_RATE_VHT80))
            {
                ccmCfgGetInt(WLAN_HDD_GET_HAL_CTX(pAdapter), WNI_CFG_VHT_TX_MCS_MAP, &vht_mcs_map);
                vhtMaxMcs = (eDataRate11ACMaxMcs)(vht_mcs_map & DATA_RATE_11AC_MCS_MASK );
                if (rate_flags & eHAL_TX_RATE_SGI)
                {
                    rateFlag |= 1;
                }
                if (DATA_RATE_11AC_MAX_MCS_7 == vhtMaxMcs)
                {
                    maxMCSIdx = 7;
                }
                else if (DATA_RATE_11AC_MAX_MCS_8 == vhtMaxMcs)
                {
                    maxMCSIdx = 8;
                }
                else if (DATA_RATE_11AC_MAX_MCS_9 == vhtMaxMcs)
                {
                    //VHT20 is supporting 0~8
                    if (rate_flags & eHAL_TX_RATE_VHT20)
                        maxMCSIdx = 8;
                    else
                        maxMCSIdx = 9;
                }

                if (rssidx != 0)
                {
                    for (i=0; i <= maxMCSIdx ; i++)
                    {
                         if (sinfo->signal <= rssiMcsTbl[mode][i])
                         {
                             maxMCSIdx = i;
                             break;
                         }
                    }
                }

                if (rate_flags & eHAL_TX_RATE_VHT80)
                {
                    currentRate = supported_vht_mcs_rate[pAdapter->hdd_stats.ClassA_stat.mcs_index].supported_VHT80_rate[rateFlag];
                    maxRate = supported_vht_mcs_rate[maxMCSIdx].supported_VHT80_rate[rateFlag];
                }
                else if (rate_flags & eHAL_TX_RATE_VHT40)
                {
                    currentRate = supported_vht_mcs_rate[pAdapter->hdd_stats.ClassA_stat.mcs_index].supported_VHT40_rate[rateFlag];
                    maxRate = supported_vht_mcs_rate[maxMCSIdx].supported_VHT40_rate[rateFlag];
                }
                else if (rate_flags & eHAL_TX_RATE_VHT20)
                {
                    currentRate = supported_vht_mcs_rate[pAdapter->hdd_stats.ClassA_stat.mcs_index].supported_VHT20_rate[rateFlag];
                    maxRate = supported_vht_mcs_rate[maxMCSIdx].supported_VHT20_rate[rateFlag];
                }

                maxSpeedMCS = 1;
                if (currentRate > maxRate)
                {
                    maxRate = currentRate;
                }

            }
            else
#endif /* WLAN_FEATURE_11AC */
            {
                if (rate_flags & eHAL_TX_RATE_HT40)
                {
                    rateFlag |= 1;
                }
                if (rate_flags & eHAL_TX_RATE_SGI)
                {
                    rateFlag |= 2;
                }

                supported_mcs_rate = (struct index_data_rate_type *)
                                      ((nss == 1)? &supported_mcs_rate_nss1 :
                                                   &supported_mcs_rate_nss2);

                maxHtIdx = MAX_HT_MCS_IDX;
                if (rssidx != 0)
                {
                    for (i=0; i < MAX_HT_MCS_IDX; i++)
                    {
                         if (sinfo->signal <= rssiMcsTbl[mode][i])
                         {
                             maxHtIdx = i + 1;
                             break;
                         }
                    }
                }

                for (i = 0; i < MCSLeng; i++)
                {
                    for (j = 0; j < maxHtIdx; j++)
                    {
                        if (supported_mcs_rate[j].beacon_rate_index == MCSRates[i])
                        {
                            currentRate = supported_mcs_rate[j].supported_rate[rateFlag];
                            break;
                        }
                    }

                    if ((j < MAX_HT_MCS_IDX) && (currentRate > maxRate))
                    {
                        maxRate     = currentRate;
                        maxSpeedMCS = 1;
                        maxMCSIdx   = supported_mcs_rate[j].beacon_rate_index;
                    }
                }
            }
        }

        else if (!(rate_flags & eHAL_TX_RATE_LEGACY))
        {
            maxRate = myRate;
            maxSpeedMCS = 1;
            maxMCSIdx = pAdapter->hdd_stats.ClassA_stat.mcs_index;
        }

        // make sure we report a value at least as big as our current rate
        if ((maxRate < myRate) || (0 == maxRate))
        {
           maxRate = myRate;
           if (rate_flags & eHAL_TX_RATE_LEGACY)
           {
              maxSpeedMCS = 0;
           }
           else
           {
              maxSpeedMCS = 1;
              maxMCSIdx = pAdapter->hdd_stats.ClassA_stat.mcs_index;
           }
        }

        if (rate_flags & eHAL_TX_RATE_LEGACY)
        {
            sinfo->txrate.legacy  = maxRate;
#ifdef LINKSPEED_DEBUG_ENABLED
            pr_info("Reporting legacy rate %d\n", sinfo->txrate.legacy);
#endif //LINKSPEED_DEBUG_ENABLED
        }
        else
        {
            sinfo->txrate.mcs    = maxMCSIdx;
#ifdef WLAN_FEATURE_11AC
            sinfo->txrate.nss = nss;
            if (rate_flags & eHAL_TX_RATE_VHT80)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
                sinfo->txrate.flags |= RATE_INFO_FLAGS_80_MHZ_WIDTH;
            }
            else if (rate_flags & eHAL_TX_RATE_VHT40)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
                sinfo->txrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
            }
            else if (rate_flags & eHAL_TX_RATE_VHT20)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
            }
            else
                sinfo->txrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
#endif /* WLAN_FEATURE_11AC */
            if (rate_flags & (eHAL_TX_RATE_HT20 | eHAL_TX_RATE_HT40))
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_MCS;
                if (rate_flags & eHAL_TX_RATE_HT40)
                {
                    sinfo->txrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
                }
            }
            if (rate_flags & eHAL_TX_RATE_SGI)
            {
                if (!(sinfo->txrate.flags & RATE_INFO_FLAGS_VHT_MCS))
                    sinfo->txrate.flags |= RATE_INFO_FLAGS_MCS;
                sinfo->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
            }

#ifdef LINKSPEED_DEBUG_ENABLED
            pr_info("Reporting MCS rate %d flags %x\n",
                    sinfo->txrate.mcs,
                    sinfo->txrate.flags );
#endif //LINKSPEED_DEBUG_ENABLED
        }
    }
    else
    {
        // report current rate instead of max rate

        if (rate_flags & eHAL_TX_RATE_LEGACY)
        {
            //provide to the UI in units of 100kbps
            sinfo->txrate.legacy = myRate;
#ifdef LINKSPEED_DEBUG_ENABLED
            pr_info("Reporting actual legacy rate %d\n", sinfo->txrate.legacy);
#endif //LINKSPEED_DEBUG_ENABLED
        }
        else
        {
            //must be MCS
            sinfo->txrate.mcs = pAdapter->hdd_stats.ClassA_stat.mcs_index;
#ifdef WLAN_FEATURE_11AC
            sinfo->txrate.nss = nss;
            sinfo->txrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
            if (rate_flags & eHAL_TX_RATE_VHT80)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_80_MHZ_WIDTH;
            }
            else if (rate_flags & eHAL_TX_RATE_VHT40)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
            }
#endif /* WLAN_FEATURE_11AC */
            if (rate_flags & (eHAL_TX_RATE_HT20 | eHAL_TX_RATE_HT40))
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_MCS;
                if (rate_flags & eHAL_TX_RATE_HT40)
                {
                    sinfo->txrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
                }
            }
            if (rate_flags & eHAL_TX_RATE_SGI)
            {
                sinfo->txrate.flags |= RATE_INFO_FLAGS_MCS;
                sinfo->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
            }
#ifdef LINKSPEED_DEBUG_ENABLED
            pr_info("Reporting actual MCS rate %d flags %x\n",
                    sinfo->txrate.mcs,
                    sinfo->txrate.flags );
#endif //LINKSPEED_DEBUG_ENABLED
        }
    }
    sinfo->filled |= STATION_INFO_TX_BITRATE;

    sinfo->tx_bytes = pAdapter->stats.tx_bytes;
    sinfo->filled |= STATION_INFO_TX_BYTES;

    sinfo->tx_packets =
       pAdapter->hdd_stats.summary_stat.tx_frm_cnt[0] +
       pAdapter->hdd_stats.summary_stat.tx_frm_cnt[1] +
       pAdapter->hdd_stats.summary_stat.tx_frm_cnt[2] +
       pAdapter->hdd_stats.summary_stat.tx_frm_cnt[3];

    sinfo->tx_retries =
       pAdapter->hdd_stats.summary_stat.retry_cnt[0] +
       pAdapter->hdd_stats.summary_stat.retry_cnt[1] +
       pAdapter->hdd_stats.summary_stat.retry_cnt[2] +
       pAdapter->hdd_stats.summary_stat.retry_cnt[3];

    sinfo->tx_failed =
       pAdapter->hdd_stats.summary_stat.fail_cnt[0] +
       pAdapter->hdd_stats.summary_stat.fail_cnt[1] +
       pAdapter->hdd_stats.summary_stat.fail_cnt[2] +
       pAdapter->hdd_stats.summary_stat.fail_cnt[3];

    sinfo->filled |=
       STATION_INFO_TX_PACKETS |
       STATION_INFO_TX_RETRIES |
       STATION_INFO_TX_FAILED;

    sinfo->rx_bytes = pAdapter->stats.rx_bytes;
    sinfo->filled |= STATION_INFO_RX_BYTES;

    sinfo->rx_packets = pAdapter->stats.rx_packets;
    sinfo->filled |= STATION_INFO_RX_PACKETS;

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_GET_STA,
                     pAdapter->sessionId, maxRate));
       EXIT();
       return 0;
}

static int wlan_hdd_cfg80211_get_station(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         u8* mac, struct station_info *sinfo)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_get_station(wiphy, dev, mac, sinfo);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int __wlan_hdd_cfg80211_set_power_mgmt(struct wiphy *wiphy,
                     struct net_device *dev, bool mode, int timeout)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    VOS_STATUS vos_status;
    int status;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_POWER_MGMT,
                     pAdapter->sessionId, timeout));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    if ((DRIVER_POWER_MODE_AUTO == !mode) &&
        (TRUE == pHddCtx->hdd_wlan_suspended) &&
        (pHddCtx->cfg_ini->fhostArpOffload) &&
        (eConnectionState_Associated ==
             (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState))
    {

        hddLog(VOS_TRACE_LEVEL_INFO,
               "offload: in cfg80211_set_power_mgmt, calling arp offload");
        vos_status = hdd_conf_arp_offload(pAdapter, TRUE);
        if (!VOS_IS_STATUS_SUCCESS(vos_status))
        {
            hddLog(VOS_TRACE_LEVEL_INFO,
                   "%s:Failed to enable ARPOFFLOAD Feature %d",
                   __func__, vos_status);
        }
    }

    /**The get power cmd from the supplicant gets updated by the nl only
     *on successful execution of the function call
     *we are oppositely mapped w.r.t mode in the driver
     **/
    if(!pHddCtx->cfg_ini->enablePowersaveOffload)
        vos_status =  wlan_hdd_enter_bmps(pAdapter, !mode);
    else
        vos_status =  wlan_hdd_set_powersave(pAdapter, !mode);

    if (!mode)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_DEBUG,
                  "%s: DHCP start indicated through power save", __func__);
        sme_DHCPStartInd(pHddCtx->hHal, pAdapter->device_mode,
                         pAdapter->macAddressCurrent.bytes, pAdapter->sessionId);
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_DEBUG,
                  "%s: DHCP stop indicated through power save", __func__);
        sme_DHCPStopInd(pHddCtx->hHal, pAdapter->device_mode,
                        pAdapter->macAddressCurrent.bytes, pAdapter->sessionId);
    }

    EXIT();
    if (VOS_STATUS_E_FAILURE == vos_status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to enter bmps mode", __func__);
        return -EINVAL;
    }
    return 0;
}


static int wlan_hdd_cfg80211_set_power_mgmt(struct wiphy *wiphy,
                     struct net_device *dev, bool mode, int timeout)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_set_power_mgmt(wiphy, dev, mode, timeout);
    vos_ssr_unprotect(__func__);

    return ret;
}


static int wlan_hdd_set_default_mgmt_key(struct wiphy *wiphy,
                         struct net_device *netdev,
                         u8 key_index)
{
    ENTER();
    return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,4,0))
static int wlan_hdd_set_txq_params(struct wiphy *wiphy,
                   struct net_device *dev,
                   struct ieee80211_txq_params *params)
{
    ENTER();
    return 0;
}
#else
static int wlan_hdd_set_txq_params(struct wiphy *wiphy,
                   struct ieee80211_txq_params *params)
{
    ENTER();
    return 0;
}
#endif //LINUX_VERSION_CODE

static int __wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
                                      struct net_device *dev,
                                      struct tagCsrDelStaParams *pDelStaParams)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    VOS_STATUS vos_status;
    int status;
    v_U8_t staId;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_DEL_STA,
                     pAdapter->sessionId, pAdapter->device_mode));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
        return status;
    }

    if ((WLAN_HDD_SOFTAP == pAdapter->device_mode) ||
        (WLAN_HDD_P2P_GO == pAdapter->device_mode)) {
        if (vos_is_macaddr_broadcast((v_MACADDR_t *)pDelStaParams->peerMacAddr))
        {
            v_U16_t i;
            for (i = 0; i < WLAN_MAX_STA_COUNT; i++) {
                if ((pAdapter->aStaInfo[i].isUsed) &&
                    (!pAdapter->aStaInfo[i].isDeauthInProgress)) {
                    vos_mem_copy(pDelStaParams->peerMacAddr,
                                 pAdapter->aStaInfo[i].macAddrSTA.bytes,
                                 ETHER_ADDR_LEN);

#ifdef IPA_UC_OFFLOAD
                    if (pHddCtx->cfg_ini->IpaUcOffloadEnabled) {
                       hdd_ipa_wlan_evt(pAdapter, pAdapter->aStaInfo[i].ucSTAId,
                          WLAN_CLIENT_DISCONNECT, pDelStaParams->peerMacAddr);
                    }
#endif /* IPA_UC_OFFLOAD */
                    hddLog(VOS_TRACE_LEVEL_INFO,
                           FL("Delete STA with MAC::"MAC_ADDRESS_STR),
                           MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));

                    /* Send disassoc and deauth both to avoid some IOT issues */
                    hdd_softap_sta_disassoc(pAdapter, pDelStaParams);

                    vos_status = hdd_softap_sta_deauth(pAdapter, pDelStaParams);
                    if (VOS_IS_STATUS_SUCCESS(vos_status))
                        pAdapter->aStaInfo[i].isDeauthInProgress = TRUE;
                }
            }
        } else {
            vos_status = hdd_softap_GetStaId(pAdapter,
                            (v_MACADDR_t *)pDelStaParams->peerMacAddr, &staId);
            if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
                hddLog(VOS_TRACE_LEVEL_INFO,
                       FL("Skip DEL STA as this is not used::"MAC_ADDRESS_STR),
                       MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));
                return -ENOENT;
            }

#ifdef IPA_UC_OFFLOAD
            if (pHddCtx->cfg_ini->IpaUcOffloadEnabled) {
               hdd_ipa_wlan_evt(pAdapter, staId,
                  WLAN_CLIENT_DISCONNECT, pDelStaParams->peerMacAddr);
            }
#endif /* IPA_UC_OFFLOAD */

            if (pAdapter->aStaInfo[staId].isDeauthInProgress == TRUE) {
                hddLog(VOS_TRACE_LEVEL_INFO,
                       FL("Skip DEL STA as deauth is in progress::"
                          MAC_ADDRESS_STR),
                          MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));
                return -ENOENT;
            }

            pAdapter->aStaInfo[staId].isDeauthInProgress = TRUE;

            hddLog(VOS_TRACE_LEVEL_INFO,
                   FL("Delete STA with MAC::"MAC_ADDRESS_STR),
                   MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));

            /* Send disassoc and deauth both to avoid some IOT issues */
            hdd_softap_sta_disassoc(pAdapter, pDelStaParams);
            vos_status = hdd_softap_sta_deauth(pAdapter, pDelStaParams);
            if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
                pAdapter->aStaInfo[staId].isDeauthInProgress = FALSE;
                hddLog(VOS_TRACE_LEVEL_INFO,
                       FL("STA removal failed for ::"MAC_ADDRESS_STR),
                       MAC_ADDR_ARRAY(pDelStaParams->peerMacAddr));
                return -ENOENT;
            }
        }
    }

    EXIT();
    return 0;
}

#ifdef CFG80211_DEL_STA_V2
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         struct station_del_parameters *param)
#else
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
                                  struct net_device *dev, u8 *mac)
#endif
{
    int ret;
    struct tagCsrDelStaParams delStaParams;

    vos_ssr_protect(__func__);
#ifdef CFG80211_DEL_STA_V2
    if (NULL == param) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Invalid argumet passed", __func__);
        return -EINVAL;
    }

    WLANSAP_PopulateDelStaParams(param->mac, param->reason_code,
                                 param->subtype, &delStaParams);

#else
    WLANSAP_PopulateDelStaParams(mac, eSIR_MAC_DEAUTH_LEAVING_BSS_REASON,
                                 (SIR_MAC_MGMT_DEAUTH >> 4), &delStaParams);
#endif
    ret = __wlan_hdd_cfg80211_del_station(wiphy, dev, &delStaParams);
    vos_ssr_unprotect(__func__);

    return ret;
}

static int __wlan_hdd_cfg80211_add_station(struct wiphy *wiphy,
          struct net_device *dev, u8 *mac, struct station_parameters *params)
{
    int status = -EPERM;
#ifdef FEATURE_WLAN_TDLS
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    u32 mask, set;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_ADD_STA,
                     pAdapter->sessionId, params->listen_interval));

    if (0 != wlan_hdd_validate_context(pHddCtx))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: HDD context is not valid", __func__);
       return -EINVAL;
    }

    mask = params->sta_flags_mask;

    set = params->sta_flags_set;

#ifdef WLAN_FEATURE_TDLS_DEBUG
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: mask 0x%x set 0x%x " MAC_ADDRESS_STR,
               __func__, mask, set, MAC_ADDR_ARRAY(mac));
#endif

    if (mask & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
        if (set & BIT(NL80211_STA_FLAG_TDLS_PEER)) {
            status = wlan_hdd_tdls_add_station(wiphy, dev, mac, 0, NULL);
        }
    }
#endif
    return status;
}

static int wlan_hdd_cfg80211_add_station(struct wiphy *wiphy,
          struct net_device *dev, u8 *mac, struct station_parameters *params)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_add_station(wiphy, dev, mac, params);
    vos_ssr_unprotect(__func__);

    return ret;
}

#ifdef FEATURE_WLAN_LFR
static int __wlan_hdd_cfg80211_set_pmksa(struct wiphy *wiphy, struct net_device *dev,
            struct cfg80211_pmksa *pmksa)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tHalHandle halHandle;
    eHalStatus result = eHAL_STATUS_SUCCESS;
    int status;
    tPmkidCacheInfo pmk_id;

    if (!pmksa) {
        hddLog(LOGE, FL("pmksa is NULL"));
        return -EINVAL;
    }

    if (!pmksa->bssid || !pmksa->pmkid) {
        hddLog(LOGE, FL("pmksa->bssid(%p) or pmksa->pmkid(%p) is NULL"),
               pmksa->bssid, pmksa->pmkid);
        return -EINVAL;
    }

    hddLog(VOS_TRACE_LEVEL_DEBUG, FL("set PMKSA for "MAC_ADDRESS_STR),
           MAC_ADDR_ARRAY(pmksa->bssid));

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return status;
    }

    halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

    vos_mem_copy(pmk_id.BSSID, pmksa->bssid, ETHER_ADDR_LEN);
    vos_mem_copy(pmk_id.PMKID, pmksa->pmkid, CSR_RSN_PMKID_SIZE);

    /* Add to the PMKSA ID Cache in CSR */
    result = sme_RoamSetPMKIDCache(halHandle,pAdapter->sessionId,
                                   &pmk_id, 1, FALSE);

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_PMKSA,
                     pAdapter->sessionId, result));

    return HAL_STATUS_SUCCESS(result) ? 0 : -EINVAL;
}

static int wlan_hdd_cfg80211_set_pmksa(struct wiphy *wiphy, struct net_device *dev,
            struct cfg80211_pmksa *pmksa)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __wlan_hdd_cfg80211_set_pmksa(wiphy, dev, pmksa);
   vos_ssr_unprotect(__func__);

   return ret;
}


static int __wlan_hdd_cfg80211_del_pmksa(struct wiphy *wiphy, struct net_device *dev,
             struct cfg80211_pmksa *pmksa)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tHalHandle halHandle;
    int status = 0;

    if (!pmksa) {
        hddLog(LOGE, FL("pmksa is NULL"));
        return -EINVAL;
    }

    if (!pmksa->bssid) {
        hddLog(LOGE, FL("pmksa->bssid is NULL"));
        return -EINVAL;
    }

    hddLog(VOS_TRACE_LEVEL_DEBUG, FL("Deleting PMKSA for "MAC_ADDRESS_STR),
           MAC_ADDR_ARRAY(pmksa->bssid));

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return status;
    }

    halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

    /* Delete the PMKID CSR cache */
    if (eHAL_STATUS_SUCCESS !=
        sme_RoamDelPMKIDfromCache(halHandle,
                                  pAdapter->sessionId, pmksa->bssid, FALSE)) {
       hddLog(LOGE, FL("Failed to delete PMKSA for "MAC_ADDRESS_STR),
                      MAC_ADDR_ARRAY(pmksa->bssid));
       status = -EINVAL;
    }

    return status;
}


static int wlan_hdd_cfg80211_del_pmksa(struct wiphy *wiphy, struct net_device *dev,
             struct cfg80211_pmksa *pmksa)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_del_pmksa(wiphy, dev, pmksa);
    vos_ssr_unprotect(__func__);

    return ret;

}

static int __wlan_hdd_cfg80211_flush_pmksa(struct wiphy *wiphy, struct net_device *dev)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    tHalHandle halHandle;
    int status = 0;

    hddLog(VOS_TRACE_LEVEL_DEBUG, FL("Flushing PMKSA"));

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
       return status;
    }

    /* Retrieve halHandle */
    halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

    /* Flush the PMKID cache in CSR */
    if (eHAL_STATUS_SUCCESS !=
        sme_RoamDelPMKIDfromCache(halHandle, pAdapter->sessionId, NULL, TRUE)) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("Cannot flush PMKIDCache"));
       status = -EINVAL;
    }

    return status;
}

static int wlan_hdd_cfg80211_flush_pmksa(struct wiphy *wiphy,
                                         struct net_device *dev)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_flush_pmksa(wiphy, dev);
    vos_ssr_unprotect(__func__);

    return ret;
}
#endif

#if defined(WLAN_FEATURE_VOWIFI_11R) && defined(KERNEL_SUPPORT_11R_CFG80211)
static int
__wlan_hdd_cfg80211_update_ft_ies(struct wiphy *wiphy,
                                  struct net_device *dev,
                                  struct cfg80211_update_ft_ies_params *ftie)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_station_ctx_t *pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_UPDATE_FT_IES,
                     pAdapter->sessionId, pHddStaCtx->conn_info.connState));
    // Added for debug on reception of Re-assoc Req.
    if (eConnectionState_Associated != pHddStaCtx->conn_info.connState)
    {
        hddLog(LOGE, FL("Called with Ie of length = %zu when not associated"),
               ftie->ie_len);
        hddLog(LOGE, FL("Should be Re-assoc Req IEs"));
    }

#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
    hddLog(LOG1, FL("%s called with Ie of length = %zu"), __func__,
           ftie->ie_len);
#endif

    // Pass the received FT IEs to SME
    sme_SetFTIEs( WLAN_HDD_GET_HAL_CTX(pAdapter), pAdapter->sessionId,
                  (const u8 *)ftie->ie,
                  ftie->ie_len);
    return 0;
}

static int
wlan_hdd_cfg80211_update_ft_ies(struct wiphy *wiphy,
                                struct net_device *dev,
                                struct cfg80211_update_ft_ies_params *ftie)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_update_ft_ies(wiphy, dev, ftie);
    vos_ssr_unprotect(__func__);

    return ret;
}
#endif

#ifdef FEATURE_WLAN_SCAN_PNO
int wlan_hdd_scan_abort(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    hdd_scaninfo_t *pScanInfo = NULL;
    unsigned long rc;

    pScanInfo = &pAdapter->scan_info;

    if (pScanInfo->mScanPending && pAdapter->request)
    {
        INIT_COMPLETION(pScanInfo->abortscan_event_var);
        hdd_abort_mac_scan(pHddCtx, pAdapter->sessionId,
                           eCSR_SCAN_ABORT_DEFAULT);

        rc = wait_for_completion_timeout(
                           &pScanInfo->abortscan_event_var,
                           msecs_to_jiffies(5000));
        if (!rc) {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Timeout occurred while waiting for abort scan" ,
                  __func__);
            return -ETIME;
        }
    }
    return 0;
}
void hdd_cfg80211_sched_scan_done_callback(void *callbackContext,
                              tSirPrefNetworkFoundInd *pPrefNetworkFoundInd)
{
    int ret;
    hdd_adapter_t* pAdapter = (hdd_adapter_t*)callbackContext;
    hdd_context_t *pHddCtx;

    ENTER();

    if (NULL == pAdapter)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD adapter is Null", __func__);
        return ;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    if (NULL == pHddCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HDD context is Null!!!", __func__);
        return ;
    }

    spin_lock(&pHddCtx->schedScan_lock);
    if (TRUE == pHddCtx->isWiphySuspended)
    {
        pHddCtx->isSchedScanUpdatePending = TRUE;
        spin_unlock(&pHddCtx->schedScan_lock);
        hddLog(VOS_TRACE_LEVEL_INFO,
               "%s: Update cfg80211 scan database after it resume", __func__);
        return ;
    }
    spin_unlock(&pHddCtx->schedScan_lock);

    ret = wlan_hdd_cfg80211_update_bss(pHddCtx->wiphy, pAdapter);

    if (0 > ret)
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: NO SCAN result", __func__);

    cfg80211_sched_scan_results(pHddCtx->wiphy);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
            "%s: cfg80211 scan result database updated", __func__);
}

/**
 * wlan_hdd_is_pno_allowed() -  Check if PNO is allowed
 * @adapter: HDD Device Adapter
 *
 * The PNO Start request is coming from upper layers.
 * It is to be allowed only for Infra STA device type
 * and the link should be in a disconnected state.
 *
 * Return: Success if PNO is allowed, Failure otherwise.
 */
static eHalStatus wlan_hdd_is_pno_allowed(hdd_adapter_t *adapter)
{
	hddLog(LOG1,
		FL("dev_mode=%d, conn_state=%d, session ID=%d"),
		adapter->device_mode,
		adapter->sessionCtx.station.conn_info.connState,
		adapter->sessionId);
	if ((adapter->device_mode == WLAN_HDD_INFRA_STATION) &&
		(eConnectionState_NotConnected ==
			 adapter->sessionCtx.station.conn_info.connState))
		return eHAL_STATUS_SUCCESS;
	else
		return eHAL_STATUS_FAILURE;

}

/*
 * FUNCTION: __wlan_hdd_cfg80211_sched_scan_start
 * Function to enable PNO
 */
static int __wlan_hdd_cfg80211_sched_scan_start(struct wiphy *wiphy,
          struct net_device *dev, struct cfg80211_sched_scan_request *request)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    tpSirPNOScanReq pPnoRequest = NULL;
    hdd_context_t *pHddCtx;
    tHalHandle hHal;
    v_U32_t i, indx, num_ch, j;
    u8 valid_ch[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
    u8 channels_allowed[WNI_CFG_VALID_CHANNEL_LIST_LEN] = {0};
    v_U32_t num_channels_allowed = WNI_CFG_VALID_CHANNEL_LIST_LEN;
    eHalStatus status = eHAL_STATUS_FAILURE;
    int ret = 0;
    hdd_scaninfo_t *pScanInfo = &pAdapter->scan_info;

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    ret = wlan_hdd_validate_context(pHddCtx);

    if (0 != ret)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return ret;
    }

    hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    if (NULL == hHal)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HAL context  is Null!!!", __func__);
        return -EINVAL;
    }

    if ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) &&
        (eConnectionState_Connecting ==
           (WLAN_HDD_GET_STATION_CTX_PTR(pAdapter))->conn_info.connState))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR,
                "%s: %p(%d) Connection in progress: sched_scan_start denied (EBUSY)", __func__, \
                WLAN_HDD_GET_STATION_CTX_PTR(pAdapter), pAdapter->sessionId);
        return -EBUSY;
    }

    /*
     * The current umac is unable to handle the SCAN_PREEMPT and SCAN_DEQUEUED
     * so its necessary to terminate the existing scan which is already issued
     * otherwise the host won't enter into the suspend state due to the reason
     * that the wlan wakelock which was held in the wlan_hdd_cfg80211_scan
     * function.
     */
    if (TRUE == pScanInfo->mScanPending)
    {
        ret = wlan_hdd_scan_abort(pAdapter);
        if(ret < 0){
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: aborting the existing scan is unsuccessful", __func__);
            return -EBUSY;
        }
    }

    if (eHAL_STATUS_FAILURE == wlan_hdd_is_pno_allowed(pAdapter))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  "%s: pno is not allowed", __func__);
        return -ENOTSUPP;
    }

    pPnoRequest = (tpSirPNOScanReq) vos_mem_malloc(sizeof (tSirPNOScanReq));
    if (NULL == pPnoRequest)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                  "%s: vos_mem_malloc failed", __func__);
        return -ENOMEM;
    }

    memset(pPnoRequest, 0, sizeof (tSirPNOScanReq));
    pPnoRequest->enable = 1; /*Enable PNO */
    pPnoRequest->ucNetworksCount = request->n_match_sets;

    if (( !pPnoRequest->ucNetworksCount ) ||
        ( pPnoRequest->ucNetworksCount > SIR_PNO_MAX_SUPP_NETWORKS ))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: Network input is not correct %d",
                 __func__, pPnoRequest->ucNetworksCount);
        ret = -EINVAL;
        goto error;
    }

    if ( SIR_PNO_MAX_NETW_CHANNELS_EX < request->n_channels )
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Incorrect number of channels %d",
                  __func__, request->n_channels);
        ret = -EINVAL;
        goto error;
    }

    /* Framework provides one set of channels(all)
     * common for all saved profile */
    if (0 != ccmCfgGetStr(hHal, WNI_CFG_VALID_CHANNEL_LIST,
            channels_allowed, &num_channels_allowed))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: failed to get valid channel list", __func__);
        ret = -EINVAL;
        goto error;
    }
    /* Checking each channel against allowed channel list */
    num_ch = 0;
    if (request->n_channels)
    {
        char chList [(request->n_channels*5)+1];
        int len;
        for (i = 0, len = 0; i < request->n_channels; i++)
        {
            for (indx = 0; indx < num_channels_allowed; indx++)
            {
                if (request->channels[i]->hw_value == channels_allowed[indx])
                {
                   valid_ch[num_ch++] = request->channels[i]->hw_value;
                   len += snprintf(chList+len, 5, "%d ",
                                  request->channels[i]->hw_value);
                   break ;
                }
             }
         }
         hddLog(VOS_TRACE_LEVEL_INFO,"Channel-List: %s ", chList);
     }
    /* Filling per profile  params */
    for (i = 0; i < pPnoRequest->ucNetworksCount; i++)
    {
        pPnoRequest->aNetworks[i].ssId.length =
               request->match_sets[i].ssid.ssid_len;

        if (( 0 == pPnoRequest->aNetworks[i].ssId.length ) ||
            ( pPnoRequest->aNetworks[i].ssId.length > 32 ) )
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: SSID Len %d is not correct for network %d",
                      __func__, pPnoRequest->aNetworks[i].ssId.length, i);
            ret = -EINVAL;
            goto error;
        }

        memcpy(pPnoRequest->aNetworks[i].ssId.ssId,
               request->match_sets[i].ssid.ssid,
               request->match_sets[i].ssid.ssid_len);
        pPnoRequest->aNetworks[i].authentication = 0; /*eAUTH_TYPE_ANY*/
        pPnoRequest->aNetworks[i].encryption     = 0; /*eED_ANY*/
        pPnoRequest->aNetworks[i].bcastNetwType  = 0; /*eBCAST_UNKNOWN*/

        /*Copying list of valid channel into request */
        memcpy(pPnoRequest->aNetworks[i].aChannels, valid_ch, num_ch);
        pPnoRequest->aNetworks[i].ucChannelCount = num_ch;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
        pPnoRequest->aNetworks[i].rssiThreshold =
                                    request->match_sets[i].rssi_thold;
#else
        pPnoRequest->aNetworks[i].rssiThreshold = 0; //Default value
#endif
    }

    for (i = 0; i < request->n_ssids; i++) {
        j = 0;
        while (j < pPnoRequest->ucNetworksCount) {
            if ((pPnoRequest->aNetworks[j].ssId.length ==
                 request->ssids[i].ssid_len) &&
                (0 == memcmp(pPnoRequest->aNetworks[j].ssId.ssId,
                            request->ssids[i].ssid,
                            pPnoRequest->aNetworks[j].ssId.length))) {
                pPnoRequest->aNetworks[j].bcastNetwType = eBCAST_HIDDEN;
                break;
            }
            j++;
        }
    }
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "Number of hidden networks being Configured = %d",
              request->n_ssids);

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "request->ie_len = %zu", request->ie_len);
    if ((0 < request->ie_len) && (NULL != request->ie))
    {
        pPnoRequest->us24GProbeTemplateLen = request->ie_len;
        memcpy(&pPnoRequest->p24GProbeTemplate, request->ie,
                pPnoRequest->us24GProbeTemplateLen);

        pPnoRequest->us5GProbeTemplateLen = request->ie_len;
        memcpy(&pPnoRequest->p5GProbeTemplate, request->ie,
                pPnoRequest->us5GProbeTemplateLen);
    }

    /*
     * Driver gets only one time interval which is hard coded in
     * supplicant for 10000ms. Taking power consumption into account
     * firmware after gPNOScanTimerRepeatValue times fast_scan_period switches
     * slow_scan_period. This is less frequent scans and firmware shall be
     * in slow_scan_period mode until next PNO Start.
     */
    pPnoRequest->fast_scan_period = request->interval;
    pPnoRequest->fast_scan_max_cycles =
                                 pHddCtx->cfg_ini->configPNOScanTimerRepeatValue;
    pPnoRequest->slow_scan_period = pHddCtx->cfg_ini->pno_slow_scan_multiplier *
                                        pPnoRequest->fast_scan_period;

    hddLog(LOG1, "Base scan interval: %d sec PNOScanTimerRepeatValue: %d",
           (request->interval / 1000),
            pHddCtx->cfg_ini->configPNOScanTimerRepeatValue);

    pPnoRequest->modePNO = SIR_PNO_MODE_IMMEDIATE;

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "SessionId %d, enable %d, modePNO %d",
             pAdapter->sessionId, pPnoRequest->enable, pPnoRequest->modePNO);

    status = sme_SetPreferredNetworkList(WLAN_HDD_GET_HAL_CTX(pAdapter),
                              pPnoRequest, pAdapter->sessionId,
                              hdd_cfg80211_sched_scan_done_callback, pAdapter);
    if (eHAL_STATUS_SUCCESS != status)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Failed to enable PNO", __func__);
        ret = -EINVAL;
        goto error;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                  "PNO scanRequest offloaded");

error:
    vos_mem_free(pPnoRequest);
    return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_sched_scan_start
 * NL interface to enable PNO
 */
static int wlan_hdd_cfg80211_sched_scan_start(struct wiphy *wiphy,
          struct net_device *dev, struct cfg80211_sched_scan_request *request)
{
     int ret;

     vos_ssr_protect(__func__);
     ret = __wlan_hdd_cfg80211_sched_scan_start(wiphy, dev, request);
     vos_ssr_unprotect(__func__);

     return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_sched_scan_stop
 * Function to disable PNO
 */
static int __wlan_hdd_cfg80211_sched_scan_stop(struct wiphy *wiphy,
          struct net_device *dev)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    tHalHandle hHal;
    tpSirPNOScanReq pPnoRequest = NULL;
    int ret = 0;

    ENTER();

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    if (NULL == pHddCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HDD context is Null", __func__);
        return -ENODEV;
    }

    /* The return 0 is intentional when isLogpInProgress and
     * isLoadUnloadInProgress. We did observe a crash due to a return of
     * failure in sched_scan_stop , especially for a case where the unload
     * of the happens at the same time. The function __cfg80211_stop_sched_scan
     * was clearing rdev->sched_scan_req only when the sched_scan_stop returns
     * success. If it returns a failure , then its next invocation due to the
     * clean up of the second interface will have the dev pointer corresponding
     * to the first one leading to a crash.
     */
    if (pHddCtx->isLogpInProgress)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: LOGP in Progress. Ignore!!!", __func__);
        return ret;
    }

    if ((pHddCtx->isLoadInProgress) ||
        (pHddCtx->isUnloadInProgress))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Unloading/Loading in Progress. Ignore!!!", __func__);
        return ret;
    }

    hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    if (NULL == hHal)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: HAL context  is Null!!!", __func__);
        return -EINVAL;
    }

    pPnoRequest = (tpSirPNOScanReq) vos_mem_malloc(sizeof (tSirPNOScanReq));
    if (NULL == pPnoRequest)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: vos_mem_malloc failed", __func__);
        return -ENOMEM;
    }

    memset(pPnoRequest, 0, sizeof (tSirPNOScanReq));
    pPnoRequest->enable = 0; /* Disable PNO */
    pPnoRequest->ucNetworksCount = 0;


    status = sme_SetPreferredNetworkList(hHal, pPnoRequest,
                                pAdapter->sessionId,
                                NULL, pAdapter);
    if (eHAL_STATUS_SUCCESS != status)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "Failed to disabled PNO");
        ret = -EINVAL;
    }

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "%s: PNO scan disabled", __func__);

    vos_mem_free(pPnoRequest);

    EXIT();
    return ret;
}

/*
 * FUNCTION: wlan_hdd_cfg80211_sched_scan_stop
 * NL interface to disable PNO
 */
static int wlan_hdd_cfg80211_sched_scan_stop(struct wiphy *wiphy,
          struct net_device *dev)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_sched_scan_stop(wiphy, dev);
    vos_ssr_unprotect(__func__);

    return ret;
}
#endif /*FEATURE_WLAN_SCAN_PNO*/


#ifdef FEATURE_WLAN_TDLS
#if TDLS_MGMT_VERSION2
static int wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
                     u8 *peer, u8 action_code,  u8 dialog_token,
                      u16 status_code, u32 peer_capability, const u8 *buf, size_t len)
#else
static int wlan_hdd_cfg80211_tdls_mgmt(struct wiphy *wiphy, struct net_device *dev,
                     u8 *peer, u8 action_code,  u8 dialog_token,
                     u16 status_code, const u8 *buf, size_t len)
#endif
{

    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    u8 peerMac[VOS_MAC_ADDR_SIZE];
    VOS_STATUS status;
    int max_sta_failed = 0;
    int responder;
    unsigned long rc;
    tANI_U16 numCurrTdlsPeers;
#if !(TDLS_MGMT_VERSION2)
    u32 peer_capability;
    peer_capability = 0;
#endif

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_TDLS_MGMT,
                     pAdapter->sessionId, action_code));

    if (0 != wlan_hdd_validate_context(pHddCtx))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: HDD context is not valid", __func__);
        return -EINVAL;
    }

    if (eTDLS_SUPPORT_NOT_ENABLED == pHddCtx->tdls_mode)
    {
         VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                    "%s: TDLS mode is disabled OR not enabled in FW."
                    MAC_ADDRESS_STR " action %d declined.",
                    __func__, MAC_ADDR_ARRAY(peer), action_code);
        return -ENOTSUPP;
    }
    /* If any concurrency is detected */
    if (((1 << VOS_STA_MODE) != pHddCtx->concurrency_mode) ||
        (pHddCtx->no_of_active_sessions[VOS_STA_MODE] > 1)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH,
                  FL("Multiple STA OR Concurrency detected. Ignore TDLS MGMT frame. action_code=%d, concurrency_mode: 0x%x, active_sessions: %d"),
                  action_code,
                  pHddCtx->concurrency_mode,
                  pHddCtx->no_of_active_sessions[VOS_STA_MODE]);
        return -EPERM;
    }
    /* other than teardown frame, mgmt frames are not sent if disabled */
    if (SIR_MAC_TDLS_TEARDOWN != action_code)
    {
       /* if tdls_mode is disabled to respond to peer's request */
        if (eTDLS_SUPPORT_DISABLED == pHddCtx->tdls_mode)
        {
             VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                        "%s: " MAC_ADDRESS_STR
                        " TDLS mode is disabled. action %d declined.",
                        __func__, MAC_ADDR_ARRAY(peer), action_code);

             return -ENOTSUPP;
        }
    }
    if (WLAN_IS_TDLS_SETUP_ACTION(action_code))
    {
        if (NULL != wlan_hdd_tdls_is_progress(pHddCtx, peer, TRUE))
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: " MAC_ADDRESS_STR
                       " TDLS setup is ongoing. action %d declined.",
                       __func__, MAC_ADDR_ARRAY(peer), action_code);
            return -EPERM;
        }
    }

    if (SIR_MAC_TDLS_SETUP_REQ == action_code ||
        SIR_MAC_TDLS_SETUP_RSP == action_code )
    {
        numCurrTdlsPeers = wlan_hdd_tdlsConnectedPeers(pAdapter);
        if (pHddCtx->max_num_tdls_sta <= numCurrTdlsPeers)
        {
            /* supplicant still sends tdls_mgmt(SETUP_REQ) even after
               we return error code at 'add_station()'. Hence we have this
               check again in addition to add_station().
               Anyway, there is no hard to double-check. */
            if (SIR_MAC_TDLS_SETUP_REQ == action_code)
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           "%s: " MAC_ADDRESS_STR
                           " TDLS Max peer already connected. action (%d) declined. Num of peers (%d), Max allowed (%d).",
                           __func__, MAC_ADDR_ARRAY(peer), action_code,
                           numCurrTdlsPeers, pHddCtx->max_num_tdls_sta);
                return -EINVAL;
            }
            else
            {
                /* maximum reached. tweak to send error code to peer and return
                   error code to supplicant */
                status_code = eSIR_MAC_UNSPEC_FAILURE_STATUS;
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           "%s: " MAC_ADDRESS_STR
                           " TDLS Max peer already connected, send response status (%d). Num of peers (%d), Max allowed (%d).",
                           __func__, MAC_ADDR_ARRAY(peer), status_code,
                           numCurrTdlsPeers, pHddCtx->max_num_tdls_sta);
                max_sta_failed = -EPERM;
                /* fall through to send setup resp with failure status
                code */
            }
        }
        else
        {
            hddTdlsPeer_t *pTdlsPeer;
            pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, peer, TRUE);
            if (pTdlsPeer && TDLS_IS_CONNECTED(pTdlsPeer))
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                        "%s:" MAC_ADDRESS_STR " already connected. action %d declined.",
                        __func__, MAC_ADDR_ARRAY(peer), action_code);
                return -EPERM;
            }
        }
    }
    vos_mem_copy(peerMac, peer, 6);

#ifdef WLAN_FEATURE_TDLS_DEBUG
    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "%s: " MAC_ADDRESS_STR " action %d, dialog_token %d status %d, len = %zu",
               "tdls_mgmt", MAC_ADDR_ARRAY(peer),
               action_code, dialog_token, status_code, len);
#endif

    /*Except teardown responder will not be used so just make 0*/
    responder = 0;
    if (SIR_MAC_TDLS_TEARDOWN == action_code)
    {

       hddTdlsPeer_t *pTdlsPeer;
       pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, peerMac, TRUE);

       if(pTdlsPeer && TDLS_IS_CONNECTED(pTdlsPeer))
            responder = pTdlsPeer->is_responder;
       else
       {
           VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s: " MAC_ADDRESS_STR " peer doesn't exist or not connected %d dialog_token %d status %d, len = %zu",
                    __func__, MAC_ADDR_ARRAY(peer), (NULL == pTdlsPeer) ? -1 : pTdlsPeer->link_status,
                     dialog_token, status_code, len);
           return -EPERM;
       }
    }

    /* For explicit trigger of DIS_REQ come out of BMPS for
       successfully receiving DIS_RSP from peer. */
    if ((SIR_MAC_TDLS_SETUP_RSP == action_code) ||
        (SIR_MAC_TDLS_DIS_RSP == action_code) ||
        (SIR_MAC_TDLS_DIS_REQ == action_code))
    {
        /* Fw will take care if PS offload is enabled. */
        if (!pHddCtx->cfg_ini->enablePowersaveOffload)
        {
            if (TRUE == sme_IsPmcBmps(WLAN_HDD_GET_HAL_CTX(pAdapter)))
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                          "%s: Sending frame action_code %u.Disable BMPS",
                          __func__, action_code);
                hdd_disable_bmps_imps(pHddCtx, WLAN_HDD_INFRA_STATION);
            }
        }
        if (SIR_MAC_TDLS_DIS_REQ != action_code)
            wlan_hdd_tdls_set_cap(pAdapter, peerMac, eTDLS_CAP_SUPPORTED);
    }

    /* make sure doesn't call send_mgmt() while it is pending */
    if (TDLS_CTX_MAGIC == pAdapter->mgmtTxCompletionStatus)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "%s: " MAC_ADDRESS_STR " action %d couldn't sent, as one is pending. return EBUSY",
            __func__, MAC_ADDR_ARRAY(peer), action_code);
        return -EBUSY;
    }

    pAdapter->mgmtTxCompletionStatus = TDLS_CTX_MAGIC;
    INIT_COMPLETION(pAdapter->tdls_mgmt_comp);

    status = sme_SendTdlsMgmtFrame(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                   pAdapter->sessionId, peerMac, action_code,
                                   dialog_token, status_code, peer_capability,
                                   (tANI_U8 *)buf, len, !responder);

    if (VOS_STATUS_SUCCESS != status)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: sme_SendTdlsMgmtFrame failed!", __func__);
        pAdapter->mgmtTxCompletionStatus = FALSE;

        wlan_hdd_tdls_check_bmps(pAdapter);
        return -EINVAL;
    }

    rc = wait_for_completion_timeout(&pAdapter->tdls_mgmt_comp,
                                       msecs_to_jiffies(WAIT_TIME_TDLS_MGMT));

    if ((0 == rc) || (TRUE != pAdapter->mgmtTxCompletionStatus)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Mgmt Tx Completion timed out TxCompletion %u",
                  __func__, pAdapter->mgmtTxCompletionStatus);

        if (pHddCtx->isLogpInProgress)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: LOGP in Progress. Ignore!!!", __func__);
            return -EAGAIN;
        }

        if (pHddCtx->isUnloadInProgress)
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: Unloading/Loading in Progress. Ignore!!!", __func__);
            return -EAGAIN;
        }

        pAdapter->mgmtTxCompletionStatus = FALSE;
        wlan_hdd_tdls_check_bmps(pAdapter);
        return -EINVAL;
    }

    if (max_sta_failed)
    {
      wlan_hdd_tdls_check_bmps(pAdapter);
      return max_sta_failed;
    }

    if (SIR_MAC_TDLS_SETUP_RSP == action_code)
    {
        return wlan_hdd_tdls_set_responder(pAdapter, peerMac, FALSE);
    }
    else if (SIR_MAC_TDLS_SETUP_CNF == action_code)
    {
        return wlan_hdd_tdls_set_responder(pAdapter, peerMac, TRUE);
    }

    return 0;
}


int wlan_hdd_tdls_extctrl_config_peer(hdd_adapter_t *pAdapter,
                                      u8 *peer,
                                      cfg80211_exttdls_callback callback,
                                      u32 chan,
                                      u32 max_latency,
                                      u32 op_class,
                                      u32 min_bandwidth)
{
    hddTdlsPeer_t *pTdlsPeer;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s : NL80211_TDLS_SETUP for " MAC_ADDRESS_STR,
              __func__, MAC_ADDR_ARRAY(peer));

    if ( (FALSE == pHddCtx->cfg_ini->fTDLSExternalControl) ||
         (FALSE == pHddCtx->cfg_ini->fEnableTDLSImplicitTrigger) ) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s TDLS External control or Implicit Trigger not enabled ",
              __func__);
        return -ENOTSUPP;
    }

    /* To cater the requirement of establishing the TDLS link
     * irrespective of the data traffic , get an entry of TDLS peer.
     */
    pTdlsPeer = wlan_hdd_tdls_get_peer(pAdapter, peer);
    if (pTdlsPeer == NULL) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: peer " MAC_ADDRESS_STR " does not exist",
                  __func__, MAC_ADDR_ARRAY(peer));
        return -EINVAL;
    }

    if ( 0 != wlan_hdd_tdls_set_force_peer(pAdapter, peer, TRUE) ) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s TDLS Add Force Peer Failed",
              __func__);
        return -EINVAL;
    }

    if ( 0 != wlan_hdd_tdls_set_extctrl_param(pAdapter, peer,
                                              chan, max_latency,
                                              op_class, min_bandwidth) ) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s TDLS Set Peer's External Ctrl Parameter Failed",
              __func__);
        return -EINVAL;
    }

    if ( 0 != wlan_hdd_set_callback(pTdlsPeer, callback) ) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s TDLS set callback Failed",
              __func__);
        return -EINVAL;
    }

    return(0);
}

int wlan_hdd_tdls_extctrl_deconfig_peer(hdd_adapter_t *pAdapter, u8 *peer)
{
    hddTdlsPeer_t *pTdlsPeer;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s : NL80211_TDLS_TEARDOWN for " MAC_ADDRESS_STR,
              __func__, MAC_ADDR_ARRAY(peer));

    if ( (FALSE == pHddCtx->cfg_ini->fTDLSExternalControl) ||
         (FALSE == pHddCtx->cfg_ini->fEnableTDLSImplicitTrigger) ) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "%s TDLS External control or Implicit Trigger not enabled ",
              __func__);
        return -ENOTSUPP;
    }


    pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, peer, TRUE);

    if ( NULL == pTdlsPeer ) {
        hddLog(VOS_TRACE_LEVEL_INFO, "%s: " MAC_ADDRESS_STR
               "peer matching MAC_ADDRESS_STR not found",
               __func__, MAC_ADDR_ARRAY(peer));
        return -EINVAL;
    }
    else {
        wlan_hdd_tdls_indicate_teardown(pAdapter, pTdlsPeer,
                           eSIR_MAC_TDLS_TEARDOWN_UNSPEC_REASON);
    }

    if (0 != wlan_hdd_tdls_set_force_peer(pAdapter, peer, FALSE)) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s Failed",
              __func__);
        return -EINVAL;
    }
    /* EXT TDLS */
    if ( 0 != wlan_hdd_set_callback(pTdlsPeer, NULL )) {

        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s TDLS set callback Failed",
              __func__);
        return -EINVAL;
    }
    return(0);
}
static int __wlan_hdd_cfg80211_tdls_oper(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         u8 *peer,
                                         enum nl80211_tdls_operation oper)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    int status;
    tSmeTdlsPeerStateParams smeTdlsPeerStateParams;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    hddTdlsPeer_t *pTdlsPeer;

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_TDLS_OPER,
                     pAdapter->sessionId, oper));
    if ( NULL == peer )
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid arguments", __func__);
        return -EINVAL;
    }

    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }


    /* QCA 2.0 Discrete ANDs feature capability in cfg_ini with that
     * received from target, so cfg_ini gives combined intersected result
     */
    if (FALSE == pHddCtx->cfg_ini->fEnableTDLSSupport)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "TDLS Disabled in INI OR not enabled in FW. "
                "Cannot process TDLS commands");
        return -ENOTSUPP;
    }

    switch (oper) {
        case NL80211_TDLS_ENABLE_LINK:
            {
                VOS_STATUS status;
                unsigned long rc;
                tCsrTdlsLinkEstablishParams tdlsLinkEstablishParams;

                pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, peer, TRUE);

                if (NULL == pTdlsPeer)
                {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                               "%s: peer matching "MAC_ADDRESS_STR
                               " not found, ignore NL80211_TDLS_ENABLE_LINK",
                                __func__, MAC_ADDR_ARRAY(peer));
                    return -EINVAL;
                }

                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           "%s: NL80211_TDLS_ENABLE_LINK for peer "
                           MAC_ADDRESS_STR" link_status: %d",
                           __func__, MAC_ADDR_ARRAY(peer), pTdlsPeer->link_status);

                if (!TDLS_STA_INDEX_VALID(pTdlsPeer->staId))
                {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                               "%s: invalid sta index %u for "
                                MAC_ADDRESS_STR" TDLS_ENABLE_LINK failed",
                                __func__, pTdlsPeer->staId, MAC_ADDR_ARRAY(peer));
                    return -EINVAL;
                }

                if (eTDLS_LINK_CONNECTED != pTdlsPeer->link_status)
                {
                    if (IS_ADVANCE_TDLS_ENABLE) {

                        if (0 != wlan_hdd_tdls_get_link_establish_params(
                                   pAdapter, peer,&tdlsLinkEstablishParams)) {
                            return -EINVAL;
                        }
                        INIT_COMPLETION(pAdapter->tdls_link_establish_req_comp);

                        sme_SendTdlsLinkEstablishParams(WLAN_HDD_GET_HAL_CTX(pAdapter),
                            pAdapter->sessionId, peer, &tdlsLinkEstablishParams);
                        /* Send TDLS peer UAPSD capabilities to the firmware and
                         * register with the TL on after the response for this operation
                         * is received .
                         */
                        rc = wait_for_completion_timeout(
                                &pAdapter->tdls_link_establish_req_comp,
                                msecs_to_jiffies(WAIT_TIME_TDLS_LINK_ESTABLISH_REQ));
                        if (!rc) {
                            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                              "%s: Link Establish Request timed out", __func__);
                            return -EINVAL;
                        }
                    }
                    wlan_hdd_tdls_set_peer_link_status(pTdlsPeer,
                                                       eTDLS_LINK_CONNECTED,
                                                       eTDLS_LINK_SUCCESS);
                    /* start TDLS client registration with TL */
                    status = hdd_roamRegisterTDLSSTA( pAdapter, peer, pTdlsPeer->staId, pTdlsPeer->signature);
                    if (VOS_STATUS_SUCCESS == status)
                    {
                        tANI_U8 i;

                        vos_mem_zero(&smeTdlsPeerStateParams,
                                     sizeof(tSmeTdlsPeerStateParams));

                        smeTdlsPeerStateParams.vdevId = pAdapter->sessionId;
                        vos_mem_copy(&smeTdlsPeerStateParams.peerMacAddr,
                                     &pTdlsPeer->peerMac,
                                     sizeof(tSirMacAddr));
                        smeTdlsPeerStateParams.peerState =
                            eSME_TDLS_PEER_STATE_CONNECTED;
                        smeTdlsPeerStateParams.peerCap.isPeerResponder =
                            pTdlsPeer->is_responder;
                        smeTdlsPeerStateParams.peerCap.peerUapsdQueue =
                            pTdlsPeer->uapsdQueues;
                        smeTdlsPeerStateParams.peerCap.peerMaxSp =
                            pTdlsPeer->maxSp;
                        smeTdlsPeerStateParams.peerCap.peerBuffStaSupport =
                            pTdlsPeer->isBufSta;
                        smeTdlsPeerStateParams.peerCap.peerOffChanSupport =
                            pTdlsPeer->isOffChannelSupported;
                        smeTdlsPeerStateParams.peerCap.peerCurrOperClass = 0;
                        smeTdlsPeerStateParams.peerCap.selfCurrOperClass = 0;
                        smeTdlsPeerStateParams.peerCap.peerChanLen =
                            pTdlsPeer->supported_channels_len;
                        smeTdlsPeerStateParams.peerCap.prefOffChanNum =
                            pTdlsPeer->pref_off_chan_num;
                        smeTdlsPeerStateParams.peerCap.prefOffChanBandwidth =
                            pHddCtx->cfg_ini->fTDLSPrefOffChanBandwidth;
                        if (pTdlsPeer->op_class_for_pref_off_chan_is_set) {
                           smeTdlsPeerStateParams.peerCap.opClassForPrefOffChanIsSet =
                                  pTdlsPeer->op_class_for_pref_off_chan_is_set;
                           smeTdlsPeerStateParams.peerCap.opClassForPrefOffChan =
                                  pTdlsPeer->op_class_for_pref_off_chan;
                        }
                        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                           "%s: Peer " MAC_ADDRESS_STR "vdevId: %d, peerState: %d, isPeerResponder: %d, uapsdQueues: 0x%x, maxSp: 0x%x, peerBuffStaSupport: %d, peerOffChanSupport: %d, peerCurrOperClass: %d, selfCurrOperClass: %d, peerChanLen: %d, peerOperClassLen: %d, prefOffChanNum: %d, prefOffChanBandwidth: %d, op_class_for_pref_off_chan_is_set: %d, op_class_for_pref_off_chan: %d",
                              __func__, MAC_ADDR_ARRAY(peer),
                           smeTdlsPeerStateParams.vdevId,
                           smeTdlsPeerStateParams.peerState,
                           smeTdlsPeerStateParams.peerCap.isPeerResponder,
                           smeTdlsPeerStateParams.peerCap.peerUapsdQueue,
                           smeTdlsPeerStateParams.peerCap.peerMaxSp,
                           smeTdlsPeerStateParams.peerCap.peerBuffStaSupport,
                           smeTdlsPeerStateParams.peerCap.peerOffChanSupport,
                           smeTdlsPeerStateParams.peerCap.peerCurrOperClass,
                           smeTdlsPeerStateParams.peerCap.selfCurrOperClass,
                           smeTdlsPeerStateParams.peerCap.peerChanLen,
                           smeTdlsPeerStateParams.peerCap.peerOperClassLen,
                           smeTdlsPeerStateParams.peerCap.prefOffChanNum,
                           smeTdlsPeerStateParams.peerCap.prefOffChanBandwidth,
                           pTdlsPeer->op_class_for_pref_off_chan_is_set,
                           pTdlsPeer->op_class_for_pref_off_chan);

                        for (i = 0; i < pTdlsPeer->supported_channels_len; i++)
                        {
                            smeTdlsPeerStateParams.peerCap.peerChan[i] =
                                pTdlsPeer->supported_channels[i];
                        }
                        smeTdlsPeerStateParams.peerCap.peerOperClassLen =
                            pTdlsPeer->supported_oper_classes_len;
                        for (i = 0; i < pTdlsPeer->supported_oper_classes_len; i++)
                        {
                            smeTdlsPeerStateParams.peerCap.peerOperClass[i] =
                                pTdlsPeer->supported_oper_classes[i];
                        }

                        halStatus = sme_UpdateTdlsPeerState(pHddCtx->hHal,
                                                      &smeTdlsPeerStateParams);
                        if (eHAL_STATUS_SUCCESS != halStatus)
                        {
                           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                     "%s: sme_UpdateTdlsPeerState failed for "
                                     MAC_ADDRESS_STR,
                                     __func__, MAC_ADDR_ARRAY(peer));
                           return -EPERM;
                        }
                        wlan_hdd_tdls_increment_peer_count(pAdapter);
                    }
                    wlan_hdd_tdls_check_bmps(pAdapter);

                    /* Update TL about the UAPSD masks , to route the packets to firmware */
                    if ((TRUE == pHddCtx->cfg_ini->fEnableTDLSBufferSta)
                        || pHddCtx->cfg_ini->fTDLSUapsdMask )
                    {
                        int ac;
                        uint8 ucAc[4] = { WLANTL_AC_VO,
                                          WLANTL_AC_VI,
                                          WLANTL_AC_BK,
                                          WLANTL_AC_BE };
                        uint8 tlTid[4] = { 7, 5, 2, 3 } ;
                        for(ac=0; ac < 4; ac++)
                        {
                            status = WLANTL_EnableUAPSDForAC( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                                               pTdlsPeer->staId, ucAc[ac],
                                                               tlTid[ac], tlTid[ac], 0, 0,
                                                               WLANTL_BI_DIR,
                                                               1,
                                                               pAdapter->sessionId );
                        }
                    }
                }

            }
            break;
        case NL80211_TDLS_DISABLE_LINK:
            {
                pTdlsPeer = wlan_hdd_tdls_find_peer(pAdapter, peer, TRUE);

                if ( NULL == pTdlsPeer ) {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                               "%s: peer matching "MAC_ADDRESS_STR
                               " not found, ignore NL80211_TDLS_DISABLE_LINK",
                                __func__, MAC_ADDR_ARRAY(peer));
                    return -EINVAL;
                }

                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                           "%s: NL80211_TDLS_DISABLE_LINK for peer "
                            MAC_ADDRESS_STR " link_status: %d",
                            __func__, MAC_ADDR_ARRAY(peer), pTdlsPeer->link_status);

                if(TDLS_STA_INDEX_VALID(pTdlsPeer->staId))
                {
                    unsigned long rc;

                    INIT_COMPLETION(pAdapter->tdls_del_station_comp);

                    sme_DeleteTdlsPeerSta( WLAN_HDD_GET_HAL_CTX(pAdapter),
                            pAdapter->sessionId, peer );

                    rc = wait_for_completion_timeout(&pAdapter->tdls_del_station_comp,
                              msecs_to_jiffies(WAIT_TIME_TDLS_DEL_STA));
                    if (!rc) {
                        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                  "%s: Del station timed out", __func__);
                        return -EPERM;
                    }
                    wlan_hdd_tdls_set_peer_link_status(pTdlsPeer,
                                eTDLS_LINK_IDLE,
                                (pTdlsPeer->link_status == eTDLS_LINK_TEARING)?
                                eTDLS_LINK_UNSPECIFIED:
                                eTDLS_LINK_DROPPED_BY_REMOTE);
                }
                else
                {
                    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                              "%s: TDLS Peer Station doesn't exist.", __func__);
                }
            }
            break;
        case NL80211_TDLS_TEARDOWN:
            {
                status = wlan_hdd_tdls_extctrl_deconfig_peer(pAdapter, peer);

                if (0 != status)
                {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                               "%s: Error in TDLS Teardown", __func__);
                    return status;
                 }
            }
            break;
        case NL80211_TDLS_SETUP:
            {
                status = wlan_hdd_tdls_extctrl_config_peer(pAdapter, peer,
                             NULL, pHddCtx->cfg_ini->fTDLSPrefOffChanNum,
                             0, 0, 0);
                if (0 != status)
                {
                    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                               "%s: Error in TDLS Setup", __func__);
                    return status;
                }
            }
            break;
        case NL80211_TDLS_DISCOVERY_REQ:
            /* We don't support in-driver setup/teardown/discovery */
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                      "%s: We don't support in-driver setup/teardown/discovery",
                      __func__);
            return -ENOTSUPP;
        default:
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s: unsupported event", __func__);
            return -ENOTSUPP;
    }
    return 0;
}

static int wlan_hdd_cfg80211_tdls_oper(struct wiphy *wiphy,
                                       struct net_device *dev,
                                       u8 *peer,
                                       enum nl80211_tdls_operation oper)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_tdls_oper(wiphy, dev, peer, oper);
    vos_ssr_unprotect(__func__);

    return ret;
}

int wlan_hdd_cfg80211_send_tdls_discover_req(struct wiphy *wiphy,
                            struct net_device *dev, u8 *peer)
{
    hddLog(VOS_TRACE_LEVEL_INFO,
           "tdls send discover req: "MAC_ADDRESS_STR,
            MAC_ADDR_ARRAY(peer));
#if TDLS_MGMT_VERSION2
    return wlan_hdd_cfg80211_tdls_mgmt(wiphy, dev, peer,
                            WLAN_TDLS_DISCOVERY_REQUEST, 1, 0, 0, NULL, 0);
#else
    return wlan_hdd_cfg80211_tdls_mgmt(wiphy, dev, peer,
                            WLAN_TDLS_DISCOVERY_REQUEST, 1, 0, NULL, 0);
#endif
}
#endif

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/*
 * FUNCTION: wlan_hdd_cfg80211_update_replayCounterCallback
 * Callback routine called upon receiving response for
 * get offload info
 */
void wlan_hdd_cfg80211_update_replayCounterCallback(void *callbackContext,
                            tpSirGtkOffloadGetInfoRspParams pGtkOffloadGetInfoRsp)
{

    hdd_adapter_t *pAdapter = (hdd_adapter_t *)callbackContext;
    tANI_U8 tempReplayCounter[8];
    hdd_station_ctx_t *pHddStaCtx;

    ENTER();

    if (NULL == pAdapter)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD adapter is Null", __func__);
        return ;
    }

    if (NULL == pGtkOffloadGetInfoRsp)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: pGtkOffloadGetInfoRsp is Null", __func__);
        return ;
    }

    if (VOS_STATUS_SUCCESS != pGtkOffloadGetInfoRsp->ulStatus)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: wlan Failed to get replay counter value",
                __func__);
        return ;
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    /* Update replay counter */
    pHddStaCtx->gtkOffloadReqParams.ullKeyReplayCounter =
                                   pGtkOffloadGetInfoRsp->ullKeyReplayCounter;

    {
        /* changing from little to big endian since supplicant
         * works on big endian format
         */
        int i;
        tANI_U8 *p = (tANI_U8 *)&pGtkOffloadGetInfoRsp->ullKeyReplayCounter;

        for (i = 0; i < 8; i++)
        {
            tempReplayCounter[7-i] = (tANI_U8)p[i];
        }
    }

    /* Update replay counter to NL */
    cfg80211_gtk_rekey_notify(pAdapter->dev, pGtkOffloadGetInfoRsp->bssId,
          tempReplayCounter, GFP_KERNEL);
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_set_rekey_data
 * This function is used to offload GTK rekeying job to the firmware.
 */
int __wlan_hdd_cfg80211_set_rekey_data(struct wiphy *wiphy,
                                       struct net_device *dev,
                                       struct cfg80211_gtk_rekey_data *data)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    hdd_station_ctx_t *pHddStaCtx;
    tHalHandle hHal;
    int result;
    tSirGtkOffloadParams hddGtkOffloadReqParams;
    eHalStatus status = eHAL_STATUS_FAILURE;

    ENTER();

    MTRACE(vos_trace(VOS_MODULE_ID_HDD,
                     TRACE_CODE_HDD_CFG80211_SET_REKEY_DATA,
                     pAdapter->sessionId, pAdapter->device_mode));

    result = wlan_hdd_validate_context(pHddCtx);

    if (0 != result) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return result;
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
    if (NULL == hHal){
        hddLog(LOGE, FL("HAL context is Null!!!"));
        return -EAGAIN;
    }

    pHddStaCtx->gtkOffloadReqParams.ulFlags = GTK_OFFLOAD_ENABLE;
    memcpy(pHddStaCtx->gtkOffloadReqParams.aKCK, data->kck, NL80211_KCK_LEN);
    memcpy(pHddStaCtx->gtkOffloadReqParams.aKEK, data->kek, NL80211_KEK_LEN);
    memcpy(pHddStaCtx->gtkOffloadReqParams.bssId, &pHddStaCtx->conn_info.bssId,
          VOS_MAC_ADDR_SIZE);
    {
        /* changing from big to little endian since driver
         * works on little endian format
         */
        tANI_U8 *p =
              (tANI_U8 *)&pHddStaCtx->gtkOffloadReqParams.ullKeyReplayCounter;
        int i;

        for (i = 0; i < 8; i++) {
            p[7-i] = data->replay_ctr[i];
        }
    }

    if (TRUE == pHddCtx->hdd_wlan_suspended) {
        /* if wlan is suspended, enable GTK offload directly from here */
        memcpy(&hddGtkOffloadReqParams, &pHddStaCtx->gtkOffloadReqParams,
              sizeof (tSirGtkOffloadParams));
        status = sme_SetGTKOffload(hHal, &hddGtkOffloadReqParams,
                       pAdapter->sessionId);

        if (eHAL_STATUS_SUCCESS != status) {
            hddLog(LOGE, FL("sme_SetGTKOffload failed, status(%d)"), status);
            return -EINVAL;
        }
        hddLog(LOG1, FL("sme_SetGTKOffload successful"));
    } else {
        hddLog(LOG1, FL("wlan not suspended GTKOffload request is stored"));
    }

    return result;
}

int wlan_hdd_cfg80211_set_rekey_data(struct wiphy *wiphy,
                                     struct net_device *dev,
                                     struct cfg80211_gtk_rekey_data *data)
{
    int ret;

    vos_ssr_protect(__func__);
    ret =  __wlan_hdd_cfg80211_set_rekey_data(wiphy, dev, data);
    vos_ssr_unprotect(__func__);

    return ret;
}
#endif /*WLAN_FEATURE_GTK_OFFLOAD*/
/*
 * FUNCTION: wlan_hdd_cfg80211_set_mac_acl
 * This function is used to set access control policy
 */
static int wlan_hdd_cfg80211_set_mac_acl(struct wiphy *wiphy,
                struct net_device *dev, const struct cfg80211_acl_data *params)
{
    int i;
    hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_hostapd_state_t *pHostapdState;
    tsap_Config_t *pConfig;
    v_CONTEXT_t pVosContext = NULL;
    hdd_context_t *pHddCtx;
    int status;
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

    ENTER();

    if (NULL == params)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: params is Null", __func__);
        return -EINVAL;
    }

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    pVosContext = pHddCtx->pvosContext;
    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

    if (NULL == pHostapdState)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                   "%s: pHostapdState is Null", __func__);
        return -EINVAL;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,"acl policy: = %d"
             "no acl entries = %d", params->acl_policy, params->n_acl_entries);

    if (WLAN_HDD_SOFTAP == pAdapter->device_mode)
    {
        pConfig = &pAdapter->sessionCtx.ap.sapConfig;

        /* default value */
        pConfig->num_accept_mac = 0;
        pConfig->num_deny_mac = 0;

        /**
         * access control policy
         * @NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED: Deny stations which are
         *   listed in hostapd.deny file.
         * @NL80211_ACL_POLICY_DENY_UNLESS_LISTED: Allow stations which are
         *   listed in hostapd.accept file.
         */
        if (NL80211_ACL_POLICY_DENY_UNLESS_LISTED == params->acl_policy)
        {
            pConfig->SapMacaddr_acl = eSAP_DENY_UNLESS_ACCEPTED;
        }
        else if (NL80211_ACL_POLICY_ACCEPT_UNLESS_LISTED == params->acl_policy)
        {
            pConfig->SapMacaddr_acl = eSAP_ACCEPT_UNLESS_DENIED;
        }
        else
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s:Acl Policy : %d is not supported",
                                            __func__, params->acl_policy);
            return -ENOTSUPP;
        }

        if (eSAP_DENY_UNLESS_ACCEPTED == pConfig->SapMacaddr_acl)
        {
            pConfig->num_accept_mac = params->n_acl_entries;
            for (i = 0; i < params->n_acl_entries; i++)
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "** Add ACL MAC entry %i in WhiletList :"
                                MAC_ADDRESS_STR, i,
                                MAC_ADDR_ARRAY(params->mac_addrs[i].addr));

                vos_mem_copy(&pConfig->accept_mac[i], params->mac_addrs[i].addr,
                                                             sizeof(qcmacaddr));
            }
        }
        else if (eSAP_ACCEPT_UNLESS_DENIED == pConfig->SapMacaddr_acl)
        {
            pConfig->num_deny_mac = params->n_acl_entries;
            for (i = 0; i < params->n_acl_entries; i++)
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                    "** Add ACL MAC entry %i in BlackList :"
                                MAC_ADDRESS_STR, i,
                                MAC_ADDR_ARRAY(params->mac_addrs[i].addr));

                vos_mem_copy(&pConfig->deny_mac[i], params->mac_addrs[i].addr,
                                                           sizeof(qcmacaddr));
            }
        }

#ifdef WLAN_FEATURE_MBSSID
        vos_status = WLANSAP_SetMacACL(WLAN_HDD_GET_SAP_CTX_PTR(pAdapter), pConfig);
#else
        vos_status = WLANSAP_SetMacACL(pVosContext, pConfig);
#endif
        if (!VOS_IS_STATUS_SUCCESS(vos_status))
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                       "%s: SAP Set Mac Acl fail", __func__);
            return -EINVAL;
        }
    }
    else
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid device_mode = %d",
                                 __func__, pAdapter->device_mode);
        return -EINVAL;
    }

    return 0;
}

#ifdef WLAN_NL80211_TESTMODE
#ifdef FEATURE_WLAN_LPHB
void wlan_hdd_cfg80211_lphb_ind_handler
(
   void *pHddCtx,
   tSirLPHBInd *lphbInd
)
{
   struct sk_buff  *skb;

   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "LPHB indication arrived");

   if (0 != wlan_hdd_validate_context((hdd_context_t *)pHddCtx)) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: invalid argument pHddCtx", __func__);
      return;
   }

   if (NULL == lphbInd) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: invalid argument lphbInd", __func__);
      return;
   }

   skb = cfg80211_testmode_alloc_event_skb(
                  ((hdd_context_t *)pHddCtx)->wiphy,
                  sizeof(tSirLPHBInd),
                  GFP_ATOMIC);
   if (!skb)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "LPHB timeout, NL buffer alloc fail");
      return;
   }

   if(nla_put_u32(skb, WLAN_HDD_TM_ATTR_CMD, WLAN_HDD_TM_CMD_WLAN_HB))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "WLAN_HDD_TM_ATTR_CMD put fail");
      goto nla_put_failure;
   }
   if(nla_put_u32(skb, WLAN_HDD_TM_ATTR_TYPE, lphbInd->protocolType))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "WLAN_HDD_TM_ATTR_TYPE put fail");
      goto nla_put_failure;
   }
   if(nla_put(skb, WLAN_HDD_TM_ATTR_DATA,
           sizeof(tSirLPHBInd), lphbInd))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "WLAN_HDD_TM_ATTR_DATA put fail");
      goto nla_put_failure;
   }
   cfg80211_testmode_event(skb, GFP_ATOMIC);
   return;

nla_put_failure:
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "NLA Put fail");
   kfree_skb(skb);

   return;
}
#endif /* FEATURE_WLAN_LPHB */

static int __wlan_hdd_cfg80211_testmode(struct wiphy *wiphy,
                                        void *data, int len)
{
    struct nlattr *tb[WLAN_HDD_TM_ATTR_MAX + 1];
    int err = 0;
#ifdef FEATURE_WLAN_LPHB
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    eHalStatus smeStatus;
#endif /* FEATURE_WLAN_LPHB */

    err = nla_parse(tb, WLAN_HDD_TM_ATTR_MAX, data, len, wlan_hdd_tm_policy);
    if (err) {
        hddLog(LOGE, FL("Testmode INV ATTR"));
        return err;
    }

    if (!tb[WLAN_HDD_TM_ATTR_CMD]) {
        hddLog(LOGE, FL("Testmode INV CMD"));
        return -EINVAL;
    }

    switch (nla_get_u32(tb[WLAN_HDD_TM_ATTR_CMD]))
    {
#ifdef FEATURE_WLAN_LPHB
        /* Low Power Heartbeat configuration request */
        case WLAN_HDD_TM_CMD_WLAN_HB:
        {
            int   buf_len;
            void *buf;
            tSirLPHBReq *hb_params = NULL;
            tSirLPHBReq *hb_params_temp = NULL;

            if (!tb[WLAN_HDD_TM_ATTR_DATA]) {
                hddLog(LOGE, FL("Testmode INV DATA"));
                return -EINVAL;
            }

            buf = nla_data(tb[WLAN_HDD_TM_ATTR_DATA]);
            buf_len = nla_len(tb[WLAN_HDD_TM_ATTR_DATA]);

            hb_params_temp =(tSirLPHBReq *)buf;
            if ((hb_params_temp->cmd == LPHB_SET_TCP_PARAMS_INDID) &&
                (hb_params_temp->params.lphbTcpParamReq.timePeriodSec == 0))
                return -EINVAL;

            hb_params = (tSirLPHBReq *)vos_mem_malloc(sizeof(tSirLPHBReq));
            if (NULL == hb_params) {
                hddLog(LOGE, FL("Request Buffer Alloc Fail"));
                return -ENOMEM;
            }

            vos_mem_copy(hb_params, buf, buf_len);
            smeStatus = sme_LPHBConfigReq((tHalHandle)(pHddCtx->hHal),
                               hb_params,
                               wlan_hdd_cfg80211_lphb_ind_handler);
            if (eHAL_STATUS_SUCCESS != smeStatus) {
               hddLog(LOGE, "LPHB Config Fail, disable");
               vos_mem_free(hb_params);
            }
            return 0;
         }
#endif /* FEATURE_WLAN_LPHB */

#if  defined(QCA_WIFI_FTM)
        case WLAN_HDD_TM_CMD_WLAN_FTM:
        {
            int   buf_len;
            void *buf;
            VOS_STATUS status;
            if (!tb[WLAN_HDD_TM_ATTR_DATA]) {
                    hddLog(LOGE,
                           FL("WLAN_HDD_TM_ATTR_DATA attribute is invalid"));
                    return -EINVAL;
            }

            buf = nla_data(tb[WLAN_HDD_TM_ATTR_DATA]);
            buf_len = nla_len(tb[WLAN_HDD_TM_ATTR_DATA]);

            pr_info("****FTM Tx cmd len = %d*****\n", buf_len);

            status = wlan_hdd_ftm_testmode_cmd(buf, buf_len);

            if (status != VOS_STATUS_SUCCESS)
                err = -EBUSY;
            break;
        }
#endif

        default:
            hddLog(LOGE, FL("command %d not supported"),
                         nla_get_u32(tb[WLAN_HDD_TM_ATTR_CMD]));
            return -EOPNOTSUPP;
    }

   return err;
}

static int wlan_hdd_cfg80211_testmode(struct wiphy *wiphy, void *data, int len)
{
   int ret;

   vos_ssr_protect(__func__);
   ret = __wlan_hdd_cfg80211_testmode(wiphy, data, len);
   vos_ssr_unprotect(__func__);

   return ret;
}

#if  defined(QCA_WIFI_FTM)
void wlan_hdd_testmode_rx_event(void *buf, size_t buf_len)
{
    struct sk_buff *skb;
    hdd_context_t *hdd_ctx;
    void *vos_global_ctx;

    if (!buf || !buf_len) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: buf or buf_len invalid, buf = %p buf_len = %zu",
                  __func__, buf, buf_len);
        return;
    }

    vos_global_ctx = vos_get_global_context(VOS_MODULE_ID_HDD, NULL);

    if (!vos_global_ctx) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: voss global context invalid",
                  __func__);
        return;
    }

    hdd_ctx = vos_get_context(VOS_MODULE_ID_HDD, vos_global_ctx);

    if (!hdd_ctx) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: hdd context invalid",
                  __func__);
        return;
    }

    skb = cfg80211_testmode_alloc_event_skb(hdd_ctx->wiphy,
                                            buf_len, GFP_KERNEL);
    if (!skb) {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                  "%s: failed to allocate testmode rx skb!",
                  __func__);
        return;
    }

    if (nla_put_u32(skb, WLAN_HDD_TM_ATTR_CMD, WLAN_HDD_TM_CMD_WLAN_FTM) ||
        nla_put(skb, WLAN_HDD_TM_ATTR_DATA, buf_len, buf))
        goto nla_put_failure;

    pr_info("****FTM Rx cmd len = %zu*****\n", buf_len);

    cfg80211_testmode_event(skb, GFP_KERNEL);
    return;

nla_put_failure:
    kfree_skb(skb);
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
              "%s: nla_put failed on testmode rx skb!",
              __func__);
}
#endif
#endif /* CONFIG_NL80211_TESTMODE */

static int __wlan_hdd_cfg80211_dump_survey(struct wiphy *wiphy,
                                           struct net_device *dev,
                                           int idx, struct survey_info *survey)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    hdd_station_ctx_t *pHddStaCtx;
    tHalHandle halHandle;
    v_U32_t channel = 0, freq = 0; /* Initialization Required */
    v_S7_t snr,rssi;
    int status, i, j, filled = 0;

    ENTER();

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: HDD context is not valid", __func__);
        return status;
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    if (0 == pHddCtx->cfg_ini->fEnableSNRMonitoring ||
        0 != pAdapter->survey_idx ||
        eConnectionState_Associated != pHddStaCtx->conn_info.connState)
    {
        /* The survey dump ops when implemented completely is expected to
         * return a survey of all channels and the ops is called by the
         * kernel with incremental values of the argument 'idx' till it
         * returns -ENONET. But we can only support the survey for the
         * operating channel for now. survey_idx is used to track
         * that the ops is called only once and then return -ENONET for
         * the next iteration
         */
        pAdapter->survey_idx = 0;
        return -ENONET;
    }

    halHandle = WLAN_HDD_GET_HAL_CTX(pAdapter);

    wlan_hdd_get_snr(pAdapter, &snr);
    wlan_hdd_get_rssi(pAdapter, &rssi);

    sme_GetOperationChannel(halHandle, &channel, pAdapter->sessionId);
    hdd_wlan_get_freq(channel, &freq);


    for (i = 0; i < IEEE80211_NUM_BANDS; i++)
    {
        if (NULL == wiphy->bands[i])
        {
           VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
                     "%s: wiphy->bands[i] is NULL, i = %d", __func__, i);
           continue;
        }

        for (j = 0; j < wiphy->bands[i]->n_channels; j++)
        {
            struct ieee80211_supported_band *band = wiphy->bands[i];

            if (band->channels[j].center_freq == (v_U16_t)freq)
            {
                survey->channel = &band->channels[j];
                /* The Rx BDs contain SNR values in dB for the received frames
                 * while the supplicant expects noise. So we calculate and
                 * return the value of noise (dBm)
                 *  SNR (dB) = RSSI (dBm) - NOISE (dBm)
                 */
                survey->noise = rssi - snr;
                survey->filled = SURVEY_INFO_NOISE_DBM;
                filled = 1;
            }
        }
     }

     if (filled)
        pAdapter->survey_idx = 1;
     else
     {
        pAdapter->survey_idx = 0;
        return -ENONET;
     }

     return 0;
}

static int wlan_hdd_cfg80211_dump_survey(struct wiphy *wiphy,
                                         struct net_device *dev,
                                         int idx, struct survey_info *survey)
{
     int ret;

     vos_ssr_protect(__func__);
     ret = __wlan_hdd_cfg80211_dump_survey(wiphy, dev, idx, survey);
     vos_ssr_unprotect(__func__);

     return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_resume_wlan
 * this is called when cfg80211 driver resume
 * driver updates  latest sched_scan scan result(if any) to cfg80211 database
 */
int __wlan_hdd_cfg80211_resume_wlan(struct wiphy *wiphy)
{
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    hdd_adapter_t *pAdapter;
    hdd_adapter_list_node_t *pAdapterNode, *pNext;
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    int result;
    pVosSchedContext vosSchedContext = get_vos_sched_ctxt();

    ENTER();

    result = wlan_hdd_validate_context(pHddCtx);
    if (0 != result) {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context is not valid"));
       return result;
    }

#ifdef CONFIG_CNSS
    cnss_request_bus_bandwidth(CNSS_BUS_WIDTH_MEDIUM);
#endif

    /* Resume MC thread */
    if (pHddCtx->isMcThreadSuspended) {
        complete(&vosSchedContext->ResumeMcEvent);
        pHddCtx->isMcThreadSuspended = FALSE;
    }

#ifdef QCA_CONFIG_SMP
    /* Resume tlshim Rx thread */
    if (pHddCtx->isTlshimRxThreadSuspended) {
        complete(&vosSchedContext->ResumeTlshimRxEvent);
        pHddCtx->isTlshimRxThreadSuspended = FALSE;
    }

#endif
    hdd_resume_wlan();

    spin_lock(&pHddCtx->schedScan_lock);
    pHddCtx->isWiphySuspended = FALSE;
    if (TRUE != pHddCtx->isSchedScanUpdatePending) {
        spin_unlock(&pHddCtx->schedScan_lock);
        hddLog(LOG1, FL("Return resume is not due to PNO indication"));
        return 0;
    }
    /* Reset flag to avoid updating cfg80211 data old results again */
    pHddCtx->isSchedScanUpdatePending = FALSE;
    spin_unlock(&pHddCtx->schedScan_lock);

    status = hdd_get_front_adapter (pHddCtx, &pAdapterNode);

    while (NULL != pAdapterNode && VOS_STATUS_SUCCESS == status) {
        pAdapter = pAdapterNode->pAdapter;
        if ((NULL != pAdapter) &&
            (WLAN_HDD_INFRA_STATION == pAdapter->device_mode)) {
            if (0 != wlan_hdd_cfg80211_update_bss(pHddCtx->wiphy, pAdapter)) {
                hddLog(LOGW, FL("NO SCAN result"));
            } else {
                /* Acquire wakelock to handle the case where APP's tries to
                 * suspend immediately after updating the scan results. This
                 * results in app's is in suspended state and not able to
                 * process the connect request to AP
                 */
                hdd_prevent_suspend_timeout(2000,
                                         WIFI_POWER_EVENT_WAKELOCK_RESUME_WLAN);
                cfg80211_sched_scan_results(pHddCtx->wiphy);
            }

            hddLog(LOG1, FL("cfg80211 scan result database updated"));

            return result;
        }
        status = hdd_get_next_adapter (pHddCtx, pAdapterNode, &pNext);
        pAdapterNode = pNext;
    }

    hddLog(LOG1, FL("Failed to find Adapter"));
    return result;
}

void wlan_hdd_cfg80211_ready_to_suspend(void *callbackContext, boolean suspended)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)callbackContext;
    pHddCtx->suspended = suspended;
    complete(&pHddCtx->ready_to_suspend);
}

int wlan_hdd_cfg80211_resume_wlan(struct wiphy *wiphy)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_resume_wlan(wiphy);
    vos_ssr_unprotect(__func__);

    return ret;
}

/*
 * FUNCTION: __wlan_hdd_cfg80211_suspend_wlan
 * this is called when cfg80211 driver suspends
 */
int __wlan_hdd_cfg80211_suspend_wlan(struct wiphy *wiphy,
                                   struct cfg80211_wowlan *wow)
{
#ifdef QCA_CONFIG_SMP
#define RX_TLSHIM_SUSPEND_TIMEOUT 200 /* msecs */
#endif
    hdd_context_t *pHddCtx = wiphy_priv(wiphy);
    pVosSchedContext vosSchedContext = get_vos_sched_ctxt();
    hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
    hdd_adapter_t *pAdapter;
    hdd_scaninfo_t *pScanInfo;
    VOS_STATUS status;
    int rc;

    ENTER();
    rc = wlan_hdd_validate_context(pHddCtx);
    if (0 != rc) {
       hddLog(LOGE, FL("HDD context is not valid"));
       return rc;
    }

    /* If RADAR detection is in progress (HDD), prevent suspend. The flag
     * "dfs_cac_block_tx" is set to TRUE when RADAR is found and stay TRUE until
     * CAC is done for a SoftAP which is in started state.
     */
    status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);
    while (NULL != pAdapterNode && VOS_STATUS_SUCCESS == status) {
        pAdapter = pAdapterNode->pAdapter;
        if (WLAN_HDD_SOFTAP == pAdapter->device_mode) {
            if (BSS_START ==
                    WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter)->bssState &&
                VOS_TRUE ==
                    WLAN_HDD_GET_AP_CTX_PTR(pAdapter)->dfs_cac_block_tx) {
                hddLog(VOS_TRACE_LEVEL_DEBUG,
                    FL("RADAR detection in progress, do not allow suspend"));
                return -EAGAIN;
            } else if (!pHddCtx->cfg_ini->enableSapSuspend) {
                /* return -EOPNOTSUPP if SAP does not support suspend
                 */
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s:SAP does not support suspend!!", __func__);
                return -EOPNOTSUPP;
            }
        } else if (WLAN_HDD_P2P_GO == pAdapter->device_mode) {
            if (!pHddCtx->cfg_ini->enableSapSuspend) {
                /* return -EOPNOTSUPP if GO does not support suspend
                 */
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                    "%s:GO does not support suspend!!", __func__);
                return -EOPNOTSUPP;
            }
        }
        status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
        pAdapterNode = pNext;
    }

    /* Stop ongoing scan on each interface */
    status =  hdd_get_front_adapter ( pHddCtx, &pAdapterNode );
    while ( NULL != pAdapterNode && VOS_STATUS_SUCCESS == status )
    {
        pAdapter = pAdapterNode->pAdapter;
        pScanInfo = &pAdapter->scan_info;

        if (sme_staInMiddleOfRoaming(pHddCtx->hHal, pAdapter->sessionId)) {
            hddLog(LOG1, FL("Roaming in progress, do not allow suspend"));
            return -EAGAIN;
        }

        if (pHddCtx->cfg_ini->enablePowersaveOffload &&
            pHddCtx->cfg_ini->fIsBmpsEnabled &&
            ((WLAN_HDD_INFRA_STATION == pAdapter->device_mode) ||
            (WLAN_HDD_P2P_CLIENT == pAdapter->device_mode))) {
            if (!sme_PsOffloadIsStaInPowerSave(pHddCtx->hHal,
                                               pAdapter->sessionId)) {
                hddLog(VOS_TRACE_LEVEL_DEBUG,
                  FL("STA is not in power save, Do not allow suspend"));
                return -EAGAIN;
            }
        }

        if (pScanInfo->mScanPending && pAdapter->request)
        {
           INIT_COMPLETION(pScanInfo->abortscan_event_var);
           hdd_abort_mac_scan(pHddCtx, pAdapter->sessionId,
                              eCSR_SCAN_ABORT_DEFAULT);

           status = wait_for_completion_timeout(
                           &pScanInfo->abortscan_event_var,
                           msecs_to_jiffies(WLAN_WAIT_TIME_ABORTSCAN));
           if (!status)
           {
              VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                         "%s: Timeout occurred while waiting for abort scan" ,
                         __func__);
              return -ETIME;
           }
        }
        status = hdd_get_next_adapter ( pHddCtx, pAdapterNode, &pNext );
        pAdapterNode = pNext;
    }

#ifdef IPA_OFFLOAD
    /*
     * Suspend IPA early before proceeding to suspend other entities like
     * firmware to avoid any race conditions.
     */
    if (hdd_ipa_suspend(pHddCtx)) {
        hddLog(VOS_TRACE_LEVEL_DEBUG, FL("IPA not ready to suspend!"));
        return -EAGAIN;
    }
#endif

    /* Wait for the target to be ready for suspend */
    INIT_COMPLETION(pHddCtx->ready_to_suspend);

    hdd_suspend_wlan(&wlan_hdd_cfg80211_ready_to_suspend, pHddCtx);

    rc = wait_for_completion_timeout(&pHddCtx->ready_to_suspend,
                             msecs_to_jiffies(WLAN_WAIT_TIME_READY_TO_SUSPEND));
    if (!rc)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Failed to get ready to suspend", __func__);
        goto resume_tx;
    }

    if (!pHddCtx->suspended) {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Faied as suspend_status is wrong:%d",
                           __func__, pHddCtx->suspended);
       goto resume_tx;
    }

    /* Suspend MC thread */
    set_bit(MC_SUSPEND_EVENT_MASK, &vosSchedContext->mcEventFlag);
    wake_up_interruptible(&vosSchedContext->mcWaitQueue);

    /* Wait for suspend confirmation from MC thread */
    rc = wait_for_completion_timeout(&pHddCtx->mc_sus_event_var,
                                 msecs_to_jiffies(WLAN_WAIT_TIME_MCTHREAD_SUSPEND));
    if (!rc)
    {
        clear_bit(MC_SUSPEND_EVENT_MASK, &vosSchedContext->mcEventFlag);
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Failed to stop mc thread", __func__);
        goto resume_tx;
    }

    pHddCtx->isMcThreadSuspended = TRUE;

#ifdef QCA_CONFIG_SMP
    /* Suspend tlshim rx thread */
    set_bit(RX_SUSPEND_EVENT_MASK, &vosSchedContext->tlshimRxEvtFlg);
    wake_up_interruptible(&vosSchedContext->tlshimRxWaitQueue);
    rc = wait_for_completion_timeout(
                     &vosSchedContext->SuspndTlshimRxEvent,
                     msecs_to_jiffies(RX_TLSHIM_SUSPEND_TIMEOUT));
    if (!rc) {
        clear_bit(RX_SUSPEND_EVENT_MASK, &vosSchedContext->tlshimRxEvtFlg);
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s: Failed to stop tl_shim rx thread", __func__);
        goto resume_all;
    }
    pHddCtx->isTlshimRxThreadSuspended = TRUE;
#endif

    pHddCtx->isWiphySuspended = TRUE;

#ifdef CONFIG_CNSS
    cnss_request_bus_bandwidth(CNSS_BUS_WIDTH_NONE);
#endif

    EXIT();
    return 0;

#ifdef QCA_CONFIG_SMP
resume_all:

    complete(&vosSchedContext->ResumeMcEvent);
    pHddCtx->isMcThreadSuspended = FALSE;
#endif

resume_tx:

    hdd_resume_wlan();

    return -ETIME;
}

int wlan_hdd_cfg80211_suspend_wlan(struct wiphy *wiphy,
                                   struct cfg80211_wowlan *wow)
{
    int ret;

    vos_ssr_protect(__func__);
    ret = __wlan_hdd_cfg80211_suspend_wlan(wiphy, wow);
    vos_ssr_unprotect(__func__);

    return ret;
}

#ifdef QCA_HT_2040_COEX
int wlan_hdd_cfg80211_set_ap_channel_width(struct wiphy *wiphy,
                                     struct net_device *dev,
                                     struct cfg80211_chan_def *chandef)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx;
    VOS_STATUS status;
    tSmeConfigParams smeConfig;
    bool cbModeChange;

    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    status = wlan_hdd_validate_context(pHddCtx);

    if (0 != status) {
        hddLog(LOGE, FL("HDD context is not valid"));
        return status;
    }

    if (!pHddCtx->cfg_ini->ht2040CoexEnabled)
        return -EOPNOTSUPP;

    vos_mem_zero(&smeConfig, sizeof (tSmeConfigParams));
    sme_GetConfigParam(pHddCtx->hHal, &smeConfig);
    switch (chandef->width) {
    case NL80211_CHAN_WIDTH_20:
        if (smeConfig.csrConfig.channelBondingMode24GHz !=
                      eCSR_INI_SINGLE_CHANNEL_CENTERED) {
            smeConfig.csrConfig.channelBondingMode24GHz =
                      eCSR_INI_SINGLE_CHANNEL_CENTERED;
            sme_UpdateConfig(pHddCtx->hHal, &smeConfig);
            cbModeChange = TRUE;
        }
        break;

    case NL80211_CHAN_WIDTH_40:
        if (smeConfig.csrConfig.channelBondingMode24GHz ==
                      eCSR_INI_SINGLE_CHANNEL_CENTERED) {
            if ( NL80211_CHAN_HT40MINUS == cfg80211_get_chandef_type(chandef))
                smeConfig.csrConfig.channelBondingMode24GHz =
                      eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY;
            else
                smeConfig.csrConfig.channelBondingMode24GHz =
                      eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY;
            sme_UpdateConfig(pHddCtx->hHal, &smeConfig);
            cbModeChange = TRUE;
        }
        break;

    default:
        hddLog(LOGE, FL("Error!!! Invalid HT20/40 mode !"));
        return -EINVAL;
    }

    if (!cbModeChange)
       return 0;

    if (WLAN_HDD_SOFTAP != pAdapter->device_mode)
        return 0;

    hddLog(LOG1, FL("Channel bonding changed to %d"),
                 smeConfig.csrConfig.channelBondingMode24GHz);

    /* Change SAP ht2040 mode */
    status = hdd_set_sap_ht2040_mode(pAdapter,
                                     cfg80211_get_chandef_type(chandef));
    if (status != VOS_STATUS_SUCCESS) {
        hddLog(LOGE,
               FL("Error!!! Cannot set SAP HT20/40 mode!"));
        return -EINVAL;
    }

    return 0;
}
#endif

#ifdef FEATURE_WLAN_EXTSCAN
/**
 * wlan_hdd_cfg80211_extscan_get_capabilities_rsp() - response from target
 * @ctx: hdd global context
 * @data: capabilities data
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_get_capabilities_rsp(void *ctx,
	struct ext_scan_capabilities_response *data)
{
	struct hdd_ext_scan_context *context;
	hdd_context_t *hdd_ctx  = (hdd_context_t *)ctx;

	ENTER();

	if (wlan_hdd_validate_context(hdd_ctx) || !data) {
		hddLog(LOGE, FL("HDD context is invalid or data(%p) is null"),
			data);
		return;
	}

	spin_lock(&hdd_context_lock);

	context = &hdd_ctx->ext_scan_context;
	/* validate response received from target*/
	if (context->request_id != data->requestId) {
		spin_unlock(&hdd_context_lock);
		hddLog(LOGE,
			FL("Target response id did not match: request_id %d resposne_id %d"),
			context->request_id, data->requestId);
		return;
	} else {
		context->capability_response = *data;
		complete(&context->response_event);
	}

	spin_unlock(&hdd_context_lock);

	return;
}

/*
 * define short names for the global vendor params
 * used by hdd_extscan_nl_fill_bss()
 */
#define PARAM_TIME_STAMP \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_TIME_STAMP
#define PARAM_SSID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_SSID
#define PARAM_BSSID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BSSID
#define PARAM_CHANNEL \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CHANNEL
#define PARAM_RSSI \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RSSI
#define PARAM_RTT \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT
#define PARAM_RTT_SD \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT_SD
#define PARAM_BEACON_PERIOD \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BEACON_PERIOD
#define PARAM_CAPABILITY \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CAPABILITY
#define PARAM_IE_LENGTH \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_IE_LENGTH
#define PARAM_IE_DATA \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_IE_DATA

/** hdd_extscan_nl_fill_bss() - extscan nl fill bss
 * @skb: socket buffer
 * @ap: bss information
 * @idx: nesting index
 *
 * Return: 0 on success; error number otherwise
 */
static int hdd_extscan_nl_fill_bss(struct sk_buff *skb, tSirWifiScanResult *ap,
					int idx)
{
	struct nlattr *nla_ap;

	nla_ap = nla_nest_start(skb, idx);
	if (!nla_ap)
		return -EINVAL;

	if (nla_put_u64(skb, PARAM_TIME_STAMP, ap->ts) ||
	    nla_put(skb, PARAM_SSID, sizeof(ap->ssid), ap->ssid) ||
	    nla_put(skb, PARAM_BSSID, sizeof(ap->bssid), ap->bssid) ||
	    nla_put_u32(skb, PARAM_CHANNEL, ap->channel) ||
	    nla_put_s32(skb, PARAM_RSSI, ap->rssi) ||
	    nla_put_u32(skb, PARAM_RTT, ap->rtt) ||
	    nla_put_u32(skb, PARAM_RTT_SD, ap->rtt_sd) ||
	    nla_put_u16(skb, PARAM_BEACON_PERIOD, ap->beaconPeriod) ||
	    nla_put_u16(skb, PARAM_CAPABILITY, ap->capability) ||
	    nla_put_u16(skb, PARAM_IE_LENGTH, ap->ieLength)) {
		hddLog(LOGE, FL("put fail"));
		return -EINVAL;
	}

	if (ap->ieLength)
		if (nla_put(skb, PARAM_IE_DATA, ap->ieLength, ap->ieData)) {
			hddLog(LOGE, FL("put fail"));
			return -EINVAL;
		}

	nla_nest_end(skb, nla_ap);

	return 0;
}
/*
 * done with short names for the global vendor params
 * used by hdd_extscan_nl_fill_bss()
 */
#undef PARAM_TIME_STAMP
#undef PARAM_SSID
#undef PARAM_BSSID
#undef PARAM_CHANNEL
#undef PARAM_RSSI
#undef PARAM_RTT
#undef PARAM_RTT_SD
#undef PARAM_BEACON_PERIOD
#undef PARAM_CAPABILITY
#undef PARAM_IE_LENGTH
#undef PARAM_IE_DATA


/** wlan_hdd_cfg80211_extscan_cached_results_ind() - get cached results
 * @ctx: hdd global context
 * @data: cached results
 *
 * This function reads the cached results %data, populates the NL
 * attributes and sends the NL event to the upper layer.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_cached_results_ind(void *ctx,
				struct extscan_cached_scan_results *data)
{
	hdd_context_t *pHddCtx = (hdd_context_t *)ctx;
	struct extscan_cached_scan_result *result;
	struct hdd_ext_scan_context *context;
	struct sk_buff *skb    = NULL;
	tSirWifiScanResult *ap;
	uint32_t i, j, nl_buf_len;
	bool ignore_cached_results = false;

	ENTER();

	if (wlan_hdd_validate_context(pHddCtx) || !data) {
		hddLog(LOGE,
		FL("HDD context is invalid or data(%p) is null"), data);
	        return;
	}

	spin_lock(&hdd_context_lock);
	context = &pHddCtx->ext_scan_context;
	ignore_cached_results = context->ignore_cached_results;
	spin_unlock(&hdd_context_lock);

	if (ignore_cached_results) {
		hddLog(LOGE,
			FL("Ignore the cached results received after timeout"));
		return;
	}

#define EXTSCAN_CACHED_NEST_HDRLEN NLA_HDRLEN
#define EXTSCAN_CACHED_NL_FIXED_TLV \
		(sizeof(data->request_id) + NLA_HDRLEN) + \
		(sizeof(data->num_scan_ids) + NLA_HDRLEN) + \
		(sizeof(data->more_data) + NLA_HDRLEN)
#define EXTSCAN_CACHED_NL_SCAN_ID_TLV \
		(sizeof(result->scan_id) + NLA_HDRLEN) + \
		(sizeof(result->flags) + NLA_HDRLEN) + \
		(sizeof(result->num_results) + NLA_HDRLEN)
#define EXTSCAN_CACHED_NL_SCAN_RESULTS_TLV \
		(sizeof(ap->ts) + NLA_HDRLEN) + \
		(sizeof(ap->ssid) + NLA_HDRLEN) + \
		(sizeof(ap->bssid) + NLA_HDRLEN) + \
		(sizeof(ap->channel) + NLA_HDRLEN) + \
		(sizeof(ap->rssi) + NLA_HDRLEN) + \
		(sizeof(ap->rtt) + NLA_HDRLEN) + \
		(sizeof(ap->rtt_sd) + NLA_HDRLEN) + \
		(sizeof(ap->beaconPeriod) + NLA_HDRLEN) + \
		(sizeof(ap->capability) + NLA_HDRLEN) + \
		(sizeof(ap->ieLength) + NLA_HDRLEN)
#define EXTSCAN_CACHED_NL_SCAN_RESULTS_IE_DATA_TLV \
		(ap->ieLength + NLA_HDRLEN)

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len += EXTSCAN_CACHED_NL_FIXED_TLV;
	if (data->num_scan_ids) {
		nl_buf_len += sizeof(result->scan_id) + NLA_HDRLEN;
		nl_buf_len += EXTSCAN_CACHED_NEST_HDRLEN;
		result = &data->result[0];
		for (i = 0; i < data->num_scan_ids; i++) {
			nl_buf_len += EXTSCAN_CACHED_NEST_HDRLEN;
			nl_buf_len += EXTSCAN_CACHED_NL_SCAN_ID_TLV;
			nl_buf_len += EXTSCAN_CACHED_NEST_HDRLEN;

			ap = &result->ap[0];
			for (j = 0; j < result->num_results; j++) {
				nl_buf_len += EXTSCAN_CACHED_NEST_HDRLEN;
				nl_buf_len +=
					EXTSCAN_CACHED_NL_SCAN_RESULTS_TLV;
				if (ap->ieLength)
					nl_buf_len +=
					EXTSCAN_CACHED_NL_SCAN_RESULTS_IE_DATA_TLV;
				ap++;
			}
			result++;
		}
	}

	hddLog(LOG2, FL("nl_buf_len = %u"), nl_buf_len);
	skb = cfg80211_vendor_cmd_alloc_reply_skb(pHddCtx->wiphy, nl_buf_len);

	if (!skb) {
		hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
		goto fail;
	}
	hddLog(LOG1, "Req Id %u Num_scan_ids %u More Data %u",
		data->request_id, data->num_scan_ids, data->more_data);

	result = &data->result[0];
	for (i = 0; i < data->num_scan_ids; i++) {
		hddLog(LOG1, "[i=%d] scan_id %u flags %u num_results %u",
			i, result->scan_id, result->flags, result->num_results);

		ap = &result->ap[0];
		for (j = 0; j < result->num_results; j++) {
			/*
			 * Firmware returns timestamp from ext scan start till
			 * BSSID was cached (in micro seconds). Add this with
			 * time gap between system boot up to ext scan start
			 * to derive the time since boot when the
			 * BSSID was cached.
			 */
			ap->ts += pHddCtx->ext_scan_start_since_boot;
			hddLog(LOG1, "Timestamp %llu "
				"Ssid: %s "
				"Bssid (" MAC_ADDRESS_STR ") "
				"Channel %u "
				"Rssi %d "
				"RTT %u "
				"RTT_SD %u "
				"Beacon Period %u "
				"Capability 0x%x "
				"Ie length %d",
				ap->ts,
				ap->ssid,
				MAC_ADDR_ARRAY(ap->bssid),
				ap->channel,
				ap->rssi,
				ap->rtt,
				ap->rtt_sd,
				ap->beaconPeriod,
				ap->capability,
				ap->ieLength);
			ap++;
		}
		result++;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->request_id) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_NUM_RESULTS_AVAILABLE,
		data->num_scan_ids) ||
	    nla_put_u8(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
		data->more_data)) {
		hddLog(LOGE, FL("put fail"));
		goto fail;
	}

	if (data->num_scan_ids) {
		struct nlattr *nla_results;
		result = &data->result[0];

		if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_CACHED_RESULTS_SCAN_ID,
			result->scan_id)) {
			hddLog(LOGE, FL("put fail"));
			goto fail;
		}
		nla_results = nla_nest_start(skb,
			      QCA_WLAN_VENDOR_ATTR_EXTSCAN_CACHED_RESULTS_LIST);
		if (!nla_results)
			goto fail;

		for (i = 0; i < data->num_scan_ids; i++) {
			struct nlattr *nla_result;
			struct nlattr *nla_aps;

			nla_result = nla_nest_start(skb, i);
			if(!nla_result)
				goto fail;

			if (nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CACHED_RESULTS_SCAN_ID,
				result->scan_id) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CACHED_RESULTS_FLAGS,
				result->flags) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_NUM_RESULTS_AVAILABLE,
				result->num_results)) {
				hddLog(LOGE, FL("put fail"));
				goto fail;
			}

			nla_aps = nla_nest_start(skb,
				     QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
			if (!nla_aps)
				goto fail;

			ap = &result->ap[0];
			for (j = 0; j < result->num_results; j++) {
				if (hdd_extscan_nl_fill_bss(skb, ap, j))
					goto fail;
				ap++;
			}
			nla_nest_end(skb, nla_aps);
			nla_nest_end(skb, nla_result);
			result++;
		}
		nla_nest_end(skb, nla_results);
	}

	cfg80211_vendor_cmd_reply(skb);

	if (!data->more_data) {
		spin_lock(&hdd_context_lock);
		context->response_status = 0;
		complete(&context->response_event);
		spin_unlock(&hdd_context_lock);
	}

	return;

fail:
	if (skb)
		kfree_skb(skb);

	spin_lock(&hdd_context_lock);
	context->response_status = -EINVAL;
	spin_unlock(&hdd_context_lock);
	return;
}

/**
 * wlan_hdd_cfg80211_extscan_hotlist_match_ind() - hotlist match callback
 * @hddctx: HDD context
 * @data: event data
 *
 * This function reads the hotlist matched event %data and fill in the skb with
 * NL attributes and send up the NL event.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_hotlist_match_ind(void *ctx,
                                            struct extscan_hotlist_match *data)
{
	hdd_context_t *pHddCtx = ctx;
	struct sk_buff *skb    = NULL;
	uint32_t i, index;
	int flags = vos_get_gfp_flags();

	ENTER();

	if (wlan_hdd_validate_context(pHddCtx) || !data) {
		hddLog(LOGE, FL("HDD context is invalid or data(%p) is null"),
				data);
		return;
	}

	if (data->ap_found)
		index = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_FOUND_INDEX;
	else
		index = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_LOST_INDEX;

	skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
					EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
					index, flags);

	if (!skb) {
		hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
		return;
	}
	hddLog(LOG1, "Req Id: %u Num_APs: %u MoreData: %u ap_found: %u",
			data->requestId, data->numOfAps, data->moreData,
			data->ap_found);

	for (i = 0; i < data->numOfAps; i++) {
		data->ap[i].ts = vos_get_monotonic_boottime();

		hddLog(LOG1, "[i=%d] Timestamp %llu "
				"Ssid: %s "
				"Bssid (" MAC_ADDRESS_STR ") "
				"Channel %u "
				"Rssi %d "
				"RTT %u "
				"RTT_SD %u",
				i,
				data->ap[i].ts,
				data->ap[i].ssid,
				MAC_ADDR_ARRAY(data->ap[i].bssid),
				data->ap[i].channel,
				data->ap[i].rssi,
				data->ap[i].rtt,
				data->ap[i].rtt_sd);
	}

	if (nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->requestId) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_NUM_RESULTS_AVAILABLE,
		data->numOfAps)) {
		hddLog(LOGE, FL("put fail"));
		goto fail;
	}

	if (data->numOfAps) {
		struct nlattr *aps;

		aps = nla_nest_start(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
		if (!aps)
			goto fail;

		for (i = 0; i < data->numOfAps; i++) {
			struct nlattr *ap;

			ap = nla_nest_start(skb, i);
			if (!ap)
				goto fail;

			if (nla_put_u64(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_TIME_STAMP,
				data->ap[i].ts) ||
			    nla_put(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_SSID,
				sizeof(data->ap[i].ssid),
				data->ap[i].ssid) ||
			    nla_put(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BSSID,
				sizeof(data->ap[i].bssid),
				data->ap[i].bssid) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CHANNEL,
			        data->ap[i].channel) ||
			    nla_put_s32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RSSI,
				data->ap[i].rssi) ||
			    nla_put_u32(skb,
			        QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT,
				data->ap[i].rtt) ||
			    nla_put_u32(skb,
			        QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT_SD,
				data->ap[i].rtt_sd))
				goto fail;

			nla_nest_end(skb, ap);
		}
		nla_nest_end(skb, aps);

		if (nla_put_u8(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
			data->moreData))
			goto fail;
	}

	cfg80211_vendor_event(skb, flags);
	return;

fail:
	kfree_skb(skb);
	return;
}

/**
 * wlan_hdd_cfg80211_extscan_generic_rsp() -
 *	Handle a generic ExtScan Response message
 * @ctx: HDD context registered with SME
 * @response: The ExtScan response from firmware
 *
 * This function will handle a generic ExtScan response message from
 * firmware and will communicate the result to the userspace thread
 * that is waiting for the response.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_generic_rsp
	(void *ctx,
	 struct sir_extscan_generic_response *response)
{
	hdd_context_t *hdd_ctx = ctx;
	struct hdd_ext_scan_context *context;

	ENTER();

	if (wlan_hdd_validate_context(hdd_ctx) || !response) {
		hddLog(LOGE,
		       FL("HDD context is not valid or response(%p) is null"),
		       response);
		return;
	}

	hddLog(LOG1, FL("request %u status %d"),
	       response->request_id, response->status);

	context = &hdd_ctx->ext_scan_context;
	spin_lock(&hdd_context_lock);
	if (context->request_id == response->request_id) {
		context->response_status = response->status ? -EINVAL : 0;
		complete(&context->response_event);
	}
	spin_unlock(&hdd_context_lock);

	return;
}

/**
 * wlan_hdd_cfg80211_extscan_hotlist_ssid_match_ind() -
 *	Handle an SSID hotlist match event
 * @ctx: HDD context registered with SME
 * @event: The SSID hotlist match event
 *
 * This function will take an SSID match event that was generated by
 * firmware and will convert it into a cfg80211 vendor event which is
 * sent to userspace.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_hotlist_ssid_match_ind(void *ctx,
						 tpSirWifiScanResultEvent event)
{
	hdd_context_t *hdd_ctx = ctx;
	struct sk_buff *skb;
	unsigned i;
	unsigned index;
	int flags = vos_get_gfp_flags();

	ENTER();

	if (wlan_hdd_validate_context(hdd_ctx) || !event) {
		hddLog(LOGE,
		       FL("HDD context is not valid or event(%p) is null"),
		       event);
		return;
	}

	if (event->ap_found) {
		index = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_SSID_FOUND_INDEX;
		hddLog(LOG1, "SSID hotlist found");
	} else {
		index = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_SSID_LOST_INDEX;
		hddLog(LOG1, "SSID hotlist lost");
	}

	skb = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
		EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
		index, flags);

	if (!skb) {
		hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
		return;
	}
	hddLog(LOG1, "Req Id %u, Num results %u, More Data (%u)",
	       event->requestId, event->numOfAps, event->moreData);

	for (i = 0; i < event->numOfAps; i++) {
		hddLog(LOG1, "[i=%d] Timestamp %llu "
		       "Ssid: %s "
		       "Bssid (" MAC_ADDRESS_STR ") "
		       "Channel %u "
		       "Rssi %d "
		       "RTT %u "
		       "RTT_SD %u",
		       i,
		       event->ap[i].ts,
		       event->ap[i].ssid,
		       MAC_ADDR_ARRAY(event->ap[i].bssid),
		       event->ap[i].channel,
		       event->ap[i].rssi,
		       event->ap[i].rtt,
		       event->ap[i].rtt_sd);
	}

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
			event->requestId) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_NUM_RESULTS_AVAILABLE,
			event->numOfAps)) {
		hddLog(LOGE, FL("put fail"));
		goto fail;
	}

	if (event->numOfAps) {
		struct nlattr *aps;
		aps = nla_nest_start(skb,
				     QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
		if (!aps) {
			hddLog(LOGE, FL("nest fail"));
			goto fail;
		}

		for (i = 0; i < event->numOfAps; i++) {
			struct nlattr *ap;

			ap = nla_nest_start(skb, i);
			if (!ap) {
				hddLog(LOGE, FL("nest fail"));
				goto fail;
			}

			if (nla_put_u64(skb,
					QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_TIME_STAMP,
					event->ap[i].ts) ||
			    nla_put(skb,
				    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_SSID,
				    sizeof(event->ap[i].ssid),
				    event->ap[i].ssid) ||
			    nla_put(skb,
				    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BSSID,
				    sizeof(event->ap[i].bssid),
				    event->ap[i].bssid) ||
			    nla_put_u32(skb,
					QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CHANNEL,
					event->ap[i].channel) ||
			    nla_put_s32(skb,
					QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RSSI,
					event->ap[i].rssi) ||
			    nla_put_u32(skb,
					QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT,
					event->ap[i].rtt) ||
			    nla_put_u32(skb,
					QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT_SD,
					event->ap[i].rtt_sd)) {
				hddLog(LOGE, FL("put fail"));
				goto fail;
			}
			nla_nest_end(skb, ap);
		}
		nla_nest_end(skb, aps);

		if (nla_put_u8(skb,
			       QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
			       event->moreData)) {
			hddLog(LOGE, FL("put fail"));
			goto fail;
		}
	}

	cfg80211_vendor_event(skb, flags);
	return;

fail:
	kfree_skb(skb);
	return;
}

/**
 * wlan_hdd_cfg80211_extscan_signif_wifi_change_results_ind() - results callback
 * @hddctx: HDD context
 * @data: event data
 *
 * This function reads the event %data and fill in the skb with
 * NL attributes and send up the NL event.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_signif_wifi_change_results_ind(
                                          void *ctx,
                                          tpSirWifiSignificantChangeEvent pData)
{
    hdd_context_t  *pHddCtx = (hdd_context_t *)ctx;
    struct sk_buff *skb     = NULL;
    tSirWifiSignificantChange *ap_info;
    tANI_S32                  *rssi;
    tANI_U32 i, j;
    int flags = vos_get_gfp_flags();

    ENTER();

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(LOGE, FL("HDD context is invalid or pData(%p) is null"),	pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                    EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                    QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SIGNIFICANT_CHANGE_INDEX,
                    flags);

    if (!skb) {
        hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(LOG1, "Req Id %u Num results %u More Data %u", pData->requestId,
           pData->numResults, pData->moreData);

    ap_info = &pData->ap[0];
    for (i = 0; i < pData->numResults; i++) {
        hddLog(LOG1, "[i=%d] "
                     "Bssid (" MAC_ADDRESS_STR ") "
                     "Channel %u "
                     "numOfRssi %d",
                     i,
                     MAC_ADDR_ARRAY(ap_info->bssid),
                     ap_info->channel,
                     ap_info->numOfRssi);
        rssi = &(ap_info)->rssi[0];
        for (j = 0; j < ap_info->numOfRssi; j++)
            hddLog(LOG1, "Rssi %d", *rssi++);

        ap_info += ap_info->numOfRssi * sizeof(*rssi);
    }

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                         pData->requestId) ||
        nla_put_u32(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_NUM_RESULTS_AVAILABLE,
                    pData->numResults)) {
        hddLog(LOGE, FL("put fail"));
        goto fail;
    }

    if (pData->numResults) {
        struct nlattr *aps;

        aps = nla_nest_start(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
        if (!aps)
            goto fail;

        ap_info = &pData->ap[0];
        for (i = 0; i < pData->numResults; i++) {
            struct nlattr *ap;

            ap = nla_nest_start(skb, i);
            if (!ap)
                goto fail;

            if (nla_put(skb,
                  QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_BSSID,
                sizeof(tSirMacAddr), ap_info->bssid) ||
                nla_put_u32(skb,
                  QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_CHANNEL,
                ap_info->channel) ||
                nla_put_u32(skb,
                  QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_NUM_RSSI,
                ap_info->numOfRssi) ||
                nla_put(skb,
                  QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_RSSI_LIST,
                  sizeof(s32) * ap_info->numOfRssi, &(ap_info)->rssi[0]))
                goto fail;

              nla_nest_end(skb, ap);

            ap_info += ap_info->numOfRssi * sizeof(*rssi);
        }
        nla_nest_end(skb, aps);

        if (nla_put_u8(skb,
                      QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
                      pData->moreData))
            goto fail;
    }

    cfg80211_vendor_event(skb, flags);
    return;

fail:
    kfree_skb(skb);
    return;

}

/**
 * wlan_hdd_cfg80211_extscan_full_scan_result_event() - full scan results event
 * @hddctx: HDD context
 * @data: event data
 *
 * This function reads the event %data and fill in the skb with
 * NL attributes and send up the NL event.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_full_scan_result_event(void *ctx,
					tpSirWifiFullScanResultEvent pData)
{
	hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
	struct sk_buff *skb     = NULL;
#ifdef CONFIG_CNSS
	struct timespec ts;
#endif
	int flags = vos_get_gfp_flags();

	ENTER();

	if (wlan_hdd_validate_context(pHddCtx) || !pData) {
		hddLog(LOGE, FL("HDD context is invalid or pData(%p) is null"),
			pData);
		return;
	}

	/*
	 * If the full scan result including IE data exceeds NL 4K size
	 * limitation, drop that beacon/probe rsp frame.
	 */
	if ((sizeof(*pData) + pData->ap.ieLength) >= EXTSCAN_EVENT_BUF_SIZE) {
		hddLog(LOGE, FL("Frame exceeded NL size limilation, drop it!"));
		return;
	}

	skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
		  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
		  QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_FULL_SCAN_RESULT_INDEX,
		  flags);

	if (!skb) {
		hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
		return;
	}

	pData->ap.channel = vos_chan_to_freq(pData->ap.channel);
#ifdef CONFIG_CNSS
	/* Android does not want the time stamp from the frame.
	   Instead it wants a monotonic increasing value since boot */
	cnss_get_monotonic_boottime(&ts);
	pData->ap.ts = ((u64)ts.tv_sec * 1000000) + (ts.tv_nsec / 1000);
#endif
	hddLog(LOG1, "Req Id %u More Data %u",
		pData->requestId, pData->moreData);
	hddLog(LOG1, "AP Info: Timestamp %llu Ssid: %s "
				"Bssid (" MAC_ADDRESS_STR ") "
				"Channel %u "
				"Rssi %d "
				"RTT %u "
				"RTT_SD %u "
				"Bcn Period %d "
				"Capability 0x%X "
				"IE Length %d",
				pData->ap.ts,
				pData->ap.ssid,
				MAC_ADDR_ARRAY(pData->ap.bssid),
				pData->ap.channel,
				pData->ap.rssi,
				pData->ap.rtt,
				pData->ap.rtt_sd,
				pData->ap.beaconPeriod,
				pData->ap.capability,
				pData->ap.ieLength);

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		pData->requestId) ||
	    nla_put_u64(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_TIME_STAMP,
		pData->ap.ts) ||
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_SSID,
		sizeof(pData->ap.ssid),
		pData->ap.ssid) ||
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BSSID,
		sizeof(pData->ap.bssid),
		pData->ap.bssid) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CHANNEL,
		pData->ap.channel) ||
	    nla_put_s32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RSSI,
		pData->ap.rssi) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT,
		pData->ap.rtt) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT_SD,
		pData->ap.rtt_sd) ||
	    nla_put_u16(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BEACON_PERIOD,
		pData->ap.beaconPeriod) ||
	    nla_put_u16(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CAPABILITY,
		pData->ap.capability) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_IE_LENGTH,
		pData->ap.ieLength) ||
	    nla_put_u8(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
		pData->moreData)) {
		hddLog(LOGE, FL("nla put fail"));
		goto nla_put_failure;
	}

	if (pData->ap.ieLength) {
		if (nla_put(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_IE_DATA,
			pData->ap.ieLength, pData->ap.ieData))
		goto nla_put_failure;
	}

	cfg80211_vendor_event(skb, flags);
	return;

nla_put_failure:
	kfree_skb(skb);
	return;
}

/**
 * wlan_hdd_cfg80211_extscan_epno_match_found() - pno match found
 * @hddctx: HDD context
 * @data: matched network data
 *
 * This function reads the matched network data and fills NL vendor attributes
 * and send it to upper layer.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: 0 on success, error number otherwise
 */
static void
wlan_hdd_cfg80211_extscan_epno_match_found(void *ctx,
					struct pno_match_found *data)
{
	hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
	struct sk_buff *skb     = NULL;
	uint32_t len, i;
	int flags = vos_get_gfp_flags();

	ENTER();

	if (wlan_hdd_validate_context(pHddCtx) || !data) {
		hddLog(LOGE, FL("HDD context is invalid or data(%p) is null"),
			data);
		return;
	}

	/*
	 * If the number of match found APs including IE data exceeds NL 4K size
	 * limitation, drop that beacon/probe rsp frame.
	 */
	len = sizeof(*data) +
			(data->num_results + sizeof(tSirWifiScanResult));
	for (i = 0; i < data->num_results; i++) {
		len += data->ap[i].ieLength;
	}
	if (len >= EXTSCAN_EVENT_BUF_SIZE) {
		hddLog(LOGE, FL("Frame exceeded NL size limitation, drop it!"));
		return;
}

	skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
		  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
		  QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_NETWORK_FOUND_INDEX,
		  flags);

	if (!skb) {
		hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
		return;
	}

	hddLog(LOG1, "Req Id %u More Data %u num_results %d",
		data->request_id, data->more_data, data->num_results);
	for (i = 0; i < data->num_results; i++) {
		data->ap[i].channel = vos_chan_to_freq(data->ap[i].channel);
		hddLog(LOG1, "AP Info: Timestamp %llu Ssid: %s "
					"Bssid (" MAC_ADDRESS_STR ") "
					"Channel %u "
					"Rssi %d "
					"RTT %u "
					"RTT_SD %u "
					"Bcn Period %d "
					"Capability 0x%X "
					"IE Length %d",
					data->ap[i].ts,
					data->ap[i].ssid,
					MAC_ADDR_ARRAY(data->ap[i].bssid),
					data->ap[i].channel,
					data->ap[i].rssi,
					data->ap[i].rtt,
					data->ap[i].rtt_sd,
					data->ap[i].beaconPeriod,
					data->ap[i].capability,
					data->ap[i].ieLength);
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->request_id) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_NUM_RESULTS_AVAILABLE,
		data->num_results) ||
	    nla_put_u8(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
		data->more_data)) {
		hddLog(LOGE, FL("nla put fail"));
		goto fail;
	}

	if (data->num_results) {
		struct nlattr *nla_aps;
		nla_aps = nla_nest_start(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
		if (!nla_aps)
			goto fail;

		for (i = 0; i < data->num_results; i++) {
			if (hdd_extscan_nl_fill_bss(skb, &data->ap[i], i))
				goto fail;
		}
		nla_nest_end(skb, nla_aps);
	}

	cfg80211_vendor_event(skb, flags);
	return;

fail:
	kfree_skb(skb);
	return;
}

/**
 * wlan_hdd_cfg80211_extscan_scan_res_available_event() - scan available event
 * @hddctx: HDD context
 * @data: event data
 *
 * This function reads the event %data and fill in the skb with
 * NL attributes and send up the NL event.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_scan_res_available_event(void *ctx,
                                    tpSirExtScanResultsAvailableIndParams pData)
{
    hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
    struct sk_buff *skb     = NULL;
    int flags = vos_get_gfp_flags();

    ENTER();

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(LOGE, FL("HDD context is invalid or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_RESULTS_AVAILABLE_INDEX,
                flags);

    if (!skb) {
        hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
        return;
    }

    hddLog(LOG1, "Req Id %u Num results %u", pData->requestId,
           pData->numResultsAvailable);
    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                    pData->requestId) ||
        nla_put_u32(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_NUM_RESULTS_AVAILABLE,
                    pData->numResultsAvailable)) {
        hddLog(LOGE, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, flags);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}

/**
 * wlan_hdd_cfg80211_extscan_scan_progress_event() - scan progress event
 * @hddctx: HDD context
 * @data: event data
 *
 * This function reads the event %data and fill in the skb with
 * NL attributes and send up the NL event.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_scan_progress_event(void *ctx,
                                         tpSirExtScanOnScanEventIndParams pData)
{
    hdd_context_t *pHddCtx  = (hdd_context_t *)ctx;
    struct sk_buff *skb     = NULL;
    int flags = vos_get_gfp_flags();

    ENTER();

    if (wlan_hdd_validate_context(pHddCtx) || !pData) {
        hddLog(LOGE, FL("HDD context is invalid or pData(%p) is null"), pData);
        return;
    }

    skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
                            EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
                            QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_EVENT_INDEX,
                            flags);

    if (!skb) {
        hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
        return;
    }
    hddLog(LOG1, "Request Id %u Scan event type %u Scan event status %u",
           pData->requestId, pData->scanEventType, pData->status);

    if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
                    pData->requestId) ||
        nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_EVENT_TYPE,
                   pData->scanEventType) ||
        nla_put_u32(skb,
                    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_EVENT_STATUS,
                    pData->status)) {
        hddLog(LOGE, FL("nla put fail"));
        goto nla_put_failure;
    }

    cfg80211_vendor_event(skb, flags);
    return;

nla_put_failure:
    kfree_skb(skb);
    return;
}

/**
 * wlan_hdd_cfg80211_passpoint_match_found() - passpoint match found
 * @hddctx: HDD context
 * @data: matched network data
 *
 * This function reads the match network %data and fill in the skb with
 * NL attributes and send up the NL event
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_passpoint_match_found(void *ctx,
					struct wifi_passpoint_match *data)
{
	hdd_context_t *pHddCtx  = ctx;
	struct sk_buff *skb     = NULL;
	uint32_t len, i, num_matches = 1, more_data = 0;
	struct nlattr *nla_aps;
	struct nlattr *nla_bss;
	int flags = vos_get_gfp_flags();

	ENTER();

	if (wlan_hdd_validate_context(pHddCtx) || !data) {
		hddLog(LOGE, FL("HDD context is invalid or data(%p) is null"),
			data);
		return;
	}

	len = sizeof(*data) + data->ap.ieLength + data->anqp_len;
	if (len >= EXTSCAN_EVENT_BUF_SIZE) {
		hddLog(LOGE, FL("Result exceeded NL size limitation, drop it"));
		return;
	}

	skb = cfg80211_vendor_event_alloc(pHddCtx->wiphy,
		  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
		  QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_PASSPOINT_NETWORK_FOUND_INDEX,
		  flags);

	if (!skb) {
		hddLog(LOGE, FL("cfg80211_vendor_event_alloc failed"));
		return;
	}

	hddLog(LOG1, "Req Id %u Id %u ANQP length %u num_matches %u",
		data->request_id, data->id, data->anqp_len, num_matches);
	for (i = 0; i < num_matches; i++) {
		hddLog(LOG1, "AP Info: Timestamp %llu Ssid: %s "
					"Bssid (" MAC_ADDRESS_STR ") "
					"Channel %u "
					"Rssi %d "
					"RTT %u "
					"RTT_SD %u "
					"Bcn Period %d "
					"Capability 0x%X "
					"IE Length %d",
					data->ap.ts,
					data->ap.ssid,
					MAC_ADDR_ARRAY(data->ap.bssid),
					data->ap.channel,
					data->ap.rssi,
					data->ap.rtt,
					data->ap.rtt_sd,
					data->ap.beaconPeriod,
					data->ap.capability,
					data->ap.ieLength);
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->request_id) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_PNO_RESULTS_PASSPOINT_NETWORK_FOUND_NUM_MATCHES,
		num_matches) ||
	    nla_put_u8(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
		more_data)) {
		hddLog(LOGE, FL("nla put fail"));
		goto fail;
	}

	nla_aps = nla_nest_start(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_PNO_RESULTS_PASSPOINT_MATCH_RESULT_LIST);
	if (!nla_aps)
		goto fail;

	for (i = 0; i < num_matches; i++) {
		struct nlattr *nla_ap;

		nla_ap = nla_nest_start(skb, i);
		if (!nla_ap)
			goto fail;

		if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_PNO_RESULTS_PASSPOINT_MATCH_ID,
			data->id) ||
		    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_PNO_RESULTS_PASSPOINT_MATCH_ANQP_LEN,
			data->anqp_len)) {
			goto fail;
		}

		if (data->anqp_len)
			if (nla_put(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_PNO_RESULTS_PASSPOINT_MATCH_ANQP,
				data->anqp_len, data->anqp))
				goto fail;

		nla_bss = nla_nest_start(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
		if (!nla_bss)
			goto fail;

		if (hdd_extscan_nl_fill_bss(skb, &data->ap, 0))
				goto fail;
		nla_nest_end(skb, nla_bss);
		nla_nest_end(skb, nla_ap);
	}
	nla_nest_end(skb, nla_aps);

	cfg80211_vendor_event(skb, flags);
	return;

fail:
	kfree_skb(skb);
	return;
}

void wlan_hdd_cfg80211_extscan_callback(void *ctx, const tANI_U16 evType,
                                        void *pMsg)
{
    hdd_context_t *pHddCtx = (hdd_context_t *)ctx;

    if (wlan_hdd_validate_context(pHddCtx)) {
        hddLog(LOGE, FL("HDD context is invalid; Received event %d"), evType);
        return;
    }

    hddLog(LOG1, FL("Rcvd Event %d"), evType);

    switch (evType) {
    case eSIR_EXTSCAN_CACHED_RESULTS_RSP:
             /* There is no need to send this response to upper layer
                Just log the message */
             hddLog(LOG2, FL("Rcvd eSIR_EXTSCAN_CACHED_RESULTS_RSP"));
            break;

    case eSIR_EXTSCAN_GET_CAPABILITIES_IND:
            wlan_hdd_cfg80211_extscan_get_capabilities_rsp(ctx,
                    (struct ext_scan_capabilities_response *)pMsg);
            break;

    case eSIR_EXTSCAN_HOTLIST_MATCH_IND:
            wlan_hdd_cfg80211_extscan_hotlist_match_ind(ctx, pMsg);
            break;

    case eSIR_EXTSCAN_SIGNIFICANT_WIFI_CHANGE_RESULTS_IND:
            wlan_hdd_cfg80211_extscan_signif_wifi_change_results_ind(
                                         ctx,
                                         (tpSirWifiSignificantChangeEvent)pMsg);
            break;

    case eSIR_EXTSCAN_CACHED_RESULTS_IND:
            wlan_hdd_cfg80211_extscan_cached_results_ind(ctx, pMsg);
            break;

    case eSIR_EXTSCAN_SCAN_RES_AVAILABLE_IND:
            wlan_hdd_cfg80211_extscan_scan_res_available_event(ctx,
                                   (tpSirExtScanResultsAvailableIndParams)pMsg);
            break;

    case eSIR_EXTSCAN_FULL_SCAN_RESULT_IND:
            wlan_hdd_cfg80211_extscan_full_scan_result_event(ctx,
                                   (tpSirWifiFullScanResultEvent)pMsg);
            break;

    case eSIR_EPNO_NETWORK_FOUND_IND:
            wlan_hdd_cfg80211_extscan_epno_match_found(ctx,
                                   (struct pno_match_found *)pMsg);
            break;

    case eSIR_EXTSCAN_SCAN_PROGRESS_EVENT_IND:
            wlan_hdd_cfg80211_extscan_scan_progress_event(ctx,
                                   (tpSirExtScanOnScanEventIndParams)pMsg);
            break;

    case eSIR_PASSPOINT_NETWORK_FOUND_IND:
            wlan_hdd_cfg80211_passpoint_match_found(ctx,
                                    (struct wifi_passpoint_match *) pMsg);
            break;

    case eSIR_EXTSCAN_START_RSP:
    case eSIR_EXTSCAN_STOP_RSP:
    case eSIR_EXTSCAN_SET_BSSID_HOTLIST_RSP:
    case eSIR_EXTSCAN_RESET_BSSID_HOTLIST_RSP:
    case eSIR_EXTSCAN_SET_SIGNIFICANT_WIFI_CHANGE_RSP:
    case eSIR_EXTSCAN_RESET_SIGNIFICANT_WIFI_CHANGE_RSP:
    case eSIR_EXTSCAN_SET_SSID_HOTLIST_RSP:
    case eSIR_EXTSCAN_RESET_SSID_HOTLIST_RSP:
	    wlan_hdd_cfg80211_extscan_generic_rsp(ctx, pMsg);
	    break;

    case eSIR_EXTSCAN_HOTLIST_SSID_MATCH_IND:
            wlan_hdd_cfg80211_extscan_hotlist_ssid_match_ind(ctx,
                                    (tpSirWifiScanResultEvent)pMsg);
            break;

    default:
            hddLog(LOGE, FL("Unknown event type %u"), evType);
            break;
    }
}

#endif /* FEATURE_WLAN_EXTSCAN */

/* cfg80211_ops */
static struct cfg80211_ops wlan_hdd_cfg80211_ops =
{
    .add_virtual_intf = wlan_hdd_add_virtual_intf,
    .del_virtual_intf = wlan_hdd_del_virtual_intf,
    .change_virtual_intf = wlan_hdd_cfg80211_change_iface,
    .change_station = wlan_hdd_change_station,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,4,0))
    .add_beacon = wlan_hdd_cfg80211_add_beacon,
    .del_beacon = wlan_hdd_cfg80211_del_beacon,
    .set_beacon = wlan_hdd_cfg80211_set_beacon,
#else
    .start_ap = wlan_hdd_cfg80211_start_ap,
    .change_beacon = wlan_hdd_cfg80211_change_beacon,
    .stop_ap = wlan_hdd_cfg80211_stop_ap,
#endif
    .change_bss = wlan_hdd_cfg80211_change_bss,
    .add_key = wlan_hdd_cfg80211_add_key,
    .get_key = wlan_hdd_cfg80211_get_key,
    .del_key = wlan_hdd_cfg80211_del_key,
    .set_default_key = wlan_hdd_cfg80211_set_default_key,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
    .set_channel = wlan_hdd_cfg80211_set_channel,
#endif
    .scan = wlan_hdd_cfg80211_scan,
    .connect = wlan_hdd_cfg80211_connect,
    .disconnect = wlan_hdd_cfg80211_disconnect,
    .join_ibss  = wlan_hdd_cfg80211_join_ibss,
    .leave_ibss = wlan_hdd_cfg80211_leave_ibss,
    .set_wiphy_params = wlan_hdd_cfg80211_set_wiphy_params,
    .set_tx_power = wlan_hdd_cfg80211_set_txpower,
    .get_tx_power = wlan_hdd_cfg80211_get_txpower,
    .remain_on_channel = wlan_hdd_cfg80211_remain_on_channel,
    .cancel_remain_on_channel =  wlan_hdd_cfg80211_cancel_remain_on_channel,
    .mgmt_tx = wlan_hdd_mgmt_tx,
     .mgmt_tx_cancel_wait = wlan_hdd_cfg80211_mgmt_tx_cancel_wait,
     .set_default_mgmt_key = wlan_hdd_set_default_mgmt_key,
     .set_txq_params = wlan_hdd_set_txq_params,
     .get_station = wlan_hdd_cfg80211_get_station,
     .set_power_mgmt = wlan_hdd_cfg80211_set_power_mgmt,
     .del_station  = wlan_hdd_cfg80211_del_station,
     .add_station  = wlan_hdd_cfg80211_add_station,
#ifdef FEATURE_WLAN_LFR
     .set_pmksa = wlan_hdd_cfg80211_set_pmksa,
     .del_pmksa = wlan_hdd_cfg80211_del_pmksa,
     .flush_pmksa = wlan_hdd_cfg80211_flush_pmksa,
#endif
#if defined(WLAN_FEATURE_VOWIFI_11R) && defined(KERNEL_SUPPORT_11R_CFG80211)
     .update_ft_ies = wlan_hdd_cfg80211_update_ft_ies,
#endif
#ifdef FEATURE_WLAN_TDLS
     .tdls_mgmt = wlan_hdd_cfg80211_tdls_mgmt,
     .tdls_oper = wlan_hdd_cfg80211_tdls_oper,
#endif
#ifdef WLAN_FEATURE_GTK_OFFLOAD
     .set_rekey_data = wlan_hdd_cfg80211_set_rekey_data,
#endif /* WLAN_FEATURE_GTK_OFFLOAD */
#ifdef FEATURE_WLAN_SCAN_PNO
     .sched_scan_start = wlan_hdd_cfg80211_sched_scan_start,
     .sched_scan_stop = wlan_hdd_cfg80211_sched_scan_stop,
#endif /*FEATURE_WLAN_SCAN_PNO */
     .resume = wlan_hdd_cfg80211_resume_wlan,
     .suspend = wlan_hdd_cfg80211_suspend_wlan,
     .set_mac_acl = wlan_hdd_cfg80211_set_mac_acl,
#ifdef WLAN_NL80211_TESTMODE
     .testmode_cmd = wlan_hdd_cfg80211_testmode,
#endif
#ifdef QCA_HT_2040_COEX
     .set_ap_chanwidth = wlan_hdd_cfg80211_set_ap_channel_width,
#endif
     .dump_survey = wlan_hdd_cfg80211_dump_survey,
#ifdef NL80211_KEY_LEN_PMK /* kernel supports key mgmt offload */
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
     .key_mgmt_set_pmk = wlan_hdd_cfg80211_key_mgmt_set_pmk,
#endif
#endif
};
