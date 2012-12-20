/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2011, Motorola Mobility, LLC.
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
#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_mot.h"
#include "mdp4.h"

static struct mipi_mot_panel *mot_panel;

/* 720x1280/60 fps/4 lanes/230 Mhz */
/* copy from qinara for bring up, modify if need to run at different dsi clk such as @227M */
static struct mipi_dsi_phy_ctrl dsi_cmd_mode_phy_db = {
	{0x09, 0x08, 0x05, 0x00, 0x20}, /* regulator */
	/* timing   */
	{0xb4, 0x2b, 0x1d, 0x00, 0x96, 0x94, 0x21,
	 0x2d, 0x21, 0x03, 0x04, 0xa0},
	{0x5f, 0x00, 0x00, 0x10},       /* phy ctrl */
	{0xff, 0x00, 0x06, 0x00},       /* strength */
	/* pll control */
	{0x00, 0xcb, 0x01, 0x1a, 0x00, 0x50, 0x48, 0x63,
	/* 4 MIPI lanes */
	0x41, 0x0f, 0x03,
	0x00, 0x14, 0x03, 0x0, 0x02, 0x00, 0x20, 0x00, 0x01},
};

static char enter_sleep[2] = {DCS_CMD_ENTER_SLEEP_MODE, 0x00};
static char exit_sleep[2] = {DCS_CMD_EXIT_SLEEP_MODE, 0x00};
static char display_off[2] = {DCS_CMD_SET_DISPLAY_OFF, 0x00};
/* TODO
static char led_pwm1[2] = {DCS_CMD_SET_BRIGHTNESS, 0xFF};
static char led_pwm2[2] = {DCS_CMD_SET_CTRL_DISP, 0x2C};
static char led_pwm3[2] = {DCS_CMD_SET_CABC, 0x03};
*/

/* Set scan line to 2/3 of the screen */
/* TODO
static char set_scanline[3] = {DCS_CMD_SET_SCAN_LINE, 0x03, 0x55};
*/

static struct dsi_cmd_desc mot_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
};

static struct dsi_cmd_desc mot_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(enter_sleep), enter_sleep},
	{DTYPE_DCS_WRITE, 1, 0, 0, 1, sizeof(display_off), display_off},
};

static int panel_enable(struct msm_fb_data_type *mfd)
{
	mipi_mot_tx_cmds(&mot_cmd_on_cmds[0],
		ARRAY_SIZE(mot_cmd_on_cmds));
	return 0;
}

static int panel_disable(struct msm_fb_data_type *mfd)
{
	mipi_mot_tx_cmds(&mot_display_off_cmds[0],
		ARRAY_SIZE(mot_display_off_cmds));
	return 0;
}

static int is_valid_power_mode(struct msm_fb_data_type *mfd)
{
	u8 pwr_mod;
	pwr_mod = mipi_mode_get_pwr_mode(mfd);
	/*Bit7: Booster on ;Bit4: Sleep Out ;Bit2: Display On*/
	return (pwr_mod & 0x94) == 0x94;
}
static int __init mipi_mot_cmd_jdi_hd_430_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	pr_debug("...%s\n", __func__);

	if (msm_fb_detect_client("mipi_mot_cmd_jdi_hd_430"))
		return 0;

	mot_panel = mipi_mot_get_mot_panel();
	if (mot_panel == NULL) {
		pr_err("%s:get mot_panel failed\n", __func__);
		return -EAGAIN;
	}

	pinfo = &mot_panel->pinfo;

	pinfo->xres = 720;
	pinfo->yres = 1280;
	pinfo->type = MIPI_CMD_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;

	/*TODO: check*/
	pinfo->lcdc.h_back_porch = 50;
	pinfo->lcdc.h_front_porch = 50;
	pinfo->lcdc.h_pulse_width = 20;
	pinfo->lcdc.v_back_porch = 11;
	pinfo->lcdc.v_front_porch = 10;
	pinfo->lcdc.v_pulse_width = 5;

	pinfo->lcdc.border_clr = 0;     /* blk */
	pinfo->lcdc.underflow_clr = 0xff;       /* blue */
	pinfo->lcdc.hsync_skew = 0;

	/*TODO*/
	pinfo->bl_max = 100;
	pinfo->bl_min = 1;

	pinfo->fb_num = 2;
	pinfo->clk_rate = 460000000;
	pinfo->physical_width_mm = 53;
	pinfo->physical_height_mm = 95;

	pinfo->lcd.vsync_enable = TRUE;
	pinfo->lcd.hw_vsync_mode = TRUE;
	pinfo->lcd.v_back_porch = 10;
	pinfo->lcd.v_front_porch = 10;
	pinfo->lcd.v_pulse_width = 5;
	pinfo->lcd.refx100 = 6000; /* adjust refx100 to prevent tearing */

	pinfo->mipi.mode = DSI_CMD_MODE;
	pinfo->mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;
	pinfo->mipi.vc = 0;
	pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo->mipi.data_lane0 = TRUE;
	pinfo->mipi.data_lane1 = TRUE;
	pinfo->mipi.data_lane2 = TRUE;
	pinfo->mipi.data_lane3 = TRUE;
	/*TODO*/
	pinfo->mipi.t_clk_post = 0x19;
	pinfo->mipi.t_clk_pre = 0x2f;
	pinfo->mipi.stream = 0; /* dma_p */
	pinfo->mipi.mdp_trigger = DSI_CMD_TRIGGER_NONE;
	pinfo->mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo->mipi.te_sel = 1; /* TE from vsycn gpio */
	pinfo->mipi.interleave_max = 1;
	pinfo->mipi.insert_dcs_cmd = TRUE;
	pinfo->mipi.wr_mem_continue = 0x3c;
	pinfo->mipi.wr_mem_start = 0x2c;
	pinfo->mipi.dsi_phy_db = &dsi_cmd_mode_phy_db;
	pinfo->mipi.tx_eot_append = 0x01;
	pinfo->mipi.rx_eot_ignore = 0;

	mot_panel->acl_support_present = false;
	mot_panel->acl_enabled = FALSE; /* By default the ACL is disabled. */

	mot_panel->elvss_tth_support_present = false;

	mot_panel->panel_enable = panel_enable;
	mot_panel->panel_disable = panel_disable;

	atomic_set(&mot_panel->state, MOT_PANEL_ON);
	/* TODO
	mot_panel->enable_acl = enable_acl; */

	/* For ESD detection information */
	mot_panel->esd_enabled = false;
	/*TODO
	mot_panel->is_valid_manufacture_id = is_valid_manufacture_id; */
	mot_panel->is_valid_power_mode = is_valid_power_mode;

	ret = mipi_mot_device_register(pinfo, MIPI_DSI_PRIM, MIPI_DSI_PANEL_HD);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	pr_debug("...%s device registered\n", __func__);

	return ret;
}

module_init(mipi_mot_cmd_jdi_hd_430_init);
