/*
 * Copyright (C) 2010 Melfas, Inc.
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
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpiomux.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include "mms100a_ts.h"

#define PAGE_HEADER		3
#define PAGE_DATA		1024
#define PAGE_CRC		2
#define PACKET_SIZE		(PAGE_HEADER + PAGE_DATA + PAGE_CRC)

#define MMS_CONFIRM_STATUS	0xAF
#define MMS_ENTER_ISC		0x5F
#define MMS_ENABLE_WRITE	0x55
#define MMS_STATUS_ISC_READY	0x01
#define MMS_WRITE_CMD		0xAE
#define MMS_GET_CUSTOM_ADDRESS	0xE5
#define MMS_DATA_WRITE		0xF1
#define MMS_STATUS_WRITING_DONE	0x03

#define mms_ts_i2c_write_block(client, buf, len) i2c_master_send(client, buf, len)
#define is_section_valid(addr) ((addr) > 0 && (addr) <= 30)

#define __calc_crc(seed, crc) \
	do { \
		int _bit_index; \
		u16 send, in, xor_bit_1, xor_bit_2, xor_bit_3; \
		for (_bit_index = 7; _bit_index >= 0; _bit_index--) { \
			in = (seed >> _bit_index) & 0x01; \
			xor_bit_1 = (crc & 0x0001) ^ in; \
			xor_bit_2 = xor_bit_1 ^ (crc >> 11 & 0x01); \
			xor_bit_3 = xor_bit_1 ^ (crc >> 4 & 0x01); \
			send = (xor_bit_1 << 4) | (crc >> 12 & 0x0F); \
			send = (send << 7) | (xor_bit_2 << 6) | (crc >> 5 & 0x3F); \
			send = (send << 4) | (xor_bit_3 << 3) | (crc >> 1 & 0x0007); \
			crc = send; \
		} \
	} while (0)

struct mms_info_isc {
	struct i2c_client		*client;
	struct melfas_tsi_platform_data	*pdata;
	struct mms_fw_info *fw_info;
	char buf[PACKET_SIZE];
};

static const char crc0[31] = {
	0x1D, 0x2C, 0x05, 0x34, 0x95, 0xA4, 0x8D, 0xBC, 0x59, 0x68, 0x41, 0x70,
	0xD1, 0xE0, 0xC9, 0xF8, 0x3F, 0x0E, 0x27, 0x16, 0xB7, 0x86, 0xAF, 0x9E,
	0x7B, 0x4A, 0x63, 0x52, 0xF3, 0xC2, 0xEB
};

static const char crc1[31] = {
	0x1E, 0x9C, 0xDF, 0x5D, 0x76, 0xF4, 0xB7, 0x35, 0x2A, 0xA8, 0xEB, 0x69,
	0x42, 0xC0, 0x83, 0x01, 0x04, 0x86, 0xC5, 0x47, 0x6C, 0xEE, 0xAD, 0x2F,
	0x30, 0xB2, 0xF1, 0x73, 0x58, 0xDA, 0x99
};

static int mms_ts_i2c_read(struct i2c_client *client, u8 reg, char *buf, int len)
{
	int ret = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, 2);

	if (ret < 0) {
		if (printk_ratelimit())
			TOUCH_INFO_MSG("transfer error: %d\n", ret);
		return -EIO;
	} else {
		return 0;
	}
}

static int mms_ts_isc_enter(struct mms_info_isc *ts)
{
	char tmp[2] = { MMS_ENTER_ISC, 0x01 };

	if (mms_ts_i2c_write_block(ts->client, tmp, 2) < 0)
		return -EIO;

	TOUCH_INFO_MSG("isc entered...\n");

	return 0;
}

static int mms_ts_isc_write_enable(struct mms_info_isc *ts)
{
	char tmp[10] = { MMS_WRITE_CMD, MMS_ENABLE_WRITE, };

	if (mms_ts_i2c_write_block(ts->client, tmp, 10) < 0)
		return -EIO;

	if (mms_ts_i2c_read(ts->client, MMS_CONFIRM_STATUS, tmp, 1) < 0)
		return -EIO;

	if (tmp[0] != MMS_STATUS_ISC_READY) {
		TOUCH_INFO_MSG("STATUS_WRITING_DONE check failed\n");
		return -EIO;
	}

	TOUCH_INFO_MSG("isc write enabled...\n");

	return 0;
}

static int mms_ts_isc_clear_page(struct mms_info_isc *ts, u8 page)
{
	char tmp[1] = { 0 };

	ts->buf[0] = MMS_WRITE_CMD;
	ts->buf[1] = MMS_DATA_WRITE;
	ts->buf[2] = page;
	memset(&ts->buf[PAGE_HEADER], 0xFF, PAGE_DATA);
	ts->buf[PAGE_HEADER + PAGE_DATA] = crc0[page];
	ts->buf[PAGE_HEADER + PAGE_DATA+1] = crc1[page];

	if (mms_ts_i2c_write_block(ts->client, ts->buf, PACKET_SIZE) < 0)
		return -EIO;

	if (mms_ts_i2c_read(ts->client, MMS_CONFIRM_STATUS, tmp, 1) < 0)
		return -EIO;

	if (tmp[0] != MMS_STATUS_WRITING_DONE) {
		TOUCH_INFO_MSG("STATUS_WRTING_DONE check failed\n");
		return -EIO;
	}

	return 0;
}

static int mms_ts_isc_clear_validate_makers(struct mms_info_isc *ts)
{
	int i, j;
	bool matched;

	TOUCH_INFO_MSG("fw need_update = [%d %d %d]\n",
					ts->fw_info->need_update[0],
					ts->fw_info->need_update[1],
					ts->fw_info->need_update[2]);

	for (i = 1; i < MAX_SECTION_NUM; i++) {
		if (!ts->fw_info->need_update[i])
			continue;

		if (is_section_valid(ts->fw_info->ts_section[i].end_addr)) {
			if (mms_ts_isc_clear_page(ts,
				ts->fw_info->ts_section[i].end_addr) < 0) {
				TOUCH_INFO_MSG("clear page[%d] failed\n",
					ts->fw_info->ts_section[i].end_addr);
				return -EIO;
			}

			TOUCH_INFO_MSG("ts:page[%d] cleared...\n",
				ts->fw_info->ts_section[i].end_addr);
		}
	}

	for (i = 1; i < MAX_SECTION_NUM; i++) {
		if (!ts->fw_info->need_update[i])
			continue;

		matched = false;

		for (j = 1; j < MAX_SECTION_NUM; j++) {
			if (ts->fw_info->fw_img[i]->end_page ==
				ts->fw_info->ts_section[j].end_addr) {
				matched = true;
				break;
			}
		}

		if (matched == false &&
			is_section_valid(ts->fw_info->fw_img[i]->end_page)) {
			if (mms_ts_isc_clear_page(ts,
					ts->fw_info->fw_img[i]->end_page) < 0) {
				TOUCH_INFO_MSG("clear page[%d] failed\n",
					ts->fw_info->fw_img[i]->end_page);
				return -EIO;
			}

			TOUCH_INFO_MSG("bin:page[%d] cleared...\n",
				ts->fw_info->fw_img[i]->end_page);
		}
	}

	return 0;
}

static u16 mms_ts_isc_cacl_crc(char *crc, int seed, char *buf)
{
	u16 _crc = 0xffff;
	int i;

	__calc_crc(seed, _crc);

	for (i = 0; i < 1024; i++)
		__calc_crc(buf[i], _crc);

	crc[0] = (char)((_crc >> 8) & 0xff);
	crc[1] = (char)(_crc & 0xff);

	return _crc;
}

static int mms_ts_isc_update_section_data(struct mms_info_isc *ts)
{
	char crc[2];
	char tmp[1];
	char *ptr_fw;
	struct mms_bin_hdr *fw_hdr;
	struct mms_fw_img *fw_img;
	int i, j, page;

	fw_hdr = ts->fw_info->fw_hdr;

	TOUCH_INFO_MSG("fw need_update = [%d %d %d]\n",
					ts->fw_info->need_update[0],
					ts->fw_info->need_update[1],
					ts->fw_info->need_update[2]);

	for (i = 0; i < MAX_SECTION_NUM; i++) {
		if (!ts->fw_info->need_update[i])
			continue;

		fw_img = ts->fw_info->fw_img[i];
		ptr_fw = (char *) fw_hdr + fw_hdr->binary_offset
						+ fw_img->offset;

		for (page = fw_img->start_page;
					page <= fw_img->end_page; page++) {
			ts->buf[0] = MMS_WRITE_CMD;
			ts->buf[1] = MMS_DATA_WRITE;
			ts->buf[2] = (char) page;

			for (j = 0; j < PAGE_DATA; j += 4) {
				ts->buf[PAGE_HEADER+j+0] = ptr_fw[j+3];
				ts->buf[PAGE_HEADER+j+1] = ptr_fw[j+2];
				ts->buf[PAGE_HEADER+j+2] = ptr_fw[j+1];
				ts->buf[PAGE_HEADER+j+3] = ptr_fw[j+0];
			}

			mms_ts_isc_cacl_crc(crc, page, &ts->buf[PAGE_HEADER]);

			ts->buf[PAGE_HEADER+PAGE_DATA+0] = crc[0];
			ts->buf[PAGE_HEADER+PAGE_DATA+1] = crc[1];

			if (mms_ts_i2c_write_block(ts->client,
						ts->buf, PACKET_SIZE) < 0) {
				TOUCH_INFO_MSG("page[%d] write failed\n", page);
				return -EIO;
			}

			if (mms_ts_i2c_read(ts->client,
				MMS_CONFIRM_STATUS, tmp, 1) < 0) {
				TOUCH_INFO_MSG("page[%d] status read failed\n", page);
				return -EIO;
			}

			if (tmp[0] != MMS_STATUS_WRITING_DONE)
				TOUCH_INFO_MSG("page[%d] check failed\n", page);

			TOUCH_INFO_MSG("page[%d] is written.. crc=0x%02x0x%02x\n", page, crc[0], crc[1]);
			ptr_fw += PAGE_DATA;
		}

		ts->fw_info->need_update[i] = false;
	}

	return 0;
}

int mms_flash_fw_file_isc(struct i2c_client *client, struct melfas_tsi_platform_data *pdata, struct mms_fw_info *data)
{
	struct mms_info_isc *info = kzalloc(sizeof(struct mms_info_isc), GFP_KERNEL);

	info->client = client;
	info->pdata = pdata;
	info->fw_info = data;

	if (mms_ts_isc_enter(info) < 0)
		goto err_isc;

	if (mms_ts_isc_write_enable(info) < 0)
		goto err_isc;

	if (mms_ts_isc_clear_validate_makers(info) < 0)
		goto err_isc;

	if (mms_ts_isc_update_section_data(info) < 0)
		goto err_isc;

	return 0;

err_isc:
	kfree(info);
	return -EIO;
}
EXPORT_SYMBOL(mms_flash_fw_file_isc);

