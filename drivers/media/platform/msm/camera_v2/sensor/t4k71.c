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

#define T4K71_SENSOR_NAME "t4k71"
DEFINE_MSM_MUTEX(t4k71_mut);

#define T4K71_OTP_PAGE_SIZE 8
#define T4K71_OTP_NUM_PAGES 4
#define T4K71_OTP_TOTAL_SIZE 32 /* no of pages*page size */
static uint8_t t4k71_otp_buf[T4K71_OTP_TOTAL_SIZE];

static struct msm_sensor_ctrl_t t4k71_s_ctrl;
static struct msm_sensor_power_setting t4k71_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
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
		.delay = 10,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
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

static struct v4l2_subdev_info t4k71_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static int32_t msm_t4k71_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &t4k71_s_ctrl);
}

static const struct i2c_device_id t4k71_i2c_id[] = {
	{T4K71_SENSOR_NAME, (kernel_ulong_t)&t4k71_s_ctrl},
	{ }
};

static struct i2c_driver t4k71_i2c_driver = {
	.id_table = t4k71_i2c_id,
	.probe  = msm_t4k71_i2c_probe,
	.driver = {
		.name = T4K71_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client t4k71_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int32_t t4k71_read_otp_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	int8_t otp_page_no;
	int32_t rc = 0;

	/* Check otp already read or not */
	if (s_ctrl->sensor_otp.otp_read)
		return rc;

	/* Assign OTP memory */
	t4k71_s_ctrl.sensor_otp.otp_info = (uint8_t *)t4k71_otp_buf;
	t4k71_s_ctrl.sensor_otp.size = sizeof(t4k71_otp_buf);

	/* Read otp page */
	for (otp_page_no = 0; otp_page_no < T4K71_OTP_NUM_PAGES;
		otp_page_no++) {

		/* OTP Read Enable */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3500, 0x01,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s: Fail to enable otp\n", __func__);
			break;
		}

		/* Select OTP page */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3503, otp_page_no,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s: Fail to set OTP page number!\n", __func__);
			break;
		}

		/* Start OTP access */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3500, 0x81,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s: Fail to access otp status\n", __func__);
			break;
		}

		/* Read and store OTP Data */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq(
			s_ctrl->sensor_i2c_client,
			0x3504,
			s_ctrl->sensor_otp.otp_info +
			otp_page_no*T4K71_OTP_PAGE_SIZE,
			T4K71_OTP_PAGE_SIZE);
		if (rc < 0) {
			pr_err("%s: Fail to read OTP page data\n",
				__func__);
			break;
		}
	}

	/* Set otp read done flag */
	s_ctrl->sensor_otp.otp_read = 1;

	/* OTP Read Disable */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client,
		0x3500, 0x0,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0)
		pr_err("%s: OTP Disable failed\n", __func__);

	return rc;
}

static int32_t t4k71_get_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	/* This function isn't really needed at this time, even for
	 * factory, as we will be populating this in user space.
	 */
	return 0;
}


static struct msm_sensor_fn_t t4k71_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_get_module_info = t4k71_get_module_info,
	.sensor_read_otp_info = t4k71_read_otp_info,
};

static struct msm_sensor_ctrl_t t4k71_s_ctrl = {
	.sensor_i2c_client = &t4k71_sensor_i2c_client,
	.power_setting_array.power_setting = t4k71_power_setting,
	.power_setting_array.size = ARRAY_SIZE(t4k71_power_setting),
	.msm_sensor_mutex = &t4k71_mut,
	.sensor_v4l2_subdev_info = t4k71_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(t4k71_subdev_info),
	.func_tbl = &t4k71_func_tbl,
};

static const struct of_device_id t4k71_dt_match[] = {
	{.compatible = "qcom,t4k71", .data = &t4k71_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, t4k71_dt_match);

static struct platform_driver t4k71_platform_driver = {
	.driver = {
		.name = "qcom,t4k71",
		.owner = THIS_MODULE,
		.of_match_table = t4k71_dt_match,
	},
};

static int32_t t4k71_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	match = of_match_device(t4k71_dt_match, &pdev->dev);

	rc = msm_sensor_platform_probe(pdev, match->data);
	if (!rc)
		pr_info("%s:%d Success", __func__, __LINE__);
	return rc;
}

static int __init t4k71_init_module(void)
{
	int32_t rc = 0;

	rc = platform_driver_probe(&t4k71_platform_driver,
		t4k71_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&t4k71_i2c_driver);
}

static void __exit t4k71_exit_module(void)
{
	if (t4k71_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&t4k71_s_ctrl);
		platform_driver_unregister(&t4k71_platform_driver);
	} else
		i2c_del_driver(&t4k71_i2c_driver);
	return;
}

module_init(t4k71_init_module);
module_exit(t4k71_exit_module);
MODULE_DESCRIPTION("t4k71");
MODULE_LICENSE("GPL v2");
