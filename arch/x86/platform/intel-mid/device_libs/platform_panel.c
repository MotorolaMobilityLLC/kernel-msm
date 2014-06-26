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
