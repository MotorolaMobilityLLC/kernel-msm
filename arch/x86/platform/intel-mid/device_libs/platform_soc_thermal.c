/*
 * platform_soc_thermal.c: Platform data for SoC DTS driver
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Durgadoss R <durgadoss.r@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#define pr_fmt(fmt)  "intel_soc_thermal: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>

#include "platform_soc_thermal.h"

#include <asm/intel-mid.h>
#include <asm/intel_mid_thermal.h>

#define BYT_SOC_THRM_IRQ	86
#define BYT_SOC_THRM		"soc_thrm"

static struct resource res = {
		.flags = IORESOURCE_IRQ,
};

/* Annidale based MOFD platform for Phone FFD */
static struct soc_throttle_data ann_mofd_soc_data[] = {
	{
		.power_limit = 0xbb, /* 6W */
		.floor_freq = 0x00,
	},
	{
		.power_limit = 0x41, /* 2.1W */
		.floor_freq = 0x01,
	},
	{
		.power_limit = 0x1C, /* 0.9W */
		.floor_freq = 0x01,
	},
	{
		.power_limit = 0x1C, /* 0.9W */
		.floor_freq = 0x01,
	},
};

static struct soc_throttle_data tng_soc_data[] = {
	{
		.power_limit = 0x6d, /* 3.5W */
		.floor_freq = 0x00,
	},
	{
		.power_limit = 0x38, /* 1.8W */
		.floor_freq = 0x01,
	},
	{
		.power_limit = 0x1C, /* 0.9W */
		.floor_freq = 0x01,
	},
	{
		.power_limit = 0x1C, /* 0.9W */
		.floor_freq = 0x01,
	},
};

static struct soc_throttle_data vlv2_soc_data[] = {
	{
		.power_limit = 0xDA, /* 7W */
		.floor_freq = 0x00,
	},
	{
		.power_limit = 0x6D, /* 3.5W */
		.floor_freq = 0x01,
	},
	{
		.power_limit = 0x2E, /* 1.5W */
		.floor_freq = 0x01,
	},
	{
		.power_limit = 0x2E, /* 1.5W */
		.floor_freq = 0x01,
	},
};

void soc_thrm_device_handler(struct sfi_device_table_entry *pentry,
				struct devs_id *dev)
{
	int ret;
	struct platform_device *pdev;

	pr_info("IPC bus = %d, name = %16.16s, irq = 0x%2x\n",
		pentry->host_num, pentry->name, pentry->irq);

	res.start = pentry->irq;

	pdev = platform_device_register_simple(pentry->name, -1,
					(const struct resource *)&res, 1);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		pr_err("platform_soc_thermal:pdev_register failed: %d\n", ret);
	}

	if (INTEL_MID_BOARD(1, PHONE, MRFL) ||
			INTEL_MID_BOARD(1, TABLET, MRFL))
		pdev->dev.platform_data = &tng_soc_data;
	else if (INTEL_MID_BOARD(1, PHONE, MOFD) ||
			INTEL_MID_BOARD(1, TABLET, MOFD))
		pdev->dev.platform_data = &ann_mofd_soc_data;
}

static inline int byt_program_ioapic(int irq, int trigger, int polarity)
{
	struct io_apic_irq_attr irq_attr;
	int ioapic;

	ioapic = mp_find_ioapic(irq);
	if (ioapic < 0)
		return -EINVAL;
	irq_attr.ioapic = ioapic;
	irq_attr.ioapic_pin = irq;
	irq_attr.trigger = trigger;
	irq_attr.polarity = polarity;
	return io_apic_set_pci_routing(NULL, irq, &irq_attr);
}

static int __init byt_soc_thermal_init(void)
{
	int ret;
	struct platform_device *pdev;

	res.start = BYT_SOC_THRM_IRQ;

	pdev = platform_device_register_simple(BYT_SOC_THRM, -1,
					(const struct resource *)&res, 1);
	if (IS_ERR(pdev)) {
		ret = PTR_ERR(pdev);
		pr_err("byt_soc_thermal:pdev_register failed: %d\n", ret);
		return ret;
	}

	ret = byt_program_ioapic(BYT_SOC_THRM_IRQ, 0, 1);
	if (ret) {
		pr_err("%s: ioapic programming failed", __func__);
		platform_device_unregister(pdev);
	}

	pdev->dev.platform_data = &vlv2_soc_data;

	return ret;
}

static int __init platform_soc_thermal_init(void)
{
	if (INTEL_MID_BOARD(1, TABLET, BYT))
		return byt_soc_thermal_init();

	return 0;
}
device_initcall(platform_soc_thermal_init);
