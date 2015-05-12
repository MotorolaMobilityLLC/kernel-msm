/*
 * Copyright (C) 2015 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/const.h>
#include <asm/page.h>

#if defined(CONFIG_ARCH_THREAD_INFO_ALLOCATOR)

	.macro  adjust_sp_to_page1, rs
	mov     \rs, sp
	ands	\rs, \rs, #((THREAD_SIZE - 1) ^ (PAGE_SIZE - 1))
	b.eq	adj
	mov     \rs, sp
	and     \rs, \rs, #~(THREAD_SIZE - 1)   // thread_info
	add	\rs, \rs, #PAGE_SIZE		// start of second page
	sub	\rs, sp, \rs
	sub	sp, sp, \rs
adj:	str	\rs, [sp, #-16]!
	.endm

	.macro  restore_sp_from_page1, rs
	ldr	\rs, [sp], #16
	add	sp, sp, \rs
	.endm

#else

	.macro  adjust_sp_to_page1, rs
	.endm

	.macro  restore_sp_from_page1, rs
	.endm

#endif
