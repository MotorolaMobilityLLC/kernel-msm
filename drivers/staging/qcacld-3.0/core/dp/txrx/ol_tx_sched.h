/*
 * Copyright (c) 2012-2013, 2016-2019 The Linux Foundation. All rights reserved.
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
 * @file ol_tx_sched.h
 * @brief API definitions for the tx scheduler module within the data SW.
 */
#ifndef _OL_TX_SCHED__H_
#define _OL_TX_SCHED__H_

#include <qdf_types.h>

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
		struct ol_txrx_pdev_t *pdev,
		struct ol_tx_sched_notify_ctx_t *ctx);

void
ol_tx_sched(struct ol_txrx_pdev_t *pdev);

u_int16_t
ol_tx_sched_discard_select(
		struct ol_txrx_pdev_t *pdev,
		u_int16_t frms,
		ol_tx_desc_list *tx_descs,
		bool force);

void *
ol_tx_sched_attach(struct ol_txrx_pdev_t *pdev);

void
ol_tx_sched_detach(struct ol_txrx_pdev_t *pdev);

void ol_tx_sched_stats_display(struct ol_txrx_pdev_t *pdev);

void ol_tx_sched_cur_state_display(struct ol_txrx_pdev_t *pdev);

void ol_tx_sched_stats_clear(struct ol_txrx_pdev_t *pdev);

void
ol_txrx_set_wmm_param(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
		      struct ol_tx_wmm_param_t wmm_param);

#else

static inline void
ol_tx_sched_notify(
		struct ol_txrx_pdev_t *pdev,
		struct ol_tx_sched_notify_ctx_t *ctx)
{
}

static inline void
ol_tx_sched(struct ol_txrx_pdev_t *pdev)
{
}

static inline u_int16_t
ol_tx_sched_discard_select(
		struct ol_txrx_pdev_t *pdev,
		u_int16_t frms,
		ol_tx_desc_list *tx_descs,
		bool force)
{
	return 0;
}

static inline void *
ol_tx_sched_attach(struct ol_txrx_pdev_t *pdev)
{
	return NULL;
}

static inline void
ol_tx_sched_detach(struct ol_txrx_pdev_t *pdev)
{
}

static inline void ol_tx_sched_stats_display(struct ol_txrx_pdev_t *pdev)
{
}

static inline void ol_tx_sched_cur_state_display(struct ol_txrx_pdev_t *pdev)
{
}

static inline void ol_tx_sched_stats_clear(struct ol_txrx_pdev_t *pdev)
{
}

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

static inline void
ol_tx_target_credit_adjust(int factor,
			   struct ol_txrx_pdev_t *pdev,
			   qdf_nbuf_t msdu)
{
	qdf_atomic_add(factor * htt_tx_msdu_credit(msdu),
		       &pdev->target_tx_credit);
}

static inline void ol_tx_target_credit_decr(struct ol_txrx_pdev_t *pdev,
					    qdf_nbuf_t msdu)
{
	ol_tx_target_credit_adjust(-1, pdev, msdu);
}

static inline void ol_tx_target_credit_incr(struct ol_txrx_pdev_t *pdev,
					    qdf_nbuf_t msdu)
{
	ol_tx_target_credit_adjust(1, pdev, msdu);
}

#ifdef QCA_TX_PADDING_CREDIT_SUPPORT

#define MIN_TX_PAD_CREDIT_THRESH        4
#define MAX_TX_PAD_CREDIT_THRESH        5

#endif /* QCA_TX_PADDING_CREDIT_SUPPORT */

#else
/*
 * LL does not need to keep track of target credit.
 * Since the host tx descriptor pool size matches the target's,
 * we know the target has space for the new tx frame if the host's
 * tx descriptor allocation succeeded.
 */
static inline void
ol_tx_target_credit_adjust(int factor,
			   struct ol_txrx_pdev_t *pdev,
			   qdf_nbuf_t msdu)
{
}

static inline void ol_tx_target_credit_decr(struct ol_txrx_pdev_t *pdev,
					    qdf_nbuf_t msdu)
{
}

static inline void ol_tx_target_credit_incr(struct ol_txrx_pdev_t *pdev,
					    qdf_nbuf_t msdu)
{
}

#endif
#endif /* _OL_TX_SCHED__H_ */
