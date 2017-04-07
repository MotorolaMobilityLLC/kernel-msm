/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _MC_PLATFORM_H_
#define _MC_PLATFORM_H_

/* MobiCore Interrupt for Qualcomm (DT IRQ has priority if present) */
#define MC_INTR_SSIQ	280

/* Use SMC for fastcalls */
#define MC_SMC_FASTCALL

#include <linux/types.h>

/*--------------- Implementation -------------- */
#include <soc/qcom/scm.h>
#include <soc/qcom/qseecomi.h>

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <asm/cacheflush.h>
#include <linux/errno.h>

#define SCM_MOBIOS_FNID(s, c) (((((s) & 0xFF) << 8) | ((c) & 0xFF)) \
		| 0x33000000)

#define TZ_EXECUTIVE_EXT_ID_PARAM_ID \
	TZ_SYSCALL_CREATE_PARAM_ID_4( \
			TZ_SYSCALL_PARAM_TYPE_BUF_RW, \
			TZ_SYSCALL_PARAM_TYPE_VAL, \
			TZ_SYSCALL_PARAM_TYPE_BUF_RW, \
			TZ_SYSCALL_PARAM_TYPE_VAL)

/* from following file */
#define SCM_SVC_MOBICORE		250
#define SCM_CMD_MOBICORE		1

static inline int smc_fastcall(void *fc_generic, size_t size)
{
	if (is_scm_armv8()) {
		struct scm_desc desc = {0};
		int ret;
		void *scm_buf = NULL;

		scm_buf = kzalloc(PAGE_ALIGN(size), GFP_KERNEL);
		if (!scm_buf)
			return -ENOMEM;
		memcpy(scm_buf, fc_generic, size);
		dmac_flush_range(scm_buf, scm_buf + size);

		desc.arginfo = TZ_EXECUTIVE_EXT_ID_PARAM_ID;
		desc.args[0] = virt_to_phys(scm_buf);
		desc.args[1] = (u32)size;
		desc.args[2] = virt_to_phys(scm_buf);
		desc.args[3] = (u32)size;

		ret = scm_call2(
			SCM_MOBIOS_FNID(SCM_SVC_MOBICORE, SCM_CMD_MOBICORE),
				&desc);

		dmac_flush_range(scm_buf, scm_buf + size);

		memcpy(fc_generic, scm_buf, size);
		kfree(scm_buf);
		return ret;
	}
	return scm_call(SCM_SVC_MOBICORE, SCM_CMD_MOBICORE,
			fc_generic, size,
			fc_generic, size);
}

/* Should be defined for all TZBSPv4 platform common flag */
/* DHV */
#ifndef CONFIG_TRUSTONIC_TEE_LPAE
#define CONFIG_TRUSTONIC_TEE_LPAE
#endif

/*
 * Perform crypto clock enable/disable
 * of clocks
 *     "bus_clk"
 *     "core_clk"
 *     "iface_clk"
 */
#define MC_CRYPTO_CLOCK_MANAGEMENT
#define MC_CRYPTO_CLOCK_CORESRC_PROPNAME "qcom,ce-opp-freq"
#define MC_CLOCK_CORESRC_DEFAULTRATE 100000000

/*
 * Perform clock enable/disable for clock  "core_clk_src"
 */
#define MC_DEVICE_PROPNAME "qcom,mcd"

/* All TZBSPv4 targets are using AARCH32_FC flag */
#define MC_AARCH32_FC

/*
 * On Kernel >= 4.4 debugfs_create_bool API changed
 * and this flag should be defined
 * define DEBUGFS_CREATE_BOOL_TAKES_A_BOOL
 */
#define DEBUGFS_CREATE_BOOL_TAKES_A_BOOL

#endif /* _MC_PLATFORM_H_ */
