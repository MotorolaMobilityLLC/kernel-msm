/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/of.h>
#include <linux/batterydata-lib.h>

#ifdef CONFIG_OF_QPNP
/**
 * of_qpnp_populate_bms_tbls() - Populate the BMS Tables for DTS
 * @batt_data: bms_battery_data Pointer to location for BMS Tables
 * @batt_id: ADC value taken to decipher which battery is attached.
 *
 * This routine scans the BMS Device Tree, allocating memory and
 * filling in the BMS Tables.
 */
int of_qpnp_populate_bms_tbls(struct bms_battery_data **batt_data,
			      int64_t batt_id);
#else
static int of_qpnp_populate_bms_tbls(struct bms_battery_data **batt_data,
				     int64_t batt_id);
{
	return -ENXIO;
}
#endif /* CONFIG_OF_QPNP */
