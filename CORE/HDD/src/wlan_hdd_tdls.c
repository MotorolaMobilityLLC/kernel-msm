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

/**========================================================================

  \file  wlan_hdd_tdls.c

  \brief WLAN Host Device Driver implementation for TDLS

  ========================================================================*/

#include <wlan_hdd_includes.h>
#include <wlan_hdd_hostapd.h>
#include <net/cfg80211.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/list.h>
#include <linux/etherdevice.h>
#include <net/ieee80211_radiotap.h>
#include "wlan_hdd_tdls.h"
#include "wlan_hdd_cfg80211.h"


typedef struct {
    struct list_head node;
    tSirMacAddr peerMac;
    tANI_U16    staId ;
    tANI_S8     rssi;
    tANI_S8     tdls_support;
    tANI_S8     link_status;
    tANI_U16    discovery_attempt;
    tANI_U16    tx_pkt;
    tANI_U16    rx_pkt;
    vos_timer_t     peerIdleTimer;
} hddTdlsPeer_t;


typedef struct {
    hddTdlsPeer_t*  peer_list[256];
    struct net_device *dev;
    spinlock_t      lock;
    vos_timer_t     peerDiscoverTimer;
    vos_timer_t     peerUpdateTimer;
    tdls_config_params_t threshold_config;
    tANI_S8         ap_rssi;
} tdlsCtx_t;

static tdlsCtx_t *pHddTdlsCtx;
static v_BOOL_t tdlsImplicitTrigger;

void wlan_hdd_freeTdlsPeer(void);

#ifndef WLAN_FEATURE_TDLS_DEBUG
#define TDLS_LOG_LEVEL VOS_TRACE_LEVEL_INFO
#else
#define TDLS_LOG_LEVEL VOS_TRACE_LEVEL_WARN
#endif

static v_VOID_t wlan_hdd_discover_peer_cb( v_PVOID_t userData )
{
    int i;
    hddTdlsPeer_t *curr_peer;
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;

    if (NULL == pHddTdlsCtx) return;

    pAdapter = WLAN_HDD_GET_PRIV_PTR(pHddTdlsCtx->dev);
    pHddCtx = WLAN_HDD_GET_CTX( pAdapter );

    for (i = 0; i < 256; i++) {
        curr_peer = pHddTdlsCtx->peer_list[i];
        if (NULL == curr_peer) continue;

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "hdd discovery cb - %d: %x %x %x %x %x %x\n", i,
                   curr_peer->peerMac[0],
                   curr_peer->peerMac[1],
                   curr_peer->peerMac[2],
                   curr_peer->peerMac[3],
                   curr_peer->peerMac[4],
                   curr_peer->peerMac[5]);

        do {
            if ((eTDLS_CAP_UNKNOWN == curr_peer->tdls_support) &&
                (eTDLS_LINK_NOT_CONNECTED == curr_peer->link_status) &&
                (curr_peer->discovery_attempt <
                pHddTdlsCtx->threshold_config.discovery_tries_n)) {
                    wlan_hdd_cfg80211_send_tdls_discover_req(pHddCtx->wiphy,
                                        pHddTdlsCtx->dev, curr_peer->peerMac);
//        cfg80211_tdls_oper_request(pHddTdlsCtx->dev, curr_peer->peerMac,
//                                   NL80211_TDLS_DISCOVERY_REQ, FALSE, GFP_KERNEL);

//                if (++curr_peer->discovery_attempt >= pHddTdlsCtx->threshold_config.discovery_tries_n) {
//                    curr_peer->tdls_support = eTDLS_CAP_NOT_SUPPORTED;
//                }
            }

            curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
        } while (&curr_peer->node != curr_peer->node.next);
    }

    vos_timer_start( &pHddTdlsCtx->peerDiscoverTimer,
                        pHddTdlsCtx->threshold_config.discovery_period_t );

    wlan_hdd_get_rssi(pAdapter, &pHddTdlsCtx->ap_rssi);

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "beacon rssi: %d",
                        pHddTdlsCtx->ap_rssi);
}

static v_VOID_t wlan_hdd_update_peer_cb( v_PVOID_t userData )
{
    int i;
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return;

    for (i = 0; i < 256; i++) {
        curr_peer = pHddTdlsCtx->peer_list[i];
        if (NULL == curr_peer) continue;

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "hdd update cb - %d: %x %x %x %x %x %x -> %d\n", i,
                   curr_peer->peerMac[0],
                   curr_peer->peerMac[1],
                   curr_peer->peerMac[2],
                   curr_peer->peerMac[3],
                   curr_peer->peerMac[4],
                   curr_peer->peerMac[5],
                   curr_peer->tx_pkt);

        do {
            if (eTDLS_CAP_SUPPORTED == curr_peer->tdls_support) {
                VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                    "%s: (tx %d, rx %d, config %d) %02x:%02x:%02x:%02x:%02x:%02x (%d) ",
                       __func__,  curr_peer->tx_pkt, curr_peer->rx_pkt,
                        pHddTdlsCtx->threshold_config.tx_packet_n,
                        curr_peer->peerMac[0], curr_peer->peerMac[1], curr_peer->peerMac[2],
                        curr_peer->peerMac[3], curr_peer->peerMac[4], curr_peer->peerMac[5],
                        curr_peer->link_status);
                if (eTDLS_LINK_CONNECTED != curr_peer->link_status) {
                    if (curr_peer->tx_pkt >=
                            pHddTdlsCtx->threshold_config.tx_packet_n) {

                    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "-> Tput trigger TDLS SETUP");
#ifdef CONFIG_TDLS_IMPLICIT
                        cfg80211_tdls_oper_request(pHddTdlsCtx->dev,
                                                   curr_peer->peerMac,
                                                   NL80211_TDLS_SETUP, FALSE,
                                                   GFP_KERNEL);
#endif
                        goto next_peer;
                    }
#ifdef WLAN_FEATURE_TDLS_DEBUG
                    else  {
                        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "-> ignored.");
                    }
#endif
                    if ((tANI_S32)curr_peer->rssi >
                            (tANI_S32)(pHddTdlsCtx->threshold_config.rssi_hysteresis +
                                pHddTdlsCtx->ap_rssi)) {

                        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                                "%s: RSSI (peer %d > ap %d + hysteresis %d) triggering to %02x:%02x:%02x:%02x:%02x:%02x ",
                                __func__, (tANI_S32)curr_peer->rssi,
                                pHddTdlsCtx->ap_rssi,
                                (tANI_S32)(pHddTdlsCtx->threshold_config.rssi_hysteresis),
                                curr_peer->peerMac[0], curr_peer->peerMac[1], curr_peer->peerMac[2],
                                curr_peer->peerMac[3], curr_peer->peerMac[4], curr_peer->peerMac[5]);

#ifdef CONFIG_TDLS_IMPLICIT
                        cfg80211_tdls_oper_request(pHddTdlsCtx->dev,
                                                   curr_peer->peerMac,
                                                   NL80211_TDLS_SETUP, FALSE,
                                                   GFP_KERNEL);
#endif
                    }
                } else {
                    if ((curr_peer->tx_pkt > 0 &&
                            curr_peer->tx_pkt <
                            pHddTdlsCtx->threshold_config.tx_packet_n) ||
                        (curr_peer->tx_pkt == 0 &&
                            curr_peer->rx_pkt == 0)) {
                        if (VOS_TIMER_STATE_RUNNING !=
                                vos_timer_getCurrentState(&curr_peer->peerIdleTimer)) {
                            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "-> start Idle Timer (%d)", pHddTdlsCtx->threshold_config.rx_timeout_t);
                            vos_timer_start( &curr_peer->peerIdleTimer,
                                        pHddTdlsCtx->threshold_config.rx_timeout_t );
                        }

                        goto next_peer;
                    }
                    else {
                        if (VOS_TIMER_STATE_RUNNING ==
                                vos_timer_getCurrentState(&curr_peer->peerIdleTimer)) {
                            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "-> stop Idle Timer ");
                            vos_timer_stop ( &curr_peer->peerIdleTimer );
                        }
#ifdef WLAN_FEATURE_TDLS_DEBUG
                        else
                            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "-> idle time was not running. ignored.");
#endif
                    }
//                    if (curr_peer->rssi <
//                            (pHddTdlsCtx->threshold_config.rssi_hysteresis +
//                                pHddTdlsCtx->ap_rssi)) {
//
//#ifdef CONFIG_TDLS_IMPLICIT
//                        cfg80211_tdls_oper_request(pHddTdlsCtx->dev,
//                                                   curr_peer->peerMac,
//                                                   NL80211_TDLS_TEARDOWN, FALSE,
//                                                   GFP_KERNEL);
//#endif
//                    }
                }
            }

next_peer:
            curr_peer->tx_pkt = 0;
            curr_peer->rx_pkt = 0;

            curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
        } while (&curr_peer->node != curr_peer->node.next);
    }

    vos_timer_start( &pHddTdlsCtx->peerUpdateTimer,
                        pHddTdlsCtx->threshold_config.tx_period_t );
}

static v_VOID_t wlan_hdd_tdls_idle_cb( v_PVOID_t userData )
{
#ifdef CONFIG_TDLS_IMPLICIT
    hddTdlsPeer_t *curr_peer = (hddTdlsPeer_t *)userData;


    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
               "%s: Tx/Rx Idle %02x:%02x:%02x:%02x:%02x:%02x trigger teardown",
               __func__,
               curr_peer->peerMac[0], curr_peer->peerMac[1], curr_peer->peerMac[2],
               curr_peer->peerMac[3], curr_peer->peerMac[4], curr_peer->peerMac[5]);

    cfg80211_tdls_oper_request(pHddTdlsCtx->dev,
                               curr_peer->peerMac,
                               NL80211_TDLS_TEARDOWN, FALSE,
                               GFP_KERNEL);
#endif
}

int wlan_hdd_tdls_init(struct net_device *dev)
{
    VOS_STATUS status;
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );

    if(FALSE == pHddCtx->cfg_ini->fEnableTDLSSupport)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s TDLS not enabled!", __func__);
        return -1;
    }

    pHddTdlsCtx = vos_mem_malloc(sizeof(tdlsCtx_t));
    if (NULL == pHddTdlsCtx) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s malloc failed!", __func__);
        return -1;
    }

    vos_mem_zero(pHddTdlsCtx, sizeof(tdlsCtx_t));

    pHddTdlsCtx->dev = dev;

    tdlsImplicitTrigger = pHddCtx->cfg_ini->fEnableTDLSImplicitTrigger;
    if(FALSE == tdlsImplicitTrigger)
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s TDLS Implicit trigger not enabled!", __func__);
        return -1;
    }
    pHddTdlsCtx->threshold_config.tx_period_t = pHddCtx->cfg_ini->fTDLSTxStatsPeriod;
    pHddTdlsCtx->threshold_config.tx_packet_n = pHddCtx->cfg_ini->fTDLSTxPacketThreshold;
    pHddTdlsCtx->threshold_config.discovery_period_t = pHddCtx->cfg_ini->fTDLSDiscoveryPeriod;
    pHddTdlsCtx->threshold_config.discovery_tries_n = pHddCtx->cfg_ini->fTDLSMaxDiscoveryAttempt;
    pHddTdlsCtx->threshold_config.rx_timeout_t = pHddCtx->cfg_ini->fTDLSRxIdleTimeout;
    pHddTdlsCtx->threshold_config.rssi_hysteresis = pHddCtx->cfg_ini->fTDLSRssiHysteresis;

    status = vos_timer_init(&pHddTdlsCtx->peerDiscoverTimer,
                     VOS_TIMER_TYPE_SW,
                     wlan_hdd_discover_peer_cb,
                     pHddTdlsCtx);

    status = vos_timer_start( &pHddTdlsCtx->peerDiscoverTimer,
                        pHddTdlsCtx->threshold_config.discovery_period_t );

    status = vos_timer_init(&pHddTdlsCtx->peerUpdateTimer,
                     VOS_TIMER_TYPE_SW,
                     wlan_hdd_update_peer_cb,
                     pHddTdlsCtx);

    status = vos_timer_start( &pHddTdlsCtx->peerUpdateTimer,
                        pHddTdlsCtx->threshold_config.tx_period_t );

    return 0;
}

void wlan_hdd_tdls_exit()
{
    if (NULL == pHddTdlsCtx) {
        hddLog(VOS_TRACE_LEVEL_WARN, "%s TDLS not enabled, exiting!", __func__);
        return;
    }

    if(FALSE != tdlsImplicitTrigger) {
        vos_timer_stop(&pHddTdlsCtx->peerDiscoverTimer);
        vos_timer_destroy(&pHddTdlsCtx->peerDiscoverTimer);
        vos_timer_stop(&pHddTdlsCtx->peerUpdateTimer);
        vos_timer_destroy(&pHddTdlsCtx->peerUpdateTimer);
    }

    wlan_hdd_freeTdlsPeer();

    vos_mem_free(pHddTdlsCtx);
    pHddTdlsCtx = NULL;
}

int wlan_hdd_tdls_set_link_status(u8 *mac, int status)
{
    hddTdlsPeer_t *curr_peer;
    int i;
    u8 key = 0;

    if (NULL == pHddTdlsCtx) return -1;

    for (i = 0; i < 6; i++)
       key ^= mac[i];

    curr_peer = pHddTdlsCtx->peer_list[key];

    if (NULL == curr_peer) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s no matching MAC!", __func__);
        return -1;
    }

    do {
        if (!memcmp(mac, curr_peer->peerMac, 6)) goto found_peer;
        curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
    } while (&curr_peer->node != curr_peer->node.next);

    hddLog(VOS_TRACE_LEVEL_ERROR, "%s no matching MAC!", __func__);
    return -1;

found_peer:

    hddLog(VOS_TRACE_LEVEL_WARN, "tdls set peer %d link status to %d",
            key, status);

    curr_peer->link_status = status;

    if(FALSE != tdlsImplicitTrigger) {
        if (eTDLS_LINK_CONNECTED == status) {
            vos_timer_init(&curr_peer->peerIdleTimer,
                             VOS_TIMER_TYPE_SW,
                             wlan_hdd_tdls_idle_cb,
                             curr_peer);
        }
    }

    return status;
}

int wlan_hdd_tdls_set_cap(u8 *mac, int cap)
{
    hddTdlsPeer_t *curr_peer;
    int i;
    u8 key = 0;

    if (NULL == pHddTdlsCtx) return -1;

    for (i = 0; i < 6; i++)
       key ^= mac[i];

    curr_peer = pHddTdlsCtx->peer_list[key];

    if (NULL == curr_peer) {
        curr_peer = vos_mem_malloc(sizeof(hddTdlsPeer_t));

        if (NULL == curr_peer) {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s peer malloc failed!", __func__);
            return -1;
        }
        vos_mem_zero(curr_peer, sizeof(hddTdlsPeer_t));

        INIT_LIST_HEAD(&curr_peer->node);
        memcpy(curr_peer->peerMac, mac, 6);
        curr_peer->tdls_support = cap;
        pHddTdlsCtx->peer_list[key] = curr_peer;

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "new peer - key:%d, mac:%x %x %x %x %x %x, 0x%x",
                   key, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                   curr_peer );
        return 0;
    }

    do {
        if (!memcmp(mac, curr_peer->peerMac, 6)) goto found_peer;
        curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
    } while (&curr_peer->node != curr_peer->node.next);

    hddLog(VOS_TRACE_LEVEL_ERROR, "%s no matching MAC %d: %x %x %x %x %x %x",
         __func__, key, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return -1;

found_peer:

    hddLog(VOS_TRACE_LEVEL_WARN, "tdls set peer %d support cap to %d",
            key, cap);

    curr_peer->tdls_support = cap;

    return cap;
}

int wlan_hdd_tdls_set_rssi(u8 *mac, tANI_S8 rxRssi)
{
    hddTdlsPeer_t *curr_peer;
    int i;
    u8 key = 0;

    if (NULL == pHddTdlsCtx) return -1;

    for (i = 0; i < 6; i++)
       key ^= mac[i];

    curr_peer = pHddTdlsCtx->peer_list[key];

    if (NULL == curr_peer) {
        curr_peer = vos_mem_malloc(sizeof(hddTdlsPeer_t));

        if (NULL == curr_peer) {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s peer malloc failed!", __func__);
            return -1;
        }
        vos_mem_zero(curr_peer, sizeof(hddTdlsPeer_t));

        INIT_LIST_HEAD(&curr_peer->node);
        memcpy(curr_peer->peerMac, mac, 6);
        curr_peer->rssi = rxRssi;
        pHddTdlsCtx->peer_list[key] = curr_peer;

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "new peer - key:%d, mac:%x %x %x %x %x %x, 0x%x",
                   key, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                   curr_peer );
        return 0;
    }

    do {
        if (!memcmp(mac, curr_peer->peerMac, 6)) goto found_peer;
        curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
    } while (&curr_peer->node != curr_peer->node.next);

    hddLog(VOS_TRACE_LEVEL_ERROR, "%s no matching MAC %d: %x %x %x %x %x %x",
         __func__, key, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return -1;

found_peer:

    hddLog(VOS_TRACE_LEVEL_WARN, "tdls set peer %d rssi to %d",
            key, rxRssi);

    curr_peer->rssi = rxRssi;

    return rxRssi;
}

u8 wlan_hdd_tdls_extract_da(struct sk_buff *skb, u8 *mac)
{
    int i;
    u8 hash = 0;

    memcpy(mac, skb->data, 6);
    for (i = 0; i < 6; i++)
       hash ^= mac[i];

    return hash;
}

u8 wlan_hdd_tdls_extract_sa(struct sk_buff *skb, u8 *mac)
{
    int i;
    u8 hash = 0;

    memcpy(mac, skb->data+6, 6);
    for (i = 0; i < 6; i++)
       hash ^= mac[i];

    return hash;
}

int wlan_hdd_tdls_add_peer_to_list(u8 key, u8 *mac, u8 tx)
{
    hddTdlsPeer_t *new_peer, *curr_peer;

    if (NULL == pHddTdlsCtx) return -1;

    curr_peer = pHddTdlsCtx->peer_list[key];

    if (NULL == curr_peer) {
        curr_peer = vos_mem_malloc(sizeof(hddTdlsPeer_t));

        if (NULL == curr_peer) {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s peer malloc failed!", __func__);
            return -1;
        }
        vos_mem_zero(curr_peer, sizeof(hddTdlsPeer_t));
        curr_peer->rssi = -120;

        INIT_LIST_HEAD(&curr_peer->node);
        memcpy(curr_peer->peerMac, mac, 6);

        pHddTdlsCtx->peer_list[key] = curr_peer;

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                   "new peer - key:%d, mac:%x %x %x %x %x %x, 0x%x",
                   key, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                   curr_peer );
        return 0;
    }

    do {
        if (!memcmp(mac, curr_peer->peerMac, 6)) goto known_peer;
        curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
    } while (&curr_peer->node != curr_peer->node.next);

    new_peer = vos_mem_malloc(sizeof(hddTdlsPeer_t));
    if (NULL == new_peer) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s peer malloc failed!", __func__);
        return -1;
    }
    vos_mem_zero(new_peer, sizeof(hddTdlsPeer_t));
    curr_peer->rssi = -120;
    memcpy(new_peer->peerMac, mac, 6);

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
              "add peer - key:%d, mac:%x %x %x %x %x %x",
              key, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );

    list_add(&new_peer->node, &curr_peer->node);
    curr_peer = new_peer;

known_peer:
    if (tx)
        curr_peer->tx_pkt++;
    else
        curr_peer->rx_pkt++;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
               "known peer - key:%d, mac:%x %x %x %x %x %x, tx:%d",
               key, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
               curr_peer->tx_pkt );

    return 0;
}

int wlan_hdd_tdls_set_params(tdls_config_params_t *config)
{
    if (NULL == pHddTdlsCtx ||
        FALSE == tdlsImplicitTrigger) return -1;

    vos_timer_stop( &pHddTdlsCtx->peerDiscoverTimer);

    vos_timer_stop( &pHddTdlsCtx->peerUpdateTimer);

    memcpy(&pHddTdlsCtx->threshold_config, config, sizeof(tdls_config_params_t));

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "iw set tdls params: %d %d %d %d %d %d",
            pHddTdlsCtx->threshold_config.tx_period_t,
            pHddTdlsCtx->threshold_config.tx_packet_n,
            pHddTdlsCtx->threshold_config.discovery_period_t,
            pHddTdlsCtx->threshold_config.discovery_tries_n,
            pHddTdlsCtx->threshold_config.rx_timeout_t,
            pHddTdlsCtx->threshold_config.rssi_hysteresis);

    vos_timer_start( &pHddTdlsCtx->peerDiscoverTimer,
                        pHddTdlsCtx->threshold_config.discovery_period_t );

    vos_timer_start( &pHddTdlsCtx->peerUpdateTimer,
                        pHddTdlsCtx->threshold_config.tx_period_t );
    return 0;
}

int wlan_hdd_saveTdlsPeer(tCsrRoamInfo *pRoamInfo)
{
    hddTdlsPeer_t *new_peer, *curr_peer;
    int i;
    u8 key = 0;

    if (NULL == pHddTdlsCtx) return -1;

    for (i = 0; i < 6; i++)
       key ^= pRoamInfo->peerMac[i];

    curr_peer = pHddTdlsCtx->peer_list[key];

    if (NULL == curr_peer) {
        curr_peer = vos_mem_malloc(sizeof(hddTdlsPeer_t));

        if (NULL == curr_peer) {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "saveTdlsPeer: NOT saving staId %d", pRoamInfo->staId);
            return -1;
        }
        vos_mem_zero(curr_peer, sizeof(hddTdlsPeer_t));

        INIT_LIST_HEAD(&curr_peer->node);
        memcpy(curr_peer->peerMac, pRoamInfo->peerMac, 6);
        curr_peer->staId = pRoamInfo->staId;
        pHddTdlsCtx->peer_list[key] = curr_peer;

        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     "saveTdlsPeer: saved staId %d", pRoamInfo->staId);
        return 0;
    }

    do {
        if (!memcmp(pRoamInfo->peerMac, curr_peer->peerMac, 6)) goto known_peer;
        curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
    } while (&curr_peer->node != curr_peer->node.next);

    new_peer = vos_mem_malloc(sizeof(hddTdlsPeer_t));
    if (NULL == new_peer) {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "saveTdlsPeer: NOT saving staId %d", pRoamInfo->staId);
        return -1;
    }
    vos_mem_zero(new_peer, sizeof(hddTdlsPeer_t));

    list_add(&new_peer->node, &curr_peer->node);
    curr_peer = new_peer;

known_peer:
    memcpy(curr_peer->peerMac, pRoamInfo->peerMac, 6);
    curr_peer->staId = pRoamInfo->staId;

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                 "saveTdlsPeer: saved staId %d", pRoamInfo->staId);

    return 0;
}

int wlan_hdd_findTdlsPeer(tSirMacAddr peerMac)
{
    int i;
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return -1;

    for (i = 0; i < 256; i++) {
        curr_peer = pHddTdlsCtx->peer_list[i];
        if (NULL == curr_peer) continue;

        do {
            if (!memcmp(peerMac, curr_peer->peerMac, 6)) {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                         "findTdlsPeer: found staId %d", curr_peer->staId);
                return curr_peer->staId;
            }

            curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
        } while (&curr_peer->node != curr_peer->node.next);
    }

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
        "findTdlsPeer: staId NOT found");

    return -1;
}

void wlan_hdd_removeTdlsPeer(tCsrRoamInfo *pRoamInfo)
{
    int i;
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return;

    for (i = 0; i < 256; i++) {

        curr_peer = pHddTdlsCtx->peer_list[i];

        if (NULL == curr_peer) continue;

        do {
            if (curr_peer->staId == pRoamInfo->staId) goto found_peer;

            curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
        } while (&curr_peer->node != curr_peer->node.next);
    }

    if (i == 256) return;

found_peer:
    curr_peer->link_status = eTDLS_LINK_NOT_CONNECTED;
    curr_peer->staId = 0;
    curr_peer->rssi = -120;
    if(FALSE != tdlsImplicitTrigger) {
        vos_timer_stop( &curr_peer->peerIdleTimer );
        vos_timer_destroy( &curr_peer->peerIdleTimer );
    }
}

void wlan_hdd_freeTdlsPeer(void)
{
    int i;
    hddTdlsPeer_t *curr_peer;
    hddTdlsPeer_t *temp_peer;

    if (NULL == pHddTdlsCtx) return;

    for (i = 0; i < 256; i++) {

        curr_peer = pHddTdlsCtx->peer_list[i];

        if (NULL != curr_peer) {
            while (&curr_peer->node != curr_peer->node.next) {
                temp_peer = curr_peer;
                curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
                vos_mem_free(temp_peer);
            }
            vos_mem_free(curr_peer);
        }
    }
}

/* TODO: Currently I am using conn_info.staId
   here as per current design but tdls.c shouldn't
   have touch this.*/
u8 wlan_hdd_tdlsConnectedPeers(void)
{
    hdd_adapter_t *pAdapter;
    hdd_station_ctx_t *pHddStaCtx;
    u8 staIdx;
    u8 count = 0;

    if (NULL == pHddTdlsCtx) return -1;

    pAdapter = WLAN_HDD_GET_PRIV_PTR(pHddTdlsCtx->dev);
    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

    for ( staIdx = 0 ; staIdx < HDD_MAX_NUM_TDLS_STA; staIdx++ )
    {
        if (0 != pHddStaCtx->conn_info.staId[staIdx] )
            count++;
    }
    return count;
}

int wlan_hdd_tdls_get_all_peers(char *buf, int buflen)
{
    int i;
    int len, init_len;
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) {
        len = snprintf(buf, buflen, "TDLS not enabled\n");
        return len;
    }

    init_len = buflen;
    len = snprintf(buf, buflen, "\n%-18s%-3s%-4s%-3s%-5s\n", "MAC", "Id", "cap", "up", "RSSI");
    buf += len;
    buflen -= len;
    /*                           1234567890123456789012345678901234567 */
    len = snprintf(buf, buflen, "---------------------------------\n");
    buf += len;
    buflen -= len;

    for (i = 0; i < 256; i++) {

        curr_peer = pHddTdlsCtx->peer_list[i];

        if (NULL != curr_peer) {
            do {
                if(buflen < 32+1)
                    break;
                len = snprintf(buf, buflen,
                    "%02x:%02x:%02x:%02x:%02x:%02x%3d%4s%3s%5d\n",
                    curr_peer->peerMac[0], curr_peer->peerMac[1],
                    curr_peer->peerMac[2], curr_peer->peerMac[3],
                    curr_peer->peerMac[4], curr_peer->peerMac[5],
                    curr_peer->staId,
                    (curr_peer->tdls_support == eTDLS_CAP_SUPPORTED) ? "Y":"N",
                    (curr_peer->link_status == eTDLS_LINK_CONNECTED) ? "Y":"N",
                    curr_peer->rssi);
                buf += len;
                buflen -= len;
                curr_peer = (hddTdlsPeer_t *)curr_peer->node.next;
            } while (&curr_peer->node != curr_peer->node.next);
        }
    }
    return init_len-buflen;
}
