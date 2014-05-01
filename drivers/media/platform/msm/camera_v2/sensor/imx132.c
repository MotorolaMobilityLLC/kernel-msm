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
#include "msm_sensor.h"

#define IMX132_SENSOR_NAME "imx132"
DEFINE_MSM_MUTEX(imx132_mut);

#define IMX132_OTP_SIZE 3
#define IMX132_OTP_ADDR1 0x3517
#define IMX132_OTP_ADDR2 0x351E
/* #define DEBUG_OTP_RAW_DUMP */

static struct msm_sensor_ctrl_t imx132_s_ctrl;

static struct msm_sensor_power_setting imx132_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VIO,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VIO,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 10,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info imx132_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static int32_t msm_imx132_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &imx132_s_ctrl);
}

static const struct i2c_device_id imx132_i2c_id[] = {
	{IMX132_SENSOR_NAME, (kernel_ulong_t)&imx132_s_ctrl},
	{ }
};

static struct i2c_driver imx132_i2c_driver = {
	.id_table = imx132_i2c_id,
	.probe  = msm_imx132_i2c_probe,
	.driver = {
		.name = IMX132_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx132_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

#ifdef DEBUG_OTP_RAW_DUMP
static void imx132_otp_raw_dump(uint8_t *data)
{
	int i = 0;
	/* Print raw OTP Data */
	for (i = 0; i < IMX132_OTP_SIZE; i++) {
		pr_info("%s: data[%d] = 0x%02x\n",
			__func__, i, *(data+i));
	}
}
#endif

static int32_t imx132_read_otp_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint8_t *imx132_otp_data;
	uint16_t data;
	/* Set default OTP info */

	if (s_ctrl->sensor_otp.otp_read)
		return rc;

	imx132_otp_data = s_ctrl->sensor_otp.otp_info;

	/* Read OTP Data */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
		s_ctrl->sensor_i2c_client,
		IMX132_OTP_ADDR1,
		&data, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: Fail to read OTP register data\n",
			__func__);
		return rc;
	}
	/* store 0x3517 data */
	*imx132_otp_data = (uint8_t)data;

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
		s_ctrl->sensor_i2c_client,
		IMX132_OTP_ADDR2,
		&data, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: Fail to read OTP register data\n",
			__func__);
		return rc;
	}
	/* store 0x351E data */
	*(imx132_otp_data+1) = (uint8_t)(data >> 8);
	/* store 0x351F data */
	*(imx132_otp_data+2) = (uint8_t)(data & 0xFF);

#ifdef DEBUG_OTP_RAW_DUMP
	/* Dump OTP */
	imx132_otp_raw_dump(imx132_otp_data);
#endif
	s_ctrl->sensor_otp.otp_read = 1;
	return rc;
}

static int32_t imx132_get_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	/* This function isn't really needed at this time, even for
	 * factory, as we will be populating this in user space.
	 */
	return 0;
}


static struct msm_sensor_fn_t imx132_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_read_otp_info = imx132_read_otp_info,
	.sensor_get_module_info = imx132_get_module_info,
};

static struct msm_sensor_ctrl_t imx132_s_ctrl = {
	.sensor_i2c_client = &imx132_sensor_i2c_client,
	.power_setting_array.power_setting = imx132_power_setting,
	.power_setting_array.size = ARRAY_SIZE(imx132_power_setting),
	.msm_sensor_mutex = &imx132_mut,
	.sensor_v4l2_subdev_info = imx132_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx132_subdev_info),
	.func_tbl = &imx132_func_tbl,
};

static const struct of_device_id imx132_dt_match[] = {
	{.compatible = "qcom,imx132", .data = &imx132_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, imx132_dt_match);

static struct platform_driver imx132_platform_driver = {
	.driver = {
		.name = "qcom,imx132",
		.owner = THIS_MODULE,
		.of_match_table = imx132_dt_match,
	},
};

static int32_t imx132_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	match = of_match_device(imx132_dt_match, &pdev->dev);

	imx132_s_ctrl.sensor_otp.otp_info = devm_kzalloc(&pdev->dev,
		IMX132_OTP_SIZE, GFP_KERNEL);
	if (imx132_s_ctrl.sensor_otp.otp_info == NULL) {
		pr_err("%s: Unable to allocate memory for OTP!\n",
			__func__);
		return -ENOMEM;
	}
	imx132_s_ctrl.sensor_otp.size = IMX132_OTP_SIZE;

	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init imx132_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_probe(&imx132_platform_driver,
		imx132_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&imx132_i2c_driver);
}

static void __exit imx132_exit_module(void)
{
	if (imx132_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&imx132_s_ctrl);
		platform_driver_unregister(&imx132_platform_driver);
	} else
		i2c_del_driver(&imx132_i2c_driver);
	return;
}

module_init(imx132_init_module);
module_exit(imx132_exit_module);
MODULE_DESCRIPTION("imx132");
MODULE_LICENSE("GPL v2");
