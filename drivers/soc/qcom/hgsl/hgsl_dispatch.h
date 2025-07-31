/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hgsl_drawobj.h"

struct cmd_obj {
	/** @node: List node to put it in the list of inflight commands */
	struct list_head node;
	/** @drawobj: Handle to the draw object */
	struct hgsl_drawobj *drawobj;
};

struct hgsl_dispatch_context {
	struct qcom_hgsl *hgsl;
	struct hgsl_context *ctxt;
	/** @mutex: Mutex needed to run dispatcher function */
	struct rt_mutex mutex;

	/** @count - The count of dispatch jobs */
	atomic_t count;
	/** @drawobj_list: List of objects submitted to dispatch queues */
	struct list_head drawobj_list;
	/** @scheduler_worker: kthread worker for scheduling gpu commands */
	struct kthread_worker *worker;
	/** @scheduler_work: work_struct to put the gpu command scheduler in a work queue */
	struct kthread_work work;
};

int hgsl_dispatch_init(struct qcom_hgsl *hgsl);
void hgsl_dispatch_deinit(struct qcom_hgsl *hgsl);

int hgsl_dispatch_ctxt_init(struct qcom_hgsl *hgsl,
	struct hgsl_context *ctxt);
void hgsl_dispatch_ctxt_deinit(struct qcom_hgsl *hgsl,
	struct hgsl_context *ctxt);

int hgsl_dispatch_queue_cmds(struct hgsl_priv *priv,
	struct hgsl_context *ctxt, struct hgsl_drawobj *drawobj[],
	u32 count, u32 *timestamp);
int hgsl_dispatch_queue_context(struct hgsl_context *ctxt);

void hgsl_dispatch_ctxt_schedule(struct hgsl_context *ctxt);
