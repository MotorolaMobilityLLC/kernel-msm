/*
 * arch/arm/mach-msm/lge/lge_handle_panic.c
 *
 * Copyright (C) 2012-2014 LGE, Inc
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <asm/setup.h>
#include <mach/subsystem_restart.h>
#include <mach/msm_iomap.h>
#include <mach/lge_handle_panic.h>
#include <soc/qcom/restart.h>
#include <soc/qcom/scm.h>

#define PANIC_HANDLER_NAME        "panic-handler"

#define SCM_WDOG_DEBUG_BOOT_PART    0x9
#define BOOT_PART_EN_VAL            0x5D1

#define RESTART_REASON_OFFSET       0x65C
#define UEFI_RAM_DUMP_MAGIC_OFFSET  0xC
#define RAM_CONSOLE_ADDR_OFFSET     0x28
#define RAM_CONSOLE_SIZE_OFFSET     0x2C
#define FB1_ADDR_OFFSET             0x30

static void *msm_imem_base;
static int dummy_arg;
static int subsys_crash_magic = 0x0;
static int enable = 0; /* 0 - disabled, 1 - enabled */
static int enable_set(const char *val, struct kernel_param *kp);
module_param_call(enable, enable_set, param_get_int,
		&enable, S_IWUSR | S_IRUGO);

static void imem_writel(unsigned int val, unsigned int offset)
{
	if (msm_imem_base)
		__raw_writel(val, msm_imem_base + offset);
}

static int enable_set(const char *val, struct kernel_param *kp)
{
	int ret;
	int old_val = enable;

	ret = param_set_int(val, kp);
	if (ret)
		return ret;

	/* if enable is not zero or one, ignore */
	if (enable >> 1) {
		enable = old_val;
		return -EINVAL;
	}

#ifdef CONFIG_MSM_DLOAD_MODE
	set_dload_mode(enable);
#endif
	return 0;
}

int lge_is_handle_panic_enable(void)
{
	return enable;
}
EXPORT_SYMBOL(lge_is_handle_panic_enable);

int lge_set_magic_subsystem(const char *name, int type)
{
	const char *subsys_name[] = { "adsp", "wcnss" };
	int i = 0;

	if (!name)
		return -1;

	for (i = 0; i < ARRAY_SIZE(subsys_name); i++) {
		if (!strncmp(subsys_name[i], name, 40)) {
			subsys_crash_magic = LGE_RB_MAGIC | ((i+1) << 12)
				| type;
			break;
		}
	}

	return 0;
}
EXPORT_SYMBOL(lge_set_magic_subsystem);

void lge_skip_dload_by_sbl(int on)
{
	/* skip entering dload mode at sbl1 */
	imem_writel(on ? 1 : 0, UEFI_RAM_DUMP_MAGIC_OFFSET);
}
EXPORT_SYMBOL(lge_skip_dload_by_sbl);

void lge_set_ram_console_addr(unsigned int addr, unsigned int size)
{
	imem_writel(addr, RAM_CONSOLE_ADDR_OFFSET);
	imem_writel(size, RAM_CONSOLE_SIZE_OFFSET);
}
EXPORT_SYMBOL(lge_set_ram_console_addr);

void lge_set_fb1_addr(unsigned int addr)
{
	imem_writel(addr, FB1_ADDR_OFFSET);
}
EXPORT_SYMBOL(lge_set_fb1_addr);

void lge_set_panic_reason(void)
{
	if (subsys_crash_magic == 0)
		imem_writel(LGE_RB_MAGIC | LGE_ERR_KERN, RESTART_REASON_OFFSET);
	else
		imem_writel(subsys_crash_magic, RESTART_REASON_OFFSET);
}
EXPORT_SYMBOL(lge_set_panic_reason);

void lge_set_restart_reason(unsigned int reason)
{
	imem_writel(reason, RESTART_REASON_OFFSET);
}
EXPORT_SYMBOL(lge_set_restart_reason);


static int gen_bug(const char *val, struct kernel_param *kp)
{
	BUG();
	return 0;
}
module_param_call(gen_bug, gen_bug, param_get_bool, &dummy_arg,
		S_IWUSR | S_IRUGO);

static int gen_panic(const char *val, struct kernel_param *kp)
{
	panic("generate test-panic");
	return 0;
}
module_param_call(gen_panic, gen_panic, param_get_bool, &dummy_arg,\
		S_IWUSR | S_IRUGO);

static int gen_adsp_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("adsp");
	return 0;
}
module_param_call(gen_adsp_panic, gen_adsp_panic, param_get_bool, &dummy_arg,
		S_IWUSR | S_IRUGO);

static int gen_wcnss_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("wcnss");
	return 0;
}
module_param_call(gen_wcnss_panic, gen_wcnss_panic, param_get_bool, &dummy_arg,
		S_IWUSR | S_IRUGO);

#define WDT0_RST        0x04
#define WDT0_EN         0x08
#define WDT0_BARK_TIME  0x10
#define WDT0_BITE_TIME  0x14

extern void __iomem *wdt_timer_get_timer0_base(void);

static int gen_wdt_bark(const char *val, struct kernel_param *kp)
{
	static void __iomem *msm_tmr0_base;
	msm_tmr0_base = wdt_timer_get_timer0_base();

	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(5 * 0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	__raw_writel(1, msm_tmr0_base + WDT0_EN);
	return 0;
}
module_param_call(gen_wdt_bark, gen_wdt_bark, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_wdt_bite(const char *val, struct kernel_param *kp)
{
	static void __iomem *msm_tmr0_base;
	msm_tmr0_base = wdt_timer_get_timer0_base();
	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(5 * 0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	__raw_writel(1, msm_tmr0_base + WDT0_EN);
	return 0;
}
module_param_call(gen_wdt_bite, gen_wdt_bite, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_bus_hang(const char *val, struct kernel_param *kp)
{
	static void __iomem *reserved;

	reserved = ioremap(0xFF000000 - 0x10, 0x10);
	__raw_writel(1, reserved);

	return 0;
}
module_param_call(gen_bus_hang, gen_bus_hang, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

void lge_msm_enable_wdog_debug(void)
{
	int ret;

	ret = scm_call_atomic2(SCM_SVC_BOOT,
	SCM_WDOG_DEBUG_BOOT_PART, 0, BOOT_PART_EN_VAL);
	if (ret)
		pr_err("failed to enable wdog debug: %d\n", ret);
}

void lge_msm_disable_wdog_debug(void)
{
	int ret;

	ret = scm_call_atomic2(SCM_SVC_BOOT,
	SCM_WDOG_DEBUG_BOOT_PART, 1, 0);
	if (ret)
		pr_err("failed to disable wdog debug: %d\n", ret);
}

static int gen_hw_reset(const char *val, struct kernel_param *kp)
{
	static void __iomem *reserved;

	lge_msm_disable_wdog_debug();
	reserved = ioremap(0xFF000000 - 0x10, 0x10);
	__raw_writel(1, reserved);

	return 0;
}
module_param_call(gen_hw_reset, gen_hw_reset, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int __init lge_panic_handler_probe(struct platform_device *pdev)
{
	int ret = 0;

	return ret;
}

static int __exit lge_panic_handler_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver panic_handler_driver __refdata = {
	.probe = lge_panic_handler_probe,
	.remove = __exit_p(lge_panic_handler_remove),
	.driver = {
		.name = PANIC_HANDLER_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init lge_panic_handler_init(void)
{
	return platform_driver_register(&panic_handler_driver);
}

static void __exit lge_panic_handler_exit(void)
{
	platform_driver_unregister(&panic_handler_driver);
}

module_init(lge_panic_handler_init);
module_exit(lge_panic_handler_exit);

static int __init lge_panic_handler_early_init(void)
{
	struct device_node *np;

	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem");
	if (!np) {
		pr_err("%s: unable to find DT imem node\n", __func__);
		return -ENODEV;
	}

	msm_imem_base = of_iomap(np, 0);
	if (!msm_imem_base) {
		pr_err("%s: unable to map imem offset\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

postcore_initcall(lge_panic_handler_early_init);

MODULE_DESCRIPTION("LGE panic handler driver");
MODULE_AUTHOR("SungEun Kim <cleaneye.kim@lge.com>");
MODULE_LICENSE("GPL");
