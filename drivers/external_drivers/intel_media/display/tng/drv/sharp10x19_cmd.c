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
 * Faxing Lu <faxing.lu@intel.com>
 */

#include "mdfld_dsi_dbi.h"
#include "mdfld_dsi_esd.h"
#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_mid_remoteproc.h>

#include "displays/sharp10x19_cmd.h"

/* The register to control secure I2C FLIS pin */
#define SECURE_I2C_FLIS_REG	0xFF0C1D30

#define EXPANDER_BUS_NUMBER 7

static int mipi_reset_gpio;
static int mipic_reset_gpio;
static int bias_en_gpio;

#define sharp10x19_remove_nvm_reload 0xd6
static	u8 sharp10x19_mcs_column_addr[] = { 0x2a, 0x00, 0x00, 0x04, 0x37 };
static	u8 sharp10x19_mcs_page_addr[] = { 0x2b, 0x00, 0x00, 0x07, 0x7f };

static int sharp10x19_cmd_drv_ic_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	struct drm_device *dev = dsi_config->dev;
	int ret;
	u8 cmd;
	int i;
	int loop = 1;

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	if (is_dual_panel(dev))
		loop = 2;
	for(i = 0; i < loop; i++) {
		if (i == 0)
			sender->work_for_slave_panel = false;
		else
			sender->work_for_slave_panel = true;

		/* exit sleep */
		cmd = exit_sleep_mode;
		ret = mdfld_dsi_send_mcs_short_hs(sender,
						  cmd, 0, 0, MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;
		msleep(120);

		/* unlock MCW */
		cmd = access_protect;
		ret = mdfld_dsi_send_mcs_short_hs(sender,
						  cmd, 0x0, 1, MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* reload NVM */
		cmd = sharp10x19_remove_nvm_reload;
		ret = mdfld_dsi_send_mcs_short_hs(sender,
						  cmd, 0x1, 1, MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* send display brightness */
		cmd = write_display_brightness;
		ret = mdfld_dsi_send_mcs_short_hs(sender,
						  cmd, 0xff, 1, MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* display control */
		cmd = write_ctrl_display;
		ret = mdfld_dsi_send_mcs_short_hs(sender,
						  cmd, 0x0c, 1, MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* CABC */
		cmd = 0x55;
		ret = mdfld_dsi_send_mcs_short_hs(sender,
						  cmd, 0x0, 1, MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* tear on*/
		cmd = set_tear_on;
		ret = mdfld_dsi_send_mcs_short_hs(sender,
						  cmd, 0x0, 1, MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* column address */
		cmd = 0x2a;
		ret = mdfld_dsi_send_mcs_long_hs(sender, sharp10x19_mcs_column_addr, 5,
						 MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* page address */
		cmd = 0x2b;
		ret = mdfld_dsi_send_mcs_long_hs(sender, sharp10x19_mcs_page_addr, 5,
						 MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;
	}
	sender->work_for_slave_panel = false;
	return 0;

err_out:
	sender->work_for_slave_panel = false;
	DRM_ERROR("failed to send command %#x\n", cmd);
	return ret;
}

static void sharp10x19_cmd_controller_init(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_hw_context *hw_ctx =
				&dsi_config->dsi_hw_context;
	struct drm_device *dev = dsi_config->dev;

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
	hw_ctx->turn_around_timeout = 0x14;
	if (is_dual_panel(dev)) {
		hw_ctx->high_low_switch_count = 0x2B;
		hw_ctx->clk_lane_switch_time_cnt =  0x2b0014;
		hw_ctx->eot_disable = 0x0;
	} else {
		hw_ctx->high_low_switch_count = 0x2c;
		hw_ctx->clk_lane_switch_time_cnt =  0x2e0016;
		hw_ctx->eot_disable = 0x1;
	}
	hw_ctx->lp_byteclk = 0x6;
	hw_ctx->dphy_param = 0x2a18681f;

	hw_ctx->init_count = 0xf0;
	hw_ctx->dbi_bw_ctrl = calculate_dbi_bw_ctrl(dsi_config->lane_count);
	hw_ctx->hs_ls_dbi_enable = 0x0;
	hw_ctx->dsi_func_prg = ((DBI_DATA_WIDTH_OPT2 << 13) |
				dsi_config->lane_count);
	if (is_dual_panel(dev))
		hw_ctx->mipi = SEL_FLOPPED_HSTX	| PASS_FROM_SPHY_TO_AFE |
			DUAL_LINK_ENABLE | DUAL_LINK_CAPABLE;
	else
		hw_ctx->mipi = PASS_FROM_SPHY_TO_AFE |
			BANDGAP_CHICKEN_BIT | TE_TRIGGER_GPIO_PIN;

	hw_ctx->video_mode_format = 0xf;

}

static int
sharp10x19_cmd_panel_connection_detect(struct mdfld_dsi_config *dsi_config)
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

static int sharp10x19_cmd_power_on(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	struct drm_device *dev = dsi_config->dev;
	int ret;
	u8 cmd;
	int i;
	int loop = 1;

	PSB_DEBUG_ENTRY("\n");

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	if (is_dual_panel(dev))
		loop = 2;
	for(i = 0; i < loop; i++) {
		if (i == 0)
			sender->work_for_slave_panel = false;
		else
			sender->work_for_slave_panel = true;
		/* address mode */
		cmd = set_address_mode;
		ret = mdfld_dsi_send_mcs_short_hs(sender,
						  cmd, 0x0, 1, MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* pixel format*/
		cmd = set_pixel_format;
		ret = mdfld_dsi_send_mcs_short_hs(sender,
						  cmd, 0x77, 1, MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;

		/* display on */
		cmd = set_display_on;
		ret = mdfld_dsi_send_mcs_short_hs(sender,
						  cmd, 0, 0, MDFLD_DSI_SEND_PACKAGE);
		if (ret)
			goto err_out;
	}
	sender->work_for_slave_panel = false;
	return 0;

err_out:
	sender->work_for_slave_panel = false;
	DRM_ERROR("failed to send command %#x\n", cmd);
	return ret;
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

static int sharp10x19_cmd_power_off(struct mdfld_dsi_config *dsi_config)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	struct drm_device *dev = dsi_config->dev;
	int err;
	int i;
	int loop = 1;

	PSB_DEBUG_ENTRY("\n");

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}
	if (is_dual_panel(dev))
		loop = 2;

	for(i = 0; i < loop; i++) {
		if (i == 0)
			sender->work_for_slave_panel = false;
		else
			sender->work_for_slave_panel = true;

		err = mdfld_dsi_send_mcs_short_hs(sender,
				set_display_off, 0, 0,
				MDFLD_DSI_SEND_PACKAGE);
		if (err) {
			DRM_ERROR("%s: %d: Set Display Off\n", __func__, __LINE__);
			goto power_off_err;
		}
		usleep_range(20000, 20100);

		err = mdfld_dsi_send_mcs_short_hs(sender,
				set_tear_off, 0, 0,
				MDFLD_DSI_SEND_PACKAGE);
		if (err) {
			DRM_ERROR("%s: %d: Set Tear Off\n", __func__, __LINE__);
			goto power_off_err;
		}

		err = mdfld_dsi_send_mcs_short_hs(sender,
				enter_sleep_mode, 0, 0,
				MDFLD_DSI_SEND_PACKAGE);
		if (err) {
			DRM_ERROR("%s: %d: Enter Sleep Mode\n", __func__, __LINE__);
			goto power_off_err;
		}

		msleep(60);

		err = mdfld_dsi_send_gen_short_hs(sender,
			access_protect, 4, 2,
			MDFLD_DSI_SEND_PACKAGE);
		if (err) {
			DRM_ERROR("%s: %d: Set Access Protect\n", __func__, __LINE__);
			goto power_off_err;
		}

		err = mdfld_dsi_send_gen_short_hs(sender, low_power_mode, 1, 2,
						  MDFLD_DSI_SEND_PACKAGE);
		if (err) {
			DRM_ERROR("%s: %d: Set Low Power Mode\n", __func__, __LINE__);
			goto power_off_err;
		}
		if (bias_en_gpio)
			gpio_set_value_cansleep(bias_en_gpio, 0);
		usleep_range(1000, 1500);
	}
	sender->work_for_slave_panel = false;
	return 0;
power_off_err:
	sender->work_for_slave_panel = false;
	err = -EIO;
	return err;
}

static int
sharp10x19_cmd_set_brightness( struct mdfld_dsi_config *dsi_config, int level)
{
	struct mdfld_dsi_pkg_sender *sender =
		mdfld_dsi_get_pkg_sender(dsi_config);
	u8 duty_val = 0;
	struct drm_device *dev = dsi_config->dev;

	PSB_DEBUG_ENTRY("level = %d\n", level);

	if (!sender) {
		DRM_ERROR("Failed to get DSI packet sender\n");
		return -EINVAL;
	}

	duty_val = (0xFF * level) / 255;
	mdfld_dsi_send_mcs_short_hs(sender,
			write_display_brightness, duty_val, 1,
			MDFLD_DSI_SEND_PACKAGE);
	if (is_dual_panel(dev)) {
		sender->work_for_slave_panel = true;
		mdfld_dsi_send_mcs_short_hs(sender,
				write_display_brightness, duty_val, 1,
				MDFLD_DSI_SEND_PACKAGE);
		sender->work_for_slave_panel = false;
	}
	return 0;
}

static void _get_panel_reset_gpio(bool is_dual_panel)
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
		pr_info("gpio_reseta=%d\n", mipi_reset_gpio);
	}
	if (is_dual_panel && (mipic_reset_gpio == 0)) {
		ret = 155;
		if (ret < 0) {
			DRM_ERROR("Faild to get panel reset gpio, " \
				  "use default reset pin\n");
			return;
		}
		mipic_reset_gpio = ret;
		ret = gpio_request(mipic_reset_gpio, "mipic_display");
		if (ret) {
			DRM_ERROR("Faild to request panel reset gpio(c)\n");
			return;
		}
		gpio_direction_output(mipic_reset_gpio, 0);
		pr_info("gpio_resetc=%d\n", mipic_reset_gpio);
	}

}

static int sharp10x19_cmd_panel_reset(struct mdfld_dsi_config *dsi_config)
{
	int ret = 0;
	u8 *vaddr = NULL, *vaddr1 = NULL;
	struct drm_device *dev = dsi_config->dev;
	int reg_value_scl = 0;

	PSB_DEBUG_ENTRY("\n");
	if (is_dual_panel(dev)) {
		struct i2c_adapter *adapter;
		u8 i2_data[4];
		adapter = i2c_get_adapter(EXPANDER_BUS_NUMBER);
		if (adapter) {
			i2_data[0] = 0x4;
			i2_data[1] = 0x0;
			i2c_clients_command(adapter, 1, i2_data);
			i2_data[0] = 0x5;
			i2_data[1] = 0x3;
			i2c_clients_command(adapter, 1, i2_data);
		}
	}
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
	usleep_range(2000, 2500);

	if (bias_en_gpio == 0) {
		bias_en_gpio = 189;
		ret = gpio_request(bias_en_gpio, "bias_enable");
		if (ret) {
			DRM_ERROR("Faild to request bias_enable gpio\n");
			return -EINVAL;
		}
		gpio_direction_output(bias_en_gpio, 0);
		pr_info("gpio_bias_enable=%d\n", bias_en_gpio);
	}

	_get_panel_reset_gpio(is_dual_panel(dev));

	gpio_direction_output(bias_en_gpio, 0);
	gpio_direction_output(mipi_reset_gpio, 0);
	if (is_dual_panel(dev))
		gpio_direction_output(mipic_reset_gpio, 0);
	gpio_set_value_cansleep(bias_en_gpio, 0);
	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	if (is_dual_panel(dev))
		gpio_set_value_cansleep(mipic_reset_gpio, 0);
	usleep_range(2000, 2500);
	gpio_set_value_cansleep(bias_en_gpio, 1);
	usleep_range(2000, 2500);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	usleep_range(2000, 2500);
	if (is_dual_panel(dev)) {
		gpio_set_value_cansleep(mipic_reset_gpio, 1);
		usleep_range(3000, 3500);
	}
	vaddr = ioremap(0xff0c2d00, 0x60);
	iowrite32(0x3221, vaddr + 0x1c);
	usleep_range(2000, 2500);
	iounmap(vaddr);

	/* switch i2c scl pin back */
	reg_value_scl |= 0x1000;
	rpmsg_send_generic_raw_command(RP_INDIRECT_WRITE, 0,
					(u8 *)&reg_value_scl, 4,
					NULL, 0,
					SECURE_I2C_FLIS_REG, 0);
	iounmap(vaddr1);
	return 0;
}

static int sharp10x19_cmd_exit_deep_standby(struct mdfld_dsi_config *dsi_config)
{
	struct drm_device *dev = dsi_config->dev;
	PSB_DEBUG_ENTRY("\n");

	if (bias_en_gpio)
		gpio_set_value_cansleep(bias_en_gpio, 1);
	_get_panel_reset_gpio(is_dual_panel(dev));
	gpio_direction_output(mipi_reset_gpio, 0);

	gpio_set_value_cansleep(mipi_reset_gpio, 0);
	usleep_range(1000, 1500);
	gpio_set_value_cansleep(mipi_reset_gpio, 1);
	usleep_range(3000, 3500);
	if (is_dual_panel(dev)) {
		gpio_direction_output(mipic_reset_gpio, 0);
		gpio_set_value_cansleep(mipic_reset_gpio, 0);
		usleep_range(1000, 1500);
		gpio_set_value_cansleep(mipic_reset_gpio, 1);
		usleep_range(3000, 3500);
	}
	return 0;
}

static struct drm_display_mode *sharp10x19_dual_cmd_get_config_mode(void)
{
	struct drm_display_mode *mode;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	mode->hdisplay = 2160;
	mode->hsync_start = mode->hdisplay + 8;
	mode->hsync_end = mode->hsync_start + 24;
	mode->htotal = mode->hsync_end + 8;

	mode->vdisplay = 1920;
	mode->vsync_start = 1923;
	mode->vsync_end = 1926;
	mode->vtotal = 1987;

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal * mode->htotal / 1000;
	mode->type |= DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	return mode;
}

static struct drm_display_mode *sharp10x19_cmd_get_config_mode(void)
{
	struct drm_display_mode *mode;

	PSB_DEBUG_ENTRY("\n");

	mode = kzalloc(sizeof(*mode), GFP_KERNEL);
	if (!mode)
		return NULL;

	mode->hdisplay = 1080;
	mode->hsync_start = mode->hdisplay + 8;
	mode->hsync_end = mode->hsync_start + 24;
	mode->htotal = mode->hsync_end + 8;

	mode->vdisplay = 1920;
	mode->vsync_start = 1923;
	mode->vsync_end = 1926;
	mode->vtotal = 1987;

	mode->vrefresh = 60;
	mode->clock =  mode->vrefresh * mode->vtotal * mode->htotal / 1000;
	mode->type |= DRM_MODE_TYPE_PREFERRED;

	drm_mode_set_name(mode);
	drm_mode_set_crtcinfo(mode, 0);

	return mode;
}


static void sharp10x19_cmd_get_panel_info(int pipe, struct panel_info *pi)
{
	PSB_DEBUG_ENTRY("\n");

	if (pipe == 0) {
		pi->width_mm = PANEL_4DOT3_WIDTH;
		pi->height_mm = PANEL_4DOT3_HEIGHT;
	}
}

void sharp10x19_cmd_init(struct drm_device *dev, struct panel_funcs *p_funcs)
{
	if (!dev || !p_funcs) {
		DRM_ERROR("Invalid parameters\n");
		return;
	}

	PSB_DEBUG_ENTRY("\n");
	if (is_dual_panel(dev))
		p_funcs->get_config_mode =
			sharp10x19_dual_cmd_get_config_mode;
	else
		p_funcs->get_config_mode =
			sharp10x19_cmd_get_config_mode;

	p_funcs->reset = sharp10x19_cmd_panel_reset;
	p_funcs->power_on = sharp10x19_cmd_power_on;
	p_funcs->power_off = sharp10x19_cmd_power_off;
	p_funcs->drv_ic_init = sharp10x19_cmd_drv_ic_init;
	p_funcs->get_panel_info = sharp10x19_cmd_get_panel_info;
	p_funcs->dsi_controller_init = sharp10x19_cmd_controller_init;
	p_funcs->detect = sharp10x19_cmd_panel_connection_detect;
	p_funcs->set_brightness = sharp10x19_cmd_set_brightness;
	p_funcs->exit_deep_standby = sharp10x19_cmd_exit_deep_standby;
}
