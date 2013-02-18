/*
 * Copyright (C) 2012 Motorola, Inc.
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

#ifndef __MMI_BATTERY_DATA_H__
#define __MMI_BATTERY_DATA_H__

#include <linux/mfd/pm8xxx/pm8921-bms.h>
#include <linux/mfd/pm8xxx/pm8921-charger.h>
#include <linux/power/mmi-battery.h>

enum {
	MMI_BATTERY_DEFAULT = 0,
	MMI_BATTERY_EB20_P1,
	MMI_BATTERY_EB20_PRE,
	MMI_BATTERY_EB41_B,
	MMI_BATTERY_EV30_CID_5858,
	MMI_BATTERY_EV30_CID_4246,
	MMI_BATTERY_EG30_SDI,
	MMI_BATTERY_EB20_SDI,
	MMI_BATTERY_EB40_LG,
	MMI_BATTERY_EG30_LG,
	MMI_BATTERY_EU20_LG,
	MMI_BATTERY_EX34_LG,
	MMI_BATTERY_MOCK_EX34_LG,
	MMI_BATTERY_EU40_LG,
	MMI_BATTERY_NUM,
};

struct mmi_battery_list {
	unsigned int                        num_cells;
	struct mmi_battery_cell             *cell_list[MMI_BATTERY_NUM];
	struct bms_battery_data      *bms_list[MMI_BATTERY_NUM];
	struct pm8921_charger_battery_data  *chrg_list[MMI_BATTERY_NUM];
};

extern struct mmi_battery_list mmi_batts;

#endif

