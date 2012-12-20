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
#include <asm/mach/mmc.h>
#include <asm/setup.h>
#include <asm/system_info.h>

#include <linux/bootmem.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/persistent_ram.h>
#include <linux/platform_data/mmi-factory.h>
#include <linux/apanic_mmc.h>

#include <mach/devtree_util.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/mpm.h>
#include <mach/restart.h>
#include <mach/msm_smsm.h>
#include <mach/msm_iomap.h>

#include "board-8960.h"
#include "board-mmi.h"
#include "devices-mmi.h"
#include "timer.h"
#include "msm_watchdog.h"

static void (*msm8960_common_cal_rsv_sizes)(void) __initdata;

static void __init msm8960_mmi_cal_rsv_sizes(void)
{
	if (msm8960_common_cal_rsv_sizes)
		msm8960_common_cal_rsv_sizes();
	reserve_memory_for_watchdog();
}

static void __init mmi_msm8960_reserve(void)
{
	msm8960_common_cal_rsv_sizes = reserve_info->calculate_reserve_sizes;
	reserve_info->calculate_reserve_sizes = msm8960_mmi_cal_rsv_sizes;
	msm8960_reserve();

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	persistent_ram_early_init(&mmi_ram_console_pram);
#endif
}

static u32 fdt_start_address; /* flattened device tree address */
static u32 fdt_size;
static u32 prod_id;

#define EXPECTED_MBM_PROTOCOL_VER 1
static u32 mbmprotocol;

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
	u16 gpio;

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
			gpio = be16_to_cpup((__be16 *)&prop->gpio);
			if (msm_gpiomux_write(gpio, prop->setting,
						&setting, NULL))
				pr_err("%s: gpio%d mux setting %d failed\n",
					__func__, gpio, prop->setting);
			prop++;
		}
	}

	of_node_put(node);
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
	&mmi_pm8xxx_rgb_leds_device,
	&mmi_alsa_to_h2w_hs_device,
	&mmi_bq5101xb_device,
};

static void __init mmi_factory_register(void)
{
	struct device_node *factory_node;
	struct device_node *gpio_node;
	struct mmi_factory_gpio_entry *gpios;
	int num_gpios;
	int ret;

	factory_node = of_find_node_by_path("/System@0/FactoryDevice@0");
	if (!factory_node)
		goto register_device;

	num_gpios = dt_children_count(factory_node);
	if (!num_gpios) {
		pr_warn("%s read empty node\n", factory_node->full_name);
		goto register_device;
	}

	gpios = kzalloc(num_gpios * sizeof(struct mmi_factory_gpio_entry),
		GFP_KERNEL);
	if (!gpios) {
		pr_err("%s allocation failure for factory device "\
			"failed to allocate %d gpios.",
			__func__, num_gpios);
		goto register_device;
	}

	num_gpios = 0;
	for_each_child_of_node(factory_node, gpio_node) {
		const char *gpio_name;
		u32 gpio_number;
		u8 gpio_direction;  /* 0 == IN, 1 == OUT */
		u32 gpio_value = 0; /* value is only used for output */
		u32 val;

		if (of_property_read_string(gpio_node,
				"gpio_name", &gpio_name)) {
			pr_warn("%s missing required property - gpio_name\n",
				gpio_node->full_name);
			continue;
		}

		ret = of_property_read_u32(gpio_node, "gpio_number",
					   &gpio_number);
		if (ret) {
			pr_warn("%s missing required property - gpio_number\n",
				gpio_node->full_name);
			continue;
		}

		if (gpio_number >= NR_GPIO_IRQS) {
			pr_warn("%s gpio_number (%d) is out of range\n",
				gpio_node->full_name, gpio_number);
			continue;
		}

		ret  = of_property_read_u32(gpio_node, "gpio_direction",
					    &val);
		if (ret) {
			pr_warn("%s missing required property - gpio_number\n",
				gpio_node->full_name);
			continue;
		}
		gpio_direction = val ? GPIOF_DIR_OUT : GPIOF_DIR_IN;

		if (gpio_direction == GPIOF_DIR_OUT) {
			ret = of_property_read_u32(gpio_node, "gpio_value",
						   &gpio_value);
			if (ret) {
				pr_warn("%s missing required property "\
				"- gpio_direction\n", gpio_node->full_name);
				continue;
			}
		}

		gpios[num_gpios].name = gpio_name;
		gpios[num_gpios].number = gpio_number;
		gpios[num_gpios].direction = gpio_direction;
		gpios[num_gpios].value = gpio_value;
		num_gpios++;
	}

	if (num_gpios) {
		((struct mmi_factory_platform_data *)
			mmi_factory_device.dev.platform_data)->gpios = gpios;
		((struct mmi_factory_platform_data *)
			mmi_factory_device.dev.platform_data)->num_gpios =
			num_gpios;
	}

register_device:
	of_node_put(factory_node);
	platform_device_register(&mmi_factory_device);
}

#define SERIALNO_MAX_LEN 64
static char serialno[SERIALNO_MAX_LEN + 1];
int __init board_serialno_init(char *s)
{
	strncpy(serialno, s, SERIALNO_MAX_LEN);
	serialno[SERIALNO_MAX_LEN] = '\0';
	return 1;
}
__setup("androidboot.serialno=", board_serialno_init);

static char carrier[CARRIER_MAX_LEN + 1];
int __init board_carrier_init(char *s)
{
	strncpy(carrier, s, CARRIER_MAX_LEN);
	carrier[CARRIER_MAX_LEN] = '\0';
	return 1;
}
__setup("androidboot.carrier=", board_carrier_init);

static char baseband[BASEBAND_MAX_LEN + 1];
int __init board_baseband_init(char *s)
{
	strncpy(baseband, s, BASEBAND_MAX_LEN);
	baseband[BASEBAND_MAX_LEN] = '\0';
	return 1;
}
__setup("androidboot.baseband=", board_baseband_init);

static char extended_baseband[BASEBAND_MAX_LEN+1] = "\0";
static int __init mot_parse_atag_baseband(const struct tag *tag)
{
	const struct tag_baseband *baseband_tag = &tag->u.baseband;
	strncpy(extended_baseband, baseband_tag->baseband, BASEBAND_MAX_LEN);
	extended_baseband[BASEBAND_MAX_LEN] = '\0';
	pr_info("%s: %s\n", __func__, extended_baseband);
	return 0;
}
__tagtable(ATAG_BASEBAND, mot_parse_atag_baseband);

static void __init mmi_unit_info_init(void){
	struct mmi_unit_info *mui;

	#define SMEM_KERNEL_RESERVE_SIZE 1024
	mui = (struct mmi_unit_info *) smem_alloc(SMEM_KERNEL_RESERVE,
		SMEM_KERNEL_RESERVE_SIZE);

	if (!mui) {
		pr_err("%s: failed to allocate mmi_unit_info in SMEM\n",
			__func__);
		return;
	}

	mui->version = MMI_UNIT_INFO_VER;
	mui->prod_id = prod_id;
	mui->system_rev = system_rev;
	mui->system_serial_low = system_serial_low;
	mui->system_serial_high = system_serial_high;
	strncpy(mui->machine, machine_desc->name, MACHINE_MAX_LEN);
	strncpy(mui->barcode, serialno, BARCODE_MAX_LEN);
	strncpy(mui->baseband, extended_baseband, BASEBAND_MAX_LEN);
	strncpy(mui->carrier, carrier, CARRIER_MAX_LEN);

	if (mui->version != MMI_UNIT_INFO_VER) {
		pr_err("%s: unexpected unit_info version %d in SMEM\n",
			__func__, mui->version);
	}

	pr_err("mmi_unit_info (SMEM) for modem: version = 0x%02x,"
		" prod_id = 0x%08x, system_rev = 0x%08x,"
		" system_serial = 0x%08x%08x,"
		" machine = '%s', barcode = '%s',"
		" baseband = '%s', carrier = '%s'\n",
		mui->version,
		mui->prod_id, mui->system_rev,
		mui->system_serial_high, mui->system_serial_low,
		mui->machine, mui->barcode,
		mui->baseband, mui->carrier);
}

static void __init mmi_msm8960_get_acputype(void)
{
/* PTE EFUSE register. */
#define QFPROM_PTE_EFUSE_ADDR	(MSM_QFPROM_BASE + 0x00C0)

	uint32_t pte_efuse, pvs;
	char acpu_type[32];

	pte_efuse = readl_relaxed(QFPROM_PTE_EFUSE_ADDR);
	pvs = (pte_efuse >> 10) & 0x7;
	if (pvs == 0x7)
		pvs = (pte_efuse >> 13) & 0x7;

	switch (pvs) {
	case 0x0:
	case 0x7:
		snprintf(acpu_type, 30, "ACPU PVS: Slow(%1d)\n", pvs);
		break;
	case 0x1:
		snprintf(acpu_type, 30, "ACPU PVS: Nominal\n");
		break;
	case 0x3:
		snprintf(acpu_type, 30, "ACPU PVS: Fast\n");
		break;
	default:
		snprintf(acpu_type, 30, "ACPU PVS: Unknown(%1d)\n", pvs);
		break;
	}

	apanic_mmc_annotate(acpu_type);
	persistent_ram_ext_oldbuf_print(acpu_type);
}

static void __init mmi_device_init(struct msm8960_oem_init_ptrs *oem_ptr)
{
	platform_add_devices(mmi_devices, ARRAY_SIZE(mmi_devices));
	mmi_audio_dsp_init();
	mmi_i2s_dai_init();
	if (mmi_boot_mode_is_factory())
		mmi_factory_register();

	mmi_vibrator_init();
	mmi_unit_info_init();
	mmi_msm8960_get_acputype();

	if (mbmprotocol == 0) {
		/* do not reboot - version was not reported */
		/* not expecting bootloader to recognize reboot flag*/
		pr_err("ERROR: mbm protocol version missing\n");
	} else if (EXPECTED_MBM_PROTOCOL_VER != mbmprotocol) {
		pr_err("ERROR: mbm protocol version mismatch\n");
		msm_restart(0, "mbmprotocol_ver_mismatch");
	}
}

static void __init mmi_disp_init(struct msm8960_oem_init_ptrs *oem_ptr,
				struct msm_fb_platform_data *msm_fb_pdata,
				struct mipi_dsi_platform_data *mipi_dsi_pdata)
{
	mmi_display_init(msm_fb_pdata, mipi_dsi_pdata);
}

/* Motorola ULPI default register settings
 * TXPREEMPAMPTUNE[5:4] = 11 (3x preemphasis current)
 * TXVREFTUNE[3:0] = 1111 increasing the DC level
 */
static int mmi_phy_settings[] = {0x34, 0x82, 0x3f, 0x81, -1};

static void __init mmi_otg_init(struct msm8960_oem_init_ptrs *oem_ptr,
			void *pdata)
{
	struct device_node *chosen;
	int len;
	const void *prop;
	int ret;
	unsigned int val;
	struct msm_otg_platform_data *otg_pdata =
				(struct msm_otg_platform_data *) pdata;

	chosen = of_find_node_by_path("/Chosen@0");

	/*
	 * the phy init sequence read from the device tree should be a
	 * sequence of value/register pairs
	 */
	prop = of_get_property(chosen, "ulpi_phy_init_seq", &len);
	if (prop && !(len % 2)) {
		int i;
		u8 *prop_val;

		otg_pdata->phy_init_seq = kzalloc(sizeof(int)*(len+1),
							GFP_KERNEL);

		if (!otg_pdata->phy_init_seq) {
			otg_pdata->phy_init_seq = mmi_phy_settings;
			goto put_node;
		}

		otg_pdata->phy_init_seq[len] = -1;
		prop_val = (u8 *)prop;

		for (i = 0; i < len; i += 2) {
			otg_pdata->phy_init_seq[i] = prop_val[i];
			otg_pdata->phy_init_seq[i+1] = prop_val[i+1];
		}
	} else
		otg_pdata->phy_init_seq = mmi_phy_settings;

	/* Skip EMU id logic detection for factory mode */
	if (mmi_boot_mode_is_factory())
		goto put_node;
	/*
	 * If the EMU circuitry provides id, then read the id irq and
	 * active logic from the device tree.
	 */
	ret = of_property_read_u32(chosen, "emu_id_mpp_gpio", &val);
	if (!ret) {
		otg_pdata->pmic_id_irq = gpio_to_irq(val);
		pr_debug("%s: PMIC id irq = %d\n", __func__,
				otg_pdata->pmic_id_irq);
	}

	ret = of_property_read_u32(chosen, "emu_id_activehigh", &val);
	if (!ret) {
		pr_debug("%s: PMIC id irq is active %s\n",
				__func__, val ? "high" : "low");
		otg_pdata->pmic_id_irq_active_high = val;
	}

put_node:
	of_node_put(chosen);
	return;
}

static void __init mmi_mmc_init(struct msm8960_oem_init_ptrs *oem_ptr,
				int host, void *pdata)
{
	struct mmc_platform_data *sdcc = (struct mmc_platform_data *)pdata;
	switch (host) {
	case 1:		/* SDCC1 */
		sdcc->pin_data->pad_data->drv->on[0].val = GPIO_CFG_6MA;
		sdcc->pin_data->pad_data->drv->on[1].val = GPIO_CFG_6MA;
		sdcc->pin_data->pad_data->drv->on[2].val = GPIO_CFG_6MA;
		/* Enable UHS rates up to DDR50 */
		/*
		 * FIXME: investigate SDR100 modes on pro HW (no card support
		 * right now).
		 */
		sdcc->uhs_caps = (MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 |
				  MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_DDR50 |
				  MMC_CAP_1_8V_DDR);
		/*
		 * FIXME: Might need devtree here if it turns out that 8960
		 * layout can't support HS200.  No cards currently support
		 * this, but they are coming soon.
		 */
		sdcc->uhs_caps2 = 0;

		/*
		 * Current hardware is rated to a maximum power class of 6.
		 */
		sdcc->msmsdcc_max_pwrclass = 6;
		break;
	case 3:		/* SDCC3 */
		sdcc->pin_data->pad_data->drv->on[0].val = GPIO_CFG_8MA;
		sdcc->pin_data->pad_data->drv->on[1].val = GPIO_CFG_8MA;
		sdcc->pin_data->pad_data->drv->on[2].val = GPIO_CFG_8MA;
		sdcc->status_gpio = PM8921_GPIO_PM_TO_SYS(20);
		sdcc->status_irq = gpio_to_irq(sdcc->status_gpio);
		sdcc->is_status_gpio_active_low = true;
		/* FIXME: need to validate SDR100 on the SD slot. */
		sdcc->uhs_caps = (MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 |
				  MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_DDR50 |
				  MMC_CAP_MAX_CURRENT_600);
		sdcc->uhs_caps2 = 0;
		break;
	}
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
	msm8960_oem_funcs.msm_gsbi_init = mmi_gsbi_init;
	msm8960_oem_funcs.msm_gpio_mpp_init = mmi_gpio_mpp_init;
	msm8960_oem_funcs.msm_i2c_init = mmi_i2c_init;
	msm8960_oem_funcs.msm_pmic_init = mmi_pmic_init;
	msm8960_oem_funcs.msm_clock_init = mmi_clk_init;
	msm8960_oem_funcs.msm_device_init = mmi_device_init;
	msm8960_oem_funcs.msm_display_init = mmi_disp_init;
	msm8960_oem_funcs.msm_regulator_init = mmi_regulator_init;
	msm8960_oem_funcs.msm_otg_init = mmi_otg_init;
	msm8960_oem_funcs.msm_mmc_init = mmi_mmc_init;

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

static const char *mmi_dt_match[] __initdata = {
	"mmi,msm8960",
	NULL
};

static const struct of_device_id mmi_msm8960_dt_gic_match[] __initconst = {
	{ .compatible = "qcom,msm-qgic2", .data = gic_of_init },
	{ .compatible = "qcom,msm-gpio", .data = msm_gpio_of_init_legacy, },
	{ }
};

static void __init mmi_msm8960_init_irq(void)
{
	struct device_node *node;

	node = of_find_matching_node(NULL, mmi_msm8960_dt_gic_match);
	if (node) {
		msm_mpm_irq_extn_init(&msm8960_mpm_dev_data);
		of_irq_init(mmi_msm8960_dt_gic_match);
		of_node_put(node);
	} else
		msm8960_init_irq();
}

static struct of_device_id mmi_of_setup[] __initdata = {
	{ .compatible = "linux,seriallow", .data = &system_serial_low },
	{ .compatible = "linux,serialhigh", .data = &system_serial_high },
	{ .compatible = "linux,hwrev", .data = &system_rev },
	{ .compatible = "mmi,prod_id", .data = &prod_id },
	{ .compatible = "mmi,mbmprotocol", .data = &mbmprotocol },
	{ }
};

static void __init mmi_of_populate_setup(void)
{
	struct device_node *n = of_find_node_by_path("/chosen");
	struct of_device_id *tbl = mmi_of_setup;
	const char *baseband;

	while (tbl->data) {
		of_property_read_u32(n, tbl->compatible, tbl->data);
		tbl++;
	}

	if (0 == of_property_read_string(n, "mmi,baseband", &baseband))
		strlcpy(extended_baseband, baseband, sizeof(extended_baseband));

	of_node_put(n);
}

static struct of_dev_auxdata mmi_auxdata[] __initdata = {
	OF_DEV_AUXDATA("qcom,i2c-qup", 0x16200000, "qup_i2c.3", NULL),
	OF_DEV_AUXDATA("qcom,i2c-qup", 0x16300000, "qup_i2c.4", NULL),
	OF_DEV_AUXDATA("qcom,i2c-qup", 0x1a000000, "qup_i2c.8", NULL),
	OF_DEV_AUXDATA("qcom,i2c-qup", 0x1a200000, "qup_i2c.10", NULL),
	OF_DEV_AUXDATA("qcom,i2c-qup", 0x12480000, "qup_i2c.12", NULL),
	{}
};

static void __init mmi_msm8960_dt_init(void)
{
	struct of_dev_auxdata *adata = mmi_auxdata;

	mmi_of_populate_setup();
	of_platform_populate(NULL, of_default_bus_match_table, adata, NULL);
	msm8960_cdp_init();
}

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
	.init_irq = mmi_msm8960_init_irq,
	.handle_irq = gic_handle_irq,
	.timer = &msm_timer,
	.init_machine = mmi_msm8960_dt_init,
	.init_early = mmi_msm8960_init_early,
	.init_very_early = msm8960_early_memory,
	.restart = msm_restart,
	.dt_compat = mmi_dt_match,
MACHINE_END

