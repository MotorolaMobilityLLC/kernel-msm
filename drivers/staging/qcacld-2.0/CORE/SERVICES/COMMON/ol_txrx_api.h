/*
 * Copyright (c) 2011-2013 The Linux Foundation. All rights reserved.
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
 * @file ol_txrx_api.h
 * @brief Definitions used in multiple external interfaces to the txrx SW.
 */
#ifndef _OL_TXRX_API__H_
#define _OL_TXRX_API__H_

/**
 * @typedef ol_txrx_pdev_handle
 * @brief opaque handle for txrx physical device object
 */
struct ol_txrx_pdev_t;
typedef struct ol_txrx_pdev_t* ol_txrx_pdev_handle;

/**
 * @typedef ol_txrx_vdev_handle
 * @brief opaque handle for txrx virtual device object
 */
struct ol_txrx_vdev_t;
typedef struct ol_txrx_vdev_t* ol_txrx_vdev_handle;

/**
 * @typedef ol_txrx_peer_handle
 * @brief opaque handle for txrx peer object
 */
struct ol_txrx_peer_t;
typedef struct ol_txrx_peer_t* ol_txrx_peer_handle;

/**
 * @brief ADDBA negotiation status, used both during requests and confirmations
 */
enum ol_addba_status {
    /* status: negotiation started or completed successfully */
    ol_addba_success,

    /* reject: aggregation is not applicable - don't try again */
    ol_addba_reject,

    /* busy: ADDBA negotiation couldn't be performed - try again later */
    ol_addba_busy,
};

enum ol_sec_type {
    ol_sec_type_none,
    ol_sec_type_wep128,
    ol_sec_type_wep104,
    ol_sec_type_wep40,
    ol_sec_type_tkip,
    ol_sec_type_tkip_nomic,
    ol_sec_type_aes_ccmp,
    ol_sec_type_wapi,

    /* keep this last! */
    ol_sec_type_types
};

/**
 * @enum ol_tx_spec
 * @brief indicate what non-standard transmission actions to apply
 * @details
 *  Indicate one or more of the following:
 *    - The tx frame already has a complete 802.11 header.
 *      Thus, skip 802.3/native-WiFi to 802.11 header encapsulation and
 *      A-MSDU aggregation.
 *    - The tx frame should not be aggregated (A-MPDU or A-MSDU)
 *    - The tx frame is already encrypted - don't attempt encryption.
 *    - The tx frame is a segment of a TCP jumbo frame.
 *    - This tx frame should not be unmapped and freed by the txrx layer
 *      after transmission, but instead given to a registered tx completion
 *      callback.
 *  More than one of these specification can apply, though typically
 *  only a single specification is applied to a tx frame.
 *  A compound specification can be created, as a bit-OR of these
 *  specifications.
 */
enum ol_tx_spec {
     ol_tx_spec_std         = 0x0,  /* do regular processing */
     ol_tx_spec_raw         = 0x1,  /* skip encap + A-MSDU aggr */
     ol_tx_spec_no_aggr     = 0x2,  /* skip encap + all aggr */
     ol_tx_spec_no_encrypt  = 0x4,  /* skip encap + encrypt */
     ol_tx_spec_tso         = 0x8,  /* TCP segmented */
     ol_tx_spec_nwifi_no_encrypt = 0x10, /* skip encrypt for nwifi */
     ol_tx_spec_no_free     = 0x20, /* give to cb rather than free */
};

#endif /* _OL_TXRX_API__H_ */
