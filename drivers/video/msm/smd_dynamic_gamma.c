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

#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include "smd_dynamic_gamma.h"

/* #define VERBOSE_LOGGING */

#ifdef VERBOSE_LOGGING
static int debug_print = 1;
#else
static int debug_print;
#endif

/* Voltage point index values */
#define V1   0
#define V15  1
#define V35  2
#define V59  3
#define V87  4
#define V171 5
#define V255 6

#define RED   0
#define GREEN 1
#define BLUE  2

#define NUM_GRAY_LVL_RANGE 6

static uint8_t gray_lvl_index[NUM_NIT_LVLS][NUM_VOLT_PTS] = {
	{1, 1, 3, 4, 6, 13, 19},	 /* 1 nit(s) */
	{1, 1, 4, 6, 9, 18, 26},	 /* 2 nit(s) */
	{1, 2, 4, 7, 11, 21, 31},	 /* 3 nit(s) */
	{1, 2, 5, 8, 12, 24, 36},	 /* 4 nit(s) */
	{1, 2, 5, 9, 14, 27, 40},	 /* 5 nit(s) */
	{1, 2, 6, 10, 15, 29, 43},	 /* 6 nit(s) */
	{1, 3, 6, 11, 16, 31, 46},	 /* 7 nit(s) */
	{1, 3, 7, 11, 17, 33, 49},	 /* 8 nit(s) */
	{1, 3, 7, 12, 18, 35, 52},	 /* 9 nit(s) */
	{1, 3, 7, 13, 19, 36, 54},	 /* 10 nit(s) */
	{1, 3, 8, 13, 19, 38, 57},	 /* 11 nit(s) */
	{1, 3, 8, 14, 20, 40, 59},	 /* 12 nit(s) */
	{1, 4, 8, 14, 21, 41, 61},	 /* 13 nit(s) */
	{1, 4, 9, 15, 22, 42, 63},	 /* 14 nit(s) */
	{1, 4, 9, 15, 22, 44, 65},	 /* 15 nit(s) */
	{1, 4, 9, 16, 23, 45, 67},	 /* 16 nit(s) */
	{1, 4, 9, 16, 24, 46, 69},	 /* 17 nit(s) */
	{1, 4, 10, 16, 24, 48, 71},	 /* 18 nit(s) */
	{1, 4, 10, 17, 25, 49, 73},	 /* 19 nit(s) */
	{1, 4, 10, 17, 25, 50, 74},	 /* 20 nit(s) */
	{1, 4, 10, 18, 26, 51, 76},	 /* 21 nit(s) */
	{1, 5, 11, 18, 27, 52, 78},	 /* 22 nit(s) */
	{1, 5, 11, 18, 27, 53, 79},	 /* 23 nit(s) */
	{1, 5, 11, 19, 28, 54, 81},	 /* 24 nit(s) */
	{1, 5, 11, 19, 28, 55, 82},	 /* 25 nit(s) */
	{1, 5, 12, 19, 29, 56, 84},	 /* 26 nit(s) */
	{1, 5, 12, 20, 29, 57, 85},	 /* 27 nit(s) */
	{1, 5, 12, 20, 30, 58, 87},	 /* 28 nit(s) */
	{1, 5, 12, 20, 30, 59, 88},	 /* 29 nit(s) */
	{1, 5, 12, 21, 31, 60, 90},	 /* 30 nit(s) */
	{1, 5, 12, 21, 31, 61, 91},	 /* 31 nit(s) */
	{1, 5, 13, 21, 31, 62, 92},	 /* 32 nit(s) */
	{1, 5, 13, 22, 32, 63, 93},	 /* 33 nit(s) */
	{1, 6, 13, 22, 32, 64, 95},	 /* 34 nit(s) */
	{1, 6, 13, 22, 33, 64, 96},	 /* 35 nit(s) */
	{1, 6, 13, 22, 33, 65, 97},	 /* 36 nit(s) */
	{1, 6, 14, 23, 34, 66, 98},	 /* 37 nit(s) */
	{1, 6, 14, 23, 34, 67, 100},	 /* 38 nit(s) */
	{1, 6, 14, 23, 34, 68, 101},	 /* 39 nit(s) */
	{1, 6, 14, 24, 35, 68, 102},	 /* 40 nit(s) */
	{1, 6, 14, 24, 35, 69, 103},	 /* 41 nit(s) */
	{1, 6, 14, 24, 36, 70, 104},	 /* 42 nit(s) */
	{1, 6, 14, 24, 36, 71, 105},	 /* 43 nit(s) */
	{1, 6, 15, 25, 36, 71, 107},	 /* 44 nit(s) */
	{1, 6, 15, 25, 37, 72, 108},	 /* 45 nit(s) */
	{1, 6, 15, 25, 37, 73, 109},	 /* 46 nit(s) */
	{1, 6, 15, 25, 37, 74, 110},	 /* 47 nit(s) */
	{1, 6, 15, 26, 38, 74, 111},	 /* 48 nit(s) */
	{1, 7, 15, 26, 38, 75, 112},	 /* 49 nit(s) */
	{1, 7, 15, 26, 39, 76, 113},	 /* 50 nit(s) */
	{1, 7, 16, 26, 39, 76, 114},	 /* 51 nit(s) */
	{1, 7, 16, 27, 39, 77, 115},	 /* 52 nit(s) */
	{1, 7, 16, 27, 40, 78, 116},	 /* 53 nit(s) */
	{1, 7, 16, 27, 40, 78, 117},	 /* 54 nit(s) */
	{1, 7, 16, 27, 40, 79, 118},	 /* 55 nit(s) */
	{1, 7, 16, 28, 41, 80, 119},	 /* 56 nit(s) */
	{1, 7, 16, 28, 41, 80, 120},	 /* 57 nit(s) */
	{1, 7, 17, 28, 41, 81, 121},	 /* 58 nit(s) */
	{1, 7, 17, 28, 42, 82, 122},	 /* 59 nit(s) */
	{1, 7, 17, 28, 42, 82, 123},	 /* 60 nit(s) */
	{1, 7, 17, 29, 42, 83, 124},	 /* 61 nit(s) */
	{1, 7, 17, 29, 42, 84, 125},	 /* 62 nit(s) */
	{1, 7, 17, 29, 43, 84, 125},	 /* 63 nit(s) */
	{1, 7, 17, 29, 43, 85, 126},	 /* 64 nit(s) */
	{1, 7, 17, 29, 43, 85, 127},	 /* 65 nit(s) */
	{1, 8, 18, 30, 44, 86, 128},	 /* 66 nit(s) */
	{1, 8, 18, 30, 44, 87, 129},	 /* 67 nit(s) */
	{1, 8, 18, 30, 44, 87, 130},	 /* 68 nit(s) */
	{1, 8, 18, 30, 45, 88, 131},	 /* 69 nit(s) */
	{1, 8, 18, 30, 45, 88, 132},	 /* 70 nit(s) */
	{1, 8, 18, 31, 45, 89, 132},	 /* 71 nit(s) */
	{1, 8, 18, 31, 45, 89, 133},	 /* 72 nit(s) */
	{1, 8, 18, 31, 46, 90, 134},	 /* 73 nit(s) */
	{1, 8, 19, 31, 46, 91, 135},	 /* 74 nit(s) */
	{1, 8, 19, 31, 46, 91, 136},	 /* 75 nit(s) */
	{1, 8, 19, 32, 47, 92, 137},	 /* 76 nit(s) */
	{1, 8, 19, 32, 47, 92, 137},	 /* 77 nit(s) */
	{1, 8, 19, 32, 47, 93, 138},	 /* 78 nit(s) */
	{1, 8, 19, 32, 47, 93, 139},	 /* 79 nit(s) */
	{1, 8, 19, 32, 48, 94, 140},	 /* 80 nit(s) */
	{1, 8, 19, 33, 48, 94, 141},	 /* 81 nit(s) */
	{1, 8, 19, 33, 48, 95, 141},	 /* 82 nit(s) */
	{1, 8, 20, 33, 49, 95, 142},	 /* 83 nit(s) */
	{1, 8, 20, 33, 49, 96, 143},	 /* 84 nit(s) */
	{1, 8, 20, 33, 49, 96, 144},	 /* 85 nit(s) */
	{1, 8, 20, 33, 49, 97, 145},	 /* 86 nit(s) */
	{1, 9, 20, 34, 50, 97, 145},	 /* 87 nit(s) */
	{1, 9, 20, 34, 50, 98, 146},	 /* 88 nit(s) */
	{1, 9, 20, 34, 50, 98, 147},	 /* 89 nit(s) */
	{1, 9, 20, 34, 50, 99, 148},	 /* 90 nit(s) */
	{1, 9, 20, 34, 51, 99, 148},	 /* 91 nit(s) */
	{1, 9, 20, 34, 51, 100, 149},	 /* 92 nit(s) */
	{1, 9, 21, 35, 51, 100, 150},	 /* 93 nit(s) */
	{1, 9, 21, 35, 51, 101, 150},	 /* 94 nit(s) */
	{1, 9, 21, 35, 52, 101, 151},	 /* 95 nit(s) */
	{1, 9, 21, 35, 52, 102, 152},	 /* 96 nit(s) */
	{1, 9, 21, 35, 52, 102, 153},	 /* 97 nit(s) */
	{1, 9, 21, 35, 52, 103, 153},	 /* 98 nit(s) */
	{1, 9, 21, 36, 53, 103, 154},	 /* 99 nit(s) */
	{1, 9, 21, 36, 53, 104, 155},	 /* 100 nit(s) */
	{1, 9, 21, 36, 53, 104, 155},	 /* 101 nit(s) */
	{1, 9, 21, 36, 53, 105, 156},	 /* 102 nit(s) */
	{1, 9, 22, 36, 54, 105, 157},	 /* 103 nit(s) */
	{1, 9, 22, 36, 54, 106, 158},	 /* 104 nit(s) */
	{1, 9, 22, 37, 54, 106, 158},	 /* 105 nit(s) */
	{1, 9, 22, 37, 54, 107, 159},	 /* 106 nit(s) */
	{1, 9, 22, 37, 54, 107, 160},	 /* 107 nit(s) */
	{1, 9, 22, 37, 55, 107, 160},	 /* 108 nit(s) */
	{1, 9, 22, 37, 55, 108, 161},	 /* 109 nit(s) */
	{1, 9, 22, 37, 55, 108, 162},	 /* 110 nit(s) */
	{1, 10, 22, 38, 55, 109, 162},	 /* 111 nit(s) */
	{1, 10, 22, 38, 56, 109, 163},	 /* 112 nit(s) */
	{1, 10, 22, 38, 56, 110, 164},	 /* 113 nit(s) */
	{1, 10, 23, 38, 56, 110, 164},	 /* 114 nit(s) */
	{1, 10, 23, 38, 56, 111, 165},	 /* 115 nit(s) */
	{1, 10, 23, 38, 56, 111, 166},	 /* 116 nit(s) */
	{1, 10, 23, 38, 57, 111, 166},	 /* 117 nit(s) */
	{1, 10, 23, 39, 57, 112, 167},	 /* 118 nit(s) */
	{1, 10, 23, 39, 57, 112, 167},	 /* 119 nit(s) */
	{1, 10, 23, 39, 57, 113, 168},	 /* 120 nit(s) */
	{1, 10, 23, 39, 58, 113, 169},	 /* 121 nit(s) */
	{1, 10, 23, 39, 58, 114, 169},	 /* 122 nit(s) */
	{1, 10, 23, 39, 58, 114, 170},	 /* 123 nit(s) */
	{1, 10, 23, 39, 58, 114, 171},	 /* 124 nit(s) */
	{1, 10, 24, 40, 58, 115, 171},	 /* 125 nit(s) */
	{1, 10, 24, 40, 59, 115, 172},	 /* 126 nit(s) */
	{1, 10, 24, 40, 59, 116, 173},	 /* 127 nit(s) */
	{1, 10, 24, 40, 59, 116, 173},	 /* 128 nit(s) */
	{1, 10, 24, 40, 59, 117, 174},	 /* 129 nit(s) */
	{1, 10, 24, 40, 59, 117, 174},	 /* 130 nit(s) */
	{1, 10, 24, 40, 60, 117, 175},	 /* 131 nit(s) */
	{1, 10, 24, 41, 60, 118, 176},	 /* 132 nit(s) */
	{1, 10, 24, 41, 60, 118, 176},	 /* 133 nit(s) */
	{1, 10, 24, 41, 60, 119, 177},	 /* 134 nit(s) */
	{1, 10, 24, 41, 61, 119, 177},	 /* 135 nit(s) */
	{1, 10, 24, 41, 61, 119, 178},	 /* 136 nit(s) */
	{1, 10, 25, 41, 61, 120, 179},	 /* 137 nit(s) */
	{1, 11, 25, 41, 61, 120, 179},	 /* 138 nit(s) */
	{1, 11, 25, 42, 61, 121, 180},	 /* 139 nit(s) */
	{1, 11, 25, 42, 62, 121, 180},	 /* 140 nit(s) */
	{1, 11, 25, 42, 62, 121, 181},	 /* 141 nit(s) */
	{1, 11, 25, 42, 62, 122, 182},	 /* 142 nit(s) */
	{1, 11, 25, 42, 62, 122, 182},	 /* 143 nit(s) */
	{1, 11, 25, 42, 62, 122, 183},	 /* 144 nit(s) */
	{1, 11, 25, 42, 63, 123, 183},	 /* 145 nit(s) */
	{1, 11, 25, 43, 63, 123, 184},	 /* 146 nit(s) */
	{1, 11, 25, 43, 63, 124, 184},	 /* 147 nit(s) */
	{1, 11, 25, 43, 63, 124, 185},	 /* 148 nit(s) */
	{1, 11, 25, 43, 63, 124, 186},	 /* 149 nit(s) */
	{1, 11, 26, 43, 63, 125, 186},	 /* 150 nit(s) */
	{1, 11, 26, 43, 64, 125, 187},	 /* 151 nit(s) */
	{1, 11, 26, 43, 64, 126, 187},	 /* 152 nit(s) */
	{1, 11, 26, 43, 64, 126, 188},	 /* 153 nit(s) */
	{1, 11, 26, 44, 64, 126, 188},	 /* 154 nit(s) */
	{1, 11, 26, 44, 64, 127, 189},	 /* 155 nit(s) */
	{1, 11, 26, 44, 65, 127, 189},	 /* 156 nit(s) */
	{1, 11, 26, 44, 65, 127, 190},	 /* 157 nit(s) */
	{1, 11, 26, 44, 65, 128, 191},	 /* 158 nit(s) */
	{1, 11, 26, 44, 65, 128, 191},	 /* 159 nit(s) */
	{1, 11, 26, 44, 65, 128, 192},	 /* 160 nit(s) */
	{1, 11, 26, 44, 66, 129, 192},	 /* 161 nit(s) */
	{1, 11, 26, 45, 66, 129, 193},	 /* 162 nit(s) */
	{1, 11, 27, 45, 66, 130, 193},	 /* 163 nit(s) */
	{1, 11, 27, 45, 66, 130, 194},	 /* 164 nit(s) */
	{1, 11, 27, 45, 66, 130, 194},	 /* 165 nit(s) */
	{1, 11, 27, 45, 66, 131, 195},	 /* 166 nit(s) */
	{1, 11, 27, 45, 67, 131, 195},	 /* 167 nit(s) */
	{1, 12, 27, 45, 67, 131, 196},	 /* 168 nit(s) */
	{1, 12, 27, 45, 67, 132, 196},	 /* 169 nit(s) */
	{1, 12, 27, 46, 67, 132, 197},	 /* 170 nit(s) */
	{1, 12, 27, 46, 67, 132, 198},	 /* 171 nit(s) */
	{1, 12, 27, 46, 68, 133, 198},	 /* 172 nit(s) */
	{1, 12, 27, 46, 68, 133, 199},	 /* 173 nit(s) */
	{1, 12, 27, 46, 68, 133, 199},	 /* 174 nit(s) */
	{1, 12, 27, 46, 68, 134, 200},	 /* 175 nit(s) */
	{1, 12, 27, 46, 68, 134, 200},	 /* 176 nit(s) */
	{1, 12, 28, 46, 68, 135, 201},	 /* 177 nit(s) */
	{1, 12, 28, 47, 69, 135, 201},	 /* 178 nit(s) */
	{1, 12, 28, 47, 69, 135, 202},	 /* 179 nit(s) */
	{1, 12, 28, 47, 69, 136, 202},	 /* 180 nit(s) */
	{1, 12, 28, 47, 69, 136, 203},	 /* 181 nit(s) */
	{1, 12, 28, 47, 69, 136, 203},	 /* 182 nit(s) */
	{1, 12, 28, 47, 69, 137, 204},	 /* 183 nit(s) */
	{1, 12, 28, 47, 70, 137, 204},	 /* 184 nit(s) */
	{1, 12, 28, 47, 70, 137, 205},	 /* 185 nit(s) */
	{1, 12, 28, 47, 70, 138, 205},	 /* 186 nit(s) */
	{1, 12, 28, 48, 70, 138, 206},	 /* 187 nit(s) */
	{1, 12, 28, 48, 70, 138, 206},	 /* 188 nit(s) */
	{1, 12, 28, 48, 71, 139, 207},	 /* 189 nit(s) */
	{1, 12, 28, 48, 71, 139, 207},	 /* 190 nit(s) */
	{1, 12, 29, 48, 71, 139, 208},	 /* 191 nit(s) */
	{1, 12, 29, 48, 71, 140, 208},	 /* 192 nit(s) */
	{1, 12, 29, 48, 71, 140, 209},	 /* 193 nit(s) */
	{1, 12, 29, 48, 71, 140, 209},	 /* 194 nit(s) */
	{1, 12, 29, 49, 72, 141, 210},	 /* 195 nit(s) */
	{1, 12, 29, 49, 72, 141, 210},	 /* 196 nit(s) */
	{1, 12, 29, 49, 72, 141, 211},	 /* 197 nit(s) */
	{1, 12, 29, 49, 72, 142, 211},	 /* 198 nit(s) */
	{1, 12, 29, 49, 72, 142, 212},	 /* 199 nit(s) */
	{1, 12, 29, 49, 72, 142, 212},	 /* 200 nit(s) */
	{1, 12, 29, 49, 73, 143, 213},	 /* 201 nit(s) */
	{1, 13, 29, 49, 73, 143, 213},	 /* 202 nit(s) */
	{1, 13, 29, 49, 73, 143, 214},	 /* 203 nit(s) */
	{1, 13, 29, 50, 73, 144, 214},	 /* 204 nit(s) */
	{1, 13, 29, 50, 73, 144, 214},	 /* 205 nit(s) */
	{1, 13, 29, 50, 73, 144, 215},	 /* 206 nit(s) */
	{1, 13, 30, 50, 73, 144, 215},	 /* 207 nit(s) */
	{1, 13, 30, 50, 74, 145, 216},	 /* 208 nit(s) */
	{1, 13, 30, 50, 74, 145, 216},	 /* 209 nit(s) */
	{1, 13, 30, 50, 74, 145, 217},	 /* 210 nit(s) */
	{1, 13, 30, 50, 74, 146, 217},	 /* 211 nit(s) */
	{1, 13, 30, 50, 74, 146, 218},	 /* 212 nit(s) */
	{1, 13, 30, 50, 74, 146, 218},	 /* 213 nit(s) */
	{1, 13, 30, 51, 75, 147, 219},	 /* 214 nit(s) */
	{1, 13, 30, 51, 75, 147, 219},	 /* 215 nit(s) */
	{1, 13, 30, 51, 75, 147, 220},	 /* 216 nit(s) */
	{1, 13, 30, 51, 75, 148, 220},	 /* 217 nit(s) */
	{1, 13, 30, 51, 75, 148, 221},	 /* 218 nit(s) */
	{1, 13, 30, 51, 75, 148, 221},	 /* 219 nit(s) */
	{1, 13, 30, 51, 76, 149, 221},	 /* 220 nit(s) */
	{1, 13, 30, 51, 76, 149, 222},	 /* 221 nit(s) */
	{1, 13, 31, 51, 76, 149, 222},	 /* 222 nit(s) */
	{1, 13, 31, 52, 76, 149, 223},	 /* 223 nit(s) */
	{1, 13, 31, 52, 76, 150, 223},	 /* 224 nit(s) */
	{1, 13, 31, 52, 76, 150, 224},	 /* 225 nit(s) */
	{1, 13, 31, 52, 76, 150, 224},	 /* 226 nit(s) */
	{1, 13, 31, 52, 77, 151, 225},	 /* 227 nit(s) */
	{1, 13, 31, 52, 77, 151, 225},	 /* 228 nit(s) */
	{1, 13, 31, 52, 77, 151, 226},	 /* 229 nit(s) */
	{1, 13, 31, 52, 77, 152, 226},	 /* 230 nit(s) */
	{1, 13, 31, 52, 77, 152, 226},	 /* 231 nit(s) */
	{1, 13, 31, 52, 77, 152, 227},	 /* 232 nit(s) */
	{1, 13, 31, 53, 78, 152, 227},	 /* 233 nit(s) */
	{1, 13, 31, 53, 78, 153, 228},	 /* 234 nit(s) */
	{1, 13, 31, 53, 78, 153, 228},	 /* 235 nit(s) */
	{1, 13, 31, 53, 78, 153, 229},	 /* 236 nit(s) */
	{1, 13, 31, 53, 78, 154, 229},	 /* 237 nit(s) */
	{1, 13, 31, 53, 78, 154, 230},	 /* 238 nit(s) */
	{1, 14, 32, 53, 78, 154, 230},	 /* 239 nit(s) */
	{1, 14, 32, 53, 79, 155, 230},	 /* 240 nit(s) */
	{1, 14, 32, 53, 79, 155, 231},	 /* 241 nit(s) */
	{1, 14, 32, 54, 79, 155, 231},	 /* 242 nit(s) */
	{1, 14, 32, 54, 79, 155, 232},	 /* 243 nit(s) */
	{1, 14, 32, 54, 79, 156, 232},	 /* 244 nit(s) */
	{1, 14, 32, 54, 79, 156, 233},	 /* 245 nit(s) */
	{1, 14, 32, 54, 79, 156, 233},	 /* 246 nit(s) */
	{1, 14, 32, 54, 80, 157, 233},	 /* 247 nit(s) */
	{1, 14, 32, 54, 80, 157, 234},	 /* 248 nit(s) */
	{1, 14, 32, 54, 80, 157, 234},	 /* 249 nit(s) */
	{1, 14, 32, 54, 80, 157, 235},	 /* 250 nit(s) */
	{1, 14, 32, 54, 80, 158, 235},	 /* 251 nit(s) */
	{1, 14, 32, 55, 80, 158, 236},	 /* 252 nit(s) */
	{1, 14, 32, 55, 81, 158, 236},	 /* 253 nit(s) */
	{1, 14, 32, 55, 81, 159, 236},	 /* 254 nit(s) */
	{1, 14, 33, 55, 81, 159, 237},	 /* 255 nit(s) */
	{1, 14, 33, 55, 81, 159, 237},	 /* 256 nit(s) */
	{1, 14, 33, 55, 81, 159, 238},	 /* 257 nit(s) */
	{1, 14, 33, 55, 81, 160, 238},	 /* 258 nit(s) */
	{1, 14, 33, 55, 81, 160, 239},	 /* 259 nit(s) */
	{1, 14, 33, 55, 82, 160, 239},	 /* 260 nit(s) */
	{1, 14, 33, 55, 82, 161, 239},	 /* 261 nit(s) */
	{1, 14, 33, 55, 82, 161, 240},	 /* 262 nit(s) */
	{1, 14, 33, 56, 82, 161, 240},	 /* 263 nit(s) */
	{1, 14, 33, 56, 82, 161, 241},	 /* 264 nit(s) */
	{1, 14, 33, 56, 82, 162, 241},	 /* 265 nit(s) */
	{1, 14, 33, 56, 82, 162, 241},	 /* 266 nit(s) */
	{1, 14, 33, 56, 83, 162, 242},	 /* 267 nit(s) */
	{1, 14, 33, 56, 83, 162, 242},	 /* 268 nit(s) */
	{1, 14, 33, 56, 83, 163, 243},	 /* 269 nit(s) */
	{1, 14, 33, 56, 83, 163, 243},	 /* 270 nit(s) */
	{1, 14, 33, 56, 83, 163, 243},	 /* 271 nit(s) */
	{1, 14, 33, 56, 83, 164, 244},	 /* 272 nit(s) */
	{1, 14, 34, 57, 83, 164, 244},	 /* 273 nit(s) */
	{1, 14, 34, 57, 83, 164, 245},	 /* 274 nit(s) */
	{1, 14, 34, 57, 84, 164, 245},	 /* 275 nit(s) */
	{1, 14, 34, 57, 84, 165, 246},	 /* 276 nit(s) */
	{1, 14, 34, 57, 84, 165, 246},	 /* 277 nit(s) */
	{1, 14, 34, 57, 84, 165, 246},	 /* 278 nit(s) */
	{1, 15, 34, 57, 84, 165, 247},	 /* 279 nit(s) */
	{1, 15, 34, 57, 84, 166, 247},	 /* 280 nit(s) */
	{1, 15, 34, 57, 84, 166, 248},	 /* 281 nit(s) */
	{1, 15, 34, 57, 85, 166, 248},	 /* 282 nit(s) */
	{1, 15, 34, 57, 85, 167, 248},	 /* 283 nit(s) */
	{1, 15, 34, 58, 85, 167, 249},	 /* 284 nit(s) */
	{1, 15, 34, 58, 85, 167, 249},	 /* 285 nit(s) */
	{1, 15, 34, 58, 85, 167, 250},	 /* 286 nit(s) */
	{1, 15, 34, 58, 85, 168, 250},	 /* 287 nit(s) */
	{1, 15, 34, 58, 85, 168, 250},	 /* 288 nit(s) */
	{1, 15, 34, 58, 86, 168, 251},	 /* 289 nit(s) */
	{1, 15, 34, 58, 86, 168, 251},	 /* 290 nit(s) */
	{1, 15, 35, 58, 86, 169, 251},	 /* 291 nit(s) */
	{1, 15, 35, 58, 86, 169, 252},	 /* 292 nit(s) */
	{1, 15, 35, 58, 86, 169, 252},	 /* 293 nit(s) */
	{1, 15, 35, 58, 86, 169, 253},	 /* 294 nit(s) */
	{1, 15, 35, 59, 86, 170, 253},	 /* 295 nit(s) */
	{1, 15, 35, 59, 86, 170, 253},	 /* 296 nit(s) */
	{1, 15, 35, 59, 87, 170, 254},	 /* 297 nit(s) */
	{1, 15, 35, 59, 87, 170, 254},	 /* 298 nit(s) */
	{1, 15, 35, 59, 87, 171, 255},	 /* 299 nit(s) */
	{1, 15, 35, 59, 87, 171, 255},	 /* 300 nit(s) */
};

/* Gray level index values not to calculate, and the voltage
   point value to use */
const uint8_t gray_lvl_non_calc[][2] = {
	{1, V1},
	{15, V15},
	{35, V35},
	{59, V59},
	{87, V87},
	{171, V171},
	{255, V255}
};

/* Gray level calculation range start and end points */
const uint8_t gray_lvl_range[NUM_GRAY_LVL_RANGE][2] = {
	{2, 15},
	{16, 35},
	{36, 59},
	{60, 87},
	{88, 171},
	{172, 255}
};

/* Voltage points used to calculate gray level ranges */
const uint8_t gray_lvl_v_pt[NUM_GRAY_LVL_RANGE][2] = {
	{V1, V15},
	{V15, V35},
	{V35, V59},
	{V59, V87},
	{V87, V171},
	{V171, V255}
};

/* Denominators to use for each gray level range */
const uint8_t gray_lvl_denom[NUM_GRAY_LVL_RANGE] = {
	52, 70, 24, 28, 84, 84
};

/* Numerators to use for V1 <-> V15 rage */
const uint8_t v1_v15_numerators[] = {
	47, 42, 37, 32, 27, 23, 19, 15, 12, 9, 6, 4, 2
};

/* Numerators to use for V15 <-> V35 rage */
const uint8_t v15_v35_numerators[] = {
	66, 62, 58, 54, 50, 46, 42, 38, 34, 30, 27, 24, 21,
	18, 15, 12, 9, 6, 3
};

/* Indicates if gray level calculation uses computed or hardcode numerator */
const uint8_t * const gray_lvl_numer_ptr[NUM_GRAY_LVL_RANGE] = {
	v1_v15_numerators,
	v15_v35_numerators,
	NULL,
	NULL,
	NULL,
	NULL
};

uint32_t full_gray_lvl_volt[NUM_GRAY_LVLS][NUM_COLORS];

/* Parse raw MTP data from panel into signed values */
static void parse_raw_mtp(uint8_t raw_mtp[RAW_MTP_SIZE],
			int16_t mtp_table[NUM_VOLT_PTS][NUM_COLORS])
{
	int color;
	int volt;
	uint16_t temp;
	int16_t mtp_val;
	uint8_t *raw_ptr = raw_mtp;

	for (color = 0; color < NUM_COLORS; color++) {
		for (volt = 0; volt < NUM_VOLT_PTS; volt++) {
			if (volt != 6) {
				if (0x80 & *raw_ptr)
					mtp_val = -(((~*raw_ptr) + 1) & 0xFF);
				else
					mtp_val = *raw_ptr;

				pr_debug("Parse MTP: raw value = 0x%04x, "
					"final = %d\n",
					*raw_ptr, mtp_val);
			} else {
				temp = *raw_ptr << 8;
				raw_ptr++;
				temp |= *raw_ptr;
				if (temp & 0x100)
					mtp_val = -(((~temp) + 1) & 0x1FF);
				else
					mtp_val = temp;

				pr_debug("Parse MTP: raw value = 0x%04x, "
					"final = %d\n",
					temp, mtp_val);
			}
			raw_ptr++;
			mtp_table[volt][color] = mtp_val;
		}
	}
}

/* Applies MTP values to gamma settings */
static int apply_mtp_to_gamma(int16_t mtp_table[NUM_VOLT_PTS][NUM_COLORS],
			uint16_t pre_gamma[NUM_VOLT_PTS][NUM_COLORS],
			uint16_t post_gamma[NUM_VOLT_PTS][NUM_COLORS],
			bool add)
{
	int16_t temp;
	int color;
	int i;
	int r = 0;

	for (color = 0; color < NUM_COLORS; color++) {
		for (i = 0; i < NUM_VOLT_PTS; i++) {
			if (add)
				temp = pre_gamma[i][color] +
					mtp_table[i][color];
			else
				temp = pre_gamma[i][color] -
					mtp_table[i][color];

			if ((i != 6) && ((temp < 0) || (temp > 255))) {
				pr_err("ERROR: Converted gamma value out of "
					"range, value = %d\n",
					temp);
				r = -EINVAL;
				temp = (temp < 0) ? 0 : 255;
			} else if ((i == 6) && ((temp < 0) || (temp > 511))) {
				pr_err("ERROR: Converted gamma value out of "
					"range, value = %d\n",
					temp);
				r = -EINVAL;
				temp = (temp < 0) ? 0 : 511;
			}

			post_gamma[i][color] = (uint16_t) temp;

			pr_debug("Apply MTP: MTP = %d, 0x%04x (%d) -> 0x%04x "
				"(%d)\n",
				mtp_table[i][color], pre_gamma[i][color],
				pre_gamma[i][color], post_gamma[i][color],
				post_gamma[i][color]);
		}
	}

	return r;
}

/* Converts gamma values to voltage level points */
static void convert_gamma_to_volt_pts(uint32_t v0_val,
				uint16_t g[NUM_VOLT_PTS][NUM_COLORS],
				uint32_t v[NUM_VOLT_PTS][NUM_COLORS])
{
	int color = 0;
	int i;

	for (color = 0; color < NUM_COLORS; color++) {
		v[V1][color] = v0_val -
			DIV_ROUND_CLOSEST(((v0_val * (g[V1][color] + 5))),
					600);
		v[V255][color] = v0_val -
			DIV_ROUND_CLOSEST(((v0_val * (g[V255][color] + 100))),
					600);
		v[V171][color] = v[V1][color] -
			DIV_ROUND_CLOSEST(((v[V1][color] - v[V255][color]) *
					(g[V171][color] + 65)), 320);
		v[V87][color] = v[V1][color] -
			DIV_ROUND_CLOSEST(((v[V1][color] - v[V171][color]) *
					(g[V87][color] + 65)), 320);
		v[V59][color] = v[V1][color] -
			DIV_ROUND_CLOSEST(((v[V1][color] - v[V87][color]) *
					(g[V59][color] + 65)), 320);
		v[V35][color] = v[V1][color] -
			DIV_ROUND_CLOSEST(((v[V1][color] - v[V59][color]) *
					(g[V35][color] + 65)), 320);
		v[V15][color] = v[V1][color] -
			DIV_ROUND_CLOSEST(((v[V1][color] - v[V35][color]) *
					(g[V15][color] + 20)), 320);
	}

	if (debug_print) {
		for (i = 0; i < 7; i++) {
			pr_info("Gamma -> Voltage Point: "
				"R = %d, G = %d, B = %d\n",
				v[i][0], v[i][1], v[i][2]);
		}
	}
}

/* Calculates voltage values for all gray levels */
static void calc_gray_lvl_volt(uint32_t v0_val,
			uint32_t in_v[NUM_VOLT_PTS][NUM_COLORS],
			uint32_t gray_v[NUM_GRAY_LVLS][NUM_COLORS])
{
	int i;
	int j;
	uint8_t numer;
	uint8_t denom;
	const uint8_t *numer_ptr = NULL;
	int gray_level;
	uint32_t in_v_btm[NUM_COLORS];
	uint32_t in_v_top[NUM_COLORS];
	uint32_t in_v_diff[NUM_COLORS];


	/* Level 0 is always V0 */
	gray_v[0][RED] = v0_val;
	gray_v[0][GREEN] = v0_val;
	gray_v[0][BLUE] = v0_val;

	/* Set all of the non-calculated gray levels */
	for (i = 0; i < ARRAY_SIZE(gray_lvl_non_calc); i++) {
		gray_v[gray_lvl_non_calc[i][0]][RED] =
			in_v[gray_lvl_non_calc[i][1]][RED];
		gray_v[gray_lvl_non_calc[i][0]][GREEN] =
			in_v[gray_lvl_non_calc[i][1]][GREEN];
		gray_v[gray_lvl_non_calc[i][0]][BLUE] =
			in_v[gray_lvl_non_calc[i][1]][BLUE];
	}

	for (i = 0; i < ARRAY_SIZE(gray_lvl_range); i++) {
		denom = gray_lvl_denom[i];

		/* Determine if numerator is hardcoded or calculated */
		if (gray_lvl_numer_ptr[i] == NULL) {
			numer = denom - 1;
		} else {
			numer_ptr = gray_lvl_numer_ptr[i];
			numer = numer_ptr[0];
		}

		for (j = 0; j < NUM_COLORS; j++) {
			in_v_top[j] = in_v[gray_lvl_v_pt[i][0]][j];
			in_v_btm[j] = in_v[gray_lvl_v_pt[i][1]][j];
			in_v_diff[j] = in_v_top[j] - in_v_btm[j];
		}

		for (gray_level = gray_lvl_range[i][0];
		     gray_level < gray_lvl_range[i][1];
		     gray_level++) {

			gray_v[gray_level][RED] =
				in_v_btm[RED] +
				DIV_ROUND_CLOSEST((in_v_diff[RED] * numer),
						denom);

			gray_v[gray_level][GREEN] =
				in_v_btm[GREEN] +
				DIV_ROUND_CLOSEST((in_v_diff[GREEN] * numer),
						denom);

			gray_v[gray_level][BLUE] =
				in_v_btm[BLUE] +
				DIV_ROUND_CLOSEST((in_v_diff[BLUE] * numer),
						denom);

			if (gray_lvl_numer_ptr[i] == NULL)
				numer--;
			else
				numer = *(++numer_ptr);
		}
	}

	if (debug_print) {
		for (i = 0; i < NUM_GRAY_LVLS; i++) {
			pr_info("Calc gray level %d: Voltage "
				"R = %d, G = %d, B = %d\n",
				i, gray_v[i][RED],
				gray_v[i][GREEN], gray_v[i][BLUE]);
		}
	}
}

static void populate_out_gamma(uint32_t v0_val,
			uint8_t preamble_1, uint8_t preamble_2,
			int16_t mtp_table[NUM_VOLT_PTS][NUM_COLORS],
			uint32_t gray_v[NUM_VOLT_PTS][NUM_COLORS],
			uint8_t out_g[NUM_NIT_LVLS][RAW_GAMMA_SIZE])
{
	uint32_t nit_index;
	uint8_t v_pt;
	uint32_t temp_v[NUM_VOLT_PTS][NUM_COLORS];
	uint8_t color_index;
	uint16_t temp_g[NUM_VOLT_PTS][NUM_COLORS];
	uint16_t final_g[NUM_VOLT_PTS][NUM_COLORS];
	uint8_t *out_ptr = NULL;
	uint32_t i;

	for (nit_index = 0; nit_index < NUM_NIT_LVLS; nit_index++) {
		/* Use the desired gray level values for the voltage points */
		for (v_pt = 0; v_pt < NUM_VOLT_PTS; v_pt++) {
			temp_v[v_pt][RED] =
				gray_v[gray_lvl_index[nit_index][v_pt]][RED];
			temp_v[v_pt][GREEN] =
				gray_v[gray_lvl_index[nit_index][v_pt]][GREEN];
			temp_v[v_pt][BLUE] =
				gray_v[gray_lvl_index[nit_index][v_pt]][BLUE];
		}

		/* Translate voltage value to gamma value */
		for (color_index = 0; color_index < NUM_COLORS; color_index++) {
			temp_g[V1][color_index] =
				(((v0_val - temp_v[V1][color_index]) * 600)
					/ v0_val) - 5;

			temp_g[V255][color_index] =
				(((v0_val - temp_v[V255][color_index]) * 600)
					/ v0_val) - 100;

			temp_g[V171][color_index] =
				(((temp_v[V1][color_index] -
				temp_v[V171][color_index]) * 320) /
				(temp_v[V1][color_index] -
					temp_v[V255][color_index])) - 65;

			temp_g[V87][color_index] =
				(((temp_v[V1][color_index] -
				temp_v[V87][color_index]) * 320) /
				(temp_v[V1][color_index] -
					temp_v[V171][color_index])) - 65;

			temp_g[V59][color_index] =
				(((temp_v[V1][color_index] -
				temp_v[V59][color_index]) * 320) /
				(temp_v[V1][color_index] -
					temp_v[V87][color_index])) - 65;

			temp_g[V35][color_index] =
				(((temp_v[V1][color_index] -
				temp_v[V35][color_index]) * 320) /
				(temp_v[V1][color_index] -
					temp_v[V59][color_index])) - 65;

			temp_g[V15][color_index] =
				(((temp_v[V1][color_index] -
				temp_v[V15][color_index]) * 320) /
				(temp_v[V1][color_index] -
					temp_v[V35][color_index])) - 20;
		}

		if (debug_print) {
			for (i = 0; i < 7; i++) {
				pr_info("%d nits: Voltage (Gamma) R = %d (%d), "
					"G = %d (%d), B = %d (%d)\n",
					(nit_index + 1), temp_v[i][RED],
					temp_g[i][RED],
					temp_v[i][GREEN], temp_g[i][GREEN],
					temp_v[i][BLUE], temp_g[i][BLUE]);
			}
			/* Avoid printk flood watchdog reset */
			msleep(5);
		}

		/* Remove MTP offset from value */
		apply_mtp_to_gamma(mtp_table, temp_g, final_g, false);

		out_g[nit_index][0] = preamble_1;
		out_g[nit_index][1] = preamble_2;

		out_ptr = &out_g[nit_index][2];
		for (v_pt = 0; v_pt < NUM_VOLT_PTS; v_pt++) {
			if (v_pt != 6) {
				out_ptr[(v_pt * NUM_COLORS) + 0] =
					(uint8_t) final_g[v_pt][0];
				out_ptr[(v_pt * NUM_COLORS) + 1] =
					(uint8_t) final_g[v_pt][1];
				out_ptr[(v_pt * NUM_COLORS) + 2] =
					(uint8_t) final_g[v_pt][2];
			} else {
				out_ptr[(v_pt * NUM_COLORS) + 0] =
					(uint8_t) ((final_g[v_pt][0] & 0xFF00)
						>> 8);
				out_ptr[(v_pt * NUM_COLORS) + 1] =
					(uint8_t) (final_g[v_pt][0] & 0xFF);
				out_ptr[(v_pt * NUM_COLORS) + 2] =
					(uint8_t) ((final_g[v_pt][1] & 0xFF00)
						>> 8);
				out_ptr[(v_pt * NUM_COLORS) + 3] =
					(uint8_t) (final_g[v_pt][1] & 0xFF);
				out_ptr[(v_pt * NUM_COLORS) + 4] =
					(uint8_t) ((final_g[v_pt][2] & 0xFF00)
						>> 8);
				out_ptr[(v_pt * NUM_COLORS) + 5] =
					(uint8_t) (final_g[v_pt][2] & 0xFF);
			}
		}
	}
}

void smd_dynamic_gamma_dbg_dump(uint8_t raw_mtp[RAW_MTP_SIZE],
				uint8_t out_gamma[NUM_NIT_LVLS][RAW_GAMMA_SIZE],
				struct seq_file *m, void *data, bool csv)
{
	int row;
	int col;

	seq_printf(m, "Raw MTP Data:%c", csv ? ',' : ' ');
	for (col = 0; col < RAW_MTP_SIZE; col++)
		seq_printf(m, "0x%02x%c", raw_mtp[col], csv ? ',' : ' ');
	seq_printf(m, "\n\n");

	seq_printf(m, "Final Gamma Data:\n");
	for (row = 0; row < NUM_NIT_LVLS; row++) {
		seq_printf(m, "%d nits:%c", row + 1, csv ? ',' : ' ');
		for (col = 0; col < RAW_GAMMA_SIZE; col++) {
			seq_printf(m, "0x%02x%c",
				out_gamma[row][col], csv ? ',' : ' ');
		}
		seq_printf(m, "\n");
	}

	seq_printf(m, "\nFinal Gamma Data DDC Format:\n");
	for (row = NUM_NIT_LVLS - 1; row != -1; row--) {
		seq_printf(m, "%d nits:%c", row + 1, csv ? ',' : ' ');
		for (col = 0; col < RAW_GAMMA_SIZE; col++) {
			seq_printf(m, "%02X%c",
				out_gamma[row][col], csv ? ',' : ' ');
		}
		seq_printf(m, "\n");
	}
}

int smd_dynamic_gamma_calc(uint32_t v0_val, uint8_t preamble_1,
			uint8_t preamble_2,
			uint8_t raw_mtp[RAW_MTP_SIZE],
			uint16_t in_gamma[NUM_VOLT_PTS][NUM_COLORS],
			uint8_t out_gamma[NUM_NIT_LVLS][RAW_GAMMA_SIZE])
{
	int16_t mtp_offset[NUM_VOLT_PTS][NUM_COLORS];
	uint16_t in_gamma_mtp[NUM_VOLT_PTS][NUM_COLORS];
	uint32_t in_gamma_volt_pts[NUM_VOLT_PTS][NUM_COLORS];
	uint32_t nit_index;

	parse_raw_mtp(raw_mtp, mtp_offset);

	if (apply_mtp_to_gamma(mtp_offset, in_gamma, in_gamma_mtp, true) != 0) {
		pr_err("Failed apply MTP offset, use 0'd values\n");
		memset(mtp_offset, 0, sizeof(mtp_offset));
		apply_mtp_to_gamma(mtp_offset, in_gamma, in_gamma_mtp, true);
	}

	convert_gamma_to_volt_pts(v0_val, in_gamma_mtp, in_gamma_volt_pts);

	calc_gray_lvl_volt(v0_val, in_gamma_volt_pts, full_gray_lvl_volt);

	populate_out_gamma(v0_val, preamble_1, preamble_2,
			mtp_offset, full_gray_lvl_volt, out_gamma);

	if (debug_print) {
		for (nit_index = 0; nit_index < NUM_NIT_LVLS; nit_index++) {
			pr_info("%d nits:\n", nit_index + 1);
			print_hex_dump(KERN_INFO, "gamma data = ",
				DUMP_PREFIX_NONE,
				32, 1, out_gamma[nit_index],
				RAW_GAMMA_SIZE, false);
		}
	}

	return 0;
}
