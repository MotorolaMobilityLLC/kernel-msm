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
 * Austin Hu <austin.hu@intel.com>
 */

#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_mid_remoteproc.h>

#include "displays/jdi_vid.h"

/* The register to control secure I2C FLIS pin */
#define SECURE_I2C_FLIS_REG	0xFF0C1D30

static int mipi_reset_gpio;
static int bias_en_gpio;

static u8 jdi_set_address_mode[] = {0x36, 0x00, 0x00, 0x00};
static u8 jdi_write_display_brightness[] = {0x51, 0x0f, 0xff, 0x00};

static u8 jdi_mcs_clumn_addr[] = {0x2a, 0x00, 0x00, 0x02, 0xcf};
static u8 jdi_mcs_page_addr[] = {0x2b, 0x00, 0x00, 0x04, 0xff};
static u8 jdi_set_mode[] = {0xb3, 0x35};
int mdfld_dsi_jdi_ic_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender
		= mdfld_dsi_get_pkg_sender(dsi_config);
	int err = 0;

	PSB_DEBUG_ENTRY("\n");

	if (!sender) {
		DRM_ERROR("Cannot get sender\n");
		return -EINVAL;
	}

	/* Set Address Mode */
	err = mdfld_dsi_send_mcs_long_hs(sender, jdi_set_address_mode,
			4,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Address Mode\n", __func__, __LINE__);
		goto ic_init_err;
	}

	/* Set Pixel format */
	err = mdfld_dsi_send_mcs_short_hs(sender, set_pixel_format, 0x70, 1,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Pixel format\n", __func__, __LINE__);
		goto ic_init_err;
	}

	/* change "ff0f" according to the brightness desired. */
	err = mdfld_dsi_send_mcs_long_hs(sender, jdi_write_display_brightness,
			4, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Brightness\n", __func__, __LINE__);
		goto ic_init_err;
	}

    /* Write control CABC */
	err = mdfld_dsi_send_mcs_short_hs(sender, write_ctrl_cabc, STILL_IMAGE,
			1, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Write Control CABC\n", __func__, __LINE__);
		goto ic_init_err;
	}
	err = mdfld_dsi_send_mcs_long_hs(sender,jdi_mcs_clumn_addr,
			5, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Clumn Address\n",__func__, __LINE__);
		goto ic_init_err;
	}

	err = mdfld_dsi_send_mcs_long_hs(sender,jdi_mcs_page_addr,
			5, MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Page Address\n",__func__, __LINE__);
		goto ic_init_err;
	}
	return 0;

ic_init_err:
	err = -EIO;
	return err;
}

static
void mdfld_dsi_jdi_dsi_controller_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *hw_ctx =
		&dsi_config->dsi_hw_context;
	/* Virtual channel number */
	int mipi_vc = 0;
	int mipi_pixel_format = 0x4;
	/* BURST_MODE */
	int mipi_mode = 0x3;
	/* IP_TG_CONFIG */
	int ip_tg_config = 0x4;

	PSB_DEBUG_ENTRY("\n");

	/*reconfig lane configuration*/
	dsi_config->lane_count = 3;
	dsi_config->lane_config = MDFLD_DSI_DATA_LANE_4_0;
	hw_ctx->pll_bypass_mode = 0;
	/* This is for 400 mhz.  Set it to 0 for 800mhz */
	hw_ctx->cck_div = 1;

	hw_ctx->mipi_control = 0x18;
	hw_ctx->intr_en = 0xFFFFFFFF;
	hw_ctx->hs_tx_timeout = 0xFFFFFF;
	hw_ctx->lp_rx_timeout = 0xFFFFFF;
	hw_ctx->turn_around_timeout = 0xFFFF;
	hw_ctx->device_reset_timer = 0xFF;
	hw_ctx->high_low_switch_count = 0x20;
	hw_ctx->clk_lane_switch_time_cnt = 0x0020000E;
	hw_ctx->dbi_bw_ctrl = 0x0;
	hw_ctx->eot_disable = 0x0;
	hw_ctx->init_count = 0x7D0;
	hw_ctx->lp_byteclk = 0x4;
	hw_ctx->dphy_param = 0x1B0F4115;

	/*setup video mode format*/
	hw_ctx->video_mode_format = mipi_mode | ip_tg_config;

	/*set up func_prg*/
	hw_ctx->dsi_func_prg = ((mipi_pixel_format << 7) | (mipi_vc << 3) |
			dsi_config->lane_count);

	/*setup mipi port configuration*/
	hw_ctx->mipi = MIPI_PORT_EN | PASS_FROM_SPHY_TO_AFE |
		BANDGAP_CHICKEN_BIT | dsi_config->lane_config;
}

static int mdfld_dsi_jdi_detect(struct mdfld_dsi_config *dsi_config)
{
	int status;
	struct drm_device *dev = dsi_config->dev;
	struct mdfld_dsi_hw_registers *regs = &dsi_config->regs;
	u32 dpll_val, device_ready_val;
	int pipe = dsi_config->pipe;
	u32 power_island = 0;

	PSB_DEBUG_ENTRY("\n");

	if (pipe == 0) {
		/*
		 * FIXME: WA to detect the panel connection status, and need to
		 * implement detection feature with get_power_mode DSI command.
		 */
		power_island = pipe_to_island(pipe);

		if (!power_island_get(power_island)) {
			DRM_ERROR("Failed to turn on power island\n");
			return -EAGAIN;
		}

		dpll_val = REG_READ(regs->dpll_reg);
		device_ready_val = REG_READ(regs->device_ready_reg);
		if ((device_ready_val & DSI_DEVICE_READY) &&
				(dpll_val & DPLL_VCO_ENABLE)) {
			dsi_config->dsi_hw_context.panel_on = true;
		} else {
			dsi_config->dsi_hw_context.panel_on = false;
			DRM_INFO("%s: panel is not detected!\n", __func__);
		}

		status = MDFLD_DSI_PANEL_CONNECTED;

		power_island_put(power_island);
	} else {
		DRM_INFO("%s: do NOT support dual panel\n", __func__);
		status = MDFLD_DSI_PANEL_DISCONNECTED;
	}

	return status;
}

static int mdfld_dsi_jdi_power_on(struct mdfld_dsi_config *dsi_config)
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

	err = mdfld_dsi_send_gen_short_hs(sender,access_protect, 0x4, 2,
                        MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set MCAP\n",__func__, __LINE__);
		goto power_on_err;
	}

	err = mdfld_dsi_send_gen_long_hs(sender, jdi_set_mode,2,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set Mode\n", __func__, __LINE__);
		goto power_on_err;
	}

	err = mdfld_dsi_send_gen_short_hs(sender,access_protect, 0x3, 2,
                        MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Set MCAP\n",__func__, __LINE__);
		goto power_on_err;
	}

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

	/* Write control display */
	err = mdfld_dsi_send_mcs_short_hs(sender, write_ctrl_display, 0x24, 1,
			MDFLD_DSI_SEND_PACKAGE);
	if (err) {
		DRM_ERROR("%s: %d: Write Control Display\n", __func__,
				__LINE__);
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

static int mdfld_dsi_jdi_power_off(struct mdfld_dsi_config *dsi_config)
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

	/* Can not poweroff VPROG2, because many other module related to
	 * this power supply, such as PSH sensor. */
	/*__vpro2_power_ctrl(false);*/
	if (bias_en_gpio)
		gpio_set_value_cansleep(bias_en_gpio, 0);

	return 0;

power_off_err:
	err = -EIO;
	return err;
}

static int mdfld_dsi_jdi_set_brightness(struct mdfld_dsi_config *dsi_config,
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
	mdfld_dsi_send_mcs_short_hs(sender,
			0x51, duty_val, 1,
			MDFLD_DSI_SEND_PACKAGE);
	return 0;
}

static int mdfld_dsi_jdi_panel_reset(struct mdfld_dsi_config *dsi_config)
{
	u8 *vaddr1 = NULL;
	int reg_value_scl = 0;
	int ret = 0;

	PSB_DEBUG_ENTRY("\n");

	/* Because when reset touchscreen panel, touchscreen will pull i2c bus
	 * to low, sometime this operation will cause i2c bus enter into wrong
	 * status, so before reset, switch i2c scl pin */
	vaddr1 = ioremap(SECURE_I2C_FLIS_REG, 4);
	reg_value_scl = ioread32(vaddr1);
	reg_value_scl &= ~0x1000;
	rpmsg_send_generic_raw_command(RP_INDIRECT_WRITE, 0,
					(u8 *)&reg_value_scl, 4,
					NULL, 0,
					SECURE_I2C_FLIS_REG, 0);

	__vpro2_power_ctrl(true);

	/* For meeting tRW1 panel spec */
	usleep_range(2000, 2500);

	if (bias_en_gpio == 0) {
		bias_en_gpio = 189;
		ret = gpio_request(bias_en_gpio, "bias_enable");
		if (ret) {
			DRM_ERROR("Faild to request bias_enable gpio\n");
			return -EINVAL;
		}
		gpio_direction_output(bias_en_gpio, 0);
	}
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
	gpio_direction_output(bias_en_gpio, 0);
	gpio_direction_output(mipi_reset_gpio, 0);
	gpio_set_value_cansleep(bias_en_gpio, 0);
	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	usleep_range(2000, 2500);
	gpio_set_value_cansleep(bias_en_gpio, 1);
	usleep_range(2000, 2500);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	usleep_range(2000, 2500);
	/* switch i2c scl pin back */
	reg_value_scl |= 0x1000;
	rpmsg_send_generic_raw_command(RP_INDIRECT_WRITE, 0,
					(u8 *)&reg_value_scl, 4,
					NULL, 0,
					SECURE_I2C_FLIS_REG, 0);
	iounmap(vaddr1);

	return 0;
}

static struct drm_display_mode *jdi_vid_get_config_mode(void)
{
	struct drm_display_mode *mode;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	mode->hdisplay = 720;
	mode->hsync_start = 816;
	mode->hsync_end = 818;
	mode->htotal = 920;

	mode->vdisplay = 1280;
	mode->vsync_start = 1288;
	mode->vsync_end = 1296;
	mode->vtotal = 1304;

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal *
		mode->htotal / 1000;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	mode->type |= DRM_MODE_TYPE_PREFERRED;

	return mode;
}

static void jdi_vid_get_panel_info(int pipe, struct panel_info *pi)
{
	if (!pi)
		return;

	if (pipe == 0) {
		pi->width_mm = 56;
		pi->height_mm = 99;
	}

	return;
}

void jdi_vid_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	PSB_DEBUG_ENTRY("\n");

	p_funcs->get_config_mode = jdi_vid_get_config_mode;
	p_funcs->get_panel_info = jdi_vid_get_panel_info;
	p_funcs->reset = mdfld_dsi_jdi_panel_reset;
	p_funcs->drv_ic_init = mdfld_dsi_jdi_ic_init;
	p_funcs->dsi_controller_init = mdfld_dsi_jdi_dsi_controller_init;
	p_funcs->detect = mdfld_dsi_jdi_detect;
	p_funcs->power_on = mdfld_dsi_jdi_power_on;
	p_funcs->power_off = mdfld_dsi_jdi_power_off;
	p_funcs->set_brightness = mdfld_dsi_jdi_set_brightness;
}
