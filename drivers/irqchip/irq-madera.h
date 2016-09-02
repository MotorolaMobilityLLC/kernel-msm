/*
 * Interrupt support for Cirrus Logic Madera codecs
 *
 * Copyright 2016 Cirrus Logic
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _IRQ_MADERA
#define _IRQ_MADERA

struct regmap_irq_chip;

extern const struct regmap_irq_chip cs47l35_irq;
extern const struct regmap_irq_chip cs47l85_irq;
extern const struct regmap_irq_chip cs47l90_irq;

#endif
