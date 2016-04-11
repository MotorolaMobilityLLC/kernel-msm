/*
 * Copyright (c) 2014, 2016 The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
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
#include <linux/delay.h>
#include <linux/ctype.h>
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_softap_tx_rx.h>
#include "epping_main.h"
#include "epping_internal.h"

int epping_cookie_init(epping_context_t*pEpping_ctx)
{
   A_UINT32 i, j;

   pEpping_ctx->cookie_list = NULL;
   pEpping_ctx->cookie_count = 0;
   for (i = 0; i < MAX_COOKIE_SLOTS_NUM; i++) {
      pEpping_ctx->s_cookie_mem[i] =
         vos_mem_malloc(sizeof(struct epping_cookie)*MAX_COOKIE_SLOT_SIZE);
      if (pEpping_ctx->s_cookie_mem[i] == NULL) {
         EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
            "%s: no mem for cookie (idx = %d)", __func__, i);
         goto error;
      }
      vos_mem_zero(pEpping_ctx->s_cookie_mem[i],
         sizeof(struct epping_cookie)*MAX_COOKIE_SLOT_SIZE);
   }
   adf_os_spinlock_init(&pEpping_ctx->cookie_lock);

   for (i = 0; i < MAX_COOKIE_SLOTS_NUM; i++) {
      struct epping_cookie *cookie_mem = pEpping_ctx->s_cookie_mem[i];
      for (j = 0; j < MAX_COOKIE_SLOT_SIZE; j++) {
         epping_free_cookie(pEpping_ctx, &cookie_mem[j]);
      }
   }
   return 0;
error:
   for (i = 0; i < MAX_COOKIE_SLOTS_NUM; i++) {
      if (pEpping_ctx->s_cookie_mem[i]) {
         vos_mem_free(pEpping_ctx->s_cookie_mem[i]);
         pEpping_ctx->s_cookie_mem[i] = NULL;
      }
   }
   return -ENOMEM;
}

/* cleanup cookie queue */
void epping_cookie_cleanup(epping_context_t*pEpping_ctx)
{
   int i;
   adf_os_spin_lock_bh(&pEpping_ctx->cookie_lock);
   pEpping_ctx->cookie_list = NULL;
   pEpping_ctx->cookie_count = 0;
   adf_os_spin_unlock_bh(&pEpping_ctx->cookie_lock);
   for (i = 0; i < MAX_COOKIE_SLOTS_NUM; i++) {
      if (pEpping_ctx->s_cookie_mem[i]) {
         vos_mem_free(pEpping_ctx->s_cookie_mem[i]);
         pEpping_ctx->s_cookie_mem[i] = NULL;
      }
   }
}

void epping_free_cookie(epping_context_t*pEpping_ctx,
                        struct epping_cookie *cookie)
{
   adf_os_spin_lock_bh(&pEpping_ctx->cookie_lock);
   cookie->next = pEpping_ctx->cookie_list;
   pEpping_ctx->cookie_list = cookie;
   pEpping_ctx->cookie_count++;
   adf_os_spin_unlock_bh(&pEpping_ctx->cookie_lock);
}

struct epping_cookie *epping_alloc_cookie(epping_context_t*pEpping_ctx)
{
   struct epping_cookie   *cookie;

   adf_os_spin_lock_bh(&pEpping_ctx->cookie_lock);
   cookie = pEpping_ctx->cookie_list;
   if(cookie != NULL)
   {
      pEpping_ctx->cookie_list = cookie->next;
      pEpping_ctx->cookie_count--;
   }
   adf_os_spin_unlock_bh(&pEpping_ctx->cookie_lock);
   return cookie;
}

void epping_get_dummy_mac_addr(tSirMacAddr macAddr)
{
   macAddr[0] = 69; /* E */
   macAddr[1] = 80; /* P */
   macAddr[2] = 80; /* P */
   macAddr[3] = 73; /* I */
   macAddr[4] = 78; /* N */
   macAddr[5] = 71; /* G */
}

void epping_hex_dump(void *data, int buf_len, const char *str)
{
   char *buf = (char *)data;
   int i;

   printk("%s: E, %s\n", __func__, str);
   for (i=0; (i+7)< buf_len; i+=8)
   {
      printk("%02x %02x %02x %02x %02x %02x %02x %02x\n",
         buf[i],
         buf[i+1],
         buf[i+2],
         buf[i+3],
         buf[i+4],
         buf[i+5],
         buf[i+6],
         buf[i+7]);
   }

   // Dump the bytes in the last line
   for (; i < buf_len; i++)
   {
      printk("%02x ", buf[i]);
   }
   printk("\n%s: X %s\n", __func__, str);
}


void *epping_get_adf_ctx(void)
{
   VosContextType *pVosContext = NULL;
   adf_os_device_t *pAdfCtx;
   pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
   pAdfCtx = vos_get_context(VOS_MODULE_ID_ADF, pVosContext);
   return pAdfCtx;
}

void epping_log_packet(epping_adapter_t *pAdapter,
                       EPPING_HEADER *eppingHdr, int ret, const char *str)
{
   if (eppingHdr->Cmd_h & EPPING_LOG_MASK) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
         "%s: cmd = %d, seqNo = %u, flag = 0x%x, ret = %d, "
         "txCount = %lu, txDrop =  %lu, txBytes = %lu,"
         "rxCount = %lu, rxDrop = %lu, rxBytes = %lu\n",
         str, eppingHdr->Cmd_h, eppingHdr->SeqNo,
         eppingHdr->CmdFlags_h, ret,
         pAdapter->stats.tx_packets,
         pAdapter->stats.tx_dropped,
         pAdapter->stats.tx_bytes,
         pAdapter->stats.rx_packets,
         pAdapter->stats.rx_dropped,
         pAdapter->stats.rx_bytes);
   }
}

void epping_log_stats(epping_adapter_t *pAdapter, const char *str)
{
   EPPING_LOG(VOS_TRACE_LEVEL_FATAL,
      "%s: txCount = %lu, txDrop = %lu, tx_bytes = %lu, "
      "rxCount = %lu, rxDrop = %lu, rx_bytes = %lu, tx_acks = %u\n",
      str,
      pAdapter->stats.tx_packets,
      pAdapter->stats.tx_dropped,
      pAdapter->stats.tx_bytes,
      pAdapter->stats.rx_packets,
      pAdapter->stats.rx_dropped,
      pAdapter->stats.rx_bytes,
      pAdapter->pEpping_ctx->total_tx_acks);
}

void epping_set_kperf_flag(epping_adapter_t *pAdapter,
                           HTC_ENDPOINT_ID eid,
                           A_UINT8 kperf_flag)
{
   pAdapter->pEpping_ctx->kperf[eid] = kperf_flag;
   pAdapter->pEpping_ctx->kperf_num_rx_recv[eid] = 0;
   pAdapter->pEpping_ctx->kperf_num_tx_acks[eid] = 0;
}

#ifdef HIF_PCI

static int epping_tx_thread_fn(void *data)
{
    int i;
   epping_poll_t *epping_poll = data;

   EPPING_LOG(VOS_TRACE_LEVEL_FATAL, "%s: arg = %p", __func__, data);
   while (!epping_poll->done) {
      down(&epping_poll->sem);
      adf_os_atomic_dec(&epping_poll->atm);
      if (epping_poll->skb && !epping_poll->done) {
         for (i = 0; i < MAX_TX_PKT_DUP_NUM; i++) {
            epping_tx_dup_pkt((epping_adapter_t *)epping_poll->arg,
               epping_poll->eid, epping_poll->skb);
            udelay(WLAN_EPPING_DELAY_TIMEOUT_US);
         }
      }
   }
   return 0;
}

#define EPPING_TX_THREAD "EPPINGTX"
void epping_register_tx_copier(HTC_ENDPOINT_ID eid, epping_context_t *pEpping_ctx)
{
   epping_poll_t *epping_poll = &pEpping_ctx->epping_poll[eid];
   epping_poll->eid = eid;
   epping_poll->arg = pEpping_ctx->epping_adapter;
   epping_poll->done = false;
   EPPING_LOG(VOS_TRACE_LEVEL_FATAL, "%s: eid = %d, arg = %p",
              __func__, eid, pEpping_ctx->epping_adapter);
   sema_init(&epping_poll->sem, 0);
   adf_os_atomic_init(&epping_poll->atm);
   epping_poll->inited = true;
   epping_poll->pid = kthread_create(epping_tx_thread_fn,
                                     epping_poll, EPPING_TX_THREAD);
   wake_up_process(epping_poll->pid);
}
void epping_unregister_tx_copier(HTC_ENDPOINT_ID eid, epping_context_t *pEpping_ctx)
{
   epping_poll_t *epping_poll;

   if (eid < 0 || eid >= EPPING_MAX_NUM_EPIDS ) {
      EPPING_LOG(VOS_TRACE_LEVEL_FATAL, "%s: invalid eid = %d",
         __func__, eid);
      return;
   }

   epping_poll = &pEpping_ctx->epping_poll[eid];

   epping_poll->done = true;
   if (epping_poll->inited) {
      epping_tx_copier_schedule(pEpping_ctx, eid, NULL);
      msleep(EPPING_KTID_KILL_WAIT_TIME_MS);
   }
   if (epping_poll->skb)
      adf_nbuf_free(epping_poll->skb);
   OS_MEMZERO(epping_poll, sizeof(epping_poll_t));
   EPPING_LOG(VOS_TRACE_LEVEL_FATAL, "%s: eid = %d",
               __func__, eid);
}
void epping_tx_copier_schedule(epping_context_t *pEpping_ctx, HTC_ENDPOINT_ID eid, adf_nbuf_t skb)
{
   epping_poll_t *epping_poll = &pEpping_ctx->epping_poll[eid];

   if (!epping_poll->skb && skb) {
      epping_poll->skb = adf_nbuf_copy(skb);
   }
   if (adf_os_atomic_read(&epping_poll->atm) < EPPING_MAX_WATER_MARK) {
      adf_os_atomic_inc(&epping_poll->atm);
      up(&epping_poll->sem);
   }
}
#endif /* HIF_PCI */
