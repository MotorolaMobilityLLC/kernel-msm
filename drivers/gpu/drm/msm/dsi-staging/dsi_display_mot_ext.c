/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt)	"msm-dsi-display:[%s] " fmt, __func__

#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/random.h>

#include <linux/string.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/firmware.h>

#include "msm_drv.h"
#include "sde_connector.h"
#include "msm_mmu.h"
#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_ctrl.h"
#include "dsi_ctrl_hw.h"
#include "dsi_drm.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "sde_dbg.h"
#include "dsi_parser.h"
#include "dsi_display_mot_ext.h"

static struct dsi_mot_ext_feature *g_dsi_mot_ext = NULL;
static int g_early_power_count = 0;
//Timer for pressure test and debug
static static struct alarm *g_wakeup_timer = NULL;
static int g_wakeup_timer_interval = 0;
static int g_early_power_test_en = 0;

extern const char *cmd_set_prop_map[DSI_CMD_SET_MAX];

static void dsi_display_early_power_on_work(struct work_struct *work)
{
	struct dsi_display *display = NULL;
	struct dsi_mot_ext_feature *mot_ext = NULL;
	int rc = 0;

	pr_info("+++++ early_power_state: %d\n", (g_dsi_mot_ext!=NULL)?g_dsi_mot_ext->early_power_state:-1);
	mot_ext = container_of(work, struct dsi_mot_ext_feature, early_on_work);
	if (!mot_ext) {
		pr_warning("failed to get mot_ext\n");
		return;
	}
	__pm_wakeup_event(&mot_ext->early_wake_src, 900);
	display = mot_ext->display;
	if (!display || !display->panel || !display->is_dsi_mot_primary ||
	    (display->panel->panel_mode != DSI_OP_VIDEO_MODE) ||
	    atomic_read(&display->panel->esd_recovery_pending)) {
		pr_warning("----- Invalid recovery use case, early_power_state: %d\n", mot_ext->early_power_state);
		__pm_relax(&mot_ext->early_wake_src);
		return;
	}

	mutex_lock(&display->display_lock);
	if (mot_ext->early_power_state == DSI_EARLY_POWER_FORBIDDEN || display->is_dsi_display_prepared) {
		pr_warning("----- already prepared or set_mode called, early_power_state: %d, is_dsi_display_prepared %d\n",
			mot_ext->early_power_state, display->is_dsi_display_prepared);
		mutex_unlock(&display->display_lock);
		__pm_relax(&mot_ext->early_wake_src);
		return;
	}
	mot_ext->early_power_state = DSI_EARLY_POWER_BEGIN;
	mutex_unlock(&display->display_lock);

	rc = dsi_display_prepare(display);
	if (rc) {
		pr_err("----- DSI display prepare failed, rc=%d. early_power_state %d\n", rc, mot_ext->early_power_state);
		__pm_relax(&mot_ext->early_wake_src);
		return;
	}

	mutex_lock(&display->display_lock);
	if (mot_ext->early_power_state == DSI_EARLY_POWER_FORBIDDEN || display->panel->panel_initialized) {
		mutex_unlock(&display->display_lock);
		pr_warning("----- already enabled or set_mode called, early_power_state: %d, panel_initialized %d\n",
			mot_ext->early_power_state, display->panel->panel_initialized);
		__pm_relax(&mot_ext->early_wake_src);
		return;
	}
	mot_ext->early_power_state = DSI_EARLY_POWER_PREPARED;
	mutex_unlock(&display->display_lock);

	rc = dsi_display_enable(display);
	if (rc) {
		pr_err("DSI display enable failed, rc=%d. early_power_state %d\n", rc, mot_ext->early_power_state);
		(void)dsi_display_unprepare(display);
		return;
	}

	mutex_lock(&display->display_lock);
	if (mot_ext->early_power_state != DSI_EARLY_POWER_FORBIDDEN) {
		mot_ext->early_power_state = DSI_EARLY_POWER_INITIALIZED;
		queue_delayed_work(mot_ext->early_power_workq, &mot_ext->early_off_work, HZ/2);
	}
	mutex_unlock(&display->display_lock);

	//Do not relax wake_lock here to avoid AP enter deep suspend before early_off called, which may cause kernel panic.
	//__pm_relax(&mot_ext->early_wake_src);

	pr_info("----- early_power_state: %d, g_early_power_count %d\n", mot_ext->early_power_state, g_early_power_count++);
}

static void dsi_display_early_power_off_work(struct work_struct *work)
{
	struct dsi_display *display = NULL;
	struct dsi_mot_ext_feature *mot_ext = NULL;
	int rc = 0;
	struct delayed_work *dw = to_delayed_work(work);

	pr_info("+++++ early_power_state: %d\n", (g_dsi_mot_ext!=NULL)?g_dsi_mot_ext->early_power_state:-1);
	mot_ext = container_of(dw, struct dsi_mot_ext_feature, early_off_work);
	if (!mot_ext) {
		pr_warning("failed to get mot_ext\n");
		return;
	}
	__pm_wakeup_event(&mot_ext->early_wake_src, 500);
	display = mot_ext->display;
	if (!display || !display->panel ||
	    (display->panel->panel_mode != DSI_OP_VIDEO_MODE) ||
	    atomic_read(&display->panel->esd_recovery_pending)) {
		pr_warning("Invalid recovery use case, early_power_state: %d\n", mot_ext->early_power_state);
		__pm_relax(&mot_ext->early_wake_src);
		return;
	}
	if (mot_ext->early_power_state != DSI_EARLY_POWER_INITIALIZED) {
		pr_info("%s ----- no need, return.  early_power_state %d\n", __func__, mot_ext->early_power_state);
		//mot_ext->early_power_state = DSI_EARLY_POWER_IDLE;
		__pm_relax(&mot_ext->early_wake_src);
		return;
	}

	rc = dsi_display_disable(display);
	if (rc) {
		pr_err("DSI display disable failed, rc=%d. early_power_state %d\n", rc, mot_ext->early_power_state);
		__pm_relax(&mot_ext->early_wake_src);
		return;
	}

	mutex_lock(&display->display_lock);
	if (mot_ext->early_power_state == DSI_EARLY_POWER_FORBIDDEN) {
		pr_warning("----- set_mode called, early_power_state: %d, is_dsi_display_prepared %d\n",
			mot_ext->early_power_state, display->is_dsi_display_prepared);
		mutex_unlock(&display->display_lock);
		__pm_relax(&mot_ext->early_wake_src);
		return;
	}
	mutex_unlock(&display->display_lock);

	rc = dsi_display_unprepare(display);
	if (rc) {
		pr_err("DSI display unprepare failed, rc=%d. early_power_state %d\n", rc, mot_ext->early_power_state);
		__pm_relax(&mot_ext->early_wake_src);
		return;
	}

	mutex_lock(&display->display_lock);
	if (mot_ext->early_power_state != DSI_EARLY_POWER_FORBIDDEN) {
		g_dsi_mot_ext->early_power_state = DSI_EARLY_POWER_IDLE;
	}
	mutex_unlock(&display->display_lock);

	__pm_relax(&mot_ext->early_wake_src);

	pr_info("----- early_power_state: %d\n", mot_ext->early_power_state);
}

void ext_dsi_display_early_power_on(void)
{
	// If previous work has not finished, skip
	if (g_dsi_mot_ext == NULL || g_dsi_mot_ext->early_power_state == DSI_EARLY_POWER_FORBIDDEN) {
		//pr_info("==== g_dsi_mot_ext %p, early_power_state: %d\n",
		//	g_dsi_mot_ext, g_dsi_mot_ext->early_power_state);
		return;
	}
        if (g_dsi_mot_ext->early_power_state != DSI_EARLY_POWER_IDLE) {
		//pr_info("==== early_power_state: %d is not IDLE\n",
		//	g_dsi_mot_ext->early_power_state);
		return;
        }

	//pr_info("#### display(%p), early_power_state: %d\n", g_dsi_mot_ext->display, g_dsi_mot_ext->early_power_state);
	queue_work(g_dsi_mot_ext->early_power_workq, &g_dsi_mot_ext->early_on_work);
}
EXPORT_SYMBOL(ext_dsi_display_early_power_on);

static enum alarmtimer_restart dsi_display_wakeup_timer_func(struct alarm *alarm, ktime_t now)
{
	ktime_t start, add;
	int randomTime = 0;
	pr_info("g_wakeup_timer_interval %d, g_early_power_test_en %d\n", g_wakeup_timer_interval, g_early_power_test_en);
	if (g_early_power_test_en)
		ext_dsi_display_early_power_on();

	if (g_wakeup_timer_interval != 0) {
		//srand(time(NULL));
		start = ktime_get_boottime();
		randomTime = prandom_u32_max(g_wakeup_timer_interval) + g_wakeup_timer_interval;
		add = ktime_set(randomTime, 0);
		alarm_start(g_wakeup_timer, ktime_add(start, add));
		pr_info("%s: randomTimer %d seconds set\n", __func__, randomTime);
	}

	return ALARMTIMER_NORESTART;
}

static int my_atoi(const char *src)
{
      int s = 0;
      bool isMinus = false;

      while(*src == ' ')
      {
          src++;
      }

      if(*src == '+' || *src == '-')
      {
          if(*src == '-')
          {
              isMinus = true;
          }
          src++;
      }
      else if(*src < '0' || *src > '9')
      {
          s = 2147483647;
          return s;
      }

      while(*src != '\0' && *src >= '0' && *src <= '9')
      {
          s = s * 10 + *src - '0';
          src++;
      }
      return s * (isMinus ? -1 : 1);
}

static u32 atoh(const unsigned char *in)
{
	u32 sum = 0;
	unsigned int mult = 1;
	unsigned char c;
	unsigned int len = strlen(in);

	while (len) {
		int value;

		c = in[len - 1];
		value = hex_to_bin(c);
		if (value >= 0)
			sum += mult * value;
		mult *= 16;
		--len;
	}
	return sum;
}

static char *strtok(char *s, const char * sep)
{
	static char *p;
	if (!s && !(s = p)) return NULL;
	s += strspn(s, sep);
	if (!*s) return p = 0;
	p = s + strcspn(s, sep);
	if (*p) *p++ = 0;
	else p = 0;
	return s;
}

static bool isNumStr(char *s)
{
	int i;
	int len;
	char* pbuf = s;

	len = strlen(pbuf);
	while(i < len) {
		if(!((pbuf[i]>= '0' && pbuf[i] <= '9' ) || (pbuf[i] >= 'a' && pbuf[i] <= 'f' ) || (pbuf[i] >= 'A' && pbuf[i] <= 'F' )))
			return false;
		i++;
	}
	return true;
}

static char* dsi_panel_line_remove_front_space(char* pinstr)
{
	char* pbuf = NULL;
	int i = 0;
	int len = strlen(pinstr);
	pbuf = pinstr;
	while(i < len) {
		if( pbuf[i]  !=  0x20 && pbuf[i]  !=  0x09 )  //skip space and horizontal tab
			break;
		i++;
	}
	//pr_info("strlen %d\n", len);
	return pbuf + i;
}

static int dsi_panel_get_cmd_pkt_count(const char *data, u32 length, u32 *cnt)
{
	const u32 cmd_set_min_size = 7;
	u32 count = 0;
	u32 packet_length;
	u32 tmp;
	char* pbuf = NULL;
	char tembuf[8];
	int i;

	pbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	while (length >= cmd_set_min_size) {
		packet_length = cmd_set_min_size;
		tmp = ((data[5] << 8) | (data[6]));
		packet_length += tmp;
		if (packet_length > length) {
			pr_err("format error\n");
			return -EINVAL;
		}
#if 1 //print data
		if (pbuf) {
			snprintf(pbuf, PAGE_SIZE, "packet_data:   %2x  %2x  %2x  %2x  %2x   %2x  %2x  ",
				data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
		}
		for (i=0; i<tmp; i++) {
			memset(tembuf, 0, 8);
			snprintf(tembuf, 8, " %2x ", data[cmd_set_min_size+i]);
			strcat(pbuf, tembuf);
		}
		strcat(pbuf, "\n");
		printk("%s", pbuf);
#endif
		length -= packet_length;
		data += packet_length;
		count++;
	};

	*cnt = count;
	if (pbuf)
		kfree(pbuf);
	return 0;
}

static void dsi_panel_destroy_cmd_packets(struct dsi_panel_cmd_set *set)
{
	u32 i = 0;
	struct dsi_cmd_desc *cmd;

	for (i = 0; i < set->count; i++) {
		cmd = &set->cmds[i];
		kfree(cmd->msg.tx_buf);
	}
}

static void dsi_panel_dealloc_cmd_packets(struct dsi_panel_cmd_set *set)
{
	kfree(set->cmds);
}

static int dsi_panel_create_cmd_packets(const char *data,
					u32 length,
					u32 count,
					struct dsi_cmd_desc *cmd)
{
	int rc = 0;
	int i, j;
	u8 *payload;

	for (i = 0; i < count; i++) {
		u32 size;

		cmd[i].msg.type = data[0];
		cmd[i].last_command = (data[1] == 1 ? true : false);
		cmd[i].msg.channel = data[2];
		cmd[i].msg.flags |= (data[3] == 1 ? MIPI_DSI_MSG_REQ_ACK : 0);
		cmd[i].msg.ctrl = 0;
		cmd[i].post_wait_ms = cmd[i].msg.wait_ms = data[4];
		cmd[i].msg.tx_len = ((data[5] << 8) | (data[6]));

		size = cmd[i].msg.tx_len * sizeof(u8);

		payload = kzalloc(size, GFP_KERNEL);
		if (!payload) {
			rc = -ENOMEM;
			goto error_free_payloads;
		}

		for (j = 0; j < cmd[i].msg.tx_len; j++)
			payload[j] = data[7 + j];

		cmd[i].msg.tx_buf = payload;
		data += (7 + cmd[i].msg.tx_len);
	}

	return rc;
error_free_payloads:
	for (i = i - 1; i >= 0; i--) {
		cmd--;
		kfree(cmd->msg.tx_buf);
	}

	return rc;
}

static int dsi_panel_alloc_cmd_packets(struct dsi_panel_cmd_set *cmd,
					u32 packet_count)
{
	u32 size;

	size = packet_count * sizeof(*cmd->cmds);
	cmd->cmds = kzalloc(size, GFP_KERNEL);
	if (!cmd->cmds)
		return -ENOMEM;

	cmd->count = packet_count;
	return 0;
}

static bool dsi_panel_mot_parse_commands(char* str, u32* length, struct dsi_display_mode_priv_info *priv_info, enum dsi_cmd_set_type type, char* pcur)
{
	char * pos = NULL;
	char * posStart = NULL;
	char * posEnd = NULL;
	char *buf = NULL;  //dont change this point after alloc, else kfree will error and kernel panic
	char *pbuf = NULL;
	char *pLine = NULL;
	char *pLineEnd = NULL;
	char *token = NULL;
	char numstr[8];
	uint len = 0;
	uint cmdlen = 0;
	u8* data = NULL;
	uint index = 0;
	char delim[] = " \t";  //space and horizontal tab

	//pr_info("input str : %s\n", str);
	*length = 0;
	pos = strstr(str, "=");
	if (pos) {
		posStart = strstr(pos, "[");
		if (posStart) {
			posStart ++;
			posEnd = strstr(posStart, "]");
			if (posEnd) {
				pos = strstr(posEnd, "\n");
				if (pos)
					posEnd = pos;
				*length = posEnd - pcur + 1;
			}
			if (posEnd) {
				pr_info("posStart address(%llx), val: posStart[0][1][2]=[%2x][%2x][%2x],   posEnd address(%llx), val: posEnd[0][1][2]=[%2x][%2x][%2x]\n",
					posStart, posStart[0], posStart[1], posStart[2], posEnd, posEnd[0], posEnd[1], posEnd[2]);
				len = posEnd - posStart - 1;
				//*length = len - 1;
				buf = kmalloc(len+5, GFP_KERNEL);	//data in this buffer can be modified, if there is "\n", we also copy it to buf
				if (!buf) {
					pr_warn("kmalloc failed for command buf\n");
					goto error_exit;
				}
				pbuf = buf;
				memset(pbuf, 0, len+5);
				//strncpy(pbuf, posStart, len);
				memcpy(pbuf, posStart, len);
				//copy_from_user(pbuf, posStart, len);
				//pbuf[len] = '\0';
				data = (u8*)pbuf;
				posEnd = pbuf + len;
				memset(numstr, 0, 8);
				pr_info("bufStart address(%llx), val: bufStart[0][1][2]=[%2x][%2x][%2x],   bufEnd address(%llx), val: bufEnd[0][1][2]=[%2x][%2x][%2x]\n",
					pbuf, pbuf[0], pbuf[1], pbuf[2], posEnd, posEnd[0], posEnd[1], posEnd[2]);
				pr_info("command str : len %d\n", len);
				pLine = pbuf;
				pLineEnd = posEnd;
				token = pLine;

				while (pbuf < posEnd) {

					pLineEnd = strstr(pLine, "\n");
					if (pLineEnd) {
						pLineEnd--;  //If this is a '\n' only line, ptr need --
					} else
						pLineEnd = posEnd;

					pLineEnd[0] = '\0'; //add terminal flag and remove '\n' in the str
					pLineEnd[1] = '\0';
					token = strstr(pLine, "]");
					if (!token) {
						token = strstr(pLine, "//");
						if (!token)
							token = strstr(pLine, "/*");
					}
					if (token) {
						pr_info("there is comment '/*' in pLine address: %llx, token[0]=%c, pLine str: %s\n", pLine, token[0], pLine);
						token[0] = '\0';
					} else {
						token = pLineEnd;
					}
					//pr_info("#pre pLine address: %llx, pLine[0]=%x, pLine str: %s\n", pLine, pLine[0], pLine);
					pLine = dsi_panel_line_remove_front_space(pLine);
					pr_info("=pst pLine address: %llx, pLine[0]=%x, pLine str: %s\n", pLine, pLine[0], pLine);
					//pr_info("token address:     %llx,      token[0][1][2]=[%2x][%2x][%2x]\n", token, token[0], token[1], token[2]);
					//pr_info("pLineEnd address: %llx, pLineEnd[0][1][2]=[%2x][%2x][%2x]\n", pLineEnd, pLineEnd[0], pLineEnd[1], pLineEnd[2]);
					pLineEnd += 2;
					if (pLine != token) {
						for(token = strtok(pLine, delim); token != NULL; token = strtok(NULL, delim)) {
							if (isNumStr(token)) {
								data[index++] = atoh(token);
								//pr_info("find num str: %s -> %2x\n", token, data[index-1]);
							}
						}
					} else
						pr_info("nothing line: %llx, pLine str: %s\n", pLine, pLine);

					pbuf = pLineEnd ;
					pLine = pLineEnd;

				}

				dsi_panel_get_cmd_pkt_count(data, index, &cmdlen);
				pr_info("got command packet num : %d\n", cmdlen);
				if (type < DSI_CMD_SET_MAX) {
					dsi_panel_destroy_cmd_packets(&priv_info->cmd_sets[type]);
					dsi_panel_dealloc_cmd_packets(&priv_info->cmd_sets[type]);
					dsi_panel_alloc_cmd_packets(&priv_info->cmd_sets[type], cmdlen);
					dsi_panel_create_cmd_packets(data, 0, cmdlen, priv_info->cmd_sets[type].cmds);
				}

			} else {
				pr_err(" ] not found in this line!\n");
				goto error_exit;
			}
		} else {
			pr_err(" [ not found in this line\n");
			goto error_exit;
		}
	} else {
		pr_err(" = not found in this line!\n");
		goto error_exit;
	}

error_exit:
	if (buf)
		kfree(buf);
	//pr_info("need skip length : %d\n", *length);
	return true;
}

static bool dsi_panel_mot_parse_u32(char* str, u32* val)
{
	char * pos = NULL;
	char * posStart = NULL;
	char * posEnd = NULL;
	char buf[64];
	uint len = 0;
	posStart = strstr(str, "=");
	if (posStart) {
		posStart = strstr(posStart, "<");
		if (posStart) {
			posStart ++;
			posEnd = strstr(posStart, ">");
			if (posEnd) {
				pos = strstr(posStart, " ");
				while(pos != NULL) {
					if (pos == posStart)
						posStart++;
					else if (pos > posStart) {
						posEnd = pos;
						break;
					}
					pos = strstr(posStart, " ");
				}
				len = posEnd - posStart;
				strncpy(buf, posStart, len);
				buf[len] = 0;
				//pr_info("val str : %s\n", buf);
				*val = my_atoi(buf);
			} else {
				pr_err(" > not found in this line!\n");
				goto error_exit;
			}
		} else {
			pr_err(" < not found in this line, please try find [\n");
			goto error_exit;
		}
	} else {
		pr_err(" = not found in this line!\n");
		goto error_exit;
	}
	return true;

error_exit:
	return false;
}

void dsi_panel_mot_parse_timing_from_file(struct dsi_display *display, int index)
{
	int rc = 0;
	u32 val = 0;
	u32 keylen, llen, cmdstrlen;
	struct dsi_display_mode *display_mode;
	struct dsi_display_mode_priv_info *priv_info;
	char * fwbuf = NULL;
	char * pbuf = NULL;
	char * pline = NULL;
	char * plinebuf = NULL;
	char * plineEnd = NULL;
	const struct firmware *fw = NULL;
	struct dsi_mode_info *mode_timing = NULL ;

	if (!display) {
		pr_warn("display is null\n");
		return;
	}

	mode_timing = &display->modes[index].timing;
	pr_info(", index: %d !\n", index);
	pr_info("panel horz active:%d front_portch:%d back_porch:%d pulse_width:%d h_skew:%d\n",
		mode_timing->h_active, mode_timing->h_front_porch, mode_timing->h_back_porch, 	mode_timing->h_sync_width, mode_timing->h_skew);
	pr_info("panel vert active:%d front_portch:%d back_porch:%d pulse_width:%d\n",
		mode_timing->v_active, mode_timing->v_front_porch, mode_timing->v_back_porch, 	mode_timing->v_sync_width);

	display_mode = &display->modes[index];//container_of(mode_timing, struct dsi_display_mode, timing);
	priv_info = display->modes[index].priv_info;
	pr_info("panel clk rate:%d mdp_transfer_time_us:%d refresh_rate:%d\n",
		display_mode->priv_info->clk_rate_hz , display_mode->priv_info->mdp_transfer_time_us, mode_timing->refresh_rate);

	rc = request_firmware(&fw, "lcd-paras.txt", &display->pdev->dev);//
	if (rc < 0) {
		dev_warn_once(&display->pdev->dev, "Request firmware failed - /sdcard/lcd-paras.txt (%d)\n", rc);
		return;
	}
	pr_info("found LCD para debug file, length: %d !\n", fw->size);

	if (fw->size < 8 || fw->size > 65536) {
		dev_warn_once(&display->pdev->dev, "Invalid firmware size (%zu)\n", fw->size);
	}

	fwbuf = kmalloc(fw->size+1, GFP_KERNEL);
	if (!fwbuf) {
		pr_warn("kmalloc failed for fwbuf\n");
		return;
	}
	memset(fwbuf, 0, fw->size + 1);
	memcpy(fwbuf, fw->data, fw->size);
	pbuf = fwbuf;

	plinebuf = kmalloc(LCD_PARA_LINE_LEN, GFP_KERNEL);
	memset(plinebuf, 0, LCD_PARA_LINE_LEN);
	if (!plinebuf) {
		pr_warn("kmalloc failed for pline\n");
		goto timing_err_exit;
		return;
	}

	plineEnd = strstr(pbuf, "\n");
	if (plineEnd) {
		llen = plineEnd - pbuf;
		if (llen < LCD_PARA_LINE_LEN) {
			//strncpy(plinebuf, pbuf, llen);
			memcpy(plinebuf, pbuf, llen);
			plinebuf[llen] = '\0';
			pr_info("First line pre:  pbuf address(%llx), len%d, plinebuf[0]=%2x, plinebuf str: %s\n", pbuf, llen, plinebuf[0], plinebuf);
			pline = dsi_panel_line_remove_front_space(plinebuf);
			if (pline >= plinebuf)
				llen -= (pline - plinebuf);
			else {
				pr_warn("error line a\n");
				goto timing_err_exit;
			}
			pr_info("First line pst:  pbuf address(%llx), len%d, pline[0]=%2x, pline str: %s\n", pbuf, llen, pline[0], pline);
			pbuf = plineEnd + 1;
		} else {
			pr_warn("too large line a\n");
			goto timing_err_exit;
		}
	} else {
		pr_warn("found no line\n");
		goto timing_err_exit;
	}

	while (pbuf < (fwbuf + fw->size)) {

		if ( strncmp(pline, "//", 2)  == 0) {
			//go relaign pbuf
			goto goContinue;
		}
		if ( strncmp(pline, "/*", 2)  == 0) {
			//go relaign pbuf
			goto goContinue;
		}

		// qcom,mdss-dsi-panel-clockrate
		keylen = strlen("qcom,mdss-dsi-panel-clockrate");
		if ( strncmp(pline, "qcom,mdss-dsi-panel-clockrate", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-panel-clockrate: %d\n", val);
				mode_timing->clk_rate_hz = val;
				display_mode->priv_info->clk_rate_hz = mode_timing->clk_rate_hz;
			}
		}

		// qcom,mdss-mdp-transfer-time-us
		keylen = strlen("qcom,mdss-mdp-transfer-time-us");
		if ( strncmp(pline, "qcom,mdss-mdp-transfer-time-us", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-mdp-transfer-time-us: %d\n", val);
				mode_timing->mdp_transfer_time_us = val;
				display_mode->priv_info->mdp_transfer_time_us =	mode_timing->mdp_transfer_time_us;
			}
		}

		// qcom,mdss-dsi-panel-framerate
		keylen = strlen("qcom,mdss-dsi-panel-framerate");
		if ( strncmp(pline, "qcom,mdss-dsi-panel-framerate", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-panel-framerate: %d\n", val);
				mode_timing->refresh_rate = val;
			}
		}

		// qcom,mdss-dsi-panel-width
		keylen = strlen("qcom,mdss-dsi-panel-width");
		if ( strncmp(pline, "qcom,mdss-dsi-panel-width", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-panel-width: %d\n", val);
				mode_timing->h_active = val;
			}
		}

		// qcom,mdss-dsi-h-front-porch
		keylen = strlen("qcom,mdss-dsi-h-front-porch");
		if ( strncmp(pline, "qcom,mdss-dsi-h-front-porch", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-h-front-porch: %d\n", val);
				mode_timing->h_front_porch = val;
			}
		}

		// qcom,mdss-dsi-h-back-porch
		keylen = strlen("qcom,mdss-dsi-h-back-porch");
		if ( strncmp(pline, "qcom,mdss-dsi-h-back-porch", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-h-back-porch: %d\n", val);
				mode_timing->h_back_porch = val;
			}
		}

		// qcom,mdss-dsi-h-pulse-width
		keylen = strlen("qcom,mdss-dsi-h-pulse-width");
		if ( strncmp(pline, "qcom,mdss-dsi-h-pulse-width", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-h-pulse-width: %d\n", val);
				mode_timing->h_sync_width = val;
			}
		}

		// qcom,mdss-dsi-h-sync-skew
		keylen = strlen("qcom,mdss-dsi-h-sync-skew");
		if ( strncmp(pline, "qcom,mdss-dsi-h-sync-skew", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-h-sync-skew: %d\n", val);
				mode_timing->h_skew = val;
			}
		}

		// qcom,mdss-dsi-panel-height
		keylen = strlen("qcom,mdss-dsi-panel-height");
		if ( strncmp(pline, "qcom,mdss-dsi-panel-height", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-panel-height: %d\n", val);
				mode_timing->v_active = val;
			}
		}

		// qcom,mdss-dsi-v-back-porch
		keylen = strlen("qcom,mdss-dsi-v-back-porch");
		if ( strncmp(pline, "qcom,mdss-dsi-v-back-porch", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-v-back-porch: %d\n", val);
				mode_timing->v_back_porch = val;
			}
		}

		// qcom,mdss-dsi-v-front-porch
		keylen = strlen("qcom,mdss-dsi-v-front-porch");
		if ( strncmp(pline, "qcom,mdss-dsi-v-front-porch", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-v-front-porch: %d\n", val);
				mode_timing->v_front_porch = val;
			}
		}

		// qcom,mdss-dsi-v-pulse-width
		keylen = strlen("qcom,mdss-dsi-v-pulse-width");
		if ( strncmp(pline, "qcom,mdss-dsi-v-pulse-width", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-v-pulse-width: %d\n", val);
				mode_timing->v_sync_width = val;
			}
		}

		// qcom,mdss-dsi-t-clk-post
		keylen = strlen("qcom,mdss-dsi-t-clk-post");
		if ( strncmp(pline, "qcom,mdss-dsi-t-clk-post", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-t-clk-post: %d\n", val);
				display->panel->host_config.t_clk_post = val;
			}
		}

		// qcom,mdss-dsi-t-clk-pre
		keylen = strlen("qcom,mdss-dsi-t-clk-pre");
		if ( strncmp(pline, "qcom,mdss-dsi-t-clk-pre", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-dsi-t-clk-pre: %d\n", val);
				display->panel->host_config.t_clk_pre = val;
			}
		}

		// qcom,mdss-pan-physical-width-dimension
		keylen = strlen("qcom,mdss-pan-physical-width-dimension");
		if ( strncmp(pline, "qcom,mdss-pan-physical-width-dimension", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-pan-physical-width-dimension: %d\n", val);
				display->panel->phy_props.panel_width_mm = val;
			}
		}

		// qcom,mdss-pan-physical-height-dimension
		keylen = strlen("qcom,mdss-pan-physical-height-dimension");
		if ( strncmp(pline, "qcom,mdss-pan-physical-height-dimension", keylen)  == 0) {
			if (dsi_panel_mot_parse_u32(pline+keylen, &val)) {
				pr_info("got qcom,mdss-pan-physical-height-dimension: %d\n", val);
				display->panel->phy_props.panel_height_mm = val;
			}
		}

		// qcom,mdss-dsi-on-command
		keylen = strlen("qcom,mdss-dsi-on-command");
		if ( strncmp(pline, "qcom,mdss-dsi-on-command", keylen)  == 0) {
			if (dsi_panel_mot_parse_commands(pbuf - llen + keylen -1, &cmdstrlen, priv_info, DSI_CMD_SET_ON, pbuf)) {
				pr_info("got qcom,mdss-dsi-on-command: str lenght: %d\n", cmdstrlen);
				pbuf += cmdstrlen;
			}
		}

		// qcom,mdss-dsi-off-command
		keylen = strlen("qcom,mdss-dsi-off-command");
		if ( strncmp(pline, "qcom,mdss-dsi-off-command", keylen)  == 0) {
			if (dsi_panel_mot_parse_commands(pbuf - llen + keylen -1, &cmdstrlen, priv_info, DSI_CMD_SET_OFF, pbuf)) {
				pr_info("got qcom,mdss-dsi-off-command: lenght: %d\n", cmdstrlen);
				pbuf += cmdstrlen;
			}
		}

goContinue:
		memset(plinebuf, 0, LCD_PARA_LINE_LEN);
		plineEnd = strstr(pbuf, "\n");
		if (plineEnd) {
			llen = plineEnd - pbuf;
			if (llen < LCD_PARA_LINE_LEN) {
				//strncpy(plinebuf, pbuf, llen);
				memcpy(plinebuf, pbuf, llen);
				plinebuf[llen] = '\0';
				//pr_info("New line pre:  pbuf address(%llx), len%d, plinebuf[0]=%2x, plinebuf str: %s\n", pbuf, llen, plinebuf[0], plinebuf);
				pline = dsi_panel_line_remove_front_space(plinebuf);
				if (pline >= plinebuf)
					llen -= (pline - plinebuf);
				else {
					pr_warn("error line b\n");
					goto timing_err_exit;
				}
				if (llen > 7)
					pr_info("New line pst:  pbuf(%llx),  pbuf[0][1][2][3][4][5][6][7]=[%2x][%2x][%2x][%2x][%2x][%2x][%2x][%2x], len %d, pline[0]=%2x, pline str: %s\n",
						pbuf, pbuf[0], pbuf[1], pbuf[2], pbuf[3], pbuf[4], pbuf[5], pbuf[6], pbuf[7], llen, pline[0], pline);
				pbuf = plineEnd + 1;
			} else {
				pr_warn("too large line b\n");
				goto timing_err_exit;
			}
		} else {
			llen = fwbuf + fw->size - pbuf;
			memcpy(plinebuf, pbuf, llen);
			plinebuf[llen] = '\0';
			pr_info("End line:  pbuf(%llx): len%d:  %s\n", pbuf, llen, plinebuf);
			break;
		}

	}

timing_err_exit:
	if (plinebuf)
		kfree(plinebuf);
	if (fwbuf)
		kfree(fwbuf);
	release_firmware(fw);
	return;
}

static ssize_t dsi_display_early_power_read(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct dsi_display *display;

	display = dev_get_drvdata(dev);
	if (!display) {
		pr_err("Invalid display\n");
		return -EINVAL;
	}

	rc = snprintf(buf, PAGE_SIZE, "%d\n", display->dsi_mot_ext.early_power_state);
	pr_info("%s: early_power_state %d\n", __func__,
		display->dsi_mot_ext.early_power_state);

	return rc;
}

static ssize_t dsi_display_early_power_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct dsi_display *display;

	display = dev_get_drvdata(dev);
	if (!display) {
		pr_err("Invalid display\n");
		return count;
	}

	if (display->panel->panel_initialized) {
		pr_err("panel already initialized\n");
		return count;
	}
	pr_info("%s: early_power_state %d\n", __func__,
		display->dsi_mot_ext.early_power_state);

	ext_dsi_display_early_power_on();

	return count;

}

static ssize_t dsi_display_wakup_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc = snprintf(buf, PAGE_SIZE, "%s: timerhandle:%p\n",  (g_wakeup_timer_interval==0)? "Stopped":"Started", g_wakeup_timer);
	pr_info("%s:  %s: timerhandle:%p\n", __func__,  (g_wakeup_timer_interval==0)? "Stopped":"Started", g_wakeup_timer);

	return rc;
}

static ssize_t dsi_display_wakup_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct dsi_display *display;
	ktime_t now, add;
	u16 val = 0;

	display = dev_get_drvdata(dev);
	if (!display) {
		pr_err("Invalid display\n");
		return count;
	}

	if (kstrtou16(buf, 16, &val) < 0)
		return count;
	g_wakeup_timer_interval = val&0x7fff;

	pr_info("val: 0x%x(enable:0x%x : interval:%d seconds), early_power_state %d\n", val, (val&0x8000), g_wakeup_timer_interval,
		display->dsi_mot_ext.early_power_state);

	if (g_wakeup_timer == NULL) {
		g_wakeup_timer = devm_kzalloc(&display->pdev->dev, sizeof(*g_wakeup_timer), GFP_KERNEL);
		if (g_wakeup_timer) {
			alarm_init(g_wakeup_timer, ALARM_BOOTTIME, dsi_display_wakeup_timer_func);
		}
		else {
			pr_err("failed to init timer\n");
			return count;
		}
	}

	// 0xffff :   0x8000/enable/disable    0x7fff/interval seconds
	if (val&0x8000) {
		now = ktime_get_boottime();
		add = ktime_set(g_wakeup_timer_interval, 0);
		alarm_start(g_wakeup_timer, ktime_add(now, add));
	}else {
		if (g_wakeup_timer)
			alarm_cancel(g_wakeup_timer);
		g_wakeup_timer_interval = 0;
	}
	return count;
}

static ssize_t dsi_display_early_test_en_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;

	rc = snprintf(buf, PAGE_SIZE, "g_early_power_test_en: %d\n",   g_early_power_test_en);
	pr_info("g_early_power_test_en:%d\n", g_early_power_test_en);

	return rc;
}

static ssize_t dsi_display_early_test_en_set(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct dsi_display *display;
	u16 val = 0;

	display = dev_get_drvdata(dev);
	if (!display) {
		pr_err("Invalid display\n");
		return count;
	}

	if (kstrtou16(buf, 10, &val) < 0)
		return count;
	g_early_power_test_en = val;

	pr_info("interval:%d seconds, early_power_state %d, g_early_power_test_en %d\n",
		g_wakeup_timer_interval, display->dsi_mot_ext.early_power_state, g_early_power_test_en);

	return count;
}

static void dsi_display_show_para(char* buf, char* pbuf, enum dsi_cmd_set_type type, struct dsi_display_mode_priv_info *priv_info)
{
	int i,j,rc;
	u8* data;
	int count = priv_info->cmd_sets[type].count;

	memset(pbuf, 0, PAGE_SIZE);
	rc = snprintf(pbuf, PAGE_SIZE, "## count:%d ====\n", count);
	strcat(buf, pbuf);
	memset(pbuf, 0, PAGE_SIZE);
	for (i=0; i<count; i++) {
	rc = snprintf(pbuf, PAGE_SIZE, "%2x %2x %2x %2x %2x %2x   %2x ",
			priv_info->cmd_sets[type].cmds[i].msg.type, 		//data0
			priv_info->cmd_sets[type].cmds[i].last_command, 	//data1
			priv_info->cmd_sets[type].cmds[i].msg.channel,	//data2
			priv_info->cmd_sets[type].cmds[i].msg.flags&0x01,	//data3
			priv_info->cmd_sets[type].cmds[i].post_wait_ms,	//data4
			priv_info->cmd_sets[type].cmds[i].msg.tx_len&0xff00,	//data5
			priv_info->cmd_sets[type].cmds[i].msg.tx_len&0x00ff	//data6
			);
		strcat(buf, pbuf);
		data = (u8*)priv_info->cmd_sets[type].cmds[i].msg.tx_buf;
		for (j=0; j<priv_info->cmd_sets[type].cmds[i].msg.tx_len; j++) {
			memset(pbuf, 0, PAGE_SIZE);
			rc = snprintf(pbuf, PAGE_SIZE, " %2x ", data[j]);
			strcat(buf, pbuf);
		}
		strcat(buf, "\n");
	}
}

static ssize_t dsi_display_parse_para_get(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int rc = 0;
	struct dsi_display *display;
	u16 index = 0;
	struct dsi_mode_info *mode_timing = NULL ;
	struct dsi_display_mode *display_mode;
	struct dsi_display_mode_priv_info *priv_info;
	char* pbuf = NULL;

	display = dev_get_drvdata(dev);
	if (!display) {
		pr_err("Invalid display\n");
		return rc;
	}
	display_mode = &display->modes[index];
	priv_info = display->modes[index].priv_info;
	mode_timing = &display->modes[index].timing;

	pbuf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (pbuf) {
		rc = snprintf(buf, PAGE_SIZE, "panel horz active:%d front_portch:%d back_porch:%d pulse_width:%d h_skew:%d\n",
			mode_timing->h_active, mode_timing->h_front_porch, mode_timing->h_back_porch, 	mode_timing->h_sync_width, mode_timing->h_skew);
		memset(pbuf, 0, PAGE_SIZE);
		rc = snprintf(pbuf, PAGE_SIZE, "panel vert active:%d front_portch:%d back_porch:%d pulse_width:%d\n",
			mode_timing->v_active, mode_timing->v_front_porch, mode_timing->v_back_porch, 	mode_timing->v_sync_width);
		strcat(buf, pbuf);
		memset(pbuf, 0, PAGE_SIZE);
		rc = snprintf(pbuf, PAGE_SIZE, "panel clk rate:%d mdp_transfer_time_us:%d refresh_rate:%d\n",
			display_mode->priv_info->clk_rate_hz , display_mode->priv_info->mdp_transfer_time_us, mode_timing->refresh_rate);
		strcat(buf, pbuf);
		dsi_display_show_para(buf, pbuf, DSI_CMD_SET_ON, priv_info);
		dsi_display_show_para(buf, pbuf, DSI_CMD_SET_OFF, priv_info);
		printk("%s\n", buf);

	} else {
		pr_warn("kmalloc failed\n" );
	}

	if (pbuf)
		kfree(pbuf);
	return rc;
}

// val 0: update the paras for the first modes
static ssize_t dsi_display_parse_para_update(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct dsi_display *display;
	u16 index = 0;

	display = dev_get_drvdata(dev);
	if (!display) {
		pr_err("Invalid display\n");
		return count;
	}

	if (kstrtou16(buf, 10, &index) < 0)
		return count;

	dsi_panel_mot_parse_timing_from_file(display, index);

	return count;
}

///sys/devices/platform/soc/soc:qcom,dsi-display/
static DEVICE_ATTR(dsi_display_early_power, 0644,
			dsi_display_early_power_read,
			dsi_display_early_power_write);
static DEVICE_ATTR(dsi_display_wakeup, 0644,
			dsi_display_wakup_get,
			dsi_display_wakup_set);
static DEVICE_ATTR(early_test_en, 0644,
			dsi_display_early_test_en_get,
			dsi_display_early_test_en_set);
static DEVICE_ATTR(dsi_display_parse_para, 0644,
			dsi_display_parse_para_get,
			dsi_display_parse_para_update);


static struct attribute *dsi_display_mot_ext_fs_attrs[] = {
	&dev_attr_dsi_display_early_power.attr,
	&dev_attr_dsi_display_wakeup.attr,
	&dev_attr_early_test_en.attr,
	&dev_attr_dsi_display_parse_para.attr,
	NULL,
};
static struct attribute_group dsi_display_mot_ext_fs_attrs_group = {
	.attrs = dsi_display_mot_ext_fs_attrs,
};

static int dsi_display_sysfs_ext_init(struct dsi_display *display)
{
	int rc = 0;
	struct device *dev = &display->pdev->dev;

	rc = sysfs_create_group(&dev->kobj,
		&dsi_display_mot_ext_fs_attrs_group);

	return rc;
}

static int dsi_display_sysfs_ext_deinit(struct dsi_display *display)
{
	struct device *dev = &display->pdev->dev;

	sysfs_remove_group(&dev->kobj,
		&dsi_display_mot_ext_fs_attrs_group);

	return 0;
}

int dsi_display_ext_init(struct dsi_display *display)
{
	int rc = 0;

	g_dsi_mot_ext = &display->dsi_mot_ext;
	g_dsi_mot_ext->display = display;

	dsi_display_sysfs_ext_init(display);

	display->is_dsi_display_prepared = false;
	display->is_dsi_mot_primary = false;
	g_dsi_mot_ext->early_power_state = DSI_EARLY_POWER_STATE_NUM;
	g_dsi_mot_ext->early_power_workq = create_singlethread_workqueue("dsi_early_power_workq");
	if (!g_dsi_mot_ext->early_power_workq) {
		pr_err("failed to create dsi early power workq!\n");
		dsi_display_sysfs_ext_deinit(display);
		return 0;
	}

	wakeup_source_init(&g_dsi_mot_ext->early_wake_src, "dsi_early_wakelock");

	INIT_WORK(&g_dsi_mot_ext->early_on_work,
				dsi_display_early_power_on_work);
	INIT_DELAYED_WORK(&g_dsi_mot_ext->early_off_work,
				dsi_display_early_power_off_work);
	pr_info("dsi_display_ext_init success\n");

	return rc;
}

