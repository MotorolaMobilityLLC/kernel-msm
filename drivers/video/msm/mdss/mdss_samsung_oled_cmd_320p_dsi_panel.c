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
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/qpnp/pin.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/err.h>
#ifdef CONFIG_LCD_CLASS_DEVICE
#include <linux/lcd.h>
#endif
#include "mdss_fb.h"
#include "mdss_dsi.h"
#include "mdss_samsung_dsi_panel_msm8x26.h"

#define DT_CMD_HDR 6

/* #define CMD_DEBUG */
#define ALPM_MODE

static struct dsi_cmd display_on_seq;
static struct dsi_cmd display_on_cmd;
static struct dsi_cmd display_off_seq;
static struct dsi_cmd manufacture_id_cmds;
static struct dsi_cmd mtp_id_cmds;
static struct dsi_cmd mtp_enable_cmds;
static struct dsi_cmd gamma_cmds_list;
static struct dsi_cmd backlight_cmds;
static struct dsi_cmd rddpm_cmds;

static struct candella_lux_map candela_map_table;
DEFINE_LED_TRIGGER(bl_led_trigger);
static struct mdss_samsung_driver_data msd;
#if defined(ALPM_MODE)
/* ALPM mode on/off command */
static struct dsi_cmd alpm_on_seq;
static struct dsi_cmd alpm_off_seq;
static int disp_esd_gpio;
/*
 * APIs for ALPM mode
 * alpm_store()
 *	- Check or store status like alpm mode status or brightness level
 */
static void alpm_store(u8 mode);
#endif
static int mdss_dsi_panel_dimming_init(struct mdss_panel_data *pdata);
static int mipi_samsung_disp_send_cmd(
				enum mipi_samsung_cmd_list cmd,
				unsigned char lock);
static struct panel_rev panel_supp_cdp[] = {
	{"SDC_AMS163AX01", PANEL_320_OCTA_S6E63J},
	{NULL}
};

static char brightness[28] = {0xD4, };
static struct dsi_cmd_desc brightness_cmd = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0, sizeof(brightness)},
	brightness
};

static int get_cmd_idx(int bl_level)
{
	return candela_map_table.cmd_idx[candela_map_table.bkl[bl_level]];
}
static int get_candela_value(int bl_level)
{
	return candela_map_table.lux_tab[candela_map_table.bkl[bl_level]];
}
void get_gamma_control_set(int candella)
{
	int cnt;
/*  Just a safety check to ensure smart dimming data is initialised well */
	BUG_ON(msd.sdimconf->generate_gamma == NULL);
	msd.sdimconf->generate_gamma(candella,
		&gamma_cmds_list.cmd_desc[0].payload[1]);
	for (cnt = 1; cnt < (GAMMA_SET_MAX + 1); cnt++)
		brightness[cnt] = gamma_cmds_list.cmd_desc[0].payload[cnt];
}

void mdss_dsi_cmds_send(struct mdss_dsi_ctrl_pdata *ctrl,
	struct dsi_cmd_desc *cmds, int cnt, int flag)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cnt;
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
}

static void mdss_dsi_panel_bklt_dcs(struct mdss_dsi_ctrl_pdata *ctrl,
					int bl_level)
{
	struct dcs_cmd_req cmdreq;
	int cd_idx = 0, cd_level = 0;
	static int stored_cd_level;

	/*gamma*/
	cd_idx = get_cmd_idx(bl_level);
	cd_level = get_candela_value(bl_level);

	/* gamma control */
	get_gamma_control_set(cd_level);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = &brightness_cmd;
	cmdreq.cmds_cnt = 1;

	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mdss_mdp_cmd_clk_enable();
	mdss_dsi_cmd_dma_trigger_sel(ctrl, 1);
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	mdss_dsi_cmd_dma_trigger_sel(ctrl, 0);
	stored_cd_level = cd_level;
	mdss_mdp_cmd_clk_disable();
}

void mdss_dsi_samsung_panel_reset(struct mdss_panel_data *pdata, int enable)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;
	int ambient_mode_check = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (msd.dstat.on)
		return;

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

	pr_info("%s:enable = %d\n", __func__, enable);
	pinfo = &(ctrl_pdata->panel_data.panel_info);

	if (pinfo->alpm_event) {
		if (enable && pinfo->alpm_event(CHECK_PREVIOUS_STATUS)) {
			if (gpio_get_value(disp_esd_gpio)) {
				pr_info("[ESD_DEBUG] check current alpm status\n");
				if (pinfo->alpm_event(CHECK_CURRENT_STATUS))
					ambient_mode_check = 1;
				pinfo->alpm_event(CLEAR_MODE_STATUS);
				if (ambient_mode_check)
					pinfo->alpm_event(ALPM_MODE_ON);
			} else
				return;
		} else if (!enable && pinfo->alpm_event(CHECK_CURRENT_STATUS)) {
			pinfo->alpm_event(STORE_CURRENT_STATUS);
			return;
		}
		pr_debug("[ALPM_DEBUG] %s: Panel reset, enable : %d\n",
					__func__, enable);
	}

	if (enable) {
		if (gpio_is_valid(ctrl_pdata->rst_gpio)) {
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			usleep_range(5000, 5000);
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
			usleep_range(5000, 5000);
			gpio_set_value((ctrl_pdata->rst_gpio), 1);
			usleep_range(5000, 5000);
		}

		if (gpio_is_valid(ctrl_pdata->mode_gpio)) {
			if (pinfo->mode_gpio_state == MODE_GPIO_HIGH)
				gpio_set_value((ctrl_pdata->mode_gpio), 1);
			else if (pinfo->mode_gpio_state == MODE_GPIO_LOW)
				gpio_set_value((ctrl_pdata->mode_gpio), 0);
		}

		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 1);
		if (ctrl_pdata->ctrl_state & CTRL_STATE_PANEL_INIT) {
			pr_debug("%s: Panel Not properly turned OFF\n",
						__func__);
			ctrl_pdata->ctrl_state &= ~CTRL_STATE_PANEL_INIT;
			pr_debug("%s: Reset panel done\n", __func__);
		}
	} else {
		if (gpio_is_valid(ctrl_pdata->rst_gpio))
			gpio_set_value((ctrl_pdata->rst_gpio), 0);
		if (gpio_is_valid(ctrl_pdata->disp_en_gpio))
			gpio_set_value((ctrl_pdata->disp_en_gpio), 0);
	}
	return;
}

static int mipi_samsung_disp_send_cmd(
		enum mipi_samsung_cmd_list cmd,
		unsigned char lock)
{
	struct dsi_cmd_desc *cmd_desc;
	int cmd_size = 0;
	int flag = 0;
#ifdef CMD_DEBUG
	int i, j;
#endif
	if (lock)
		mutex_lock(&msd.lock);

	switch (cmd) {
	case PANEL_DISPLAY_ON_SEQ:
		cmd_desc = display_on_seq.cmd_desc;
		cmd_size = display_on_seq.num_of_cmds;
		break;
	case PANEL_DISPLAY_ON:
		cmd_desc = display_on_cmd.cmd_desc;
		cmd_size = display_on_cmd.num_of_cmds;
		break;
	case PANEL_DISP_OFF:
		cmd_desc = display_off_seq.cmd_desc;
		cmd_size = display_off_seq.num_of_cmds;
		break;
	case PANEL_MTP_ENABLE:
		cmd_desc = mtp_enable_cmds.cmd_desc;
		cmd_size = mtp_enable_cmds.num_of_cmds;
		break;
	case PANEL_BACKLIGHT_CMD:
		cmd_desc = backlight_cmds.cmd_desc;
		cmd_size = backlight_cmds.num_of_cmds;
		break;
#if defined(ALPM_MODE)
	case PANEL_ALPM_ON:
		cmd_desc = alpm_on_seq.cmd_desc;
		cmd_size = alpm_on_seq.num_of_cmds;
		break;
	case PANEL_ALPM_OFF:
		cmd_desc = alpm_off_seq.cmd_desc;
		cmd_size = alpm_off_seq.num_of_cmds;
		break;
#endif
	default:
		pr_err("%s : unknown_command..\n", __func__);
		goto unknown_command;
		;
	}

	if (!cmd_size) {
		pr_err("%s : cmd_size is zero!..\n", __func__);
		goto unknown_command;
	}

#ifdef CMD_DEBUG
	for (i = 0; i < cmd_size; i++) {
		for (j = 0; j < cmd_desc[i].dchdr.dlen; j++)
			pr_info("%x ", cmd_desc[i].payload[j]);
		pr_info("\n");
	}
#endif

	if (cmd != PANEL_DISPLAY_ON_SEQ && cmd != PANEL_DISP_OFF)
		mdss_mdp_cmd_clk_enable();
	mdss_dsi_cmds_send(msd.ctrl_pdata, cmd_desc, cmd_size, flag);
	if (cmd != PANEL_DISPLAY_ON_SEQ && cmd != PANEL_DISP_OFF)
		mdss_mdp_cmd_clk_disable();

	if (lock)
		mutex_unlock(&msd.lock);
	return 0;

unknown_command:
	LCD_DEBUG("Undefined command\n");

	if (lock)
		mutex_unlock(&msd.lock);

	return -EINVAL;
}
static int mdss_dsi_panel_registered(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_info *pinfo = NULL;

	if (pdata == NULL) {
		pr_err("%s : Invalid input data\n", __func__);
		return -EINVAL;
	}

	pinfo = &pdata->panel_info;
	ctrl_pdata = container_of(pdata,
		struct mdss_dsi_ctrl_pdata, panel_data);
	msd.mfd = (struct msm_fb_data_type *)registered_fb[0]->par;
	msd.pdata = pdata;
	msd.ctrl_pdata = ctrl_pdata;

	if (pinfo->mipi.mode == DSI_CMD_MODE)
		msd.dstat.on = 1;

	pr_info("%s:%d, panel registered succesfully\n", __func__, __LINE__);
	return 0;
}

static void mdss_dsi_panel_alpm_ctrl(struct mdss_panel_data *pdata,
							bool mode)
{
	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (!pdata->panel_info.panel_power_on) {
		pr_err("%s: DSI block is off state\n", __func__);
		return;
	}

	if (pdata->panel_info.alpm_event(CHECK_PREVIOUS_STATUS)
		&& pdata->panel_info.alpm_event(CHECK_CURRENT_STATUS)) {
		pdata->panel_info.alpm_mode = mode;
		pr_info("%s: ambient -> ambient\n", __func__);
		return;
	}

	if (pdata->panel_info.alpm_mode == mode)
		return;

	if (mode)
		mipi_samsung_disp_send_cmd(PANEL_ALPM_ON, true);
	else
		/* Turn Off ALPM Mode */
		mipi_samsung_disp_send_cmd(PANEL_ALPM_OFF, true);

	pr_info("[ALPM_DEBUG]: Send ALPM %s cmds\n",
			mode ? "on" : "off");

	pdata->panel_info.alpm_event(STORE_CURRENT_STATUS);
	pdata->panel_info.alpm_mode = mode;
}

static void mdss_dsi_panel_bl_ctrl(struct mdss_panel_data *pdata,
							u32 bl_level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	static int stored_bl_level = 255;
	int request_bl_dim = 0;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return;
	}

	if (bl_level == PANEL_BACKLIGHT_RESTORE)
		bl_level = stored_bl_level;
	else if (bl_level == PANEL_BACKLIGHT_DIM) {
		request_bl_dim = 1;
		bl_level = 30;
	}

	if (!pdata->panel_info.panel_power_on) {
		pr_err("%s: DSI block is off state\n", __func__);
		return;
	}

	mdss_dsi_panel_dimming_init(pdata);
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);
	/*
	 * Some backlight controllers specify a minimum duty cycle
	 * for the backlight brightness. If the brightness is less
	 * than it, the controller can malfunction.
	 */

	if ((bl_level < pdata->panel_info.bl_min) && (bl_level != 0))
		bl_level = pdata->panel_info.bl_min;
	switch (ctrl_pdata->bklt_ctrl) {
	case BL_WLED:
		led_trigger_event(bl_led_trigger, bl_level);
		break;
	case BL_DCS_CMD:
		msd.dstat.bright_level = bl_level;
		mdss_dsi_panel_bklt_dcs(ctrl_pdata, bl_level);
		break;
	default:
		pr_err("%s: Unknown bl_ctrl configuration\n",
			__func__);
		break;
	}
	if (pdata->panel_info.first_bl_update) {
		mipi_samsung_disp_send_cmd(PANEL_BACKLIGHT_CMD, true);
		pdata->panel_info.first_bl_update = 0;
	}
	if (request_bl_dim)
		bl_level = stored_bl_level;
	stored_bl_level = bl_level;
}

void mdss_dsi_panel_bl_dim(struct mdss_panel_data *pdata, int flag)
{
	if (likely(msd.dstat.on)) {
			mdss_dsi_panel_bl_ctrl(pdata, flag);
	} else
		pr_info("[ALPM_DEBUG] %s: The LCD already turned off\n"
					, __func__);
}

u32 mdss_dsi_cmd_receive(struct mdss_dsi_ctrl_pdata *ctrl,
	struct dsi_cmd_desc *cmd, int rlen)
{
	struct dcs_cmd_req cmdreq;

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.cmds = cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rbuf = ctrl->rx_buf.data;
	cmdreq.rlen = rlen;
	cmdreq.cb = NULL; /* call back */
	/*
	* This mutex is to sync up with dynamic FPS changes
	* so that DSI lockups shall not happen
	*/
	mdss_mdp_cmd_clk_enable();
	mdss_dsi_cmdlist_put(ctrl, &cmdreq);
	mdss_mdp_cmd_clk_disable();

	/*
	* blocked here, untill call back called
	*/
	return ctrl->rx_buf.len;
}
static unsigned int mipi_samsung_manufacture_id(struct mdss_panel_data *pdata)
{
	unsigned int id = 0;

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&manufacture_id_cmds.cmd_desc[0],
		manufacture_id_cmds.read_size[0]);

	id = msd.ctrl_pdata->rx_buf.data[0] << 16;
	id |= msd.ctrl_pdata->rx_buf.data[1] << 8;
	id |= msd.ctrl_pdata->rx_buf.data[2];

	pr_info("%s: manufacture id = %x\n", __func__, id);
	return id;
}
static unsigned int mdss_sasmung_mtp_id(char *destbuffer)
{
	int cnt;
	mipi_samsung_disp_send_cmd(PANEL_MTP_ENABLE, true);

	mdss_dsi_cmd_receive(msd.ctrl_pdata,
		&mtp_id_cmds.cmd_desc[0],
		mtp_id_cmds.read_size[0]);
	pr_info("%s : MTP ID :\n", __func__);
	for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++) {
		pr_info("%d, ", msd.ctrl_pdata->rx_buf.data[cnt]);
		destbuffer[cnt] = msd.ctrl_pdata->rx_buf.data[cnt];
	}
	pr_info("\n");
	return 0;
}
static int mdss_dsi_panel_dimming_init(struct mdss_panel_data *pdata)
{
	/* If the ID is not read yet, then read it*/
	if (!msd.manufacture_id)
		msd.manufacture_id = mipi_samsung_manufacture_id(pdata);

	if (!msd.dstat.is_smart_dim_loaded) {
		pr_info(" %s ++\n", __func__);
		switch (msd.panel) {
		case PANEL_320_OCTA_S6E63J:
			pr_info("%s : S6E63J\n", __func__);
			msd.sdimconf = smart_S6E63J_get_conf();
			break;
		}
		/* Just a safety check to ensure
		smart dimming data is initialised well */
		BUG_ON(msd.sdimconf == NULL);

		/* Set the mtp read buffer pointer and read the NVM value*/
		mdss_sasmung_mtp_id(msd.sdimconf->mtp_buffer);

		/* lux_tab setting for 350cd */
		msd.sdimconf->lux_tab = &candela_map_table.lux_tab[0];
		msd.sdimconf->lux_tabsize = candela_map_table.lux_tab_size;
		msd.sdimconf->man_id = msd.manufacture_id;

		/* Just a safety check to ensure
		smart dimming data is initialised well */
		BUG_ON(msd.sdimconf->init == NULL);
		msd.sdimconf->init();
		msd.dstat.is_smart_dim_loaded = true;

		/*
		 * Since dimming is loaded,
		 * we can assume that device is out of suspend state
		 * and can accept backlight commands.
		 */
		msd.mfd->resume_state = MIPI_RESUME_STATE;
		pr_info(" %s --\n", __func__);
	}
	return 0;
}
static int mdss_dsi_panel_on(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	pinfo = &(ctrl->panel_data.panel_info);

	if (!msd.manufacture_id)
		msd.manufacture_id = mipi_samsung_manufacture_id(pdata);

	mdss_dsi_panel_dimming_init(pdata);

	/* Normaly the else is working for PANEL_DISP_ON_SEQ */
	if (pinfo->alpm_event) {
		if (!pinfo->alpm_event(CHECK_PREVIOUS_STATUS))
			mipi_samsung_disp_send_cmd(PANEL_DISPLAY_ON_SEQ, true);
	} else
		mipi_samsung_disp_send_cmd(PANEL_DISPLAY_ON_SEQ, true);

	msd.dstat.wait_disp_on = 1;
	msd.dstat.on = 1;
	msd.mfd->resume_state = MIPI_RESUME_STATE;

	/* ALPM Mode Change */
	if (pinfo->alpm_event) {
		if (!pinfo->alpm_event(CHECK_PREVIOUS_STATUS)
				&& pinfo->alpm_event(CHECK_CURRENT_STATUS)) {
			/* Turn On ALPM Mode */

			mdss_dsi_panel_bl_dim(pdata, PANEL_BACKLIGHT_DIM);
			mipi_samsung_disp_send_cmd(PANEL_ALPM_ON, true);
			pinfo->alpm_event(STORE_CURRENT_STATUS);
			pr_info("[ALPM_DEBUG] %s: Send ALPM mode on cmds\n",
						 __func__);
		} else if (!pinfo->alpm_event(CHECK_CURRENT_STATUS)
				&& pinfo->alpm_event(CHECK_PREVIOUS_STATUS)) {
			/* Turn Off ALPM Mode */
			mipi_samsung_disp_send_cmd(PANEL_ALPM_OFF, true);
			/*
			mdss_dsi_panel_bl_dim(msd.pdata,
					PANEL_BACKLIGHT_RESTORE);
			pinfo->alpm_event(CLEAR_MODE_STATUS);
			*/
			pr_info("[ALPM_DEBUG] %s: Send ALPM off cmds\n",
						 __func__);
		}
	}

	if (androidboot_mode_charger || androidboot_is_recovery)
		mipi_samsung_disp_send_cmd(PANEL_BACKLIGHT_CMD, true);
	pr_info("%s:-\n", __func__);
	return 0;
}

static int mdss_dsi_panel_off(struct mdss_panel_data *pdata)
{
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct mdss_panel_info *pinfo = NULL;

	if (pdata == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata, struct mdss_dsi_ctrl_pdata,
				panel_data);

	pr_info("%s: ctrl=%p ndx=%d\n", __func__, ctrl, ctrl->ndx);

	pinfo = &(ctrl->panel_data.panel_info);

	msd.dstat.on = 0;
	msd.mfd->resume_state = MIPI_SUSPEND_STATE;
	pdata->panel_info.first_bl_update = 1;
	if (pinfo->alpm_event &&
		pinfo->alpm_event(CHECK_CURRENT_STATUS) &&
		!pinfo->alpm_event(CHECK_PREVIOUS_STATUS)) {
		pr_info("[ALPM_DEBUG] %s: Skip to send panel off cmds\n",
				__func__);
		mdss_dsi_panel_bl_dim(pdata, PANEL_BACKLIGHT_DIM);
		mipi_samsung_disp_send_cmd(PANEL_ALPM_ON, true);
		pinfo->alpm_event(STORE_CURRENT_STATUS);
	} else if (pinfo->is_suspending)
		pr_debug("[ALPM_DEBUG] %s: Skip to send panel off cmds\n",
				__func__);
	else
		mipi_samsung_disp_send_cmd(PANEL_DISP_OFF, true);

	pr_info("%s:-\n", __func__);
	return 0;
}

static void alpm_enable(int enable)
{
	struct mdss_panel_info *pinfo = &msd.pdata->panel_info;
	struct display_status *dstat = &msd.dstat;

	pr_info("[ALPM_DEBUG] %s: enable: %d\n", __func__, enable);

	/*
	 * Possible mode status for Blank(0) or Unblank(1)
	 *	* Blank *
	 *		1) ALPM_MODE_ON
	 *		2) MODE_OFF
	 *
	 *		The mode(1, 2) will change when unblank
	 *	* Unblank *
	 *		1) ALPM_MODE_ON
	 *			-> The mode will change when blank
	 *		2) MODE_OFF
	 *			-> The mode will change immediately
	 */

	alpm_store(enable);
	if (enable == MODE_OFF) {
		if (pinfo->alpm_event(CHECK_PREVIOUS_STATUS)
					 == ALPM_MODE_ON) {
			if (dstat->on) {
				mipi_samsung_disp_send_cmd(
					PANEL_ALPM_OFF, true);
				/* wait 1 frame(more than 16ms) */
				msleep(20);
				pinfo->alpm_event(CLEAR_MODE_STATUS);
				pr_info("[ALPM_DEBUG] %s: Send ALPM off cmds\n",
							__func__);
			}
		}
	}
}

static int mdss_samsung_parse_panel_cmd(struct device_node *np,
	struct dsi_cmd *commands, char *keystring) {
	const char *data;
	int type, len = 0, i = 0;
	char *bp;
	struct dsi_ctrl_hdr *dchdr;
	int is_read = 0;
	data = of_get_property(np, keystring, &len);
	if (!data) {
		pr_info("%s:%d, Unable to read %s",
			__func__, __LINE__, keystring);
		return -ENOMEM;
	}
	commands->cmds_buff = kzalloc(sizeof(char) * len, GFP_KERNEL);
	if (!commands->cmds_buff)
		return -ENOMEM;
	memcpy(commands->cmds_buff, data, len);
	commands->cmds_len = len;
	/* scan dcs commands */
	bp = commands->cmds_buff;
	while (len > sizeof(*dchdr)) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		dchdr->dlen = ntohs(dchdr->dlen);
		if (dchdr->dlen > 200)
			goto error2;
		bp += sizeof(*dchdr);
		len -= sizeof(*dchdr);
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		commands->num_of_cmds++;
		type = dchdr->dtype;
		if (type == DTYPE_GEN_READ ||
			type == DTYPE_GEN_READ1 ||
			type == DTYPE_GEN_READ2 ||
			type == DTYPE_DCS_READ)	{
			/* Read command :
			last byte contain read size, read start */
			bp += 2;
			len -= 2;
			is_read = 1;
		}
	}
	if (len != 0) {
		pr_info("%s: dcs OFF command byte Error, len=%d",
						__func__, len);
		commands->cmds_len = 0;
		commands->num_of_cmds = 0;
		goto error2;
	}
	if (is_read) {
		/*
		Allocate an array which will store the number
		for bytes to read for each read command
		*/
		commands->read_size = kzalloc(sizeof(char) *
			commands->num_of_cmds, GFP_KERNEL);
		if (!commands->read_size) {
			pr_err("%s:%d, Unable to read NV cmds",
				__func__, __LINE__);
			goto error2;
		}
		commands->read_startoffset = kzalloc(sizeof(char) *
			commands->num_of_cmds, GFP_KERNEL);
		if (!commands->read_startoffset) {
			pr_err("%s:%d, Unable to read NV cmds",
				__func__, __LINE__);
			goto error1;
		}
	}
	commands->cmd_desc = kzalloc(commands->num_of_cmds
		* sizeof(struct dsi_cmd_desc),
		GFP_KERNEL);
	if (!commands->cmd_desc)
		goto error1;
	bp = commands->cmds_buff;
	len = commands->cmds_len;
	for (i = 0; i < commands->num_of_cmds; i++) {
		dchdr = (struct dsi_ctrl_hdr *)bp;
		len -= sizeof(*dchdr);
		bp += sizeof(*dchdr);
		commands->cmd_desc[i].dchdr = *dchdr;
		commands->cmd_desc[i].payload = bp;
		bp += dchdr->dlen;
		len -= dchdr->dlen;
		if (is_read) {
			commands->read_size[i] = *bp++;
			commands->read_startoffset[i] = *bp++;
			len -= 2;
		}
	}
	return 0;
error1:
	kfree(commands->read_size);
error2:
	kfree(commands->cmds_buff);
	return -EINVAL;
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

static int mdss_samsung_parse_candella_lux_mapping_table(struct device_node *np,
	struct candella_lux_map *table, char *keystring)
{
	const __be32 *data;
	int  data_offset, len = 0 , i = 0;
	int  cdmap_start = 0, cdmap_end = 0;
	data = of_get_property(np, keystring, &len);
	if (!data) {
		pr_err("%s:%d, Unable to read table %s ",
			__func__, __LINE__, keystring);
		return -EINVAL;
	}
	if ((len % 4) != 0) {
		pr_err("%s:%d, Incorrect table entries for %s",
			__func__, __LINE__, keystring);
		return -EINVAL;
	}
	table->lux_tab_size = len / (sizeof(int)*4);
	table->lux_tab = kzalloc((sizeof(int) * table->lux_tab_size),
						GFP_KERNEL);
	if (!table->lux_tab)
		return -ENOMEM;
	table->cmd_idx = kzalloc((sizeof(int) * table->lux_tab_size),
						GFP_KERNEL);
	if (!table->cmd_idx)
		goto error;
	data_offset = 0;
	for (i = 0; i < table->lux_tab_size; i++) {
		table->cmd_idx[i] = be32_to_cpup(&data[data_offset++]);
		/* 1rst field => <idx> */
		cdmap_start = be32_to_cpup(&data[data_offset++]);
		/* 2nd field => <from> */
		cdmap_end = be32_to_cpup(&data[data_offset++]);
		/* 3rd field => <till> */
		table->lux_tab[i] = be32_to_cpup(&data[data_offset++]);
		/* 4th field => <candella> */
		/* Fill the backlight level to lux mapping array */
		do {
			table->bkl[cdmap_start++] = i;
		} while (cdmap_start <= cdmap_end);
	}
	return 0;
error:
	kfree(table->lux_tab);
	return -ENOMEM;
}

static void mdss_panel_parse_te_params(struct device_node *np,
				       struct mdss_panel_info *panel_info)
{
	u32 tmp;
	int rc = 0;
	/*
	 * TE default: dsi byte clock calculated base on 70 fps;
	 * around 14 ms to complete a kickoff cycle if te disabled;
	 * vclk_line base on 60 fps; write is faster than read;
	 * init == start == rdptr;
	 */
	panel_info->te.tear_check_en =
		!of_property_read_bool(np, "qcom,mdss-tear-check-disable");
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-cfg-height", &tmp);
	panel_info->te.sync_cfg_height = (!rc ? tmp : 0xfff0);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-init-val", &tmp);
	panel_info->te.vsync_init_val = (!rc ? tmp : panel_info->yres);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-threshold-start", &tmp);
	panel_info->te.sync_threshold_start = (!rc ? tmp : 4);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-sync-threshold-continue", &tmp);
	panel_info->te.sync_threshold_continue = (!rc ? tmp : 4);
	rc = of_property_read_u32(np, "qcom,mdss-tear-check-start-pos", &tmp);
	panel_info->te.start_pos = (!rc ? tmp : panel_info->yres);
	rc = of_property_read_u32
		(np, "qcom,mdss-tear-check-rd-ptr-trigger-intr", &tmp);
	panel_info->te.rd_ptr_irq = (!rc ? tmp : panel_info->yres + 1);
	rc = of_property_read_u32(np, "qcom,mdss-tear-check-frame-rate", &tmp);
	panel_info->te.refx100 = (!rc ? tmp : 6000);
}


static int mdss_panel_parse_dt(struct device_node *np,
			struct mdss_dsi_ctrl_pdata *ctrl_pdata)
{
	u32	tmp;
	int rc, i, len;
	const char *data;
	static const char *pdest;
	struct mdss_panel_info *pinfo = &(ctrl_pdata->panel_data.panel_info);
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
	if (rc)
		pr_err("%s:%d, bpp not specified\n", __func__, __LINE__);
	pinfo->bpp = (!rc ? tmp : 24);
	rc = of_property_read_u32(np, "qcom,alpm-ldo-offset", &tmp);
	pinfo->alpm_ldo_offset = (!rc ? tmp : 0);
	pr_info("[ALPM_DEBUG] ldo-offset : %d\n", pinfo->alpm_ldo_offset);
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
	data = of_get_property(np, "qcom,mdss-dsi-bl-pmic-control-type", NULL);
	if (data) {
		if (!strncmp(data, "bl_ctrl_wled", 12)) {
			led_trigger_register_simple("bkl-trigger",
				&bl_led_trigger);
			pr_debug("%s: SUCCESS-> WLED TRIGGER register\n",
				__func__);
			ctrl_pdata->bklt_ctrl = BL_WLED;
		}  else if (!strncmp(data, "bl_ctrl_dcs", 11))
			ctrl_pdata->bklt_ctrl = BL_DCS_CMD;
	}

	pinfo->brightness_max = MDSS_MAX_BL_BRIGHTNESS;
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

	rc = of_property_read_u32(np,
		"qcom,mdss-pan-dsi-tx-eot-append", &tmp);
	pinfo->mipi.tx_eot_append = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-virtual-channel-id", &tmp);
	pinfo->mipi.vc = (!rc ? tmp : 0);

	rc = of_property_read_u32(np, "qcom,mdss-dsi-color-order", &tmp);
	pinfo->mipi.rgb_swap = (!rc ? tmp : DSI_RGB_SWAP_RGB);

	rc = of_property_read_u32(np, "qcom,mdss-force-clk-lane-hs", &tmp);
	pinfo->mipi.force_clk_lane_hs = (!rc ? tmp : 0);

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

	pinfo->mipi.lp11_init = of_property_read_bool(np,
					"qcom,mdss-dsi-lp11-init");
	rc = of_property_read_u32(np, "qcom,mdss-dsi-init-delay-us", &tmp);
	pinfo->mipi.init_delay = (!rc ? tmp : 0);

	mdss_panel_parse_te_params(np, pinfo);

	mdss_samsung_parse_panel_cmd(np, &display_on_seq,
			"qcom,mdss-display-on-seq");
	mdss_samsung_parse_panel_cmd(np, &display_on_cmd,
			"qcom,mdss-display-on-cmd");
	mdss_samsung_parse_panel_cmd(np, &display_off_seq,
			"qcom,mdss-display-off-seq");
	mdss_samsung_parse_panel_cmd(np, &manufacture_id_cmds,
			"samsung,panel-manufacture-id-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &mtp_id_cmds,
			"samsung,panel-mtp-id-read-cmds");
	mdss_samsung_parse_panel_cmd(np, &mtp_enable_cmds,
			"samsung,panel-mtp-enable-cmds");
	mdss_samsung_parse_panel_cmd(np, &gamma_cmds_list,
			"samsung,panel-gamma-cmds-list");
	mdss_samsung_parse_panel_cmd(np, &backlight_cmds,
			"samsung,panel-backlight-cmds");

	mdss_samsung_parse_candella_lux_mapping_table(np,
			&candela_map_table,
			"samsung,panel-candella-mapping-table-300");
	mdss_samsung_parse_panel_cmd(np, &rddpm_cmds,
				"samsung,panel-rddpm-read-cmds");

#if defined(ALPM_MODE)
	mdss_samsung_parse_panel_cmd(np, &alpm_on_seq,
				"samsung,panel-alpm-on-seq");
	mdss_samsung_parse_panel_cmd(np, &alpm_off_seq,
				"samsung,panel-alpm-off-seq");
#endif

	return 0;
error:
	return -EINVAL;
}

#if defined(CONFIG_LCD_CLASS_DEVICE)
static ssize_t mdss_dsi_disp_get_power(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	pr_info("mipi_samsung_disp_get_power(0)\n");
	return 0;
}

static ssize_t mdss_dsi_disp_set_power(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int power;
	if (sscanf(buf, "%u", &power) != 1)
		return -EINVAL;
	pr_info("mipi_samsung_disp_set_power:%d\n", power);
	return size;
}

static DEVICE_ATTR(lcd_power, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_dsi_disp_get_power,
			mdss_dsi_disp_set_power);

static ssize_t mdss_siop_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;

	rc = snprintf(buf, PAGE_SIZE, "%d\n", msd.dstat.siop_status);
	pr_info("%s : siop status : %d\n", __func__, msd.dstat.siop_status);
	return rc;
}
static ssize_t mdss_siop_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{

	if (sysfs_streq(buf, "1") && !msd.dstat.siop_status)
		msd.dstat.siop_status = true;
	else if (sysfs_streq(buf, "0") && msd.dstat.siop_status)
		msd.dstat.siop_status = false;
	else
		pr_info("%s: Invalid argument!!", __func__);

	return size;

}

static DEVICE_ATTR(siop_enable, S_IRUGO | S_IWUSR | S_IWGRP,
			mdss_siop_enable_show,
			mdss_siop_enable_store);


static struct lcd_ops mdss_dsi_disp_props = {

	.get_power = NULL,
	.set_power = NULL,

};

static ssize_t mdss_disp_lcdtype_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "SDC_%x\n", msd.manufacture_id);
}
static DEVICE_ATTR(lcd_type, S_IRUGO, mdss_disp_lcdtype_show, NULL);

#endif

static int is_panel_supported(const char *panel_name)
{
	int i = 0;

	if (panel_name == NULL)
		return -EINVAL;
	while (panel_supp_cdp[i].name != NULL) {
		if (!strcmp(panel_name, panel_supp_cdp[i].name))
			break;
		i++;
	}
	if (i < ARRAY_SIZE(panel_supp_cdp)) {
		memcpy(msd.panel_name, panel_name, MAX_PANEL_NAME_SIZE);
		msd.panel = panel_supp_cdp[i].panel_code;
		return 0;
	}
	return -EINVAL;
}

static int mipi_samsung_rddpm_status(void)
{
	mdss_dsi_cmd_receive(msd.ctrl_pdata,
				&rddpm_cmds.cmd_desc[0],
				rddpm_cmds.read_size[0]);

	return (int)msd.ctrl_pdata->rx_buf.data[0];
}

static int samsung_dsi_panel_event_handler(int event)
{
	pr_debug("%s : %d", __func__, event);
	switch (event) {
	case MDSS_EVENT_FRAME_UPDATE:
		if (msd.dstat.wait_disp_on) {
			mipi_samsung_disp_send_cmd(PANEL_DISPLAY_ON, true);
			msd.dstat.wait_disp_on = 0;
			pr_info("DISPLAY_ON(rddpm: 0x%x)\n",
					mipi_samsung_rddpm_status());
		}
		break;
	default:
		pr_debug("%s : unknown event (%d)\n", __func__, event);
		break;
	}

	return 0;
}

static ssize_t mipi_samsung_ambient_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static struct mdss_panel_info *pinfo;

	if (msd.pdata && unlikely(!pinfo))
		pinfo = &msd.pdata->panel_info;

	pr_info("[ALPM_DEBUG] %s: current status : %d\n",
					 __func__, pinfo->is_suspending);

	return 0;
}

static ssize_t mipi_samsung_ambient_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ambient_mode = 0;
	sscanf(buf, "%d" , &ambient_mode);

	pr_info("[ALPM_DEBUG] %s: mode : %d\n", __func__, ambient_mode);

	if (msd.dstat.on) {
		if (ambient_mode)
			mdss_dsi_panel_bl_dim(msd.pdata,
					PANEL_BACKLIGHT_RESTORE);
		else
			mdss_dsi_panel_bl_dim(msd.pdata, PANEL_BACKLIGHT_DIM);
	} else
		pr_info("[ALPM_DEBUG] %s: The LCD already turned off\n"
					, __func__);

	alpm_enable(ambient_mode ? MODE_OFF : ALPM_MODE_ON);

	return 0;
}
static DEVICE_ATTR(ambient, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_ambient_show,
			mipi_samsung_ambient_store);
#if defined(ALPM_MODE)
static ssize_t mipi_samsung_alpm_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int rc;
	static struct mdss_panel_info *pinfo;
	int current_status = 0;

	if (msd.pdata && unlikely(!pinfo))
		pinfo = &msd.pdata->panel_info;

	if (pinfo && pinfo->alpm_event)
		current_status = (int)pinfo->alpm_event(CHECK_CURRENT_STATUS);

	rc = snprintf(buf, PAGE_SIZE, "%d\n", current_status);
	pr_info("[ALPM_DEBUG] %s: current status : %d\n",
					 __func__, current_status);

	return rc;
}

static ssize_t mipi_samsung_alpm_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int mode = 0;
	struct mdss_panel_info *pinfo = &msd.pdata->panel_info;
	struct display_status *dstat = &msd.dstat;

	sscanf(buf, "%d" , &mode);
	pr_info("[ALPM_DEBUG] %s: mode : %d\n", __func__, mode);

	/*
	 * Possible mode status for Blank(0) or Unblank(1)
	 *	* Blank *
	 *		1) ALPM_MODE_ON
	 *			-> That will set during wakeup
	 *	* Unblank *
	 */
	if (mode == ALPM_MODE_ON) {
		alpm_store(mode);
		/*
		 * This will work if the ALPM must be on or chagne partial area
		 * if that already in the status of unblank
		 */
		if (dstat->on) {
			if (!pinfo->alpm_event(CHECK_PREVIOUS_STATUS)
				&& pinfo->alpm_event(CHECK_CURRENT_STATUS)) {
				/* Turn On ALPM Mode */
				mipi_samsung_disp_send_cmd(PANEL_ALPM_ON, true);
				if (dstat->wait_disp_on == 0) {
					/* wait 1 frame(more than 16ms) */
					msleep(20);
					mipi_samsung_disp_send_cmd(
						PANEL_DISPLAY_ON, true);
				}
				pinfo->alpm_event(STORE_CURRENT_STATUS);
				pr_info("[ALPM_DEBUG] %s: Send ALPM mode on cmds\n",
							__func__);
			}
		}
	} else if (mode == MODE_OFF) {
		if (pinfo->alpm_event) {
			alpm_store(mode);
			if (pinfo->alpm_event(CHECK_PREVIOUS_STATUS)
						 == ALPM_MODE_ON) {
				if (dstat->on) {
					mipi_samsung_disp_send_cmd(
						PANEL_ALPM_OFF, true);
					/* wait 1 frame(more than 16ms) */
					msleep(20);
					pinfo->alpm_event(CLEAR_MODE_STATUS);
				}
				pr_info("[ALPM_DEBUG] %s: Send ALPM off cmds\n",
							__func__);
			}
		}
	} else {
		pr_info("[ALPM_DEBUG] %s: no operation\n", __func__);
	}

	return size;
}

/*
 * This will use to enable/disable or check the status of ALPM
 * * Description for STATUS_OR_EVENT_FLAG *
 *	1) ALPM_MODE_ON
 *	2) CHECK_CURRENT_STATUS
 *		-> Check current status
 *			that will return current status
 *			 like ALPM_MODE_ON or MODE_OFF
 *	3) CHECK_PREVIOUS_STATUS
 *		-> Check previous status that will return previous status like
 *			 ALPM_MODE_ON or MODE_OFF
 *	4) STORE_CURRENT_STATUS
 *		-> Store current status to previous status because that will use
 *			for next turn on sequence
 *	5) CLEAR_MODE_STATUS
 *		-> Clear current and previous status as MODE_OFF status
 *			 that can use with
 *	* Usage *
 *		Call function "alpm_event_func(STATUS_OR_EVENT_FLAG)"
 */
u8 alpm_event_func(u8 flag)
{
	static u8 current_status;
	static u8 previous_status;
	u8 ret = 0;

	switch (flag) {
	case ALPM_MODE_ON:
		current_status = ALPM_MODE_ON;
		break;
	case MODE_OFF:
		current_status = MODE_OFF;
		break;
	case CHECK_CURRENT_STATUS:
		ret = current_status;
		break;
	case CHECK_PREVIOUS_STATUS:
		ret = previous_status;
		break;
	case STORE_CURRENT_STATUS:
		previous_status = current_status;
		break;
	case CLEAR_MODE_STATUS:
		previous_status = 0;
		current_status = 0;
		break;
	default:
		break;
	}

	pr_debug("[ALPM_DEBUG] current_status : %d, previous_status : %d, ret : %d\n",
				current_status, previous_status, ret);

	return ret;
}

static void alpm_store(u8 mode)
{
	struct mdss_panel_info *pinfo = &msd.pdata->panel_info;

	/* Register ALPM event function */
	if (unlikely(!pinfo->alpm_event))
		pinfo->alpm_event = alpm_event_func;

	pinfo->alpm_event(mode);
}
static DEVICE_ATTR(alpm, S_IRUGO | S_IWUSR | S_IWGRP,
			mipi_samsung_alpm_show,
			mipi_samsung_alpm_store);
#endif

static struct attribute *panel_sysfs_attributes[] = {
	&dev_attr_lcd_power.attr,
	&dev_attr_siop_enable.attr,
	&dev_attr_lcd_type.attr,
#if defined(ALPM_MODE)
	&dev_attr_alpm.attr,
#endif
	&dev_attr_ambient.attr,
	NULL
};
static const struct attribute_group panel_sysfs_group = {
	.attrs = panel_sysfs_attributes,
};

int mdss_dsi_panel_init(struct device_node *node,
	struct mdss_dsi_ctrl_pdata *ctrl_pdata,
	bool cmd_cfg_cont_splash)
{
	int rc = 0;
	static const char *panel_name;
	bool cont_splash_enabled;
	struct mdss_panel_info *pinfo;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct lcd_device *lcd_device;

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	struct backlight_device *bd = NULL;
#endif
#endif
#if defined(CONFIG_LCD_CLASS_DEVICE)
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	np = of_parse_phandle(node,
			"qcom,mdss-dsi-panel-controller", 0);
	if (!np) {
		pr_err("%s: Dsi controller node not initialized\n", __func__);
		return -EPROBE_DEFER;
	}

	pdev = of_find_device_by_node(np);
#endif
	mutex_init(&msd.lock);
	if (!node) {
		pr_err("%s: no panel node\n", __func__);
		return -ENODEV;
	}

	pr_debug("%s:%d\n", __func__, __LINE__);
	panel_name = of_get_property(node, "qcom,mdss-dsi-panel-name", NULL);
	if (!panel_name)
		pr_info("%s:%d, Panel name not specified\n",
						__func__, __LINE__);
	else
		pr_info("%s: Panel Name = %s\n", __func__, panel_name);

	rc = is_panel_supported(panel_name);
	if (rc) {
		LCD_DEBUG("Panel : %s is not supported:", panel_name);
		return rc;
	}

	rc = mdss_panel_parse_dt(node, ctrl_pdata);
	if (rc) {
		pr_err("%s:%d panel dt parse failed\n", __func__, __LINE__);
		return rc;
	}

	pinfo = &ctrl_pdata->panel_data.panel_info;

	pinfo->ulps_feature_enabled = of_property_read_bool(np,
		"qcom,ulps-enabled");
	pr_info("%s: ulps feature %s", __func__,
		(pinfo->ulps_feature_enabled ? "enabled" : "disabled"));
	pinfo->ulps_feature_enabled = true;

	/*if (cmd_cfg_cont_splash)*/
		cont_splash_enabled = of_property_read_bool(node,
				"qcom,cont-splash-enabled");
	/*else
		cont_splash_enabled = false;*/
	if (!cont_splash_enabled) {
		pr_info("%s:%d Continuous splash flag not found.\n",
				__func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 0;
	} else {
		pr_info("%s:%d Continuous splash flag enabled.\n",
				__func__, __LINE__);
		ctrl_pdata->panel_data.panel_info.cont_splash_enabled = 1;
	}

	ctrl_pdata->on = mdss_dsi_panel_on;
	ctrl_pdata->off = mdss_dsi_panel_off;
	ctrl_pdata->event_handler = samsung_dsi_panel_event_handler;
	ctrl_pdata->panel_data.set_backlight = mdss_dsi_panel_bl_ctrl;
	ctrl_pdata->panel_data.send_alpm = mdss_dsi_panel_alpm_ctrl;
	ctrl_pdata->panel_reset = mdss_dsi_samsung_panel_reset;
	ctrl_pdata->registered = mdss_dsi_panel_registered;
#if defined(CONFIG_LCD_CLASS_DEVICE)
	lcd_device = lcd_device_register("panel", &pdev->dev, NULL,
					&mdss_dsi_disp_props);

	if (IS_ERR(lcd_device)) {
		rc = PTR_ERR(lcd_device);
		pr_info("lcd : failed to register device\n");
		return rc;
	}

	sysfs_remove_file(&lcd_device->dev.kobj, &dev_attr_lcd_power.attr);

	rc = sysfs_create_group(&lcd_device->dev.kobj, &panel_sysfs_group);
	if (rc) {
		pr_err("Failed to create panel sysfs group..\n");
		sysfs_remove_group(&lcd_device->dev.kobj, &panel_sysfs_group);
		return rc;
	}

#if defined(CONFIG_BACKLIGHT_CLASS_DEVICE)
	bd = backlight_device_register("panel", &lcd_device->dev,
			NULL, NULL, NULL);
	if (IS_ERR(bd)) {
		rc = PTR_ERR(bd);
		pr_info("backlight : failed to register device\n");
		return rc;
	}
#endif
#endif

	msd.msm_pdev = pdev;
	msd.dstat.on = 0;

	disp_esd_gpio = of_get_named_gpio(node, "qcom,esd-det-gpio", 0);
	rc = gpio_request(disp_esd_gpio, "err_fg");
	if (rc) {
		pr_err("request gpio GPIO_ESD failed, ret=%d\n", rc);
		gpio_free(disp_esd_gpio);
		return rc;
	}
	gpio_tlmm_config(GPIO_CFG(disp_esd_gpio, 0, GPIO_CFG_INPUT,
			GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_direction_input(disp_esd_gpio);
	if (unlikely(rc < 0)) {
		pr_err("%s: failed to set gpio %d as input (%d)\n",
			__func__, disp_esd_gpio, rc);
	}

	return 0;
}
