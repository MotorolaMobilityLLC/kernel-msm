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

#define S5K5E2_OTP_PAGE_SIZE 64
#define S5K5E2_OTP_NUM_PAGES 6
#define S5K5E2_OTP_TOTAL_SIZE 384 /* no of pages*page size */
static uint8_t s5k5e2_otp_buf[S5K5E2_OTP_TOTAL_SIZE];
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

static int32_t s5k5e2_read_otp_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	int8_t otp_page_no = 0;

	/* Read otp only once */
	if (s_ctrl->sensor_otp.otp_read) {
		pr_debug("%s: OTP block already read", __func__);
		return rc;
	}

	/* Assign OTP memory */
	s5k5e2_s_ctrl.sensor_otp.otp_info = (uint8_t *)s5k5e2_otp_buf;
	s5k5e2_s_ctrl.sensor_otp.size = sizeof(s5k5e2_otp_buf);

	/* Disable Streaming */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client,
		0x0100, 0x0,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0)
		pr_err("%s: Fail to stream off sensor\n", __func__);

	for (otp_page_no = 0; otp_page_no < S5K5E2_OTP_NUM_PAGES;
		otp_page_no++) {
		/* Make initial state */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x0A00, 0x04,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s: Unable to set Initial state !\n", __func__);
			break;
		}

		/* Set page number */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x0A02, otp_page_no,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s: Unable to set otp page no: %d !\n",
				__func__, otp_page_no);
			break;
		}

		/* set read mode */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x0A00, 0x01,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s: Unable to set read mode !\n", __func__);
			break;
		}

		/* Transfer page data from OTP to buffer */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq(
			s_ctrl->sensor_i2c_client,
			0x0A04,
			s_ctrl->sensor_otp.otp_info +
			otp_page_no*S5K5E2_OTP_PAGE_SIZE,
			S5K5E2_OTP_PAGE_SIZE);
		if (rc < 0) {
			pr_err("%s: Fail to read OTP page data: otp_page_no: %d\n",
				__func__, otp_page_no);
			break;
		}
	}

	/* Make initial state */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client,
		0x0A00, 0x04,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0)
		pr_err("%s: Unable to set Initial state !\n", __func__);

	/* Disable NVM controller */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client,
		0x0A00, 0x0,
		MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: Unable to disable NVM controller !\n",
			__func__);
	}

	/* set otp read flag to read otp only once */
	s_ctrl->sensor_otp.otp_read = 1;

	return rc;
}

static int32_t s5k5e2_get_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	/* This function isn't really needed at this time, even for
	 * factory, as we will be populating this in user space. */
	return 0;
}

static struct msm_sensor_fn_t s5k5e2_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_read_otp_info = s5k5e2_read_otp_info,
	.sensor_get_module_info = s5k5e2_get_module_info,
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
