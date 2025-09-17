// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hgsl.h"
#include "hgsl_dispatch.h"
#include "hgsl_trace.h"

#define DRAWQUEUE_NEXT(_i, _s) (((_i) + 1) % (_s))
/*
 * Number of commands that can be queued in a context before it sleeps
 *
 * Our code that "puts back" a command from the context is much cleaner
 * if we are sure that there will always be enough room in the ringbuffer
 * so restrict the size of the context queue to CONTEXT_DRAWQUEUE_SIZE - 1
 */
static u32 _context_drawqueue_size = CONTEXT_DRAWQUEUE_SIZE - 1;

/* Number of milliseconds to wait for the context queue to clear */
static u32 _context_queue_wait = 10000;

/* Use a kmem cache to speed up allocations for inflight command objects */
static struct kmem_cache *obj_cache;

/**
 * hgsl_dispatch_requeue_drawobj() - Put a draw objet back on the context
 * queue
 * @ctxt: Pointer to the hgsl context
 * @drawobj: Pointer to the KGSL draw object to requeue
 *
 * Failure to submit a drawobj to the ringbuffer isn't the fault of the drawobj
 * being submitted so if a failure happens, push it back on the head of the
 * context queue to be reconsidered again.
 */
static inline void hgsl_dispatch_requeue_drawobj(
		struct hgsl_context *ctxt,
		struct hgsl_drawobj *drawobj)
{
	u32 prev;

	spin_lock(&ctxt->drawq_lock);

	prev = ctxt->drawq_head == 0 ? (CONTEXT_DRAWQUEUE_SIZE - 1) :
		(ctxt->drawq_head - 1);

	/*
	 * The maximum queue size always needs to be one less than the size of
	 * the draw queue so there is "room" to put the drawobj back in.
	 */

	WARN_ON(prev == ctxt->drawq_tail);

	ctxt->drawq[prev] = drawobj;
	ctxt->queued++;

	/* Reset the command queue head to reflect the newly requeued change */
	ctxt->drawq_head = prev;
	spin_unlock(&ctxt->drawq_lock);
}

static bool is_cmdobj(struct hgsl_drawobj *drawobj)
{
	return (drawobj->type & CMDOBJ_TYPE);
}

static bool _check_context_queue(struct hgsl_context *ctxt, u32 count)
{
	bool ret;

	spin_lock(&ctxt->drawq_lock);

	/* Wake up if there is room in the context*/
	ret = ((ctxt->queued + count) < _context_drawqueue_size) ? 1 : 0;

	spin_unlock(&ctxt->drawq_lock);

	return ret;
}

static void _pop_drawobj(struct hgsl_context *ctxt)
{
	ctxt->drawq_head = DRAWQUEUE_NEXT(ctxt->drawq_head,
		CONTEXT_DRAWQUEUE_SIZE);
	ctxt->queued--;
}

static int _retire_syncobj(struct qcom_hgsl *hgsl,
	struct hgsl_drawobj_sync *syncobj, struct hgsl_context *ctxt)
{

	if (!hgsl_drawobj_events_pending(syncobj)) {
		trace_syncobj_retired(syncobj);
		_pop_drawobj(ctxt);
		hgsl_drawobj_destroy(DRAWOBJ(syncobj));
		return 0;
	}

	/*
	 * If we got here, there are pending events for sync object.
	 * Start the canary timer if it hasnt been started already.
	 */
	if (!syncobj->timeout_jiffies) {
		syncobj->timeout_jiffies = jiffies + msecs_to_jiffies(5000);
			mod_timer(&syncobj->timer, syncobj->timeout_jiffies);
	}

	return -EAGAIN;
}

static bool _marker_expired(struct hgsl_drawobj_cmd *markerobj)
{
	struct hgsl_drawobj *drawobj = DRAWOBJ(markerobj);
	bool expired;

	if (!(drawobj->flags & HGSL_DRAWOBJ_MARKER) ||
		!drawobj->context->shadow_ts)
		return false;

	if (!hgsl_context_get(drawobj->context))
		return true;

	expired = hgsl_check_timestamp(drawobj->priv, drawobj->context,
		markerobj->marker_timestamp);

	hgsl_put_context(drawobj->context);

	return expired;
}

/* Only retire the timestamp. The drawobj will be destroyed later */
static void _retire_timestamp_only(struct hgsl_drawobj *drawobj)
{
	struct qcom_hgsl *hgsl = drawobj->priv->dev;
	struct hgsl_context *ctxt = drawobj->context;

	set_context_shadow_ts(ctxt, GSL_TIMESTAMP_CONSUMED,
		drawobj->timestamp);
	set_context_shadow_ts(ctxt, GSL_TIMESTAMP_RETIRED,
		drawobj->timestamp);

	/* Retire pending GPU events for the object */
	hgsl_process_event_group(hgsl, &ctxt->event_group);
}

static void _retire_timestamp(struct hgsl_drawobj *drawobj)
{
	_retire_timestamp_only(drawobj);

	hgsl_drawobj_destroy(drawobj);
}

static int _retire_markerobj(struct qcom_hgsl *hgsl,
	struct hgsl_drawobj_cmd *cmdobj,
	struct hgsl_context *ctxt)
{
	if (_marker_expired(cmdobj)) {
		set_bit(CMDOBJ_MARKER_EXPIRED, &cmdobj->priv);
		_pop_drawobj(ctxt);
		_retire_timestamp(DRAWOBJ(cmdobj));
		return 0;
	}

	/*
	 * If the marker isn't expired but the SKIP bit
	 * is set then there are real commands following
	 * this one in the queue. This means that we
	 * need to dispatch the command so that we can
	 * keep the timestamp accounting correct. If
	 * skip isn't set then we block this queue
	 * until the dependent timestamp expires
	 */

	return test_bit(CMDOBJ_SKIP, &cmdobj->priv) ? 1 : -EAGAIN;
}

static int _retire_timelineobj(struct hgsl_drawobj *drawobj,
		struct hgsl_context *ctxt)
{
	_pop_drawobj(ctxt);
	hgsl_drawobj_destroy(drawobj);
	return 0;
}

/**
 * sendcmd() - Send a drawobj to the GPU hardware
 * @dispatcher: Pointer to the adreno dispatcher struct
 * @drawobj: Pointer to the KGSL drawobj being sent
 *
 * Send a KGSL drawobj to the GPU hardware
 */
static int _sendcmd(struct qcom_hgsl *hgsl,
	struct hgsl_drawobj *drawobj)
{
	struct hgsl_context *ctxt = drawobj->context;
	struct hgsl_dispatch_context *dispatch = ctxt->dispatch;
	int ret;
	struct cmd_obj *obj;

	obj = kmem_cache_alloc(obj_cache, GFP_KERNEL);
	if (!obj)
		return -ENOMEM;

	ret = hgsl_issue_drawobj(hgsl, drawobj);
	if (ret) {
		kmem_cache_free(obj_cache, obj);
		return ret;
	}

	if (is_cmdobj(drawobj)) {
		struct hgsl_drawobj_cmd *cmdobj = CMDOBJ(drawobj);

		/* If this MARKER object is already retired, we can destroy it here */
		if ((test_bit(CMDOBJ_MARKER_EXPIRED, &cmdobj->priv))) {
			kmem_cache_free(obj_cache, obj);
			hgsl_drawobj_destroy(drawobj);
			return 0;
		}
	}

	obj->drawobj = drawobj;
	list_add_tail(&obj->node, &dispatch->drawobj_list);
	trace_drawobj_submitted(drawobj);
	return 0;
}

/*
 * Retires all expired marker and sync objs from the context
 * queue and returns one of the below
 * a) next drawobj that needs to be sent to ringbuffer
 * b) -EAGAIN for syncobj with syncpoints pending.
 * c) -EAGAIN for markerobj whose marker timestamp has not expired yet.
 * c) NULL for no commands remaining in drawq.
 */
static struct hgsl_drawobj *_process_drawqueue_get_next_drawobj(
	struct qcom_hgsl *hgsl, struct hgsl_context *ctxt)
{
	struct hgsl_drawobj *drawobj;
	u32 i = ctxt->drawq_head;
	struct hgsl_drawobj_cmd *cmdobj;
	int ret = 0;

	if (ctxt->drawq_head == ctxt->drawq_tail)
		return NULL;

	for (i = ctxt->drawq_head; i != ctxt->drawq_tail;
			i = DRAWQUEUE_NEXT(i, CONTEXT_DRAWQUEUE_SIZE)) {

		drawobj = ctxt->drawq[i];

		if (!drawobj)
			return NULL;

		switch (drawobj->type) {
		case CMDOBJ_TYPE:
			cmdobj = CMDOBJ(drawobj);
			return drawobj;
		case SYNCOBJ_TYPE:
			ret = _retire_syncobj(hgsl, SYNCOBJ(drawobj), ctxt);
			break;
		case MARKEROBJ_TYPE:
			ret = _retire_markerobj(hgsl, CMDOBJ(drawobj), ctxt);
			/* Special case where marker needs to be sent to GPU */
			if (ret == 1)
				return drawobj;
			break;
		case BINDOBJ_TYPE:
			/* TODO: */
			LOGW("BINDOBJ is not supported");
			break;
		case TIMELINEOBJ_TYPE:
			ret = _retire_timelineobj(drawobj, ctxt);
			break;
		default:
			ret = -EINVAL;
			break;
		}

		if (ret)
			return ERR_PTR(ret);
	}

	return NULL;
}

/**
 * hgsl_dispatch_sendcmds() - Send commands from a context to the GPU
 * @hgsl: Pointer to the hgsl device
 * @ctxt: Pointer to the adreno context to dispatch commands from
 *
 * Dequeue and send a burst of commands from the specified context to the GPU
 * Returns postive if the context needs to be put back on the pending queue
 * 0 if the context is empty or detached and negative on error
 */
static int hgsl_dispatch_sendcmds(struct qcom_hgsl *hgsl,
		struct hgsl_context *ctxt)
{
	int count = 0;
	int ret = 0;

	while (1) {
		struct hgsl_drawobj *drawobj;

		spin_lock(&ctxt->drawq_lock);
		drawobj = _process_drawqueue_get_next_drawobj(hgsl, ctxt);

		/*
		 * _process_drawqueue_get_next_drawobj returns -EAGAIN if the current
		 * drawobj has pending sync points so no more to do here.
		 * When the sync points are satisfied then the context will get
		 * reqeueued
		 */
		if (IS_ERR_OR_NULL(drawobj)) {
			if (IS_ERR(drawobj))
				ret = PTR_ERR(drawobj);
			spin_unlock(&ctxt->drawq_lock);
			break;
		}
		_pop_drawobj(ctxt);
		spin_unlock(&ctxt->drawq_lock);

		ret = _sendcmd(hgsl, drawobj);

		/* On error from _sendcmd() try to requeue the cmdobj. */
		if (ret) {
			/*
			 * TODO: -ENOENT which means that the context has been
			 * destroyed and there will be no more deliveries from
			 * here, then destroy the cmdobj.
			 */
			if (ret == -ENOENT)
				hgsl_drawobj_destroy(drawobj);
			else {
				/*
				 * If we couldn't put it on dispatch queue
				 * then return it to the context queue
				 */
				hgsl_dispatch_requeue_drawobj(
					ctxt, drawobj);
			}
			break;
		}
		count++;
	}

	/*
	 * Wake up any snoozing threads if we have consumed any real commands
	 * or marker commands and we have room in the context queue.
	 */
	if (_check_context_queue(ctxt, 0))
		wake_up_all(&ctxt->drawq_wq);

	if (!ret)
		ret = count;

	/* Return error or the number of commands queued */
	return ret;
}

static int _check_for_room_in_context_drawq(
	struct hgsl_context *ctxt, u32 count)
{
	int ret = 0;

	/*
	 * There is always a possibility that dispatcher may end up pushing
	 * the last popped draw object back to the context drawqueue. Hence,
	 * we can only queue up to _context_drawqueue_size - 1 here to make
	 * sure we never let drawqueue->queued exceed _context_drawqueue_size.
	 */
	if ((ctxt->queued + count) > (_context_drawqueue_size - 1)) {
		trace_ctxt_sleep(ctxt);
		spin_unlock(&ctxt->drawq_lock);

		ret = wait_event_interruptible_timeout(ctxt->drawq_wq,
			_check_context_queue(ctxt, count),
			msecs_to_jiffies(_context_queue_wait));

		spin_lock(&ctxt->drawq_lock);
		trace_ctxt_wake(ctxt);
	}

	return ret;
}

static void _queue_drawobj(struct hgsl_context *ctxt,
	struct hgsl_drawobj *drawobj)
{
	/* Put the command into the queue */
	ctxt->drawq[ctxt->drawq_tail] = drawobj;
	ctxt->drawq_tail =
		(ctxt->drawq_tail + 1) % CONTEXT_DRAWQUEUE_SIZE;
	ctxt->queued++;

	trace_drawobj_queued(drawobj, ctxt->queued);
}

static int _queue_cmdobj(struct hgsl_context *ctxt,
	struct hgsl_drawobj_cmd *cmdobj, uint32_t *timestamp,
	u32 user_ts)
{
	struct hgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	u32 j;
	int ret;

	ret = hgsl_db_next_timestamp(ctxt, &user_ts);
	if (ret)
		return ret;

	*timestamp = user_ts;
	drawobj->timestamp = *timestamp;

	/*
	 * If this is a real command then we need to force any markers
	 * queued before it to dispatch to keep time linear - set the
	 * skip bit so the commands get NOPed.
	 */
	j = ctxt->drawq_head;

	while (j != ctxt->drawq_tail) {
		if (ctxt->drawq[j]->type == MARKEROBJ_TYPE) {
			struct hgsl_drawobj_cmd *markerobj =
				CMDOBJ(ctxt->drawq[j]);

			set_bit(CMDOBJ_SKIP, &markerobj->priv);
		}

		j = DRAWQUEUE_NEXT(j, CONTEXT_DRAWQUEUE_SIZE);
	}

	ctxt->queued_ts = *timestamp;

	_queue_drawobj(ctxt, drawobj);
	return 0;
}

static void _queue_syncobj(struct hgsl_context *ctxt,
	struct hgsl_drawobj_sync *syncobj, uint32_t *timestamp)
{
	struct hgsl_drawobj *drawobj = DRAWOBJ(syncobj);

	trace_syncobj_queued(syncobj);

	*timestamp = 0;
	drawobj->timestamp = 0;

	_queue_drawobj(ctxt, drawobj);
}

static int _queue_markerobj(struct hgsl_context *ctxt,
	struct hgsl_drawobj_cmd *markerobj, u32 *timestamp,
	u32 user_ts)
{
	struct hgsl_drawobj *drawobj = DRAWOBJ(markerobj);
	int ret;

	ret = hgsl_db_next_timestamp(ctxt, &user_ts);
	if (ret)
		return ret;

	*timestamp = user_ts;
	drawobj->timestamp = *timestamp;

	if (ctxt->shadow_ts) {
		/*
		 * See if we can fastpath this thing - if nothing is queued
		 * and nothing is inflight retire without bothering the GPU
		 */
		if (!ctxt->queued && hgsl_check_timestamp(drawobj->priv,
				drawobj->context, ctxt->queued_ts)) {
			ctxt->queued_ts = drawobj->timestamp;
			_retire_timestamp(drawobj);
			return 1;
		}
	} else {
		/*
		 * Currently, there is no way to set retire ts if it's hyp case,
		 * set SKIP flag to issue this marker obj directly, this can help
		 * us to bookkeeping timestamp correctly.
		 */
		set_bit(CMDOBJ_SKIP, &markerobj->priv);
	}

	/*
	 * Remember the last queued timestamp - the marker will block
	 * until that timestamp is expired (unless another command
	 * comes along and forces the marker to execute)
	 */
	markerobj->marker_timestamp = ctxt->queued_ts;
	ctxt->queued_ts = drawobj->timestamp;

	_queue_drawobj(ctxt, drawobj);
	return 0;
}

static void _queue_timelineobj(struct hgsl_context *ctxt,
		struct hgsl_drawobj *drawobj)
{
	/*
	 * This drawobj is not submitted to the GPU so use a timestamp of 0.
	 * Update the timestamp through a subsequent marker to keep userspace
	 * happy.
	 */
	drawobj->timestamp = 0;

	_queue_drawobj(ctxt, drawobj);
}

static void _retire_drawobjs(struct hgsl_context *ctxt)
{
	struct hgsl_dispatch_context *dispatch = ctxt->dispatch;
	struct cmd_obj *obj, *tmp;

	list_for_each_entry_safe(obj, tmp, &dispatch->drawobj_list,
			node) {
		struct hgsl_drawobj *drawobj = obj->drawobj;

		if (!hgsl_check_timestamp(drawobj->priv, drawobj->context,
				drawobj->timestamp))
			continue;

		hgsl_drawobj_destroy(drawobj);
		list_del_init(&obj->node);
		kmem_cache_free(obj_cache, obj);
	}
}

void hgsl_dispatch_ctxt_schedule(struct hgsl_context *ctxt)
{
	struct hgsl_dispatch_context *dispatch = ctxt->dispatch;

	kthread_queue_work(dispatch->worker, &dispatch->work);
}

void hgsl_dispatch_ctxt_issuecmds(
	struct hgsl_context *ctxt)
{
	struct hgsl_dispatch_context *dispatch = ctxt->dispatch;
	struct qcom_hgsl *hgsl = dispatch->hgsl;
	int i, count = atomic_xchg(&dispatch->count, 0);

	for (i = 0; i < count; i++) {
		int ret;

		if (i > 0) {
			hgsl_put_context(ctxt);
			continue;
		}

		ret = hgsl_dispatch_sendcmds(hgsl, ctxt);

		/*
		 * If the context had nothing queued or the context has been
		 * destroyed then drop the job
		 */
		if (!ret || ret == -ENOENT) {
			hgsl_put_context(ctxt);
			continue;
		}

		/*
		 * If the dispatch queue is full then requeue the job.
		 * Otherwise the context either successfully submmitted
		 * to the GPU or another error happened and it should
		 * re-scheduled.
		 */
		atomic_inc(&dispatch->count);
		hgsl_dispatch_ctxt_schedule(ctxt);
	}
}

static void hgsl_dispatch_ctxt_work(struct kthread_work *work)
{
	struct hgsl_dispatch_context *dispatch =
			container_of(work, struct hgsl_dispatch_context, work);
	struct hgsl_context *ctxt = dispatch->ctxt;
	struct qcom_hgsl *hgsl = dispatch->hgsl;

	rt_mutex_lock(&dispatch->mutex);

	_retire_drawobjs(ctxt);
	hgsl_process_event_group(hgsl, &ctxt->event_group);
	hgsl_dispatch_ctxt_issuecmds(ctxt);

	rt_mutex_unlock(&dispatch->mutex);
}

int hgsl_dispatch_queue_context(struct hgsl_context *ctxt)
{
	struct hgsl_dispatch_context *dispatch = ctxt->dispatch;

	if (!hgsl_context_get(ctxt))
		return 0;

	trace_dispatch_queue_context(ctxt);
	atomic_inc(&dispatch->count);
	hgsl_dispatch_ctxt_schedule(ctxt);

	return 0;
}

int hgsl_dispatch_queue_cmds(
	struct hgsl_priv *priv, struct hgsl_context *ctxt,
	struct hgsl_drawobj *drawobj[],
	u32 count, u32 *timestamp)
{
	int ret = 0;
	u32 i, user_ts;

	/*
	 * There is always a possibility that dispatcher may end up pushing
	 * the last popped draw object back to the context drawq. Hence,
	 * we can only queue up to _context_drawqueue_size - 1 here to make
	 * sure we never let drawq->queued exceed _context_drawqueue_size.
	 */
	if (!ctxt || !count || count > _context_drawqueue_size - 1) {
		LOGE("drawobj number %u or invalid context", count);
		return -EINVAL;
	}

	spin_lock(&ctxt->drawq_lock);

	ret = _check_for_room_in_context_drawq(ctxt, count);
	if (ret)
		goto out;

	user_ts = *timestamp;
	/*
	 * If there is only one drawobj in the array and it is of
	 * type SYNCOBJ_TYPE, skip comparing user_ts as it can be 0
	 */
	if (!(count == 1 && drawobj[0]->type == SYNCOBJ_TYPE) &&
		(ctxt->flags & GSL_CONTEXT_FLAG_CLIENT_GENERATED_TS)) {
		/*
		 * User specified timestamps need to be greater than the last
		 * issued timestamp in the context
		 */
		if (hgsl_ts32_ge(ctxt->queued_ts, user_ts)) {
			LOGW("ctx:%d next client ts %d isn't greater than current ts %d",
				ctxt->context_id, user_ts, ctxt->queued_ts);
			ret = -ERANGE;
			goto out;
		}
	}
	for (i = 0; i < count; i++) {
		switch (drawobj[i]->type) {
		case MARKEROBJ_TYPE:
			ret = _queue_markerobj(ctxt, CMDOBJ(drawobj[i]),
					timestamp, user_ts);
			if (ret == 1) {
				ret = 0;
				goto out;
			} else if (ret)
				goto out;
			break;
		case CMDOBJ_TYPE:
			ret = _queue_cmdobj(ctxt, CMDOBJ(drawobj[i]),
						timestamp, user_ts);
			if (ret)
				goto out;
			break;
		case SYNCOBJ_TYPE:
			_queue_syncobj(ctxt, SYNCOBJ(drawobj[i]),
						timestamp);
			break;
		case BINDOBJ_TYPE:
			/* TODO: */
			LOGW("BINDOBJ is not supported");
			break;
		case TIMELINEOBJ_TYPE:
			_queue_timelineobj(ctxt, drawobj[i]);
			break;
		default:
			ret = -EINVAL;
			goto out;
		}
	}
	spin_unlock(&ctxt->drawq_lock);

	return hgsl_dispatch_queue_context(ctxt);
out:
	spin_unlock(&ctxt->drawq_lock);
	return ret;
}

int hgsl_dispatch_ctxt_init(struct qcom_hgsl *hgsl,
	struct hgsl_context *ctxt)
{
	int ret = 0;
	struct hgsl_dispatch_context *dispatch;

	rt_mutex_lock(&ctxt->dispatch_lock);
	if (ctxt->dispatch)
		goto out;

	dispatch = hgsl_zalloc(sizeof(*dispatch));
	if (IS_ERR_OR_NULL(dispatch)) {
		ret = -ENOMEM;
		goto out;
	}

	dispatch->worker = kthread_create_worker(0, "hgsl_dispatch_%u_%u",
		ctxt->devhandle, ctxt->context_id);
	if (IS_ERR(dispatch->worker)) {
		ret = PTR_ERR(dispatch->worker);
		hgsl_free(dispatch);
		goto out;
	}

	rt_mutex_init(&dispatch->mutex);
	kthread_init_work(&dispatch->work, hgsl_dispatch_ctxt_work);

	sched_set_fifo(dispatch->worker->task);

	INIT_LIST_HEAD(&dispatch->drawobj_list);
	atomic_set(&dispatch->count, 0);

	dispatch->hgsl = hgsl;
	dispatch->ctxt = ctxt;
	ctxt->dispatch = dispatch;

out:
	rt_mutex_unlock(&ctxt->dispatch_lock);
	return ret;
}

void hgsl_dispatch_ctxt_deinit(struct qcom_hgsl *hgsl,
	struct hgsl_context *ctxt)
{
	struct hgsl_dispatch_context *dispatch = ctxt->dispatch;

	/* Remove the event group from the list */
	hgsl_del_event_group(hgsl, &ctxt->event_group);

	rt_mutex_lock(&ctxt->dispatch_lock);
	if (!dispatch)
		goto out;

	if (!IS_ERR_OR_NULL(dispatch->worker))
		kthread_destroy_worker(dispatch->worker);

	hgsl_free(dispatch);
	ctxt->dispatch = NULL;

out:
	rt_mutex_unlock(&ctxt->dispatch_lock);
}

void hgsl_dispatch_deinit(struct qcom_hgsl *hgsl)
{
	hgsl_events_deinit(hgsl);
	hgsl_drawobjs_deinit();

	if (!IS_ERR(obj_cache))
		kmem_cache_destroy(obj_cache);
}

int hgsl_dispatch_init(struct qcom_hgsl *hgsl)
{
	/* Set up the GPU events for the device */
	int ret = hgsl_events_init(hgsl);

	if (ret) {
		LOGE("events init failed, ret %d\n", ret);
		return ret;
	}

	hgsl_drawobjs_init();
	obj_cache = KMEM_CACHE(cmd_obj, 0);

	return 0;
}
