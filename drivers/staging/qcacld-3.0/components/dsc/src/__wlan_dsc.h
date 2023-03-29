/*
 * Copyright (c) 2018-2019, 2021 The Linux Foundation. All rights reserved.
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
 * DOC: Driver State Management (DSC) APIs for *internal* use
 */

#ifndef ____WLAN_DSC_H
#define ____WLAN_DSC_H

#include "qdf_event.h"
#include "qdf_list.h"
#include "qdf_threads.h"
#include "qdf_timer.h"
#include "qdf_trace.h"
#include "qdf_types.h"
#include "wlan_dsc.h"

#define dsc_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_QDF, params)
#define dsc_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_QDF, params)
#ifdef WLAN_DSC_DEBUG
#define dsc_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_QDF, params)
#else
#define dsc_debug(params...) /* no-op */
#endif

#define dsc_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_QDF, params)
#define dsc_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_QDF, params)
#define dsc_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_QDF, params)

#define dsc_enter_exit dsc_debug
#define dsc_enter() dsc_enter_exit("enter")
#define dsc_enter_str(str) dsc_enter_exit("enter(\"%s\")", str)
#define dsc_exit() dsc_enter_exit("exit")
#define dsc_exit_status(status) dsc_enter_exit("exit(status:%u)", status)

static inline bool __dsc_assert(const bool cond, const char *cond_str,
				const char *func, const uint32_t line)
{
	if (cond)
		return true;

	QDF_DEBUG_PANIC_FL(func, line, "Failed assertion '%s'!", cond_str);

	return false;
}

#define dsc_assert(cond) __dsc_assert(cond, #cond, __func__, __LINE__)
#define dsc_assert_success(status) dsc_assert(QDF_IS_STATUS_SUCCESS(status))

#ifdef WLAN_DSC_DEBUG
#define DSC_OP_TIMEOUT_MS		(1 * 60 * 1000) /* 1 minute */
#define DSC_TRANS_TIMEOUT_MS		(1 * 60 * 1000) /* 1 minute */
#define DSC_TRANS_WAIT_TIMEOUT_MS	(2 * 60 * 1000) /* 2 minutes */

/**
 * struct dsc_op - list node for operation tracking information
 * @node: list node
 * @timeout_timer: a timer used to detect operation timeouts
 * @thread: the thread which started the operation
 * @func: name of the function the operation was started from
 */
struct dsc_op {
	qdf_list_node_t node;
	qdf_timer_t timeout_timer;
	qdf_thread_t *thread;
	const char *func;
};
#endif /* WLAN_DSC_DEBUG */

/**
 * struct dsc_ops - operations in flight tracking container
 * @list: list for tracking debug information
 * @count: count of current operations in flight
 * @event: event used to wait in *_wait_for_ops() APIs
 */
struct dsc_ops {
#ifdef WLAN_DSC_DEBUG
	qdf_list_t list;
#endif
	uint32_t count;
	qdf_event_t event;
};

/**
 * struct dsc_tran - representation of a pending transition
 * @abort: used to indicate if the transition stopped waiting due to an abort
 * @desc: unique description of the transition
 * @node: list node
 * @event: event used to wait in *_start_trans_wait() APIs
 * @timeout_timer: a timer used to detect transition wait timeouts
 * @thread: the thread which started the transition wait
 */
struct dsc_tran {
	bool abort;
	const char *desc;
	qdf_list_node_t node;
	qdf_event_t event;
#ifdef WLAN_DSC_DEBUG
	qdf_timer_t timeout_timer;
	qdf_thread_t *thread;
#endif
};

/**
 * struct dsc_trans - transition information container
 * @active_desc: unique description of the current transition in progress
 * @queue: queue of pending transitions
 * @timeout_timer: a timer used to detect transition timeouts
 * @thread: the thread which started the transition
 */
struct dsc_trans {
	const char *active_desc;
	qdf_list_t queue;
#ifdef WLAN_DSC_DEBUG
	qdf_timer_t timeout_timer;
	qdf_thread_t *thread;
#endif
};

/**
 * struct dsc_driver - concrete dsc driver context
 * @lock: lock under which all dsc APIs execute
 * @psocs: list of children psoc contexts
 * @trans: transition tracking container for this node
 * @ops: operations in flight tracking container for this node
 */
struct dsc_driver {
	struct qdf_spinlock lock;
	qdf_list_t psocs;
	struct dsc_trans trans;
	struct dsc_ops ops;
};

/**
 * struct dsc_psoc - concrete dsc psoc context
 * @node: list node for membership in @driver->psocs
 * @driver: parent driver context
 * @vdevs: list of children vdevs contexts
 * @trans: transition tracking container for this node
 * @ops: operations in flight tracking container for this node
 */
struct dsc_psoc {
	qdf_list_node_t node;
	struct dsc_driver *driver;
	qdf_list_t vdevs;
	struct dsc_trans trans;
	struct dsc_ops ops;
};

/**
 * struct dsc_vdev - concrete dsc vdev context
 * @node: list node for membership in @psoc->vdevs
 * @psoc: parent psoc context
 * @trans: transition tracking container for this node
 * @ops: operations in flight tracking container for this node
 * @nb_cmd_during_ssr: north bound command id
 */
struct dsc_vdev {
	qdf_list_node_t node;
	struct dsc_psoc *psoc;
	struct dsc_trans trans;
	struct dsc_ops ops;
	uint8_t nb_cmd_during_ssr;
};

#define dsc_for_each_driver_psoc(driver_ptr, psoc_cursor) \
	qdf_list_for_each(&(driver_ptr)->psocs, psoc_cursor, node)

#define dsc_for_each_psoc_vdev(psoc_ptr, vdev_cursor) \
	qdf_list_for_each(&(psoc_ptr)->vdevs, vdev_cursor, node)

/**
 * __dsc_lock() - grab the dsc driver lock
 * @driver: the driver to lock
 *
 * Return: None
 */
void __dsc_lock(struct dsc_driver *driver);

/**
 * __dsc_unlock() - release the dsc driver lock
 * @driver: the driver to unlock
 *
 * Return: None
 */
void __dsc_unlock(struct dsc_driver *driver);

/**
 * __dsc_ops_init() - initialize @ops
 * @ops: the ops container to initialize
 *
 * Return: None
 */
void __dsc_ops_init(struct dsc_ops *ops);

/**
 * __dsc_ops_deinit() - de-initialize @ops
 * @ops: the ops container to de-initialize
 *
 * Return: None
 */
void __dsc_ops_deinit(struct dsc_ops *ops);

/**
 * __dsc_ops_insert() - insert @func into the trakcing information in @ops
 * @ops: the ops container to insert into
 * @func: the debug information to insert
 *
 * Return: QDF_STATUS
 */
QDF_STATUS __dsc_ops_insert(struct dsc_ops *ops, const char *func);

/**
 * __dsc_ops_remove() - remove @func from the tracking information in @ops
 * @ops: the ops container to remove from
 * @func: the debug information to remove
 *
 * Return: None
 */
bool __dsc_ops_remove(struct dsc_ops *ops, const char *func);

/**
 * __dsc_trans_init() - initialize @trans
 * @trans: the trans container to initialize
 *
 * Return: None
 */
void __dsc_trans_init(struct dsc_trans *trans);

/**
 * __dsc_trans_deinit() - de-initialize @trans
 * @trans: the trans container to de-initialize
 *
 * Return: None
 */
void __dsc_trans_deinit(struct dsc_trans *trans);

/**
 * __dsc_trans_start() - set the active transition on @trans
 * @trans: the transition container used to track the new active transition
 * @desc: unique description of the transition being started
 *
 * Return: QDF_STATUS
 */
QDF_STATUS __dsc_trans_start(struct dsc_trans *trans, const char *desc);

/**
 * __dsc_trans_stop() - unset the active transition on @trans
 * @trans: the transition container currently tracking the active transition
 *
 * Return: None
 */
void __dsc_trans_stop(struct dsc_trans *trans);

/**
 * __dsc_trans_queue() - queue @tran at the back of @trans
 * @trans: the transitions container to enqueue to
 * @tran: the transition to enqueue
 * @desc: unique description of the transition being queued
 *
 * Return: QDF_STATUS
 */
QDF_STATUS __dsc_trans_queue(struct dsc_trans *trans, struct dsc_tran *tran,
			     const char *desc);

/**
 * __dsc_tran_wait() - block until @tran completes
 * @tran: the transition to wait on
 *
 * Return: QDF_STATUS
 */
QDF_STATUS __dsc_tran_wait(struct dsc_tran *tran);

/**
 * __dsc_trans_abort() - abort the next queued transition from @trans
 * @trans: the transitions container to abort from
 *
 * Return: true if a transition was aborted, false if @trans is empty
 */
bool __dsc_trans_abort(struct dsc_trans *trans);

/**
 * __dsc_trans_trigger() - trigger the next queued trans in @trans
 * @trans: the transitions container to trigger from
 *
 * Return: true if a transition was triggered
 */
bool __dsc_trans_trigger(struct dsc_trans *trans);

/**
 * __dsc_trans_active() - check if a transition is active in @trans
 * @trans: the transitions container to check
 *
 * Return: true if @trans has an active transition
 */
bool __dsc_trans_active(struct dsc_trans *trans);

/**
 * __dsc_trans_queued() - check if a transition is queued in @trans
 * @trans: the transitions container to check
 *
 * Return: true if @trans has a queued transition
 */
bool __dsc_trans_queued(struct dsc_trans *trans);

/**
 * __dsc_trans_active_or_queued() - check if a transition is active or queued
 *	in @trans
 * @trans: the transitions container to check
 *
 * Return: true if @trans has an active or queued transition
 */
bool __dsc_trans_active_or_queued(struct dsc_trans *trans);

/**
 * __dsc_driver_trans_trigger_checked() - trigger any next pending driver
 *	transition, only after passing the "can trans" check
 *
 * Return: true if the trigger was "handled." This indicates down-tree nodes
 * should _not_ attempt to trigger a new transition.
 */
bool __dsc_driver_trans_trigger_checked(struct dsc_driver *driver);

/**
 * __dsc_psoc_trans_trigger_checked() - trigger any next pending psoc
 *	transition, only after passing the "can trans" check
 *
 * Return: true if the trigger was "handled." This indicates down-tree nodes
 * should _not_ attempt to trigger a new transition.
 */
bool __dsc_psoc_trans_trigger_checked(struct dsc_psoc *psoc);

#endif /* ____WLAN_DSC_H */
