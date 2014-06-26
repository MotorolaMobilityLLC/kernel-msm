/*
 * intel_soc_mrfld.h
 * Copyright (c) 2012, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifdef CONFIG_REMOVEME_INTEL_ATOM_MRFLD_POWER

#define PM_SUPPORT		0x21

#define ISP_POS			7
#define ISP_SUB_CLASS		0x80

#define PUNIT_PORT		0x04
#define SSS_SHIFT		24

/* Soft reset mask */
#define SR_MASK			0x2

#define PMU1_MAX_DEVS			8
#define PMU2_MAX_DEVS			55

#define MRFLD_S3_HINT			0x64

#define PUNIT_PORT			0x04
#define NC_PM_SSS			0x3F

/* SRAM locations to get S0ix count and residency */
#define S0IX_COUNTERS_BASE	0xFFFFF500
#define S0IX_COUNTERS_SIZE	(0x608 - 0x500)

/* IPC commands to start, stop and
 * dump S0ix residency counters */
#define START_RES_COUNTER	0x00EB
#define STOP_RES_COUNTER	0x10EB
#define DUMP_RES_COUNTER	0x20EB

/* IPC commands to start/reset and
 * dump S0ix count */
#define START_S0IX_COUNT	0x00E1
#define DUMP_S0IX_COUNT		0x10E1

#define GFX_LSS_INDEX			1

#define PMU_PSH_LSS_00			0
#define PMU_SDIO0_LSS_01		1
#define PMU_EMMC0_LSS_02		2
#define PMU_RESERVED_LSS_03		3
#define PMU_SDIO1_LSS_04		4
#define PMU_HSI_LSS_05			5
#define PMU_SECURITY_LSS_06		6
#define PMU_RESERVED_LSS_07		7
#define PMU_USB_MPH_LSS_08		8
#define PMU_USB3_LSS_09			9
#define PMU_AUDIO_LSS_10		10
#define PMU_RESERVED_LSS_11		11
#define PMU_RESERVED_LSS_12		12
#define PMU_RESERVED_LSS_13		13
#define PMU_RESERVED_LSS_14		14
#define PMU_RESERVED_LSS_15		15
#define PMU_RESERVED_LSS_16		16
#define PMU_SSP3_LSS_17			17
#define PMU_SSP5_LSS_18			18
#define PMU_SSP6_LSS_19			19
#define PMU_I2C1_LSS_20			20
#define PMU_I2C2_LSS_21			21
#define PMU_I2C3_LSS_22			22
#define PMU_I2C4_LSS_23			23
#define PMU_I2C5_LSS_24			24
#define PMU_GP_DMA_LSS_25		25
#define PMU_I2C6_LSS_26			26
#define PMU_I2C7_LSS_27			27
#define PMU_USB_OTG_LSS_28		28
#define PMU_RESERVED_LSS_29		29
#define PMU_RESERVED_LSS_30		30
#define PMU_UART0_LSS_31		31
#define PMU_UART1_LSS_31		31
#define PMU_UART2_LSS_31		31

#define PMU_I2C8_LSS_33			33
#define PMU_I2C9_LSS_34			34
#define PMU_SSP4_LSS_35			35
#define PMU_PMW_LSS_36			36

#define EMMC0_LSS			PMU_EMMC0_LSS_02

#define IGNORE_SSS0			0
#define IGNORE_SSS1			0
#define IGNORE_SSS2			0
#define IGNORE_SSS3			0

#define PMU_WAKE_GPIO0      (1 << 0)
#define PMU_WAKE_GPIO1      (1 << 1)
#define PMU_WAKE_GPIO2      (1 << 2)
#define PMU_WAKE_GPIO3      (1 << 3)
#define PMU_WAKE_GPIO4      (1 << 4)
#define PMU_WAKE_GPIO5      (1 << 5)
#define PMU_WAKE_TIMERS     (1 << 6)
#define PMU_WAKE_SECURITY   (1 << 7)
#define PMU_WAKE_AONT32K    (1 << 8)
#define PMU_WAKE_AONT       (1 << 9)
#define PMU_WAKE_SVID_ALERT (1 << 10)
#define PMU_WAKE_AUDIO      (1 << 11)
#define PMU_WAKE_USB2       (1 << 12)
#define PMU_WAKE_USB3       (1 << 13)
#define PMU_WAKE_ILB        (1 << 14)
#define PMU_WAKE_TAP        (1 << 15)
#define PMU_WAKE_WATCHDOG   (1 << 16)
#define PMU_WAKE_HSIC       (1 << 17)
#define PMU_WAKE_PSH        (1 << 18)
#define PMU_WAKE_PSH_GPIO   (1 << 19)
#define PMU_WAKE_PSH_AONT   (1 << 20)
#define PMU_WAKE_PSH_HALT   (1 << 21)
#define PMU_GLBL_WAKE_MASK  (1 << 31)

/* Ignore AONT WAKES and ALL from WKC1 */
#define IGNORE_S3_WKC0 (PMU_WAKE_AONT32K | PMU_WAKE_AONT)
#define IGNORE_S3_WKC1 (~0)

#define S0IX_TARGET_SSS0_MASK (0xFFF3FFFF)
#define S0IX_TARGET_SSS1_MASK (0xFFFFFFFF)
#define S0IX_TARGET_SSS2_MASK (0xFFFFFFFF)
#define S0IX_TARGET_SSS3_MASK (0xFFFFFFFF)

#define S0IX_TARGET_SSS0 (0xFFF3FFFF)
#define S0IX_TARGET_SSS1 (0xFFFFFFFF)
#define S0IX_TARGET_SSS2 (0xFFFFFF3F)
#define S0IX_TARGET_SSS3 (0xFFFFFFFF)

#define LPMP3_TARGET_SSS0_MASK (0xFFF3FFFF)
#define LPMP3_TARGET_SSS0 (0xFFC3FFFF)

extern char *mrfl_nc_devices[];
extern int mrfl_no_of_nc_devices;
extern int intel_scu_ipc_simple_command(int, int);
extern void log_wakeup_irq(void);
extern void s0ix_complete(void);
extern bool could_do_s0ix(void);

#endif
