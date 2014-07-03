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
 * Faxing Lu
 */

#include "displays/cmi_vid.h"

static u8 cmi_set_extension[] = {0xb9, 0xff, 0x83, 0x92};
static u8 cmi_ic_bias_current[] = {
	0xbf, 0x05, 0x60, 0x82,
	0x00, 0x00, 0x00, 0x00};
static u8 cmi_set_power[] = {
	0xb1, 0x7c, 0x00, 0x44,
	0x24, 0x00, 0x0d, 0x0d,
	0x12, 0x1a, 0x3f, 0x3f,
	0x42, 0x72, 0x00, 0x00};
static u8 cmi_set_disp_reg[] = {
	0xb2, 0x0f, 0xc8, 0x05,
	0x0f, 0x08, 0x84, 0x00,
	0xff, 0x05, 0x0f, 0x04,
	0x20, 0x00, 0x00, 0x00};
static u8 cmi_set_command_cyc[] = {
	0xb4, 0x00, 0x00, 0x05,
	0x00, 0xa0, 0x05, 0x16,
	0x9d, 0x30, 0x03, 0x16,
	0x00, 0x03, 0x03, 0x00,
	0x1b, 0x06, 0x07, 0x07,
	0x00, 0x00, 0x00, 0x00};
static u8 cmi_set_mipi_ctrl[] = {0xba, 0x12, 0x83, 0x00};
static u8 cmi_set_blanking_opt_2[]  = {0xc7, 0x00, 0x40, 0x00};
static u8 cmi_set_ltps_ctrl_output[] = {
	0xd5, 0x00, 0x08, 0x08,
	0x00, 0x44, 0x55, 0x66,
	0x77, 0xcc, 0xcc, 0xcc,
	0xcc, 0x00, 0x77, 0x66,
	0x55, 0x44, 0xcc, 0xcc,
	0xcc, 0xcc, 0x00, 0x00};
static u8 cmi_set_video_cyc[] = {
	0xd8, 0x00, 0x00, 0x04,
	0x00, 0xa0, 0x04, 0x16,
	0x9d, 0x30, 0x03, 0x16,
	0x00, 0x03, 0x03, 0x00,
	0x1b, 0x06, 0x07, 0x07,
	0x00, 0x00, 0x00, 0x00};
static u8 cmi_gamma_r[] = {
	0xe0, 0x3a, 0x3e, 0x3c,
	0x2f, 0x31, 0x32, 0x33,
	0x46, 0x04, 0x08, 0x0c,
	0x0d, 0x10, 0x0f, 0x11,
	0x10, 0x17, 0x3a, 0x3e,
	0x3c, 0x2f, 0x31, 0x32,
	0x33, 0x46, 0x04, 0x08,
	0x0c, 0x0d, 0x10, 0x0f,
	0x11, 0x10, 0x17, 0x00};
static u8 cmi_gamma_g[] = {
	0xe1, 0x3b, 0x3e, 0x3d,
	0x31, 0x31, 0x32, 0x33,
	0x46, 0x03, 0x07, 0x0b,
	0x0d, 0x10, 0x0e, 0x11,
	0x10, 0x17, 0x3b, 0x3e,
	0x3d, 0x31, 0x31, 0x32,
	0x33, 0x46, 0x03, 0x07,
	0x0b, 0x0d, 0x10, 0x0e,
	0x11, 0x10, 0x17, 0x00};
static u8 cmi_gamma_b[] = {
	0xe2, 0x01, 0x06, 0x07,
	0x2d, 0x2a, 0x32, 0x1f,
	0x40, 0x05, 0x0c, 0x0e,
	0x11, 0x14, 0x12, 0x13,
	0x0f, 0x18, 0x01, 0x06,
	0x07, 0x2d, 0x2a, 0x32,
	0x1f, 0x40, 0x05, 0x0c,
	0x0e, 0x11, 0x14, 0x12,
	0x13, 0x0f, 0x18, 0x00};
static u8 cmi_enter_set_cabc[] = {
	0xc9, 0x1f, 0x00, 0x1e,
	0x1e, 0x00, 0x00, 0x00,
	0x01, 0xe3, 0x00, 0x00};
static u8 cmi_mcs_protect_on[]      = {0xb9, 0x00, 0x00, 0x00};
static u8 cmi_set_address_mode[]    = {0x36, 0x00, 0x00, 0x00};
static u8 cmi_set_pixel_format[] = {0x3a, 0x70, 0x00, 0x00};
static int mdfld_dsi_cmi_ic_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender
			= mdfld_dsi_get_pkg_sender(dsi_config);
	struct drm_device *dev = dsi_config->dev;
	struct mdfld_dsi_hw_registers *regs = &dsi_config->regs;
	unsigned long wait_timeout;
	if (!sender) {
		DRM_ERROR("Cannot get sender\n");
		return -EINVAL;
	}

	PSB_DEBUG_ENTRY("\n");
	sender->status = MDFLD_DSI_PKG_SENDER_FREE;

	/* sleep out and wait for 150ms. */
	mdfld_dsi_send_mcs_short_hs(sender,
			exit_sleep_mode, 0, 0, 0);
	wait_timeout = jiffies + (3 * HZ / 20);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender,
			cmi_set_extension, 4, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	/* set TE on and wait for 10ms. */
	mdfld_dsi_send_mcs_short_hs(sender,
			set_tear_on, 0, 1, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_mcs_short_hs(sender,
		write_display_brightness, 0xff, 1, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_mcs_short_hs(sender,
			write_ctrl_display, 0x24, 1, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_mcs_short_hs(sender,
			write_ctrl_cabc, 0x2, STILL_IMAGE, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender,
			cmi_ic_bias_current, 5, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender,
			cmi_set_power, 0xe, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender,
			cmi_set_disp_reg, 0xd, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender,
			cmi_set_command_cyc, 24, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender,
			cmi_set_mipi_ctrl, 4, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	REG_WRITE(regs->device_ready_reg, 0);
	REG_WRITE(regs->hs_tx_timeout_reg, 0x00ffffff);
	REG_WRITE(regs->lp_rx_timeout_reg, 0x00ffffff);
	REG_WRITE(regs->turn_around_timeout_reg, 0x0000ffff);
	REG_WRITE(regs->device_reset_timer_reg, 0x000000ff);
	REG_WRITE(regs->high_low_switch_count_reg, 0x00000020);
	REG_WRITE(regs->clk_lane_switch_time_cnt_reg, 0x0020000e);
	REG_WRITE(regs->eot_disable_reg, 0x00000000);
	REG_WRITE(regs->init_count_reg, 0x0000007D0);
	REG_WRITE(regs->lp_byteclk_reg, 0x0000000e);
	REG_WRITE(regs->dphy_param_reg, 0x1b104315);

	REG_WRITE(regs->mipi_reg, 0x80030100);
	REG_WRITE(regs->mipi_control_reg, 0x18);
	REG_WRITE(regs->dsi_func_prg_reg, 0x203);
	REG_WRITE(regs->video_mode_format_reg, 0x17);

	REG_WRITE(regs->intr_en_reg, 0xffffffff);
	REG_WRITE(regs->dpi_resolution_reg, 0x50002d0);
	REG_WRITE(regs->hsync_count_reg, 0x4);
	REG_WRITE(regs->hbp_count_reg, 0x33);
	REG_WRITE(regs->hfp_count_reg, 0x30);
	REG_WRITE(regs->hactive_count_reg, 0x5a0);
	REG_WRITE(regs->vsync_count_reg, 0x8);
	REG_WRITE(regs->vbp_count_reg, 0x8);
	REG_WRITE(regs->vfp_count_reg, 0x8);
	REG_WRITE(regs->video_mode_format_reg, 0x1f);
	REG_WRITE(regs->device_ready_reg, 1);

	mdfld_dsi_send_mcs_short_hs(sender,
			set_video_mode, 0x3, 1, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender,
			cmi_set_blanking_opt_2, 4, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_mcs_short_hs(sender,
			set_panel, 0x8, 1, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_mcs_short_hs(sender,
			set_eq_func_ltps, 0xc, 1, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender, cmi_set_ltps_ctrl_output, 24, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender, cmi_set_video_cyc, 24, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender, cmi_gamma_r, 36, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender, cmi_gamma_g, 36, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender, cmi_gamma_b, 36, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender, cmi_enter_set_cabc, 10, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender, cmi_mcs_protect_on, 4, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender, cmi_set_address_mode, 4, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	mdfld_dsi_send_gen_long_hs(sender, cmi_set_pixel_format, 4, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();
	if (sender->status == MDFLD_DSI_CONTROL_ABNORMAL)
		return -EIO;

	return 0;
}

static
void mdfld_dsi_cmi_dsi_controller_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *hw_ctx =
		&dsi_config->dsi_hw_context;

	PSB_DEBUG_ENTRY("\n");

	/*reconfig lane configuration*/
	dsi_config->lane_count = 2;
	dsi_config->lane_config = MDFLD_DSI_DATA_LANE_4_0;
	hw_ctx->pll_bypass_mode = 0;
	/* This is for 400 mhz.  Set it to 0 for 800mhz */
	hw_ctx->cck_div = 1;

	hw_ctx->mipi_control = 0x18;
	hw_ctx->intr_en = 0xffffffff;
	hw_ctx->hs_tx_timeout = 0xffffff;
	hw_ctx->lp_rx_timeout = 0xffffff;
	hw_ctx->turn_around_timeout = 0xffff;
	hw_ctx->device_reset_timer = 0xff;
	hw_ctx->high_low_switch_count = 0x1C;
	hw_ctx->init_count = 0x7d0;
	hw_ctx->eot_disable = 0x0;
	hw_ctx->lp_byteclk = 0x4;
	hw_ctx->clk_lane_switch_time_cnt = 0x1E000E;
	hw_ctx->dphy_param = 0x1B104315;

	/*setup video mode format*/
	hw_ctx->video_mode_format = 0x17 ;

	/*set up func_prg*/
	hw_ctx->dsi_func_prg = (0x200 | dsi_config->lane_count);
	/*setup mipi port configuration*/
	hw_ctx->mipi = MIPI_PORT_EN | PASS_FROM_SPHY_TO_AFE |
		BANDGAP_CHICKEN_BIT | dsi_config->lane_config | BIT17;
}

static
int mdfld_dsi_cmi_detect(struct mdfld_dsi_config *dsi_config)
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

static int mdfld_dsi_cmi_power_on(struct mdfld_dsi_config *dsi_config)
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

static void __vpro2_power_ctrl(bool on)
{
	u8 addr, value;
	addr = 0xad;
	if (intel_scu_ipc_ioread8(addr, &value))
		DRM_ERROR("%s: %d: failed to read vPro2\n", __func__, __LINE__);

	/* Control vPROG2 power rail with 2.85v. */
	if (on)
		value |= 0x1;
	else
		value &= ~0x1;

	if (intel_scu_ipc_iowrite8(addr, value))
		DRM_ERROR("%s: %d: failed to write vPro2\n",
				__func__, __LINE__);
}

static int mdfld_dsi_cmi_power_off(struct mdfld_dsi_config *dsi_config)
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

	__vpro2_power_ctrl(false);

	return 0;

power_off_err:
	err = -EIO;
	return err;
}

static int mdfld_dsi_cmi_set_brightness(struct mdfld_dsi_config *dsi_config,
		int level)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	u8 duty_val = 0;
	unsigned long wait_timeout;

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	duty_val = (255 * level) / 255;

	mdfld_dsi_send_mcs_short_hs(sender, 0x51, duty_val, 1, 0);
	wait_timeout = jiffies + (HZ / 100);
	while (time_before_eq(jiffies, wait_timeout))
		cpu_relax();

	return 0;
}

static int mdfld_dsi_cmi_panel_reset(struct mdfld_dsi_config *dsi_config)
{
	static int mipi_reset_gpio;

	PSB_DEBUG_ENTRY("\n");
	__vpro2_power_ctrl(true);
	mdelay(1100);
	mipi_reset_gpio = 190;
	gpio_direction_output(mipi_reset_gpio, 0);

	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	mdelay(100);

	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	mdelay(400);

	return 0;
}

static struct drm_display_mode *cmi_vid_get_config_mode(void)
{
	struct drm_display_mode *mode;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	mode->hdisplay = 720;
	mode->vdisplay = 1280;
	mode->hsync_start = 816;
	mode->hsync_end = 824;
	mode->htotal = 920;
	mode->vsync_start = 1284;
	mode->vsync_end = 1286;
	mode->vtotal = 1300;

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal *
		mode->htotal / 1000;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	mode->type |= DRM_MODE_TYPE_PREFERRED;

	return mode;
}

static void  cmi_vid_get_panel_info(int pipe, struct panel_info *pi)
{
	if (!pi)
		return;

	if (pipe == 0) {
		pi->width_mm = PANEL_4DOT3_WIDTH;
		pi->height_mm = PANEL_4DOT3_HEIGHT;
	}

	return;
}

void  cmi_vid_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	PSB_DEBUG_ENTRY("\n");

	p_funcs->get_config_mode =  cmi_vid_get_config_mode;
	p_funcs->get_panel_info =  cmi_vid_get_panel_info;
	p_funcs->reset = mdfld_dsi_cmi_panel_reset;
	p_funcs->drv_ic_init = mdfld_dsi_cmi_ic_init;
	p_funcs->dsi_controller_init =
		mdfld_dsi_cmi_dsi_controller_init;
	p_funcs->detect = mdfld_dsi_cmi_detect;
	p_funcs->power_on = mdfld_dsi_cmi_power_on;
	p_funcs->power_off = mdfld_dsi_cmi_power_off;
	p_funcs->set_brightness =
		mdfld_dsi_cmi_set_brightness;
}
