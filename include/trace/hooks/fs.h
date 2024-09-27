/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM fs

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_FS_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_FS_H

#include <trace/hooks/vendor_hooks.h>

DECLARE_RESTRICTED_HOOK(android_rvh_ksys_umount,
		TP_PROTO(char __user *name, int flags),
		TP_ARGS(name, flags), 1);
DECLARE_HOOK(android_vh_f2fs_file_open,
	TP_PROTO(struct inode *inode, struct file *filp),
	TP_ARGS(inode, filp));

#endif /* _TRACE_HOOK_FS_H */

/* This part must be outside protection */
#include <trace/define_trace.h>
