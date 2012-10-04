/* Copyright (c) 2011-2012, Motorola Mobility Inc. All rights reserved.
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

#include <linux/bootmem.h>
#include <linux/init.h>
#include <linux/ion.h>
#include <linux/ioport.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <mach/msm_bus_board.h>
#include <mach/msm_memtypes.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/ion.h>
#include <mach/socinfo.h>

#include "devices.h"
#include "board-8960.h"
#include "board-mmi.h"

#define get_l17_voltage()	2700000
#define HWREV_P2 0x8200
#define HWREV_P1 0x8100
#define HWREV_P1C 0x81C0

#define MDP_VSYNC_GPIO 0
#define MDP_VSYNC_ENABLED       true
#define MDP_VSYNC_DISABLED      false

extern unsigned int system_rev;

#define MIPI_DSI_MOT_PANEL_NAME "mipi_dsi_mot_panel"
#define MIPI_VIDEO_SMD_HD_PANEL_NAME "mipi_mot_video_smd_hd_465"
#define MIPI_CMD_SMD_QHD_PANEL_NAME "mipi_mot_cmd_smd_qhd_429"
#define MIPI_CMD_AUO_HD_PANEL_NAME "mipi_mot_cmd_auo_hd_450"
#define MIPI_CMD_CMI_HD_PANEL_NAME  "mipi_mot_cmd_cmi_hd_430"
#define HDMI_PANEL_NAME	"hdmi_msm"

/*
 * This is a flag that only be used when bringup a new platform.
 * There is a minimun changes from the MOT display FW and board files are
 * needed turn on the display.
 * The MIPI_MOT_PANEL_PLATF_BRINGUP flag needs to uncomment in
 * arch/arm/mach-msm/board-mmi-display.c & driver/video/msm/mipi_mot_common.c
 * to do this task
 */
#define MIPI_MOT_PANEL_PLATF_BRINGUP 1

static int  mmi_feature_hdmi = 1;
static char panel_name[PANEL_NAME_MAX_LEN + 1] = MIPI_VIDEO_SMD_HD_PANEL_NAME;

static int __init mot_parse_atag_display(const struct tag *tag)
{
	const struct tag_display *display_tag = &tag->u.display;
	strlcpy(panel_name, display_tag->display, PANEL_NAME_MAX_LEN);
	panel_name[PANEL_NAME_MAX_LEN] = '\0';
	pr_info("%s: %s\n", __func__, panel_name);
	return 0;
}
__tagtable(ATAG_DISPLAY, mot_parse_atag_display);

static int is_smd_hd_465(void)
{
	return !strncmp(panel_name, MIPI_VIDEO_SMD_HD_PANEL_NAME,
				PANEL_NAME_MAX_LEN);
}

static int is_smd_qhd_429(void)
{
	return !strncmp(panel_name, MIPI_CMD_SMD_QHD_PANEL_NAME,
				PANEL_NAME_MAX_LEN);
}

static int is_auo_hd_450(void)
{
	return !strncmp(panel_name, MIPI_CMD_AUO_HD_PANEL_NAME,
				PANEL_NAME_MAX_LEN);
}

static int mmi_mipi_panel_power_en(int on);

/*
 * - This is a work around for the panel SOL mooth transition feature
 *   QCOm has provided patch to make this feature to work for video mode panel
 *   but this will break MOT's feature for command mode.
 * - While waiting for QCOm to delivery a patch that can make this feature
 *   to work for both pane; types. We will use this in the mean time
 */
static bool mipi_mot_panel_is_cmd_mode(void)
{
	bool ret = true;

	if (!strncmp("mipi_mot_video_smd_hd_465", panel_name,
							PANEL_NAME_MAX_LEN))
		ret = false;

	return ret;
}

static bool use_mdp_vsync = MDP_VSYNC_ENABLED;

static int mipi_panel_power(int on);
static int mipi_dsi_power(int on)
{
	static struct regulator *reg_l23, *reg_l2;
	int rc;

	pr_debug("%s (%d) is called\n", __func__, on);

	if (!(reg_l23 || reg_l2)) {
		/*
		 * This is a HACK for SOL smooth transtion: Because QCOM can
		 * not keep the MIPI lines at LP11 start when the kernel start
		 * so, when the first update, we need to reset the panel to
		 * raise the err detection from panel due to the MIPI lines
		 * drop.
		 * - With Video mode, this will be call mdp4_dsi_video_on()
		 *   before it disable the timing generator. This will prevent
		 *   the image to fade away.
		 * - Command mode, then we have to reset in the first call
		 *   to trun on the power
		 */
		if (mipi_mot_panel_is_cmd_mode())
			mmi_mipi_panel_power_en(0);

		reg_l23 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_vddio");
		if (IS_ERR(reg_l23)) {
			pr_err("could not get 8921_l23, rc = %ld\n",
							PTR_ERR(reg_l23));
			rc = -ENODEV;
			goto end;
		}

		reg_l2 = regulator_get(&msm_mipi_dsi1_device.dev, "dsi_vdda");
		if (IS_ERR(reg_l2)) {
			pr_err("could not get 8921_l2, rc = %ld\n",
							PTR_ERR(reg_l2));
			rc = -ENODEV;
			goto end;
		}

		rc = regulator_set_voltage(reg_l23, 1800000, 1800000);
		if (rc) {
			pr_err("set_voltage l23 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}

		rc = regulator_set_voltage(reg_l2, 1200000, 1200000);
		if (rc) {
			pr_err("set_voltage l2 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}
	}

	if (on) {
		rc = regulator_set_optimum_mode(reg_l23, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}

		rc = regulator_set_optimum_mode(reg_l2, 100000);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}

		rc = regulator_enable(reg_l23);
		if (rc) {
			pr_err("enable l23 failed, rc=%d\n", rc);
			rc =  -ENODEV;
			goto end;
		}

		rc = regulator_enable(reg_l2);
		if (rc) {
			pr_err("enable l2 failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		mdelay(10);
	} else {
		rc = regulator_disable(reg_l2);
		if (rc) {
			pr_err("disable reg_l2 failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		rc = regulator_disable(reg_l23);
		if (rc) {
			pr_err("disable reg_l23 failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		rc = regulator_set_optimum_mode(reg_l23, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l23 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}

		rc = regulator_set_optimum_mode(reg_l2, 100);
		if (rc < 0) {
			pr_err("set_optimum_mode l2 failed, rc=%d\n", rc);
			rc = -EINVAL;
			goto end;
		}
	}

#ifdef MIPI_MOT_PANEL_PLATF_BRINGUP
	mipi_panel_power(on);
#endif
	rc = 0;
end:
	return rc;
}

static int mmi_mipi_panel_power_en(int on)
{
	int rc;
	static int disp_5v_en, lcd_reset;
	static int lcd_reset1; /* this is a hacked for vanquish phone */

	pr_debug("%s (%d) is called\n", __func__, on);

	if (lcd_reset == 0) {
		/*
		* This is a work around for Vanquish P1C HW ONLY.
		* There are 2 HW versions of vanquish P1C, wing board phone and
		* normal phone. The wing P1C phone will use GPIO_PM 43 and
		* normal P1C phone will use GPIO_PM 37  but both of them will
		* have the same HWREV.
		* To make both of them to work, then if HWREV=P1C, then we
		* will toggle both GPIOs 43 and 37, but there will be one to
		* be used, and there will be no harm if another doesn't use.
		*/
		if (is_smd_hd_465()) {
			if (system_rev == HWREV_P1C) {
				lcd_reset = PM8921_GPIO_PM_TO_SYS(43);
				lcd_reset1 = PM8921_GPIO_PM_TO_SYS(37);
			} else if (system_rev > HWREV_P1C)
				lcd_reset = PM8921_GPIO_PM_TO_SYS(37);
			else
				lcd_reset = PM8921_GPIO_PM_TO_SYS(43);
		} else if (is_smd_qhd_429())
			lcd_reset = PM8921_GPIO_PM_TO_SYS(37);
		else
			lcd_reset = PM8921_GPIO_PM_TO_SYS(43);

		rc = gpio_request(lcd_reset, "disp_rst_n");
		if (rc) {
			pr_err("request lcd_reset failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		if (is_smd_hd_465() && lcd_reset1 != 0) {
			rc = gpio_request(lcd_reset1, "disp_rst_1_n");
			if (rc) {
				pr_err("request lcd_reset1 failed, rc=%d\n",
									rc);
				rc = -ENODEV;
				goto end;
			}
		}

		disp_5v_en = 13;
		rc = gpio_request(disp_5v_en, "disp_5v_en");
		if (rc) {
			pr_err("request disp_5v_en failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		rc = gpio_direction_output(disp_5v_en, 1);
		if (rc) {
			pr_err("set output disp_5v_en failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}

		if ((is_smd_hd_465() && system_rev < HWREV_P1) ||
							is_smd_qhd_429()) {
			rc = gpio_request(12, "disp_3_3");
			if (rc) {
				pr_err("%s: unable to request gpio %d (%d)\n",
							__func__, 12, rc);
				rc = -ENODEV;
				goto end;
			}

			rc = gpio_direction_output(12, 1);
			if (rc) {
				pr_err("%s: Unable to set direction\n",
								__func__);
				rc = -EINVAL;
				goto end;
			}
		}

		if (is_smd_hd_465() && system_rev >= HWREV_P1) {
			rc = gpio_request(0, "dsi_vci_en");
			if (rc) {
				pr_err("%s: unable to request gpio %d (%d)\n",
							__func__, 0, rc);
				rc = -ENODEV;
				goto end;
			}

			rc = gpio_direction_output(0, 1);
			if (rc) {
				pr_err("%s: Unable to set direction\n",
								__func__);
				rc = -EINVAL;
				goto end;
			}
		}
	}

	if (on) {
		gpio_set_value(disp_5v_en, 1);

		if (is_smd_hd_465())
			mdelay(5);

		if ((is_smd_hd_465() && system_rev < HWREV_P1) ||
							is_smd_qhd_429())
			gpio_set_value_cansleep(12, 1);

		if (is_smd_hd_465() && system_rev >= HWREV_P1)
			gpio_set_value_cansleep(0, 1);

		if (is_smd_hd_465())
			mdelay(30);
		else if (is_smd_qhd_429())
			mdelay(25);
		else
			mdelay(10);

		gpio_set_value_cansleep(lcd_reset, 1);

		if (is_smd_hd_465() && lcd_reset1 != 0)
			gpio_set_value_cansleep(lcd_reset1, 1);

		mdelay(20);
	} else {
		gpio_set_value_cansleep(lcd_reset, 0);

		if (is_smd_hd_465() && lcd_reset1 != 0)
			gpio_set_value_cansleep(lcd_reset1, 0);

		mdelay(10);

		if (is_auo_hd_450()) {
			/* There is a HW issue of qinara P1, that if we release
			 * reg_5V during suspend, then we will have problem to
			 * turn it back on during resume
			 */
			if (system_rev >= HWREV_P2)
				gpio_set_value(disp_5v_en, 0);
		} else
			gpio_set_value(disp_5v_en, 0);

		if ((is_smd_hd_465() && system_rev < HWREV_P1) ||
							is_smd_qhd_429())
			gpio_set_value_cansleep(12, 0);

		if (is_smd_hd_465() && system_rev >= HWREV_P1)
			gpio_set_value_cansleep(0, 0);
	}

	rc = 0;
end:
	return rc;
}


static int mipi_panel_power(int on)
{
	static bool panel_power_on;
	static struct regulator *reg_vddio, *reg_vci;
	int rc;

	pr_debug("%s (%d) is called\n", __func__, on);

	if (!panel_power_on) {
		if (is_smd_hd_465() && system_rev >= HWREV_P2) {
			/* Vanquish P2 is not using VREG_L17 */
			reg_vddio = NULL;
		} else if (is_smd_hd_465() && system_rev >= HWREV_P1) {
			reg_vddio = regulator_get(
						&msm_mipi_dsi1_device.dev,
						"disp_vddio");
		} else if (is_smd_qhd_429())
			/* TODO: Need to check qinara P2 */
			reg_vddio = regulator_get(&msm_mipi_dsi1_device.dev,
								"dsi_s4");
		else
			reg_vddio = regulator_get(&msm_mipi_dsi1_device.dev,
								"dsi_vdc");

		if (IS_ERR(reg_vddio)) {
			pr_err("could not get 8921_vddio/vdc, rc = %ld\n",
			PTR_ERR(reg_vddio));
			rc = -ENODEV;
			goto end;
		}

		if ((is_smd_hd_465() && system_rev >= HWREV_P1) ||
							is_smd_qhd_429()) {
			reg_vci = regulator_get(&msm_mipi_dsi1_device.dev,
								"disp_vci");
			if (IS_ERR(reg_vci)) {
				pr_err("could not get disp_vci, rc = %ld\n",
							PTR_ERR(reg_vci));
				rc = -ENODEV;
				goto end;
			}
		}

		if (NULL != reg_vddio) {
			if (is_smd_qhd_429())
				/* TODO: Need to check qinara P2 */
				rc = regulator_set_voltage(reg_vddio, 1800000,
								1800000);
			else
				rc = regulator_set_voltage(reg_vddio,
						get_l17_voltage(), 2850000);

			if (rc) {
				pr_err("set_voltage l17 failed, rc=%d\n", rc);
				rc = -EINVAL;
				goto end;
			}
		}

		if (NULL != reg_vci) {
			rc = regulator_set_voltage(reg_vci, 3100000, 3100000);
			if (rc) {
				pr_err("set_voltage vci failed, rc=%d\n", rc);
				rc = -EINVAL;
				goto end;
			}
		}

		panel_power_on = true;
	}

	if (on) {
		if (NULL != reg_vddio) {
			rc = regulator_set_optimum_mode(reg_vddio, 100000);
			if (rc < 0) {
				pr_err("set_optimum_mode l8 failed, rc=%d\n",
									rc);
				rc = -EINVAL;
				goto end;
			}

			rc = regulator_enable(reg_vddio);
			if (rc) {
				pr_err("enable l8 failed, rc=%d\n", rc);
				rc = -ENODEV;
				goto end;
			}
		}

		if (NULL != reg_vci) {
			rc = regulator_set_optimum_mode(reg_vci, 100000);
			if (rc < 0) {
				pr_err("set_optimum_mode vci failed, rc=%d\n",
									rc);
				rc = -EINVAL;
				goto end;
		}

		rc = regulator_enable(reg_vci);
		if (rc) {
			pr_err("enable vci failed, rc=%d\n", rc);
			rc = -ENODEV;
			goto end;
		}
	}

	mmi_mipi_panel_power_en(1);

	} else {
		mmi_mipi_panel_power_en(0);

		if (NULL != reg_vddio) {
			rc = regulator_disable(reg_vddio);
			if (rc) {
				pr_err("disable reg_l8 failed, rc=%d\n", rc);
				rc = -ENODEV;
				goto end;
			}

			rc = regulator_set_optimum_mode(reg_vddio, 100);
			if (rc < 0) {
				pr_err("set_optimum_mode l8 failed, rc=%d\n",
									rc);
				rc = -EINVAL;
				goto end;
			}

		}

		if (NULL != reg_vci) {
			rc = regulator_disable(reg_vci);
			if (rc) {
				pr_err("disable reg_vci failed, rc=%d\n", rc);
				rc = -ENODEV;
				goto end;
			}

			rc = regulator_set_optimum_mode(reg_vci, 100);
			if (rc < 0) {
				pr_err("set_optimum_mode vci failed, rc=%d\n",
									rc);
				rc = -EINVAL;
				goto end;
			}
		}
	}

	rc = 0;
end:
	return rc;
}

static int msm_fb_detect_panel(const char *name)
{
	if (!strncmp(name, panel_name, PANEL_NAME_MAX_LEN)) {
		pr_info("%s: detected %s\n", __func__, name);
		return 0;
	}
	/*
	 * This is a HACK for SOL smooth transition, will need to clean up
	 * to use device tree for this purpose.
	 * with the Video mode, it uses this MDP_VSYNC_GPIO, 0 GPIO to
	 * enable the VDDIO. Because of the SOL smooth transition, this gpio
	 * must configure "high" to keep the VDDIO on
	 */
	else if (mipi_mot_panel_is_cmd_mode() != true)
		use_mdp_vsync = MDP_VSYNC_DISABLED;
	else
		use_mdp_vsync = MDP_VSYNC_ENABLED;

	if (mmi_feature_hdmi && !strncmp(name, HDMI_PANEL_NAME,
				strnlen(HDMI_PANEL_NAME, PANEL_NAME_MAX_LEN)))
		return 0;

	pr_warning("%s: not supported '%s'", __func__, name);
	return -ENODEV;
}


static struct mipi_dsi_panel_platform_data mipi_dsi_mot_pdata = {
};

static struct platform_device mipi_dsi_mot_panel_device = {
	.name = MIPI_DSI_MOT_PANEL_NAME,
	.id = 0,
	.dev = {
		.platform_data = &mipi_dsi_mot_pdata,
	}
};

void __init mmi_display_init(struct msm_fb_platform_data *msm_fb_pdata,
	struct mipi_dsi_platform_data *mipi_dsi_pdata)
{
	msm_fb_pdata->detect_client = msm_fb_detect_panel;
	mipi_dsi_pdata->vsync_gpio = MDP_VSYNC_GPIO;
	mipi_dsi_pdata->dsi_power_save = mipi_dsi_power;
	mipi_dsi_pdata->panel_power_save = mipi_panel_power;
	platform_device_register(&mipi_dsi_mot_panel_device);
}
