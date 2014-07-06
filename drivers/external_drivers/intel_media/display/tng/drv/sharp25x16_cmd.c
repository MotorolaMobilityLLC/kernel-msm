/*
 * Copyright © 2010 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 * Faxing Lu<faxing.lu@intel.com>
 */

#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_esd.h"
#include <asm/intel_scu_pmic.h>
#include <linux/i2c.h>
#include <linux/i2c/pca953x.h>

#include "displays/sharp25x16_cmd.h"

static int mipi_reset_gpio;

static u8 sharp_mode_set_data[7][3] = {
			{0x10, 0x00, 0x2f},
			{0x10, 0x01, 0x01},
			{0x10, 0x07, 0x00},
			{0x70, 0x00, 0x70},
			{0x00, 0x1f, 0x00},
			{0x20, 0x2e, 0x12},
			{0x20, 0x2a, 0x0}
			};
static u8 sharp_clumn_addr_left[] = {
			0x2a, 0x00, 0x00, 0x09, 0xff};
static u8 sharp_page_addr_left[] = {
			0x2b, 0x00, 0x00, 0x06, 0x3f};
static u8 sharp_set_brightness[3] =
			{0x20, 0x2a, 0x0};
static
int sharp25x16_cmd_drv_ic_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender
		= mdfld_dsi_get_pkg_sender(dsi_config);
	struct drm_device *dev = dsi_config->dev;
	int err = 0;
	int i;

	PSB_DEBUG_ENTRY("\n");
	if (!sender) {
		DRM_ERROR("Cannot get sender\n");
		return -EINVAL;
	}
	for (i = 0; i < 7; i++) {
		err = mdfld_dsi_send_gen_long_hs(sender, sharp_mode_set_data[i],
				3,
				MDFLD_DSI_SEND_PACKAGE);
		if (err) {
			DRM_ERROR("%s: %d: Set Mode data\n", __func__, __LINE__);
			goto ic_init_err;
		}
		REG_WRITE(MIPIA_HS_GEN_CTRL_REG, 5);
	}

	err = mdfld_dsi_send_mcs_long_hs(sender,
			sharp_clumn_addr_left,
			5, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Clumn Address\n",
		__func__, __LINE__);
		goto ic_init_err;
	}
	REG_WRITE(MIPIA_HS_GEN_CTRL_REG, 5);
	err = mdfld_dsi_send_mcs_long_hs(sender,
			sharp_page_addr_left,
			5, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Page Address\n",
		__func__, __LINE__);
		goto ic_init_err;
	}
	REG_WRITE(MIPIA_HS_GEN_CTRL_REG, 5);

	err = mdfld_dsi_send_mcs_short_hs(sender,
			set_tear_on, 0x00, 1,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Tear On\n",
		__func__, __LINE__);
		goto ic_init_err;
	}
	return 0;

ic_init_err:
	err = -EIO;
	return err;
}

static
void sharp25x16_cmd_controller_init(
		struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *hw_ctx =
				&dsi_config->dsi_hw_context;

	PSB_DEBUG_ENTRY("\n");

	/*reconfig lane configuration*/
	dsi_config->lane_count = 4;
	dsi_config->lane_config = MDFLD_DSI_DATA_LANE_4_0;
	hw_ctx->cck_div = 1;
	hw_ctx->pll_bypass_mode = 0;

	hw_ctx->mipi_control = 0x0;
	hw_ctx->intr_en = 0xFFFFFFFF;
	hw_ctx->hs_tx_timeout = 0xFFFFFF;
	hw_ctx->lp_rx_timeout = 0xFFFFFF;
	hw_ctx->device_reset_timer = 0xffff;
	hw_ctx->turn_around_timeout = 0x3f;
	hw_ctx->high_low_switch_count = 0x40;
	hw_ctx->clk_lane_switch_time_cnt = 0x16002d;
	hw_ctx->lp_byteclk = 0x5;
	hw_ctx->dphy_param = 0x3c1fc51f;
	hw_ctx->eot_disable = 0x0;
	hw_ctx->init_count = 0xfa0;
	hw_ctx->dbi_bw_ctrl = 1024;
	hw_ctx->hs_ls_dbi_enable = 0x0;
	hw_ctx->dsi_func_prg = ((DBI_DATA_WIDTH_OPT2 << 13) |
				dsi_config->lane_count);

	hw_ctx->mipi = SEL_FLOPPED_HSTX	| PASS_FROM_SPHY_TO_AFE |
		DUAL_LINK_ENABLE | DUAL_LINK_CAPABLE | TE_TRIGGER_GPIO_PIN |
		DUAL_LINK_MODE_PIXEL_ALTER;
	hw_ctx->video_mode_format = 0xf;

}
static
int sharp25x16_cmd_panel_connection_detect(
	struct mdfld_dsi_config *dsi_config)
{
	int status;
	int pipe = dsi_config->pipe;

	PSB_DEBUG_ENTRY("\n");

	if (pipe == 0) {
		status = MDFLD_DSI_PANEL_CONNECTED;
	} else {
		DRM_INFO("%s: do NOT support dual panel\n",
		__func__);
		status = MDFLD_DSI_PANEL_DISCONNECTED;
	}

	return status;
}

static
int sharp25x16_cmd_power_on(
	struct mdfld_dsi_config *dsi_config)
{

	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err = 0;

	PSB_DEBUG_ENTRY("\n");
	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	msleep(150);
	err = mdfld_dsi_send_mcs_short_hs(sender,
		set_address_mode, 0x0, 1,
		MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Address Mode\n",
		__func__, __LINE__);
		goto power_err;
	}
	err = mdfld_dsi_send_mcs_short_hs(sender,
			set_pixel_format, 0x77, 1,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Pixel format\n",
		__func__, __LINE__);
		goto power_err;
	}

	/* Sleep Out */
	err = mdfld_dsi_send_mcs_short_hs(sender, exit_sleep_mode, 0, 0,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Exit Sleep Mode\n", __func__, __LINE__);
		goto power_err;
	}

	msleep(20);
	/* Set Display on 0x29 */
	err = mdfld_dsi_send_mcs_short_hs(sender, set_display_on, 0, 0,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Display On\n", __func__, __LINE__);
		goto power_err;
	}
	msleep(150);

power_err:
	return err;
}
static int sharp25x16_cmd_power_off(
		struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	PSB_DEBUG_ENTRY("\n");

	msleep(100);
	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	err = mdfld_dsi_send_mcs_short_hs(sender,
			set_display_off, 0, 0,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Display Off\n",
		__func__, __LINE__);
		goto power_off_err;
	}
	err = mdfld_dsi_send_mcs_short_hs(sender,
			enter_sleep_mode, 0, 0,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Enter Sleep Mode\n",
		__func__, __LINE__);
		goto power_off_err;
	}
	/* enable it after AOB re-work
	* gpio_set_value_cansleep(mipi_reset_gpio, 0);
	*/
	msleep(100);
	return 0;
power_off_err:
	err = -EIO;
	return err;
}

static
int sharp25x16_cmd_set_brightness(
		struct mdfld_dsi_config *dsi_config,
		int level)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	struct drm_device *dev = dsi_config->dev;
	u8 duty_val = 0;

	PSB_DEBUG_ENTRY("level = %d\n", level);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	duty_val = (0xFF * level) / 255;
	if (duty_val < 12)
		duty_val = 0;
	sharp_set_brightness[2] = duty_val;
	mdfld_dsi_send_gen_long_hs(sender, sharp_set_brightness,
				3,
				MDFLD_DSI_SEND_PACKAGE);

	REG_WRITE(MIPIA_HS_GEN_CTRL_REG, 5);
	return 0;
}

static
int sharp25x16_cmd_panel_reset(
		struct mdfld_dsi_config *dsi_config)
{
	int ret = 0;

	PSB_DEBUG_ENTRY("\n");
	msleep(10);
	if (mipi_reset_gpio == 0) {
		ret = get_gpio_by_name("disp0_rst");
		if (ret < 0) {
			DRM_ERROR("Faild to get panel reset gpio, " \
				  "use default reset pin\n");
			return 0;
		}
		mipi_reset_gpio = ret;
		ret = gpio_request(mipi_reset_gpio, "mipi_display");
		if (ret) {
			DRM_ERROR("Faild to request panel reset gpio\n");
			return 0;
		}
	}
	gpio_direction_output(mipi_reset_gpio, 0);
	usleep_range(1000, 1500);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);

	return 0;
}

static
int sharp25x16_cmd_exit_deep_standby(
		struct mdfld_dsi_config *dsi_config)
{
	PSB_DEBUG_ENTRY("\n");
	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	usleep_range(1000, 1500);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	return 0;
}

static
struct drm_display_mode *sharp25x16_cmd_get_config_mode(void)
{
	struct drm_display_mode *mode;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	mode->hdisplay = 2560;

	mode->hsync_start = mode->hdisplay + 48;
	mode->hsync_end = mode->hsync_start + 32;
	mode->htotal = mode->hsync_end + 80;

	mode->vdisplay = 1600;
	mode->vsync_start = mode->vdisplay + 3;
	mode->vsync_end = mode->vsync_start + 33;
	mode->vtotal = mode->vsync_end + 10;


	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal * mode->htotal / 1000;
	mode->type |= DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	return mode;
}

static
void sharp25x16_cmd_get_panel_info(int pipe,
		struct panel_info *pi)
{
	PSB_DEBUG_ENTRY("\n");

	if (pipe == 0) {
		pi->width_mm = 217;
		pi->height_mm = 136;
	}
}

void sharp25x16_cmd_init(struct drm_device *dev,
		struct panel_funcs *p_funcs)
{
	if (!dev || !p_funcs) {
		DRM_ERROR("Invalid parameters\n");
		return;
	}
	PSB_DEBUG_ENTRY("\n");
	p_funcs->reset = sharp25x16_cmd_panel_reset;
	p_funcs->power_on = sharp25x16_cmd_power_on;
	p_funcs->power_off = sharp25x16_cmd_power_off;
	p_funcs->drv_ic_init = sharp25x16_cmd_drv_ic_init;
	p_funcs->get_config_mode = sharp25x16_cmd_get_config_mode;
	p_funcs->get_panel_info = sharp25x16_cmd_get_panel_info;
	p_funcs->dsi_controller_init =
			sharp25x16_cmd_controller_init;
	p_funcs->detect =
			sharp25x16_cmd_panel_connection_detect;
	p_funcs->set_brightness =
			sharp25x16_cmd_set_brightness;
	p_funcs->exit_deep_standby =
				sharp25x16_cmd_exit_deep_standby;

}
