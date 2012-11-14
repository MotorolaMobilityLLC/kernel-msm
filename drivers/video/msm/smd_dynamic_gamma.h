/*
 * Copyright (C) 2012 Motorola Mobility LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/types.h>
#include <linux/seq_file.h>

#define RAW_MTP_SIZE 24
#define RAW_GAMMA_SIZE 26

#define NUM_VOLT_PTS 7
#define NUM_COLORS   3
#define NUM_NIT_LVLS 300
#define NUM_GRAY_LVLS 256

int smd_dynamic_gamma_calc(uint32_t v0, uint8_t preamble_1, uint8_t preamble_2,
			uint8_t raw_mtp[RAW_MTP_SIZE],
			uint16_t in_gamma[NUM_VOLT_PTS][NUM_COLORS],
			uint8_t out_gamma[NUM_NIT_LVLS][RAW_GAMMA_SIZE]);

void smd_dynamic_gamma_dbg_dump(uint8_t raw_mtp[RAW_MTP_SIZE],
				uint8_t out_gamma[NUM_NIT_LVLS][RAW_GAMMA_SIZE],
				struct seq_file *m, void *data, bool csv);
