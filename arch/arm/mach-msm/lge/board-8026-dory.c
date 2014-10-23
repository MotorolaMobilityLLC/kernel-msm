/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 * Copyright (c) 2014, LGE inc. All rights reserved.
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

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/i2c/i2c-qup.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#include <linux/memblock.h>
#include <linux/regulator/cpr-regulator.h>
#include <linux/regulator/qpnp-regulator.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <linux/regulator/spm-regulator.h>
#include <linux/clk/msm-clk-provider.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/board.h>
#include <linux/msm-bus.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_memtypes.h>
#include <soc/qcom/socinfo.h>
#include <mach/board.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include "../board-dt.h"
#include "../clock.h"
#include "../platsmp.h"
#include <mach/board_lge.h>

static struct of_dev_auxdata msm_hsic_host_adata[] = {
	OF_DEV_AUXDATA("qcom,hsic-host", 0xF9A00000, "msm_hsic_host", NULL),
	{}
};

static struct of_dev_auxdata msm8226_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9864900, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,hsic-host", 0xF9A00000, "msm_hsic_host", NULL),
	OF_DEV_AUXDATA("qcom,hsic-smsc-hub", 0, "msm_smsc_hub",
			msm_hsic_host_adata),

	{}
};

static unsigned long limit_mem;

static int __init limit_mem_setup(char *param)
{
	limit_mem = memparse(param, NULL);
	return 0;
}
early_param("limit_mem", limit_mem_setup);

static void __init limit_mem_reserve(void)
{
	unsigned long to_remove;
	unsigned long reserved_mem;
	unsigned long i;
	phys_addr_t base;

	if (!limit_mem)
		return;

	reserved_mem = ALIGN(memblock.reserved.total_size, PAGE_SIZE);

	to_remove = memblock.memory.total_size - reserved_mem - limit_mem;

	pr_info("Limiting memory from %lu KB to to %lu kB by removing %lu kB\n",
			(memblock.memory.total_size - reserved_mem) / 1024,
			limit_mem / 1024,
			to_remove / 1024);

	/* First find as many highmem pages as possible */
	for (i = 0; i < to_remove; i += PAGE_SIZE) {
		base = memblock_find_in_range(memblock.current_limit,
				MEMBLOCK_ALLOC_ANYWHERE, PAGE_SIZE, PAGE_SIZE);
		if (!base)
			break;
		memblock_remove(base, PAGE_SIZE);
	}
	/* Then find as many lowmem 1M sections as possible */
	for (; i < to_remove; i += SECTION_SIZE) {
		base = memblock_find_in_range(0, MEMBLOCK_ALLOC_ACCESSIBLE,
				SECTION_SIZE, SECTION_SIZE);
		if (!base)
			break;
		memblock_remove(base, SECTION_SIZE);
	}
}

static void __init msm8226_reserve(void)
{
	of_scan_flat_dt(dt_scan_for_memory_reserve, NULL);
#ifdef CONFIG_PSTORE_RAM
	lge_reserve();
#endif
	limit_mem_reserve();
}
/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm8226_add_drivers(void)
{
	msm_smd_init();
	msm_rpm_driver_init();
	msm_spm_device_init();
	msm_pm_sleep_status_init();
	rpm_smd_regulator_driver_init();
	qpnp_regulator_init();
	spm_regulator_init();
	msm_gcc_8226_init();
	msm_bus_fabric_init_driver();
	qup_i2c_init_driver();
	cpr_regulator_init();
#ifdef CONFIG_PSTORE_RAM
	lge_add_persistent_device();
#endif
}

void __init msm8226_init(void)
{
	struct of_dev_auxdata *adata = msm8226_auxdata_lookup;

	/*
	 * populate devices from DT first so smem probe will get called as part
	 * of msm_smem_init.  socinfo_init needs smem support so call
	 * msm_smem_init before it.  msm8226_init_gpiomux needs socinfo so
	 * call socinfo_init before it.
	 */
	board_dt_populate(adata);

	msm_smem_init();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm8226_init_gpiomux();
	msm8226_add_drivers();
}

static const char * const msm8226_dt_match[] __initconst = {
	"qcom,msm8226",
	"qcom,msm8926",
	"qcom,apq8026",
	NULL
};

DT_MACHINE_START(MSM8226_DT, "Qualcomm MSM 8226 DORY (Flattened Device Tree)")
	.map_io = msm_map_msm8226_io,
	.init_machine = msm8226_init,
	.dt_compat = msm8226_dt_match,
	.reserve = msm8226_reserve,
	.smp = &arm_smp_ops,
MACHINE_END
