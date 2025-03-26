/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM swapfile
#undef TRACE_INCLUDE_PATH

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_SWAPFILE_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_SWAPFILE_H

#include <trace/hooks/vendor_hooks.h>

DECLARE_HOOK(android_vh_get_swap_pages_bypass,
	TP_PROTO(struct swap_info_struct *si, int entry_order, bool *skip),
	TP_ARGS(si, entry_order, skip));

#endif /* _TRACE_HOOK_SWAPFILE_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
