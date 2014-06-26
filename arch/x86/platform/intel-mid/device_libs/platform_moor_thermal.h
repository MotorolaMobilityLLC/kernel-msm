/*
 * platform_moor_thermal.h: Platform data initilization file for
 *			Intel Moorefield Platform thermal driver
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Sumeet R Pawnikar <sumeet.r.pawnikar@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MOOR_THERMAL_H_
#define _PLATFORM_MOOR_THERMAL_H_

#define MOOR_THERM_DEV_NAME "scove_thrm"

extern void __init *moor_thermal_platform_data(void *)
			__attribute__((weak));

enum {
	moor_thermal,
};

#endif
