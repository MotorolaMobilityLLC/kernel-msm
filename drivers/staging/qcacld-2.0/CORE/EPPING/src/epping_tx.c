/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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
#include <linux/rtnetlink.h>
#include <linux/semaphore.h>
#include <linux/ctype.h>
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_softap_tx_rx.h>
#include "epping_main.h"
#include "epping_internal.h"
#include "epping_test.h"

#define TX_RETRY_TIMEOUT_IN_MS 1

static bool enb_tx_dump = 0;

void epping_tx_dup_pkt(epping_adapter_t *pAdapter,
                       HTC_ENDPOINT_ID eid, adf_nbuf_t skb)
{
   struct epping_cookie * cookie = NULL;
   int skb_len, ret;
   adf_nbuf_t new_skb;

   cookie = epping_alloc_cookie(pAdapter->pEpping_ctx);
   if (cookie == NULL) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: epping_alloc_cookie returns no resource\n", __func__);
      return;
   }
   new_skb = adf_nbuf_copy(skb);
   if (!new_skb) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: adf_nbuf_copy returns no resource\n", __func__);
      epping_free_cookie(pAdapter->pEpping_ctx, cookie);
      return;
   }
   SET_HTC_PACKET_INFO_TX(&cookie->HtcPkt,
      cookie, adf_nbuf_data(skb), adf_nbuf_len(new_skb), eid, 0);
   SET_HTC_PACKET_NET_BUF_CONTEXT(&cookie->HtcPkt, new_skb);
   skb_len = (int)adf_nbuf_len(new_skb);
   /* send the packet */
   ret = HTCSendPkt(pAdapter->pEpping_ctx->HTCHandle, &cookie->HtcPkt);
   if (ret != A_OK) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: HTCSendPkt failed, ret = %d\n", __func__, ret);
      epping_free_cookie(pAdapter->pEpping_ctx, cookie);
      adf_nbuf_free(new_skb);
      return;
   }
   pAdapter->stats.tx_bytes += skb_len;
   ++pAdapter->stats.tx_packets;
   if (((pAdapter->stats.tx_packets +
         pAdapter->stats.tx_dropped) % EPPING_STATS_LOG_COUNT) == 0 &&
       (pAdapter->stats.tx_packets || pAdapter->stats.tx_dropped)) {
       epping_log_stats(pAdapter, __func__);
   }
}
static int epping_tx_send_int(adf_nbuf_t skb,
                              epping_adapter_t *pAdapter)
{
   EPPING_HEADER *eppingHdr = (EPPING_HEADER *)adf_nbuf_data(skb);
   HTC_ENDPOINT_ID eid = ENDPOINT_UNUSED;
   struct epping_cookie * cookie = NULL;
   A_UINT8 ac = 0;
   A_STATUS ret = A_OK;
   int skb_len;
   EPPING_HEADER tmpHdr = *eppingHdr;

   /* allocate resource for this packet */
   cookie = epping_alloc_cookie(pAdapter->pEpping_ctx);
   /* no resource */
   if (cookie == NULL) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
          "%s: epping_alloc_cookie returns no resource\n", __func__);
      return -1;
   }

   if (enb_tx_dump)
      epping_hex_dump((void *)eppingHdr, skb->len, __func__);
   /*
    * a quirk of linux, the payload of the frame is 32-bit aligned and thus
    * the addition of the HTC header will mis-align the start of the HTC
    * frame, so we add some padding which will be stripped off in the target
    */
   if (EPPING_ALIGNMENT_PAD > 0) {
      A_NETBUF_PUSH(skb, EPPING_ALIGNMENT_PAD);
   }
   /* prepare ep/HTC information */
   ac = eppingHdr->StreamNo_h;
   eid = pAdapter->pEpping_ctx->EppingEndpoint[ac];
   if (eid < 0 || eid >= EPPING_MAX_NUM_EPIDS) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: invalid eid = %d, ac = %d\n", __func__, eid, ac);
      return -1;
   }
   if (tmpHdr.Cmd_h == EPPING_CMD_RESET_RECV_CNT ||
      tmpHdr.Cmd_h == EPPING_CMD_CONT_RX_START) {
      epping_set_kperf_flag(pAdapter, eid, tmpHdr.CmdBuffer_t[0]);
   }
   if (pAdapter->pEpping_ctx->kperf[eid]) {
      switch (tmpHdr.Cmd_h) {
      case EPPING_CMD_NO_ECHO:
#ifdef HIF_PCI
         epping_tx_copier_schedule(pAdapter->pEpping_ctx, eid, skb);
#endif /* HIF_PCI */
         break;
      default:
         break;
      }
   }
   if (pAdapter->pEpping_ctx->kperf[eid] &&
      tmpHdr.Cmd_h == EPPING_CMD_NO_ECHO) {
      epping_tx_dup_pkt(pAdapter, eid, skb);
   }
   SET_HTC_PACKET_INFO_TX(&cookie->HtcPkt,
      cookie, adf_nbuf_data(skb), adf_nbuf_len(skb), eid, 0);
   SET_HTC_PACKET_NET_BUF_CONTEXT(&cookie->HtcPkt, skb);
   skb_len = skb->len;
   /* send the packet */
   ret = HTCSendPkt(pAdapter->pEpping_ctx->HTCHandle, &cookie->HtcPkt);
   epping_log_packet(pAdapter, &tmpHdr, ret, __func__);
   if (ret != A_OK) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: HTCSendPkt failed, status = %d\n", __func__, ret);
      epping_free_cookie(pAdapter->pEpping_ctx, cookie);
      return -1;
   }
   pAdapter->stats.tx_bytes += skb_len;
   ++pAdapter->stats.tx_packets;
   if (((pAdapter->stats.tx_packets +
         pAdapter->stats.tx_dropped) % EPPING_STATS_LOG_COUNT) == 0 &&
       (pAdapter->stats.tx_packets || pAdapter->stats.tx_dropped)) {
      epping_log_stats(pAdapter, __func__);
   }

   return 0;
}

void epping_tx_timer_expire(epping_adapter_t *pAdapter)
{
   adf_nbuf_t nodrop_skb;

   EPPING_LOG(VOS_TRACE_LEVEL_INFO, "%s: queue len: %d\n", __func__,
      adf_nbuf_queue_len(&pAdapter->nodrop_queue));

   if (!adf_nbuf_queue_len(&pAdapter->nodrop_queue)) {
      /* nodrop queue is empty so no need to arm timer */
      pAdapter->epping_timer_state = EPPING_TX_TIMER_STOPPED;
      return;
   }

   /* try to flush nodrop queue */
   while ((nodrop_skb = adf_nbuf_queue_remove(&pAdapter->nodrop_queue))) {
      if (epping_tx_send_int(nodrop_skb, pAdapter)) {
         EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
            "%s: nodrop: %p xmit fail in timer\n", __func__, nodrop_skb);
         /* fail to xmit so put the nodrop packet to the nodrop queue */
         adf_nbuf_queue_insert_head(&pAdapter->nodrop_queue, nodrop_skb);
         break;
      } else {
         EPPING_LOG(VOS_TRACE_LEVEL_INFO,
            "%s: nodrop: %p xmit ok in timer\n", __func__, nodrop_skb);
      }
   }

   /* if nodrop queue is not empty, continue to arm timer */
   if (nodrop_skb) {
      adf_os_spin_lock_bh(&pAdapter->data_lock);
      /* if nodrop queue is not empty, continue to arm timer */
      if (pAdapter->epping_timer_state != EPPING_TX_TIMER_RUNNING) {
         pAdapter->epping_timer_state = EPPING_TX_TIMER_RUNNING;
         adf_os_timer_mod(&pAdapter->epping_timer, TX_RETRY_TIMEOUT_IN_MS);
      }
      adf_os_spin_unlock_bh(&pAdapter->data_lock);
   } else {
      pAdapter->epping_timer_state = EPPING_TX_TIMER_STOPPED;
   }
}

int epping_tx_send(adf_nbuf_t skb, epping_adapter_t *pAdapter)
{
   adf_nbuf_t nodrop_skb;
   EPPING_HEADER *eppingHdr;
   A_UINT8 ac = 0;

   eppingHdr = (EPPING_HEADER *)adf_nbuf_data(skb);

   if (!IS_EPPING_PACKET(eppingHdr)) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: Recived non endpoint ping packets\n", __func__);
      /* no packet to send, cleanup */
      adf_nbuf_free(skb);
      return -ENOMEM;
   }

   /* the stream ID is mapped to an access class */
   ac = eppingHdr->StreamNo_h;
   /* hard coded two ep ids */
   if (ac != 0 && ac != 1) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: ac %d is not mapped to mboxping service\n", __func__, ac);
      adf_nbuf_free(skb);
      return -ENOMEM;
   }

   /*
    * some EPPING packets cannot be dropped no matter what access class
    * it was sent on. A special care has been taken:
    * 1. when there is no TX resource, queue the control packets to
    *    a special queue
    * 2. when there is TX resource, send the queued control packets first
    *    and then other packets
    * 3. a timer launches to check if there is queued control packets and
    *    flush them
    */

   /* check the nodrop queue first */
   while ((nodrop_skb = adf_nbuf_queue_remove(&pAdapter->nodrop_queue))) {
      if (epping_tx_send_int(nodrop_skb, pAdapter)) {
         EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
            "%s: nodrop: %p xmit fail\n", __func__, nodrop_skb);
         /* fail to xmit so put the nodrop packet to the nodrop queue */
         adf_nbuf_queue_insert_head(&pAdapter->nodrop_queue, nodrop_skb);
         /* no cookie so free the current skb */
         goto tx_fail;
      } else {
         EPPING_LOG(VOS_TRACE_LEVEL_INFO,
            "%s: nodrop: %p xmit ok\n", __func__, nodrop_skb);
      }
   }

   /* send the original packet */
   if (epping_tx_send_int(skb, pAdapter))
      goto tx_fail;

   return 0;

tx_fail:
   if (!IS_EPING_PACKET_NO_DROP(eppingHdr)) {
      /* allow to drop the skb so drop it */
      adf_nbuf_free(skb);
      ++pAdapter->stats.tx_dropped;
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: Tx skb %p dropped, stats.tx_dropped = %ld\n",
         __func__, skb, pAdapter->stats.tx_dropped);
      return -ENOMEM;
   } else {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
                 "%s: nodrop: %p queued\n", __func__, skb);
      adf_nbuf_queue_add(&pAdapter->nodrop_queue, skb);
      adf_os_spin_lock_bh(&pAdapter->data_lock);
      if (pAdapter->epping_timer_state != EPPING_TX_TIMER_RUNNING) {
         pAdapter->epping_timer_state = EPPING_TX_TIMER_RUNNING;
         adf_os_timer_mod(&pAdapter->epping_timer, TX_RETRY_TIMEOUT_IN_MS);
      }
      adf_os_spin_unlock_bh(&pAdapter->data_lock);
   }

   return 0;
}

#ifdef HIF_SDIO
HTC_SEND_FULL_ACTION epping_tx_queue_full(void *Context,
   HTC_PACKET *pPacket)
{
   epping_context_t *pEpping_ctx = (epping_context_t *)Context;
   epping_adapter_t *pAdapter = pEpping_ctx->epping_adapter;
   HTC_SEND_FULL_ACTION action = HTC_SEND_FULL_KEEP;
   netif_stop_queue(pAdapter->dev);
   return action;
}
#endif /* HIF_SDIO */
void epping_tx_complete_multiple(void *ctx,
   HTC_PACKET_QUEUE *pPacketQueue)
{
   epping_context_t *pEpping_ctx = (epping_context_t *)ctx;
   epping_adapter_t *pAdapter = pEpping_ctx->epping_adapter;
   struct net_device* dev = pAdapter->dev;
   A_STATUS status;
   HTC_ENDPOINT_ID eid;
   adf_nbuf_t pktSkb;
   struct epping_cookie *cookie;
   A_BOOL flushing = FALSE;
   adf_nbuf_queue_t skb_queue;
   HTC_PACKET *htc_pkt;

   adf_nbuf_queue_init(&skb_queue);

   adf_os_spin_lock_bh(&pAdapter->data_lock);

   while (!HTC_QUEUE_EMPTY(pPacketQueue)) {
      htc_pkt = HTC_PACKET_DEQUEUE(pPacketQueue);
      if (htc_pkt == NULL)
         break;
      status=htc_pkt->Status;
      eid=htc_pkt->Endpoint;
      pktSkb=GET_HTC_PACKET_NET_BUF_CONTEXT(htc_pkt);
      cookie = htc_pkt->pPktContext;

      ASSERT(pktSkb);
      ASSERT(htc_pkt->pBuffer == adf_nbuf_data(pktSkb));

      /* add this to the list, use faster non-lock API */
      adf_nbuf_queue_add(&skb_queue,pktSkb);

      if (A_SUCCESS(status)) {
         ASSERT(htc_pkt->ActualLength == adf_nbuf_len(pktSkb));
      }
      EPPING_LOG(VOS_TRACE_LEVEL_INFO,
         "%s skb=%p data=%p len=0x%x eid=%d ",
         __func__, pktSkb, htc_pkt->pBuffer,
         htc_pkt->ActualLength, eid);

      if (A_FAILED(status)) {
         if (status == A_ECANCELED) {
            /* a packet was flushed  */
            flushing = TRUE;
         }
         if (status != A_NO_RESOURCE) {
            printk("%s() -TX ERROR, status: 0x%x\n", __func__,
               status);
         }
      } else {
         EPPING_LOG(VOS_TRACE_LEVEL_INFO, "%s: OK\n", __func__);
         flushing = FALSE;
      }

      epping_free_cookie(pAdapter->pEpping_ctx, cookie);
   }

   adf_os_spin_unlock_bh(&pAdapter->data_lock);

   /* free all skbs in our local list */
   while (adf_nbuf_queue_len(&skb_queue)) {
      /* use non-lock version */
      pktSkb = adf_nbuf_queue_remove(&skb_queue);
      if (pktSkb == NULL)
         break;
      adf_nbuf_free(pktSkb);
      pEpping_ctx->total_tx_acks++;
   }

   if (!flushing) {
      netif_wake_queue(dev);
   }
}
