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

#define FLASH_NAME "qcom,sky81296"

static struct msm_led_flash_ctrl_t fctrl;
static struct i2c_driver sky81296_i2c_driver;

static struct msm_camera_i2c_reg_array sky81296_init_array[] = {
	{0x00, 0x05},
	{0x01, 0x05},
	{0x02, 0xBB},
	{0x03, 0x11},
	{0x04, 0x00},
	{0x05, 0x00},
	{0x06, 0x76},
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
	{0x03, 0x11},
	{0x04, 0x11},
};

static struct msm_camera_i2c_reg_array sky81296_high_array[] = {
	{0x00, 0x05},
	{0x01, 0x05},
	{0x04, 0x22},
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
	.flash_led_low = msm_flash_led_low,
	.flash_led_high = msm_flash_led_high,
};

static struct msm_led_flash_ctrl_t fctrl = {
	.flash_i2c_client = &sky81296_i2c_client,
	.reg_setting = &sky81296_regs,
	.func_tbl = &sky81296_func_tbl,
};

/*subsys_initcall(msm_flash_i2c_add_driver);*/
module_init(msm_flash_sky81296_init_module);
module_exit(msm_flash_sky81296_exit_module);
MODULE_DESCRIPTION("sky81296 FLASH");
MODULE_LICENSE("GPL v2");
