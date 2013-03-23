/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#ifndef __HDD_TDSL_H
#define __HDD_TDSL_H
/**===========================================================================

\file         wlan_hdd_tdls.h

\brief       Linux HDD TDLS include file

==========================================================================*/

#define MAX_NUM_TDLS_PEER           3

#define TDLS_SUB_DISCOVERY_PERIOD   100

#define TDLS_MAX_DISCOVER_REQS_PER_TIMER 1

#define TDLS_DISCOVERY_PERIOD       3600000

#define TDLS_TX_STATS_PERIOD        3600000

#define TDLS_IMPLICIT_TRIGGER_PKT_THRESHOLD     100

#define TDLS_RX_IDLE_TIMEOUT        5000

#define TDLS_RSSI_TRIGGER_HYSTERESIS 50

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
} tdls_config_params_t;

typedef enum {
    eTDLS_SUPPORT_DISABLED = 0,
    eTDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY,
    eTDLS_SUPPORT_ENABLED,
} eTDLSSupportMode;

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
} tTDLSLinkStatus;

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

typedef struct {
    struct list_head peer_list[256];
    hdd_adapter_t   *pAdapter;
    vos_timer_t     peerDiscoverTimer;
    vos_timer_t     peerUpdateTimer;
    tdls_config_params_t threshold_config;
    tANI_S32        discovery_peer_cnt;
    tANI_S8         ap_rssi;
} tdlsCtx_t;

typedef struct {
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
    vos_timer_t     peerIdleTimer;
} hddTdlsPeer_t;

typedef struct {
    /* Session ID */
    tANI_U8 sessionId;
    /*TDLS peer station id */
    v_U8_t staId;
    /* TDLS peer mac Address */
    v_MACADDR_t peerMac;
} tdlsConnInfo_t;

int wlan_hdd_tdls_init(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_exit(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_extract_da(struct sk_buff *skb, u8 *mac);

void wlan_hdd_tdls_extract_sa(struct sk_buff *skb, u8 *mac);

int wlan_hdd_tdls_increment_pkt_count(hdd_adapter_t *pAdapter, u8 *mac, u8 tx);

int wlan_hdd_tdls_set_sta_id(hdd_adapter_t *pAdapter, u8 *mac, u8 staId);

hddTdlsPeer_t *wlan_hdd_tdls_find_peer(hdd_adapter_t *pAdapter, u8 *mac);

hddTdlsPeer_t *wlan_hdd_tdls_get_peer(hdd_adapter_t *pAdapter, u8 *mac);

void wlan_hdd_tdls_set_peer_link_status(hddTdlsPeer_t *curr_peer, tTDLSLinkStatus status);

void wlan_hdd_tdls_set_link_status(hdd_adapter_t *pAdapter, u8* mac, tTDLSLinkStatus status);

int wlan_hdd_tdls_recv_discovery_resp(hdd_adapter_t *pAdapter, u8 *mac);

int wlan_hdd_tdls_set_rssi(hdd_adapter_t *pAdapter, u8 *mac, tANI_S8 rxRssi);

int wlan_hdd_tdls_set_responder(hdd_adapter_t *pAdapter, u8 *mac, tANI_U8 responder);

int wlan_hdd_tdls_get_responder(hdd_adapter_t *pAdapter, u8 *mac);

int wlan_hdd_tdls_set_signature(hdd_adapter_t *pAdapter, u8 *mac, tANI_U8 uSignature);

int wlan_hdd_tdls_set_params(struct net_device *dev, tdls_config_params_t *config);

int wlan_hdd_tdls_reset_peer(hdd_adapter_t *pAdapter, u8 *mac);

tANI_U16 wlan_hdd_tdlsConnectedPeers(hdd_adapter_t *pAdapter);

int wlan_hdd_tdls_get_all_peers(hdd_adapter_t *pAdapter, char *buf, int buflen);

void wlan_hdd_tdls_connection_callback(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_disconnection_callback(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_mgmt_completion_callback(hdd_adapter_t *pAdapter, tANI_U32 statusCode);

void wlan_hdd_tdls_increment_peer_count(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_decrement_peer_count(hdd_adapter_t *pAdapter);

void wlan_hdd_tdls_check_bmps(hdd_adapter_t *pAdapter);

u8 wlan_hdd_tdls_is_progress(hdd_adapter_t *pAdapter, u8* mac, u8 skip_self);

#endif // __HDD_TDSL_H
