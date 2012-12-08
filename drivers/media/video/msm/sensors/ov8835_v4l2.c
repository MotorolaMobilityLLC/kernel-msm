/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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

#define SENSOR_NAME "ov8835"
#define PLATFORM_DRIVER_NAME "msm_camera_ov8835"

#define OV8835_DEFAULT_MCLK_RATE 24000000

DEFINE_MUTEX(ov8835_mut);
static struct msm_sensor_ctrl_t ov8835_s_ctrl;

static struct regulator *cam_vdig;
static struct regulator *cam_vio;
static struct regulator *cam_mipi_mux;

static struct msm_cam_clk_info cam_mot_8960_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static struct msm_camera_i2c_reg_conf ov8835_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf ov8835_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf ov8835_groupon_settings[] = {
	{0x3208, 0x00},
};

static struct msm_camera_i2c_reg_conf ov8835_groupoff_settings[] = {
	{0x3208, 0x10},
	{0x3208, 0xA0},
};

/*RES_1920x1080_15fps_4lane*/
static struct msm_camera_i2c_reg_conf ov8835_1080_settings[] = {
	{0x3090, 0x03},
	{0x3091, 0x23},
	{0x3092, 0x01},
	{0x309c, 0x00},
	{0x3501, 0xa0},
	{0x3502, 0xc0},
	{0x3507, 0x10},
	{0x3708, 0xe3},
	{0x3801, 0x0c},
	{0x3802, 0x01},
	{0x3803, 0x40},
	{0x3805, 0xd3},
	{0x3806, 0x08},
	{0x3807, 0x73},
	{0x3808, 0x07},
	{0x3809, 0x80},
	{0x380a, 0x04},
	{0x380b, 0x38},
	{0x380e, 0x0a},
	{0x380f, 0x1a},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x10},
	{0x3821, 0x0e},
	{0x3a04, 0x07},
	{0x3a05, 0x49},
	{0x4004, 0x08},
	{0x4512, 0x01},
};

/*RES_1632x1224_BIN_30fps_4lane */
static struct msm_camera_i2c_reg_conf ov8835_prev_settings[] = {
	{0x3090, 0x03},
	{0x3091, 0x23},
	{0x3092, 0x01},
	{0x309c, 0x00},
	{0x3501, 0x50},
	{0x3502, 0x00},
	{0x3507, 0x08},
	{0x3708, 0xe6},
	{0x3801, 0x08},
	{0x3802, 0x00},
	{0x3803, 0x08},
	{0x3805, 0xd7},
	{0x3806, 0x09},
	{0x3807, 0xa7},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380e, 0x05},
	{0x380f, 0x0c},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3820, 0x11},
	{0x3821, 0x0f},
	{0x3a04, 0x04},
	{0x3a05, 0xc9},
	{0x4004, 0x02},
	{0x4512, 0x00},
};

/*RES_3264x2448_24fps_4lane*/
/*TBD use 30fps once we get the working set from OV*/
static struct msm_camera_i2c_reg_conf ov8835_snap_settings[] = {
	{0x3090, 0x02},
	{0x3091, 0x12},
	{0x3092, 0x00},
	{0x309c, 0x00},
	{0x3501, 0xa0},
	{0x3502, 0xc0},
	{0x3507, 0x10},
	{0x3708, 0xe3},
	{0x3801, 0x0c},
	{0x3802, 0x00},
	{0x3803, 0x0c},
	{0x3805, 0xd3},
	{0x3806, 0x09},
	{0x3807, 0xa3},
	{0x3808, 0x0c},
	{0x3809, 0xc0},
	{0x380a, 0x09},
	{0x380b, 0x90},
	{0x380e, 0x09},
	{0x380f, 0xbe},
	{0x3814, 0x11},
	{0x3815, 0x11},
	{0x3820, 0x10},
	{0x3821, 0x0e},
	{0x3a04, 0x09},
	{0x3a05, 0xa9},
	{0x4004, 0x08},
	{0x4512, 0x01},
};
static struct msm_camera_i2c_reg_conf ov8835_reset_settings[] = {
	{0x0103, 0x01},
};

/*From OV8835_AM03_redFPGA_0x6c.ovd - RES_1632x1224_BIN_30fps_4lane */
static struct msm_camera_i2c_reg_conf ov8835_recommend_settings[] = {
	{0x3001, 0x2a},
	{0x3002, 0x88},
	{0x3005, 0x00},
	{0x3011, 0x41},
	{0x3015, 0x08},
	{0x301b, 0xb4},
	{0x301d, 0x02},
	{0x3021, 0x00},
	{0x3022, 0x02},
	{0x3081, 0x02},
	{0x3083, 0x01},
	{0x3090, 0x03},
	{0x3091, 0x23},
	{0x3094, 0x00},
	{0x3092, 0x00},
	{0x3093, 0x00},
	{0x3098, 0x04},
	{0x3099, 0x10},
	{0x309a, 0x00},
	{0x309b, 0x00},
	{0x30a2, 0x01},
	{0x30b0, 0x05},
	{0x30b2, 0x00},
	{0x30b3, 0x42},
	{0x30b4, 0x03},
	{0x30b5, 0x04},
	{0x30b6, 0x01},
	{0x3104, 0xa1},
	{0x3106, 0x01},
	{0x3400, 0x04},
	{0x3401, 0x00},
	{0x3402, 0x04},
	{0x3403, 0x00},
	{0x3404, 0x04},
	{0x3405, 0x00},
	{0x3406, 0x01},
	{0x3500, 0x00},
	{0x3501, 0x50},
	{0x3502, 0x00},
	{0x3503, 0x07},
	{0x3504, 0x00},
	{0x3505, 0x30},
	{0x3506, 0x00},
	{0x3507, 0x08},
	{0x3508, 0x80},
	{0x3509, 0x00},
	{0x350a, 0x00},
	{0x350b, 0x38},
	{0x3600, 0xb8},
	{0x3601, 0x02},
	{0x3602, 0x7c},
	{0x3604, 0x38},
	{0x3612, 0x80},
	{0x3620, 0x41},
	{0x3621, 0xa4},
	{0x3622, 0x0b},
	{0x3625, 0x44},
	{0x3630, 0x55},
	{0x3631, 0xf2},
	{0x3632, 0x00},
	{0x3633, 0x34},
	{0x3634, 0x03},
	{0x364d, 0x0d},
	{0x364f, 0x60},
	{0x3660, 0x80},
	{0x3662, 0x10},
	{0x3665, 0x00},
	{0x3666, 0x00},
	{0x3667, 0x00},
	{0x366a, 0x80},
	{0x366c, 0x00},
	{0x366d, 0x00},
	{0x366e, 0x00},
	{0x366f, 0x20},
	{0x3680, 0xb5},
	{0x3681, 0x00},
	{0x3701, 0x14},
	{0x3702, 0x50},
	{0x3703, 0x8c},
	{0x3704, 0x68},
	{0x3705, 0x02},
	{0x3708, 0xe6},
	{0x3709, 0x03},
	{0x370a, 0x00},
	{0x370b, 0x20},
	{0x370c, 0x0c},
	{0x370d, 0x11},
	{0x370e, 0x00},
	{0x370f, 0x00},
	{0x3710, 0x00},
	{0x371c, 0x01},
	{0x371f, 0x0c},
	{0x3721, 0x00},
	{0x3724, 0x10},
	{0x3726, 0x00},
	{0x372a, 0x01},
	{0x3730, 0x18},
	{0x3738, 0x22},
	{0x3739, 0xd0},
	{0x373a, 0x50},
	{0x373b, 0x02},
	{0x373c, 0x20},
	{0x373f, 0x02},
	{0x3740, 0x42},
	{0x3741, 0x02},
	{0x3742, 0x18},
	{0x3743, 0x01},
	{0x3744, 0x02},
	{0x3747, 0x10},
	{0x374c, 0x04},
	{0x3751, 0xf0},
	{0x3752, 0x00},
	{0x3753, 0x00},
	{0x3754, 0xc0},
	{0x3755, 0x00},
	{0x3756, 0x1a},
	{0x3758, 0x00},
	{0x3759, 0x0f},
	{0x375c, 0x04},
	{0x3767, 0x01},
	{0x376b, 0x44},
	{0x3774, 0x10},
	{0x3776, 0x00},
	{0x377f, 0x08},
	{0x3780, 0x22},
	{0x3781, 0xcc},
	{0x3784, 0x2c},
	{0x3785, 0x08},
	{0x3786, 0x16},
	{0x378f, 0xf5},
	{0x3791, 0xb0},
	{0x3795, 0x00},
	{0x3796, 0x94},
	{0x3797, 0x11},
	{0x3798, 0x30},
	{0x3799, 0x41},
	{0x379a, 0x07},
	{0x379b, 0xb0},
	{0x379c, 0x0c},
	{0x37c5, 0x00},
	{0x37c6, 0xa0},
	{0x37c7, 0x00},
	{0x37c9, 0x00},
	{0x37ca, 0x00},
	{0x37cb, 0x00},
	{0x37cc, 0x00},
	{0x37cd, 0x00},
	{0x37ce, 0x01},
	{0x37cf, 0x00},
	{0x37d1, 0x01},
	{0x37de, 0x00},
	{0x37df, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x08},
	{0x3802, 0x00},
	{0x3803, 0x08},
	{0x3804, 0x0c},
	{0x3805, 0xd7},
	{0x3806, 0x09},
	{0x3807, 0xa7},
	{0x3808, 0x06},
	{0x3809, 0x60},
	{0x380a, 0x04},
	{0x380b, 0xc8},
	{0x380c, 0x0e},
	{0x380d, 0x18},
	{0x380e, 0x05},
	{0x380f, 0x0c},
	{0x3810, 0x00},
	{0x3811, 0x04},
	{0x3812, 0x00},
	{0x3813, 0x04},
	{0x3814, 0x31},
	{0x3815, 0x31},
	{0x3820, 0x11},
	{0x3821, 0x0f},
	{0x3823, 0x00},
	{0x3824, 0x00},
	{0x3825, 0x00},
	{0x3826, 0x00},
	{0x3827, 0x00},
	{0x382a, 0x04},
	{0x3a04, 0x04},
	{0x3a05, 0xc9},
	{0x3a06, 0x00},
	{0x3a07, 0xf8},
	{0x3b00, 0x00},
	{0x3b02, 0x00},
	{0x3b03, 0x00},
	{0x3b04, 0x00},
	{0x3b05, 0x00},
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
	{0x3d80, 0x00},
	{0x3d81, 0x00},
	{0x3d84, 0x00},
	{0x4000, 0x18},
	{0x4001, 0x04},
	{0x4002, 0x45},
	{0x4004, 0x02},
	{0x4005, 0x18},
	{0x4006, 0x20},
	{0x4008, 0x24},
	{0x4009, 0x10},
	{0x404f, 0x90},
	{0x4100, 0x17},
	{0x4101, 0x03},
	{0x4102, 0x04},
	{0x4103, 0x03},
	{0x4104, 0x5a},
	{0x4307, 0x30},
	{0x4315, 0x00},
	{0x4511, 0x05},
	{0x4512, 0x00},
	{0x4805, 0x21},
	{0x4806, 0x00},
	{0x481f, 0x36},
	{0x4831, 0x6c},
	{0x4837, 0x0f},
	{0x4a00, 0xaa},
	{0x4a03, 0x01},
	{0x4a05, 0x08},
	{0x4a0a, 0x88},
	{0x4d03, 0xbb},
	{0x5000, 0x06},
	{0x5001, 0x01},
	{0x5002, 0x80},
	{0x5003, 0x20},
	{0x5013, 0x00},
	{0x5046, 0x4a},
	{0x5780, 0x1c},
	{0x5786, 0x20},
	{0x5787, 0x10},
	{0x5788, 0x18},
	{0x578a, 0x04},
	{0x578b, 0x02},
	{0x578c, 0x02},
	{0x578e, 0x06},
	{0x578f, 0x02},
	{0x5790, 0x02},
	{0x5791, 0xff},
	{0x5a08, 0x02},
	{0x5e00, 0x00},
	{0x5e10, 0x0c},
};

static struct v4l2_subdev_info ov8835_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array ov8835_init_conf[] = {
	{&ov8835_reset_settings[0],
	ARRAY_SIZE(ov8835_reset_settings), 50, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov8835_recommend_settings[0],
	ARRAY_SIZE(ov8835_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array ov8835_confs[] = {
	{&ov8835_snap_settings[0],
	ARRAY_SIZE(ov8835_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov8835_prev_settings[0],
	ARRAY_SIZE(ov8835_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov8835_1080_settings[0],
	ARRAY_SIZE(ov8835_1080_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

/* vt_pixel_clk==pll sys clk, op_pixel_clk==mipi clk */
static struct msm_sensor_output_info_t ov8835_dimensions[] = {
	{
		.x_output = 0xCC0,
		.y_output = 0x990,
		.line_length_pclk = 0xe18,
		.frame_length_lines = 0x9be,
		.vt_pixel_clk = 216000000,
		.op_pixel_clk = 264000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x660,
		.y_output = 0x4C8,
		.line_length_pclk = 0xe18,
		.frame_length_lines = 0x50c,
		.vt_pixel_clk = 140000000,
		.op_pixel_clk = 264000000,
		.binning_factor = 2,
	},
	{
		.x_output = 0x780, /*1920*/
		.y_output = 0x438, /*1080*/
		.line_length_pclk = 0xe18, /*3568*/
		.frame_length_lines = 0xa1a, /*1868*/
		.vt_pixel_clk = 140000000,
		.op_pixel_clk = 264000000,
		.binning_factor = 1,
	},
};

static struct msm_sensor_output_reg_addr_t ov8835_reg_addr = {
	.x_output = 0x3808,
	.y_output = 0x380a,
	.line_length_pclk = 0x380c,
	.frame_length_lines = 0x380e,
};

/* Use address 300A with word length id 8830 if OTP not programmed */
static struct msm_sensor_id_info_t ov8835_id_info = {
	.sensor_id_reg_addr = 0x3D00,
	.sensor_id = 0x35,
};

/* TBD: Need to revisit*/
static struct msm_sensor_exp_gain_info_t ov8835_exp_gain_info = {
	.coarse_int_time_addr = 0x3501,
	.global_gain_addr = 0x350A,
	.vert_offset = 6,
};

/* TBD: Need to revisit*/
static int32_t ov8835_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
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

static int32_t ov8835_regulator_on(struct regulator **reg,
				struct device *dev, char *regname, int uV)
{
	int32_t rc = 0;

	pr_debug("ov8835_regulator_on: %s %d\n", regname, uV);

	*reg = regulator_get(dev, regname);
	if (IS_ERR(*reg)) {
		pr_err("ov8835: failed to get %s (%ld)\n",
				regname, PTR_ERR(*reg));
		goto reg_on_done;
	}

	if (uV != 0) {
		rc = regulator_set_voltage(*reg, uV, uV);
		if (rc) {
			pr_err("ov8835: failed to set voltage for %s (%d)\n",
					regname, rc);
			goto reg_on_done;
		}
	}

	rc = regulator_enable(*reg);
	if (rc) {
		pr_err("ov8835: failed to enable %s (%d)\n",
				regname, rc);
		goto reg_on_done;
	}

reg_on_done:
	return rc;
}

static int32_t ov8835_regulator_off(struct regulator **reg, char *regname)
{
	int32_t rc = 0;

	if (regname)
		pr_debug("ov8835_regulator_off: %s\n", regname);

	if (*reg == NULL) {
		if (regname)
			pr_err("ov8835_regulator_off: %s is null, aborting\n",
								regname);
		rc = -EINVAL;
		goto reg_off_done;
	}

	rc = regulator_disable(*reg);
	if (rc) {
		if (regname)
			pr_err("ov8835: failed to disable %s (%d)\n",
								regname, rc);
		goto reg_off_done;
	}

	regulator_put(*reg);

reg_off_done:
	return rc;
}


static int32_t ov8835_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device *dev = &s_ctrl->sensor_i2c_client->client->dev;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	pr_debug("ov8835_power_up\n");

	/* Request gpios */
	rc = gpio_request(info->sensor_pwd, "ov8835");
	if (rc < 0) {
		pr_err("ov8835: gpio request sensor_pwd failed (%d)\n", rc);
		goto power_up_done;
	}

	rc = gpio_request(info->sensor_reset, "ov8835");
	if (rc < 0) {
		pr_err("ov8835: gpio request sensor_reset failed (%d)\n", rc);
		goto power_up_done;
	}

	/* Enable supplies */
	rc = ov8835_regulator_on(&cam_vdig, dev, "cam_vdig", 1200000);
	if (rc < 0)
		goto power_up_done;

	rc = ov8835_regulator_on(&cam_vio, dev, "cam_vio", 0);
	if (rc < 0)
		goto power_up_done;

	/* Not every board needs this so ignore error */
	rc = ov8835_regulator_on(&cam_mipi_mux, dev, "cam_mipi_mux", 2800000);

	/* Set reset low */
	gpio_direction_output(info->sensor_reset, 0);

	/* Wait for core supplies to power up */
	usleep(10000);

	/*Enable MCLK*/
	cam_mot_8960_clk_info->clk_rate = s_ctrl->clk_rate;
	rc = msm_cam_clk_enable(dev, cam_mot_8960_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_mot_8960_clk_info), 1);
	if (rc < 0) {
		pr_err("ov8835: msm_cam_clk_enable failed (%d)\n", rc);
		goto power_up_done;
	}
	usleep(20000);

	/* Set pwd high */
	gpio_direction_output(info->sensor_pwd, 1);
	usleep(26000);

	/* Set reset high */
	gpio_direction_output(info->sensor_reset, 1);
	usleep(35000);

power_up_done:
	return rc;
}

static int32_t ov8835_power_down(
		struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device *dev = &s_ctrl->sensor_i2c_client->client->dev;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	pr_debug("ov8835_power_down\n");

	/*Set Reset Low*/
	gpio_direction_output(info->sensor_reset, 0);
	usleep(10000);

	/*Disable MCLK*/
	rc = msm_cam_clk_enable(dev, cam_mot_8960_clk_info, s_ctrl->cam_clk,
			ARRAY_SIZE(cam_mot_8960_clk_info), 0);
	if (rc < 0)
		pr_err("ov8835: msm_cam_clk_enable failed (%d)\n", rc);
	usleep(10000);

	/* Disable supplies */
	rc = ov8835_regulator_off(&cam_vio, "cam_vio");
	if (rc < 0)
		pr_err("ov8835: regulator off for cam_vio failed (%d)\n", rc);

	rc = ov8835_regulator_off(&cam_vdig, "cam_vdig");
	if (rc < 0)
		pr_err("ov8835: regulator off for cam_vdig failed (%d)\n",
									rc);

	/* Not every board needs this so ignore error */
	ov8835_regulator_off(&cam_mipi_mux, NULL);

	/* Wait for core to shut off */
	usleep(10000);

	/*Set pwd low*/
	gpio_direction_output(info->sensor_pwd, 0);

	/*Clean up*/
	gpio_free(info->sensor_pwd);
	gpio_free(info->sensor_reset);

	return rc;
}

static const struct i2c_device_id ov8835_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov8835_s_ctrl},
	{ }
};

static struct i2c_driver ov8835_i2c_driver = {
	.id_table = ov8835_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov8835_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int32_t ov8835_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	/*Select Bank 0*/
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x3D84, 0xC0, MSM_CAMERA_I2C_BYTE_DATA);
	/*Enable OTP READ*/
	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			0x3D81, 0x01, MSM_CAMERA_I2C_BYTE_DATA);
	/*wait 10ms*/
	usleep(10000);
	/*Read sensor id*/
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: read id failed\n", __func__);
		return rc;
	}

	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("%s: chip id %x does not match expected %x\n", __func__,
				chipid, s_ctrl->sensor_id_info->sensor_id);
		return -ENODEV;
	}

	pr_debug("ov8835: match_id success\n");
	return 0;
}

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&ov8835_i2c_driver);
}

static struct v4l2_subdev_core_ops ov8835_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov8835_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov8835_subdev_ops = {
	.core = &ov8835_subdev_core_ops,
	.video = &ov8835_subdev_video_ops,
};

static struct msm_sensor_fn_t ov8835_func_tbl = {
	.sensor_start_stream            = msm_sensor_start_stream,
	.sensor_stop_stream             = msm_sensor_stop_stream,
	.sensor_group_hold_on           = msm_sensor_group_hold_on,
	.sensor_group_hold_off          = msm_sensor_group_hold_off,
	.sensor_set_fps                 = msm_sensor_set_fps,
	.sensor_write_exp_gain          = ov8835_write_exp_gain,
	.sensor_write_snapshot_exp_gain = ov8835_write_exp_gain,
	.sensor_setting                 = msm_sensor_setting,
	.sensor_set_sensor_mode         = msm_sensor_set_sensor_mode,
	.sensor_mode_init               = msm_sensor_mode_init,
	.sensor_get_output_info         = msm_sensor_get_output_info,
	.sensor_config                  = msm_sensor_config,
	.sensor_power_up                = ov8835_power_up,
	.sensor_power_down              = ov8835_power_down,
	.sensor_match_id                = ov8835_match_id,
	.sensor_get_csi_params          = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t ov8835_regs = {
	.default_data_type        = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf        = ov8835_start_settings,
	.start_stream_conf_size   = ARRAY_SIZE(ov8835_start_settings),
	.stop_stream_conf         = ov8835_stop_settings,
	.stop_stream_conf_size    = ARRAY_SIZE(ov8835_stop_settings),
	.group_hold_on_conf       = ov8835_groupon_settings,
	.group_hold_on_conf_size  = ARRAY_SIZE(ov8835_groupon_settings),
	.group_hold_off_conf      = ov8835_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(ov8835_groupoff_settings),
	.init_settings            = &ov8835_init_conf[0],
	.init_size                = ARRAY_SIZE(ov8835_init_conf),
	.mode_settings            = &ov8835_confs[0],
	.output_settings          = &ov8835_dimensions[0],
	.num_conf                 = ARRAY_SIZE(ov8835_confs),
};

static struct msm_sensor_ctrl_t ov8835_s_ctrl = {
	.msm_sensor_reg               = &ov8835_regs,
	.sensor_i2c_client            = &ov8835_sensor_i2c_client,
	.sensor_i2c_addr              = 0x20,
	.sensor_output_reg_addr       = &ov8835_reg_addr,
	.sensor_id_info               = &ov8835_id_info,
	.sensor_exp_gain_info         = &ov8835_exp_gain_info,
	.cam_mode                     = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex             = &ov8835_mut,
	.sensor_i2c_driver            = &ov8835_i2c_driver,
	.sensor_v4l2_subdev_info      = ov8835_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov8835_subdev_info),
	.sensor_v4l2_subdev_ops       = &ov8835_subdev_ops,
	.func_tbl                     = &ov8835_func_tbl,
	.clk_rate                     = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision 8835 Bayer sensor driver");
MODULE_LICENSE("GPL v2");
