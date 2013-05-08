/* Copyright (c) 2012, ASUSTek Inc.
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
#include <linux/memory.h>
#include <linux/persistent_ram.h>

#include <asm/setup.h>
#include <asm/sizes.h>
#include <asm/system_info.h>
#include <asm/memory.h>

#include <mach/board_asustek.h>

#include <ram_console.h>


#ifdef CONFIG_ANDROID_PERSISTENT_RAM
static struct persistent_ram_descriptor pram_descs[] = {
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	{
		.name = "ram_console",
		.size = ASUSTEK_RAM_CONSOLE_SIZE,
	},
#endif
};

static struct persistent_ram asustek_persistent_ram = {
	.size = ASUSTEK_PERSISTENT_RAM_SIZE,
	.num_descs = ARRAY_SIZE(pram_descs),
	.descs = pram_descs,
};

void __init asustek_add_persistent_ram(void)
{
	struct persistent_ram *pram = &asustek_persistent_ram;
	struct membank* bank = &meminfo.bank[0];

	pram->start = bank->start + bank->size - ASUSTEK_PERSISTENT_RAM_SIZE;

	persistent_ram_early_init(pram);
}
#endif

void __init asustek_reserve(void)
{
	asustek_add_persistent_ram();
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static char bootreason[128] = {0,};
int __init asustek_boot_reason(char *s)
{
	int n;

	if (*s == '=')
		s++;
	n = snprintf(bootreason, sizeof(bootreason),
		 "Boot info:\n"
		 "Last boot reason: %s\n", s);
	bootreason[n] = '\0';
	return 1;
}
__setup("bootreason", asustek_boot_reason);

struct ram_console_platform_data ram_console_pdata = {
	.bootinfo = bootreason,
};

static struct platform_device ram_console_device = {
	.name = "ram_console",
	.id = -1,
	.dev = {
		.platform_data = &ram_console_pdata,
	}
};

void __init asustek_add_ramconsole_devices(void)
{
	platform_device_register(&ram_console_device);
}
#endif /* CONFIG_ANDROID_RAM_CONSOLE */

#ifdef CONFIG_ASUSTEK_PCBID
static char serialno[32] = {0,};
int __init asustek_androidboot_serialno(char *s)
{
	int n;

	if (*s == '=')
		s++;
	n = snprintf(serialno, sizeof(serialno), "%s", s);
	serialno[n] = '\0';

	return 1;
}
__setup("androidboot.serialno", asustek_androidboot_serialno);

struct asustek_pcbid_platform_data asustek_pcbid_pdata = {
	.UUID = serialno,
};

static struct resource resources_asustek_pcbid[] = {
	{
		.start	= 57,
		.end	= 57,
		.name	= "PCB_ID0",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 59,
		.end	= 59,
		.name	= "PCB_ID1",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 12,
		.end	= 12,
		.name	= "PCB_ID2",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 1,
		.end	= 1,
		.name	= "PCB_ID3",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 14,
		.end	= 14,
		.name	= "PCB_ID4",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 53,
		.end	= 53,
		.name	= "PCB_ID5",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 51,
		.end	= 51,
		.name	= "PCB_ID6",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 28,
		.end	= 28,
		.name	= "PCB_ID7",
		.flags	= IORESOURCE_IO,
	},
	{
		.start	= 86,
		.end	= 86,
		.name	= "PCB_ID8",
		.flags	= IORESOURCE_IO,
	},
};

static struct platform_device asustek_pcbid_device = {
	.name		= "asustek_pcbid",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(resources_asustek_pcbid),
	.resource = resources_asustek_pcbid,
	.dev = {
		.platform_data = &asustek_pcbid_pdata,
	}
};

void __init asustek_add_pcbid_devices(void)
{
	platform_device_register(&asustek_pcbid_device);
}

#endif
