/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/mfd/pmic8058.h>
#include <linux/jiffies.h>
#include <linux/suspend.h>
#include <linux/percpu.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>
#include <asm/fiq.h>
#include <asm/hardware/gic.h>
#include <mach/msm_iomap.h>
#include <asm/mach-types.h>
#include <asm/cacheflush.h>
#include <mach/scm.h>
#include <mach/socinfo.h>
#include "msm_watchdog.h"
#include "timer.h"
#ifdef CONFIG_MSM_WATCHDOG_CTX_PRINT
#include <linux/memory_alloc.h>
#include <linux/persistent_ram.h>
#include <asm/bootinfo.h>
#include <asm/stacktrace.h>
#include <asm/traps.h>
#include <mach/msm_memtypes.h>
#endif /* CONFIG_MSM_WATCHDOG_CTX_PRINT */

#define MODULE_NAME "msm_watchdog"

#define TCSR_WDT_CFG	0x30

#define WDT_RST		0x0
#define WDT_EN		0x8
#define WDT_STS		0xC
#define WDT_BARK_TIME	0x14
#define WDT_BITE_TIME	0x24

#define WDT_HZ		32768

struct msm_watchdog_dump msm_dump_cpu_ctx;

static void __iomem *msm_wdt_base;

static unsigned long delay_time;
static unsigned long bark_time;
static unsigned long long last_pet;
static bool has_vic;
static unsigned int msm_wdog_irq;

/*
 * On the kernel command line specify
 * msm_watchdog.enable=1 to enable the watchdog
 * By default watchdog is turned on
 */
static int enable = 1;
module_param(enable, int, 0);

/*
 * Watchdog bark reboot timeout in seconds.
 * Can be specified in kernel command line.
 */
static int reboot_bark_timeout = 22;
module_param(reboot_bark_timeout, int, 0644);
/*
 * If the watchdog is enabled at bootup (enable=1),
 * the runtime_disable sysfs node at
 * /sys/module/msm_watchdog/runtime_disable
 * can be used to deactivate the watchdog.
 * This is a one-time setting. The watchdog
 * cannot be re-enabled once it is disabled.
 */
static int runtime_disable;
static DEFINE_MUTEX(disable_lock);
static int wdog_enable_set(const char *val, struct kernel_param *kp);
module_param_call(runtime_disable, wdog_enable_set, param_get_int,
			&runtime_disable, 0644);

/*
 * On the kernel command line specify msm_watchdog.appsbark=1 to handle
 * watchdog barks in Linux. By default barks are processed by the secure side.
 */
static int appsbark;
module_param(appsbark, int, 0);

static int appsbark_fiq;

/*
 * Use /sys/module/msm_watchdog/parameters/print_all_stacks
 * to control whether stacks of all running
 * processes are printed when a wdog bark is received.
 */
static int print_all_stacks = 1;
module_param(print_all_stacks, int,  S_IRUGO | S_IWUSR);

/* Area for context dump in secure mode */
static unsigned long scm_regsave;	/* phys */

static struct msm_watchdog_pdata __percpu **percpu_pdata;

static void pet_watchdog_work(struct work_struct *work);
static void init_watchdog_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(dogwork_struct, pet_watchdog_work);
static DECLARE_WORK(init_dogwork_struct, init_watchdog_work);

/* Called from the FIQ bark handler */
void msm_wdog_bark_fin(void)
{
	flush_cache_all();
	pr_crit("\nApps Watchdog bark received - Calling BUG()\n");
	BUG();
}

static int msm_watchdog_suspend(struct device *dev)
{
	if (!enable)
		return 0;

	__raw_writel(1, msm_wdt_base + WDT_RST);
	__raw_writel(0, msm_wdt_base + WDT_EN);
	mb();
	return 0;
}

static int msm_watchdog_resume(struct device *dev)
{
	if (!enable)
		return 0;

	__raw_writel(1, msm_wdt_base + WDT_EN);
	__raw_writel(1, msm_wdt_base + WDT_RST);
	mb();
	return 0;
}

static int panic_wdog_handler(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	if (panic_timeout == 0) {
		__raw_writel(0, msm_wdt_base + WDT_EN);
		mb();
	} else {
		__raw_writel(WDT_HZ * (panic_timeout + 4),
				msm_wdt_base + WDT_BARK_TIME);
		__raw_writel(WDT_HZ * (panic_timeout + 4),
				msm_wdt_base + WDT_BITE_TIME);
		__raw_writel(1, msm_wdt_base + WDT_RST);
	}
	return NOTIFY_DONE;
}

static struct notifier_block panic_blk = {
	.notifier_call	= panic_wdog_handler,
};

#define get_sclk_hz(t_ms) ((t_ms / 1000) * WDT_HZ)
#define get_reboot_bark_timeout(t_s) ((t_s * MSEC_PER_SEC) < bark_time ? \
		get_sclk_hz(bark_time) : get_sclk_hz(t_s * MSEC_PER_SEC))

static int msm_watchdog_reboot_notifier(struct notifier_block *this,
		unsigned long code, void *unused)
{

	u64 timeout = get_reboot_bark_timeout(reboot_bark_timeout);
	__raw_writel(timeout, msm_wdt_base + WDT_BARK_TIME);
	__raw_writel(timeout + 3 * WDT_HZ,
			msm_wdt_base + WDT_BITE_TIME);
	__raw_writel(1, msm_wdt_base + WDT_RST);

	return NOTIFY_DONE;
}

static struct notifier_block msm_reboot_notifier = {
	.notifier_call = msm_watchdog_reboot_notifier,
};

struct wdog_disable_work_data {
	struct work_struct work;
	struct completion complete;
};

static void wdog_disable_work(struct work_struct *work)
{
	struct wdog_disable_work_data *work_data =
		container_of(work, struct wdog_disable_work_data, work);
	__raw_writel(0, msm_wdt_base + WDT_EN);
	mb();
	if (has_vic) {
		free_irq(msm_wdog_irq, 0);
	} else {
		disable_percpu_irq(msm_wdog_irq);
		if (!appsbark_fiq) {
			free_percpu_irq(msm_wdog_irq,
					percpu_pdata);
			free_percpu(percpu_pdata);
		}
	}
	enable = 0;
	atomic_notifier_chain_unregister(&panic_notifier_list, &panic_blk);
	unregister_reboot_notifier(&msm_reboot_notifier);
	cancel_delayed_work(&dogwork_struct);
	/* may be suspended after the first write above */
	__raw_writel(0, msm_wdt_base + WDT_EN);
	complete(&work_data->complete);
	pr_info("MSM Watchdog deactivated.\n");
}

static int wdog_enable_set(const char *val, struct kernel_param *kp)
{
	int ret = 0;
	int old_val = runtime_disable;
	struct wdog_disable_work_data work_data;

	mutex_lock(&disable_lock);
	if (!enable) {
		printk(KERN_INFO "MSM Watchdog is not active.\n");
		ret = -EINVAL;
		goto done;
	}

	ret = param_set_int(val, kp);
	if (ret)
		goto done;

	if (runtime_disable == 1) {
		if (old_val)
			goto done;
		init_completion(&work_data.complete);
		INIT_WORK_ONSTACK(&work_data.work, wdog_disable_work);
		schedule_work_on(0, &work_data.work);
		wait_for_completion(&work_data.complete);
	} else {
		runtime_disable = old_val;
		ret = -EINVAL;
	}
done:
	mutex_unlock(&disable_lock);
	return ret;
}

unsigned min_slack_ticks = UINT_MAX;
unsigned long long min_slack_ns = ULLONG_MAX;

void pet_watchdog(void)
{
	int slack;
	unsigned long long time_ns;
	unsigned long long slack_ns;
	unsigned long long bark_time_ns = bark_time * 1000000ULL;

	if (!enable || !msm_wdt_base)
		return;

	slack = __raw_readl(msm_wdt_base + WDT_STS) >> 3;
	slack = ((bark_time*WDT_HZ)/1000) - slack;
	if (slack < min_slack_ticks)
		min_slack_ticks = slack;
	__raw_writel(1, msm_wdt_base + WDT_RST);
	time_ns = sched_clock();
	slack_ns = (last_pet + bark_time_ns) - time_ns;
	if (slack_ns < min_slack_ns)
		min_slack_ns = slack_ns;
	last_pet = time_ns;
}

static void pet_watchdog_work(struct work_struct *work)
{
	pet_watchdog();

	if (enable)
		schedule_delayed_work_on(0, &dogwork_struct, delay_time);
}

void msm_watchdog_reset(unsigned int timeout)
{
	unsigned long flags;

	if (!msm_wdt_base)
		return;

	local_irq_save(flags);

	if (timeout > 60)
		timeout = 60;

	__raw_writel(WDT_HZ * timeout, msm_wdt_base + WDT_BARK_TIME);
	__raw_writel(WDT_HZ * (timeout + 2), msm_wdt_base + WDT_BITE_TIME);
	__raw_writel(1, msm_wdt_base + WDT_EN);
	__raw_writel(1, msm_wdt_base + WDT_RST);

	for (timeout += 2; timeout > 0; timeout--)
		mdelay(1000);

	for (timeout = 2; timeout > 0; timeout--) {
		__raw_writel(0, msm_wdt_base + WDT_BARK_TIME);
		__raw_writel(WDT_HZ, msm_wdt_base + WDT_BITE_TIME);
		__raw_writel(1, msm_wdt_base + WDT_RST);
		mdelay(1000);
	}

	for (timeout = 2; timeout > 0; timeout--) {
		__raw_writel(WDT_HZ, msm_wdt_base + WDT_BARK_TIME);
		__raw_writel(0, msm_wdt_base + WDT_BITE_TIME);
		__raw_writel(1, msm_wdt_base + WDT_RST);
		mdelay(1000);
	}
	pr_err("Watchdog reset has failed\n");

	local_irq_restore(flags);
}
static irqreturn_t wdog_bark_handler(int irq, void *dev_id)
{
	unsigned long nanosec_rem;
	unsigned long long t = sched_clock();
	struct task_struct *tsk;

	nanosec_rem = do_div(t, 1000000000);
	printk(KERN_INFO "Watchdog bark! Now = %lu.%06lu\n", (unsigned long) t,
		nanosec_rem / 1000);

	nanosec_rem = do_div(last_pet, 1000000000);
	printk(KERN_INFO "Watchdog last pet at %lu.%06lu\n", (unsigned long)
		last_pet, nanosec_rem / 1000);

	if (print_all_stacks) {

		/* Suspend wdog until all stacks are printed */
		msm_watchdog_suspend(NULL);

		printk(KERN_INFO "Stack trace dump:\n");

		for_each_process(tsk) {
			printk(KERN_INFO "\nPID: %d, Name: %s\n",
				tsk->pid, tsk->comm);
			show_stack(tsk, NULL);
		}

		msm_watchdog_resume(NULL);
	}

	pr_err("Apps watchdog bark received!");
	BUG();
	return IRQ_HANDLED;
}

#define SCM_SET_REGSAVE_CMD 0x2

#ifdef CONFIG_MSM_WATCHDOG_CTX_PRINT

#define TZBSP_SC_STATUS_NS_BIT		0x01
#define TZBSP_SC_STATUS_WDT		0x02
#define TZBSP_SC_STATUS_SGI		0x04
#define TZBSP_SC_STATUS_WARM_BOOT	0x08

#define TZBSP_DUMP_CTX_MAGIC		0x44434151
#define TZBSP_DUMP_CTX_VERSION		1

#define MSM_WDT_CTX_SIG			0x77647473
#define MSM_WDT_CTX_REV			0x00010003

#define NR_CPUS_MAX			4
#define NR_CPUS_2			2
#define NR_CPUS_4			4

#define RET_STAGE_SHIFT			(28)
#define RET_SECTION_SHIFT		(24)
#define RET_ERR_SHIFT			(16)
#define RET_MISC_SHIFT			(12)

#define RET_TZ				(0xfu << RET_STAGE_SHIFT)
#define RET_SBL2			(0xeu << RET_STAGE_SHIFT)

#define ERR_NONE			(0xffu << RET_ERR_SHIFT)

struct tzbsp_mon_cpu_ctx_s {
	u32 mon_lr;
	u32 mon_spsr;
	u32 usr_r0;
	u32 usr_r1;
	u32 usr_r2;
	u32 usr_r3;
	u32 usr_r4;
	u32 usr_r5;
	u32 usr_r6;
	u32 usr_r7;
	u32 usr_r8;
	u32 usr_r9;
	u32 usr_r10;
	u32 usr_r11;
	u32 usr_r12;
	u32 usr_r13;
	u32 usr_r14;
	u32 irq_spsr;
	u32 irq_r13;
	u32 irq_r14;
	u32 svc_spsr;
	u32 svc_r13;
	u32 svc_r14;
	u32 abt_spsr;
	u32 abt_r13;
	u32 abt_r14;
	u32 und_spsr;
	u32 und_r13;
	u32 und_r14;
	u32 fiq_spsr;
	u32 fiq_r8;
	u32 fiq_r9;
	u32 fiq_r10;
	u32 fiq_r11;
	u32 fiq_r12;
	u32 fiq_r13;
	u32 fiq_r14;
};

/* Structure of the entire non-secure context dump buffer. Because TZ is single
 * entry only a single secure context is saved. */
struct tzbsp_dump_buf_s0 {
	u32 magic;
	u32 version;
	u32 cpu_count;
};

struct tzbsp_dump_buf_s2 {
	u32 magic;
	u32 version;
	u32 cpu_count;
	u32 sc_status[NR_CPUS_2];
	struct tzbsp_mon_cpu_ctx_s sc_ns[NR_CPUS_2];
	struct tzbsp_mon_cpu_ctx_s sec;
	u32 wdt0_sts[NR_CPUS_2];
};

struct tzbsp_dump_buf_s4 {
	u32 magic;
	u32 version;
	u32 cpu_count;
	u32 sc_status[NR_CPUS_4];
	struct tzbsp_mon_cpu_ctx_s sc_ns[NR_CPUS_4];
	struct tzbsp_mon_cpu_ctx_s sec;
	u32 wdt0_sts[NR_CPUS_4];
};

union tzbsp_dump_buf_s {
	struct tzbsp_dump_buf_s0 s0;
	struct tzbsp_dump_buf_s2 s2;
	struct tzbsp_dump_buf_s4 s4;
};

struct msm_wdt_lnx_info {
	u32 tsk_size;
	u32 ti_tsk_offset;
} __packed __aligned(4);

struct msm_wdt_ctx_info {
	struct msm_wdt_lnx_info lnx;
	u32 size_tz;
	u32 rev_tz;
	u32 size;
	u32 cpu_count;
	u32 ret;
	u32 rev;
	u32 sig;
} __packed __aligned(4);

struct msm_wdt_page1 {
	union tzbsp_dump_buf_s tzbsp;
	u8 spare[PAGE_SIZE - sizeof(union tzbsp_dump_buf_s)
			- sizeof(struct msm_wdt_ctx_info)];
	struct msm_wdt_ctx_info cinfo;
} __packed __aligned(PAGE_SIZE);

struct msm_wdt_cpy {
	u32 from;
	u32 to;
	u32 size;
} __packed __aligned(4);

#define LNX_STACK	0
#define LNX_TASK	1
#define LNX_CTX_AREAS	2

struct msm_wdt_pcp {
	u32 ret;
	u32 stack_va;
	struct msm_wdt_cpy jobs[LNX_CTX_AREAS];
} __packed __aligned(4);

struct msm_wdt_ctx {
	struct msm_wdt_page1 p1;
} __packed __aligned(PAGE_SIZE);

/*
 *	---------	---------------------
 *	|	|	| tzbsp_dump_buf_s
 *	| page1 |	| padding
 *	|	|	| msm_wdt_ctx_info
 *	---------	---------------------
 *	| page2 |	| msm_wdt_pcp[0], ...
 *	| ...	|	| task_struct[0], ...
 *	| pagex |	| padding
 *	---------	---------------------
 *	| ...	|	| stack[0], ...
 *	|	|	|
 */

#define MSM_WDT_CTX_PCP_SIZE	(sizeof(struct msm_wdt_pcp) * get_core_count())
#define MSM_WDT_CTX_TSK_SIZE	(sizeof(struct task_struct) * get_core_count())

#define MSM_WDT_CTX_MID_SIZE	(ALIGN((MSM_WDT_CTX_PCP_SIZE + \
					MSM_WDT_CTX_TSK_SIZE), PAGE_SIZE))
#define MSM_WDT_CTX_THREAD_SIZE	(THREAD_SIZE * get_core_count())
#define MSM_WDT_CTX_SIZE	(sizeof(struct msm_wdt_ctx)	\
				+ MSM_WDT_CTX_MID_SIZE \
				+ MSM_WDT_CTX_THREAD_SIZE)

void __init reserve_memory_for_watchdog(void)
{
	reserve_info->memtype_reserve_table[MEMTYPE_EBI1].size
		+= MSM_WDT_CTX_SIZE;
}

static unsigned long msm_watchdog_alloc_buf(void)
{
	BUILD_BUG_ON((sizeof(union tzbsp_dump_buf_s)
		+ sizeof(struct msm_wdt_ctx_info)) > PAGE_SIZE);
	BUILD_BUG_ON(CONFIG_NR_CPUS > NR_CPUS_MAX);
	return allocate_contiguous_memory_nomap(MSM_WDT_CTX_SIZE,
			MEMTYPE_EBI1, PAGE_SIZE);
}

#define MSMWDTD(fmt, args...) do {			\
	persistent_ram_ext_oldbuf_print(fmt, ##args);	\
} while (0)

static void __init msm_wdt_show_status(u32 sc_status, const char *label)
{
	if (!sc_status) {
		MSMWDTD("%s: probably didn't finish dump.\n", label);
		return;
	}
	MSMWDTD("%s: was in %ssecure world.\n", label,
		(sc_status & TZBSP_SC_STATUS_NS_BIT) ? "non-" : "");
	if (sc_status & TZBSP_SC_STATUS_WDT)
		MSMWDTD("%s: experienced a watchdog timeout.\n", label);
	if (sc_status & TZBSP_SC_STATUS_SGI)
		MSMWDTD("%s: some other core experienced a watchdog timeout.\n",
			label);
	if (sc_status & TZBSP_SC_STATUS_WARM_BOOT)
		MSMWDTD("%s: WDT bark occured during TZ warm boot.\n", label);
}

static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;
static void __init msm_wdt_show_task(struct task_struct *p,
			struct thread_info *ti)
{
	unsigned state;

	state = p->state ? __ffs(p->state) + 1 : 0;
	MSMWDTD("%-15.15s %c", p->comm,
		state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
	if (state == TASK_RUNNING)
		MSMWDTD(" running  ");
	else
		MSMWDTD(" %08x ", ti->cpu_context.pc);
	MSMWDTD("pid %6d tgid %6d 0x%08lx\n", task_pid_nr(p), task_tgid_nr(p),
			(unsigned long)ti->flags);
}

static void __init msm_wdt_show_regs(struct tzbsp_mon_cpu_ctx_s *regs,
				const char *label)
{
	MSMWDTD("%s\n", label);
	MSMWDTD("\tr12: %08x  r11: %08x  r10: %08x  r9 : %08x  r8 : %08x\n",
			regs->usr_r12, regs->usr_r11, regs->usr_r10,
			regs->usr_r9, regs->usr_r8);
	MSMWDTD("\tr7 : %08x  r6 : %08x  r5 : %08x  r4 : %08x\n",
			regs->usr_r7, regs->usr_r6, regs->usr_r5,
			regs->usr_r4);
	MSMWDTD("\tr3 : %08x  r2 : %08x  r1 : %08x  r0 : %08x\n",
			regs->usr_r3, regs->usr_r2, regs->usr_r1,
			regs->usr_r0);
	MSMWDTD("MON:\tlr : %08x  spsr: %08x\n",
			regs->mon_lr, regs->mon_spsr);
	MSMWDTD("SVC:\tlr : %08x  sp : %08x  spsr : %08x\n",
			regs->svc_r14, regs->svc_r13, regs->svc_spsr);
	MSMWDTD("IRQ:\tlr : %08x  sp : %08x  spsr : %08x\n",
			regs->irq_r14, regs->irq_r13, regs->irq_spsr);
	MSMWDTD("ABT:\tlr : %08x  sp : %08x  spsr : %08x\n",
			regs->abt_r14, regs->abt_r13, regs->abt_spsr);
	MSMWDTD("UND:\tlr : %08x  sp : %08x  spsr : %08x\n",
			regs->und_r14, regs->und_r13, regs->und_spsr);
	MSMWDTD("FIQ:\tlr : %08x  sp : %08x  spsr : %08x\n",
			regs->fiq_r14, regs->fiq_r13, regs->fiq_spsr);
	MSMWDTD("\tr12: %08x  r11: %08x  r10: %08x  r9 : %08x  r8 : %08x\n",
			regs->fiq_r12, regs->fiq_r11, regs->fiq_r10,
			regs->fiq_r9, regs->fiq_r8);
	MSMWDTD("USR:\tlr : %08x  sp : %08x\n",
			regs->usr_r14, regs->usr_r13);
}

static void __init msm_wdt_show_raw_mem(unsigned long addr, int nbytes,
			unsigned long old_addr, const char *label)
{
	int	i, j;
	int	nlines;
	u32	*p;

	MSMWDTD("%s: %#lx: ", label, old_addr);
	if (!virt_addr_valid(old_addr)) {
		MSMWDTD("is not valid kernel address.\n");
		return;
	}
	MSMWDTD("\n");

	/*
	 * round address down to a 32 bit boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (u32 *)(addr & ~(sizeof(u32) - 1));
	nbytes += (addr & (sizeof(u32) - 1));
	nlines = (nbytes + 31) / 32;


	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		MSMWDTD("%04lx ", (unsigned long)old_addr & 0xffff);
		for (j = 0; j < 8; j++) {
			MSMWDTD(" %08x", *p);
			++p;
			old_addr += sizeof(*p);
		}
		MSMWDTD("\n");
	}
}

static void __init msm_wdt_unwind_backtrace(struct tzbsp_mon_cpu_ctx_s *regs,
			unsigned long addr, unsigned long stack)
{
	struct stackframe frame;
	int offset;

	if (!virt_addr_valid(addr)) {
		MSMWDTD("%08lx is not valid kernel address.\n", addr);
		return;
	}
	if ((regs->svc_r13 & ~(THREAD_SIZE - 1)) == addr) {
		frame.fp = (regs->usr_r11 & (THREAD_SIZE - 1)) + stack;
		frame.sp = (regs->svc_r13 & (THREAD_SIZE - 1)) + stack;
		frame.lr = regs->svc_r14;
		frame.pc = regs->mon_lr;
	} else {
		struct thread_info *ti = (struct thread_info *)stack;
		frame.fp = ti->cpu_context.fp - addr + stack;
		frame.sp = ti->cpu_context.sp - addr + stack;
		frame.lr = 0;
		frame.pc = ti->cpu_context.pc;
	}
	offset = (frame.sp - stack - 128) & ~(128 - 1);
	msm_wdt_show_raw_mem(stack, 96, addr, "thread_info");
	msm_wdt_show_raw_mem(stack + offset, THREAD_SIZE - offset,
			addr + offset, "stack");
	while (1) {
		int urc;
		unsigned long where = frame.pc;

		urc = unwind_frame(&frame);
		if (urc < 0)
			break;
		MSMWDTD("[<%08lx>] (%pS) from [<%08lx>] (%pS)\n", where,
			(void *)where, frame.pc, (void *)frame.pc);
		if (in_exception_text(where)) {
			struct pt_regs *excep_regs =
					(struct pt_regs *)(frame.sp);
			if ((excep_regs->ARM_sp & ~(THREAD_SIZE - 1)) == addr) {
				excep_regs->ARM_sp -= addr;
				excep_regs->ARM_sp += stack;
			}
		}
	}
}

static void __init msm_wdt_ctx_print(struct msm_wdt_ctx *ctx)
{
	int i;
	unsigned long stack_tmp = 0, orr = 0;
	u32 *sc_status, *wdt0_sts;
	struct tzbsp_mon_cpu_ctx_s *sc_ns;
	char label[64];
	u8 (*stack)[THREAD_SIZE];
	const int cpu_count = get_core_count();
	struct msm_wdt_pcp *pcp;
	struct task_struct *task;

	if ((ctx->p1.tzbsp.s0.magic != TZBSP_DUMP_CTX_MAGIC)
		|| (ctx->p1.tzbsp.s0.version != TZBSP_DUMP_CTX_VERSION)
		|| (ctx->p1.tzbsp.s0.cpu_count != cpu_count)) {
		MSMWDTD("msm_wdt_ctx: tzbsp dump buffer mismatch.\n");
		MSMWDTD("Expected: magic 0x%08x, version %d, cpu_count %d\n",
			TZBSP_DUMP_CTX_MAGIC, TZBSP_DUMP_CTX_VERSION,
			cpu_count);
		MSMWDTD("Found:    magic 0x%08x, version %d, cpu_count %d\n",
			ctx->p1.tzbsp.s0.magic, ctx->p1.tzbsp.s0.version,
			ctx->p1.tzbsp.s0.cpu_count);
		return;
	}
	switch (cpu_count) {
	case NR_CPUS_2:
		sc_status = ctx->p1.tzbsp.s2.sc_status;
		sc_ns = ctx->p1.tzbsp.s2.sc_ns;
		wdt0_sts = ctx->p1.tzbsp.s2.wdt0_sts;
		break;
	case NR_CPUS_4:
		sc_status = ctx->p1.tzbsp.s4.sc_status;
		sc_ns = ctx->p1.tzbsp.s4.sc_ns;
		wdt0_sts = ctx->p1.tzbsp.s4.wdt0_sts;
		break;
	default:
		MSMWDTD("msm_wdt_ctx: unsupported cpu_count %d\n", cpu_count);
		return;
	}
	for (i = 0; i < cpu_count; i++) {
		orr |= wdt0_sts[i];
		orr |= sc_ns[i].mon_lr;
	}
	if (!orr) {
		u32 *tzctx;
		if (bi_powerup_reason() != PU_REASON_WDOG_AP_RESET)
			return;
		for (tzctx = sc_status; tzctx < (u32 *)ctx->p1.spare; tzctx++) {
			orr |= *tzctx;
			if (orr)
				break;
		}
		if (!orr) {
			MSMWDTD("\n*** Might be Watchdog Bite ***\n");
			return;
		} else {
			MSMWDTD("*** Likely Bad Dump ***\n");
		}
	}
	MSMWDTD("sc_status: ");
	for (i = 0; i < cpu_count; i++)
		MSMWDTD("0x%x ", sc_status[i]);
	MSMWDTD("\n");
	for (i = 0; i < cpu_count; i++) {
		snprintf(label, sizeof(label) - 1, "CPU%d", i);
		msm_wdt_show_status(sc_status[i], label);
	}
	MSMWDTD("wdt0_sts: ");
	for (i = 0; i < cpu_count; i++)
		MSMWDTD("0x%08x ", wdt0_sts[i]);
	MSMWDTD("\n");
	for (i = 0; i < cpu_count; i++) {
		snprintf(label, sizeof(label) - 1, "CPU%d nsec", i);
		msm_wdt_show_regs(&sc_ns[i], label);
	}
	msm_wdt_show_regs(&sc_ns[cpu_count], "sec");
	if ((ctx->p1.cinfo.sig != MSM_WDT_CTX_SIG)
		|| (ctx->p1.cinfo.rev_tz != MSM_WDT_CTX_REV)
		|| (ctx->p1.cinfo.size_tz != MSM_WDT_CTX_SIZE)) {
		MSMWDTD("msm_wdt_ctx: linux dump buffer mismatch.\n");
		MSMWDTD("Expected: sig 0x%08x, ver 0x%08x size 0x%08x\n",
			MSM_WDT_CTX_SIG, MSM_WDT_CTX_REV, MSM_WDT_CTX_SIZE);
		MSMWDTD("Found:    sig 0x%08x, ver 0x%08x size 0x%08x\n",
			ctx->p1.cinfo.sig, ctx->p1.cinfo.rev_tz,
			ctx->p1.cinfo.size_tz);
		return;
	}
	/* now dump customized stuff */
	MSMWDTD("\n");
	MSMWDTD("ret 0x%08x\n", ctx->p1.cinfo.ret);
	MSMWDTD("\n");
	pcp = (struct msm_wdt_pcp *)(ctx + 1);
	for (i = 0; i < cpu_count; i++) {
		struct msm_wdt_cpy *job;
		MSMWDTD("CPU%d: ", i);
		MSMWDTD("ret 0x%x ", pcp[i].ret);
		MSMWDTD("stack %08x ", pcp[i].stack_va);
		job = &pcp[i].jobs[LNX_TASK];
		MSMWDTD("%08x -> %08x (%x) ", job->from, job->to, job->size);
		job = &pcp[i].jobs[LNX_STACK];
		MSMWDTD("%08x -> %08x (%x)", job->from, job->to, job->size);
		MSMWDTD("\n");
	}
	if ((ctx->p1.cinfo.ret != (RET_SBL2 | ERR_NONE)))
		return;
	MSMWDTD("\n");
	task = (struct task_struct *)(pcp + cpu_count);
	stack = (u8 (*)[THREAD_SIZE])(ALIGN((unsigned long)(task + cpu_count),
					PAGE_SIZE));
	if (!IS_ALIGNED((unsigned long)stack, THREAD_SIZE)) {
		stack_tmp = __get_free_pages(GFP_KERNEL, THREAD_SIZE_ORDER);
		if (!stack_tmp)
			pr_err("Allocating temporary storage for stack failed.\n");
	}
	for (i = 0; i < cpu_count; i++) {
		unsigned long addr, data;

		if (pcp[i].ret != (RET_SBL2 | ERR_NONE))
			continue;
		MSMWDTD("CPU%d: ", i);
		addr = pcp[i].stack_va;
		if (stack_tmp) {
			memcpy((void *)stack_tmp, stack[i], THREAD_SIZE);
			data = stack_tmp;
		} else {
			data = (unsigned long)stack[i];
		}
		msm_wdt_show_task(&task[i], (struct thread_info *)data);
		msm_wdt_unwind_backtrace(&sc_ns[i], addr, data);
		MSMWDTD("\n");
	}
	if (stack_tmp)
		free_pages(stack_tmp, THREAD_SIZE_ORDER);
}

static void __init msm_watchdog_ctx_lnx(struct msm_wdt_lnx_info *lnx)
{
	lnx->tsk_size = sizeof(struct task_struct);
	lnx->ti_tsk_offset = offsetof(struct thread_info, task);
}

static void __init msm_wdt_ctx_reset(struct msm_wdt_ctx *ctx)
{
	memset(ctx, 0, MSM_WDT_CTX_SIZE);
	ctx->p1.cinfo.sig = MSM_WDT_CTX_SIG;
	ctx->p1.cinfo.rev = MSM_WDT_CTX_REV;
	ctx->p1.cinfo.cpu_count = get_core_count();
	ctx->p1.cinfo.size = MSM_WDT_CTX_SIZE;
	msm_watchdog_ctx_lnx(&ctx->p1.cinfo.lnx);
}

#else /* CONFIG_MSM_WATCHDOG_CTX_PRINT undefined below */

#define MSM_WDT_CTX_SIZE	PAGE_SIZE

static unsigned long msm_watchdog_alloc_buf(void)
{
	unsigned long ctx_va;

	ctx_va = __get_free_page(GFP_KERNEL);
	if (ctx_va)
		return __pa(ctx_va);
	else
		return 0;
}
#endif /* CONFIG_MSM_WATCHDOG_CTX_PRINT */

static void configure_bark_dump(void)
{
	int ret;
	struct {
		unsigned addr;
		int len;
	} cmd_buf;

	if (!appsbark) {
		if (!scm_regsave)
			scm_regsave = msm_watchdog_alloc_buf();

		if (scm_regsave) {
			cmd_buf.addr = scm_regsave;
			cmd_buf.len  = MSM_WDT_CTX_SIZE;

			ret = scm_call(SCM_SVC_UTIL, SCM_SET_REGSAVE_CMD,
				       &cmd_buf, sizeof(cmd_buf), NULL, 0);
			if (ret)
				pr_err("Setting register save address failed.\n"
				       "Registers won't be dumped on a dog "
				       "bite\n");
		} else {
			pr_err("Allocating register save space failed\n"
			       "Registers won't be dumped on a dog bite\n");
			/*
			 * No need to bail if allocation fails. Simply don't
			 * send the command, and the secure side will reset
			 * without saving registers.
			 */
		}
	}
}

struct fiq_handler wdog_fh = {
	.name = MODULE_NAME,
};

static void init_watchdog_work(struct work_struct *work)
{
	u64 timeout = (bark_time * WDT_HZ)/1000;
	void *stack;
	int ret;

	if (has_vic) {
		ret = request_irq(msm_wdog_irq, wdog_bark_handler, 0,
				  "apps_wdog_bark", NULL);
		if (ret)
			return;
	} else if (appsbark_fiq) {
		claim_fiq(&wdog_fh);
		set_fiq_handler(&msm_wdog_fiq_start, msm_wdog_fiq_length);
		stack = (void *)__get_free_pages(GFP_KERNEL, THREAD_SIZE_ORDER);
		if (!stack) {
			pr_info("No free pages available - %s fails\n",
					__func__);
			return;
		}

		msm_wdog_fiq_setup(stack);
		gic_set_irq_secure(msm_wdog_irq);
	} else {
		percpu_pdata = alloc_percpu(struct msm_watchdog_pdata *);
		if (!percpu_pdata) {
			pr_err("%s: memory allocation failed for percpu data\n",
					__func__);
			return;
		}

		/* Must request irq before sending scm command */
		ret = request_percpu_irq(msm_wdog_irq,
			wdog_bark_handler, "apps_wdog_bark", percpu_pdata);
		if (ret) {
			free_percpu(percpu_pdata);
			return;
		}
	}

	configure_bark_dump();

	__raw_writel(timeout, msm_wdt_base + WDT_BARK_TIME);
	__raw_writel(timeout + 3*WDT_HZ, msm_wdt_base + WDT_BITE_TIME);

	schedule_delayed_work_on(0, &dogwork_struct, delay_time);

	atomic_notifier_chain_register(&panic_notifier_list,
				       &panic_blk);

	ret = register_reboot_notifier(&msm_reboot_notifier);
	if (ret)
		pr_err("Failed to register reboot notifier\n");

	__raw_writel(1, msm_wdt_base + WDT_EN);
	__raw_writel(1, msm_wdt_base + WDT_RST);
	last_pet = sched_clock();

	if (!has_vic)
		enable_percpu_irq(msm_wdog_irq, IRQ_TYPE_EDGE_RISING);

	printk(KERN_INFO "MSM Watchdog Initialized\n");

	return;
}

static int msm_watchdog_probe(struct platform_device *pdev)
{
	struct msm_watchdog_pdata *pdata = pdev->dev.platform_data;

	if (!enable || !pdata || !pdata->pet_time || !pdata->bark_time) {
		printk(KERN_INFO "MSM Watchdog Not Initialized\n");
		return -ENODEV;
	}

	bark_time = pdata->bark_time;
	/* reboot_bark_timeout (in seconds) might have been supplied as
	 * module parameter.
	 */
	if ((reboot_bark_timeout * MSEC_PER_SEC) < bark_time)
		reboot_bark_timeout = (bark_time / MSEC_PER_SEC);
	has_vic = pdata->has_vic;
	if (!pdata->has_secure) {
		appsbark = 1;
		appsbark_fiq = pdata->use_kernel_fiq;
	}

	msm_wdt_base = pdata->base;
	msm_wdog_irq = platform_get_irq(pdev, 0);

	/*
	 * This is only temporary till SBLs turn on the XPUs
	 * This initialization will be done in SBLs on a later releases
	 */
	if (cpu_is_msm9615())
		__raw_writel(0xF, MSM_TCSR_BASE + TCSR_WDT_CFG);

	if (pdata->needs_expired_enable)
		__raw_writel(0x1, MSM_CLK_CTL_BASE + 0x3820);

	delay_time = msecs_to_jiffies(pdata->pet_time);
	schedule_work_on(0, &init_dogwork_struct);
	return 0;
}

static const struct dev_pm_ops msm_watchdog_dev_pm_ops = {
	.suspend_noirq = msm_watchdog_suspend,
	.resume_noirq = msm_watchdog_resume,
};

static struct platform_driver msm_watchdog_driver = {
	.probe = msm_watchdog_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.pm = &msm_watchdog_dev_pm_ops,
	},
};

#ifdef CONFIG_MSM_WATCHDOG_CTX_PRINT
static int __init init_watchdog_buf_early(void)
{
	struct msm_wdt_ctx *ctx;
	scm_regsave = msm_watchdog_alloc_buf();
	if (scm_regsave) {
		ctx = ioremap(scm_regsave, MSM_WDT_CTX_SIZE);
		if (!ctx) {
			pr_err("msm_watchdog: cannot remap buffer: %08lx\n",
					scm_regsave);
			return -EFAULT;
		}
		msm_wdt_ctx_print(ctx);
		msm_wdt_ctx_reset(ctx);
		iounmap(ctx);
	}
	return 0;
}
#else
static inline void init_watchdog_buf_early(void) { }
#endif /* CONFIG_MSM_WATCHDOG_CTX_PRINT */

static int __init init_watchdog(void)
{
	init_watchdog_buf_early();
	return platform_driver_register(&msm_watchdog_driver);
}

core_initcall(init_watchdog);
MODULE_DESCRIPTION("MSM Watchdog Driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
