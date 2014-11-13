/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/reboot.h>

#define FLASH_NAME "qcom,lm3642"

static struct msm_led_flash_ctrl_t fctrl;
static struct i2c_driver lm3642_i2c_driver;

static struct msm_camera_i2c_reg_array lm3642_init_array[] = {
	{0x01, 0x00},
	{0x06, 0x00},
	{0x08, 0x52},
	{0x09, 0x29},
	{0x0A, 0x40},
	{0x0B, 0x00},
};

static struct msm_camera_i2c_reg_array lm3642_off_array[] = {
	{0x0A, 0x00},
};

static struct msm_camera_i2c_reg_array lm3642_release_array[] = {
	{0x0A, 0x00},
};

static struct msm_camera_i2c_reg_array lm3642_low_array[] = {
/* when configured Flash in torch mode because of h/w issue
 Flash fault is happening.Following is the workaround
 to fix the issue.*/
	{0x0A, 0x01},
	{0x09, 0x29},
	{0x0A, 0x02},
};

static struct msm_camera_i2c_reg_array lm3642_high_array[] = {
	{0x0A, 0x03},
};

/* Flash High mode settings to Enable the Strobe Line */
static struct msm_camera_i2c_reg_array lm3642_high_smode_array[] = {
	{0x0A, 0x63},
};

static void __exit msm_flash_lm3642_i2c_remove(void)
{
	i2c_del_driver(&lm3642_i2c_driver);
	return;
}

static const struct of_device_id lm3642_flash_dt_match[] = {
	{.compatible = "qcom,lm3642", .data = &fctrl},
	{}
};

MODULE_DEVICE_TABLE(of, lm3642_flash_dt_match);

static const struct i2c_device_id lm3642_i2c_id[] = {
	{"qcom,lm3642", (kernel_ulong_t)&fctrl},
	{ }
};

static struct platform_driver msm_flash_lm3642_platform_driver = {
	.driver = {
		.name = "qcom,lm3642",
		.owner = THIS_MODULE,
		.of_match_table = lm3642_flash_dt_match,
	},
};

static bool lm3642_active;

static int lm3642_msm_flash_led_init(struct msm_led_flash_ctrl_t *fctrl)
{
	int32_t rc = 0;

	rc = msm_flash_led_init(fctrl);
	if (rc < 0)
		lm3642_active = false;
	else
		lm3642_active = true;

	return rc;
}

static int lm3642_msm_flash_led_release(struct msm_led_flash_ctrl_t *fctrl)
{
	int32_t rc = 0;

	if (lm3642_active) {
		rc = msm_flash_led_release(fctrl);
		lm3642_active = false;
	} else {
		pr_err("%s Redundant call, hence ignoring\n", __func__);
	}

	return rc;
}

static int lm3642_notify_sys(struct notifier_block *this, unsigned long code,
				void *unused)
{
	if (code == SYS_DOWN || code == SYS_HALT || code ==  SYS_POWER_OFF) {
		msm_flash_led_off(&fctrl);
		lm3642_msm_flash_led_release(&fctrl);
	}
	return 0;
}

static struct notifier_block lm3642_notifier = {
	.notifier_call = lm3642_notify_sys,
};

static int msm_flash_lm3642_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int32_t rc = 0;
	if (!id) {
		pr_err("msm_flash_lm3642_i2c_probe: id is NULL");
		id = lm3642_i2c_id;
	}

	rc = msm_flash_i2c_probe(client, id);
	if (!rc)
		register_reboot_notifier(&lm3642_notifier);
	return rc;
}

static struct i2c_driver lm3642_i2c_driver = {
	.id_table = lm3642_i2c_id,
	.probe  = msm_flash_lm3642_i2c_probe,
	.remove = __exit_p(msm_flash_lm3642_i2c_remove),
	.driver = {
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lm3642_flash_dt_match,
	},
};

static int32_t msm_flash_lm3642_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	match = of_match_device(lm3642_flash_dt_match, &pdev->dev);
	if (match) {
		rc = msm_flash_probe(pdev, match->data);
	} else {
		pr_err("%s: %d failed match device\n", __func__, __LINE__);
		return -EINVAL;
	}

	return rc;
}

int lm3642_flash_led_high_smode(struct msm_led_flash_ctrl_t *fctrl)
{
	int32_t rc = 0;

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->high_smode_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	return rc;
}

static struct msm_camera_i2c_client lm3642_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_camera_i2c_reg_setting lm3642_init_setting = {
	.reg_setting = lm3642_init_array,
	.size = ARRAY_SIZE(lm3642_init_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3642_off_setting = {
	.reg_setting = lm3642_off_array,
	.size = ARRAY_SIZE(lm3642_off_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3642_release_setting = {
	.reg_setting = lm3642_release_array,
	.size = ARRAY_SIZE(lm3642_release_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3642_low_setting = {
	.reg_setting = lm3642_low_array,
	.size = ARRAY_SIZE(lm3642_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3642_high_setting = {
	.reg_setting = lm3642_high_array,
	.size = ARRAY_SIZE(lm3642_high_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting lm3642_high_smode_setting = {
	.reg_setting = lm3642_high_smode_array,
	.size = ARRAY_SIZE(lm3642_high_smode_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_led_flash_reg_t lm3642_regs = {
	.init_setting = &lm3642_init_setting,
	.off_setting = &lm3642_off_setting,
	.low_setting = &lm3642_low_setting,
	.high_setting = &lm3642_high_setting,
	.release_setting = &lm3642_release_setting,
	.high_smode_setting = &lm3642_high_smode_setting,
};

static struct msm_flash_fn_t lm3642_func_tbl = {
	.flash_get_subdev_id = msm_led_i2c_trigger_get_subdev_id,
	.flash_led_config = msm_led_i2c_trigger_config,
	.flash_led_init = lm3642_msm_flash_led_init,
	.flash_led_release = lm3642_msm_flash_led_release,
	.flash_led_off = msm_flash_led_off,
	.flash_led_low = msm_flash_led_low,
	.flash_led_high = msm_flash_led_high,
	.flash_led_high_smode = lm3642_flash_led_high_smode,
};

static struct msm_led_flash_ctrl_t fctrl = {
	.flash_i2c_client = &lm3642_i2c_client,
	.reg_setting = &lm3642_regs,
	.func_tbl = &lm3642_func_tbl,
};

static int __init msm_flash_lm3642_init(void)
{
	int32_t rc = 0;
	rc = platform_driver_probe(&msm_flash_lm3642_platform_driver,
		msm_flash_lm3642_platform_probe);
	if (!rc)
		return rc;

	rc = i2c_add_driver(&lm3642_i2c_driver);
	return rc;
}

static void __exit msm_flash_lm3642_exit_module(void)
{
	unregister_reboot_notifier(&lm3642_notifier);
	if (fctrl.pdev)
		platform_driver_unregister(&msm_flash_lm3642_platform_driver);
	else
		i2c_del_driver(&lm3642_i2c_driver);
	return;
}

module_init(msm_flash_lm3642_init);
module_exit(msm_flash_lm3642_exit_module);
MODULE_DESCRIPTION("LM3642 FLASH");
MODULE_LICENSE("GPL v2");
