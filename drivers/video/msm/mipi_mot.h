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

#include <mach/mmi_panel_notifier.h>
#include "smd_dynamic_gamma.h"
#include "msm_fb.h"
#include "mipi_dsi.h"

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
/* MIPI_MOT_PANEL_ESD_TEST is used to run ESD detection/recovery stress test */
/* #define MIPI_MOT_PANEL_ESD_TEST 1 */
#define MIPI_MOT_PANEL_ESD_CNT_MAX      3

/* ESD spec require 10s, select 8s */
#ifdef MIPI_MOT_PANEL_ESD_TEST
/*
 * MOT_PANEL_ESD_SELF_TRIGGER is triggered ESD recovery depending how many
 * times of MIPI_MOT_PANEL_ESD_CNT_MAX detection
 */
#define MOT_PANEL_ESD_SELF_TRIGGER  1

#define MOT_PANEL_ESD_CHECK_PERIOD     msecs_to_jiffies(200)
#define MOT_PANEL_ESD_WQ_TIME_MSEC     10000
#else
#define MOT_PANEL_ESD_CHECK_PERIOD     msecs_to_jiffies(8000)
#define MOT_PANEL_ESD_WQ_TIME_MSEC     20000
#endif

#define MOT_PANEL_ESD_NUM_TRY_MAX    1

#define MOT_PANEL_OFF     0x0
#define MOT_PANEL_ON      0x1

#define MOT_ESD_OK         0
#define MOT_ESD_PANEL_OFF  1

struct mipi_mot_cmd_seq {
	enum {
		MIPI_MOT_SEQ_TX,
		MIPI_MOT_SEQ_EXEC_SUB_SEQ,
		/* TODO: Add LP power mode when needed */
		MIPI_MOT_SEQ_TX_PWR_MODE_HS,
		MIPI_MOT_SEQ_EXIT_SLEEP,
	} type;
	int (*cond_func)(struct msm_fb_data_type *);
	union {
		struct dsi_cmd_desc cmd;
		struct mipi_mot_cmd_seq_sub {
			struct mipi_mot_cmd_seq *seq;
			int count;
		} sub;
	};
};

#define MIPI_MOT_TX_DEF(cond, type, delay, data) \
	{MIPI_MOT_SEQ_TX, (cond), .cmd.dtype = (type), .cmd.last = 1, \
			.cmd.vc = 0, .cmd.ack = 0, .cmd.wait = (delay), \
			.cmd.dlen = sizeof(data), .cmd.payload = data}
#define MIPI_MOT_EXEC_SEQ(cond, data) \
	{MIPI_MOT_SEQ_EXEC_SUB_SEQ, (cond), .sub.seq = data,\
			.sub.count = ARRAY_SIZE(data)}
#define MIPI_MOT_TX_EXIT_SLEEP(cond) {MIPI_MOT_SEQ_EXIT_SLEEP, (cond)}

/*
 * Used to overwrite clk config part of panel mipi_dsi_phy_ctrl data
 * based on desired clk_rate to get a more suitable MIPI DSI clk freq.
 *
 * A map table between radio id and desired dsi clk freq should be
 * provided in device tree of a certain device.
 * The desired clk_rate would be get in board display file through the
 * passed in radio id and the map table read from device tree.
 *
 */
struct mipi_dsi_clk_config {
	/* a "key" to match the desired dsi clk freq get from
	 * board display file
	 */
	__u32 clk_rate;
	/* to overwrite the default value timing[0] - timing [10]
	 * in panel mipi_dsi_phy_ctrl data
	 */
	__u32 timing[11];
	/* to overwrite the default value pll[1] - pll[3]
	 * in panel mipi_dsi_phy_ctrl data
	 */
	__u32 pll[3];
};
#define MIPI_DSI_CLK_MAX_NR	5
struct mipi_mot_panel {
	struct mipi_dsi_panel_platform_data *pdata;
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
	u8 esd_expected_pwr_mode;
	struct workqueue_struct *esd_wq;
	struct delayed_work esd_work;
	bool is_no_disp;

	/* reboot notifier for panel flash when power down or unplug charger */
	struct notifier_block reboot_notifier;
	struct srcu_notifier_head panel_notifier_list;

	int (*panel_enable) (struct msm_fb_data_type *mfd);
	int (*panel_disable) (struct msm_fb_data_type *mfd);
	int (*panel_enable_wa)(struct msm_fb_data_type *mfd);
	int (*panel_on)(struct msm_fb_data_type *mfd);
	int (*panel_off)(struct msm_fb_data_type *mfd);
	void (*panel_en_from_partial) (struct msm_fb_data_type *mfd);
	unsigned int exit_sleep_wait;
	unsigned int exit_sleep_panel_on_wait;
	struct timer_list exit_sleep_panel_on_timer;
	void (*esd_run) (void);
	void (*set_backlight)(struct msm_fb_data_type *mfd);
	void (*set_backlight_curve)(struct msm_fb_data_type *mfd);
	u16 (*get_manufacture_id)(struct msm_fb_data_type *mfd);
	u16 (*get_controller_ver)(struct msm_fb_data_type *mfd);
	u16 (*get_controller_drv_ver)(struct msm_fb_data_type *mfd);
	void (*enable_acl)(struct msm_fb_data_type *mfd);
	void (*enable_te)(struct msm_fb_data_type *mfd, int on);
	int (*is_valid_manufacture_id)(struct msm_fb_data_type *mfd, u8 id);
	int (*is_valid_power_mode)(struct msm_fb_data_type *mfd);

	int (*hide_img)(struct msm_fb_data_type *, int hide);
	int (*prepare_for_suspend) (struct msm_fb_data_type *, int full);
	int (*prepare_for_resume) (struct msm_fb_data_type *,
		int full, int in_sleep, int gamma);
};

int mipi_mot_device_register(struct msm_panel_info *pinfo, u32 channel,
								u32 panel);

struct mipi_mot_panel *mipi_mot_get_mot_panel(void);
void mipi_mot_set_mot_panel(struct mipi_mot_panel *mot_panel_ptr);


u16 mipi_mot_get_manufacture_id(struct msm_fb_data_type *mfd);
u16 mipi_mot_get_controller_ver(struct msm_fb_data_type *mfd);
u16 mipi_mot_get_controller_drv_ver(struct msm_fb_data_type *mfd);
int mipi_mot_is_valid_power_mode(struct msm_fb_data_type *mfd);
int mipi_mot_get_raw_mtp(struct msm_fb_data_type *mfd);
void mipi_mot_dynamic_gamma_calc(uint32_t v0_val, uint8_t preamble_1,
	uint8_t preamble_2, uint16_t in_gamma[NUM_VOLT_PTS][NUM_COLORS]);

u8 *mipi_mot_get_gamma_setting(struct msm_fb_data_type *mfd, int level);
int mipi_mot_panel_on(struct msm_fb_data_type *mfd);
int mipi_mot_panel_off(struct msm_fb_data_type *mfd);
void mipi_mot_panel_exit_sleep(void);
void mipi_mot_exit_sleep_wait(void);
int mipi_mot_get_pwr_mode(struct msm_fb_data_type *mfd, u8 *pwr_mode);
void mipi_mot_esd_work(void);
int mipi_mot_tx_cmds(struct dsi_cmd_desc *cmds, int cnt);
int mipi_mot_rx_cmd(struct dsi_cmd_desc *cmd, u8 *data, int rlen);
int mipi_mot_hide_img(struct msm_fb_data_type *mfd, int hide);
void mipi_mot_set_tear(struct msm_fb_data_type *mfd, int on);
int __init moto_panel_debug_init(void);
int mipi_mot_exec_cmd_seq(struct msm_fb_data_type *mfd,
			struct mipi_mot_cmd_seq *seq, int cnt);
int is_aod_supported(struct msm_fb_data_type *mfd);
#define AOD_SUPPORTED is_aod_supported

int __init mipi_mot_reconfig_dsiphy_db(struct msm_panel_info *pinfo,
			     struct mipi_dsi_phy_ctrl *phy_db,
			     struct mipi_dsi_clk_config *clk_config_tbl);
#endif /* MIPI_MOT_PANEL_H */
