/*
 * Copyright (c) 2011, 2014-2017 The Linux Foundation. All rights reserved.
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
 * @file ol_txrx_dbg.h
 * @brief Functions provided for visibility and debugging.
 */
#ifndef _OL_TXRX_DBG__H_
#define _OL_TXRX_DBG__H_

#include <athdefs.h>            /* A_STATUS, uint64_t */
#include <qdf_lock.h>           /* qdf_semaphore_t */
#include <htt.h>                /* htt_dbg_stats_type */
#include <ol_txrx_stats.h>      /* ol_txrx_stats */

#ifndef TXRX_DEBUG_LEVEL
#define TXRX_DEBUG_LEVEL 0      /* no debug info */
#endif

enum {
	TXRX_DBG_MASK_OBJS = 0x01,
	TXRX_DBG_MASK_STATS = 0x02,
	TXRX_DBG_MASK_PROT_ANALYZE = 0x04,
	TXRX_DBG_MASK_RX_REORDER_TRACE = 0x08,
	TXRX_DBG_MASK_RX_PN_TRACE = 0x10
};

/*--- txrx printouts ---*/

/*
 * Uncomment this to enable txrx printouts with dynamically adjustable
 * verbosity.  These printouts should not impact performance.
 */
#define TXRX_PRINT_ENABLE 1
/* uncomment this for verbose txrx printouts (may impact performance) */
/* #define TXRX_PRINT_VERBOSE_ENABLE 1 */

/*--- txrx object (pdev, vdev, peer) display debug functions ---*/

#if TXRX_DEBUG_LEVEL > 5
void ol_txrx_pdev_display(ol_txrx_pdev_handle pdev, int indent);
void ol_txrx_vdev_display(ol_txrx_vdev_handle vdev, int indent);
void ol_txrx_peer_display(ol_txrx_peer_handle peer, int indent);
#else
#define ol_txrx_pdev_display(pdev, indent)
#define ol_txrx_vdev_display(vdev, indent)
#define ol_txrx_peer_display(peer, indent)
#endif

/*--- txrx stats display debug functions ---*/

/**
 * ol_txrx_stats_display() - display tx rx stats
 * @pdev: pdev handle
 * @level: verbosity level for logs
 *
 * Return: none
 */
void ol_txrx_stats_display(ol_txrx_pdev_handle pdev,
			   enum qdf_stats_verbosity_level level);

void ol_txrx_stats_clear(ol_txrx_pdev_handle pdev);


/*--- txrx protocol analyzer debug feature ---*/

/* uncomment this to enable the protocol analzyer feature */
/* #define ENABLE_TXRX_PROT_ANALYZE 1 */

#if defined(ENABLE_TXRX_PROT_ANALYZE)

void ol_txrx_prot_ans_display(ol_txrx_pdev_handle pdev);

#else

#define ol_txrx_prot_ans_display(pdev)

#endif /* ENABLE_TXRX_PROT_ANALYZE */

/*--- txrx sequence number trace debug feature ---*/

/* uncomment this to enable the rx reorder trace feature */
/* #define ENABLE_RX_REORDER_TRACE 1 */

#define ol_txrx_seq_num_trace_display(pdev) \
	ol_rx_reorder_trace_display(pdev, 0, 0)

#if defined(ENABLE_RX_REORDER_TRACE)

void
ol_rx_reorder_trace_display(ol_txrx_pdev_handle pdev, int just_once, int limit);

#else

#define ol_rx_reorder_trace_display(pdev, just_once, limit)

#endif /* ENABLE_RX_REORDER_TRACE */

/*--- txrx packet number trace debug feature ---*/

/* uncomment this to enable the rx PN trace feature */
/* #define ENABLE_RX_PN_TRACE 1 */

#define ol_txrx_pn_trace_display(pdev) ol_rx_pn_trace_display(pdev, 0)

#if defined(ENABLE_RX_PN_TRACE)

void ol_rx_pn_trace_display(ol_txrx_pdev_handle pdev, int just_once);

#else

#define ol_rx_pn_trace_display(pdev, just_once)

#endif /* ENABLE_RX_PN_TRACE */

/*--- tx queue log debug feature ---*/
/* uncomment this to enable the tx queue log feature */
/* #define ENABLE_TX_QUEUE_LOG 1 */

#if defined(DEBUG_HL_LOGGING) && defined(CONFIG_HL_SUPPORT)

void
ol_tx_queue_log_display(ol_txrx_pdev_handle pdev);
void ol_tx_queue_log_clear(ol_txrx_pdev_handle pdev);
#else

static inline
void ol_tx_queue_log_display(ol_txrx_pdev_handle pdev)
{
}

static inline
void ol_tx_queue_log_clear(ol_txrx_pdev_handle pdev)
{
}
#endif /* defined(DEBUG_HL_LOGGING) && defined(CONFIG_HL_SUPPORT) */


/*----------------------------------------*/

#endif /* _OL_TXRX_DBG__H_ */
