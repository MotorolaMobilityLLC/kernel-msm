/*
 * arch/arm/mach-msm/include/mach/board_lge.h
 *
 * Copyright (C) 2012,2013 LGE, Inc
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

typedef enum {
	HW_REV_EVB1 = 0,
	HW_REV_EVB2,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_E,
	HW_REV_F,
	HW_REV_G,
	HW_REV_H,
	HW_REV_1_0,
	HW_REV_1_1,
	HW_REV_1_2,
	HW_REV_MAX
} hw_rev_type;

extern char *rev_str[];

hw_rev_type lge_get_board_revno(void);

enum lge_boot_mode_type {
	LGE_BOOT_MODE_NORMAL = 0,
	LGE_BOOT_MODE_CHARGER,
	LGE_BOOT_MODE_CHARGERLOGO,
	LGE_BOOT_MODE_FACTORY,
	LGE_BOOT_MODE_FACTORY2,
	LGE_BOOT_MODE_PIFBOOT
};

enum lge_boot_mode_type lge_get_boot_mode(void);

#define UART_MODE_ALWAYS_OFF_BMSK   BIT(0)
#define UART_MODE_ALWAYS_ON_BMSK    BIT(1)
#define UART_MODE_INIT_BMSK         BIT(2)
#define UART_MODE_EN_BMSK           BIT(3)

extern unsigned int lge_get_uart_mode(void);
extern void lge_set_uart_mode(unsigned int um);

void __init lge_reserve(void);
void __init lge_add_persistent_device(void);

#endif
