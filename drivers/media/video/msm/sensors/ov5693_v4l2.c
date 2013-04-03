/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include "msm.h"
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/proc_fs.h>
#include <asm/mach-types.h>
#include "ov5693_v4l2.h"

#define SENSOR_NAME "ov5693"
#define PLATFORM_DRIVER_NAME "msm_camera_ov5693"
#define ov5693_obj ov5693_##obj
/* Macros assume PMIC GPIOs and MPPs start at 1 */
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
#define SENSOR_MAX_RETRIES      3 /* max counter for retry I2C access */
#define OTP_LOAD_DUMP   0x3D81
#define OTP_BANK_SELECT 0x3D84
#define OTP_BANK_START  0x3D00

static struct msm_sensor_ctrl_t ov5693_s_ctrl;
static struct msm_calib_af ov5693_af_position;

DEFINE_MUTEX(ov5693_mut);


static struct v4l2_subdev_info ov5693_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_SBGGR10_1X10,//V4L2_MBUS_FMT_SBGGR8_1X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array ov5693_init_conf[] = {
	{&ov5693_recommend_settings[0],
	ARRAY_SIZE(ov5693_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array ov5693_confs[] = {
	{&ov5693_snap_settings[0],
	ARRAY_SIZE(ov5693_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&ov5693_prev_settings[0],
	ARRAY_SIZE(ov5693_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t ov5693_dimensions[] = {
	{ /* For SNAPSHOT */
		.x_output = 0xA20,             /* 2592 */
		.y_output = 0x798,             /* 1944 */
		.line_length_pclk = 0xA80,     /* 2688 */
		.frame_length_lines = 0x7C0,   /* 1984 */
		.vt_pixel_clk = 160000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 0x1,
	},
	/* preview : 1536 * 864 */
	{
		.x_output = 0x600,             /* 1536 */
		.y_output = 0x360,             /* 864 */
		.line_length_pclk = 0xDE0,     /* 3552 */
		.frame_length_lines = 0x5DC,   /* 1500 */
		.vt_pixel_clk = 160000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 0x1,
	},
};

static struct msm_camera_csid_vc_cfg ov5693_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
};

static struct msm_camera_csi2_params ov5693_csi_params = {
	.csid_params = {
		.lane_cnt = 2,
		.lut_params = {
			.num_cid = ARRAY_SIZE(ov5693_cid_cfg),
			.vc_cfg = ov5693_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 2,
		.settle_cnt = 0x07,
	},
};

static struct msm_camera_csi2_params *ov5693_csi_params_array[] = {
	&ov5693_csi_params, /* Snapshot */
	&ov5693_csi_params, /* Preview */
};

static struct msm_sensor_output_reg_addr_t ov5693_reg_addr = {
	.x_output = 0x3808,
	.y_output = 0x380A,
	.line_length_pclk = 0x380C,
	.frame_length_lines = 0x380E,
};

static struct msm_sensor_id_info_t ov5693_id_info = {
	.sensor_id_reg_addr = 0x300A,
	.sensor_id = 0x5690,
};

static struct msm_sensor_exp_gain_info_t ov5693_exp_gain_info = {
	.coarse_int_time_addr = 0x3500,
	.global_gain_addr = 0x350A,
	.vert_offset = 4,
};

static int32_t ov5693_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	uint32_t fl_lines, offset;
	uint8_t int_time[3];

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);

	/* adjust the frame rate */
	fl_lines = (s_ctrl->curr_frame_length_lines *
		s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;
	fl_lines += (fl_lines & 0x1);

	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);

	/* exposure control */
	line = line<<4;
	int_time[0] = (u8) (line>>16);
	int_time[1] = (u8) ((line & 0xFF00) >> 8);
	int_time[2] = (u8) (line & 0x00FF);

	msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr,
		&int_time[0], 3);

	/* gain control */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);

	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);

	return 0;
}

static void ov5693_read_afcal(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_calib_af *af_pos)
{
	uint8_t data[2];
	uint16_t address;

	/* select otp bank 1 */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OTP_BANK_SELECT, 0xc1,
		MSM_CAMERA_I2C_BYTE_DATA);

	/* load otp */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, OTP_LOAD_DUMP, 0x01,
		MSM_CAMERA_I2C_BYTE_DATA);

	/* read inf */
	address = OTP_BANK_START;
	msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client, address, data, 2);

	af_pos->inf_dac = data[0] << 8 | data[1];

	/* read 1m */
	address += 2;
/*	msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client, address, data, 2); */

	/* read mac */
	address += 2;
	msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client, address, data, 2);

	af_pos->macro_dac = data[0] << 8 | data[1];

	/* read start position */
	address += 2;
	msm_camera_i2c_read_seq(s_ctrl->sensor_i2c_client, address, data, 2);

	af_pos->start_dac = data[0] << 8 | data[1];

	pr_info("* inf = 0x%x * mac = 0x%x * start = 0x%x",
		af_pos->inf_dac, af_pos->macro_dac, af_pos->start_dac);

	CDBG("OTP inf = 0x%x", af_pos->inf_dac);
	CDBG("OTP mac = 0x%x", af_pos->macro_dac);
	CDBG("OTP start = 0x%x", af_pos->start_dac);
}

static void ov5693_read_otp(struct msm_sensor_ctrl_t *s_ctrl)
{
	ov5693_read_afcal(s_ctrl, &ov5693_af_position);
}

int32_t ov5693_sensor_setting(struct msm_sensor_ctrl_t *s_ctrl,
	int update_type, int res)
{
	int32_t rc = 0;
	s_ctrl->func_tbl->sensor_stop_stream(s_ctrl);
	msleep(30);

	if (update_type == MSM_SENSOR_REG_INIT) {
		s_ctrl->curr_csi_params = NULL;
		msm_sensor_write_init_settings(s_ctrl);

		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x100, 0x1,
			MSM_CAMERA_I2C_BYTE_DATA);
		ov5693_read_otp(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x100, 0x0,
		  MSM_CAMERA_I2C_BYTE_DATA);

	} else if (update_type == MSM_SENSOR_UPDATE_PERIODIC) {
		msm_sensor_write_res_settings(s_ctrl, res);

		if (s_ctrl->curr_csi_params != s_ctrl->csi_params[res]) {
			s_ctrl->curr_csi_params = s_ctrl->csi_params[res];
			s_ctrl->curr_csi_params->csid_params.lane_assign =
				s_ctrl->sensordata->sensor_platform_info->
				csi_lane_params->csi_lane_assign;
			s_ctrl->curr_csi_params->csiphy_params.lane_mask =
				s_ctrl->sensordata->sensor_platform_info->
				csi_lane_params->csi_lane_mask;
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSID_CFG,
				&s_ctrl->curr_csi_params->csid_params);
			mb();
			v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
				NOTIFY_CSIPHY_CFG,
				&s_ctrl->curr_csi_params->csiphy_params);
			mb();
			msleep(20);
		}

		v4l2_subdev_notify(&s_ctrl->sensor_v4l2_subdev,
			NOTIFY_PCLK_CHANGE, &s_ctrl->msm_sensor_reg->
			output_settings[res].op_pixel_clk);
		s_ctrl->func_tbl->sensor_start_stream(s_ctrl);
		msleep(30);
	}
	return rc;
}

void ov5693_get_af_calib(struct msm_sensor_ctrl_t *s_ctrl,
	struct msm_calib_af *af)
{
	af->inf_dac = ov5693_af_position.inf_dac;
	af->macro_dac = ov5693_af_position.macro_dac;
	af->start_dac = ov5693_af_position.start_dac;

	CDBG("* inf = 0x%x", af->inf_dac);
	CDBG("* mac = 0x%x", af->macro_dac);
	CDBG("* start = 0x%x", af->start_dac);
}

static const struct i2c_device_id ov5693_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&ov5693_s_ctrl},
	{ }
};
int sensor_read_reg(struct i2c_client *client, u16 addr, u16 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];
	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = 0x10;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = 0x10;
	msg[1].flags = I2C_M_RD;

	msg[1].len = 1;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	memcpy(val, data+2, 1);
	*val=*val&0xff;

	return 0;
}

int sensor_write_reg(struct i2c_client *client, u16 addr, u16 val)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[4];
	int retry = 0;
	if (!client->adapter){
		pr_info("%s client->adapter is null",__func__);
		return -ENODEV;
	}

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);
	data[2] = (u8) (val & 0xff);

	msg.addr = (client->addr)>>1;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = data;
	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry++;
		pr_err("yuv_sensor : i2c transfer failed, retrying %x %x\n",
		       addr, val);
	} while (retry <= SENSOR_MAX_RETRIES);

	if(err == 0) {
		pr_err("%s(%d): i2c_transfer error, but return 0!?\n",
			__func__, __LINE__);
		err = 0xAAAA;
	}

	return err;
}

static struct pm_gpio pm_isp_gpio_high = {
	.direction		  = PM_GPIO_DIR_OUT,
	.output_buffer	  = PM_GPIO_OUT_BUF_CMOS,
	.output_value	  = 1,
	.pull			  = PM_GPIO_PULL_NO,
	.vin_sel		  = PM_GPIO_VIN_S4,
	.out_strength	  = PM_GPIO_STRENGTH_HIGH,
	.function		  = PM_GPIO_FUNC_PAIRED,
	.inv_int_pol	  = 0,
	.disable_pin	  = 0,
};

static struct pm_gpio pm_isp_gpio_low = {
	.direction		  = PM_GPIO_DIR_OUT,
	.output_buffer	  = PM_GPIO_OUT_BUF_CMOS,
	.output_value	  = 0,
	.pull			  = PM_GPIO_PULL_NO,
	.vin_sel		  = PM_GPIO_VIN_S4,
	.out_strength	  = PM_GPIO_STRENGTH_HIGH,
	.function		  = PM_GPIO_FUNC_PAIRED,
	.inv_int_pol	  = 0,
	.disable_pin	  = 0,
};

static int ov5693_regulator_init(bool on)
{
	static struct regulator *reg_8921_dvdd, *reg_8921_l8, *reg_8921_l16;
	int rc;

	pr_info("%s +++\n",__func__);

	if (on) {
		pr_info("Turn on the regulators\n");
		if(!reg_8921_dvdd){
			reg_8921_dvdd = regulator_get(NULL, "8921_l23");
			if (!IS_ERR(reg_8921_dvdd)) {
				rc = regulator_set_voltage(reg_8921_dvdd, 1800000, 1800000);
				if(rc){
					pr_err("DVDD: reg_8921_dvdd regulator set_voltage failed, rc=%d--\n", rc);
					goto reg_put_dvdd;
				}
			}
		}
		rc = regulator_enable(reg_8921_dvdd);
		if (rc){
			pr_err("DVDD regulator enable failed(%d)\n", rc);
			goto reg_put_dvdd;
		}
		pr_info("DVDD enable(%d)\n", regulator_is_enabled(reg_8921_dvdd));

		if (!reg_8921_l8) {
			reg_8921_l8 = regulator_get(&ov5693_s_ctrl.sensor_i2c_client->client->dev, "8921_l8");
			if (IS_ERR(reg_8921_l8)) {
				pr_err("PTR_ERR(reg_8921_l8)=%ld\n", PTR_ERR(reg_8921_l8));
				goto reg_put_l8;
			}
		}
		rc = regulator_set_voltage(reg_8921_l8, 2800000, 2800000);
		if(rc){
			pr_err("AVDD: reg_8921_l8 regulator set_voltage failed, rc=%d--\n", rc);
			goto reg_put_l8;
		}
		rc = regulator_enable(reg_8921_l8);
		if(rc){
			pr_err("AVDD: reg_8921_l8 regulator enable failed, rc=%d--\n", rc);
			goto reg_put_l8;
		}
		pr_info("AVDD: reg_8921_l8(%d) enable(%d)\n", regulator_get_voltage(reg_8921_l8), regulator_is_enabled(reg_8921_l8));

		if (!reg_8921_l16) {
			reg_8921_l16 = regulator_get(&ov5693_s_ctrl.sensor_i2c_client->client->dev, "8921_l16");
			if (IS_ERR(reg_8921_l16)) {
				pr_err("PTR_ERR(reg_8921_l16)=%ld\n", PTR_ERR(reg_8921_l16));
				goto reg_put_l16;
			}
		}
		rc = regulator_set_voltage(reg_8921_l16, 2800000, 2800000);
		if(rc){
			pr_err("VCM_VDD: reg_8921_l16 regulator set_voltage failed, rc=%d--\n", rc);
			goto reg_put_l16;
		}
		rc = regulator_enable(reg_8921_l16);
		if(rc){
			pr_err("VCM_VDD: reg_8921_l16 regulator enable failed, rc=%d--\n", rc);
			goto reg_put_l16;
		}
		pr_info("VCM_VDD: reg_8921_l16(%d) enable(%d)\n", regulator_get_voltage(reg_8921_l16), regulator_is_enabled(reg_8921_l16));

		return 0;
	}
	else {
		pr_info("Turn off the regulators\n");

		if(reg_8921_l16){
			pr_info("Turn off the regulators VCM_VDD:reg_8921_l16\n");
			regulator_put(reg_8921_l16);
			regulator_disable(reg_8921_l16);
			reg_8921_l16 = NULL;
		}
		if(reg_8921_l8){
			pr_info("Turn off the regulators AVDD:reg_8921_l8\n");
			regulator_put(reg_8921_l8);
			regulator_disable(reg_8921_l8);
			reg_8921_l8 = NULL;
		}
		if(reg_8921_dvdd){
			pr_info("Turn off the regulators DVDD\n");
			regulator_put(reg_8921_dvdd);
			regulator_disable(reg_8921_dvdd);
			reg_8921_dvdd = NULL;
		}
		return 0;
	}

reg_put_l16:
	regulator_put(reg_8921_l16);
	regulator_disable(reg_8921_l8);
	reg_8921_l16 = NULL;

reg_put_l8:
	regulator_put(reg_8921_l8);
	regulator_disable(reg_8921_dvdd);
	reg_8921_l8 = NULL;

reg_put_dvdd:
	regulator_put(reg_8921_dvdd);
	reg_8921_dvdd = NULL;

	return rc;
}

static int ov5693_gpio_request(void)
{
	int32_t rc = 0;

	pr_info("%s +++\n",__func__);
	// OV5693 VCM_PD:
	rc = gpio_request(PM8921_GPIO_PM_TO_SYS(25), "ov5693");
	if (rc) {
		pr_err("%s: gpio rear_mclk %d, rc(%d)fail\n",__func__, 25, rc);
		goto init_probe_fail;
		}
	// OV5693 XSHUTDN:
	rc = gpio_request(PM8921_GPIO_PM_TO_SYS(31), "ov5693");
	if (rc) {
		pr_err("%s: PMIC gpio PWDN %d, rc(%d)fail\n",__func__, 31, rc);
		goto init_probe_fail;
	}

	pr_info("%s ---\n",__func__);
	return rc;

init_probe_fail:
	pr_info("%s fail---\n",__func__);
	return rc;
}

int32_t ov5693_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	pr_info("%s +++\n",__func__);

	if(!s_ctrl->sensordata || !ov5693_s_ctrl.sensordata){
		pr_err("ov5693_s_ctrl.sensordata is NULL, return\n");
		pr_info("%s ---\n",__func__);
		return -1;
	}

	// enable MCLK
	msm_sensor_power_up(&ov5693_s_ctrl);

	rc = ov5693_gpio_request();
	if(rc < 0)	{
		pr_err("5M Camera GPIO request fail!!\n");
		pr_info("%s ---\n",__func__);
		return -1;
	}

	//PMIC regulator - DOVDD 1.8V and AVDD 2.8V ON
	rc = ov5693_regulator_init(true);
	if(rc < 0){
		pr_err("5M Camera regulator init fail!!\n");
		pr_info("%s ---\n",__func__);
		return -1;
	}
	//OV5693 VCM_PD
	rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(25), &pm_isp_gpio_high);
	if (rc != 0)
		pr_err("gpio 25 VCM_PD config high fail\n");
	else
		pr_info("gpio 25 VCM_PD(%d)\n",gpio_get_value(PM8921_GPIO_PM_TO_SYS(25)));
	//OV5693 XSHUTDN
	rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(31), &pm_isp_gpio_high);
	if (rc != 0)
		pr_err("gpio 31 XSHUTDN config high fail\n");
	else
		pr_info("gpio 31 XSHUTDN(%d)\n",gpio_get_value(PM8921_GPIO_PM_TO_SYS(31)));

	msleep(20);

	pr_info("%s ---\n",__func__);
	return 0;
}

int32_t ov5693_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	pr_info("%s +++\n",__func__);

	if(!s_ctrl->sensordata || !ov5693_s_ctrl.sensordata){
		pr_info("ov5693_s_ctrl.sensordata is NULL, return\n");
		pr_info("%s ---\n",__func__);
		return -1;
	}

	//XSHUTDOWN low
	rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(31), &pm_isp_gpio_low);
	if (rc != 0)
		pr_err("%s: XSHUTDOWN config low fail\n", __func__);
		//VCM_PD low
		rc = pm8xxx_gpio_config(PM8921_GPIO_PM_TO_SYS(25), &pm_isp_gpio_low);
		if (rc != 0)
			pr_err("%s: VCM_PD config low fail\n", __func__);

	//PMIC regulator - DOVDD 1.8V and AVDD 2.8V OFF
	ov5693_regulator_init(false);

	//disable MCLK
	msm_sensor_power_down(&ov5693_s_ctrl);

	msleep(20);
	gpio_free(PM8921_GPIO_PM_TO_SYS(31));
	gpio_free(PM8921_GPIO_PM_TO_SYS(25));
	pr_info("%s ---\n",__func__);
	return 0;
}

int32_t ov5693_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t rdata = 0;

	msm_camera_i2c_read(ov5693_s_ctrl.sensor_i2c_client, 0x300A,
			&rdata, MSM_CAMERA_I2C_WORD_DATA);
	if(rdata == ov5693_id_info.sensor_id)
		rc = 0;
	else
		rc = -EIO;

	pr_info("Sensor id: 0x%x\n", rdata);
	return rc;
}

int32_t ov5693_sensor_read_vendor_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	unsigned short rdata = 0, vendor_id;

	sensor_write_reg(ov5693_s_ctrl.sensor_i2c_client->client, 0x0100, 0x01);
	usleep(3000);
	sensor_write_reg(ov5693_s_ctrl.sensor_i2c_client->client,
		OTP_BANK_SELECT, 0xc1);
	sensor_write_reg(ov5693_s_ctrl.sensor_i2c_client->client,
		OTP_LOAD_DUMP, 0x01);
	usleep(3000);
	sensor_read_reg(ov5693_s_ctrl.sensor_i2c_client->client, 0x3D08,
		&rdata);
	sensor_read_reg(ov5693_s_ctrl.sensor_i2c_client->client, 0x3D09,
		&vendor_id);
	if (rdata == 2)
		snprintf(s_ctrl->sensordata->vendor_name,
			sizeof(s_ctrl->sensor_v4l2_subdev.name),
			"%s", "B");
	else if (rdata == 0) {
		sensor_write_reg(ov5693_s_ctrl.
			sensor_i2c_client->client, OTP_BANK_SELECT, 0xc2);
		sensor_write_reg(ov5693_s_ctrl.
			sensor_i2c_client->client, OTP_LOAD_DUMP, 0x01);
		usleep(3000);
		sensor_read_reg(ov5693_s_ctrl.
			sensor_i2c_client->client, 0x3D08, &rdata);
		sensor_read_reg(ov5693_s_ctrl.
			sensor_i2c_client->client, 0x3D09, &vendor_id);
		if (rdata == 2)
			snprintf(s_ctrl->sensordata->vendor_name,
				sizeof(s_ctrl->sensor_v4l2_subdev.name),
				"%s", "B");
		else if (rdata == 0) {
			sensor_write_reg(ov5693_s_ctrl.
				sensor_i2c_client->client,
				OTP_BANK_SELECT, 0xc3);
			sensor_write_reg(ov5693_s_ctrl.
				sensor_i2c_client->client,
				OTP_LOAD_DUMP, 0x01);
			usleep(3000);
			sensor_read_reg(ov5693_s_ctrl.
				sensor_i2c_client->client, 0x3D08, &rdata);
			sensor_read_reg(ov5693_s_ctrl.
				sensor_i2c_client->client, 0x3D09, &vendor_id);
			if (rdata == 2)
				snprintf(s_ctrl->sensordata->vendor_name,
					sizeof(s_ctrl->sensor_v4l2_subdev.name),
					"%s", "B");
		}
	}
	sensor_write_reg(ov5693_s_ctrl.sensor_i2c_client->client, 0x0100, 0x0);
	usleep(3000);
	pr_info("Module name: %s Vendor id: %d\n",
		s_ctrl->sensordata->vendor_name, vendor_id);
	return rdata;
}

int32_t ov5693_sensor_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int32_t rc = 0, vendor_id;
	struct msm_sensor_ctrl_t *s_ctrl;
	unsigned short rdata = 0;

	pr_info("%s +++\n", __func__);

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		if (s_ctrl->sensor_i2c_addr != 0)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensor_i2c_addr;
	} else {
		rc = -EIO;
		pr_err("%s ---\n", __func__);
		return rc;
	}

	if (client->dev.platform_data == NULL) {
		pr_err("%s: NULL sensor data\n", __func__);
		return -EIO;
	}

	s_ctrl->sensordata = client->dev.platform_data;
	ov5693_s_ctrl.sensordata = client->dev.platform_data;

	rc = s_ctrl->func_tbl->sensor_power_up(&ov5693_s_ctrl);
	if (rc < 0)
		goto probe_fail;

	sensor_read_reg(ov5693_s_ctrl.sensor_i2c_client->client, 0x300A, &rdata);
	pr_info("Sensor id: 0x%x", rdata);
	sensor_read_reg(ov5693_s_ctrl.sensor_i2c_client->client, 0x300B, &rdata);
	pr_info("Sensor id: 0x%x", rdata);

	vendor_id = ov5693_sensor_read_vendor_id(s_ctrl);
	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		s_ctrl->sensor_v4l2_subdev_ops);

	msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);

	goto power_down;

probe_fail:
	pr_info("Sensor power on fail\n");

power_down:
	s_ctrl->func_tbl->sensor_power_down(&ov5693_s_ctrl);

	pr_info("%s --- \n",__func__);
	return rc;
}

static struct i2c_driver ov5693_i2c_driver = {
	.id_table = ov5693_i2c_id,
	.probe  = ov5693_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};



static struct msm_camera_i2c_client ov5693_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

void create_ov5693_proc_file(void);

static int __init msm_sensor_init_module(void)
{
	create_ov5693_proc_file();
	return  i2c_add_driver(&ov5693_i2c_driver);
}

static struct v4l2_subdev_core_ops ov5693_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops ov5693_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops ov5693_subdev_ops = {
	.core = &ov5693_subdev_core_ops,
	.video  = &ov5693_subdev_video_ops,
};


static struct msm_sensor_fn_t ov5693_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = ov5693_write_exp_gain,
	.sensor_write_snapshot_exp_gain = ov5693_write_exp_gain,
	.sensor_csi_setting = ov5693_sensor_setting,
	.sensor_setting = ov5693_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_match_id = ov5693_sensor_match_id,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = ov5693_sensor_power_up,
	.sensor_power_down = ov5693_sensor_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_get_af_calib = ov5693_get_af_calib,
};

static struct msm_sensor_reg_t ov5693_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = ov5693_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(ov5693_start_settings),
	.stop_stream_conf = ov5693_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(ov5693_stop_settings),
	.group_hold_on_conf = ov5693_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(ov5693_groupon_settings),
	.group_hold_off_conf = ov5693_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(ov5693_groupoff_settings),
	.init_settings = &ov5693_init_conf[0],
	.init_size = ARRAY_SIZE(ov5693_init_conf),
	.mode_settings = &ov5693_confs[0],
	.output_settings = &ov5693_dimensions[0],
	.num_conf = ARRAY_SIZE(ov5693_confs),
};

static struct msm_sensor_ctrl_t ov5693_s_ctrl = {
	.msm_sensor_reg = &ov5693_regs,
	.sensor_i2c_client = &ov5693_sensor_i2c_client,
	.sensor_i2c_addr =  0x20,
	.sensor_output_reg_addr = &ov5693_reg_addr,
	.sensor_id_info = &ov5693_id_info,
	.sensor_exp_gain_info = &ov5693_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &ov5693_csi_params_array[0],
	.msm_sensor_mutex = &ov5693_mut,
	.sensor_i2c_driver = &ov5693_i2c_driver,
	.sensor_v4l2_subdev_info = ov5693_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov5693_subdev_info),
	.sensor_v4l2_subdev_ops = &ov5693_subdev_ops,
	.func_tbl = &ov5693_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

#ifdef	CONFIG_PROC_FS
#define	OV5693_PROC_CAMERA_STATUS	"driver/camera_status"
static ssize_t ov5693_proc_read_camera_status(char *page, char **start, off_t off, int count,
			int *eof, void *data)
{
	int len = 0, rc = 0;

	ov5693_s_ctrl.func_tbl->sensor_power_up(&ov5693_s_ctrl);
	rc = ov5693_s_ctrl.func_tbl->sensor_match_id(&ov5693_s_ctrl);
	ov5693_s_ctrl.func_tbl->sensor_power_down(&ov5693_s_ctrl);

	if(*eof == 0){
		if(!rc)
			len += sprintf(page+len, "1\n");
		else
			len += sprintf(page+len, "0\n");
		*eof = 1;
		pr_info("%s: string:%s", __func__, (char *)page);
	}
	return len;
}
void create_ov5693_proc_file(void)
{
	if(create_proc_read_entry(OV5693_PROC_CAMERA_STATUS, 0666, NULL,
			ov5693_proc_read_camera_status, NULL) == NULL){
		pr_err("[Camera]proc file create failed!\n");
	}
}

void remove_ov5693_proc_file(void)
{
    pr_info("ov5693_proc_file\n");
    remove_proc_entry(OV5693_PROC_CAMERA_STATUS, &proc_root);
}
#endif

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Omnivision Bayer sensor driver");
MODULE_LICENSE("GPL v2");

//Create debugfs for ov5693
#define DBG_TXT_BUF_SIZE 256
static char debugTxtBuf[DBG_TXT_BUF_SIZE];
static unsigned int register_value = 0xffffffff;
static int i2c_set_open(struct inode *inode, struct file *file)
{
    file->private_data = inode->i_private;
    return 0;
}

static ssize_t i2c_camera(
	struct file *file,
	const char __user *buf,
	size_t count,
	loff_t *ppos)
{
    int len;
    int arg[2];
    int err;

    if (*ppos)
        return 0;    /* the end */

//+ parsing......
    len=( count > DBG_TXT_BUF_SIZE-1 ) ? ( DBG_TXT_BUF_SIZE-1 ) : (count);
    if ( copy_from_user( debugTxtBuf, buf, len ) )
        return -EFAULT;

    debugTxtBuf[len] = 0; //add string end

    sscanf(debugTxtBuf, "%x", &arg[0]);
    pr_info("1 is open_camera 0 is close_camera\n");
    pr_info("command is arg1=%x \n", arg[0]);

    *ppos = len;

    switch(arg[0])
    {
        case 0:
        {
            pr_info("ov5693 power_off\n");
            err = ov5693_sensor_power_down(&ov5693_s_ctrl);
            if(err)
                return -ENOMEM;
            break;
        }
        case 1:
        {
            pr_info("ov5693 power_on\n");
            err = ov5693_sensor_power_up(&ov5693_s_ctrl);
            if(err)
                return -ENOMEM;
            break;
        }
        case 2:
        {
            pr_info("ov5693 change vendor to A\n");
            snprintf(ov5693_s_ctrl.sensordata->vendor_name,
				sizeof(ov5693_s_ctrl.sensor_v4l2_subdev.name), "%s", "A");
            break;
        }
        case 3:
        {
            pr_info("ov5693 change vendor to B\n");
            snprintf(ov5693_s_ctrl.sensordata->vendor_name,
				sizeof(ov5693_s_ctrl.sensor_v4l2_subdev.name), "%s", "B");
            break;
        }
        default:
            break;
    }

    return len;    /* the end */
}

static ssize_t i2c_camera_read(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int len;
	u32 arg[2];

	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];
	unsigned int val;

	if (!ov5693_s_ctrl.sensor_i2c_client->client->adapter)
		return -ENODEV;

	if (*ppos)
		return 0;    /* the end */

//+ parsing......
	len=( count > DBG_TXT_BUF_SIZE-1 ) ? ( DBG_TXT_BUF_SIZE-1 ) : (count);
	if ( copy_from_user( debugTxtBuf, buf, len ) )
		return -EFAULT;

	debugTxtBuf[len] = 0; //add string end

	sscanf(debugTxtBuf, "%x %x", &arg[0], &arg[1]);
	pr_info("command is slave_address=%x addr=%x\n", arg[0], arg[1]);

	msg[0].addr = arg[0];
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	if( arg[0] == 0x10 ){
		/* high byte goes out first */
		data[0] = (u8) (arg[1] >> 8);;
		data[1] = (u8) (arg[1] & 0xff);

		msg[1].addr = arg[0];
		msg[1].flags = I2C_M_RD;
		msg[1].len = 1;
		msg[1].buf = data + 2;
	}
	else{
		data[0] = (u8) (arg[1] & 0xff);;

		msg[1].addr = arg[0];
		msg[1].flags = I2C_M_RD;
		msg[1].len = 1;
		msg[1].buf = data + 1;
	}

	err = i2c_transfer(ov5693_s_ctrl.sensor_i2c_client->client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	if( arg[0] == 0x10 )
		val = data[2] & 0xFF;
	else
		val = data[1] & 0xFF;

	register_value = val;
	pr_info("register value: 0x%x\n", val);

	*ppos = len;
	return len;    /* the end */
}

static ssize_t i2c_camera_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int len;
	u32 arg[3];
	int err;
	struct i2c_msg msg;
	unsigned char data[4];
	int retry = 0;

	if (*ppos)
		return 0;    /* the end */

//+ parsing......
	len=( count > DBG_TXT_BUF_SIZE-1 ) ? ( DBG_TXT_BUF_SIZE-1 ) : (count);
	if ( copy_from_user( debugTxtBuf, buf, len ) )
		return -EFAULT;

	debugTxtBuf[len] = 0; //add string end

	sscanf(debugTxtBuf, "%x %x %x", &arg[0], &arg[1], &arg[2]);
	pr_info("command is slave_address=%x addr=%x value=%x\n", arg[0], arg[1], arg[2]);

	if (!ov5693_s_ctrl.sensor_i2c_client->client->adapter){
		pr_info("%s client->adapter is null",__func__);
		return -ENODEV;
	}

	if( arg[0] == 0x10 ){
		data[0] = (u8) (arg[1] >> 8);
		data[1] = (u8) (arg[1] & 0xff);
		data[2] = (u8) (arg[2] & 0xff);
		msg.addr = arg[0];
		msg.flags = 0;
		msg.len = 3;
		msg.buf = data;
	}
	else{
		data[0] = (u8) (arg[1] & 0xff);
		data[1] = (u8) (arg[2] & 0xff);
		msg.addr = arg[0];
		msg.flags = 0;
		msg.len = 2;
		msg.buf = data;
	}

	do {
		err = i2c_transfer(ov5693_s_ctrl.sensor_i2c_client->client->adapter, &msg, 1);
		if (err == 1){
			*ppos = len;
			return len;    /* the end */
		}
		retry++;
		pr_err("yuv_sensor : i2c transfer failed, retrying %x %x\n",
		arg[1], arg[2]);
	} while (retry <= SENSOR_MAX_RETRIES);

	*ppos = len;
	return len;    /* the end */
}

static int i2c_read_value(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int len = 0;
	char *bp = debugTxtBuf;

	if (*ppos)
		return 0;    /* the end */

	len = snprintf(bp, DBG_TXT_BUF_SIZE, "Register value is 0x%X\n", register_value);

	if (copy_to_user(buf, debugTxtBuf, len))
		return -EFAULT;

	*ppos += len;
	return len;
}

static const struct file_operations i2c_open_camera = {
	.open = i2c_set_open,
	.write = i2c_camera,
};

static const struct file_operations i2c_read_register = {
	.open = i2c_set_open,
	.write = i2c_camera_read,
};
static const struct file_operations i2c_write_register = {
	.open = i2c_set_open,
	.write = i2c_camera_write,
};
static const struct file_operations read_register_value = {
	.open = i2c_set_open,
	.read = i2c_read_value,
};

static int __init qualcomm_i2c_debuginit(void)
{
	struct dentry *dent = debugfs_create_dir("ov5693_v4l2", NULL);

	(void) debugfs_create_file("i2c_open_camera", S_IRUGO | S_IWUSR,
			dent, NULL, &i2c_open_camera);
	(void) debugfs_create_file("i2c_read", S_IRUGO | S_IWUSR,
			dent, NULL, &i2c_read_register);
	(void) debugfs_create_file("i2c_write", S_IRUGO | S_IWUSR,
			dent, NULL, &i2c_write_register);
	(void) debugfs_create_file("read_register_value", S_IRUGO | S_IWUSR,
			dent, NULL, &read_register_value);

	return 0;
}

late_initcall(qualcomm_i2c_debuginit);

