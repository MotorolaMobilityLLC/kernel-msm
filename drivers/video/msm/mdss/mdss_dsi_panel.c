/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/err.h>
#include <linux/dropbox.h>

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <mach/mmi_panel_notifier.h>

#include "mdss_dsi.h"

#define DT_CMD_HDR 6
#define ESD_DROPBOX_MSG "ESD event detected"
#define ESD_TE_DROPBOX_MSG "ESD TE event detected"

/* MDSS_PANEL_ESD_SELFTEST is used to run ESD detection/recovery stress test */
/* #define MDSS_PANEL_ESD_SELFTEST 1 */
#define MDSS_PANEL_ESD_CNT_MAX 3
#define MDSS_PANEL_ESD_TE_TRIGGER (MDSS_PANEL_ESD_CNT_MAX * 2)

/* ESD spec require 10s, select 8s */
#ifdef MDSS_PANEL_ESD_SELFTEST
/*
 * MDSS_PANEL_ESD_SELF_TRIGGER is triggered ESD recovery depending how many
 * times of MDSS_PANEL_ESD_CNT_MAX detection
 */
#define MDSS_PANEL_ESD_SELF_TRIGGER  1

#define MDSS_PANEL_ESD_CHECK_PERIOD     msecs_to_jiffies(200)
#else
/* ESD spec require 10ms, select 8ms */
#define MDSS_PANEL_ESD_CHECK_PERIOD     msecs_to_jiffies(8000)
#endif

#define MDSS_PANEL_DEFAULT_VER 0xffffffffffffffff
#define MDSS_PANEL_UNKNOWN_NAME "unknown"
#define TE_MONITOR_TO  68
#define TE_PULSE_USEC  100

#define PWR_MODE_DISON 0x4

DEFINE_LED_TRIGGER(bl_led_trigger);

void mdss_dsi_panel_lock_mutex(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	mutex_lock(&ctrl->panel_config.panel_mutex);
}

void mdss_dsi_panel_unlock_mutex(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	mutex_unlock(&ctrl->panel_config.panel_mutex);
}

void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;

	if (!gpio_is_valid(ctrl->pwm_pmic_gpio)) {
		pr_err("%s: pwm_pmic_gpio=%d Invalid\n", __func__,
				ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
		return;
	}

	ret = gpio_request(ctrl->pwm_pmic_gpio, "disp_pwm");
	if (ret) {
		pr_err("%s: pwm_pmic_gpio=%d request failed\n", __func__,
				ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
		return;
	}

	ctrl->pwm_bl = pwm_request(ctrl->pwm_lpg_chan, "lcd-bklt");
	if (ctrl->pwm_bl == NULL || IS_ERR(ctrl->pwm_bl)) {
		pr_err("%s: lpg_chan=%d pwm request failed", __func__,
				ctrl->pwm_lpg_chan);
		gpio_free(ctrl->pwm_pmic_gpio);
		ctrl->pwm_pmic_gpio = -1;
	}
}

static void mdss_dsi_panel_bklt_pwm(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	int ret;
	u32 duty;

	if (ctrl->pwm_bl == NULL) {
		pr_err("%s: no PWM\n", __func__);
		return;
	}

	if (level == 0) {
		if (ctrl->pwm_enabled)
			pwm_disable(ctrl->pwm_bl);
		ctrl->pwm_enabled = 0;
		return;
	}

	duty = level * ctrl->pwm_period;
	duty /= ctrl->bklt_max;

	pr_debug("%s: bklt_ctrl=%d pwm_period=%d pwm_gpio=%d pwm_lpg_chan=%d\n",
			__func__, ctrl->bklt_ctrl, ctrl->pwm_period,
				ctrl->pwm_pmic_gpio, ctrl->pwm_lpg_chan);

	pr_debug("%s: ndx=%d level=%d duty=%d\n", __func__,
					ctrl->ndx, level, duty);

	if (ctrl->pwm_enabled) {
		pwm_disable(ctrl->pwm_bl);
		ctrl->pwm_enabled = 0;
	}

	ret = pwm_config(ctrl->pwm_bl, duty, ctrl->pwm_period);
	if (ret) {
		pr_err("%s: pwm_config() failed err=%d.\n", __func__, ret);
		return;
	}

	ret = pwm_enable(ctrl->pwm_bl);
	if (ret)
		pr_err("%s: pwm_enable() failed err=%d\n", __func__, ret);
	ctrl->pwm_enabled = 1;
}

static char dcs_cmd[2] = {0x54, 0x00}; /* DTYPE_DCS_READ */
static struct dsi_cmd_desc dcs_read_cmd = {
	{DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(dcs_cmd)},
	dcs_cmd
};

static int mdss_dsi_panel_cmd_read(struct mdss_dsi_ctrl_pdata *ctrl, char cmd0,
		char cmd1, void (*fxn)(int), char *rbuf, int len)
{
	struct dcs_cmd_req cmdreq;
	int rc;

	dcs_cmd[0] = cmd0;
	dcs_cmd[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dcs_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = len;
	cmdreq.rbuf = rbuf;
	cmdreq.cb = fxn; /* call back */
	if (ctrl->dsi_cmdlist_put)
		rc = ctrl->dsi_cmdlist_put(ctrl, &cmdreq);
	else {
		pr_err("%s: dsi_cmdlist_put is not defined\n", __func__);
		rc = -EINVAL;
	}
	/*
	 * blocked here, until call back called
	 */

	return rc;
}

static int mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;
	int rc;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	if (ctrl->dsi_cmdlist_put)
		rc = ctrl->dsi_cmdlist_put(ctrl, &cmdreq);
	else {
		pr_err("%s: dsi_cmdlist_put is not defined\n", __func__);
		rc = -EINVAL;
	}

	return rc;
}

static char led_pwm1[2] = {0x51, 0x0};	/* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc backlight_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(led_pwm1)},
	led_pwm1
};

static void mdss_dsi_panel_bklt_dcs(struct mdss_dsi_ctrl_pdata *ctrl, int level)
{
	struct dcs_cmd_req cmdreq;

	pr_debug("%s: level=%d\n", __func__, level);

	led_pwm1[1] = (unsigned char)level;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &backlight_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

void mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (!gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
	}

	if (!gpio_is_valid(ctrl_pdata->rst_gpio)) {
		pr_debug("%s:%d, reset line not configured\n",
			   __func__, __LINE__);
		return;
	}

	pr_debug("%s: enable = %d\n", __func__, enable);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (enable) {
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 1);

		for (i = 0; i < ctrl_pdata->rst_seq_len; ++i) {
			gpio_set_value((ctrl_pdata->rst_gpio),
				ctrl_pdata->rst_seq[i]);
			if (ctrl_pdata->rst_seq[++i])
				usleep(ctrl_pdata->rst_seq[i] * 1000);
		}

		if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
			if (pinfo->mode_gpio_state == MODE_GPIO_HIGH)
				gpio_set_value((ctrl_pdata->mode_gpio), 1);
			else if (pinfo->mode_gpio_state == MODE_GPIO_LOW)
				gpio_set_value((ctrl_pdata->mode_gpio), 0);
		}
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}

	} else {
		for (i = 0; i < ctrl_pdata->dis_rst_seq_len; i++) {
			gpio_set_value((ctrl_pdata->rst_gpio),
				ctrl_pdata->dis_rst_seq[i++]);
			if (ctrl_pdata->dis_rst_seq[i])
				usleep_range(ctrl_pdata->dis_rst_seq[i] * 1000,
					ctrl_pdata->dis_rst_seq[i] * 1000);
		}
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
	}
}

static char caset[] = {0x2a, 0x00, 0x00, 0x03, 0x00};	/* DTYPE_DCS_LWRITE */
static char paset[] = {0x2b, 0x00, 0x00, 0x05, 0x00};	/* DTYPE_DCS_LWRITE */

static struct dsi_cmd_desc partial_update_enable_cmd[] = {
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(caset)}, caset},
	{{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(paset)}, paset},
};

static int mdss_dsi_panel_partial_update(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct dcs_cmd_req cmdreq;
	int rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	pr_debug("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	caset[1] = (((pdata->panel_info.roi_x) & 0xFF00) >> 8);
	caset[2] = (((pdata->panel_info.roi_x) & 0xFF));
	caset[3] = (((pdata->panel_info.roi_x - 1 + pdata->panel_info.roi_w)
								& 0xFF00) >> 8);
	caset[4] = (((pdata->panel_info.roi_x - 1 + pdata->panel_info.roi_w)
								& 0xFF));
	partial_update_enable_cmd[0].payload = caset;

	paset[1] = (((pdata->panel_info.roi_y) & 0xFF00) >> 8);
	paset[2] = (((pdata->panel_info.roi_y) & 0xFF));
	paset[3] = (((pdata->panel_info.roi_y - 1 + pdata->panel_info.roi_h)
								& 0xFF00) >> 8);
	paset[4] = (((pdata->panel_info.roi_y - 1 + pdata->panel_info.roi_h)
								& 0xFF));
	partial_update_enable_cmd[1].payload = paset;

	pr_debug("%s: enabling partial update\n", __func__);
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = partial_update_enable_cmd;
	cmdreq.cmds_cnt = 2;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	return rc;
}

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	switch (ctrl_pdata->bklt_ctrl) {
	case BL_WLED:
		led_trigger_event(bl_led_trigger, bl_level);
		break;
	case BL_PWM:
		mdss_dsi_panel_bklt_pwm(ctrl_pdata, bl_level);
		break;
	case BL_DCS_CMD:
		mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
		break;
	default:
		pr_err("%s: Unknown bl_ctrl configuration\n",
			__func__);
		break;
	}
}

static int mdss_dsi_get_pwr_mode(struct mdss_panel_data *pdata, u8 *pwr_mode)
{
	struct mdss_dsi_ctrl_pdata *ctrl;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (ctrl->panel_config.bare_board == true) {
		*pwr_mode = 0;
		goto end;
	}

	mdss_dsi_panel_cmd_read(ctrl, DCS_CMD_GET_POWER_MODE, 0x00,
					NULL, pwr_mode, 1);
end:
	pr_debug("%s: panel power mode = 0x%x\n", __func__, *pwr_mode);
	return 0;
}

static int mdss_dsi_panel_regulator_init(struct mdss_panel_data *pdata)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct platform_device *pdev;

	pr_debug("%s: is called\n", __func__);

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (!ctrl) {
		pr_err("%s: Invalid ctrl\n", __func__);
		return -EINVAL;
	}

	pdev = ctrl->pdev;
	if (!pdev) {
		pr_err("%s: Invalid pdev\n", __func__);
		return -EINVAL;
	}

	if (ctrl->panel_vregs.num_vreg > 0) {
		ret = msm_dss_config_vreg(&pdev->dev,
					ctrl->panel_vregs.vreg_config,
					ctrl->panel_vregs.num_vreg, 1);
		if (ret)
			pr_err("%s:fail to init regs. ret=%d\n", __func__, ret);
	}

	return ret;
}

static int mdss_dsi_panel_regulator_on(struct mdss_panel_data *pdata,
						int enable)
{
	int ret = 0;
	struct mdss_dsi_ctrl_pdata *ctrl;
	static bool is_reg_inited;

	pr_debug("%s(%d) is called\n", __func__, enable);

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (is_reg_inited == false) {
		ret = mdss_dsi_panel_regulator_init(pdata);
		if (ret)
			goto error;
		else
			is_reg_inited = true;
	}

	if (enable) {
		if (ctrl->panel_vregs.num_vreg > 0) {
			ret = msm_dss_enable_vreg(ctrl->panel_vregs.vreg_config,
						ctrl->panel_vregs.num_vreg, 1);
			if (ret)
				pr_err("%s:Failed to enable regulators.rc=%d\n",
							__func__, ret);
		} else
			pr_err("%s(%d): No panel regulators in the list\n",
							__func__, enable);
	} else {
		if (ctrl->panel_config.disallow_panel_pwr_off) {
			pr_warn("%s - skipping panel regulator off\n",
				__func__);
			goto error;
		}

		if (ctrl->panel_vregs.num_vreg > 0) {
			ret = msm_dss_enable_vreg(ctrl->panel_vregs.vreg_config,
						ctrl->panel_vregs.num_vreg, 0);
			if (ret)
				pr_err("%s: Failed to disable regs.rc=%d\n",
						__func__, ret);
		} else
			pr_err("%s(%d): No panel regulators in the list\n",
							__func__, enable);
	}

error:
	return ret;
}

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata);
static int mdss_dsi_panel_off(struct mdss_panel_data *pdata);

static irqreturn_t mdss_panel_esd_te_irq_handler(int irq, void *ctrl_ptr)
{
	struct mdss_dsi_ctrl_pdata *ctrl =
				(struct mdss_dsi_ctrl_pdata *)ctrl_ptr;
	pr_debug("%s: is called\n", __func__);

	complete(&ctrl->panel_esd_data.te_detected);
	disable_irq_nosync(ctrl->panel_esd_data.te_irq);
	return IRQ_HANDLED;
}

static int mdss_panel_esd_recovery_start(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	pr_warn("MDSS PANEL: ESD recovering is started\n");

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (!pdata->panel_info.panel_power_on) {
		pr_warn("%s:%d Panel already off.\n", __func__, __LINE__);
		 mdss_dsi_panel_unlock_mutex(pdata);
		goto end;
	}
	ctrl->panel_esd_data.esd_recovery_run = true;
	mdss_dsi_panel_off(pdata);
	msleep(200);
	mdss_dsi_panel_on(pdata);
	ctrl->panel_esd_data.esd_recovery_run = false;
end:
	pr_warn("MDSS PANEL: ESD recovering is done\n");

	return 0;
}

static void mdss_panel_esd_te_monitor(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;
	static bool dropbox_sent;

	INIT_COMPLETION(ctrl->panel_esd_data.te_detected);

	enable_irq(ctrl->panel_esd_data.te_irq);

	ret = wait_for_completion_timeout(&ctrl->panel_esd_data.te_detected,
					msecs_to_jiffies(TE_MONITOR_TO));
	if (ret == 0) {
		pr_warn("%s: No TE sig for %d usec. Trigger ESD recovery\n",
				__func__, TE_MONITOR_TO);
		mdss_panel_esd_recovery_start(&ctrl->panel_data);
		if (!dropbox_sent) {
			dropbox_queue_event_text("display_issue",
						ESD_TE_DROPBOX_MSG,
						strlen(ESD_TE_DROPBOX_MSG));
			dropbox_sent = true;
		}
	}

}

#ifdef MDSS_PANEL_ESD_SELF_TRIGGER
static char disp_off[2] = {0x28, 0x0};  /* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc dispoff_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 1, sizeof(disp_off)}, disp_off
};

static void mdss_dsi_panel_dispoff_dcs(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct dcs_cmd_req cmdreq;

	pr_debug("%s+:\n", __func__);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dispoff_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}
#endif

static void mdss_panel_esd_work(struct work_struct *work)
{
	u8 pwr_mode = 0;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	static bool dropbox_sent;
	struct mdss_panel_esd_pdata *esd_data;
#ifdef MDSS_PANEL_ESD_SELF_TRIGGER
	static int esd_count;
	static int esd_trigger_cnt;
#endif

	ctrl = container_of(work, struct mdss_dsi_ctrl_pdata, esd_work.work);
	if (ctrl == NULL) {
		pr_err("%s: invalid ctrl\n", __func__);
		return;
	} else
		pr_debug("%s: ctrl = %p\n", __func__, ctrl);

	esd_data = &ctrl->panel_esd_data;
	mdss_dsi_panel_lock_mutex(&ctrl->panel_data);
	if (!ctrl->panel_data.panel_info.panel_power_on) {
		mdss_dsi_panel_unlock_mutex(&ctrl->panel_data);
		return;
	}
	mdss_dsi_get_pwr_mode(&ctrl->panel_data, &pwr_mode);

	pr_debug("%s: is called. pwr_mode = 0x%x\n", __func__, pwr_mode);

#ifdef MDSS_PANEL_ESD_SELF_TRIGGER
	if (esd_count > MDSS_PANEL_ESD_TE_TRIGGER)
		esd_count = 0;
	esd_count++;
	if (esd_count == MDSS_PANEL_ESD_CNT_MAX) {
		pwr_mode = 0x00;
		pr_info("%s(%d): start to ESD test\n", __func__,
							esd_trigger_cnt++);
	}
#endif
	if ((pwr_mode & esd_data->esd_pwr_mode_chk) !=
					esd_data->esd_pwr_mode_chk) {
		pr_warn("%s: ESD detected pwr_mode =0x%x expected = 0x%x\n",
					__func__, pwr_mode,
					esd_data->esd_pwr_mode_chk);
		mdss_panel_esd_recovery_start(&ctrl->panel_data);
		if (!dropbox_sent) {
			dropbox_queue_event_text("display_issue",
						ESD_DROPBOX_MSG,
						strlen(ESD_DROPBOX_MSG));
			dropbox_sent = true;
		}
	} else {
		pr_debug("%s. esd_det_mode=%d\n",
				__func__, esd_data->esd_detect_mode);
		if (esd_data->esd_detect_mode == ESD_TE_DET &&
			(esd_data->esd_pwr_mode_chk & PWR_MODE_DISON)) {
#ifdef MDSS_PANEL_ESD_SELF_TRIGGER
			if (esd_count == MDSS_PANEL_ESD_TE_TRIGGER) {
				pr_warn("%s(%d): start to ESD test. "
					"turn off display\n", __func__,
					esd_trigger_cnt++);
				mdss_dsi_panel_dispoff_dcs(ctrl);
				esd_count = 0;
			}
#endif
			mdss_panel_esd_te_monitor(ctrl);
		}
	}

	if (ctrl->panel_data.panel_info.panel_power_on)
		queue_delayed_work(esd_data->esd_wq, &ctrl->esd_work,
						MDSS_PANEL_ESD_CHECK_PERIOD);
	mdss_dsi_panel_unlock_mutex(&ctrl->panel_data);
}

static int mdss_dsi_panel_esd_init(struct mdss_dsi_ctrl_pdata *ctrl)
{
	int ret;
	struct mdss_panel_esd_pdata *esd_data = &ctrl->panel_esd_data;

	INIT_DELAYED_WORK_DEFERRABLE(&ctrl->esd_work, mdss_panel_esd_work);

	if (esd_data->esd_detect_mode == ESD_TE_DET) {
		init_completion(&esd_data->te_detected);
		esd_data->te_irq = gpio_to_irq(ctrl->disp_te_gpio);
		ret = request_irq(esd_data->te_irq,
				mdss_panel_esd_te_irq_handler,
				IRQF_TRIGGER_RISING, "mdss_panel_esd_te", ctrl);
		if (ret < 0) {
			pr_err("%s: unable to request IRQ %d\n",
						__func__, ctrl->disp_te_gpio);
			return -EAGAIN;
		}
	}

	return 0;
}

static int mdss_dsi_panel_esd(struct mdss_panel_data *pdata)
{
	int ret;
	static bool esd_work_queue_init;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	pr_debug("%s is called.\n", __func__);
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	if (ctrl->panel_config.esd_enable &&
			ctrl->panel_esd_data.esd_detection_run == false &&
			ctrl->panel_esd_data.esd_recovery_run == false) {
		if (esd_work_queue_init == false) {
			ret = mdss_dsi_panel_esd_init(ctrl);
			if (ret)
				return ret;
			esd_work_queue_init = true;
		}

		queue_delayed_work(ctrl->panel_esd_data.esd_wq, &ctrl->esd_work,
						MDSS_PANEL_ESD_CHECK_PERIOD);

		ctrl->panel_esd_data.esd_detection_run = true;

		pr_debug("%s: start the  ESD work queue\n", __func__);
	}

	return 0;
}

static int mdss_dsi_panel_cont_splash_on(struct mdss_panel_data *pdata)
{
	mmi_panel_notify(MMI_PANEL_EVENT_DISPLAY_ON, NULL);
	mdss_dsi_panel_esd(pdata);
	return 0;
}

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	u8 pwr_mode = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	pr_info("%s+: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	mdss_dsi_panel_regulator_on(pdata, 1);

	mdss_dsi_panel_reset(pdata, 1);

	if (ctrl->panel_config.bare_board == true) {
		mmi_panel_notify(MMI_PANEL_EVENT_DISPLAY_ON, NULL);
		pr_warn("%s: This is bare_board configuration\n", __func__);
		goto end;
	}

	if (ctrl->on_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);

	/* Send display on notification.  This will need to be revisited once
	   we implement command mode support the way we want, since display
	   may not be made visible to user until a point later than this */
	mmi_panel_notify(MMI_PANEL_EVENT_DISPLAY_ON, NULL);

	mdss_dsi_get_pwr_mode(pdata, &pwr_mode);
	/* validate screen is actually on */
	if ((pwr_mode & 0x04) != 0x04) {
		pr_err("%s: Display failure: DISON (0x04) bit not set\n",
			__func__);
		dropbox_queue_event_empty("display_issue");
	}

	mdss_dsi_panel_esd(pdata);

end:
	pr_info("%s-. Pwr_mode(0x0A) = 0x%x\n", __func__, pwr_mode);

	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s+: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	mipi  = &pdata->panel_info.mipi;
	mmi_panel_notify(MMI_PANEL_EVENT_PRE_DISPLAY_OFF, NULL);

	if (ctrl->panel_config.bare_board == true)
		goto disable_regs;

	if (ctrl->panel_config.esd_enable &&
			ctrl->panel_esd_data.esd_detection_run == true &&
			ctrl->panel_esd_data.esd_recovery_run == false) {
		cancel_delayed_work(&ctrl->esd_work);
		ctrl->panel_esd_data.esd_detection_run = false;
		pr_debug("%s: cancel the  ESD work queue\n", __func__);
	}

	if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds);

disable_regs:
	mdss_dsi_panel_reset(pdata, 0);
	mdss_dsi_panel_regulator_on(pdata, 0);

	pr_info("%s-:\n", __func__);

	return 0;
}


static int mdss_dsi_parse_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key)
{
	const char *data;
	int blen = 0, len;
	char *buf, *bp;
	struct dsi_ctrl_hdr *dchdr;
	int i, cnt;

	data = of_get_property(np, cmd_key, &blen);
	if (!data) {
		pr_err("%s: failed, key=%s\n", __func__, cmd_key);
		return -ENOMEM;
	}

	buf = kzalloc(sizeof(char) * blen, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	memcpy(buf, data, blen);

	/* scan dcs commands */
	bp = buf;
	len = blen;
	cnt = 0;
	while (len > sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > len) {
			pr_err("%s: dtsi cmd=%x error, len=%d",
				__func__, dchdr->dtype, dchdr->dlen);
			return -ENOMEM;
		}
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		cnt++;
	}

	if (len != 0) {
		pr_err("%s: dcs_cmd=%x len=%d error!",
				__func__, buf[0], blen);
		kfree(buf);
		return -ENOMEM;
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!pcmds->cmds)
		return -ENOMEM;

	pcmds->cmd_cnt = cnt;
	pcmds->buf = buf;
	pcmds->blen = blen;

	bp = buf;
	len = blen;
	for (i = 0; i < cnt; i++) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		len -= sizeof(*dchdr);
		bp += sizeof(*dchdr);
		pcmds->cmds[i].dchdr = *dchdr;
		pcmds->cmds[i].payload = bp;
		bp += dchdr->dlen;
		len -= dchdr->dlen;
	}

	data = of_get_property(np, link_key, NULL);
	if (!strncmp(data, "dsi_hs_mode", 11))
		pcmds->link_state = DSI_HS_MODE;
	else
		pcmds->link_state = DSI_LP_MODE;

	pr_debug("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;
}

static int mdss_panel_parse_panel_reg_dt(struct mdss_dsi_ctrl_pdata *ctrl_pdata,
					struct device_node *node)
{
	int rc;

	pr_debug("%s is called\n", __func__);

	if (!ctrl_pdata->pdev) {
		pr_err("%s: invalid pdev\n", __func__);
		return -EINVAL;
	}
	/* Parse the regulator information */
	if (ctrl_pdata->get_dt_vreg_data) {
		rc = ctrl_pdata->get_dt_vreg_data(&ctrl_pdata->pdev->dev,
						&ctrl_pdata->panel_vregs, node);
		if (rc)
			pr_err("%s: failed to get vreg data from dt. rc=%d\n",
								__func__, rc);
	} else {
		pr_err("%s: get_dt_vreg_data is not defined\n", __func__);
		rc = -EINVAL;
	}

	return rc;
}

static int mdss_panel_dt_get_dst_fmt(u32 bpp, char mipi_mode, u32 pixel_packing,
				char *dst_format)
{
	int rc = 0;
	switch (bpp) {
	case 3:
		*dst_format = DSI_CMD_DST_FORMAT_RGB111;
		break;
	case 8:
		*dst_format = DSI_CMD_DST_FORMAT_RGB332;
		break;
	case 12:
		*dst_format = DSI_CMD_DST_FORMAT_RGB444;
		break;
	case 16:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB565;
			break;
		default:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB565;
			break;
		}
		break;
	case 18:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			if (pixel_packing == 0)
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
			else
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666_LOOSE;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB666;
			break;
		default:
			if (pixel_packing == 0)
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666;
			else
				*dst_format = DSI_VIDEO_DST_FORMAT_RGB666_LOOSE;
			break;
		}
		break;
	case 24:
		switch (mipi_mode) {
		case DSI_VIDEO_MODE:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
			break;
		case DSI_CMD_MODE:
			*dst_format = DSI_CMD_DST_FORMAT_RGB888;
			break;
		default:
			*dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
			break;
		}
		break;
	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}


static int mdss_dsi_parse_fbc_params(struct device_node *np,
				struct mdss_panel_info *panel_info)
{
	int rc, fbc_enabled = 0;
	u32 tmp;

	fbc_enabled = of_property_read_bool(np,	"qcom,mdss-dsi-fbc-enable");
	if (fbc_enabled) {
		pr_debug("%s:%d FBC panel enabled.\n", __func__, __LINE__);
		panel_info->fbc.enabled = 1;
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-bpp", &tmp);
		panel_info->fbc.target_bpp =	(!rc ? tmp : panel_info->bpp);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-packing",
				&tmp);
		panel_info->fbc.comp_mode = (!rc ? tmp : 0);
		panel_info->fbc.qerr_enable = of_property_read_bool(np,
			"qcom,mdss-dsi-fbc-quant-error");
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-bias", &tmp);
		panel_info->fbc.cd_bias = (!rc ? tmp : 0);
		panel_info->fbc.pat_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-pat-mode");
		panel_info->fbc.vlc_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-vlc-mode");
		panel_info->fbc.bflc_enable = of_property_read_bool(np,
				"qcom,mdss-dsi-fbc-bflc-mode");
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-h-line-budget",
				&tmp);
		panel_info->fbc.line_x_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-budget-ctrl",
				&tmp);
		panel_info->fbc.block_x_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-block-budget",
				&tmp);
		panel_info->fbc.block_budget = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossless-threshold", &tmp);
		panel_info->fbc.lossless_mode_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossy-threshold", &tmp);
		panel_info->fbc.lossy_mode_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np, "qcom,mdss-dsi-fbc-rgb-threshold",
				&tmp);
		panel_info->fbc.lossy_rgb_thd = (!rc ? tmp : 0);
		rc = of_property_read_u32(np,
				"qcom,mdss-dsi-fbc-lossy-mode-idx", &tmp);
		panel_info->fbc.lossy_mode_idx = (!rc ? tmp : 0);
	} else {
		pr_debug("%s:%d Panel does not support FBC.\n",
				__func__, __LINE__);
		panel_info->fbc.enabled = 0;
		panel_info->fbc.target_bpp =
			panel_info->bpp;
	}
	return 0;
}

static int mdss_panel_parse_reset_seq(struct device_node *np, const char *name,
				int rst_seq[MDSS_DSI_RST_SEQ_LEN], int *rst_len)
{
	int num_u32 = 0;
	int rc;
	struct property *pp;
	u32 tmp[MDSS_DSI_RST_SEQ_LEN];

	*rst_len = 0;

	pp = of_find_property(np, name, &num_u32);
	num_u32 /= sizeof(u32);
	if (!pp || num_u32 == 0 || num_u32 > MDSS_DSI_RST_SEQ_LEN ||
		num_u32 % 2)
		pr_err("%s:%d, error reading property %s, num_u32 = %d\n",
			__func__, __LINE__, name, num_u32);
	else {
		rc = of_property_read_u32_array(np, name, tmp, num_u32);
		if (rc)
			pr_err("%s:%d, unable to read %s, rc = %d\n",
				__func__, __LINE__, name, rc);
		else {
			memcpy(rst_seq, tmp, num_u32 * sizeof(u32));
			*rst_len = num_u32;
		}
	}
	return 0;
}

static int mdss_dsi_parse_reset_seq(struct device_node *np,
		u32 rst_seq[MDSS_DSI_RST_SEQ_LEN], u32 *rst_len,
		const char *name)
{
	int num = 0, i;
	int rc;
	struct property *data;
	u32 tmp[MDSS_DSI_RST_SEQ_LEN];
	*rst_len = 0;
	data = of_find_property(np, name, &num);
	num /= sizeof(u32);
	if (!data || !num || num > MDSS_DSI_RST_SEQ_LEN || num % 2) {
		pr_debug("%s:%d, error reading %s, length found = %d\n",
			__func__, __LINE__, name, num);
	} else {
		rc = of_property_read_u32_array(np, name, tmp, num);
		if (rc)
			pr_debug("%s:%d, error reading %s, rc = %d\n",
				__func__, __LINE__, name, rc);
		else {
			for (i = 0; i < num; ++i)
				rst_seq[i] = tmp[i];
			*rst_len = num;
		}
	}
	return 0;
}

void mdss_panel_set_reg_boot_on(struct device_node *node,
				struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	if (of_property_read_bool(node, "qcom,cont-splash-enabled")) {
		ctrl_pdata->panel_vregs.boot_on = true;
		ctrl_pdata->power_data.boot_on = true;
	}
}

int mdss_panel_parse_panel_config_dt(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct device_node *np;
	const char *pname;
	u32 panel_ver;

	np = of_find_node_by_path("/chosen");
	ctrl_pdata->panel_config.esd_disable_bl =
				of_property_read_bool(np, "mmi,esd");
	if (ctrl_pdata->panel_config.esd_disable_bl)
		pr_warn("%s: ESD detection is disabled by UTAGS\n", __func__);

	if (of_property_read_bool(np, "mmi,bare_board") == true)
		ctrl_pdata->panel_config.bare_board = true;

	ctrl_pdata->panel_config.panel_ver = MDSS_PANEL_DEFAULT_VER;
	of_property_read_u64(np, "mmi,panel_ver",
					&ctrl_pdata->panel_config.panel_ver);

	pname = of_get_property(np, "mmi,panel_name", NULL);
	if (!pname || strlen(pname) == 0) {
		pr_warn("Failed to get mmi,panel_name\n");
		strlcpy(ctrl_pdata->panel_config.panel_name,
			MDSS_PANEL_UNKNOWN_NAME,
			sizeof(ctrl_pdata->panel_config.panel_name));
	} else
		strlcpy(ctrl_pdata->panel_config.panel_name, pname,
			sizeof(ctrl_pdata->panel_config.panel_name));

	pr_debug("%s: esd_disable_bl=%d bare_board_bl=%d, " \
		" factory_cable=%d panel_name=%s bare_board=%d\n",
		__func__, ctrl_pdata->panel_config.esd_disable_bl,
		of_property_read_bool(np, "mmi,bare_board"),
		of_property_read_bool(np, "mmi,factory-cable"),
		ctrl_pdata->panel_config.panel_name,
		ctrl_pdata->panel_config.bare_board);

	panel_ver = (u32)ctrl_pdata->panel_config.panel_ver;
	pr_info("BL: panel=%s, manufacture_id(0xDA)= 0x%x controller_ver(0xDB)= 0x%x controller_drv_ver(0XDC)= 0x%x, full=0x%016llx\n",
		ctrl_pdata->panel_config.panel_name,
		panel_ver & 0xff, (panel_ver & 0xff00) >> 8,
		(panel_ver & 0xff0000) >> 16,
		ctrl_pdata->panel_config.panel_ver);

	of_node_put(np);

	return 0;
}

static int mdss_dsi_panel_reg_read(struct mdss_panel_data *pdata, u8 reg,
				int mode, size_t size, u8 *buffer)
{
	int old_tx_mode;
	int ret;
	struct dcs_cmd_req cmdreq;
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct dsi_cmd_desc reg_read_cmd = {
		.dchdr.dtype = DTYPE_DCS_READ,
		.dchdr.last = 1,
		.dchdr.vc = 0,
		.dchdr.ack = 1,
		.dchdr.wait = 1,
		.dchdr.dlen = 1,
		.payload = &reg
	};

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	if (size > MDSS_DSI_LEN) {
		pr_warn("%s: size %d, max rx length is %d.\n", __func__,
				size, MDSS_DSI_LEN);
		return -EINVAL;
	}

	pr_debug("%s: Reading %d bytes from 0x%02x\n", __func__, size, reg);

	old_tx_mode = mdss_get_tx_power_mode(pdata);
	if (mode != old_tx_mode)
		mdss_set_tx_power_mode(mode, pdata);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &reg_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = size;
	cmdreq.cb = NULL; /* call back */
	cmdreq.rbuf = kmalloc(MDSS_DSI_LEN, GFP_KERNEL);
	if (!cmdreq.rbuf) {
		ret = -ENOMEM;
		goto err1;
	}
	ret = mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	if (ret <= 0) {
		pr_err("%s: Error reading %d bytes from reg 0x%02x error.\n",
			__func__, size, (unsigned int) reg);
		ret = -EFAULT;
	} else {
		memcpy(buffer, cmdreq.rbuf, size);
		ret = 0;
	}
	kfree(cmdreq.rbuf);

err1:
	if (mode != old_tx_mode)
		mdss_set_tx_power_mode(old_tx_mode, pdata);
	return ret;
}

static int mdss_dsi_panel_reg_write(struct mdss_panel_data *pdata, int mode,
				size_t size, u8 *buffer)
{
	int old_tx_mode;
	int ret = 0;
	struct dcs_cmd_req cmdreq;
	struct mdss_dsi_ctrl_pdata *ctrl;
	struct dsi_cmd_desc reg_write_cmd = {
		.dchdr.dtype = DTYPE_DCS_LWRITE,
		.dchdr.last = 1,
		.dchdr.vc = 0,
		.dchdr.ack = 0,
		.dchdr.wait = 0,
		.dchdr.dlen = size,
		.payload = buffer
	};

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}
	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_debug("%s: Writing %d bytes to 0x%02x\n", __func__, size, buffer[0]);

	old_tx_mode = mdss_get_tx_power_mode(pdata);
	if (mode != old_tx_mode)
		mdss_set_tx_power_mode(mode, pdata);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &reg_write_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	ret = mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	if (ret <= 0) {
		pr_err("%s: Failed writing %d bytes to 0x%02x.\n",
			__func__, size, buffer[0]);
		ret = -EFAULT;
	} else
		ret = 0;

	if (mode != old_tx_mode)
		mdss_set_tx_power_mode(old_tx_mode, pdata);

	return ret;
}

static int mdss_panel_parse_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	u32 tmp;
	int rc, i, len;
	const char *data;
	static const char *pdest;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);

	pr_debug("%s is called\n", __func__);
	rc = mdss_panel_parse_panel_reg_dt(ctrl_pdata, np);
	if (rc)
		return rc;

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-width", &tmp);

	if (rc) {
		pr_err("%s:%d, panel width not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pinfo->xres = (!rc ? tmp : 640);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-height", &tmp);
	if (rc) {
		pr_err("%s:%d, panel height not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	pinfo->yres = (!rc ? tmp : 480);

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-physical-width-dimension", &tmp);
	pinfo->physical_width = (!rc ? tmp : 0);
	rc = of_property_read_u32(np,
		"qcom,mdss-pan-physical-height-dimension", &tmp);
	pinfo->physical_height = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-left-border", &tmp);
	pinfo->lcdc.xres_pad = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-right-border", &tmp);
	if (!rc)
		pinfo->lcdc.xres_pad += tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-top-border", &tmp);
	pinfo->lcdc.yres_pad = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-bottom-border", &tmp);
	if (!rc)
		pinfo->lcdc.yres_pad += tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bpp", &tmp);
	if (rc) {
		pr_err("%s:%d, bpp not specified\n", __func__, __LINE__);
		return -EINVAL;
	}
	pinfo->bpp = (!rc ? tmp : 24);
	pinfo->mipi.mode = DSI_VIDEO_MODE;
	data = of_get_property(np, "qcom,mdss-dsi-panel-type", NULL);
	if (data && !strncmp(data, "dsi_cmd_mode", 12))
		pinfo->mipi.mode = DSI_CMD_MODE;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-pixel-packing", &tmp);
	tmp = (!rc ? tmp : 0);
	rc = mdss_panel_dt_get_dst_fmt(pinfo->bpp,
		pinfo->mipi.mode, tmp,
		&(pinfo->mipi.dst_format));
	if (rc) {
		pr_debug("%s: problem determining dst format. Set Default\n",
			__func__);
		pinfo->mipi.dst_format =
			DSI_VIDEO_DST_FORMAT_RGB888;
	}
	pdest = of_get_property(np,
		"qcom,mdss-dsi-panel-destination", NULL);

	if (strlen(pdest) != 9) {
		pr_err("%s: Unknown pdest specified\n", __func__);
		return -EINVAL;
	}
	if (!strncmp(pdest, "display_1", 9))
		pinfo->pdest = DISPLAY_1;
	else if (!strncmp(pdest, "display_2", 9))
		pinfo->pdest = DISPLAY_2;
	else {
		pr_debug("%s: pdest not specified. Set Default\n",
							__func__);
		pinfo->pdest = DISPLAY_1;
	}
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-front-porch", &tmp);
	pinfo->lcdc.h_front_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-back-porch", &tmp);
	pinfo->lcdc.h_back_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-pulse-width", &tmp);
	pinfo->lcdc.h_pulse_width = (!rc ? tmp : 2);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-h-sync-skew", &tmp);
	pinfo->lcdc.hsync_skew = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-back-porch", &tmp);
	pinfo->lcdc.v_back_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-front-porch", &tmp);
	pinfo->lcdc.v_front_porch = (!rc ? tmp : 6);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-v-pulse-width", &tmp);
	pinfo->lcdc.v_pulse_width = (!rc ? tmp : 2);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-underflow-color", &tmp);
	pinfo->lcdc.underflow_clr = (!rc ? tmp : 0xff);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-border-color", &tmp);
	pinfo->lcdc.border_clr = (!rc ? tmp : 0);
	pinfo->bklt_ctrl = UNKNOWN_CTRL;

	/* lcdc_tune is the same as lcdc by default, but can be overridden */
	memcpy(&pinfo->lcdc_tune, &pinfo->lcdc,
		sizeof(struct lcd_panel_info));

	rc = of_property_read_u32(np, "qcom,mdss-dsi-tune-h-front-porch", &tmp);
	if (!rc)
		pinfo->lcdc_tune.h_front_porch = tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-tune-h-back-porch", &tmp);
	if (!rc)
		pinfo->lcdc_tune.h_back_porch = tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-tune-h-pulse-width", &tmp);
	if (!rc)
		pinfo->lcdc_tune.h_pulse_width = tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-tune-h-sync-skew", &tmp);
	if (!rc)
		pinfo->lcdc_tune.hsync_skew = tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-tune-v-back-porch", &tmp);
	if (!rc)
		pinfo->lcdc_tune.v_back_porch = tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-tune-v-front-porch", &tmp);
	if (!rc)
		pinfo->lcdc_tune.v_front_porch = tmp;
	rc = of_property_read_u32(np, "qcom,mdss-dsi-tune-v-pulse-width", &tmp);
	if (!rc)
		pinfo->lcdc_tune.v_pulse_width = tmp;

	data = of_get_property(np, "qcom,mdss-dsi-bl-pmic-control-type", NULL);
	if (data) {
		if (!strncmp(data, "bl_ctrl_wled", 12)) {
			led_trigger_register_simple("bkl-trigger",
				&bl_led_trigger);
			pr_debug("%s: SUCCESS-> WLED TRIGGER register\n",
				__func__);
			ctrl_pdata->bklt_ctrl = BL_WLED;
		} else if (!strncmp(data, "bl_ctrl_pwm", 11)) {
			ctrl_pdata->bklt_ctrl = BL_PWM;
			rc = of_property_read_u32(np,
				"qcom,mdss-dsi-bl-pmic-pwm-frequency", &tmp);
			if (rc) {
				pr_err("%s:%d, Error, panel pwm_period\n",
						__func__, __LINE__);
				return -EINVAL;
			}
			ctrl_pdata->pwm_period = tmp;
			rc = of_property_read_u32(np,
				"qcom,mdss-dsi-bl-pmic-bank-select", &tmp);
			if (rc) {
				pr_err("%s:%d, Error, dsi lpg channel\n",
						__func__, __LINE__);
				return -EINVAL;
			}
			ctrl_pdata->pwm_lpg_chan = tmp;
			tmp = of_get_named_gpio(np,
				"qcom,mdss-dsi-pwm-gpio", 0);
			ctrl_pdata->pwm_pmic_gpio = tmp;
		} else if (!strncmp(data, "bl_ctrl_dcs", 11)) {
			ctrl_pdata->bklt_ctrl = BL_DCS_CMD;
		}
	}
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-min-level", &tmp);
	pinfo->bl_min = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-max-level", &tmp);
	pinfo->bl_max = (!rc ? tmp : 255);
	ctrl_pdata->bklt_max = pinfo->bl_max;

	rc = of_property_read_u32(np, "qcom,mdss-dsi-interleave-mode", &tmp);
	pinfo->mipi.interleave_mode = (!rc ? tmp : 0);

	pinfo->mipi.vsync_enable = of_property_read_bool(np,
		"qcom,mdss-dsi-te-check-enable");
	pinfo->mipi.hw_vsync_mode = of_property_read_bool(np,
		"qcom,mdss-dsi-te-using-te-pin");

	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-h-sync-pulse", &tmp);
	pinfo->mipi.pulse_mode_hsa_he = (!rc ? tmp : false);

	pinfo->mipi.hfp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hfp-power-mode");
	pinfo->mipi.hsa_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hsa-power-mode");
	pinfo->mipi.hbp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-hbp-power-mode");
	pinfo->mipi.bllp_power_stop = of_property_read_bool(np,
		"qcom,mdss-dsi-bllp-power-mode");
	pinfo->mipi.eof_bllp_power_stop = of_property_read_bool(
		np, "qcom,mdss-dsi-bllp-eof-power-mode");
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-traffic-mode", &tmp);
	pinfo->mipi.traffic_mode =
			(!rc ? tmp : DSI_NON_BURST_SYNCH_PULSE);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-dcs-command", &tmp);
	pinfo->mipi.insert_dcs_cmd =
			(!rc ? tmp : 1);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-v-sync-continue-lines", &tmp);
	pinfo->mipi.wr_mem_continue =
			(!rc ? tmp : 0x3c);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-v-sync-rd-ptr-irq-line", &tmp);
	pinfo->mipi.wr_mem_start =
			(!rc ? tmp : 0x2c);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-pin-select", &tmp);
	pinfo->mipi.te_sel =
			(!rc ? tmp : 1);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-virtual-channel-id", &tmp);
	pinfo->mipi.vc = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-color-order", &tmp);
	pinfo->mipi.rgb_swap = (!rc ? tmp : DSI_RGB_SWAP_RGB);
	pinfo->mipi.data_lane0 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-0-state");
	pinfo->mipi.data_lane1 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-1-state");
	pinfo->mipi.data_lane2 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-2-state");
	pinfo->mipi.data_lane3 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-3-state");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-lane-map", &tmp);
	pinfo->mipi.dlane_swap = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-pre", &tmp);
	pinfo->mipi.t_clk_pre = (!rc ? tmp : 0x24);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-post", &tmp);
	pinfo->mipi.t_clk_post = (!rc ? tmp : 0x03);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-stream", &tmp);
	pinfo->mipi.stream = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-mdp-trigger", &tmp);
	pinfo->mipi.mdp_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (pinfo->mipi.mdp_trigger > 6) {
		pr_err("%s:%d, Invalid mdp trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		pinfo->mipi.mdp_trigger =
					DSI_CMD_TRIGGER_SW;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-dma-trigger", &tmp);
	pinfo->mipi.dma_trigger =
			(!rc ? tmp : DSI_CMD_TRIGGER_SW);
	if (pinfo->mipi.dma_trigger > 6) {
		pr_err("%s:%d, Invalid dma trigger. Forcing to sw trigger",
						 __func__, __LINE__);
		pinfo->mipi.dma_trigger =
					DSI_CMD_TRIGGER_SW;
	}
	data = of_get_property(np, "qcom,mdss-dsi-panel-mode-gpio-state", &tmp);
	if (data) {
		if (!strcmp(data, "high"))
			pinfo->mode_gpio_state = MODE_GPIO_HIGH;
		else if (!strcmp(data, "low"))
			pinfo->mode_gpio_state = MODE_GPIO_LOW;
	} else {
		pinfo->mode_gpio_state = MODE_GPIO_NOT_VALID;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-frame-rate", &tmp);
	pinfo->mipi.frame_rate = (!rc ? tmp : 60);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-clock-rate", &tmp);
	pinfo->clk_rate = (!rc ? tmp : 0);
	data = of_get_property(np, "qcom,mdss-dsi-panel-timings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		pinfo->mipi.dsi_phy_db.timing[i] = data[i];

	mdss_dsi_parse_fbc_params(np, pinfo);

	mdss_dsi_parse_reset_seq(np, pinfo->rst_seq, &(pinfo->rst_seq_len),
		"qcom,mdss-dsi-reset-sequence");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
		"qcom,mdss-dsi-on-command", "qcom,mdss-dsi-on-command-state");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->off_cmds,
		"qcom,mdss-dsi-off-command", "qcom,mdss-dsi-off-command-state");

	if (ctrl_pdata->panel_config.bare_board == true ||
			ctrl_pdata->panel_config.esd_disable_bl == true) {
		ctrl_pdata->panel_config.esd_enable = false;

		pr_info("%s: bare_board=%d esd_disable_bl=%d\n", __func__,
			ctrl_pdata->panel_config.bare_board,
			ctrl_pdata->panel_config.esd_disable_bl);

	} else {
		ctrl_pdata->panel_config.esd_enable = !of_property_read_bool(np,
					"qcom,panel-esd-detect-disable");
		if (!(ctrl_pdata->panel_config.esd_enable))
			pr_warn("%s: ESD detetion is disabled by DTS\n",
								__func__);
		else {
			of_property_read_u32(np,
				"qcom,panel-esd-power-mode-chk",
				&ctrl_pdata->panel_esd_data.esd_pwr_mode_chk);

			of_property_read_u32(np, "qcom,mdss-dsi-esd-det-mode",
				&ctrl_pdata->panel_esd_data.esd_detect_mode);
		}
	}

	mdss_panel_parse_reset_seq(np, "qcom,panel-en-reset-sequence",
				ctrl_pdata->rst_seq,
				&ctrl_pdata->rst_seq_len);

	mdss_panel_parse_reset_seq(np, "qcom,panel-dis-reset-sequence",
				ctrl_pdata->dis_rst_seq,
				&ctrl_pdata->dis_rst_seq_len);

	ctrl_pdata->panel_config.disallow_panel_pwr_off =
		of_property_read_bool(np, "qcom,panel-disallow-pwr-off");

	return 0;

error:
	return -EINVAL;
}

bool mdss_dsi_match_chosen_panel(struct device_node *np,
				struct mdss_panel_config *pconfig)
{
	const char *panel_name;
	u64 panel_ver_min;
	u64 panel_ver_max;

	if (of_property_read_u64(np, "mmi,panel_ver_min", &panel_ver_min))
		return false;

	if (of_property_read_u64(np, "mmi,panel_ver_max", &panel_ver_max))
		return false;

	panel_name = of_get_property(np, "mmi,panel_name", NULL);
	if (!panel_name) {
		pr_err("%s: Panel name not set\n", __func__);
		return false;
	}

	/* If we are in bare board mode or if the panel name isn't set, use
	   first default version panel we find */
	if ((pconfig->bare_board ||
		!strcmp(pconfig->panel_name,
			MDSS_PANEL_UNKNOWN_NAME)) &&
		panel_ver_max == MDSS_PANEL_DEFAULT_VER)
		pr_warn("%s: In bare board mode, using panel %s [0x%016llx, 0x%016llx]\n",
			__func__, panel_name, panel_ver_min, panel_ver_max);
	else if (!strcmp(panel_name, pconfig->panel_name) &&
			pconfig->panel_ver >= panel_ver_min &&
			pconfig->panel_ver <= panel_ver_max)
		pr_info("%s: Found match for panel %s, version 0x%016llx [0x%016llx, 0x%016llx]\n",
			__func__, panel_name, pconfig->panel_ver,
			panel_ver_min, panel_ver_max);
	else {
		pr_debug("%s: Panel %s [0x%016llx, 0x%016llx] did not match\n",
			__func__, panel_name, panel_ver_min, panel_ver_max);
		return false;
	}

	return true;
}

int mdss_dsi_panel_init(struct device_node *node,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool cmd_cfg_cont_splash)
{
	int rc = 0;
	static const char *panel_name;
	bool cont_splash_enabled;
	bool partial_update_enabled;

	if (!node) {
		pr_err("%s: no panel node\n", __func__);
		return -ENODEV;
	}

	pr_debug("%s:%d\n", __func__, __LINE__);
	panel_name = of_get_property(node, "qcom,mdss-dsi-panel-name", NULL);
	if (!panel_name)
		pr_info("%s:%d, Panel name not specified\n",
						__func__, __LINE__);
	else {
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

		/*
		 * When BL can not read from panel, and if factory cable is
		 * detected, then BL will pass
		 * "qcom,mdss_dsi_mot_dummy_qhd_video" to kernel boot arg, and
		 * this panel "miPI_mot_video_dummy_qhd" node will be
		 * used then this will be a bare_board configuration.
		 */
		if (strncmp(panel_name, "mipi_mot_video_dummy_qhd", 24) == 0) {
			ctrl_pdata->panel_config.bare_board = true;
			pr_warn("%s: This is bare_board config.", __func__);
		}
	}

	rc = mdss_panel_parse_dt(node, ctrl_pdata);
	if (rc) {
		pr_err("%s:%d panel dt parse failed\n", __func__, __LINE__);
		return rc;
	}

	if (cmd_cfg_cont_splash)
		cont_splash_enabled = of_property_read_bool(node,
				"qcom,cont-splash-enabled");
	else
		cont_splash_enabled = false;
	if (!cont_splash_enabled) {
		pr_info("%s:%d Continuous splash flag not found.\n",
				__func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 0;
	} else {
		pr_info("%s:%d Continuous splash flag enabled.\n",
				__func__, __LINE__);

		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 1;
	}

	partial_update_enabled = of_property_read_bool(node,
						"qcom,partial-update-enabled");
	if (partial_update_enabled) {
		pr_info("%s:%d Partial update enabled.\n", __func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.partial_update_enabled = 1;
		ctrl_pdata->partial_update_fnc = mdss_dsi_panel_partial_update;
	} else {
		pr_info("%s:%d Partial update disabled.\n", __func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.partial_update_enabled = 0;
		ctrl_pdata->partial_update_fnc = NULL;
	}

	/* If it is bare board, disable splash feature. */
	if (ctrl_pdata->panel_config.bare_board)
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 0;

	ctrl_pdata->on = mdss_dsi_panel_on;
	ctrl_pdata->off = mdss_dsi_panel_off;
	ctrl_pdata->panel_data.set_backlight = mdss_dsi_panel_bl_ctrl;
	ctrl_pdata->lock_mutex = mdss_dsi_panel_lock_mutex;
	ctrl_pdata->unlock_mutex = mdss_dsi_panel_unlock_mutex;
	ctrl_pdata->reg_read = mdss_dsi_panel_reg_read;
	ctrl_pdata->reg_write = mdss_dsi_panel_reg_write;
	ctrl_pdata->cont_splash_on = mdss_dsi_panel_cont_splash_on;


	return 0;
}
