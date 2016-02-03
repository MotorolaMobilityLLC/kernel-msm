/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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
 * @file htt_h2t.c
 * @brief Provide functions to send host->target HTT messages.
 * @details
 *  This file contains functions related to host->target HTT messages.
 *  There are a couple aspects of this host->target messaging:
 *  1.  This file contains the function that is called by HTC when
 *      a host->target send completes.
 *      This send-completion callback is primarily relevant to HL,
 *      to invoke the download scheduler to set up a new download,
 *      and optionally free the tx frame whose download is completed.
 *      For both HL and LL, this completion callback frees up the
 *      HTC_PACKET object used to specify the download.
 *  2.  This file contains functions for creating messages to send
 *      from the host to the target.
 */

#include <adf_os_mem.h>  /* adf_os_mem_copy */
#include <adf_nbuf.h>    /* adf_nbuf_map_single */
#include <htc_api.h>     /* HTC_PACKET */
#include <htc.h>         /* HTC_HDR_ALIGNMENT_PADDING */
#include <htt.h>         /* HTT host->target msg defs */
#include <ol_txrx_htt_api.h> /* ol_tx_completion_handler, htt_tx_status */
#include <ol_htt_tx_api.h>


#include <htt_internal.h>

#define HTT_MSG_BUF_SIZE(msg_bytes) \
   ((msg_bytes) + HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING)

#ifndef container_of
#define container_of(ptr, type, member) ((type *)( \
                (char *)(ptr) - (char *)(&((type *)0)->member) ) )
#endif

#ifdef FEATURE_RUNTIME_PM
void
htt_tx_resume_handler(void *context)
{
   struct htt_pdev_t *pdev =  (struct htt_pdev_t *) context;

   htt_tx_sched(pdev);
}
#else
void
htt_tx_resume_handler(void *context) { }
#endif

static void
htt_h2t_send_complete_free_netbuf(
    void *pdev, A_STATUS status, adf_nbuf_t netbuf, u_int16_t msdu_id)
{
    adf_nbuf_free(netbuf);
}

void
htt_h2t_send_complete(void *context, HTC_PACKET *htc_pkt)
{
    void (*send_complete_part2)(
        void *pdev, A_STATUS status, adf_nbuf_t msdu, u_int16_t msdu_id);
    struct htt_pdev_t *pdev =  (struct htt_pdev_t *) context;
    struct htt_htc_pkt *htt_pkt;
    adf_nbuf_t netbuf;

    send_complete_part2 = htc_pkt->pPktContext;

    htt_pkt = container_of(htc_pkt, struct htt_htc_pkt, htc_pkt);

    /* process (free or keep) the netbuf that held the message */
    netbuf = (adf_nbuf_t) htc_pkt->pNetBufContext;
    if (send_complete_part2 != NULL) {
        send_complete_part2(
            htt_pkt->pdev_ctxt, htc_pkt->Status, netbuf, htt_pkt->msdu_id);
    }

    if (pdev->cfg.is_high_latency && !pdev->cfg.default_tx_comp_req) {
        int32_t credit_delta;
        HTT_TX_MUTEX_ACQUIRE(&pdev->credit_mutex);
        adf_os_atomic_add(1, &pdev->htt_tx_credit.bus_delta);
        credit_delta = htt_tx_credit_update(pdev);
        HTT_TX_MUTEX_RELEASE(&pdev->credit_mutex);
        if (credit_delta) {
            ol_tx_credit_completion_handler(pdev->txrx_pdev, credit_delta);
        }
    }

    /* free the htt_htc_pkt / HTC_PACKET object */
    htt_htc_pkt_free(pdev, htt_pkt);
}

HTC_SEND_FULL_ACTION
htt_h2t_full(void *context, HTC_PACKET *pkt)
{
/* FIX THIS */
    return HTC_SEND_FULL_KEEP;
}

A_STATUS
htt_h2t_ver_req_msg(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt *pkt;
    adf_nbuf_t msg;
    u_int32_t *msg_word;
    u_int32_t msg_size;
    u_int32_t max_tx_group;

    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return A_ERROR; /* failure */
    }

    max_tx_group = OL_TX_GET_MAX_GROUPS(pdev->txrx_pdev);

    if (max_tx_group) {
        msg_size = HTT_VER_REQ_BYTES +
               sizeof(struct htt_option_tlv_mac_tx_queue_groups_t);
    } else {
        msg_size = HTT_VER_REQ_BYTES;
    }

    /* show that this is not a tx frame download (not required, but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = adf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(msg_size),
        /* reserve room for the HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return A_ERROR; /* failure */
    }

    /*
     * Set the length of the message.
     * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
     * separately during the below call to adf_nbuf_push_head.
     * The contribution from the HTC header is added separately inside HTC.
     */
    adf_nbuf_put_tail(msg, msg_size);

    /* fill in the message contents */
    msg_word = (u_int32_t *) adf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    adf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_VERSION_REQ);

    if (max_tx_group) {
        *(msg_word + 1) = 0;
        /* Fill Group Info */
        HTT_OPTION_TLV_TAG_SET(*(msg_word+1),
                           HTT_OPTION_TLV_TAG_MAX_TX_QUEUE_GROUPS);
        HTT_OPTION_TLV_LENGTH_SET(*(msg_word+1),
                          (sizeof(struct htt_option_tlv_mac_tx_queue_groups_t)/
                           sizeof(u_int32_t)));
        HTT_OPTION_TLV_VALUE0_SET(*(msg_word+1), max_tx_group);
    }

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        htt_h2t_send_complete_free_netbuf,
        adf_nbuf_data(msg),
        adf_nbuf_len(msg),
        pdev->htc_endpoint,
        1); /* tag - not relevant here */

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
    if (HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt) == A_OK) {
        htt_htc_misc_pkt_list_add(pdev, pkt);
    }
#else
    HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif
    if ((pdev->cfg.is_high_latency) &&
        (!pdev->cfg.default_tx_comp_req)) {
        ol_tx_target_credit_update(pdev->txrx_pdev, -1);
    }
    return A_OK;
}

A_STATUS
htt_h2t_rx_ring_cfg_msg_ll(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt *pkt;
    adf_nbuf_t msg;
    u_int32_t *msg_word;
    int enable_ctrl_data, enable_mgmt_data,
        enable_null_data, enable_phy_data, enable_hdr,
        enable_ppdu_start, enable_ppdu_end;

    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return A_ERROR; /* failure */
    }

    /* show that this is not a tx frame download (not required, but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = adf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_RX_RING_CFG_BYTES(1)),
        /* reserve room for the HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return A_ERROR; /* failure */
    }
    /*
     * Set the length of the message.
     * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
     * separately during the below call to adf_nbuf_push_head.
     * The contribution from the HTC header is added separately inside HTC.
     */
    adf_nbuf_put_tail(msg, HTT_RX_RING_CFG_BYTES(1));

    /* fill in the message contents */
    msg_word = (u_int32_t *) adf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    adf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_RX_RING_CFG);
    HTT_RX_RING_CFG_NUM_RINGS_SET(*msg_word, 1);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_SET(
        *msg_word, pdev->rx_ring.alloc_idx.paddr);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_BASE_PADDR_SET(*msg_word, pdev->rx_ring.base_paddr);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_LEN_SET(*msg_word, pdev->rx_ring.size);
    HTT_RX_RING_CFG_BUF_SZ_SET(*msg_word, HTT_RX_BUF_SIZE);

/* FIX THIS: if the FW creates a complete translated rx descriptor, then the MAC DMA of the HW rx descriptor should be disabled. */
    msg_word++;
    *msg_word = 0;
#ifndef REMOVE_PKT_LOG
 if (ol_cfg_is_packet_log_enabled(pdev->ctrl_pdev))
   {
       enable_ctrl_data = 1;
       enable_mgmt_data = 1;
       enable_null_data = 1;
       enable_phy_data  = 1;
       enable_hdr       = 1;
       enable_ppdu_start= 1;
       enable_ppdu_end  = 1;
       /* Disable ASPM when pkt log is enabled */
       adf_os_print("Pkt log is enabled\n");
       htt_htc_disable_aspm();
   }
   else
   {
       adf_os_print("Pkt log is disabled\n");
       enable_ctrl_data = 0;
       enable_mgmt_data = 0;
       enable_null_data = 0;
       enable_phy_data  = 0;
       enable_hdr       = 0;
       enable_ppdu_start= 0;
       enable_ppdu_end  = 0;
   }
#else
    enable_ctrl_data = 0;
    enable_mgmt_data = 0;
    enable_null_data = 0;
    enable_phy_data  = 0;
    enable_hdr       = 0;
    enable_ppdu_start= 0;
    enable_ppdu_end  = 0;
#endif
    HTT_RX_RING_CFG_ENABLED_802_11_HDR_SET(*msg_word, enable_hdr);
    HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_PPDU_START_SET(*msg_word, enable_ppdu_start);
    HTT_RX_RING_CFG_ENABLED_PPDU_END_SET(*msg_word, enable_ppdu_end);
    HTT_RX_RING_CFG_ENABLED_MPDU_START_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_MPDU_END_SET(*msg_word,   1);
    HTT_RX_RING_CFG_ENABLED_MSDU_START_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_MSDU_END_SET(*msg_word,   1);
    HTT_RX_RING_CFG_ENABLED_RX_ATTN_SET(*msg_word,    1);
    HTT_RX_RING_CFG_ENABLED_FRAG_INFO_SET(*msg_word,  1); /* always present? */
    HTT_RX_RING_CFG_ENABLED_UCAST_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_MCAST_SET(*msg_word, 1);
    /* Must change to dynamic enable at run time
     * rather than at compile time
     */
    HTT_RX_RING_CFG_ENABLED_CTRL_SET(*msg_word, enable_ctrl_data);
    HTT_RX_RING_CFG_ENABLED_MGMT_SET(*msg_word, enable_mgmt_data);
    HTT_RX_RING_CFG_ENABLED_NULL_SET(*msg_word, enable_null_data);
    HTT_RX_RING_CFG_ENABLED_PHY_SET(*msg_word, enable_phy_data);
    HTT_RX_RING_CFG_IDX_INIT_VAL_SET(*msg_word,
            *pdev->rx_ring.alloc_idx.vaddr);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_802_11_HDR_SET(*msg_word,
            RX_STD_DESC_HDR_STATUS_OFFSET_DWORD);
    HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_SET(*msg_word,
            HTT_RX_STD_DESC_RESERVATION_DWORD);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_PPDU_START_SET(*msg_word,
            RX_STD_DESC_PPDU_START_OFFSET_DWORD);
    HTT_RX_RING_CFG_OFFSET_PPDU_END_SET(*msg_word,
            RX_STD_DESC_PPDU_END_OFFSET_DWORD);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_MPDU_START_SET(*msg_word,
            RX_STD_DESC_MPDU_START_OFFSET_DWORD);
    HTT_RX_RING_CFG_OFFSET_MPDU_END_SET(*msg_word,
            RX_STD_DESC_MPDU_END_OFFSET_DWORD);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_MSDU_START_SET(*msg_word,
            RX_STD_DESC_MSDU_START_OFFSET_DWORD);
    HTT_RX_RING_CFG_OFFSET_MSDU_END_SET(*msg_word,
            RX_STD_DESC_MSDU_END_OFFSET_DWORD);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_RX_ATTN_SET(*msg_word,
            RX_STD_DESC_ATTN_OFFSET_DWORD);
    HTT_RX_RING_CFG_OFFSET_FRAG_INFO_SET(*msg_word,
            RX_STD_DESC_FRAG_INFO_OFFSET_DWORD);

    SET_HTC_PACKET_INFO_TX(
            &pkt->htc_pkt,
            htt_h2t_send_complete_free_netbuf,
            adf_nbuf_data(msg),
            adf_nbuf_len(msg),
            pdev->htc_endpoint,
            HTC_TX_PACKET_TAG_RUNTIME_PUT);

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
    if (HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt) == A_OK)
        htt_htc_misc_pkt_list_add(pdev, pkt);
#else
    HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif
    return A_OK;
}

A_STATUS
htt_h2t_rx_ring_cfg_msg_hl(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt *pkt;
    adf_nbuf_t msg;
    u_int32_t *msg_word;

    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return A_ERROR; /* failure */
    }

    /* show that this is not a tx frame download (not required, but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = adf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_RX_RING_CFG_BYTES(1)),
        /* reserve room for the HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return A_ERROR; /* failure */
    }
    /*
     * Set the length of the message.
     * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
     * separately during the below call to adf_nbuf_push_head.
     * The contribution from the HTC header is added separately inside HTC.
     */
    adf_nbuf_put_tail(msg, HTT_RX_RING_CFG_BYTES(1));

    /* fill in the message contents */
    msg_word = (u_int32_t *) adf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    adf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_RX_RING_CFG);
    HTT_RX_RING_CFG_NUM_RINGS_SET(*msg_word, 1);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_SET(
        *msg_word, pdev->rx_ring.alloc_idx.paddr);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_BASE_PADDR_SET(*msg_word, pdev->rx_ring.base_paddr);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_LEN_SET(*msg_word, pdev->rx_ring.size);
    HTT_RX_RING_CFG_BUF_SZ_SET(*msg_word, HTT_RX_BUF_SIZE);

/* FIX THIS: if the FW creates a complete translated rx descriptor, then the MAC DMA of the HW rx descriptor should be disabled. */
    msg_word++;
    *msg_word = 0;

    HTT_RX_RING_CFG_ENABLED_802_11_HDR_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_PPDU_START_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_PPDU_END_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_MPDU_START_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_MPDU_END_SET(*msg_word,   0);
    HTT_RX_RING_CFG_ENABLED_MSDU_START_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_MSDU_END_SET(*msg_word,   0);
    HTT_RX_RING_CFG_ENABLED_RX_ATTN_SET(*msg_word,    0);
    HTT_RX_RING_CFG_ENABLED_FRAG_INFO_SET(*msg_word,  0); /* always present? */
    HTT_RX_RING_CFG_ENABLED_UCAST_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_MCAST_SET(*msg_word, 1);
    /* Must change to dynamic enable at run time
     * rather than at compile time
     */
    HTT_RX_RING_CFG_ENABLED_CTRL_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_MGMT_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_NULL_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_PHY_SET(*msg_word, 0);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_802_11_HDR_SET(*msg_word,
            0);
    HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_SET(*msg_word,
            0);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_PPDU_START_SET(*msg_word,
            0);
    HTT_RX_RING_CFG_OFFSET_PPDU_END_SET(*msg_word,
            0);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_MPDU_START_SET(*msg_word,
            0);
    HTT_RX_RING_CFG_OFFSET_MPDU_END_SET(*msg_word,
            0);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_MSDU_START_SET(*msg_word,
            0);
    HTT_RX_RING_CFG_OFFSET_MSDU_END_SET(*msg_word,
            0);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_RX_ATTN_SET(*msg_word,
            0);
    HTT_RX_RING_CFG_OFFSET_FRAG_INFO_SET(*msg_word,
            0);

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        htt_h2t_send_complete_free_netbuf,
        adf_nbuf_data(msg),
        adf_nbuf_len(msg),
        pdev->htc_endpoint,
        1); /* tag - not relevant here */

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
    if (HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt) == A_OK) {
        htt_htc_misc_pkt_list_add(pdev, pkt);
    }
#else
    HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif
    if (!pdev->cfg.default_tx_comp_req) {
        ol_tx_target_credit_update(pdev->txrx_pdev, -1);
    }
    return A_OK;
}

int
htt_h2t_dbg_stats_get(
    struct htt_pdev_t *pdev,
    u_int32_t stats_type_upload_mask,
    u_int32_t stats_type_reset_mask,
    u_int8_t cfg_stat_type,
    u_int32_t cfg_val,
    u_int64_t cookie)
{
    struct htt_htc_pkt *pkt;
    adf_nbuf_t msg;
    u_int32_t *msg_word;
    uint16_t htc_tag = 1;

    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return -1; /* failure */
    }

    if (stats_type_upload_mask >= 1 << HTT_DBG_NUM_STATS ||
        stats_type_reset_mask >= 1 << HTT_DBG_NUM_STATS)
    {
        /* FIX THIS - add more details? */
        adf_os_print("%#x %#x stats not supported\n",
            stats_type_upload_mask, stats_type_reset_mask);
        return -1; /* failure */
    }

    if (stats_type_reset_mask)
        htc_tag = HTC_TX_PACKET_TAG_RUNTIME_PUT;

    /* show that this is not a tx frame download (not required, but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = adf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_H2T_STATS_REQ_MSG_SZ),
        /* reserve room for HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, FALSE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return -1; /* failure */
    }
    /* set the length of the message */
    adf_nbuf_put_tail(msg, HTT_H2T_STATS_REQ_MSG_SZ);

    /* fill in the message contents */
    msg_word = (u_int32_t *) adf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    adf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_STATS_REQ);
    HTT_H2T_STATS_REQ_UPLOAD_TYPES_SET(*msg_word, stats_type_upload_mask);

    msg_word++;
    *msg_word = 0;
    HTT_H2T_STATS_REQ_RESET_TYPES_SET(*msg_word, stats_type_reset_mask);

    msg_word++;
    *msg_word = 0;
    HTT_H2T_STATS_REQ_CFG_VAL_SET(*msg_word, cfg_val);
    HTT_H2T_STATS_REQ_CFG_STAT_TYPE_SET(*msg_word, cfg_stat_type);

    /* cookie LSBs */
    msg_word++;
    *msg_word = cookie & 0xffffffff;

    /* cookie MSBs */
    msg_word++;
    *msg_word = cookie >> 32;

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        htt_h2t_send_complete_free_netbuf,
        adf_nbuf_data(msg),
        adf_nbuf_len(msg),
        pdev->htc_endpoint,
        htc_tag);

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
    if (HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt) == A_OK) {
        htt_htc_misc_pkt_list_add(pdev, pkt);
    }
#else
    HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif
    if ((pdev->cfg.is_high_latency) &&
        (!pdev->cfg.default_tx_comp_req)) {
        ol_tx_target_credit_update(pdev->txrx_pdev, -1);
    }
    return 0;
}

A_STATUS
htt_h2t_sync_msg(struct htt_pdev_t *pdev, u_int8_t sync_cnt)
{
    struct htt_htc_pkt *pkt;
    adf_nbuf_t msg;
    u_int32_t *msg_word;

    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return A_NO_MEMORY;
    }

    /* show that this is not a tx frame download (not required, but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = adf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_H2T_SYNC_MSG_SZ),
        /* reserve room for HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, FALSE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return A_NO_MEMORY;
    }
    /* set the length of the message */
    adf_nbuf_put_tail(msg, HTT_H2T_SYNC_MSG_SZ);

    /* fill in the message contents */
    msg_word = (u_int32_t *) adf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    adf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_SYNC);
    HTT_H2T_SYNC_COUNT_SET(*msg_word, sync_cnt);

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        htt_h2t_send_complete_free_netbuf,
        adf_nbuf_data(msg),
        adf_nbuf_len(msg),
        pdev->htc_endpoint,
        HTC_TX_PACKET_TAG_RUNTIME_PUT);

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
    if (HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt) == A_OK)
        htt_htc_misc_pkt_list_add(pdev, pkt);
#else
    HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif
    if ((pdev->cfg.is_high_latency) &&
        (!pdev->cfg.default_tx_comp_req)) {
        ol_tx_target_credit_update(pdev->txrx_pdev, -1);
    }
    return A_OK;
}

int
htt_h2t_aggr_cfg_msg(struct htt_pdev_t *pdev,
                     int max_subfrms_ampdu,
                     int max_subfrms_amsdu)
{
    struct htt_htc_pkt *pkt;
    adf_nbuf_t msg;
    u_int32_t *msg_word;

    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return -1; /* failure */
    }

    /* show that this is not a tx frame download (not required, but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = adf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_AGGR_CFG_MSG_SZ),
        /* reserve room for HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, FALSE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return -1; /* failure */
    }
    /* set the length of the message */
    adf_nbuf_put_tail(msg, HTT_AGGR_CFG_MSG_SZ);

    /* fill in the message contents */
    msg_word = (u_int32_t *) adf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    adf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_AGGR_CFG);

    if (max_subfrms_ampdu && (max_subfrms_ampdu <= 64)) {
        HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM_SET(*msg_word, max_subfrms_ampdu);
    }

    if (max_subfrms_amsdu && (max_subfrms_amsdu < 32)) {
        HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM_SET(*msg_word, max_subfrms_amsdu);
    }

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        htt_h2t_send_complete_free_netbuf,
        adf_nbuf_data(msg),
        adf_nbuf_len(msg),
        pdev->htc_endpoint,
        HTC_TX_PACKET_TAG_RUNTIME_PUT);

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
    if (HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt) == A_OK)
        htt_htc_misc_pkt_list_add(pdev, pkt);
#else
    HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif
    if ((pdev->cfg.is_high_latency) &&
        (!pdev->cfg.default_tx_comp_req)) {
        ol_tx_target_credit_update(pdev->txrx_pdev, -1);
    }
    return 0;
}

#ifdef IPA_UC_OFFLOAD
int htt_h2t_ipa_uc_rsc_cfg_msg(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt *pkt;
    adf_nbuf_t msg;
    u_int32_t *msg_word;

    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return A_NO_MEMORY;
    }

    /* show that this is not a tx frame download (not required, but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = adf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_WDI_IPA_CFG_SZ),
        /* reserve room for HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, FALSE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return A_NO_MEMORY;
    }
    /* set the length of the message */
    adf_nbuf_put_tail(msg, HTT_WDI_IPA_CFG_SZ);

    /* fill in the message contents */
    msg_word = (u_int32_t *) adf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    adf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_SET(*msg_word,
         pdev->ipa_uc_tx_rsc.alloc_tx_buf_cnt);
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_WDI_IPA_CFG);

    msg_word++;
    *msg_word = 0;
    HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_SET(*msg_word,
        (unsigned int)pdev->ipa_uc_tx_rsc.tx_comp_base.paddr);

    msg_word++;
    *msg_word = 0;
    HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_SET(*msg_word,
        (unsigned int)ol_cfg_ipa_uc_tx_max_buf_cnt(pdev->ctrl_pdev));

    msg_word++;
    *msg_word = 0;
    HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_SET(*msg_word,
        (unsigned int)pdev->ipa_uc_tx_rsc.tx_comp_idx_paddr);

    msg_word++;
    *msg_word = 0;
    HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_SET(*msg_word,
        (unsigned int)pdev->ipa_uc_tx_rsc.tx_ce_idx.paddr);

    msg_word++;
    *msg_word = 0;
    HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_SET(*msg_word,
        (unsigned int)pdev->ipa_uc_rx_rsc.rx_ind_ring_base.paddr);

    msg_word++;
    *msg_word = 0;
    HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_SET(*msg_word,
        (unsigned int)ol_cfg_ipa_uc_rx_ind_ring_size(pdev->ctrl_pdev));

    msg_word++;
    *msg_word = 0;
    HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_SET(*msg_word,
        (unsigned int)pdev->ipa_uc_rx_rsc.rx_ipa_prc_done_idx.paddr);

    msg_word++;
    *msg_word = 0;
    HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_SET(*msg_word,
        (unsigned int)pdev->ipa_uc_rx_rsc.rx_rdy_idx_paddr);

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        htt_h2t_send_complete_free_netbuf,
        adf_nbuf_data(msg),
        adf_nbuf_len(msg),
        pdev->htc_endpoint,
        HTC_TX_PACKET_TAG_RUNTIME_PUT);

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
    if (HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt) == A_OK)
        htt_htc_misc_pkt_list_add(pdev, pkt);
#else
    HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif

    return A_OK;
}


int htt_h2t_ipa_uc_set_active(struct htt_pdev_t *pdev,
   a_bool_t uc_active,
   a_bool_t is_tx)
{
    struct htt_htc_pkt *pkt;
    adf_nbuf_t msg;
    u_int32_t *msg_word;
    u_int8_t active_target = 0;

    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return A_NO_MEMORY;
    }

    /* show that this is not a tx frame download (not required, but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = adf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_WDI_IPA_OP_REQUEST_SZ),
        /* reserve room for HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, FALSE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return A_NO_MEMORY;
    }
    /* set the length of the message */
    adf_nbuf_put_tail(msg, HTT_WDI_IPA_OP_REQUEST_SZ);

    /* fill in the message contents */
    msg_word = (u_int32_t *) adf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    adf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    if (uc_active && is_tx)
    {
        active_target = HTT_WDI_IPA_OPCODE_TX_RESUME;
    }
    else if (!uc_active && is_tx)
    {
        active_target = HTT_WDI_IPA_OPCODE_TX_SUSPEND;
    }
    else if (uc_active && !is_tx)
    {
        active_target = HTT_WDI_IPA_OPCODE_RX_RESUME;
    }
    else if (!uc_active && !is_tx)
    {
        active_target = HTT_WDI_IPA_OPCODE_RX_SUSPEND;
    }
    HTT_WDI_IPA_OP_REQUEST_OP_CODE_SET(*msg_word,
         active_target);
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQ);

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        htt_h2t_send_complete_free_netbuf,
        adf_nbuf_data(msg),
        adf_nbuf_len(msg),
        pdev->htc_endpoint,
        1); /* tag - not relevant here */

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);


#ifdef ATH_11AC_TXCOMPACT
    if (HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt) == A_OK)
        htt_htc_misc_pkt_list_add(pdev, pkt);
#else
    HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif

    return A_OK;
}


int htt_h2t_ipa_uc_get_stats(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt *pkt = NULL;
    adf_nbuf_t msg = NULL;
    u_int32_t *msg_word;

    /* New buffer alloc send */
    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return A_NO_MEMORY;
    }

    /* show that this is not a tx frame download (not required,
     * but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = adf_nbuf_alloc(
                         pdev->osdev,
                         HTT_MSG_BUF_SIZE(HTT_WDI_IPA_OP_REQUEST_SZ),
                         /* reserve room for HTC header */
                         HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, FALSE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return A_NO_MEMORY;
    }
    /* set the length of the message */
    adf_nbuf_put_tail(msg, HTT_WDI_IPA_OP_REQUEST_SZ);
    /* rewind beyond alignment pad to get to the HTC header reserved area */
    adf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    /* fill in the message contents */
    msg_word = (u_int32_t *) adf_nbuf_data(msg);
    *msg_word = 0;
    HTT_WDI_IPA_OP_REQUEST_OP_CODE_SET(*msg_word,
                                       HTT_WDI_IPA_OPCODE_DBG_STATS);
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQ);

    SET_HTC_PACKET_INFO_TX(
                           &pkt->htc_pkt,
                           htt_h2t_send_complete_free_netbuf,
                           adf_nbuf_data(msg),
                           adf_nbuf_len(msg),
                           pdev->htc_endpoint,
                           1); /* tag - not relevant here */

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
    if (HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt) == A_OK)
        htt_htc_misc_pkt_list_add(pdev, pkt);
#else
    HTCSendPkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif
    return A_OK;
}
#endif /* IPA_UC_OFFLOAD */

