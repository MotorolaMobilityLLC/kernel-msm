/*
 * platform_mmc_sdhci_pci.h: mmc sdhci pci platform data header file
 *
 * (C) Copyright 2012 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MMC_SDHCI_PCI_H_
#define _PLATFORM_MMC_SDHCI_PCI_H_

#define EMMC0_INDEX	0
#define EMMC1_INDEX	1
#define SD_INDEX	2
#define SDIO_INDEX	3

#define MRFLD_GPIO_SDIO_0_CD		77

#define MRFLD_PMIC_VLDOCNT		0xaf
#define MRFLD_PMIC_VLDOCNT_VSWITCH_BIT	0x02
#define MRFLD_PMIC_VLDOCNT_PW_OFF	0xfd

int sdhci_pdata_set_quirks(const unsigned int quirks);
int sdhci_pdata_set_embedded_control(void (*fnp)
			(void *dev_id, void (*virtual_cd)
			(void *dev_id, int card_present)));
#endif

