/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#define FLASH_NAME "qcom,lm3646"

static struct msm_led_flash_ctrl_t fctrl;
static struct i2c_driver lm3646_i2c_driver;

static struct msm_camera_i2c_reg_array lm3646_init_array[] = {
	{0x01, 0xE0},
	{0x02, 0xA4},
	{0x03, 0x20},
	{0x04, 0x42},
	{0x05, 0x5A},
	{0x06, 0x2F},
	{0x07, 0x3F},
	{0x08, 0x00},
	{0x09, 0x30},
};

static struct msm_camera_i2c_reg_array lm3646_off_array[] = {
	{0x01, 0x00},
};

static struct msm_camera_i2c_reg_array lm3646_release_array[] = {
	{0x0f, 0x00},
};

static struct msm_camera_i2c_reg_array lm3646_low_array[] = {
	{0x01, 0xE2},
};

static struct msm_camera_i2c_reg_array lm3646_high_array[] = {
	{0x01, 0xE3},
};

static void __exit msm_flash_lm3646_i2c_remove(void)
{
	i2c_del_driver(&lm3646_i2c_driver);
	return;
}

static const struct of_device_id lm3646_flash_dt_match[] = {
	{.compatible = "qcom,lm3646", .data = &fctrl},
	{}
};

MODULE_DEVICE_TABLE(of, lm3646_flash_dt_match);

static const struct i2c_device_id lm3646_i2c_id[] = {
	{"qcom,lm3646", (kernel_ulong_t)&fctrl},
	{ }
};

static struct platform_driver msm_flash_lm3646_platform_driver = {
	.driver = {
		.name = "qcom,lm3646",
		.owner = THIS_MODULE,
		.of_match_table = lm3646_flash_dt_match,
	},
};

static int msm_flash_lm3646_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	if (!id) {
		pr_err("msm_flash_lm3646_i2c_probe: id is NULL");
		id = lm3646_i2c_id;
	}

	return msm_flash_i2c_probe(client, id);
}

static struct i2c_driver lm3646_i2c_driver = {
	.id_table = lm3646_i2c_id,
	.probe  = msm_flash_lm3646_i2c_probe,
	.remove = __exit_p(msm_flash_lm3646_i2c_remove),
	.driver = {
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lm3646_flash_dt_match,
	},
};

static int32_t msm_flash_lm3646_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	match = of_match_device(lm3646_flash_dt_match, &pdev->dev);
	if (match) {
		rc = msm_flash_probe(pdev, match->data);
	} else {
		pr_err("%s: %d failed match device\n", __func__, __LINE__);
		return -EINVAL;
	}

	return rc;
}

static struct msm_camera_i2c_client lm3646_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_camera_i2c_reg_setting lm3646_init_setting = {
	.reg_setting = lm3646_init_array,
	.size = ARRAY_SIZE(lm3646_init_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3646_off_setting = {
	.reg_setting = lm3646_off_array,
	.size = ARRAY_SIZE(lm3646_off_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3646_release_setting = {
	.reg_setting = lm3646_release_array,
	.size = ARRAY_SIZE(lm3646_release_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3646_low_setting = {
	.reg_setting = lm3646_low_array,
	.size = ARRAY_SIZE(lm3646_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3646_high_setting = {
	.reg_setting = lm3646_high_array,
	.size = ARRAY_SIZE(lm3646_high_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_led_flash_reg_t lm3646_regs = {
	.init_setting = &lm3646_init_setting,
	.off_setting = &lm3646_off_setting,
	.low_setting = &lm3646_low_setting,
	.high_setting = &lm3646_high_setting,
	.release_setting = &lm3646_release_setting,
};

static struct msm_flash_fn_t lm3646_func_tbl = {
	.flash_get_subdev_id = msm_led_i2c_trigger_get_subdev_id,
	.flash_led_config = msm_led_i2c_trigger_config,
	.flash_led_init = msm_flash_led_init,
	.flash_led_release = msm_flash_led_release,
	.flash_led_off = msm_flash_led_off,
	.flash_led_low = msm_flash_led_low,
	.flash_led_high = msm_flash_led_high,
};

static struct msm_led_flash_ctrl_t fctrl = {
	.flash_i2c_client = &lm3646_i2c_client,
	.reg_setting = &lm3646_regs,
	.func_tbl = &lm3646_func_tbl,
};

static int __init msm_flash_lm3646_init(void)
{
	int32_t rc = 0;
	rc = platform_driver_probe(&msm_flash_lm3646_platform_driver,
		msm_flash_lm3646_platform_probe);
	if (!rc)
		return rc;
	return i2c_add_driver(&lm3646_i2c_driver);
}

static void __exit msm_flash_lm3646_exit_module(void)
{
	if (fctrl.pdev)
		platform_driver_unregister(&msm_flash_lm3646_platform_driver);
	else
		i2c_del_driver(&lm3646_i2c_driver);
	return;
}

module_init(msm_flash_lm3646_init);
module_exit(msm_flash_lm3646_exit_module);
MODULE_DESCRIPTION("LM3646 FLASH");
MODULE_LICENSE("GPL v2");
