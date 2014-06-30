/*
 * platform_display.c: put platform display configuration in
 * this file. If any platform level display related configuration
 * has to be made, then that configuration shoule be in this
 * file.
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/printk.h>
#include <linux/platform_data/lp855x.h>

static struct i2c_board_info __initdata lp8556_i2c_device = {
	I2C_BOARD_INFO("lp8556", 0x2C),
};

struct lp855x_platform_data platform_data = {
	.name = "lp8556",
	.device_control = 0,
	.initial_brightness = 0,
	.period_ns = 5000000, /* 200 Hz */
	.size_program = 0,
	.rom_data = NULL,
};

void *lp8556_get_platform_data(void)
{

	return (void *)&platform_data;
}

static int __init platform_display_module_init(void)
{

	lp8556_i2c_device.platform_data = lp8556_get_platform_data();

	if (lp8556_i2c_device.platform_data == NULL) {
		pr_debug("failed to get platform data for lp8556.");
		return -EINVAL;
	}

	return -EPERM;
}

module_init(platform_display_module_init);

