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
#include "board-mmi.h"

#include <linux/input/touch_platform.h>
#include <linux/melfas100_ts.h>

#define MELFAS_TOUCH_SCL_GPIO       17
#define MELFAS_TOUCH_SDA_GPIO       16
#define MELFAS_TOUCH_INT_GPIO       46
#define MELFAS_TOUCH_RESET_GPIO     50

static struct  touch_firmware  melfas_ts_firmware;

static uint8_t melfas_fw_version[]   = { 0x45 };
static uint8_t melfas_priv_v[]       = { 0x07 };
static uint8_t melfas_pub_v[]        = { 0x15 };
static uint8_t melfas_fw_file_name[] = "melfas_45_7_15.fw";

struct touch_platform_data melfas_touch_pdata = {
	.flags          = TS_FLIP_X | TS_FLIP_Y,

	.gpio_interrupt = MELFAS_TOUCH_INT_GPIO,
	.gpio_reset     = MELFAS_TOUCH_RESET_GPIO,
	.gpio_scl       = MELFAS_TOUCH_SCL_GPIO,
	.gpio_sda       = MELFAS_TOUCH_SDA_GPIO,

	.max_x          = 719,
	.max_y          = 1279,

	.invert_x       = 1,
	.invert_y       = 1,

	.int_latency    = MMI_TOUCH_CALCULATE_LATENCY_FUNC,
	.int_time       = MMI_TOUCH_SET_INT_TIME_FUNC,
	.get_avg_lat    = MMI_TOUCH_GET_AVG_LATENCY_FUNC,
	.get_high_lat   = MMI_TOUCH_GET_HIGH_LATENCY_FUNC,
	.get_slow_cnt   = MMI_TOUCH_GET_SLOW_INT_COUNT_FUNC,
	.get_int_cnt    = MMI_TOUCH_GET_INT_COUNT_FUNC,
	.set_dbg_lvl    = MMI_TOUCH_SET_LATENCY_DEBUG_LEVEL_FUNC,
	.get_dbg_lvl    = MMI_TOUCH_GET_LATENCY_DEBUG_LEVEL_FUNC,
	.get_time_ptr   = MMI_TOUCH_GET_TIMESTAMP_PTR_FUNC,
	.get_lat_ptr    = MMI_TOUCH_GET_LATENCY_PTR_FUNC,
};

static int __init melfas_init_i2c_device(struct i2c_board_info *info,
				       struct device_node *node)
{
	pr_info("%s MELFAS TS: platform init for %s\n", __func__, info->type);

	info->platform_data = &melfas_touch_pdata;

	/* melfas reset gpio */
	gpio_request(MELFAS_TOUCH_RESET_GPIO, "touch_reset");
	gpio_direction_output(MELFAS_TOUCH_RESET_GPIO, 1);

	/* melfas interrupt gpio */
	gpio_request(MELFAS_TOUCH_INT_GPIO, "touch_irq");
	gpio_direction_input(MELFAS_TOUCH_INT_GPIO);

	gpio_request(MELFAS_TOUCH_SCL_GPIO, "touch_scl");
	gpio_request(MELFAS_TOUCH_SDA_GPIO, "touch_sda");

	/* Setup platform structure with the firmware information */
	melfas_ts_firmware.ver = &(melfas_fw_version[0]);
	melfas_ts_firmware.vsize = sizeof(melfas_fw_version);
	melfas_ts_firmware.private_fw_v = &(melfas_priv_v[0]);
	melfas_ts_firmware.private_fw_v_size = sizeof(melfas_priv_v);
	melfas_ts_firmware.public_fw_v = &(melfas_pub_v[0]);
	melfas_ts_firmware.public_fw_v_size = sizeof(melfas_pub_v);

	melfas_touch_pdata.fw = &melfas_ts_firmware;
	strcpy(melfas_touch_pdata.fw_name, melfas_fw_file_name);

	return 0;
}

#define DT_GENERIC_TOUCH_TDAT          0x0000001E
#define DT_GENERIC_TOUCH_TGPIO         0x0000001F

static int __init mmi_touch_tdat_init(
		struct touch_platform_data *tpdata,
		struct device_node *node)
{
	int err = 0;
	const void *prop;
	int size = 0;

	prop = of_get_property(node, "tdat_filename", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: tdat file name is missing.\n", __func__);
		err = -ENOENT;
		goto touch_tdat_init_fail;
	} else {
		tpdata->filename = kstrndup((char *)prop, size, GFP_KERNEL);
		if (!tpdata->filename) {
			pr_err("%s: unable to allocate memory for "
					"tdat file name\n", __func__);
			err = -ENOMEM;
			goto touch_tdat_init_fail;
		}
	}

touch_tdat_init_fail:
	return err;
}

static int __init dt_get_gpio(struct device_node *node,
		const char *name, int *gpio) {
	int rv = 0;
	int size;
	const void *prop = of_get_property(node, name, &size);
	if (prop == NULL || size != sizeof(u8)) {
		pr_err("%s: tgpio %s is missing.\n", __func__, name);
		rv = -ENOENT;
	} else {
		*gpio = *(u8 *)prop;
	}

	return rv;
}

static int __init mmi_touch_tgpio_init(
		struct touch_platform_data *tpdata,
		struct device_node *node)
{
	int rv = -EINVAL;

	if (dt_get_gpio(node, "irq_gpio", &tpdata->gpio_interrupt))
		goto touch_gpio_init_fail;
	if (dt_get_gpio(node, "rst_gpio", &tpdata->gpio_reset))
		goto touch_gpio_init_fail;
	if (dt_get_gpio(node, "en_gpio", &tpdata->gpio_enable))
		goto touch_gpio_init_fail;

	rv = 0;
touch_gpio_init_fail:
	return rv;
}

static int __init atmxt_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	int rv = 0;
	struct device_node *child;
	struct touch_platform_data *tpdata;

	pr_info("%s ATMXT TS: platform init for %s\n", __func__, info->type);

	tpdata = kzalloc(sizeof(struct touch_platform_data), GFP_KERNEL);
	if (!tpdata) {
		pr_err("%s: Unable to create platform data.\n", __func__);
		rv = -ENOMEM;
		goto out;
	}

	info->platform_data = tpdata;

	for_each_child_of_node(node, child) {
		int len = 0;
		const void *prop;

		prop = of_get_property(child, "type", &len);
		if (prop && (len == sizeof(u32))) {
			switch (*(u32 *)prop) {
			case DT_GENERIC_TOUCH_TDAT:
				rv = mmi_touch_tdat_init(tpdata, child);
				break;
			case DT_GENERIC_TOUCH_TGPIO:
				rv = mmi_touch_tgpio_init(tpdata, child);
				break;
			}
		}
		if (rv)
			goto out;

	}
out:
	return rv;
}

static int __init stub_init_i2c_device(struct i2c_board_info *info,
				       struct device_node *node)
{
	pr_info("%s called for %s\n", __func__, info->type);
	return -1;
}

typedef int (*I2C_INIT_FUNC)(struct i2c_board_info *info,
			     struct device_node *node);

struct mmi_apq_i2c_lookup {
	u32 dt_device;
	I2C_INIT_FUNC init_func;
};

struct mmi_apq_i2c_lookup mmi_apq_i2c_lookup_table[] __initdata = {
	{0x00270000, melfas_init_i2c_device},  /* Melfas_MMS100 */
	{0x00260001, atmxt_init_i2c_device},   /* Atmel_MXT */
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
