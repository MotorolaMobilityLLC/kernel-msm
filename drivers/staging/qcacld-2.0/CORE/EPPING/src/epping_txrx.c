/*
 * Copyright (c) 2014-2016 The Linux Foundation. All rights reserved.
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

/*========================================================================

  \file  epping_main.c

  \brief WLAN End Point Ping test tool implementation

  ========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <wlan_hdd_includes.h>
#include <vos_api.h>
#include <vos_sched.h>
#include <linux/etherdevice.h>
#include <linux/firmware.h>
#include <wcnss_api.h>
#include <wlan_hdd_tx_rx.h>
#include <wniApi.h>
#include <wlan_nlink_srv.h>
#include <wlan_btc_svc.h>
#include <wlan_hdd_cfg.h>
#include <wlan_ptt_sock_svc.h>
#include <wlan_hdd_wowl.h>
#include <wlan_hdd_misc.h>
#include <wlan_hdd_wext.h>
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include "vos_cnss.h"
#include <linux/rtnetlink.h>
#include <linux/semaphore.h>
#include <linux/ctype.h>
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_softap_tx_rx.h>
#include "epping_main.h"
#include "epping_internal.h"

static int epping_start_adapter(epping_adapter_t *pAdapter);
static void epping_stop_adapter(epping_adapter_t *pAdapter);

static void epping_timer_expire(void *data)
{
   struct net_device *dev = (struct net_device *) data;
   epping_adapter_t *pAdapter;

   if (dev == NULL) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: netdev = NULL", __func__);
      return;
   }

   pAdapter = netdev_priv(dev);
   if (pAdapter == NULL) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: adapter = NULL", __func__);
      return;
   }
   pAdapter->epping_timer_state = EPPING_TX_TIMER_STOPPED;
   epping_tx_timer_expire(pAdapter);
}

static int epping_ndev_open(struct net_device *dev)
{
   epping_adapter_t *pAdapter;
   int ret = 0;

   pAdapter = netdev_priv(dev);
   epping_start_adapter(pAdapter);
   return ret;
}

static int epping_ndev_stop(struct net_device *dev)
{
   epping_adapter_t *pAdapter;
   int ret = 0;

   pAdapter = netdev_priv(dev);
   if (NULL == pAdapter) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: EPPING adapter context is Null", __func__);
      ret = -ENODEV;
      goto end;
   }
   epping_stop_adapter(pAdapter);
end:
   return ret;
}

static void epping_ndev_uninit (struct net_device *dev)
{
   epping_adapter_t *pAdapter;

   pAdapter = netdev_priv(dev);
   if (NULL == pAdapter) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: EPPING adapter context is Null", __func__);
      goto end;
   }
   epping_stop_adapter(pAdapter);
end:
   return;
}

void epping_tx_queue_timeout(struct net_device *dev)
{
   epping_adapter_t *pAdapter;

   pAdapter = netdev_priv(dev);
   if (NULL == pAdapter) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: EPPING adapter context is Null", __func__);
      goto end;
   }

   EPPING_LOG(VOS_TRACE_LEVEL_ERROR,
      "%s: Transmission timeout occurred, pAdapter->started= %d",
      __func__, pAdapter->started);

   /* Getting here implies we disabled the TX queues
    * for too long. Since this is epping
    * (not because of disassociation or low resource scenarios),
    * try to restart the queue
    */
   if (pAdapter->started)
      netif_wake_queue(dev);
end:
   return;

}

int epping_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
   epping_adapter_t *pAdapter;
   int ret = 0;

   pAdapter = netdev_priv(dev);
   if (NULL == pAdapter) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: EPPING adapter context is Null", __func__);
      ret = -ENODEV;
      goto end;
   }
   ret = epping_tx_send(skb, pAdapter);
end:
   return ret;
}

struct net_device_stats* epping_get_stats(struct net_device *dev)
{
   epping_adapter_t *pAdapter =  netdev_priv(dev);

   if ( NULL == pAdapter )
   {
      EPPING_LOG(VOS_TRACE_LEVEL_ERROR, "%s: pAdapter = NULL", __func__);
      return NULL;
   }

   return &pAdapter->stats;
}

int epping_ndev_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
   epping_adapter_t *pAdapter;
   int ret = 0;

   pAdapter = netdev_priv(dev);
   if (NULL == pAdapter) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
                 "%s: EPPING adapter context is Null", __func__);
      ret = -ENODEV;
      goto end;
   }
   if (dev != pAdapter->dev) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
                 "%s: HDD adapter/dev inconsistency", __func__);
      ret = -ENODEV;
      goto end;
   }

   if ((!ifr) || (!ifr->ifr_data)) {
      ret = -EINVAL;
      goto end;
   }


   switch (cmd) {
   case (SIOCDEVPRIVATE + 1):
      EPPING_LOG(VOS_TRACE_LEVEL_ERROR,
         "%s: do not support ioctl %d (SIOCDEVPRIVATE + 1)",
         __func__, cmd);
      break;
   default:
      EPPING_LOG(VOS_TRACE_LEVEL_ERROR, "%s: unknown ioctl %d",
             __func__, cmd);
      ret = -EINVAL;
      break;
   }

end:
   return ret;
}

static int epping_set_mac_address(struct net_device *dev, void *addr)
{
   epping_adapter_t *pAdapter = netdev_priv(dev);
   struct sockaddr *psta_mac_addr = addr;
   vos_mem_copy(&pAdapter->macAddressCurrent,
                psta_mac_addr->sa_data, ETH_ALEN);
   vos_mem_copy(dev->dev_addr, psta_mac_addr->sa_data, ETH_ALEN);
   return 0;
}

static void epping_stop_adapter(epping_adapter_t *pAdapter)
{
   struct device *dev;

   if (pAdapter && pAdapter->started) {
      EPPING_LOG(LOG1, FL("Disabling queues"));
      netif_tx_disable(pAdapter->dev);
      netif_carrier_off(pAdapter->dev);
      pAdapter->started = false;
      dev = pAdapter->pEpping_ctx->parent_dev;
      if (dev)
         vos_request_bus_bandwidth(dev, CNSS_BUS_WIDTH_LOW);
   }
}

static int epping_start_adapter(epping_adapter_t *pAdapter)
{
   struct device *dev;

   if (!pAdapter) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: pAdapter= NULL\n", __func__);
      return -1;
   }
   if (!pAdapter->started) {
      dev = pAdapter->pEpping_ctx->parent_dev;
      if (dev)
         vos_request_bus_bandwidth(dev, CNSS_BUS_WIDTH_HIGH);
      netif_carrier_on(pAdapter->dev);
      EPPING_LOG(LOG1, FL("Enabling queues"));
      netif_tx_start_all_queues(pAdapter->dev);
      pAdapter->started = true;
   } else {
      EPPING_LOG(VOS_TRACE_LEVEL_WARN,
         "%s: pAdapter %p already started\n", __func__, pAdapter);
   }
   return 0;
}
static int epping_register_adapter(epping_adapter_t *pAdapter)
{
   int ret = 0;

   if ((ret = register_netdev(pAdapter->dev)) != 0) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: unable to register device\n", pAdapter->dev->name);
   } else {
      pAdapter->registered = true;
   }
   return ret;
}

static void epping_unregister_adapter(epping_adapter_t *pAdapter)
{
   if (pAdapter) {
      epping_stop_adapter(pAdapter);
      if (pAdapter->registered) {
         unregister_netdev(pAdapter->dev);
         pAdapter->registered = false;
      }
   } else {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: pAdapter = NULL, unable to unregister device\n",
         __func__);
   }
}

void epping_destroy_adapter(epping_adapter_t *pAdapter)
{
   struct net_device *dev = NULL;
   epping_context_t *pEpping_ctx;

   if (!pAdapter || !pAdapter->pEpping_ctx) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: pAdapter = NULL\n", __func__);
     return;
   }

   dev = pAdapter->dev;
   pEpping_ctx= pAdapter->pEpping_ctx;
   epping_unregister_adapter(pAdapter);

   adf_os_spinlock_destroy(&pAdapter->data_lock);
   adf_os_timer_free(&pAdapter->epping_timer);
   pAdapter->epping_timer_state = EPPING_TX_TIMER_STOPPED;

   while (adf_nbuf_queue_len(&pAdapter->nodrop_queue)) {
      adf_nbuf_t tmp_nbuf = NULL;
      tmp_nbuf = adf_nbuf_queue_remove(&pAdapter->nodrop_queue);
      if (tmp_nbuf)
         adf_nbuf_free(tmp_nbuf);
   }

   free_netdev(dev);
   if (!pEpping_ctx)
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: pEpping_ctx = NULL\n", __func__);
   else
      pEpping_ctx->epping_adapter = NULL;
}

static struct net_device_ops epping_drv_ops = {
   .ndo_open = epping_ndev_open,
   .ndo_stop = epping_ndev_stop,
   .ndo_uninit = epping_ndev_uninit,
   .ndo_start_xmit = epping_hard_start_xmit,
   .ndo_tx_timeout = epping_tx_queue_timeout,
   .ndo_get_stats = epping_get_stats,
   .ndo_do_ioctl = epping_ndev_ioctl,
   .ndo_set_mac_address = epping_set_mac_address,
   .ndo_select_queue    = NULL,
 };

#define EPPING_TX_QUEUE_MAX_LEN 128 /* need to be power of 2 */

epping_adapter_t *epping_add_adapter(epping_context_t *pEpping_ctx,
   tSirMacAddr macAddr, device_mode_t device_mode)
{
   struct net_device *dev;
   epping_adapter_t *pAdapter;

   dev = alloc_netdev(sizeof(epping_adapter_t),
                      "wifi%d",
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)) || defined(WITH_BACKPORTS)
                      NET_NAME_UNKNOWN,
#endif
                      ether_setup);
   if (dev == NULL) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: Cannot allocate epping_adapter_t\n", __func__);
      return NULL;
   }

   pAdapter = netdev_priv(dev);
   vos_mem_zero(pAdapter, sizeof(*pAdapter));
   pAdapter->dev = dev;
   pAdapter->pEpping_ctx = pEpping_ctx;
   pAdapter->device_mode = device_mode; /* station, SAP, etc */
   vos_mem_copy(dev->dev_addr, (void *)macAddr, sizeof(tSirMacAddr));
   vos_mem_copy(pAdapter->macAddressCurrent.bytes,
                macAddr, sizeof(tSirMacAddr));
   adf_os_spinlock_init(&pAdapter->data_lock);
   adf_nbuf_queue_init(&pAdapter->nodrop_queue);
   pAdapter->epping_timer_state = EPPING_TX_TIMER_STOPPED;
   adf_os_timer_init(epping_get_adf_ctx(), &pAdapter->epping_timer,
      epping_timer_expire, dev, ADF_DEFERRABLE_TIMER);
   dev->type = ARPHRD_IEEE80211;
   dev->netdev_ops = &epping_drv_ops;
   dev->watchdog_timeo = 5 * HZ;           /* XXX */
   dev->tx_queue_len = ATH_TXBUF-1;        /* 1 for mgmt frame */
   if (epping_register_adapter(pAdapter) == 0) {
      EPPING_LOG(LOG1, FL("Disabling queues"));
      netif_tx_disable(dev);
      netif_carrier_off(dev);
      return pAdapter;
   } else {
      epping_destroy_adapter(pAdapter);
      return NULL;
   }
}

int epping_connect_service(epping_context_t *pEpping_ctx)
{
   int status, i;
   HTC_SERVICE_CONNECT_REQ connect;
   HTC_SERVICE_CONNECT_RESP response;

   vos_mem_zero(&connect, sizeof(connect));
   vos_mem_zero(&response, sizeof(response));

   /* these fields are the same for all service endpoints */
   connect.EpCallbacks.pContext = pEpping_ctx;
   connect.EpCallbacks.EpTxCompleteMultiple = epping_tx_complete_multiple;
   connect.EpCallbacks.EpRecv = epping_rx;
   /* epping_tx_complete use Multiple version */
   connect.EpCallbacks.EpTxComplete = NULL;
   connect.MaxSendQueueDepth = 64;

#ifdef HIF_SDIO
   connect.EpCallbacks.EpRecvRefill = epping_refill;
   connect.EpCallbacks.EpSendFull =
      epping_tx_queue_full /* ar6000_tx_queue_full */;
#elif defined(HIF_USB) || defined(HIF_PCI)
   connect.EpCallbacks.EpRecvRefill = NULL /* provided by HIF */;
   connect.EpCallbacks.EpSendFull = NULL /* provided by HIF */;
   /* disable flow control for hw flow control */
   connect.ConnectionFlags |= HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL;
#endif

   /* connect to service */
   connect.ServiceID = WMI_DATA_BE_SVC;
   status = HTCConnectService(pEpping_ctx->HTCHandle, &connect, &response);
   if (status != EOK) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "Failed to connect to Endpoint Ping BE service status:%d \n",
         status);
      return -1;;
   } else {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "eppingtest BE endpoint:%d\n", response.Endpoint);
   }
   pEpping_ctx->EppingEndpoint[0] = response.Endpoint;

#if defined(HIF_PCI) || defined(HIF_USB)
   connect.ServiceID = WMI_DATA_BK_SVC;
   status = HTCConnectService(pEpping_ctx->HTCHandle,
                              &connect, &response);
   if (status  != EOK)
   {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "Failed to connect to Endpoint Ping BK service status:%d \n",
         status);
      return -1;;
   } else {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "eppingtest BK endpoint:%d\n", response.Endpoint);
   }
   pEpping_ctx->EppingEndpoint[1] = response.Endpoint;
   /* Since we do not create other two SVC use BK endpoint
    * for rest ACs (2, 3) */
   for (i = 2; i < EPPING_MAX_NUM_EPIDS; i++) {
      pEpping_ctx->EppingEndpoint[i] = response.Endpoint;
   }
#else
   /* we only use one endpoint for high latenance bus.
    * Map all AC's EPIDs to the same endpoint ID returned by HTC */
   for (i = 0; i < EPPING_MAX_NUM_EPIDS; i++) {
      pEpping_ctx->EppingEndpoint[i] = response.Endpoint;
   }
#endif
   return 0;
}
