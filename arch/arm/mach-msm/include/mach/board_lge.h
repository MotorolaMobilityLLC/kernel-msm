/*
 * arch/arm/mach-msm/include/mach/board_lge.h
 *
 * Copyright (C) 2012,2013 LGE Inc.
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
#ifndef __ARCH_MSM_BOARD_LGE_H
#define __ARCH_MSM_BOARD_LGE_H

enum {
	HW_REV_UNKNOWN = 0,
	HW_REV_EVB1,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_1_0,
	HW_REV_1_1
};

int lge_get_board_revno(void);

#ifdef CONFIG_PSTORE_RAM
#define LGE_RAM_CONSOLE_SIZE (128 * SZ_1K * 2)
#define LGE_PERSISTENT_RAM_SIZE (SZ_1M)

void __init lge_reserve(void);
void __init lge_add_persistent_device(void);
#endif

int lge_uart_console_enabled(void);

#endif
