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


static tdlsCtx_t *pHddTdlsCtx;
static struct mutex tdls_lock;
static tANI_S32 wlan_hdd_get_tdls_discovery_peer_cnt(void);
static tANI_S32 wlan_hdd_tdls_peer_reset_discovery_processed(void);
static void wlan_hdd_tdls_timers_destroy(void);

#ifndef WLAN_FEATURE_TDLS_DEBUG
#define TDLS_LOG_LEVEL VOS_TRACE_LEVEL_INFO
#else
#define TDLS_LOG_LEVEL VOS_TRACE_LEVEL_WARN
#endif

static u8 wlan_hdd_tdls_hash_key (u8 *mac)
{
    int i;
    u8 key = 0;

    for (i = 0; i < 6; i++)
       key ^= mac[i];

    return key;
}

static v_VOID_t wlan_hdd_tdls_start_peer_discover_timer(tANI_BOOLEAN mutexLock, v_U32_t discoveryExpiry)
{
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    hdd_station_ctx_t *pHddStaCtx;


    if ( mutexLock )
    {
        if (mutex_lock_interruptible(&tdls_lock))
        {
           VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                     "%s: unable to lock list: %d", __func__, __LINE__);
           return;
        }
    }
    if (NULL == pHddTdlsCtx)
    {
        if ( mutexLock )
            mutex_unlock(&tdls_lock);
        return;
    }

    pAdapter = WLAN_HDD_GET_PRIV_PTR(pHddTdlsCtx->dev);

    if (NULL == pAdapter)
    {
        if ( mutexLock )
            mutex_unlock(&tdls_lock);
        return;
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
    if (hdd_connIsConnected( pHddStaCtx ))
    {
        vos_timer_start(&pHddTdlsCtx->peerDiscoverTimer, discoveryExpiry);
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "beacon rssi: %d",
                   pHddTdlsCtx->ap_rssi);
    }
    if ( mutexLock )
        mutex_unlock(&tdls_lock);

    return;
}

static v_VOID_t wlan_hdd_tdls_discover_peer_cb( v_PVOID_t userData )
{
    int i;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer;
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    hdd_station_ctx_t *pHddStaCtx;
    int discover_req_sent = 0;
    v_U32_t discover_expiry = TDLS_SUB_DISCOVERY_PERIOD;
    tANI_BOOLEAN doMutexLock = eANI_BOOLEAN_TRUE;

    if (mutex_lock_interruptible(&tdls_lock))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: unable to lock list : %d", __func__, __LINE__);
       return;
    }

    if (NULL == pHddTdlsCtx)
    {
        mutex_unlock(&tdls_lock);
        return;
    }

    pAdapter = WLAN_HDD_GET_PRIV_PTR(pHddTdlsCtx->dev);

    if (NULL == pAdapter)
    {
        mutex_unlock(&tdls_lock);
        return;
    }

    pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
    pHddCtx = WLAN_HDD_GET_CTX( pAdapter );

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: ", __func__);

    if (0 == pHddTdlsCtx->discovery_peer_cnt)
        pHddTdlsCtx->discovery_peer_cnt = wlan_hdd_get_tdls_discovery_peer_cnt();

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];

        list_for_each (pos, head) {
            curr_peer = list_entry (pos, hddTdlsPeer_t, node);

            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       "%d " MAC_ADDRESS_STR " %d %d, %d %d %d %d", i,
                       MAC_ADDR_ARRAY(curr_peer->peerMac),
                       curr_peer->discovery_processed,
                       discover_req_sent,
                       curr_peer->tdls_support,
                       curr_peer->link_status,
                       curr_peer->discovery_attempt,
                       pHddTdlsCtx->threshold_config.discovery_tries_n);

            if (discover_req_sent < TDLS_MAX_DISCOVER_REQS_PER_TIMER) {
                if (!curr_peer->discovery_processed) {

                    curr_peer->discovery_processed = 1;
                    discover_req_sent++;
                    pHddTdlsCtx->discovery_peer_cnt--;

                    if ((eTDLS_CAP_UNKNOWN == curr_peer->tdls_support) &&
                        (eTDLS_LINK_NOT_CONNECTED == curr_peer->link_status) &&
                         (curr_peer->tx_pkt >=
                             pHddTdlsCtx->threshold_config.tx_packet_n)) {

                        if (curr_peer->discovery_attempt <
                            pHddTdlsCtx->threshold_config.discovery_tries_n) {

                            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                      "sme_SendTdlsMgmtFrame(%d)", pAdapter->sessionId);

                            sme_SendTdlsMgmtFrame(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                                  pAdapter->sessionId,
                                                  curr_peer->peerMac,
                                                  WLAN_TDLS_DISCOVERY_REQUEST,
                                                  1, 0, NULL, 0, 0);
                            curr_peer->discovery_attempt++;
                        }
                        else
                        {
                           VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                                     "%s: Maximum Discovery retries reached", __func__);
                           curr_peer->tdls_support = eTDLS_CAP_NOT_SUPPORTED;
                        }

                   }
                }
            }
            else
                goto exit_loop;
        }
    }
exit_loop:

    if (0 != pHddTdlsCtx->discovery_peer_cnt) {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                  "discovery_peer_cnt is %d , Starting SUB_DISCOVERY_TIMER",
                  pHddTdlsCtx->discovery_peer_cnt);
        discover_expiry = TDLS_SUB_DISCOVERY_PERIOD;
        doMutexLock = eANI_BOOLEAN_FALSE;
        goto done;
    }
    discover_expiry = pHddTdlsCtx->threshold_config.discovery_period_t;

    mutex_unlock(&tdls_lock);

    wlan_hdd_tdls_peer_reset_discovery_processed();

    /* Commenting out the following function as it was introducing
     * a race condition when pHddTdlsCtx is deleted. Also , this
     * function is consuming more time in the timer callback.
     * RSSI based trigger needs to revisit this part of the code.
     */

    /*
     * wlan_hdd_get_rssi(pAdapter, &pHddTdlsCtx->ap_rssi);
     */

done:
    wlan_hdd_tdls_start_peer_discover_timer(doMutexLock, discover_expiry);

    if ( !doMutexLock )
        mutex_unlock(&tdls_lock);
    return;
}

static v_VOID_t wlan_hdd_tdls_update_peer_cb( v_PVOID_t userData )
{
    int i;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer;


    if (mutex_lock_interruptible(&tdls_lock))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: unable to lock list", __func__);
       return;
    }

    if (NULL == pHddTdlsCtx)
    {
        mutex_unlock(&tdls_lock);
        return;
    }

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];

        list_for_each (pos, head) {
            curr_peer = list_entry (pos, hddTdlsPeer_t, node);

            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                       "hdd update cb - %d: " MAC_ADDRESS_STR " -> %d\n", i,
                       MAC_ADDR_ARRAY(curr_peer->peerMac),
                       curr_peer->tx_pkt);

            if (eTDLS_CAP_SUPPORTED == curr_peer->tdls_support) {
                VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                    "%s: (tx %d, rx %d, config %d) " MAC_ADDRESS_STR " (%d) ",
                       __func__,  curr_peer->tx_pkt, curr_peer->rx_pkt,
                        pHddTdlsCtx->threshold_config.tx_packet_n,
                        MAC_ADDR_ARRAY(curr_peer->peerMac), curr_peer->link_status);

                if (eTDLS_LINK_CONNECTED != curr_peer->link_status) {
                    if (curr_peer->tx_pkt >=
                            pHddTdlsCtx->threshold_config.tx_packet_n) {

                        if (HDD_MAX_NUM_TDLS_STA > wlan_hdd_tdlsConnectedPeers() &&
                            (FALSE == curr_peer->isTDLSInProgress))
                        {

                            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "-> Tput trigger TDLS SETUP");
#ifdef CONFIG_TDLS_IMPLICIT
                            cfg80211_tdls_oper_request(pHddTdlsCtx->dev,
                                                       curr_peer->peerMac,
                                                       NL80211_TDLS_SETUP, FALSE,
                                                       GFP_KERNEL);
#endif
                        }
                        else
                        {
                            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                                      "%s: Skip Tput trigger due to isTDLSInProgress %u or connected_peer_count %d",
                                      __func__, curr_peer->isTDLSInProgress, wlan_hdd_tdlsConnectedPeers() );
                        }
                        goto next_peer;
                    }
#ifdef WLAN_FEATURE_TDLS_DEBUG
                    else  {
                        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL, "-> ignored.");
                    }
#endif
                    if ((((tANI_S32)curr_peer->rssi >
                            (tANI_S32)(pHddTdlsCtx->threshold_config.rssi_hysteresis +
                                pHddTdlsCtx->ap_rssi)) ||
                         ((tANI_S32)(curr_peer->rssi >
                            pHddTdlsCtx->threshold_config.rssi_trigger_threshold))) &&
                         (HDD_MAX_NUM_TDLS_STA > wlan_hdd_tdlsConnectedPeers())
                         && (FALSE == curr_peer->isTDLSInProgress)) {

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
                    if ((tANI_S32)curr_peer->rssi <
                        (tANI_S32)pHddTdlsCtx->threshold_config.rssi_teardown_threshold) {

                                VOS_TRACE( VOS_MODULE_ID_HDD,
                                           VOS_TRACE_LEVEL_WARN,
                                           "Tear down - low RSSI: " MAC_ADDRESS_STR "!",
                                           MAC_ADDR_ARRAY(curr_peer->peerMac));
#ifdef CONFIG_TDLS_IMPLICIT
                        cfg80211_tdls_oper_request(pHddTdlsCtx->dev,
                                                   curr_peer->peerMac,
                                                   NL80211_TDLS_TEARDOWN, FALSE,
                                                   GFP_KERNEL);
#endif
                        goto next_peer;
                    }

                    if ((curr_peer->tx_pkt <
                            pHddTdlsCtx->threshold_config.idle_packet_n) &&
                        (curr_peer->rx_pkt <
                            pHddTdlsCtx->threshold_config.idle_packet_n)) {
                        if (VOS_TIMER_STATE_RUNNING !=
                                vos_timer_getCurrentState(&curr_peer->peerIdleTimer)) {
                            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                                    "Tx/Rx Idle timer start: " MAC_ADDRESS_STR "!",
                                       MAC_ADDR_ARRAY(curr_peer->peerMac));
                            vos_timer_start( &curr_peer->peerIdleTimer,
                                        pHddTdlsCtx->threshold_config.idle_timeout_t );
                        }
                    } else {
                        if (VOS_TIMER_STATE_RUNNING ==
                                vos_timer_getCurrentState(&curr_peer->peerIdleTimer)) {
                            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_WARN,
                                    "Tx/Rx Idle timer stop: " MAC_ADDRESS_STR "!",
                                       MAC_ADDR_ARRAY(curr_peer->peerMac));
                            vos_timer_stop( &curr_peer->peerIdleTimer);
                        }
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
            } else if (eTDLS_CAP_UNKNOWN == curr_peer->tdls_support) {
                if (eTDLS_LINK_CONNECTED != curr_peer->link_status) {
                    if (curr_peer->tx_pkt >=
                            pHddTdlsCtx->threshold_config.tx_packet_n) {
                        hdd_adapter_t *pAdapter;

                        pAdapter = WLAN_HDD_GET_PRIV_PTR(pHddTdlsCtx->dev);

                        if (++curr_peer->discovery_attempt <
                                 pHddTdlsCtx->threshold_config.discovery_tries_n) {
                            curr_peer->tdls_support = eTDLS_CAP_NOT_SUPPORTED;
                            sme_SendTdlsMgmtFrame(WLAN_HDD_GET_HAL_CTX(pAdapter),
                                                  pAdapter->sessionId,
                                                  curr_peer->peerMac,
                                                  WLAN_TDLS_DISCOVERY_REQUEST,
                                                  1, 0, NULL, 0, 0);
                        }

                        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                                "Tput triggering TDLS discovery: " MAC_ADDRESS_STR "!",
                                   MAC_ADDR_ARRAY(curr_peer->peerMac));
                    }
                }
            }

next_peer:
            curr_peer->tx_pkt = 0;
            curr_peer->rx_pkt = 0;
        }
    }

    vos_timer_start( &pHddTdlsCtx->peerUpdateTimer,
                        pHddTdlsCtx->threshold_config.tx_period_t );
    mutex_unlock(&tdls_lock);
}

static v_VOID_t wlan_hdd_tdls_idle_cb( v_PVOID_t userData )
{
#ifdef CONFIG_TDLS_IMPLICIT
    hddTdlsPeer_t *curr_peer = (hddTdlsPeer_t *)userData;


    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
               "%s: Tx/Rx Idle " MAC_ADDRESS_STR " trigger teardown",
               __func__,
               MAC_ADDR_ARRAY(curr_peer->peerMac));
    if (mutex_lock_interruptible(&tdls_lock))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: unable to lock list", __func__);
       return;
    }

    cfg80211_tdls_oper_request(pHddTdlsCtx->dev,
                               curr_peer->peerMac,
                               NL80211_TDLS_TEARDOWN,
                               eSIR_MAC_TDLS_TEARDOWN_UNSPEC_REASON,
                               GFP_KERNEL);
    mutex_unlock(&tdls_lock);
#endif
}

static void wlan_hdd_tdls_free_list(void)
{
    int i;
    struct list_head *head;
    hddTdlsPeer_t *tmp;
    struct list_head *pos, *q;

    if (NULL == pHddTdlsCtx) return;


    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each_safe (pos, q, head) {
            tmp = list_entry(pos, hddTdlsPeer_t, node);
            list_del(pos);
            vos_mem_free(tmp);
        }
    }
}

static int wlan_hdd_tdls_core_init(struct net_device *dev, tdls_config_params_t *config)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );
    int i;

    if (NULL == pHddTdlsCtx) {
        pHddTdlsCtx = vos_mem_malloc(sizeof(tdlsCtx_t));

        if (NULL == pHddTdlsCtx) {
            hddLog(VOS_TRACE_LEVEL_ERROR, "%s malloc failed!", __func__);
            return -1;
        }

        vos_mem_zero(pHddTdlsCtx, sizeof(tdlsCtx_t));

        if (NULL == config) {
            if (eTDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY != pHddCtx->tdls_mode) {
                pHddTdlsCtx->threshold_config.tx_period_t =
                    pHddCtx->cfg_ini->fTDLSTxStatsPeriod;
                pHddTdlsCtx->threshold_config.tx_packet_n =
                    pHddCtx->cfg_ini->fTDLSTxPacketThreshold;
                pHddTdlsCtx->threshold_config.discovery_period_t =
                    pHddCtx->cfg_ini->fTDLSDiscoveryPeriod;
                pHddTdlsCtx->threshold_config.discovery_tries_n =
                    pHddCtx->cfg_ini->fTDLSMaxDiscoveryAttempt;
                pHddTdlsCtx->threshold_config.idle_timeout_t =
                    pHddCtx->cfg_ini->fTDLSIdleTimeout;
                pHddTdlsCtx->threshold_config.idle_packet_n =
                    pHddCtx->cfg_ini->fTDLSIdlePacketThreshold;
                pHddTdlsCtx->threshold_config.rssi_hysteresis =
                    pHddCtx->cfg_ini->fTDLSRSSIHysteresis;
                pHddTdlsCtx->threshold_config.rssi_trigger_threshold =
                    pHddCtx->cfg_ini->fTDLSRSSITriggerThreshold;
                pHddTdlsCtx->threshold_config.rssi_teardown_threshold =
                    pHddCtx->cfg_ini->fTDLSRSSITeardownThreshold;
            }
        } else {
            memcpy(&pHddTdlsCtx->threshold_config, config,
                sizeof(tdls_config_params_t));
        }

        for (i = 0; i < 256; i++)
        {
            INIT_LIST_HEAD(&pHddTdlsCtx->peer_list[i]);
        }

        pHddTdlsCtx->dev = dev;
    }

    vos_timer_init(&pHddTdlsCtx->peerDiscoverTimer,
            VOS_TIMER_TYPE_SW,
            wlan_hdd_tdls_discover_peer_cb,
            pHddTdlsCtx);

    vos_timer_init(&pHddTdlsCtx->peerUpdateTimer,
            VOS_TIMER_TYPE_SW,
            wlan_hdd_tdls_update_peer_cb,
            pHddTdlsCtx);

    if (eTDLS_SUPPORT_ENABLED == pHddCtx->tdls_mode) {
        vos_timer_start( &pHddTdlsCtx->peerDiscoverTimer,
                   pHddTdlsCtx->threshold_config.discovery_period_t );

        vos_timer_start( &pHddTdlsCtx->peerUpdateTimer,
                            pHddTdlsCtx->threshold_config.tx_period_t );
    }

    return 0;
}

int wlan_hdd_tdls_init(struct net_device *dev)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );

    mutex_init(&tdls_lock);

    if (FALSE == pHddCtx->cfg_ini->fEnableTDLSSupport)
    {
        pHddCtx->tdls_mode = eTDLS_SUPPORT_DISABLED;
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s TDLS not enabled!", __func__);
        return 0;
    }

    pHddCtx->tdls_mode = eTDLS_SUPPORT_ENABLED;

    if (FALSE == pHddCtx->cfg_ini->fEnableTDLSImplicitTrigger)
    {
        pHddCtx->tdls_mode = eTDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY;
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s TDLS Implicit trigger not enabled!", __func__);
    }

    return wlan_hdd_tdls_core_init(dev, NULL);
}

void wlan_hdd_tdls_exit()
{
    if (mutex_lock_interruptible(&tdls_lock))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: unable to lock list", __func__);
       return;
    }
    if (NULL == pHddTdlsCtx)
    {
        mutex_unlock(&tdls_lock);
        hddLog(VOS_TRACE_LEVEL_WARN, "%s TDLS not enabled, exiting!", __func__);
        return;
    }

    /* must stop timer here before freeing peer list, because peerIdleTimer is
    part of peer list structure. */
    wlan_hdd_tdls_timers_destroy();
    wlan_hdd_tdls_free_list();

    vos_mem_free(pHddTdlsCtx);
    pHddTdlsCtx = NULL;
    mutex_unlock(&tdls_lock);
}

/* stop all the tdls timers running */
static void wlan_hdd_tdls_timers_stop(void)
{
    int i;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer;

    vos_timer_stop(&pHddTdlsCtx->peerDiscoverTimer);
    vos_timer_stop(&pHddTdlsCtx->peerUpdateTimer);


    for (i = 0; i < 256; i++)
    {
        head = &pHddTdlsCtx->peer_list[i];

        list_for_each (pos, head) {
            curr_peer = list_entry (pos, hddTdlsPeer_t, node);

            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       "%s: " MAC_ADDRESS_STR " -> stop idle timer",
                       __func__,
                       MAC_ADDR_ARRAY(curr_peer->peerMac));
            vos_timer_stop ( &curr_peer->peerIdleTimer );
        }
    }
}

/* destroy all the tdls timers running */
static void wlan_hdd_tdls_timers_destroy(void)
{
    int i;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer;

    vos_timer_stop(&pHddTdlsCtx->peerDiscoverTimer);
    vos_timer_destroy(&pHddTdlsCtx->peerDiscoverTimer);
    vos_timer_stop(&pHddTdlsCtx->peerUpdateTimer);
    vos_timer_destroy(&pHddTdlsCtx->peerUpdateTimer);

    for (i = 0; i < 256; i++)
    {
        head = &pHddTdlsCtx->peer_list[i];

        list_for_each (pos, head) {
            curr_peer = list_entry (pos, hddTdlsPeer_t, node);

            VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                       "%s: " MAC_ADDRESS_STR " -> destroy idle timer",
                       __func__,
                       MAC_ADDR_ARRAY(curr_peer->peerMac));
            vos_timer_stop ( &curr_peer->peerIdleTimer );
            vos_timer_destroy ( &curr_peer->peerIdleTimer );
        }
    }
}

/* if mac address exist, return pointer
   if mac address doesn't exist, create a list and add, return pointer
   return NULL if fails to get new mac address
*/
hddTdlsPeer_t *wlan_hdd_tdls_get_peer(u8 *mac)
{
    struct list_head *head;
    hddTdlsPeer_t *peer;
    u8 key;

    if (NULL == pHddTdlsCtx)
        return NULL;

    /* if already there, just update */
    peer = wlan_hdd_tdls_find_peer(mac);
    if (peer != NULL)
    {
        return peer;
    }

    /* not found, allocate and add the list */
    peer = vos_mem_malloc(sizeof(hddTdlsPeer_t));
    if (NULL == peer) {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s peer malloc failed!", __func__);
        return NULL;
    }

    key = wlan_hdd_tdls_hash_key(mac);
    head = &pHddTdlsCtx->peer_list[key];

    if (mutex_lock_interruptible(&tdls_lock))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: unable to lock list", __func__);
       return NULL;
    }

    vos_mem_zero(peer, sizeof(hddTdlsPeer_t));
    vos_mem_copy(peer->peerMac, mac, sizeof(peer->peerMac));

    vos_timer_init(&peer->peerIdleTimer,
                    VOS_TIMER_TYPE_SW,
                    wlan_hdd_tdls_idle_cb,
                    peer);

    list_add_tail(&peer->node, head);
    mutex_unlock(&tdls_lock);

    return peer;
}

void wlan_hdd_tdls_set_link_status(hddTdlsPeer_t *curr_peer, int status)
{
    if (curr_peer == NULL)
        return;

    hddLog(VOS_TRACE_LEVEL_WARN, "tdls set peer " MAC_ADDRESS_STR " link status to %d",
            MAC_ADDR_ARRAY(curr_peer->peerMac), status);

    curr_peer->link_status = status;

}

int wlan_hdd_tdls_set_cap(u8 *mac, int cap)
{
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return -1;

    curr_peer = wlan_hdd_tdls_get_peer(mac);
    if (curr_peer == NULL)
        return -1;

    curr_peer->tdls_support = cap;

    return 0;
}

int wlan_hdd_tdls_set_rssi(u8 *mac, tANI_S8 rxRssi)
{
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return -1;

    curr_peer = wlan_hdd_tdls_get_peer(mac);
    if (curr_peer == NULL)
        return -1;

    curr_peer->rssi = rxRssi;

    return 0;
}

int wlan_hdd_tdls_set_responder(u8 *mac, tANI_U8 responder)
{
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return -1;

    curr_peer = wlan_hdd_tdls_get_peer(mac);
    if (curr_peer == NULL)
        return -1;

    curr_peer->is_responder = responder;

    return 0;
}

int wlan_hdd_tdls_get_responder(u8 *mac)
{
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return -1;

    curr_peer = wlan_hdd_tdls_find_peer(mac);
    if (curr_peer == NULL)
        return -1;

    return (curr_peer->is_responder);
}

int wlan_hdd_tdls_set_signature(u8 *mac, tANI_U8 uSignature)
{
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return -1;

    curr_peer = wlan_hdd_tdls_get_peer(mac);
    if (curr_peer == NULL)
        return -1;

    curr_peer->signature = uSignature;

    return 0;
}


void wlan_hdd_tdls_extract_da(struct sk_buff *skb, u8 *mac)
{
    memcpy(mac, skb->data, 6);
}

void wlan_hdd_tdls_extract_sa(struct sk_buff *skb, u8 *mac)
{
    memcpy(mac, skb->data+6, 6);
}

int wlan_hdd_tdls_increment_pkt_count(u8 *mac, u8 tx)
{
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return -1;

    curr_peer = wlan_hdd_tdls_get_peer(mac);
    if (curr_peer == NULL)
        return -1;

    if (tx)
        curr_peer->tx_pkt++;
    else
        curr_peer->rx_pkt++;

    return 0;
}

int wlan_hdd_tdls_set_params(struct net_device *dev, tdls_config_params_t *config)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );

    if ((eTDLS_SUPPORT_DISABLED == config->tdls) &&
        ((NULL == pHddTdlsCtx) ||
         (eTDLS_SUPPORT_DISABLED == pHddCtx->tdls_mode))) return -1;

    if (eTDLS_SUPPORT_ENABLED == pHddCtx->tdls_mode) {
        vos_timer_stop( &pHddTdlsCtx->peerDiscoverTimer);
        vos_timer_stop( &pHddTdlsCtx->peerUpdateTimer);
    }

    if (eTDLS_SUPPORT_DISABLED != pHddCtx->tdls_mode) {
        memcpy(&pHddTdlsCtx->threshold_config, config, sizeof(tdls_config_params_t));
    }

    wlan_hdd_tdls_peer_reset_discovery_processed();

    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
            "iw set tdls params: %d %d %d %d %d %d %d %d %d %d",
            config->tdls,
            config->tx_period_t,
            config->tx_packet_n,
            config->discovery_period_t,
            config->discovery_tries_n,
            config->idle_timeout_t,
            config->idle_packet_n,
            config->rssi_hysteresis,
            config->rssi_trigger_threshold,
            config->rssi_teardown_threshold);

    pHddCtx->tdls_mode = config->tdls;

    if (eTDLS_SUPPORT_DISABLED == config->tdls) {
        wlan_hdd_tdls_exit();
    } else {
        wlan_hdd_tdls_core_init(dev, config);
    }

    return 0;
}

int wlan_hdd_tdls_set_sta_id(u8 *mac, u8 staId)
{
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return -1;

    curr_peer = wlan_hdd_tdls_get_peer(mac);
    if (curr_peer == NULL)
        return -1;

    curr_peer->staId = staId;

    return 0;
}

/* if peerMac is found, then it returns pointer to hddTdlsPeer_t
   otherwise, it returns NULL
*/
hddTdlsPeer_t *wlan_hdd_tdls_find_peer(u8 *mac)
{
    u8 key;
    struct list_head *pos;
    struct list_head *head;
    hddTdlsPeer_t *curr_peer;


    if (mutex_lock_interruptible(&tdls_lock))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: unable to lock list", __func__);
       return NULL;
    }
    if (NULL == pHddTdlsCtx)
    {
        mutex_unlock(&tdls_lock);
        return NULL;
    }

    key = wlan_hdd_tdls_hash_key(mac);

    head = &pHddTdlsCtx->peer_list[key];

    list_for_each(pos, head) {
        curr_peer = list_entry (pos, hddTdlsPeer_t, node);
        if (!memcmp(mac, curr_peer->peerMac, 6)) {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                     "findTdlsPeer: found staId %d", curr_peer->staId);
            mutex_unlock(&tdls_lock);
            return curr_peer;
        }
    }

    mutex_unlock(&tdls_lock);
    return NULL;
}

int wlan_hdd_tdls_reset_peer(u8 *mac)
{
    hdd_adapter_t *pAdapter;
    hdd_context_t *pHddCtx;
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return -1;

    pAdapter = WLAN_HDD_GET_PRIV_PTR(pHddTdlsCtx->dev);
    pHddCtx = WLAN_HDD_GET_CTX( pAdapter );

    curr_peer = wlan_hdd_tdls_get_peer(mac);
    if (curr_peer == NULL)
        return -1;

    curr_peer->link_status = eTDLS_LINK_NOT_CONNECTED;
    curr_peer->staId = 0;
    curr_peer->rssi = -120;

    if(eTDLS_SUPPORT_ENABLED == pHddCtx->tdls_mode) {
        vos_timer_stop( &curr_peer->peerIdleTimer );
    }
    return 0;
}

static tANI_S32 wlan_hdd_tdls_peer_reset_discovery_processed(void)
{
    int i;
    struct list_head *head;
    hddTdlsPeer_t *tmp;
    struct list_head *pos, *q;

    if (mutex_lock_interruptible(&tdls_lock))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
        "%s: unable to lock list", __func__);
        return -1;
    }
    if ( NULL == pHddTdlsCtx )
    {
        mutex_unlock(&tdls_lock);
        return -1;
    }

    pHddTdlsCtx->discovery_peer_cnt = 0;

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each_safe (pos, q, head) {
            tmp = list_entry(pos, hddTdlsPeer_t, node);
            tmp->discovery_processed = 0;
        }
    }
    mutex_unlock(&tdls_lock);

    return 0;
}

static tANI_S32 wlan_hdd_get_tdls_discovery_peer_cnt(void)
{
    int i;
    struct list_head *head;
    struct list_head *pos, *q;
    int discovery_peer_cnt=0;
    hddTdlsPeer_t *tmp;

    /*
     * This function expects the callers to acquire the Mutex.
     */

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each_safe (pos, q, head) {
            tmp = list_entry(pos, hddTdlsPeer_t, node);
            VOS_TRACE(VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
                      "%s, %d, " MAC_ADDRESS_STR, __func__, i,
                      MAC_ADDR_ARRAY(tmp->peerMac));
            discovery_peer_cnt++;
        }
    }
    return discovery_peer_cnt;
}

tANI_U16 wlan_hdd_tdlsConnectedPeers(void)
{
    if (NULL == pHddTdlsCtx)
        return 0;

    return pHddTdlsCtx->connected_peer_count;
}

int wlan_hdd_tdls_get_all_peers(char *buf, int buflen)
{
    int i;
    int len, init_len;
    struct list_head *head;
    struct list_head *pos;
    hddTdlsPeer_t *curr_peer;


    init_len = buflen;
    len = snprintf(buf, buflen, "\n%-18s%-3s%-4s%-3s%-5s\n", "MAC", "Id", "cap", "up", "RSSI");
    buf += len;
    buflen -= len;
    /*                           1234567890123456789012345678901234567 */
    len = snprintf(buf, buflen, "---------------------------------\n");
    buf += len;
    buflen -= len;

    if (mutex_lock_interruptible(&tdls_lock))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: unable to lock list", __func__);
       return init_len-buflen;
    }
    if (NULL == pHddTdlsCtx) {
        mutex_unlock(&tdls_lock);
        len = snprintf(buf, buflen, "TDLS not enabled\n");
        return len;
    }
    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];

        list_for_each(pos, head) {
            curr_peer= list_entry (pos, hddTdlsPeer_t, node);

            if (buflen < 32+1)
                break;
            len = snprintf(buf, buflen,
                MAC_ADDRESS_STR"%3d%4s%3s%5d\n",
                MAC_ADDR_ARRAY(curr_peer->peerMac),
                curr_peer->staId,
                (curr_peer->tdls_support == eTDLS_CAP_SUPPORTED) ? "Y":"N",
                (curr_peer->link_status == eTDLS_LINK_CONNECTED) ? "Y":"N",
                curr_peer->rssi);
            buf += len;
            buflen -= len;
        }
    }
    mutex_unlock(&tdls_lock);
    return init_len-buflen;
}

void wlan_hdd_tdls_connection_callback(hdd_adapter_t *pAdapter)
{
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX( pAdapter );

    if (NULL == pHddTdlsCtx) return;

    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,
    "%s, update %d discover %d", __func__,
        pHddTdlsCtx->threshold_config.tx_period_t,
        pHddTdlsCtx->threshold_config.discovery_period_t);

    pHddTdlsCtx->connected_peer_count = 0;

    if (eTDLS_SUPPORT_ENABLED == pHddCtx->tdls_mode)
    {
       wlan_hdd_tdls_peer_reset_discovery_processed();

       vos_timer_start(&pHddTdlsCtx->peerDiscoverTimer,
                       pHddTdlsCtx->threshold_config.discovery_period_t);


       vos_timer_start(&pHddTdlsCtx->peerUpdateTimer,
                       pHddTdlsCtx->threshold_config.tx_period_t);
    }

}

void wlan_hdd_tdls_disconnection_callback(hdd_adapter_t *pAdapter)
{

    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,"%s", __func__);

    if (mutex_lock_interruptible(&tdls_lock))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: unable to lock list", __func__);
       return;
    }
    if (NULL == pHddTdlsCtx)
    {
        mutex_unlock(&tdls_lock);
        return;
    }


    wlan_hdd_tdls_timers_stop();
    wlan_hdd_tdls_free_list();

    mutex_unlock(&tdls_lock);
}

void wlan_hdd_tdls_mgmt_completion_callback(hdd_adapter_t *pAdapter, tANI_U32 statusCode)
{
    pAdapter->mgmtTxCompletionStatus = statusCode;
    VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,"%s: Mgmt TX Completion %d",
               __func__, statusCode);
    complete(&pAdapter->tdls_mgmt_comp);
}

void wlan_hdd_tdls_increment_peer_count(void)
{
    if (NULL == pHddTdlsCtx) return;

    pHddTdlsCtx->connected_peer_count++;
}

void wlan_hdd_tdls_decrement_peer_count(void)
{
    if (NULL == pHddTdlsCtx) return;

    if (pHddTdlsCtx->connected_peer_count)
        pHddTdlsCtx->connected_peer_count--;
}

void wlan_hdd_tdls_check_bmps(hdd_context_t *pHddCtx)
{
    if (NULL == pHddTdlsCtx) return;

    if (0 == pHddTdlsCtx->connected_peer_count)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,"No TDLS peer connected so Enable BMPS");
        hdd_enable_bmps_imps(pHddCtx);
    }
    else if (1 == pHddTdlsCtx->connected_peer_count)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, TDLS_LOG_LEVEL,"TDLS peer connected so Disable BMPS");
        hdd_disable_bmps_imps(pHddCtx, WLAN_HDD_INFRA_STATION);
    }
    return;
}

/* return TRUE if TDLS is ongoing
 * mac - if NULL check for all the peer list, otherwise, skip this mac when skip_self is TRUE
 * skip_self - if TRUE, skip this mac. otherwise, check all the peer list. if
   mac is NULL, this argument is ignored, and check for all the peer list.
 */
u8 wlan_hdd_tdls_is_progress(u8 *mac, u8 skip_self)
{
    int i;
    struct list_head *head;
    hddTdlsPeer_t *curr_peer;
    struct list_head *pos;

    if (mutex_lock_interruptible(&tdls_lock))
    {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                 "%s: unable to lock list", __func__);
       return FALSE;
    }
    if (NULL == pHddTdlsCtx)
    {
        mutex_unlock(&tdls_lock);
        return FALSE;
    }

    for (i = 0; i < 256; i++) {
        head = &pHddTdlsCtx->peer_list[i];
        list_for_each(pos, head) {
            curr_peer = list_entry (pos, hddTdlsPeer_t, node);
            if (skip_self && mac && !memcmp(mac, curr_peer->peerMac, 6)) {
                continue;
            }
            else
            {
                if (curr_peer->isTDLSInProgress)
                {
                  mutex_unlock(&tdls_lock);
                  return TRUE;
                }
            }
        }
    }

    mutex_unlock(&tdls_lock);
    return FALSE;
}

void wlan_hdd_tdls_set_connection_progress(u8* mac, u8 isProgress)
{
    hddTdlsPeer_t *curr_peer;

    if (NULL == pHddTdlsCtx) return;

    curr_peer = wlan_hdd_tdls_find_peer(mac);
    if (curr_peer == NULL)
        return;

    curr_peer->isTDLSInProgress = isProgress;

    return;
}
