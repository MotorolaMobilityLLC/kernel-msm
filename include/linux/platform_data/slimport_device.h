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

#ifndef __SLIMPORT_DEVICE
#define __SLIMPORT_DEVICE

struct anx7808_platform_data
{
	int gpio_p_dwn;
	int gpio_reset;
	int gpio_int;
	int gpio_cbl_det;

	spinlock_t lock;

	int (*switch_power)(bool on);

	unsigned int phy_reg2;
	unsigned int phy_reg12;
	unsigned int phy_reg6;
	unsigned int phy_reg16;
};

#endif
