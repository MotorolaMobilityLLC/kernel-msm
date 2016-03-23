/*
 * Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
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

#ifndef __HDD_TDSL_H
#define __HDD_TDSL_H
/**===========================================================================

\file         wlan_hdd_tdls.h

\brief       Linux HDD TDLS include file
==========================================================================*/

#ifdef FEATURE_WLAN_TDLS

#define MAX_NUM_TDLS_PEER           3

#define TDLS_SUB_DISCOVERY_PERIOD   100

#define TDLS_MAX_DISCOVER_REQS_PER_TIMER 1

#define TDLS_DISCOVERY_PERIOD       3600000

#define TDLS_TX_STATS_PERIOD        3600000

#define TDLS_IMPLICIT_TRIGGER_PKT_THRESHOLD     100

#define TDLS_RX_IDLE_TIMEOUT        5000

#define TDLS_RSSI_TRIGGER_HYSTERESIS 50

/* before UpdateTimer expires, we want to timeout discovery response.
should not be more than 2000 */
#define TDLS_DISCOVERY_TIMEOUT_BEFORE_UPDATE     1000

#define TDLS_CTX_MAGIC 0x54444c53    // "TDLS"

#define TDLS_MAX_SCAN_SCHEDULE          10
#define TDLS_MAX_SCAN_REJECT            5
#define TDLS_DELAY_SCAN_PER_CONNECTION 100
#define TDLS_MAX_CONNECTED_PEERS_TO_ALLOW_SCAN   1

#define TDLS_IS_CONNECTED(peer)  \
        ((eTDLS_LINK_CONNECTED == (peer)->link_status) || \
         (eTDLS_LINK_TEARING == (peer)->link_status))

/* bit mask flag for tdls_option to FW */
#define ENA_TDLS_OFFCHAN      (1 << 0)  /* TDLS Off Channel support */
#define ENA_TDLS_BUFFER_STA   (1 << 1)  /* TDLS Buffer STA support */
#define ENA_TDLS_SLEEP_STA    (1 << 2)  /* TDLS Sleep STA support */
#define TDLS_SEC_OFFCHAN_OFFSET_0        0
#define TDLS_SEC_OFFCHAN_OFFSET_40PLUS   40
#define TDLS_SEC_OFFCHAN_OFFSET_40MINUS  (-40)
#define TDLS_SEC_OFFCHAN_OFFSET_80       80
#define TDLS_SEC_OFFCHAN_OFFSET_160      160

#define TDLS_PEER_LIST_SIZE   256

typedef struct
{
    tANI_U32    tdls;
    tANI_U32    tx_period_t;
    tANI_U32    tx_packet_n;
    tANI_U32    discovery_period_t;
    tANI_U32    discovery_tries_n;
    tANI_U32    idle_timeout_t;
    tANI_U32    idle_packet_n;
    tANI_U32    rssi_hysteresis;
    tANI_S32    rssi_trigger_threshold;
    tANI_S32    rssi_teardown_threshold;
    tANI_S32    rssi_delta;
} tdls_config_params_t;

typedef struct
{
    struct wiphy *wiphy;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)) && !defined(WITH_BACKPORTS)
    struct net_device *dev;
#endif
    struct cfg80211_scan_request *scan_request;
    int magic;
    int attempt;
    int reject;
    struct delayed_work tdls_scan_work;
} tdls_scan_context_t;

typedef enum {
    eTDLS_SUPPORT_NOT_ENABLED = 0,
    eTDLS_SUPPORT_DISABLED, /* suppress implicit trigger and not respond to the peer */
    eTDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY, /* suppress implicit trigger, but respond to the peer */
    eTDLS_SUPPORT_ENABLED, /* implicit trigger */
    /* External control means implicit trigger
     * but only to a peer mac configured by user space.
     */
    eTDLS_SUPPORT_EXTERNAL_CONTROL
} eTDLSSupportMode;

enum tdls_spatial_streams {
    TDLS_NSS_1x1_MODE = 0,
    TDLS_NSS_2x2_MODE = 0xff,
};

/**
 * enum tdls_nss_transition_type - TDLS NSS transition states
 * @TDLS_NSS_TRANSITION_UNKNOWN: default state
 * @TDLS_NSS_TRANSITION_2x2_to_1x1: transition from 2x2 to 1x1 stream
 * @TDLS_NSS_TRANSITION_1x1_to_2x2: transition from 1x1 to 2x2 stream
 */
enum tdls_nss_transition_type {
    TDLS_NSS_TRANSITION_UNKNOWN = 0,
    TDLS_NSS_TRANSITION_2x2_to_1x1,
    TDLS_NSS_TRANSITION_1x1_to_2x2,
};

typedef enum eTDLSCapType{
    eTDLS_CAP_NOT_SUPPORTED = -1,
    eTDLS_CAP_UNKNOWN = 0,
    eTDLS_CAP_SUPPORTED = 1,
} tTDLSCapType;

typedef enum eTDLSLinkStatus {
    eTDLS_LINK_IDLE = 0,
    eTDLS_LINK_DISCOVERING,
    eTDLS_LINK_DISCOVERED,
    eTDLS_LINK_CONNECTING,
    eTDLS_LINK_CONNECTED,
    eTDLS_LINK_TEARING,
} tTDLSLinkStatus;

/**
 * enum tdls_teardown_reason - Reason for TDLS teardown
 * @eTDLS_TEARDOWN_EXT_CTRL: Reason ext ctrl.
 * @eTDLS_TEARDOWN_CONCURRENCY: Reason concurrency.
 * @eTDLS_TEARDOWN_RSSI_THRESHOLD: Reason rssi threashold.
 * @eTDLS_TEARDOWN_TXRX_THRESHOLD: Reason txrx threashold.
 * @eTDLS_TEARDOWN_BTCOEX: Reason BTCOEX.
 * @eTDLS_TEARDOWN_SCAN: Reason scan.
 * @eTDLS_TEARDOWN_BSS_DISCONNECT: Reason bss disconnected.
 *
 * Reason to indicate in diag event of tdls teardown.
 */

enum tdls_teardown_reason {
	eTDLS_TEARDOWN_EXT_CTRL,
	eTDLS_TEARDOWN_CONCURRENCY,
	eTDLS_TEARDOWN_RSSI_THRESHOLD,
	eTDLS_TEARDOWN_TXRX_THRESHOLD,
	eTDLS_TEARDOWN_BTCOEX,
	eTDLS_TEARDOWN_SCAN,
	eTDLS_TEARDOWN_BSS_DISCONNECT,
	eTDLS_TEARDOWN_ANTENNA_SWITCH,
};

typedef enum {
    eTDLS_LINK_SUCCESS,                /* Success */
    eTDLS_LINK_UNSPECIFIED       = -1, /* Unspecified reason */
    eTDLS_LINK_NOT_SUPPORTED     = -2, /* Remote side doesn't support TDLS */
    eTDLS_LINK_UNSUPPORTED_BAND  = -3, /* Remote side doesn't support this
                                          band */
    eTDLS_LINK_NOT_BENEFICIAL    = -4, /* Going to AP is better than direct */
    eTDLS_LINK_DROPPED_BY_REMOTE = -5  /* Remote side doesn't want it anymore */
} tTDLSLinkReason;

typedef struct {
    int channel;                       /* channel hint, in channel number
                                         (NOT frequency ) */
    int global_operating_class;        /* operating class to use */
    int max_latency_ms;                /* max latency that can be tolerated
                                          by apps */
    int min_bandwidth_kbps;            /* bandwidth required by apps, in kilo
                                          bits per second */
} tdls_req_params_t;

typedef enum {
    QCA_WIFI_HAL_TDLS_DISABLED = 1, /* TDLS is not enabled, or is disabled
                                       now */
    QCA_WIFI_HAL_TDLS_ENABLED,      /* TDLS is enabled, but not yet tried */
    QCA_WIFI_HAL_TDLS_ESTABLISHED,  /* Direct link is established */
    QCA_WIFI_HAL_TDLS_ESTABLISHED_OFF_CHANNEL, /* Direct link is established
                                                  using MCC */
    QCA_WIFI_HAL_TDLS_DROPPED,      /* Direct link was established, but is
                                       now dropped */
    QCA_WIFI_HAL_TDLS_FAILED        /* Direct link failed */
} tdls_state_t;

typedef int (*cfg80211_exttdls_callback)(const tANI_U8* mac,
                                         uint32_t opclass,
                                         uint32_t channel,
                                         tANI_U32 state,
                                         tANI_S32 reason,
                                         void *ctx);
typedef struct {
    tANI_U16    period;
    tANI_U16    bytes;
} tdls_tx_tput_config_t;

typedef struct {
    tANI_U16    period;
    tANI_U16    tries;
} tdls_discovery_config_t;

typedef struct {
    tANI_U16    timeout;
} tdls_rx_idle_config_t;

typedef struct {
    tANI_U16    rssi_thres;
} tdls_rssi_config_t;

struct _hddTdlsPeer_t;

typedef struct {
    struct list_head peer_list[TDLS_PEER_LIST_SIZE];
    hdd_adapter_t   *pAdapter;
    vos_timer_t     peerDiscoveryTimeoutTimer;
    tdls_config_params_t threshold_config;
    tANI_U32        discovery_sent_cnt;
    tANI_S8         ap_rssi;
    struct _hddTdlsPeer_t  *curr_candidate;
    v_U32_t            magic;
} tdlsCtx_t;

typedef struct _hddTdlsPeer_t {
    struct list_head node;
    tdlsCtx_t   *pHddTdlsCtx;
    tSirMacAddr peerMac;
    tANI_U16    staId ;
    tANI_S8     rssi;
    tTDLSCapType     tdls_support;
    tTDLSLinkStatus  link_status;
    tANI_U8     signature;
    tANI_U8     is_responder;
    tANI_U8     discovery_processed;
    tANI_U16    discovery_attempt;
    tANI_U16    tx_pkt;
    tANI_U16    rx_pkt;
    tANI_U8     uapsdQueues;
    tANI_U8     maxSp;
    uint8_t     qos;
    tANI_U8     isBufSta;
    tANI_U8     isOffChannelSupported;
    tANI_U8     supported_channels_len;
    tANI_U8     supported_channels[SIR_MAC_MAX_SUPP_CHANNELS];
    tANI_U8     supported_oper_classes_len;
    tANI_U8     supported_oper_classes[SIR_MAC_MAX_SUPP_OPER_CLASSES];
    tANI_BOOLEAN  isForcedPeer;
    tANI_U8       op_class_for_pref_off_chan;
    tANI_U8       pref_off_chan_num;
    tANI_U8       op_class_for_pref_off_chan_is_set;
    uint8_t     spatial_streams;
    /* EXT TDLS */
    tTDLSLinkReason reason;
    cfg80211_exttdls_callback state_change_notification;
} hddTdlsPeer_t;

typedef struct {
    /* Session ID */
    tANI_U8 sessionId;
    /*TDLS peer station id */
    v_U8_t staId;
    /* TDLS peer mac Address */
    v_MACADDR_t peerMac;
} tdlsConnInfo_t;

typedef struct {
    tANI_U32 vdev_id;
    tANI_U32 tdls_state;
    tANI_U32 notification_interval_ms;
    tANI_U32 tx_discovery_threshold;
    tANI_U32 tx_teardown_threshold;
    tANI_S32 rssi_teardown_threshold;
    tANI_S32 rssi_delta;
    tANI_U32 tdls_options;
    tANI_U32 peer_traffic_ind_window;
    tANI_U32 peer_traffic_response_timeout;
    tANI_U32 puapsd_mask;
    tANI_U32 puapsd_inactivity_time;
    tANI_U32 puapsd_rx_frame_threshold;
    uint32_t teardown_notification_ms;
    uint32_t tdls_peer_kickout_threshold;
} tdlsInfo_t;

int wlan_hdd_tdls_init(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_exit(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_extract_da(struct sk_buff *skb, u8 *mac);

void wlan_hdd_tdls_extract_sa(struct sk_buff *skb, u8 *mac);

int wlan_hdd_tdls_increment_pkt_count(hdd_adapter_t *pAdapter, const u8 *mac,
                                      u8 tx);

int wlan_hdd_tdls_set_sta_id(hdd_adapter_t *pAdapter, const u8 *mac, u8 staId);

hddTdlsPeer_t *wlan_hdd_tdls_find_peer(hdd_adapter_t *pAdapter,
                                       const u8 *mac, tANI_BOOLEAN mutexLock);

hddTdlsPeer_t *wlan_hdd_tdls_find_all_peer(hdd_context_t *pHddCtx,
                                           const u8 *mac);

int wlan_hdd_tdls_get_link_establish_params(hdd_adapter_t *pAdapter,
                                            const u8 *mac,
                                            tCsrTdlsLinkEstablishParams* tdlsLinkEstablishParams);
hddTdlsPeer_t *wlan_hdd_tdls_get_peer(hdd_adapter_t *pAdapter, const u8 *mac);

int wlan_hdd_tdls_set_cap(hdd_adapter_t *pAdapter, const u8* mac,
                          tTDLSCapType cap);

void wlan_hdd_tdls_set_peer_link_status(hddTdlsPeer_t *curr_peer,
                                        tTDLSLinkStatus status,
                                        tTDLSLinkReason reason);
void wlan_hdd_tdls_set_link_status(hdd_adapter_t *pAdapter,
                                   const u8* mac,
                                   tTDLSLinkStatus linkStatus,
                                   tTDLSLinkReason reason);

int wlan_hdd_tdls_recv_discovery_resp(hdd_adapter_t *pAdapter, u8 *mac);

int wlan_hdd_tdls_set_peer_caps(hdd_adapter_t *pAdapter,
                                const u8 *mac,
                                tCsrStaParams *StaParams,
                                tANI_BOOLEAN isBufSta,
                                tANI_BOOLEAN isOffChannelSupported,
                                bool is_qos_wmm_sta);

int wlan_hdd_tdls_set_rssi(hdd_adapter_t *pAdapter, const u8 *mac,
                           tANI_S8 rxRssi);

int wlan_hdd_tdls_set_responder(hdd_adapter_t *pAdapter, const u8 *mac,
                                tANI_U8 responder);

int wlan_hdd_tdls_set_signature(hdd_adapter_t *pAdapter, const u8 *mac,
                                tANI_U8 uSignature);

int wlan_hdd_tdls_set_params(struct net_device *dev, tdls_config_params_t *config);

int wlan_hdd_tdls_reset_peer(hdd_adapter_t *pAdapter, const u8 *mac);

tANI_U16 wlan_hdd_tdlsConnectedPeers(hdd_adapter_t *pAdapter);

int wlan_hdd_tdls_get_all_peers(hdd_adapter_t *pAdapter, char *buf, int buflen);

void wlan_hdd_tdls_connection_callback(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_disconnection_callback(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_mgmt_completion_callback(hdd_adapter_t *pAdapter, tANI_U32 statusCode);

void wlan_hdd_tdls_increment_peer_count(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_decrement_peer_count(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_check_bmps(hdd_adapter_t *pAdapter);

hddTdlsPeer_t *wlan_hdd_tdls_is_progress(hdd_context_t *pHddCtx,
                                         const u8* mac,
                                         u8 skip_self);

int wlan_hdd_tdls_copy_scan_context(hdd_context_t *pHddCtx,
                            struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)) && !defined(WITH_BACKPORTS)
                            struct net_device *dev,
#endif
                            struct cfg80211_scan_request *request);

int wlan_hdd_tdls_scan_callback (hdd_adapter_t *pAdapter,
                                struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)) && !defined(WITH_BACKPORTS)
                                struct net_device *dev,
#endif
                                struct cfg80211_scan_request *request);

void wlan_hdd_tdls_scan_done_callback(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_timer_restart(hdd_adapter_t *pAdapter,
                                 vos_timer_t *timer,
                                 v_U32_t expirationTime);
void wlan_hdd_tdls_indicate_teardown(hdd_adapter_t *pAdapter,
                                     hddTdlsPeer_t *curr_peer,
                                     tANI_U16 reason);

void wlan_hdd_tdls_implicit_send_discovery_request(tdlsCtx_t *pHddTdlsCtx);

int wlan_hdd_tdls_set_extctrl_param(hdd_adapter_t *pAdapter,
                                    const uint8_t  *mac,
                                    uint32_t chan,
                                    uint32_t max_latency,
                                    uint32_t op_class,
                                    uint32_t min_bandwidth);
#ifdef FEATURE_WLAN_DIAG_SUPPORT
void hdd_send_wlan_tdls_teardown_event(uint32_t reason,
					uint8_t *peer_mac);
void hdd_wlan_tdls_enable_link_event(const uint8_t *peer_mac,
					uint8_t is_off_chan_supported,
					uint8_t is_off_chan_configured,
					uint8_t is_off_chan_established);
void hdd_wlan_block_scan_by_tdls_event(void);
#else
static inline
void hdd_send_wlan_tdls_teardown_event(uint32_t reason,
					uint8_t *peer_mac)
{
	return;
}
static inline
void hdd_wlan_tdls_enable_link_event(const uint8_t *peer_mac,
					uint8_t is_off_chan_supported,
					uint8_t is_off_chan_configured,
					uint8_t is_off_chan_established)
{
	return;
}
static inline
void hdd_wlan_block_scan_by_tdls_event(void)
{
	return;
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

int wlan_hdd_tdls_set_force_peer(hdd_adapter_t *pAdapter, const u8 *mac,
                                 tANI_BOOLEAN forcePeer);
int wlan_hdd_tdls_update_peer_mac(hdd_adapter_t *pAdapter,
				const uint8_t *mac,
				uint32_t peerState);

int wlan_hdd_tdls_extctrl_deconfig_peer(hdd_adapter_t *pAdapter,
                                        const u8 *peer);
int wlan_hdd_tdls_extctrl_config_peer(hdd_adapter_t *pAdapter,
                                      const u8 *peer,
                                      cfg80211_exttdls_callback callback,
                                      uint32_t chan,
                                      uint32_t max_latency,
                                      uint32_t op_class,
                                      uint32_t min_bandwidth);
void hdd_tdls_notify_mode_change(hdd_adapter_t *pAdapter,
				hdd_context_t *pHddCtx);
void wlan_hdd_tdls_disable_offchan_and_teardown_links(hdd_context_t *pHddCtx);

/* EXT TDLS */
int wlan_hdd_tdls_get_status(hdd_adapter_t *pAdapter,
                             const tANI_U8* mac,
                             uint32_t *opclass,
                             uint32_t *channel,
                             tANI_U32 *state,
                             tANI_S32 *reason);
void wlan_hdd_tdls_get_wifi_hal_state(hddTdlsPeer_t *curr_peer,
                                      tANI_U32 *state,
                                      tANI_S32 *reason);
int wlan_hdd_set_callback(hddTdlsPeer_t *curr_peer,
                          cfg80211_exttdls_callback callback);
int hdd_set_tdls_offchannel(hdd_context_t *pHddCtx, int offchannel);
int hdd_set_tdls_secoffchanneloffset(hdd_context_t *pHddCtx, int offchanoffset);
int hdd_set_tdls_offchannelmode(hdd_adapter_t *pAdapter, int offchanmode);
void wlan_hdd_update_tdls_info(hdd_adapter_t *adapter, bool tdls_prohibited,
                               bool tdls_chan_swit_prohibited);
int hdd_set_tdls_scan_type(hdd_context_t *hdd_ctx, int val);
int wlan_hdd_tdls_antenna_switch(hdd_context_t *hdd_ctx,
					hdd_adapter_t *adapter,
					uint32_t mode);


#else
static inline void hdd_tdls_notify_mode_change(hdd_adapter_t *pAdapter,
				hdd_context_t *pHddCtx)
{
}
static inline void
wlan_hdd_tdls_disable_offchan_and_teardown_links(hdd_context_t *pHddCtx)
{
}
static inline void wlan_hdd_tdls_exit(hdd_adapter_t *pAdapter)
{
}
static inline void
wlan_hdd_tdls_implicit_send_discovery_request(void *pHddTdlsCtx)
{
}
static inline int wlan_hdd_tdls_antenna_switch(hdd_context_t *hdd_ctx,
						      hdd_adapter_t *adapter,
						      uint32_t mode)
{
	return 0;
}
#endif

#endif // __HDD_TDSL_H
