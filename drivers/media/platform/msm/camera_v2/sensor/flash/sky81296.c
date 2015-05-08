/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/export.h>
#include "msm_led_flash.h"

/* Logging macro */
/*#define SKY81296_DRIVER_DEBUG*/

#undef CDBG
#ifdef SKY81296_DRIVER_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define FLASH_NAME "qcom,sky81296"

#define LED_NUM_CONNECTED  2

#define LED_TORCH_CURRENT_REG    0x03
#define LED1_STROBE_CURRENT_REG  0x00
#define LED2_STROBE_CURRENT_REG  0x01

#define LED_MAX_STROBE_CURRENT      1500
#define LED_STROBE_CURRENT_250      0x00
#define LED_DEFAULT_STROBE_CURRENT  0x0A /* 750 mA */
#define LED_STROBE_EN_DEFAULT       0x22 /* both enabled */

#define LED_MAX_TORCH_CURRENT      250
#define LED_DEFAULT_TORCH_CURRENT  0x22 /* 75 mA each */
#define LED_TORCH_EN_DEFAULT       0x11 /* both enabled */

#define LED_MAX_TOTAL_STROBE_CURRENT  1500
#define LED_MAX_TOTAL_TORCH_CURRENT    250


static struct msm_led_flash_ctrl_t fctrl;
static struct i2c_driver sky81296_i2c_driver;

struct sky81296_current_to_reg {
	uint16_t current_level;
	uint16_t reg_value;
};

static struct sky81296_current_to_reg sky81296_torch_values[] = {
	{25, 0x00},
	{50, 0x01},
	{75, 0x02},
	{100, 0x03},
	{125, 0x04},
	{150, 0x05},
	{175, 0x06},
	{200, 0x07},
	{225, 0x08},
	{250, 0x09},
};

static struct sky81296_current_to_reg sky81296_strobe_values[] = {
	{250, 0x00},
	{300, 0x01},
	{350, 0x02},
	{400, 0x03},
	{450, 0x04},
	{500, 0x05},
	{550, 0x06},
	{600, 0x07},
	{650, 0x08},
	{700, 0x09},
	{750, 0x0A},
	{800, 0x0B},
	{850, 0x0C},
	{900, 0x0D},
	{950, 0x0E},
	{1000, 0x0F},
	{1100, 0x10},
	{1200, 0x11},
	{1300, 0x12},
	{1400, 0x13},
	{1500, 0x14},
};

static struct msm_camera_i2c_reg_array sky81296_init_array[] = {
	{0x00, 0x05},
	{0x01, 0x05},
	{0x02, 0x88}, /* 760 ms */
	{0x03, 0x11},
	{0x04, 0x00},
	{0x05, 0x01}, /* VINM */
	{0x06, 0x21}, /* VINHYS 3.0v,VINMON 2.9v */
	{0x07, 0x11},
	{0x08, 0x11},
};

static struct msm_camera_i2c_reg_array sky81296_off_array[] = {
	{0x04, 0x00},
};

static struct msm_camera_i2c_reg_array sky81296_release_array[] = {
	{0x04, 0x00},
};

static struct msm_camera_i2c_reg_array sky81296_low_array[] = {
	{0x03, LED_DEFAULT_TORCH_CURRENT},
	{0x04, LED_TORCH_EN_DEFAULT},
};

static struct msm_camera_i2c_reg_array sky81296_high_array[] = {
	{0x00, LED_DEFAULT_STROBE_CURRENT},
	{0x01, LED_DEFAULT_STROBE_CURRENT},
	{0x03, LED_DEFAULT_TORCH_CURRENT},
	{0x04, LED_STROBE_EN_DEFAULT},
};

static void __exit msm_flash_sky81296_i2c_remove(void)
{
	i2c_del_driver(&sky81296_i2c_driver);
	return;
}

static const struct of_device_id sky81296_trigger_dt_match[] = {
	{.compatible = "qcom,sky81296", .data = &fctrl},
	{}
};

MODULE_DEVICE_TABLE(of, sky81296_trigger_dt_match);

static const struct i2c_device_id flash_i2c_id[] = {
	{"qcom,sky81296", (kernel_ulong_t)&fctrl},
	{ }
};

static const struct i2c_device_id sky81296_i2c_id[] = {
	{FLASH_NAME, (kernel_ulong_t)&fctrl},
	{ }
};

static uint8_t msm_flash_sky81296_get_reg_value(
		struct sky81296_current_to_reg *val_array,
		int size,
		uint16_t op_current)
{
	int i;
	uint8_t ret_reg_val = 0x00;
	uint16_t lo_curr = 0;
	uint16_t hi_curr = 0;

	CDBG("%s:%d - size:%d op_cur:%d\n", __func__, __LINE__,
			size, op_current);

	if (op_current <= val_array[0].current_level) {
		ret_reg_val = val_array[0].reg_value;
	} else if (op_current >= val_array[size-1].current_level) {
		ret_reg_val = val_array[size-1].reg_value;
	} else {
		for (i = 1; i < size; i++) {
			lo_curr = val_array[i-1].current_level;
			hi_curr = val_array[i].current_level;
			if (op_current >= lo_curr &&
				op_current <= hi_curr) {
				/* in between these two, find closest */
				if ((hi_curr - op_current) <
					(op_current - lo_curr)) {
					ret_reg_val = val_array[i].reg_value;
					break;
				} else {
					ret_reg_val = val_array[i-1].reg_value;
					break;
				}
			}
		}
	}

	return ret_reg_val;
}

static int msm_flash_sky81296_led_low(struct msm_led_flash_ctrl_t *fctrl)
{
	uint8_t led1_reg_val = 0x00;
	uint8_t led2_reg_val = 0x00;
	uint8_t reg_val = 0x00;
	uint32_t total_current = 0;
	int tbl_size = sizeof(sky81296_torch_values) /
			sizeof(struct sky81296_current_to_reg);

	/* set defaults */
	sky81296_low_array[0].reg_data = LED_DEFAULT_TORCH_CURRENT;
	sky81296_low_array[1].reg_data = LED_TORCH_EN_DEFAULT;

	total_current = fctrl->torch_op_current[0] +
			fctrl->torch_op_current[1];

	if ((total_current > LED_MAX_TOTAL_TORCH_CURRENT) ||
		(fctrl->torch_op_current[0] == 0 &&
		fctrl->torch_op_current[1] == 0)) {
		CDBG("%s:%d - use default current reg:0x%x\n",
				__func__, __LINE__,
				sky81296_low_array[0].reg_data);
	} else {
		led1_reg_val = msm_flash_sky81296_get_reg_value(
				sky81296_torch_values,
				tbl_size,
				fctrl->torch_op_current[0]);

		led2_reg_val = msm_flash_sky81296_get_reg_value(
				sky81296_torch_values,
				tbl_size,
				fctrl->torch_op_current[1]);

		reg_val = (led2_reg_val << 4) | led1_reg_val;
		CDBG("%s:%d - current level %d %d reg:0x%x\n",
				__func__, __LINE__,
				fctrl->torch_op_current[0],
				fctrl->torch_op_current[1],
				reg_val);

		sky81296_low_array[0].reg_data = reg_val;
		/* if current level is 0, then turn off LED */
		if (fctrl->torch_op_current[1] == 0)
			sky81296_low_array[1].reg_data &= 0x0F;
		if (fctrl->torch_op_current[0] == 0)
			sky81296_low_array[1].reg_data &= 0xF0;
	}
	return msm_flash_led_low(fctrl);
}

static int msm_flash_sky81296_led_high(struct msm_led_flash_ctrl_t *fctrl)
{
	uint8_t led1_reg_val = 0x00;
	uint8_t led2_reg_val = 0x00;
	uint32_t total_current = 0;
	uint8_t led1_torch = false;
	uint8_t led2_torch = false;
	int tbl_size = sizeof(sky81296_strobe_values) /
			sizeof(struct sky81296_current_to_reg);
	int tbl_size_torch = sizeof(sky81296_torch_values) /
			sizeof(struct sky81296_current_to_reg);
	/* set defaults */
	sky81296_high_array[0].reg_data = LED_DEFAULT_STROBE_CURRENT;
	sky81296_high_array[1].reg_data = LED_DEFAULT_STROBE_CURRENT;
	sky81296_high_array[2].reg_data = 0;
	sky81296_high_array[3].reg_data = 0;

	total_current = fctrl->flash_op_current[0] +
			fctrl->flash_op_current[1];

	if ((total_current > LED_MAX_TOTAL_STROBE_CURRENT) ||
		(fctrl->flash_op_current[0] == 0 &&
		fctrl->flash_op_current[1] == 0)) {
		CDBG("%s:%d - use default current reg:0x%x\n",
				__func__, __LINE__,
				sky81296_high_array[0].reg_data);
	} else {
		/*  if one current is < 250 then we need to use mixed
		 *  torch and strobe current levels
		 */
		if (fctrl->flash_op_current[0] >= 250) {
			led1_reg_val = msm_flash_sky81296_get_reg_value(
				sky81296_strobe_values,
				tbl_size,
				fctrl->flash_op_current[0]);
			sky81296_high_array[0].reg_data = led1_reg_val;
		} else {
			led1_reg_val = msm_flash_sky81296_get_reg_value(
				sky81296_torch_values,
				tbl_size_torch,
				fctrl->flash_op_current[0]);
			led1_torch = true;
			sky81296_high_array[2].reg_data = (0x0f & led1_reg_val);
			sky81296_high_array[0].reg_data = LED_STROBE_CURRENT_250;
		}

		if (fctrl->flash_op_current[1] >= 250) {
			led2_reg_val = msm_flash_sky81296_get_reg_value(
				sky81296_strobe_values,
				tbl_size,
				fctrl->flash_op_current[1]);
			sky81296_high_array[1].reg_data = led2_reg_val;
		} else {
			led2_reg_val = msm_flash_sky81296_get_reg_value(
				sky81296_torch_values,
				tbl_size_torch,
				fctrl->flash_op_current[1]);
			led2_torch = true;
			sky81296_high_array[2].reg_data = ((led2_reg_val << 4) |
					sky81296_high_array[2].reg_data);
			sky81296_high_array[1].reg_data = LED_STROBE_CURRENT_250;
		}

		/* if current level is 0, then turn off LED */
		if (led1_torch == true)
			sky81296_high_array[3].reg_data = 0x01;
		else
			sky81296_high_array[3].reg_data = 0x02;

		if (led2_torch == true)
			sky81296_high_array[3].reg_data |= 0x10;
		else
			sky81296_high_array[3].reg_data |= 0x20;

		if (fctrl->flash_op_current[1] == 0)
			sky81296_high_array[3].reg_data &= 0x0F;
		if (fctrl->flash_op_current[0] == 0)
			sky81296_high_array[3].reg_data &= 0xF0;

		CDBG("%s:%d - current level %d %d reg:0x%x 0x%x 0x%x 0x%x\n",
				__func__, __LINE__,
				fctrl->flash_op_current[0],
				fctrl->flash_op_current[1],
				sky81296_high_array[0].reg_data,
				sky81296_high_array[1].reg_data,
				sky81296_high_array[2].reg_data,
				sky81296_high_array[3].reg_data);
	}
	return msm_flash_led_high(fctrl);
}

static int msm_flash_sky81296_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	if (!id) {
		pr_err("msm_flash_sky81296_i2c_probe: id is NULL");
		id = sky81296_i2c_id;
	}

	return msm_flash_i2c_probe(client, id);
}

static struct i2c_driver sky81296_i2c_driver = {
	.id_table = sky81296_i2c_id,
	.probe  = msm_flash_sky81296_i2c_probe,
	.remove = __exit_p(msm_flash_sky81296_i2c_remove),
	.driver = {
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = sky81296_trigger_dt_match,
	},
};

static int msm_flash_sky81296_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	match = of_match_device(sky81296_trigger_dt_match, &pdev->dev);
	if (!match)
		return -EFAULT;
	return msm_flash_probe(pdev, match->data);
}

static struct platform_driver sky81296_platform_driver = {
	.probe = msm_flash_sky81296_platform_probe,
	.driver = {
		.name = "qcom,sky81296",
		.owner = THIS_MODULE,
		.of_match_table = sky81296_trigger_dt_match,
	},
};

static int __init msm_flash_sky81296_init_module(void)
{
	int32_t rc = 0;
	rc = platform_driver_register(&sky81296_platform_driver);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&sky81296_i2c_driver);
}

static void __exit msm_flash_sky81296_exit_module(void)
{
	if (fctrl.pdev)
		platform_driver_unregister(&sky81296_platform_driver);
	else
		i2c_del_driver(&sky81296_i2c_driver);
}

static struct msm_camera_i2c_client sky81296_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_camera_i2c_reg_setting sky81296_init_setting = {
	.reg_setting = sky81296_init_array,
	.size = ARRAY_SIZE(sky81296_init_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting sky81296_off_setting = {
	.reg_setting = sky81296_off_array,
	.size = ARRAY_SIZE(sky81296_off_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting sky81296_release_setting = {
	.reg_setting = sky81296_release_array,
	.size = ARRAY_SIZE(sky81296_release_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting sky81296_low_setting = {
	.reg_setting = sky81296_low_array,
	.size = ARRAY_SIZE(sky81296_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting sky81296_high_setting = {
	.reg_setting = sky81296_high_array,
	.size = ARRAY_SIZE(sky81296_high_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_led_flash_reg_t sky81296_regs = {
	.init_setting = &sky81296_init_setting,
	.off_setting = &sky81296_off_setting,
	.low_setting = &sky81296_low_setting,
	.high_setting = &sky81296_high_setting,
	.release_setting = &sky81296_release_setting,
};

static struct msm_flash_fn_t sky81296_func_tbl = {
	.flash_get_subdev_id = msm_led_i2c_trigger_get_subdev_id,
	.flash_led_config = msm_led_i2c_trigger_config,
	.flash_led_init = msm_flash_led_init,
	.flash_led_release = msm_flash_led_release,
	.flash_led_off = msm_flash_led_off,
	.flash_led_low = msm_flash_sky81296_led_low,
	.flash_led_high = msm_flash_sky81296_led_high,
};

static struct msm_led_flash_ctrl_t fctrl = {
	.flash_i2c_client = &sky81296_i2c_client,
	.reg_setting = &sky81296_regs,
	.func_tbl = &sky81296_func_tbl,
	.flash_num_sources = LED_NUM_CONNECTED,
	.flash_max_current[0] = LED_MAX_STROBE_CURRENT,
	.flash_max_current[1] = LED_MAX_STROBE_CURRENT,
	.torch_num_sources = LED_NUM_CONNECTED,
	.torch_max_current[0] = LED_MAX_TORCH_CURRENT,
	.torch_max_current[1] = LED_MAX_TORCH_CURRENT,
};

/*subsys_initcall(msm_flash_i2c_add_driver);*/
module_init(msm_flash_sky81296_init_module);
module_exit(msm_flash_sky81296_exit_module);
MODULE_DESCRIPTION("sky81296 FLASH");
MODULE_LICENSE("GPL v2");
