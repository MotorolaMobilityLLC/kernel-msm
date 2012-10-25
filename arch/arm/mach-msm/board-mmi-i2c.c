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
#include <sound/tlv320aic3253.h>
#include <linux/input/touch_platform.h>
#include <linux/melfas100_ts.h>
#include <linux/msp430.h>
#include <linux/nfc/pn544-mot.h>
#include <linux/drv2605.h>
#include <linux/tpa6165a2.h>
#include <linux/leds-lm3556.h>

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

struct msm_camera_sensor_info msm_camera_sensor_s5k5b3g_data;
static int __init s5k5b3g_init_i2c_device(struct i2c_board_info *info,
                                          struct device_node *node)
{
	info->platform_data = &msm_camera_sensor_s5k5b3g_data;
	return 0;
}

struct lm3556_platform_data cam_flash_3556;
static int __init lm3556_init_i2c_device(struct i2c_board_info *info,
                                          struct device_node *node)
{
	int len = 0;
	const void *prop;

	prop = of_get_property(node, "enable_gpio", &len);
	if (prop && (len == sizeof(u32)))
		cam_flash_3556.hw_enable = *(u32 *)prop;

	prop = of_get_property(node, "current_cntrl_reg_val", &len);
	if (prop && (len == sizeof(u8)))
		cam_flash_3556.current_cntrl_reg_def = *(u8 *)prop;

	info->platform_data = &cam_flash_3556;

	return 0;
}

struct msp430_platform_data mp_msp430_data = {
	.gpio_reset = -1,
	.gpio_bslen = -1,
	.gpio_int = -1,
	.bslen_pin_active_value = 1,
};

struct platform_device msp430_platform_device = {
	.name = "msp430",
	.id = -1,
	.dev = {
		.platform_data = &mp_msp430_data,
	},
};

int __init msp430_init_i2c_device(struct i2c_board_info *info,
		struct device_node *child)
{
	int err = 0;
	int len = 0;
	const void *prop;
	unsigned int irq_gpio = -1, reset_gpio = -1, bsl_gpio  = -1;

	info->platform_data = &mp_msp430_data;

	/* irq */
	prop = of_get_property(child, "irq,gpio", &len);
	if (!prop || (len != sizeof(u8)))
		return -EINVAL;
	irq_gpio = *(u8 *)prop;
	mp_msp430_data.gpio_int = irq_gpio;

	err = gpio_request(irq_gpio, "msp430 int");
	if (err) {
		pr_err("msp430 gpio_request failed: %d\n", err);
		goto fail;
	}
	gpio_direction_input(irq_gpio);
	err = gpio_export(irq_gpio, 0);
	if (err)
		pr_err("msp430 gpio_export failed: %d\n", err);

	/* reset */
	prop = of_get_property(child, "msp430_gpio_reset", &len);
	if (prop && (len == sizeof(u8)))
		reset_gpio = *(u8 *)prop;
	mp_msp430_data.gpio_reset = reset_gpio;

	err = gpio_request(reset_gpio, "msp430 reset");
	if (err) {
		pr_err("msp430 reset gpio_request failed: %d\n", err);
		goto fail;
	}
	gpio_direction_output(reset_gpio, 1);
	gpio_set_value(reset_gpio, 1);
	err = gpio_export(reset_gpio, 0);
	if (err)
		pr_err("msp430 reset gpio_export failed: %d\n", err);

	/* bslen */
	prop = of_get_property(child, "msp430_gpio_bslen", &len);
	if (prop && (len == sizeof(u8)))
		bsl_gpio = *(u8 *)prop;
	mp_msp430_data.gpio_bslen = bsl_gpio;

	err = gpio_request(bsl_gpio, "msp430 bslen");
	if (err) {
		pr_err("msp430 bslen gpio_request failed: %d\n", err);
		goto fail;
	}
	gpio_direction_output(bsl_gpio, 0);
	gpio_set_value(bsl_gpio, 0);
	err = gpio_export(bsl_gpio, 0);

 fail:
	return err;
}

/* NXP PN544 Init */
struct pn544_i2c_platform_data pn544_platform_data = {
	.irq_gpio = 0,
	.ven_gpio = 0,
	.firmware_gpio = 0,
	.ven_polarity = 0,
	.discharge_delay = 0,
};

struct platform_device pn544_platform_device = {
	.name = "pn544",
	.id = -1,
	.dev = {
		.platform_data = &pn544_platform_data,
	},
};

int __init pn544_init_i2c_device(struct i2c_board_info *info,
		struct device_node *child)
{
	int err;
	int len = 0;
	const void *prop;
	unsigned int irq_gpio = 0;
	unsigned int ven_gpio = 0;
	unsigned int firmware_gpio = 0;
	unsigned int ven_polarity = 0;
	unsigned int discharge_delay = 0;

	prop = of_get_property(child, "platform_data", &len);
	if (prop && len) {
		info->platform_data = kmemdup(prop, len, GFP_KERNEL);
		pr_info("pn544 got platform_data from dt\n");
		return 0;
	} else {
		pr_info("pn544 platform_data not found in dt\n");
	}

	info->platform_data = &pn544_platform_data;

	/* irq */
	prop = of_get_property(child, "pn544_gpio_irq", &len);
	if (!prop || (len != sizeof(u32)))
		return -EINVAL;
	irq_gpio = *(u32 *)prop;
	pn544_platform_data.irq_gpio = irq_gpio;

	err = gpio_request(irq_gpio, "pn544 irq");
	if (err) {
		pr_err("pn544 irq gpio_request failed: %d\n", err);
		goto fail;
	}
	gpio_direction_input(irq_gpio);
	err = gpio_export(irq_gpio, 0);
	if (err)
		pr_err("pn544 irq gpio_export failed: %d\n", err);

	/* ven */
	prop = of_get_property(child, "pn544_gpio_ven", &len);
	if (prop && (len == sizeof(u32))) {
		ven_gpio = *(u32 *)prop;
		ven_gpio = PM8921_GPIO_PM_TO_SYS(ven_gpio);
	} else {
		gpio_free(irq_gpio);
		return -EINVAL;
	}
	pn544_platform_data.ven_gpio = ven_gpio;

	err = gpio_request(ven_gpio, "pn544 reset");
	if (err) {
		gpio_free(irq_gpio);
		pr_err("pn544 ven gpio_request failed: %d\n", err);
		goto fail;
	}
	gpio_direction_output(ven_gpio, 0);
	gpio_set_value(ven_gpio, 0);
	err = gpio_export(ven_gpio, 0);
	if (err)
		pr_err("pna544 ven gpio_export failed: %d\n", err);

	/* firmware download */
	prop = of_get_property(child, "pn544_gpio_fwdownload", &len);
	if (prop && (len == sizeof(u32))) {
		firmware_gpio = *(u32 *)prop;
		firmware_gpio = PM8921_GPIO_PM_TO_SYS(firmware_gpio);
	} else {
		gpio_free(irq_gpio);
		gpio_free(ven_gpio);
		return -EINVAL;
	}
	pn544_platform_data.firmware_gpio = firmware_gpio;

	err = gpio_request(firmware_gpio, "pn544 firmware download");
	if (err) {
		gpio_free(irq_gpio);
		gpio_free(ven_gpio);
		pr_err("pn544 firmware gpio_request failed: %d\n",
			err);
		goto fail;
	}
	gpio_direction_output(firmware_gpio, 0);
	gpio_set_value(firmware_gpio, 0);
	err = gpio_export(firmware_gpio, 0);

	/* ven polarity */
	prop = of_get_property(child, "pn544_ven_polarity", &len);
	if (prop && (len == sizeof(u32))) {
		ven_polarity = *(u32 *)prop;
	} else {
		gpio_free(irq_gpio);
		gpio_free(ven_gpio);
		gpio_free(firmware_gpio);
		return -EINVAL;
	}
	pn544_platform_data.ven_polarity = ven_polarity;

	/* dischage delay */
	prop = of_get_property(child, "pn544_discharge_delay", &len);
	if (prop && (len == sizeof(u32))) {
		discharge_delay = *(u32 *)prop;
	} else {
		gpio_free(irq_gpio);
		gpio_free(ven_gpio);
		gpio_free(firmware_gpio);
		return -EINVAL;
	}
	pn544_platform_data.discharge_delay = discharge_delay;

	pr_info("pn544 initialized i2c device. irq = %d, ven = %d, \
		fwdownload = %d, polarity = %d, delay = %d\n",
		pn544_platform_data.irq_gpio,
		pn544_platform_data.ven_gpio,
		pn544_platform_data.firmware_gpio,
		pn544_platform_data.ven_polarity,
		pn544_platform_data.discharge_delay);

 fail:
	pr_err("pn544 init returned: %d\n", err);
	return err;
}
/* End of NXP PN544 Init */

static int __init stub_init_i2c_device(struct i2c_board_info *info,
				       struct device_node *node)
{
	pr_info("%s called for %s\n", __func__, info->type);
	return -ENODEV;
}

static struct drv260x_platform_data drv2605_data;

static int __init drv2605_init_i2c_device(struct i2c_board_info *info,
				       struct device_node *child)
{
	int len = 0;
	const void *prop;

	info->platform_data = &drv2605_data;
	/* enable gpio */
	prop = of_get_property(child, "en_gpio", &len);
	if (!prop || (len != sizeof(u8)))
		return -EINVAL;
	drv2605_data.en_gpio = *(u8 *)prop;

	/* trigger gpio */
	prop = of_get_property(child, "trigger_gpio", &len);
	if (!prop || (len != sizeof(u8)))
		return 0;
	drv2605_data.trigger_gpio = *(u8 *)prop;

	return 0;
}

static struct tpa6165a2_platform_data tpa6165_pdata;

static int __init tpa6165a2_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	int err;
	int len;
	const void *prop;

	info->platform_data = &tpa6165_pdata;
	/* get irq */
	prop = of_get_property(node, "hs_irq_gpio", &len);
	if (!prop || (len != sizeof(u32)))
		return -EINVAL;

	tpa6165_pdata.irq_gpio = *(u32 *)prop;

	err = gpio_request(tpa6165_pdata.irq_gpio, "hs irq");
	if (err)
		pr_err("tpa6165 hs irq gpio_request failed: %d\n", err);

	return err;
}

static struct aic3253_pdata aic_platform_data;

static int __init aic3253_init_i2c_device(struct i2c_board_info *info,
		struct device_node *node)
{
	int rst_gpio;
	int err;
	int len;
	const void *prop;

	info->platform_data = &aic_platform_data;
	/* irq */
	prop = of_get_property(node, "reset_gpio", &len);
	if (!prop || (len != sizeof(u32)))
		return -EINVAL;
	rst_gpio = *(u32 *)prop;
	aic_platform_data.reset_gpio = rst_gpio;

	err = gpio_request(rst_gpio, "aic reset gpio");
	if (err) {
		pr_err("aic3253 reset gpio_request failed: %d\n", err);
		goto err;
	}
	/* GPIO will be pulled high by the driver to bring the
		device out of reset */
	gpio_direction_output(rst_gpio, 0);
err:
	return err;
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
	{0x00030015, msp430_init_i2c_device}, /* TI MSP430 */
	{0x00190001, pn544_init_i2c_device}, /* NXP PN544 */
	{0x00030017, drv2605_init_i2c_device}, /* TI DRV2605 Haptic driver */
	{0x00030018, aic3253_init_i2c_device}, /* TI aic3253 audio codec Driver */
	{0x0003001A, tpa6165a2_init_i2c_device}, /* TI headset Det/amp Driver */
	{0x00090007, s5k5b3g_init_i2c_device}, /* Samsung 2MP Bayer */
	{0x000B0006, lm3556_init_i2c_device}, /* National LM3556 LED Flash */
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
				strlcpy(info.type, (const char *)prop,
					len > I2C_NAME_SIZE ? I2C_NAME_SIZE :
					len);

			prop = of_get_property(dev_node, "i2c,address", &len);
			if (prop && (len == sizeof(u32)))
				info.addr = *(u32 *)prop;

			prop = of_get_property(dev_node, "irq,gpio", &len);
			if (prop && (len == sizeof(u8)))
				info.irq = gpio_to_irq(*(u8 *)prop);

			prop = of_get_property(dev_node, "type", &len);
			if (prop && (len == sizeof(u32))) {
				I2C_INIT_FUNC init_func =
					get_init_i2c_func(*(u32 *)prop);
				if (init_func)
					err = init_func(&info, dev_node);
			}
			if (err >= 0)
				i2c_register_board_info(bus_no, &info, 1);
		}
	}

	return;
}
