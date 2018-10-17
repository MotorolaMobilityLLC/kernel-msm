/* Copyright (c) 2017-2018, 2020, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_SPI_PANEL_H__
#define __MDSS_SPI_PANEL_H__

#if defined(CONFIG_FB_MSM_MDSS_SPI_PANEL) && defined(CONFIG_SPI_QUP)
#include <linux/list.h>
#include <linux/mdss_io_util.h>
#include <linux/irqreturn.h>
#include <linux/pinctrl/consumer.h>
#include <linux/gpio.h>

#include "mdss_fb.h"
#include "mdss_panel.h"
#ifdef TARGET_HW_MDSS_MDP3
#include "mdp3_dma.h"
#endif

#define MDSS_MAX_BL_BRIGHTNESS 255

#define MDSS_SPI_RST_SEQ_LEN	10

#define NONE_PANEL "none"

#define CTRL_STATE_UNKNOWN		0x00
#define CTRL_STATE_PANEL_INIT		BIT(0)
#define CTRL_STATE_MDP_ACTIVE		BIT(1)

#define CTRL_STATE_PANEL_ACTIVE         BIT(1)

#define MDSS_PINCTRL_STATE_DEFAULT "mdss_default"
#define MDSS_PINCTRL_STATE_SLEEP  "mdss_sleep"
#define SPI_PANEL_TE_TIMEOUT	400
#define KOFF_TIMEOUT_MS 84
#define KOFF_TIMEOUT msecs_to_jiffies(KOFF_TIMEOUT_MS)
enum spi_panel_data_type {
	panel_cmd,
	panel_parameter,
	panel_pixel,
	UNKNOWN_FORMAT,
};

enum spi_panel_bl_ctrl {
	SPI_BL_PWM,
	SPI_BL_WLED,
	SPI_BL_DCS_CMD,
	SPI_UNKNOWN_CTRL,
};

struct spi_pinctrl_res {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};
#define SPI_PANEL_DST_FORMAT_RGB565		0

struct spi_ctrl_hdr {
	char wait;	/* ms */
	char dlen;	/* 8 bits */
};

struct spi_cmd_desc {
	struct spi_ctrl_hdr dchdr;
	char *command;
	char *parameter;
};

struct spi_panel_cmds {
	char *buf;
	int blen;
	struct spi_cmd_desc *cmds;
	int cmd_cnt;
};

enum spi_panel_status_mode {
	SPI_ESD_REG,
	SPI_SEND_PANEL_COMMAND,
	SPI_ESD_MAX,
};

struct mdss_spi_img_data {
	void *addr;
	unsigned long len;
	struct dma_buf *srcp_dma_buf;
	struct dma_buf_attachment *srcp_attachment;
	struct sg_table *srcp_table;
	bool mapped;
};

struct spi_panel_data {
	struct mdss_panel_data panel_data;
	struct mdss_util_intf *mdss_util;
	struct spi_pinctrl_res pin_res;
	struct mdss_module_power panel_power_data;
	struct completion spi_panel_te;
#ifdef TARGET_HW_MDSS_MDP3
	struct mdp3_notification vsync_client;
#endif
	unsigned int vsync_status;
	char *tx_buf;
	u8 ctrl_state;
	int disp_te_gpio;
	int rst_gpio;
	int disp_dc_gpio;	/* command or data */
	struct spi_panel_cmds on_cmds;
	struct spi_panel_cmds off_cmds;
	bool (*check_status)(struct spi_panel_data *pdata);
	int (*on)(struct mdss_panel_data *pdata);
	int (*off)(struct mdss_panel_data *pdata);
	struct pwm_device *pwm_bl;
	int bklt_ctrl;	/* backlight ctrl */
	bool pwm_pmi;
	int pwm_period;
	int pwm_pmic_gpio;
	int pwm_lpg_chan;
	int pwm_enabled;
	int bklt_max;
	int status_mode;
	atomic_t koff_cnt;
	int byte_per_frame;
	char *front_buf;
	char *back_buf;
	wait_queue_head_t tx_done_waitq;
	struct mutex spi_tx_mutex;
	struct mutex te_mutex;
	struct mdss_spi_img_data image_data;
	u32 status_cmds_rlen;
	u8 panel_status_reg;
	u8 *exp_status_value;
	u8 *act_status_value;
	ktime_t vsync_time;
	int vsync_per_te;
	bool vsync_enable;
	struct kernfs_node *vsync_event_sd;
	unsigned char *return_buf;
	struct blocking_notifier_head notifier_head;
};

#ifdef TARGET_HW_MDSS_MDP3
int mdss_spi_panel_kickoff(struct mdss_panel_data *pdata,
				char __iomem *buf, int len, int stride);
void mdp3_spi_vsync_enable(struct mdss_panel_data *pdata,
				struct mdp3_notification *vsync_client);
void mdp3_check_spi_panel_status(struct work_struct *work,
				uint32_t interval);
#endif
void mdss_spi_vsync_enable(struct mdss_panel_data *pdata, int enable);
int mdss_spi_wait_tx_done(struct spi_panel_data *ctrl_pdata);
void mdss_spi_tx_fb_complete(void *ctx);
int is_spi_panel_continuous_splash_on(struct mdss_panel_data *pdata);
void enable_spi_panel_te_irq(struct spi_panel_data *ctrl_pdata, bool enable);
int mdss_spi_panel_power_ctrl(struct mdss_panel_data *pdata, int power_state);
int mdss_spi_panel_pinctrl_set_state(struct spi_panel_data *ctrl_pdata,
					bool active);
int mdss_spi_wait_tx_done(struct spi_panel_data *ctrl_pdata);
int mdss_spi_panel_reset(struct mdss_panel_data *pdata, int enable);
int mdss_spi_panel_on(struct mdss_panel_data *pdata);
int mdss_spi_panel_off(struct mdss_panel_data *pdata);

#else
static inline int is_spi_panel_continuous_splash_on(
				struct mdss_panel_data *pdata)
{
	return 0;
}
#endif/* End of CONFIG_FB_MSM_MDSS_SPI_PANEL && ONFIG_SPI_QUP */

#endif /* End of __MDSS_SPI_PANEL_H__ */
