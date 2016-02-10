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

#include <adf_nbuf.h>          /* adf_nbuf_t, etc. */
#include <adf_os_io.h>         /* adf_os_cpu_to_le64 */
#include <adf_os_types.h>      /* a_bool_t */
#include <ieee80211_common.h>         /* ieee80211_frame */

/* external API header files */
#include <ol_ctrl_txrx_api.h>  /* ol_rx_notify */
#include <ol_htt_api.h>        /* htt_pdev_handle */
#include <ol_txrx_api.h>       /* ol_txrx_pdev_handle */
#include <ol_txrx_htt_api.h>   /* ol_rx_indication_handler */
#include <ol_htt_rx_api.h>     /* htt_rx_peer_id, etc. */

/* internal API header files */
#include <ol_txrx_types.h>     /* ol_txrx_vdev_t, etc. */
#include <ol_txrx_peer_find.h> /* ol_txrx_peer_find_by_id */
#include <ol_rx_reorder.h>     /* ol_rx_reorder_store, etc. */
#include <ol_rx_reorder_timeout.h> /* OL_RX_REORDER_TIMEOUT_UPDATE */
#include <ol_rx_defrag.h>      /* ol_rx_defrag_waitlist_flush */
#include <ol_txrx_internal.h>
#include <wdi_event.h>
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
#include <ol_txrx_encap.h>         /* ol_rx_decap_info_t, etc*/
#endif

/* FIX THIS: txrx should not include private header files of other modules */
#include <htt_types.h>
#include <ol_if_athvar.h>
#include <enet.h>    /* ethernet + SNAP/LLC header defs and ethertype values */
#include <ip_prot.h> /* IP protocol values */
#include <ipv4.h>    /* IPv4 header defs */
#include <ipv6_defs.h>    /* IPv6 header defs */
#include <ol_vowext_dbg_defs.h>
#include <wma.h>

#ifdef HTT_RX_RESTORE
#include "vos_cnss.h"
#endif

#ifdef OSIF_NEED_RX_PEER_ID
#define OL_RX_OSIF_DELIVER(vdev, peer, msdus) \
       vdev->osif_rx(vdev->osif_dev, peer->local_id, msdus)
#else
#define OL_RX_OSIF_DELIVER(vdev, peer, msdus) \
       vdev->osif_rx(vdev->osif_dev, msdus)
#endif /* OSIF_NEED_RX_PEER_ID */

#ifdef HTT_RX_RESTORE

static void ol_rx_restore_handler(struct work_struct *htt_rx)
{
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
        "Enter: %s", __func__);
    vos_device_self_recovery();
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
        "Exit: %s", __func__);
}

static DECLARE_WORK(ol_rx_restore_work, ol_rx_restore_handler);

void ol_rx_trigger_restore(htt_pdev_handle htt_pdev, adf_nbuf_t head_msdu,
                        adf_nbuf_t tail_msdu)
{
    adf_nbuf_t next;

    while (head_msdu) {
        next = adf_nbuf_next(head_msdu);
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
            "freeing %p\n", head_msdu);
        adf_nbuf_free(head_msdu);
        head_msdu = next;
    }

    if ( !htt_pdev->rx_ring.htt_rx_restore){
        vos_set_logp_in_progress(VOS_MODULE_ID_VOSS, TRUE);
        htt_pdev->rx_ring.htt_rx_restore = 1;
        schedule_work(&ol_rx_restore_work);
    }
}
#endif

static void ol_rx_process_inv_peer(
    ol_txrx_pdev_handle pdev,
    void *rx_mpdu_desc,
    adf_nbuf_t msdu
    )
{
    a_uint8_t a1[IEEE80211_ADDR_LEN];
    htt_pdev_handle htt_pdev = pdev->htt_pdev;
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ieee80211_frame *wh;
    struct wdi_event_rx_peer_invalid_msg msg;

    wh = (struct ieee80211_frame *)
        htt_rx_mpdu_wifi_hdr_retrieve(htt_pdev, rx_mpdu_desc);
    /*
     * Klocwork issue #6152
     *  All targets that send a "INVALID_PEER" rx status provide a
     *  802.11 header for each rx MPDU, so it is certain that
     *  htt_rx_mpdu_wifi_hdr_retrieve will succeed.
     *  However, both for robustness, e.g. if this function is given a
     *  MSDU descriptor rather than a MPDU descriptor, and to make it
     *  clear to static analysis that this code is safe, add an explicit
     *  check that htt_rx_mpdu_wifi_hdr_retrieve provides a non-NULL value.
     */
    if (wh == NULL || !IEEE80211_IS_DATA(wh)) {
        return;
    }

    /* ignore frames for non-existent bssids */
    adf_os_mem_copy(a1, wh->i_addr1, IEEE80211_ADDR_LEN);
    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
        if (adf_os_mem_cmp(a1, vdev->mac_addr.raw, IEEE80211_ADDR_LEN) == 0) {
            break;
        }
    }
    if (!vdev) {
        return;
    }
    msg.wh = wh;
    msg.msdu = msdu;
    msg.vdev_id = vdev->vdev_id;
    #ifdef WDI_EVENT_ENABLE
    wdi_event_handler(WDI_EVENT_RX_PEER_INVALID, pdev, &msg);
    #endif
}

#ifdef QCA_SUPPORT_PEER_DATA_RX_RSSI
static inline int16_t
ol_rx_rssi_avg(struct ol_txrx_pdev_t *pdev, int16_t rssi_old, int16_t rssi_new)
{
    int rssi_old_weight;

    if (rssi_new == HTT_RSSI_INVALID) {
        return rssi_old;
    }
    if (rssi_old == HTT_RSSI_INVALID) {
        return rssi_new;
    }
    rssi_old_weight = (1 << pdev->rssi_update_shift) - pdev->rssi_new_weight;
    return (rssi_new * pdev->rssi_new_weight + rssi_old * rssi_old_weight) >>
        pdev->rssi_update_shift;
}

static void
OL_RX_IND_RSSI_UPDATE(
    struct ol_txrx_peer_t *peer,
    adf_nbuf_t rx_ind_msg)
{
    struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;
    peer->rssi_dbm = ol_rx_rssi_avg(
        pdev, peer->rssi_dbm,
        htt_rx_ind_rssi_dbm(pdev->htt_pdev, rx_ind_msg));
}

static void
OL_RX_MPDU_RSSI_UPDATE(
    struct ol_txrx_peer_t *peer,
    void *rx_mpdu_desc)
{
    struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;
    if (! peer) {
        return;
    }
    peer->rssi_dbm = ol_rx_rssi_avg(
        pdev, peer->rssi_dbm,
        htt_rx_mpdu_desc_rssi_dbm(pdev->htt_pdev, rx_mpdu_desc));
}

#else
#define OL_RX_IND_RSSI_UPDATE(peer, rx_ind_msg) /* no-op */
#define OL_RX_MPDU_RSSI_UPDATE(peer, rx_mpdu_desc) /* no-op */
#endif /* QCA_SUPPORT_PEER_DATA_RX_RSSI */

void
ol_rx_indication_handler(
    ol_txrx_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    u_int16_t peer_id,
    u_int8_t tid,
    int num_mpdu_ranges)
{
    int mpdu_range, i;
    unsigned seq_num_start = 0, seq_num_end = 0;
    a_bool_t rx_ind_release = A_FALSE;
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ol_txrx_peer_t *peer;
    htt_pdev_handle htt_pdev;
    uint16_t center_freq;
    uint16_t chan1;
    uint16_t chan2;
    uint8_t phymode;
    a_bool_t ret;

    htt_pdev = pdev->htt_pdev;
    peer = ol_txrx_peer_find_by_id(pdev, peer_id);
    if (!peer) {
        /* If we can't find a peer send this packet to OCB interface using
           OCB self peer */
        if (!ol_txrx_get_ocb_peer(pdev, &peer))
			peer = NULL;
    }

    if (peer) {
        vdev = peer->vdev;
        OL_RX_IND_RSSI_UPDATE(peer, rx_ind_msg);

        if (vdev->opmode == wlan_op_mode_ocb) {
            htt_rx_ind_legacy_rate(pdev->htt_pdev, rx_ind_msg,
                                   &peer->last_pkt_legacy_rate,
                                   &peer->last_pkt_legacy_rate_sel);
            peer->last_pkt_rssi_cmb = htt_rx_ind_rssi_dbm(pdev->htt_pdev,
                                                          rx_ind_msg);
            for (i = 0; i < 4; i++)
                peer->last_pkt_rssi[i] = htt_rx_ind_rssi_dbm_chain(
                    pdev->htt_pdev, rx_ind_msg, i);
            htt_rx_ind_timestamp(pdev->htt_pdev, rx_ind_msg,
                                 &peer->last_pkt_timestamp_microsec,
                                 &peer->last_pkt_timestamp_submicrosec);
            peer->last_pkt_tsf = htt_rx_ind_tsf32(pdev->htt_pdev, rx_ind_msg);
            peer->last_pkt_tid = htt_rx_ind_ext_tid(pdev->htt_pdev, rx_ind_msg);
        }
    }

    TXRX_STATS_INCR(pdev, priv.rx.normal.ppdus);

    OL_RX_REORDER_TIMEOUT_MUTEX_LOCK(pdev);

    if (htt_rx_ind_flush(pdev->htt_pdev, rx_ind_msg) && peer) {
        htt_rx_ind_flush_seq_num_range(
            pdev->htt_pdev, rx_ind_msg, &seq_num_start, &seq_num_end);
        if (tid == HTT_INVALID_TID) {
            /*
             * host/FW reorder state went out-of sync
             * for a while because FW ran out of Rx indication
             * buffer. We have to discard all the buffers in
             * reorder queue.
             */
            ol_rx_reorder_peer_cleanup(vdev, peer);
        } else {
            ol_rx_reorder_flush(
                vdev, peer, tid, seq_num_start,
                seq_num_end, htt_rx_flush_release);
        }
    }

    if (htt_rx_ind_release(pdev->htt_pdev, rx_ind_msg)) {
        /* the ind info of release is saved here and do release at the end.
         * This is for the reason of in HL case, the adf_nbuf_t for msg and
         * payload are the same buf. And the buf will be changed during
         * processing */
        rx_ind_release = A_TRUE;
        htt_rx_ind_release_seq_num_range(
            pdev->htt_pdev, rx_ind_msg, &seq_num_start, &seq_num_end);
    }

#ifdef DEBUG_DMA_DONE
    pdev->htt_pdev->rx_ring.dbg_initial_msdu_payld =
        pdev->htt_pdev->rx_ring.sw_rd_idx.msdu_payld;
#endif

    for (mpdu_range = 0; mpdu_range < num_mpdu_ranges; mpdu_range++) {
        enum htt_rx_status status;
        int i, num_mpdus;
        adf_nbuf_t head_msdu, tail_msdu, msdu;
        void *rx_mpdu_desc;

#ifdef DEBUG_DMA_DONE
        pdev->htt_pdev->rx_ring.dbg_mpdu_range = mpdu_range;
#endif

        htt_rx_ind_mpdu_range_info(
            pdev->htt_pdev, rx_ind_msg, mpdu_range, &status, &num_mpdus);
        if ((status == htt_rx_status_ok) && peer) {
            TXRX_STATS_ADD(pdev, priv.rx.normal.mpdus, num_mpdus);
            /* valid frame - deposit it into the rx reordering buffer */
            for (i = 0; i < num_mpdus; i++) {
                int msdu_chaining;
                /*
                 * Get a linked list of the MSDUs that comprise this MPDU.
                 * This also attaches each rx MSDU descriptor to the
                 * corresponding rx MSDU network buffer.
                 * (In some systems, the rx MSDU desc is already in the
                 * same buffer as the MSDU payload; in other systems they
                 * are separate, so a pointer needs to be set in the netbuf
                 * to locate the corresponding rx descriptor.)
                 *
                 * It is neccessary to call htt_rx_amsdu_pop before
                 * htt_rx_mpdu_desc_list_next, because the (MPDU) rx
                 * descriptor has the DMA unmapping done during the
                 * htt_rx_amsdu_pop call.  The rx desc should not be
                 * accessed until this DMA unmapping has been done,
                 * since the DMA unmapping involves making sure the
                 * cache area for the mapped buffer is flushed, so the
                 * data written by the MAC DMA into memory will be
                 * fetched, rather than garbage from the cache.
                 */

#ifdef DEBUG_DMA_DONE
                pdev->htt_pdev->rx_ring.dbg_mpdu_count = i;
#endif

                msdu_chaining = htt_rx_amsdu_pop(
                    htt_pdev, rx_ind_msg, &head_msdu, &tail_msdu);
#ifdef HTT_RX_RESTORE
                if (htt_pdev->rx_ring.rx_reset) {
                    ol_rx_trigger_restore(htt_pdev, head_msdu, tail_msdu);
                    return;
                }
#endif
                rx_mpdu_desc =
                    htt_rx_mpdu_desc_list_next(htt_pdev, rx_ind_msg);
                ret = htt_rx_msdu_center_freq(htt_pdev, peer, rx_mpdu_desc,
                                              &center_freq, &chan1, &chan2, &phymode);
                if (ret == A_TRUE) {
                        peer->last_pkt_center_freq = center_freq;
                } else {
                        peer->last_pkt_center_freq = 0;
                }

                /* Pktlog */
    #ifdef WDI_EVENT_ENABLE
                wdi_event_handler(WDI_EVENT_RX_DESC_REMOTE, pdev, head_msdu);
    #endif

                if (msdu_chaining) {
                    /*
                     * TBDXXX - to deliver SDU with chaining, we need to
                     * stitch those scattered buffers into one single buffer.
                     * Just discard it now.
                     */
                    while (1) {
                        adf_nbuf_t next;
                        next = adf_nbuf_next(head_msdu);
                        htt_rx_desc_frame_free(htt_pdev, head_msdu);
                        if (head_msdu == tail_msdu) {
                            break;
                        }
                        head_msdu = next;
                    }
                } else {
                    enum htt_rx_status mpdu_status;
                    int reorder_idx;
                    reorder_idx =
                        htt_rx_mpdu_desc_reorder_idx(htt_pdev, rx_mpdu_desc);
                    OL_RX_REORDER_TRACE_ADD(
                        pdev, tid, reorder_idx,
                        htt_rx_mpdu_desc_seq_num(htt_pdev, rx_mpdu_desc), 1);
                    OL_RX_MPDU_RSSI_UPDATE(peer, rx_mpdu_desc);
                    /*
                     * In most cases, out-of-bounds and duplicate sequence
                     * number detection is performed by the target, but in
                     * some cases it is done by the host.
                     * Specifically, the host does rx out-of-bounds sequence
                     * number detection for:
                     * 1.  Peregrine or Rome target for peer-TIDs that do
                     *     not have aggregation enabled, if the
                     *     RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK flag
                     *     is set during the driver build.
                     * 2.  Riva-family targets, which have rx reorder timeouts
                     *     handled by the host rather than the target.
                     *     (The target already does duplicate detection, but
                     *     the host may have given up waiting for a particular
                     *     sequence number before it arrives.  In this case,
                     *     the out-of-bounds sequence number of the late frame
                     *     allows the host to discard it, rather than sending
                     *     it out of order.
                     */
                    mpdu_status = OL_RX_SEQ_NUM_CHECK(
                        pdev, peer, tid, rx_mpdu_desc);

                    if (mpdu_status != htt_rx_status_ok) {
                        /*
                         * If the sequence number was out of bounds,
                         * the MPDU needs to be discarded.
                         */
                        while (1) {
                            adf_nbuf_t next;
                            next = adf_nbuf_next(head_msdu);
                            htt_rx_desc_frame_free(htt_pdev, head_msdu);
                            if (head_msdu == tail_msdu) {
                                break;
                            }
                            head_msdu = next;
                        }

                        /*
                         * For Peregrine and Rome,
                         * OL_RX_REORDER_SEQ_NUM_CHECK should only fail for
                         * the case of (duplicate) non-aggregates.
                         *
                         * For Riva, Pronto, and Northstar,
                         * there should be only one MPDU delivered at a time.
                         * Thus, there are no further MPDUs that need to be
                         * processed here.
                         * Just to be sure this is true, check the assumption
                         * that this was the only MPDU referenced by the rx
                         * indication.
                         */
                        TXRX_ASSERT2(num_mpdu_ranges == 1 && num_mpdus == 1);

                        /*
                         * The MPDU was not stored in the rx reorder array,
                         * so there's nothing to release.
                         */
                        rx_ind_release = A_FALSE;
                    } else {
                        ol_rx_reorder_store(
                            pdev, peer, tid, reorder_idx, head_msdu, tail_msdu);
                        if (peer->tids_rx_reorder[tid].win_sz_mask == 0) {
                            peer->tids_last_seq[tid] =
                                         htt_rx_mpdu_desc_seq_num(htt_pdev,
                                                                  rx_mpdu_desc);
                        }
                    }
                }
            }
        } else {
            /* invalid frames - discard them */
            OL_RX_REORDER_TRACE_ADD(
                pdev, tid, TXRX_SEQ_NUM_ERR(status),
                TXRX_SEQ_NUM_ERR(status), num_mpdus);
            TXRX_STATS_ADD(pdev, priv.rx.err.mpdu_bad, num_mpdus);
            for (i = 0; i < num_mpdus; i++) {
                /* pull the MPDU's MSDUs off the buffer queue */
                htt_rx_amsdu_pop(htt_pdev, rx_ind_msg, &msdu, &tail_msdu);
#ifdef HTT_RX_RESTORE
                if (htt_pdev->rx_ring.rx_reset) {
                    ol_rx_trigger_restore(htt_pdev, msdu, tail_msdu);
                    return;
                }
#endif
                /* pull the MPDU desc off the desc queue */
                rx_mpdu_desc =
                    htt_rx_mpdu_desc_list_next(htt_pdev, rx_ind_msg);
                OL_RX_ERR_STATISTICS_2(
                    pdev, vdev, peer, rx_mpdu_desc, msdu, status);

                if (status == htt_rx_status_tkip_mic_err &&
                    vdev != NULL &&  peer != NULL)
                {
                    union htt_rx_pn_t pn;
                    u_int8_t key_id;
                    htt_rx_mpdu_desc_pn(pdev->htt_pdev,
                                 htt_rx_msdu_desc_retrieve(pdev->htt_pdev,
                                                           msdu), &pn, 48);
                    if (htt_rx_msdu_desc_key_id(pdev->htt_pdev,
                                      htt_rx_msdu_desc_retrieve(pdev->htt_pdev,
                                      msdu), &key_id) == A_TRUE) {
                        ol_rx_err(pdev->ctrl_pdev, vdev->vdev_id,
                                  peer->mac_addr.raw, tid, 0,
                                  OL_RX_ERR_TKIP_MIC, msdu, &pn.pn48, key_id);
                    }
                }

    #ifdef WDI_EVENT_ENABLE
                if (status != htt_rx_status_ctrl_mgmt_null) {
                    /* Pktlog */
                    wdi_event_handler(WDI_EVENT_RX_DESC_REMOTE, pdev, msdu);
                }
    #endif
                if (status == htt_rx_status_err_inv_peer) {
                    /* once per mpdu */
                    ol_rx_process_inv_peer(pdev, rx_mpdu_desc, msdu);
                }
		while (1) {
			 /* Free the nbuf */
			adf_nbuf_t next;
			next = adf_nbuf_next(msdu);
			htt_rx_desc_frame_free(htt_pdev, msdu);
			if (msdu == tail_msdu) {
				break;
			}
			msdu = next;
		}
            }
        }
    }
    /*
     * Now that a whole batch of MSDUs have been pulled out of HTT
     * and put into the rx reorder array, it is an appropriate time
     * to request HTT to provide new rx MSDU buffers for the target
     * to fill.
     * This could be done after the end of this function, but it's
     * better to do it now, rather than waiting until after the driver
     * and OS finish processing the batch of rx MSDUs.
     */
    htt_rx_msdu_buff_replenish(htt_pdev);

    if ((A_TRUE == rx_ind_release) && peer && vdev) {
        ol_rx_reorder_release(vdev, peer, tid, seq_num_start, seq_num_end);
    }
    OL_RX_REORDER_TIMEOUT_UPDATE(peer, tid);
    OL_RX_REORDER_TIMEOUT_MUTEX_UNLOCK(pdev);

    if (pdev->rx.flags.defrag_timeout_check) {
        ol_rx_defrag_waitlist_flush(pdev);
    }
}

void
ol_rx_sec_ind_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_id,
    enum htt_sec_type sec_type,
    int is_unicast,
    u_int32_t *michael_key,
    u_int32_t *rx_pn)
{
    struct ol_txrx_peer_t *peer;
    int sec_index, i;

    peer = ol_txrx_peer_find_by_id(pdev, peer_id);
    if (! peer) {
        TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
            "Couldn't find peer from ID %d - skipping security inits\n",
            peer_id);
        return;
    }
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
        "sec spec for peer %p (%02x:%02x:%02x:%02x:%02x:%02x): "
        "%s key of type %d\n",
        peer,
        peer->mac_addr.raw[0], peer->mac_addr.raw[1], peer->mac_addr.raw[2],
        peer->mac_addr.raw[3], peer->mac_addr.raw[4], peer->mac_addr.raw[5],
        is_unicast ? "ucast" : "mcast",
        sec_type);
    sec_index = is_unicast ? txrx_sec_ucast : txrx_sec_mcast;
    peer->security[sec_index].sec_type = sec_type;
    /* michael key only valid for TKIP, but for simplicity, copy it anyway */
    adf_os_mem_copy(
        &peer->security[sec_index].michael_key[0],
        michael_key,
        sizeof(peer->security[sec_index].michael_key));

    if (sec_type != htt_sec_type_wapi) {
        adf_os_mem_set(peer->tids_last_pn_valid, 0x00, OL_TXRX_NUM_EXT_TIDS);
    } else if (sec_index == txrx_sec_mcast || peer->tids_last_pn_valid[0]) {
        for (i = 0; i < OL_TXRX_NUM_EXT_TIDS; i++) {
            /*
             * Setting PN valid bit for WAPI sec_type,
             * since WAPI PN has to be started with predefined value
             */
            peer->tids_last_pn_valid[i] = 1;
            adf_os_mem_copy(
                (u_int8_t *) &peer->tids_last_pn[i],
                (u_int8_t *) rx_pn, sizeof(union htt_rx_pn_t));
            peer->tids_last_pn[i].pn128[1] =
                adf_os_cpu_to_le64(peer->tids_last_pn[i].pn128[1]);
            peer->tids_last_pn[i].pn128[0] =
                adf_os_cpu_to_le64(peer->tids_last_pn[i].pn128[0]);
        }
    }
}

#if defined(PERE_IP_HDR_ALIGNMENT_WAR)

#include <ieee80211_common.h>

static void
transcap_nwifi_to_8023(adf_nbuf_t msdu)
{
    struct ieee80211_frame *wh;
    a_uint32_t hdrsize;
    struct llc *llchdr;
    struct ether_header *eth_hdr;
    a_uint16_t ether_type = 0;
    a_uint8_t a1[IEEE80211_ADDR_LEN];
    a_uint8_t a2[IEEE80211_ADDR_LEN];
    a_uint8_t a3[IEEE80211_ADDR_LEN];
    a_uint8_t fc1;

    wh = (struct ieee80211_frame *)adf_nbuf_data(msdu);
    adf_os_mem_copy(a1, wh->i_addr1, IEEE80211_ADDR_LEN);
    adf_os_mem_copy(a2, wh->i_addr2, IEEE80211_ADDR_LEN);
    adf_os_mem_copy(a3, wh->i_addr3, IEEE80211_ADDR_LEN);
    fc1 = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
    /* Native Wifi header is 80211 non-QoS header */
    hdrsize = sizeof(struct ieee80211_frame);

    llchdr = (struct llc *)(((a_uint8_t *)adf_nbuf_data(msdu)) + hdrsize);
    ether_type = llchdr->llc_un.type_snap.ether_type;

    /*
     * Now move the data pointer to the beginning of the mac header :
     * new-header = old-hdr + (wifhdrsize + llchdrsize - ethhdrsize)
     */
    adf_nbuf_pull_head(
        msdu, (hdrsize + sizeof(struct llc) - sizeof(struct ether_header)));
    eth_hdr = (struct ether_header *)(adf_nbuf_data(msdu));
    switch (fc1) {
    case IEEE80211_FC1_DIR_NODS:
        adf_os_mem_copy(eth_hdr->ether_dhost, a1, IEEE80211_ADDR_LEN);
        adf_os_mem_copy(eth_hdr->ether_shost, a2, IEEE80211_ADDR_LEN);
        break;
    case IEEE80211_FC1_DIR_TODS:
        adf_os_mem_copy(eth_hdr->ether_dhost, a3, IEEE80211_ADDR_LEN);
        adf_os_mem_copy(eth_hdr->ether_shost, a2, IEEE80211_ADDR_LEN);
        break;
    case IEEE80211_FC1_DIR_FROMDS:
        adf_os_mem_copy(eth_hdr->ether_dhost, a1, IEEE80211_ADDR_LEN);
        adf_os_mem_copy(eth_hdr->ether_shost, a3, IEEE80211_ADDR_LEN);
        break;
    case IEEE80211_FC1_DIR_DSTODS:
        break;
    }
    eth_hdr->ether_type = ether_type;
}
#endif

void ol_rx_notify(ol_pdev_handle pdev,
		  u_int8_t vdev_id,
		  u_int8_t *peer_mac_addr,
		  int tid,
		  u_int32_t tsf32,
		  enum ol_rx_notify_type notify_type,
		  adf_nbuf_t rx_frame)
{
	/*
	 * NOTE: This is used in qca_main for AP mode to handle IGMP
	 * packets specially. Umac has a corresponding handler for this
         * not sure if we need to have this for CLD as well.
	 */
}

/**
 * @brief Look into a rx MSDU to see what kind of special handling it requires
 * @details
 *      This function is called when the host rx SW sees that the target
 *      rx FW has marked a rx MSDU as needing inspection.
 *      Based on the results of the inspection, the host rx SW will infer
 *      what special handling to perform on the rx frame.
 *      Currently, the only type of frames that require special handling
 *      are IGMP frames.  The rx data-path SW checks if the frame is IGMP
 *      (it should be, since the target would not have set the inspect flag
 *      otherwise), and then calls the ol_rx_notify function so the
 *      control-path SW can perform multicast group membership learning
 *      by sniffing the IGMP frame.
 */
#define SIZEOF_80211_HDR (sizeof(struct ieee80211_frame))
void
ol_rx_inspect(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    adf_nbuf_t msdu,
    void *rx_desc)
{
    ol_txrx_pdev_handle pdev = vdev->pdev;
    u_int8_t *data, *l3_hdr;
    u_int16_t ethertype;
    int offset;

    data = adf_nbuf_data(msdu);
    if (pdev->frame_format == wlan_frm_fmt_native_wifi) {
        offset = SIZEOF_80211_HDR + LLC_SNAP_HDR_OFFSET_ETHERTYPE;
        l3_hdr = data + SIZEOF_80211_HDR + LLC_SNAP_HDR_LEN;
    } else {
        offset = ETHERNET_ADDR_LEN * 2;
        l3_hdr = data + ETHERNET_HDR_LEN;
    }
    ethertype = (data[offset] << 8) | data[offset+1];
    if (ethertype == ETHERTYPE_IPV4) {
        offset = IPV4_HDR_OFFSET_PROTOCOL;
        if (l3_hdr[offset] == IP_PROTOCOL_IGMP) {
            ol_rx_notify(
                pdev->ctrl_pdev,
                vdev->vdev_id,
                peer->mac_addr.raw,
                tid,
                htt_rx_mpdu_desc_tsf32(pdev->htt_pdev, rx_desc),
                OL_RX_NOTIFY_IPV4_IGMP,
                msdu);
        }
    }
}

void
ol_rx_offload_deliver_ind_handler(
    ol_txrx_pdev_handle pdev,
    adf_nbuf_t msg,
    int msdu_cnt)
{
    int vdev_id, peer_id, tid;
    adf_nbuf_t head_buf, tail_buf, buf;
    struct ol_txrx_peer_t *peer;
    struct ol_txrx_vdev_t *vdev = NULL;
    u_int8_t fw_desc;
    htt_pdev_handle htt_pdev = pdev->htt_pdev;

    while (msdu_cnt) {
        htt_rx_offload_msdu_pop(
            htt_pdev, msg, &vdev_id, &peer_id,
            &tid, &fw_desc, &head_buf, &tail_buf);

        peer = ol_txrx_peer_find_by_id(pdev, peer_id);
        if (peer && peer->vdev) {
            vdev = peer->vdev;
	    OL_RX_OSIF_DELIVER(vdev, peer, head_buf);
        } else {
            buf = head_buf;
            while (1) {
                adf_nbuf_t next;
                next = adf_nbuf_next(buf);
                htt_rx_desc_frame_free(htt_pdev, buf);
                if (buf == tail_buf) {
                    break;
                }
                buf = next;
            }
        }
        msdu_cnt--;
    }
    htt_rx_msdu_buff_replenish(htt_pdev);
}

/**
 * @brief Check the first msdu to decide whether the a-msdu should be accepted.
 */
a_bool_t
ol_rx_filter(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    adf_nbuf_t msdu,
    void *rx_desc)
{
    #define FILTER_STATUS_REJECT 1
    #define FILTER_STATUS_ACCEPT 0
    u_int8_t *wh;
    u_int32_t offset = 0;
    u_int16_t ether_type = 0;
    a_bool_t is_encrypted = A_FALSE, is_mcast = A_FALSE;
    u_int8_t i;
    privacy_filter_packet_type packet_type = PRIVACY_FILTER_PACKET_UNICAST;
    ol_txrx_pdev_handle pdev = vdev->pdev;
    htt_pdev_handle htt_pdev = pdev->htt_pdev;
    int sec_idx;

    /*
     * Safemode must avoid the PrivacyExemptionList and
     * ExcludeUnencrypted checking
     */
    if (vdev->safemode) {
        return FILTER_STATUS_ACCEPT;
    }

    is_mcast = htt_rx_msdu_is_wlan_mcast(htt_pdev, rx_desc);
    if (vdev->num_filters > 0) {
        if (pdev->frame_format == wlan_frm_fmt_native_wifi) {
            offset = SIZEOF_80211_HDR + LLC_SNAP_HDR_OFFSET_ETHERTYPE;
        } else {
            offset = ETHERNET_ADDR_LEN * 2;
        }
        /* get header info from msdu */
        wh = adf_nbuf_data(msdu);

        /* get ether type */
        ether_type = (wh[offset] << 8) | wh[offset+1];
        /* get packet type */
        if (A_TRUE == is_mcast) {
            packet_type = PRIVACY_FILTER_PACKET_MULTICAST;
        } else {
            packet_type = PRIVACY_FILTER_PACKET_UNICAST;
        }
    }
    /* get encrypt info */
    is_encrypted = htt_rx_mpdu_is_encrypted(htt_pdev, rx_desc);
#ifdef ATH_SUPPORT_WAPI
    if ((A_TRUE == is_encrypted) && ( ETHERTYPE_WAI == ether_type ))
    {
        /* We expect the WAI frames to be always unencrypted when the UMAC
         * gets it.*/
        return FILTER_STATUS_REJECT;
    }
#endif //ATH_SUPPORT_WAPI

    for (i = 0; i < vdev->num_filters; i++) {
        privacy_filter filter_type;
        privacy_filter_packet_type filter_packet_type;

        /* skip if the ether type does not match */
        if (vdev->privacy_filters[i].ether_type != ether_type) {
            continue;
        }

        /* skip if the packet type does not match */
        filter_packet_type = vdev->privacy_filters[i].packet_type;
        if (filter_packet_type != packet_type &&
            filter_packet_type != PRIVACY_FILTER_PACKET_BOTH)
        {
            continue;
        }


        filter_type = vdev->privacy_filters[i].filter_type;
        if (filter_type == PRIVACY_FILTER_ALWAYS) {
            /*
             * In this case, we accept the frame if and only if it was
             * originally NOT encrypted.
             */
            if (A_TRUE == is_encrypted) {
                return FILTER_STATUS_REJECT;
            } else {
                return FILTER_STATUS_ACCEPT;
            }
        } else if (filter_type == PRIVACY_FILTER_KEY_UNAVAILABLE) {
            /*
             * In this case, we reject the frame if it was originally NOT
             * encrypted but we have the key mapping key for this frame.
             */
            if (!is_encrypted && !is_mcast &&
                peer->security[txrx_sec_ucast].sec_type != htt_sec_type_none &&
                (peer->keyinstalled || !ETHERTYPE_IS_EAPOL_WAPI(ether_type)))
            {
                 return FILTER_STATUS_REJECT;
            } else {
                return FILTER_STATUS_ACCEPT;
            }
        } else {
            /*
             * The privacy exemption does not apply to this frame.
             */
            break;
        }
    }

    /*
     * If the privacy exemption list does not apply to the frame,
     * check ExcludeUnencrypted.
     * If ExcludeUnencrypted is not set, or if this was oringially
     * an encrypted frame, it will be accepted.
     */
    if (!vdev->drop_unenc || (A_TRUE == is_encrypted)) {
        return FILTER_STATUS_ACCEPT;
    }

    /*
     *  If this is a open connection, it will be accepted.
     */
    sec_idx = (A_TRUE == is_mcast) ? txrx_sec_mcast : txrx_sec_ucast;
    if (peer->security[sec_idx].sec_type == htt_sec_type_none) {
        return FILTER_STATUS_ACCEPT;
    }
    if ((A_FALSE == is_encrypted) && vdev->drop_unenc) {
        OL_RX_ERR_STATISTICS(
            pdev, vdev, OL_RX_ERR_PRIVACY,
            pdev->sec_types[htt_sec_type_none], is_mcast);
    }
    return FILTER_STATUS_REJECT;
}

void
ol_rx_deliver(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    adf_nbuf_t msdu_list)
{
    ol_txrx_pdev_handle pdev = vdev->pdev;
    htt_pdev_handle htt_pdev = pdev->htt_pdev;
    adf_nbuf_t deliver_list_head = NULL;
    adf_nbuf_t deliver_list_tail = NULL;
    adf_nbuf_t msdu;
    a_bool_t filter = A_FALSE;
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
    struct ol_rx_decap_info_t info;
    adf_os_mem_set(&info, 0, sizeof(info));
#endif

    msdu = msdu_list;
    /*
     * Check each MSDU to see whether it requires special handling,
     * and free each MSDU's rx descriptor
     */
    while (msdu) {
        void *rx_desc;
        int discard, inspect, dummy_fwd;
        adf_nbuf_t next = adf_nbuf_next(msdu);

        rx_desc = htt_rx_msdu_desc_retrieve(pdev->htt_pdev, msdu);
        // for HL, point to payload right now
        if (pdev->cfg.is_high_latency) {
            adf_nbuf_pull_head(msdu,
                    htt_rx_msdu_rx_desc_size_hl(htt_pdev,
                        rx_desc));
        }

#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
        info.is_msdu_cmpl_mpdu =
            htt_rx_msdu_desc_completes_mpdu(htt_pdev, rx_desc);
        info.is_first_subfrm = htt_rx_msdu_first_msdu_flag(htt_pdev, rx_desc);
        if (OL_RX_DECAP(vdev, peer, msdu, &info) != A_OK) {
            discard = 1;
            TXRX_PRINT(TXRX_PRINT_LEVEL_WARN,
                "decap error %p from peer %p "
                "(%02x:%02x:%02x:%02x:%02x:%02x) len %d\n",
                 msdu, peer,
                 peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                 peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                 peer->mac_addr.raw[4], peer->mac_addr.raw[5],
                 adf_nbuf_len(msdu));
            goto DONE;
        }
#endif
        htt_rx_msdu_actions(
            pdev->htt_pdev, rx_desc, &discard, &dummy_fwd, &inspect);
        if (inspect) {
            ol_rx_inspect(vdev, peer, tid, msdu, rx_desc);
        }

        /*
         * Check the first msdu in the mpdu, if it will be filtered out,
         * then discard the entire mpdu.
         */
        if (htt_rx_msdu_first_msdu_flag(htt_pdev, rx_desc)) {
            filter = ol_rx_filter(vdev, peer, msdu, rx_desc);
        }

#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
DONE:
#endif
        htt_rx_msdu_desc_free(htt_pdev, msdu);
        if (discard || (A_TRUE == filter)) {
            OL_TXRX_FRMS_DUMP(
                "rx discarding:",
                pdev, deliver_list_head,
                ol_txrx_frm_dump_tcp_seq | ol_txrx_frm_dump_contents,
                0 /* don't print contents */);
            adf_nbuf_free(msdu);
            /* If discarding packet is last packet of the delivery list,NULL terminator should be added for delivery list. */
            if (next == NULL && deliver_list_head){
                adf_nbuf_set_next(deliver_list_tail, NULL); /* add NULL terminator */
            }
        } else {
            /* If this is for OCB, then prepend the RX stats header. */
            if (vdev->opmode == wlan_op_mode_ocb) {
                int i;
                struct ol_txrx_ocb_chan_info *chan_info = 0;
                int packet_freq = peer->last_pkt_center_freq;
                for (i = 0; i < vdev->ocb_channel_count; i++) {
                    if (vdev->ocb_channel_info[i].chan_freq == packet_freq) {
                        chan_info = &vdev->ocb_channel_info[i];
                        break;
                    }
                }
                if (!chan_info || !chan_info->disable_rx_stats_hdr) {
                    struct ether_header eth_header = { {0} };
                    struct ocb_rx_stats_hdr_t rx_header = {0};

                    /*
                     * Construct the RX stats header and push that to the front
                     * of the packet.
                     */
                    rx_header.version = 1;
                    rx_header.length = sizeof(rx_header);
                    rx_header.channel_freq = peer->last_pkt_center_freq;
                    rx_header.rssi_cmb = peer->last_pkt_rssi_cmb;
                    adf_os_mem_copy(rx_header.rssi, peer->last_pkt_rssi,
                                    sizeof(rx_header.rssi));
                    if (peer->last_pkt_legacy_rate_sel == 0) {
                        switch (peer->last_pkt_legacy_rate) {
                        case 0x8:
                            rx_header.datarate = 6;
                            break;
                        case 0x9:
                            rx_header.datarate = 4;
                            break;
                        case 0xA:
                            rx_header.datarate = 2;
                            break;
                        case 0xB:
                            rx_header.datarate = 0;
                            break;
                        case 0xC:
                            rx_header.datarate = 7;
                            break;
                        case 0xD:
                            rx_header.datarate = 5;
                            break;
                        case 0xE:
                            rx_header.datarate = 3;
                            break;
                        case 0xF:
                            rx_header.datarate = 1;
                            break;
                        default:
                            rx_header.datarate = 0xFF;
                            break;
                        }
                    } else {
                        rx_header.datarate = 0xFF;
                    }

                    rx_header.timestamp_microsec =
                        peer->last_pkt_timestamp_microsec;
                    rx_header.timestamp_submicrosec =
                        peer->last_pkt_timestamp_submicrosec;
                    rx_header.tsf32 = peer->last_pkt_tsf;
                    rx_header.ext_tid = peer->last_pkt_tid;

                    adf_nbuf_push_head(msdu, sizeof(rx_header));
                    adf_os_mem_copy(adf_nbuf_data(msdu), &rx_header,
                                    sizeof(rx_header));

                    /* Construct the ethernet header with type 0x8152 and push
                       that to the front of the packet to indicate the RX stats
                       header. */
                    eth_header.ether_type = adf_os_htons(ETHERTYPE_OCB_RX);
                    adf_nbuf_push_head(msdu, sizeof(eth_header));
                    adf_os_mem_copy(adf_nbuf_data(msdu), &eth_header,
                                    sizeof(eth_header));
                }
            }
            OL_RX_PEER_STATS_UPDATE(peer, msdu);
            OL_RX_ERR_STATISTICS_1(pdev, vdev, peer, rx_desc, OL_RX_ERR_NONE);
            TXRX_STATS_MSDU_INCR(vdev->pdev, rx.delivered, msdu);

            OL_TXRX_LIST_APPEND(deliver_list_head, deliver_list_tail, msdu);
        }
        msdu = next;
    }
    /* sanity check - are there any frames left to give to the OS shim? */
    if (!deliver_list_head) {
        return;
    }

#if defined(PERE_IP_HDR_ALIGNMENT_WAR)
    if (pdev->host_80211_enable) {
        for (msdu = deliver_list_head; msdu; msdu = adf_nbuf_next(msdu)) {
            transcap_nwifi_to_8023(msdu);
        }
    }
#endif

    OL_TXRX_FRMS_DUMP(
        "rx delivering:",
        pdev, deliver_list_head,
        ol_txrx_frm_dump_tcp_seq | ol_txrx_frm_dump_contents,
        0 /* don't print contents */);

    OL_RX_OSIF_DELIVER(vdev, peer, deliver_list_head);
}

void
ol_rx_discard(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    adf_nbuf_t msdu_list)
{
    ol_txrx_pdev_handle pdev = vdev->pdev;
    htt_pdev_handle htt_pdev = pdev->htt_pdev;

    while (msdu_list) {
        adf_nbuf_t msdu = msdu_list;

        msdu_list = adf_nbuf_next(msdu_list);
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
            "discard rx %p from partly-deleted peer %p "
            "(%02x:%02x:%02x:%02x:%02x:%02x)\n",
            msdu, peer,
            peer->mac_addr.raw[0], peer->mac_addr.raw[1],
            peer->mac_addr.raw[2], peer->mac_addr.raw[3],
            peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
        htt_rx_desc_frame_free(htt_pdev, msdu);
    }
}

void
ol_rx_peer_init(struct ol_txrx_pdev_t *pdev, struct ol_txrx_peer_t *peer)
{
    u_int8_t tid;
    for (tid = 0; tid < OL_TXRX_NUM_EXT_TIDS; tid++) {
        ol_rx_reorder_init(&peer->tids_rx_reorder[tid], tid);

        /* invalid sequence number */
        peer->tids_last_seq[tid] = IEEE80211_SEQ_MAX;
    }
    /*
     * Set security defaults: no PN check, no security.
     * The target may send a HTT SEC_IND message to overwrite these defaults.
     */
    peer->security[txrx_sec_ucast].sec_type =
        peer->security[txrx_sec_mcast].sec_type = htt_sec_type_none;
    peer->keyinstalled = 0;
    adf_os_atomic_init(&peer->fw_pn_check);
}

void
ol_rx_peer_cleanup(struct ol_txrx_vdev_t *vdev, struct ol_txrx_peer_t *peer)
{
    peer->keyinstalled = 0;
    ol_rx_reorder_peer_cleanup(vdev, peer);
}

/*
 * Free frames including both rx descriptors and buffers
 */
void
ol_rx_frames_free(
    htt_pdev_handle htt_pdev,
    adf_nbuf_t frames)
{
    adf_nbuf_t next, frag = frames;

    while (frag) {
        next = adf_nbuf_next(frag);
        htt_rx_desc_frame_free(htt_pdev, frag);
        frag = next;
    }
}

void
ol_rx_in_order_indication_handler(
    ol_txrx_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    u_int16_t peer_id,
    u_int8_t tid,
    u_int8_t is_offload )
{
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ol_txrx_peer_t *peer = NULL;
    htt_pdev_handle htt_pdev = NULL;
    int status;
    adf_nbuf_t head_msdu, tail_msdu = NULL;

    if (pdev) {
        peer = ol_txrx_peer_find_by_id(pdev, peer_id);
        htt_pdev = pdev->htt_pdev;
    } else {
        TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                   "%s: Invalid pdev passed!\n", __FUNCTION__);
        adf_os_assert_always(pdev);
        return;
    }

    /*
     * Get a linked list of the MSDUs in the rx in order indication.
     * This also attaches each rx MSDU descriptor to the
     * corresponding rx MSDU network buffer.
     */
    status = htt_rx_amsdu_pop(htt_pdev, rx_ind_msg, &head_msdu, &tail_msdu);
    if (adf_os_unlikely(0 == status)) {
        TXRX_PRINT(TXRX_PRINT_LEVEL_WARN,
                    "%s: Pop status is 0, returning here\n", __FUNCTION__);
        return;
    }

    /* Replenish the rx buffer ring first to provide buffers to the target
       rather than waiting for the indeterminate time taken by the OS to consume
       the rx frames */
    htt_rx_msdu_buff_replenish(htt_pdev);

    /* Send the chain of MSDUs to the OS */
    /* rx_opt_proc takes a NULL-terminated list of msdu netbufs */
    adf_nbuf_set_next(tail_msdu, NULL);

    /* Pktlog */
#ifdef WDI_EVENT_ENABLE
     wdi_event_handler(WDI_EVENT_RX_DESC_REMOTE, pdev, head_msdu);
#endif

    /* if this is an offload indication, peer id is carried in the rx buffer */
    if (peer) {
        vdev = peer->vdev;
    } else {
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2,
                   "%s: Couldn't find peer from ID 0x%x\n", __FUNCTION__,
                   peer_id);
        while (head_msdu) {
            adf_nbuf_t msdu = head_msdu;

            head_msdu = adf_nbuf_next(head_msdu);
            htt_rx_desc_frame_free(htt_pdev, msdu);
        }
        return;
    }

    peer->rx_opt_proc(vdev, peer, tid, head_msdu);
}

/* the msdu_list passed here must be NULL terminated */
void
ol_rx_in_order_deliver(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    adf_nbuf_t msdu_list)
{
    adf_nbuf_t msdu;

    msdu = msdu_list;
    /*
     * Currently, this does not check each MSDU to see whether it requires
     * special handling. MSDUs that need special handling (example: IGMP frames)
     * should be sent via a seperate HTT message. Also, this does not do rx->tx
     * forwarding or filtering.
     */

    while (msdu) {
        adf_nbuf_t next = adf_nbuf_next(msdu);

        OL_RX_PEER_STATS_UPDATE(peer, msdu);
        OL_RX_ERR_STATISTICS_1(vdev->pdev, vdev, peer, rx_desc, OL_RX_ERR_NONE);
        TXRX_STATS_MSDU_INCR(vdev->pdev, rx.delivered, msdu);

        msdu = next;
    }

    OL_TXRX_FRMS_DUMP(
        "rx delivering:",
        pdev, deliver_list_head,
        ol_txrx_frm_dump_tcp_seq | ol_txrx_frm_dump_contents,
        0 /* don't print contents */);

    OL_RX_OSIF_DELIVER(vdev, peer, msdu_list);
}

void
ol_rx_offload_paddr_deliver_ind_handler(
   htt_pdev_handle htt_pdev,
   u_int32_t msdu_count,
   u_int32_t * msg_word )
{
    int vdev_id, peer_id, tid;
    adf_nbuf_t head_buf, tail_buf, buf;
    struct ol_txrx_peer_t *peer;
    struct ol_txrx_vdev_t *vdev = NULL;
    u_int8_t fw_desc;
    int msdu_iter = 0;

    while (msdu_count) {
        htt_rx_offload_paddr_msdu_pop_ll(
            htt_pdev, msg_word, msdu_iter, &vdev_id,
             &peer_id, &tid, &fw_desc, &head_buf, &tail_buf);

        peer = ol_txrx_peer_find_by_id(htt_pdev->txrx_pdev, peer_id);
        if (peer && peer->vdev) {
            vdev = peer->vdev;
            OL_RX_OSIF_DELIVER(vdev, peer, head_buf);
        } else {
            buf = head_buf;
            while (1) {
                adf_nbuf_t next;
                next = adf_nbuf_next(buf);
                htt_rx_desc_frame_free(htt_pdev, buf);
                if (buf == tail_buf) {
                    break;
                }
                buf = next;
            }
        }
        msdu_iter++;
        msdu_count--;
    }
    htt_rx_msdu_buff_replenish(htt_pdev);
}

void
ol_rx_mic_error_handler(
    ol_txrx_pdev_handle pdev,
    u_int8_t tid,
    u_int16_t peer_id,
    void * msdu_desc,
    adf_nbuf_t msdu )
{
    union htt_rx_pn_t pn = {0};
    u_int8_t key_id = 0;

    struct ol_txrx_peer_t *peer = NULL;
    struct ol_txrx_vdev_t *vdev = NULL;

    if (pdev) {
        peer = ol_txrx_peer_find_by_id(pdev, peer_id);
        if (peer) {
            vdev = peer->vdev;
            if (vdev) {
                htt_rx_mpdu_desc_pn(vdev->pdev->htt_pdev, msdu_desc, &pn, 48);

                if (htt_rx_msdu_desc_key_id(vdev->pdev->htt_pdev, msdu_desc, &key_id)
                     == A_TRUE) {
                    ol_rx_err(vdev->pdev->ctrl_pdev, vdev->vdev_id,
                              peer->mac_addr.raw, tid, 0,
                              OL_RX_ERR_TKIP_MIC, msdu, &pn.pn48, key_id);
                }
            }
        }
    }
}
#if 0
/**
 * @brief populates vow ext stats in given network buffer.
 * @param msdu - network buffer handle
 * @param pdev - handle to htt dev.
 */
void ol_ath_add_vow_extstats(htt_pdev_handle pdev, adf_nbuf_t msdu)
{
    /* FIX THIS:
     * txrx should not be directly using data types (scn)
     * that are internal to other modules.
     */
    struct ol_ath_softc_net80211 *scn =
        (struct ol_ath_softc_net80211 *)pdev->ctrl_pdev;

    if (scn->vow_extstats == 0) {
        return;
    } else {
        u_int8_t *data, *l3_hdr, *bp;
        u_int16_t ethertype;
        int offset;
        struct vow_extstats vowstats;

        data = adf_nbuf_data(msdu);

        offset = ETHERNET_ADDR_LEN * 2;
        l3_hdr = data + ETHERNET_HDR_LEN;
        ethertype = (data[offset] << 8) | data[offset+1];
        if (ethertype == ETHERTYPE_IPV4) {
            offset = IPV4_HDR_OFFSET_PROTOCOL;
            if ((l3_hdr[offset] == IP_PROTOCOL_UDP) &&
                (l3_hdr[0] == IP_VER4_N_NO_EXTRA_HEADERS))
            {
                bp = data+EXT_HDR_OFFSET;

                if ( (data[RTP_HDR_OFFSET] == UDP_PDU_RTP_EXT) &&
                        (bp[0] == 0x12) &&
                        (bp[1] == 0x34) &&
                        (bp[2] == 0x00) &&
                        (bp[3] == 0x08))
                {
                    /*
                     * Clear UDP checksum so we do not have to recalculate it
                     * after filling in status fields.
                     */
                    data[UDP_CKSUM_OFFSET] = 0;
                    data[(UDP_CKSUM_OFFSET+1)] = 0;

                    bp += IPERF3_DATA_OFFSET;

                    htt_rx_get_vowext_stats(msdu, &vowstats);

                    /* control channel RSSI */
                    *bp++ = vowstats.rx_rssi_ctl0;
                    *bp++ = vowstats.rx_rssi_ctl1;
                    *bp++ = vowstats.rx_rssi_ctl2;

                    /* rx rate info */
                    *bp++ = vowstats.rx_bw;
                    *bp++ = vowstats.rx_sgi;
                    *bp++ = vowstats.rx_nss;

                    *bp++ = vowstats.rx_rssi_comb;
                    *bp++ = vowstats.rx_rs_flags; /* rsflags */

                    /* Time stamp Lo*/
                    *bp++ = (u_int8_t)
                        ((vowstats.rx_macTs & 0x0000ff00) >> 8);
                    *bp++ = (u_int8_t)
                        (vowstats.rx_macTs & 0x000000ff);
                    /* rx phy errors */
                    *bp++ = (u_int8_t)
                        ((scn->chan_stats.phy_err_cnt >> 8) & 0xff);
                    *bp++ = (u_int8_t)(scn->chan_stats.phy_err_cnt & 0xff);
                    /* rx clear count */
                    *bp++ = (u_int8_t)
                        ((scn->mib_cycle_cnts.rx_clear_count >> 24) & 0xff);
                    *bp++ = (u_int8_t)
                        ((scn->mib_cycle_cnts.rx_clear_count >> 16) & 0xff);
                    *bp++ = (u_int8_t)
                        ((scn->mib_cycle_cnts.rx_clear_count >>  8) & 0xff);
                    *bp++ = (u_int8_t)
                        (scn->mib_cycle_cnts.rx_clear_count & 0xff);
                    /* rx cycle count */
                    *bp++ = (u_int8_t)
                        ((scn->mib_cycle_cnts.cycle_count >> 24) & 0xff);
                    *bp++ = (u_int8_t)
                        ((scn->mib_cycle_cnts.cycle_count >> 16) & 0xff);
                    *bp++ = (u_int8_t)
                        ((scn->mib_cycle_cnts.cycle_count >>  8) & 0xff);
                    *bp++ = (u_int8_t)
                        (scn->mib_cycle_cnts.cycle_count & 0xff);

                    *bp++ = vowstats.rx_ratecode;
                    *bp++ = vowstats.rx_moreaggr;

                    /* sequence number */
                    *bp++ = (u_int8_t)((vowstats.rx_seqno >> 8) & 0xff);
                    *bp++ = (u_int8_t)(vowstats.rx_seqno & 0xff);
                }
            }
        }
    }
}

#endif
