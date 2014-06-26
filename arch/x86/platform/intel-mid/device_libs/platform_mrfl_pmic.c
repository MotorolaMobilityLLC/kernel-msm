/*
 * platform_mrfl_pmic.c: Platform data for Merrifield PMIC driver
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
#include <asm/intel_scu_pmic.h>

#include "platform_ipc.h"
#include "platform_mrfl_pmic.h"

#define MCHGRIRQ0_ADDR			0x12
#define MCHGRIRQ1_ADDR			0x13

#define PMIC_ID_ADDR    0x00
#define SHADYCOVE_A0	0x00
#define SHADYCOVE_A1	0x01

static struct temp_lookup basincove_adc_tbl[] = {
	{0x24, 125, 0}, {0x28, 120, 0},
	{0x2D, 115, 0}, {0x32, 110, 0},
	{0x38, 105, 0}, {0x40, 100, 0},
	{0x48, 95, 0}, {0x51, 90, 0},
	{0x5C, 85, 0}, {0x68, 80, 0},
	{0x77, 75, 0}, {0x87, 70, 0},
	{0x99, 65, 0}, {0xAE, 60, 0},
	{0xC7, 55, 0}, {0xE2, 50, 0},
	{0x101, 45, 0}, {0x123, 40, 0},
	{0x149, 35, 0}, {0x172, 30, 0},
	{0x19F, 25, 0}, {0x1CE, 20, 0},
	{0x200, 15, 0}, {0x233, 10, 0},
	{0x266, 5, 0}, {0x299, 0, 0},
	{0x2CA, -5, 0}, {0x2F9, -10, 0},
	{0x324, -15, 0}, {0x34B, -20, 0},
	{0x36D, -25, 0}, {0x38A, -30, 0},
	{0x3A4, -35, 0}, {0x3B8, -40, 0},
};

static struct temp_lookup shadycove_adc_tbl[] = {
	{0x35, 125, 0}, {0x3C, 120, 0},
	{0x43, 115, 0}, {0x4C, 110, 0},
	{0x56, 105, 0}, {0x61, 100, 0},
	{0x6F, 95, 0}, {0x7F, 90, 0},
	{0x91, 85, 0}, {0xA7, 80, 0},
	{0xC0, 75, 0}, {0xDF, 70, 0},
	{0x103, 65, 0}, {0x12D, 60, 0},
	{0x161, 55, 0}, {0x1A0, 50, 0},
	{0x1EC, 45, 0}, {0x247, 40, 0},
	{0x2B7, 35, 0}, {0x33F, 30, 0},
	{0x3E8, 25, 0}, {0x4B8, 20, 0},
	{0x5BB, 15, 0}, {0x700, 10, 0},
	{0x89A, 5, 0}, {0xAA2, 0, 0},
	{0xD3D, -5, 0}, {0x109B, -10, 0},
	{0x14F5, -15, 0}, {0x1AA7, -20, 0},
	{0x2234, -25, 0}, {0x2C47, -30, 0},
	{0x39E4, -35, 0}, {0x4C6D, -40, 0},
};

void __init *mrfl_pmic_ccsm_platform_data(void *info)
{
	struct sfi_device_table_entry *entry = info;
	static struct pmic_platform_data pmic_pdata;
	struct platform_device *pdev = NULL;
	int ret;

	pdev = platform_device_alloc(entry->name, -1);
	if (!pdev) {
		pr_err("Out of memory for SFI platform dev %s\n", entry->name);
		goto out;
	}
	pdev->dev.platform_data = &pmic_pdata;
	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("Failed to add adc platform device\n");
		platform_device_put(pdev);
		goto out;
	}
	install_irq_resource(pdev, entry->irq);

	if (INTEL_MID_BOARD(1, PHONE, MRFL) ||
			INTEL_MID_BOARD(1, TABLET, MRFL)) {
		pmic_pdata.max_tbl_row_cnt = ARRAY_SIZE(basincove_adc_tbl);
		pmic_pdata.adc_tbl = basincove_adc_tbl;
	} else if (INTEL_MID_BOARD(1, PHONE, MOFD) ||
			INTEL_MID_BOARD(1, TABLET, MOFD)) {
		pmic_pdata.max_tbl_row_cnt = ARRAY_SIZE(shadycove_adc_tbl);
		pmic_pdata.adc_tbl = shadycove_adc_tbl;
	}

#ifdef CONFIG_BQ24261_CHARGER
	pmic_pdata.cc_to_reg = bq24261_cc_to_reg;
	pmic_pdata.cv_to_reg = bq24261_cv_to_reg;
	pmic_pdata.inlmt_to_reg = bq24261_inlmt_to_reg;
#endif
	register_rpmsg_service("rpmsg_pmic_ccsm", RPROC_SCU,
				RP_PMIC_CCSM);
out:
	return &pmic_pdata;
}

/* WA for ShadyCove PMIC issue to reset MCHGRIRQ0/1 to default values
 * as soon as the IPC driver is loaded.
 * Issue is supposed to be fixed with A2-PMIC
 */
void __init pmic_reset_value_wa(void)
{
	u8 id_val;
	int ret;

	if (INTEL_MID_BOARD(1, PHONE, MOFD) ||
			INTEL_MID_BOARD(1, TABLET, MOFD)) {
		ret = intel_scu_ipc_ioread8(PMIC_ID_ADDR, &id_val);
		if (ret) {
			pr_err("%s:%d Error(%d) reading PMIC ID register\n",
					__func__, __LINE__, ret);
			return;
		}

		pr_info("%s:%d ShadyCove ID_REG-val:%x\n",
				__func__, __LINE__, id_val);
		if ((id_val == SHADYCOVE_A0) || (id_val == SHADYCOVE_A1)) {
			pr_info("%s:%d Reset MCHGRIRQs\n", __func__, __LINE__);
			intel_scu_ipc_iowrite8(MCHGRIRQ0_ADDR, 0xFF);
			intel_scu_ipc_iowrite8(MCHGRIRQ1_ADDR, 0x1F);
		}
	}
}
rootfs_initcall(pmic_reset_value_wa);
