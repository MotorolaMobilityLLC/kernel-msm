/*
 * platform_msic.h: MSIC platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MSIC_H_
#define _PLATFORM_MSIC_H_

#include <linux/mfd/intel_msic.h>

extern struct intel_msic_platform_data msic_pdata;

extern void *msic_generic_platform_data(void *info,
			enum intel_msic_block block) __attribute__((weak));
#endif
