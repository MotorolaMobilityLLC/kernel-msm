/*
* Simple driver for Intersil ISL98611 Backlight driver chip
* Copyright (C) 2015 MMI
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/


/*
 *@init_level   : led a init brightness. 4~255
 *@vp_level   : positive power supply VP = 4.5V + vp_level * 50mV
 *@vn_level   : negative power supply VN = -4.5V - vn_level * 50mV
 */

enum isl98611_led_current {
	ISL98611_20MA = 0x01,
	ISL98611_25MA,
	ISL98611_30MA
};

struct isl98611_platform_data {

	/* led config.  */
	int init_level;
	int vp_level;
	int vn_level;
	int led_current;
	bool no_reset;
	const char *name;
	const char *trigger;
	bool default_on;
	bool cabc_off;
};
