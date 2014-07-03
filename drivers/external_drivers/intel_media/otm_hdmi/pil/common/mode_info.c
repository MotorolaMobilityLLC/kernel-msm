/*
 This file is provided under a dual BSD/GPLv2 license.  When using or
 redistributing this file, you may do so under either license.

 GPL LICENSE SUMMARY

 Copyright(c) 2006-2011 Intel Corporation. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of version 2 of the GNU General Public License as
 published by the Free Software Foundation.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 The full GNU General Public License is included in this distribution
 in the file called LICENSE.GPL.

 Contact Information:
      Intel Corporation
      2200 Mission College Blvd.
      Santa Clara, CA  97052

 BSD LICENSE

 Copyright(c) 2006-2011 Intel Corporation. All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

   - Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in
     the documentation and/or other materials provided with the
     distribution.
   - Neither the name of Intel Corporation nor the names of its
     contributors may be used to endorse or promote products derived
     from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "hdmi_internal.h"
#include "otm_hdmi_defs.h"
#include "hdmi_timings.h"
/*-----------------------------------------------------------------------------
			480P TIMINGS
-----------------------------------------------------------------------------*/
#define TIMING_640x480p5994_60 \
		640,		/* width	*/ \
		480,		/* height	*/ \
		OTM_HDMI_REFRESH_60, /* refresh rate */ \
		25200,		/* clock	*/ \
		800,		/* htotal	*/ \
		640,		/* hblank start */ \
		800,		/* hblank end	*/ \
		656,		/* hsync start	*/ \
		752,		/* hsync end	*/ \
		525,		/* vtotal	*/ \
		480,		/* vblank start */ \
		525,		/* vblank end	*/ \
		490,		/* vsync start	*/ \
		492		/* vsync end	*/


#define TIMING_720x480p5994_60 \
		720,		/* width	*/ \
		480,		/* height	*/ \
		OTM_HDMI_REFRESH_60, /* refresh rate */ \
		27027,		/* clock	*/ \
		858,		/* htotal	*/ \
		720,		/* hblank start */ \
		858,		/* hblank end	*/ \
		736,		/* hsync start	*/ \
		798,		/* hsync end	*/ \
		525,		/* vtotal	*/ \
		480,		/* vblank start */ \
		525,		/* vblank end	*/ \
		489,		/* vsync start	*/ \
		495		/* vsync end	*/

/*-----------------------------------------------------------------------------
			576P TIMINGS
-----------------------------------------------------------------------------*/
#define TIMING_720x576p50 \
		720,		/* width	*/ \
		576,		/* height	*/ \
		OTM_HDMI_REFRESH_50, /* refresh rate */ \
		27000,		/* clock	*/ \
		864,		/* htotal	*/ \
		720,		/* hblank start */ \
		864,		/* hblank end	*/ \
		732,		/* hsync start	*/ \
		796,		/* hsync end	*/ \
		625,		/* vtotal	*/ \
		576,		/* vblank start */ \
		625,		/* vblank end	*/ \
		581,		/* vsync start	*/ \
		586		/* vsync end	*/

/*-----------------------------------------------------------------------------
			720I TIMINGS
-----------------------------------------------------------------------------*/
#define TIMING_720_1440x480i5994_60 \
		1440,		/* width	*/ \
		240,		/* height	*/ \
		OTM_HDMI_REFRESH_60, /* refresh rate */ \
		27027,		/* clock	*/ \
		1716,		/* htotal	*/ \
		1440,		/* hblank start */ \
		1716,		/* hblank end	*/ \
		1478,		/* hsync start	*/ \
		1602,		/* hsync end	*/ \
		262,		/* vtotal	*/ \
		240,		/* vblank start */ \
		262,		/* vblank end	*/ \
		244,		/* vsync start	*/ \
		247		/* vsync end	*/

#define TIMING_720_1440x576i50 \
		1440,		/* width	*/ \
		288,		/* height	*/ \
		OTM_HDMI_REFRESH_50, /* refresh rate */ \
		27000,		/* clock	*/ \
		1728,		/* htotal	*/ \
		1440,		/* hblank start */ \
		1728,		/* hblank end	*/ \
		1464,		/* hsync start	*/ \
		1590,		/* hsync end	*/ \
		312,		/* vtotal	*/ \
		288,		/* vblank start */ \
		312,		/* vblank end	*/ \
		290,		/* vsync start	*/ \
		293		/* vsync end	*/

/*-----------------------------------------------------------------------------
			720P TIMINGS
-----------------------------------------------------------------------------*/
#define TIMING_1280x720p50 \
		1280,		/* width	*/ \
		720,		/* height	*/ \
		OTM_HDMI_REFRESH_50, /* refresh rate */ \
		74250,		/* clock	*/ \
		1980,		/* htotal	*/ \
		1280,		/* hblank start */ \
		1980,		/* hblank end	*/ \
		1720,		/* hsync start	*/ \
		1760,		/* hsync end	*/ \
		750,		/* vtotal	*/ \
		720,		/* vblank start */ \
		750,		/* vblank end	*/ \
		725,		/* vsync start	*/ \
		730		/* vsync end	*/

#define TIMING_1280x720p50_FP \
		1280,		/* width	*/ \
		720,		/* height	*/ \
		OTM_HDMI_REFRESH_50, /* refresh rate */ \
		148500,	 /* clock	*/ \
		1980,		/* htotal	*/ \
		1280,		/* hblank start */ \
		1980,		/* hblank end	*/ \
		1720,		/* hsync start	*/ \
		1760,		/* hsync end	*/ \
		750,		/* vtotal	*/ \
		720,		/* vblank start */ \
		750,		/* vblank end	*/ \
		725,		/* vsync start	*/ \
		730		/* vsync end	*/

#define TIMING_1280x720p50_FP2 \
		1280,		/* width	*/ \
		720,		/* height	*/ \
		OTM_HDMI_REFRESH_50, /* refresh rate */ \
		148500,		/* clock	*/ \
		1980,		/* htotal	*/ \
		1280,		/* hblank start */ \
		1980,		/* hblank end	*/ \
		1720,		/* hsync start	*/ \
		1760,		/* hsync end	*/ \
		1500,		/* vtotal	*/ \
		1470,		/* vblank start */ \
		1500,		/* vblank end	*/ \
		1475,		/* vsync start	*/ \
		1480		/* vsync end	*/

#define TIMING_1280x720p50_FSEQ \
		1280,		/* width	*/ \
		720,		/* height	*/ \
		OTM_HDMI_REFRESH_50, /* refresh rate */ \
		148500,		/* clock	*/ \
		1980,		/* htotal	*/ \
		1280,		/* hblank start */ \
		1980,		/* hblank end	*/ \
		1720,		/* hsync start	*/ \
		1760,		/* hsync end	*/ \
		750,		/* vtotal	*/ \
		720,		/* vblank start */ \
		750,		/* vblank end	*/ \
		725,		/* vsync start	*/ \
		730		/* vsync end	*/

#define TIMING_1280x720p5994_60 \
		1280,		/* width	*/ \
		720,		/* height	*/ \
		OTM_HDMI_REFRESH_60, /* refresh rate */ \
		74250,		/* clock	*/ \
		1650,		/* htotal	*/ \
		1280,		/* hblank start */ \
		1650,		/* hblank end	*/ \
		1390,		/* hsync start	*/ \
		1430,		/* hsync end	*/ \
		750,		/* vtotal	*/ \
		720,		/* vblank start */ \
		750,		/* vblank end	*/ \
		725,		/* vsync start	*/ \
		730		/* vsync end	*/

#define TIMING_1280x720p5994_60_FP \
		1280,		/* width	*/ \
		720,		/* height	*/ \
		OTM_HDMI_REFRESH_60, /* refresh rate */ \
		148500,		/* clock	*/ \
		1650,		/* htotal	*/ \
		1280,		/* hblank start */ \
		1650,		/* hblank end	*/ \
		1390,		/* hsync start	*/ \
		1430,		/* hsync end	*/ \
		750,		/* vtotal	*/ \
		720,		/* vblank start */ \
		750,		/* vblank end	*/ \
		725,		/* vsync start	*/ \
		730		/* vsync end	*/

#define TIMING_1280x720p5994_60_FP2 \
		1280,		/* width	*/ \
		720,		/* height	*/ \
		OTM_HDMI_REFRESH_60, /* refresh rate */ \
		148500,		/* clock	*/ \
		1650,		/* htotal	*/ \
		1280,		/* hblank start */ \
		1650,		/* hblank end	*/ \
		1390,		/* hsync start	*/ \
		1430,		/* hsync end	*/ \
		1500,		/* vtotal	*/ \
		1470,		/* vblank start */ \
		1500,		/* vblank end	*/ \
		1475,		/* vsync start	*/ \
		1480		/* vsync end	*/

#define TIMING_1280x720p5994_60_FSEQ \
		1280,		/* width	*/ \
		720,		/* height	*/ \
		OTM_HDMI_REFRESH_60, /* refresh rate */ \
		148500,	 /* clock	*/ \
		1650,		/* htotal	*/ \
		1280,		/* hblank start */ \
		1650,		/* hblank end	*/ \
		1390,		/* hsync start	*/ \
		1430,		/* hsync end	*/ \
		750,		/* vtotal	*/ \
		720,		/* vblank start */ \
		750,		/* vblank end	*/ \
		725,		/* vsync start	*/ \
		730		/* vsync end	*/

/*-----------------------------------------------------------------------------
			1080I TIMINGS
-----------------------------------------------------------------------------*/
#define TIMING_1920x1080i50 \
		1920,		/* width	*/ \
		540,		/* height	*/ \
		OTM_HDMI_REFRESH_50, /* refresh rate */ \
		74250,		/* clock	*/ \
		2640,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2640,		/* hblank end	*/ \
		2448,		/* hsync start	*/ \
		2492,		/* hsync end	*/ \
		562,		/* vtotal	*/ \
		540,		/* vblank start */ \
		562,		/* vblank end	*/ \
		542,		/* vsync start	*/ \
		547		/* vsync end	*/

#define TIMING_1920x1080i50_FP \
		1920,		/* width	*/ \
		540,		/* height	*/ \
		OTM_HDMI_REFRESH_50, /* refresh rate */ \
		148500,		/* clock	*/ \
		2640,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2640,		/* hblank end	*/ \
		2448,		/* hsync start	*/ \
		2492,		/* hsync end	*/ \
		562,		/* vtotal	*/ \
		540,		/* vblank start */ \
		562,		/* vblank end	*/ \
		542,		/* vsync start	*/ \
		547		/* vsync end	*/

#define TIMING_1920x1080i5994_60 \
		1920,		/* width	*/ \
		540,		/* height	*/ \
		OTM_HDMI_REFRESH_60, /* refresh rate */ \
		74250,		/* clock	*/ \
		2200,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2200,		/* hblank end	*/ \
		2008,		/* hsync start	*/ \
		2052,		/* hsync end	*/ \
		562,		/* vtotal	*/ \
		540,		/* vblank start */ \
		562,		/* vblank end	*/ \
		542,		/* vsync start	*/ \
		547		/* vsync end	*/

#define TIMING_1920x1080i5994_60_FP \
		1920,		/* width	*/ \
		540,		/* height	*/ \
		OTM_HDMI_REFRESH_60, /* refresh rate */ \
		148500,		/* clock	*/ \
		2200,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2200,		/* hblank end	*/ \
		2008,		/* hsync start	*/ \
		2052,		/* hsync end	*/ \
		562,		/* vtotal	*/ \
		540,		/* vblank start */ \
		562,		/* vblank end	*/ \
		542,		/* vsync start	*/ \
		547		/* vsync end	*/

/*-----------------------------------------------------------------------------
			1080P TIMINGS
-----------------------------------------------------------------------------*/
#define TIMING_1920x1080p24 \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_24,	/* refresh	*/ \
		74250,		/* clock	*/ \
		2750,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2750,		/* hblank end	*/ \
		2558,		/* hsync start	*/ \
		2602,		/* hsync end	*/ \
		1125,		/* vtotal	*/ \
		1080,		/* vblank start */ \
		1125,		/* vblank end	*/ \
		1084,		/* vsync start	*/ \
		1089		/* vsync end	*/

#define TIMING_1920x1080p24_FP \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_24,	/* refresh	*/ \
		148500,	 /* clock	*/ \
		2750,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2750,		/* hblank end	*/ \
		2558,		/* hsync start	*/ \
		2602,		/* hsync end	*/ \
		1125,		/* vtotal	*/ \
		1080,		/* vblank start */ \
		1125,		/* vblank end	*/ \
		1084,		/* vsync start	*/ \
		1089		/* vsync end	*/

#define TIMING_1920x1080p24_FP2 \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_24,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2750,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2750,		/* hblank end	*/ \
		2558,		/* hsync start	*/ \
		2602,		/* hsync end	*/ \
		2250,		/* vtotal	*/ \
		2205,		/* vblank start */ \
		2250,		/* vblank end	*/ \
		2209,		/* vsync start	*/ \
		2214		/* vsync end	*/

#define TIMING_1920x1080p24_FSEQ \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_24,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2750,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2750,		/* hblank end	*/ \
		2558,		/* hsync start	*/ \
		2602,		/* hsync end	*/ \
		1125,		/* vtotal	*/ \
		1080,		/* vblank start */ \
		1125,		/* vblank end	*/ \
		1084,		/* vsync start	*/ \
		1089		/* vsync end	*/

#define TIMING_1920x1080p25 \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_25,	/* refresh	*/ \
		74250,		/* clock	*/ \
		2640,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2640,		/* hblank end	*/ \
		2448,		/* hsync start	*/ \
		2492,		/* hsync end	*/ \
		1125,		/* vtotal	*/ \
		1080,		/* vblank start */ \
		1125,		/* vblank end	*/ \
		1084,		/* vsync start	*/ \
		1089		/* vsync end	*/

#define TIMING_1920x1080p30 \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_30,	/* refresh	*/ \
		74250,		/* clock	*/ \
		2200,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2200,		/* hblank end	*/ \
		2008,		/* hsync start	*/ \
		2052,		/* hsync end	*/ \
		1125,		/* vtotal	*/ \
		1080,		/* vblank start */ \
		1125,		/* vblank end	*/ \
		1084,		/* vsync start	*/ \
		1089		/* vsync end	*/

#define TIMING_1920x1080p30_FP \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_30,	/* refresh	*/ \
		148500,	 /* clock	*/ \
		2200,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2200,		/* hblank end	*/ \
		2008,		/* hsync start	*/ \
		2052,		/* hsync end	*/ \
		1125,		/* vtotal	*/ \
		1080,		/* vblank start */ \
		1125,		/* vblank end	*/ \
		1084,		/* vsync start	*/ \
		1089		/* vsync end	*/

#define TIMING_1920x1080p30_FP2 \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_30,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2200,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2200,		/* hblank end	*/ \
		2008,		/* hsync start	*/ \
		2052,		/* hsync end	*/ \
		2250,		/* vtotal	*/ \
		2205,		/* vblank start */ \
		2250,		/* vblank end	*/ \
		2209,		/* vsync start	*/ \
		2214		/* vsync end	*/

#define TIMING_1920x1080p48 \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_48,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2750,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2750,		/* hblank end	*/ \
		2558,		/* hsync start	*/ \
		2602,		/* hsync end	*/ \
		1125,		/* vtotal	*/ \
		1080,		/* vblank start */ \
		1125,		/* vblank end	*/ \
		1084,		/* vsync start	*/ \
		1089		/* vsync end	*/

#define TIMING_1920x1080p50 \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_50,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2640,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2640,		/* hblank end	*/ \
		2448,		/* hsync start	*/ \
		2492,		/* hsync end	*/ \
		1125,		/* vtotal	*/ \
		1080,		/* vblank start */ \
		1125,		/* blank end	*/ \
		1084,		/* vsync start	*/ \
		1089		/* vsync end	*/

#define TIMING_1920x1080p5994_60	\
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_60,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2200,		/* htotal	*/ \
		1920,		/* hblank start */ \
		2200,		/* hblank end	*/ \
		2008,		/* hsync start	*/ \
		2052,		/* hsync end	*/ \
		1125,		/* vtotal	*/ \
		1080,		/* vblank start */ \
		1125,		/* blank end	*/ \
		1084,		/* vsync start	*/ \
		1089		/* vsync end	*/

/*-----------------------------------------------------------------------------
			Panel TIMINGS
-----------------------------------------------------------------------------*/

#define TIMING_1920x1080p60_PANEL \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_60,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2184,		/* htotal	*/ \
		1920,		/* hblank start*/ \
		2184,		/* hblank end	*/ \
		1920 + 32,	/* hsync start */ \
		1920 + 32 + 32, /* hsync end	*/ \
		1134,		/* vtotal	*/ \
		1080,		/* vblank start*/ \
		1134,		/* vblank end	*/ \
		1080 + 34,	/* vsync start */ \
		1080 + 34 + 4	/* vsync end	*/

#define TIMING_1920x1080p50_PANEL \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_50,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2184,		/* htotal	*/ \
		1920,		/* hblank start*/ \
		2184,		/* hblank end	*/ \
		1920 + 32,	/* hsync start */ \
		1920 + 32 + 32, /* hsync end	*/ \
		1360,		/* vtotal	*/ \
		1080,		/* vblank start*/ \
		1360,		/* vblank end	*/ \
		1080 + 260,	/* vsync start*/ \
		1080 + 260 + 4	/* vsync end	*/

#define TIMING_1920x1080p48_PANEL \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_48,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2184,		/* htotal	*/ \
		1920,		/* hblank start*/ \
		2184,		/* hblank end	*/ \
		1920 + 32,	/* hsync start */ \
		1920 + 32 + 32, /* hsync end	*/ \
		1417,		/* vtotal	*/ \
		1080,		/* vblank start*/ \
		1417,		/* vblank end	*/ \
		1080 + 317,	/* vsync start */ \
		1080 + 317 + 4	/* vsync end	*/

#define TIMING_1920x1080p30_PANEL_FS \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_30,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2184,		/* htotal	*/ \
		1920,		/* hblank start*/ \
		2184,		/* hblank end	*/ \
		1920 + 32,	/* hsync start */ \
		1920 + 32 + 32, /* hsync end	*/ \
		1134,		/* vtotal	*/ \
		1080,		/* vblank start*/ \
		1134,		/* vblank end	*/ \
		1080 + 34,	/* vsync start */ \
		1080 + 34 + 4	/* vsync end	*/

#define TIMING_1920x1080p25_PANEL_FS \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_25,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2184,		/* htotal	*/ \
		1920,		/* hblank start*/ \
		2184,		/* hblank end	*/ \
		1920 + 32,	/* hsync start */ \
		1920 + 32 + 32, /* hsync end	*/ \
		1360,		/* vtotal	*/ \
		1080,		/* vblank start*/ \
		1360,		/* vblank end	*/ \
		1080 + 260,	/* vsync start*/ \
		1080 + 260 + 4	/* vsync end	*/

#define TIMING_1920x1080p24_PANEL_FS \
		1920,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_24,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2184,		/* htotal	*/ \
		1920,		/* hblank start*/ \
		2184,		/* hblank end	*/ \
		1920 + 32,	/* hsync start */ \
		1920 + 32 + 32, /* hsync end	*/ \
		1417,		/* vtotal	*/ \
		1080,		/* vblank start*/ \
		1417,		/* vblank end	*/ \
		1080 + 317,	/* vsync start */ \
		1080 + 317 + 4	/* vsync end	*/

#define TIMING_960x1080p60_PANEL_FS \
		960,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_60,	/* refresh	*/ \
		148500,		/* clock	*/ \
		1092,		/* htotal	*/ \
		960,		/* hblank start*/ \
		1092,		/* hblank end	*/ \
		960 + 24,	/* hsync start */ \
		960 + 24 + 32,	/* hsync end	*/ \
		1134,		/* vtotal	*/ \
		1080,		/* vblank start*/ \
		1134,		/* vblank end	*/ \
		1080 + 34,	/* vsync start */ \
		1080 + 34 + 4	/* vsync end	*/

#define TIMING_960x1080p50_PANEL_FS \
		960,		/* width	*/ \
		1080,		/* height	*/ \
		OTM_HDMI_REFRESH_50,	/* refresh	*/ \
		148500,		/* clock	*/ \
		1092,		/* htotal	*/ \
		960,		/* hblank start*/ \
		1092,		/* hblank end	*/ \
		960 + 24,	/* hsync start */ \
		960 + 24 + 32,	/* hsync end	*/ \
		1360,		/* vtotal	*/ \
		1080,		/* vblank start*/ \
		1360,		/* vblank end	*/ \
		1080 + 260,	/* vsync start */ \
		1080 + 260 + 4	/* vsync end	*/

#define TIMING_1920x540p60_PANEL_FS \
		1920,		/* width	*/ \
		540,		/* height	*/ \
		OTM_HDMI_REFRESH_60,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2184,		/* htotal	*/ \
		1920,		/* hblank start*/ \
		2184,		/* hblank end	*/ \
		1920 + 32,	/* hsync start */ \
		1920 + 32 + 32, /* hsync end	*/ \
		567,		/* vtotal	*/ \
		540,		/* vblank start*/ \
		567,		/* vblank end	*/ \
		540 + 17,	/* vsync start */ \
		540 + 17 + 4	/* vsync end	*/

#define TIMING_1920x540p50_PANEL_FS \
		1920,		/* width	*/ \
		540,		/* height	*/ \
		OTM_HDMI_REFRESH_50,	/* refresh	*/ \
		148500,		/* clock	*/ \
		2184,		/* htotal	*/ \
		1920,		/* hblank start*/ \
		2184,		/* hblank end	*/ \
		1920 + 32,	/* hsync start */ \
		1920 + 32 + 32, /* hsync end	*/ \
		680,		/* vtotal	*/ \
		540,		/* vblank start*/ \
		680,		/* vblank end	*/ \
		540 + 130,	/* vsync start */ \
		540 + 130 + 4	/* vsync end	*/

#define TIMING_1280x720p60_PANEL_FS \
		1280,		/* width	*/ \
		720,		/* height	*/ \
		OTM_HDMI_REFRESH_60,	/* refresh	*/ \
		148500,		/* clock	*/ \
		1650,		/* htotal	*/ \
		1280,		/* hblank start*/ \
		1650,		/* hblank end	*/ \
		1390,		/* hsync start */ \
		1422,		/* hsync end	*/ \
		750,		/* vtotal	*/ \
		720,		/* vblank start*/ \
		750,		/* vblank end	*/ \
		735,		/* vsync start */ \
		739		/* vsync end	*/

#define TIMING_1280x720p50_PANEL_FS \
		1280,		/* width	*/ \
		720,		/* height	*/ \
		OTM_HDMI_REFRESH_50,	/* refresh	*/ \
		148500,		/* clock	*/ \
		1650,		/* htotal	*/ \
		1280,		/* hblank start*/ \
		1650,		/* hblank end	*/ \
		1280 + 110,	/* hsync start */ \
		1280 + 110 + 32,/* hsync end	*/ \
		900,		/* vtotal	*/ \
		720,		/* vblank start*/ \
		900,		/* vblank end	*/ \
		720 + 165,	/* vsync start */ \
		720 + 165 + 4	/* vsync end	*/


const otm_hdmi_timing_t MODE_640x480p5994_60 = {
	TIMING_640x480p5994_60,
	0,			/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	1			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_720x480p5994_60 = {
	TIMING_720x480p5994_60,
	0,			/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	2			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_720x480p5994_60__16by9 = {
	TIMING_720x480p5994_60,
	PD_AR_16_BY_9,		/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	3			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p5994_60 = {
	TIMING_1280x720p5994_60,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	4			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080i5994_60 = {
	TIMING_1920x1080i5994_60,
	PD_SCAN_INTERLACE | PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	5			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080i5994_60__FP = {
	TIMING_1920x1080i5994_60_FP,
	PD_SCAN_INTERLACE | PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
					/* flags */
	OTM_HDMI_STEREO_FRAME_PACKING,	/* stereo_type */
	5				/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_720_1440x480i5994_60 = {
	TIMING_720_1440x480i5994_60,
	PD_SCAN_INTERLACE,	/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	6			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_720_1440x480i5994_60__16by9 = {
	TIMING_720_1440x480i5994_60,
	PD_SCAN_INTERLACE | PD_AR_16_BY_9,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	7			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p5994_60 = {
	TIMING_1920x1080p5994_60,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	16			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_720x576p50 = {
	TIMING_720x576p50,
	0,			/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	17			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_720x576p50__16by9 = {
	TIMING_720x576p50,
	PD_AR_16_BY_9,		/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	18			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p50 = {
	TIMING_1280x720p50,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	19			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080i50 = {
	TIMING_1920x1080i50,
	PD_SCAN_INTERLACE | PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	20			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080i50__FP = {
	TIMING_1920x1080i50_FP,
	PD_SCAN_INTERLACE | PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
					/* flags */
	OTM_HDMI_STEREO_FRAME_PACKING,	/* stereo_type */
	20				/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_720_1440x576i50 = {
	TIMING_720_1440x576i50,
	PD_SCAN_INTERLACE,	/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	21			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_720_1440x576i50__16by9 = {
	TIMING_720_1440x576i50,
	PD_SCAN_INTERLACE | PD_AR_16_BY_9,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	22			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p50 = {
	TIMING_1920x1080p50,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	31			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p24 = {
	TIMING_1920x1080p24,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	32			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p25 = {
	TIMING_1920x1080p25,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	33			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p30 = {
	TIMING_1920x1080p30,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	34			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p30__FP2 = {
	TIMING_1920x1080p30_FP2,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
					/* flags */
	OTM_HDMI_STEREO_FRAME_PACKING_2,/* stereo_type */
	34				/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p30__FP = {
	TIMING_1920x1080p30_FP,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
					/* flags */
	OTM_HDMI_STEREO_FRAME_PACKING,	/* stereo_type */
	34				/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p30__TBH2 = {
	TIMING_1920x1080p30,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_TOP_BOTTOM_HALF_2,	/* stereo_type */
	34					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p48 = {
	TIMING_1920x1080p48,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	32			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p24__FP2 = {
	TIMING_1920x1080p24_FP2,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
					/* flags */
	OTM_HDMI_STEREO_FRAME_PACKING_2,/* stereo_type */
	32				/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p24__FP = {
	TIMING_1920x1080p24_FP,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
					/* flags */
	OTM_HDMI_STEREO_FRAME_PACKING,	/* stereo_type */
	32				/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p5994_60__FP2 = {
	TIMING_1280x720p5994_60_FP2,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
					/* flags */
	OTM_HDMI_STEREO_FRAME_PACKING_2,/* stereo_type */
	4				/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p5994_60__FP = {
	TIMING_1280x720p5994_60_FP,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
					/* flags */
	OTM_HDMI_STEREO_FRAME_PACKING,	/* stereo_type */
	4				/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p50__FP2 = {
	TIMING_1280x720p50_FP2,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
					/* flags */
	OTM_HDMI_STEREO_FRAME_PACKING_2,/* stereo_type */
	19				/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p50__FP = {
	TIMING_1280x720p50_FP,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
					/* flags */
	OTM_HDMI_STEREO_FRAME_PACKING,	/* stereo_type */
	19				/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p5994_60__SBSH2 = {
	TIMING_1280x720p5994_60,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2,	/* stereo_type */
	4					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p50__SBSH2 = {
	TIMING_1280x720p50,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2,	/* stereo_type */
	19					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080i5994_60__SBSH2 = {
	TIMING_1920x1080i5994_60,
	PD_SCAN_INTERLACE | PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2,	/* stereo_type */
	5					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080i50__SBSH2 = {
	TIMING_1920x1080i50,
	PD_SCAN_INTERLACE | PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2,	/* stereo_type */
	20					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p5994_60__SBSH2 = {
	TIMING_1920x1080p5994_60,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2,	/* stereo_type */
	16					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p50__SBSH2 = {
	TIMING_1920x1080p50,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2,	/* stereo_type */
	31					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p24__SBSH2 = {
	TIMING_1920x1080p24,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_SIDE_BY_SIDE_HALF_2,	/* stereo_type */
	32					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p5994_60__TBH2 = {
	TIMING_1280x720p5994_60,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_TOP_BOTTOM_HALF_2,	/* stereo_type */
	4					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p50__TBH2 = {
	TIMING_1280x720p50,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_TOP_BOTTOM_HALF_2,	/* stereo_type */
	19					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p5994_60__TBH2 = {
	TIMING_1920x1080p5994_60,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_TOP_BOTTOM_HALF_2,	/* stereo_type */
	16					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p50__TBH2 = {
	TIMING_1920x1080p50,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_TOP_BOTTOM_HALF_2,	/* stereo_type */
	31					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p24__TBH2 = {
	TIMING_1920x1080p24,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH,
						/* flags */
	OTM_HDMI_STEREO_TOP_BOTTOM_HALF_2,	/* stereo_type */
	32					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p60__PANEL_FS = {
	TIMING_1280x720p60_PANEL_FS,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
						/* flags */
	OTM_HDMI_STEREO_FRAME_SEQUENTIAL,	/* stereo_type */
	0					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1280x720p50__PANEL_FS = {
	TIMING_1280x720p50_PANEL_FS,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
						/* flags */
	OTM_HDMI_STEREO_FRAME_SEQUENTIAL,	/* stereo_type */
	0					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x540p60__PANEL_FS = {
	TIMING_1920x540p60_PANEL_FS,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
						/* flags */
	OTM_HDMI_STEREO_FRAME_SEQUENTIAL,	/* stereo_type */
	0					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x540p50__PANEL_FS = {
	TIMING_1920x540p50_PANEL_FS,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
						/* flags */
	OTM_HDMI_STEREO_FRAME_SEQUENTIAL,	/* stereo_type */
	0					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_920x1080p60__PANEL_FS = {
	TIMING_960x1080p60_PANEL_FS,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
						/* flags */
	OTM_HDMI_STEREO_FRAME_SEQUENTIAL,	/* stereo_type */
	0					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_920x1080p50__PANEL_FS = {
	TIMING_960x1080p50_PANEL_FS,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
						/* flags */
	OTM_HDMI_STEREO_FRAME_SEQUENTIAL,	/* stereo_type */
	0					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p30__PANEL_FS = {
	TIMING_1920x1080p30_PANEL_FS,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
						/* flags */
	OTM_HDMI_STEREO_FRAME_SEQUENTIAL,	/* stereo_type */
	0					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p25__PANEL_FS = {
	TIMING_1920x1080p25_PANEL_FS,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
						/* flags */
	OTM_HDMI_STEREO_FRAME_SEQUENTIAL,	/* stereo_type */
	0					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p24__PANEL_FS = {
	TIMING_1920x1080p24_PANEL_FS,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
						/* flags */
	OTM_HDMI_STEREO_FRAME_SEQUENTIAL,	/* stereo_type */
	0					/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p60__PANEL = {
	TIMING_1920x1080p60_PANEL,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	0			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p50__PANEL = {
	TIMING_1920x1080p50_PANEL,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	0			/* Metadata VIC */
};

const otm_hdmi_timing_t MODE_1920x1080p48__PANEL = {
	TIMING_1920x1080p48_PANEL,
	PD_AR_16_BY_9 | PD_HSYNC_HIGH | PD_VSYNC_HIGH | PD_DTV_MODE,
				/* flags */
	OTM_HDMI_STEREO_NONE,	/* stereo_type */
	0			/* Metadata VIC */
};
