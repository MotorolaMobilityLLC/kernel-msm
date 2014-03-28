/*
 * =================================================================
 *
 *       Filename:  smart_dimming.h
 *
 *    Description:  cmc624, lvds control driver
 *
 *        Author:  Krishna Kishor Jha,
 *        Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2011, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/

#ifndef __SMART_DIMMING_H__
#define __SMART_DIMMING_H__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>

struct smartdim_conf {
	void (*generate_gamma)(int cd, char *str);
	void (*get_min_lux_table)(char *str, int size);
	void (*init)(void);
	void (*print_aid_log)(void);
	char *mtp_buffer;
	int *lux_tab;
	int lux_tabsize;
	unsigned int man_id;
};
/* Define the smart dimming LDIs*/
struct smartdim_conf *smart_S6E63J_get_conf(void);
enum {
	CI_RED = 0,
	CI_GREEN = 1,
	CI_BLUE = 2,
	CI_MAX = 3,
};

enum {
	IV_1 = 0,
	IV_15 = 1,
	IV_35 = 2,
	IV_59 = 3,
	IV_87 = 4,
	IV_171 = 5,
	IV_255 = 6,
	IV_MAX = 7,
	IV_TABLE_MAX = 8,
};

enum {
	AD_IV0 = 0,
	AD_IV1 = 1,
	AD_IV15 = 2,
	AD_IV35 = 3,
	AD_IV59 = 4,
	AD_IV87 = 5,
	AD_IV171 = 6,
	AD_IV255 = 7,
	AD_IVMAX = 8,
};

#define MAX_GRADATION   300

struct str_voltage_entry {
	u32 v[CI_MAX];
};

struct str_table_info {
	u8 st;
	u8 et;
	u8 count;
	u8 *offset_table;
	u32 rv;
};

struct str_flookup_table {
	u16 entry;
	u16 count;
};

struct str_smart_dim {
	s16 mtp[CI_MAX][IV_MAX];
	struct str_voltage_entry ve[256];
	u8 *default_gamma;
	struct str_table_info t_info[IV_TABLE_MAX];
	struct str_flookup_table *flooktbl;
	u32 *g22_tbl;
	u32 *g300_gra_tbl;
	u32 adjust_volt[CI_MAX][AD_IVMAX];
	s16 adjust_mtp[CI_MAX][IV_MAX];

};

int init_table_info(struct str_smart_dim *smart, unsigned char *srcGammaTable);
u8 calc_voltage_table(struct str_smart_dim *smart, const u8 *mtp);
u32 calc_gamma_table(struct str_smart_dim *smart, u32 gv, u8 result[]);

#endif
