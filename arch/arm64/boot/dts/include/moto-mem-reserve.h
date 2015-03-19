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

#define WDOG_CPUCTX_SIZE_PERCPU	0x5800 /* sizeof(struct msm_wdog_cpuctx) */
#define WDOG_CPUCTX_SIZE_4CPUS	(WDOG_CPUCTX_SIZE_PERCPU * 4)

#define WDOG_CPUCTX_END_ADDR	0xaf000000

#define WDOG_CPUCTX_SIZE_8996	WDOG_CPUCTX_SIZE_4CPUS
#define WDOG_CPUCTX_BASE_8996	(WDOG_CPUCTX_END_ADDR - WDOG_CPUCTX_SIZE_4CPUS)

#define TZLOG_BCK_SIZE		0x2000
#define TZLOG_BCK_BASE_8996	(WDOG_CPUCTX_BASE_8996 - TZLOG_BCK_SIZE)

#endif /* __MOTO_MEM_RESERVE_H */
