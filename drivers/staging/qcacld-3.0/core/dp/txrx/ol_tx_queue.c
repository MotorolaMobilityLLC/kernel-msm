/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

#include <qdf_nbuf.h>           /* qdf_nbuf_t, etc. */
#include <qdf_atomic.h>         /* qdf_atomic_read, etc. */
#include <ol_cfg.h>             /* ol_cfg_addba_retry */
#include <htt.h>                /* HTT_TX_EXT_TID_MGMT */
#include <ol_htt_tx_api.h>      /* htt_tx_desc_tid */
#include <ol_txrx_api.h>        /* ol_txrx_vdev_handle */
#include <ol_txrx_ctrl_api.h>   /* ol_txrx_sync, ol_tx_addba_conf */
#include <cdp_txrx_tx_throttle.h>
#include <ol_ctrl_txrx_api.h>   /* ol_ctrl_addba_req */
#include <ol_txrx_internal.h>   /* TXRX_ASSERT1, etc. */
#include <ol_tx_desc.h>         /* ol_tx_desc, ol_tx_desc_frame_list_free */
#include <ol_tx.h>              /* ol_tx_vdev_ll_pause_queue_send */
#include <ol_tx_sched.h>	/* ol_tx_sched_notify, etc. */
#include <ol_tx_queue.h>
#include <ol_txrx.h>          /* ol_tx_desc_pool_size_hl */
#include <ol_txrx_dbg.h>        /* ENABLE_TX_QUEUE_LOG */
#include <qdf_types.h>          /* bool */
#include "cdp_txrx_flow_ctrl_legacy.h"
#include <ol_txrx_peer_find.h>
#include <cdp_txrx_handle.h>
#if defined(CONFIG_HL_SUPPORT)

#ifndef offsetof
#define offsetof(type, field)   ((qdf_size_t)(&((type *)0)->field))
#endif

/*--- function prototypes for optional host ADDBA negotiation ---------------*/

#define OL_TX_QUEUE_ADDBA_CHECK(pdev, txq, tx_msdu_info) /* no-op */

#ifndef container_of
#define container_of(ptr, type, member) ((type *)( \
			(char *)(ptr) - (char *)(&((type *)0)->member)))
#endif
/*--- function definitions --------------------------------------------------*/

/**
 * ol_tx_queue_vdev_flush() - try to flush pending frames in the tx queues
 *			      no matter it's queued in the TX scheduler or not
 * @pdev: the physical device object
 * @vdev: the virtual device object
 *
 * Return: None
 */
static void
ol_tx_queue_vdev_flush(struct ol_txrx_pdev_t *pdev, struct ol_txrx_vdev_t *vdev)
{
#define PEER_ARRAY_COUNT        10
	struct ol_tx_frms_queue_t *txq;
	struct ol_txrx_peer_t *peer, *peers[PEER_ARRAY_COUNT];
	int i, j, peer_count;

	ol_tx_hl_queue_flush_all(vdev);

	/* flush VDEV TX queues */
	for (i = 0; i < OL_TX_VDEV_NUM_QUEUES; i++) {
		txq = &vdev->txqs[i];
		/*
		 * currently txqs of MCAST_BCAST/DEFAULT_MGMT packet are using
		 * tid HTT_TX_EXT_TID_NON_QOS_MCAST_BCAST/HTT_TX_EXT_TID_MGMT
		 * when inserted into scheduler, so use same tid when we flush
		 * them
		 */
		if (i == OL_TX_VDEV_MCAST_BCAST)
			ol_tx_queue_free(pdev,
					txq,
					HTT_TX_EXT_TID_NON_QOS_MCAST_BCAST,
					false);
		else if (i == OL_TX_VDEV_DEFAULT_MGMT)
			ol_tx_queue_free(pdev,
					txq,
					HTT_TX_EXT_TID_MGMT,
					false);
		else
			ol_tx_queue_free(pdev,
					txq,
					(i + OL_TX_NUM_TIDS),
					false);
	}
	/* flush PEER TX queues */
	do {
		peer_count = 0;
		/* select candidate peers */
		qdf_spin_lock_bh(&pdev->peer_ref_mutex);
		TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
			for (i = 0; i < OL_TX_NUM_TIDS; i++) {
				txq = &peer->txqs[i];
				if (txq->frms) {
					ol_txrx_peer_get_ref
						(peer,
						 PEER_DEBUG_ID_OL_TXQ_VDEV_FL);
					peers[peer_count++] = peer;
					break;
				}
			}
			if (peer_count >= PEER_ARRAY_COUNT)
				break;
		}
		qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
		/* flush TX queues of candidate peers */
		for (i = 0; i < peer_count; i++) {
			for (j = 0; j < OL_TX_NUM_TIDS; j++) {
				txq = &peers[i]->txqs[j];
				if (txq->frms)
					ol_tx_queue_free(pdev, txq, j, true);
			}
			ol_txrx_info("Delete Peer %pK", peer);
			ol_txrx_peer_release_ref(peers[i],
						 PEER_DEBUG_ID_OL_TXQ_VDEV_FL);
		}
	} while (peer_count >= PEER_ARRAY_COUNT);
}

/**
 * ol_tx_queue_flush() - try to flush pending frames in the tx queues
 *			 no matter it's queued in the TX scheduler or not
 * @pdev: the physical device object
 *
 * Return: None
 */
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
	bool flush_all,
	ol_tx_desc_list *tx_descs)
{
	u_int16_t num;
	u_int16_t discarded, actual_discarded = 0;

	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);

	if (flush_all == true)
		/* flush all the pending tx queues in the scheduler */
		num = ol_tx_desc_pool_size_hl(pdev->ctrl_pdev) -
			qdf_atomic_read(&pdev->tx_queue.rsrc_cnt);
	else
		/*TODO: Discard frames for a particular vdev only */
		num = pdev->tx_queue.rsrc_threshold_hi -
			pdev->tx_queue.rsrc_threshold_lo;

	TX_SCHED_DEBUG_PRINT("+%u", qdf_atomic_read(&pdev->tx_queue.rsrc_cnt));
	while (num > 0) {
		discarded = ol_tx_sched_discard_select(
				pdev, (u_int16_t)num, tx_descs, flush_all);
		if (discarded == 0)
			/*
			 * No more packets could be discarded.
			 * Probably tx queues are empty.
			 */
			break;

		num -= discarded;
		actual_discarded += discarded;
	}
	qdf_atomic_add(actual_discarded, &pdev->tx_queue.rsrc_cnt);
	TX_SCHED_DEBUG_PRINT("-");

	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);

	if (flush_all == true && num > 0)
		/*
		 * try to flush pending frames in the tx queues
		 * which are not queued in the TX scheduler.
		 */
		ol_tx_queue_flush(pdev);
}

#ifdef QCA_HL_NETDEV_FLOW_CONTROL

/**
 * is_ol_tx_discard_frames_success() - check whether currently queued tx frames
 *				       can be discarded or not
 * @pdev: the physical device object
 * @tx_desc: tx desciptor ptr
 *
 * Return: Success if available tx descriptors are too few
 */
static inline bool
is_ol_tx_discard_frames_success(struct ol_txrx_pdev_t *pdev,
				struct ol_tx_desc_t *tx_desc)
{
	ol_txrx_vdev_handle vdev;
	bool discard_frames;

	vdev = tx_desc->vdev;

	qdf_spin_lock_bh(&vdev->pdev->tx_mutex);
	if (vdev->tx_desc_limit == 0) {
		/* Flow control not enabled */
		discard_frames = qdf_atomic_read(&pdev->tx_queue.rsrc_cnt) <=
					pdev->tx_queue.rsrc_threshold_lo;
	} else {
	/*
	 * Discard
	 * if netbuf is normal priority and tx_desc_count greater than
	 * queue stop threshold
	 * AND
	 * if netbuf is high priority and tx_desc_count greater than
	 * tx desc limit.
	 */
		discard_frames = (!ol_tx_desc_is_high_prio(tx_desc->netbuf) &&
				  qdf_atomic_read(&vdev->tx_desc_count) >
				  vdev->queue_stop_th) ||
				  (ol_tx_desc_is_high_prio(tx_desc->netbuf) &&
				  qdf_atomic_read(&vdev->tx_desc_count) >
				  vdev->tx_desc_limit);
	}
	qdf_spin_unlock_bh(&vdev->pdev->tx_mutex);

	return discard_frames;
}
#else

static inline bool
is_ol_tx_discard_frames_success(struct ol_txrx_pdev_t *pdev,
				struct ol_tx_desc_t *tx_desc)
{
	return qdf_atomic_read(&pdev->tx_queue.rsrc_cnt) <=
				pdev->tx_queue.rsrc_threshold_lo;
}
#endif /* QCA_HL_NETDEV_FLOW_CONTROL */

void
ol_tx_enqueue(
	struct ol_txrx_pdev_t *pdev,
	struct ol_tx_frms_queue_t *txq,
	struct ol_tx_desc_t *tx_desc,
	struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	int bytes;
	struct ol_tx_sched_notify_ctx_t notify_ctx;

	TX_SCHED_DEBUG_PRINT("Enter");

	/*
	 * If too few tx descriptors are available, drop some currently-queued
	 * tx frames, to provide enough tx descriptors for new frames, which
	 * may be higher priority than the current frames.
	 */
	if (is_ol_tx_discard_frames_success(pdev, tx_desc)) {
		ol_tx_desc_list tx_descs;

		TAILQ_INIT(&tx_descs);
		ol_tx_queue_discard(pdev, false, &tx_descs);
		/*Discard Frames in Discard List*/
		ol_tx_desc_frame_list_free(pdev, &tx_descs, 1 /* error */);
	}

	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);
	TAILQ_INSERT_TAIL(&txq->head, tx_desc, tx_desc_list_elem);

	bytes = qdf_nbuf_len(tx_desc->netbuf);
	txq->frms++;
	txq->bytes += bytes;
	ol_tx_update_grp_frm_count(txq, 1);
	ol_tx_queue_log_enqueue(pdev, tx_msdu_info, 1, bytes);

	if (txq->flag != ol_tx_queue_paused) {
		notify_ctx.event = OL_TX_ENQUEUE_FRAME;
		notify_ctx.frames = 1;
		notify_ctx.bytes = qdf_nbuf_len(tx_desc->netbuf);
		notify_ctx.txq = txq;
		notify_ctx.info.tx_msdu_info = tx_msdu_info;
		ol_tx_sched_notify(pdev, &notify_ctx);
		txq->flag = ol_tx_queue_active;
	}

	if (!ETHERTYPE_IS_EAPOL_WAPI(tx_msdu_info->htt.info.ethertype))
		OL_TX_QUEUE_ADDBA_CHECK(pdev, txq, tx_msdu_info);

	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);
	TX_SCHED_DEBUG_PRINT("Leave");
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
	unsigned int credit_sum;

	TXRX_ASSERT2(txq->flag != ol_tx_queue_paused);
	TX_SCHED_DEBUG_PRINT("Enter");

	if (txq->frms < max_frames)
		max_frames = txq->frms;

	bytes_sum = 0;
	credit_sum = 0;
	for (num_frames = 0; num_frames < max_frames; num_frames++) {
		unsigned int frame_credit;
		struct ol_tx_desc_t *tx_desc;

		tx_desc = TAILQ_FIRST(&txq->head);

		frame_credit = htt_tx_msdu_credit(tx_desc->netbuf);
		if (credit_sum + frame_credit > *credit)
			break;

		credit_sum += frame_credit;
		bytes_sum += qdf_nbuf_len(tx_desc->netbuf);
		TAILQ_REMOVE(&txq->head, tx_desc, tx_desc_list_elem);
		TAILQ_INSERT_TAIL(head, tx_desc, tx_desc_list_elem);
	}
	txq->frms -= num_frames;
	txq->bytes -= bytes_sum;
	ol_tx_update_grp_frm_count(txq, -credit_sum);

	/* a paused queue remains paused, regardless of whether it has frames */
	if (txq->frms == 0 && txq->flag == ol_tx_queue_active)
		txq->flag = ol_tx_queue_empty;

	ol_tx_queue_log_dequeue(pdev, txq, num_frames, bytes_sum);
	TX_SCHED_DEBUG_PRINT("Leave");

	*bytes = bytes_sum;
	*credit = credit_sum;
	return num_frames;
}

void
ol_tx_queue_free(
	struct ol_txrx_pdev_t *pdev,
	struct ol_tx_frms_queue_t *txq,
	int tid, bool is_peer_txq)
{
	int frms = 0, bytes = 0;
	struct ol_tx_desc_t *tx_desc;
	struct ol_tx_sched_notify_ctx_t notify_ctx;
	ol_tx_desc_list tx_tmp_list;

	TAILQ_INIT(&tx_tmp_list);
	TX_SCHED_DEBUG_PRINT("Enter");
	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);

	notify_ctx.event = OL_TX_DELETE_QUEUE;
	notify_ctx.txq = txq;
	notify_ctx.info.ext_tid = tid;
	ol_tx_sched_notify(pdev, &notify_ctx);

	frms = txq->frms;
	tx_desc = TAILQ_FIRST(&txq->head);
	while (txq->frms) {
		bytes += qdf_nbuf_len(tx_desc->netbuf);
		txq->frms--;
		tx_desc = TAILQ_NEXT(tx_desc, tx_desc_list_elem);
	}
	ol_tx_queue_log_free(pdev, txq, tid, frms, bytes, is_peer_txq);
	txq->bytes -= bytes;
	ol_tx_queue_log_free(pdev, txq, tid, frms, bytes, is_peer_txq);
	txq->flag = ol_tx_queue_empty;
	/* txq->head gets reset during the TAILQ_CONCAT call */
	TAILQ_CONCAT(&tx_tmp_list, &txq->head, tx_desc_list_elem);
	ol_tx_update_grp_frm_count(txq, -frms);
	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);
	/* free tx frames without holding tx_queue_spinlock */
	qdf_atomic_add(frms, &pdev->tx_queue.rsrc_cnt);
	while (frms) {
		tx_desc = TAILQ_FIRST(&tx_tmp_list);
		TAILQ_REMOVE(&tx_tmp_list, tx_desc, tx_desc_list_elem);
		ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 0);
		frms--;
	}
	TX_SCHED_DEBUG_PRINT("Leave");
}


/*--- queue pause / unpause functions ---------------------------------------*/

/**
 * ol_txrx_peer_tid_pause_base() - suspend/pause txq for a given tid given peer
 * @pdev: the physical device object
 * @peer: peer device object
 * @tid: tid for which queue needs to be paused
 *
 * Return: None
 */
static void
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
#ifdef QCA_BAD_PEER_TX_FLOW_CL

/**
 * ol_txrx_peer_pause_but_no_mgmt_q_base() - suspend/pause all txqs except
 *					     management queue for a given peer
 * @pdev: the physical device object
 * @peer: peer device object
 *
 * Return: None
 */
static void
ol_txrx_peer_pause_but_no_mgmt_q_base(
	struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_peer_t *peer)
{
	int i;

	for (i = 0; i < OL_TX_MGMT_TID; i++)
		ol_txrx_peer_tid_pause_base(pdev, peer, i);
}
#endif


/**
 * ol_txrx_peer_pause_base() - suspend/pause all txqs for a given peer
 * @pdev: the physical device object
 * @peer: peer device object
 *
 * Return: None
 */
static void
ol_txrx_peer_pause_base(
	struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_peer_t *peer)
{
	int i;

	for (i = 0; i < QDF_ARRAY_SIZE(peer->txqs); i++)
		ol_txrx_peer_tid_pause_base(pdev, peer, i);
}

/**
 * ol_txrx_peer_tid_unpause_base() - unpause txq for a given tid given peer
 * @pdev: the physical device object
 * @peer: peer device object
 * @tid: tid for which queue needs to be unpaused
 *
 * Return: None
 */
static void
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
			qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);
			ol_tx_sched(pdev);
			qdf_spin_lock_bh(&pdev->tx_queue_spinlock);
		}
	}
}

/**
 * ol_txrx_peer_unpause_base() - unpause all txqs for a given peer
 * @pdev: the physical device object
 * @peer: peer device object
 *
 * Return: None
 */
static void
ol_txrx_peer_unpause_base(
	struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_peer_t *peer)
{
	int i;

	for (i = 0; i < QDF_ARRAY_SIZE(peer->txqs); i++)
		ol_txrx_peer_tid_unpause_base(pdev, peer, i);
}

#ifdef QCA_BAD_PEER_TX_FLOW_CL
/**
 * ol_txrx_peer_unpause_but_no_mgmt_q_base() - unpause all txqs except
 *					       management queue for a given peer
 * @pdev: the physical device object
 * @peer: peer device object
 *
 * Return: None
 */
static void
ol_txrx_peer_unpause_but_no_mgmt_q_base(
	struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_peer_t *peer)
{
	int i;

	for (i = 0; i < OL_TX_MGMT_TID; i++)
		ol_txrx_peer_tid_unpause_base(pdev, peer, i);
}
#endif

void
ol_txrx_peer_tid_unpause(ol_txrx_peer_handle peer, int tid)
{
	struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;

	/* TO DO: log the queue unpause */

	/* acquire the mutex lock, since we'll be modifying the queues */
	TX_SCHED_DEBUG_PRINT("Enter");
	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);

	if (tid == -1) {
		int i;

		for (i = 0; i < QDF_ARRAY_SIZE(peer->txqs); i++)
			ol_txrx_peer_tid_unpause_base(pdev, peer, i);

	} else {
		ol_txrx_peer_tid_unpause_base(pdev, peer, tid);
	}

	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);
	TX_SCHED_DEBUG_PRINT("Leave");
}

void
ol_txrx_vdev_pause(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
		   uint32_t reason, uint32_t pause_type)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);
	struct ol_txrx_pdev_t *pdev;
	struct ol_txrx_peer_t *peer;
	/* TO DO: log the queue pause */
	/* acquire the mutex lock, since we'll be modifying the queues */
	TX_SCHED_DEBUG_PRINT("Enter");

	if (qdf_unlikely(!vdev)) {
		ol_txrx_err("vdev is NULL");
		return;
	}

	pdev = vdev->pdev;

	/* use peer_ref_mutex before accessing peer_list */
	qdf_spin_lock_bh(&pdev->peer_ref_mutex);
	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);
	TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
		if (pause_type == PAUSE_TYPE_CHOP) {
			if (!(peer->is_tdls_peer && peer->tdls_offchan_enabled))
				ol_txrx_peer_pause_base(pdev, peer);
		} else if (pause_type == PAUSE_TYPE_CHOP_TDLS_OFFCHAN) {
			if (peer->is_tdls_peer && peer->tdls_offchan_enabled)
				ol_txrx_peer_pause_base(pdev, peer);
		} else {
			ol_txrx_peer_pause_base(pdev, peer);
		}
	}
	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);
	qdf_spin_unlock_bh(&pdev->peer_ref_mutex);

	TX_SCHED_DEBUG_PRINT("Leave");
}

void ol_txrx_vdev_unpause(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			  uint32_t reason, uint32_t pause_type)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);
	struct ol_txrx_pdev_t *pdev;
	struct ol_txrx_peer_t *peer;

	/* TO DO: log the queue unpause */
	/* acquire the mutex lock, since we'll be modifying the queues */
	TX_SCHED_DEBUG_PRINT("Enter");

	if (qdf_unlikely(!vdev)) {
		ol_txrx_err("vdev is NULL");
		return;
	}

	pdev = vdev->pdev;

	/* take peer_ref_mutex before accessing peer_list */
	qdf_spin_lock_bh(&pdev->peer_ref_mutex);
	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);

	TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
		if (pause_type == PAUSE_TYPE_CHOP) {
			if (!(peer->is_tdls_peer && peer->tdls_offchan_enabled))
				ol_txrx_peer_unpause_base(pdev, peer);
		} else if (pause_type == PAUSE_TYPE_CHOP_TDLS_OFFCHAN) {
			if (peer->is_tdls_peer && peer->tdls_offchan_enabled)
				ol_txrx_peer_unpause_base(pdev, peer);
		} else {
			ol_txrx_peer_unpause_base(pdev, peer);
		}
	}
	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);
	qdf_spin_unlock_bh(&pdev->peer_ref_mutex);

	TX_SCHED_DEBUG_PRINT("Leave");
}

void ol_txrx_vdev_flush(struct cdp_soc_t *soc_hdl, uint8_t vdev_id)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (qdf_unlikely(!vdev)) {
		ol_txrx_err("vdev is NULL");
		return;
	}

	if (!vdev)
		return;

	ol_tx_queue_vdev_flush(vdev->pdev, vdev);
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

	for (i = 0; i < pdev->tx_peer_bal.peer_num; i++) {
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
				"reach the maxinum peer num %d", peer_num);
				return;
		}
		pdev->tx_peer_bal.limit_list[peer_num].peer_id = peer_id;
		pdev->tx_peer_bal.limit_list[peer_num].limit_flag = true;
		pdev->tx_peer_bal.limit_list[peer_num].limit = peer_limit;
		pdev->tx_peer_bal.peer_num++;

		peer = ol_txrx_peer_find_by_id(pdev, peer_id);
		if (peer) {
			peer->tx_limit_flag = true;
			peer->tx_limit = peer_limit;
		}

		TX_SCHED_DEBUG_PRINT_ALWAYS(
			"Add one peer into limit queue, peer_id %d, cur peer num %d",
			peer_id,
			pdev->tx_peer_bal.peer_num);
	}

	/* Only start the timer once */
	if (pdev->tx_peer_bal.peer_bal_timer_state ==
					ol_tx_peer_bal_timer_inactive) {
		qdf_timer_start(&pdev->tx_peer_bal.peer_bal_timer,
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
		if (pdev->tx_peer_bal.limit_list[i].peer_id == peer_id) {
			pdev->tx_peer_bal.limit_list[i] =
				pdev->tx_peer_bal.limit_list[
					pdev->tx_peer_bal.peer_num - 1];
			pdev->tx_peer_bal.peer_num--;

			peer = ol_txrx_peer_find_by_id(pdev, peer_id);
			if (peer)
				peer->tx_limit_flag = false;


			TX_SCHED_DEBUG_PRINT(
				"Remove one peer from limitq, peer_id %d, cur peer num %d",
				peer_id,
				pdev->tx_peer_bal.peer_num);
			break;
		}
	}

	/* Only stop the timer if no peer in limit state */
	if (pdev->tx_peer_bal.peer_num == 0) {
		qdf_timer_stop(&pdev->tx_peer_bal.peer_bal_timer);
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
	TX_SCHED_DEBUG_PRINT("Enter");
	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);

	ol_txrx_peer_pause_but_no_mgmt_q_base(pdev, peer);

	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);
	TX_SCHED_DEBUG_PRINT("Leave");
}

void
ol_txrx_peer_unpause_but_no_mgmt_q(ol_txrx_peer_handle peer)
{
	struct ol_txrx_pdev_t *pdev = peer->vdev->pdev;

	/* TO DO: log the queue pause */

	/* acquire the mutex lock, since we'll be modifying the queues */
	TX_SCHED_DEBUG_PRINT("Enter");
	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);

	ol_txrx_peer_unpause_but_no_mgmt_q_base(pdev, peer);

	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);
	TX_SCHED_DEBUG_PRINT("Leave");
}

u_int16_t
ol_tx_bad_peer_dequeue_check(struct ol_tx_frms_queue_t *txq,
			     u_int16_t max_frames,
			     u_int16_t *tx_limit_flag)
{
	if (txq && (txq->peer) && (txq->peer->tx_limit_flag) &&
	    (txq->peer->tx_limit < max_frames)) {
		TX_SCHED_DEBUG_PRINT(
			"Peer ID %d goes to limit, threshold is %d",
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
	if (unlikely(!pdev)) {
		TX_SCHED_DEBUG_PRINT_ALWAYS("Error: NULL pdev handler");
		return;
	}

	if (unlikely(!txq)) {
		TX_SCHED_DEBUG_PRINT_ALWAYS("Error: NULL txq");
		return;
	}

	qdf_spin_lock_bh(&pdev->tx_peer_bal.mutex);
	if (tx_limit_flag && (txq->peer) &&
	    (txq->peer->tx_limit_flag)) {
		if (txq->peer->tx_limit < frames)
			txq->peer->tx_limit = 0;
		else
			txq->peer->tx_limit -= frames;

		TX_SCHED_DEBUG_PRINT_ALWAYS(
				"Peer ID %d in limit, deque %d frms",
				txq->peer->peer_ids[0], frames);
	} else if (txq->peer) {
		TX_SCHED_DEBUG_PRINT("Download peer_id %d, num_frames %d",
				     txq->peer->peer_ids[0], frames);
	}
	qdf_spin_unlock_bh(&pdev->tx_peer_bal.mutex);
}

void
ol_txrx_bad_peer_txctl_set_setting(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				   int enable, int period, int txq_limit)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);

	if (enable)
		pdev->tx_peer_bal.enabled = ol_tx_peer_bal_enable;
	else
		pdev->tx_peer_bal.enabled = ol_tx_peer_bal_disable;

	/* Set the current settingl */
	pdev->tx_peer_bal.peer_bal_period_ms = period;
	pdev->tx_peer_bal.peer_bal_txq_limit = txq_limit;
}

void
ol_txrx_bad_peer_txctl_update_threshold(struct cdp_soc_t *soc_hdl,
					uint8_t pdev_id, int level,
					int tput_thresh, int tx_limit)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);

	/* Set the current settingl */
	pdev->tx_peer_bal.ctl_thresh[level].tput_thresh =
		tput_thresh;
	pdev->tx_peer_bal.ctl_thresh[level].tx_limit =
		tx_limit;
}

/**
 * ol_tx_pdev_peer_bal_timer() - timer function
 * @context: context of timer function
 *
 * Return: None
 */
static void
ol_tx_pdev_peer_bal_timer(void *context)
{
	int i;
	struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)context;

	qdf_spin_lock_bh(&pdev->tx_peer_bal.mutex);

	for (i = 0; i < pdev->tx_peer_bal.peer_num; i++) {
		if (pdev->tx_peer_bal.limit_list[i].limit_flag) {
			u_int16_t peer_id =
				pdev->tx_peer_bal.limit_list[i].peer_id;
			u_int16_t tx_limit =
				pdev->tx_peer_bal.limit_list[i].limit;

			struct ol_txrx_peer_t *peer = NULL;

			peer = ol_txrx_peer_find_by_id(pdev, peer_id);
			TX_SCHED_DEBUG_PRINT(
				"peer_id %d  peer = 0x%x tx limit %d",
				peer_id,
				(int)peer, tx_limit);

			/*
			 * It is possible the peer limit is still not 0,
			 * but it is the scenario should not be cared
			 */
			if (peer) {
				peer->tx_limit = tx_limit;
			} else {
				ol_txrx_peer_bal_remove_limit_peer(pdev,
								   peer_id);
				TX_SCHED_DEBUG_PRINT_ALWAYS(
					"No such a peer, peer id = %d",
					peer_id);
			}
		}
	}

	qdf_spin_unlock_bh(&pdev->tx_peer_bal.mutex);

	if (pdev->tx_peer_bal.peer_num) {
		ol_tx_sched(pdev);
		qdf_timer_start(&pdev->tx_peer_bal.peer_bal_timer,
					pdev->tx_peer_bal.peer_bal_period_ms);
	}
}

void
ol_txrx_set_txq_peer(
	struct ol_tx_frms_queue_t *txq,
	struct ol_txrx_peer_t *peer)
{
	if (txq)
		txq->peer = peer;
}

void ol_tx_badpeer_flow_cl_init(struct ol_txrx_pdev_t *pdev)
{
	u_int32_t timer_period;

	qdf_spinlock_create(&pdev->tx_peer_bal.mutex);
	pdev->tx_peer_bal.peer_num = 0;
	pdev->tx_peer_bal.peer_bal_timer_state
		= ol_tx_peer_bal_timer_inactive;

	timer_period = 2000;
	pdev->tx_peer_bal.peer_bal_period_ms = timer_period;

	qdf_timer_init(
			pdev->osdev,
			&pdev->tx_peer_bal.peer_bal_timer,
			ol_tx_pdev_peer_bal_timer,
			pdev, QDF_TIMER_TYPE_SW);
}

void ol_tx_badpeer_flow_cl_deinit(struct ol_txrx_pdev_t *pdev)
{
	qdf_timer_stop(&pdev->tx_peer_bal.peer_bal_timer);
	pdev->tx_peer_bal.peer_bal_timer_state =
					ol_tx_peer_bal_timer_inactive;
	qdf_timer_free(&pdev->tx_peer_bal.peer_bal_timer);
	qdf_spinlock_destroy(&pdev->tx_peer_bal.mutex);
}

void
ol_txrx_peer_link_status_handler(
	ol_txrx_pdev_handle pdev,
	u_int16_t peer_num,
	struct rate_report_t *peer_link_status)
{
	u_int16_t i = 0;
	struct ol_txrx_peer_t *peer = NULL;

	if (!pdev) {
		TX_SCHED_DEBUG_PRINT_ALWAYS("Error: NULL pdev handler");
		return;
	}

	if (!peer_link_status) {
		TX_SCHED_DEBUG_PRINT_ALWAYS(
			"Error:NULL link report message. peer num %d",
			peer_num);
		return;
	}

	/* Check if bad peer tx flow CL is enabled */
	if (pdev->tx_peer_bal.enabled != ol_tx_peer_bal_enable) {
		TX_SCHED_DEBUG_PRINT_ALWAYS(
			"Bad peer tx flow CL is not enabled, ignore it");
		return;
	}

	/* Check peer_num is reasonable */
	if (peer_num > MAX_NO_PEERS_IN_LIMIT) {
		TX_SCHED_DEBUG_PRINT_ALWAYS("Bad peer_num %d", peer_num);
		return;
	}

	TX_SCHED_DEBUG_PRINT_ALWAYS("peer_num %d", peer_num);

	for (i = 0; i < peer_num; i++) {
		u_int16_t peer_limit, peer_id;
		u_int16_t pause_flag, unpause_flag;
		u_int32_t peer_phy, peer_tput;

		peer_id = peer_link_status->id;
		peer_phy = peer_link_status->phy;
		peer_tput = peer_link_status->rate;

		TX_SCHED_DEBUG_PRINT("peer id %d tput %d phy %d",
				     peer_id, peer_tput, peer_phy);

		/* Sanity check for the PHY mode value */
		if (peer_phy > TXRX_IEEE11_AC) {
			TX_SCHED_DEBUG_PRINT_ALWAYS(
				"PHY value is illegal: %d, and the peer_id %d",
				peer_link_status->phy, peer_id);
			continue;
		}
		pause_flag   = false;
		unpause_flag = false;
		peer_limit   = 0;

		/* From now on, PHY, PER info should be all fine */
		qdf_spin_lock_bh(&pdev->tx_peer_bal.mutex);

		/* Update link status analysis for each peer */
		peer = ol_txrx_peer_find_by_id(pdev, peer_id);
		if (peer) {
			u_int32_t thresh, limit, phy;

			phy = peer_link_status->phy;
			thresh = pdev->tx_peer_bal.ctl_thresh[phy].tput_thresh;
			limit = pdev->tx_peer_bal.ctl_thresh[phy].tx_limit;

			if (((peer->tx_pause_flag) || (peer->tx_limit_flag)) &&
			    (peer_tput) && (peer_tput < thresh))
				peer_limit = limit;

			if (peer_limit) {
				ol_txrx_peer_bal_add_limit_peer(pdev, peer_id,
								peer_limit);
			} else if (pdev->tx_peer_bal.peer_num) {
				TX_SCHED_DEBUG_PRINT(
					"Check if peer_id %d exit limit",
					peer_id);
				ol_txrx_peer_bal_remove_limit_peer(pdev,
								   peer_id);
			}
			if ((peer_tput == 0) &&
			    (peer->tx_pause_flag == false)) {
				peer->tx_pause_flag = true;
				pause_flag = true;
			} else if (peer->tx_pause_flag) {
				unpause_flag = true;
				peer->tx_pause_flag = false;
			}
		} else {
			TX_SCHED_DEBUG_PRINT(
				"Remove peer_id %d from limit list", peer_id);
			ol_txrx_peer_bal_remove_limit_peer(pdev, peer_id);
		}

		peer_link_status++;
		qdf_spin_unlock_bh(&pdev->tx_peer_bal.mutex);
		if (pause_flag)
			ol_txrx_peer_pause_but_no_mgmt_q(peer);
		else if (unpause_flag)
			ol_txrx_peer_unpause_but_no_mgmt_q(peer);
	}
}
#endif /* QCA_BAD_PEER_TX_FLOW_CL */

/*--- ADDBA triggering functions --------------------------------------------*/


/*=== debug functions =======================================================*/

/*--- queue event log -------------------------------------------------------*/

#if defined(DEBUG_HL_LOGGING)

#define negative_sign -1

/**
 * ol_tx_queue_log_entry_type_info() - log queues entry info
 * @type: log entry type
 * @size: size
 * @align: alignment
 * @var_size: variable size record
 *
 * Return: None
 */
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
			/* read the variable-sized record,
			 * to see how large it is
			 */
			int align_pad;
			struct ol_tx_log_queue_state_var_sz_t *record;

			align_pad =
			(*align - (uint32_t)(((unsigned long) type) + 1))
							& (*align - 1);
			record = (struct ol_tx_log_queue_state_var_sz_t *)
				(type + 1 + align_pad);
			*size += record->num_cats_active *
				(sizeof(u_int32_t) /* bytes */ +
					sizeof(u_int16_t) /* frms */);
		}
		break;

	/*case ol_tx_log_entry_type_drop:*/
	default:
		*size = 0;
		*align = 0;
	};
}

/**
 * ol_tx_queue_log_oldest_update() - log oldest record
 * @pdev: pointer to txrx handle
 * @offset: offset value
 *
 * Return: None
 */
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
	if (!pdev->txq_log.wrapped)
		/*
		 * The log has not even filled up yet - no need to remove
		 * the oldest record to make room for a new record.
		 */
		return;


	if (offset > pdev->txq_log.offset) {
		/*
		 * not wraparound -
		 * The oldest record offset may have already wrapped around,
		 * even if the newest record has not.  In this case, then
		 * the oldest record offset is fine where it is.
		 */
		if (pdev->txq_log.oldest_record_offset == 0)
			return;

		oldest_record_offset = pdev->txq_log.oldest_record_offset;
	} else
		/* wraparound */
		oldest_record_offset = 0;


	while (oldest_record_offset < offset) {
		int size, align, align_pad;
		u_int8_t type;

		type = pdev->txq_log.data[oldest_record_offset];
		if (type == ol_tx_log_entry_type_wrap) {
			oldest_record_offset = 0;
			break;
		}
		ol_tx_queue_log_entry_type_info(
				&pdev->txq_log.data[oldest_record_offset],
				&size, &align, 1);
		align_pad =
			(align - ((oldest_record_offset + 1/*type*/)))
							& (align - 1);
		/*
		 * QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		 * "TXQ LOG old alloc: offset %d, type %d, size %d (%d)\n",
		 * oldest_record_offset, type, size, size + 1 + align_pad);
		 */
		oldest_record_offset += size + 1 + align_pad;
	}
	if (oldest_record_offset >= pdev->txq_log.size)
		oldest_record_offset = 0;

	pdev->txq_log.oldest_record_offset = oldest_record_offset;
}

/**
 * ol_tx_queue_log_alloc() - log data allocation
 * @pdev: physical device object
 * @type: ol_tx_log_entry_type
 * @extra_bytes: extra bytes
 *
 *
 * Return: log element
 */
static void *
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

	if (pdev->txq_log.size - offset >= size + 1 + align_pad)
		/* no need to wrap around */
		goto alloc_found;

	if (!pdev->txq_log.allow_wrap)
		return NULL; /* log is full and can't wrap */

	/* handle wrap-around */
	pdev->txq_log.wrapped = 1;
	offset = 0;
	align_pad = (align - ((offset + 1/*type*/))) & (align - 1);
	/* sanity check that the log is large enough to hold this entry */
	if (pdev->txq_log.size <= size + 1 + align_pad)
		return NULL;


alloc_found:
	ol_tx_queue_log_oldest_update(pdev, offset + size + 1 + align_pad);
	if (offset == 0)
		pdev->txq_log.data[pdev->txq_log.offset] =
						ol_tx_log_entry_type_wrap;

	/*
	 * QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
	 * "TXQ LOG new alloc: offset %d, type %d, size %d (%d)\n",
	 * offset, type, size, size + 1 + align_pad);
	 */
	pdev->txq_log.data[offset] = type;
	pdev->txq_log.offset = offset + size + 1 + align_pad;
	if (pdev->txq_log.offset >= pdev->txq_log.size) {
		pdev->txq_log.offset = 0;
		pdev->txq_log.wrapped = 1;
	}
	return &pdev->txq_log.data[offset + 1 + align_pad];
}

/**
 * ol_tx_queue_log_record_display() - show log record of tx queue
 * @pdev: pointer to txrx handle
 * @offset: offset value
 *
 * Return: size of record
 */
static int
ol_tx_queue_log_record_display(struct ol_txrx_pdev_t *pdev, int offset)
{
	int size, align, align_pad;
	u_int8_t type;
	struct ol_txrx_peer_t *peer;

	qdf_spin_lock_bh(&pdev->txq_log_spinlock);
	type = pdev->txq_log.data[offset];
	ol_tx_queue_log_entry_type_info(
			&pdev->txq_log.data[offset], &size, &align, 1);
	align_pad = (align - ((offset + 1/*type*/))) & (align - 1);

	switch (type) {
	case ol_tx_log_entry_type_enqueue:
	{
		struct ol_tx_log_queue_add_t record;

		qdf_mem_copy(&record,
			     &pdev->txq_log.data[offset + 1 + align_pad],
			     sizeof(struct ol_tx_log_queue_add_t));
		qdf_spin_unlock_bh(&pdev->txq_log_spinlock);

		if (record.peer_id != 0xffff) {
			peer = ol_txrx_peer_find_by_id(pdev,
						       record.peer_id);
			if (peer)
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "Q: %6d  %5d  %3d  %4d ("QDF_MAC_ADDR_FMT")",
					  record.num_frms, record.num_bytes,
					  record.tid,
					  record.peer_id,
					  QDF_MAC_ADDR_REF(peer->mac_addr.raw));
			else
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "Q: %6d  %5d  %3d  %4d",
					  record.num_frms, record.num_bytes,
					  record.tid, record.peer_id);
		} else {
			QDF_TRACE(QDF_MODULE_ID_TXRX,
				  QDF_TRACE_LEVEL_INFO,
				  "Q: %6d  %5d  %3d  from vdev",
				  record.num_frms, record.num_bytes,
				  record.tid);
		}
		break;
	}
	case ol_tx_log_entry_type_dequeue:
	{
		struct ol_tx_log_queue_add_t record;

		qdf_mem_copy(&record,
			     &pdev->txq_log.data[offset + 1 + align_pad],
			     sizeof(struct ol_tx_log_queue_add_t));
		qdf_spin_unlock_bh(&pdev->txq_log_spinlock);

		if (record.peer_id != 0xffff) {
			peer = ol_txrx_peer_find_by_id(pdev, record.peer_id);
			if (peer)
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "DQ: %6d  %5d  %3d  %4d ("QDF_MAC_ADDR_FMT")",
					  record.num_frms, record.num_bytes,
					  record.tid,
					  record.peer_id,
					  QDF_MAC_ADDR_REF(peer->mac_addr.raw));
			else
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "DQ: %6d  %5d  %3d  %4d",
					  record.num_frms, record.num_bytes,
					  record.tid, record.peer_id);
		} else {
			QDF_TRACE(QDF_MODULE_ID_TXRX,
				  QDF_TRACE_LEVEL_INFO,
				  "DQ: %6d  %5d  %3d  from vdev",
				  record.num_frms, record.num_bytes,
				  record.tid);
		}
		break;
	}
	case ol_tx_log_entry_type_queue_free:
	{
		struct ol_tx_log_queue_add_t record;

		qdf_mem_copy(&record,
			     &pdev->txq_log.data[offset + 1 + align_pad],
			     sizeof(struct ol_tx_log_queue_add_t));
		qdf_spin_unlock_bh(&pdev->txq_log_spinlock);

		if (record.peer_id != 0xffff) {
			peer = ol_txrx_peer_find_by_id(pdev, record.peer_id);
			if (peer)
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "F: %6d  %5d  %3d  %4d ("QDF_MAC_ADDR_FMT")",
					  record.num_frms, record.num_bytes,
					  record.tid,
					  record.peer_id,
					  QDF_MAC_ADDR_REF(peer->mac_addr.raw));
			else
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "F: %6d  %5d  %3d  %4d",
					  record.num_frms, record.num_bytes,
					  record.tid, record.peer_id);
		} else {
			/* shouldn't happen */
			QDF_TRACE(QDF_MODULE_ID_TXRX,
				  QDF_TRACE_LEVEL_INFO,
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

		qdf_mem_copy(&record,
			     &pdev->txq_log.data[offset + 1 + align_pad],
			     sizeof(struct ol_tx_log_queue_state_var_sz_t));
		qdf_spin_unlock_bh(&pdev->txq_log_spinlock);

		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "S: bitmap = %#x",
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
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "cat %2d: %6d  %5d",
					  i, frms, bytes);
				data += 6;
				j++;
			}
			i++;
			active_bitmap >>= 1;
		}
		break;
	}

	/*case ol_tx_log_entry_type_drop:*/

	case ol_tx_log_entry_type_wrap:
		qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
		return negative_sign * offset; /* go back to the top */

	default:
		qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "*** invalid tx log entry type (%d)\n", type);
		return 0; /* error */
	};

	return size + 1 + align_pad;
}

/**
 * ol_tx_queue_log_display() - show tx queue log
 * @pdev: pointer to txrx handle
 *
 * Return: None
 */
void
ol_tx_queue_log_display(struct ol_txrx_pdev_t *pdev)
{
	int offset;
	int unwrap;

	qdf_spin_lock_bh(&pdev->txq_log_spinlock);
	offset = pdev->txq_log.oldest_record_offset;
	unwrap = pdev->txq_log.wrapped;
	qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
	/*
	 * In theory, this should use mutex to guard against the offset
	 * being changed while in use, but since this is just for debugging,
	 * don't bother.
	 */
	txrx_nofl_info("Current target credit: %d",
		       qdf_atomic_read(&pdev->target_tx_credit));
	txrx_nofl_info("Tx queue log:");
	txrx_nofl_info(": Frames  Bytes  TID  PEER");

	while (unwrap || offset != pdev->txq_log.offset) {
		int delta = ol_tx_queue_log_record_display(pdev, offset);

		if (delta == 0)
			return; /* error */

		if (delta < 0)
			unwrap = 0;

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

	qdf_spin_lock_bh(&pdev->txq_log_spinlock);
	log_elem = ol_tx_queue_log_alloc(pdev, ol_tx_log_entry_type_enqueue, 0);
	if (!log_elem) {
		qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
		return;
	}

	log_elem->num_frms = frms;
	log_elem->num_bytes = bytes;
	log_elem->peer_id = peer_id;
	log_elem->tid = tid;
	qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
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
	qdf_spin_lock_bh(&pdev->txq_log_spinlock);
	log_elem = ol_tx_queue_log_alloc(pdev, ol_tx_log_entry_type_dequeue, 0);
	if (!log_elem) {
		qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
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
	qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
}

void
ol_tx_queue_log_free(
	struct ol_txrx_pdev_t *pdev,
	struct ol_tx_frms_queue_t *txq,
	int tid, int frms, int bytes, bool is_peer_txq)
{
	u_int16_t peer_id;
	struct ol_tx_log_queue_add_t *log_elem;

	qdf_spin_lock_bh(&pdev->txq_log_spinlock);
	log_elem = ol_tx_queue_log_alloc(pdev, ol_tx_log_entry_type_queue_free,
									0);
	if (!log_elem) {
		qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
		return;
	}

	if ((tid < OL_TX_NUM_TIDS) && is_peer_txq) {
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
	qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
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

	data_size = sizeof(u_int32_t) /* bytes */ +
				sizeof(u_int16_t) /* frms */;
	data_size *= *num_cats;

	qdf_spin_lock_bh(&pdev->txq_log_spinlock);
	log_elem = ol_tx_queue_log_alloc(
			pdev, ol_tx_log_entry_type_queue_state, data_size);
	if (!log_elem) {
		*num_cats = 0;
		qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
		return;
	}
	log_elem->num_cats_active = *num_cats;
	log_elem->active_bitmap = 0;
	log_elem->credit = credit;

	*active_bitmap = &log_elem->active_bitmap;
	*data = &log_elem->data[0];
	qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
}

/**
 * ol_tx_queue_log_clear() - clear tx queue log
 * @pdev: pointer to txrx handle
 *
 * Return: None
 */
void
ol_tx_queue_log_clear(struct ol_txrx_pdev_t *pdev)
{
	qdf_spin_lock_bh(&pdev->txq_log_spinlock);
	qdf_mem_zero(&pdev->txq_log, sizeof(pdev->txq_log));
	pdev->txq_log.size = OL_TXQ_LOG_SIZE;
	pdev->txq_log.oldest_record_offset = 0;
	pdev->txq_log.offset = 0;
	pdev->txq_log.allow_wrap = 1;
	pdev->txq_log.wrapped = 0;
	qdf_spin_unlock_bh(&pdev->txq_log_spinlock);
}
#endif /* defined(DEBUG_HL_LOGGING) */

/*--- queue state printouts -------------------------------------------------*/

#if TXRX_DEBUG_LEVEL > 5

/**
 * ol_tx_queue_display() - show tx queue info
 * @txq: pointer to txq frames
 * @indent: indent
 *
 * Return: None
 */
static void
ol_tx_queue_display(struct ol_tx_frms_queue_t *txq, int indent)
{
	char *state;

	state = (txq->flag == ol_tx_queue_active) ? "active" : "paused";
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "%*stxq %pK (%s): %d frms, %d bytes\n",
		  indent, " ", txq, state, txq->frms, txq->bytes);
}

void
ol_tx_queues_display(struct ol_txrx_pdev_t *pdev)
{
	struct ol_txrx_vdev_t *vdev;

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "pdev %pK tx queues:\n", pdev);
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		struct ol_txrx_peer_t *peer;
		int i;

		for (i = 0; i < QDF_ARRAY_SIZE(vdev->txqs); i++) {
			if (vdev->txqs[i].frms == 0)
				continue;

			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
				  "vdev %d (%pK), txq %d\n", vdev->vdev_id,
				  vdev, i);
			ol_tx_queue_display(&vdev->txqs[i], 4);
		}
		TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
			for (i = 0; i < QDF_ARRAY_SIZE(peer->txqs); i++) {
				if (peer->txqs[i].frms == 0)
					continue;

				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_INFO_LOW,
					  "peer %d (%pK), txq %d\n",
					  peer->peer_ids[0], vdev, i);
				ol_tx_queue_display(&peer->txqs[i], 6);
			}
		}
	}
}
#endif

#endif /* defined(CONFIG_HL_SUPPORT) */

#if defined(CONFIG_HL_SUPPORT)

/**
 * ol_txrx_pdev_pause() - pause network queues for each vdev
 * @pdev: pdev handle
 * @reason: reason
 *
 * Return: none
 */
void ol_txrx_pdev_pause(struct ol_txrx_pdev_t *pdev, uint32_t reason)
{
	struct ol_txrx_vdev_t *vdev = NULL, *tmp;

	TAILQ_FOREACH_SAFE(vdev, &pdev->vdev_list, vdev_list_elem, tmp) {
		cdp_fc_vdev_pause(cds_get_context(QDF_MODULE_ID_SOC),
				  vdev->vdev_id, reason, 0);
	}

}

/**
 * ol_txrx_pdev_unpause() - unpause network queues for each vdev
 * @pdev: pdev handle
 * @reason: reason
 *
 * Return: none
 */
void ol_txrx_pdev_unpause(struct ol_txrx_pdev_t *pdev, uint32_t reason)
{
	struct ol_txrx_vdev_t *vdev = NULL, *tmp;

	TAILQ_FOREACH_SAFE(vdev, &pdev->vdev_list, vdev_list_elem, tmp) {
		cdp_fc_vdev_unpause(cds_get_context(QDF_MODULE_ID_SOC),
				    vdev->vdev_id, reason, 0);
	}

}
#endif

#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL

/**
 * ol_tx_vdev_has_tx_queue_group() - check for vdev having txq groups
 * @group: pointer to tx queue grpup
 * @vdev_id: vdev id
 *
 * Return: true if vedv has txq groups
 */
static bool
ol_tx_vdev_has_tx_queue_group(
	struct ol_tx_queue_group_t *group,
	u_int8_t vdev_id)
{
	u_int16_t vdev_bitmap;

	vdev_bitmap = OL_TXQ_GROUP_VDEV_ID_MASK_GET(group->membership);
	if (OL_TXQ_GROUP_VDEV_ID_BIT_MASK_GET(vdev_bitmap, vdev_id))
		return true;

	return false;
}

/**
 * ol_tx_ac_has_tx_queue_group() - check for ac having txq groups
 * @group: pointer to tx queue grpup
 * @ac: access category
 *
 * Return: true if vedv has txq groups
 */
static bool
ol_tx_ac_has_tx_queue_group(
	struct ol_tx_queue_group_t *group,
	u_int8_t ac)
{
	u_int16_t ac_bitmap;

	ac_bitmap = OL_TXQ_GROUP_AC_MASK_GET(group->membership);
	if (OL_TXQ_GROUP_AC_BIT_MASK_GET(ac_bitmap, ac))
		return true;

	return false;
}

#ifdef FEATURE_HL_DBS_GROUP_CREDIT_SHARING
static inline struct ol_tx_queue_group_t *
ol_tx_txq_find_other_group(struct ol_txrx_pdev_t *pdev,
			   struct ol_tx_queue_group_t *txq_grp)
{
	int i;
	struct ol_tx_queue_group_t *other_grp = NULL;

	for (i = 0; i < OL_TX_MAX_TXQ_GROUPS; i++) {
		if (&pdev->txq_grps[i] != txq_grp) {
			other_grp = &pdev->txq_grps[i];
			break;
		}
	}
	return other_grp;
}

u32 ol_tx_txq_group_credit_limit(
	struct ol_txrx_pdev_t *pdev,
	struct ol_tx_frms_queue_t *txq,
	u32 credit)
{
	struct ol_tx_queue_group_t *txq_grp = txq->group_ptrs[0];
	struct ol_tx_queue_group_t *other_grp;
	u32 ask;
	u32 updated_credit;
	u32 credit_oth_grp;

	if (qdf_unlikely(!txq_grp))
		return credit;

	updated_credit = qdf_atomic_read(&txq_grp->credit);

	if (credit <= updated_credit)
		/* We have enough credits */
		return credit;

	ask = credit - updated_credit;
	other_grp = ol_tx_txq_find_other_group(pdev, txq_grp);
	if (qdf_unlikely(!other_grp))
		return credit;

	credit_oth_grp = qdf_atomic_read(&other_grp->credit);
	if (other_grp->frm_count < credit_oth_grp) {
		u32 spare = credit_oth_grp - other_grp->frm_count;

		if (pdev->limit_lend) {
			if (spare > pdev->min_reserve)
				spare -= pdev->min_reserve;
			else
				spare = 0;
		}
		updated_credit += min(spare, ask);
	}
	return updated_credit;
}

u32 ol_tx_txq_update_borrowed_group_credits(struct ol_txrx_pdev_t *pdev,
					    struct ol_tx_frms_queue_t *txq,
					    u32 credits_used)
{
	struct ol_tx_queue_group_t *txq_grp = txq->group_ptrs[0];
	u32 credits_cur_grp;
	u32 credits_brwd;

	if (qdf_unlikely(!txq_grp))
		return credits_used;

	credits_cur_grp = qdf_atomic_read(&txq_grp->credit);
	if (credits_used > credits_cur_grp) {
		struct ol_tx_queue_group_t *other_grp =
			ol_tx_txq_find_other_group(pdev, txq_grp);

		if (qdf_likely(other_grp)) {
			credits_brwd = credits_used - credits_cur_grp;
			/*
			 * All the credits were used from the active txq group.
			 */
			credits_used = credits_cur_grp;
			/* Deduct credits borrowed from other group */
			ol_txrx_update_group_credit(other_grp, -credits_brwd,
						    0);
		}
	}
	return credits_used;
}
#else /* FEATURE_HL_DBS_GROUP_CREDIT_SHARING */
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

			group_credit = qdf_atomic_read(
					&txq->group_ptrs[i]->credit);
			updated_credit = QDF_MIN(updated_credit, group_credit);
		}
	}

	credit = (updated_credit < 0) ? 0 : updated_credit;

	return credit;
}
#endif /* FEATURE_HL_DBS_GROUP_CREDIT_SHARING */

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
		if (txq->group_ptrs[i])
			ol_txrx_update_group_credit(txq->group_ptrs[i],
						    credit, absolute);
	}
	ol_tx_update_group_credit_stats(pdev);
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
				for (j = 0; j < OL_TX_MAX_GROUPS_PER_QUEUE; j++)
					vdev->txqs[i].group_ptrs[j] = grp_ptr;
			}
			qdf_spin_lock_bh(&pdev->peer_ref_mutex);
			/* Update peer queue group pointers */
			TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
				for (i = 0; i < OL_TX_NUM_TIDS; i++) {
					for (j = 0;
						j < OL_TX_MAX_GROUPS_PER_QUEUE;
							j++)
						peer->txqs[i].group_ptrs[j] =
							grp_ptr;
				}
			}
			qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
			break;
		}
	}
}

void ol_tx_txq_set_group_ptr(
	struct ol_tx_frms_queue_t *txq,
	struct ol_tx_queue_group_t *grp_ptr)
{
	u_int8_t i;

	for (i = 0; i < OL_TX_MAX_GROUPS_PER_QUEUE; i++)
		txq->group_ptrs[i] = grp_ptr;
}

void ol_tx_set_peer_group_ptr(
	ol_txrx_pdev_handle pdev,
	struct ol_txrx_peer_t *peer,
	u_int8_t vdev_id,
	u_int8_t tid)
{
	u_int8_t i, j = 0;
	struct ol_tx_queue_group_t *group = NULL;

	for (i = 0; i < OL_TX_MAX_GROUPS_PER_QUEUE; i++)
		peer->txqs[tid].group_ptrs[i] = NULL;

	for (i = 0; i < OL_TX_MAX_TXQ_GROUPS; i++) {
		group = &pdev->txq_grps[i];
		if (ol_tx_vdev_has_tx_queue_group(group, vdev_id)) {
			if (tid < OL_TX_NUM_QOS_TIDS) {
				if (ol_tx_ac_has_tx_queue_group(
						group,
						TXRX_TID_TO_WMM_AC(tid))) {
					peer->txqs[tid].group_ptrs[j] = group;
					j++;
				}
			} else {
				peer->txqs[tid].group_ptrs[j] = group;
				j++;
			}
		}
		if (j >= OL_TX_MAX_GROUPS_PER_QUEUE)
			break;
	}
}

u_int32_t ol_tx_get_max_tx_groups_supported(struct ol_txrx_pdev_t *pdev)
{
		return OL_TX_MAX_TXQ_GROUPS;
}
#endif /* FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL */

#if defined(FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL) && \
	defined(FEATURE_HL_DBS_GROUP_CREDIT_SHARING)
void ol_tx_update_grp_frm_count(struct ol_tx_frms_queue_t *txq, int num_frms)
{
	int i;

	if (!num_frms || !txq) {
		ol_txrx_dbg("Invalid params\n");
		return;
	}

	for (i = 0; i < OL_TX_MAX_GROUPS_PER_QUEUE; i++) {
		if (txq->group_ptrs[i]) {
			txq->group_ptrs[i]->frm_count += num_frms;
			qdf_assert(txq->group_ptrs[i]->frm_count >= 0);
		}
	}
}
#endif

/*--- End of LL tx throttle queue code ---------------------------------------*/
