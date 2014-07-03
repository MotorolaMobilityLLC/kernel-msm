/*
 * platform_r69001.c: r69001 touch platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/input.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/r69001-ts.h>
#include <asm/intel-mid.h>
#include "platform_r69001.h"

void *r69001_platform_data(void *info)
{
	struct i2c_board_info *i2c_info = info;
	static struct r69001_platform_data r69001_platform_data = {
		.irq_type = IRQF_ONESHOT,
		.gpio = -1,
	};

	if (!i2c_info->irq) { /* not a fast-int */
		r69001_platform_data.gpio = get_gpio_by_name("jdi_touch_int");
		if (r69001_platform_data.gpio == -1)
			r69001_platform_data.gpio = 183;
		r69001_platform_data.irq_type |= IRQ_TYPE_EDGE_FALLING;
	}
	return &r69001_platform_data;
}
