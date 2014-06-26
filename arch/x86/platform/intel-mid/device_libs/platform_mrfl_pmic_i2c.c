/*
 * platform_mrfl_pmic_i2c.c: Platform data for Merrifield PMIC I2C
 * adapter driver.
 *
 * (C) Copyright 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <linux/init.h>
#include <linux/sfi.h>
#include <asm/intel-mid.h>
#include <asm/pmic_pdata.h>
#include <asm/intel_mid_remoteproc.h>
#include <linux/power/bq24261_charger.h>

#include "platform_ipc.h"
#include "platform_mrfl_pmic_i2c.h"

void __init *mrfl_pmic_i2c_platform_data(void *info)
{
	struct sfi_device_table_entry *entry = info;
	struct platform_device *pdev = NULL;
	int ret;

	pdev = platform_device_alloc(entry->name, -1);
	if (!pdev) {
		pr_err("Out of memory for SFI platform dev %s\n", entry->name);
		goto out;
	}
	pdev->dev.platform_data = NULL;
	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("Failed to add adc platform device\n");
		platform_device_put(pdev);
		goto out;
	}
	install_irq_resource(pdev, entry->irq);
	register_rpmsg_service("rpmsg_i2c_pmic_adap", RPROC_SCU,
				RP_PMIC_I2C);
out:
	return NULL;
}
