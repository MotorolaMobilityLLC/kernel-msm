/*
 * platform_mrfl_ocd.h: msic_thermal platform data header file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Saranya Gopal <saranya.gopal@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MRFL_OCD_H_
#define _PLATFORM_MRFL_OCD_H_

#define MRFL_OCD_DEV_NAME "bcove_bcu"

extern void __init *mrfl_ocd_platform_data(void *info)
			__attribute__((weak));
#endif
