/*
 * =================================================================
 *
 *       Filename:  smart_mtp_se6e8fa.c
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

#include "smart_mtp_s6e63j.h"
#include "smart_mtp_2p2_gamma_s6e63j.h"
#include "smart_dimming.h"

/* #define SMART_DIMMING_DEBUG 1 */

static char max_lux_table[GAMMA_SET_MAX];
static char min_lux_table[GAMMA_SET_MAX];

/*
*	To support different center cell gamma setting
*/
static char V255_300CD_R_MSB;
static char V255_300CD_R_LSB;

static char V255_300CD_G_MSB;
static char V255_300CD_G_LSB;

static char V255_300CD_B_MSB;
static char V255_300CD_B_LSB;

static char V191_300CD_R;
static char V191_300CD_G;
static char V191_300CD_B;

static char V127_300CD_R;
static char V127_300CD_G;
static char V127_300CD_B;

static char V63_300CD_R;
static char V63_300CD_G;
static char V63_300CD_B;

static char V31_300CD_R;
static char V31_300CD_G;
static char V31_300CD_B;

static char V15_300CD_R;
static char V15_300CD_G;
static char V15_300CD_B;

static char V5_300CD_R;
static char V5_300CD_G;
static char V5_300CD_B;

static char VT_300CD_R;
static char VT_300CD_G;
static char VT_300CD_B;

#ifdef SMART_DIMMING_DEBUG
static int char_to_int(char data1)
{
	int cal_data;

	if (data1 & 0x80) {
		cal_data = data1 & 0x7F;
		cal_data *= -1;
	} else
		cal_data = data1;

	return cal_data;
}

static int char_to_int_v255(char data1, char data2)
{
	int cal_data;

	if (data1)
		cal_data = (data1 << 8) | data2;
	else
		cal_data = data2;

	return cal_data;
}

static void print_RGB_offset(struct SMART_DIM *pSmart)
{
	pr_info("%s MTP Offset VT R:%x G:%x B:%x\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_1),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_1),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_1));

	pr_info("%s MTP Offset V5 R:%x G:%x B:%x\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_5),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_5),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_5));
	pr_info("%s MTP Offset V15 R:%x G:%x B:%x\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_15),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_15),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_15));
	pr_info("%s MTP Offset V31 R:%x G:%x B:%x\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_31),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_31),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_31));
	pr_info("%s MTP Offset V63 R:%x G:%x B:%x\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_63),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_63),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_63));
	pr_info("%s MTP Offset V127 R:%x G:%x B:%x\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_127),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_127),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_127));
	pr_info("%s MTP Offset V191 R:%x G:%x B:%x\n", __func__,
			char_to_int(pSmart->MTP.R_OFFSET.OFFSET_191),
			char_to_int(pSmart->MTP.G_OFFSET.OFFSET_191),
			char_to_int(pSmart->MTP.B_OFFSET.OFFSET_191));
	pr_info("%s MTP Offset V255 R:%x G:%x B:%x\n", __func__,
			char_to_int_v255(pSmart->MTP.R_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.R_OFFSET.OFFSET_255_LSB),
			char_to_int_v255(pSmart->MTP.G_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.G_OFFSET.OFFSET_255_LSB),
			char_to_int_v255(pSmart->MTP.B_OFFSET.OFFSET_255_MSB,
				pSmart->MTP.B_OFFSET.OFFSET_255_LSB));
}
#endif
#define v255_coefficient 0
#define v255_denominator 605
static int v255_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;
	int v255_value;

	v255_value = (V255_300CD_R_MSB << 8) | (V255_300CD_R_LSB);
	result_1 = result_2 = (v255_coefficient + v255_value) << BIT_SHIFT;
	do_div(result_2, v255_denominator);
	result_3 = (S6E8FA_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = S6E8FA_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_0 = S6E8FA_VREG0_REF;

	v255_value = (V255_300CD_G_MSB << 8) | (V255_300CD_G_LSB);
	result_1 = result_2 = (v255_coefficient + v255_value) << BIT_SHIFT;
	do_div(result_2, v255_denominator);
	result_3 = (S6E8FA_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = S6E8FA_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_0 = S6E8FA_VREG0_REF;

	v255_value = (V255_300CD_B_MSB << 8) | (V255_300CD_B_LSB);
	result_1 = result_2 = (v255_coefficient + v255_value) << BIT_SHIFT;
	do_div(result_2, v255_denominator);
	result_3 = (S6E8FA_VREG0_REF * result_2) >> BIT_SHIFT;
	result_4 = S6E8FA_VREG0_REF - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_255 = result_4;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_0 = S6E8FA_VREG0_REF;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V255 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_255,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_255,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_255);
#endif

	return 0;
}

static void v255_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = S6E8FA_VREG0_REF -
			(pSmart->GRAY.TABLE[index[V255_INDEX]].R_Gray);
	result_2 = result_1 * v255_denominator;
	do_div(result_2, S6E8FA_VREG0_REF);
	result_3 = result_2  - v255_coefficient;
	str[21] = (result_3 & 0xff00) >> 8;
	str[22] = result_3 & 0xff;

	result_1 = S6E8FA_VREG0_REF -
			(pSmart->GRAY.TABLE[index[V255_INDEX]].G_Gray);
	result_2 = result_1 * v255_denominator;
	do_div(result_2, S6E8FA_VREG0_REF);
	result_3 = result_2  - v255_coefficient;
	str[23] = (result_3 & 0xff00) >> 8;
	str[24] = result_3 & 0xff;

	result_1 = S6E8FA_VREG0_REF -
			(pSmart->GRAY.TABLE[index[V255_INDEX]].B_Gray);
	result_2 = result_1 * v255_denominator;
	do_div(result_2, S6E8FA_VREG0_REF);
	result_3 = result_2  - v255_coefficient;
	str[25] = (result_3 & 0xff00) >> 8;
	str[26] = result_3 & 0xff;

}

#define vt_coefficient 0
#define vt_denominator 605
static int vt_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = vt_coefficient << BIT_SHIFT;
	do_div(result_1, vt_denominator);
	result_2 = (S6E8FA_VREG0_REF * result_1) >> BIT_SHIFT;
	result_3 = S6E8FA_VREG0_REF - result_2;
	pSmart->GRAY.VT_TABLE.R_Gray = result_3;

	result_1 = vt_coefficient << BIT_SHIFT;
	do_div(result_1, vt_denominator);
	result_2 = (S6E8FA_VREG0_REF * result_1) >> BIT_SHIFT;
	result_3 = S6E8FA_VREG0_REF - result_2;
	pSmart->GRAY.VT_TABLE.G_Gray = result_3;

	result_1 = vt_coefficient << BIT_SHIFT;
	do_div(result_1, vt_denominator);
	result_2 = (S6E8FA_VREG0_REF * result_1) >> BIT_SHIFT;
	result_3 = S6E8FA_VREG0_REF - result_2;
	pSmart->GRAY.VT_TABLE.B_Gray = result_3;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s VT RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->GRAY.VT_TABLE.R_Gray,
			pSmart->GRAY.VT_TABLE.G_Gray,
			pSmart->GRAY.VT_TABLE.B_Gray);
#endif
	return 0;
}

static void vt_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	str[0] = VT_300CD_R;
	str[1] = VT_300CD_G;
	str[2] = VT_300CD_B;
}

#define v191_coefficient 64
#define v191_denominator 128
static int v191_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
				- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_255);
	result_2 = (v191_coefficient + V191_300CD_R) << BIT_SHIFT;
	do_div(result_2, v191_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_191 = result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
				- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_255);
	result_2 = (v191_coefficient + V191_300CD_G) << BIT_SHIFT;
	do_div(result_2, v191_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_191 = result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
				- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_255);
	result_2 = (v191_coefficient + V191_300CD_B) << BIT_SHIFT;
	do_div(result_2, v191_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_191 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V191 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_191,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_191,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_191);
#endif
	return 0;
}

static void v191_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V191_INDEX]].R_Gray);
	result_2 = result_1 * v191_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[18] = (result_2  - v191_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V191_INDEX]].G_Gray);
	result_2 = result_1 * v191_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[19] = (result_2  - v191_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V191_INDEX]].B_Gray);
	result_2 = result_1 * v191_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V255_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[20] = (result_2  - v191_coefficient) & 0xff;
}

#define v127_coefficient 64
#define v127_denominator 128
static int v127_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_191);
	result_2 = (v127_coefficient + V127_300CD_R) << BIT_SHIFT;
	do_div(result_2, v127_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_127 = result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_191);
	result_2 = (v127_coefficient + V127_300CD_G) << BIT_SHIFT;
	do_div(result_2, v127_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_127 = result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_191);
	result_2 = (v127_coefficient + V127_300CD_B) << BIT_SHIFT;
	do_div(result_2, v127_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_127 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V127 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_127,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_127,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_127);
#endif
	return 0;
}

static void v127_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;
	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V127_INDEX]].R_Gray);
	result_2 = result_1 * v127_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V191_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[15] = (result_2  - v127_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V127_INDEX]].G_Gray);
	result_2 = result_1 * v127_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V191_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[16] = (result_2  - v127_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V127_INDEX]].B_Gray);
	result_2 = result_1 * v127_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V191_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[17] = (result_2  - v127_coefficient) & 0xff;
}


#define v63_coefficient 64
#define v63_denominator 128
static int v63_adjustment(struct SMART_DIM *pSmart)
{

	unsigned long long result_1, result_2, result_3, result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_127);
	result_2 = (v63_coefficient + V63_300CD_R) << BIT_SHIFT;
	do_div(result_2, v63_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_63 = result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_127);
	result_2 = (v63_coefficient + V63_300CD_G) << BIT_SHIFT;
	do_div(result_2, v63_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_63 = result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_127);
	result_2 = (v63_coefficient + V63_300CD_B) << BIT_SHIFT;
	do_div(result_2, v63_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_63 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V63 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_63,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_63,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_63);
#endif
	return 0;
}

static void v63_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V63_INDEX]].R_Gray);
	result_2 = result_1 * v63_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V127_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[12] = (result_2  - v63_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V63_INDEX]].G_Gray);
	result_2 = result_1 * v63_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V127_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[13] = (result_2  - v63_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V63_INDEX]].B_Gray);
	result_2 = result_1 * v63_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V127_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[14] = (result_2  - v63_coefficient) & 0xff;
}

#define v31_coefficient 64
#define v31_denominator 128
static int v31_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_63);
	result_2 = (v31_coefficient + V31_300CD_R) << BIT_SHIFT;
	do_div(result_2, v31_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_31 = result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_63);
	result_2 = (v31_coefficient + V31_300CD_G) << BIT_SHIFT;
	do_div(result_2, v31_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_31 = result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_63);
	result_2 = (v31_coefficient + V31_300CD_B) << BIT_SHIFT;
	do_div(result_2, v31_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_31 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V31 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_31,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_31,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_31);
#endif
	return 0;
}

static void v31_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V31_INDEX]].R_Gray);
	result_2 = result_1 * v31_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V63_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[9] = (result_2  - v31_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V31_INDEX]].G_Gray);
	result_2 = result_1 * v31_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V63_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[10] = (result_2  - v31_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V31_INDEX]].B_Gray);
	result_2 = result_1 * v31_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V63_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[11] = (result_2  - v31_coefficient) & 0xff;
}

#define v15_coefficient 16
#define v15_denominator 144
static int v15_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_31);
	result_2 = (v15_coefficient + V15_300CD_R) << BIT_SHIFT;
	do_div(result_2, v15_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.R_Gray) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_15 = result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_31);
	result_2 = (v15_coefficient + V15_300CD_G) << BIT_SHIFT;
	do_div(result_2, v15_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.G_Gray) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_15 = result_4;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_31);
	result_2 = (v15_coefficient + V15_300CD_B) << BIT_SHIFT;
	do_div(result_2, v15_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (pSmart->GRAY.VT_TABLE.B_Gray) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_15 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V15 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_15,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_15,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_15);
#endif
	return 0;
}

static void v15_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V15_INDEX]].R_Gray);
	result_2 = result_1 * v15_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.R_Gray)
			- (pSmart->GRAY.TABLE[index[V31_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[6] = (result_2  - v15_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V15_INDEX]].G_Gray);
	result_2 = result_1 * v15_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.G_Gray)
			- (pSmart->GRAY.TABLE[index[V31_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[7] = (result_2  - v15_coefficient) & 0xff;

	result_1 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V15_INDEX]].B_Gray);
	result_2 = result_1 * v15_denominator;
	result_3 = (pSmart->GRAY.VT_TABLE.B_Gray)
			- (pSmart->GRAY.TABLE[index[V31_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[8] = (result_2  - v15_coefficient) & 0xff;
}

#define v5_coefficient 0
#define v5_denominator 144
static int v5_adjustment(struct SMART_DIM *pSmart)
{
	unsigned long long result_1, result_2, result_3, result_4;

	result_1 = (S6E8FA_VREG0_REF)
			- (pSmart->RGB_OUTPUT.R_VOLTAGE.level_15);
	result_2 = (v5_coefficient + V5_300CD_R) << BIT_SHIFT;
	do_div(result_2, v5_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (S6E8FA_VREG0_REF) - result_3;
	pSmart->RGB_OUTPUT.R_VOLTAGE.level_5 = result_4;

	result_1 = (S6E8FA_VREG0_REF)
			- (pSmart->RGB_OUTPUT.G_VOLTAGE.level_15);
	result_2 = (v5_coefficient + V5_300CD_G) << BIT_SHIFT;
	do_div(result_2, v5_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (S6E8FA_VREG0_REF) - result_3;
	pSmart->RGB_OUTPUT.G_VOLTAGE.level_5 = result_4;

	result_1 = (S6E8FA_VREG0_REF)
			- (pSmart->RGB_OUTPUT.B_VOLTAGE.level_15);
	result_2 = (v5_coefficient + V5_300CD_B) << BIT_SHIFT;
	do_div(result_2, v5_denominator);
	result_3 = (result_1 * result_2) >> BIT_SHIFT;
	result_4 = (S6E8FA_VREG0_REF) - result_3;
	pSmart->RGB_OUTPUT.B_VOLTAGE.level_5 = result_4;

#ifdef SMART_DIMMING_DEBUG
	pr_info("%s V3 RED:%d GREEN:%d BLUE:%d\n", __func__,
			pSmart->RGB_OUTPUT.R_VOLTAGE.level_5,
			pSmart->RGB_OUTPUT.G_VOLTAGE.level_5,
			pSmart->RGB_OUTPUT.B_VOLTAGE.level_5);
#endif
	return 0;
}

static void v5_hexa(int *index, struct SMART_DIM *pSmart, char *str)
{
	unsigned long long result_1, result_2, result_3;

	result_1 = (S6E8FA_VREG0_REF)
			- (pSmart->GRAY.TABLE[index[V5_INDEX]].R_Gray);
	result_2 = result_1 * v5_denominator;
	result_3 = (S6E8FA_VREG0_REF)
			- (pSmart->GRAY.TABLE[index[V15_INDEX]].R_Gray);
	do_div(result_2, result_3);
	str[3] = (result_2  - v5_coefficient) & 0xff;

	result_1 = (S6E8FA_VREG0_REF)
			- (pSmart->GRAY.TABLE[index[V5_INDEX]].G_Gray);
	result_2 = result_1 * v5_denominator;
	result_3 = (S6E8FA_VREG0_REF)
			- (pSmart->GRAY.TABLE[index[V15_INDEX]].G_Gray);
	do_div(result_2, result_3);
	str[4] = (result_2  - v5_coefficient) & 0xff;

	result_1 = (S6E8FA_VREG0_REF)
			- (pSmart->GRAY.TABLE[index[V5_INDEX]].B_Gray);
	result_2 = result_1 * v5_denominator;
	result_3 = (S6E8FA_VREG0_REF)
			- (pSmart->GRAY.TABLE[index[V15_INDEX]].B_Gray);
	do_div(result_2, result_3);
	str[5] = (result_2  - v5_coefficient) & 0xff;
}


/*V0,V1,V3,V11,V23,V35,V51,V87,V151,V203,V255*/
static int S6E8FA_ARRAY[S6E8FA_MAX] = {0, 1, 5, 15, 31, 63, 127, 191, 255};

#define V0toV5_Coefficient 4
#define V0toV5_Multiple 1
#define V0toV5_denominator 5

#define V5toV15_Coefficient 39
#define V5toV15_Multiple 1
#define V5toV15_denominator 40

#define V15toV31_Coefficient 15
#define V15toV31_Multiple 1
#define V15toV31_denominator 16

#define V31toV63_Coefficient 31
#define V31toV63_Multiple 1
#define V31toV63_denominator 32

#define V63toV127_Coefficient 63
#define V63toV127_Multiple 1
#define V63toV127_denominator 64

#define V127toV191_Coefficient 63
#define V127toV191_Multiple 1
#define V127toV191_denominator 64

#define V191toV255_Coefficient 63
#define V191toV255_Multiple 1
#define V191toV255_denominator 64

static int cal_gray_scale_linear(int up, int low, int coeff,
int mul, int deno, int cnt)
{
	unsigned long long result_1, result_2, result_3, result_4;

	result_1 = up - low;
	result_2 = (result_1 * (coeff - (cnt * mul))) << BIT_SHIFT;
	do_div(result_2, deno);
	result_3 = result_2 >> BIT_SHIFT;
	result_4 = up - result_3;

	return (int)result_4;
}

static int generate_gray_scale(struct SMART_DIM *pSmart)
{
	int cnt = 0, cal_cnt = 0;
	int array_index = 0;
	struct GRAY_VOLTAGE *ptable = (struct GRAY_VOLTAGE *)
						(&(pSmart->GRAY.TABLE));

	for (cnt = 0; cnt < S6E8FA_MAX; cnt++) {
		pSmart->GRAY.TABLE[S6E8FA_ARRAY[cnt]].R_Gray =
			((int *)&(pSmart->RGB_OUTPUT.R_VOLTAGE))[cnt];

		pSmart->GRAY.TABLE[S6E8FA_ARRAY[cnt]].G_Gray =
			((int *)&(pSmart->RGB_OUTPUT.G_VOLTAGE))[cnt];

		pSmart->GRAY.TABLE[S6E8FA_ARRAY[cnt]].B_Gray =
			((int *)&(pSmart->RGB_OUTPUT.B_VOLTAGE))[cnt];
	}

	/*
		below codes use hard coded value.
		So it is possible to modify on each model.
		V0,V1,V5,V15,V31,V63,V127,V191,V255
	*/
	for (cnt = 0; cnt < S6E8FA_GRAY_SCALE_MAX; cnt++) {
		if (cnt == S6E8FA_ARRAY[0]) {
			/* 0 */
			array_index = 0;
			cal_cnt = 3;
		} else if ((cnt > S6E8FA_ARRAY[0]) &&
			(cnt < S6E8FA_ARRAY[2])) {
			/* 1 ~ 4 */
			array_index = 2;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-2]].R_Gray,
			ptable[S6E8FA_ARRAY[array_index]].R_Gray,
			V0toV5_Coefficient, V0toV5_Multiple,
			V0toV5_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-2]].G_Gray,
			ptable[S6E8FA_ARRAY[array_index]].G_Gray,
			V0toV5_Coefficient, V0toV5_Multiple,
			V0toV5_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-2]].B_Gray,
			ptable[S6E8FA_ARRAY[array_index]].B_Gray,
			V0toV5_Coefficient, V0toV5_Multiple,
			V0toV5_denominator , cal_cnt);

			cal_cnt--;
		} else if (cnt == S6E8FA_ARRAY[2]) {
			/* 5 */
			cal_cnt = 34;
		} else if ((cnt > S6E8FA_ARRAY[2]) &&
			(cnt < S6E8FA_ARRAY[3])) {
			/* 6 ~ 14 */
			array_index = 3;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].R_Gray,
			ptable[S6E8FA_ARRAY[array_index]].R_Gray,
			V5toV15_Coefficient, V5toV15_Multiple,
			V5toV15_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].G_Gray,
			ptable[S6E8FA_ARRAY[array_index]].G_Gray,
			V5toV15_Coefficient, V5toV15_Multiple,
			V5toV15_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].B_Gray,
			ptable[S6E8FA_ARRAY[array_index]].B_Gray,
			V5toV15_Coefficient, V5toV15_Multiple,
			V5toV15_denominator , cal_cnt);

			if (cnt < 10)
				cal_cnt -= 5;
			else
				cal_cnt -= 3;
		} else if (cnt == S6E8FA_ARRAY[3]) {
			/* 15 */
			cal_cnt = 14;
		} else if ((cnt > S6E8FA_ARRAY[3]) &&
			(cnt < S6E8FA_ARRAY[4])) {
			/* 16 ~ 30 */
			array_index = 4;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].R_Gray,
			ptable[S6E8FA_ARRAY[array_index]].R_Gray,
			V15toV31_Coefficient, V15toV31_Multiple,
			V15toV31_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].G_Gray,
			ptable[S6E8FA_ARRAY[array_index]].G_Gray,
			V15toV31_Coefficient, V15toV31_Multiple,
			V15toV31_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].B_Gray,
			ptable[S6E8FA_ARRAY[array_index]].B_Gray,
			V15toV31_Coefficient, V15toV31_Multiple,
			V15toV31_denominator , cal_cnt);

			cal_cnt--;
		}  else if (cnt == S6E8FA_ARRAY[4]) {
			/* 31 */
			cal_cnt = 30;
		} else if ((cnt > S6E8FA_ARRAY[4]) &&
			(cnt < S6E8FA_ARRAY[5])) {
			/* 32 ~ 62 */
			array_index = 5;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].R_Gray,
			ptable[S6E8FA_ARRAY[array_index]].R_Gray,
			V31toV63_Coefficient, V31toV63_Multiple,
			V31toV63_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].G_Gray,
			ptable[S6E8FA_ARRAY[array_index]].G_Gray,
			V31toV63_Coefficient, V31toV63_Multiple,
			V31toV63_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].B_Gray,
			ptable[S6E8FA_ARRAY[array_index]].B_Gray,
			V31toV63_Coefficient, V31toV63_Multiple,
			V31toV63_denominator , cal_cnt);

			cal_cnt--;
		} else if (cnt == S6E8FA_ARRAY[5]) {
			/* 63 */
			cal_cnt = 62;
		} else if ((cnt > S6E8FA_ARRAY[5]) &&
			(cnt < S6E8FA_ARRAY[6])) {
			/* 64 ~ 126 */
			array_index = 6;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].R_Gray,
			ptable[S6E8FA_ARRAY[array_index]].R_Gray,
			V63toV127_Coefficient, V63toV127_Multiple,
			V63toV127_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].G_Gray,
			ptable[S6E8FA_ARRAY[array_index]].G_Gray,
			V63toV127_Coefficient, V63toV127_Multiple,
			V63toV127_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].B_Gray,
			ptable[S6E8FA_ARRAY[array_index]].B_Gray,
			V63toV127_Coefficient, V63toV127_Multiple,
			V63toV127_denominator, cal_cnt);
			cal_cnt--;

		} else if (cnt == S6E8FA_ARRAY[6]) {
			/* 127 */
			cal_cnt = 62;
		} else if ((cnt > S6E8FA_ARRAY[6]) &&
			(cnt < S6E8FA_ARRAY[7])) {
			/* 128 ~ 190 */
			array_index = 7;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].R_Gray,
			ptable[S6E8FA_ARRAY[array_index]].R_Gray,
			V127toV191_Coefficient, V127toV191_Multiple,
			V127toV191_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].G_Gray,
			ptable[S6E8FA_ARRAY[array_index]].G_Gray,
			V127toV191_Coefficient, V127toV191_Multiple,
			V127toV191_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].B_Gray,
			ptable[S6E8FA_ARRAY[array_index]].B_Gray,
			V127toV191_Coefficient, V127toV191_Multiple,
			V127toV191_denominator, cal_cnt);
			cal_cnt--;

		} else if (cnt == S6E8FA_ARRAY[7]) {
			/* 191 */
			cal_cnt = 62;
		} else if ((cnt > S6E8FA_ARRAY[7]) &&
			(cnt < S6E8FA_ARRAY[8])) {
			/* 192 ~ 254 */
			array_index = 8;

			pSmart->GRAY.TABLE[cnt].R_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].R_Gray,
			ptable[S6E8FA_ARRAY[array_index]].R_Gray,
			V191toV255_Coefficient, V191toV255_Multiple,
			V191toV255_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].G_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].G_Gray,
			ptable[S6E8FA_ARRAY[array_index]].G_Gray,
			V191toV255_Coefficient, V191toV255_Multiple,
			V191toV255_denominator, cal_cnt);

			pSmart->GRAY.TABLE[cnt].B_Gray = cal_gray_scale_linear(
			ptable[S6E8FA_ARRAY[array_index-1]].B_Gray,
			ptable[S6E8FA_ARRAY[array_index]].B_Gray,
			V191toV255_Coefficient, V191toV255_Multiple,
			V191toV255_denominator, cal_cnt);

			cal_cnt--;
		 } else {
			if (cnt == S6E8FA_ARRAY[8]) {
				pr_info("%s end\n", __func__);
			} else {
				pr_err("%s fail cnt:%d\n", __func__, cnt);
				return -EINVAL;
			}
		}

	}
#ifdef SMART_DIMMING_DEBUG
		for (cnt = 0; cnt < S6E8FA_GRAY_SCALE_MAX; cnt++) {
			pr_info("%s %8d %8d %8d %d\n", __func__,
				pSmart->GRAY.TABLE[cnt].R_Gray,
				pSmart->GRAY.TABLE[cnt].G_Gray,
				pSmart->GRAY.TABLE[cnt].B_Gray, cnt);
		}
#endif
	return 0;
}

static int searching_function(long long candela, int *index, int gamma_curve)
{
	long long delta_1 = 0, delta_2 = 0;
	int cnt;

	/*
	*	This searching_functin should be changed with improved
		searcing algorithm to reduce searching time.
	*/
	*index = -1;
	for (cnt = 0; cnt < (S6E8FA_GRAY_SCALE_MAX - 1); cnt++) {
		delta_1 = candela - curve_2p2[cnt];
		delta_2 = candela - curve_2p2[cnt+1];


		if (delta_2 < 0) {
			*index = (delta_1 + delta_2) <= 0 ? cnt : cnt+1;
			break;
		}

		if (delta_1 == 0) {
			*index = cnt;
			break;
		}

		if (delta_2 == 0) {
			*index = cnt+1;
			break;
		}
	}

	if (*index == -1)
		return -EINVAL;
	else
		return 0;
}


/* -1 means V1 */
#define S6E8FA_TABLE_MAX  (S6E8FA_MAX - 1)
static void(*Make_hexa[S6E8FA_TABLE_MAX])(int*, struct SMART_DIM*, char*) = {
	v255_hexa,
	v191_hexa,
	v127_hexa,
	v63_hexa,
	v31_hexa,
	v15_hexa,
	v5_hexa,
	vt_hexa,
};

#if defined(AID_OPERATION)
/*
*	Because of AID operation & display quality.
*
*	only smart dimmg range : 300CD ~ 190CD
*	AOR fix range : 180CD ~ 110CD  AOR 40%
*	AOR adjust range : 100CD ~ 10CD
*/

#define CCG6_MAX_TABLE 30
static int ccg6_candela_table[][2] = {
	{10, 0,},
	{20, 1,},
	{30, 2,},
	{40, 3,},
	{50, 4,},
	{60, 5,},
	{70, 6,},
	{80, 7,},
	{90, 8,},
	{100, 9,},
	{110, 10,},
	{120, 11,},
	{130, 12,},
	{140, 13,},
	{150, 14,},
	{160, 15,},
	{170, 16,},
	{180, 17,},
	{190, 18,},
	{200, 19,},
	{210, 20,},
	{220, 21,},
	{230, 22,},
	{240, 23,},
	{250, 24,},
	{260, 25,},
	{270, 26,},
	{280, 27,},
	{290, 28,},
	{300, 29,},
};
#define RGB_COMPENSATION 21
static int find_cadela_table(int brightness)
{
	int loop;
	int err = -1;

	for (loop = 0; loop <= CCG6_MAX_TABLE; loop++)
		if (ccg6_candela_table[loop][0] == brightness)
			return ccg6_candela_table[loop][1];

	return err;
}

static int gradation_offset[][7] = {
/*	V255 V191 V127 V63 V31 V15 V3*/
	{0, 0, 0, 0, 0, 0, 0},
	{5, 5, 4, 2, 7, 6, 7},
	{4, 3, 3, 2, 5, 5, 7},
	{3, 3, 3, 2, 4, 4, 5},
	{2, 3, 3, 3, 3, 4, 5},
	{2, 3, 3, 3, 1, 3, 5},
	{0, 4, 3, 2, 2, 3, 5},
	{0, 3, 2, 3, 2, 3, 3},
	{1, 2, 3, 2, 2, 3, 3},
	{0, 2, 3, 3, 2, 3, 3},
	{1, 3, 4, 4, 2, 3, 2},
	{1, 2, 4, 3, 2, 3, 2},
	{2, 2, 5, 4, 3, 3, 2},
	{2, 2, 4, 4, 3, 3, 2},
	{2, 2, 4, 3, 2, 2, 1},
	{1, 3, 4, 4, 2, 2, 0},
	{1, 2, 4, 4, 3, 3, 0},
	{1, 2, 4, 3, 3, 2, 0},
	{1, 1, 3, 3, 2, 2, 0},
	{1, 1, 2, 3, 3, 2, 0},
	{1, 2, 3, 3, 2, 2, 0},
	{1, 2, 3, 3, 3, 2, 0},
	{1, 2, 4, 3, 3, 2, 0},
	{1, 2, 3, 3, 3, 2, 0},
	{1, 2, 2, 2, 3, 1, 0},
	{1, 2, 2, 2, 2, 1, 0},
	{1, 3, 3, 3, 3, 1, 0},
	{2, 3, 2, 3, 3, 2, 0},
	{1, 2, 2, 2, 3, 1, 0},
	{1, 2, 2, 1, 2, 1, 0},
	{0, 0, 0, 0, 0, 0, 0},
};

static int rgb_offset_evt1_third[][RGB_COMPENSATION] = {
/* RGB255  RGB191   RGB127   RGB63     RGB31          RGB15    RGB5*/
{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -20, -20, -16, -6, 0, 3, 0, 0, 0},
{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -19, -16, -10, -20, -6, 1, 0, 0, 0},
{0, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -11, -9 , -6, -6 , -4, -1, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -5, -5, -4, -3, -3, -3, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -4, -7, -3, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, -3, -3, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, -1, -1, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 1, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, -1, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, -1, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, -1, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, -1, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 1, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 1, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 1, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static void gamma_init_evt1_third(
				struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E8FA_TABLE_MAX] = {-1, };
	int bl_index[S6E8FA_TABLE_MAX] = {-1, };

	long long temp_cal_data = 0;
	int bl_level;

	int level_255_temp_MSB = 0;
	int level_V255 = 0;

	int point_index;
	int cnt;
	int table_index;

	/*calculate candela level */
	bl_level = pSmart->brightness_level;

	for (cnt = 0; cnt < S6E8FA_TABLE_MAX; cnt++) {
		point_index = S6E8FA_ARRAY[cnt+1];
		temp_cal_data =
		((long long)(candela_coeff_2p2[point_index])) *
		((long long)(bl_level));
		candela_level[cnt] = temp_cal_data;
	}

#ifdef SMART_DIMMING_DEBUG
	pr_info("\n candela_1:%llu  candela_5:%llu  candela_15:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	pr_info("candela_31:%llu  candela_63:%llu  candela_127:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	pr_info("candela_191:%llu ", candela_level[6]);
	pr_info("candela_255:%llu brightness_level %d\n",
		candela_level[7], pSmart->brightness_level);
#endif

	for (cnt = 0; cnt < S6E8FA_TABLE_MAX; cnt++) {
		if (searching_function(candela_level[cnt],
			&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
			pr_info("%s searching functioin error cnt:%d\n",
			__func__, cnt);
		}
	}

	/*
	*	Candela compensation
	*/
	for (cnt = 1; cnt < S6E8FA_TABLE_MAX; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail candela table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		bl_index[S6E8FA_TABLE_MAX - cnt] +=
			gradation_offset[table_index][cnt - 1];
	}

#ifdef SMART_DIMMING_DEBUG
	pr_info("\n bl_index_1:%d  bl_index_5:%d  bl_index_15:%d",
		bl_index[0], bl_index[1], bl_index[2]);
	pr_info("bl_index_31:%d bl_index_63:%d  bl_index_127:%d",
		bl_index[3], bl_index[4], bl_index[5]);
	pr_info("bl_index_191:%d bl_index_255:%d\n",
		bl_index[6], bl_index[7]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E8FA_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);

	/*
	*	RGB compensation
	*/
	for (cnt = 0; cnt < RGB_COMPENSATION; cnt++) {
		table_index = find_cadela_table(pSmart->brightness_level);

		if (table_index == -1) {
			table_index = CCG6_MAX_TABLE;
			pr_info("%s fail RGB table_index cnt : %d brightness %d",
				__func__, cnt, pSmart->brightness_level);
		}

		if (cnt < 3) {
			level_V255 = str[cnt * 2] << 8 | str[(cnt * 2) + 1];
			level_V255 +=
				rgb_offset_evt1_third[table_index][cnt];
			level_255_temp_MSB = level_V255 / 256;

			str[cnt * 2] = level_255_temp_MSB & 0xff;
			str[(cnt * 2) + 1] = level_V255 & 0xff;
		} else {
			str[cnt+3] += rgb_offset_evt1_third[table_index][cnt];
		}
	}
}

#endif

static void pure_gamma_init(struct SMART_DIM *pSmart, char *str, int size)
{
	long long candela_level[S6E8FA_TABLE_MAX] = {-1, };
	int bl_index[S6E8FA_TABLE_MAX] = {-1, };

	long long temp_cal_data = 0;
	int bl_level, cnt;
	int point_index;

	bl_level = pSmart->brightness_level;

	for (cnt = 0; cnt < S6E8FA_TABLE_MAX; cnt++) {
			point_index = S6E8FA_ARRAY[cnt+1];
			temp_cal_data =
			((long long)(candela_coeff_2p2[point_index])) *
			((long long)(bl_level));
			candela_level[cnt] = temp_cal_data;
	}

#ifdef SMART_DIMMING_DEBUG
	pr_info("\n candela_1:%llu  candela_5:%llu  candela_15:%llu ",
		candela_level[0], candela_level[1], candela_level[2]);
	pr_info("candela_31:%llu  candela_63:%llu  candela_127:%llu ",
		candela_level[3], candela_level[4], candela_level[5]);
	pr_info("candela_191:%llu candela_255:%llu\n",
		candela_level[6], candela_level[7]);

#endif
	/*calculate brightness level*/
	for (cnt = 0; cnt < S6E8FA_TABLE_MAX; cnt++) {
			if (searching_function(candela_level[cnt],
				&(bl_index[cnt]), GAMMA_CURVE_2P2)) {
				pr_info("%s searching functioin error cnt:%d\n",
					__func__, cnt);
			}
	}

#ifdef SMART_DIMMING_DEBUG
	pr_info("\n bl_index_1:%d  bl_index_5:%d  bl_index_15:%d ",
		bl_index[0], bl_index[1], bl_index[2]);
	pr_info("bl_index_31:%d bl_index_63:%d  bl_index_127:%d ",
		bl_index[3], bl_index[4], bl_index[5]);
	pr_info("bl_index_191:%d bl_index_255:%d\n",
		bl_index[6], bl_index[7]);
#endif
	/*Generate Gamma table*/
	for (cnt = 0; cnt < S6E8FA_TABLE_MAX; cnt++)
		(void)Make_hexa[cnt](bl_index , pSmart, str);
}


static void set_max_lux_table(void)
{
	max_lux_table[21] = V255_300CD_R_MSB;
	max_lux_table[22] = V255_300CD_R_LSB;

	max_lux_table[23] = V255_300CD_G_MSB;
	max_lux_table[24] = V255_300CD_G_LSB;

	max_lux_table[25] = V255_300CD_B_MSB;
	max_lux_table[26] = V255_300CD_B_LSB;

	max_lux_table[18] = V191_300CD_R;
	max_lux_table[19] = V191_300CD_G;
	max_lux_table[20] = V191_300CD_B;

	max_lux_table[15] = V127_300CD_R;
	max_lux_table[16] = V127_300CD_G;
	max_lux_table[17] = V127_300CD_B;

	max_lux_table[12] = V63_300CD_R;
	max_lux_table[13] = V63_300CD_G;
	max_lux_table[14] = V63_300CD_B;

	max_lux_table[9] = V31_300CD_R;
	max_lux_table[10] = V31_300CD_G;
	max_lux_table[11] = V31_300CD_B;

	max_lux_table[6] = V15_300CD_R;
	max_lux_table[7] = V15_300CD_G;
	max_lux_table[8] = V15_300CD_B;

	max_lux_table[3] = V5_300CD_R;
	max_lux_table[4] = V5_300CD_G;
	max_lux_table[5] = V5_300CD_B;

	max_lux_table[0] = VT_300CD_R;
	max_lux_table[1] = VT_300CD_G;
	max_lux_table[2] = VT_300CD_B;
}


static void set_min_lux_table(struct SMART_DIM *psmart)
{
	psmart->brightness_level = MIN_CANDELA;
	pure_gamma_init(psmart, min_lux_table, GAMMA_SET_MAX);
}

static void get_min_lux_table(char *str, int size)
{
	memcpy(str, min_lux_table, size);
}

static void generate_gamma(struct SMART_DIM *psmart, char *str, int size)
{
	int lux_loop;
	struct illuminance_table *ptable = (struct illuminance_table *)
						(&(psmart->gen_table));

	/* searching already generated gamma table */
	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		if (ptable[lux_loop].lux == psmart->brightness_level) {
			memcpy(str, &(ptable[lux_loop].gamma_setting), size);
			break;
		}
	}

	/* searching fail... Setting 300CD value on gamma table */
	if (lux_loop == psmart->lux_table_max) {
		pr_info("%s searching fail lux : %d\n", __func__,
				psmart->brightness_level);
		memcpy(str, max_lux_table, size);
	}

#ifdef SMART_DIMMING_DEBUG
	if (lux_loop != psmart->lux_table_max)
		pr_info("%s searching ok index : %d lux : %d", __func__,
			lux_loop, ptable[lux_loop].lux);
#endif
}
static void gamma_cell_determine(struct SMART_DIM *psmart, int ldi_revision)
{
	char *pdest;
	pdest = (char *)&(psmart->MTP);
	pr_info("%s ldi_revision: 0x%x", __func__, ldi_revision);

	V255_300CD_R_MSB = pdest[0];
	V255_300CD_R_LSB = pdest[1];

	V255_300CD_G_MSB = pdest[9];
	V255_300CD_G_LSB = pdest[10];

	V255_300CD_B_MSB = pdest[18];
	V255_300CD_B_LSB = pdest[19];

	V191_300CD_R = pdest[2];
	V191_300CD_G = pdest[11];
	V191_300CD_B = pdest[20];

	V127_300CD_R = pdest[3];
	V127_300CD_G = pdest[12];
	V127_300CD_B = pdest[21];

	V63_300CD_R = pdest[4];
	V63_300CD_G = pdest[13];
	V63_300CD_B = pdest[22];

	V31_300CD_R = pdest[5];
	V31_300CD_G = pdest[14];
	V31_300CD_B = pdest[23];

	V15_300CD_R = pdest[6];
	V15_300CD_G = pdest[15];
	V15_300CD_B = pdest[24];

	V5_300CD_R = pdest[7];
	V5_300CD_G = pdest[16];
	V5_300CD_B = pdest[25];

	VT_300CD_R = pdest[8];
	VT_300CD_G = pdest[17];
	VT_300CD_B = pdest[26];
}

static void mtp_sorting(struct SMART_DIM *psmart)
{
	int sorting_clock[27] = {
		21, 22, 18, 15, 12, 9, 6, 3, 0, /* R */
		23, 24, 19, 16, 13, 10, 7, 4, 1, /* G */
		25, 26, 20, 17, 14, 11, 8, 5, 2, /* B */
	};
	int loop;
	char *pfrom, *pdest;

	pfrom = (char *)&(psmart->MTP_ORIGN);
	pdest = (char *)&(psmart->MTP);

	for (loop = 0; loop < GAMMA_SET_MAX; loop++)
		pdest[loop] = pfrom[sorting_clock[loop]];
}

static int smart_dimming_init(struct SMART_DIM *psmart)
{
	int lux_loop;
	int id1, id2, id3;
#ifdef SMART_DIMMING_DEBUG
	int cnt;
	char pBuffer[256];
	memset(pBuffer, 0x00, 256);
#endif
	id1 = (psmart->ldi_revision & 0x00FF0000) >> 16;
	id2 = (psmart->ldi_revision & 0x0000FF00) >> 8;
	id3 = psmart->ldi_revision & 0xFF;

	mtp_sorting(psmart);
	gamma_cell_determine(psmart, psmart->ldi_revision);
	set_max_lux_table();

#ifdef SMART_DIMMING_DEBUG
	print_RGB_offset(psmart);
#endif

	v255_adjustment(psmart);
	vt_adjustment(psmart);
	v191_adjustment(psmart);
	v127_adjustment(psmart);
	v63_adjustment(psmart);
	v31_adjustment(psmart);
	v15_adjustment(psmart);
	v5_adjustment(psmart);


	if (generate_gray_scale(psmart)) {
		pr_info(KERN_ERR "lcd smart dimming fail generate_gray_scale\n");
		return -EINVAL;
	}

	/*Generating lux_table*/
	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		/* To set brightness value */
		psmart->brightness_level = psmart->plux_table[lux_loop];
		/* To make lux table index*/
		psmart->gen_table[lux_loop].lux = psmart->plux_table[lux_loop];
#if defined(AID_OPERATION)
		gamma_init_evt1_third(psmart,
			(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX);
#else
		pure_gamma_init(psmart,
			(char *)(&(psmart->gen_table[lux_loop].gamma_setting)),
			GAMMA_SET_MAX);
#endif
	}

	/* set 300CD max gamma table */
	memcpy(&(psmart->gen_table[lux_loop-1].gamma_setting),
			max_lux_table, GAMMA_SET_MAX);

	set_min_lux_table(psmart);

#ifdef SMART_DIMMING_DEBUG
/*	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256, " %d",
				psmart->gen_table[lux_loop].gamma_setting[cnt]);

		pr_info("lux : %d  %s", psmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}*/

	for (lux_loop = 0; lux_loop < psmart->lux_table_max; lux_loop++) {
		for (cnt = 0; cnt < GAMMA_SET_MAX; cnt++)
			snprintf(pBuffer + strnlen(pBuffer, 256), 256,
				" %02X",
				psmart->gen_table[lux_loop].gamma_setting[cnt]);

		pr_info("lux : %d  %s", psmart->plux_table[lux_loop], pBuffer);
		memset(pBuffer, 0x00, 256);
	}
#endif
	return 0;
}

/* ----------------------------------------------------------------------------
 * Wrapper functions for smart dimming to work with 8974 generic code
 * ----------------------------------------------------------------------------
 */

static struct SMART_DIM	smart_S6E63J;
static struct smartdim_conf	__S6E63J__;

static void wrap_generate_gamma(int cd, char *cmd_str)
{
	smart_S6E63J.brightness_level = cd;
	generate_gamma(&smart_S6E63J, cmd_str, GAMMA_SET_MAX);
}

static void wrap_smart_dimming_init(void)
{
	smart_S6E63J.plux_table = __S6E63J__.lux_tab;
	smart_S6E63J.lux_table_max = __S6E63J__.lux_tabsize;
	smart_S6E63J.ldi_revision = __S6E63J__.man_id;
	smart_dimming_init(&smart_S6E63J);
}

struct smartdim_conf *smart_S6E63J_get_conf(void)
{
	__S6E63J__.generate_gamma = wrap_generate_gamma;
	__S6E63J__.init = wrap_smart_dimming_init;
	__S6E63J__.get_min_lux_table = get_min_lux_table;
	__S6E63J__.mtp_buffer = (char *)(&smart_S6E63J.MTP_ORIGN);
	return (struct smartdim_conf *)&__S6E63J__;
}

/* ----------------------------------------------------------------------------
 * END - Wrapper
 * ----------------------------------------------------------------------------
 */

