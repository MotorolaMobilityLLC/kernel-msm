/*
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_MMU_H
#define __ASM_MMU_H

#include <linux/smp.h>

#include <asm/cpufeature.h>
#include <asm/percpu.h>

typedef struct {
	unsigned int id;
	raw_spinlock_t id_lock;
	void *vdso;
} mm_context_t;

#define INIT_MM_CONTEXT(name) \
	.context.id_lock = __RAW_SPIN_LOCK_UNLOCKED(name.context.id_lock),

#define ASID(mm)	((mm)->context.id & 0xffff)

#define INIT_MM_CONTEXT(name)	\
	.context.id_lock    = __RAW_SPIN_LOCK_UNLOCKED(name.context.id_lock),

typedef void (*bp_hardening_cb_t)(void);

struct bp_hardening_data {
	bp_hardening_cb_t	fn;
};

#ifdef CONFIG_HARDEN_BRANCH_PREDICTOR

DECLARE_PER_CPU_READ_MOSTLY(struct bp_hardening_data, bp_hardening_data);

static inline struct bp_hardening_data *arm64_get_bp_hardening_data(void)
{
	return this_cpu_ptr(&bp_hardening_data);
}

static inline void arm64_apply_bp_hardening(void)
{
	struct bp_hardening_data *d;

	d = arm64_get_bp_hardening_data();
	if (d->fn)
		d->fn();
}
#else
static inline struct bp_hardening_data *arm64_get_bp_hardening_data(void)
{
	return NULL;
}

static inline void arm64_apply_bp_hardening(void)	{ }
#endif	/* CONFIG_HARDEN_BRANCH_PREDICTOR */

extern void paging_init(void);
extern void setup_mm_for_reboot(void);
extern void __iomem *early_io_map(phys_addr_t phys, unsigned long virt);
extern void mem_text_write_kernel_word(u32 *addr, u32 word);
extern void init_mem_pgprot(void);
/* create an identity mapping for memory (or io if map_io is true) */
extern void create_id_mapping(phys_addr_t addr, phys_addr_t size, int map_io);

#endif
