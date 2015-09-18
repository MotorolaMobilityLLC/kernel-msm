/*
 * Atmel maXTouch Touchscreen driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Joonyoung Shim <jy0922.shim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __LINUX_ATMEL_MXT_TS_H
#define __LINUX_ATMEL_MXT_TS_H

#include <linux/types.h>

struct mxt_info {
	u8 family_id;
	u8 variant_id;
	u8 version;
	u8 build;
	u8 matrix_xsize;
	u8 matrix_ysize;
	u8 object_num;
};

struct mxt_panel_resolution {
	u16	x_max, y_max;
};

/* The platform data for the Atmel maXTouch touchscreen driver */
struct mxt_platform_data {
	struct mxt_info dt_info;
	struct mxt_panel_resolution res;
	unsigned long irqflags;
	u8 t19_num_keys;
	unsigned int *t19_keymap;
	int t15_num_keys;
	unsigned int *t15_keymap;
	unsigned gpio_reset;
	unsigned gpio_irq;
	bool common_vdd_supply;
	const char *cfg_name;
};

#endif /* __LINUX_ATMEL_MXT_TS_H */
