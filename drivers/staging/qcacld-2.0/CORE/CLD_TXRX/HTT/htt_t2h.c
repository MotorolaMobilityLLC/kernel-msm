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
 * @file htt_t2h.c
 * @brief Provide functions to process target->host HTT messages.
 * @details
 *  This file contains functions related to target->host HTT messages.
 *  There are two categories of functions:
 *  1.  A function that receives a HTT message from HTC, and dispatches it
 *      based on the HTT message type.
 *  2.  functions that provide the info elements from specific HTT messages.
 */

#include <htc_api.h>         /* HTC_PACKET */
#include <htt.h>             /* HTT_T2H_MSG_TYPE, etc. */
#include <adf_nbuf.h>        /* adf_nbuf_t */

#include <ol_htt_rx_api.h>
#include <ol_htt_tx_api.h>
#include <ol_txrx_htt_api.h> /* htt_tx_status */

#include <htt_internal.h>    /* HTT_TX_SCHED, etc. */
#include <pktlog_ac_fmt.h>
#include <wdi_event.h>
#include <ol_htt_tx_api.h>
#include <ol_txrx_types.h>
/*--- target->host HTT message dispatch function ----------------------------*/

#ifndef DEBUG_CREDIT
#define DEBUG_CREDIT 0
#endif

static u_int8_t *
htt_t2h_mac_addr_deswizzle(u_int8_t *tgt_mac_addr, u_int8_t *buffer)
{
#ifdef BIG_ENDIAN_HOST
    /*
     * The host endianness is opposite of the target endianness.
     * To make u_int32_t elements come out correctly, the target->host
     * upload has swizzled the bytes in each u_int32_t element of the
     * message.
     * For byte-array message fields like the MAC address, this
     * upload swizzling puts the bytes in the wrong order, and needs
     * to be undone.
     */
    buffer[0] = tgt_mac_addr[3];
    buffer[1] = tgt_mac_addr[2];
    buffer[2] = tgt_mac_addr[1];
    buffer[3] = tgt_mac_addr[0];
    buffer[4] = tgt_mac_addr[7];
    buffer[5] = tgt_mac_addr[6];
    return buffer;
#else
    /*
     * The host endianness matches the target endianness -
     * we can use the mac addr directly from the message buffer.
     */
    return tgt_mac_addr;
#endif
}

#if defined(CONFIG_HL_SUPPORT)
#define HTT_RX_FRAG_SET_LAST_MSDU(pdev, msg) /* no-op */
#else
static void HTT_RX_FRAG_SET_LAST_MSDU(
    struct htt_pdev_t *pdev, adf_nbuf_t msg)
{
    u_int32_t *msg_word;
    unsigned num_msdu_bytes;
    adf_nbuf_t msdu;
    struct htt_host_rx_desc_base *rx_desc;
    int start_idx;
    u_int8_t *p_fw_msdu_rx_desc = 0;

    msg_word = (u_int32_t *) adf_nbuf_data(msg);
    num_msdu_bytes = HTT_RX_FRAG_IND_FW_RX_DESC_BYTES_GET(*(msg_word +
               HTT_RX_FRAG_IND_HDR_PREFIX_SIZE32));
    /*
     * 1 word for the message header,
     * 1 word to specify the number of MSDU bytes,
     * 1 word for every 4 MSDU bytes (round up),
     * 1 word for the MPDU range header
     */
    pdev->rx_mpdu_range_offset_words = 3 + ((num_msdu_bytes + 3) >> 2);
    pdev->rx_ind_msdu_byte_idx = 0;

    p_fw_msdu_rx_desc = ((u_int8_t *)(msg_word) +
               HTT_ENDIAN_BYTE_IDX_SWAP(HTT_RX_FRAG_IND_FW_DESC_BYTE_OFFSET));

    /*
     * Fix for EV126710, in which BSOD occurs due to last_msdu bit
     * not set while the next pointer is deliberately set to NULL
     * before calling ol_rx_pn_check_base()
     *
     * For fragment frames, the HW may not have set the last_msdu bit
     * in the rx descriptor, but the SW expects this flag to be set,
     * since each fragment is in a separate MPDU. Thus, set the flag here,
     * just in case the HW didn't.
     */
    start_idx = pdev->rx_ring.sw_rd_idx.msdu_payld;
    msdu = pdev->rx_ring.buf.netbufs_ring[start_idx];
    adf_nbuf_set_pktlen(msdu, HTT_RX_BUF_SIZE);
    adf_nbuf_unmap(pdev->osdev, msdu, ADF_OS_DMA_FROM_DEVICE);
    rx_desc = htt_rx_desc(msdu);
    *((u_int8_t *) &rx_desc->fw_desc.u.val) = *p_fw_msdu_rx_desc;
    rx_desc->msdu_end.last_msdu = 1;
    adf_nbuf_map(pdev->osdev, msdu, ADF_OS_DMA_FROM_DEVICE);
}
#endif /* CONFIG_HL_SUPPORT */

/* Target to host Msg/event  handler  for low priority messages*/
void
htt_t2h_lp_msg_handler(void *context, adf_nbuf_t htt_t2h_msg )
{
    struct htt_pdev_t *pdev = (struct htt_pdev_t *) context;
    u_int32_t *msg_word;
    enum htt_t2h_msg_type msg_type;

    msg_word = (u_int32_t *) adf_nbuf_data(htt_t2h_msg);
    msg_type = HTT_T2H_MSG_TYPE_GET(*msg_word);
    switch (msg_type) {
    case HTT_T2H_MSG_TYPE_VERSION_CONF:
        {
            htc_pm_runtime_put(pdev->htc_pdev);
            pdev->tgt_ver.major = HTT_VER_CONF_MAJOR_GET(*msg_word);
            pdev->tgt_ver.minor = HTT_VER_CONF_MINOR_GET(*msg_word);
            adf_os_print("target uses HTT version %d.%d; host uses %d.%d\n",
                pdev->tgt_ver.major, pdev->tgt_ver.minor,
                HTT_CURRENT_VERSION_MAJOR, HTT_CURRENT_VERSION_MINOR);
            if (pdev->tgt_ver.major != HTT_CURRENT_VERSION_MAJOR) {
                adf_os_print("*** Incompatible host/target HTT versions!\n");
            }
            /* abort if the target is incompatible with the host */
            adf_os_assert(pdev->tgt_ver.major == HTT_CURRENT_VERSION_MAJOR);
            if (pdev->tgt_ver.minor != HTT_CURRENT_VERSION_MINOR) {
                adf_os_print(
                    "*** Warning: host/target HTT versions are different, "
                    "though compatible!\n");
            }
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_FLUSH:
        {
            u_int16_t peer_id;
            u_int8_t tid;
            int seq_num_start, seq_num_end;
            enum htt_rx_flush_action action;

            peer_id = HTT_RX_FLUSH_PEER_ID_GET(*msg_word);
            tid = HTT_RX_FLUSH_TID_GET(*msg_word);
            seq_num_start = HTT_RX_FLUSH_SEQ_NUM_START_GET(*(msg_word+1));
            seq_num_end = HTT_RX_FLUSH_SEQ_NUM_END_GET(*(msg_word+1));
            action =
                HTT_RX_FLUSH_MPDU_STATUS_GET(*(msg_word+1)) == 1 ?
                htt_rx_flush_release : htt_rx_flush_discard;
            ol_rx_flush_handler(
                pdev->txrx_pdev,
                peer_id, tid,
                seq_num_start,
                seq_num_end,
                action);
            break;
        }
    case  HTT_T2H_MSG_TYPE_RX_OFFLOAD_DELIVER_IND:
        {
            int msdu_cnt;
            msdu_cnt = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_CNT_GET(*msg_word);
            ol_rx_offload_deliver_ind_handler(
                pdev->txrx_pdev,
                htt_t2h_msg,
                msdu_cnt);
            break;
        }
    case  HTT_T2H_MSG_TYPE_RX_FRAG_IND:
        {
            u_int16_t peer_id;
            u_int8_t tid;

            peer_id = HTT_RX_FRAG_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_FRAG_IND_EXT_TID_GET(*msg_word);
            HTT_RX_FRAG_SET_LAST_MSDU(pdev, htt_t2h_msg);

            ol_rx_frag_indication_handler(
                pdev->txrx_pdev,
                htt_t2h_msg,
                peer_id,
                tid);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_ADDBA:
        {
            u_int16_t peer_id;
            u_int8_t tid;
            u_int8_t win_sz;
            u_int16_t start_seq_num;

            /*
             * FOR NOW, the host doesn't need to know the initial
             * sequence number for rx aggregation.
             * Thus, any value will do - specify 0.
             */
            start_seq_num = 0;
            peer_id = HTT_RX_ADDBA_PEER_ID_GET(*msg_word);
            tid = HTT_RX_ADDBA_TID_GET(*msg_word);
            win_sz = HTT_RX_ADDBA_WIN_SIZE_GET(*msg_word);
            ol_rx_addba_handler(
                pdev->txrx_pdev, peer_id, tid, win_sz, start_seq_num,
                0 /* success */);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_DELBA:
        {
            u_int16_t peer_id;
            u_int8_t tid;

            peer_id = HTT_RX_DELBA_PEER_ID_GET(*msg_word);
            tid = HTT_RX_DELBA_TID_GET(*msg_word);
            ol_rx_delba_handler(pdev->txrx_pdev, peer_id, tid);
            break;
        }
    case HTT_T2H_MSG_TYPE_PEER_MAP:
        {
            u_int8_t mac_addr_deswizzle_buf[HTT_MAC_ADDR_LEN];
            u_int8_t *peer_mac_addr;
            u_int16_t peer_id;
            u_int8_t vdev_id;

            peer_id = HTT_RX_PEER_MAP_PEER_ID_GET(*msg_word);
            vdev_id = HTT_RX_PEER_MAP_VDEV_ID_GET(*msg_word);
            peer_mac_addr = htt_t2h_mac_addr_deswizzle(
                (u_int8_t *) (msg_word+1), &mac_addr_deswizzle_buf[0]);

            ol_rx_peer_map_handler(
                pdev->txrx_pdev, peer_id, vdev_id, peer_mac_addr, 1/*can tx*/);
            break;
        }
    case HTT_T2H_MSG_TYPE_PEER_UNMAP:
        {
            u_int16_t peer_id;
            peer_id = HTT_RX_PEER_UNMAP_PEER_ID_GET(*msg_word);

            ol_rx_peer_unmap_handler(pdev->txrx_pdev, peer_id);
            break;
        }
    case HTT_T2H_MSG_TYPE_SEC_IND:
        {
            u_int16_t peer_id;
            enum htt_sec_type sec_type;
            int is_unicast;

            peer_id = HTT_SEC_IND_PEER_ID_GET(*msg_word);
            sec_type = HTT_SEC_IND_SEC_TYPE_GET(*msg_word);
            is_unicast = HTT_SEC_IND_UNICAST_GET(*msg_word);
            msg_word++; /* point to the first part of the Michael key */
            ol_rx_sec_ind_handler(
                pdev->txrx_pdev, peer_id, sec_type, is_unicast, msg_word, msg_word+2);
            break;
        }
    case HTT_T2H_MSG_TYPE_MGMT_TX_COMPL_IND:
        {
            struct htt_mgmt_tx_compl_ind *compl_msg;

            compl_msg = (struct htt_mgmt_tx_compl_ind *)(msg_word + 1);
            if (pdev->cfg.is_high_latency) {
                ol_tx_target_credit_update(pdev->txrx_pdev, 1);
            }
            ol_tx_single_completion_handler(
                pdev->txrx_pdev, compl_msg->status, compl_msg->desc_id);
            htc_pm_runtime_put(pdev->htc_pdev);
            HTT_TX_SCHED(pdev);
            break;
        }
#if TXRX_STATS_LEVEL != TXRX_STATS_LEVEL_OFF
    case HTT_T2H_MSG_TYPE_STATS_CONF:
        {
            u_int64_t cookie;
            u_int8_t *stats_info_list;

            cookie = *(msg_word + 1);
            cookie |= ((u_int64_t) (*(msg_word + 2))) << 32;

            stats_info_list = (u_int8_t *) (msg_word + 3);
            htc_pm_runtime_put(pdev->htc_pdev);
            ol_txrx_fw_stats_handler(pdev->txrx_pdev, cookie, stats_info_list);
            break;
        }
#endif
#ifndef REMOVE_PKT_LOG
    case HTT_T2H_MSG_TYPE_PKTLOG:
        {
            u_int32_t *pl_hdr;
            u_int32_t log_type;
            pl_hdr = (msg_word + 1);
            log_type = (*(pl_hdr + 1) & ATH_PKTLOG_HDR_LOG_TYPE_MASK) >>
                                            ATH_PKTLOG_HDR_LOG_TYPE_SHIFT;
            if (log_type == PKTLOG_TYPE_TX_CTRL ||
               (log_type) == PKTLOG_TYPE_TX_STAT ||
               (log_type) == PKTLOG_TYPE_TX_MSDU_ID ||
               (log_type) == PKTLOG_TYPE_TX_FRM_HDR ||
               (log_type) == PKTLOG_TYPE_TX_VIRT_ADDR) {
                wdi_event_handler(WDI_EVENT_TX_STATUS, pdev->txrx_pdev, pl_hdr);
            } else if ((log_type) == PKTLOG_TYPE_RC_FIND) {
                wdi_event_handler(WDI_EVENT_RATE_FIND, pdev->txrx_pdev, pl_hdr);
            } else if ((log_type) == PKTLOG_TYPE_RC_UPDATE) {
                wdi_event_handler(
                    WDI_EVENT_RATE_UPDATE, pdev->txrx_pdev, pl_hdr);
            } else if ((log_type) == PKTLOG_TYPE_RX_STAT) {
                wdi_event_handler(WDI_EVENT_RX_DESC, pdev->txrx_pdev, pl_hdr);
            }
            break;
        }
#endif
    case HTT_T2H_MSG_TYPE_TX_CREDIT_UPDATE_IND:
    {
        u_int32_t htt_credit_delta_abs;
        int32_t htt_credit_delta;
        int sign;

        htt_credit_delta_abs = HTT_TX_CREDIT_DELTA_ABS_GET(*msg_word);
        sign = HTT_TX_CREDIT_SIGN_BIT_GET(*msg_word) ? -1 : 1;
        htt_credit_delta = sign * htt_credit_delta_abs;
        ol_tx_credit_completion_handler(pdev->txrx_pdev, htt_credit_delta);
        break;
    }

#ifdef IPA_UC_OFFLOAD
    case HTT_T2H_MSG_TYPE_WDI_IPA_OP_RESPONSE:
        {
            u_int8_t op_code;
            u_int16_t len;
            u_int8_t *op_msg_buffer;
            u_int8_t *msg_start_ptr;

            htc_pm_runtime_put(pdev->htc_pdev);
            msg_start_ptr = (u_int8_t *)msg_word;
            op_code = HTT_WDI_IPA_OP_RESPONSE_OP_CODE_GET(*msg_word);
            msg_word++;
            len = HTT_WDI_IPA_OP_RESPONSE_RSP_LEN_GET(*msg_word);

            op_msg_buffer = adf_os_mem_alloc(NULL,
                sizeof(struct htt_wdi_ipa_op_response_t) + len);
            if (!op_msg_buffer) {
                adf_os_print("OPCODE messsage buffer alloc fail");
                break;
            }
            adf_os_mem_copy(op_msg_buffer,
                    msg_start_ptr,
                    sizeof(struct htt_wdi_ipa_op_response_t) + len);
            ol_txrx_ipa_uc_op_response(pdev->txrx_pdev, op_msg_buffer);
            break;
        }
#endif /* IPA_UC_OFFLOAD */

    default:
        break;
    };
    /* Free the indication buffer */
    adf_nbuf_free(htt_t2h_msg);
}

/* Generic Target to host Msg/event  handler  for low priority messages
  Low priority message are handler in a different handler called from
  this function . So that the most likely succes path like Rx and
  Tx comp   has little code   foot print
 */
void
htt_t2h_msg_handler(void *context, HTC_PACKET *pkt)
{
    struct htt_pdev_t *pdev = (struct htt_pdev_t *) context;
    adf_nbuf_t htt_t2h_msg = (adf_nbuf_t) pkt->pPktContext;
    u_int32_t *msg_word;
    enum htt_t2h_msg_type msg_type;

    /* check for successful message reception */
    if (pkt->Status != A_OK) {
        if (pkt->Status != A_ECANCELED) {
            pdev->stats.htc_err_cnt++;
        }
        adf_nbuf_free(htt_t2h_msg);
        return;
    }

#ifdef HTT_RX_RESTORE
if (adf_os_unlikely(pdev->rx_ring.rx_reset)) {
        adf_os_print("rx restore ..\n");
        adf_nbuf_free(htt_t2h_msg);
        return;
    }
#endif

    /* confirm alignment */
    HTT_ASSERT3((((unsigned long) adf_nbuf_data(htt_t2h_msg)) & 0x3) == 0);

    msg_word = (u_int32_t *) adf_nbuf_data(htt_t2h_msg);
    msg_type = HTT_T2H_MSG_TYPE_GET(*msg_word);
    switch (msg_type) {
    case HTT_T2H_MSG_TYPE_RX_IND:
        {
            unsigned num_mpdu_ranges;
            unsigned num_msdu_bytes;
            u_int16_t peer_id;
            u_int8_t tid;

            if (adf_os_unlikely(pdev->cfg.is_full_reorder_offload)) {
                adf_os_print("HTT_T2H_MSG_TYPE_RX_IND not supported with full "
                             "reorder offload\n");
                break;
            }
            peer_id = HTT_RX_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_IND_EXT_TID_GET(*msg_word);

            if (tid >= OL_TXRX_NUM_EXT_TIDS) {
                adf_os_print("HTT_T2H_MSG_TYPE_RX_IND, invalid tid %d\n", tid);
                break;
            }

            num_msdu_bytes = HTT_RX_IND_FW_RX_DESC_BYTES_GET(
                *(msg_word + 2 + HTT_RX_PPDU_DESC_SIZE32));
            /*
             * 1 word for the message header,
             * HTT_RX_PPDU_DESC_SIZE32 words for the FW rx PPDU desc
             * 1 word to specify the number of MSDU bytes,
             * 1 word for every 4 MSDU bytes (round up),
             * 1 word for the MPDU range header
             */
            pdev->rx_mpdu_range_offset_words =
                (HTT_RX_IND_HDR_BYTES + num_msdu_bytes + 3) >> 2;
            num_mpdu_ranges = HTT_RX_IND_NUM_MPDU_RANGES_GET(*(msg_word + 1));
            pdev->rx_ind_msdu_byte_idx = 0;

            if (pdev->cfg.is_high_latency) {
                /*
                 * TODO: remove copy after stopping reuse skb on HIF layer
                 * because SDIO HIF may reuse skb before upper layer release it
                 */
                ol_rx_indication_handler(
                    pdev->txrx_pdev, htt_t2h_msg, peer_id, tid,
                    num_mpdu_ranges);

                return;
            } else {
                ol_rx_indication_handler(
                    pdev->txrx_pdev, htt_t2h_msg, peer_id, tid,
                    num_mpdu_ranges);
            }
            break;
        }
    case HTT_T2H_MSG_TYPE_TX_COMPL_IND:
        {
            int num_msdus;
            enum htt_tx_status status;

            /* status - no enum translation needed */
            status = HTT_TX_COMPL_IND_STATUS_GET(*msg_word);
            num_msdus = HTT_TX_COMPL_IND_NUM_GET(*msg_word);
            if (num_msdus & 0x1) {
                struct htt_tx_compl_ind_base *compl = (void *)msg_word;

                /*
                 * Host CPU endianness can be different from FW CPU. This
                 * can result in even and odd MSDU IDs being switched. If
                 * this happens, copy the switched final odd MSDU ID from
                 * location payload[size], to location payload[size-1],
                 * where the message handler function expects to find it
                 */
                if (compl->payload[num_msdus] != HTT_TX_COMPL_INV_MSDU_ID) {
                    compl->payload[num_msdus - 1] =
                        compl->payload[num_msdus];
                }
            }
            if (pdev->cfg.is_high_latency) {
                ol_tx_target_credit_update(
                    pdev->txrx_pdev, num_msdus /* 1 credit per MSDU */);
            }
            ol_tx_completion_handler(
                pdev->txrx_pdev, num_msdus, status, msg_word + 1);
            HTT_TX_SCHED(pdev);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_PN_IND:
        {
            u_int16_t peer_id;
            u_int8_t tid, pn_ie_cnt, *pn_ie=NULL;
            int seq_num_start, seq_num_end;

            /*First dword */
            peer_id = HTT_RX_PN_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_PN_IND_EXT_TID_GET(*msg_word);

            msg_word++;
            /*Second dword */
            seq_num_start = HTT_RX_PN_IND_SEQ_NUM_START_GET(*msg_word);
            seq_num_end = HTT_RX_PN_IND_SEQ_NUM_END_GET(*msg_word);
            pn_ie_cnt = HTT_RX_PN_IND_PN_IE_CNT_GET(*msg_word);

            msg_word++;
            /*Third dword*/
            if (pn_ie_cnt) {
                pn_ie = (u_int8_t *)msg_word;
            }

            ol_rx_pn_ind_handler(
                pdev->txrx_pdev, peer_id, tid, seq_num_start, seq_num_end,
                pn_ie_cnt, pn_ie);

            break;
        }
    case HTT_T2H_MSG_TYPE_TX_INSPECT_IND:
        {
            int num_msdus;

            num_msdus = HTT_TX_COMPL_IND_NUM_GET(*msg_word);
            if (num_msdus & 0x1) {
                struct htt_tx_compl_ind_base *compl = (void *)msg_word;

                /*
                 * Host CPU endianness can be different from FW CPU. This
                 * can result in even and odd MSDU IDs being switched. If
                 * this happens, copy the switched final odd MSDU ID from
                 * location payload[size], to location payload[size-1],
                 * where the message handler function expects to find it
                 */
                if (compl->payload[num_msdus] != HTT_TX_COMPL_INV_MSDU_ID) {
                    compl->payload[num_msdus - 1] =
                        compl->payload[num_msdus];
                }
            }
            ol_tx_inspect_handler(pdev->txrx_pdev, num_msdus, msg_word + 1);
            HTT_TX_SCHED(pdev);
            break;
        }
    case HTT_T2H_MSG_TYPE_RX_IN_ORD_PADDR_IND:
        {
            u_int16_t peer_id;
            u_int8_t tid;
            u_int8_t offload_ind, frag_ind;

            if (adf_os_unlikely(!pdev->cfg.is_full_reorder_offload)) {
                adf_os_print("HTT_T2H_MSG_TYPE_RX_IN_ORD_PADDR_IND not supported"
                             " when full reorder offload is disabled\n");
                break;
            }

            if (adf_os_unlikely(pdev->cfg.is_high_latency)) {
                adf_os_print("HTT_T2H_MSG_TYPE_RX_IN_ORD_PADDR_IND not supported"
                             " on high latency\n");
                break;
            }

            peer_id = HTT_RX_IN_ORD_PADDR_IND_PEER_ID_GET(*msg_word);
            tid = HTT_RX_IN_ORD_PADDR_IND_EXT_TID_GET(*msg_word);
            offload_ind = HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_GET(*msg_word);
            frag_ind = HTT_RX_IN_ORD_PADDR_IND_FRAG_GET(*msg_word);

            if (adf_os_unlikely(frag_ind)) {
                ol_rx_frag_indication_handler(pdev->txrx_pdev, htt_t2h_msg,
                                               peer_id, tid);
                break;
            }

            ol_rx_in_order_indication_handler(pdev->txrx_pdev, htt_t2h_msg,
                                               peer_id, tid, offload_ind);
            break;
     }

    default:
        htt_t2h_lp_msg_handler(context, htt_t2h_msg);
        return ;

    };

    /* Free the indication buffer */
    adf_nbuf_free(htt_t2h_msg);
}

/*--- target->host HTT message Info Element access methods ------------------*/

/*--- tx completion message ---*/

u_int16_t
htt_tx_compl_desc_id(void *iterator, int num)
{
    /*
     * The MSDU IDs are packed , 2 per 32-bit word.
     * Iterate on them as an array of 16-bit elements.
     * This will work fine if the host endianness matches
     * the target endianness.
     * If the host endianness is opposite of the target's,
     * this iterator will produce descriptor IDs in a different
     * order than the target inserted them into the message -
     * if the target puts in [0, 1, 2, 3, ...] the host will
     * put out [1, 0, 3, 2, ...].
     * This is fine, except for the last ID if there are an
     * odd number of IDs.  But the TX_COMPL_IND handling code
     * in the htt_t2h_msg_handler already added a duplicate
     * of the final ID, if there were an odd number of IDs,
     * so this function can safely treat the IDs as an array
     * of 16-bit elements.
     */
    return *(((u_int16_t *) iterator) + num);
}

/*--- rx indication message ---*/

int
htt_rx_ind_flush(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_ind_msg);
    return HTT_RX_IND_FLUSH_VALID_GET(*msg_word);
}

void
htt_rx_ind_flush_seq_num_range(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    unsigned *seq_num_start,
    unsigned *seq_num_end)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_ind_msg);
    msg_word++;
    *seq_num_start = HTT_RX_IND_FLUSH_SEQ_NUM_START_GET(*msg_word);
    *seq_num_end   = HTT_RX_IND_FLUSH_SEQ_NUM_END_GET(*msg_word);
}

int
htt_rx_ind_release(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_ind_msg);
    return HTT_RX_IND_REL_VALID_GET(*msg_word);
}

void
htt_rx_ind_release_seq_num_range(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    unsigned *seq_num_start,
    unsigned *seq_num_end)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_ind_msg);
    msg_word++;
    *seq_num_start = HTT_RX_IND_REL_SEQ_NUM_START_GET(*msg_word);
    *seq_num_end   = HTT_RX_IND_REL_SEQ_NUM_END_GET(*msg_word);
}

void
htt_rx_ind_mpdu_range_info(
    struct htt_pdev_t *pdev,
    adf_nbuf_t rx_ind_msg,
    int mpdu_range_num,
    enum htt_rx_status *status,
    int *mpdu_count)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_ind_msg);
    msg_word += pdev->rx_mpdu_range_offset_words + mpdu_range_num;
    *status = HTT_RX_IND_MPDU_STATUS_GET(*msg_word);
    *mpdu_count = HTT_RX_IND_MPDU_COUNT_GET(*msg_word);
}

#define HTT_TGT_NOISE_FLOOR_DBM (-95) /* approx */
int16_t
htt_rx_ind_rssi_dbm(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg)
{
    int16_t rssi;
    u_int32_t *msg_word;

    msg_word = (u_int32_t *)
        (adf_nbuf_data(rx_ind_msg) + HTT_RX_IND_FW_RX_PPDU_DESC_BYTE_OFFSET);

    /* check if the RX_IND message contains valid rx PPDU start info */
    if (!HTT_RX_IND_START_VALID_GET(*msg_word)) {
        return HTT_RSSI_INVALID;
    }

    rssi = HTT_RX_IND_RSSI_CMB_GET(*msg_word);
    return (HTT_TGT_RSSI_INVALID == rssi) ?
        HTT_RSSI_INVALID :
        rssi + HTT_TGT_NOISE_FLOOR_DBM;
}


/*--- stats confirmation message ---*/

void
htt_t2h_dbg_stats_hdr_parse(
    u_int8_t *stats_info_list,
    enum htt_dbg_stats_type *type,
    enum htt_dbg_stats_status *status,
    int *length,
    u_int8_t **stats_data)
{
    u_int32_t *msg_word = (u_int32_t *) stats_info_list;
    *type = HTT_T2H_STATS_CONF_TLV_TYPE_GET(*msg_word);
    *status = HTT_T2H_STATS_CONF_TLV_STATUS_GET(*msg_word);
    *length = HTT_T2H_STATS_CONF_TLV_HDR_SIZE +       /* header length */
        HTT_T2H_STATS_CONF_TLV_LENGTH_GET(*msg_word); /* data length */
    *stats_data = stats_info_list + HTT_T2H_STATS_CONF_TLV_HDR_SIZE;
}

void
htt_rx_frag_ind_flush_seq_num_range(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_frag_ind_msg,
    int *seq_num_start,
    int *seq_num_end)
{
    u_int32_t *msg_word;

    msg_word = (u_int32_t *) adf_nbuf_data(rx_frag_ind_msg);
    msg_word++;
    *seq_num_start = HTT_RX_FRAG_IND_FLUSH_SEQ_NUM_START_GET(*msg_word);
    *seq_num_end   = HTT_RX_FRAG_IND_FLUSH_SEQ_NUM_END_GET(*msg_word);
}
