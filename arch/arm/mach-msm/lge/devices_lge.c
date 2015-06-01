/*
 * arch/arm/mach-msm/lge/device_lge.c
 *
 * Copyright (C) 2012,2013 LGE Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/string.h>
#include <linux/platform_device.h>
#include <linux/memblock.h>
#include <asm/setup.h>
#include <asm/system_info.h>
#include <mach/board_lge.h>
#ifdef CONFIG_PSTORE_RAM
#include <linux/pstore_ram.h>
#endif
#ifdef CONFIG_LGE_HANDLE_PANIC
#include <mach/lge_handle_panic.h>
#endif

static int uart_console_enabled = 0;

#ifdef CONFIG_PSTORE_RAM
static char bootreason[128] = {0,};

int __init lge_boot_reason(char *s)
{
	snprintf(bootreason, sizeof(bootreason),
			"Boot info:\n"
			"Last boot reason: %s\n", s);
	return 1;
}
__setup("androidboot.bootreason=", lge_boot_reason);

static struct ramoops_platform_data lge_ramoops_data = {
	.mem_size     = LGE_PERSISTENT_RAM_SIZE,
	.console_size = LGE_RAM_CONSOLE_SIZE,
	.bootinfo     = bootreason,
};

static struct platform_device lge_ramoops_dev = {
	.name = "ramoops",
	.dev = {
		.platform_data = &lge_ramoops_data,
	}
};

static void __init lge_add_persist_ram_devices(void)
{
	int ret;
	phys_addr_t base;
	phys_addr_t size;

	size = lge_ramoops_data.mem_size;

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

	lge_ramoops_data.mem_address = base;
	ret = memblock_reserve(lge_ramoops_data.mem_address,
			lge_ramoops_data.mem_size);

	if (ret)
		pr_err("%s: failed to initialize persistent ram\n", __func__);
}


void __init lge_reserve(void)
{
	lge_add_persist_ram_devices();
}

void __init lge_add_persistent_device(void)
{
	int ret;

	if (!lge_ramoops_data.mem_address) {
		pr_err("%s: not allocated memory for ramoops\n", __func__);
		return;
	}

	ret = platform_device_register(&lge_ramoops_dev);
	if (ret){
		pr_err("unable to register platform device\n");
		return;
	}
#ifdef CONFIG_LGE_HANDLE_PANIC
	/* write ram console addr to imem */
	lge_set_ram_console_addr(lge_ramoops_data.mem_address,
			lge_ramoops_data.console_size);
#endif
}
#endif

/* See include/mach/board_lge.h. CAUTION: These strings come from LK. */
static char *rev_str[] = {"unknown", "evb1", "rev_a", "rev_b", "rev_c",
	"rev_d", "rev_10", "rev_11"};

static int __init board_revno_setup(char *rev_info)
{
	int i;

	/* it is defined externally in <asm/system_info.h> */
	system_rev = 0;

	for (i = 0; i < ARRAY_SIZE(rev_str); i++) {
		if (!strcmp(rev_info, rev_str[i])) {
			system_rev = i;
			break;
		}
	}

	pr_info("BOARD: LGE %s\n", rev_str[system_rev]);
	return 1;
}
__setup("lge.rev=", board_revno_setup);

int lge_get_board_revno(void)
{
    return system_rev;
}

int lge_uart_console_enabled(void)
{
	return uart_console_enabled;
}

static int __init uart_console_setup(char *str)
{
	if (str && !strncmp(str, "enable", 6))
		uart_console_enabled = 1;

	pr_info("UART CONSOLE: %s\n",
			uart_console_enabled? "enabled" : "disabled");

	return 1;
}

__setup("uart_console=", uart_console_setup);
