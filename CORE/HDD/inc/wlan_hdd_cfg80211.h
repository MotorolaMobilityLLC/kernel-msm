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

#if !defined( HDD_CFG80211_H__ )
#define HDD_CFG80211_H__


/**===========================================================================

  \file  wlan_hdd_cfg80211.h

  \brief cfg80211 functions declarations

  ==========================================================================*/

/* $HEADER$ */


//value for initial part of frames and number of bytes to be compared
#define GAS_INITIAL_REQ "\x04\x0a"
#define GAS_INITIAL_REQ_SIZE 2

#define GAS_INITIAL_RSP "\x04\x0b"
#define GAS_INITIAL_RSP_SIZE 2

#define GAS_COMEBACK_REQ "\x04\x0c"
#define GAS_COMEBACK_REQ_SIZE 2

#define GAS_COMEBACK_RSP "\x04\x0d"
#define GAS_COMEBACK_RSP_SIZE 2

#define P2P_PUBLIC_ACTION_FRAME "\x04\x09\x50\x6f\x9a\x09"
#define P2P_PUBLIC_ACTION_FRAME_SIZE 6

#define P2P_ACTION_FRAME "\x7f\x50\x6f\x9a\x09"
#define P2P_ACTION_FRAME_SIZE 5

#define SA_QUERY_FRAME_REQ "\x08\x00"
#define SA_QUERY_FRAME_REQ_SIZE 2

#define SA_QUERY_FRAME_RSP "\x08\x01"
#define SA_QUERY_FRAME_RSP_SIZE 2

#define HDD_P2P_WILDCARD_SSID "DIRECT-" //TODO Put it in proper place;
#define HDD_P2P_WILDCARD_SSID_LEN 7

#define WNM_BSS_ACTION_FRAME "\x0a\x07"
#define WNM_BSS_ACTION_FRAME_SIZE 2

#define WNM_NOTIFICATION_FRAME "\x0a\x1a"
#define WNM_NOTIFICATION_FRAME_SIZE 2

#define WPA_OUI_TYPE   "\x00\x50\xf2\x01"
#define BLACKLIST_OUI_TYPE   "\x00\x50\x00\x00"
#define WHITELIST_OUI_TYPE   "\x00\x50\x00\x01"
#define WPA_OUI_TYPE_SIZE  4

#define WLAN_BSS_MEMBERSHIP_SELECTOR_HT_PHY 127
#define BASIC_RATE_MASK   0x80
#define RATE_MASK         0x7f

#ifdef WLAN_ENABLE_AGEIE_ON_SCAN_RESULTS
/* GPS application requirement */
#define QCOM_VENDOR_IE_ID 221
#define QCOM_OUI1         0x00
#define QCOM_OUI2         0xA0
#define QCOM_OUI3         0xC6
#define QCOM_VENDOR_IE_AGE_TYPE  0x100
#define QCOM_VENDOR_IE_AGE_LEN   11

#ifdef FEATURE_WLAN_TDLS
#define WLAN_IS_TDLS_SETUP_ACTION(action) \
         ((SIR_MAC_TDLS_SETUP_REQ <= action) && (SIR_MAC_TDLS_SETUP_CNF >= action))
#if !defined (TDLS_MGMT_VERSION2)
#define TDLS_MGMT_VERSION2 0
#endif
#endif

#define MAX_CHANNEL MAX_2_4GHZ_CHANNEL + NUM_5GHZ_CHANNELS

typedef struct {
   u8 element_id;
   u8 len;
   u8 oui_1;
   u8 oui_2;
   u8 oui_3;
   u32 type;
   u32 age;
}__attribute__((packed)) qcom_ie_age ;
#endif

enum qca_nl80211_vendor_subcmds {
    QCA_NL80211_VENDOR_SUBCMD_UNSPEC = 0,
    QCA_NL80211_VENDOR_SUBCMD_TEST = 1,
    /* subcmds 2..9 not yet allocated */
    QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY = 10,
    QCA_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY =  11,
    QCA_NL80211_VENDOR_SUBCMD_NAN =  12,
    QCA_NL80211_VENDOR_SUBCMD_STATS_EXT = 13,

    QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET = 14,
    QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET = 15,
    QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR = 16,
    QCA_NL80211_VENDOR_SUBCMD_LL_STATS_RADIO_RESULTS = 17,
    QCA_NL80211_VENDOR_SUBCMD_LL_STATS_IFACE_RESULTS = 18,
    QCA_NL80211_VENDOR_SUBCMD_LL_STATS_PEERS_RESULTS = 19,
};

enum qca_nl80211_vendor_subcmds_index {
    QCA_NL80211_VENDOR_SUBCMD_NAN_INDEX = 0,
    QCA_NL80211_VENDOR_SUBCMD_LL_STATS_SET_INDEX,
    QCA_NL80211_VENDOR_SUBCMD_LL_STATS_GET_INDEX,
    QCA_NL80211_VENDOR_SUBCMD_LL_STATS_CLR_INDEX,
    QCA_NL80211_VENDOR_SUBCMD_LL_RADIO_STATS_INDEX,
    QCA_NL80211_VENDOR_SUBCMD_LL_IFACE_STATS_INDEX,
    QCA_NL80211_VENDOR_SUBCMD_LL_PEER_INFO_STATS_INDEX,
};

enum qca_wlan_vendor_attr
{
    QCA_WLAN_VENDOR_ATTR_INVALID = 0,
    /* used by QCOM_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY */
    QCA_WLAN_VENDOR_ATTR_DFS     = 1,
    /* used by QCOM_NL80211_VENDOR_SUBCMD_NAN */
    QCA_WLAN_VENDOR_ATTR_NAN     = 2,

    /* keep last */
    QCA_WLAN_VENDOR_ATTR_AFTER_LAST,
    QCA_WLAN_VENDOR_ATTR_MAX       = QCA_WLAN_VENDOR_ATTR_AFTER_LAST - 1,
};

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

enum qca_wlan_vendor_attr_ll_stats_set
{
    QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_INVALID = 0,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD = 1,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING,
    /* keep last */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_AFTER_LAST,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX =
                    QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_ll_stats_get
{
    QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_INVALID = 0,
    /* Unsigned 32bit value provided by the caller issuing the GET stats
     * command. When reporting the stats results, the driver uses the same
     * value to indicate which GET request the results correspond to.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID,
    /* Unsigned 34bit value - bit mask to identify what
     * statistics are requested for retrieval.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK,
    /* keep last */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_AFTER_LAST,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX =
                  QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_ll_stats_clr
{
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_INVALID = 0,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP,

    QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_AFTER_LAST,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX       =
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_AFTER_LAST - 1,
};

enum qca_wlan_vendor_attr_ll_stats_results
{
    QCA_WLAN_VENDOR_ATTR_LL_STATS_INVALID = 0,
    /* Unsigned 32bit value */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RESULTS_REQ_ID = 1,
    /* Unsigned 32bit value */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX,
    /* Unsigned 32bit value */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX,
    /* Unsigned 32bit value */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX,
    /* Unsigned 32bit value */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX,
    /* Unsigned 32bit value */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT,
    /* Unsigned 32bit value */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA,
    /* Unsigned 32bit value */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK,
    /* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_* are
     * nested within the interface stats.
     */

    /* Interface mode, e.g., STA, SOFTAP, IBSS, etc.
     * Type = enum wifi_interface_mode */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MODE,
    /* Interface MAC address. An array of 6 Unsigned int8 */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MAC_ADDR,
    /* Type = enum wifi_connection_state,
     * e.g., DISCONNECTED, AUTHENTICATING, etc.
     * valid for STA, CLI only.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_STATE,
    /* Type = enum wifi_roam_state. Roaming state,
     * e.g., IDLE or ACTIVE (is that valid for STA only?)
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_ROAMING,
    /* Unsigned 32bit value. WIFI_CAPABILITY_XXX */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_CAPABILITIES,
    /* NULL terminated SSID. An array of 33 Unsigned 8bit values */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_SSID,
    /* BSSID. An array of 6 Unsigned 8bit values */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_BSSID,
    /* Country string advertised by AP. An array of 3 Unsigned 8bit values */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_AP_COUNTRY_STR,
    /* Country string for this association. An array of 3 Unsigned 8bit values*/
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_COUNTRY_STR,

    /* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_* could
     * be nested within the interface stats.
     */

    /* Type = enum wifi_traffic_ac, e.g., V0, VI, BE and BK */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_AC,
    /* Unsigned int 32 value corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MPDU,
    /* Unsigned int 32 value corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MPDU,
    /* Unsigned int 32 value corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MCAST,
    /* Unsigned int 32 value corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MCAST,
    /* Unsigned int 32 value corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_AMPDU,
    /* Unsigned int 32 value corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_AMPDU,
    /* Unsigned int 32 value corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_MPDU_LOST,
    /* Unsigned int 32 value corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES,
    /* Unsigned int 32 value corresponding to respective AC  */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_SHORT,
    /* Unsigned int 32 values corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_LONG,
    /* Unsigned int 32 values corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MIN,
    /* Unsigned int 32 values corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MAX,
    /* Unsigned int 32 values corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_AVG,
    /* Unsigned int 32 values corresponding to respective AC */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_NUM_SAMPLES,
    /* Unsigned 32bit value. Number of peers */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS,

    /* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_* are
     * nested within the interface stats.
     */

    /* Type = enum wifi_peer_type. Peer type, e.g., STA, AP, P2P GO etc. */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_TYPE,
    /* MAC addr corresponding to respective peer.
     *  An array of 6 Unsigned 8bit values.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_MAC_ADDRESS,
    /* Unsigned int 32bit value representing capabilities
     * corresponding to respective peer.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_CAPABILITIES,
    /* Unsigned 32bit value. Number of rates */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES,

    /* Attributes nested within the rate stats.*/
    /* Unsigned 8bit value */
    /* Unsigned int 8bit value; 0: OFDM, 1:CCK, 2:HT 3:VHT 4..7 reserved */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_PREAMBLE,
    /* Unsigned int 8bit value; 0:1x1, 1:2x2, 3:3x3, 4:4x4 */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_NSS,
    /* Unsigned int 8bit value; 0:20MHz, 1:40Mhz, 2:80Mhz, 3:160Mhz */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BW,
    /* Unsigned int 8bit value; OFDM/CCK rate code would be as per IEEE Std
     * in the units of 0.5mbps HT/VHT it would be mcs index */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MCS_INDEX,

    /* Unsigned 32bit value. Bit rate in units of 100Kbps */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BIT_RATE,

    /* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_* could be
     * nested within the peer info stats.
     */

    /* Unsigned int 32bit value. Number of successfully transmitted data pkts,
     * i.e., with ACK received  *corresponding to the respective rate.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_TX_MPDU,
    /* Unsigned int 32bit value. Number of received data pkts
     * corresponding to the respective rate. */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RX_MPDU,
    /* Unsigned int 32bit value. Number of data pkts losses, i.e.,
     * no ACK received corresponding to *the respective rate.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MPDU_LOST,
    /* Unsigned int 32bit value. Total number of data pkt retries for
     *   the respective rate.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES,
    /* Unsigned int 32bit value. Total number of short data pkt retries for
      the respective rate. */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_SHORT,
    /* Unsigned int 32bit value. Total number of long data pkt retries for
     * the respective rate.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_LONG,

    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID,
    /* Unsigned 32bit value. Total number of msecs the radio is awake
     *  accruing over time.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME,
    /* Unsigned 32bit value. Total number of msecs the radio is
     * transmitting accruing over time.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME,
    /* Unsigned 32bit value. Total number of msecs the radio is
     * in active receive accruing over time.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_RX_TIME,
    /* Unsigned 32bit value. Total number of msecs the radio is
     * awake due to all scan accruing over time.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_SCAN,
    /* Unsigned 32bit value. Total number of msecs the radio is
     *   awake due to NAN accruing over time.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_NBD,
    /* Unsigned 32bit value. Total number of msecs the radio is
     * awake due to GSCAN accruing over time.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_GSCAN,
    /* Unsigned 32bit value. Total number of msecs the radio is
     * awake due to roam scan accruing over time.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_ROAM_SCAN,
    /* Unsigned 32bit value. Total number of msecs the radio is
     * awake due to PNO scan accruing over time.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_PNO_SCAN,
    /* Unsigned 32bit value. Total number of msecs the radio is
     * awake due to HS2.0 scans and GAS exchange accruing over time.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_HS20,
    /* Unsigned 32bit value. Number of channels. */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS,

    /* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_
     * could be nested within the channel stats.
     */

    /* Type = enum wifi_channel_width. Channel width, e.g., 20, 40, 80, etc.*/
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_WIDTH,
    /* Unsigned 32bit value. Primary 20MHz channel. */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ,
    /* Unsigned 32bit value. Center frequency (MHz) first segment. */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ0,
    /* Unsigned 32bit value. Center frequency (MHz) second segment. */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ1,

    /* Attributes of type QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_ could be
     * nested within the radio stats.
     */

    /* Unsigned int 32bit value representing total number of msecs the radio
     * s awake on that *channel accruing over time, corresponding to
     * the respective channel.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_ON_TIME,
    /* Unsigned int 32bit value representing total number of msecs the
     * CCA register is busy accruing  *over time corresponding to the
     * respective channel.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_CCA_BUSY_TIME,


    QCA_WLAN_VENDOR_ATTR_LL_STATS_NUM_RADIOS,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_RATE_INFO,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO,

    /* Unsigned 8bit value. Used by the driver; if set to 1, it indicates that
     * more stats, e.g., peers or radio, are to follow in the next
     * QCA_NL80211_VENDOR_SUBCMD_LL_STATS_*_RESULTS event.
     * Otherwise, it is set to 0.
     */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_RESULTS_MORE_DATA,

    /* keep last */
    QCA_WLAN_VENDOR_ATTR_LL_STATS_AFTER_LAST,
    QCA_WLAN_VENDOR_ATTR_LL_STATS_MAX   =
                        QCA_WLAN_VENDOR_ATTR_LL_STATS_AFTER_LAST -1,
};


#endif /* WLAN_FEATURE_LINK_LAYER_STATS */


/* Vendor id to be used in vendor specific command and events
 * to user space. Use QCA OUI 00:13:74 to match with define in
 * supplicant code.
 */
#define QCA_NL80211_VENDOR_ID                0x001374

/* Vendor speicific sub-command id and their index */
#ifdef FEATURE_WLAN_CH_AVOID
#define QCA_NL80211_VENDOR_SUBCMD_AVOID_FREQUENCY_INDEX   0
#endif /* FEATURE_WLAN_CH_AVOID */

#ifdef FEATURE_WLAN_CH_AVOID
#define HDD_MAX_AVOID_FREQ_RANGES   4
typedef struct sHddAvoidFreqRange
{
   u32 startFreq;
   u32 endFreq;
} tHddAvoidFreqRange;

typedef struct sHddAvoidFreqList
{
   u32 avoidFreqRangeCount;
   tHddAvoidFreqRange avoidFreqRange[HDD_MAX_AVOID_FREQ_RANGES];
} tHddAvoidFreqList;
#endif /* FEATURE_WLAN_CH_AVOID */

struct cfg80211_bss* wlan_hdd_cfg80211_update_bss_db( hdd_adapter_t *pAdapter,
                                      tCsrRoamInfo *pRoamInfo
                                      );

#ifdef FEATURE_WLAN_LFR
int wlan_hdd_cfg80211_pmksa_candidate_notify(
                    hdd_adapter_t *pAdapter, tCsrRoamInfo *pRoamInfo,
                    int index, bool preauth );
#endif

#ifdef FEATURE_WLAN_LFR_METRICS
VOS_STATUS wlan_hdd_cfg80211_roam_metrics_preauth(hdd_adapter_t *pAdapter,
                                                  tCsrRoamInfo *pRoamInfo);

VOS_STATUS wlan_hdd_cfg80211_roam_metrics_preauth_status(
    hdd_adapter_t *pAdapter, tCsrRoamInfo *pRoamInfo, bool preauth_status);

VOS_STATUS wlan_hdd_cfg80211_roam_metrics_handover(hdd_adapter_t *pAdapter,
                                                   tCsrRoamInfo *pRoamInfo);
#endif

#ifdef FEATURE_WLAN_WAPI
void wlan_hdd_cfg80211_set_key_wapi(hdd_adapter_t* pAdapter,
              u8 key_index, const u8 *mac_addr, u8 *key , int key_Len);
#endif
struct wiphy *wlan_hdd_cfg80211_wiphy_alloc(int priv_size);

int wlan_hdd_cfg80211_scan( struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                            struct net_device *dev,
#endif
                            struct cfg80211_scan_request *request);

int wlan_hdd_cfg80211_init(struct device *dev,
                               struct wiphy *wiphy,
                               hdd_config_t *pCfg
                                         );

int wlan_hdd_cfg80211_register( struct wiphy *wiphy);
void wlan_hdd_cfg80211_register_frames(hdd_adapter_t* pAdapter);

void wlan_hdd_cfg80211_deregister_frames(hdd_adapter_t* pAdapter);
#ifdef CONFIG_ENABLE_LINUX_REG
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
void wlan_hdd_linux_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request);
#else
int wlan_hdd_linux_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request);
#endif
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
void wlan_hdd_crda_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request);
#else
int wlan_hdd_crda_reg_notifier(struct wiphy *wiphy, struct regulatory_request *request);
#endif
#endif

extern v_VOID_t hdd_connSetConnectionState( hdd_station_ctx_t *pHddStaCtx,
                                        eConnectionState connState );
VOS_STATUS wlan_hdd_validate_operation_channel(hdd_adapter_t *pAdapter,int channel);
#ifdef FEATURE_WLAN_TDLS
int wlan_hdd_cfg80211_send_tdls_discover_req(struct wiphy *wiphy,
                            struct net_device *dev, u8 *peer);
#endif
#ifdef WLAN_FEATURE_GTK_OFFLOAD
extern void wlan_hdd_cfg80211_update_replayCounterCallback(void *callbackContext,
                            tpSirGtkOffloadGetInfoRspParams pGtkOffloadGetInfoRsp);
#endif
void* wlan_hdd_change_country_code_cb(void *pAdapter);
void hdd_select_cbmode( hdd_adapter_t *pAdapter,v_U8_t operationChannel);


#ifdef FEATURE_WLAN_CH_AVOID
int wlan_hdd_send_avoid_freq_event(hdd_context_t *pHddCtx,
                                   tHddAvoidFreqList *pAvoidFreqList);
#endif /* FEATURE_WLAN_CH_AVOID */

#endif
