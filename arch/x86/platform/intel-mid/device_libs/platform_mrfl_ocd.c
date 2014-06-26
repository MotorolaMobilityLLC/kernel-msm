/*
 * platform_mrfl_ocd.c: Platform data for Merrifield Platform OCD  Driver
 *
 * (C) Copyright 2013 Intel Corporation
 *  Author: Saranya Gopal <saranya.gopal@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/intel-mid.h>
#include <asm/intel_mid_remoteproc.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_basincove_ocd.h>

#include "platform_msic.h"
#include "platform_mrfl_ocd.h"

static int get_bcu_config(struct ocd_bcove_config_data *ocd_smip_data)
{
	int i;
	void __iomem *bcu_smip_sram_addr;
	u8 *plat_smip_data;
	unsigned long sram_addr;

	if (!ocd_smip_data)
		return -ENXIO;

	if (INTEL_MID_BOARD(1, PHONE, MRFL) ||
		INTEL_MID_BOARD(1, TABLET, MRFL)) {
		sram_addr = MRFL_SMIP_SRAM_ADDR;
	} else if (INTEL_MID_BOARD(1, PHONE, MOFD) ||
		INTEL_MID_BOARD(1, TABLET, MOFD)) {
		sram_addr = MOFD_SMIP_SRAM_ADDR;
	} else
		return -EINVAL;

	plat_smip_data = (u8 *)ocd_smip_data;
	bcu_smip_sram_addr = ioremap_nocache(sram_addr +
					BCU_SMIP_OFFSET, NUM_SMIP_BYTES);

	for (i = 0; i < NUM_SMIP_BYTES; i++)
		*(plat_smip_data + i) = ioread8(bcu_smip_sram_addr + i);

	return 0;
}

static struct ocd_platform_data ocd_data;

void __init *mrfl_ocd_platform_data(void *info)
{
	struct sfi_device_table_entry *entry = info;
	struct platform_device *pdev;

	pdev = platform_device_alloc(MRFL_OCD_DEV_NAME, -1);
	if (!pdev) {
		pr_err("out of memory for SFI platform dev %s\n",
			MRFL_OCD_DEV_NAME);
		return NULL;
	}

	if (platform_device_add(pdev)) {
		pr_err("failed to add merrifield ocd platform device\n");
		platform_device_put(pdev);
		return NULL;
	}

	install_irq_resource(pdev, entry->irq);
	ocd_data.bcu_config_data = &get_bcu_config;
	pdev->dev.platform_data	= &ocd_data;
	register_rpmsg_service("rpmsg_mrfl_ocd", RPROC_SCU, RP_MRFL_OCD);

	return &ocd_data;
}
