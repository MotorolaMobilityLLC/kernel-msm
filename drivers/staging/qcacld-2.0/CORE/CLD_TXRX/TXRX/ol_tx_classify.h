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
 * @file ol_tx_classify.h
 * @brief API definitions for the tx classify module within the data SW.
 */
#ifndef _OL_TX_CLASSIFY__H_
#define _OL_TX_CLASSIFY__H_

#include <adf_nbuf.h>      /* adf_nbuf_t */
#include <ol_txrx_types.h> /* ol_txrx_vdev_t, etc. */

static inline u_int8_t *
ol_tx_dest_addr_find(
    struct ol_txrx_pdev_t *pdev,
    adf_nbuf_t tx_nbuf)
{
    u_int8_t *hdr_ptr;
    void *datap = adf_nbuf_data(tx_nbuf);

    if (pdev->frame_format == wlan_frm_fmt_raw) {
        /* adjust hdr_ptr to RA */
        struct ieee80211_frame *wh = (struct ieee80211_frame *)datap;
        hdr_ptr = wh->i_addr1;
    } else if (pdev->frame_format == wlan_frm_fmt_native_wifi) {
        /* adjust hdr_ptr to RA */
        struct ieee80211_frame *wh = (struct ieee80211_frame *)datap;
        hdr_ptr = wh->i_addr1;
    } else if (pdev->frame_format == wlan_frm_fmt_802_3) {
        hdr_ptr = datap;
    } else {
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
            "Invalid standard frame type: %d\n",
            pdev->frame_format);
        adf_os_assert(0);
        hdr_ptr = NULL;
    }
    return hdr_ptr;
}

#if defined(CONFIG_HL_SUPPORT)

/**
 * @brief Classify a tx frame to which tid queue.
 *
 * @param vdev - the virtual device sending the data
 *      (for specifying the transmitter address for multicast / broadcast data)
 * @param tx_desc - descriptor object with meta-data about the tx frame
 * @param netbuf - the tx frame
 * @param tx_msdu_info - characteristics of the tx frame
 */
struct ol_tx_frms_queue_t *
ol_tx_classify(
    struct ol_txrx_vdev_t *vdev,
    struct ol_tx_desc_t *tx_desc,
    adf_nbuf_t netbuf,
    struct ol_txrx_msdu_info_t *tx_msdu_info);

struct ol_tx_frms_queue_t *
ol_tx_classify_mgmt(
    struct ol_txrx_vdev_t *vdev,
    struct ol_tx_desc_t *tx_desc,
    adf_nbuf_t netbuf,
    struct ol_txrx_msdu_info_t *tx_msdu_info);

#else

#define ol_tx_classify(vdev, tx_desc, netbuf, tx_msdu_info) NULL
#define ol_tx_classify_mgmt(vdev, tx_desc, netbuf, tx_msdu_info) NULL

#endif /* defined(CONFIG_HL_SUPPORT) */


#endif /* _OL_TX_CLASSIFY__H_ */
