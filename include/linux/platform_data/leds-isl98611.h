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
 *@init_level		: led init brightness. 4~255
 *@vp_level		: positive power supply VP = 4.5V + vp_level * 50mV
 *@vn_level		: negative power supply VN = -4.5V - vn_level * 50mV
 *@led_current		: peak led current register value
 *@hbm_led_curren	: above value in high brightness mode
 *@cur_scale		: peak led current multiplier register value
 *@hbm_cur_scal		: above value in high brightness mode
 *@pwm_res		: pwm resolution register value
 *@dimm_threshold	: dimming threshold for dimming control register
 *@hbm_on		: enable high brightness mode
 */

struct isl98611_platform_data {

	/* led config.  */
	int init_level;
	int vp_level;
	int vn_level;
	int led_current;
	int hbm_led_current;
	int cur_scale;
	int hbm_cur_scale;
	int pwm_res;
	int dimm_threshold;
	bool no_reset;
	const char *name;
	const char *trigger;
	bool default_on;
	bool cabc_off;
	bool hbm_on;
};
