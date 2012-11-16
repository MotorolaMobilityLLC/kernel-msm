/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2012, Motorola Mobility LLC. All rights reserved.
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
#include <linux/regulator/consumer.h>

#define SENSOR_NAME "ov8820"
#define PLATFORM_DRIVER_NAME "msm_camera_ov8820"

#if 0
#define OV8820_OTP_DATA      0x3D00
#define OV8820_OTP_LOAD      0x3D81
#define OV8820_OTP_BANK      0x3D84
#define OV8820_OTP_BANK_SIZE 0x20

#define SWAP_2_BYTES(x) (((x << 8) & 0xff00) | \
		(x >> 8))
#define SWAP_4_BYTES(x) (((x) << 24) | \
		(((x) << 8) & 0x00ff0000) | \
		(((x) >> 8) & 0x0000ff00) | \
		((x) >> 24))
#endif

DEFINE_MUTEX(ov8820_mut);
static struct msm_sensor_ctrl_t ov8820_s_ctrl;
#if 0
static struct otp_info_t otp_info;
struct af_info_t af_info;
#endif

static struct msm_cam_clk_info cam_mot_8960_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};
static struct msm_camera_i2c_reg_conf ov8820_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf ov8820_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf ov8820_groupon_settings[] = {
	{0x3208, 0x00},
};

static struct msm_camera_i2c_reg_conf ov8820_groupoff_settings[] = {
	{0x3208, 0x10},
	{0x3208, 0xA0},
};

static struct msm_camera_i2c_reg_conf ov8820_1080_settings[] = {
	{0x3004, 0xce},
	{0x3005, 0x10},
	{0x3006, 0x00},
	/* {0x3501, 0x74}, */
	/* {0x3502, 0x60}, */
	{0x370e, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x01},
	{0x3803, 0x30},
	{0x3805, 0xdf},
	{0x3806, 0x08},
	{0x3807, 0x67},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380c, 0x0d},
	{0x380d, 0xf0},
	{0x380e, 0x07},
	{0x380f, 0x4c},
	{0x3811, 0x10},
	{0x3813, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x00},
	{0x3821, 0x16},
	{0x3f00, 0x02},
	{0x3f05, 0x10},
	{0x4005, 0x1a},
	{0x4600, 0x04},
	{0x4601, 0x01},
	{0x4602, 0x00},
	{0x4837, 0x27},
	{0x5068, 0x53},
	{0x506a, 0x53},
};

static struct msm_camera_i2c_reg_conf ov8820_prev_settings[] = {
	{0x3004, 0xd5},
	{0x3005, 0x11},
	{0x3006, 0x11},
	/* {0x3501, 0x4e}, */
	/* {0x3502, 0xa0}, */
	{0x370e, 0x08},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0x9b},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380c, 0x06},
	{0x380d, 0xde},
	{0x380e, 0x05},
	{0x380f, 0x06},
	{0x3811, 0x08},
	{0x3813, 0x04},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3820, 0x00},
	{0x3821, 0x17},
	{0x3f00, 0x00},
	{0x3f05, 0x10},
	{0x4005, 0x1a},
	{0x4600, 0x04},
	{0x4601, 0x00},
	{0x4602, 0x78},
	{0x4837, 0x5a},
	{0x5068, 0x00},
	{0x506a, 0x00},
};

static struct msm_camera_i2c_reg_conf ov8820_snap_settings[] = {
	{0x3004, 0xbf},
	{0x3005, 0x10},
	{0x3006, 0x00},
	/* {0x3501, 0x9a}, */
	/* {0x3502, 0xa0}, */
	{0x370e, 0x00},
	{0x3801, 0x00},
	{0x3712, 0xcc},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0x9b},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380c, 0x0d},
	{0x380d, 0x20},
	{0x380e, 0x09},
	{0x380f, 0xb0},
	{0x3811, 0x10},
	{0x3813, 0x06},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x00},
	{0x3821, 0x16},
	{0x3f00, 0x02},
	{0x3f05, 0x10},
	{0x4005, 0x1a},
	{0x4600, 0x04},
	{0x4601, 0x00},
	{0x4602, 0x20},
	{0x4837, 0x1e},
	{0x5068, 0x00},
	{0x506a, 0x00},
};

static struct msm_camera_i2c_reg_conf ov8820_60fps_settings[] = {
	{0x3004, 0xbf},
	{0x3005, 0x10},
	{0x3006, 0x00},
	{0x3501, 0x39},
	{0x3502, 0xa0},
	{0x370e, 0x08},
	{0x3801, 0x28},
	{0x3802, 0x01},
	{0x3803, 0x40},
	{0x3805, 0xb7},
	{0x3806, 0x08},
	{0x3807, 0x57},
	{0x3808, 0x05},
	{0x3809, 0x00},
	{0x380a, 0x02},
	{0x380b, 0xd0},
	{0x380c, 0x0d},
	{0x380d, 0xe0},
	{0x380e, 0x03},
	{0x380f, 0xa0},
	{0x3811, 0x04},
	{0x3813, 0x04},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3820, 0x80},
	{0x3821, 0x17},
	{0x3f00, 0x00},
	{0x3f05, 0x50},
	{0x4005, 0x18},
	{0x4600, 0x14},
	{0x4601, 0x14},
	{0x4602, 0x00},
	{0x4837, 0x1e},
	{0x5068, 0x5a},
	{0x506a, 0x5a},
	/*
	 * TBD if we need these registers *
	{0x5c00, 0x81},
	{0x5c01, 0x01},
	{0x5c02, 0x1f},
	{0x5c03, 0x01},
	{0x5c04, 0x1f},
	{0x5c08, 0x87},
	{0x6703, 0xd7},
	{0x6900, 0x63},
	*/
};

static struct msm_camera_i2c_reg_conf ov8820_reset_settings[] = {
	{0x0103, 0x01},
};

static struct msm_camera_i2c_reg_conf ov8820_recommend_settings[] = {
	{0x3000, 0x02},
	{0x3001, 0x00},
	{0x3002, 0x6c},
	{0x3003, 0xce},
	{0x3004, 0xd5},
	{0x3005, 0x11},
	{0x3006, 0x11},
	{0x3007, 0x3b},
	{0x300d, 0x00},
	{0x301f, 0x09},
	{0x3010, 0x00},
	{0x3011, 0x02},
	{0x3012, 0x80},
	{0x3013, 0x39},
	{0x3018, 0x00},
	{0x3104, 0x20},
	{0x3300, 0x00},
	{0x3500, 0x00},
	{0x3501, 0x4e},
	{0x3502, 0xa0},
	{0x3503, 0x07},
	{0x3509, 0x00},
	{0x350b, 0x1f},
	{0x3600, 0x05},
	{0x3601, 0x32},
	{0x3602, 0x44},
	{0x3603, 0x5c},
	{0x3604, 0x98},
	{0x3605, 0xe9},
	{0x3609, 0xb8},
	{0x360a, 0xbc},
	{0x360b, 0xb4},
	{0x360c, 0x0d},
	{0x3613, 0x02},
	{0x3614, 0x0f},
	{0x3615, 0x00},
	{0x3616, 0x03},
	{0x3617, 0x01},
	{0x3618, 0x00},
	{0x3619, 0x00},
	{0x361a, 0x00},
	{0x361b, 0x00},
	{0x3700, 0x20},
	{0x3701, 0x44},
	{0x3702, 0x70},
	{0x3703, 0x4f},
	{0x3704, 0x69},
	{0x3706, 0x7b},
	{0x3707, 0x63},
	{0x3708, 0x85},
	{0x3709, 0x40},
	{0x370a, 0x12},
	{0x370b, 0x01},
	{0x370c, 0x50},
	{0x370d, 0x0c},
	{0x370e, 0x08},
	{0x3711, 0x01},
	{0x3712, 0xcc},
	{0x3800, 0x00},
	{0x3801, 0x00},
	{0x3802, 0x00},
	{0x3803, 0x00},
	{0x3804, 0x0c},
	{0x3805, 0xdf},
	{0x3806, 0x09},
	{0x3807, 0x9b},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380c, 0x06},
	{0x380d, 0xde},
	{0x380e, 0x05},
	{0x380f, 0x06},
	{0x3810, 0x00},
	{0x3811, 0x08},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3816, 0x02},
	{0x3817, 0x40},
	{0x3818, 0x00},
	{0x3819, 0x40},
	{0x3820, 0x00},
	{0x3821, 0x17},
	{0x3d00, 0x00},
	{0x3d01, 0x00},
	{0x3d02, 0x00},
	{0x3d03, 0x00},
	{0x3d04, 0x00},
	{0x3d05, 0x00},
	{0x3d06, 0x00},
	{0x3d07, 0x00},
	{0x3d08, 0x00},
	{0x3d09, 0x00},
	{0x3d0a, 0x00},
	{0x3d0b, 0x00},
	{0x3d0c, 0x00},
	{0x3d0d, 0x00},
	{0x3d0e, 0x00},
	{0x3d0f, 0x00},
	{0x3d10, 0x00},
	{0x3d11, 0x00},
	{0x3d12, 0x00},
	{0x3d13, 0x00},
	{0x3d14, 0x00},
	{0x3d15, 0x00},
	{0x3d16, 0x00},
	{0x3d17, 0x00},
	{0x3d18, 0x00},
	{0x3d19, 0x00},
	{0x3d1a, 0x00},
	{0x3d1b, 0x00},
	{0x3d1c, 0x00},
	{0x3d1d, 0x00},
	{0x3d1e, 0x00},
	{0x3d1f, 0x00},
	{0x3d80, 0x00},
	{0x3d81, 0x00},
	{0x3d84, 0x00},
	{0x3f00, 0x00},
	{0x3f01, 0xfc},
	{0x3f05, 0x10},
	{0x3f06, 0x00},
	{0x3f07, 0x00},
	{0x4000, 0x29},
	{0x4001, 0x02},
	{0x4002, 0x45},
	{0x4003, 0x08},
	{0x4004, 0x04},
	{0x4005, 0x1a},
	{0x4300, 0xff},
	{0x4303, 0x00},
	{0x4304, 0x08},
	{0x4307, 0x00},
	{0x4600, 0x04},
	{0x4601, 0x00},
	{0x4602, 0x78},
	{0x4800, 0x04},
	{0x4801, 0x0f},
	{0x4837, 0x5a},
	{0x4843, 0x02},
	{0x5000, 0x06},
	{0x5001, 0x00},
	{0x5002, 0x00},
	{0x5068, 0x00},
	{0x506a, 0x00},
	{0x501f, 0x00},
	{0x5780, 0xfc},
	{0x5c00, 0x80},
	{0x5c01, 0x00},
	{0x5c02, 0x00},
	{0x5c03, 0x00},
	{0x5c04, 0x00},
	{0x5c05, 0x00},
	{0x5c06, 0x00},
	{0x5c07, 0x80},
	{0x5c08, 0x10},
	{0x6700, 0x05},
	{0x6701, 0x19},
	{0x6702, 0xfd},
	{0x6703, 0xd1},
	{0x6704, 0xff},
	{0x6705, 0xff},
	{0x6800, 0x10},
	{0x6801, 0x02},
	{0x6802, 0x90},
	{0x6803, 0x10},
	{0x6804, 0x59},
	{0x6900, 0x61},
	{0x6901, 0x04},
	{0x3612, 0x00},
	{0x3617, 0xa1},
	{0x3b1f, 0x00},
	{0x3000, 0x12},
	{0x3000, 0x16},
	{0x3b1f, 0x00},
	{0x0100, 0x01},
	{0x5800, 0x16},
	{0x5801, 0x0b},
	{0x5802, 0x09},
	{0x5803, 0x09},
	{0x5804, 0x0b},
	{0x5805, 0x15},
	{0x5806, 0x07},
	{0x5807, 0x05},
	{0x5808, 0x03},
	{0x5809, 0x03},
	{0x580a, 0x05},
	{0x580b, 0x06},
	{0x580c, 0x05},
	{0x580d, 0x02},
	{0x580e, 0x00},
	{0x580f, 0x00},
	{0x5810, 0x02},
	{0x5811, 0x05},
	{0x5812, 0x06},
	{0x5813, 0x02},
	{0x5814, 0x00},
	{0x5815, 0x00},
	{0x5816, 0x02},
	{0x5817, 0x05},
	{0x5818, 0x07},
	{0x5819, 0x05},
	{0x581a, 0x04},
	{0x581b, 0x03},
	{0x581c, 0x05},
	{0x581d, 0x06},
	{0x581e, 0x13},
	{0x581f, 0x0b},
	{0x5820, 0x09},
	{0x5821, 0x09},
	{0x5822, 0x0b},
	{0x5823, 0x16},
	{0x5824, 0x63},
	{0x5825, 0x23},
	{0x5826, 0x25},
	{0x5827, 0x23},
	{0x5828, 0x45},
	{0x5829, 0x23},
	{0x582a, 0x21},
	{0x582b, 0x41},
	{0x582c, 0x41},
	{0x582d, 0x05},
	{0x582e, 0x23},
	{0x582f, 0x41},
	{0x5830, 0x41},
	{0x5831, 0x41},
	{0x5832, 0x03},
	{0x5833, 0x25},
	{0x5834, 0x23},
	{0x5835, 0x21},
	{0x5836, 0x23},
	{0x5837, 0x05},
	{0x5838, 0x25},
	{0x5839, 0x43},
	{0x583a, 0x25},
	{0x583b, 0x23},
	{0x583c, 0x65},
	{0x583d, 0xcf},
	{0x5842, 0x00},
	{0x5843, 0xef},
	{0x5844, 0x01},
	{0x5845, 0x3f},
	{0x5846, 0x01},
	{0x5847, 0x3f},
	{0x5848, 0x00},
	{0x5849, 0xd5},
};

static struct v4l2_subdev_info ov8820_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array ov8820_init_conf[] = {
	{&ov8820_reset_settings[0],
	ARRAY_SIZE(ov8820_reset_settings), 50, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov8820_recommend_settings[0],
	ARRAY_SIZE(ov8820_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array ov8820_confs[] = {
	{&ov8820_snap_settings[0],
	ARRAY_SIZE(ov8820_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov8820_prev_settings[0],
	ARRAY_SIZE(ov8820_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov8820_1080_settings[0],
	ARRAY_SIZE(ov8820_1080_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov8820_60fps_settings[0],
	ARRAY_SIZE(ov8820_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

/* vt_pixel_clk==pll sys clk, op_pixel_clk==mipi clk */
static struct msm_sensor_output_info_t ov8820_dimensions[] = {
	{
		.x_output = 0xCC0,
		.y_output = 0x990,
		.line_length_pclk = 0xD68,
		.frame_length_lines = 0x9B0,
		.vt_pixel_clk = 200000000,
		.op_pixel_clk = 264000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x660,
		.y_output = 0x4C8,
		.line_length_pclk = 0x6DE,
		.frame_length_lines = 0x506,
		.vt_pixel_clk = 66700000,
		.op_pixel_clk = 88000000,
		.binning_factor = 2,
	},
	{
		.x_output = 0x780, /*1920*/
		.y_output = 0x438, /*1080*/
		.line_length_pclk = 0xDF0, /*3568*/
		.frame_length_lines = 0x74C, /*1868*/
		.vt_pixel_clk = 200000000,
		.op_pixel_clk = 204000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x500, /*1280*/
		.y_output = 0x2D0, /*720*/
		.line_length_pclk = 0xDE0, /*3552*/
		.frame_length_lines = 0x3A0, /*928*/
		.vt_pixel_clk = 200000000,
		.op_pixel_clk = 264000000,
		.binning_factor = 1,
	},
};

static struct msm_sensor_output_reg_addr_t ov8820_reg_addr = {
	.x_output = 0x3808,
	.y_output = 0x380a,
	.line_length_pclk = 0x380c,
	.frame_length_lines = 0x380e,
};

static struct msm_sensor_id_info_t ov8820_id_info = {
	.sensor_id_reg_addr = 0x300A,
	.sensor_id = 0x8820,
};

static struct msm_sensor_exp_gain_info_t ov8820_exp_gain_info = {
	.coarse_int_time_addr = 0x3501,
	.global_gain_addr = 0x350A,
	.vert_offset = 6,
};

#if 0
static int32_t ov8820_read_otp(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	int16_t i, j;
	uint8_t otp[256];
	uint16_t readData;

	/* Start Stream to read OTP Data */
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0100, 0x01, MSM_CAMERA_I2C_BYTE_DATA);

	if (rc < 0) {
		pr_err("%s: Unable to read otp\n", __func__);
		return rc;
	}

	/* Read all 8 banks */
	for (i = 0; i < 8; i++) {
		/* Reset OTP Buffer Registers */
		for (j = 0; j < 32; j++) {
			rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
					(uint16_t)(OV8820_OTP_DATA+j), 0xFF,
					MSM_CAMERA_I2C_BYTE_DATA);
			if (rc < 0)
				return rc;
		}

		/* Set OTP Bank & Enable */
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				OV8820_OTP_BANK, 0x08|i,
				MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
			return rc;

		/* Set Read OTP Bank */
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				OV8820_OTP_LOAD, 0x01,
				MSM_CAMERA_I2C_BYTE_DATA);

		if (rc < 0)
			return rc;

		/* Delay */
		msleep(25);

		/* Read OTP Buffer Registers */
		for (j = 0; j < 32; j++) {
			rc = msm_camera_i2c_read(s_ctrl->sensor_i2c_client,
					OV8820_OTP_DATA+j,
					&readData,
					MSM_CAMERA_I2C_BYTE_DATA);

			otp[(i*32)+j] = (uint8_t)readData;

			if (rc < 0)
				return rc;
		}

		/* Reset Read OTP Bank */
		rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
				OV8820_OTP_LOAD, 0x00,
				MSM_CAMERA_I2C_BYTE_DATA);

		if (rc < 0)
			return rc;
	}

	/* Stop Streaming */
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x0100, 0x00, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: Unable to stop streaming of imager\n", __func__);
		return rc;
	}

	memcpy((void *)&otp_info, otp, sizeof(struct otp_info_t));
	memcpy(&af_info.af_man_type1, &(otp_info.otp_info[16]),
			sizeof(uint32_t));
	memcpy(&af_info.af_man_type2, &(otp_info.otp_info[20]),
			sizeof(uint32_t));
	memcpy(&af_info.af_act_type, &(otp_info.otp_info[25]),
			sizeof(uint16_t));

	af_info.af_act_type = SWAP_2_BYTES(af_info.af_act_type);
	af_info.af_man_type1 = SWAP_4_BYTES(af_info.af_man_type1);
	af_info.af_man_type2 = SWAP_4_BYTES(af_info.af_man_type2);

	return rc;
}
#endif

static int32_t ov8820_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines, offset;
	uint8_t int_time[3];

	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	fl_lines += (fl_lines & 0x1);
	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	int_time[0] = line >> 12;
	int_time[1] = line >> 4;
	int_time[2] = line << 4;
	msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr-1,
		&int_time[0], 3);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}

static int32_t ov8820_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device *dev = NULL;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	pr_info("ov8820_power_up\n");
	dev = &s_ctrl->sensor_i2c_client->client->dev;

	if (!info->oem_data) {
		pr_err("%s: oem data NULL in sensor info, aborting",
				__func__);
		rc = -EINVAL;
		goto power_up_done;
	}

	/*obtain gpios*/
	/* AVDD */
	rc = gpio_request(info->oem_data->sensor_avdd_en, "ov8820");
	if (rc < 0) {
		pr_err("ov8820: gpio request ANALOG_EN failed (%d)\n",
				rc);
		goto power_up_done;
	}
	/* PWRDWN */
	rc = gpio_request(info->sensor_pwd, "ov8820");
	if (rc < 0) {
		pr_err("ov8820: gpio request PWRDWN failed (%d)\n", rc);
		goto power_up_done;
	}
	/* RESET */
	rc = gpio_request(info->sensor_reset, "ov8820");
	if (rc < 0) {
		pr_err("ov8820: gpio request RESET failed (%d)\n", rc);
		goto power_up_done;
	}
	/* VDD */
	rc = gpio_request(info->oem_data->sensor_dig_en, "ov8820");
	if (rc < 0) {
		pr_err("ov8820: gpio request DIG_EN failed (%d)\n", rc);
		goto power_up_done;
	}

	/*Turn on VDDIO*/
	gpio_direction_output(info->oem_data->sensor_dig_en, 1);

	/* Wait for core supplies to power up */
	usleep(10000);

	/* Enable AVDD and AF supplies*/
	gpio_direction_output(info->oem_data->sensor_avdd_en, 1);
	usleep(200);

	/*Enable MCLK*/
	cam_mot_8960_clk_info->clk_rate = s_ctrl->clk_rate;
	rc = msm_cam_clk_enable(dev, cam_mot_8960_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_mot_8960_clk_info), 1);
	if (rc < 0) {
		pr_err("ov8820: msm_cam_clk_enable failed (%d)\n", rc);
		goto power_up_done;
	}
	usleep(20000);

	/*set PWRDWN high*/
	gpio_direction_output(info->sensor_pwd, 1);
	usleep(26000);

	/*Set Reset high*/
	gpio_direction_output(info->sensor_reset, 1);
	usleep(35000);

power_up_done:
	return rc;
}

static int32_t ov8820_power_down(
		struct msm_sensor_ctrl_t *s_ctrl)
{
	struct device *dev = NULL;
	int32_t rc = 0;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;
	dev = &s_ctrl->sensor_i2c_client->client->dev;

	pr_info("ov8820_power_down\n");

	/*Set Reset Low*/
	gpio_direction_output(info->sensor_reset, 0);
	usleep(10000);

	/*Disable MCLK*/
	rc = msm_cam_clk_enable(dev, cam_mot_8960_clk_info, s_ctrl->cam_clk,
			ARRAY_SIZE(cam_mot_8960_clk_info), 0);
	if (rc < 0)
		pr_err("ov8820: msm_cam_clk_enable failed (%d)\n", rc);
	usleep(10000);

	/*Disable AVDD and AF supplies*/
	gpio_direction_output(info->oem_data->sensor_avdd_en, 0);
	usleep(15000);

	/*Disable VDDIO*/
	gpio_direction_output(info->oem_data->sensor_dig_en, 0);

	/* Wait for core to shut off */
	usleep(10000);

	/*Set PWRDWN Low*/
	gpio_direction_output(info->sensor_pwd, 0);

	/*Clean up*/
	gpio_free(info->oem_data->sensor_avdd_en);
	gpio_free(info->sensor_pwd);
	gpio_free(info->sensor_reset);
	gpio_free(info->oem_data->sensor_dig_en);
	return 0;
}

static const struct i2c_device_id ov8820_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov8820_s_ctrl},
	{ }
};

static struct i2c_driver ov8820_i2c_driver = {
	.id_table = ov8820_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov8820_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

#if 0
static int32_t ov8820_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: read id failed\n", __func__);
		return rc;
	}

	pr_info("ov8820 chipid: %04x\n", chipid);
	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("%s: chip id does not match\n", __func__);
		return -ENODEV;
	}

	rc = ov8820_read_otp(s_ctrl);
	if (rc < 0) {
		pr_err("%s: unable to read otp data\n", __func__);
		return -ENODEV;
	}

	pr_info("ov8820: match_id success\n");
	return 0;
}
#endif
#if 0
static int32_t ov8820_get_module_info(struct msm_sensor_ctrl_t *s_ctrl,
		struct otp_info_t *module_info)
{
	*(module_info) = otp_info;
	return 0;
}
#endif

#if 0
int32_t ov8820_adjust_frame_lines(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t res)
{
	uint16_t cur_line = 0;
	uint16_t exp_fl_lines = 0;
	uint8_t int_time[3];
	if (s_ctrl->sensor_exp_gain_info) {
		msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client,
				s_ctrl->sensor_exp_gain_info->
				coarse_int_time_addr-1,
				&int_time[0], 3);
		cur_line |= int_time[0] << 12;
		cur_line |= int_time[1] << 4;
		cur_line |= int_time[2] >> 4;
		exp_fl_lines = cur_line +
				s_ctrl->sensor_exp_gain_info->vert_offset;
		if (exp_fl_lines > s_ctrl->msm_sensor_reg->
				output_settings[res].frame_length_lines) {
			exp_fl_lines += (exp_fl_lines & 0x1);
			msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
					s_ctrl->sensor_output_reg_addr->
					frame_length_lines,
					exp_fl_lines,
					MSM_CAMERA_I2C_WORD_DATA);
		}
		CDBG("%s cur_line %x cur_fl_lines %x, exp_fl_lines %x\n",
				__func__,
				cur_line,
				s_ctrl->msm_sensor_reg->
				output_settings[res].frame_length_lines,
				exp_fl_lines);
	}
	return 0;
}
#endif

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&ov8820_i2c_driver);
}

static struct v4l2_subdev_core_ops ov8820_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov8820_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov8820_subdev_ops = {
	.core = &ov8820_subdev_core_ops,
	.video = &ov8820_subdev_video_ops,
};

static struct msm_sensor_fn_t ov8820_func_tbl = {
	.sensor_start_stream            = msm_sensor_start_stream,
	.sensor_stop_stream             = msm_sensor_stop_stream,
	.sensor_group_hold_on           = msm_sensor_group_hold_on,
	.sensor_group_hold_off          = msm_sensor_group_hold_off,
	.sensor_set_fps                 = msm_sensor_set_fps,
	.sensor_write_exp_gain          = ov8820_write_exp_gain,
	.sensor_write_snapshot_exp_gain = ov8820_write_exp_gain,
	.sensor_setting                 = msm_sensor_setting,
	.sensor_csi_setting             = msm_sensor_setting1,
	.sensor_set_sensor_mode         = msm_sensor_set_sensor_mode,
	.sensor_mode_init               = msm_sensor_mode_init,
	.sensor_get_output_info         = msm_sensor_get_output_info,
	.sensor_config                  = msm_sensor_config,
	.sensor_power_up                = ov8820_power_up,
	.sensor_power_down              = ov8820_power_down,
	.sensor_get_csi_params          = msm_sensor_get_csi_params,
	/* Added by Motorola TODO need to clean up*/
	/*.sensor_get_module_info         = ov8820_get_module_info,*/
	/*.sensor_match_id                = ov8820_match_id, */
	/*.sensor_adjust_frame_lines      = ov8820_adjust_frame_lines,*/
};

static struct msm_sensor_reg_t ov8820_regs = {
	.default_data_type        = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf        = ov8820_start_settings,
	.start_stream_conf_size   = ARRAY_SIZE(ov8820_start_settings),
	.stop_stream_conf         = ov8820_stop_settings,
	.stop_stream_conf_size    = ARRAY_SIZE(ov8820_stop_settings),
	.group_hold_on_conf       = ov8820_groupon_settings,
	.group_hold_on_conf_size  = ARRAY_SIZE(ov8820_groupon_settings),
	.group_hold_off_conf      = ov8820_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(ov8820_groupoff_settings),
	.init_settings            = &ov8820_init_conf[0],
	.init_size                = ARRAY_SIZE(ov8820_init_conf),
	.mode_settings            = &ov8820_confs[0],
	.output_settings          = &ov8820_dimensions[0],
	.num_conf                 = ARRAY_SIZE(ov8820_confs),
};

static struct msm_sensor_ctrl_t ov8820_s_ctrl = {
	.msm_sensor_reg               = &ov8820_regs,
	.sensor_i2c_client            = &ov8820_sensor_i2c_client,
	.sensor_i2c_addr              = 0x6C,
	.sensor_output_reg_addr       = &ov8820_reg_addr,
	.sensor_id_info               = &ov8820_id_info,
	.sensor_exp_gain_info         = &ov8820_exp_gain_info,
	.cam_mode                     = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex             = &ov8820_mut,
	.sensor_i2c_driver            = &ov8820_i2c_driver,
	.sensor_v4l2_subdev_info      = ov8820_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov8820_subdev_info),
	.sensor_v4l2_subdev_ops       = &ov8820_subdev_ops,
	.func_tbl                     = &ov8820_func_tbl,
	.clk_rate                     = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision 8820 Bayer sensor driver");
MODULE_LICENSE("GPL v2");
