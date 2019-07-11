/*
 * TI LM3697 Backlight
 *
 * Copyright 2014 Texas Instruments
 *
 * Author: Milo Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LM3697_BL_H__
#define __LM3697_BL_H__

#include <linux/regmap.h>

enum lm3697_max_current {
	LM3697_IMAX_5mA,
	LM3697_IMAX_6mA,
	LM3697_IMAX_7mA = 0x03,
	LM3697_IMAX_8mA,
	LM3697_IMAX_9mA,
	LM3697_IMAX_10mA = 0x07,
	LM3697_IMAX_11mA,
	LM3697_IMAX_12mA,
	LM3697_IMAX_13mA,
	LM3697_IMAX_14mA,
	LM3697_IMAX_15mA = 0x0D,
	LM3697_IMAX_16mA,
	LM3697_IMAX_17mA,
	LM3697_IMAX_18mA,
	LM3697_IMAX_19mA,
	LM3697_IMAX_20mA = 0x13,
	LM3697_IMAX_21mA,
	LM3697_IMAX_22mA,
	LM3697_IMAX_23mA = 0x17,
	LM3697_IMAX_24mA,
	LM3697_IMAX_25mA,
	LM3697_IMAX_26mA,
	LM3697_IMAX_27mA = 0x1C,
	LM3697_IMAX_28mA,
	LM3697_IMAX_29mA,
	LM3697_IMAX_30mA,
};

/*
 * struct lm3633_backlight_platform_data
 * @name: Backlight device name
 * @bl_string: Bit mask of backlight output string
 * @imax: Max current for backlight output string
 * @init_brightness: Initial brightness value
 * @pwm_period: Platform specific PWM period value. unit is nano
 */
struct lm3697_backlight_platform_data {
	const char *name;

	unsigned long bl_string;	/* bit OR mask of LM3697_HVLEDx */
#define LM3697_HVLED1		BIT(0)
#define LM3697_HVLED2		BIT(1)
#define LM3697_HVLED3		BIT(2)

	enum lm3697_max_current imax;
	unsigned int init_brightness;
	bool pwm_enable;

	/* Only valid in case of PWM mode */
	unsigned int pwm_period;
};

/*
 * struct lm3697_platform_data
 * @en_gpio: GPIO for HWEN pin
 * @bl_pdata: Backlight platform data
 * @num_backlights: Number of backlight outputs
 */
struct lm3697_platform_data {
	int en_gpio;
	struct lm3697_backlight_platform_data *bl_pdata;
	int num_backlights;
};

#endif
