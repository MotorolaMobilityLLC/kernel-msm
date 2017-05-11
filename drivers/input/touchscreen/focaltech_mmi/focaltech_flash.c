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

#ifdef CONFIG_TOUCHSCREEN_FOCALTECH_UPGRADE_8716
	if (pdata->family_id == FT8716_ID) {
		ft8716_set_upgrade_function(&fts_updatefun_curr);
		ft8716_set_chip_id(&fts_chip_type_curr);
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

