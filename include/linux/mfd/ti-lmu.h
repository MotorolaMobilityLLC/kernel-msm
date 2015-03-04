/*
 * TI LMU(Lighting Management Unit) Devices
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

#ifndef __MFD_TI_LMU_H__
#define __MFD_TI_LMU_H__

#include <linux/gpio.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#define LM3631_NUM_REGULATORS			5

enum ti_lmu_id {
	LM3532,
	LM3631,
	LM3633,
	LM3695,
	LM3697,
};

enum ti_lmu_max_current {
	LMU_IMAX_5mA,
	LMU_IMAX_6mA,
	LMU_IMAX_7mA = 0x03,
	LMU_IMAX_8mA,
	LMU_IMAX_9mA,
	LMU_IMAX_10mA = 0x07,
	LMU_IMAX_11mA,
	LMU_IMAX_12mA,
	LMU_IMAX_13mA,
	LMU_IMAX_14mA,
	LMU_IMAX_15mA = 0x0D,
	LMU_IMAX_16mA,
	LMU_IMAX_17mA,
	LMU_IMAX_18mA,
	LMU_IMAX_19mA,
	LMU_IMAX_20mA = 0x13,
	LMU_IMAX_21mA,
	LMU_IMAX_22mA,
	LMU_IMAX_23mA = 0x17,
	LMU_IMAX_24mA,
	LMU_IMAX_25mA,
	LMU_IMAX_26mA,
	LMU_IMAX_27mA = 0x1C,
	LMU_IMAX_28mA,
	LMU_IMAX_29mA,
	LMU_IMAX_30mA,
};

/*
 * struct lm3633_bl_platform_data
 * @name: Backlight device name
 * @bl_string: Bit mask of backlight output string
 * @imax: Max current for backlight output string
 * @init_brightness: Initial brightness value
 * @default_on: apply initial brightness on power up
 * @ramp_up_ms: Backlight light effect for ramp up or slope rate
 * @ramp_down_ms: Backlight light effect for ramp down rate
 * @pwm_period: Platform specific PWM period value. unit is nano
 */
struct ti_lmu_backlight_platform_data {
	const char *name;

	unsigned long bl_string;	/* bit OR mask of LMU_HVLEDx */
#define LMU_HVLED1		BIT(0)
#define LMU_HVLED2		BIT(1)
#define LMU_HVLED3		BIT(2)

	enum ti_lmu_max_current imax;
	u8 init_brightness;
	bool default_on;

	/* Used for light effect */
	unsigned int ramp_up_ms;
	unsigned int ramp_down_ms;

	/* Only valid in case of PWM mode */
	unsigned int pwm_period;
};

/*
 * struct lmu_led_platform_data
 * @name: LED channel name
 * @led_string: Bit mask of LED output string
 * @imax: LED max current
 */
struct ti_lmu_led_platform_data {
	const char *name;

	unsigned long led_string;	/* bit OR mask of LMU_LVLEDx */;
#define LMU_LVLED1		BIT(0)
#define LMU_LVLED2		BIT(1)
#define LMU_LVLED3		BIT(2)
#define LMU_LVLED4		BIT(3)
#define LMU_LVLED5		BIT(4)
#define LMU_LVLED6		BIT(5)

	enum ti_lmu_max_current imax;
};

/*
 * struct lmu_platform_data
 * @en_gpio: GPIO for HWEN pin
 * @bl_pdata: Backlight platform data
 * @num_backlights: Number of backlight outputs
 * @led_pdata: LED platform data
 * @num_leds: Number of LED outputs
 * @regulator_data: Regulator init data
 */
struct ti_lmu_platform_data {
	int en_gpio;

	/* Backlight */
	struct ti_lmu_backlight_platform_data *bl_pdata;
	int num_backlights;

	/* LEDs */
	struct ti_lmu_led_platform_data *led_pdata;
	int num_leds;

	/* Regulators of LM3631 */
	struct regulator_init_data *regulator_data[LM3631_NUM_REGULATORS];

	/* MMI I2C swicth regulator parameters*/
	const char *supply_name;
	struct regulator *vreg;
};

/*
 * struct ti_lmu
 * @id: Chip ID
 * @dev: Parent device pointer
 * @regmap: Used for i2c communcation on accessing registers
 * @pdata: LMU platform specific data
 */
struct ti_lmu {
	int id;
	struct device *dev;
	struct regmap *regmap;
	struct ti_lmu_platform_data *pdata;
};

int ti_lmu_read_byte(struct ti_lmu *lmu, u8 reg, u8 *read);
int ti_lmu_write_byte(struct ti_lmu *lmu, u8 reg, u8 data);
int ti_lmu_update_bits(struct ti_lmu *lmu, u8 reg, u8 mask, u8 data);
enum ti_lmu_max_current ti_lmu_get_current_code(u8 imax);
#endif
