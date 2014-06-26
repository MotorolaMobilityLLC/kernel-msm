/*
 * intel_mid_gps.h: Intel interface for gps devices
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#ifndef __INTEL_MID_GPS_H__
#define __INTEL_MID_GPS_H__

#define RESET_ON	1
#define RESET_OFF	0
#define ENABLE_ON	1
#define ENABLE_OFF	0

/**
 * struct intel_mid_gps_platform_data - Intel MID GPS platform data
 * @has_reset:		GPS reset GPIO availability
 * @has_enable:		GPS enable GPIO availability
 * @gpio_reset:		GPS reset GPIO number
 * @gpio_enable:	GPS enable GPIO number
 * @gpio_hostwake:	GPS hostwake GPIO number
 * @reset:		GPS reset GPIO current value
 * @enable:		GPS enable GPIO current value
 * @hsu_port:		HSU port number
 */

struct intel_mid_gps_platform_data {
	unsigned int has_reset;
	unsigned int has_enable;
	int gpio_reset;
	int gpio_enable;
	int gpio_hostwake;
	unsigned int reset;
	unsigned int enable;
	unsigned int hsu_port;
};

#endif
