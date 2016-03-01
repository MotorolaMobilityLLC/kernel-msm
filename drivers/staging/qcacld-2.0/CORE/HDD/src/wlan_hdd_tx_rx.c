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

/**===========================================================================

  \file  wlan_hdd_tx_rx.c

  \brief Linux HDD Tx/RX APIs

  ==========================================================================*/

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/

/* Needs to be removed when completely root-caused */
#define IPV6_MCAST_WAR 1

#include <wlan_hdd_tx_rx.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_dp_utils.h>
#include <wlan_qct_tl.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <vos_sched.h>
#ifdef IPV6_MCAST_WAR
#include <linux/if_ether.h>
#endif

#include <wlan_hdd_p2p.h>
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include <net/ieee80211_radiotap.h>
#include "sapApi.h"

#ifdef FEATURE_WLAN_TDLS
#include "wlan_hdd_tdls.h"
#endif

#ifdef IPA_OFFLOAD
#include <wlan_hdd_ipa.h>
#endif

/*---------------------------------------------------------------------------
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/
const v_U8_t hddWmmAcToHighestUp[] = {
   SME_QOS_WMM_UP_RESV,
   SME_QOS_WMM_UP_EE,
   SME_QOS_WMM_UP_VI,
   SME_QOS_WMM_UP_NC
};

//Mapping Linux AC interpretation to TL AC.
const v_U8_t hdd_QdiscAcToTlAC[] = {
   WLANTL_AC_VO,
   WLANTL_AC_VI,
   WLANTL_AC_BE,
   WLANTL_AC_BK,
};

/*---------------------------------------------------------------------------
  Type declarations
  -------------------------------------------------------------------------*/

#ifdef FEATURE_WLAN_DIAG_SUPPORT
#define HDD_EAPOL_ETHER_TYPE             (0x888E)
#define HDD_EAPOL_ETHER_TYPE_OFFSET      (12)
#define HDD_EAPOL_PACKET_TYPE_OFFSET     (15)
#define HDD_EAPOL_KEY_INFO_OFFSET        (19)
#define HDD_EAPOL_DEST_MAC_OFFSET        (0)
#define HDD_EAPOL_SRC_MAC_OFFSET         (6)
#define EAPOL_MASK                       0x8013
#define EAPOL_M1_BIT_MASK                0x8000
#define EAPOL_M2_BIT_MASK                0x0001
#define EAPOL_M3_BIT_MASK                0x8013
#define EAPOL_M4_BIT_MASK                0x0003
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

/*---------------------------------------------------------------------------
  Function definitions and documentation
  -------------------------------------------------------------------------*/

/**
 * wlan_hdd_is_eapol() - Function to check if frame is EAPOL or not
 * @skb:    skb data
 *
 * This function checks if the frame is an EAPOL frame or not
 *
 * Return: true (1) if packet is EAPOL
 *
 */
static bool wlan_hdd_is_eapol(struct sk_buff *skb)
{
	uint16_t ether_type;

	if (!skb) {
		hddLog(VOS_TRACE_LEVEL_ERROR, FL("skb is NULL"));
		return false;
	}

	ether_type = (uint16_t)(*(uint16_t *)
			(skb->data + HDD_ETHERTYPE_802_1_X_FRAME_OFFSET));

	if (ether_type == VOS_SWAP_U16(HDD_ETHERTYPE_802_1_X)) {
		return true;
	} else {
		/* No error msg handled since this will happen often */
		return false;
	}
}

/**
 * wlan_hdd_is_wai() - Check if frame is EAPOL or WAPI
 * @skb:    skb data
 *
 * This function checks if the frame is EAPOL or WAPI.
 * single routine call will check for both types, thus avoiding
 * data path performance penalty.
 *
 * Return: true (1) if packet is EAPOL or WAPI
 *
 */
static bool wlan_hdd_is_eapol_or_wai(struct sk_buff *skb)
{
	uint16_t ether_type;

	if (!skb) {
		hddLog(VOS_TRACE_LEVEL_ERROR, FL("skb is NULL"));
		return false;
	}

	ether_type = (uint16_t)(*(uint16_t *)
			(skb->data + HDD_ETHERTYPE_802_1_X_FRAME_OFFSET));

	if (ether_type == VOS_SWAP_U16(HDD_ETHERTYPE_802_1_X) ||
	    ether_type == VOS_SWAP_U16(HDD_ETHERTYPE_WAI))
		return true;

	/* No error msg handled since this will happen often */
	return false;
}


/**============================================================================
  @brief hdd_flush_tx_queues() - Utility function to flush the TX queues

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
static VOS_STATUS hdd_flush_tx_queues( hdd_adapter_t *pAdapter )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   v_SINT_t i = -1;
   hdd_list_node_t *anchor = NULL;
   skb_list_node_t *pktNode = NULL;
   struct sk_buff *skb = NULL;

   pAdapter->isVosLowResource = VOS_FALSE;

   while (++i != NUM_TX_QUEUES)
   {
      //Free up any packets in the Tx queue
      spin_lock_bh(&pAdapter->wmm_tx_queue[i].lock);
      while (true)
      {
         status = hdd_list_remove_front( &pAdapter->wmm_tx_queue[i], &anchor );
         if(VOS_STATUS_E_EMPTY != status)
         {
            pktNode = list_entry(anchor, skb_list_node_t, anchor);
            skb = pktNode->skb;
            ++pAdapter->stats.tx_dropped;
            kfree_skb(skb);
            continue;
         }
         break;
      }
      spin_unlock_bh(&pAdapter->wmm_tx_queue[i].lock);
      /* Back pressure is no longer in effect */
      pAdapter->isTxSuspended[i] = VOS_FALSE;
   }

   return status;
}

/**============================================================================
  @brief hdd_flush_ibss_tx_queues() - Utility function to flush the TX queues
                                      in IBSS mode

  @param pAdapter : [in] pointer to adapter context
                  : [in] Station Id
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
void hdd_flush_ibss_tx_queues( hdd_adapter_t *pAdapter, v_U8_t STAId)
{
   v_U8_t i;
   v_SIZE_t size = 0;
   v_U8_t skbStaIdx;
   skb_list_node_t *pktNode = NULL;
   hdd_list_node_t *tmp = NULL, *next = NULL;
   struct netdev_queue *txq;
   struct sk_buff *skb = NULL;

   if (NULL == pAdapter)
   {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter is NULL %u"), STAId);
       VOS_ASSERT(0);
       return;
   }

   for (i = 0; i < NUM_TX_QUEUES; i++)
   {
      spin_lock_bh(&pAdapter->wmm_tx_queue[i].lock);

      if ( list_empty( &pAdapter->wmm_tx_queue[i].anchor ) )
      {
         spin_unlock_bh(&pAdapter->wmm_tx_queue[i].lock);
         continue;
      }

      /* Iterate through the queue and identify the data for STAId */
      list_for_each_safe(tmp, next, &pAdapter->wmm_tx_queue[i].anchor)
      {
         pktNode = list_entry(tmp, skb_list_node_t, anchor);
         if (pktNode != NULL)
         {
            skb = pktNode->skb;

            /* Get the STAId from data */
            skbStaIdx = *(v_U8_t *)(((v_U8_t *)(skb->data)) - 1);
            if (skbStaIdx == STAId)
            {
               /* Data for STAId is freed along with the queue node */

               list_del(tmp);
               kfree_skb(skb);

               pAdapter->wmm_tx_queue[i].count--;
            }
         }
      }

      /* Restart the queue only-if suspend and the queue was flushed */
      hdd_list_size( &pAdapter->wmm_tx_queue[i], &size );
      txq = netdev_get_tx_queue(pAdapter->dev, i);

      if (VOS_TRUE == pAdapter->isTxSuspended[i] &&
          size <= HDD_TX_QUEUE_LOW_WATER_MARK &&
          netif_tx_queue_stopped(txq) )
      {
         hddLog(LOG1, FL("Enabling queue for queue %d"), i);
         netif_tx_start_queue(txq);
         pAdapter->isTxSuspended[i] = VOS_FALSE;
      }

      spin_unlock_bh(&pAdapter->wmm_tx_queue[i].lock);
   }
}

#ifdef QCA_LL_TX_FLOW_CT
/**============================================================================
  @brief hdd_tx_resume_timer_expired_handler() - Resume OS TX Q timer expired
      handler.
      If Blocked OS Q is not resumed during timeout period, to prevent
      permanent stall, resume OS Q forcefully.

  @param adapter_context : [in] pointer to vdev adapter

  @return         : NONE
  ===========================================================================*/
void hdd_tx_resume_timer_expired_handler(void *adapter_context)
{
   hdd_adapter_t *pAdapter = (hdd_adapter_t *)adapter_context;

   if (!pAdapter)
   {
      /* INVALID ARG */
      return;
   }

   hddLog(LOG1, FL("Enabling queues"));
   netif_tx_wake_all_queues(pAdapter->dev);
   pAdapter->hdd_stats.hddTxRxStats.txflow_unpause_cnt++;
   pAdapter->hdd_stats.hddTxRxStats.is_txflow_paused = FALSE;

   return;
}

/**============================================================================
  @brief hdd_tx_resume_cb() - Resume OS TX Q.
      Q was stopped due to WLAN TX path low resource condition

  @param adapter_context : [in] pointer to vdev adapter
  @param tx_resume       : [in] TX Q resume trigger

  @return         : NONE
  ===========================================================================*/
void hdd_tx_resume_cb(void *adapter_context,
                        v_BOOL_t tx_resume)
{
   hdd_adapter_t *pAdapter = (hdd_adapter_t *)adapter_context;
   hdd_station_ctx_t *hdd_sta_ctx = NULL;

   if (!pAdapter)
   {
      /* INVALID ARG */
      return;
   }

   hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

   /* Resume TX  */
   if (VOS_TRUE == tx_resume)
   {
       if (VOS_TIMER_STATE_STOPPED !=
          vos_timer_getCurrentState(&pAdapter->tx_flow_control_timer))
       {
          vos_timer_stop(&pAdapter->tx_flow_control_timer);
       }
       if (adf_os_unlikely(hdd_sta_ctx->hdd_ReassocScenario)) {
           hddLog(LOGW,
                  FL("flow control, tx queues un-pause avoided as we are in REASSOCIATING state"));
           return;
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
                          WLAN_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);
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

/**
 * hdd_drop_skb() - drop a packet
 * @adapter: pointer to hdd adapter
 * @skb: pointer to OS packet
 *
 * Return: Nothing to return
 */
void hdd_drop_skb(hdd_adapter_t *adapter, struct sk_buff *skb)
{
	++adapter->stats.tx_dropped;
	++adapter->hdd_stats.hddTxRxStats.txXmitDropped;
	kfree_skb(skb);
}

/**
 * hdd_drop_skb_list() - drop packet list
 * @adapter: pointer to hdd adapter
 * @skb: pointer to OS packet list
 * @is_update_ac_stats: macro to update ac stats
 *
 * Return: Nothing to return
 */
void hdd_drop_skb_list(hdd_adapter_t *adapter, struct sk_buff *skb,
                                               bool is_update_ac_stats)
{
	WLANTL_ACEnumType ac;
	struct sk_buff *skb_next;

	while (skb) {
		++adapter->stats.tx_dropped;
		++adapter->hdd_stats.hddTxRxStats.txXmitDropped;
		if (is_update_ac_stats == TRUE) {
			ac = hdd_QdiscAcToTlAC[skb->queue_mapping];
			++adapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[ac];
		}
		skb_next = skb->next;
		kfree_skb(skb);
		skb = skb_next;
	}
}

/**============================================================================
  @brief hdd_hard_start_xmit() - Function registered with the Linux OS for
  transmitting packets. This version of the function directly passes the packet
  to Transport Layer.

  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param dev      : [in] pointer to network device

  @return         : NET_XMIT_DROP if packets are dropped
                  : NET_XMIT_SUCCESS if packet is enqueued successfully
  ===========================================================================*/
int __hdd_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
   VOS_STATUS status;
   WLANTL_ACEnumType ac;
   sme_QosWmmUpType up;
   hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
   v_BOOL_t granted;
   v_U8_t STAId = WLAN_MAX_STA_COUNT;
   hdd_station_ctx_t *pHddStaCtx = &pAdapter->sessionCtx.station;
   struct sk_buff *skb_next, *list_head = NULL, *list_tail = NULL;
   void *vdev_handle = NULL, *vdev_temp;
   bool is_update_ac_stats = FALSE;
#ifdef QCA_PKT_PROTO_TRACE
   hdd_context_t *hddCtxt = WLAN_HDD_GET_CTX(pAdapter);
   v_U8_t proto_type = 0;
#endif /* QCA_PKT_PROTO_TRACE */

#ifdef QCA_WIFI_FTM
   if (hdd_get_conparam() == VOS_FTM_MODE) {
       while (skb) {
           skb_next = skb->next;
           kfree_skb(skb);
           skb = skb_next;
       }
       return NETDEV_TX_OK;
   }
#endif

   ++pAdapter->hdd_stats.hddTxRxStats.txXmitCalled;

   if(vos_is_logp_in_progress(VOS_MODULE_ID_VOSS, NULL)) {
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,
            "LOPG in progress, dropping the packet\n");
       goto drop_list;
   }

   while (skb) {
       skb_next = skb->next;
       if (WLAN_HDD_IBSS == pAdapter->device_mode)
       {
           v_MACADDR_t *pDestMacAddress = (v_MACADDR_t*)skb->data;

           if ( VOS_STATUS_SUCCESS !=
               hdd_Ibss_GetStaId(&pAdapter->sessionCtx.station,
                                 pDestMacAddress, &STAId))
           {
               STAId = HDD_WLAN_INVALID_STA_ID;
           }

           if ((STAId == HDD_WLAN_INVALID_STA_ID) &&
               (vos_is_macaddr_broadcast( pDestMacAddress ) ||
                vos_is_macaddr_group(pDestMacAddress)))
           {
               STAId = IBSS_BROADCAST_STAID;
               VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO_LOW,
                    "%s: BC/MC packet", __func__);
           }
           else if (STAId == HDD_WLAN_INVALID_STA_ID)
           {
               VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,
                    "%s: Received Unicast frame with invalid staID", __func__);
               goto drop_pkt;
           }
       }
       else
       {
           if (WLAN_HDD_OCB != pAdapter->device_mode
               && eConnectionState_Associated !=
                  pHddStaCtx->conn_info.connState) {
               VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                    FL("Tx frame in not associated state in %d context"),
                        pAdapter->device_mode);
               goto drop_pkt;
           }
           STAId = pHddStaCtx->conn_info.staId[0];
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
           hddLog(LOG1, FL("Disabling queues"));
           netif_tx_stop_all_queues(dev);
           if ((pAdapter->tx_flow_timer_initialized == TRUE) &&
               (VOS_TIMER_STATE_STOPPED ==
                vos_timer_getCurrentState(&pAdapter->tx_flow_control_timer))) {
               vos_timer_start(&pAdapter->tx_flow_control_timer,
                               WLAN_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);
               pAdapter->hdd_stats.hddTxRxStats.txflow_timer_cnt++;
               pAdapter->hdd_stats.hddTxRxStats.txflow_pause_cnt++;
               pAdapter->hdd_stats.hddTxRxStats.is_txflow_paused = TRUE;
           }
       }
#endif /* QCA_LL_TX_FLOW_CT */

       //Get TL AC corresponding to Qdisc queue index/AC.
       ac = hdd_QdiscAcToTlAC[skb->queue_mapping];

       //user priority from IP header, which is already extracted and set from
       //select_queue call back function
       up = skb->priority;

       ++pAdapter->hdd_stats.hddTxRxStats.txXmitClassifiedAC[ac];
#ifdef HDD_WMM_DEBUG
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
                  "%s: Classified as ac %d up %d", __func__, ac, up);
#endif // HDD_WMM_DEBUG

       if (HDD_PSB_CHANGED == pAdapter->psbChanged)
       {
           /* Function which will determine acquire admittance for a
            * WMM AC is required or not based on psb configuration done
            * in the framework
            */
           hdd_wmm_acquire_access_required(pAdapter, ac);
       }

       /*
        * Make sure we already have access to this access category
        * or it is EAPOL or WAPI frame during initial authentication which
        * can have artifically boosted higher qos priority.
        */

       if (((pAdapter->psbChanged & (1 << ac)) &&
             likely(
             pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessAllowed)) ||
           ((pHddStaCtx->conn_info.uIsAuthenticated == VOS_FALSE) &&
             wlan_hdd_is_eapol_or_wai(skb)))
       {
           granted = VOS_TRUE;
       }
       else
       {
           status = hdd_wmm_acquire_access( pAdapter, ac, &granted );
           pAdapter->psbChanged |= (1 << ac);
       }

       if (!granted) {
           bool isDefaultAc = VOS_FALSE;
           /* ADDTS request for this AC is sent, for now
            * send this packet through next available lower
            * Access category until ADDTS negotiation completes.
            */
           while (!likely(
                   pAdapter->hddWmmStatus.wmmAcStatus[ac].wmmAcAccessAllowed)) {
               switch(ac) {
               case WLANTL_AC_VO:
                    ac = WLANTL_AC_VI;
                    up = SME_QOS_WMM_UP_VI;
                    break;
               case WLANTL_AC_VI:
                    ac = WLANTL_AC_BE;
                    up = SME_QOS_WMM_UP_BE;
                    break;
               case WLANTL_AC_BE:
                    ac = WLANTL_AC_BK;
                    up = SME_QOS_WMM_UP_BK;
                    break;
               default:
                    ac = WLANTL_AC_BK;
                    up = SME_QOS_WMM_UP_BK;
                    isDefaultAc = VOS_TRUE;
                    break;
               }
               if (isDefaultAc)
                   break;
           }
           skb->priority = up;
           skb->queue_mapping = hddLinuxUpToAcMap[up];
       }

       wlan_hdd_log_eapol(skb,
                          WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED);

#ifdef QCA_PKT_PROTO_TRACE
       if ((hddCtxt->cfg_ini->gEnableDebugLog & VOS_PKT_TRAC_TYPE_EAPOL) ||
           (hddCtxt->cfg_ini->gEnableDebugLog & VOS_PKT_TRAC_TYPE_DHCP))
       {
           proto_type = vos_pkt_get_proto_type(skb,
                        hddCtxt->cfg_ini->gEnableDebugLog, 0);
           if (VOS_PKT_TRAC_TYPE_EAPOL & proto_type)
           {
               vos_pkt_trace_buf_update("ST:T:EPL");
           }
           else if (VOS_PKT_TRAC_TYPE_DHCP & proto_type)
           {
               vos_pkt_trace_buf_update("ST:T:DHC");
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

   /*
    * TODO: Should we stop net queues when txrx returns non-NULL?.
    */
   skb = WLANTL_SendSTA_DataFrame((WLAN_HDD_GET_CTX(pAdapter))->pvosContext,
                                   vdev_handle, list_head
#ifdef QCA_PKT_PROTO_TRACE
                                 , proto_type
#endif /* QCA_PKT_PROTO_TRACE */
                                 );
   if (skb != NULL) {
        VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,
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

int hdd_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	int ret;

	vos_ssr_protect(__func__);
	ret = __hdd_hard_start_xmit(skb, dev);
	vos_ssr_unprotect(__func__);
	return ret;
}

/**============================================================================
  @brief hdd_Ibss_GetStaId() - Get the StationID using the Peer Mac address

  @param pHddStaCtx : [in] pointer to HDD Station Context
  pMacAddress [in]  pointer to Peer Mac address
  staID [out]  pointer to Station Index
  @return    : VOS_STATUS_SUCCESS/VOS_STATUS_E_FAILURE
  ===========================================================================*/

VOS_STATUS hdd_Ibss_GetStaId(hdd_station_ctx_t *pHddStaCtx, v_MACADDR_t *pMacAddress, v_U8_t *staId)
{
    v_U8_t idx;

    for (idx = 0; idx < HDD_MAX_NUM_IBSS_STA; idx++)
    {
        if (vos_mem_compare(&pHddStaCtx->conn_info.peerMacAddress[ idx ],
                pMacAddress, sizeof(v_MACADDR_t)))
        {
            *staId = pHddStaCtx->conn_info.staId[idx];
            return VOS_STATUS_SUCCESS;
        }
    }

    return VOS_STATUS_E_FAILURE;
}

/**
 * __hdd_tx_timeout() - HDD tx timeout handler
 * @dev: pointer to net_device structure
 *
 * Function called by OS if there is any timeout during transmission.
 * Since HDD simply enqueues packet and returns control to OS right away,
 * this would never be invoked
 *
 * Return: none
 */
static void __hdd_tx_timeout(struct net_device *dev)
{
   hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
   struct netdev_queue *txq;
   int i = 0;

   hddLog(LOGE, FL("Transmission timeout occurred jiffies %lu trans_start %lu"),
          jiffies, dev->trans_start);
   /*
    * Getting here implies we disabled the TX queues for too long. Queues are
    * disabled either because of disassociation or low resource scenarios. In
    * case of disassociation it is ok to ignore this. But if associated, we have
    * do possible recovery here.
    */

   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
              "num_bytes AC0: %d AC1: %d AC2: %d AC3: %d",
              pAdapter->wmm_tx_queue[0].count,
              pAdapter->wmm_tx_queue[1].count,
              pAdapter->wmm_tx_queue[2].count,
              pAdapter->wmm_tx_queue[3].count);

   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
              "tx_suspend AC0: %d AC1: %d AC2: %d AC3: %d",
              pAdapter->isTxSuspended[0],
              pAdapter->isTxSuspended[1],
              pAdapter->isTxSuspended[2],
              pAdapter->isTxSuspended[3]);

   for (i = 0; i < NUM_TX_QUEUES; i++) {
      txq = netdev_get_tx_queue(dev, i);
      hddLog(LOG1, FL("Queue%d status: %d txq->trans_start %lu"),
             i, netif_tx_queue_stopped(txq), txq->trans_start);
   }

   VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
              "carrier state: %d", netif_carrier_ok(dev));
}

/**
 * hdd_tx_timeout() - Wrapper function to protect __hdd_tx_timeout from SSR
 * @dev: pointer to net_device structure
 *
 * Function called by OS if there is any timeout during transmission.
 * Since HDD simply enqueues packet and returns control to OS right away,
 * this would never be invoked
 *
 * Return: none
 */
void hdd_tx_timeout(struct net_device *dev)
{
	vos_ssr_protect(__func__);
	__hdd_tx_timeout(dev);
	vos_ssr_unprotect(__func__);
}

/**
 * __hdd_stats() - Function registered with the Linux OS for
 *			device TX/RX statistics
 * @dev: pointer to net_device structure
 *
 * Return: pointer to net_device_stats structure
 */
static struct net_device_stats *__hdd_stats(struct net_device *dev)
{
	hdd_adapter_t *pAdapter =  WLAN_HDD_GET_PRIV_PTR(dev);
	return &pAdapter->stats;
}

/**
 * hdd_stats() - SSR wrapper for __hdd_stats
 * @dev: pointer to net_device structure
 *
 * Return: pointer to net_device_stats structure
 */
struct net_device_stats* hdd_stats(struct net_device *dev)
{
	struct net_device_stats *dev_stats;

	vos_ssr_protect(__func__);
	dev_stats = __hdd_stats(dev);
	vos_ssr_unprotect(__func__);

	return dev_stats;
}

/**============================================================================
  @brief hdd_init_tx_rx() - Init function to initialize Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_init_tx_rx( hdd_adapter_t *pAdapter )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   v_SINT_t i = -1;

   if ( NULL == pAdapter )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter is NULL"));
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   pAdapter->isVosOutOfResource = VOS_FALSE;
   pAdapter->isVosLowResource = VOS_FALSE;

   //vos_mem_zero(&pAdapter->stats, sizeof(struct net_device_stats));
   //Will be zeroed out during alloc

   while (++i != NUM_TX_QUEUES)
   {
      pAdapter->isTxSuspended[i] = VOS_FALSE;
      hdd_list_init( &pAdapter->wmm_tx_queue[i], HDD_TX_QUEUE_MAX_LEN);
   }

   return status;
}


/**============================================================================
  @brief hdd_deinit_tx_rx() - Deinit function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_deinit_tx_rx( hdd_adapter_t *pAdapter )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   v_SINT_t i = -1;

   if ( NULL == pAdapter )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
              FL("pAdapter is NULL"));
      VOS_ASSERT(0);
      return VOS_STATUS_E_FAILURE;
   }

   status = hdd_flush_tx_queues(pAdapter);
   if (VOS_STATUS_SUCCESS != status)
       VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_WARN,
          FL("failed to flush tx queues"));

   while (++i != NUM_TX_QUEUES)
   {
      //Free up actual list elements in the Tx queue
      hdd_list_destroy( &pAdapter->wmm_tx_queue[i] );
   }

   return status;
}


/**============================================================================
  @brief hdd_disconnect_tx_rx() - Disconnect function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_disconnect_tx_rx( hdd_adapter_t *pAdapter )
{
   return hdd_flush_tx_queues(pAdapter);
}


/**============================================================================
  @brief hdd_IsEAPOLPacket() - Checks the packet is EAPOL or not.

  @param pVosPacket : [in] pointer to vos packet
  @return         : VOS_TRUE if the packet is EAPOL
                  : VOS_FALSE otherwise
  ===========================================================================*/

v_BOOL_t hdd_IsEAPOLPacket( vos_pkt_t *pVosPacket )
{
    VOS_STATUS vosStatus  = VOS_STATUS_SUCCESS;
    v_BOOL_t   fEAPOL     = VOS_FALSE;
    void       *pBuffer   = NULL;


    vosStatus = vos_pkt_peek_data( pVosPacket, (v_SIZE_t)HDD_ETHERTYPE_802_1_X_FRAME_OFFSET,
                          &pBuffer, HDD_ETHERTYPE_802_1_X_SIZE );
    if (VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
       if (pBuffer && vos_be16_to_cpu( *(unsigned short*)pBuffer) == HDD_ETHERTYPE_802_1_X )
       {
          fEAPOL = VOS_TRUE;
       }
    }

   return fEAPOL;
}


#ifdef FEATURE_WLAN_WAPI // Need to update this function
/**============================================================================
  @brief hdd_IsWAIPacket() - Checks the packet is WAI or not.

  @param pVosPacket : [in] pointer to vos packet
  @return         : VOS_TRUE if the packet is WAI
                  : VOS_FALSE otherwise
  ===========================================================================*/

v_BOOL_t hdd_IsWAIPacket( vos_pkt_t *pVosPacket )
{
    VOS_STATUS vosStatus  = VOS_STATUS_SUCCESS;
    v_BOOL_t   fIsWAI     = VOS_FALSE;
    void       *pBuffer   = NULL;

    // Need to update this function
    vosStatus = vos_pkt_peek_data( pVosPacket, (v_SIZE_t)HDD_ETHERTYPE_802_1_X_FRAME_OFFSET,
                          &pBuffer, HDD_ETHERTYPE_802_1_X_SIZE );

    if (VOS_IS_STATUS_SUCCESS( vosStatus ) )
    {
       if (pBuffer && vos_be16_to_cpu( *((unsigned short*)pBuffer)) == HDD_ETHERTYPE_WAI)
       {
          fIsWAI = VOS_TRUE;
       }
    }

   return fIsWAI;
}
#endif /* FEATURE_WLAN_WAPI */

#ifdef IPV6_MCAST_WAR
/*
 * Return TRUE if the packet is to be dropped
 */
static inline
bool drop_ip6_mcast(struct sk_buff *skb)
{
    struct ethhdr *eth;

    eth = eth_hdr(skb);
    if (unlikely(skb->pkt_type == PACKET_MULTICAST)) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0))
       if (unlikely(ether_addr_equal(eth->h_source, skb->dev->dev_addr)))
#else
       if (unlikely(!compare_ether_addr(eth->h_source, skb->dev->dev_addr)))
#endif
            return true;
    }
    return false;
}
#else
#define drop_ip6_mcast(_a) 0
#endif

/**
 * hdd_mon_rx_packet_cbk() - Receive callback registered with TLSHIM.
 * @vosContext: [in] pointer to VOS context
 * @staId:      [in] Station Id
 * @rxBuf:      [in] pointer to rx adf_nbuf
 *
 * TL will call this to notify the HDD when one or more packets were
 * received for a registered STA.
 * Return: VOS_STATUS_E_FAILURE if any errors encountered, VOS_STATUS_SUCCESS
 * otherwise
 */
VOS_STATUS hdd_mon_rx_packet_cbk(v_VOID_t *vos_ctx, adf_nbuf_t rx_buf,
				 uint8_t sta_id)
{
	hdd_adapter_t *adapter = NULL;
	hdd_context_t *hdd_ctx = NULL;
	int rxstat;
	struct sk_buff *skb = NULL;
	struct sk_buff *skb_next;
	unsigned int cpu_index;
	hdd_adapter_list_node_t *adapter_node = NULL;

	/* Sanity check on inputs */
	if ((NULL == vos_ctx) || (NULL == rx_buf)) {
		VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
			  "%s: Null params being passed", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	hdd_ctx = vos_get_context(VOS_MODULE_ID_HDD, vos_ctx);
	if (NULL == hdd_ctx) {
		VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
			  "%s: HDD adapter context is Null", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	hdd_get_front_adapter(hdd_ctx, &adapter_node);
	adapter = adapter_node->pAdapter;
	if ((NULL == adapter) || (WLAN_HDD_ADAPTER_MAGIC != adapter->magic)) {
		VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,
			  "invalid adapter %p for sta Id %d", adapter, sta_id);
		return VOS_STATUS_E_FAILURE;
	}

	cpu_index = wlan_hdd_get_cpu();

	/* walk the chain until all are processed */
	skb = (struct sk_buff *) rx_buf;
	while (NULL != skb) {
		skb_next = skb->next;
		skb->dev = adapter->dev;

		++adapter->hdd_stats.hddTxRxStats.rxPackets[cpu_index];
		++adapter->stats.rx_packets;
		adapter->stats.rx_bytes += skb->len;
		/*
		 * If this is not a last packet on the chain
		 * Just put packet into backlog queue, not scheduling RX sirq
		 */
		if (skb->next) {
			rxstat = netif_rx(skb);
		} else {
			/*
			 * This is the last packet on the chain
			 * Scheduling rx sirq
			 */
			rxstat = netif_rx_ni(skb);
		}

		if (NET_RX_SUCCESS == rxstat)
			++adapter->
				hdd_stats.hddTxRxStats.rxDelivered[cpu_index];
		else
			++adapter->hdd_stats.hddTxRxStats.rxRefused[cpu_index];

		skb = skb_next;
	}

	adapter->dev->last_rx = jiffies;

	return VOS_STATUS_SUCCESS;
}
/**============================================================================
  @brief hdd_rx_packet_cbk() - Receive callback registered with TL.
  TL will call this to notify the HDD when one or more packets were
  received for a registered STA.

  @param vosContext      : [in] pointer to VOS context
  @param staId           : [in] Station Id
  @param rxBuf           : [in] pointer to rx adf_nbuf

  @return                : VOS_STATUS_E_FAILURE if any errors encountered,
                         : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
VOS_STATUS hdd_rx_packet_cbk(v_VOID_t *vosContext,
                             adf_nbuf_t rxBuf, v_U8_t staId)
{
   hdd_adapter_t *pAdapter = NULL;
   hdd_context_t *pHddCtx = NULL;
   int rxstat;
   struct sk_buff *skb = NULL;
   struct sk_buff *skb_next;
   unsigned int cpu_index;
#ifdef QCA_PKT_PROTO_TRACE
   v_U8_t proto_type;
#endif /* QCA_PKT_PROTO_TRACE */
   hdd_station_ctx_t *pHddStaCtx = NULL;

   //Sanity check on inputs
   if ((NULL == vosContext) || (NULL == rxBuf))
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: Null params being passed", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   pHddCtx = (hdd_context_t *)vos_get_context( VOS_MODULE_ID_HDD, vosContext );
   if ( NULL == pHddCtx )
   {
      VOS_TRACE( VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_ERROR,"%s: HDD adapter context is Null", __func__);
      return VOS_STATUS_E_FAILURE;
   }

   pAdapter = pHddCtx->sta_to_adapter[staId];
   if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)) {
      hddLog(LOGE, FL("invalid adapter %p for sta Id %d"), pAdapter, staId);
      return VOS_STATUS_E_FAILURE;
   }

   if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic) {
       VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_FATAL,
          "Magic cookie(%x) for adapter sanity verification is invalid",
          pAdapter->magic);
       return VOS_STATUS_E_FAILURE;
   }

   cpu_index = wlan_hdd_get_cpu();

   // walk the chain until all are processed
   skb = (struct sk_buff *) rxBuf;
   pHddStaCtx = WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);
   while (NULL != skb) {
      skb_next = skb->next;

      if (((pHddStaCtx->conn_info.proxyARPService) &&
         cfg80211_is_gratuitous_arp_unsolicited_na(skb)) ||
         vos_is_load_unload_in_progress(VOS_MODULE_ID_VOSS, NULL)) {
            ++pAdapter->hdd_stats.hddTxRxStats.rxDropped[cpu_index];
            VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
               "%s: Dropping HS 2.0 Gratuitous ARP or Unsolicited NA"
               " else dropping as Driver load/unload is in progress",
               __func__);
            kfree_skb(skb);

            skb = skb_next;
            continue;
      }

      wlan_hdd_log_eapol(skb, WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED);

#ifdef QCA_PKT_PROTO_TRACE
      if ((pHddCtx->cfg_ini->gEnableDebugLog & VOS_PKT_TRAC_TYPE_EAPOL) ||
          (pHddCtx->cfg_ini->gEnableDebugLog & VOS_PKT_TRAC_TYPE_DHCP)) {
         proto_type = vos_pkt_get_proto_type(skb,
                        pHddCtx->cfg_ini->gEnableDebugLog, 0);
         if (VOS_PKT_TRAC_TYPE_EAPOL & proto_type)
            vos_pkt_trace_buf_update("ST:R:EPL");
         else if (VOS_PKT_TRAC_TYPE_DHCP & proto_type)
            vos_pkt_trace_buf_update("ST:R:DHC");
      }
#endif /* QCA_PKT_PROTO_TRACE */

      skb->dev = pAdapter->dev;
      skb->protocol = eth_type_trans(skb, skb->dev);

      /* Check & drop mcast packets (for IPV6) as required */
      if (drop_ip6_mcast(skb)) {
         hddLog(VOS_TRACE_LEVEL_INFO, "MAC Header:");
         VOS_TRACE_HEX_DUMP(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                            skb_mac_header(skb), 16);
         ++pAdapter->hdd_stats.hddTxRxStats.rxDropped[cpu_index];
         VOS_TRACE(VOS_MODULE_ID_HDD_DATA, VOS_TRACE_LEVEL_INFO,
                   "%s: Dropping multicast to self NA", __func__);
         kfree_skb(skb);

         skb = skb_next;
         continue;
      }

      ++pAdapter->hdd_stats.hddTxRxStats.rxPackets[cpu_index];
      ++pAdapter->stats.rx_packets;
      pAdapter->stats.rx_bytes += skb->len;

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

#ifdef FEATURE_WLAN_DIAG_SUPPORT

/**
 * wlan_hdd_get_eapol_params() - Function to extract EAPOL params
 * @skb:                sbb data
 * @eapol_params:       Pointer to hold the parsed EAPOL params
 * @event_type:         Event type to indicate Tx/Rx
 *
 * This function parses the input skb data to get the EAPOL params,if the
 * packet is EAPOL and store it in the pointer passed as input
 *
 * Return: 0 on success and negative value in failure
 *
 */
static int wlan_hdd_get_eapol_params(struct sk_buff *skb,
			      struct vos_event_wlan_eapol *eapol_params,
			      uint8_t event_type)
{
	bool ret;
	uint8_t packet_type=0;

	ret = wlan_hdd_is_eapol(skb);

	if (!ret)
		return -1;

	packet_type = (uint8_t)(*(uint8_t *)
			(skb->data + HDD_EAPOL_PACKET_TYPE_OFFSET));

	eapol_params->eapol_packet_type = packet_type;
	eapol_params->eapol_key_info = (uint16_t)(*(uint16_t *)
				       (skb->data + HDD_EAPOL_KEY_INFO_OFFSET));
	eapol_params->event_sub_type = event_type;
	eapol_params->eapol_rate = 0;/* As of now, zero */

	vos_mem_copy(eapol_params->dest_addr,
			(skb->data + HDD_EAPOL_DEST_MAC_OFFSET),
			sizeof(eapol_params->dest_addr));
	vos_mem_copy(eapol_params->src_addr,
			(skb->data + HDD_EAPOL_SRC_MAC_OFFSET),
			sizeof(eapol_params->src_addr));
	return 0;
}
/**
 * wlan_hdd_event_eapol_log() - Function to log EAPOL events
 * @eapol_params:    Structure containing EAPOL params
 *
 * This function logs the parsed EAPOL params
 *
 * Return: None
 *
 */
static void wlan_hdd_event_eapol_log(struct vos_event_wlan_eapol eapol_params)
{
	WLAN_VOS_DIAG_EVENT_DEF(wlan_diag_event, struct vos_event_wlan_eapol);

	wlan_diag_event.event_sub_type = eapol_params.event_sub_type;
	wlan_diag_event.eapol_packet_type = eapol_params.eapol_packet_type;
	wlan_diag_event.eapol_key_info = eapol_params.eapol_key_info;
	wlan_diag_event.eapol_rate = eapol_params.eapol_rate;
	vos_mem_copy(wlan_diag_event.dest_addr,
		     eapol_params.dest_addr,
		     sizeof (wlan_diag_event.dest_addr));
        vos_mem_copy(wlan_diag_event.src_addr,
		     eapol_params.src_addr,
		     sizeof (wlan_diag_event.src_addr));

	WLAN_VOS_DIAG_EVENT_REPORT(&wlan_diag_event, EVENT_WLAN_EAPOL);
}

/**
 * wlan_hdd_log_eapol() - Function to check and extract EAPOL params
 * @skb:               skb data
 * @event_type:        One of enum wifi_connectivity_events to indicate Tx/Rx
 *
 * This function parses the input skb data to get the EAPOL params,if the
 * packet is EAPOL and store it in the pointer passed as input
 *
 * Return: None
 *
 */
void wlan_hdd_log_eapol(struct sk_buff *skb,
		uint8_t event_type)
{
	int ret;
	struct vos_event_wlan_eapol eapol_params;

	ret = wlan_hdd_get_eapol_params(skb, &eapol_params, event_type);
	if (ret)
		return;

	wlan_hdd_event_eapol_log(eapol_params);
	if ((eapol_params.eapol_key_info & EAPOL_MASK) == EAPOL_M1_BIT_MASK) {
		hddLog(LOG1,
			FL("%s: M1 packet"), eapol_params.event_sub_type ==
				WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED ?
								"RX" : "TX");
	} else if ((eapol_params.eapol_key_info & EAPOL_MASK) ==
						EAPOL_M2_BIT_MASK) {
		hddLog(LOG1,
			FL("%s: M2 packet"), eapol_params.event_sub_type ==
				WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED ?
								"RX" : "TX");

	} else if ((eapol_params.eapol_key_info & EAPOL_MASK) ==
						EAPOL_M3_BIT_MASK) {
		hddLog(LOG1,
			FL("%s: M3 packet"), eapol_params.event_sub_type ==
				WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED ?
								"RX" : "TX");
	} else if ((eapol_params.eapol_key_info & EAPOL_MASK) ==
						EAPOL_M4_BIT_MASK) {
		hddLog(LOG1,
			FL("%s: M4 packet"), eapol_params.event_sub_type ==
				WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED ?
								"RX" : "TX");
	}
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
