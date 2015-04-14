/* Copyright (c) 2015, LGE Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_MSM_BOARD_LGE_H
#define __ASM_ARCH_MSM_BOARD_LGE_H

enum hw_rev_type {
	HW_REV_EVB1 = 0,
	HW_REV_EVB2,
	HW_REV_EVB3,
	HW_REV_0,
	HW_REV_A,
	HW_REV_B,
	HW_REV_C,
	HW_REV_D,
	HW_REV_E,
	HW_REV_F,
	HW_REV_G,
	HW_REV_1_0,
	HW_REV_1_1,
	HW_REV_1_2,
	HW_REV_MAX
};

extern char *rev_str[];

enum hw_rev_type lge_get_board_revno(void);

enum lge_laf_mode_type {
	LGE_LAF_MODE_NORMAL = 0,
	LGE_LAF_MODE_LAF,
};

enum lge_laf_mode_type lge_get_laf_mode(void);

int lge_get_android_dlcomplete(void);
int get_lge_frst_status(void);

extern int lge_get_bootreason(void);
#endif
