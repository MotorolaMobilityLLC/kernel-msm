/*
 * mfld.c: Intel Medfield platform setup code
 *
 * (C) Copyright 2012 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/mfd/intel_msic.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/intel_pmic_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>

#include <asm/setup.h>
#include <asm/mpspec_def.h>
#include <asm/hw_irq.h>
#include <asm/apic.h>
#include <asm/io_apic.h>
#include <asm/intel-mid.h>
#include <asm/intel_mid_vrtc.h>
#include <asm/io.h>
#include <asm/i8259.h>
#include <asm/intel_scu_ipc.h>
#include <asm/apb_timer.h>
#include <asm/reboot.h>

static void penwell_arch_setup(void);
/* penwell arch ops */
static struct intel_mid_ops penwell_ops = {
	.arch_setup = penwell_arch_setup,
};

static unsigned long __init mfld_calibrate_tsc(void)
{
	unsigned long fast_calibrate;
	u32 lo, hi, ratio, fsb;

	rdmsr(MSR_IA32_PERF_STATUS, lo, hi);
	pr_debug("IA32 perf status is 0x%x, 0x%0x\n", lo, hi);
	ratio = (hi >> 8) & 0x1f;
	pr_debug("ratio is %d\n", ratio);
	if (!ratio) {
		pr_err("read a zero ratio, should be incorrect!\n");
		pr_err("force tsc ratio to 16 ...\n");
		ratio = 16;
	}
	rdmsr(MSR_FSB_FREQ, lo, hi);
	switch (lo & BSEL_SOC_FUSE_MASK) {
	case BSEL_SOC_FUSE_001:
		fsb = FSB_FREQ_133SKU;
		break;
	case BSEL_SOC_FUSE_101:
		fsb = FSB_FREQ_100SKU;
		break;
	case BSEL_SOC_FUSE_111:
		fsb = FSB_FREQ_83SKU;
		break;
	default:
		pr_err("%s: unsupported BSEL_SOC_FUSE: %d, fall back to 83SKU",
					__func__, lo & BSEL_SOC_FUSE_MASK);
		fsb = FSB_FREQ_83SKU;
		break;
	}

	if (intel_mid_identify_cpu() == INTEL_MID_CPU_CHIP_CLOVERVIEW
		&& fsb == FSB_FREQ_133SKU) {
		pr_warn("See a Clovertrail+ B0/B1/B2 processor and will correct its tsc info!\n");
		/* The 133MHz FSB for CLVP+ is actually 133.120 MHz */
		fsb = 133120;
	}

	fast_calibrate = ratio * fsb;
	pr_debug("read penwell tsc %lu khz\n", fast_calibrate);
	lapic_timer_frequency = fsb * 1000 / HZ;
	/* mark tsc clocksource as reliable */
	set_cpu_cap(&boot_cpu_data, X86_FEATURE_TSC_RELIABLE);

	if (fast_calibrate)
		return fast_calibrate;

	return 0;
}

static void __init penwell_arch_setup()
{
	x86_platform.calibrate_tsc = mfld_calibrate_tsc;
}

void *get_penwell_ops()
{
	return &penwell_ops;
}

void *get_cloverview_ops()
{
	return &penwell_ops;
}
