/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __HGSL_H_
#define __HGSL_H_

#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/dma-buf.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/sync_file.h>
#include <linux/rtmutex.h>

#include "hgsl_hyp.h"
#include "hgsl_memory.h"
#include "hgsl_tcsr.h"
#include "hgsl_gmugos.h"

/*
 * --- kgsl drawobj flags ---
 * These flags are same as --- drawobj flags ---
 * but renamed to reflect that cmdbatch is renamed to drawobj.
 */
#define HGSL_DRAWOBJ_MEMLIST           HGSL_CMDBATCH_MEMLIST
#define HGSL_DRAWOBJ_MARKER            HGSL_CMDBATCH_MARKER
#define HGSL_DRAWOBJ_SUBMIT_IB_LIST    HGSL_CMDBATCH_SUBMIT_IB_LIST
#define HGSL_DRAWOBJ_CTX_SWITCH        HGSL_CMDBATCH_CTX_SWITCH
#define HGSL_DRAWOBJ_PROFILING         HGSL_CMDBATCH_PROFILING
#define HGSL_DRAWOBJ_END_OF_FRAME      HGSL_CMDBATCH_END_OF_FRAME
#define HGSL_DRAWOBJ_SYNC              HGSL_CMDBATCH_SYNC

#define HGSL_TIMELINE_NAME_LEN 64

#define HGSL_ISYNC_32BITS_TIMELINE 0
#define HGSL_ISYNC_64BITS_TIMELINE 1

#define CONTEXT_DRAWQUEUE_SIZE (128)

#define ECP_MAX_NUM_IB1    (2000)

/* Support upto 3 GVMs: 3 DBQs(Low/Medium/High priority) per GVM */
#define MAX_DB_QUEUE 9
#define HGSL_TCSR_NUM 4

/* Number of the GPU device */
#define HGSL_DEVICE_NUM  (2)
#define HGSL_CONTEXT_NUM (256)

#define USRPTR(a) u64_to_user_ptr((uint64_t)(a))

#define HGSL_MAX_IOC_SIZE (128)
#define HGSL_IOCTL_FUNC(_cmd, _func) \
	[_IOC_NR((_cmd))] = \
		{ .cmd = (_cmd), .func = (_func) }

enum {
	HGSL_DB_SIGNAL_NONE = 0,
	HGSL_DB_SIGNAL_TCSR_0,
	HGSL_DB_SIGNAL_TCSR_1,
	HGSL_DB_SIGNAL_TCSR_2,
	HGSL_DB_SIGNAL_TCSR_3,
	HGSL_DB_SIGNAL_GMU_GOS_0,
	HGSL_DB_SIGNAL_GMU_GOS_1,
	HGSL_DB_SIGNAL_GMU_GOS_2,
	HGSL_DB_SIGNAL_GMU_GOS_3,
	HGSL_DB_SIGNAL_GMU_GOS_4,
	HGSL_DB_SIGNAL_GMU_GOS_5,
	HGSL_DB_SIGNAL_GMU_GOS_6,
	HGSL_DB_SIGNAL_GMU_GOS_7,
	HGSL_DB_SIGNAL_MAX = HGSL_DB_SIGNAL_GMU_GOS_7,
	HGSL_DB_SIGNAL_NUM
};

struct hgsl_ioctl {
	unsigned int cmd;
	int (*func)(struct file *filep, void *data);
};

struct qcom_hgsl;
struct hgsl_hsync_timeline;

#pragma pack(push, 4)
struct shadow_ts {
	unsigned int sop;
	unsigned int unused1;
	unsigned int eop;
	unsigned int unused2;
	unsigned int reserved[6];
};
#pragma pack(pop)

struct reg {
	unsigned long paddr;
	unsigned long size;
	void __iomem *vaddr;
};

struct hw_version {
	unsigned int version;
	unsigned int release;
};

struct db_buffer {
	int32_t dwords;
	void  *vaddr;
};

struct dbq_ibdesc_priv {
	bool   buf_inuse;
	uint32_t context_id;
	uint32_t timestamp;
};

struct doorbell_queue {
	struct dma_buf *dma;
	struct iosys_map map;
	void *vbase;
	uint64_t  gmuaddr;
	struct db_buffer data;
	uint32_t state;
	int tcsr_idx;
	uint32_t dbq_idx;
	struct dbq_ibdesc_priv ibdesc_priv;
	uint32_t  ibdesc_max_size;
	struct mutex lock;
	atomic_t seq_num;
};

struct doorbell_context_queue {
	struct hgsl_mem_node *queue_mem;
	struct iosys_map map;
	uint32_t db_signal;
	uint32_t seq_num;
	void *queue_header;
	void *queue_body;
	void *indirect_ibs;
	uint32_t queue_header_gmuaddr;
	uint32_t queue_body_gmuaddr;
	uint32_t indirect_ibs_gmuaddr;
	uint32_t queue_size;
	int irq_bit_idx;
	uint32_t indirect_ib_ts;
};

struct hgsl_event_group;
typedef void (*hgsl_event_func)(struct qcom_hgsl *, struct hgsl_event_group *,
		void *, int);
/**
 * struct hgsl_event - HGSL timestamp event
 * @hgsl: Pointer to the HGSL device that owns the event
 * @context: Pointer to the context that owns the event
 * @timestamp: Timestamp for the event to expire
 * @func: Callback function for the event when it expires
 * @priv: Private data passed to the callback function
 * @node: List node for the hgsl_event_group list
 * @created: Jiffies when the event was created
 * @work: kthread_work struct for dispatching the callback
 * @result: HGSL event result type to pass to the callback
 * group: The event group this event belongs to
 */
struct hgsl_event {
	struct qcom_hgsl *hgsl;
	struct hgsl_context *context;
	u32 timestamp;
	hgsl_event_func func;
	void *priv;
	struct list_head node;
	u32 created;
	struct kthread_work work;
	int result;
	struct hgsl_event_group *group;
};

typedef int (*readtimestamp_func)(struct hgsl_context *,
	enum gsl_timestamp_type_t, u32 *);

/**
 * struct event_group - A list of HGSL events
 * @context: Pointer to the active context for the events
 * @lock: Spinlock for protecting the list
 * @events: List of active HGSL events
 * @group: Node for the master group list
 * @processed: Last processed timestamp
 * @name: String name for the group (for the debugfs file)
 * @readtimestamp: Function pointer to read a timestamp
 * @priv: Priv member to pass to the readtimestamp function
 */
struct hgsl_event_group {
	struct hgsl_context *context;
	spinlock_t lock;
	struct list_head events;
	struct list_head node;
	u32 processed;
	char name[64];
	readtimestamp_func readtimestamp;
	void *priv;
};

struct qcom_hgsl {
	struct device *dev;

	/* character device info */
	struct cdev cdev;
	dev_t device_no;
	struct class *driver_class;
	struct device *class_dev;

	/* registers mapping */
	struct reg reg_dbidx;

	struct doorbell_queue dbq[MAX_DB_QUEUE];
	struct hgsl_dbq_info dbq_info[MAX_DB_QUEUE];

	/* Could disable db and use isync only */
	bool db_off;

	/* global doorbell tcsr */
	struct hgsl_tcsr *tcsr[HGSL_TCSR_NUM][HGSL_TCSR_ROLE_MAX];
	int tcsr_idx;

	struct hgsl_context **contexts[HGSL_DEVICE_NUM];
	rwlock_t ctxt_lock;

	struct hgsl_gmugos gmugos[HGSL_DEVICE_NUM];

	struct list_head active_wait_list;
	spinlock_t active_wait_lock;

	struct workqueue_struct *wq;
	struct work_struct ts_retire_work;

	struct kthread_worker *events_worker;
	/** @event_groups: List of event groups for this device */
	struct list_head event_groups;
	/** @event_groups_lock: A R/W lock for the events group list */
	rwlock_t event_groups_lock;
	struct workqueue_struct *lockless_wq;

	struct hw_version *ver;
	struct hgsl_hyp_priv_t global_hyp;
	bool global_hyp_inited;
	struct mutex mutex;
	struct list_head active_list;
	struct list_head release_list;
	struct workqueue_struct *release_wq;
	struct work_struct release_work;
	struct idr isync_timeline_idr;
	spinlock_t isync_timeline_lock;
	atomic64_t total_mem_size;
	struct hgsl_cache_flags cache_flags;

	/* Debug nodes */
	struct kobject sysfs;
	struct kobject *clients_sysfs;
	struct dentry *debugfs;
	struct dentry *clients_debugfs;
	struct dentry *debugfs_stat;
};

/**
 * HGSL context define
 **/
struct hgsl_context {
	struct hgsl_priv *priv;
	struct iosys_map map;
	uint32_t context_id;
	uint32_t devhandle;
	uint32_t flags;
	struct shadow_ts *shadow_ts;
	wait_queue_head_t wait_q;
	pid_t pid;
	bool dbq_assigned;
	uint32_t dbq_info;
	struct doorbell_queue *dbq;
	struct hgsl_mem_node *shadow_ts_node;
	uint32_t shadow_ts_flags;
	bool is_fe_shadow;
	bool in_destroy;
	bool destroyed;
	struct kref kref;

	uint32_t last_ts;
	struct hgsl_hsync_timeline *timeline;
	uint32_t queued_ts;
	bool is_killed;
	int tcsr_idx;
	struct mutex lock;
	struct doorbell_context_queue *dbcq;
	uint32_t dbcq_export_id;
	uint32_t db_signal;

	/* Dispatcher */
	spinlock_t drawq_lock;
	struct hgsl_drawobj *drawq[CONTEXT_DRAWQUEUE_SIZE];
	unsigned int drawq_head;
	unsigned int drawq_tail;
	int queued;
	wait_queue_head_t drawq_wq;

	struct rt_mutex dispatch_lock;
	struct hgsl_dispatch_context *dispatch;
	struct hgsl_event_group event_group;
};

struct hgsl_priv {
	struct qcom_hgsl *dev;
	pid_t pid;
	struct list_head node;
	struct hgsl_hyp_priv_t hyp_priv;
	struct mutex lock;
	struct rb_root mem_mapped;
	struct rb_root mem_allocated;
	int open_count;

	atomic64_t total_mem_size;

	/* sysfs stuff */
	struct kobject kobj;
	struct kobject sysfs_client;
	struct kobject sysfs_mem_size;
	struct dentry *debugfs_client;
	struct dentry *debugfs_mem;
	struct dentry *debugfs_memtype;
};

/**
 * struct hgsl_memobj_node - Memory object descriptor
 * @node: Local list node for the object
 * @id: GPU memory ID for the object
 * @offset: Offset within the object
 * @gpuaddr: GPU address for the object
 * @flags: External flags passed by the user
 * @priv: Internal flags set by the driver
 */
struct hgsl_memobj_node {
	struct list_head node;
	u32 id;
	uint64_t offset;
	uint64_t gpuaddr;
	uint64_t size;
	unsigned long flags;
	unsigned long priv;
};

static inline bool hgsl_ts32_ge(uint32_t a, uint32_t b)
{
	static const uint32_t TIMESTAMP_WINDOW = 0x80000000;

	return (a - b) < TIMESTAMP_WINDOW;
}

static inline bool hgsl_ts64_ge(uint64_t a, uint64_t b)
{
	static const uint64_t TIMESTAMP_WINDOW = 0x8000000000000000LL;

	return (a - b) < TIMESTAMP_WINDOW;
}

static inline bool hgsl_ts_ge(uint64_t a, uint64_t b, bool is64)
{
	if (is64)
		return hgsl_ts64_ge(a, b);
	else
		return hgsl_ts32_ge((uint32_t)a, (uint32_t)b);
}

static inline bool hgsl_mem_rb_empty(struct hgsl_priv *priv)
{
	return (RB_EMPTY_ROOT(&priv->mem_mapped) &&
		RB_EMPTY_ROOT(&priv->mem_allocated));
}

/**
 * lightweight function to increase the ref count of context
 */
static inline struct hgsl_context *hgsl_context_get(struct hgsl_context *ctxt)
{
	if (ctxt && kref_get_unless_zero(&ctxt->kref))
		return ctxt;

	return NULL;
}

static inline u32 hgsl_hnd2id(u32 dev_hnd)
{
	return (dev_hnd == GSL_HANDLE_NULL) ? (U32_MAX) :
		((dev_hnd == GSL_HANDLE_DEV1) ? 1 : 0);
}

static inline uint32_t get_context_retired_ts(struct hgsl_context *ctxt)
{
	u32 ts = ctxt->shadow_ts->eop;

	/* ensure read is done before comparison */
	dma_rmb();
	return ts;
}

static inline int get_context_shadow_ts(
	struct hgsl_context *ctxt,
	enum gsl_timestamp_type_t type,
	uint32_t *timestamp)
{
	int ret = 0;

	if (!ctxt || !ctxt->shadow_ts) {
		*timestamp = 0;
		return -EINVAL;
	}

	switch (type) {
	case GSL_TIMESTAMP_RETIRED:
		*timestamp = ctxt->shadow_ts->eop;
		break;
	case GSL_TIMESTAMP_CONSUMED:
		*timestamp = ctxt->shadow_ts->sop;
		break;
	case GSL_TIMESTAMP_QUEUED:
		*timestamp = ctxt->queued_ts;
		break;
	default:
		ret = -EINVAL;
		*timestamp = 0;
		break;
	}

	/* ensure read is done before return */
	dma_rmb();
	LOGD("%d, %u, %u, %u", ret, ctxt->context_id, type, *timestamp);
	return ret;
}

static inline void set_context_shadow_ts(
	struct hgsl_context *ctxt,
	enum gsl_timestamp_type_t type,
	uint32_t ts)
{
	if (!ctxt || !ctxt->shadow_ts)
		return;

	switch (type) {
	case GSL_TIMESTAMP_RETIRED:
		ctxt->shadow_ts->eop = ts;
		break;
	case GSL_TIMESTAMP_CONSUMED:
		ctxt->shadow_ts->sop = ts;
		break;
	default:
		LOGW("invalid type=%u context=[%u:%u] ts=%u",
			type, ctxt->devhandle, ctxt->context_id, ts);
		return;
	}

	/* ensure update is done before return */
	dma_wmb();
	LOGD("[%u:%u], %u, %u", ctxt->devhandle, ctxt->context_id, type, ts);
}

static inline bool _timestamp_retired(struct hgsl_context *ctxt,
	unsigned int timestamp)
{
	return hgsl_ts32_ge(get_context_retired_ts(ctxt), timestamp);
}

/**
 * struct hgsl_hsync_timeline - A sync timeline attached under each hgsl context
 * @kref: Refcount to keep the struct alive
 * @name: String to describe this timeline
 * @fence_context: Used by the fence driver to identify fences belonging to
 *		   this context
 * @child_list_head: List head for all fences on this timeline
 * @lock: Spinlock to protect this timeline
 * @last_ts: Last timestamp when signaling fences
 */
struct hgsl_hsync_timeline {
	struct kref kref;
	struct hgsl_context *context;

	char name[HGSL_TIMELINE_NAME_LEN];
	u64 fence_context;

	spinlock_t lock;
	struct list_head fence_list;
	unsigned int last_ts;
};

/**
 * struct hgsl_hsync_fence - A struct containing a fence and other data
 *				associated with it
 * @fence: The fence struct
 * @sync_file: Pointer to the sync file
 * @parent: Pointer to the hgsl sync timeline this fence is on
 * @child_list: List of fences on the same timeline
 * @context_id: hgsl context id
 * @ts: Context timestamp that this fence is associated with
 */
struct hgsl_hsync_fence {
	struct dma_fence fence;
	struct sync_file *sync_file;
	struct hgsl_hsync_timeline *timeline;
	struct list_head child_list;
	u32 context_id;
	unsigned int ts;
};

struct hgsl_isync_timeline {
	struct kref kref;
	struct list_head free_list;
	char name[HGSL_TIMELINE_NAME_LEN];
	int id;
	struct hgsl_priv *priv;
	struct list_head fence_list;
	u64 context;
	spinlock_t lock;
	u64 last_ts;
	u32 flags;
	bool is64bits;
};

struct hgsl_isync_fence {
	struct dma_fence fence;
	struct list_head free_list;  /* For free in batch */
	struct hgsl_isync_timeline *timeline;
	struct list_head child_list;
	u64 ts;
};

struct hgsl_active_wait {
	struct list_head head;
	struct hgsl_context *ctxt;
	unsigned int timestamp;
};

/**
 * struct hgsl_sync_fence_cb - Used for fence callbacks
 * fence_cb: Fence callback struct
 * fence: Pointer to the fence for which the callback is done
 * priv: Private data for the callback
 * func: Pointer to the hgsl function to call. This function should return
 * false if the sync callback is marked for cancellation in a separate thread.
 */
struct hgsl_sync_fence_cb {
	struct dma_fence_cb fence_cb;
	struct dma_fence *fence;
	void *priv;
	bool (*func)(void *priv);
};

struct hgsl_drawobj_sync_event;

/* Fence for commands. */
struct hgsl_hsync_fence *hgsl_hsync_fence_create(
					struct hgsl_context *context,
					uint32_t ts);
int hgsl_hsync_fence_create_fd(struct hgsl_context *context,
				uint32_t ts);
int hgsl_hsync_timeline_create(struct hgsl_context *context);
void hgsl_hsync_timeline_put(struct hgsl_hsync_timeline *timeline);
void hgsl_hsync_timeline_fini(struct hgsl_context *context);

/* Fence for process sync. */
int hgsl_isync_timeline_create(struct hgsl_priv *priv,
				    uint32_t *timeline_id,
				    uint32_t flags,
				    uint64_t initial_ts);
int hgsl_isync_timeline_destroy(struct hgsl_priv *priv, uint32_t id);
void hgsl_isync_fini(struct hgsl_priv *priv);
int hgsl_isync_fence_create(struct hgsl_priv *priv, uint32_t timeline_id,
				uint32_t ts, bool ts_is_valid, int *fence_fd);
int hgsl_isync_fence_signal(struct hgsl_priv *priv, uint32_t timeline_id,
							       int fence_fd);
int hgsl_isync_forward(struct hgsl_priv *priv, uint32_t timeline_id,
								uint64_t ts, bool check_owner);
struct hgsl_isync_timeline *hgsl_isync_timeline_get(struct hgsl_priv *priv,
		int id, bool check_owner);

void hgsl_isync_timeline_put(struct hgsl_isync_timeline *timeline);

int hgsl_isync_query(struct hgsl_priv *priv, uint32_t timeline_id,
							uint64_t *ts);
int hgsl_isync_wait_multiple(struct hgsl_priv *priv, struct hgsl_timeline_wait *param);

struct dma_fence *hgsl_timelines_to_fence_array(struct hgsl_priv *priv,
		u64 timelines, u32 count, u64 usize, bool any);

void hgsl_retire_common(struct qcom_hgsl *hgsl, u32 dev_hnd);

struct hgsl_context *hgsl_get_context(struct qcom_hgsl *hgsl,
	uint32_t dev_hnd, uint32_t context_id);
void hgsl_put_context(struct hgsl_context *ctxt);

int hgsl_db_next_timestamp(struct hgsl_context *ctxt, uint32_t *timestamp);

int hgsl_read_timestamp(struct hgsl_context *ctxt, enum gsl_timestamp_type_t type,
		u32 *timestamp);

static inline bool hgsl_check_timestamp(struct hgsl_priv *priv,
	struct hgsl_context *ctxt, u32 timestamp)
{
	u32 retired;

	if (hgsl_read_timestamp(ctxt, GSL_TIMESTAMP_RETIRED, &retired))
		return false;

	return hgsl_ts32_ge(retired, timestamp);
}

void hgsl_get_fence_info(struct hgsl_drawobj_sync_event *event);
int hgsl_issue_drawobj(struct qcom_hgsl *hgsl, struct hgsl_drawobj *drawobj);

int hgsl_events_init(struct qcom_hgsl *hgsl);
void hgsl_events_deinit(struct qcom_hgsl *hgsl);

void hgsl_add_event_group(struct qcom_hgsl *hgsl, struct hgsl_event_group *group,
		struct hgsl_context *ctxt, readtimestamp_func readtimestamp, void *priv,
		const char *fmt, ...);
void hgsl_del_event_group(struct qcom_hgsl *hgsl, struct hgsl_event_group *group);
int hgsl_add_event(struct hgsl_priv *hgsl_priv, struct hgsl_event_group *group,
		u32 timestamp, hgsl_event_func func, void *priv);
void hgsl_cancel_event(struct qcom_hgsl *hgsl, struct hgsl_event_group *group,
		u32 timestamp, hgsl_event_func func, void *priv);
void hgsl_cancel_events_timestamp(struct qcom_hgsl *hgsl, struct hgsl_event_group *group,
		u32 timestamp);
void hgsl_process_event_group(struct qcom_hgsl *hgsl, struct hgsl_event_group *group);
void hgsl_flush_event_group(struct qcom_hgsl *hgsl, struct hgsl_event_group *group);

struct hgsl_sync_fence_cb *hgsl_sync_fence_async_wait(int fd, bool (*func)(void *priv),
		void *priv);
void hgsl_sync_fence_async_cancel(struct hgsl_sync_fence_cb *kcb);

#endif /* __HGSL_H_ */
