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
#include <linux/persistent_ram.h>
#include <linux/platform_data/mmi-factory.h>

#include <mach/gpiomux.h>
#include <mach/restart.h>

#include "board-8960.h"
#include "board-mmi.h"
#include "devices-mmi.h"
#include "timer.h"

static void __init mmi_msm8960_reserve(void)
{
	msm8960_reserve();

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	persistent_ram_early_init(&mmi_ram_console_pram);
#endif
}

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

#define BOOT_MODE_MAX_LEN 64
static char boot_mode[BOOT_MODE_MAX_LEN + 1];
int __init board_boot_mode_init(char *s)
{
	strlcpy(boot_mode, s, BOOT_MODE_MAX_LEN);
	boot_mode[BOOT_MODE_MAX_LEN] = '\0';
	return 1;
}
__setup("androidboot.mode=", board_boot_mode_init);

static int mmi_boot_mode_is_factory(void)
{
	return !strncmp(boot_mode, "factory", BOOT_MODE_MAX_LEN);
}

#define BATTERY_DATA_MAX_LEN 32
static char battery_data[BATTERY_DATA_MAX_LEN+1];
int __init board_battery_data_init(char *s)
{
	strlcpy(battery_data, s, BATTERY_DATA_MAX_LEN);
	battery_data[BATTERY_DATA_MAX_LEN] = '\0';
	return 1;
}
__setup("battery=", board_battery_data_init);

static int mmi_battery_data_is_meter_locked(void)
{
	return !strncmp(battery_data, "meter_lock", BATTERY_DATA_MAX_LEN);
}

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
	mmi_pm8921_init(oem_ptr->oem_data, pdata);
	mmi_pm8921_keypad_init(pdata);
}

static void __init mmi_clk_init(struct msm8960_oem_init_ptrs *oem_ptr,
				struct clock_init_data *data)
{
	struct clk_lookup *mmi_clks;
	int size;

	mmi_clks = mmi_init_clocks_from_dt(&size);
	if (mmi_clks) {
		data->oem_clk_tbl = mmi_clks;
		data->oem_clk_size = size;
	}
}

static struct platform_device *mmi_devices[] __initdata = {
	&mmi_w1_gpio_device,
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	&mmi_ram_console_device,
#endif
};

static void __init mmi_factory_register(void)
{
	struct device_node *chosen;
	int len;
	bool fk_enable;
	int fk_gpio_ndx = -1;
	struct mmi_factory_gpio_entry *gpios;
	int num_gpios;
	const void *prop;
	int i;

	gpios = ((struct mmi_factory_platform_data *)
		mmi_factory_device.dev.platform_data)->gpios;
	num_gpios = ((struct mmi_factory_platform_data *)
		mmi_factory_device.dev.platform_data)->num_gpios;

	chosen = of_find_node_by_path("/Chosen@0");
	if (!chosen)
		goto register_device;

	for (i = 0; i < num_gpios; i++) {
		if (!strcmp("factory_kill", gpios[i].name))
			fk_gpio_ndx = i;
	}

	if (fk_gpio_ndx >= 0) {
		prop = of_get_property(chosen,
				"factory_kill_disable", &len);
		if (prop && (len == sizeof(u8))) {
			fk_enable = (*(u8 *)prop) ? 0 : 1;
			pr_debug("%s: factory_kill_disable = %d\n",
					__func__, fk_enable);
			gpios[fk_gpio_ndx].value = fk_enable;
		}

		prop = of_get_property(chosen,
				"factory_kill_gpio", &len);
		if (prop && (len == sizeof(u32))) {
			gpios[fk_gpio_ndx].number = (*(u32 *)prop);
		}
	}

	of_node_put(chosen);

register_device:
	platform_device_register(&mmi_factory_device);
}

static void __init mmi_device_init(struct msm8960_oem_init_ptrs *oem_ptr)
{
	platform_add_devices(mmi_devices, ARRAY_SIZE(mmi_devices));
	mmi_audio_dsp_init();
	if (mmi_boot_mode_is_factory())
		mmi_factory_register();
}

static void __init mmi_disp_init(struct msm8960_oem_init_ptrs *oem_ptr,
				struct msm_fb_platform_data *msm_fb_pdata,
				struct mipi_dsi_platform_data *mipi_dsi_pdata)
{
	mmi_display_init(msm_fb_pdata, mipi_dsi_pdata);
}

static struct mmi_oem_data mmi_data;

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
	msm8960_oem_funcs.msm_clock_init = mmi_clk_init;
	msm8960_oem_funcs.msm_device_init = mmi_device_init;
	msm8960_oem_funcs.msm_display_init = mmi_disp_init;

	/* Custom OEM Platform Data */
	mmi_data.is_factory = mmi_boot_mode_is_factory;
	mmi_data.is_meter_locked = mmi_battery_data_is_meter_locked;
	mmi_data.mmi_camera = true;
	msm8960_oem_funcs.oem_data = &mmi_data;
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
	.reserve = mmi_msm8960_reserve,
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
	.reserve = mmi_msm8960_reserve,
	.init_irq = msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = msm8960_cdp_init,
	.init_early = mmi_msm8960_init_early,
	.init_very_early = msm8960_early_memory,
	.restart = msm_restart,
MACHINE_END

