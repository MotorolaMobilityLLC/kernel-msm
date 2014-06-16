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

#define IMX135_SENSOR_NAME "imx135"
#define IMX135_PRIMARY_I2C_ADDRESS 0x34
#define IMX135_SECONDARY_I2C_ADDRESS 0x20
DEFINE_MSM_MUTEX(imx135_mut);

static struct msm_sensor_ctrl_t imx135_s_ctrl;

#define IMX135_EEPROM_I2C_ADDR 0xA0
#define IMX135_EEPROM_PAGE_SIZE 256
#define IMX135_EEPROM_NUM_PAGES 8
#define IMX135_EEPROM_SIZE   (IMX135_EEPROM_NUM_PAGES * IMX135_EEPROM_PAGE_SIZE)
static bool eeprom_read;

static struct regulator *imx135_cam_vdd;
static struct regulator *imx135_cam_vddio;
static struct regulator *imx135_cam_vaf;
struct clk *imx135_cam_mclk[2];

static bool imx135_devboard_config;

#define VREG_ON              1
#define VREG_OFF             0
#define GPIO_REQUEST_USE     1
#define GPIO_REQUEST_NO_USE  0
#define CLK_ON               1
#define CLK_OFF              0

/* This list of enum has to match what's defined in dtsi file */
enum msm_sensor_imx135_vreg_t {
	IMX135_CAM_VIO,
	IMX135_CAM_VDIG,
	IMX135_CAM_VAF,
};

static struct v4l2_subdev_info imx135_subdev_info[] = {
	{
		.code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt = 1,
		.order = 0,
	},
};

static const struct i2c_device_id imx135_i2c_id[] = {
	{IMX135_SENSOR_NAME, (kernel_ulong_t)&imx135_s_ctrl},
	{ }
};

static int32_t msm_imx135_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &imx135_s_ctrl);
}

static struct i2c_driver imx135_i2c_driver = {
	.id_table = imx135_i2c_id,
	.probe  = msm_imx135_i2c_probe,
	.driver = {
		.name = IMX135_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client imx135_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id imx135_dt_match[] = {
	{.compatible = "qcom,imx135", .data = &imx135_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, imx135_dt_match);

static struct platform_driver imx135_platform_driver = {
	.driver = {
		.name = "qcom,imx135",
		.owner = THIS_MODULE,
		.of_match_table = imx135_dt_match,
	},
};

static int32_t imx135_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;
	match = of_match_device(imx135_dt_match, &pdev->dev);

	if (match) {
		if (of_property_read_bool(pdev->dev.of_node,
					"qcom,mot-cam-usedevboard")
				== true)
			imx135_devboard_config = true;

		imx135_s_ctrl.sensor_otp.otp_info = devm_kzalloc(&pdev->dev,
				IMX135_EEPROM_SIZE, GFP_KERNEL);
		if (imx135_s_ctrl.sensor_otp.otp_info == NULL) {
			pr_err("%s: Unable to allocate memory for EEPROM!\n",
					__func__);
			return -ENOMEM;
		}

		imx135_s_ctrl.sensor_otp.size = IMX135_EEPROM_SIZE;

		rc = msm_sensor_platform_probe(pdev, match->data);
	} else {
		pr_err("%s: Failed to match device tree!\n", __func__);
		rc = -EINVAL;
	}

	return rc;
}

static void imx135_read_eeprom(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	int i;
	uint16_t eeprom_slave_addr;
	uint8_t *eeprom_ptr;
	uint16_t cam_slave_addr =
		s_ctrl->sensordata->slave_info->sensor_slave_addr;
	int cam_addr_type = s_ctrl->sensor_i2c_client->addr_type;

	if (eeprom_read)
		return;

	s_ctrl->sensor_i2c_client->addr_type = MSM_CAMERA_I2C_BYTE_ADDR;
	eeprom_ptr = s_ctrl->sensor_otp.otp_info;

	for (i = 0; i < IMX135_EEPROM_NUM_PAGES; i++) {
		eeprom_slave_addr = IMX135_EEPROM_I2C_ADDR + i * 2;
		s_ctrl->sensordata->slave_info->sensor_slave_addr =
			eeprom_slave_addr;
		s_ctrl->sensor_i2c_client->cci_client->sid =
			(eeprom_slave_addr >> 1);

		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read_seq(
				s_ctrl->sensor_i2c_client,
				0x00,
				eeprom_ptr,
				IMX135_EEPROM_PAGE_SIZE);
		if (rc < 0) {
			pr_err("%s: Unable to read page: %d, status %d\n",
					__func__, i, rc);
			goto exit;
		}
		eeprom_ptr += IMX135_EEPROM_PAGE_SIZE;
	}

	eeprom_read = true;

exit:
	/* Restore sensor defaults */
	s_ctrl->sensordata->slave_info->sensor_slave_addr = cam_slave_addr;
	s_ctrl->sensor_i2c_client->cci_client->sid = (cam_slave_addr >> 1);
	s_ctrl->sensor_i2c_client->addr_type = cam_addr_type;
	return;
}

static int32_t imx135_sensor_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;
	uint16_t hw_rev = 0;

	s_ctrl->sensordata->slave_info->sensor_slave_addr =
		IMX135_PRIMARY_I2C_ADDRESS;
	s_ctrl->sensor_i2c_client->cci_client->sid =
		(IMX135_PRIMARY_I2C_ADDRESS >> 1);
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc >= 0)
		goto check_chipid;

	s_ctrl->sensordata->slave_info->sensor_slave_addr =
		IMX135_SECONDARY_I2C_ADDRESS;
	s_ctrl->sensor_i2c_client->cci_client->sid =
		(IMX135_SECONDARY_I2C_ADDRESS >> 1);
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			s_ctrl->sensordata->slave_info->sensor_id_reg_addr,
			&chipid,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: Unable to read chip id!\n", __func__);
		return rc;
	}

check_chipid:
	if (chipid != s_ctrl->sensordata->slave_info->sensor_id) {
		pr_err("%s: chip id %x does not match expected %x\n", __func__,
				chipid, s_ctrl->sensordata->
				slave_info->sensor_id);
		return -ENODEV;
	}

	imx135_read_eeprom(s_ctrl);

	if (s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				0x0018,
				&hw_rev,
				MSM_CAMERA_I2C_BYTE_DATA) < 0)
		pr_err("%s: Unable to read hw rev of camera!\n", __func__);

	s_ctrl->sensor_otp.hw_rev = hw_rev;
	pr_info("%s: success with slave addr 0x%x and hw rev 0x%x!\n", __func__,
			s_ctrl->sensor_i2c_client->cci_client->sid,
			s_ctrl->sensor_otp.hw_rev);
	return rc;
}

static int32_t imx135_sensor_power_down_devboard(
		struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_board_info *info = s_ctrl->sensordata;
	struct device *dev = s_ctrl->dev;

	pr_debug("%s: Enter\n", __func__);

	/* Disable MCLK */
	msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
			(struct clk **)&imx135_cam_mclk[0],
			s_ctrl->clk_info_size, CLK_OFF);
	usleep_range(500, 600);

	/* Put into Reset State */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[2].gpio,
			GPIO_OUT_LOW);
	usleep_range(5000, 6000);

	/* Disable VAF */
	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VAF],
			&imx135_cam_vaf, VREG_OFF);

	/* Disable Avdd */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_LOW);
	usleep_range(1000, 2000);

	/* Disable VDIG */
	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VDIG],
			&imx135_cam_vdd, VREG_OFF);
	usleep_range(1000, 2000);

	/* Disable VDDIO */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_LOW);

	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VIO],
			&imx135_cam_vddio, VREG_OFF);
	usleep_range(1000, 2000);

	/* Disable GPIO's */
	msm_camera_request_gpio_table(
		info->gpio_conf->cam_gpio_req_tbl,
		info->gpio_conf->cam_gpio_req_tbl_size, GPIO_REQUEST_NO_USE);

	return rc;
}

static int32_t imx135_sensor_power_down(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_board_info *info = s_ctrl->sensordata;
	struct device *dev = s_ctrl->dev;

	pr_debug("%s: Enter\n", __func__);

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}

	if (imx135_devboard_config == true)
		return imx135_sensor_power_down_devboard(s_ctrl);

	/* Disable MCLK */
	msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
			(struct clk **)&imx135_cam_mclk[0],
			s_ctrl->clk_info_size, CLK_OFF);
	usleep_range(500, 600);

	/* Set reset low */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[4].gpio,
			GPIO_OUT_LOW);
	usleep_range(1000, 2000);

	/* Disable VAF */
	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VAF],
			&imx135_cam_vaf, VREG_OFF);

	/* Disable AVDD */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_LOW);
	usleep_range(1000, 2000);

	/* Disable VDD_EN */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_LOW);
	usleep_range(500, 600);

	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VDIG],
			&imx135_cam_vdd, VREG_OFF);
	usleep_range(1000, 2000);

	/* Disable I/O Supply(Vddio) */
	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VIO],
			&imx135_cam_vddio, VREG_OFF);
	usleep_range(1000, 2000);

	msm_camera_request_gpio_table(
		info->gpio_conf->cam_gpio_req_tbl,
		info->gpio_conf->cam_gpio_req_tbl_size, GPIO_REQUEST_NO_USE);
	return rc;
}

static int32_t imx135_sensor_power_up_devboard(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_board_info *info = s_ctrl->sensordata;
	struct device *dev = s_ctrl->dev;

	pr_debug("%s: Enter\n", __func__);

	/* Initialize gpios low state */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_LOW);
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[2].gpio,
			GPIO_OUT_LOW);
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_LOW);
	usleep_range(1000, 2000);

	/* Enable Core supply (VDD) */
	rc = msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VDIG],
			&imx135_cam_vdd, VREG_ON);
	if (rc < 0) {
		pr_err("%s: Unable to turn on VDD (%d)\n", __func__, rc);
		goto abort0;
	}

	/* Enable I/O Supply(Vddio) */
	rc = msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VIO],
			&imx135_cam_vddio, VREG_ON);
	if (rc < 0) {
		pr_err("%s: Unable to turn on VDDIO (%d)\n", __func__, rc);
		goto abort1;
	}
	usleep_range(1000, 2000);

	/* Enable Switch to send DVDD to sensor */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_HIGH);
	usleep_range(1000, 2000);

	/* Enable AVDD LDO */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_HIGH);
	usleep_range(1000, 2000);

	/* Enable AF Supply */
	rc = msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VAF],
			&imx135_cam_vaf, VREG_ON);
	if (rc < 0) {
		pr_err("%s: Unable to turn on VAF (%d)\n", __func__, rc);
		goto abort2;
	}
	usleep_range(1000, 2000);

	/* Enable MCLK */
	s_ctrl->clk_info[0].clk_rate = 24000000;
	rc = msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
			(struct clk **)&imx135_cam_mclk[0],
			s_ctrl->clk_info_size, CLK_ON);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto abort3;
	}
	usleep_range(2000, 3000);

	/* Initiate a Reset */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[2].gpio,
			GPIO_OUT_HIGH);
	usleep_range(30000, 31000);

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto abort4;
		}
	}

	rc = imx135_sensor_match_id(s_ctrl);
	if (rc < 0) {
		pr_err("%s: match id failed!\n", __func__);
		goto abort5;
	}

	goto power_up_done;

abort5:
	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}

abort4:
	msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
			(struct clk **)&imx135_cam_mclk[0],
			s_ctrl->clk_info_size, CLK_OFF);
	usleep_range(1000, 1500);

	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[2].gpio,
			GPIO_OUT_LOW);
	usleep_range(1000, 1500);

abort3:
	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VAF],
			&imx135_cam_vaf, VREG_OFF);

abort2:
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_LOW);

	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_LOW);

	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VIO],
			&imx135_cam_vddio, VREG_OFF);

abort1:
	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VDIG],
			&imx135_cam_vdd, VREG_OFF);

abort0:
	msm_camera_request_gpio_table(
			info->gpio_conf->cam_gpio_req_tbl,
			info->gpio_conf->cam_gpio_req_tbl_size,
			GPIO_REQUEST_NO_USE);
power_up_done:
	return rc;
}

static int32_t imx135_sensor_power_up(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	struct msm_camera_sensor_board_info *info = s_ctrl->sensordata;
	struct device *dev = s_ctrl->dev;

	pr_debug("%s: Enter\n", __func__);

	if (info->gpio_conf->cam_gpiomux_conf_tbl != NULL) {
		msm_gpiomux_install(
				(struct msm_gpiomux_config *)
				info->gpio_conf->cam_gpiomux_conf_tbl,
				info->gpio_conf->cam_gpiomux_conf_tbl_size);
	}

	rc = msm_camera_request_gpio_table(
			info->gpio_conf->cam_gpio_req_tbl,
			info->gpio_conf->cam_gpio_req_tbl_size,
			GPIO_REQUEST_USE);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	if (imx135_devboard_config == true)
		return imx135_sensor_power_up_devboard(s_ctrl);

	/* Initialize gpios */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[2].gpio,
			GPIO_OUT_LOW);
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_LOW);
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[4].gpio,
			GPIO_OUT_LOW);

	/* Enable VDDIO Supply */
	rc = msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VIO],
			&imx135_cam_vddio, VREG_ON);
	if (rc < 0) {
		pr_err("%s: Unable to turn on VDDIO (%d)\n", __func__, rc);
		goto abort0;
	}
	usleep_range(1000, 2000);

	/* Enable Core supply (VDD) and enable load switch */
	rc = msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VDIG],
			&imx135_cam_vdd, VREG_ON);
	if (rc < 0) {
		pr_err("%s: Unable to turn on VDD (%d)\n", __func__, rc);
		goto abort1;
	}
	usleep_range(1000, 2000);

	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_HIGH);
	usleep_range(1000, 2000);

	/* Enable AVDD supply */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_HIGH);
	usleep_range(1000, 2000);

	/* Enable AF Supply (VAF) */
	rc = msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VAF],
			&imx135_cam_vaf, VREG_ON);
	if (rc < 0) {
		pr_err("%s: Unable to turn on VAF (%d)\n", __func__, rc);
		goto abort2;
	}
	usleep_range(1000, 2000);

	/* Enable MCLK */
	s_ctrl->clk_info[0].clk_rate = 24000000;
	rc = msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
				(struct clk **)&imx135_cam_mclk[0],
				s_ctrl->clk_info_size, CLK_ON);
	if (rc < 0) {
		pr_err("%s: clk enable failed\n", __func__);
		goto abort3;
	}
	usleep_range(2000, 3000);

	/* Initiate a Reset */
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[4].gpio,
			GPIO_OUT_HIGH);
	usleep_range(30000, 31000);

	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_INIT);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
			goto abort4;
		}
	}

	rc = imx135_sensor_match_id(s_ctrl);
	if (rc < 0) {
		pr_err("%s: match id failed!\n", __func__);
		goto abort5;
	}

	goto power_up_done;
abort5:
	if (s_ctrl->sensor_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_util(
			s_ctrl->sensor_i2c_client, MSM_CCI_RELEASE);
	}

abort4:
	msm_cam_clk_enable(dev, &s_ctrl->clk_info[0],
			(struct clk **)&imx135_cam_mclk[0],
			s_ctrl->clk_info_size, CLK_OFF);
	usleep_range(1000, 1500);

	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[2].gpio,
			GPIO_OUT_LOW);
	usleep_range(1000, 1500);

abort3:
	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VAF],
			&imx135_cam_vaf, VREG_OFF);

abort2:
	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[1].gpio,
			GPIO_OUT_LOW);

	gpio_set_value_cansleep(info->gpio_conf->cam_gpio_req_tbl[3].gpio,
			GPIO_OUT_LOW);

	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VDIG],
			&imx135_cam_vdd, VREG_OFF);

abort1:
	msm_camera_config_single_vreg(dev,
			&info->cam_vreg[IMX135_CAM_VIO],
			&imx135_cam_vddio, VREG_OFF);

abort0:
	msm_camera_request_gpio_table(
			info->gpio_conf->cam_gpio_req_tbl,
			info->gpio_conf->cam_gpio_req_tbl_size,
			GPIO_REQUEST_NO_USE);

power_up_done:
	return rc;
}

static int32_t imx135_get_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	/* Place holder so that data is transferred to user space */
	return 0;
}

static int __init imx135_init_module(void)
{
	int32_t rc = 0;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&imx135_platform_driver,
		imx135_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&imx135_i2c_driver);
}

static void __exit imx135_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (imx135_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&imx135_s_ctrl);
		platform_driver_unregister(&imx135_platform_driver);
	} else
		i2c_del_driver(&imx135_i2c_driver);
	return;
}

static struct msm_sensor_fn_t imx135_func_tbl = {
	.sensor_config = msm_sensor_config,
	.sensor_power_up = imx135_sensor_power_up,
	.sensor_power_down = imx135_sensor_power_down,
	.sensor_match_id = imx135_sensor_match_id,
	.sensor_get_module_info = imx135_get_module_info,
};

static struct msm_sensor_ctrl_t imx135_s_ctrl = {
	.sensor_i2c_client = &imx135_sensor_i2c_client,
	.msm_sensor_mutex = &imx135_mut,
	.sensor_v4l2_subdev_info = imx135_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(imx135_subdev_info),
	.func_tbl = &imx135_func_tbl,
};

module_init(imx135_init_module);
module_exit(imx135_exit_module);
MODULE_DESCRIPTION("imx135");
MODULE_LICENSE("GPL v2");
