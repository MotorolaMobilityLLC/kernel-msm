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
 * @file ol_tx_sched.h
 * @brief API definitions for the tx scheduler module within the data SW.
 */
#ifndef _OL_TX_SCHED__H_
#define _OL_TX_SCHED__H_

#include <adf_os_types.h>   /* a_bool_t */

enum ol_tx_queue_action {
    OL_TX_ENQUEUE_FRAME,
    OL_TX_DELETE_QUEUE,
    OL_TX_PAUSE_QUEUE,
    OL_TX_UNPAUSE_QUEUE,
    OL_TX_DISCARD_FRAMES,
};

struct ol_tx_sched_notify_ctx_t {
    int event;
    struct ol_tx_frms_queue_t *txq;
    union {
        int ext_tid;
        struct ol_txrx_msdu_info_t *tx_msdu_info;
    } info;
    int frames;
    int bytes;
};

#if defined(CONFIG_HL_SUPPORT)

void
ol_tx_sched_notify(
    struct ol_txrx_pdev_t *pdev, struct ol_tx_sched_notify_ctx_t *ctx);

void
ol_tx_sched(struct ol_txrx_pdev_t *pdev);

u_int16_t
ol_tx_sched_discard_select(
    struct ol_txrx_pdev_t *pdev,
    u_int16_t frms,
    ol_tx_desc_list *tx_descs,
    a_bool_t force);

void *
ol_tx_sched_attach(struct ol_txrx_pdev_t *pdev);

void
ol_tx_sched_detach(struct ol_txrx_pdev_t *pdev);

#else

#define ol_tx_notify_sched(pdev, ctx) /* no-op */
#define ol_tx_sched(pdev) /* no-op */
#define ol_tx_sched_discard_select(pdev, frms, tx_descs, force) 0
#define ol_tx_sched_attach(pdev) NULL
#define ol_tx_sched_detach(pdev) /* no-op */

#endif /* defined(CONFIG_HL_SUPPORT) */


#if defined(CONFIG_HL_SUPPORT) || defined(TX_CREDIT_RECLAIM_SUPPORT)
/*
 * HL needs to keep track of the amount of credit available to download
 * tx frames to the target - the download scheduler decides when to
 * download frames, and which frames to download, based on the credit
 * availability.
 * LL systems that use TX_CREDIT_RECLAIM_SUPPORT also need to keep track
 * of the target_tx_credit, to determine when to poll for tx completion
 * messages.
 */
#define OL_TX_TARGET_CREDIT_ADJUST(factor, pdev, msdu) \
    adf_os_atomic_add( \
        factor * htt_tx_msdu_credit(msdu), &pdev->target_tx_credit)
#define OL_TX_TARGET_CREDIT_DECR(pdev, msdu) \
    OL_TX_TARGET_CREDIT_ADJUST(-1, pdev, msdu)
#define OL_TX_TARGET_CREDIT_INCR(pdev, msdu) \
    OL_TX_TARGET_CREDIT_ADJUST(1, pdev, msdu)
#else
/*
 * LL does not need to keep track of target credit.
 * Since the host tx descriptor pool size matches the target's,
 * we know the target has space for the new tx frame if the host's
 * tx descriptor allocation succeeded.
 */
#define OL_TX_TARGET_CREDIT_DECR(pdev, msdu)  /* no-op */
#define OL_TX_TARGET_CREDIT_INCR(pdev, msdu)  /* no-op */
#define OL_TX_TARGET_CREDIT_ADJUST(factor, pdev, msdu)  /* no-op */

#endif

#endif /* _OL_TX_SCHED__H_ */
