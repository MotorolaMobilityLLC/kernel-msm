/*
 * Copyright (C) 2010 Intel Corporation
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
 * Faxing Lu <faxing.lu@intel.com>
 */

#include <asm/intel_scu_pmic.h>

#include "displays/sharp25x16_vid.h"
static int mipi_reset_gpio;

static u8 sharp_mode_set_data[7][3] = {
			{0x10, 0x00, 0x3f},
			{0x10, 0x01, 0x00},
			{0x10, 0x07, 0x00},
			{0x70, 0x00, 0x70},
			{0x00, 0x1f, 0x00},
			{0x20, 0x2e, 0x12},
			{0x20, 0x2a, 0x00}
			};
static u8 sharp_set_brightness[3] =
			{0x20, 0x2a, 0x0};
int mdfld_dsi_sharp25x16_ic_init(struct mdfld_dsi_config *dsi_config)
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
	return 0;

ic_init_err:
	err = -EIO;
	return err;
}

static
void mdfld_dsi_sharp25x16_dsi_controller_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *hw_ctx =
		&dsi_config->dsi_hw_context;
	/* Virtual channel number */
	int mipi_vc = 0;
	int mipi_pixel_format = 0x4;

	PSB_DEBUG_ENTRY("\n");

	/*reconfig lane configuration*/
	dsi_config->lane_count = 4;
	dsi_config->lane_config = MDFLD_DSI_DATA_LANE_4_0;
	hw_ctx->pll_bypass_mode = 0;
	/* This is for 400 mhz.  Set it to 0 for 800mhz */
	hw_ctx->cck_div = 1;

	hw_ctx->mipi_control = 0;
	hw_ctx->intr_en = 0xFFFFFFFF;
	hw_ctx->hs_tx_timeout = 0xFFFFFF;
	hw_ctx->lp_rx_timeout = 0xFFFFFF;
	hw_ctx->device_reset_timer = 0xffff;
	hw_ctx->turn_around_timeout = 0x3f;
	hw_ctx->high_low_switch_count = 0x40;
	hw_ctx->clk_lane_switch_time_cnt =  0x16002d;
	hw_ctx->lp_byteclk = 0x5;
	hw_ctx->dphy_param = 0x3c1fc51f;
	hw_ctx->eot_disable = 0x2;
	hw_ctx->init_count = 0xfa0;
	hw_ctx->dbi_bw_ctrl = 0x820;

	/*setup video mode format*/
	hw_ctx->video_mode_format = 0xf;

	/*set up func_prg*/
	hw_ctx->dsi_func_prg = ((mipi_pixel_format << 7) | (mipi_vc << 3) |
			dsi_config->lane_count);

	/*setup mipi port configuration*/
	hw_ctx->mipi = MIPI_PORT_EN | PASS_FROM_SPHY_TO_AFE |
		dsi_config->lane_config |
		DUAL_LINK_ENABLE | DUAL_LINK_CAPABLE | DUAL_LINK_MODE_PIXEL_ALTER;
}

static int mdfld_dsi_sharp25x16_detect(struct mdfld_dsi_config *dsi_config)
{
	int status;
	int pipe = dsi_config->pipe;

	PSB_DEBUG_ENTRY("\n");

	if (pipe == 0) {
		status = MDFLD_DSI_PANEL_CONNECTED;
	} else {
		DRM_INFO("%s: do NOT support dual panel\n", __func__);
		status = MDFLD_DSI_PANEL_DISCONNECTED;
	}

	return status;
}

static int mdfld_dsi_sharp25x16_power_on(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	PSB_DEBUG_ENTRY("\n");

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	/* Sleep Out */
	err = mdfld_dsi_send_mcs_short_hs(sender, exit_sleep_mode, 0, 0,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Exit Sleep Mode\n", __func__, __LINE__);
		goto power_on_err;
	}
	/* Wait for 6 frames after exit_sleep_mode. */
	msleep(100);

	/* Set Display on */
	err = mdfld_dsi_send_mcs_short_hs(sender, set_display_on, 0, 0,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Display On\n", __func__, __LINE__);
		goto power_on_err;
	}
	/* Wait for 1 frame after set_display_on. */
	msleep(20);

	/* Send TURN_ON packet */
	err = mdfld_dsi_send_dpi_spk_pkg_hs(sender, MDFLD_DSI_DPI_SPK_TURN_ON);
	if (err) {
		DRM_ERROR("Failed to send turn on packet\n");
		goto power_on_err;
	}
	return 0;

power_on_err:
	err = -EIO;
	return err;
}

static int mdfld_dsi_sharp25x16_power_off(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	PSB_DEBUG_ENTRY("\n");

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	/*send SHUT_DOWN packet */
	err = mdfld_dsi_send_dpi_spk_pkg_hs(sender,
			MDFLD_DSI_DPI_SPK_SHUT_DOWN);
	if (err) {
		DRM_ERROR("Failed to send turn off packet\n");
		goto power_off_err;
	}
	/* According HW DSI spec, need to wait for 100ms. */
	msleep(100);

	/* Set Display off */
	err = mdfld_dsi_send_mcs_short_hs(sender, set_display_off, 0, 0,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Display On\n", __func__, __LINE__);
		goto power_off_err;
	}
	/* Wait for 1 frame after set_display_on. */
	msleep(20);

	/* Sleep In */
	err = mdfld_dsi_send_mcs_short_hs(sender, enter_sleep_mode, 0, 0,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Exit Sleep Mode\n", __func__, __LINE__);
		goto power_off_err;
	}
	/* Wait for 3 frames after enter_sleep_mode. */
	msleep(51);

	/* enable it after AOB re-work
	* gpio_set_value_cansleep(mipi_reset_gpio, 0);
	*/
	return 0;

power_off_err:
	err = -EIO;
	return err;
}

static int mdfld_dsi_sharp25x16_panel_reset(struct mdfld_dsi_config *dsi_config)
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

static struct drm_display_mode *sharp25x16_vid_get_config_mode(void)
{
	struct drm_display_mode *mode;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	mode->hdisplay = 2560;

	mode->hsync_start = mode->hdisplay + 8;
	mode->hsync_end = mode->hsync_start + 30;
	mode->htotal = mode->hsync_end + 32;

	mode->vdisplay = 1600;
	mode->vsync_start = mode->vdisplay + 12;
	mode->vsync_end = mode->vsync_start + 4;
	mode->vtotal = mode->vsync_end + 4;

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal *
		mode->htotal / 1000;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	mode->type |= DRM_MODE_TYPE_PREFERRED;

	return mode;
}

static int mdfld_dsi_sharp25x16_set_brightness(struct mdfld_dsi_config *dsi_config,
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

static void sharp25x16_vid_get_panel_info(int pipe, struct panel_info *pi)
{
	if (!pi)
		return;

	if (pipe == 0) {
		pi->width_mm = 217;
		pi->height_mm = 136;
	}

	return;
}

void sharp25x16_vid_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	PSB_DEBUG_ENTRY("\n");

	p_funcs->get_config_mode = sharp25x16_vid_get_config_mode;
	p_funcs->get_panel_info = sharp25x16_vid_get_panel_info;
	p_funcs->reset = mdfld_dsi_sharp25x16_panel_reset;
	p_funcs->drv_ic_init = mdfld_dsi_sharp25x16_ic_init;
	p_funcs->dsi_controller_init = mdfld_dsi_sharp25x16_dsi_controller_init;
	p_funcs->detect = mdfld_dsi_sharp25x16_detect;
	p_funcs->power_on = mdfld_dsi_sharp25x16_power_on;
	p_funcs->power_off = mdfld_dsi_sharp25x16_power_off;
	p_funcs->set_brightness = mdfld_dsi_sharp25x16_set_brightness;
}
