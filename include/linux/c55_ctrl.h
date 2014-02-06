/*
 * Copyright (c) 2012, Motorola Mobility. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __C55_CTRL_H
#define __C55_CTRL_H

#include <linux/gpio.h>

struct c55_ctrl_platform_data {
	struct gpio *gpios;
	int num_gpios;

	int (*adc_clk_en)(int en);
};

#endif

