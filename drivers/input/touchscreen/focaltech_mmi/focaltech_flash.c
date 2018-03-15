/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2016, Focaltech Ltd. All rights reserved.
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

#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/input/focaltech_mmi.h>
#include "focaltech_flash.h"
#include "focaltech_upgrade_common.h"

static struct fts_upgrade_fun *fts_updatefun_curr;
static struct ft_chip_t *fts_chip_type_curr;
static const struct ft_ts_platform_data *fts_pdata_curr;
static const char *fts_fw_name_curr;

int fts_i2c_read(struct i2c_client *client, char *writebuf,
			int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
			 },
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}

int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s: i2c write error.\n", __func__);

	return ret;
}

int fts_i2c_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;

	return fts_i2c_write(client, buf, sizeof(buf));
}

int fts_i2c_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return fts_i2c_read(client, &addr, 1, val, 1);
}

/************************************************************************
* Name: fts_ctpm_i2c_hid2std
* Brief:  HID to I2C
* Input: i2c info
* Output: no
* Return: fail =0
***********************************************************************/
int fts_ctpm_i2c_hid2std(struct i2c_client *client)
{
	u8 buf[5] = {0};
	int bRet = 0;

	if (FTS_CHIP_IDC(fts_chip_type_curr->chip_type))
		return 0;

	buf[0] = 0xeb;
	buf[1] = 0xaa;
	buf[2] = 0x09;
	bRet = fts_i2c_write(client, buf, 3);
	msleep(20);
	buf[0] = buf[1] = buf[2] = 0;
	fts_i2c_read(client, buf, 0, buf, 3);

	if ((buf[0] == 0xeb) && (buf[1] == 0xaa) && (buf[2] == 0x08)) {
		FTS_DEBUG("hidi2c change to stdi2c successful\n");
		bRet = 1;
	} else {
		FTS_ERROR("hidi2c change to stdi2c error\n");
		bRet = 0;
	}

	return bRet;
}

/************************************************************************
* Name: fts_ctpm_rom_or_pram_reset
* Brief: RST CMD(07), reset to romboot(maybe->bootloader)
* Input:
* Output:
* Return:
***********************************************************************/
void fts_ctpm_rom_or_pram_reset(struct i2c_client *client)
{
	u8 rst_cmd = FTS_REG_RESET_FW;

	FTS_INFO("[UPGRADE] Reset to romboot/bootloader\n");
	fts_i2c_write(client, &rst_cmd, 1);
	/* The delay can't be changed */
	msleep(300);
}

/************************************************************************
* Name: fts_ctpm_get_pram_or_rom_id
* Brief: 0
* Input: 0
* Output: 0
* Return: 0
***********************************************************************/
enum FW_STATUS fts_ctpm_get_pram_or_rom_id(struct i2c_client *client)
{
	u8 buf[4];
	u8 reg_val[2] = {0};
	enum FW_STATUS inRomBoot = FTS_RUN_IN_ERROR;

	fts_ctpm_i2c_hid2std(client);

	/*Enter upgrade mode*/
	/*send 0x55 in time windows*/
	buf[0] = FTS_UPGRADE_55;
	buf[1] = FTS_UPGRADE_AA;
	fts_i2c_write(client, buf, 2);

	msleep(20);

	buf[0] = 0x90;
	buf[1] = buf[2] = buf[3] = 0x00;
	fts_i2c_read(client, buf, 4, reg_val, 2);

	FTS_DEBUG("[UPGRADE] ROM/PRAM/Bootloader id:0x%02x%02x\n",
			reg_val[0], reg_val[1]);
	if ((reg_val[0] == 0x00) || (reg_val[0] == 0xFF))
		inRomBoot = FTS_RUN_IN_ERROR;
	else if ((reg_val[0] == fts_chip_type_curr->pramboot_idh) &&
		(reg_val[1] == fts_chip_type_curr->pramboot_idl))
		inRomBoot = FTS_RUN_IN_PRAM;
	else if ((reg_val[0] == fts_chip_type_curr->rom_idh) &&
		(reg_val[1] == fts_chip_type_curr->rom_idl))
		inRomBoot = FTS_RUN_IN_ROM;
	else if ((reg_val[0] == fts_chip_type_curr->bootloader_idh) &&
		(reg_val[1] == fts_chip_type_curr->bootloader_idl))
		inRomBoot = FTS_RUN_IN_BOOTLOADER;

	return inRomBoot;
}

/************************************************************************
* Name: fts_ctpm_get_app_ver
* Brief:  get app file version
* Input:
* Output:
* Return: fw version
***********************************************************************/
int fts_ctpm_get_app_ver(void)
{
	int i_ret = 0;

	if (fts_updatefun_curr->get_app_bin_file_ver)
		i_ret = fts_updatefun_curr->get_app_bin_file_ver
							(fts_fw_name_curr);

	if (i_ret < 0)
		i_ret = 0;

	return i_ret;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade
* Brief:  fw upgrade entry funciotn
* Input:
* Output:
* Return: 0  - upgrade successfully
*         <0 - upgrade failed
***********************************************************************/
int fts_ctpm_fw_upgrade(struct i2c_client *client)
{
	int i_ret = 0;

	if (fts_updatefun_curr->upgrade_with_app_bin_file)
		i_ret = fts_updatefun_curr->upgrade_with_app_bin_file(client,
							fts_fw_name_curr);

	return i_ret;
}

/************************************************************************
* Name: fts_ctpm_check_fw_status
* Brief: Check App is valid or not
* Input:
* Output:
* Return: -EIO - I2C communication error
*         FTS_RUN_IN_APP - APP valid
*         0 - APP invalid
***********************************************************************/
static int fts_ctpm_check_fw_status(struct i2c_client *client)
{
	u8 chip_id1 = 0;
	u8 chip_id2 = 0;
	int fw_status = FTS_RUN_IN_ERROR;
	int i = 0;
	int ret = 0;
	int i2c_noack_retry = 0;

	for (i = 0; i < 5; i++) {
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &chip_id1);
		if (ret < 0) {
			i2c_noack_retry++;
			continue;
		}
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID2, &chip_id2);
		if (ret < 0) {
			i2c_noack_retry++;
			continue;
		}
		if (chip_id1 != fts_chip_type_curr->chip_idh)
			continue;
		if (FTS_CHIP_IDC(fts_chip_type_curr->chip_type) &&
			(chip_id2 != fts_chip_type_curr->chip_idl))
			continue;
		fw_status = FTS_RUN_IN_APP;
		break;
	}

	FTS_DEBUG("[UPGRADE] chip_id=%02x%02x, chip_types.chip_idh=%02x%02x\n",
		chip_id1, chip_id2,
		fts_chip_type_curr->chip_idh, fts_chip_type_curr->chip_idl);

	/* I2C No ACK 5 times, then return -EIO */
	if (i2c_noack_retry >= 5)
		return -EIO;

	/* not get correct ID, need check pram/rom/bootloader */
	if (i >= 5)
		fw_status = fts_ctpm_get_pram_or_rom_id(client);

	return fw_status;
}

/************************************************************************
* Name: fts_ctpm_check_fw_ver
* Brief: Check vendor id is valid or not
* Input:
* Output:
* Return: 1 - vendor id valid
*         0 - vendor id invalid
***********************************************************************/
static int fts_ctpm_check_fw_ver(struct i2c_client *client)
{
	/* FW version check is done in moto FW upgrade script */
	return 1;
}

/************************************************************************
* Name: fts_ctpm_check_need_upgrade
* Brief:
* Input:
* Output:
* Return: 1 - Need upgrade
*         0 - No upgrade
***********************************************************************/
static int fts_ctpm_check_need_upgrade(struct i2c_client *client)
{
	int fw_status = 0;
	int bUpgradeFlag = 0;

	FTS_FUNC_ENTER();

	/* 1. veriry FW APP is valid or not */
	fw_status = fts_ctpm_check_fw_status(client);
	FTS_DEBUG("[UPGRADE] fw_status = %d\n", fw_status);
	if (fw_status < 0) {
		/* I2C no ACK, return immediately */
		FTS_ERROR("[UPGRADE] I2C NO ACK,exit upgrade\n");
		return -EIO;
	} else if (fw_status == FTS_RUN_IN_ERROR)
		FTS_ERROR("[UPGRADE] IC Type Fail\n");
	else if (fw_status == FTS_RUN_IN_APP) {
		if (fts_ctpm_check_fw_ver(client) == 1) {
			FTS_INFO("[UPGRADE] need upgrade fw\n");
			bUpgradeFlag = 1;
		} else {
			FTS_INFO("[UPGRADE] Don't need upgrade fw\n");
			bUpgradeFlag = 0;
		}
	} else {
		/* if app is invalid, reset to run ROM */
		FTS_INFO("[UPGRADE] FW APP invalid\n");
		fts_ctpm_rom_or_pram_reset(client);

		bUpgradeFlag = 1;
	}

	FTS_FUNC_EXIT();

	return bUpgradeFlag;
}

/************************************************************************
* Name: fts_ctpm_auto_upgrade
* Brief:  auto upgrade
* Input:
* Output:
* Return: 0 - no upgrade
***********************************************************************/
int fts_ctpm_auto_upgrade(struct i2c_client *client,
				const char *fw_name,
				const struct ft_ts_platform_data *pdata)
{
	int i_ret = 0;
	int bUpgradeFlag = 0;
	u8 uc_upgrade_times = 0;

	FTS_DEBUG("[UPGRADE] set update function and type by ID\n");

#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_8716_MMI
	if (pdata->family_id == FT8716_ID) {
		ft8716_set_upgrade_function(&fts_updatefun_curr);
		ft8716_set_chip_id(&fts_chip_type_curr);
	}
#endif
#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_5X46_MMI
	if (pdata->family_id == FT5X46_ID) {
		ft5x46_set_upgrade_function(&fts_updatefun_curr);
		ft5x46_set_chip_id(&fts_chip_type_curr);
	}
#endif
#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_8006U_MMI
	if (pdata->family_id == FT8006U_ID) {
		return fts_fwupg_do_upgrade(fw_name);
	}
#endif

	/* no IC upgrade function enabled, return error */
	if ((fts_updatefun_curr == NULL) || (fts_chip_type_curr == NULL)) {
		FTS_ERROR("[UPGRADE] ID(0x%02x) not support\n",
			pdata->family_id);
		return -EINVAL;
	}
	fts_pdata_curr = pdata;
	fts_fw_name_curr = fw_name;

	FTS_DEBUG("[UPGRADE] check upgrade need or not\n");
	bUpgradeFlag = fts_ctpm_check_need_upgrade(client);
	FTS_DEBUG("[UPGRADE] bUpgradeFlag = 0x%x\n", bUpgradeFlag);
	if (bUpgradeFlag <= 0) {
		FTS_INFO("[UPGRADE] No Upgrade, exit(%d)\n", bUpgradeFlag);
		return 0;
	}

	/* FW Upgrade */
	do {
		FTS_DEBUG("[UPGRADE] star upgrade(%d)\n", uc_upgrade_times);

		i_ret = fts_ctpm_fw_upgrade(client);
		if (i_ret != 0) {
			/* upgrade fail, reset to run ROM BOOT..
			 * if app in flash is ok, TP will work success
			 */
			FTS_ERROR("[UPGRADE] upgrade fail, reset now\n");
			fts_ctpm_rom_or_pram_reset(client);
			continue;
		}
;		FTS_INFO("[UPGRADE] Upgrade success, reset now\n");
		fts_ctpm_rom_or_pram_reset(client);
		break;
	} while (++uc_upgrade_times < 2);

	return i_ret;
}

#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_8006U_MMI
struct fts_upgrade g_upgrade;
struct fts_upgrade *fwupgrade;

struct ft_chip_t ft8006u_fct = {
	.type = 0x0B,
	.chip_idh = 0xF0,
	.chip_idl = 0x06,
	.rom_idh = 0xF0,
	.rom_idl = 0x06,
	.pramboot_idh = 0xF0,
	.pramboot_idl = 0xA6,
	.bootloader_idh = 0x00,
	.bootloader_idl = 0x00,
	.chip_type = 0x8006D80B,
};

struct fts_ts_data g_fts_data;
struct fts_ts_data *fts_data;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static u16 fts_pram_ecc_calc_host(u8 *pbuf, u16 length)
{
	u16 ecc = 0;
	u16 i = 0;
	u16 j = 0;

	for ( i = 0; i < length; i += 2 ) {
		ecc ^= ((pbuf[i] << 8) | (pbuf[i + 1]));
		for (j = 0; j < 16; j ++) {
			if (ecc & 0x01)
				ecc = (u16)((ecc >> 1) ^ AL2_FCS_COEF_8006);
			else
				ecc >>= 1;
		}
	}

	return ecc;
}

/************************************************************************
 * fts_pram_ecc_cal - Calculate and get pramboot ecc
 *
 * return pramboot ecc of tp if success, otherwise return error code
 ***********************************************************************/
static int fts_pram_ecc_cal_algo(
	struct i2c_client *client,
	u32 start_addr,
	u32 ecc_length)
{
	int ret = 0;
	int i = 0;
	int ecc = 0;
	u8 val[2] = { 0 };
	u8 cmd[FTS_ROMBOOT_CMD_ECC_NEW_LEN] = { 0 };

	FTS_DEBUG("[UPGRADE]read out pramboot checksum\n");
	cmd[0] = FTS_ROMBOOT_CMD_ECC;
	cmd[1] = BYTE_OFF_16(start_addr);
	cmd[2] = BYTE_OFF_8(start_addr);
	cmd[3] = BYTE_OFF_0(start_addr);
	cmd[4] = BYTE_OFF_16(ecc_length);
	cmd[5] = BYTE_OFF_8(ecc_length);
	cmd[6] = BYTE_OFF_0(ecc_length);
	ret = fts_i2c_write(client, cmd, FTS_ROMBOOT_CMD_ECC_NEW_LEN);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]write pramboot ecc cal cmd fail\n");
		return ret;
	}

	cmd[0] = FTS_ROMBOOT_CMD_ECC_FINISH;
	for (i = 0; i < 100; i++) {
		msleep(1);
		ret = fts_i2c_read(client, cmd, 1, val, 1);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]ecc_finish read cmd fail\n");
			return ret;
		}
		if (0 == val[0])
			break;
	}
	if (i >= 100) {
		FTS_ERROR("[UPGRADE]wait ecc finish fail\n");
		return -EIO;
	}

	cmd[0] = FTS_CMD_READ_ECC;
	ret = fts_i2c_read(client, cmd, 1, val, 2);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]read pramboot ecc fail\n");
		return ret;
	}

	ecc = ((u16)(val[0] << 8) + val[1]) & 0x0000FFFF;
	return ecc;
}

static int fts_pram_ecc_cal_xor(struct i2c_client *client)
{
	int ret = 0;
	u8 reg_val = 0;

	FTS_DEBUG("[UPGRADE]read out pramboot checksum\n");

	ret = fts_i2c_read_reg(client, FTS_ROMBOOT_CMD_ECC, &reg_val);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]read pramboot ecc fail\n");
		return ret;
	}

	return (int)reg_val;
}

int fts_pram_ecc_cal(struct i2c_client *client, u32 saddr, u32 len)
{
	if ((NULL == fwupgrade) && (NULL == fwupgrade->func)) {
		FTS_ERROR("[UPGRADE]fwupgrade/func is null\n");
		return -EINVAL;
	}

	if (fwupgrade->func->newmode) {
		return fts_pram_ecc_cal_algo(client, saddr, len);
	} else {
		return fts_pram_ecc_cal_xor(client);
	}
}

/************************************************************************
 * fts_pram_write_buf - write pramboot data and calculate ecc
 *
 * return pramboot ecc of host if success, otherwise return error code
 ***********************************************************************/
int fts_pram_write_buf(struct i2c_client *client, u8 *buf, u32 len)
{
	int ret = 0;
	u32 i = 0;
	u32 j = 0;
	u32 offset = 0;
	u32 remainder = 0;
	u32 packet_number;
	u32 packet_len = 0;
	u8 packet_buf[FTS_FLASH_PACKET_LENGTH + FTS_CMD_WRITE_LEN] = { 0 };
	u8 ecc_tmp = 0;
	int ecc_in_host = 0;

	FTS_DEBUG("[UPGRADE]write pramboot to pram\n");
	if ((NULL == fwupgrade) && (NULL == fwupgrade->func)) {
		FTS_ERROR("[UPGRADE]fwupgrade/func is null\n");
		return -EINVAL;
	}

	if (NULL == buf) {
		FTS_ERROR("[UPGRADE]pramboot buf is null\n");
		return -EINVAL;
	}

	FTS_DEBUG("[UPGRADE]pramboot len=%d\n", len);
	if ((len < PRAMBOOT_MIN_SIZE) || (len > PRAMBOOT_MAX_SIZE)) {
		FTS_ERROR("[UPGRADE]pramboot length(%d) fail\n", len);
		return -EINVAL;
	}

	packet_number = len / FTS_FLASH_PACKET_LENGTH;
	remainder = len % FTS_FLASH_PACKET_LENGTH;
	if (remainder > 0)
		packet_number++;
	packet_len = FTS_FLASH_PACKET_LENGTH;

	packet_buf[0] = FTS_ROMBOOT_CMD_WRITE;
	for (i = 0; i < packet_number; i++) {
		offset = i * FTS_FLASH_PACKET_LENGTH;
		packet_buf[1] = BYTE_OFF_16(offset);
		packet_buf[2] = BYTE_OFF_8(offset);
		packet_buf[3] = BYTE_OFF_0(offset);

		/* last packet */
		if ((i == (packet_number - 1)) && remainder)
			packet_len = remainder;

		packet_buf[4] = BYTE_OFF_8(packet_len);
		packet_buf[5] = BYTE_OFF_0(packet_len);

		for (j = 0; j < packet_len; j++) {
			packet_buf[FTS_CMD_WRITE_LEN + j] = buf[offset + j];
			if (!fwupgrade->func->newmode) {
				ecc_tmp ^= packet_buf[FTS_CMD_WRITE_LEN + j];
			}
		}

		ret = fts_i2c_write(client, packet_buf, packet_len + FTS_CMD_WRITE_LEN);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]pramboot write data(%d) fail\n", i);
			return ret;
		}
	}

	if (fwupgrade->func->newmode) {
		ecc_in_host = (int)fts_pram_ecc_calc_host(buf, len);
	} else {
		ecc_in_host = (int)ecc_tmp;
	}

	return ecc_in_host;
}

/************************************************************************
 * fts_pram_start - remap to start pramboot
 *
 * return 0 if success, otherwise return error code
 ***********************************************************************/
int fts_pram_start(struct i2c_client *client)
{
	u8 cmd = FTS_ROMBOOT_CMD_START_APP;
	int ret = 0;

	FTS_DEBUG("[UPGRADE]remap to start pramboot\n");

	ret = fts_i2c_write(client, &cmd, 1);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]write start pram cmd fail\n");
		return ret;
	}
	msleep(FTS_DELAY_PRAMBOOT_START);

	return 0;
}

/************************************************************************
 * fts_pram_init - initialize pramboot
 *
 * return 0 if success, otherwise return error code
***********************************************************************/
static int fts_pram_init(struct i2c_client *client)
{
	int ret = 0;
	u8 reg_val = 0;
	u8 wbuf[3] = { 0 };

	FTS_DEBUG("[UPGRADE]pramboot initialization\n");

	/* read flash ID */
	wbuf[0] = FTS_CMD_FLASH_TYPE;
	ret = fts_i2c_read(client, wbuf, 1, &reg_val, 1);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]read flash type fail\n");
		return ret;
	}

	/* set flash clk */
	wbuf[0] = FTS_CMD_FLASH_TYPE;
	wbuf[1] = reg_val;
	wbuf[2] = 0x00;
	ret = fts_i2c_write(client, wbuf, 3);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]write flash type fail\n");
		return ret;
	}

	return 0;
}

/************************************************************************
* Name: fts_pram_write_init
* Brief: wirte pramboot to pram and initialize
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
int fts_pram_write_init(struct i2c_client *client)
{
	int ret = 0;
	bool state = 0;
	enum FW_STATUS status = FTS_RUN_IN_ERROR;
	struct fts_upgrade *upg = fwupgrade;

	FTS_DEBUG("[UPGRADE]**********pram write and init**********\n");
	if ((NULL == upg) || (NULL == upg->func)) {
		FTS_ERROR("[UPGRADE]upgrade/func is null\n");
		return -EINVAL;
	}

	if (!upg->func->pramboot_supported) {
		FTS_ERROR("[UPGRADE]ic not support pram\n");
		return -EINVAL;
	}

	FTS_DEBUG("[UPGRADE]check whether tp is in romboot or not \n");
	/* need reset to romboot when non-romboot state */
	ret = fts_fwupg_get_boot_state(client, &status);
	if (status != FTS_RUN_IN_ROM) {
		if (FTS_RUN_IN_PRAM == status) {
			FTS_INFO("[UPGRADE]tp is in pramboot, need send reset cmd before upgrade\n");
			ret = fts_pram_init(client);
			if (ret < 0) {
				FTS_ERROR("[UPGRADE]pramboot(before) init fail\n");
				return ret;
			}
		}

		FTS_INFO("[UPGRADE]tp isn't in romboot, need send reset to romboot\n");
		ret = fts_fwupg_reset_to_romboot(client);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]reset to romboot fail\n");
			return ret;
		}
	}

	/* check the length of the pramboot */
	if (upg->func->fts_8006u)
		ret = fts_ft8006u_pram_write_remap(client);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]pram write fail, ret=%d\n", ret);
		return ret;
	}

	FTS_DEBUG("[UPGRADE]after write pramboot, confirm run in pramboot\n");
	state = fts_fwupg_check_state(client, FTS_RUN_IN_PRAM);
	if (!state) {
		FTS_ERROR("[UPGRADE]not in pramboot\n");
		return -EIO;
	}

	ret = fts_pram_init(client);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]pramboot init fail\n");
		return ret;
	}

	return 0;
}
/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(struct i2c_client *client)
{
	int ret = 0;
	int cnt = 0;
	u8 reg_value = 0;
	u8 chip_id = fts_data->ic_info.ids.chip_idh;

	do {
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &reg_value);
		if ((ret < 0) || (reg_value != chip_id)) {
			FTS_DEBUG("[UPGRADE]TP Not Ready, ReadData = 0x%x\n", reg_value);
			FTS_DEBUG("[UPGRADE]TP Not Ready, chip_id = 0x%x\n", chip_id);
		} else if (reg_value == chip_id) {
			FTS_INFO("[UPGRADE]TP Ready, Device ID = 0x%x\n", reg_value);
			return 0;
		}
		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	return -EIO;
}
/************************************************************************
* Name: fts_fwupg_check_fw_valid
* Brief: check fw in tp is valid or not
* Input:
* Output:
* Return: return true if fw is valid, otherwise return false
***********************************************************************/
bool fts_fwupg_check_fw_valid(struct i2c_client *client)
{
	int ret = 0;

	ret = fts_wait_tp_to_valid(client);
	if (ret < 0) {
		FTS_INFO("[UPGRADE]tp fw invaild\n");
		return false;
	}

	FTS_DEBUG("[UPGRADE]tp fw vaild\n");
	return true;
}
/************************************************************************
* HID to standard I2C
***********************************************************************/
void fts_i2c_hid2std(struct i2c_client *client)
{
	int ret = 0;
	u8 buf[3] = {0xeb, 0xaa, 0x09};

	ret = fts_i2c_write(client, buf, 3);
	if (ret < 0)
		FTS_ERROR("[UPGRADE]hid2std cmd write fail\n");
	else {
		msleep(10);
		buf[0] = buf[1] = buf[2] = 0;
		ret = fts_i2c_read(client, NULL, 0, buf, 3);
		if (ret < 0)
			FTS_ERROR("[UPGRADE]hid2std cmd read fail\n");
		else if ((0xeb == buf[0]) && (0xaa == buf[1]) && (0x08 == buf[2])) {
			FTS_DEBUG("[UPGRADE]hidi2c change to stdi2c successful\n");
		} else {
			FTS_ERROR("[UPGRADE]hidi2c change to stdi2c fail\n");
		}
	}
}
/************************************************************************
* Name: fts_fwupg_get_boot_state
* Brief: read boot id(rom/pram/bootloader), confirm boot environment
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
int fts_fwupg_get_boot_state(struct i2c_client *client, enum FW_STATUS *fw_sts)
{
	int ret = 0;
	u8 cmd[4] = { 0 };
	u32 cmd_len = 0;
	u8 val[2] = { 0 };
	struct ft_chip_t ids = fts_data->ic_info.ids;
	struct fts_upgrade *upg = fwupgrade;

	FTS_DEBUG("[UPGRADE]**********read boot id**********\n");
	if ((NULL == fw_sts) || (NULL == upg) || (NULL == upg->func)) {
		FTS_ERROR("[UPGRADE]upgrade/func/fw_sts is null\n");
		return -EINVAL;
	}

	if (upg->func->hid_supported)
		fts_i2c_hid2std(client);

	cmd[0] = FTS_CMD_START1;
	cmd[1] = FTS_CMD_START2;
	ret = fts_i2c_write(client, cmd, 2);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]write 55 aa cmd fail\n");
		return ret;
	}

	msleep(FTS_CMD_START_DELAY);
	cmd[0] = FTS_CMD_READ_ID;
	cmd[1] = cmd[2] = cmd[3] = 0x00;
	if (fts_data->ic_info.is_incell)
		cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
	else
		cmd_len = FTS_CMD_READ_ID_LEN;
	ret = fts_i2c_read(client, cmd, cmd_len, val, 2);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]write 90 cmd fail\n");
		return ret;
	}

	FTS_DEBUG("[UPGRADE]read boot id:0x%02x%02x\n", val[0], val[1]);
	if ((val[0] == ids.rom_idh) && (val[1] == ids.rom_idl)) {
		FTS_INFO("[UPGRADE]tp run in romboot\n");
		*fw_sts = FTS_RUN_IN_ROM;
	} else if ((val[0] == ids.pramboot_idh) && (val[1] == ids.pramboot_idl)) {
		FTS_INFO("[UPGRADE]tp run in pramboot\n");
		*fw_sts = FTS_RUN_IN_PRAM;
	} else if ((val[0] == ids.bootloader_idh) && (val[1] == ids.bootloader_idl)) {
		FTS_INFO("[UPGRADE]tp run in bootloader\n");
		*fw_sts = FTS_RUN_IN_BOOTLOADER;
	}

	return 0;
}

/************************************************************************
* Name: fts_fwupg_check_state
* Brief: confirm tp run in romboot/pramboot/bootloader
* Input:
* Output:
* Return: return true if state is match, otherwise return false
***********************************************************************/
bool fts_fwupg_check_state(struct i2c_client *client, enum FW_STATUS rstate)
{
	int ret = 0;
	int i = 0;
	enum FW_STATUS cstate = FTS_RUN_IN_ERROR;

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		ret = fts_fwupg_get_boot_state(client, &cstate);
		if (cstate == rstate)
			return true;
		msleep(FTS_DELAY_READ_ID);
	}

	return false;
}

/************************************************************************
* Name: fts_fwupg_reset_in_boot
* Brief: RST CMD(07), reset to romboot(bootloader) in boot environment
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
int fts_fwupg_reset_in_boot(struct i2c_client *client)
{
	int ret = 0;
	u8 cmd = FTS_CMD_RESET;

	FTS_DEBUG("[UPGRADE]reset in boot environment\n");
	ret = fts_i2c_write(client, &cmd, 1);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]pram/rom/bootloader reset cmd write fail\n");
		return ret;
	}

	msleep(FTS_DELAY_UPGRADE_RESET);
	return 0;
}

/************************************************************************
* Name: fts_fwupg_reset_to_boot
* Brief: reset to boot environment
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
int fts_fwupg_reset_to_boot(struct i2c_client *client)
{
	int ret = 0;

	FTS_DEBUG("[UPGRADE]send 0xAA and 0x55 to FW, reset to boot environment\n");

	ret = fts_i2c_write_reg(client, FTS_REG_UPGRADE, FTS_UPGRADE_AA);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]write FC=0xAA fail\n");
		return ret;
	}
	msleep(FTS_DELAY_FC_AA);

	ret = fts_i2c_write_reg(client, FTS_REG_UPGRADE, FTS_UPGRADE_55);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]write FC=0x55 fail\n");
		return ret;
	}

	msleep(FTS_DELAY_UPGRADE_RESET);
	return 0;
}

/************************************************************************
* Name: fts_fwupg_reset_to_romboot
* Brief: reset to romboot, to load pramboot
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
int fts_fwupg_reset_to_romboot(struct i2c_client *client)
{
	int ret = 0;
	int i = 0;
	u8 cmd = FTS_CMD_RESET;
	enum FW_STATUS state = FTS_RUN_IN_ERROR;

	ret = fts_i2c_write(client, &cmd, 1);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]pram/rom/bootloader reset cmd write fail\n");
		return ret;
	}
	mdelay(10);

	for (i = 0; i < FTS_UPGRADE_LOOP; i++) {
		ret = fts_fwupg_get_boot_state(client, &state);
		if (FTS_RUN_IN_ROM == state)
			break;
		mdelay(5);
	}
	if (i >= FTS_UPGRADE_LOOP) {
		FTS_ERROR("[UPGRADE]reset to romboot fail\n");
		return -EIO;
	}

	return 0;
}

/************************************************************************
* Name: fts_fwupg_enter_into_boot
* Brief: enter into boot environment, ready for upgrade
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
int fts_fwupg_enter_into_boot(struct i2c_client *client)
{
	int ret = 0;
	bool fwvalid = false;
	bool state = false;
	struct fts_upgrade *upg = fwupgrade;

	FTS_DEBUG("[UPGRADE]***********enter into pramboot/bootloader***********\n");
	if ((NULL == upg) || (NULL == upg->func)) {
		FTS_ERROR("[UPGRADE]upgrade/func is null\n");
		return -EINVAL;
	}

	fwvalid = fts_fwupg_check_fw_valid(client);
	if (fwvalid) {
		ret = fts_fwupg_reset_to_boot(client);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]enter into romboot/bootloader fail\n");
			return ret;
		}
	}

	if (upg->func->pramboot_supported) {
		FTS_DEBUG("[UPGRADE]pram supported, write pramboot and init\n");
		/* pramboot */
		ret = fts_pram_write_init(client);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]pram write_init fail\n");
			return ret;
		}
	} else {
		FTS_DEBUG("[UPGRADE]pram not supported, confirm in bootloader\n");
		/* bootloader */
		state = fts_fwupg_check_state(client, FTS_RUN_IN_BOOTLOADER);
		if (!state) {
			FTS_ERROR("[UPGRADE]fw not in bootloader, fail\n");
			return -EIO;
		}
	}

	return 0;
}

/************************************************************************
 * Name: fts_fwupg_check_flash_status
 * Brief: read status from tp
 * Input: flash_status: correct value from tp
 *        retries: read retry times
 *        retries_delay: retry delay
 * Output:
 * Return: return true if flash status check pass, otherwise return false
***********************************************************************/
static bool fts_fwupg_check_flash_status(
	struct i2c_client *client,
	u16 flash_status,
	int retries,
	int retries_delay)
{
	int ret = 0;
	int i = 0;
	u8 cmd = 0;
	u8 val[FTS_CMD_FLASH_STATUS_LEN] = { 0 };
	u16 read_status = 0;

	for (i = 0; i < retries; i++) {
		cmd = FTS_CMD_FLASH_STATUS;
		ret = fts_i2c_read(client, &cmd , 1, val, FTS_CMD_FLASH_STATUS_LEN);
		read_status = (((u16)val[0]) << 8) + val[1];
		if (flash_status == read_status)
			return true;
		msleep(retries_delay);
	}

	return false;
}

/************************************************************************
 * Name: fts_fwupg_erase
 * Brief: erase flash area
 * Input: delay - delay after erase
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
int fts_fwupg_erase(struct i2c_client *client, u32 delay)
{
	int ret = 0;
	u8 cmd = 0;
	bool flag = false;

	FTS_DEBUG("[UPGRADE]**********erase now**********\n");

	/*send to erase flash*/
	cmd = FTS_CMD_ERASE_APP;
	ret = fts_i2c_write(client, &cmd, 1);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]erase cmd fail\n");
		return ret;
	}
	msleep(delay);

	/* read status 0xF0AA: success */
	flag = fts_fwupg_check_flash_status(client, FTS_CMD_FLASH_STATUS_ERASE_OK,
			FTS_RETRIES_REASE, FTS_RETRIES_DELAY_REASE);
	if (!flag) {
		FTS_ERROR("[UPGRADE]ecc flash status check fail\n");
		return -EIO;
	}

	return 0;
}

/************************************************************************
 * Name: fts_fwupg_ecc_cal
 * Brief: calculate and get ecc from tp
 * Input: saddr - start address need calculate ecc
 *        len - length need calculate ecc
 * Output:
 * Return: return data ecc of tp if success, otherwise return error code
 ***********************************************************************/
int fts_fwupg_ecc_cal(struct i2c_client *client, u32 saddr, u32 len)
{
	int ret = 0;
	u32 i = 0;
	u8 wbuf[FTS_CMD_ECC_CAL_LEN] = { 0 };
	u8 val[FTS_CMD_FLASH_STATUS_LEN] = { 0 };
	u32 packet_num = 0;
	u32 packet_len = 0;
	u32 remainder = 0;
	u32 addr = 0;
	u32 offset = 0;

	FTS_DEBUG( "**********read out checksum**********");

	/* check sum init */
	wbuf[0] = FTS_CMD_ECC_INIT;
	ret = fts_i2c_write(client, wbuf, 1);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]ecc init cmd write fail\n");
		return ret;
	}

	packet_num = len / FTS_MAX_LEN_ECC_CALC;
	remainder = len % FTS_MAX_LEN_ECC_CALC;
	if (remainder)
		packet_num++;
	packet_len = FTS_MAX_LEN_ECC_CALC;
	FTS_DEBUG("[UPGRADE]ecc calc num:%d, remainder:%d\n", packet_num, remainder);

	/* send commond to start checksum */
	wbuf[0] = FTS_CMD_ECC_CAL;
	for (i = 0; i < packet_num; i++) {
		offset = FTS_MAX_LEN_ECC_CALC * i;
		addr = saddr + offset;
		wbuf[1] = BYTE_OFF_16(addr);
		wbuf[2] = BYTE_OFF_8(addr);
		wbuf[3] = BYTE_OFF_0(addr);

		if ((i == (packet_num - 1)) && remainder)
			packet_len = remainder;
		wbuf[4] = BYTE_OFF_8(packet_len);
		wbuf[5] = BYTE_OFF_0(packet_len);

		FTS_DEBUG("[UPGRADE]ecc calc startaddr:0x%04x, len:%d\n", addr, packet_len);
		ret = fts_i2c_write(client, wbuf, FTS_CMD_ECC_CAL_LEN);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]ecc calc cmd write fail\n");
			return ret;
		}

		msleep(packet_len / 256);

		/* read status if check sum is finished */
		ret = fts_fwupg_check_flash_status(client, FTS_CMD_FLASH_STATUS_ECC_OK,
				FTS_RETRIES_ECC_CAL, FTS_RETRIES_DELAY_ECC_CAL);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]ecc flash status read fail\n");
			return ret;
		}
	}

	/* read out check sum */
	wbuf[0] = FTS_CMD_ECC_READ;
	ret = fts_i2c_read(client, wbuf, 1, val, 1);
	if (ret < 0) {
		FTS_ERROR( "ecc read cmd write fail");
		return ret;
	}

	return val[0];
}

/************************************************************************
 * Name: fts_flash_write_buf
 * Brief: write buf data to flash address
 * Input: saddr - start address data write to flash
 *        buf - data buffer
 *        len - data length
 *        delay - delay after write
 * Output:
 * Return: return data ecc of host if success, otherwise return error code
 ***********************************************************************/
int fts_flash_write_buf(
	struct i2c_client *client,
	u32 saddr,
	u8 *buf,
	u32 len,
	u32 delay)
{
	int ret = 0;
	u32 i = 0;
	u32 j = 0;
	u32 packet_number = 0;
	u32 packet_len = 0;
	u32 addr = 0;
	u32 offset = 0;
	u32 remainder = 0;
	u8 packet_buf[FTS_FLASH_PACKET_LENGTH + FTS_CMD_WRITE_LEN] = { 0 };
	u8 ecc_in_host = 0;
	u8 cmd = 0;
	u8 val[FTS_CMD_FLASH_STATUS_LEN] = { 0 };
	u16 read_status = 0;
	u16 wr_ok = 0;

	FTS_DEBUG( "**********write data to flash**********");

	if ((NULL == buf) || (0 == len)) {
		FTS_ERROR("[UPGRADE]buf is NULL or len is 0\n");
		return -EINVAL;
	}

	FTS_DEBUG("[UPGRADE]data buf start addr=0x%x, len=0x%x\n", saddr, len);
	packet_number = len / FTS_FLASH_PACKET_LENGTH;
	remainder = len % FTS_FLASH_PACKET_LENGTH;
	if (remainder > 0)
		packet_number++;
	packet_len = FTS_FLASH_PACKET_LENGTH;
	FTS_DEBUG("[UPGRADE]write data, num:%d remainder:%d\n", packet_number, remainder);

	packet_buf[0] = FTS_CMD_WRITE;
	for (i = 0; i < packet_number; i++) {
		offset = i * FTS_FLASH_PACKET_LENGTH;
		addr = saddr + offset;
		packet_buf[1] = BYTE_OFF_16(addr);
		packet_buf[2] = BYTE_OFF_8(addr);
		packet_buf[3] = BYTE_OFF_0(addr);

		/* last packet */
		if ((i == (packet_number - 1)) && remainder)
			packet_len = remainder;

		packet_buf[4] = BYTE_OFF_8(packet_len);
		packet_buf[5] = BYTE_OFF_0(packet_len);

		for (j = 0; j < packet_len; j++) {
			packet_buf[FTS_CMD_WRITE_LEN + j] = buf[offset + j];
			ecc_in_host ^= packet_buf[FTS_CMD_WRITE_LEN + j];
		}

		ret = fts_i2c_write(client, packet_buf, packet_len + FTS_CMD_WRITE_LEN);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]app write fail\n");
			return ret;
		}
		mdelay(delay);

		/* read status */
		wr_ok = FTS_CMD_FLASH_STATUS_WRITE_OK + addr / packet_len;
		for (j = 0; j < FTS_RETRIES_WRITE; j++) {
			cmd = FTS_CMD_FLASH_STATUS;
			ret = fts_i2c_read(client, &cmd , 1, val, FTS_CMD_FLASH_STATUS_LEN);
			read_status = (((u16)val[0]) << 8) + val[1];
			if (wr_ok == read_status) {
				break;
			}
			mdelay(FTS_RETRIES_DELAY_WRITE);
		}
	}

	return (int)ecc_in_host;
}

/************************************************************************
 * Name: fts_flash_read_buf
 * Brief: read data from flash
 * Input: saddr - start address data write to flash
 *        buf - buffer to store data read from flash
 *        len - read length
 * Output:
 * Return: return 0 if success, otherwise return error code
 *
 * Warning: can't call this function directly, need call in boot environment
 ***********************************************************************/
int fts_flash_read_buf(struct i2c_client *client, u32 saddr, u8 *buf, u32 len)
{
	int ret = 0;
	u32 i = 0;
	u32 packet_number = 0;
	u32 packet_len = 0;
	u32 addr = 0;
	u32 offset = 0;
	u32 remainder = 0;
	u8 wbuf[FTS_CMD_READ_LEN];

	if ((NULL == buf) || (0 == len)) {
		FTS_ERROR("[UPGRADE]buf is NULL or len is 0\n");
		return -EINVAL;
	}

	packet_number = len / FTS_FLASH_PACKET_LENGTH;
	remainder = len % FTS_FLASH_PACKET_LENGTH;
	if (remainder > 0)
		packet_number++;
	packet_len = FTS_FLASH_PACKET_LENGTH;
	FTS_DEBUG("[UPGRADE]read packet_number:%d, remainder:%d\n", packet_number, remainder);

	wbuf[0] = FTS_CMD_READ;
	for (i = 0; i < packet_number; i++) {
		offset = i * FTS_FLASH_PACKET_LENGTH;
		addr = saddr + offset;
		wbuf[1] = BYTE_OFF_16(addr);
		wbuf[2] = BYTE_OFF_8(addr);
		wbuf[3] = BYTE_OFF_0(addr);

		/* last packet */
		if ((i == (packet_number - 1)) && remainder)
			packet_len = remainder;

		ret = fts_i2c_write(client, wbuf, FTS_CMD_READ_LEN);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]pram/bootloader write 03 command fail\n");
			return ret;
		}

		msleep(FTS_CMD_READ_DELAY); /* must wait, otherwise read wrong data */
		ret = fts_i2c_read(client, NULL, 0, buf + offset, packet_len);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]pram/bootloader read 03 command fail\n");
			return ret;
		}
	}

	return 0;
}

/************************************************************************
 * Name: fts_flash_read
 * Brief:
 * Input:  addr  - address of flash
 *         len   - length of read
 * Output: buf   - data read from flash
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
int fts_flash_read(struct i2c_client *client, u32 addr, u8 *buf, u32 len)
{
	int ret = 0;

	FTS_DEBUG("[UPGRADE]***********read flash***********\n");

	if ((NULL == buf) || (0 == len)) {
		FTS_ERROR("[UPGRADE]buf is NULL or len is 0\n");
		return -EINVAL;
	}

	ret = fts_fwupg_enter_into_boot(client);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]enter into pramboot/bootloader fail\n");
		goto read_flash_err;
	}

	ret = fts_flash_read_buf(client, addr, buf, len);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]read flash fail\n");
		goto read_flash_err;
	}

read_flash_err:
	/* reset to normal boot */
	ret = fts_fwupg_reset_in_boot(client);
	if (ret < 0)
		FTS_ERROR("[UPGRADE]reset to normal boot fail\n");
	return ret;
}

/************************************************************************
 * Name: fts_read_file
 * Brief:  read file
 * Input: file name
 * Output:
 * Return: return file len if succuss, otherwise return error code
 ***********************************************************************/
int fts_read_file(char *file_name, u8 **file_buf)
{
	int ret = 0;
	char file_path[FILE_NAME_LENGTH] = { 0 };
	struct file *filp = NULL;
	struct inode *inode;
	mm_segment_t old_fs;
	loff_t pos;
	loff_t file_len = 0;

	if ((NULL == file_name) || (NULL == file_buf)) {
		FTS_ERROR("[UPGRADE]filename/filebuf is NULL\n");
		return -EINVAL;
	}

	snprintf(file_path, FILE_NAME_LENGTH, "%s%s", FTS_FW_BIN_FILEPATH, file_name);
	filp = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		FTS_ERROR("[UPGRADE]open %s file fail\n", file_path);
		return -ENOENT;
	}

	inode = filp->f_inode;
	file_len = inode->i_size;
	*file_buf = (u8 *)vmalloc(file_len);
	if (NULL == *file_buf) {
		FTS_ERROR("[UPGRADE]file buf malloc fail\n");
		filp_close(filp, NULL);
		return -ENOMEM;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;
	ret = vfs_read(filp, *file_buf, file_len , &pos);
	if (ret < 0)
		FTS_ERROR("[UPGRADE]read file fail\n");
	FTS_DEBUG("[UPGRADE]file len:%x read len:%x pos:%x\n", (u32)file_len, ret, (u32)pos);
	filp_close(filp, NULL);
	set_fs(old_fs);

	return ret;
}
#define FTS_USE_FIRMWARE 1
/************************************************************************
* Name: fts_upgrade_bin
* Brief:
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
int fts_upgrade_bin(struct i2c_client *client, char *fw_name, bool force)
{
	int ret = 0;
	u32 fw_file_len = 0;
	u8 *fw_file_buf;
	struct fts_upgrade *upg = fwupgrade;
#if FTS_USE_FIRMWARE
	const struct firmware *fw;
#endif

	FTS_DEBUG("[UPGRADE]start upgrade with fw bin\n");
	if ((NULL == upg) || (NULL == upg->func)) {
		FTS_ERROR("[UPGRADE]upgrade/func is null\n");
		return -EINVAL;
	}

#if FTS_USE_FIRMWARE
	ret = request_firmware(&fw, fw_name, &client->dev);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE] Request firmware failed - %s (%d)\n",
			fw_name, ret);
		return ret;
	}
	fw_file_buf = (u8*)fw->data;
	fw_file_len = (u32)fw->size;
#else
	ret = fts_read_file(fw_name, &fw_file_buf);
	if ((ret < 0) || (ret < FTS_MIN_LEN) || (ret > FTS_MAX_LEN_FILE)) {
		FTS_ERROR("[UPGRADE]read fw bin file(sdcard) fail, len:%d\n", ret);
		goto err_bin;
	}
	fw_file_len = ret;
#endif
	FTS_DEBUG("[UPGRADE]fw bin file len:%x\n", fw_file_len);
	if (force) {
		if (upg->func->force_upgrade) {
			ret = upg->func->force_upgrade(client, fw_file_buf, fw_file_len);
		} else {
			FTS_INFO("[UPGRADE]force_upgrade function is null, no upgrade\n");
			goto err_bin;
		}
	} else {
		if (FTS_AUTO_LIC_UPGRADE_EN) {
			if (upg->func->lic_upgrade)
				ret = upg->func->lic_upgrade(client, fw_file_buf, fw_file_len);
			else
				FTS_INFO("[UPGRADE]lic_upgrade function is null, no upgrade\n");
		}
		if (upg->func->upgrade)
			ret = upg->func->upgrade(client, fw_file_buf, fw_file_len);
		else
			FTS_INFO("[UPGRADE]upgrade function is null, no upgrade\n");
	}

	if (ret < 0) {
		FTS_ERROR("[UPGRADE]upgrade fw bin failed\n");
		fts_fwupg_reset_in_boot(client);
		goto err_bin;
	}

	FTS_INFO("[UPGRADE]upgrade fw bin success\n");

err_bin:
#if FTS_USE_FIRMWARE
	release_firmware(fw);
#else
	if (fw_file_buf) {
		vfree(fw_file_buf);
		fw_file_buf = NULL;
	}
#endif
	return ret;
}

static int fts_lic_get_vid_in_tp(struct i2c_client *client, u16 *vid)
{
	int ret = 0;
	u8 val[2] = { 0 };

	if (NULL == vid) {
		FTS_ERROR("[UPGRADE]vid is NULL\n");
		return -EINVAL;
	}

	ret = fts_i2c_read_reg(client, FTS_REG_VENDOR_ID, &val[0]);
	if (fts_data->ic_info.is_incell)
		ret = fts_i2c_read_reg(client, FTS_REG_MODULE_ID, &val[1]);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]read vid from tp fail\n");
		return ret;
	}

	*vid = *(u16 *)val;
	return 0;
}

static int fts_lic_get_vid_in_host(u16 *vid)
{
	u8 val[2] = { 0 };
	u8 *licbuf = NULL;
	u32 conf_saddr = 0;
	struct fts_upgrade *upg = fwupgrade;

	if (!upg || !upg->func || !upg->lic || !vid) {
		FTS_ERROR("[UPGRADE]upgrade/func/get_hlic_ver/lic/vid is null\n");
		return -EINVAL;
	}

	if (upg->lic_length < FTS_MAX_LEN_SECTOR) {
		FTS_ERROR("[UPGRADE]lic length(%x) fail\n", upg->lic_length);
		return -EINVAL;
	}

	licbuf  = upg->lic;
	conf_saddr = upg->func->fwcfgoff;
	val[0] = licbuf[conf_saddr + FTS_CONIFG_VENDORID_OFF];
	if (fts_data->ic_info.is_incell)
		val[1] = licbuf[conf_saddr + FTS_CONIFG_MODULEID_OFF];

	*vid = *(u16 *)val;
	return 0;
}

static int fts_lic_get_ver_in_tp(struct i2c_client *client, u8 *ver)
{
	int ret = 0;

	if (NULL == ver) {
		FTS_ERROR("[UPGRADE]ver is NULL\n");
		return -EINVAL;
	}

	ret = fts_i2c_read_reg(client, FTS_REG_LIC_VER, ver);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]read lcd initcode ver from tp fail\n");
		return ret;
	}

	return 0;
}

static int fts_lic_get_ver_in_host(u8 *ver)
{
	int ret = 0;
	struct fts_upgrade *upg = fwupgrade;

	if (!upg || !upg->func || !upg->func->get_hlic_ver || !upg->lic) {
		FTS_ERROR("[UPGRADE]upgrade/func/get_hlic_ver/lic is null\n");
		return -EINVAL;
	}

	ret = upg->func->get_hlic_ver(upg->lic);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]get host lcd initial code version fail\n");
		return ret;
	}

	*ver = (u8)ret;
	return ret;
}

/* check if lcd init code need upgrade
* true-need  false-no need
*/
static bool fts_lic_need_upgrade(struct i2c_client *client)
{
	int ret = 0;
	u8 initcode_ver_in_tp = 0;
	u8 initcode_ver_in_host = 0;
	u16 vid_in_tp = 0;
	u16 vid_in_host = 0;
	bool fwvalid = false;

	fwvalid = fts_fwupg_check_fw_valid(client);
	if ( !fwvalid) {
		FTS_INFO("[UPGRADE]fw is invalid, no upgrade lcd init code\n");
		return false;
	}

	ret = fts_lic_get_vid_in_host(&vid_in_host);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]vendor id in host invalid\n");
		return false;
	}

	ret = fts_lic_get_vid_in_tp(client, &vid_in_tp);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]vendor id in tp invalid\n");
		return false;
	}

	FTS_DEBUG("[UPGRADE]vid in tp:%x, host:%x\n", vid_in_tp, vid_in_host);
	if (vid_in_tp != vid_in_host) {
		FTS_INFO("[UPGRADE]vendor id in tp&host are different, no upgrade lic\n");
		return false;
	}

	ret = fts_lic_get_ver_in_host(&initcode_ver_in_host);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]init code in host invalid\n");
		return false;
	}

	ret = fts_lic_get_ver_in_tp(client, &initcode_ver_in_tp);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]read reg0xE4 fail\n");
		return false;
	}

	FTS_DEBUG("[UPGRADE]lcd initial code version in tp:%x, host:%x\n",
			initcode_ver_in_tp, initcode_ver_in_host);
	if (0xA5 == initcode_ver_in_tp) {
		FTS_INFO("[UPGRADE]lcd init code ver is 0xA5, don't upgade init code\n");
		return false;
	} else if (0xFF == initcode_ver_in_tp) {
		FTS_DEBUG("[UPGRADE]lcd init code in tp is invalid, need upgrade init code\n");
		return true;
	} else if (initcode_ver_in_tp < initcode_ver_in_host)
		return true;
	else
		return false;
}

int fts_lic_upgrade(struct i2c_client *client, struct fts_upgrade *upg)
{
	int ret = 0;
	bool hlic_upgrade = false;
	int upgrade_count = 0;
	u8 ver = 0;

	FTS_DEBUG("[UPGRADE]lcd initial code auto upgrade function\n");
	if ((!upg) || (!upg->func) || (!upg->func->lic_upgrade)) {
		FTS_ERROR("[UPGRADE]lcd upgrade function is null\n");
		return -EINVAL;
	}

	hlic_upgrade = fts_lic_need_upgrade(client);
	FTS_DEBUG("[UPGRADE]lcd init code upgrade flag:%d\n", hlic_upgrade);
	if (hlic_upgrade) {
		FTS_INFO("[UPGRADE]lcd initial code need upgrade, upgrade begin...\n");
		do {
			FTS_INFO("[UPGRADE]lcd initial code upgrade times:%d\n", upgrade_count);
			upgrade_count++;

			ret = upg->func->lic_upgrade(client, upg->lic, upg->lic_length);
			if (ret < 0) {
				fts_fwupg_reset_in_boot(client);
			} else {
				fts_lic_get_ver_in_tp(client, &ver);
				FTS_INFO("[UPGRADE]success upgrade to lcd initcode ver:%02x\n", ver);
				break;
			}
		} while (upgrade_count < 2);
	} else {
		FTS_INFO("[UPGRADE]lcd initial code don't need upgrade\n");
	}

	return ret;
}


static int fts_param_get_ver_in_tp(struct i2c_client *client, u8 *ver)
{
	int ret = 0;

	if (NULL == ver) {
		FTS_ERROR("[UPGRADE]ver is NULL\n");
		return -EINVAL;
	}

	ret = fts_i2c_read_reg(client, FTS_REG_IDE_PARA_VER_ID, ver);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]read fw param ver from tp fail\n");
		return ret;
	}

	if ((0x00 == *ver) || (0xFF == *ver)) {
		FTS_INFO("[UPGRADE]param version in tp invalid\n");
		return -EIO;
	}

	return 0;
}

static int fts_param_get_ver_in_host(u8 *ver)
{
	struct fts_upgrade *upg = fwupgrade;

	if ((!upg) || (!upg->func) || (!upg->fw) || (!ver)) {
		FTS_ERROR("[UPGRADE]fts_data/upgrade/func/fw/ver is NULL\n");
		return -EINVAL;
	}

	if (upg->fw_length < upg->func->paramcfgveroff) {
		FTS_ERROR("[UPGRADE]fw len(%x) < paramcfg ver offset(%x)\n",
				upg->fw_length, upg->func->paramcfgveroff);
		return -EINVAL;
	}

	FTS_DEBUG("[UPGRADE]fw paramcfg version offset:%x\n", upg->func->paramcfgveroff);
	*ver = upg->fw[upg->func->paramcfgveroff];

	if ((0x00 == *ver) || (0xFF == *ver)) {
		FTS_INFO("[UPGRADE]param version in host invalid\n");
		return -EIO;
	}

	return 0;
}

/************************************************************************
 * fts_param_need_upgrade - check fw paramcfg need upgrade or not
 *
 * Return: return true if paramcfg need upgrade
 ***********************************************************************/
static bool fts_param_need_upgrade(struct i2c_client *client)
{
	int ret = 0;
	u8 val = 0;
	u8 ver_in_host = 0;
	u8 ver_in_tp = 0;
	bool fwvalid = false;

	fwvalid = fts_fwupg_check_fw_valid(client);
	if ( !fwvalid) {
		FTS_INFO("[UPGRADE]fw is invalid, no upgrade paramcfg\n");
		return false;
	}

	ret = fts_param_get_ver_in_host(&ver_in_host);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]param version in host invalid\n");
		return false;
	}

	ret = fts_i2c_read_reg(client, FTS_REG_IDE_PARA_STATUS, &val);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]read IDE PARAM STATUS in tp fail\n");
		return false;
	}

	if ((val & 0x80) != 0x80) {
		FTS_INFO("[UPGRADE]no IDE VER in tp\n");
		return false;
	} else if ((val & 0x7F) != 0x00) {
		FTS_INFO("[UPGRADE]IDE VER, param invalid, need upgrade param\n");
		return true;
	}

	ret = fts_param_get_ver_in_tp(client, &ver_in_tp);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]get IDE param ver in tp fail\n");
		return false;
	}

	FTS_INFO("[UPGRADE]fw paramcfg version in tp:%x, host:%x\n", ver_in_tp, ver_in_host);
	if (ver_in_tp < ver_in_host)
		return true;

	return false;
}

/************************************************************************
 * fts_fwupg_get_ver_in_tp - read fw ver from tp register
 *
 * return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_fwupg_get_ver_in_tp(struct i2c_client *client, u8 *ver)
{
	int ret = 0;

	if (NULL == ver) {
		FTS_ERROR("[UPGRADE]ver is NULL\n");
		return -EINVAL;
	}

	ret = fts_i2c_read_reg(client, FTS_REG_FW_VER, ver);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]read fw ver from tp fail\n");
		return ret;
	}

	return 0;
}

/************************************************************************
 * fts_fwupg_get_ver_in_host - read fw ver in host fw image
 *
 * return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_fwupg_get_ver_in_host(u8 *ver)
{
	struct fts_upgrade *upg = fwupgrade;

	if ((!upg) || (!upg->func) || (!upg->fw) || (!ver)) {
		FTS_ERROR("[UPGRADE]fts_data/upgrade/func/fw/ver is NULL\n");
		return -EINVAL;
	}

	if (upg->fw_length < upg->func->fwveroff) {
		FTS_ERROR("[UPGRADE]fw len(%x) < fw ver offset(%x)\n",
				upg->fw_length, upg->func->fwveroff);
		return -EINVAL;
	}

	FTS_DEBUG("[UPGRADE]fw version offset:%x\n", upg->func->fwveroff);
	*ver = upg->fw[upg->func->fwveroff];
	return 0;
}

/************************************************************************
 * fts_fwupg_need_upgrade - check fw need upgrade or not
 *
 * Return: return true if fw need upgrade
 ***********************************************************************/
static bool fts_fwupg_need_upgrade(struct i2c_client *client)
{
	int ret = 0;
	bool fwvalid = false;
	u8 fw_ver_in_host = 0;
	u8 fw_ver_in_tp = 0;

	fwvalid = fts_fwupg_check_fw_valid(client);
	if (fwvalid) {
		ret = fts_fwupg_get_ver_in_host(&fw_ver_in_host);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]get fw ver in host fail\n");
			return false;
		}

		ret = fts_fwupg_get_ver_in_tp(client, &fw_ver_in_tp);
		if (ret < 0) {
			FTS_ERROR("[UPGRADE]get fw ver in tp fail\n");
			return false;
		}

		FTS_INFO("[UPGRADE]fw version in tp:%x, host:%x\n", fw_ver_in_tp, fw_ver_in_host);
		if (fw_ver_in_tp < fw_ver_in_host)
			return true;
	} else {
		FTS_INFO("[UPGRADE]fw invalid, need upgrade fw\n");
		return true;
	}

	return false;
}

/************************************************************************
 * Name: fts_fw_upgrade
 * Brief: fw upgrade main entry
 * Input:
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
int fts_fwupg_upgrade(struct i2c_client *client, struct fts_upgrade *upg)
{
	int ret = 0;
	bool upgrade_flag = false;
	int upgrade_count = 0;
	u8 ver = 0;

	FTS_DEBUG("[UPGRADE]fw auto upgrade function\n");
	if ((NULL == upg) || (NULL == upg->func)) {
		FTS_ERROR("[UPGRADE]upg/upg->func is null\n");
		return -EINVAL;
	}

	upgrade_flag = fts_fwupg_need_upgrade(client);
	FTS_DEBUG("[UPGRADE]fw upgrade flag:%d\n", upgrade_flag);
	do {
		upgrade_count++;
		if (upgrade_flag) {
			FTS_INFO("[UPGRADE]upgrade fw app(times:%d)\n", upgrade_count);
			if (upg->func->upgrade) {
				ret = upg->func->upgrade(client, upg->fw, upg->fw_length);
				if (ret < 0) {
					fts_fwupg_reset_in_boot(client);
				} else {
					fts_fwupg_get_ver_in_tp(client, &ver);
					FTS_INFO("[UPGRADE]success upgrade to fw version %02x\n", ver);
					break;
				}
			} else {
				FTS_ERROR("[UPGRADE]upgrade func/upgrade is null, return immediately\n");
				ret = -ENODATA;
				break;
			}
		} else {
			FTS_INFO("[UPGRADE]fw don't need upgrade\n");
			if (upg->func->param_upgrade) {
				if (fts_param_need_upgrade(client)) {
					FTS_INFO("[UPGRADE]upgrade param area(times:%d)\n", upgrade_count);
					ret = upg->func->param_upgrade(client, upg->fw, upg->fw_length);
					if (ret < 0) {
						fts_fwupg_reset_in_boot(client);
					} else {
						fts_param_get_ver_in_tp(client, &ver);
						FTS_INFO("[UPGRADE]success upgrade to fw param version %02x\n", ver);
						break;
					}
				} else {
					FTS_INFO("[UPGRADE]param don't need upgrade\n");
					break;
				}
			} else
				break;
		}
	} while (upgrade_count < 2);

	return ret;
}
bool FTS_AUTO_LIC_UPGRADE_EN = false;

/*
 * fts_fwupg_init_ic_detail - for ic detail initialaztion
 */
static void fts_fwupg_init_ic_detail(void)
{
	struct fts_upgrade *upg = fwupgrade;

	if (upg && upg->func && upg->func->init)
		upg->func->init();
}
/*
 * fts_fwupg_do_upgrade - fw upgrade work function
 * 1. get fw image/file
 * 2. call upgrade main function(fts_fwupg_auto_upgrade)
 */
int fts_fwupg_do_upgrade(const char *fwname)
{
	struct fts_ts_data *ts_data = fts_data;
	fts_fwupg_init_ic_detail();
	return fts_upgrade_bin(ts_data->client, (char*)fwname, false);
}

int fts_extra_exit(void)
{
	return 0;
}

int fts_extra_init(struct i2c_client *client, struct input_dev *input_dev, struct ft_ts_platform_data *pdata)
{
	int type = pdata->family_id;

	fts_data = &g_fts_data;
	fwupgrade = &g_upgrade;
	fts_data->client = client;
	fts_data->input_dev = input_dev;
	fts_data->pdata = pdata;

	if (FT8006U_ID == type) {
#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_8006U_MMI
		FTS_AUTO_LIC_UPGRADE_EN = true;
		fwupgrade->func = &upgrade_func_ft8006u;
		fts_data->ic_info.ids = ft8006u_fct;
		fts_data->ic_info.is_incell = true;
		fts_data->ic_info.hid_supported = false;
#endif
	}
	return 0;
}
#endif
