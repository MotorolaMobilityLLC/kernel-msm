/*
 * Maxim ModelGauge ICs fuel gauge driver header file
 *
 * Author: Vladimir Barinov <source@cogentembedded.com>
 * Copyright (C) 2016 Cogent Embedded, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef __BATTERY_MAX17058_H_
#define __BATTERY_MAX17058_H_

#define MAX17058_TABLE_SIZE	64

struct max17058_platform_data {
	u8	rcomp0;
	int	temp_co_up;
	int	temp_co_down;
	u16	ocvtest;
	u8	soc_check_a;
	u8	soc_check_b;
	u8	bits;
	u16	rcomp_seg;
	u8	*model_data;
	const char *batt_psy_name;
};
#endif
