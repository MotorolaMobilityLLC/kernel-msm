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

	info->platform_data = &cam_flash_3556;

	return 0;
}

struct msp430_platform_data mp_msp430_data = {
	.gpio_reset = -1,
	.gpio_bslen = -1,
	.gpio_wakeirq = -1,
	.gpio_int = -1,
	.bslen_pin_active_value = 1,
	.fw_version = {'\0'},
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
	int irq_gpio = -1, reset_gpio = -1, bsl_gpio = -1, wake_gpio = -1;
	int lsize, bsize;
	int index;
	unsigned int lux_table[LIGHTING_TABLE_SIZE];
	unsigned int brightness_table[LIGHTING_TABLE_SIZE];
	const char *name;

	info->platform_data = &mp_msp430_data;

	/* irq */

	if (of_property_read_u32(child, "irq,gpio", &irq_gpio))
		return -EINVAL;

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
	if (of_property_read_u32(child, "msp430_gpio_reset", &reset_gpio)) {
		err = -ENODEV;
		goto fail;
	}

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
	if (of_property_read_u32(child, "msp430_gpio_bslen", &bsl_gpio)) {
		err = -ENODEV;
		goto fail;
	}

	mp_msp430_data.gpio_bslen = bsl_gpio;

	err = gpio_request(bsl_gpio, "msp430 bslen");
	if (err) {
		pr_err("msp430 bslen gpio_request failed: %d\n", err);
		goto fail;
	}
	gpio_direction_output(bsl_gpio, 0);
	gpio_set_value(bsl_gpio, 0);
	err = gpio_export(bsl_gpio, 0);

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
		err = -EINVAL;
		goto fail;
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
		err = -EINVAL;
		goto fail;
	}

	if ((lsize + 1) != bsize) {
		pr_err("%s: Lux and Brightness table sizes don't match\n",
			__func__);
		err = -EINVAL;
		goto fail;
	}

	/* wakeirq */
	if (!of_property_read_u32(child, "msp430_gpio_wakeirq", &wake_gpio)) {
		wake_gpio = PM8921_GPIO_PM_TO_SYS(wake_gpio);
		mp_msp430_data.gpio_wakeirq = wake_gpio;

		err = gpio_request(wake_gpio, "msp430 wakeirq");
		if (err) {
			pr_err("msp430 wakeirq gpio_request failed: %d\n", err);
			goto fail;
		}
		gpio_direction_input(wake_gpio);
		err = gpio_export(wake_gpio, 0);
	} else {
		pr_err("msp430 wakeirq not specified\n");
		mp_msp430_data.gpio_wakeirq = -1;
	}

	/* firmware version */
	of_get_property(child, "msp430_fw_version", &len);
	if((len < FW_VERSION_SIZE) &&
		(!of_property_read_string(child, "msp430_fw_version", &name)))
		strcpy(mp_msp430_data.fw_version, name);

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
	unsigned int irq_gpio = 0;
	unsigned int ven_gpio = 0;
	unsigned int firmware_gpio = 0;
	unsigned int ven_polarity = 0;
	unsigned int discharge_delay = 0;

	if (!of_property_read_u32_array(child, "platform_data",
					(u32 *)(&pn544_platform_data), 4)) {
		info->platform_data = &pn544_platform_data;
		pr_info("pn544 got platform_data from dt\n");
		return 0;
	} else {
		pr_info("pn544 platform_data not found in dt\n");
	}

	info->platform_data = &pn544_platform_data;

	/* irq */
	if (of_property_read_u32(child, "pn544_gpio_irq", &irq_gpio))
		return -EINVAL;
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
	if (of_property_read_u32(child, "pn544_gpio_ven", &ven_gpio)) {
		gpio_free(irq_gpio);
		return -EINVAL;
	}
	ven_gpio = PM8921_GPIO_PM_TO_SYS(ven_gpio);
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
	if (of_property_read_u32(child, "pn544_gpio_fwdownload",
						&firmware_gpio)) {
		gpio_free(irq_gpio);
		gpio_free(ven_gpio);
		return -EINVAL;
	}
	firmware_gpio = PM8921_GPIO_PM_TO_SYS(firmware_gpio);
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
	if (of_property_read_u32(child, "pn544_ven_polarity", &ven_polarity)) {
		gpio_free(irq_gpio);
		gpio_free(ven_gpio);
		gpio_free(firmware_gpio);
		return -EINVAL;
	}

	pn544_platform_data.ven_polarity = ven_polarity;

	/* dischage delay */
	if (of_property_read_u32(child, "pn544_discharge_delay",
						&discharge_delay)) {
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
	int err;

	info->platform_data = &tpa6165_pdata;
	/* get irq */
	if (of_property_read_u32(node, "hs_irq_gpio", &tpa6165_pdata.irq_gpio))
		return -EINVAL;

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

	info->platform_data = &aic_platform_data;
	/* irq */
	if (of_property_read_u32(node, "reset_gpio", &rst_gpio))
		return -EINVAL;

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
	{0x00290000, ov8820_init_i2c_device},  /* OV8820 8MP Bayer Sensor */
	{0x00030015, msp430_init_i2c_device}, /* TI MSP430 */
	{0x00190001, pn544_init_i2c_device}, /* NXP PN544 */
	{0x00290002, ov8835_init_i2c_device},  /* Omnivision 8MP Bayer */
	{0x00280001, ar0834_init_i2c_device},  /* Aptina 8MP */
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
