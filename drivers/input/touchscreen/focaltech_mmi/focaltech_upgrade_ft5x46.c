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

#include <linux/firmware.h>
#include "focaltech_flash.h"
#include "focaltech_upgrade_common.h"

#define APP_FILE_MAX_SIZE           (60 * 1024)
#define APP_FILE_MIN_SIZE           (8)
#define CONFIG_START_ADDR           (0xD780)
#define CONFIG_VENDOR_ID_OFFSET     (0x04)
#define CONFIG_PROJECT_ID_OFFSET    (0x20)
#define CONFIG_VENDOR_ID_ADDR       (CONFIG_START_ADDR+CONFIG_VENDOR_ID_OFFSET)
#define CONFIG_PROJECT_ID_ADDR      (CONFIG_START_ADDR+CONFIG_PROJECT_ID_OFFSET)

static int fts_5x46_get_app_bin_file_ver(const char *firmware_name);
static int fts_5x46_fw_upgrade_with_app_bin_file(
		struct i2c_client *client, const char *firmware_name);


static struct fts_upgrade_fun fts_updatefun_5x46 = {
	.get_app_bin_file_ver = fts_5x46_get_app_bin_file_ver,
	.upgrade_with_app_bin_file = fts_5x46_fw_upgrade_with_app_bin_file,
	.upgrade_with_lcd_cfg_bin_file = NULL,
};

static struct ft_chip_t fts_chip_type_5x46 = {
	.type = 0x02,
	.chip_idh = 0x54,
	.chip_idl = 0x22,
	.rom_idh = 0x54,
	.rom_idl = 0x22,
	.pramboot_idh = 0x00,
	.pramboot_idl = 0x00,
	.bootloader_idh = 0x54,
	.bootloader_idl = 0x2C,
	.chip_type = 0x54220002,
};

inline void ft5x46_set_upgrade_function(struct fts_upgrade_fun **curr)
{
	*curr = &fts_updatefun_5x46;
}
inline void ft5x46_set_chip_id(struct ft_chip_t **curr)
{
	*curr = &fts_chip_type_5x46;
}

static int fts_5x46_get_app_bin_file_ver(const char *firmware_name)
{
	int fw_ver = 0;
	int rc;
	const struct firmware *fw = NULL;

	FTS_FUNC_ENTER();

	rc = request_firmware(&fw, firmware_name, NULL);
	if (rc < 0) {
		FTS_ERROR("[UPGRADE] Request firmware failed - %s (%d)\n",
							firmware_name, rc);
		return rc;
	}
	if (fw->size < APP_FILE_MIN_SIZE || fw->size > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE] FW length(%zd) error\n", fw->size);
		return -EIO;
		goto file_ver_rel_fw;
	}

	fw_ver = fw->data[fw->size - 2];

file_ver_rel_fw:
	release_firmware(fw);
	FTS_FUNC_EXIT();

	return fw_ver;
}


/************************************************************************
* Name: fts_ft5436_upgrade_use_buf
* Brief: fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
static int fts_ft5x46_upgrade_use_buf(struct i2c_client *client,
					const u8 *pbt_buf, u32 dw_lenth)
{
    u8 reg_val[4] = {0};
    u32 i = 0;
    u32 packet_number;
    u32 j = 0;
    u32 temp;
    u32 lenght;
    u8 packet_buf[FTS_PACKET_LENGTH + 6];
    u8 auc_i2c_write_buf[10];
    u8 upgrade_ecc;
    int i_ret;

    fts_ctpm_i2c_hid2std(client);

    for (i = 0; i < FTS_UPGRADE_LOOP; i++)
    {
        /*********Step 1:Reset  CTPM *****/
        fts_i2c_write_reg(client, FTS_RST_CMD_REG1, FTS_UPGRADE_AA);
        msleep(10);
        fts_i2c_write_reg(client, FTS_RST_CMD_REG1, FTS_UPGRADE_55);
        msleep(200);

        /*********Step 2:Enter upgrade mode *****/
        fts_ctpm_i2c_hid2std(client);
        msleep(5);

        auc_i2c_write_buf[0] = FTS_UPGRADE_55;
        auc_i2c_write_buf[1] = FTS_UPGRADE_AA;
        i_ret = fts_i2c_write(client, auc_i2c_write_buf, 2);
        if (i_ret < 0)
        {
            FTS_ERROR("[UPGRADE]: failed writing  0x55 and 0xaa!!");
            continue;
        }

        /*********Step 3:Check bootloader ID *****/
        msleep(1);
        auc_i2c_write_buf[0] = FTS_READ_ID_REG;
        auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
        reg_val[0] = reg_val[1] = 0x00;
        fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
        if ((reg_val[0] == 0x54) && (reg_val[1] != 0))
        {
            FTS_DEBUG("[UPGRADE]: read bootload id ok!! ID1 = 0x%x,ID2 = 0x%x!!",reg_val[0], reg_val[1]);
            break;
        }
        else
        {
            FTS_ERROR("[UPGRADE]: read bootload id fail!! ID1 = 0x%x,ID2 = 0x%x!!",reg_val[0], reg_val[1]);
            continue;
        }
    }

    if (i >= FTS_UPGRADE_LOOP)
    {
        FTS_ERROR("[UPGRADE]: failed writing  0x55 and 0xaa : i = %d!!", i);
        return -EIO;
    }

    /*Step 4:erase app and panel paramenter area*/
    FTS_DEBUG("[UPGRADE]: erase app and panel paramenter area!!");
    auc_i2c_write_buf[0] = FTS_ERASE_APP_REG;
    fts_i2c_write(client, auc_i2c_write_buf, 1);
    msleep(1350);
    for (i = 0; i < 15; i++)
    {
        auc_i2c_write_buf[0] = 0x6a;
        reg_val[0] = reg_val[1] = 0x00;
        fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);
        if ((0xF0 == reg_val[0]) && (0xAA == reg_val[1]))
        {
            break;
        }
        msleep(50);
    }
    FTS_DEBUG("[UPGRADE]: erase app area reg_val[0] = %x reg_val[1] = %x!!", reg_val[0], reg_val[1]);

    auc_i2c_write_buf[0] = 0xB0;
    auc_i2c_write_buf[1] = (u8) ((dw_lenth >> 16) & 0xFF);
    auc_i2c_write_buf[2] = (u8) ((dw_lenth >> 8) & 0xFF);
    auc_i2c_write_buf[3] = (u8) (dw_lenth & 0xFF);
    fts_i2c_write(client, auc_i2c_write_buf, 4);

    /*********Step 5:write firmware(FW) to ctpm flash*********/
    upgrade_ecc = 0;
    FTS_DEBUG("[UPGRADE]: write FW to ctpm flash!!");
    temp = 0;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = FTS_FW_WRITE_CMD;
    packet_buf[1] = 0x00;

    for (j = 0; j < packet_number; j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8) (temp >> 8);
        packet_buf[3] = (u8) temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (u8) (lenght >> 8);
        packet_buf[5] = (u8) lenght;
        for (i = 0; i < FTS_PACKET_LENGTH; i++)
        {
            packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
            upgrade_ecc ^= packet_buf[6 + i];
        }
        fts_i2c_write(client, packet_buf, FTS_PACKET_LENGTH + 6);
        msleep(10);

        for (i = 0; i < 30; i++)
        {
            auc_i2c_write_buf[0] = 0x6a;
            reg_val[0] = reg_val[1] = 0x00;
            fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);
            if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
            {
                break;
            }
            FTS_DEBUG("[UPGRADE]: reg_val[0] = %x reg_val[1] = %x!!", reg_val[0], reg_val[1]);
            //msleep(1);
            msleep(1000);
        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8) (temp >> 8);
        packet_buf[3] = (u8) temp;
        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (u8) (temp >> 8);
        packet_buf[5] = (u8) temp;
        for (i = 0; i < temp; i++)
        {
            packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
            upgrade_ecc ^= packet_buf[6 + i];
        }
        fts_i2c_write(client, packet_buf, temp + 6);
        msleep(10);

        for (i = 0; i < 30; i++)
        {
            auc_i2c_write_buf[0] = 0x6a;
            reg_val[0] = reg_val[1] = 0x00;
            fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);

            if ((0x1000 + ((packet_number * FTS_PACKET_LENGTH)/((dw_lenth) % FTS_PACKET_LENGTH))) == (((reg_val[0]) << 8) | reg_val[1]))
            {
                break;
            }
            FTS_DEBUG("[UPGRADE]: reg_val[0] = %x reg_val[1] = %x  reg_val[2] = 0x%x!!", reg_val[0], reg_val[1], (((packet_number * FTS_PACKET_LENGTH)/((dw_lenth) % FTS_PACKET_LENGTH))+0x1000));
            //msleep(1);
            msleep(100);
        }
    }

    msleep(50);

    /*********Step 6: read out checksum***********************/
    /*send the opration head */
    FTS_DEBUG("[UPGRADE]: read out checksum!!");
    auc_i2c_write_buf[0] = 0x64;
    fts_i2c_write(client, auc_i2c_write_buf, 1);
    msleep(300);

    temp = 0;
    auc_i2c_write_buf[0] = 0x65;
    auc_i2c_write_buf[1] = (u8)(temp >> 16);
    auc_i2c_write_buf[2] = (u8)(temp >> 8);
    auc_i2c_write_buf[3] = (u8)(temp);
    temp = dw_lenth;
    auc_i2c_write_buf[4] = (u8)(temp >> 8);
    auc_i2c_write_buf[5] = (u8)(temp);
    i_ret = fts_i2c_write(client, auc_i2c_write_buf, 6);
    msleep(dw_lenth/256);

    for (i = 0; i < 100; i++)
    {
        auc_i2c_write_buf[0] = 0x6a;
        reg_val[0] = reg_val[1] = 0x00;
        fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 2);
        FTS_DEBUG("[UPGRADE]: reg_val[0]=%02x reg_val[0]=%02x!!", reg_val[0], reg_val[1]);
        if ((0xF0 == reg_val[0]) && (0x55 == reg_val[1]))
        {
            break;
        }
        msleep(1);
    }
    auc_i2c_write_buf[0] = 0x66;
    fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 1);
    if (reg_val[0] != upgrade_ecc)
    {
        FTS_ERROR("[UPGRADE]: ecc error! FW=%02x upgrade_ecc=%02x!!", reg_val[0], upgrade_ecc);
        return -EIO;
    }
    FTS_DEBUG("[UPGRADE]: checksum %x %x!!",reg_val[0],upgrade_ecc);

    FTS_DEBUG("[UPGRADE]: reset the new FW!!");
    auc_i2c_write_buf[0] = FTS_REG_RESET_FW;
    fts_i2c_write(client, auc_i2c_write_buf, 1);
    msleep(200);

    fts_ctpm_i2c_hid2std(client);

    return 0;
}

#define AL2_FCS_COEF    ((1 << 7) + (1 << 6) + (1 << 5))
/*****************************************************************************
*   Name: ecc_calc
*  Brief:
*  Input:
* Output:
* Return:
*****************************************************************************/
static u8 ecc_calc(const u8 *pbt_buf, u16 start, u16 length)
{
    u8 cFcs = 0;
    u16 i, j;

    for ( i = 0; i < length; i++ )
    {
        cFcs ^= pbt_buf[start++];
        for (j = 0; j < 8; j ++)
        {
            if (cFcs & 1)
            {
                cFcs = (u8)((cFcs >> 1) ^ AL2_FCS_COEF);
            }
            else
            {
                cFcs >>= 1;
            }
        }
    }
    return cFcs;
}


/*****************************************************************************
* Name: fts_check_app_bin_valid
* Brief:
* Input:
* Output:
* Return:
*****************************************************************************/
static bool fts_5x46_check_app_bin_valid(const u8 *pbt_buf, u32 dw_lenth)
{
    u8 ecc1;
    u8 ecc2;
    u16 len1;
    u16 len2;
    u8 cal_ecc;
    u16 usAddrInfo;

    /* 1. First Byte */
    if (pbt_buf[0] != 0x02)
    {
        FTS_DEBUG("[UPGRADE]APP.BIN Verify- the first byte(%x) error", pbt_buf[0]);
        return false;
    }

    usAddrInfo = dw_lenth - 8;

    /* 2.len */
    len1  = pbt_buf[usAddrInfo++] << 8;
    len1 += pbt_buf[usAddrInfo++];

    len2  = pbt_buf[usAddrInfo++] << 8;
    len2 += pbt_buf[usAddrInfo++];

    if ((len1 + len2) != 0xFFFF)
    {
        FTS_DEBUG("[UPGRADE]APP.BIN Verify- LENGTH(%04x) XOR error", len1);
        return false;
    }

    /* 3.ecc */
    ecc1 = pbt_buf[usAddrInfo++];
    ecc2 = pbt_buf[usAddrInfo++];

    if ((ecc1 + ecc2) != 0xFF)
    {
        FTS_DEBUG("[UPGRADE]APP.BIN Verify- ECC(%x) XOR error", ecc1);
        return false;
    }

    cal_ecc = ecc_calc(pbt_buf, 0x0, len1);

    if (ecc1 != cal_ecc)
    {
        FTS_DEBUG("[UPGRADE]APP.BIN Verify- ECC calc error");
        return false;
    }
    return true;
}

/************************************************************************
* Name: fts_5436_fw_upgrade_with_app_bin_file
* Brief: upgrade with *.bin file
* Input: i2c info, file name
* Output: no
* Return: success =0
***********************************************************************/
static int fts_5x46_fw_upgrade_with_app_bin_file(struct i2c_client *client, const char *firmware_name)
{
	int i_ret = 0;
	bool ecc_ok = false;
	const struct firmware *fw = NULL;

	FTS_INFO("[UPGRADE] start upgrade with app.bin\n");

	i_ret = request_firmware(&fw, firmware_name, &client->dev);
	if (i_ret < 0) {
		FTS_ERROR("[UPGRADE] Request firmware failed - %s (%d)\n",
							firmware_name, i_ret);
		return i_ret;
	}

	if (fw->size < APP_FILE_MIN_SIZE || fw->size > APP_FILE_MAX_SIZE) {
		FTS_ERROR("[UPGRADE] FW length(%zd) error\n", fw->size);
		i_ret = -EINVAL;
		goto file_upgrade_rel_fw;
	}

    ecc_ok = fts_5x46_check_app_bin_valid(fw->data, fw->size);

    if (ecc_ok)
    {
        FTS_INFO("[UPGRADE] app.bin ecc ok");
        i_ret = fts_ft5x46_upgrade_use_buf(client, fw->data, fw->size);
        if (i_ret != 0)
        {
            FTS_ERROR("[UPGRADE]: upgrade app.bin failed");
        }
        else
        {
            FTS_INFO("[UPGRADE]: upgrade app.bin succeed");
        }
    }
    else
    {
        FTS_ERROR("[UPGRADE] app.bin ecc failed");
    }

    return i_ret;
	
file_upgrade_rel_fw:
	release_firmware(fw);
	return i_ret;
}

