/*
 * intel-mid.h: Intel MID specific setup code
 *
 * (C) Copyright 2009 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _ASM_X86_INTEL_MID_H
#define _ASM_X86_INTEL_MID_H

#include <linux/types.h>
#include <linux/init.h>
#include <linux/sfi.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <asm/spid.h>
#include <asm/intel_mid_pcihelpers.h>

#ifdef CONFIG_SFI
extern int get_gpio_by_name(const char *name);
extern void install_irq_resource(struct platform_device *pdev, int irq);
#else
static inline int get_gpio_by_name(const char *name) { return -ENODEV; }
/* Dummy function to prevent compilation error in byt */
static inline void install_irq_resource(struct platform_device *pdev, int irq)
{};
#endif

extern int intel_mid_pci_init(void);
extern void *get_oem0_table(void);
extern void intel_delayed_device_register(void *dev,
			void (*delayed_callback)(void *dev_desc));
extern void intel_scu_device_register(struct platform_device *pdev);
extern struct devs_id *get_device_id(u8 type, char *name);
extern int __init sfi_parse_mrtc(struct sfi_table_header *table);
extern int __init sfi_parse_mtmr(struct sfi_table_header *table);
extern int sfi_mrtc_num;
extern struct sfi_rtc_table_entry sfi_mrtc_array[];
extern void *get_oem0_table(void);
extern void register_rpmsg_service(char *name, int id, u32 addr);
extern int sdhci_pci_request_regulators(void);
extern unsigned int sfi_get_watchdog_irq(void);

/* OEMB table */
struct sfi_table_oemb {
	struct sfi_table_header header;
	u32 board_id;
	u32 board_fab;
	u8 iafw_major_version;
	u8 iafw_main_version;
	u8 val_hooks_major_version;
	u8 val_hooks_minor_version;
	u8 ia_suppfw_major_version;
	u8 ia_suppfw_minor_version;
	u8 scu_runtime_major_version;
	u8 scu_runtime_minor_version;
	u8 ifwi_major_version;
	u8 ifwi_minor_version;
	struct soft_platform_id spid;
	u8 ssn[INTEL_PLATFORM_SSN_SIZE];
} __packed;

/*
 * Here defines the array of devices platform data that IAFW would export
 * through SFI "DEVS" table, we use name and type to match the device and
 * its platform data.
 */
struct devs_id {
	char name[SFI_NAME_LEN + 1];
	u8 type;
	u8 delay;
	void *(*get_platform_data)(void *info);
	void (*device_handler)(struct sfi_device_table_entry *pentry,
				struct devs_id *dev);
	/* Custom handler for devices */
	u8 trash_itp;/* true if this driver uses pin muxed with XDB connector */
};

#define SD_NAME_SIZE 16
/**
 * struct sd_board_info - template for device creation
 * @name: Initializes sdio_device.name; identifies the driver.
 * @bus_num: board-specific identifier for a given SDIO controller.
 * @board_ref_clock: Initializes sd_device.board_ref_clock;
 * @platform_data: Initializes sd_device.platform_data; the particular
 *      data stored there is driver-specific.
 *
 */
struct sd_board_info {
	char            name[SD_NAME_SIZE];
	int             bus_num;
	unsigned short  addr;
	u32             board_ref_clock;
	void            *platform_data;
};


/*
 * Medfield is the follow-up of Moorestown, it combines two chip solution into
 * one. Other than that it also added always-on and constant tsc and lapic
 * timers. Medfield is the platform name, and the chip name is called Penwell
 * we treat Medfield/Penwell as a variant of Moorestown. Penwell can be
 * identified via MSRs.
 */
enum intel_mid_cpu_type {
	INTEL_CPU_CHIP_NOTMID = 0,
	/* 1 was Moorestown */
	INTEL_MID_CPU_CHIP_PENWELL = 2,
	INTEL_MID_CPU_CHIP_CLOVERVIEW,
	INTEL_MID_CPU_CHIP_TANGIER,
	INTEL_MID_CPU_CHIP_VALLEYVIEW2,
	INTEL_MID_CPU_CHIP_ANNIEDALE,
};

extern enum intel_mid_cpu_type __intel_mid_cpu_chip;

/**
 * struct intel_mid_ops - Interface between intel-mid & sub archs
 * @arch_setup: arch_setup function to re-initialize platform
 *             structures (x86_init, x86_platform_init)
 *
 * This structure can be extended if any new interface is required
 * between intel-mid & its sub arch files.
 */
struct intel_mid_ops {
	void (*arch_setup)(void);
};

/* Helper API's for INTEL_MID_OPS_INIT */
#define DECLARE_INTEL_MID_OPS_INIT(cpuname, cpuid)[cpuid] = \
		get_##cpuname##_ops,

/* Maximum number of CPU ops */
#define MAX_CPU_OPS(a) (sizeof(a)/sizeof(void *))

/*
 * For every new cpu addition, a weak get_<cpuname>_ops() function needs be
 * declared in arch/x86/platform/intel_mid/intel_mid_weak_decls.h.
 */
#define INTEL_MID_OPS_INIT {\
	DECLARE_INTEL_MID_OPS_INIT(penwell, INTEL_MID_CPU_CHIP_PENWELL) \
	DECLARE_INTEL_MID_OPS_INIT(cloverview, INTEL_MID_CPU_CHIP_CLOVERVIEW) \
	DECLARE_INTEL_MID_OPS_INIT(tangier, INTEL_MID_CPU_CHIP_TANGIER) \
	DECLARE_INTEL_MID_OPS_INIT(anniedale, INTEL_MID_CPU_CHIP_ANNIEDALE) \
};

static inline enum intel_mid_cpu_type intel_mid_identify_cpu(void)
{
#ifdef CONFIG_X86_INTEL_MID
	return __intel_mid_cpu_chip;
#else
	return INTEL_CPU_CHIP_NOTMID;
#endif
}

enum intel_mid_timer_options {
	INTEL_MID_TIMER_DEFAULT,
	INTEL_MID_TIMER_APBT_ONLY,
	INTEL_MID_TIMER_LAPIC_APBT,
};

extern enum intel_mid_timer_options intel_mid_timer_options;

/*
 * Penwell uses spread spectrum clock, so the freq number is not exactly
 * the same as reported by MSR based on SDM.
 */
#define FSB_FREQ_83SKU	83200
#define FSB_FREQ_100SKU	99840
#define FSB_FREQ_133SKU	133120

#define FSB_FREQ_167SKU	167000
#define FSB_FREQ_200SKU	200000
#define FSB_FREQ_267SKU	267000
#define FSB_FREQ_333SKU	333000
#define FSB_FREQ_400SKU	400000

/* Bus Select SoC Fuse value */
#define BSEL_SOC_FUSE_MASK	0x7
#define BSEL_SOC_FUSE_001	0x1 /* FSB 133MHz */
#define BSEL_SOC_FUSE_101	0x5 /* FSB 100MHz */
#define BSEL_SOC_FUSE_111	0x7 /* FSB 83MHz */

#define SFI_MTMR_MAX_NUM 8
#define SFI_MRTC_MAX	8

extern struct console early_mrst_console;
extern void mrst_early_console_init(void);

extern struct console early_mrfld_console;
extern void mrfld_early_console_init(void);

extern struct console early_hsu_console;
extern void hsu_early_console_init(const char *);

extern struct console early_pti_console;

extern void intel_scu_devices_create(void);
extern void intel_scu_devices_destroy(void);
extern void intel_psh_devices_create(void);
extern void intel_psh_devices_destroy(void);

/* VRTC timer */
#define MRST_VRTC_MAP_SZ	(1024)
/*#define MRST_VRTC_PGOFFSET	(0xc00) */

extern void intel_mid_rtc_init(void);

enum intel_mid_sim_type {
	INTEL_MID_CPU_SIMULATION_NONE = 0,
	INTEL_MID_CPU_SIMULATION_VP,
	INTEL_MID_CPU_SIMULATION_SLE,
	INTEL_MID_CPU_SIMULATION_HVP,
};
extern enum intel_mid_sim_type __intel_mid_sim_platform;
static inline enum intel_mid_sim_type intel_mid_identify_sim(void)
{
#ifdef CONFIG_X86_INTEL_MID
	return __intel_mid_sim_platform;
#else
	return INTEL_MID_CPU_SIMULATION_NONE;
#endif
}

#define INTEL_MID_IRQ_OFFSET 0x100

extern void pstore_ram_reserve_memory(void);

#endif /* _ASM_X86_INTEL_MID_H */
