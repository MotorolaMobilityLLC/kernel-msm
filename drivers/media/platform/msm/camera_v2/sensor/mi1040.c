/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include "msm_camera_io_util.h"
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/proc_fs.h>
#include "mi1040.h"
#define MI1040_SENSOR_NAME "mi1040"
#define PLATFORM_DRIVER_NAME "msm_camera_mi1040"
#define mi1040_obj mi1040_##obj

#define PM8921_GPIO_BASE		NR_GPIO_IRQS
#define PM8921_GPIO_PM_TO_SYS(pm_gpio)	(pm_gpio - 1 + PM8921_GPIO_BASE)
#define SENSOR_MAX_RETRIES      3 /* max counter for retry I2C access */
#define CAM_RST PM8921_GPIO_PM_TO_SYS(43)
#define CAM_VENDOR_1 PM8921_GPIO_PM_TO_SYS(11)
#define CAM_VENDOR_0 PM8921_GPIO_PM_TO_SYS(10)
#define CAM_LENS PM8921_GPIO_PM_TO_SYS(24)

/*#define CONFIG_MSMB_CAMERA_DEBUG*/
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

DEFINE_MSM_MUTEX(mi1040_mut);
static struct msm_sensor_ctrl_t mi1040_s_ctrl;
static int effect_value = -1;
static int wb_value = -1;
static int ev_value = -1;
static int fps_value = -1;
static int vendor_id = -1;

static struct msm_sensor_power_setting mi1040_power_setting[] = {
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 50,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 50,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 50,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 50,
	},
};

static struct v4l2_subdev_info mi1040_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static const struct i2c_device_id mi1040_i2c_id[] = {
	{MI1040_SENSOR_NAME, (kernel_ulong_t)&mi1040_s_ctrl},
	{ }
};

static struct msm_camera_i2c_client mi1040_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id mi1040_dt_match[] = {
	{.compatible = "qcom,mi1040", .data = &mi1040_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, mi1040_dt_match);

static struct platform_driver mi1040_platform_driver = {
	.driver = {
		.name = "qcom,mi1040",
		.owner = THIS_MODULE,
		.of_match_table = mi1040_dt_match,
	},
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
	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	msg[1].addr = 0x48;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = data + 2;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	swap(*(data+2), *(data+3));
	memcpy(val, data+2, 2);

	return 0;
}

static int sensor_write_reg(struct i2c_client *client, u16 addr,
	u16 val, u16 data_type)
{
	int err;
	struct i2c_msg msg;
	unsigned char data[8];
	int retry = 0;

	if (!client->adapter)
		return -ENODEV;

	data[0] = (u8) (addr >> 8);
	data[1] = (u8) (addr & 0xff);

	switch (data_type) {
	case 1:
			data[2] = (u8)(val & 0xff);
			msg.len = 3;
			break;
	case 2:
			data[2] = (u8)((val & 0xff00) >> 8);
			data[3] = (u8)(val & 0x00ff);
			msg.len = 4;
			break;
	default:
			break;
	}

	msg.addr = 0x48;
	msg.flags = 0;
	msg.buf = data;

	do {
		err = i2c_transfer(client->adapter, &msg, 1);
		if (err == 1)
			return 0;
		retry++;
		pr_err("yuv_sensor : i2c transfer failed, retrying %x %x\n",
			addr, val);
		pr_err("yuv_sensor : i2c transfer failed, count %x\n",
			msg.addr);
	} while (retry <= SENSOR_MAX_RETRIES);
	return err;
}

static int sensor_write_table(struct msm_sensor_ctrl_t *s_ctrl,
			      struct msm_camera_i2c_reg_conf *data,
			      int data_num)
{
	int i, data_type;
	int32_t rc = -EFAULT;

	for (i = 0; i < data_num; i++) {
		if ((data+i)->dt == 1)
			data_type = (data+i)->dt;
		else
			data_type = 2;
		rc = sensor_write_reg(mi1040_s_ctrl.sensor_i2c_client->client,
			(data+i)->reg_addr, (data+i)->reg_data, data_type);
	}
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

	switch (vendor) {
	case 0:
			vendor_id =  B;
			snprintf(s_ctrl->sensordata->vendor_name,
				sizeof(s_ctrl->sensordata->vendor_name),
				"%s", "B");
			pr_info("Vendor is B\n");
			break;
	case 1:
			vendor_id = A;
			snprintf(s_ctrl->sensordata->vendor_name,
				sizeof(s_ctrl->sensordata->vendor_name),
				"%s", "A");
			pr_info("Vendor is A\n");
			break;
	default:
			vendor_id =  B;
			pr_info("Default Vendor is B\n");
			break;
	}
	return vendor_id;
}

int32_t mi1040_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t rdata = 0;
	pr_info("%s +++\n", __func__);

	sensor_read_word(mi1040_s_ctrl.sensor_i2c_client->client,
		s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
		&rdata);
	if (rdata == s_ctrl->sensordata->slave_info->sensor_id)
		rc = 0;
	else
		rc = -EFAULT;

	pr_info("Sensor id: 0x%x\n", rdata);

	mi1040_sensor_read_vendor_id(s_ctrl);

	pr_info("%s ---\n", __func__);
	return rc;
}

static struct i2c_driver mi1040_i2c_driver = {
	.id_table = mi1040_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = MI1040_SENSOR_NAME,
	},
};

static int32_t mi1040_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(mi1040_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

#ifdef	CONFIG_PROC_FS
#define	MI1040_PROC_CAMERA_STATUS	"driver/vga_status"
static ssize_t mi1040_proc_read_vga_status(char *page,
	char **start, off_t off, int count, int *eof, void *data)
{
	int len = 0, rc = 0;

	mi1040_s_ctrl.func_tbl->sensor_power_up(&mi1040_s_ctrl);
	rc = mi1040_s_ctrl.func_tbl->sensor_match_id(&mi1040_s_ctrl);
	mi1040_s_ctrl.func_tbl->sensor_power_down(&mi1040_s_ctrl);

	if (*eof == 0) {
		if (!rc)
			len += snprintf(page+len, 10, "1\n");
		else
			len += snprintf(page+len, 10, "0\n");
		*eof = 1;
		pr_info("%s: string:%s", __func__, (char *)page);
	}
	return len;
}
void create_mi1040_proc_file(void)
{
	if (create_proc_read_entry(MI1040_PROC_CAMERA_STATUS, 0666, NULL,
			mi1040_proc_read_vga_status, NULL) == NULL) {
		pr_err("[Camera]proc file create failed!\n");
	}
}

void remove_mi1040_proc_file(void)
{
	pr_info("mi1040_s_ctrl_proc_file\n");
	remove_proc_entry(MI1040_PROC_CAMERA_STATUS, &proc_root);
}
#endif

static int __init mi1040_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	create_mi1040_proc_file();
	rc = platform_driver_probe(&mi1040_platform_driver,
		mi1040_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&mi1040_i2c_driver);
}

static void __exit mi1040_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (mi1040_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&mi1040_s_ctrl);
		platform_driver_unregister(&mi1040_platform_driver);
	} else
		i2c_del_driver(&mi1040_i2c_driver);
	return;
}

int32_t mi1040_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	long rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);

		break;
	case CFG_SET_INIT_SETTING:
		/* 1. Write Recommend settings */
		/* 2. Write change settings */
		if (vendor_id == A) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client,
				mi1040_recommend_settings,
				ARRAY_SIZE(mi1040_recommend_settings),
				MSM_CAMERA_I2C_WORD_DATA);
		} else {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_write_conf_tbl(
				s_ctrl->sensor_i2c_client,
				mi1040_B_recommend_settings,
				ARRAY_SIZE(mi1040_B_recommend_settings),
				MSM_CAMERA_I2C_WORD_DATA);
		}

		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			mi1040_config_change_settings,
			ARRAY_SIZE(mi1040_config_change_settings),
			MSM_CAMERA_I2C_WORD_DATA);
		break;
	case CFG_SET_RESOLUTION:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, mi1040_960p_settings,
			ARRAY_SIZE(mi1040_960p_settings),
			MSM_CAMERA_I2C_WORD_DATA);
		break;
	case CFG_SET_STOP_STREAM:
		break;
	case CFG_SET_START_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			mi1040_config_change_settings,
			ARRAY_SIZE(mi1040_config_change_settings),
			MSM_CAMERA_I2C_WORD_DATA);
		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params =
			*s_ctrl->sensordata->sensor_init_params;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down) {
			rc = s_ctrl->func_tbl->sensor_power_down(
				s_ctrl);
			effect_value = -1;
			wb_value = -1;
			ev_value = -1;
			fps_value = -1;
		}
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
		    (void *)reg_setting, stop_setting->size *
		    sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
	}

	case CFG_SET_EFFECT:
		if (s_ctrl->func_tbl->
		sensor_set_effect == NULL) {
			rc = -EFAULT;
			break;
		}
		rc = s_ctrl->func_tbl->
			sensor_set_effect(
				s_ctrl,
				cdata->cfg.effect);
		break;

	case CFG_SET_WB:
		if (s_ctrl->func_tbl->
		sensor_set_wb == NULL) {
			rc = -EFAULT;
			break;
		}
		rc = s_ctrl->func_tbl->
			sensor_set_wb(
				s_ctrl,
				cdata->cfg.wb_val);
		break;

	case CFG_SET_EXPOSURE_COMPENSATION:
		if (s_ctrl->func_tbl->
		sensor_set_ev == NULL) {
			rc = -EFAULT;
			break;
		}
		rc = s_ctrl->func_tbl->
			sensor_set_ev(
				s_ctrl,
				cdata->cfg.exp_compensation);
		break;

	case CFG_SET_FPS:
		if (s_ctrl->func_tbl->
		sensor_set_fps == NULL) {
			rc = -EFAULT;
			break;
		}
		rc = s_ctrl->func_tbl->
			sensor_set_fps(
				s_ctrl,
				cdata->cfg.fps);
		break;

	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

int32_t mi1040_sensor_set_effect(struct msm_sensor_ctrl_t *s_ctrl,
	int effect)
{
	int32_t rc = 0;
	if (effect_value != effect) {
		pr_info("%s +++ effect: %d effect_value: %d\n", __func__,
			effect, effect_value);
		effect_value = effect;
		switch (effect)	{
		case CAMERA_EFFECT_MONO:
			CDBG("--CAMERA-- %s ...CAMERA_EFFECT_MONO\n",
				__func__);
			rc = sensor_write_table(s_ctrl, ColorEffect_Mono,
				ARRAY_SIZE(ColorEffect_Mono));
			break;

		case CAMERA_EFFECT_SOLARIZE:
			CDBG("--CAMERA-- %s ...CAMERA_EFFECT_NEGATIVE\n",
				__func__);
			rc = sensor_write_table(s_ctrl, ColorEffect_Solarize,
				ARRAY_SIZE(ColorEffect_Solarize));
			break;

		case CAMERA_EFFECT_SEPIA:
			CDBG("--CAMERA-- %s ...CAMERA_EFFECT_SEPIA\n",
				__func__);
			rc = sensor_write_table(s_ctrl, ColorEffect_Sepia,
				ARRAY_SIZE(ColorEffect_Sepia));
			break;

		case CAMERA_EFFECT_NEGATIVE:
			CDBG("--CAMERA-- %s ...CAMERA_EFFECT_NEGATIVE\n",
				__func__);
			rc = sensor_write_table(s_ctrl, ColorEffect_Negative,
				ARRAY_SIZE(ColorEffect_Negative));
			break;

		default:
			CDBG("--CAMERA-- %s ...Default(Not Support)\n",
				__func__);
			rc = sensor_write_table(s_ctrl, ColorEffect_None,
				ARRAY_SIZE(ColorEffect_None));
		}
	}

	return rc;
}

int32_t mi1040_sensor_set_wb(struct msm_sensor_ctrl_t *s_ctrl, int wb)
{
	int32_t rc = 0;
	if (wb_value != wb) {
		pr_info("%s +++ wb: %d wb_value: %d\n", __func__,
			wb, wb_value);
		wb_value = wb;
		switch (wb) {
		case YUV_CAMERA_WB_AUTO:
			CDBG("--CAMERA--YUV_CAMERA_WB_AUTO\n");
			rc = sensor_write_table(s_ctrl, Whitebalance_Auto,
				ARRAY_SIZE(Whitebalance_Auto));
			break;
		case YUV_CAMERA_WB_INCANDESCENT:
			CDBG("--CAMERA--YUV_CAMERA_WB_INCANDESCENT\n");
			rc = sensor_write_table(s_ctrl,
				Whitebalance_Incandescent,
				ARRAY_SIZE(Whitebalance_Incandescent));
			break;
		case YUV_CAMERA_WB_DAYLIGHT:
			CDBG("--CAMERA--YUV_CAMERA_WB_DAYLIGHT\n");
			rc = sensor_write_table(s_ctrl,
				Whitebalance_Daylight,
				ARRAY_SIZE(Whitebalance_Daylight));
			break;
		case YUV_CAMERA_WB_FLUORESCENT:
			CDBG("--CAMERA--YUV_CAMERA_WB_FLUORESCENT\n");
			rc = sensor_write_table(s_ctrl,
				Whitebalance_Fluorescent,
				ARRAY_SIZE(Whitebalance_Fluorescent));
			break;
		case YUV_CAMERA_WB_CLOUDY_DAYLIGHT:
			CDBG("--CAMERA--YUV_CAMERA_WB_CLOUDY_DAYLIGHT\n");
			rc = sensor_write_table(s_ctrl,
				Whitebalance_Cloudy,
				ARRAY_SIZE(Whitebalance_Cloudy));
			break;
		default:
			rc = sensor_write_table(s_ctrl, Whitebalance_Auto,
				ARRAY_SIZE(Whitebalance_Auto));
			break;
		}
	}

	return rc;
}

int32_t mi1040_sensor_set_ev(struct msm_sensor_ctrl_t *s_ctrl, int compensation)
{
	int32_t rc = 0;
	if (ev_value != compensation) {
		pr_info("%s +++ ev: %d ev_value: %d\n", __func__,
			compensation, ev_value);
		ev_value = compensation;
		switch (compensation) {
		case CAMERA_EXPOSURE_COMPENSATION_LV0:
			CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV0\n");
			rc = sensor_write_table(s_ctrl, EV_plus_2,
				ARRAY_SIZE(EV_plus_2));
			break;

		case CAMERA_EXPOSURE_COMPENSATION_LV1:
			CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV1\n");
			rc = sensor_write_table(s_ctrl, EV_plus_1,
				ARRAY_SIZE(EV_plus_1));
			break;

		case CAMERA_EXPOSURE_COMPENSATION_LV2:
			CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV2\n");
			rc = sensor_write_table(s_ctrl, EV_zero,
				ARRAY_SIZE(EV_zero));
			break;

		case CAMERA_EXPOSURE_COMPENSATION_LV3:
			CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV3\n");
			rc = sensor_write_table(s_ctrl, EV_minus_1,
				ARRAY_SIZE(EV_minus_1));
			break;

		case CAMERA_EXPOSURE_COMPENSATION_LV4:
			CDBG("--CAMERA--CAMERA_EXPOSURE_COMPENSATION_LV3\n");
			rc = sensor_write_table(s_ctrl, EV_minus_2,
				ARRAY_SIZE(EV_minus_2));
			break;

		default:
			CDBG("--CAMERA--ERROR CAMERA_EXPOSURE_COMPENSATION\n");
			rc = sensor_write_table(s_ctrl, EV_zero,
				ARRAY_SIZE(EV_zero));
			break;
		}
	}

	return rc;
}

int32_t mi1040_sensor_set_fps(struct msm_sensor_ctrl_t *s_ctrl, int fps)
{
	int32_t rc = 0;

	if (fps_value != fps) {
		pr_info("%s +++ fps: %d fps_value: %d\n", __func__,
			fps, fps_value);
		fps_value = fps;
		switch (fps) {
		case CAMERA_FPS_FIX_30:
			CDBG("--CAMERA--CAMERA_FPS_FIX_30\n");
			rc = sensor_write_table(s_ctrl, Fix_30_fps,
				ARRAY_SIZE(Fix_30_fps));
			break;
		case CAMERA_FPS_FIX_25:
			CDBG("--CAMERA--CAMERA_FPS_FIX_25\n");
			rc = sensor_write_table(s_ctrl, Fix_25_fps,
				ARRAY_SIZE(Fix_25_fps));
			break;
		case CAMERA_FPS_FIX_24:
			CDBG("--CAMERA--CAMERA_FPS_FIX_24\n");
			rc = sensor_write_table(s_ctrl, Fix_24_fps,
				ARRAY_SIZE(Fix_24_fps));
			break;
		case CAMERA_FPS_FIX_20:
			CDBG("--CAMERA--CAMERA_FPS_FIX_20\n");
			rc = sensor_write_table(s_ctrl, Fix_20_fps,
				ARRAY_SIZE(Fix_20_fps));
			break;
		case CAMERA_FPS_FIX_15:
			CDBG("--CAMERA--CAMERA_FPS_FIX_15\n");
			rc = sensor_write_table(s_ctrl, Fix_15_fps,
				ARRAY_SIZE(Fix_15_fps));
			break;
		case CAMERA_FPS_AUTO_30:
			CDBG("--CAMERA--ERROR CAMERA_FPS_AUTO_30\n");
			rc = sensor_write_table(s_ctrl, auto_30_fps,
				ARRAY_SIZE(auto_30_fps));
			break;
		default:
			CDBG("--CAMERA--CAMERA_FPS_AUTO\n");
			rc = sensor_write_table(s_ctrl, auto_30_fps,
				ARRAY_SIZE(auto_30_fps));
			break;
		}
		msleep(20);
	}

	return rc;
}

static struct msm_sensor_fn_t mi1040_sensor_func_tbl = {
	.sensor_config = mi1040_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = mi1040_sensor_match_id,
	.sensor_set_effect = mi1040_sensor_set_effect,
	.sensor_set_wb = mi1040_sensor_set_wb,
	.sensor_set_ev = mi1040_sensor_set_ev,
	.sensor_set_fps = mi1040_sensor_set_fps,
};

static struct msm_sensor_ctrl_t mi1040_s_ctrl = {
	.sensor_i2c_client = &mi1040_sensor_i2c_client,
	.power_setting_array.power_setting = mi1040_power_setting,
	.power_setting_array.size = ARRAY_SIZE(mi1040_power_setting),
	.msm_sensor_mutex = &mi1040_mut,
	.sensor_v4l2_subdev_info = mi1040_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(mi1040_subdev_info),
	.func_tbl = &mi1040_sensor_func_tbl,
};

module_init(mi1040_init_module);
module_exit(mi1040_exit_module);
MODULE_DESCRIPTION("Aptina 1.26MP YUV sensor driver");
MODULE_LICENSE("GPL v2");

/*Create debugfs for mi1040 */
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
		return 0;

	len = (count > DBG_TXT_BUF_SIZE-1) ? (DBG_TXT_BUF_SIZE-1) : (count);
	if (copy_from_user(debugTxtBuf, buf, len))
		return -EFAULT;

	debugTxtBuf[len] = 0;

	sscanf(debugTxtBuf, "%x", &arg[0]);
	pr_info("1 is open_camera 0 is close_camera\n");
	pr_info("command is arg1=%x\n", arg[0]);

	*ppos = len;

	switch (arg[0]) {
	case 0:
			pr_info("mi1040 power_off\n");
			err = msm_sensor_power_down(&mi1040_s_ctrl);
			if (err)
				return -ENOMEM;
			break;
	case 1:
			pr_info("mi1040 power_on\n");
			err = msm_sensor_power_up(&mi1040_s_ctrl);
			if (err)
				return -ENOMEM;
			break;
	default:
			break;
	}

	return len;
}

static ssize_t i2c_camera_read(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
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
		return 0;

	len = (count > DBG_TXT_BUF_SIZE-1) ? (DBG_TXT_BUF_SIZE-1) : (count);
	if (copy_from_user(debugTxtBuf, buf, len))
		return -EFAULT;

	debugTxtBuf[len] = 0;

	sscanf(debugTxtBuf, "%x %x", &arg[0], &arg[1]);
	pr_info("command is reg_addr=%x bytes:%x\n", arg[0], arg[1]);

	msg[0].addr = 0x48;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u8) (arg[0] >> 8);
	data[1] = (u8) (arg[0] & 0xff);

	msg[1].addr = 0x48;
	msg[1].flags = I2C_M_RD;
	msg[1].len = arg[1];
	msg[1].buf = data + 2;

	err = i2c_transfer(mi1040_s_ctrl.
		sensor_i2c_client->client->adapter, msg, 2);

	if (err != 2)
		return -EINVAL;

	if (arg[1] == 1)
		val = data[2] & 0xff;
	else{
		swap(*(data+2), *(data+3));
		memcpy(&val, data+2, 2);
	}
	register_value = val;
	pr_info("register value: 0x%x\n", val);

	*ppos = len;
	return len;    /* the end */
}

static ssize_t i2c_camera_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	int len;
	u32 arg[3];
	int err;
	struct i2c_msg msg;
	unsigned char data[4];
	int retry = 0;

	if (*ppos)
		return 0;

	len = (count > DBG_TXT_BUF_SIZE-1) ? (DBG_TXT_BUF_SIZE-1) : (count);
	if (copy_from_user(debugTxtBuf, buf, len))
		return -EFAULT;

	debugTxtBuf[len] = 0;

	sscanf(debugTxtBuf, "%x %x %x", &arg[0], &arg[1], &arg[2]);
	pr_info("command is reg_addr=%x value=%x bytes:%x\n",
		arg[0], arg[1], arg[2]);

	if (!mi1040_s_ctrl.sensor_i2c_client->client->adapter) {
		pr_info("%s client->adapter is null", __func__);
		return -ENODEV;
	}

	data[0] = (u8) (arg[0] >> 8);
	data[1] = (u8) (arg[0] & 0xff);

	if (arg[2] == 1) {
		data[2] = (u8) (arg[1] & 0xff);
		msg.len = 3;
	} else{
		data[2] = (u8) (arg[1] >> 8);
		data[3] = (u8) (arg[1] & 0xff);
		msg.len = 4;
	}
	msg.addr = 0x48;
	msg.flags = 0;
	msg.buf = data;

	do {
		err = i2c_transfer(mi1040_s_ctrl.
			sensor_i2c_client->client->adapter, &msg, 1);
		if (err == 1) {
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

static int i2c_read_value(struct file *file, char __user *buf,
	size_t count, loff_t *ppos)
{
	int len = 0;
	char *bp = debugTxtBuf;

	if (*ppos)
		return 0;    /* the end */

	len = snprintf(bp, DBG_TXT_BUF_SIZE, "Register value is 0x%X\n",
		register_value);

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
