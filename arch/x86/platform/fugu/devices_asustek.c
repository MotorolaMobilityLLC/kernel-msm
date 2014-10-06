/* Copyright (c) 2014, ASUSTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/board_asustek.h>

static struct resource resources_asustek_pcbid[] = {
	{
		.start	= 163,
		.end	= 163,
		.name	= "PCB_ID0",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 97,
		.end	= 97,
		.name	= "PCB_ID1",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 154,
		.end	= 154,
		.name	= "PCB_ID2",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 155,
		.end	= 155,
		.name	= "PCB_ID3",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 156,
		.end	= 156,
		.name	= "PCB_ID4",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 157,
		.end	= 157,
		.name	= "PCB_ID5",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 159,
		.end	= 159,
		.name	= "PCB_ID6",
		.flags	= IORESOURCE_IO,
	},
};

static struct platform_device asustek_pcbid_device = {
	.name		= "asustek_pcbid",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_asustek_pcbid),
	.resource = resources_asustek_pcbid,
};

static int __init asustek_add_pcbid_devices(void)
{
	platform_device_register(&asustek_pcbid_device);
	return 0;
}

rootfs_initcall(asustek_add_pcbid_devices);
