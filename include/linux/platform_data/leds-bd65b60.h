/*
* Simple driver for ROHM Semiconductor BD65B60GWL Backlight driver chip
* Copyright (C) 2014 ROHM Semiconductor.com
* Copyright (C) 2014 MMI
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

enum bd65b60_ovp {
	BD65B60_25V_OVP = 0x00,
	BD65B60_30V_OVP = 0x08,
	BD65B60_35V_OVP = 0x10,
};

enum bd65b60_ledsel {
	BD65B60_DISABLE = 0x00,
	BD65B60_LED1SEL = 0x01,
	BD65B60_LED2SEL = 0x04,
	BD65B60_LED12SEL = 0x05,
};

enum bd65b60_pwm_ctrl {
	BD65B60_PWM_DISABLE = 0x00,
	BD65B60_PWM_ENABLE = 0x20,
};

/*
 *@init_level   : led a init brightness. 4~255
 *@led_sel	: led rail enable/disable
 *@ovp_val	: LED OVP Settings
 *@pwm_period   : pwm period
 *@pwm_ctrl     : pwm enable/disable
 */
struct bd65b60_platform_data {

	/* led config.  */
	int init_level;
	bool no_reset;
	enum bd65b60_ledsel led_sel;
	enum bd65b60_ovp ovp_val;
	const char *name;
	const char *trigger;
	bool default_on;
	bool pwm_on;
	/* pwm config. */
	unsigned int pwm_period;
	enum bd65b60_pwm_ctrl pwm_ctrl;
};
