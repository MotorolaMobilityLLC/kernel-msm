/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
/*#define LM3648_DRIVER_DEBUG*/

#undef CDBG
#ifdef LM3648_DRIVER_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

#define FLASH_NAME "ti,lm3648"

#define LED_NUM_CONNECTED  1

#define LED_TORCH_CURRENT_REG    0x03
#define LED1_STROBE_CURRENT_REG  0x00

#define LED_MAX_STROBE_CURRENT      1500
#define LED_STROBE_CURRENT_250      0x00
#define LED_DEFAULT_STROBE_CURRENT  0x1F /* 748.75 mA */
#define LED_STROBE_EN_DEFAULT       0x22 /* both enabled */

#define LED_MAX_TORCH_CURRENT      250
#define LED_DEFAULT_TORCH_CURRENT  0x1A /* 75 mA each */
#define LED_TORCH_EN_DEFAULT       0xDB /* both enabled */

#define LED_MAX_TOTAL_STROBE_CURRENT  1500
#define LED_MAX_TOTAL_TORCH_CURRENT    179


static struct msm_led_flash_ctrl_t fctrl;
static struct i2c_driver lm3648_i2c_driver;

struct lm3648_current_to_reg {
	uint16_t current_level;
	uint16_t reg_value;
};

static struct msm_camera_i2c_reg_array lm3648_init_array[] = {
	{0x01, 0xF3}, /*gpio enable & led on*/
	{0x08, 0x0F}, /*set flash time-out duration to 400ms*/
};

static struct msm_camera_i2c_reg_array lm3648_off_array[] = {
	{0x01, 0xF0},
};

static struct msm_camera_i2c_reg_array lm3648_release_array[] = {
	{0x01, 0xF0},
};

static struct msm_camera_i2c_reg_array lm3648_low_array[] = {
	{0x05, 0xBF}, /*LED1 torch current*/
	{0x01, 0xDB},
};

static struct msm_camera_i2c_reg_array lm3648_high_array[] = {
	{0x03, 0x9F}, /*LED1 flash current */
	{0x01, 0xEF},
	{0x08, 0x0F}, /*set flash time-out duration to 400ms*/
};

static void __exit msm_flash_lm3648_i2c_remove(void)
{
	i2c_del_driver(&lm3648_i2c_driver);
	return;
}

static const struct of_device_id lm3648_trigger_dt_match[] = {
	{.compatible = "ti,lm3648", .data = &fctrl},
	{}
};

MODULE_DEVICE_TABLE(of, lm3648_trigger_dt_match);

static const struct i2c_device_id flash_i2c_id[] = {
	{"ti,lm3648", (kernel_ulong_t)&fctrl},
	{ }
};

static const struct i2c_device_id lm3648_i2c_id[] = {
	{FLASH_NAME, (kernel_ulong_t)&fctrl},
	{ }
};

static int msm_flash_lm3648_led_low(struct msm_led_flash_ctrl_t *fctrl)
{
	uint8_t led1_reg_val = 0x00;

	/* set defaults */
	lm3648_low_array[0].reg_data = LED_DEFAULT_TORCH_CURRENT;
	lm3648_low_array[1].reg_data = LED_TORCH_EN_DEFAULT;

	if ((fctrl->torch_op_current[0] > LED_MAX_TOTAL_TORCH_CURRENT) ||
		(fctrl->torch_op_current[0] == 0)) {
		CDBG("%s:%d - use default current reg:0x%x\n",
				__func__, __LINE__,
				lm3648_low_array[0].reg_data);
	} else {
		led1_reg_val = (fctrl->torch_op_current[0] >= 1) ?
			((fctrl->torch_op_current[0] * 1000 - 1954) / 2800) : 0;

		CDBG("%s:%d - current level %d \n",
				__func__, __LINE__,
				fctrl->torch_op_current[0]);

		lm3648_low_array[0].reg_data = led1_reg_val;

		/* if current level is 0, then turn off LED */
		if (fctrl->torch_op_current[0] == 0)
			lm3648_low_array[1].reg_data &= 0xF0;
		else
			lm3648_low_array[1].reg_data |= 0xDB;
	}
	return msm_flash_led_low(fctrl);
}

static int msm_flash_lm3648_led_high(struct msm_led_flash_ctrl_t *fctrl)
{
	uint8_t led1_reg_val = 0x00;

	/* set defaults */
	lm3648_high_array[0].reg_data = LED_DEFAULT_STROBE_CURRENT;
	if ((fctrl->flash_op_current[0] > LED_MAX_TOTAL_STROBE_CURRENT) ||
		(fctrl->flash_op_current[0] == 0)) {
		CDBG("%s:%d - use default current reg:0x%x\n",
				__func__, __LINE__,
				lm3648_high_array[0].reg_data);
	} else {
		led1_reg_val = (fctrl->flash_op_current[0] >= 11) ?
			((fctrl->flash_op_current[0] * 1000-21800) / 23450) : 0;
		lm3648_high_array[0].reg_data = led1_reg_val;

		/* if current level is 0, then turn off LED */
		if (fctrl->flash_op_current[0] == 0)
			lm3648_high_array[1].reg_data &= 0xF0;
		else
			lm3648_high_array[1].reg_data |= 0xEF;

		CDBG("%s:%d - current level %d %d reg:0x%x 0x%x 0x%x\n",
				__func__, __LINE__,
				fctrl->flash_op_current[0],
				fctrl->flash_op_current[1],
				lm3648_high_array[0].reg_data,
				lm3648_high_array[1].reg_data,
				lm3648_high_array[2].reg_data);
	}
	return msm_flash_led_high(fctrl);
}

static int msm_flash_lm3648_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	if (!id) {
		pr_err("msm_flash_lm3648_i2c_probe: id is NULL");
		id = lm3648_i2c_id;
	}

	return msm_flash_i2c_probe(client, id);
}

static struct i2c_driver lm3648_i2c_driver = {
	.id_table = lm3648_i2c_id,
	.probe  = msm_flash_lm3648_i2c_probe,
	.remove = __exit_p(msm_flash_lm3648_i2c_remove),
	.driver = {
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lm3648_trigger_dt_match,
	},
};

static int msm_flash_lm3648_platform_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	match = of_match_device(lm3648_trigger_dt_match, &pdev->dev);
	if (!match)
		return -EFAULT;
	return msm_flash_probe(pdev, match->data);
}

static struct platform_driver lm3648_platform_driver = {
	.probe = msm_flash_lm3648_platform_probe,
	.driver = {
		.name = "ti,lm3648",
		.owner = THIS_MODULE,
		.of_match_table = lm3648_trigger_dt_match,
	},
};

static int __init msm_flash_lm3648_init_module(void)
{
	int32_t rc = 0;
	rc = platform_driver_register(&lm3648_platform_driver);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&lm3648_i2c_driver);
}

static void __exit msm_flash_lm3648_exit_module(void)
{
	if (fctrl.pdev)
		platform_driver_unregister(&lm3648_platform_driver);
	else
		i2c_del_driver(&lm3648_i2c_driver);
}

static struct msm_camera_i2c_client lm3648_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_camera_i2c_reg_setting lm3648_init_setting = {
	.reg_setting = lm3648_init_array,
	.size = ARRAY_SIZE(lm3648_init_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3648_off_setting = {
	.reg_setting = lm3648_off_array,
	.size = ARRAY_SIZE(lm3648_off_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3648_release_setting = {
	.reg_setting = lm3648_release_array,
	.size = ARRAY_SIZE(lm3648_release_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3648_low_setting = {
	.reg_setting = lm3648_low_array,
	.size = ARRAY_SIZE(lm3648_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3648_high_setting = {
	.reg_setting = lm3648_high_array,
	.size = ARRAY_SIZE(lm3648_high_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_led_flash_reg_t lm3648_regs = {
	.init_setting = &lm3648_init_setting,
	.off_setting = &lm3648_off_setting,
	.low_setting = &lm3648_low_setting,
	.high_setting = &lm3648_high_setting,
	.release_setting = &lm3648_release_setting,
};

static struct msm_flash_fn_t lm3648_func_tbl = {
	.flash_get_subdev_id = msm_led_i2c_trigger_get_subdev_id,
	.flash_led_config = msm_led_i2c_trigger_config,
	.flash_led_init = msm_flash_led_init,
	.flash_led_release = msm_flash_led_release,
	.flash_led_off = msm_flash_led_off,
	.flash_led_low = msm_flash_lm3648_led_low,
	.flash_led_high = msm_flash_lm3648_led_high,
};

static struct msm_led_flash_ctrl_t fctrl = {
	.flash_i2c_client = &lm3648_i2c_client,
	.reg_setting = &lm3648_regs,
	.func_tbl = &lm3648_func_tbl,
	.flash_num_sources = LED_NUM_CONNECTED,
	.flash_max_current[0] = LED_MAX_STROBE_CURRENT,
	.torch_num_sources = LED_NUM_CONNECTED,
	.torch_max_current[0] = LED_MAX_TORCH_CURRENT,

};

/*subsys_initcall(msm_flash_i2c_add_driver);*/
module_init(msm_flash_lm3648_init_module);
module_exit(msm_flash_lm3648_exit_module);
MODULE_DESCRIPTION("lm3648 FLASH");
MODULE_LICENSE("GPL v2");
