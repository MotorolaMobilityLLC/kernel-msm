/*
* Copyright(c) 2012, LG Electronics Inc. All rights reserved.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
*/

#ifndef __BATTERY_TEMP_CTRL_H
#define __BATTERY_TEMP_CTRL_H

struct batt_temp_pdata {
	int (*set_chg_i_limit)(int max_current);
	int (*get_chg_i_limit)(void);
	int (*set_health_state)(int state, int i_value);
	int (*enable_charging)(void);
	int (*disable_charging)(void);
	int (*is_ext_power)(void);
	int update_time;
	int *temp_level;
	int temp_nums;
	int thr_mvolt;
	int i_decrease;
	int i_restore;
};

enum {
	TEMP_LEVEL_POWER_OFF = 0,
	TEMP_LEVEL_WARNINGOVERHEAT,
	TEMP_LEVEL_HOT_STOPCHARGING,
	TEMP_LEVEL_DECREASING,
	TEMP_LEVEL_HOT_RECHARGING,
	TEMP_LEVEL_COLD_RECHARGING,
	TEMP_LEVEL_WARNING_COLD,
	TEMP_LEVEL_COLD_STOPCHARGING,
	TEMP_LEVEL_NORMAL
};
#endif
