/*
 *  android_battery.h
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LINUX_ANDROID_BATTERY_H
#define _LINUX_ANDROID_BATTERY_H
#include <linux/types.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/of.h>

enum {
	CHARGE_SOURCE_NONE = 0,
	CHARGE_SOURCE_AC,
	CHARGE_SOURCE_USB,
};

struct android_bat_data;
struct android_bat_callbacks {
	void (*charge_source_changed)
		(struct android_bat_callbacks *, int);
	void (*battery_set_full)(struct android_bat_callbacks *);
};

struct android_bat_platform_data {
	void (*register_callbacks)(struct android_bat_callbacks *);
	void (*unregister_callbacks)(void);
	void (*set_charging_current) (int);
	void (*set_charging_enable) (int);
	int (*poll_charge_source) (struct android_bat_data *);
	int (*get_capacity) (void);
	void (*get_temperature) (struct android_bat_data *, int *);
	int (*get_voltage_now)(void);
	int (*get_current_now)(int *);
	void (*initial_check)(void);
	void (*init_adc)(struct android_bat_data *);
	int (*is_poweroff_charging)(void);

	char * charger_name;
	char * fuelgauge_name;
	int temp_high_threshold;
	int temp_high_recovery;
	int temp_low_recovery;
	int temp_low_threshold;

	unsigned long full_charging_time;
	unsigned long recharging_time;
	unsigned int recharging_voltage;
};

struct android_bat_data {
	struct android_bat_platform_data *pdata;
	struct android_bat_callbacks callbacks;

	struct device		*dev;

	struct power_supply	psy_bat;
	struct power_supply	psy_ac;
	struct power_supply	psy_usb;

	struct wake_lock	monitor_wake_lock;
	struct wake_lock	charger_wake_lock;

	int			charge_source;

	int			batt_temp;
	int			temp_adc;
	int			batt_current;
	unsigned int		batt_health;
	unsigned int		batt_vcell;
	unsigned int		batt_soc;
	unsigned int		charging_status;
	bool			recharging;
	unsigned long		charging_start_time;

	struct workqueue_struct *monitor_wqueue;
	struct work_struct	monitor_work;
	struct work_struct	charger_work;

	ktime_t			last_poll;

	struct dentry		*debugfs_entry;
};

static inline struct power_supply *get_power_supply_by_name(char *name)
{
	if (!name)
		return (struct power_supply *)NULL;
	else
		return power_supply_get_by_name(name);
}

#define psy_do_property(name, function, property, value) \
{	\
	struct power_supply *psy;	\
	int ret;	\
	psy = get_power_supply_by_name((name));	\
	if (!psy) {	\
		pr_err("%s: Fail to "#function" psy (%s)\n",	\
			__func__, (name));	\
		value.intval = 0;	\
	} else {	\
		if (psy->function##_property != NULL) { \
			ret = psy->function##_property(psy, (property), &(value)); \
			if (ret < 0) {	\
				pr_err("%s: Fail to %s "#function" (%d=>%d)\n", \
						__func__, name, (property), ret);	\
				value.intval = 0;	\
			}	\
		}	\
	}	\
}
#endif
