/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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

/*=== header file includes ===*/
/* generic utilities */
#include <qdf_nbuf.h>           /* qdf_nbuf_t, etc. */
#include <qdf_mem.h>         /* qdf_mem_malloc */

/* external interfaces */
#include <ol_txrx_api.h>        /* ol_txrx_pdev_handle */
#include <ol_txrx_htt_api.h>    /* ol_rx_addba_handler, etc. */
#include <ol_ctrl_txrx_api.h>   /* ol_ctrl_rx_addba_complete */
#include <ol_htt_rx_api.h>      /* htt_rx_desc_frame_free */
#include <ol_ctrl_txrx_api.h>   /* ol_rx_err */

/* datapath internal interfaces */
#include <ol_txrx_peer_find.h>  /* ol_txrx_peer_find_by_id */
#include <ol_txrx_internal.h>   /* TXRX_ASSERT */
#include <ol_rx_reorder_timeout.h>      /* OL_RX_REORDER_TIMEOUT_REMOVE, etc. */
#include <ol_rx_reorder.h>
#include <ol_rx_defrag.h>

/*=== data types and defines ===*/
#define OL_RX_REORDER_ROUND_PWR2(value) g_log2ceil[value]

/*=== global variables ===*/

static char g_log2ceil[] = {
	1,                      /* 0 -> 1 */
	1,                      /* 1 -> 1 */
	2,                      /* 2 -> 2 */
	4, 4,                   /* 3-4 -> 4 */
	8, 8, 8, 8,             /* 5-8 -> 8 */
	16, 16, 16, 16, 16, 16, 16, 16, /* 9-16 -> 16 */
	32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32, 32, 32, 32, 32, 32, /* 17-32 -> 32 */
	64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 64, 64, 64, 64, 64, 64, /* 33-64 -> 64 */
};

/*=== function definitions ===*/

/*---*/

#define QCA_SUPPORT_RX_REORDER_RELEASE_CHECK 0
#define OL_RX_REORDER_IDX_START_SELF_SELECT(peer, tid, idx_start)  /* no-op */
#define OL_RX_REORDER_IDX_WRAP(idx, win_sz, win_sz_mask) { idx &= win_sz_mask; }
#define OL_RX_REORDER_IDX_MAX(win_sz, win_sz_mask) win_sz_mask
#define OL_RX_REORDER_IDX_INIT(seq_num, win_sz, win_sz_mask) 0  /* n/a */
#define OL_RX_REORDER_NO_HOLES(rx_reorder) 0
#define OL_RX_REORDER_MPDU_CNT_INCR(rx_reorder, incr)   /* n/a */
#define OL_RX_REORDER_MPDU_CNT_DECR(rx_reorder, decr)   /* n/a */

/*---*/

/* reorder array elements are known to be non-NULL */
#define OL_RX_REORDER_LIST_APPEND(head_msdu, tail_msdu, rx_reorder_array_elem) \
	do {								\
		if (tail_msdu) {					\
			qdf_nbuf_set_next(tail_msdu,			\
					  rx_reorder_array_elem->head); \
		}							\
	} while (0)

/* functions called by txrx components */

void ol_rx_reorder_init(struct ol_rx_reorder_t *rx_reorder, uint8_t tid)
{
	rx_reorder->win_sz = 1;
	rx_reorder->win_sz_mask = 0;
	rx_reorder->array = &rx_reorder->base;
	rx_reorder->base.head = rx_reorder->base.tail = NULL;
	rx_reorder->tid = tid;
	rx_reorder->defrag_timeout_ms = 0;

	rx_reorder->defrag_waitlist_elem.tqe_next = NULL;
	rx_reorder->defrag_waitlist_elem.tqe_prev = NULL;
}

static enum htt_rx_status
ol_rx_reorder_seq_num_check(
			    struct ol_txrx_pdev_t *pdev,
			    struct ol_txrx_peer_t *peer,
			    unsigned int tid, unsigned int seq_num)
{
	unsigned int seq_num_delta;

	/* don't check the new seq_num against last_seq
	   if last_seq is not valid */
	if (peer->tids_last_seq[tid] == IEEE80211_SEQ_MAX)
		return htt_rx_status_ok;

	/*
	 * For duplicate detection, it might be helpful to also check
	 * whether the retry bit is set or not - a strict duplicate packet
	 * should be the one with retry bit set.
	 * However, since many implementations do not set the retry bit,
	 * and since this same function is also used for filtering out
	 * late-arriving frames (frames that arive after their rx reorder
	 * timeout has expired) which are not retries, don't bother checking
	 * the retry bit for now.
	 */
	/* note: if new seq_num == old seq_num, seq_num_delta = 4095 */
	seq_num_delta = (seq_num - 1 - peer->tids_last_seq[tid]) &
		(IEEE80211_SEQ_MAX - 1);     /* account for wraparound */

	if (seq_num_delta > (IEEE80211_SEQ_MAX >> 1)) {
		return htt_rx_status_err_replay;
		/* or maybe htt_rx_status_err_dup */
	}
	return htt_rx_status_ok;
}

/**
 * ol_rx_seq_num_check() - Does duplicate detection for mcast packets and
 *                           duplicate detection & check for out-of-order
 *                           packets for unicast packets.
 * @pdev:                        Pointer to pdev maintained by OL
 * @peer:                        Pointer to peer structure maintained by OL
 * @tid:                         TID value passed as part of HTT msg by f/w
 * @rx_mpdu_desc:                Pointer to Rx Descriptor for the given MPDU
 *
 *  This function
 *      1) For Multicast Frames -- does duplicate detection
 *          A frame is considered duplicate & dropped if it has a seq.number
 *          which is received twice in succession and with the retry bit set
 *          in the second case.
 *          A frame which is older than the last sequence number received
 *          is not considered duplicate but out-of-order. This function does
 *          perform out-of-order check for multicast frames, which is in
 *          keeping with the 802.11 2012 spec section 9.3.2.10
 *      2) For Unicast Frames -- does duplicate detection & out-of-order check
 *          only for non-aggregation tids.
 *
 * Return:        Returns htt_rx_status_err_replay, if packet needs to be
 *                dropped, htt_rx_status_ok otherwise.
 */
enum htt_rx_status
ol_rx_seq_num_check(struct ol_txrx_pdev_t *pdev,
					struct ol_txrx_peer_t *peer,
					uint8_t tid,
					void *rx_mpdu_desc)
{
	uint16_t pkt_tid = 0xffff;
	uint16_t seq_num = IEEE80211_SEQ_MAX;
	bool retry = 0;

	seq_num = htt_rx_mpdu_desc_seq_num(pdev->htt_pdev, rx_mpdu_desc, false);

	 /* For mcast packets, we only the dup-detection, not re-order check */

	if (qdf_unlikely(OL_RX_MCAST_TID == tid)) {

		pkt_tid = htt_rx_mpdu_desc_tid(pdev->htt_pdev, rx_mpdu_desc);

		/* Invalid packet TID, expected only for HL */
		/* Pass the packet on */
		if (qdf_unlikely(pkt_tid >= OL_TXRX_NUM_EXT_TIDS))
			return htt_rx_status_ok;

		retry = htt_rx_mpdu_desc_retry(pdev->htt_pdev, rx_mpdu_desc);

		/*
		 * At this point, we define frames to be duplicate if they
		 * arrive "ONLY" in succession with the same sequence number
		 * and the last one has the retry bit set. For an older frame,
		 * we consider that as an out of order frame, and hence do not
		 * perform the dup-detection or out-of-order check for multicast
		 * frames as per discussions & spec.
		 * Hence "seq_num <= last_seq_num" check is not necessary.
		 */
		if (qdf_unlikely(retry &&
			(seq_num == peer->tids_mcast_last_seq[pkt_tid]))) {
			/* drop mcast */
			TXRX_STATS_INCR(pdev, priv.rx.err.msdu_mc_dup_drop);
			return htt_rx_status_err_replay;
		}

		/*
		 * This is a multicast packet likely to be passed on...
		 * Set the mcast last seq number here
		 * This is fairly accurate since:
		 * a) f/w sends multicast as separate PPDU/HTT messages
		 * b) Mcast packets are not aggregated & hence single
		 * c) Result of b) is that, flush / release bit is set
		 *    always on the mcast packets, so likely to be
		 *    immediatedly released.
		 */
		peer->tids_mcast_last_seq[pkt_tid] = seq_num;
		return htt_rx_status_ok;
	} else
		return ol_rx_reorder_seq_num_check(pdev, peer, tid, seq_num);
}


void
ol_rx_reorder_store(struct ol_txrx_pdev_t *pdev,
		    struct ol_txrx_peer_t *peer,
		    unsigned int tid,
		    unsigned int idx, qdf_nbuf_t head_msdu,
		    qdf_nbuf_t tail_msdu)
{
	struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;

	idx &= peer->tids_rx_reorder[tid].win_sz_mask;
	rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[idx];
	if (rx_reorder_array_elem->head) {
		qdf_nbuf_set_next(rx_reorder_array_elem->tail, head_msdu);
	} else {
		rx_reorder_array_elem->head = head_msdu;
		OL_RX_REORDER_MPDU_CNT_INCR(&peer->tids_rx_reorder[tid], 1);
	}
	rx_reorder_array_elem->tail = tail_msdu;
}

void
ol_rx_reorder_release(struct ol_txrx_vdev_t *vdev,
		      struct ol_txrx_peer_t *peer,
		      unsigned int tid, unsigned int idx_start,
		      unsigned int idx_end)
{
	unsigned int idx;
	unsigned int win_sz, win_sz_mask;
	struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
	qdf_nbuf_t head_msdu;
	qdf_nbuf_t tail_msdu;

	OL_RX_REORDER_IDX_START_SELF_SELECT(peer, tid, &idx_start);
	/* may get reset below */
	peer->tids_next_rel_idx[tid] = (uint16_t) idx_end;

	win_sz = peer->tids_rx_reorder[tid].win_sz;
	win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;
	idx_start &= win_sz_mask;
	idx_end &= win_sz_mask;
	rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[idx_start];

	head_msdu = rx_reorder_array_elem->head;
	tail_msdu = rx_reorder_array_elem->tail;
	rx_reorder_array_elem->head = rx_reorder_array_elem->tail = NULL;
	if (head_msdu)
		OL_RX_REORDER_MPDU_CNT_DECR(&peer->tids_rx_reorder[tid], 1);

	idx = (idx_start + 1);
	OL_RX_REORDER_IDX_WRAP(idx, win_sz, win_sz_mask);
	while (idx != idx_end) {
		rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[idx];
		if (rx_reorder_array_elem->head) {
			OL_RX_REORDER_MPDU_CNT_DECR(&peer->tids_rx_reorder[tid],
						    1);
			OL_RX_REORDER_LIST_APPEND(head_msdu, tail_msdu,
						  rx_reorder_array_elem);
			tail_msdu = rx_reorder_array_elem->tail;
		}
		rx_reorder_array_elem->head = rx_reorder_array_elem->tail =
						      NULL;
		idx++;
		OL_RX_REORDER_IDX_WRAP(idx, win_sz, win_sz_mask);
	}
	if (head_msdu) {
		uint16_t seq_num;
		htt_pdev_handle htt_pdev = vdev->pdev->htt_pdev;

		/*
		 * This logic is not quite correct - the last_seq value should
		 * be the sequence number of the final MPDU released rather than
		 * the initial MPDU released.
		 * However, tracking the sequence number of the first MPDU in
		 * the released batch works well enough:
		 * For Peregrine and Rome, the last_seq is checked only for
		 * non-aggregate cases, where only one MPDU at a time is
		 * released.
		 * For Riva, Pronto, and Northstar, the last_seq is checked to
		 * filter out late-arriving rx frames, whose sequence number
		 * will be less than the first MPDU in this release batch.
		 */
		seq_num = htt_rx_mpdu_desc_seq_num(
			htt_pdev,
			htt_rx_msdu_desc_retrieve(htt_pdev,
						  head_msdu), false);
		peer->tids_last_seq[tid] = seq_num;
		/* rx_opt_proc takes a NULL-terminated list of msdu netbufs */
		qdf_nbuf_set_next(tail_msdu, NULL);
		peer->rx_opt_proc(vdev, peer, tid, head_msdu);
	}
	/*
	 * If the rx reorder timeout is handled by host SW rather than the
	 * target's rx reorder logic, then stop the timer here.
	 * (If there are remaining rx holes, then the timer will be restarted.)
	 */
	OL_RX_REORDER_TIMEOUT_REMOVE(peer, tid);
}

void
ol_rx_reorder_flush(struct ol_txrx_vdev_t *vdev,
		    struct ol_txrx_peer_t *peer,
		    unsigned int tid,
		    unsigned int idx_start,
		    unsigned int idx_end, enum htt_rx_flush_action action)
{
	struct ol_txrx_pdev_t *pdev;
	unsigned int win_sz;
	uint8_t win_sz_mask;
	struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
	qdf_nbuf_t head_msdu = NULL;
	qdf_nbuf_t tail_msdu = NULL;

	pdev = vdev->pdev;
	win_sz = peer->tids_rx_reorder[tid].win_sz;
	win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;

	OL_RX_REORDER_IDX_START_SELF_SELECT(peer, tid, &idx_start);
	/* a idx_end value of 0xffff means to flush the entire array */
	if (idx_end == 0xffff) {
		idx_end = idx_start;
		/*
		 * The array is being flushed in entirety because the block
		 * ack window has been shifted to a new position that does not
		 * overlap with the old position.  (Or due to reception of a
		 * DELBA.)
		 * Thus, since the block ack window is essentially being reset,
		 * reset the "next release index".
		 */
		peer->tids_next_rel_idx[tid] =
			OL_RX_REORDER_IDX_INIT(0 /*n/a */, win_sz, win_sz_mask);
	} else {
		peer->tids_next_rel_idx[tid] = (uint16_t) idx_end;
	}

	idx_start &= win_sz_mask;
	idx_end &= win_sz_mask;

	do {
		rx_reorder_array_elem =
			&peer->tids_rx_reorder[tid].array[idx_start];
		idx_start = (idx_start + 1);
		OL_RX_REORDER_IDX_WRAP(idx_start, win_sz, win_sz_mask);

		if (rx_reorder_array_elem->head) {
			OL_RX_REORDER_MPDU_CNT_DECR(&peer->tids_rx_reorder[tid],
						    1);
			if (!head_msdu) {
				head_msdu = rx_reorder_array_elem->head;
				tail_msdu = rx_reorder_array_elem->tail;
				rx_reorder_array_elem->head = NULL;
				rx_reorder_array_elem->tail = NULL;
				continue;
			}
			qdf_nbuf_set_next(tail_msdu,
					  rx_reorder_array_elem->head);
			tail_msdu = rx_reorder_array_elem->tail;
			rx_reorder_array_elem->head =
				rx_reorder_array_elem->tail = NULL;
		}
	} while (idx_start != idx_end);

	ol_rx_defrag_waitlist_remove(peer, tid);

	if (head_msdu) {
		uint16_t seq_num;
		htt_pdev_handle htt_pdev = vdev->pdev->htt_pdev;

		seq_num = htt_rx_mpdu_desc_seq_num(
			htt_pdev,
			htt_rx_msdu_desc_retrieve(htt_pdev, head_msdu), false);
		peer->tids_last_seq[tid] = seq_num;
		/* rx_opt_proc takes a NULL-terminated list of msdu netbufs */
		qdf_nbuf_set_next(tail_msdu, NULL);
		if (action == htt_rx_flush_release) {
			peer->rx_opt_proc(vdev, peer, tid, head_msdu);
		} else {
			do {
				qdf_nbuf_t next;

				next = qdf_nbuf_next(head_msdu);
				htt_rx_desc_frame_free(pdev->htt_pdev,
						       head_msdu);
				head_msdu = next;
			} while (head_msdu);
		}
	}
	/*
	 * If the rx reorder array is empty, then reset the last_seq value -
	 * it is likely that a BAR or a sequence number shift caused the
	 * sequence number to jump, so the old last_seq value is not relevant.
	 */
	if (OL_RX_REORDER_NO_HOLES(&peer->tids_rx_reorder[tid]))
		peer->tids_last_seq[tid] = IEEE80211_SEQ_MAX;   /* invalid */

	OL_RX_REORDER_TIMEOUT_REMOVE(peer, tid);
}

void
ol_rx_reorder_first_hole(struct ol_txrx_peer_t *peer,
			 unsigned int tid, unsigned int *idx_end)
{
	unsigned int win_sz, win_sz_mask;
	unsigned int idx_start = 0, tmp_idx = 0;

	win_sz = peer->tids_rx_reorder[tid].win_sz;
	win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;

	OL_RX_REORDER_IDX_START_SELF_SELECT(peer, tid, &idx_start);
	tmp_idx++;
	OL_RX_REORDER_IDX_WRAP(tmp_idx, win_sz, win_sz_mask);
	/* bypass the initial hole */
	while (tmp_idx != idx_start &&
	       !peer->tids_rx_reorder[tid].array[tmp_idx].head) {
		tmp_idx++;
		OL_RX_REORDER_IDX_WRAP(tmp_idx, win_sz, win_sz_mask);
	}
	/* bypass the present frames following the initial hole */
	while (tmp_idx != idx_start &&
	       peer->tids_rx_reorder[tid].array[tmp_idx].head) {
		tmp_idx++;
		OL_RX_REORDER_IDX_WRAP(tmp_idx, win_sz, win_sz_mask);
	}
	/*
	 * idx_end is exclusive rather than inclusive.
	 * In other words, it is the index of the first slot of the second
	 * hole, rather than the index of the final present frame following
	 * the first hole.
	 */
	*idx_end = tmp_idx;
}

#ifdef HL_RX_AGGREGATION_HOLE_DETECTION

/**
 * ol_rx_reorder_detect_hole - ol rx reorder detect hole
 * @peer: ol_txrx_peer_t
 * @tid: tid
 * @idx_start: idx_start
 *
 * Return: void
 */
static void ol_rx_reorder_detect_hole(struct ol_txrx_peer_t *peer,
					uint32_t tid,
					uint32_t idx_start)
{
	uint32_t win_sz_mask, next_rel_idx, hole_size;

	if (tid >= OL_TXRX_NUM_EXT_TIDS) {
		ol_txrx_err("%s:  invalid tid, %u\n", __FUNCTION__, tid);
		return;
	}

	if (peer->tids_next_rel_idx[tid] == INVALID_REORDER_INDEX)
		return;

	win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;
	/* Return directly if block-ack not enable */
	if (win_sz_mask == 0)
		return;

	idx_start &= win_sz_mask;
	next_rel_idx = peer->tids_next_rel_idx[tid] & win_sz_mask;

	if (idx_start != next_rel_idx) {
		hole_size = ((int)idx_start - (int)next_rel_idx) & win_sz_mask;

		ol_rx_aggregation_hole(hole_size);
	}

	return;
}

#else

/**
 * ol_rx_reorder_detect_hole - ol rx reorder detect hole
 * @peer: ol_txrx_peer_t
 * @tid: tid
 * @idx_start: idx_start
 *
 * Return: void
 */
static void ol_rx_reorder_detect_hole(struct ol_txrx_peer_t *peer,
					uint32_t tid,
					uint32_t idx_start)
{
	/* no-op */
}

#endif

void
ol_rx_reorder_peer_cleanup(struct ol_txrx_vdev_t *vdev,
			   struct ol_txrx_peer_t *peer)
{
	int tid;

	for (tid = 0; tid < OL_TXRX_NUM_EXT_TIDS; tid++) {
		ol_rx_reorder_flush(vdev, peer, tid, 0, 0,
				    htt_rx_flush_discard);
	}
	OL_RX_REORDER_TIMEOUT_PEER_CLEANUP(peer);
}

/* functions called by HTT */

void
ol_rx_addba_handler(ol_txrx_pdev_handle pdev,
		    uint16_t peer_id,
		    uint8_t tid,
		    uint8_t win_sz, uint16_t start_seq_num, uint8_t failed)
{
	uint8_t round_pwr2_win_sz;
	unsigned int array_size;
	struct ol_txrx_peer_t *peer;
	struct ol_rx_reorder_t *rx_reorder;
	void *array_mem = NULL;

	if (tid >= OL_TXRX_NUM_EXT_TIDS) {
		ol_txrx_err("invalid tid, %u", tid);
		WARN_ON(1);
		return;
	}

	peer = ol_txrx_peer_find_by_id(pdev, peer_id);
	if (!peer) {
		ol_txrx_err("not able to find peer, %u", peer_id);
		return;
	}

	if (pdev->cfg.host_addba) {
		ol_ctrl_rx_addba_complete(pdev->ctrl_pdev,
					  &peer->mac_addr.raw[0], tid, failed);
	}
	if (failed)
		return;

	peer->tids_last_seq[tid] = IEEE80211_SEQ_MAX;   /* invalid */
	rx_reorder = &peer->tids_rx_reorder[tid];

	TXRX_ASSERT2(win_sz <= 64);
	round_pwr2_win_sz = OL_RX_REORDER_ROUND_PWR2(win_sz);
	array_size =
		round_pwr2_win_sz * sizeof(struct ol_rx_reorder_array_elem_t);

	array_mem = qdf_mem_malloc(array_size);
	if (!array_mem)
		return;

	if (rx_reorder->array != &rx_reorder->base) {
		ol_txrx_info("delete array for tid %d", tid);
		qdf_mem_free(rx_reorder->array);
	}

	rx_reorder->array = array_mem;
	rx_reorder->win_sz = win_sz;
	TXRX_ASSERT1(rx_reorder->array);

	rx_reorder->win_sz_mask = round_pwr2_win_sz - 1;
	rx_reorder->num_mpdus = 0;

	peer->tids_next_rel_idx[tid] =
		OL_RX_REORDER_IDX_INIT(start_seq_num, rx_reorder->win_sz,
				       rx_reorder->win_sz_mask);
}

void
ol_rx_delba_handler(ol_txrx_pdev_handle pdev, uint16_t peer_id, uint8_t tid)
{
	struct ol_txrx_peer_t *peer;
	struct ol_rx_reorder_t *rx_reorder;

	if (tid >= OL_TXRX_NUM_EXT_TIDS) {
		ol_txrx_err("invalid tid, %u", tid);
		WARN_ON(1);
		return;
	}

	peer = ol_txrx_peer_find_by_id(pdev, peer_id);
	if (!peer) {
		ol_txrx_err("not able to find peer, %u", peer_id);
		return;
	}

	peer->tids_next_rel_idx[tid] = INVALID_REORDER_INDEX;
	rx_reorder = &peer->tids_rx_reorder[tid];

	/* check that there really was a block ack agreement */
	TXRX_ASSERT1(rx_reorder->win_sz_mask != 0);
	/*
	 * Deallocate the old rx reorder array.
	 * The call to ol_rx_reorder_init below
	 * will reset rx_reorder->array to point to
	 * the single-element statically-allocated reorder array
	 * used for non block-ack cases.
	 */
	if (rx_reorder->array != &rx_reorder->base) {
		ol_txrx_dbg("delete reorder array, tid:%d",
			    tid);
		qdf_mem_free(rx_reorder->array);
	}

	/* set up the TID with default parameters (ARQ window size = 1) */
	ol_rx_reorder_init(rx_reorder, tid);
}

void
ol_rx_flush_handler(ol_txrx_pdev_handle pdev,
		    uint16_t peer_id,
		    uint8_t tid,
		    uint16_t idx_start,
		    uint16_t idx_end, enum htt_rx_flush_action action)
{
	struct ol_txrx_vdev_t *vdev = NULL;
	void *rx_desc;
	struct ol_txrx_peer_t *peer;
	int idx;
	struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
	htt_pdev_handle htt_pdev = pdev->htt_pdev;

	if (tid >= OL_TXRX_NUM_EXT_TIDS) {
		ol_txrx_err("%s:  invalid tid, %u\n", __FUNCTION__, tid);
		return;
	}

	peer = ol_txrx_peer_find_by_id(pdev, peer_id);
	if (peer)
		vdev = peer->vdev;
	else
		return;

	OL_RX_REORDER_TIMEOUT_MUTEX_LOCK(pdev);

	idx = idx_start & peer->tids_rx_reorder[tid].win_sz_mask;
	rx_reorder_array_elem = &peer->tids_rx_reorder[tid].array[idx];
	if (rx_reorder_array_elem->head) {
		rx_desc =
			htt_rx_msdu_desc_retrieve(htt_pdev,
						  rx_reorder_array_elem->head);
		if (htt_rx_msdu_is_frag(htt_pdev, rx_desc)) {
			ol_rx_reorder_flush_frag(htt_pdev, peer, tid,
						 idx_start);
			/*
			 * Assuming flush message sent separately for frags
			 * and for normal frames
			 */
			OL_RX_REORDER_TIMEOUT_MUTEX_UNLOCK(pdev);
			return;
		}
	}

	if (action == htt_rx_flush_release)
		ol_rx_reorder_detect_hole(peer, tid, idx_start);

	ol_rx_reorder_flush(vdev, peer, tid, idx_start, idx_end, action);
	/*
	 * If the rx reorder timeout is handled by host SW, see if there are
	 * remaining rx holes that require the timer to be restarted.
	 */
	OL_RX_REORDER_TIMEOUT_UPDATE(peer, tid);
	OL_RX_REORDER_TIMEOUT_MUTEX_UNLOCK(pdev);
}

void
ol_rx_pn_ind_handler(ol_txrx_pdev_handle pdev,
		     uint16_t peer_id,
		     uint8_t tid,
		     uint16_t seq_num_start,
		     uint16_t seq_num_end, uint8_t pn_ie_cnt, uint8_t *pn_ie)
{
	struct ol_txrx_vdev_t *vdev = NULL;
	void *rx_desc;
	struct ol_txrx_peer_t *peer;
	struct ol_rx_reorder_array_elem_t *rx_reorder_array_elem;
	unsigned int win_sz_mask;
	qdf_nbuf_t head_msdu = NULL;
	qdf_nbuf_t tail_msdu = NULL;
	htt_pdev_handle htt_pdev = pdev->htt_pdev;
	uint16_t seq_num;
	int i = 0;

	if (tid >= OL_TXRX_NUM_EXT_TIDS) {
		ol_txrx_err("%s:  invalid tid, %u\n", __FUNCTION__, tid);
		WARN_ON(1);
		return;
	}
	peer = ol_txrx_peer_find_by_id(pdev, peer_id);

	if (!peer) {
		/*
		 * If we can't find a peer send this packet to OCB interface
		 * using OCB self peer
		 */
		if (!ol_txrx_get_ocb_peer(pdev, &peer))
			peer = NULL;
	}

	if (peer)
		vdev = peer->vdev;
	else
		return;

	qdf_atomic_set(&peer->fw_pn_check, 1);
	/*TODO: Fragmentation case */
	win_sz_mask = peer->tids_rx_reorder[tid].win_sz_mask;
	seq_num_start &= win_sz_mask;
	seq_num_end &= win_sz_mask;
	seq_num = seq_num_start;

	do {
		rx_reorder_array_elem =
			&peer->tids_rx_reorder[tid].array[seq_num];

		if (rx_reorder_array_elem->head) {
			if (pn_ie_cnt && seq_num == (int)(pn_ie[i])) {
				qdf_nbuf_t msdu, next_msdu, mpdu_head,
					   mpdu_tail;
				static uint32_t last_pncheck_print_time;
				/* Do not need to initialize as C does it */

				uint32_t current_time_ms;
				union htt_rx_pn_t pn = { 0 };
				int index, pn_len;

				mpdu_head = msdu = rx_reorder_array_elem->head;
				mpdu_tail = rx_reorder_array_elem->tail;

				pn_ie_cnt--;
				i++;
				rx_desc = htt_rx_msdu_desc_retrieve(htt_pdev,
								    msdu);
				index = htt_rx_msdu_is_wlan_mcast(
					pdev->htt_pdev, rx_desc)
					? txrx_sec_mcast
					: txrx_sec_ucast;
				pn_len = pdev->rx_pn[peer->security[index].
						     sec_type].len;
				htt_rx_mpdu_desc_pn(htt_pdev, rx_desc, &pn,
						    pn_len);

				current_time_ms = qdf_system_ticks_to_msecs(
					qdf_system_ticks());
				if (TXRX_PN_CHECK_FAILURE_PRINT_PERIOD_MS <
				    (current_time_ms -
				     last_pncheck_print_time)) {
					last_pncheck_print_time =
						current_time_ms;
					ol_txrx_warn(
					   "Tgt PN check failed - TID %d, peer %pK "
					   "("QDF_MAC_ADDR_FMT")\n"
					   "    PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
					   "    new seq num = %d\n",
					   tid, peer,
					   QDF_MAC_ADDR_REF(peer->mac_addr.raw),
					   pn.pn128[1],
					   pn.pn128[0],
					   pn.pn128[0] & 0xffffffffffffULL,
					   htt_rx_mpdu_desc_seq_num(htt_pdev,
								    rx_desc,
								    false));
				} else {
					ol_txrx_dbg(
					   "Tgt PN check failed - TID %d, peer %pK "
					   "("QDF_MAC_ADDR_FMT")\n"
					   "    PN (u64 x2)= 0x%08llx %08llx (LSBs = %lld)\n"
					   "    new seq num = %d\n",
					   tid, peer,
					   QDF_MAC_ADDR_REF(peer->mac_addr.raw),
					   pn.pn128[1],
					   pn.pn128[0],
					   pn.pn128[0] & 0xffffffffffffULL,
					   htt_rx_mpdu_desc_seq_num(htt_pdev,
								    rx_desc,
								    false));
				}
				ol_rx_err(pdev->ctrl_pdev, vdev->vdev_id,
					  peer->mac_addr.raw, tid,
					  htt_rx_mpdu_desc_tsf32(htt_pdev,
								 rx_desc),
					  OL_RX_ERR_PN, mpdu_head, NULL, 0);

				/* free all MSDUs within this MPDU */
				do {
					next_msdu = qdf_nbuf_next(msdu);
					htt_rx_desc_frame_free(htt_pdev, msdu);
					if (msdu == mpdu_tail)
						break;
					msdu = next_msdu;
				} while (1);

			} else {
				if (!head_msdu) {
					head_msdu = rx_reorder_array_elem->head;
					tail_msdu = rx_reorder_array_elem->tail;
				} else {
					qdf_nbuf_set_next(
						tail_msdu,
						rx_reorder_array_elem->head);
					tail_msdu = rx_reorder_array_elem->tail;
				}
			}
			rx_reorder_array_elem->head = NULL;
			rx_reorder_array_elem->tail = NULL;
		}
		seq_num = (seq_num + 1) & win_sz_mask;
	} while (seq_num != seq_num_end);

	if (head_msdu) {
		/* rx_opt_proc takes a NULL-terminated list of msdu netbufs */
		qdf_nbuf_set_next(tail_msdu, NULL);
		peer->rx_opt_proc(vdev, peer, tid, head_msdu);
	}
}

#if defined(ENABLE_RX_REORDER_TRACE)

A_STATUS ol_rx_reorder_trace_attach(ol_txrx_pdev_handle pdev)
{
	int num_elems;

	num_elems = 1 << TXRX_RX_REORDER_TRACE_SIZE_LOG2;
	pdev->rx_reorder_trace.idx = 0;
	pdev->rx_reorder_trace.cnt = 0;
	pdev->rx_reorder_trace.mask = num_elems - 1;
	pdev->rx_reorder_trace.data = qdf_mem_malloc(
		sizeof(*pdev->rx_reorder_trace.data) * num_elems);
	if (!pdev->rx_reorder_trace.data)
		return A_NO_MEMORY;

	while (--num_elems >= 0)
		pdev->rx_reorder_trace.data[num_elems].seq_num = 0xffff;

	return A_OK;
}

void ol_rx_reorder_trace_detach(ol_txrx_pdev_handle pdev)
{
	qdf_mem_free(pdev->rx_reorder_trace.data);
}

void
ol_rx_reorder_trace_add(ol_txrx_pdev_handle pdev,
			uint8_t tid,
			uint16_t reorder_idx, uint16_t seq_num, int num_mpdus)
{
	uint32_t idx = pdev->rx_reorder_trace.idx;

	pdev->rx_reorder_trace.data[idx].tid = tid;
	pdev->rx_reorder_trace.data[idx].reorder_idx = reorder_idx;
	pdev->rx_reorder_trace.data[idx].seq_num = seq_num;
	pdev->rx_reorder_trace.data[idx].num_mpdus = num_mpdus;
	pdev->rx_reorder_trace.cnt++;
	idx++;
	pdev->rx_reorder_trace.idx = idx & pdev->rx_reorder_trace.mask;
}

void
ol_rx_reorder_trace_display(ol_txrx_pdev_handle pdev, int just_once, int limit)
{
	static int print_count;
	uint32_t i, start, end;
	uint64_t cnt;
	int elems;

	if (print_count != 0 && just_once)
		return;

	print_count++;

	end = pdev->rx_reorder_trace.idx;
	if (pdev->rx_reorder_trace.data[end].seq_num == 0xffff) {
		/* trace log has not yet wrapped around - start at the top */
		start = 0;
		cnt = 0;
	} else {
		start = end;
		cnt = pdev->rx_reorder_trace.cnt -
			(pdev->rx_reorder_trace.mask + 1);
	}
	elems = (end - 1 - start) & pdev->rx_reorder_trace.mask;
	if (limit > 0 && elems > limit) {
		int delta;

		delta = elems - limit;
		start += delta;
		start &= pdev->rx_reorder_trace.mask;
		cnt += delta;
	}

	i = start;
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
		  "           log       array seq");
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
		  "   count   idx  tid   idx  num (LSBs)");
	do {
		uint16_t seq_num, reorder_idx;

		seq_num = pdev->rx_reorder_trace.data[i].seq_num;
		reorder_idx = pdev->rx_reorder_trace.data[i].reorder_idx;
		if (seq_num < (1 << 14)) {
			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
				  "  %6lld  %4d  %3d  %4d  %4d (%d)",
				  cnt, i, pdev->rx_reorder_trace.data[i].tid,
				  reorder_idx, seq_num, seq_num & 63);
		} else {
			int err = TXRX_SEQ_NUM_ERR(seq_num);

			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
				  "  %6lld  %4d err %d (%d MPDUs)",
				  cnt, i, err,
				  pdev->rx_reorder_trace.data[i].num_mpdus);
		}
		cnt++;
		i++;
		i &= pdev->rx_reorder_trace.mask;
	} while (i != end);
}

#endif /* ENABLE_RX_REORDER_TRACE */
