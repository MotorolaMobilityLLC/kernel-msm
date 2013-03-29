/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2013, Motorola Mobility,LLC.
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

#define NUMBER_BRIGHTNESS_LEVELS 30

static struct mipi_mot_panel *mot_panel;

/* 720x1280/60 fps/4 lanes/228.5 Mhz(data rate - 457Mb/s) bit clk */
static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
	/* regulator */
	{0x09, 0x08, 0x05, 0x00, 0x20},
	/* timing */
	{0x81, 0x31, 0x13, 0x0, 0x42, 0x46, 0x18,
	0x35, 0x20, 0x03, 0x04, 0x0},
	/* phy ctrl */
	{0x5f, 0x00, 0x00, 0x10},
	/* strength */
	{0xff, 0x00, 0x06, 0x00},
	/* pll control */
	{0x0, 0xc8, 0x31, 0xda, 0x00, 0x50, 0x48, 0x63,
	0x41, 0x0f, 0x03,
	0x00, 0x14, 0x03, 0x00, 0x02, 0x0e, 0x01, 0x00, 0x01},
};

static void enable_acl(struct msm_fb_data_type *mfd)
{
	pr_debug("%s(dummy_panel): is called\n", __func__);
}


static int panel_enable(struct msm_fb_data_type *mfd)
{
	pr_debug("%s(dummy_panel): is called\n", __func__);
	return 0;
}

static int panel_disable(struct msm_fb_data_type *mfd)
{
	pr_debug("%s(dummy_panel): is called\n", __func__);
	return 0;
}

static void panel_set_backlight(struct msm_fb_data_type *mfd)
{
	pr_debug("%s(%d)(dummy_panel) is called\n",
					__func__, (s32)mfd->bl_level);
	return;
}

static int __init mipi_video_mot_hd_pt_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	pr_debug("%s\n", __func__);

	if (msm_fb_detect_client("mipi_mot_video_hd_dummy"))
		return 0;

	mot_panel = mipi_mot_get_mot_panel();
	if (mot_panel == NULL) {
		pr_err("%s:get mot_panel failed\n", __func__);
		return -EAGAIN;  /*TODO.. need to fix this */
	}

	pinfo = &mot_panel->pinfo;

	pinfo->xres = 720;
	pinfo->yres = 1280;
	pinfo->type = MIPI_VIDEO_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;

	pinfo->lcdc.h_back_porch = 12;
	pinfo->mipi.hbp_power_stop = FALSE;

	pinfo->lcdc.h_front_porch = 236;
	pinfo->mipi.hfp_power_stop = FALSE;

	pinfo->lcdc.h_pulse_width = 12;
	pinfo->mipi.hsa_power_stop = FALSE;

	/*
	 * qcom requires the sum of the vertical back porch + pulse width
	 * should >= 5, otherwise have underrun in some upscaling use cases
	 */
	pinfo->lcdc.v_back_porch = 3;
	pinfo->lcdc.v_front_porch = 13;
	pinfo->lcdc.v_pulse_width = 2;

	pinfo->lcdc.border_clr = 0x0;
	pinfo->lcdc.underflow_clr = 0xff;
	pinfo->lcdc.hsync_skew = 0;
	pinfo->bl_max = NUMBER_BRIGHTNESS_LEVELS - 1;
	pinfo->bl_min = 0;
	pinfo->fb_num = 2;
	pinfo->physical_width_mm = 58;
	pinfo->physical_height_mm = 103;

	pinfo->mipi.mode = DSI_VIDEO_MODE;
	pinfo->mipi.pulse_mode_hsa_he = TRUE;


	pinfo->mipi.eof_bllp_power_stop = TRUE;
	pinfo->mipi.bllp_power_stop = TRUE;
	pinfo->mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
	pinfo->mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo->mipi.vc = 0;
	pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo->mipi.data_lane0 = TRUE;
	pinfo->mipi.data_lane1 = TRUE;
	pinfo->mipi.data_lane2 = TRUE;
	pinfo->mipi.data_lane3 = TRUE;
	pinfo->mipi.force_clk_lane_hs = TRUE;


	pinfo->mipi.tx_eot_append = TRUE;
	pinfo->mipi.t_clk_post = 4;
	pinfo->mipi.t_clk_pre = 28;
	pinfo->mipi.stream = 0; /* dma_p */
	pinfo->mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo->mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo->mipi.dsi_phy_db = &dsi_video_mode_phy_db;
	pinfo->mipi.esc_byte_ratio = 4;

	mot_panel->acl_support_present = FALSE;
	mot_panel->acl_enabled = FALSE; /* By default the ACL is disabled. */

	mot_panel->elvss_tth_support_present = FALSE;

	mot_panel->panel_enable = panel_enable;
	mot_panel->panel_disable = panel_disable;
	mot_panel->set_backlight = panel_set_backlight;
	/*
	 * panel's state need to set to on because mot_panel->panel_on is NULL
	 * and disp_on (0x28) be a part of init seq
	 */
	mot_panel->panel_on = NULL;
	mot_panel->panel_off = panel_disable;
	mot_panel->is_no_disp = true;

	atomic_set(&mot_panel->state, MOT_PANEL_ON);

	mot_panel->enable_acl = enable_acl;

	/* For ESD detection information */
	mot_panel->esd_enabled = false;

	ret = mipi_mot_device_register(pinfo, MIPI_DSI_PRIM, MIPI_DSI_PANEL_HD);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	pr_debug("%s device registered\n", __func__);

	return ret;
}

module_init(mipi_video_mot_hd_pt_init);
