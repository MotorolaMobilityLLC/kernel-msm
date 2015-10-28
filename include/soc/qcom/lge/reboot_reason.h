/*
 * include/soc/qcom/lge/reboot_reason.h
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

#ifndef __REBOOT_REASON_H
#define __REBOOT_REASON_H

// Base BOOT REASONS - currently first 3 bits, reserve nibble
// Same as enum pon_restart_reason
#define	BOOT_UNKNOWN				0x00
#define	RECOVERY_MODE				0x01
#define	FASTBOOT_MODE				0x02
#define	ALARM_BOOT				0x03
#define	VERITY_BOOT				0x04
#define	BOOT_OTHER				0x05
#define	REBOOT_BATTERY                          0x06

// FOTA reboot reasons
#define	FOTA_REBOOT_REASON_LCD_OFF		0x10
#define	FOTA_REBOOT_REASON_LCD_OUT_OFF		0x11
#define	REBOOT_REASON_LCD_OFF			0x12
#define	FOTA_REBOOT				0x13

#define	LAF_DOWNLOAD_MODE			0x1E
// crash descriptive reasons
#define	LAF_CRASH_IN_LAF			0x1F
#define	UNDEFINED_CRITICAL_ERROR		0x20
#define	KERNEL_PANIC				0x21
#define	KERNEL_UNDEFINED_ERROR			0x22
#define	RPM_CRASH				0x23
#define	RPM_WATCHDOG_BITE			0x24
#define	RPM_RESET				0x25
#define	RPM_UNDEFINED_ERROR			0x26
#define	TZ_UNKNOWN_RESET			0x27
#define	TZ_WATCHDOG_APPS_BITE			0x28
#define	TZ_CRASH				0x29
#define	TZ_WATCHDOG_APPS_BARK			0x2A
#define	TZ_AHB_TIMEOUT				0x2B
#define	TZ_OCMEM_NOC_ERROR			0x2C
#define	TZ_MM_NOC_ERROR				0x2D
#define	TZ_PERIPHERAL_NOC_ERROR			0x2E
#define	TZ_SYS_NOC_ERROR			0x2F
#define	TZ_CONF_NOC_ERROR			0x30
#define	TZ_XPU_ERROR				0x31
#define	TZ_THERMAL_BITE_RESET			0x32
#define	TZ_UNDEFINED_ERROR			0x33
#define	SDI_WATCHDOG_SDI_APPS_RESET		0x34
#define	SDI_FATAL_ERROR				0x35
#define	SDI_UNDEFINED_ERROR			0x36
#define	LK_CRASH_IN_LK				0x37

// sub error type defines for subsys
#define	ETYPE_UNDEFINED_ERROR			0x00
#define	ETYPE_FAILED_SHUTDOWN			0x01
#define	ETYPE_RESETTING_SOC			0x02
#define	ETYPE_UNKNOWN_RESTART_LEVEL		0x03
#define	ETYPE_FAILED_POWERUP			0x04
#define	ETYPE_WAIT_TIMEOUT			0x05
#define	ETYPE_SSR_CRASH				0x06
#define	ETYPE_LIMIT_OVERFLOW_CRASH		0x07

// Subsystems range
#define	SUBSYSTEM_APPS				0x38
#define	SUBSYSTEM_ADSP				0x40
#define	SUBSYSTEM_MBA				0x48
#define	SUBSYSTEM_MODEM				0x50
#define	SUBSYSTEM_WCNSS				0x58
#define	SUBSYSTEM_VENUS				0x60
#define	SUBSYSTEM_AR6320			0x68

#define	REBOOT_REASONS_MAX			0x7F
#define	REBOOT_REASONS_MASK			REBOOT_REASONS_MAX

/* LGE reboot reason for crash handler */
#define	LGE_RB_MAGIC				0x6D630000

#define	LGE_ERR_KERN				0x0100
#define	LGE_ERR_RPM				0x0200
#define	LGE_ERR_TZ				0x0300
#define	LGE_ERR_SDI				0x0400
#define	LGE_ERR_LAF				0x0500
#define	LGE_ERR_LK				0x0600

#define	LGE_SUB_ADSP				0x1000
#define	LGE_SUB_MBA				0x2000
#define	LGE_SUB_MODEM				0x3000
#define	LGE_SUB_WCNSS				0x4000
#define	LGE_SUB_VENUS				0x5000
#define	LGE_SUB_AR6320				0x6000

#define	LGE_ERR_SUB_SD				ETYPE_FAILED_SHUTDOWN
#define	LGE_ERR_SUB_RST				ETYPE_RESETTING_SOC
#define	LGE_ERR_SUB_UNK				ETYPE_UNKNOWN_RESTART_LEVEL
#define	LGE_ERR_SUB_PWR				ETYPE_FAILED_POWERUP
#define	LGE_ERR_SUB_TOW				ETYPE_WAIT_TIMEOUT
#define	LGE_ERR_SUB_CDS				ETYPE_SSR_CRASH
#define	LGE_ERR_SUB_CLO				ETYPE_LIMIT_OVERFLOW_CRASH

#define	LGE_ERR_RPM_ERR				0x0000
#define	LGE_ERR_RPM_WDT				0x0001
#define	LGE_ERR_RPM_RST				0x0002

#define	LGE_ERR_TZ_SEC_WDT			0x0000
#define	LGE_ERR_TZ_NON_SEC_WDT			0x0001
#define	LGE_ERR_TZ_ERR				0x0002
#define	LGE_ERR_TZ_WDT_BARK			0x0003
#define	LGE_ERR_TZ_AHB_TIMEOUT			0x0004
#define	LGE_ERR_TZ_OCMEM_NOC_ERR		0x0005
#define	LGE_ERR_TZ_MM_NOC_ERR			0x0006
#define	LGE_ERR_TZ_PERIPH_NOC_ERR		0x0007
#define	LGE_ERR_TZ_SYS_NOC_ERR			0x0008
#define	LGE_ERR_TZ_CONF_NOC_ERR			0x0009
#define	LGE_ERR_TZ_XPU_ERR			0x000A
#define	LGE_ERR_TZ_THERM_SEC_BITE		0x000B

#define	LGE_ERR_SDI_SEC_WDT			0x0000
#define	LGE_ERR_SDI_ERR_FATAL			0x0001

#define	LAF_DLOAD_MODE				0x6C616664 /* lafd */

#endif /* __REBOOT_REASON_H */
