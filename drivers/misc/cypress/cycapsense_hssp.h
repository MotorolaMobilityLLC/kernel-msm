
/*
 * Header file for:
 * Cypress CapSense(TM) firmware downloader.
 *
 * Copyright (c) 2015 Motorola Mobility LLC
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
#ifndef __CYCAPSENSE_HSSP_H__
#define __CYCAPSENSE_HSSP_H__

#include <linux/string.h>
#include <linux/limits.h>
#include <linux/workqueue.h>

struct hex_info {
	char fw_name[NAME_MAX];
	u8 *data;
	u8 *s_data;
	u8 *m_data;
	u8 cl_protection;
	u16 cs;
};
struct hssp_data {
	struct hex_info inf;
	const char *name;
	unsigned int silicon_id;
	int block_size;
	int num_of_blocks;
	int secure_bytes;
	int c_gpio;
	int d_gpio;
	int rst_gpio;
	int raw_mode;
	int c_offset;
	int d_offset;
	u32 c_group_offset;
	u32 d_group_offset;
	void __iomem *gpio_base;
	u16 chip_cs;
	u16 sw_rev;
};
struct cycapsense_ctrl_data {
	struct device *dev;
	struct hssp_data hssp_d;
	struct work_struct work;
	int cmd;
};

#define HSSP_CMD_NONE		0
#define HSSP_CMD_RESET		1
#define HSSP_CMD_ERASE		2
#define HSSP_CMD_FW_UPDATE	3

int cycapsense_hssp_dnld(struct hssp_data *d);

#define P4000_METADATA_SIZE		12
#define SILICON_ID_BYTE_LENGTH		4
#define FLASH_ROW_BYTE_SIZE_HEX_FILE	128
#define NUMBER_OF_FLASH_ROWS_HEX_FILE	128
#define SW_REVISION_OFFSET		0x3f7c

#define FAILURE 1
#define SUCCESS 0

#define RST_GPIO 1
#define CLK_GPIO 4
#define DAT_GPIO 7


#endif /* __CYCAPSENSE_HSSP_H__ */
