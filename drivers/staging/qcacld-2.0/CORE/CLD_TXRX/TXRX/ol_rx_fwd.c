/*
 * Copyright (c) 2011 The Linux Foundation. All rights reserved.
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

/* standard header files */
#include <adf_nbuf.h>         /* adf_nbuf_map */
#include <adf_os_mem.h>       /* adf_os_mem_cmp */

/* external header files */
#include <ol_cfg.h>           /* wlan_op_mode_ap, etc. */
#include <ol_htt_rx_api.h>    /* htt_rx_msdu_desc_retrieve */
#include <ieee80211_common.h>        /* ieee80211_frame, etc. */

/* internal header files */
#include <ol_txrx_types.h>    /* ol_txrx_dev_t, etc. */
#include <ol_rx_fwd.h>        /* our own defs */
#include <ol_rx.h>            /* ol_rx_deliver */
#include <ol_txrx_internal.h> /* TXRX_ASSERT1 */

/*
 * Porting from Ap11PrepareForwardedPacket.
 * This routine is called when a RX data frame from an associated station is
 * to be forwarded to another associated station. We will prepare the
 * received packet so that it is suitable for transmission again.
 * Check that this Packet is suitable for forwarding. If yes, then
 * prepare the new 802.11 header.
 */
static inline
void
ol_ap_fwd_check(struct ol_txrx_vdev_t *vdev, adf_nbuf_t msdu)
{
    struct ieee80211_frame *mac_header;
    unsigned char tmp_addr[6];
    unsigned char type;
    unsigned char subtype;
    unsigned char fromds;
    unsigned char tods;

    mac_header = (struct ieee80211_frame *) (adf_nbuf_data(msdu));
    TXRX_ASSERT1(mac_header);

    type    = mac_header->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = mac_header->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    tods    = mac_header->i_fc[1] & IEEE80211_FC1_DIR_TODS;
    fromds  = mac_header->i_fc[1] & IEEE80211_FC1_DIR_FROMDS;

	/*
     * Make sure no QOS or any other non-data subtype
     * Should be a ToDs data frame.
     * Make sure that this frame is unicast and not for us.
     * These packets should come up through the normal rx path and not forwarded.
     */
    if (type != IEEE80211_FC0_TYPE_DATA ||
        subtype != 0x0 ||
        ((tods != 1) || (fromds != 0)) ||
        (adf_os_mem_cmp(
            mac_header->i_addr3, vdev->mac_addr.raw, IEEE80211_ADDR_LEN) == 0))
	{
#ifdef DEBUG_HOST_RC
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit: %s | Unnecessary to adjust mac header\n", __func__);
#endif
    }
    else
    {
        // Flip the ToDs bit to FromDs
        mac_header->i_fc[1] &= 0xfe;
        mac_header->i_fc[1] |= 0x2;

        /*
         * Flip the addresses
         * (ToDs, addr1, RA=BSSID) move to (FrDs, addr2, TA=BSSID)
         * (ToDs, addr2, SA) move to (FrDs, addr3, SA)
         * (ToDs, addr3, DA) move to (FrDs, addr1, DA)
         */

        memcpy(tmp_addr,
               mac_header->i_addr2,
               sizeof (tmp_addr));

        memcpy(mac_header->i_addr2,
               mac_header->i_addr1,
               sizeof (tmp_addr));

        memcpy(mac_header->i_addr1,
               mac_header->i_addr3,
               sizeof (tmp_addr));

        memcpy(mac_header->i_addr3,
               tmp_addr,
               sizeof (tmp_addr));
    }
}

static inline
void
ol_rx_fwd_to_tx(struct ol_txrx_vdev_t *vdev, adf_nbuf_t msdu)
{
    struct ol_txrx_pdev_t *pdev = vdev->pdev;

    if (pdev->frame_format == wlan_frm_fmt_native_wifi)
    {
        ol_ap_fwd_check(vdev, msdu);
    }
    /*
     * Map the netbuf, so it's accessible to the DMA that
     * sends it to the target.
     */
    adf_nbuf_map_single(pdev->osdev, msdu, ADF_OS_DMA_TO_DEVICE);
    adf_nbuf_set_next(msdu, NULL); /* add NULL terminator */
    TXRX_STATS_MSDU_INCR(vdev->pdev, rx.forwarded, msdu);

    /* for HL, point to payload before send to tx again.*/
    if (pdev->cfg.is_high_latency) {
        void *rx_desc;
        rx_desc = htt_rx_msdu_desc_retrieve(pdev->htt_pdev, msdu);

        adf_nbuf_pull_head(msdu,
                htt_rx_msdu_rx_desc_size_hl(pdev->htt_pdev,
                    rx_desc));
    }

    msdu = vdev->tx(vdev, msdu);

    if (msdu) {
        /*
         * The frame was not accepted by the tx.
         * We could store the frame and try again later,
         * but the simplest solution is to discard the frames.
         */
        adf_nbuf_unmap_single(pdev->osdev, msdu, ADF_OS_DMA_TO_DEVICE);
        adf_nbuf_free(msdu);
    }
}

void
ol_rx_fwd_check(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    adf_nbuf_t msdu_list)
{
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    adf_nbuf_t deliver_list_head = NULL;
    adf_nbuf_t deliver_list_tail = NULL;
    adf_nbuf_t msdu;

    msdu = msdu_list;
    while (msdu) {
        struct ol_txrx_vdev_t *tx_vdev;
        void *rx_desc;
        /*
         * Remember the next list elem, because our processing
         * may cause the MSDU to get linked into a different list.
         */
        msdu_list = adf_nbuf_next(msdu);

        rx_desc = htt_rx_msdu_desc_retrieve(pdev->htt_pdev, msdu);

        if (!vdev->disable_intrabss_fwd &&
            htt_rx_msdu_forward(pdev->htt_pdev, rx_desc)) {
            /*
             * Use the same vdev that received the frame to
             * transmit the frame.
             * This is exactly what we want for intra-BSS forwarding,
             * like STA-to-STA forwarding and multicast echo.
             * If this is a intra-BSS forwarding case (which is not
             * currently supported), then the tx vdev is different
             * from the rx vdev.
             * On the LL host the vdevs are not actually used for tx,
             * so it would still work to use the rx vdev rather than
             * the tx vdev.
             * For HL, the tx classification searches for the DA within
             * the given vdev, so we would want to get the DA peer ID
             * from the target, so we can locate the tx vdev.
             */
            tx_vdev = vdev;
            /*
             * Copying TID value of RX packet to forwarded
             * packet if the tid is other than non qos tid.
             * But for non qos tid fill invalid tid so that
             * Fw will take care of filling proper tid.
             */
            if (tid != HTT_NON_QOS_TID) {
                adf_nbuf_set_tid(msdu, tid);
            } else {
                adf_nbuf_set_tid(msdu, ADF_NBUF_TX_EXT_TID_INVALID);
            }
            /*
             * This MSDU needs to be forwarded to the tx path.
             * Check whether it also needs to be sent to the OS shim,
             * in which case we need to make a copy (or clone?).
             */
            if (htt_rx_msdu_discard(pdev->htt_pdev, rx_desc)) {
                htt_rx_msdu_desc_free(pdev->htt_pdev, msdu);
                ol_rx_fwd_to_tx(tx_vdev, msdu);
                msdu = NULL; /* already handled this MSDU */
            } else {
				adf_nbuf_t copy;
				copy = adf_nbuf_copy(msdu);
                if (copy) {
					ol_rx_fwd_to_tx(tx_vdev, copy);
                }
            }
        }
        if (msdu) {
            /* send this frame to the OS */
            OL_TXRX_LIST_APPEND(deliver_list_head, deliver_list_tail, msdu);
        }
        msdu = msdu_list;
    }
    if (deliver_list_head) {
        adf_nbuf_set_next(deliver_list_tail, NULL); /* add NULL terminator */
        if (ol_cfg_is_full_reorder_offload(pdev->ctrl_pdev)) {
            ol_rx_in_order_deliver(vdev, peer, tid, deliver_list_head);
        } else {
            ol_rx_deliver(vdev, peer, tid, deliver_list_head);
        }
    }
}
