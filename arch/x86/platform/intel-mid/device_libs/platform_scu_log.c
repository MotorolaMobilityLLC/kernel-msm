/*
 * platform_scu_log.c: Platform data for intel_fw_logging driver.
 *
 * (C) Copyright 2013 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sfi.h>
#include <linux/irq.h>
#include <asm/intel-mid.h>

void __init *scu_log_platform_data(void *info)
{
	struct sfi_device_table_entry *entry = info;
	struct platform_device *pdev;
	int ret;

	pdev = platform_device_alloc(entry->name, -1);
	if (!pdev) {
		pr_err("Out of memory for SFI platform dev %s\n", entry->name);
		goto out;
	}
	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("Failed to add platform device\n");
		platform_device_put(pdev);
		goto out;
	}
	irq_set_status_flags(entry->irq, IRQ_NOAUTOEN);
	install_irq_resource(pdev, entry->irq);
out:
	return NULL;
}
