/*
 * platform_msic_gpio.c: MSIC GPIO platform data initilization file
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
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/mfd/intel_msic.h>
#include <asm/intel-mid.h>
#include <linux/platform_data/intel_mid_remoteproc.h>
#include "platform_msic.h"
#include "platform_msic_gpio.h"

void __init *msic_gpio_platform_data(void *info)
{
	struct platform_device *pdev = NULL;
	struct sfi_device_table_entry *entry = info;
	static struct intel_msic_gpio_pdata msic_gpio_pdata;
	int ret;
	int gpio;
	struct resource res;

	pdev = platform_device_alloc(MSIC_GPIO_DEVICE_NAME, -1);

	if (!pdev) {
		pr_err("out of memory for SFI platform dev %s\n",
					MSIC_GPIO_DEVICE_NAME);
		return NULL;
	}

	gpio = get_gpio_by_name("msic_gpio_base");

	if (gpio < 0)
		return NULL;

	if (INTEL_MID_BOARD(1, PHONE, CLVTP) ||
		INTEL_MID_BOARD(1, TABLET, CLVT)) {
		msic_gpio_pdata.ngpio_lv = 1;
		msic_gpio_pdata.ngpio_hv = 8;
		msic_gpio_pdata.gpio0_lv_ctlo = 0x48;
		msic_gpio_pdata.gpio0_lv_ctli = 0x58;
		msic_gpio_pdata.gpio0_hv_ctlo = 0x6D;
		msic_gpio_pdata.gpio0_hv_ctli = 0x75;
	} else if (INTEL_MID_BOARD(1, PHONE, MRFL) ||
		INTEL_MID_BOARD(1, PHONE, MOFD) ||
		INTEL_MID_BOARD(1, TABLET, MOFD)) {
		/* Basincove PMIC GPIO has total 8 GPIO pins,
		 * GPIO[5:2,0] support 1.8V, GPIO[7:6,1] support 1.8V and 3.3V,
		 * We group GPIO[5:2] to low voltage and GPIO[7:6] to
		 * high voltage. Because the CTL registers are contiguous,
		 * this grouping method doesn't affect the driver usage but
		 * easy for the driver sharing among multiple platforms.
		 */
		msic_gpio_pdata.ngpio_lv = 6;
		msic_gpio_pdata.ngpio_hv = 2;
		msic_gpio_pdata.gpio0_lv_ctlo = 0x7E;
		msic_gpio_pdata.gpio0_lv_ctli = 0x8E;
		msic_gpio_pdata.gpio0_hv_ctlo = 0x84;
		msic_gpio_pdata.gpio0_hv_ctli = 0x94;
	}

	msic_gpio_pdata.can_sleep = 1;
	msic_gpio_pdata.gpio_base = gpio;

	pdev->dev.platform_data = &msic_gpio_pdata;

	ret = platform_device_add(pdev);
	if (ret) {
		pr_err("failed to add msic gpio platform device\n");
		platform_device_put(pdev);
		return NULL;
	}

	res.name = "IRQ",
	res.flags = IORESOURCE_IRQ,
	res.start = entry->irq;
	platform_device_add_resources(pdev, &res, 1);

	register_rpmsg_service("rpmsg_msic_gpio", RPROC_SCU, RP_MSIC_GPIO);

	return &msic_gpio_pdata;
}
