/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM virtio_fastrpc

#if !defined(_TRACE_VIRTIO_FASTRPC_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_VIRTIO_FASTRPC_H
#include <linux/tracepoint.h>
#include "virtio_fastrpc_core.h"

TRACE_EVENT(fastrpc_internal_invoke_start,

	TP_PROTO(uint32_t handle, uint32_t sc, s64 seq_num),

	TP_ARGS(handle, sc, seq_num),

	TP_STRUCT__entry(
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(s64, seq_num)
		__field(unsigned long long, mpm_tv)
	),

	TP_fast_assign(
		__entry->handle = handle;
		__entry->sc = sc;
		__entry->seq_num = seq_num;
		__entry->mpm_tv = msm_hr_timer_get_sclk_ticks();
	),

	TP_printk("%lld:0x%x:0x%x:%llu", __entry->seq_num,
		__entry->handle, __entry->sc,
		__entry->mpm_tv)
);

TRACE_EVENT(fastrpc_txbuf_send_start,

	TP_PROTO(struct vfastrpc_invoke_ctx *ctx),

	TP_ARGS(ctx),

	TP_STRUCT__entry(
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(s64, seq_num)
		__field(unsigned long long, mpm_tv)
	),

	TP_fast_assign(
		__entry->handle = ctx->handle;
		__entry->sc = ctx->sc;
		__entry->seq_num = ctx->seq_num;
		__entry->mpm_tv = msm_hr_timer_get_sclk_ticks();
	),

	TP_printk("%lld:0x%x:0x%x:%llu", __entry->seq_num,
		__entry->handle, __entry->sc,
		__entry->mpm_tv)
);

TRACE_EVENT(fastrpc_txbuf_send_end,

	TP_PROTO(struct vfastrpc_invoke_ctx *ctx),

	TP_ARGS(ctx),

	TP_STRUCT__entry(
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(s64, seq_num)
		__field(unsigned long long, mpm_tv)
	),

	TP_fast_assign(
		__entry->handle = ctx->handle;
		__entry->sc = ctx->sc;
		__entry->seq_num = ctx->seq_num;
		__entry->mpm_tv = msm_hr_timer_get_sclk_ticks();
	),

	TP_printk("%lld:0x%x:0x%x:%llu", __entry->seq_num,
		__entry->handle, __entry->sc,
		__entry->mpm_tv)
);

TRACE_EVENT(recv_done_start,

	TP_PROTO(uint32_t dummy),

	TP_ARGS(dummy),

	TP_STRUCT__entry(
		__field(uint32_t, dummy)
	),

	TP_fast_assign(
		__entry->dummy = dummy;
	),

	TP_printk("%d", __entry->dummy)
);

TRACE_EVENT(recv_single_end,

	TP_PROTO(struct vfastrpc_invoke_ctx *ctx),

	TP_ARGS(ctx),

	TP_STRUCT__entry(
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(s64, seq_num)
	),

	TP_fast_assign(
		__entry->handle = ctx->handle;
		__entry->sc = ctx->sc;
		__entry->seq_num = ctx->seq_num;
	),

	TP_printk("%lld:0x%x:0x%x", __entry->seq_num,
		__entry->handle, __entry->sc)
);

TRACE_EVENT(fastrpc_internal_invoke_interrupted,

	TP_PROTO(struct vfastrpc_invoke_ctx *ctx),

	TP_ARGS(ctx),

	TP_STRUCT__entry(
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(s64, seq_num)
		__field(unsigned long long, mpm_tv)
	),

	TP_fast_assign(
		__entry->handle = ctx->handle;
		__entry->sc = ctx->sc;
		__entry->seq_num = ctx->seq_num;
		__entry->mpm_tv = msm_hr_timer_get_sclk_ticks();
	),

	TP_printk("%lld:0x%x:0x%x:%llu", __entry->seq_num,
		__entry->handle, __entry->sc,
		__entry->mpm_tv)
);

TRACE_EVENT(wait_for_completion_end,

	TP_PROTO(struct vfastrpc_invoke_ctx *ctx),

	TP_ARGS(ctx),

	TP_STRUCT__entry(
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(s64, seq_num)
		__field(unsigned long long, mpm_tv)
	),

	TP_fast_assign(
		__entry->handle = ctx->handle;
		__entry->sc = ctx->sc;
		__entry->seq_num = ctx->seq_num;
		__entry->mpm_tv = msm_hr_timer_get_sclk_ticks();
	),

	TP_printk("%lld:0x%x:0x%x:%llu", __entry->seq_num,
		__entry->handle, __entry->sc,
		__entry->mpm_tv)
);

TRACE_EVENT(fastrpc_rxbuf_send_start,

	TP_PROTO(struct vfastrpc_invoke_ctx *ctx),

	TP_ARGS(ctx),

	TP_STRUCT__entry(
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(s64, seq_num)
		__field(unsigned long long, mpm_tv)
	),

	TP_fast_assign(
		__entry->handle = ctx->handle;
		__entry->sc = ctx->sc;
		__entry->seq_num = ctx->seq_num;
		__entry->mpm_tv = msm_hr_timer_get_sclk_ticks();
	),

	TP_printk("%lld:0x%x:0x%x:%llu", __entry->seq_num,
		__entry->handle, __entry->sc,
		__entry->mpm_tv)
);

TRACE_EVENT(fastrpc_rxbuf_send_end,

	TP_PROTO(struct vfastrpc_invoke_ctx *ctx),

	TP_ARGS(ctx),

	TP_STRUCT__entry(
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(s64, seq_num)
		__field(unsigned long long, mpm_tv)
	),

	TP_fast_assign(
		__entry->handle = ctx->handle;
		__entry->sc = ctx->sc;
		__entry->seq_num = ctx->seq_num;
		__entry->mpm_tv = msm_hr_timer_get_sclk_ticks();
	),

	TP_printk("%lld:0x%x:0x%x:%llu", __entry->seq_num,
		__entry->handle, __entry->sc,
		__entry->mpm_tv)
);

TRACE_EVENT(fastrpc_internal_invoke_end,

	TP_PROTO(uint32_t handle, uint32_t sc, s64 seq_num),

	TP_ARGS(handle, sc, seq_num),

	TP_STRUCT__entry(
		__field(uint32_t, handle)
		__field(uint32_t, sc)
		__field(s64, seq_num)
		__field(unsigned long long, mpm_tv)
	),

	TP_fast_assign(
		__entry->handle = handle;
		__entry->sc = sc;
		__entry->seq_num = seq_num;
		__entry->mpm_tv = msm_hr_timer_get_sclk_ticks();
	),

	TP_printk("%lld:0x%x:0x%x:%llu", __entry->seq_num,
		__entry->handle, __entry->sc,
		__entry->mpm_tv)
);

#endif /* _TRACE_VIRTIO_FASTRPC_H */

/* This part must be outside protection */
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .
#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE virtio_fastrpc_trace

#include <trace/define_trace.h>
