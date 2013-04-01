/* Copyright (c) 2013, The Linux Foundation. All Rights Reserved.
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
#define SENSOR_NAME "ov9724"
#define PLATFORM_DRIVER_NAME "msm_camera_ov9724"
#define ov9724_obj ov9724_##obj

DEFINE_MUTEX(ov9724_mut);
static struct msm_sensor_ctrl_t ov9724_s_ctrl;

static struct msm_camera_i2c_reg_conf ov9724_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf ov9724_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf ov9724_groupon_settings[] = {
	{0x0104, 0x01},
};

static struct msm_camera_i2c_reg_conf ov9724_groupoff_settings[] = {
	{0x0104, 0x00},
};

static struct msm_camera_i2c_reg_conf ov9724_prev_settings[] = {
};

static struct msm_camera_i2c_reg_conf ov9724_recommend_settings[] = {
	{0x0103, 0x01}, /* SOFTWARE_RESET */
	{0x3002, 0x03}, /* IO_CTRL00 */
	{0x0340, 0x03}, /* FRAME_LENGTH_LINES_HI */
	{0x0341, 0xC1}, /* FRAME_LENGTH_LINES_LO */
	{0x0342, 0x06}, /* LINE_LENGTH_PCK_HI */
	{0x0343, 0x80}, /* LINE_LENGTH_PCK_LO */
	{0x0202, 0x03}, /* COARSE_INTEGRATION_TIME_HI */
	{0x0203, 0x43}, /* COARSE_INTEGRATION_TIME_LO */
	{0x034c, 0x05}, /* X_OUTPUT_SIZE_HI */
	{0x034d, 0x10}, /* X_OUTPUT_SIZE_LO */
	{0x034e, 0x02}, /* Y_OUTPUT_SIZE_HI */
	{0x034f, 0xd8}, /* Y_OUTPUT_SIZE_LO */
	{0x0340, 0x03}, /* FRAME_LENGTH_LINES_HI */
	{0x0341, 0xC1}, /* FRAME_LENGTH_LINES_LO */
	{0x0342, 0x06}, /* LINE_LENGTH_PCK_HI */
	{0x0343, 0x80}, /* LINE_LENGTH_PCK_LO */
	{0x0202, 0x03}, /* COARSE_INTEGRATION_TIME_HI */
	{0x0203, 0x43}, /* COARSE_INTEGRATION_TIME_LO */
	{0x0303, 0x01}, /* VT_SYS_CLK_DIV_LO */
	{0x3002, 0x03}, /* IO_CTRL00 */
	{0x4801, 0x0e}, /* MIPI_CTRL01 */
	{0x4803, 0x05}, /* MIPI_CTRL03 */
	{0x0305, 0x04}, /* PRE_PLL_CLK_DIV_LO */
	{0x0307, 0x64}, /* PLL_MULTIPLIER_LO */
	{0x0101, 0x02}, /* IMAGE_ORIENTATION */
	{0x5000, 0x06}, /* ISP_CTRL0 */
	{0x5001, 0x71}, /* ISP_CTRL1 */
	{0x4008, 0x10}, /* BLC_CTRL05 */
	{0x0100, 0x01}, /* MODE_SELECT */
};

static struct v4l2_subdev_info ov9724_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array ov9724_init_conf[] = {
	{&ov9724_recommend_settings[0],
		ARRAY_SIZE(ov9724_recommend_settings), 0,
		MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array ov9724_confs[] = {
	{&ov9724_prev_settings[0],
		ARRAY_SIZE(ov9724_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t ov9724_dimensions[] = {
	{
		.x_output = 0x510, /* 1296 */
		.y_output = 0x2d8, /* 728 */
		.line_length_pclk = 0x680, /* 1664 */
		.frame_length_lines = 0x3C1, /* 961 */
		.vt_pixel_clk = 320000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
};

static struct msm_sensor_output_reg_addr_t ov9724_reg_addr = {
	.x_output = 0x034c,
	.y_output = 0x034e,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t ov9724_id_info = {
	.sensor_id_reg_addr = 0x0000,
	.sensor_id = 0x9724,
};

static struct msm_sensor_exp_gain_info_t ov9724_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0205,
	.vert_offset = 6,
};

static const struct i2c_device_id ov9724_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov9724_s_ctrl},
	{ }
};

static struct i2c_driver ov9724_i2c_driver = {
	.id_table = ov9724_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov9724_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&ov9724_i2c_driver);
}


int32_t ov9724_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line, int32_t luma_avg, uint16_t fgain)
{
	uint32_t fl_lines;
	uint8_t offset;
	uint32_t gain_reg = 0;
	uint8_t  gain_offset = 4;
	fl_lines = s_ctrl->curr_frame_length_lines;
	fl_lines = (fl_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_output_reg_addr->frame_length_lines,
			fl_lines, MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
			line, MSM_CAMERA_I2C_WORD_DATA);

	while (gain > 31) {
		gain_reg = gain_reg | (1 << gain_offset);
		gain     = gain >> 1;
	}
	gain_reg = gain_reg | (gain - 16);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
			MSM_CAMERA_I2C_BYTE_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
}


static struct v4l2_subdev_core_ops ov9724_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov9724_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov9724_subdev_ops = {
	.core = &ov9724_subdev_core_ops,
	.video  = &ov9724_subdev_video_ops,
};

static struct msm_sensor_fn_t ov9724_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = ov9724_write_exp_gain,
	.sensor_write_snapshot_exp_gain = ov9724_write_exp_gain,
	.sensor_setting = msm_sensor_setting,
	.sensor_csi_setting = msm_sensor_setting1,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t ov9724_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = ov9724_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(ov9724_start_settings),
	.stop_stream_conf = ov9724_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(ov9724_stop_settings),
	.group_hold_on_conf = ov9724_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(ov9724_groupon_settings),
	.group_hold_off_conf = ov9724_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(ov9724_groupoff_settings),
	.init_settings = &ov9724_init_conf[0],
	.init_size = ARRAY_SIZE(ov9724_init_conf),
	.mode_settings = &ov9724_confs[0],
	.output_settings = &ov9724_dimensions[0],
	.num_conf = ARRAY_SIZE(ov9724_confs),
};

static struct msm_sensor_ctrl_t ov9724_s_ctrl = {
	.msm_sensor_reg = &ov9724_regs,
	.sensor_i2c_client = &ov9724_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &ov9724_reg_addr,
	.sensor_id_info = &ov9724_id_info,
	.sensor_exp_gain_info = &ov9724_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex = &ov9724_mut,
	.sensor_i2c_driver = &ov9724_i2c_driver,
	.sensor_v4l2_subdev_info = ov9724_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov9724_subdev_info),
	.sensor_v4l2_subdev_ops = &ov9724_subdev_ops,
	.func_tbl = &ov9724_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision WXGA Bayer sensor driver");
MODULE_LICENSE("GPL v2");

