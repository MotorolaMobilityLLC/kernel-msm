/*
 * Copyright (C) 2012 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#ifndef __MMI_FACTORY__
#define __MMI_FACTORY__

#include <linux/gpio.h>

struct mmi_factory_gpio_entry {
	u32 number;
	int direction; /* GPIOF_DIR_IN, GPIOF_DIR_OUT */
	int value;     /* only used for output */
	const char *name;
};

struct mmi_factory_platform_data {
	int num_gpios;
	struct mmi_factory_gpio_entry *gpios;
};
#endif /* __MMI_FACTORY__ */
