/*
 * platform_mrfl_pmic.h: platform data for pmic driver
 *
 * (C) Copyright 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MRFL_PMIC_H_
#define _PLATFORM_MRFL_PMIC_H_

extern void __init *mrfl_pmic_ccsm_platform_data(
				void *info) __attribute__((weak));

#endif
