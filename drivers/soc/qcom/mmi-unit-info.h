/*
 * Copyright (C) 2013 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __ARCH_ARM_MACH_MSM_MMI_UNIT_INFO_H
#define __ARCH_ARM_MACH_MSM_MMI_UNIT_INFO_H

/* set of data provided to the modem over SMEM */
#define MMI_UNIT_INFO_VER 3
#define BARCODE_MAX_LEN 65
#define MACHINE_MAX_LEN 33
#define CARRIER_MAX_LEN 65
#define BASEBAND_MAX_LEN 97
#define MSMHW_MAX_LEN 32
#define DEVICE_MAX_LEN 33
struct mmi_unit_info {
	uint32_t version;
	uint32_t system_rev;
	uint32_t system_serial_low;
	uint32_t system_serial_high;
	char machine[MACHINE_MAX_LEN];
	char barcode[BARCODE_MAX_LEN];
	char carrier[CARRIER_MAX_LEN];
	char baseband[BASEBAND_MAX_LEN];
	char device[DEVICE_MAX_LEN];
	uint32_t radio;
	uint32_t powerup_reason;
};

/* Function that sets the modem reset value in the SMEM location
 * where mmi_unit_info is stored.
 */
void mmi_set_pureason(uint32_t val);

#endif
