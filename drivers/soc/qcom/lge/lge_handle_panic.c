/*
 * drivers/soc/qcom/lge/lge_handle_panic.c
 *
 * Copyright (C) 2014-2015 LG Electronics, Inc
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

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <asm/setup.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/subsystem_restart.h>

#include <soc/qcom/lge/lge_handle_panic.h>
#include <soc/qcom/lge/board_lge.h>

#define PANIC_HANDLER_NAME        "panic-handler"

#define RESTART_REASON_ADDR       0x65C
#define DLOAD_MODE_ADDR           0x0

#define CRASH_HANDLER_MAGIC_NUM   0x4c474500
#define CRASH_HANDLER_MAGIC_ADDR  0x34
#define RAM_CONSOLE_ADDR_ADDR     0x38
#define RAM_CONSOLE_SIZE_ADDR     0x3C
#define FB_ADDR_ADDR              0x40

#define RESTART_REASON      (msm_imem_base + RESTART_REASON_ADDR)
#define CRASH_HANDLER_MAGIC (msm_imem_base + CRASH_HANDLER_MAGIC_ADDR)
#define RAM_CONSOLE_ADDR    (msm_imem_base + RAM_CONSOLE_ADDR_ADDR)
#define RAM_CONSOLE_SIZE    (msm_imem_base + RAM_CONSOLE_SIZE_ADDR)
#define FB_ADDR             (msm_imem_base + FB_ADDR_ADDR)

static void *msm_imem_base;
static int dummy_arg;

static int subsys_crash_magic = 0x0;
static int gen_modem_panic_type;

static struct panic_handler_data *panic_handler;

void lge_set_subsys_crash_reason(const char *name, int type)
{
	const char *subsys_name[] = { "adsp", "mba", "modem", "wcnss", "venus", "AR6320"};
	int i = 0;

	if (!name)
		return;

	for (i = 0; i < ARRAY_SIZE(subsys_name); i++) {
		if (!strncmp(subsys_name[i], name, 40)) {
			subsys_crash_magic = LGE_RB_MAGIC | ((i+1) << 12)
					| type;
			break;
		}
	}
	return;
}

void lge_set_ram_console_addr(unsigned int addr, unsigned int size)
{
	writel_relaxed(addr, RAM_CONSOLE_ADDR);
	writel_relaxed(size, RAM_CONSOLE_SIZE);
}

void lge_set_fb_addr(unsigned int addr)
{
	writel_relaxed(addr, FB_ADDR);
}

void lge_set_restart_reason(unsigned int reason)
{
	writel_relaxed(reason, RESTART_REASON);
}

void lge_set_panic_reason(void)
{
	if (subsys_crash_magic == 0)
		lge_set_restart_reason(LGE_RB_MAGIC | LGE_ERR_KERN);
	else
		lge_set_restart_reason(subsys_crash_magic);
}

static bool lge_crash_handler_skiped = false;
void lge_check_crash_skiped(char *reason)
{
	char *p;

	p = strstr(reason, "AP requested modem reset!!!");
	if (p)
		lge_crash_handler_skiped = true;
}
EXPORT_SYMBOL(lge_check_crash_skiped);

bool lge_is_crash_skipped(void)
{
	return lge_crash_handler_skiped;
}
EXPORT_SYMBOL(lge_is_crash_skipped);

void lge_clear_crash_skipped(void)
{
	lge_crash_handler_skiped = false;
};
EXPORT_SYMBOL(lge_clear_crash_skipped);

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
module_param_call(gen_panic, gen_panic, param_get_bool, &dummy_arg,
		S_IWUSR | S_IRUGO);

static int gen_adsp_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("adsp");
	return 0;
}
module_param_call(gen_adsp_panic, gen_adsp_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_mba_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("mba");
	return 0;
}
module_param_call(gen_mba_panic, gen_mba_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_venus_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("venus");
	return 0;
}
module_param_call(gen_venus_panic, gen_venus_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_ar6320_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("AR6320");
	return 0;
}
module_param_call(gen_ar6320_panic, gen_ar6320_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

int lge_get_modem_panic(void)
{
	return gen_modem_panic_type;
}

EXPORT_SYMBOL(lge_get_modem_panic);

static int gen_modem_panic(const char *val, struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}
	pr_err("gen_modem_panic param to %d\n", gen_modem_panic_type);
	switch (gen_modem_panic_type) {
	case 2:
		subsys_modem_restart();
		break;
	default:
		subsystem_restart("modem");
		break;
	}
	return 0;
}
module_param_call(gen_modem_panic, gen_modem_panic, param_get_bool,
		&gen_modem_panic_type, S_IWUSR | S_IRUGO);

static int gen_wcnss_panic(const char *val, struct kernel_param *kp)
{
	subsystem_restart("wcnss");
	return 0;
}
module_param_call(gen_wcnss_panic, gen_wcnss_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define WDT0_RST        0x04
#define WDT0_EN         0x08
#define WDT0_BARK_TIME  0x10
#define WDT0_BITE_TIME  0x14

extern void __iomem *wdt_timer_get_timer0_base(void);

static int gen_wdt_bark(const char *val, struct kernel_param *kp)
{
	void __iomem *msm_tmr0_base;
	msm_tmr0_base = wdt_timer_get_timer0_base();

	pr_info("%s\n", __func__);
	writel_relaxed(0, msm_tmr0_base + WDT0_EN);
	writel_relaxed(1, msm_tmr0_base + WDT0_RST);
	writel_relaxed(0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	writel_relaxed(5 * 0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	writel_relaxed(1, msm_tmr0_base + WDT0_EN);
	mb();
	mdelay(5000);

	pr_err("%s failed\n", __func__);

	return -EIO;
}
module_param_call(gen_wdt_bark, gen_wdt_bark, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_wdt_bite(const char *val, struct kernel_param *kp)
{
	void __iomem *msm_tmr0_base;
	msm_tmr0_base = wdt_timer_get_timer0_base();

	pr_info("%s\n", __func__);
	writel_relaxed(0, msm_tmr0_base + WDT0_EN);
	writel_relaxed(1, msm_tmr0_base + WDT0_RST);
	writel_relaxed(5 * 0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	writel_relaxed(0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	writel_relaxed(1, msm_tmr0_base + WDT0_EN);
	mb();
	mdelay(5000);

	pr_err("%s failed\n", __func__);

	return -EIO;
}
module_param_call(gen_wdt_bite, gen_wdt_bite, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define REG_MPM2_WDOG_BASE             0xFC4AA000
#define REG_OFFSET_MPM2_WDOG_RESET     0x0
#define REG_OFFSET_MPM2_WDOG_BITE_VAL  0x10
#define REG_VAL_WDOG_RESET_DO_RESET    0x10
#define REG_VAL_WDOG_BITE_VAL          0x400

/* forced sec wdt bite can cause unexpected bus hang */
static int gen_sec_wdt_bite(const char *val, struct kernel_param *kp)
{
	void *sec_wdog_virt;
	sec_wdog_virt = ioremap(REG_MPM2_WDOG_BASE, SZ_4K);

	if (!sec_wdog_virt) {
		pr_err("unable to map sec wdog page\n");
		return -ENOMEM;
	}

	pr_info("%s\n", __func__);
	writel_relaxed(REG_VAL_WDOG_RESET_DO_RESET,
		sec_wdog_virt + REG_OFFSET_MPM2_WDOG_RESET);
	writel_relaxed(REG_VAL_WDOG_BITE_VAL,
		sec_wdog_virt + REG_OFFSET_MPM2_WDOG_BITE_VAL);
	mb();
	mdelay(5000);

	pr_err("%s failed\n", __func__);
	iounmap(sec_wdog_virt);

	return -EIO;
}
module_param_call(gen_sec_wdt_bite, gen_sec_wdt_bite, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define SCM_SVC_SEC_WDOG_TRIG  0x08

static int gen_sec_wdt_scm(const char *val, struct kernel_param *kp)
{
	struct scm_desc desc;
	desc.args[0] = 0;
	desc.arginfo = SCM_ARGS(1);

	pr_info("%s\n", __func__);
	scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_BOOT,
			SCM_SVC_SEC_WDOG_TRIG), &desc);

	pr_err("%s failed\n", __func__);

	return -EIO;
}
module_param_call(gen_sec_wdt_scm, gen_sec_wdt_scm, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define MPM2_TSENS_Sn_MIN_MAX_STATUS_CTRL  0xFC4A800C

static int gen_tsense_reset(const char *val, struct kernel_param *kp)
{
	int status;
	int max_threshold = 0;
	void *mpm2_tsens_min_max_status_ctrl;

	mpm2_tsens_min_max_status_ctrl
		= ioremap(MPM2_TSENS_Sn_MIN_MAX_STATUS_CTRL, SZ_32);

	if (!mpm2_tsens_min_max_status_ctrl) {
		pr_err("unable to map mpm2 register space\n");
		return -ENOMEM;
	}

	status = readl_relaxed(mpm2_tsens_min_max_status_ctrl);
	max_threshold = (status & 0xFFC00) >> 10;

	while (max_threshold > 0) {
		max_threshold -= 0xF;

		pr_info("%s sensor0 threshold:0x%x\n", __func__, max_threshold);
		writel_relaxed((status & 0xFFF003FF) | (max_threshold << 10),
			mpm2_tsens_min_max_status_ctrl);
		mb();
		mdelay(100);
	}

	pr_err("%s failed\n", __func__);
	iounmap(mpm2_tsens_min_max_status_ctrl);

	return -EIO;
}
module_param_call(gen_tsense_reset, gen_tsense_reset, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define SCM_SVC_SEC_WDOG_DIS  0x07

void sec_watchdog_disable(void)
{
	int ret;
	struct scm_desc desc;
	desc.args[0] = 1;
	desc.arginfo = SCM_ARGS(1);

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_BOOT,
				SCM_SVC_SEC_WDOG_DIS), &desc);

	if (ret || desc.ret[0]) {
		pr_err("%s failed\n", __func__);
		return;
	}

	pr_info("%s\n", __func__);
}

static int gen_bus_hang(const char *val, struct kernel_param *kp)
{
	void *reserved = ioremap(0xFF000000 - 0x10, SZ_32);

	if (!reserved) {
		pr_err("unable to map reserved\n");
		return -ENOMEM;
	}

	pr_info("%s\n", __func__);
	writel_relaxed(0x1, reserved);
	mb();

	pr_err("%s failed\n", __func__);
	iounmap(reserved);

	return -EIO;
}
module_param_call(gen_bus_hang, gen_bus_hang, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_unknown_reset(const char *val, struct kernel_param *kp)
{
	sec_watchdog_disable();
	/* TODO */
	return 0;
}
module_param_call(gen_unknown_reset, gen_unknown_reset, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

void lge_disable_watchdog(void)
{
	static int once = 1;
	void __iomem *msm_tmr0_base;

	if (once > 1)
		return;

	msm_tmr0_base = wdt_timer_get_timer0_base();
	if (!msm_tmr0_base)
		return;

	writel_relaxed(0, msm_tmr0_base + WDT0_EN);
	mb();
	once++;

	pr_info("%s\n", __func__);
}

void lge_panic_handler_fb_free_page(unsigned long mem_addr, unsigned long size)
{
	unsigned long pfn_start, pfn_end, pfn_idx;

	pfn_start = mem_addr >> PAGE_SHIFT;
	pfn_end = (mem_addr + size) >> PAGE_SHIFT;
	for (pfn_idx = pfn_start; pfn_idx < pfn_end; pfn_idx++)
		free_reserved_page(pfn_to_page(pfn_idx));
}

void lge_panic_handler_fb_cleanup(void)
{
	static int free = 1;

	if (!panic_handler || free > 1)
		return;

	if (panic_handler->fb_addr && panic_handler->fb_size) {
		memblock_free(panic_handler->fb_addr, panic_handler->fb_size);
		lge_panic_handler_fb_free_page(
				panic_handler->fb_addr, panic_handler->fb_size);
		free++;

		pr_info("%s: free[@0x%lx+@0x%lx)\n", PANIC_HANDLER_NAME,
				panic_handler->fb_addr, panic_handler->fb_size);
	}
}

static int __init lge_panic_handler_early_init(void)
{
	struct device_node *np;
	uint32_t crash_handler_magic = 0;
	unsigned long ramoops_addr = 0;
	unsigned long ramoops_size = 0;

	panic_handler = kzalloc(sizeof(*panic_handler), GFP_KERNEL);
	if (!panic_handler) {
		pr_err("could not allocate memory for panic_handler\n");
		return -ENOMEM;
	}

	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem");
	if (!np) {
		pr_err("unable to find DT imem node\n");
		return -ENODEV;
	}

	msm_imem_base = of_iomap(np, 0);
	if (!msm_imem_base) {
		pr_err("unable to map imem\n");
		return -ENODEV;
	}

	np = of_find_compatible_node(NULL, NULL, "ramoops");
	if (!np) {
		pr_err("unable to find ramoops node\n");
		return -ENODEV;
	}

	of_property_read_u32(np, "android,ramoops-buffer-start", (u32*)&ramoops_addr);
	of_property_read_u32(np, "android,ramoops-buffer-size", (u32*)&ramoops_size);
	pr_info("%s: ramoops addr+size[@0x%lx+@0x%lx)\n", PANIC_HANDLER_NAME,
			ramoops_addr, ramoops_size);

	lge_set_ram_console_addr(ramoops_addr, ramoops_size);

	/* check struct boot_shared_imem_cookie_type is matched */
	crash_handler_magic = __raw_readl(CRASH_HANDLER_MAGIC);
	WARN(crash_handler_magic != CRASH_HANDLER_MAGIC_NUM,
			"Check sbl's struct boot_shared_imem_cookie_type.\n"
			"Need to update lge_handle_panic's imem offset.\n");

	/* Set default restart_reason to Unknown reset. */
	lge_set_restart_reason(LGE_RB_MAGIC | LGE_ERR_TZ);

	return 0;
}

early_initcall(lge_panic_handler_early_init);

static int __init lge_panic_handler_probe(struct platform_device *pdev)
{
	int ret = 0;

	return ret;
}

static int lge_panic_handler_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver panic_handler_driver __refdata = {
	.probe = lge_panic_handler_probe,
	.remove = lge_panic_handler_remove,
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

MODULE_DESCRIPTION("LGE panic handler driver");
MODULE_AUTHOR("SungEun Kim <cleaneye.kim@lge.com>");
MODULE_LICENSE("GPL");
