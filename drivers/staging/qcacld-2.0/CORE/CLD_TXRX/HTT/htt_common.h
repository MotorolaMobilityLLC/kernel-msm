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
 * @file htt_common.h
 *
 * @details the public header file of HTT layer shared between host and firmware
 */

#ifndef _HTT_COMMON_H_
#define _HTT_COMMON_H_

enum htt_sec_type {
    htt_sec_type_none,
    htt_sec_type_wep128,
    htt_sec_type_wep104,
    htt_sec_type_wep40,
    htt_sec_type_tkip,
    htt_sec_type_tkip_nomic,
    htt_sec_type_aes_ccmp,
    htt_sec_type_wapi,

    /* keep this last! */
    htt_num_sec_types
};


enum htt_rx_ind_mpdu_status {
    HTT_RX_IND_MPDU_STATUS_UNKNOWN = 0x0,
    HTT_RX_IND_MPDU_STATUS_OK,
    HTT_RX_IND_MPDU_STATUS_ERR_FCS,
    HTT_RX_IND_MPDU_STATUS_ERR_DUP,
    HTT_RX_IND_MPDU_STATUS_ERR_REPLAY,
    HTT_RX_IND_MPDU_STATUS_ERR_INV_PEER,
    HTT_RX_IND_MPDU_STATUS_UNAUTH_PEER, /* only accept EAPOL frames */
    HTT_RX_IND_MPDU_STATUS_OUT_OF_SYNC,
    HTT_RX_IND_MPDU_STATUS_MGMT_CTRL, /* Non-data in promiscous mode */
    HTT_RX_IND_MPDU_STATUS_TKIP_MIC_ERR,

    /*
     * MISC: discard for unspecified reasons.
     * Leave this enum value last.
     */
    HTT_RX_IND_MPDU_STATUS_ERR_MISC = 0xFF
};


#endif /* _HTT_COMMON_H_ */
