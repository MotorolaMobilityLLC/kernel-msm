/*
 *
 * FocalTech fts TouchScreen driver.
 *
 * Copyright (c) 2010-2017, Focaltech Ltd. All rights reserved.
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

/*****************************************************************************
*
* File Name: focaltech_upgrade_ft8006.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-12-29
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "focaltech_flash.h"

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
#define FT8006M_PRAMBOOT_FW_NAME		"FT8006M_Pramboot.bin"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_MAX_LEN_APP_FT8006     (94 * 1024)

#define MAX_BANK_DATA               0x80
#define MAX_GAMMA_LEN               0x180
#define LIC_CHECKSUM_H_OFF          0x00
#define LIC_CHECKSUM_L_OFF          0x01
#define LIC_LCD_ECC_H_OFF           0x04
#define LIC_LCD_ECC_L_OFF           0x05
#define LIC_ECC_REG_H_OFF           0x43D
#define LIC_ECC_REG_L_OFF           0x43C
#define LIC_REG_2                   0xB2

static int gamma_enable[] = { 0x040d, 0x91, 0x80, 0x00, 0x19, 0x01 };
union short_bits {
	u16 dshort;
	struct bits {
		u16 bit0: 1;
		u16 bit1: 1;
		u16 bit2: 1;
		u16 bit3: 1;
		u16 bit4: 1;
		u16 bit5: 1;
		u16 bit6: 1;
		u16 bit7: 1;
		u16 bit8: 1;
		u16 bit9: 1;
		u16 bit10: 1;
		u16 bit11: 1;
		u16 bit12: 1;
		u16 bit13: 1;
		u16 bit14: 1;
		u16 bit15: 1;
	} bits;
};

/* calculate lcd init code ecc */
static int cal_lcdinitcode_ecc(u8 *buf, u16 *ecc_val)
{
	u32 bank_crc_en = 0;
	u8 bank_data[MAX_BANK_DATA] = { 0 };
	u16 bank_len = 0;
	u16 bank_addr = 0;
	u32 bank_num = 0;
	u16 file_len = 0;
	u16 pos = 0;
	int i = 0;
	union short_bits ecc;
	union short_bits ecc_last;
	union short_bits temp_byte;
	u8 bank_mapping[] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9,
						  0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x18,
						  0x19, 0x1A, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x22, 0x23, 0x24
						}; /* Actaul mipi bank */
	u8 banknum = 0;

	ecc.dshort = 0;
	ecc_last.dshort = 0;
	temp_byte.dshort = 0;

	file_len = (u16)(((u16)buf[2] << 8) + buf[3]);
	bank_crc_en = (u32)(((u32)buf[9] << 24) + ((u32)buf[8] << 16) + \
						((u32)buf[7] << 8) + (u32)buf[6]);
	FTS_INFO("[FTS-UPGRADE]lcd init code len=%x bank en=%x\n", file_len, bank_crc_en);

	pos = 0x0A; /*  addr of first bank */
	while (pos < file_len) {
		bank_addr = (u16)(((u16)buf[pos + 0] << 8) + buf[pos + 1]);
		bank_len = (u16)(((u16)buf[pos + 2] << 8) + buf[pos + 3]);
		/*         FTS_INFO("[FTS-UPGRADE]bank pos=%x bank_addr=%x bank_len=%x\n", pos, bank_addr, bank_len); */
		if (bank_len > MAX_BANK_DATA)
			return -EINVAL;
		memset(bank_data, 0, MAX_BANK_DATA);
		memcpy(bank_data, buf + pos + 4, bank_len);

		bank_num = (bank_addr - 0x8000) / MAX_BANK_DATA;
		/*         FTS_INFO("[FTS-UPGRADE]actual mipi bank number = %x\n", bank_num); */
		for (i = 0; i < sizeof(bank_mapping) / sizeof(u8); i++) {
			if (bank_num == bank_mapping[i]) {
				banknum = i;
				break;
			}
		}
		if (i >= sizeof(bank_mapping) / sizeof(u8)) {
			FTS_INFO("[FTS-UPGRADE]actual mipi bank(%d) not find in bank mapping, need jump\n", bank_num);
		} else {
			/*             FTS_INFO("[FTS-UPGRADE]bank number = %d\n", banknum); */
			if ((bank_crc_en >> banknum) & 0x01) {
				for (i = 0; i < MAX_BANK_DATA; i++) {
					temp_byte.dshort = (u16)bank_data[i];
					/*                     if(i == 0) */
					/*                         FTS_INFO("[FTS-UPGRADE]data0=%x, %d %d %d %d %d %d %d %d\n", temp_byte.dshort, temp_byte.bits.bit0, */
					/*                             temp_byte.bits.bit1, temp_byte.bits.bit2, temp_byte.bits.bit3, temp_byte.bits.bit4, */
					/*                             temp_byte.bits.bit5, temp_byte.bits.bit6, temp_byte.bits.bit7); */

					ecc.bits.bit0 = ecc_last.bits.bit8 ^ ecc_last.bits.bit9 ^ ecc_last.bits.bit10 ^ ecc_last.bits.bit11
									^ ecc_last.bits.bit12 ^ ecc_last.bits.bit13 ^ ecc_last.bits.bit14 ^ ecc_last.bits.bit15
									^ temp_byte.bits.bit0 ^ temp_byte.bits.bit1 ^ temp_byte.bits.bit2 ^ temp_byte.bits.bit3
									^ temp_byte.bits.bit4 ^ temp_byte.bits.bit5 ^ temp_byte.bits.bit6 ^ temp_byte.bits.bit7;

					ecc.bits.bit1 = ecc_last.bits.bit9 ^ ecc_last.bits.bit10 ^ ecc_last.bits.bit11 ^ ecc_last.bits.bit12
									^ ecc_last.bits.bit13 ^ ecc_last.bits.bit14 ^ ecc_last.bits.bit15
									^ temp_byte.bits.bit1 ^ temp_byte.bits.bit2 ^ temp_byte.bits.bit3 ^ temp_byte.bits.bit4
									^ temp_byte.bits.bit5 ^ temp_byte.bits.bit6 ^ temp_byte.bits.bit7;

					ecc.bits.bit2 = ecc_last.bits.bit8 ^ ecc_last.bits.bit9 ^ temp_byte.bits.bit0 ^ temp_byte.bits.bit1;

					ecc.bits.bit3 = ecc_last.bits.bit9 ^ ecc_last.bits.bit10 ^ temp_byte.bits.bit1 ^ temp_byte.bits.bit2;

					ecc.bits.bit4 = ecc_last.bits.bit10 ^ ecc_last.bits.bit11 ^ temp_byte.bits.bit2 ^ temp_byte.bits.bit3;

					ecc.bits.bit5 = ecc_last.bits.bit11 ^ ecc_last.bits.bit12 ^ temp_byte.bits.bit3 ^ temp_byte.bits.bit4;

					ecc.bits.bit6 = ecc_last.bits.bit12 ^ ecc_last.bits.bit13 ^ temp_byte.bits.bit4 ^ temp_byte.bits.bit5;

					ecc.bits.bit7 = ecc_last.bits.bit13 ^ ecc_last.bits.bit14 ^ temp_byte.bits.bit5 ^ temp_byte.bits.bit6;

					ecc.bits.bit8 = ecc_last.bits.bit0 ^ ecc_last.bits.bit14 ^ ecc_last.bits.bit15 ^ temp_byte.bits.bit6 ^ temp_byte.bits.bit7;

					ecc.bits.bit9 = ecc_last.bits.bit1 ^ ecc_last.bits.bit15 ^ temp_byte.bits.bit7;

					ecc.bits.bit10 = ecc_last.bits.bit2;

					ecc.bits.bit11 = ecc_last.bits.bit3;

					ecc.bits.bit12 = ecc_last.bits.bit4;

					ecc.bits.bit13 = ecc_last.bits.bit5;

					ecc.bits.bit14 = ecc_last.bits.bit6;

					ecc.bits.bit15 = ecc_last.bits.bit7 ^ ecc_last.bits.bit8 ^ ecc_last.bits.bit9 ^ ecc_last.bits.bit10
									 ^ ecc_last.bits.bit11 ^ ecc_last.bits.bit12 ^ ecc_last.bits.bit13 ^ ecc_last.bits.bit14 ^ ecc_last.bits.bit15
									 ^ temp_byte.bits.bit0 ^ temp_byte.bits.bit1 ^ temp_byte.bits.bit2 ^ temp_byte.bits.bit3
									 ^ temp_byte.bits.bit4 ^ temp_byte.bits.bit5 ^ temp_byte.bits.bit6 ^ temp_byte.bits.bit7;

					ecc_last.dshort = ecc.dshort;

				}
			}
		}
		pos += bank_len + 4;
	}

	*ecc_val = ecc.dshort;
	return 0;
}

/* calculate lcd init code checksum */
static u16 cal_lcdinitcode_checksum(u8 *ptr , int length)
{
	/* CRC16 */
	u16 cfcs = 0;
	int i, j;

	if (length % 2) {
		return 0xFFFF;
	}

	for (i = 0; i < length; i += 2) {
		cfcs ^= ((ptr[i] << 8) + ptr[i + 1]);
		for (j = 0; j < 16; j++) {
			if (cfcs & 1) {
				cfcs = (u16)((cfcs >> 1) ^ ((1 << 15) + (1 << 10) + (1 << 3)));
			} else {
				cfcs >>= 1;
			}
		}
	}
	return cfcs;
}

static int print_data(u8 *buf, u32 len)
{
	int i = 0;
	int n = 0;
	u8 *p = NULL;

	p = kmalloc(len * 4, GFP_KERNEL);
	memset(p, 0, len * 4);

	for (i = 0; i < len; i++) {
		n += snprintf(p + n, PAGE_SIZE, "%02x ", buf[i]);
	}

	FTS_DEBUG("[FTS-UPGRADE]%s\n", p);

	kfree(p);
	return 0;
}

static int read_3gamma(struct i2c_client *client, u8 **gamma, u16 *len)
{
	int ret = 0;
	int i = 0;
	int packet_num = 0;
	int packet_len = 0;
	int remainder = 0;
	u8 cmd[4] = { 0 };
	u32 addr = 0x01D000;
	u8 gamma_header[0x20] = { 0 };
	u16 gamma_len = 0;
	u16 gamma_len_n = 0;
	u16 pos = 0;
	bool gamma_has_enable = false;
	u8 *pgamma = NULL;
	int j = 0;
	u8 gamma_ecc = 0;

	cmd[0] = 0x03;
	cmd[1] = (u8)(addr >> 16);
	cmd[2] = (u8)(addr >> 8);
	cmd[3] = (u8)addr;
	ret = fts_i2c_write(client, cmd, 4);
	msleep(10);
	ret = fts_i2c_read(client, NULL, 0, gamma_header, 0x20);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]read 3-gamma header fail\n");
		return ret;
	}

	gamma_len = (u16)((u16)gamma_header[0] << 8) + gamma_header[1];
	gamma_len_n = (u16)((u16)gamma_header[2] << 8) + gamma_header[3];

	if ((gamma_len + gamma_len_n) != 0xFFFF) {
		FTS_INFO("[FTS-UPGRADE]gamma length check fail:%x %x\n", gamma_len, gamma_len);
		return -EIO;
	}

	if ((gamma_header[4] + gamma_header[5]) != 0xFF) {
		FTS_INFO("[FTS-UPGRADE]gamma ecc check fail:%x %x\n", gamma_header[4], gamma_header[5]);
		return -EIO;
	}

	if (gamma_len > MAX_GAMMA_LEN) {
		FTS_ERROR("[FTS-UPGRADE]gamma data len(%d) is too long\n", gamma_len);
		return -EINVAL;
	}

	*gamma = kmalloc(MAX_GAMMA_LEN, GFP_KERNEL);
	if (NULL == *gamma) {
		FTS_ERROR("[FTS-UPGRADE]malloc gamma memory fail\n");
		return -ENOMEM;
	}
	pgamma = *gamma;

	packet_num = gamma_len / 256;
	packet_len = 256;
	remainder = gamma_len % 256;
	if (remainder)
		packet_num++;
	FTS_INFO("[FTS-UPGRADE]3-gamma len:%d\n", gamma_len);
	cmd[0] = 0x03;
	addr += 0x20;
	for (i = 0; i < packet_num; i++) {
		addr += i * 256;
		cmd[1] = (u8)(addr >> 16);
		cmd[2] = (u8)(addr >> 8);
		cmd[3] = (u8)addr;
		if ((i == packet_num - 1) && remainder)
			packet_len = remainder;
		ret = fts_i2c_write(client, cmd, 4);
		msleep(10);
		ret = fts_i2c_read(client, NULL, 0, pgamma + i * 256, packet_len);
		if (ret < 0) {
			FTS_ERROR("[FTS-UPGRADE]read 3-gamma data fail\n");
			return ret;
		}
	}

	/*  ecc */
	for (j = 0; j < gamma_len; j++) {
		gamma_ecc ^= pgamma[j];
	}
	FTS_INFO("[FTS-UPGRADE]back_3gamma_ecc: 0x%x, 0x%x\n", gamma_ecc, gamma_header[0x04]);
	if (gamma_ecc != gamma_header[0x04]) {
		FTS_ERROR("[FTS-UPGRADE]back gamma ecc check fail:%x %x\n", gamma_ecc, gamma_header[0x04]);
		return -EIO;
	}

	/* check last byte is 91 80 00 19 01 */
	pos = gamma_len - 5;
	if ((gamma_enable[1] == pgamma[pos]) && (gamma_enable[2] == pgamma[pos + 1])
		&& (gamma_enable[3] == pgamma[pos + 2]) && (gamma_enable[4] == pgamma[pos + 3])) {
		gamma_has_enable = true;
	}

	if (false == gamma_has_enable) {
		FTS_INFO("[FTS-UPGRADE]3-gamma has no gamma enable info\n");
		pgamma[gamma_len++] = gamma_enable[1];
		pgamma[gamma_len++] = gamma_enable[2];
		pgamma[gamma_len++] = gamma_enable[3];
		pgamma[gamma_len++] = gamma_enable[4];
		pgamma[gamma_len++] = gamma_enable[5];
	}

	*len = gamma_len;

	FTS_DEBUG("[FTS-UPGRADE]read 3-gamma data:\n");
	print_data(*gamma, gamma_len);

	return 0;
}

static int replace_3gamma(u8 *initcode, u8 *gamma, u16 gamma_len)
{
	u16 gamma_pos = 0;
	const int gamma_analog[] = { 0x003A, 0x85, 0x00, 0x00, 0x2C, 0x2B };
	const int gamma_digital1[] = { 0x0355, 0x8D, 0x00, 0x00, 0x80, 0x80 };
	const int gamma_digital2[] = { 0x03d9, 0x8D, 0x80, 0x00, 0x14, 0x13 };

	/* Analog Gamma */
	if ((initcode[gamma_analog[0]] == gamma[gamma_pos])
		&& (initcode[gamma_analog[0] + 1] == gamma[gamma_pos + 1])) {
		memcpy(initcode + gamma_analog[0] + 4 , gamma + gamma_pos + 4, gamma_analog[5]);
		gamma_pos += gamma_analog[5] + 4;
	} else
		goto find_gamma_bank_err;

	/* Digital1 Gamma */
	if ((initcode[gamma_digital1[0]] == gamma[gamma_pos])
		&& (initcode[gamma_digital1[0] + 1] == gamma[gamma_pos + 1])) {
		memcpy(initcode + gamma_digital1[0] + 4 , gamma + gamma_pos + 4, gamma_digital1[5]);
		gamma_pos += gamma_digital1[5] + 4;
	} else
		goto find_gamma_bank_err;

	/* Digital2 Gamma */
	if ((initcode[gamma_digital2[0]] == gamma[gamma_pos])
		&& (initcode[gamma_digital2[0] + 1] == gamma[gamma_pos + 1])) {
		memcpy(initcode + gamma_digital2[0] + 4 , gamma + gamma_pos + 4, gamma_digital2[5]);
		gamma_pos += gamma_digital2[5] + 4;
	} else
		goto find_gamma_bank_err;

	/* enable Gamma */
	if ((initcode[gamma_enable[0]] == gamma[gamma_pos])
		&& (initcode[gamma_enable[0] + 1] == gamma[gamma_pos + 1])) {
		if (gamma[gamma_pos + 4])
			initcode[gamma_enable[0] + 4 + 15] |= 0x01;
		else
			initcode[gamma_enable[0] + 4 + 15] &= 0xFE;
		gamma_pos += 1 + 4;
	} else
		goto find_gamma_bank_err;

	FTS_DEBUG("[FTS-UPGRADE]replace 3-gamma data:\n");
	print_data(initcode, 1100);

	return 0;

find_gamma_bank_err:
	FTS_INFO("[FTS-UPGRADE]3-gamma bank(%02x %02x) not find\n",
			 gamma[gamma_pos], gamma[gamma_pos + 1]);
	return -ENODATA;
}

/*
 * read_replace_3gamma - read and replace 3-gamma data
 */
static int read_replace_3gamma(struct i2c_client *client, u8 *buf, bool flag)
{
	int ret = 0;
	u16 initcode_ecc = 0;
	u16 initcode_checksum = 0;
	u8 *tmpbuf = NULL;
	u8 *gamma = NULL;
	u16 gamma_len = 0;
	u16 hlic_len = 0;
	int base_addr = 0;
	int i = 0;

	FTS_FUNC_ENTER();

	ret = read_3gamma(client, &gamma, &gamma_len);
	if (ret < 0) {
		FTS_INFO("[FTS-UPGRADE]no vaid 3-gamma data, not replace\n");
		if (gamma) {
			kfree(gamma);
			gamma = NULL;
		}
		return 0;
	}

	base_addr = 0;
	for (i = 0; i < 2; i++) {
		if (1 == i) {
			if (true == flag)
				base_addr = 0x7C0;
			else
				break;
		}

		tmpbuf = buf + base_addr;
		ret = replace_3gamma(tmpbuf, gamma, gamma_len);
		if (ret < 0) {
			FTS_ERROR("[FTS-UPGRADE]replace 3-gamma fail\n");
			goto REPLACE_GAMMA_ERR;
		}

		ret = cal_lcdinitcode_ecc(tmpbuf, &initcode_ecc);
		if (ret < 0) {
			FTS_ERROR("[FTS-UPGRADE]lcd init code ecc calculate fail\n");
			goto REPLACE_GAMMA_ERR;
		}
		FTS_INFO("[FTS-UPGRADE]lcd init code cal ecc:%04x\n", initcode_ecc);
		tmpbuf[LIC_LCD_ECC_H_OFF] = (u8)(initcode_ecc >> 8);
		tmpbuf[LIC_LCD_ECC_L_OFF] = (u8)(initcode_ecc);
		tmpbuf[LIC_ECC_REG_H_OFF] = (u8)(initcode_ecc >> 8);
		tmpbuf[LIC_ECC_REG_L_OFF] = (u8)(initcode_ecc);

		hlic_len = (u16)(((u16)tmpbuf[2]) << 8) + tmpbuf[3];
		initcode_checksum = cal_lcdinitcode_checksum(tmpbuf + 2, hlic_len - 2);
		FTS_INFO("[FTS-UPGRADE]lcd init code calc checksum:%04x\n", initcode_checksum);
		tmpbuf[LIC_CHECKSUM_H_OFF] = (u8)(initcode_checksum >> 8);
		tmpbuf[LIC_CHECKSUM_L_OFF] = (u8)(initcode_checksum);
	}

	if (gamma) {
		kfree(gamma);
		gamma = NULL;
	}

	FTS_FUNC_EXIT();
	return 0;

REPLACE_GAMMA_ERR:
	if (gamma) {
		kfree(gamma);
		gamma = NULL;
	}
	return ret;
}

/*
 * check_initial_code_valid - check initial code valid or not
 */
static int check_initial_code_valid(struct i2c_client *client, u8 *buf)
{
	int ret = 0;
	u16 initcode_ecc = 0;
	u16 buf_ecc = 0;
	u16 initcode_checksum = 0;
	u16 buf_checksum = 0;
	u16 hlic_len = 0;

	hlic_len = (u16)(((u16)buf[2]) << 8) + buf[3];
	initcode_checksum = cal_lcdinitcode_checksum(buf + 2, hlic_len - 2);
	buf_checksum = ((u16)((u16)buf[0] << 8) + buf[1]);
	FTS_INFO("[FTS-UPGRADE]lcd init code calc checksum:%04x, %04x\n", initcode_checksum, buf_checksum);
	if (initcode_checksum != buf_checksum) {
		FTS_ERROR("[FTS-UPGRADE]Initial Code checksum fail\n");
		return -EINVAL;
	}

	ret = cal_lcdinitcode_ecc(buf, &initcode_ecc);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]lcd init code ecc calculate fail\n");
		return ret;
	}
	buf_ecc = ((u16)((u16)buf[4] << 8) + buf[5]);
	FTS_INFO("[FTS-UPGRADE]lcd init code cal ecc:%04x, %04x\n", initcode_ecc, buf_ecc);
	if (initcode_ecc != buf_ecc) {
		FTS_ERROR("[FTS-UPGRADE]Initial Code ecc check fail\n");
		return -EINVAL;
	}

	return 0;
}

static bool fts_ft8006_check_ide(u8 *buf, u32 len)
{
	u32 off = 0;

	FTS_INFO("[FTS-UPGRADE]ft8006 Host FW file IDE version check\n");
	if (NULL == buf) {
		FTS_ERROR("[FTS-UPGRADE]buf is null fail\n");
		return false;
	}

	if (len < FTS_MAX_LEN_FILE) {
		FTS_INFO("[FTS-UPGRADE]buf len(%x) abnormal, no IDE\n", len);
		return false;
	}

	off = upgrade_func_ft8006.paramcfgoff;
	if ((buf[off] == 'I') && (buf[off + 1] == 'D') && (buf[off + 2] == 'E'))
		return true;

	return false;
}

/* fts_ft8006_write_ecc - write and check ecc
 * return 0 if success
 */
static int fts_ft8006_write_ecc(
	struct i2c_client *client,
	u32 saddr,
	u8 *buf,
	u32 len)
{
	int ecc_in_host = 0;
	int ecc_in_tp = 0;

	ecc_in_host = fts_flash_write_buf(client, saddr, buf, len, 1);
	if (ecc_in_host < 0) {
		FTS_ERROR("[FTS-UPGRADE]write buffer to flash fail\n");
		return ecc_in_host;
	}

	/* ecc */
	ecc_in_tp = fts_fwupg_ecc_cal(client, saddr, len);
	if (ecc_in_tp < 0) {
		FTS_ERROR("[FTS-UPGRADE]ecc read fail\n");
		return ecc_in_tp;
	}

	FTS_INFO("[FTS-UPGRADE]ecc in tp:%x, host:%x\n", ecc_in_tp, ecc_in_host);
	if (ecc_in_tp != ecc_in_host) {
		FTS_ERROR("[FTS-UPGRADE]ecc check fail\n");
		return -EIO;
	}

	return 0;
}

/************************************************************************
 * Name: fts_ft8006_param_flash
 * Brief: param upgrade(erase/write/ecc check)
 * Input: buf - all.bin
 *        len - len of all.bin
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8006_param_flash(struct i2c_client *client, u8 *buf, u32 len)
{
	int ret = 0;
	u8 cmd[2] = { 0 };
	u32 delay = 0;
	u32 start_addr = 0;
	u32 paramcfg_len = 0;
	u8 *tmpbuf = NULL;

	/* erase gesture & parameter sector */
	cmd[0] = FTS_CMD_FLASH_MODE;
	cmd[1] = FLASH_MODE_PARAM_VALUE;
	ret = fts_i2c_write(client, cmd, 2);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]upgrade mode(09) cmd write fail\n");
		goto PARAM_FLASH_ERR;
	}

	delay = FTS_ERASE_SECTOR_DELAY * 2;
	ret = fts_fwupg_erase(client, delay);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]erase cmd write fail\n");
		goto PARAM_FLASH_ERR;
	}

	/* write flash */
	start_addr = upgrade_func_ft8006.paramcfgoff;
	paramcfg_len = FTS_MAX_LEN_SECTOR;
	tmpbuf = buf + start_addr;
	ret = fts_ft8006_write_ecc(client, start_addr, tmpbuf, paramcfg_len);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]parameter configure area write fail\n");
		goto PARAM_FLASH_ERR;
	}

	start_addr = upgrade_func_ft8006.paramcfg2off;
	paramcfg_len = FTS_MAX_LEN_SECTOR;
	tmpbuf = buf + start_addr;
	ret = fts_ft8006_write_ecc(client, start_addr, tmpbuf, paramcfg_len);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]parameter2 configure area write fail\n");
		goto PARAM_FLASH_ERR;
	}

	return 0;

PARAM_FLASH_ERR:
	return ret;
}

/*
 * fts_get_hlic_ver - read host lcd init code version
 *
 * return 0 if host lcd init code is valid, otherwise return error code
 */
static int fts_ft8006_get_hlic_ver(u8 *initcode)
{
	u8 *hlic_buf = initcode;
	u16 hlic_len = 0;
	u8 hlic_ver[2] = { 0 };

	hlic_len = (u16)(((u16)hlic_buf[2]) << 8) + hlic_buf[3];
	FTS_INFO("[FTS-UPGRADE]host lcd init code len:%x\n", hlic_len);
	if (hlic_len >= FTS_MAX_LEN_SECTOR) {
		FTS_ERROR("[FTS-UPGRADE]host lcd init code len(%x) is too large\n", hlic_len);
		return -EINVAL;
	}

	hlic_ver[0] = hlic_buf[hlic_len];
	hlic_ver[1] = hlic_buf[hlic_len + 1];

	FTS_INFO("[FTS-UPGRADE]host lcd init code ver:%x %x\n", hlic_ver[0], hlic_ver[1]);
	if (0xFF != (hlic_ver[0] + hlic_ver[1])) {
		FTS_ERROR("[FTS-UPGRADE]host lcd init code version check fail\n");
		return -EINVAL;
	}

	return hlic_ver[0];
}

/************************************************************************
 * Name: fts_ft8006_upgrade
 * Brief:
 * Input: buf - all.bin
 *        len - len of all.bin
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8006_upgrade(struct i2c_client *client, u8 *buf, u32 len)
{
	int ret = 0;
	u8 *tmpbuf = NULL;
	u8 cmd[2] = { 0 };
	u32 delay = 0;
	u32 start_addr = 0;
	u32 app_1_len = 0;
	u32 app_2_len = 0;
	u32 app_len = 0;
	u32 off = 0;

	FTS_INFO("[FTS-UPGRADE]app upgrade...\n");
	if (NULL == buf) {
		FTS_ERROR("[FTS-UPGRADE]fw file buffer is null\n");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
		FTS_ERROR("[FTS-UPGRADE]fw file buffer len(%x) fail\n", len);
		return -EINVAL;
	}

	off = upgrade_func_ft8006.appoff + FTS_APPINFO_OFF + FTS_APPINFO_APPLEN_OFF;
	app_1_len = (((u32)buf[off] << 8) + buf[off + 1]);
	off = upgrade_func_ft8006.appoff + FTS_APPINFO_OFF + FTS_APPINFO_APPLEN2_OFF;
	app_2_len = (((u32)buf[off] << 8) + buf[off + 1]);
	app_len = (app_2_len << 16) + app_1_len;
	if ((app_len < FTS_MIN_LEN) || (app_len > FTS_MAX_LEN_APP_FT8006)) {
		FTS_ERROR("[FTS-UPGRADE]app len(%x) fail\n", app_len);
		return -EINVAL;
	}

	/* enter into upgrade environment */
	ret = fts_fwupg_enter_into_boot(client);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]enter into pramboot/bootloader fail,ret=%d\n", ret);
		goto APP_UPG_ERR;
	}

	/* erase gesture & parameter sector */
	cmd[0] = FTS_CMD_FLASH_MODE;
	cmd[1] = FLASH_MODE_UPGRADE_VALUE;
	ret = fts_i2c_write(client, cmd, 2);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]upgrade mode(09) cmd write fail\n");
		goto APP_UPG_ERR;
	}

	delay = FTS_ERASE_SECTOR_DELAY * (app_len / FTS_MAX_LEN_SECTOR);
	ret = fts_fwupg_erase(client, delay);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]erase cmd write fail\n");
		goto APP_UPG_ERR;
	}

	/* write flash */
	start_addr = upgrade_func_ft8006.appoff;
	tmpbuf = buf + start_addr;
	ret = fts_ft8006_write_ecc(client, start_addr, tmpbuf, app_len);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]app buffer write fail\n");
		goto APP_UPG_ERR;
	}

	if (fts_ft8006_check_ide(buf, len)) {
		FTS_INFO("[FTS-UPGRADE]erase and write param configure area\n");
		ret = fts_ft8006_param_flash(client, buf, len);
		if (ret < 0) {
			FTS_ERROR("[FTS-UPGRADE]param upgrade(erase/write/ecc) fail\n");
			goto APP_UPG_ERR;
		}
	}

	FTS_INFO("[FTS-UPGRADE]upgrade success, reset to normal boot\n");
	ret = fts_fwupg_reset_in_boot(client);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]reset to normal boot fail\n");
	}
	msleep(400);
	return 0;

APP_UPG_ERR:
	return ret;
}

/************************************************************************
 * Name: fts_ft8006_lic_upgrade
 * Brief:
 * Input: buf - all.bin
 *        len - len of all.bin
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8006_lic_upgrade(struct i2c_client *client, u8 *buf, u32 len)
{
	int ret = 0;
	u8 *tmpbuf = NULL;
	u8 cmd[2] = { 0 };
	u32 delay = 0;
	u32 start_addr = 0;
	u32 lic_len = 0;
	u8 val = 0;
	bool flag = false;

	FTS_INFO("[FTS-UPGRADE]LCD initial code upgrade...\n");
	if (NULL == buf) {
		FTS_ERROR("[FTS-UPGRADE]fw file buffer is null\n");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
		FTS_ERROR("[FTS-UPGRADE]fw file buffer len(%x) fail\n", len);
		return -EINVAL;
	}

	ret = check_initial_code_valid(client, buf);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]initial code invalid, not upgrade lcd init code\n");
		return -EINVAL;
	}

	ret = fts_i2c_read_reg(client, LIC_REG_2, &val);
	FTS_DEBUG("[FTS-UPGRADE]lic flag:%x\n", val);
	if ((ret > 0) && (1 == val))
		flag = true;

	lic_len = FTS_MAX_LEN_SECTOR;
	/* remalloc memory for initcode, need change content of initcode afterwise */
	tmpbuf = kmalloc(lic_len, GFP_KERNEL);
	if (NULL == tmpbuf) {
		FTS_INFO("[FTS-UPGRADE]initial code buf malloc fail\n");
		return -EINVAL;
	}
	start_addr = upgrade_func_ft8006.licoff;
	memcpy(tmpbuf, buf + start_addr, lic_len);

	/* enter into upgrade environment */
	ret = fts_fwupg_enter_into_boot(client);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]enter into pramboot/bootloader fail,ret=%d\n", ret);
		goto LIC_UPG_ERR;
	}

	/* 3-gamma remap */
	ret = read_replace_3gamma(client, tmpbuf, flag);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]replace 3-gamma fail, not upgrade lcd init code\n");
		goto LIC_UPG_ERR;
	}

	/* erase gesture & parameter sector */
	cmd[0] = FTS_CMD_FLASH_MODE;
	cmd[1] = FLASH_MODE_LIC_VALUE;
	ret = fts_i2c_write(client, cmd, 2);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]upgrade mode(09) cmd write fail\n");
		goto LIC_UPG_ERR;
	}

	delay = FTS_ERASE_SECTOR_DELAY * 1;
	ret = fts_fwupg_erase(client, delay);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]erase cmd write fail\n");
		goto LIC_UPG_ERR;
	}

	ret = fts_ft8006_write_ecc(client, start_addr, tmpbuf, lic_len);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]LCD initial code write fail\n");
		goto LIC_UPG_ERR;
	}

	FTS_INFO("[FTS-UPGRADE]upgrade success, reset to normal boot\n");
	ret = fts_fwupg_reset_in_boot(client);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]reset to normal boot fail\n");
	}

	if (tmpbuf) {
		kfree(tmpbuf);
		tmpbuf = NULL;
	}

	msleep(400);
	return 0;

LIC_UPG_ERR:
	if (tmpbuf) {
		kfree(tmpbuf);
		tmpbuf = NULL;
	}
	return ret;
}

/************************************************************************
 * Name: fts_ft8006_param_upgrade
 * Brief:
 * Input: buf - all.bin
 *        len - len of all.bin
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8006_param_upgrade(struct i2c_client *client, u8 *buf, u32 len)
{
	int ret = 0;

	FTS_INFO("[FTS-UPGRADE]parameter configure upgrade...\n");
	if (NULL == buf) {
		FTS_ERROR("[FTS-UPGRADE]fw file buffer is null\n");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
		FTS_ERROR("[FTS-UPGRADE]fw file buffer len(%x) fail\n", len);
		return -EINVAL;
	}

	/* enter into upgrade environment */
	ret = fts_fwupg_enter_into_boot(client);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]enter into pramboot/bootloader fail,ret=%d\n", ret);
		goto PARAM_UPG_ERR;
	}

	ret = fts_ft8006_param_flash(client, buf, len);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]param upgrade(erase/write/ecc) fail\n");
		goto PARAM_UPG_ERR;
	}

	FTS_INFO("[FTS-UPGRADE]upgrade success, reset to normal boot\n");
	ret = fts_fwupg_reset_in_boot(client);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]reset to normal boot fail\n");
	}

	msleep(400);
	return 0;

PARAM_UPG_ERR:
	return ret;
}

/************************************************************************
 * Name: fts_ft8006_force_upgrade
 * Brief:
 * Input: buf - all.bin
 *        len - constant:128 * 1024
 * Output:
 * Return: return 0 if success, otherwise return error code
 ***********************************************************************/
static int fts_ft8006_force_upgrade(struct i2c_client *client, u8 *buf, u32 len)
{
	int ret = 0;
	u8 *tmpbuf = NULL;
	u8 cmd[2] = { 0 };
	u32 delay = 0;
	u32 start_addr = 0;
	u32 tmplen = 0;

	FTS_INFO("[FTS-UPGRADE]fw force upgrade...\n");
	if (NULL == buf) {
		FTS_ERROR("[FTS-UPGRADE]fw file buffer is null\n");
		return -EINVAL;
	}

	if ((len < FTS_MIN_LEN) || (len > FTS_MAX_LEN_FILE)) {
		FTS_ERROR("[FTS-UPGRADE]fw file buffer len(%x) fail\n", len);
		return -EINVAL;
	}

	/* enter into upgrade environment */
	ret = fts_fwupg_enter_into_boot(client);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]enter into pramboot/bootloader fail,ret=%d\n", ret);
		goto FORCE_UPG_ERR;
	}

	/* erase 0k~116k flash */
	cmd[0] = FTS_CMD_FLASH_MODE;
	cmd[1] = FLASH_MODE_WRITE_FLASH_VALUE;
	ret = fts_i2c_write(client, cmd, 2);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]upgrade mode(09) cmd write fail\n");
		goto FORCE_UPG_ERR;
	}

	if (len > (116 * 1024)) {
		tmplen = 116 * 1024;
	} else {
		tmplen = len;
	}
	delay = FTS_ERASE_SECTOR_DELAY * (tmplen / FTS_MAX_LEN_SECTOR);
	ret = fts_fwupg_erase(client, delay);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]erase cmd write fail\n");
		goto FORCE_UPG_ERR;
	}

	/* write flash */
	start_addr = 0;
	tmpbuf = buf + start_addr;
	ret = fts_ft8006_write_ecc(client, start_addr, tmpbuf, tmplen);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]app buffer write fail\n");
		goto FORCE_UPG_ERR;
	}

	if (fts_ft8006_check_ide(buf, len)) {
		FTS_INFO("[FTS-UPGRADE]erase and write param configure area\n");
		ret = fts_ft8006_param_flash(client, buf, len);
		if (ret < 0) {
			FTS_ERROR("[FTS-UPGRADE]param upgrade(erase/write/ecc) fail\n");
			goto FORCE_UPG_ERR;
		}
	}

	FTS_INFO("[FTS-UPGRADE]upgrade success, reset to normal boot\n");
FORCE_UPG_ERR:
	ret = fts_fwupg_reset_in_boot(client);
	if (ret < 0) {
		FTS_ERROR("[FTS-UPGRADE]reset to normal boot fail\n");
	}

	msleep(400);
	return ret;
}

/************************************************************************
 * fts_ft8006m_pram_write_remap - write pramboot to pram and start pramboot
 *
 * return 0 if success, otherwise return error code
***********************************************************************/
int fts_ft8006m_pram_write_remap(struct i2c_client *client)
{
	int ret = 0;
	int ecc_in_host = 0;
	int ecc_in_tp = 0;
	u8 *pb_buf = NULL;
	u32 pb_len = 0;
	int i = 0;
	int j = 0;
	u8 temp = 0;
	const struct firmware *fw = NULL;

	FTS_INFO("[UPGRADE]write pram and remap\n");

	ret = request_firmware(&fw, FT8006M_PRAMBOOT_FW_NAME, &client->dev);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE] write pram failed\n");
		return ret;
	}

	if (fw->size < FTS_MIN_LEN) {
		FTS_ERROR("[UPGRADE]pramboot length(%x) fail\n", fw->size);
		return -EINVAL;
	}

	pb_buf = (u8 *)fw->data;
	pb_len = (u32)fw->size;

	for (i = 0; i < pb_len; i = i + 4) {
		for (j = 0; j < 2; j++) {
			temp = pb_buf[i + j];
			pb_buf[i + j] = pb_buf[i + 3 - j];
			pb_buf[i + 3 - j] = temp;
		}
	}

	/* write pramboot to pram */
	ecc_in_host = fts_pram_write_buf(client, pb_buf, pb_len);
	if (ecc_in_host < 0) {
		FTS_ERROR("write pramboot fail");
		return ecc_in_host;
	}
	release_firmware(fw);

	/* read out checksum */
	ecc_in_tp = fts_pram_ecc_cal(client, 0, pb_len);
	if (ecc_in_tp < 0) {
		FTS_ERROR("read pramboot ecc fail");
		return ecc_in_tp;
	}

	FTS_INFO("[UPGRADE]pram ecc in tp:%x, host:%x\n", ecc_in_tp, ecc_in_host);
	/*  pramboot checksum != fw checksum, upgrade fail */
	if (ecc_in_host != ecc_in_tp) {
		FTS_ERROR("[UPGRADE]pramboot ecc check fail\n");
		return -EIO;
	}

	/*start pram*/
	ret = fts_pram_start(client);
	if (ret < 0) {
		FTS_ERROR("[UPGRADE]pram start fail\n");
		return ret;
	}

	return 0;
}

struct upgrade_func upgrade_func_ft8006 = {
	.ctype = {0x07},
	.fwveroff = 0x510E,
	.fwcfgoff = 0x0F80,
	.appoff = 0x5000,
	.licoff = 0x0000,
	.paramcfgoff = 0x1F000,
	.paramcfgveroff = 0x1F004,
	.paramcfg2off = 0x4000,
	.pramboot_supported = true,
	.hid_supported = false,
	.upgrade = fts_ft8006_upgrade,
	.get_hlic_ver = fts_ft8006_get_hlic_ver,
	.lic_upgrade = fts_ft8006_lic_upgrade,
	.param_upgrade = fts_ft8006_param_upgrade,
	.force_upgrade = fts_ft8006_force_upgrade,
};

