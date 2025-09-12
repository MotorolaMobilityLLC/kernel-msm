/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __HGSL_DRAWOBJ_H
#define __HGSL_DRAWOBJ_H

#define HGSL_MAX_SYNCPOINTS 32

#define DRAWOBJ(obj) (&obj->base)
#define SYNCOBJ(obj) \
	container_of(obj, struct hgsl_drawobj_sync, base)
#define CMDOBJ(obj) \
	container_of(obj, struct hgsl_drawobj_cmd, base)

#define CMDOBJ_TYPE         BIT(0)
#define MARKEROBJ_TYPE      BIT(1)
#define SYNCOBJ_TYPE        BIT(2)
#define BINDOBJ_TYPE        BIT(3)
#define TIMELINEOBJ_TYPE    BIT(4)

/**
 * enum hgsl_drawobj_cmd_priv - Internal command obj flags
 * @CMDOBJ_SKIP - skip the entire command obj
 * @CMDOBJ_MARKER_EXPIRED: Whether this MARKER object is retired or not
 */
enum hgsl_drawobj_cmd_priv {
	CMDOBJ_SKIP = 0,
	CMDOBJ_MARKER_EXPIRED,
};

#define CMDLIST_FLAGS \
	(HGSL_CMDLIST_IB | \
	 HGSL_CMDLIST_CTXTSWITCH_PREAMBLE | \
	 HGSL_CMDLIST_IB_PREAMBLE)

#define ADD_SYNC_EVENT_FUNC(_type, _func) \
	[HGSL_CMD_SYNCPOINT_TYPE_##_type] = \
		{ .type = (HGSL_CMD_SYNCPOINT_TYPE_##_type), \
		.func = (drawobj_add_sync_##_func) }

/**
 * struct hgsl_drawobj - drawobj descriptor
 * @priv: private data of hgsl
 * @context: context that created the command
 * @type: Object type
 * @timestamp: Timestamp assigned to the command
 * @flags: flags
 * @refcount: kref structure to maintain the reference count
 */
struct hgsl_drawobj {
	struct hgsl_priv *priv;
	struct hgsl_context *context;
	uint32_t type;
	uint32_t timestamp;
	u64 flags;
	struct kref refcount;
	/** @destroy: Callbak function to take down the object */
	void (*destroy)(struct hgsl_drawobj *drawobj);
	/** @destroy_object: Callback function to free the object memory */
	void (*destroy_object)(struct hgsl_drawobj *drawobj);
};

/**
 * struct hgsl_drawobj_cmd - command obj, This covers marker
 * cmds also since markers are special form of cmds that do not
 * need their cmds to be executed.
 * @base: Base hgsl_drawobj, this needs to be the first entry
 * @priv: Internal flags
 * @cmdlist: List of IBs to issue
 * @memlist: List of all memory used in this command batch
 * @marker_timestamp: For markers, the timestamp of the last "real" command that
 * was queued
 * @profiling_buf_entry: Mem entry containing the profiling buffer
 * @profiling_buffer_gpuaddr: GPU virt address of the profile buffer added here
 * for easy access
 */
struct hgsl_drawobj_cmd {
	struct hgsl_drawobj base;
	unsigned long priv;
	struct list_head cmdlist;
	struct list_head memlist;
	u32 marker_timestamp;
	struct hgsl_mem_node *profiling_mem_node;
	uint64_t profiling_mem_gpuaddr;
	/* @numibs: Number of ibs in this cmdobj */
	u32 numibs;
};

/**
 * struct hgsl_drawobj_sync - sync object
 * @base: Base hgsl_drawobj, this needs to be the first entry
 * @synclist: Array of context/timestamp tuples to wait for before issuing
 * @numsyncs: Number of sync entries in the array
 * @pending: Bitmask of sync events that are active
 * @timer: a timer used to track possible sync timeouts for this
 *         sync obj
 * @timeout_jiffies: For a sync obj the jiffies at
 * which the timer will expire
 */
struct hgsl_drawobj_sync {
	struct hgsl_drawobj base;
	struct hgsl_drawobj_sync_event *synclist;
	u32 numsyncs;
	unsigned long pending;
	struct timer_list timer;
	unsigned long timeout_jiffies;
	/** @flags: sync object internal flags */
	u32 flags;
};

struct hgsl_add_sync_event {
	u32 type;
	int (*func)(struct hgsl_priv *priv,
		struct hgsl_drawobj_sync *syncobj,
		struct hgsl_cmd_syncpoint *sync);
};

/**
 * struct hgsl_timeline_event - Contains data to signal a timeline
 */
struct hgsl_timeline_event {
	/** @timeline: Pointer to the timeline to signal */
	struct hgsl_isync_timeline *timeline;
	/** @seqno: seqno of the timeline to signal */
	u64 seqno;
	/** @context: context this event is waiting for */
	struct hgsl_context *context;
	/** @timestamp: context timestamp this event is waiting for */
	u32 timestamp;
};

/**
 * struct hgsl_drawobj_timeline - HGSL timeline signal operation
 */
struct hgsl_drawobj_timeline {
	/** @base: &struct hgsl_drawobj container */
	struct hgsl_drawobj base;
	/** @sig_refcount: Refcount to trigger timeline signaling */
	struct kref sig_refcount;
	/* @timelines: Array of timeline events to signal */
	struct hgsl_timeline_event *timelines;
	/** @count: Number of items in timelines */
	int count;
};

static inline struct hgsl_drawobj_timeline *
TIMELINEOBJ(struct hgsl_drawobj *obj)
{
	return container_of(obj, struct hgsl_drawobj_timeline, base);
}

#define HGSL_EVENT_NAME_LEN  (64)
#define HGSL_FENCE_NAME_LEN  (128)

struct fence_info {
	char name[HGSL_FENCE_NAME_LEN];
};

struct event_fence_info {
	struct fence_info *fences;
	u32 num_fences;
};

struct event_timeline_info {
	u64 seqno;
	u32 timeline;
};

/**
 * struct hgsl_drawobj_sync_event
 * @id: identifer (positiion within the pending bitmap)
 * @type: Syncpoint type
 * @syncobj: Pointer to the syncobj that owns the sync event
 * @context: HGSL context for whose timestamp we want to
 *           register this event
 * @timestamp: Pending timestamp for the event
 * @handle: Pointer to a sync fence handle
 * @hgsl_priv: Pointer to the HGSL private data
 */
struct hgsl_drawobj_sync_event {
	u32 id;
	int type;
	struct hgsl_drawobj_sync *syncobj;
	struct hgsl_context *context;
	u32 timestamp;
	struct hgsl_sync_fence_cb *handle;
	struct hgsl_priv *hgsl_priv;
	/** @priv: Type specific private information */
	void *priv;
	/**
	 * @fence: Pointer to a dma fence for HGSL_CMD_SYNCPOINT_TYPE_TIMELINE
	 * events
	 */
	struct dma_fence *fence;
	/** @cb: Callback struct for HGSL_CMD_SYNCPOINT_TYPE_TIMELINE */
	struct dma_fence_cb cb;
	/** @work : work_struct for HGSL_CMD_SYNCPOINT_TYPE_TIMELINE */
	struct work_struct work;

	struct list_head node;
	struct kthread_work event_work;
};

/**
 * struct events - A list of GPU events
 * @context: Pointer to the active context for the events
 * @lock: Spinlock for protecting the list
 * @event_list: List of active GPU events
 * @processed: Last processed timestamp
 * @name: String name for the events per context
 * @priv: Priv member to pass to the readtimestamp function
 */
struct hgsl_drawobj_events {
	struct hgsl_context *context;
	spinlock_t lock;
	struct list_head event_list;
	u32 processed;
	char name[HGSL_EVENT_NAME_LEN];
	void *priv;
};

void hgsl_drawobjs_init(void);
void hgsl_drawobjs_deinit(void);

void hgsl_drawobj_destroy(struct hgsl_drawobj *drawobj);
void hgsl_drawobj_destroy_object(struct kref *kref);

static inline bool hgsl_drawobj_events_pending(
		struct hgsl_drawobj_sync *syncobj)
{
	return !bitmap_empty(&syncobj->pending, HGSL_MAX_SYNCPOINTS);
}

static inline bool hgsl_drawobj_event_pending(
		struct hgsl_drawobj_sync *syncobj, u32 bit)
{
	if (bit >= HGSL_MAX_SYNCPOINTS)
		return false;

	return test_bit(bit, &syncobj->pending);
}

static inline void hgsl_drawobj_put(struct hgsl_drawobj *drawobj)
{
	if (drawobj)
		kref_put(&drawobj->refcount, hgsl_drawobj_destroy_object);
}

struct hgsl_drawobj_timeline *
hgsl_drawobj_timeline_create(struct hgsl_priv *hgsl_priv, struct hgsl_context *ctxt);
int hgsl_drawobj_add_timeline(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_timeline *timelineobj, void __user *src, u64 cmdsize);
struct hgsl_drawobj_sync *hgsl_drawobj_sync_create(struct hgsl_priv *hgsl_priv,
		struct hgsl_context *ctxt);
struct hgsl_drawobj_cmd *hgsl_drawobj_cmd_create(struct hgsl_priv *hgsl_priv,
		struct hgsl_context *ctxt, u64 flags, u32 type);
int hgsl_drawobj_cmd_add_cmdlist(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_cmd *cmdobj, void __user *ptr, u32 size,
		u32 count);
int hgsl_drawobj_cmd_add_memlist(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_cmd *cmdobj, void __user *ptr,
		u32 size, u32 count);
int hgsl_drawobj_sync_add_synclist(struct hgsl_priv *hgsl_priv,
		struct hgsl_drawobj_sync *syncobj, void __user *ptr,
		u32 size, u32 count);
void hgsl_ctxt_detach_drawobjs(struct qcom_hgsl *hgsl, struct hgsl_context *ctxt);
#endif /* __HGSL_DRAWOBJ_H */
