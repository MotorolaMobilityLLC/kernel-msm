/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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
#define SENSOR_NAME "imx119"
#define PLATFORM_DRIVER_NAME "msm_camera_imx119"
#define imx119_obj imx119_##obj

DEFINE_MUTEX(imx119_mut);
static struct msm_sensor_ctrl_t imx119_s_ctrl;

static struct msm_camera_i2c_reg_conf imx119_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf imx119_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf imx119_groupon_settings[] = {
	{0x104, 0x01},
};

static struct msm_camera_i2c_reg_conf imx119_groupoff_settings[] = {
	{0x104, 0x00},
};

static struct msm_camera_i2c_reg_conf imx119_prev_settings[] = {
	{0x0101, 0x03}, /* read out direction */
	{0x0340, 0x04},
	{0x0341, 0x28},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x034A, 0x04},
	{0x034B, 0x0F},
	{0x034C, 0x05},
	{0x034D, 0x10},
	{0x034E, 0x04},
	{0x034F, 0x10},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x3001, 0x00},
	{0x3016, 0x02},
	{0x3060, 0x30},
	{0x30E8, 0x00},
	{0x3301, 0x05},
	{0x308A, 0x43},
	{0x3305, 0x03},
	{0x3309, 0x05},
	{0x330B, 0x03},
	{0x330D, 0x05},
};

static struct msm_camera_i2c_reg_conf imx119_recommend_settings[] = {
	{0x0305, 0x02},
	{0x0307, 0x26},
	{0x3025, 0x0A},
	{0x302B, 0x4B},
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x301C, 0x02},
	{0x302C, 0x85},
	{0x303A, 0xA4},
	{0x3108, 0x25},
	{0x310A, 0x27},
	{0x3122, 0x26},
	{0x3138, 0x26},
	{0x313A, 0x27},
	{0x316D, 0x0A},
	{0x308C, 0x00},
	{0x302E, 0x8C},
	{0x302F, 0x81},
	{0x0101, 0x03},
};

static struct v4l2_subdev_info imx119_subdev_info[] = {
	{
	.code = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt = 1,
	.order = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array imx119_init_conf[] = {
	{&imx119_recommend_settings[0],
	ARRAY_SIZE(imx119_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array imx119_confs[] = {
	{&imx119_prev_settings[0],
	ARRAY_SIZE(imx119_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t imx119_dimensions[] = {
	{
		.x_output = 0x510,
		.y_output = 0x410,
		.line_length_pclk = 0x570,
		.frame_length_lines = 0x432,
		.vt_pixel_clk = 45600000,
		.op_pixel_clk = 45600000,
	},
};

static struct msm_camera_csid_vc_cfg imx119_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
	{2, CSI_RESERVED_DATA_0, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params imx119_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 1,
		.lut_params = {
			.num_cid = 3,
			.vc_cfg = imx119_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 1,
		.settle_cnt = 0x1B,
	},
};

static struct msm_camera_csi2_params *imx119_csi_params_array[] = {
	&imx119_csi_params,
};

static struct msm_sensor_output_reg_addr_t imx119_reg_addr = {
	.x_output = 0x34C,
	.y_output = 0x34E,
	.line_length_pclk = 0x342,
	.frame_length_lines = 0x340,
};

static struct msm_sensor_id_info_t imx119_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x119,
};

static struct msm_sensor_exp_gain_info_t imx119_exp_gain_info = {
	.coarse_int_time_addr = 0x202,
	.global_gain_addr = 0x204,
	.vert_offset = 3,
};


static const struct i2c_device_id imx119_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&imx119_s_ctrl},
	{ }
};

static struct i2c_driver imx119_i2c_driver = {
	.id_table = imx119_i2c_id,
	.probe = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx119_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&imx119_i2c_driver);
}

static struct v4l2_subdev_core_ops imx119_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};
static struct v4l2_subdev_video_ops imx119_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops imx119_subdev_ops = {
	.core = &imx119_subdev_core_ops,
	.video = &imx119_subdev_video_ops,
};

static struct msm_sensor_fn_t imx119_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_write_snapshot_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t imx119_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = imx119_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(imx119_start_settings),
	.stop_stream_conf = imx119_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(imx119_stop_settings),
	.group_hold_on_conf = imx119_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(imx119_groupon_settings),
	.group_hold_off_conf = imx119_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(imx119_groupoff_settings),
	.init_settings = &imx119_init_conf[0],
	.init_size = ARRAY_SIZE(imx119_init_conf),
	.mode_settings = &imx119_confs[0],
	.output_settings = &imx119_dimensions[0],
	.num_conf = ARRAY_SIZE(imx119_confs),
};

static struct msm_sensor_ctrl_t imx119_s_ctrl = {
	.msm_sensor_reg = &imx119_regs,
	.sensor_i2c_client = &imx119_sensor_i2c_client,
	.sensor_i2c_addr = 0x6E,
	.sensor_output_reg_addr = &imx119_reg_addr,
	.sensor_id_info = &imx119_id_info,
	.sensor_exp_gain_info = &imx119_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &imx119_csi_params_array[0],
	.msm_sensor_mutex = &imx119_mut,
	.sensor_i2c_driver = &imx119_i2c_driver,
	.sensor_v4l2_subdev_info = imx119_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx119_subdev_info),
	.sensor_v4l2_subdev_ops = &imx119_subdev_ops,
	.func_tbl = &imx119_func_tbl,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Sony 1.3MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");

