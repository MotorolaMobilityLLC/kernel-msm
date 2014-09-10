/* include/linux/board_asustek.h
 *
 * Copyright (c) 2012-2014, ASUSTek Computer Inc.
 * Author: Paris Yeh <paris_yeh@asus.com>
 *	   Hank Lee  <hank_lee@asus.com>
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

#ifndef _INTEL_MID_BOARD_ASUSTEK_H
#define _INTEL_MID_BOARD_ASUSTEK_H

typedef enum {
	HW_REV_INVALID = -1,
	HW_REV_A = 0,
	HW_REV_B = 1,
	HW_REV_C = 2,
	HW_REV_D = 3,
	HW_REV_E = 4,
	HW_REV_F = 5,
	HW_REV_G = 6,
	HW_REV_H = 7,
	HW_REV_MAX = 8
} hw_rev;

hw_rev asustek_get_hw_rev(void);

#endif /* _INTEL_MID_BOARD_ASUSTEK_H */
