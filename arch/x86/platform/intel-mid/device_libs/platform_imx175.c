/*
 * platform_imx175.c: imx175 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
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
#include <linux/regulator/consumer.h>
#include <asm/intel_scu_ipcutil.h>
#include <asm/intel-mid.h>
#include <media/v4l2-subdev.h>
#include <linux/mfd/intel_mid_pmic.h>

#ifdef CONFIG_VLV2_PLAT_CLK
#include <linux/vlv2_plat_clock.h>
#endif

#include "platform_camera.h"
#include "platform_imx175.h"

/* workround - pin defined for byt */
#define CAMERA_0_RESET 126
#define CAMERA_0_RESET_CHT  150
#define CAMERA_0_RESET_CRV2 119
#ifdef CONFIG_VLV2_PLAT_CLK
#define OSC_CAM0_CLK 0x0
#define CLK_19P2MHz 0x1
#endif

#ifdef CONFIG_CRYSTAL_COVE
static struct regulator *v1p8_reg;
static struct regulator *v2p8_reg;
#endif

static int camera_reset;
static int camera_power_down;
static int camera_vprog1_on;
/*
 * MRFLD VV primary camera sensor - IMX175 platform data
 */

static int imx175_gpio_ctrl(struct v4l2_subdev *sd, int flag)
{
	int ret;

	if (!IS_BYT && !IS_CHT) {
		if (camera_reset < 0) {
			ret = camera_sensor_gpio(-1, GP_CAMERA_0_RESET,
					GPIOF_DIR_OUT, 1);
			if (ret < 0)
				return ret;
			camera_reset = ret;
		}
	} else {
		/*
		 * FIXME: WA using hardcoded GPIO value here.
		 * The GPIO value would be provided by ACPI table, which is
		 * not implemented currently
		 */
		if (camera_reset < 0) {
			if (spid.hardware_id == BYT_TABLET_BLK_CRV2)
				camera_reset = CAMERA_0_RESET_CRV2;
			else if (IS_CHT)
				camera_reset = CAMERA_0_RESET_CHT;
			else
				camera_reset = CAMERA_0_RESET;
			ret = gpio_request(camera_reset, "camera_reset");
			if (ret) {
				pr_err("%s: failed to request gpio(pin %d)\n",
				__func__, CAMERA_0_RESET);
				return -EINVAL;
			}
		}

		ret = gpio_direction_output(camera_reset, 1);
		if (ret) {
			pr_err("%s: failed to set gpio(pin %d) direction\n",
				__func__, camera_reset);
			gpio_free(camera_reset);
		}
	}
	if (flag) {
		gpio_set_value(camera_reset, 1);
		/* imx175 core silicon initializing time - t1+t2+t3
		 * 400us(t1) - Time to VDDL is supplied after REGEN high
		 * 600us(t2) - imx175 core Waking up time
		 * 459us(t3, 8825clocks) -Initializing time of silicon
		 */
		usleep_range(1500, 1600);

	} else {
		gpio_set_value(camera_reset, 0);
		/* 1us - Falling time of REGEN after XCLR H -> L */
		udelay(1);
	}

	return 0;
}

static int imx175_flisclk_ctrl(struct v4l2_subdev *sd, int flag)
{
#ifdef CONFIG_VLV2_PLAT_CLK
	if (flag) {
		int ret;
		ret = vlv2_plat_set_clock_freq(OSC_CAM0_CLK, CLK_19P2MHz);
		if (ret)
			return ret;
	}
	return vlv2_plat_configure_clock(OSC_CAM0_CLK, flag);
#elif defined(CONFIG_INTEL_SCU_IPC_UTIL)
	static const unsigned int clock_khz = 19200;
	return intel_scu_ipc_osc_clk(OSC_CLK_CAM0,
			flag ? clock_khz : 0);
#else
	pr_err("imx175 clock is not set\n");
	return 0;
#endif
}

static int imx175_power_ctrl(struct v4l2_subdev *sd, int flag)
{
#ifdef CONFIG_CRYSTAL_COVE
	struct i2c_client *client = v4l2_get_subdevdata(sd);
#endif
	int ret = 0;

#ifdef CONFIG_CRYSTAL_COVE
	if (!IS_CHT && (!v1p8_reg || !v2p8_reg)) {
		dev_err(&client->dev,
				"not avaiable regulator\n");
		return -EINVAL;
	}
#endif

	/*
	 * FIXME: VRF has no implementation for CHT now,
	 * remove pmic power control when VRF is ready.
	 */
#ifdef CONFIG_CRYSTAL_COVE
	if (IS_CHT) {
		if (flag) {
			if (!camera_vprog1_on) {
				ret = camera_set_pmic_power(CAMERA_2P8V, true);
				if (ret) {
					dev_err(&client->dev,
						"Failed to enable pmic power v2p8\n");
					return ret;
				}

				ret = camera_set_pmic_power(CAMERA_1P8V, true);
				if (ret) {
					camera_set_pmic_power(CAMERA_2P8V, false);
					dev_err(&client->dev,
						"Failed to enable pmic power v1p8\n");
				}
			}
		} else {
			if (camera_vprog1_on) {
				ret = camera_set_pmic_power(CAMERA_2P8V, false);
				if (ret)
					dev_warn(&client->dev,
						 "Failed to disable pmic power v2p8\n");
				ret = camera_set_pmic_power(CAMERA_1P8V, false);
				if (ret)
					dev_warn(&client->dev,
						 "Failed to disable pmic power v1p8\n");
			}
		}
		return ret;
	}
#endif
	if (flag) {
		if (!camera_vprog1_on) {
#ifdef CONFIG_CRYSTAL_COVE
			ret = regulator_enable(v2p8_reg);
			if (ret) {
				dev_err(&client->dev,
						"Failed to enable regulator v2p8\n");
				return ret;
			}
			ret = regulator_enable(v1p8_reg);
			if (ret) {
				regulator_disable(v2p8_reg);
				dev_err(&client->dev,
						"Failed to enable regulator v1p8\n");
			}
#elif defined(CONFIG_INTEL_SCU_IPC_UTIL)
			ret = intel_scu_ipc_msic_vprog1(1);
#else
			pr_err("imx175 power is not set.\n");
#endif
			if (!ret) {
				/* imx1x5 VDIG rise to XCLR release */
				usleep_range(1000, 1200);
				camera_vprog1_on = 1;
			}
			return ret;
		}
	} else {
		if (camera_vprog1_on) {
#ifdef CONFIG_CRYSTAL_COVE
			ret = regulator_disable(v2p8_reg);
			if (ret)
				dev_warn(&client->dev,
						"Failed to disable regulator v2p8\n");
			ret = regulator_disable(v1p8_reg);
			if (ret)
				dev_warn(&client->dev,
						"Failed to disable regulator v1p8\n");
#elif defined(CONFIG_INTEL_SCU_IPC_UTIL)
			ret = intel_scu_ipc_msic_vprog1(0);
#else
			pr_err("imx175 power is not set.\n");
#endif
			if (!ret)
				camera_vprog1_on = 0;
			return ret;
		}
	}
	return ret;
}

static int imx175_csi_configure(struct v4l2_subdev *sd, int flag)
{
	static const int LANES = 4;
	return camera_sensor_csi(sd, ATOMISP_CAMERA_PORT_PRIMARY, LANES,
		ATOMISP_INPUT_FORMAT_RAW_10, atomisp_bayer_order_rggb, flag);
}

#ifdef CONFIG_CRYSTAL_COVE
static int imx175_platform_init(struct i2c_client *client)
{
	if (IS_CHT)
		return 0;
	v1p8_reg = regulator_get(&client->dev, "v1p8sx");
	if (IS_ERR(v1p8_reg)) {
		dev_err(&client->dev, "v1p8s regulator_get failed\n");
		return PTR_ERR(v1p8_reg);
	}

	v2p8_reg = regulator_get(&client->dev, "v2p85sx");
	if (IS_ERR(v2p8_reg)) {
		regulator_put(v1p8_reg);
		dev_err(&client->dev, "v2p85sx regulator_get failed\n");
		return PTR_ERR(v2p8_reg);
	}

	return 0;
}

static int imx175_platform_deinit(void)
{
	if (IS_CHT)
		return 0;
	regulator_put(v1p8_reg);
	regulator_put(v2p8_reg);

	return 0;
}
#endif
static struct camera_sensor_platform_data imx175_sensor_platform_data = {
	.gpio_ctrl      = imx175_gpio_ctrl,
	.flisclk_ctrl   = imx175_flisclk_ctrl,
	.power_ctrl     = imx175_power_ctrl,
	.csi_cfg        = imx175_csi_configure,
#ifdef CONFIG_CRYSTAL_COVE
	.platform_init = imx175_platform_init,
	.platform_deinit = imx175_platform_deinit,
#endif
};

void *imx175_platform_data(void *info)
{
	camera_reset = -1;
	camera_power_down = -1;

	return &imx175_sensor_platform_data;
}
