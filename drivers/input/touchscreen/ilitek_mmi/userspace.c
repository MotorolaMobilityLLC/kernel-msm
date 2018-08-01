/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "common.h"
#include "platform.h"
#include "core/config.h"
#include "core/firmware.h"
#include "core/finger_report.h"
#include "core/flash.h"
#include "core/i2c.h"
#include "core/protocol.h"
#include "core/mp_test.h"
#include "core/parser.h"
#include "core/gesture.h"

#define USER_STR_BUFF   2000
#define ILITEK_IOCTL_MAGIC  100
#define ILITEK_IOCTL_MAXNR  21

#define ILITEK_IOCTL_I2C_WRITE_DATA         _IOWR(ILITEK_IOCTL_MAGIC, 0, uint8_t*)
#define ILITEK_IOCTL_I2C_SET_WRITE_LENGTH   _IOWR(ILITEK_IOCTL_MAGIC, 1, int)
#define ILITEK_IOCTL_I2C_READ_DATA          _IOWR(ILITEK_IOCTL_MAGIC, 2, uint8_t*)
#define ILITEK_IOCTL_I2C_SET_READ_LENGTH    _IOWR(ILITEK_IOCTL_MAGIC, 3, int)

#define ILITEK_IOCTL_TP_HW_RESET            _IOWR(ILITEK_IOCTL_MAGIC, 4, int)
#define ILITEK_IOCTL_TP_POWER_SWITCH        _IOWR(ILITEK_IOCTL_MAGIC, 5, int)
#define ILITEK_IOCTL_TP_REPORT_SWITCH       _IOWR(ILITEK_IOCTL_MAGIC, 6, int)
#define ILITEK_IOCTL_TP_IRQ_SWITCH          _IOWR(ILITEK_IOCTL_MAGIC, 7, int)

#define ILITEK_IOCTL_TP_DEBUG_LEVEL         _IOWR(ILITEK_IOCTL_MAGIC, 8, int)
#define ILITEK_IOCTL_TP_FUNC_MODE           _IOWR(ILITEK_IOCTL_MAGIC, 9, int)

#define ILITEK_IOCTL_TP_FW_VER              _IOWR(ILITEK_IOCTL_MAGIC, 10, uint8_t*)
#define ILITEK_IOCTL_TP_PL_VER              _IOWR(ILITEK_IOCTL_MAGIC, 11, uint8_t*)
#define ILITEK_IOCTL_TP_CORE_VER            _IOWR(ILITEK_IOCTL_MAGIC, 12, uint8_t*)
#define ILITEK_IOCTL_TP_DRV_VER             _IOWR(ILITEK_IOCTL_MAGIC, 13, uint8_t*)
#define ILITEK_IOCTL_TP_CHIP_ID             _IOWR(ILITEK_IOCTL_MAGIC, 14, uint32_t*)

#define ILITEK_IOCTL_TP_NETLINK_CTRL        _IOWR(ILITEK_IOCTL_MAGIC, 15, int*)
#define ILITEK_IOCTL_TP_NETLINK_STATUS      _IOWR(ILITEK_IOCTL_MAGIC, 16, int*)

#define ILITEK_IOCTL_TP_MODE_CTRL           _IOWR(ILITEK_IOCTL_MAGIC, 17, uint8_t*)
#define ILITEK_IOCTL_TP_MODE_STATUS         _IOWR(ILITEK_IOCTL_MAGIC, 18, int*)
#define ILITEK_IOCTL_ICE_MODE_SWITCH		_IOWR(ILITEK_IOCTL_MAGIC, 19, int)

#define ILITEK_IOCTL_MP_WRITE_CDC_FLAG_FALSE _IOWR(ILITEK_IOCTL_MAGIC, 20, int*)
#define ILITEK_IOCTL_MP_READ_CDC_FLAG	     _IOWR(ILITEK_IOCTL_MAGIC, 21, int*)

unsigned char *g_user_buf;
static struct class *touchscreen_class;
static struct device *touchscreen_class_dev;

int katoi(char *string)
{
	int result = 0;
	unsigned int digit;
	int sign;

	if (*string == '-') {
		sign = 1;
		string += 1;
	} else {
		sign = 0;
		if (*string == '+') {
			string += 1;
		}
	}

	for (;; string += 1) {
		digit = *string - '0';
		if (digit > 9)
			break;
		result = (10 * result) + digit;
	}

	if (sign) {
		return -result;
	}
	return result;
}

EXPORT_SYMBOL(katoi);

int str2hex(char *str)
{
	int strlen, result, intermed, intermedtop;
	char *s = str;

	while (*s != 0x0) {
		s++;
	}

	strlen = (int)(s - str);
	s = str;
	if (*s != 0x30) {
		return ERROR;
	}

	s++;

	if (*s != 0x78 && *s != 0x58) {
		return ERROR;
	}
	s++;

	strlen = strlen - 3;
	result = 0;
	while (*s != 0x0) {
		intermed = *s & 0x0f;
		intermedtop = *s & 0xf0;
		if (intermedtop == 0x60 || intermedtop == 0x40) {
			intermed += 0x09;
		}
		intermed = intermed << (strlen << 2);
		result = result | intermed;
		strlen -= 1;
		s++;
	}
	return result;
}

EXPORT_SYMBOL(str2hex);

static ssize_t ilitek_proc_debug_switch_read(struct file *pFile,
					     char __user *buff, size_t nCount,
					     loff_t *pPos)
{
	int res = 0;

	if (*pPos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	ipd->debug_node_open = !ipd->debug_node_open;

	ipio_info(" %s debug_flag message = %x\n",
		  ipd->debug_node_open ? "Enabled" : "Disabled",
		  ipd->debug_node_open);

	nCount =
	    snprintf(g_user_buf, USER_STR_BUFF * sizeof(unsigned char), "ipd->debug_node_open : %s\n",
		    ipd->debug_node_open ? "Enabled" : "Disabled");

	*pPos += nCount;

	res = copy_to_user(buff, g_user_buf, nCount);
	if (res < 0) {
		ipio_err("Failed to copy data to user space");
	}

	return nCount;
}

static ssize_t ilitek_proc_debug_message_write(struct file *filp,
					       const char *buff, size_t size,
					       loff_t *pPos)
{
	int ret = 0;
	unsigned char buffer[512] = { 0 };

	/* check the buffer size whether it exceeds the local buffer size or not */
	if (size > 512) {
		ipio_err("buffer exceed 512 bytes\n");
		size = 512;
	}

	ret = copy_from_user(buffer, buff, size - 1);
	if (ret < 0) {
		ipio_err("copy data from user space, failed");
		return ERROR;
	}

	if (strcmp(buffer, "dbg_flag") == 0) {
		ipd->debug_node_open = !ipd->debug_node_open;
		ipio_info(" %s debug_flag message(%X).\n",
			  ipd->debug_node_open ? "Enabled" : "Disabled",
			  ipd->debug_node_open);
	}
	return size;
}

static ssize_t ilitek_proc_debug_message_read(struct file *filp,
					      char __user *buff, size_t size,
					      loff_t *pPos)
{
	unsigned long p = *pPos;
	unsigned int count = size;
	int i = 0;
	int send_data_len = 0;
	size_t ret = 0;
	int data_count = 0;
	int one_data_bytes = 0;
	int need_read_data_len = 0;
	int type = 0;
	int tmpbufsize = 4096;
	unsigned char *tmpbuf = NULL;
	unsigned char tmpbufback[128] = { 0 };

	mutex_lock(&ipd->ilitek_debug_read_mutex);

	while (ipd->debug_data_frame <= 0) {
		if (filp->f_flags & O_NONBLOCK) {
			return -EAGAIN;
		}
		wait_event_interruptible(ipd->inq, ipd->debug_data_frame > 0);
	}

	mutex_lock(&ipd->ilitek_debug_mutex);

	tmpbuf = vmalloc(4096);	/* buf size if even */
	if (ERR_ALLOC_MEM(tmpbuf)) {
		ipio_err("buffer vmalloc error\n");
		send_data_len +=
		    snprintf(tmpbufback + send_data_len,
		     (sizeof(tmpbufback)-send_data_len),
			    "buffer vmalloc error\n");
		ret = copy_to_user(buff, tmpbufback, send_data_len);	/*ipd->debug_buf[0] */
	} else {
		if (ipd->debug_data_frame > 0) {
			if (ipd->debug_buf[0][0] == 0x5A) {
				need_read_data_len = 43;
			} else if (ipd->debug_buf[0][0] == 0x7A) {
				type = ipd->debug_buf[0][3] & 0x0F;

				data_count =
				    ipd->debug_buf[0][1] * ipd->debug_buf[0][2];

				if (type == 0 || type == 1 || type == 6) {
					one_data_bytes = 1;
				} else if (type == 2 || type == 3) {
					one_data_bytes = 2;
				} else if (type == 4 || type == 5) {
					one_data_bytes = 4;
				}
				need_read_data_len =
				    data_count * one_data_bytes + 1 + 5;
			}

			send_data_len = 0;	/* ipd->debug_buf[0][1] - 2; */
			need_read_data_len = 2040;
			if (need_read_data_len <= 0) {
				ipio_err("parse data err data len = %d\n",
					 need_read_data_len);
				send_data_len +=
				    snprintf(tmpbuf + send_data_len,
				     (tmpbufsize-send_data_len),
					    "parse data err data len = %d\n",
					    need_read_data_len);
			} else {
				for (i = 0 ; i < need_read_data_len ; i++) {
					send_data_len +=
					    snprintf(tmpbuf + send_data_len,
					     (tmpbufsize-send_data_len),
						"%02X", ipd->debug_buf[0][i]);
					if (send_data_len >= 4096) {
						ipio_err
						    ("send_data_len = %d set 4096 i = %d\n",
						     send_data_len, i);
						send_data_len = 4096;
						break;
					}
				}
			}
			send_data_len += snprintf(tmpbuf + send_data_len,
			     (tmpbufsize-send_data_len), "\n\n");

			if (p == 5 || size == 4096 || size == 2048) {
				ipd->debug_data_frame--;
				if (ipd->debug_data_frame < 0) {
					ipd->debug_data_frame = 0;
				}

				for (i = 1; i <= ipd->debug_data_frame; i++) {
					memcpy(ipd->debug_buf[i - 1],
					       ipd->debug_buf[i], 2048);
				}
			}
		} else {
			ipio_err("no data send\n");
			send_data_len += snprintf(tmpbuf + send_data_len,
			 (tmpbufsize-send_data_len), "no data send\n");
		}

		/* Preparing to send data to user */
		if (size == 4096)
			ret = copy_to_user(buff, tmpbuf, send_data_len);
		else
			ret = copy_to_user(buff, tmpbuf + p, send_data_len - p);

		if (ret) {
			ipio_err("copy_to_user err\n");
			ret = -EFAULT;
		} else {
			*pPos += count;
			ret = count;
			ipio_debug(DEBUG_FINGER_REPORT,
				   "Read %d bytes(s) from %ld\n", count, p);
		}
	}
	/* ipio_err("send_data_len = %d\n", send_data_len); */
	if (send_data_len <= 0 || send_data_len > 4096) {
		ipio_err("send_data_len = %d set 2048\n", send_data_len);
		send_data_len = 4096;
	}
	if (tmpbuf != NULL) {
		vfree(tmpbuf);
		tmpbuf = NULL;
	}

	mutex_unlock(&ipd->ilitek_debug_mutex);
	mutex_unlock(&ipd->ilitek_debug_read_mutex);
	return send_data_len;
}

/* Created only for oppo */
static ssize_t ilitek_proc_oppo_mp_lcm_on_read(struct file *filp,
					       char __user *buff, size_t size,
					       loff_t *pPos)
{
	int len = 0;

	if (*pPos != 0)
		return 0;

	if (core_parser_path(INI_NAME_PATH) < 0) {
		ipio_err("Failed to parsing INI file\n");
		goto out;
	}

	/* Init MP structure */
	if (core_mp_init() < 0) {
		ipio_err("Failed to init mp\n");
		goto out;
	}

	/* Switch to test mode */
	core_fr_mode_control(&protocol->test_mode);

	if (core_mp->busy_cdc != ISR_CHECK)
		ilitek_platform_disable_irq();

	core_fr->isEnableFR = false;

	core_mp->oppo_run = true;

	/* Do not chang the sequence of test */
	core_mp_run_test("Noise Peak To Peak(With Panel)", true);
	core_mp_run_test("Noise Peak to Peak(IC Only)", true);
	core_mp_run_test("Short Test -ILI9881", true);
	core_mp_run_test("Open Test(integration)_SP", true);
	core_mp_run_test("Raw Data(Have BK)", true);
	core_mp_run_test("Calibration Data(DAC)", true);
	core_mp_run_test("Raw Data(No BK)", true);
	core_mp_run_test("Doze Raw Data", true);
	core_mp_run_test("Doze Peak To Peak", true);

	if (core_mp->busy_cdc == ISR_CHECK)
		ilitek_platform_disable_irq();

	core_mp_show_result();

	core_mp->oppo_run = false;

	core_mp_test_free();

	/* Switch to demo mode */
	core_fr_mode_control(&protocol->demo_mode);

#ifdef HOST_DOWNLOAD
	ilitek_platform_tp_hw_reset(true);
#endif

	/*ilitek_platform_tp_hw_reset(true);*/
	/* Soft reset */
	core_config_ice_mode_enable();
	core_config_set_watch_dog(false);
	mdelay(10);
	core_config_ic_reset();

	core_fr->isEnableFR = true;
	ilitek_platform_enable_irq();

out:
	*pPos = len;
	return len;
}

/* Created only for oppo */
static ssize_t ilitek_proc_oppo_mp_lcm_off_read(struct file *filp,
						char __user *buff, size_t size,
						loff_t *pPos)
{
	int len = 0;

	if (*pPos != 0)
		return 0;

	if (core_parser_path(INI_NAME_PATH) < 0) {
		ipio_err("Failed to parsing INI file\n");
		goto out;
	}

	/* Init MP structure */
	if (core_mp_init() < 0) {
		ipio_err("Failed to init mp\n");
		goto out;
	}

	if (core_mp->busy_cdc != ISR_CHECK)
		ilitek_platform_disable_irq();

	core_fr->isEnableFR = false;

	/* Enter to suspend and move gesture code to iram */
	core_config->isEnableGesture = true;
	core_gesture->mode = GESTURE_INFO_MPDE;

	/* sense stop */
	core_config_sense_ctrl(false);

	if (core_config_check_cdc_busy(50, 100) < 0)
		ipio_err("Check busy is timout !\n");

	core_fr->actual_fw_mode = P5_0_FIRMWARE_GESTURE_MODE;

#ifdef HOST_DOWNLOAD
	core_gesture_load_code();
#endif

	/* Switch to test mode which moves mp code to iram */
	core_fr_mode_control(&protocol->test_mode);


	/* Indicates running mp test is called by oppo node */
	core_mp->oppo_run = true;
	core_mp->oppo_lcm = true;

	/* Do not chang the sequence of test */
	core_mp_run_test("Raw Data(No BK) (LCM OFF)", true);
	core_mp_run_test("Noise Peak to Peak(With Panel) (LCM OFF)", true);
	core_mp_run_test("Raw Data_TD (LCM OFF)", true);
	core_mp_run_test("Peak To Peak_TD (LCM OFF)", true);

	if (core_mp->busy_cdc == ISR_CHECK)
		ilitek_platform_disable_irq();

	core_mp_show_result();

	core_mp->oppo_run = false;
	core_mp->oppo_lcm = false;

	core_mp_ctrl_lcm_status(true);

	core_mp_test_free();

	core_fr_mode_control(&protocol->demo_mode);

	/*ilitek_platform_tp_hw_reset(true);*/
	/* Soft reset */
	core_config_ice_mode_enable();
	core_config_set_watch_dog(false);
	mdelay(10);
	core_config_ic_reset();

	core_fr->isEnableFR = true;
	ilitek_platform_enable_irq();

out:
	*pPos = len;
	return len;
}

static ssize_t ilitek_proc_mp_test_read(struct file *filp, char __user *buff,
					size_t size, loff_t *pPos)
{
	uint32_t len = 0;

	if (*pPos != 0)
		return 0;

	if (core_parser_path(INI_NAME_PATH) < 0) {
		ipio_err("Failed to parsing INI file\n");
		goto out;
	}

	/* Init MP structure */
	if (core_mp_init() < 0) {
		ipio_err("Failed to init mp\n");
		goto out;
	}

	/* Switch to Test mode nad move mp code */
	core_fr_mode_control(&protocol->test_mode);

	if (core_mp->busy_cdc != ISR_CHECK)
		ilitek_platform_disable_irq();

	core_fr->isEnableFR = false;
	/* Start to run MP test */
	core_mp->run = true;

	/*
	 * Get timing parameters first.
	 * Howerver, this can be ignored if read them from ini.
	 */
	if (protocol->major >= 5 && protocol->mid >= 4) {
		if (core_mp_calc_timing_nodp() < 0) {
			ipio_err("Can't get timing parameters\n");
			goto out;
		}
	}

	/* Do not chang the sequence of test */
	core_mp_run_test("Noise Peak To Peak(With Panel)", true);
	core_mp_run_test("Noise Peak to Peak(IC Only)", true);
	core_mp_run_test("Short Test -ILI9881", true);
	core_mp_run_test("Open Test(integration)_SP", true);
	core_mp_run_test("Raw Data(Have BK)", true);
	/*core_mp_run_test("Raw Data(Have BK) (LCM OFF)", true); */
	core_mp_run_test("Calibration Data(DAC)", true);
	core_mp_run_test("Raw Data(No BK)", true);
	core_mp_run_test("Raw Data(No BK) (LCM OFF)", true);
	core_mp_run_test("Noise Peak to Peak(With Panel) (LCM OFF)", true);
	/*core_mp_run_test("Noise Peak to Peak(IC Only) (LCM OFF)", true); */
	core_mp_run_test("Raw Data_TD (LCM OFF)", true);
	core_mp_run_test("Peak To Peak_TD (LCM OFF)", true);
	core_mp_run_test("Doze Raw Data", true);
	core_mp_run_test("Doze Peak To Peak", true);
	/*core_mp_run_test("Pin Test ( INT and RST )", true); */


	if (core_mp->busy_cdc == ISR_CHECK)
		ilitek_platform_disable_irq();

	core_mp_show_result();

	core_mp->run = false;

	core_mp_test_free();

#ifndef HOST_DOWNLOAD
	core_config_ice_mode_enable();

	if (core_config_set_watch_dog(false) < 0) {
		ipio_err("Failed to disable watch dog\n");
	}

	core_config_ic_reset();
#endif
	/* Switch to Demo mode */
	core_fr_mode_control(&protocol->demo_mode);

#ifdef HOST_DOWNLOAD
	ilitek_platform_tp_hw_reset(true);
#endif


	core_fr->isEnableFR = true;
	ilitek_platform_enable_irq();

out:
	*pPos = len;
	return len;
}

static ssize_t ilitek_proc_mp_test_write(struct file *filp, const char *buff,
					 size_t size, loff_t *pPos)
{
	int i, res = 0, count = 0;
	char cmd[64] = { 0 }, str[512] = {
	0};
	char *token = NULL, *cur = NULL;
	uint8_t *va = NULL;

	if (buff != NULL) {
		res = copy_from_user(cmd, buff, size - 1);
		if (res < 0) {
			ipio_info("copy data from user space, failed\n");
			return ERROR;
		}
	}

	ipio_info("size = %d, cmd = %s\n", (int)size, cmd);

	if (size > 64) {
		ipio_err("The size of string is too long\n");
		return size;
	}

	token = cur = cmd;

	va = kcalloc(64, sizeof(uint8_t), GFP_KERNEL);

	while ((token = strsep(&cur, ",")) != NULL) {
		va[count] = katoi(token);
		ipio_info("data[%d] = %x\n", count, va[count]);
		count++;
	}

	ipio_info("cmd = %s\n", cmd);

	/* Init MP structure */
	if (core_mp_init() < 0) {
		ipio_err("Failed to init mp\n");
		return size;
	}

	/* Switch to Test mode */
	core_fr_mode_control(&protocol->test_mode);

	ilitek_platform_disable_irq();

	for (i = 0 ; i < core_mp->mp_items; i++) {
		if (strcmp(cmd, tItems[i].name) == 0) {
			strlcpy(str, tItems[i].desp, strlen(str)+1);
			tItems[i].run = 1;
			tItems[i].max = va[1];
			tItems[i].min = va[2];
			tItems[i].frame_count = va[3];
			break;
		}
	}

	core_mp_run_test(str, false);

	core_mp_show_result();

	core_mp_test_free();

#ifndef HOST_DOWNLOAD
	core_config_ice_mode_enable();

	if (core_config_set_watch_dog(false) < 0) {
		ipio_err("Failed to disable watch dog\n");
	}

	core_config_ic_reset();
#endif

	core_fr_mode_control(&protocol->demo_mode);

#ifdef HOST_DOWNLOAD
	ilitek_platform_tp_hw_reset(true);
#endif

	ilitek_platform_enable_irq();

	ipio_info("MP Test DONE\n");
	ipio_kfree((void **)&va);
	return size;
}

static ssize_t ilitek_proc_debug_level_read(struct file *filp,
					    char __user *buff, size_t size,
					    loff_t *pPos)
{
	int res = 0;
	uint32_t len = 0;

	if (*pPos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	len = snprintf(g_user_buf, USER_STR_BUFF, "%d", ipio_debug_level);

	ipio_info("Current DEBUG Level = %d\n", ipio_debug_level);
	ipio_info("You can set one of levels for debug as below:\n");
	ipio_info("DEBUG_NONE = %d\n", DEBUG_NONE);
	ipio_info("DEBUG_IRQ = %d\n", DEBUG_IRQ);
	ipio_info("DEBUG_FINGER_REPORT = %d\n", DEBUG_FINGER_REPORT);
	ipio_info("DEBUG_FIRMWARE = %d\n", DEBUG_FIRMWARE);
	ipio_info("DEBUG_CONFIG = %d\n", DEBUG_CONFIG);
	ipio_info("DEBUG_I2C = %d\n", DEBUG_I2C);
	ipio_info("DEBUG_BATTERY = %d\n", DEBUG_BATTERY);
	ipio_info("DEBUG_MP_TEST = %d\n", DEBUG_MP_TEST);
	ipio_info("DEBUG_IOCTL = %d\n", DEBUG_IOCTL);
	ipio_info("DEBUG_NETLINK = %d\n", DEBUG_NETLINK);
	ipio_info("DEBUG_ALL = %d\n", DEBUG_ALL);

	res = copy_to_user((uint32_t *) buff, &ipio_debug_level, len);
	if (res < 0) {
		ipio_err("Failed to copy data to user space\n");
	}

	*pPos = len;

	return len;
}

static ssize_t ilitek_proc_debug_level_write(struct file *filp,
					     const char *buff, size_t size,
					     loff_t *pPos)
{
	int res = 0;
	char cmd[10] = { 0 };

	if (buff != NULL) {
		res = copy_from_user(cmd, buff, size - 1);
		if (res < 0) {
			ipio_info("copy data from user space, failed\n");
			return ERROR;
		}
	}

	ipio_debug_level = katoi(cmd);

	ipio_info("ipio_debug_level = %d\n", ipio_debug_level);

	return size;
}

static ssize_t ilitek_proc_gesture_read(struct file *filp, char __user *buff,
					size_t size, loff_t *pPos)
{
	int res = 0;
	uint32_t len = 0;

	if (*pPos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	len = snprintf(g_user_buf, USER_STR_BUFF, "%d", core_config->isEnableGesture);

	ipio_info("isEnableGesture = %d\n", core_config->isEnableGesture);

	res =
	    copy_to_user((uint32_t *) buff, &core_config->isEnableGesture, len);
	if (res < 0) {
		ipio_err("Failed to copy data to user space\n");
	}

	*pPos = len;

	return len;
}

static ssize_t ilitek_proc_gesture_write(struct file *filp, const char *buff,
					 size_t size, loff_t *pPos)
{
	int res = 0;
	char cmd[10] = { 0 };

	if (buff != NULL) {
		res = copy_from_user(cmd, buff, size - 1);
		if (res < 0) {
			ipio_info("copy data from user space, failed\n");
			return ERROR;
		}
	}

	ipio_info("size = %d, cmd = %s\n", (int)size, cmd);

	if (strcmp(cmd, "on") == 0) {
		ipio_info("enable gesture mode\n");
		core_config->isEnableGesture = true;
	} else if (strcmp(cmd, "off") == 0) {
		ipio_info("disable gesture mode\n");
		core_config->isEnableGesture = false;
	} else if (strcmp(cmd, "info") == 0) {
		ipio_info("gesture info mode\n");
		core_gesture->mode = GESTURE_INFO_MPDE;
	} else if (strcmp(cmd, "normal") == 0) {
		ipio_info("gesture normal mode\n");
		core_gesture->mode = GESTURE_NORMAL_MODE;
	} else
		ipio_err("Unknown command\n");

	return size;
}

static ssize_t ilitek_proc_check_battery_read(struct file *filp,
					      char __user *buff, size_t size,
					      loff_t *pPos)
{
	int res = 0;
	uint32_t len = 0;

	if (*pPos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	len = snprintf(g_user_buf, USER_STR_BUFF, "%d", ipd->isEnablePollCheckPower);

	ipio_info("isEnablePollCheckPower = %d\n", ipd->isEnablePollCheckPower);

	res =
	    copy_to_user((uint32_t *) buff, &ipd->isEnablePollCheckPower, len);
	if (res < 0) {
		ipio_err("Failed to copy data to user space\n");
	}

	*pPos = len;

	return len;
}

static ssize_t ilitek_proc_check_battery_write(struct file *filp,
					       const char *buff, size_t size,
					       loff_t *pPos)
{
	int res = 0;
	char cmd[10] = { 0 };

	if (buff != NULL) {
		res = copy_from_user(cmd, buff, size - 1);
		if (res < 0) {
			ipio_info("copy data from user space, failed\n");
			return ERROR;
		}
	}

	ipio_info("size = %d, cmd = %s\n", (int)size, cmd);

#ifdef ENABLE_BATTERY_CHECK
	if (strcmp(cmd, "on") == 0) {
		ipio_info("Start the thread of check power status\n");
		queue_delayed_work(ipd->check_power_status_queue,
				   &ipd->check_power_status_work,
				   ipd->work_delay);
		ipd->isEnablePollCheckPower = true;
	} else if (strcmp(cmd, "off") == 0) {
		ipio_info("Cancel the thread of check power status\n");
		cancel_delayed_work_sync(&ipd->check_power_status_work);
		ipd->isEnablePollCheckPower = false;
	} else
		ipio_err("Unknown command\n");
#else
	ipio_err("You need to enable its MACRO before operate it.\n");
#endif

	return size;
}

static ssize_t ilitek_proc_fw_process_read(struct file *filp,
					   char __user *buff, size_t size,
					   loff_t *pPos)
{
	int res = 0;
	uint32_t len = 0;

	/*
	 * If file position is non-zero,  we assume the string has been read
	 * and indicates that there is no more data to be read.
	 */
	if (*pPos != 0)
		return 0;

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	len = snprintf(g_user_buf, USER_STR_BUFF, "%02d", core_firmware->update_status);

	ipio_info("update status = %d\n", core_firmware->update_status);

	res =
	    copy_to_user((uint32_t *) buff, &core_firmware->update_status, len);
	if (res < 0) {
		ipio_err("Failed to copy data to user space");
	}

	*pPos = len;

	return len;
}

/*
 * To avoid the restriction of selinux, we assigned a fixed path where locates firmware file,
 * reading (cat) this node to notify driver running the upgrade process from user space.
 */
static ssize_t ilitek_proc_fw_upgrade_read(struct file *filp,
					   char __user *buff, size_t size,
					   loff_t *pPos)
{
	int res = 0;
	uint32_t len = 0;

	ipio_info("Preparing to upgarde firmware\n");

	if (*pPos != 0)
		return 0;

	ilitek_platform_disable_irq();

	if (ipd->isEnablePollCheckPower)
		cancel_delayed_work_sync(&ipd->check_power_status_work);
#ifdef HOST_DOWNLOAD
	ilitek_platform_tp_hw_reset(true);
#else
	res = core_firmware_upgrade(UPDATE_FW_PATH, false);
#endif
	ilitek_platform_enable_irq();

	if (ipd->isEnablePollCheckPower)
		queue_delayed_work(ipd->check_power_status_queue,
				   &ipd->check_power_status_work,
				   ipd->work_delay);

	if (res < 0) {
		core_firmware->update_status = res;
		ipio_err("Failed to upgrade firwmare\n");
	} else {
		core_firmware->update_status = 100;
		ipio_info("Succeed to upgrade firmware\n");
	}

	*pPos = len;

	return len;
}

static ssize_t ilitek_proc_iram_upgrade_read(struct file *filp,
					     char __user *buff, size_t size,
					     loff_t *pPos)
{
	int res = 0;
	uint32_t len = 0;

	ipio_info("Preparing to upgarde firmware by IRAM\n");

	if (*pPos != 0)
		return 0;

	ilitek_platform_disable_irq();

	res = core_firmware_upgrade(UPDATE_FW_PATH, true);

	ilitek_platform_enable_irq();

	if (res < 0) {
		/* return the status to user space even if any error occurs. */
		core_firmware->update_status = res;
		ipio_err("Failed to upgrade firwmare by IRAM, res = %d\n", res);
	} else {
		ipio_info("Succeed to upgrade firmware by IRAM\n");
	}

	*pPos = len;

	return len;
}

/* for debug */
static ssize_t ilitek_proc_ioctl_read(struct file *filp, char __user *buff,
				      size_t size, loff_t *pPos)
{
	int res = 0;
	uint32_t len = 0;
	uint8_t cmd[2] = { 0 };

	if (*pPos != 0)
		return 0;

	if (size < 4095) {
		res = copy_from_user(cmd, buff, size - 1);
		if (res < 0) {
			ipio_info("copy data from user space, failed\n");
			return ERROR;
		}
	}

	ipio_info("size = %d, cmd = %d", (int)size, cmd[0]);

	/* test */
	if (cmd[0] == 0x1) {
		ipio_info("HW Reset\n");
		ilitek_platform_tp_hw_reset(true);
	} else if (cmd[0] == 0x02) {
		ipio_info("Disable IRQ\n");
		ilitek_platform_disable_irq();
	} else if (cmd[0] == 0x03) {
		ipio_info("Enable IRQ\n");
		ilitek_platform_enable_irq();
	} else if (cmd[0] == 0x04) {
		ipio_info("Get Chip id\n");
		core_config_get_chip_id();
	}

	*pPos = len;

	return len;
}

/* for debug */
static ssize_t ilitek_proc_ioctl_write(struct file *filp, const char *buff,
				       size_t size, loff_t *pPos)
{
	int res = 0, count = 0, i;
	int w_len = 0, r_len = 0, delay = 0;
	char cmd[512] = { 0 };
	char *token = NULL, *cur = NULL;
	uint8_t temp[256] = { 0 };
	uint8_t *data = NULL;

	if (buff != NULL) {
		res = copy_from_user(cmd, buff, size - 1);
		if (res < 0) {
			ipio_info("copy data from user space, failed\n");
			return ERROR;
		}
	}

	ipio_info("size = %d, cmd = %s\n", (int)size, cmd);

	token = cur = cmd;

	data = kmalloc(512 * sizeof(uint8_t), GFP_KERNEL);
	memset(data, 0, 512);

	while ((token = strsep(&cur, ",")) != NULL) {
		data[count] = str2hex(token);
		ipio_info("data[%d] = %x\n", count, data[count]);
		count++;
	}

	ipio_info("cmd = %s\n", cmd);

	if (strcmp(cmd, "reset") == 0) {
		ipio_info("HW Reset\n");
		ilitek_platform_tp_hw_reset(true);
	} else if (strcmp(cmd, "disirq") == 0) {
		ipio_info("Disable IRQ\n");
		ilitek_platform_disable_irq();
	} else if (strcmp(cmd, "enairq") == 0) {
		ipio_info("Enable IRQ\n");
		ilitek_platform_enable_irq();
	} else if (strcmp(cmd, "getchip") == 0) {
		ipio_info("Get Chip id\n");
		core_config_get_chip_id();
	} else if (strcmp(cmd, "dispcc") == 0) {
		ipio_info("disable phone cover\n");
		core_config_phone_cover_ctrl(false);
	} else if (strcmp(cmd, "enapcc") == 0) {
		ipio_info("enable phone cover\n");
		core_config_phone_cover_ctrl(true);
	} else if (strcmp(cmd, "disfsc") == 0) {
		ipio_info("disable finger sense\n");
		    core_config_finger_sense_ctrl(false);
	} else if (strcmp(cmd, "enafsc") == 0) {
		ipio_info("enable finger sense\n");
		core_config_finger_sense_ctrl(true);
	} else if (strcmp(cmd, "disprox") == 0) {
		ipio_info("disable proximity\n");
		core_config_proximity_ctrl(false);
	} else if (strcmp(cmd, "enaprox") == 0) {
		ipio_info("enable proximity\n");
		core_config_proximity_ctrl(true);
	} else if (strcmp(cmd, "disglove") == 0) {
		ipio_info("disable glove function\n");
		core_config_glove_ctrl(false, false);
	} else if (strcmp(cmd, "enaglove") == 0) {
		ipio_info("enable glove function\n");
		core_config_glove_ctrl(true, false);
	} else if (strcmp(cmd, "glovesl") == 0) {
		ipio_info("set glove as seamless\n");
		core_config_glove_ctrl(true, true);
	} else if (strcmp(cmd, "enastylus") == 0) {
		ipio_info("enable stylus\n");
		core_config_stylus_ctrl(true, false);
	} else if (strcmp(cmd, "disstylus") == 0) {
		ipio_info("disable stylus\n");
		core_config_stylus_ctrl(false, false);
	} else if (strcmp(cmd, "stylussl") == 0) {
		ipio_info("set stylus as seamless\n");
		core_config_stylus_ctrl(true, true);
	} else if (strcmp(cmd, "tpscan_ab") == 0) {
		ipio_info("set TP scan as mode AB\n");
		core_config_tp_scan_mode(true);
	} else if (strcmp(cmd, "tpscan_b") == 0) {
		ipio_info("set TP scan as mode B\n");
		core_config_tp_scan_mode(false);
	} else if (strcmp(cmd, "phone_cover") == 0) {
		ipio_info("set size of phone conver window\n");
		core_config_set_phone_cover(data);
	} else if (strcmp(cmd, "debugmode") == 0) {
		ipio_info("debug mode test enter\n");
		temp[0] = protocol->debug_mode;
		core_fr_mode_control(temp);
	} else if (strcmp(cmd, "baseline") == 0) {
		ipio_info("test baseline raw\n");
		temp[0] = protocol->debug_mode;
		core_fr_mode_control(temp);
		ilitek_platform_disable_irq();
		temp[0] = 0xFA;
		temp[1] = 0x08;
		core_write(core_config->slave_i2c_addr, temp, 2);
		ilitek_platform_enable_irq();
	} else if (strcmp(cmd, "delac_on") == 0) {
		ipio_info("test get delac\n");
		temp[0] = protocol->debug_mode;
		core_fr_mode_control(temp);
		ilitek_platform_disable_irq();
		temp[0] = 0xFA;
		temp[1] = 0x03;
		core_write(core_config->slave_i2c_addr, temp, 2);
		ilitek_platform_enable_irq();
	} else if (strcmp(cmd, "delac_off") == 0) {
		ipio_info("test get delac\n");
		temp[0] = protocol->demo_mode;
		core_fr_mode_control(temp);
	} else if (strcmp(cmd, "test") == 0) {
		ipio_info("test test_reset test 1\n");
		gpio_direction_output(ipd->reset_gpio, 1);
		mdelay(1);
		gpio_set_value(ipd->reset_gpio, 0);
		mdelay(1);
		gpio_set_value(ipd->reset_gpio, 1);
		mdelay(10);
	} else if (strcmp(cmd, "gt") == 0) {
		ipio_info("test Gesture test\n");
#ifdef HOST_DOWNLOAD
		core_gesture_load_code();
#endif
	} else if (strcmp(cmd, "suspend") == 0) {
		ipio_info("test suspend test\n");
		core_config_ic_suspend();
	} else if (strcmp(cmd, "resume") == 0) {
		ipio_info("test resume test\n");
		core_config_ic_resume();
	}

	else if (strcmp(cmd, "gt1") == 0) {
		ipio_info("test Gesture test 1\n");
		temp[0] = 0x01;
		temp[1] = 0x01;
		temp[2] = 0x00;
		w_len = 3;
		core_write(core_config->slave_i2c_addr, temp, w_len);
		if (core_config_check_cdc_busy(50, 100) < 0)
			ipio_err("Check busy is timout !\n");
	} else if (strcmp(cmd, "gt2") == 0) {
		temp[0] = 0x01;
		temp[1] = 0x0A;
		temp[2] = 0x01;
		w_len = 3;
		core_write(core_config->slave_i2c_addr, temp, w_len);
		ipio_info("test Gesture test\n");
	} else if (strcmp(cmd, "i2c_w") == 0) {
		w_len = data[1];
		ipio_info("w_len = %d\n", w_len);

		for (i = 0 ; i < w_len ; i++) {
			temp[i] = data[2 + i];
			ipio_info("i2c[%d] = %x\n", i, temp[i]);
		}

		core_write(core_config->slave_i2c_addr, temp, w_len);
	} else if (strcmp(cmd, "i2c_r") == 0) {
		r_len = data[1];
		ipio_info("r_len = %d\n", r_len);

		core_read(core_config->slave_i2c_addr, &temp[0], r_len);

		for (i = 0 ; i < r_len ; i++)
			ipio_info("temp[%d] = %x\n", i, temp[i]);
	} else if (strcmp(cmd, "i2c_w_r") == 0) {
		w_len = data[1];
		r_len = data[2];
		delay = data[3];
		ipio_info("w_len = %d, r_len = %d, delay = %d\n", w_len, r_len,
			  delay);

		for (i = 0 ; i < w_len ; i++) {
			temp[i] = data[4 + i];
			ipio_info("temp[%d] = %x\n", i, temp[i]);
		}

		core_write(core_config->slave_i2c_addr, temp, w_len);

		memset(temp, 0, sizeof(temp));
		mdelay(delay);

		core_read(core_config->slave_i2c_addr, &temp[0], r_len);

		for (i = 0 ; i < r_len ; i++)
			ipio_info("temp[%d] = %x\n", i, temp[i]);
	} else {
		ipio_err("Unknown command\n");
	}

	ipio_kfree((void **)&data);
	return size;
}

static long ilitek_proc_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	int res = 0, length = 0;
	uint8_t *szBuf = NULL;
	static uint16_t i2c_rw_length;
	uint32_t id_to_user = 0x0;
	char dbg[10] = { 0 };

	ipio_debug(DEBUG_IOCTL, "cmd = %d\n", _IOC_NR(cmd));

	if (_IOC_TYPE(cmd) != ILITEK_IOCTL_MAGIC) {
		ipio_err("The Magic number doesn't match\n");
		return -ENOTTY;
	}

	if (_IOC_NR(cmd) > ILITEK_IOCTL_MAXNR) {
		ipio_err("The number of ioctl doesn't match\n");
		return -ENOTTY;
	}

	szBuf = kmalloc(2048 * sizeof(uint8_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(szBuf)) {
		ipio_err("Failed to allocate szBuf memory\n");
		return -ENOMEM;
	}

	switch (cmd) {
	case ILITEK_IOCTL_I2C_WRITE_DATA:
		res = copy_from_user(szBuf, (uint8_t *) arg, i2c_rw_length);
		if (res < 0) {
			ipio_err("Failed to copy data from user space\n");
		} else {
			res =
			    core_write(core_config->slave_i2c_addr, &szBuf[0],
				       i2c_rw_length);
			if (res < 0) {
				ipio_err("Failed to write data via i2c\n");
			}
		}
		break;

	case ILITEK_IOCTL_I2C_READ_DATA:
		res =
		    core_read(core_config->slave_i2c_addr, szBuf,
			      i2c_rw_length);
		if (res < 0) {
			ipio_err("Failed to read data via i2c\n");
		} else {
			res =
			    copy_to_user((uint8_t *) arg, szBuf, i2c_rw_length);
			if (res < 0) {
				ipio_err("Failed to copy data to user space\n");
			}
		}
		break;

	case ILITEK_IOCTL_I2C_SET_WRITE_LENGTH:
	case ILITEK_IOCTL_I2C_SET_READ_LENGTH:
		i2c_rw_length = arg;
		break;

	case ILITEK_IOCTL_TP_HW_RESET:
		ilitek_platform_tp_hw_reset(true);
		break;

	case ILITEK_IOCTL_TP_POWER_SWITCH:
		ipio_info("Not implemented yet\n");
		break;

	case ILITEK_IOCTL_TP_REPORT_SWITCH:
		res = copy_from_user(szBuf, (uint8_t *) arg, 1);
		if (res < 0) {
			ipio_err("Failed to copy data from user space\n");
		} else {
			if (szBuf[0]) {
				core_fr->isEnableFR = true;
				ipio_debug(DEBUG_IOCTL,
					   "Function of finger report was enabled\n");
			} else {
				core_fr->isEnableFR = false;
				ipio_debug(DEBUG_IOCTL,
					   "Function of finger report was disabled\n");
			}
		}
		break;

	case ILITEK_IOCTL_TP_IRQ_SWITCH:
		res = copy_from_user(szBuf, (uint8_t *) arg, 1);
		if (res < 0) {
			ipio_err("Failed to copy data from user space\n");
		} else {
			if (szBuf[0]) {
				ilitek_platform_enable_irq();
			} else {
				ilitek_platform_disable_irq();
			}
		}
		break;

	case ILITEK_IOCTL_TP_DEBUG_LEVEL:
		res = copy_from_user(dbg, (uint32_t *) arg, sizeof(uint32_t));
		if (res < 0) {
			ipio_err("Failed to copy data from user space\n");
		} else {
			ipio_debug_level = katoi(dbg);
			ipio_info("ipio_debug_level = %d", ipio_debug_level);
		}
		break;

	case ILITEK_IOCTL_TP_FUNC_MODE:
		res = copy_from_user(szBuf, (uint8_t *) arg, 3);
		if (res < 0) {
			ipio_err("Failed to copy data from user space\n");
		} else {
			core_write(core_config->slave_i2c_addr, &szBuf[0], 3);
		}

		break;

	case ILITEK_IOCTL_TP_FW_VER:
		res = core_config_get_fw_ver();
		if (res < 0) {
			ipio_err("Failed to get firmware version\n");
		} else {
			res =
			    copy_to_user((uint8_t *) arg,
					 core_config->firmware_ver,
					 protocol->fw_ver_len);
			if (res < 0) {
				ipio_err
				    ("Failed to copy firmware version to user space\n");
			}
		}
		break;

	case ILITEK_IOCTL_TP_PL_VER:
		res = core_config_get_protocol_ver();
		if (res < 0) {
			ipio_err("Failed to get protocol version\n");
		} else {
			res =
			    copy_to_user((uint8_t *) arg,
					 core_config->protocol_ver,
					 protocol->pro_ver_len);
			if (res < 0) {
				ipio_err
				    ("Failed to copy protocol version to user space\n");
			}
		}
		break;

	case ILITEK_IOCTL_TP_CORE_VER:
		res = core_config_get_core_ver();
		if (res < 0) {
			ipio_err("Failed to get core version\n");
		} else {
			res =
			    copy_to_user((uint8_t *) arg, core_config->core_ver,
					 protocol->core_ver_len);
			if (res < 0) {
				ipio_err
				    ("Failed to copy core version to user space\n");
			}
		}
		break;

	case ILITEK_IOCTL_TP_DRV_VER:
		length = snprintf(szBuf, 2048, "%s", DRIVER_VERSION);
		if (!length) {
			ipio_err
			    ("Failed to convert driver version from definiation\n");
		} else {
			res = copy_to_user((uint8_t *) arg, szBuf, length);
			if (res < 0) {
				ipio_err
				    ("Failed to copy driver ver to user space\n");
			}
		}
		break;

	case ILITEK_IOCTL_TP_CHIP_ID:
		res = core_config_get_chip_id();
		if (res < 0) {
			ipio_err("Failed to get chip id\n");
		} else {
			id_to_user =
			    core_config->chip_id << 16 | core_config->chip_type;

			res =
			    copy_to_user((uint32_t *) arg, &id_to_user,
					 sizeof(uint32_t));
			if (res < 0) {
				ipio_err
				    ("Failed to copy chip id to user space\n");
			}
		}
		break;

	case ILITEK_IOCTL_TP_NETLINK_CTRL:
		res = copy_from_user(szBuf, (uint8_t *) arg, 1);
		if (res < 0) {
			ipio_err("Failed to copy data from user space\n");
		} else {
			if (szBuf[0]) {
				core_fr->isEnableNetlink = true;
				ipio_debug(DEBUG_IOCTL,
					   "Netlink has been enabled\n");
			} else {
				core_fr->isEnableNetlink = false;
				ipio_debug(DEBUG_IOCTL,
					   "Netlink has been disabled\n");
			}
		}
		break;

	case ILITEK_IOCTL_TP_NETLINK_STATUS:
		ipio_debug(DEBUG_IOCTL, "Netlink is enabled : %d\n",
			   core_fr->isEnableNetlink);
		res =
		    copy_to_user((int *)arg, &core_fr->isEnableNetlink,
				 sizeof(int));
		if (res < 0) {
			ipio_err("Failed to copy chip id to user space\n");
		}
		break;

	case ILITEK_IOCTL_TP_MODE_CTRL:
		res = copy_from_user(szBuf, (uint8_t *) arg, 4);
		if (res < 0) {
			ipio_err("Failed to copy data from user space\n");
		} else {
			core_fr_mode_control(szBuf);
		}
		break;

	case ILITEK_IOCTL_TP_MODE_STATUS:
		ipio_debug(DEBUG_IOCTL, "Current firmware mode : %d",
			   core_fr->actual_fw_mode);
		res =
		    copy_to_user((int *)arg, &core_fr->actual_fw_mode,
				 sizeof(int));
		if (res < 0) {
			ipio_err("Failed to copy chip id to user space\n");
		}
		break;

	case ILITEK_IOCTL_ICE_MODE_SWITCH:
		res = copy_from_user(szBuf, (uint8_t *) arg, 1);
		if (res < 0) {
			ipio_err("Failed to copy data from user space\n");
		} else {
			if (szBuf[0]) {
				core_config->icemodeenable = true;
			} else {
				core_config->icemodeenable = false;
			}
		}
		break;
	case ILITEK_IOCTL_MP_WRITE_CDC_FLAG_FALSE:
		res = copy_from_user(szBuf, (uint8_t *) arg, 1);
		if (res < 0) {
			ipio_err("Failed to copy data from user space\n");
		} else {
			ipio_debug(DEBUG_IOCTL, "ioctl write = %d\n", szBuf[0]);
			if (szBuf[0] == 0) {
				core_config->interrupt_flag = false;
				ipio_debug(DEBUG_IOCTL, "core_config->interrupt_flag = %d\n", core_config->interrupt_flag);
			}
		}
		break;

	case ILITEK_IOCTL_MP_READ_CDC_FLAG:

		res =
		    copy_to_user((int *)arg, &core_config->interrupt_flag,
				 sizeof(int));
		ipio_debug(DEBUG_IOCTL, "core_config->interrupt_flag = %d\n", core_config->interrupt_flag);
		if (res < 0) {
			ipio_err("Failed to copy interrupt_flag flag to user space\n");
		}
		break;

	default:
		res = -ENOTTY;
		break;
	}
	kfree((void **)szBuf);

	return res;
}

struct proc_dir_entry *proc_dir_ilitek;
struct proc_dir_entry *proc_ioctl;
struct proc_dir_entry *proc_fw_process;
struct proc_dir_entry *proc_fw_upgrade;
struct proc_dir_entry *proc_iram_upgrade;
struct proc_dir_entry *proc_gesture;
struct proc_dir_entry *proc_debug_level;
struct proc_dir_entry *proc_mp_test;
struct proc_dir_entry *proc_debug_message;
struct proc_dir_entry *proc_debug_message_switch;

struct file_operations proc_ioctl_fops = {
	.unlocked_ioctl = ilitek_proc_ioctl,
	.read = ilitek_proc_ioctl_read,
	.write = ilitek_proc_ioctl_write,
};

struct file_operations proc_fw_process_fops = {
	.read = ilitek_proc_fw_process_read,
};

struct file_operations proc_fw_upgrade_fops = {
	.read = ilitek_proc_fw_upgrade_read,
};

struct file_operations proc_iram_upgrade_fops = {
	.read = ilitek_proc_iram_upgrade_read,
};

struct file_operations proc_gesture_fops = {
	.write = ilitek_proc_gesture_write,
	.read = ilitek_proc_gesture_read,
};

struct file_operations proc_check_battery_fops = {
	.write = ilitek_proc_check_battery_write,
	.read = ilitek_proc_check_battery_read,
};

struct file_operations proc_debug_level_fops = {
	.write = ilitek_proc_debug_level_write,
	.read = ilitek_proc_debug_level_read,
};

struct file_operations proc_mp_test_fops = {
	.write = ilitek_proc_mp_test_write,
	.read = ilitek_proc_mp_test_read,
};

struct file_operations proc_oppo_mp_lcm_on_fops = {
	.read = ilitek_proc_oppo_mp_lcm_on_read,
};

struct file_operations proc_oppo_mp_lcm_off_fops = {
	.read = ilitek_proc_oppo_mp_lcm_off_read,
};

struct file_operations proc_debug_message_fops = {
	.write = ilitek_proc_debug_message_write,
	.read = ilitek_proc_debug_message_read,
};

struct file_operations proc_debug_message_switch_fops = {
	.read = ilitek_proc_debug_switch_read,
};

/**
 * This struct lists all file nodes will be created under /proc filesystem.
 *
 * Before creating a node that you want, declaring its file_operations structure
 * is necessary. After that, puts the structure into proc_table, defines its
 * node's name in the same row, and the init function lterates the table and
 * creates all nodes under /proc.
 *
 */
typedef struct {
	char *name;
	struct proc_dir_entry *node;
	struct file_operations *fops;
	bool isCreated;
} proc_node_t;

proc_node_t proc_table[] = {
	{"ioctl", NULL, &proc_ioctl_fops, false},
	{"fw_process", NULL, &proc_fw_process_fops, false},
	{"fw_upgrade", NULL, &proc_fw_upgrade_fops, false},
	{"iram_upgrade", NULL, &proc_iram_upgrade_fops, false},
	{"gesture", NULL, &proc_gesture_fops, false},
	{"check_battery", NULL, &proc_check_battery_fops, false},
	{"debug_level", NULL, &proc_debug_level_fops, false},
	{"mp_test", NULL, &proc_mp_test_fops, false},
	{"oppo_mp_lcm_on", NULL, &proc_oppo_mp_lcm_on_fops, false},
	{"oppo_mp_lcm_off", NULL, &proc_oppo_mp_lcm_off_fops, false},
	{"debug_message", NULL, &proc_debug_message_fops, false},
	{"debug_message_switch", NULL, &proc_debug_message_switch_fops, false},
};

#define NETLINK_USER 21
struct sock *_gNetLinkSkb;
struct nlmsghdr *_gNetLinkHead;
struct sk_buff *_gSkbOut;
int _gPID;

void netlink_reply_msg(void *raw, int size)
{
	int res;
	int msg_size = size;
	uint8_t *data = (uint8_t *) raw;

	ipio_debug(DEBUG_NETLINK, "The size of data being sent to user = %d\n",
		   msg_size);
	ipio_debug(DEBUG_NETLINK, "pid = %d\n", _gPID);
	ipio_debug(DEBUG_NETLINK, "Netlink is enable = %d\n",
		   core_fr->isEnableNetlink);

	if (core_fr->isEnableNetlink) {
		_gSkbOut = nlmsg_new(msg_size, 0);

		if (!_gSkbOut) {
			ipio_err("Failed to allocate new skb\n");
			return;
		}

		_gNetLinkHead =
		    nlmsg_put(_gSkbOut, 0, 0, NLMSG_DONE, msg_size, 0);
		NETLINK_CB(_gSkbOut).dst_group = 0;	/* not in mcast group */

		/* strncpy(NLMSG_DATA(_gNetLinkHead), data, msg_size); */
		memcpy(nlmsg_data(_gNetLinkHead), data, msg_size);

		res = nlmsg_unicast(_gNetLinkSkb, _gSkbOut, _gPID);
		if (res < 0)
			ipio_err("Failed to send data back to user\n");
	}
}

EXPORT_SYMBOL(netlink_reply_msg);

static void netlink_recv_msg(struct sk_buff *skb)
{
	_gPID = 0;

	ipio_debug(DEBUG_NETLINK, "Netlink is enable = %d\n",
		   core_fr->isEnableNetlink);

	_gNetLinkHead = (struct nlmsghdr *)skb->data;

	ipio_debug(DEBUG_NETLINK, "Received a request from client: %s, %d\n",
		   (char *)NLMSG_DATA(_gNetLinkHead),
		   (int)strlen((char *)NLMSG_DATA(_gNetLinkHead)));

	/* pid of sending process */
	_gPID = _gNetLinkHead->nlmsg_pid;

	ipio_debug(DEBUG_NETLINK, "the pid of sending process = %d\n", _gPID);

	/* TODO: may do something if there's not receiving msg from user. */
	if (_gPID != 0) {
		ipio_err
		    ("The channel of Netlink has been established successfully !\n");
		core_fr->isEnableNetlink = true;
	} else {
		ipio_err
		    ("Failed to establish the channel between kernel and user space\n");
		core_fr->isEnableNetlink = false;
	}
}

static int netlink_init(void)
{
	int res = 0;

#if KERNEL_VERSION(3, 4, 0) > LINUX_VERSION_CODE
	_gNetLinkSkb =
	    netlink_kernel_create(&init_net, NETLINK_USER, netlink_recv_msg,
				  NULL, THIS_MODULE);
#else
	struct netlink_kernel_cfg cfg = {
		.input = netlink_recv_msg,
	};

	_gNetLinkSkb = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
#endif

	ipio_info("Initialise Netlink and create its socket\n");

	if (!_gNetLinkSkb) {
		ipio_err("Failed to create nelink socket\n");
		res = -EFAULT;
	}

	return res;
}

int ilitek_proc_init(void)
{
	int i = 0, res = 0;

	g_user_buf = kcalloc(USER_STR_BUFF, sizeof(char), GFP_KERNEL);

	if (ERR_ALLOC_MEM(g_user_buf)) {
		ipio_err("Failed to allocate buffers for user space\n");
		return -ENOMEM;
	}

	proc_dir_ilitek = proc_mkdir("ilitek", NULL);

	for (; i < ARRAY_SIZE(proc_table); i++) {
		proc_table[i].node =
		    proc_create(proc_table[i].name, 0666, proc_dir_ilitek,
				proc_table[i].fops);

		if (proc_table[i].node == NULL) {
			proc_table[i].isCreated = false;
			ipio_err("Failed to create %s under /proc\n",
				 proc_table[i].name);
			res = -ENODEV;
		} else {
			proc_table[i].isCreated = true;
			ipio_info("Succeed to create %s under /proc\n",
				  proc_table[i].name);
		}
	}

	netlink_init();

	return res;
}

EXPORT_SYMBOL(ilitek_proc_init);


void ilitek_proc_remove(void)
{
	int i = 0;

	for (; i < ARRAY_SIZE(proc_table); i++) {
		if (proc_table[i].isCreated == true) {
			ipio_info("Removed %s under /proc\n",
				  proc_table[i].name);
			remove_proc_entry(proc_table[i].name, proc_dir_ilitek);
		}
	}

	remove_proc_entry("ilitek", NULL);
	netlink_kernel_release(_gNetLinkSkb);
	ipio_kfree((void **)&g_user_buf);
}

EXPORT_SYMBOL(ilitek_proc_remove);


static ssize_t drv_power_on_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	ipio_info("*** %s() ipd->suspended = %d ***\n", __func__, ipd->suspended);

	if (ipd->suspended == true)
		return snprintf(pBuf, PAGE_SIZE, "0\n");
	else
		return snprintf(pBuf, PAGE_SIZE, "1\n");
}
static DEVICE_ATTR(poweron, 0444, drv_power_on_show, NULL);

static ssize_t drv_product_info_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	ipio_info ("*** %s() productinfo = %s ***\n", __func__, ipd->TP_IC_TYPE);

	return scnprintf(pBuf, PAGE_SIZE, "%s\n", ipd->TP_IC_TYPE);
}
static DEVICE_ATTR(productinfo, 0444, drv_product_info_show, NULL);

static ssize_t drv_force_reflash_store(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
	int temp;
	ipio_info ("*** %s() ***\n", __func__);
	// ipio_info( "pBuf = %s\n", pBuf);
	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));
	memcpy(g_user_buf, pBuf, nSize - 1);

	ipio_info ("%s g_user_buf=%s\n", __func__, g_user_buf);
	temp = katoi(g_user_buf);
	ipio_info ("%s temp=%d\n", __func__, temp);
	if (temp == 1)
		core_firmware->force_upgrad = true;
	else
		core_firmware->force_upgrad = false;

	ipio_info("%s core_firmware->force_upgrad =%d\n", __func__, core_firmware->force_upgrad);
	return nSize;
}
static DEVICE_ATTR(forcereflash, 0220, NULL, drv_force_reflash_store);


static ssize_t drv_flash_prog_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	ipio_info ("*** %s() core_firmware->isUpgrading = %d ***\n", __func__, core_firmware->isUpgrading);

	return scnprintf(pBuf, PAGE_SIZE, "%d\n", core_firmware->isUpgrading);
}
static DEVICE_ATTR(flashprog, 0444, drv_flash_prog_show, NULL);

static ssize_t do_reflash_store(struct device *pDevice, struct device_attribute *pAttr, const char *pBuf, size_t nSize)
{
	int ret;
	char prefix[128] = "ILITEK";
	const char *chip_name = ipd->TP_IC_TYPE;

	//int temp;
	ipio_info ("*** %s() ***\n", __func__);
	// ipio_info( "pBuf = %s\n", pBuf);
	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));
	memcpy(g_user_buf, pBuf, nSize - 1);
	ipio_info ("%s g_user_buf=%s\n", __func__, g_user_buf);

	if (ipd->suspended == true) {
		ipio_err("%s: In suspend state, try again later\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (core_firmware->isUpgrading == true) {
		ipio_err("%s: In FW flashing state, try again later\n", __func__);
		ret = -EINVAL;
		goto exit;
	}

	if (core_firmware->force_upgrad != 1) {
		if (strnstr(g_user_buf, prefix, strnlen(g_user_buf, USER_STR_BUFF * sizeof(unsigned char))) <= 0) {
			ipio_err("%s: FW does not belong to %s\n", __func__, prefix);
			ret = -EINVAL;
			goto exit;
		}
		if (strnstr(g_user_buf, chip_name, strnlen(g_user_buf, USER_STR_BUFF * sizeof(unsigned char))) <= 0) {
			ipio_err("%s: FW does not belong to chip %s\n", __func__, chip_name);
			ret = -EINVAL;
			goto exit;
		}
		ipio_info("%s: FW belong to %s\n", __func__, prefix);
		ipio_err("%s: FW belong to chip %s\n", __func__, chip_name);
	}

	ilitek_platform_disable_irq();

	if (ipd->isEnablePollCheckPower)
		cancel_delayed_work_sync(&ipd->check_power_status_work);

	ret = core_firmware_upgrade(g_user_buf, false);

	ilitek_platform_enable_irq();

	if (ipd->isEnablePollCheckPower)
		queue_delayed_work(ipd->check_power_status_queue,
				   &ipd->check_power_status_work,
				   ipd->work_delay);
	if (ret < 0) {
		core_firmware->update_status = ret;
		ipio_err("Failed to upgrade firwmare\n");
	} else {
		core_firmware->update_status = 100;
		ipio_info("Succeed to upgrade firmware\n");
	}


	core_firmware->isUpgrading = false;
exit:
	return nSize;
}

static DEVICE_ATTR(doreflash, 0220, NULL, do_reflash_store);

static ssize_t drv_build_id_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	ipio_info ("*** %s() Fw Version = V%d.%d.%d.%d***\n", __func__, core_config->firmware_ver[1], core_config->firmware_ver[2], core_config->firmware_ver[3], core_config->firmware_ver[4]);

	return scnprintf(pBuf, PAGE_SIZE, "%02x-%02x\n", core_config->firmware_ver[2], core_config->firmware_ver[3]);
}
static DEVICE_ATTR(buildid, 0444, drv_build_id_show, NULL);

static const struct attribute *dev_attrs_list[] = {
	&dev_attr_poweron.attr,
	&dev_attr_productinfo.attr,
	&dev_attr_forcereflash.attr,
	&dev_attr_flashprog.attr,
	&dev_attr_doreflash.attr,
	&dev_attr_buildid.attr,
	NULL
};

static ssize_t path_show(struct device *pDevice, struct device_attribute *pAttr, char *pBuf)
{
	ssize_t blen;
	const char *path;

	path = kobject_get_path(&ipd->client->dev.kobj, GFP_KERNEL);
	blen = scnprintf(pBuf, PAGE_SIZE, "%s\n", path ? path : "na");
	kfree(path);

	return blen;
}

/* Attribute: vendor (RO) */
static ssize_t vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ipio_info("*** %s() vendor = %s ***\n", __func__, "ilitek");
	return scnprintf(buf, PAGE_SIZE, "ilitek");
}


static struct device_attribute touchscreen_attributes[] = {
	__ATTR_RO(path),
	__ATTR_RO(vendor),
	__ATTR_NULL
};

int ilitek_sys_init(void)
{
	int i;
	s32 ret = 0;
	dev_t devno;
	struct device_attribute *attrs = touchscreen_attributes;

	ret = alloc_chrdev_region(&devno, 0, 1, ipd->TP_IC_TYPE);
	if (ret) {
		ipio_err ("can't allocate chrdev\n");
		return ret;
	} else {

		/* set sysfs for firmware */
		touchscreen_class = class_create(THIS_MODULE, "touchscreen");
		if (IS_ERR(touchscreen_class)) {
			ret = PTR_ERR(touchscreen_class);
			touchscreen_class = NULL;
			ipio_err("Failed to create touchscreen class!\n");
			return ret;
		}

		touchscreen_class_dev = device_create(touchscreen_class, NULL, devno, NULL, ipd->TP_IC_TYPE);
		pr_info(" touchscreen_class_dev = %p \n", touchscreen_class_dev);
		if (IS_ERR(touchscreen_class_dev)) {
			ret = PTR_ERR(touchscreen_class_dev);
			touchscreen_class_dev = NULL;
			ipio_err("Failed to create device(touchscreen_class_dev)!\n");
			return ret;
		}
		ipio_info("Succeed to create device(touchscreen_class_dev)!\n");

		for (i = 0; attrs[i].attr.name != NULL; ++i) {
			ret = device_create_file(touchscreen_class_dev, &attrs[i]);
			if (ret < 0)
				goto device_destroy;
		}

		ret = sysfs_create_files(&ipd->client->dev.kobj, dev_attrs_list);
		if (ret < 0) {
			ipio_info("Fail to create dev_attrs_list files)!\n");
			goto device_destroy;
		}

		ipio_info("Succeed to sysfs create files)!\n");
	}

	return ret;

device_destroy:
	for (--i; i >= 0; --i)
		device_remove_file(touchscreen_class_dev, &attrs[i]);

	touchscreen_class_dev = NULL;
	class_unregister(touchscreen_class);
	ipio_err("error creating touchscreen class\n");

	return -ENODEV;
}

EXPORT_SYMBOL(ilitek_sys_init);

void ilitek_sys_remove(void)
{
	int i;
	struct device_attribute *attrs = touchscreen_attributes;

	sysfs_remove_files(&ipd->client->dev.kobj, dev_attrs_list);

	for (i = 0; attrs[i].attr.name != NULL; ++i)
		device_remove_file(touchscreen_class_dev, &attrs[i]);


	device_unregister(touchscreen_class_dev);

	class_unregister(touchscreen_class);
}

EXPORT_SYMBOL(ilitek_sys_remove);
