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

#define mdp4_dsi_cmd_dma_busy_wait(x) { }
#define mdp4_dsi_blt_dmap_busy_wait(x) { }
#define mdp4_overlay_dsi_video_wait4event(x, y) { }

static struct mipi_mot_panel *mot_panel;

static char manufacture_id[2] = {DCS_CMD_READ_DA, 0x00}; /* DTYPE_DCS_READ */
static char controller_ver[2] = {DCS_CMD_READ_DB, 0x00};
static char controller_drv_ver[2] = {DCS_CMD_READ_DC, 0x00};
static char display_on[2] = {DCS_CMD_SET_DISPLAY_ON, 0x00};
static char display_off[2] = {DCS_CMD_SET_DISPLAY_OFF, 0x00};
static char get_power_mode[2] = {DCS_CMD_GET_POWER_MODE, 0x00};

static struct dsi_cmd_desc mot_manufacture_id_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 0, sizeof(manufacture_id), manufacture_id};

static struct dsi_cmd_desc mot_controller_ver_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(controller_ver), controller_ver};

static struct dsi_cmd_desc mot_controller_drv_ver_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(controller_drv_ver),
							controller_drv_ver};

static struct dsi_cmd_desc mot_display_on_cmd = {
	DTYPE_DCS_WRITE, 1, 0, 0, 5, sizeof(display_on), display_on};

static struct dsi_cmd_desc mot_display_off_cmd = {
	DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_off), display_off};

static struct dsi_cmd_desc mot_get_pwr_mode_cmd = {
	DTYPE_DCS_READ,  1, 0, 1, 0, sizeof(get_power_mode), get_power_mode};


int mipi_mot_panel_on(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *tp = mot_panel->mot_tx_buf;

	mdp4_dsi_cmd_dma_busy_wait(mfd);
	mipi_dsi_mdp_busy_wait();

	mipi_dsi_buf_init(tp);
	mipi_dsi_cmds_tx(tp, &mot_display_on_cmd, 1);

	return 0;
}

int mipi_mot_panel_off(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *tp = mot_panel->mot_tx_buf;

	mdp4_dsi_cmd_dma_busy_wait(mfd);
	mipi_dsi_mdp_busy_wait();

	mipi_dsi_buf_init(tp);
	mipi_dsi_cmds_tx(tp, &mot_display_off_cmd, 1);

	return 0;
}

static int32 get_panel_info(struct msm_fb_data_type *mfd,
				struct  mipi_mot_panel *mot_panel,
				struct dsi_cmd_desc *cmd)
{
	struct dsi_buf *rp, *tp;
	uint32 *lp;
	int ret;

	tp = mot_panel->mot_tx_buf;
	rp = mot_panel->mot_rx_buf;
	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);

	ret = mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 1);
	if (!ret)
		ret = -1;
	else {
		lp = (uint32 *)rp->data;
		ret = (int)*lp;
	}

	return ret;
}

void mipi_mot_set_mot_panel(struct mipi_mot_panel *mot_panel_ptr)
{
	mot_panel = mot_panel_ptr;
}

u8 mipi_mode_get_pwr_mode(struct msm_fb_data_type *mfd)
{
	struct dsi_cmd_desc *cmd;
	u8 power_mode;

	cmd = &mot_get_pwr_mode_cmd;
	power_mode = get_panel_info(mfd, mot_panel, cmd);

	pr_debug("%s: panel power mode = 0x%x\n", __func__, power_mode);
	return power_mode;
}

u16 mipi_mot_get_manufacture_id(struct msm_fb_data_type *mfd)
{
	struct dsi_cmd_desc *cmd;
	static int manufacture_id = INVALID_VALUE;

	if (manufacture_id == INVALID_VALUE) {
		if (mot_panel == NULL) {
			pr_err("%s: invalid mot_panel\n", __func__);
			return -EAGAIN;
		}

		cmd = &mot_manufacture_id_cmd;
		manufacture_id = get_panel_info(mfd, mot_panel, cmd);
	}

	return manufacture_id;
}


u16 mipi_mot_get_controller_ver(struct msm_fb_data_type *mfd)
{
	struct dsi_cmd_desc *cmd;
	static int controller_ver = INVALID_VALUE;

	if (controller_ver == INVALID_VALUE) {
		if (mot_panel == NULL) {
			pr_err("%s: invalid mot_panel\n", __func__);
			return -EAGAIN;
		}

		cmd = &mot_controller_ver_cmd;
		controller_ver =  get_panel_info(mfd, mot_panel, cmd);
	}

	return controller_ver;
}


u16 mipi_mot_get_controller_drv_ver(struct msm_fb_data_type *mfd)
{
	struct dsi_cmd_desc *cmd;
	static int controller_drv_ver = INVALID_VALUE;

	if (controller_drv_ver == INVALID_VALUE) {
		if (mot_panel == NULL) {
			pr_err("%s: invalid mot_panel\n", __func__);
			return -EAGAIN;
		}

		cmd = &mot_controller_drv_ver_cmd;
		controller_drv_ver = get_panel_info(mfd, mot_panel, cmd);
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

void mipi_mot_mipi_busy_wait(struct msm_fb_data_type *mfd)
{
	/* Todo: consider to remove mdp4_dsi_cmd_dma_busy_wait
	 * mipi_dsi_cmds_tx/rx wait for dma completion already.
	 */
	if (mfd->panel_info.type == MIPI_CMD_PANEL) {
		mdp4_dsi_cmd_dma_busy_wait(mfd);
		mipi_dsi_mdp_busy_wait();
		mdp4_dsi_blt_dmap_busy_wait(mfd);
	} else if (mfd->panel_info.type == MIPI_VIDEO_PANEL) {
		mdp4_overlay_dsi_video_wait4event(mfd, INTR_PRIMARY_VSYNC);
		mdp4_dsi_cmd_dma_busy_wait(mfd);
		mdp4_dsi_blt_dmap_busy_wait(mfd);
	}

}

static int mipi_read_cmd_locked(struct msm_fb_data_type *mfd,
					struct dsi_cmd_desc *cmd, u8 *rd_data)
{
	int ret;

	mutex_lock(&mfd->dma->ov_mutex);
	if (atomic_read(&mot_panel->state) == MOT_PANEL_OFF) {
		ret = MOT_ESD_PANEL_OFF;
		goto panel_off_ret;
	} else {
		mipi_set_tx_power_mode(0);
		mipi_mot_mipi_busy_wait(mfd);
		/* For video mode panel, after INTR_PRIMARY_VSYNC happened,
		 * only have 5H(VSA+VBP) left for blanking period, might not
		 * have enough time to complete read command. Want to
		 * wait for 100us more (about 5H * 13us per line = 65us)
		 * before calling mdp to send out read command.
		 * with this delay, guarant mdp will send out read command
		 * at the beginning of next blanking period.
		 */
		if (mfd->panel_info.type == MIPI_VIDEO_PANEL)
			udelay(100);
		*rd_data = (u8)get_panel_info(mfd, mot_panel, cmd);
	}

	ret = MOT_ESD_OK;

panel_off_ret:
	mutex_unlock(&mfd->dma->ov_mutex);
	return ret;
}

static int mipi_mot_esd_detection(struct msm_fb_data_type *mfd)
{
	u8 expected_mode, pwr_mode = 0;
	u8 rd_manufacture_id;
	int ret;

	ret = mipi_read_cmd_locked(mfd, &mot_get_pwr_mode_cmd,
							&pwr_mode);
	if (ret == MOT_ESD_PANEL_OFF)
		return ret;
	/*
	 * There is a issue of the mipi_dsi_cmds_rx(), and case# 00743147
	 * is opened for this API, but the patch from QCOm doesn't fix the
	 * problem. In this commit will include the change from QCOM also.
	 *
	 * During the ESD test, if there is any issue of the MDP/DSI or
	 * panel, this mipi_dsi_cmds_rx() will return the data of the previous
	 * read.
	 * To workaround this problem, we will provide 2 reads, and if ESD
	 * happens, then 1 of data will be wrong, then the ESD will be kicked
	 * in.
	 * For Video mode, the blanking time will not big enough for 2 read
	 * commands, therefore we will free the DSI bus for 100msec after
	 * the first read.
	 */
	msleep(42);	/* wait for 2.5 frames */

	ret = mipi_read_cmd_locked(mfd, &mot_manufacture_id_cmd,
						&rd_manufacture_id);
	if (ret == MOT_ESD_PANEL_OFF)
		return ret;

	expected_mode = 0x9c;

	if (pwr_mode != expected_mode) {
		pr_err("%s: Power mode in incorrect state. " \
				"Cur_mode=0x%x Expected_mode=0x%x ",
					__func__, pwr_mode, expected_mode);
		ret = MOT_ESD_ESD_DETECT;
		goto esd_detect;
	}

	if (mot_panel->is_valid_manufacture_id(mfd, rd_manufacture_id))
		ret = MOT_ESD_OK;
	else {
		pr_err("%s: wrong manufacture_id: 0x%x\n",
			__func__, rd_manufacture_id);
		ret = MOT_ESD_ESD_DETECT;
	}

esd_detect:
	pr_debug("%s: Cur_mode=0x%x Expected_mode=0x%x manufacture_id=0x%x\n",
			__func__, pwr_mode, expected_mode, rd_manufacture_id);
	return ret;
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

	mfd = mot_panel->mfd;

	if (mot_panel == NULL) {
		pr_err("%s: invalid mot_panel\n", __func__);
		return;
	}

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
