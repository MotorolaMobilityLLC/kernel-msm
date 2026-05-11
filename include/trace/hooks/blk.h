/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM blk

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_BLK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_BLK_H

#include <trace/hooks/vendor_hooks.h>

struct path;
struct vfsmount;

DECLARE_HOOK(android_vh_do_new_mount_fc,
	TP_PROTO(struct path *mountpoint, struct vfsmount *mnt),
	TP_ARGS(mountpoint, mnt));

#endif /* _TRACE_HOOK_BLK_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
