/*
 * arch/arm/mach-msm/include/mach/mmi_watchdog.h: include file for watchdog
 *
 * Copyright (C) 2012 Motorola Mobility LLC.
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

#include "../../msm_watchdog.h"

static inline void touch_hw_watchdog(void)
{
	pet_watchdog();
}

static inline void trigger_watchdog_reset(void)
{
	msm_watchdog_reset(0);
}
static inline void panic_watchdog_set(unsigned int t)
{
	msm_panic_wdt_set(t);
}
