/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
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
 * @file htt_fw_stats.c
 * @brief Provide functions to process FW status retrieved from FW.
 */

#include <htc_api.h>         /* HTC_PACKET */
#include <htt.h>             /* HTT_T2H_MSG_TYPE, etc. */
#include <adf_nbuf.h>        /* adf_nbuf_t */
#include <adf_os_mem.h>      /* adf_os_mem_set */
#include <ol_fw_tx_dbg.h>    /* ol_fw_tx_dbg_ppdu_base */

#include <ol_htt_rx_api.h>
#include <ol_txrx_htt_api.h> /* htt_tx_status */

#include <htt_internal.h>

#include <wlan_defs.h>

#define ROUND_UP_TO_4(val) (((val) + 3) & ~0x3)

static void htt_t2h_stats_tx_rate_stats_print(
    wlan_dbg_tx_rate_info_t *tx_rate_info, int concise)
{
    adf_os_print("TX Rate Info:\n");

    /* MCS */
    adf_os_print("MCS counts (0..9): ");
    adf_os_print("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
                  tx_rate_info->mcs[0],
                  tx_rate_info->mcs[1],
                  tx_rate_info->mcs[2],
                  tx_rate_info->mcs[3],
                  tx_rate_info->mcs[4],
                  tx_rate_info->mcs[5],
                  tx_rate_info->mcs[6],
                  tx_rate_info->mcs[7],
                  tx_rate_info->mcs[8],
                  tx_rate_info->mcs[9]
                  );

   /* SGI */
    adf_os_print("SGI counts (0..9): ");
    adf_os_print("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
                  tx_rate_info->sgi[0],
                  tx_rate_info->sgi[1],
                  tx_rate_info->sgi[2],
                  tx_rate_info->sgi[3],
                  tx_rate_info->sgi[4],
                  tx_rate_info->sgi[5],
                  tx_rate_info->sgi[6],
                  tx_rate_info->sgi[7],
                  tx_rate_info->sgi[8],
                  tx_rate_info->sgi[9]
                  );

    /* NSS */
    adf_os_print("NSS  counts: ");
    adf_os_print("1x1 %d, 2x2 %d, 3x3 %d\n",
                  tx_rate_info->nss[0],
                  tx_rate_info->nss[1],
                  tx_rate_info->nss[2]
                  );

    /* BW */
    adf_os_print("BW counts: ");
    adf_os_print("20MHz %d, 40MHz %d, 80MHz %d\n",
                  tx_rate_info->bw[0],
                  tx_rate_info->bw[1],
                  tx_rate_info->bw[2]
                  );

     /* Preamble */
    adf_os_print("Preamble (O C H V) counts: ");
    adf_os_print("%d, %d, %d, %d\n",
                  tx_rate_info->pream[0],
                  tx_rate_info->pream[1],
                  tx_rate_info->pream[2],
                  tx_rate_info->pream[3]
                  );

     /* STBC rate counts */
    adf_os_print("STBC rate counts (0..9): ");
    adf_os_print("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
                  tx_rate_info->stbc[0],
                  tx_rate_info->stbc[1],
                  tx_rate_info->stbc[2],
                  tx_rate_info->stbc[3],
                  tx_rate_info->stbc[4],
                  tx_rate_info->stbc[5],
                  tx_rate_info->stbc[6],
                  tx_rate_info->stbc[7],
                  tx_rate_info->stbc[8],
                  tx_rate_info->stbc[9]
                  );

     /* LDPC and TxBF counts */
    adf_os_print("LDPC Counts: ");
    adf_os_print("%d\n", tx_rate_info->ldpc);
    adf_os_print("RTS Counts: ");
    adf_os_print("%d\n", tx_rate_info->rts_cnt);
    /* RSSI Values for last ack frames */
    adf_os_print("Ack RSSI: %d\n",tx_rate_info->ack_rssi);
}

static void htt_t2h_stats_rx_rate_stats_print(
    wlan_dbg_rx_rate_info_t *rx_phy_info, int concise)
{
    adf_os_print("RX Rate Info:\n");

    /* MCS */
    adf_os_print("MCS counts (0..9): ");
    adf_os_print("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
                  rx_phy_info->mcs[0],
                  rx_phy_info->mcs[1],
                  rx_phy_info->mcs[2],
                  rx_phy_info->mcs[3],
                  rx_phy_info->mcs[4],
                  rx_phy_info->mcs[5],
                  rx_phy_info->mcs[6],
                  rx_phy_info->mcs[7],
                  rx_phy_info->mcs[8],
                  rx_phy_info->mcs[9]
                  );

   /* SGI */
    adf_os_print("SGI counts (0..9): ");
    adf_os_print("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
                  rx_phy_info->sgi[0],
                  rx_phy_info->sgi[1],
                  rx_phy_info->sgi[2],
                  rx_phy_info->sgi[3],
                  rx_phy_info->sgi[4],
                  rx_phy_info->sgi[5],
                  rx_phy_info->sgi[6],
                  rx_phy_info->sgi[7],
                  rx_phy_info->sgi[8],
                  rx_phy_info->sgi[9]
                  );

    /* NSS */
    adf_os_print("NSS  counts: ");
    /* nss[0] just holds the count of non-stbc frames that were sent at 1x1
     * rates and nsts holds the count of frames sent with stbc. It was decided
     * to not include PPDUs sent w/ STBC in nss[0] since it would be easier to
     * change the value that needs to be printed (from "stbc+non-stbc count to
     * only non-stbc count") if needed in the future. Hence the addition in the
     * host code at this line. */
    adf_os_print("1x1 %d, 2x2 %d, 3x3 %d, 4x4 %d\n",
                  rx_phy_info->nss[0] + rx_phy_info->nsts,
                  rx_phy_info->nss[1],
                  rx_phy_info->nss[2],
                  rx_phy_info->nss[3]
                  );

    /* NSTS */
    adf_os_print("NSTS count: ");
    adf_os_print("%d\n", rx_phy_info->nsts);

    /* BW */
    adf_os_print("BW counts: ");
    adf_os_print("20MHz %d, 40MHz %d, 80MHz %d\n",
                  rx_phy_info->bw[0],
                  rx_phy_info->bw[1],
                  rx_phy_info->bw[2]
                  );

     /* Preamble */
    adf_os_print("Preamble counts: ");
    adf_os_print("%d, %d, %d, %d, %d, %d\n",
                  rx_phy_info->pream[0],
                  rx_phy_info->pream[1],
                  rx_phy_info->pream[2],
                  rx_phy_info->pream[3],
                  rx_phy_info->pream[4],
                  rx_phy_info->pream[5]
                  );

     /* STBC rate counts */
    adf_os_print("STBC rate counts (0..9): ");
    adf_os_print("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
                  rx_phy_info->stbc[0],
                  rx_phy_info->stbc[1],
                  rx_phy_info->stbc[2],
                  rx_phy_info->stbc[3],
                  rx_phy_info->stbc[4],
                  rx_phy_info->stbc[5],
                  rx_phy_info->stbc[6],
                  rx_phy_info->stbc[7],
                  rx_phy_info->stbc[8],
                  rx_phy_info->stbc[9]
                  );

     /* LDPC and TxBF counts */
    adf_os_print("LDPC TXBF Counts: ");
    adf_os_print("%d, %d\n", rx_phy_info->ldpc, rx_phy_info->txbf);
    /* RSSI Values for last received frames */
    adf_os_print("RSSI (data, mgmt): %d, %d\n",rx_phy_info->data_rssi,
                    rx_phy_info->mgmt_rssi);

    adf_os_print("RSSI Chain 0 (0x%02x 0x%02x 0x%02x 0x%02x)\n",
        ((rx_phy_info->rssi_chain0 >> 24) & 0xff),
        ((rx_phy_info->rssi_chain0 >> 16) & 0xff),
        ((rx_phy_info->rssi_chain0 >> 8) & 0xff),
        ((rx_phy_info->rssi_chain0 >> 0) & 0xff));

    adf_os_print("RSSI Chain 1 (0x%02x 0x%02x 0x%02x 0x%02x)\n",
        ((rx_phy_info->rssi_chain1 >> 24) & 0xff),
        ((rx_phy_info->rssi_chain1 >> 16) & 0xff),
        ((rx_phy_info->rssi_chain1 >> 8) & 0xff),
        ((rx_phy_info->rssi_chain1 >> 0) & 0xff));

    adf_os_print("RSSI Chain 2 (0x%02x 0x%02x 0x%02x 0x%02x)\n",
        ((rx_phy_info->rssi_chain2 >> 24) & 0xff),
        ((rx_phy_info->rssi_chain2 >> 16) & 0xff),
        ((rx_phy_info->rssi_chain2 >> 8) & 0xff),
        ((rx_phy_info->rssi_chain2 >> 0) & 0xff));
}


static void
htt_t2h_stats_pdev_stats_print(
    struct wlan_dbg_stats *wlan_pdev_stats, int concise)
{
    struct wlan_dbg_tx_stats *tx = &wlan_pdev_stats->tx;
    struct wlan_dbg_rx_stats *rx = &wlan_pdev_stats->rx;

    adf_os_print("WAL Pdev stats:\n");
    adf_os_print("\n### Tx ###\n");

    /* Num HTT cookies queued to dispatch list */
	adf_os_print("comp_queued       :\t%d\n",tx->comp_queued);
    /* Num HTT cookies dispatched */
	adf_os_print("comp_delivered    :\t%d\n",tx->comp_delivered);
    /* Num MSDU queued to WAL */
	adf_os_print("msdu_enqued       :\t%d\n",tx->msdu_enqued);
    /* Num MPDU queued to WAL */
	adf_os_print("mpdu_enqued       :\t%d\n",tx->mpdu_enqued);
    /* Num MSDUs dropped by WMM limit */
	adf_os_print("wmm_drop          :\t%d\n",tx->wmm_drop);
    /* Num Local frames queued */
	adf_os_print("local_enqued      :\t%d\n",tx->local_enqued);
    /* Num Local frames done */
	adf_os_print("local_freed       :\t%d\n",tx->local_freed);
    /* Num queued to HW */
	adf_os_print("hw_queued         :\t%d\n",tx->hw_queued);
    /* Num PPDU reaped from HW */
	adf_os_print("hw_reaped         :\t%d\n",tx->hw_reaped);
    /* Num underruns */
	adf_os_print("mac underrun      :\t%d\n",tx->underrun);
    /* Num underruns */
	adf_os_print("phy underrun      :\t%d\n",tx->phy_underrun);
    /* Num PPDUs cleaned up in TX abort */
    adf_os_print("tx_abort          :\t%d\n",tx->tx_abort);
    /* Num MPDUs requed by SW */
    adf_os_print("mpdus_requed      :\t%d\n",tx->mpdus_requed);
    /* Excessive retries */
    adf_os_print("excess retries    :\t%d\n",tx->tx_ko);
    /* last data rate */
    adf_os_print("last rc           :\t%d\n",tx->data_rc);
    /* scheduler self triggers */
    adf_os_print("sched self trig   :\t%d\n",tx->self_triggers);
    /* SW retry failures */
    adf_os_print("ampdu retry failed:\t%d\n",tx->sw_retry_failure);
    /* ilegal phy rate errirs */
    adf_os_print("illegal rate errs :\t%d\n",tx->illgl_rate_phy_err);
    /* pdev continous excessive retries  */
    adf_os_print("pdev cont xretry  :\t%d\n",tx->pdev_cont_xretry);
    /* pdev continous excessive retries  */
    adf_os_print("pdev tx timeout   :\t%d\n",tx->pdev_tx_timeout);
    /* pdev resets  */
    adf_os_print("pdev resets       :\t%d\n",tx->pdev_resets);
    /* PPDU > txop duration  */
    adf_os_print("ppdu txop ovf     :\t%d\n",tx->txop_ovf);

    adf_os_print("\n### Rx ###\n");
    /* Cnts any change in ring routing mid-ppdu */
	adf_os_print("ppdu_route_change :\t%d\n",rx->mid_ppdu_route_change);
    /* Total number of statuses processed */
	adf_os_print("status_rcvd       :\t%d\n",rx->status_rcvd);
    /* Extra frags on rings 0-3 */
	adf_os_print("r0_frags          :\t%d\n",rx->r0_frags);
	adf_os_print("r1_frags          :\t%d\n",rx->r1_frags);
	adf_os_print("r2_frags          :\t%d\n",rx->r2_frags);
	adf_os_print("r3_frags          :\t%d\n",rx->r3_frags);
    /* MSDUs / MPDUs delivered to HTT */
	adf_os_print("htt_msdus         :\t%d\n",rx->htt_msdus);
	adf_os_print("htt_mpdus         :\t%d\n",rx->htt_mpdus);
    /* MSDUs / MPDUs delivered to local stack */
	adf_os_print("loc_msdus         :\t%d\n",rx->loc_msdus);
	adf_os_print("loc_mpdus         :\t%d\n",rx->loc_mpdus);
    /* AMSDUs that have more MSDUs than the status ring size */
	adf_os_print("oversize_amsdu    :\t%d\n",rx->oversize_amsdu);
    /* Number of PHY errors */
	adf_os_print("phy_errs          :\t%d\n",rx->phy_errs);
    /* Number of PHY errors dropped */
	adf_os_print("phy_errs dropped  :\t%d\n",rx->phy_err_drop);
    /* Number of mpdu errors - FCS, MIC, ENC etc. */
	adf_os_print("mpdu_errs         :\t%d\n",rx->mpdu_errs);

}

static void
htt_t2h_stats_rx_reorder_stats_print(
    struct rx_reorder_stats *stats_ptr, int concise)
{
    adf_os_print("Rx reorder statistics:\n");
    adf_os_print("  %u non-QoS frames received\n",
                 stats_ptr->deliver_non_qos);
    adf_os_print("  %u frames received in-order\n",
                 stats_ptr->deliver_in_order);
    adf_os_print("  %u frames flushed due to timeout\n",
                 stats_ptr->deliver_flush_timeout);
    adf_os_print("  %u frames flushed due to moving out of window\n",
                 stats_ptr->deliver_flush_oow);
    adf_os_print("  %u frames flushed due to receiving DELBA\n",
                 stats_ptr->deliver_flush_delba);
    adf_os_print("  %u frames discarded due to FCS error\n",
                 stats_ptr->fcs_error);
    adf_os_print("  %u frames discarded due to invalid peer\n",
                 stats_ptr->invalid_peer);
    adf_os_print("  %u frames discarded due to duplication (non aggregation)\n",
                 stats_ptr->dup_non_aggr);
    adf_os_print("  %u frames discarded due to duplication in "
                 "reorder queue\n", stats_ptr->dup_in_reorder);
    adf_os_print("  %u frames discarded due to processed before\n",
                 stats_ptr->dup_past);
    adf_os_print("  %u times reorder timeout happened\n",
                 stats_ptr->reorder_timeout);
    adf_os_print("  %u times incorrect bar received\n",
                 stats_ptr->invalid_bar_ssn);
    adf_os_print("  %u times bar ssn reset happened\n",
                 stats_ptr->ssn_reset);
    adf_os_print("  %u times flushed due to peer delete\n",
                 stats_ptr->deliver_flush_delpeer);
    adf_os_print("  %u times flushed due to offload\n",
                 stats_ptr->deliver_flush_offload);
    adf_os_print("  %u times flushed due to ouf of buffer\n",
                 stats_ptr->deliver_flush_oob);
    adf_os_print("  %u MPDU's dropped due to PN check fail\n",
                 stats_ptr->pn_fail);
    adf_os_print("  %u MPDU's dropped due to lack of memory\n",
                 stats_ptr->store_fail);
    adf_os_print("  %u times tid pool alloc succeeded\n",
                 stats_ptr->tid_pool_alloc_succ);
    adf_os_print("  %u times MPDU pool alloc succeeded\n",
                 stats_ptr->mpdu_pool_alloc_succ);
    adf_os_print("  %u times MSDU pool alloc succeeded\n",
                 stats_ptr->msdu_pool_alloc_succ);
    adf_os_print("  %u times tid pool alloc failed\n",
                 stats_ptr->tid_pool_alloc_fail);
    adf_os_print("  %u times MPDU pool alloc failed\n",
                 stats_ptr->mpdu_pool_alloc_fail);
    adf_os_print("  %u times MSDU pool alloc failed\n",
                 stats_ptr->msdu_pool_alloc_fail);
    adf_os_print("  %u times tid pool freed\n",
                 stats_ptr->tid_pool_free);
    adf_os_print("  %u times MPDU pool freed\n",
                 stats_ptr->mpdu_pool_free);
    adf_os_print("  %u times MSDU pool freed\n",
                 stats_ptr->msdu_pool_free);
    adf_os_print("  %u MSDUs undelivered to HTT, queued to Rx MSDU free list\n",
                 stats_ptr->msdu_queued);
    adf_os_print("  %u MSDUs released from Rx MSDU list to MAC ring\n",
                 stats_ptr->msdu_recycled);
    adf_os_print("  %u MPDUs with invalid peer but A2 found in AST\n",
                 stats_ptr->invalid_peer_a2_in_ast);
    adf_os_print("  %u MPDUs with invalid peer but A3 found in AST\n",
                 stats_ptr->invalid_peer_a3_in_ast);
    adf_os_print("  %u MPDUs with invalid peer, Broadcast or Mulitcast frame\n",
                 stats_ptr->invalid_peer_bmc_mpdus);
    adf_os_print("  %u MSDUs with err attention word\n",
                 stats_ptr->rxdesc_err_att);
    adf_os_print("  %u MSDUs with flag of peer_idx_invalid\n",
                 stats_ptr->rxdesc_err_peer_idx_inv);
    adf_os_print("  %u MSDUs with  flag of peer_idx_timeout\n",
                 stats_ptr->rxdesc_err_peer_idx_to);
    adf_os_print("  %u MSDUs with  flag of overflow\n",
                 stats_ptr->rxdesc_err_ov);
    adf_os_print("  %u MSDUs with  flag of msdu_length_err\n",
                 stats_ptr->rxdesc_err_msdu_len);
    adf_os_print("  %u MSDUs with  flag of mpdu_length_err\n",
                 stats_ptr->rxdesc_err_mpdu_len);
    adf_os_print("  %u MSDUs with  flag of tkip_mic_err\n",
                 stats_ptr->rxdesc_err_tkip_mic);
    adf_os_print("  %u MSDUs with  flag of decrypt_err\n",
                 stats_ptr->rxdesc_err_decrypt);
    adf_os_print("  %u MSDUs with  flag of fcs_err\n",
                 stats_ptr->rxdesc_err_fcs);
    adf_os_print("  %u Unicast frames with invalid peer handler\n",
                 stats_ptr->rxdesc_uc_msdus_inv_peer);
    adf_os_print("  %u unicast frame directly to DUT with invalid peer handler\n",
                 stats_ptr->rxdesc_direct_msdus_inv_peer);
    adf_os_print("  %u Broadcast/Multicast frames with invalid peer handler\n",
                 stats_ptr->rxdesc_bmc_msdus_inv_peer);
    adf_os_print("  %u MSDUs dropped due to no first MSDU flag\n",
                 stats_ptr->rxdesc_no_1st_msdu);
    adf_os_print("  %u MSDUs dropped due to ring overflow\n",
                 stats_ptr->msdu_drop_ring_ov);
    adf_os_print("  %u MSDUs dropped due to FC mismatch\n",
                 stats_ptr->msdu_drop_fc_mismatch);
    adf_os_print("  %u MSDUs dropped due to mgt frame in Remote ring\n",
                 stats_ptr->msdu_drop_mgmt_remote_ring);
    adf_os_print("  %u MSDUs dropped due to misc non error\n",
                 stats_ptr->msdu_drop_misc);
    adf_os_print("  %u MSDUs go to offload before reorder\n",
                 stats_ptr->offload_msdu_wal);
    adf_os_print("  %u data frame dropped by offload after reorder\n",
                 stats_ptr->offload_msdu_reorder);
    adf_os_print("  %u  MPDUs with SN in the past & within BA window\n",
                 stats_ptr->dup_past_within_window);
    adf_os_print("  %u  MPDUs with SN in the past & outside BA window\n",
                 stats_ptr->dup_past_outside_window);
}

static void
htt_t2h_stats_rx_rem_buf_stats_print(
    struct rx_remote_buffer_mgmt_stats *stats_ptr, int concise)
{
    adf_os_print("Rx Remote Buffer Statistics:\n");
    adf_os_print("  %u MSDU's reaped for Rx processing\n",
                 stats_ptr->remote_reaped);
    adf_os_print("  %u MSDU's recycled within firmware\n",
                 stats_ptr->remote_recycled);
    adf_os_print("  %u MSDU's stored by Data Rx\n",
                 stats_ptr->data_rx_msdus_stored);
    adf_os_print("  %u HTT indications from WAL Rx MSDU\n",
                 stats_ptr->wal_rx_ind);
    adf_os_print("  %u HTT indications unconsumed from WAL Rx MSDU\n",
                 stats_ptr->wal_rx_ind_unconsumed);
    adf_os_print("  %u HTT indications from Data Rx MSDU\n",
                 stats_ptr->data_rx_ind);
    adf_os_print("  %u HTT indications unconsumed from Data Rx MSDU\n",
                 stats_ptr->data_rx_ind_unconsumed);
    adf_os_print("  %u HTT indications from ATHBUF\n",
                 stats_ptr->athbuf_rx_ind);
    adf_os_print("  %u Remote buffers requested for refill\n",
                 stats_ptr->refill_buf_req);
    adf_os_print("  %u Remote buffers filled by host\n",
                 stats_ptr->refill_buf_rsp);
    adf_os_print("  %u times MAC has no buffers\n",
                 stats_ptr->mac_no_bufs);
    adf_os_print("  %u times f/w write & read indices on MAC ring are equal\n",
                 stats_ptr->fw_indices_equal);
    adf_os_print("  %u times f/w has no remote buffers to post to MAC\n",
                 stats_ptr->host_no_bufs);
}

#define HTT_T2H_STATS_TX_PPDU_TIME_TO_MICROSEC(ticks, microsec_per_tick) \
    (ticks * microsec_per_tick)
static inline int
HTT_T2H_STATS_TX_PPDU_RATE_FLAGS_TO_MHZ(u_int8_t rate_flags)
{
    if (rate_flags & 0x20) return 40;   /* WHAL_RC_FLAG_40MHZ */
    if (rate_flags & 0x40) return 80;   /* WHAL_RC_FLAG_80MHZ */
    if (rate_flags & 0x80) return 160;  /* WHAL_RC_FLAG_160MHZ */
    return 20;
}

#define HTT_FW_STATS_MAX_BLOCK_ACK_WINDOW 64

static void
htt_t2h_tx_ppdu_log_bitmaps_print(
    u_int32_t *queued_ptr,
    u_int32_t *acked_ptr)
{
    char queued_str[HTT_FW_STATS_MAX_BLOCK_ACK_WINDOW+1];
    char acked_str[HTT_FW_STATS_MAX_BLOCK_ACK_WINDOW+1];
    int i, j, word;

    adf_os_mem_set(queued_str, '0', HTT_FW_STATS_MAX_BLOCK_ACK_WINDOW);
    adf_os_mem_set(acked_str, '-', HTT_FW_STATS_MAX_BLOCK_ACK_WINDOW);
    i = 0;
    for (word = 0; word < 2; word++) {
        u_int32_t queued = *(queued_ptr + word);
        u_int32_t acked = *(acked_ptr + word);
        for (j = 0; j < 32; j++, i++) {
            if (queued & (1 << j)) {
                queued_str[i] = '1';
                acked_str[i] = (acked & (1 << j)) ? 'y' : 'N';
            }
        }
    }
    queued_str[HTT_FW_STATS_MAX_BLOCK_ACK_WINDOW] = '\0';
    acked_str[HTT_FW_STATS_MAX_BLOCK_ACK_WINDOW] = '\0';
    adf_os_print("%s\n", queued_str);
    adf_os_print("%s\n", acked_str);
}

static inline u_int16_t htt_msg_read16(u_int16_t *p16)
{
#ifdef BIG_ENDIAN_HOST
    /*
     * During upload, the bytes within each u_int32_t word were
     * swapped by the HIF HW.  This results in the lower and upper bytes
     * of each u_int16_t to be in the correct big-endian order with
     * respect to each other, but for each even-index u_int16_t to
     * have its position switched with its successor neighbor u_int16_t.
     * Undo this u_int16_t position swapping.
     */
    return (((size_t) p16) & 0x2) ? *(p16 - 1) : *(p16 + 1);
#else
    return *p16;
#endif
}

static inline u_int8_t htt_msg_read8(u_int8_t *p8)
{
#ifdef BIG_ENDIAN_HOST
    /*
     * During upload, the bytes within each u_int32_t word were
     * swapped by the HIF HW.
     * Undo this byte swapping.
     */
    switch (((size_t) p8) & 0x3) {
    case 0:
        return *(p8 + 3);
    case 1:
        return *(p8 + 1);
    case 2:
        return *(p8 - 1);
    default /* 3 */:
        return *(p8 - 3);
    }
#else
    return *p8;
#endif
}

void htt_make_u8_list_str(
    u_int32_t *aligned_data,
    char *buffer,
    int space,
    int max_elems)
{
    u_int8_t *p8 = (u_int8_t *) aligned_data;
    char *buf_p = buffer;
    while (max_elems-- > 0) {
        int bytes;
        u_int8_t val;

        val = htt_msg_read8(p8);
        if (val == 0) {
            break; /* not enough data to fill the reserved msg buffer space */
        }
        bytes = adf_os_snprint(buf_p, space, "%d,", val);
        space -= bytes;
        if (space > 0) {
            buf_p += bytes;
        } else {
            break; /* not enough print buffer space for all the data */
        }
        p8++;
    }
    if (buf_p == buffer) {
        *buf_p = '\0'; /* nothing was written */
    } else {
        *(buf_p - 1) = '\0'; /* erase the final comma */
    }
}

void htt_make_u16_list_str(
    u_int32_t *aligned_data,
    char *buffer,
    int space,
    int max_elems)
{
    u_int16_t *p16 = (u_int16_t *) aligned_data;
    char *buf_p = buffer;
    while (max_elems-- > 0) {
        int bytes;
        u_int16_t val;

        val = htt_msg_read16(p16);
        if (val == 0) {
            break; /* not enough data to fill the reserved msg buffer space */
        }
        bytes = adf_os_snprint(buf_p, space, "%d,", val);
        space -= bytes;
        if (space > 0) {
            buf_p += bytes;
        } else {
            break; /* not enough print buffer space for all the data */
        }
        p16++;
    }
    if (buf_p == buffer) {
        *buf_p = '\0'; /* nothing was written */
    } else {
        *(buf_p - 1) = '\0'; /* erase the final comma */
    }
}

void
htt_t2h_tx_ppdu_log_print(
    struct ol_fw_tx_dbg_ppdu_msg_hdr *hdr,
    struct ol_fw_tx_dbg_ppdu_base *record,
    int length, int concise)
{
    int i;
    int record_size;
    int num_records;

    record_size =
        sizeof(*record) +
        hdr->mpdu_bytes_array_len * sizeof(u_int16_t) +
        hdr->mpdu_msdus_array_len * sizeof(u_int8_t) +
        hdr->msdu_bytes_array_len * sizeof(u_int16_t);
    num_records = (length - sizeof(*hdr)) / record_size;
    adf_os_print("Tx PPDU log elements:\n");

    for (i = 0; i < num_records; i++) {
        u_int16_t start_seq_num;
        u_int16_t start_pn_lsbs;
        u_int8_t  num_mpdus;
        u_int16_t peer_id;
        u_int8_t  ext_tid;
        u_int8_t  rate_code;
        u_int8_t  rate_flags;
        u_int8_t  tries;
        u_int8_t  complete;
        u_int32_t time_enqueue_us;
        u_int32_t time_completion_us;
        u_int32_t *msg_word = (u_int32_t *) record;

        /* fields used for both concise and complete printouts */
        start_seq_num =
            ((*(msg_word + OL_FW_TX_DBG_PPDU_START_SEQ_NUM_WORD)) &
            OL_FW_TX_DBG_PPDU_START_SEQ_NUM_M) >>
            OL_FW_TX_DBG_PPDU_START_SEQ_NUM_S;
        complete =
            ((*(msg_word + OL_FW_TX_DBG_PPDU_COMPLETE_WORD)) &
            OL_FW_TX_DBG_PPDU_COMPLETE_M) >>
            OL_FW_TX_DBG_PPDU_COMPLETE_S;

        /* fields used only for complete printouts */
        if (! concise) {
            #define BUF_SIZE 80
            char buf[BUF_SIZE];
            u_int8_t *p8;
            time_enqueue_us = HTT_T2H_STATS_TX_PPDU_TIME_TO_MICROSEC(
                    record->timestamp_enqueue, hdr->microsec_per_tick);
            time_completion_us = HTT_T2H_STATS_TX_PPDU_TIME_TO_MICROSEC(
                    record->timestamp_completion, hdr->microsec_per_tick);

            start_pn_lsbs =
                ((*(msg_word + OL_FW_TX_DBG_PPDU_START_PN_LSBS_WORD)) &
                OL_FW_TX_DBG_PPDU_START_PN_LSBS_M) >>
                OL_FW_TX_DBG_PPDU_START_PN_LSBS_S;
            num_mpdus =
                ((*(msg_word + OL_FW_TX_DBG_PPDU_NUM_MPDUS_WORD)) &
                OL_FW_TX_DBG_PPDU_NUM_MPDUS_M) >>
                OL_FW_TX_DBG_PPDU_NUM_MPDUS_S;
            peer_id =
                ((*(msg_word + OL_FW_TX_DBG_PPDU_PEER_ID_WORD)) &
                OL_FW_TX_DBG_PPDU_PEER_ID_M) >>
                OL_FW_TX_DBG_PPDU_PEER_ID_S;
            ext_tid =
                ((*(msg_word + OL_FW_TX_DBG_PPDU_EXT_TID_WORD)) &
                OL_FW_TX_DBG_PPDU_EXT_TID_M) >>
                OL_FW_TX_DBG_PPDU_EXT_TID_S;
            rate_code =
                ((*(msg_word + OL_FW_TX_DBG_PPDU_RATE_CODE_WORD)) &
                OL_FW_TX_DBG_PPDU_RATE_CODE_M) >>
                OL_FW_TX_DBG_PPDU_RATE_CODE_S;
            rate_flags =
                ((*(msg_word + OL_FW_TX_DBG_PPDU_RATE_FLAGS_WORD)) &
                OL_FW_TX_DBG_PPDU_RATE_FLAGS_M) >>
                OL_FW_TX_DBG_PPDU_RATE_FLAGS_S;
            tries =
                ((*(msg_word + OL_FW_TX_DBG_PPDU_TRIES_WORD)) &
                OL_FW_TX_DBG_PPDU_TRIES_M) >>
                OL_FW_TX_DBG_PPDU_TRIES_S;

            adf_os_print("  - PPDU tx to peer %d, TID %d\n", peer_id, ext_tid);
            adf_os_print("    start seq num = %u, start PN LSBs = %#04x\n",
                start_seq_num, start_pn_lsbs);
            adf_os_print("    PPDU is %d MPDUs, (unknown) MSDUs, %d bytes\n",
                num_mpdus,
                //num_msdus, /* not yet being computed in target */
                record->num_bytes);
            if (complete) {
                adf_os_print("    enqueued at %u, completed at %u (microsec)\n",
                    time_enqueue_us, time_completion_us);
                adf_os_print(
                    "    %d total tries, last tx used rate %d "
                    "on %d MHz chan (flags = %#x)\n",
                    tries, rate_code,
                    HTT_T2H_STATS_TX_PPDU_RATE_FLAGS_TO_MHZ(rate_flags),
                    rate_flags);
                adf_os_print("    enqueued and acked MPDU bitmaps:\n");
                htt_t2h_tx_ppdu_log_bitmaps_print(
                    msg_word + OL_FW_TX_DBG_PPDU_ENQUEUED_LSBS_WORD,
                    msg_word + OL_FW_TX_DBG_PPDU_BLOCK_ACK_LSBS_WORD);
            } else {
                adf_os_print(
                    "    enqueued at %d ms (microsec), not yet completed\n",
                    time_enqueue_us);
            }
            /* skip past the regular message fields to reach the tail area */
            p8 = (u_int8_t *) record;
            p8 += sizeof(struct ol_fw_tx_dbg_ppdu_base);
            if (hdr->mpdu_bytes_array_len) {
                htt_make_u16_list_str(
                    (u_int32_t *) p8, buf, BUF_SIZE, hdr->mpdu_bytes_array_len);
                adf_os_print("    MPDU bytes: %s\n", buf);
            }
            p8 += hdr->mpdu_bytes_array_len * sizeof(u_int16_t);
            if (hdr->mpdu_msdus_array_len) {
                htt_make_u8_list_str(
                    (u_int32_t *) p8, buf, BUF_SIZE, hdr->mpdu_msdus_array_len);
                adf_os_print("    MPDU MSDUs: %s\n", buf);
            }
            p8 += hdr->mpdu_msdus_array_len * sizeof(u_int8_t);
            if (hdr->msdu_bytes_array_len) {
                htt_make_u16_list_str(
                    (u_int32_t *) p8, buf, BUF_SIZE, hdr->msdu_bytes_array_len);
                adf_os_print("    MSDU bytes: %s\n", buf);
            }
        } else {
            /* concise */
            adf_os_print(
                "start seq num = %u, enqueued and acked MPDU bitmaps:\n",
                start_seq_num);
            if (complete) {
                htt_t2h_tx_ppdu_log_bitmaps_print(
                    msg_word + OL_FW_TX_DBG_PPDU_ENQUEUED_LSBS_WORD,
                    msg_word + OL_FW_TX_DBG_PPDU_BLOCK_ACK_LSBS_WORD);
            } else {
                adf_os_print("(not completed)\n");
            }
        }
        record = (struct ol_fw_tx_dbg_ppdu_base *)
            (((u_int8_t *) record) + record_size);
    }
}

void
htt_t2h_stats_print(u_int8_t *stats_data, int concise)
{
    u_int32_t *msg_word = (u_int32_t *)stats_data;
    enum htt_dbg_stats_type   type;
    enum htt_dbg_stats_status status;
    int length;

    type = HTT_T2H_STATS_CONF_TLV_TYPE_GET(*msg_word);
    status = HTT_T2H_STATS_CONF_TLV_STATUS_GET(*msg_word);
    length = HTT_T2H_STATS_CONF_TLV_LENGTH_GET(*msg_word);

    /* check that we've been given a valid stats type */
    if (status == HTT_DBG_STATS_STATUS_SERIES_DONE) {
        return;
    } else if (status == HTT_DBG_STATS_STATUS_INVALID) {
        adf_os_print(
            "Target doesn't support stats type %d\n", type);
        return;
    } else if (status == HTT_DBG_STATS_STATUS_ERROR) {
        adf_os_print(
            "Target couldn't upload stats type %d (no mem?)\n", type);
        return;
    }
    /* got valid (though perhaps partial) stats - process them */
    switch (type) {
    case HTT_DBG_STATS_WAL_PDEV_TXRX:
        {
            struct wlan_dbg_stats *wlan_dbg_stats_ptr;

            wlan_dbg_stats_ptr = (struct wlan_dbg_stats *)(msg_word + 1);
            htt_t2h_stats_pdev_stats_print(wlan_dbg_stats_ptr, concise);
            break;
        }
    case HTT_DBG_STATS_RX_REORDER:
        {
            struct rx_reorder_stats *rx_reorder_stats_ptr;

            rx_reorder_stats_ptr = (struct rx_reorder_stats *)(msg_word + 1);
            htt_t2h_stats_rx_reorder_stats_print(rx_reorder_stats_ptr, concise);
            break;
        }

    case HTT_DBG_STATS_RX_RATE_INFO:
        {
            wlan_dbg_rx_rate_info_t *rx_phy_info;
            rx_phy_info = (wlan_dbg_rx_rate_info_t *)(msg_word + 1);
            htt_t2h_stats_rx_rate_stats_print(rx_phy_info, concise);
            break;
        }
    case HTT_DBG_STATS_TX_PPDU_LOG:
        {
            struct ol_fw_tx_dbg_ppdu_msg_hdr *hdr;
            struct ol_fw_tx_dbg_ppdu_base *record;

            if (status == HTT_DBG_STATS_STATUS_PARTIAL && length == 0) {
               adf_os_print("HTT_DBG_STATS_TX_PPDU_LOG -- length = 0!\n");
               break;
            }
            hdr = (struct ol_fw_tx_dbg_ppdu_msg_hdr *)(msg_word + 1);
            record = (struct ol_fw_tx_dbg_ppdu_base *)(hdr + 1);
            htt_t2h_tx_ppdu_log_print(hdr, record, length, concise);
        }
        break;
    case HTT_DBG_STATS_TX_RATE_INFO:
        {
            wlan_dbg_tx_rate_info_t *tx_rate_info;
            tx_rate_info = (wlan_dbg_tx_rate_info_t *)(msg_word + 1);
            htt_t2h_stats_tx_rate_stats_print(tx_rate_info, concise);
            break;
        }
    case HTT_DBG_STATS_RX_REMOTE_RING_BUFFER_INFO:
        {

            struct rx_remote_buffer_mgmt_stats *rx_rem_buf;

            rx_rem_buf = (struct rx_remote_buffer_mgmt_stats *)(msg_word + 1);
            htt_t2h_stats_rx_rem_buf_stats_print(rx_rem_buf, concise);
            break;
        }
    default:
        break;
    }
}
