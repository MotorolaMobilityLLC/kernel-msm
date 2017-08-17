#ifndef __PCAL6416_H__
#define __PCAL6416_H__
/*
** =============================================================================
** Copyright (c)2017  Motorola Mobility LLC.
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** File:
**     gpio-pcal6416.h
**
** Description:
**     Header file for gpio-pcal6416.c
**
** =============================================================================
*/

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/i2c.h>
#include <linux/gpio.h>

#define PCAL_DEVICE_NAME "pcal6416"

#define	PCAL6416_REG_ID					0x00

#define	PCAL6416_REG_INPUT_VAL			0x00
#define	PCAL6416_REG_OUTPUT_VAL			0x02
#define	PCAL6416_REG_CONFIG				0x06
#define	PCAL6416_REG_OUTPUT_DRV			0x40
#define	PCAL6416_REG_INPUT_LATCH			0x44
#define	PCAL6416_REG_PULL_EN				0x46
#define	PCAL6416_REG_PULL_SEL				0x48
#define	PCAL6416_REG_INT_MASK			0x4A
#define	PCAL6416_REG_INT_STATUS			0x4C
#define	PCAL6416_REG_OUTPUT_CONTROL	0x4F

/*pcal6416 has 2 group ios(p0.0-p0.7 and p1.0-p1.7), and its register is in sequence. */
/*	e.g. input_value _0's register is 0x00, and input_value_1's register is 0x01*/
#define REGISTER_BIT_NUM		8
#define FIND_IO_GROUP(pin)		((pin) / REGISTER_BIT_NUM)
#define FIND_IO_BIT(pin)			((pin) % REGISTER_BIT_NUM)
#define IO_REGISTER(reg, pin)		((reg) + FIND_IO_GROUP(pin))

struct pcal6416_data {
	struct i2c_client *i2c_client;
	struct device *dev;
	struct regmap *mpRegmap;
	struct gpio_chip	gpio_chip;
	unsigned char *all_gpio_config;
	unsigned char chip_registered;
	int gpio_num;
	int reset_gpio;
};

enum GPIO_DIRECTION {
	GPIO_OUT = 0,
	GPIO_IN
};

#endif
