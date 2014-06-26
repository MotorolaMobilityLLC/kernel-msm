/*
 * platform_imx208.c: imx208 platform data initialization file
 *
 * (C) Copyright 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/atomisp_platform.h>
#include <linux/regulator/consumer.h>
#include <asm/intel_scu_ipcutil.h>
#include <asm/intel-mid.h>
#include <media/v4l2-subdev.h>
#include "platform_camera.h"
#include "platform_imx208.h"

static int camera_reset = -1;

static int camera_vprog1_on;
static struct regulator *vprog1_reg;
#define VPROG1_VAL 2800000

/*
 * MOFD VV secondary camera sensor - imx208 platform data
 */

static int is_moorefield(void)
{
	return INTEL_MID_BOARD(1, PHONE, MOFD);
}

static int imx208_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;
	if (camera_reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_1_RESET,
					GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;
		camera_reset = ret;
	}

	if (flag) {
		gpio_set_value(camera_reset, 1);

		/* imx208 initializing time - t1+t2
		 * 427us(t1) - 8192 mclk(19.2Mhz) before sccb communication
		 * 1ms(t2) - sccb stable time when using internal dvdd
		 */
		usleep_range(1500, 1500);
	} else {
		gpio_set_value(camera_reset, 0);
	}

	return 0;
}

static int imx208_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
#ifdef CONFIG_INTEL_SCU_IPC_UTIL
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM1, flag ? clock_khz : 0);
#else
	pr_err("imx208 clock is not set.\n");
	return 0;
#endif
}

static int imx208_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret = 0;

	if (is_moorefield()) {
#ifdef CONFIG_INTEL_SCU_IPC_UTIL
		ret = camera_set_vprog_power(CAMERA_VPROG1, flag,
					     DEFAULT_VOLTAGE);
#else
		ret = -ENODEV;
#endif
		if (ret)
			pr_err("imx208 voltage setting failed\n");
		if (flag)
			usleep_range(1000, 1200);
		return ret;
	}

	if (flag) {
		if (!camera_vprog1_on) {
			ret = regulator_enable(vprog1_reg);
			if (!ret) {
				usleep_range(1000, 1200);
				camera_vprog1_on = 1;
			}
			return ret;
		}
	} else {
		if (camera_vprog1_on) {
			ret = regulator_disable(vprog1_reg);
			if (!ret)
				camera_vprog1_on = 0;
			return ret;
		}
	}
	return 0;
}

static int imx208_csi_configure(struct v4l2_subdev *sd, int flag)
{
	static const int LANES = 1;
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_SECONDARY, LANES,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_rggb, flag);
}

static int imx208_platform_init(struct i2c_client *client)
{
	int ret;

	if (is_moorefield())
		return 0;

	vprog1_reg = regulator_get(&client->dev, "vprog1");
	if (IS_ERR(vprog1_reg)) {
		dev_err(&client->dev, "regulator_get failed\n");
		return PTR_ERR(vprog1_reg);
	}
	ret = regulator_set_voltage(vprog1_reg, VPROG1_VAL, VPROG1_VAL);
	if (ret) {
		dev_err(&client->dev, "regulator voltage set failed\n");
		regulator_put(vprog1_reg);
	}

	return ret;
}

static int imx208_platform_deinit(void)
{
	if (!is_moorefield())
		regulator_put(vprog1_reg);

	return 0;
}

static struct camera_sensor_platform_data imx208_sensor_platform_data = {
	.gpio_ctrl       = imx208_gpio_ctrl,
	.flisclk_ctrl    = imx208_flisclk_ctrl,
	.power_ctrl      = imx208_power_ctrl,
	.csi_cfg         = imx208_csi_configure,
	.platform_init   = imx208_platform_init,
	.platform_deinit = imx208_platform_deinit,
};
void *imx208_platform_data(void *info)
{
	camera_reset = -1;

	return &imx208_sensor_platform_data;
}

