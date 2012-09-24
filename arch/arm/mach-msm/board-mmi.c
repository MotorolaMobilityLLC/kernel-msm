/* Copyright (c) 2011-2012, Motorola Mobility. All rights reserved.
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

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>

#include <linux/bootmem.h>
#include <linux/of_fdt.h>
#include <linux/of.h>

#include <mach/gpiomux.h>
#include <mach/restart.h>

#include "board-8960.h"
#include "board-mmi.h"
#include "timer.h"

static u32 fdt_start_address; /* flattened device tree address */
static u32 fdt_size;

struct dt_gpiomux {
	u16 gpio;
	u8 setting;
	u8 func;
	u8 pull;
	u8 drv;
	u8 dir;
} __attribute__ ((__packed__));

#define DT_PATH_MUX		"/System@0/IOMUX@0"
#define DT_PROP_MUX_SETTINGS	"settings"

static void __init mmi_gpiomux_init(struct msm8960_oem_init_ptrs *oem_ptr)
{
	struct device_node *node;
	const struct dt_gpiomux *prop;
	int i;
	int size = 0;
	struct gpiomux_setting setting;

	/* Override the default setting by devtree.  Do not manually
	 * install via msm_gpiomux_install hardcoded values.
	 */
	node = of_find_node_by_path(DT_PATH_MUX);
	if (!node) {
		pr_info("%s: no node found: %s\n", __func__, DT_PATH_MUX);
		return;
	}
	prop = (const void *)of_get_property(node, DT_PROP_MUX_SETTINGS, &size);
	if (prop && ((size % sizeof(struct dt_gpiomux)) == 0)) {
		for (i = 0; i < size / sizeof(struct dt_gpiomux); i++) {
			setting.func = prop->func;
			setting.drv = prop->drv;
			setting.pull = prop->pull;
			setting.dir = prop->dir;
			if (msm_gpiomux_write(prop->gpio, prop->setting,
						&setting, NULL))
				pr_err("%s: gpio%d mux setting %d failed\n",
					__func__, prop->gpio, prop->setting);
			prop++;
		}
	}

	of_node_put(node);
}

static void __init mmi_cam_init(struct msm8960_oem_init_ptrs *oem_ptr)
{
	pr_info("%s: camera support disabled\n", __func__);
}

static void __init mmi_gsbi_init(struct msm8960_oem_init_ptrs *oem_ptr)
{
	mmi_init_gsbi_devices_from_dt();
}

static void __init mmi_gpio_mpp_init(struct msm8960_oem_init_ptrs *oem_ptr)
{
	mmi_init_pm8921_gpio_mpp();
}

static void __init mmi_i2c_init(struct msm8960_oem_init_ptrs *oem_ptr)
{
	mmi_register_i2c_devices_from_dt();
}

static void __init mmi_pmic_init(struct msm8960_oem_init_ptrs *oem_ptr,
				 void *pdata)
{
	mmi_pm8921_init(pdata);
	mmi_pm8921_keypad_init(pdata);
}

static void __init mmi_msm8960_init_early(void)
{
	msm8960_allocate_memory_regions();
	if (fdt_start_address) {
		void *mem;
		mem = __alloc_bootmem(fdt_size, __alignof__(int), 0);
		BUG_ON(!mem);
		memcpy(mem, (const void *)fdt_start_address, fdt_size);
		initial_boot_params = (struct boot_param_header *)mem;
		pr_info("Unflattening device tree: 0x%08x\n", (u32)mem);

		unflatten_device_tree();
	}

	/* Initialize OEM initialization overrides */
	msm8960_oem_funcs.msm_gpio_init = mmi_gpiomux_init;
	msm8960_oem_funcs.msm_cam_init = mmi_cam_init;
	msm8960_oem_funcs.msm_gsbi_init = mmi_gsbi_init;
	msm8960_oem_funcs.msm_gpio_mpp_init = mmi_gpio_mpp_init;
	msm8960_oem_funcs.msm_i2c_init = mmi_i2c_init;
	msm8960_oem_funcs.msm_pmic_init = mmi_pmic_init;
}

static int __init parse_tag_flat_dev_tree_address(const struct tag *tag)
{
	struct tag_flat_dev_tree_address *fdt_addr =
		(struct tag_flat_dev_tree_address *)&tag->u.fdt_addr;

	if (fdt_addr->size) {
		fdt_start_address = (u32)phys_to_virt(fdt_addr->address);
		fdt_size = fdt_addr->size;
	}

	pr_info("flat_dev_tree_address=0x%08x, flat_dev_tree_size == 0x%08X\n",
		fdt_addr->address, fdt_addr->size);

	return 0;
}
__tagtable(ATAG_FLAT_DEV_TREE_ADDRESS, parse_tag_flat_dev_tree_address);

MACHINE_START(VANQUISH, "Vanquish")
	.map_io = msm8960_map_io,
	.reserve = msm8960_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_cdp_init,
	.init_early = mmi_msm8960_init_early,
	.init_very_early = msm8960_early_memory,
	.restart = msm_restart,
MACHINE_END

MACHINE_START(MSM8960DT, "msm8960dt")
	.map_io = msm8960_map_io,
	.reserve = msm8960_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_cdp_init,
	.init_early = mmi_msm8960_init_early,
	.init_very_early = msm8960_early_memory,
	.restart = msm_restart,
MACHINE_END

