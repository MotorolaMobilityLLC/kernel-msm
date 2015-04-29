/*
 * include/soc/qcom/lge/lge_handle_panic.h
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

#ifndef __MACH_LGE_HANDLE_PANIC_H
#define __MACH_LGE_HANDLE_PANIC_H

/* LGE reboot reason for crash handler */
#define LGE_RB_MAGIC    0x6D630000

#define LGE_ERR_KERN              0x0100
#define LGE_ERR_RPM               0x0200
#define LGE_ERR_TZ                0x0300
#define LGE_ERR_SDI               0x0400
#define LGE_ERR_LAF               0x0500
#define LGE_ERR_LK                0x0600

#define LGE_SUB_ADSP              0x1000
#define LGE_SUB_MBA               0x2000
#define LGE_SUB_MODEM             0x3000
#define LGE_SUB_WCNSS             0x4000
#define LGE_SUB_VENUS             0x5000
#define LGE_SUB_AR6320            0x6000

#define LGE_ERR_SUB_SD            0x0001
#define LGE_ERR_SUB_RST           0x0002
#define LGE_ERR_SUB_UNK           0x0003
#define LGE_ERR_SUB_PWR           0x0004
#define LGE_ERR_SUB_TOW           0x0005
#define LGE_ERR_SUB_CDS           0x0006
#define LGE_ERR_SUB_CLO           0x0007

#define LGE_ERR_RPM_ERR           0x0000
#define LGE_ERR_RPM_WDT           0x0001
#define LGE_ERR_RPM_RST           0x0002

#define LGE_ERR_TZ_SEC_WDT        0x0000
#define LGE_ERR_TZ_NON_SEC_WDT    0x0001
#define LGE_ERR_TZ_ERR            0x0002
#define LGE_ERR_TZ_WDT_BARK       0x0003
#define LGE_ERR_TZ_AHB_TIMEOUT    0x0004
#define LGE_ERR_TZ_OCMEM_NOC_ERR  0x0005
#define LGE_ERR_TZ_MM_NOC_ERR     0x0006
#define LGE_ERR_TZ_PERIPH_NOC_ERR 0x0007
#define LGE_ERR_TZ_SYS_NOC_ERR    0x0008
#define LGE_ERR_TZ_CONF_NOC_ERR   0x0009
#define LGE_ERR_TZ_XPU_ERR        0x000A
#define LGE_ERR_TZ_THERM_SEC_BITE 0x000B

#define LGE_ERR_SDI_SEC_WDT       0x0000
#define LGE_ERR_SDI_ERR_FATAL     0x0001

#define LAF_DLOAD_MODE   0x6C616664 /* lafd */

void lge_set_subsys_crash_reason(const char *name, int type);
void lge_set_ram_console_addr(unsigned int addr, unsigned int size);
void lge_set_panic_reason(void);
void lge_set_fb_addr(unsigned int addr);
void lge_set_restart_reason(unsigned int);
void lge_disable_watchdog(void);
void lge_panic_handler_fb_cleanup(void);

struct panic_handler_data {
	unsigned long	fb_addr;
	unsigned long	fb_size;
};

#endif
