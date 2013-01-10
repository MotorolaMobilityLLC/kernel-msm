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

/* MIPI_MOT_PANEL_ESD_TEST is used to run the ESD recovery stress test */
/* #define MIPI_MOT_PANEL_ESD_TEST	1 */
#define MIPI_MOT_PANEL_ESD_CNT_MAX	3

static struct mipi_mot_panel *mot_panel;

static char manufacture_id[2] = {DCS_CMD_READ_DA, 0x00}; /* DTYPE_DCS_READ */
static char controller_ver[2] = {DCS_CMD_READ_DB, 0x00};
static char controller_drv_ver[2] = {DCS_CMD_READ_DC, 0x00};
static char display_on[2] = {DCS_CMD_SET_DISPLAY_ON, 0x00};
static char normal_mode_on[2] = {DCS_CMD_SET_NORMAL_MODE_ON, 0x00};
static char display_off[2] = {DCS_CMD_SET_DISPLAY_OFF, 0x00};
static char get_power_mode[2] = {DCS_CMD_GET_POWER_MODE, 0x00};

static struct dsi_cmd_desc mot_manufacture_id_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 0, sizeof(manufacture_id), manufacture_id};

static struct dsi_cmd_desc mot_controller_ver_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(controller_ver), controller_ver};

static struct dsi_cmd_desc mot_controller_drv_ver_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(controller_drv_ver),
							controller_drv_ver};

static struct dsi_cmd_desc mot_display_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 5, sizeof(display_on), display_on}
};

static struct dsi_cmd_desc mot_display_normal_mode_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(normal_mode_on), normal_mode_on},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0, sizeof(display_on), display_on},
};

static struct dsi_cmd_desc mot_display_off_cmd = {
	DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_off), display_off};

static struct dsi_cmd_desc mot_get_pwr_mode_cmd = {
	DTYPE_DCS_READ,  1, 0, 1, 0, sizeof(get_power_mode), get_power_mode};

static struct dsi_cmd_desc mot_hide_img_cmd = {
	DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_off), display_off};

int mipi_mot_panel_on(struct msm_fb_data_type *mfd)
{
	struct dcs_cmd_req cmdreq;

	if (mfd->resume_cfg.partial) {
		pr_debug("%s: sending normal mode on\n", __func__);
		cmdreq.cmds = &mot_display_normal_mode_on_cmds[0];
		cmdreq.cmds_cnt = ARRAY_SIZE(mot_display_normal_mode_on_cmds);
	} else {
		pr_debug("%s: sending display on\n", __func__);
		cmdreq.cmds = &mot_display_on_cmds[0];
		cmdreq.cmds_cnt = ARRAY_SIZE(mot_display_on_cmds);
	}
	cmdreq.flags = CMD_REQ_TX | CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

	return 0;
}

int mipi_mot_panel_off(struct msm_fb_data_type *mfd)
{
	struct dcs_cmd_req cmdreq;

	cmdreq.cmds = &mot_display_off_cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_TX | CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

	return 0;
}

/* TODO: need to check for ERR return when read is failed */
/*
 * This API is used to send the DSI MIPI command right away
 * (flags = CMD_REQ_COMMIT) when DSI link is free.
*/
int mipi_mot_rx_cmd(struct dsi_cmd_desc *cmd, u8 *data, int rlen)
{
	int ret;
	struct dcs_cmd_req cmdreq;

	pr_debug("%s is called\n", __func__);
	cmdreq.cmds = cmd;
	cmdreq.cmds_cnt = 1;
	cmdreq.flags = CMD_REQ_RX | CMD_REQ_COMMIT;
	cmdreq.rlen = rlen;
	cmdreq.cb = NULL;

	ret = mipi_dsi_cmdlist_put(&cmdreq);
	if (ret < 0) {
		pr_err("%s: failed to read cmd=0x%x\n", __func__,
							cmd->payload[0]);
		goto end;
	} else if (ret != rlen)
		pr_warning("%s: read cmd=0x%x returns data(%d) != rlen(%d)\n",
					__func__, cmd->payload[0], rlen, ret);
	else if (cmdreq.rdata) {
		memcpy(data, cmdreq.rdata, rlen);
		ret = rlen;
	} else
		ret = 0;
end:
	return ret;
}

void mipi_mot_set_mot_panel(struct mipi_mot_panel *mot_panel_ptr)
{
	mot_panel = mot_panel_ptr;
}

/* TODO: need to check for ERR return*/
/*
 * This API is used to send the DSI MIPI command right away
 * (flags = CMD_REQ_COMMIT) when DSI link is free
*/
void mipi_mot_tx_cmds(struct dsi_cmd_desc *cmds, int cnt)
{
	struct dcs_cmd_req cmdreq;

	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cnt;
	cmdreq.flags = CMD_REQ_TX | CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	mipi_dsi_cmdlist_put(&cmdreq);

	pr_debug("%s: is called for list cmd ; size= %d\n",
						__func__, cmdreq.cmds_cnt);
}

u8 mipi_mode_get_pwr_mode(struct msm_fb_data_type *mfd)
{
	int ret;
	u8 power_mode;

	ret = mipi_mot_rx_cmd(&mot_get_pwr_mode_cmd, &power_mode, 1);
	if (ret < 0) {
		pr_err("%s: failed to read pwr_mode\n", __func__);
		power_mode = 0;
	}

	pr_debug("%s: panel power mode = 0x%x\n", __func__, power_mode);
	return power_mode;
}

u16 mipi_mot_get_manufacture_id(struct msm_fb_data_type *mfd)
{
	static int manufacture_id = INVALID_VALUE;
	u8 rdata;
	int ret;

	if (manufacture_id == INVALID_VALUE) {
		ret =  mipi_mot_rx_cmd(&mot_manufacture_id_cmd,
							&rdata, 1);
		if (ret < 0) {
			pr_err("%s: failed to read manufacture_id\n", __func__);
			manufacture_id = ret;
		} else
			manufacture_id = (int)rdata;
	}

	return manufacture_id;
}


u16 mipi_mot_get_controller_ver(struct msm_fb_data_type *mfd)
{
	static int controller_ver = INVALID_VALUE;
	u8 rdata;
	int ret;

	if (controller_ver == INVALID_VALUE) {
		ret  = mipi_mot_rx_cmd(&mot_controller_ver_cmd, &rdata, 1);
		if (ret < 0) {
			pr_err("%s: failed to read controller_ver\n", __func__);
			controller_ver = ret;
		} else
			controller_ver = (int)rdata;
	}

	return controller_ver;
}


u16 mipi_mot_get_controller_drv_ver(struct msm_fb_data_type *mfd)
{
	static int controller_drv_ver = INVALID_VALUE;
	u8 rdata;
	int ret;

	if (controller_drv_ver == INVALID_VALUE) {
		ret = mipi_mot_rx_cmd(&mot_controller_drv_ver_cmd, &rdata, 1);
		if (ret < 0) {
			pr_err("%s: failed to read controller_drv_ver\n",
								__func__);
			controller_drv_ver = ret;
		} else
			controller_drv_ver = (int)rdata;
	}

	return controller_drv_ver;
}

static int esd_recovery_start(struct msm_fb_data_type *mfd)
{
	struct msm_fb_panel_data *pdata;

	pr_info("MIPI MOT: ESD recovering is started\n");

	pdata = (struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;

	if (!mfd->panel_power_on)  /* panel is off, do nothing */
		goto end;

	mfd->panel_power_on = FALSE;
	pdata->off(mfd->pdev);
	msleep(20);
	pdata->on(mfd->pdev);
	mfd->panel_power_on = TRUE;
	mfd->dma_fnc(mfd);
end:
	pr_info("MIPI MOT: ESD recovering is done\n");

	return 0;
}

static int mipi_mot_esd_detection(struct msm_fb_data_type *mfd)
{
	/*
	 * TODO.. This will need to redo in the different way in other
	 * ESD patch
	 */
	return MOT_ESD_OK;
}

void mipi_mot_esd_work(void)
{
	struct msm_fb_data_type *mfd;
	int i;
	int ret;
#ifdef MIPI_MOT_PANEL_ESD_TEST
	static int esd_count;
	static int esd_trigger_cnt;
#endif

	if (mot_panel == NULL) {
		pr_err("%s: invalid mot_panel\n", __func__);
		return;
	}

	mfd = mot_panel->mfd;

	/*
	 * Try to run ESD detection on the panel MOT_PANEL_ESD_NUM_TRY_MAX
	 * times, if the response data are incorrect then start to reset the
	 * MDP and panel
	 */

	for (i = 0; i < MOT_PANEL_ESD_NUM_TRY_MAX; i++) {
		if (i > 0)
			msleep(100);

		ret =  mipi_mot_esd_detection(mfd);
		if (ret == MOT_ESD_OK)
			break;
		/* If the panel is off then just exit and not queue ESD work */
		else if (ret == MOT_ESD_PANEL_OFF)
			return;
	}

	if (i >= MOT_PANEL_ESD_NUM_TRY_MAX)
		esd_recovery_start(mot_panel->mfd);

#ifdef MIPI_MOT_PANEL_ESD_TEST
	if (esd_count++ >= MIPI_MOT_PANEL_ESD_CNT_MAX) {
		pr_info("%s(%d): start to ESD test\n", __func__,
							esd_trigger_cnt++);
		esd_count = 0;
		esd_recovery_start(mot_panel->mfd);
	} else
		pr_info("%s(%d):is called.\n", __func__, esd_trigger_cnt++);
#endif
	queue_delayed_work(mot_panel->esd_wq, &mot_panel->esd_work,
						MOT_PANEL_ESD_CHECK_PERIOD);
}
int mipi_mot_hide_img(struct msm_fb_data_type *mfd, int hide)
{
	pr_debug("%s(%d)\n", __func__, hide);
	if ((mfd->op_enable != 0) && (mfd->panel_power_on != 0)) {
		/* TODO: implement "unhide" */
		mipi_set_tx_power_mode(0);
		mipi_mot_tx_cmds(&mot_hide_img_cmd, 1);
	}
	return 0;
}
