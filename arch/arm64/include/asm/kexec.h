/*
 * kexec for arm64
 *
 * Copyright (C) Linaro.
 * Copyright (C) Futurewei Technologies.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#if !defined(_ARM64_KEXEC_H)
#define _ARM64_KEXEC_H

/* Maximum physical address we can use pages from */

#define KEXEC_SOURCE_MEMORY_LIMIT (-1UL)

/* Maximum address we can reach in physical address mode */

#define KEXEC_DESTINATION_MEMORY_LIMIT (-1UL)

/* Maximum address we can use for the control code buffer */

#define KEXEC_CONTROL_MEMORY_LIMIT (-1UL)

#define KEXEC_CONTROL_PAGE_SIZE	4096

#define KEXEC_ARCH KEXEC_ARCH_ARM64

#if !defined(__ASSEMBLY__)

/**
 * crash_setup_regs() - save registers for the panic kernel
 *
 * @newregs: registers are saved here
 * @oldregs: registers to be saved (may be %NULL)
 */

static inline void crash_setup_regs(struct pt_regs *newregs,
				    struct pt_regs *oldregs)
{
	/* Empty routine needed to avoid build errors. */
}

#endif /* !defined(__ASSEMBLY__) */

#endif
