/*
 * intel_soc_pm_debug.h
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
#ifndef _INTEL_SOC_PM_DEBUG_H
#define _INTEL_SOC_PM_DEBUG_H
#include <linux/intel_mid_pm.h>

#include "intel_soc_pmu.h"


#define NANO_SEC		1000000000UL /* 10^9 in sec */
#define MICRO_SEC		1000000UL /* 10^6 in sec */
#define PMU_LOG_INTERVAL_SECS	(60*5) /* 5 mins in secs */

#define S0IX_LAT_SRAM_ADDR_CLVP		0xFFFF7FD0
#define S0IX_LAT_SRAM_SIZE_CLVP		8

#define IPC_CMD_S0IX_LATENCY_CLVP	0xCE
#define IPC_SUB_MEASURE_START_CLVP	0x00
#define IPC_SUB_MEASURE_STOP_CLVP	0x01

struct simple_stat {
	u64 min;
	u64 max;
	u64 total;
	u64 curr;
};

struct entry_exit_stat {
	struct simple_stat entry;
	struct simple_stat exit;
};

struct latency_stat {
	struct entry_exit_stat scu_latency[SYS_STATE_MAX];
	struct entry_exit_stat os_latency[SYS_STATE_MAX];
	struct simple_stat s3_parts_lat[MAX_S3_PARTS];
	u64 count[SYS_STATE_MAX];
	u32 __iomem *scu_s0ix_lat_addr;
	struct dentry *dentry;
	bool latency_measure;
};

struct island {
	int type;
	int index;
	char *name;
};

struct lss_definition {
	char *lss_name;
	char *block;
	char *subsystem;
};

#ifdef CONFIG_REMOVEME_INTEL_ATOM_MRFLD_POWER
#define PUNIT_CR_CORE_C1_RES_MSR	0x660
#define PUNIT_CR_CORE_C4_RES_MSR	0x3fc
#define PUNIT_CR_CORE_C6_RES_MSR	0x3fd

#define NUM_CSTATES_RES_MEASURE		3

extern unsigned int enable_s3;
extern unsigned int enable_s0ix;

extern u8 __iomem *s0ix_counters;
extern int s0ix_counter_reg_map[];
extern int s0ix_residency_reg_map[];

#endif

/* platform dependency starts */
#ifdef CONFIG_REMOVEME_INTEL_ATOM_MDFLD_POWER

#define DEV_GFX		2
#define FUNC_GFX	0
#define ISLANDS_GFX	8
#define DEV_ISP		3
#define FUNC_ISP	0
#define ISLANDS_ISP	2
#define NC_DEVS		2

static struct lss_definition lsses[] = {
	{"Lss00", "Storage", "SDIO0 (HC2)"},
	{"Lss01", "Storage", "eMMC0 (HC0a)"},
	{"NA", "Storage", "ND_CTL (Note 5)"},
	{"Lss03", "H S I", "H S I DMA"},
	{"Lss04", "Security", "RNG"},
	{"Lss05", "Storage", "eMMC1 (HC0b)"},
	{"Lss06", "USB", "USB OTG (ULPI)"},
	{"Lss07", "USB", "USB_SPH"},
	{"Lss08", "Audio", ""},
	{"Lss09", "Audio", ""},
	{"Lss10", "SRAM", " SRAM CTL+SRAM_16KB"},
	{"Lss11", "SRAM", " SRAM CTL+SRAM_16KB"},
	{"Lss12", "SRAM", "SRAM BANK (16KB+3x32KBKB)"},
	{"Lss13", "SRAM", "SRAM BANK(4x32KB)"},
	{"Lss14", "SDIO COMMS", "SDIO2 (HC1b)"},
	{"Lss15", "PTI, DAFCA", " DFX Blocks"},
	{"Lss16", "SC", " DMA"},
	{"NA", "SC", "SPI0/MSIC"},
	{"Lss18", "GP", "SPI1"},
	{"Lss19", "GP", " SPI2"},
	{"Lss20", "GP", " I2C0"},
	{"Lss21", "GP", " I2C1"},
	{"NA", "Fabrics", " Main Fabric"},
	{"NA", "Fabrics", " Secondary Fabric"},
	{"NA", "SC", "SC Fabric"},
	{"Lss25", "Audio", " I-RAM BANK1 (32 + 256KB)"},
	{"NA", "SCU", " ROM BANK1 (18KB+18KB+18KB)"},
	{"Lss27", "GP", "I2C2"},
	{"NA", "SSC", "SSC (serial bus controller to FLIS)"},
	{"Lss29", "Security", "Chaabi AON Registers"},
	{"Lss30", "SDIO COMMS", "SDIO1 (HC1a)"},
	{"NA", "SCU", "I-RAM BANK0 (32KB)"},
	{"NA", "SCU", "I-RAM BANK1 (32KB)"},
	{"Lss33", "GP", "I2C3 (HDMI)"},
	{"Lss34", "GP", "I2C4"},
	{"Lss35", "GP", "I2C5"},
	{"Lss36", "GP", "SSP (SPI3)"},
	{"Lss37", "GP", "GPIO1"},
	{"NA", "GP", "GP Fabric"},
	{"Lss39", "SC", "GPIO0"},
	{"Lss40", "SC", "KBD"},
	{"Lss41", "SC", "UART2:0"},
	{"NA", "NA", "NA"},
	{"NA", "NA", "NA"},
	{"Lss44", "Security", " Security TAPC"},
	{"NA", "MISC", "AON Timers"},
	{"NA", "PLL", "LFHPLL and Spread Spectrum"},
	{"NA", "PLL", "USB PLL"},
	{"NA", "NA", "NA"},
	{"NA", "Audio", "SLIMBUS CTL 1 (note 5)"},
	{"NA", "Audio", "SLIMBUS CTL 2 (note 5)"},
	{"Lss51", "Audio", "SSP0"},
	{"Lss52", "Audio", "SSP1"},
	{"NA", "Bridge", "IOSF to OCP Bridge"},
	{"Lss54", "GP", "DMA"},
	{"NA", "SC", "SVID (Serial Voltage ID)"},
	{"NA", "SOC Fuse", "SoC Fuse Block (note 3)"},
	{"NA", "NA", "NA"},
};
#endif


#ifdef CONFIG_REMOVEME_INTEL_ATOM_CLV_POWER

#define DEV_GFX		2
#define FUNC_GFX	0
#define ISLANDS_GFX	8
#define DEV_ISP		3
#define FUNC_ISP	0
#define ISLANDS_ISP	2
#define NC_DEVS		2

static struct lss_definition lsses[] = {
	{"Lss00", "Storage", "SDIO0 (HC2)"},
	{"Lss01", "Storage", "eMMC0 (HC0a)"},
	{"NA", "Timer", "AONT"},
	{"Lss03", "H S I", "H S I DMA"},
	{"Lss04", "Security", "RNG"},
	{"Lss05", "Storage", "eMMC1 (HC0b)"},
	{"Lss06", "USB", "USB OTG (ULPI)"},
	{"Lss07", "USB", "USB_SPH"},
	{"Lss08", "Audio", "Audio ENGINE"},
	{"Lss09", "Audio", "Audio DMA"},
	{"Lss10", "SRAM", " SRAM CTL+SRAM_16KB"},
	{"Lss11", "SRAM", " SRAM CTL+SRAM_16KB"},
	{"Lss12", "SRAM", "SRAM BANK (16KB+3x32KBKB)"},
	{"Lss13", "SRAM", "SRAM BANK(4x32KB)"},
	{"Lss14", "SDIO COMMS", "SDIO2 (HC1b)"},
	{"Lss15", "PTI, DAFCA", " DFX Blocks"},
	{"Lss16", "SC", " DMA"},
	{"NA", "SC", "SPI0/MSIC"},
	{"Lss18", "GP", "SPI1"},
	{"Lss19", "GP", " SPI2"},
	{"Lss20", "GP", " I2C0"},
	{"Lss21", "GP", " I2C1"},
	{"NA", "Timer", "HPET"},
	{"NA", "Timer", "External Timer"},
	{"NA", "SC", "SC Fabric"},
	{"Lss25", "Audio", " I-RAM BANK1 (32 + 256KB)"},
	{"NA", "SCU", " ROM BANK1 (18KB+18KB+18KB)"},
	{"Lss27", "GP", "I2C2"},
	{"NA", "SSC", "SSC (serial bus controller to FLIS)"},
	{"Lss29", "Security", "Chaabi AON Registers"},
	{"Lss30", "SDIO COMMS", "SDIO1 (HC1a)"},
	{"NA", "Timer", "vRTC"},
	{"NA", "Security", "Security Timer"},
	{"Lss33", "GP", "I2C3 (HDMI)"},
	{"Lss34", "GP", "I2C4"},
	{"Lss35", "GP", "I2C5"},
	{"Lss36", "GP", "SSP (SPI3)"},
	{"Lss37", "GP", "GPIO1"},
	{"NA", "MSIC", "Power Button"},
	{"Lss39", "SC", "GPIO0"},
	{"Lss40", "SC", "KBD"},
	{"Lss41", "SC", "UART2:0"},
	{"NA", "MSIC", "ADC"},
	{"NA", "MSIC", "Charger"},
	{"Lss44", "Security", " Security TAPC"},
	{"NA", "MSIC", "AON Timers"},
	{"NA", "MSIC", "GPI"},
	{"NA", "MSIC", "BCU"},
	{"NA", "NA", "SSP2"},
	{"NA", "Audio", "SLIMBUS CTL 1 (note 5)"},
	{"NA", "Audio", "SLIMBUS CTL 2 (note 5)"},
	{"Lss51", "Audio", "SSP0"},
	{"Lss52", "Audio", "SSP1"},
	{"NA", "Bridge", "IOSF to OCP Bridge"},
	{"Lss54", "GP", "DMA"},
	{"NA", "MSIC", "RESET"},
	{"NA", "SOC Fuse", "SoC Fuse Block (note 3)"},
	{"NA", "NA", "NA"},
	{"Lss58", "NA", "SSP4"},
};
#endif
/* platform dependency ends */

#endif
