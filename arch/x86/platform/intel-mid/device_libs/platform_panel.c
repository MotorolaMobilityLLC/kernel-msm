/*
 * platform_panel.c: panel platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */


#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/intel-mid.h>
#include <linux/string.h>
#include <linux/sfi.h>
#include <linux/panel_psb_drv.h>

int PanelID = GCT_DETECT;
EXPORT_SYMBOL(PanelID);

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

void panel_handler(struct sfi_device_table_entry *pentry,
				struct devs_id *dev) {
	int i;

	/* JDI_7x12_CMD will be used as default panel */
	PanelID = JDI_7x12_CMD;
	for (i = 0; i < NUM_SUPPORT_PANELS; i++)
		if (strncmp(pentry->name, support_panel_list[i].name,
						SFI_NAME_LEN) == 0) {
			PanelID = support_panel_list[i].panel_id;
			break;
		}
	if (i == NUM_SUPPORT_PANELS)
		pr_err("Could not detected this panel, set to default panel\n");
	pr_info("Panel name = %16.16s PanelID = %d\n", pentry->name, PanelID);
}
