/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>

int androidboot_is_recovery;
int androidboot_mode_charger;

static int __init androidboot_recovery(char *p)
{
	int tmp;
	if (get_option(&p, &tmp) == 0)
		return -EINVAL;
	if (tmp == 1 || tmp == 2)
		androidboot_is_recovery = 1;
	return 0;
}
early_param("androidboot.boot_recovery", androidboot_recovery);

static int __init androidboot_mode(char *p)
{
	if (strncmp(p, "charger", 7) == 0)
		androidboot_mode_charger = 1;
	return 0;
}
early_param("androidboot.mode", androidboot_mode);
