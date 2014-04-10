/*
 *  board-crepe-battery.c
 *
 * Copyright (C) 2014 Samsung Electronics
 *
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

#include <linux/platform_data/android_battery.h>

struct android_bat_platform_data android_battery_pdata;
extern int current_cable_type;

static int crepe_bat_get_temperature(int * value) {
	return 0;
}

static void crepe_bat_set_charging_enable(int cable_type) {
	union power_supply_propval value;

	value.intval = cable_type;

	psy_do_property(android_battery_pdata.charger_name, set,
			POWER_SUPPLY_PROP_ONLINE, value);
}

void crepe_bat_initial_check(void) {
	union power_supply_propval value;

	pr_info("%s : current_cable_type : (%d)\n", __func__, current_cable_type);
	if (CHARGE_SOURCE_NONE != current_cable_type) {
		value.intval = current_cable_type;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
	}
}

struct android_bat_platform_data android_battery_pdata = {
	.initial_check
		= crepe_bat_initial_check,
	.set_charging_enable
		= crepe_bat_set_charging_enable,
	.get_temperature
		= crepe_bat_get_temperature,
};

MODULE_DESCRIPTION("Crepe board battery file");
MODULE_LICENSE("GPL");
