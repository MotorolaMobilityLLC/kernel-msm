/*
 * platform_bcm43xx.c: bcm43xx platform data initilization file
 *
 * (C) Copyright 2011 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <asm/intel-mid.h>
#include <linux/wlan_plat.h>
#include <linux/interrupt.h>
#include <linux/mmc/sdhci.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include "pci/platform_sdhci_pci.h"
#include "platform_wifi.h"

static struct resource wifi_res[] = {
	{
	.name = "wlan_irq",
	.start = -1,
	.end = -1,
	.flags = IORESOURCE_IRQ | IRQF_TRIGGER_FALLING ,
	},
};

static struct wifi_platform_data pdata;

static struct platform_device wifi_device = {
	.name = "wlan",
	.dev = {
		.platform_data = &pdata,
		},
	.num_resources = ARRAY_SIZE(wifi_res),
	.resource = wifi_res,
};

static const unsigned int sdhci_quirk = SDHCI_QUIRK2_NON_STD_CIS |
		SDHCI_QUIRK2_ENABLE_MMC_PM_IGNORE_PM_NOTIFY;

static void __init wifi_platform_data_init_sfi_fastirq(struct sfi_device_table_entry *pentry,
						       bool should_register)
{
	int wifi_irq_gpio = -1;

	/* If the GPIO mode was previously called, this code overloads
	   the IRQ anyway */
	wifi_res[0].start = wifi_res[0].end = pentry->irq;
	wifi_res[0].flags = IORESOURCE_IRQ | IRQF_TRIGGER_HIGH;

	pr_info("wifi_platform_data: IRQ == %d\n", pentry->irq);

	if (should_register && platform_device_register(&wifi_device) < 0)
		pr_err("platform_device_register failed for wifi_device\n");
}

/* Called if SFI device WLAN is present */
void __init wifi_platform_data_fastirq(struct sfi_device_table_entry *pe,
				       struct devs_id *dev)
{
	/* This is used in the driver to know if it is GPIO/FastIRQ */
	pdata.use_fast_irq = true;

	if (wifi_res[0].start == -1) {
		pr_info("Using WiFi platform data (Fast IRQ)\n");

		/* Set vendor specific SDIO quirks */
		sdhci_pdata_set_quirks(sdhci_quirk);
		wifi_platform_data_init_sfi_fastirq(pe, true);
	} else {
		pr_info("Using WiFi platform data (Fast IRQ, overloading GPIO mode set previously)\n");
		/* We do not register platform device, as it's already been
		   done by wifi_platform_data */
		wifi_platform_data_init_sfi_fastirq(pe, false);
	}

}

/* GPIO legacy code path */
static void __init wifi_platform_data_init_sfi_gpio(void)
{
	int wifi_irq_gpio = -1;

	/*Get GPIO numbers from the SFI table*/
	wifi_irq_gpio = get_gpio_by_name(WIFI_SFI_GPIO_IRQ_NAME);
	if (wifi_irq_gpio < 0) {
		pr_err("%s: Unable to find " WIFI_SFI_GPIO_IRQ_NAME
		       " WLAN-interrupt GPIO in the SFI table\n",
		       __func__);
		return;
	}

	wifi_res[0].start = wifi_res[0].end = wifi_irq_gpio;
	pr_info("wifi_platform_data: GPIO == %d\n", wifi_irq_gpio);

	if (platform_device_register(&wifi_device) < 0)
		pr_err("platform_device_register failed for wifi_device\n");
}

/* Called from board.c */
void __init *wifi_platform_data(void *info)
{
	/* When fast IRQ platform data has been called first, don't pursue */
	if (wifi_res[0].start != -1)
		return NULL;

	pr_info("Using generic wifi platform data\n");

	/* Set vendor specific SDIO quirks */
#ifdef CONFIG_MMC_SDHCI_PCI
	sdhci_pdata_set_quirks(sdhci_quirk);
#endif

#ifndef CONFIG_ACPI
	/* We are SFI here, register platform device */
	wifi_platform_data_init_sfi_gpio();
#endif

	return &wifi_device;
}
