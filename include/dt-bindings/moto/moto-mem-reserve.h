/*
 * Copyright (C) 2016 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __MOTO_MEM_RESERVE_H
#define __MOTO_MEM_RESERVE_H

#define RAMOOPS_CONSOLE_SIZE	0x40000
#define RAMOOPS_PMSG_SIZE	0x40000
#define RAMOOPS_RECORD_SIZE	0x3f800
#define RAMOOPS_SIZE		(RAMOOPS_CONSOLE_SIZE + RAMOOPS_PMSG_SIZE + RAMOOPS_RECORD_SIZE)
#define RAMOOPS_BASE_ADDR	0xaf000000

#define WDOG_CPUCTX_SIZE_PERCPU	0x6400 /* sizeof(struct msm_wdog_cpuctx) */
#define WDOG_CPUCTX_SIZE_8CPUS	(WDOG_CPUCTX_SIZE_PERCPU * 8)
#define WDOG_CPUCTX_SIZE_4CPUS	(WDOG_CPUCTX_SIZE_PERCPU * 4)

#define WDOG_CPUCTX_END_ADDR	RAMOOPS_BASE_ADDR

#define WDOG_CPUCTX_SIZE	WDOG_CPUCTX_SIZE_8CPUS
#define WDOG_CPUCTX_BASE	(WDOG_CPUCTX_END_ADDR - WDOG_CPUCTX_SIZE_8CPUS)

#define TZLOG_BCK_SIZE		0x30000
#define TZLOG_BCK_BASE		(WDOG_CPUCTX_BASE - TZLOG_BCK_SIZE)

#define MMI_ANNOTATE_SIZE	0x800
#define MMI_ANNOTATE_BASE	(TZLOG_BCK_BASE - MMI_ANNOTATE_SIZE)

#endif /* __MOTO_MEM_RESERVE_H */
