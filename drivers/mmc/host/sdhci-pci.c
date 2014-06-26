/*  linux/drivers/mmc/host/sdhci-pci.c - SDHCI on PCI bus interface
 *
 *  Copyright (C) 2005-2008 Pierre Ossman, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * Thanks to the following companies for their support:
 *
 *     - JMicron (hardware and technical support)
 */

#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/mmc/host.h>
#include <linux/scatterlist.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/pm_runtime.h>
#include <linux/mmc/sdhci-pci-data.h>
#include <linux/lnw_gpio.h>
#include <linux/acpi_gpio.h>

#include <asm/intel_mid_rpmsg.h>
#include <asm/intel_scu_flis.h>
#include <asm/intel_scu_pmic.h>
#include <asm/spid.h>

#include "sdhci.h"

/* Settle down values copied from broadcom reference design. */
#define DELAY_CARD_INSERTED	200
#define DELAY_CARD_REMOVED	50

/*
 * PCI device IDs
 */
#define PCI_DEVICE_ID_INTEL_PCH_SDIO0	0x8809
#define PCI_DEVICE_ID_INTEL_PCH_SDIO1	0x880a

/*
 * PCI registers
 */

#define PCI_SDHCI_IFPIO			0x00
#define PCI_SDHCI_IFDMA			0x01
#define PCI_SDHCI_IFVENDOR		0x02

#define PCI_SLOT_INFO			0x40	/* 8 bits */
#define  PCI_SLOT_INFO_SLOTS(x)		((x >> 4) & 7)
#define  PCI_SLOT_INFO_FIRST_BAR_MASK	0x07

#define MAX_SLOTS			8
#define IPC_EMMC_MUTEX_CMD             0xEE

/* CLV SD card power resource */

#define VCCSDIO_ADDR		0xd5
#define VCCSDIO_OFF		0x4
#define VCCSDIO_NORMAL		0x7
#define ENCTRL0_ISOLATE		0x55555557
#define ENCTRL1_ISOLATE		0x5555
#define STORAGESTIO_FLISNUM	0x8
#define ENCTRL0_OFF		0x10
#define ENCTRL1_OFF		0x11

struct sdhci_pci_chip;
struct sdhci_pci_slot;

struct sdhci_pci_fixes {
	unsigned int		quirks;
	unsigned int		quirks2;
	bool			allow_runtime_pm;

	int			(*probe) (struct sdhci_pci_chip *);

	int			(*probe_slot) (struct sdhci_pci_slot *);
	void			(*remove_slot) (struct sdhci_pci_slot *, int);

	int			(*suspend) (struct sdhci_pci_chip *);
	int			(*resume) (struct sdhci_pci_chip *);
};

struct sdhci_pci_slot {
	struct sdhci_pci_chip	*chip;
	struct sdhci_host	*host;
	struct sdhci_pci_data	*data;

	int			pci_bar;
	int			rst_n_gpio;
	int			cd_gpio;
	int			cd_irq;
	bool			dev_power;
	struct mutex		power_lock;
	bool			dma_enabled;
	unsigned int		tuning_count;
};

struct sdhci_pci_chip {
	struct pci_dev		*pdev;

	unsigned int		quirks;
	unsigned int		quirks2;
	bool			allow_runtime_pm;
	unsigned int		autosuspend_delay;
	const struct sdhci_pci_fixes *fixes;

	int			num_slots;	/* Slots on controller */
	struct sdhci_pci_slot	*slots[MAX_SLOTS]; /* Pointers to host slots */

	unsigned int		enctrl0_orig;
	unsigned int		enctrl1_orig;
};


/*****************************************************************************\
 *                                                                           *
 * Hardware specific quirk handling                                          *
 *                                                                           *
\*****************************************************************************/

static int ricoh_probe(struct sdhci_pci_chip *chip)
{
	if (chip->pdev->subsystem_vendor == PCI_VENDOR_ID_SAMSUNG ||
	    chip->pdev->subsystem_vendor == PCI_VENDOR_ID_SONY)
		chip->quirks |= SDHCI_QUIRK_NO_CARD_NO_RESET;
	return 0;
}

static int ricoh_mmc_probe_slot(struct sdhci_pci_slot *slot)
{
	slot->host->caps =
		((0x21 << SDHCI_TIMEOUT_CLK_SHIFT)
			& SDHCI_TIMEOUT_CLK_MASK) |

		((0x21 << SDHCI_CLOCK_BASE_SHIFT)
			& SDHCI_CLOCK_BASE_MASK) |

		SDHCI_TIMEOUT_CLK_UNIT |
		SDHCI_CAN_VDD_330 |
		SDHCI_CAN_DO_HISPD |
		SDHCI_CAN_DO_SDMA;
	return 0;
}

static int ricoh_mmc_resume(struct sdhci_pci_chip *chip)
{
	/* Apply a delay to allow controller to settle */
	/* Otherwise it becomes confused if card state changed
		during suspend */
	msleep(500);
	return 0;
}

static const struct sdhci_pci_fixes sdhci_ricoh = {
	.probe		= ricoh_probe,
	.quirks		= SDHCI_QUIRK_32BIT_DMA_ADDR |
			  SDHCI_QUIRK_FORCE_DMA |
			  SDHCI_QUIRK_CLOCK_BEFORE_RESET,
};

static const struct sdhci_pci_fixes sdhci_ricoh_mmc = {
	.probe_slot	= ricoh_mmc_probe_slot,
	.resume		= ricoh_mmc_resume,
	.quirks		= SDHCI_QUIRK_32BIT_DMA_ADDR |
			  SDHCI_QUIRK_CLOCK_BEFORE_RESET |
			  SDHCI_QUIRK_NO_CARD_NO_RESET |
			  SDHCI_QUIRK_MISSING_CAPS
};

static const struct sdhci_pci_fixes sdhci_ene_712 = {
	.quirks		= SDHCI_QUIRK_SINGLE_POWER_WRITE |
			  SDHCI_QUIRK_BROKEN_DMA,
};

static const struct sdhci_pci_fixes sdhci_ene_714 = {
	.quirks		= SDHCI_QUIRK_SINGLE_POWER_WRITE |
			  SDHCI_QUIRK_RESET_CMD_DATA_ON_IOS |
			  SDHCI_QUIRK_BROKEN_DMA,
};

static const struct sdhci_pci_fixes sdhci_cafe = {
	.quirks		= SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER |
			  SDHCI_QUIRK_NO_BUSY_IRQ |
			  SDHCI_QUIRK_BROKEN_CARD_DETECTION |
			  SDHCI_QUIRK_BROKEN_TIMEOUT_VAL,
};

static int mrst_hc_probe_slot(struct sdhci_pci_slot *slot)
{
	slot->host->mmc->caps |= MMC_CAP_8_BIT_DATA;
	return 0;
}

/*
 * ADMA operation is disabled for Moorestown platform due to
 * hardware bugs.
 */
static int mrst_hc_probe(struct sdhci_pci_chip *chip)
{
	/*
	 * slots number is fixed here for MRST as SDIO3/5 are never used and
	 * have hardware bugs.
	 */
	chip->num_slots = 1;
	return 0;
}

static int pch_hc_probe_slot(struct sdhci_pci_slot *slot)
{
	slot->host->mmc->caps |= MMC_CAP_8_BIT_DATA;
	return 0;
}

#ifdef CONFIG_PM_RUNTIME

static irqreturn_t sdhci_pci_sd_cd(int irq, void *dev_id)
{
	struct sdhci_pci_slot *slot = dev_id;
	struct sdhci_host *host = slot->host;

	mmc_detect_change(host->mmc, msecs_to_jiffies(200));
	return IRQ_HANDLED;
}

static void sdhci_pci_add_own_cd(struct sdhci_pci_slot *slot)
{
	int err, irq, gpio = slot->cd_gpio;

	slot->cd_gpio = -EINVAL;
	slot->cd_irq = -EINVAL;

	if (!gpio_is_valid(gpio))
		return;

	err = gpio_request(gpio, "sd_cd");
	if (err < 0)
		goto out;

	err = gpio_direction_input(gpio);
	if (err < 0)
		goto out_free;

	irq = gpio_to_irq(gpio);
	if (irq < 0)
		goto out_free;

	err = request_irq(irq, sdhci_pci_sd_cd, IRQF_TRIGGER_RISING |
			  IRQF_TRIGGER_FALLING, "sd_cd", slot);
	if (err)
		goto out_free;

	slot->cd_gpio = gpio;
	slot->cd_irq = irq;

	return;

out_free:
	gpio_free(gpio);
out:
	dev_warn(&slot->chip->pdev->dev, "failed to setup card detect wake up\n");
}

static void sdhci_pci_remove_own_cd(struct sdhci_pci_slot *slot)
{
	if (slot->cd_irq >= 0)
		free_irq(slot->cd_irq, slot);
	if (gpio_is_valid(slot->cd_gpio))
		gpio_free(slot->cd_gpio);
}

#else

static inline void sdhci_pci_add_own_cd(struct sdhci_pci_slot *slot)
{
}

static inline void sdhci_pci_remove_own_cd(struct sdhci_pci_slot *slot)
{
}

#endif

#define MFD_SDHCI_DEKKER_BASE  0xffff7fb0
static void mfd_emmc_mutex_register(struct sdhci_pci_slot *slot)
{
	u32 mutex_var_addr;
#ifdef CONFIG_INTEL_SCU_IPC
	int err;

	err = rpmsg_send_generic_command(IPC_EMMC_MUTEX_CMD, 0,
			NULL, 0, &mutex_var_addr, 1);
	if (err) {
		dev_err(&slot->chip->pdev->dev, "IPC error: %d\n", err);
		dev_info(&slot->chip->pdev->dev, "Specify mutex address\n");
		/*
		 * Since we failed to get mutex sram address, specify it
		 */
		mutex_var_addr = MFD_SDHCI_DEKKER_BASE;
	}
#else
	mutex_var_addr = MFD_SDHCI_DEKKER_BASE;
#endif

	/* 3 housekeeping mutex variables, 12 bytes length */
	slot->host->sram_addr = ioremap_nocache(mutex_var_addr,
			3 * sizeof(u32));
	if (!slot->host->sram_addr)
		dev_err(&slot->chip->pdev->dev, "ioremap failed!\n");
	else {
		dev_info(&slot->chip->pdev->dev, "mapped addr: %p\n",
				slot->host->sram_addr);
		dev_info(&slot->chip->pdev->dev,
		"current eMMC owner: %d, IA req: %d, SCU req: %d\n",
				readl(slot->host->sram_addr +
					DEKKER_EMMC_OWNER_OFFSET),
				readl(slot->host->sram_addr +
					DEKKER_IA_REQ_OFFSET),
				readl(slot->host->sram_addr +
					DEKKER_SCU_REQ_OFFSET));
	}
	spin_lock_init(&slot->host->dekker_lock);
}

static int mfd_emmc_probe_slot(struct sdhci_pci_slot *slot)
{
	switch (slot->chip->pdev->device) {
	case PCI_DEVICE_ID_INTEL_MFD_EMMC0:
		mfd_emmc_mutex_register(slot);
		sdhci_alloc_panic_host(slot->host);
		slot->host->mmc->caps2 |= MMC_CAP2_INIT_CARD_SYNC |
			MMC_CAP2_BOOTPART_NOACC | MMC_CAP2_RPMBPART_NOACC;
		break;
	case PCI_DEVICE_ID_INTEL_MFD_EMMC1:
		break;
	case PCI_DEVICE_ID_INTEL_CLV_EMMC0:
		sdhci_alloc_panic_host(slot->host);
		slot->host->mmc->caps |= MMC_CAP_1_8V_DDR;
		slot->host->mmc->caps2 |= MMC_CAP2_INIT_CARD_SYNC |
					MMC_CAP2_CACHE_CTRL;
		slot->host->quirks2 |= SDHCI_QUIRK2_V2_0_SUPPORT_DDR50;
		/*
		 * CLV host controller has a special POWER_CTL register,
		 * which can do HW reset, so it doesn't need to operate
		 * a GPIO, so make sure platform data won't pass a valid
		 * GPIO pin to CLV host
		 */
		slot->host->mmc->caps |= MMC_CAP_HW_RESET;
		slot->rst_n_gpio = -EINVAL;
		break;
	case PCI_DEVICE_ID_INTEL_CLV_EMMC1:
		slot->host->mmc->caps |= MMC_CAP_1_8V_DDR;
		slot->host->quirks2 |= SDHCI_QUIRK2_V2_0_SUPPORT_DDR50;
		slot->host->mmc->caps2 |= MMC_CAP2_BOOTPART_NOACC |
			MMC_CAP2_RPMBPART_NOACC;
		/*
		 * CLV host controller has a special POWER_CTL register,
		 * which can do HW reset, so it doesn't need to operate
		 * a GPIO, so make sure platform data won't pass a valid
		 * GPIO pin to CLV host
		 */
		slot->host->mmc->caps |= MMC_CAP_HW_RESET;
		slot->rst_n_gpio = -EINVAL;
		break;
	}
	slot->host->mmc->caps |= MMC_CAP_8_BIT_DATA | MMC_CAP_NONREMOVABLE;
	slot->host->mmc->caps2 |= MMC_CAP2_HC_ERASE_SZ | MMC_CAP2_POLL_R1B_BUSY;
	return 0;
}

static void mfd_emmc_remove_slot(struct sdhci_pci_slot *slot, int dead)
{
	switch (slot->chip->pdev->device) {
	case PCI_DEVICE_ID_INTEL_MFD_EMMC0:
		if (slot->host->sram_addr)
			iounmap(slot->host->sram_addr);
		break;
	case PCI_DEVICE_ID_INTEL_MFD_EMMC1:
		break;
	}
}

static int mfd_sdio_probe_slot(struct sdhci_pci_slot *slot)
{
	slot->host->mmc->caps |= MMC_CAP_POWER_OFF_CARD | MMC_CAP_NONREMOVABLE;
	return 0;
}

#ifdef CONFIG_INTEL_SCU_FLIS
/*
 * Save the current power and shim status, if they are on, turn them off.
 */
static int ctp_sd_card_power_save(struct sdhci_pci_slot *slot)
{
	int err;
	u16 addr;
	u8 data;
	struct sdhci_pci_chip *chip;

	if (!slot->dev_power)
		return 0;

	chip = slot->chip;
	err = intel_scu_ipc_read_shim(&chip->enctrl0_orig,
			STORAGESTIO_FLISNUM, ENCTRL0_OFF);
	if (err) {
		pr_err("SDHCI device %04X: ENCTRL0 read failed, err %d\n",
				chip->pdev->device, err);
		chip->enctrl0_orig = ENCTRL0_ISOLATE;
		chip->enctrl1_orig = ENCTRL1_ISOLATE;
		/*
		 * stop to shut down VCCSDIO, since we cannot recover
		 * it.
		 * this should not block system entering S3
		 */
		return 0;
	}
	err = intel_scu_ipc_read_shim(&chip->enctrl1_orig,
			STORAGESTIO_FLISNUM, ENCTRL1_OFF);
	if (err) {
		pr_err("SDHCI device %04X: ENCTRL1 read failed, err %d\n",
				chip->pdev->device, err);
		chip->enctrl0_orig = ENCTRL0_ISOLATE;
		chip->enctrl1_orig = ENCTRL1_ISOLATE;
		/*
		 * stop to shut down VCCSDIO, since we cannot recover
		 * it.
		 * this should not block system entering S3
		 */
		return 0;
	}

	/* isolate shim */
	err = intel_scu_ipc_write_shim(ENCTRL0_ISOLATE,
			STORAGESTIO_FLISNUM, ENCTRL0_OFF);
	if (err) {
		pr_err("SDHCI device %04X: ENCTRL0 ISOLATE failed, err %d\n",
				chip->pdev->device, err);
		/*
		 * stop to shut down VCCSDIO. Without isolate shim, the power
		 * may have leak if turn off VCCSDIO.
		 * during S3 resuming, shim and VCCSDIO will be recofigured
		 * this should not block system entering S3
		 */
		return 0;
	}

	err = intel_scu_ipc_write_shim(ENCTRL1_ISOLATE,
			STORAGESTIO_FLISNUM, ENCTRL1_OFF);
	if (err) {
		pr_err("SDHCI device %04X: ENCTRL1 ISOLATE failed, err %d\n",
				chip->pdev->device, err);
		/*
		 * stop to shut down VCCSDIO. Without isolate shim, the power
		 * may have leak if turn off VCCSDIO.
		 * during S3 resuming, shim and VCCSDIO will be recofigured
		 * this should not block system entering S3
		 */
		return 0;
	}

	addr = VCCSDIO_ADDR;
	data = VCCSDIO_OFF;
	err = intel_scu_ipc_writev(&addr, &data, 1);
	if (err) {
		pr_err("SDHCI device %04X: VCCSDIO turn off failed, err %d\n",
				chip->pdev->device, err);
		/*
		 * during S3 resuming, VCCSDIO will be recofigured
		 * this should not block system entering S3.
		 */
	}

	slot->dev_power = false;
	return 0;
}

/*
 * Restore the power and shim if they are original on.
 */
static int ctp_sd_card_power_restore(struct sdhci_pci_slot *slot)
{
	int err;
	u16 addr;
	u8 data;
	struct sdhci_pci_chip *chip;

	if (slot->dev_power)
		return 0;

	chip = slot->chip;

	addr = VCCSDIO_ADDR;
	data = VCCSDIO_NORMAL;
	err = intel_scu_ipc_writev(&addr, &data, 1);
	if (err) {
		pr_err("SDHCI device %04X: VCCSDIO turn on failed, err %d\n",
				chip->pdev->device, err);
		/*
		 * VCCSDIO trun on failed. This may impact the SD card
		 * init and the read/write functions. We just report a
		 * warning, and go on to have a try. Anyway, SD driver
		 * can encounter error if powering up is failed
		 */
		WARN_ON(err);
	}

	if (chip->enctrl0_orig == ENCTRL0_ISOLATE &&
			chip->enctrl1_orig == ENCTRL1_ISOLATE)
		/* means we needn't to reconfigure the shim */
		return 0;

	/* reconnect shim */
	err = intel_scu_ipc_write_shim(chip->enctrl0_orig,
			STORAGESTIO_FLISNUM, ENCTRL0_OFF);

	if (err) {
		pr_err("SDHCI device %04X: ENCTRL0 CONNECT shim failed, err %d\n",
				chip->pdev->device, err);
		/* keep on setting enctrl1, but report a waring */
		WARN_ON(err);
	}

	err = intel_scu_ipc_write_shim(chip->enctrl1_orig,
			STORAGESTIO_FLISNUM, ENCTRL1_OFF);
	if (err) {
		pr_err("SDHCI device %04X: ENCTRL1 CONNECT shim failed, err %d\n",
				chip->pdev->device, err);
		/* leave this error to driver, but report a warning */
		WARN_ON(err);
	}

	slot->dev_power = true;
	return 0;
}
#else
static int ctp_sd_card_power_save(struct sdhci_pci_slot *slot)
{
	return 0;
}
static int ctp_sd_card_power_restore(struct sdhci_pci_slot *slot)
{
	return 0;
}
#endif

static const struct sdhci_pci_fixes sdhci_intel_mrst_hc0 = {
	.quirks		= SDHCI_QUIRK_BROKEN_ADMA | SDHCI_QUIRK_NO_HISPD_BIT,
	.probe_slot	= mrst_hc_probe_slot,
};

static const struct sdhci_pci_fixes sdhci_intel_mrst_hc1_hc2 = {
	.quirks		= SDHCI_QUIRK_BROKEN_ADMA | SDHCI_QUIRK_NO_HISPD_BIT,
	.probe		= mrst_hc_probe,
};

static int ctp_sd_probe_slot(struct sdhci_pci_slot *slot)
{
#ifdef CONFIG_INTEL_SCU_IPC
	int err;
	u16 addr;
#endif
	u8 data = VCCSDIO_OFF;

	if (!slot || !slot->chip || !slot->chip->pdev)
		return -ENODEV;

	if (slot->chip->pdev->device != PCI_DEVICE_ID_INTEL_CLV_SDIO0)
		return 0;

	mutex_init(&slot->power_lock);

	slot->host->flags |= SDHCI_POWER_CTRL_DEV;

#ifdef CONFIG_INTEL_SCU_IPC
	addr = VCCSDIO_ADDR;
	err = intel_scu_ipc_readv(&addr, &data, 1);
	if (err) {
		/* suppose dev_power is true */
		slot->dev_power = true;
		return 0;
	}
#endif
	if (data == VCCSDIO_NORMAL)
		slot->dev_power = true;
	else
		slot->dev_power = false;

	return 0;
}

static const struct sdhci_pci_fixes sdhci_intel_mfd_sd = {
	.quirks		= SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
	.quirks2	= SDHCI_QUIRK2_BAD_SD_CD,
	.allow_runtime_pm = true,
	.probe_slot	= ctp_sd_probe_slot,
};

static const struct sdhci_pci_fixes sdhci_intel_mfd_sdio = {
	.quirks		= SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
	.quirks2	= SDHCI_QUIRK2_HOST_OFF_CARD_ON |
		SDHCI_QUIRK2_FAKE_VDD,
	.allow_runtime_pm = true,
	.probe_slot	= mfd_sdio_probe_slot,
};

static const struct sdhci_pci_fixes sdhci_intel_mfd_emmc = {
	.quirks		= SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
	.allow_runtime_pm = true,
	.probe_slot	= mfd_emmc_probe_slot,
	.remove_slot	= mfd_emmc_remove_slot,
};

static const struct sdhci_pci_fixes sdhci_intel_pch_sdio = {
	.quirks		= SDHCI_QUIRK_BROKEN_ADMA,
	.probe_slot	= pch_hc_probe_slot,
};

static int byt_emmc_probe_slot(struct sdhci_pci_slot *slot)
{
	slot->host->mmc->caps |= MMC_CAP_8_BIT_DATA | MMC_CAP_NONREMOVABLE;
	slot->host->mmc->caps2 |= MMC_CAP2_HC_ERASE_SZ;

	switch (slot->chip->pdev->device) {
	case PCI_DEVICE_ID_INTEL_BYT_EMMC45:
		slot->host->quirks2 |= SDHCI_QUIRK2_CARD_CD_DELAY |
			SDHCI_QUIRK2_WAIT_FOR_IDLE | SDHCI_QUIRK2_TUNING_POLL;
		if (!INTEL_MID_BOARDV3(TABLET, BYT, BLK, PRO, RVP1) &&
			!INTEL_MID_BOARDV3(TABLET, BYT, BLK, PRO, RVP2) &&
			!INTEL_MID_BOARDV3(TABLET, BYT, BLK, PRO, RVP3) &&
			!INTEL_MID_BOARDV3(TABLET, BYT, BLK, ENG, RVP1) &&
			!INTEL_MID_BOARDV3(TABLET, BYT, BLK, ENG, RVP2) &&
			!INTEL_MID_BOARDV3(TABLET, BYT, BLK, ENG, RVP3))
			slot->host->mmc->caps2 |= MMC_CAP2_HS200_1_8V_SDR;
	case PCI_DEVICE_ID_INTEL_BYT_EMMC:
		if (!INTEL_MID_BOARDV2(TABLET, BYT, BLB, PRO) &&
				!INTEL_MID_BOARDV2(TABLET, BYT, BLB, ENG))
			sdhci_alloc_panic_host(slot->host);
		slot->host->mmc->caps |= MMC_CAP_1_8V_DDR;
		slot->host->mmc->caps2 |= MMC_CAP2_INIT_CARD_SYNC |
			MMC_CAP2_CACHE_CTRL;
		slot->host->mmc->qos = kzalloc(sizeof(struct pm_qos_request),
				GFP_KERNEL);
		slot->tuning_count = 8;
		break;
	default:
		break;
	}
	if (slot->host->mmc->qos)
		pm_qos_add_request(slot->host->mmc->qos, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	return 0;
}

static void byt_emmc_remove_slot(struct sdhci_pci_slot *slot, int dead)
{
	if (slot->host->mmc->qos) {
		pm_qos_remove_request(slot->host->mmc->qos);
		kfree(slot->host->mmc->qos);
	}
}

#define BYT_SD_WP	7
#define BYT_SD_CD	38
#define BYT_SD_1P8_EN	40
#define BYT_SD_PWR_EN	41
static int byt_sd_probe_slot(struct sdhci_pci_slot *slot)
{
	int err;

	slot->cd_gpio = acpi_get_gpio("\\_SB.GPO0", BYT_SD_CD);
	/*
	 * change GPIOC_7 to alternate function 2
	 * This should be done in IA FW
	 * Do this in driver as a temporal solution
	 */
	lnw_gpio_set_alt(BYT_SD_WP, 2);
	/* change the GPIO pin to GPIO mode */
	if (slot->host->quirks2 & SDHCI_QUIRK2_POWER_PIN_GPIO_MODE) {
		/* change to GPIO mode */
		lnw_gpio_set_alt(BYT_SD_PWR_EN, 0);
		err = gpio_request(BYT_SD_PWR_EN, "sd_pwr_en");
		if (err)
			return -ENODEV;
		slot->host->gpio_pwr_en = BYT_SD_PWR_EN;
		/* disable the power by default */
		gpio_direction_output(slot->host->gpio_pwr_en, 0);
		gpio_set_value(slot->host->gpio_pwr_en, 1);

		/* change to GPIO mode */
		lnw_gpio_set_alt(BYT_SD_1P8_EN, 0);
		err = gpio_request(BYT_SD_1P8_EN, "sd_1p8_en");
		if (err) {
			gpio_free(slot->host->gpio_pwr_en);
			return -ENODEV;
		}
		slot->host->gpio_1p8_en = BYT_SD_1P8_EN;
		/* 3.3v signaling by default */
		gpio_direction_output(slot->host->gpio_1p8_en, 0);
		gpio_set_value(slot->host->gpio_1p8_en, 0);
	}

	/* Bayley Bay board */
	if (INTEL_MID_BOARD(2, TABLET, BYT, BLB, PRO) ||
			INTEL_MID_BOARD(2, TABLET, BYT, BLB, ENG))
		slot->host->quirks2 |= SDHCI_QUIRK2_NO_1_8_V;

	slot->host->mmc->caps2 |= MMC_CAP2_PWCTRL_POWER |
		MMC_CAP2_FIXED_NCRC;

	/* On BYT-M, SD card is using to store ipanic as a W/A */
	if (INTEL_MID_BOARDV2(TABLET, BYT, BLB, PRO) ||
			INTEL_MID_BOARDV2(TABLET, BYT, BLB, ENG))
		sdhci_alloc_panic_host(slot->host);

	slot->host->mmc->qos = kzalloc(sizeof(struct pm_qos_request),
			GFP_KERNEL);
	if (slot->host->mmc->qos)
		pm_qos_add_request(slot->host->mmc->qos, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	return 0;
}

static void byt_sd_remove_slot(struct sdhci_pci_slot *slot, int dead)
{
	if (slot->host->quirks2 & SDHCI_QUIRK2_POWER_PIN_GPIO_MODE) {
		if (gpio_is_valid(slot->host->gpio_1p8_en))
			gpio_free(slot->host->gpio_1p8_en);
		if (gpio_is_valid(slot->host->gpio_pwr_en))
			gpio_free(slot->host->gpio_pwr_en);
	}
	if (slot->host->mmc->qos) {
		pm_qos_remove_request(slot->host->mmc->qos);
		kfree(slot->host->mmc->qos);
	}
}

static int byt_sdio_probe_slot(struct sdhci_pci_slot *slot)
{
	slot->host->mmc->caps |= MMC_CAP_POWER_OFF_CARD | MMC_CAP_NONREMOVABLE;
	switch (slot->chip->pdev->device) {
	case PCI_DEVICE_ID_INTEL_BYT_SDIO:
		/* add a delay after runtime resuming back from D0i3 */
		slot->chip->pdev->d3_delay = 10;
		/* reduce the auto suspend delay for SDIO to be 500ms */
		slot->chip->autosuspend_delay = 500;
		slot->host->mmc->qos = kzalloc(sizeof(struct pm_qos_request),
				GFP_KERNEL);
		break;
	}

	if (slot->host->mmc->qos)
		pm_qos_add_request(slot->host->mmc->qos, PM_QOS_CPU_DMA_LATENCY,
				PM_QOS_DEFAULT_VALUE);

	return 0;
}

static void byt_sdio_remove_slot(struct sdhci_pci_slot *slot, int dead)
{
	if (slot->host->mmc->qos) {
		pm_qos_remove_request(slot->host->mmc->qos);
		kfree(slot->host->mmc->qos);
	}
}

static const struct sdhci_pci_fixes sdhci_intel_byt_emmc = {
	.quirks2	= SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
	.allow_runtime_pm = true,
	.probe_slot	= byt_emmc_probe_slot,
	.remove_slot	= byt_emmc_remove_slot,
};

static const struct sdhci_pci_fixes sdhci_intel_byt_sdio = {
	.quirks2	= SDHCI_QUIRK2_HOST_OFF_CARD_ON |
		SDHCI_QUIRK2_PRESET_VALUE_BROKEN | SDHCI_QUIRK2_FAKE_VDD,
	.allow_runtime_pm = true,
	.probe_slot	= byt_sdio_probe_slot,
	.remove_slot	= byt_sdio_remove_slot,
};

static const struct sdhci_pci_fixes sdhci_intel_byt_sd = {
	.quirks2	= SDHCI_QUIRK2_PRESET_VALUE_BROKEN |
		SDHCI_QUIRK2_POWER_PIN_GPIO_MODE,
	.allow_runtime_pm = true,
	.probe_slot	= byt_sd_probe_slot,
	.remove_slot	= byt_sd_remove_slot,
};

#define TNG_IOAPIC_IDX	0xfec00000
static void mrfl_ioapic_rte_reg_addr_map(struct sdhci_pci_slot *slot)
{
	slot->host->rte_addr = ioremap_nocache(TNG_IOAPIC_IDX, 256);
	if (!slot->host->rte_addr)
		dev_err(&slot->chip->pdev->dev, "rte_addr ioremap fail!\n");
	else
		dev_info(&slot->chip->pdev->dev, "rte_addr mapped addr: %p\n",
			slot->host->rte_addr);
}

/* Define Host controllers for Intel Merrifield platform */
#define INTEL_MRFL_EMMC_0	0
#define INTEL_MRFL_EMMC_1	1
#define INTEL_MRFL_SD		2
#define INTEL_MRFL_SDIO		3

static int intel_mrfl_mmc_probe_slot(struct sdhci_pci_slot *slot)
{
	int ret = 0;

	switch (PCI_FUNC(slot->chip->pdev->devfn)) {
	case INTEL_MRFL_EMMC_0:
		sdhci_alloc_panic_host(slot->host);
		slot->host->mmc->caps |= MMC_CAP_8_BIT_DATA |
					MMC_CAP_NONREMOVABLE |
					MMC_CAP_1_8V_DDR;
		slot->host->mmc->caps2 |= MMC_CAP2_POLL_R1B_BUSY |
					MMC_CAP2_INIT_CARD_SYNC |
					MMC_CAP2_CACHE_CTRL;
		if (slot->chip->pdev->revision == 0x1) { /* B0 stepping */
			slot->host->mmc->caps2 |= MMC_CAP2_HS200_1_8V_SDR;
			/* WA for async abort silicon issue */
			slot->host->quirks2 |= SDHCI_QUIRK2_CARD_CD_DELAY |
					SDHCI_QUIRK2_WAIT_FOR_IDLE |
					SDHCI_QUIRK2_TUNING_POLL;
		}
		mrfl_ioapic_rte_reg_addr_map(slot);
		slot->tuning_count = 8;
		break;
	case INTEL_MRFL_SD:
		slot->host->quirks2 |= SDHCI_QUIRK2_WAIT_FOR_IDLE;
		slot->host->mmc->caps2 |= MMC_CAP2_FIXED_NCRC;
		break;
	case INTEL_MRFL_SDIO:
		slot->host->mmc->caps |= MMC_CAP_NONREMOVABLE;
		slot->host->quirks2 |= SDHCI_QUIRK2_FAKE_VDD;
		break;
	}

	if (slot->data->platform_quirks & PLFM_QUIRK_NO_HIGH_SPEED) {
		slot->host->quirks2 |= SDHCI_QUIRK2_DISABLE_HIGH_SPEED;
		slot->host->mmc->caps &= ~MMC_CAP_1_8V_DDR;
	}

	if (slot->data->platform_quirks & PLFM_QUIRK_NO_EMMC_BOOT_PART)
		slot->host->mmc->caps2 |= MMC_CAP2_BOOTPART_NOACC;

	if (slot->data->platform_quirks & PLFM_QUIRK_NO_HOST_CTRL_HW) {
		dev_info(&slot->chip->pdev->dev, "Disable MMC Func %d.\n",
			PCI_FUNC(slot->chip->pdev->devfn));
		ret = -ENODEV;
	}

	return ret;
}

static void intel_mrfl_mmc_remove_slot(struct sdhci_pci_slot *slot, int dead)
{
	if (PCI_FUNC(slot->chip->pdev->devfn) == INTEL_MRFL_EMMC_0)
		if (slot->host->rte_addr)
			iounmap(slot->host->rte_addr);
}

static const struct sdhci_pci_fixes sdhci_intel_mrfl_mmc = {
	.quirks		= SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
	.quirks2	= SDHCI_QUIRK2_BROKEN_AUTO_CMD23 |
				SDHCI_QUIRK2_HIGH_SPEED_SET_LATE |
				SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
	.allow_runtime_pm = true,
	.probe_slot	= intel_mrfl_mmc_probe_slot,
	.remove_slot	= intel_mrfl_mmc_remove_slot,
};

static int intel_moor_emmc_probe_slot(struct sdhci_pci_slot *slot)
{
	slot->host->mmc->caps |= MMC_CAP_8_BIT_DATA |
				MMC_CAP_NONREMOVABLE |
				MMC_CAP_1_8V_DDR;

	sdhci_alloc_panic_host(slot->host);

	slot->host->mmc->caps2 |= MMC_CAP2_POLL_R1B_BUSY |
				MMC_CAP2_INIT_CARD_SYNC |
				MMC_CAP2_CACHE_CTRL;

	/* Enable HS200 and HS400 */
	slot->host->mmc->caps2 |= MMC_CAP2_HS200_1_8V_SDR |
				MMC_CAP2_HS200_DIS;

	if (slot->chip->pdev->revision == 0x1) { /* B0 stepping */
		slot->host->mmc->caps2 |= MMC_CAP2_HS400_1_8V_DDR;
	}

	slot->host->quirks2 |= SDHCI_QUIRK2_TUNING_POLL;

	if (slot->data)
		if (slot->data->platform_quirks & PLFM_QUIRK_NO_HIGH_SPEED) {
			slot->host->quirks2 |= SDHCI_QUIRK2_DISABLE_HIGH_SPEED;
			slot->host->mmc->caps &= ~MMC_CAP_1_8V_DDR;
			slot->host->mmc->caps2 &= ~MMC_CAP2_HS200_1_8V_SDR;
			if (slot->chip->pdev->revision == 0x1) {
				slot->host->mmc->caps2 &=
					~MMC_CAP2_HS400_1_8V_DDR;
			}
		}

	if (slot->data)
		if (slot->data->platform_quirks & PLFM_QUIRK_NO_EMMC_BOOT_PART)
			slot->host->mmc->caps2 |= MMC_CAP2_BOOTPART_NOACC;

	return 0;
}

static void intel_moor_emmc_remove_slot(struct sdhci_pci_slot *slot, int dead)
{
}

static int intel_moor_sd_probe_slot(struct sdhci_pci_slot *slot)
{
	int ret = 0;

	slot->host->mmc->caps2 |= MMC_CAP2_FIXED_NCRC;
	if (slot->data)
		if (slot->data->platform_quirks & PLFM_QUIRK_NO_HOST_CTRL_HW)
			ret = -ENODEV;

	return ret;
}

static void intel_moor_sd_remove_slot(struct sdhci_pci_slot *slot, int dead)
{
}

static int intel_moor_sdio_probe_slot(struct sdhci_pci_slot *slot)
{
	int ret = 0;

	slot->host->mmc->caps |= MMC_CAP_NONREMOVABLE;

	if (slot->data)
		if (slot->data->platform_quirks & PLFM_QUIRK_NO_HOST_CTRL_HW)
			ret = -ENODEV;

	return ret;
}

static void intel_moor_sdio_remove_slot(struct sdhci_pci_slot *slot, int dead)
{
}

static const struct sdhci_pci_fixes sdhci_intel_moor_emmc = {
	.quirks		= SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
	.quirks2	= SDHCI_QUIRK2_BROKEN_AUTO_CMD23 |
				SDHCI_QUIRK2_HIGH_SPEED_SET_LATE,
	.allow_runtime_pm = true,
	.probe_slot	= intel_moor_emmc_probe_slot,
	.remove_slot	= intel_moor_emmc_remove_slot,
};

static const struct sdhci_pci_fixes sdhci_intel_moor_sd = {
	.quirks		= SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
	.quirks2	= SDHCI_QUIRK2_BROKEN_AUTO_CMD23 |
				SDHCI_QUIRK2_HIGH_SPEED_SET_LATE,
	.allow_runtime_pm = true,
	.probe_slot	= intel_moor_sd_probe_slot,
	.remove_slot	= intel_moor_sd_remove_slot,
};

static const struct sdhci_pci_fixes sdhci_intel_moor_sdio = {
	.quirks		= SDHCI_QUIRK_NO_ENDATTR_IN_NOPDESC,
	.quirks2	= SDHCI_QUIRK2_BROKEN_AUTO_CMD23 |
				SDHCI_QUIRK2_HIGH_SPEED_SET_LATE |
				SDHCI_QUIRK2_FAKE_VDD |
				SDHCI_QUIRK2_PRESET_VALUE_BROKEN,
	.allow_runtime_pm = true,
	.probe_slot	= intel_moor_sdio_probe_slot,
	.remove_slot	= intel_moor_sdio_remove_slot,
};

/* O2Micro extra registers */
#define O2_SD_LOCK_WP		0xD3
#define O2_SD_MULTI_VCC3V	0xEE
#define O2_SD_CLKREQ		0xEC
#define O2_SD_CAPS		0xE0
#define O2_SD_ADMA1		0xE2
#define O2_SD_ADMA2		0xE7
#define O2_SD_INF_MOD		0xF1

static int o2_probe(struct sdhci_pci_chip *chip)
{
	int ret;
	u8 scratch;

	switch (chip->pdev->device) {
	case PCI_DEVICE_ID_O2_8220:
	case PCI_DEVICE_ID_O2_8221:
	case PCI_DEVICE_ID_O2_8320:
	case PCI_DEVICE_ID_O2_8321:
		/* This extra setup is required due to broken ADMA. */
		ret = pci_read_config_byte(chip->pdev, O2_SD_LOCK_WP, &scratch);
		if (ret)
			return ret;
		scratch &= 0x7f;
		pci_write_config_byte(chip->pdev, O2_SD_LOCK_WP, scratch);

		/* Set Multi 3 to VCC3V# */
		pci_write_config_byte(chip->pdev, O2_SD_MULTI_VCC3V, 0x08);

		/* Disable CLK_REQ# support after media DET */
		ret = pci_read_config_byte(chip->pdev, O2_SD_CLKREQ, &scratch);
		if (ret)
			return ret;
		scratch |= 0x20;
		pci_write_config_byte(chip->pdev, O2_SD_CLKREQ, scratch);

		/* Choose capabilities, enable SDMA.  We have to write 0x01
		 * to the capabilities register first to unlock it.
		 */
		ret = pci_read_config_byte(chip->pdev, O2_SD_CAPS, &scratch);
		if (ret)
			return ret;
		scratch |= 0x01;
		pci_write_config_byte(chip->pdev, O2_SD_CAPS, scratch);
		pci_write_config_byte(chip->pdev, O2_SD_CAPS, 0x73);

		/* Disable ADMA1/2 */
		pci_write_config_byte(chip->pdev, O2_SD_ADMA1, 0x39);
		pci_write_config_byte(chip->pdev, O2_SD_ADMA2, 0x08);

		/* Disable the infinite transfer mode */
		ret = pci_read_config_byte(chip->pdev, O2_SD_INF_MOD, &scratch);
		if (ret)
			return ret;
		scratch |= 0x08;
		pci_write_config_byte(chip->pdev, O2_SD_INF_MOD, scratch);

		/* Lock WP */
		ret = pci_read_config_byte(chip->pdev, O2_SD_LOCK_WP, &scratch);
		if (ret)
			return ret;
		scratch |= 0x80;
		pci_write_config_byte(chip->pdev, O2_SD_LOCK_WP, scratch);
	}

	return 0;
}

static int jmicron_pmos(struct sdhci_pci_chip *chip, int on)
{
	u8 scratch;
	int ret;

	ret = pci_read_config_byte(chip->pdev, 0xAE, &scratch);
	if (ret)
		return ret;

	/*
	 * Turn PMOS on [bit 0], set over current detection to 2.4 V
	 * [bit 1:2] and enable over current debouncing [bit 6].
	 */
	if (on)
		scratch |= 0x47;
	else
		scratch &= ~0x47;

	ret = pci_write_config_byte(chip->pdev, 0xAE, scratch);
	if (ret)
		return ret;

	return 0;
}

static int jmicron_probe(struct sdhci_pci_chip *chip)
{
	int ret;
	u16 mmcdev = 0;

	if (chip->pdev->revision == 0) {
		chip->quirks |= SDHCI_QUIRK_32BIT_DMA_ADDR |
			  SDHCI_QUIRK_32BIT_DMA_SIZE |
			  SDHCI_QUIRK_32BIT_ADMA_SIZE |
			  SDHCI_QUIRK_RESET_AFTER_REQUEST |
			  SDHCI_QUIRK_BROKEN_SMALL_PIO;
	}

	/*
	 * JMicron chips can have two interfaces to the same hardware
	 * in order to work around limitations in Microsoft's driver.
	 * We need to make sure we only bind to one of them.
	 *
	 * This code assumes two things:
	 *
	 * 1. The PCI code adds subfunctions in order.
	 *
	 * 2. The MMC interface has a lower subfunction number
	 *    than the SD interface.
	 */
	if (chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB38X_SD)
		mmcdev = PCI_DEVICE_ID_JMICRON_JMB38X_MMC;
	else if (chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB388_SD)
		mmcdev = PCI_DEVICE_ID_JMICRON_JMB388_ESD;

	if (mmcdev) {
		struct pci_dev *sd_dev;

		sd_dev = NULL;
		while ((sd_dev = pci_get_device(PCI_VENDOR_ID_JMICRON,
						mmcdev, sd_dev)) != NULL) {
			if ((PCI_SLOT(chip->pdev->devfn) ==
				PCI_SLOT(sd_dev->devfn)) &&
				(chip->pdev->bus == sd_dev->bus))
				break;
		}

		if (sd_dev) {
			pci_dev_put(sd_dev);
			dev_info(&chip->pdev->dev, "Refusing to bind to "
				"secondary interface.\n");
			return -ENODEV;
		}
	}

	/*
	 * JMicron chips need a bit of a nudge to enable the power
	 * output pins.
	 */
	ret = jmicron_pmos(chip, 1);
	if (ret) {
		dev_err(&chip->pdev->dev, "Failure enabling card power\n");
		return ret;
	}

	/* quirk for unsable RO-detection on JM388 chips */
	if (chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB388_SD ||
	    chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB388_ESD)
		chip->quirks |= SDHCI_QUIRK_UNSTABLE_RO_DETECT;

	return 0;
}

static void jmicron_enable_mmc(struct sdhci_host *host, int on)
{
	u8 scratch;

	scratch = readb(host->ioaddr + 0xC0);

	if (on)
		scratch |= 0x01;
	else
		scratch &= ~0x01;

	writeb(scratch, host->ioaddr + 0xC0);
}

static int jmicron_probe_slot(struct sdhci_pci_slot *slot)
{
	if (slot->chip->pdev->revision == 0) {
		u16 version;

		version = readl(slot->host->ioaddr + SDHCI_HOST_VERSION);
		version = (version & SDHCI_VENDOR_VER_MASK) >>
			SDHCI_VENDOR_VER_SHIFT;

		/*
		 * Older versions of the chip have lots of nasty glitches
		 * in the ADMA engine. It's best just to avoid it
		 * completely.
		 */
		if (version < 0xAC)
			slot->host->quirks |= SDHCI_QUIRK_BROKEN_ADMA;
	}

	/* JM388 MMC doesn't support 1.8V while SD supports it */
	if (slot->chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB388_ESD) {
		slot->host->ocr_avail_sd = MMC_VDD_32_33 | MMC_VDD_33_34 |
			MMC_VDD_29_30 | MMC_VDD_30_31 |
			MMC_VDD_165_195; /* allow 1.8V */
		slot->host->ocr_avail_mmc = MMC_VDD_32_33 | MMC_VDD_33_34 |
			MMC_VDD_29_30 | MMC_VDD_30_31; /* no 1.8V for MMC */
	}

	/*
	 * The secondary interface requires a bit set to get the
	 * interrupts.
	 */
	if (slot->chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB38X_MMC ||
	    slot->chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB388_ESD)
		jmicron_enable_mmc(slot->host, 1);

	slot->host->mmc->caps |= MMC_CAP_BUS_WIDTH_TEST;

	return 0;
}

static void jmicron_remove_slot(struct sdhci_pci_slot *slot, int dead)
{
	if (dead)
		return;

	if (slot->chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB38X_MMC ||
	    slot->chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB388_ESD)
		jmicron_enable_mmc(slot->host, 0);
}

static int jmicron_suspend(struct sdhci_pci_chip *chip)
{
	int i;

	if (chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB38X_MMC ||
	    chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB388_ESD) {
		for (i = 0; i < chip->num_slots; i++)
			jmicron_enable_mmc(chip->slots[i]->host, 0);
	}

	return 0;
}

static int jmicron_resume(struct sdhci_pci_chip *chip)
{
	int ret, i;

	if (chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB38X_MMC ||
	    chip->pdev->device == PCI_DEVICE_ID_JMICRON_JMB388_ESD) {
		for (i = 0; i < chip->num_slots; i++)
			jmicron_enable_mmc(chip->slots[i]->host, 1);
	}

	ret = jmicron_pmos(chip, 1);
	if (ret) {
		dev_err(&chip->pdev->dev, "Failure enabling card power\n");
		return ret;
	}

	return 0;
}

static const struct sdhci_pci_fixes sdhci_o2 = {
	.probe		= o2_probe,
};

static const struct sdhci_pci_fixes sdhci_jmicron = {
	.probe		= jmicron_probe,

	.probe_slot	= jmicron_probe_slot,
	.remove_slot	= jmicron_remove_slot,

	.suspend	= jmicron_suspend,
	.resume		= jmicron_resume,
};

/* SysKonnect CardBus2SDIO extra registers */
#define SYSKT_CTRL		0x200
#define SYSKT_RDFIFO_STAT	0x204
#define SYSKT_WRFIFO_STAT	0x208
#define SYSKT_POWER_DATA	0x20c
#define   SYSKT_POWER_330	0xef
#define   SYSKT_POWER_300	0xf8
#define   SYSKT_POWER_184	0xcc
#define SYSKT_POWER_CMD		0x20d
#define   SYSKT_POWER_START	(1 << 7)
#define SYSKT_POWER_STATUS	0x20e
#define   SYSKT_POWER_STATUS_OK	(1 << 0)
#define SYSKT_BOARD_REV		0x210
#define SYSKT_CHIP_REV		0x211
#define SYSKT_CONF_DATA		0x212
#define   SYSKT_CONF_DATA_1V8	(1 << 2)
#define   SYSKT_CONF_DATA_2V5	(1 << 1)
#define   SYSKT_CONF_DATA_3V3	(1 << 0)

static int syskt_probe(struct sdhci_pci_chip *chip)
{
	if ((chip->pdev->class & 0x0000FF) == PCI_SDHCI_IFVENDOR) {
		chip->pdev->class &= ~0x0000FF;
		chip->pdev->class |= PCI_SDHCI_IFDMA;
	}
	return 0;
}

static int syskt_probe_slot(struct sdhci_pci_slot *slot)
{
	int tm, ps;

	u8 board_rev = readb(slot->host->ioaddr + SYSKT_BOARD_REV);
	u8  chip_rev = readb(slot->host->ioaddr + SYSKT_CHIP_REV);
	dev_info(&slot->chip->pdev->dev, "SysKonnect CardBus2SDIO, "
					 "board rev %d.%d, chip rev %d.%d\n",
					 board_rev >> 4, board_rev & 0xf,
					 chip_rev >> 4,  chip_rev & 0xf);
	if (chip_rev >= 0x20)
		slot->host->quirks |= SDHCI_QUIRK_FORCE_DMA;

	writeb(SYSKT_POWER_330, slot->host->ioaddr + SYSKT_POWER_DATA);
	writeb(SYSKT_POWER_START, slot->host->ioaddr + SYSKT_POWER_CMD);
	udelay(50);
	tm = 10;  /* Wait max 1 ms */
	do {
		ps = readw(slot->host->ioaddr + SYSKT_POWER_STATUS);
		if (ps & SYSKT_POWER_STATUS_OK)
			break;
		udelay(100);
	} while (--tm);
	if (!tm) {
		dev_err(&slot->chip->pdev->dev,
			"power regulator never stabilized");
		writeb(0, slot->host->ioaddr + SYSKT_POWER_CMD);
		return -ENODEV;
	}

	return 0;
}

static const struct sdhci_pci_fixes sdhci_syskt = {
	.quirks		= SDHCI_QUIRK_NO_SIMULT_VDD_AND_POWER,
	.probe		= syskt_probe,
	.probe_slot	= syskt_probe_slot,
};

static int via_probe(struct sdhci_pci_chip *chip)
{
	if (chip->pdev->revision == 0x10)
		chip->quirks |= SDHCI_QUIRK_DELAY_AFTER_POWER;

	return 0;
}

static const struct sdhci_pci_fixes sdhci_via = {
	.probe		= via_probe,
};

static const struct pci_device_id pci_ids[] = {
	{
		.vendor		= PCI_VENDOR_ID_RICOH,
		.device		= PCI_DEVICE_ID_RICOH_R5C822,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_ricoh,
	},

	{
		.vendor         = PCI_VENDOR_ID_RICOH,
		.device         = 0x843,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.driver_data    = (kernel_ulong_t)&sdhci_ricoh_mmc,
	},

	{
		.vendor         = PCI_VENDOR_ID_RICOH,
		.device         = 0xe822,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.driver_data    = (kernel_ulong_t)&sdhci_ricoh_mmc,
	},

	{
		.vendor         = PCI_VENDOR_ID_RICOH,
		.device         = 0xe823,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.driver_data    = (kernel_ulong_t)&sdhci_ricoh_mmc,
	},

	{
		.vendor		= PCI_VENDOR_ID_ENE,
		.device		= PCI_DEVICE_ID_ENE_CB712_SD,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_ene_712,
	},

	{
		.vendor		= PCI_VENDOR_ID_ENE,
		.device		= PCI_DEVICE_ID_ENE_CB712_SD_2,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_ene_712,
	},

	{
		.vendor		= PCI_VENDOR_ID_ENE,
		.device		= PCI_DEVICE_ID_ENE_CB714_SD,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_ene_714,
	},

	{
		.vendor		= PCI_VENDOR_ID_ENE,
		.device		= PCI_DEVICE_ID_ENE_CB714_SD_2,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_ene_714,
	},

	{
		.vendor         = PCI_VENDOR_ID_MARVELL,
		.device         = PCI_DEVICE_ID_MARVELL_88ALP01_SD,
		.subvendor      = PCI_ANY_ID,
		.subdevice      = PCI_ANY_ID,
		.driver_data    = (kernel_ulong_t)&sdhci_cafe,
	},

	{
		.vendor		= PCI_VENDOR_ID_JMICRON,
		.device		= PCI_DEVICE_ID_JMICRON_JMB38X_SD,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_jmicron,
	},

	{
		.vendor		= PCI_VENDOR_ID_JMICRON,
		.device		= PCI_DEVICE_ID_JMICRON_JMB38X_MMC,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_jmicron,
	},

	{
		.vendor		= PCI_VENDOR_ID_JMICRON,
		.device		= PCI_DEVICE_ID_JMICRON_JMB388_SD,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_jmicron,
	},

	{
		.vendor		= PCI_VENDOR_ID_JMICRON,
		.device		= PCI_DEVICE_ID_JMICRON_JMB388_ESD,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_jmicron,
	},

	{
		.vendor		= PCI_VENDOR_ID_SYSKONNECT,
		.device		= 0x8000,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_syskt,
	},

	{
		.vendor		= PCI_VENDOR_ID_VIA,
		.device		= 0x95d0,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_via,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MRST_SD0,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mrst_hc0,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MRST_SD1,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mrst_hc1_hc2,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MRST_SD2,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mrst_hc1_hc2,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MFD_SD,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mfd_sd,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MFD_SDIO1,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mfd_sdio,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MFD_SDIO2,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mfd_sdio,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MFD_EMMC0,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mfd_emmc,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MFD_EMMC1,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mfd_emmc,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_PCH_SDIO0,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_pch_sdio,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_PCH_SDIO1,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_pch_sdio,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_BYT_EMMC,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_byt_emmc,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_BYT_EMMC45,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_byt_emmc,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_BYT_SDIO,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_byt_sdio,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_BYT_SD,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_byt_sd,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_CLV_SDIO0,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mfd_sd,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_CLV_SDIO1,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mfd_sdio,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_CLV_SDIO2,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mfd_sdio,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_CLV_EMMC0,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mfd_emmc,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_CLV_EMMC1,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mfd_emmc,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MRFL_MMC,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_mrfl_mmc,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MOOR_EMMC,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_moor_emmc,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MOOR_SD,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_moor_sd,
	},

	{
		.vendor		= PCI_VENDOR_ID_INTEL,
		.device		= PCI_DEVICE_ID_INTEL_MOOR_SDIO,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_intel_moor_sdio,
	},

	{
		.vendor		= PCI_VENDOR_ID_O2,
		.device		= PCI_DEVICE_ID_O2_8120,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_o2,
	},

	{
		.vendor		= PCI_VENDOR_ID_O2,
		.device		= PCI_DEVICE_ID_O2_8220,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_o2,
	},

	{
		.vendor		= PCI_VENDOR_ID_O2,
		.device		= PCI_DEVICE_ID_O2_8221,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_o2,
	},

	{
		.vendor		= PCI_VENDOR_ID_O2,
		.device		= PCI_DEVICE_ID_O2_8320,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_o2,
	},

	{
		.vendor		= PCI_VENDOR_ID_O2,
		.device		= PCI_DEVICE_ID_O2_8321,
		.subvendor	= PCI_ANY_ID,
		.subdevice	= PCI_ANY_ID,
		.driver_data	= (kernel_ulong_t)&sdhci_o2,
	},

	{	/* Generic SD host controller */
		PCI_DEVICE_CLASS((PCI_CLASS_SYSTEM_SDHCI << 8), 0xFFFF00)
	},

	{ /* end: all zeroes */ },
};

MODULE_DEVICE_TABLE(pci, pci_ids);

/*****************************************************************************\
 *                                                                           *
 * SDHCI core callbacks                                                      *
 *                                                                           *
\*****************************************************************************/

static int try_request_regulator(struct device *dev, void *data)
{
	struct pci_dev        *pdev = container_of(dev, struct pci_dev, dev);
	struct sdhci_pci_chip *chip;
	struct sdhci_pci_slot *slot;
	struct sdhci_host     *host;
	int i;

	chip = pci_get_drvdata(pdev);
	if (!chip)
		return 0;

	for (i = 0; i < chip->num_slots; i++) {
		slot = chip->slots[i];
		if (!slot)
			continue;
		host = slot->host;
		if (!host)
			continue;
		if (sdhci_try_get_regulator(host) == 0)
			mmc_detect_change(host->mmc, 0);
	}
	return 0;
}

static struct pci_driver sdhci_driver;

/**
 *      sdhci_pci_request_regulators - retry requesting regulators of
 *                                     all sdhci-pci devices
 *
 *      One some platforms, the regulators associated to the mmc are available
 *      late in the boot.
 *      sdhci_pci_request_regulators() is called by platform code to retry
 *      getting the regulators associated to pci sdhcis
 */

int sdhci_pci_request_regulators(void)
{
	/* driver not yet registered */
	if (!sdhci_driver.driver.p)
		return 0;
	return driver_for_each_device(&sdhci_driver.driver,
				      NULL, NULL, try_request_regulator);
}
EXPORT_SYMBOL_GPL(sdhci_pci_request_regulators);

static int sdhci_pci_enable_dma(struct sdhci_host *host)
{
	struct sdhci_pci_slot *slot;
	struct pci_dev *pdev;
	int ret;

	slot = sdhci_priv(host);
	if (slot->dma_enabled)
		return 0;

	pdev = slot->chip->pdev;

	if (((pdev->class & 0xFFFF00) == (PCI_CLASS_SYSTEM_SDHCI << 8)) &&
		((pdev->class & 0x0000FF) != PCI_SDHCI_IFDMA) &&
		(host->flags & SDHCI_USE_SDMA)) {
		dev_warn(&pdev->dev, "Will use DMA mode even though HW "
			"doesn't fully claim to support it.\n");
	}

	ret = pci_set_dma_mask(pdev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	pci_set_master(pdev);

	slot->dma_enabled = true;

	return 0;
}

static int sdhci_pci_bus_width(struct sdhci_host *host, int width)
{
	u8 ctrl;

	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);

	switch (width) {
	case MMC_BUS_WIDTH_8:
		ctrl |= SDHCI_CTRL_8BITBUS;
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		break;
	case MMC_BUS_WIDTH_4:
		ctrl |= SDHCI_CTRL_4BITBUS;
		ctrl &= ~SDHCI_CTRL_8BITBUS;
		break;
	default:
		ctrl &= ~(SDHCI_CTRL_8BITBUS | SDHCI_CTRL_4BITBUS);
		break;
	}

	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

	return 0;
}

static void sdhci_pci_hw_reset(struct sdhci_host *host)
{
	struct sdhci_pci_slot *slot = sdhci_priv(host);
	int rst_n_gpio = slot->rst_n_gpio;
	u8 pwr;

	if (gpio_is_valid(rst_n_gpio)) {
		gpio_set_value_cansleep(rst_n_gpio, 0);
		/* For eMMC, minimum is 1us but give it 10us for good measure */
		udelay(10);
		gpio_set_value_cansleep(rst_n_gpio, 1);
		/*
		 * For eMMC, minimum is 200us,
		 * but give it 300us for good measure
		 */
		usleep_range(300, 1000);
	} else if (slot->host->mmc->caps & MMC_CAP_HW_RESET) {
		/* first set bit4 of power control register */
		pwr = sdhci_readb(host, SDHCI_POWER_CONTROL);
		pwr |= SDHCI_HW_RESET;
		sdhci_writeb(host, pwr, SDHCI_POWER_CONTROL);
		/* keep the same delay for safe */
		usleep_range(300, 1000);
		/* then clear bit4 of power control register */
		pwr &= ~SDHCI_HW_RESET;
		sdhci_writeb(host, pwr, SDHCI_POWER_CONTROL);
		/* keep the same delay for safe */
		usleep_range(300, 1000);
	}
}

static int sdhci_pci_power_up_host(struct sdhci_host *host)
{
	int ret = -ENOSYS;
	struct sdhci_pci_slot *slot = sdhci_priv(host);

	if (slot->data && slot->data->power_up)
		ret = slot->data->power_up(host);
	else {
		/*
		 * use standard PCI power up function
		 */
		ret = pci_set_power_state(slot->chip->pdev, PCI_D0);
		mdelay(50);
	}
	/*
	 * If there is no power_up callbacks in platform data,
	 * return -ENOSYS;
	 */
	if (ret)
		return ret;

	/*
	 * after power up host, let's have a little test
	 */

	if (sdhci_readl(host, SDHCI_HOST_VERSION) ==
			0xffffffff) {
		pr_err("%s: power up sdhci host failed\n",
				__func__);
		return -EPERM;
	}

	pr_info("%s: host controller power up is done\n", __func__);

	return 0;
}

static void sdhci_pci_set_dev_power(struct sdhci_host *host, bool poweron)
{
	struct sdhci_pci_slot *slot;
	struct sdhci_pci_chip *chip;

	slot = sdhci_priv(host);
	if (slot)
		chip = slot->chip;
	else
		return;

	/* only available for Intel CTP platform */
	if (chip && chip->pdev &&
		chip->pdev->device == PCI_DEVICE_ID_INTEL_CLV_SDIO0) {
		mutex_lock(&slot->power_lock);
		if (poweron)
			ctp_sd_card_power_restore(slot);
		else
			ctp_sd_card_power_save(slot);
		mutex_unlock(&slot->power_lock);
	}
}

static int sdhci_pci_get_cd(struct sdhci_host *host)
{
	bool present;
	struct sdhci_pci_slot *slot = sdhci_priv(host);

	if (gpio_is_valid(slot->cd_gpio))
		return gpio_get_value(slot->cd_gpio) ? 0 : 1;

	/* If nonremovable or polling, assume that the card is always present */
	if (host->quirks & SDHCI_QUIRK_BROKEN_CARD_DETECTION)
		present = true;
	else
		present = sdhci_readl(host, SDHCI_PRESENT_STATE) &
			SDHCI_CARD_PRESENT;

	return present;
}

static void  sdhci_platform_reset_exit(struct sdhci_host *host, u8 mask)
{
	if (host->quirks2 & SDHCI_QUIRK2_POWER_PIN_GPIO_MODE) {
		if (mask & SDHCI_RESET_ALL) {
			/* reset back to 3.3v signaling */
			gpio_set_value(host->gpio_1p8_en, 0);
			/* disable the VDD power */
			gpio_set_value(host->gpio_pwr_en, 1);
		}
	}
}

static int sdhci_pci_get_tuning_count(struct sdhci_host *host)
{
	struct sdhci_pci_slot *slot = sdhci_priv(host);

	return slot->tuning_count;
}

static int sdhci_gpio_buf_check(struct sdhci_host *host, unsigned int clk)
{
	int ret = -ENOSYS;
	struct sdhci_pci_slot *slot = sdhci_priv(host);

	if (slot->data && slot->data->flis_check)
		ret = slot->data->flis_check(slot->data->flis_addr,
					host->clock, clk);

	return ret;
}

static int sdhci_gpio_buf_dump(struct sdhci_host *host)
{
	int ret = -ENOSYS;
	struct sdhci_pci_slot *slot = sdhci_priv(host);

	if (slot->data && slot->data->flis_dump)
		ret = slot->data->flis_dump(slot->data->flis_addr);

	return ret;
}
static const struct sdhci_ops sdhci_pci_ops = {
	.enable_dma	= sdhci_pci_enable_dma,
	.platform_bus_width	= sdhci_pci_bus_width,
	.hw_reset		= sdhci_pci_hw_reset,
	.power_up_host		= sdhci_pci_power_up_host,
	.set_dev_power		= sdhci_pci_set_dev_power,
	.get_cd		= sdhci_pci_get_cd,
	.platform_reset_exit = sdhci_platform_reset_exit,
	.get_tuning_count = sdhci_pci_get_tuning_count,
	.gpio_buf_check = sdhci_gpio_buf_check,
	.gpio_buf_dump = sdhci_gpio_buf_dump,
};

/*****************************************************************************\
 *                                                                           *
 * Suspend/resume                                                            *
 *                                                                           *
\*****************************************************************************/

#ifdef CONFIG_PM

static int sdhci_pci_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sdhci_pci_chip *chip;
	struct sdhci_pci_slot *slot;
	mmc_pm_flag_t slot_pm_flags;
	mmc_pm_flag_t pm_flags = 0;
	int i, ret;

	chip = pci_get_drvdata(pdev);
	if (!chip)
		return 0;

	for (i = 0; i < chip->num_slots; i++) {
		slot = chip->slots[i];
		if (!slot)
			continue;

		ret = sdhci_suspend_host(slot->host);

		if (ret)
			goto err_pci_suspend;

		slot_pm_flags = slot->host->mmc->pm_flags;
		if (slot_pm_flags & MMC_PM_WAKE_SDIO_IRQ)
			sdhci_enable_irq_wakeups(slot->host);

		pm_flags |= slot_pm_flags;
		slot->dma_enabled = false;
	}

	if (chip->fixes && chip->fixes->suspend) {
		ret = chip->fixes->suspend(chip);
		if (ret)
			goto err_pci_suspend;
	}

	return 0;

err_pci_suspend:
	while (--i >= 0)
		sdhci_resume_host(chip->slots[i]->host);
	return ret;
}

static int sdhci_pci_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct sdhci_pci_chip *chip;
	struct sdhci_pci_slot *slot;
	int i, ret;

	chip = pci_get_drvdata(pdev);
	if (!chip)
		return 0;

	if (chip->fixes && chip->fixes->resume) {
		ret = chip->fixes->resume(chip);
		if (ret)
			return ret;
	}

	for (i = 0; i < chip->num_slots; i++) {
		slot = chip->slots[i];
		if (!slot)
			continue;

		ret = sdhci_resume_host(slot->host);
		if (ret)
			return ret;
	}

	return 0;
}

#else /* CONFIG_PM */

#define sdhci_pci_suspend NULL
#define sdhci_pci_resume NULL

#endif /* CONFIG_PM */

#ifdef CONFIG_PM_RUNTIME

static int sdhci_pci_runtime_suspend(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct sdhci_pci_chip *chip;
	struct sdhci_pci_slot *slot;
	int i, ret;

	chip = pci_get_drvdata(pdev);
	if (!chip)
		return 0;

	for (i = 0; i < chip->num_slots; i++) {
		slot = chip->slots[i];
		if (!slot)
			continue;

		ret = sdhci_runtime_suspend_host(slot->host);

		if (ret)
			goto err_pci_runtime_suspend;
	}

	if (chip->fixes && chip->fixes->suspend) {
		ret = chip->fixes->suspend(chip);
		if (ret)
			goto err_pci_runtime_suspend;
	}

	return 0;

err_pci_runtime_suspend:
	while (--i >= 0)
		sdhci_runtime_resume_host(chip->slots[i]->host);
	return ret;
}

static int sdhci_pci_runtime_resume(struct device *dev)
{
	struct pci_dev *pdev = container_of(dev, struct pci_dev, dev);
	struct sdhci_pci_chip *chip;
	struct sdhci_pci_slot *slot;
	int i, ret;

	chip = pci_get_drvdata(pdev);
	if (!chip)
		return 0;

	if (chip->fixes && chip->fixes->resume) {
		ret = chip->fixes->resume(chip);
		if (ret)
			return ret;
	}

	for (i = 0; i < chip->num_slots; i++) {
		slot = chip->slots[i];
		if (!slot)
			continue;

		ret = sdhci_runtime_resume_host(slot->host);
		if (ret)
			return ret;
	}

	return 0;
}

static int sdhci_pci_runtime_idle(struct device *dev)
{
	return 0;
}

#else

#define sdhci_pci_runtime_suspend	NULL
#define sdhci_pci_runtime_resume	NULL
#define sdhci_pci_runtime_idle		NULL

#endif

static const struct dev_pm_ops sdhci_pci_pm_ops = {
	.suspend = sdhci_pci_suspend,
	.resume = sdhci_pci_resume,
	.runtime_suspend = sdhci_pci_runtime_suspend,
	.runtime_resume = sdhci_pci_runtime_resume,
	.runtime_idle = sdhci_pci_runtime_idle,
};

static void sdhci_hsmmc_virtual_detect(void *dev_id, int carddetect)
{
	struct sdhci_host *host = dev_id;

	if (carddetect)
		mmc_detect_change(host->mmc,
			msecs_to_jiffies(DELAY_CARD_INSERTED));
	else
		mmc_detect_change(host->mmc,
			msecs_to_jiffies(DELAY_CARD_REMOVED));
}


/*****************************************************************************\
 *                                                                           *
 * Device probing/removal                                                    *
 *                                                                           *
\*****************************************************************************/

static struct sdhci_pci_slot *sdhci_pci_probe_slot(
	struct pci_dev *pdev, struct sdhci_pci_chip *chip, int first_bar,
	int slotno)
{
	struct sdhci_pci_slot *slot;
	struct sdhci_host *host;
	int ret, bar = first_bar + slotno;

	if (!(pci_resource_flags(pdev, bar) & IORESOURCE_MEM)) {
		dev_err(&pdev->dev, "BAR %d is not iomem. Aborting.\n", bar);
		return ERR_PTR(-ENODEV);
	}

	if (pci_resource_len(pdev, bar) < 0x100) {
		dev_err(&pdev->dev, "Invalid iomem size. You may "
			"experience problems.\n");
	}

	if ((pdev->class & 0x0000FF) == PCI_SDHCI_IFVENDOR) {
		dev_err(&pdev->dev, "Vendor specific interface. Aborting.\n");
		return ERR_PTR(-ENODEV);
	}

	if ((pdev->class & 0x0000FF) > PCI_SDHCI_IFVENDOR) {
		dev_err(&pdev->dev, "Unknown interface. Aborting.\n");
		return ERR_PTR(-ENODEV);
	}

	host = sdhci_alloc_host(&pdev->dev, sizeof(struct sdhci_pci_slot));
	if (IS_ERR(host)) {
		dev_err(&pdev->dev, "cannot allocate host\n");
		return ERR_CAST(host);
	}

	slot = sdhci_priv(host);

	slot->chip = chip;
	slot->host = host;
	slot->pci_bar = bar;
	slot->rst_n_gpio = -EINVAL;
	slot->cd_gpio = -EINVAL;

	host->hw_name = "PCI";
	host->ops = &sdhci_pci_ops;
	host->quirks = chip->quirks;
	host->quirks2 = chip->quirks2;

	/* Retrieve platform data if there is any */
	if (*sdhci_pci_get_data)
		slot->data = sdhci_pci_get_data(pdev, slotno);

	if (slot->data) {
		slot->data->pdev = pdev;
		if (slot->data->setup) {
			ret = slot->data->setup(slot->data);
			if (ret) {
				dev_err(&pdev->dev, "platform setup failed\n");
				goto free;
			}
		}
		slot->rst_n_gpio = slot->data->rst_n_gpio;
		slot->cd_gpio = slot->data->cd_gpio;

		if (slot->data->quirks)
			host->quirks2 |= slot->data->quirks;

		if (slot->data->register_embedded_control)
			slot->data->register_embedded_control(host,
					sdhci_hsmmc_virtual_detect);
	}


	host->irq = pdev->irq;

	ret = pci_request_region(pdev, bar, mmc_hostname(host->mmc));
	if (ret) {
		dev_err(&pdev->dev, "cannot request region\n");
		goto cleanup;
	}

	host->ioaddr = pci_ioremap_bar(pdev, bar);
	if (!host->ioaddr) {
		dev_err(&pdev->dev, "failed to remap registers\n");
		ret = -ENOMEM;
		goto release;
	}

	if (chip->fixes && chip->fixes->probe_slot) {
		ret = chip->fixes->probe_slot(slot);
		if (ret)
			goto unmap;
	}

	if (gpio_is_valid(slot->rst_n_gpio)) {
		if (!gpio_request(slot->rst_n_gpio, "eMMC_reset")) {
			gpio_direction_output(slot->rst_n_gpio, 1);
			slot->host->mmc->caps |= MMC_CAP_HW_RESET;
		} else {
			dev_warn(&pdev->dev, "failed to request rst_n_gpio\n");
			slot->rst_n_gpio = -EINVAL;
		}
	}

	host->mmc->pm_caps = MMC_PM_KEEP_POWER | MMC_PM_WAKE_SDIO_IRQ;
	host->mmc->slotno = slotno;

	if (host->quirks2 & SDHCI_QUIRK2_DISABLE_MMC_CAP_NONREMOVABLE)
		host->mmc->caps &= ~MMC_CAP_NONREMOVABLE;
	host->mmc->caps2 |= MMC_CAP2_NO_PRESCAN_POWERUP;

	if (host->quirks2 & SDHCI_QUIRK2_ENABLE_MMC_PM_IGNORE_PM_NOTIFY)
		host->mmc->pm_flags |= MMC_PM_IGNORE_PM_NOTIFY;

	ret = sdhci_add_host(host);
	if (ret)
		goto remove;

	sdhci_pci_add_own_cd(slot);

	return slot;

remove:
	if (gpio_is_valid(slot->rst_n_gpio))
		gpio_free(slot->rst_n_gpio);

	if (chip->fixes && chip->fixes->remove_slot)
		chip->fixes->remove_slot(slot, 0);

unmap:
	iounmap(host->ioaddr);

release:
	pci_release_region(pdev, bar);

cleanup:
	if (slot->data && slot->data->cleanup)
		slot->data->cleanup(slot->data);

free:
	sdhci_free_host(host);

	return ERR_PTR(ret);
}

static void sdhci_pci_remove_slot(struct sdhci_pci_slot *slot)
{
	int dead;
	u32 scratch;

	sdhci_pci_remove_own_cd(slot);

	dead = 0;
	scratch = readl(slot->host->ioaddr + SDHCI_INT_STATUS);
	if (scratch == (u32)-1)
		dead = 1;

	sdhci_remove_host(slot->host, dead);

	if (gpio_is_valid(slot->rst_n_gpio))
		gpio_free(slot->rst_n_gpio);

	if (slot->chip->fixes && slot->chip->fixes->remove_slot)
		slot->chip->fixes->remove_slot(slot, dead);

	if (slot->data && slot->data->cleanup)
		slot->data->cleanup(slot->data);

	pci_release_region(slot->chip->pdev, slot->pci_bar);

	sdhci_free_host(slot->host);
}

static void sdhci_pci_runtime_pm_allow(struct sdhci_pci_chip *chip)
{
	struct device *dev;

	if (!chip || !chip->pdev)
		return;

	dev = &chip->pdev->dev;
	pm_runtime_put_noidle(dev);
	pm_runtime_allow(dev);
	if (chip->autosuspend_delay)
		pm_runtime_set_autosuspend_delay(dev, chip->autosuspend_delay);
	else
		pm_runtime_set_autosuspend_delay(dev, 50);
	pm_runtime_use_autosuspend(dev);
	pm_suspend_ignore_children(dev, 1);
}

static void sdhci_pci_runtime_pm_forbid(struct device *dev)
{
	pm_runtime_forbid(dev);
	pm_runtime_get_noresume(dev);
}

static int sdhci_pci_probe(struct pci_dev *pdev,
				     const struct pci_device_id *ent)
{
	struct sdhci_pci_chip *chip;
	struct sdhci_pci_slot *slot;

	u8 slots, first_bar;
	int ret, i;

	BUG_ON(pdev == NULL);
	BUG_ON(ent == NULL);

	dev_info(&pdev->dev, "SDHCI controller found [%04x:%04x] (rev %x)\n",
		 (int)pdev->vendor, (int)pdev->device, (int)pdev->revision);

	ret = pci_read_config_byte(pdev, PCI_SLOT_INFO, &slots);
	if (ret)
		return ret;

	slots = PCI_SLOT_INFO_SLOTS(slots) + 1;
	dev_dbg(&pdev->dev, "found %d slot(s)\n", slots);
	if (slots == 0)
		return -ENODEV;

	if (slots > MAX_SLOTS) {
		dev_err(&pdev->dev, "Invalid number of the slots. Aborting.\n");
		return -ENODEV;
	}

	ret = pci_read_config_byte(pdev, PCI_SLOT_INFO, &first_bar);
	if (ret)
		return ret;

	first_bar &= PCI_SLOT_INFO_FIRST_BAR_MASK;

	if (first_bar > 4) {
		dev_err(&pdev->dev, "Invalid first BAR. Aborting.\n");
		return -ENODEV;
	}

	ret = pci_enable_device(pdev);
	if (ret)
		return ret;

	chip = kzalloc(sizeof(struct sdhci_pci_chip), GFP_KERNEL);
	if (!chip) {
		ret = -ENOMEM;
		goto err;
	}

	chip->pdev = pdev;
	chip->fixes = (const struct sdhci_pci_fixes *)ent->driver_data;
	if (chip->fixes) {
		chip->quirks = chip->fixes->quirks;
		chip->quirks2 = chip->fixes->quirks2;
		chip->allow_runtime_pm = chip->fixes->allow_runtime_pm;
	}
	chip->num_slots = slots;

	pci_set_drvdata(pdev, chip);

	if (chip->fixes && chip->fixes->probe) {
		ret = chip->fixes->probe(chip);
		if (ret)
			goto free;
	}

	slots = chip->num_slots;	/* Quirk may have changed this */
	/* slots maybe changed again, so check again */
	if (slots > MAX_SLOTS) {
		dev_err(&pdev->dev, "Invalid number of the slots. Aborting.\n");
		goto free;
	}

	for (i = 0; i < slots; i++) {
		slot = sdhci_pci_probe_slot(pdev, chip, first_bar, i);
		if (IS_ERR(slot)) {
			for (i--; i >= 0; i--)
				sdhci_pci_remove_slot(chip->slots[i]);
			ret = PTR_ERR(slot);
			goto free;
		}

		chip->slots[i] = slot;
	}

	if (chip->allow_runtime_pm)
		sdhci_pci_runtime_pm_allow(chip);

	return 0;

free:
	pci_set_drvdata(pdev, NULL);
	kfree(chip);

err:
	pci_disable_device(pdev);
	return ret;
}

static void sdhci_pci_remove(struct pci_dev *pdev)
{
	int i;
	struct sdhci_pci_chip *chip;

	chip = pci_get_drvdata(pdev);

	if (chip) {
		if (chip->allow_runtime_pm)
			sdhci_pci_runtime_pm_forbid(&pdev->dev);

		for (i = 0; i < chip->num_slots; i++)
			sdhci_pci_remove_slot(chip->slots[i]);

		pci_set_drvdata(pdev, NULL);
		kfree(chip);
	}

	pci_disable_device(pdev);
}

static void sdhci_pci_shutdown(struct pci_dev *pdev)
{
	struct sdhci_pci_chip *chip;
	struct sdhci_pci_slot *slot;
	int i;

	chip = pci_get_drvdata(pdev);

	if (!chip || !chip->pdev)
		return;

	for (i = 0; i < chip->num_slots; i++) {
		slot = chip->slots[i];
		if (slot && slot->data)
			if (slot->data->cleanup)
				slot->data->cleanup(slot->data);
	}

	switch (chip->pdev->device) {
	case PCI_DEVICE_ID_INTEL_CLV_SDIO0:
		for (i = 0; i < chip->num_slots; i++) {
			if (chip->slots[i]->host->flags & SDHCI_POWER_CTRL_DEV)
				ctp_sd_card_power_save(chip->slots[i]);
		}
		break;
	case PCI_DEVICE_ID_INTEL_MRFL_MMC:
		if (chip->allow_runtime_pm) {
			pm_runtime_get_sync(&pdev->dev);
			pm_runtime_disable(&pdev->dev);
			pm_runtime_put_noidle(&pdev->dev);
		}
		break;
	case PCI_DEVICE_ID_INTEL_BYT_EMMC:
	case PCI_DEVICE_ID_INTEL_BYT_EMMC45:
	case PCI_DEVICE_ID_INTEL_BYT_SD:
	case PCI_DEVICE_ID_INTEL_BYT_SDIO:
		if (chip->allow_runtime_pm) {
			pm_runtime_put_sync_suspend(&pdev->dev);
			pm_runtime_disable(&pdev->dev);
		}
		break;
	default:
		break;
	}
}

static struct pci_driver sdhci_driver = {
	.name =		"sdhci-pci",
	.id_table =	pci_ids,
	.probe =	sdhci_pci_probe,
	.remove =	sdhci_pci_remove,
	.shutdown =	sdhci_pci_shutdown,
	.driver =	{
		.pm =   &sdhci_pci_pm_ops
	},
};

module_pci_driver(sdhci_driver);

MODULE_AUTHOR("Pierre Ossman <pierre@ossman.eu>");
MODULE_DESCRIPTION("Secure Digital Host Controller Interface PCI driver");
MODULE_LICENSE("GPL");
