/*
 * Copyright © 2014 Intel Corporation
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
 * Eckhart Koeppen <eckhart.koeppen@intel.com>
 */

#include "mdfld_dsi_dbi.h"
#include "mdfld_output.h"
#include "mdfld_dsi_pkg_sender.h"

#include "displays/sdc16x25_8_cmd.h"

#define WIDTH 1600
#define HEIGHT 2560

static int mipi_reset_gpio;
static int bias_en_gpio;

static u8 sdc16x25_8_eight_lane_enable[] = { 0xf2, 0x03, 0x00, 0x01, 0xa4, 0x03, 0x05, 0xa0 };
static u8 sdc16x25_8_test_key_enable[] = { 0xf0, 0x5a, 0x5a };
static u8 sdc16x25_8_test_key_disable[] = { 0xf0, 0xa5, 0xa5 };
static u8 sdc16x25_8_mcs_column_addr[] = { 0x2a, 0x00, 0x00, (WIDTH - 1) >> 8, (WIDTH - 1)  & 0xff };
static u8 sdc16x25_8_mcs_page_addr[] = { 0x2b, 0x00, 0x00, (HEIGHT - 1) >> 8, (HEIGHT -1) & 0xff };

static
int sdc16x25_8_cmd_drv_ic_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int ret;
	u8 cmd;

	PSB_DEBUG_ENTRY("\n");

	/* interface control: dual DSI */
	cmd = sdc16x25_8_test_key_enable[0];
	ret = mdfld_dsi_send_mcs_long_lp(sender, sdc16x25_8_test_key_enable, sizeof(sdc16x25_8_test_key_enable),
					 MDFLD_DSI_SEND_PACKAGE);
	if (ret)
		goto err_out;

	cmd = sdc16x25_8_eight_lane_enable[0];
	ret = mdfld_dsi_send_mcs_long_lp(sender, sdc16x25_8_eight_lane_enable, sizeof(sdc16x25_8_eight_lane_enable),
					 MDFLD_DSI_SEND_PACKAGE);
	if (ret)
		goto err_out;

	cmd = sdc16x25_8_test_key_disable[0];
	ret = mdfld_dsi_send_mcs_long_lp(sender, sdc16x25_8_test_key_disable, sizeof(sdc16x25_8_test_key_disable),
					 MDFLD_DSI_SEND_PACKAGE);
	if (ret)
		goto err_out;

	msleep(200);

	/* exit sleep */
	cmd = exit_sleep_mode;
	ret = mdfld_dsi_send_mcs_short_lp(sender,
					  cmd, 0, 0, MDFLD_DSI_SEND_PACKAGE);
	if (ret)
		goto err_out;
	msleep(200);

	/* send display brightness */
	cmd = write_display_brightness;
	ret = mdfld_dsi_send_mcs_short_lp(sender,
					  cmd, 0xff, 1, MDFLD_DSI_SEND_PACKAGE);
	if (ret)
		goto err_out;

	/* display control */
	cmd = write_ctrl_display;
	ret = mdfld_dsi_send_mcs_short_lp(sender,
					  cmd, 0x20, 1, MDFLD_DSI_SEND_PACKAGE);
	if (ret)
		goto err_out;

	/* tear on*/
	cmd = set_tear_on;
	ret = mdfld_dsi_send_mcs_short_lp(sender,
					  cmd, 0x0, 1, MDFLD_DSI_SEND_PACKAGE);
	if (ret)
		goto err_out;

	/* column address */
	cmd = sdc16x25_8_mcs_column_addr[0];
	ret = mdfld_dsi_send_mcs_long_lp(sender, sdc16x25_8_mcs_column_addr, 5,
					 MDFLD_DSI_SEND_PACKAGE);
	if (ret)
		goto err_out;

	/* page address */
	cmd = sdc16x25_8_mcs_page_addr[0];
	ret = mdfld_dsi_send_mcs_long_lp(sender, sdc16x25_8_mcs_page_addr, 5,
					 MDFLD_DSI_SEND_PACKAGE);
	if (ret)
		goto err_out;

	return 0;

err_out:
	DRM_ERROR("failed to send command %#x\n", cmd);
	return ret;
}

static
void sdc16x25_8_cmd_controller_init(
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
	hw_ctx->high_low_switch_count = 0x2b;
	hw_ctx->clk_lane_switch_time_cnt =  0x2b0014;
	hw_ctx->lp_byteclk = 0x6;
	hw_ctx->dphy_param = 0x2a18681f;
	hw_ctx->eot_disable = 0x3;
	hw_ctx->init_count = 0xf0;
	hw_ctx->dbi_bw_ctrl = 1024;
	hw_ctx->hs_ls_dbi_enable = 0x0;
	hw_ctx->dsi_func_prg = ((DBI_DATA_WIDTH_OPT2 << 13) |
				dsi_config->lane_count);

	hw_ctx->mipi = SEL_FLOPPED_HSTX
			| PASS_FROM_SPHY_TO_AFE
			| DUAL_LINK_ENABLE
			| DUAL_LINK_CAPABLE
			;
	hw_ctx->video_mode_format = 0xf;
}

static int
sdc16x25_8_cmd_panel_connection_detect(struct mdfld_dsi_config *dsi_config)
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

static
int sdc16x25_8_cmd_power_on(
			    struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int ret;
	u8 cmd;

	PSB_DEBUG_ENTRY("\n");

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	/* display on */
	cmd = set_display_on;
	ret = mdfld_dsi_send_mcs_short_lp(sender,
					  cmd, 0, 0, MDFLD_DSI_SEND_PACKAGE);
	if (ret)
		goto err_out;
	return 0;

err_out:
	DRM_ERROR("failed to send command %#x\n", cmd);
	return ret;
}

static int sdc16x25_8_cmd_power_off(
				    struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	int err;

	PSB_DEBUG_ENTRY("\n");

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	err = mdfld_dsi_send_mcs_short_lp(sender,
					  set_display_off, 0, 0,
					  MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Display Off\n", __func__, __LINE__);
		goto power_off_err;
	}

	msleep(35);

	err = mdfld_dsi_send_mcs_short_lp(sender,
					  enter_sleep_mode, 0, 0,
					  MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Enter Sleep Mode\n", __func__, __LINE__);
		goto power_off_err;
	}

	msleep(120);

	if (bias_en_gpio)
		gpio_set_value_cansleep(bias_en_gpio, 0);
	usleep_range(1000, 1500);
	return 0;
power_off_err:
	err = -EIO;
	return err;
}

static
int sdc16x25_8_cmd_set_brightness(
				  struct mdfld_dsi_config *dsi_config,
				  int level)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	u8 duty_val = 0;

	PSB_DEBUG_ENTRY("level = %d\n", level);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	duty_val = (0xFF * level) / 255;
	mdfld_dsi_send_mcs_short_lp(sender,
				    write_display_brightness, duty_val, 1,
				    MDFLD_DSI_SEND_PACKAGE);
	return 0;
}

static void _get_panel_reset_gpio(void)
{
	int ret = 0;
	if (mipi_reset_gpio == 0) {
		ret = get_gpio_by_name("disp0_rst");
		if (ret < 0) {
			DRM_ERROR("Faild to get panel reset gpio, " \
				  "use default reset pin\n");
			return;
		}
		mipi_reset_gpio = ret;
		ret = gpio_request(mipi_reset_gpio, "mipi_display");
		if (ret) {
			DRM_ERROR("Faild to request panel reset gpio\n");
			return;
		}
		gpio_direction_output(mipi_reset_gpio, 0);
	}
}

static
int sdc16x25_8_cmd_panel_reset(
			       struct mdfld_dsi_config *dsi_config)
{
	int ret = 0;

	PSB_DEBUG_ENTRY("\n");

	if (bias_en_gpio == 0) {
		bias_en_gpio = 189;
		ret = gpio_request(bias_en_gpio, "bias_enable");
		if (ret) {
			DRM_ERROR("Faild to request bias_enable gpio\n");
			return -EINVAL;
		}
		gpio_direction_output(bias_en_gpio, 0);
	}

	_get_panel_reset_gpio();

	gpio_direction_output(bias_en_gpio, 0);
	gpio_direction_output(mipi_reset_gpio, 0);
	gpio_set_value_cansleep(bias_en_gpio, 0);
	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	msleep(15);
	gpio_set_value_cansleep(bias_en_gpio, 1);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	msleep(5);
	return 0;
}

static
int sdc16x25_8_cmd_exit_deep_standby(
				     struct mdfld_dsi_config *dsi_config)
{
	PSB_DEBUG_ENTRY("\n");

	if (bias_en_gpio)
		gpio_set_value_cansleep(bias_en_gpio, 1);
	_get_panel_reset_gpio();
	gpio_direction_output(mipi_reset_gpio, 0);

	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	msleep(15);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	msleep(5);
	return 0;
}

static
struct drm_display_mode *sdc16x25_8_cmd_get_config_mode(void)
{
	struct drm_display_mode *mode;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	mode->hdisplay = WIDTH;

	mode->hsync_start = mode->hdisplay + 48;
	mode->hsync_end = mode->hsync_start + 32;
	mode->htotal = mode->hsync_end + 80;

	mode->vdisplay = HEIGHT;
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


static void sdc16x25_8_cmd_get_panel_info(int pipe, struct panel_info *pi)
{
	PSB_DEBUG_ENTRY("\n");

	if (pipe == 0) {
		pi->width_mm = 130;
		pi->height_mm = 181;
		pi->panel_180_rotation = true;
	}
}

void sdc16x25_8_cmd_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	if (!dev || !p_funcs) {
		DRM_ERROR("Invalid parameters\n");
		return;
	}
	PSB_DEBUG_ENTRY("\n");
	p_funcs->reset = sdc16x25_8_cmd_panel_reset;
	p_funcs->power_on = sdc16x25_8_cmd_power_on;
	p_funcs->power_off = sdc16x25_8_cmd_power_off;
	p_funcs->drv_ic_init = sdc16x25_8_cmd_drv_ic_init;
	p_funcs->get_config_mode = sdc16x25_8_cmd_get_config_mode;
	p_funcs->get_panel_info = sdc16x25_8_cmd_get_panel_info;
	p_funcs->dsi_controller_init =
		sdc16x25_8_cmd_controller_init;
	p_funcs->detect =
		sdc16x25_8_cmd_panel_connection_detect;
	p_funcs->set_brightness =
		sdc16x25_8_cmd_set_brightness;
	p_funcs->exit_deep_standby =
		sdc16x25_8_cmd_exit_deep_standby;

}
