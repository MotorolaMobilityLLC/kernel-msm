/* Copyright (c) 2012, Motorola Mobility, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/of.h>
#include "board-8960.h"

static int __init stub_init_i2c_device(struct i2c_board_info *info,
				       struct device_node *node)
{
	pr_info("%s called for %s\n", __func__, info->type);
	return 0;
}

typedef int (*I2C_INIT_FUNC)(struct i2c_board_info *info,
			     struct device_node *node);

struct mmi_apq_i2c_lookup {
	u32 dt_device;
	I2C_INIT_FUNC init_func;
};

struct mmi_apq_i2c_lookup mmi_apq_i2c_lookup_table[] __initdata = {
	{0x000b0006, stub_init_i2c_device},
	{0x00190001, stub_init_i2c_device},
	{0x00030014, stub_init_i2c_device},
	{0x000B0004, stub_init_i2c_device},    /* National_LM3532 */
	{0x00250001, stub_init_i2c_device},    /* TAOS_CT406 */
	{0x00260001, stub_init_i2c_device},    /* Atmel_MXT */
	{0x00270000, stub_init_i2c_device},    /* Melfas_MMS100 */
	{0x00280000, stub_init_i2c_device},
	{0x00290000, stub_init_i2c_device},
};

static __init I2C_INIT_FUNC get_init_i2c_func(u32 dt_device) {
	int index;

	for (index = 0; index < ARRAY_SIZE(mmi_apq_i2c_lookup_table); index++)
		if (mmi_apq_i2c_lookup_table[index].dt_device == dt_device)
			return mmi_apq_i2c_lookup_table[index].init_func;

	return NULL;
}

__init void mmi_register_i2c_devices_from_dt(void)
{
	struct device_node *bus_node;

	for_each_node_by_name(bus_node, "I2C") {
		struct device_node *dev_node;
		int bus_no;
		int cnt;

		cnt = sscanf(bus_node->full_name,
				"/System@0/I2C@%d", &bus_no);
		if (cnt != 1) {
			pr_err("%s: bad i2c bus entry %s",
					__func__, bus_node->full_name);
			continue;
		}

		pr_info("%s: register devices for %s@%d\n", __func__,
				bus_node->name, bus_no);
		for_each_child_of_node(bus_node, dev_node) {
			const void *prop;
			struct i2c_board_info info;
			int len;
			int err = 0;

			memset(&info, 0, sizeof(struct i2c_board_info));

			prop = of_get_property(dev_node, "i2c,type", &len);
			if (prop)
				strncpy(info.type, (const char *)prop,
					len > I2C_NAME_SIZE ? I2C_NAME_SIZE :
					len);

			prop = of_get_property(dev_node, "i2c,address", &len);
			if (prop && (len == sizeof(u32)))
				info.addr = *(u32 *)prop;

			prop = of_get_property(dev_node, "irq,gpio", &len);
			if (prop && (len == sizeof(u8)))
				info.irq = MSM_GPIO_TO_INT(*(u8 *)prop);

			prop = of_get_property(dev_node, "type", &len);
			if (prop && (len == sizeof(u32))) {
			       I2C_INIT_FUNC init_func =
				       get_init_i2c_func(*(u32 *)prop);
			       if (init_func)
				       err = init_func(&info, dev_node);
			       else {
				       pr_err("%s: Unable to find init function for "
					      "i2c device! Skipping!(0x%08x: %s)\n",
					      __func__, *(u32*)prop, info.type);
				       err = -EINVAL;
			       }
			}
			if (err >= 0)
				i2c_register_board_info(bus_no, &info, 1);
		}
	}

	return;
}
