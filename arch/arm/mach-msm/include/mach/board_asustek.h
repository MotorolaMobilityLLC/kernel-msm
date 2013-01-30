/* arch/arm/mach-msm/include/mach/board_asustek.h
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2012, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2012, LGE Inc.
 * Copyright (c) 2012-2013, ASUSTek Computer Inc.
 * Author: Paris Yeh <paris_yeh@asus.com>
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

#ifndef __ASM_ARCH_MSM_BOARD_ASUSTEK_H
#define __ASM_ARCH_MSM_BOARD_ASUSTEK_H

#ifdef CONFIG_ANDROID_PERSISTENT_RAM
#define ASUSTEK_PERSISTENT_RAM_SIZE	(SZ_1M)
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE
#define ASUSTEK_RAM_CONSOLE_SIZE	(124 * SZ_1K * 2)
#endif

void __init asustek_reserve(void);

#ifdef CONFIG_ANDROID_PERSISTENT_RAM
void __init asustek_add_persistent_ram(void);
#else
static inline void __init asustek_add_persistent_ram(void)
{
	/* empty */
}
#endif

#ifdef CONFIG_ANDROID_RAM_CONSOLE
void __init asustek_add_ramconsole_devices(void);
#else
static inline void __init asustek_add_ramconsole_devices(void)
{
	/* empty */
}
#endif

#endif // __ASM_ARCH_MSM_BOARD_ASUSTEK_H
