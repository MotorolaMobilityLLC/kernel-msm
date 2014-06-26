/*
 * platform_soc_thermal.h: platform SoC thermal driver library header file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Durgadoss R <durgadoss.r@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_SOC_THERMAL_H_
#define _PLATFORM_SOC_THERMAL_H_

#include <linux/sfi.h>
#include <asm/intel-mid.h>

extern void soc_thrm_device_handler(struct sfi_device_table_entry *,
			struct devs_id *) __attribute__((weak));
#endif
