/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include "msm_sensor.h"
#include <linux/crc16.h>
#include <linux/bitrev.h>
#define S5K5E2_SENSOR_NAME "s5k5e2"
DEFINE_MSM_MUTEX(s5k5e2_mut);

static struct msm_sensor_ctrl_t s5k5e2_s_ctrl;

static struct msm_sensor_power_setting s5k5e2_power_setting[] = {
{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_VANA,
	.config_val = GPIO_OUT_LOW,
	.delay = 1,
},
{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_RESET,
	.config_val = GPIO_OUT_LOW,
	.delay = 1,
},
{
	.seq_type = SENSOR_VREG,
	.seq_val = CAM_VDIG,
	.config_val = 0,
	.delay = 1,
},
{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_VANA,
	.config_val = GPIO_OUT_HIGH,
	.delay = 1,
},
{
	.seq_type = SENSOR_GPIO,
	.seq_val = SENSOR_GPIO_RESET,
	.config_val = GPIO_OUT_HIGH,
	.delay = 30,
},
{
	.seq_type = SENSOR_VREG,
	.seq_val = CAM_VAF,
	.config_val = 0,
	.delay = 1,
},
{
	.seq_type = SENSOR_CLK,
	.seq_val = SENSOR_CAM_MCLK,
	.config_val = 24000000,
	.delay = 1,
},
{
	.seq_type = SENSOR_I2C_MUX,
	.seq_val = 0,
	.config_val = 0,
	.delay = 1,
},
};

static struct v4l2_subdev_info s5k5e2_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id s5k5e2_i2c_id[] = {
	{S5K5E2_SENSOR_NAME, (kernel_ulong_t)&s5k5e2_s_ctrl},
	{ }
};

static int32_t msm_s5k5e2_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t rc = 0;
	rc = msm_sensor_i2c_probe(client, id, &s5k5e2_s_ctrl);
	if (!rc)
		pr_info("%s:%d s5k5e2 probe succeeded\n", __func__, __LINE__);
	else
		pr_err("%s:%d s5k5e2 probe failed\n", __func__, __LINE__);
	return rc;
}

static struct i2c_driver s5k5e2_i2c_driver = {
	.id_table = s5k5e2_i2c_id,
	.probe  = msm_s5k5e2_i2c_probe,
	.driver = {
		.name = S5K5E2_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k5e2_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id s5k5e2_dt_match[] = {
	{.compatible = "qcom,s5k5e2", .data = &s5k5e2_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, s5k5e2_dt_match);

static struct platform_driver s5k5e2_platform_driver = {
	.driver = {
		.name = "qcom,s5k5e2",
		.owner = THIS_MODULE,
		.of_match_table = s5k5e2_dt_match,
	},
};

static int32_t s5k5e2_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(s5k5e2_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init s5k5e2_init_module(void)
{
	int32_t rc = 0;
	rc = platform_driver_probe(&s5k5e2_platform_driver,
		s5k5e2_platform_probe);
	if (!rc)
		return rc;
	rc = i2c_add_driver(&s5k5e2_i2c_driver);
	return rc;
}

static void __exit s5k5e2_exit_module(void)
{
	if (s5k5e2_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&s5k5e2_s_ctrl);
		platform_driver_unregister(&s5k5e2_platform_driver);
	} else
		i2c_del_driver(&s5k5e2_i2c_driver);
	return;
}

static struct msm_sensor_fn_t s5k5e2_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
};

static struct msm_sensor_ctrl_t s5k5e2_s_ctrl = {
	.sensor_i2c_client = &s5k5e2_sensor_i2c_client,
	.power_setting_array.power_setting = s5k5e2_power_setting,
	.power_setting_array.size = ARRAY_SIZE(s5k5e2_power_setting),
	.msm_sensor_mutex = &s5k5e2_mut,
	.sensor_v4l2_subdev_info = s5k5e2_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k5e2_subdev_info),
	.func_tbl = &s5k5e2_func_tbl,
};

module_init(s5k5e2_init_module);
module_exit(s5k5e2_exit_module);
MODULE_DESCRIPTION("s5k5e2");
MODULE_LICENSE("GPL v2");
