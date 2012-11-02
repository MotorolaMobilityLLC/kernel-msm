/*
 * Copyright (C) 2012 Motorola Mobility, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/of.h>

__init u8 dt_get_u8_or_die(struct device_node *node, const char *name)
{
	int len = 0;
	const void *prop;

	prop = of_get_property(node, name, &len);
	BUG_ON(!prop || (len != sizeof(u8)));

	return *(u8 *)prop;
}

__init u16 dt_get_u16_or_die(struct device_node *node, const char *name)
{
	int len = 0;
	const void *prop;

	prop = of_get_property(node, name, &len);
	BUG_ON(!prop || (len != sizeof(u16)));

	return *(u16 *)prop;
}

__init u32 dt_get_u32_or_die(struct device_node *node, const char *name)
{
	int len = 0;
	const void *prop;

	prop = of_get_property(node, name, &len);
	BUG_ON(!prop || (len != sizeof(u32)));

	return *(u32 *)prop;
}

__init unsigned int dt_children_count(struct device_node *node)
{
	struct device_node *child;
	unsigned int count = 0;

	for_each_child_of_node(node, child)
		count++;

	return count;
}

