/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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
#include <ol_txrx_dbg.h>      /* ENABLE_TX_QUEUE_LOG */
#include <ol_txrx.h>          /* ol_tx_desc_pool_size_hl */
#include <adf_os_types.h>     /* a_bool_t */


#if defined(CONFIG_HL_SUPPORT)

#ifndef offsetof
#define offsetof(type, field)   ((adf_os_size_t)(&((type *)0)->field))
#endif

/*--- function prototypes for optional queue log feature --------------------*/
#if defined(ENABLE_TX_QUEUE_LOG)

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

    TX_SCHED_DEBUG_PRINT("Enter %s\n", __func__);

    /*
     * If too few tx descriptors are available, drop some currently-queued
     * tx frames, to provide enough tx descriptors for new frames, which
     * may be higher priority than the current frames.
     */
    if (adf_os_atomic_read(&pdev->tx_queue.rsrc_cnt) <=
        pdev->tx_queue.rsrc_threshold_lo)
    {
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
        adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);
        TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
            ol_txrx_peer_pause_base(pdev, peer);
        }
        adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
#endif /* defined(CONFIG_HL_SUPPORT) */
    } else {
        adf_os_spin_lock_bh(&vdev->ll_pause.mutex);
        vdev->ll_pause.paused_reason |= reason;
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
        adf_os_spin_lock_bh(&pdev->tx_queue_spinlock);

        TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
            int i;
            for (i = 0; i < ARRAY_LEN(peer->txqs); i++) {
                ol_txrx_peer_tid_unpause_base(pdev, peer, i);
            }
        }
        adf_os_spin_unlock_bh(&pdev->tx_queue_spinlock);
#endif /* defined(CONFIG_HL_SUPPORT) */
    } else {
        adf_os_spin_lock_bh(&vdev->ll_pause.mutex);
        if (vdev->ll_pause.paused_reason & reason)
        {
            vdev->ll_pause.paused_reason &= ~reason;
            if (reason == OL_TXQ_PAUSE_REASON_VDEV_SUSPEND) {
                adf_os_spin_unlock_bh(&vdev->ll_pause.mutex);
                ol_tx_vdev_ll_pause_start_timer(vdev);
            }
            else if (!vdev->ll_pause.paused_reason) {
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

#if defined(ENABLE_TX_QUEUE_LOG)

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

    type = pdev->txq_log.data[offset];
    ol_tx_queue_log_entry_type_info(
        &pdev->txq_log.data[offset], &size, &align, 1);
    align_pad = (align - ((offset + 1/*type*/))) & (align - 1);

    switch (type) {
    case ol_tx_log_entry_type_enqueue:
        {
            struct ol_tx_log_queue_add_t *record;
            record = (struct ol_tx_log_queue_add_t *)
                &pdev->txq_log.data[offset + 1 + align_pad];
            if (record->peer_id != 0xffff) {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "  added %d frms (%d bytes) for peer %d, tid %d\n",
                    record->num_frms, record->num_bytes,
                    record->peer_id, record->tid);
            } else {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "  added %d frms (%d bytes) vdev tid %d\n",
                    record->num_frms, record->num_bytes, record->tid);
            }
            break;
        }
    case ol_tx_log_entry_type_dequeue:
        {
            struct ol_tx_log_queue_add_t *record;
            record = (struct ol_tx_log_queue_add_t *)
                &pdev->txq_log.data[offset + 1 + align_pad];
            if (record->peer_id != 0xffff) {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "  download %d frms (%d bytes) from peer %d, tid %d\n",
                    record->num_frms, record->num_bytes,
                    record->peer_id, record->tid);
            } else {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "  download %d frms (%d bytes) from vdev tid %d\n",
                    record->num_frms, record->num_bytes, record->tid);
            }
            break;
        }
    case ol_tx_log_entry_type_queue_free:
        {
            struct ol_tx_log_queue_add_t *record;
            record = (struct ol_tx_log_queue_add_t *)
                &pdev->txq_log.data[offset + 1 + align_pad];
            if (record->peer_id != 0xffff) {
                VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                    "  peer %d, tid %d queue removed (%d frms, %d bytes)\n",
                    record->peer_id, record->tid,
                    record->num_frms, record->num_bytes);
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
            struct ol_tx_log_queue_state_var_sz_t *record;
            u_int8_t *data;

            record = (struct ol_tx_log_queue_state_var_sz_t *)
                &pdev->txq_log.data[offset + 1 + align_pad];
            VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                "  credit = %d, active category bitmap = %#x\n",
                record->credit, record->active_bitmap);
            data = &record->data[0];
            j = 0;
            i = 0;
            active_bitmap = record->active_bitmap;
            while (active_bitmap) {
                if (active_bitmap & 0x1) {
                    u_int16_t frms;
                    u_int32_t bytes;

                    frms = data[0] | (data[1] << 8);
                    bytes = (data[2] <<  0) | (data[3] <<  8) |
                            (data[4] << 16) | (data[5] << 24);
                    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
                        "    cat %d: %d frms, %d bytes\n",
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
        return -1 * offset; /* go back to the top */

    default:
        VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
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
    offset = pdev->txq_log.oldest_record_offset;

    /*
     * In theory, this should use mutex to guard against the offset
     * being changed while in use, but since this is just for debugging,
     * don't bother.
     */
    VOS_TRACE(VOS_MODULE_ID_TXRX, VOS_TRACE_LEVEL_INFO,
        "tx queue log:\n");
    unwrap = pdev->txq_log.wrapped;
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

    log_elem = ol_tx_queue_log_alloc(pdev, ol_tx_log_entry_type_enqueue, 0);
    if (!log_elem) {
        return;
    }

    log_elem->num_frms = frms;
    log_elem->num_bytes = bytes;
    log_elem->peer_id = peer_id;
    log_elem->tid = tid;
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
    log_elem = ol_tx_queue_log_alloc(pdev, ol_tx_log_entry_type_dequeue, 0);
    if (!log_elem) {
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
}

void
ol_tx_queue_log_free(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_frms_queue_t *txq,
    int tid, int frms, int bytes)
{
    u_int16_t peer_id;
    struct ol_tx_log_queue_add_t *log_elem;

    log_elem = ol_tx_queue_log_alloc(pdev, ol_tx_log_entry_type_queue_free, 0);
    if (!log_elem) {
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

    log_elem = ol_tx_queue_log_alloc(
        pdev, ol_tx_log_entry_type_queue_state, data_size);
    if (!log_elem) {
        *num_cats = 0;
        return;
    }
    log_elem->num_cats_active = *num_cats;
    log_elem->active_bitmap = 0;
    log_elem->credit = credit;

    *active_bitmap = &log_elem->active_bitmap;
    *data = &log_elem->data[0];
}

#endif /* defined(ENABLE_TX_QUEUE_LOG) */

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
