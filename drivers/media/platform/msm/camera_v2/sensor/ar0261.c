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

#define AR0261_OTP_BASE_PAGE 0x32
#define AR0261_OTP_NUM_PAGES 3
#define AR0261_OTP_PAGE_SIZE 50
#define AR0261_RESET_REG 0x301A
#define AR0261_OTPM_CONTROL_REG 0x304A
#define AR0261_OTPM_RECORD_REG 0x304C
#define AR0261_OTPM_DATA_BASE_REG 0x3800
#define AR0261_GPIO_CONTROL_REG 0x30F8

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

static uint8_t ar0261_reverse_byte(uint8_t data)
{
	return ((data * 0x0802LU & 0x22110LU) |
			(data * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
}

static int32_t ar0261_check_crc(uint8_t *otp_data)
{
	int32_t crc_match = -1;
	uint16_t crc = 0x0000;
	uint16_t crc_reverse = 0x0000;
	int i, j;
	uint32_t tmp;
	uint32_t tmp_reverse;
	uint16_t otp_crc = otp_data[AR0261_OTP_PAGE_SIZE - 2] << 8 |
		otp_data[AR0261_OTP_PAGE_SIZE - 1];

	/* Calculate both methods of CRC since integrators differ on
	 * how CRC should be calculated. */
	for (i = 0; i < (AR0261_OTP_PAGE_SIZE - 2); i++) {
		tmp_reverse = ar0261_reverse_byte(otp_data[i]);
		tmp = otp_data[i] & 0xff;
		for (j = 0; j < 8; j++) {
			if (((crc & 0x8000) >> 8) ^ (tmp & 0x80))
				crc = (crc << 1) ^ 0x8005;
			else
				crc = crc << 1;
			tmp <<= 1;

			if (((crc_reverse & 0x8000) >> 8) ^
					(tmp_reverse & 0x80))
				crc_reverse = (crc_reverse << 1) ^ 0x8005;
			else
				crc_reverse = crc_reverse << 1;

			tmp_reverse <<= 1;
		}
	}

	crc_reverse = (ar0261_reverse_byte(crc_reverse) << 8) |
		ar0261_reverse_byte(crc_reverse >> 8);

	if (crc == otp_crc || crc_reverse == otp_crc)
		crc_match = 0;

	pr_debug("%s: OTP_CRC %x CALC CRC %x CALC Reverse CRC %x matches? %d\n"
			, __func__, otp_crc, crc, crc_reverse, crc_match);

	return crc_match;
}

static void ar0261_read_otp(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t otp_page = AR0261_OTP_BASE_PAGE;
	int i;
	int poll_times;
	uint16_t otpm_control;
	uint8_t *otp_data_ptr;

	if (s_ctrl->sensor_otp.otp_read)
		return;

	otp_data_ptr = s_ctrl->sensor_otp.otp_info;

	/* Enable Streaming */
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			AR0261_RESET_REG, 0x021C,
			MSM_CAMERA_I2C_WORD_DATA);
	if (rc < 0) {
		pr_err("%s: Unable to streamon the sensor!\n", __func__);
		goto exit0;
	}

	/* Work in reverse order of the pages to find the correct data */
	for (i = AR0261_OTP_NUM_PAGES; i > 0; i--) {
		/* Set Page Index */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
				s_ctrl->sensor_i2c_client,
				AR0261_OTPM_RECORD_REG, (otp_page << 8),
				MSM_CAMERA_I2C_WORD_DATA);
		otp_page -= 0x1;
		if (rc < 0) {
			pr_err("%s: Unable to write OTP record reg!\n",
					__func__);
			goto exit1;
		}

		/* Issue Read of OTP memory to put memory into the OTP data
		 * registers of AR0261 */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
				s_ctrl->sensor_i2c_client,
				AR0261_OTPM_CONTROL_REG, 0x0010,
				MSM_CAMERA_I2C_WORD_DATA);
		if (rc < 0) {
			pr_err("%s: Unable to write OTP control reg read!\n",
					__func__);
			goto exit1;
		}

		/* Poll to see if read is complete in the internal memory */
		poll_times = 0;
		while (poll_times < 3) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
					s_ctrl->sensor_i2c_client,
					AR0261_OTPM_CONTROL_REG, &otpm_control,
					MSM_CAMERA_I2C_WORD_DATA);
			if (rc < 0) {
				pr_err("%s: Unable to read OTPM_CONTROL reg!\n",
						__func__);
				goto exit1;
			}

			if (0x20 == (otpm_control & 0x20))
				break;

			usleep_range(2000, 3000);
			poll_times++;
		}

		/* Check to see if read was successful so that we can
		 * officially read the OTP data over the i2c bus */
		if (0x40 == (otpm_control & 0x40)) {
			rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
				i2c_read_seq(s_ctrl->sensor_i2c_client,
						AR0261_OTPM_DATA_BASE_REG,
						otp_data_ptr,
						AR0261_OTP_PAGE_SIZE);
			if (rc < 0) {
				pr_err("%s: Unable to read otp data over i2c!\n"
						, __func__);
				goto exit1;
			}

			/* check crc */
			rc = ar0261_check_crc(otp_data_ptr);
			if (rc < 0) {
				pr_warn("%s: CRC failed on page %x!\n",
						__func__, (otp_page + 0x1));
				s_ctrl->sensor_otp.otp_crc_pass = 0;
			} else {
				s_ctrl->sensor_otp.otp_crc_pass = 1;
				break;
			}
		} else
			pr_warn("%s: OTP Read not successful, on page %x.\n",
					__func__, (otp_page + 0x1));

	}

	s_ctrl->sensor_otp.otp_read = 1;
exit1:
	/* Issue Stream off */
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			AR0261_RESET_REG, 0x0218,
			MSM_CAMERA_I2C_WORD_DATA);

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
			s_ctrl->sensor_i2c_client,
			AR0261_GPIO_CONTROL_REG, 0x0330,
			MSM_CAMERA_I2C_WORD_DATA);
exit0:
	return;
}

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

	if (rc >= 0) {
		pr_info("%s success with slave addr 0x%x\n", __func__,
				s_ctrl->sensor_i2c_client->cci_client->sid);
		ar0261_read_otp(s_ctrl);
	}

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

		ar0261_s_ctrl.sensor_otp.otp_info = devm_kzalloc(&pdev->dev,
				AR0261_OTP_PAGE_SIZE, GFP_KERNEL);
		if (ar0261_s_ctrl.sensor_otp.otp_info == NULL) {
			pr_err("%s: Unable to allocate memory for OTP!\n",
					__func__);
			return -ENOMEM;
		}

		ar0261_s_ctrl.sensor_otp.size = AR0261_OTP_PAGE_SIZE;
		rc = msm_sensor_platform_probe(pdev, match->data);
	} else {
		pr_err("%s:%d failed match device\n", __func__, __LINE__);
		return -EINVAL;
	}

	return rc;
}

static int32_t ar0261_get_module_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	/* Place holder so that data is transferred to user space */
	return 0;
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
	.sensor_get_module_info = ar0261_get_module_info,
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
