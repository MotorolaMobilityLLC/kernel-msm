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
#include <linux/crc16.h>
#define AR0543_SENSOR_NAME "ar0543"
DEFINE_MSM_MUTEX(ar0543_mut);

static struct msm_sensor_ctrl_t ar0543_s_ctrl;

#define AR0543_OTP_MAX_ENTRIES 4
#define AR0543_OTP_MOD_DATA_SIZE 17
#define AR0543_OTP_MOD_DATA_OFFSET 0
#define AR0543_OTP_MOD_DATA_RECORD_ENTRY_BASE 0x30
#define AR0543_OTP_CAL_DATA_RECORD_ENTRY_BASE 0x31
#define AR0543_OTP_CAL_DATA_SIZE 15
#define AR0543_OTP_CAL_DATA_OFFSET 17
#define AR0543_OTP_SIZE (AR0543_OTP_MOD_DATA_SIZE+AR0543_OTP_CAL_DATA_SIZE)
#define AR0543_OTP_CRC_SIZE (AR0543_OTP_SIZE-1)
#define AR0543_OTP_CRC_OFFSET 31
#define AR0543_OTP_CRC_INIT 0
#define AR0543_SN_SIZE 4

static uint16_t ar0543_otp[AR0543_OTP_SIZE];

static struct otp_info_t ar0543_otp_info;

static struct msm_sensor_power_setting ar0543_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_PWREN,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_PWREN,
		.config_val = GPIO_OUT_HIGH,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VAF,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 100,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
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
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct v4l2_subdev_info ar0543_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id ar0543_i2c_id[] = {
	{AR0543_SENSOR_NAME, (kernel_ulong_t)&ar0543_s_ctrl},
	{ }
};

static int32_t msm_ar0543_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ar0543_s_ctrl);
}

static struct i2c_driver ar0543_i2c_driver = {
	.id_table = ar0543_i2c_id,
	.probe  = msm_ar0543_i2c_probe,
	.driver = {
		.name = AR0543_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ar0543_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ar0543_dt_match[] = {
	{.compatible = "qcom,ar0543", .data = &ar0543_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ar0543_dt_match);

static struct platform_driver ar0543_platform_driver = {
	.driver = {
		.name = "qcom,ar0543",
		.owner = THIS_MODULE,
		.of_match_table = ar0543_dt_match,
	},
};

static int32_t ar0543_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(ar0543_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init ar0543_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&ar0543_platform_driver,
		ar0543_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	rc = i2c_add_driver(&ar0543_i2c_driver);
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
    return rc;
}

static void __exit ar0543_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (ar0543_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ar0543_s_ctrl);
		platform_driver_unregister(&ar0543_platform_driver);
	} else
		i2c_del_driver(&ar0543_i2c_driver);
	return;
}

#if 0
static void ar0543_otp_raw_dump(uint16_t *ar0543_otp_ptr)
{
	int i = 0;

	/* Print raw words */
	for (i = 0; i < AR0543_OTP_SIZE; i++) {
		pr_info("%s: ar0543_otp[%d] = 0x%x\n",
				__func__, i, *(ar0543_otp_ptr+i));
	}
}
#endif

static int32_t ar0543_otp_validate_crc(uint16_t *ar0543_otp_ptr)
{
	int32_t rc = 0;
	u16 crc_otp = 0;
	u16 crc = 0;

	/* Read OTP CRC */
	crc_otp = *(ar0543_otp_ptr+AR0543_OTP_CRC_SIZE);

	/* Check CRC16 */
	crc = crc16(AR0543_OTP_CRC_INIT, (u8 *)ar0543_otp_ptr,
			(AR0543_OTP_CRC_SIZE)*2);
	if (crc == crc_otp) {
		pr_debug("%s: OTP CRC pass\n", __func__);
	} else {
		pr_warn("%s: OTP CRC fail(crc = 0x%x, crc_otp = 0x%x)!\n",
				__func__, crc, crc_otp);
		rc = -1;
	}

	return rc;
}

static int32_t ar0543_auto_otp_read(struct msm_sensor_ctrl_t *s_ctrl,
	uint8_t otp_rt)
{
	int32_t rc = 0;
	uint16_t retry = 0;
	uint16_t otp_control_reg = 0;

	/* Disable Streaming */
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x301A, 0x0610,
			MSM_CAMERA_I2C_WORD_DATA);

	/* OTP Timing Parameters */
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3134, 0xCD95,
			MSM_CAMERA_I2C_WORD_DATA);

	/* OTP Record Type */
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x304C, otp_rt<<8,
			MSM_CAMERA_I2C_WORD_DATA);

	/* OTP Record Multiple Read */
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x304A, 0x0000,
			MSM_CAMERA_I2C_WORD_DATA);

	/* Initiate Automatic OTP Read Sequence */
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x304A, 0x0010,
			MSM_CAMERA_I2C_WORD_DATA);

	/* Poll OTP Read Record Type Complete flag */
	do {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				0x304A, &otp_control_reg,
				MSM_CAMERA_I2C_WORD_DATA);
		if (0x20 == (otp_control_reg & 0x20)) {
			pr_debug("%s: Finished reading OTP rt = 0x%x\n",
					__func__, otp_rt);
			break;
		}
		usleep_range(5000, 5100);
		retry++;
	} while (retry < 10);

	/* Check OTP Read Record Type Success flag */
	if (0x40 == (otp_control_reg & 0x40)) {
		pr_debug("%s: Located OTP rt = 0x%x\n",
				__func__, otp_rt);
	} else {
		pr_warn("%s: Unable to locate OTP rt = 0x%x!\n",
				__func__, otp_rt);
		rc = -1;
	}

	return rc;
}

static int32_t ar0543_read_otp(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t *ar0543_otp_ptr)
{
	int32_t rc = 0;
	uint8_t i = 0;
	uint8_t otp_rt = 0;
	uint16_t j = 0;

	/* For the maximum number of OTP entries */
	for (i = 0; i < AR0543_OTP_MAX_ENTRIES; i++) {
		/* Initialize OTP array to clear old data */
		for (j = 0; j < AR0543_OTP_SIZE; j++)
			*(ar0543_otp_ptr+j) = 0xFFFF;

		/* Auto read OTP Module Data */
		otp_rt = AR0543_OTP_MOD_DATA_RECORD_ENTRY_BASE+i*2;
		if (ar0543_auto_otp_read(s_ctrl, otp_rt) == 0) {
			/* Read and store OTP Module Data */
		    for (j = 0; j < AR0543_OTP_MOD_DATA_SIZE; j++) {
			s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client, (0x3800+j*2),
				(ar0543_otp_ptr+AR0543_OTP_MOD_DATA_OFFSET+j),
				MSM_CAMERA_I2C_WORD_DATA);
		    }
		} else {
			pr_warn("%s: Auto read OTP Mod Data rt = 0x%x fail!\n",
					__func__, otp_rt);
		}

		/* Auto read OTP Calibration Data */
		otp_rt = AR0543_OTP_CAL_DATA_RECORD_ENTRY_BASE+i*2;
		if (ar0543_auto_otp_read(s_ctrl, otp_rt) == 0) {
			/* Read and store OTP Calibration Data */
		    for (j = 0; j < AR0543_OTP_CAL_DATA_SIZE; j++) {
			s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client, (0x3800+j*2),
				(ar0543_otp_ptr+AR0543_OTP_CAL_DATA_OFFSET+j),
				MSM_CAMERA_I2C_WORD_DATA);
		    }
		} else {
			pr_warn("%s: Auto read OTP Cal Data rt = 0x%x fail!\n",
					__func__, otp_rt);
		}


		/* Validate OTP CRC */
		rc = ar0543_otp_validate_crc(ar0543_otp_ptr);
		if (rc < 0) {
			pr_warn("%s: OTP CRC (record group = %d) fail!\n",
					__func__, i);
		} else {
			pr_debug("%s: OTP CRC (record group = %d) pass\n",
					__func__, i);
			break;
		}
	}

#if 0
	/* Dump OTP */
	ar0543_otp_raw_dump(ar0543_otp_ptr);
#endif
	return rc;
}

static int32_t ar0543_read_otp_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	int i = 0;
	uint16_t hw_rev = 0;
	uint16_t sn[4];

	/* Read OTP */
	rc = ar0543_read_otp(s_ctrl, ar0543_otp);
	if (rc < 0) {
		pr_err("%s: Read OTP fail!\n", __func__);
		ar0543_otp_info.size = 0;
	} else {
		pr_debug("%s: Read OTP pass\n", __func__);
		ar0543_otp_info.size = sizeof(ar0543_otp);
		ar0543_otp_info.otp_info = (uint8_t *)ar0543_otp;
	}

	/* Read HW Revision */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x0002, &hw_rev,
			MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: Read hw_rev fail!\n", __func__);
		ar0543_otp_info.hw_rev = 0x0000;
	} else {
		pr_debug("%s: Read hw_rev pass\n", __func__);
		ar0543_otp_info.hw_rev = hw_rev;
		pr_debug("%s: hw_rev=0x%x\n", __func__, hw_rev);
	}

	/* Read Serial Number */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client, 0x301A, 0x0630,
			MSM_CAMERA_I2C_WORD_DATA);
	rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x31f4, &sn[0],
			MSM_CAMERA_I2C_WORD_DATA);
	rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x31f6, &sn[1],
			MSM_CAMERA_I2C_WORD_DATA);
	rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x31f8, &sn[2],
			MSM_CAMERA_I2C_WORD_DATA);
	rc += s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x31fa, &sn[3],
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: Read sn fail!\n", __func__);
		for (i = 0; i < AR0543_SN_SIZE; i++)
			ar0543_otp_info.sn[i] = 0x0000;
	} else {
		pr_debug("%s: Read sn pass\n", __func__);
		for (i = 0; i < AR0543_SN_SIZE; i++) {
			pr_debug("%s: sn[%d]=0x%04x\n", __func__, i, sn[i]);
			ar0543_otp_info.sn[i] = sn[i];
		}
	}

	return rc;
}

static int32_t ar0543_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	pr_info("%s: read id: %x expected id %x:\n", __func__, chipid,
		s_ctrl->sensordata->slave_info->sensor_id);
	if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
		pr_err("msm_sensor_match_id chip id does not match\n");
		return -ENODEV;
	} else {
		/* Read OTP info */
		ar0543_read_otp_info(s_ctrl);
	}
	return rc;
}

static int32_t ar0543_get_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	int i;

	if (ar0543_otp_info.size > 0) {
		s_ctrl->sensor_otp.otp_info = ar0543_otp_info.otp_info;
		s_ctrl->sensor_otp.size = ar0543_otp_info.size;
		s_ctrl->sensor_otp.hw_rev = ar0543_otp_info.hw_rev;
		s_ctrl->sensor_otp.asic_rev = ar0543_otp_info.asic_rev;
		for (i = 0; i < AR0543_SN_SIZE; i++)
			s_ctrl->sensor_otp.sn[i] = ar0543_otp_info.sn[i];
		return 0;
	} else {
		pr_err("%s: Unable to get module info as otp failed!\n",
				__func__);
		return -EINVAL;
	}
}

static struct msm_sensor_fn_t ar0543_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = ar0543_match_id,
	.sensor_get_module_info = ar0543_get_module_info,
};

static struct msm_sensor_ctrl_t ar0543_s_ctrl = {
	.sensor_i2c_client = &ar0543_sensor_i2c_client,
	.power_setting_array.power_setting = ar0543_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ar0543_power_setting),
	.msm_sensor_mutex = &ar0543_mut,
	.sensor_v4l2_subdev_info = ar0543_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ar0543_subdev_info),
	.func_tbl = &ar0543_func_tbl,
};

module_init(ar0543_init_module);
module_exit(ar0543_exit_module);
MODULE_DESCRIPTION("ar0543");
MODULE_LICENSE("GPL v2");
