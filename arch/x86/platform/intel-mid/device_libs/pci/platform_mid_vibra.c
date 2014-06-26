/*
 * platform_mid_vibra.c: Vibra driver platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:Hevendra R <hevendrax.raja.reddy@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/pci.h>
#include <linux/gpio.h>
#include <asm/intel-mid.h>
#include <asm/spid.h>
#include <linux/input/intel_mid_vibra.h>

static struct mid_vibra_pdata clv_vibra_pci_data = {
	.time_divisor = 0xFF,
	.base_unit = 0x80,
	.alt_fn = 2,
	.ext_drv = 0,
	.gpio_en = INTEL_VIBRA_ENABLE_GPIO,
	.gpio_pwm = INTEL_PWM_ENABLE_GPIO,
	.name = "drv8601",
	.use_gpio_en = true,
};

static struct mid_vibra_pdata mrfld_vibra_pci_data = {
	.time_divisor = 0x40,
	.base_unit = 0x80,
	.alt_fn = 1,
	.ext_drv = 1,
	.name = "drv2605",
	.use_gpio_en = true,
};

static struct mid_vibra_pdata moor_pr_vibra_pci_data = {
	.time_divisor = 0x40,
	.base_unit = 0x80,
	.alt_fn = 1,
	.ext_drv = 0,
	.name = "drv2603",
	.use_gpio_en = true,
};

static struct mid_vibra_pdata bb_vibra_pci_data = {
	.time_divisor = 0x40,
	.base_unit = 0x80,
	.alt_fn = 1,
	.ext_drv = 0,
	.name = "drv8601",
	.use_gpio_en = true,
};

static struct mid_vibra_pdata *get_vibra_platform_data(struct pci_dev *pdev)
{
	struct mid_vibra_pdata *pdata = NULL;

	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_VIBRA_CLV:
		pdata = &clv_vibra_pci_data;
		break;
	case PCI_DEVICE_ID_INTEL_VIBRA_MRFLD:
		if SPID_PRODUCT(INTEL, MRFL, PHONE, BB)
			pdata = &bb_vibra_pci_data;
		else if SPID_PRODUCT(INTEL, MRFL, PHONE, SB)
			pdata = &mrfld_vibra_pci_data;
		if (pdata != NULL) {
			pdata->gpio_en = get_gpio_by_name("haptics_en");
			pdata->gpio_pwm = get_gpio_by_name("haptics_pwm");
		}
		break;
	case PCI_DEVICE_ID_INTEL_VIBRA_MOOR:
		/* Mountain Praire board uses DRV2605, same as Saltbay */
		if (SPID_HARDWARE_ID(MOFD, PHONE, MP, VVA) ||
				SPID_HARDWARE_ID(MOFD, PHONE, MP, VVB))
			pdata = &mrfld_vibra_pci_data;
		else
			pdata = &moor_pr_vibra_pci_data; /*PR uses DRV2603*/

		pdata->gpio_en = get_gpio_by_name("haptics_en");
		pdata->gpio_pwm = get_gpio_by_name("haptics_pwm");
		break;
	default:
		break;
	}

	return pdata;
}

static void vibra_pci_early_quirks(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = get_vibra_platform_data(pci_dev);
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_VIBRA_CLV,
			vibra_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_VIBRA_MRFLD,
			vibra_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_VIBRA_MOOR,
			vibra_pci_early_quirks);
