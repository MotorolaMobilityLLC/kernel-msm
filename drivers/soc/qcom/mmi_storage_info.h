/*
 * Copyright (C) 2016 Motorola Mobility LLC
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
#ifndef __ARCH_ARM_MACH_MSM_MMI_EMMC_INFO_H
#define __ARCH_ARM_MACH_MSM_MMI_EMMC_INFO_H

struct mmi_storage_info {
	char type[16];	/* UFS or eMMC */
	char size[16];	/* size in GB */
	char card_manufacturer[32];
	char product_name[32];	/* model ID */
	char firmware_version[32];
};

#endif
