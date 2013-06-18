
/*
 * Header file for:
 * Cypress CapSense(TM) firmware downloader.
 * Supported parts include:
 * CY8C20237
 *
 * Copyright (c) 2012-2013 Motorola Mobility LLC
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
#ifndef __CYCAPSENSE_ISSP_H__
#define __CYCAPSENSE_ISSP_H__

struct capsense_id {
	const char *name;
	unsigned short id;
	int block_size;
	int num_of_blocks;
	int secure_bytes;
};
struct hex_info {
	const char *fw_name;
	u8 *data;
	u8 *s_data;
	u16 cs;
};
struct issp_data {
	struct hex_info inf;
	const struct capsense_id *id;
	int c_gpio;
	int d_gpio;
	int rst_gpio;
};
int cycapsense_issp_parse_hex(struct hex_info *inf, const u8 *src, int len,
		const struct capsense_id *id);

int cycapsense_issp_get_cs(const struct issp_data *d, unsigned int *cs);

const struct capsense_id *cycapsense_issp_verify_id(const struct issp_data *d);

int cycapsense_issp_erase(const struct issp_data *d);

int cycapsense_issp_dnld(const struct issp_data *d);

#endif /* __CYCAPSENSE_ISSP_H__ */
