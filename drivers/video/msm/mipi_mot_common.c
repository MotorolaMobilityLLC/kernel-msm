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

#include <linux/dropbox.h>
#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_mot.h"
#include "mdp4.h"

static struct mipi_mot_panel *mot_panel;

static char manufacture_id[2] = {DCS_CMD_READ_DA, 0x00}; /* DTYPE_DCS_READ */
static char controller_ver[2] = {DCS_CMD_READ_DB, 0x00};
static char controller_drv_ver[2] = {DCS_CMD_READ_DC, 0x00};
static char display_on[2] = {DCS_CMD_SET_DISPLAY_ON, 0x00};
static char display_off[2] = {DCS_CMD_SET_DISPLAY_OFF, 0x00};
static char get_power_mode[2] = {DCS_CMD_GET_POWER_MODE, 0x00};
static char get_raw_mtp[2] = {DCS_CMD_READ_RAW_MTP, 0x00};
static char  set_reg_offset_0[2] = {DCS_CMD_SET_OFFSET, 0x0};
static char  set_reg_offset_8[2] = {DCS_CMD_SET_OFFSET, 0x8};
static char  set_reg_offset_16[2] = {DCS_CMD_SET_OFFSET, 0x10};
static char exit_sleep[2] = {DCS_CMD_EXIT_SLEEP_MODE, 0x00};
static char set_tear_on[2] = {DCS_CMD_SET_TEAR_ON, 0x00};
static char set_tear_off[1] = {DCS_CMD_SET_TEAR_OFF};

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

static struct dsi_cmd_desc mot_display_off_cmd = {
	DTYPE_DCS_WRITE, 1, 0, 0, 10, sizeof(display_off), display_off};

static struct dsi_cmd_desc mot_get_pwr_mode_cmd = {
	DTYPE_DCS_READ,  1, 0, 1, 0, sizeof(get_power_mode), get_power_mode};

static struct dsi_cmd_desc mot_get_raw_mtp_cmd = {
	DTYPE_DCS_READ,  1, 0, 1, 0, sizeof(get_raw_mtp), get_raw_mtp};

static struct dsi_cmd_desc set_offset_cmd[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(set_reg_offset_0),
							set_reg_offset_0},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(set_reg_offset_8),
							set_reg_offset_8},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 1, sizeof(set_reg_offset_16),
							set_reg_offset_16},
};

static struct dsi_cmd_desc mot_display_exit_sleep_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 120, sizeof(exit_sleep), exit_sleep},
};

static struct dsi_cmd_desc mot_display_set_tear_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 1, sizeof(set_tear_on), set_tear_on},
};

static struct dsi_cmd_desc mot_display_set_tear_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 1, sizeof(set_tear_off), set_tear_off},
};

static u8 panel_raw_mtp[RAW_MTP_SIZE];
static u8 gamma_settings[NUM_NIT_LVLS][RAW_GAMMA_SIZE];


static DECLARE_COMPLETION(panel_on_delay_completion);
void mipi_mot_panel_on_timer_expire(unsigned long timeout)
{
	complete_all(&panel_on_delay_completion);
}

void mipi_mot_panel_exit_sleep(void)
{
	pr_debug("%s: exiting sleep mode [wait: %d ms][deferred: %d ms]\n",
					__func__, mot_panel->exit_sleep_wait,
					mot_panel->exit_sleep_panel_on_wait);
	mot_display_exit_sleep_cmds[0].wait =
					mot_panel->exit_sleep_wait;

	mipi_mot_tx_cmds(&mot_display_exit_sleep_cmds[0],
			ARRAY_SIZE(mot_display_exit_sleep_cmds));

	if (mot_panel->exit_sleep_panel_on_wait > 0) {
		pr_info("%s: start deferred wait [%d ms]\n", __func__,
				mot_panel->exit_sleep_panel_on_wait -
				mot_panel->exit_sleep_wait);
		INIT_COMPLETION(panel_on_delay_completion);
		mot_panel->exit_sleep_panel_on_timer.function =
						mipi_mot_panel_on_timer_expire;
		/* set the timer to fire ~(exit_sleep_panel_on_wait -
		   exit_sleep_wait) milliseconds in the future. We add a single
		   jiffy to this timer to ensure that the timer doesn't fire
		   early due to the restriction that jiffies have an accuracy
		   of 10ms.  This ensures that if our timer is set for 94ms in
		   the future it doesn't fire at 90ms, but instead at 100ms */
		mod_timer(&mot_panel->exit_sleep_panel_on_timer, jiffies + 1 +
				msecs_to_jiffies(
					mot_panel->exit_sleep_panel_on_wait -
					mot_panel->exit_sleep_wait));
	}
}

void mipi_mot_exit_sleep_wait()
{
	if (mot_panel->exit_sleep_panel_on_wait > 0) {
		pr_debug("%s: wait for exit_sleep timer\n", __func__);
		wait_for_completion_timeout(&panel_on_delay_completion,
			msecs_to_jiffies(
			mot_panel->exit_sleep_panel_on_wait) + 1);
		pr_debug("%s: wait for exit_sleep timer - done\n", __func__);
	}
}

int mipi_mot_panel_on(struct msm_fb_data_type *mfd)
{
	u8 pwr_mode;
	int ret = 0;
	u8 attempts = 1;
	u8 failures = 0;

	mipi_mot_exit_sleep_wait();

	mmi_panel_notify(MMI_PANEL_EVENT_POST_INIT, NULL);
	pr_info("%s: sending display on\n", __func__);
	do {
		mipi_mot_tx_cmds(&mot_display_on_cmds[0],
			ARRAY_SIZE(mot_display_on_cmds));
		mot_panel->esd_expected_pwr_mode = 0x94;

		ret = mipi_mot_get_pwr_mode(mfd, &pwr_mode);
		if (ret > 0) {
			pr_info("%s: Power_mode =0x%x\n", __func__,
				pwr_mode);
			/* validate screen is actually on */
			if ((pwr_mode & 0x04) != 0x04) {
				pr_err("%s: display on fail! [0x%x]\n",
					__func__, pwr_mode);
				ret = -1;
				failures = attempts;
			}
		} else
			pr_err("%s: Failed to get power_mode. [%d]\n",
				__func__, ret);
	} while (ret <= 0 && attempts++ < 5);

	if (failures > 0) {
		pr_err("%s: Display failure: DISON (0x04) bit not set | fail count: %d, display %s\n",
			__func__, failures,
			(ret > 0) ? "recovered" : "did not recover");
		dropbox_queue_event_empty("display_issue");
	}

	return 0;
}

int mipi_mot_panel_off(struct msm_fb_data_type *mfd)
{
	pr_debug("%s: sending display off\n", __func__);
	mipi_mot_tx_cmds(&mot_display_off_cmd, 1);

	return 0;
}

/* This API is used to send the DSI MIPI command right away
 * (flags = CMD_REQ_COMMIT) when DSI link is free.
*/
int mipi_mot_rx_cmd(struct dsi_cmd_desc *cmd, u8 *data, int rlen)
{
	int ret;
	struct dcs_cmd_req cmdreq;

	pr_debug("%s is called\n", __func__);
	/* if there is no display, then do nothing */
	if (mot_panel && mot_panel->is_no_disp)
		return rlen;

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

/* This API is used to send the DSI MIPI command right away
 * (flags = CMD_REQ_COMMIT) when DSI link is free
*/
int mipi_mot_tx_cmds(struct dsi_cmd_desc *cmds, int cnt)
{
	struct dcs_cmd_req cmdreq;
	struct dsi_cmd_desc *tx_cmds = cmds;
	int ret;

	/* if there is no display, then do nothing */
	if (mot_panel && mot_panel->is_no_disp)
		return cnt;

	cmdreq.cmds = cmds;
	cmdreq.cmds_cnt = cnt;
	cmdreq.flags = CMD_REQ_TX | CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;

	ret = mipi_dsi_cmdlist_put(&cmdreq);
	if (ret != cnt) {
		tx_cmds += ret;
		pr_err("%s: failed to tx cmd=0x%x\n", __func__,
							tx_cmds->payload[0]);
		ret = -1;
	} else {
		int i;
		pr_debug("%s: done with tx command; size= %d cmd: ",
						__func__, cmdreq.cmds_cnt);
		for (i = 0; i < cnt; i++) {
			pr_debug("0x%x ", tx_cmds->payload[0]);
			tx_cmds++;
		}
	}

	return ret;
}

int mipi_mot_exec_cmd_seq(struct msm_fb_data_type *mfd,
			struct mipi_mot_cmd_seq *seq, int cnt)
{
	int r = 0;
	int sub_ret;
	int i;
	struct mipi_mot_cmd_seq *cur;

	for (i = 0; i < cnt; i++) {
		cur = &seq[i];
		if (!cur) {
			pr_err("%s: Item %d - Invalid\n", __func__, i);
			r = -EINVAL;
			break;
		}

		if (cur->cond_func &&
			!cur->cond_func(mfd)) {
			pr_debug("%s: Item %d - Skipping\n", __func__, i);
			continue;
		}

		if (cur->type == MIPI_MOT_SEQ_TX) {
			pr_debug("%s: Item %d - TX MIPI cmd = 0x%02x, size = %d\n",
				__func__, i, cur->cmd.payload[0],
				cur->cmd.dlen);
			sub_ret = mipi_mot_tx_cmds(&cur->cmd, 1);
			if (sub_ret < 0) {
				pr_err("%s: Item %d - fail to call mot_tx_cmds\n",
							__func__, i);
				r = -EIO;
				break;
			}
		} else if (cur->type == MIPI_MOT_SEQ_EXEC_SUB_SEQ) {
			pr_debug("%s: Item %d - Executing sub sequence with %d items\n",
				__func__, i, cur->sub.count);
			sub_ret = mipi_mot_exec_cmd_seq(mfd, cur->sub.seq,
							cur->sub.count);
			if (sub_ret) {
				pr_err("%s: Item %d - sub sequence failed, ret = %d\n",
					__func__, i, sub_ret);
				r = sub_ret;
				break;
			}
		} else if (cur->type == MIPI_MOT_SEQ_TX_PWR_MODE_HS) {
			pr_debug("%s: Item %d - Enabling HS mode\n",
				__func__, i);
			mipi_set_tx_power_mode(0);
		} else if (cur->type == MIPI_MOT_SEQ_EXIT_SLEEP) {
			mipi_mot_panel_exit_sleep();
		}
	}

	return r;
}

int mipi_mot_get_pwr_mode(struct msm_fb_data_type *mfd, u8 *pwr_mode)
{
	int ret;

	ret = mipi_mot_rx_cmd(&mot_get_pwr_mode_cmd, pwr_mode, 1);
	if (ret <= 0)
		pr_err("%s: failed to read pwr_mode\n", __func__);

	pr_debug("%s: panel power mode = 0x%x\n", __func__, *pwr_mode);

	return ret;
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


int mipi_mot_is_valid_power_mode(struct msm_fb_data_type *mfd)
{
	u8 pwr_mode = 0;
	int ret = 1;

	ret = mipi_mot_get_pwr_mode(mfd, &pwr_mode);
	if (ret <= 0) {
		pr_err("%s: failed to get power mode. Ret = %d\n",
						__func__, ret);
		ret = 0;
	/*Bit7: Booster on ;Bit4: Sleep Out ;Bit2: Display On*/
	} else if ((pwr_mode & mot_panel->esd_expected_pwr_mode) !=
		mot_panel->esd_expected_pwr_mode) {
		pr_warning("%s: power state = 0x%x while 0x%x expected\n",
			__func__,
			(u32)pwr_mode,
			(u32)mot_panel->esd_expected_pwr_mode);
		ret = 0;
	}

	return ret;
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

int mipi_mot_get_raw_mtp(struct msm_fb_data_type *mfd)
{
	struct dsi_cmd_desc *cmd;
	int ret, i;
	unsigned char mtp[8];

	cmd = &mot_get_raw_mtp_cmd;

	/*
	 * display driver doesn't support read 24 bytes. It needs to be read
	 * in three times. 0xb0 is offset cmd.
	*/
	for (i = 0; i < 3; i++) {
		mipi_mot_tx_cmds(&set_offset_cmd[i], 1);
		ret = mipi_mot_rx_cmd(cmd, mtp, 8);
		if (ret > 0)
			memcpy(panel_raw_mtp + i * 8, mtp, 8);
		else {
			memset(panel_raw_mtp, 0, sizeof(panel_raw_mtp));
			pr_err("%s: %d.failed to read D3h\n", __func__, i);
			return -EIO;
		}
	}

	return 0;
}

void mipi_mot_dynamic_gamma_calc(uint32_t v0_val, uint8_t preamble_1,
			uint8_t preamble_2,
			uint16_t in_gamma[NUM_VOLT_PTS][NUM_COLORS])
{
	smd_dynamic_gamma_calc(v0_val, preamble_1, preamble_2, panel_raw_mtp,
			in_gamma, gamma_settings);
}

u8 *mipi_mot_get_gamma_setting(struct msm_fb_data_type *mfd, int level)
{
	if (level < 0)
		level = 0;
	else if (level > NUM_NIT_LVLS - 1)
		level = NUM_NIT_LVLS - 1;

	return gamma_settings[level];
}

#ifdef CONFIG_DEBUG_FS
static int panel_gamma_debug_show(struct seq_file *m, void *v)
{
	smd_dynamic_gamma_dbg_dump(panel_raw_mtp, gamma_settings, m, v, 0);
	return 0;
}

static int panel_gamma_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, panel_gamma_debug_show, NULL);
}

static const struct file_operations panel_gamma_debug_fops = {
	.open           = panel_gamma_debug_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};
#endif /* CONFIG_DEBUG_FS */

static int esd_recovery_start(struct msm_fb_data_type *mfd)
{
	struct msm_fb_panel_data *pdata;
	int ret;

	pr_info("MIPI MOT: ESD recovering is started\n");

	pdata = (struct msm_fb_panel_data *)mfd->pdev->dev.platform_data;

	if (!mfd->panel_power_on) { /* panel is off, do nothing */
		ret = MOT_ESD_PANEL_OFF;
		goto end;
	}

	if (mot_panel && atomic_read(&mot_panel->state) == MOT_PANEL_OFF) {
		ret = MOT_ESD_PANEL_OFF;
		goto end;
	}

	atomic_set(&mot_panel->state, MOT_PANEL_OFF);
	if (mot_panel->panel_disable)
		mot_panel->panel_disable(mfd);
	mipi_dsi_panel_power_enable(0);
	msleep(200);

	mipi_dsi_panel_power_enable(1);
	if (mot_panel->panel_enable)
		mot_panel->panel_enable(mfd);
	atomic_set(&mot_panel->state, MOT_PANEL_ON);
	mdp4_dsi_cmd_pipe_commit(0, 1);
	mipi_mot_panel_on(mfd);
	ret = MOT_ESD_OK;
end:
	pr_info("MIPI MOT: ESD recovering is done\n");

	return ret;
}

static int mipi_mot_esd_detection(struct msm_fb_data_type *mfd)
{
#ifdef MOT_PANEL_ESD_SELF_TRIGGER
	static int esd_count;
	static int esd_trigger_cnt;
#endif
	int ret;

	pr_debug("%s is called\n", __func__);

	if (atomic_read(&mot_panel->state) == MOT_PANEL_OFF) {
		pr_debug("%s exit because of panel is off.\n", __func__);
		return MOT_ESD_PANEL_OFF;
	}

	mutex_lock(&mfd->dma->ov_mutex);
	if (mot_panel->is_valid_power_mode)
		if (!mot_panel->is_valid_power_mode(mot_panel->mfd)) {
			mutex_unlock(&mfd->dma->ov_mutex);
			goto trigger_esd_recovery;
		}
	mutex_unlock(&mfd->dma->ov_mutex);

#ifdef MOT_PANEL_ESD_SELF_TRIGGER
	if (esd_count++ >= MIPI_MOT_PANEL_ESD_CNT_MAX) {
		pr_info("%s(%d): start to ESD test\n", __func__,
							esd_trigger_cnt++);
		esd_count = 0;
		goto trigger_esd_recovery;
	}
#endif

	return MOT_ESD_OK;

trigger_esd_recovery:
	ret = esd_recovery_start(mot_panel->mfd);

	return ret;
}

void mipi_mot_esd_work(void)
{
	struct msm_fb_data_type *mfd;
	int ret;

	mfd = mot_panel->mfd;

	if (mot_panel == NULL) {
		pr_err("%s: invalid mot_panel\n", __func__);
		goto end;
	}

	if (atomic_read(&mot_panel->state) == MOT_PANEL_OFF) {
		pr_debug("%s: panel is OFF.. exiting ESD thread\n",
								__func__);
		goto end;
	}

	pr_debug("%s: is called\n", __func__);
	mutex_lock(&mfd->panel_info.mipi.panel_mutex);
	ret = mipi_mot_esd_detection(mfd);
	mutex_unlock(&mfd->panel_info.mipi.panel_mutex);

	if (ret == MOT_ESD_PANEL_OFF) {
		mot_panel->esd_detection_run = false;
		pr_debug("%s: panel is OFF.. don't queue ESD worker thread\n",
								__func__);
	} else
		queue_delayed_work(mot_panel->esd_wq, &mot_panel->esd_work,
						MOT_PANEL_ESD_CHECK_PERIOD);
end:
	return;
}

void mipi_mot_set_tear(struct msm_fb_data_type *mfd, int on)
{
	mutex_lock(&mfd->dma->ov_mutex);
	if ((mfd->op_enable != 0) && (mfd->panel_power_on != 0)) {
		pr_debug("%s: setting tear, on = %d\n", __func__, on);
		mipi_set_tx_power_mode(0);
		if (on)
			mipi_mot_tx_cmds(&mot_display_set_tear_on_cmds[0],
				ARRAY_SIZE(mot_display_set_tear_on_cmds));
		else
			mipi_mot_tx_cmds(&mot_display_set_tear_off_cmds[0],
				ARRAY_SIZE(mot_display_set_tear_off_cmds));
	}
	mutex_unlock(&mfd->dma->ov_mutex);
}

static void __init dump_mipi_dsi_phy_ctrl(struct mipi_dsi_phy_ctrl *phy_db)
{
	int i;

	pr_debug("mipi_dsi_phy_ctrl data dump:\n");

	for (i = 0; i < 5; i++)
		pr_debug("regulator[%d]: 0x%08x ", i, phy_db->regulator[i]);

	for (i = 0; i < 12; i++)
		pr_debug("timing[%d]: 0x%08x ", i, phy_db->timing[i]);

	for (i = 0; i < 4; i++)
		pr_debug("ctrl[%d]: 0x%08x ", i, phy_db->ctrl[i]);

	for (i = 0; i < 4; i++)
		pr_debug("strength[%d]: 0x%08x ", i, phy_db->strength[i]);

	for (i = 0; i < 20; i++)
		pr_debug("pll[%d]: 0x%08x ", i, phy_db->pll[i]);
}

int __init mipi_mot_reconfig_dsiphy_db(struct msm_panel_info *pinfo,
				struct mipi_dsi_phy_ctrl *phy_db,
				struct mipi_dsi_clk_config *clk_config_tbl)
{
	int ret = -1;
	int idx;
	int dsi_clk = -1;
	struct mipi_dsi_clk_config *clk_db = NULL;

	if (!mot_panel->pdata || !mot_panel->pdata->get_dsi_clk_rate) {
		pr_err("%s: not able to get desired dsi_freq.\n", __func__);
		goto out;
	}

	dsi_clk = mot_panel->pdata->get_dsi_clk_rate();
	if (dsi_clk <= 0) {
		pr_err("%s: invalid desired dsi_clk %d.\n",
			   __func__, dsi_clk);
		ret = dsi_clk;
		goto out;
	}

	for (idx = 0; idx < MIPI_DSI_CLK_MAX_NR; idx++) {
		clk_db = clk_config_tbl + idx;
		if (clk_db->clk_rate == dsi_clk) {
			int i;
			/* timing registers overwritting */
			for (i = 0; i < 11; i++)
				phy_db->timing[i] = clk_db->timing[i];

			/* PLL registers overwritting */
			for (i = 0; i < 3; i++)
				phy_db->pll[i+1] = clk_db->pll[i];

			pinfo->clk_rate = dsi_clk;
			dump_mipi_dsi_phy_ctrl(phy_db);
			ret = dsi_clk;
			goto out;
		}
	}

	if (idx >= MIPI_DSI_CLK_MAX_NR) {
		ret = -2;
		pr_err("%s: no matching config for dsi clk %d\n",
		       __func__, dsi_clk);
	}

out:
	return ret;
}

int __init moto_panel_debug_init(void)
{
#ifdef CONFIG_DEBUG_FS
	struct dentry *panel_root;

	panel_root = debugfs_create_dir("panel", NULL);
	if (!IS_ERR(panel_root))
		debugfs_create_file("gamma", S_IFREG | S_IRUGO, panel_root,
				NULL, &panel_gamma_debug_fops);
	else
		pr_err("Moto panel debugfs couldn't be created.\n");
#endif
	return 0;
}

int is_aod_supported(struct msm_fb_data_type *mfd)
{
	return mfd && mfd->is_partial_mode_supported &&
		mfd->is_partial_mode_supported();
}

int correct_shift_for_aod(struct msm_fb_data_type *mfd, int x2, int y2)
{
	/* Workaround window size is a product of testing and verification */
	struct mipi_mot_panel *mot_panel = mipi_mot_get_mot_panel();
	struct msm_panel_info *pinfo = &mot_panel->pinfo;
	int x1 = x2 - 7;
	int y1 = y2 - 1;
	char col[] = {0x2a, 0x00, 0x00, 0x00, 0x00};
	char row[] = {0x2b, 0x00, 0x00, 0x00, 0x00};
	char frame[] = {
		0x2c,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00
	};
	struct mipi_mot_cmd_seq correct_shift_seq[] = {
		MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, col),
		MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, row),
		MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, frame),
	};

	if (x1 < 0 || y1 < 0 || x2 >= pinfo->xres || y2 >= pinfo->yres) {
		pr_warn("%s: Invalid x,y coordinates (%d, %d)\n",
			__func__, x2, y2);
		return -EINVAL;
	}

	col[1] = x1 >> 8;
	col[2] = x1 & 0xFF;
	col[3] = x2 >> 8;
	col[4] = x2 & 0xFF;
	row[1] = y1 >> 8;
	row[2] = y1 & 0xFF;
	row[3] = y2 >> 8;
	row[4] = y2 & 0xFF;

	pr_debug("%s: cmd 2ah[h:0x%X l:0x%X -> h:0x%X l:0x%X]\n", __func__,
		col[1], col[2], col[3], col[4]);
	pr_debug("%s: cmd 2bh[h:0x%X l:0x%X -> h:0x%X l:0x%X]\n", __func__,
		row[1], row[2], row[3], row[4]);

	return mipi_mot_exec_cmd_seq(mfd, correct_shift_seq,
		ARRAY_SIZE(correct_shift_seq));
}

int mipi_mot_set_partial_window(struct msm_fb_data_type *mfd,
	int x, int y, int w, int h)
{
	struct mipi_mot_panel *mot_panel = mipi_mot_get_mot_panel();
	struct msm_panel_info *pinfo;
	char col[] = {0x2a, 0x00, 0x00, 0x00, 0x00};
	char row[] = {0x2b, 0x00, 0x00, 0x00, 0x00};
	struct mipi_mot_cmd_seq partial_window_seq[] = {
		MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, col),
		MIPI_MOT_TX_DEF(NULL, DTYPE_DCS_LWRITE, 0, row),
	};

	pr_debug("%s: (%d, %d, %d, %d)\n", __func__, x, y, w, h);

	pinfo = &mot_panel->pinfo;
	if (x < 0 ||
	    (x + w) > pinfo->xres ||
	    y < 0 ||
	    (y + h) > pinfo->yres) {
		pr_err("%s: Invalid parameters!\n", __func__);
		return -EINVAL;
	}

	if (mot_panel->is_correct_shift_for_aod_needed &&
		mot_panel->is_correct_shift_for_aod_needed(mfd))
		correct_shift_for_aod(mfd, x + w - 1, y + h - 1);

	col[1] = x >> 8;
	col[2] = x & 0xFF;
	col[3] = (x + w - 1) >> 8;
	col[4] = (x + w - 1) & 0xFF;
	row[1] = y >> 8;
	row[2] = y & 0xFF;
	row[3] = (y + h - 1) >> 8;
	row[4] = (y + h - 1) & 0xFF;
	pr_debug("%s: cmd 2ah[h:0x%X l:0x%X -> h:0x%X l:0x%X]\n", __func__,
		col[1], col[2], col[3], col[4]);
	pr_debug("%s: cmd 2bh[h:0x%X l:0x%X -> h:0x%X l:0x%X]\n", __func__,
		row[1], row[2], row[3], row[4]);

	return mipi_mot_exec_cmd_seq(mfd, partial_window_seq,
		ARRAY_SIZE(partial_window_seq));
}
