/*
 * siw_touch_gpio.c - SiW touch gpio driver
 *
 * Copyright (C) 2016 Silicon Works - http://www.siliconworks.co.kr
 * Author: Hyunho Kim <kimhh@siliconworks.co.kr>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */

#ifndef __SIW_TOUCH_GPIO_H
#define __SIW_TOUCH_GPIO_H

enum {
	GPIO_PULL_DOWN  = 0,
	GPIO_PULL_UP,
	GPIO_NO_PULL,
};

enum {
	GPIO_OUT_ZERO = 0,
	GPIO_OUT_ONE,
};

extern int siw_touch_gpio_init(struct device *dev, int pin, const char *name);
extern void siw_touch_gpio_free(struct device *dev, int pin);

extern void siw_touch_gpio_direction_input(struct device *dev, int pin);
extern void siw_touch_gpio_direction_output(struct device *dev,
							int pin, int value);
extern void siw_touch_gpio_set_pull(struct device *dev, int pin, int value);

extern int siw_touch_power_init(struct device *dev);
extern int siw_touch_power_free(struct device *dev);
extern void siw_touch_power_vdd(struct device *dev, int value);
extern void siw_touch_power_vio(struct device *dev, int value);

#endif	/* __SIW_TOUCH_GPIO_H */

