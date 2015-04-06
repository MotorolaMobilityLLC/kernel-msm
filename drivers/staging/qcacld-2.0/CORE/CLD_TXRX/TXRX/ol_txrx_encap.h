/*
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
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
 * @file ol_txrx_encap.h
 * @brief definitions for txrx encap/decap function and struct
 */
#ifndef _OL_TXRX_ENCAP__H_
#define _OL_TXRX_ENCAP__H_

#ifdef QCA_SUPPORT_SW_TXRX_ENCAP

#include <adf_nbuf.h>    /* adf_nbuf_t */
#include <ieee80211_common.h>     /* ieee80211_qosframe_htc_addr4 */
#include <ol_txrx_types.h>  /* ol_tx_desc_t, ol_txrx_msdu_info_t */

/**
 * @brief Encap outgoing frm from OS dependent format to Target
 *        acceptable frm format
 * @details
 *     For native wifi format, the function will add Qos control field
 *  based on peer's QOS capbabilities .
 *     For 802.3 format, the function will transform to 802.11 format
 *  with or without QOS control field based on peer's QOS capabilites.
 * @param vdev - handle to vdev object
 * @param tx_desc - tx desc struct,some fields will be updated.
 * @param msdu - adf_nbuf_t
 * @param msdu_info - informations from tx classification.
 * @return
 *     A_OK: encap operation sucessful
 *     other: operation failed,the msdu need be dropped.
 */
A_STATUS
ol_tx_encap(
    struct ol_txrx_vdev_t *vdev,
    struct ol_tx_desc_t *tx_desc,
    adf_nbuf_t msdu,
    struct ol_txrx_msdu_info_t *msdu_info);


struct ol_rx_decap_info_t {
    u_int8_t hdr[sizeof(struct ieee80211_qosframe_htc_addr4)];
    int hdr_len;
    u_int8_t is_subfrm : 1,
             is_first_subfrm :1,
             is_msdu_cmpl_mpdu : 1;
};

/**
 * @brief decap incoming frm from Target to Host OS
 *        acceptable frm format
 * @details
 *     For native wifi format, the function will remove Qos control field
 *  and HT control field if any.
 *     For 802.3 format, the function will will do llc snap header process
 *  if Target haven't done that.
 * @param vdev - handle to vdev object
 * @param peer - the peer object.
 * @param msdu - adf_nbuf_t
 * @param info - ol_rx_decap_info_t: context info for decap
 * @return
 *     A_OK: decap operation sucessful
 *     other: operation failed,the msdu need be dropped.
 */
A_STATUS
ol_rx_decap (
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    adf_nbuf_t msdu,
    struct ol_rx_decap_info_t *info);


static inline A_STATUS
OL_TX_ENCAP(
    struct ol_txrx_vdev_t *vdev,
    struct ol_tx_desc_t *tx_desc,
    adf_nbuf_t msdu,
    struct ol_txrx_msdu_info_t *msdu_info)
{
    if (vdev->pdev->sw_tx_encap) {
        return ol_tx_encap(vdev, tx_desc, msdu,msdu_info);
    }
    return A_OK;
}

static inline A_STATUS
OL_RX_DECAP(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    adf_nbuf_t msdu,
    struct ol_rx_decap_info_t *info)
{
    if (vdev->pdev->sw_rx_decap) {
        return ol_rx_decap(vdev, peer, msdu, info);
    }
    return A_OK;
}

#define OL_TX_RESTORE_HDR(__tx_desc,__msdu)  \
        if(__tx_desc->orig_l2_hdr_bytes != 0) { \
            adf_nbuf_push_head(__msdu, __tx_desc->orig_l2_hdr_bytes); \
        }
#else
#define OL_TX_ENCAP(vdev, tx_desc, msdu, msdu_info) A_OK
#define OL_RX_DECAP(vdev, peer, msdu, info) A_OK
#define OL_TX_RESTORE_HDR(__tx_desc,__msdu)
#endif
#endif /* _OL_TXRX_ENCAP__H_ */
