/*
 * platform_mrfl_pmic_i2c.h: platform data for pmic i2c adapter driver
 *
 * (C) Copyright 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MRFL_PMIC_I2C_H_
#define _PLATFORM_MRFL_PMIC_I2C_H_

extern void __init *mrfl_pmic_i2c_platform_data(
				void *info) __attribute__((weak));

#endif
