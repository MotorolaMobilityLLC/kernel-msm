/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM dmabuf

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_DMA_BUF_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_DMA_BUF_H

struct dma_buf;

#include <trace/hooks/vendor_hooks.h>

DECLARE_HOOK(android_vh_ignore_dmabuf_vmap_bounds,
	     TP_PROTO(struct dma_buf *dma_buf, bool *ignore_bounds),
	     TP_ARGS(dma_buf, ignore_bounds));
DECLARE_HOOK(android_vh_dma_heap_buffer_alloc_lat_start,
	TP_PROTO(unsigned long long *stime),
	TP_ARGS(stime));
DECLARE_HOOK(android_vh_dma_heap_buffer_alloc_lat_end,
	TP_PROTO(unsigned long long stime, size_t len, struct dma_buf *dma_buf),
	TP_ARGS(stime, len, dma_buf));

#endif /* _TRACE_HOOK_DMA_BUF_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
