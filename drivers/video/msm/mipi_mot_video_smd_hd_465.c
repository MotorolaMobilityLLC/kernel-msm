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

#define NUMBER_BRIGHTNESS_LEVELS 30

/* 200 nits. Default brightness should be same with it in bootloader */
#define DEFAULT_BRIGHTNESS_LEVELS 19

/* Define ratio for panel level to nits level. Nits levels/panel levels = 10 */
#define PANEL_LEVEL_TO_NITS_LEVEL 10

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

static char enter_sleep[2] = {DCS_CMD_ENTER_SLEEP_MODE, 0x00};

static char display_off[2] = {DCS_CMD_SET_DISPLAY_OFF, 0x00};

static char MTP_key_enable_1[3] = {0xf0, 0x5a, 0x5a};
static char MTP_key_enable_2[3] = {0xf1, 0x5a, 0x5a};

static char ACL_param_setting[29] = {
	0xc1, 0x47, 0x53, 0x13, 0x53,
	0x00, 0x00, 0x02, 0xCF, 0x00,
	0x00, 0x04, 0xFF, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x03,
	0x07, 0x09, 0x0B, 0x10, 0x16,
	0x1A, 0x1F, 0x26, 0x2F
};
static char ACL_enable_disable_settings[2] = {0xc0, 0x00};

static char  panel_condition_set_0to18[20] = {
	0xf8, 0x19, 0x30, 0x00, 0x00,
	0x00, 0x87, 0x00, 0x38, 0x73,
	0x07, 0x24, 0x73, 0x3a, 0x0d,
	0x07, 0x00, 0x00, 0x04, 0x07,
};
static char  panel_condition_set_19to37[20] = {
	0xf8, 0x65, 0x07, 0x00, 0x00,
	0x02, 0x07, 0x07, 0x20, 0x20,
	0xC0, 0xC1, 0x01, 0x81, 0xC1,
	0x00, 0xC1, 0xF6, 0xF6, 0xC1,
};

static char  set_reg_offset_19[2] = {0xb0, 0x13};
static char  set_reg_offset_0[2] = {0xb0, 0x0};

/* DTYPE_DCS_LWRITE */
static char display_condition_set[4] = {0xf2, 0x80, 0x05, 0x0d};

static char gamma_ltps_set_update[2] = {0xf7, 0x03};

static char gamma_set_update[2] = {0xf7, 0x01}; /* DTYPE_DCS_WRITE1 */

static char etc_condition_set_source_control[4] = {0xf6, 0x00, 0x02, 0x00};

static char etc_condition_set_pentile_control[10] = {
	0xb6, 0x63, 0x02, 0x03, 0x32,
	0xff, 0x44, 0x44, 0xc0, 0x00
}; /* DTYPE_DCS_LWRITE */

static char etc_condition_set_nvm_setting[15] = {
	0xd9, 0x14, 0x40, 0x0c, 0xcb,
	0xce, 0x6e, 0xc4, 0x07, 0x40,
	0x41, 0xd0, 0x00, 0x60, 0x19
}; /* DTYPE_DCS_LWRITE */

static char etc_condition_set_power_control[8] = {
	0xf4, 0xcf, 0x0a, 0x12, 0x10,
	0x1e, 0x33, 0x02
}; /* DTYPE_DCS_LWRITE */

static char etc_condition_set_power_control_mtp_workaround[8] = {
	0xf4, 0xcf, 0x0a, 0x0f, 0x10,
	0x19, 0x33, 0x02
}; /* DTYPE_DCS_LWRITE */

#define V0 4600000

static u16 default_input_gamma[NUM_VOLT_PTS][NUM_COLORS] = {
	{0x5F, 0x2F, 0x61},
	{0xB0, 0xC3, 0xA3},
	{0xB1, 0xC4, 0xBA},
	{0xC3, 0xCC, 0xC2},
	{0x96, 0xA0, 0x91},
	{0xAF, 0xB5, 0xAF},
	{0xC0, 0xAC, 0xD0}
};

static u16 a1_input_gamma[NUM_VOLT_PTS][NUM_COLORS] = {
	{0x70, 0x55, 0x76},
	{0xAA, 0xB4, 0x97},
	{0xAC, 0xB5, 0xA2},
	{0xBE, 0xC5, 0xB8},
	{0x8D, 0x98, 0x88},
	{0xA7, 0xAF, 0xA4},
	{0xC2, 0xB5, 0xD3}
};

static u16 a1_83_8b_input_gamma[NUM_VOLT_PTS][NUM_COLORS] = {
	{0x69, 0x53, 0x6B},
	{0xA8, 0xAD, 0x9D},
	{0xAE, 0xBD, 0xA9},
	{0xC0, 0xC4, 0xBC},
	{0x8F, 0x95, 0x8D},
	{0xAB, 0xAC, 0xA7},
	{0xBA, 0xB3, 0xD0}
};

static __u8 id_regs[3] = {0, 0, 0};
static int mipi_mot_read_byte(__u8 address, __u8 *value)
{
	int ret;
	__u8 cmd_buf[2] = {address, 0};
	struct dsi_cmd_desc reg_read_cmd = {
		.dtype = DTYPE_DCS_READ,
		.last = 1,
		.vc = 0,
		.ack = 1,
		.wait = 1,
		.dlen = 2,
		.payload = cmd_buf
	};

	ret = mipi_mot_rx_cmd(&reg_read_cmd, &address, 1);
	if (ret < 0) {
		pr_err("%s: failed to read cmd=0x%x\n", __func__, address);
		*value = 0;
	}

	pr_debug("%s: %08x=%08x\n", __func__,
		 (unsigned int)address, (unsigned int)(*value));

	return 0;
}

static char smart_dynamic_elvss[3] = {0xb1, 0x08, 0x81};
static char default_elvss[3] = {0xb1, 0x08, 0x81};
static char static_elvss[4][3] = {
	{0xb1, 0x04, 0x90},
	{0xb1, 0x04, 0x93},
	{0xb1, 0x04, 0x95},
	{0xb1, 0x04, 0x96},
};

#define A1_FACTORY_ID		0x25
#define A2_FACTORY_ID		0x05
#define EARLY0_FACTORY_ID	0x00
#define EARLY1_FACTORY_ID	0x01

static char *getElvssForGamma(int gammaIdx)
{
	int nits = 300 - gammaIdx * 10;
	if ((id_regs[0] == A2_FACTORY_ID && id_regs[1] >= 0x5)
		|| (id_regs[0] == A1_FACTORY_ID)) {
		int brightness = nits <= 300 && nits > 200 ? 0x00 :
			nits <= 200 && nits > 100 ? 0x05 : 0x0d;
		/* elvss_tth_status will be set to "1" when battery temp
		 * is >= threshold
		 */
		if (mot_panel->elvss_tth_status)
			smart_dynamic_elvss[2] = (unsigned char)min(0x9f,
				max(0x81, id_regs[2] + brightness));
		else
			smart_dynamic_elvss[2] = (unsigned char)min(0x9f,
				max(0x81, id_regs[2] + brightness - 0xf));

		return &smart_dynamic_elvss[0];
	} else if ((id_regs[0] == EARLY0_FACTORY_ID && id_regs[1] == 0x0)
		|| (id_regs[0] == EARLY1_FACTORY_ID && id_regs[1] == 0x0)
		|| (id_regs[0] == A2_FACTORY_ID && id_regs[1] >= 0x2
			&& id_regs[1] <= 0x4)) {
		return &static_elvss[nits <= 100 ? 3 : nits <= 160 ? 2 :
			nits <= 200 ? 1 : 0][0];
	}
	return &default_elvss[0];
}

static char *getPwrCtrl(void)
{
	if (id_regs[0] == A1_FACTORY_ID
		&& (id_regs[1] == 0x00 || id_regs[1] == 0x80))
		return &etc_condition_set_power_control_mtp_workaround[0];
	return &etc_condition_set_power_control[0];
}

#define DEFAULT_DELAY 1

static struct dsi_cmd_desc mot_set_brightness_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, RAW_GAMMA_SIZE, NULL},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(gamma_set_update), gamma_set_update},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(default_elvss), default_elvss}
};


static struct dsi_cmd_desc mot_video_on_cmds1[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, DEFAULT_DELAY,
			sizeof(MTP_key_enable_1), MTP_key_enable_1},
	{DTYPE_DCS_WRITE, 1, 0, 0, DEFAULT_DELAY,
			sizeof(MTP_key_enable_2), MTP_key_enable_2},
};

static struct dsi_cmd_desc mot_acl_enable_disable[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(ACL_enable_disable_settings),
					ACL_enable_disable_settings}
};

static struct dsi_cmd_desc mot_video_on_cmds2_1[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(panel_condition_set_0to18), panel_condition_set_0to18},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(set_reg_offset_19), set_reg_offset_19},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(panel_condition_set_19to37), panel_condition_set_19to37},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(set_reg_offset_0), set_reg_offset_0},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(display_condition_set), display_condition_set},
};

static struct dsi_cmd_desc mot_video_on_cmds2_2[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY, RAW_GAMMA_SIZE, NULL},
};

static struct dsi_cmd_desc mot_video_on_cmds2_3[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, DEFAULT_DELAY,
		sizeof(gamma_ltps_set_update), gamma_ltps_set_update},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(etc_condition_set_source_control),
			etc_condition_set_source_control},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(etc_condition_set_pentile_control),
			etc_condition_set_pentile_control},
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(etc_condition_set_nvm_setting),
			etc_condition_set_nvm_setting}
};

static struct dsi_cmd_desc mot_video_on_cmds2_pwr_ctrl[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(etc_condition_set_power_control),
			etc_condition_set_power_control},
};
static struct dsi_cmd_desc mot_video_on_cmds2_elvss[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(default_elvss), default_elvss},
};

static struct dsi_cmd_desc mot_video_on_cmds2_acl[] = {
	/* This is the ACL parameter setting */
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(ACL_param_setting), ACL_param_setting},
	/* This is for enabling or disabling the ACL */
	{DTYPE_DCS_LWRITE, 1, 0, 0, DEFAULT_DELAY,
		sizeof(ACL_enable_disable_settings),
					ACL_enable_disable_settings},
};

static struct dsi_cmd_desc mot_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(display_off), display_off},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(enter_sleep), enter_sleep}
};

static void enable_acl(struct msm_fb_data_type *mfd)
{
	/* Write the value only if the display is enable and powerd on */
	if ((mfd->op_enable != 0) && (mfd->panel_power_on != 0)) {
		mutex_lock(&mfd->dma->ov_mutex);
		mipi_set_tx_power_mode(0);
		ACL_enable_disable_settings[1] = mot_panel->acl_enabled;
		mipi_mot_tx_cmds(&mot_acl_enable_disable[0],
					ARRAY_SIZE(mot_acl_enable_disable));
		mutex_unlock(&mfd->dma->ov_mutex);
	}
}

#define RETRY_MAX_CNT 15
static int panel_enable(struct msm_fb_data_type *mfd)
{
	static int bootloader_nit = 1;
	int idx, retry_cnt, ret;

	mipi_mot_tx_cmds(mot_video_on_cmds1, ARRAY_SIZE(mot_video_on_cmds1));
	mipi_mot_panel_exit_sleep();

	/*
	 * TODO: this is a software workaround for IKHSS7-35239.
	 * - There might be a timing issue when host can read from
	 * the panel, if not then panel will respond some messages that the host
	 * can not parse it correctly. The QCOM case#00789265 was opened
	 * but could not have all the testcases to test this mipi_dsi_cmds_rx()
	 * correctly. The clone CR#IKHSS7-38202 was opened to investigate the
	 * root cause of this issue
	 * - This SW workaround is testing on 6 P3 units, and observed that
	 * after 1-5 failure readings, the panel returns correct reading message
	 */
	for (retry_cnt = 0; retry_cnt < RETRY_MAX_CNT; retry_cnt++) {
		ret = mipi_mot_read_byte(0xda, &id_regs[0]);
		if (ret == -1) {
			mdelay(1);
			continue;
		} else
			break;
	}

	if (retry_cnt > 0 && retry_cnt < RETRY_MAX_CNT)
		pr_info("%s: retry to read 0xda %d times and it is OK now\n",
							__func__, retry_cnt);
	else if (retry_cnt == RETRY_MAX_CNT)
		pr_err("%s: retry to read %d but still fails\n",
			__func__, RETRY_MAX_CNT);


	mipi_mot_read_byte(0xdb, &id_regs[1]);
	mipi_mot_read_byte(0xdc, &id_regs[2]);

	ACL_enable_disable_settings[1] = mot_panel->acl_enabled;
	mipi_mot_tx_cmds(&mot_video_on_cmds2_1[0],
				ARRAY_SIZE(mot_video_on_cmds2_1));
	/* Set backlight */
	if (bootloader_nit) {
		/*
		 * Kernel bootup. Set it to default nit which should
		 * be same with it in bootloader.
		 */
		idx = DEFAULT_BRIGHTNESS_LEVELS;

		/* Calculate gamma setting */
		mipi_mot_get_raw_mtp(mfd);

		if (id_regs[0] == A1_FACTORY_ID) {
			if (id_regs[1] == 0x83 || id_regs[1] == 0x8b) {
				/* A1 panel 83, 8b */
				mipi_mot_dynamic_gamma_calc(V0, 0xfa, 0x01,
						a1_83_8b_input_gamma);
			} else {
				/* A1 panel 00 ~ 82 */
				mipi_mot_dynamic_gamma_calc(V0, 0xfa, 0x01,
						a1_input_gamma);
			}
		} else {
			/* A2 panel */
			mipi_mot_dynamic_gamma_calc(V0, 0xfa, 0x01,
					default_input_gamma);
		}

		bootloader_nit = 0;
	} else
		idx = mfd->bl_level;

	if (idx < 0)
		idx = 0;
	if (idx > NUMBER_BRIGHTNESS_LEVELS - 1)
		idx = NUMBER_BRIGHTNESS_LEVELS - 1;

	/* Mapping 1 ~ 30 level to 10 ~ 300 nits table */
	idx = (idx + 1) * PANEL_LEVEL_TO_NITS_LEVEL - 1;

	mot_video_on_cmds2_2[0].payload = mipi_mot_get_gamma_setting(mfd, idx);
	mipi_mot_tx_cmds(mot_video_on_cmds2_2,
			ARRAY_SIZE(mot_video_on_cmds2_2));

	mipi_mot_tx_cmds(&mot_video_on_cmds2_3[0],
				ARRAY_SIZE(mot_video_on_cmds2_3));

	mot_video_on_cmds2_pwr_ctrl[0].payload = getPwrCtrl();
	mipi_mot_tx_cmds(&mot_video_on_cmds2_pwr_ctrl[0],
				ARRAY_SIZE(mot_video_on_cmds2_pwr_ctrl));

	mot_video_on_cmds2_elvss[0].payload = getElvssForGamma(idx);
	mipi_mot_tx_cmds(&mot_video_on_cmds2_elvss[0],
				ARRAY_SIZE(mot_video_on_cmds2_elvss));

	mipi_mot_tx_cmds(&mot_video_on_cmds2_acl[0],
				ARRAY_SIZE(mot_video_on_cmds2_acl));

	return 0;
}

static int panel_disable(struct msm_fb_data_type *mfd)
{
	mipi_mot_tx_cmds(&mot_display_off_cmds[0],
				ARRAY_SIZE(mot_display_off_cmds));
	return 0;
}

static void panel_set_backlight(struct msm_fb_data_type *mfd)
{

	static int bl_level_old = -1;
	int idx = 0;

	pr_debug("%s(%d)\n", __func__, (s32)mfd->bl_level);

	if (!mfd->panel_power_on)
		return;

	if (bl_level_old == mfd->bl_level)
		return;

	idx = mfd->bl_level;
	if (idx < 0)
		idx = 0;
	if (idx > NUMBER_BRIGHTNESS_LEVELS - 1)
		idx = NUMBER_BRIGHTNESS_LEVELS - 1;

	/* Mapping 1 ~ 30 level to 10 ~ 300 nits table */
	idx = (idx + 1) * PANEL_LEVEL_TO_NITS_LEVEL - 1;

	mot_set_brightness_cmds[0].payload =
		mipi_mot_get_gamma_setting(mfd, idx);
	mot_set_brightness_cmds[2].payload = getElvssForGamma(idx);

	pr_debug(".. ELVSS test: ID3=0x%x brightnes=%d elvsss=%d B1[2]=0x%x\n",
			id_regs[2], mfd->bl_level , mot_panel->elvss_tth_status,
			(unsigned int)(*(mot_set_brightness_cmds[2].payload)));

	mutex_lock(&mfd->dma->ov_mutex);
	mipi_set_tx_power_mode(0);
	mipi_mot_tx_cmds(&mot_set_brightness_cmds[0],
				ARRAY_SIZE(mot_set_brightness_cmds));

	bl_level_old = mfd->bl_level;
	mutex_unlock(&mfd->dma->ov_mutex);

	pr_debug("%s(%d) completed\n", __func__, (s32)mfd->bl_level);
	return;
}

static int __init mipi_video_mot_hd_pt_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	pr_debug("%s\n", __func__);

	if (msm_fb_detect_client("mipi_mot_video_smd_hd_465"))
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

	mot_panel->acl_support_present = TRUE;
	mot_panel->acl_enabled = FALSE; /* By default the ACL is disabled. */

	mot_panel->elvss_tth_support_present = TRUE;

	mot_panel->panel_enable = panel_enable;
	mot_panel->panel_disable = panel_disable;
	mot_panel->set_backlight = panel_set_backlight;
	mot_panel->panel_off = panel_disable;
	mot_panel->exit_sleep_wait = 20;

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
