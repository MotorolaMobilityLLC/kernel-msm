/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#if !defined(_HGSL_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _HGSL_TRACE_H

#undef TRACE_SYSTEM
#define TRACE_SYSTEM hgsl
#undef TRACE_INCLUDE_PATH
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_PATH .
#define TRACE_INCLUDE_FILE hgsl_trace

#include <linux/tracepoint.h>

TRACE_EVENT(isync_release,
	TP_PROTO(
		u32 id
	),
	TP_ARGS(
		id
	),
	TP_STRUCT__entry(
		__field(u32, id)
	),
	TP_fast_assign(
		__entry->id = id;
	),
	TP_printk("id=%u",
		__entry->id
	)
);

DECLARE_EVENT_CLASS(hgsl_isync_class,
	TP_PROTO(u32 timeline_id, u64 timestamp),
	TP_ARGS(timeline_id, timestamp),
	TP_STRUCT__entry(
		__field(u32, timeline_id)
		__field(u64, timestamp)
	),
	TP_fast_assign(
		__entry->timeline_id = timeline_id;
		__entry->timestamp = timestamp;
	),
	TP_printk("timeline_id=%u ts=%llu",
		__entry->timeline_id, __entry->timestamp
	)
);

DEFINE_EVENT(hgsl_isync_class, isync_alloc,
	TP_PROTO(u32 timeline_id, u64 timestamp),
	TP_ARGS(timeline_id, timestamp)
);

DEFINE_EVENT(hgsl_isync_class, isync_signal,
	TP_PROTO(u32 timeline_id, u64 timestamp),
	TP_ARGS(timeline_id, timestamp)
);

DEFINE_EVENT(hgsl_isync_class, isync_fence_alloc,
	TP_PROTO(u32 timeline_id, u64 timestamp),
	TP_ARGS(timeline_id, timestamp)
);

DEFINE_EVENT(hgsl_isync_class, isync_fence_release,
	TP_PROTO(u32 timeline_id, u64 timestamp),
	TP_ARGS(timeline_id, timestamp)
);

TRACE_EVENT(drawobj_timeline,
	TP_PROTO(u32 timeline_id, u64 timepoint
	),
	TP_ARGS(timeline_id, timepoint
	),
	TP_STRUCT__entry(
		__field(u32, timeline_id)
		__field(u64, timepoint)
	),
	TP_fast_assign(
		__entry->timeline_id = timeline_id;
		__entry->timepoint = timepoint;
	),
	TP_printk("timeline_id=%u timepoint=%llu",
		__entry->timeline_id, __entry->timepoint
	)
);

TRACE_EVENT(drawobj_queued,
	TP_PROTO(struct hgsl_drawobj *drawobj, u32 queued),
	TP_ARGS(drawobj, queued),
	TP_STRUCT__entry(
		__field(u32, devhandle)
		__field(u32, context_id)
		__field(u32, refcount)
		__field(u32, timestamp)
		__field(u32, queued)
		__field(u32, flags)
	),
	TP_fast_assign(
		__entry->devhandle = drawobj->context->devhandle;
		__entry->context_id = drawobj->context->context_id;
		__entry->refcount = kref_read(&drawobj->context->kref);
		__entry->timestamp = drawobj->timestamp;
		__entry->queued = queued;
		__entry->flags = drawobj->flags;
	),
	TP_printk("ctx=[%u:%u] refcount=%u ts=%u queued=%u flags=%s",
			__entry->devhandle, __entry->context_id,
			__entry->refcount, __entry->timestamp, __entry->queued,
			__entry->flags ? __print_flags(
						__entry->flags, "|",
						{ HGSL_DRAWOBJ_MARKER, "MARKER" },
						{ HGSL_DRAWOBJ_CTX_SWITCH, "CTX_SWITCH" },
						{ HGSL_DRAWOBJ_SYNC, "SYNC" },
						{ HGSL_DRAWOBJ_END_OF_FRAME, "EOF" },
						{ HGSL_DRAWOBJ_SUBMIT_IB_LIST, "IB_LIST" }
						) : "none"
	)
);

DECLARE_EVENT_CLASS(hgsl_drawobj_class,
	TP_PROTO(struct hgsl_drawobj *drawobj),
	TP_ARGS(drawobj),
	TP_STRUCT__entry(
		__field(u32, devhandle)
		__field(u32, context_id)
		__field(u32, refcount)
		__field(u32, timestamp)
	),
	TP_fast_assign(
		__entry->devhandle = drawobj->context->devhandle;
		__entry->context_id = drawobj->context->context_id;
		__entry->refcount = kref_read(&drawobj->context->kref);
		__entry->timestamp = drawobj->timestamp;
	),
	TP_printk("ctx=[%u:%u] refcount=%u timestamp=%u",
		__entry->devhandle, __entry->context_id,
		__entry->refcount, __entry->timestamp)
);

DEFINE_EVENT(hgsl_drawobj_class, drawobj_submitted,
	TP_PROTO(struct hgsl_drawobj *drawobj),
	TP_ARGS(drawobj)
);

DEFINE_EVENT(hgsl_drawobj_class, drawobj_retired,
	TP_PROTO(struct hgsl_drawobj *drawobj),
	TP_ARGS(drawobj)
);

DEFINE_EVENT(hgsl_drawobj_class, drawobj_destroy,
	TP_PROTO(struct hgsl_drawobj *drawobj),
	TP_ARGS(drawobj)
);

DECLARE_EVENT_CLASS(hgsl_syncobj_class,
	TP_PROTO(struct hgsl_drawobj_sync *syncobj),
	TP_ARGS(syncobj),
	TP_STRUCT__entry(
		__field(u32, devhandle)
		__field(u32, context_id)
		__field(u32, refcount)
		__field(u32, numsyncs)
		__field(uintptr_t, syncobj)
	),
	TP_fast_assign(
		__entry->devhandle = syncobj->base.context->devhandle;
		__entry->context_id = syncobj->base.context->context_id;
		__entry->refcount = kref_read(&syncobj->base.context->kref);
		__entry->numsyncs = syncobj->numsyncs;
		__entry->syncobj = (uintptr_t)syncobj;
	),
	TP_printk("ctx=[%u:%u] refcount=%u numsyncs=%u syncobj=0x%llx",
		__entry->devhandle, __entry->context_id,
		__entry->refcount, __entry->numsyncs, __entry->syncobj)
);

DEFINE_EVENT(hgsl_syncobj_class, syncobj_queued,
	TP_PROTO(struct hgsl_drawobj_sync *syncobj),
	TP_ARGS(syncobj)
);

DEFINE_EVENT(hgsl_syncobj_class, syncobj_retired,
	TP_PROTO(struct hgsl_drawobj_sync *syncobj),
	TP_ARGS(syncobj)
);

DECLARE_EVENT_CLASS(syncpoint_timestamp_class,
	TP_PROTO(struct hgsl_drawobj_sync *syncobj,
		struct hgsl_context *context,
		u32 timestamp),
	TP_ARGS(syncobj, context, timestamp),
	TP_STRUCT__entry(
		__field(u32, syncobj_devhandle)
		__field(u32, syncobj_context_id)
		__field(u32, devhandle)
		__field(u32, context_id)
		__field(u32, timestamp)
	),
	TP_fast_assign(
		__entry->syncobj_devhandle = syncobj->base.context->devhandle;
		__entry->syncobj_context_id = syncobj->base.context->context_id;
		__entry->devhandle = context->devhandle;
		__entry->context_id = context->context_id;
		__entry->timestamp = timestamp;
	),
	TP_printk("ctx=[%u:%u] sync ctx=[%u:%u] ts=%u",
		__entry->syncobj_devhandle, __entry->syncobj_context_id,
		__entry->devhandle, __entry->context_id,
		__entry->timestamp)
);

DEFINE_EVENT(syncpoint_timestamp_class, syncpoint_timestamp,
	TP_PROTO(struct hgsl_drawobj_sync *syncobj,
		struct hgsl_context *context,
		u32 timestamp),
	TP_ARGS(syncobj, context, timestamp)
);

DEFINE_EVENT(syncpoint_timestamp_class, syncpoint_timestamp_expire,
	TP_PROTO(struct hgsl_drawobj_sync *syncobj,
		struct hgsl_context *context,
		u32 timestamp),
	TP_ARGS(syncobj, context, timestamp)
);

DECLARE_EVENT_CLASS(syncpoint_fence_class,
	TP_PROTO(struct hgsl_drawobj_sync *syncobj, char *name),
	TP_ARGS(syncobj, name),
	TP_STRUCT__entry(
		__string(fence_name, name)
		__field(u32, syncobj_devhandle)
		__field(u32, syncobj_context_id)
	),
	TP_fast_assign(
		__entry->syncobj_devhandle = syncobj->base.context->devhandle;
		__entry->syncobj_context_id = syncobj->base.context->context_id;
		__assign_str(fence_name, name);
	),
	TP_printk("ctx=[%u:%u] fence=%s",
		__entry->syncobj_devhandle, __entry->syncobj_context_id,
		__get_str(fence_name))
);

DEFINE_EVENT(syncpoint_fence_class, syncpoint_fence,
	TP_PROTO(struct hgsl_drawobj_sync *syncobj, char *name),
	TP_ARGS(syncobj, name)
);

DEFINE_EVENT(syncpoint_fence_class, syncpoint_fence_expire,
	TP_PROTO(struct hgsl_drawobj_sync *syncobj, char *name),
	TP_ARGS(syncobj, name)
);

DECLARE_EVENT_CLASS(hgsl_ctxt_class,
	TP_PROTO(struct hgsl_context *ctxt),
	TP_ARGS(ctxt),
	TP_STRUCT__entry(
		__field(u32, devhandle)
		__field(u32, context_id)
		__field(u32, refcount)
	),
	TP_fast_assign(
		__entry->devhandle = ctxt->devhandle;
		__entry->context_id = ctxt->context_id;
		__entry->refcount = kref_read(&ctxt->kref);
	),
	TP_printk("ctx=[%u:%u] refcount=%u", __entry->devhandle,
		__entry->context_id, __entry->refcount)
);

DEFINE_EVENT(hgsl_ctxt_class, ctxt_sleep,
	TP_PROTO(struct hgsl_context *ctxt),
	TP_ARGS(ctxt)
);

DEFINE_EVENT(hgsl_ctxt_class, ctxt_wake,
	TP_PROTO(struct hgsl_context *ctxt),
	TP_ARGS(ctxt)
);

DEFINE_EVENT(hgsl_ctxt_class, dispatch_queue_context,
	TP_PROTO(struct hgsl_context *ctxt),
	TP_ARGS(ctxt)
);

DEFINE_EVENT(hgsl_ctxt_class, ctxt_detach_drawobjs,
	TP_PROTO(struct hgsl_context *ctxt),
	TP_ARGS(ctxt)
);

DEFINE_EVENT(hgsl_ctxt_class, ctxt_release,
	TP_PROTO(struct hgsl_context *ctxt),
	TP_ARGS(ctxt)
);

TRACE_EVENT(hgsl_aux_command,
	TP_PROTO(u32 devhandle, u32 context_id, u32 numcmds,
		u32 flags, u32 timestamp
	),
	TP_ARGS(devhandle, context_id, numcmds, flags, timestamp
	),
	TP_STRUCT__entry(
		__field(u32, devhandle)
		__field(u32, context_id)
		__field(u32, numcmds)
		__field(u32, flags)
		__field(u32, timestamp)
	),
	TP_fast_assign(
		__entry->devhandle = devhandle;
		__entry->context_id = context_id;
		__entry->numcmds = numcmds;
		__entry->flags = flags;
		__entry->timestamp = timestamp;
	),
	TP_printk("context=[%u:%u] numcmds=%u flags=0x%x timestamp=%u",
		__entry->devhandle, __entry->context_id, __entry->numcmds,
		__entry->flags, __entry->timestamp
	)
);

TRACE_EVENT(next_timestamp,
	TP_PROTO(struct hgsl_context *ctxt, u32 timestamp, int ret
	),
	TP_ARGS(ctxt, timestamp, ret
	),
	TP_STRUCT__entry(
		__field(u32, devhandle)
		__field(u32, context_id)
		__field(u32, timestamp)
		__field(int, ret)
	),
	TP_fast_assign(
		__entry->devhandle = ctxt->devhandle;
		__entry->context_id = ctxt->context_id;
		__entry->timestamp = timestamp;
		__entry->ret = ret;
	),
	TP_printk("context=[%u:%u] timestamp=%u ret=%d",
		__entry->devhandle, __entry->context_id,
		__entry->timestamp, __entry->ret
	)
);

#endif /* _HGSL_TRACE_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
