/*
 * =================================================================
 *
 *       Filename:  smart_mtp_se6e8fa.h
 *
 *    Description:  Smart dimming algorithm implementation
 *
 *        Author: jb09.kim
 *        Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2012, Samsung Electronics. All rights reserved.

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
#ifndef _SMART_MTP_SE6E8FA_H_
#define _SMART_MTP_SE6E8FA_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <asm/div64.h>

/* #define AID_OPERATION */

#define GAMMA_CURVE_2P2 2
#define LUMINANCE_MAX 30
#define GAMMA_SET_MAX 27
#define VT_GAMMA_SET_MAX 3
#define VT_GAMMA_OFFSET	(GAMMA_SET_MAX - VT_GAMMA_SET_MAX)

#define BIT_SHIFT 22
/*
	it means BIT_SHIFT is 22.  pow(2,BIT_SHIFT) is 4194304.
	BIT_SHIFT is used for right bit shfit
*/
#define BIT_SHFIT_MUL	4194304

#define S6E8FA_GRAY_SCALE_MAX	256

/*6.3*4194304 */
#define S6E8FA_VREG0_REF 17616076

/*V0,V1,V5,V15,V31,V63,V127,V191,V255*/
#define S6E8FA_MAX 9

/* PANEL DEPENDENT THINGS */
#define MAX_CANDELA	300
#define MIN_CANDELA	10

/* PANEL DEPENDENT THINGS END*/

enum {
	V1_INDEX = 0,
	V5_INDEX = 1,
	V15_INDEX = 2,
	V31_INDEX = 3,
	V63_INDEX = 4,
	V127_INDEX = 5,
	V191_INDEX = 6,
	V255_INDEX = 7,
};

struct GAMMA_LEVEL {
	int level_0;
	int level_1;
	int level_5;
	int level_15;
	int level_31;
	int level_63;
	int level_127;
	int level_191;
	int level_255;
} __packed;

struct RGB_OUTPUT_VOLTARE {
	struct GAMMA_LEVEL R_VOLTAGE;
	struct GAMMA_LEVEL G_VOLTAGE;
	struct GAMMA_LEVEL B_VOLTAGE;
} __packed;

struct GRAY_VOLTAGE {
	/*
		This voltage value use 14bit right shit
		it means voltage is divied by 16384.
	*/
	int R_Gray;
	int G_Gray;
	int B_Gray;
} __packed;

struct GRAY_SCALE {
	struct GRAY_VOLTAGE TABLE[S6E8FA_GRAY_SCALE_MAX];
	struct GRAY_VOLTAGE VT_TABLE;
} __packed;

/*V0,V1,V5,V15,V31,V63,V127,V191,V255*/

struct MTP_SET {
	char OFFSET_255_MSB;
	char OFFSET_255_LSB;
	char OFFSET_191;
	char OFFSET_127;
	char OFFSET_63;
	char OFFSET_31;
	char OFFSET_15;
	char OFFSET_5;
	char OFFSET_1;
} __packed;

#ifdef CONFIG_HBM_PSRE
struct MTP_OFFSET_400CD {
	struct MTP_SET R_OFFSET;
	struct MTP_SET G_OFFSET;
	struct MTP_SET B_OFFSET;
} __packed;
#endif

struct MTP_OFFSET {
	struct MTP_SET R_OFFSET;
	struct MTP_SET G_OFFSET;
	struct MTP_SET B_OFFSET;
} __packed;

struct illuminance_table {
	int lux;
	char gamma_setting[GAMMA_SET_MAX];
} __packed;

struct SMART_DIM {
#ifdef CONFIG_HBM_PSRE
	struct MTP_OFFSET_400CD MTP_ORIGN;
#else
	struct MTP_OFFSET MTP_ORIGN;
#endif
	struct MTP_OFFSET MTP;
	struct RGB_OUTPUT_VOLTARE RGB_OUTPUT;
	struct GRAY_SCALE GRAY;

	/* Because of AID funtion, below members are added*/
	int lux_table_max;
	int *plux_table;
	struct illuminance_table gen_table[LUMINANCE_MAX];

	int brightness_level;
	int ldi_revision;
} __packed;

#endif

