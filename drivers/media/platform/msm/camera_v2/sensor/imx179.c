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
#include <linux/device.h>
#define IMX179_SENSOR_NAME "imx179"
DEFINE_MSM_MUTEX(imx179_mut);

static struct msm_sensor_ctrl_t imx179_s_ctrl;

#define IMX179_OTP_PAGE_SIZE 30
#define IMX179_OTP_PAGE_ADDR 0x3404
#define IMX179_OTP_NUM_PAGES 3
/*#define DEBUG_OTP_RAW_DUMP*/

static int32_t imx179_otp_validate_crc(uint8_t *imx179_otp_data)
{
	int32_t crc_match = 0;
	uint16_t crc = 0x0000;
	int i, j;
	uint32_t tmp;
	uint16_t otp_crc = imx179_otp_data[IMX179_OTP_PAGE_SIZE - 2] << 8 |
	imx179_otp_data[IMX179_OTP_PAGE_SIZE - 1];

	for (i = 0; i < (IMX179_OTP_PAGE_SIZE - 2); i++) {
		tmp = imx179_otp_data[i] & 0xff;
		for (j = 0; j < 8; j++) {
			if (((crc & 0x8000) >> 8) ^ (tmp & 0x80))
				crc = (crc << 1) ^ 0x8005;
			else
				crc = crc << 1;
			tmp <<= 1;
		}
	}
	if ((crc == otp_crc) && (otp_crc != 0)) {
		pr_debug("%s: OTP CRC pass\n", __func__);
	} else {
		pr_debug("%s: OTP CRC fail(crc = 0x%x, otp_crc = 0x%x)!\n",
			__func__, crc, otp_crc);
		crc_match = -1;
	}
	return crc_match;
}

static struct msm_sensor_power_setting imx179_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VIO,
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
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VANA,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_VIO,
		.config_val = GPIO_OUT_HIGH,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 24000000,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 30,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 1,
	},
	/* Fixme - Currently CAM_VAF is failing.Need this for Actuator */
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info imx179_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static const struct i2c_device_id imx179_i2c_id[] = {
	{IMX179_SENSOR_NAME, (kernel_ulong_t)&imx179_s_ctrl},
	{ }
};

static int32_t msm_imx179_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &imx179_s_ctrl);
}

static struct i2c_driver imx179_i2c_driver = {
	.id_table = imx179_i2c_id,
	.probe  = msm_imx179_i2c_probe,
	.driver = {
		.name = IMX179_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx179_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id imx179_dt_match[] = {
	{.compatible = "qcom,imx179", .data = &imx179_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, imx179_dt_match);

static struct platform_driver imx179_platform_driver = {
	.driver = {
		.name = "qcom,imx179",
		.owner = THIS_MODULE,
		.of_match_table = imx179_dt_match,
	},
};

static int32_t imx179_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(imx179_dt_match, &pdev->dev);

	imx179_s_ctrl.sensor_otp.otp_info = devm_kzalloc(&pdev->dev,
		IMX179_OTP_PAGE_SIZE, GFP_KERNEL);
	if (imx179_s_ctrl.sensor_otp.otp_info == NULL) {
		pr_err("%s: Unable to allocate memory for OTP!\n",
			__func__);
		return -ENOMEM;
	}
	imx179_s_ctrl.sensor_otp.size = IMX179_OTP_PAGE_SIZE;

	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init imx179_init_module(void)
{
	int32_t rc = 0;
	rc = platform_driver_probe(&imx179_platform_driver,
			imx179_platform_probe);
	if (!rc)
		return rc;
	return i2c_add_driver(&imx179_i2c_driver);
}

static void __exit imx179_exit_module(void)
{
	if (imx179_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&imx179_s_ctrl);
		platform_driver_unregister(&imx179_platform_driver);
	} else
		i2c_del_driver(&imx179_i2c_driver);
	return;
}

#ifdef DEBUG_OTP_RAW_DUMP
static void imx179_otp_raw_dump(uint8_t *data)
{
	int i = 0;
	/* Print raw OTP Data */
	for (i = 0; i < IMX179_OTP_PAGE_SIZE; i++) {
		pr_info("%s: data[%d] = 0x%02x\n",
			__func__, i, *(data+i));
	}
}
#endif

static int32_t imx179_read_otp_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	int8_t otp_page_no = 0;
	int32_t rc = 0;
	int8_t *imx179_otp_data;

	if (s_ctrl->sensor_otp.otp_read)
		return 0;

	/* Set default OTP info */
	imx179_otp_data = s_ctrl->sensor_otp.otp_info;

	/* read pages in reverse order (page2 to page0) to get correct data */
	for (otp_page_no = IMX179_OTP_NUM_PAGES-1; otp_page_no >= 0;
		otp_page_no--) {

		/* Disable Streaming */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x0100, 0x0000,
			MSM_CAMERA_I2C_WORD_DATA);
		if (rc < 0) {
			pr_err("%s: Fail to standby sensor\n", __func__);
			break;
		}

		/* Set ECC OFF */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3380, 0x08,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s: Fail to OFF ECC\n", __func__);
			break;
		}

		/* Set OTP Read */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3400, 0x01,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s: Fail set OTP page read flag\n", __func__);
			break;
		}

		/* Set OTP page */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3402, otp_page_no,
			MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0) {
			pr_err("%s: Fail to set OTP page number!\n", __func__);
			break;
		}

		/* Read and store OTP Data */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq(
			s_ctrl->sensor_i2c_client,
			IMX179_OTP_PAGE_ADDR,
			imx179_otp_data,
			IMX179_OTP_PAGE_SIZE);
		if (rc < 0) {
			pr_err("%s: Fail to read OTP page data\n",
				__func__);
			break;
		}

#ifdef DEBUG_OTP_RAW_DUMP
		/* Dump OTP */
		imx179_otp_raw_dump(imx179_otp_data);
#endif
		/* Validate OTP CRC */
		rc = imx179_otp_validate_crc(imx179_otp_data);
		if (rc < 0) {
			if (otp_page_no == 0)
				pr_err(" %s: OTP CRC (page = %d) fail!\n",
					__func__, otp_page_no);
			s_ctrl->sensor_otp.otp_crc_pass = 0;
		} else {
			pr_debug("%s: OTP CRC (page = %d) pass\n",
				__func__, otp_page_no);
			s_ctrl->sensor_otp.otp_crc_pass = 1;
			s_ctrl->sensor_otp.otp_read = 1;
			break;
		}
	}
	return 0;
}

static int32_t imx179_get_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	/* This function isn't really needed at this time, even for
	 * factory, as we will be populating this in user space.
	 */
	return 0;
}

static int32_t imx179_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid,
			MSM_CAMERA_I2C_WORD_DATA);

	if (rc < 0) {
		pr_err("%s: Unable to read chip id!\n", __func__);
		return rc;
	}

	/* consider only 12 bits of the device id register as a valid device id */
	if ((chipid & 0x0FFF) != s_ctrl->sensordata->slave_info->sensor_id) {
		pr_err("%s: chip id %x does not match expected %x\n", __func__,
				chipid, s_ctrl->sensordata->
				slave_info->sensor_id);
		return -ENODEV;
	}
	return rc;
}

static struct msm_sensor_fn_t imx179_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_get_module_info = imx179_get_module_info,
	.sensor_read_otp_info = imx179_read_otp_info,
	.sensor_match_id = imx179_sensor_match_id,
};

static struct msm_sensor_ctrl_t imx179_s_ctrl = {
	.sensor_i2c_client = &imx179_sensor_i2c_client,
	.power_setting_array.power_setting = imx179_power_setting,
	.power_setting_array.size = ARRAY_SIZE(imx179_power_setting),
	.msm_sensor_mutex = &imx179_mut,
	.sensor_v4l2_subdev_info = imx179_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx179_subdev_info),
	.func_tbl = &imx179_func_tbl,
};

module_init(imx179_init_module);
module_exit(imx179_exit_module);
MODULE_DESCRIPTION("imx179");
MODULE_LICENSE("GPL v2");
