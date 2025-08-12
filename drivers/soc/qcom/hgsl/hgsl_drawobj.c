// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/slab.h>
#include <linux/dma-fence-array.h>

#include <uapi/linux/hgsl.h>
#include "hgsl.h"
#include "hgsl_drawobj.h"
#include "hgsl_dispatch.h"
#include "hgsl_trace.h"

/*
 * Define an kmem cache for the memobj structures since we
 * allocate and free them so frequently
 */
static struct kmem_cache *memobjs_cache;

static void _timestamp_signaled(struct qcom_hgsl *hgsl,
		struct hgsl_event_group *group, void *priv, int ret);

static void syncobj_destroy_object(struct hgsl_drawobj *drawobj)
{
	struct hgsl_drawobj_sync *syncobj = SYNCOBJ(drawobj);
	int i;

	for (i = 0; i < syncobj->numsyncs; i++) {
		struct hgsl_drawobj_sync_event *event = &syncobj->synclist[i];

		if (event->type == HGSL_CMD_SYNCPOINT_TYPE_FENCE) {
			struct event_fence_info *priv = event->priv;

			if (priv) {
				kfree(priv->fences);
				kfree(priv);
			}

			if (event->handle) {
				struct hgsl_sync_fence_cb *kcb = event->handle;

				dma_fence_put(kcb->fence);
				kfree(kcb);
			}

		} else if (event->type == HGSL_CMD_SYNCPOINT_TYPE_TIMELINE) {
			kfree(event->priv);
		}
	}

	kfree(syncobj->synclist);
	kfree(syncobj);
}

static void cmdobj_destroy_object(struct hgsl_drawobj *drawobj)
{
	kfree(CMDOBJ(drawobj));
}

static void timelineobj_destroy_object(struct hgsl_drawobj *drawobj)
{
	kfree(TIMELINEOBJ(drawobj));
}

void hgsl_drawobj_destroy_object(struct kref *kref)
{
	struct hgsl_drawobj *drawobj = container_of(kref,
		struct hgsl_drawobj, refcount);

	trace_drawobj_destroy(drawobj);
	hgsl_put_context(drawobj->context);
	drawobj->destroy_object(drawobj);
}

static void cmdobj_destroy(struct hgsl_drawobj *drawobj)
{
	struct hgsl_drawobj_cmd *cmdobj = CMDOBJ(drawobj);
	struct hgsl_memobj_node *mem, *tmpmem;

	/* Destroy the command list */
	list_for_each_entry_safe(mem, tmpmem, &cmdobj->cmdlist, node) {
		list_del_init(&mem->node);
		kmem_cache_free(memobjs_cache, mem);
	}

	/* Destroy the memory list */
	list_for_each_entry_safe(mem, tmpmem, &cmdobj->memlist, node) {
		list_del_init(&mem->node);
		kmem_cache_free(memobjs_cache, mem);
	}
}

static void syncobj_timer(struct timer_list *t)
{
	struct hgsl_drawobj_sync *syncobj = from_timer(syncobj, t, timer);
	struct hgsl_drawobj *drawobj;
	struct hgsl_drawobj_sync_event *event;
	unsigned int i;

	drawobj = DRAWOBJ(syncobj);
	if (!kref_get_unless_zero(&drawobj->refcount))
		return;

	if (drawobj->context == NULL) {
		hgsl_drawobj_put(drawobj);
		return;
	}

	LOGE("hgsl: possible gpu syncpoint deadlock for context %u timestamp %u\n",
		drawobj->context->context_id, drawobj->timestamp);
	LOGE("      pending events:\n");
	for (i = 0; i < syncobj->numsyncs; i++) {
		event = &syncobj->synclist[i];

		if (!hgsl_drawobj_event_pending(syncobj, i))
			continue;

		switch (event->type) {
		case HGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP:
			LOGE("       [%u] TIMESTAMP %u:%u\n",
				i, event->context->context_id, event->timestamp);
			break;
		case HGSL_CMD_SYNCPOINT_TYPE_FENCE: {
			int j;
			struct event_fence_info *info = event->priv;

			for (j = 0; info && j < info->num_fences; j++)
				LOGE("       [%u] FENCE %s\n",
					i, info->fences[j].name);
			break;
		}
		case HGSL_CMD_SYNCPOINT_TYPE_TIMELINE: {
			int j;
			struct event_timeline_info *info = event->priv;
			struct dma_fence *fence = event->fence;
			bool retired = false;
			bool signaled = test_bit(DMA_FENCE_FLAG_SIGNALED_BIT,
					&fence->flags);
			const char *str = NULL;

			if (fence->ops->signaled && fence->ops->signaled(fence))
				retired = true;

			if (!retired)
				str = "not retired";
			else if (retired && signaled)
				str = "signaled";
			else if (retired && !signaled)
				str = "retired but not signaled";
			LOGE("       [%u] FENCE %s\n",
				i, str);
			for (j = 0; info && info[j].timeline; j++)
				LOGE("       TIMELINE %d SEQNO %lld\n",
					info[j].timeline, info[j].seqno);
			break;
		}
		}
	}

	hgsl_drawobj_put(drawobj);
	LOGE("--gpu syncpoint deadlock print end--\n");
}

/*
 * a generic function to retire a pending sync event and (possibly) kick the
 * dispatcher.
 * Returns false if the event was already marked for cancellation in another
 * thread. This function should return true if this thread is responsible for
 * freeing up the memory, and the event will not be cancelled.
 */
static bool drawobj_sync_expire(struct qcom_hgsl *hgsl,
	struct hgsl_drawobj_sync_event *event)
{
	struct hgsl_drawobj_sync *syncobj = event->syncobj;
	/*
	 * Clear the event from the pending mask - if it is already clear, then
	 * leave without doing anything useful
	 */
	if (!test_and_clear_bit(event->id, &syncobj->pending))
		return false;

	/*
	 * If no more pending events, delete the timer and schedule the command
	 * for dispatch
	 */
	if (!hgsl_drawobj_events_pending(event->syncobj)) {
		del_timer(&syncobj->timer);

		hgsl_dispatch_queue_context(DRAWOBJ(syncobj)->context);
	}
	return true;
}

static void drawobj_sync_timeline_fence_work(struct work_struct *work)
{
	struct hgsl_drawobj_sync_event *event = container_of(work,
		struct hgsl_drawobj_sync_event, work);

	dma_fence_put(event->fence);
	hgsl_drawobj_put(&event->syncobj->base);
}

static void trace_syncpoint_timeline_fence(
	struct hgsl_drawobj_sync *syncobj,
	struct dma_fence *f, bool expire)
{
	struct dma_fence_array *array = to_dma_fence_array(f);
	struct dma_fence **fences = &f;
	u32 num_fences = 1;
	int i;

	if (array) {
		num_fences = array->num_fences;
		fences = array->fences;
	}

	for (i = 0; i < num_fences; i++) {
		char fence_name[HGSL_FENCE_NAME_LEN];

		snprintf(fence_name, sizeof(fence_name), "%s:%llu",
			fences[i]->ops->get_timeline_name(fences[i]),
			fences[i]->seqno);
		if (expire)
			trace_syncpoint_fence_expire(syncobj, fence_name);
		else
			trace_syncpoint_fence(syncobj, fence_name);
	}
}

static void drawobj_sync_timeline_fence_callback(struct dma_fence *f,
		struct dma_fence_cb *cb)
{
	struct hgsl_drawobj_sync_event *event = container_of(cb,
		struct hgsl_drawobj_sync_event, cb);
	struct qcom_hgsl *hgsl = event->hgsl_priv->dev;

	trace_syncpoint_timeline_fence(event->syncobj, f, true);
	/*
	 * Mark the event as synced and then fire off a worker to handle
	 * removing the fence
	 */
	if (drawobj_sync_expire(hgsl, event))
		queue_work(hgsl->lockless_wq, &event->work);
}

void hgsl_ctxt_detach_drawobjs(struct qcom_hgsl *hgsl,
	struct hgsl_context *ctxt)
{
	int i, count = 0;
	struct hgsl_drawobj *list[CONTEXT_DRAWQUEUE_SIZE];

	if (WARN_ON(!hgsl_context_get(ctxt)))
		return;

	wake_up_all(&ctxt->drawq_wq);
	trace_ctxt_detach_drawobjs(ctxt);

	hgsl_flush_event_group(hgsl, &ctxt->event_group);
	spin_lock(&ctxt->drawq_lock);
	while (ctxt->drawq_head != ctxt->drawq_tail) {
		struct hgsl_drawobj *drawobj =
			ctxt->drawq[ctxt->drawq_head];

		ctxt->drawq_head = (ctxt->drawq_head + 1) %
			CONTEXT_DRAWQUEUE_SIZE;

		list[count++] = drawobj;
	}
	spin_unlock(&ctxt->drawq_lock);

	for (i = 0; i < count; i++) {
		hgsl_cancel_events_timestamp(hgsl, &ctxt->event_group,
			list[i]->timestamp);
		hgsl_drawobj_destroy(list[i]);
	}

	if (ctxt->dispatch) {
		hgsl_dispatch_ctxt_schedule(ctxt);
		kthread_flush_worker(ctxt->dispatch->worker);
	}
	hgsl_put_context(ctxt);
}

static void syncobj_destroy(struct hgsl_drawobj *drawobj)
{
	struct hgsl_drawobj_sync *syncobj = SYNCOBJ(drawobj);
	unsigned int i;

	/* Zap the canary timer */
	del_timer_sync(&syncobj->timer);

	/*
	 * Clear all pending events - this will render any subsequent async
	 * callbacks harmless
	 */
	for (i = 0; i < syncobj->numsyncs; i++) {
		struct hgsl_drawobj_sync_event *event = &syncobj->synclist[i];

		/*
		 * Don't do anything if the event has already expired.
		 * If this thread clears the pending bit mask then it is
		 * responsible for doing context put.
		 */
		if (!test_and_clear_bit(i, &syncobj->pending))
			continue;

		switch (event->type) {
		case HGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP:
			hgsl_cancel_event(drawobj->priv->dev,
				&event->context->event_group, event->timestamp,
				_timestamp_signaled, event);
			/*
			 * Do context put here to make sure the context is alive
			 * till this thread cancels hgsl event.
			 */
			hgsl_put_context(event->context);
			break;
		case HGSL_CMD_SYNCPOINT_TYPE_FENCE:
			hgsl_sync_fence_async_cancel(event->handle);
			hgsl_drawobj_put(drawobj);
			break;
		case HGSL_CMD_SYNCPOINT_TYPE_TIMELINE:
			dma_fence_remove_callback(event->fence, &event->cb);
			dma_fence_put(event->fence);
			hgsl_drawobj_put(drawobj);
			break;
		}
	}

	/*
	 * If we cancelled an event, there's a good chance that the context is
	 * on a dispatcher queue, so schedule to get it removed.
	 */
	if (!bitmap_empty(&syncobj->pending, HGSL_MAX_SYNCPOINTS))
		hgsl_dispatch_queue_context(drawobj->context);
}

static void drawobj_add_profiling_buffer(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_cmd *cmdobj,
		uint64_t gpuaddr, uint64_t size,
		unsigned int id, uint64_t offset)
{
	struct hgsl_drawobj *drawobj = DRAWOBJ(cmdobj);
	struct hgsl_mem_node *node_found = NULL;

	if (!(drawobj->flags & HGSL_DRAWOBJ_PROFILING))
		return;

	/* Only the first memory node counts - ignore the rest */
	if (cmdobj->profiling_mem_node != NULL)
		return;

	mutex_lock(&hgsl_priv->lock);
	node_found = hgsl_mem_find_node_locked(&hgsl_priv->mem_allocated,
					gpuaddr, size, false);
	if (!node_found)
		node_found = hgsl_mem_find_node_locked(&hgsl_priv->mem_mapped,
					gpuaddr, size, false);

	mutex_unlock(&hgsl_priv->lock);
	if (!node_found) {
		LOGE("ignore bad profile buffer ctxt %u id %d offset %lld gpuaddr %llx size %lld\n",
			drawobj->context->context_id, id, offset, gpuaddr, size);
		return;
	}

	cmdobj->profiling_mem_gpuaddr = id ?
		(node_found->memdesc.gpuaddr + offset) : gpuaddr;
	cmdobj->profiling_mem_node = node_found;
}

static int drawobj_init(struct hgsl_priv *hgsl_priv,
	struct hgsl_context *ctxt, struct hgsl_drawobj *drawobj,
	int type)
{
	/*
	 * Increase the reference count on the context so it doesn't disappear
	 * during the lifetime of this object
	 */
	if (!hgsl_context_get(ctxt))
		return -ENOENT;

	kref_init(&drawobj->refcount);

	drawobj->priv = hgsl_priv;
	drawobj->context = ctxt;
	drawobj->type = type;

	return 0;
}

static void _drawobj_timelineobj_retire(struct kref *kref)
{
	int i;
	struct hgsl_drawobj_timeline *timelineobj = container_of(kref,
				struct hgsl_drawobj_timeline, sig_refcount);

	for (i = 0; i < timelineobj->count; i++) {
		hgsl_isync_forward(timelineobj->base.priv,
			timelineobj->timelines[i].timeline->id,
			timelineobj->timelines[i].seqno,
			false);

		hgsl_isync_timeline_put(timelineobj->timelines[i].timeline);
		hgsl_put_context(timelineobj->timelines[i].context);
	}

	kvfree(timelineobj->timelines);
	timelineobj->timelines = NULL;
	timelineobj->count = 0;
}

static void hgsl_timelineobj_signal(struct hgsl_drawobj_timeline *timelineobj)
{
	kref_put(&timelineobj->sig_refcount, _drawobj_timelineobj_retire);
}

static void timelineobj_destroy(struct hgsl_drawobj *drawobj)
{
	struct hgsl_drawobj_timeline *timelineobj = TIMELINEOBJ(drawobj);

	/*
	 * The scheduler is done with the timelineobj. Put the initial
	 * sig_refcount to continue with the signaling process.
	 */
	hgsl_timelineobj_signal(timelineobj);
}

static void _timeline_signaled(struct qcom_hgsl *hgsl,
		struct hgsl_event_group *group, void *priv, int ret)
{
	struct hgsl_drawobj_timeline *timelineobj = priv;
	struct hgsl_drawobj *drawobj = DRAWOBJ(timelineobj);

	/* Put the sig_refcount we took when registering this event */
	hgsl_timelineobj_signal(timelineobj);

	/* Put the drawobj refcount we took when registering this event */
	hgsl_drawobj_put(drawobj);
}

static int get_aux_command(void __user *ptr, u64 generic_size,
		int type, void *auxcmd, size_t auxcmd_size)
{
	struct hgsl_gpu_aux_command_generic generic;
	u64 size;

	if (copy_struct_from_user(&generic, sizeof(generic),
			ptr, generic_size))
		return -EFAULT;

	if (generic.type != type)
		return -EINVAL;

	size = min_t(u64, auxcmd_size, generic.size);
	if (copy_from_user(auxcmd, USRPTR(generic.priv), size))
		return -EFAULT;

	return 0;
}

struct hgsl_drawobj_timeline *
hgsl_drawobj_timeline_create(struct hgsl_priv *hgsl_priv,
		struct hgsl_context *ctxt)
{
	int ret;
	struct hgsl_drawobj_timeline *timelineobj =
		kzalloc(sizeof(*timelineobj), GFP_KERNEL);

	if (!timelineobj)
		return ERR_PTR(-ENOMEM);

	ret = drawobj_init(hgsl_priv, ctxt, &timelineobj->base,
		TIMELINEOBJ_TYPE);
	if (ret) {
		kfree(timelineobj);
		return ERR_PTR(ret);
	}

	/*
	 * Initialize the sig_refcount that triggers the timeline signal.
	 * This refcount goes to 0 when:
	 * 1) This timelineobj is popped off the context queue. This implies
	 *    any syncobj blocking this timelineobj was already signaled, or
	 *    the context queue is cleaned up at detach time.
	 * 2) The cmdobjs queued on this context before this timeline object
	 *    are retired.
	 */
	kref_init(&timelineobj->sig_refcount);

	timelineobj->base.destroy = timelineobj_destroy;
	timelineobj->base.destroy_object = timelineobj_destroy_object;

	return timelineobj;
}

int hgsl_drawobj_add_timeline(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_timeline *timelineobj,
		void __user *src, u64 cmdsize)
{
	struct hgsl_gpu_aux_command_timeline cmd;
	struct hgsl_drawobj *drawobj = DRAWOBJ(timelineobj);
	struct hgsl_context *ctxt = drawobj->context;
	int i, ret;
	u32 queued;

	memset(&cmd, 0, sizeof(cmd));
	ret = get_aux_command(src, cmdsize,
		HGSL_GPU_AUX_COMMAND_TIMELINE, &cmd, sizeof(cmd));
	if (ret)
		return ret;

	if (!cmd.count)
		return -EINVAL;

	/* Get the last queued timestamp on the drawobj context */
	ret = hgsl_read_timestamp(ctxt, GSL_TIMESTAMP_QUEUED, &queued);
	if (ret)
		return ret;

	/*
	 * Allocate memory for timelines after validating timestamp
	 * to avoid unnecessary allocation.
	 */
	timelineobj->timelines = kvcalloc(cmd.count,
		sizeof(*timelineobj->timelines),
		GFP_KERNEL | __GFP_NORETRY | __GFP_NOWARN);
	if (!timelineobj->timelines)
		return -ENOMEM;

	src = USRPTR(cmd.timelines);
	for (i = 0; i < cmd.count; i++) {
		struct hgsl_timeline_val val;

		if (copy_struct_from_user(&val, sizeof(val), src,
			cmd.timelines_size)) {
			ret = -EFAULT;
			goto err;
		}

		if (val.padding) {
			ret = -EINVAL;
			goto err;
		}

		timelineobj->timelines[i].timeline =
				hgsl_isync_timeline_get(hgsl_priv,
				val.timeline_id, false);

		if (!timelineobj->timelines[i].timeline) {
			ret = -ENODEV;
			goto err;
		}

		/* Get a context refcount so we can use the context pointer */
		if (!hgsl_context_get(ctxt)) {
			ret = -ENODEV;
			goto err;
		}

		trace_drawobj_timeline(val.timeline_id, val.timepoint);
		timelineobj->timelines[i].seqno = val.timepoint;
		timelineobj->timelines[i].context = ctxt;
		timelineobj->timelines[i].timestamp = queued;

		src += cmd.timelines_size;
	}

	timelineobj->count = cmd.count;

	/*
	 * Register a event to notify us when the last queued timestamp
	 * retires. Take a refcount on the drawobj to keep it valid for the
	 * callback, and take the sig_refcount to synchronize with the
	 * timelineobj retire. Both these refcounts are put in the callback.
	 */
	kref_get(&drawobj->refcount);
	kref_get(&timelineobj->sig_refcount);
	ret = hgsl_add_event(hgsl_priv, &ctxt->event_group, queued,
			_timeline_signaled, timelineobj);

	if (ret)
		goto event_err;

	return 0;

event_err:
	/*
	 * If there was an error, put back sig_refcount and drawobj refcounts.
	 * The caller still holds initial refcounts on both and puts them in
	 * hgsl_drawobj_destroy(). Clean up the timelinelines array since we
	 * do not want to signal anything now.
	 */
	hgsl_timelineobj_signal(timelineobj);
	hgsl_drawobj_put(drawobj);
err:
	for (i = 0; i < cmd.count; i++) {
		hgsl_isync_timeline_put(timelineobj->timelines[i].timeline);
		hgsl_put_context(timelineobj->timelines[i].context);
	}

	kvfree(timelineobj->timelines);
	timelineobj->timelines = NULL;
	return ret;
}

/**
 * Allocate an new hgsl_drawobj_sync structure
 */
struct hgsl_drawobj_sync *hgsl_drawobj_sync_create(
	struct hgsl_priv *hgsl_priv,
	struct hgsl_context *ctxt)
{
	struct hgsl_drawobj_sync *syncobj =
		kzalloc(sizeof(*syncobj), GFP_KERNEL);
	int ret;

	if (!syncobj)
		return ERR_PTR(-ENOMEM);

	ret = drawobj_init(hgsl_priv, ctxt, &syncobj->base, SYNCOBJ_TYPE);
	if (ret) {
		kfree(syncobj);
		return ERR_PTR(ret);
	}

	syncobj->base.destroy = syncobj_destroy;
	syncobj->base.destroy_object = syncobj_destroy_object;

	timer_setup(&syncobj->timer, syncobj_timer, 0);

	return syncobj;
}

/**
 * Allocate a new hgsl_drawobj_cmd structure
 */
struct hgsl_drawobj_cmd *hgsl_drawobj_cmd_create(
	struct hgsl_priv *hgsl_priv, struct hgsl_context *ctxt,
	u64 flags, u32 type)
{
	struct hgsl_drawobj_cmd *cmdobj = kzalloc(sizeof(*cmdobj), GFP_KERNEL);
	int ret;

	if (!cmdobj)
		return ERR_PTR(-ENOMEM);

	ret = drawobj_init(hgsl_priv, ctxt, &cmdobj->base,
		(type & (CMDOBJ_TYPE | MARKEROBJ_TYPE)));
	if (ret) {
		kfree(cmdobj);
		return ERR_PTR(ret);
	}

	cmdobj->base.destroy = cmdobj_destroy;
	cmdobj->base.destroy_object = cmdobj_destroy_object;

	cmdobj->base.flags = flags;

	INIT_LIST_HEAD(&cmdobj->cmdlist);
	INIT_LIST_HEAD(&cmdobj->memlist);

	return cmdobj;
}

/* Returns:
 *   -EINVAL: Bad data
 *   0: All data fields are empty (nothing to do)
 *   1: All list information is valid
 */
static int _verify_input_list(unsigned int count, void __user *ptr,
		unsigned int size)
{
	/* Return early if nothing going on */
	if (count == 0 && ptr == NULL && size == 0)
		return 0;

	/* Sanity check inputs */
	if (count == 0 || ptr == NULL || size == 0)
		return -EINVAL;

	return 1;
}

/**
 * hgsl_drawobj_destroy() - Destroy a hgsl object structure
 * @obj: Pointer to the hgsl object to destroy
 *
 * Start the process of destroying a command batch.  Cancel any pending events
 * and decrement the refcount.  Asynchronous events can still signal after
 * hgsl_drawobj_destroy has returned.
 */
void hgsl_drawobj_destroy(struct hgsl_drawobj *drawobj)
{
	if (IS_ERR_OR_NULL(drawobj))
		return;

	trace_drawobj_retired(drawobj);
	drawobj->destroy(drawobj);

	hgsl_drawobj_put(drawobj);
}

static bool drawobj_sync_fence_func(void *priv)
{
	struct hgsl_drawobj_sync_event *event = priv;
	struct event_fence_info *info = event->priv;
	int i;

	for (i = 0; info && i < info->num_fences; i++) {
		trace_syncpoint_fence_expire(event->syncobj,
			info->fences[i].name);
	}
	/*
	 * Only call hgsl_drawobj_put() if it's not marked for cancellation
	 * in another thread.
	 */
	if (drawobj_sync_expire(event->hgsl_priv->dev, event)) {
		hgsl_drawobj_put(&event->syncobj->base);
		return true;
	}
	return false;
}

static struct event_timeline_info *
drawobj_get_sync_timeline_priv(void __user *uptr, u64 usize, u32 count)
{
	int i;
	struct event_timeline_info *priv;

	/* Make sure we don't accidently overflow count */
	if (count == UINT_MAX)
		return NULL;

	priv = kcalloc(count + 1, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return NULL;

	for (i = 0; i < count; i++, uptr += usize) {
		struct hgsl_timeline_val val;

		if (copy_struct_from_user(&val, sizeof(val), uptr, usize))
			continue;

		priv[i].timeline = val.timeline_id;
		priv[i].seqno = val.timepoint;
	}

	priv[i].timeline = 0;
	return priv;
}

static int drawobj_add_sync_timeline(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_sync *syncobj,
		struct hgsl_cmd_syncpoint *sync)
{
	struct hgsl_drawobj *drawobj = DRAWOBJ(syncobj);
	struct hgsl_cmd_syncpoint_timeline sync_timeline;
	struct hgsl_drawobj_sync_event *event;
	struct dma_fence *fence;
	unsigned int id;
	int ret;

	if (copy_struct_from_user(&sync_timeline, sizeof(sync_timeline),
		sync->priv, sync->size))
		return -EFAULT;

	fence = hgsl_timelines_to_fence_array(hgsl_priv,
		sync_timeline.timelines, sync_timeline.count,
		sync_timeline.timelines_size, false);
	if (IS_ERR(fence))
		return PTR_ERR(fence);

	kref_get(&drawobj->refcount);

	id = syncobj->numsyncs++;

	event = &syncobj->synclist[id];

	event->id = id;
	event->type = HGSL_CMD_SYNCPOINT_TYPE_TIMELINE;
	event->syncobj = syncobj;
	event->hgsl_priv = hgsl_priv;
	event->context = NULL;
	event->fence = fence;
	INIT_WORK(&event->work, drawobj_sync_timeline_fence_work);

	INIT_LIST_HEAD(&event->cb.node);

	event->priv =
		drawobj_get_sync_timeline_priv(USRPTR(sync_timeline.timelines),
			sync_timeline.timelines_size, sync_timeline.count);

	/* Set pending flag before adding callback to avoid race */
	set_bit(event->id, &syncobj->pending);

	/* Get a dma_fence refcount to hand over to the callback */
	dma_fence_get(event->fence);
	ret = dma_fence_add_callback(event->fence,
		&event->cb, drawobj_sync_timeline_fence_callback);

	if (ret) {
		clear_bit(event->id, &syncobj->pending);

		if (dma_fence_is_signaled(event->fence)) {
			trace_syncpoint_fence_expire(syncobj, "signaled");
			dma_fence_put(event->fence);
			ret = 0;
		}

		/* Put the refcount from fence creation */
		dma_fence_put(event->fence);
		hgsl_drawobj_put(drawobj);
		return ret;
	}

	trace_syncpoint_timeline_fence(syncobj, event->fence, false);
	/* Put the refcount from fence creation */
	dma_fence_put(event->fence);
	return 0;
}

static int drawobj_add_sync_fence(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_sync *syncobj,
		struct hgsl_cmd_syncpoint *sync)
{
	struct hgsl_cmd_syncpoint_fence sync_fence;
	struct hgsl_drawobj *drawobj = DRAWOBJ(syncobj);
	struct hgsl_drawobj_sync_event *event;
	struct event_fence_info *priv;
	unsigned int id, i;

	if (copy_struct_from_user(&sync_fence, sizeof(sync_fence),
		sync->priv, sync->size))
		return -EFAULT;

	kref_get(&drawobj->refcount);

	id = syncobj->numsyncs++;

	event = &syncobj->synclist[id];

	event->id = id;
	event->type = HGSL_CMD_SYNCPOINT_TYPE_FENCE;
	event->syncobj = syncobj;
	event->hgsl_priv = hgsl_priv;
	event->context = NULL;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);

	set_bit(event->id, &syncobj->pending);

	event->handle = hgsl_sync_fence_async_wait(sync_fence.fd,
		drawobj_sync_fence_func, event);

	event->priv = priv;

	if (IS_ERR_OR_NULL(event->handle)) {
		int ret = PTR_ERR(event->handle);

		clear_bit(event->id, &syncobj->pending);
		event->handle = NULL;

		hgsl_drawobj_put(drawobj);
		/*
		 * If ret == 0 the fence was already signaled - print a trace
		 * message so we can track that
		 */
		if (ret == 0)
			trace_syncpoint_fence_expire(syncobj, "signaled");

		return ret;
	}

	hgsl_get_fence_info(event);

	for (i = 0; priv && i < priv->num_fences; i++)
		trace_syncpoint_fence(syncobj, priv->fences[i].name);

	return 0;
}

static void _timestamp_signaled(struct qcom_hgsl *hgsl,
		struct hgsl_event_group *group, void *priv, int ret)
{
	struct hgsl_drawobj_sync_event *event = priv;

	trace_syncpoint_timestamp_expire(event->syncobj,
		event->context, event->timestamp);
	/*
	 * Put down the context ref count only if
	 * this thread successfully clears the pending bit mask.
	 */
	if (drawobj_sync_expire(hgsl, event))
		hgsl_put_context(event->context);

	hgsl_drawobj_put(&event->syncobj->base);
}

/*
 * Add a new sync point timestamp event to the sync obj.
 */
static int drawobj_add_sync_timestamp(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_sync *syncobj,
		struct hgsl_cmd_syncpoint_timestamp *timestamp)
{
	struct qcom_hgsl *hgsl = hgsl_priv->dev;
	struct hgsl_drawobj *drawobj = DRAWOBJ(syncobj);
	struct hgsl_context *ctxt = hgsl_get_context(hgsl, timestamp->devhandle,
		timestamp->context_id);
	struct hgsl_drawobj_sync_event *event;
	int ret = -EINVAL;
	unsigned int id;

	if (!ctxt)
		return -EINVAL;

	/*
	 * We allow somebody to create a sync point on their own context.
	 * This has the effect of delaying a command from submitting until the
	 * dependent command has cleared.  That said we obviously can't let them
	 * create a sync point on a future timestamp.
	 */
	if (ctxt == drawobj->context) {
		if (hgsl_ts32_ge(timestamp->timestamp, ctxt->queued_ts)) {
			LOGE("Cannot create syncpoint for future timestamp %d (current %d)\n",
				timestamp->timestamp, ctxt->queued_ts);
			goto done;
		}
	}

	kref_get(&drawobj->refcount);

	id = syncobj->numsyncs++;

	event = &syncobj->synclist[id];
	event->id = id;

	event->type = HGSL_CMD_SYNCPOINT_TYPE_TIMESTAMP;
	event->syncobj = syncobj;
	event->context = ctxt;
	event->timestamp = timestamp->timestamp;
	event->hgsl_priv = hgsl_priv;

	set_bit(event->id, &syncobj->pending);
	ret = hgsl_add_event(hgsl_priv, &ctxt->event_group,
		timestamp->timestamp, _timestamp_signaled, event);
	if (ret) {
		clear_bit(event->id, &syncobj->pending);
		hgsl_drawobj_put(drawobj);
	} else
		trace_syncpoint_timestamp(syncobj, ctxt,
			timestamp->timestamp);

done:
	if (ret)
		hgsl_put_context(ctxt);

	return ret;
}

static int drawobj_add_sync_timestamp_from_user(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_sync *syncobj,
		struct hgsl_cmd_syncpoint *sync)
{
	struct hgsl_cmd_syncpoint_timestamp timestamp;

	if (copy_struct_from_user(&timestamp, sizeof(timestamp),
			sync->priv, sync->size))
		return -EFAULT;

	return drawobj_add_sync_timestamp(hgsl_priv, syncobj, &timestamp);
}

static int hgsl_drawobj_add_memobject(struct list_head *head,
		struct hgsl_command_object *obj)
{
	struct hgsl_memobj_node *mem;

	mem = kmem_cache_alloc(memobjs_cache, GFP_KERNEL);
	if (mem == NULL)
		return -ENOMEM;

	mem->gpuaddr = obj->gpuaddr;
	mem->size = obj->size;
	mem->id = obj->id;
	mem->offset = obj->offset;
	mem->flags = obj->flags;
	mem->priv = 0;

	list_add_tail(&mem->node, head);
	return 0;
}

/* This can only accept CMDOBJ_TYPE */
int hgsl_drawobj_cmd_add_cmdlist(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_cmd *cmdobj, void __user *ptr,
		unsigned int size, unsigned int count)
{
	struct hgsl_command_object obj;
	struct hgsl_drawobj *baseobj = DRAWOBJ(cmdobj);
	int i, ret;

	/* Ignore everything if this is a MARKER */
	if (baseobj->type & MARKEROBJ_TYPE)
		return 0;

	ret = _verify_input_list(count, ptr, size);
	if (ret <= 0)
		return ret;

	for (i = 0; i < count; i++) {
		if (copy_struct_from_user(&obj, sizeof(obj), ptr, size))
			return -EFAULT;

		/* Sanity check the flags */
		if (!(obj.flags & CMDLIST_FLAGS)) {
			LOGE(
				"invalid cmdobj ctxt %u flags %d id %d offset %llu addr %llx size %llu\n",
				baseobj->context->context_id, obj.flags, obj.id,
				obj.offset, obj.gpuaddr, obj.size);
			return -EINVAL;
		}

		ret = hgsl_drawobj_add_memobject(&cmdobj->cmdlist, &obj);
		if (ret)
			return ret;

		ptr += sizeof(obj);
		cmdobj->numibs++;
	}

	if (cmdobj->numibs > ECP_MAX_NUM_IB1) {
		LOGE("%u exceed max ib number %u",
			cmdobj->numibs, ECP_MAX_NUM_IB1);
		return -EINVAL;
	}
	return 0;
}

int hgsl_drawobj_cmd_add_memlist(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_cmd *cmdobj, void __user *ptr,
		unsigned int size, unsigned int count)
{
	struct hgsl_command_object obj;
	struct hgsl_drawobj *baseobj = DRAWOBJ(cmdobj);
	int i, ret;

	/* Ignore everything if this is a MARKER */
	if (baseobj->type & MARKEROBJ_TYPE)
		return 0;

	ret = _verify_input_list(count, ptr, size);
	if (ret <= 0)
		return ret;

	for (i = 0; i < count; i++) {
		if (copy_struct_from_user(&obj, sizeof(obj), ptr, size))
			return -EFAULT;

		if (!(obj.flags & HGSL_OBJLIST_MEMOBJ)) {
			LOGE(
				"invalid memobj ctxt %u flags %d id %d offset %lld addr %lld size %llu\n",
				DRAWOBJ(cmdobj)->context->context_id, obj.flags,
				obj.id, obj.offset, obj.gpuaddr,
				obj.size);
			return -EINVAL;
		}
		if (obj.flags & HGSL_OBJLIST_PROFILE)
			drawobj_add_profiling_buffer(hgsl_priv, cmdobj, obj.gpuaddr,
				obj.size, obj.id, obj.offset);
		else {
			ret = hgsl_drawobj_add_memobject(&cmdobj->memlist,
				&obj);
			if (ret)
				return ret;
		}

		ptr += sizeof(obj);
	}

	return 0;
}

static const struct hgsl_add_sync_event add_sync_event_table[] = {
	ADD_SYNC_EVENT_FUNC(TIMESTAMP, timestamp_from_user),
	ADD_SYNC_EVENT_FUNC(FENCE, fence),
	ADD_SYNC_EVENT_FUNC(TIMELINE, timeline),
};

int hgsl_drawobj_sync_add_synclist(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_sync *syncobj, void __user *ptr,
		unsigned int size, unsigned int count)
{
	const struct hgsl_add_sync_event *add_sync_event =
		add_sync_event_table;
	int max_num = ARRAY_SIZE(add_sync_event_table);
	struct hgsl_command_syncpoint syncpt;
	struct hgsl_cmd_syncpoint sync;
	u32 type;
	int i, ret;

	/* If creating a sync and the data is not there or wrong then error */
	ret = _verify_input_list(count, ptr, size);
	if (ret <= 0)
		return -EINVAL;

	syncobj->synclist = kcalloc(count,
		sizeof(struct hgsl_drawobj_sync_event), GFP_KERNEL);

	if (syncobj->synclist == NULL)
		return -ENOMEM;

	for (i = 0; i < count; i++) {
		if (copy_struct_from_user(&syncpt, sizeof(syncpt), ptr, size))
			return -EFAULT;

		type = syncpt.type;
		if (unlikely(type >= max_num)) {
			LOGE("[%s:%d] bad sync type %d for dev_hnd %u ctxt %u",
				__func__, __LINE__, type,
				DRAWOBJ(syncobj)->context->devhandle,
				DRAWOBJ(syncobj)->context->context_id);
			return -EINVAL;
		} else if (unlikely(add_sync_event[type].type != type)) {
			LOGE("[%s:%d] mismatch type [0x%08x 0x%08x]",
				__func__, __LINE__, add_sync_event[type].type, type);
			return -EINVAL;
		}

		sync.type = type;
		sync.priv = USRPTR(syncpt.priv);
		sync.size = syncpt.size;

		ret = add_sync_event[type].func(hgsl_priv, syncobj, &sync);
		if (ret)
			return ret;

		ptr += sizeof(syncpt);
	}

	return 0;
}

void hgsl_drawobjs_deinit(void)
{
	if (!IS_ERR(memobjs_cache))
		kmem_cache_destroy(memobjs_cache);
}

void hgsl_drawobjs_init(void)
{
	memobjs_cache = KMEM_CACHE(hgsl_memobj_node, 0);
}
