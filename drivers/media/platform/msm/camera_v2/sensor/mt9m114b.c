/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#define MT9M114B_SENSOR_NAME "mt9m114b"
DEFINE_MSM_MUTEX(mt9m114b_mut);

#undef CDBG
#ifdef MT9M114B_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

static struct msm_sensor_ctrl_t mt9m114b_s_ctrl;

static struct msm_sensor_power_setting mt9m114b_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VDIG,
		.config_val = GPIO_OUT_HIGH,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 50,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info mt9m114b_subdev_info[] = {
	{
		.code		= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.fmt		= 1,
		.order		= 0,
	},
};

static const struct i2c_device_id mt9m114b_i2c_id[] = {
	{MT9M114B_SENSOR_NAME, (kernel_ulong_t)&mt9m114b_s_ctrl},
	{ }
};

static int32_t msm_mt9m114b_i2c_probe(struct i2c_client *client,
       const struct i2c_device_id *id)
{
       return msm_sensor_i2c_probe(client, id, &mt9m114b_s_ctrl);
}

static struct i2c_driver mt9m114b_i2c_driver = {
	.id_table = mt9m114b_i2c_id,
	.probe  = msm_mt9m114b_i2c_probe,
	.driver = {
		.name = MT9M114B_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client mt9m114b_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id mt9m114b_dt_match[] = {
	{.compatible = "qcom,mt9m114b", .data = &mt9m114b_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, mt9m114b_dt_match);

static struct platform_driver mt9m114b_platform_driver = {
	.driver = {
		.name = "qcom,mt9m114b",
		.owner = THIS_MODULE,
		.of_match_table = mt9m114b_dt_match,
	},
};

static int mt9m114b_platform_probe(struct platform_device *pdev)
{
	int rc = 0;
	const struct of_device_id *match;

	CDBG("%s E\n", __func__);
	match = of_match_device(mt9m114b_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	CDBG("%s: X, rc = %d\n", __func__, rc);
	return rc;
}

static int __init mt9m114b_init_module(void)
{
	int rc = 0;
	CDBG("%s E\n", __func__);
	rc = platform_driver_probe(&mt9m114b_platform_driver,
		mt9m114b_platform_probe);
	if (!rc) {
		CDBG("%s: X, rc = %d\n", __func__, rc);
		return rc;
	}
	return i2c_add_driver(&mt9m114b_i2c_driver);
}

static void __exit mt9m114b_exit_module(void)
{
	if (mt9m114b_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&mt9m114b_s_ctrl);
		platform_driver_unregister(&mt9m114b_platform_driver);
	} else {
		i2c_del_driver(&mt9m114b_i2c_driver);
	}
	return;
}

static struct msm_sensor_ctrl_t mt9m114b_s_ctrl = {
	.sensor_i2c_client = &mt9m114b_sensor_i2c_client,
	.power_setting_array.power_setting = mt9m114b_power_setting,
	.power_setting_array.size = ARRAY_SIZE(mt9m114b_power_setting),
	.msm_sensor_mutex = &mt9m114b_mut,
	.sensor_v4l2_subdev_info = mt9m114b_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(mt9m114b_subdev_info),
};

module_init(mt9m114b_init_module);
module_exit(mt9m114b_exit_module);
MODULE_DESCRIPTION("mt9m114b");
MODULE_LICENSE("GPL v2");
