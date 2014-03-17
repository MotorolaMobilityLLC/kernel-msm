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
#include <linux/of_gpio.h>
#include "msm_camera_io_util.h"
#include <linux/regulator/consumer.h>
#include "msm_cci.h"
#include <mach/gpiomux.h>
#include "msm_camera_i2c_mux.h"
#include <linux/device.h>

/*
 * Highjack the sensor_power_down routine
 * in order to implement alternate I2C
 * address selection
 */
/*
#define SET_ALT_I2C_ADDRESS
 */

#ifdef SET_ALT_I2C_ADDRESS
#include <mach/gpiomux.h>
#include "msm_sensor.h"
#include "msm_sd.h"
#include "camera.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include <mach/rpm-regulator.h>
#include <mach/rpm-regulator-smd.h>
#include <linux/regulator/consumer.h>
#endif

#define AR0261_SENSOR_NAME "ar0261"
#define ALT_I2C_ADDRESS 0x6E
#define PRI_I2C_ADDRESS 0x6C
DEFINE_MSM_MUTEX(ar0261_mut);

static struct msm_sensor_ctrl_t ar0261_s_ctrl;

static struct regulator *ar0261_cam_vddio;
static struct regulator *ar0261_cam_vdd;
static struct regulator *ar0261_cam_vana;
struct clk *ar0261_cam_mclk[2];

static bool ar0261_devboard_config;

#define VREG_ON              1
#define VREG_OFF             0
#define GPIO_REQUEST_USE     1
#define GPIO_REQUEST_NO_USE  0
#define CLK_ON               1
#define CLK_OFF              0

static struct v4l2_subdev_info ar0261_subdev_info[] = {
	{
		.code		= V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace	= V4L2_COLORSPACE_JPEG,
		.fmt		= 1,
		.order		= 0,
	},
};

static const struct i2c_device_id ar0261_i2c_id[] = {
	{AR0261_SENSOR_NAME, (kernel_ulong_t)&ar0261_s_ctrl},
	{ }
};

static int32_t msm_ar0261_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &ar0261_s_ctrl);
}

static struct i2c_driver ar0261_i2c_driver = {
	.id_table = ar0261_i2c_id,
	.probe  = msm_ar0261_i2c_probe,
	.driver = {
		.name = AR0261_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client ar0261_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id ar0261_dt_match[] = {
	{.compatible = "qcom,ar0261", .data = &ar0261_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, ar0261_dt_match);

static struct platform_driver ar0261_platform_driver = {
	.driver = {
		.name = "qcom,ar0261",
		.owner = THIS_MODULE,
		.of_match_table = ar0261_dt_match,
	},
};



#ifdef SET_ALT_I2C_ADDRESS
/* This is essentially msm_sensor_enable/disable_i2c_mux highjacked */
static int32_t msm_sensor_enable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_INIT, NULL);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_CFG, (void *)&i2c_conf->i2c_mux_mode);
	return 0;
}

static int32_t msm_sensor_disable_i2c_mux(struct msm_camera_i2c_conf *i2c_conf)
{
	struct v4l2_subdev *i2c_mux_sd =
		dev_get_drvdata(&i2c_conf->mux_dev->dev);
	v4l2_subdev_call(i2c_mux_sd, core, ioctl,
		VIDIOC_MSM_I2C_MUX_RELEASE, NULL);
	return 0;
}
#endif

/* ar0261 match_id function */
int32_t ar0261_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	int32_t retry = 0;

#ifdef SET_ALT_I2C_ADDRESS
	/* to hack in alternate I2C address */
	/* Set slave address to primary */
	ar0261_s_ctrl.sensordata->slave_info->sensor_slave_addr =
		PRI_I2C_ADDRESS;
	ar0261_s_ctrl.sensor_i2c_client->cci_client->sid =
		ar0261_s_ctrl.sensordata->slave_info->sensor_slave_addr >> 1;

	/* Unlock sensor GPIO configuration */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x301a,
			0x0110, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %d I2C address configure failed!\n",
			__func__, __LINE__);
	}
	/* Change sensor to use alternate I2C address */
	/* via GPIO 0 */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3026,
			0x0FF83, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: %d I2C address configure failed!\n",
			__func__, __LINE__);
	}

	/* Set slave address to alternate */
	ar0261_s_ctrl.sensordata->slave_info->sensor_slave_addr =
		ALT_I2C_ADDRESS;
	ar0261_s_ctrl.sensor_i2c_client->cci_client->sid =
		ar0261_s_ctrl.sensordata->slave_info->sensor_slave_addr >> 1;
	/* End of added code */
#endif

	for (retry = 0; retry < 3; retry++) {
		rc = msm_sensor_match_id(s_ctrl);
		if (rc < 0) {
			pr_err("%s:%d match id failed rc %d,retry = %d\n",
						__func__, __LINE__, rc , retry);
			if (retry < 2)
				continue;
			else
				break;
		} else {
			break;
		}
	}

	if (rc >= 0)
		pr_info("%s success with slave addr 0x%x\n", __func__,
				s_ctrl->sensor_i2c_client->cci_client->sid);

	return rc;
}

static int32_t ar0261_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
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

	/* Set initial values for gpios */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_LOW);
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[2].gpio,
			GPIO_OUT_LOW);
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_LOW);

	/* Enable VDDIO supply */
	rc = msm_camera_config_single_vreg(dev, &info->cam_vreg[1],
			&ar0261_cam_vddio, VREG_ON);
	if (rc < 0) {
		pr_err("%s: Unable to turn on VDDIO supply!\n", __func__);
		goto abort1;
	}
	usleep_range(5000, 6000);

	/* If using aptina dev board, enable regulator l10 */
	if (ar0261_devboard_config == true) {
		rc = msm_camera_config_single_vreg(dev, &info->cam_vreg[2],
			&ar0261_cam_vana, VREG_ON);
		if (rc < 0) {
			pr_err("%s: Unable to turn on VDDIO supply!\n",
					__func__);
			goto abort2;
		}
		usleep_range(5000, 6000);
	}

	/* Enable AVDD */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[2].gpio,
			GPIO_OUT_HIGH);
	usleep_range(5000, 6000);

	/* enable Core Supply (VDD) from PMIC */
	rc = msm_camera_config_single_vreg(dev, &info->cam_vreg[0],
			&ar0261_cam_vdd, VREG_ON);
	if (rc < 0) {
		pr_err("%s: Unable to turn on VDD supply!\n", __func__);
		goto abort3;
	}
	usleep_range(5000, 6000);

	/* Enable load switch */
	gpio_set_value_cansleep(
			info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_HIGH);
	usleep_range(1000, 2000);

	/* Enable MCLK */
	rc = msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
			(struct clk **)&ar0261_cam_mclk[0],
			s_ctrl->clk_info_size, CLK_ON);
	if (rc < 0) {
		pr_err("%s: Unable to enable MCLK!\n", __func__);
		goto abort4;
	}
	usleep_range(18000, 19000);

	/* Set reset to normal mode */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_HIGH);
	usleep_range(4000, 5000);

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
				s_ctrl->sensor_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto abort5;
		}
	}

	rc = ar0261_match_id(s_ctrl);
	if (rc < 0) {
		pr_err("%s:%d match id failed rc %d\n", __func__, __LINE__, rc);
		goto abort6;
	}

	goto power_up_done;
abort6:
	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}

abort5:
	msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
			(struct clk **)&ar0261_cam_mclk[0],
			s_ctrl->clk_info_size, CLK_OFF);
	gpio_set_value_cansleep(
			info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_LOW);

abort4:
	gpio_set_value_cansleep(
			info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_LOW);
	msm_camera_config_single_vreg(dev, &info->cam_vreg[0],
			&ar0261_cam_vdd, VREG_OFF);

abort3:
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[2].gpio,
			GPIO_OUT_LOW);
	if (ar0261_devboard_config == true)
		msm_camera_config_single_vreg(dev, &info->cam_vreg[2],
			&ar0261_cam_vana, VREG_OFF);

abort2:
	msm_camera_config_single_vreg(dev, &info->cam_vreg[1],
			&ar0261_cam_vddio, VREG_OFF);

abort1:
	msm_camera_request_gpio_table(
			info->gpio_conf->cam_gpio_req_tbl,
			info->gpio_conf->cam_gpio_req_tbl_size,
			GPIO_REQUEST_NO_USE);

power_up_done:
	return rc;
}

static int32_t ar0261_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_board_info *info = s_ctrl->sensordata;
	struct device *dev = s_ctrl->dev;

	pr_debug("%s: Enter\n", __func__);

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}

	/* Disable MCLK */
	msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
			(struct clk **)&ar0261_cam_mclk[0],
			s_ctrl->clk_info_size, CLK_OFF);
	usleep_range(5000, 6000);

	/* reset low */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_LOW);
	usleep_range(5000, 6000);

	/* Disable VDD */
	gpio_set_value_cansleep(
			info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_LOW);

	msm_camera_config_single_vreg(dev, &info->cam_vreg[0],
			&ar0261_cam_vdd, VREG_OFF);
	usleep_range(5000, 6000);

	/* Disable AVDD */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[2].gpio,
			GPIO_OUT_LOW);
	usleep_range(5000, 6000);

	/* If using devboard, turn off l10 */
	if (ar0261_devboard_config == true) {
		msm_camera_config_single_vreg(dev, &info->cam_vreg[2],
			&ar0261_cam_vana, VREG_OFF);
		usleep_range(1000, 2000);
	}

	msm_camera_config_single_vreg(dev, &info->cam_vreg[1],
			&ar0261_cam_vddio, VREG_OFF);
	usleep_range(1000, 2000);

	msm_camera_request_gpio_table(
		info->gpio_conf->cam_gpio_req_tbl,
		info->gpio_conf->cam_gpio_req_tbl_size, GPIO_REQUEST_NO_USE);

	return rc;
}

static int32_t ar0261_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	match = of_match_device(ar0261_dt_match, &pdev->dev);
	if (match) {
		if (of_property_read_bool(pdev->dev.of_node,
					"qcom,cam-nondirect") == true)
			/* Devboard is used */
			ar0261_devboard_config = true;

		rc = msm_sensor_platform_probe(pdev, match->data);
	} else {
		pr_err("%s:%d failed match device\n", __func__, __LINE__);
		return -EINVAL;
	}

	return rc;
}

static int __init ar0261_init_module(void)
{
	int32_t rc = 0;
	rc = platform_driver_probe(&ar0261_platform_driver,
		ar0261_platform_probe);
	if (rc < 0)
		return rc;

	return i2c_add_driver(&ar0261_i2c_driver);
}

static void __exit ar0261_exit_module(void)
{
	if (ar0261_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&ar0261_s_ctrl);
		platform_driver_unregister(&ar0261_platform_driver);
	} else
		i2c_del_driver(&ar0261_i2c_driver);
	return;
}

static struct msm_sensor_fn_t ar0261_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = ar0261_sensor_power_up,
	.sensor_power_down = ar0261_sensor_power_down,
	.sensor_match_id = ar0261_match_id,
};

static struct msm_sensor_ctrl_t ar0261_s_ctrl = {
	.sensor_i2c_client = &ar0261_sensor_i2c_client,
	.msm_sensor_mutex = &ar0261_mut,
	.sensor_v4l2_subdev_info = ar0261_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ar0261_subdev_info),
	.func_tbl = &ar0261_func_tbl,
};

module_init(ar0261_init_module);
module_exit(ar0261_exit_module);
MODULE_DESCRIPTION("ar0261");
MODULE_LICENSE("GPL v2");
