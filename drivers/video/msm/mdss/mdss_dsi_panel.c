/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/qpnp/pwm.h>
#include <linux/err.h>
#include <linux/dropbox.h>
#include <linux/uaccess.h>
#include <linux/msm_mdp.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>


#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <mach/mmi_panel_notifier.h>

#include "mdss_dsi.h"
#include "mdss_fb.h"
#include "dsi_v2.h"

#define DT_CMD_HDR 6
#define DROPBOX_DISPLAY_ISSUE "display_issue"
#define ESD_DROPBOX_MSG "ESD event detected"
#define ESD_TE_DROPBOX_MSG "ESD TE event detected"
#define ESD_SENSORHUB_DROPBOX_MSG "ESD sensorhub detected"
#define PWR_MODE_BLACK_DROPBOX_MSG "PWR_MODE black screen detected"
#define PWR_MODE_FAIL_DROPBOX_MSG "PWR_MODE read failure"
#define PWR_MODE_INVALID_DROPBOX_MSG "PWR_MODE invalid mode detected"
#define PWR_MODE_MISMATCH_DROPBOX_MSG "PWR_MODE mis-match sensorhub reported"

/*
 * MDSS_PANEL_ESD_SELF_TRIGGER is triggered ESD recovery depending how many
 * times of MDSS_PANEL_ESD_CNT_MAX detection
 */
/* #define MDSS_PANEL_ESD_SELF_TRIGGER  1 */
#define MDSS_PANEL_ESD_CNT_MAX 3
#define MDSS_PANEL_ESD_TE_TRIGGER (MDSS_PANEL_ESD_CNT_MAX * 2)

#define MDSS_PANEL_DEFAULT_VER 0xffffffffffffffff
#define MDSS_PANEL_UNKNOWN_NAME "unknown"
#define TE_MONITOR_TO  68
#define TE_PULSE_USEC  100

#define PWR_MODE_DISON 0x4

DEFINE_LED_TRIGGER(bl_led_trigger);


static DECLARE_COMPLETION(bl_on_delay_completion);
static enum hrtimer_restart mdss_dsi_panel_bl_on_defer_timer_expire(
			   struct hrtimer *timer)
{
	complete_all(&bl_on_delay_completion);
	return HRTIMER_NORESTART;
}

static void mdss_dsi_panel_bl_on_defer_start(struct mdss_dsi_ctrl_pdata *ctrl)
{
	ktime_t delay_time;
	struct mdss_panel_info *pinfo;

	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->bl_on_defer_delay) {
		delay_time = ktime_set(0, pinfo->bl_on_defer_delay * 1000000);
		hrtimer_cancel(&pinfo->bl_on_defer_hrtimer);
		INIT_COMPLETION(bl_on_delay_completion);
		hrtimer_start(&pinfo->bl_on_defer_hrtimer,
				delay_time,
				HRTIMER_MODE_REL);
	}
}

static void mdss_dsi_panel_bl_on_defer_wait(struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo;
	pinfo = &(ctrl->panel_data.panel_info);
	if (pinfo->bl_on_defer_delay &&
	   !pinfo->cont_splash_enabled) {
		wait_for_completion_timeout(&bl_on_delay_completion,
				msecs_to_jiffies(pinfo->bl_on_defer_delay) + 1);
	}
}

void mdss_dsi_panel_pwm_cfg(struct mdss_dsi_ctrl_pdata *ctrl)
{
	ctrl->pwm_bl = pwm_request(ctrl->pwm_lpg_chan, "lcd-bklt");
	if (ctrl->pwm_bl == NULL || IS_ERR(ctrl->pwm_bl)) {
		pr_err("%s: Error: lpg_chan=%d pwm request failed",
				__func__, ctrl->pwm_lpg_chan);
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

	ret = pwm_config_us(ctrl->pwm_bl, duty, ctrl->pwm_period);
	if (ret) {
		pr_err("%s: pwm_config_us() failed err=%d.\n", __func__, ret);
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

	dcs_cmd[0] = cmd0;
	dcs_cmd[1] = cmd1;
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &dcs_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = len;
	cmdreq.rbuf = rbuf;
	cmdreq.cb = fxn; /* call back */
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	/*
	 * blocked here, until call back called
	 */

	return 0;
}

static int mdss_dsi_panel_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
			struct dsi_panel_cmds *pcmds)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = pcmds->cmds;
	cmdreq.cmds_cnt = pcmds->cmd_cnt;
	cmdreq.flags = CMD_REQ_COMMIT;

	/*Panel ON/Off commands should be sent in DSI Low Power Mode*/
	if (pcmds->link_state == DSI_LP_MODE)
		cmdreq.flags  |= CMD_REQ_LP_MODE;

	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	return 0;
}

static char led_pwm1[2] = {0x51, 0x0};	/* DTYPE_DCS_WRITE1 */
static struct dsi_cmd_desc backlight_cmd = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0, sizeof(led_pwm1)},
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

static int mdss_dsi_request_gpios(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	int rc = 0;

	if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
		rc = gpio_request(ctrl_pdata->disp_en_gpio,
						"disp_enable");
		if (rc) {
			pr_err("request disp_en gpio failed, rc=%d\n",
				       rc);
			goto disp_en_gpio_err;
		}
	}
	rc = gpio_request(ctrl_pdata->rst_gpio, "disp_rst_n");
	if (rc) {
		pr_err("request reset gpio failed, rc=%d\n",
			rc);
		goto rst_gpio_err;
	}
	if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
		rc = gpio_request(ctrl_pdata->mode_gpio, "panel_mode");
		if (rc) {
			pr_err("request panel mode gpio failed,rc=%d\n",
								rc);
			goto mode_gpio_err;
		}
	}
	return rc;

mode_gpio_err:
	gpio_free(ctrl_pdata->rst_gpio);
rst_gpio_err:
	if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
		gpio_free(ctrl_pdata->disp_en_gpio);
disp_en_gpio_err:
	return rc;
}

int mdss_dsi_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int i, rc = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
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
		return rc;
	}

	pr_debug("%s: enable = %d\n", __func__, enable);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (enable) {
		rc = mdss_dsi_request_gpios(ctrl_pdata);
		if (rc) {
			pr_err("gpio request failed\n");
			return rc;
		}
		if (!pinfo->cont_splash_enabled) {
			if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
				gpio_set_value((ctrl_pdata->disp_en_gpio), 1);

			for (i = 0; i < ctrl_pdata->rst_seq_len; ++i) {
				gpio_set_value((ctrl_pdata->rst_gpio),
					ctrl_pdata->rst_seq[i]);
				if (ctrl_pdata->rst_seq[++i])
					usleep_range(ctrl_pdata->rst_seq[i] * 1000,
						ctrl_pdata->rst_seq[i] * 1000);
			}
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
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio)) {
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
			gpio_free(ctrl_pdata->disp_en_gpio);
		}
		gpio_free(ctrl_pdata->rst_gpio);
		if (gpio_is_valid(ctrl_pdata->mode_gpio))
			gpio_free(ctrl_pdata->mode_gpio);
	}
	return rc;
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

	/*
	 * Some backlight controllers specify a minimum duty cycle
	 * for the backlight brightness. If the brightness is less
	 * than it, the controller can malfunction.
	 */

	if ((bl_level < pdata->panel_info.bl_min) && (bl_level != 0))
		bl_level = pdata->panel_info.bl_min;

	mdss_dsi_panel_bl_on_defer_wait(ctrl_pdata);

	switch (ctrl_pdata->bklt_ctrl) {
	case BL_WLED:
		led_trigger_event(bl_led_trigger, bl_level);
		break;
	case BL_PWM:
		mdss_dsi_panel_bklt_pwm(ctrl_pdata, bl_level);
		break;
	case BL_DCS_CMD:
		mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
		if (mdss_dsi_is_master_ctrl(ctrl_pdata)) {
			struct mdss_dsi_ctrl_pdata *sctrl =
				mdss_dsi_get_slave_ctrl();
			if (!sctrl) {
				pr_err("%s: Invalid slave ctrl data\n",
					__func__);
				return;
			}
			mdss_dsi_panel_bklt_dcs(sctrl, bl_level);
		}
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

static irqreturn_t mdss_panel_esd_te_irq_handler(int irq, void *ctrl_ptr)
{
	struct mdss_dsi_ctrl_pdata *ctrl =
				(struct mdss_dsi_ctrl_pdata *)ctrl_ptr;
	pr_debug("%s: is called\n", __func__);

	complete(&ctrl->panel_esd_data.te_detected);
	disable_irq_nosync(ctrl->panel_esd_data.te_irq);
	return IRQ_HANDLED;
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

int mdss_panel_check_status(struct mdss_dsi_ctrl_pdata *ctrl)
{
	static bool initialized, dropbox_sent;
	int ret = 0;
	u8 pwr_mode = 0;
	struct mdss_panel_esd_pdata *esd_data = &ctrl->panel_esd_data;
#ifdef MDSS_PANEL_ESD_SELF_TRIGGER
	static int esd_count;
	static int esd_trigger_cnt;
#endif

	if (!ctrl->panel_data.panel_info.panel_power_on) {
		ret = 1;
		goto end;
	}

	/* Set up TE pin monitor */
	if (!initialized && esd_data->esd_detect_mode == ESD_TE_DET) {
		pr_debug("%s: init ESD TE monitor.\n", __func__);
		init_completion(&esd_data->te_detected);
		esd_data->te_irq = gpio_to_irq(ctrl->disp_te_gpio);
		if (request_irq(esd_data->te_irq,
			mdss_panel_esd_te_irq_handler,
			IRQF_TRIGGER_RISING, "mdss_panel_esd_te", ctrl)) {
			pr_err("%s: unable to request IRQ %d\n",
						__func__, ctrl->disp_te_gpio);
			goto end;
		}
		initialized = true;
	}

	/* Check panel power mode */
	pr_debug("%s: Checking power mode\n", __func__);
	mdss_dsi_get_pwr_mode(&ctrl->panel_data, &pwr_mode);
#ifdef MDSS_PANEL_ESD_SELF_TRIGGER
	if (esd_count++ > MDSS_PANEL_ESD_TE_TRIGGER)
		esd_count = 0;
	if (esd_count == MDSS_PANEL_ESD_CNT_MAX) {
		pr_info("%s(%d): Start ESD power mode test\n", __func__,
				esd_trigger_cnt++);
		pwr_mode = 0x00;
	}
#endif
	if ((pwr_mode & esd_data->esd_pwr_mode_chk) !=
					esd_data->esd_pwr_mode_chk) {
		pr_warn("%s: ESD detected pwr_mode =0x%x expected = 0x%x\n",
			__func__, pwr_mode, esd_data->esd_pwr_mode_chk);
		if (!dropbox_sent) {
			dropbox_queue_event_text(DROPBOX_DISPLAY_ISSUE,
				ESD_DROPBOX_MSG, strlen(ESD_DROPBOX_MSG));
			dropbox_sent = true;
		}
		goto end;
	}

	/* Check panel TE pin status */
	if (esd_data->esd_detect_mode == ESD_TE_DET &&
		(esd_data->esd_pwr_mode_chk & PWR_MODE_DISON)) {
#ifdef MDSS_PANEL_ESD_SELF_TRIGGER
		if (esd_count == MDSS_PANEL_ESD_TE_TRIGGER) {
			pr_warn("%s(%d): Start ESD TE test.\n", __func__,
				esd_trigger_cnt);
			mdss_dsi_panel_dispoff_dcs(ctrl);
			esd_count = 0;
		}
#endif
		pr_debug("%s: Checking TE status.\n", __func__);
		INIT_COMPLETION(ctrl->panel_esd_data.te_detected);
		enable_irq(ctrl->panel_esd_data.te_irq);
		if (wait_for_completion_timeout(
			&ctrl->panel_esd_data.te_detected,
			msecs_to_jiffies(TE_MONITOR_TO)) == 0) {
			pr_warn("%s: No TE sig for %d usec.\n",  __func__,
							TE_MONITOR_TO);
			if (!dropbox_sent) {
				dropbox_queue_event_text(DROPBOX_DISPLAY_ISSUE,
					ESD_TE_DROPBOX_MSG,
					strlen(ESD_TE_DROPBOX_MSG));
				dropbox_sent = true;
			}
			goto end;
		}
	}
	ret = 1;
end:
	return ret;
}

static int mdss_dsi_panel_cont_splash_on(struct mdss_panel_data *pdata)
{
	mdss_dsi_panel_regulator_on(pdata, 1);

	if (pdata->panel_info.type == MIPI_VIDEO_PANEL &&
		pdata->panel_info.no_solid_fill)
		mdss_dsi_sw_reset(pdata);

	mmi_panel_notify(MMI_PANEL_EVENT_DISPLAY_ON, NULL);

#ifndef CONFIG_FB_MSM_MDSS_MDP3
	if (pdata->panel_info.hs_cmds_post_init)
		mdss_set_tx_power_mode(DSI_MODE_BIT_HS, pdata);
#endif
	pr_info("%s: Panel continuous splash finished\n", __func__);
	return 0;
}

static unsigned int detect_panel_state(u8 pwr_mode)
{
	unsigned int panel_state = DSI_DISP_INVALID_STATE;

	switch (pwr_mode & 0x14) {
	case 0x14:
		panel_state = DSI_DISP_ON_SLEEP_OUT;
		break;
	case 0x10:
		panel_state = DSI_DISP_OFF_SLEEP_OUT;
		break;
	case 0x00:
		panel_state = DSI_DISP_OFF_SLEEP_IN;
		break;
	}

	return panel_state;
}

static int mdss_dsi_quickdraw_check_panel_state(struct mdss_panel_data *pdata,
					u8 *pwr_mode, char **dropbox_issue)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mipi_panel_info *mipi;
	struct msm_fb_data_type *mfd;
	int panel_dead = 0;
	unsigned int panel_state = 0;
	int ret = 0;

	*pwr_mode = 0xFF;

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	mfd = pdata->mfd;
	if (mfd->quickdraw_panel_state == DSI_DISP_INVALID_STATE) {
		pr_warn("%s: quickdraw requests full reinitialization\n",
			__func__);
		panel_dead = 1;
		*dropbox_issue = ESD_SENSORHUB_DROPBOX_MSG;
	} else {
		mdss_dsi_get_pwr_mode(pdata, pwr_mode);

		if (*pwr_mode == 0xFF) {
			int gpio_val = gpio_get_value(ctrl->mipi_d0_sel);
			pr_warn("%s: unable to read power state! [gpio: %d]\n",
				__func__, gpio_val);
			panel_dead = 1;
			*dropbox_issue = PWR_MODE_FAIL_DROPBOX_MSG;
		} else {
			panel_state = detect_panel_state(*pwr_mode);
			if (panel_state == DSI_DISP_INVALID_STATE) {
				pr_warn("%s: detected invalid panel state\n",
					__func__);
				panel_dead = 1;
				*dropbox_issue = PWR_MODE_INVALID_DROPBOX_MSG;
			}
		}

		if (!panel_dead) {
			if (mfd->quickdraw_panel_state != panel_state) {
				pr_warn("%s: panel state is %d while %d expected\n",
					__func__, panel_state,
					mfd->quickdraw_panel_state);
				panel_dead = 1;
				*dropbox_issue = PWR_MODE_MISMATCH_DROPBOX_MSG;
			} else if (mfd->quickdraw_panel_state ==
				DSI_DISP_OFF_SLEEP_IN)
				ret = 1;
		}
	}

	if (panel_dead) {
		pr_err("%s: triggering ESD recovery\n", __func__);
		mfd->quickdraw_reset_panel = true;
	}

	return ret;
}

static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct msm_fb_data_type *mfd;
	u8 pwr_mode = 0;
	char *dropbox_issue = NULL;
	static int dropbox_count;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	mipi  = &pdata->panel_info.mipi;

	mfd = pdata->mfd;
	pr_info("%s+: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	if (!mfd->quickdraw_in_progress)
		mmi_panel_notify(MMI_PANEL_EVENT_PRE_DISPLAY_ON, NULL);

	if (ctrl->partial_mode_enabled
		&& !pdata->panel_info.panel_dead) {
		/* If we're doing partial display, we need to turn on the
		   regulators once since we don't enable and disable during
		   panel on/offs */
		if (!ctrl->panel_data.panel_info.cont_splash_feature_on) {
			static int once = 1;
			if (once) {
				mdss_dsi_panel_regulator_on(pdata, 1);
				mdss_dsi_panel_reset(pdata, 1);
				once = 0;
			}
		}

		gpio_set_value(ctrl->mipi_d0_sel, 0);
	} else {
		if (ctrl->partial_mode_enabled && pdata->panel_info.panel_dead)
			pr_warn("%s: Panel is dead, turn on panel regulators\n",
				__func__);

		mdss_dsi_panel_regulator_on(pdata, 1);
		mdss_dsi_panel_reset(pdata, 1);
	}

	if (ctrl->panel_config.bare_board == true) {
		if (!mfd->quickdraw_in_progress)
			mmi_panel_notify(MMI_PANEL_EVENT_DISPLAY_ON, NULL);
		pr_warn("%s: This is bare_board configuration\n", __func__);
		goto end;
	}

	if (mfd->quickdraw_in_progress) {
		int do_init = mdss_dsi_quickdraw_check_panel_state(pdata,
			&pwr_mode, &dropbox_issue);
		if (!do_init || mfd->quickdraw_reset_panel) {
			pr_info("%s: skip panel init cmds\n", __func__);
			goto end;
		}
	}

	if (ctrl->on_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->on_cmds);

	/* Send display on notification.  This will need to be revisited once
	   we implement command mode support the way we want, since display
	   may not be made visible to user until a point later than this */
	if (!mfd->quickdraw_in_progress)
		mmi_panel_notify(MMI_PANEL_EVENT_DISPLAY_ON, NULL);

	mdss_dsi_get_pwr_mode(pdata, &pwr_mode);
	/* validate screen is actually on */
	if ((pwr_mode & 0x04) != 0x04) {
		pr_err("%s: Display failure: DISON (0x04) bit not set\n",
			__func__);
		dropbox_issue = PWR_MODE_BLACK_DROPBOX_MSG;

		if (pdata->panel_info.panel_dead)
			pr_err("%s: Panel recovery FAILED!!\n", __func__);

		pdata->panel_info.panel_dead = true;
	}

#ifndef CONFIG_FB_MSM_MDSS_MDP3
	if (pdata->panel_info.hs_cmds_post_init)
		mdss_set_tx_power_mode(DSI_MODE_BIT_HS, pdata);
#endif
	/* Default CABC mode is UI while turning on display */
	if (pdata->panel_info.dynamic_cabc_enabled)
		pdata->panel_info.cabc_mode = CABC_UI_MODE;

end:
	if (dropbox_issue != NULL) {
		char dropbox_entry[256];

		snprintf(dropbox_entry, sizeof(dropbox_entry), "%s\ncount: %d",
			dropbox_issue, ++dropbox_count);

		pr_err("%s: display issue detected[count: %d][%s], reporting dropbox\n",
			__func__, dropbox_count, dropbox_issue);
		dropbox_queue_event_text(DROPBOX_DISPLAY_ISSUE,
			dropbox_entry, strlen(dropbox_entry));
	} else
		dropbox_count = 0;

	pr_info("%s-. Pwr_mode(0x0A) = 0x%x\n", __func__, pwr_mode);

	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mipi_panel_info *mipi;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct msm_fb_data_type *mfd;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	mfd = pdata->mfd;
	pr_info("%s+: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	mipi  = &pdata->panel_info.mipi;

	if (!mfd->quickdraw_in_progress)
		mmi_panel_notify(MMI_PANEL_EVENT_PRE_DISPLAY_OFF, NULL);

	if (ctrl->panel_config.bare_board == true)
		goto disable_regs;

	if (ctrl->set_hbm)
		ctrl->set_hbm(ctrl, 0);

	if (mfd->quickdraw_in_progress)
		pr_debug("%s: in quickdraw, SH wants the panel SLEEP OUT\n",
			__func__);
	else if (ctrl->off_cmds.cmd_cnt)
		mdss_dsi_panel_cmds_send(ctrl, &ctrl->off_cmds);

disable_regs:
	if (ctrl->partial_mode_enabled && !pdata->panel_info.panel_dead)
		gpio_set_value(ctrl->mipi_d0_sel, 1);
	else {
		if (ctrl->partial_mode_enabled && pdata->panel_info.panel_dead)
			pr_warn("%s: Panel is dead, turn off panel regulators\n",
				__func__);

		mdss_dsi_panel_reset(pdata, 0);
		mdss_dsi_panel_regulator_on(pdata, 0);
	}

	if (!mfd->quickdraw_in_progress)
		mmi_panel_notify(MMI_PANEL_EVENT_DISPLAY_OFF, NULL);

	if (pdata->panel_info.dynamic_cabc_enabled)
		pdata->panel_info.cabc_mode = CABC_OFF_MODE;

	pr_info("%s-:\n", __func__);

	return 0;
}

static void mdss_dsi_parse_lane_swap(struct device_node *np, char *dlane_swap)
{
	const char *data;

	*dlane_swap = DSI_LANE_MAP_0123;
	data = of_get_property(np, "qcom,mdss-dsi-lane-map", NULL);
	if (data) {
		if (!strcmp(data, "lane_map_3012"))
			*dlane_swap = DSI_LANE_MAP_3012;
		else if (!strcmp(data, "lane_map_2301"))
			*dlane_swap = DSI_LANE_MAP_2301;
		else if (!strcmp(data, "lane_map_1230"))
			*dlane_swap = DSI_LANE_MAP_1230;
		else if (!strcmp(data, "lane_map_0321"))
			*dlane_swap = DSI_LANE_MAP_0321;
		else if (!strcmp(data, "lane_map_1032"))
			*dlane_swap = DSI_LANE_MAP_1032;
		else if (!strcmp(data, "lane_map_2103"))
			*dlane_swap = DSI_LANE_MAP_2103;
		else if (!strcmp(data, "lane_map_3210"))
			*dlane_swap = DSI_LANE_MAP_3210;
	}
}

static void mdss_dsi_parse_trigger(struct device_node *np, char *trigger,
		char *trigger_key)
{
	const char *data;

	*trigger = DSI_CMD_TRIGGER_SW;
	data = of_get_property(np, trigger_key, NULL);
	if (data) {
		if (!strcmp(data, "none"))
			*trigger = DSI_CMD_TRIGGER_NONE;
		else if (!strcmp(data, "trigger_te"))
			*trigger = DSI_CMD_TRIGGER_TE;
		else if (!strcmp(data, "trigger_sw_seof"))
			*trigger = DSI_CMD_TRIGGER_SW_SEOF;
		else if (!strcmp(data, "trigger_sw_te"))
			*trigger = DSI_CMD_TRIGGER_SW_TE;
	}
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
			goto exit_free;
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
		goto exit_free;
	}

	pcmds->cmds = kzalloc(cnt * sizeof(struct dsi_cmd_desc),
						GFP_KERNEL);
	if (!pcmds->cmds)
		goto exit_free;

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

	if (link_key) {
		data = of_get_property(np, link_key, NULL);
		if (data && !strcmp(data, "dsi_hs_mode"))
			pcmds->link_state = DSI_HS_MODE;
		else
			pcmds->link_state = DSI_LP_MODE;
	} else
		pcmds->link_state = DSI_HS_MODE;

	pr_debug("%s: dcs_cmd=%x len=%d, cmd_cnt=%d link_state=%d\n", __func__,
		pcmds->buf[0], pcmds->blen, pcmds->cmd_cnt, pcmds->link_state);

	return 0;

exit_free:
	kfree(buf);
	return -ENOMEM;
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

int mdss_panel_parse_panel_config_dt(struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	struct device_node *np;
	const char *pname;
	u32 panel_ver;
	u32 detect_status;

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

	if (of_property_read_u32(np, "mmi,display_auto_detect", &detect_status))
		detect_status = 0xff;

	pr_debug("%s: esd_disable_bl=%d bare_board_bl=%d, " \
		" factory_cable=%d panel_name=%s bare_board=%d\n",
		__func__, ctrl_pdata->panel_config.esd_disable_bl,
		of_property_read_bool(np, "mmi,bare_board"),
		of_property_read_bool(np, "mmi,factory-cable"),
		ctrl_pdata->panel_config.panel_name,
		ctrl_pdata->panel_config.bare_board);

	panel_ver = (u32)ctrl_pdata->panel_config.panel_ver;
	pr_info("BL: panel=%s, manufacture_id(0xDA)= 0x%x controller_ver(0xDB)= 0x%x controller_drv_ver(0XDC)= 0x%x, full=0x%016llx, detect status = 0x%x\n",
		ctrl_pdata->panel_config.panel_name,
		panel_ver & 0xff, (panel_ver & 0xff00) >> 8,
		(panel_ver & 0xff0000) >> 16,
		ctrl_pdata->panel_config.panel_ver,
		detect_status);

	panelinfo.panel_name = (char *) &ctrl_pdata->panel_config.panel_name;
	panelinfo.panel_ver = &ctrl_pdata->panel_config.panel_ver;

	of_node_put(np);

	return 0;
}

static int mdss_dsi_parse_panel_features(struct device_node *np,
	struct mdss_dsi_ctrl_pdata *ctrl)
{
	struct mdss_panel_info *pinfo;

	if (!np || !ctrl) {
		pr_err("%s: Invalid arguments\n", __func__);
		return -ENODEV;
	}

	pinfo = &ctrl->panel_data.panel_info;

	pinfo->cont_splash_enabled = of_property_read_bool(np,
		"qcom,cont-splash-enabled");

	pinfo->partial_update_enabled = of_property_read_bool(np,
		"qcom,partial-update-enabled");
	pr_info("%s:%d Partial update %s\n", __func__, __LINE__,
		(pinfo->partial_update_enabled ? "enabled" : "disabled"));
	if (pinfo->partial_update_enabled)
		ctrl->partial_update_fnc = mdss_dsi_panel_partial_update;

	pinfo->ulps_feature_enabled = of_property_read_bool(np,
		"qcom,ulps-enabled");
	pr_info("%s: ulps feature %s", __func__,
		(pinfo->ulps_feature_enabled ? "enabled" : "disabled"));

	return 0;
}

static int mdss_dsi_panel_reg_read(struct mdss_panel_data *pdata,
		u8 reg, size_t size, u8 *buffer, uint8_t use_hs_mode)
{
	int ret;
	struct dcs_cmd_req cmdreq;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
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

	if (size > MDSS_DSI_LEN) {
		pr_warn("%s: size %d, max rx length is %d.\n", __func__,
				size, MDSS_DSI_LEN);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	pr_debug("%s: Reading %d bytes from 0x%02x\n", __func__, size, reg);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &reg_read_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT |
				(use_hs_mode ? 0 : CMD_REQ_LP_MODE);
	cmdreq.rlen = size;
	cmdreq.cb = NULL; /* call back */
	cmdreq.rbuf = kmalloc(MDSS_DSI_LEN, GFP_KERNEL);
	if (!cmdreq.rbuf) {
		ret = -ENOMEM;
		goto err1;
	}
	ret = mdss_dsi_cmdlist_put(ctrl, &cmdreq);

	if (ret <= 0) {
		pr_err("%s: Error reading %d bytes from reg 0x%02x. ret=0x%x\n",
			__func__, size, (unsigned int) reg, ret);
		ret = -EFAULT;
	} else {
		memcpy(buffer, cmdreq.rbuf, size);
		ret = 0;
	}
	kfree(cmdreq.rbuf);

err1:
	return ret;
}

static int mdss_dsi_panel_reg_write(struct mdss_panel_data *pdata,
		size_t size, u8 *buffer, uint8_t use_hs_mode)
{
	int ret = 0;
	struct dcs_cmd_req cmdreq;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
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

	/* Limit size to 32 to match with disp_util size checking */
	if (size > 32) {
		pr_err("%s: size is larger than 32 bytes. size=%d\n",
						__func__, size);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);
	pr_debug("%s: Writing %d bytes to 0x%02x\n", __func__, size, buffer[0]);
	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &reg_write_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_COMMIT | (use_hs_mode ? 0 : CMD_REQ_LP_MODE);
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	ret = mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	if (ret != 0) {
		pr_err("%s: Failed writing %d bytes to 0x%02x. Ret=0x%x\n",
			__func__, size, buffer[0], ret);
		ret = -EFAULT;
	}

	return ret;
}

static int mdss_dsi_parse_optional_dcs_cmds(struct device_node *np,
		struct dsi_panel_cmds *pcmds, char *cmd_key, char *link_key)
{
	int rc;

	if (!of_get_property(np, cmd_key, NULL))
		return 0;

	rc = mdss_dsi_parse_dcs_cmds(np, pcmds, cmd_key, link_key);
	if (rc)
		pr_err("%s : Failed parsing %s commands, rc = %d\n",
			__func__, cmd_key, rc);
	return rc;
}

static int mdss_panel_parse_optional_prop(struct device_node *np,
				struct mdss_panel_info *pinfo,
				struct mdss_dsi_ctrl_pdata *ctrl)
{
	int rc = 0;
	u32 tmp;

	/* HBM properties */
	pinfo->hbm_state = 0;
	pinfo->hbm_feature_enabled = false;
	rc = mdss_dsi_parse_optional_dcs_cmds(np, &ctrl->hbm_on_cmds,
				"qcom,mdss-dsi-hbm-on-command", NULL);
	rc |= mdss_dsi_parse_optional_dcs_cmds(np, &ctrl->hbm_off_cmds,
				"qcom,mdss-dsi-hbm-off-command", NULL);
	if (!of_property_read_u32(np, "qcom,mdss-dsi-hbm-on-brightness", &tmp))
		ctrl->hbm_on_brts = tmp;
	if (!of_property_read_u32(np, "qcom,mdss-dsi-hbm-off-brightness", &tmp))
		ctrl->hbm_off_brts = tmp;
	if ((ctrl->hbm_on_cmds.cmd_cnt && ctrl->hbm_off_cmds.cmd_cnt) ||
		(ctrl->hbm_on_brts && ctrl->hbm_off_brts)) {
		pinfo->hbm_feature_enabled = true;
		pr_info("%s: High Brightness Mode enabled.\n", __func__);
	}

	/* Dynamic CABC properties */
	pinfo->dynamic_cabc_enabled = false;
	rc |= mdss_dsi_parse_optional_dcs_cmds(np, &ctrl->cabc_ui_cmds,
				"qcom,mdss-dsi-cabc-ui-command", NULL);
	rc |= mdss_dsi_parse_optional_dcs_cmds(np, &ctrl->cabc_mv_cmds,
				"qcom,mdss-dsi-cabc-mv-command", NULL);
	if (ctrl->cabc_ui_cmds.cmd_cnt && ctrl->cabc_mv_cmds.cmd_cnt) {
		pinfo->dynamic_cabc_enabled = true;
		pinfo->cabc_mode = CABC_UI_MODE;
		pr_info("%s: Dynamic CABC enabled.\n", __func__);
	}

	return rc;
}

int mdss_dsi_panel_set_hbm(struct mdss_dsi_ctrl_pdata *ctrl, int state)
{
	int rc = 0;
	if (!ctrl->panel_data.panel_info.hbm_feature_enabled) {
		pr_debug("HBM is disabled, ignore request\n");
		return 0;
	}

	if (ctrl->panel_data.panel_info.hbm_state == state) {
		pr_debug("HBM already in request state %d\n",
			state);
		return 0;
	}

	if (state) {
		if (ctrl->hbm_on_cmds.cmd_cnt)
			rc = mdss_dsi_panel_cmds_send(ctrl,
						&ctrl->hbm_on_cmds);
		if (ctrl->hbm_on_brts)
			mdss_dsi_panel_bl_ctrl(&ctrl->panel_data,
						ctrl->hbm_on_brts);
	} else {
		if (ctrl->hbm_off_cmds.cmd_cnt)
			rc = mdss_dsi_panel_cmds_send(ctrl,
						&ctrl->hbm_off_cmds);
		if (ctrl->hbm_off_brts)
			mdss_dsi_panel_bl_ctrl(&ctrl->panel_data,
						ctrl->hbm_off_brts);
	}

	if (!rc)
		ctrl->panel_data.panel_info.hbm_state = state;
	else
		pr_err("%s : Failed to set HBM state to %d\n",
			__func__, state);

	return rc;
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
	tmp = 0;
	data = of_get_property(np, "qcom,mdss-dsi-pixel-packing", NULL);
	if (data && !strcmp(data, "loose"))
		tmp = 1;
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

	if (pdest) {
		if (strlen(pdest) != 9) {
			pr_err("%s: Unknown pdest specified\n", __func__);
			return -EINVAL;
		}
		if (!strcmp(pdest, "display_1"))
			pinfo->pdest = DISPLAY_1;
		else if (!strcmp(pdest, "display_2"))
			pinfo->pdest = DISPLAY_2;
		else {
			pr_debug("%s: pdest not specified. Set Default\n",
								__func__);
			pinfo->pdest = DISPLAY_1;
		}
	} else {
		pr_err("%s: pdest not specified\n", __func__);
		return -EINVAL;
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
	pinfo->bklt_ctrl = ctrl_pdata->bklt_ctrl;
	rc = of_property_read_u32(np, "qcom,mdss-brightness-max-level", &tmp);
	pinfo->brightness_max = (!rc ? tmp : MDSS_MAX_BL_BRIGHTNESS);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-min-level", &tmp);
	pinfo->bl_min = (!rc ? tmp : 0);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-max-level", &tmp);
	pinfo->bl_max = (!rc ? tmp : 255);
	ctrl_pdata->bklt_max = pinfo->bl_max;

	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-shutdown-delay", &tmp);
	pinfo->bl_shutdown_delay = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-bl-on-defer-delay", &tmp);
	pinfo->bl_on_defer_delay = (!rc ? tmp : 0);
	if (pinfo->bl_on_defer_delay) {
		hrtimer_init(&pinfo->bl_on_defer_hrtimer,
				CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		pinfo->bl_on_defer_hrtimer.function =
			&mdss_dsi_panel_bl_on_defer_timer_expire;
	}

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
	pinfo->mipi.traffic_mode = DSI_NON_BURST_SYNCH_PULSE;
	data = of_get_property(np, "qcom,mdss-dsi-traffic-mode", NULL);
	if (data) {
		if (!strcmp(data, "non_burst_sync_event"))
			pinfo->mipi.traffic_mode = DSI_NON_BURST_SYNCH_EVENT;
		else if (!strcmp(data, "burst_mode"))
			pinfo->mipi.traffic_mode = DSI_BURST_MODE;
	}
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-dcs-command", &tmp);
	pinfo->mipi.insert_dcs_cmd =
			(!rc ? tmp : 1);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-wr-mem-continue", &tmp);
	pinfo->mipi.wr_mem_continue =
			(!rc ? tmp : 0x3c);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-wr-mem-start", &tmp);
	pinfo->mipi.wr_mem_start =
			(!rc ? tmp : 0x2c);
	rc = of_property_read_u32(np,
		"qcom,mdss-dsi-te-pin-select", &tmp);
	pinfo->mipi.te_sel =
			(!rc ? tmp : 1);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-virtual-channel-id", &tmp);
	pinfo->mipi.vc = (!rc ? tmp : 0);
	pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	data = of_get_property(np, "qcom,mdss-dsi-color-order", NULL);
	if (data) {
		if (!strcmp(data, "rgb_swap_rbg"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_RBG;
		else if (!strcmp(data, "rgb_swap_bgr"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_BGR;
		else if (!strcmp(data, "rgb_swap_brg"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_BRG;
		else if (!strcmp(data, "rgb_swap_grb"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_GRB;
		else if (!strcmp(data, "rgb_swap_gbr"))
			pinfo->mipi.rgb_swap = DSI_RGB_SWAP_GBR;
	}
	pinfo->mipi.data_lane0 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-0-state");
	pinfo->mipi.data_lane1 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-1-state");
	pinfo->mipi.data_lane2 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-2-state");
	pinfo->mipi.data_lane3 = of_property_read_bool(np,
		"qcom,mdss-dsi-lane-3-state");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-pre", &tmp);
	pinfo->mipi.t_clk_pre = (!rc ? tmp : 0x24);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-t-clk-post", &tmp);
	pinfo->mipi.t_clk_post = (!rc ? tmp : 0x03);

	pinfo->mipi.rx_eot_ignore = of_property_read_bool(np,
		"qcom,mdss-dsi-rx-eot-ignore");
	pinfo->mipi.tx_eot_append = of_property_read_bool(np,
		"qcom,mdss-dsi-tx-eot-append");

	rc = of_property_read_u32(np, "qcom,mdss-dsi-stream", &tmp);
	pinfo->mipi.stream = (!rc ? tmp : 0);

	data = of_get_property(np, "qcom,mdss-dsi-panel-mode-gpio-state", NULL);
	if (data) {
		if (!strcmp(data, "high"))
			pinfo->mode_gpio_state = MODE_GPIO_HIGH;
		else if (!strcmp(data, "low"))
			pinfo->mode_gpio_state = MODE_GPIO_LOW;
	} else {
		pinfo->mode_gpio_state = MODE_GPIO_NOT_VALID;
	}

	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-framerate", &tmp);
	pinfo->mipi.frame_rate = (!rc ? tmp : 60);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-col-align", &tmp);
	pinfo->col_align = (!rc ? tmp : 1);
	rc = of_property_read_u32(np, "qcom,mdss-dsi-panel-clockrate", &tmp);
	pinfo->clk_rate = (!rc ? tmp : 0);
	data = of_get_property(np, "qcom,mdss-dsi-panel-timings", &len);
	if ((!data) || (len != 12)) {
		pr_err("%s:%d, Unable to read Phy timing settings",
		       __func__, __LINE__);
		goto error;
	}
	for (i = 0; i < len; i++)
		pinfo->mipi.dsi_phy_db.timing[i] = data[i];

	pinfo->mipi.lp11_init = of_property_read_bool(np,
					"qcom,mdss-dsi-lp11-init");
	rc = of_property_read_u32(np, "qcom,mdss-dsi-init-delay-us", &tmp);
	pinfo->mipi.init_delay = (!rc ? tmp : 0);

	mdss_dsi_parse_fbc_params(np, pinfo);

	mdss_dsi_parse_trigger(np, &(pinfo->mipi.mdp_trigger),
		"qcom,mdss-dsi-mdp-trigger");

	mdss_dsi_parse_trigger(np, &(pinfo->mipi.dma_trigger),
		"qcom,mdss-dsi-dma-trigger");

	mdss_dsi_parse_lane_swap(np, &(pinfo->mipi.dlane_swap));

	mdss_dsi_parse_reset_seq(np, pinfo->rst_seq, &(pinfo->rst_seq_len),
		"qcom,mdss-dsi-reset-sequence");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->on_cmds,
		"qcom,mdss-dsi-on-command", "qcom,mdss-dsi-on-command-state");

	mdss_dsi_parse_dcs_cmds(np, &ctrl_pdata->off_cmds,
		"qcom,mdss-dsi-off-command", "qcom,mdss-dsi-off-command-state");

	rc = mdss_dsi_parse_panel_features(np, ctrl_pdata);
	if (rc) {
		pr_err("%s: failed to parse panel features\n", __func__);
		goto error;
	}

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

	pinfo->hs_cmds_post_init = of_property_read_bool(np,
		"qcom,mdss-dsi-hs-cmds-post-init");

	pinfo->quickdraw_enabled = of_property_read_bool(np,
						"mmi,quickdraw-enabled");
	if (pinfo->quickdraw_enabled) {
		pr_info("%s:%d Quickdraw enabled.\n", __func__, __LINE__);
	} else {
		pr_info("%s:%d Quickdraw disabled.\n", __func__, __LINE__);
	}

	pinfo->no_solid_fill =
		of_property_read_bool(np, "qcom,splash-no-solid-fill");
	if (pinfo->mipi.mode == DSI_VIDEO_MODE && !pinfo->no_solid_fill)
		pr_warn("%s: 'No Solid Fill' not set for video mode panel",
			__func__);

	if (mdss_panel_parse_optional_prop(np, pinfo, ctrl_pdata)) {
		pr_err("Error parsing optional properties\n");
		goto error;
	}

	data = of_get_property(np, "qcom,mdss-dsi-panel-supplier", NULL);
	if (!data)
		memset(pinfo->supplier, '\0', sizeof(pinfo->supplier));
	else if (strlcpy(pinfo->supplier, data, sizeof(pinfo->supplier)) >=
		 sizeof(pinfo->supplier)) {
		pr_err("%s: Panel supplier name too large\n", __func__);
	}
	panelinfo.panel_supplier = pinfo->supplier;

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

int mdss_dsi_panel_set_cabc(struct mdss_dsi_ctrl_pdata *ctrl, int mode)
{
	int rc = -EINVAL;
	const char *name;

	if (!ctrl) {
		pr_warn("%s: Invalid ctrl pointer.\n", __func__);
		goto end;
	}

	name = mdss_panel_map_cabc_name(mode);
	if (!name) {
		pr_err("%s: Invalid mode: %d\n", __func__, mode);
		goto end;
	}

	if (ctrl->panel_data.panel_info.cabc_mode == mode) {
		pr_warn("%s: Already in requested mode: %s\n", __func__, name);
		rc = 0;
		goto end;
	}

	pr_debug("%s: Start to set %s mode\n", __func__, name);
	if (mode == CABC_MV_MODE && ctrl->cabc_mv_cmds.cmd_cnt)
		rc = mdss_dsi_panel_cmds_send(ctrl, &ctrl->cabc_mv_cmds);
	else if (mode == CABC_UI_MODE && ctrl->cabc_ui_cmds.cmd_cnt)
		rc = mdss_dsi_panel_cmds_send(ctrl, &ctrl->cabc_ui_cmds);

	if (rc < 0)
		pr_err("%s: Failed to set %s mode\n", __func__, name);
	else {
		pr_info("%s: Done setting %s mode\n", __func__, name);
		ctrl->panel_data.panel_info.cabc_mode = mode;
	}
end:
	return rc;
}

int mdss_dsi_panel_ioctl_handler(struct mdss_panel_data *pdata,
					u32 cmd, void *arg)
{
	int rc = -EINVAL;
	struct msmfb_reg_access reg_access;
	u8 *reg_access_buf;

	if (copy_from_user(&reg_access, arg, sizeof(reg_access)))
		return -EFAULT;

	reg_access_buf = kmalloc(reg_access.buffer_size + 1, GFP_KERNEL);
	if (reg_access_buf == NULL)
		return -ENOMEM;

	switch (cmd) {
	case MSMFB_REG_WRITE:
		reg_access_buf[0] = reg_access.address;
		if (copy_from_user(&reg_access_buf[1], reg_access.buffer,
						reg_access.buffer_size))
			rc = -EFAULT;
		else
			rc = mdss_dsi_panel_reg_write(pdata,
					reg_access.buffer_size + 1,
					reg_access_buf, reg_access.use_hs_mode);
		break;
	case MSMFB_REG_READ:
		rc = mdss_dsi_panel_reg_read(pdata, reg_access.address,
					reg_access.buffer_size,
					reg_access_buf, reg_access.use_hs_mode);
		if ((rc == 0) && (copy_to_user(reg_access.buffer,
						reg_access_buf,
						reg_access.buffer_size)))
			rc = -EFAULT;
		break;
	default:
		pr_err("%s: unsupport ioctl =0x%x\n", __func__, cmd);
		rc = -EFAULT;
		break;
	}

	kfree(reg_access_buf);

	return rc;
}

int mdss_dsi_panel_init(struct device_node *node,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool cmd_cfg_cont_splash)
{
	int rc = 0;
	static const char *panel_name;
	struct mdss_panel_info *pinfo;

	if (!node || !ctrl_pdata) {
		pr_err("%s: Invalid arguments\n", __func__);
		return -ENODEV;
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;

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

	if (!cmd_cfg_cont_splash)
		pinfo->cont_splash_enabled = false;
	pr_info("%s: Continuous splash %s", __func__,
		pinfo->cont_splash_enabled ? "enabled" : "disabled");

	/* If it is bare board, disable splash feature. */
	if (ctrl_pdata->panel_config.bare_board)
		pinfo->cont_splash_enabled = 0;

	/* Since the cont_splash_enabled flag gets cleared when continuous
	   splash is done, copy it here for later use so we can know if we
	   ever did continuous splash */
	pinfo->cont_splash_feature_on = pinfo->cont_splash_enabled;
	pinfo->cont_splash_esd_rdy = false;
	ctrl_pdata->partial_mode_enabled = of_property_read_bool(node,
						"mmi,partial-mode-enabled");
	pr_debug("%s: MMI partial mode %s", __func__,
		ctrl_pdata->partial_mode_enabled ? "enabled" : "disabled");

	ctrl_pdata->on = mdss_dsi_panel_on;
	ctrl_pdata->off = mdss_dsi_panel_off;
	ctrl_pdata->panel_data.set_backlight = mdss_dsi_panel_bl_ctrl;
	ctrl_pdata->cont_splash_on = mdss_dsi_panel_cont_splash_on;
	ctrl_pdata->check_status_disabled =
				!ctrl_pdata->panel_config.esd_enable;
	ctrl_pdata->check_status = mdss_panel_check_status;
	ctrl_pdata->set_hbm = mdss_dsi_panel_set_hbm;
	ctrl_pdata->set_cabc = mdss_dsi_panel_set_cabc;
	ctrl_pdata->bl_on_defer = mdss_dsi_panel_bl_on_defer_start;


	return 0;
}
