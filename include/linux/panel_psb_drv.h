/**************************************************************************
 * Copyright (c) 2007-2008, Intel Corporation.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 **************************************************************************/

#ifndef _PANEL_PSB_DRV_H_
#define _PANEL_PSB_DRV_H_

#include <linux/kernel.h>
#include <linux/sfi.h>

extern int PanelID;

enum panel_type {
	TPO_CMD,
	TPO_VID,
	TMD_CMD,
	TMD_VID,
	TMD_6X10_VID,
	PYR_CMD,
	PYR_VID,
	TPO,
	TMD,
	PYR,
	HDMI,
	JDI_7x12_VID,
	JDI_7x12_CMD,
	CMI_7x12_VID,
	CMI_7x12_CMD,
	JDI_10x19_VID,
	JDI_10x19_CMD,
	JDI_25x16_VID,
	JDI_25x16_CMD,
	SHARP_10x19_VID,
	SHARP_10x19_CMD,
	SHARP_10x19_DUAL_CMD,
	SHARP_25x16_VID,
	SHARP_25x16_CMD,
	SDC_16x25_CMD,
	SDC_25x16_CMD,
	GCT_DETECT
};

struct support_panel_list_t {
	enum panel_type panel_id;
	char name[SFI_NAME_LEN];
};
static struct support_panel_list_t
	support_panel_list[] = {
		{CMI_7x12_CMD, "PANEL_CMI_CMD"},
		{JDI_7x12_VID, "PANEL_JDI_VID"},
		{JDI_7x12_CMD, "PANEL_JDI_CMD"},
	/*  above 3 items will be removed
	* after firmware changing
	*/
		{CMI_7x12_CMD, "PNC_CMI_7x12"},
		{JDI_7x12_VID, "PNV_JDI_7x12"},
		{JDI_7x12_CMD, "PNC_JDI_7x12"},
		{SHARP_10x19_CMD, "PNC_SHARP_10x19"},
		{SHARP_10x19_DUAL_CMD, "PNCD_SHARP_10x19"},
		{SHARP_25x16_VID, "PNV_SHARP_25x16"},
		{SHARP_25x16_CMD, "PNC_SHARP_25x16"},
		{JDI_25x16_VID, "PNV_JDI_25x16"},
		{JDI_25x16_CMD, "PNC_JDI_25x16"},
		{SDC_16x25_CMD, "PNC_SDC_16x25"},
		{SDC_25x16_CMD, "PNC_SDC_25x16"}
	};

#define NUM_SUPPORT_PANELS (sizeof( \
		support_panel_list) \
	/ sizeof(struct support_panel_list_t))


#endif
