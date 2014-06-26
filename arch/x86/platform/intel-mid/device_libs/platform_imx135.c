/*
 * platform_imx135.c: imx135 platform data initilization file
 *
 * (C) Copyright 2012 Intel Corporation
 * Author:
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
#include <asm/intel-mid.h>
#include <media/v4l2-subdev.h>
#include <linux/regulator/consumer.h>
#include "platform_camera.h"
#include "platform_imx135.h"


static int camera_reset;
static int camera_power;

static int camera_vemmc1_on;  
static struct regulator *vemmc1_reg;  
#define VEMMC1_VAL 2850000  

static int camera_vprog1_on;
static struct regulator *vprog1_reg;
#define VPROG1_VAL 2800000

static int is_ctp(void)
{
	return INTEL_MID_BOARD(1, PHONE, CLVTP)
	       || INTEL_MID_BOARD(1, TABLET, CLVT);
}

static int is_victoriabay(void)
{
	return INTEL_MID_BOARD(2, PHONE, CLVTP, VB, PRO)
	       || INTEL_MID_BOARD(2, PHONE, CLVTP, VB, ENG)
	       || INTEL_MID_BOARD(3, PHONE, CLVTP, RHB, PRO, VVLITE)
	       || INTEL_MID_BOARD(3, PHONE, CLVTP, RHB, ENG, VVLITE)
	       || ((INTEL_MID_BOARD(2, PHONE, CLVTP, RHB, PRO)
		    || INTEL_MID_BOARD(2, PHONE, CLVTP, RHB, ENG))
		   && (SPID_HARDWARE_ID(CLVTP, PHONE, VB, PR1A)   
		       || SPID_HARDWARE_ID(CLVTP, PHONE, VB, PR1B)
		       || SPID_HARDWARE_ID(CLVTP, PHONE, VB, PR20)));
}

static int is_moorefield(void)
{
	return INTEL_MID_BOARD(1, PHONE, MOFD) ||
	       INTEL_MID_BOARD(1, TABLET, MOFD);
}

/*
 * MRFLD VV primary camera sensor - IMX135 platform data
 */

static int imx135_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;

	if (camera_reset < 0) {
		ret = camera_sensor_gpio(-1, GP_CAMERA_0_RESET,
					GPIOF_DIR_OUT, 1);
		if (ret < 0)
			return ret;
		camera_reset = ret;
	}

	if (flag) {
		gpio_set_value(camera_reset, 1);
		/* min 250us -Initializing time of silicon */
		usleep_range(250, 300);

	} else {
		gpio_set_value(camera_reset, 0);
	}

	return 0;
}

static int imx135_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
	static const unsigned int clock_khz = 19200;
#ifdef CONFIG_INTEL_SCU_IPC_UTIL
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM0, flag ? clock_khz : 0);
#else
	pr_err("imx135 clock is not set.\n");
	return 0;
#endif
}

static int imx135_power_ctrl(struct v4l2_subdev *sd, int flag)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;

	if (is_moorefield()) {
#ifdef CONFIG_INTEL_SCU_IPC_UTIL
		ret = camera_set_vprog_power(CAMERA_VPROG1, flag,
					     DEFAULT_VOLTAGE);
		if (ret) {
			pr_err("imx135 power %s vprog1 failed\n",
			       flag ? "on" : "off");
			return ret;
		}

		ret = camera_set_vprog_power(CAMERA_VPROG3, flag,
					     CAMERA_1_05_VOLT);
		if (ret) {
			pr_err("imx135 power %s vprog3 failed\n",
			       flag ? "on" : "off");
			if (flag)
				camera_set_vprog_power(CAMERA_VPROG1, !flag,
						       DEFAULT_VOLTAGE);
			return ret;
		}

		if (flag)
			usleep_range(1000, 1200);
#else
		ret = -ENODEV;
#endif
		return ret;
	}

	if (flag) {
		if (is_ctp()) {
			if (!camera_vemmc1_on) {

				camera_vemmc1_on = 1;
				ret = regulator_enable(vemmc1_reg);
				if (ret) {
					dev_err(&client->dev,
						"Failed to enable regulator vemmc1\n");
					return ret;
				}

			}
			if (vprog1_reg && !camera_vprog1_on) {
				camera_vprog1_on = 1;
				ret = regulator_enable(vprog1_reg);
				if (ret) {
					dev_err(&client->dev,
						"Failed to enable regulator vprog1\n");
					return ret;
				}

			}
			if (!is_victoriabay()) {
				if (camera_power < 0) {
					ret = camera_sensor_gpio(-1,
						GP_CAMERA_1_POWER_DOWN,
						GPIOF_DIR_OUT, 1);
					if (ret < 0)
						return ret;
					camera_power = ret;
				}
				gpio_set_value(camera_power, 1);
			}
			/* min 250us -Initializing time of silicon */
			usleep_range(250, 300);
		} else {
			if (!camera_vprog1_on) {
				ret = regulator_enable(vprog1_reg);
				if (!ret) {
					/* imx1x5 VDIG rise to XCLR release */
					usleep_range(1000, 1200);
					camera_vprog1_on = 1;
				}
				return ret;
			}
		}
	} else {
		if (is_ctp()) {
			if (camera_vemmc1_on) {
				camera_vemmc1_on = 0;

				ret = regulator_disable(vemmc1_reg);
				if (ret) {
					dev_err(&client->dev,
						"Failed to disable regulator vemmc1\n");
					return ret;
				}
			}
			if (vprog1_reg && camera_vprog1_on) {
				camera_vprog1_on = 0;

				ret = regulator_disable(vprog1_reg);
				if (ret) {
					dev_err(&client->dev,
						"Failed to disable regulator vprog1\n");
					return ret;
				}
			}
		} else {
			if (camera_vprog1_on) {
				ret = regulator_disable(vprog1_reg);
				if (!ret)
					camera_vprog1_on = 0;
				return ret;
			}
		}
	}
	return 0;
}
static int imx135_platform_init(struct i2c_client *client)
{
	int ret;
	if (is_ctp()) {
		vemmc1_reg = regulator_get(&client->dev, "vemmc1");
		if (IS_ERR(vemmc1_reg)) {
			dev_err(&client->dev, "regulator_get failed\n");
			return PTR_ERR(vemmc1_reg);
		}
	}
	if (!is_victoriabay() && !is_moorefield()) {
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
	}

	return 0;
}

static int imx135_platform_deinit(void)
{
	if (!is_victoriabay() && !is_moorefield())
		regulator_put(vprog1_reg);

	if (is_ctp())
		regulator_put(vemmc1_reg);

	return 0;
}

static int imx135_csi_configure(struct v4l2_subdev *sd, int flag)
{
	static const int LANES = 4;
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_PRIMARY, LANES,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_rggb, flag);
}

static char *imx135_msr_file_name(void)
{
	static char buffer[32];
	return camera_get_msr_filename(buffer, sizeof(buffer), "imx135", 0);
}

static struct camera_sensor_platform_data imx135_sensor_platform_data = {
	.gpio_ctrl      = imx135_gpio_ctrl,
	.flisclk_ctrl   = imx135_flisclk_ctrl,
	.power_ctrl     = imx135_power_ctrl,
	.csi_cfg        = imx135_csi_configure,
	.msr_file_name  = imx135_msr_file_name,
	.platform_init = imx135_platform_init,
	.platform_deinit = imx135_platform_deinit,
};

void *imx135_platform_data(void *info)
{
	camera_reset = -1;
	camera_power = -1;

	return &imx135_sensor_platform_data;
}

