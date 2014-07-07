/*
 * platform_sst_pci.c: SST platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author:  Dharageswari R <dharageswari.r@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <linux/delay.h>
#include <linux/intel_mid_dma.h>
#include <asm/intel-mid.h>
#include <asm/platform_sst.h>

#define CTP_SSP_BASE 0xffa23000
#define CTP_DMA_BASE 0xffaf8000
#define MRFLD_SSP_BASE 0xff2a0000
#define MRFLD_DMA_BASE 0xff298000
#define CTP_MAX_CONFIG_SIZE 500

#define SST_CTP_IRAM_START	0
#define SST_CTP_IRAM_END	0x80000
#define SST_CTP_DRAM_START	0x400000
#define SST_CTP_DRAM_END	0x480000
#define SSP_SIZE 0x1000
#define DMA_SIZE_CTP 0x1000
#define DMA_SIZE_MRFLD 0x4000
#define SST_CHECKPOINT_OFFSET 0x1C00
#define SST_CHECKPOINT_OFFSET_MRFLD 0x0C00
#define CHECKPOINT_DUMP_SZ 256

#define SST_V1_MAILBOX_RECV	0x800
#define SST_V2_MAILBOX_RECV	0x400

#define MRFLD_FW_LSP_DDR_BASE 0xC5E00000
#define MRFLD_FW_MOD_END (MRFLD_FW_LSP_DDR_BASE + 0x1FFFFF)
#define MRFLD_FW_MOD_TABLE_OFFSET 0x80000
#define MRFLD_FW_MOD_TABLE_SIZE 0x100

struct sst_platform_info sst_data;

static struct sst_ssp_info ssp_inf_mrfld = {
	.base_add = MRFLD_SSP_BASE,
	.gpio_in_use = false,
};

static struct sst_platform_config_data sst_mrfld_pdata = {
	.sst_dma_base[0] = MRFLD_DMA_BASE,
	.sst_dma_base[1] = 0x0,
};

static const struct sst_info mrfld_sst_info = {
	.iram_start = 0,
	.iram_end = 0,
	.iram_use = false,
	.dram_start = 0,
	.dram_end = 0,
	.dram_use = false,
	.imr_start = 0,
	.imr_end = 0,
	.imr_use = false,
	.mailbox_start = 0,
	.use_elf = true,
	.lpe_viewpt_rqd = false,
	.max_streams = MAX_NUM_STREAMS_MRFLD,
	.dma_max_len = SST_MAX_DMA_LEN_MRFLD,
	.num_probes = 16,
};

static struct sst_platform_debugfs_data mrfld_debugfs_data = {
	.ssp_reg_size = SSP_SIZE,
	.dma_reg_size = DMA_SIZE_MRFLD,
	.num_ssp = 3,
	.num_dma = 2,
	.checkpoint_offset = SST_CHECKPOINT_OFFSET_MRFLD,
	.checkpoint_size = CHECKPOINT_DUMP_SZ,
};

static const struct sst_ipc_info mrfld_ipc_info = {
	.use_32bit_ops = false,
	.ipc_offset = 0,
	.mbox_recv_off = SST_V2_MAILBOX_RECV,
};

static const struct sst_lib_dnld_info  mrfld_lib_dnld_info = {
	.mod_base           = MRFLD_FW_LSP_DDR_BASE,
	.mod_end            = MRFLD_FW_MOD_END,
	.mod_table_offset   = MRFLD_FW_MOD_TABLE_OFFSET,
	.mod_table_size     = MRFLD_FW_MOD_TABLE_SIZE,
	.mod_ddr_dnld       = true,
};

static void set_mrfld_sst_config(struct sst_platform_info *sst_info)
{
	sst_info->ssp_data = &ssp_inf_mrfld;
	sst_info->pdata = &sst_mrfld_pdata;
	sst_info->bdata = NULL;
	sst_info->probe_data = &mrfld_sst_info;
	sst_info->ipc_info = &mrfld_ipc_info;
	sst_info->debugfs_data = &mrfld_debugfs_data;
	sst_info->lib_info = &mrfld_lib_dnld_info;
	/* By default set recovery to true for all mrfld based devices */
	sst_info->enable_recovery = 1;

	return ;

}

static struct sst_platform_info *get_sst_platform_data(struct pci_dev *pdev)
{
	struct sst_platform_info *sst_pinfo = NULL;

	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_SST_MRFLD:
	case PCI_DEVICE_ID_INTEL_SST_MOOR:
		set_mrfld_sst_config(&sst_data);
		sst_pinfo = &sst_data;
		break;
	default:
		return NULL;
	}
	return sst_pinfo;
}

static void sst_pci_early_quirks(struct pci_dev *pci_dev)
{
	pci_dev->dev.platform_data = get_sst_platform_data(pci_dev);
}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_SST_MRFLD,
							sst_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_SST_MOOR,
							sst_pci_early_quirks);
