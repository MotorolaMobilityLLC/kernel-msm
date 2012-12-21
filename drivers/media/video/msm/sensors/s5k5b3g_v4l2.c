/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.7006
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "msm_sensor.h"
#include <linux/regulator/consumer.h>

#ifndef SENSOR_NAME
#define SENSOR_NAME "s5k5b3g"
#endif

#ifdef SECOND_CAMERA
DEFINE_MUTEX(s5k5b3g_2nd_mut);
#else
DEFINE_MUTEX(s5k5b3g_mut);
#endif

static struct msm_sensor_ctrl_t s5k5b3g_s_ctrl;

static struct regulator *cam_vdig;
static struct regulator *cam_vio;
static struct regulator *cam_mipi_mux;

static struct msm_cam_clk_info cam_mot_8960_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_start_settings[] = {
	{0x0100, 0x01, MSM_CAMERA_I2C_BYTE_DATA},  /* */
};

static struct msm_camera_i2c_reg_conf s5k5b3g_stop_settings[] = {
	{0x0100, 0x00, MSM_CAMERA_I2C_BYTE_DATA},  /* */
};

static struct msm_camera_i2c_reg_conf s5k5b3g_groupon_settings[] = {
	{0x0104, 0x0001},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_groupoff_settings[] = {
	{0x0104, 0x0000},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_prev_settings[] = {
	/* Placeholder */
	{0x7006, 0x0000},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_snap_settings[] = {
	/* Placeholder */
	{0x7006, 0x0001},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_reset_settings[] = {
	{0x0103, 0x0001},
};

static struct msm_camera_i2c_reg_conf s5k5b3g_recommend_settings[] = {
	/* Settings from Samsung Dev Kit */
	{0x0B06, 0x0180,},/* */
	{0x0B08, 0x0180,},/* */
	{0x0B80, 0x00, MSM_CAMERA_I2C_BYTE_DATA},  /* */
	{0x0B00, 0x00, MSM_CAMERA_I2C_BYTE_DATA},  /* */

	/* PLL Parameters */
	{0x3000, 0x1800,},/* input_clock_mhz_int (24Mhz) */
	{0x0300, 0x0005,},/* vt_pix_clk_div (72Mhz) */
	{0x0302, 0x0002,},/* vt_sys_clk_div (360Mhz) */
	{0x0304, 0x0006,},/* pre_pll_clk_div (4Mhz) */
	{0x0306, 0x00B4,},/* pll_multiplier (720Mhz) */
	{0x0308, 0x000A,},/* op_pix_clk_div (72Mhz) */
	{0x030A, 0x0001,},/* op_sys_clk_div (720Mhz) */

	/* Frame Size */
	{0x0340, 0x0479,},/* Frame Lenght Lines */
	{0x0342, 0x0830,},/* Line Length Pclk */

	/* Input Size and Format */
	{0x0344, 0x0004,},/* Array X start*/
	{0x0346, 0x0000,},/* Array Y start*/
	{0x0348, 0x0783,},/* Array X end (1923) */
	{0x034A, 0x043F,},/* Array Y end (1087) */
	{0x0382, 0x0001,},/* */
	{0x0386, 0x0001,},/* */
	/*V 0 - Subsampling ; 1 - binning V*/
	{0x0900, 0x01, MSM_CAMERA_I2C_BYTE_DATA},
	/*V High nib vert;Low nib horiz; 1-Subsample; 2-Binning V*/
	{0x0901, 0x22, MSM_CAMERA_I2C_BYTE_DATA},
	{0x0101, 0x03, MSM_CAMERA_I2C_BYTE_DATA},  /* XY Mirror */

	/* Output Size and Format */
	{0x034C, 0x0780,},/* (1920) */
	{0x034E, 0x440,},/* (1088) */
	{0x0112, 0x0A0A,},/* 10 bit out */
	{0x0111, 0x02, MSM_CAMERA_I2C_BYTE_DATA},  /* MIPI */
	/*V PVI mode, no CCP2 framing V*/
	{0x30EE, 0x00, MSM_CAMERA_I2C_BYTE_DATA},
	{0x30F4, 0x0008,},/* RAW10 mode */
	{0x300C, 0x00, MSM_CAMERA_I2C_BYTE_DATA},  /* No embedded lines */
	/*V Horiz size not limited to mult of 16 V*/
	{0x30ED, 0x00, MSM_CAMERA_I2C_BYTE_DATA},
	{0x7088, 0x0157,},/* Increase drive strength on PCLK */
	{0xB0E6, 0x0000,},/* */

	/* Integration Time */
	{0x0200, 0x0200,},/* */
	{0x0202, 0x0475,},/* (33ms) */

	/* Analog Gain */
	{0x0120, 0x0000,},/* Global analog gain (not per channel) */
	/* gain=128/(x+16) ; x = 128/gain - 16 */
	/* AGx1 0070 */
	/* AGx2 0030 */
	/* AGx3 001B */
	/* AGx4 0010 */
	/* AGx5 000A */
	/* AGx6 0005 */
	/* AGx8 0000 */
	{0x0204, 0x0070,},/* AGx1 */

	/* Digital Gain */
	{0x020E, 0x0100,},/* */
	{0x0210, 0x0100,},/* */
	{0x0212, 0x0100,},/* */
	{0x0214, 0x0100,},/* */

	/* #smiaRegs_vendor_emb_use_header //for Frame count */
	{0x300C, 0x01, MSM_CAMERA_I2C_BYTE_DATA},  /* */
};

static struct v4l2_subdev_info s5k5b3g_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array s5k5b3g_init_conf[] = {
	{&s5k5b3g_reset_settings[0],
	ARRAY_SIZE(s5k5b3g_reset_settings), 50, MSM_CAMERA_I2C_WORD_DATA},
	{&s5k5b3g_recommend_settings[0],
	ARRAY_SIZE(s5k5b3g_recommend_settings), 0, MSM_CAMERA_I2C_WORD_DATA}
};

static struct msm_camera_i2c_conf_array s5k5b3g_confs[] = {
	{&s5k5b3g_snap_settings[0],
	ARRAY_SIZE(s5k5b3g_snap_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
	{&s5k5b3g_prev_settings[0],
	ARRAY_SIZE(s5k5b3g_prev_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_sensor_output_info_t s5k5b3g_dimensions[] = {
	{
		.x_output = 0x780,
		.y_output = 0x440,
		.line_length_pclk = 0x830,
		.frame_length_lines = 0x479,
		.vt_pixel_clk = 72000000,
		.op_pixel_clk = 72000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x780,
		.y_output = 0x440,
		.line_length_pclk = 0x830,
		.frame_length_lines = 0x479,
		.vt_pixel_clk = 72000000,
		.op_pixel_clk = 72000000,
		.binning_factor = 1,
	},
};

static struct msm_sensor_output_reg_addr_t s5k5b3g_reg_addr = {
	.x_output = 0x348,
	.y_output = 0x34A,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,
};

static struct msm_sensor_id_info_t s5k5b3g_id_info = {
	.sensor_id_reg_addr = 0x7006,
	.sensor_id = 0x05B3,
};

static struct msm_sensor_exp_gain_info_t s5k5b3g_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0204,
	.vert_offset = 6,
};

static int32_t s5k5b3g_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines, offset;
	int32_t rc = 0;

	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);

	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);

	if (rc < 0) {
		pr_err("%s: write frame length register failed\n", __func__);
		return rc;
	}

	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
		line, MSM_CAMERA_I2C_WORD_DATA);

	if (rc < 0) {
		pr_err("%s: write coarse integration register failed\n",
			__func__);
		return rc;
	}

	rc = msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);

	if (rc < 0) {
		pr_err("%s: write global gain register failed\n", __func__);
		return rc;
	}

	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);

	return rc;
}

static int32_t s5k5b3g_regulator_on(struct regulator **reg,
				struct device *dev, char *regname, int uV)
{
	int32_t rc;

	pr_debug("s5k5b3g_regulator_on: %s %d\n", regname, uV);

	*reg = regulator_get(dev, regname);
	if (IS_ERR(*reg)) {
		pr_err("s5k5b3g: failed to get %s (%d)\n",
				regname, rc = PTR_ERR(*reg));
		goto reg_on_done;
	}

	if (uV != 0) {
		rc = regulator_set_voltage(*reg, uV, uV);
		if (rc) {
			pr_err("s5k5b3g: failed to set voltage for %s (%d)\n",
					regname, rc);
			goto reg_on_done;
		}
	}

	rc = regulator_enable(*reg);
	if (rc) {
		pr_err("s5k5b3g: failed to enable %s (%d)\n",
				regname, rc);
		goto reg_on_done;
	}

reg_on_done:
	return rc;
}

static int32_t s5k5b3g_regulator_off(struct regulator **reg, char *regname)
{
	int32_t rc;

	if (regname)
		pr_debug("s5k5b3g_regulator_off: %s\n", regname);

	if (*reg == NULL) {
		if (regname)
			pr_err("s5k5b3g_regulator_off: %s is null, aborting\n",
								regname);
		rc = -EINVAL;
		goto reg_off_done;
	}

	rc = regulator_disable(*reg);
	if (rc) {
		if (regname)
			pr_err("s5k5b3g: failed to disable %s (%d)\n",
								regname, rc);
		goto reg_off_done;
	}

	regulator_put(*reg);

reg_off_done:
	return rc;
}


static int32_t s5k5b3g_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc;
	struct device *dev = &s_ctrl->sensor_i2c_client->client->dev;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	pr_debug("s5k5b3g_power_up\n");

	if (!info->oem_data) {
		pr_err("s5k5b3g: oem data NULL in sensor info, aborting");
		rc = -EINVAL;
		goto power_up_done;
	}

	/* Request gpios */
	rc = gpio_request(info->oem_data->sensor_avdd_en, "s5k5b3g");
	if (rc < 0) {
		pr_err("s5k5b3g: gpio request sensor_avdd_en failed (%d)\n", rc);
		goto power_up_done;
	}

	rc = gpio_request(info->sensor_reset, "s5k5b3g");
	if (rc < 0) {
		pr_err("s5k5b3g: gpio request sensor_reset failed (%d)\n", rc);
		goto abort0;
	}

	rc = gpio_request(info->oem_data->sensor_dig_en, "s5k5b3g");
	if (rc < 0) {
		pr_err("s5k5b3g: gpio request sensor_dig_en failed (%d)\n", rc);
		goto abort1;
	}

	/* Set reset low */
	gpio_direction_output(info->sensor_reset, 0);

	/* Enable supplies */
	rc = s5k5b3g_regulator_on(&cam_vio, dev, "cam_vio", 0);
	if (rc < 0)
		goto abort2;

	rc = s5k5b3g_regulator_on(&cam_vdig, dev, "cam_vdig", 1200000);
	if (rc < 0)
		goto abort2;

	/* Not every board will use this so if devtree doesn't define it just
	   ignore the error and move on */
	rc = s5k5b3g_regulator_on(&cam_mipi_mux, dev, "cam_mipi_mux", 2800000);

	/* Wait for core supplies to power up */
	usleep_range(10000, 15000);

	/* Set dig_en high */
	gpio_direction_output(info->oem_data->sensor_dig_en, 1);

	/*Enable MCLK*/
	cam_mot_8960_clk_info->clk_rate = s_ctrl->clk_rate;
	rc = msm_cam_clk_enable(dev, cam_mot_8960_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_mot_8960_clk_info), 1);
	if (rc < 0) {
		pr_err("s5k5b3g: msm_cam_clk_enable failed (%d)\n", rc);
		goto abort2;
	}

	/* Set avdd_en high */
	gpio_direction_output(info->oem_data->sensor_avdd_en, 1);

	/* Set reset high */
	gpio_direction_output(info->sensor_reset, 1);
	usleep_range(1000, 2000);

	goto power_up_done;


	/* Cleanup, ignore errors during abort */
abort2:
	gpio_free(info->oem_data->sensor_dig_en);
abort1:
	gpio_free(info->sensor_reset);
abort0:
	gpio_free(info->oem_data->sensor_avdd_en);

	s5k5b3g_regulator_off(&cam_vio, NULL);
	s5k5b3g_regulator_off(&cam_vdig, NULL);
	s5k5b3g_regulator_off(&cam_mipi_mux, NULL);

power_up_done:
	return rc;
}

static int32_t s5k5b3g_power_down(
		struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc;
	struct device *dev = &s_ctrl->sensor_i2c_client->client->dev;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	pr_debug("s5k5b3g_power_down\n");

	/*Set Reset Low*/
	gpio_direction_output(info->sensor_reset, 0);
	usleep_range(1000, 2000);

	/*Set avdd_en low*/
	gpio_direction_output(info->oem_data->sensor_avdd_en, 0);

	/*Set dig_en low*/
	gpio_direction_output(info->oem_data->sensor_dig_en, 0);

	/* Disable supplies */

	/* Not every board needs this so ignore error */
	s5k5b3g_regulator_off(&cam_mipi_mux, NULL);

	rc = s5k5b3g_regulator_off(&cam_vdig, "cam_vdig");
	if (rc < 0)
		pr_err("s5k5b3g: regulator off for cam_vdig failed (%d)\n", rc);

	rc = s5k5b3g_regulator_off(&cam_vio, "cam_vio");
	if (rc < 0)
		pr_err("s5k5b3g: regulator off for cam_vio failed (%d)\n", rc);

	/*Disable MCLK*/
	rc = msm_cam_clk_enable(dev, cam_mot_8960_clk_info, s_ctrl->cam_clk,
			ARRAY_SIZE(cam_mot_8960_clk_info), 0);
	if (rc < 0)
		pr_err("s5k5b3g: msm_cam_clk_enable failed (%d)\n", rc);

	/*Clean up*/
	gpio_free(info->oem_data->sensor_avdd_en);
	gpio_free(info->sensor_reset);
	gpio_free(info->oem_data->sensor_dig_en);

	return rc;
}

static const struct i2c_device_id s5k5b3g_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&s5k5b3g_s_ctrl},
	{ }
};

static struct i2c_driver s5k5b3g_i2c_driver = {
	.id_table = s5k5b3g_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k5b3g_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&s5k5b3g_i2c_driver);
}

static struct v4l2_subdev_core_ops s5k5b3g_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops s5k5b3g_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops s5k5b3g_subdev_ops = {
	.core = &s5k5b3g_subdev_core_ops,
	.video  = &s5k5b3g_subdev_video_ops,
};

static struct msm_sensor_fn_t s5k5b3g_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = s5k5b3g_write_exp_gain,
	.sensor_write_snapshot_exp_gain = s5k5b3g_write_exp_gain,
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = s5k5b3g_power_up,
	.sensor_power_down = s5k5b3g_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t s5k5b3g_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = s5k5b3g_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(s5k5b3g_start_settings),
	.stop_stream_conf = s5k5b3g_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(s5k5b3g_stop_settings),
	.group_hold_on_conf = s5k5b3g_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(s5k5b3g_groupon_settings),
	.group_hold_off_conf = s5k5b3g_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(s5k5b3g_groupoff_settings),
	.init_settings = &s5k5b3g_init_conf[0],
	.init_size = ARRAY_SIZE(s5k5b3g_init_conf),
	.mode_settings = &s5k5b3g_confs[0],
	.output_settings = &s5k5b3g_dimensions[0],
	.num_conf = ARRAY_SIZE(s5k5b3g_confs),
};

static struct msm_sensor_ctrl_t s5k5b3g_s_ctrl = {
	.msm_sensor_reg = &s5k5b3g_regs,
	.sensor_i2c_client = &s5k5b3g_sensor_i2c_client,
	.sensor_i2c_addr = 0x5a,
	.sensor_output_reg_addr = &s5k5b3g_reg_addr,
	.sensor_id_info = &s5k5b3g_id_info,
	.sensor_exp_gain_info = &s5k5b3g_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
#ifdef SECOND_CAMERA
	.msm_sensor_mutex = &s5k5b3g_2nd_mut,
#else
	.msm_sensor_mutex = &s5k5b3g_mut,
#endif
	.sensor_i2c_driver = &s5k5b3g_i2c_driver,
	.sensor_v4l2_subdev_info = s5k5b3g_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k5b3g_subdev_info),
	.sensor_v4l2_subdev_ops = &s5k5b3g_subdev_ops,
	.func_tbl = &s5k5b3g_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
#ifdef SECOND_CAMERA
MODULE_DESCRIPTION("Samsung S5K5B3G Bayer sensor driver (alternate)");
#else
MODULE_DESCRIPTION("Samsung S5K5B3G Bayer sensor driver");
#endif
MODULE_LICENSE("GPL v2");


