/*
 * Platform data for Madera codecs MICSUPP regulator
 *
 * Copyright 2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MADERA_MICSUPP_H_
#define _MADERA_MICSUPP_H_

struct regulator_init_data;

struct madera_micsupp_pdata {
	/** Regulator configuration for MICSUPP */
	const struct regulator_init_data *init_data;
};

#endif
