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
#include "msm_ispif.h"
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/proc_fs.h>
#include <asm/mach-types.h>
#include "mi1040.h"

#define SENSOR_NAME "mi1040"
#define PLATFORM_DRIVER_NAME "msm_camera_mi1040"
#define mi1040_obj mi1040_##obj
/* Macros assume PMIC GPIOs and MPPs start at 1 */
#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
#define SENSOR_MAX_RETRIES      3 /* max counter for retry I2C access */
#define CAM_RST PM8921_GPIO_PM_TO_SYS(43)
#define CAM_VENDOR_1 PM8921_GPIO_PM_TO_SYS(11)
#define CAM_VENDOR_0 PM8921_GPIO_PM_TO_SYS(10)
#define CAM_LENS PM8921_GPIO_PM_TO_SYS(24)

static struct msm_sensor_ctrl_t mi1040_s_ctrl;
static int effect_value = 0;
static int wb_value = 0;
static int ev_value = 0;
static int vendor_id = 0;

DEFINE_MUTEX(mi1040_mut);

static struct msm_camera_i2c_conf_array mi1040_init_conf[] = {
	{mi1040_recommend_settings,
	ARRAY_SIZE(mi1040_recommend_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_config_change_settings,
	ARRAY_SIZE(mi1040_config_change_settings),
	0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_camera_i2c_conf_array mi1040_confs[] = {
	{mi1040_960p_settings,
	ARRAY_SIZE(mi1040_960p_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_720p_settings,
	ARRAY_SIZE(mi1040_720p_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_camera_i2c_conf_array mi1040_B_init_conf[] = {
	{mi1040_B_recommend_settings,
	ARRAY_SIZE(mi1040_B_recommend_settings), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_config_change_settings,
	ARRAY_SIZE(mi1040_config_change_settings),
	0, MSM_CAMERA_I2C_WORD_DATA},
};

static struct msm_camera_i2c_conf_array mi1040_saturation_confs[][2] = {
	{{mi1040_saturation[0],
		ARRAY_SIZE(mi1040_saturation[0]), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
	{{mi1040_saturation[1],
		ARRAY_SIZE(mi1040_saturation[1]), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
	{{mi1040_saturation[2],
		ARRAY_SIZE(mi1040_saturation[2]), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
	{{mi1040_saturation[3],
		ARRAY_SIZE(mi1040_saturation[3]), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
	{{mi1040_saturation[4],
		ARRAY_SIZE(mi1040_saturation[4]), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
	{{mi1040_saturation[5],
		ARRAY_SIZE(mi1040_saturation[5]), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
	{{mi1040_saturation[6],
		ARRAY_SIZE(mi1040_saturation[6]), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
	{{mi1040_saturation[7],
		ARRAY_SIZE(mi1040_saturation[7]), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
	{{mi1040_saturation[8],
		ARRAY_SIZE(mi1040_saturation[8]), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
	{{mi1040_saturation[9],
		ARRAY_SIZE(mi1040_saturation[9]), 0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
	{{mi1040_saturation[10],
		ARRAY_SIZE(mi1040_saturation[10]),
		0, MSM_CAMERA_I2C_WORD_DATA},
	{mi1040_refresh,
		ARRAY_SIZE(mi1040_refresh), 0, MSM_CAMERA_I2C_WORD_DATA},},
};

static struct msm_camera_i2c_enum_conf_array mi1040_saturation_enum_confs = {
	.conf = &mi1040_saturation_confs[0][0],
	.conf_enum = mi1040_saturation_enum_map,
	.num_enum = ARRAY_SIZE(mi1040_saturation_enum_map),
	.num_index = ARRAY_SIZE(mi1040_saturation_confs),
	.num_conf = ARRAY_SIZE(mi1040_saturation_confs[0]),
	.data_type = MSM_CAMERA_I2C_WORD_DATA,
};

struct msm_sensor_v4l2_ctrl_info_t mi1040_v4l2_ctrl_info[] = {
	{
		.ctrl_id = V4L2_CID_SATURATION,
		.min = MSM_V4L2_SATURATION_L0,
		.max = MSM_V4L2_SATURATION_L10,
		.step = 1,
		.enum_cfg_settings = &mi1040_saturation_enum_confs,
		.s_v4l2_ctrl = msm_sensor_s_ctrl_by_enum,
	},
};

static struct v4l2_subdev_info mi1040_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_YUYV8_2X8,  //V4L2_MBUS_FMT_SBGGR10_1X10 // LiJen: format ???
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_sensor_output_info_t mi1040_dimensions[] = {
	{
		.x_output = 0x500,
		.y_output = 0x3C0,
		.line_length_pclk = 0x500,
		.frame_length_lines = 0x3C0,
		.vt_pixel_clk = 48000000,
		.op_pixel_clk = 128000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0x500,
		.y_output = 0x2D0,
		.line_length_pclk = 0x500,
		.frame_length_lines = 0x2D0,
		.vt_pixel_clk = 48000000,
		.op_pixel_clk = 128000000,
		.binning_factor = 1,
	},
};

static struct msm_camera_csid_vc_cfg mi1040_cid_cfg[] = {
	{0, CSI_YUV422_8, CSI_DECODE_8BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params mi1040_csi_params = {
	.csid_params = {
		.lane_cnt = 1,
		.lut_params = {
			.num_cid = 2,
			.vc_cfg = mi1040_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 1,
		.settle_cnt = 0x14,
	},
};

static struct msm_camera_csi2_params *mi1040_csi_params_array[] = {
	&mi1040_csi_params,
	&mi1040_csi_params,
};

static struct msm_sensor_output_reg_addr_t mi1040_reg_addr = {
	.x_output = 0xC868,
	.y_output = 0xC86A,
	.line_length_pclk = 0xC868,		//0xC814
	.frame_length_lines = 0xC86A,	//0xC812
};

static struct msm_sensor_id_info_t mi1040_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x2481,
};

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

int sensor_read_word(struct i2c_client *client, u16 addr, u16 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];

	if (!client->adapter)
		return -ENODEV;

	msg[0].addr = 0x48;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (addr >> 8);;
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = 0x48;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	swap(*(data+2),*(data+3)); //swap high and low byte to match table format
	memcpy(val, data+2, 2);

	return 0;
}

static int sensor_write_reg(struct i2c_client *client, u16 addr, u16 val, u16 data_type)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[8];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	switch(data_type)
	{
		case 1:
		{
			data[2] = (u8)(val & 0xff);
			msg.len = 3;
			break;
		}
		case 2:
		{
			data[2] = (u8)((val & 0xff00) >> 8);
			data[3] = (u8)(val & 0x00ff);
			msg.len = 4;
			break;
		}
		default:
			break;
	}

	msg.addr = 0x48;
	msg.flags = 0;
	msg.buf = data;

//	printk("Addr:0x%x, Value:0x%x Byte:%d\n", addr, val, data_type);

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry++;
		pr_err("yuv_sensor : i2c transfer failed, retrying %x %x\n", addr, val);
		pr_err("yuv_sensor : i2c transfer failed, count %x \n", msg.addr);
	} while (retry <= SENSOR_MAX_RETRIES);
	return err;
}

static int sensor_write_table(struct msm_sensor_ctrl_t *s_ctrl,
			      struct msm_camera_i2c_reg_conf *data, int data_num)
{
	int i, data_type;
	int32_t rc = -EFAULT;

	for (i = 0; i < data_num; i++) {
		if((data+i)->dt == 1)
			data_type = (data+i)->dt;
		else
			data_type = 2;
		rc = sensor_write_reg(mi1040_s_ctrl.sensor_i2c_client->client, (data+i)->reg_addr, (data+i)->reg_data, data_type);
	}
	return rc;
}

static int mi1040_regulator_init(bool on)
{
	static struct regulator *reg_8921_dvdd, *reg_8921_l8;
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
			if (IS_ERR(reg_8921_dvdd)) {
				pr_info("PTR_ERR(DVDD)=%ld\n", PTR_ERR(reg_8921_dvdd));
				return -ENODEV;
			}
		}

		rc = regulator_enable(reg_8921_dvdd);
		if (rc){
			pr_err("DVDD regulator enable failed(%d)\n", rc);
			goto reg_put_dvdd;
		}
		pr_info("DVDD enable(%d)\n", regulator_is_enabled(reg_8921_dvdd));

		if (!reg_8921_l8) {
			reg_8921_l8 = regulator_get(&mi1040_s_ctrl.sensor_i2c_client->client->dev, "8921_l8");
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

		return 0;
	}
	else {
		pr_info("Turn off the regulators\n");

		if(reg_8921_l8){
			pr_info("Turn off the regulators AVDD:reg_8921_l8\n");
			regulator_put(reg_8921_l8);
			regulator_disable(reg_8921_l8);
			reg_8921_l8 = NULL;
		}
		if(reg_8921_dvdd){
			pr_info("Turn off the regulators DOVDD:reg_8921_dvdd\n");
			regulator_put(reg_8921_dvdd);
			regulator_disable(reg_8921_dvdd);
			reg_8921_dvdd = NULL;
		}
		return 0;
	}

reg_put_l8:
	regulator_put(reg_8921_l8);
	regulator_disable(reg_8921_dvdd);
	reg_8921_l8 = NULL;

reg_put_dvdd:
	regulator_put(reg_8921_dvdd);
	reg_8921_dvdd = NULL;

	return rc;
}

int32_t mi1040_sensor_read_vendor_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0, vendor = 0;
	rc = gpio_request(CAM_VENDOR_1, "mi1040_v1");
	pr_info("PMIC gpio 11 CAM_VENDOR_1(%d)\n",
		gpio_get_value(CAM_VENDOR_1));
	rc = gpio_request(CAM_VENDOR_0, "mi1040_v0");
	pr_info("PMIC gpio 10 CAM_VENDOR_0(%d)\n",
		gpio_get_value(CAM_VENDOR_0));
	rc = gpio_request(CAM_LENS, "mi1040_lens");
	vendor = gpio_get_value(CAM_LENS);
	pr_info("PMIC gpio 24 CAM_LENS(%d)\n", gpio_get_value(CAM_LENS));

	gpio_free(CAM_VENDOR_1);
	gpio_free(CAM_VENDOR_0);
	gpio_free(CAM_LENS);

	switch(vendor){
		case 0:
			vendor_id =  A;
			pr_info("Vendor is A\n");
			break;
		case 1:
			vendor_id = B;
			snprintf(s_ctrl->sensordata->vendor_name,
				sizeof(s_ctrl->sensor_v4l2_subdev.name),
				"%s", "B");
			pr_info("Vendor is B\n");
			break;
		default:
			vendor_id =  A;
			pr_info("Default Vendor is A\n");
			break;
	}
	return vendor_id;
}

int32_t mi1040_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t rdata = 0;
	pr_info("%s +++ \n", __func__);

	msm_camera_i2c_read(mi1040_s_ctrl.sensor_i2c_client, 0,
		&rdata, MSM_CAMERA_I2C_WORD_DATA);
	if(rdata == mi1040_id_info.sensor_id)
		rc = 0;
	else
		rc = -EFAULT;

	pr_info("Sensor id: 0x%x\n", rdata);

	pr_info("%s --- \n", __func__);
	return rc;
}

int32_t mi1040_sensor_set_effect(struct msm_sensor_ctrl_t *s_ctrl, int effect)
{
	int32_t rc = 0;
	pr_info("%s +++ effect: %d effect_value: %d\n", __func__, effect, effect_value);

	if(effect_value != effect){
		effect_value = effect;
		switch (effect)	{
		case CAMERA_EFFECT_MONO:
			CDBG("--CAMERA-- %s ...CAMERA_EFFECT_MONO\n", __func__);
			rc = sensor_write_table(s_ctrl, ColorEffect_Mono, ARRAY_SIZE(ColorEffect_Mono));
			break;

		case CAMERA_EFFECT_SOLARIZE:
			CDBG("--CAMERA-- %s ...CAMERA_EFFECT_NEGATIVE\n", __func__);
			rc = sensor_write_table(s_ctrl, ColorEffect_Solarize, ARRAY_SIZE(ColorEffect_Solarize));
			break;

		case CAMERA_EFFECT_SEPIA:
			CDBG("--CAMERA-- %s ...CAMERA_EFFECT_SEPIA\n", __func__);
			rc = sensor_write_table(s_ctrl, ColorEffect_Sepia, ARRAY_SIZE(ColorEffect_Sepia));
			break;

		case CAMERA_EFFECT_NEGATIVE:
			CDBG("--CAMERA-- %s ...CAMERA_EFFECT_NEGATIVE\n", __func__);
			rc = sensor_write_table(s_ctrl, ColorEffect_Negative, ARRAY_SIZE(ColorEffect_Negative));
			break;

		default:
			CDBG("--CAMERA-- %s ...Default(Not Support)\n", __func__);
			rc = sensor_write_table(s_ctrl, ColorEffect_None, ARRAY_SIZE(ColorEffect_None));
		}
	}

	return rc;
}

int32_t mi1040_sensor_set_wb(struct msm_sensor_ctrl_t *s_ctrl, int wb)
{
	int32_t rc = 0;
	pr_info("%s +++ wb: %d wb_value: %d\n", __func__, wb, wb_value);

	if(wb_value != wb){
		wb_value = wb;
		switch (wb) {
		case CAMERA_WB_AUTO:
			CDBG("--CAMERA--CAMERA_WB_AUTO\n");
			rc = sensor_write_table(s_ctrl, Whitebalance_Auto, ARRAY_SIZE(Whitebalance_Auto));
			break;
		case CAMERA_WB_INCANDESCENT:
			CDBG("--CAMERA--CAMERA_WB_INCANDESCENT\n");
			rc = sensor_write_table(s_ctrl, Whitebalance_Incandescent, ARRAY_SIZE(Whitebalance_Incandescent));
			break;
		case CAMERA_WB_DAYLIGHT:
			CDBG("--CAMERA--CAMERA_WB_DAYLIGHT\n");
			rc = sensor_write_table(s_ctrl, Whitebalance_Daylight, ARRAY_SIZE(Whitebalance_Daylight));
			break;
		case CAMERA_WB_FLUORESCENT:
			CDBG("--CAMERA--CAMERA_WB_DAYLIGHT\n");
			rc = sensor_write_table(s_ctrl, Whitebalance_Fluorescent, ARRAY_SIZE(Whitebalance_Fluorescent));
			break;
		default:
			rc = sensor_write_table(s_ctrl, Whitebalance_Auto, ARRAY_SIZE(Whitebalance_Auto));
			break;
		}
	}

	return rc;
}

int32_t mi1040_sensor_set_ev(struct msm_sensor_ctrl_t *s_ctrl, int compensation)
{
	int32_t rc = 0;
	pr_info("%s +++ ev: %d ev_value: %d\n", __func__, compensation, ev_value);

	if(ev_value != compensation){
		ev_value = compensation;
		switch (compensation) {
		case CAMERA_EXPOSURE_COMPENSATION_LV0:
			CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV0\n");
			rc = sensor_write_table(s_ctrl, EV_plus_2, ARRAY_SIZE(EV_plus_2));
			break;

		case CAMERA_EXPOSURE_COMPENSATION_LV1:
			CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV1\n");
			rc = sensor_write_table(s_ctrl, EV_plus_1, ARRAY_SIZE(EV_plus_1));
			break;

		case CAMERA_EXPOSURE_COMPENSATION_LV2:
			CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV2\n");
			rc = sensor_write_table(s_ctrl, EV_zero, ARRAY_SIZE(EV_zero));
			break;

		case CAMERA_EXPOSURE_COMPENSATION_LV3:
			CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV3\n");
			rc = sensor_write_table(s_ctrl, EV_minus_1, ARRAY_SIZE(EV_minus_1));
			break;

		case CAMERA_EXPOSURE_COMPENSATION_LV4:
			CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV3\n");
			rc = sensor_write_table(s_ctrl, EV_minus_2, ARRAY_SIZE(EV_minus_2));
			break;

		default:
			CDBG("--CAMERA--ERROR CAMERA_EXPOSURE_COMPENSATION\n");
			rc = sensor_write_table(s_ctrl, EV_zero, ARRAY_SIZE(EV_zero));
			break;
		}
	}

	return rc;
}

static int mi1040_gpio_request(void)
{
	int32_t rc = 0;

	pr_info("%s +++\n", __func__);

	//MI1040 CAM_RST:
	rc = gpio_request(CAM_RST, "mi1040");
	if (rc) {
		pr_err("%s: gpio CAM_RST %d, rc(%d)fail\n", __func__, CAM_RST, rc);
		goto init_probe_fail;
	}

	pr_info("%s ---\n", __func__);
	return rc;

init_probe_fail:
	pr_info("%s fail---\n", __func__);
	return rc;
}

int32_t mi1040_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	pr_info("%s +++\n", __func__);

	if(!s_ctrl->sensordata || !mi1040_s_ctrl.sensordata){
		pr_err("mi1040_s_ctrl.sensordata is NULL, return\n");
		pr_info("%s ---\n", __func__);
		return -1;
	}

	rc = mi1040_gpio_request();
	if(rc < 0)	{
		pr_err("1.2M Camera GPIO request fail!!\n");
		pr_info("%s ---\n", __func__);
		return -1;
	}

	rc = pm8xxx_gpio_config(CAM_RST, &pm_isp_gpio_low);
	if (rc != 0)
		pr_err("gpio 43 CAM_RST config low fail\n");
	else
		pr_info("gpio 43 CAM_RST(%d)\n", gpio_get_value(CAM_RST));

	msleep(5);

	//PMIC regulator - DOVDD 1.8V and AVDD 2.8V ON
	rc = mi1040_regulator_init(true);
	if(rc < 0){
		pr_err("1.2M Camera regulator init fail!!\n");
		pr_info("%s ---\n", __func__);
		return -1;
	}

	//enable MCLK
	msm_sensor_power_up(&mi1040_s_ctrl);

	//MI1040 CAM_RST:
	msleep(5);
	rc = pm8xxx_gpio_config(CAM_RST, &pm_isp_gpio_high);
	if (rc != 0)
		pr_err("gpio 43 CAM_RST config high fail\n");
	else
		pr_info("gpio 43 CAM_RST(%d)\n", gpio_get_value(CAM_RST));

	msleep(50);

	pr_info("%s ---\n", __func__);
	return 0;
}

int32_t mi1040_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	pr_info("%s +++\n", __func__);

	if(!s_ctrl->sensordata || !mi1040_s_ctrl.sensordata){
		pr_info("mi1040_s_ctrl.sensordata is NULL, return\n");
		pr_info("%s ---\n",__func__);
		return -1;
	}

	//CAM_RST low
	rc = pm8xxx_gpio_config(CAM_RST, &pm_isp_gpio_low);
	if (rc != 0)
		pr_err("%s: CAM_RST config low fail\n", __func__);

	//disable MCLK
	msm_sensor_power_down(&mi1040_s_ctrl);

	//PMIC regulator - DOVDD 1.8V and AVDD 2.8V OFF
	mi1040_regulator_init(false);

	msleep(20);
	gpio_free(CAM_RST);
	effect_value = 0;
	wb_value = 0;
	ev_value = 0;
	pr_info("%s ---\n", __func__);
	return 0;
}


int32_t mi1040_sensor_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int32_t rc = 0;
	struct msm_sensor_ctrl_t *s_ctrl;
	unsigned short rdata = 0;

	pr_info("%s +++ \n",__func__);

	s_ctrl = (struct msm_sensor_ctrl_t *)(id->driver_data);
	if (s_ctrl->sensor_i2c_client != NULL) {
		s_ctrl->sensor_i2c_client->client = client;
		if (s_ctrl->sensor_i2c_addr != 0)
			s_ctrl->sensor_i2c_client->client->addr =
				s_ctrl->sensor_i2c_addr;
	} else {
		rc = -EFAULT;
	            pr_info("%s --- \n",__func__);
		return rc;
	}

	if (client->dev.platform_data == NULL) {
		pr_err("%s: NULL sensor data\n", __func__);
		return -EFAULT;
	}

	s_ctrl->sensordata = client->dev.platform_data;
	mi1040_s_ctrl.sensordata = client->dev.platform_data;

	rc = s_ctrl->func_tbl->sensor_power_up(&mi1040_s_ctrl);
	if (rc < 0)
		goto probe_fail;

	sensor_read_word(mi1040_s_ctrl.sensor_i2c_client->client, 0x0, &rdata);
	pr_info("Sensor id: 0x%x\n", rdata);

	mi1040_sensor_read_vendor_id(s_ctrl);
	if(vendor_id != A){
		mi1040_s_ctrl.msm_sensor_reg->init_settings = mi1040_B_init_conf;
		s_ctrl->msm_sensor_reg->init_settings = mi1040_B_init_conf;
	}

	snprintf(s_ctrl->sensor_v4l2_subdev.name,
		sizeof(s_ctrl->sensor_v4l2_subdev.name), "%s", id->name);
	v4l2_i2c_subdev_init(&s_ctrl->sensor_v4l2_subdev, client,
		s_ctrl->sensor_v4l2_subdev_ops);

	msm_sensor_register(&s_ctrl->sensor_v4l2_subdev);

	goto power_down;

probe_fail:
	pr_info("Sensor power on fail\n");

power_down:
	s_ctrl->func_tbl->sensor_power_down(&mi1040_s_ctrl);

	pr_info("%s --- \n",__func__);
	return rc;
}


static const struct i2c_device_id mi1040_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&mi1040_s_ctrl},
	{ }
};

static struct i2c_driver mi1040_i2c_driver = {
	.id_table = mi1040_i2c_id,
	.probe  = mi1040_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client mi1040_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

void create_mi1040_proc_file(void);

static int __init mi1040_sensor_init_module(void)
{
	create_mi1040_proc_file();
	return  i2c_add_driver(&mi1040_i2c_driver);
}

static struct v4l2_subdev_core_ops mi1040_subdev_core_ops = {
	.s_ctrl = msm_sensor_v4l2_s_ctrl,
	.queryctrl = msm_sensor_v4l2_query_ctrl,
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops mi1040_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops mi1040_subdev_ops = {
	.core = &mi1040_subdev_core_ops,
	.video  = &mi1040_subdev_video_ops,
};


static struct msm_sensor_fn_t mi1040_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = mi1040_stop_stream,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
	.sensor_config = msm_sensor_config,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_setting = msm_sensor_setting,
	.sensor_config = msm_sensor_config,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_power_up = mi1040_sensor_power_up,
	.sensor_power_down = mi1040_sensor_power_down,
	.sensor_match_id = mi1040_sensor_match_id,
	.sensor_set_effect = mi1040_sensor_set_effect,
	.sensor_set_wb = mi1040_sensor_set_wb,
	.sensor_set_ev = mi1040_sensor_set_ev,
};

static struct msm_sensor_reg_t mi1040_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = mi1040_config_change_settings,
	.start_stream_conf_size = ARRAY_SIZE(mi1040_config_change_settings),
	.init_settings = &mi1040_init_conf[0],
	.init_size = ARRAY_SIZE(mi1040_init_conf),
	.mode_settings = &mi1040_confs[0],
	.output_settings = &mi1040_dimensions[0],
	.num_conf = ARRAY_SIZE(mi1040_confs),
};

static struct msm_sensor_ctrl_t mi1040_s_ctrl = {
	.msm_sensor_reg = &mi1040_regs,
	.msm_sensor_v4l2_ctrl_info = mi1040_v4l2_ctrl_info,
	.num_v4l2_ctrl = ARRAY_SIZE(mi1040_v4l2_ctrl_info),
	.sensor_i2c_client = &mi1040_sensor_i2c_client,
	.sensor_i2c_addr = 0x90,
	.sensor_output_reg_addr = &mi1040_reg_addr,
	.sensor_id_info = &mi1040_id_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &mi1040_csi_params_array[0],
	.msm_sensor_mutex = &mi1040_mut,
	.sensor_i2c_driver = &mi1040_i2c_driver,
	.sensor_v4l2_subdev_info = mi1040_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(mi1040_subdev_info),
	.sensor_v4l2_subdev_ops = &mi1040_subdev_ops,
	.func_tbl = &mi1040_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

#ifdef	CONFIG_PROC_FS
#define	MI1040_PROC_CAMERA_STATUS	"driver/vga_status"
static ssize_t mi1040_proc_read_vga_status(char *page, char **start, off_t off, int count,
			int *eof, void *data)
{
	int len = 0, rc = 0;

	mi1040_s_ctrl.func_tbl->sensor_power_up(&mi1040_s_ctrl);
	rc = mi1040_s_ctrl.func_tbl->sensor_match_id(&mi1040_s_ctrl);
	mi1040_s_ctrl.func_tbl->sensor_power_down(&mi1040_s_ctrl);

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
void create_mi1040_proc_file(void)
{
	if(create_proc_read_entry(MI1040_PROC_CAMERA_STATUS, 0666, NULL,
			mi1040_proc_read_vga_status, NULL) == NULL){
		pr_err("[Camera]proc file create failed!\n");
	}
}

void remove_mi1040_proc_file(void)
{
    extern struct proc_dir_entry proc_root;
    pr_info("mi1040_s_ctrl_proc_file\n");
    remove_proc_entry(MI1040_PROC_CAMERA_STATUS, &proc_root);
}
#endif
module_init(mi1040_sensor_init_module);
MODULE_DESCRIPTION("Aptina 1.2MP YUV sensor driver");
MODULE_LICENSE("GPL v2");

//Create debugfs for mi1040
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
			pr_info("mi1040 power_off\n");
			err = mi1040_sensor_power_down(&mi1040_s_ctrl);
			if(err)
				return -ENOMEM;
			break;
		}
		case 1:
		{
			pr_info("mi1040 power_on\n");
			err = mi1040_sensor_power_up(&mi1040_s_ctrl);
			if(err)
				return -ENOMEM;
			break;
		}
		case 2:
		{
			pr_info("mi1040 change vendor to A\n");
			snprintf(mi1040_s_ctrl.sensordata->vendor_name,
				sizeof(mi1040_s_ctrl.sensor_v4l2_subdev.name), "%s", "A");
			mi1040_s_ctrl.msm_sensor_reg->init_settings = mi1040_init_conf;
			break;
		}
		case 3:
		{
			pr_info("mi1040 change vendor to B\n");
			snprintf(mi1040_s_ctrl.sensordata->vendor_name,
				sizeof(mi1040_s_ctrl.sensor_v4l2_subdev.name), "%s", "B");
			mi1040_s_ctrl.msm_sensor_reg->init_settings = mi1040_B_init_conf;
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
	unsigned int val = 0;

	if (!mi1040_s_ctrl.sensor_i2c_client->client->adapter)
		return -ENODEV;

	if (*ppos)
		return 0;    /* the end */

//+ parsing......
	len=( count > DBG_TXT_BUF_SIZE-1 ) ? ( DBG_TXT_BUF_SIZE-1 ) : (count);
	if ( copy_from_user( debugTxtBuf, buf, len ) )
		return -EFAULT;

	debugTxtBuf[len] = 0; //add string end

	sscanf(debugTxtBuf, "%x %x", &arg[0], &arg[1]);
	pr_info("command is reg_addr=%x bytes:%x\n", arg[0], arg[1]);

	msg[0].addr = 0x48;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (arg[0] >> 8);;
	data[1] = (u8) (arg[0] & 0xff);

	msg[1].addr = 0x48;
	msg[1].flags = I2C_M_RD;
	msg[1].len = arg[1];
	msg[1].buf = data + 2;

	err = i2c_transfer(mi1040_s_ctrl.sensor_i2c_client->client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	if( arg[1] == 1)
		val = data[2] & 0xff;
	else{
		swap(*(data+2),*(data+3)); //swap high and low byte to match table format
		memcpy(&val, data+2, 2);
	}
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
	pr_info("command is reg_addr=%x value=%x bytes:%x\n", arg[0], arg[1], arg[2]);

	if (!mi1040_s_ctrl.sensor_i2c_client->client->adapter){
		pr_info("%s client->adapter is null",__func__);
		return -ENODEV;
	}

	data[0] = (u8) (arg[0] >> 8);
	data[1] = (u8) (arg[0] & 0xff);

	if( arg[2] == 1){
		data[2] = (u8) (arg[1] & 0xff);
		msg.len = 3;
	}
	else{
		data[2] = (u8) (arg[1] >> 8);
		data[3] = (u8) (arg[1] & 0xff);
		msg.len = 4;
	}
	msg.addr = 0x48;
	msg.flags = 0;
	msg.buf = data;

	do {
		err = i2c_transfer(mi1040_s_ctrl.sensor_i2c_client->client->adapter, &msg, 1);
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
	struct dentry *dent = debugfs_create_dir("mi1040", NULL);

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
