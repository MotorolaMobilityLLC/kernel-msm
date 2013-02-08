/*
 * arch/arm/mach-msm/lge/lge_panic_handler.c
 *
 * Copyright (C) 2010 LGE, Inc
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/persistent_ram.h>
#include <asm/setup.h>
#include <mach/board_lge.h>

#include <mach/subsystem_restart.h>
#ifdef CONFIG_CPU_CP15_MMU
#include <linux/ptrace.h>
#endif

#define PANIC_HANDLER_NAME "panic-handler"
#define PANIC_DUMP_CONSOLE 0
#define PANIC_MAGIC_KEY    0x12345678
#define NORMAL_MAGIC_KEY   0x4E4F524D
#define CRASH_ARM9         0x87654321
#define CRASH_REBOOT       0x618E1000

struct crash_log_dump {
	unsigned int magic_key;
	unsigned int size;
	unsigned char buffer[0];
};

static struct crash_log_dump *crash_dump_log;
static unsigned int crash_buf_size = 0;
static int crash_store_flag = 0;

#ifdef CONFIG_CPU_CP15_MMU
unsigned long *cpu_crash_ctx=NULL;
#endif

unsigned int msm_mmuctrl;
unsigned int msm_mmudac;

void store_ctrl(void)
{
	asm("mrc p15, 0, %0, c1, c0, 0\n"
			: "=r" (msm_mmuctrl));
}

void store_dac(void)
{
	asm("mrc p15, 0, %0, c3, c0, 0\n"
			: "=r" (msm_mmudac));
}
static DEFINE_SPINLOCK(panic_lock);

static int dummy_arg;
static int gen_bug(const char *val, struct kernel_param *kp)
{
	BUG();
	return 0;
}
module_param_call(gen_bug, gen_bug, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_panic(const char *val, struct kernel_param *kp)
{
	panic("generate test-panic");
	return 0;
}
module_param_call(gen_panic, gen_panic, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_mdm_ssr(const char *val, struct kernel_param *kp)
{
	subsystem_restart("external_modem");
	return 0;
}
module_param_call(gen_mdm_ssr, gen_mdm_ssr, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_modem_ssr(const char *val, struct kernel_param *kp)
{
	subsystem_restart("modem");
	return 0;
}
module_param_call(gen_modem_ssr, gen_modem_ssr, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_riva_ssr(const char *val, struct kernel_param *kp)
{
	subsystem_restart("riva");
	return 0;
}
module_param_call(gen_riva_ssr, gen_riva_ssr, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_dsps_ssr(const char *val, struct kernel_param *kp)
{
	subsystem_restart("dsps");
	return 0;
}
module_param_call(gen_dsps_ssr, gen_dsps_ssr, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_lpass_ssr(const char *val, struct kernel_param *kp)
{
	subsystem_restart("lpass");
	return 0;
}
module_param_call(gen_lpass_ssr, gen_lpass_ssr, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

#define WDT0_RST        0x38
#define WDT0_EN         0x40
#define WDT0_BARK_TIME  0x4C
#define WDT0_BITE_TIME  0x5C

extern void __iomem *msm_timer_get_timer0_base(void);

static int gen_wdt_bark(const char *val, struct kernel_param *kp)
{
	static void __iomem *msm_tmr0_base;
	msm_tmr0_base = msm_timer_get_timer0_base();
	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(5 * 0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	__raw_writel(1, msm_tmr0_base + WDT0_EN);
	return 0;
}
module_param_call(gen_wdt_bark, gen_wdt_bark, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

static int gen_hw_reset(const char *val, struct kernel_param *kp)
{
	static void __iomem *msm_tmr0_base;
	msm_tmr0_base = msm_timer_get_timer0_base();
	__raw_writel(0, msm_tmr0_base + WDT0_EN);
	__raw_writel(1, msm_tmr0_base + WDT0_RST);
	__raw_writel(5 * 0x31F3, msm_tmr0_base + WDT0_BARK_TIME);
	__raw_writel(0x31F3, msm_tmr0_base + WDT0_BITE_TIME);
	__raw_writel(1, msm_tmr0_base + WDT0_EN);
	return 0;
}
module_param_call(gen_hw_reset, gen_hw_reset, param_get_bool,
		&dummy_arg, S_IWUSR | S_IRUGO);

void set_crash_store_enable(void)
{
	if (crash_dump_log->magic_key == NORMAL_MAGIC_KEY)
		crash_store_flag = 1;
	return;
}

void set_crash_store_disable(void)
{
	crash_store_flag = 0;
	return;
}

void store_crash_log(char *p)
{
	if (!crash_dump_log)
		return;

	if (!crash_store_flag)
		return;

	if (crash_dump_log->size == crash_buf_size)
		return;

	for ( ; *p; p++) {
		if (*p == '[') {
			for ( ; *p != ']'; p++)
				;
			p++;
			if (*p == ' ')
				p++;
		}
		if (*p == '<') {
			for ( ; *p != '>'; p++)
				;
			p++;
		}

		crash_dump_log->buffer[crash_dump_log->size++] = *p;

		/* check the buffer size */
		if (crash_dump_log->size == crash_buf_size)
			break;
	}
	crash_dump_log->buffer[crash_dump_log->size] = 0;

	return;
}

#ifdef CONFIG_CPU_CP15_MMU
void lge_save_ctx(struct pt_regs* regs, unsigned int ctrl,
		unsigned int transbase, unsigned int dac)
{
	/* save cpu register for simulation */
	cpu_crash_ctx[0] = regs->ARM_r0;
	cpu_crash_ctx[1] = regs->ARM_r1;
	cpu_crash_ctx[2] = regs->ARM_r2;
	cpu_crash_ctx[3] = regs->ARM_r3;
	cpu_crash_ctx[4] = regs->ARM_r4;
	cpu_crash_ctx[5] = regs->ARM_r5;
	cpu_crash_ctx[6] = regs->ARM_r6;
	cpu_crash_ctx[7] = regs->ARM_r7;
	cpu_crash_ctx[8] = regs->ARM_r8;
	cpu_crash_ctx[9] = regs->ARM_r9;
	cpu_crash_ctx[10] = regs->ARM_r10;
	cpu_crash_ctx[11] = regs->ARM_fp;
	cpu_crash_ctx[12] = regs->ARM_ip;
	cpu_crash_ctx[13] = regs->ARM_sp;
	cpu_crash_ctx[14] = regs->ARM_lr;
	cpu_crash_ctx[15] = regs->ARM_pc;
	cpu_crash_ctx[16] = regs->ARM_cpsr;
	/* save mmu register for simulation */
	cpu_crash_ctx[17] = ctrl;
	cpu_crash_ctx[18] = transbase;
	cpu_crash_ctx[19] = dac;
}
#endif

static int restore_crash_log(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	unsigned long flags;
	crash_store_flag = 0;
	spin_lock_irqsave(&panic_lock, flags);
	crash_dump_log->magic_key = PANIC_MAGIC_KEY;
	spin_unlock_irqrestore(&panic_lock, flags);

	return NOTIFY_DONE;
}

static struct notifier_block panic_handler_block = {
	.notifier_call = restore_crash_log,
};

static int __init panic_handler_probe(struct platform_device *pdev)
{
	struct persistent_ram_zone *prz;
	size_t buffer_size;
	void *buffer;
	int ret = 0;
#ifdef CONFIG_CPU_CP15_MMU
	void *ctx_buf;
#endif

	prz = persistent_ram_init_ringbuffer(&pdev->dev, false);
	if (IS_ERR(prz))
		return PTR_ERR(prz);

	buffer_size = prz->buffer_size - SZ_1K;
	buffer = (void *)prz->buffer;;

	crash_dump_log = (struct crash_log_dump *)buffer;
	memset(crash_dump_log, 0, buffer_size);
	crash_dump_log->magic_key = NORMAL_MAGIC_KEY;
	crash_dump_log->size = 0;
	crash_buf_size =
		buffer_size - offsetof(struct crash_log_dump, buffer) - 1;
#ifdef CONFIG_CPU_CP15_MMU
	ctx_buf = (void *)(buffer + buffer_size);
	cpu_crash_ctx = (unsigned long *)ctx_buf;
#endif
	atomic_notifier_chain_register(&panic_notifier_list,
			&panic_handler_block);
	return ret;
}

static int __devexit panic_handler_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver panic_handler_driver __refdata = {
	.probe = panic_handler_probe,
	.remove = __devexit_p(panic_handler_remove),
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
