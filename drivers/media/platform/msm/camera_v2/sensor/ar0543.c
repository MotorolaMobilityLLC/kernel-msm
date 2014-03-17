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
#include <linux/bitrev.h>
#define AR0543_SENSOR_NAME "ar0543"
DEFINE_MSM_MUTEX(ar0543_mut);

static struct msm_sensor_ctrl_t ar0543_s_ctrl;

/*#define DEBUG_OTP_RAW_DUMP*/
#define DEBUG_OTP_LSC_DUMP

#define AR0543_SN_SIZE 4
#define AR0543_SN_ADDR 0x31f4
#define AR0543_OTP_SIZE 256
#define AR0543_OTP_ADDR 0x3800
#define AR0543_LSC_SIZE 106
#define AR0543_LSC_EN_SIZE 2
#define AR0543_LSC_EN_ADDR 0x3780
#define AR0543_LSC_PX_SIZE 40
#define AR0543_LSC_P0_ADDR 0x3600
#define AR0543_LSC_P1_ADDR 0x3640
#define AR0543_LSC_P2_ADDR 0x3680
#define AR0543_LSC_P3_ADDR 0x36C0
#define AR0543_LSC_P4_ADDR 0x3700
#define AR0543_LSC_XY_SIZE 4
#define AR0543_LSC_XY_ADDR 0x3782
#define AR0543_LSC_Q5_SIZE 8
#define AR0543_LSC_Q5_ADDR 0x37C0

enum sensor_otp_cal_rev_t {
	AR0543_OTP_CAL_REV_1,
	AR0543_OTP_CAL_REV_2,
	AR0543_OTP_CAL_REV_3,
	AR0543_OTP_CAL_REV_DEF,
	AR0543_OTP_CAL_REV_SIZE,
	AR0543_OTP_CAL_REV_MAX = AR0543_OTP_CAL_REV_DEF - 1,
};

struct module_otp_t {
	uint16_t size;
	uint16_t max_entries;
	uint16_t mod_size;
	uint16_t mod_os;
	uint8_t mod_record_entry_base;
	uint16_t cal_size;
	uint16_t cal_os;
	uint8_t cal_record_entry_base;
	uint16_t cal_lsc_en;
	uint16_t cal_lsc_os;
	uint16_t cal_lsc_p0_os;
	uint16_t cal_lsc_p1_os;
	uint16_t cal_lsc_p2_os;
	uint16_t cal_lsc_p3_os;
	uint16_t cal_lsc_p4_os;
	uint16_t cal_lsc_xy_os;
	uint16_t cal_lsc_q5_os;
};

struct module_otp_rev_t {
	struct module_otp_t n[AR0543_OTP_CAL_REV_SIZE];
};

static struct module_otp_rev_t ar0543_otp_rev = {
	.n[AR0543_OTP_CAL_REV_1] = {
		.size = 32,
		.max_entries = 4,
		.mod_size = 17,
		.mod_os = 0,
		.mod_record_entry_base = 0x30,
		.cal_size = 15,
		.cal_os = 17,
		.cal_record_entry_base = 0x31,
		.cal_lsc_en = 0,
		.cal_lsc_os = 0,
		.cal_lsc_p0_os = 0,
		.cal_lsc_p1_os = 0,
		.cal_lsc_p2_os = 0,
		.cal_lsc_p3_os = 0,
		.cal_lsc_p4_os = 0,
		.cal_lsc_xy_os = 0,
		.cal_lsc_q5_os = 0,
	},
	.n[AR0543_OTP_CAL_REV_2] = {
		.size = 133,
		.max_entries = 3,
		.mod_size = 18,
		.mod_os = 0,
		.mod_record_entry_base = 0x30,
		.cal_size = 115,
		.cal_os = 18,
		.cal_record_entry_base = 0x31,
		.cal_lsc_en = 1,
		.cal_lsc_os = 26,
		.cal_lsc_p0_os = 0,
		.cal_lsc_p1_os = 20,
		.cal_lsc_p2_os = 40,
		.cal_lsc_p3_os = 60,
		.cal_lsc_p4_os = 80,
		.cal_lsc_xy_os = 100,
		.cal_lsc_q5_os = 102,
	},
	.n[AR0543_OTP_CAL_REV_3] = {
		.size = 135,
		.max_entries = 3,
		.mod_size = 18,
		.mod_os = 0,
		.mod_record_entry_base = 0x30,
		.cal_size = 117,
		.cal_os = 18,
		.cal_record_entry_base = 0x31,
		.cal_lsc_en = 1,
		.cal_lsc_os = 28,
		.cal_lsc_p0_os = 0,
		.cal_lsc_p1_os = 20,
		.cal_lsc_p2_os = 40,
		.cal_lsc_p3_os = 60,
		.cal_lsc_p4_os = 80,
		.cal_lsc_xy_os = 100,
		.cal_lsc_q5_os = 102,
	},
	.n[AR0543_OTP_CAL_REV_DEF] = {
		.size = 0,
		.max_entries = 0,
		.mod_size = 0,
		.mod_os = 0,
		.mod_record_entry_base = 0,
		.cal_size = 0,
		.cal_os = 0,
		.cal_record_entry_base = 0,
		.cal_lsc_en = 1,
		.cal_lsc_os = 0,
		.cal_lsc_p0_os = 0,
		.cal_lsc_p1_os = 20,
		.cal_lsc_p2_os = 40,
		.cal_lsc_p3_os = 60,
		.cal_lsc_p4_os = 80,
		.cal_lsc_xy_os = 100,
		.cal_lsc_q5_os = 102,
	},
};

static uint16_t ar0543_otp[AR0543_OTP_SIZE];

static struct otp_info_t ar0543_otp_info;

static uint16_t ar0543_otp_lsc_def[AR0543_LSC_SIZE] = {
	0x5012,
	0x2A8F,
	0x5075,
	0x4EC8,
	0x51CD,
	0xF012,
	0xCA8A,
	0x302B,
	0xEC4C,
	0xD190,
	0x9012,
	0xCCDF,
	0xAF10,
	0xCEAD,
	0x50AA,
	0xB014,
	0xECCA,
	0xB103,
	0xECBA,
	0x91E3,
	0x0CBC,
	0x6DBF,
	0xEE33,
	0xEC2D,
	0x6CA6,
	0x8AC7,
	0xAE46,
	0x2E55,
	0xAF82,
	0xEFCE,
	0x8C12,
	0xAE1A,
	0xEF0F,
	0x2F93,
	0xD0CA,
	0x0B87,
	0x4EB6,
	0x8F5B,
	0xAF10,
	0x10AB,
	0xF10B,
	0xEDAE,
	0x7391,
	0xAD3B,
	0x527C,
	0x1116,
	0x4EC0,
	0x9380,
	0x7081,
	0x3278,
	0x3055,
	0x8C2F,
	0x12ED,
	0x701D,
	0x1327,
	0x510A,
	0xEDBD,
	0xB396,
	0x6ECF,
	0xD30D,
	0x4E4A,
	0x4C17,
	0x6FB4,
	0xF01C,
	0xEF2B,
	0x0F3E,
	0x4EEC,
	0x11B7,
	0xEF11,
	0xB21C,
	0xEA3B,
	0x6DDF,
	0xF1A3,
	0xF02B,
	0x9267,
	0xED0B,
	0x8F56,
	0x918A,
	0xCFDC,
	0x1213,
	0x529C,
	0x6FD4,
	0x5319,
	0x317E,
	0xD1C4,
	0x929E,
	0xEBCD,
	0x331D,
	0x521C,
	0xB38E,
	0x3296,
	0xEFBB,
	0xD403,
	0x2EEF,
	0xF4D1,
	0xD296,
	0x6F9E,
	0x530D,
	0x921D,
	0x11AE,
	0x6C05,
	0x8C03,
	0x6C41,
	0x6C1F,
	0xCC20,
	0x6C2C,
};

static struct msm_sensor_power_setting ar0543_power_setting[] = {
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_PWREN2,
		.config_val = GPIO_OUT_LOW,
		.delay = 0,
	},
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
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_PWREN2,
		.config_val = GPIO_OUT_HIGH,
		.delay = 2,
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

#ifdef DEBUG_OTP_RAW_DUMP
static void ar0543_otp_raw_dump(uint16_t *data, uint16_t size)
{
	int i = 0;

	/* Print raw words */
	for (i = 0; i < size; i++) {
		pr_info("%s: data[%d] = 0x%04x\n",
				__func__, i, *(data+i));
	}
}
#else
static inline void ar0543_otp_raw_dump(uint16_t *data, uint16_t size) {; }
#endif

#ifdef DEBUG_OTP_LSC_DUMP
static void ar0543_otp_lsc_dump(struct msm_sensor_ctrl_t *s_ctrl)
{
	int i = 0;
	uint16_t data;

	pr_warn("%s - Dumping LSC values from sensor...\n", __func__);

	/* Read and print LSC registers */
	for (i = 0; i < AR0543_LSC_PX_SIZE/2; i++) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				AR0543_LSC_P0_ADDR+i*2, &data,
				MSM_CAMERA_I2C_WORD_DATA);
		pr_err("%s - p0[%d] = 0x%04x\n", __func__, i, data);
	}
	for (i = 0; i < AR0543_LSC_PX_SIZE/2; i++) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				AR0543_LSC_P1_ADDR+i*2, &data,
				MSM_CAMERA_I2C_WORD_DATA);
		pr_err("%s - p1[%d] = 0x%04x\n", __func__, i, data);
	}
	for (i = 0; i < AR0543_LSC_PX_SIZE/2; i++) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				AR0543_LSC_P2_ADDR+i*2, &data,
				MSM_CAMERA_I2C_WORD_DATA);
		pr_err("%s - p2[%d] = 0x%04x\n", __func__, i, data);
	}
	for (i = 0; i < AR0543_LSC_PX_SIZE/2; i++) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				AR0543_LSC_P3_ADDR+i*2, &data,
				MSM_CAMERA_I2C_WORD_DATA);
		pr_err("%s - p3[%d] = 0x%04x\n", __func__, i, data);
	}
	for (i = 0; i < AR0543_LSC_PX_SIZE/2; i++) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				AR0543_LSC_P4_ADDR+i*2, &data,
				MSM_CAMERA_I2C_WORD_DATA);
		pr_err("%s - p4[%d] = 0x%04x\n", __func__, i, data);
	}
	for (i = 0; i < AR0543_LSC_XY_SIZE/2; i++) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				AR0543_LSC_XY_ADDR+i*2, &data,
				MSM_CAMERA_I2C_WORD_DATA);
		pr_err("%s - xy[%d] = 0x%04x\n", __func__, i, data);
	}
	for (i = 0; i < AR0543_LSC_Q5_SIZE/2; i++) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				AR0543_LSC_Q5_ADDR+i*2, &data,
				MSM_CAMERA_I2C_WORD_DATA);
		pr_err("%s - q5[%d] = 0x%04x\n", __func__, i, data);
	}
}
#else
static inline void ar0543_otp_lsc_dump(struct msm_sensor_ctrl_t *s_ctrl) {; }
#endif

static int32_t ar0543_otp_validate_crc(uint8_t *otp_ptr, uint16_t size)
{
	int32_t rc = 0;
	u16 crc_otp = 0;
	u16 crc = 0;
	u16 crc_ref = 0;
	u8 *otp_ref;
	int i;

	/* Allocate memory for reflected OTP data */
	otp_ref = kmalloc(size, GFP_KERNEL);
	if (otp_ref == NULL)
		return -ENOMEM;

	/* Reflect OTP data */
	for (i = 0; i < size; i++)
		*(otp_ref + i) = bitrev8(*(otp_ptr + i));

	/* Calculate CRC */
	crc = crc16(crc, otp_ref, size);

	/* Reflect CRC */
	crc_ref = bitrev16(crc);

	/* Read OTP CRC */
	crc_otp = *(otp_ptr + size) | *(otp_ptr + size + 1) << 8;

	/* Compare CRC */
	if ((crc_ref == crc_otp) && (crc_otp != 0)) {
		pr_debug("%s: OTP CRC pass\n", __func__);
	} else {
		pr_warn("%s: OTP CRC fail(crc_ref = 0x%x, crc_otp = 0x%x)!\n",
				__func__, crc_ref, crc_otp);
		rc = -1;
	}

	/* Free reflected OTP data */
	kfree(otp_ref);

	return rc;
}

static int32_t ar0543_auto_otp_read(struct msm_sensor_ctrl_t *s_ctrl,
	uint8_t otp_rt)
{
	int32_t rc = 0;
	uint16_t retry = 0;
	uint16_t otp_control_reg = 0;

	/* OTP Record Type */
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x304C, otp_rt<<8,
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
		usleep_range(2500, 2510);
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

static uint16_t ar0543_read_otp_version(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint16_t cal_ver = 0x0000;

	/* Auto read OTP Module Data */
	if (ar0543_auto_otp_read(s_ctrl, 0x30) == 0) {
		/* Read Cal Version */
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x3800, &cal_ver,
			MSM_CAMERA_I2C_WORD_DATA);
	} else {
		pr_warn("%s: Auto read OTP Cal Version fail!\n", __func__);
	}

	/* Check Cal Version */
	if (cal_ver == 0x3030)
		cal_ver = AR0543_OTP_CAL_REV_1;
	else if (cal_ver == 0x3031)
		cal_ver = AR0543_OTP_CAL_REV_2;
	else if (cal_ver == 0x3032)
		cal_ver = AR0543_OTP_CAL_REV_3;
	else
		cal_ver = AR0543_OTP_CAL_REV_DEF;

	pr_warn("%s: cal_ver = 0x%x\n", __func__, cal_ver);

	return cal_ver;
}

static int32_t ar0543_read_otp(struct msm_sensor_ctrl_t *s_ctrl,
	uint16_t *otp_ptr, struct module_otp_t *otp_str)
{
	struct msm_camera_i2c_fn_t *i2c_func_tbl;
	int32_t rc = 0;
	uint8_t i = 0;
	uint8_t otp_rt = 0;
	uint16_t j = 0;
	uint16_t swap = 0;

	/* Initialize i2c function table ptr */
	i2c_func_tbl = s_ctrl->sensor_i2c_client->i2c_func_tbl;

	/* For the maximum number of OTP entries */
	for (i = 0; i < otp_str->max_entries; i++) {
		/* Auto read OTP Module Data */
		otp_rt = otp_str->mod_record_entry_base+i*2;
		rc = ar0543_auto_otp_read(s_ctrl, otp_rt);
		if (rc < 0) {
			pr_warn("%s: Auto read OTP Mod (rt = 0x%x) fail!\n",
					__func__, otp_rt);
			continue;
		}

		/* Read and store OTP Module Data */
		for (j = 0; j < otp_str->mod_size; j++)
			i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				(AR0543_OTP_ADDR+j*2),
				(otp_ptr+otp_str->mod_os+j),
				MSM_CAMERA_I2C_WORD_DATA);

		/* Auto read OTP Calibration Data */
		otp_rt = otp_str->cal_record_entry_base+i*2;
		rc = ar0543_auto_otp_read(s_ctrl, otp_rt);
		if (rc < 0) {
			pr_warn("%s: Auto read OTP Cal (rt = 0x%x) fail!\n",
					__func__, otp_rt);
			continue;
		}

		/* Read and store OTP Calibration Data */
		for (j = 0; j < otp_str->cal_size; j++)
			i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				(AR0543_OTP_ADDR+j*2),
				(otp_ptr+otp_str->cal_os+j),
				MSM_CAMERA_I2C_WORD_DATA);

		/* Dump OTP */
		ar0543_otp_raw_dump(otp_ptr, otp_str->size);

		/* Fix endianness of data, except CRC bytes */
		for (j = 0; j < (otp_str->size-1); j++) {
			swap = otp_ptr[j];
			otp_ptr[j] = (swap>>8) | (swap<<8);
		}

		/* Validate OTP CRC */
		rc = ar0543_otp_validate_crc((uint8_t *)otp_ptr,
				((otp_str->size-1)*2));
		if (rc < 0) {
			pr_warn("%s: OTP CRC (rg = %d) fail!\n",
					__func__, i);
		} else {
			pr_debug("%s: OTP CRC (rg = %d) pass\n",
					__func__, i);
			break;
		}
	}

	return rc;
}

static int32_t ar0543_read_otp_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	int i = 0;
	uint16_t cal_ver = 0x0000;
	uint16_t hw_rev = 0;
	uint16_t sn[4];
	static bool ar0543_otp_read_done;

	/* Check OTP has already been read */
	if (ar0543_otp_read_done == true)
		return rc;

	/* Set default OTP info */
	ar0543_otp_info.size = sizeof(ar0543_otp);
	ar0543_otp_info.otp_info = (uint8_t *)ar0543_otp;
	ar0543_otp_info.cal_ver = AR0543_OTP_CAL_REV_DEF;
	ar0543_otp_info.hw_rev = 0x0000;
	for (i = 0; i < AR0543_SN_SIZE; i++)
		ar0543_otp_info.sn[i] = 0x0000;

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

	/* Attempt to read OTP Cal Version */
	cal_ver = ar0543_read_otp_version(s_ctrl);

	/* For the number of valid OTP revisions */
	for (i = AR0543_OTP_CAL_REV_MAX; i >= 0; i--) {
		/* Recursive version read */
		rc = ar0543_read_otp(s_ctrl, ar0543_otp,
				&ar0543_otp_rev.n[i]);
		if (rc < 0) {
			pr_err("%s: Read OTP fail!\n", __func__);
		} else {
			pr_debug("%s: Read OTP pass\n", __func__);
			ar0543_otp_info.cal_ver = i;
			break;
		}
	}

	/* Read HW Revision */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client, 0x0002, &hw_rev,
			MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: Read hw_rev fail!\n", __func__);
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
	} else {
		pr_debug("%s: Read sn pass\n", __func__);
		for (i = 0; i < AR0543_SN_SIZE; i++) {
			pr_debug("%s: sn[%d]=0x%04x\n", __func__, i, sn[i]);
			ar0543_otp_info.sn[i] = sn[i];
		}
	}

	/* Indicate OTP has been read */
	ar0543_otp_read_done = true;

	return rc;
}

static int32_t ar0543_set_lsc(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t v;
	uint16_t *lsc_ptr;

	/* There are 3 cases to handle */
	/* 1. Module does not support LSC -> Use default LSC table */
	/* 2. Module does not support OTP -> Use default LSC table */
	/* 3. Module supports LSC -> Use module LSC table */
	v = ar0543_otp_info.cal_ver;
	if (ar0543_otp_rev.n[v].cal_lsc_en == 0) {
		/* Set Cal Version to the default version */
		v = AR0543_OTP_CAL_REV_DEF;
	}

	/* Check Cal Version */
	if (v == AR0543_OTP_CAL_REV_DEF) {
		pr_warn("%s: Setting default LSC!\n", __func__);
		lsc_ptr = ar0543_otp_lsc_def + ar0543_otp_rev.n[v].cal_lsc_os;
	} else {
		pr_warn("%s: Setting module LSC!\n", __func__);
		lsc_ptr = ar0543_otp + ar0543_otp_rev.n[v].cal_lsc_os;
	}

	/* Write LSC p0 */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq(
		s_ctrl->sensor_i2c_client,
		AR0543_LSC_P0_ADDR,
		(uint8_t *)(lsc_ptr + ar0543_otp_rev.n[v].cal_lsc_p0_os),
		AR0543_LSC_PX_SIZE);
	if (rc < 0) {
		pr_err("%s - Failed to write LSC p0!\n", __func__);
		return rc;
	}

	/* Write LSC p1 */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq(
		s_ctrl->sensor_i2c_client,
		AR0543_LSC_P1_ADDR,
		(uint8_t *)(lsc_ptr + ar0543_otp_rev.n[v].cal_lsc_p1_os),
		AR0543_LSC_PX_SIZE);
	if (rc < 0) {
		pr_err("%s - Failed to write LSC p1!\n", __func__);
		return rc;
	}

	/* Write LSC p2 */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq(
		s_ctrl->sensor_i2c_client,
		AR0543_LSC_P2_ADDR,
		(uint8_t *)(lsc_ptr + ar0543_otp_rev.n[v].cal_lsc_p2_os),
		AR0543_LSC_PX_SIZE);
	if (rc < 0) {
		pr_err("%s - Failed to write LSC p2!\n", __func__);
		return rc;
	}

	/* Write LSC p3 */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq(
		s_ctrl->sensor_i2c_client,
		AR0543_LSC_P3_ADDR,
		(uint8_t *)(lsc_ptr + ar0543_otp_rev.n[v].cal_lsc_p3_os),
		AR0543_LSC_PX_SIZE);
	if (rc < 0) {
		pr_err("%s - Failed to write LSC p3!\n", __func__);
		return rc;
	}

	/* Write LSC p4 */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq(
		s_ctrl->sensor_i2c_client,
		AR0543_LSC_P4_ADDR,
		(uint8_t *)(lsc_ptr + ar0543_otp_rev.n[v].cal_lsc_p4_os),
		AR0543_LSC_PX_SIZE);
	if (rc < 0) {
		pr_err("%s - Failed to write LSC p4!\n", __func__);
		return rc;
	}

	/* Write LSC xy */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq(
		s_ctrl->sensor_i2c_client,
		AR0543_LSC_XY_ADDR,
		(uint8_t *)(lsc_ptr + ar0543_otp_rev.n[v].cal_lsc_xy_os),
		AR0543_LSC_XY_SIZE);
	if (rc < 0) {
		pr_err("%s - Failed to write LSC xy!\n", __func__);
		return rc;
	}

	/* Write LSC q5 */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_seq(
		s_ctrl->sensor_i2c_client,
		AR0543_LSC_Q5_ADDR,
		(uint8_t *)(lsc_ptr + ar0543_otp_rev.n[v].cal_lsc_q5_os),
		AR0543_LSC_Q5_SIZE);
	if (rc < 0) {
		pr_err("%s - Failed to write LSC q5!\n", __func__);
		return rc;
	}

	return rc;
}

static int32_t ar0543_get_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	int i;

	s_ctrl->sensor_otp.otp_info = ar0543_otp_info.otp_info;
	s_ctrl->sensor_otp.size = ar0543_otp_info.size;
	s_ctrl->sensor_otp.hw_rev = ar0543_otp_info.hw_rev;
	s_ctrl->sensor_otp.asic_rev = ar0543_otp_info.asic_rev;
	s_ctrl->sensor_otp.cal_ver = ar0543_otp_info.cal_ver;
	for (i = 0; i < AR0543_SN_SIZE; i++)
		s_ctrl->sensor_otp.sn[i] = ar0543_otp_info.sn[i];

	return 0;
}

static int32_t ar0543_get_lsc(struct msm_sensor_ctrl_t *s_ctrl)
{
	static bool ar0543_lsc_read_done;

	/* Check whether LSC has been read */
	if (ar0543_lsc_read_done == false) {
		/* Dump LSC */
		ar0543_otp_lsc_dump(s_ctrl);

		/* Indicate the LSC has been read */
		ar0543_lsc_read_done = true;
	}

	return 0;
}

static struct msm_sensor_fn_t ar0543_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_get_module_info = ar0543_get_module_info,
	.sensor_read_otp_info = ar0543_read_otp_info,
	.sensor_set_lsc = ar0543_set_lsc,
	.sensor_get_lsc = ar0543_get_lsc,
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
