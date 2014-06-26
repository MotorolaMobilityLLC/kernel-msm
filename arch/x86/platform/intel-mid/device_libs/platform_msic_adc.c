/*
 * platform_msic_adc.c: MSIC ADC platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
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
#include <asm/intel_mid_gpadc.h>
#include <asm/intel_mid_remoteproc.h>
#include "platform_msic.h"
#include "platform_msic_adc.h"

void __init *msic_adc_platform_data(void *info)
{
	struct platform_device *pdev = NULL;
	struct sfi_device_table_entry *entry = info;
	static struct intel_mid_gpadc_platform_data msic_adc_pdata;
	int ret = 0;

	pdev = platform_device_alloc(ADC_DEVICE_NAME, -1);

	if (!pdev) {
		pr_err("out of memory for SFI platform dev %s\n",
					ADC_DEVICE_NAME);
		goto out;
	}

	msic_adc_pdata.intr = 0xffff7fc0;

	pdev->dev.platform_data = &msic_adc_pdata;

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add adc platform device\n");
		platform_device_put(pdev);
		goto out;
	}

	install_irq_resource(pdev, entry->irq);

	register_rpmsg_service("rpmsg_msic_adc", RPROC_SCU,
				RP_MSIC_ADC);
out:
	return &msic_adc_pdata;
}
