/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/regulator/cpr-regulator.h>
#include <linux/regulator/fan53555.h>
#include <linux/regulator/onsemi-ncp6335d.h>
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
#include <soc/qcom/restart.h>
#include <soc/qcom/socinfo.h>
#include <mach/board.h>
#include <soc/qcom/rpm-smd.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"

static struct of_dev_auxdata msm_hsic_host_adata[] = {
	OF_DEV_AUXDATA("qcom,hsic-host", 0xF9A00000, "msm_hsic_host", NULL),
	{}
};

//++ASUS_BSP : add for gpio
#include <mach/gpio.h>
#include <mach/gpiomux.h>
extern int __init device_gpio_init(void);// asus gpio init
void __init device_gpiomux_init(void)
{
	int rc;

	rc = msm_gpiomux_init_dt();
	if (rc) {
		pr_err("%s failed %d\n", __func__, rc);
		return;
	}

	device_gpio_init();

}
//--ASUS_BSP : add for gpio

//ASUS_BSP BerylHou +++ "Add for BT porting"
static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake", //gpio bt_wake_up_host
		.start	= -1,
		.end	= -1, 
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake", //gpio host_wake_up_bt
		.start	= -1, 
		.end	= -1, 
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake", //IRQ bt wake up host
		.start	= -1, 
		.end	= -1, 
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_bluesleep_device = {
	.name		= "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

//ASUS_BSP BerylHou ---

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

//ASUS_BSP BerylHou +++ "Add for BT porting"
static void gpio_bt_init(void)
{
	printk("[bt] gpio init");
	
	bluesleep_resources[0].start = GPIO_BT_WAKE_UP_HOST;
	bluesleep_resources[0].end = GPIO_BT_WAKE_UP_HOST;

	if (g_ASUS_hwID == SPARROW_EVB) {
		bluesleep_resources[1].start = GPIO_HOST_WAKE_UP_BT_EVB2;
		bluesleep_resources[1].end = GPIO_HOST_WAKE_UP_BT_EVB2;
	} else {
		bluesleep_resources[1].start = GPIO_HOST_WAKE_UP_BT_SR;
		bluesleep_resources[1].end = GPIO_HOST_WAKE_UP_BT_SR;
	}

	bluesleep_resources[2].start = gpio_to_irq(GPIO_BT_WAKE_UP_HOST);
	bluesleep_resources[2].end = gpio_to_irq(GPIO_BT_WAKE_UP_HOST);

}

static void __init board_8226_bluesleep_setup(void)
{
	printk("set up bt sleep mode\n");
	gpio_bt_init();
	platform_device_register(&msm_bluesleep_device);
}
//ASUS_BSP BerylHou ---

static void __init msm8226_reserve(void)
{
	of_scan_flat_dt(dt_scan_for_memory_reserve, NULL);
}


//ASUS_BSP lenter +++
#ifdef CONFIG_BATTERY_ASUS
static struct platform_device robin_asus_bat_device = {
       .name = "asus_bat",
       .id = 0,
};     
#endif

static struct platform_device *msm_robin_devices[] = {
	#ifdef CONFIG_BATTERY_ASUS
	&robin_asus_bat_device,
	#endif
};
//ASUS_BSP lenter ---

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
extern void init_bcm_wifi(void);

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
	ncp6335d_regulator_init();
	fan53555_regulator_init();
	cpr_regulator_init();

//ASUS_BSP lenter +++
	platform_add_devices(msm_robin_devices, ARRAY_SIZE(msm_robin_devices));
//ASUS_BSP lenter ---

// ASUS_BSP +++ Init for wifi
	printk("[wlan]: msm8226_add_drivers\n");
	init_bcm_wifi();
// ASUS_BSP --- Init for wifi
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

//+++ASUS_BSP : add for gpio
//	msm8226_init_gpiomux();
	device_gpiomux_init();
//---ASUS_BSP : add for gpio
	msm8226_add_drivers();
	board_8226_bluesleep_setup(); //ASUS_BSP BerylHou +++ "BT porting"
}

static const char *msm8226_dt_match[] __initconst = {
	"qcom,msm8226",
	"qcom,msm8926",
	"qcom,apq8026",
	NULL
};

DT_MACHINE_START(MSM8226_DT,
		"Qualcomm Technologies, Inc. MSM 8226 (Flattened Device Tree)")
	.map_io			= msm_map_msm8226_io,
	.init_machine		= msm8226_init,
	.dt_compat		= msm8226_dt_match,
	.reserve		= msm8226_reserve,
	.smp			= &arm_smp_ops,
MACHINE_END
