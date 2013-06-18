/*
 * arch/arm/mach-msm/include/mach/mmi_watchdog.h: include file for watchdog
 *
 * Copyright (C) 2015 Motorola Mobility LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _ASM_ARCH_MMI_WATCHDOG_H_
#define _ASM_ARCH_MMI_WATCHDOG_H_

#ifdef CONFIG_MSM_WATCHDOG_V2
void msm_trigger_wdog_bite(void);
void g_pet_watchdog(void);
#define pet_watchdog(void) g_pet_watchdog(void);
#else
static inline void msm_trigger_wdog_bite(void) { }
static inline void pet_watchdog(void) { }
#endif

static inline void touch_hw_watchdog(void)
{
	pet_watchdog();
}

#endif
