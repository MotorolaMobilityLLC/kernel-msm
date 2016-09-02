/*
 * Platform data for Madera codecs LDO1 regulator
 *
 * Copyright 2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MADERA_LDO1_H_
#define _MADERA_LDO1_H_

struct regulator_init_data;

struct madera_ldo1_pdata {
	/** GPIO controlling LODENA, if any */
	int ldoena;

	/** Regulator configuration for LDO1 */
	const struct regulator_init_data *init_data;
};

#endif
