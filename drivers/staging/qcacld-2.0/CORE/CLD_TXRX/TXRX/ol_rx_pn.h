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

#ifndef _OL_RX_PN_H_
#define _OL_RX_PN_H_

#include <adf_nbuf.h>        /* adf_nbuf_t, etc. */

#include <ol_txrx_api.h>     /* ol_txrx_peer_t, etc. */

int ol_rx_pn_cmp24(
    union htt_rx_pn_t *new_pn,
    union htt_rx_pn_t *old_pn,
    int is_unicast,
    int opmode);

int ol_rx_pn_cmp48(
    union htt_rx_pn_t *new_pn,
    union htt_rx_pn_t *old_pn,
    int is_unicast,
    int opmode);

int ol_rx_pn_wapi_cmp(
    union htt_rx_pn_t *new_pn,
    union htt_rx_pn_t *old_pn,
    int is_unicast,
    int opmode);

/**
 * @brief If applicable, check the Packet Number to detect replays.
 * @details
 *  Determine whether a PN check is needed, and if so, what the PN size is.
 *  (A PN size of 0 is used to indirectly bypass the PN check for security
 *  methods that don't involve a PN check.)
 *  This function produces event notifications for any PN failures, via the
 *  ol_rx_err function.
 *  After the PN check, call the next stage of rx processing (rx --> tx
 *  forwarding check).
 *
 * @param vdev - which virtual device the frames were addressed to
 * @param peer - which peer the rx frames belong to
 * @param tid  - which TID within the peer the rx frames belong to
 * @param msdu_list - NULL-terminated list of MSDUs to perform PN check on
 *      (if PN check is applicable, i.e. PN length > 0)
 */
void
ol_rx_pn_check(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    adf_nbuf_t msdu_list);

/**
 * @brief If applicable, check the Packet Number to detect replays.
 * @details
 *  Determine whether a PN check is needed, and if so, what the PN size is.
 *  (A PN size of 0 is used to indirectly bypass the PN check for security
 *  methods that don't involve a PN check.)
 *  This function produces event notifications for any PN failures, via the
 *  ol_rx_err function.
 *  After the PN check, deliver the valid rx frames to the OS shim.
 *  (Don't perform a rx --> tx forwarding check.)
 *
 * @param vdev - which virtual device the frames were addressed to
 * @param peer - which peer the rx frames belong to
 * @param tid  - which TID within the peer the rx frames belong to
 * @param msdu_list - NULL-terminated list of MSDUs to perform PN check on
 *      (if PN check is applicable, i.e. PN length > 0)
 */
void
ol_rx_pn_check_only(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    adf_nbuf_t msdu_list);

/**
 * @brief If applicable, check the Packet Number to detect replays.
 * @details
 *  Same as ol_rx_pn_check but return valid rx netbufs
 *  rather than invoking the rx --> tx forwarding check.
 *
 * @param vdev - which virtual device the frames were addressed to
 * @param peer - which peer the rx frames belong to
 * @param tid - which TID within the peer the rx frames belong to
 * @param msdu_list - NULL-terminated list of MSDUs to perform PN check on
 *      (if PN check is applicable, i.e. PN length > 0)
 * @return list of netbufs that didn't fail the PN check
 */
adf_nbuf_t
ol_rx_pn_check_base(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    unsigned tid,
    adf_nbuf_t msdu_list);


#endif /* _OL_RX_PN_H_ */
