/*
 * Copyright (C) 2014 Motorola Mobility LLC
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
#ifndef __WATCHDOG_CPU_CTX_H
#define __WATCHDOG_CPU_CTX_H

/*  */
#define MAX_CPU_CTX_SIZE	2048

#ifdef CONFIG_MSM_WATCHDOG_CTX_PRINT
void msm_wdog_get_cpu_ctx(struct platform_device *pdev,
		phys_addr_t *cpu_ctx_addr, size_t *cpu_ctx_size_percpu);

#else
static inline void msm_wdog_get_cpu_ctx(struct platform_device *pdev,
		phys_addr_t *cpu_ctx_addr, size_t *cpu_ctx_size_percpu)
{
	if (cpu_ctx_addr)
		*cpu_ctx_addr = 0;
	if (cpu_ctx_size_percpu)
		*cpu_ctx_size_percpu = 0;
}
#endif
#endif /* __WATCHDOG_CPU_CTX_H */
