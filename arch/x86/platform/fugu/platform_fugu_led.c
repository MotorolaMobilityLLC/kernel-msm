/*
 * platform_fugu_led.c: Platform data for fugu LED driver.
 *
 * (C) Copyright 2014 ASUSTek Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/intel-mid.h>
#include <asm/pmic_pdata.h>
#include <asm/intel_mid_remoteproc.h>

static int __init fugu_led_init(void)
{
	register_rpmsg_service("rpmsg_fugu_led", RPROC_SCU,
				RP_PMIC_ACCESS);
	return 0;
}

postcore_initcall(fugu_led_init);
