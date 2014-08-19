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
#ifndef __ARCH_ARM_MACH_MSM_MMI_CLOCK_H
#define __ARCH_ARM_MACH_MSM_MMI_CLOCK_H

extern struct rcg_clk mclk1_clk_src;
extern struct branch_clk camss_mclk1_clk;

#ifndef CONFIG_MMI_MSM_CLOCK
static inline int mmi_msm_clock_init(struct clock_init_data *data) { return 0; }
#else
extern int mmi_msm_clock_init(struct clock_init_data *data);
#endif

#endif
