/*
 * Copyright(c) 2012, LG Electronics Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SLIMPORT_H
#define __SLIMPORT_H

#include <linux/spinlock_types.h>
#include <linux/err.h>

struct anx7808_platform_data
{
	int gpio_p_dwn;
	int gpio_reset;
	int gpio_int;
	int gpio_cbl_det;
	int gpio_v10_ctrl;
	int gpio_v33_ctrl;
	int external_ldo_control;
	int (* avdd_power) (unsigned int onoff);
	int (* dvdd_power) (unsigned int onoff);
	struct regulator *avdd_10;
	struct regulator *dvdd_10;
	spinlock_t lock;
};

#ifdef CONFIG_SLIMPORT_ANX7808
int slimport_read_edid_block(int block, uint8_t *edid_buf);
#else
static inline int slimport_read_edid_block(int block, uint8_t *edid_buf) { return -ENOSYS; }
#endif

#endif
