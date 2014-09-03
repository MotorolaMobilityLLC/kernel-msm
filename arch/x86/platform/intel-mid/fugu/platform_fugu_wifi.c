/*
 * platform_fugu_wifi.c: fugu wifi platform data initilization file
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
#include <linux/fs.h>
#include "../device_libs/pci/platform_sdhci_pci.h"
#include "platform_fugu_wifi.h"

static int fugu_wifi_get_mac_addr(unsigned char *buf);

static struct wifi_platform_data fugu_wifi_control = {
	.get_mac_addr	= fugu_wifi_get_mac_addr,
};

static struct resource wifi_res[] = {
	{
	.name = "wlan_irq",
	.start = -1,
	.end = -1,
	.flags = IORESOURCE_IRQ | IRQF_TRIGGER_FALLING ,
	},
};

static struct platform_device wifi_device = {
	.name = "wlan",
	.dev = {
		.platform_data = &fugu_wifi_control,
		},
	.num_resources = ARRAY_SIZE(wifi_res),
	.resource = wifi_res,
};

static const unsigned int sdhci_quirk = SDHCI_QUIRK2_NON_STD_CIS |
		SDHCI_QUIRK2_ENABLE_MMC_PM_IGNORE_PM_NOTIFY;

static void __init wifi_platform_data_init_sfi_fastirq(struct sfi_device_table_entry *pentry,
						       bool should_register)
{
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
	fugu_wifi_control.use_fast_irq = true;

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

#define WIFI_MAC_ADDR_FILE	"/factory/wifi/mac.txt"
#define EMMC_ID			"/proc/emmc0_id_entry"

static int check_mac(char *str)
{
	int i;

	if (strlen(str) != 12) {
		pr_err("%s: bad mac address file len %zu < 12\n",
				__func__, strlen(str));
		return -1;
	}
	for (i = 0; i < strlen(str); i++) {
		if (!strchr("1234567890abcdefABCDEF", str[i])) {
			pr_err("%s: illegal wifi mac\n", __func__);
			return -1;
		}
	}
	return 0;
}

static void string_to_mac(char *str, unsigned char *buf)
{
	char temp[3]="\0";
	int mac[6];
	int i;

	for (i = 0; i < 6; i++) {
		strncpy(temp, str+(i*2), 2);
		sscanf(temp, "%x", &mac[i]);
	}
	pr_info("%s: using wifi mac %02x:%02x:%02x:%02x:%02x:%02x\n",
		__func__, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	buf[0] = (unsigned char) mac[0];
	buf[1] = (unsigned char) mac[1];
	buf[2] = (unsigned char) mac[2];
	buf[3] = (unsigned char) mac[3];
	buf[4] = (unsigned char) mac[4];
	buf[5] = (unsigned char) mac[5];
}

static int fugu_wifi_get_mac_addr(unsigned char *buf)
{
	struct file *fp;
	char str[32];
	char default_mac[12] = "00904C";

	pr_debug("%s\n", __func__);

	/* open wifi mac address file */
	fp = filp_open(WIFI_MAC_ADDR_FILE, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: cannot open %s\n", __func__, WIFI_MAC_ADDR_FILE);
		goto random_mac;
	}

	/* read wifi mac address file */
	memset(str, 0, sizeof(str));
	kernel_read(fp, fp->f_pos, str, 12);

	if (check_mac(str)) {
		filp_close(fp, NULL);
		goto random_mac;
	}
	string_to_mac(str, buf);
	filp_close(fp, NULL);
	return 0;

random_mac:
	/* open wifi mac address file */
	fp = filp_open(EMMC_ID, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: cannot open %s\n", __func__, EMMC_ID);
		return -1;
	}

	/* read wifi mac address file */
	memset(str, 0, sizeof(str));
	kernel_read(fp, fp->f_pos, str, 32);
	strcat(default_mac, str+strlen(str)-6);

	if (check_mac(default_mac)) {
		filp_close(fp, NULL);
		return -1;
	}
	string_to_mac(default_mac, buf);
	filp_close(fp, NULL);
	return 0;
}
