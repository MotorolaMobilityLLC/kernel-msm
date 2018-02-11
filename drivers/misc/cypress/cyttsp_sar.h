/*
 * Copyright (C) 2013 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _CYTTSP_SAR_H_
#define _CYTTSP_SAR_H_

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/wakelock.h>

#define CYTTSP_SAR_CHANNEL_ENABLE		0x06
#define CYTTSP_SAR_FORCE_CALIBRATE		0x07
#define CYTTSP_SAR_REFRESH_BASELINE		0x08
#define CYTTSP_SAR_CHANNEL_MAX			0x08
#define CYTTSP_SAR_STATE_ERROR			0x03

#define CYTTSP_CMD_VERIFY_CHKSUM		0x31
#define CYTTSP_CMD_GET_FLASH_SIZE		0x32
#define CYTTSP_CMD_ERASE_ROW			0x34
#define CYTTSP_CMD_SEND_DATA			0x37
#define CYTTSP_CMD_ENTER_BTLD			0x38
#define CYTTSP_CMD_PROGRAM_ROW			0x39
#define CYTTSP_CMD_VERIFY_ROW			0x3A
#define CYTTSP_CMD_EXIT_BTLD			0x3B

#define CYTTSP_REG_TOUCHMODE			0x00
#define CYTTSP_REG_HWVERSION			0x59
#define CYTTSP_REG_INTERRUPT_PEDNING		0x50
#define CYTTSP_REG_STATUS_LSB			0x51
#define CYTTSP_REG_STATUS_MSB			0x52


#define CYTTSP_REG_INVALID			0xFF

#define CYTTSP_NORMAL_MODE			0x00
#define CYTTSP_STANDBY_MODE			0x01

#define CYTTSP_STS_SUCCESS			0x00

#define CYTTSP_SAR_OP_MODE			0x00
#define CYTTSP_SAR_FW_VER1			0x57
#define CYTTSP_SAR_FW_VER2			0x58

#define CYTTSP_PACKET_START			0x01
#define CYTTSP_PACKET_END			0x17

#define CYTTSP_MAX_PAYLOAD_LEN			0x15

#define CYTTSP_RESP_HEADER_LEN			0x4
#define CYTTSP_RESP_TAIL_LEN			0x3

#define CYTTSP_ENTER_BTLD_RESP_LEN		8
#define CYTTSP_GET_FLASHSZ_RESP_LEN		4


#define CYTTSP_FW_VERSION_FILE_OFFSET		183


/**************************************
 * define platform data
 *
 **************************************/
struct cyttsp_reg_data {
	unsigned char reg;
	unsigned char val;
};

static struct cyttsp_reg_data cyttsp_i2c_reg_setup[] = {
	{
		.reg = CYTTSP_NORMAL_MODE,
		.val = 0x00,
	},
	{
		.reg = CYTTSP_SAR_CHANNEL_ENABLE,
		.val = 0x0f,
	},
};

typedef struct cyttsp_sar_data *pcyttsp_data_t;
struct cyttsp_sar_data {
	struct i2c_client *client;
	struct cyttsp_sar_platform_data *pdata;
	struct regulator *regulator_vdd;
	struct regulator *regulator_avdd;
	struct regulator *regulator_vddio;
	struct input_dev *input_dev[4];
	struct delayed_work  eint_work;
	struct wake_lock cap_lock;
	bool dbgdump;
	unsigned long sensorStatus;
	bool enable;
	struct work_struct ps_notify_work;
	struct notifier_block ps_notif;
	bool ps_is_present;
	u8 bootloader_addr;
	u8 app_addr;
};


struct cyttsp_sar_platform_data {
	int irq_gpio;
	u32 irq_gpio_flags;
	unsigned long irqflags;
	const char *input_name;
	int i2c_reg_num;
	struct mutex i2c_mutex;
	struct cyttsp_reg_data *pi2c_reg;
	int nsars;
	int *key_code;
	u8 sar_status_reg;
	u8 bootloader_addr;
	u8 standby_reg;
	int default_config;
	int config_array_size;
	struct cyttsp_config_info *config_array;
	bool cut_off_power;
};

#endif
