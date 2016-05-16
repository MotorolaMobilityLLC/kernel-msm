/* include/soc/qcom/sharp/shrlog.h
 *
 * Copyright (C) 2010- Sharp Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _SHRLOG_H_
#define _SHRLOG_H_

#define SHRLOG_FIXED_MAGIC_NUM (0x88990011)
#define SHRLOG_FIXED_LINUX_MAGIC_NUM (0x88992211)

#if defined(CONFIG_ARM64)
typedef unsigned long shrlog_uint64;
#elif defined(CONFIG_ARM)
typedef unsigned long long shrlog_uint64;
#else
#error "types in linux not specified."
#endif

struct shrlog_ram_fixed_t {
	shrlog_uint64 shrlog_ram_fixed_addr;
	shrlog_uint64 magic_num1;
	shrlog_uint64 modem_region_start;
	shrlog_uint64 modem_region_size;
	shrlog_uint64 linux_phys_offset;
	shrlog_uint64 pad[2];
	shrlog_uint64 magic_num2;
};

typedef unsigned long shrlog_kernel_t;

typedef struct
{
	shrlog_kernel_t xtime_sec_addr;
	shrlog_kernel_t _text_addr;
	shrlog_kernel_t _stext_addr;
	shrlog_kernel_t _etext_addr;
} shrlog_fixed_linux_addresses_t;

typedef struct {
	shrlog_kernel_t info_size;
	union {
		shrlog_fixed_linux_addresses_t st;
		shrlog_kernel_t values[sizeof(shrlog_fixed_linux_addresses_t)/sizeof(shrlog_kernel_t)];
	} adr;
	shrlog_kernel_t size_of_task_struct;
	shrlog_kernel_t size_of_thread_struct;
	shrlog_kernel_t size_of_mm_struct;
	shrlog_kernel_t stack_offset;
	shrlog_kernel_t tasks_offset;
	shrlog_kernel_t pid_offset;
	shrlog_kernel_t thread_group_offset;
	shrlog_kernel_t comm_offset;
	shrlog_kernel_t mm_offset;
	shrlog_kernel_t pgd_offset;
#ifdef CONFIG_ARM64
	shrlog_kernel_t size_of_cpu_context;
	shrlog_kernel_t cpu_context_fp_offset;
	shrlog_kernel_t cpu_context_sp_offset;
	shrlog_kernel_t cpu_context_pc_offset;
#endif /* CONFIG_ARM64 */
#ifdef CONFIG_ARM
	shrlog_kernel_t size_of_thread_info;
	shrlog_kernel_t cpu_context_in_thread_info;
	shrlog_kernel_t size_of_cpu_context_save;
	shrlog_kernel_t cpu_context_save_fp_offset;
	shrlog_kernel_t cpu_context_save_sp_offset;
	shrlog_kernel_t cpu_context_save_pc_offset;
#endif /* CONFIG_ARM */

#if defined(CONFIG_SHARP_SHLOG_RAMOOPS)
	shrlog_kernel_t oops_addr;
	shrlog_kernel_t oops_size;
	shrlog_kernel_t oops_console_size;
	shrlog_kernel_t oops_record_size;
	shrlog_kernel_t oops_pmsg_size;
#endif /* CONFIG_SHARP_SHLOG_RAMOOPS */

	shrlog_kernel_t info_magic;
} shrlog_fixed_apps_info;

#endif /* _SHRLOG_H_ */
