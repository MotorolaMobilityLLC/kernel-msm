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
#include "mdp4.h"
#include "smd_dynamic_gamma.h"

#define NUMBER_BRIGHTNESS_LEVELS    30
/* 200 nits. Default brightness should be same with it in bootloader */
#define DEFAULT_BRIGHTNESS_LEVELS 19
#define DEFAULT_DELAY 0

/* Define ratio for panel level to nits level. Nits levels/panel levels = 10 */
#define PANEL_LEVEL_TO_NITS_LEVEL 10

static struct mipi_mot_panel *mot_panel;

/* DSI timing calculated for qhd 4.3 based on QCOM table */
static struct mipi_dsi_phy_ctrl dsi_cmd_mode_phy_db = {
	{0x09, 0x08, 0x05, 0x00, 0x20},/* regulator */
	/* timing   */
	{0xb4, 0x2b, 0x1d, 0x00, 0x96, 0x94, 0x21,
	0x2d, 0x21, 0x03, 0x04, 0xa0},
	{0x5f, 0x00, 0x00, 0x10},/* phy ctrl */
	{0xff, 0x00, 0x06, 0x00},/* strength */
	/* pll control */
	{0x0, 0xcb, 0x1, 0x1a, 0x00, 0x50, 0x48, 0x63,
	/* 2 MIPI lanes */
	0x41, 0x0f, 0x03,
	0x00, 0x14, 0x03, 0x00, 0x02, 0x0e, 0x01, 0x00, 0x01 },
};

static char enter_sleep[2] = {DCS_CMD_ENTER_SLEEP_MODE, 0x00};
static char display_off[2] = {DCS_CMD_SET_DISPLAY_OFF, 0x00};

static struct dsi_cmd_desc display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(enter_sleep), enter_sleep},
};

static char enable_te[2] = {DCS_CMD_SET_TEAR_ON, 0x00};

/* Dyanmic Gamma data */
#define V0 4500000

static u16 input_gamma[NUM_VOLT_PTS][NUM_COLORS] = {
	{0x51, 0x39, 0x55},
	{0xb0, 0xc7, 0xa0},
	{0xb0, 0xc5, 0xb8},
	{0xc2, 0xcb, 0xc1},
	{0x94, 0xa0, 0x8f},
	{0xad, 0xb3, 0xa6},
	{0x00e0, 0x00d7, 0x0108}
};

/* [1] */
static char etc_condition_set1_F0[3] = {0xf0, 0x5a, 0x5a};
static char etc_condition_set1_F1[3] = {0xf1, 0x5a, 0x5a};
static char etc_condition_set1_FC[3] = {0xfc, 0x5a, 0x5a};
static struct dsi_cmd_desc smd_qhd_429_cmds_1[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set1_F0), etc_condition_set1_F0},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set1_F1), etc_condition_set1_F1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set1_FC), etc_condition_set1_FC},
};
/* [2] */
static char gamma_set_update[2] = {0xfa, 0x03}; /* DTYPE_DCS_WRITE1 */

static struct dsi_cmd_desc smd_qhd_429_cmds_2[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY, RAW_GAMMA_SIZE, NULL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
		sizeof(gamma_set_update), gamma_set_update},
};
/* [3] */
static char panel_condition_set[14] = {
	0xF8,
	0x27, 0x27, 0x08, 0x08, 0x4E, 0xAA, 0x5E, 0x8A,
	0x10, 0x3F, 0x10, 0x10, 0x00,
};
static struct dsi_cmd_desc smd_qhd_429_cmds_3[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(panel_condition_set), panel_condition_set},
};
/* [4] */
static char etc_condition_set2_1[4] = {
	0xF6, 0x00, 0x84, 0x09,
};
static char etc_condition_set2_2[2] = { /* DTYPE_DCS_WRITE1 */
	0xB0, 0x09,
};
static char etc_condition_set2_3[2] = { /* DTYPE_DCS_WRITE1 */
	0xD5, 0x64,
};
static char etc_condition_set2_4[2] = { /* DTYPE_DCS_WRITE1 */
	0xB0, 0x0B,
};
static char etc_condition_set2_5[4] = {
	0xD5, 0xA4, 0x7E, 0x20,
};
static char etc_condition_set2_6[2] = { /* DTYPE_DCS_WRITE1 */
	0xB0, 0x08,
};
static char etc_condition_set2_7[2] = { /* DTYPE_DCS_WRITE1 */
	0xFD, 0xF8,
};
static char etc_condition_set2_8[2] = { /* DTYPE_DCS_WRITE1 */
	0xB0, 0x04,
};
static char etc_condition_set2_9[2] = { /* DTYPE_DCS_WRITE1 */
	0xF2, 0x4D,
};
static char etc_condition_set2_A[2] = { /* DTYPE_DCS_WRITE1 */
	0xB0, 0x05,
};
static char etc_condition_set2_B[2] = { /* DTYPE_DCS_WRITE1 */
	0xFD, 0x1F,
};
static char etc_condition_set2_C[4] = {
	0xB1, 0x01, 0x00, 0x16,
};
static struct dsi_cmd_desc smd_qhd_429_cmds_4[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_1), etc_condition_set2_1},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_2), etc_condition_set2_2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_3), etc_condition_set2_3},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_4), etc_condition_set2_4},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_5), etc_condition_set2_5},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_6), etc_condition_set2_6},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_7), etc_condition_set2_7},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_8), etc_condition_set2_8},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_9), etc_condition_set2_9},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_A), etc_condition_set2_A},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_B), etc_condition_set2_B},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
			sizeof(etc_condition_set2_C), etc_condition_set2_C},
};
static struct dsi_cmd_desc smd_qhd_429_cmds_5[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 1, sizeof(enable_te), enable_te}
};

/* win size = 540 * 960 */
static char mem_window_set_1[5] = { /* DTYPE_DCS_LWRITE */
	0x2A, 0x00, 0x1E, 0x02, 0x39
};
static char mem_window_set_2[5] = { /* DTYPE_DCS_LWRITE */
	0x2B, 0x00, 0x00, 0x03, 0xBF
};
static char vinit_set[2] = { /* DTYPE_DCS_WRITE1 */
	0xD1, 0x8A
};
static struct dsi_cmd_desc smd_qhd_429_cmds_6[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
			sizeof(mem_window_set_1), mem_window_set_1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
			sizeof(mem_window_set_2), mem_window_set_2},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
			sizeof(vinit_set), vinit_set},
};
/* [7] */
static char acl_default_set[29] = { /* 70% */
	0xC1,
	0x47, 0x53, 0x13, 0x53,
	0x00, 0x00, 0x01, 0xDF,
	0x00, 0x00, 0x03, 0x1F,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x02, 0x03,
	0x07, 0x0E, 0x14, 0x1C,
	0x24, 0x2D, 0x2D, 0x00,
};

static char ACL_enable_disable_settings[2] = {0xc0, 0x00};

static struct dsi_cmd_desc smd_qhd_429_cmds_7[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(acl_default_set), acl_default_set},
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
		sizeof(ACL_enable_disable_settings),
			ACL_enable_disable_settings},
};

static struct dsi_cmd_desc acl_enable_disable[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(ACL_enable_disable_settings),
			ACL_enable_disable_settings}
};

/* default elvss set, update based on 0xD4 */
static char elvss_output_set[5] = {
	0xB2,
	0x08, 0x08, 0x08, 0x08,
};
static struct dsi_cmd_desc elvss_set_cmd[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(elvss_output_set), elvss_output_set}
};

static char d4_val[2] = {0xD4, 0x00};
static struct dsi_cmd_desc get_d4_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, DEFAULT_DELAY, sizeof(d4_val), d4_val
};

static u8 elvss_value = 0x08;

/*
 * Framework brightness <--> panel nit mapping table
 * Kernel brigheness is (framework brightness) / 2,
 * So maximum is 127(255/2).
 */
static u8 backlight_curve_mapping[FB_BACKLIGHT_LEVELS] = {
	/* Framework level / 2: from 0 to 15 */
	0,   0,  0,  0,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  3,  3,
	/* 16 to 31 */
	3,   3,  4,  4,  4,  4,  5,  5,  5,  5,  6,  6,  6,  6,  6,  7,
	/* 32 to 47 */
	7,   7,  7,  8,  8,  8,  8,  9,  9,  9,  9, 10, 10, 10, 10, 10,
	/* 48 to 63 */
	11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14,
	/* 64 to 79 */
	14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18,
	/* 80 to 95 */
	18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 22, 22,
	/* 96 to 111 */
	22, 22, 22, 23, 23, 23, 23, 24, 24, 24, 24, 25, 25, 25, 25, 26,
	/* 112 to 127 */
	26, 26, 26, 26, 27, 27, 27, 27, 28, 28, 28, 28, 29, 29, 29, 29
};

static void enable_acl(struct msm_fb_data_type *mfd)
{
	/* Write the value only if the display is enable and powerd on */
	if ((mfd->op_enable != 0) && (mfd->panel_power_on != 0)) {
		mipi_set_tx_power_mode(0);
		mipi_mot_tx_cmds(&acl_enable_disable[0],
					ARRAY_SIZE(acl_enable_disable));
	}
}

static struct dsi_cmd_desc set_brightness_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, RAW_GAMMA_SIZE, NULL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(gamma_set_update), gamma_set_update}
};

static void mipi_mot_get_mtpset4(struct msm_fb_data_type *mfd)
{
	static u8 d4_value = INVALID_VALUE;
	int ret;

	if (d4_value == INVALID_VALUE) {
		ret = mipi_mot_rx_cmd(&get_d4_cmd, &d4_value, 1);
		if (ret != 1)
			pr_err("%s: failed to read d4_value. ret =%d\n",
						__func__, ret);
		else {
			elvss_value = d4_value & 0x3F;

			pr_info("%s: elvss = 0x%2x\n", __func__, elvss_value);
		}
	}
}

static int panel_enable(struct msm_fb_data_type *mfd)
{
	int i;
	int idx;
	static bool first_boot = true;

	mipi_mot_tx_cmds(smd_qhd_429_cmds_1, ARRAY_SIZE(smd_qhd_429_cmds_1));

	/* Read D4h for elvss and apply gamma */
	mipi_mot_get_mtpset4(mfd);

	if (first_boot) {
		/*
		 * Kernel bootup. Set it to default nit which should
		 * be same with it in bootloader.
		 */
		idx = DEFAULT_BRIGHTNESS_LEVELS;

		/* Calculate gamma setting */
		mipi_mot_get_raw_mtp(mfd);
		mipi_mot_dynamic_gamma_calc(V0, 0xfa, 0x02, input_gamma);
		first_boot = false;
	} else
		idx = mfd->bl_level;

	if (idx < 0)
		idx = 0;
	if (idx > NUMBER_BRIGHTNESS_LEVELS - 1)
		idx = NUMBER_BRIGHTNESS_LEVELS - 1;

	/* Mapping 1 ~ 30 level to 10 ~ 300 nits table */
	idx = (idx + 1) * PANEL_LEVEL_TO_NITS_LEVEL - 1;

	/* gamma */
	smd_qhd_429_cmds_2[0].payload =
		mipi_mot_get_gamma_setting(mfd, idx);
	mipi_mot_tx_cmds(smd_qhd_429_cmds_2, ARRAY_SIZE(smd_qhd_429_cmds_2));

	mipi_mot_tx_cmds(smd_qhd_429_cmds_3, ARRAY_SIZE(smd_qhd_429_cmds_3));
	mipi_mot_tx_cmds(smd_qhd_429_cmds_4, ARRAY_SIZE(smd_qhd_429_cmds_4));

	mipi_mot_tx_cmds(&smd_qhd_429_cmds_2[0],
					ARRAY_SIZE(smd_qhd_429_cmds_2));
	mipi_mot_tx_cmds(&smd_qhd_429_cmds_3[0],
					ARRAY_SIZE(smd_qhd_429_cmds_3));
	mipi_mot_tx_cmds(&smd_qhd_429_cmds_4[0],
					ARRAY_SIZE(smd_qhd_429_cmds_4));
	/* elvss */
	for (i = 1; i < 5; i++)
		elvss_output_set[i] = elvss_value;

	if (mot_panel->elvss_tth_support_present &&
		mot_panel->elvss_tth_status)
		for (i = 1; i < 5; i++)
			elvss_output_set[i] = elvss_value + 0xF;

	mipi_mot_tx_cmds(&elvss_set_cmd[0], ARRAY_SIZE(elvss_set_cmd));
	mipi_mot_panel_exit_sleep();
	mipi_mot_tx_cmds(&smd_qhd_429_cmds_5[0],
					ARRAY_SIZE(smd_qhd_429_cmds_5));
	mipi_mot_tx_cmds(&smd_qhd_429_cmds_6[0],
					ARRAY_SIZE(smd_qhd_429_cmds_6));
	/* acl */
	ACL_enable_disable_settings[1] = mot_panel->acl_enabled;
	mipi_mot_tx_cmds(&smd_qhd_429_cmds_7[0],
					ARRAY_SIZE(smd_qhd_429_cmds_7));

	return 0;
}

static int panel_disable(struct msm_fb_data_type *mfd)
{
	mipi_mot_tx_cmds(&display_off_cmds[0], ARRAY_SIZE(display_off_cmds));

	return 0;
}

static void panel_set_backlight(struct msm_fb_data_type *mfd)
{
	static int bl_level_old;
	int idx = 0;

	pr_debug("%s(bl_level=%d)\n", __func__, (s32)mfd->bl_level);

	if (!mfd->panel_power_on)
		return;

	if (bl_level_old == mfd->bl_level)
		return;

	/* should already be in panel bl range */
	idx = mfd->bl_level;

	if (idx < 0)
		idx = 0;
	if (idx > NUMBER_BRIGHTNESS_LEVELS - 1)
		idx = NUMBER_BRIGHTNESS_LEVELS - 1;

	/* Mapping 1 ~ 30 level to 10 ~ 300 nits table */
	idx = (idx + 1) * PANEL_LEVEL_TO_NITS_LEVEL - 1;

	pr_debug("%s(idx=%d)\n", __func__, (s32)idx);
	set_brightness_cmds[0].payload =
		mipi_mot_get_gamma_setting(mfd, idx);

	mutex_lock(&mfd->dma->ov_mutex);
	mipi_set_tx_power_mode(0);
	mipi_mot_tx_cmds(&set_brightness_cmds[0],
				ARRAY_SIZE(set_brightness_cmds));

	bl_level_old = mfd->bl_level;
	mutex_unlock(&mfd->dma->ov_mutex);

	pr_debug("%s(%d) completed\n", __func__, (s32)mfd->bl_level);

	return;
}

static void panel_set_backlight_curve(struct msm_fb_data_type *mfd)
{
	struct fb_info *fbi;
	int i;

	pr_debug("panel backlight curve is called.\n");
	fbi = mfd->fbi;
	if (!fbi) {
		pr_warning("%s: fb_info is null!\n", __func__);
		return;
	}

	for (i = 0; i < FB_BACKLIGHT_LEVELS; i++)
		fbi->bl_curve[i] = backlight_curve_mapping[i];
}

static int __init mipi_cmd_mot_smd_qhd_429_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	pr_debug("%s\n", __func__);

	/* comment it out to force to use this panel */
	if (msm_fb_detect_client("mipi_mot_cmd_smd_qhd_429"))
		return 0;

	mot_panel = mipi_mot_get_mot_panel();
	if (mot_panel == NULL) {
		pr_err("%s:get mot_panel failed\n", __func__);
		return -EINVAL;  /*TODO.. need to fix this */
	}

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
	pinfo->bl_max = NUMBER_BRIGHTNESS_LEVELS - 1;
	pinfo->bl_min = 0;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 460000000;
	pinfo->physical_width_mm = 54;
	pinfo->physical_height_mm = 95;

	pinfo->lcd.vsync_enable = TRUE;
	pinfo->lcd.hw_vsync_mode = TRUE;
	pinfo->lcd.v_back_porch = 2;	/* TODO: check later */
	pinfo->lcd.v_front_porch = 2;
	pinfo->lcd.v_pulse_width = 2;
	pinfo->lcd.refx100 = 6000; /* adjust refx100 to prevent tearing */

	pinfo->mipi.mode = DSI_CMD_MODE;
	pinfo->mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;
	pinfo->mipi.vc = 0;
	pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo->mipi.data_lane0 = TRUE;
	pinfo->mipi.data_lane1 = TRUE;
	pinfo->mipi.t_clk_post = 0x19;  /* DSI_CLKOUT_TIMING_CTRL >>8 */
	pinfo->mipi.t_clk_pre = 0x2f;	/* DSI_CLKOUT_TIMING_CTRL && 0XFF */
	pinfo->mipi.stream = 0;	/* dma_p */
	pinfo->mipi.mdp_trigger = DSI_CMD_TRIGGER_NONE;
	pinfo->mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo->mipi.te_sel = 1;
	pinfo->mipi.interleave_max = 1;
	pinfo->mipi.insert_dcs_cmd = TRUE;
	pinfo->mipi.wr_mem_continue = 0x3c;
	pinfo->mipi.wr_mem_start = 0x2c;
	pinfo->mipi.dsi_phy_db = &dsi_cmd_mode_phy_db;
	pinfo->mipi.esc_byte_ratio = 2;
	pinfo->mipi.tx_eot_append = 0x01;
	pinfo->mipi.rx_eot_ignore = 0;

	mot_panel->acl_support_present = TRUE;
	mot_panel->acl_enabled = FALSE; /* By default the ACL is disbled. */

	mot_panel->elvss_tth_support_present = TRUE;

	mot_panel->panel_enable = panel_enable;
	mot_panel->panel_disable = panel_disable;
	mot_panel->exit_sleep_wait = 20;
	mot_panel->set_backlight = panel_set_backlight;
	mot_panel->set_backlight_curve = panel_set_backlight_curve;
	mot_panel->enable_acl = enable_acl;
	mot_panel->esd_enabled = true;

	ret = mipi_mot_device_register(pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_HD);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	pr_debug("%s device registered\n", __func__);

	return ret;
}

module_init(mipi_cmd_mot_smd_qhd_429_init);
