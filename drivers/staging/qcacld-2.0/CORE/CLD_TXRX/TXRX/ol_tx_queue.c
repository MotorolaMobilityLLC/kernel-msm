/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

#include <adf_nbuf.h>         /* adf_nbuf_t, etc. */
#include <adf_os_atomic.h>    /* adf_os_atomic_add, etc. */
#include <ol_cfg.h>           /* ol_cfg_addba_retry */
#include <htt.h>              /* HTT_TX_EXT_TID_MGMT */
#include <ol_htt_tx_api.h>    /* htt_tx_desc_tid */
#include <ol_txrx_api.h>      /* ol_txrx_vdev_handle */
#include <ol_txrx_ctrl_api.h> /* ol_txrx_sync, ol_tx_addba_conf */
#include <ol_ctrl_txrx_api.h> /* ol_ctrl_addba_req */
#include <ol_txrx_internal.h> /* TXRX_ASSERT1, etc. */
#include <ol_txrx_types.h>    /* pdev stats */
#include <ol_tx_desc.h>       /* ol_tx_desc, ol_tx_desc_frame_list_free */
#include <ol_tx.h>            /* ol_tx_vdev_ll_pause_queue_send */
#include <ol_tx_sched.h>      /* ol_tx_sched_notify, etc. */
#include <ol_tx_queue.h>
#include <ol_txrx_dbg.h>      /* DEBUG_HL_LOGGING */
#include <ol_txrx.h>          /* ol_tx_desc_pool_size_hl */
#include <adf_os_types.h>     /* a_bool_t */
#include <ol_txrx_peer_find.h>


#if defined(CONFIG_HL_SUPPORT)

#ifndef offsetof
#define offsetof(type, field)   ((adf_os_size_t)(&((type *)0)->field))
#endif

/*--- function prototypes for optional queue log feature --------------------*/
#if defined(DEBUG_HL_LOGGING)

void
ol_tx_queue_log_enqueue(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_msdu_info_t *msdu_info,
    int frms, int bytes);
void
ol_tx_queue_log_dequeue(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    int frms, int bytes);
void
ol_tx_queue_log_free(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    int tid, int frms, int bytes);
#define OL_TX_QUEUE_LOG_ENQUEUE ol_tx_queue_log_enqueue
#define OL_TX_QUEUE_LOG_DEQUEUE ol_tx_queue_log_dequeue
#define OL_TX_QUEUE_LOG_FREE    ol_tx_queue_log_free

#else

#define OL_TX_QUEUE_LOG_ENQUEUE(pdev, msdu_info, frms, bytes) /* no-op */
#define OL_TX_QUEUE_LOG_DEQUEUE(pdev, txq, frms, bytes) /* no-op */
#define OL_TX_QUEUE_LOG_FREE(pdev, txq, tid, frms, bytes) /* no-op */

#endif /* TXRX_DEBUG_LEVEL > 5 */


/*--- function prototypes for optional host ADDBA negotiation ---------------*/

#define OL_TX_QUEUE_ADDBA_CHECK(pdev, txq, tx_msdu_info) /* no-op */


#ifndef container_of
#define container_of(ptr, type, member) ((type *)( \
                (char *)(ptr) - (char *)(&((type *)0)->member) ) )
#endif
/*--- function definitions --------------------------------------------------*/

/*
 * Try to flush pending frames in the tx queues
 * no matter it's queued in the TX scheduler or not.
 */
static inline void
ol_tx_queue_vdev_flush(struct ol_txrx_pdev_t *pdev, struct ol_txrx_vdev_t *vdev)
{
#define PEER_ARRAY_COUNT        10
    struct ol_tx_frms_queue_t *txq;
    struct ol_txrx_peer_t *peer, *peers[PEER_ARRAY_COUNT];
    int i, j, peer_count;

    /* flush VDEV TX queues */
    for (i = 0; i < OL_TX_VDEV_NUM_QUEUES; i++) {
        txq = &vdev->txqs[i];
        ol_tx_queue_free(pdev, txq, (i + OL_TX_NUM_TIDS));
    }
    /* flush PEER TX queues */
    do {
        peer_count = 0;
        /* select candidate peers */
        adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
        TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
            for (i = 0; i < OL_TX_NUM_TIDS; i++) {
                txq = &peer->txqs[i];
                if (txq->frms) {
                    adf_os_atomic_inc(&peer->ref_cnt);
                    peers[peer_count++] = peer;
                    break;
                }
            }
            if (peer_count >= PEER_ARRAY_COUNT) {
                break;
            }
        }
        adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
        /* flush TX queues of candidate peers */
        for (i = 0; i < peer_count; i++) {
            for (j = 0; j < OL_TX_NUM_TIDS; j++) {
                txq = &peers[i]->txqs[j];
                if (txq->frms) {
                    ol_tx_queue_free(pdev, txq, j);
                }
            }
            TXRX_PRINT(TXRX_PRINT_LEVEL_ERR,
                      "%s: Delete Peer %p\n", __func__, peer);
            ol_txrx_peer_unref_delete(peers[i]);
        }
    } while (peer_count >= PEER_ARRAY_COUNT);
}

static inline void
ol_tx_queue_flush(struct ol_txrx_pdev_t *pdev)
{
    struct ol_txrx_vdev_t *vdev;

    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
        ol_tx_queue_vdev_flush(pdev, vdev);
    }
}

void
ol_tx_queue_discard(
    struct ol_txrx_pdev_t *pdev,
    a_bool_t flush_all,
    ol_tx_desc_list *tx_descs)
{
    u_int16_t num;
    u_int16_t discarded, actual_discarded = 0;

    adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);

    if (flush_all == A_TRUE) {
        /* flush all the pending tx queues in the scheduler */
        num = ol_tx_desc_pool_size_hl(pdev->ctrl_pdev) -
            adf_os_atomic_read(&pdev->tx_queue.rsrc_cnt);
    } else {
        num = pdev->tx_queue.rsrc_threshold_hi -
            pdev->tx_queue.rsrc_threshold_lo;
    }
    TX_SCHED_DEBUG_PRINT("+%s : %u\n,", __FUNCTION__,
                            adf_os_atomic_read(&pdev->tx_queue.rsrc_cnt));
    while (num > 0) {
        discarded = ol_tx_sched_discard_select(
            pdev, (u_int16_t)num, tx_descs, flush_all);
        if (discarded == 0) {
             /*
              * No more packets could be discarded.
              * Probably tx queues are empty.
              */
             break;
        }
        num -= discarded;
        actual_discarded += discarded;
    }
    adf_os_atomic_add(actual_discarded, &pdev->tx_queue.rsrc_cnt);
    TX_SCHED_DEBUG_PRINT("-%s \n",__FUNCTION__);

    adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);

    if (flush_all == A_TRUE && num > 0) {
        /*
         * try to flush pending frames in the tx queues
         * which are not queued in the TX scheduler.
         */
        ol_tx_queue_flush(pdev);
    }
}

void
ol_tx_enqueue(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    struct ol_tx_desc_t *tx_desc,
    struct ol_txrx_msdu_info_t *tx_msdu_info)
{
    int bytes;
    struct ol_tx_sched_notify_ctx_t notify_ctx;
#if defined(CONFIG_PER_VDEV_TX_DESC_POOL)
    ol_txrx_vdev_handle vdev;
#endif

    TX_SCHED_DEBUG_PRINT("Enter %s\n", __func__);

    /*
     * If too few tx descriptors are available, drop some currently-queued
     * tx frames, to provide enough tx descriptors for new frames, which
     * may be higher priority than the current frames.
     */
#if defined(CONFIG_PER_VDEV_TX_DESC_POOL)
    vdev = tx_desc->vdev;
    if (adf_os_atomic_read(&vdev->tx_desc_count) >
          ((ol_tx_desc_pool_size_hl(pdev->ctrl_pdev) >> 1)
           - TXRX_HL_TX_FLOW_CTRL_MGMT_RESERVED)) {
#else
    if (adf_os_atomic_read(&pdev->tx_queue.rsrc_cnt) <=
        pdev->tx_queue.rsrc_threshold_lo)
    {
#endif
        ol_tx_desc_list tx_descs;
        TAILQ_INIT(&tx_descs);
        ol_tx_queue_discard(pdev, A_FALSE, &tx_descs);
        //Discard Frames in Discard List
        ol_tx_desc_frame_list_free(pdev, &tx_descs, 1 /* error */);
    }
    adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);
    TAILQ_INSERT_TAIL(&txq->head, tx_desc, tx_desc_list_elem);

    bytes = adf_nbuf_len(tx_desc->netbuf);
    txq->frms++;
    txq->bytes += bytes;
    OL_TX_QUEUE_LOG_ENQUEUE(pdev, tx_msdu_info, 1, bytes);

    if (txq->flag != ol_tx_queue_paused) {
        notify_ctx.event = OL_TX_ENQUEUE_FRAME;
        notify_ctx.frames = 1;
        notify_ctx.bytes = adf_nbuf_len(tx_desc->netbuf);
        notify_ctx.txq = txq;
        notify_ctx.info.tx_msdu_info = tx_msdu_info;
        ol_tx_sched_notify(pdev, &notify_ctx);
        txq->flag = ol_tx_queue_active;
    }

    if (!ETHERTYPE_IS_EAPOL_WAPI(tx_msdu_info->htt.info.ethertype)) {
        OL_TX_QUEUE_ADDBA_CHECK(pdev, txq, tx_msdu_info);
    }
    adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
    TX_SCHED_DEBUG_PRINT("Leave %s\n", __func__);
}

u_int16_t
ol_tx_dequeue(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    ol_tx_desc_list *head,
    u_int16_t max_frames,
    u_int32_t *credit,
    int *bytes)
{
    u_int16_t num_frames;
    int bytes_sum;
    unsigned credit_sum;

    TXRX_ASSERT2(txq->flag != ol_tx_queue_paused);
    TX_SCHED_DEBUG_PRINT("Enter %s\n", __func__);

    if (txq->frms < max_frames) {
        max_frames = txq->frms;
    }
    bytes_sum = 0;
    credit_sum = 0;
    for (num_frames = 0; num_frames < max_frames; num_frames++) {
        unsigned frame_credit;
        struct ol_tx_desc_t *tx_desc;
        tx_desc = TAILQ_FIRST(&txq->head);

        frame_credit = htt_tx_msdu_credit(tx_desc->netbuf);
        if (credit_sum + frame_credit > *credit) {
            break;
        }
        credit_sum += frame_credit;
        bytes_sum += adf_nbuf_len(tx_desc->netbuf);
        TAILQ_REMOVE(&txq->head, tx_desc, tx_desc_list_elem);
        TAILQ_INSERT_TAIL(head, tx_desc, tx_desc_list_elem);
    }
    txq->frms -= num_frames;
    txq->bytes -= bytes_sum;
    /* a paused queue remains paused, regardless of whether it has frames */
    if (txq->frms == 0 && txq->flag == ol_tx_queue_active) {
        txq->flag = ol_tx_queue_empty;
    }
    OL_TX_QUEUE_LOG_DEQUEUE(pdev, txq, num_frames, bytes_sum);
    TX_SCHED_DEBUG_PRINT("Leave %s\n", __func__);

    *bytes = bytes_sum;
    *credit = credit_sum;
    return num_frames;
}

void
ol_tx_queue_free(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    int tid)
{
    int frms = 0, bytes = 0;
    struct ol_tx_desc_t *tx_desc;
    struct ol_tx_sched_notify_ctx_t notify_ctx;
    ol_tx_desc_list tx_tmp_list;

    TAILQ_INIT(&tx_tmp_list);
    TX_SCHED_DEBUG_PRINT("Enter %s\n", __func__);
    adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);

    notify_ctx.event = OL_TX_DELETE_QUEUE;
    notify_ctx.txq = txq;
    notify_ctx.info.ext_tid = tid;
    ol_tx_sched_notify(pdev, &notify_ctx);

    frms = txq->frms;
    tx_desc = TAILQ_FIRST(&txq->head);
    while (txq->frms) {
        bytes += adf_nbuf_len(tx_desc->netbuf);
        txq->frms--;
        tx_desc = TAILQ_NEXT(tx_desc, tx_desc_list_elem);
    }
    OL_TX_QUEUE_LOG_FREE(pdev, txq, tid, frms, bytes);
    txq->bytes -= bytes;
    OL_TX_QUEUE_LOG_FREE(pdev, txq, tid, frms, bytes);
    txq->flag = ol_tx_queue_empty;
    /* txq->head gets reset during the TAILQ_CONCAT call */
    TAILQ_CONCAT(&tx_tmp_list, &txq->head, tx_desc_list_elem);

    adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
    /* free tx frames without holding tx_queue_spinlock */
    adf_os_atomic_add(frms, &pdev->tx_queue.rsrc_cnt);
    while (frms) {
        tx_desc = TAILQ_FIRST(&tx_tmp_list);
        TAILQ_REMOVE(&tx_tmp_list, tx_desc, tx_desc_list_elem);
        ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 0);
        frms--;
    }
    TX_SCHED_DEBUG_PRINT("Leave %s\n", __func__);
}


/*--- queue pause / unpause functions ---------------------------------------*/

static inline void
ol_txrx_peer_tid_pause_base(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    int tid)
{
    struct ol_tx_frms_queue_t *txq = &peer->txqs[tid];

    if (txq->paused_count.total++ == 0) {
        struct ol_tx_sched_notify_ctx_t notify_ctx;

        notify_ctx.event = OL_TX_PAUSE_QUEUE;
        notify_ctx.txq = txq;
        notify_ctx.info.ext_tid = tid;
        ol_tx_sched_notify(pdev, &notify_ctx);
        txq->flag = ol_tx_queue_paused;
    }
}

static inline void
ol_txrx_peer_pause_but_no_mgmt_q_base(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer)
{
    int i;
    for (i = 0; i < OL_TX_MGMT_TID; i++) {
        ol_txrx_peer_tid_pause_base(pdev, peer, i);
    }
}

static inline void
ol_txrx_peer_pause_base(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer)
{
    int i;
    for (i = 0; i < ARRAY_LEN(peer->txqs); i++) {
        ol_txrx_peer_tid_pause_base(pdev, peer, i);
    }
}

static inline void
ol_txrx_peer_tid_unpause_base(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    int tid)
{
    struct ol_tx_frms_queue_t *txq = &peer->txqs[tid];
    /*
     * Don't actually unpause the tx queue until all pause requests
     * have been removed.
     */
    TXRX_ASSERT2(txq->paused_count.total > 0);
    /* return, if not already paused */
    if (txq->paused_count.total == 0)
        return;

    if (--txq->paused_count.total == 0) {
        struct ol_tx_sched_notify_ctx_t notify_ctx;

        notify_ctx.event = OL_TX_UNPAUSE_QUEUE;
        notify_ctx.txq = txq;
        notify_ctx.info.ext_tid = tid;
        ol_tx_sched_notify(pdev, &notify_ctx);

        if (txq->frms == 0) {
            txq->flag = ol_tx_queue_empty;
        } else {
            txq->flag = ol_tx_queue_active;
            /*
             * Now that the are new tx frames available to download,
             * invoke the scheduling function, to see if it wants to
             * download the new frames.
             * Since the queue lock is currently held, and since
             * the scheduler function takes the lock, temporarily
             * release the lock.
             */
            adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
            ol_tx_sched(pdev);
            adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);
        }
    }
}

static inline void
ol_txrx_peer_unpause_but_no_mgmt_q_base(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer)
{
    int i;
    for (i = 0; i < OL_TX_MGMT_TID; i++) {
        ol_txrx_peer_tid_unpause_base(pdev, peer, i);
    }
}

void
ol_txrx_peer_tid_unpause(ol_txrx_peer_handle peer, int tid)
{
    struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;

    /* TO DO: log the queue unpause */

    /* acquire the mutex lock, since we'll be modifying the queues */
    TX_SCHED_DEBUG_PRINT("Enter %s\n", __func__);
    adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);

    if (tid == -1) {
        int i;
        for (i = 0; i < ARRAY_LEN(peer->txqs); i++) {
            ol_txrx_peer_tid_unpause_base(pdev, peer, i);
        }
    } else {
        ol_txrx_peer_tid_unpause_base(pdev, peer, tid);
    }

    adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
    TX_SCHED_DEBUG_PRINT("Leave %s\n", __func__);
}

void
ol_txrx_throttle_pause(ol_txrx_pdev_handle pdev)
{
#if defined(QCA_SUPPORT_TX_THROTTLE)
    adf_os_spin_lock_bh(&pdev->tx_throttle.mutex);

    if (pdev->tx_throttle.is_paused == TRUE) {
        adf_os_spin_unlock_bh(&pdev->tx_throttle.mutex);
        return;
    }

    pdev->tx_throttle.is_paused = TRUE;
    adf_os_spin_unlock_bh(&pdev->tx_throttle.mutex);
#endif
    ol_txrx_pdev_pause(pdev, 0);
}

void
ol_txrx_throttle_unpause(ol_txrx_pdev_handle pdev)
{
#if defined(QCA_SUPPORT_TX_THROTTLE)
    adf_os_spin_lock_bh(&pdev->tx_throttle.mutex);

    if (pdev->tx_throttle.is_paused == FALSE) {
        adf_os_spin_unlock_bh(&pdev->tx_throttle.mutex);
        return;
    }

    pdev->tx_throttle.is_paused = FALSE;
    adf_os_spin_unlock_bh(&pdev->tx_throttle.mutex);
#endif
    ol_txrx_pdev_unpause(pdev, 0);
}
#endif /* defined(CONFIG_HL_SUPPORT) */

#if defined(CONFIG_HL_SUPPORT) || defined(QCA_SUPPORT_TXRX_VDEV_PAUSE_LL)
/**
 * ol_txrx_pdev_pause() - Suspend all tx data for the specified physical device.
 * @data_pdev: the physical device being paused.
 * @reason:  pause reason.
 *
 * This function applies to HL systems -
 * in LL systems, applies when txrx_vdev_pause_all is enabled.
 * In some systems it is necessary to be able to temporarily
 * suspend all WLAN traffic, e.g. to allow another device such as bluetooth
 * to temporarily have exclusive access to shared RF chain resources.
 * This function suspends tx traffic within the specified physical device.
 *
 *
 * Return: None
 */
void
ol_txrx_pdev_pause(ol_txrx_pdev_handle pdev, u_int32_t reason)
{
	struct ol_txrx_vdev_t *vdev = NULL, *tmp;

	TAILQ_FOREACH_SAFE(vdev, &pdev->vdev_list, vdev_list_elem, tmp) {
		ol_txrx_vdev_pause(vdev, reason);
	}
}

/**
 * ol_txrx_pdev_unpause() - Resume tx for the specified physical device..
 * @data_pdev: the physical device being paused.
 * @reason:  pause reason.
 *
 *  This function applies to HL systems -
 *  in LL systems, applies when txrx_vdev_pause_all is enabled.
 *
 *
 * Return: None
 */
void
ol_txrx_pdev_unpause(ol_txrx_pdev_handle pdev, u_int32_t reason)
{
	struct ol_txrx_vdev_t *vdev = NULL, *tmp;

	TAILQ_FOREACH_SAFE(vdev, &pdev->vdev_list, vdev_list_elem, tmp) {
		ol_txrx_vdev_unpause(vdev, reason);
	}
}

#ifdef QCA_BAD_PEER_TX_FLOW_CL

/**
 * ol_txrx_peer_bal_add_limit_peer() - add one peer into limit list
 * @pdev:		Pointer to PDEV structure.
 * @peer_id:	Peer Identifier.
 * @peer_limit	Peer limit threshold
 *
 * Add one peer into the limit list of pdev
 * Note that the peer limit info will be also updated
 * If it is the first time, start the timer
 *
 * Return: None
 */
void
ol_txrx_peer_bal_add_limit_peer(struct ol_txrx_pdev_t *pdev,
			u_int16_t peer_id, u_int16_t peer_limit)
{
	u_int16_t i, existed = 0;
	struct ol_txrx_peer_t *peer = NULL;

	for (i = 0; i < pdev->tx_peer_bal.peer_num; i++){
		if (pdev->tx_peer_bal.limit_list[i].peer_id == peer_id) {
			existed = 1;
			break;
		}
	}

	if (!existed) {
		u_int32_t peer_num = pdev->tx_peer_bal.peer_num;
		/* Check if peer_num has reached the capabilit */
		if (peer_num >= MAX_NO_PEERS_IN_LIMIT) {
			TX_SCHED_DEBUG_PRINT_ALWAYS(
				"reach the maxinum peer num %d\n",
				peer_num);
				return;
		}

		pdev->tx_peer_bal.limit_list[peer_num].peer_id = peer_id;
		pdev->tx_peer_bal.limit_list[peer_num].limit_flag = TRUE;
		pdev->tx_peer_bal.limit_list[peer_num].limit = peer_limit;
		pdev->tx_peer_bal.peer_num++;

		peer = ol_txrx_peer_find_by_id(pdev, peer_id);
		if (peer) {
			peer->tx_limit_flag = TRUE;
			peer->tx_limit = peer_limit;
		}

		TX_SCHED_DEBUG_PRINT_ALWAYS(
			"Add one peer into limit queue, peer_id %d, cur peer num %d\n",
			peer_id,
			pdev->tx_peer_bal.peer_num);
	}

	/* Only start the timer once */
	if (pdev->tx_peer_bal.peer_bal_timer_state ==
					ol_tx_peer_bal_timer_inactive) {
		adf_os_timer_start(&pdev->tx_peer_bal.peer_bal_timer,
				pdev->tx_peer_bal.peer_bal_period_ms);
		pdev->tx_peer_bal.peer_bal_timer_state =
				ol_tx_peer_bal_timer_active;
	}
}

/**
 * ol_txrx_peer_bal_remove_limit_peer() - remove one peer from limit list
 * @pdev:		Pointer to PDEV structure.
 * @peer_id:	Peer Identifier.
 *
 * Remove one peer from the limit list of pdev
 * Note that Only stop the timer if no peer in limit state
 *
 * Return: NULL
 */
void
ol_txrx_peer_bal_remove_limit_peer(struct ol_txrx_pdev_t *pdev,
			u_int16_t peer_id)
{
	u_int16_t i;
	struct ol_txrx_peer_t *peer = NULL;

	for (i = 0; i < pdev->tx_peer_bal.peer_num; i++) {
		if ( pdev->tx_peer_bal.limit_list[i].peer_id == peer_id) {
			pdev->tx_peer_bal.limit_list[i] =
				pdev->tx_peer_bal.limit_list[pdev->tx_peer_bal.peer_num - 1];
			pdev->tx_peer_bal.peer_num--;

			peer = ol_txrx_peer_find_by_id(pdev, peer_id);
			if (peer) {
				peer->tx_limit_flag = FALSE;
			}

			TX_SCHED_DEBUG_PRINT(
				"Remove one peer from limitq, peer_id %d, cur peer num %d\n",
				peer_id,
				pdev->tx_peer_bal.peer_num);
			break;
		}
	}

	/* Only stop the timer if no peer in limit state */
	if (pdev->tx_peer_bal.peer_num == 0) {
		adf_os_timer_cancel(&pdev->tx_peer_bal.peer_bal_timer);
		pdev->tx_peer_bal.peer_bal_timer_state =
				ol_tx_peer_bal_timer_inactive;
	}
}

void
ol_txrx_peer_pause_but_no_mgmt_q(ol_txrx_peer_handle peer)
{
	struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;

	/* TO DO: log the queue pause */

	/* acquire the mutex lock, since we'll be modifying the queues */
	TX_SCHED_DEBUG_PRINT("Enter %s\n", __func__);
	adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);

	ol_txrx_peer_pause_but_no_mgmt_q_base(pdev, peer);

	adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
	TX_SCHED_DEBUG_PRINT("Leave %s\n", __func__);
}

void
ol_txrx_peer_unpause_but_no_mgmt_q(ol_txrx_peer_handle peer)
{
	struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;

	/* TO DO: log the queue pause */

	/* acquire the mutex lock, since we'll be modifying the queues */
	TX_SCHED_DEBUG_PRINT("Enter %s\n", __func__);
	adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);

	ol_txrx_peer_unpause_but_no_mgmt_q_base(pdev, peer);

	adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
	TX_SCHED_DEBUG_PRINT("Leave %s\n", __func__);
}

u_int16_t
ol_tx_bad_peer_dequeue_check(struct ol_tx_frms_queue_t *txq,
	u_int16_t max_frames,
	u_int16_t *tx_limit_flag)
{
	if (txq && (txq->peer) && (txq->peer->tx_limit_flag)
			&& (txq->peer->tx_limit < max_frames)) {
		TX_SCHED_DEBUG_PRINT("Peer ID %d goes to limit, threshold is %d\n",
			txq->peer->peer_ids[0], txq->peer->tx_limit);
		*tx_limit_flag = 1;
		return txq->peer->tx_limit;
	} else {
		return max_frames;
	}
}

void
ol_tx_bad_peer_update_tx_limit(struct ol_txrx_pdev_t *pdev,
	struct ol_tx_frms_queue_t *txq,
	u_int16_t frames,
	u_int16_t tx_limit_flag)
{
    adf_os_spin_lock_bh(&pdev->tx_peer_bal.mutex);
    if (txq && tx_limit_flag && (txq->peer) && (txq->peer->tx_limit_flag)) {
        if (txq->peer->tx_limit < frames) {
            txq->peer->tx_limit = 0;
        } else {
            txq->peer->tx_limit -= frames;
        }
        TX_SCHED_DEBUG_PRINT_ALWAYS("Peer ID %d in limit, deque %d frms\n",
			txq->peer->peer_ids[0], frames);
    } else if (txq->peer) {
        TX_SCHED_DEBUG_PRINT("Download peer_id %d, num_frames %d\n",
			txq->peer->peer_ids[0], frames);
    }
    adf_os_spin_unlock_bh(&pdev->tx_peer_bal.mutex);
}

void
ol_txrx_bad_peer_txctl_set_setting(struct ol_txrx_pdev_t *pdev,
			int enable, int period, int txq_limit)
{
	if (enable) {
		pdev->tx_peer_bal.enabled = ol_tx_peer_bal_enable;
	} else {
		pdev->tx_peer_bal.enabled = ol_tx_peer_bal_disable;
	}
	/* Set the current settingl */
	pdev->tx_peer_bal.peer_bal_period_ms = period;
	pdev->tx_peer_bal.peer_bal_txq_limit = txq_limit;
}

void
ol_txrx_bad_peer_txctl_update_threshold(struct ol_txrx_pdev_t *pdev,
			int level, int tput_thresh, int tx_limit)
{
	/* Set the current settingl */
	pdev->tx_peer_bal.ctl_thresh[level].tput_thresh =
			tput_thresh;
	pdev->tx_peer_bal.ctl_thresh[level].tx_limit =
			tx_limit;
}

void
ol_tx_pdev_peer_bal_timer(void *context)
{
	int i;
	struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)context;

	adf_os_spin_lock_bh(&pdev->tx_peer_bal.mutex);

	for (i = 0; i < pdev->tx_peer_bal.peer_num; i++) {
		if (pdev->tx_peer_bal.limit_list[i].limit_flag) {
			u_int16_t peer_id =
				pdev->tx_peer_bal.limit_list[i].peer_id;
			u_int16_t tx_limit =
				pdev->tx_peer_bal.limit_list[i].limit;

			struct ol_txrx_peer_t *peer = NULL;
			peer = ol_txrx_peer_find_by_id(pdev, peer_id);
			TX_SCHED_DEBUG_PRINT("%s peer_id %d  peer = 0x%x tx limit %d\n",
					__FUNCTION__, peer_id,
					(int)peer, tx_limit);

			/* It is possible the peer limit is still not 0,
			   but it is the scenario should not be cared */
			if (peer) {
				peer->tx_limit = tx_limit;
			} else {
				ol_txrx_peer_bal_remove_limit_peer(pdev,
								peer_id);
				TX_SCHED_DEBUG_PRINT_ALWAYS("No such a peer, peer id = %d\n",
					peer_id);
			}
		}
	}

	adf_os_spin_unlock_bh(&pdev->tx_peer_bal.mutex);

	if (pdev->tx_peer_bal.peer_num) {
		ol_tx_sched(pdev);
		adf_os_timer_start(&pdev->tx_peer_bal.peer_bal_timer,
				pdev->tx_peer_bal.peer_bal_period_ms);
	}
}

void
ol_txrx_set_txq_peer(
	struct ol_tx_frms_queue_t *txq,
	struct ol_txrx_peer_t *peer)
{
	if (txq) {
		txq->peer = peer;
	}
}

void ol_tx_badpeer_flow_cl_init(struct ol_txrx_pdev_t *pdev)
{
	u_int32_t timer_period;

	adf_os_spinlock_init(&pdev->tx_peer_bal.mutex);
	pdev->tx_peer_bal.peer_num = 0;
	pdev->tx_peer_bal.peer_bal_timer_state
		= ol_tx_peer_bal_timer_inactive;

	timer_period = 2000;
	pdev->tx_peer_bal.peer_bal_period_ms = timer_period;

	adf_os_timer_init(
			pdev->osdev,
			&pdev->tx_peer_bal.peer_bal_timer,
			ol_tx_pdev_peer_bal_timer,
			pdev);
}

void ol_tx_badpeer_flow_cl_deinit(struct ol_txrx_pdev_t *pdev)
{
	adf_os_timer_cancel(&pdev->tx_peer_bal.peer_bal_timer);
	pdev->tx_peer_bal.peer_bal_timer_state =
					ol_tx_peer_bal_timer_inactive;
	adf_os_timer_free(&pdev->tx_peer_bal.peer_bal_timer);
	adf_os_spinlock_destroy(&pdev->tx_peer_bal.mutex);
}

void
ol_txrx_peer_link_status_handler(
    ol_txrx_pdev_handle pdev,
    u_int16_t peer_num,
    struct rate_report_t* peer_link_status)
{
	u_int16_t i = 0;
	struct ol_txrx_peer_t *peer = NULL;

	if (NULL == pdev) {
		TX_SCHED_DEBUG_PRINT_ALWAYS("Error: NULL pdev handler \n");
		return;
	}

	if (NULL == peer_link_status) {
		TX_SCHED_DEBUG_PRINT_ALWAYS(
			"Error:NULL link report message. peer num %d\n",
			peer_num);
		return;
	}

	/* Check if bad peer tx flow CL is enabled */
	if (pdev->tx_peer_bal.enabled != ol_tx_peer_bal_enable){
		TX_SCHED_DEBUG_PRINT_ALWAYS(
			"Bad peer tx flow CL is not enabled, ignore it\n");
		return;
	}

	/* Check peer_num is reasonable */
	if (peer_num > MAX_NO_PEERS_IN_LIMIT){
		TX_SCHED_DEBUG_PRINT_ALWAYS(
			"%s: Bad peer_num %d \n", __func__, peer_num);
		return;
	}

	VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_DEBUG,
		  "%s: peer_num %d", __func__, peer_num);

	for (i = 0; i < peer_num; i++) {
		u_int16_t peer_limit, peer_id;
		u_int16_t pause_flag, unpause_flag;
		u_int32_t peer_phy, peer_tput;

		peer_id = peer_link_status->id;
		peer_phy = peer_link_status->phy;
		peer_tput = peer_link_status->rate;

		TX_SCHED_DEBUG_PRINT("%s: peer id %d tput %d phy %d\n",
				__func__, peer_id, peer_tput, peer_phy);

		/* Sanity check for the PHY mode value */
		if (peer_phy > TXRX_IEEE11_AC) {
			TX_SCHED_DEBUG_PRINT_ALWAYS(
				"%s: PHY value is illegal: %d, and the peer_id %d \n",
				__func__, peer_link_status->phy, peer_id);
			continue;
		}
		pause_flag   = FALSE;
		unpause_flag = FALSE;
		peer_limit   = 0;

		/* From now on, PHY, PER info should be all fine */
		adf_os_spin_lock_bh(&pdev->tx_peer_bal.mutex);

		/* Update link status analysis for each peer */
		peer = ol_txrx_peer_find_by_id(pdev, peer_id);
		if (peer) {
			u_int32_t thresh, limit, phy;
			phy = peer_link_status->phy;
			thresh = pdev->tx_peer_bal.ctl_thresh[phy].tput_thresh;
			limit = pdev->tx_peer_bal.ctl_thresh[phy].tx_limit;

			if (((peer->tx_pause_flag) || (peer->tx_limit_flag))
				&& (peer_tput) && (peer_tput < thresh)) {
				peer_limit = limit;
			}

			if (peer_limit) {
				ol_txrx_peer_bal_add_limit_peer(pdev, peer_id,
					peer_limit);
			} else if (pdev->tx_peer_bal.peer_num) {
				TX_SCHED_DEBUG_PRINT("%s: Check if peer_id %d exit limit\n",
						__func__, peer_id);
				ol_txrx_peer_bal_remove_limit_peer(pdev, peer_id);
			}
			if ((peer_tput == 0) && (peer->tx_pause_flag == FALSE)) {
				peer->tx_pause_flag = TRUE;
				pause_flag = TRUE;
			} else if (peer->tx_pause_flag){
				unpause_flag = TRUE;
				peer->tx_pause_flag = FALSE;
			}
		} else {
			TX_SCHED_DEBUG_PRINT("%s: Remove peer_id %d from limit list\n",
						__func__, peer_id);
			ol_txrx_peer_bal_remove_limit_peer(pdev, peer_id);
		}

		peer_link_status++;
		adf_os_spin_unlock_bh(&pdev->tx_peer_bal.mutex);
		if (pause_flag) {
			ol_txrx_peer_pause_but_no_mgmt_q(peer);
		} else if (unpause_flag) {
			ol_txrx_peer_unpause_but_no_mgmt_q(peer);
		}
	}
}
#endif /* QCA_BAD_PEER_TX_FLOW_CL */

void
ol_txrx_vdev_pause(ol_txrx_vdev_handle vdev, u_int32_t reason)
{
    /* TO DO: log the queue pause */
    /* acquire the mutex lock, since we'll be modifying the queues */
    TX_SCHED_DEBUG_PRINT("Enter %s\n", __func__);

    if (vdev->pdev->cfg.is_high_latency) {
#if defined(CONFIG_HL_SUPPORT)
        struct ol_txrx_pdev_t *pdev = vdev->pdev;
        struct ol_txrx_peer_t *peer;
        /* use peer_ref_mutex before accessing peer_list */
        adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
        adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);
        TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
            ol_txrx_peer_pause_base(pdev, peer);
        }
        adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
        adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
#endif /* defined(CONFIG_HL_SUPPORT) */
    } else {
        adf_os_spin_lock_bh(&vdev->ll_pause.mutex);
        vdev->ll_pause.paused_reason |= reason;
        vdev->ll_pause.q_pause_cnt++;
        vdev->ll_pause.is_q_paused = TRUE;
        adf_os_spin_unlock_bh(&vdev->ll_pause.mutex);
    }

    TX_SCHED_DEBUG_PRINT("Leave %s\n", __func__);
}

void
ol_txrx_vdev_unpause(ol_txrx_vdev_handle vdev, u_int32_t reason)
{
    /* TO DO: log the queue unpause */
    /* acquire the mutex lock, since we'll be modifying the queues */
    TX_SCHED_DEBUG_PRINT("Enter %s\n", __func__);

    if (vdev->pdev->cfg.is_high_latency) {
#if defined(CONFIG_HL_SUPPORT)
        struct ol_txrx_pdev_t *pdev = vdev->pdev;
        struct ol_txrx_peer_t *peer;

        /* take peer_ref_mutex before accessing peer_list */
        adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
        adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);

        TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
            int i;
            for (i = 0; i < ARRAY_LEN(peer->txqs); i++) {
                ol_txrx_peer_tid_unpause_base(pdev, peer, i);
            }
        }
        adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
        adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
#endif /* defined(CONFIG_HL_SUPPORT) */
    } else {
        adf_os_spin_lock_bh(&vdev->ll_pause.mutex);
        if (vdev->ll_pause.paused_reason & reason)
        {
            vdev->ll_pause.paused_reason &= ~reason;
            vdev->ll_pause.is_q_paused = FALSE;
            vdev->ll_pause.q_unpause_cnt++;
#ifdef QCA_SUPPORT_TXRX_VDEV_LL_TXQ
            if (reason == OL_TXQ_PAUSE_REASON_VDEV_SUSPEND) {
                adf_os_spin_unlock_bh(&vdev->ll_pause.mutex);
                ol_tx_vdev_ll_pause_start_timer(vdev);
            }
            else
#endif
            if (!vdev->ll_pause.paused_reason) {
                adf_os_spin_unlock_bh(&vdev->ll_pause.mutex);
                ol_tx_vdev_ll_pause_queue_send(vdev);
            } else {
                adf_os_spin_unlock_bh(&vdev->ll_pause.mutex);
            }
        } else {
            adf_os_spin_unlock_bh(&vdev->ll_pause.mutex);
        }
    }
    TX_SCHED_DEBUG_PRINT("Leave %s\n", __func__);
}

void
ol_txrx_vdev_flush(ol_txrx_vdev_handle vdev)
{
    if (vdev->pdev->cfg.is_high_latency) {
        #if defined(CONFIG_HL_SUPPORT)
        ol_tx_queue_vdev_flush(vdev->pdev, vdev);
        #endif
    } else {
        adf_os_spin_lock_bh(&vdev->ll_pause.mutex);
        adf_os_timer_cancel(&vdev->ll_pause.timer);
        vdev->ll_pause.is_q_timer_on = FALSE;
        while (vdev->ll_pause.txq.head) {
            adf_nbuf_t next = adf_nbuf_next(vdev->ll_pause.txq.head);
            adf_nbuf_set_next(vdev->ll_pause.txq.head, NULL);
            adf_nbuf_unmap(vdev->pdev->osdev, vdev->ll_pause.txq.head,
                           ADF_OS_DMA_TO_DEVICE);
            adf_nbuf_tx_free(vdev->ll_pause.txq.head, 1 /* error */);
            vdev->ll_pause.txq.head = next;
        }
        vdev->ll_pause.txq.tail = NULL;
        vdev->ll_pause.txq.depth = 0;
        adf_os_spin_unlock_bh(&vdev->ll_pause.mutex);
    }
}

#endif  // defined(CONFIG_HL_SUPPORT) || defined(QCA_SUPPORT_TXRX_VDEV_PAUSE_LL)

/*--- LL tx throttle queue code --------------------------------------------*/
#if defined(QCA_SUPPORT_TX_THROTTLE)
u_int8_t ol_tx_pdev_is_target_empty(void)
{
    /* TM TODO */
    return 1;
}

void ol_tx_pdev_throttle_phase_timer(void *context)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)context;
    int ms = 0;
    throttle_level cur_level;
    throttle_phase cur_phase;

    /* update the phase */
    pdev->tx_throttle.current_throttle_phase++;

    if (pdev->tx_throttle.current_throttle_phase == THROTTLE_PHASE_MAX) {
        pdev->tx_throttle.current_throttle_phase = THROTTLE_PHASE_OFF;
    }

    if (pdev->tx_throttle.current_throttle_phase == THROTTLE_PHASE_OFF) {
        if (ol_tx_pdev_is_target_empty(/*pdev*/)) {
            TXRX_PRINT(TXRX_PRINT_LEVEL_WARN, "throttle phase --> OFF\n");

            if (pdev->cfg.is_high_latency)
                ol_txrx_throttle_pause(pdev);

            cur_level = pdev->tx_throttle.current_throttle_level;
            cur_phase = pdev->tx_throttle.current_throttle_phase;
            ms = pdev->tx_throttle.throttle_time_ms[cur_level][cur_phase];
            if (pdev->tx_throttle.current_throttle_level !=
                THROTTLE_LEVEL_0) {
                TXRX_PRINT(TXRX_PRINT_LEVEL_WARN, "start timer %d ms\n", ms);
                adf_os_timer_start(&pdev->tx_throttle.phase_timer, ms);
            }
        }
    }
    else /* THROTTLE_PHASE_ON */
    {
        TXRX_PRINT(TXRX_PRINT_LEVEL_WARN, "throttle phase --> ON\n");

        if (pdev->cfg.is_high_latency)
            ol_txrx_throttle_unpause(pdev);
#ifdef QCA_SUPPORT_TXRX_VDEV_LL_TXQ
        else
            ol_tx_pdev_ll_pause_queue_send_all(pdev);
#endif

        cur_level = pdev->tx_throttle.current_throttle_level;
        cur_phase = pdev->tx_throttle.current_throttle_phase;
        ms = pdev->tx_throttle.throttle_time_ms[cur_level][cur_phase];
        if (pdev->tx_throttle.current_throttle_level != THROTTLE_LEVEL_0) {
            TXRX_PRINT(TXRX_PRINT_LEVEL_WARN, "start timer %d ms\n", ms);
            adf_os_timer_start(&pdev->tx_throttle.phase_timer, ms);
        }
    }
}

#ifdef QCA_SUPPORT_TXRX_VDEV_LL_TXQ
void ol_tx_pdev_throttle_tx_timer(void *context)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)context;
    ol_tx_pdev_ll_pause_queue_send_all(pdev);
}
#endif

void ol_tx_throttle_set_level(struct ol_txrx_pdev_t *pdev, int level)
{
    int ms = 0;

    if (level >= THROTTLE_LEVEL_MAX) {
        TXRX_PRINT(TXRX_PRINT_LEVEL_WARN,
                    "%s invalid throttle level set %d, ignoring\n",
                    __func__, level);
        return;
    }

    TXRX_PRINT(TXRX_PRINT_LEVEL_ERR, "Setting throttle level %d\n", level);

    /* Set the current throttle level */
    pdev->tx_throttle.current_throttle_level = (throttle_level)level;

    if (pdev->cfg.is_high_latency) {

        adf_os_timer_cancel(&pdev->tx_throttle.phase_timer);

        /* Set the phase */
        if (level != THROTTLE_LEVEL_0) {
            pdev->tx_throttle.current_throttle_phase = THROTTLE_PHASE_OFF;
            ms = pdev->tx_throttle.throttle_time_ms[level][THROTTLE_PHASE_OFF];
            /* pause all */
            ol_txrx_throttle_pause(pdev);
        } else {
            pdev->tx_throttle.current_throttle_phase = THROTTLE_PHASE_ON;
            ms = pdev->tx_throttle.throttle_time_ms[level][THROTTLE_PHASE_ON];
            /* unpause all */
            ol_txrx_throttle_unpause(pdev);
        }
    } else {
        /* Reset the phase */
        pdev->tx_throttle.current_throttle_phase = THROTTLE_PHASE_OFF;

        /* Start with the new time */
        ms = pdev->tx_throttle.throttle_time_ms[level][THROTTLE_PHASE_OFF];

        adf_os_timer_cancel(&pdev->tx_throttle.phase_timer);
    }

    if (level != THROTTLE_LEVEL_0) {
        adf_os_timer_start(&pdev->tx_throttle.phase_timer, ms);
    }
}

/* This table stores the duty cycle for each level.
   Example "on" time for level 2 with duty period 100ms is:
   "on" time = duty_period_ms >> throttle_duty_cycle_table[2]
   "on" time = 100 ms >> 2 = 25ms */
static u_int8_t g_throttle_duty_cycle_table[THROTTLE_LEVEL_MAX] =
{ 0, 1, 2, 4 };

void ol_tx_throttle_init_period(struct ol_txrx_pdev_t *pdev, int period)
{
    int i;

    /* Set the current throttle level */
    pdev->tx_throttle.throttle_period_ms = period;

    TXRX_PRINT(TXRX_PRINT_LEVEL_WARN, "level  OFF  ON\n");
    for (i = 0; i < THROTTLE_LEVEL_MAX; i++) {
        pdev->tx_throttle.throttle_time_ms[i][THROTTLE_PHASE_ON] =
                pdev->tx_throttle.throttle_period_ms >>
            g_throttle_duty_cycle_table[i];
        pdev->tx_throttle.throttle_time_ms[i][THROTTLE_PHASE_OFF] =
            pdev->tx_throttle.throttle_period_ms -
            pdev->tx_throttle.throttle_time_ms[i][THROTTLE_PHASE_ON];
        TXRX_PRINT(TXRX_PRINT_LEVEL_WARN, "%d      %d    %d\n", i,
                   pdev->tx_throttle.throttle_time_ms[i][THROTTLE_PHASE_OFF],
                   pdev->tx_throttle.throttle_time_ms[i][THROTTLE_PHASE_ON]);
    }
}

void ol_tx_throttle_init(struct ol_txrx_pdev_t *pdev)
{
    u_int32_t throttle_period;

    pdev->tx_throttle.current_throttle_level = THROTTLE_LEVEL_0;
    pdev->tx_throttle.current_throttle_phase = THROTTLE_PHASE_OFF;
    adf_os_spinlock_init(&pdev->tx_throttle.mutex);

    throttle_period = ol_cfg_throttle_period_ms(pdev->ctrl_pdev);

    ol_tx_throttle_init_period(pdev, throttle_period);

    adf_os_timer_init(
            pdev->osdev,
            &pdev->tx_throttle.phase_timer,
            ol_tx_pdev_throttle_phase_timer,
            pdev, ADF_DEFERRABLE_TIMER);

#ifdef QCA_SUPPORT_TXRX_VDEV_LL_TXQ
    adf_os_timer_init(
            pdev->osdev,
            &pdev->tx_throttle.tx_timer,
            ol_tx_pdev_throttle_tx_timer,
            pdev, ADF_DEFERRABLE_TIMER);
#endif

    pdev->tx_throttle.tx_threshold = THROTTLE_TX_THRESHOLD;
}
#endif /* QCA_SUPPORT_TX_THROTTLE */
/*--- End of LL tx throttle queue code ---------------------------------------*/

#if defined(CONFIG_HL_SUPPORT)

/*--- ADDBA triggering functions --------------------------------------------*/


/*=== debug functions =======================================================*/

/*--- queue event log -------------------------------------------------------*/

#if defined(DEBUG_HL_LOGGING)

static void
ol_tx_queue_log_entry_type_info(
    u_int8_t *type, int *size, int *align, int var_size)
{
    switch (*type) {
    case ol_tx_log_entry_type_enqueue:
    case ol_tx_log_entry_type_dequeue:
    case ol_tx_log_entry_type_queue_free:
        *size = sizeof(struct ol_tx_log_queue_add_t);
        *align = 2;
        break;

    case ol_tx_log_entry_type_queue_state:
        *size = offsetof(struct ol_tx_log_queue_state_var_sz_t, data);
        *align = 4;
        if (var_size) {
            /* read the variable-sized record, to see how large it is */
            int align_pad;
            struct ol_tx_log_queue_state_var_sz_t *record;

            align_pad =
                (*align - ((((u_int32_t) type) + 1))) & (*align - 1);
            record = (struct ol_tx_log_queue_state_var_sz_t *)
                (type + 1 + align_pad);
            *size += record->num_cats_active *
                (sizeof(u_int32_t) /* bytes */ + sizeof(u_int16_t) /* frms */);
        }
        break;

    //case ol_tx_log_entry_type_drop:
    default:
        *size = 0;
        *align = 0;
    };
}

static void
ol_tx_queue_log_oldest_update(struct ol_txrx_pdev_t *pdev, int offset)
{
    int oldest_record_offset;

    /*
     * If the offset of the oldest record is between the current and
     * new values of the offset of the newest record, then the oldest
     * record has to be dropped from the log to provide room for the
     * newest record.
     * Advance the offset of the oldest record until it points to a
     * record that is beyond the new value of the offset of the newest
     * record.
     */
    if (!pdev->txq_log.wrapped) {
        /*
         * The log has not even filled up yet - no need to remove
         * the oldest record to make room for a new record.
         */
        return;
    }

    if (offset > pdev->txq_log.offset) {
        /*
         * not wraparound -
         * The oldest record offset may have already wrapped around,
         * even if the newest record has not.  In this case, then
         * the oldest record offset is fine where it is.
         */
        if (pdev->txq_log.oldest_record_offset == 0) {
            return;
        }
        oldest_record_offset = pdev->txq_log.oldest_record_offset;
    } else {
        /* wraparound */
        oldest_record_offset = 0;
    }

    while (oldest_record_offset < offset) {
        int size, align, align_pad;
        u_int8_t type;

        type = pdev->txq_log.data[oldest_record_offset];
        if (type == ol_tx_log_entry_type_wrap) {
            oldest_record_offset = 0;
            break;
        }
        ol_tx_queue_log_entry_type_info(
            &pdev->txq_log.data[oldest_record_offset], &size, &align, 1);
        align_pad =
            (align - ((oldest_record_offset + 1/*type*/))) & (align - 1);
        /*
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
            "TXQ LOG old alloc: offset %d, type %d, size %d (%d)\n",
            oldest_record_offset, type, size, size + 1 + align_pad);
         */
        oldest_record_offset += size + 1 + align_pad;
    }
    if (oldest_record_offset >= pdev->txq_log.size) {
        oldest_record_offset = 0;
    }
    pdev->txq_log.oldest_record_offset = oldest_record_offset;
}

void*
ol_tx_queue_log_alloc(
    struct ol_txrx_pdev_t *pdev,
    u_int8_t type /* ol_tx_log_entry_type */,
    int extra_bytes)
{
    int size, align, align_pad;
    int offset;

    ol_tx_queue_log_entry_type_info(&type, &size, &align, 0);
    size += extra_bytes;

    offset = pdev->txq_log.offset;
    align_pad = (align - ((offset + 1/*type*/))) & (align - 1);

    if (pdev->txq_log.size - offset >= size + 1 + align_pad) {
        /* no need to wrap around */
        goto alloc_found;
    }
    if (! pdev->txq_log.allow_wrap) {
        return NULL; /* log is full and can't wrap */
    }
    /* handle wrap-around */
    pdev->txq_log.wrapped = 1;
    offset = 0;
    align_pad = (align - ((offset + 1/*type*/))) & (align - 1);
    /* sanity check that the log is large enough to hold this entry */
    if (pdev->txq_log.size <= size + 1 + align_pad) {
        return NULL;
    }

alloc_found:
    ol_tx_queue_log_oldest_update(pdev, offset + size + 1 + align_pad);
    if (offset == 0)  {
        pdev->txq_log.data[pdev->txq_log.offset] = ol_tx_log_entry_type_wrap;
    }
    /*
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
    "TXQ LOG new alloc: offset %d, type %d, size %d (%d)\n",
        offset, type, size, size + 1 + align_pad);
     */
    pdev->txq_log.data[offset] = type;
    pdev->txq_log.offset = offset + size + 1 + align_pad;
    if (pdev->txq_log.offset >= pdev->txq_log.size) {
        pdev->txq_log.offset = 0;
        pdev->txq_log.wrapped = 1;
    }
    return &pdev->txq_log.data[offset + 1 + align_pad];
}

static int
ol_tx_queue_log_record_display(struct ol_txrx_pdev_t *pdev, int offset)
{
    int size, align, align_pad;
    u_int8_t type;
    struct ol_txrx_peer_t *peer;

    adf_os_spin_lock_bh(&pdev->txq_log_spinlock);
    type = pdev->txq_log.data[offset];
    ol_tx_queue_log_entry_type_info(
        &pdev->txq_log.data[offset], &size, &align, 1);
    align_pad = (align - ((offset + 1/*type*/))) & (align - 1);

    switch (type) {
    case ol_tx_log_entry_type_enqueue:
        {
            struct ol_tx_log_queue_add_t record;
            adf_os_mem_copy(&record,
                            &pdev->txq_log.data[offset + 1 + align_pad],
                            sizeof(struct ol_tx_log_queue_add_t));
            adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);

            if (record.peer_id != 0xffff) {
                peer = ol_txrx_peer_find_by_id(pdev, record.peer_id);
                if (peer != NULL)
                    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                        "       Q: %6d  %5d  %3d  %4d (%02x:%02x:%02x:%02x:%02x:%02x)",
                        record.num_frms, record.num_bytes, record.tid,
                        record.peer_id,
                        peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                        peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                        peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
               else
                    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                        "       Q: %6d  %5d  %3d  %4d",
                        record.num_frms, record.num_bytes,
                        record.tid, record.peer_id);
            } else {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "       Q: %6d  %5d  %3d  from vdev",
                    record.num_frms, record.num_bytes, record.tid);
            }
            break;
        }
    case ol_tx_log_entry_type_dequeue:
        {
            struct ol_tx_log_queue_add_t record;
            adf_os_mem_copy(&record,
                            &pdev->txq_log.data[offset + 1 + align_pad],
                            sizeof(struct ol_tx_log_queue_add_t));
            adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);

            if (record.peer_id != 0xffff) {
                peer = ol_txrx_peer_find_by_id(pdev, record.peer_id);
                if (peer != NULL)
                    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                        "      DQ: %6d  %5d  %3d  %4d (%02x:%02x:%02x:%02x:%02x:%02x)",
                        record.num_frms, record.num_bytes, record.tid,
                        record.peer_id,
                        peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                        peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                        peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
                else
                    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                        "      DQ: %6d  %5d  %3d  %4d",
                        record.num_frms, record.num_bytes,
                        record.tid, record.peer_id);
            } else {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "      DQ: %6d  %5d  %3d  from vdev",
                    record.num_frms, record.num_bytes, record.tid);
            }
            break;
        }
    case ol_tx_log_entry_type_queue_free:
        {
            struct ol_tx_log_queue_add_t record;
            adf_os_mem_copy(&record,
                            &pdev->txq_log.data[offset + 1 + align_pad],
                            sizeof(struct ol_tx_log_queue_add_t));
            adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);

            if (record.peer_id != 0xffff) {
                peer = ol_txrx_peer_find_by_id(pdev, record.peer_id);
                if (peer != NULL)
                    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                        "     F: %6d  %5d  %3d  %4d (%02x:%02x:%02x:%02x:%02x:%02x)",
                        record.num_frms, record.num_bytes, record.tid,
                        record.peer_id,
                        peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                        peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                        peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
                else
                    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                        "      F: %6d  %5d  %3d  %4d",
                        record.num_frms, record.num_bytes,
                        record.tid, record.peer_id);
            } else {
                /* shouldn't happen */
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "Unexpected vdev queue removal\n");
            }
            break;
        }

    case ol_tx_log_entry_type_queue_state:
        {
            int i, j;
            u_int32_t active_bitmap;
            struct ol_tx_log_queue_state_var_sz_t record;
            u_int8_t *data;

            adf_os_mem_copy(&record,
                            &pdev->txq_log.data[offset + 1 + align_pad],
                            sizeof(struct ol_tx_log_queue_state_var_sz_t));
            adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);

            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                "       S: bitmap = %#x",
                 record.active_bitmap);
            data = &record.data[0];
            j = 0;
            i = 0;
            active_bitmap = record.active_bitmap;
            while (active_bitmap) {
                if (active_bitmap & 0x1) {
                    u_int16_t frms;
                    u_int32_t bytes;

                    frms = data[0] | (data[1] << 8);
                    bytes = (data[2] <<  0) | (data[3] <<  8) |
                            (data[4] << 16) | (data[5] << 24);
                    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
                        "  cat %2d: %6d  %5d",
                        i, frms, bytes);
                    data += 6;
                    j++;
                }
                i++;
                active_bitmap >>= 1;
            }
            break;
        }

    //case ol_tx_log_entry_type_drop:

    case ol_tx_log_entry_type_wrap:
        adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
        return -1 * offset; /* go back to the top */

    default:
        adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
            "  *** invalid tx log entry type (%d)\n", type);
        return 0; /* error */
    };

    return size + 1 + align_pad;
}

void
ol_tx_queue_log_display(struct ol_txrx_pdev_t *pdev)
{
    int offset;
    int unwrap;

    adf_os_spin_lock_bh(&pdev->txq_log_spinlock);
    offset = pdev->txq_log.oldest_record_offset;
    unwrap = pdev->txq_log.wrapped;
    adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
    /*
     * In theory, this should use mutex to guard against the offset
     * being changed while in use, but since this is just for debugging,
     * don't bother.
     */
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
        "Tx queue log:");
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_ERROR,
        "      : Frames  Bytes  TID  PEER");

    while (unwrap || offset != pdev->txq_log.offset) {
        int delta = ol_tx_queue_log_record_display(pdev, offset);
        if (delta == 0) {
            return; /* error */
        }
        if (delta < 0) {
            unwrap = 0;
        }
        offset += delta;
    }
}

void
ol_tx_queue_log_enqueue(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_msdu_info_t *msdu_info,
    int frms, int bytes)
{
    int tid;
    u_int16_t peer_id = msdu_info->htt.info.peer_id;
    struct ol_tx_log_queue_add_t *log_elem;
    tid = msdu_info->htt.info.ext_tid;

    adf_os_spin_lock_bh(&pdev->txq_log_spinlock);
    log_elem = ol_tx_queue_log_alloc(pdev, ol_tx_log_entry_type_enqueue, 0);
    if (!log_elem) {
        adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
        return;
    }

    log_elem->num_frms = frms;
    log_elem->num_bytes = bytes;
    log_elem->peer_id = peer_id;
    log_elem->tid = tid;
    adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
}

void
ol_tx_queue_log_dequeue(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    int frms, int bytes)
{
    int ext_tid;
    u_int16_t peer_id;
    struct ol_tx_log_queue_add_t *log_elem;

    ext_tid = txq->ext_tid;
    adf_os_spin_lock_bh(&pdev->txq_log_spinlock);
    log_elem = ol_tx_queue_log_alloc(pdev, ol_tx_log_entry_type_dequeue, 0);
    if (!log_elem) {
        adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
        return;
    }

    if (ext_tid < OL_TX_NUM_TIDS) {
        struct ol_txrx_peer_t *peer;
        struct ol_tx_frms_queue_t *txq_base;

        txq_base = txq - ext_tid;
        peer = container_of(txq_base, struct ol_txrx_peer_t, txqs[0]);
        peer_id = peer->peer_ids[0];
    } else {
        peer_id = ~0;
    }

    log_elem->num_frms = frms;
    log_elem->num_bytes = bytes;
    log_elem->peer_id = peer_id;
    log_elem->tid = ext_tid;
    adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
}

void
ol_tx_queue_log_free(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    int tid, int frms, int bytes)
{
    u_int16_t peer_id;
    struct ol_tx_log_queue_add_t *log_elem;

    adf_os_spin_lock_bh(&pdev->txq_log_spinlock);
    log_elem = ol_tx_queue_log_alloc(pdev, ol_tx_log_entry_type_queue_free, 0);
    if (!log_elem) {
        adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
        return;
    }

    if (tid < OL_TX_NUM_TIDS) {
        struct ol_txrx_peer_t *peer;
        struct ol_tx_frms_queue_t *txq_base;

        txq_base = txq - tid;
        peer = container_of(txq_base, struct ol_txrx_peer_t, txqs[0]);
        peer_id = peer->peer_ids[0];
    } else {
        peer_id = ~0;
    }

    log_elem->num_frms = frms;
    log_elem->num_bytes = bytes;
    log_elem->peer_id = peer_id;
    log_elem->tid = tid;
    adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
}

void
ol_tx_queue_log_sched(
    struct ol_txrx_pdev_t *pdev,
    int credit,
    int *num_cats,
    u_int32_t **active_bitmap,
    u_int8_t  **data)
{
    int data_size;
    struct ol_tx_log_queue_state_var_sz_t *log_elem;

    data_size = sizeof(u_int32_t) /* bytes */ + sizeof(u_int16_t) /* frms */;
    data_size *= *num_cats;

    adf_os_spin_lock_bh(&pdev->txq_log_spinlock);
    log_elem = ol_tx_queue_log_alloc(
        pdev, ol_tx_log_entry_type_queue_state, data_size);
    if (!log_elem) {
        *num_cats = 0;
        adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
        return;
    }
    log_elem->num_cats_active = *num_cats;
    log_elem->active_bitmap = 0;
    log_elem->credit = credit;

    *active_bitmap = &log_elem->active_bitmap;
    *data = &log_elem->data[0];
    adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
}

void
ol_tx_queue_log_clear(struct ol_txrx_pdev_t *pdev)
{
    adf_os_spin_lock_bh(&pdev->txq_log_spinlock);
    adf_os_mem_zero(&pdev->txq_log, sizeof(pdev->txq_log));
    pdev->txq_log.size = OL_TXQ_LOG_SIZE;
    pdev->txq_log.oldest_record_offset = 0;
    pdev->txq_log.offset = 0;
    pdev->txq_log.allow_wrap = 1;
    pdev->txq_log.wrapped = 0;
    adf_os_spin_unlock_bh(&pdev->txq_log_spinlock);
}
#endif /* defined(DEBUG_HL_LOGGING) */

/*--- queue state printouts -------------------------------------------------*/

#if TXRX_DEBUG_LEVEL > 5

void
ol_tx_queue_display(struct ol_tx_frms_queue_t *txq, int indent)
{
    char *state;

    state = (txq->flag == ol_tx_queue_active) ? "active" : "paused";
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "%*stxq %p (%s): %d frms, %d bytes\n",
        indent, " ", txq, state, txq->frms, txq->bytes);
}

void
ol_tx_queues_display(struct ol_txrx_pdev_t *pdev)
{
    struct ol_txrx_vdev_t *vdev;

    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
        "pdev %p tx queues:\n", pdev);
    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
        struct ol_txrx_peer_t *peer;
        int i;
        for (i = 0; i < ARRAY_LEN(vdev->txqs); i++) {
            if (vdev->txqs[i].frms == 0) {
                continue;
            }
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
                "  vdev %d (%p), txq %d\n", vdev->vdev_id, vdev, i);
            ol_tx_queue_display(&vdev->txqs[i], 4);
        }
        TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
            for (i = 0; i < ARRAY_LEN(peer->txqs); i++) {
                if (peer->txqs[i].frms == 0) {
                    continue;
                }
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO_LOW,
                    "    peer %d (%p), txq %d\n",
                    peer->peer_ids[0], vdev, i);
                ol_tx_queue_display(&peer->txqs[i], 6);
            }
        }
    }
}

#endif

#endif /* defined(CONFIG_HL_SUPPORT) */

#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL
static a_bool_t
ol_tx_vdev_has_tx_queue_group(
    struct ol_tx_queue_group_t* group,
    u_int8_t vdev_id)
{
    u_int16_t vdev_bitmap;
    vdev_bitmap = OL_TXQ_GROUP_VDEV_ID_MASK_GET(group->membership);
    if (OL_TXQ_GROUP_VDEV_ID_BIT_MASK_GET(vdev_bitmap, vdev_id)) {
        return A_TRUE;
    }
    return A_FALSE;
}

static a_bool_t
ol_tx_ac_has_tx_queue_group(
    struct ol_tx_queue_group_t* group,
    u_int8_t ac)
{
    u_int16_t ac_bitmap;
    ac_bitmap = OL_TXQ_GROUP_AC_MASK_GET(group->membership);
    if (OL_TXQ_GROUP_AC_BIT_MASK_GET(ac_bitmap, ac)) {
        return A_TRUE;
    }
    return A_FALSE;
}

u_int32_t ol_tx_txq_group_credit_limit(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    u_int32_t credit)
{
    u_int8_t i;
    int updated_credit = credit;
    /*
     * If this tx queue belongs to a group, check whether the group's
     * credit limit is more stringent than the global credit limit.
     */
    for (i = 0; i < OL_TX_MAX_GROUPS_PER_QUEUE; i++) {
        if (txq->group_ptrs[i]) {
            int group_credit;
            group_credit = adf_os_atomic_read(&txq->group_ptrs[i]->credit);
            updated_credit = MIN(updated_credit, group_credit);
        }
    }

    credit = (updated_credit < 0) ? 0 : updated_credit;

    return credit;
}

void ol_tx_txq_group_credit_update(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    int32_t credit,
    u_int8_t absolute)
{
    u_int8_t i;
    /*
     * If this tx queue belongs to a group then
     * update group credit
     */
    for (i = 0; i < OL_TX_MAX_GROUPS_PER_QUEUE; i++) {
        if (txq->group_ptrs[i]) {
            ol_txrx_update_group_credit(txq->group_ptrs[i], credit, absolute);
        }
    }
    OL_TX_UPDATE_GROUP_CREDIT_STATS(pdev);
}

void
ol_tx_set_vdev_group_ptr(
    ol_txrx_pdev_handle pdev,
    u_int8_t vdev_id,
    struct ol_tx_queue_group_t *grp_ptr)
{
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ol_txrx_peer_t *peer = NULL;

    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
        if (vdev->vdev_id == vdev_id) {
            u_int8_t i, j;
            /* update vdev queues group pointers */
            for (i = 0; i < OL_TX_VDEV_NUM_QUEUES; i++) {
                for (j = 0; j < OL_TX_MAX_GROUPS_PER_QUEUE; j++) {
                    vdev->txqs[i].group_ptrs[j] = grp_ptr;
                }
            }
            adf_os_spin_lock_bh(&pdev->peer_ref_mutex);
            /* Update peer queue group pointers */
            TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
                for (i = 0; i < OL_TX_NUM_TIDS; i++) {
                    for (j = 0; j < OL_TX_MAX_GROUPS_PER_QUEUE; j++) {
                        peer->txqs[i].group_ptrs[j] = grp_ptr;
                    }
                }
            }
            adf_os_spin_unlock_bh(&pdev->peer_ref_mutex);
            break;
        }
    }
}

void
ol_tx_txq_set_group_ptr(
    struct ol_tx_frms_queue_t *txq,
    struct ol_tx_queue_group_t *grp_ptr)
{
    u_int8_t i;
    for (i = 0; i < OL_TX_MAX_GROUPS_PER_QUEUE; i++) {
        txq->group_ptrs[i] = grp_ptr;
    }
}

void
ol_tx_set_peer_group_ptr(
    ol_txrx_pdev_handle pdev,
    struct ol_txrx_peer_t *peer,
    u_int8_t vdev_id,
    u_int8_t tid)
{
    u_int8_t i, j = 0;
    struct ol_tx_queue_group_t *group = NULL;

    for (i = 0; i < OL_TX_MAX_GROUPS_PER_QUEUE; i++) {
        peer->txqs[tid].group_ptrs[i] = NULL;
    }
    for (i = 0; i < OL_TX_MAX_TXQ_GROUPS; i++) {
        group = &pdev->txq_grps[i];
        if (ol_tx_vdev_has_tx_queue_group(group, vdev_id)) {
            if (tid < OL_TX_NUM_QOS_TIDS) {
                if (ol_tx_ac_has_tx_queue_group(
                    group, TXRX_TID_TO_WMM_AC(tid))) {
                    peer->txqs[tid].group_ptrs[j] = group;
                    j++;
                }
            } else {
                peer->txqs[tid].group_ptrs[j] = group;
                j++;
            }
        }
        if (j >= OL_TX_MAX_GROUPS_PER_QUEUE) {
            break;
        }
    }
}

u_int32_t ol_tx_get_max_tx_groups_supported(struct ol_txrx_pdev_t *pdev)
{
#ifdef HIF_SDIO
    return OL_TX_MAX_TXQ_GROUPS;
#else
    return 0;
#endif
}
#endif
