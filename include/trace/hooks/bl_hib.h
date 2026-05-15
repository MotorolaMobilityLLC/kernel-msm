/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM bl_hib

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_BL_HIB_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_BL_HIB_H

#include <trace/hooks/vendor_hooks.h>

struct block_device;

#ifndef __TRACE_HOOK_BL_HIB_RESUME_STATE
#define __TRACE_HOOK_BL_HIB_RESUME_STATE
enum hibernate_resume_state {
	HIBERNATION_IMAGE_RESTORE_START,
	HIBERNATION_DEVICE_RESUME_START,
	HIBERNATION_DEVICE_RESUME_DONE,
	HIBERNATION_IMAGE_RESTORE_DONE,
	HIBERNATION_EXIT,
};
#endif /*__TRACE_HOOK_BL_HIB_RESUME_STATE__*/

DECLARE_HOOK(android_vh_check_hibernation_swap,
	TP_PROTO(struct block_device *resume_block, bool *hib_swap),
	TP_ARGS(resume_block, hib_swap));

DECLARE_HOOK(android_vh_save_cpu_resume,
	TP_PROTO(u64 *addr, u64 phys_addr),
	TP_ARGS(addr, phys_addr));

DECLARE_HOOK(android_vh_save_hib_resume_bdev,
	TP_PROTO(struct block_device *hib_resume_bdev),
	TP_ARGS(hib_resume_bdev));

DECLARE_HOOK(android_vh_encrypt_page,
	TP_PROTO(void *buf),
	TP_ARGS(buf));

DECLARE_HOOK(android_vh_init_aes_encrypt,
	TP_PROTO(void *unused),
	TP_ARGS(unused));

DECLARE_HOOK(android_vh_skip_swap_map_write,
	TP_PROTO(bool *skip),
	TP_ARGS(skip));

DECLARE_HOOK(android_vh_post_image_save,
	TP_PROTO(unsigned short root_swap),
	TP_ARGS(root_swap));

DECLARE_HOOK(android_vh_hibernated_do_mem_alloc,
        TP_PROTO(unsigned long nr_pages, unsigned int swsusp_header_flags,
		 int *ret),
        TP_ARGS(nr_pages, swsusp_header_flags, ret));

DECLARE_HOOK(android_vh_hibernate_save_cmp_len,
        TP_PROTO(size_t cmp_len),
        TP_ARGS(cmp_len));

DECLARE_HOOK(android_vh_hibernate_resume_state,
        TP_PROTO(enum hibernate_resume_state state),
        TP_ARGS(state));

#endif /* _TRACE_HOOK_S2D_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
