/* Copyright (c) 2008-2013, The Linux Foundation. All rights reserved.
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

#ifndef MDSS_SAMSUNG_PANEL_H
#define MDSS_SAMSUNG_PANEL_H

#include "mdss_panel.h"
#include "smart_dimming.h"
#include "smart_mtp_s6e63j.h"
#define MAX_PANEL_NAME_SIZE 100
#define LCD_DEBUG(X, ...) pr_info("[LCD]%s:"X, __func__, ## __VA_ARGS__);

enum mdss_samsung_cmd_list {
	PANEL_DISPLAY_ON_SEQ,
	PANEL_DISPLAY_ON,
	PANEL_DISP_OFF,
	PANEL_BRIGHT_CTRL,
	PANEL_MTP_ENABLE,
	PANEL_MTP_DISABLE,
	PANEL_ACL_OFF,
	PANEL_ACL_ON,
	PANEL_TOUCHSENSING_ON,
	PANEL_TOUCHSENSING_OFF,
	PANEL_TEAR_ON,
	PANEL_TEAR_OFF,
	PANEL_HBM_READ,
	PANEL_ALPM_ON,
	PANEL_ALPM_OFF,
	PANEL_BACKLIGHT_CMD,
};

struct candella_lux_map {
	int *lux_tab;
	int *cmd_idx;
	int lux_tab_size;
	int bkl[256];
};
struct display_status {
	unsigned char auto_brightness;
	int bright_level;
	int siop_status;
	int wait_disp_on;
	unsigned char is_smart_dim_loaded;
	u8 on;
};

struct mdss_samsung_driver_data {
	struct display_status dstat;
	struct dsi_buf sdc_tx_buf;
	struct msm_fb_data_type *mfd;
	struct dsi_buf sdc_rx_buf;
	struct mdss_panel_common_pdata *mdss_sdc_disp_pdata;
	struct mutex lock;
	unsigned int manufacture_id;
	struct mdss_panel_data *pdata;
	struct mdss_dsi_ctrl_pdata *ctrl_pdata;
	int oled_det_gpio;
	int panel;
	struct smartdim_conf *sdimconf;
	char panel_name[MAX_PANEL_NAME_SIZE];
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct platform_device *msm_pdev;
#endif
};
struct panel_rev {
	char *name;
	int panel_code;
};
enum {
	PANEL_320_OCTA_S6E63J,
};
void mdnie_lite_tuning_init(struct mdss_samsung_driver_data *msd);
void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_cmd_desc *cmds, int cnt, int flag);
#endif
