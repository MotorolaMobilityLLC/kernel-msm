/*
 * platform_pca9574.c: pca9574 platform data initilization file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/i2c.h>
#include <linux/kernel.h>
void __init *nxp_pca9574_platform_data(void *info)
{
	pr_info("pca9574\n");
	return NULL;
}
