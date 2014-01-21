/* Copyright (c) 2012-2013, Motorola Mobility LLC
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
#include <sound/tlv320aic3253.h>
#include <linux/input/touch_platform.h>
#include <linux/melfas100_ts.h>
#include <linux/msp430.h>
#include <linux/nfc/pn544-mot.h>
#include <linux/drv2605.h>
#include <linux/tpa6165a2.h>
#include <linux/leds-lm3556.h>
#include <linux/tps65132.h>
#include <linux/lp8556.h>
#include <sound/tfa9890.h>
#include <linux/ct406.h>
#include <mach/gpiomux.h>
#ifdef CONFIG_BACKLIGHT_LM3532
#include <linux/i2c/lm3532.h>
#endif


#define SY32xx_TOUCH_SCL_GPIO       37
#define SY32xx_TOUCH_SDA_GPIO       36

#define SY32xx_TOUCH_INT_GPIO       70
#define SY32xx_TOUCH_RESET_GPIO     34

extern int __init ct406_init(struct i2c_board_info *info, struct device_node *child);

struct touch_platform_data sy3200_touch_pdata = {
	.flags          = TS_FLIP_X | TS_FLIP_Y,

	.gpio_interrupt = SY32xx_TOUCH_INT_GPIO,
	.gpio_reset     = SY32xx_TOUCH_RESET_GPIO,
	.gpio_scl       = SY32xx_TOUCH_SCL_GPIO,
	.gpio_sda       = SY32xx_TOUCH_SDA_GPIO,

	.max_x          = 1023,
	.max_y          = 1023,

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

static int __init sy3200_init_i2c_device(struct i2c_board_info *info,
                                      struct device_node *node)
{
	int retval = 0;
	unsigned int irq_gpio = -1, i2c_address = -1;
	unsigned int rst_gpio = -1;
	const char *name;

	info->platform_data = &sy3200_touch_pdata;

	/* i2c address */
	if(of_property_read_u32(node, "i2c,address", &i2c_address))
		return -EINVAL;
	sy3200_touch_pdata.addr[0] = (u8)i2c_address;
	sy3200_touch_pdata.addr[1] = 0;

	/* interrupt gpio */
	if(of_property_read_u32(node, "irq,gpio", &irq_gpio))
		return -EINVAL;
	sy3200_touch_pdata.gpio_interrupt = irq_gpio;

	/* reset gpio */
	if(of_property_read_u32(node, "rst_gpio", &rst_gpio))
		return -EINVAL;
	sy3200_touch_pdata.gpio_reset = rst_gpio;

	/* tdat_filename */
	if (of_property_read_string(node, "tdat_filename", &name)) {
		pr_err("%s: tdat file name is missing.\n", __func__);
		return -ENOENT;
	}
	sy3200_touch_pdata.filename = (char *)name;

	pr_info("%s: i2c addr: 0x%x gpio:%u,%u tdat: %s\n",
		__func__, i2c_address, irq_gpio, rst_gpio,
		sy3200_touch_pdata.filename);

	return retval;
}

#define SY34xx_TOUCH_SCL_GPIO       17
#define SY34xx_TOUCH_SDA_GPIO       16

#define SY34xx_TOUCH_INT_GPIO       46
#define SY34xx_TOUCH_RESET_GPIO     63

struct touch_platform_data sy3400_touch_pdata = {
	.flags          = TS_FLIP_X | TS_FLIP_Y,

	.gpio_interrupt = SY34xx_TOUCH_INT_GPIO,
	.gpio_reset     = SY34xx_TOUCH_RESET_GPIO,
	.gpio_scl       = SY34xx_TOUCH_SCL_GPIO,
	.gpio_sda       = SY34xx_TOUCH_SDA_GPIO,

	.max_x          = 1023,
	.max_y          = 1023,

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

static int __init sy3400_init_i2c_device(struct i2c_board_info *info,
                                      struct device_node *node)
{
	int retval = 0;
	unsigned int irq_gpio = -1, i2c_address = -1;
	unsigned int rst_gpio = -1;
	const char *name;

	info->platform_data = &sy3400_touch_pdata;

	/* i2c address */
	if(of_property_read_u32(node, "i2c,address", &i2c_address))
		return -EINVAL;
	sy3400_touch_pdata.addr[0] = (u8)i2c_address;
	sy3400_touch_pdata.addr[1] = 0;

	/* interrupt gpio */
	if(of_property_read_u32(node, "irq,gpio", &irq_gpio))
		return -EINVAL;
	sy3400_touch_pdata.gpio_interrupt = irq_gpio;

	/* reset gpio */
	if(of_property_read_u32(node, "rst_gpio", &rst_gpio))
		return -EINVAL;
	sy3400_touch_pdata.gpio_reset = rst_gpio;

	/* tdat_filename */
	if (of_property_read_string(node, "tdat_filename", &name)) {
		pr_err("%s: tdat file name is missing.\n", __func__);
		return -ENOENT;
	}
	sy3400_touch_pdata.filename = (char *)name;

	pr_info("%s: i2c addr: 0x%x gpio:%u,%u tdat: %s\n",
		__func__, i2c_address, irq_gpio, rst_gpio,
		sy3400_touch_pdata.filename);

	return retval;
}

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

#define DT_GENERIC_TOUCH_DRIVER        0x00000019
#define DT_GENERIC_TOUCH_IC            0x0000001A
#define DT_GENERIC_TOUCH_FIRMWARE      0x0000001B
#define DT_GENERIC_TOUCH_FRAMEWORK     0x0000001C
#define DT_GENERIC_TOUCH_TDAT          0x0000001E
#define DT_GENERIC_TOUCH_TGPIO         0x0000001F


static int __init mmi_touch_driver_settings_init(
		struct touch_platform_data *tpdata,
		struct device_node *node)
{
	int err = 0;
	const void *prop;
	int size = 0;

	prop = of_get_property(node, "touch_driver_flags", &size);
	if (prop != NULL && size > 0)
		tpdata->flags = *((uint16_t *) prop);
	else
		tpdata->flags = 0;

	return err;
}


static int __init mmi_touch_ic_settings_init(
		struct touch_platform_data *tpdata,
		struct device_node *node)
{
	int err = 0;
	const void *prop;
	int size = 0;
	const void *list_prop;
	int list_size = 0;
	char *prop_name = NULL;
	int  prop_name_length;
	char *str_num = NULL;
	const char str1[] = "tsett";
	const char str2[] = "_tag";
	uint8_t tsett_num;
	int i = 0;

	prop = of_get_property(node, "i2c_addrs", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: I2C address data is missing.\n", __func__);
		err = -ENOENT;
		goto touch_ic_settings_init_fail;
	} else if (size > ARRAY_SIZE(tpdata->addr)) {
		pr_err("%s: Too many I2C addresses provided.\n", __func__);
		err = -E2BIG;
		goto touch_ic_settings_init_fail;
	}

	for (i = 0; i < size; i++)
		tpdata->addr[i] = ((uint8_t *)prop)[i];

	for (i = size; i < ARRAY_SIZE(tpdata->addr); i++)
		tpdata->addr[i] = 0;

	list_prop = of_get_property(node, "tsett_list", &list_size);
	if (list_prop == NULL || size <= 0) {
		pr_err("%s: No settings list provided.\n", __func__);
		err = -ENOENT;
		goto touch_ic_settings_init_fail;
	}

	prop_name_length = sizeof(str1) + (sizeof(char) * 4) + sizeof(str2);
	prop_name = kzalloc(prop_name_length, GFP_KERNEL);
	if (prop_name == NULL) {
		pr_err("%s: No memory for prop_name.\n", __func__);
		err = -ENOMEM;
		goto touch_ic_settings_init_fail;
	}

	str_num = kzalloc(sizeof(char) * 4, GFP_KERNEL);
	if (str_num == NULL) {
		pr_err("%s: No memory for str_num.\n", __func__);
		err = -ENOMEM;
		goto touch_ic_settings_init_fail;
	}

	for (i = 0; i < ARRAY_SIZE(tpdata->sett); i++)
		tpdata->sett[i] = NULL;

	for (i = 0; i < list_size; i++) {
		tsett_num = ((uint8_t *)list_prop)[i];
		err = snprintf(str_num, 3, "%hu", tsett_num);
		if (err < 0) {
			pr_err("%s: Error in snprintf converting %hu.\n",
					__func__, tsett_num);
			goto touch_ic_settings_init_fail;
		}
		err = 0;
		prop_name[0] = '\0';
		strlcat(prop_name, str1, prop_name_length);
		strlcat(prop_name, str_num, prop_name_length);

		prop = of_get_property(node, prop_name, &size);
		if (prop == NULL || size <= 0) {
			pr_err("%s: Entry %s from tsett_list is missing.\n",
					__func__, prop_name);
			err = -ENOENT;
			goto touch_ic_settings_init_fail;
		}

		tpdata->sett[tsett_num] = kzalloc(sizeof(struct touch_settings),
				GFP_KERNEL);
		if (tpdata->sett[tsett_num] == NULL) {
			pr_err("%s: Unable to create tsett node %s.\n",
					__func__, str_num);
			err = -ENOMEM;
			goto touch_ic_settings_init_fail;
		}

		tpdata->sett[tsett_num]->size = size;
		tpdata->sett[tsett_num]->data = kmemdup(prop, size,
				GFP_KERNEL);

		if (!tpdata->sett[tsett_num]->data) {
			pr_err("%s: unable to allocate memory for sett data\n",
					__func__);
			err = -ENOMEM;
			goto touch_ic_settings_init_fail;
		}

		strlcat(prop_name, str2, prop_name_length);

		prop = of_get_property(node, prop_name, &size);
		if (prop != NULL && size > 0)
			tpdata->sett[tsett_num]->tag = *((uint8_t *)prop);
		else
			tpdata->sett[tsett_num]->tag = 0;
	}

touch_ic_settings_init_fail:
	kfree(prop_name);
	kfree(str_num);

	return err;
}

static int __init mmi_touch_firmware_init(
		struct touch_platform_data *tpdata,
		struct device_node *node)
{
	int err = 0;
	const void *prop;
	int size = 0;

	tpdata->fw =
		kzalloc(sizeof(struct touch_firmware), GFP_KERNEL);
	if (tpdata->fw == NULL) {
		pr_err("%s: Unable to create fw node.\n", __func__);
		err = -ENOMEM;
		goto touch_firmware_init_fail;
	}

	prop = of_get_property(node, "fw_image", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: Firmware image is missing.\n", __func__);
		err = -ENOENT;
		goto touch_firmware_init_fail;
	} else {
		tpdata->fw->img = kmemdup(prop, size, GFP_KERNEL);
		tpdata->fw->size = size;

		if (!tpdata->fw->img) {
			pr_err("%s: unable to allocate memory for FW image\n",
					__func__);
			err = -ENOMEM;
			goto touch_firmware_init_fail;
		}
	}

	prop = of_get_property(node, "fw_version", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: Firmware version is missing.\n", __func__);
		err = -ENOENT;
		goto touch_firmware_init_fail;
	} else {
		tpdata->fw->ver = kmemdup(prop, size, GFP_KERNEL);
		tpdata->fw->vsize = size;

		if (!tpdata->fw->ver) {
			kfree(tpdata->fw->img);

			pr_err("%s: unable to allocate memory for FW version\n",
					__func__);
			err = -ENOMEM;
			goto touch_firmware_init_fail;
		}
	}

touch_firmware_init_fail:
	return err;
}


static int __init mmi_touch_framework_settings_init(
		struct touch_platform_data *tpdata,
		struct device_node *node)
{
	int err = 0;
	const void *prop;
	int size = 0;

	tpdata->frmwrk =
		kzalloc(sizeof(struct touch_framework), GFP_KERNEL);
	if (tpdata->frmwrk == NULL) {
		pr_err("%s: Unable to create frmwrk node.\n",
			__func__);
		err = -ENOMEM;
		goto touch_framework_settings_fail;
	}

	prop = of_get_property(node, "abs_params", &size);
	if (prop == NULL || size <= 0) {
		pr_err("%s: Abs parameters are missing.\n", __func__);
		err = -ENOENT;
		goto touch_framework_settings_fail;
	} else {
		tpdata->frmwrk->abs = kmemdup(prop, size, GFP_KERNEL);
		tpdata->frmwrk->size = size/sizeof(uint16_t);

		if (!tpdata->frmwrk->abs) {
			pr_err("%s: unable to allocate memory for abs_params\n",
					__func__);
			err = -ENOMEM;
			goto touch_framework_settings_fail;
		}
	}

	tpdata->frmwrk->enable_vkeys = of_property_read_bool(
									node,
									"enable_touch_vkeys");

	if (!tpdata->frmwrk->enable_vkeys) {
		kfree(tpdata->frmwrk->abs);

		pr_err("%s: unable to allocate memory for enable_touch_vkeys\n",
				__func__);
		err = -ENOMEM;
		goto touch_framework_settings_fail;
	}

touch_framework_settings_fail:
	return err;
}


static int __init mmi_touch_tdat_init(
		struct touch_platform_data *tpdata,
		struct device_node *node)
{
	int err = 0;
	const char *name;

	if (of_property_read_string(node, "tdat_filename", &name)) {
		pr_err("%s: tdat file name is missing.\n", __func__);
		err = -ENOENT;
		goto touch_tdat_init_fail;
	} else {
		tpdata->filename = (char *)name;
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

static int __init mmi_touch_tgpio_init(
		struct touch_platform_data *tpdata,
		struct device_node *node)
{
	int rv = -EINVAL;

	if (of_property_read_u32(node, "irq_gpio", &tpdata->gpio_interrupt))
		goto touch_gpio_init_fail;
	if (of_property_read_u32(node, "rst_gpio", &tpdata->gpio_reset))
		goto touch_gpio_init_fail;
	if (of_property_read_u32(node, "en_gpio", &tpdata->gpio_enable))
		goto touch_gpio_init_fail;

	rv = 0;
touch_gpio_init_fail:
	return rv;
}


static int __init mmi_touch_panel_init(
		struct touch_platform_data *tpdata,
		struct device_node *node)
{
	int err = 0;
	struct device_node *child;

	for_each_child_of_node(node, child) {
		int type;
		if (!of_property_read_u32(child, "type", &type)) {
			switch (type) {
			case DT_GENERIC_TOUCH_DRIVER:
				err = mmi_touch_driver_settings_init(tpdata,
									child);
				break;

			case DT_GENERIC_TOUCH_IC:
				err = mmi_touch_ic_settings_init(tpdata, child);
				break;

			case DT_GENERIC_TOUCH_FIRMWARE:
				err = mmi_touch_firmware_init(tpdata, child);
				break;

			case DT_GENERIC_TOUCH_FRAMEWORK:
				err = mmi_touch_framework_settings_init(tpdata,
									child);
				break;

			case DT_GENERIC_TOUCH_TDAT:
				err = mmi_touch_tdat_init(tpdata, child);
				break;

			case DT_GENERIC_TOUCH_TGPIO:
				err = mmi_touch_tgpio_init(tpdata, child);
				break;
			}
		}

		if (err) {
			pr_err(KERN_ERR "%s: Failure Noticed. Bailing out.\n",
					__func__);
			break;
		}
	}

	return err;
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
		int type;
		if (!of_property_read_u32(child, "type", &type)) {
			switch (type) {
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

#define CYTT_GPIO_INTR  46
#define CYTT_GPIO_RST   50
#define CYTT_GPIO_ENABLE        52

static struct gpiomux_setting cyts3_resout_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting cyts3_resout_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_UP,
};

static struct gpiomux_setting cyts3_sleep_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting cyts3_sleep_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_6MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting cyts3_int_act_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct gpiomux_setting cyts3_int_sus_cfg = {
	.func = GPIOMUX_FUNC_GPIO,
	.drv = GPIOMUX_DRV_2MA,
	.pull = GPIOMUX_PULL_DOWN,
};

static struct msm_gpiomux_config msm8960_cyts3_configs[] = {
	{
		.gpio = CYTT_GPIO_INTR,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cyts3_int_act_cfg,
			[GPIOMUX_SUSPENDED] = &cyts3_int_sus_cfg,
		},
	},
	{
		.gpio = CYTT_GPIO_ENABLE,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cyts3_sleep_act_cfg,
			[GPIOMUX_SUSPENDED] = &cyts3_sleep_sus_cfg,
		},
	},
	{
		.gpio = CYTT_GPIO_RST,
		.settings = {
			[GPIOMUX_ACTIVE]    = &cyts3_resout_act_cfg,
			[GPIOMUX_SUSPENDED] = &cyts3_resout_sus_cfg,
		},
	},
};

static int __init cyttsp3_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	int rv = 0;
	struct touch_platform_data *tpdata;

	pr_info("%s Cypress TTSP3: platform init for %s\n",
		__func__, info->type);

	tpdata = kzalloc(sizeof(struct touch_platform_data), GFP_KERNEL);
	if (!tpdata) {
		pr_err("%s: Unable to create platform data.\n", __func__);
		rv = -ENOMEM;
		goto out;
	}

	info->platform_data = tpdata;

	tpdata->gpio_enable    = CYTT_GPIO_ENABLE;
	tpdata->gpio_reset     = CYTT_GPIO_RST;
	tpdata->gpio_interrupt = CYTT_GPIO_INTR;

	msm_gpiomux_install(msm8960_cyts3_configs,
			ARRAY_SIZE(msm8960_cyts3_configs));

	if (mmi_touch_panel_init(tpdata, node) != 0) {
		pr_err("%s:FATAL! No touch devtree params found\n", __func__);
		kfree(tpdata);
		info->platform_data = NULL;
		rv = -ENOMEM;
	}

out:
	return rv;
}


static struct oem_camera_sensor_data s5k5b3g_oem_data;

static int __init s5k5b3g_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	/* get reset gpio */
	of_property_read_u32(node, "gpio_reset",
		&msm_camera_sensor_s5k5b3g_data.sensor_reset);

	/* get avdd_en gpio */
	of_property_read_u32(node, "gpio_avdd_en",
		&s5k5b3g_oem_data.sensor_avdd_en);

	/* get dig_en gpio */
	of_property_read_u32(node, "gpio_dig_en",
		&s5k5b3g_oem_data.sensor_dig_en);

	/* get dig_en gpio */
	of_property_read_u32(node, "vdig_on_always",
		&s5k5b3g_oem_data.sensor_vdig_on_always);

	/* Determine if mipi lines are shared to see if we have to enable
	 * another regulator supply */
	of_property_read_u32(node, "is_shared_mipi",
				&s5k5b3g_oem_data.sensor_using_shared_mipi);

	msm_camera_sensor_s5k5b3g_data.oem_data = &s5k5b3g_oem_data;
	info->platform_data = &msm_camera_sensor_s5k5b3g_data;
	return 0;
}

static int __init ov8835_init_i2c_device(struct i2c_board_info *info,
						struct device_node *node)
{
	/* get reset gpio */
	of_property_read_u32(node, "gpio_reset",
			&msm_camera_sensor_ov8835_data.sensor_reset);

	/* get pwd gpio */
	of_property_read_u32(node, "gpio_pwd",
			&msm_camera_sensor_ov8835_data.sensor_pwd);

	info->platform_data = &msm_camera_sensor_ov8835_data;

	return 0;
}

static int __init ar0834_init_i2c_device(struct i2c_board_info *info,
						struct device_node *node)
{
	/* get reset gpio */
	of_property_read_u32(node, "gpio_reset",
			&msm_camera_sensor_ar0834_data.sensor_reset);

	/* get pwd gpio */
	of_property_read_u32(node, "gpio_pwd",
			&msm_camera_sensor_ar0834_data.sensor_pwd);

	info->platform_data = &msm_camera_sensor_ar0834_data;

	return 0;
}

static struct oem_camera_sensor_data mt9m114_oem_data;

static int __init mt9m114_init_i2c_device(struct i2c_board_info *info,
				struct device_node *node)
{
	int ret = -EINVAL;

	/* get reset gpio */
	of_property_read_u32(node, "gpio_reset",
			&msm_camera_sensor_mt9m114_mmi_data.sensor_reset);

	ret = of_property_read_u32(node, "mclk_freq",
		&mt9m114_oem_data.mclk_freq);
	if (ret)
		mt9m114_oem_data.mclk_freq = 24000000;

	/* get digital supply enable gpio */
	of_property_read_u32(node, "gpio_dig_en",
			&mt9m114_oem_data.sensor_dig_en);

	/* get avdd_en gpio */
	of_property_read_u32(node, "gpio_avdd_en",
		&mt9m114_oem_data.sensor_avdd_en);

	msm_camera_sensor_mt9m114_mmi_data.oem_data = &mt9m114_oem_data;
	info->platform_data = &msm_camera_sensor_mt9m114_mmi_data;

	return 0;
}

static int __init ov660_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	/* TODO - Hardware will be changing for P1, will add in support
	 * any necessary gpio changes here. NOTE: ov10820 has shared
	 * supplies with ov660 and ov10820 is already enabling them */
	return 0;
}

static struct oem_camera_sensor_data ov10820_oem_data;

static int __init ov10820_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	/* get reset gpio */
	of_property_read_u32(node, "gpio_reset",
			&msm_camera_sensor_ov10820_data.sensor_reset);

	/* get pwd gpio */
	of_property_read_u32(node, "gpio_pwd",
			&msm_camera_sensor_ov10820_data.sensor_pwd);

	/* get digital supply enable gpio */
	of_property_read_u32(node, "gpio_dig_en",
			&ov10820_oem_data.sensor_dig_en);

	/* see if we are using a different dvdd regulator */
	of_property_read_u32(node, "is_separate_dvdd",
				&ov10820_oem_data.sensor_using_separate_dvdd);

	/* see if we will allow detection of 10mp if asic is there
	 * or not */
	of_property_read_u32(node, "allow_asic_bypass",
			&ov10820_oem_data.sensor_allow_asic_bypass);

	/* Since ASIC revision is not being populated for rev 1b, and
	 * the other method requires more than 50ms for detection, and
	 * starting from p2 builds, all asic's are rev 1b, which allows
	 * for 24 fps. Will use this flag to enable 24 fps */
	of_property_read_u32(node, "is_asic_ver_r1b",
			&ov10820_oem_data.sensor_asic_revision);

	/* See if the PK DVDD supply location was changed */
	of_property_read_u32(node, "is_new_pk_dvdd",
			&ov10820_oem_data.sensor_using_new_pk_dvdd);

	msm_camera_sensor_ov10820_data.oem_data = &ov10820_oem_data;
	info->platform_data = &msm_camera_sensor_ov10820_data;

	return 0;
}


static struct oem_camera_sensor_data ov8820_oem_data;

static int __init ov8820_init_i2c_device(struct i2c_board_info *info,
						struct device_node *node)
{
	/* get reset gpio */
	of_property_read_u32(node, "gpio_reset",
		&msm_camera_sensor_ov8820_data.sensor_reset);

	/* get pwd gpio */
	of_property_read_u32(node, "gpio_pwd",
		&msm_camera_sensor_ov8820_data.sensor_pwd);

	/* get dig_en gpio */
	of_property_read_u32(node, "gpio_dig_en",
		&ov8820_oem_data.sensor_dig_en);

	/* get avdd_en gpio */
	of_property_read_u32(node, "gpio_avdd_en",
		&ov8820_oem_data.sensor_avdd_en);

	msm_camera_sensor_ov8820_data.oem_data = &ov8820_oem_data;
	info->platform_data = &msm_camera_sensor_ov8820_data;

	return 0;
}

static struct oem_camera_sensor_data ov7736_oem_data;

static int __init ov7736_init_i2c_device(struct i2c_board_info *info,
						struct device_node *node)
{
	int ret = -EINVAL;
	/* get reset gpio */
	of_property_read_u32(node, "gpio_reset",
		&msm_camera_sensor_ov7736_data.sensor_reset);

	/* get pwd gpio */
	of_property_read_u32(node, "gpio_pwd",
		&msm_camera_sensor_ov7736_data.sensor_pwd);

	ret = of_property_read_u32(node, "mclk_freq",
		&ov7736_oem_data.mclk_freq);
	if (ret)
		ov7736_oem_data.mclk_freq = 24000000;
	/* get avdd_en gpio */
	of_property_read_u32(node, "gpio_avdd_en",
		&ov7736_oem_data.sensor_avdd_en);

	msm_camera_sensor_ov7736_data.oem_data = &ov7736_oem_data;
	info->platform_data = &msm_camera_sensor_ov7736_data;

	return 0;
}

#ifdef CONFIG_BACKLIGHT_LM3532
/*
 * LM3532
 */

#define LM3532_RESET_GPIO 12

static struct gpiomux_setting lm3532_reset_suspend_config = {
			.func = GPIOMUX_FUNC_GPIO,
			.drv = GPIOMUX_DRV_2MA,
			.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting lm3532_reset_active_config = {
			.func = GPIOMUX_FUNC_GPIO,
			.drv = GPIOMUX_DRV_2MA,
			.pull = GPIOMUX_PULL_NONE,
			.dir = GPIOMUX_OUT_LOW,
};

static struct msm_gpiomux_config lm3532_reset_gpio_config = {
	.gpio = LM3532_RESET_GPIO,
	.settings = {
		[GPIOMUX_SUSPENDED] = &lm3532_reset_suspend_config,
		[GPIOMUX_ACTIVE] = &lm3532_reset_active_config,
	},
};

static int lm3532_power_up(void)
{
	int ret;

	msm_gpiomux_install(&lm3532_reset_gpio_config, 1);

	ret = gpio_request(LM3532_RESET_GPIO, "lm3532_reset");
	if (ret) {
		pr_err("%s: Request LM3532 reset GPIO failed, ret=%d\n",
			__func__, ret);
		return -ENODEV;
	}

	return 0;
}

static int lm3532_power_down(void)
{
	gpio_free(LM3532_RESET_GPIO);

	return 0;
}

static int lm3532_reset_assert(void)
{
	gpio_set_value(LM3532_RESET_GPIO, 0);

	return 0;
}

static int lm3532_reset_release(void)
{
	gpio_set_value(LM3532_RESET_GPIO, 1);

	return 0;
}

struct lm3532_backlight_platform_data mp_lm3532_pdata = {
	.power_up = lm3532_power_up,
	.power_down = lm3532_power_down,
	.reset_assert = lm3532_reset_assert,
	.reset_release = lm3532_reset_release,

	.led1_controller = LM3532_CNTRL_A,
	.led2_controller = LM3532_CNTRL_A,
	.led3_controller = LM3532_CNTRL_C,

	.ctrl_a_name = "lcd-backlight",
	.ctrl_b_name = "lm3532_led1",
	.ctrl_c_name = "lm3532_led2",

	.susd_ramp = 0xC0,
	.runtime_ramp = 0xC0,

	.als1_res = 0xE0,
	.als2_res = 0xE0,
	.als_dwn_delay = 0x00,
	.als_cfgr = 0x2C,

	.en_ambl_sens = 0,

	.pwm_init_delay_ms = 5000,
	.pwm_resume_delay_ms = 0,
	.pwm_disable_threshold = 0,

	.ctrl_a_usage = LM3532_BACKLIGHT_DEVICE,
	.ctrl_a_pwm = 0xC2,
	.ctrl_a_fsc = 0xFF,
	.ctrl_a_l4_daylight = 0xFF,
	.ctrl_a_l3_bright = 0xCC,
	.ctrl_a_l2_office = 0x99,
	.ctrl_a_l1_indoor = 0x66,
	.ctrl_a_l0_dark = 0x33,

	.ctrl_b_usage = LM3532_UNUSED_DEVICE,
	.ctrl_b_pwm = 0x82,
	.ctrl_b_fsc = 0xFF,
	.ctrl_b_l4_daylight = 0xFF,
	.ctrl_b_l3_bright = 0xCC,
	.ctrl_b_l2_office = 0x99,
	.ctrl_b_l1_indoor = 0x66,
	.ctrl_b_l0_dark = 0x33,

	.ctrl_c_usage = LM3532_UNUSED_DEVICE,
	.ctrl_c_pwm = 0x82,
	.ctrl_c_fsc = 0xFF,
	.ctrl_c_l4_daylight = 0xFF,
	.ctrl_c_l3_bright = 0xCC,
	.ctrl_c_l2_office = 0x99,
	.ctrl_c_l1_indoor = 0x66,
	.ctrl_c_l0_dark = 0x33,

	.l4_high = 0xCC,
	.l4_low = 0xCC,
	.l3_high = 0x99,
	.l3_low = 0x99,
	.l2_high = 0x66,
	.l2_low = 0x66,
	.l1_high = 0x33,
	.l1_low = 0x33,

	.boot_brightness = 102,
};
#endif

static int __init lm3532_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	info->platform_data = &mp_lm3532_pdata;
	return 0;
}
static int __init lm3556_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	int value = 0;

	of_property_read_u32(node, "enable_gpio", &cam_flash_3556.hw_enable);

	of_property_read_u32(node, "current_cntrl_reg_val", &value);
	cam_flash_3556.current_cntrl_reg_def = (u8)value;

	/* Set back cameras to use available camera flash */
	msm_camera_sensor_ov8835_data.flash_data = &camera_flash_lm3556;
	msm_camera_sensor_ov8820_data.flash_data = &camera_flash_lm3556;
	msm_camera_sensor_ov10820_data.flash_data = &camera_flash_lm3556;

	info->platform_data = &cam_flash_3556;

	return 0;
}

static struct tps65132_platform_data mp_tps65132_data = {
	.disp_v1_en = 13,
	.disp_v3_en = 90,
};
static int __init tps65132_init_i2c_device(struct i2c_board_info *info,
		struct device_node *child)
{
	pr_debug("...%s: +\n", __func__);

	info->platform_data = &mp_tps65132_data;

	return 0;
}

/* TI MSP430 Init */
struct msp430_platform_data mp_msp430_data;

static int __init msp430_init_i2c_device(struct i2c_board_info *info,
		struct device_node *child)
{
	int len = 0;
	int irq_gpio = -1, reset_gpio = -1, bsl_gpio = -1, wake_gpio = -1;
	int mipi_busy_gpio = -1, mipi_req_gpio = -1;
	int lsize, bsize;
	int index;
	unsigned int lux_table[LIGHTING_TABLE_SIZE];
	unsigned int brightness_table[LIGHTING_TABLE_SIZE];
	const char *name;

	info->platform_data = &mp_msp430_data;

	/* irq */
	of_property_read_u32(child, "irq,gpio", &irq_gpio);
	mp_msp430_data.gpio_int = irq_gpio;

	/* reset */
	of_property_read_u32(child, "msp430_gpio_reset", &reset_gpio);
	mp_msp430_data.gpio_reset = reset_gpio;

	/* bslen */
	of_property_read_u32(child, "msp430_gpio_bslen", &bsl_gpio);
	mp_msp430_data.gpio_bslen = bsl_gpio;

	/* mipi_busy */
	of_property_read_u32(child, "msp430_gpio_mipi_busy", &mipi_busy_gpio);
	mp_msp430_data.gpio_mipi_busy = mipi_busy_gpio;

	/* mipi_req */
	of_property_read_u32(child, "msp430_gpio_mipi_req", &mipi_req_gpio);
	mp_msp430_data.gpio_mipi_req = mipi_req_gpio;

	/* wakeirq, need to tell whether it is correctly retrieved, as after
	 * PM8921_GPIO_PM_TO_SYS, -1 is changed as 150 which is valid gpio */
	if (!of_property_read_u32(child, "msp430_gpio_wakeirq", &wake_gpio)) {
		wake_gpio = PM8921_GPIO_PM_TO_SYS(wake_gpio);
		mp_msp430_data.gpio_wakeirq = wake_gpio;
	} else {
		pr_warn("msp430 wakeirq not specified\n");
	}

	/* lux table */
	of_get_property(child, "lux_table", &len);
	lsize = len / sizeof(u32);
	if ((lsize != 0) && (lsize < (LIGHTING_TABLE_SIZE - 1)) &&
		(!of_property_read_u32_array(child, "lux_table",
					(u32 *)(lux_table),
					lsize)))
		for (index = 0; index < lsize; index++)
			mp_msp430_data.lux_table[index] = ((u32 *)lux_table)[index];
	else {
		pr_err("%s: Lux table is missing\n", __func__);
		return -EINVAL;
	}
	mp_msp430_data.lux_table[lsize] = 0xFFFF;

	/* brightness table */
	of_get_property(child, "brightness_table", &len);
	bsize = len / sizeof(u32);
	if ((bsize != 0) && (bsize < (LIGHTING_TABLE_SIZE)) &&
		!of_property_read_u32_array(child,
					"brightness_table",
					(u32 *)(brightness_table),
					bsize)) {

		for (index = 0; index < bsize; index++)
			mp_msp430_data.brightness_table[index]
				= ((u32 *)brightness_table)[index];
	} else {
		pr_err("%s: Brightness table is missing\n", __func__);
		return -EINVAL;
	}

	if ((lsize + 1) != bsize) {
		pr_err("%s: Lux and Brightness table sizes don't match\n",
			__func__);
		return -EINVAL;
	}

	/* firmware version */
	of_get_property(child, "msp430_fw_version", &len);
	if((len < FW_VERSION_SIZE) &&
		(!of_property_read_string(child, "msp430_fw_version", &name)))
		strcpy(mp_msp430_data.fw_version, name);
	else
		mp_msp430_data.fw_version[0] = '\0';

	/* set the pin as acive high */
	mp_msp430_data.bslen_pin_active_value = 1;

	return 0;
}

/* NXP PN544 Init */
struct pn544_i2c_platform_data pn544_platform_data;

static int __init pn544_init_i2c_device(struct i2c_board_info *info,
		struct device_node *child)
{
	unsigned int irq_gpio = -1;
	unsigned int ven_gpio = -1;
	unsigned int firmware_gpio = -1;
	unsigned int ven_polarity;
	unsigned int delay;

	/* Read legacy format of PN544 DT, should become deprecated */
	if (!of_property_read_u32_array(child, "platform_data",
					(u32 *)(&pn544_platform_data), 4)) {
		info->platform_data = &pn544_platform_data;
		pr_warn("%s: Using deprecated DT entry\n", __func__);
		return 0;
	}

	info->platform_data = &pn544_platform_data;

	of_property_read_u32(child, "pn544_gpio_irq", &irq_gpio);
	pn544_platform_data.irq_gpio = irq_gpio;

	of_property_read_u32(child, "pn544_gpio_ven", &ven_gpio);
	ven_gpio = PM8921_GPIO_PM_TO_SYS(ven_gpio);
	pn544_platform_data.ven_gpio = ven_gpio;

	of_property_read_u32(child, "pn544_gpio_fwdownload", &firmware_gpio);
	firmware_gpio = PM8921_GPIO_PM_TO_SYS(firmware_gpio);
	pn544_platform_data.firmware_gpio = firmware_gpio;

	/* ven polarity */
	if (of_property_read_u32(child, "pn544_ven_polarity", &ven_polarity)) {
		pr_err("%s: could not find ven_polarity!\n", __func__);
		return -EINVAL;
	}
	pn544_platform_data.ven_polarity = ven_polarity;

	/* dischage delay */
	if (of_property_read_u32(child, "pn544_discharge_delay", &delay)) {
		pr_err("%s: could not find discharge delayy!\n", __func__);
		return -EINVAL;
	}
	pn544_platform_data.discharge_delay = delay;

	pr_info("pn544 initialized i2c device. irq = %d, ven = %d, " \
		"fwdownload = %d, polarity = %d, delay = %d\n",
		pn544_platform_data.irq_gpio,
		pn544_platform_data.ven_gpio,
		pn544_platform_data.firmware_gpio,
		pn544_platform_data.ven_polarity,
		pn544_platform_data.discharge_delay);

	return 0;
}

static struct drv260x_platform_data drv2605_data;

static int __init drv2605_init_i2c_device(struct i2c_board_info *info,
				       struct device_node *child)
{
	info->platform_data = &drv2605_data;
	/* enable gpio */
	if (of_property_read_u32(child, "en_gpio", &drv2605_data.en_gpio))
		return -EINVAL;

	/* trigger gpio */
	if (!of_property_read_u32(child, "trigger_gpio",
					&drv2605_data.trigger_gpio)) {
		if (!gpio_request(drv2605_data.trigger_gpio, "vib-trigger")) {
			gpio_direction_output(drv2605_data.trigger_gpio, 0);
			gpio_export(drv2605_data.trigger_gpio, 0);
		}
	}

	/* external trigger mode enable flag*/
	of_property_read_u32(child, "external_trigger",
					&drv2605_data.external_trigger);

	/* default vibration effect for external trigger mode */
	of_property_read_u32(child, "default_effect",
					&drv2605_data.default_effect);

	return 0;
}

static struct tpa6165a2_platform_data tpa6165_pdata;

static int __init tpa6165a2_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	int irq_gpio = -1;

	info->platform_data = &tpa6165_pdata;
	/* get irq */
	of_property_read_u32(node, "hs_irq_gpio", &irq_gpio);

	tpa6165_pdata.irq_gpio = irq_gpio;

	return 0;
}

static struct aic3253_pdata aic_platform_data;

static int __init aic3253_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	int rst_gpio = -1;
	int mclk_gpio = -1;

	info->platform_data = &aic_platform_data;

	of_property_read_u32(node, "reset_gpio", &rst_gpio);

	aic_platform_data.reset_gpio = rst_gpio;

	of_property_read_u32(node, "mclk_sel_gpio", &mclk_gpio);

	aic_platform_data.mclk_sel_gpio = mclk_gpio;

	return 0;
}

static struct tfa9890_pdata tfa_platform_data;

static int __init tfa9890_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	int rst_gpio;

	info->platform_data = &tfa_platform_data;

	if (of_property_read_u32(node, "reset_gpio", &rst_gpio))
		return -EINVAL;

	tfa_platform_data.reset_gpio = rst_gpio;

	return 0;
}

/* backlight init */
static struct lp8556_eeprom_data lp8556_eeprom_pdata[] = {
	{0, 0x98, 0x00},
	{1, 0x9E, 0x02},
	{0, 0xA0, 0xFF},
	{0, 0xA1, 0x4F},
	{0, 0xA2, 0xA0},
	{0, 0xA3, 0x03},
	{0, 0xA4, 0x12},
	{1, 0xA5, 0x3C},
	{1, 0xA6, 0x40},
	{1, 0xA7, 0xFC},
	{0, 0xA8, 0x00},
	{0, 0xA9, 0x80},
	{0, 0xAA, 0x0F},
	{0, 0xAB, 0x00},
	{0, 0xAC, 0x00},
	{0, 0xAD, 0x00},
	{0, 0xAE, 0x13},
	{0, 0xAF, 0x00},
};

static struct lp8556_platform_data lp8556_backlight_pdata = {
	.enable_gpio = -1,
	.power_up_brightness = 0x3A,
	.dev_ctrl_config = 0x84, /* no PWM */
	.eeprom_table = lp8556_eeprom_pdata,
	.eeprom_tbl_sz = ARRAY_SIZE(lp8556_eeprom_pdata),
};

static int __init lp8556_init_i2c_device(struct i2c_board_info *info,
					 struct device_node *node)
{
	/* lp8556 gpios */
	of_property_read_u32(node, "enable_gpio",
			     &lp8556_backlight_pdata.enable_gpio);

	info->platform_data = &lp8556_backlight_pdata;

	return 0;
}
/* end backlight init */

typedef int (*I2C_INIT_FUNC)(struct i2c_board_info *info,
			     struct device_node *node);

struct mmi_apq_i2c_lookup {
	u32 dt_device;
	I2C_INIT_FUNC init_func;
};

struct mmi_apq_i2c_lookup mmi_apq_i2c_lookup_table[] __initdata = {
	{0x00270000, melfas_init_i2c_device},  /* Melfas_MMS100 */
	{0x00260001, atmxt_init_i2c_device},   /* Atmel_MXT */
	{0x00040002, cyttsp3_init_i2c_device}, /* Cypress_CYTTSP3 */
	{0x00290000, ov8820_init_i2c_device},  /* OV8820 8MP Bayer Sensor */
	{0x00290001, ov7736_init_i2c_device},  /* ov7736 yuv Sensor */
	{0x00030015, msp430_init_i2c_device}, /* TI MSP430 */
	{0x00190001, pn544_init_i2c_device}, /* NXP PN544 */
	{0x00290002, ov8835_init_i2c_device},  /* Omnivision 8MP Bayer */
	{0x00280001, ar0834_init_i2c_device},  /* Aptina 8MP */
	{0x00280000, mt9m114_init_i2c_device}, /* MT9M114 Front YUV Sensor */
	{0x00290004, ov660_init_i2c_device},  /* Omnivision OV660 ASIC IC */
	{0x00290003, ov10820_init_i2c_device},  /* Omnivision 8MP RGBC */
	{0x00030017, drv2605_init_i2c_device}, /* TI DRV2605 Haptic driver */
	{0x00030018, aic3253_init_i2c_device}, /* TI aic3253 audio codec Driver */
	{0x0003001A, tpa6165a2_init_i2c_device}, /* TI headset Det/amp Driver */
	{0x00090007, s5k5b3g_init_i2c_device}, /* Samsung 2MP Bayer */
	{0x000B0004, lm3532_init_i2c_device}, /* National LM3532 Backlight */
	{0x000B0006, lm3556_init_i2c_device}, /* National LM3556 LED Flash */
	{0x0003001C, tps65132_init_i2c_device}, /* TI lcd bias Driver */
	{0x000B0007, lp8556_init_i2c_device}, /* National LP8556 Backlight */
	{0x002B0000, sy3200_init_i2c_device},   /* Synaptics 32xx */
	{0x002B0001, sy3400_init_i2c_device},   /* Synaptics 34xx */
	{0x00190002, tfa9890_init_i2c_device}, /* NXP Audio Codec Driver */
        {0x00250001, ct406_init}, 	       /*CT406 driver*/
	{0x00030014, msm8960_tmp105_init},	/*TMP105*/
};

static __init I2C_INIT_FUNC get_init_i2c_func(u32 dt_device)
{
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
			pr_debug("%s: bad i2c bus entry %s",
					__func__, bus_node->full_name);
			continue;
		}

		pr_info("%s: register devices for %s@%d\n", __func__,
				bus_node->name, bus_no);
		for_each_child_of_node(bus_node, dev_node) {
			struct i2c_board_info info;
			const char *name;
			int err = 0, value = 0;

			memset(&info, 0, sizeof(struct i2c_board_info));

			if (!of_property_read_string(dev_node,
							"i2c,type", &name))
				strlcpy(info.type, name, I2C_NAME_SIZE);
			if (!of_property_read_u32(dev_node,
							"i2c,address", &value))
				info.addr = value;

			if (!of_property_read_u32(dev_node,
							"irq,gpio", &value))
				info.irq = gpio_to_irq(value);
			if (!of_property_read_u32(dev_node,
							"type", &value)) {
				I2C_INIT_FUNC init_func =
					get_init_i2c_func(value);
				if (init_func)
					err = init_func(&info, dev_node);
			}
			if (err >= 0)
				i2c_register_board_info(bus_no, &info, 1);
		}
	}

	return;
}
