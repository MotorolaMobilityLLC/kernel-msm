/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 * Copyright (c) 2011, Motorola Mobility, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef MIPI_MOT_PANEL_H
#define MIPI_MOT_PANEL_H

#include "smd_dynamic_gamma.h"

#define DCS_CMD_SOFT_RESET           0x01
#define DCS_CMD_GET_POWER_MODE       0x0A
#define DCS_CMD_ENTER_SLEEP_MODE     0x10
#define DCS_CMD_EXIT_SLEEP_MODE      0x11
#define DCS_CMD_SET_NORMAL_MODE_ON   0x13
#define DCS_CMD_SET_DISPLAY_ON       0x29
#define DCS_CMD_SET_DISPLAY_OFF      0x28
#define DCS_CMD_SET_COLUMN_ADDRESS   0x2A
#define DCS_CMD_SET_PAGE_ADDRESS     0x2B
#define DCS_CMD_SET_TEAR_OFF         0x34
#define DCS_CMD_SET_TEAR_ON          0x35
#define DCS_CMD_SET_INVERTED_MODE    0x36
#define DCS_CMD_SET_SCAN_LINE        0X44
#define DCS_CMD_SET_TEAR_SCANLINE    0x44
#define DCS_CMD_SET_BRIGHTNESS       0x51
#define DCS_CMD_READ_BRIGHTNESS      0x52
#define DCS_CMD_SET_CTRL_DISP        0x53
#define DCS_CMD_READ_CTRL_DISP       0x54
#define DCS_CMD_SET_CABC             0x55
#define DCS_CMD_READ_CABC            0x56
#define DCS_CMD_READ_DDB_START       0xA1
#define DCS_CMD_SET_OFFSET           0xB0
#define DCS_CMD_SET_MCS              0xB2
#define DCS_CMD_SET_DISPLAY_MODE     0xB3
#define DCS_CMD_SET_BCKLGHT_PWM      0xB4
#define DCS_CMD_DATA_LANE_CONFIG     0xB5
#define DCS_CMD_READ_RAW_MTP         0xD3
#define DCS_CMD_READ_DA              0xDA
#define DCS_CMD_READ_DB              0xDB
#define DCS_CMD_READ_DC              0xDC
#define DCS_CMD_RDDSDR               0x0F

#define INVALID_VALUE   -1
#define VALID_PWR_MODE 0x90

/* ESD spec require 10ms, select 8ms */
#define MOT_PANEL_ESD_CHECK_PERIOD   msecs_to_jiffies(8000)
#define MOT_PANEL_ESD_NUM_TRY_MAX    1

#define MOT_PANEL_OFF     0x0
#define MOT_PANEL_ON      0x1

enum {
	MOT_ESD_PANEL_OFF = 0,
	MOT_ESD_ESD_DETECT,
	MOT_ESD_OK,
};

struct mipi_mot_panel {

	struct msm_panel_info pinfo;
	struct msm_fb_data_type *mfd;
	struct dsi_buf *mot_tx_buf;
	struct dsi_buf *mot_rx_buf;
	struct mutex lock;
	boolean acl_support_present;
	boolean acl_enabled;
	/* Adjust elvss output to keep or remove temp margin
	 * based on panel temperature to save panel power consumption
	 * this is the flag to indicate panel support it or not */
	boolean elvss_tth_support_present;
	/* Panel temp threshold was maintained in framework configure file for
	 * each product. Framework monitor and notify temp status via sysfs
	 * 1: above temp threshold (remove temp margin)
	 * 0: below temp threshold (keep temp  margin, default value to be safe)
	 */
	boolean elvss_tth_status;
	atomic_t state;
	bool esd_enabled;
	bool esd_detection_run;
	struct workqueue_struct *esd_wq;
	struct delayed_work esd_work;

	/* reboot notifier for panel flash when power down or unplug charger */
	struct notifier_block reboot_notifier;
	struct srcu_notifier_head panel_notifier_list;

	int (*panel_enable) (struct msm_fb_data_type *mfd);
	int (*panel_disable) (struct msm_fb_data_type *mfd);
	int (*panel_on)(struct msm_fb_data_type *mfd);
	int (*panel_off)(struct msm_fb_data_type *mfd);
	int (*panel_enter_normal_mode)(struct msm_fb_data_type *mfd);
	void (*esd_run) (void);
	void (*set_backlight)(struct msm_fb_data_type *mfd);
	void (*set_backlight_curve)(struct msm_fb_data_type *mfd);
	u16 (*get_manufacture_id)(struct msm_fb_data_type *mfd);
	u16 (*get_controller_ver)(struct msm_fb_data_type *mfd);
	u16 (*get_controller_drv_ver)(struct msm_fb_data_type *mfd);
	void (*enable_acl)(struct msm_fb_data_type *mfd);
	int (*is_valid_manufacture_id)(struct msm_fb_data_type *mfd, u8 id);
	int (*is_valid_power_mode)(struct msm_fb_data_type *mfd);

	int (*hide_img)(struct msm_fb_data_type *, int hide);
	int (*prepare_for_suspend) (struct msm_fb_data_type *, int full);
	int (*prepare_for_resume) (struct msm_fb_data_type *,
		int full, int in_sleep, int gamma);
	void (*shift_correct) (void);
};

int mipi_mot_device_register(struct msm_panel_info *pinfo, u32 channel,
								u32 panel);

struct mipi_mot_panel *mipi_mot_get_mot_panel(void);
void mipi_mot_set_mot_panel(struct mipi_mot_panel *mot_panel_ptr);


u16 mipi_mot_get_manufacture_id(struct msm_fb_data_type *mfd);
u16 mipi_mot_get_controller_ver(struct msm_fb_data_type *mfd);
u16 mipi_mot_get_controller_drv_ver(struct msm_fb_data_type *mfd);
int mipi_mot_get_raw_mtp(struct msm_fb_data_type *mfd);
void mipi_mot_dynamic_gamma_calc(uint32_t v0_val, uint8_t preamble_1,
	uint8_t preamble_2, uint16_t in_gamma[NUM_VOLT_PTS][NUM_COLORS]);

u8 *mipi_mot_get_gamma_setting(struct msm_fb_data_type *mfd, int level);
int mipi_mot_panel_on(struct msm_fb_data_type *mfd);
int mipi_mot_panel_off(struct msm_fb_data_type *mfd);
u8 mipi_mode_get_pwr_mode(struct msm_fb_data_type *mfd);
void mipi_mot_esd_work(void);
void mipi_mot_tx_cmds(struct dsi_cmd_desc *cmds, int cnt);
int mipi_mot_rx_cmd(struct dsi_cmd_desc *cmd, u8 *data, int rlen);
int mipi_mot_hide_img(struct msm_fb_data_type *mfd, int hide);
int mipi_mot_enter_normal_mode(struct msm_fb_data_type *mfd);
int __init moto_panel_debug_init(void);
#endif /* MIPI_MOT_PANEL_H */
