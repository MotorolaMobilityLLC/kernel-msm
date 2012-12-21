/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2012, Aptina Imaging, Inc. All rights reserved.
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
#define SENSOR_NAME "ar0834"
#define PLATFORM_DRIVER_NAME "msm_camera_ar0834"

DEFINE_MUTEX(ar0834_mut);
static struct msm_sensor_ctrl_t ar0834_s_ctrl;

static struct regulator *cam_vdig;
static struct regulator *cam_vio;
static struct regulator *cam_mipi_mux;

/*#define QTR*/
#define STUB_GAINS

static struct msm_cam_clk_info cam_mot_8960_clk_info[] = {
	{"cam_clk", MSM_SENSOR_MCLK_24HZ},
};

static struct msm_camera_i2c_reg_conf ar0834_start_settings[] = {
	{0x301A, 0x8250},
	{0x301A, 0x8650},
	{0x301A, 0x8658},
	{0x0104, 0x00, MSM_CAMERA_I2C_BYTE_DATA},
	{0x301A, 0x065C},
};

static struct msm_camera_i2c_reg_conf ar0834_stop_settings[] = {
	{0x301A, 0x0058},
	{0x301A, 0x0050},
	{0x0104, 0x01, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_reg_conf ar0834_groupon_settings[] = {
	{0x0104, 0x01, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_reg_conf ar0834_groupoff_settings[] = {
	{0x0104, 0x00, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_camera_i2c_reg_conf ar0834_prev_settings[] = {
#ifdef QTR
	/*Output Size (1632x1224)*/
	{0x0342, 0x1720}, /* LINE_LENGTH_PCK 5920 */
	{0x0340, 0x0547}, /* FRAME_LENGTH_LINES 1351 */
	{0x0202, 0x0547}, /* COARSE_INTEGRATION_TIME 1351 */
	{0x0344, 0x0008}, /* X_ADDR_START 8 */
	{0x0348, 0x0CC5}, /* X_ADDR_END 3269 */
	{0x0346, 0x0008}, /* Y_ADDR_START 8 */
	{0x034A, 0x0995}, /* Y_ADDR_END 2453 */
	{0x034C, 0x0660}, /* X_OUTPUT_SIZE 1632 */
	{0x034E, 0x04C8}, /* Y_OUTPUT_SIZE 1224 */
	{0x3040, 0x48C3}, /* X_ODD_INC and Y_ODD_INC */
#else
	/*Output Size (3264x2448)*/
	{0x0342, 0x0ECC}, /* LINE_LENGTH_PCK 3788 */
	{0x0340, 0x0A0F}, /* FRAME_LENGTH_LINES 2575 */
	{0x0202, 0x083F}, /* COARSE_INTEGRATION_TIME 2111 */
	{0x0344, 0x0008}, /* X_ADDR_START 8 */
	{0x0348, 0x0CC7}, /* X_ADDR_END 3271 */
	{0x0346, 0x0008}, /* Y_ADDR_START 8 */
	{0x034A, 0x0997}, /* Y_ADDR_END 2455 */
	{0x034C, 0x0CC0}, /* X_OUTPUT_SIZE 3264 */
	{0x034E, 0x0990}, /* Y_OUTPUT_SIZE 2448 */
	{0x3040, 0x4041}, /* X_ODD_INC and Y_ODD_INC */
#endif
};

static struct msm_camera_i2c_reg_conf ar0834_recommend_settings[] = {
	{0x0300, 0x0005},
	{0x0302, 0x0001},
	{0x0304, /*0x0004*/0x0008}, /* FIXME: using bigger divider */
	{0x0306, 0x0064},
	{0x0308, 0x000A},
	{0x030A, 0x0001},
	{0x3064, 0x7800},
	{1, 0, 0, MSM_CAMERA_I2C_CMD_DELAYMS},

	{0x0400, 0x0000},
	{0x0402, 0x0000},
	{0x0404, 0x0010},
	{0x0408, 0x1010},
	{0x040A, 0x0210},
	{0x306E, 0x9080},

	{0x301A, 0x001C},

	{0x31B0, 0x0037},
	{0x31B2, 0x0021},
	{0x31B4, 0x2945},
	{0x31B6, 0x1191},
	{0x31B8, 0x3049},
	{0x31BA, 0x0188},
	{0x31BC, 0x8007},
	{1, 0, 0, MSM_CAMERA_I2C_CMD_DELAYMS},

	{0x3042, 0x0000},
	{0x30C0, 0x1810},
	{0x30C8, 0x0018},
	{0x30D2, 0x0000},
	{0x30D4, 0xD030},
	{0x30D6, 0x2200},
	{0x30DA, 0x0080},
	{0x30DC, 0x0080},
	{0x30EE, 0x0340},
	{0x316A, 0x8800},
	{0x316C, 0x8200},
	{0x3172, 0x0286},
	{0x3174, 0x8000},
	{0x317C, 0xE103},
	{0x3180, 0xF0FF},
	{0x31E0, 0x0741},
	{0x3ECC, 0x0056},
	{0x3ED0, 0xA8AA},
	{0x3ED2, 0xAAA8},
	{0x3ED4, 0x8ACC},
	{0x3ED8, 0x7288},
	{0x3EDA, 0x77CA},
	{0x3EDE, 0x6664},
	{0x3EE0, 0x26D5},
	{0x3EE4, 0x1548},
	{0x3EE6, 0xB10C},
	{0x3EE8, 0x6E79},
	{0x3EFE, 0x77CC},
	{0x31E6, 0x0000},
	{0x3F00, 0x0028},
	{0x3F02, 0x0140},
	{0x3F04, 0x0002},
	{0x3F06, 0x0004},
	{0x3F08, 0x0008},
	{0x3F0A, 0x0B09},
	{0x3F0C, 0x0302},
	{0x3F10, 0x0505},
	{0x3F12, 0x0303},
	{0x3F14, 0x0101},
	{0x3F16, 0x2020},
	{0x3F18, 0x0404},
	{0x3F1A, 0x7070},
	{0x3F1C, 0x003A},
	{0x3F1E, 0x003C},
	{0x3F2C, 0x2210},
	{0x3F40, 0x2020},
	{0x3F42, 0x0808},
	{0x3F44, 0x0101},

	{50, 0, 0, MSM_CAMERA_I2C_CMD_DELAYMS},
	{0x30D4, 0x3030},
	{0x3ED0, 0xC8AA},
	{0x3EE4, 0x3548},
	{0x3F20, 0x0209},
	{0x30EE, 0x0340},
	{0x3F2C, 0x2210},
	{0x3180, 0xB080},
	{0x3F38, 0x44A8},

	{0x3ED4, 0x6ACC},
	{0x3ED8, 0x5488},

	{0x316E, 0x8400},
};

static struct v4l2_subdev_info ar0834_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array ar0834_init_conf[] = {
	{&ar0834_recommend_settings[0],
	ARRAY_SIZE(ar0834_recommend_settings), 0, MSM_CAMERA_I2C_WORD_DATA}
};

static struct msm_camera_i2c_conf_array ar0834_confs[] = {
	{&ar0834_prev_settings[0],
	ARRAY_SIZE(ar0834_prev_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_sensor_output_info_t ar0834_dimensions[] = {
#ifdef QTR
	{
		.x_output = 0x660, /* 1632 */
		.y_output = 0x4C8, /* 1224 */
		.line_length_pclk = 0x1720, /* 5920 */
		.frame_length_lines = 0x547, /* 1351 */
		.vt_pixel_clk = 174000000,
		.op_pixel_clk = 174000000,
		.binning_factor = 1,
	},
#else
	{
		.x_output = 0xCC0, /* 3264 */
		.y_output = 0x990, /* 2448 */
		.line_length_pclk = 0x0ECC, /* 3788 */
		.frame_length_lines = 0xA0F, /* 2575 */
		.vt_pixel_clk = 216000000,
		.op_pixel_clk = 216000000,
		.binning_factor = 1,
	},
#endif
};

static struct msm_sensor_output_reg_addr_t ar0834_reg_addr = {
	.x_output = 0x34C,
	.y_output = 0x34E,
	.line_length_pclk = 0x342,
	.frame_length_lines = 0x340,
};

static struct msm_sensor_id_info_t ar0834_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x4B10,
};

static struct msm_sensor_exp_gain_info_t ar0834_exp_gain_info = {
	.coarse_int_time_addr = 0x500,
	.global_gain_addr = 0x500,
	.vert_offset = 0,
};

static int32_t ar0834_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
#ifdef STUB_GAINS
	return 0; /* FIXME: stub until user code is working */
#else
	uint32_t fl_lines;
	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain | 0x1000,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
#endif
}

static int32_t ar0834_write_exp_snapshot_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
#ifdef STUB_GAINS
	return 0; /* FIXME: stub until user code is working */
#else
	uint32_t fl_lines;
	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain | 0x1000,
		MSM_CAMERA_I2C_WORD_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr, line,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		0x301A, (0x065C|0x2), MSM_CAMERA_I2C_WORD_DATA);

	return 0;
#endif
}

static int32_t ar0834_regulator_on(struct regulator **reg,
		struct device *dev, char *regname, int uV)
{
	int32_t rc = 0;

	pr_debug("ar0834_regulator_on: %s %d\n", regname, uV);

	*reg = regulator_get(dev, regname);
	if (IS_ERR(*reg)) {
		pr_err("ar0834: failed to get %s (%ld)\n",
				regname, PTR_ERR(*reg));
		goto reg_on_done;
	}

	if (uV != 0) {
		rc = regulator_set_voltage(*reg, uV, uV);
		if (rc) {
			pr_err("ar0834: failed to set voltage for %s (%d)\n",
					regname, rc);
			goto reg_on_done;
		}
	}

	rc = regulator_enable(*reg);
	if (rc) {
		pr_err("ar0834: failed to enable %s (%d)\n",
				regname, rc);
		goto reg_on_done;
	}

reg_on_done:
	return rc;
}

static int32_t ar0834_regulator_off(struct regulator **reg, char *regname)
{
	int32_t rc = 0;

	if (regname)
		pr_debug("ar0834_regulator_off: %s\n", regname);

	if (*reg == NULL) {
		if (regname)
			pr_err("ar0834_regulator_off: %s is null, aborting\n",
								regname);
		rc = -EINVAL;
		goto reg_off_done;
	}

	rc = regulator_disable(*reg);
	if (rc) {
		if (regname)
			pr_err("ar0834: failed to disable %s (%d)\n",
								regname, rc);
		goto reg_off_done;
	}

	regulator_put(*reg);

reg_off_done:
	return rc;
}

static int32_t ar0834_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device *dev = &s_ctrl->sensor_i2c_client->client->dev;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	pr_debug("ar0834_power_up\n");

	/* Request gpios */
	rc = gpio_request(info->sensor_pwd, "ar0834");
	if (rc < 0) {
		pr_err("ar0834: gpio request sensor_pwd failed (%d)\n", rc);
		goto power_up_done;
	}

	rc = gpio_request(info->sensor_reset, "ar0834");
	if (rc < 0) {
		pr_err("ar0834: gpio request sensor_reset failed (%d)\n", rc);
		goto power_up_done;
	}

	/* Set reset low */
	gpio_direction_output(info->sensor_reset, 0);

	/* Wait for core supplies to power up */
	usleep(10000);

	/* Enable supplies */
	rc = ar0834_regulator_on(&cam_vio, dev, "cam_vio", 0);
	if (rc < 0)
		goto power_up_done;

	/* Wait for core supplies to power up */
	usleep(10000);

	rc = ar0834_regulator_on(&cam_vdig, dev, "cam_vdig", 1200000);
	if (rc < 0)
		goto power_up_done;

	/* Not every board needs this so ignore error */
	rc = ar0834_regulator_on(&cam_mipi_mux, dev, "cam_mipi_mux", 2800000);

	/*Enable MCLK*/
	cam_mot_8960_clk_info->clk_rate = s_ctrl->clk_rate;
	rc = msm_cam_clk_enable(dev, cam_mot_8960_clk_info,
			s_ctrl->cam_clk, ARRAY_SIZE(cam_mot_8960_clk_info), 1);
	if (rc < 0) {
		pr_err("ar0834: msm_cam_clk_enable failed (%d)\n", rc);
		goto power_up_done;
	}
	usleep(20000);

	/* Set pwd high */
	/* This also enables the pixel analog supply (2.85V LDO) */
	gpio_direction_output(info->sensor_pwd, 1);
	usleep(26000);

	/* Set reset high */
	gpio_direction_output(info->sensor_reset, 1);
	usleep(35000);

power_up_done:
	return rc;
}

static int32_t ar0834_power_down(
		struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct device *dev = &s_ctrl->sensor_i2c_client->client->dev;
	struct msm_camera_sensor_info *info = s_ctrl->sensordata;

	pr_debug("ar0834_power_down\n");

	/*Set Reset Low*/
	gpio_direction_output(info->sensor_reset, 0);
	usleep(10000);

	/*Disable MCLK*/
	rc = msm_cam_clk_enable(dev, cam_mot_8960_clk_info, s_ctrl->cam_clk,
			ARRAY_SIZE(cam_mot_8960_clk_info), 0);
	if (rc < 0)
		pr_err("ar0834: msm_cam_clk_enable failed (%d)\n", rc);
	usleep(10000);

	/* Disable supplies */
	rc = ar0834_regulator_off(&cam_vio, "cam_vio");
	if (rc < 0)
		pr_err("ar0834: regulator off for cam_vio failed (%d)\n", rc);

	rc = ar0834_regulator_off(&cam_vdig, "cam_vdig");
	if (rc < 0)
		pr_err("ar0834: regulator off for cam_vdig failed (%d)\n",
									rc);

	/* Not every board needs this so ignore error */
	ar0834_regulator_off(&cam_mipi_mux, NULL);

	/* Wait for core to shut off */
	usleep(10000);

	/*Set pwd low*/
	gpio_direction_output(info->sensor_pwd, 0);

	/*Clean up*/
	gpio_free(info->sensor_pwd);
	gpio_free(info->sensor_reset);

	return rc;
}

static int32_t ar0834_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	/*wait 10ms*/
	usleep(10000);
	/*Read sensor id*/
	rc = msm_camera_i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensor_id_info->sensor_id_reg_addr, &chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: read id failed\n", __func__);
		return rc;
	}

	if (chipid != s_ctrl->sensor_id_info->sensor_id) {
		pr_err("%s: chip id %x does not match expected %x\n", __func__,
				chipid, s_ctrl->sensor_id_info->sensor_id);
		return -ENODEV;
	}

	pr_debug("ar0834: match_id success\n");
	return 0;
}

static const struct i2c_device_id ar0834_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ar0834_s_ctrl},
	{ }
};

static struct i2c_driver ar0834_i2c_driver = {
	.id_table = ar0834_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ar0834_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&ar0834_i2c_driver);
}

static struct v4l2_subdev_core_ops ar0834_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ar0834_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ar0834_subdev_ops = {
	.core = &ar0834_subdev_core_ops,
	.video  = &ar0834_subdev_video_ops,
};

static struct msm_sensor_fn_t ar0834_func_tbl = {
	.sensor_start_stream            = msm_sensor_start_stream,
	.sensor_stop_stream             = msm_sensor_stop_stream,
	.sensor_group_hold_on           = msm_sensor_group_hold_on,
	.sensor_group_hold_off          = msm_sensor_group_hold_off,
	.sensor_set_fps                 = msm_sensor_set_fps,
	.sensor_write_exp_gain          = ar0834_write_exp_gain,
	.sensor_write_snapshot_exp_gain = ar0834_write_exp_snapshot_gain,
	.sensor_setting                 = msm_sensor_setting,
	.sensor_set_sensor_mode         = msm_sensor_set_sensor_mode,
	.sensor_mode_init               = msm_sensor_mode_init,
	.sensor_get_output_info         = msm_sensor_get_output_info,
	.sensor_config                  = msm_sensor_config,
	.sensor_power_up                = ar0834_power_up,
	.sensor_power_down              = ar0834_power_down,
	.sensor_match_id                = ar0834_match_id,
	.sensor_get_csi_params          = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t ar0834_regs = {
	.default_data_type        = MSM_CAMERA_I2C_WORD_DATA,
	.start_stream_conf        = ar0834_start_settings,
	.start_stream_conf_size   = ARRAY_SIZE(ar0834_start_settings),
	.stop_stream_conf         = ar0834_stop_settings,
	.stop_stream_conf_size    = ARRAY_SIZE(ar0834_stop_settings),
	.group_hold_on_conf       = ar0834_groupon_settings,
	.group_hold_on_conf_size  = ARRAY_SIZE(ar0834_groupon_settings),
	.group_hold_off_conf      = ar0834_groupoff_settings,
	.group_hold_off_conf_size = ARRAY_SIZE(ar0834_groupoff_settings),
	.init_settings            = &ar0834_init_conf[0],
	.init_size                = ARRAY_SIZE(ar0834_init_conf),
	.mode_settings            = &ar0834_confs[0],
	.output_settings          = &ar0834_dimensions[0],
	.num_conf                 = ARRAY_SIZE(ar0834_confs),
};

static struct msm_sensor_ctrl_t ar0834_s_ctrl = {
	.msm_sensor_reg               = &ar0834_regs,
	.sensor_i2c_client            = &ar0834_sensor_i2c_client,
	.sensor_i2c_addr              = 0x6C,
	.sensor_output_reg_addr       = &ar0834_reg_addr,
	.sensor_id_info               = &ar0834_id_info,
	.sensor_exp_gain_info         = &ar0834_exp_gain_info,
	.cam_mode                     = MSM_SENSOR_MODE_INVALID,
	.msm_sensor_mutex             = &ar0834_mut,
	.sensor_i2c_driver            = &ar0834_i2c_driver,
	.sensor_v4l2_subdev_info      = ar0834_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ar0834_subdev_info),
	.sensor_v4l2_subdev_ops       = &ar0834_subdev_ops,
	.func_tbl                     = &ar0834_func_tbl,
	.clk_rate                     = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Aptina 8MP sensor driver");
MODULE_LICENSE("GPL v2");
