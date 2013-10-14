/*
 * LM2755 LED chip driver.
 *
 * Copyright (C) 2012 Motorola Mobility, Inc.
 *
 * Contact: Alina Yakovleva <qvdh43@motorola.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __LINUX_LM2755_H__
#define __LINUX_LM2755_H__

#define LM2755_CLOCK_INT	     0
#define LM2755_CLOCK_EXT	     0x40
#define LM2755_CHARGE_PUMP_BEFORE    0
#define LM2755_CHARGE_PUMP_AFTER     0x80
#define LM2755_MAX_LEVEL             0x1F

enum {
	LM2755_D1,
	LM2755_D2,
	LM2755_D3,
	LM2755_NUM_LEDS
};

struct lm2755_platform_data {
	/* Group A, group B and group C LED names in that order */
	const char *led_names[LM2755_NUM_LEDS];
	const char *rgb_name;    /* RGB LED name */
	uint8_t	clock_mode;      /* Internal or external */
	uint8_t charge_pump_mode;
	/* Minimum tstep interval in nanoseconds(!) for external clock */
	uint32_t min_tstep;
	uint8_t max_level;
	void	(*enable)(bool state);
};

#endif /* __LINUX_LM2755_H__ */
