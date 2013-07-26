/* Copyright (c) 2013, The LG Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/types.h>
#include <mach/camera2.h>
#include "msm_ois.h"
#include "msm_ois_i2c.h"
#include "lgit_ois_onsemi.h"

unsigned char spigyrocheck = 0x00;

#define OIS_FW_POLLING_PASS      OIS_SUCCESS
#define OIS_FW_POLLING_FAIL      OIS_INIT_TIMEOUT
#define CLRGYR_POLLING_LIMIT_A   6
#define ACCWIT_POLLING_LIMIT_A   6
#define INIDGY_POLLING_LIMIT_A   12
#define INIDGY_POLLING_LIMIT_B   12
#define BSYWIT_POLLING_LIMIT_A   6

#define HALREGTAB    32
#define HALFILTAB    138
#define GYRFILTAB    125

stCalDat StCalDat;         /* Execute Command Parameter */
unsigned char UcOscAdjFlg; /* For Measure trigger */

unsigned char UcVerHig;    /* System Version */
unsigned char UcVerLow;    /* Filter Version */

unsigned short UsCntXof;   /* OPTICAL Center Xvalue */
unsigned short UsCntYof;   /* OPTICAL Center Yvalue */

/* Filter Calculator Version 4.02
 * the time and date: 2013/3/15 16:46:01
 * the time and date: 2013/3/28 22:25:01
 * fs,23438Hz
 * LSI No.,LC898111_AS
 * Comment,
 */

/* 8bit */
const struct STHALREG CsHalReg[][HALREGTAB] = {
	{
		/* VER 00: FC filename: Mitsumi_130315_Y */
		{ 0x000E, 0x00 }, /* 00,000E */
		{ 0x000F, 0x00 }, /* 00,000F,0dB */
		{ 0x0086, 0x00 }, /* 00,0086,0dB */
		{ 0x0087, 0x00 }, /* 00,0087,0dB */
		{ 0x0088, 0x25 }, /* 25,0088,30dB */
		{ 0x0089, 0x00 }, /* 00,0089 */
		{ 0x008A, 0x40 }, /* 40,008A,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x008B, 0x44 }, /* 44,008B,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x008C, 0x00 }, /* 00,008C,LBF,10Hz,15Hz,0dB,fs/1,invert=0 */
		{ 0x0090, 0x00 }, /* 00,0090,0dB */
		{ 0x0091, 0x00 }, /* 00,0091,0dB */
		{ 0x0092, 0x25 }, /* 25,0092,30dB */
		{ 0x0093, 0x00 }, /* 00,0093 */
		{ 0x0094, 0x40 }, /* 40,0094,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x0095, 0x44 }, /* 44,0095,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x0096, 0x00 }, /* 00,0096,LBF,10Hz,15Hz,0dB,fs/1,invert=0 */
		{ 0x00A0, 0x00 }, /* 00,00A0,0dB */
		{ 0x00A1, 0x00 }, /* 00,00A1 */
		{ 0x00B4, 0x00 }, /* 00,00B4 */
		{ 0x00B5, 0x00 }, /* 00,00B5,0dB */
		{ 0x00B6, 0x00 }, /* 00,00B6,Through,0dB,fs/1,invert=0 */
		{ 0x00BB, 0x00 }, /* 00,00BB */
		{ 0x00BC, 0x00 }, /* 00,00BC,0dB */
		{ 0x00BD, 0x00 }, /* 00,00BD,Through,0dB,fs/1,invert=0 */
		{ 0x00C1, 0x00 }, /* 00,00C1,Through,0dB,fs/1,invert=0 */
		{ 0x00C5, 0x00 }, /* 00,00C5,Through,0dB,fs/1,invert=0 */
		{ 0x00C8, 0x00 }, /* 00,00C8 */
		{ 0x0110, 0x01 }, /* 01,0110 */
		{ 0x0111, 0x00 }, /* 00,0111 */
		{ 0x0112, 0x01 }, /* 01,0112 */
		{ 0x017D, 0x00 }, /* 00,017D */
		{ 0xFFFF, 0xFF }
	},
	{
		/* VER 01: FC filename: Mitsumi_130326_4 */
		{ 0x000E, 0x00 }, /* 00,000E */
		{ 0x000F, 0x00 }, /* 00,000F,0dB */
		{ 0x0086, 0x00 }, /* 00,0086,0dB */
		{ 0x0087, 0x00 }, /* 00,0087,0dB */
		{ 0x0088, 0x25 }, /* 25,0088,30dB */
		{ 0x0089, 0x00 }, /* 00,0089 */
		{ 0x008A, 0x40 }, /* 40,008A,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x008B, 0x44 }, /* 44,008B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x008C, 0x00 }, /* 00,008C,LBF,10Hz,15Hz,0dB,fs/1,invert=0 */
		{ 0x0090, 0x00 }, /* 00,0090,0dB */
		{ 0x0091, 0x00 }, /* 00,0091,0dB */
		{ 0x0092, 0x25 }, /* 25,0092,30dB */
		{ 0x0093, 0x00 }, /* 00,0093 */
		{ 0x0094, 0x40 }, /* 40,0094,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x0095, 0x44 }, /* 44,0095,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x0096, 0x00 }, /* 00,0096,LBF,10Hz,15Hz,0dB,fs/1,invert=0 */
		{ 0x00A0, 0x00 }, /* 00,00A0,0dB */
		{ 0x00A1, 0x00 }, /* 00,00A1 */
		{ 0x00B4, 0x00 }, /* 00,00B4 */
		{ 0x00B5, 0x00 }, /* 00,00B5,0dB */
		{ 0x00B6, 0x00 }, /* 00,00B6,Through,0dB,fs/1,invert=0 */
		{ 0x00BB, 0x00 }, /* 00,00BB */
		{ 0x00BC, 0x00 }, /* 00,00BC,0dB */
		{ 0x00BD, 0x00 }, /* 00,00BD,Through,0dB,fs/1,invert=0 */
		{ 0x00C1, 0x00 }, /* 00,00C1,Through,0dB,fs/1,invert=0 */
		{ 0x00C5, 0x00 }, /* 00,00C5,Through,0dB,fs/1,invert=0 */
		{ 0x00C8, 0x00 }, /* 00,00C8 */
		{ 0x0110, 0x01 }, /* 01,0110 */
		{ 0x0111, 0x00 }, /* 00,0111 */
		{ 0x0112, 0x01 }, /* 01,0112 */
		{ 0x017D, 0x00 }, /* 00,017D */
		{ 0xFFFF, 0xFF }
	},
	{
		/* VER 02: FC filename: Mitsumi_130326_4_X_RVS */
		{ 0x000E, 0x00 }, /* 00,000E */
		{ 0x000F, 0x00 }, /* 00,000F,0dB */
		{ 0x0086, 0x00 }, /* 00,0086,0dB */
		{ 0x0087, 0x00 }, /* 00,0087,0dB */
		{ 0x0088, 0x25 }, /* 25,0088,30dB */
		{ 0x0089, 0x00 }, /* 00,0089 */
		{ 0x008A, 0x40 }, /* 40,008A,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x008B, 0x44 }, /* 44,008B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x008C, 0x00 }, /* 00,008C,LBF,10Hz,15Hz,0dB,fs/1,invert=0 */
		{ 0x0090, 0x00 }, /* 00,0090,0dB */
		{ 0x0091, 0x00 }, /* 00,0091,0dB */
		{ 0x0092, 0x25 }, /* 25,0092,30dB */
		{ 0x0093, 0x00 }, /* 00,0093 */
		{ 0x0094, 0x40 }, /* 40,0094,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x0095, 0x44 }, /* 44,0095,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x0096, 0x00 }, /* 00,0096,LBF,10Hz,15Hz,0dB,fs/1,invert=0 */
		{ 0x00A0, 0x00 }, /* 00,00A0,0dB */
		{ 0x00A1, 0x00 }, /* 00,00A1 */
		{ 0x00B4, 0x00 }, /* 00,00B4 */
		{ 0x00B5, 0x00 }, /* 00,00B5,0dB */
		{ 0x00B6, 0x00 }, /* 00,00B6,Through,0dB,fs/1,invert=0 */
		{ 0x00BB, 0x00 }, /* 00,00BB */
		{ 0x00BC, 0x00 }, /* 00,00BC,0dB */
		{ 0x00BD, 0x00 }, /* 00,00BD,Through,0dB,fs/1,invert=0 */
		{ 0x00C1, 0x00 }, /* 00,00C1,Through,0dB,fs/1,invert=0 */
		{ 0x00C5, 0x00 }, /* 00,00C5,Through,0dB,fs/1,invert=0 */
		{ 0x00C8, 0x00 }, /* 00,00C8 */
		{ 0x0110, 0x01 }, /* 01,0110 */
		{ 0x0111, 0x00 }, /* 00,0111 */
		{ 0x0112, 0x01 }, /* 01,0112 */
		{ 0x017D, 0x00 }, /* 00,017D */
		{ 0xFFFF, 0xFF }
	},
	{
		/* VER 03: FC filename: Mitsumi_130501 */
		{ 0x000E, 0x00 }, /* 00,000E */
		{ 0x000F, 0x00 }, /* 00,000F,0dB */
		{ 0x0086, 0x00 }, /* 00,0086,0dB */
		{ 0x0087, 0x00 }, /* 00,0087,0dB */
		{ 0x0088, 0x15 }, /* 15,0088,30dB */
		{ 0x0089, 0x00 }, /* 00,0089 */
		{ 0x008A, 0x40 }, /* 40,008A,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x008B, 0x44 }, /* 44,008B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x008C, 0x00 }, /* 00,008C,LBF,10Hz,15Hz,0dB,fs/1,invert=0 */
		{ 0x0090, 0x00 }, /* 00,0090,0dB */
		{ 0x0091, 0x00 }, /* 00,0091,0dB */
		{ 0x0092, 0x15 }, /* 15,0092,30dB */
		{ 0x0093, 0x00 }, /* 00,0093 */
		{ 0x0094, 0x40 }, /* 40,0094,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x0095, 0x44 }, /* 44,0095,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x0096, 0x00 }, /* 00,0096,LBF,10Hz,15Hz,0dB,fs/1,invert=0 */
		{ 0x00A0, 0x00 }, /* 00,00A0,0dB */
		{ 0x00A1, 0x00 }, /* 00,00A1 */
		{ 0x00B4, 0x00 }, /* 00,00B4 */
		{ 0x00B5, 0x00 }, /* 00,00B5,0dB */
		{ 0x00B6, 0x00 }, /* 00,00B6,Through,0dB,fs/1,invert=0 */
		{ 0x00BB, 0x00 }, /* 00,00BB */
		{ 0x00BC, 0x00 }, /* 00,00BC,0dB */
		{ 0x00BD, 0x00 }, /* 00,00BD,Through,0dB,fs/1,invert=0 */
		{ 0x00C1, 0x00 }, /* 00,00C1,Through,0dB,fs/1,invert=0 */
		{ 0x00C5, 0x00 }, /* 00,00C5,Through,0dB,fs/1,invert=0 */
		{ 0x00C8, 0x00 }, /* 00,00C8 */
		{ 0x0110, 0x01 }, /* 01,0110 */
		{ 0x0111, 0x00 }, /* 00,0111 */
		{ 0x0112, 0x01 }, /* 01,0112 */
		{ 0x017D, 0x00 }, /* 00,017D */
		{ 0xFFFF, 0xFF }
	},
	{
		/* VER 04: FC filename: Mitsumi_130501 */
		{ 0x000E, 0x00 }, /* 00,000E */
		{ 0x000F, 0x00 }, /* 00,000F,0dB */
		{ 0x0086, 0x00 }, /* 00,0086,0dB */
		{ 0x0087, 0x00 }, /* 00,0087,0dB */
		{ 0x0088, 0x15 }, /* 15,0088,30dB */
		{ 0x0089, 0x00 }, /* 00,0089 */
		{ 0x008A, 0x40 }, /* 40,008A,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x008B, 0x54 }, /* 54,008B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x008C, 0x00 }, /* 00,008C,LBF,10Hz,15Hz,0dB,fs/1,invert=0 */
		{ 0x0090, 0x00 }, /* 00,0090,0dB */
		{ 0x0091, 0x00 }, /* 00,0091,0dB */
		{ 0x0092, 0x15 }, /* 15,0092,30dB */
		{ 0x0093, 0x00 }, /* 00,0093 */
		{ 0x0094, 0x40 }, /* 40,0094,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x0095, 0x54 }, /* 54,0095,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x0096, 0x00 }, /* 00,0096,LBF,10Hz,15Hz,0dB,fs/1,invert=0 */
		{ 0x00A0, 0x00 }, /* 00,00A0,0dB */
		{ 0x00A1, 0x00 }, /* 00,00A1 */
		{ 0x00B4, 0x00 }, /* 00,00B4 */
		{ 0x00B5, 0x00 }, /* 00,00B5,0dB */
		{ 0x00B6, 0x00 }, /* 00,00B6,Through,0dB,fs/1,invert=0 */
		{ 0x00BB, 0x00 }, /* 00,00BB */
		{ 0x00BC, 0x00 }, /* 00,00BC,0dB */
		{ 0x00BD, 0x00 }, /* 00,00BD,Through,0dB,fs/1,invert=0 */
		{ 0x00C1, 0x00 }, /* 00,00C1,Through,0dB,fs/1,invert=0 */
		{ 0x00C5, 0x00 }, /* 00,00C5,Through,0dB,fs/1,invert=0 */
		{ 0x00C8, 0x00 }, /* 00,00C8 */
		{ 0x0110, 0x01 }, /* 01,0110 */
		{ 0x0111, 0x00 }, /* 00,0111 */
		{ 0x0112, 0x00 }, /* 00,0112 */
		{ 0x017D, 0x01 }, /* 01,017D */
		{ 0xFFFF, 0xFF }
	}
};

/* 16bit */
const struct STHALFIL CsHalFil[][HALFILTAB] = {
	{
		/* VER 00: FC filename: Mitsumi_130315_Y */
		{ 0x1128, 0x0000 }, /* 0000,1128,Cutoff,invert=0 */
		{ 0x1168, 0x0000 }, /* 0000,1168,Cutoff,invert=0 */
		{ 0x11E0, 0x7FFF }, /* 7FFF,11E0,0dB,invert=0 */
		{ 0x11E1, 0x7FFF }, /* 7FFF,11E1,0dB,invert=0 */
		{ 0x11E2, 0x7FFF }, /* 7FFF,11E2,0dB,invert=0 */
		{ 0x11E3, 0x7FFF }, /* 7FFF,11E3,0dB,invert=0 */
		{ 0x11E4, 0x7FFF }, /* 7FFF,11E4,0dB,invert=0 */
		{ 0x11E5, 0x7FFF }, /* 7FFF,11E5,0dB,invert=0 */
		{ 0x12E0, 0x7FFF }, /* 7FFF,12E0,Through,0dB,fs/1,invert=0 */
		{ 0x12E1, 0x0000 }, /* 0000,12E1,Through,0dB,fs/1,invert=0 */
		{ 0x12E2, 0x0000 }, /* 0000,12E2,Through,0dB,fs/1,invert=0 */
		{ 0x12E3, 0x7FFF }, /* 7FFF,12E3,Through,0dB,fs/1,invert=0 */
		{ 0x12E4, 0x0000 }, /* 0000,12E4,Through,0dB,fs/1,invert=0 */
		{ 0x12E5, 0x0000 }, /* 0000,12E5,Through,0dB,fs/1,invert=0 */
		{ 0x12E6, 0x7FFF }, /* 7FFF,12E6,0dB,invert=0 */
		{ 0x1301, 0x7FFF }, /* 7FFF,1301,0dB,invert=0 */
		{ 0x1302, 0x7FFF }, /* 7FFF,1302,0dB,invert=0 */
		{ 0x1305, 0x7FFF }, /* 7FFF,1305,Through,0dB,fs/1,invert=0 */
		{ 0x1306, 0x0000 }, /* 0000,1306,Through,0dB,fs/1,invert=0 */
		{ 0x1307, 0x0000 }, /* 0000,1307,Through,0dB,fs/1,invert=0 */
		{ 0x1308, 0x0000 }, /* 0000,1308,Cutoff,invert=0 */
		{ 0x1309, 0x5A9D }, /* 5A9D,1309,-3dB,invert=0 */
		{ 0x130A, 0x0000 }, /* 0000,130A,Cutoff,invert=0 */
		{ 0x130B, 0x0000 }, /* 0000,130B,Cutoff,invert=0 */
		{ 0x130C, 0x7FFF }, /* 7FFF,130C,0dB,invert=0 */
		{ 0x130D, 0x43B7 }, /* 43B7,130D,HBF,70Hz,1500Hz,2dB,fs/1,invert=0 */
		{ 0x130E, 0xBD8B }, /* BD8B,130E,HBF,70Hz,1500Hz,2dB,fs/1,invert=0 */
		{ 0x130F, 0x2A93 }, /* 2A93,130F,HBF,70Hz,1500Hz,2dB,fs/1,invert=0 */
		{ 0x1310, 0x0000 }, /* 0000,1310,HBF,70Hz,1500Hz,2dB,fs/1,invert=0 */
		{ 0x1311, 0x0000 }, /* 0000,1311,HBF,70Hz,1500Hz,2dB,fs/1,invert=0 */
		{ 0x1312, 0x7FFF }, /* 7FFF,1312,Through,0dB,fs/1,invert=0 */
		{ 0x1313, 0x0000 }, /* 0000,1313,Through,0dB,fs/1,invert=0 */
		{ 0x1314, 0x0000 }, /* 0000,1314,Through,0dB,fs/1,invert=0 */
		{ 0x1315, 0x0000 }, /* 0000,1315,Through,0dB,fs/1,invert=0 */
		{ 0x1316, 0x0000 }, /* 0000,1316,Through,0dB,fs/1,invert=0 */
		{ 0x1317, 0x7FFF }, /* 7FFF,1317,0dB,invert=0 */
		{ 0x1318, 0x0000 }, /* 0000,1318,Cutoff,invert=0 */
		{ 0x1319, 0x0000 }, /* 0000,1319,Cutoff,invert=0 */
		{ 0x131A, 0x0035 }, /* 0035,131A,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131B, 0x006A }, /* 006A,131B,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131C, 0x716F }, /* 716F,131C,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131D, 0x0035 }, /* 0035,131D,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131E, 0xCDBC }, /* CDBC,131E,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131F, 0x013B }, /* 013B,131F,LPF,0.9Hz,26dB,fs/4,invert=0 */
		{ 0x1320, 0x013B }, /* 013B,1320,LPF,0.9Hz,26dB,fs/4,invert=0 */
		{ 0x1321, 0x7FE1 }, /* 7FE1,1321,LPF,0.9Hz,26dB,fs/4,invert=0 */
		{ 0x1322, 0x0000 }, /* 0000,1322,LPF,0.9Hz,26dB,fs/4,invert=0 */
		{ 0x1323, 0x0000 }, /* 0000,1323,LPF,0.9Hz,26dB,fs/4,invert=0 */
		{ 0x1324, 0x5391 }, /* 5391,1324,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1325, 0xACF2 }, /* ACF2,1325,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1326, 0x7F7D }, /* 7F7D,1326,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1327, 0x0000 }, /* 0000,1327,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1328, 0x0000 }, /* 0000,1328,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1329, 0x7FFF }, /* 7FFF,1329,0dB,invert=0 */
		{ 0x132A, 0x7FFF }, /* 7FFF,132A,0dB,invert=0 */
		{ 0x132B, 0x0000 }, /* 0000,132B,Cutoff,invert=0 */
		{ 0x132C, 0x0000 }, /* 0000,132C,Cutoff,invert=0 */
		{ 0x132D, 0x0000 }, /* 0000,132D,Cutoff,invert=0 */
		{ 0x132E, 0x0000 }, /* 0000,132E,Cutoff,invert=0 */
		{ 0x132F, 0x7FFF }, /* 7FFF,132F,Through,0dB,fs/1,invert=0 */
		{ 0x1330, 0x0000 }, /* 0000,1330,Through,0dB,fs/1,invert=0 */
		{ 0x1331, 0x0000 }, /* 0000,1331,Through,0dB,fs/1,invert=0 */
		{ 0x1332, 0x378D }, /* 378D,1332,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1333, 0x9DB1 }, /* 9DB1,1333,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1334, 0x624F }, /* 624F,1334,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1335, 0x30EB }, /* 30EB,1335,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1336, 0xD788 }, /* D788,1336,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1337, 0x0DC7 }, /* 0DC7,1337,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x1338, 0x0DC7 }, /* 0DC7,1338,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x1339, 0x6471 }, /* 6471,1339,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x133A, 0x0000 }, /* 0000,133A,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x133B, 0x0000 }, /* 0000,133B,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x133C, 0x7FFF }, /* 7FFF,133C,0dB,invert=0 */
		{ 0x133D, 0x0000 }, /* 0000,133D,Cutoff,invert=0 */
		{ 0x133E, 0x0000 }, /* 0000,133E,Cutoff,invert=0 */
		{ 0x133F, 0x8001 }, /* 8001,133F,0dB,invert=1 */
		{ 0x1341, 0x7FFF }, /* 7FFF,1341,0dB,invert=0 */
		{ 0x1342, 0x7FFF }, /* 7FFF,1342,0dB,invert=0 */
		{ 0x1345, 0x7FFF }, /* 7FFF,1345,Through,0dB,fs/1,invert=0 */
		{ 0x1346, 0x0000 }, /* 0000,1346,Through,0dB,fs/1,invert=0 */
		{ 0x1347, 0x0000 }, /* 0000,1347,Through,0dB,fs/1,invert=0 */
		{ 0x1348, 0x0000 }, /* 0000,1348,Cutoff,invert=0 */
		{ 0x1349, 0x5A9D }, /* 5A9D,1349,-3dB,invert=0 */
		{ 0x134A, 0x0000 }, /* 0000,134A,Cutoff,invert=0 */
		{ 0x134B, 0x0000 }, /* 0000,134B,Cutoff,invert=0 */
		{ 0x134C, 0x7FFF }, /* 7FFF,134C,0dB,invert=0 */
		{ 0x134D, 0x43B7 }, /* 43B7,134D,HBF,70Hz,1500Hz,2dB,fs/1,invert=0 */
		{ 0x134E, 0xBD8B }, /* BD8B,134E,HBF,70Hz,1500Hz,2dB,fs/1,invert=0 */
		{ 0x134F, 0x2A93 }, /* 2A93,134F,HBF,70Hz,1500Hz,2dB,fs/1,invert=0 */
		{ 0x1350, 0x0000 }, /* 0000,1350,HBF,70Hz,1500Hz,2dB,fs/1,invert=0 */
		{ 0x1351, 0x0000 }, /* 0000,1351,HBF,70Hz,1500Hz,2dB,fs/1,invert=0 */
		{ 0x1352, 0x7FFF }, /* 7FFF,1352,Through,0dB,fs/1,invert=0 */
		{ 0x1353, 0x0000 }, /* 0000,1353,Through,0dB,fs/1,invert=0 */
		{ 0x1354, 0x0000 }, /* 0000,1354,Through,0dB,fs/1,invert=0 */
		{ 0x1355, 0x0000 }, /* 0000,1355,Through,0dB,fs/1,invert=0 */
		{ 0x1356, 0x0000 }, /* 0000,1356,Through,0dB,fs/1,invert=0 */
		{ 0x1357, 0x7FFF }, /* 7FFF,1357,0dB,invert=0 */
		{ 0x1358, 0x0000 }, /* 0000,1358,Cutoff,invert=0 */
		{ 0x1359, 0x0000 }, /* 0000,1359,Cutoff,invert=0 */
		{ 0x135A, 0x0035 }, /* 0035,135A,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135B, 0x006A }, /* 006A,135B,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135C, 0x716F }, /* 716F,135C,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135D, 0x0035 }, /* 0035,135D,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135E, 0xCDBC }, /* CDBC,135E,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135F, 0x013B }, /* 013B,135F,LPF,0.9Hz,26dB,fs/4,invert=0 */
		{ 0x1360, 0x013B }, /* 013B,1360,LPF,0.9Hz,26dB,fs/4,invert=0 */
		{ 0x1361, 0x7FE1 }, /* 7FE1,1361,LPF,0.9Hz,26dB,fs/4,invert=0 */
		{ 0x1362, 0x0000 }, /* 0000,1362,LPF,0.9Hz,26dB,fs/4,invert=0 */
		{ 0x1363, 0x0000 }, /* 0000,1363,LPF,0.9Hz,26dB,fs/4,invert=0 */
		{ 0x1364, 0x5391 }, /* 5391,1364,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1365, 0xACF2 }, /* ACF2,1365,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1366, 0x7F7D }, /* 7F7D,1366,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1367, 0x0000 }, /* 0000,1367,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1368, 0x0000 }, /* 0000,1368,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1369, 0x7FFF }, /* 7FFF,1369,0dB,invert=0 */
		{ 0x136A, 0x7FFF }, /* 7FFF,136A,0dB,invert=0 */
		{ 0x136B, 0x0000 }, /* 0000,136B,Cutoff,invert=0 */
		{ 0x136C, 0x0000 }, /* 0000,136C,Cutoff,invert=0 */
		{ 0x136D, 0x0000 }, /* 0000,136D,Cutoff,invert=0 */
		{ 0x136E, 0x0000 }, /* 0000,136E,Cutoff,invert=0 */
		{ 0x136F, 0x7FFF }, /* 7FFF,136F,Through,0dB,fs/1,invert=0 */
		{ 0x1370, 0x0000 }, /* 0000,1370,Through,0dB,fs/1,invert=0 */
		{ 0x1371, 0x0000 }, /* 0000,1371,Through,0dB,fs/1,invert=0 */
		{ 0x1372, 0x378D }, /* 378D,1372,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1373, 0x9DB1 }, /* 9DB1,1373,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1374, 0x624F }, /* 624F,1374,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1375, 0x30EB }, /* 30EB,1375,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1376, 0xD788 }, /* D788,1376,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1377, 0x0DC7 }, /* 0DC7,1377,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x1378, 0x0DC7 }, /* 0DC7,1378,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x1379, 0x6471 }, /* 6471,1379,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x137A, 0x0000 }, /* 0000,137A,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x137B, 0x0000 }, /* 0000,137B,LPF,900Hz,0dB,fs/1,invert=0 */
		{ 0x137C, 0x7FFF }, /* 7FFF,137C,0dB,invert=0 */
		{ 0x137D, 0x0000 }, /* 0000,137D,Cutoff,invert=0 */
		{ 0x137E, 0x0000 }, /* 0000,137E,Cutoff,invert=0 */
		{ 0x137F, 0x7FFF }, /* 7FFF,137F,0dB,invert=0 */
		{ 0xFFFF, 0xFFFF }
	},
	{
		/* VER 01: FC filename: Mitsumi_130326_4 */
		{ 0x1128, 0x0000 }, /* 0000,1128,Cutoff,invert=0 */
		{ 0x1168, 0x0000 }, /* 0000,1168,Cutoff,invert=0 */
		{ 0x11E0, 0x7FFF }, /* 7FFF,11E0,0dB,invert=0 */
		{ 0x11E1, 0x7FFF }, /* 7FFF,11E1,0dB,invert=0 */
		{ 0x11E2, 0x7FFF }, /* 7FFF,11E2,0dB,invert=0 */
		{ 0x11E3, 0x7FFF }, /* 7FFF,11E3,0dB,invert=0 */
		{ 0x11E4, 0x7FFF }, /* 7FFF,11E4,0dB,invert=0 */
		{ 0x11E5, 0x7FFF }, /* 7FFF,11E5,0dB,invert=0 */
		{ 0x12E0, 0x7FFF }, /* 7FFF,12E0,Through,0dB,fs/1,invert=0 */
		{ 0x12E1, 0x0000 }, /* 0000,12E1,Through,0dB,fs/1,invert=0 */
		{ 0x12E2, 0x0000 }, /* 0000,12E2,Through,0dB,fs/1,invert=0 */
		{ 0x12E3, 0x7FFF }, /* 7FFF,12E3,Through,0dB,fs/1,invert=0 */
		{ 0x12E4, 0x0000 }, /* 0000,12E4,Through,0dB,fs/1,invert=0 */
		{ 0x12E5, 0x0000 }, /* 0000,12E5,Through,0dB,fs/1,invert=0 */
		{ 0x12E6, 0x7FFF }, /* 7FFF,12E6,0dB,invert=0 */
		{ 0x1301, 0x7FFF }, /* 7FFF,1301,0dB,invert=0 */
		{ 0x1302, 0x7FFF }, /* 7FFF,1302,0dB,invert=0 */
		{ 0x1305, 0x7FFF }, /* 7FFF,1305,Through,0dB,fs/1,invert=0 */
		{ 0x1306, 0x0000 }, /* 0000,1306,Through,0dB,fs/1,invert=0 */
		{ 0x1307, 0x0000 }, /* 0000,1307,Through,0dB,fs/1,invert=0 */
		{ 0x1308, 0x0000 }, /* 0000,1308,Cutoff,invert=0 */
		{ 0x1309, 0x5A9D }, /* 5A9D,1309,-3dB,invert=0 */
		{ 0x130A, 0x0000 }, /* 0000,130A,Cutoff,invert=0 */
		{ 0x130B, 0x0000 }, /* 0000,130B,Cutoff,invert=0 */
		{ 0x130C, 0x7FFF }, /* 7FFF,130C,0dB,invert=0 */
		{ 0x130D, 0x47AD }, /* 47AD,130D,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x130E, 0xB98F }, /* B98F,130E,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x130F, 0x2A93 }, /* 2A93,130F,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1310, 0x0000 }, /* 0000,1310,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1311, 0x0000 }, /* 0000,1311,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1312, 0x7E5F }, /* 7E5F,1312,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1313, 0x8B66 }, /* 8B66,1313,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1314, 0x72F9 }, /* 72F9,1314,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1315, 0x0000 }, /* 0000,1315,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1316, 0x0000 }, /* 0000,1316,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1317, 0x7FFF }, /* 7FFF,1317,0dB,invert=0 */
		{ 0x1318, 0x0000 }, /* 0000,1318,Cutoff,invert=0 */
		{ 0x1319, 0x0000 }, /* 0000,1319,Cutoff,invert=0 */
		{ 0x131A, 0x0035 }, /* 0035,131A,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131B, 0x006A }, /* 006A,131B,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131C, 0x716F }, /* 716F,131C,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131D, 0x0035 }, /* 0035,131D,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131E, 0xCDBC }, /* CDBC,131E,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131F, 0x0169 }, /* 0169,131F,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1320, 0x0169 }, /* 0169,1320,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1321, 0x7FE9 }, /* 7FE9,1321,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1322, 0x0000 }, /* 0000,1322,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1323, 0x0000 }, /* 0000,1323,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1324, 0x5391 }, /* 5391,1324,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1325, 0xACF2 }, /* ACF2,1325,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1326, 0x7F7D }, /* 7F7D,1326,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1327, 0x0000 }, /* 0000,1327,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1328, 0x0000 }, /* 0000,1328,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1329, 0x7FFF }, /* 7FFF,1329,0dB,invert=0 */
		{ 0x132A, 0x7FFF }, /* 7FFF,132A,0dB,invert=0 */
		{ 0x132B, 0x0000 }, /* 0000,132B,Cutoff,invert=0 */
		{ 0x132C, 0x0000 }, /* 0000,132C,Cutoff,invert=0 */
		{ 0x132D, 0x0000 }, /* 0000,132D,Cutoff,invert=0 */
		{ 0x132E, 0x0000 }, /* 0000,132E,Cutoff,invert=0 */
		{ 0x132F, 0x7FFF }, /* 7FFF,132F,Through,0dB,fs/1,invert=0 */
		{ 0x1330, 0x0000 }, /* 0000,1330,Through,0dB,fs/1,invert=0 */
		{ 0x1331, 0x0000 }, /* 0000,1331,Through,0dB,fs/1,invert=0 */
		{ 0x1332, 0x378D }, /* 378D,1332,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1333, 0x9DB1 }, /* 9DB1,1333,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1334, 0x624F }, /* 624F,1334,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1335, 0x30EB }, /* 30EB,1335,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1336, 0xD788 }, /* D788,1336,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1337, 0x0D17 }, /* 0D17,1337,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1338, 0x0D17 }, /* 0D17,1338,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1339, 0x65D1 }, /* 65D1,1339,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133A, 0x0000 }, /* 0000,133A,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133B, 0x0000 }, /* 0000,133B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133C, 0x7FFF }, /* 7FFF,133C,0dB,invert=0 */
		{ 0x133D, 0x0000 }, /* 0000,133D,Cutoff,invert=0 */
		{ 0x133E, 0x0000 }, /* 0000,133E,Cutoff,invert=0 */
		{ 0x133F, 0x8001 }, /* 8001,133F,0dB,invert=1 */
		{ 0x1341, 0x7FFF }, /* 7FFF,1341,0dB,invert=0 */
		{ 0x1342, 0x7FFF }, /* 7FFF,1342,0dB,invert=0 */
		{ 0x1345, 0x7FFF }, /* 7FFF,1345,Through,0dB,fs/1,invert=0 */
		{ 0x1346, 0x0000 }, /* 0000,1346,Through,0dB,fs/1,invert=0 */
		{ 0x1347, 0x0000 }, /* 0000,1347,Through,0dB,fs/1,invert=0 */
		{ 0x1348, 0x0000 }, /* 0000,1348,Cutoff,invert=0 */
		{ 0x1349, 0x5A9D }, /* 5A9D,1349,-3dB,invert=0 */
		{ 0x134A, 0x0000 }, /* 0000,134A,Cutoff,invert=0 */
		{ 0x134B, 0x0000 }, /* 0000,134B,Cutoff,invert=0 */
		{ 0x134C, 0x7FFF }, /* 7FFF,134C,0dB,invert=0 */
		{ 0x134D, 0x47AD }, /* 47AD,134D,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x134E, 0xB98F }, /* B98F,134E,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x134F, 0x2A93 }, /* 2A93,134F,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1350, 0x0000 }, /* 0000,1350,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1351, 0x0000 }, /* 0000,1351,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1352, 0x7E5F }, /* 7E5F,1352,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1353, 0x8B66 }, /* 8B66,1353,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1354, 0x72F9 }, /* 72F9,1354,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1355, 0x0000 }, /* 0000,1355,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1356, 0x0000 }, /* 0000,1356,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1357, 0x7FFF }, /* 7FFF,1357,0dB,invert=0 */
		{ 0x1358, 0x0000 }, /* 0000,1358,Cutoff,invert=0 */
		{ 0x1359, 0x0000 }, /* 0000,1359,Cutoff,invert=0 */
		{ 0x135A, 0x0035 }, /* 0035,135A,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135B, 0x006A }, /* 006A,135B,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135C, 0x716F }, /* 716F,135C,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135D, 0x0035 }, /* 0035,135D,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135E, 0xCDBC }, /* CDBC,135E,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135F, 0x0169 }, /* 0169,135F,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1360, 0x0169 }, /* 0169,1360,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1361, 0x7FE9 }, /* 7FE9,1361,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1362, 0x0000 }, /* 0000,1362,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1363, 0x0000 }, /* 0000,1363,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1364, 0x5391 }, /* 5391,1364,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1365, 0xACF2 }, /* ACF2,1365,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1366, 0x7F7D }, /* 7F7D,1366,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1367, 0x0000 }, /* 0000,1367,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1368, 0x0000 }, /* 0000,1368,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1369, 0x7FFF }, /* 7FFF,1369,0dB,invert=0 */
		{ 0x136A, 0x7FFF }, /* 7FFF,136A,0dB,invert=0 */
		{ 0x136B, 0x0000 }, /* 0000,136B,Cutoff,invert=0 */
		{ 0x136C, 0x0000 }, /* 0000,136C,Cutoff,invert=0 */
		{ 0x136D, 0x0000 }, /* 0000,136D,Cutoff,invert=0 */
		{ 0x136E, 0x0000 }, /* 0000,136E,Cutoff,invert=0 */
		{ 0x136F, 0x7FFF }, /* 7FFF,136F,Through,0dB,fs/1,invert=0 */
		{ 0x1370, 0x0000 }, /* 0000,1370,Through,0dB,fs/1,invert=0 */
		{ 0x1371, 0x0000 }, /* 0000,1371,Through,0dB,fs/1,invert=0 */
		{ 0x1372, 0x378D }, /* 378D,1372,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1373, 0x9DB1 }, /* 9DB1,1373,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1374, 0x624F }, /* 624F,1374,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1375, 0x30EB }, /* 30EB,1375,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1376, 0xD788 }, /* D788,1376,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1377, 0x0D17 }, /* 0D17,1377,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1378, 0x0D17 }, /* 0D17,1378,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1379, 0x65D1 }, /* 65D1,1379,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137A, 0x0000 }, /* 0000,137A,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137B, 0x0000 }, /* 0000,137B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137C, 0x7FFF }, /* 7FFF,137C,0dB,invert=0 */
		{ 0x137D, 0x0000 }, /* 0000,137D,Cutoff,invert=0 */
		{ 0x137E, 0x0000 }, /* 0000,137E,Cutoff,invert=0 */
		{ 0x137F, 0x7FFF }, /* 7FFF,137F,0dB,invert=0 */
		{ 0xFFFF, 0xFFFF }
	},
	{
		/* VER 02: FC filename: Mitsumi_130326_4_X_RVS */
		{ 0x1128, 0x0000 }, /* 0000,1128,Cutoff,invert=0 */
		{ 0x1168, 0x0000 }, /* 0000,1168,Cutoff,invert=0 */
		{ 0x11E0, 0x7FFF }, /* 7FFF,11E0,0dB,invert=0 */
		{ 0x11E1, 0x7FFF }, /* 7FFF,11E1,0dB,invert=0 */
		{ 0x11E2, 0x7FFF }, /* 7FFF,11E2,0dB,invert=0 */
		{ 0x11E3, 0x7FFF }, /* 7FFF,11E3,0dB,invert=0 */
		{ 0x11E4, 0x7FFF }, /* 7FFF,11E4,0dB,invert=0 */
		{ 0x11E5, 0x7FFF }, /* 7FFF,11E5,0dB,invert=0 */
		{ 0x12E0, 0x7FFF }, /* 7FFF,12E0,Through,0dB,fs/1,invert=0 */
		{ 0x12E1, 0x0000 }, /* 0000,12E1,Through,0dB,fs/1,invert=0 */
		{ 0x12E2, 0x0000 }, /* 0000,12E2,Through,0dB,fs/1,invert=0 */
		{ 0x12E3, 0x7FFF }, /* 7FFF,12E3,Through,0dB,fs/1,invert=0 */
		{ 0x12E4, 0x0000 }, /* 0000,12E4,Through,0dB,fs/1,invert=0 */
		{ 0x12E5, 0x0000 }, /* 0000,12E5,Through,0dB,fs/1,invert=0 */
		{ 0x12E6, 0x7FFF }, /* 7FFF,12E6,0dB,invert=0 */
		{ 0x1301, 0x7FFF }, /* 7FFF,1301,0dB,invert=0 */
		{ 0x1302, 0x7FFF }, /* 7FFF,1302,0dB,invert=0 */
		{ 0x1305, 0x7FFF }, /* 7FFF,1305,Through,0dB,fs/1,invert=0 */
		{ 0x1306, 0x0000 }, /* 0000,1306,Through,0dB,fs/1,invert=0 */
		{ 0x1307, 0x0000 }, /* 0000,1307,Through,0dB,fs/1,invert=0 */
		{ 0x1308, 0x0000 }, /* 0000,1308,Cutoff,invert=0 */
		{ 0x1309, 0x5A9D }, /* 5A9D,1309,-3dB,invert=0 */
		{ 0x130A, 0x0000 }, /* 0000,130A,Cutoff,invert=0 */
		{ 0x130B, 0x0000 }, /* 0000,130B,Cutoff,invert=0 */
		{ 0x130C, 0x7FFF }, /* 7FFF,130C,0dB,invert=0 */
		{ 0x130D, 0x47AD }, /* 47AD,130D,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x130E, 0xB98F }, /* B98F,130E,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x130F, 0x2A93 }, /* 2A93,130F,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1310, 0x0000 }, /* 0000,1310,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1311, 0x0000 }, /* 0000,1311,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1312, 0x7E5F }, /* 7E5F,1312,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1313, 0x8B66 }, /* 8B66,1313,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1314, 0x72F9 }, /* 72F9,1314,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1315, 0x0000 }, /* 0000,1315,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1316, 0x0000 }, /* 0000,1316,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1317, 0x7FFF }, /* 7FFF,1317,0dB,invert=0 */
		{ 0x1318, 0x0000 }, /* 0000,1318,Cutoff,invert=0 */
		{ 0x1319, 0x0000 }, /* 0000,1319,Cutoff,invert=0 */
		{ 0x131A, 0x0035 }, /* 0035,131A,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131B, 0x006A }, /* 006A,131B,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131C, 0x716F }, /* 716F,131C,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131D, 0x0035 }, /* 0035,131D,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131E, 0xCDBC }, /* CDBC,131E,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x131F, 0x0169 }, /* 0169,131F,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1320, 0x0169 }, /* 0169,1320,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1321, 0x7FE9 }, /* 7FE9,1321,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1322, 0x0000 }, /* 0000,1322,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1323, 0x0000 }, /* 0000,1323,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1324, 0x5391 }, /* 5391,1324,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1325, 0xACF2 }, /* ACF2,1325,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1326, 0x7F7D }, /* 7F7D,1326,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1327, 0x0000 }, /* 0000,1327,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1328, 0x0000 }, /* 0000,1328,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1329, 0x7FFF }, /* 7FFF,1329,0dB,invert=0 */
		{ 0x132A, 0x7FFF }, /* 7FFF,132A,0dB,invert=0 */
		{ 0x132B, 0x0000 }, /* 0000,132B,Cutoff,invert=0 */
		{ 0x132C, 0x0000 }, /* 0000,132C,Cutoff,invert=0 */
		{ 0x132D, 0x0000 }, /* 0000,132D,Cutoff,invert=0 */
		{ 0x132E, 0x0000 }, /* 0000,132E,Cutoff,invert=0 */
		{ 0x132F, 0x7FFF }, /* 7FFF,132F,Through,0dB,fs/1,invert=0 */
		{ 0x1330, 0x0000 }, /* 0000,1330,Through,0dB,fs/1,invert=0 */
		{ 0x1331, 0x0000 }, /* 0000,1331,Through,0dB,fs/1,invert=0 */
		{ 0x1332, 0x378D }, /* 378D,1332,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1333, 0x9DB1 }, /* 9DB1,1333,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1334, 0x624F }, /* 624F,1334,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1335, 0x30EB }, /* 30EB,1335,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1336, 0xD788 }, /* D788,1336,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1337, 0x0D17 }, /* 0D17,1337,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1338, 0x0D17 }, /* 0D17,1338,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1339, 0x65D1 }, /* 65D1,1339,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133A, 0x0000 }, /* 0000,133A,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133B, 0x0000 }, /* 0000,133B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133C, 0x7FFF }, /* 7FFF,133C,0dB,invert=0 */
		{ 0x133D, 0x0000 }, /* 0000,133D,Cutoff,invert=0 */
		{ 0x133E, 0x0000 }, /* 0000,133E,Cutoff,invert=0 */
		{ 0x133F, 0x7FFF }, /* 7FFF,133F,0dB,invert=1 */
		{ 0x1341, 0x7FFF }, /* 7FFF,1341,0dB,invert=0 */
		{ 0x1342, 0x7FFF }, /* 7FFF,1342,0dB,invert=0 */
		{ 0x1345, 0x7FFF }, /* 7FFF,1345,Through,0dB,fs/1,invert=0 */
		{ 0x1346, 0x0000 }, /* 0000,1346,Through,0dB,fs/1,invert=0 */
		{ 0x1347, 0x0000 }, /* 0000,1347,Through,0dB,fs/1,invert=0 */
		{ 0x1348, 0x0000 }, /* 0000,1348,Cutoff,invert=0 */
		{ 0x1349, 0x5A9D }, /* 5A9D,1349,-3dB,invert=0 */
		{ 0x134A, 0x0000 }, /* 0000,134A,Cutoff,invert=0 */
		{ 0x134B, 0x0000 }, /* 0000,134B,Cutoff,invert=0 */
		{ 0x134C, 0x7FFF }, /* 7FFF,134C,0dB,invert=0 */
		{ 0x134D, 0x47AD }, /* 47AD,134D,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x134E, 0xB98F }, /* B98F,134E,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x134F, 0x2A93 }, /* 2A93,134F,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1350, 0x0000 }, /* 0000,1350,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1351, 0x0000 }, /* 0000,1351,HBF,65Hz,1500Hz,2.5dB,fs/1,invert=0 */
		{ 0x1352, 0x7E5F }, /* 7E5F,1352,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1353, 0x8B66 }, /* 8B66,1353,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1354, 0x72F9 }, /* 72F9,1354,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1355, 0x0000 }, /* 0000,1355,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1356, 0x0000 }, /* 0000,1356,HBF,300Hz,400Hz,0dB,fs/1,invert=0 */
		{ 0x1357, 0x7FFF }, /* 7FFF,1357,0dB,invert=0 */
		{ 0x1358, 0x0000 }, /* 0000,1358,Cutoff,invert=0 */
		{ 0x1359, 0x0000 }, /* 0000,1359,Cutoff,invert=0 */
		{ 0x135A, 0x0035 }, /* 0035,135A,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135B, 0x006A }, /* 006A,135B,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135C, 0x716F }, /* 716F,135C,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135D, 0x0035 }, /* 0035,135D,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135E, 0xCDBC }, /* CDBC,135E,LPF,450Hz,0dB,0,fs/1,invert=0 */
		{ 0x135F, 0x0169 }, /* 0169,135F,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1360, 0x0169 }, /* 0169,1360,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1361, 0x7FE9 }, /* 7FE9,1361,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1362, 0x0000 }, /* 0000,1362,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1363, 0x0000 }, /* 0000,1363,LPF,0.65Hz,30dB,fs/4,invert=0 */
		{ 0x1364, 0x5391 }, /* 5391,1364,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1365, 0xACF2 }, /* ACF2,1365,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1366, 0x7F7D }, /* 7F7D,1366,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1367, 0x0000 }, /* 0000,1367,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1368, 0x0000 }, /* 0000,1368,LBF,15Hz,23Hz,0dB,fs/1,invert=0 */
		{ 0x1369, 0x7FFF }, /* 7FFF,1369,0dB,invert=0 */
		{ 0x136A, 0x7FFF }, /* 7FFF,136A,0dB,invert=0 */
		{ 0x136B, 0x0000 }, /* 0000,136B,Cutoff,invert=0 */
		{ 0x136C, 0x0000 }, /* 0000,136C,Cutoff,invert=0 */
		{ 0x136D, 0x0000 }, /* 0000,136D,Cutoff,invert=0 */
		{ 0x136E, 0x0000 }, /* 0000,136E,Cutoff,invert=0 */
		{ 0x136F, 0x7FFF }, /* 7FFF,136F,Through,0dB,fs/1,invert=0 */
		{ 0x1370, 0x0000 }, /* 0000,1370,Through,0dB,fs/1,invert=0 */
		{ 0x1371, 0x0000 }, /* 0000,1371,Through,0dB,fs/1,invert=0 */
		{ 0x1372, 0x378D }, /* 378D,1372,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1373, 0x9DB1 }, /* 9DB1,1373,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1374, 0x624F }, /* 624F,1374,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1375, 0x30EB }, /* 30EB,1375,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1376, 0xD788 }, /* D788,1376,PKF,1300Hz,-11dB,3,fs/1,invert=0 */
		{ 0x1377, 0x0D17 }, /* 0D17,1377,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1378, 0x0D17 }, /* 0D17,1378,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1379, 0x65D1 }, /* 65D1,1379,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137A, 0x0000 }, /* 0000,137A,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137B, 0x0000 }, /* 0000,137B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137C, 0x7FFF }, /* 7FFF,137C,0dB,invert=0 */
		{ 0x137D, 0x0000 }, /* 0000,137D,Cutoff,invert=0 */
		{ 0x137E, 0x0000 }, /* 0000,137E,Cutoff,invert=0 */
		{ 0x137F, 0x7FFF }, /* 7FFF,137F,0dB,invert=0 */
		{ 0xFFFF, 0xFFFF }
	},
	{
		/* VER 03: FC filename: Mitsumi_130501 */
		{ 0x1128, 0x0000 }, /* 0000,1128,Cutoff,invert=0 */
		{ 0x1168, 0x0000 }, /* 0000,1168,Cutoff,invert=0 */
		{ 0x11E0, 0x7FFF }, /* 7FFF,11E0,0dB,invert=0 */
		{ 0x11E1, 0x7FFF }, /* 7FFF,11E1,0dB,invert=0 */
		{ 0x11E2, 0x7FFF }, /* 7FFF,11E2,0dB,invert=0 */
		{ 0x11E3, 0x7FFF }, /* 7FFF,11E3,0dB,invert=0 */
		{ 0x11E4, 0x7FFF }, /* 7FFF,11E4,0dB,invert=0 */
		{ 0x11E5, 0x7FFF }, /* 7FFF,11E5,0dB,invert=0 */
		{ 0x12E0, 0x7FFF }, /* 7FFF,12E0,Through,0dB,fs/1,invert=0 */
		{ 0x12E1, 0x0000 }, /* 0000,12E1,Through,0dB,fs/1,invert=0 */
		{ 0x12E2, 0x0000 }, /* 0000,12E2,Through,0dB,fs/1,invert=0 */
		{ 0x12E3, 0x7FFF }, /* 7FFF,12E3,Through,0dB,fs/1,invert=0 */
		{ 0x12E4, 0x0000 }, /* 0000,12E4,Through,0dB,fs/1,invert=0 */
		{ 0x12E5, 0x0000 }, /* 0000,12E5,Through,0dB,fs/1,invert=0 */
		{ 0x12E6, 0x7FFF }, /* 7FFF,12E6,0dB,invert=0 */
		{ 0x1301, 0x7FFF }, /* 7FFF,1301,0dB,invert=0 */
		{ 0x1302, 0x7FFF }, /* 7FFF,1302,0dB,invert=0 */
		{ 0x1305, 0x7FFF }, /* 7FFF,1305,Through,0dB,fs/1,invert=0 */
		{ 0x1306, 0x0000 }, /* 0000,1306,Through,0dB,fs/1,invert=0 */
		{ 0x1307, 0x0000 }, /* 0000,1307,Through,0dB,fs/1,invert=0 */
		{ 0x1308, 0x0000 }, /* 0000,1308,Cutoff,invert=0 */
		{ 0x1309, 0x5A9D }, /* 5A9D,1309,-3dB,invert=0 */
		{ 0x130A, 0x0000 }, /* 0000,130A,Cutoff,invert=0 */
		{ 0x130B, 0x0000 }, /* 0000,130B,Cutoff,invert=0 */
		{ 0x130C, 0x7FFF }, /* 7FFF,130C,0dB,invert=0 */
		{ 0x130D, 0x4A41 }, /* 4A41,130D,HBF,60Hz,700Hz,2dB,fs/1,invert=0 */
		{ 0x130E, 0xB6EF }, /* B6EF,130E,HBF,60Hz,700Hz,2dB,fs/1,invert=0 */
		{ 0x130F, 0x3505 }, /* 3505,130F,HBF,60Hz,700Hz,2dB,fs/1,invert=0 */
		{ 0x1310, 0x0000 }, /* 0000,1310,HBF,60Hz,700Hz,2dB,fs/1,invert=0 */
		{ 0x1311, 0x0000 }, /* 0000,1311,HBF,60Hz,700Hz,2dB,fs/1,invert=0 */
		{ 0x1312, 0x7FFF }, /* 7FFF,1312,Through,0dB,fs/1,invert=0 */
		{ 0x1313, 0x0000 }, /* 0000,1313,Through,0dB,fs/1,invert=0 */
		{ 0x1314, 0x0000 }, /* 0000,1314,Through,0dB,fs/1,invert=0 */
		{ 0x1315, 0x0000 }, /* 0000,1315,Through,0dB,fs/1,invert=0 */
		{ 0x1316, 0x0000 }, /* 0000,1316,Through,0dB,fs/1,invert=0 */
		{ 0x1317, 0x7FFF }, /* 7FFF,1317,0dB,invert=0 */
		{ 0x1318, 0x0000 }, /* 0000,1318,Cutoff,invert=0 */
		{ 0x1319, 0x0000 }, /* 0000,1319,Cutoff,invert=0 */
		{ 0x131A, 0x002B }, /* 002B,131A,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x131B, 0x0055 }, /* 0055,131B,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x131C, 0x72F9 }, /* 72F9,131C,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x131D, 0x002B }, /* 002B,131D,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x131E, 0xCC5D }, /* CC5D,131E,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x131F, 0x020D }, /* 020D,131F,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1320, 0x020D }, /* 020D,1320,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1321, 0x7FCB }, /* 7FCB,1321,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1322, 0x0000 }, /* 0000,1322,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1323, 0x0000 }, /* 0000,1323,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1324, 0x334D }, /* 334D,1324,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1325, 0xCD0A }, /* CD0A,1325,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1326, 0x7FA9 }, /* 7FA9,1326,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1327, 0x0000 }, /* 0000,1327,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1328, 0x0000 }, /* 0000,1328,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1329, 0x7FFF }, /* 7FFF,1329,0dB,invert=0 */
		{ 0x132A, 0x7FFF }, /* 7FFF,132A,0dB,invert=0 */
		{ 0x132B, 0x0000 }, /* 0000,132B,Cutoff,invert=0 */
		{ 0x132C, 0x0000 }, /* 0000,132C,Cutoff,invert=0 */
		{ 0x132D, 0x0000 }, /* 0000,132D,Cutoff,invert=0 */
		{ 0x132E, 0x0000 }, /* 0000,132E,Cutoff,invert=0 */
		{ 0x132F, 0x7FFF }, /* 7FFF,132F,Through,0dB,fs/1,invert=0 */
		{ 0x1330, 0x0000 }, /* 0000,1330,Through,0dB,fs/1,invert=0 */
		{ 0x1331, 0x0000 }, /* 0000,1331,Through,0dB,fs/1,invert=0 */
		{ 0x1332, 0x3853 }, /* 3853,1332,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1333, 0x9CAB }, /* 9CAB,1333,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1334, 0x6355 }, /* 6355,1334,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1335, 0x313B }, /* 313B,1335,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1336, 0xD672 }, /* D672,1336,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1337, 0x0D17 }, /* 0D17,1337,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1338, 0x0D17 }, /* 0D17,1338,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1339, 0x65D1 }, /* 65D1,1339,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133A, 0x0000 }, /* 0000,133A,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133B, 0x0000 }, /* 0000,133B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133C, 0x7FFF }, /* 7FFF,133C,0dB,invert=0 */
		{ 0x133D, 0x0000 }, /* 0000,133D,Cutoff,invert=0 */
		{ 0x133E, 0x0000 }, /* 0000,133E,Cutoff,invert=0 */
		{ 0x133F, 0x7FFF }, /* 7FFF,133F,0dB,invert=0 */
		{ 0x1341, 0x7FFF }, /* 7FFF,1341,0dB,invert=0 */
		{ 0x1342, 0x7FFF }, /* 7FFF,1342,0dB,invert=0 */
		{ 0x1345, 0x7FFF }, /* 7FFF,1345,Through,0dB,fs/1,invert=0 */
		{ 0x1346, 0x0000 }, /* 0000,1346,Through,0dB,fs/1,invert=0 */
		{ 0x1347, 0x0000 }, /* 0000,1347,Through,0dB,fs/1,invert=0 */
		{ 0x1348, 0x0000 }, /* 0000,1348,Cutoff,invert=0 */
		{ 0x1349, 0x5A9D }, /* 5A9D,1349,-3dB,invert=0 */
		{ 0x134A, 0x0000 }, /* 0000,134A,Cutoff,invert=0 */
		{ 0x134B, 0x0000 }, /* 0000,134B,Cutoff,invert=0 */
		{ 0x134C, 0x7FFF }, /* 7FFF,134C,0dB,invert=0 */
		{ 0x134D, 0x4A41 }, /* 4A41,134D,HBF,60Hz,700Hz,2dB,fs/1,invert=0 */
		{ 0x134E, 0xB6EF }, /* B6EF,134E,HBF,60Hz,700Hz,2dB,fs/1,invert=0 */
		{ 0x134F, 0x3505 }, /* 3505,134F,HBF,60Hz,700Hz,2dB,fs/1,invert=0 */
		{ 0x1350, 0x0000 }, /* 0000,1350,HBF,60Hz,700Hz,2dB,fs/1,invert=0 */
		{ 0x1351, 0x0000 }, /* 0000,1351,HBF,60Hz,700Hz,2dB,fs/1,invert=0 */
		{ 0x1352, 0x7FFF }, /* 7FFF,1352,Through,0dB,fs/1,invert=0 */
		{ 0x1353, 0x0000 }, /* 0000,1353,Through,0dB,fs/1,invert=0 */
		{ 0x1354, 0x0000 }, /* 0000,1354,Through,0dB,fs/1,invert=0 */
		{ 0x1355, 0x0000 }, /* 0000,1355,Through,0dB,fs/1,invert=0 */
		{ 0x1356, 0x0000 }, /* 0000,1356,Through,0dB,fs/1,invert=0 */
		{ 0x1357, 0x7FFF }, /* 7FFF,1357,0dB,invert=0 */
		{ 0x1358, 0x0000 }, /* 0000,1358,Cutoff,invert=0 */
		{ 0x1359, 0x0000 }, /* 0000,1359,Cutoff,invert=0 */
		{ 0x135A, 0x002B }, /* 002B,135A,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x135B, 0x0055 }, /* 0055,135B,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x135C, 0x72F9 }, /* 72F9,135C,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x135D, 0x002B }, /* 002B,135D,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x135E, 0xCC5D }, /* CC5D,135E,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x135F, 0x020D }, /* 020D,135F,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1360, 0x020D }, /* 020D,1360,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1361, 0x7FCB }, /* 7FCB,1361,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1362, 0x0000 }, /* 0000,1362,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1363, 0x0000 }, /* 0000,1363,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1364, 0x334D }, /* 334D,1364,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1365, 0xCD0A }, /* CD0A,1365,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1366, 0x7FA9 }, /* 7FA9,1366,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1367, 0x0000 }, /* 0000,1367,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1368, 0x0000 }, /* 0000,1368,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1369, 0x7FFF }, /* 7FFF,1369,0dB,invert=0 */
		{ 0x136A, 0x7FFF }, /* 7FFF,136A,0dB,invert=0 */
		{ 0x136B, 0x0000 }, /* 0000,136B,Cutoff,invert=0 */
		{ 0x136C, 0x0000 }, /* 0000,136C,Cutoff,invert=0 */
		{ 0x136D, 0x0000 }, /* 0000,136D,Cutoff,invert=0 */
		{ 0x136E, 0x0000 }, /* 0000,136E,Cutoff,invert=0 */
		{ 0x136F, 0x7FFF }, /* 7FFF,136F,Through,0dB,fs/1,invert=0 */
		{ 0x1370, 0x0000 }, /* 0000,1370,Through,0dB,fs/1,invert=0 */
		{ 0x1371, 0x0000 }, /* 0000,1371,Through,0dB,fs/1,invert=0 */
		{ 0x1372, 0x3853 }, /* 3853,1372,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1373, 0x9CAB }, /* 9CAB,1373,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1374, 0x6355 }, /* 6355,1374,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1375, 0x313B }, /* 313B,1375,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1376, 0xD672 }, /* D672,1376,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1377, 0x0D17 }, /* 0D17,1377,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1378, 0x0D17 }, /* 0D17,1378,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1379, 0x65D1 }, /* 65D1,1379,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137A, 0x0000 }, /* 0000,137A,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137B, 0x0000 }, /* 0000,137B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137C, 0x7FFF }, /* 7FFF,137C,0dB,invert=0 */
		{ 0x137D, 0x0000 }, /* 0000,137D,Cutoff,invert=0 */
		{ 0x137E, 0x0000 }, /* 0000,137E,Cutoff,invert=0 */
		{ 0x137F, 0x7FFF }, /* 7FFF,137F,0dB,invert=0 */
		{ 0xFFFF, 0xFFFF }
	},
	{
		/* VER 04: FC filename: Mitsumi_130501 */
		{ 0x1128, 0x0000 }, /* 0000,1128,Cutoff,invert=0 */
		{ 0x1168, 0x0000 }, /* 0000,1168,Cutoff,invert=0 */
		{ 0x11E0, 0x7FFF }, /* 7FFF,11E0,0dB,invert=0 */
		{ 0x11E1, 0x7FFF }, /* 7FFF,11E1,0dB,invert=0 */
		{ 0x11E2, 0x7FFF }, /* 7FFF,11E2,0dB,invert=0 */
		{ 0x11E3, 0x7FFF }, /* 7FFF,11E3,0dB,invert=0 */
		{ 0x11E4, 0x7FFF }, /* 7FFF,11E4,0dB,invert=0 */
		{ 0x11E5, 0x7FFF }, /* 7FFF,11E5,0dB,invert=0 */
		{ 0x12E0, 0x7FFF }, /* 7FFF,12E0,Through,0dB,fs/1,invert=0 */
		{ 0x12E1, 0x0000 }, /* 0000,12E1,Through,0dB,fs/1,invert=0 */
		{ 0x12E2, 0x0000 }, /* 0000,12E2,Through,0dB,fs/1,invert=0 */
		{ 0x12E3, 0x7FFF }, /* 7FFF,12E3,Through,0dB,fs/1,invert=0 */
		{ 0x12E4, 0x0000 }, /* 0000,12E4,Through,0dB,fs/1,invert=0 */
		{ 0x12E5, 0x0000 }, /* 0000,12E5,Through,0dB,fs/1,invert=0 */
		{ 0x12E6, 0x7FFF }, /* 7FFF,12E6,0dB,invert=0 */
		{ 0x1301, 0x7FFF }, /* 7FFF,1301,0dB,invert=0 */
		{ 0x1302, 0x7FFF }, /* 7FFF,1302,0dB,invert=0 */
		{ 0x1305, 0x7FFF }, /* 7FFF,1305,Through,0dB,fs/1,invert=0 */
		{ 0x1306, 0x0000 }, /* 0000,1306,Through,0dB,fs/1,invert=0 */
		{ 0x1307, 0x0000 }, /* 0000,1307,Through,0dB,fs/1,invert=0 */
		{ 0x1308, 0x0000 }, /* 0000,1308,Cutoff,invert=0 */
		{ 0x1309, 0x5A9D }, /* 5A9D,1309,-3dB,invert=0 */
		{ 0x130A, 0x0000 }, /* 0000,130A,Cutoff,invert=0 */
		{ 0x130B, 0x0000 }, /* 0000,130B,Cutoff,invert=0 */
		{ 0x130C, 0x7FFF }, /* 7FFF,130C,0dB,invert=0 */
		{ 0x130D, 0x46EB }, /* 46EB,130D,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0 */
		{ 0x130E, 0xBA1D }, /* BA1D,130E,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0 */
		{ 0x130F, 0x3679 }, /* 3679,130F,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0 */
		{ 0x1310, 0x0000 }, /* 0000,1310,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0 */
		{ 0x1311, 0x0000 }, /* 0000,1311,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0 */
		{ 0x1312, 0x43B3 }, /* 43B3,1312,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0 */
		{ 0x1313, 0xBCD7 }, /* BCD7,1313,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0 */
		{ 0x1314, 0x3F51 }, /* 3F51,1314,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0 */
		{ 0x1315, 0x0000 }, /* 0000,1315,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0 */
		{ 0x1316, 0x0000 }, /* 0000,1316,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0 */
		{ 0x1317, 0x7FFF }, /* 7FFF,1317,0dB,invert=0 */
		{ 0x1318, 0x0000 }, /* 0000,1318,Cutoff,invert=0 */
		{ 0x1319, 0x0000 }, /* 0000,1319,Cutoff,invert=0 */
		{ 0x131A, 0x002B }, /* 002B,131A,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x131B, 0x0055 }, /* 0055,131B,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x131C, 0x72F9 }, /* 72F9,131C,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x131D, 0x002B }, /* 002B,131D,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x131E, 0xCC5D }, /* CC5D,131E,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x131F, 0x020D }, /* 020D,131F,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1320, 0x020D }, /* 020D,1320,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1321, 0x7FCB }, /* 7FCB,1321,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1322, 0x0000 }, /* 0000,1322,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1323, 0x0000 }, /* 0000,1323,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1324, 0x334D }, /* 334D,1324,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1325, 0xCD0A }, /* CD0A,1325,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1326, 0x7FA9 }, /* 7FA9,1326,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1327, 0x0000 }, /* 0000,1327,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1328, 0x0000 }, /* 0000,1328,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1329, 0x7FFF }, /* 7FFF,1329,0dB,invert=0 */
		{ 0x132A, 0x7FFF }, /* 7FFF,132A,0dB,invert=0 */
		{ 0x132B, 0x0000 }, /* 0000,132B,Cutoff,invert=0 */
		{ 0x132C, 0x0000 }, /* 0000,132C,Cutoff,invert=0 */
		{ 0x132D, 0x0000 }, /* 0000,132D,Cutoff,invert=0 */
		{ 0x132E, 0x0000 }, /* 0000,132E,Cutoff,invert=0 */
		{ 0x132F, 0x7485 }, /* 7485,132F,LBF,100Hz,110Hz,0dB,fs/1,invert=0 */
		{ 0x1330, 0x8EDF }, /* 8EDF,1330,LBF,100Hz,110Hz,0dB,fs/1,invert=0 */
		{ 0x1331, 0x7C9D }, /* 7C9D,1331,LBF,100Hz,110Hz,0dB,fs/1,invert=0 */
		{ 0x1332, 0x3853 }, /* 3853,1332,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1333, 0x9CAB }, /* 9CAB,1333,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1334, 0x6355 }, /* 6355,1334,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1335, 0x313B }, /* 313B,1335,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1336, 0xD672 }, /* D672,1336,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1337, 0x0D17 }, /* 0D17,1337,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1338, 0x0D17 }, /* 0D17,1338,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1339, 0x65D1 }, /* 65D1,1339,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133A, 0x0000 }, /* 0000,133A,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133B, 0x0000 }, /* 0000,133B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x133C, 0x7FFF }, /* 7FFF,133C,0dB,invert=0 */
		{ 0x133D, 0x0000 }, /* 0000,133D,Cutoff,invert=0 */
		{ 0x133E, 0x0000 }, /* 0000,133E,Cutoff,invert=0 */
		{ 0x133F, 0x7FFF }, /* 7FFF,133F,0dB,invert=0 */
		{ 0x1341, 0x7FFF }, /* 7FFF,1341,0dB,invert=0 */
		{ 0x1342, 0x7FFF }, /* 7FFF,1342,0dB,invert=0 */
		{ 0x1345, 0x7FFF }, /* 7FFF,1345,Through,0dB,fs/1,invert=0 */
		{ 0x1346, 0x0000 }, /* 0000,1346,Through,0dB,fs/1,invert=0 */
		{ 0x1347, 0x0000 }, /* 0000,1347,Through,0dB,fs/1,invert=0 */
		{ 0x1348, 0x0000 }, /* 0000,1348,Cutoff,invert=0 */
		{ 0x1349, 0x5A9D }, /* 5A9D,1349,-3dB,invert=0 */
		{ 0x134A, 0x0000 }, /* 0000,134A,Cutoff,invert=0 */
		{ 0x134B, 0x0000 }, /* 0000,134B,Cutoff,invert=0 */
		{ 0x134C, 0x7FFF }, /* 7FFF,134C,0dB,invert=0 */
		{ 0x134D, 0x46EB }, /* 46EB,134D,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0 */
		{ 0x134E, 0xBA1D }, /* BA1D,134E,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0 */
		{ 0x134F, 0x3679 }, /* 3679,134F,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0 */
		{ 0x1350, 0x0000 }, /* 0000,1350,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0 */
		{ 0x1351, 0x0000 }, /* 0000,1351,HBF,55Hz,600Hz,1.5dB,fs/1,invert=0 */
		{ 0x1352, 0x43B3 }, /* 43B3,1352,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0 */
		{ 0x1353, 0xBCD7 }, /* BCD7,1353,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0 */
		{ 0x1354, 0x3F51 }, /* 3F51,1354,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0 */
		{ 0x1355, 0x0000 }, /* 0000,1355,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0 */
		{ 0x1356, 0x0000 }, /* 0000,1356,HBF,30Hz,40Hz,0.5dB,fs/1,invert=0 */
		{ 0x1357, 0x7FFF }, /* 7FFF,1357,0dB,invert=0 */
		{ 0x1358, 0x0000 }, /* 0000,1358,Cutoff,invert=0 */
		{ 0x1359, 0x0000 }, /* 0000,1359,Cutoff,invert=0 */
		{ 0x135A, 0x002B }, /* 002B,135A,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x135B, 0x0055 }, /* 0055,135B,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x135C, 0x72F9 }, /* 72F9,135C,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x135D, 0x002B }, /* 002B,135D,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x135E, 0xCC5D }, /* CC5D,135E,LPF,400Hz,0dB,0,fs/1,invert=0 */
		{ 0x135F, 0x020D }, /* 020D,135F,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1360, 0x020D }, /* 020D,1360,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1361, 0x7FCB }, /* 7FCB,1361,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1362, 0x0000 }, /* 0000,1362,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1363, 0x0000 }, /* 0000,1363,LPF,1.5Hz,26dB,fs/4,invert=0 */
		{ 0x1364, 0x334D }, /* 334D,1364,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1365, 0xCD0A }, /* CD0A,1365,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1366, 0x7FA9 }, /* 7FA9,1366,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1367, 0x0000 }, /* 0000,1367,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1368, 0x0000 }, /* 0000,1368,LBF,10Hz,25Hz,0dB,fs/1,invert=0 */
		{ 0x1369, 0x7FFF }, /* 7FFF,1369,0dB,invert=0 */
		{ 0x136A, 0x7FFF }, /* 7FFF,136A,0dB,invert=0 */
		{ 0x136B, 0x0000 }, /* 0000,136B,Cutoff,invert=0 */
		{ 0x136C, 0x0000 }, /* 0000,136C,Cutoff,invert=0 */
		{ 0x136D, 0x0000 }, /* 0000,136D,Cutoff,invert=0 */
		{ 0x136E, 0x0000 }, /* 0000,136E,Cutoff,invert=0 */
		{ 0x136F, 0x7485 }, /* 7485,136F,LBF,100Hz,110Hz,0dB,fs/1,invert=0 */
		{ 0x1370, 0x8EDF }, /* 8EDF,1370,LBF,100Hz,110Hz,0dB,fs/1,invert=0 */
		{ 0x1371, 0x7C9D }, /* 7C9D,1371,LBF,100Hz,110Hz,0dB,fs/1,invert=0 */
		{ 0x1372, 0x3853 }, /* 3853,1372,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1373, 0x9CAB }, /* 9CAB,1373,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1374, 0x6355 }, /* 6355,1374,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1375, 0x313B }, /* 313B,1375,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1376, 0xD672 }, /* D672,1376,PKF,1300Hz,-10dB,3,fs/1,invert=0 */
		{ 0x1377, 0x0D17 }, /* 0D17,1377,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1378, 0x0D17 }, /* 0D17,1378,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x1379, 0x65D1 }, /* 65D1,1379,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137A, 0x0000 }, /* 0000,137A,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137B, 0x0000 }, /* 0000,137B,LPF,850Hz,0dB,fs/1,invert=0 */
		{ 0x137C, 0x7FFF }, /* 7FFF,137C,0dB,invert=0 */
		{ 0x137D, 0x0000 }, /* 0000,137D,Cutoff,invert=0 */
		{ 0x137E, 0x0000 }, /* 0000,137E,Cutoff,invert=0 */
		{ 0x137F, 0x7FFF }, /* 7FFF,137F,0dB,invert=0 */
		{ 0xFFFF, 0xFFFF }
	}
};

/* 32bit */
const struct STGYRFIL CsGyrFil[][GYRFILTAB] = {
	{
		/* VER 00: FC filename: Mitsumi_130315_Y */
		{ 0x1800, 0x3F800000 }, /* 3F800000,1800,0dB,invert=0 */
		{ 0x1801, 0x3F800000 }, /* 3F800000,1801,Through,0dB,fs/1,invert=0 */
		{ 0x1802, 0x00000000 }, /* 00000000,1802,Through,0dB,fs/1,invert=0 */
		{ 0x1803, 0x39A89AEA }, /* 39A89AEA,1803,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x1804, 0x39A89AEA }, /* 39A89AEA,1804,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x1805, 0x3F7FD5D9 }, /* 3F7FD5D9,1805,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x1806, 0x00000000 }, /* 00000000,1806,Through,0dB,fs/1,invert=0 */
		{ 0x1807, 0x00000000 }, /* 00000000,1807,Cutoff,invert=0 */
		{ 0x180A, 0x39A89AEA }, /* 39A89AEA,180A,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x180B, 0x39A89AEA }, /* 39A89AEA,180B,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x180C, 0x3F7FD5D9 }, /* 3F7FD5D9,180C,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x180D, 0x3F800000 }, /* 3F800000,180D,0dB,invert=0 */
		{ 0x180E, 0xBF800000 }, /* BF800000,180E,0dB,invert=1 */
		{ 0x180F, 0x3FFF64C1 }, /* 3FFF64C1,180F,6dB,invert=0 */
		{ 0x1810, 0x3F800000 }, /* 3F800000,1810,0dB,invert=0 */
		{ 0x1811, 0x3F800000 }, /* 3F800000,1811,0dB,invert=0 */
		{ 0x1812, 0x3B02C2F2 }, /* 3B02C2F2,1812,Free,fs/2,invert=0 */
		{ 0x1813, 0x00000000 }, /* 00000000,1813,Free,fs/2,invert=0 */
		{ 0x1814, 0x3F7FF700 }, /* 3F7FF700,1814,Free,fs/2,invert=0 */
		{ 0x1815, 0x419E43FE }, /* 419E43FE,1815,HBF,67Hz,2000Hz,29.5dB,fs/2,invert=0 */
		{ 0x1816, 0xC198AE3F }, /* C198AE3F,1816,HBF,67Hz,2000Hz,29.5dB,fs/2,invert=0 */
		{ 0x1817, 0x3E9A9997 }, /* 3E9A9997,1817,HBF,67Hz,2000Hz,29.5dB,fs/2,invert=0 */
		{ 0x1818, 0x3F80BF0F }, /* 3F80BF0F,1818,LBF,0.05Hz,0.34Hz,16.7dB,fs/2,invert=0 */
		{ 0x1819, 0xBF80B90D }, /* BF80B90D,1819,LBF,0.05Hz,0.34Hz,16.7dB,fs/2,invert=0 */
		{ 0x181A, 0x3F7FFE3E }, /* 3F7FFE3E,181A,LBF,0.05Hz,0.34Hz,16.7dB,fs/2,invert=0 */
		{ 0x181B, 0x00000000 }, /* 00000000,181B,Cutoff,invert=0 */
		{ 0x181C, 0x3F800000 }, /* 3F800000,181C,0dB,invert=0 */
		{ 0x181D, 0x3F800000 }, /* 3F800000,181D,Through,0dB,fs/2,invert=0 */
		{ 0x181E, 0x00000000 }, /* 00000000,181E,Through,0dB,fs/2,invert=0 */
		{ 0x181F, 0x00000000 }, /* 00000000,181F,Through,0dB,fs/2,invert=0 */
		{ 0x1820, 0x390C87D8 }, /* 390C87D8,1820,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1821, 0x390C87D8 }, /* 390C87D8,1821,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1822, 0x3F7FEE6F }, /* 3F7FEE6F,1822,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1823, 0x390C87D8 }, /* 390C87D8,1823,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1824, 0x390C87D8 }, /* 390C87D8,1824,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1825, 0x3F7FEE6F }, /* 3F7FEE6F,1825,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1826, 0x3F800000 }, /* 3F800000,1826,0dB,invert=0 */
		{ 0x1827, 0xBF800000 }, /* BF800000,1827,0dB,invert=1 */
		{ 0x1828, 0x3F800000 }, /* 3F800000,1828,0dB,invert=0 */
		{ 0x1829, 0x40A06142 }, /* 40A06142,1829,14dB,invert=0 */
		{ 0x182A, 0x3F800000 }, /* 3F800000,182A,0dB,invert=0 */
		{ 0x182B, 0x3F800000 }, /* 3F800000,182B,0dB,invert=0 */
		{ 0x182C, 0x00000000 }, /* 00000000,182C,Cutoff,invert=0 */
		{ 0x1830, 0x3F800000 }, /* 3F800000,1830,Through,0dB,fs/2,invert=0 */
		{ 0x1831, 0x00000000 }, /* 00000000,1831,Through,0dB,fs/2,invert=0 */
		{ 0x1832, 0x00000000 }, /* 00000000,1832,Through,0dB,fs/2,invert=0 */
		{ 0x1833, 0x00000000 }, /* 00000000,1833,Through,0dB,fs/2,invert=0 */
		{ 0x1834, 0x00000000 }, /* 00000000,1834,Through,0dB,fs/2,invert=0 */
		{ 0x1835, 0x00000000 }, /* 00000000,1835,Cutoff,invert=0 */
		{ 0x1838, 0x3F800000 }, /* 3F800000,1838,0dB,invert=0 */
		{ 0x1839, 0x3C58B55D }, /* 3C58B55D,1839,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183A, 0x3C58B55D }, /* 3C58B55D,183A,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183B, 0x3F793A55 }, /* 3F793A55,183B,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183C, 0x3C58B55D }, /* 3C58B55D,183C,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183D, 0x3C58B55D }, /* 3C58B55D,183D,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183E, 0x3F793A55 }, /* 3F793A55,183E,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x1900, 0x3F800000 }, /* 3F800000,1900,0dB,invert=0 */
		{ 0x1901, 0x3F800000 }, /* 3F800000,1901,Through,0dB,fs/1,invert=0 */
		{ 0x1902, 0x00000000 }, /* 00000000,1902,Through,0dB,fs/1,invert=0 */
		{ 0x1903, 0x39A89AEA }, /* 39A89AEA,1903,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x1904, 0x39A89AEA }, /* 39A89AEA,1904,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x1905, 0x3F7FD5D9 }, /* 3F7FD5D9,1905,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x1906, 0x00000000 }, /* 00000000,1906,Through,0dB,fs/1,invert=0 */
		{ 0x1907, 0x00000000 }, /* 00000000,1907,Cutoff,invert=0 */
		{ 0x190A, 0x39A89AEA }, /* 39A89AEA,190A,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x190B, 0x39A89AEA }, /* 39A89AEA,190B,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x190C, 0x3F7FD5D9 }, /* 3F7FD5D9,190C,LPF,1.2Hz,0dB,fs/2,invert=0 */
		{ 0x190D, 0x3F800000 }, /* 3F800000,190D,0dB,invert=0 */
		{ 0x190E, 0xBF800000 }, /* BF800000,190E,0dB,invert=1 */
		{ 0x190F, 0x3FFF64C1 }, /* 3FFF64C1,190F,6dB,invert=0 */
		{ 0x1910, 0x3F800000 }, /* 3F800000,1910,0dB,invert=0 */
		{ 0x1911, 0x3F800000 }, /* 3F800000,1911,0dB,invert=0 */
		{ 0x1912, 0x3B02C2F2 }, /* 3B02C2F2,1912,Free,fs/2,invert=0 */
		{ 0x1913, 0x00000000 }, /* 00000000,1913,Free,fs/2,invert=0 */
		{ 0x1914, 0x3F7FF700 }, /* 3F7FF700,1914,Free,fs/2,invert=0 */
		{ 0x1915, 0x419E43FE }, /* 419E43FE,1915,HBF,67Hz,2000Hz,29.5dB,fs/2,invert=0 */
		{ 0x1916, 0xC198AE3F }, /* C198AE3F,1916,HBF,67Hz,2000Hz,29.5dB,fs/2,invert=0 */
		{ 0x1917, 0x3E9A9997 }, /* 3E9A9997,1917,HBF,67Hz,2000Hz,29.5dB,fs/2,invert=0 */
		{ 0x1918, 0x3F80BF0F }, /* 3F80BF0F,1918,LBF,0.05Hz,0.34Hz,16.7dB,fs/2,invert=0 */
		{ 0x1919, 0xBF80B90D }, /* BF80B90D,1919,LBF,0.05Hz,0.34Hz,16.7dB,fs/2,invert=0 */
		{ 0x191A, 0x3F7FFE3E }, /* 3F7FFE3E,191A,LBF,0.05Hz,0.34Hz,16.7dB,fs/2,invert=0 */
		{ 0x191B, 0x00000000 }, /* 00000000,191B,Cutoff,invert=0 */
		{ 0x191C, 0x3F800000 }, /* 3F800000,191C,0dB,invert=0 */
		{ 0x191D, 0x3F800000 }, /* 3F800000,191D,Through,0dB,fs/2,invert=0 */
		{ 0x191E, 0x00000000 }, /* 00000000,191E,Through,0dB,fs/2,invert=0 */
		{ 0x191F, 0x00000000 }, /* 00000000,191F,Through,0dB,fs/2,invert=0 */
		{ 0x1920, 0x390C87D8 }, /* 390C87D8,1920,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1921, 0x390C87D8 }, /* 390C87D8,1921,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1922, 0x3F7FEE6F }, /* 3F7FEE6F,1922,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1923, 0x390C87D8 }, /* 390C87D8,1923,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1924, 0x390C87D8 }, /* 390C87D8,1924,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1925, 0x3F7FEE6F }, /* 3F7FEE6F,1925,LPF,0.5Hz,0dB,fs/2,invert=0 */
		{ 0x1926, 0x3F800000 }, /* 3F800000,1926,0dB,invert=0 */
		{ 0x1927, 0xBF800000 }, /* BF800000,1927,0dB,invert=1 */
		{ 0x1928, 0x3F800000 }, /* 3F800000,1928,0dB,invert=0 */
		{ 0x1929, 0x40A06142 }, /* 40A06142,1929,14dB,invert=0 */
		{ 0x192A, 0x3F800000 }, /* 3F800000,192A,0dB,invert=0 */
		{ 0x192B, 0x3F800000 }, /* 3F800000,192B,0dB,invert=0 */
		{ 0x192C, 0x00000000 }, /* 00000000,192C,Cutoff,invert=0 */
		{ 0x1930, 0x3F800000 }, /* 3F800000,1930,Through,0dB,fs/2,invert=0 */
		{ 0x1931, 0x00000000 }, /* 00000000,1931,Through,0dB,fs/2,invert=0 */
		{ 0x1932, 0x00000000 }, /* 00000000,1932,Through,0dB,fs/2,invert=0 */
		{ 0x1933, 0x00000000 }, /* 00000000,1933,Through,0dB,fs/2,invert=0 */
		{ 0x1934, 0x00000000 }, /* 00000000,1934,Through,0dB,fs/2,invert=0 */
		{ 0x1935, 0x00000000 }, /* 00000000,1935,Cutoff,invert=0 */
		{ 0x1938, 0x3F800000 }, /* 3F800000,1938,0dB,invert=0 */
		{ 0x1939, 0x3C58B55D }, /* 3C58B55D,1939,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193A, 0x3C58B55D }, /* 3C58B55D,193A,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193B, 0x3F793A55 }, /* 3F793A55,193B,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193C, 0x3C58B55D }, /* 3C58B55D,193C,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193D, 0x3C58B55D }, /* 3C58B55D,193D,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193E, 0x3F793A55 }, /* 3F793A55,193E,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0xFFFF, 0xFFFFFFFF }
	},
	{
		/* VER 01: 0.3Hz */
		{ 0x1800, 0x3F800000 }, /* 3F800000,1800,0dB,invert=0 */
		{ 0x1801, 0x3F800000 }, /* 3F800000,1801,Through,0dB,fs/1,invert=0 */
		{ 0x1802, 0x00000000 }, /* 00000000,1802,Through,0dB,fs/1,invert=0 */
		{ 0x1803, 0x38A8A554 }, /* 38A8A554,1803,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1804, 0x38A8A554 }, /* 38A8A554,1804,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1805, 0x3F7FF576 }, /* 3F7FF576,1805,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1806, 0x00000000 }, /* 00000000,1806,Through,0dB,fs/1,invert=0 */
		{ 0x1807, 0x00000000 }, /* 00000000,1807,Cutoff,invert=0 */
		{ 0x180A, 0x38A8A554 }, /* 38A8A554,180A,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x180B, 0x38A8A554 }, /* 38A8A554,180B,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x180C, 0x3F7FF576 }, /* 3F7FF576,180C,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x180D, 0x3F800000 }, /* 3F800000,180D,0dB,invert=0 */
		{ 0x180E, 0xBF800000 }, /* BF800000,180E,0dB,invert=1 */
		{ 0x180F, 0x3FFF64C1 }, /* 3FFF64C1,180F,6dB,invert=0 */
		{ 0x1810, 0x3F800000 }, /* 3F800000,1810,0dB,invert=0 */
		{ 0x1811, 0x3F800000 }, /* 3F800000,1811,0dB,invert=0 */
		{ 0x1812, 0x3B02C2F2 }, /* 3B02C2F2,1812,Free,fs/2,invert=0 */
		{ 0x1813, 0x00000000 }, /* 00000000,1813,Free,fs/2,invert=0 */
		{ 0x1814, 0x3F7FF700 }, /* 3F7FF700,1814,Free,fs/2,invert=0 */
		{ 0x1815, 0x4074E401 }, /* 4074E401,1815,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1816, 0xC0695A25 }, /* C0695A25,1816,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1817, 0x3F51CF40 }, /* 3F51CF40,1817,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1818, 0x3F800000 }, /* 3F800000,1818,Through,0dB,fs/2,invert=0 */
		{ 0x1819, 0x00000000 }, /* 00000000,1819,Through,0dB,fs/2,invert=0 */
		{ 0x181A, 0x00000000 }, /* 00000000,181A,Through,0dB,fs/2,invert=0 */
		{ 0x181B, 0x00000000 }, /* 00000000,181B,Cutoff,invert=0 */
		{ 0x181C, 0x3F800000 }, /* 3F800000,181C,0dB,invert=0 */
		{ 0x181D, 0x3F800000 }, /* 3F800000,181D,Through,0dB,fs/2,invert=0 */
		{ 0x181E, 0x00000000 }, /* 00000000,181E,Through,0dB,fs/2,invert=0 */
		{ 0x181F, 0x00000000 }, /* 00000000,181F,Through,0dB,fs/2,invert=0 */
		{ 0x1820, 0x37E0DF86 }, /* 37E0DF86,1820,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1821, 0x37E0DF86 }, /* 37E0DF86,1821,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1822, 0x3F7FFC7D }, /* 3F7FFC7D,1822,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1823, 0x37E0DF86 }, /* 37E0DF86,1823,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1824, 0x37E0DF86 }, /* 37E0DF86,1824,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1825, 0x3F7FFC7D }, /* 3F7FFC7D,1825,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1826, 0x3F800000 }, /* 3F800000,1826,0dB,invert=0 */
		{ 0x1827, 0xBF800000 }, /* BF800000,1827,0dB,invert=1 */
		{ 0x1828, 0x3F800000 }, /* 3F800000,1828,0dB,invert=0 */
		{ 0x1829, 0x41220A06 }, /* 41220A06,1829,20.11dB,invert=0 */
		{ 0x182A, 0x3F800000 }, /* 3F800000,182A,0dB,invert=0 */
		{ 0x182B, 0x3F800000 }, /* 3F800000,182B,0dB,invert=0 */
		{ 0x182C, 0x00000000 }, /* 00000000,182C,Cutoff,invert=0 */
		{ 0x1830, 0x3F800000 }, /* 3F800000,1830,Through,0dB,fs/2,invert=0 */
		{ 0x1831, 0x00000000 }, /* 00000000,1831,Through,0dB,fs/2,invert=0 */
		{ 0x1832, 0x00000000 }, /* 00000000,1832,Through,0dB,fs/2,invert=0 */
		{ 0x1833, 0x00000000 }, /* 00000000,1833,Through,0dB,fs/2,invert=0 */
		{ 0x1834, 0x00000000 }, /* 00000000,1834,Through,0dB,fs/2,invert=0 */
		{ 0x1835, 0x00000000 }, /* 00000000,1835,Cutoff,invert=0 */
		{ 0x1838, 0x3F800000 }, /* 3F800000,1838,0dB,invert=0 */
		{ 0x1839, 0x3C58B55D }, /* 3C58B55D,1839,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183A, 0x3C58B55D }, /* 3C58B55D,183A,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183B, 0x3F793A55 }, /* 3F793A55,183B,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183C, 0x3C58B55D }, /* 3C58B55D,183C,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183D, 0x3C58B55D }, /* 3C58B55D,183D,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183E, 0x3F793A55 }, /* 3F793A55,183E,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x1900, 0x3F800000 }, /* 3F800000,1900,0dB,invert=0 */
		{ 0x1901, 0x3F800000 }, /* 3F800000,1901,Through,0dB,fs/1,invert=0 */
		{ 0x1902, 0x00000000 }, /* 00000000,1902,Through,0dB,fs/1,invert=0 */
		{ 0x1903, 0x38A8A554 }, /* 38A8A554,1903,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1904, 0x38A8A554 }, /* 38A8A554,1904,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1905, 0x3F7FF576 }, /* 3F7FF576,1905,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1906, 0x00000000 }, /* 00000000,1906,Through,0dB,fs/1,invert=0 */
		{ 0x1907, 0x00000000 }, /* 00000000,1907,Cutoff,invert=0 */
		{ 0x190A, 0x38A8A554 }, /* 38A8A554,190A,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x190B, 0x38A8A554 }, /* 38A8A554,190B,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x190C, 0x3F7FF576 }, /* 3F7FF576,190C,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x190D, 0x3F800000 }, /* 3F800000,190D,0dB,invert=0 */
		{ 0x190E, 0xBF800000 }, /* BF800000,190E,0dB,invert=1 */
		{ 0x190F, 0x3FFF64C1 }, /* 3FFF64C1,190F,6dB,invert=0 */
		{ 0x1910, 0x3F800000 }, /* 3F800000,1910,0dB,invert=0 */
		{ 0x1911, 0x3F800000 }, /* 3F800000,1911,0dB,invert=0 */
		{ 0x1912, 0x3B02C2F2 }, /* 3B02C2F2,1912,Free,fs/2,invert=0 */
		{ 0x1913, 0x00000000 }, /* 00000000,1913,Free,fs/2,invert=0 */
		{ 0x1914, 0x3F7FF700 }, /* 3F7FF700,1914,Free,fs/2,invert=0 */
		{ 0x1915, 0x4074E401 }, /* 4074E401,1915,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1916, 0xC0695A25 }, /* C0695A25,1916,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1917, 0x3F51CF40 }, /* 3F51CF40,1917,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1918, 0x3F800000 }, /* 3F800000,1918,Through,0dB,fs/2,invert=0 */
		{ 0x1919, 0x00000000 }, /* 00000000,1919,Through,0dB,fs/2,invert=0 */
		{ 0x191A, 0x00000000 }, /* 00000000,191A,Through,0dB,fs/2,invert=0 */
		{ 0x191B, 0x00000000 }, /* 00000000,191B,Cutoff,invert=0 */
		{ 0x191C, 0x3F800000 }, /* 3F800000,191C,0dB,invert=0 */
		{ 0x191D, 0x3F800000 }, /* 3F800000,191D,Through,0dB,fs/2,invert=0 */
		{ 0x191E, 0x00000000 }, /* 00000000,191E,Through,0dB,fs/2,invert=0 */
		{ 0x191F, 0x00000000 }, /* 00000000,191F,Through,0dB,fs/2,invert=0 */
		{ 0x1920, 0x37E0DF86 }, /* 37E0DF86,1920,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1921, 0x37E0DF86 }, /* 37E0DF86,1921,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1922, 0x3F7FFC7D }, /* 3F7FFC7D,1922,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1923, 0x37E0DF86 }, /* 37E0DF86,1923,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1924, 0x37E0DF86 }, /* 37E0DF86,1924,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1925, 0x3F7FFC7D }, /* 3F7FFC7D,1925,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1926, 0x3F800000 }, /* 3F800000,1926,0dB,invert=0 */
		{ 0x1927, 0xBF800000 }, /* BF800000,1927,0dB,invert=1 */
		{ 0x1928, 0x3F800000 }, /* 3F800000,1928,0dB,invert=0 */
		{ 0x1929, 0x41220A06 }, /* 41220A06,1929,20.11dB,invert=0 */
		{ 0x192A, 0x3F800000 }, /* 3F800000,192A,0dB,invert=0 */
		{ 0x192B, 0x3F800000 }, /* 3F800000,192B,0dB,invert=0 */
		{ 0x192C, 0x00000000 }, /* 00000000,192C,Cutoff,invert=0 */
		{ 0x1930, 0x3F800000 }, /* 3F800000,1930,Through,0dB,fs/2,invert=0 */
		{ 0x1931, 0x00000000 }, /* 00000000,1931,Through,0dB,fs/2,invert=0 */
		{ 0x1932, 0x00000000 }, /* 00000000,1932,Through,0dB,fs/2,invert=0 */
		{ 0x1933, 0x00000000 }, /* 00000000,1933,Through,0dB,fs/2,invert=0 */
		{ 0x1934, 0x00000000 }, /* 00000000,1934,Through,0dB,fs/2,invert=0 */
		{ 0x1935, 0x00000000 }, /* 00000000,1935,Cutoff,invert=0 */
		{ 0x1938, 0x3F800000 }, /* 3F800000,1938,0dB,invert=0 */
		{ 0x1939, 0x3C58B55D }, /* 3C58B55D,1939,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193A, 0x3C58B55D }, /* 3C58B55D,193A,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193B, 0x3F793A55 }, /* 3F793A55,193B,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193C, 0x3C58B55D }, /* 3C58B55D,193C,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193D, 0x3C58B55D }, /* 3C58B55D,193D,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193E, 0x3F793A55 }, /* 3F793A55,193E,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0xFFFF, 0xFFFFFFFF }
	},
	{
		/* VER 02: 0.3Hz */
		{ 0x1800, 0x3F800000 }, /* 3F800000,1800,0dB,invert=0 */
		{ 0x1801, 0x3F800000 }, /* 3F800000,1801,Through,0dB,fs/1,invert=0 */
		{ 0x1802, 0x00000000 }, /* 00000000,1802,Through,0dB,fs/1,invert=0 */
		{ 0x1803, 0x38A8A554 }, /* 38A8A554,1803,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1804, 0x38A8A554 }, /* 38A8A554,1804,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1805, 0x3F7FF576 }, /* 3F7FF576,1805,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1806, 0x00000000 }, /* 00000000,1806,Through,0dB,fs/1,invert=0 */
		{ 0x1807, 0x00000000 }, /* 00000000,1807,Cutoff,invert=0 */
		{ 0x180A, 0x38A8A554 }, /* 38A8A554,180A,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x180B, 0x38A8A554 }, /* 38A8A554,180B,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x180C, 0x3F7FF576 }, /* 3F7FF576,180C,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x180D, 0x3F800000 }, /* 3F800000,180D,0dB,invert=0 */
		{ 0x180E, 0xBF800000 }, /* BF800000,180E,0dB,invert=1 */
		{ 0x180F, 0x3FFF64C1 }, /* 3FFF64C1,180F,6dB,invert=0 */
		{ 0x1810, 0x3F800000 }, /* 3F800000,1810,0dB,invert=0 */
		{ 0x1811, 0x3F800000 }, /* 3F800000,1811,0dB,invert=0 */
		{ 0x1812, 0x3B02C2F2 }, /* 3B02C2F2,1812,Free,fs/2,invert=0 */
		{ 0x1813, 0x00000000 }, /* 00000000,1813,Free,fs/2,invert=0 */
		{ 0x1814, 0x3F7FF700 }, /* 3F7FF700,1814,Free,fs/2,invert=0 */
		{ 0x1815, 0x4074E401 }, /* 4074E401,1815,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1816, 0xC0695A25 }, /* C0695A25,1816,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1817, 0x3F51CF40 }, /* 3F51CF40,1817,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1818, 0x3F800000 }, /* 3F800000,1818,Through,0dB,fs/2,invert=0 */
		{ 0x1819, 0x00000000 }, /* 00000000,1819,Through,0dB,fs/2,invert=0 */
		{ 0x181A, 0x00000000 }, /* 00000000,181A,Through,0dB,fs/2,invert=0 */
		{ 0x181B, 0x00000000 }, /* 00000000,181B,Cutoff,invert=0 */
		{ 0x181C, 0x3F800000 }, /* 3F800000,181C,0dB,invert=0 */
		{ 0x181D, 0x3F800000 }, /* 3F800000,181D,Through,0dB,fs/2,invert=0 */
		{ 0x181E, 0x00000000 }, /* 00000000,181E,Through,0dB,fs/2,invert=0 */
		{ 0x181F, 0x00000000 }, /* 00000000,181F,Through,0dB,fs/2,invert=0 */
		{ 0x1820, 0x37E0DF86 }, /* 37E0DF86,1820,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1821, 0x37E0DF86 }, /* 37E0DF86,1821,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1822, 0x3F7FFC7D }, /* 3F7FFC7D,1822,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1823, 0x37E0DF86 }, /* 37E0DF86,1823,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1824, 0x37E0DF86 }, /* 37E0DF86,1824,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1825, 0x3F7FFC7D }, /* 3F7FFC7D,1825,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1826, 0x3F800000 }, /* 3F800000,1826,0dB,invert=0 */
		{ 0x1827, 0xBF800000 }, /* BF800000,1827,0dB,invert=1 */
		{ 0x1828, 0x3F800000 }, /* 3F800000,1828,0dB,invert=0 */
		{ 0x1829, 0x41220A06 }, /* 41220A06,1829,20.11dB,invert=0 */
		{ 0x182A, 0x3F800000 }, /* 3F800000,182A,0dB,invert=0 */
		{ 0x182B, 0x3F800000 }, /* 3F800000,182B,0dB,invert=0 */
		{ 0x182C, 0x00000000 }, /* 00000000,182C,Cutoff,invert=0 */
		{ 0x1830, 0x3F800000 }, /* 3F800000,1830,Through,0dB,fs/2,invert=0 */
		{ 0x1831, 0x00000000 }, /* 00000000,1831,Through,0dB,fs/2,invert=0 */
		{ 0x1832, 0x00000000 }, /* 00000000,1832,Through,0dB,fs/2,invert=0 */
		{ 0x1833, 0x00000000 }, /* 00000000,1833,Through,0dB,fs/2,invert=0 */
		{ 0x1834, 0x00000000 }, /* 00000000,1834,Through,0dB,fs/2,invert=0 */
		{ 0x1835, 0x00000000 }, /* 00000000,1835,Cutoff,invert=0 */
		{ 0x1838, 0x3F800000 }, /* 3F800000,1838,0dB,invert=0 */
		{ 0x1839, 0x3C58B55D }, /* 3C58B55D,1839,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183A, 0x3C58B55D }, /* 3C58B55D,183A,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183B, 0x3F793A55 }, /* 3F793A55,183B,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183C, 0x3C58B55D }, /* 3C58B55D,183C,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183D, 0x3C58B55D }, /* 3C58B55D,183D,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183E, 0x3F793A55 }, /* 3F793A55,183E,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x1900, 0x3F800000 }, /* 3F800000,1900,0dB,invert=0 */
		{ 0x1901, 0x3F800000 }, /* 3F800000,1901,Through,0dB,fs/1,invert=0 */
		{ 0x1902, 0x00000000 }, /* 00000000,1902,Through,0dB,fs/1,invert=0 */
		{ 0x1903, 0x38A8A554 }, /* 38A8A554,1903,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1904, 0x38A8A554 }, /* 38A8A554,1904,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1905, 0x3F7FF576 }, /* 3F7FF576,1905,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1906, 0x00000000 }, /* 00000000,1906,Through,0dB,fs/1,invert=0 */
		{ 0x1907, 0x00000000 }, /* 00000000,1907,Cutoff,invert=0 */
		{ 0x190A, 0x38A8A554 }, /* 38A8A554,190A,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x190B, 0x38A8A554 }, /* 38A8A554,190B,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x190C, 0x3F7FF576 }, /* 3F7FF576,190C,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x190D, 0x3F800000 }, /* 3F800000,190D,0dB,invert=0 */
		{ 0x190E, 0xBF800000 }, /* BF800000,190E,0dB,invert=1 */
		{ 0x190F, 0x3FFF64C1 }, /* 3FFF64C1,190F,6dB,invert=0 */
		{ 0x1910, 0x3F800000 }, /* 3F800000,1910,0dB,invert=0 */
		{ 0x1911, 0x3F800000 }, /* 3F800000,1911,0dB,invert=0 */
		{ 0x1912, 0x3B02C2F2 }, /* 3B02C2F2,1912,Free,fs/2,invert=0 */
		{ 0x1913, 0x00000000 }, /* 00000000,1913,Free,fs/2,invert=0 */
		{ 0x1914, 0x3F7FF700 }, /* 3F7FF700,1914,Free,fs/2,invert=0 */
		{ 0x1915, 0x4074E401 }, /* 4074E401,1915,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1916, 0xC0695A25 }, /* C0695A25,1916,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1917, 0x3F51CF40 }, /* 3F51CF40,1917,HBF,90Hz,369.9Hz,12.27dB,fs/2,invert=0 */
		{ 0x1918, 0x3F800000 }, /* 3F800000,1918,Through,0dB,fs/2,invert=0 */
		{ 0x1919, 0x00000000 }, /* 00000000,1919,Through,0dB,fs/2,invert=0 */
		{ 0x191A, 0x00000000 }, /* 00000000,191A,Through,0dB,fs/2,invert=0 */
		{ 0x191B, 0x00000000 }, /* 00000000,191B,Cutoff,invert=0 */
		{ 0x191C, 0x3F800000 }, /* 3F800000,191C,0dB,invert=0 */
		{ 0x191D, 0x3F800000 }, /* 3F800000,191D,Through,0dB,fs/2,invert=0 */
		{ 0x191E, 0x00000000 }, /* 00000000,191E,Through,0dB,fs/2,invert=0 */
		{ 0x191F, 0x00000000 }, /* 00000000,191F,Through,0dB,fs/2,invert=0 */
		{ 0x1920, 0x37E0DF86 }, /* 37E0DF86,1920,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1921, 0x37E0DF86 }, /* 37E0DF86,1921,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1922, 0x3F7FFC7D }, /* 3F7FFC7D,1922,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1923, 0x37E0DF86 }, /* 37E0DF86,1923,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1924, 0x37E0DF86 }, /* 37E0DF86,1924,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1925, 0x3F7FFC7D }, /* 3F7FFC7D,1925,LPF,0.1Hz,0dB,fs/2,invert=0 */
		{ 0x1926, 0x3F800000 }, /* 3F800000,1926,0dB,invert=0 */
		{ 0x1927, 0xBF800000 }, /* BF800000,1927,0dB,invert=1 */
		{ 0x1928, 0x3F800000 }, /* 3F800000,1928,0dB,invert=0 */
		{ 0x1929, 0x41220A06 }, /* 41220A06,1929,20.11dB,invert=0 */
		{ 0x192A, 0x3F800000 }, /* 3F800000,192A,0dB,invert=0 */
		{ 0x192B, 0x3F800000 }, /* 3F800000,192B,0dB,invert=0 */
		{ 0x192C, 0x00000000 }, /* 00000000,192C,Cutoff,invert=0 */
		{ 0x1930, 0x3F800000 }, /* 3F800000,1930,Through,0dB,fs/2,invert=0 */
		{ 0x1931, 0x00000000 }, /* 00000000,1931,Through,0dB,fs/2,invert=0 */
		{ 0x1932, 0x00000000 }, /* 00000000,1932,Through,0dB,fs/2,invert=0 */
		{ 0x1933, 0x00000000 }, /* 00000000,1933,Through,0dB,fs/2,invert=0 */
		{ 0x1934, 0x00000000 }, /* 00000000,1934,Through,0dB,fs/2,invert=0 */
		{ 0x1935, 0x00000000 }, /* 00000000,1935,Cutoff,invert=0 */
		{ 0x1938, 0x3F800000 }, /* 3F800000,1938,0dB,invert=0 */
		{ 0x1939, 0x3C58B55D }, /* 3C58B55D,1939,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193A, 0x3C58B55D }, /* 3C58B55D,193A,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193B, 0x3F793A55 }, /* 3F793A55,193B,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193C, 0x3C58B55D }, /* 3C58B55D,193C,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193D, 0x3C58B55D }, /* 3C58B55D,193D,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193E, 0x3F793A55 }, /* 3F793A55,193E,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0xFFFF, 0xFFFFFFFF }
	},
	{
		/* VER 03: Invensense_130501A */
		{ 0x1800, 0x3F800000 }, /* 3F800000,1800,0dB,invert=0 */
		{ 0x1801, 0x3F800000 }, /* 3F800000,1801,Through,0dB,fs/1,invert=0 */
		{ 0x1802, 0x00000000 }, /* 00000000,1802,Through,0dB,fs/1,invert=0 */
		{ 0x1803, 0x38C4C035 }, /* 38C4C035,1803,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x1804, 0x38C4C035 }, /* 38C4C035,1804,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x1805, 0x3F7FF3B4 }, /* 3F7FF3B4,1805,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x1806, 0x00000000 }, /* 00000000,1806,Through,0dB,fs/1,invert=0 */
		{ 0x1807, 0x00000000 }, /* 00000000,1807,Cutoff,invert=0 */
		{ 0x180A, 0x38C4C035 }, /* 38C4C035,180A,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x180B, 0x38C4C035 }, /* 38C4C035,180B,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x180C, 0x3F7FF3B4 }, /* 3F7FF3B4,180C,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x180D, 0x3F800000 }, /* 3F800000,180D,0dB,invert=0 */
		{ 0x180E, 0xBF800000 }, /* BF800000,180E,0dB,invert=1 */
		{ 0x180F, 0x3FFF64C1 }, /* 3FFF64C1,180F,6dB,invert=0 */
		{ 0x1810, 0x3F800000 }, /* 3F800000,1810,0dB,invert=0 */
		{ 0x1811, 0x3F800000 }, /* 3F800000,1811,0dB,invert=0 */
		{ 0x1812, 0x3B02C2F2 }, /* 3B02C2F2,1812,Free,fs/2,invert=0 */
		{ 0x1813, 0x00000000 }, /* 00000000,1813,Free,fs/2,invert=0 */
		{ 0x1814, 0x3F7FF700 }, /* 3F7FF700,1814,Free,fs/2,invert=0 */
		{ 0x1815, 0x40EDD564 }, /* 40EDD564,1815,HBF,100Hz,900Hz,19.07dB,fs/2,invert=0 */
		{ 0x1816, 0xC0E16A39 }, /* C0E16A39,1816,HBF,100Hz,900Hz,19.07dB,fs/2,invert=0 */
		{ 0x1817, 0x3F1C7B26 }, /* 3F1C7B26,1817,HBF,100Hz,900Hz,19.07dB,fs/2,invert=0 */
		{ 0x1818, 0x3F800000 }, /* 3F800000,1818,Through,0dB,fs/2,invert=0 */
		{ 0x1819, 0x00000000 }, /* 00000000,1819,Through,0dB,fs/2,invert=0 */
		{ 0x181A, 0x00000000 }, /* 00000000,181A,Through,0dB,fs/2,invert=0 */
		{ 0x181B, 0x00000000 }, /* 00000000,181B,Cutoff,invert=0 */
		{ 0x181C, 0x3F800000 }, /* 3F800000,181C,0dB,invert=0 */
		{ 0x181D, 0x3F800000 }, /* 3F800000,181D,Through,0dB,fs/2,invert=0 */
		{ 0x181E, 0x00000000 }, /* 00000000,181E,Through,0dB,fs/2,invert=0 */
		{ 0x181F, 0x00000000 }, /* 00000000,181F,Through,0dB,fs/2,invert=0 */
		{ 0x1820, 0x38E0DAE5 }, /* 38E0DAE5,1820,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1821, 0x38E0DAE5 }, /* 38E0DAE5,1821,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1822, 0x3F7FF1F2 }, /* 3F7FF1F2,1822,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1823, 0x38E0DAE5 }, /* 38E0DAE5,1823,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1824, 0x38E0DAE5 }, /* 38E0DAE5,1824,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1825, 0x3F7FF1F2 }, /* 3F7FF1F2,1825,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1826, 0x3F800000 }, /* 3F800000,1826,0dB,invert=0 */
		{ 0x1827, 0xBF800000 }, /* BF800000,1827,0dB,invert=1 */
		{ 0x1828, 0x3F800000 }, /* 3F800000,1828,0dB,invert=0 */
		{ 0x1829, 0x41220A06 }, /* 41220A06,1829,20.11dB,invert=0 */
		{ 0x182A, 0x3F800000 }, /* 3F800000,182A,0dB,invert=0 */
		{ 0x182B, 0x3F800000 }, /* 3F800000,182B,0dB,invert=0 */
		{ 0x182C, 0x00000000 }, /* 00000000,182C,Cutoff,invert=0 */
		{ 0x1830, 0x3F800000 }, /* 3F800000,1830,Through,0dB,fs/2,invert=0 */
		{ 0x1831, 0x00000000 }, /* 00000000,1831,Through,0dB,fs/2,invert=0 */
		{ 0x1832, 0x00000000 }, /* 00000000,1832,Through,0dB,fs/2,invert=0 */
		{ 0x1833, 0x00000000 }, /* 00000000,1833,Through,0dB,fs/2,invert=0 */
		{ 0x1834, 0x00000000 }, /* 00000000,1834,Through,0dB,fs/2,invert=0 */
		{ 0x1835, 0x00000000 }, /* 00000000,1835,Cutoff,invert=0 */
		{ 0x1838, 0x3F800000 }, /* 3F800000,1838,0dB,invert=0 */
		{ 0x1839, 0x3C58B55D }, /* 3C58B55D,1839,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183A, 0x3C58B55D }, /* 3C58B55D,183A,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183B, 0x3F793A55 }, /* 3F793A55,183B,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183C, 0x3C58B55D }, /* 3C58B55D,183C,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183D, 0x3C58B55D }, /* 3C58B55D,183D,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183E, 0x3F793A55 }, /* 3F793A55,183E,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x1840, 0x3AAF73A1 }, /* 3A52A7A1,1840,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1841, 0x3AAF73A1 }, /* 3A52A7A1,1841,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1842, 0x3F7F508C }, /* 3F7F96AC,1842,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1850, 0x3AAF73A1 }, /* 3A52A7A1,1850,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1851, 0x3AAF73A1 }, /* 3A52A7A1,1851,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1852, 0x3F7F508C }, /* 3F7F96AC,1852,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1900, 0x3F800000 }, /* 3F800000,1900,0dB,invert=0 */
		{ 0x1901, 0x3F800000 }, /* 3F800000,1901,Through,0dB,fs/1,invert=0 */
		{ 0x1902, 0x00000000 }, /* 00000000,1902,Through,0dB,fs/1,invert=0 */
		{ 0x1903, 0x38C4C035 }, /* 38C4C035,1903,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x1904, 0x38C4C035 }, /* 38C4C035,1904,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x1905, 0x3F7FF3B4 }, /* 3F7FF3B4,1905,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x1906, 0x00000000 }, /* 00000000,1906,Through,0dB,fs/1,invert=0 */
		{ 0x1907, 0x00000000 }, /* 00000000,1907,Cutoff,invert=0 */
		{ 0x190A, 0x38C4C035 }, /* 38C4C035,190A,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x190B, 0x38C4C035 }, /* 38C4C035,190B,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x190C, 0x3F7FF3B4 }, /* 3F7FF3B4,190C,LPF,0.35Hz,0dB,fs/2,invert=0 */
		{ 0x190D, 0x3F800000 }, /* 3F800000,190D,0dB,invert=0 */
		{ 0x190E, 0xBF800000 }, /* BF800000,190E,0dB,invert=1 */
		{ 0x190F, 0x3FFF64C1 }, /* 3FFF64C1,190F,6dB,invert=0 */
		{ 0x1910, 0x3F800000 }, /* 3F800000,1910,0dB,invert=0 */
		{ 0x1911, 0x3F800000 }, /* 3F800000,1911,0dB,invert=0 */
		{ 0x1912, 0x3B02C2F2 }, /* 3B02C2F2,1912,Free,fs/2,invert=0 */
		{ 0x1913, 0x00000000 }, /* 00000000,1913,Free,fs/2,invert=0 */
		{ 0x1914, 0x3F7FF700 }, /* 3F7FF700,1914,Free,fs/2,invert=0 */
		{ 0x1915, 0x40EDD564 }, /* 40EDD564,1915,HBF,100Hz,900Hz,19.07dB,fs/2,invert=0 */
		{ 0x1916, 0xC0E16A39 }, /* C0E16A39,1916,HBF,100Hz,900Hz,19.07dB,fs/2,invert=0 */
		{ 0x1917, 0x3F1C7B26 }, /* 3F1C7B26,1917,HBF,100Hz,900Hz,19.07dB,fs/2,invert=0 */
		{ 0x1918, 0x3F800000 }, /* 3F800000,1918,Through,0dB,fs/2,invert=0 */
		{ 0x1919, 0x00000000 }, /* 00000000,1919,Through,0dB,fs/2,invert=0 */
		{ 0x191A, 0x00000000 }, /* 00000000,191A,Through,0dB,fs/2,invert=0 */
		{ 0x191B, 0x00000000 }, /* 00000000,191B,Cutoff,invert=0 */
		{ 0x191C, 0x3F800000 }, /* 3F800000,191C,0dB,invert=0 */
		{ 0x191D, 0x3F800000 }, /* 3F800000,191D,Through,0dB,fs/2,invert=0 */
		{ 0x191E, 0x00000000 }, /* 00000000,191E,Through,0dB,fs/2,invert=0 */
		{ 0x191F, 0x00000000 }, /* 00000000,191F,Through,0dB,fs/2,invert=0 */
		{ 0x1920, 0x38E0DAE5 }, /* 38E0DAE5,1920,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1921, 0x38E0DAE5 }, /* 38E0DAE5,1921,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1922, 0x3F7FF1F2 }, /* 3F7FF1F2,1922,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1923, 0x38E0DAE5 }, /* 38E0DAE5,1923,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1924, 0x38E0DAE5 }, /* 38E0DAE5,1924,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1925, 0x3F7FF1F2 }, /* 3F7FF1F2,1925,LPF,0.4Hz,0dB,fs/2,invert=0 */
		{ 0x1926, 0x3F800000 }, /* 3F800000,1926,0dB,invert=0 */
		{ 0x1927, 0xBF800000 }, /* BF800000,1927,0dB,invert=1 */
		{ 0x1928, 0x3F800000 }, /* 3F800000,1928,0dB,invert=0 */
		{ 0x1929, 0x41220A06 }, /* 41220A06,1929,20.11dB,invert=0 */
		{ 0x192A, 0x3F800000 }, /* 3F800000,192A,0dB,invert=0 */
		{ 0x192B, 0x3F800000 }, /* 3F800000,192B,0dB,invert=0 */
		{ 0x192C, 0x00000000 }, /* 00000000,192C,Cutoff,invert=0 */
		{ 0x1930, 0x3F800000 }, /* 3F800000,1930,Through,0dB,fs/2,invert=0 */
		{ 0x1931, 0x00000000 }, /* 00000000,1931,Through,0dB,fs/2,invert=0 */
		{ 0x1932, 0x00000000 }, /* 00000000,1932,Through,0dB,fs/2,invert=0 */
		{ 0x1933, 0x00000000 }, /* 00000000,1933,Through,0dB,fs/2,invert=0 */
		{ 0x1934, 0x00000000 }, /* 00000000,1934,Through,0dB,fs/2,invert=0 */
		{ 0x1935, 0x00000000 }, /* 00000000,1935,Cutoff,invert=0 */
		{ 0x1938, 0x3F800000 }, /* 3F800000,1938,0dB,invert=0 */
		{ 0x1939, 0x3C58B55D }, /* 3C58B55D,1939,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193A, 0x3C58B55D }, /* 3C58B55D,193A,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193B, 0x3F793A55 }, /* 3F793A55,193B,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193C, 0x3C58B55D }, /* 3C58B55D,193C,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193D, 0x3C58B55D }, /* 3C58B55D,193D,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193E, 0x3F793A55 }, /* 3F793A55,193E,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x1940, 0x3AAF73A1 }, /* 3A52A7A1,1940,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1941, 0x3AAF73A1 }, /* 3A52A7A1,1941,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1942, 0x3F7F508C }, /* 3F7F96AC,1942,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1950, 0x3AAF73A1 }, /* 3A52A7A1,1950,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1951, 0x3AAF73A1 }, /* 3A52A7A1,1951,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0x1952, 0x3F7F508C }, /* 3F7F96AC,1952,LPF,3Hz,0dB,fs/2,invert=0 */
		{ 0xFFFF, 0xFFFFFFFF }
	},
	{
		/* VER 04 */
		{ 0x1800, 0x3F800000 }, /* 3F800000,1800,0dB,invert=0 */
		{ 0x1801, 0x3C3F00A9 }, /* 3C3F00A9,1801,LPF,44Hz,0dB,fs/2,invert=0 */
		{ 0x1802, 0x3F7A07FB }, /* 3F7A07FB,1802,LPF,44Hz,0dB,fs/2,invert=0 */
		{ 0x1803, 0x38A8A554 }, /* 38A8A554,1803,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1804, 0x38A8A554 }, /* 38A8A554,1804,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1805, 0x3F7FF576 }, /* 3F7FF576,1805,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1806, 0x3C3F00A9 }, /* 3C3F00A9,1806,LPF,44Hz,0dB,fs/2,invert=0 */
		{ 0x1807, 0x00000000 }, /* 00000000,1807,Cutoff,invert=0 */
		{ 0x180A, 0x38A8A554 }, /* 38A8A554,180A,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x180B, 0x38A8A554 }, /* 38A8A554,180B,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x180C, 0x3F7FF576 }, /* 3F7FF576,180C,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x180D, 0x3F800000 }, /* 3F800000,180D,0dB,invert=0 */
		{ 0x180E, 0xBF800000 }, /* BF800000,180E,0dB,invert=1 */
		{ 0x180F, 0x3FFF64C1 }, /* 3FFF64C1,180F,6dB,invert=0 */
		{ 0x1810, 0x3F800000 }, /* 3F800000,1810,0dB,invert=0 */
		{ 0x1811, 0x3F800000 }, /* 3F800000,1811,0dB,invert=0 */
		{ 0x1812, 0x3B02C2F2 }, /* 3B02C2F2,1812,Free,fs/2,invert=0 */
		{ 0x1813, 0x00000000 }, /* 00000000,1813,Free,fs/2,invert=0 */
		{ 0x1814, 0x3F7FFD80 }, /* 3F7FFD80,1814,Free,fs/2,invert=0 */
		{ 0x1815, 0x428C7352 }, /* 428C7352,1815,HBF,24Hz,3000Hz,42dB,fs/2,invert=0 */
		{ 0x1816, 0xC28AA79E }, /* C28AA79E,1816,HBF,24Hz,3000Hz,42dB,fs/2,invert=0 */
		{ 0x1817, 0x3DDE3847 }, /* 3DDE3847,1817,HBF,24Hz,3000Hz,42dB,fs/2,invert=0 */
		{ 0x1818, 0x3F231C22 }, /* 3F231C22,1818,LBF,40Hz,50Hz,-2dB,fs/2,invert=0 */
		{ 0x1819, 0xBF1ECB8E }, /* BF1ECB8E,1819,LBF,40Hz,50Hz,-2dB,fs/2,invert=0 */
		{ 0x181A, 0x3F7A916B }, /* 3F7A916B,181A,LBF,40Hz,50Hz,-2dB,fs/2,invert=0 */
		{ 0x181B, 0x00000000 }, /* 00000000,181B,Cutoff,invert=0 */
		{ 0x181C, 0x3F800000 }, /* 3F800000,181C,0dB,invert=0 */
		{ 0x181D, 0x3F800000 }, /* 3F800000,181D,Through,0dB,fs/2,invert=0 */
		{ 0x181E, 0x00000000 }, /* 00000000,181E,Through,0dB,fs/2,invert=0 */
		{ 0x181F, 0x00000000 }, /* 00000000,181F,Through,0dB,fs/2,invert=0 */
		{ 0x1820, 0x3F75C43F }, /* 3F75C43F,1820,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0 */
		{ 0x1821, 0xBF756FF8 }, /* BF756FF8,1821,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0 */
		{ 0x1822, 0x3F7FABB9 }, /* 3F7FABB9,1822,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0 */
		{ 0x1823, 0x3F800000 }, /* 3F800000,1823,Through,0dB,fs/2,invert=0 */
		{ 0x1824, 0x00000000 }, /* 00000000,1824,Through,0dB,fs/2,invert=0 */
		{ 0x1825, 0x00000000 }, /* 00000000,1825,Through,0dB,fs/2,invert=0 */
		{ 0x1826, 0x00000000 }, /* 00000000,1826,Cutoff,invert=0 */
		{ 0x1827, 0x3F800000 }, /* 3F800000,1827,0dB,invert=0 */
		{ 0x1828, 0x3F800000 }, /* 3F800000,1828,0dB,invert=0 */
		{ 0x1829, 0x41400000 }, /* 41400000,1829,21.5836dB,invert=0 */
		{ 0x182A, 0x3F800000 }, /* 3F800000,182A,0dB,invert=0 */
		{ 0x182B, 0x3F800000 }, /* 3F800000,182B,0dB,invert=0 */
		{ 0x182C, 0x00000000 }, /* 00000000,182C,Cutoff,invert=0 */
		{ 0x1830, 0x3D1E56E0 }, /* 3D1E56E0,1830,LPF,300Hz,0dB,fs/1,invert=0 */
		{ 0x1831, 0x3D1E56E0 }, /* 3D1E56E0,1831,LPF,300Hz,0dB,fs/1,invert=0 */
		{ 0x1832, 0x3F6C3524 }, /* 3F6C3524,1832,LPF,300Hz,0dB,fs/1,invert=0 */
		{ 0x1833, 0x00000000 }, /* 00000000,1833,LPF,300Hz,0dB,fs/1,invert=0 */
		{ 0x1834, 0x00000000 }, /* 00000000,1834,LPF,300Hz,0dB,fs/1,invert=0 */
		{ 0x1835, 0x00000000 }, /* 00000000,1835,Cutoff,invert=0 */
		{ 0x1838, 0x3F800000 }, /* 3F800000,1838,0dB,invert=0 */
		{ 0x1839, 0x3C58B55D }, /* 3C58B55D,1839,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183A, 0x3C58B55D }, /* 3C58B55D,183A,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183B, 0x3F793A55 }, /* 3F793A55,183B,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183C, 0x3C58B55D }, /* 3C58B55D,183C,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183D, 0x3C58B55D }, /* 3C58B55D,183D,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x183E, 0x3F793A55 }, /* 3F793A55,183E,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x1840, 0x38A8A554 }, /* 38A8A554,1840,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1841, 0x38A8A554 }, /* 38A8A554,1841,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1842, 0x3F7FF576 }, /* 3F7FF576,1842,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1850, 0x38A8A554 }, /* 38A8A554,1850,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1851, 0x38A8A554 }, /* 38A8A554,1851,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1852, 0x3F7FF576 }, /* 3F7FF576,1852,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1900, 0x3F800000 }, /* 3F800000,1900,0dB,invert=0 */
		{ 0x1901, 0x3C3F00A9 }, /* 3C3F00A9,1901,LPF,44Hz,0dB,fs/2,invert=0 */
		{ 0x1902, 0x3F7A07FB }, /* 3F7A07FB,1902,LPF,44Hz,0dB,fs/2,invert=0 */
		{ 0x1903, 0x38A8A554 }, /* 38A8A554,1903,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1904, 0x38A8A554 }, /* 38A8A554,1904,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1905, 0x3F7FF576 }, /* 3F7FF576,1905,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1906, 0x3C3F00A9 }, /* 3C3F00A9,1906,LPF,44Hz,0dB,fs/2,invert=0 */
		{ 0x1907, 0x00000000 }, /* 00000000,1907,Cutoff,invert=0 */
		{ 0x190A, 0x38A8A554 }, /* 38A8A554,190A,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x190B, 0x38A8A554 }, /* 38A8A554,190B,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x190C, 0x3F7FF576 }, /* 3F7FF576,190C,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x190D, 0x3F800000 }, /* 3F800000,190D,0dB,invert=0 */
		{ 0x190E, 0xBF800000 }, /* BF800000,190E,0dB,invert=1 */
		{ 0x190F, 0x3FFF64C1 }, /* 3FFF64C1,190F,6dB,invert=0 */
		{ 0x1910, 0x3F800000 }, /* 3F800000,1910,0dB,invert=0 */
		{ 0x1911, 0x3F800000 }, /* 3F800000,1911,0dB,invert=0 */
		{ 0x1912, 0x3B02C2F2 }, /* 3B02C2F2,1912,Free,fs/2,invert=0 */
		{ 0x1913, 0x00000000 }, /* 00000000,1913,Free,fs/2,invert=0 */
		{ 0x1914, 0x3F7FFD80 }, /* 3F7FFD80,1914,Free,fs/2,invert=0 */
		{ 0x1915, 0x428C7352 }, /* 428C7352,1915,HBF,24Hz,3000Hz,42dB,fs/2,invert=0 */
		{ 0x1916, 0xC28AA79E }, /* C28AA79E,1916,HBF,24Hz,3000Hz,42dB,fs/2,invert=0 */
		{ 0x1917, 0x3DDE3847 }, /* 3DDE3847,1917,HBF,24Hz,3000Hz,42dB,fs/2,invert=0 */
		{ 0x1918, 0x3F231C22 }, /* 3F231C22,1918,LBF,40Hz,50Hz,-2dB,fs/2,invert=0 */
		{ 0x1919, 0xBF1ECB8E }, /* BF1ECB8E,1919,LBF,40Hz,50Hz,-2dB,fs/2,invert=0 */
		{ 0x191A, 0x3F7A916B }, /* 3F7A916B,191A,LBF,40Hz,50Hz,-2dB,fs/2,invert=0 */
		{ 0x191B, 0x00000000 }, /* 00000000,191B,Cutoff,invert=0 */
		{ 0x191C, 0x3F800000 }, /* 3F800000,191C,0dB,invert=0 */
		{ 0x191D, 0x3F800000 }, /* 3F800000,191D,Through,0dB,fs/2,invert=0 */
		{ 0x191E, 0x00000000 }, /* 00000000,191E,Through,0dB,fs/2,invert=0 */
		{ 0x191F, 0x00000000 }, /* 00000000,191F,Through,0dB,fs/2,invert=0 */
		{ 0x1920, 0x3F75C43F }, /* 3F75C43F,1920,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0 */
		{ 0x1921, 0xBF756FF8 }, /* BF756FF8,1921,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0 */
		{ 0x1922, 0x3F7FABB9 }, /* 3F7FABB9,1922,LBF,2.4Hz,2.5Hz,0dB,fs/2,invert=0 */
		{ 0x1923, 0x3F800000 }, /* 3F800000,1923,Through,0dB,fs/2,invert=0 */
		{ 0x1924, 0x00000000 }, /* 00000000,1924,Through,0dB,fs/2,invert=0 */
		{ 0x1925, 0x00000000 }, /* 00000000,1925,Through,0dB,fs/2,invert=0 */
		{ 0x1926, 0x00000000 }, /* 00000000,1926,Cutoff,invert=0 */
		{ 0x1927, 0x3F800000 }, /* 3F800000,1927,0dB,invert=0 */
		{ 0x1928, 0x3F800000 }, /* 3F800000,1928,0dB,invert=0 */
		{ 0x1929, 0x41400000 }, /* 41400000,1929,21.5836dB,invert=0 */
		{ 0x192A, 0x3F800000 }, /* 3F800000,192A,0dB,invert=0 */
		{ 0x192B, 0x3F800000 }, /* 3F800000,192B,0dB,invert=0 */
		{ 0x192C, 0x00000000 }, /* 00000000,192C,Cutoff,invert=0 */
		{ 0x1930, 0x3D1E56E0 }, /* 3D1E56E0,1930,LPF,300Hz,0dB,fs/1,invert=0 */
		{ 0x1931, 0x3D1E56E0 }, /* 3D1E56E0,1931,LPF,300Hz,0dB,fs/1,invert=0 */
		{ 0x1932, 0x3F6C3524 }, /* 3F6C3524,1932,LPF,300Hz,0dB,fs/1,invert=0 */
		{ 0x1933, 0x00000000 }, /* 00000000,1933,LPF,300Hz,0dB,fs/1,invert=0 */
		{ 0x1934, 0x00000000 }, /* 00000000,1934,LPF,300Hz,0dB,fs/1,invert=0 */
		{ 0x1935, 0x00000000 }, /* 00000000,1935,Cutoff,invert=0 */
		{ 0x1938, 0x3F800000 }, /* 3F800000,1938,0dB,invert=0 */
		{ 0x1939, 0x3C58B55D }, /* 3C58B55D,1939,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193A, 0x3C58B55D }, /* 3C58B55D,193A,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193B, 0x3F793A55 }, /* 3F793A55,193B,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193C, 0x3C58B55D }, /* 3C58B55D,193C,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193D, 0x3C58B55D }, /* 3C58B55D,193D,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x193E, 0x3F793A55 }, /* 3F793A55,193E,LPF,100Hz,0dB,fs/1,invert=0 */
		{ 0x1940, 0x38A8A554 }, /* 38A8A554,1940,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1941, 0x38A8A554 }, /* 38A8A554,1941,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1942, 0x3F7FF576 }, /* 3F7FF576,1942,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1950, 0x38A8A554 }, /* 38A8A554,1950,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1951, 0x38A8A554 }, /* 38A8A554,1951,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0x1952, 0x3F7FF576 }, /* 3F7FF576,1952,LPF,0.3Hz,0dB,fs/2,invert=0 */
		{ 0xFFFF, 0xFFFFFFFF }
	}
};

static void with_time(unsigned short Uswith_time)
{
	usleep(Uswith_time * 1000);
}

/*===========================================================================
 * Function Name: e2p_data
 * Retun Value: NON
 * Argment Value: NON
 * Explanation: E2PROM Calibration Data Read Function
 * History: First edition 2013.06.21 Y.Kim
 *==========================================================================*/
static void e2p_data(void)
{
	memset((unsigned char *)&StCalDat, 0, sizeof(stCalDat));

	e2p_read((unsigned short)ADJ_COMP_FLAG,
		2, (unsigned char *)&StCalDat.UsAdjCompF);

	e2p_read((unsigned short)HALL_OFFSET_X,
		2, (unsigned char *)&StCalDat.StHalAdj.UsHlxOff);
	e2p_read((unsigned short)HALL_BIAS_X,
		2, (unsigned char *)&StCalDat.StHalAdj.UsHlxGan);
	e2p_read((unsigned short)HALL_AD_OFFSET_X,
		2, (unsigned char *)&StCalDat.StHalAdj.UsAdxOff);

	e2p_read((unsigned short)HALL_OFFSET_Y,
		2, (unsigned char *)&StCalDat.StHalAdj.UsHlyOff);
	e2p_read((unsigned short)HALL_BIAS_Y,
		2, (unsigned char *)&StCalDat.StHalAdj.UsHlyGan);
	e2p_read((unsigned short)HALL_AD_OFFSET_Y,
		2, (unsigned char *)&StCalDat.StHalAdj.UsAdyOff);

	e2p_read((unsigned short)LOOP_GAIN_X,
		2, (unsigned char *)&StCalDat.StLopGan.UsLxgVal);
	e2p_read((unsigned short)LOOP_GAIN_Y,
		2, (unsigned char *)&StCalDat.StLopGan.UsLygVal);

	e2p_read((unsigned short)LENS_CENTER_FINAL_X,
		2, (unsigned char *)&StCalDat.StLenCen.UsLsxVal);
	e2p_read((unsigned short)LENS_CENTER_FINAL_Y,
		2, (unsigned char *)&StCalDat.StLenCen.UsLsyVal);

	e2p_read((unsigned short)GYRO_AD_OFFSET_X,
		2, (unsigned char *)&StCalDat.StGvcOff.UsGxoVal);
	e2p_read((unsigned short)GYRO_AD_OFFSET_Y,
		2, (unsigned char *)&StCalDat.StGvcOff.UsGyoVal);

	e2p_read((unsigned short)GYRO_GAIN_X,
		4, (unsigned char *)&StCalDat.UlGxgVal);
	e2p_read((unsigned short)GYRO_GAIN_Y,
		4, (unsigned char *)&StCalDat.UlGygVal);

	e2p_read((unsigned short)OSC_CLK_VAL,
		2, (unsigned char *)&StCalDat.UsOscVal);

	e2p_read((unsigned short)SYSTEM_VERSION_INFO,
		2, (unsigned char *)&StCalDat.UsVerDat);
}

/*===========================================================================
 * Function Name: version_info
 * Retun Value: NON
 * Argment Value: NON
 * Explanation: F/W Version Check
 * History: First edition 2013.03.21 Y.Kim
 *==========================================================================*/
static void version_info(void)
{
	UcVerHig = (unsigned char)(StCalDat.UsVerDat >> 8); /* System Version */
	UcVerLow = (unsigned char)(StCalDat.UsVerDat);      /* Filter Version */
	CDBG("%s: %x, %x \n", __func__, UcVerHig, UcVerLow);

	if (UcVerHig == 0xFF)  /* If System Version Info not exist */
		UcVerHig = 0x00;

	if (UcVerLow == 0xFF)  /* If System Version Info not exist */
		UcVerLow = 0x00;
}

/*===========================================================================
 * Function Name: init_clk
 * Retun Value: NON
 * Argment Value: NON
 * Explanation: Clock Setting
 * History: First edition 2009.07.30 Y.Tashita
 * LC898111 changes 2011.04.08 d.yamagata
 *==========================================================================*/
static void init_clk(void)
{
	UcOscAdjFlg = 0; /* Osc adj flag */

	reg_write_addr(CLKON,0x13); /* 0x020B [-|-|CmCalClkOn|CMGifClkOn|CmPezClkOn|CmEepClkOn|CmSrvClkOn|CmPwmClkOn] */
}

/*===========================================================================
 * Function Name: init_io_port
 * Retun Value: NON
 * Argment Value: NON
 * Explanation: I/O Port Initial Setting
 * History: First edition 2009.07.30 Y.Tashita
 * LC898111 changes 2011.04.08 d.yamagata
 *==========================================================================*/
static void init_io_port(void)
{
	/*select IOP signal*/
	reg_write_addr(IOP0SEL, 0x00); /* 0x0230 [1:0] 00: DGMOSI, 01: HPS_CTL0, 1x: IOP0 */
	reg_write_addr(IOP3SEL, 0x01); /* 0x0233 [5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND */
	/* [1:0] 00: DGINT, 01: MON, 1x: IOP3 */
	reg_write_addr(IOP4SEL, 0x00); /* 0x0234 [5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND */
	/* [1:0] 00: BUSY1/EPSIIF, 01: MON, 1x: IOP4 */
	reg_write_addr(IOP5SEL, 0x21); /* 0x0235 [5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND */
	/* [1:0] 00: BUSY2, 01: MON, 1x: IOP5 */
	reg_write_addr(IOP6SEL, 0x11); /* 0x0236 [5:4] 00: MONA, 01: MONB, 10: MONC, 11: MOND */
	/* [1:0] 00: EPSIIF, 01: MON, 1x: IOP6 */
	reg_write_addr(SRMODE, 0x02); /* 0x0251 [1] 0: SRAM DL ON, 1: OFF */
	/* [0] 0: USE SRAM OFF, 1: ON */
}

static int acc_wait(unsigned char UcTrgDat)
{
	unsigned char UcFlgVal = 1;
	unsigned char UcCntPla = 0;

	do {
		reg_read_addr(GRACC, &UcFlgVal);
		UcFlgVal &= UcTrgDat;
		UcCntPla++;
	} while (UcFlgVal && (UcCntPla < ACCWIT_POLLING_LIMIT_A));

	if (UcCntPla == ACCWIT_POLLING_LIMIT_A)
		return OIS_FW_POLLING_FAIL;

	return OIS_FW_POLLING_PASS;
}

static int init_digital_gyro(void)
{
	unsigned char UcRegBak;
	unsigned char UcRamIni0, UcRamIni1, UcCntPla, UcCntPlb;

	UcCntPla = 0;
	UcCntPlb = 0;

	/*Gyro Filter Setting*/
	reg_write_addr(GGADON, 0x01);   /* 0x011C [- | - | - | CmSht2PanOff][- | - | CmGadOn(1:0)] */
	reg_write_addr(GGADSMPT, 0x0E); /* 0x011F */
	/*Set to Command Mode*/
	reg_write_addr(GRSEL, 0x01);    /* 0x0380 [- | - | - | -][- | SRDMOE | OISMODE | COMMODE] */

	/*Digital Gyro Read settings*/
	reg_write_addr(GRINI, 0x80);    /* 0x0381 [PARA_REG | AXIS7EN | AXIS4EN | -][LSBF | SLOWMODE | I2CMODE | -] */

	/* IDG-2021 Register Write Max1MHz */
	reg_read_addr(GIFDIV, &UcRegBak);
	reg_write_addr(GIFDIV, 0x04);   /* 48MHz / 4 = 12MHz */
	reg_write_addr(GRINI, 0x84);    /* SPI Clock = Slow Mode 12MHz / 4 / 4 = 750KHz */

	/* Gyro Clock Setting */
	reg_write_addr(GRADR0, 0x6B);  /* 0x0383 Set USER CONTROL */
	reg_write_addr(GSETDT, 0x01);  /* 0x038A Set Write Data */
	reg_write_addr(GRACC, 0x10);   /* 0x0382 [ADRPLUS(1:0) | - | WR1B][- | RD4B | RD2B | RD1B] */

	if (acc_wait(0x10) == OIS_FW_POLLING_FAIL)
		return OIS_FW_POLLING_FAIL; /* Digital Gyro busy wait */

	UcRamIni0 = 0x02;
	do {
		reg_write_addr(GRACC, 0x01);         /* 0x0382 Set 1Byte Read Trigger ON */
		if (acc_wait(0x01) == OIS_FW_POLLING_FAIL)
			return OIS_FW_POLLING_FAIL; /* Digital Gyro busy wait */
		reg_read_addr(GRADT0H, &UcRamIni0);  /* 0x0390 */
		UcCntPla++;
	} while ((UcRamIni0 & 0x02) && (UcCntPla < INIDGY_POLLING_LIMIT_A));

	if (UcCntPla == INIDGY_POLLING_LIMIT_A)
		return OIS_FW_POLLING_FAIL;

	with_time(2);
	spigyrocheck = UcRamIni0;

	reg_write_addr(GRADR0, 0x1B);           /* 0x0383 Set GYRO_CONFIG */
	reg_write_addr(GSETDT, (FS_SEL << 3));  /* 0x038A Set Write Data:FS_SEL 2; 0x10: */
	reg_write_addr(GRACC, 0x10);            /* 0x0382 Set Trigger ON */

	if (acc_wait(0x10) == OIS_FW_POLLING_FAIL)
		return OIS_FW_POLLING_FAIL;    /* Digital Gyro busy wait */

	reg_write_addr(GRADR0, 0x6A);  /* 0x0383 Set USER CONTROL */
	reg_write_addr(GSETDT, 0x11);  /* 0x038A Set Write Data */
	reg_write_addr(GRACC, 0x10);   /* 0x0382 [ADRPLUS(1:0) | - | WR1B][- | RD4B | RD2B | RD1B] */

	if (acc_wait(0x10) == OIS_FW_POLLING_FAIL)
		return OIS_FW_POLLING_FAIL;     /* Digital Gyro busy wait */

	UcRamIni1 = 0x01;
	do {
		reg_write_addr(GRACC, 0x01);         /* 0x0382 Set 1Byte Read Trigger ON */
		if (acc_wait(0x01) == OIS_FW_POLLING_FAIL)
			return OIS_FW_POLLING_FAIL; /* Digital Gyro busy wait */
		reg_read_addr(GRADT0H, &UcRamIni1);  /* 0x0390 */
		UcCntPlb++;
	} while ((UcRamIni1 & 0x01) && (UcCntPlb < INIDGY_POLLING_LIMIT_B));

	if (UcCntPlb == INIDGY_POLLING_LIMIT_B)
		return OIS_FW_POLLING_FAIL;

	reg_write_addr(GIFDIV, UcRegBak); /* 48MHz / 3 = 16MHz */
	reg_write_addr(GRINI, 0x80);      /* SPI Clock = 16MHz / 4 = 4MHz */

	/* Gyro Signal Output Select */
	reg_write_addr(GRADR0, 0x43);     /* 0x0383 Set Gyro XOUT H~L */
	reg_write_addr(GRADR1, 0x45);     /* 0x0384 Set Gyro YOUT H~L */

	/* Start OIS Reading */
	reg_write_addr(GRSEL, 0x02);      /* 0x0380 [- | - | - | -][- | SRDMOE | OISMODE | COMMODE] */

	return OIS_FW_POLLING_PASS;
}

static void init_mon(void)
{
	reg_write_addr(PWMMONFC, 0x00);   /* 0x00F4 */
	reg_write_addr(DAMONFC, 0x81);    /* 0x00F5 */

	reg_write_addr(MONSELA, 0x57);    /* 0x0270 */
	reg_write_addr(MONSELB, 0x58);    /* 0x0271 */
	reg_write_addr(MONSELC, 0x56);    /* 0x0272 */
	reg_write_addr(MONSELD, 0x63);    /* 0x0273 */
}

static void drv_sw(unsigned char Ucdrv_sw)
{
	if (Ucdrv_sw == ON)
		reg_write_addr(DRVFC, 0xE3); /* 0x0070 MODE=2,Drvier Block Ena=1,FullMode=1 */
	else
		reg_write_addr(DRVFC, 0x00); /* 0x0070 Drvier Block Ena=0 */
}

/*===========================================================================
 * Function Name: init_srv
 * Retun Value: NON
 * Argment Value: NON
 * Explanation: Servo Initial Setting
 * History: First edition 2009.07.30 Y.Tashita
 *==========================================================================*/
static void init_srv(void)
{
	reg_write_addr(LXEQEN, 0x00);      /* 0x0084 */
	reg_write_addr(LYEQEN, 0x00);      /* 0x008E */

	ram_write_addr(ADHXOFF, 0x0000);   /* 0x1102 */
	ram_write_addr(ADSAD4OFF, 0x0000); /* 0x110E */
	ram_write_addr(HXINOD, 0x0000);    /* 0x1127 */
	ram_write_addr(HXDCIN, 0x0000);    /* 0x1126 */
	ram_write_addr(HXSEPT1, 0x0000);   /* 0x1123 */
	ram_write_addr(HXSEPT2, 0x0000);   /* 0x1124 */
	ram_write_addr(HXSEPT3, 0x0000);   /* 0x1125 */
	ram_write_addr(LXDOBZ, 0x0000);    /* 0x114A */
	ram_write_addr(LXFZF, 0x0000);     /* 0x114B */
	ram_write_addr(LXFZB, 0x0000);     /* 0x1156 */
	ram_write_addr(LXDX, 0x0000);      /* 0x1148 */
	ram_write_addr(LXLMT, 0x7FFF);     /* 0x1157 */
	ram_write_addr(LXLMT2, 0x7FFF);    /* 0x1158 */
	ram_write_addr(LXLMTSD, 0x0000);   /* 0x1159 */
	ram_write_addr(PLXOFF, 0x0000);    /* 0x115B */
	ram_write_addr(LXDODAT, 0x0000);   /* 0x115A */

	ram_write_addr(ADHYOFF, 0x0000);   /* 0x1105 */
	ram_write_addr(HYINOD, 0x0000);    /* 0x1167 */
	ram_write_addr(HYDCIN, 0x0000);    /* 0x1166 */
	ram_write_addr(HYSEPT1, 0x0000);   /* 0x1163 */
	ram_write_addr(HYSEPT2, 0x0000);   /* 0x1164 */
	ram_write_addr(HYSEPT3, 0x0000);   /* 0x1165 */
	ram_write_addr(LYDOBZ, 0x0000);    /* 0x118A */
	ram_write_addr(LYFZF, 0x0000);     /* 0x118B */
	ram_write_addr(LYFZB, 0x0000);     /* 0x1196 */
	ram_write_addr(LYDX, 0x0000);      /* 0x1188 */
	ram_write_addr(LYLMT, 0x7FFF);     /* 0x1197 */
	ram_write_addr(LYLMT2, 0x7FFF);    /* 0x1198 */
	ram_write_addr(LYLMTSD, 0x0000);   /* 0x1199 */
	ram_write_addr(PLYOFF, 0x0000);    /* 0x119B */
	ram_write_addr(LYDODAT, 0x0000);   /* 0x119A */

	ram_write_addr(LXOLMT, 0x2C00);    /* 0x121A: 165mA Limiter for CVL Mode */
	ram_write_addr(LYOLMT, 0x2C00);    /* 0x121B: 165mA Limiter for CVL Mode */
	reg_write_addr(LXEQFC2, 0x09);     /* 0x0083 Linear Mode ON */
	reg_write_addr(LYEQFC2, 0x09);     /* 0x008D */

	/* Calculation flow X: Y1->X1 Y: X2->Y2 */
	reg_write_addr(LCXFC, (unsigned char)0x00);         /* 0x0001 High-order function X function setting */
	reg_write_addr(LCYFC, (unsigned char)0x00);         /* 0x0006 High-order function Y function setting */

	reg_write_addr(LCY1INADD, (unsigned char)LXDOIN);   /* 0x0007 High-order function Y1 input selection */
	reg_write_addr(LCY1OUTADD, (unsigned char)DLY00);   /* 0x0008 High-order function Y1 output selection */
	reg_write_addr(LCX1INADD, (unsigned char)DLY00);    /* 0x0002 High-order function X1 input selection */
	reg_write_addr(LCX1OUTADD, (unsigned char)LXADOIN); /* 0x0003 High-order function X1 output selection */

	reg_write_addr(LCX2INADD, (unsigned char)LYDOIN);   /* 0x0004 High-order function X2 input selection */
	reg_write_addr(LCX2OUTADD, (unsigned char)DLY01);   /* 0x0005 High-order function X2 output selection */
	reg_write_addr(LCY2INADD, (unsigned char)DLY01);    /* 0x0009 High-order function Y2 input selection */
	reg_write_addr(LCY2OUTADD, (unsigned char)LYADOIN); /* 0x000A High-order function Y2 output selection */

	/* (0.5468917X^3+0.3750114X)*(0.5468917X^3+0.3750114X) 6.5ohm */
	ram_write_addr(LCY1A0, 0x0000);    /* 0x12F2 0 */
	ram_write_addr(LCY1A1, 0x4600);    /* 0x12F3 1 */
	ram_write_addr(LCY1A2, 0x0000);    /* 0x12F4 2 */
	ram_write_addr(LCY1A3, 0x3000);    /* 0x12F5 3 */
	ram_write_addr(LCY1A4, 0x0000);    /* 0x12F6 4 */
	ram_write_addr(LCY1A5, 0x0000);    /* 0x12F7 5 */
	ram_write_addr(LCY1A6, 0x0000);    /* 0x12F8 6 */

	ram_write_addr(LCX1A0, 0x0000);    /* 0x12D2 0 */
	ram_write_addr(LCX1A1, 0x4600);    /* 0x12D3 1 */
	ram_write_addr(LCX1A2, 0x0000);    /* 0x12D4 2 */
	ram_write_addr(LCX1A3, 0x3000);    /* 0x12D5 3 */
	ram_write_addr(LCX1A4, 0x0000);    /* 0x12D6 4 */
	ram_write_addr(LCX1A5, 0x0000);    /* 0x12D7 5 */
	ram_write_addr(LCX1A6, 0x0000);    /* 0x12D8 6 */

	ram_write_addr(LCX2A0, 0x0000);    /* 0x12D9 0 */
	ram_write_addr(LCX2A1, 0x4600);    /* 0x12DA 1 */
	ram_write_addr(LCX2A2, 0x0000);    /* 0x12DB 2 */
	ram_write_addr(LCX2A3, 0x3000);    /* 0x12DC 3 */
	ram_write_addr(LCX2A4, 0x0000);    /* 0x12DD 4 */
	ram_write_addr(LCX2A5, 0x0000);    /* 0x12DE 5 */
	ram_write_addr(LCX2A6, 0x0000);    /* 0x12DF 6 */

	ram_write_addr(LCY2A0, 0x0000);    /* 0x12F9 0 */
	ram_write_addr(LCY2A1, 0x4600);    /* 0x12FA 1 */
	ram_write_addr(LCY2A2, 0x0000);    /* 0x12FB 2 */
	ram_write_addr(LCY2A3, 0x3000);    /* 0x12FC 3 */
	ram_write_addr(LCY2A4, 0x0000);    /* 0x12FD 4 */
	ram_write_addr(LCY2A5, 0x0000);    /* 0x12FE 5 */
	ram_write_addr(LCY2A6, 0x0000);    /* 0x12FF 6 */

	reg_write_addr(GDPX1INADD, 0x00);  /* 0x00A5 Default Setting */
	reg_write_addr(GDPX1OUTADD, 0x00); /* 0x00A6 General Data Pass Output Cut Off */
	reg_write_addr(GDPX2INADD, 0x00);  /* 0x00A7 Default Setting */
	reg_write_addr(GDPX2OUTADD, 0x00); /* 0x00A8 General Data Pass Output Cut Off */

    /* Gyro Filter Interface */
	reg_write_addr(GYINFC, 0x00);      /* 0x00DA LXGZB,LYGZB Input Cut Off, 0 Sampling Delay, Down Sampling 1/1 */

    /* Sin Wave Generater */
	reg_write_addr(SWEN, 0x08);        /* 0x00DB Sin Wave Generate OFF, Sin Wave Setting */
	reg_write_addr(SWFC2, 0x08);       /* 0x00DE SWC = 0 */

	/* Delay RAM Monitor */
	reg_write_addr(MDLY1ADD, 0x10);    /* 0x00E5 Delay Monitor1 */
	reg_write_addr(MDLY2ADD, 0x11);    /* 0x00E6 Delay Monitor2 */

   /* Delay RAM Clear */
	reg_write_addr(DLYCLR, 0xFF);      /* 0x00EE Delay RAM All Clear */
	with_time(1);
	reg_write_addr(DLYCLR2, 0xEC);     /* 0x00EF Delay RAM All Clear */
	with_time(1);
	reg_write_addr(DLYCLR, 0x00);      /* 0x00EE CLR disable */

	/* Hall Amp... */
	reg_write_addr(RTXADD, 0x00);      /* 0x00CE Cal OFF */
	reg_write_addr(RTYADD, 0x00);      /* 0x00E8 Cal OFF */

	/* PWM Signal Generate */
	drv_sw(OFF);                   /* 0x0070 Driver Block Ena=0 */
	reg_write_addr(DRVFC2, 0x40);      /* 0x0068 PriDriver:Slope, Driver:DeadTime 12.5ns */

	reg_write_addr(PWMFC, 0x5C);       /* 0x0075 VREF, PWMCLK/64, MODE1, 12Bit Accuracy */
	reg_write_addr(PWMPERIODX, 0x00);  /* 0x0062 */
	reg_write_addr(PWMPERIODY, 0x00);  /* 0x0063 */

	reg_write_addr(PWMA, 0x00);        /* 0x0074 PWM X/Y standby */
	reg_write_addr(PWMDLY1, 0x03);     /* 0x0076 X Phase Delay Setting */
	reg_write_addr(PWMDLY2, 0x03);     /* 0x0077 Y Phase Delay Setting */

	reg_write_addr(LNA, 0xC0);         /* 0x0078 Low Noise mode enable */
	reg_write_addr(LNFC, 0x02);        /* 0x0079 */
	reg_write_addr(LNSMTHX, 0x80);     /* 0x007A */
	reg_write_addr(LNSMTHY, 0x80);     /* 0x007B */

	/* Flag Monitor */
	reg_write_addr(FLGM, 0xCC);        /* 0x00F8 BUSY2 Output ON */
	reg_write_addr(FLGIST, 0xCC);      /* 0x00F9 Interrupt Clear */
	reg_write_addr(FLGIM2, 0xF8);      /* 0x00FA BUSY2 Output ON */
	reg_write_addr(FLGIST2, 0xF8);     /* 0x00FB Interrupt Clear */

	/* Function Setting */
	reg_write_addr(FCSW, 0x00);        /* 0x00F6 2Axis Input, PWM Mode, X,Y Axis Reverse OFF */
	reg_write_addr(FCSW2, 0x00);       /* 0x00F7 X,Y Axis Invert OFF, PWM Synchronous, A/D Over Sampling ON */

	/* Srv Smooth start */
	ram_write_addr(HXSMSTP, 0x0400);   /* 0x1120 */
	ram_write_addr(HYSMSTP, 0x0400);   /* 0x1160 */

	reg_write_addr(SSSFC1, 0x43);      /* 0x0098 0.68ms * 8times = 5.46ms */
	reg_write_addr(SSSFC2, 0x03);      /* 0x0099 1.36ms * 3 = 4.08ms */
	reg_write_addr(SSSFC3, 0x50);      /* 0x009A 1.36ms */

	reg_write_addr(STBB, 0x00);        /* 0x0260 All standby */
}

static int clear_gyro(unsigned char UcClrFil, unsigned char UcClrMod)
{
	unsigned char UcRamClr;
	unsigned char UcCntPla = 0;

	/* Select Filter to clear*/
	reg_write_addr(GRAMDLYMOD, UcClrFil); /* 0x011B [- | - | - | P][T | L | H | I] */

	/* Enable Clear*/
	reg_write_addr(GRAMINITON, UcClrMod); /* 0x0103 [- | - | - | -][- | - | Clr | Clr] */

	/* Check RAM Clear complete*/
	do {
		reg_read_addr(GRAMINITON, &UcRamClr);
		UcRamClr &= 0x03;
		UcCntPla++;
	} while ((UcRamClr != 0x00) && (UcCntPla < CLRGYR_POLLING_LIMIT_A));

	if (UcCntPla == CLRGYR_POLLING_LIMIT_A)
		return OIS_FW_POLLING_FAIL;

	return OIS_FW_POLLING_PASS;
}

static void set_pan_tilt_mode(unsigned char UcPnTmod)
{
	switch (UcPnTmod) {
	case OFF:
		reg_write_addr(GPANON, 0x00); /* 0x0109 X,Y Pan/Tilt Function OFF */
		break;
	case ON:
		reg_write_addr(GPANON, 0x11); /* 0x0109 X,Y Pan/Tilt Function ON */
		break;
	}
}

static void init_gyro(void)
{
	reg_write_addr(GEQON, 0x00);       /* 0x0100 [- | - | - | -][- | - | - | CmEqOn] */

	/*Initialize Gyro RAM*/
	clear_gyro(0x00, 0x03);

	/*Gyro Filter Setting*/
	reg_write_addr(GGADON, 0x01);     /* 0x011C [- | - | - | CmSht2PanOff][- | - | CmGadOn(1:0)] */
	reg_write_addr(GGADSMPT, 0x0E);   /* 0x011F */

	/* Limiter */
	ram_write_32addr(gxlmt1L, 0x00000000); /* 0x18B0 */
	ram_write_32addr(gxlmt1H, 0x3F800000); /* 0x18B1 */
	ram_write_32addr(gxlmt2L, 0x3B16BB99); /* 0x18B2 0.0023 */
	ram_write_32addr(gxlmt2H, 0x3F800000); /* 0x18B3 */

	ram_write_32addr(gylmt1L, 0x00000000); /* 0x19B0 */
	ram_write_32addr(gylmt1H, 0x3F800000); /* 0x19B1 */
	ram_write_32addr(gylmt2L, 0x3B16BB99); /* 0x19B2 0.0023 */
	ram_write_32addr(gylmt2H, 0x3F800000); /* 0x19B3 */

	/* Limiter3 */
	ram_write_32addr(gxlmt3H0, 0x3E800000); /* 0x18B4 */
	ram_write_32addr(gylmt3H0, 0x3E800000); /* 0x19B4 Limiter = 0.25, 0.7deg */
	ram_write_32addr(gxlmt3H1, 0x3E800000); /* 0x18B5 */
	ram_write_32addr(gylmt3H1, 0x3E800000); /* 0x19B5 */

	ram_write_32addr(gxlmt4H0, GYRLMT_S1);  /* 0x1808 X Axis Limiter4 High Threshold 0 */
	ram_write_32addr(gylmt4H0, GYRLMT_S1);  /* 0x1908 Y Axis Limiter4 High Threshold 0 */

	ram_write_32addr(gxlmt4H1, GYRLMT_S2);  /* 0x1809 X Axis Limiter4 High Threshold 1 */
	ram_write_32addr(gylmt4H1, GYRLMT_S2);  /* 0x1909 Y Axis Limiter4 High Threshold 1 */

	/* Monitor Circuit */
	reg_write_addr(GDLYMON10, 0xF5);        /* 0x0184 */
	reg_write_addr(GDLYMON11, 0x01);        /* 0x0185 */
	reg_write_addr(GDLYMON20, 0xF5);        /* 0x0186 */
	reg_write_addr(GDLYMON21, 0x00);        /* 0x0187 */
	ram_write_32addr(gdm1g, 0x3F800000);    /* 0x18AC */
	ram_write_32addr(gdm2g, 0x3F800000);    /* 0x19AC */

	/* Pan/Tilt parameter */
	reg_write_addr(GPANADDA, 0x14);         /* 0x0130 GXH1Z2/GYH1Z2 */
	reg_write_addr(GPANADDB, 0x0E);         /* 0x0131 GXI3Z/GYI3Z */

	/* Threshold */
	ram_write_32addr(SttxHis, 0x00000000);      /* 0x183F */
	ram_write_32addr(SttyHis, 0x00000000);      /* 0x193F */
	ram_write_32addr(SttxaL, 0x00000000);       /* 0x18AE */
	ram_write_32addr(SttxbL, 0x00000000);       /* 0x18BE */
	ram_write_32addr(Sttx12aM, GYRA12_MID_DEG); /* 0x184F */
	ram_write_32addr(Sttx12aH, GYRA12_HGH_DEG); /* 0x185F */
	ram_write_32addr(Sttx12bM, GYRB12_MID);     /* 0x186F */
	ram_write_32addr(Sttx12bH, GYRB12_HGH);     /* 0x187F */
	ram_write_32addr(Sttx34aM, GYRA34_MID_DEG); /* 0x188F */
	ram_write_32addr(Sttx34aH, GYRA34_HGH_DEG); /* 0x189F */
	ram_write_32addr(Sttx34bM, GYRB34_MID);     /* 0x18AF */
	ram_write_32addr(Sttx34bH, GYRB34_HGH);     /* 0x18BF */
	ram_write_32addr(SttyaL, 0x00000000);       /* 0x19AE */
	ram_write_32addr(SttybL, 0x00000000);       /* 0x19BE */
	ram_write_32addr(Stty12aM, GYRA12_MID_DEG); /* 0x194F */
	ram_write_32addr(Stty12aH, GYRA12_HGH_DEG); /* 0x195F */
	ram_write_32addr(Stty12bM, GYRB12_MID);     /* 0x196F */
	ram_write_32addr(Stty12bH, GYRB12_HGH);     /* 0x197F */
	ram_write_32addr(Stty34aM, GYRA34_MID_DEG); /* 0x198F */
	ram_write_32addr(Stty34aH, GYRA34_HGH_DEG); /* 0x199F */
	ram_write_32addr(Stty34bM, GYRB34_MID);     /* 0x19AF */
	ram_write_32addr(Stty34bH, GYRB34_HGH);     /* 0x19BF */

	/* Phase Transition Setting */
	reg_write_addr(GPANSTT31JUG0, 0x01);        /* 0x0142 */
	reg_write_addr(GPANSTT31JUG1, 0x00);        /* 0x0143 */
	reg_write_addr(GPANSTT13JUG0, 0x00);        /* 0x0148 */
	reg_write_addr(GPANSTT13JUG1, 0x07);        /* 0x0149 */

	/* State Timer */

	reg_write_addr(GPANSTT1LEVTMR, 0x00);       /* 0x0160 */

	/* Control filter */
	reg_write_addr(GPANTRSON0, 0x01);           /* 0x0132: gxgain/gygain, gxistp/gyistp */
	reg_write_addr(GPANTRSON1, 0x1C);           /* 0x0133: I Filter Control */

	/* State Setting */
	reg_write_addr(GPANSTTSETGAIN, 0x10);       /* 0x0155 */
	reg_write_addr(GPANSTTSETISTP, 0x10);       /* 0x0156 */
	reg_write_addr(GPANSTTSETI1FTR, 0x10);      /* 0x0157 */
	reg_write_addr(GPANSTTSETI2FTR, 0x10);      /* 0x0158 */

	/* State2,4 Step Time Setting */
	reg_write_addr(GPANSTT2TMR0, 0xEA);         /* 0x013C */
	reg_write_addr(GPANSTT2TMR1, 0x00);         /* 0x013D */
	reg_write_addr(GPANSTT4TMR0, 0x92);         /* 0x013E */
	reg_write_addr(GPANSTT4TMR1, 0x04);         /* 0x013F */

	reg_write_addr(GPANSTTXXXTH, 0x0F);         /* 0x015D */

	reg_write_addr(GPANSTTSETILHLD, 0x00);      /* 0x0168 */

	ram_write_32addr(gxlevmid, TRI_LEVEL);      /* 0x182D Low Th */
	ram_write_32addr(gxlevhgh, TRI_HIGH);       /* 0x182E Hgh Th */
	ram_write_32addr(gylevmid, TRI_LEVEL);      /* 0x192D Low Th */
	ram_write_32addr(gylevhgh, TRI_HIGH);       /* 0x192E Hgh Th */
	ram_write_32addr(gxadjmin, XMINGAIN);       /* 0x18BA Low gain */
	ram_write_32addr(gxadjmax, XMAXGAIN);       /* 0x18BB Hgh gain */
	ram_write_32addr(gxadjdn, XSTEPDN);         /* 0x18BC -step */
	ram_write_32addr(gxadjup, XSTEPUP);         /* 0x18BD +step */
	ram_write_32addr(gyadjmin, YMINGAIN);       /* 0x19BA Low gain */
	ram_write_32addr(gyadjmax, YMAXGAIN);       /* 0x19BB Hgh gain */
	ram_write_32addr(gyadjdn, YSTEPDN);         /* 0x19BC -step */
	ram_write_32addr(gyadjup, YSTEPUP);         /* 0x19BD +step */

	reg_write_addr(GLEVGXADD, (unsigned char)XMONADR); /* 0x0120 Input signal */
	reg_write_addr(GLEVGYADD, (unsigned char)YMONADR); /* 0x0124 Input signal */
	reg_write_addr(GLEVTMR, TIMEBSE);          /* 0x0128 Base Time */
	reg_write_addr(GLEVTMRLOWGX, TIMELOW);     /* 0x0121 X Low Time */
	reg_write_addr(GLEVTMRMIDGX, TIMEMID);     /* 0x0122 X Mid Time */
	reg_write_addr(GLEVTMRHGHGX, TIMEHGH);     /* 0x0123 X Hgh Time */
	reg_write_addr(GLEVTMRLOWGY, TIMELOW);     /* 0x0125 Y Low Time */
	reg_write_addr(GLEVTMRMIDGY, TIMEMID);     /* 0x0126 Y Mid Time */
	reg_write_addr(GLEVTMRHGHGY, TIMEHGH);     /* 0x0127 Y Hgh Time */
	reg_write_addr(GADJGANADD, (unsigned char)GANADR); /* 0x012A control address */
	reg_write_addr(GADJGANGO, 0x00);           /* 0x0108 manual off */

	set_pan_tilt_mode(OFF);                  /* Pan/Tilt OFF */
}

static int init_hll_filter(void)
{
	unsigned short UsAryIda, UsAryIdb;

	/* Hall&Gyro Register Parameter Setting */
	UsAryIda = 0;
	while (CsHalReg[UcVerLow][UsAryIda].UsRegAdd != 0xFFFF) {
		reg_write_addr(CsHalReg[UcVerLow][UsAryIda].UsRegAdd,
			CsHalReg[UcVerLow][UsAryIda].UcRegDat);
		UsAryIda++;
		if (UsAryIda > HALREGTAB)
			return OIS_FW_POLLING_FAIL;
	}

	/* Hall Filter Parameter Setting */
	UsAryIdb = 0;
	while (CsHalFil[UcVerLow][UsAryIdb].UsRamAdd != 0xFFFF) {
		if (CsHalFil[UcVerLow][UsAryIdb].UsRamAdd < gag)
			ram_write_addr(CsHalFil[UcVerLow][UsAryIdb].UsRamAdd,
				CsHalFil[UcVerLow][UsAryIdb].UsRamDat);
		UsAryIdb++;
		if (UsAryIdb > HALFILTAB)
			return OIS_FW_POLLING_FAIL;
	}

	if ((unsigned char)(StCalDat.UsVerDat) <= (unsigned char)0x01) /* X Reverse */
		ram_write_addr(plxg, 0x8001);

	return OIS_FW_POLLING_PASS;
}

static int init_gyro_filter(void)
{
	unsigned short UsAryIda;

	/* Gyro Filter Parameter Setting */
	UsAryIda = 0;

	while (CsGyrFil[UcVerLow][UsAryIda].UsRamAdd != 0xFFFF) {
		if ((CsGyrFil[UcVerLow][UsAryIda].UsRamAdd & 0xFEFF) < gxi2a_a)
			ram_write_32addr(CsGyrFil[UcVerLow][UsAryIda].UsRamAdd,
				CsGyrFil[UcVerLow][UsAryIda].UlRamDat);
		UsAryIda++;
		if (UsAryIda > GYRFILTAB)
			return OIS_FW_POLLING_FAIL;
	}
	return OIS_FW_POLLING_PASS;
}

static void auto_gain_ctrl_sw(unsigned char UcModeSw)
{
	if (UcModeSw == OFF) {
		reg_write_addr(GADJGANGXMOD, 0x00); /* 0x0108 Gain Down */
		reg_write_addr(GADJGANGYMOD, 0x00); /* 0x0108 Gain Down */
		reg_write_addr(GADJGANGO, 0x11);    /* 0x0108 Gain Down */
	} else {
		reg_write_addr(GADJGANGO, 0x00);    /* 0x0108 Gain Up */
		reg_write_addr(GADJGANGXMOD, 0xA7); /* 0x0108 Gain Down */
		reg_write_addr(GADJGANGYMOD, 0xA7); /* 0x0108 Gain Down */
	}
}

static void init_adjust(void)
{
	reg_write_addr(CMSDAC, BIAS_CUR); /* 0x0261 Hall Dac Current */
	reg_write_addr(OPGSEL, AMP_GAIN); /* 0x0262 Hall Amp Gain */

	/* Hall Xaxis Bias,Offset */
	if ((StCalDat.UsAdjCompF == 0x0000) ||
		(StCalDat.UsAdjCompF & (EXE_HXADJ - EXE_END))) {
		ram_write_addr(DAHLXO, DAHLXO_INI); /* 0x1114 */
		ram_write_addr(DAHLXB, DAHLXB_INI); /* 0x1115 */
		ram_write_addr(ADHXOFF, 0x0000);    /* 0x1102 */
	} else {
		ram_write_addr(DAHLXO, StCalDat.StHalAdj.UsHlxOff);  /* 0x1114 */
		ram_write_addr(DAHLXB, StCalDat.StHalAdj.UsHlxGan);  /* 0x1115 */
		ram_write_addr(ADHXOFF, StCalDat.StHalAdj.UsAdxOff); /* 0x1102 */
	}

	/* Hall Yaxis Bias,Offset */
	if ((StCalDat.UsAdjCompF == 0x0000) ||
		(StCalDat.UsAdjCompF & (EXE_HYADJ - EXE_END))) {
		ram_write_addr(DAHLYO, DAHLYO_INI); /* 0x1116 */
		ram_write_addr(DAHLYB, DAHLYB_INI); /* 0x1117 */
		ram_write_addr(ADHYOFF, 0x0000);    /* 0x1105 */
	} else {
		ram_write_addr(DAHLYO, StCalDat.StHalAdj.UsHlyOff);  /* 0x1116 */
		ram_write_addr(DAHLYB, StCalDat.StHalAdj.UsHlyGan);  /* 0x1117 */
		ram_write_addr(ADHYOFF, StCalDat.StHalAdj.UsAdyOff); /* 0x1105 */
	}

	/* Hall Xaxis Loop Gain */
	if ((StCalDat.UsAdjCompF == 0x0000) ||
		(StCalDat.UsAdjCompF & (EXE_LXADJ - EXE_END)))
		ram_write_addr(lxgain, LXGAIN_INI); /* 0x132A */
	else
		ram_write_addr(lxgain, StCalDat.StLopGan.UsLxgVal); /* 0x132A */

	/* Hall Yaxis Loop Gain */
	if ((StCalDat.UsAdjCompF == 0x0000) ||
		(StCalDat.UsAdjCompF & (EXE_LYADJ - EXE_END)))
		ram_write_addr(lygain, LYGAIN_INI); /* 0x136A */
	else
		ram_write_addr(lygain, StCalDat.StLopGan.UsLygVal); /* 0x136A */

	/* Lens Center */
	/* X axis Lens Center Offset Read & Setting */
	if ((StCalDat.StLenCen.UsLsxVal != 0x0000) &&
		(StCalDat.StLenCen.UsLsxVal != 0xffff))
		UsCntXof = StCalDat.StLenCen.UsLsxVal; /* Set Lens center X value */
	else
		UsCntXof = MECCEN_X; /* Clear Lens center X value */
	ram_write_addr(HXINOD, UsCntXof); /* 0x1127 */

	/* Y axis Lens Center Offset Read & Setting */
	if ((StCalDat.StLenCen.UsLsyVal != 0x0000) &&
		(StCalDat.StLenCen.UsLsyVal != 0xffff))
		UsCntYof = StCalDat.StLenCen.UsLsyVal; /* Set Lens center Y value */
	else
		UsCntYof = MECCEN_Y; /* Clear Lens center Y value */
	ram_write_addr(HYINOD, UsCntYof); /* 0x1167 */

	/* Gyro Xaxis Offset */
	if ((StCalDat.StGvcOff.UsGxoVal == 0x0000) ||
		(StCalDat.StGvcOff.UsGxoVal == 0xffff)) {
		ram_write_addr(ADGXOFF, 0x0000); /* 0x1108 */
		reg_write_addr(IZAH, DGYRO_OFST_XH); /* 0x03A0 Set Offset High byte */
		reg_write_addr(IZAL, DGYRO_OFST_XL); /* 0x03A1 Set Offset Low byte */
	} else {
		ram_write_addr(ADGXOFF, 0x0000);     /* 0x1108 */
		reg_write_addr(IZAH, (unsigned char)(StCalDat.StGvcOff.UsGxoVal >> 8)); /* 0x03A0 Set Offset High byte */
		reg_write_addr(IZAL, (unsigned char)(StCalDat.StGvcOff.UsGxoVal));      /* 0x03A1 Set Offset Low byte */
	}

	/* Gyro Yaxis Offset */
	if ((StCalDat.StGvcOff.UsGyoVal == 0x0000) ||
		(StCalDat.StGvcOff.UsGyoVal == 0xffff)) {
		ram_write_addr(ADGYOFF, 0x0000); /* 0x110B */
		reg_write_addr(IZBH, DGYRO_OFST_YH); /* 0x03A2 Set Offset High byte */
		reg_write_addr(IZBL, DGYRO_OFST_YL); /* 0x03A3 Set Offset Low byte */
	} else {
		ram_write_addr(ADGYOFF, 0x0000); /* 0x110B */
		reg_write_addr(IZBH, (unsigned char)(StCalDat.StGvcOff.UsGyoVal >> 8)); /* 0x03A2 Set Offset High byte */
		reg_write_addr(IZBL, (unsigned char)(StCalDat.StGvcOff.UsGyoVal));      /* 0x03A3 Set Offset Low byte */
	}

	/* Gyro Xaxis Gain */
	if ((StCalDat.UlGxgVal != 0x00000000) &&
		(StCalDat.UlGxgVal != 0xffffffff))
		ram_write_32addr(gxzoom, StCalDat.UlGxgVal); /* 0x1828 Gyro X axis Gain adjusted value */
	else
		ram_write_32addr(gxzoom, GXGAIN_INI); /* 0x1828 Gyro X axis Gain adjusted initial value */

	/* Gyro Yaxis Gain */
	if ((StCalDat.UlGygVal != 0x00000000) &&
		(StCalDat.UlGygVal != 0xffffffff))
		ram_write_32addr(gyzoom, StCalDat.UlGygVal); /* 0x1928 Gyro Y axis Gain adjusted value */
	else
		ram_write_32addr(gyzoom, GXGAIN_INI); /* 0x1928 Gyro Y axis Gain adjusted initial value */

	/* OSC Clock value */
	if (((unsigned char)StCalDat.UsOscVal != 0x00) &&
		((unsigned char)StCalDat.UsOscVal != 0xff))
		reg_write_addr(OSCSET, (unsigned char)((unsigned char)StCalDat.UsOscVal | 0x01)); /* 0x0264 */
	else
		reg_write_addr(OSCSET, OSC_INI); /* 0x0264 */

	ram_write_addr(hxinog, 0x8001); /* 0x1128 back up initial value */
	ram_write_addr(hyinog, 0x8001); /* 0x1168 back up initial value */

	reg_write_addr(STBB, 0x0F);     /* 0x0260 [- | - | - | -][STBOPAY | STBOPAX | STBDAC | STBADC] */

	reg_write_addr(LXEQEN, 0x45);   /* 0x0084 */
	reg_write_addr(LYEQEN, 0x45);   /* 0x008E */

	ram_write_32addr(gxistp_1, 0x00000000);
	ram_write_32addr(gyistp_1, 0x00000000);

	/* I Filter X: 1s */
	ram_write_32addr(gxi1a_1, 0x38A8A554); /* 0.3Hz */
	ram_write_32addr(gxi1b_1, 0xB3E6A3C6); /* Down */
	ram_write_32addr(gxi1c_1, 0x33E6A3C6); /* Up */

	ram_write_32addr(gxi1a_a, 0x38A8A554); /* 0.3Hz */
	ram_write_32addr(gxi1b_a, 0xB3E6A3C6); /* Down */
	ram_write_32addr(gxi1c_a, 0x33E6A3C6); /* Up */

	ram_write_32addr(gxi1a_b, 0x3AAF73A1); /* 5Hz */
	ram_write_32addr(gxi1b_b, 0xB3E6A3C6); /* Down */
	ram_write_32addr(gxi1c_b, 0x3F800000); /* Up */

	ram_write_32addr(gxi1a_c, 0x38A8A554); /* 0.3Hz */
	ram_write_32addr(gxi1b_c, 0xB3E6A3C6); /* Down */
	ram_write_32addr(gxi1c_c, 0x33E6A3C6); /* Up */

	/* I Filter Y */
	ram_write_32addr(gyi1a_1, 0x38A8A554); /* 0.3Hz */
	ram_write_32addr(gyi1b_1, 0xB3E6A3C6); /* Down */
	ram_write_32addr(gyi1c_1, 0x33E6A3C6); /* Up */

	ram_write_32addr(gyi1a_a, 0x38A8A554); /* 0.3Hz */
	ram_write_32addr(gyi1b_a, 0xB3E6A3C6); /* Down */
	ram_write_32addr(gyi1c_a, 0x33E6A3C6); /* Up */

	ram_write_32addr(gyi1a_b, 0x3AAF73A1); /* 5Hz */
	ram_write_32addr(gyi1b_b, 0xB3E6A3C6); /* Down */
	ram_write_32addr(gyi1c_b, 0x3F800000); /* Up */

	ram_write_32addr(gyi1a_c, 0x38A8A554); /* 0.3Hz */
	ram_write_32addr(gyi1b_c, 0xB3E6A3C6); /* Down */
	ram_write_32addr(gyi1c_c, 0x33E6A3C6); /* Up */

	/* gxgain: 0.4s */
	ram_write_32addr(gxl4a_1, 0x3F800000); /* 1 */
	ram_write_32addr(gxl4b_1, 0xB9DFB23C); /* Down */
	ram_write_32addr(gxl4c_1, 0x39DFB23C); /* Up */
	ram_write_32addr(gxl4a_a, 0x3F800000); /* 1 */
	ram_write_32addr(gxl4b_a, 0xB9DFB23C); /* Down */
	ram_write_32addr(gxl4c_a, 0x39DFB23C); /* Up */
	ram_write_32addr(gxl4a_b, 0x00000000); /* Cut Off */
	ram_write_32addr(gxl4b_b, 0xBF800000); /* Down */
	ram_write_32addr(gxl4c_b, 0x39DFB23C); /* Up */
	ram_write_32addr(gxl4a_c, 0x3F800000); /* 1 */
	ram_write_32addr(gxl4b_c, 0xB9DFB23C); /* Down */
	ram_write_32addr(gxl4c_c, 0x39DFB23C); /* Up */

	/* gygain */
	ram_write_32addr(gyl4a_1, 0x3F800000); /* 1 */
	ram_write_32addr(gyl4b_1, 0xB9DFB23C); /* Down */
	ram_write_32addr(gyl4c_1, 0x39DFB23C); /* Up */
	ram_write_32addr(gyl4a_a, 0x3F800000); /* 1 */
	ram_write_32addr(gyl4b_a, 0xB9DFB23C); /* Down */
	ram_write_32addr(gyl4c_a, 0x39DFB23C); /* Up */
	ram_write_32addr(gyl4a_b, 0x00000000); /* Cut Off */
	ram_write_32addr(gyl4b_b, 0xBF800000); /* Down */
	ram_write_32addr(gyl4c_b, 0x39DFB23C); /* Up */
	ram_write_32addr(gyl4a_c, 0x3F800000); /* 1 */
	ram_write_32addr(gyl4b_c, 0xB9DFB23C); /* Down */
	ram_write_32addr(gyl4c_c, 0x39DFB23C); /* Up */

	/* gxistp: 0.2s */
	ram_write_32addr(gxgyro_1, 0x00000000); /* Cut Off */
	ram_write_32addr(gxgain_1, 0xBF800000); /* Down */
	ram_write_32addr(gxistp_1, 0x3F800000); /* Up */
	ram_write_32addr(gxgyro_a, 0x00000000); /* Cut Off */
	ram_write_32addr(gxgain_a, 0xBF800000); /* Down */
	ram_write_32addr(gxistp_a, 0x3F800000); /* Up */
	ram_write_32addr(gxgyro_b, 0x37EC6C50); /* -91dB */
	ram_write_32addr(gxgain_b, 0xBF800000); /* Down */
	ram_write_32addr(gxistp_b, 0x3F800000); /* Up */
	ram_write_32addr(gxgyro_c, 0x00000000); /* Cut Off */
	ram_write_32addr(gxgain_c, 0xBF800000); /* Down */
	ram_write_32addr(gxistp_c, 0x3F800000); /* Up */

	/* gyistp */
	ram_write_32addr(gygyro_1, 0x00000000); /* Cut Off */
	ram_write_32addr(gygain_1, 0xBF800000); /* Down */
	ram_write_32addr(gyistp_1, 0x3F800000); /* Up */
	ram_write_32addr(gygyro_a, 0x00000000); /* Cut Off */
	ram_write_32addr(gygain_a, 0xBF800000); /* Down */
	ram_write_32addr(gyistp_a, 0x3F800000); /* Up */
	ram_write_32addr(gygyro_b, 0x37EC6C50); /* -91dB */
	ram_write_32addr(gygain_b, 0xBF800000); /* Down */
	ram_write_32addr(gyistp_b, 0x3F800000); /* Up */
	ram_write_32addr(gygyro_c, 0x00000000); /* Cut Off */
	ram_write_32addr(gygain_c, 0xBF800000); /* Down */
	ram_write_32addr(gyistp_c, 0x3F800000); /* Up */

	/* exe function */
	auto_gain_ctrl_sw(OFF); /* Auto Gain Control Mode OFF */

	drv_sw(ON); /* 0x0070 Driver Mode setting */

	reg_write_addr(GEQON, 0x01);       /* 0x0100 [- | - | - | -][- | - | - | CmEqOn] */
	reg_write_addr(GPANFILMOD, 0x01);  /* 0x0167 */
	set_pan_tilt_mode(ON);           /* Pan/Tilt ON */
	reg_write_addr(GPANSTTFRCE, 0x44); /* 0x010A */
	reg_write_addr(GLSEL, 0x04);       /* 0x017A */

	reg_write_addr(GHCHR, 0x11);       /* 0x017B */

	ram_write_32addr(gxl2a_2, 0x00000000); /* 0x1860 0 */
	ram_write_32addr(gxl2b_2, 0x3D829952); /* 0x1861 0.063769 */
	ram_write_32addr(gyl2a_2, 0x00000000); /* 0x1960 0 */
	ram_write_32addr(gyl2b_2, 0x3D829952); /* 0x1961 0.063769 */

	ram_write_32addr(gxl2c_2, 0xBA9CD414); /* 0x1862 -0.001196506 3F7FF800 */
	ram_write_32addr(gyl2c_2, 0xBA9CD414); /* 0x1962 -0.001196506 3F7FF800 */

	ram_write_32addr(gxh1c_2, 0x3F7FFD80); /* 0x18A2 */
	ram_write_32addr(gyh1c_2, 0x3F7FFD80); /* 0x19A2 */

	reg_write_addr(GPANSTTFRCE, 0x11);  /* 0x010A */
}

/*===========================================================================
 * Function Name: init_set
 * Retun Value: NON
 * Argment Value: NON
 * Explanation: OIS initialization
 * History:
 *==========================================================================*/
static int init_set(void)
{

	with_time(5);
	reg_write_addr(SOFRES1, 0x00); /* Software Reset */
	with_time(5);
	reg_write_addr(SOFRES1, 0x11);

	/* Read calibration data from E2PROM */
	e2p_data();
	/* Version Check */
	version_info();
	/* Clock Setting */
	init_clk();
	/* I/O Port Initial Setting */
	init_io_port();
	/* Monitor & Other Initial Setting */
	init_mon();
	/* Servo Initial Setting */
	init_srv();
	/* Gyro Filter Initial Setting */
	init_gyro(); /*Pan OFF*/

	/* POLLING EXCEPTION Setting */
	/* Hll Filter Initial Setting */
	if (init_hll_filter() != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_FAIL;

	/* Gyro Filter Initial Setting */
	if (init_gyro_filter() != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_FAIL;

	/* DigitalGyro Initial Setting */
	if (init_digital_gyro() != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_FAIL;

	/* Adjust Fix Value Setting */
	init_adjust(); /* Pan ON */

	return OIS_FW_POLLING_PASS;
}

static void srv_con(unsigned char UcDirSel, unsigned char UcSwcCon)
{
	if (UcSwcCon) {
		if (!UcDirSel)   /* X Direction */
			reg_write_addr(LXEQEN, 0xC5);    /* 0x0084 LXSW = ON */
		else             /* Y Direction */
			reg_write_addr(LYEQEN, 0xC5);    /* 0x008E LYSW = ON */
	} else {
		if (!UcDirSel) { /* X Direction */
			reg_write_addr(LXEQEN, 0x45);    /* 0x0084 LXSW = OFF */
			ram_write_addr(LXDODAT, 0x0000); /* 0x115A */
		} else {         /* Y Direction */
			reg_write_addr(LYEQEN, 0x45);    /* 0x008E LYSW = OFF */
			ram_write_addr(LYDODAT, 0x0000); /* 0x119A */
		}
	}
}

/*===========================================================================
 * Function Name: stanby_on
 * Retun Value: NON
 * Argment Value: NON
 * Explanation: Stabilizer For Servo On Function
 * History: First edition 2010.09.15 Y.Shigeoka
 *==========================================================================*/
static void stanby_on(void)
{
	unsigned char UcRegValx, UcRegValy; /* Registor value */

	reg_read_addr(LXEQEN, &UcRegValx); /* 0x0084 */
	reg_read_addr(LYEQEN, &UcRegValy); /* 0x008E */
	if (((UcRegValx & 0x80) != 0x80) && ((UcRegValy & 0x80) != 0x80)) {
		reg_write_addr(SSSEN, 0x88); /* 0x0097 Smooth Start enable */

		srv_con(X_DIR, ON);
		srv_con(Y_DIR, ON);
	} else {
		srv_con(X_DIR, ON);
		srv_con(Y_DIR, ON);
	}
}

/*==============================================================================*/
/* OisIni.c Code END (2012.11.20) */
/*==============================================================================*/

static unsigned char return_center(unsigned char UcCmdPar)
{
	unsigned char UcCmdSts;

	UcCmdSts = EXE_END;

	if (!UcCmdPar) {               /* X,Y Centering */
		stanby_on();
	} else if (UcCmdPar == 0x01) { /* X Centering Only */
		srv_con(X_DIR, ON);         /* X only Servo ON */
		srv_con(Y_DIR, OFF);
	} else if (UcCmdPar == 0x02) { /* Y Centering Only */
		srv_con(X_DIR, OFF);        /* Y only Servo ON */
		srv_con(Y_DIR, ON);
	}

	return (UcCmdSts);
}

static void s2_process(unsigned char uc_mode)
{
	ram_write_32addr(gxh1c, 0x3F7FFD00);  /* 0x1814 */
	ram_write_32addr(gyh1c, 0x3F7FFD00);  /* 0x1914 */

	if (uc_mode == 1) {  /* Capture Mode */
		/* HPF Through Setting */
		reg_write_addr(G2NDCEFON0, 0x03);  /* 0x0106 I Filter Control */
	} else {   /*Video Mode*/
		/* HPF Setting */
		reg_write_addr(G2NDCEFON0, 0x00);  /* 0x0106 */
	}
}

static void gyro_control(unsigned char Ucgyro_control)
{
	/* Return HPF Setting */
	reg_write_addr(GSHTON, 0x00);  /* 0x0104 */

	if (Ucgyro_control) {  /* Gyro ON */
		ram_write_addr(lxggf, 0x7fff);         /* 0x1308 */
		ram_write_addr(lyggf, 0x7fff);         /* 0x1348 */
		ram_write_32addr(gxi1b_1, 0xBF800000); /* Down */
		ram_write_32addr(gyi1b_1, 0xBF800000); /* Down */

		ram_write_32addr(gxi1b_1, 0xB3E6A3C6); /* Down */
		ram_write_32addr(gyi1b_1, 0xB3E6A3C6); /* Down */
		reg_write_addr(GPANSTTFRCE, 0x00);     /* 0x010A */
		auto_gain_ctrl_sw(ON);            /* Auto Gain Control Mode ON */
	} else {        /* Gyro OFF */
		/* Gain3 Register */
		auto_gain_ctrl_sw(OFF);           /* Auto Gain Control Mode OFF */
	}

	s2_process(OFF); /*Go to Video Mode*/
}

/* Ois On */
static void ois_enable(void)
{
	gyro_control(ON); /* Servo ON */
}

/* Ois Off */
static void ois_diable(void)
{
	gyro_control(OFF);
	ram_write_addr(lxggf, 0x0000); /* 0x1308 */
	ram_write_addr(lyggf, 0x0000); /* 0x1348 */
}


static void vs_mode_convert(unsigned char UcVstmod)
{
	if (UcVstmod) { /* Cam Mode Convert */
		ram_write_32addr(gxl2a_2, 0x00000000); /* 0x1860 0 */
		ram_write_32addr(gxl2b_2, 0x3E06FD65); /* 0x1861 0.131826 */
		ram_write_32addr(gyl2a_2, 0x00000000); /* 0x1960 0 */
		ram_write_32addr(gyl2b_2, 0x3E06FD65); /* 0x1961 0.131826 */
		ram_write_32addr(gxl2c_2, 0xBB894028); /* 0x1862 -0.004188556 3F7FEC00 */
		ram_write_32addr(gyl2c_2, 0xBB894028); /* 0x1962 -0.004188556 3F7FEC00 */
		reg_write_addr(GPANSTTFRCE, 0x11);     /* 0x010A */
	} else {  /* Still Mode Convert */
		ram_write_32addr(gxl2a_2, 0x00000000); /* 0x1860 0 */
		ram_write_32addr(gxl2b_2, 0x3D829952); /* 0x1861 0.063769 */
		ram_write_32addr(gyl2a_2, 0x00000000); /* 0x1960 0 */
		ram_write_32addr(gyl2b_2, 0x3D829952); /* 0x1961 0.063769 */
		ram_write_32addr(gxl2c_2, 0xBA9CD414); /* 0x1862 -0.001196506 3F7FF800 */
		ram_write_32addr(gyl2c_2, 0xBA9CD414); /* 0x1962 -0.001196506 3F7FF800 */
		reg_write_addr(GPANSTTFRCE, 0x00);     /* 0x010A */
	}
}

#define HALL_ADJ    0
#define LOOPGAIN    1
#define THROUGH     2
#define NOISE       3

#define INITVAL     0x0000

/**********************************************************************************/
/* Function Name: tne_gvc */
/* Retun Value: NON */
/* Argment Value: NON */
/* Explanation: Tunes the Gyro VC offset */
/* History: First edition 2009.07.31 Y.Tashita */
/**********************************************************************************/
static int busy_wait(unsigned short UsTrgAdr, unsigned char UcTrgDat)
{
	unsigned char UcFlgVal = 1;
	unsigned char UcCntPla = 0;

	reg_write_addr(UsTrgAdr, UcTrgDat);
	do {
		reg_read_addr(FLGM, &UcFlgVal);
		UcFlgVal &= 0x40;
		UcCntPla ++;
	} while (UcFlgVal && (UcCntPla < BSYWIT_POLLING_LIMIT_A));
	if (UcCntPla == BSYWIT_POLLING_LIMIT_A)
		return OIS_FW_POLLING_FAIL;

	return OIS_FW_POLLING_PASS;
}

static short generate_measure(unsigned short UsRamAdd, unsigned char UcMesMod)
{
	short SsMesRlt = 0;

	reg_write_addr(MS1INADD, (unsigned char)(UsRamAdd & 0x00ff)); /* 0x00C2 Input Signal Select */

	if (!UcMesMod) {
		reg_write_addr(MSMPLNSH, 0x03); /* 0x00CB */
		reg_write_addr(MSMPLNSL, 0xFF); /* 0x00CA 1024 Times Measure */
		busy_wait(MSMA, 0x01);  /* 0x00C9 Average Measure */
	} else {
		reg_write_addr(MSMPLNSH, 0x00); /* 0x00CB */
		reg_write_addr(MSMPLNSL, 0x03); /* 0x00CA 4 Cycle Measure */
		busy_wait(MSMA, 0x22);  /* 0x00C9 Average Measure */
	}

	reg_write_addr(MSMA, 0x00);  /* 0x00C9 Measure Stop */
	ram_read_addr(MSAV1, (unsigned short *)&SsMesRlt); /* 0x1205 */

	return (SsMesRlt);
}

static void measure_filter(unsigned char UcMesMod)
{
	if (!UcMesMod) { /* Hall Bias&Offset Adjust */
		/* Measure Filter1 Setting */
		ram_write_addr(ms1aa, 0x0285); /* 0x13AF LPF150Hz */
		ram_write_addr(ms1ab, 0x0285); /* 0x13B0 */
		ram_write_addr(ms1ac, 0x7AF5); /* 0x13B1 */
		ram_write_addr(ms1ad, 0x0000); /* 0x13B2 */
		ram_write_addr(ms1ae, 0x0000); /* 0x13B3 */
		ram_write_addr(ms1ba, 0x7FFF); /* 0x13B4 Through */
		ram_write_addr(ms1bb, 0x0000); /* 0x13B5 */
		ram_write_addr(ms1bc, 0x0000); /* 0x13B6 */
		ram_write_addr(ms1bd, 0x0000); /* 0x13B7 */
		ram_write_addr(ms1be, 0x0000); /* 0x13B8 */

		reg_write_addr(MSF1SOF, 0x00); /* 0x00C1 */

		/* Measure Filter2 Setting */
		ram_write_addr(ms2aa, 0x0285); /* 0x13B9 LPF150Hz */
		ram_write_addr(ms2ab, 0x0285); /* 0x13BA */
		ram_write_addr(ms2ac, 0x7AF5); /* 0x13BB */
		ram_write_addr(ms2ad, 0x0000); /* 0x13BC */
		ram_write_addr(ms2ae, 0x0000); /* 0x13BD */
		ram_write_addr(ms2ba, 0x7FFF); /* 0x13BE Through */
		ram_write_addr(ms2bb, 0x0000); /* 0x13BF */
		ram_write_addr(ms2bc, 0x0000); /* 0x13C0 */
		ram_write_addr(ms2bd, 0x0000); /* 0x13C1 */
		ram_write_addr(ms2be, 0x0000); /* 0x13C2 */

		reg_write_addr(MSF2SOF, 0x00); /* 0x00C5 */
	} else if (UcMesMod == LOOPGAIN) { /* Loop Gain Adjust */
		/* Measure Filter1 Setting */
		ram_write_addr(ms1aa, 0x0F21); /* 0x13AF LPF1000Hz */
		ram_write_addr(ms1ab, 0x0F21); /* 0x13B0 */
		ram_write_addr(ms1ac, 0x61BD); /* 0x13B1 */
		ram_write_addr(ms1ad, 0x0000); /* 0x13B2 */
		ram_write_addr(ms1ae, 0x0000); /* 0x13B3 */
		ram_write_addr(ms1ba, 0x7F7D); /* 0x13B4 HPF30Hz */
		ram_write_addr(ms1bb, 0x8083); /* 0x13B5 */
		ram_write_addr(ms1bc, 0x7EF9); /* 0x13B6 */
		ram_write_addr(ms1bd, 0x0000); /* 0x13B7 */
		ram_write_addr(ms1be, 0x0000); /* 0x13B8 */

		reg_write_addr(MSF1SOF, 0x00); /* 0x00C1 */

		/* Measure Filter2 Setting */
		ram_write_addr(ms2aa, 0x0F21); /* 0x13B9 LPF1000Hz */
		ram_write_addr(ms2ab, 0x0F21); /* 0x13BA */
		ram_write_addr(ms2ac, 0x61BD); /* 0x13BB */
		ram_write_addr(ms2ad, 0x0000); /* 0x13BC */
		ram_write_addr(ms2ae, 0x0000); /* 0x13BD */
		ram_write_addr(ms2ba, 0x7F7D); /* 0x13BE HPF30Hz */
		ram_write_addr(ms2bb, 0x8083); /* 0x13BF */
		ram_write_addr(ms2bc, 0x7EF9); /* 0x13C0 */
		ram_write_addr(ms2bd, 0x0000); /* 0x13C1 */
		ram_write_addr(ms2be, 0x0000); /* 0x13C2 */

		reg_write_addr(MSF2SOF, 0x00); /* 0x00C5 */
	} else if (UcMesMod == THROUGH) { /* for Through */
		/* Measure Filter1 Setting */
		ram_write_addr(ms1aa, 0x7FFF); /* 0x13AF Through */
		ram_write_addr(ms1ab, 0x0000); /* 0x13B0 */
		ram_write_addr(ms1ac, 0x0000); /* 0x13B1 */
		ram_write_addr(ms1ad, 0x0000); /* 0x13B2 */
		ram_write_addr(ms1ae, 0x0000); /* 0x13B3 */
		ram_write_addr(ms1ba, 0x7FFF); /* 0x13B4 Through */
		ram_write_addr(ms1bb, 0x0000); /* 0x13B5 */
		ram_write_addr(ms1bc, 0x0000); /* 0x13B6 */
		ram_write_addr(ms1bd, 0x0000); /* 0x13B7 */
		ram_write_addr(ms1be, 0x0000); /* 0x13B8 */

		reg_write_addr(MSF1SOF, 0x00); /* 0x00C1 */

		/* Measure Filter2 Setting */
		ram_write_addr(ms2aa, 0x7FFF); /* 0x13B9 Through */
		ram_write_addr(ms2ab, 0x0000); /* 0x13BA */
		ram_write_addr(ms2ac, 0x0000); /* 0x13BB */
		ram_write_addr(ms2ad, 0x0000); /* 0x13BC */
		ram_write_addr(ms2ae, 0x0000); /* 0x13BD */
		ram_write_addr(ms2ba, 0x7FFF); /* 0x13BE Through */
		ram_write_addr(ms2bb, 0x0000); /* 0x13BF */
		ram_write_addr(ms2bc, 0x0000); /* 0x13C0 */
		ram_write_addr(ms2bd, 0x0000); /* 0x13C1 */
		ram_write_addr(ms2be, 0x0000); /* 0x13C2 */

		reg_write_addr(MSF2SOF, 0x00); /* 0x00C5 */
	} else if (UcMesMod == NOISE) { /* SINE WAVE TEST for NOISE */
		ram_write_addr(ms1aa, 0x04F3); /* 0x13AF LPF150Hz */
		ram_write_addr(ms1ab, 0x04F3); /* 0x13B0 */
		ram_write_addr(ms1ac, 0x761B); /* 0x13B1 */
		ram_write_addr(ms1ad, 0x0000); /* 0x13B2 */
		ram_write_addr(ms1ae, 0x0000); /* 0x13B3 */
		ram_write_addr(ms1ba, 0x04F3); /* 0x13B4 LPF150Hz */
		ram_write_addr(ms1bb, 0x04F3); /* 0x13B5 */
		ram_write_addr(ms1bc, 0x761B); /* 0x13B6 */
		ram_write_addr(ms1bd, 0x0000); /* 0x13B7 */
		ram_write_addr(ms1be, 0x0000); /* 0x13B8 */

		reg_write_addr(MSF1SOF, 0x00); /* 0x00C1 */

		ram_write_addr(ms2aa, 0x04F3); /* 0x13B9 LPF150Hz */
		ram_write_addr(ms2ab, 0x04F3); /* 0x13BA */
		ram_write_addr(ms2ac, 0x761B); /* 0x13BB */
		ram_write_addr(ms2ad, 0x0000); /* 0x13BC */
		ram_write_addr(ms2ae, 0x0000); /* 0x13BD */
		ram_write_addr(ms2ba, 0x04F3); /* 0x13BE LPF150Hz */
		ram_write_addr(ms2bb, 0x04F3); /* 0x13BF */
		ram_write_addr(ms2bc, 0x761B); /* 0x13C0 */
		ram_write_addr(ms2bd, 0x0000); /* 0x13C1 */
		ram_write_addr(ms2be, 0x0000); /* 0x13C2 */

		reg_write_addr(MSF2SOF, 0x00); /* 0x00C5 */
	}
}

static int16_t lgit_convert_float32(uint32_t float32, uint8_t len)
{
	uint8_t sgn = float32 >> 31;
	uint16_t exp = 0xFF & (float32 >> 23);
	uint32_t frc = (0x7FFFFF & (float32)) | 0x800000;

	if (exp > 127)
		frc = frc << (exp - 127);
	else
		frc = frc >> (127 - exp);

	frc = frc >> (24 - len);
	if (frc > 0x007FFF)
		frc = 0x7FFF;
	if (sgn)
		frc = (~frc) + 1;

	return 0xFFFF & frc;
}

static unsigned char tne_gvc(uint8_t flag)
{
	unsigned char UcGvcSts = EXE_END;
	unsigned short UsGxoVal, UsGyoVal;
	int16_t oldx, oldy;
	int16_t newx, newy;
	uint32_t gyrox, gyroy = 0;

	ram_read_32addr(GXADZ, &gyrox);
	ram_read_32addr(GYADZ, &gyroy);
	oldx = abs(lgit_convert_float32(gyrox, 18));
	oldy = abs(lgit_convert_float32(gyroy, 18));
	CDBG("%s old gxadz %x, %x \n", __func__, oldx, oldy);

	if (oldx < (262 * 2) && oldy < (262 * 2)) {
		CDBG("%s no need to adjust \n", __func__);
		return UcGvcSts;
	}

	/* for InvenSense Digital Gyro ex.IDG-2000 */
	/* A/D Offset Clear */
	ram_write_addr(ADGXOFF, 0x0000); /* 0x1108 */
	ram_write_addr(ADGYOFF, 0x0000); /* 0x110B */
	reg_write_addr(IZAH, (unsigned char)(INITVAL >> 8)); /* 0x03A0 Set Offset High byte */
	reg_write_addr(IZAL, (unsigned char)INITVAL);        /* 0x03A1 Set Offset Low byte */
	reg_write_addr(IZBH, (unsigned char)(INITVAL >> 8)); /* 0x03A2 Set Offset High byte */
	reg_write_addr(IZBL, (unsigned char)INITVAL);        /* 0x03A3 Set Offset Low byte */

	reg_write_addr(GDLYMON10, 0xF5); /* 0x0184 <- GXADZ(0x19F5) */
	reg_write_addr(GDLYMON11, 0x01); /* 0x0185 <- GXADZ(0x19F5) */
	reg_write_addr(GDLYMON20, 0xF5); /* 0x0186 <- GYADZ(0x18F5) */
	reg_write_addr(GDLYMON21, 0x00); /* 0x0187 <- GYADZ(0x18F5) */
	measure_filter(THROUGH);
	reg_write_addr(MSF1EN, 0x01);    /* 0x00C0 Measure Filter1 Equalizer ON */
	/*******/
	/*  X  */
	/*******/
	if (busy_wait(DLYCLR2, 0x80) != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_FAIL; /* 0x00EF Measure Filter1 Delay RAM Clear */
	UsGxoVal = (unsigned short)generate_measure(GYRMON1, 0);   /* GYRMON1(0x1110) <- GXADZ(0x19F5) */
	reg_write_addr(IZAH, (unsigned char)(UsGxoVal >> 8)); /* 0x03A0 Set Offset High byte */
	reg_write_addr(IZAL, (unsigned char)(UsGxoVal));      /* 0x03A1 Set Offset Low byte */
	/*******/
	/*  Y  */
	/*******/
	if (busy_wait(DLYCLR2, 0x80) != OIS_FW_POLLING_PASS)
		return OIS_FW_POLLING_FAIL; /* 0x00EF Measure Filter1 Delay RAM Clear */
	UsGyoVal = (unsigned short)generate_measure(GYRMON2, 0);   /* GYRMON2(0x1111) <- GYADZ(0x18F5) */

	reg_write_addr(IZBH, (unsigned char)(UsGyoVal >> 8)); /* 0x03A2 Set Offset High byte */
	reg_write_addr(IZBL, (unsigned char)(UsGyoVal));      /* 0x03A3 Set Offset Low byte */

	reg_write_addr(MSF1EN, 0x00); /* 0x00C0 Measure Filter1 Equalizer OFF */

	if (((short)UsGxoVal > (short)GYROFF_HIGH) ||
			((short)UsGxoVal < (short)GYROFF_LOW))
		UcGvcSts = EXE_GYRADJ;

	if (((short)UsGyoVal > (short)GYROFF_HIGH) ||
			((short)UsGyoVal < (short)GYROFF_LOW))
		UcGvcSts = EXE_GYRADJ;

	ram_read_32addr(GXADZ, &gyrox);
	ram_read_32addr(GYADZ, &gyroy);
	newx = abs(lgit_convert_float32(gyrox, 18));
	newy = abs(lgit_convert_float32(gyroy, 18));
	CDBG("%s new gxadz %x, %x \n", __func__, newx, newy);

	if (newx > 262 * 2 || newy > 262 * 2 || newx > oldx || newy > oldy)
		UcGvcSts = EXE_GYRADJ;

	if (UcGvcSts != EXE_GYRADJ) {
		CDBG("%s: gyro original: %x, %x \n", __func__,
			StCalDat.StGvcOff.UsGxoVal, StCalDat.StGvcOff.UsGyoVal);
		CDBG("%s: gyro result: %x, %x \n", __func__, UsGxoVal, UsGyoVal);
		if (flag == OIS_VER_CALIBRATION) {
			ois_i2c_e2p_write(GYRO_AD_OFFSET_X, 0xFFFF & UsGxoVal, 2);
			usleep(10000);
			ois_i2c_e2p_write(GYRO_AD_OFFSET_Y, 0xFFFF & UsGyoVal, 2);
		}
	}

	return (UcGvcSts);
}

static struct msm_ois_fn_t lgit_ois_onsemi_func_tbl;

static int32_t lgit_ois_onsemi_on(enum ois_ver_t ver)
{
	int32_t rc = OIS_SUCCESS;

	switch (ver) {
	case OIS_VER_RELEASE: {
		usleep(20000);
		rc = init_set();
		if (rc < 0)
			return rc;
		usleep(5000);
		return_center(0);
		usleep(100000);
	}
	break;
	case OIS_VER_DEBUG:
	case OIS_VER_CALIBRATION: {
		int i = 0;
		usleep(20000);
		rc = init_set();
		if (rc < 0)
			return rc;

		usleep(5000);
		do {
			if (tne_gvc(ver) == EXE_END)
				break;
		} while (i++ < 5);

		usleep(5000);
		return_center(0);
		ois_diable();
	}
	break;
	}
	lgit_ois_onsemi_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

	if (StCalDat.UsVerDat != 0x0204) {
		CDBG("%s, old module %x \n", __func__, StCalDat.UsVerDat);
		rc = OIS_INIT_OLD_MODULE;
	}

	vs_mode_convert(OFF);
	ois_enable();
	CDBG("%s, exit \n", __func__);

	return rc;
}

static int32_t lgit_ois_onsemi_off(void)
{
	int16_t i;
	int16_t hallx, hally;
	CDBG("%s\n", __func__);

	ois_diable();
	usleep(1000);

	ram_write_addr(LXOLMT, 0x1200);
	usleep(2000);
	ram_read_addr(HXTMP, &hallx);
	ram_write_addr(HXSEPT1, -hallx);

	ram_write_addr(LYOLMT, 0x1200);
	usleep(2000);
	ram_read_addr(HYTMP, &hally);
	ram_write_addr(HYSEPT1, -hally);

	for (i = 0x1100; i >= 0x0000; i -= 0x100) {
		ram_write_addr(LXOLMT, i);
		ram_write_addr(LYOLMT, i);
		usleep(2000);
	}

	srv_con(X_DIR, OFF);
	srv_con(Y_DIR, OFF);
	CDBG("%s, exit\n", __func__);

	return OIS_SUCCESS;
}

static int32_t lgit_ois_onsemi_mode(enum ois_mode_t data)
{
	int cur_mode = lgit_ois_onsemi_func_tbl.ois_cur_mode;

	CDBG("%s:%d\n", __func__, data);

	if (cur_mode == data)
		return OIS_SUCCESS;

	switch (cur_mode) {
	case OIS_MODE_PREVIEW_CAPTURE:
	case OIS_MODE_CAPTURE:
	case OIS_MODE_VIDEO:
		ois_diable();
		break;
	case OIS_MODE_CENTERING_ONLY:
		break;
	case OIS_MODE_CENTERING_OFF:
		return_center(0);
		break;
	}

	switch (data) {
	case OIS_MODE_PREVIEW_CAPTURE:
	case OIS_MODE_CAPTURE:
		vs_mode_convert(OFF);
		ois_enable();
		break;
	case OIS_MODE_VIDEO:
		vs_mode_convert(ON);
		ois_enable();
		break;
	case OIS_MODE_CENTERING_ONLY:
		break;
	case OIS_MODE_CENTERING_OFF:
		srv_con(X_DIR, OFF);
		srv_con(Y_DIR, OFF);
		break;
	}

	lgit_ois_onsemi_func_tbl.ois_cur_mode = data;

	return OIS_SUCCESS;
}

static int16_t lgit_convert_int32(int32_t in)
{
	if (in > 32767)
		return 32767;
	if (in < -32767)
		return -32767;
	return 0xFFFF & in;
}

/*
 * [Integration note]
 * (1) gyro scale match
 * in this driver,
 * output gyro[int16] value is calculated by,
 *
 * gyro[int16] = gyro[float32] * 0x7FFF * 4 * {256 / 262}
 *
 * main purpose is to match gyro[int16] with physical value gyro[dps]
 * gyro[dps] is given by, (if IDF2020 == 2)
 *
 * gyro[dps] = gyro[float32] * 0x7FFF / 65.5
 *
 * then,
 * gyro[int16] = gyro[dps] * 65.5 * 4 * 256 / 262
 *  = gyro[dps] * 256
 *  = {gyro[float32] * 07FFF * 4} * {256/262}
 *
 * (2) hall scale match
 * hall[pixel]: hall[in] = 61.832 pixel: 6700
 *
 * hall[out] = hall[pixel*256] = hall[in]*256*61.872/6700
 *    = hall[in]*256/108
 */

#define GYRO_SCALE_FACTOR 262
#define HALL_SCALE_FACTOR 104

static int32_t lgit_ois_onsemi_stat(struct msm_sensor_ois_info_t *ois_stat)
{
	uint32_t gyro = 0;
	int16_t hall = 0;
	snprintf(ois_stat->ois_provider, ARRAY_SIZE(ois_stat->ois_provider),
		"LGIT_ONSEMI");
	ram_read_32addr(GXADZ, &gyro);
	ois_stat->gyro[0] = lgit_convert_int32((int32_t)(lgit_convert_float32(gyro, 18)) * 256 / GYRO_SCALE_FACTOR);
	CDBG("%s gyrox %x, %x \n", __func__, gyro, ois_stat->gyro[0]);

	ram_read_addr(LXGZF, &hall);
	ois_stat->target[0] = lgit_convert_int32(-1 * (((int32_t)hall) * 256 / HALL_SCALE_FACTOR));
	CDBG("%s targetx %x, %x \n", __func__, gyro, ois_stat->target[0]);

	ram_read_addr(HXIN, &hall);
	ois_stat->hall[0] = lgit_convert_int32(((int32_t)hall) * 256 / HALL_SCALE_FACTOR);
	CDBG("%s hallx %x, %x \n", __func__, hall, ois_stat->hall[0]);

	ram_read_32addr(GYADZ, &gyro);
	ois_stat->gyro[1] = lgit_convert_int32((int32_t)(lgit_convert_float32(gyro, 18)) * 256 / GYRO_SCALE_FACTOR);
	CDBG("%s gyroy %x, %x \n", __func__, gyro, ois_stat->gyro[1]);

	ram_read_addr(LYGZF, &hall);
	ois_stat->target[1] = lgit_convert_int32(-1 * (((int32_t)hall) * 256 / HALL_SCALE_FACTOR));
	CDBG("%s targety %x, %x \n", __func__, gyro, ois_stat->target[1]);

	ram_read_addr(HYIN, &hall);
	ois_stat->hall[1] = lgit_convert_int32(((int32_t)hall) * 256 / HALL_SCALE_FACTOR);
	CDBG("%s hally %x, %x \n", __func__, hall, ois_stat->hall[1]);

	/* if all value = 0, set is_stable = 0 */
	if (ois_stat->hall[0] != 0 || ois_stat->hall[1] != 0 ||
		ois_stat->gyro[0] != 0 || ois_stat->gyro[1] != 0)
		ois_stat->is_stable = 1;
	else
		ois_stat->is_stable = 0;

	return OIS_SUCCESS;
}

#define LENS_MOVE_POLLING_LIMIT  (10)
#define LENS_MOVE_THRESHOLD      (5) /* pixels. */

static int32_t lgit_ois_onsemi_move_lens(int16_t offset_x,
		int16_t offset_y)
{
	int32_t rc = OIS_SUCCESS;
	int16_t oldx, oldy;
	int16_t hallx1, hally1;
	int16_t hallx2, hally2;

	/* to match stats and lens moving, scaling routine routine is applied. */
	/* input offset_x, offset_y --> dimension: hall[pixel*256] */
	/* hall[in] = hall[pixel*256]*108/256 */
	offset_x = offset_x * HALL_SCALE_FACTOR / 256;
	offset_y = offset_y * HALL_SCALE_FACTOR / 256;

	ram_read_addr(HXSEPT1, &oldx);
	ram_read_addr(HYSEPT1, &oldy);

	CDBG("%s offset %x,%x -> %x, %x \n", __func__, oldx, oldy, offset_x, offset_y);

	/* perform move */
	ram_write_addr(HXSEPT1, offset_x & 0xFFFF);
	ram_write_addr(HYSEPT1, offset_y & 0xFFFF);

	/* wait till converged */
	ram_read_addr(HXIN, &hallx1);
	ram_read_addr(HYIN, &hally1);
	CDBG("%s: hall %x, %x \n", __func__, hallx1, hally1);

	usleep(30000);
	ram_read_addr(HXIN, &hallx2);
	ram_read_addr(HYIN, &hally2);
	CDBG("%s: hall %x, %x \n", __func__, hallx2, hally2);

	rc = OIS_FAIL;
	if (abs(hallx2) < LENS_MOVE_THRESHOLD * HALL_SCALE_FACTOR ||
		abs(hally2) < LENS_MOVE_THRESHOLD * HALL_SCALE_FACTOR) {
		rc = OIS_SUCCESS;
	} else if (((abs(hallx1 - hallx2) < LENS_MOVE_THRESHOLD * HALL_SCALE_FACTOR) && (oldx != offset_x)) ||
		((abs(hally1 - hally2) < LENS_MOVE_THRESHOLD * HALL_SCALE_FACTOR) && (oldy != offset_y))) {
		rc = OIS_FAIL;
		CDBG("%s: fail \n", __func__);
	} else {
		rc = OIS_SUCCESS;
	}

	return rc;
}

void lgit_ois_onsemi_init(struct msm_ois_ctrl_t *msm_ois_t)
{
	lgit_ois_onsemi_func_tbl.ois_on = lgit_ois_onsemi_on;
	lgit_ois_onsemi_func_tbl.ois_off = lgit_ois_onsemi_off;
	lgit_ois_onsemi_func_tbl.ois_mode = lgit_ois_onsemi_mode;
	lgit_ois_onsemi_func_tbl.ois_stat = lgit_ois_onsemi_stat;
	lgit_ois_onsemi_func_tbl.ois_move_lens = lgit_ois_onsemi_move_lens;
	lgit_ois_onsemi_func_tbl.ois_cur_mode = OIS_MODE_CENTERING_ONLY;

	msm_ois_t->sid_ois = 0x48 >> 1;
	msm_ois_t->ois_func_tbl = &lgit_ois_onsemi_func_tbl;
}
