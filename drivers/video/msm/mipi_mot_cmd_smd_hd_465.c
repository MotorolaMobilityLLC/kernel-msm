/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2013, Motorola Mobility, LLC.
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

#define MAX_BRIGHTNESS_LEVEL 255
#define MIN_BRIGHTNESS_LEVEL 10
/* 150 nits. Default brightness should be same with it in bootloader */
#define DEFAULT_BRIGHTNESS  0x7f

static struct mipi_mot_panel *mot_panel;

static struct mipi_dsi_phy_ctrl dsi_cmd_mode_phy_db = {
	{0x09, 0x08, 0x05, 0x00, 0x20},	/* regulator */
	/* timing   */
	{0xb0, 0x8c, 0x1a, 0x00, 0x94, 0x92, 0x1e,
	0x8e, 0x1e, 0x03, 0x04, 0xa0},
	{0x5f, 0x00, 0x00, 0x10},	/* phy ctrl */
	{0xff, 0x00, 0x06, 0x00},	/* strength */
	/* pll control */
	{0x00, 0xa3, 0x31, 0xda, 0x00, 0x50, 0x48, 0x63,
	/* 4 MIPI lanes */
	0x41, 0x0f, 0x03,
	0x00, 0x14, 0x03, 0x0, 0x02, 0x0e, 0x01, 0x00, 0x01},
};

static char enter_sleep[2] = {DCS_CMD_ENTER_SLEEP_MODE, 0x00};
static char display_off[2] = {DCS_CMD_SET_DISPLAY_OFF, 0x00};
static char unlock_lvl_2[3] = {0xf0, 0x5a, 0x5a};
static char unlock_lvl_mtp[3] = {0xf1, 0x5a, 0x5a};
static char unlock_lvl_3[3] = {0xfc, 0x5a, 0x5a};
static char switch_pwr_to_mem_1[3] = {0xfd, 0x10, 0xfc};
static char switch_pwr_to_mem_2[3] = {0xc4, 0x07, 0x01};
static char enable_te[2] = {DCS_CMD_SET_TEAR_ON, 0x00};
static char normal_mode_on[2] = {DCS_CMD_SET_NORMAL_MODE_ON, 0x00};

#define DEFAULT_DELAY 1

static char acl_default_setting[6] = {0xB5, 0x03, 0x6B, 0x45, 0x35, 0x26};
static char acl_enable_disable_settings[2] = {0x55, 0x00};
static char p3_off[2] = {0xb0, 0x02};
static char p3_data[2] = {0xb1, 0x1a};
static char C8_offset[] = {0xB0, 0x00};
static char C8_data[] = {0xC8,
		0x01, 0x4B, 0x01, 0x49, 0x01, 0x80, 0xD1, 0xD1, 0xD1, 0xCC,
		0xCC, 0xCC, 0xB1, 0xB0, 0xB1, 0xB7, 0xB8, 0xB8, 0xC8, 0xCD,
		0xCF, 0xBF, 0xC5, 0xC9, 0x8D, 0xA2, 0x9C, 0xE0, 0xDD, 0xD2,
		0x0A, 0x0B, 0x0B, 0x01, 0x3C, 0x01, 0x3C, 0x01, 0x71, 0xD5,
		0xD3, 0xD3, 0xCF, 0xCF, 0xCF, 0xB1, 0xB1, 0xB1, 0xB7, 0xB8,
		0xBB, 0xC9, 0xCD, 0xCF, 0xBF, 0xC5, 0xC9, 0x8D, 0xA2, 0x9C,
		0xE0, 0xDD, 0xD2, 0x0A, 0x0B, 0x0B, 0x01, 0x2C, 0x01, 0x2C,
		0x01, 0x5D, 0xD3, 0xD3, 0xD3, 0xCF, 0xCF, 0xCF, 0xB1, 0xB1,
		0xB1, 0xB7, 0xB8, 0xBB, 0xC9, 0xCD, 0xCF, 0xBF, 0xC5, 0xC9,
		0x8D, 0xA2, 0x9C, 0xE0, 0xDD, 0xD2, 0x0A, 0x0B, 0x0B };

static char C9_offset[] = {0xB0, 0x00};
static char C9_data[] = {0xC9,
		0x01, 0x1B, 0x01, 0x1B, 0x01, 0x4B, 0xD3, 0xD3, 0xD3, 0xCF,
		0xCF, 0xCF, 0xB1, 0xB1, 0xB1, 0xB7, 0xB8, 0xBB, 0xC9, 0xCD,
		0xCF, 0xBF, 0xC5, 0xC9, 0x8D, 0xA2, 0x9C, 0xE0, 0xDD, 0xD2,
		0x0A, 0x0B, 0x0B, 0x01, 0x06, 0x01, 0x07, 0x01, 0x30, 0xD3,
		0xD3, 0xD3, 0xCF, 0xCF, 0xCF, 0xB1, 0xB1, 0xB1, 0xB7, 0xB8,
		0xBB, 0xC9, 0xCD, 0xCF, 0xBF, 0xC5, 0xC9, 0x8D, 0xA2, 0x9C,
		0xE0, 0xDD, 0xD2, 0x0A, 0x0B, 0x0B, 0x00, 0xFF, 0x00, 0xFF,
		0x01, 0x28, 0xD3, 0xD3, 0xD3, 0xCF, 0xCF, 0xCF, 0xB1, 0xB1,
		0xB1, 0xB7, 0xB8, 0xBB, 0xC9, 0xCD, 0xCF, 0xBF, 0xC5, 0xC9,
		0x8D, 0xA2, 0x9C, 0xE0, 0xDD, 0xD2, 0x0A, 0x0B, 0x0B, 0x00,
		0xD3, 0x00, 0xD7, 0x00, 0xF7, 0xD3, 0xD3, 0xD3, 0xCF, 0xCF,
		0xCF, 0xB1, 0xB1, 0xB1, 0xB7, 0xB8, 0xBB, 0xC9, 0xCD, 0xCF,
		0xBF, 0xC5, 0xC9, 0x8D, 0xA2, 0x9C, 0xE0, 0xDD, 0xD2, 0x0A,
		0x0B, 0x0B };

static char C7_reg[2] = {0xC7, 0x07};

static char disp_ctrl[2] = {0x53, 0x20};
/* default 150 nits */
static char brightness_ctrl[2] = {0x51, 0x7f};

static char p4_select[2] = {0xb0, 0x03};
static char refresh_rate[2] = {0xbb, 0x80};
static char p22_select[2] = {0xb0, 0x15};
static char ltps_set[6] = {0xcb, 0x87, 0x41, 0x87, 0x41, 0x87};
static struct mipi_mot_cmd_seq refresh_rate_seq[] = {
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE1, 0, p4_select),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE1, 0, refresh_rate),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE1, 0, p22_select),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, ltps_set),
};

static char normal_col[] = {0x2a, 0x00, 0x00, 0x02, 0xcf};
static char normal_row[] = {0x2b, 0x00, 0x00, 0x04, 0xff};

static struct mipi_mot_cmd_seq acl_enable_disable_seq[] = {
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, DEFAULT_DELAY,
			acl_enable_disable_settings),
};

static struct mipi_mot_cmd_seq set_brightness_seq[] = {
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE1, DEFAULT_DELAY, brightness_ctrl),
};

static struct mipi_mot_cmd_seq unlock_mtp_seq[] = {
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, DEFAULT_DELAY, unlock_lvl_2),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, DEFAULT_DELAY, unlock_lvl_mtp),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, DEFAULT_DELAY, unlock_lvl_3),
};

static struct mipi_mot_cmd_seq image_retention_wa_seq[] = {
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE,
			DEFAULT_DELAY, switch_pwr_to_mem_1),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE,
			DEFAULT_DELAY, switch_pwr_to_mem_2),
};

static struct mipi_mot_cmd_seq set_window_size[] = {
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, DEFAULT_DELAY, normal_col),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, DEFAULT_DELAY, normal_row),
};

static struct mipi_mot_cmd_seq brightness_wa_seq[] = {
	/* C8, C9, C7 sequence only for non-mtped panels */
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE1, DEFAULT_DELAY, C8_offset),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, DEFAULT_DELAY, C8_data),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE1, DEFAULT_DELAY, C9_offset),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, DEFAULT_DELAY, C9_data),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, DEFAULT_DELAY, C7_reg),
};

static struct mipi_mot_cmd_seq aid_wa_seq[] = {
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE1, DEFAULT_DELAY, p3_off),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE1, DEFAULT_DELAY, p3_data),
};

static char undo_partial_rows[] = {0x30, 0x00, 0x00, 0x04, 0xff};

/* Settings for correct 2Ch shift issue */
static char small_col[] = {0x2a, 0x02, 0xc8, 0x02, 0xcf};
static char small_row[] = {0x2b, 0x04, 0xfe, 0x04, 0xff};
static char frame[] = {
	0x2c,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static struct mipi_mot_cmd_seq correct_shift_seq[] = {
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, small_col),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, small_row),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, frame),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, normal_col),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, normal_row),
};

static int is_evt0_sample(struct msm_fb_data_type *mfd);
static int is_es1_evt0_sample(struct msm_fb_data_type *);
static int is_acl_default_setting_needed(struct msm_fb_data_type *);
static int is_correct_shift_for_aod_needed(struct msm_fb_data_type *);

static struct mipi_mot_cmd_seq smd_hd_465_cfg_seq[] = {
	MIPI_MOT_EXEC_SEQ(NULL, unlock_mtp_seq),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE1, DEFAULT_DELAY, enable_te),
	MIPI_MOT_EXEC_SEQ(is_evt0_sample, image_retention_wa_seq),
	MIPI_MOT_TX_DEF(is_acl_default_setting_needed, DTYPE_DCS_LWRITE,
			DEFAULT_DELAY, acl_default_setting),
	/* exit partial mode */
	MIPI_MOT_TX_DEF(AOD_SUPPORTED, DTYPE_DCS_WRITE, 0, normal_mode_on),
	MIPI_MOT_TX_DEF(AOD_SUPPORTED, DTYPE_DCS_LWRITE, 0,
		undo_partial_rows),
	/* C8, C9, C7 sequence only for non-mtped panels */
	MIPI_MOT_EXEC_SEQ(is_es1_evt0_sample, brightness_wa_seq),
	MIPI_MOT_TX_DEF(is_evt0_sample, DTYPE_DCS_LWRITE,
			DEFAULT_DELAY, disp_ctrl),
	MIPI_MOT_EXEC_SEQ(NULL, set_brightness_seq),
	MIPI_MOT_EXEC_SEQ(NULL, acl_enable_disable_seq),
	MIPI_MOT_EXEC_SEQ(is_es1_evt0_sample, aid_wa_seq),
	MIPI_MOT_EXEC_SEQ(NULL, set_window_size),
	MIPI_MOT_EXEC_SEQ(AOD_SUPPORTED, refresh_rate_seq),
};

static struct mipi_mot_cmd_seq smd_hd_465_init_seq[] = {
	MIPI_MOT_TX_EXIT_SLEEP(NULL),
	MIPI_MOT_EXEC_SEQ(NULL, smd_hd_465_cfg_seq),
};

static struct mipi_mot_cmd_seq smd_hd_465_disp_off_seq[] = {
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE, DEFAULT_DELAY, display_off),
	MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_WRITE, 120, enter_sleep),
};

static struct mipi_mot_cmd_seq smd_hd_465_en_from_partial_seq[] = {
	MIPI_MOT_EXEC_SEQ(NULL, smd_hd_465_cfg_seq),
	MIPI_MOT_EXEC_SEQ(is_correct_shift_for_aod_needed,
		correct_shift_seq),
};

static int is_evt0_sample(struct msm_fb_data_type *mfd)
{
	if ((mipi_mot_get_controller_ver(mfd) < 5) &&
			(mipi_mot_get_controller_drv_ver(mfd) == 1))
		return 1;
	else
		return 0;
}

static int is_es1_evt0_sample(struct msm_fb_data_type *mfd)
{
	if ((mipi_mot_get_controller_ver(mfd) < 2) &&
			(mipi_mot_get_controller_drv_ver(mfd) == 1))
		return 1;
	else
		return 0;
}

static int is_acl_default_setting_needed(struct msm_fb_data_type *mfd)
{
	if ((mipi_mot_get_controller_ver(mfd) < 3) &&
			(mipi_mot_get_controller_drv_ver(mfd) == 1))
		return 1;
	else
		return 0;
}

static int is_correct_shift_for_aod_needed(struct msm_fb_data_type *mfd)
{
	return is_aod_supported(mfd) && is_evt0_sample(mfd);
}

static void enable_acl(struct msm_fb_data_type *mfd)
{
	/* Write the value only if the display is enable and powered on */
	if ((mfd->op_enable != 0) && (mfd->panel_power_on != 0)) {
		acl_enable_disable_settings[1] =
				(mot_panel->acl_enabled == 1) ? 3 : 0;
		mipi_set_tx_power_mode(0);
		mipi_mot_exec_cmd_seq(mfd, acl_enable_disable_seq,
				ARRAY_SIZE(acl_enable_disable_seq));
	}
}

static int panel_enable(struct msm_fb_data_type *mfd)
{
	int idx;
	static bool first_boot = true;

	if (first_boot) {
		/*
		 * Kernel bootup. Set it to default nit which should
		 * be same with it in bootloader.
		 */
		idx = DEFAULT_BRIGHTNESS;
		first_boot = false;
	} else
		idx = mfd->bl_level;

	if (idx < MIN_BRIGHTNESS_LEVEL)
		idx = MIN_BRIGHTNESS_LEVEL;
	if (idx > MAX_BRIGHTNESS_LEVEL)
		idx = MAX_BRIGHTNESS_LEVEL;

	brightness_ctrl[1] = idx;
	acl_enable_disable_settings[1] = (mot_panel->acl_enabled == 1) ? 3 : 0;

	mipi_mot_exec_cmd_seq(mfd, smd_hd_465_init_seq,
			ARRAY_SIZE(smd_hd_465_init_seq));

	return 0;
}

static int panel_disable(struct msm_fb_data_type *mfd)
{
	mipi_mot_exec_cmd_seq(mfd, smd_hd_465_disp_off_seq,
			ARRAY_SIZE(smd_hd_465_disp_off_seq));
	return 0;
}

/* Work around for the EVT0 display sample issue */
static int panel_enable_wa(struct msm_fb_data_type *mfd)
{
	if (is_evt0_sample(mfd))
		mipi_set_mem_start_mem_cont(0x3c, 0x3c);

	return 0;
}
static void panel_set_backlight(struct msm_fb_data_type *mfd)
{

	static int bl_level_old = -1;
	int idx = 0;

	pr_debug("%s +(%d)\n", __func__, (s32)mfd->bl_level);
	if (!mfd->panel_power_on)
		return;

	if (bl_level_old == mfd->bl_level)
		return;

	idx = mfd->bl_level;
	if (mfd->bl_level < MIN_BRIGHTNESS_LEVEL)
		idx = MIN_BRIGHTNESS_LEVEL;
	if (mfd->bl_level > MAX_BRIGHTNESS_LEVEL)
		idx = MAX_BRIGHTNESS_LEVEL;

	brightness_ctrl[1] = idx;

	mutex_lock(&mfd->dma->ov_mutex);
	mipi_set_tx_power_mode(0);
	mipi_mot_exec_cmd_seq(mfd, set_brightness_seq,
			ARRAY_SIZE(set_brightness_seq));
	bl_level_old = mfd->bl_level;
	mutex_unlock(&mfd->dma->ov_mutex);

	pr_debug("%s -(%d)\n", __func__, idx);
	return;
}

static void panel_en_from_partial(struct msm_fb_data_type *mfd)
{
	mipi_mot_exec_cmd_seq(mfd, smd_hd_465_en_from_partial_seq,
			ARRAY_SIZE(smd_hd_465_en_from_partial_seq));
}

static int __init mipi_mot_cmd_smd_hd_465_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	pr_debug("%s\n", __func__);

	if (msm_fb_detect_client("mipi_mot_cmd_smd_hd_465"))
		return 0;

	mot_panel = mipi_mot_get_mot_panel();
	if (mot_panel == NULL) {
		pr_err("%s:get mot_panel failed\n", __func__);
		return -EAGAIN;  /*TODO.. need to fix this */
	}

	pinfo = &mot_panel->pinfo;

	pinfo->xres = 720;
	pinfo->yres = 1280;
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
	pinfo->lcdc.border_clr = 0;     /* blk */
	pinfo->lcdc.underflow_clr = 0xff;       /* blue */
	pinfo->lcdc.hsync_skew = 0;
	pinfo->bl_max = MAX_BRIGHTNESS_LEVEL;
	pinfo->bl_min = MIN_BRIGHTNESS_LEVEL; /* 0 is dark when use 51h */
	pinfo->fb_num = 2;
	pinfo->clk_rate = 420000000;
	pinfo->physical_width_mm = 58;
	pinfo->physical_height_mm = 103;

	pinfo->lcd.vsync_enable = TRUE;
	pinfo->lcd.hw_vsync_mode = TRUE;
	pinfo->lcd.v_back_porch = 2;
	pinfo->lcd.v_front_porch = 2;
	pinfo->lcd.v_pulse_width = 2;
	pinfo->lcd.refx100 = 6000;

	pinfo->mipi.mode = DSI_CMD_MODE;
	pinfo->mipi.dst_format = DSI_CMD_DST_FORMAT_RGB888;
	pinfo->mipi.vc = 0;
	pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo->mipi.data_lane0 = TRUE;
	pinfo->mipi.data_lane1 = TRUE;
	pinfo->mipi.data_lane2 = TRUE;
	pinfo->mipi.data_lane3 = TRUE;
	pinfo->mipi.t_clk_post = 0x19;  /* DSI_CLKOUT_TIMING_CTRL >>8 */
	pinfo->mipi.t_clk_pre = 0x2f;   /* DSI_CLKOUT_TIMING_CTRL && 0XFF */
	pinfo->mipi.stream = 0; /* dma_p */
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

	mot_panel->panel_enable = panel_enable;
	mot_panel->panel_disable = panel_disable;
	mot_panel->panel_enable_wa = panel_enable_wa;
	mot_panel->set_backlight = panel_set_backlight;
	mot_panel->enable_acl = enable_acl;
	mot_panel->enable_te = mipi_mot_set_tear;
	mot_panel->panel_en_from_partial = panel_en_from_partial;

	/* For ESD detection information */
	mot_panel->esd_enabled = true;

	ret = mipi_mot_device_register(pinfo, MIPI_DSI_PRIM, MIPI_DSI_PANEL_HD);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	pr_debug("%s device registered\n", __func__);

	return ret;
}

module_init(mipi_mot_cmd_smd_hd_465_init);
