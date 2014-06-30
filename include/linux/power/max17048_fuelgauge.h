/*
 * max17048_fuelgauge.h
 * Samsung MAX17048 Fuel Gauge Header
 *
 * Copyright (C) 2014 Samsung Electronics, Inc.
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

#ifndef __MAX17048_FUELGAUGE_H
#define __MAX17048_FUELGAUGE_H __FILE__

#include <linux/power_supply.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/slab.h>
#include <linux/module.h>

/* Slave address should be shifted to the right 1bit.
 * R/W bit should NOT be included.
 */
#define SEC_FUELGAUGE_I2C_SLAVEADDR (0x6D >> 1)

#define MAX17048_VCELL_MSB	0x02
#define MAX17048_VCELL_LSB	0x03
#define MAX17048_SOC_MSB	0x04
#define MAX17048_SOC_LSB	0x05
#define MAX17048_MODE_MSB	0x06
#define MAX17048_MODE_LSB	0x07
#define MAX17048_VER_MSB	0x08
#define MAX17048_VER_LSB	0x09
#define MAX17048_HIBERNATE	0x0A
#define MAX17048_RCOMP_MSB	0x0C
#define MAX17048_RCOMP_LSB	0x0D
#define MAX17048_OCV_MSB	0x0E
#define MAX17048_OCV_LSB	0x0F
#define MAX17048_CMD_MSB	0xFE
#define MAX17048_CMD_LSB	0xFF

#define RCOMP_DISCHARGING_THRESHOLD	20

struct max17048_fuelgauge_platform_data {
	int rcomp_charging;
	int rcomp_discharging;
	int temp_cohot;
	int temp_cocold;
	int fuel_alert_soc;
	int fg_irq;
	bool is_using_model_data;
	u8 *type_str;
	/* fuel alert can be repeated */
	bool repeated_fuelalert;
};
struct max17048_fuelgauge_data {
	struct i2c_client		*client;
	struct max17048_fuelgauge_platform_data *pdata;
	struct power_supply		psy_fg;
	struct delayed_work isr_work;
	struct wake_lock fuel_alert_wake_lock;
	bool is_fuel_alerted;
	struct mutex fg_lock;
	int fg_irq;

	int cable_type;
};

#endif /* __MAX17048_FUELGAUGE_H */
