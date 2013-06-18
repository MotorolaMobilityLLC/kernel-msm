/*
 * Header file for:
 * Cypress CapSense(TM) drivers.
 * Supported parts include:
 * CY8C20237
 *
 * Copyright (C) 2009-2011 Cypress Semiconductor, Inc.
 * Copyright (c) 2012-2013 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __CYCAPSENSE_H__
#define __CYCAPSENSE_H__

#define CYCAPSENSE_I2C_NAME                 "cycapsense_i2c"

struct cycapsense_platform_data {
	int irq_gpio;
	int reset_gpio;
	int sdata_gpio;
	int sclk_gpio;
	int irq;
};

#endif /* __CYCAPSENSE_H__ */
