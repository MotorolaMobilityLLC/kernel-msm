/*
 * platform_gps.h: gps platform data header file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#ifndef _PLATFORM_GPS_H_
#define _PLATFORM_GPS_H_

#define GPS_GPIO_RESET	"GPS-Reset"
#define GPS_GPIO_ENABLE	"GPS-Enable"
#define GPS_GPIO_HOSTWAKE	"GPS-Hostwake"

#ifdef CONFIG_INTEL_MID_GPS
void *intel_mid_gps_device_init(void *info);
#else
static inline void *intel_mid_gps_device_init(void *info)
{
	return NULL;
}
#endif
#endif
