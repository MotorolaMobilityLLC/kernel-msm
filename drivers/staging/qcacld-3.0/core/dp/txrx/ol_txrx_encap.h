/*
 * Copyright (c) 2012, 2014-2016, 2018 The Linux Foundation. All rights reserved.
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

/**
 * @file ol_txrx_encap.h
 * @brief definitions for txrx encap/decap function and struct
 */
#ifndef _OL_TXRX_ENCAP__H_
#define _OL_TXRX_ENCAP__H_

#ifdef QCA_SUPPORT_SW_TXRX_ENCAP

#include <qdf_nbuf.h>               /* qdf_nbuf_t */
#include <cds_ieee80211_common.h>   /* ieee80211_qosframe_htc_addr4 */
#include <cdp_txrx_cmn.h>           /* ol_txrx_vdev_t, etc. */

/**
 * @brief Encap outgoing frm from OS dependent format to Target
 *        acceptable frm format
 * @details
 *     For native wifi format, the function will add Qos control field
 *  based on peer's QOS capbabilities .
 *     For 802.3 format, the function will transform to 802.11 format
 *  with or without QOS control field based on peer's QOS capabilities.
 * @param vdev - handle to vdev object
 * @param tx_desc - tx desc struct,some fields will be updated.
 * @param msdu - qdf_nbuf_t
 * @param msdu_info - information from tx classification.
 * @return
 *     A_OK: encap operation successful
 *     other: operation failed,the msdu need be dropped.
 */
A_STATUS
ol_tx_encap(struct ol_txrx_vdev_t *vdev,
	    struct ol_tx_desc_t *tx_desc,
	    qdf_nbuf_t msdu, struct ol_txrx_msdu_info_t *msdu_info);

struct ol_rx_decap_info_t {
	uint8_t hdr[sizeof(struct ieee80211_qosframe_htc_addr4)];
	int hdr_len;
	uint8_t is_subfrm:1, is_first_subfrm:1, is_msdu_cmpl_mpdu:1;
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
 * @param msdu - qdf_nbuf_t
 * @param info - ol_rx_decap_info_t: context info for decap
 * @return
 *     A_OK: decap operation successful
 *     other: operation failed,the msdu need be dropped.
 */
A_STATUS
ol_rx_decap(struct ol_txrx_vdev_t *vdev,
	    struct ol_txrx_peer_t *peer,
	    qdf_nbuf_t msdu, struct ol_rx_decap_info_t *info);

static inline A_STATUS
OL_TX_ENCAP(struct ol_txrx_vdev_t *vdev,
	    struct ol_tx_desc_t *tx_desc,
	    qdf_nbuf_t msdu, struct ol_txrx_msdu_info_t *msdu_info)
{
	if (vdev->pdev->sw_tx_encap)
		return ol_tx_encap(vdev, tx_desc, msdu, msdu_info);
	return A_OK;
}

static inline A_STATUS
OL_RX_DECAP(struct ol_txrx_vdev_t *vdev,
	    struct ol_txrx_peer_t *peer,
	    qdf_nbuf_t msdu, struct ol_rx_decap_info_t *info)
{
	if (vdev->pdev->sw_rx_decap)
		return ol_rx_decap(vdev, peer, msdu, info);
	return A_OK;
}

#define OL_TX_RESTORE_HDR(__tx_desc, __msdu)  \
	do {								\
		if (__tx_desc->orig_l2_hdr_bytes != 0)			\
			qdf_nbuf_push_head(__msdu,			\
					   __tx_desc->orig_l2_hdr_bytes); \
	} while (0)
#else
#define OL_TX_ENCAP(vdev, tx_desc, msdu, msdu_info) A_OK
#define OL_RX_DECAP(vdev, peer, msdu, info) A_OK
#define OL_TX_RESTORE_HDR(__tx_desc, __msdu)
#endif
#endif /* _OL_TXRX_ENCAP__H_ */
