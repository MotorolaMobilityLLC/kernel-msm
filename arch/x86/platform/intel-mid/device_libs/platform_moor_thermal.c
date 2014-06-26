/*
 * platform_moor_thermal.c: Platform data initilization file for
 *			Intel Moorefield Platform thermal driver
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Sumeet R Pawnikar <sumeet.r.pawnikar@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/mfd/intel_msic.h>
#include <linux/platform_device.h>
#include <asm/intel_mid_thermal.h>
#include <asm/intel-mid.h>
#include <asm/intel_mid_remoteproc.h>
#include "platform_moor_thermal.h"

/*
 * Naming convention:
 * skin0 -> front skin,
 * skin1--> back skin
 */
/* Updated slope and intercept values as per received from
 * Thermal HW Team. Updated the same value for skin0.
 * Updated the best match slope and intercept values
 * for skin1 same as merrifield platform. */
static struct intel_mid_thermal_sensor moor_sensors[] = {
	{
		.name = "SYSTHERM0",
		.index = 0,
		.direct = false,
	},
	{
		.name = "SYSTHERM1",
		.index = 1,
		.direct = false,
	},
	{
		.name = "SYSTHERM2",
		.index = 2,
		.direct = false,
	},
	{
		.name = "PMIC_DIE",
		.index = 3,
		.direct = true,
	},
	/* Virtual Sensors should always be at the end */
	{
		.name = "FrontSkin",
		.index = 4,
	},
	{
		.name = "BackSkin",
		.index = 5,
	},
};

static struct intel_mid_thermal_platform_data pdata[] = {
	[moor_thermal] = {
		.num_sensors = 4,
		.sensors = moor_sensors,
		.num_virtual_sensors = 2,
	},
};

void __init *moor_thermal_platform_data(void *info)
{
	struct platform_device *pdev;
	struct sfi_device_table_entry *entry = info;

	pdev = platform_device_alloc(MOOR_THERM_DEV_NAME, -1);
	if (!pdev) {
		pr_err("out of memory for SFI platform dev %s\n",
			MOOR_THERM_DEV_NAME);
		return NULL;
	}

	if (platform_device_add(pdev)) {
		pr_err("failed to add thermal platform device\n");
		platform_device_put(pdev);
		return NULL;
	}
	pdev->dev.platform_data = &pdata[moor_thermal];

	install_irq_resource(pdev, entry->irq);

	register_rpmsg_service("rpmsg_moor_thermal", RPROC_SCU,
				RP_SCOVE_THERMAL);

	return 0;
}
