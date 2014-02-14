/*
 * TI LMU Backlight Common Driver
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

#ifndef __TI_LMU_BACKLIGHT_H__
#define __TI_LMU_BACKLIGHT_H__

#include <linux/mfd/ti-lmu.h>
#include <linux/mfd/ti-lmu-effect.h>

#define LMU_BL_DEFAULT_PWM_NAME		"lmu-backlight"

enum ti_lmu_bl_ctrl_mode {
	BL_REGISTER_BASED,
	BL_PWM_BASED,
};

struct ti_lmu_bl;
struct ti_lmu_bl_chip;

/*
 * struct ti_lmu_bl_ops
 * @init: Device specific initialization
 * @configure: Device specific string configuration
 * @update_brightness: Device specific brightness control
 * @bl_enable: Device specific backlight enable/disable control
 * @max_brightness: Max brightness value of backlight device
 */
struct ti_lmu_bl_ops {
	int (*init)(struct ti_lmu_bl_chip *lmu_chip);
	int (*configure)(struct ti_lmu_bl *lmu_bl);
	int (*update_brightness)(struct ti_lmu_bl *lmu_bl, int brightness);
	int (*bl_enable)(struct ti_lmu_bl *lmu_bl, int enable);
	const int max_brightness;
};

/* One backlight chip can have multiple backlight strings */
struct ti_lmu_bl_chip {
	struct device *dev;
	struct ti_lmu *lmu;
	const struct ti_lmu_bl_ops *ops;
	int num_backlights;
};

/* Backlight string structure */
struct ti_lmu_bl {
	int bank_id;
	struct backlight_device *bl_dev;
	struct ti_lmu_bl_chip *chip;
	struct ti_lmu_backlight_platform_data *bl_pdata;
	enum ti_lmu_bl_ctrl_mode mode;
	struct pwm_device *pwm;
	char pwm_name[20];
};

struct ti_lmu_bl_chip *
ti_lmu_backlight_init_device(struct device *dev, struct ti_lmu *lmu,
			     const struct ti_lmu_bl_ops *ops);

struct ti_lmu_bl *
ti_lmu_backlight_register(struct ti_lmu_bl_chip *chip,
			  struct ti_lmu_backlight_platform_data *pdata,
			  int num_backlights);

int ti_lmu_backlight_unregister(struct ti_lmu_bl *lmu_bl);

void ti_lmu_backlight_effect_callback(struct ti_lmu_effect *lmu_effect,
				      int req_id, void *data);
#endif
