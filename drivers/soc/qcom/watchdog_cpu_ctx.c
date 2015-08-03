/*
 * Copyright (C) 2014 Motorola Mobility LLC
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

#include <linux/types.h>
#include <linux/thread_info.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/dma-contiguous.h>
#include <linux/dma-mapping.h>
#include <linux/cma.h>
#include <linux/pstore_ram.h>
#include <asm/stacktrace.h>
#include <asm/traps.h>
#include <asm/sections.h>
#include <asm/cputype.h>
#include <soc/qcom/memory_dump.h>
#include <soc/qcom/bootinfo.h>
#include "watchdog_cpu_ctx.h"

#define DUMP_MAGIC_NUMBER	0x42445953

#define CPU_FORMAT_VERSION4	0x14

struct sysdbg_cpu64_ctxt_regs {
	uint64_t x0;
	uint64_t x1;
	uint64_t x2;
	uint64_t x3;
	uint64_t x4;
	uint64_t x5;
	uint64_t x6;
	uint64_t x7;
	uint64_t x8;
	uint64_t x9;
	uint64_t x10;
	uint64_t x11;
	uint64_t x12;
	uint64_t x13;
	uint64_t x14;
	uint64_t x15;
	uint64_t x16;
	uint64_t x17;
	uint64_t x18;
	uint64_t x19;
	uint64_t x20;
	uint64_t x21;
	uint64_t x22;
	uint64_t x23;
	uint64_t x24;
	uint64_t x25;
	uint64_t x26;
	uint64_t x27;
	uint64_t x28;
	uint64_t x29;
	uint64_t x30;
	uint64_t pc;
	uint64_t currentEL;
	uint64_t sp_el3;
	uint64_t elr_el3;
	uint64_t spsr_el3;
	uint64_t sp_el2;
	uint64_t elr_el2;
	uint64_t spsr_el2;
	uint64_t sp_el1;
	uint64_t elr_el1;
	uint64_t spsr_el1;
	uint64_t sp_el0;
	uint64_t __reserved1;
	uint64_t __reserved2;
	uint64_t __reserved3;
	uint64_t __reserved4;
};

struct sysdbg_cpu32_ctxt_regs {
	union {
		uint32_t r0;
		uint64_t x0;
	};
	union {
		uint32_t r1;
		uint64_t x1;
	};
	union {
		uint32_t r2;
		uint64_t x2;
	};
	union {
		uint32_t r3;
		uint64_t x3;
	};
	union {
		uint32_t r4;
		uint64_t x4;
	};
	union {
		uint32_t r5;
		uint64_t x5;
	};
	union {
		uint32_t r6;
		uint64_t x6;
	};
	union {
		uint32_t r7;
		uint64_t x7;
	};
	union {
		uint32_t r8;
		uint64_t x8;
	};
	union {
		uint32_t r9;
		uint64_t x9;
	};
	union {
		uint32_t r10;
		uint64_t x10;
	};
	union {
		uint32_t r11;
		uint64_t x11;
	};
	union {
		uint32_t r12;
		uint64_t x12;
	};
	union {
		uint32_t r13_usr;
		uint64_t x13;
	};
	union {
		uint32_t r14_usr;
		uint64_t x14;
	};
	union {
		uint32_t r13_hyp;
		uint64_t x15;
	};
	union {
		uint32_t r14_irq;
		uint64_t x16;
	};
	union {
		uint32_t r13_irq;
		uint64_t x17;
	};
	union {
		uint32_t r14_svc;
		uint64_t x18;
	};
	union {
		uint32_t r13_svc;
		uint64_t x19;
	};
	union {
		uint32_t r14_abt;
		uint64_t x20;
	};
	union {
		uint32_t r13_abt;
		uint64_t x21;
	};
	union {
		uint32_t r14_und;
		uint64_t x22;
	};
	union {
		uint32_t r13_und;
		uint64_t x23;
	};
	union {
		uint32_t r8_fiq;
		uint64_t x24;
	};
	union {
		uint32_t r9_fiq;
		uint64_t x25;
	};
	union {
		uint32_t r10_fiq;
		uint64_t x26;
	};
	union {
		uint32_t r11_fiq;
		uint64_t x27;
	};
	union {
		uint32_t r12_fiq;
		uint64_t x28;
	};
	union {
		uint32_t r13_fiq;
		uint64_t x29;
	};
	union {
		uint32_t r14_fiq;
		uint64_t x30;
	};
	union {
		uint32_t pc;
		uint64_t x31;
	};
	union {
		uint32_t r13_mon;
		uint64_t x32;
	};
	union {
		uint32_t r14_mon;
		uint64_t x33;
	};
	union {
		uint32_t r14_hyp;
		uint64_t x34;
	};
	union {
		uint32_t _reserved;
		uint64_t x35;
	};
	union {
		uint32_t __reserved1;
		uint64_t x36;
	};
	union {
		uint32_t __reserved2;
		uint64_t x37;
	};
	union {
		uint32_t __reserved3;
		uint64_t x38;
	};
	union {
		uint32_t __reserved4;
		uint64_t x39;
	};
};

union sysdbg_cpu_ctxt_regs_type {
	struct sysdbg_cpu32_ctxt_regs	cpu32_ctxt;
	struct sysdbg_cpu64_ctxt_regs	cpu64_ctxt;
};

struct sysdbgCPUCtxtType {
	uint32_t status[4];
	union sysdbg_cpu_ctxt_regs_type cpu_regs;
	union sysdbg_cpu_ctxt_regs_type __reserved3; /* Secure - Not used */
};

#ifndef THREAD_SIZE_ORDER
#define THREAD_SIZE_ORDER	(get_order(THREAD_SIZE))
#endif

#define WDOG_SYSDBG_SIZE_PERCPU	(MAX_CPU_CTX_SIZE)
#define WDOG_SYSDBG_SIZE	(num_present_cpus() * WDOG_SYSDBG_SIZE_PERCPU)

union sysdbg_cpuctx {
	struct sysdbgCPUCtxtType data;
	uint8_t space[WDOG_SYSDBG_SIZE_PERCPU];
};

enum cpuctx_type {
	CPUCTX_LNX_INFO,
	CPUCTX_STAT,
	CPUCTX_DUMP_DATA,
	CPUCTX_TASK_STRUCT,
	CPUCTX_THREAD,
	CPUCTX_TYPES,
	CPUCTX_MAX = 20,
};

#define CPUCTX_LNX_INFO_MAGIC		0x4c4e5831
#define CPUCTX_STAT_MAGIC		0x4c4e5832
#define CPUCTX_DUMP_DATA_MAGIC		0x4c4e5833
#define CPUCTX_TASK_STRUCT_MAGIC	0x4c4e5834
#define CPUCTX_THREAD_MAGIC		0x4c4e5835

struct msm_wdog_cpuctx_header {
	uint32_t	type; /* cpuctx_type */
	uint32_t	magic;
	uint64_t	offset;
	uint64_t	length;
} __packed __aligned(4);

#define MSM_WDOG_CTX_SIG		0x77647473
#define MSM_WDOG_CTX_REV		0x00020000

#define RET_STAGE_SHIFT			(28)
#define RET_SECTION_SHIFT		(24)
#define RET_ERR_SHIFT			(16)
#define RET_MISC_SHIFT			(12)

#define RET_TZ				(0xfu << RET_STAGE_SHIFT)
#define ERR_NONE			(RET_TZ | (0xffu << RET_ERR_SHIFT))

struct msm_wdog_cpuctx_info {
	uint32_t sig;
	uint32_t rev;
	uint32_t ret;
	uint32_t size;
	uint32_t rev2;
	uint32_t reserve1;
	uint32_t reserve2;
	uint32_t reserve3;
} __packed __aligned(4);

struct msm_wdog_lnx_info {
	uint32_t tsk_size;
	uint32_t ti_tsk_offset;
	uint32_t aa64;
	uint32_t lpae;
	uint64_t text_paddr;
	uint64_t pgd_paddr;
	uint32_t thread_size;
	uint32_t va_bits;
	uint32_t pa_bits;
	uint32_t page_shift;
	uint32_t aa32_pxn;
	uint32_t pad1;
} __packed __aligned(4);

struct msm_wdog_copy {
	uint64_t from;
	uint64_t to;
	uint64_t size;
} __packed __aligned(4);

enum {
	LNX_STACK,
	LNX_TASK,
	LNX_CTX_AREAS,
};

struct msm_wdog_cpuctx_stat {
	uint32_t ret;
	uint32_t reserved;
	uint64_t stack_va;
	struct msm_wdog_copy jobs[LNX_CTX_AREAS];
} __packed __aligned(4);
/* Each CPU register its own dump data to memory dump table v2. */
struct msm_wdog_cpuctx {
	union sysdbg_cpuctx sysdbg;
	struct msm_wdog_cpuctx_info info;
	struct msm_wdog_cpuctx_header header[CPUCTX_MAX];
	struct msm_wdog_lnx_info lnx;
	struct msm_wdog_cpuctx_stat stat;
	struct msm_dump_data cpu_data;
	struct task_struct task;
	u8 stack[THREAD_SIZE] __aligned(1024);
} __packed __aligned(4);

static const struct msm_wdog_cpuctx_header mwc_header[CPUCTX_TYPES] = {
	{
		.type	= CPUCTX_LNX_INFO,
		.magic	= CPUCTX_LNX_INFO_MAGIC,
		.offset	= offsetof(struct msm_wdog_cpuctx, lnx),
		.length	= sizeof(struct msm_wdog_lnx_info),
	},
	{
		.type	= CPUCTX_STAT,
		.magic	= CPUCTX_STAT_MAGIC,
		.offset	= offsetof(struct msm_wdog_cpuctx, stat),
		.length	= sizeof(struct msm_wdog_cpuctx_stat),
	},
	{
		.type	= CPUCTX_DUMP_DATA,
		.magic	= CPUCTX_DUMP_DATA_MAGIC,
		.offset	= offsetof(struct msm_wdog_cpuctx, cpu_data),
		.length	= sizeof(struct msm_dump_data),
	},
	{
		.type	= CPUCTX_TASK_STRUCT,
		.magic	= CPUCTX_TASK_STRUCT_MAGIC,
		.offset	= offsetof(struct msm_wdog_cpuctx, task),
		.length	= sizeof(struct task_struct),
	},
	{
		.type	= CPUCTX_THREAD,
		.magic	= CPUCTX_THREAD_MAGIC,
		.offset	= offsetof(struct msm_wdog_cpuctx, stack),
		.length	= THREAD_SIZE,
	},
};

#define WDOG_CPUCTX_SIZE_PERCPU	(sizeof(struct msm_wdog_cpuctx))
#define WDOG_CPUCTX_SIZE	(num_present_cpus() * WDOG_CPUCTX_SIZE_PERCPU)

#define MSMDBG(fmt, args...) persistent_ram_annotation_append(fmt, ##args)

#define MSMWDT_ERR(fmt, args...) do { \
	pr_err("WdogCtx: "fmt, ##args); \
	MSMWDTD("WdogCtx: "fmt, ##args); \
} while (0)

#define MSMWDTD_IFWDOG(fmt, args...) do { \
	if (bi_powerup_reason() == PU_REASON_WDOG_AP_RESET) \
		MSMDBG(fmt, ##args); \
} while (0)

#define MSMWDTD(fmt, args...) MSMWDTD_IFWDOG(fmt, ##args)

#define SC_STATUS_NS		0x01
#define SC_STATUS_WDT		0x02
#define SC_STATUS_SGI		0x04
#define SC_STATUS_WARM_BOOT	0x08
#define SC_STATUS_DBI		0x10
#define SC_STATUS_CTX_BY_TZ	0x20

#if defined(CONFIG_ARM64)

static int aa64_pa_max(void)
{
	int pa_range = read_cpuid(ID_AA64MMFR0_EL1) & 0xf;

	switch (pa_range) {
	case 0x0:
		return 32;
	case 0x1:
		return 36;
	case 0x2:
		return 40;
	case 0x3:
		return 42;
	case 0x4:
		return 44;
	case 0x5:
		return 48;
	default:
		pr_err("PAMax: Reserved PARange: %#x\n", pa_range);
		break;
	}
	return 32;
}

static void msm_wdog_ctx_lnx_arch(struct msm_wdog_lnx_info *lnx)
{
	lnx->va_bits = VA_BITS;
	lnx->pa_bits = aa64_pa_max();
}

#elif defined(CONFIG_ARM)

#if defined(CONFIG_ARM_LPAE)
static int armv7_pa_max(void)
{
	int cached_mem_size = (read_cpuid_ext(CPUID_EXT_MMFR3) >> 24) & 0xf;

	switch (cached_mem_size) {
	case 0x0:
		return 32;
	case 0x1:
		return 36;
	case 0x2:
		return 40;
	default:
		pr_err("PAMax: Unknown mem size: %#x\n", cached_mem_size);
		break;
	}
	return 32;
}
#else
static int armv7_pa_max(void)
{
	int supersection = (read_cpuid_ext(CPUID_EXT_MMFR3) >> 28) & 0xf;

	if ((supersection == 0x0) && config_enabled(CONFIG_PHYS_ADDR_T_64BIT))
		return 40;
	else
		return 32;
}
#endif /* CONFIG_ARM_LPAE */

static void msm_wdog_ctx_lnx_arch(struct msm_wdog_lnx_info *lnx)
{
	lnx->va_bits = 32;
	lnx->pa_bits = armv7_pa_max();
	lnx->aa32_pxn = ((read_cpuid_ext(CPUID_EXT_MMFR0) & 0xf) >= 0x4);
}

#endif

static void msm_wdog_ctx_lnx(struct msm_wdog_lnx_info *lnx)
{
	lnx->tsk_size = sizeof(struct task_struct);
	lnx->ti_tsk_offset = offsetof(struct thread_info, task);
	lnx->aa64 = config_enabled(CONFIG_ARM64);
	lnx->lpae = config_enabled(CONFIG_ARM_LPAE);
	lnx->text_paddr = virt_to_phys(_text);
	lnx->pgd_paddr = virt_to_phys(swapper_pg_dir);
	lnx->page_shift = PAGE_SHIFT;
	lnx->thread_size = THREAD_SIZE;
	msm_wdog_ctx_lnx_arch(lnx);
}

static void msm_wdog_ctx_header_init(struct msm_wdog_cpuctx *ctxi)
{
	struct msm_wdog_cpuctx_header *header = &ctxi->header[0];

	memset_io(header, 0, sizeof(*header));
	memcpy(header, mwc_header, sizeof(mwc_header));
}

static int msm_wdog_ctx_header_check(struct msm_wdog_cpuctx *ctxi)
{
	struct msm_wdog_cpuctx_header *header = &ctxi->header[0];
	return memcmp(header, mwc_header, sizeof(mwc_header));
}

static void msm_wdog_ctx_reset(struct msm_wdog_cpuctx *ctx, size_t ctx_size)
{
	struct msm_wdog_cpuctx *ctxi;
	int cpu;

	memset_io(ctx, 0, ctx_size);
	for_each_cpu(cpu, cpu_present_mask) {
		ctxi = &ctx[cpu];

		ctxi->info.sig = MSM_WDOG_CTX_SIG;
		ctxi->info.rev = MSM_WDOG_CTX_REV;
		ctxi->info.size = WDOG_CPUCTX_SIZE_PERCPU;
		msm_wdog_ctx_header_init(ctxi);
		msm_wdog_ctx_lnx(&ctxi->lnx);
	}
}

#if defined(CONFIG_ARM64)
static inline int virt_is_valid(unsigned long addr)
{
	return virt_is_valid_lowmem(addr) ||
		(config_enabled(CONFIG_ARCH_THREAD_INFO_ALLOCATOR) &&
			is_vmalloc_addr((const void *)addr));
}
#else
static inline int virt_is_valid(unsigned long addr)
{
	return virt_addr_valid(addr);
}
#endif

static void msm_wdt_show_raw_mem(unsigned long addr, int nbytes,
			unsigned long old_addr, const char *label)
{
	int	i, j;
	int	nlines;
	unsigned long *p;

	MSMWDTD("%s: %#lx: ", label, old_addr);
	if (!virt_is_valid(old_addr)) {
		MSMWDTD("is not valid kernel address.\n");
		return;
	}
	MSMWDTD("\n");

	/*
	 * round address down to unsigned long aligned boundary
	 * and always dump a multiple of 32 bytes
	 */
	p = (unsigned long *)(addr & ~(sizeof(unsigned long) - 1));
	nbytes += (addr & (sizeof(unsigned long) - 1));
	nlines = (nbytes + 31) / 32;

	for (i = 0; i < nlines; i++) {
		/*
		 * just display low 16 bits of address to keep
		 * each line of the dump < 80 characters
		 */
		MSMWDTD("%04lx ", (unsigned long)old_addr & 0xffff);
		for (j = 0; j < (32 / sizeof(unsigned long)); j++) {
			if (sizeof(unsigned long) == 4)
				MSMWDTD(" %08lx", *p);
			else
				MSMWDTD(" %016lx", *p);
			++p;
			old_addr += sizeof(*p);
		}
		MSMWDTD("\n");
	}
}

static int msm_wdog_cpu_regs_version_unknown(uint32_t version)
{
	if (version != CPU_FORMAT_VERSION4)
		return 1;
	return 0;
}

static void msm_wdog_show_sc_status(uint32_t sc_status)
{
	if (sc_status & SC_STATUS_DBI)
		MSMWDTD("SDI: Secure watchdog bite. ");
	if (sc_status & SC_STATUS_CTX_BY_TZ)
		MSMWDTD("TZ: Non-secure watchdog bite. ");
	MSMWDTD("%sS ", (sc_status & SC_STATUS_NS) ?  "N" : " ");
	if (sc_status & SC_STATUS_WDT)
		MSMWDTD("WDT ");
	if (sc_status & SC_STATUS_SGI)
		MSMWDTD("SGI ");
	if (sc_status & SC_STATUS_WARM_BOOT)
		MSMWDTD("WARM_BOOT ");
	MSMWDTD("\n");
}

static const char stat_nam[] = TASK_STATE_TO_CHAR_STR;
static void msm_wdt_show_thread_saved_pc(struct task_struct *p,
				struct thread_info *ti);
static void msm_wdt_show_task(struct task_struct *p, struct thread_info *ti)
{
	unsigned state;

	state = p->state ? __ffs(p->state) + 1 : 0;
	MSMWDTD("%-15.15s %c", p->comm,
		state < sizeof(stat_nam) - 1 ? stat_nam[state] : '?');
	if (state == TASK_RUNNING)
		MSMWDTD(" running  ");
	else
		msm_wdt_show_thread_saved_pc(p, ti);
	MSMWDTD("pid %6d tgid %6d 0x%08lx\n", task_pid_nr(p), task_tgid_nr(p),
			(unsigned long)ti->flags);
}

#if defined(CONFIG_ARM64)

static void msm_wdt_show_thread_saved_pc(struct task_struct *p,
				struct thread_info *ti)
{
	MSMWDTD(" %016lx ", (unsigned long)p->thread.cpu_context.pc);
}

static void msm_wdt_show_regs(struct sysdbgCPUCtxtType *sysdbg_ctx)
{
	struct sysdbg_cpu64_ctxt_regs *regs = &sysdbg_ctx->cpu_regs.cpu64_ctxt;
	uint64_t *gp_reg;
	int i;

	if ((regs->pc >= (unsigned long)_stext) &&
				(regs->pc <= (unsigned long)_etext))
		MSMWDTD("PC is at %pS <%016llx>\n",
			(void *)(unsigned long)regs->pc, regs->pc);
	else
		MSMWDTD("PC is at %016llx\n", regs->pc);
	MSMWDTD("sp : %016llx ", regs->sp_el1);
	MSMWDTD("x30 : %016llx\n", regs->x30);
	gp_reg = (uint64_t *)regs;
	for (i = 29; i >= 0; i--) {
		MSMWDTD("x%-2d: %016llx ", i, gp_reg[i]);
		if (i % 2 == 0)
			MSMWDTD("\n");
	}
	MSMWDTD("currentEL : %016llx\n", regs->currentEL);
	MSMWDTD("EL3: elr : %016llx spsr : %016llx sp : %016llx\n",
			regs->elr_el3, regs->spsr_el3, regs->sp_el3);
	MSMWDTD("EL2: elr : %016llx spsr : %016llx sp : %016llx\n",
			regs->elr_el2, regs->spsr_el2, regs->sp_el2);
	MSMWDTD("EL1: elr : %016llx spsr : %016llx\n",
			regs->elr_el1, regs->spsr_el1);
	MSMWDTD("EL0: sp : %016llx\n", regs->sp_el0);
}

static int msm_wdt_unwind_frame_aa64(struct stackframe *frame,
				unsigned long stack)
{
	unsigned long high, low;
	unsigned long fp = frame->fp;

	low  = frame->sp;
	high = ALIGN(low, THREAD_SIZE);

	if (fp < low || fp > high - 0x18 || fp & 0xf)
		return -EINVAL;

	frame->sp = fp + 0x10;
	frame->fp = (*(unsigned long *)(fp) & (THREAD_SIZE - 1)) + stack;
	frame->pc = *(unsigned long *)(fp + 8) - 4;

	return 0;
}

static void msm_wdt_unwind(struct task_struct *p,
			struct sysdbgCPUCtxtType *sysdbg_ctx,
			unsigned long addr, unsigned long stack)
{
	struct stackframe frame;
	int offset;
	struct sysdbg_cpu64_ctxt_regs *regs = &sysdbg_ctx->cpu_regs.cpu64_ctxt;

	if (!virt_is_valid(addr)) {
		MSMWDTD("%016lx is not valid kernel address.\n", addr);
		return;
	}
	if ((regs->sp_el1 & ~(THREAD_SIZE - 1)) == addr) {
		frame.fp = (regs->x29 & (THREAD_SIZE - 1)) + stack;
		frame.sp = (regs->sp_el1 & (THREAD_SIZE - 1)) + stack;
		frame.pc = regs->pc;
	} else {
		frame.fp = p->thread.cpu_context.fp - addr + stack;
		frame.sp = p->thread.cpu_context.sp - addr + stack;
		frame.pc = p->thread.cpu_context.pc;
	}
	offset = (frame.sp - stack - 128) & ~(128 - 1);
	msm_wdt_show_raw_mem(stack, 96, addr, "thread_info");
	msm_wdt_show_raw_mem(stack + offset, THREAD_SIZE - offset,
			addr + offset, "stack");
	while (1) {
		int urc;
		unsigned long where = frame.pc;

		urc = msm_wdt_unwind_frame_aa64(&frame, stack);
		if (urc < 0)
			break;
		MSMWDTD("[<%016lx>] (%pS)\n", where, (void *)where);
	}
}

#elif defined(CONFIG_ARM)

static void msm_wdt_show_thread_saved_pc(struct task_struct *p,
				struct thread_info *ti)
{
	MSMWDTD(" %08lx ", (unsigned long)ti->cpu_context.pc);
}

static void msm_wdt_show_regs(struct sysdbgCPUCtxtType *sysdbg_ctx)
{
	struct sysdbg_cpu32_ctxt_regs *regs = &sysdbg_ctx->cpu_regs.cpu32_ctxt;

	if ((regs->pc >= (unsigned long)_stext) &&
				(regs->pc <= (unsigned long)_etext))
		MSMWDTD("PC is at %pS <%08x>\n",
			(void *)(unsigned long)regs->pc, (uint32_t)regs->pc);
	else
		MSMWDTD("PC is at %08x\n", (uint32_t)regs->pc);
	MSMWDTD("\tr12: %08x  r11: %08x  r10: %08x  r9 : %08x  r8 : %08x\n",
			regs->r12, regs->r11, regs->r10, regs->r9, regs->r8);
	MSMWDTD("\tr7 : %08x  r6 : %08x  r5 : %08x  r4 : %08x\n",
			regs->r7, regs->r6, regs->r5, regs->r4);
	MSMWDTD("\tr3 : %08x  r2 : %08x  r1 : %08x  r0 : %08x\n",
			regs->r3, regs->r2, regs->r1, regs->r0);
	MSMWDTD("USR:\tr13: %08x  r14: %08x\n", regs->r13_usr, regs->r14_usr);
	MSMWDTD("HYP:\tr13: %08x  r14: %08x\n", regs->r13_hyp, regs->r14_hyp);
	MSMWDTD("IRQ:\tr13: %08x  r14: %08x\n", regs->r13_irq, regs->r14_irq);
	MSMWDTD("SVC:\tr13: %08x  r14: %08x\n", regs->r13_svc, regs->r14_svc);
	MSMWDTD("ABT:\tr13: %08x  r14: %08x\n", regs->r13_abt, regs->r14_abt);
	MSMWDTD("UND:\tr13: %08x  r14: %08x\n", regs->r13_und, regs->r14_und);
	MSMWDTD("MON:\tr13: %08x  r14: %08x\n", regs->r13_mon, regs->r14_mon);
	MSMWDTD("FIQ:\tr13: %08x  r14: %08x\n", regs->r13_fiq, regs->r14_fiq);
	MSMWDTD("\tr12: %08x  r11: %08x  r10: %08x  r9 : %08x  r8 : %08x\n",
			regs->r12_fiq, regs->r11_fiq, regs->r10_fiq,
			regs->r9_fiq, regs->r8_fiq);
}

static void msm_wdt_unwind(struct task_struct *p,
			struct sysdbgCPUCtxtType *sysdbg_ctx,
			unsigned long addr, unsigned long stack)
{
	struct stackframe frame;
	int offset;
	struct sysdbg_cpu32_ctxt_regs *regs = &sysdbg_ctx->cpu_regs.cpu32_ctxt;

	if (!virt_is_valid(addr)) {
		MSMWDTD("%08lx is not valid kernel address.\n", addr);
		return;
	}
	if ((regs->r13_svc & ~(THREAD_SIZE - 1)) == addr) {
		frame.fp = (regs->r11 & (THREAD_SIZE - 1)) + stack;
		frame.sp = (regs->r13_svc & (THREAD_SIZE - 1)) + stack;
		frame.lr = regs->r14_svc;
		frame.pc = regs->pc;
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

#endif

static void msm_wdog_ctx_print(struct msm_wdog_cpuctx *ctx,
				phys_addr_t paddr, size_t ctx_size)
{
	struct msm_wdog_cpuctx *ctxi;
	struct msm_dump_data *cpu_data;
	struct msm_wdog_cpuctx_info *info;
	struct msm_wdog_cpuctx_stat *stat;
	phys_addr_t this_paddr;
	cpumask_t cpus, cpus_nodump, cpus_regs, cpus_dd;
	unsigned long stack_tmp = 0;
	int cpu;

	cpumask_clear(&cpus);
	for_each_cpu(cpu, cpu_present_mask) {
		ctxi = &ctx[cpu];
		cpu_data = &ctxi->cpu_data;
		if (msm_wdog_ctx_header_check(ctxi)) {
			MSMWDTD_IFWDOG("CPU%d: ctx header invalid\n", cpu);
			continue;
		}

		info = &ctxi->info;
		if ((info->sig != MSM_WDOG_CTX_SIG) ||
				(info->rev2 != MSM_WDOG_CTX_REV) ||
				(info->rev != MSM_WDOG_CTX_REV) ||
				(info->size != WDOG_CPUCTX_SIZE_PERCPU) ||
				(info->ret != ERR_NONE)) {
			MSMWDTD_IFWDOG("CPU%d: sig %x rev %x/%x sz %x ret %x\n",
					cpu, info->sig, (unsigned)info->rev,
					info->rev2, info->size, info->ret);
			continue;
		}

		this_paddr = paddr + (cpu * sizeof(*ctxi));
		if (cpu_data->addr != this_paddr) {
			/* No len checking, since cpu_data->len is hacked */
			MSMWDTD_IFWDOG("CPU%d: addr %llx len %llx ", cpu,
					cpu_data->addr, cpu_data->len);
			MSMWDTD_IFWDOG("expect %pa %zx\n", &this_paddr,
					sizeof(*ctxi));
			continue;
		}
		cpumask_set_cpu(cpu, &cpus);
	}

	if (cpumask_empty(&cpus))
		return;

	cpumask_clear(&cpus_nodump);
	cpumask_clear(&cpus_regs);
	cpumask_clear(&cpus_dd);
	for_each_cpu_mask(cpu, cpus) {
		uint32_t *status;

		ctxi = &ctx[cpu];
		cpu_data = &ctxi->cpu_data;
		stat = &ctxi->stat;

		if (!cpu_data->magic && !cpu_data->version &&
				!ctxi->sysdbg.data.status[0]) {
			MSMWDTD_IFWDOG("CPU%d: No Dump!\n", cpu);
			cpumask_set_cpu(cpu, &cpus_nodump);
			continue;
		}
		if (cpu_data->magic != DUMP_MAGIC_NUMBER) {
			MSMWDTD_IFWDOG("CPU%d: dump magic mismatch %x/%x\n",
				cpu, cpu_data->magic, DUMP_MAGIC_NUMBER);
			continue;
		}
		if (msm_wdog_cpu_regs_version_unknown(cpu_data->version)) {
			MSMWDTD_IFWDOG("CPU%d: unknown version %d\n",
				cpu, cpu_data->version);
			continue;
		}
		cpumask_set_cpu(cpu, &cpus_regs);
		status = &ctxi->sysdbg.data.status[0];
		MSMWDTD("CPU%d: %x %x ", cpu, status[0], status[1]);
		msm_wdog_show_sc_status(status[1]);
		if (stat->ret == ERR_NONE)
			cpumask_set_cpu(cpu, &cpus_dd);
	}

	if (cpumask_equal(&cpus_nodump, cpu_present_mask)) {
		MSMWDTD_IFWDOG("Might be Secure Watchdog Bite!\n");
		return;
	}
	if (cpumask_empty(&cpus_regs))
		return;
	MSMWDTD("\n");
	for_each_cpu_mask(cpu, cpus_regs) {
		struct msm_wdog_copy *job;

		ctxi = &ctx[cpu];
		stat = &ctxi->stat;
		MSMWDTD("CPU%d: ret %x", cpu, stat->ret);
		if (stat->stack_va) {
			MSMWDTD(" stack %lx ", (unsigned long)stat->stack_va);
			job = &stat->jobs[LNX_STACK];
			MSMWDTD("%lx -> %lx (%lx) ", (unsigned long)job->from,
					(unsigned long)job->to,
					(unsigned long)job->size);
			job = &stat->jobs[LNX_TASK];
			MSMWDTD("%lx -> %lx (%lx)", (unsigned long)job->from,
					(unsigned long)job->to,
					(unsigned long)job->size);
		}
		MSMWDTD("\n");
	}
	for_each_cpu_mask(cpu, cpus_regs) {
		ctxi = &ctx[cpu];
		MSMWDTD("\nCPU%d\n", cpu);
		msm_wdt_show_regs(&ctxi->sysdbg.data);
	}
	for_each_cpu_mask(cpu, cpus_dd) {
		unsigned long data;

		ctxi = &ctx[cpu];
		if (!IS_ALIGNED((unsigned long)ctxi->stack, THREAD_SIZE)
					&& !stack_tmp) {
			stack_tmp = __get_free_pages(GFP_KERNEL,
					THREAD_SIZE_ORDER);
			if (!stack_tmp)
				MSMWDT_ERR("Alloc temp stack failed.\n");
		}
		if (stack_tmp) {
			memcpy_fromio((void *)stack_tmp, ctxi->stack,
					THREAD_SIZE);
			data = stack_tmp;
		} else {
			data = (unsigned long)ctxi->stack;
		}

		MSMWDTD("\nCPU%d\n", cpu);
		msm_wdt_show_task(&ctxi->task,
					(struct thread_info *)ctxi->stack);
		msm_wdt_unwind(&ctxi->task, &ctxi->sysdbg.data,
					ctxi->stat.stack_va, data);
	}
	MSMWDTD("\n");
	if (stack_tmp)
		free_pages(stack_tmp, THREAD_SIZE_ORDER);
}


void msm_wdog_get_cpu_ctx(struct platform_device *pdev,
		phys_addr_t *cpu_ctx_addr, size_t *cpu_ctx_size_percpu)
{
	struct device_node *pnode;
	struct msm_wdog_cpuctx *ctx_vaddr;
	phys_addr_t ctx_paddr;
	size_t ctx_size;
	const __be32 *basep;
	u64 size;
	u64 base;

	pnode = of_parse_phandle(pdev->dev.of_node,
			"linux,contiguous-region", 0);
	if (!pnode) {
		MSMWDT_ERR("Unable to find contiguous-region\n");
		goto no_reservation;
	}
	basep = of_get_address(pnode, 0, &size, NULL);
	if (!basep) {
		of_node_put(pnode);
		MSMWDT_ERR("Addr not found for contiguous-region\n");
		goto no_reservation;
	} else {
		base = of_translate_address(pnode, basep);
	}

	of_node_put(pnode);

	ctx_paddr = (phys_addr_t)base;
	ctx_size = size;

	if (ctx_size < WDOG_CPUCTX_SIZE) {
		MSMWDT_ERR("Mem reserve too small %zx/%zx\n",
				ctx_size, WDOG_CPUCTX_SIZE);
		goto no_reservation;
	}

	ctx_size = WDOG_CPUCTX_SIZE;
	ctx_vaddr = dma_remap(&pdev->dev, NULL, ctx_paddr, ctx_size, NULL);
	if (ctx_vaddr) {
		msm_wdog_ctx_print(ctx_vaddr, ctx_paddr, ctx_size);
		msm_wdog_ctx_reset(ctx_vaddr, ctx_size);
		dma_unremap(&pdev->dev, ctx_vaddr, ctx_size);
	} else {
		MSMWDT_ERR("Cannot remap buffer %pa size %zx\n",
						&ctx_paddr, ctx_size);
	}
	*cpu_ctx_addr = ctx_paddr;
	*cpu_ctx_size_percpu = WDOG_CPUCTX_SIZE_PERCPU;
	return;

no_reservation:
	ctx_vaddr = kzalloc(WDOG_SYSDBG_SIZE, GFP_KERNEL);
	if (ctx_vaddr) {
		*cpu_ctx_addr = virt_to_phys(ctx_vaddr);
		*cpu_ctx_size_percpu = WDOG_SYSDBG_SIZE_PERCPU;
	} else {
		MSMWDT_ERR("Cannot alloc %x\n", WDOG_SYSDBG_SIZE);
		*cpu_ctx_addr = 0;
		*cpu_ctx_size_percpu = 0;
	}
	return;
}
