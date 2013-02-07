/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
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
 */

#ifndef _SYNAPTICS_DSX_H_
#define _SYNAPTICS_DSX_H_

/*
 * struct synaptics_dsx_cap_button_map - 0d button map
 * @nbuttons: number of 0d buttons
 * @map: pointer to array of button types
 */
struct synaptics_dsx_cap_button_map {
	unsigned char nbuttons;
	unsigned char *map;
};

/*
 * struct synaptics_dsx_platform_data - dsx platform data
 * @x_flip: x flip flag
 * @y_flip: y flip flag
 * @regulator_en: regulator enable flag
 * @irq_gpio: attention interrupt gpio
 * @irq_flags: flags used by the irq
 * @reset_gpio: reset gpio
 * @gpio_config: pointer to gpio configuration function
 * @cap_button_map: pointer to 0d button map
 */
struct synaptics_dsx_platform_data {
	bool x_flip;
	bool y_flip;
	bool regulator_en;
	unsigned irq_gpio;
	unsigned long irq_flags;
	unsigned reset_gpio;
	int (*gpio_config)(unsigned gpio, bool configure);
	struct synaptics_dsx_cap_button_map *cap_button_map;
};

#endif
