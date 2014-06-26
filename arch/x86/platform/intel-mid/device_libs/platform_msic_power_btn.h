/*
 * platform_msic_power_btn.h: MSIC power btn platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MSIC_POWER_BTN_H_
#define _PLATFORM_MSIC_POWER_BTN_H_

#define INTEL_MID_POWERBTN_DEV_NAME "mid_powerbtn"

extern void __init *msic_power_btn_platform_data(void *info)
		__attribute__((weak));
#endif
