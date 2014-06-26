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

static struct sst_ssp_info ssp_inf_ctp = {
	.base_add = CTP_SSP_BASE,
	.gpio = {
		.alt_function = LNW_ALT_2,
	},
	.gpio_in_use = true,
};

static struct sst_ssp_info ssp_inf_mrfld = {
	.base_add = MRFLD_SSP_BASE,
	.gpio_in_use = false,
};

static const struct sst_platform_config_data sst_ctp_pdata = {
	.sst_sram_buff_base = 0xfffc0000,
	.sst_dma_base[0] = CTP_DMA_BASE,
	.sst_dma_base[1] = 0x0,
};

static struct sst_platform_config_data sst_mrfld_pdata = {
	.sst_dma_base[0] = MRFLD_DMA_BASE,
	.sst_dma_base[1] = 0x0,
};

static const struct sst_board_config_data sst_ctp_bdata = {
	.active_ssp_ports = 4,
	.platform_id = 2,/*FIXME: Once the firmware fix is available*/
	.board_id = 1,/*FIXME: Once the firmware fix is available*/
	.ihf_num_chan = 2,
	.osc_clk_freq = 19200000,
	.ssp_platform_data = {
		[SST_SSP_AUDIO] = {
				.ssp_cfg_sst = 1,
				.port_number = 3,
				.is_master = 1,
				.pack_mode = 1,
				.num_slots_per_frame = 2,
				.num_bits_per_slot = 25,
				.active_tx_map = 3,
				.active_rx_map = 3,
				.ssp_frame_format = 3,
				.frame_polarity = 0,
				.serial_bitrate_clk_mode = 0,
				.frame_sync_width = 24,
				.dma_handshake_interface_tx = 5,
				.dma_handshake_interface_rx = 4,
				.ssp_base_add = 0xFFA23000,
		},
		[SST_SSP_MODEM] = {0},
		[SST_SSP_BT] = {0},
		[SST_SSP_FM] = {0},
	},
};

static const struct sst_info ctp_sst_info = {
	.iram_start = SST_CTP_IRAM_START,
	.iram_end = SST_CTP_IRAM_END,
	.iram_use = true,
	.dram_start = SST_CTP_DRAM_START,
	.dram_end = SST_CTP_DRAM_END,
	.dram_use = true,
	.imr_start = 0,
	.imr_end = 0,
	.imr_use = false,
	.mailbox_start = 0,
	.lpe_viewpt_rqd = false,
	.use_elf = false,
	.max_streams = MAX_NUM_STREAMS_CTP,
	.dma_max_len = (SST_MAX_DMA_LEN * 4),
	.num_probes = 1,
};

static const struct sst_ipc_info ctp_ipc_info = {
	.use_32bit_ops = true,
	.ipc_offset = 0,
	.mbox_recv_off = SST_V1_MAILBOX_RECV,
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

static struct sst_platform_debugfs_data ctp_debugfs_data = {
	.ssp_reg_size = SSP_SIZE,
	.dma_reg_size = DMA_SIZE_CTP,
	.num_ssp = 1,
	.num_dma = 1,
	.checkpoint_offset = SST_CHECKPOINT_OFFSET,
	.checkpoint_size = CHECKPOINT_DUMP_SZ,
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

static int set_ctp_sst_config(struct sst_platform_info *sst_info)
{
	unsigned int conf_len;

	ssp_inf_ctp.gpio.i2s_rx_alt = get_gpio_by_name("gpio_i2s3_rx");
	ssp_inf_ctp.gpio.i2s_tx_alt = get_gpio_by_name("gpio_i2s3_rx");
	ssp_inf_ctp.gpio.i2s_frame = get_gpio_by_name("gpio_i2s3_fs");
	ssp_inf_ctp.gpio.i2s_clock = get_gpio_by_name("gpio_i2s3_clk");

	sst_info->ssp_data = &ssp_inf_ctp;
	conf_len = sizeof(sst_ctp_pdata) + sizeof(sst_ctp_bdata);
	if (conf_len > CTP_MAX_CONFIG_SIZE)
		return -EINVAL;
	sst_info->pdata = &sst_ctp_pdata;
	sst_info->bdata = &sst_ctp_bdata;
	sst_info->probe_data = &ctp_sst_info;
	sst_info->ipc_info = &ctp_ipc_info;
	sst_info->debugfs_data = &ctp_debugfs_data;
	sst_info->lib_info = NULL;
	sst_info->enable_recovery = 0;

	return 0;
}

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
	/* Disable recovery for MOFD V1 */
	if (INTEL_MID_BOARD(2, PHONE, MOFD, V1, PRO) ||
		 INTEL_MID_BOARD(2, PHONE, MOFD, V1, ENG))
		sst_info->enable_recovery = 0;

	return ;

}

static struct sst_platform_info *get_sst_platform_data(struct pci_dev *pdev)
{
	int ret;
	struct sst_platform_info *sst_pinfo = NULL;

	switch (pdev->device) {
	case PCI_DEVICE_ID_INTEL_SST_CLV:
		ret = set_ctp_sst_config(&sst_data);
		if (ret < 0)
			return NULL;
		sst_pinfo = &sst_data;
		break;
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

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_SST_CLV,
							sst_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_SST_MRFLD,
							sst_pci_early_quirks);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_SST_MOOR,
							sst_pci_early_quirks);
