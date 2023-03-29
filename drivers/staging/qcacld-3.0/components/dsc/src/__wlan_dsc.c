/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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

#include "qdf_list.h"
#include "qdf_mem.h"
#include "qdf_status.h"
#include "qdf_str.h"
#include "qdf_threads.h"
#include "qdf_timer.h"
#include "__wlan_dsc.h"
#include "cds_api.h"

#ifdef WLAN_DSC_DEBUG
static void __dsc_dbg_op_timeout(void *opaque_op)
{
	struct dsc_op *op = opaque_op;

	qdf_print_thread_trace(op->thread);
	QDF_DEBUG_PANIC("Operation '%s' exceeded %ums",
			op->func, DSC_OP_TIMEOUT_MS);
}

/**
 * __dsc_dbg_ops_init() - initialize debug ops data structures
 * @ops: the ops container to initialize
 *
 * Return: None
 */
static inline void __dsc_dbg_ops_init(struct dsc_ops *ops)
{
	qdf_list_create(&ops->list, 0);
}

/**
 * __dsc_dbg_ops_deinit() - de-initialize debug ops data structures
 * @ops: the ops container to de-initialize
 *
 * Return: None
 */
static inline void __dsc_dbg_ops_deinit(struct dsc_ops *ops)
{
	qdf_list_destroy(&ops->list);
}

/**
 * __dsc_dbg_ops_insert() - insert @func into the debug information in @ops
 * @ops: the ops container to insert into
 * @func: the debug information to insert
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS __dsc_dbg_ops_insert(struct dsc_ops *ops, const char *func)
{
	QDF_STATUS status;
	struct dsc_op *op;

	op = qdf_mem_malloc(sizeof(*op));
	if (!op)
		return QDF_STATUS_E_NOMEM;

	op->thread = qdf_get_current_task();
	status = qdf_timer_init(NULL, &op->timeout_timer, __dsc_dbg_op_timeout,
				op, QDF_TIMER_TYPE_SW);
	if (QDF_IS_STATUS_ERROR(status))
		goto free_op;

	op->func = func;

	qdf_timer_start(&op->timeout_timer, DSC_OP_TIMEOUT_MS);
	qdf_list_insert_back(&ops->list, &op->node);

	return QDF_STATUS_SUCCESS;

free_op:
	qdf_mem_free(op);

	return status;
}

/**
 * __dsc_dbg_ops_remove() - remove @func from the debug information in @ops
 * @ops: the ops container to remove from
 * @func: the debug information to remove
 *
 * Return: None
 */
static void __dsc_dbg_ops_remove(struct dsc_ops *ops, const char *func)
{
	struct dsc_op *op;

	/* Global pending op depth is usually <=3. Use linear search for now */
	qdf_list_for_each(&ops->list, op, node) {
		if (!qdf_str_eq(op->func, func))
			continue;

		/* this is safe because we cease iteration */
		qdf_list_remove_node(&ops->list, &op->node);

		qdf_timer_stop(&op->timeout_timer);
		qdf_timer_free(&op->timeout_timer);
		qdf_mem_free(op);

		return;
	}

	QDF_DEBUG_PANIC("Driver op '%s' is not pending", func);
}
#else
static inline void __dsc_dbg_ops_init(struct dsc_ops *ops) { }

static inline void __dsc_dbg_ops_deinit(struct dsc_ops *ops) { }

static inline QDF_STATUS
__dsc_dbg_ops_insert(struct dsc_ops *ops, const char *func)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
__dsc_dbg_ops_remove(struct dsc_ops *ops, const char *func) { }
#endif /* WLAN_DSC_DEBUG */

void __dsc_ops_init(struct dsc_ops *ops)
{
	ops->count = 0;
	qdf_event_create(&ops->event);
	__dsc_dbg_ops_init(ops);
}

void __dsc_ops_deinit(struct dsc_ops *ops)
{
	/* assert no ops in flight */
	dsc_assert(!ops->count);

	__dsc_dbg_ops_deinit(ops);
	qdf_event_destroy(&ops->event);
}

QDF_STATUS __dsc_ops_insert(struct dsc_ops *ops, const char *func)
{
	QDF_STATUS status;

	status = __dsc_dbg_ops_insert(ops, func);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	ops->count++;

	return QDF_STATUS_SUCCESS;
}

bool __dsc_ops_remove(struct dsc_ops *ops, const char *func)
{
	dsc_assert(ops->count);
	ops->count--;

	__dsc_dbg_ops_remove(ops, func);

	return ops->count == 0;
}

#ifdef WLAN_DSC_DEBUG
static void __dsc_dbg_trans_timeout(void *opaque_trans)
{
	struct dsc_trans *trans = opaque_trans;

	qdf_print_thread_trace(trans->thread);

	if (cds_is_fw_down())
		dsc_err("fw is down avoid panic");
	else
		QDF_DEBUG_PANIC("Transition '%s' exceeded %ums",
				trans->active_desc, DSC_TRANS_TIMEOUT_MS);
}

/**
 * __dsc_dbg_trans_timeout_start() - start a timeout timer for @trans
 * @trans: the active transition to start a timeout timer for
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS __dsc_dbg_trans_timeout_start(struct dsc_trans *trans)
{
	QDF_STATUS status;

	trans->thread = qdf_get_current_task();
	status = qdf_timer_init(NULL, &trans->timeout_timer,
				__dsc_dbg_trans_timeout, trans,
				QDF_TIMER_TYPE_SW);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	qdf_timer_start(&trans->timeout_timer, DSC_TRANS_TIMEOUT_MS);

	return QDF_STATUS_SUCCESS;
}

/**
 * __dsc_dbg_trans_timeout_stop() - stop the timeout timer for @trans
 * @trans: the active transition to stop the timeout timer for
 *
 * Return: None
 */
static void __dsc_dbg_trans_timeout_stop(struct dsc_trans *trans)
{
	qdf_timer_stop(&trans->timeout_timer);
	qdf_timer_free(&trans->timeout_timer);
}

static void __dsc_dbg_tran_wait_timeout(void *opaque_tran)
{
	struct dsc_tran *tran = opaque_tran;

	qdf_print_thread_trace(tran->thread);
	QDF_DEBUG_PANIC("Transition '%s' waited more than %ums",
			tran->desc, DSC_TRANS_WAIT_TIMEOUT_MS);
}

/**
 * __dsc_dbg_tran_wait_timeout_start() - start a timeout timer for @tran
 * @tran: the pending transition to start a timeout timer for
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS __dsc_dbg_tran_wait_timeout_start(struct dsc_tran *tran)
{
	QDF_STATUS status;

	tran->thread = qdf_get_current_task();
	status = qdf_timer_init(NULL, &tran->timeout_timer,
				__dsc_dbg_tran_wait_timeout, tran,
				QDF_TIMER_TYPE_SW);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	qdf_timer_start(&tran->timeout_timer, DSC_TRANS_WAIT_TIMEOUT_MS);

	return QDF_STATUS_SUCCESS;
}

/**
 * __dsc_dbg_tran_wait_timeout_stop() - stop the timeout timer for @tran
 * @tran: the pending transition to stop the timeout timer for
 *
 * Return: None
 */
static void __dsc_dbg_tran_wait_timeout_stop(struct dsc_tran *tran)
{
	qdf_timer_stop(&tran->timeout_timer);
	qdf_timer_free(&tran->timeout_timer);
}
#else
static inline QDF_STATUS __dsc_dbg_trans_timeout_start(struct dsc_trans *trans)
{
	return QDF_STATUS_SUCCESS;
}

static inline void __dsc_dbg_trans_timeout_stop(struct dsc_trans *trans) { }

static inline QDF_STATUS
__dsc_dbg_tran_wait_timeout_start(struct dsc_tran *tran)
{
	return QDF_STATUS_SUCCESS;
}

static inline void __dsc_dbg_tran_wait_timeout_stop(struct dsc_tran *tran) { }
#endif /* WLAN_DSC_DEBUG */

void __dsc_trans_init(struct dsc_trans *trans)
{
	trans->active_desc = NULL;
	qdf_list_create(&trans->queue, 0);
}

void __dsc_trans_deinit(struct dsc_trans *trans)
{
	qdf_list_destroy(&trans->queue);
	trans->active_desc = NULL;
}

QDF_STATUS __dsc_trans_start(struct dsc_trans *trans, const char *desc)
{
	QDF_STATUS status;

	status = __dsc_dbg_trans_timeout_start(trans);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	dsc_assert(!trans->active_desc);
	trans->active_desc = desc;

	return QDF_STATUS_SUCCESS;
}

void __dsc_trans_stop(struct dsc_trans *trans)
{
	dsc_assert(trans->active_desc);
	trans->active_desc = NULL;
	__dsc_dbg_trans_timeout_stop(trans);
}

QDF_STATUS __dsc_trans_queue(struct dsc_trans *trans, struct dsc_tran *tran,
			     const char *desc)
{
	QDF_STATUS status;

	tran->abort = false;
	tran->desc = desc;
	qdf_event_create(&tran->event);

	status = __dsc_dbg_tran_wait_timeout_start(tran);
	if (QDF_IS_STATUS_ERROR(status))
		goto event_destroy;

	qdf_list_insert_back(&trans->queue, &tran->node);

	return QDF_STATUS_SUCCESS;

event_destroy:
	qdf_event_destroy(&tran->event);

	return status;
}

/**
 * __dsc_trans_dequeue() - dequeue the next queued transition from @trans
 * @trans: the transactions container to dequeue from
 *
 * Return: the dequeued transition, or NULL if @trans is empty
 */
static struct dsc_tran *__dsc_trans_dequeue(struct dsc_trans *trans)
{
	QDF_STATUS status;
	qdf_list_node_t *node;
	struct dsc_tran *tran;

	status = qdf_list_remove_front(&trans->queue, &node);
	if (QDF_IS_STATUS_ERROR(status))
		return NULL;

	tran = qdf_container_of(node, struct dsc_tran, node);
	__dsc_dbg_tran_wait_timeout_stop(tran);

	return tran;
}

bool __dsc_trans_abort(struct dsc_trans *trans)
{
	struct dsc_tran *tran;

	tran = __dsc_trans_dequeue(trans);
	if (!tran)
		return false;

	tran->abort = true;
	qdf_event_set(&tran->event);

	return true;
}

bool __dsc_trans_trigger(struct dsc_trans *trans)
{
	struct dsc_tran *tran;

	tran = __dsc_trans_dequeue(trans);
	if (!tran)
		return false;

	__dsc_trans_start(trans, tran->desc);
	qdf_event_set(&tran->event);

	return true;
}

bool __dsc_trans_active(struct dsc_trans *trans)
{
	return !!trans->active_desc;
}

bool __dsc_trans_queued(struct dsc_trans *trans)
{
	return !qdf_list_empty(&trans->queue);
}

bool __dsc_trans_active_or_queued(struct dsc_trans *trans)
{
	return __dsc_trans_active(trans) || __dsc_trans_queued(trans);
}

QDF_STATUS __dsc_tran_wait(struct dsc_tran *tran)
{
	QDF_STATUS status;

	status = qdf_wait_single_event(&tran->event, 0);
	qdf_event_destroy(&tran->event);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (tran->abort)
		return QDF_STATUS_E_ABORTED;

	return QDF_STATUS_SUCCESS;
}

