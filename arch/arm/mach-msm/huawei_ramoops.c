/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Copyright (c) 2013-2015, LGE Inc.
 * Copyright (c) 2015, Huawei Technologies Co., Ltd. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/memblock.h>
#include <asm/setup.h>
#include <asm/system_info.h>
#include <linux/pstore_ram.h>
#include "huawei_ramoops.h"


static struct ramoops_platform_data huawei_ramoops_data = {
	.mem_size     = HUAWEI_PERSISTENT_RAM_SIZE,
	.console_size = HUAWEI_RAM_CONSOLE_SIZE,
};

static struct platform_device huawei_ramoops_dev = {
	.name = "ramoops",
	.dev = {
		.platform_data = &huawei_ramoops_data,
	}
};

static void __init huawei_add_persist_ram_devices(void)
{
	int ret;
	phys_addr_t base;
	phys_addr_t size;

	size = huawei_ramoops_data.mem_size;

	/* find a 1M section from highmem */
	base = memblock_find_in_range(memblock.current_limit,
			MEMBLOCK_ALLOC_ANYWHERE, size, SECTION_SIZE);
	if (!base) {
		/* find a 1M section from lowmem */
		base = memblock_find_in_range(0,
				MEMBLOCK_ALLOC_ACCESSIBLE,
				size, SECTION_SIZE);
		if (!base) {
			pr_err("%s: not enough membank\n", __func__);
			return;
		}
	}

	pr_info("ramoops: reserved 1 MiB at 0x%08x\n", (int)base);

	huawei_ramoops_data.mem_address = base;
	ret = memblock_reserve(huawei_ramoops_data.mem_address,
			huawei_ramoops_data.mem_size);

	if (ret)
		pr_err("%s: failed to initialize persistent ram\n", __func__);
}


void __init huawei_reserve(void)
{
	huawei_add_persist_ram_devices();
}

void __init huawei_add_persistent_device(void)
{
	int ret;

	if (!huawei_ramoops_data.mem_address) {
		pr_err("%s: not allocated memory for ramoops\n", __func__);
		return;
	}

	ret = platform_device_register(&huawei_ramoops_dev);
	if (ret){
		pr_err("unable to register platform device\n");
		return;
	}
}

