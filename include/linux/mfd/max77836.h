/*
 * max77836.h - Driver for the Maxim 77836
 *
 * Copyright (C) 2011 Samsung Electrnoics
 * SeungJin Hahn <sjin.hahn@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * This driver is based on max8997.h
 *
 * MAX77836 has MUIC, Charger devices.
 * The devices share the same I2C bus and interrupt line
 * included in this mfd driver.
 */

#ifndef __MAX77836_H__
#define __MAX77836_H__

#include <linux/regulator/consumer.h>
#if defined(CONFIG_CHARGER_MAX77836)
#include <linux/power/max77836_charger.h>
#endif
#ifdef CONFIG_REGULATOR_MAX77836
#include <linux/regulator/max77836-regulator.h>
#endif

#define MFD_DEV_NAME "max77836"

struct max77836_platform_data {
	/* IRQ */
	int irq_base;
	int irq_gpio;
	u32 irq_gpio_flags;
	bool wakeup;

	/* current control GPIOs */
	int gpio_pogo_vbatt_en;
	int gpio_pogo_vbus_en;

	/* current control GPIO control function */
	int (*set_gpio_pogo_vbatt_en) (int gpio_val);
	int (*set_gpio_pogo_vbus_en) (int gpio_val);

	int (*set_gpio_pogo_cb) (int new_dev);

	/* max77836 internal API function */
	int (*set_cdetctrl1_reg) (struct i2c_client *i2c, u8 val);
	int (*get_cdetctrl1_reg) (struct i2c_client *i2c, u8 *val);
	int (*set_control2_reg) (struct i2c_client *i2c, u8 val);
	int (*get_control2_reg) (struct i2c_client *i2c, u8 *val);

#ifdef CONFIG_REGULATOR_MAX77836
	int num_regulators;
	struct max77836_reg_platform_data *regulators;
	int *opmode_data;
#endif /* CONFIG_REGULATOR_MAX77836 */
};

#ifdef CONFIG_REGULATOR_MAX77836
extern struct max77836_reg_platform_data max77836_reglator_pdata[];
#endif

#endif /* __MAX77836_H__ */
