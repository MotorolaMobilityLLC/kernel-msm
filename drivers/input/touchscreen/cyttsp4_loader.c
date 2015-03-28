/* add cypress new driver ttda-02.03.01.476713 */
/* Open auto upgrate firmware ,add some necessary log */
/* add the log dynamic control */

/*
 * cyttsp4_loader.c
 * Cypress TrueTouch(TM) Standard Product V4 FW loader module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2009-2012 Cypress Semiconductor, Inc.
 * Copyright (C) 2011 Motorola Mobility, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#include <asm/unaligned.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/firmware.h>
#include <linux/cyttsp4_bus.h>
#include <linux/cyttsp4_core.h>
#include "cyttsp4_regs.h"
//#include <misc/app_info.h> //hyuc
#include "cyttsp4_device_access.h"
#include <linux/hw_tp_common.h>

#define CYTTSP4_LOADER_NAME "cyttsp4_loader"
#define CYTTSP4_AUTO_LOAD_FOR_CORRUPTED_FW 1

#define CYTTSP4_FW_UPGRADE \
	(defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE) \
	|| defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE))

#define CYTTSP4_TTCONFIG_UPGRADE \
	(defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE) \
	|| defined(CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_MANUAL_TTCONFIG_UPGRADE))

/* Timeout values in ms. */
#define CY_CMD_TIMEOUT					500
#define CY_CMD_LDR_INIT_TIMEOUT				10000
#define CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT		5000
#define CY_UPGRADE_FROM_BUILDIN_TIMEOUT		5000
#define CY_LDR_UPDATE_WAIT_TIMEOUT				2000

#define CY_CMD_BYTE					1
#define CY_STATUS_BYTE					1
#define CY_MAX_STATUS_SIZE				32
#define CY_START_OF_PACKET				0x01
#define CY_END_OF_PACKET				0x17
#define CY_DATA_ROW_SIZE				288
#define CY_DATA_ROW_SIZE_TMA400				128
#define CY_PACKET_DATA_LEN				96
#define CY_MAX_PACKET_LEN				512
#define CY_COMM_BUSY					0xFF
#define CY_CMD_BUSY					0xFE
#define CY_ARRAY_ID_OFFSET				0
#define CY_ROW_NUM_OFFSET				1
#define CY_ROW_SIZE_OFFSET				3
#define CY_ROW_DATA_OFFSET				5
#define CY_CMD_LDR_HOST_SYNC				0xFF	/* tma400 */
#define CY_CMD_LDR_EXIT					0x3B
#define CY_CMD_LDR_EXIT_CMD_SIZE			7
#define CY_CMD_LDR_EXIT_STAT_SIZE			7
#define CY_CMD_LDR_ENTER				0x38
#define CY_CMD_LDR_ENTER_CMD_SIZE			7
#define CY_CMD_LDR_ENTER_STAT_SIZE			15
#define CY_CMD_LDR_INIT					0x48
#define CY_CMD_LDR_INIT_CMD_SIZE			15
#define CY_CMD_LDR_INIT_STAT_SIZE			7
#define CY_CMD_LDR_ERASE_ROW				0x34
#define CY_CMD_LDR_ERASE_ROW_CMD_SIZE			10
#define CY_CMD_LDR_ERASE_ROW_STAT_SIZE			7
#define CY_CMD_LDR_SEND_DATA				0x37
#define CY_CMD_LDR_SEND_DATA_CMD_SIZE			4	/* hdr bytes only */
#define CY_CMD_LDR_SEND_DATA_STAT_SIZE			8
#define CY_CMD_LDR_PROG_ROW				0x39
#define CY_CMD_LDR_PROG_ROW_CMD_SIZE			7	/* hdr bytes only */
#define CY_CMD_LDR_PROG_ROW_STAT_SIZE			7
#define CY_CMD_LDR_VERIFY_ROW				0x3A
#define CY_CMD_LDR_VERIFY_ROW_STAT_SIZE			8
#define CY_CMD_LDR_VERIFY_ROW_CMD_SIZE			10
#define CY_CMD_LDR_VERIFY_CHKSUM			0x31
#define CY_CMD_LDR_VERIFY_CHKSUM_CMD_SIZE		7
#define CY_CMD_LDR_VERIFY_CHKSUM_STAT_SIZE		8

#define CY_VER_MASK 0xff
struct cyttsp4_loader_data {
	struct cyttsp4_device *ttsp;
	struct cyttsp4_sysinfo *si;
	u8 status_buf[CY_MAX_STATUS_SIZE];
	struct completion int_running;
	struct completion sysinfo_update;	//use to upgrade config
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE
	struct completion builtin_bin_fw_complete;
	int builtin_bin_fw_status;
#endif
#ifdef CONFIG_TOUCHSCREEN_HUAWEI_CYTTSP4_RECOVERY_FW_UPDATE
	int recovery_builtin_bin_fw_status;
#endif
	/* define this as delay work */
	struct delayed_work fw_and_config_upgrade;
	struct work_struct calibration_work;
	struct cyttsp4_loader_platform_data *loader_pdata;
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_MANUAL_TTCONFIG_UPGRADE
	struct mutex config_lock;
	u8 *config_data;
	int config_size;
	bool config_loading;
#endif
};
static char touch_info[50] = { 0 };

struct cyttsp4_dev_id {
	u32 silicon_id;
	u8 rev_id;
	u32 bl_ver;
};

enum ldr_status {
	ERROR_SUCCESS = 0,
	ERROR_COMMAND = 1,
	ERROR_FLASH_ARRAY = 2,
	ERROR_PACKET_DATA = 3,
	ERROR_PACKET_LEN = 4,
	ERROR_PACKET_CHECKSUM = 5,
	ERROR_FLASH_PROTECTION = 6,
	ERROR_FLASH_CHECKSUM = 7,
	ERROR_VERIFY_IMAGE = 8,
	ERROR_UKNOWN1 = 9,
	ERROR_UKNOWN2 = 10,
	ERROR_UKNOWN3 = 11,
	ERROR_UKNOWN4 = 12,
	ERROR_UKNOWN5 = 13,
	ERROR_UKNOWN6 = 14,
	ERROR_INVALID_COMMAND = 15,
	ERROR_INVALID
};

#if CYTTSP4_FW_UPGRADE || CYTTSP4_TTCONFIG_UPGRADE
/*
 * return code:
 * -1: Firmware version compared is older
 *  0: Firmware version compared is identical
 *  1: Firmware version compared is newer
 */
static int cyttsp4_check_firmware_version(struct cyttsp4_device *ttsp,
					  u32 fw_ver_new, u32 fw_revctrl_new_h,
					  u32 fw_revctrl_new_l)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	u32 fw_ver_img;
	u32 fw_revctrl_img_h;
	u32 fw_revctrl_img_l;

	fw_ver_img = data->si->si_ptrs.cydata->fw_ver_major << 8;
	fw_ver_img += data->si->si_ptrs.cydata->fw_ver_minor;

	tp_log_info("%s: img vers:0x%04X new vers:0x%04X\n", __func__,
		    fw_ver_img, fw_ver_new);

	if (fw_ver_new > fw_ver_img)
		return 1;

	if (fw_ver_new < fw_ver_img)
		return -1;

	fw_revctrl_img_h =
	    be32_to_cpu(*(u32 *) (data->si->si_ptrs.cydata->revctrl + 0));

	tp_log_info("%s: img revctrl_h:0x%04X new revctrl_h:0x%04X\n",
		    __func__, fw_revctrl_img_h, fw_revctrl_new_h);

	if (fw_revctrl_new_h > fw_revctrl_img_h)
		return 1;

	if (fw_revctrl_new_h < fw_revctrl_img_h)
		return -1;

	fw_revctrl_img_l =
	    be32_to_cpu(*(u32 *) (data->si->si_ptrs.cydata->revctrl + 4));

	tp_log_info("%s: img revctrl_l:0x%04X new revctrl_l:0x%04X\n",
		    __func__, fw_revctrl_img_l, fw_revctrl_new_l);

	if (fw_revctrl_new_l > fw_revctrl_img_l)
		return 1;

	if (fw_revctrl_new_l < fw_revctrl_img_l)
		return -1;

	return 0;
}
#endif /* CYTTSP4_FW_UPGRADE || CYTTSP4_TTCONFIG_UPGRADE */

#if CYTTSP4_FW_UPGRADE
static u16 _cyttsp4_compute_crc(struct cyttsp4_device *ttsp, u8 * buf, int size)
{
	u16 crc = 0xffff;
	u16 tmp;
	int i;

	if (size == 0)
		crc = ~crc;
	else {

		do {
			for (i = 0, tmp = 0x00ff & *buf++; i < 8;
			     i++, tmp >>= 1) {
				if ((crc & 0x0001) ^ (tmp & 0x0001))
					crc = (crc >> 1) ^ 0x8408;
				else
					crc >>= 1;
			}
		} while (--size);

		crc = ~crc;
		tmp = crc;
		crc = (crc << 8) | (tmp >> 8 & 0xFF);
	}

	return crc;
}

static u16 _cyttsp4_get_short(u8 * buf)
{
	return ((u16) (*buf) << 8) + *(buf + 1);
}

static u8 *_cyttsp4_get_row(struct cyttsp4_device *ttsp,
			    u8 * row_buf, u8 * image_buf, int size)
{
	memcpy(row_buf, image_buf, size);
	image_buf = image_buf + size;
	return image_buf;
}

static int _cyttsp4_get_status(struct cyttsp4_device *ttsp,
			       u8 * buf, int size, unsigned long timeout_ms)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);

	unsigned long uretval;
	int tries;
	int retval = 0;

	if (timeout_ms != 0) {
		/* wait until status ready interrupt or timeout occurs */
		uretval =
		    wait_for_completion_timeout(&data->int_running,
						msecs_to_jiffies(timeout_ms));

		/* TODO: Reconsider purpose of having retries here */
		for (tries = 0; tries < 5; tries++) {
			retval = cyttsp4_read(ttsp, CY_MODE_BOOTLOADER,
					      CY_REG_BASE, buf, size);
			/*
			 * retry if bus read error or
			 * status byte shows not ready
			 */
			if ((retval < 0) || buf[1] == CY_COMM_BUSY
			    || buf[1] == CY_CMD_BUSY
			    || (buf[0] != CY_START_OF_PACKET))
				//if (buf[1] == CY_COMM_BUSY || buf[1] == CY_CMD_BUSY)
				msleep(20);	/* TODO: Constant if code kept */
			else
				break;
		}
		tp_log_debug("%s: tries=%d ret=%d status=%02X\n",
			     __func__, tries, retval, buf[1]);
	}

	return retval;
}

static int _cyttsp4_send_cmd(struct cyttsp4_device *ttsp, const u8 * cmd_buf,
			     int cmd_size, u8 * stat_ret, u32 num_stat_byte,
			     u32 status_size, unsigned long timeout_ms)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);

	u8 *status_buf = data->status_buf;
	int retval = 0;

	if (cmd_buf == NULL)
		goto _cyttsp4_send_cmd_exit;

	if (cmd_size == 0)
		goto _cyttsp4_send_cmd_exit;

	if (timeout_ms > 0)
		INIT_COMPLETION(data->int_running);
	retval = cyttsp4_write(ttsp, CY_MODE_BOOTLOADER,
			       CY_REG_BASE, cmd_buf, cmd_size);
	if (retval < 0) {
		tp_log_err("%s: Fail writing command=%02X\n",
			   __func__, cmd_buf[CY_CMD_BYTE]);
		goto _cyttsp4_send_cmd_exit;
	}

	if (timeout_ms > 0) {
		memset(status_buf, 0, sizeof(data->status_buf));
		retval = _cyttsp4_get_status(ttsp, status_buf,
					     status_size, timeout_ms);
		if (retval < 0 || status_buf[0] != CY_START_OF_PACKET) {
			tp_log_err("%s: Error getting status r=%d"
				   " status_buf[0]=%02X\n",
				   __func__, retval, status_buf[0]);
			if (!(retval < 0))
				retval = -EIO;
			goto _cyttsp4_send_cmd_exit;
		} else {
			if (status_buf[CY_STATUS_BYTE] != ERROR_SUCCESS) {
				tp_log_err("%s: Status=0x%02X error\n",
					   __func__,
					   status_buf[CY_STATUS_BYTE]);
				retval = -EIO;
			} else if (stat_ret != NULL) {
				if (num_stat_byte < status_size)
					*stat_ret = status_buf[num_stat_byte];
				else
					*stat_ret = 0;
			}
		}
	} else {
		if (stat_ret != NULL)
			*stat_ret = ERROR_SUCCESS;
	}

_cyttsp4_send_cmd_exit:
	return retval;
}

static int _cyttsp4_ldr_enter(struct cyttsp4_device *ttsp,
			      struct cyttsp4_dev_id *dev_id)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);

	u16 crc = 0;
	int i = 0;
	u32 cmd_size = 0;
	u8 *status_buf = &data->status_buf[0];
	u8 status = 0;
	int retval;
	/* +1 for TMA400 host sync byte */
	u8 ldr_enter_cmd[CY_CMD_LDR_ENTER_CMD_SIZE + 1];
	memset(status_buf, 0, sizeof(data->status_buf));
	dev_id->bl_ver = 0;
	dev_id->rev_id = 0;
	dev_id->silicon_id = 0;

	ldr_enter_cmd[i++] = CY_CMD_LDR_HOST_SYNC;
	ldr_enter_cmd[i++] = CY_START_OF_PACKET;
	ldr_enter_cmd[i++] = CY_CMD_LDR_ENTER;
	ldr_enter_cmd[i++] = 0x00;	/* data len lsb */
	ldr_enter_cmd[i++] = 0x00;	/* data len msb */
	crc = _cyttsp4_compute_crc(ttsp, &ldr_enter_cmd[1], i - 1);
	cmd_size = sizeof(ldr_enter_cmd);
	ldr_enter_cmd[i++] = (u8) crc;
	ldr_enter_cmd[i++] = (u8) (crc >> 8);
	ldr_enter_cmd[i++] = CY_END_OF_PACKET;

	INIT_COMPLETION(data->int_running);

	retval = cyttsp4_write(ttsp, CY_MODE_BOOTLOADER,
			       CY_REG_BASE, ldr_enter_cmd, cmd_size);
	if (retval < 0) {
		tp_log_err("%s: write block failed %d\n", __func__, retval);
		return retval;
	}
	retval = _cyttsp4_get_status(ttsp, status_buf,
				     CY_CMD_LDR_ENTER_STAT_SIZE,
				     CY_CMD_TIMEOUT);

	if (retval < 0) {
		tp_log_err("%s: Fail get status to Enter Loader command r=%d\n",
			   __func__, retval);
		return retval;
	}
	status = status_buf[CY_STATUS_BYTE];
	if (status == ERROR_SUCCESS) {
		dev_id->bl_ver =
		    status_buf[11] << 16 |
		    status_buf[10] << 8 | status_buf[9] << 0;
		dev_id->rev_id = status_buf[8] << 0;
		dev_id->silicon_id =
		    status_buf[7] << 24 |
		    status_buf[6] << 16 |
		    status_buf[5] << 8 | status_buf[4] << 0;
		retval = 0;
	} else
		retval = -EIO;
	tp_log_debug("%s: status=%d "
		     "bl_ver=%08X rev_id=%02X silicon_id=%08X\n",
		     __func__, status,
		     dev_id->bl_ver, dev_id->rev_id, dev_id->silicon_id);

	return retval;
}

static int _cyttsp4_ldr_init(struct cyttsp4_device *ttsp)
{
	u16 crc;
	int i = 0;
	int retval = 0;
	const u8 *cyttsp4_security_key;
	int key_size;
	/* +1 for TMA400 host sync byte */
	u8 ldr_init_cmd[CY_CMD_LDR_INIT_CMD_SIZE + 1];

	cyttsp4_security_key = cyttsp4_get_security_key(ttsp, &key_size);

	ldr_init_cmd[i++] = CY_CMD_LDR_HOST_SYNC;
	ldr_init_cmd[i++] = CY_START_OF_PACKET;
	ldr_init_cmd[i++] = CY_CMD_LDR_INIT;
	ldr_init_cmd[i++] = 0x08;	/* data len lsb */
	ldr_init_cmd[i++] = 0x00;	/* data len msb */
	memcpy(&ldr_init_cmd[i], cyttsp4_security_key, key_size);
	i += key_size;
	crc = _cyttsp4_compute_crc(ttsp, &ldr_init_cmd[1], i - 1);
	ldr_init_cmd[i++] = (u8) crc;
	ldr_init_cmd[i++] = (u8) (crc >> 8);
	ldr_init_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp4_send_cmd(ttsp, ldr_init_cmd, i, NULL, 0,
				   CY_CMD_LDR_INIT_STAT_SIZE,
				   CY_CMD_LDR_INIT_TIMEOUT);
	if (retval < 0) {
		tp_log_err("%s: Fail ldr init r=%d\n", __func__, retval);
	}

	return retval;
}

struct cyttsp4_hex_image {
	u8 array_id;
	u16 row_num;
	u16 row_size;
	u8 row_data[CY_DATA_ROW_SIZE];
} __packed;

static int _cyttsp4_ldr_parse_row(struct cyttsp4_device *ttsp, u8 * row_buf,
				  struct cyttsp4_hex_image *row_image)
{
	int retval = 0;

	row_image->array_id = row_buf[CY_ARRAY_ID_OFFSET];
	row_image->row_num = _cyttsp4_get_short(&row_buf[CY_ROW_NUM_OFFSET]);
	row_image->row_size = _cyttsp4_get_short(&row_buf[CY_ROW_SIZE_OFFSET]);

	if (row_image->row_size > ARRAY_SIZE(row_image->row_data)) {
		tp_log_err("%s: row data buffer overflow\n", __func__);
		retval = -EOVERFLOW;
		goto cyttsp4_ldr_parse_row_exit;
	}

	memcpy(row_image->row_data, &row_buf[CY_ROW_DATA_OFFSET],
	       row_image->row_size);
cyttsp4_ldr_parse_row_exit:
	return retval;
}

static int _cyttsp4_ldr_prog_row(struct cyttsp4_device *ttsp,
				 struct cyttsp4_hex_image *row_image)
{
	u16 crc;
	int next;
	int data;
	int row_data;
	u16 row_sum;
	u32 data_len;
	int retval = 0;

	u8 *cmd = kzalloc(CY_MAX_PACKET_LEN, GFP_KERNEL);

	if (cmd != NULL) {
		row_data = 0;
		row_sum = 0;
		next = 0;
		cmd[next++] = CY_CMD_LDR_HOST_SYNC;
		cmd[next++] = CY_START_OF_PACKET;
		cmd[next++] = CY_CMD_LDR_PROG_ROW;
		/*
		 * include array id size and row id size in CY_PACKET_DATA_LEN
		 */
		data_len = CY_DATA_ROW_SIZE_TMA400;
		cmd[next++] = (u8) (data_len + 3);
		cmd[next++] = (u8) ((data_len + 3) >> 8);
		cmd[next++] = row_image->array_id;
		cmd[next++] = (u8) row_image->row_num;
		cmd[next++] = (u8) (row_image->row_num >> 8);

		for (data = 0; data < data_len; data++) {
			cmd[next] = row_image->row_data[row_data++];
			row_sum += cmd[next];
			next++;
		}

		crc = _cyttsp4_compute_crc(ttsp, &cmd[1], next - 1);
		cmd[next++] = (u8) crc;
		cmd[next++] = (u8) (crc >> 8);
		cmd[next++] = CY_END_OF_PACKET;

		retval = _cyttsp4_send_cmd(ttsp, cmd, next, NULL, 0,
					   CY_CMD_LDR_PROG_ROW_STAT_SIZE,
					   CY_CMD_TIMEOUT);

		if (retval < 0) {
			tp_log_err("%s: prog row=%d fail r=%d\n",
				   __func__, row_image->row_num, retval);
			goto cyttsp4_ldr_prog_row_exit;
		}

	} else {
		tp_log_err("%s prog row error - cmd buf is NULL\n", __func__);
		retval = -EIO;
	}

cyttsp4_ldr_prog_row_exit:
	kfree(cmd);
	return retval;
}

static int _cyttsp4_ldr_verify_row(struct cyttsp4_device *ttsp,
				   struct cyttsp4_hex_image *row_image)
{
	u16 crc = 0;
	int i = 0;
	u8 verify_checksum;
	int retval = 0;
	/* +1 for TMA400 host sync byte */
	u8 ldr_verify_row_cmd[CY_CMD_LDR_VERIFY_ROW_CMD_SIZE + 1];

	ldr_verify_row_cmd[i++] = CY_CMD_LDR_HOST_SYNC;
	ldr_verify_row_cmd[i++] = CY_START_OF_PACKET;
	ldr_verify_row_cmd[i++] = CY_CMD_LDR_VERIFY_ROW;
	ldr_verify_row_cmd[i++] = 0x03;	/* data len lsb */
	ldr_verify_row_cmd[i++] = 0x00;	/* data len msb */
	ldr_verify_row_cmd[i++] = row_image->array_id;
	ldr_verify_row_cmd[i++] = (u8) row_image->row_num;
	ldr_verify_row_cmd[i++] = (u8) (row_image->row_num >> 8);
	crc = _cyttsp4_compute_crc(ttsp, &ldr_verify_row_cmd[1], i - 1);
	ldr_verify_row_cmd[i++] = (u8) crc;
	ldr_verify_row_cmd[i++] = (u8) (crc >> 8);
	ldr_verify_row_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp4_send_cmd(ttsp, ldr_verify_row_cmd, i,
				   &verify_checksum, 4,
				   CY_CMD_LDR_VERIFY_ROW_STAT_SIZE,
				   CY_CMD_TIMEOUT);

	if (retval < 0) {
		tp_log_err("%s: verify row=%d fail r=%d\n",
			   __func__, row_image->row_num, retval);
	}

	return retval;
}

static int _cyttsp4_ldr_verify_chksum(struct cyttsp4_device *ttsp,
				      u8 * app_chksum)
{
	u16 crc = 0;
	int i = 0;
	int retval = 0;
	/* +1 for TMA400 host sync byte */
	u8 ldr_verify_chksum_cmd[CY_CMD_LDR_VERIFY_CHKSUM_CMD_SIZE + 1];

	ldr_verify_chksum_cmd[i++] = CY_CMD_LDR_HOST_SYNC;
	ldr_verify_chksum_cmd[i++] = CY_START_OF_PACKET;
	ldr_verify_chksum_cmd[i++] = CY_CMD_LDR_VERIFY_CHKSUM;
	ldr_verify_chksum_cmd[i++] = 0x00;	/* data len lsb */
	ldr_verify_chksum_cmd[i++] = 0x00;	/* data len msb */
	crc = _cyttsp4_compute_crc(ttsp, &ldr_verify_chksum_cmd[1], i - 1);
	ldr_verify_chksum_cmd[i++] = (u8) crc;
	ldr_verify_chksum_cmd[i++] = (u8) (crc >> 8);
	ldr_verify_chksum_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp4_send_cmd(ttsp, ldr_verify_chksum_cmd, i,
				   app_chksum, 4,
				   CY_CMD_LDR_VERIFY_CHKSUM_STAT_SIZE,
				   CY_CMD_TIMEOUT);

	if (retval < 0) {
		tp_log_err("%s: verify checksum fail r=%d\n", __func__, retval);
	}

	return retval;
}

/* Constructs loader exit command and sends via _cyttsp4_send_cmd() */
static int _cyttsp4_ldr_exit(struct cyttsp4_device *ttsp)
{
	u16 crc = 0;
	int i = 0;
	int retval;
	/* +1 for TMA400 host sync byte */
	u8 ldr_exit_cmd[CY_CMD_LDR_EXIT_CMD_SIZE + 1];

	ldr_exit_cmd[i++] = CY_CMD_LDR_HOST_SYNC;
	ldr_exit_cmd[i++] = CY_START_OF_PACKET;
	ldr_exit_cmd[i++] = CY_CMD_LDR_EXIT;
	ldr_exit_cmd[i++] = 0x00;	/* data len lsb */
	ldr_exit_cmd[i++] = 0x00;	/* data len msb */
	crc = _cyttsp4_compute_crc(ttsp, &ldr_exit_cmd[1], i - 1);
	ldr_exit_cmd[i++] = (u8) crc;
	ldr_exit_cmd[i++] = (u8) (crc >> 8);
	ldr_exit_cmd[i++] = CY_END_OF_PACKET;

	retval = _cyttsp4_send_cmd(ttsp, ldr_exit_cmd, i, NULL, 0,
				   CY_CMD_LDR_EXIT_STAT_SIZE, 0);

	if (retval < 0) {
		tp_log_err("%s: BL Loader exit fail r=%d\n", __func__, retval);
	}

	tp_log_debug("%s: Exit BL Loader r=%d\n", __func__, retval);

	return retval;
}

static int _cyttsp4_load_app(struct cyttsp4_device *ttsp, const u8 * fw,
			     int fw_size)
{
	u8 *p;
	int ret;
	int retval;		/* need separate return value at exit stage */
	struct cyttsp4_dev_id *file_id = NULL;
	struct cyttsp4_dev_id *dev_id = NULL;
	struct cyttsp4_hex_image *row_image = NULL;
	u8 app_chksum;

	u8 *row_buf = NULL;
	/* Prevent loading if TMA ver not defined. */
	u32 image_rec_size = fw_size + 1;
	u32 row_buf_size = 1024 > CY_MAX_PRBUF_SIZE ? 1024 : CY_MAX_PRBUF_SIZE;
	int row_count = 0;

	image_rec_size = CY_DATA_ROW_SIZE_TMA400 +
	    (sizeof(struct cyttsp4_hex_image) - CY_DATA_ROW_SIZE);
	if (fw_size % image_rec_size != 0) {
		tp_log_err("%s: Firmware image is misaligned\n", __func__);
		retval = -EINVAL;
		goto _cyttsp4_load_app_exit;
	}

	tp_log_info("%s: start load app\n", __func__);

	row_buf = kzalloc(row_buf_size, GFP_KERNEL);
	row_image = kzalloc(sizeof(struct cyttsp4_hex_image), GFP_KERNEL);
	file_id = kzalloc(sizeof(struct cyttsp4_dev_id), GFP_KERNEL);
	dev_id = kzalloc(sizeof(struct cyttsp4_dev_id), GFP_KERNEL);
	if (row_buf == NULL || row_image == NULL ||
	    file_id == NULL || dev_id == NULL) {
		tp_log_err("%s: Unable to alloc row buffers(%p %p %p %p)\n",
			   __func__, row_buf, row_image, file_id, dev_id);
		retval = -ENOMEM;
		goto _cyttsp4_load_app_exit;
	}

	cyttsp4_request_stop_wd(ttsp);

	p = (u8 *) fw;
	/* Enter Loader and return Silicon ID and Rev */

	retval = cyttsp4_request_reset(ttsp);
	if (retval < 0) {
		tp_log_err("%s: Fail reset device r=%d\n", __func__, retval);
		goto _cyttsp4_load_app_exit;
	}

	tp_log_info("%s: Send BL Loader Enter\n", __func__);

	retval = _cyttsp4_ldr_enter(ttsp, dev_id);
	if (retval < 0) {
		tp_log_err("%s: Error cannot start Loader (ret=%d)\n",
			   __func__, retval);
		goto _cyttsp4_load_app_exit;
	}

	tp_log_debug("%s: dev: silicon id=%08X rev=%02X bl=%08X\n",
		     __func__, dev_id->silicon_id,
		     dev_id->rev_id, dev_id->bl_ver);

	udelay(1000);
	retval = _cyttsp4_ldr_init(ttsp);
	if (retval < 0) {
		tp_log_err("%s: Error cannot init Loader (ret=%d)\n",
			   __func__, retval);
		goto _cyttsp4_load_app_exit;
	}

	tp_log_info("%s: Send BL Loader Blocks\n", __func__);

	while (p < (fw + fw_size)) {
		/* Get row */
		tp_log_debug("%s: read row=%d\n", __func__, ++row_count);
		memset(row_buf, 0, row_buf_size);
		p = _cyttsp4_get_row(ttsp, row_buf, p, image_rec_size);

		/* Parse row */
		tp_log_debug("%s: p=%p buf=%p buf[0]=%02X\n", __func__,
			     p, row_buf, row_buf[0]);
		retval = _cyttsp4_ldr_parse_row(ttsp, row_buf, row_image);
		tp_log_debug("%s: array_id=%02X row_num=%04X(%d)"
			     " row_size=%04X(%d)\n", __func__,
			     row_image->array_id,
			     row_image->row_num, row_image->row_num,
			     row_image->row_size, row_image->row_size);
		if (retval < 0) {
			tp_log_err("%s: Parse Row Error "
				   "(a=%d r=%d ret=%d\n",
				   __func__, row_image->array_id,
				   row_image->row_num, retval);
			goto bl_exit;
		} else {
			tp_log_debug("%s: Parse Row "
				     "(a=%d r=%d ret=%d\n",
				     __func__, row_image->array_id,
				     row_image->row_num, retval);
		}

		/* program row */
		retval = _cyttsp4_ldr_prog_row(ttsp, row_image);
		if (retval < 0) {
			tp_log_err("%s: Program Row Error "
				   "(array=%d row=%d ret=%d)\n",
				   __func__, row_image->array_id,
				   row_image->row_num, retval);
			goto _cyttsp4_load_app_exit;
		}

		/* the irq for programming may not complete, so wait 1 ms for it */
		mdelay(1);

		/* verify row */
		retval = _cyttsp4_ldr_verify_row(ttsp, row_image);
		if (retval < 0) {
			tp_log_err("%s: Verify Row Error "
				   "(array=%d row=%d ret=%d)\n",
				   __func__, row_image->array_id,
				   row_image->row_num, retval);
			goto _cyttsp4_load_app_exit;
		}

		tp_log_debug("%s: array=%d row_cnt=%d row_num=%04X\n",
			     __func__, row_image->array_id, row_count,
			     row_image->row_num);
	}

	/* verify app checksum */
	retval = _cyttsp4_ldr_verify_chksum(ttsp, &app_chksum);
	tp_log_debug("%s: Application Checksum = %02X r=%d\n",
		     __func__, app_chksum, retval);
	if (retval < 0) {
		tp_log_err("%s: ldr_verify_chksum fail r=%d\n", __func__,
			   retval);
		retval = 0;
	}

	/* exit loader */
bl_exit:
	tp_log_info("%s: Send BL Loader Terminate\n", __func__);
	ret = _cyttsp4_ldr_exit(ttsp);
	if (ret) {
		tp_log_err("%s: Error on exit Loader (ret=%d)\n",
			   __func__, ret);
		retval = ret;
	}

_cyttsp4_load_app_exit:
	kfree(row_buf);
	kfree(row_image);
	kfree(file_id);
	kfree(dev_id);
	return retval;
}

static void cyttsp4_fw_calibrate(struct work_struct *calibration_work)
{
	struct cyttsp4_loader_data *data = container_of(calibration_work,
							struct
							cyttsp4_loader_data,
							calibration_work);
	struct cyttsp4_device *ttsp = data->ttsp;
	struct device *dev = &ttsp->dev;
	u8 cmd_buf[CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ] = { 0 };
	u8 return_buf[CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ] = { 0 };
	int rc;

	tp_log_debug("%s\n", __func__);

	pm_runtime_get_sync(dev);

	tp_log_debug("%s: Requesting exclusive\n", __func__);
	rc = cyttsp4_request_exclusive(ttsp, CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: Error on request exclusive r=%d\n",
			   __func__, rc);
		goto exit;
	}

	tp_log_debug("%s: Requesting mode change to CAT\n", __func__);
	rc = cyttsp4_request_set_mode(ttsp, CY_MODE_CAT);
	if (rc < 0) {
		tp_log_err("%s: Error on request set mode r=%d\n",
			   __func__, rc);
		goto exit_release;
	}

	tp_log_debug("%s: Calibrating for Mutual Capacitance Screen\n",
		     __func__);
	cmd_buf[0] = CY_CMD_CAT_CALIBRATE_IDACS;
	cmd_buf[1] = 0x00;	/* Mutual Capacitance Screen */
	rc = cyttsp4_request_exec_cmd(ttsp, CY_MODE_CAT,
				      cmd_buf, CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ,
				      return_buf,
				      CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ,
				      CY_CALIBRATE_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: Unable to execute calibrate command.\n",
			   __func__);
		goto exit_setmode;
	}
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS) {
		tp_log_err("%s: calibrate command unsuccessful\n", __func__);
		goto exit_setmode;
	}

	tp_log_debug("%s: Calibrating for Mutual Capacitance Button\n",
		     __func__);
	cmd_buf[1] = 0x01;	/* Mutual Capacitance Button */
	rc = cyttsp4_request_exec_cmd(ttsp, CY_MODE_CAT,
				      cmd_buf, CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ,
				      return_buf,
				      CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ,
				      CY_CALIBRATE_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: Unable to execute calibrate command.\n",
			   __func__);
		goto exit_setmode;
	}
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS) {
		tp_log_err("%s: calibrate command unsuccessful\n", __func__);
		goto exit_setmode;
	}

	tp_log_debug("%s: Calibrating for Self Capacitance Screen\n", __func__);
	cmd_buf[1] = 0x02;	/* Self Capacitance */
	rc = cyttsp4_request_exec_cmd(ttsp, CY_MODE_CAT,
				      cmd_buf, CY_CMD_CAT_CALIBRATE_IDAC_CMD_SZ,
				      return_buf,
				      CY_CMD_CAT_CALIBRATE_IDAC_RET_SZ,
				      CY_CALIBRATE_COMPLETE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: Unable to execute calibrate command.\n",
			   __func__);
		goto exit_setmode;
	}
	if (return_buf[0] != CY_CMD_STATUS_SUCCESS) {
		tp_log_err("%s: calibrate command unsuccessful\n", __func__);
		goto exit_setmode;
	}

exit_setmode:
	tp_log_debug("%s: Requesting mode change to Operational\n", __func__);
	rc = cyttsp4_request_set_mode(ttsp, CY_MODE_OPERATIONAL);
	if (rc < 0)
		tp_log_err("%s: Error on request set mode 2 r=%d\n",
			   __func__, rc);
	else
		tp_log_debug("%s: Mode changed to Operational\n", __func__);

exit_release:
	rc = cyttsp4_release_exclusive(ttsp);
	if (rc < 0)
		tp_log_err("%s: Error on release exclusive r=%d\n",
			   __func__, rc);

exit:
	pm_runtime_put(dev);
}

static int cyttsp4_fw_calibration_attention(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	int rc = 0;

	tp_log_info("%s\n", __func__);
	complete(&data->sysinfo_update);
	schedule_work(&data->calibration_work);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
				      cyttsp4_fw_calibration_attention, 0);

	return rc;
}

/*Notify upgrade config*/
static int cyttsp4_sysinfo_attention(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_loader_data *data = dev_get_drvdata(&ttsp->dev);
	complete(&data->sysinfo_update);

	cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_STARTUP,
				      cyttsp4_sysinfo_attention, 0);

	return 0;
}


static int cyttsp4_upgrade_firmware(struct cyttsp4_device *ttsp,
				    const u8 * fw_img, int fw_size)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	int rc;

	pm_runtime_get_sync(dev);

	rc = cyttsp4_request_exclusive(ttsp, CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0)
		goto exit;

	rc = _cyttsp4_load_app(ttsp, fw_img, fw_size);
	if (rc < 0) {
		tp_log_err("%s: Firmware update failed with error code %d\n",
			   __func__, rc);
	} else if (data->loader_pdata &&
		   (data->loader_pdata->
		    flags & CY_LOADER_FLAG_CALIBRATE_AFTER_FW_UPGRADE)) {
		/* set up call back for startup */
		tp_log_debug("%s: Adding callback for calibration\n", __func__);
		INIT_COMPLETION(data->sysinfo_update);
		rc = cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
						 cyttsp4_fw_calibration_attention,
						 0);
		if (rc) {
			tp_log_err
			    ("%s: Failed adding callback for calibration\n",
			     __func__);
			tp_log_err("%s: No calibration will be performed\n",
				   __func__);
			rc = 0;
		}
	} else {
		INIT_COMPLETION(data->sysinfo_update);
		cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
					    cyttsp4_sysinfo_attention, 0);
	}			/*completion of the upgrade config */

	rc = cyttsp4_release_exclusive(ttsp);
	rc |= cyttsp4_request_restart(ttsp, false);
	if (rc < 0) {
		tp_log_err
		    ("%s: Failed adding release exclusive and request restart\n",
		     __func__);
		rc = 0;
	}

exit:
	pm_runtime_put(dev);
	return rc;
}

static int cyttsp4_loader_attention(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_loader_data *data = dev_get_drvdata(&ttsp->dev);
	if ((NULL == data) || (NULL == &data->int_running)) {
		tp_log_err("%s: data error,null point!!!\n", __func__);
		return -ENOSYS;
	}
	complete(&data->int_running);
	return 0;
}

#endif /* CYTTSP4_FW_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
static int cyttsp4_check_firmware_version_platform(struct cyttsp4_device *ttsp,
						   struct cyttsp4_touch_firmware
						   *fw)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	u32 fw_ver_new;
	u32 fw_revctrl_new_h;
	u32 fw_revctrl_new_l;
	int upgrade;

	if (!data->si) {
		tp_log_info
		    ("%s: No firmware infomation found, device FW may be corrupted\n",
		     __func__);
		return CYTTSP4_AUTO_LOAD_FOR_CORRUPTED_FW;
	}

	fw_ver_new = get_unaligned_be16(fw->ver + 2);
	fw_revctrl_new_h = get_unaligned_be32(fw->ver + 4);
	fw_revctrl_new_l = get_unaligned_be32(fw->ver + 8);

	upgrade = cyttsp4_check_firmware_version(ttsp, fw_ver_new,
						 fw_revctrl_new_h,
						 fw_revctrl_new_l);

	if (upgrade > 0)
		return 1;

	return 0;
}

static int upgrade_firmware_from_platform(struct cyttsp4_device *ttsp,
					  bool forced)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	struct cyttsp4_touch_firmware *fw;
	int rc = -ENOSYS;
	int upgrade;

	if (data->loader_pdata == NULL) {
		tp_log_err("%s: No loader platform data\n", __func__);
		return rc;
	}

	fw = data->loader_pdata->fw;
	if (fw == NULL || fw->img == NULL || fw->size == 0) {
		tp_log_err("%s: No platform firmware\n", __func__);
		return rc;
	}

	if (fw->ver == NULL || fw->vsize == 0) {
		tp_log_err("%s: No platform firmware version\n", __func__);
		return rc;
	}

	if (forced)
		upgrade = forced;
	else
		upgrade = cyttsp4_check_firmware_version_platform(ttsp, fw);

	if (upgrade)
		return cyttsp4_upgrade_firmware(ttsp, fw->img, fw->size);

	return rc;
}
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE
static void _cyttsp4_firmware_cont(const struct firmware *fw, void *context)
{
	struct cyttsp4_device *ttsp = context;
	u8 header_size = 0;

	if (fw == NULL)
		goto cyttsp4_firmware_cont_exit;

	if (fw->data == NULL || fw->size == 0) {
		tp_log_err("%s: No firmware received\n", __func__);
		goto cyttsp4_firmware_cont_release_exit;
	}

	header_size = fw->data[0];
	if (header_size >= (fw->size + 1)) {
		tp_log_err("%s: Firmware format is invalid\n", __func__);
		goto cyttsp4_firmware_cont_release_exit;
	}

	cyttsp4_upgrade_firmware(ttsp, &(fw->data[header_size + 1]),
				 fw->size - (header_size + 1));

cyttsp4_firmware_cont_release_exit:
	release_firmware(fw);

cyttsp4_firmware_cont_exit:
	return;
}

static int cyttsp4_check_firmware_version_builtin(struct cyttsp4_device *ttsp,
						  const struct firmware *fw)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	u32 fw_ver_new;
	u32 fw_revctrl_new_h;
	u32 fw_revctrl_new_l;
	int upgrade;

	if (!data->si) {
		tp_log_info
		    ("%s: No firmware infomation found, device FW may be corrupted\n",
		     __func__);
		return CYTTSP4_AUTO_LOAD_FOR_CORRUPTED_FW;
	}

	fw_ver_new = get_unaligned_be16(fw->data + 3);
	fw_revctrl_new_h = get_unaligned_be32(fw->data + 5);
	fw_revctrl_new_l = get_unaligned_be32(fw->data + 9);

	upgrade = cyttsp4_check_firmware_version(ttsp, fw_ver_new,
						 fw_revctrl_new_h,
						 fw_revctrl_new_l);

	/* We need to update always when firmware version missmatch */
	tp_log_info("%s: Firmware version compare result is: %d\n", __func__,
		    upgrade);

	if (upgrade > 0) {
		tp_log_info
		    ("%s: Firmware version too old, device need to upgrade!\n",
		     __func__);
		return 1;
	}

	return 0;
}

static void _cyttsp4_firmware_cont_builtin(const struct firmware *fw,
					   void *context)
{
	struct cyttsp4_device *ttsp = context;
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	int upgrade;

	if (fw == NULL) {
		tp_log_info("%s: No builtin firmware\n", __func__);
		goto _cyttsp4_firmware_cont_builtin_exit;
	}

	if (fw->data == NULL || fw->size == 0) {
		tp_log_err("%s: Invalid builtin firmware\n", __func__);
		goto _cyttsp4_firmware_cont_builtin_exit;
	}

	tp_log_info("%s: Found firmware\n", __func__);

	upgrade = cyttsp4_check_firmware_version_builtin(ttsp, fw);
	if (upgrade) {
		tp_log_info("%s:FW upgrade begin!\n", __func__);
		_cyttsp4_firmware_cont(fw, ttsp);
		data->builtin_bin_fw_status = 0;
		complete(&data->builtin_bin_fw_complete);
		return;
	} else {
		tp_log_info
		    ("%s,Firmware upgrade is not necessary,version is identical or older!\n",
		     __func__);
	}

_cyttsp4_firmware_cont_builtin_exit:
	release_firmware(fw);

	data->builtin_bin_fw_status = -EINVAL;
	complete(&data->builtin_bin_fw_complete);
}

static int upgrade_firmware_from_class(struct cyttsp4_device *ttsp)
{
	int retval;

	tp_log_debug("%s: Enabling firmware class loader\n", __func__);

	retval = request_firmware_nowait(THIS_MODULE, FW_ACTION_NOHOTPLUG, "",
					 &ttsp->dev, GFP_KERNEL, ttsp,
					 _cyttsp4_firmware_cont);
	if (retval < 0) {
		tp_log_err("%s: Fail request firmware class file load\n",
			   __func__);
		return retval;
	}

	return 0;
}

static int upgrade_firmware_from_builtin(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	int retval;
	const char *fw_name = data->loader_pdata->firmware_name;	//huawei 0701

	tp_log_debug("%s: Enabling firmware class loader built-in\n", __func__);

	tp_log_err("%s: firmware name is:%s\n", __func__, fw_name);

	retval = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					 //CY_FW_FILE_NAME, dev, GFP_KERNEL, ttsp,//huawei 0701
					 fw_name, dev, GFP_KERNEL, ttsp,	//huawei 0701
					 _cyttsp4_firmware_cont_builtin);
	if (retval < 0) {
		tp_log_err("%s: Fail request firmware class file load\n",
			   __func__);
		return retval;
	}

	/* wait until FW binary upgrade finishes */
	wait_for_completion(&data->builtin_bin_fw_complete);

	return data->builtin_bin_fw_status;
}
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_HUAWEI_CYTTSP4_RECOVERY_FW_UPDATE
static void _cyttsp4_firmware_cont_builtin_recovery(const struct firmware *fw,
						    void *context)
{
	struct cyttsp4_device *ttsp = context;
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);

	if (fw == NULL) {
		tp_log_info("%s: No builtin firmware\n", __func__);
		goto _cyttsp4_firmware_cont_builtin_exit;
	}

	if (fw->data == NULL || fw->size == 0) {
		tp_log_err("%s: Invalid builtin firmware\n", __func__);
		goto _cyttsp4_firmware_cont_builtin_exit;
	}

	tp_log_debug("%s: Found firmware\n", __func__);

	_cyttsp4_firmware_cont(fw, ttsp);
	data->recovery_builtin_bin_fw_status = 0;
	complete(&data->builtin_bin_fw_complete);
	return;

_cyttsp4_firmware_cont_builtin_exit:
	release_firmware(fw);

	data->recovery_builtin_bin_fw_status = -EINVAL;
	complete(&data->builtin_bin_fw_complete);
}

static int upgrade_firmware_from_builtin_recovery(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	int retval;

	tp_log_debug("%s: Enabling firmware class loader built-in\n", __func__);

	tp_log_debug("%s: firmware name is:%s\n", __func__, CY_RC_FW_FILE_NAME);

	retval = request_firmware_nowait(THIS_MODULE, FW_ACTION_HOTPLUG,
					 CY_RC_FW_FILE_NAME, dev, GFP_KERNEL,
					 ttsp,
					 _cyttsp4_firmware_cont_builtin_recovery);
	if (retval < 0) {
		tp_log_err("%s: Fail request firmware class file load\n",
			   __func__);
		return retval;
	}

	/* wait until FW binary upgrade finishes */
	wait_for_completion(&data->builtin_bin_fw_complete);

	return data->recovery_builtin_bin_fw_status;
}
#endif /*CONFIG_TOUCHSCREEN_HUAWEI_CYTTSP4_RECOVERY_FW_UPDATE */

#if CYTTSP4_TTCONFIG_UPGRADE
static int cyttsp4_upgrade_ttconfig(struct cyttsp4_device *ttsp,
				    const u8 * ttconfig_data, int ttconfig_size)
{
	struct device *dev = &ttsp->dev;
	int rc, rc2;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);

	tp_log_debug("%s\n", __func__);

	pm_runtime_get_sync(dev);

	tp_log_debug("%s: Requesting exclusive\n", __func__);
	rc = cyttsp4_request_exclusive(ttsp, CY_LDR_REQUEST_EXCLUSIVE_TIMEOUT);
	if (rc < 0) {
		tp_log_err("%s: Error on request exclusive r=%d\n",
			   __func__, rc);
		goto exit;
	}

	tp_log_debug("%s: Requesting mode change to CAT\n", __func__);
	rc = cyttsp4_request_set_mode(ttsp, CY_MODE_CAT);
	if (rc < 0) {
		tp_log_err("%s: Error on request set mode r=%d\n",
			   __func__, rc);
		goto exit_release;
	}

	rc = cyttsp4_request_write_config(ttsp, CY_TCH_PARM_EBID,
					  0, (u8 *) ttconfig_data,
					  ttconfig_size);
	/* add calibration after upgrade config data successfully */
	if (rc < 0) {
		tp_log_err("%s: Error on request write config r=%d\n",
			   __func__, rc);
		goto exit_setmode;
	} else {
		if (data->loader_pdata &&
		    (data->loader_pdata->flags & CY_FLAG_AUTO_CALIBRATE)) {
			/* set up startup call back */
			tp_log_debug("%s: Adding callback for calibration\n",
				     __func__);
			rc = cyttsp4_subscribe_attention(ttsp, CY_ATTEN_STARTUP,
							 cyttsp4_fw_calibration_attention,
							 0);
			if (rc) {
				tp_log_err
				    ("%s: Failed adding callback for calibration\n",
				     __func__);
				tp_log_err
				    ("%s: No calibration will be performed\n",
				     __func__);
				rc = 0;
			}
		}
	}
exit_setmode:
	rc2 = cyttsp4_request_set_mode(ttsp, CY_MODE_OPERATIONAL);
	if (rc2 < 0)
		tp_log_err("%s: Error on request set mode r=%d\n",
			   __func__, rc2);

exit_release:
	rc2 = cyttsp4_release_exclusive(ttsp);
	if (rc < 0)
		tp_log_err("%s: Error on release exclusive r=%d\n",
			   __func__, rc2);

exit:
	if (!rc)
		cyttsp4_request_restart(ttsp, true);

	pm_runtime_put(dev);

	return rc;
}
#endif /* CYTTSP4_TTCONFIG_UPGRADE */

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE
static int cyttsp4_get_ttconfig_crc(struct cyttsp4_device *ttsp,
				    const u8 * ttconfig_data, int ttconfig_size,
				    u16 * crc)
{
	u16 crc_loc;

	crc_loc = get_unaligned_le16(&ttconfig_data[2]);
	if (ttconfig_size < crc_loc + 2)
		return -EINVAL;

	*crc = get_unaligned_le16(&ttconfig_data[crc_loc]);

	return 0;
}

static int cyttsp4_get_ttconfig_version(struct cyttsp4_device *ttsp,
					const u8 * ttconfig_data,
					int ttconfig_size, u16 * version)
{
	if (ttconfig_size < CY_TTCONFIG_VERSION_OFFSET
	    + CY_TTCONFIG_VERSION_SIZE)
		return -EINVAL;

	*version =
	    get_unaligned_le16(&ttconfig_data[CY_TTCONFIG_VERSION_OFFSET]);

	return 0;
}

static int cyttsp4_check_ttconfig_version(struct cyttsp4_device *ttsp,
					  const u8 * ttconfig_data,
					  int ttconfig_size)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	int rc;

	if (!data->si)
		return 0;

	/* Check if device POST config CRC test failed */
	if (!(data->si->si_ptrs.test->post_codel &
	      CY_POST_CODEL_CFG_DATA_CRC_FAIL)) {
		tp_log_info("%s: Config CRC invalid, will upgrade\n", __func__);
		return 1;
	}

	/* Check for config version */
	if (data->loader_pdata->flags & CY_LOADER_FLAG_CHECK_TTCONFIG_VERSION) {
		u16 cfg_ver_new;

		rc = cyttsp4_get_ttconfig_version(ttsp, ttconfig_data,
						  ttconfig_size, &cfg_ver_new);
		if (rc)
			return 0;

		tp_log_info("%s: img_ver:0x%04X new_ver:0x%04X\n",
			    __func__, data->si->ttconfig.version, cfg_ver_new);

		/* Check if config version is newer */
		if (cfg_ver_new > data->si->ttconfig.version) {
			tp_log_info("%s: Config version newer, will upgrade\n",
				    __func__);
			return 1;
		}

		tp_log_info
		    ("%s: Config version is identical or older, will NOT upgrade\n",
		     __func__);
		/* Check for config CRC */
	} else {
		u16 cfg_crc_new;

		rc = cyttsp4_get_ttconfig_crc(ttsp, ttconfig_data,
					      ttconfig_size, &cfg_crc_new);
		if (rc)
			return 0;

		tp_log_debug("%s: img_crc:0x%04X new_crc:0x%04X\n",
			     __func__, data->si->ttconfig.crc, cfg_crc_new);

		/* Check if config CRC different. */
		if (cfg_crc_new != data->si->ttconfig.crc) {
			tp_log_debug("%s: Config CRC different, will upgrade\n",
				     __func__);
			return 1;
		}

		tp_log_debug("%s: Config CRC equal, will NOT upgrade\n",
			     __func__);
	}

	return 0;
}

static int cyttsp4_check_ttconfig_version_platform(struct cyttsp4_device *ttsp,
						   struct cyttsp4_touch_config
						   *ttconfig)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	u32 fw_ver_config;
	u32 fw_revctrl_config_h;
	u32 fw_revctrl_config_l;

	if (!data->si) {
		tp_log_info
		    ("%s: No firmware infomation found, device FW may be corrupted\n",
		     __func__);
		return 0;
	}

	fw_ver_config = get_unaligned_be16(ttconfig->fw_ver + 2);
	fw_revctrl_config_h = get_unaligned_be32(ttconfig->fw_ver + 4);
	fw_revctrl_config_l = get_unaligned_be32(ttconfig->fw_ver + 8);

	/* FW versions should match */
	if (cyttsp4_check_firmware_version(ttsp, fw_ver_config,
					   fw_revctrl_config_h,
					   fw_revctrl_config_l)) {
		tp_log_err("%s: FW versions mismatch\n", __func__);
		return 0;
	}

	return cyttsp4_check_ttconfig_version(ttsp, ttconfig->param_regs->data,
					      ttconfig->param_regs->size);
}

static int upgrade_ttconfig_from_platform(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	struct cyttsp4_touch_config *ttconfig = NULL;
	struct touch_settings *param_regs;
	struct cyttsp4_sett_ttconfig_map *ttconfig_map;
	struct cyttsp4_touch_fw;
	int rc = -ENOSYS;
	int upgrade;
	u8 panel_id;

	tp_log_info("%s:Enter....\n", __func__);

	if (data->loader_pdata == NULL) {
		tp_log_info("%s: No loader platform data\n", __func__);
		return rc;
	}
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE
	data->si = cyttsp4_request_sysinfo(ttsp);
	/* if sysinfo is NULL, return */
	if (data->si == NULL) {
		tp_log_err("%s: Fail get sysinfo pointer from core\n",
			   __func__);
		return rc;
	}
#endif

	panel_id = data->si->si_ptrs.pcfg->panel_info0;

	for (ttconfig_map = data->loader_pdata->ttconfig_map;
	     ttconfig_map->ttconfig_param; ttconfig_map++) {
		tp_log_debug("%s: panel_id=%d,  ttconfig_map->id=%d\n",
			     __func__, panel_id, ttconfig_map->id);
		if ((panel_id == ttconfig_map->id)
		    && (0 ==
			strcmp(data->loader_pdata->tp_product_family,
			       ttconfig_map->product_family))) {
			ttconfig = ttconfig_map->ttconfig_param;
			tp_log_info("%s: panel_id=%d,  product_family=%s\n",
				    __func__, panel_id,
				    ttconfig_map->product_family);
			break;
		}
	}

	//ttconfig = data->loader_pdata->ttconfig;
	if (ttconfig == NULL) {
		tp_log_info("%s: No ttconfig data\n", __func__);
		return rc;
	}

	param_regs = ttconfig->param_regs;
	if (param_regs == NULL) {
		tp_log_info("%s: No touch parameters\n", __func__);
		return rc;
	}

	if (param_regs->data == NULL || param_regs->size == 0) {
		tp_log_info("%s: Invalid touch parameters\n", __func__);
		return rc;
	}

	if (ttconfig->fw_ver == NULL || ttconfig->fw_vsize == 0) {
		tp_log_info("%s: Invalid FW version for touch parameters\n",
			    __func__);
		return rc;
	}

	upgrade = cyttsp4_check_ttconfig_version_platform(ttsp, ttconfig);
	if (upgrade)
		return cyttsp4_upgrade_ttconfig(ttsp, param_regs->data,
						param_regs->size);

	return rc;
}
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE */

/*delete*/

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_MANUAL_TTCONFIG_UPGRADE
static ssize_t cyttsp4_config_data_write(struct file *filp,
					 struct kobject *kobj,
					 struct bin_attribute *bin_attr,
					 char *buf, loff_t offset, size_t count)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	u8 *p;

	tp_log_debug("%s: offset:%lld count:%ld\n", __func__, offset, count);

	mutex_lock(&data->config_lock);

	if (!data->config_loading) {
		mutex_unlock(&data->config_lock);
		return -ENODEV;
	}

	p = krealloc(data->config_data, offset + count, GFP_KERNEL);
	if (!p) {
		kfree(data->config_data);
		data->config_data = NULL;
		data->config_size = 0;
		data->config_loading = false;
		mutex_unlock(&data->config_lock);
		return -ENOMEM;
	}
	data->config_data = p;

	memcpy(&data->config_data[offset], buf, count);
	data->config_size += count;

	mutex_unlock(&data->config_lock);

	return count;
}

static struct bin_attribute bin_attr_config_data = {
	.attr = {
		 .name = "config_data",
		 .mode = S_IWUSR,
		 },
	.size = 0,
	.write = cyttsp4_config_data_write,
};

static ssize_t cyttsp4_config_loading_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	bool config_loading;

	mutex_lock(&data->config_lock);
	config_loading = data->config_loading;
	mutex_unlock(&data->config_lock);

	return sprintf(buf, "%d\n", config_loading);
}

static int cyttsp4_verify_ttconfig_binary(struct cyttsp4_device *ttsp,
					  u8 * bin_config_data,
					  int bin_config_size, u8 ** start,
					  int *len)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	int header_size;
	u16 config_size;
	u16 max_config_size;
	u32 fw_ver_config;
	u32 fw_revctrl_config_h;
	u32 fw_revctrl_config_l;

	if (!data->si) {
		tp_log_err
		    ("%s: No firmware infomation found, device FW may be corrupted\n",
		     __func__);
		return -ENODEV;
	}

	/*
	 * We need 11 bytes for FW version control info and at
	 * least 6 bytes in config (Length + Max Length + CRC)
	 */
	header_size = bin_config_data[0] + 1;
	if (header_size < 11 || header_size >= bin_config_size - 6) {
		tp_log_err("%s: Invalid header size %d\n", __func__,
			   header_size);
		return -EINVAL;
	}

	fw_ver_config = get_unaligned_be16(&bin_config_data[1]);
	fw_revctrl_config_h = get_unaligned_be32(&bin_config_data[3]);
	fw_revctrl_config_l = get_unaligned_be32(&bin_config_data[7]);

	/* FW versions should match */
	if (cyttsp4_check_firmware_version(ttsp, fw_ver_config,
					   fw_revctrl_config_h,
					   fw_revctrl_config_l)) {
		tp_log_err("%s: FW versions mismatch\n", __func__);
		return -EINVAL;
	}

	config_size = get_unaligned_le16(&bin_config_data[header_size]);
	max_config_size = get_unaligned_le16(&bin_config_data[header_size + 2]);
	/* Perform a simple size check (2 bytes for CRC) */
	if (config_size != bin_config_size - header_size - 2) {
		tp_log_err("%s: Config size invalid\n", __func__);
		return -EINVAL;
	}
	/* Perform a size check against device config length */
	if (config_size != data->si->ttconfig.length
	    || max_config_size != data->si->ttconfig.max_length) {
		tp_log_err("%s: Config size mismatch\n", __func__);
		return -EINVAL;
	}

	*start = &bin_config_data[header_size];
	*len = bin_config_size - header_size;

	return 0;
}

/*
 * 1: Start loading TT Config
 * 0: End loading TT Config and perform upgrade
 *-1: Exit loading
 */
static ssize_t cyttsp4_config_loading_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	long value;
	u8 *start;
	int length;
	int rc;

	rc = kstrtol(buf, 10, &value);
	if (rc < 0 || value < -1 || value > 1) {
		tp_log_err("%s: Invalid value\n", __func__);
		return size;
	}

	mutex_lock(&data->config_lock);

	if (value == 1)
		data->config_loading = true;
	else if (value == -1)
		data->config_loading = false;
	else if (value == 0 && data->config_loading) {
		data->config_loading = false;
		if (data->config_size == 0) {
			tp_log_err("%s: No config data\n", __func__);
			goto exit_free;
		}

		rc = cyttsp4_verify_ttconfig_binary(data->ttsp,
						    data->config_data,
						    data->config_size, &start,
						    &length);
		if (rc)
			goto exit_free;

		rc = cyttsp4_upgrade_ttconfig(data->ttsp, start, length);
	}

exit_free:
	kfree(data->config_data);
	data->config_data = NULL;
	data->config_size = 0;

	mutex_unlock(&data->config_lock);

	if (rc)
		return rc;

	return size;
}

static DEVICE_ATTR(config_loading, S_IRUSR | S_IWUSR,
		   cyttsp4_config_loading_show, cyttsp4_config_loading_store);
#endif /* CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_MANUAL_TTCONFIG_UPGRADE */

static char *get_touch_module_name(u8 module_id)
{
	switch (module_id) {
	case OFILM:
		return "OFILM";
	case ECW:
		return "ECW";
	case TRULY:
		return "TRULY";
	case MUTTO:
		return "MUTTO";
	case GIS:
		return "GIS";
	case JUNDA:
		return "JUNDA";
	case 6:
		return "OFILM";	//just for temp sunlibin 0712
	case 7:
		return "TRULY";	//just for temp sunlibin 0712
	default:
		return "UNKNOW";
	}
}


static void cyttsp4_fw_and_config_upgrade(struct work_struct
					  *fw_and_config_upgrade)
{
	struct cyttsp4_loader_data *data = container_of(fw_and_config_upgrade,
							struct
							cyttsp4_loader_data,
							fw_and_config_upgrade.
							work);
	struct cyttsp4_device *ttsp = data->ttsp;

	u8 pannel_id;
	//int rc;
	u16 ttconfig_version;
	char *module_name = NULL;

	data->si = cyttsp4_request_sysinfo(ttsp);
	if (data->si == NULL)
		tp_log_err("%s: Fail get sysinfo pointer from core\n",
			   __func__);

#if !CYTTSP4_FW_UPGRADE
	tp_log_info("%s: No FW upgrade method selected!\n", __func__);
#endif

	if (TP_COF == data->loader_pdata->tp_location) {
		tp_log_info("%s: COF FW and config upgrade!\n", __func__);
//              goto COF_NO_FW_UPGRADE;         
	} else {
		tp_log_info("%s: COB FW and config upgrade!\n", __func__);
	}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
	if (!upgrade_firmware_from_platform(ttsp, false))
		return;
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE
	if (!upgrade_firmware_from_builtin(ttsp)) {
		if (!wait_for_completion_timeout
		    (&data->sysinfo_update,
		     msecs_to_jiffies(CY_UPGRADE_FROM_BUILDIN_TIMEOUT)))
			tp_log_err("%s wait_for_completion_timeout \n",
				   __func__);
		else
			tp_log_info("%s sysinfo is ok,FW upgrade done\n",
				    __func__);
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_TTCONFIG_UPGRADE
	if (!upgrade_ttconfig_from_platform(ttsp)) {
		tp_log_info("%s upgrade_config_from_platform finished \n",
			    __func__);
	}
#endif

/*If it is cof,don't upgrade fw,only set app_info*/
//COF_NO_FW_UPGRADE:
	/* Check null pointer */
	data->si = cyttsp4_request_sysinfo(ttsp);
	if (data->si == NULL) {
		tp_log_err("%s: Fail get sysinfo pointer from core\n",
			   __func__);
		return;
	}
	pannel_id = data->si->si_ptrs.pcfg->panel_info0;
	ttconfig_version = data->si->ttconfig.version & CY_VER_MASK;
	/* show module name in project manue */
	module_name = get_touch_module_name(GET_PANELID(pannel_id));
	sprintf(touch_info, "cypress_TMA463_%s.%d", module_name,
		ttconfig_version);
#if 0
	rc = app_info_set("touch_panel", touch_info);
	if (rc) {
		tp_log_err("set_touch_chip_info error\n");
	}
#endif
	return;
}

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
static ssize_t cyttsp4_forced_upgrade_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	int rc;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	rc = upgrade_firmware_from_platform(data->ttsp, true);
	if (rc)
		return rc;
	return size;
}

static DEVICE_ATTR(forced_upgrade, S_IRUSR | S_IWUSR,
		   NULL, cyttsp4_forced_upgrade_store);
#endif

/* use for manual upgrade in recovery mode */
#ifdef CONFIG_TOUCHSCREEN_HUAWEI_CYTTSP4_RECOVERY_FW_UPDATE
static void cyttsp4_fw_recovery_upgrade(struct cyttsp4_loader_data *data)
{
	//int rc;
	struct cyttsp4_device *ttsp = data->ttsp;
	struct device *dev = &ttsp->dev;
	u8 pannel_id;
	char *module_name = NULL;
	u16 ttconfig_version;

	pm_runtime_get_sync(dev);
	data->si = cyttsp4_request_sysinfo(ttsp);
	if (data->si == NULL)
		tp_log_err("%s: Fail get sysinfo pointer from core\n",
			   __func__);
	pm_runtime_put(dev);

	if (!upgrade_firmware_from_builtin_recovery(ttsp)) {
		tp_log_info
		    ("%s upgrade_firmware_from_builtin_recovery success! \n",
		     __func__);
		if (!wait_for_completion_timeout
		    (&data->sysinfo_update,
		     msecs_to_jiffies(CY_UPGRADE_FROM_BUILDIN_TIMEOUT))) {
			tp_log_err("%s wait_for_completion_timeout \n",
				   __func__);
		} else {
			tp_log_info("%s sysinfo is ok\n", __func__);
		}
	}

	/* Check null pointer */
	data->si = cyttsp4_request_sysinfo(ttsp);
	if (data->si == NULL) {
		tp_log_err("%s: Fail get sysinfo pointer from core\n",
			   __func__);
		return;
	}

	pannel_id = data->si->si_ptrs.pcfg->panel_info0;
	ttconfig_version = data->si->ttconfig.version & CY_VER_MASK;
	module_name = get_touch_module_name(GET_PANELID(pannel_id));
	sprintf(touch_info, "cypress_TMA463_%s.%d", module_name,
		ttconfig_version);
#if 0
	rc = app_info_set("touch_panel", touch_info);
	if (rc) {
		tp_log_err("set_touch_chip_info error\n");
	}
#endif
	return;
}

static struct device *upgrade_dev;

/*
 * The "update_firmware" file where a static variable is read from and written to.
 */
static ssize_t update_firmware_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	struct cyttsp4_loader_data *data;
	data = dev_get_drvdata(upgrade_dev);
	return sprintf((char *)buf, "%d\n",
		       data->recovery_builtin_bin_fw_status);
}

static ssize_t update_firmware_store(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	int rc;
	struct cyttsp4_loader_data *data;
	data = dev_get_drvdata(upgrade_dev);

	if (NULL == data) {
		printk("%s,data is null\n", __func__);
		return -1;
	}

	if ('2' == buf[0]) {
		cyttsp4_fw_recovery_upgrade(data);
		rc = 1;
	} else {
		printk("ERROR:%s: update_firmware_store buf[0] = %c,not '2'\n",
		       __func__, buf[0]);
		rc = -1;
	}
	return rc;
}

static struct kobj_attribute update_firmware_attribute = {
	.attr = {
		 .name = "update_firmware",
		 .mode = 0664},
	.show = update_firmware_show,
	.store = update_firmware_store,
};

static int ts_firmware_file(struct kobject *kobject_ts)
{
	int ret;

	if (NULL == kobject_ts) {
		return -1;
	}

	ret = sysfs_create_file(kobject_ts, &update_firmware_attribute.attr);
	if (ret) {
		kobject_put(kobject_ts);
		printk("%s,Cypress4 create file error\n", __func__);
		return -1;
	}
	return 0;
}

#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE
static ssize_t cyttsp4_manual_upgrade_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t size)
{
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	upgrade_firmware_from_class(data->ttsp);
	return size;
}

static DEVICE_ATTR(manual_upgrade, S_IWUSR, NULL, cyttsp4_manual_upgrade_store);
#endif

static int cyttsp4_loader_probe(struct cyttsp4_device *ttsp)
{
	struct cyttsp4_loader_data *data;
	struct device *dev = &ttsp->dev;
	//struct cyttsp4_loader_platform_data *pdata = dev_get_platdata(dev);
	int rc;
	int i = 0;
	struct kobject *kobject_ts;

	tp_log_debug("%s\n", __func__);
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		tp_log_err("%s: Error, kzalloc\n", __func__);
		rc = -ENOMEM;
		goto error_alloc_data_failed;
	}
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
	rc = device_create_file(dev, &dev_attr_forced_upgrade);
	if (rc) {
		tp_log_err("%s: Error, could not create forced_upgrade\n",
			   __func__);
		goto error_create_forced_upgrade;
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE
	rc = device_create_file(dev, &dev_attr_manual_upgrade);
	if (rc) {
		tp_log_err("%s: Error, could not create manual_upgrade\n",
			   __func__);
		goto error_create_manual_upgrade;
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_MANUAL_TTCONFIG_UPGRADE
	rc = device_create_file(dev, &dev_attr_config_loading);
	if (rc) {
		tp_log_err("%s: Error, could not create config_loading\n",
			   __func__);
		goto error_create_config_loading;
	}

	rc = device_create_bin_file(dev, &bin_attr_config_data);
	if (rc) {
		tp_log_err("%s: Error, could not create config_data\n",
			   __func__);
		goto error_create_config_data;
	}
#endif

	/* make sure that the manual upgrade node is prepared */
#ifdef CONFIG_TOUCHSCREEN_HUAWEI_CYTTSP4_RECOVERY_FW_UPDATE
	kobject_ts = tp_get_touch_screen_obj();
	if (NULL != kobject_ts) {
		for (i = 0; i < 3; i++) {
			rc = ts_firmware_file(kobject_ts);
			if (rc)
				printk
				    ("%s,Cypress creat FW_Update failed! Retry = %d \n",
				     __func__, i);
			else
				break;
		}
	}
#endif

	//data->loader_pdata = pdata;
	data->loader_pdata = cyttsp4_request_loader_pdata(ttsp);
	data->ttsp = ttsp;

	//snprintf(name, MAX_NAME_LENGTH, "virtualkeys.%s", inp_dev_name);
	dev_set_drvdata(dev, data);

	pm_runtime_enable(dev);

	upgrade_dev = dev;

#if CYTTSP4_FW_UPGRADE
	init_completion(&data->int_running);
	init_completion(&data->sysinfo_update);
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE
	init_completion(&data->builtin_bin_fw_complete);
#endif
	cyttsp4_subscribe_attention(ttsp, CY_ATTEN_IRQ,
				    cyttsp4_loader_attention,
				    CY_MODE_BOOTLOADER);

	INIT_WORK(&data->calibration_work, cyttsp4_fw_calibrate);
#endif

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_MANUAL_TTCONFIG_UPGRADE
	mutex_init(&data->config_lock);
#endif
	/*it cant load it till system comes up so its safe to delay it */
	INIT_DELAYED_WORK(&data->fw_and_config_upgrade,
			  cyttsp4_fw_and_config_upgrade);
	//schedule_delayed_work(&data->fw_and_config_upgrade, msecs_to_jiffies(CY_LDR_UPDATE_WAIT_TIMEOUT));

	tp_log_info("%s: Successful probe %s\n", __func__, ttsp->name);
	return 0;

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_MANUAL_TTCONFIG_UPGRADE
error_create_config_data:
	device_remove_file(dev, &dev_attr_config_loading);
error_create_config_loading:
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE
	device_remove_file(dev, &dev_attr_manual_upgrade);
#endif
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE
error_create_manual_upgrade:
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
	device_remove_file(dev, &dev_attr_forced_upgrade);
error_create_forced_upgrade:
#endif
	kfree(data);
error_alloc_data_failed:
	tp_log_err("%s failed.\n", __func__);
	return rc;
}

static int cyttsp4_loader_release(struct cyttsp4_device *ttsp)
{
	struct device *dev = &ttsp->dev;
	struct cyttsp4_loader_data *data = dev_get_drvdata(dev);
	int retval = 0;

	tp_log_debug("%s\n", __func__);
#if CYTTSP4_FW_UPGRADE
	retval = cyttsp4_unsubscribe_attention(ttsp, CY_ATTEN_IRQ,
					       cyttsp4_loader_attention,
					       CY_MODE_BOOTLOADER);
	if (retval < 0) {
		tp_log_err("%s: Failed to restart IC with error code %d\n",
			   __func__, retval);
	}
#endif
	pm_runtime_suspend(dev);
	pm_runtime_disable(dev);
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_MANUAL_TTCONFIG_UPGRADE
	device_remove_bin_file(dev, &bin_attr_config_data);
	device_remove_file(dev, &dev_attr_config_loading);
	kfree(data->config_data);
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_BINARY_FW_UPGRADE
	device_remove_file(dev, &dev_attr_manual_upgrade);
#endif
#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_PLATFORM_FW_UPGRADE
	device_remove_file(dev, &dev_attr_forced_upgrade);
#endif
	dev_set_drvdata(dev, NULL);
	kfree(data);
	return retval;
}

static struct cyttsp4_driver cyttsp4_loader_driver = {
	.probe = cyttsp4_loader_probe,
	.remove = cyttsp4_loader_release,
	.driver = {
		   .name = CYTTSP4_LOADER_NAME,
		   .bus = &cyttsp4_bus_type,
		   .owner = THIS_MODULE,
		   },
};

static const char cyttsp4_loader_name[] = CYTTSP4_LOADER_NAME;
static struct cyttsp4_device_info cyttsp4_loader_infos[CY_MAX_NUM_CORE_DEVS];

static char *core_ids[CY_MAX_NUM_CORE_DEVS] = {
	CY_DEFAULT_CORE_ID,
	NULL,
	NULL,
	NULL,
	NULL
};

static int num_core_ids = 1;

//module_param_array(core_ids, charp, &num_core_ids, 0);
//MODULE_PARM_DESC(core_ids,
//      "Core id list of cyttsp4 core devices for loader module");

static int __init cyttsp4_loader_init(void)
{
	int rc = 0;
	int i, j;

	/* Check for invalid or duplicate core_ids */
	for (i = 0; i < num_core_ids; i++) {
		if (!strlen(core_ids[i])) {
			tp_log_err("%s: core_id %d is empty\n",
				   __func__, i + 1);
			return -EINVAL;
		}
		for (j = i + 1; j < num_core_ids; j++)
			if (!strcmp(core_ids[i], core_ids[j])) {
				tp_log_err("%s: core_ids %d and %d are same\n",
					   __func__, i + 1, j + 1);
				return -EINVAL;
			}
	}

/*	for (i = 0; i < num_core_ids; i++) {
		cyttsp4_loader_infos[i].name = cyttsp4_loader_name;
		cyttsp4_loader_infos[i].core_id = core_ids[i];
		tp_log_info("%s: Registering loader device for core_id: %s\n",
			__func__, cyttsp4_loader_infos[i].core_id);
		rc = cyttsp4_register_device(&cyttsp4_loader_infos[i]);
		if (rc < 0) {
			tp_log_err("%s: Error, failed registering device\n",
				__func__);
			goto fail_unregister_devices;
		}
	}*/
	rc = cyttsp4_register_driver(&cyttsp4_loader_driver);
	if (rc) {
		tp_log_err("%s: Error, failed registering driver\n", __func__);
		goto fail_unregister_devices;
	}

	tp_log_info("%s: Cypress TTSP FW loader (Built %s) rc=%d\n",
		    __func__, CY_DRIVER_DATE, rc);
	return 0;

fail_unregister_devices:
	for (i--; i >= 0; i--) {
		cyttsp4_unregister_device(cyttsp4_loader_infos[i].name,
					  cyttsp4_loader_infos[i].core_id);
		tp_log_info("%s: Unregistering loader device for core_id: %s\n",
			    __func__, cyttsp4_loader_infos[i].core_id);
	}
	return rc;
}

module_init(cyttsp4_loader_init);

static void __exit cyttsp4_loader_exit(void)
{
	int i;

	cyttsp4_unregister_driver(&cyttsp4_loader_driver);
	for (i = 0; i < num_core_ids; i++) {
		cyttsp4_unregister_device(cyttsp4_loader_infos[i].name,
					  cyttsp4_loader_infos[i].core_id);
		tp_log_info("%s: Unregistering loader device for core_id: %s\n",
			    __func__, cyttsp4_loader_infos[i].core_id);
	}
	tp_log_info("%s: module exit\n", __func__);
}

module_exit(cyttsp4_loader_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cypress TrueTouch(R) Standard touchscreen FW loader");
MODULE_AUTHOR("Cypress Semiconductor");

