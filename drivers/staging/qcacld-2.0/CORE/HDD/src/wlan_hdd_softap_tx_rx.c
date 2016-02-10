/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

/**===========================================================================

  \file  wlan_hdd_softap_tx_rx.c

  \brief Linux HDD Tx/RX APIs

  ==========================================================================*/

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/
#include <linux/semaphore.h>
#include <wlan_hdd_tx_rx.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_dp_utils.h>
#include <wlan_qct_tl.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <vos_types.h>
#include <vos_sched.h>
#include <aniGlobal.h>
#include <halTypes.h>
#include <net/ieee80211_radiotap.h>
#ifdef IPA_OFFLOAD
#include <wlan_hdd_ipa.h>
#endif
/*---------------------------------------------------------------------------
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  Type declarations
  -------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  Function definitions and documentation
  -------------------------------------------------------------------------*/

/**============================================================================
  @brief hdd_softap_flush_tx_queues() - Utility function to flush the TX queues

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
static VOS_STATUS hdd_softap_flush_tx_queues( hdd_adapter_t *pAdapter )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   v_SINT_t i = -1;
   v_U8_t STAId = 0;
   hdd_list_node_t *anchor = NULL;
   skb_list_node_t *pktNode = NULL;
   struct sk_buff *skb = NULL;

   spin_lock_bh( &pAdapter->staInfo_lock );
   for (STAId = 0; STAId < WLAN_MAX_STA_COUNT; STAId++)
   {
      if (FALSE == pAdapter->aStaInfo[STAId].isUsed)
      {
         continue;
      }

      for (i = 0; i < NUM_TX_QUEUES; i ++)
      {
         spin_lock_bh(&pAdapter->aStaInfo[STAId].wmm_tx_queue[i].lock);
         while (true)
         {
            status = hdd_list_remove_front ( &pAdapter->aStaInfo[STAId].wmm_tx_queue[i], &anchor);

            if (VOS_STATUS_E_EMPTY != status)
            {
               //If success then we got a valid packet from some AC
               pktNode = list_entry(anchor, skb_list_node_t, anchor);
               skb = pktNode->skb;
               ++pAdapter->stats.tx_dropped;
               kfree_skb(skb);
               continue;
            }

            //current list is empty
            break;
         }
         pAdapter->aStaInfo[STAId].txSuspended[i] = VOS_FALSE;
         spin_unlock_bh(&pAdapter->aStaInfo[STAId].wmm_tx_queue[i].lock);
      }
      pAdapter->aStaInfo[STAId].vosLowResource = VOS_FALSE;
   }

   spin_unlock_bh( &pAdapter->staInfo_lock );

   return status;
}

#ifdef QCA_LL_TX_FLOW_CT
/**============================================================================
  @brief hdd_softap_tx_resume_timer_expired_handler() - Resume OS TX Q timer
      expired handler for SAP and P2P GO interface.
      If Blocked OS Q is not resumed during timeout period, to prevent
      permanent stall, resume OS Q forcefully for SAP and P2P GO interface.

  @param adapter_context : [in] pointer to vdev adapter

  @return         : NONE
  ===========================================================================*/
void hdd_softap_tx_resume_timer_expired_handler(void *adapter_context)
{
   hdd_adapter_t *pAdapter = (hdd_adapter_t *)adapter_context;

   if (!pAdapter)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: INV ARG", __func__);
      /* INVALID ARG */
      return;
   }

   hddLog(LOG1, FL("Enabling queues"));
   netif_tx_wake_all_queues(pAdapter->dev);
   return;
}

/**============================================================================
  @brief hdd_softap_tx_resume_cb() - Resume OS TX Q.
      Q was stopped due to WLAN TX path low resource condition

  @param adapter_context : [in] pointer to vdev adapter
  @param tx_resume       : [in] TX Q resume trigger

  @return         : NONE
  ===========================================================================*/
void hdd_softap_tx_resume_cb(void *adapter_context,
                        v_BOOL_t tx_resume)
{
   hdd_adapter_t *pAdapter = (hdd_adapter_t *)adapter_context;

   if (!pAdapter)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                "%s: INV ARG", __func__);
      /* INVALID ARG */
      return;
   }

   /* Resume TX  */
   if (VOS_TRUE == tx_resume)
   {
       if(VOS_TIMER_STATE_STOPPED !=
          vos_timer_getCurrentState(&pAdapter->tx_flow_control_timer))
       {
          vos_timer_stop(&pAdapter->tx_flow_control_timer);
       }

       hddLog(LOG1, FL("Enabling queues"));
       netif_tx_wake_all_queues(pAdapter->dev);
       pAdapter->hdd_stats.hddTxRxStats.txflow_unpause_cnt++;
       pAdapter->hdd_stats.hddTxRxStats.is_txflow_paused = FALSE;

   }
#if defined(CONFIG_PER_VDEV_TX_DESC_POOL)
    else if (VOS_FALSE == tx_resume)  /* Pause TX  */
    {
        hddLog(LOG1, FL("Disabling queues"));
        netif_tx_stop_all_queues(pAdapter->dev);
        if (VOS_TIMER_STATE_STOPPED ==
            vos_timer_getCurrentState(&pAdapter->tx_flow_control_timer))
        {
            VOS_STATUS status;
            status = vos_timer_start(&pAdapter->tx_flow_control_timer,
                          WLAN_SAP_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);
            if ( !VOS_IS_STATUS_SUCCESS(status) )
            {
                VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s: Failed to start tx_flow_control_timer", __func__);
            }
            else
            {
                pAdapter->hdd_stats.hddTxRxStats.txflow_timer_cnt++;
            }
        }
        pAdapter->hdd_stats.hddTxRxStats.txflow_pause_cnt++;
        pAdapter->hdd_stats.hddTxRxStats.is_txflow_paused = TRUE;
    }
#endif

   return;
}
#endif /* QCA_LL_TX_FLOW_CT */

/**============================================================================
  @brief hdd_softap_hard_start_xmit() - Function registered with the Linux OS
                                        for transmitting packets.

  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param dev      : [in] pointer to Libra network device

  @return         : NETDEV_TX_OK
  ===========================================================================*/
int __hdd_softap_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
   WLANTL_ACEnumType ac;
   hdd_adapter_t *pAdapter = (hdd_adapter_t *)netdev_priv(dev);
   hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   v_MACADDR_t *pDestMacAddress;
   v_U8_t STAId;
   struct sk_buff *skb_next, *list_head = NULL, *list_tail = NULL;
   void *vdev_handle = NULL, *vdev_temp;
   bool is_update_ac_stats = FALSE;
#ifdef QCA_PKT_PROTO_TRACE
   hdd_context_t *hddCtxt = (hdd_context_t *)pAdapter->pHddCtx;
   v_U8_t proto_type = 0;
#endif /* QCA_PKT_PROTO_TRACE */

   ++pAdapter->hdd_stats.hddTxRxStats.txXmitCalled;
   /* Prevent this function to be called during SSR since TL context may
      not be reinitialized at this time which will lead crash. */
   if (pHddCtx->isLogpInProgress)
   {
       VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO_HIGH,
                 "%s: LOGP in Progress. Ignore!!!", __func__);
       goto drop_list;
   }

   /*
    * If the device is operating on a DFS Channel
    * then check if SAP is in CAC WAIT state and
    * drop the packets. In CAC WAIT state device
    * is expected not to transmit any frames.
    * SAP starts Tx only after the BSS START is
    * done.
    */
   if (pHddApCtx->dfs_cac_block_tx)
   {
       goto drop_list;
   }

   while (skb) {
       skb_next = skb->next;
       pDestMacAddress = (v_MACADDR_t*)skb->data;

       if (vos_is_macaddr_broadcast( pDestMacAddress ) ||
           vos_is_macaddr_group(pDestMacAddress))
       {
           // The BC/MC station ID is assigned during BSS starting phase.
           // SAP will return the station ID used for BC/MC traffic.
           STAId = pHddApCtx->uBCStaId;
       }
       else
       {
           if (VOS_STATUS_SUCCESS !=
               hdd_softap_GetStaId(pAdapter, pDestMacAddress, &STAId)) {
               VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_WARN,
                        "%s: Failed to find the station id", __func__);
               goto drop_pkt;
           }

           if (STAId == HDD_WLAN_INVALID_STA_ID)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_WARN,
                          "%s: Failed to find right station", __func__);
               goto drop_pkt;
           }
           else if (FALSE == pAdapter->aStaInfo[STAId].isUsed )
           {
               VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_WARN,
                    "%s: STA %d is unregistered", __func__, STAId);
               goto drop_pkt;
           }

           if ((WLANTL_STA_CONNECTED !=
                pAdapter->aStaInfo[STAId].tlSTAState) &&
               (WLANTL_STA_AUTHENTICATED !=
                pAdapter->aStaInfo[STAId].tlSTAState) )
           {
               VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_WARN,
                          "%s: Station not connected yet", __func__);
               goto drop_pkt;
           }
           else if(WLANTL_STA_CONNECTED ==
                   pAdapter->aStaInfo[STAId].tlSTAState)
           {
               if(ntohs(skb->protocol) != HDD_ETHERTYPE_802_1_X)
               {
                   VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_WARN,
                   "%s: NON-EAPOL packet in non-Authenticated state", __func__);
                   goto drop_pkt;
               }
           }
       }

       vdev_temp = tlshim_peer_validity(
                     (WLAN_HDD_GET_CTX(pAdapter))->pvosContext, STAId);
       if (!vdev_temp)
           goto drop_pkt;

       vdev_handle = vdev_temp;

#ifdef QCA_LL_TX_FLOW_CT
       if ((pAdapter->hdd_stats.hddTxRxStats.is_txflow_paused != TRUE) &&
            VOS_FALSE ==
              WLANTL_GetTxResource((WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                    pAdapter->sessionId,
                                    pAdapter->tx_flow_low_watermark,
                                    pAdapter->tx_flow_high_watermark_offset)) {
           if ((pAdapter->tx_flow_timer_initialized == TRUE) &&
               (VOS_TIMER_STATE_STOPPED ==
                vos_timer_getCurrentState(&pAdapter->tx_flow_control_timer))) {
               hddLog(LOG1, FL("Disabling queues"));
               netif_tx_stop_all_queues(dev);
               vos_timer_start(&pAdapter->tx_flow_control_timer,
                               WLAN_SAP_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);
               pAdapter->hdd_stats.hddTxRxStats.txflow_timer_cnt++;
               pAdapter->hdd_stats.hddTxRxStats.txflow_pause_cnt++;
               pAdapter->hdd_stats.hddTxRxStats.is_txflow_paused = TRUE;
           }
       }
#endif /* QCA_LL_TX_FLOW_CT */

       //Get TL AC corresponding to Qdisc queue index/AC.
       ac = hdd_QdiscAcToTlAC[skb->queue_mapping];
       ++pAdapter->hdd_stats.hddTxRxStats.txXmitClassifiedAC[ac];

       wlan_hdd_log_eapol(skb,
                          WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED);

#ifdef QCA_PKT_PROTO_TRACE
       if ((hddCtxt->cfg_ini->gEnableDebugLog & VOS_PKT_TRAC_TYPE_EAPOL) ||
           (hddCtxt->cfg_ini->gEnableDebugLog & VOS_PKT_TRAC_TYPE_DHCP))
       {
           /* Proto Trace enabled */
           proto_type = vos_pkt_get_proto_type(skb,
                        hddCtxt->cfg_ini->gEnableDebugLog, 0);
           if (VOS_PKT_TRAC_TYPE_EAPOL & proto_type)
           {
               vos_pkt_trace_buf_update("HA:T:EPL");
           }
           else if (VOS_PKT_TRAC_TYPE_DHCP & proto_type)
           {
               vos_pkt_trace_buf_update("HA:T:DHC");
           }
       }
#endif /* QCA_PKT_PROTO_TRACE */

       pAdapter->stats.tx_bytes += skb->len;
       ++pAdapter->stats.tx_packets;

       if (!list_head) {
           list_head = skb;
           list_tail = skb;
       } else {
           list_tail->next = skb;
           list_tail = list_tail->next;
       }
       skb = skb_next;
       continue;

drop_pkt:
       hdd_drop_skb(pAdapter, skb);
       skb = skb_next;

   } /* end of while */

   if (!vdev_handle) {
       VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
                 "%s: All packets dropped in the list", __func__);
       return NETDEV_TX_OK;
   }

   list_tail->next = NULL;

   skb = WLANTL_SendSTA_DataFrame((WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                   vdev_handle, list_head
#ifdef QCA_PKT_PROTO_TRACE
                                 , proto_type
#endif /* QCA_PKT_PROTO_TRACE */
                                 );
   if (skb != NULL) {
       VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_WARN,
                  "%s: Failed to send packet to txrx",
                  __func__);
       is_update_ac_stats = TRUE;
       goto drop_list;
   }

   dev->trans_start = jiffies;
   return NETDEV_TX_OK;

drop_list:
   hdd_drop_skb_list(pAdapter, skb, is_update_ac_stats);
   return NETDEV_TX_OK;

}

int hdd_softap_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __hdd_softap_hard_start_xmit(skb, dev);
	vos_ssr_unprotect(__func__);
	return ret;
}

/**
 * __hdd_softap_tx_timeout() - softap tx timeout
 * @dev: pointer to net_device
 *
 * Function called by OS if there is any timeout during transmission.
 * Since HDD simply enqueues packet and returns control to OS right away,
 * this would never be invoked.
 *
 * Return: none
 */
static void __hdd_softap_tx_timeout(struct net_device *dev)
{
   hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_context_t *hdd_ctx;

   VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
      "%s: Transmission timeout occurred", __func__);

   hdd_ctx = WLAN_HDD_GET_CTX(adapter);
   if (hdd_ctx->isLogpInProgress) {
       VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
                 "%s: LOGP in Progress. Ignore!!!", __func__);
       return;
   }

   /*
    * Getting here implies we disabled the TX queues for too long. Queues are
    * disabled either because of disassociation or low resource scenarios. In
    * case of disassociation it is ok to ignore this. But if associated, we have
    * do possible recovery here.
    */
}

/**
 * hdd_softap_tx_timeout() - SSR wrapper for __hdd_softap_tx_timeout
 * @dev: pointer to net_device
 *
 * Return: none
 */
void hdd_softap_tx_timeout(struct net_device *dev)
{
	vos_ssr_protect(__func__);
	__hdd_softap_tx_timeout(dev);
	vos_ssr_unprotect(__func__);
}

/**
 * __hdd_softap_stats() - get softap tx/rx stats
 * @dev: pointer to net_device
 *
 * Function registered with the Linux OS for device TX/RX statistics.
 *
 * Return: pointer to net_device stats
 */
static struct net_device_stats *__hdd_softap_stats(struct net_device *dev)
{
	hdd_adapter_t *priv = netdev_priv(dev);
	return &priv->stats;
}

/**
 * hdd_softap_stats() - SSR wrapper function for __hdd_softap_stats
 * @dev: pointer to net_device
 *
 * Return: pointer to net_device stats
 */
struct net_device_stats* hdd_softap_stats(struct net_device *dev)
{
	struct net_device_stats *priv_stats;

	vos_ssr_protect(__func__);
	priv_stats = __hdd_softap_stats(dev);
	vos_ssr_unprotect(__func__);

	return priv_stats;
}

/**============================================================================
  @brief hdd_softap_init_tx_rx() - Init function to initialize Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_softap_init_tx_rx( hdd_adapter_t *pAdapter )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   v_SINT_t i = -1;
   v_SIZE_t size = 0;

   v_U8_t STAId = 0;

   pAdapter->isVosOutOfResource = VOS_FALSE;
   pAdapter->isVosLowResource = VOS_FALSE;

   vos_mem_zero(&pAdapter->stats, sizeof(struct net_device_stats));

   while (++i != NUM_TX_QUEUES)
      hdd_list_init( &pAdapter->wmm_tx_queue[i], HDD_TX_QUEUE_MAX_LEN);

   /* Initial HDD buffer control / flow control fields*/
   vos_pkt_get_available_buffer_pool (VOS_PKT_TYPE_TX_802_3_DATA, &size);

   pAdapter->aTxQueueLimit[WLANTL_AC_BK] = HDD_SOFTAP_TX_BK_QUEUE_MAX_LEN;
   pAdapter->aTxQueueLimit[WLANTL_AC_BE] = HDD_SOFTAP_TX_BE_QUEUE_MAX_LEN;
   pAdapter->aTxQueueLimit[WLANTL_AC_VI] = HDD_SOFTAP_TX_VI_QUEUE_MAX_LEN;
   pAdapter->aTxQueueLimit[WLANTL_AC_VO] = HDD_SOFTAP_TX_VO_QUEUE_MAX_LEN;

   spin_lock_init( &pAdapter->staInfo_lock );

   for (STAId = 0; STAId < WLAN_MAX_STA_COUNT; STAId++)
   {
      vos_mem_zero(&pAdapter->aStaInfo[STAId], sizeof(hdd_station_info_t));
      for (i = 0; i < NUM_TX_QUEUES; i ++)
      {
         hdd_list_init(&pAdapter->aStaInfo[STAId].wmm_tx_queue[i], HDD_TX_QUEUE_MAX_LEN);
      }
   }

   return status;
}

/**============================================================================
  @brief hdd_softap_deinit_tx_rx() - Deinit function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_softap_deinit_tx_rx( hdd_adapter_t *pAdapter )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   status = hdd_softap_flush_tx_queues(pAdapter);

   return status;
}

/**============================================================================
  @brief hdd_softap_flush_tx_queues_sta() - Utility function to flush the TX queues of a station

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deinit
  @return         : void
  ===========================================================================*/
static void hdd_softap_flush_tx_queues_sta( hdd_adapter_t *pAdapter, v_U8_t STAId )
{
   v_U8_t i = -1;

   hdd_list_node_t *anchor = NULL;

   skb_list_node_t *pktNode = NULL;
   struct sk_buff *skb = NULL;

   if (FALSE == pAdapter->aStaInfo[STAId].isUsed)
      return;

   for (i = 0; i < NUM_TX_QUEUES; i ++)
   {
      spin_lock_bh(&pAdapter->aStaInfo[STAId].wmm_tx_queue[i].lock);
      while (true)
      {
         if (VOS_STATUS_E_EMPTY !=
            hdd_list_remove_front(&pAdapter->aStaInfo[STAId].wmm_tx_queue[i],
                                 &anchor))
         {
            //If success then we got a valid packet from some AC
            pktNode = list_entry(anchor, skb_list_node_t, anchor);
            skb = pktNode->skb;
            ++pAdapter->stats.tx_dropped;
            kfree_skb(skb);
            continue;
         }

         //current list is empty
         break;
      }
      spin_unlock_bh(&pAdapter->aStaInfo[STAId].wmm_tx_queue[i].lock);
   }

   return;
}

/**============================================================================
  @brief hdd_softap_init_tx_rx_sta() - Init function to initialize a station in Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deinit
  @param pmacAddrSTA  : [in] pointer to the MAC address of the station
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_softap_init_tx_rx_sta( hdd_adapter_t *pAdapter, v_U8_t STAId, v_MACADDR_t *pmacAddrSTA)
{
   v_U8_t i = 0;
   spin_lock_bh( &pAdapter->staInfo_lock );
   if (pAdapter->aStaInfo[STAId].isUsed)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                 "%s: Reinit station %d", __func__, STAId );
      spin_unlock_bh( &pAdapter->staInfo_lock );
      return VOS_STATUS_E_FAILURE;
   }

   vos_mem_zero(&pAdapter->aStaInfo[STAId], sizeof(hdd_station_info_t));
   for (i = 0; i < NUM_TX_QUEUES; i ++)
   {
      hdd_list_init(&pAdapter->aStaInfo[STAId].wmm_tx_queue[i], HDD_TX_QUEUE_MAX_LEN);
   }

   pAdapter->aStaInfo[STAId].isUsed = TRUE;
   pAdapter->aStaInfo[STAId].isDeauthInProgress = FALSE;
   vos_copy_macaddr( &pAdapter->aStaInfo[STAId].macAddrSTA, pmacAddrSTA);

   spin_unlock_bh( &pAdapter->staInfo_lock );
   return VOS_STATUS_SUCCESS;
}

/**============================================================================
  @brief hdd_softap_deinit_tx_rx_sta() - Deinit function to clean up a station in Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deinit
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_softap_deinit_tx_rx_sta ( hdd_adapter_t *pAdapter, v_U8_t STAId )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   v_U8_t ac;
   /**Track whether OS TX queue has been disabled.*/
   v_BOOL_t txSuspended[NUM_TX_QUEUES];
   v_U8_t tlAC;
   hdd_hostapd_state_t *pHostapdState;
   v_U8_t i;

   pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

   spin_lock_bh( &pAdapter->staInfo_lock );
   if (FALSE == pAdapter->aStaInfo[STAId].isUsed)
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                 "%s: Deinit station not inited %d", __func__, STAId );
      spin_unlock_bh( &pAdapter->staInfo_lock );
      return VOS_STATUS_E_FAILURE;
   }

   hdd_softap_flush_tx_queues_sta(pAdapter, STAId);

   pAdapter->aStaInfo[STAId].isUsed = FALSE;
   pAdapter->aStaInfo[STAId].isDeauthInProgress = FALSE;

   /* if this STA had any of its WMM TX queues suspended, then the
      associated queue on the network interface was disabled.  check
      to see if that is the case, in which case we need to re-enable
      the interface queue.  but we only do this if the BSS is running
      since, if the BSS is stopped, all of the interfaces have been
      stopped and should not be re-enabled */

   if (BSS_START == pHostapdState->bssState)
   {
      for (ac = HDD_LINUX_AC_VO; ac <= HDD_LINUX_AC_BK; ac++)
      {
         tlAC = hdd_QdiscAcToTlAC[ac];
         txSuspended[ac] = pAdapter->aStaInfo[STAId].txSuspended[tlAC];
      }
   }
   vos_mem_zero(&pAdapter->aStaInfo[STAId], sizeof(hdd_station_info_t));

   /* re-init spin lock, since netdev can still open adapter until
    * driver gets unloaded
    */
   for (i = 0; i < NUM_TX_QUEUES; i ++)
   {
      hdd_list_init(&pAdapter->aStaInfo[STAId].wmm_tx_queue[i],
                    HDD_TX_QUEUE_MAX_LEN);
   }

   if (BSS_START == pHostapdState->bssState)
   {
      for (ac = HDD_LINUX_AC_VO; ac <= HDD_LINUX_AC_BK; ac++)
      {
         if (txSuspended[ac])
         {
            VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
                       "%s: TX queue re-enabled", __func__);
            netif_wake_subqueue(pAdapter->dev, ac);
         }
      }
   }

   spin_unlock_bh( &pAdapter->staInfo_lock );
   return status;
}

/**============================================================================
  @brief hdd_softap_rx_packet_cbk() - Receive callback registered with TL.
  TL will call this to notify the HDD when one or more packets were
  received for a registered STA.

  @param vosContext      : [in] pointer to VOS context
  @param rxBuf           : [in] pointer to rx adf_nbuf
  @param staId           : [in] Station Id (Address 1 Index)

  @return                : VOS_STATUS_E_FAILURE if any errors encountered,
                         : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_softap_rx_packet_cbk(v_VOID_t *vosContext,
                                    adf_nbuf_t rxBuf, v_U8_t staId)
{
   hdd_adapter_t *pAdapter = NULL;
   int rxstat;
   struct sk_buff *skb = NULL;
   hdd_context_t *pHddCtx = NULL;
#ifdef QCA_PKT_PROTO_TRACE
   v_U8_t proto_type;
#endif /* QCA_PKT_PROTO_TRACE */
   struct sk_buff *skb_next;
   unsigned int cpu_index;

   //Sanity check on inputs
   if ((NULL == vosContext) || (NULL == rxBuf))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,"%s: Null params being passed", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   pHddCtx = (hdd_context_t *)vos_get_context( VOS_MODULE_ID_HDD, vosContext );
   if ( NULL == pHddCtx )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,"%s: HDD adapter context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   pAdapter = pHddCtx->sta_to_adapter[staId];
   if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)) {
      hddLog(LOGE, FL("invalid adapter or adapter has invalid magic"));
      return VOS_STATUS_E_FAILURE;
   }

   if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic) {
       VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
          "Magic cookie(%x) for adapter sanity verification is invalid",
          pAdapter->magic);
       return VOS_STATUS_E_FAILURE;
   }

   if (!pAdapter->dev) {
       VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
          "Invalid DEV(NULL) Drop packets");
       return VOS_STATUS_E_FAILURE;
   }

   // walk the chain until all are processed
   skb = (struct sk_buff *) rxBuf;

   while (NULL != skb) {
      skb_next = skb->next;
      skb->dev = pAdapter->dev;

      cpu_index = wlan_hdd_get_cpu();

      ++pAdapter->hdd_stats.hddTxRxStats.rxPackets[cpu_index];
      ++pAdapter->stats.rx_packets;
      pAdapter->stats.rx_bytes += skb->len;

      wlan_hdd_log_eapol(skb, WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED);
#ifdef QCA_PKT_PROTO_TRACE
      if ((pHddCtx->cfg_ini->gEnableDebugLog & VOS_PKT_TRAC_TYPE_EAPOL) ||
          (pHddCtx->cfg_ini->gEnableDebugLog & VOS_PKT_TRAC_TYPE_DHCP)) {
         proto_type = vos_pkt_get_proto_type(skb,
                           pHddCtx->cfg_ini->gEnableDebugLog, 0);
         if (VOS_PKT_TRAC_TYPE_EAPOL & proto_type)
            vos_pkt_trace_buf_update("HA:R:EPL");
         else if (VOS_PKT_TRAC_TYPE_DHCP & proto_type)
            vos_pkt_trace_buf_update("HA:R:DHC");
      }
#endif /* QCA_PKT_PROTO_TRACE */

      skb->protocol = eth_type_trans(skb, skb->dev);

      /*
       * If this is not a last packet on the chain
       * Just put packet into backlog queue, not scheduling RX sirq
       */
      if (skb->next) {
         rxstat = netif_rx(skb);
      } else {
#ifdef WLAN_FEATURE_HOLD_RX_WAKELOCK
         vos_wake_lock_timeout_acquire(&pHddCtx->rx_wake_lock,
                                       HDD_WAKE_LOCK_DURATION,
                                       WIFI_POWER_EVENT_WAKELOCK_HOLD_RX);
#endif
         /*
          * This is the last packet on the chain
          * Scheduling rx sirq
          */
         rxstat = netif_rx_ni(skb);
      }

      if (NET_RX_SUCCESS == rxstat)
         ++pAdapter->hdd_stats.hddTxRxStats.rxDelivered[cpu_index];
      else
         ++pAdapter->hdd_stats.hddTxRxStats.rxRefused[cpu_index];

      skb = skb_next;
   }
   pAdapter->dev->last_rx = jiffies;

   return VOS_STATUS_SUCCESS;
}

VOS_STATUS hdd_softap_DeregisterSTA( hdd_adapter_t *pAdapter, tANI_U8 staId )
{
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
    hdd_context_t *pHddCtx;
    v_U8_t i;

    if (NULL == pAdapter)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                    "%s: pAdapter is NULL", __func__);
        return VOS_STATUS_E_INVAL;
    }

    if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)
    {
        VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                    "%s: Invalid pAdapter magic", __func__);
        return VOS_STATUS_E_INVAL;
    }

    pHddCtx = (hdd_context_t*)(pAdapter->pHddCtx);
    //Clear station in TL and then update HDD data structures. This helps
    //to block RX frames from other station to this station.
    vosStatus = WLANTL_ClearSTAClient( pHddCtx->pvosContext, staId );
    if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
        VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                    "WLANTL_ClearSTAClient() failed to for staID %d.  "
                    "Status= %d [0x%08X]",
                    staId, vosStatus, vosStatus );
    }

    if (pAdapter->aStaInfo[staId].isUsed) {
        spin_lock_bh( &pAdapter->staInfo_lock );
        vos_mem_zero(&pAdapter->aStaInfo[staId], sizeof(hdd_station_info_t));

        /* re-init spin lock, since netdev can still open adapter until
         * driver gets unloaded
         */
        for (i = 0; i < NUM_TX_QUEUES; i ++)
        {
            hdd_list_init(&pAdapter->aStaInfo[staId].wmm_tx_queue[i],
                          HDD_TX_QUEUE_MAX_LEN);
        }
        spin_unlock_bh( &pAdapter->staInfo_lock );
   }
    pHddCtx->sta_to_adapter[staId] = NULL;

    return( vosStatus );
}

VOS_STATUS hdd_softap_RegisterSTA( hdd_adapter_t *pAdapter,
                                       v_BOOL_t fAuthRequired,
                                       v_BOOL_t fPrivacyBit,
                                       v_U8_t staId,
                                       v_U8_t ucastSig,
                                       v_U8_t bcastSig,
                                       v_MACADDR_t *pPeerMacAddress,
                                       v_BOOL_t fWmmEnabled )
{
   VOS_STATUS vosStatus = VOS_STATUS_E_FAILURE;
   WLAN_STADescType staDesc = {0};
   hdd_context_t *pHddCtx = pAdapter->pHddCtx;
   hdd_adapter_t *pmonAdapter = NULL;

   //eCsrEncryptionType connectedCipherAlgo;
   //v_BOOL_t  fConnected;

   /*
    * Clean up old entry if it is not cleaned up properly
   */
   if ( pAdapter->aStaInfo[staId].isUsed )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
                 "clean up old entry for STA %d", staId);
      hdd_softap_DeregisterSTA( pAdapter, staId );
   }

   /* Get the Station ID from the one saved during the association */

   staDesc.ucSTAId = staId;


   /*Save the pAdapter Pointer for this staId*/
   pHddCtx->sta_to_adapter[staId] = pAdapter;

   staDesc.wSTAType = WLAN_STA_SOFTAP;

   vos_mem_copy( staDesc.vSTAMACAddress.bytes, pPeerMacAddress->bytes,sizeof(pPeerMacAddress->bytes) );
   vos_mem_copy( staDesc.vBSSIDforIBSS.bytes, &pAdapter->macAddressCurrent,6 );
   vos_copy_macaddr( &staDesc.vSelfMACAddress, &pAdapter->macAddressCurrent );

   VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
              "register station");
   VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
              "station mac " MAC_ADDRESS_STR,
              MAC_ADDR_ARRAY(staDesc.vSTAMACAddress.bytes));
   VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
              "BSSIDforIBSS " MAC_ADDRESS_STR,
              MAC_ADDR_ARRAY(staDesc.vBSSIDforIBSS.bytes));
   VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
              "SOFTAP SELFMAC " MAC_ADDRESS_STR,
              MAC_ADDR_ARRAY(staDesc.vSelfMACAddress.bytes));

   vosStatus = hdd_softap_init_tx_rx_sta(pAdapter, staId, &staDesc.vSTAMACAddress);

   staDesc.ucQosEnabled = fWmmEnabled;
   VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
              "HDD SOFTAP register TL QoS_enabled=%d",
              staDesc.ucQosEnabled );

   staDesc.ucProtectedFrame = (v_U8_t)fPrivacyBit ;


   // For PRIMA UMA frame translation is not enable yet.
   staDesc.ucSwFrameTXXlation = 1;
   staDesc.ucSwFrameRXXlation = 1;
   staDesc.ucAddRmvLLC = 1;

   // Initialize signatures and state
   staDesc.ucUcastSig  = ucastSig;
   staDesc.ucBcastSig  = bcastSig;
   staDesc.ucInitState = fAuthRequired ?
      WLANTL_STA_CONNECTED : WLANTL_STA_AUTHENTICATED;

   staDesc.ucIsReplayCheckValid = VOS_FALSE;

   // Register the Station with TL...
   /* Incase Micro controller data path offload enabled,
    * All the traffic routed to WLAN host driver, do not need to
    * route IPA. It should be routed kernel network stack */
#if defined(IPA_OFFLOAD) && !defined(IPA_UC_OFFLOAD)
   if (hdd_ipa_is_enabled(pHddCtx))
      vosStatus = WLANTL_RegisterSTAClient(
                              (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                              hdd_ipa_process_rxt,
                              &staDesc, 0);
   else
#endif
   vosStatus = WLANTL_RegisterSTAClient(
                              (WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                              hdd_softap_rx_packet_cbk,
                              &staDesc, 0);
   if ( !VOS_IS_STATUS_SUCCESS( vosStatus ) )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                 "SOFTAP WLANTL_RegisterSTAClient() failed to register.  Status= %d [0x%08X]",
                 vosStatus, vosStatus );
      return vosStatus;
   }

   // if ( WPA ), tell TL to go to 'connected' and after keys come to the driver,
   // then go to 'authenticated'.  For all other authentication types (those that do
   // not require upper layer authentication) we can put TL directly into 'authenticated'
   // state.

   //VOS_ASSERT( fConnected );
   pAdapter->aStaInfo[staId].ucSTAId = staId;
   pAdapter->aStaInfo[staId].isQosEnabled = fWmmEnabled;

   if ( !fAuthRequired )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
                 "open/shared auth StaId= %d.  Changing TL state to AUTHENTICATED at Join time",
                  pAdapter->aStaInfo[staId].ucSTAId );

      // Connections that do not need Upper layer auth, transition TL directly
      // to 'Authenticated' state.
      vosStatus = WLANTL_ChangeSTAState( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext, staDesc.ucSTAId,
                                         WLANTL_STA_AUTHENTICATED, VOS_FALSE);

      pAdapter->aStaInfo[staId].tlSTAState = WLANTL_STA_AUTHENTICATED;
      pAdapter->sessionCtx.ap.uIsAuthenticated = VOS_TRUE;
   }
   else
   {

      VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
                 "ULA auth StaId= %d.  Changing TL state to CONNECTED at Join time", pAdapter->aStaInfo[staId].ucSTAId );

      vosStatus = WLANTL_ChangeSTAState( (WLAN_HDD_GET_CTX(pAdapter))->pvosContext, staDesc.ucSTAId,
                                         WLANTL_STA_CONNECTED, VOS_FALSE);
      pAdapter->aStaInfo[staId].tlSTAState = WLANTL_STA_CONNECTED;

      pAdapter->sessionCtx.ap.uIsAuthenticated = VOS_FALSE;

   }
   pmonAdapter= hdd_get_mon_adapter( pAdapter->pHddCtx);
   if(pmonAdapter)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO_HIGH,
                  "Turn on Monitor the carrier");
       netif_carrier_on(pmonAdapter->dev);
           //Enable Tx queue
       hddLog(LOG1, FL("Enabling queues"));
       netif_tx_start_all_queues(pmonAdapter->dev);
    }
   netif_carrier_on(pAdapter->dev);
   //Enable Tx queue
   hddLog(LOG1, FL("Enabling queues"));
   netif_tx_start_all_queues(pAdapter->dev);

   return( vosStatus );
}

VOS_STATUS hdd_softap_Register_BC_STA( hdd_adapter_t *pAdapter, v_BOOL_t fPrivacyBit)
{
   VOS_STATUS vosStatus = VOS_STATUS_E_FAILURE;
   hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
   v_MACADDR_t broadcastMacAddr = VOS_MAC_ADDR_BROADCAST_INITIALIZER;
   hdd_ap_ctx_t *pHddApCtx;

   pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);

   pHddCtx->sta_to_adapter[WLAN_RX_BCMC_STA_ID] = pAdapter;
#ifdef WLAN_FEATURE_MBSSID
   pHddCtx->sta_to_adapter[pHddApCtx->uBCStaId] = pAdapter;
#else
   pHddCtx->sta_to_adapter[WLAN_RX_SAP_SELF_STA_ID] = pAdapter;
#endif
   vosStatus = hdd_softap_RegisterSTA( pAdapter, VOS_FALSE, fPrivacyBit, (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->uBCStaId, 0, 1, &broadcastMacAddr,0);

   return vosStatus;
}

VOS_STATUS hdd_softap_Deregister_BC_STA( hdd_adapter_t *pAdapter)
{
   return hdd_softap_DeregisterSTA( pAdapter, (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->uBCStaId);
}

VOS_STATUS hdd_softap_stop_bss( hdd_adapter_t *pAdapter)
{
    VOS_STATUS vosStatus = VOS_STATUS_E_FAILURE;
    v_U8_t staId = 0;
    hdd_context_t *pHddCtx;
    pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

    /*bss deregister is not allowed during wlan driver loading or unloading*/
    if ((pHddCtx->isLoadInProgress) ||
        (pHddCtx->isUnloadInProgress))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                   "%s:Loading_unloading in Progress. Ignore!!!",__func__);
        return VOS_STATUS_E_PERM;
    }

    vosStatus = hdd_softap_Deregister_BC_STA( pAdapter);

    if (!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                   "%s: Failed to deregister BC sta Id %d", __func__, (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->uBCStaId);
    }

    for (staId = 0; staId < WLAN_MAX_STA_COUNT; staId++)
    {
        if (pAdapter->aStaInfo[staId].isUsed)// This excludes BC sta as it is already deregistered
            vosStatus = hdd_softap_DeregisterSTA( pAdapter, staId);

        if (!VOS_IS_STATUS_SUCCESS(vosStatus))
        {
                VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                       "%s: Failed to deregister sta Id %d", __func__, staId);
        }
    }
    return vosStatus;
}

VOS_STATUS hdd_softap_change_STA_state( hdd_adapter_t *pAdapter, v_MACADDR_t *pDestMacAddress, WLANTL_STAStateType state)
{
    v_U8_t ucSTAId = WLAN_MAX_STA_COUNT;
    VOS_STATUS vosStatus = eHAL_STATUS_SUCCESS;
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pAdapter))->pvosContext;

    VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
               "%s: enter", __func__);

    if (VOS_STATUS_SUCCESS != hdd_softap_GetStaId(pAdapter, pDestMacAddress, &ucSTAId))
    {
        VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                    "%s: Failed to find right station", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (FALSE == vos_is_macaddr_equal(&pAdapter->aStaInfo[ucSTAId].macAddrSTA, pDestMacAddress))
    {
         VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_ERROR,
                    "%s: Station MAC address does not matching", __func__);
         return VOS_STATUS_E_FAILURE;
    }

    vosStatus = WLANTL_ChangeSTAState( pVosContext, ucSTAId, state, VOS_FALSE);
    VOS_TRACE( VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
                   "%s: change station to state %d succeed", __func__, state);

    if (VOS_STATUS_SUCCESS == vosStatus)
    {
       pAdapter->aStaInfo[ucSTAId].tlSTAState = WLANTL_STA_AUTHENTICATED;
    }

    VOS_TRACE(VOS_MODULE_ID_HDD_SAP_DATA, VOS_TRACE_LEVEL_INFO,
                                     "%s exit", __func__);

    return vosStatus;
}


VOS_STATUS hdd_softap_GetStaId(hdd_adapter_t *pAdapter, v_MACADDR_t *pMacAddress, v_U8_t *staId)
{
    v_U8_t i;

    for (i = 0; i < WLAN_MAX_STA_COUNT; i++)
    {
        if (vos_mem_compare(&pAdapter->aStaInfo[i].macAddrSTA, pMacAddress, sizeof(v_MACADDR_t)) &&
            pAdapter->aStaInfo[i].isUsed)
        {
            *staId = i;
            return VOS_STATUS_SUCCESS;
        }
    }

    return VOS_STATUS_E_FAILURE;
}

