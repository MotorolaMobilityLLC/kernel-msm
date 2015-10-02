/*
 * Copyright(c) 2015, LGE Inc. All rights reserved.
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

#ifndef __REAR_TM_H
#define __REAR_TM_H

#define REAR_TM_POWEROFF_TEMP   61

#ifdef CONFIG_REAR_TEMP_MONITOR
bool rear_tm_is_poweroff(void);
#else
static inline bool rear_tm_is_poweroff(void)
{
	return false;
}
#endif

#endif
