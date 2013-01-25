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

#define TDLS_MAX_DISCOVER_ATTEMPT   2

#define TDLS_DISCOVERY_PERIOD       3600000

#define TDLS_TX_STATS_PERIOD        3600000

#define TDLS_IMPLICIT_TRIGGER_PKT_THRESHOLD     100

#define TDLS_RX_IDLE_TIMEOUT        5000

#define TDLS_RSSI_TRIGGER_HYSTERESIS 50

typedef struct
{
    tANI_U32    tx_period_t;
    tANI_U32    tx_packet_n;
    tANI_U32    discovery_period_t;
    tANI_U32    discovery_tries_n;
    tANI_U32    rx_timeout_t;
    tANI_U32    rssi_hysteresis;
} tdls_config_params_t;

typedef enum {
    eTDLS_CAP_NOT_SUPPORTED = -1,
    eTDLS_CAP_UNKNOWN = 0,
    eTDLS_CAP_SUPPORTED = 1,
} eTDLSCapType;

typedef enum {
    eTDLS_LINK_NOT_CONNECTED = 0,
    eTDLS_LINK_CONNECTED = 1,
} eTDLSLinkStatus;

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


int wlan_hdd_tdls_init(struct net_device *dev);

void wlan_hdd_tdls_exit(void);

u8 wlan_hdd_tdls_extract_da(struct sk_buff *skb, u8 *mac);

u8 wlan_hdd_tdls_extract_sa(struct sk_buff *skb, u8 *mac);

int wlan_hdd_tdls_add_peer_to_list(u8 key, u8 *mac, u8 tx);

int wlan_hdd_saveTdlsPeer(tCsrRoamInfo *pRoamInfo);

int wlan_hdd_findTdlsPeer(tSirMacAddr peerMac);

int wlan_hdd_tdls_set_link_status(u8 *mac, int status);

int wlan_hdd_tdls_set_cap(u8 *mac, int cap);

int wlan_hdd_tdls_set_rssi(u8 *mac, tANI_S8 rxRssi);

int wlan_hdd_tdls_set_params(tdls_config_params_t *config);

void wlan_hdd_removeTdlsPeer(tCsrRoamInfo *pRoamInfo);
#endif // __HDD_TDSL_H
