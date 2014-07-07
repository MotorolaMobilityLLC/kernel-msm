/*
 * platform_mmc_sdhci_pci.c: mmc sdhci pci platform data initilization file
 *
 * (C) Copyright 2012 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <asm/intel-mid.h>
#include <linux/mmc/sdhci-pci-data.h>
#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <linux/delay.h>
#include <asm/intel_scu_ipc.h>
#include <asm/intel_scu_pmic.h>
#include <linux/intel_mid_pm.h>
#include <linux/hardirq.h>
#include <linux/mmc/sdhci.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>

#include "platform_sdhci_pci.h"

#ifdef CONFIG_ATOM_SOC_POWER
static int panic_mode_emmc0_power_up(void *data)
{
	int ret;
	bool atomic_context;
	/*
	 * Since pmu_set_emmc_to_d0i0_atomic function can
	 * only be used in atomic context, before call this
	 * function, do a check first and make sure this function
	 * is used in atomic context.
	 */
	atomic_context = (!preemptible() || in_atomic_preempt_off());

	if (!atomic_context) {
		pr_err("%s: not in atomic context!\n", __func__);
		return -EPERM;
	}

	ret = pmu_set_emmc_to_d0i0_atomic();
	if (ret) {
		pr_err("%s: power up host failed with err %d\n",
				__func__, ret);
	}

	return ret;
}
#else
static int panic_mode_emmc0_power_up(void *data)
{
	return 0;
}
#endif

static unsigned int sdhci_pdata_quirks =
	SDHCI_QUIRK2_ENABLE_MMC_PM_IGNORE_PM_NOTIFY;

int sdhci_pdata_set_quirks(const unsigned int quirks)
{
	sdhci_pdata_quirks = quirks;
	return 0;
}

static int mrfl_sdio_setup(struct sdhci_pci_data *data);
static void mrfl_sdio_cleanup(struct sdhci_pci_data *data);

static void (*sdhci_embedded_control)(void *dev_id, void (*virtual_cd)
					(void *dev_id, int card_present));

/*****************************************************************************\
 *                                                                           *
 *  Regulator declaration for WLAN SDIO card                                 *
 *                                                                           *
\*****************************************************************************/

#define DELAY_ONOFF 250

static struct regulator_consumer_supply wlan_vmmc_supply = {
	.supply		= "vmmc",
};

static struct regulator_init_data wlan_vmmc_data = {
	.constraints = {
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies = &wlan_vmmc_supply,
};

static struct fixed_voltage_config vwlan = {
	.supply_name		= "vwlan",
	.microvolts		= 1800000,
	.gpio			= -EINVAL,
	.startup_delay		= 1000 * DELAY_ONOFF,
	.enable_high		= 1,
	.enabled_at_boot	= 0,
	.init_data		= &wlan_vmmc_data,
};

static void vwlan_device_release(struct device *dev) {}

static struct platform_device vwlan_device = {
	.name		= "reg-fixed-voltage",
	.id		= PLATFORM_DEVID_AUTO,
	.dev = {
		.platform_data	= &vwlan,
		.release = vwlan_device_release,
	},
};

#define TNG_EMMC_0_FLIS_ADDR		0xff0c0900
#define TNG_EMMC_FLIS_SLEW		0x00000400
#define TNG_EMMC_0_CLK_PULLDOWN		0x00000200

static int mrfl_flis_slew_change(void __iomem *flis_addr, int slew)
{
	unsigned int reg;
	int i;

	/*
	 * Change TNG gpio FLIS settings for all eMMC0
	 * CLK/CMD/DAT pins.
	 * That is, including emmc_0_clk, emmc_0_cmd,
	 * emmc_0_d_0, emmc_0_d_1, emmc_0_d_2, emmc_0_d_3,
	 * emmc_0_d_4, emmc_0_d_5, emmc_0_d_6, emmc_0_d_7
	 */
	for (i = 0; i < 10; i++) {
		reg = readl(flis_addr + (i * 4));
		if (slew)
			reg |= TNG_EMMC_FLIS_SLEW; /* SLEW B */
		else
			reg &= ~TNG_EMMC_FLIS_SLEW; /* SLEW A */
		writel(reg, flis_addr + (i * 4));
	}

	/* Disable PullDown for emmc_0_clk */
	reg = readl(flis_addr);
	reg &= ~TNG_EMMC_0_CLK_PULLDOWN;
	writel(reg, flis_addr);

	return 0;
}

static int mrfl_flis_dump(void __iomem *addr)
{
	int i, ret = 0;
	unsigned int reg;

	if (addr) {
		/*
		 * Dump TNG gpio FLIS settings for all eMMC0
		 * CLK/CMD/DAT pins.
		 * That is, including emmc_0_clk, emmc_0_cmd,
		 * emmc_0_d_0, emmc_0_d_1, emmc_0_d_2, emmc_0_d_3,
		 * emmc_0_d_4, emmc_0_d_5, emmc_0_d_6, emmc_0_d_7
		 */
		for (i = 0; i < 10; i++) {
			reg = readl(addr + (i * 4));
			pr_err("emmc0 FLIS reg[%d] dump: 0x%08x\n", i, reg);
		}
	}

	return ret;
}

/* Board specific setup related to eMMC goes here */
static int mrfl_emmc_setup(struct sdhci_pci_data *data)
{
	struct pci_dev *pdev = data->pdev;
	int ret = 0;

	data->flis_addr = ioremap_nocache(TNG_EMMC_0_FLIS_ADDR, 64);
	if (!data->flis_addr) {
		pr_err("emmc0 FLIS addr ioremap failed!\n");
		ret = -ENOMEM;
	} else {
		pr_info("emmc0 mapped FLIS addr: %p\n", data->flis_addr);
		if (pdev->revision == 0x01) /* TNB B0 stepping */
			/* HS200 FLIS slew setting */
			ret = mrfl_flis_slew_change(data->flis_addr, 1);
	}

	return ret;
}

/* Board specific cleanup related to eMMC goes here */
static void mrfl_emmc_cleanup(struct sdhci_pci_data *data)
{
	if (data->flis_addr)
		iounmap(data->flis_addr);
}

/* Board specific setup related to SD goes here */
static int mrfl_sd_setup(struct sdhci_pci_data *data)
{
	u8 vldocnt = 0;
	int err;

	/*
	 * Change necessary GPIO pin mode for SD card working.
	 * This is something should be done in IA firmware.
	 * But, anyway, just do it here in case IA firmware
	 * forget to do so.
	 */
	lnw_gpio_set_alt(MRFLD_GPIO_SDIO_0_CD, 0);

	err = intel_scu_ipc_ioread8(MRFLD_PMIC_VLDOCNT, &vldocnt);
	if (err) {
		pr_err("PMIC vldocnt IPC read error: %d\n", err);
		return err;
	}

	vldocnt |= MRFLD_PMIC_VLDOCNT_VSWITCH_BIT;
	err = intel_scu_ipc_iowrite8(MRFLD_PMIC_VLDOCNT, vldocnt);
	if (err) {
		pr_err("PMIC vldocnt IPC write error: %d\n", err);
		return err;
	}
	msleep(20);

	return 0;
}

/* Board specific cleanup related to SD goes here */
static void mrfl_sd_cleanup(struct sdhci_pci_data *data)
{
	u8 vldocnt = 0;
	int err;

	err = intel_scu_ipc_ioread8(MRFLD_PMIC_VLDOCNT, &vldocnt);
	if (err) {
		pr_err("PMIC vldocnt IPC read error: %d\n", err);
		return;
	}

	vldocnt &= MRFLD_PMIC_VLDOCNT_PW_OFF;
	err = intel_scu_ipc_iowrite8(MRFLD_PMIC_VLDOCNT, vldocnt);
	if (err)
		pr_err("PMIC vldocnt IPC write error: %d\n", err);

	return;
}

/* Board specific setup related to SDIO goes here */
static int mrfl_sdio_setup(struct sdhci_pci_data *data)
{
	struct pci_dev *pdev = data->pdev;
	/* Control card power through a regulator */
	wlan_vmmc_supply.dev_name = dev_name(&pdev->dev);
	vwlan.gpio = get_gpio_by_name("WLAN-enable");
	if (vwlan.gpio < 0)
		pr_err("%s: No WLAN-enable GPIO in SFI table\n",
	       __func__);
	pr_info("vwlan gpio %d\n", vwlan.gpio);
	/* add a regulator to control wlan enable gpio */
	if (platform_device_register(&vwlan_device))
		pr_err("regulator register failed\n");
	else
		sdhci_pci_request_regulators();

	return 0;
}

/* Board specific cleanup related to SDIO goes here */
static void mrfl_sdio_cleanup(struct sdhci_pci_data *data)
{
}

/* MRFL platform data */
static struct sdhci_pci_data mrfl_sdhci_pci_data[] = {
	[EMMC0_INDEX] = {
			.pdev = NULL,
			.slotno = EMMC0_INDEX,
			.rst_n_gpio = -EINVAL,
			.cd_gpio = -EINVAL,
			.quirks = 0,
			.platform_quirks = 0,
			.setup = mrfl_emmc_setup,
			.cleanup = mrfl_emmc_cleanup,
			.power_up = panic_mode_emmc0_power_up,
			.flis_dump = mrfl_flis_dump,
	},
	[SD_INDEX] = {
			.pdev = NULL,
			.slotno = SD_INDEX,
			.rst_n_gpio = -EINVAL,
			.cd_gpio = 77,
			.quirks = 0,
			.platform_quirks = 0,
			.setup = mrfl_sd_setup,
			.cleanup = mrfl_sd_cleanup,
	},
	[SDIO_INDEX] = {
			.pdev = NULL,
			.slotno = SDIO_INDEX,
			.rst_n_gpio = -EINVAL,
			.cd_gpio = -EINVAL,
			.quirks = 0,
			.platform_quirks = 0,
			.setup = mrfl_sdio_setup,
			.cleanup = mrfl_sdio_cleanup,
	},
};

/* Moorefield platform data */
static struct sdhci_pci_data moor_sdhci_pci_data[] = {
	[EMMC0_INDEX] = {
			.pdev = NULL,
			.slotno = 0,
			.rst_n_gpio = -EINVAL,
			.cd_gpio = -EINVAL,
			.quirks = 0,
			.platform_quirks = 0,
			.setup = 0,
			.cleanup = 0,
			.power_up = panic_mode_emmc0_power_up,
	},
	[SD_INDEX] = {
			.pdev = NULL,
			.slotno = 0,
			.rst_n_gpio = -EINVAL,
			.cd_gpio = 77,
			.quirks = 0,
			.platform_quirks = 0,
			.setup = mrfl_sd_setup,
			.cleanup = mrfl_sd_cleanup,
			.power_up = 0,
	},
	[SDIO_INDEX] = {
			.pdev = NULL,
			.slotno = 0,
			.rst_n_gpio = -EINVAL,
			.cd_gpio = -EINVAL,
			.quirks = 0,
			.platform_quirks = 0,
			.setup = mrfl_sdio_setup,
			.cleanup = mrfl_sdio_cleanup,
			.power_up = 0,
	},
};

static struct sdhci_pci_data *get_sdhci_platform_data(struct pci_dev *pdev)
{
	struct sdhci_pci_data *pdata = NULL;

	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_MRFL_MMC:
		switch (PCI_FUNC(pdev->devfn)) {
		case 0:
			pdata = &mrfl_sdhci_pci_data[EMMC0_INDEX];
			break;
		case 1:
			pdata = &mrfl_sdhci_pci_data[EMMC1_INDEX];
			break;
		case 2:
			pdata = &mrfl_sdhci_pci_data[SD_INDEX];
			break;
		case 3:
			pdata = &mrfl_sdhci_pci_data[SDIO_INDEX];
			break;
		default:
			pr_err("%s func %s: Invalid PCI Dev func no. (%d)\n",
				__FILE__, __func__, PCI_FUNC(pdev->devfn));
			break;
		}
		break;
	case PCI_DEVICE_ID_INTEL_MOOR_EMMC:
		pdata = &moor_sdhci_pci_data[EMMC0_INDEX];
		break;
	case PCI_DEVICE_ID_INTEL_MOOR_SD:
		pdata = &moor_sdhci_pci_data[SD_INDEX];
		break;
	case PCI_DEVICE_ID_INTEL_MOOR_SDIO:
		pdata = &moor_sdhci_pci_data[SDIO_INDEX];
		pdata->quirks = sdhci_pdata_quirks;
		break;
	default:
		break;
	}
	return pdata;
}

int sdhci_pdata_set_embedded_control(void (*fnp)
			(void *dev_id, void (*virtual_cd)
			(void *dev_id, int card_present)))
{
	WARN_ON(sdhci_embedded_control);
	sdhci_embedded_control = fnp;
	return 0;
}

struct sdhci_pci_data *mmc_sdhci_pci_get_data(struct pci_dev *pci_dev, int slotno)
{
	return get_sdhci_platform_data(pci_dev);
}

static int __init init_sdhci_get_data(void)
{
	sdhci_pci_get_data = mmc_sdhci_pci_get_data;

	return 0;
}

arch_initcall(init_sdhci_get_data);

