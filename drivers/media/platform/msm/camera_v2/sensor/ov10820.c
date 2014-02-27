/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include "ov660.h"
#include "msm_camera_io_util.h"
#include <linux/regulator/consumer.h>
#include "msm_cci.h"
#include <mach/gpiomux.h>
#include "msm_camera_i2c_mux.h"
#include <linux/device.h>

#define OV10820_SENSOR_NAME     "ov10820"

#define OV10820_SECONDARY_I2C_ADDRESS 0x20
#define OV10820_HW_REV_REG 0x302a

#define VREG_ON              1
#define VREG_OFF             0
#define GPIO_REQUEST_USE     1
#define GPIO_REQUEST_NO_USE  0
#define CLK_ON               1
#define CLK_OFF              0
#define CLK_NUM              2

/* This list of enum has to match what's defined in dtsi file */
enum msm_sensor_ov10820_vreg_t {
	OV10820_CAM_VIO,
	OV10820_CAM_VDIG,
	OV10820_CAM_VAF,
};

DEFINE_MSM_MUTEX(ov10820_mut);
static uint16_t ov10820_hw_rev;

static struct msm_sensor_ctrl_t ov10820_s_ctrl;
static bool ov660_exists;

static struct regulator *cam_afvdd;
static struct regulator *cam_ov660_dvdd_pk;
static struct regulator *cam_vddio;
struct clk *cam_mclk[CLK_NUM];

static ssize_t show_ov10820(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%x\n", ov10820_hw_rev);
}

static ssize_t store_ov10820(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(ov10820_hw_rev, 0444, show_ov10820, store_ov10820);

static struct kobject *ov10820_kobject;
static struct attribute *ov10820_attributes[] = {
	&dev_attr_ov10820_hw_rev.attr,
	NULL,
};

static const struct attribute_group ov10820_group = {
	.attrs = ov10820_attributes,
};

static struct v4l2_subdev_info ov10820_subdev_info[] = {
	{
		.code       = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt        = 1,
		.order      = 0,
	},
};

static const struct i2c_device_id ov10820_i2c_id[] = {
	{OV10820_SENSOR_NAME, (kernel_ulong_t)&ov10820_s_ctrl},
	{ }
};

static int32_t msm_ov10820_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ov10820_s_ctrl);
}

static struct i2c_driver ov10820_i2c_driver = {
	.id_table = ov10820_i2c_id,
	.probe = msm_ov10820_i2c_probe,
	.driver = {
		.name = OV10820_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ov10820_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ov10820_dt_match[] = {
	{.compatible = "qcom,ov10820", .data = &ov10820_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ov10820_dt_match);

static struct platform_driver ov10820_platform_driver = {
	.driver = {
		.name = "qcom,ov10820",
		.owner = THIS_MODULE,
		.of_match_table = ov10820_dt_match,
	},
};

static int32_t ov10820_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	if (ov660_check_probe(s_ctrl) >= 0)
		ov660_exists = true;
	if (ov660_exists)
		ov660_set_i2c_bypass(s_ctrl, (int)1);

	/* Query each i2c slave address for the camera to if the
	 * part exists since the sensor has two possible i2c slave
	 * addresses. */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc >= 0)
		goto check_chipid;

	s_ctrl->sensordata->slave_info->sensor_slave_addr =
		OV10820_SECONDARY_I2C_ADDRESS;
	s_ctrl->sensor_i2c_client->cci_client->sid =
		(OV10820_SECONDARY_I2C_ADDRESS >> 1);
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("ov10820_match_id read failed");
		return rc;
	}

check_chipid:
	if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
		pr_err("%s: chip id %x does not match expected %x\n", __func__,
				chipid, s_ctrl->sensordata->
				slave_info->sensor_id);
		return -ENODEV;
	}

	/* Check hardware revision */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			OV10820_HW_REV_REG,
			&ov10820_hw_rev,
			MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: unable to determine hw rev of sensor!\n", __func__);
		return rc;
	}

	pr_info("%s: success addr:0x%x hw:0x%x\n",
			__func__, s_ctrl->sensor_i2c_client->cci_client->sid,
			ov10820_hw_rev);
	return 0;
}

static int32_t ov10820_sensor_power_down(
		struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_board_info *info = s_ctrl->sensordata;
	struct msm_sensor_power_setting_array *power_setting_array = NULL;
	struct msm_sensor_power_setting *power_setting = NULL;

	pr_debug("%s\n", __func__);

	power_setting_array = &s_ctrl->power_setting_array;
	power_setting = &power_setting_array->power_setting[0];

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}

	/*Disable MCLK*/
	rc = msm_cam_clk_enable(s_ctrl->dev, &s_ctrl->clk_info[0],
				(struct clk **)&cam_mclk[0],
				s_ctrl->clk_info_size, CLK_OFF);
	if (rc < 0)
		pr_err("ov10820: msm_cam_clk_enable failed (%d)\n", rc);
	usleep_range(500, 600);

	/*Reset 10MP*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[4].gpio,
			GPIO_OUT_LOW);
	usleep_range(100, 200);

	/*Reset OV660*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[5].gpio,
			GPIO_OUT_LOW);
	usleep_range(100, 200);

	/*Set 10MP DVDD Low*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_LOW);
	usleep_range(100, 200);

	/*Set OV660 DVDD low*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[6].gpio,
			GPIO_OUT_LOW);
	usleep_range(100, 200);

	/*Disable AVDD for 10MP*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[0].gpio,
			GPIO_OUT_LOW);
	usleep_range(100, 200);

	/*Set standby state to be on*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_LOW);
	usleep_range(100, 200);

	/*Turn off VDDIO*/
	rc = msm_camera_config_single_vreg(s_ctrl->dev,
			&s_ctrl->sensordata->cam_vreg[OV10820_CAM_VIO],
				&cam_vddio, VREG_OFF);
	usleep_range(500, 600);

	/*Turn off VDD PK*/
	msm_camera_config_single_vreg(s_ctrl->dev,
			&info->cam_vreg[OV10820_CAM_VDIG],
			&cam_ov660_dvdd_pk, VREG_OFF);

	/*Turn off AF regulator supply*/
	rc = msm_camera_config_single_vreg(s_ctrl->dev,
			&s_ctrl->sensordata->cam_vreg[OV10820_CAM_VAF],
			&cam_afvdd, VREG_OFF);
	usleep_range(100, 200);

	msm_camera_request_gpio_table(
		info->gpio_conf->cam_gpio_req_tbl,
		info->gpio_conf->cam_gpio_req_tbl_size, GPIO_REQUEST_NO_USE);

	return rc;
}

int32_t ov10820_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_board_info *info = s_ctrl->sensordata;
	struct device *dev = s_ctrl->dev;

	pr_debug("%s\n", __func__);
	if (info->gpio_conf->cam_gpiomux_conf_tbl != NULL) {
		msm_gpiomux_install(
			(struct msm_gpiomux_config *)
			info->gpio_conf->cam_gpiomux_conf_tbl,
			info->gpio_conf->cam_gpiomux_conf_tbl_size);
	}

	rc = msm_camera_request_gpio_table(
		info->gpio_conf->cam_gpio_req_tbl,
		info->gpio_conf->cam_gpio_req_tbl_size, GPIO_REQUEST_USE);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	/*Set PWDN to low for power up*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_LOW);

	/*Reset OV660*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[5].gpio,
			GPIO_OUT_LOW);

	/*Reset 10MP*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[4].gpio,
			GPIO_OUT_LOW);

	/*Turn on mclk */
	rc = msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
				(struct clk **)&cam_mclk[0],
				s_ctrl->clk_info_size, CLK_ON);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto abort0;
	}

	/*Turn on vdd pk for ov660*/
	rc = msm_camera_config_single_vreg(dev,
			&info->cam_vreg[OV10820_CAM_VDIG],
			&cam_ov660_dvdd_pk, VREG_ON);
	if (rc < 0) {
		pr_err("%s: Unable to turn on cam_pk_dvdd (%d)\n",
				__func__, rc);
		goto abort1;
	}
	usleep_range(1000, 2000);

	/*Turn on VIO for both 10MP and OV660*/
	rc = msm_camera_config_single_vreg(dev,
			&info->cam_vreg[OV10820_CAM_VIO],
			&cam_vddio, VREG_ON);

	if (rc < 0) {
		pr_err("%s: Unable to turn on VDDIO (%d)\n", __func__, rc);
		goto abort2;
	}
	usleep_range(500, 600);

	/*Turn on AF Supply*/
	rc = msm_camera_config_single_vreg(dev,
			&info->cam_vreg[OV10820_CAM_VAF],
			&cam_afvdd, VREG_ON);

	if (rc < 0) {
		pr_err("%s: Unable to turn on AF Supply (%d)\n", __func__, rc);
		goto abort3;
	}
	usleep_range(500, 600);

	/*Enable 1.2V PD*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[6].gpio,
			GPIO_OUT_HIGH);
	usleep_range(500, 600);

	/*Enable 10mp analog*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[0].gpio,
			GPIO_OUT_HIGH);
	usleep_range(500, 600);

	/*Enable 10mp 1.2v*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_HIGH);
	usleep_range(500, 600);

	/*Wait for core supplies to power up*/
	usleep_range(1000, 2000);

	/*Set PWRDWN to high for normal mode for 10MP and OV660*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_HIGH);
	usleep_range(500, 600);

	/*Set reset high for 10MP*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[4].gpio,
			GPIO_OUT_HIGH);

	/*Set reset high for OV660*/
	gpio_direction_output(info->gpio_conf->cam_gpio_req_tbl[5].gpio,
			GPIO_OUT_HIGH);
	usleep_range(500, 600);

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto abort4;
		}
	}

	rc = ov10820_match_id(s_ctrl);
	if (rc < 0) {
		pr_err("%s:%d match id failed rc %d\n", __func__, __LINE__, rc);
		goto abort5;
	}

	goto power_up_done;

abort5:
	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
				s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}
abort4:
	msm_camera_config_single_vreg(s_ctrl->dev,
			&info->cam_vreg[OV10820_CAM_VIO],
			&cam_vddio, VREG_OFF);
abort3:
	msm_camera_config_single_vreg(s_ctrl->dev,
			&info->cam_vreg[OV10820_CAM_VAF],
			&cam_afvdd, VREG_OFF);
abort2:
	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[OV10820_CAM_VDIG],
			&cam_ov660_dvdd_pk, VREG_OFF);
abort1:
	msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
			(struct clk **)&cam_mclk[0],
			s_ctrl->clk_info_size, CLK_OFF);
abort0:
	msm_camera_request_gpio_table(
			info->gpio_conf->cam_gpio_req_tbl,
			info->gpio_conf->cam_gpio_req_tbl_size,
			GPIO_REQUEST_NO_USE);
power_up_done:
	return rc;
}

static int32_t ov10820_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	match = of_match_device(ov10820_dt_match, &pdev->dev);
	if (match) {
		rc = msm_sensor_platform_probe(pdev, match->data);
	} else {
		pr_err("%s:%d failed match device\n", __func__, __LINE__);
		return -EINVAL;
	}

	ov10820_kobject = kobject_create_and_add("ov10820", fs_kobj);
	if (ov10820_kobject == NULL) {
		pr_err("%s: unable to create kobject!\n", __func__);
		return -EINVAL;
	}

	rc = sysfs_create_group(ov10820_kobject, &ov10820_group);
	if (rc) {
		pr_err("%s: unable to create kobject group!\n", __func__);
		kobject_del(ov10820_kobject);
		ov10820_kobject = NULL;
	}

	return rc;
}

static int __init ov10820_init_module(void)
{
	int32_t rc = 0;
	rc = platform_driver_probe(&ov10820_platform_driver,
			ov10820_platform_probe);
	if (!rc)
		return rc;

	return i2c_add_driver(&ov10820_i2c_driver);
}

static void __exit ov10820_exit_module(void)
{
	if (ov10820_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ov10820_s_ctrl);
		platform_driver_unregister(&ov10820_platform_driver);
	} else
		i2c_del_driver(&ov10820_i2c_driver);

	if (ov10820_kobject) {
		sysfs_remove_group(ov10820_kobject, &ov10820_group);
		kobject_del(ov10820_kobject);
	}
	return;
}

static struct msm_sensor_fn_t ov10820_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = ov10820_sensor_power_up,
	.sensor_power_down = ov10820_sensor_power_down,
	.sensor_match_id = ov10820_match_id,
};

static struct msm_sensor_ctrl_t ov10820_s_ctrl = {
	.sensor_i2c_client = &ov10820_sensor_i2c_client,
	.msm_sensor_mutex = &ov10820_mut,
	.sensor_v4l2_subdev_info = ov10820_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ov10820_subdev_info),
	.func_tbl = &ov10820_func_tbl,
};

module_init(ov10820_init_module);
module_exit(ov10820_exit_module);
MODULE_DESCRIPTION("ov10820");
MODULE_LICENSE("GPL v2");
