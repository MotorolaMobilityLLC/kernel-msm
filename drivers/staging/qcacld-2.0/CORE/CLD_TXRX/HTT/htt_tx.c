/*
 * Copyright (c) 2011, 2014 The Linux Foundation. All rights reserved.
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

/**
 * @file htt_tx.c
 * @brief Implement transmit aspects of HTT.
 * @details
 *  This file contains three categories of HTT tx code:
 *  1.  An abstraction of the tx descriptor, to hide the
 *      differences between the HL vs. LL tx descriptor.
 *  2.  Functions for allocating and freeing HTT tx descriptors.
 *  3.  The function that accepts a tx frame from txrx and sends the
 *      tx frame to HTC.
 */
#include <osdep.h>          /* u_int32_t, offsetof, etc. */
#include <adf_os_types.h>   /* adf_os_dma_addr_t */
#include <adf_os_mem.h>     /* adf_os_mem_alloc_consistent,free_consistent */
#include <adf_nbuf.h>       /* adf_nbuf_t, etc. */
#include <adf_os_time.h>    /* adf_os_mdelay */

#include <htt.h>             /* htt_tx_msdu_desc_t */
#include <htc.h>             /* HTC_HDR_LENGTH */
#include <htc_api.h>         /* HTCFlushSurpriseRemove */
#include <ol_cfg.h>          /* ol_cfg_netbuf_frags_max, etc. */
#include <ol_htt_tx_api.h>   /* HTT_TX_DESC_VADDR_OFFSET */
#include <ol_txrx_htt_api.h> /* ol_tx_msdu_id_storage */
#include <htt_internal.h>

#ifdef IPA_UC_OFFLOAD
/* IPA Micro controler TX data packet HTT Header Preset */
/* 31 | 30  29 | 28 | 27 | 26  22  | 21   16 | 15  13   | 12  8       | 7 0
 *------------------------------------------------------------------------------
 * R  | CS  OL | R  | PP | ext TID | vdev ID | pkt type | pkt subtype | msg type
 * 0  | 0      | 0  |    | 0x1F    | 0       | 2        | 0           | 0x01
 *------------------------------------------------------------------------------
 * pkt ID                                    | pkt length
 *------------------------------------------------------------------------------
 *                                frag_desc_ptr
 *------------------------------------------------------------------------------
 *                                   peer_id
 *------------------------------------------------------------------------------
 */
#define HTT_IPA_UC_OFFLOAD_TX_HEADER_DEFAULT 0x07C04001
#endif /* IPA_UC_OFFLOAD */

/*--- setup / tear-down functions -------------------------------------------*/

#ifdef QCA_SUPPORT_TXDESC_SANITY_CHECKS
u_int32_t *g_dbg_htt_desc_end_addr, *g_dbg_htt_desc_start_addr;
#endif

int
htt_tx_attach(struct htt_pdev_t *pdev, int desc_pool_elems)
{
    int i, pool_size;
    u_int32_t **p;
    adf_os_dma_addr_t pool_paddr = {0};

    if (pdev->cfg.is_high_latency) {
        pdev->tx_descs.size = sizeof(struct htt_host_tx_desc_t);
    } else {
        pdev->tx_descs.size =
            /*
             * Start with the size of the base struct
             * that actually gets downloaded.
             */
            sizeof(struct htt_host_tx_desc_t)
            /*
             * Add the fragmentation descriptor elements.
             * Add the most that the OS may deliver, plus one more in
             * case the txrx code adds a prefix fragment (for TSO or
             * audio interworking SNAP header)
             */
            + (ol_cfg_netbuf_frags_max(pdev->ctrl_pdev)+1) * 8 // 2x u_int32_t
            + 4; /* u_int32_t fragmentation list terminator */
    }

    /*
     * Make sure tx_descs.size is a multiple of 4-bytes.
     * It should be, but round up just to be sure.
     */
    pdev->tx_descs.size = (pdev->tx_descs.size + 3) & (~0x3);

    pdev->tx_descs.pool_elems = desc_pool_elems;
    pdev->tx_descs.alloc_cnt = 0;

    pool_size = pdev->tx_descs.pool_elems * pdev->tx_descs.size;

    if (pdev->cfg.is_high_latency)
        pdev->tx_descs.pool_vaddr = adf_os_mem_alloc(pdev->osdev, pool_size);
    else
        pdev->tx_descs.pool_vaddr =
        adf_os_mem_alloc_consistent( pdev->osdev, pool_size, &pool_paddr,
            adf_os_get_dma_mem_context((&pdev->tx_descs), memctx));

    pdev->tx_descs.pool_paddr = pool_paddr;

    if (!pdev->tx_descs.pool_vaddr) {
        return 1; /* failure */
    }

    adf_os_print("%s:htt_desc_start:0x%p htt_desc_end:0x%p\n", __func__,
                 pdev->tx_descs.pool_vaddr,
                 (u_int32_t *) (pdev->tx_descs.pool_vaddr + pool_size));

#ifdef QCA_SUPPORT_TXDESC_SANITY_CHECKS
    g_dbg_htt_desc_end_addr = (u_int32_t *)
                         (pdev->tx_descs.pool_vaddr + pool_size);
    g_dbg_htt_desc_start_addr = (u_int32_t *) pdev->tx_descs.pool_vaddr;
#endif

    /* link tx descriptors into a freelist */
    pdev->tx_descs.freelist = (u_int32_t *) pdev->tx_descs.pool_vaddr;
    p = (u_int32_t **) pdev->tx_descs.freelist;
    for (i = 0; i < desc_pool_elems - 1; i++) {
        *p = (u_int32_t *) (((char *) p) + pdev->tx_descs.size);
        p = (u_int32_t **) *p;
    }
    *p = NULL;

    return 0; /* success */
}

void
htt_tx_detach(struct htt_pdev_t *pdev)
{
    if (pdev){
        if (pdev->cfg.is_high_latency)
            adf_os_mem_free(pdev->tx_descs.pool_vaddr);
        else
            adf_os_mem_free_consistent(
            pdev->osdev,
            pdev->tx_descs.pool_elems * pdev->tx_descs.size, /* pool_size */
            pdev->tx_descs.pool_vaddr,
            pdev->tx_descs.pool_paddr,
            adf_os_get_dma_mem_context((&pdev->tx_descs), memctx));
        }
}


/*--- descriptor allocation functions ---------------------------------------*/

void *
htt_tx_desc_alloc(htt_pdev_handle pdev, u_int32_t *paddr_lo)
{
    struct htt_host_tx_desc_t *htt_host_tx_desc; /* includes HTC hdr space */
    struct htt_tx_msdu_desc_t *htt_tx_desc;      /* doesn't include HTC hdr */

    htt_host_tx_desc = (struct htt_host_tx_desc_t *) pdev->tx_descs.freelist;
    if (! htt_host_tx_desc) {
        return NULL; /* pool is exhausted */
    }
    htt_tx_desc = &htt_host_tx_desc->align32.tx_desc;

    if (pdev->tx_descs.freelist) {
        pdev->tx_descs.freelist = *((u_int32_t **) pdev->tx_descs.freelist);
        pdev->tx_descs.alloc_cnt++;
    }
    /*
     * For LL, set up the fragmentation descriptor address.
     * Currently, this HTT tx desc allocation is performed once up front.
     * If this is changed to have the allocation done during tx, then it
     * would be helpful to have separate htt_tx_desc_alloc functions for
     * HL vs. LL, to remove the below conditional branch.
     */
    if (!pdev->cfg.is_high_latency) {
        u_int32_t *fragmentation_descr_field_ptr;

        fragmentation_descr_field_ptr = (u_int32_t *)
            ((u_int32_t *) htt_tx_desc) +
            HTT_TX_DESC_FRAGS_DESC_PADDR_OFFSET_DWORD;
        /*
         * The fragmentation descriptor is allocated from consistent mem.
         * Therefore, we can use the address directly rather than having
         * to map it from a virtual/CPU address to a physical/bus address.
         */
        *fragmentation_descr_field_ptr =
            HTT_TX_DESC_PADDR(pdev, htt_tx_desc) + HTT_TX_DESC_LEN;
    }
    /*
     * Include the headroom for the HTC frame header when specifying the
     * physical address for the HTT tx descriptor.
     */
    *paddr_lo = (u_int32_t) HTT_TX_DESC_PADDR(pdev, htt_host_tx_desc);
    /*
     * The allocated tx descriptor space includes headroom for a
     * HTC frame header.  Hide this headroom, so that we don't have
     * to jump past the headroom each time we program a field within
     * the tx desc, but only once when we download the tx desc (and
     * the headroom) to the target via HTC.
     * Skip past the headroom and return the address of the HTT tx desc.
     */
    return (void *) htt_tx_desc;
}

void
htt_tx_desc_free(htt_pdev_handle pdev, void *tx_desc)
{
    char *htt_host_tx_desc = tx_desc;
    /* rewind over the HTC frame header space */
    htt_host_tx_desc -= offsetof(struct htt_host_tx_desc_t, align32.tx_desc);
    *((u_int32_t **) htt_host_tx_desc) = pdev->tx_descs.freelist;
    pdev->tx_descs.freelist = (u_int32_t *) htt_host_tx_desc;
    pdev->tx_descs.alloc_cnt--;
}

/*--- descriptor field access methods ---------------------------------------*/

void htt_tx_desc_frags_table_set(
    htt_pdev_handle pdev,
    void *htt_tx_desc,
    u_int32_t paddr,
    int reset)
{
    u_int32_t *fragmentation_descr_field_ptr;

    /* fragments table only applies to LL systems */
    if (pdev->cfg.is_high_latency) {
        return;
    }
    fragmentation_descr_field_ptr = (u_int32_t *)
        ((u_int32_t *) htt_tx_desc) + HTT_TX_DESC_FRAGS_DESC_PADDR_OFFSET_DWORD;
    if (reset) {
        *fragmentation_descr_field_ptr =
            HTT_TX_DESC_PADDR(pdev, htt_tx_desc) + HTT_TX_DESC_LEN;
    } else {
        *fragmentation_descr_field_ptr = paddr;
    }
}

/* PUT THESE AS INLINE IN ol_htt_tx_api.h */

void
htt_tx_desc_flag_postponed(htt_pdev_handle pdev, void *desc)
{
}

#if !defined(CONFIG_HL_SUPPORT)
void
htt_tx_pending_discard(htt_pdev_handle pdev)
{
    HTCFlushSurpriseRemove(pdev->htc_pdev);
}
#endif

void
htt_tx_desc_flag_batch_more(htt_pdev_handle pdev, void *desc)
{
}

/*--- tx send function ------------------------------------------------------*/

#ifdef ATH_11AC_TXCOMPACT

/* Scheduling the Queued packets in HTT which could not be sent out because of No CE desc*/
void
htt_tx_sched(htt_pdev_handle pdev)
{
    adf_nbuf_t msdu;
    int download_len = pdev->download_len;
    int packet_len;

        HTT_TX_NBUF_QUEUE_REMOVE(pdev, msdu);
        while (msdu != NULL){
	int not_accepted;
        /* packet length includes HTT tx desc frag added above */
        packet_len = adf_nbuf_len(msdu);
        if (packet_len < download_len) {
            /*
            * This case of packet length being less than the nominal download
            * length can happen for a couple reasons:
            * In HL, the nominal download length is a large artificial value.
            * In LL, the frame may not have the optional header fields
            * accounted for in the nominal download size (LLC/SNAP header,
            * IPv4 or IPv6 header).
             */
            download_len = packet_len;
        }


        not_accepted = HTCSendDataPkt(
            pdev->htc_pdev, msdu, pdev->htc_endpoint, download_len);
        if (not_accepted) {
            HTT_TX_NBUF_QUEUE_INSERT_HEAD(pdev, msdu);
            return;
        }
        HTT_TX_NBUF_QUEUE_REMOVE(pdev, msdu);
    }
}


int
htt_tx_send_std(
        htt_pdev_handle pdev,
        adf_nbuf_t msdu,
        u_int16_t msdu_id)
{

    int download_len = pdev->download_len;

    int packet_len;

    /* packet length includes HTT tx desc frag added above */
    packet_len = adf_nbuf_len(msdu);
    if (packet_len < download_len) {
        /*
        * This case of packet length being less than the nominal download
        * length can happen for a couple reasons:
        * In HL, the nominal download length is a large artificial value.
        * In LL, the frame may not have the optional header fields
        * accounted for in the nominal download size (LLC/SNAP header,
        * IPv4 or IPv6 header).
         */
        download_len = packet_len;
    }

    if (adf_nbuf_queue_len(&pdev->txnbufq) > 0) {
        HTT_TX_NBUF_QUEUE_ADD(pdev, msdu);
        htt_tx_sched(pdev);
        return 0;
    }

    adf_nbuf_trace_update(msdu, "HT:T:");
    if (HTCSendDataPkt(pdev->htc_pdev, msdu, pdev->htc_endpoint, download_len)){
        HTT_TX_NBUF_QUEUE_ADD(pdev, msdu);
    }

    return 0; /* success */

}

adf_nbuf_t
htt_tx_send_batch(htt_pdev_handle pdev, adf_nbuf_t head_msdu, int num_msdus)
{
    adf_os_print("*** %s curently only applies for HL systems\n", __func__);
    adf_os_assert(0);
    return head_msdu;

}

int
htt_tx_send_nonstd(
    htt_pdev_handle pdev,
    adf_nbuf_t msdu,
    u_int16_t msdu_id,
    enum htt_pkt_type pkt_type)
{
    int download_len;

    /*
     * The pkt_type could be checked to see what L2 header type is present,
     * and then the L2 header could be examined to determine its length.
     * But for simplicity, just use the maximum possible header size,
     * rather than computing the actual header size.
     */
    download_len =
        sizeof(struct htt_host_tx_desc_t) +
        HTT_TX_HDR_SIZE_OUTER_HDR_MAX + /* worst case */
        HTT_TX_HDR_SIZE_802_1Q +
        HTT_TX_HDR_SIZE_LLC_SNAP +
        ol_cfg_tx_download_size(pdev->ctrl_pdev);
    adf_os_assert(download_len <= pdev->download_len);
    return htt_tx_send_std(pdev, msdu, msdu_id);
}

#else  /*ATH_11AC_TXCOMPACT*/

#ifdef QCA_TX_HTT2_SUPPORT
static inline HTC_ENDPOINT_ID
htt_tx_htt2_get_ep_id(
    htt_pdev_handle pdev,
    adf_nbuf_t msdu)
{
    /*
     * TX HTT2 service mainly for small sized frame and check if
     * this candidate frame allow or not.
     */
    if ((pdev->htc_tx_htt2_endpoint != ENDPOINT_UNUSED) &&
        adf_nbuf_get_tx_parallel_dnload_frm(msdu) &&
        (adf_nbuf_len(msdu) < pdev->htc_tx_htt2_max_size))
        return pdev->htc_tx_htt2_endpoint;
    else
        return pdev->htc_endpoint;
}
#else
#define htt_tx_htt2_get_ep_id(pdev, msdu)     (pdev->htc_endpoint)
#endif /* QCA_TX_HTT2_SUPPORT */

static inline int
htt_tx_send_base(
    htt_pdev_handle pdev,
    adf_nbuf_t msdu,
    u_int16_t msdu_id,
    int download_len,
    u_int8_t more_data)
{
    struct htt_host_tx_desc_t *htt_host_tx_desc;
    struct htt_htc_pkt *pkt;
    int packet_len;
    HTC_ENDPOINT_ID ep_id;

    /*
     * The HTT tx descriptor was attached as the prefix fragment to the
     * msdu netbuf during the call to htt_tx_desc_init.
     * Retrieve it so we can provide its HTC header space to HTC.
     */
    htt_host_tx_desc = (struct htt_host_tx_desc_t *)
        adf_nbuf_get_frag_vaddr(msdu, 0);

    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return 1; /* failure */
    }

    pkt->msdu_id = msdu_id;
    pkt->pdev_ctxt = pdev->txrx_pdev;

    /* packet length includes HTT tx desc frag added above */
    packet_len = adf_nbuf_len(msdu);
    if (packet_len < download_len) {
        /*
         * This case of packet length being less than the nominal download
         * length can happen for a couple reasons:
         * In HL, the nominal download length is a large artificial value.
         * In LL, the frame may not have the optional header fields
         * accounted for in the nominal download size (LLC/SNAP header,
         * IPv4 or IPv6 header).
         */
        download_len = packet_len;
    }

    ep_id = htt_tx_htt2_get_ep_id(pdev, msdu);

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        pdev->tx_send_complete_part2,
        (unsigned char *) htt_host_tx_desc,
        download_len - HTC_HDR_LENGTH,
        ep_id,
        1); /* tag - not relevant here */

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msdu);

    adf_nbuf_trace_update(msdu, "HT:T:");
    HTCSendDataPkt(pdev->htc_pdev, &pkt->htc_pkt, more_data);

    return 0; /* success */
}

adf_nbuf_t
htt_tx_send_batch(
    htt_pdev_handle pdev,
    adf_nbuf_t head_msdu, int num_msdus)
{
    adf_nbuf_t rejected = NULL;
    u_int16_t *msdu_id_storage;
    u_int16_t msdu_id;
    adf_nbuf_t msdu;
    /*
     * FOR NOW, iterate through the batch, sending the frames singly.
     * Eventually HTC and HIF should be able to accept a batch of
     * data frames rather than singles.
     */
    msdu = head_msdu;
    while (num_msdus--)
    {
         adf_nbuf_t next_msdu = adf_nbuf_next(msdu);
         msdu_id_storage = ol_tx_msdu_id_storage(msdu);
         msdu_id = *msdu_id_storage;

         /* htt_tx_send_base returns 0 as success and 1 as failure */
         if (htt_tx_send_base(pdev, msdu, msdu_id, pdev->download_len,
                              num_msdus)) {
             adf_nbuf_set_next(msdu, rejected);
             rejected = msdu;
         }
         msdu = next_msdu;
    }
    return rejected;
}

int
htt_tx_send_nonstd(
    htt_pdev_handle pdev,
    adf_nbuf_t msdu,
    u_int16_t msdu_id,
    enum htt_pkt_type pkt_type)
{
    int download_len;

    /*
     * The pkt_type could be checked to see what L2 header type is present,
     * and then the L2 header could be examined to determine its length.
     * But for simplicity, just use the maximum possible header size,
     * rather than computing the actual header size.
     */
    download_len =
        sizeof(struct htt_host_tx_desc_t) +
        HTT_TX_HDR_SIZE_OUTER_HDR_MAX + /* worst case */
        HTT_TX_HDR_SIZE_802_1Q +
        HTT_TX_HDR_SIZE_LLC_SNAP +
        ol_cfg_tx_download_size(pdev->ctrl_pdev);
    return htt_tx_send_base(pdev, msdu, msdu_id, download_len, 0);
}

int
htt_tx_send_std(
    htt_pdev_handle pdev,
    adf_nbuf_t msdu,
    u_int16_t msdu_id)
{
    return htt_tx_send_base(pdev, msdu, msdu_id, pdev->download_len, 0);
}

#endif /*ATH_11AC_TXCOMPACT*/
#ifdef HTT_DBG
void
htt_tx_desc_display(void *tx_desc)
{
    struct htt_tx_msdu_desc_t *htt_tx_desc;

    htt_tx_desc = (struct htt_tx_msdu_desc_t *) tx_desc;

    /* only works for little-endian */
    adf_os_print("HTT tx desc (@ %p):\n", htt_tx_desc);
    adf_os_print("  msg type = %d\n", htt_tx_desc->msg_type);
    adf_os_print("  pkt subtype = %d\n", htt_tx_desc->pkt_subtype);
    adf_os_print("  pkt type = %d\n", htt_tx_desc->pkt_type);
    adf_os_print("  vdev ID = %d\n", htt_tx_desc->vdev_id);
    adf_os_print("  ext TID = %d\n", htt_tx_desc->ext_tid);
    adf_os_print("  postponed = %d\n", htt_tx_desc->postponed);
    adf_os_print("  batch more = %d\n", htt_tx_desc->more_in_batch);
    adf_os_print("  length = %d\n", htt_tx_desc->len);
    adf_os_print("  id = %d\n", htt_tx_desc->id);
    adf_os_print("  frag desc addr = %#x\n", htt_tx_desc->frags_desc_ptr);
    if (htt_tx_desc->frags_desc_ptr) {
        int frag = 0;
        u_int32_t *base;
        u_int32_t addr;
        u_int32_t len;
        do {
            base = ((u_int32_t *) htt_tx_desc->frags_desc_ptr) + (frag * 2);
            addr = *base;
            len = *(base + 1);
            if (addr) {
                adf_os_print(
                    "    frag %d: addr = %#x, len = %d\n", frag, addr, len);
            }
            frag++;
        } while (addr);
    }
}
#endif

#ifdef IPA_UC_OFFLOAD
int htt_tx_ipa_uc_attach(struct htt_pdev_t *pdev,
    unsigned int uc_tx_buf_sz,
    unsigned int uc_tx_buf_cnt,
    unsigned int uc_tx_partition_base)
{
   unsigned int  tx_buffer_count;
   adf_nbuf_t    buffer_vaddr;
   u_int32_t     buffer_paddr;
   u_int32_t    *header_ptr;
   u_int32_t    *ring_vaddr;
   int           return_code = 0;

   /* Allocate CE Write Index WORD */
   pdev->ipa_uc_tx_rsc.tx_ce_idx.vaddr =
       adf_os_mem_alloc_consistent(pdev->osdev,
                4,
                &pdev->ipa_uc_tx_rsc.tx_ce_idx.paddr,
                adf_os_get_dma_mem_context(
                   (&pdev->ipa_uc_tx_rsc.tx_ce_idx), memctx));
   if (!pdev->ipa_uc_tx_rsc.tx_ce_idx.vaddr) {
      adf_os_print("%s: CE Write Index WORD alloc fail", __func__);
      return -1;
   }

   /* Allocate TX COMP Ring */
   pdev->ipa_uc_tx_rsc.tx_comp_base.vaddr =
       adf_os_mem_alloc_consistent(pdev->osdev,
                uc_tx_buf_cnt * 4,
                &pdev->ipa_uc_tx_rsc.tx_comp_base.paddr,
                adf_os_get_dma_mem_context(
                   (&pdev->ipa_uc_tx_rsc.tx_comp_base), memctx));
   if (!pdev->ipa_uc_tx_rsc.tx_comp_base.vaddr) {
      adf_os_print("%s: TX COMP ring alloc fail", __func__);
      return_code = -2;
      goto free_tx_ce_idx;
   }

   adf_os_mem_zero(pdev->ipa_uc_tx_rsc.tx_comp_base.vaddr, uc_tx_buf_cnt * 4);

   /* Allocate TX BUF vAddress Storage */
   pdev->ipa_uc_tx_rsc.tx_buf_pool_vaddr_strg =
         (adf_nbuf_t *)adf_os_mem_alloc(pdev->osdev,
                          uc_tx_buf_cnt * sizeof(adf_nbuf_t));
   if (!pdev->ipa_uc_tx_rsc.tx_buf_pool_vaddr_strg) {
      adf_os_print("%s: TX BUF POOL vaddr storage alloc fail",
                   __func__);
      return_code = -3;
      goto free_tx_comp_base;
   }
   adf_os_mem_zero(pdev->ipa_uc_tx_rsc.tx_buf_pool_vaddr_strg,
                   uc_tx_buf_cnt * sizeof(adf_nbuf_t));

   ring_vaddr = pdev->ipa_uc_tx_rsc.tx_comp_base.vaddr;
   /* Allocate TX buffers as many as possible */
   for (tx_buffer_count = 0;
        tx_buffer_count < (uc_tx_buf_cnt - 1);
        tx_buffer_count++) {
      buffer_vaddr = adf_nbuf_alloc(pdev->osdev,
                uc_tx_buf_sz, 0, 4, FALSE);
      if (!buffer_vaddr)
      {
         adf_os_print("%s: TX BUF alloc fail, allocated buffer count %d",
                      __func__, tx_buffer_count);
         return 0;
      }

      /* Init buffer */
      adf_os_mem_zero(adf_nbuf_data(buffer_vaddr), uc_tx_buf_sz);
      header_ptr = (u_int32_t *)adf_nbuf_data(buffer_vaddr);

      *header_ptr = HTT_IPA_UC_OFFLOAD_TX_HEADER_DEFAULT;
      header_ptr++;
      *header_ptr |= ((u_int16_t)uc_tx_partition_base + tx_buffer_count) << 16;

      adf_nbuf_map(pdev->osdev, buffer_vaddr, ADF_OS_DMA_BIDIRECTIONAL);
      buffer_paddr = adf_nbuf_get_frag_paddr_lo(buffer_vaddr, 0);
      header_ptr++;
      *header_ptr = (u_int32_t)(buffer_paddr + 16);

      header_ptr++;
      *header_ptr = 0xFFFFFFFF;

      /* FRAG Header */
      header_ptr++;
      *header_ptr = buffer_paddr + 32;

      *ring_vaddr = buffer_paddr;
      pdev->ipa_uc_tx_rsc.tx_buf_pool_vaddr_strg[tx_buffer_count] =
            buffer_vaddr;
      /* Memory barrier to ensure actual value updated */

      ring_vaddr++;
   }

   pdev->ipa_uc_tx_rsc.alloc_tx_buf_cnt = tx_buffer_count;

   return 0;

free_tx_comp_base:
   adf_os_mem_free_consistent(pdev->osdev,
                   ol_cfg_ipa_uc_tx_max_buf_cnt(pdev->ctrl_pdev) * 4,
                   pdev->ipa_uc_tx_rsc.tx_comp_base.vaddr,
                   pdev->ipa_uc_tx_rsc.tx_comp_base.paddr,
                   adf_os_get_dma_mem_context(
                      (&pdev->ipa_uc_tx_rsc.tx_comp_base), memctx));
free_tx_ce_idx:
   adf_os_mem_free_consistent(pdev->osdev,
                   4,
                   pdev->ipa_uc_tx_rsc.tx_ce_idx.vaddr,
                   pdev->ipa_uc_tx_rsc.tx_ce_idx.paddr,
                   adf_os_get_dma_mem_context(
                      (&pdev->ipa_uc_tx_rsc.tx_ce_idx), memctx));
   return return_code;
}

int htt_tx_ipa_uc_detach(struct htt_pdev_t *pdev)
{
   u_int16_t idx;

   if (pdev->ipa_uc_tx_rsc.tx_ce_idx.vaddr) {
      adf_os_mem_free_consistent(pdev->osdev,
                   4,
                   pdev->ipa_uc_tx_rsc.tx_ce_idx.vaddr,
                   pdev->ipa_uc_tx_rsc.tx_ce_idx.paddr,
                   adf_os_get_dma_mem_context(
                      (&pdev->ipa_uc_tx_rsc.tx_ce_idx), memctx));
   }

   if (pdev->ipa_uc_tx_rsc.tx_comp_base.vaddr) {
     adf_os_mem_free_consistent(pdev->osdev,
                   ol_cfg_ipa_uc_tx_max_buf_cnt(pdev->ctrl_pdev) * 4,
                   pdev->ipa_uc_tx_rsc.tx_comp_base.vaddr,
                   pdev->ipa_uc_tx_rsc.tx_comp_base.paddr,
                   adf_os_get_dma_mem_context(
                      (&pdev->ipa_uc_tx_rsc.tx_comp_base), memctx));
   }

   /* Free each single buffer */
   for(idx = 0; idx < pdev->ipa_uc_tx_rsc.alloc_tx_buf_cnt; idx++) {
      if (pdev->ipa_uc_tx_rsc.tx_buf_pool_vaddr_strg[idx]) {
         adf_nbuf_unmap(pdev->osdev,
            pdev->ipa_uc_tx_rsc.tx_buf_pool_vaddr_strg[idx],
            ADF_OS_DMA_FROM_DEVICE);
         adf_nbuf_free(pdev->ipa_uc_tx_rsc.tx_buf_pool_vaddr_strg[idx]);
      }
   }

   /* Free storage */
   adf_os_mem_free(pdev->ipa_uc_tx_rsc.tx_buf_pool_vaddr_strg);

   return 0;
}
#endif /* IPA_UC_OFFLOAD */
