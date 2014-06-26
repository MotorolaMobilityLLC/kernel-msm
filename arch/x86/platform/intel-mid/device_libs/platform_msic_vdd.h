/*
 * platform_msic_vdd.h: MSIC VDD platform data header file
 *
 * (C) Copyright 2012 Intel Corporation
 * Author: Chaurasia, Avinash K <avinash.k.chaurasia@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MSIC_VDD_H_
#define _PLATFORM_MSIC_VDD_H_

/* BZ 37319- Change the driver name from msic_ocd to msic_vdd */
#define MSIC_VDD_DEV_NAME	"msic_vdd"

#ifdef CONFIG_ACPI
extern void *msic_vdd_platform_data(void *info) __attribute__((weak));
#else
extern void __init *msic_vdd_platform_data(void *info) __attribute__((weak));
#endif
#endif
