/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
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

#include <dp_txrx.h>
#include "dp_peer.h"
#include "dp_internal.h"
#include "dp_types.h"
#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_peer_ops.h>
#include <cds_sched.h>

/* Timeout in ms to wait for a DP rx thread */
#ifdef HAL_CONFIG_SLUB_DEBUG_ON
#define DP_RX_THREAD_WAIT_TIMEOUT 4000
#else
#define DP_RX_THREAD_WAIT_TIMEOUT 2000
#endif

#define DP_RX_TM_DEBUG 0
#if DP_RX_TM_DEBUG
/**
 * dp_rx_tm_walk_skb_list() - Walk skb list and print members
 * @nbuf_list - nbuf list to print
 *
 * Returns: None
 */
static inline void dp_rx_tm_walk_skb_list(qdf_nbuf_t nbuf_list)
{
	qdf_nbuf_t nbuf;
	int i = 0;

	nbuf = nbuf_list;
	while (nbuf) {
		dp_debug("%d nbuf:%pK nbuf->next:%pK nbuf->data:%pK", i,
			 nbuf, qdf_nbuf_next(nbuf), qdf_nbuf_data(nbuf));
		nbuf = qdf_nbuf_next(nbuf);
		i++;
	}
}
#else
static inline void dp_rx_tm_walk_skb_list(qdf_nbuf_t nbuf_list)
{ }
#endif /* DP_RX_TM_DEBUG */

/**
 * dp_rx_tm_get_soc_handle() - get soc handle from struct dp_rx_tm_handle_cmn
 * @rx_tm_handle_cmn - rx thread manager cmn handle
 *
 * Returns: ol_txrx_soc_handle on success, NULL on failure.
 */
static inline
ol_txrx_soc_handle dp_rx_tm_get_soc_handle(struct dp_rx_tm_handle_cmn *rx_tm_handle_cmn)
{
	struct dp_txrx_handle_cmn *txrx_handle_cmn;
	ol_txrx_soc_handle soc;

	txrx_handle_cmn =
		dp_rx_thread_get_txrx_handle(rx_tm_handle_cmn);

	soc = dp_txrx_get_soc_from_ext_handle(txrx_handle_cmn);
	return soc;
}

/**
 * dp_rx_tm_thread_dump_stats() - display stats for a rx_thread
 * @rx_thread - rx_thread pointer for which the stats need to be
 *            displayed
 *
 * Returns: None
 */
static void dp_rx_tm_thread_dump_stats(struct dp_rx_thread *rx_thread)
{
	uint8_t reo_ring_num;
	uint32_t off = 0;
	char nbuf_queued_string[100];
	uint32_t total_queued = 0;
	uint32_t temp = 0;

	qdf_mem_zero(nbuf_queued_string, sizeof(nbuf_queued_string));

	for (reo_ring_num = 0; reo_ring_num < DP_RX_TM_MAX_REO_RINGS;
	     reo_ring_num++) {
		temp = rx_thread->stats.nbuf_queued[reo_ring_num];
		if (!temp)
			continue;
		total_queued += temp;
		if (off >= sizeof(nbuf_queued_string))
			continue;
		off += qdf_scnprintf(&nbuf_queued_string[off],
				     sizeof(nbuf_queued_string) - off,
				     "reo[%u]:%u ", reo_ring_num, temp);
	}

	if (!total_queued)
		return;

	dp_info("thread:%u - qlen:%u queued:(total:%u %s) dequeued:%u stack:%u gro_flushes: %u gro_flushes_by_vdev_del: %u rx_flushes: %u max_len:%u invalid(peer:%u vdev:%u rx-handle:%u others:%u enq fail:%u)",
		rx_thread->id,
		qdf_nbuf_queue_head_qlen(&rx_thread->nbuf_queue),
		total_queued,
		nbuf_queued_string,
		rx_thread->stats.nbuf_dequeued,
		rx_thread->stats.nbuf_sent_to_stack,
		rx_thread->stats.gro_flushes,
		rx_thread->stats.gro_flushes_by_vdev_del,
		rx_thread->stats.rx_flushed,
		rx_thread->stats.nbufq_max_len,
		rx_thread->stats.dropped_invalid_peer,
		rx_thread->stats.dropped_invalid_vdev,
		rx_thread->stats.dropped_invalid_os_rx_handles,
		rx_thread->stats.dropped_others,
		rx_thread->stats.dropped_enq_fail);
}

QDF_STATUS dp_rx_tm_dump_stats(struct dp_rx_tm_handle *rx_tm_hdl)
{
	int i;

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		if (!rx_tm_hdl->rx_thread[i])
			continue;
		dp_rx_tm_thread_dump_stats(rx_tm_hdl->rx_thread[i]);
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_ALLOW_PKT_DROPPING
/*
 * dp_check_and_update_pending() - Check and Set RX Pending flag
 * @tm_handle_cmn - DP thread pointer
 *
 * Returns: QDF_STATUS_SUCCESS on success or qdf error code on
 * failure
 */
static inline
QDF_STATUS dp_check_and_update_pending(struct dp_rx_tm_handle_cmn
				       *tm_handle_cmn)
{
	struct dp_txrx_handle_cmn *txrx_handle_cmn;
	struct dp_rx_tm_handle *rx_tm_hdl =
		    (struct dp_rx_tm_handle *)tm_handle_cmn;
	struct dp_soc *dp_soc;
	uint32_t rx_pending_hl_threshold;
	uint32_t rx_pending_lo_threshold;
	uint32_t nbuf_queued_total = 0;
	uint32_t nbuf_dequeued_total = 0;
	uint32_t rx_flushed_total = 0;
	uint32_t pending = 0;
	int i;

	txrx_handle_cmn =
		dp_rx_thread_get_txrx_handle(tm_handle_cmn);
	if (!txrx_handle_cmn) {
		dp_err("invalid txrx_handle_cmn!");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	dp_soc = (struct dp_soc *)dp_txrx_get_soc_from_ext_handle(
					txrx_handle_cmn);
	if (!dp_soc) {
		dp_err("invalid soc!");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	rx_pending_hl_threshold = wlan_cfg_rx_pending_hl_threshold(
				  dp_soc->wlan_cfg_ctx);
	rx_pending_lo_threshold = wlan_cfg_rx_pending_lo_threshold(
				  dp_soc->wlan_cfg_ctx);

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		if (likely(rx_tm_hdl->rx_thread[i])) {
			nbuf_queued_total +=
			    rx_tm_hdl->rx_thread[i]->stats.nbuf_queued_total;
			nbuf_dequeued_total +=
			    rx_tm_hdl->rx_thread[i]->stats.nbuf_dequeued;
			rx_flushed_total +=
			    rx_tm_hdl->rx_thread[i]->stats.rx_flushed;
		}
	}

	if (nbuf_queued_total > (nbuf_dequeued_total + rx_flushed_total))
		pending = nbuf_queued_total - (nbuf_dequeued_total +
					       rx_flushed_total);

	if (unlikely(pending > rx_pending_hl_threshold))
		qdf_atomic_set(&rx_tm_hdl->allow_dropping, 1);
	else if (pending < rx_pending_lo_threshold)
		qdf_atomic_set(&rx_tm_hdl->allow_dropping, 0);

	return QDF_STATUS_SUCCESS;
}

#else
static inline
QDF_STATUS dp_check_and_update_pending(struct dp_rx_tm_handle_cmn
				       *tm_handle_cmn)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_rx_tm_thread_enqueue() - enqueue nbuf list into rx_thread
 * @rx_thread - rx_thread in which the nbuf needs to be queued
 * @nbuf_list - list of packets to be queued into the thread
 *
 * Enqueue packet into rx_thread and wake it up. The function
 * moves the next pointer of the nbuf_list into the ext list of
 * the first nbuf for storage into the thread. Only the first
 * nbuf is queued into the thread nbuf queue. The reverse is
 * done at the time of dequeue.
 *
 * Returns: QDF_STATUS_SUCCESS on success or qdf error code on
 * failure
 */
static QDF_STATUS dp_rx_tm_thread_enqueue(struct dp_rx_thread *rx_thread,
					  qdf_nbuf_t nbuf_list)
{
	qdf_nbuf_t head_ptr, next_ptr_list;
	uint32_t temp_qlen;
	uint32_t num_elements_in_nbuf;
	uint32_t nbuf_queued;
	struct dp_rx_tm_handle_cmn *tm_handle_cmn;
	uint8_t reo_ring_num = QDF_NBUF_CB_RX_CTX_ID(nbuf_list);
	qdf_wait_queue_head_t *wait_q_ptr;
	uint8_t allow_dropping;

	tm_handle_cmn = rx_thread->rtm_handle_cmn;

	if (!tm_handle_cmn) {
		dp_alert("tm_handle_cmn is null!");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	wait_q_ptr = &rx_thread->wait_q;

	if (reo_ring_num >= DP_RX_TM_MAX_REO_RINGS) {
		dp_alert("incorrect ring %u", reo_ring_num);
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	num_elements_in_nbuf = QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(nbuf_list);
	nbuf_queued = num_elements_in_nbuf;

	allow_dropping = qdf_atomic_read(
		&((struct dp_rx_tm_handle *)tm_handle_cmn)->allow_dropping);
	if (unlikely(allow_dropping)) {
		qdf_nbuf_list_free(nbuf_list);
		rx_thread->stats.dropped_enq_fail += num_elements_in_nbuf;
		nbuf_queued = 0;
		goto enq_done;
	}

	dp_rx_tm_walk_skb_list(nbuf_list);

	head_ptr = nbuf_list;

	/* Ensure head doesn't have an ext list */
	while (qdf_unlikely(head_ptr && qdf_nbuf_get_ext_list(head_ptr))) {
		QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(head_ptr) = 1;
		num_elements_in_nbuf--;
		next_ptr_list = head_ptr->next;
		qdf_nbuf_set_next(head_ptr, NULL);
		/* count aggregated RX frame into enqueued stats */
		nbuf_queued += qdf_nbuf_get_gso_segs(head_ptr);
		qdf_nbuf_queue_head_enqueue_tail(&rx_thread->nbuf_queue,
						 head_ptr);
		head_ptr = next_ptr_list;
	}

	if (!head_ptr)
		goto enq_done;

	QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(head_ptr) = num_elements_in_nbuf;

	next_ptr_list = head_ptr->next;

	if (next_ptr_list) {
		/* move ->next pointer to ext list */
		qdf_nbuf_append_ext_list(head_ptr, next_ptr_list, 0);
		dp_debug("appended next_ptr_list %pK to nbuf %pK ext list %pK",
			 qdf_nbuf_next(nbuf_list), nbuf_list,
			 qdf_nbuf_get_ext_list(nbuf_list));
	}
	qdf_nbuf_set_next(head_ptr, NULL);

	qdf_nbuf_queue_head_enqueue_tail(&rx_thread->nbuf_queue, head_ptr);

enq_done:
	temp_qlen = qdf_nbuf_queue_head_qlen(&rx_thread->nbuf_queue);

	rx_thread->stats.nbuf_queued[reo_ring_num] += nbuf_queued;
	rx_thread->stats.nbuf_queued_total += nbuf_queued;

	dp_check_and_update_pending(tm_handle_cmn);

	if (temp_qlen > rx_thread->stats.nbufq_max_len)
		rx_thread->stats.nbufq_max_len = temp_qlen;

	dp_debug("enqueue packet thread %pK wait queue %pK qlen %u",
		 rx_thread, wait_q_ptr,
		 qdf_nbuf_queue_head_qlen(&rx_thread->nbuf_queue));

	qdf_set_bit(RX_POST_EVENT, &rx_thread->event_flag);
	qdf_wake_up_interruptible(wait_q_ptr);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_tm_thread_gro_flush_ind() - Rxthread flush ind post
 * @rx_thread: rx_thread in which the flush needs to be handled
 * @flush_code: flush code to differentiate low TPUT flush
 *
 * Return: QDF_STATUS_SUCCESS on success or qdf error code on
 * failure
 */
static QDF_STATUS
dp_rx_tm_thread_gro_flush_ind(struct dp_rx_thread *rx_thread,
			      enum dp_rx_gro_flush_code flush_code)
{
	struct dp_rx_tm_handle_cmn *tm_handle_cmn;
	qdf_wait_queue_head_t *wait_q_ptr;

	tm_handle_cmn = rx_thread->rtm_handle_cmn;
	wait_q_ptr = &rx_thread->wait_q;

	qdf_atomic_set(&rx_thread->gro_flush_ind, flush_code);

	dp_debug("Flush indication received");

	qdf_set_bit(RX_POST_EVENT, &rx_thread->event_flag);
	qdf_wake_up_interruptible(wait_q_ptr);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_thread_adjust_nbuf_list() - create an nbuf list from the frag list
 * @head - nbuf list to be created
 *
 * Returns: void
 */
static void dp_rx_thread_adjust_nbuf_list(qdf_nbuf_t head)
{
	qdf_nbuf_t next_ptr_list, nbuf_list;

	nbuf_list = head;
	if (head && QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(head) > 1) {
		/* move ext list to ->next pointer */
		next_ptr_list = qdf_nbuf_get_ext_list(head);
		qdf_nbuf_append_ext_list(head, NULL, 0);
		qdf_nbuf_set_next(nbuf_list, next_ptr_list);
		dp_rx_tm_walk_skb_list(nbuf_list);
	}
}

/**
 * dp_rx_tm_thread_dequeue() - dequeue nbuf list from rx_thread
 * @rx_thread - rx_thread from which the nbuf needs to be dequeued
 *
 * Returns: nbuf or nbuf_list dequeued from rx_thread
 */
static qdf_nbuf_t dp_rx_tm_thread_dequeue(struct dp_rx_thread *rx_thread)
{
	qdf_nbuf_t head;

	head = qdf_nbuf_queue_head_dequeue(&rx_thread->nbuf_queue);
	dp_rx_thread_adjust_nbuf_list(head);

	dp_debug("Dequeued %pK nbuf_list", head);
	return head;
}

/**
 * dp_rx_thread_process_nbufq() - process nbuf queue of a thread
 * @rx_thread - rx_thread whose nbuf queue needs to be processed
 *
 * Returns: 0 on success, error code on failure
 */
static int dp_rx_thread_process_nbufq(struct dp_rx_thread *rx_thread)
{
	qdf_nbuf_t nbuf_list;
	uint8_t vdev_id;
	ol_txrx_rx_fp stack_fn;
	ol_osif_vdev_handle osif_vdev;
	ol_txrx_soc_handle soc;
	uint32_t num_list_elements = 0;

	struct dp_txrx_handle_cmn *txrx_handle_cmn;

	txrx_handle_cmn =
		dp_rx_thread_get_txrx_handle(rx_thread->rtm_handle_cmn);

	soc = dp_txrx_get_soc_from_ext_handle(txrx_handle_cmn);
	if (!soc) {
		dp_err("invalid soc!");
		QDF_BUG(0);
		return -EFAULT;
	}

	dp_debug("enter: qlen  %u",
		 qdf_nbuf_queue_head_qlen(&rx_thread->nbuf_queue));

	nbuf_list = dp_rx_tm_thread_dequeue(rx_thread);
	while (nbuf_list) {
		num_list_elements =
			QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(nbuf_list);
		/* count aggregated RX frame into stats */
		num_list_elements += qdf_nbuf_get_gso_segs(nbuf_list);
		rx_thread->stats.nbuf_dequeued += num_list_elements;

		vdev_id = QDF_NBUF_CB_RX_VDEV_ID(nbuf_list);
		cdp_get_os_rx_handles_from_vdev(soc, vdev_id, &stack_fn,
						&osif_vdev);
		dp_debug("rx_thread %pK sending packet %pK to stack",
			 rx_thread, nbuf_list);
		if (!stack_fn || !osif_vdev ||
		    QDF_STATUS_SUCCESS != stack_fn(osif_vdev, nbuf_list)) {
			rx_thread->stats.dropped_invalid_os_rx_handles +=
							num_list_elements;
			qdf_nbuf_list_free(nbuf_list);
		} else {
			rx_thread->stats.nbuf_sent_to_stack +=
							num_list_elements;
		}
		nbuf_list = dp_rx_tm_thread_dequeue(rx_thread);
	}

	dp_debug("exit: qlen  %u",
		 qdf_nbuf_queue_head_qlen(&rx_thread->nbuf_queue));

	return 0;
}

/**
 * dp_rx_thread_gro_flush() - flush GRO packets for the RX thread
 * @rx_thread: rx_thread to be processed
 * @gro_flush_code: flush code to differentiating flushes
 *
 * Return: void
 */
static void dp_rx_thread_gro_flush(struct dp_rx_thread *rx_thread,
				   enum dp_rx_gro_flush_code gro_flush_code)
{
	dp_debug("flushing packets for thread %u", rx_thread->id);

	local_bh_disable();
	dp_rx_napi_gro_flush(&rx_thread->napi, gro_flush_code);
	local_bh_enable();

	rx_thread->stats.gro_flushes++;
}

/**
 * dp_rx_thread_sub_loop() - rx thread subloop
 * @rx_thread - rx_thread to be processed
 * @shutdown - pointer to shutdown variable
 *
 * The function handles shutdown and suspend events from other
 * threads and processes nbuf queue of a rx thread. In case a
 * shutdown event is received from some other wlan thread, the
 * function sets the shutdown pointer to true and returns
 *
 * Returns: 0 on success, error code on failure
 */
static int dp_rx_thread_sub_loop(struct dp_rx_thread *rx_thread, bool *shutdown)
{
	enum dp_rx_gro_flush_code gro_flush_code;

	while (true) {
		if (qdf_atomic_test_and_clear_bit(RX_SHUTDOWN_EVENT,
						  &rx_thread->event_flag)) {
			if (qdf_atomic_test_and_clear_bit(RX_SUSPEND_EVENT,
							  &rx_thread->event_flag)) {
				qdf_event_set(&rx_thread->suspend_event);
			}
			dp_debug("shutting down (%s) id %d pid %d",
				 qdf_get_current_comm(), rx_thread->id,
				 qdf_get_current_pid());
			*shutdown = true;
			break;
		}

		dp_rx_thread_process_nbufq(rx_thread);

		gro_flush_code = qdf_atomic_read(&rx_thread->gro_flush_ind);

		if (gro_flush_code ||
		    qdf_atomic_test_bit(RX_VDEV_DEL_EVENT,
					&rx_thread->event_flag)) {
			dp_rx_thread_gro_flush(rx_thread, gro_flush_code);
			qdf_atomic_set(&rx_thread->gro_flush_ind, 0);
		}

		if (qdf_atomic_test_and_clear_bit(RX_VDEV_DEL_EVENT,
						  &rx_thread->event_flag)) {
			rx_thread->stats.gro_flushes_by_vdev_del++;
			qdf_event_set(&rx_thread->vdev_del_event);
		}

		if (qdf_atomic_test_and_clear_bit(RX_SUSPEND_EVENT,
						  &rx_thread->event_flag)) {
			dp_debug("received suspend ind (%s) id %d pid %d",
				 qdf_get_current_comm(), rx_thread->id,
				 qdf_get_current_pid());
			qdf_event_set(&rx_thread->suspend_event);
			dp_debug("waiting for resume (%s) id %d pid %d",
				 qdf_get_current_comm(), rx_thread->id,
				 qdf_get_current_pid());
			qdf_wait_single_event(&rx_thread->resume_event, 0);
		}
		break;
	}
	return 0;
}

/**
 * dp_rx_thread_loop() - main dp rx thread loop
 * @arg: pointer to dp_rx_thread structure for the rx thread
 *
 * Return: thread exit code
 */
static int dp_rx_thread_loop(void *arg)
{
	struct dp_rx_thread *rx_thread = arg;
	bool shutdown = false;
	int status;
	struct dp_rx_tm_handle_cmn *tm_handle_cmn;

	if (!arg) {
		dp_err("bad Args passed");
		return 0;
	}

	tm_handle_cmn = rx_thread->rtm_handle_cmn;

	qdf_set_user_nice(qdf_get_current_task(), -1);
	qdf_set_wake_up_idle(true);

	qdf_event_set(&rx_thread->start_event);
	dp_info("starting rx_thread (%s) id %d pid %d", qdf_get_current_comm(),
		rx_thread->id, qdf_get_current_pid());
	while (!shutdown) {
		/* This implements the execution model algorithm */
		dp_debug("sleeping");
		status =
		    qdf_wait_queue_interruptible
				(rx_thread->wait_q,
				 qdf_atomic_test_bit(RX_POST_EVENT,
						     &rx_thread->event_flag) ||
				 qdf_atomic_test_bit(RX_SUSPEND_EVENT,
						     &rx_thread->event_flag) ||
				 qdf_atomic_test_bit(RX_VDEV_DEL_EVENT,
						     &rx_thread->event_flag));
		dp_debug("woken up");

		if (status == -ERESTARTSYS) {
			QDF_DEBUG_PANIC("wait_event_interruptible returned -ERESTARTSYS");
			break;
		}
		qdf_atomic_clear_bit(RX_POST_EVENT, &rx_thread->event_flag);
		dp_rx_thread_sub_loop(rx_thread, &shutdown);
	}

	/* If we get here the scheduler thread must exit */
	dp_info("exiting (%s) id %d pid %d", qdf_get_current_comm(),
		rx_thread->id, qdf_get_current_pid());
	qdf_event_set(&rx_thread->shutdown_event);
	qdf_exit_thread(QDF_STATUS_SUCCESS);

	return 0;
}

static int dp_rx_refill_thread_sub_loop(struct dp_rx_refill_thread *rx_thread,
					bool *shutdown)
{
	while (true) {
		if (qdf_atomic_test_and_clear_bit(RX_REFILL_SHUTDOWN_EVENT,
						  &rx_thread->event_flag)) {
			if (qdf_atomic_test_and_clear_bit(RX_REFILL_SUSPEND_EVENT,
							  &rx_thread->event_flag)) {
				qdf_event_set(&rx_thread->suspend_event);
			}
			dp_debug("shutting down (%s) pid %d",
				 qdf_get_current_comm(), qdf_get_current_pid());
			*shutdown = true;
			break;
		}

		dp_rx_refill_buff_pool_enqueue((struct dp_soc *)rx_thread->soc);

		if (qdf_atomic_test_and_clear_bit(RX_REFILL_SUSPEND_EVENT,
						  &rx_thread->event_flag)) {
			dp_debug("refill thread received suspend ind (%s) pid %d",
				 qdf_get_current_comm(),
				 qdf_get_current_pid());
			qdf_event_set(&rx_thread->suspend_event);
			dp_debug("refill thread waiting for resume (%s) pid %d",
				 qdf_get_current_comm(),
				 qdf_get_current_pid());
			qdf_wait_single_event(&rx_thread->resume_event, 0);
		}
		break;
	}
	return 0;
}

static int dp_rx_refill_thread_loop(void *arg)
{
	struct dp_rx_refill_thread *rx_thread = arg;
	bool shutdown = false;
	int status;

	if (!arg) {
		dp_err("bad Args passed");
		return 0;
	}

	qdf_set_user_nice(qdf_get_current_task(), -1);
	qdf_set_wake_up_idle(true);

	qdf_event_set(&rx_thread->start_event);
	dp_info("starting rx_refill_thread (%s) pid %d", qdf_get_current_comm(),
		qdf_get_current_pid());
	while (!shutdown) {
		/* This implements the execution model algorithm */
		status =
		    qdf_wait_queue_interruptible
				(rx_thread->wait_q,
				 qdf_atomic_test_bit(RX_REFILL_POST_EVENT,
						     &rx_thread->event_flag) ||
				 qdf_atomic_test_bit(RX_REFILL_SUSPEND_EVENT,
						     &rx_thread->event_flag));

		if (status == -ERESTARTSYS) {
			QDF_DEBUG_PANIC("wait_event_interruptible returned -ERESTARTSYS");
			break;
		}
		dp_rx_refill_thread_sub_loop(rx_thread, &shutdown);
		qdf_atomic_clear_bit(RX_REFILL_POST_EVENT, &rx_thread->event_flag);
	}

	/* If we get here the scheduler thread must exit */
	dp_info("exiting (%s) pid %d", qdf_get_current_comm(),
		qdf_get_current_pid());
	qdf_event_set(&rx_thread->shutdown_event);
	qdf_exit_thread(QDF_STATUS_SUCCESS);

	return 0;
}

/**
 * dp_rx_tm_thread_napi_poll() - dummy napi poll for rx_thread NAPI
 * @napi: pointer to DP rx_thread NAPI
 * @budget: NAPI BUDGET
 *
 * Return: 0 as it is not supposed to be polled at all as it is not scheduled.
 */
static int dp_rx_tm_thread_napi_poll(struct napi_struct *napi, int budget)
{
	QDF_DEBUG_PANIC("this napi_poll should not be polled as we don't schedule it");

	return 0;
}

/**
 * dp_rx_tm_thread_napi_init() - Initialize dummy rx_thread NAPI
 * @rx_thread: dp_rx_thread structure containing dummy napi and netdev
 *
 * Return: None
 */
static void dp_rx_tm_thread_napi_init(struct dp_rx_thread *rx_thread)
{
	/* Todo - optimize to use only one dummy netdev for all thread napis */
	init_dummy_netdev(&rx_thread->netdev);
	netif_napi_add(&rx_thread->netdev, &rx_thread->napi,
		       dp_rx_tm_thread_napi_poll, 64);
	napi_enable(&rx_thread->napi);
}

/**
 * dp_rx_tm_thread_napi_deinit() - De-initialize dummy rx_thread NAPI
 * @rx_thread: dp_rx_thread handle containing dummy napi and netdev
 *
 * Return: None
 */
static void dp_rx_tm_thread_napi_deinit(struct dp_rx_thread *rx_thread)
{
	netif_napi_del(&rx_thread->napi);
}

/*
 * dp_rx_tm_thread_init() - Initialize dp_rx_thread structure and thread
 *
 * @rx_thread: dp_rx_thread structure to be initialized
 * @id: id of the thread to be initialized
 *
 * Return: QDF_STATUS on success, QDF error code on failure
 */
static QDF_STATUS dp_rx_tm_thread_init(struct dp_rx_thread *rx_thread,
				       uint8_t id)
{
	char thread_name[15];
	QDF_STATUS qdf_status;

	qdf_mem_zero(thread_name, sizeof(thread_name));

	if (!rx_thread) {
		dp_err("rx_thread is null!");
		return QDF_STATUS_E_FAULT;
	}
	rx_thread->id = id;
	rx_thread->event_flag = 0;
	qdf_nbuf_queue_head_init(&rx_thread->nbuf_queue);
	qdf_event_create(&rx_thread->start_event);
	qdf_event_create(&rx_thread->suspend_event);
	qdf_event_create(&rx_thread->resume_event);
	qdf_event_create(&rx_thread->shutdown_event);
	qdf_event_create(&rx_thread->vdev_del_event);
	qdf_atomic_init(&rx_thread->gro_flush_ind);
	qdf_init_waitqueue_head(&rx_thread->wait_q);
	qdf_scnprintf(thread_name, sizeof(thread_name), "dp_rx_thread_%u", id);
	dp_info("%s %u", thread_name, id);

	if (cdp_cfg_get(dp_rx_tm_get_soc_handle(rx_thread->rtm_handle_cmn),
			cfg_dp_gro_enable))
		dp_rx_tm_thread_napi_init(rx_thread);

	rx_thread->task = qdf_create_thread(dp_rx_thread_loop,
					    rx_thread, thread_name);
	if (!rx_thread->task) {
		dp_err("could not create dp_rx_thread %d", id);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_wake_up_process(rx_thread->task);
	qdf_status = qdf_wait_single_event(&rx_thread->start_event, 0);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		dp_err("failed waiting for thread creation id %d", id);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/*
 * dp_rx_tm_thread_deinit() - De-Initialize dp_rx_thread structure and thread
 * @rx_thread: dp_rx_thread structure to be de-initialized
 * @id: id of the thread to be initialized
 *
 * Return: QDF_STATUS_SUCCESS
 */
static QDF_STATUS dp_rx_tm_thread_deinit(struct dp_rx_thread *rx_thread)
{
	qdf_event_destroy(&rx_thread->start_event);
	qdf_event_destroy(&rx_thread->suspend_event);
	qdf_event_destroy(&rx_thread->resume_event);
	qdf_event_destroy(&rx_thread->shutdown_event);
	qdf_event_destroy(&rx_thread->vdev_del_event);

	if (cdp_cfg_get(dp_rx_tm_get_soc_handle(rx_thread->rtm_handle_cmn),
			cfg_dp_gro_enable))
		dp_rx_tm_thread_napi_deinit(rx_thread);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_rx_refill_thread_init(struct dp_rx_refill_thread *refill_thread)
{
	char refill_thread_name[20] = {0};
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	qdf_scnprintf(refill_thread_name, sizeof(refill_thread_name),
		      "dp_refill_thread");
	dp_info("Initializing %s", refill_thread_name);

	refill_thread->state = DP_RX_REFILL_THREAD_INVALID;
	refill_thread->event_flag = 0;
	qdf_event_create(&refill_thread->start_event);
	qdf_event_create(&refill_thread->suspend_event);
	qdf_event_create(&refill_thread->resume_event);
	qdf_event_create(&refill_thread->shutdown_event);
	qdf_init_waitqueue_head(&refill_thread->wait_q);
	refill_thread->task = qdf_create_thread(dp_rx_refill_thread_loop,
						refill_thread,
						refill_thread_name);
	if (!refill_thread->task) {
		dp_err("could not create dp_rx_refill_thread");
		return QDF_STATUS_E_FAILURE;
	}
	qdf_wake_up_process(refill_thread->task);
	qdf_status = qdf_wait_single_event(&refill_thread->start_event,
					   0);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		dp_err("failed waiting for refill thread creation status: %d",
		       qdf_status);
		return QDF_STATUS_E_FAILURE;
	}

	refill_thread->state = DP_RX_REFILL_THREAD_RUNNING;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_rx_refill_thread_deinit(struct dp_rx_refill_thread *refill_thread)
{
	qdf_set_bit(RX_REFILL_SHUTDOWN_EVENT,
		    &refill_thread->event_flag);
	qdf_set_bit(RX_REFILL_POST_EVENT,
		    &refill_thread->event_flag);
	qdf_wake_up_interruptible(&refill_thread->wait_q);
	qdf_wait_single_event(&refill_thread->shutdown_event, 0);

	qdf_event_destroy(&refill_thread->start_event);
	qdf_event_destroy(&refill_thread->suspend_event);
	qdf_event_destroy(&refill_thread->resume_event);
	qdf_event_destroy(&refill_thread->shutdown_event);

	refill_thread->state = DP_RX_REFILL_THREAD_INVALID;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_rx_tm_init(struct dp_rx_tm_handle *rx_tm_hdl,
			 uint8_t num_dp_rx_threads)
{
	int i;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	if (num_dp_rx_threads > DP_MAX_RX_THREADS) {
		dp_err("unable to initialize %u number of threads. MAX %u",
		       num_dp_rx_threads, DP_MAX_RX_THREADS);
		return QDF_STATUS_E_INVAL;
	}

	rx_tm_hdl->num_dp_rx_threads = num_dp_rx_threads;
	rx_tm_hdl->state = DP_RX_THREADS_INVALID;

	dp_info("initializing %u threads", num_dp_rx_threads);

	/* allocate an array to contain the DP RX thread pointers */
	rx_tm_hdl->rx_thread = qdf_mem_malloc(num_dp_rx_threads *
					      sizeof(struct dp_rx_thread *));

	if (qdf_unlikely(!rx_tm_hdl->rx_thread)) {
		qdf_status = QDF_STATUS_E_NOMEM;
		goto ret;
	}

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		rx_tm_hdl->rx_thread[i] =
			(struct dp_rx_thread *)
			qdf_mem_malloc(sizeof(struct dp_rx_thread));
		if (qdf_unlikely(!rx_tm_hdl->rx_thread[i])) {
			QDF_ASSERT(0);
			qdf_status = QDF_STATUS_E_NOMEM;
			goto ret;
		}
		rx_tm_hdl->rx_thread[i]->rtm_handle_cmn =
				(struct dp_rx_tm_handle_cmn *)rx_tm_hdl;
		qdf_status =
			dp_rx_tm_thread_init(rx_tm_hdl->rx_thread[i], i);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			break;
	}
ret:
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		dp_rx_tm_deinit(rx_tm_hdl);
	else
		rx_tm_hdl->state = DP_RX_THREADS_RUNNING;

	return qdf_status;
}

/**
 * dp_rx_tm_resume() - suspend DP RX threads
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread
 *            infrastructure
 *
 * Return: Success/Failure
 */
QDF_STATUS dp_rx_tm_suspend(struct dp_rx_tm_handle *rx_tm_hdl)
{
	int i;
	QDF_STATUS qdf_status;
	struct dp_rx_thread *rx_thread;

	if (rx_tm_hdl->state == DP_RX_THREADS_SUSPENDED) {
		dp_info("already in suspend state! Ignoring.");
		return QDF_STATUS_E_INVAL;
	}

	rx_tm_hdl->state = DP_RX_THREADS_SUSPENDING;

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		if (!rx_tm_hdl->rx_thread[i])
			continue;
		qdf_event_reset(&rx_tm_hdl->rx_thread[i]->resume_event);
		qdf_event_reset(&rx_tm_hdl->rx_thread[i]->suspend_event);
		qdf_set_bit(RX_SUSPEND_EVENT,
			    &rx_tm_hdl->rx_thread[i]->event_flag);
		qdf_wake_up_interruptible(&rx_tm_hdl->rx_thread[i]->wait_q);
	}

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		rx_thread = rx_tm_hdl->rx_thread[i];
		if (!rx_thread)
			continue;
		dp_debug("thread %d", i);
		qdf_status = qdf_wait_single_event(&rx_thread->suspend_event,
						   DP_RX_THREAD_WAIT_TIMEOUT);
		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			dp_debug("thread:%d suspended", rx_thread->id);
		else
			goto suspend_fail;
	}
	rx_tm_hdl->state = DP_RX_THREADS_SUSPENDED;

	return QDF_STATUS_SUCCESS;

suspend_fail:
	dp_err("thread:%d %s(%d) while waiting for suspend",
	       rx_thread->id,
	       qdf_status == QDF_STATUS_E_TIMEOUT ? "timeout out" : "failed",
	       qdf_status);

	dp_rx_tm_resume(rx_tm_hdl);

	return qdf_status;
}

/**
 * dp_rx_refill_thread_suspend() - Suspend DP RX refill threads
 * @refill_thread: containing the overall refill thread infrastructure
 *
 * Return: Success/Failure
 */
QDF_STATUS
dp_rx_refill_thread_suspend(struct dp_rx_refill_thread *refill_thread)
{
	QDF_STATUS qdf_status;

	if (refill_thread->state == DP_RX_REFILL_THREAD_SUSPENDED) {
		dp_info("already in suspend state! Ignoring.");
		return QDF_STATUS_E_INVAL;
	}

	refill_thread->state = DP_RX_REFILL_THREAD_SUSPENDING;

	qdf_event_reset(&refill_thread->resume_event);
	qdf_event_reset(&refill_thread->suspend_event);
	qdf_set_bit(RX_REFILL_SUSPEND_EVENT,
		    &refill_thread->event_flag);
	qdf_wake_up_interruptible(&refill_thread->wait_q);

	qdf_status = qdf_wait_single_event(&refill_thread->suspend_event,
					   DP_RX_THREAD_WAIT_TIMEOUT);
	if (QDF_IS_STATUS_SUCCESS(qdf_status))
		dp_debug("Refill thread  suspended");
	else
		goto suspend_fail;

	refill_thread->state = DP_RX_REFILL_THREAD_SUSPENDED;
	return QDF_STATUS_SUCCESS;

suspend_fail:
	dp_err("Refill thread %s(%d) while waiting for suspend",
	       qdf_status == QDF_STATUS_E_TIMEOUT ? "timeout out" : "failed",
	       qdf_status);

	dp_rx_refill_thread_resume(refill_thread);

	return qdf_status;
}

/**
 * dp_rx_thread_flush_by_vdev_id() - flush rx packets by vdev_id in
				     a particular rx thread queue
 * @rx_thread - rx_thread pointer of the queue from which packets are
 *              to be flushed out
 * @vdev_id: vdev id for which packets are to be flushed
 * @wait_timeout: wait time value for rx thread to complete flush
 *
 * The function will flush the RX packets by vdev_id in a particular
 * RX thead queue. And will notify and wait the TX thread to flush the
 * packets in the NAPI RX GRO hash list
 *
 * Return: Success/Failure
 */
static inline
QDF_STATUS dp_rx_thread_flush_by_vdev_id(struct dp_rx_thread *rx_thread,
					 uint8_t vdev_id, int wait_timeout)
{
	qdf_nbuf_t nbuf_list, tmp_nbuf_list;
	uint32_t num_list_elements = 0;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	qdf_nbuf_queue_head_lock(&rx_thread->nbuf_queue);
	QDF_NBUF_QUEUE_WALK_SAFE(&rx_thread->nbuf_queue, nbuf_list,
				 tmp_nbuf_list) {
		if (QDF_NBUF_CB_RX_VDEV_ID(nbuf_list) == vdev_id) {
			qdf_nbuf_unlink_no_lock(nbuf_list,
						&rx_thread->nbuf_queue);
			dp_rx_thread_adjust_nbuf_list(nbuf_list);
			num_list_elements =
				QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(nbuf_list);
			rx_thread->stats.rx_flushed += num_list_elements;
			qdf_nbuf_list_free(nbuf_list);
		}
	}
	qdf_nbuf_queue_head_unlock(&rx_thread->nbuf_queue);

	qdf_event_reset(&rx_thread->vdev_del_event);
	qdf_set_bit(RX_VDEV_DEL_EVENT, &rx_thread->event_flag);
	qdf_wake_up_interruptible(&rx_thread->wait_q);

	qdf_status = qdf_wait_single_event(&rx_thread->vdev_del_event,
					    wait_timeout);

	if (QDF_IS_STATUS_SUCCESS(qdf_status))
		dp_debug("thread:%d napi gro flush successfully",
			 rx_thread->id);
	else if (qdf_status == QDF_STATUS_E_TIMEOUT) {
		dp_err("thread:%d timed out waiting for napi gro flush",
		       rx_thread->id);
		/*
		 * If timeout, then force flush here in case any rx packets
		 * belong to this vdev is still pending on stack queue,
		 * while net_vdev will be freed soon.
		 */
		dp_rx_thread_gro_flush(rx_thread,
				       DP_RX_GRO_NORMAL_FLUSH);
	} else
		dp_err("thread:%d failed while waiting for napi gro flush",
		       rx_thread->id);

	return qdf_status;
}

/**
 * dp_rx_tm_flush_by_vdev_id() - flush rx packets by vdev_id in all
				 rx thread queues
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread
 *             infrastructure
 * @vdev_id: vdev id for which packets are to be flushed
 *
 * Return: QDF_STATUS_SUCCESS
 */
#ifdef HAL_CONFIG_SLUB_DEBUG_ON
QDF_STATUS dp_rx_tm_flush_by_vdev_id(struct dp_rx_tm_handle *rx_tm_hdl,
				     uint8_t vdev_id)
{
	struct dp_rx_thread *rx_thread;
	QDF_STATUS qdf_status;
	int i;
	int wait_timeout = DP_RX_THREAD_WAIT_TIMEOUT;

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		rx_thread = rx_tm_hdl->rx_thread[i];
		if (!rx_thread)
			continue;

		dp_debug("thread %d", i);
		qdf_status = dp_rx_thread_flush_by_vdev_id(rx_thread, vdev_id,
							   wait_timeout);

		/* if one thread timeout happened, shrink timeout value
		 * to 1/4 of origional value
		 */
		if (qdf_status == QDF_STATUS_E_TIMEOUT)
			wait_timeout = DP_RX_THREAD_WAIT_TIMEOUT / 4;
	}

	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS dp_rx_tm_flush_by_vdev_id(struct dp_rx_tm_handle *rx_tm_hdl,
				     uint8_t vdev_id)
{
	struct dp_rx_thread *rx_thread;
	int i;

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		rx_thread = rx_tm_hdl->rx_thread[i];
		if (!rx_thread)
			continue;

		dp_debug("thread %d", i);
		dp_rx_thread_flush_by_vdev_id(rx_thread, vdev_id,
					      DP_RX_THREAD_WAIT_TIMEOUT);
	}

	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * dp_rx_tm_resume() - resume DP RX threads
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread
 *            infrastructure
 *
 * Return: QDF_STATUS_SUCCESS on resume success. QDF error otherwise.
 */
QDF_STATUS dp_rx_tm_resume(struct dp_rx_tm_handle *rx_tm_hdl)
{
	int i;

	if (rx_tm_hdl->state != DP_RX_THREADS_SUSPENDED &&
	    rx_tm_hdl->state != DP_RX_THREADS_SUSPENDING) {
		dp_info("resume callback received w/o suspend! Ignoring.");
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		if (!rx_tm_hdl->rx_thread[i])
			continue;
		dp_debug("calling thread %d to resume", i);

		/* postively reset event_flag for DP_RX_THREADS_SUSPENDING
		 * state
		 */
		qdf_clear_bit(RX_SUSPEND_EVENT,
			      &rx_tm_hdl->rx_thread[i]->event_flag);
		qdf_event_set(&rx_tm_hdl->rx_thread[i]->resume_event);
	}

	rx_tm_hdl->state = DP_RX_THREADS_RUNNING;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_refill_thread_resume() - Resume DP RX refill threads
 * @refill_thread: refill_thread containing the overall thread infrastructure
 *
 * Return: QDF_STATUS_SUCCESS on resume success. QDF error otherwise.
 */
QDF_STATUS dp_rx_refill_thread_resume(struct dp_rx_refill_thread *refill_thread)
{
	dp_debug("calling refill thread to resume");

	if (refill_thread->state != DP_RX_REFILL_THREAD_SUSPENDED &&
	    refill_thread->state != DP_RX_REFILL_THREAD_SUSPENDING) {
		dp_info("resume callback received in %d state ! Ignoring.",
			refill_thread->state);
		return QDF_STATUS_E_INVAL;
	}

	/* postively reset event_flag for DP_RX_REFILL_THREAD_SUSPENDING
	 * state
	 */
	qdf_clear_bit(RX_REFILL_SUSPEND_EVENT,
		      &refill_thread->event_flag);
	qdf_event_set(&refill_thread->resume_event);

	refill_thread->state = DP_RX_REFILL_THREAD_RUNNING;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_tm_shutdown() - shutdown all DP RX threads
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread infrastructure
 *
 * Return: QDF_STATUS_SUCCESS
 */
static QDF_STATUS dp_rx_tm_shutdown(struct dp_rx_tm_handle *rx_tm_hdl)
{
	int i;

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		if (!rx_tm_hdl->rx_thread[i])
			continue;
		qdf_set_bit(RX_SHUTDOWN_EVENT,
			    &rx_tm_hdl->rx_thread[i]->event_flag);
		qdf_set_bit(RX_POST_EVENT,
			    &rx_tm_hdl->rx_thread[i]->event_flag);
		qdf_wake_up_interruptible(&rx_tm_hdl->rx_thread[i]->wait_q);
	}


	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		if (!rx_tm_hdl->rx_thread[i])
			continue;
		dp_debug("waiting for shutdown of thread %d", i);
		qdf_wait_single_event(&rx_tm_hdl->rx_thread[i]->shutdown_event,
				      0);
	}
	rx_tm_hdl->state = DP_RX_THREADS_INVALID;
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_tm_deinit() - de-initialize RX thread infrastructure
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread
 *            infrastructure
 *
 * Return: QDF_STATUS_SUCCESS
 */
QDF_STATUS dp_rx_tm_deinit(struct dp_rx_tm_handle *rx_tm_hdl)
{
	int i = 0;
	if (!rx_tm_hdl->rx_thread) {
		dp_err("rx_tm_hdl->rx_thread not initialized!");
		return QDF_STATUS_SUCCESS;
	}

	dp_rx_tm_shutdown(rx_tm_hdl);

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		if (!rx_tm_hdl->rx_thread[i])
			continue;
		dp_rx_tm_thread_deinit(rx_tm_hdl->rx_thread[i]);
		qdf_mem_free(rx_tm_hdl->rx_thread[i]);
	}

	/* free the array of RX thread pointers*/
	qdf_mem_free(rx_tm_hdl->rx_thread);
	rx_tm_hdl->rx_thread = NULL;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_tm_select_thread() - select a DP RX thread for a nbuf
 * @rx_tm_hdl: dp_rx_tm_handle containing the overall thread
 *            infrastructure
 * @reo_ring_num: REO ring number corresponding to the thread
 *
 * The function relies on the presence of QDF_NBUF_CB_RX_CTX_ID passed to it
 * from the nbuf list. Depending on the RX_CTX (copy engine or reo
 * ring) on which the packet was received, the function selects
 * a corresponding rx_thread.
 *
 * Return: rx thread ID selected for the nbuf
 */
static uint8_t dp_rx_tm_select_thread(struct dp_rx_tm_handle *rx_tm_hdl,
				      uint8_t reo_ring_num)
{
	uint8_t selected_rx_thread;

	if (reo_ring_num >= rx_tm_hdl->num_dp_rx_threads) {
		dp_err_rl("unexpected ring number");
		QDF_BUG(0);
		return 0;
	}

	selected_rx_thread = reo_ring_num;
	dp_debug("selected thread %u", selected_rx_thread);
	return selected_rx_thread;
}

QDF_STATUS dp_rx_tm_enqueue_pkt(struct dp_rx_tm_handle *rx_tm_hdl,
				qdf_nbuf_t nbuf_list)
{
	uint8_t selected_thread_id;

	selected_thread_id =
		dp_rx_tm_select_thread(rx_tm_hdl,
				       QDF_NBUF_CB_RX_CTX_ID(nbuf_list));
	dp_rx_tm_thread_enqueue(rx_tm_hdl->rx_thread[selected_thread_id],
				nbuf_list);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
dp_rx_tm_gro_flush_ind(struct dp_rx_tm_handle *rx_tm_hdl, int rx_ctx_id,
		       enum dp_rx_gro_flush_code flush_code)
{
	uint8_t selected_thread_id;

	selected_thread_id = dp_rx_tm_select_thread(rx_tm_hdl, rx_ctx_id);
	dp_rx_tm_thread_gro_flush_ind(rx_tm_hdl->rx_thread[selected_thread_id],
				      flush_code);

	return QDF_STATUS_SUCCESS;
}

struct napi_struct *dp_rx_tm_get_napi_context(struct dp_rx_tm_handle *rx_tm_hdl,
					      uint8_t rx_ctx_id)
{
	if (rx_ctx_id >= rx_tm_hdl->num_dp_rx_threads) {
		dp_err_rl("unexpected rx_ctx_id %u", rx_ctx_id);
		QDF_BUG(0);
		return NULL;
	}

	return &rx_tm_hdl->rx_thread[rx_ctx_id]->napi;
}

QDF_STATUS dp_rx_tm_set_cpu_mask(struct dp_rx_tm_handle *rx_tm_hdl,
				 qdf_cpu_mask *new_mask)
{
	int i = 0;

	for (i = 0; i < rx_tm_hdl->num_dp_rx_threads; i++) {
		if (!rx_tm_hdl->rx_thread[i])
			continue;
		qdf_thread_set_cpus_allowed_mask(rx_tm_hdl->rx_thread[i]->task,
						 new_mask);
	}
	return QDF_STATUS_SUCCESS;
}
