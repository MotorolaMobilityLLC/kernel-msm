/*
 * platform_ov8858.c: ov8858 platform data initialization file
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
#include <asm/intel_scu_ipcutil.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel-mid.h>
#include <media/v4l2-subdev.h>
#include <linux/regulator/consumer.h>
#include <linux/sfi.h>
#include "platform_camera.h"
#include "platform_ov8858.h"

static int camera_reset;

static int ov8858_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;
	if (camera_reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_0_RESET,
					GPIOF_DIR_OUT, 0);
		if (ret < 0)
			return ret;
		camera_reset = ret;
	}

	if (flag) {
		gpio_set_value(camera_reset, 0);
		msleep(20); /* Wait for power lines to stabilize */
		gpio_set_value(camera_reset, 1);
	} else {
		gpio_set_value(camera_reset, 0);
	}

	return 0;
}

static int ov8858_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
#ifdef CONFIG_INTEL_SCU_IPC_UTIL
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM0, flag ? clock_khz : 0);
#else
	pr_err("ov8858 clock is not set.\n");
	return 0;
#endif
}

static int ov8858_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;

#ifdef CONFIG_INTEL_SCU_IPC_UTIL
	ret = camera_set_vprog_power(CAMERA_VPROG1, flag, DEFAULT_VOLTAGE);
#else
	ret = -EINVAL;
#endif
	if (ret) {
		pr_err("Failed to set regulator vprog1 %s\n",
		       flag ? "on" : "off");
		return ret;
	}


#ifdef CONFIG_INTEL_SCU_IPC_UTIL
	ret = camera_set_vprog_power(CAMERA_VPROG3, flag, CAMERA_1_83_VOLT);
#else
	ret = -EINVAL;
#endif
	if (ret) {
		pr_err("Failed to set regulator vprog3 %s\n",
		       flag ? "on" : "off");
		if (flag)
			camera_set_vprog_power(CAMERA_VPROG1, !flag,
					       DEFAULT_VOLTAGE);
		return ret;
	}

	if (flag)
		msleep(20); /* Wait for power lines to stabilize */

	return ret;
}

static int ov8858_csi_configure(struct v4l2_subdev *sd, int flag)
{
	static const int LANES = 4;
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_PRIMARY, LANES,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_bggr, flag);
}

static int ov8858_platform_init(struct i2c_client *client)
{
	return 0;
}

static int ov8858_platform_deinit(void)
{
	return 0;
}

static struct camera_sensor_platform_data ov8858_sensor_platform_data = {
	.gpio_ctrl       = ov8858_gpio_ctrl,
	.flisclk_ctrl    = ov8858_flisclk_ctrl,
	.power_ctrl      = ov8858_power_ctrl,
	.csi_cfg         = ov8858_csi_configure,
	.platform_init   = ov8858_platform_init,
	.platform_deinit = ov8858_platform_deinit,
};

void *ov8858_platform_data(void *info)
{
	camera_reset = -1;
	return &ov8858_sensor_platform_data;
}

