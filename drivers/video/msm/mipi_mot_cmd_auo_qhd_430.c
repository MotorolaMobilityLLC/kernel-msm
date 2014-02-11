/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2011, Motorola Mobility, Inc.
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

static struct mipi_mot_panel *mot_panel;

static struct mipi_dsi_phy_ctrl dsi_cmd_mode_phy_db = {
	{0x09, 0x08, 0x05, 0x00, 0x20},/* regulator */
	/* timing   */
	{0x7F, 0x1c, 0x13, 0x00, 0x41, 0x47, 0x17,
	0x1f, 0x20, 0x03, 0x04, 0xa0},
	{0x5f, 0x00, 0x00, 0x10},/* phy ctrl */
	{0xff, 0x00, 0x06, 0x00},/* strength */
	/* pll control */
	{0x0, 0xB0, 0x1, 0x19, 0x00, 0x50, 0x48, 0x63,
	/* 2 MIPI lanes */
	0x41, 0x0f, 0x03,
	0x00, 0x14, 0x03, 0x00, 0x02, 0x00, 0x20, 0x00, 0x01 },
};


static char enter_sleep[2] = {DCS_CMD_ENTER_SLEEP_MODE, 0x00};
static char exit_sleep[2] = {DCS_CMD_EXIT_SLEEP_MODE, 0x00};
static char display_off[2] = {DCS_CMD_SET_DISPLAY_OFF, 0x00};

static char led_pwm1[2] = {DCS_CMD_SET_BRIGHTNESS, 0xFF};
static char led_pwm2[2] = {DCS_CMD_SET_CABC, 0x03};
static char led_pwm3[2] = {DCS_CMD_SET_CTRL_DISP, 0x28};
static char led_pwm4[2] = {DCS_CMD_SET_CTRL_DISP, 0x2C};

static char inverted_mode[2] = {DCS_CMD_SET_INVERTED_MODE, 0xD0};

static bool rotate_display;

static char enable_l2_cmd_1[4] = {0xFF, 0x96, 0x01, 0x01};
static char enable_l2_cmd_2[3] = {0xFF, 0x96, 0x01};
static char disable_l2_cmd_1[3] = {0xFF, 0x00, 0x00};
static char disable_l2_cmd_2[4] = {0xFF, 0x00, 0x00, 0x00};

static char cmd_00_00[2] = {0x00, 0x00};
static char cmd_00_80[2] = {0x00, 0x80};
static char cmd_00_92[2] = {0x00, 0x92};
static char cmd_C4_60[2] = {0x00, 0x60};
static char cmd_00_96[2] = {0x00, 0x96};
static char cmd_C4_40[2] = {0x00, 0x40};

static char set_2_dot_inversion_1[2] = {0x00, 0xB3};
static char set_2_dot_inversion_2[2] = {0xC0, 0x10};

/* Set scan line to 2/3 of the screen */
static char set_scanline[3] = {DCS_CMD_SET_SCAN_LINE, 0x02, 0x80};
static struct dsi_cmd_desc mot_cmd_scanline_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(set_scanline), set_scanline},
};

static struct dsi_cmd_desc mot_cmd_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm1), led_pwm1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm2), led_pwm2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm3), led_pwm3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm4), led_pwm4},
};

static struct dsi_cmd_desc mot_cmd_inverted_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm1), led_pwm1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm2), led_pwm2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm3), led_pwm3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm4), led_pwm4},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(inverted_mode), inverted_mode},
};

static struct dsi_cmd_desc mot_cmd_2_dot_on_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(enable_l2_cmd_1),
						enable_l2_cmd_1},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_00_80), cmd_00_80},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(enable_l2_cmd_2),
						enable_l2_cmd_2},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(set_2_dot_inversion_1),
						set_2_dot_inversion_1},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(set_2_dot_inversion_2),
						set_2_dot_inversion_2},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_00_92), cmd_00_92},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_C4_60), cmd_C4_60},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_00_96), cmd_00_96},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_C4_40), cmd_C4_40},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_00_80), cmd_00_80},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disable_l2_cmd_1),
						disable_l2_cmd_1},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_00_00), cmd_00_00},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disable_l2_cmd_2),
						disable_l2_cmd_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm1), led_pwm1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm2), led_pwm2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm3), led_pwm3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm4), led_pwm4},
};

static struct dsi_cmd_desc mot_cmd_2_dot_inverted_on_cmds[] = {
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(enable_l2_cmd_1),
						enable_l2_cmd_1},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_00_80), cmd_00_80},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(enable_l2_cmd_2),
						enable_l2_cmd_2},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(set_2_dot_inversion_1),
						set_2_dot_inversion_1},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(set_2_dot_inversion_2),
						set_2_dot_inversion_2},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_00_92), cmd_00_92},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_C4_60), cmd_C4_60},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_00_96), cmd_00_96},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_C4_40), cmd_C4_40},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_00_80), cmd_00_80},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disable_l2_cmd_1),
						disable_l2_cmd_1},
	{DTYPE_GEN_WRITE2, 1, 0, 0, 0, sizeof(cmd_00_00), cmd_00_00},
	{DTYPE_GEN_LWRITE, 1, 0, 0, 0, sizeof(disable_l2_cmd_2),
						disable_l2_cmd_2},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm1), led_pwm1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm2), led_pwm2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm3), led_pwm3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm4), led_pwm4},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(inverted_mode), inverted_mode},
};

static struct dsi_cmd_desc mot_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(enter_sleep), enter_sleep},
};

static int panel_enable(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *dsi_tx_buf;
	static bool exec_once;

	if (mot_panel == NULL) {
		pr_err("%s: Invalid mot_panel\n", __func__);
		return -1;
	}

	dsi_tx_buf = mot_panel->mot_tx_buf;

	/*Send display off commands only one time - IKHSS7-48508*/
	if (exec_once == false) {
		mipi_dsi_cmds_tx(mfd, dsi_tx_buf, mot_display_off_cmds,
			ARRAY_SIZE(mot_display_off_cmds));

		exec_once = true;
	}

	if (mipi_mot_get_controller_ver(mfd) <= 2) {
		if (rotate_display == true) {
			mipi_dsi_cmds_tx(mfd, dsi_tx_buf,
				mot_cmd_2_dot_inverted_on_cmds,
				ARRAY_SIZE(mot_cmd_2_dot_inverted_on_cmds));
		} else {
			mipi_dsi_cmds_tx(mfd, dsi_tx_buf,
				mot_cmd_2_dot_on_cmds,
				ARRAY_SIZE(mot_cmd_2_dot_on_cmds));
		}
	} else {
		if (rotate_display == true) {
			mipi_dsi_cmds_tx(mfd, dsi_tx_buf,
				mot_cmd_inverted_on_cmds,
				ARRAY_SIZE(mot_cmd_inverted_on_cmds));
		} else {
			mipi_dsi_cmds_tx(mfd, dsi_tx_buf,
				mot_cmd_on_cmds,
				ARRAY_SIZE(mot_cmd_on_cmds));
		}
	}

	mipi_dsi_cmds_tx(mfd, dsi_tx_buf, mot_cmd_scanline_cmds,
				ARRAY_SIZE(mot_cmd_scanline_cmds));
	return 0;
}

static int panel_disable(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *dsi_tx_buf;

	if (mot_panel == NULL) {
		pr_err("%s: Invalid mot_panel\n", __func__);
		return -1;
	}

	dsi_tx_buf =  mot_panel->mot_tx_buf;

	mipi_dsi_cmds_tx(mfd, dsi_tx_buf, mot_display_off_cmds,
					ARRAY_SIZE(mot_display_off_cmds));

	return 0;
}

static int is_valid_manufacture_id(struct msm_fb_data_type *mfd, u8 id)
{
	return id == 0x83 || id == 0xc3;
}

static int __init mipi_cmd_mot_auo_qhd_430_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	struct device_node *chosen;
	int len = 0;
	const void *prop;

	pr_debug("%s\n", __func__);

	if (msm_fb_detect_client("mipi_mot_cmd_auo_qhd_430"))
		return 0;

	mot_panel = mipi_mot_get_mot_panel();
	if (mot_panel == NULL) {
		pr_err("%s:get mot_panel failed\n", __func__);
		return -1;  /*TODO.. need to fix this */
	}

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto out;

	prop = of_get_property(chosen, "rotate_display", &len);
	if (prop && (len == sizeof(u8)) && *(u8 *)prop) {
		rotate_display = true;
		pr_info("inverted display mode on\n");
	} else {
		rotate_display = false;
	}
	of_node_put(chosen);

out:
	pinfo = &mot_panel->pinfo;

	pinfo->xres = 540;
	pinfo->yres = 960;
	pinfo->type = MIPI_CMD_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->lcdc.h_back_porch = 32;
	pinfo->lcdc.h_front_porch = 32;
	pinfo->lcdc.h_pulse_width = 32;
	pinfo->lcdc.v_back_porch = 2;
	pinfo->lcdc.v_front_porch = 2;
	pinfo->lcdc.v_pulse_width = 2;
	pinfo->lcdc.border_clr = 0;	/* blk */
	pinfo->lcdc.underflow_clr = 0xff;	/* blue */
	pinfo->lcdc.hsync_skew = 0;
	pinfo->bl_max = 100;
	pinfo->bl_min = 1;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 449723230;
	pinfo->physical_width_mm = 54;
	pinfo->physical_height_mm = 95;

	pinfo->lcd.vsync_enable = TRUE;
	pinfo->lcd.hw_vsync_mode = TRUE;
	pinfo->lcd.v_back_porch = 2;
	pinfo->lcd.v_front_porch = 2;
	pinfo->lcd.v_pulse_width = 2;
	pinfo->lcd.refx100 = 5800; /* adjust refx100 to prevent tearing */

	pinfo->mipi.mode = DSI_CMD_MODE;
	pinfo->mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;
	pinfo->mipi.vc = 0;
	pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo->mipi.data_lane0 = TRUE;
	pinfo->mipi.data_lane1 = TRUE;
	pinfo->mipi.t_clk_post = 0x04;
	pinfo->mipi.t_clk_pre = 0x1B;
	pinfo->mipi.stream = 0;	/* dma_p */
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

	mot_panel->panel_enable = panel_enable;
	mot_panel->panel_disable = panel_disable;

	/* For ESD detection information */
	mot_panel->esd_enabled = true;
	mot_panel->is_valid_manufacture_id = is_valid_manufacture_id;

	ret = mipi_mot_device_register(pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_HD);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	pr_debug("%s device registered\n", __func__);

	return ret;
}

module_init(mipi_cmd_mot_auo_qhd_430_init);
