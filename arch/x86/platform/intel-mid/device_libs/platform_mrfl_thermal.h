/*
 * platform_mrfl_thermal.h: Platform data initilization file for
 *			Intel Merrifield Platform thermal driver
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Durgadoss R <durgadoss.r@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MRFL_THERMAL_H_
#define _PLATFORM_MRFL_THERMAL_H_

#define MRFL_THERM_DEV_NAME "bcove_thrm"

extern void __init *mrfl_thermal_platform_data(void *)
			__attribute__((weak));

enum {
	mrfl_thermal,
	bdgb_thermal,
};

#endif
