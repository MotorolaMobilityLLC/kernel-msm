/*
 * platform_msic_ocd.c: MSIC OCD platform data initilization file
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
#include <linux/sfi.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/mfd/intel_msic.h>
#include <asm/intel-mid.h>
#include "platform_msic.h"
#include "platform_msic_ocd.h"

void __init *msic_ocd_platform_data(void *info)
{
	static struct intel_msic_ocd_pdata msic_ocd_pdata;
	int gpio;

	gpio = get_gpio_by_name("ocd_gpio");

	if (gpio < 0)
		return NULL;

	msic_ocd_pdata.gpio = gpio;
	msic_pdata.ocd = &msic_ocd_pdata;

	return msic_generic_platform_data(info, INTEL_MSIC_BLOCK_OCD);
}

