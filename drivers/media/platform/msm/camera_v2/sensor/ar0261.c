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
#define AR0261_SENSOR_NAME "ar0261"

/*
 * Highjack the sensor_power_down routine
 * in order to implement alternate I2C
 * address selection
 */
/*
#define SET_ALT_I2C_ADDRESS
 */
#define CONFIG_MSMB_CAMERA_DEBUG 1
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#define ALT_I2C_ADDRESS 0x6E
#define PRI_I2C_ADDRESS 0x6C

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

DEFINE_MSM_MUTEX(ar0261_mut);

static struct msm_sensor_ctrl_t ar0261_s_ctrl;

/* Power up sequencing if Aptina devboard is used */
static struct msm_sensor_power_setting ar0261_power_setting_with_devboard[] = {
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_RESET,
		.config_val	= GPIO_OUT_LOW,
		.delay		= 1,
	},
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_VDIG,
		.config_val	= GPIO_OUT_LOW,
		.delay		= 1,
	},
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_VANA,
		.config_val	= GPIO_OUT_LOW,
		.delay		= 1,
	},
	{
		.seq_type	= SENSOR_VREG,
		.seq_val	= CAM_VIO,
		.config_val	= 0,
		.delay		= 5,
	},
	/* ar0261 dev boards needs regulator l10 */
	{
		.seq_type	= SENSOR_VREG,
		.seq_val	= CAM_VANA,
		.config_val	= 0,
		.delay		= 5,
	},
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_VANA,
		.config_val	= GPIO_OUT_HIGH,
		.delay		= 5,
	},
	{
		.seq_type	= SENSOR_VREG,
		.seq_val	= CAM_VDIG,
		.config_val	= 0,
		.delay		= 1,
	},
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_VDIG,
		.config_val	= GPIO_OUT_HIGH,
		.delay		= 5,
	},
	{
		.seq_type	= SENSOR_CLK,
		.seq_val	= SENSOR_CAM_MCLK,
		.config_val	= 24000000,
		.delay		= 15,
	},
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_RESET,
		.config_val	= GPIO_OUT_HIGH,
		.delay		= 20,
	},
};

/* Power up sequencing if Aptina devboard is not used */
static struct msm_sensor_power_setting ar0261_power_setting[] = {
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_RESET,
		.config_val	= GPIO_OUT_LOW,
		.delay		= 1,
	},
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_VDIG,
		.config_val	= GPIO_OUT_LOW,
		.delay		= 1,
	},
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_VANA,
		.config_val	= GPIO_OUT_LOW,
		.delay		= 1,
	},
	{
		.seq_type	= SENSOR_VREG,
		.seq_val	= CAM_VIO,
		.config_val	= 0,
		.delay		= 5,
	},
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_VANA,
		.config_val	= GPIO_OUT_HIGH,
		.delay		= 5,
	},
	{
		.seq_type	= SENSOR_VREG,
		.seq_val	= CAM_VDIG,
		.config_val	= 0,
		.delay		= 1,
	},
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_VDIG,
		.config_val	= GPIO_OUT_HIGH,
		.delay		= 5,
	},
	{
		.seq_type	= SENSOR_CLK,
		.seq_val	= SENSOR_CAM_MCLK,
		.config_val	= 24000000,
		.delay		= 15,
	},
	{
		.seq_type	= SENSOR_GPIO,
		.seq_val	= SENSOR_GPIO_RESET,
		.config_val	= GPIO_OUT_HIGH,
		.delay		= 20,
	},
};

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
		pr_info("%s: %d I2C address configure failed!\n",
			__func__, __LINE__);
	}
	/* Change sensor to use alternate I2C address */
	/* via GPIO 0 */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			0x3026,
			0x0FF83, MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_info("%s: %d I2C address configure failed!\n",
			__func__, __LINE__);
	}

	/* Set slave address to alternate */
	ar0261_s_ctrl.sensordata->slave_info->sensor_slave_addr =
		ALT_I2C_ADDRESS;
	ar0261_s_ctrl.sensor_i2c_client->cci_client->sid =
		ar0261_s_ctrl.sensordata->slave_info->sensor_slave_addr >> 1;
	/* End of added code */
#endif

	rc = msm_sensor_match_id(s_ctrl);

	if (rc < 0)
		pr_err("%s:%d match id failed rc %d\n", __func__, __LINE__, rc);
	else
		pr_info("%s success\n", __func__);

	CDBG("%s exit\n", __func__);
	return rc;
}


static int32_t ar0261_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	struct msm_sensor_ctrl_t *s_ctrl;

	match = of_match_device(ar0261_dt_match, &pdev->dev);
	if (match) {
		if (of_property_read_bool(pdev->dev.of_node, "qcom,cam-nondirect") == true) {
			/* Devboard is used.  Replace the power up sequence */
			s_ctrl = (struct msm_sensor_ctrl_t *)match->data;
			s_ctrl->power_setting_array.power_setting =
				ar0261_power_setting_with_devboard;
			s_ctrl->power_setting_array.size =
				ARRAY_SIZE(ar0261_power_setting_with_devboard);
		}

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
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = ar0261_match_id,
};

static struct msm_sensor_ctrl_t ar0261_s_ctrl = {
	.sensor_i2c_client = &ar0261_sensor_i2c_client,
	.power_setting_array.power_setting = ar0261_power_setting,
	.power_setting_array.size = ARRAY_SIZE(ar0261_power_setting),
	.msm_sensor_mutex = &ar0261_mut,
	.sensor_v4l2_subdev_info = ar0261_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(ar0261_subdev_info),
	.func_tbl = &ar0261_func_tbl,
};

module_init(ar0261_init_module);
module_exit(ar0261_exit_module);
MODULE_DESCRIPTION("ar0261");
MODULE_LICENSE("GPL v2");
