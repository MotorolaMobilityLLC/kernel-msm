/*
 * ITE it8566 HDMI CEC driver
 *
 * Copyright(c) 2014 ASUSTek COMPUTER INC. All rights reserved.
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

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include "it8566_hdmi_cec.h"

#define DEV_NAME_FLASH "it8566_flash_mod"
static struct i2c_client        *flash_mode_client;
struct mutex it8566_fw_lock;
/* fw update ++ */
static char *fw_bin_path = "/system/etc/firmware/IT8566_CEC.BIN";
#define FW_FILE_SIZE 65536
unsigned char *gbuffer;
unsigned char flash_id[3];
unsigned int start;
unsigned int end;
#define EFLASH_CMD_BYTE_PROGRAM		0x02
#define EFLASH_CMD_WRITE_DISABLE	0x04
#define EFLASH_CMD_READ_STATUS		0x05
#define EFLASH_CMD_WRITE_ENABLE		0x06
#define EFLASH_CMD_FAST_READ		0x0B
#define EFLASH_CMD_CHIP_ERASE		0x60
#define EFLASH_CMD_READ_ID		0x9F
#define EFLASH_CMD_AAI_WORD_PROGRAM	0xAD
#define EFLASH_CMD_SECTOR_ERASE		0xD7
#define CMD_CS_HIGH			0x17

static int load_fw_bin_to_buffer(char *path)
{
	int result = 0;
	struct file *fp = NULL;
	mm_segment_t old_fs;

	kfree(gbuffer);
	gbuffer = kzalloc(FW_FILE_SIZE, GFP_KERNEL);

	if (!gbuffer) {
		dev_dbg(&flash_mode_client->dev,
			"%s:unable to allocate gbuffer\n", __func__);
		return -1;
	}

	dev_info(&flash_mode_client->dev, "open file:%s\n", path);
	fp = filp_open(path, O_RDONLY , 0);

	if (!IS_ERR_OR_NULL(fp)) {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		if (fp->f_op != NULL && fp->f_op->read != NULL) {
			fp->f_op->read(fp, gbuffer, FW_FILE_SIZE, &fp->f_pos);
		} else {
			dev_err(&flash_mode_client->dev, "fail to read fw file\n");
			result = -2;
		}
		set_fs(old_fs);
	} else if (PTR_ERR(fp) == -ENOENT) {
		dev_err(&flash_mode_client->dev, "fw file not found error\n");
		result = -3;
	} else {
		dev_err(&flash_mode_client->dev, "fw file open error\n");
		result = -4;
	}

	if (fp)
		filp_close(fp, NULL);

	return result;
}

static int ite_i2c_pre_define_cmd_read(unsigned char cmd1,
	unsigned int payload_len, unsigned char payload[])
{
	int result = 0, err = 0;
	struct i2c_msg msg[2];
	unsigned char buf1[2] = {0x18};

	if (!flash_mode_client) {
		dev_err(&flash_mode_client->dev,
			"%s:no flash_mode_client\n", __func__);
		return -1;
	}

	err = i2c_smbus_write_byte(flash_mode_client, CMD_CS_HIGH);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #1: err=%d\n", __func__, err);
		return -1;
	}

	/*CMD1*/
	buf1[1] = cmd1;
	msg[0].addr = flash_mode_client->addr;
	msg[0].len = 2;
	msg[0].flags = 0;
	msg[0].buf = buf1;

	/*CMD1 Read Payload*/
	msg[1].addr = flash_mode_client->addr;
	msg[1].len = payload_len;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = payload;

	err = i2c_transfer(flash_mode_client->adapter, msg, 2);

	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #2: err=%d\n", __func__, err);
		return -1;
	}

	return result;
}

/*
  payload under 256 can call this function
  Write to Read

  S 0x5B [W] [A] 0x17 [A] S 0x5B [W] [A] 0x18 [A] 0x0B [A] S
  0x5B [R] payload[0] [A] ... payload[N] [NA] [P]
*/
int ite_i2c_pre_define_cmd_fastread(unsigned char addr[],
	unsigned int payload_len, unsigned char payload[])
{
	int result = 0, err = 0;
	struct i2c_msg msg[2];
	unsigned char buf1[6] = {0x18, 0x0B};      /* 0x0b => Fast Read */

	if (!flash_mode_client) {
		dev_err(&flash_mode_client->dev,
			"%s:no flash_mode_client\n", __func__);
		return -1;
	}

	err = i2c_smbus_write_byte(flash_mode_client, CMD_CS_HIGH);

	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #1: err=%d\n", __func__, err);
		return -1;
	}

	buf1[2] = addr[3]; /* Address H */
	buf1[3] = addr[2]; /* Address M */
	buf1[4] = addr[1]; /* Address L */
	buf1[5] = addr[0]; /* Dummy */

	/*CMD1*/
	msg[0].addr = flash_mode_client->addr;
	msg[0].len = 6;
	msg[0].flags = 0;
	msg[0].buf = buf1;

	/*CMD1 Read Payload*/
	msg[1].addr = flash_mode_client->addr;
	msg[1].len = payload_len;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = payload;

	err = i2c_transfer(flash_mode_client->adapter, msg, 2);

	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #2: err=%d\n", __func__, err);
		return -1;
	}

	return result;
}

/*
  payload under 256 can call this function
  S 0x5B [W] [A] 0x17 [A] S 0x5B [W] [A] 0x18 [A] cmd1 [A] payload[0] [A] ...
  ... payload[N] [NA] [P]
*/
static int ite_i2c_pre_define_cmd_write(unsigned char cmd1,
	unsigned int payload_len, unsigned char payload[])
{
	int i, err = 0;
	unsigned char buf1[256];

	if (!flash_mode_client) {
		dev_err(&flash_mode_client->dev,
			"%s:no flash_mode_client\n", __func__);
		return -1;
	}

	err = i2c_smbus_write_byte(flash_mode_client, CMD_CS_HIGH);

	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #1: err=%d\n", __func__, err);
		return -1;
	}

	buf1[0] = cmd1;
	if (payload_len < 256) {
		for (i = 0; i < payload_len; i++)
			buf1[i+1] = payload[i];
	} else {
		dev_err(&flash_mode_client->dev,
			"%s:payload_len over 256\n", __func__);
		return -1;
	}

	err = i2c_smbus_write_i2c_block_data(flash_mode_client,
			0x18, payload_len + 1, buf1);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #2: err=%d\n", __func__, err);
		return -1;
	}

	return 0;
}

/*
   payload under 256 can call this function
   S 0x5B [W] [A] 0x17 [A] S 0x5B [W] [A] 0x18 [A] cmd1 [A] payload[0] [A] ...
   ... payload[N] [NA] [P]
*/
static int ite_i2c_pre_define_cmd_write_with_status(unsigned char cmd1,
		unsigned int payload_len, unsigned char payload[])
{
	int result = 0, i, err = 0;
	struct i2c_msg msg[2];
	unsigned char buf1[256];
	unsigned char cmd_status[2] = {0x18, EFLASH_CMD_READ_STATUS};
	unsigned char read_status[3];

	/*CS High*/
	err = i2c_smbus_write_byte(flash_mode_client, CMD_CS_HIGH);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #1: err=%d\n", __func__, err);
		return -1;
	}

	buf1[0] = cmd1;
	if (payload_len < 256) {
		for (i = 0; i < payload_len; i++)
			buf1[i+1] = payload[i];
	} else {
		dev_err(&flash_mode_client->dev,
			"%s: payload_len over 256\n", __func__);
		return -1;
	}

	err = i2c_smbus_write_i2c_block_data(flash_mode_client,
			0x18, payload_len + 1, buf1);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #2: err=%d\n", __func__, err);
		return -1;
	}

	/*1st read status
	CS High*/
	err = i2c_smbus_write_byte(flash_mode_client, CMD_CS_HIGH);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #3: err=%d\n", __func__, err);
		return -1;
	}

	/*CMD1 change to EFLASH_CMD_READ_STATUS*/
	msg[0].addr = flash_mode_client->addr;
	msg[0].len = 2;
	msg[0].flags = 0;
	msg[0].buf = cmd_status;

	/*CMD1 Read Payload*/
	msg[1].addr = flash_mode_client->addr;
	msg[1].len = 3;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = read_status;

	err = i2c_transfer(flash_mode_client->adapter, msg, 2);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c_transfer FAIL #4: err=%d\n", __func__, err);
		return -1;
	}

	result = (int)read_status[2];

	return result;
}

static int i2ec_readbyte(unsigned int address)
{
	int result;
	unsigned char buf0[2] = {0};

	buf0[0] = (address >> 8) & 0xFF; /*addr[15:8]*/
	buf0[1] = (address) & 0xFF; /*addr[7:0]*/

	result = i2c_smbus_write_i2c_block_data(flash_mode_client,
						0x10, 2, buf0);
	if (result < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #1: result=%d\n",
			__func__, result);
		return -1;
	}

	result = i2c_smbus_read_byte_data(flash_mode_client, 0x11);
	if (result < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #2: result=%d\n",
			__func__, result);
		return -1;
	}

	return result;
}

static int i2ec_writebyte(unsigned int address, unsigned char data)
{
	int err = 0;
	unsigned char buf0[2] = {0};

	buf0[0] = (address >> 8) & 0xFF; /*addr[15:8]*/
	buf0[1] = (address) & 0xFF; /*addr[7:0]*/

	err = i2c_smbus_write_i2c_block_data(flash_mode_client, 0x10, 2, buf0);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #1: err=%d\n", __func__, err);
		return -1;
	}

	/*I2EC WRITE BYTE DATA*/
	err = i2c_smbus_write_byte_data(flash_mode_client, 0x11, data);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #2: err=%d\n", __func__,  err);
		return -1;
	}

	return err;
}

static int cmd_write_enable(void)
{
	return ite_i2c_pre_define_cmd_write(EFLASH_CMD_WRITE_ENABLE,
			0, NULL);
}

static int cmd_write_disable(void)
{
	return ite_i2c_pre_define_cmd_write(EFLASH_CMD_WRITE_DISABLE,
			0, NULL);
}

static unsigned char cmd_check_status(void)
{
	unsigned char status[2];
	ite_i2c_pre_define_cmd_read(EFLASH_CMD_READ_STATUS, 2, status);
	return status[1];
}

static int cmd_erase_sector(int address)
{

	int result;
	unsigned char addr_h, addr_m, addr_l, buf[3];
	addr_h = (unsigned char)((address >> 16) & 0xFF);
	addr_m = (unsigned char)((address >> 8) & 0xFF);
	addr_l = (unsigned char)((address) & 0xFF);

	buf[0] = addr_h;
	buf[1] = addr_m;
	buf[2] = addr_l;
	result = ite_i2c_pre_define_cmd_write(EFLASH_CMD_SECTOR_ERASE, 3, buf);
	while (cmd_check_status() & 0x1)
		usleep_range(1000, 2000);

	return result;

}

static int cmd_enter_flash_mode(void)
{
	int err = 0;
	struct i2c_client *cec_client = it8566_get_cec_client();

	if (!cec_client)
		return -1;

	err = i2c_smbus_write_byte(cec_client, 0xEF);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL...#1: err=%d\n", __func__, err);
		return -1;
	}

	return err;
}

static void get_flash_id(void)
{
	ite_i2c_pre_define_cmd_read(EFLASH_CMD_READ_ID, 3, flash_id);
}

static int get_ec_ver(char *ec_ver)
{
	int err;
#ifdef DEBUG
	int i;
#endif
	struct i2c_client *cec_client = it8566_get_cec_client();

	if (!cec_client)
		return -1;

	err = i2c_smbus_read_i2c_block_data(cec_client, 0xEC, 8, ec_ver);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:i2c transfer FAIL #1: err=%d\n", __func__, err);
		return -1;
	}
#ifdef DEBUG
	dev_dbg(&flash_mode_client->dev, "EC Version:\n");
	for (i = 0; i < 8; i++)
		dev_dbg(&flash_mode_client->dev, "%x:\n", ec_ver[i]);
#endif
	return err;
}

static int check_ec_has_been_flash(char *ec_ver)
{
	unsigned char addr_18, addr_19;

	if (get_ec_ver(ec_ver) < 0) {
		/* may flash fw fail before or ec
		   has not been flash fw yet */
		addr_18 = i2ec_readbyte(0x18);
		addr_19 = i2ec_readbyte(0x19);

		dev_info(&flash_mode_client->dev,
			"%s:SRAM(0x18h)=0x%x, SRAM(0x19h)=0x%x\n",
			__func__, addr_18, addr_19);

		if ((addr_18 == 0xBB) && (addr_19 == 0xBB)) {
			/* has been flash fw, but fail before */
			/* no need enter flash mode*/
			return 0;
		} else if ((addr_18 == 0xEC) && (addr_19 == 0xEC)) {
			/* has been flash fw, but fail before */
			/* need enter flash mode*/
			return 1;
		} else {
			/* has not been flash fw yet */
			/* no need enter flash mode*/
			return -1;
		}
	} else {
		/* means ec has valid fw*/
		/* need enter flash mode*/
		return 2;
	}
}

static void cmd_erase_all_sector(void)
{
	int address;
	dev_info(&flash_mode_client->dev,
		"%s: start=0x%x, end=0x%x\n", __func__, start, end);
	for (address = start; address < end; address += 0x400) {
		cmd_write_enable();
		while ((cmd_check_status() & 0x02) != 0x02)
			usleep_range(1000, 2000);
		cmd_erase_sector(address); /*per 1k byte*/
	}
}

static void do_erase_all(void)
{
	cmd_erase_all_sector();
	cmd_write_disable();
}

static int do_check(void)
{
	int i, result = 0, addr = 0;
	unsigned char *buffer;
	unsigned char address[4] = {0}; /* Dummy , L , M , H */

	buffer = kzalloc(FW_FILE_SIZE, GFP_KERNEL);

	if (!buffer) {
		dev_dbg(&flash_mode_client->dev,
			"%s:unable to allocate buffer\n", __func__);
		return -1;
	}

	dev_info(&flash_mode_client->dev, "Check erase ...\n");
#ifdef FAST_READ_256_BYTE
	dev_dbg(&flash_mode_client->dev, "per 256 byte\n");
	for (i = 0; i < 0x100; i++) {/*per 256 bytes, 256 times*/
		address[2] = i;
		ite_i2c_pre_define_cmd_fastread(address, 0x100,
			&buffer[0x100*i]);
	}
#else
	dev_dbg(&flash_mode_client->dev, "per 32 byte\n");
	for (i = 0; i < 0x800; i++) {/*per 32 bytes, 2048 times*/
		addr = 0x20*i;
		address[1] = (unsigned char)(addr & 0xFF);
		address[2] = (unsigned char)((addr>>8) & 0xFF);
		address[3] = (unsigned char)((addr>>16) & 0xFF);
		ite_i2c_pre_define_cmd_fastread(address, 0x20,
			&buffer[addr]);
	}
#endif
	dev_info(&flash_mode_client->dev,
		"start=0x%x, end=0x%x: check if all values are 0xFF...\n",
		start, end);
	for (i = start; i < end; i++) {
		if (buffer[i] != 0xFF) {
			dev_err(&flash_mode_client->dev,
				"Check Erase Error on offset[%x] ; EFLASH=%02x\n",
				i, buffer[i]);
			result = -1;
			break;
		}
	}

	if (result == 0)
		dev_info(&flash_mode_client->dev, "Check erase OK!\n");

	kfree(buffer);

	return result;
}

int do_program(void)
{
	int result = 0, i;
	unsigned char payload[5];/*A2,A1,A0,Data0,Data1*/

	cmd_write_enable();
	while ((cmd_check_status() & 0x02) != 0x02)
		usleep_range(1000, 2000);

	payload[0] = (unsigned char)((start >> 16) & 0xFF); /*A2*/
	payload[1] = (unsigned char)((start >> 8) & 0xFF); /*A1*/
	payload[2] = (unsigned char)(start & 0xFF); /*A0*/

	payload[3] = gbuffer[start+0]; /*Data 0*/
	payload[4] = gbuffer[start+1]; /*Data 1*/

	result = ite_i2c_pre_define_cmd_write_with_status(
			EFLASH_CMD_AAI_WORD_PROGRAM, 5, payload);
	/*double check status not in busy status*/
	while (result & 0x01) {
		usleep_range(1000, 2000);
		result = cmd_check_status();
	}
	/*Combine command & check status to enhance speed*/
	dev_info(&flash_mode_client->dev, "Program...\n");

	for (i = (2+start); i < end;) {
		result = ite_i2c_pre_define_cmd_write_with_status(
				EFLASH_CMD_AAI_WORD_PROGRAM,
				2, &gbuffer[i]);
		/*wait Bit0 = 0*/
		while (result & 0x01) {
			usleep_range(1000, 2000);
			result = cmd_check_status();
		}
		if ((i%4096) == 0)
			dev_info(&flash_mode_client->dev,
				"Program i=0x%04x\n", i);

		i += 2;
	}

	cmd_write_disable();
	dev_info(&flash_mode_client->dev, "Program OK!\n");

	return result;
}

static int do_verify(void)
{
	int i, result = 0, addr = 0;
	unsigned char *buffer;
	unsigned char address[4] = {0}; /* Dummy , L , M , H */

	buffer = kzalloc(FW_FILE_SIZE, GFP_KERNEL);

	if (!buffer) {
		dev_dbg(&flash_mode_client->dev,
			"%s:unable to allocate buffer\n", __func__);
		kfree(gbuffer);
		gbuffer = NULL;
		return -1;
	}

	dev_info(&flash_mode_client->dev, "Verify ...\n");
#ifdef FAST_READ_256_BYTE
	dev_dbg(&flash_mode_client->dev, "per 256 byte\n");
	for (i = 0; i < 0x100; i++) {
		address[2] = i;
		ite_i2c_pre_define_cmd_fastread(address,
			0x100, &buffer[0x100*i]);
	}
#else
	dev_dbg(&flash_mode_client->dev, "per 32 byte\n");
	for (i = 0; i < 0x800; i++) {/*per 32 bytes, 2048 times*/
		addr = 0x20*i;
		address[1] = (unsigned char)(addr & 0xFF);
		address[2] = (unsigned char)((addr>>8) & 0xFF);
		address[3] = (unsigned char)((addr>>16) & 0xFF);
		ite_i2c_pre_define_cmd_fastread(address,
			0x20, &buffer[addr]);
	}
#endif
	dev_info(&flash_mode_client->dev,
		"start=0x%x, end=0x%x: check if all values are equal to fw file...\n",
		start, end);

	for (i = start; i < end; i++) {
		if (buffer[i] != gbuffer[i]) {
			dev_err(&flash_mode_client->dev,
			"Verify Error on offset[%x] ; file=%02x EFLASH=%02x\n",
			i, gbuffer[i], buffer[i]);

			result = -1;
			break;
		}
	}

	if (result == 0)
		dev_info(&flash_mode_client->dev, "Verify OK!\n");

	kfree(buffer);

	kfree(gbuffer);
	gbuffer = NULL;

	return result;
}

static void do_reset(void)
{
	unsigned char tmp1;
	tmp1 = i2ec_readbyte(0x1F01);
	i2ec_writebyte(0x1F01, 0x20);
	i2ec_writebyte(0x1F07, 0x01);
	i2ec_writebyte(0x1F01, tmp1);
}

static int load_fw_and_set_flash_region(int force)
{
	unsigned char ec_version[8];
	unsigned char bin_version[3];
	int err, i;

	err = load_fw_bin_to_buffer(fw_bin_path);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s: load %s fail, abort\n",
			__func__, fw_bin_path);
		return -1;
	}

	err = check_ec_has_been_flash(ec_version);

	if (!force) {
		if (err < 0) {
			/* err < 0 means no fw in ic yet, do not
			   do update since it may brick the ic
			   if shutdown during flash */
			dev_err(&flash_mode_client->dev,
				"it8566 has no fw, abort\n");
			return 1;
		}

		if (err < 2) {
			/* err == 0, 1 means ic has fw flash fail before,
			   continue to do fw update */
			dev_info(&flash_mode_client->dev,
				"it8566 has fw flash fail before\n");
		} else {
			/* err == 2 means ic has valid fw inside,
			   futhur check fw version to see if need
			   to update */
			for (i = 0; i < 3; i++)
				bin_version[i] = gbuffer[0x7f20 + i];

			dev_info(&flash_mode_client->dev,
				"Check bin ver: %x.%x.%x, current fw ver: %x.%x.%x\n",
				bin_version[0], bin_version[1], bin_version[2],
				ec_version[0], ec_version[1], ec_version[2]);

			if (ec_version[0] > bin_version[0] ||
			    (ec_version[0] == bin_version[0] &&
			     ec_version[1] > bin_version[1]) ||
			    (ec_version[0] == bin_version[0] &&
			     ec_version[1] == bin_version[1] &&
			     ec_version[2] >= bin_version[2])) {
				dev_info(&flash_mode_client->dev,
						"no need to update\n");
				return 1;
			}
		}
	} else {
		if (err < 0)
			dev_info(&flash_mode_client->dev,
			"it8566 has no fw, do full flash\n");
		else if (err < 2)
			dev_info(&flash_mode_client->dev,
			"it8566 fail to flash fw before, do partial flash\n");
		else
			dev_info(&flash_mode_client->dev,
			"it8566 has fw, do partial flash. Current fw ver: %x.%x.%x\n",
			ec_version[0], ec_version[1], ec_version[2]);
	}

	if (err < 0) {
		start = 0x0;
		end = 0x10000; /*65536*/

		get_flash_id();
		if ((flash_id[0] != 0xFF) ||
			(flash_id[1] != 0xFF) ||
			(flash_id[2] != 0xFE)) {
			dev_err(&flash_mode_client->dev,
			"%s:fail: flash_id:[0]=%d, [1]=%d, [2]=%d\n",
			__func__, flash_id[0], flash_id[1], flash_id[2]);
			return -1;
		}
	} else {
		start = 0x2000; /*8192*/
		end = 0xFC00; /*64512*/

		if (err > 0) {
			cmd_enter_flash_mode();
			msleep(20);/*wait for enter flash mode*/
		}

		/*double check if get flash id ok*/
		get_flash_id();
		if ((flash_id[0] != 0xFF) ||
			(flash_id[1] != 0xFF) ||
			(flash_id[2] != 0xFE)) {
			dev_err(&flash_mode_client->dev,
			"%s:fail: flash_id:[0]=%d, [1]=%d, [2]=%d\n",
			__func__, flash_id[0], flash_id[1], flash_id[2]);
			return -1;
		}
	}
	return 0;
}

#define NUM_FW_UPDATE_RETRY 2
int it8566_fw_update(int force)
{
	int err = 0;
	int retry = -1;
	unsigned char ec_version[8];

retry:
	retry++;
	if (retry > NUM_FW_UPDATE_RETRY)
		goto update_fail;

	dev_info(&flash_mode_client->dev, "Update fw..., retry=%d\n", retry);

	err = load_fw_and_set_flash_region(force);
	if (err < 0)
		goto retry;
	else if (err > 0)
		goto update_none;

	do_erase_all();
	err = do_check();
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
		"%s:check erase fail...\n", __func__);
		goto retry;
	}

	do_program();
	err = do_verify();
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
		"%s:verify program fail...\n", __func__);
		goto retry;
	}

	do_reset();
	msleep(2000);

	err = get_ec_ver(ec_version);
	if (err < 0) {
		dev_err(&flash_mode_client->dev,
			"%s:get fw version fail...\n", __func__);
		goto retry;
	}

	dev_info(&flash_mode_client->dev,
		"Update fw Success!, Current fw ver: %x.%x.%x\n",
		ec_version[0], ec_version[1], ec_version[2]);

	/*update success*/
	return 0;

update_fail:
	dev_err(&flash_mode_client->dev, "Update fw Fail...\n");
	kfree(gbuffer);
	gbuffer = NULL;
	/*update fail*/
	return -1;
update_none:
	kfree(gbuffer);
	gbuffer = NULL;
	/*no need to update*/
	return 1;
}
/* fw update -- */

/* dbg_fw_update ++ */
static ssize_t dbg_fw_update_write(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	char char_buf[10];
	int int_buf = 0;

	size_t buf_size = min(count, sizeof(buf)-1);

	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;
	/*buf[buf_size] = 0;*/
	sscanf(buf, "%s %d", char_buf, &int_buf);
	dev_dbg(&flash_mode_client->dev,
		"char_buf=%s, int_buf=%d\n", char_buf, int_buf);

	mutex_lock(&it8566_fw_lock);
	if (!strcmp(char_buf, "force")) {
		it8566_fw_update(1);
	} else if (!strcmp(char_buf, "step")) {
		switch (int_buf) {
		case 1:
			load_fw_and_set_flash_region(1);
			break;
		case 2:
			do_erase_all();
			break;
		case 3:
			do_check();
			break;
		case 4:
			do_program();
			break;
		case 5:
			do_verify();
			break;
		case 6:
			do_reset();
			break;
		default:
			dev_info(&flash_mode_client->dev, "do nothing");
		}
	}
	mutex_unlock(&it8566_fw_lock);

	return buf_size;
}

static const struct file_operations dbg_fw_update_fops = {
	.open           = simple_open,
	.write          = dbg_fw_update_write,
};
/* dbg_fw_update -- */

/* dbg_ec_ver ++ */
static ssize_t dbg_ec_ver_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *ppos)
{
	char buf[32];
	char v[8];

	if (get_ec_ver(v) < 0) {
		dev_err(&flash_mode_client->dev,
			"%s: can't get fw version\n", __func__);

		snprintf(buf, 32, "%s\n",
				"can't get fw version");
		return simple_read_from_buffer(user_buf, count, ppos,
				buf, strlen(buf));
	}
	snprintf(buf, 32, "%x.%x.%x\n", v[0], v[1], v[2]);
	return simple_read_from_buffer(user_buf, count, ppos,
			buf, strlen(buf));
}

static const struct file_operations dbg_ec_ver_fops = {
	.open           = simple_open,
	.read           = dbg_ec_ver_read,
};
/* dbg_ec_ver -- */

static void add_debugfs(void)
{
	struct dentry *cecdir, *d;

	cecdir = it8566_get_debugfs_dir();
	if (!cecdir) {
		dev_err(&flash_mode_client->dev,
			"can not create debugfs dir\n");
		return;
	}
	d = debugfs_create_file("fw_update", S_IWUSR, cecdir,
			NULL, &dbg_fw_update_fops);
	if (!d) {
		dev_err(&flash_mode_client->dev,
			"can not create debugfs fw_update\n");
		return;
	}
	d = debugfs_create_file("fw_version", S_IRUSR , cecdir,
			NULL, &dbg_ec_ver_fops);
	if (!d) {
		dev_err(&flash_mode_client->dev,
			"can not create debugfs fw_version\n");
		return;
	}
}

static int it8566_flash_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter;

	dev_info(&client->dev, "%s\n", __func__);
	adapter = to_i2c_adapter(client->dev.parent);

	if (!i2c_check_functionality(adapter,
			I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev,
			"I2C adapter %s doesn't support"
			" SMBus BYTE DATA & I2C BLOCK\n",
			adapter->name);
		return -EIO;
	}

	flash_mode_client = client;
	mutex_init(&it8566_fw_lock);

	add_debugfs();

	return 0;
}

static int it8566_flash_remove(struct i2c_client *client)
{
	mutex_destroy(&it8566_fw_lock);
	return 0;
}

static const struct i2c_device_id it8566_flash_id[] = {
	{DEV_NAME_FLASH, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, it8566_flash_id);

static struct i2c_driver it8566_i2c_flash_driver = {
	.driver = {
		.name = DEV_NAME_FLASH,
	},
	.probe = it8566_flash_probe,
	.remove = it8566_flash_remove,
	.id_table = it8566_flash_id,
};

static int __init it8566_flash_init(void)
{
	return i2c_add_driver(&it8566_i2c_flash_driver);
}

module_init(it8566_flash_init);

static void __exit it8566_flash_exit(void)
{
	i2c_del_driver(&it8566_i2c_flash_driver);
}

module_exit(it8566_flash_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ASUS IT8566 firmware flash driver");
MODULE_AUTHOR("Joey SY Chen <joeysy_chen@asus.com>");
