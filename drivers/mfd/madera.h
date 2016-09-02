/*
 * madera.h  --  MFD internals for Cirrus Logic Madera codecs
 *
 * Copyright 2015-2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MADERA_MFD_H
#define _MADERA_MFD_H

#include <linux/pm.h>
#include <linux/of.h>

struct madera;

extern const struct dev_pm_ops madera_pm_ops;
extern const struct of_device_id madera_of_match[];

int madera_dev_init(struct madera *madera);
int madera_dev_exit(struct madera *madera);
int madera_irq_init(struct madera *madera);
int madera_irq_exit(struct madera *madera);

#ifdef CONFIG_OF
unsigned long madera_of_get_type(struct device *dev);
#else
static inline unsigned long madera_of_get_type(struct device *dev)
{
	return 0;
}
#endif

extern const struct regmap_config cs47l35_16bit_spi_regmap;
extern const struct regmap_config cs47l35_32bit_spi_regmap;
extern const struct regmap_config cs47l35_16bit_i2c_regmap;
extern const struct regmap_config cs47l35_32bit_i2c_regmap;
extern int cs47l35_patch(struct madera *madera);

extern const struct regmap_config cs47l85_16bit_spi_regmap;
extern const struct regmap_config cs47l85_32bit_spi_regmap;
extern const struct regmap_config cs47l85_16bit_i2c_regmap;
extern const struct regmap_config cs47l85_32bit_i2c_regmap;
extern int cs47l85_patch(struct madera *madera);

extern const struct regmap_config cs47l90_16bit_spi_regmap;
extern const struct regmap_config cs47l90_32bit_spi_regmap;
extern const struct regmap_config cs47l90_16bit_i2c_regmap;
extern const struct regmap_config cs47l90_32bit_i2c_regmap;
extern int cs47l90_patch(struct madera *madera);

#endif
