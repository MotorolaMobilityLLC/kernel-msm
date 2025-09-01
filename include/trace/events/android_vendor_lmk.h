/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2025 Google, Inc.
 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM android_vendor_lmk
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/events
#if !defined(_ANDROID_VENDOR_LMK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _ANDROID_VENDOR_LMK_H

#include <linux/tracepoint.h>

TRACE_EVENT(android_trigger_vendor_lmk_kill,
	TP_PROTO(int reason, short min_oom_score_adj),
	TP_ARGS(reason, min_oom_score_adj),

	TP_STRUCT__entry(
		__field(int, reason)
		__field(short, min_oom_score_adj)
	),
	TP_fast_assign(
		__entry->reason = reason;
		__entry->min_oom_score_adj = min_oom_score_adj;
	),
	TP_printk("reason=%u min_oom_score_adj=%hd", __entry->reason, __entry->min_oom_score_adj)
);

#endif /* _ANDROID_VENDOR_LMK_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
