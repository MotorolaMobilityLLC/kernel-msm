/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Description: CoreSight TMC PCIe driver
 */

#ifndef _CORESIGHT_TMC_PCIE_H
#define _CORESIGHT_TMC_PCIE_H

#include <linux/ipa_qdss.h>
#include <linux/msm_mhi_dev.h>

#define TMC_ETR_BAM_PIPE_INDEX	0
#define TMC_ETR_BAM_NR_PIPES	2
#define TMC_PCIE_MEM_SIZE     0x400000
#define PCIE_BLK_SIZE 4096

enum tmc_pcie_path {
	TMC_PCIE_SW_PATH,
	TMC_PCIE_HW_PATH,
};

static const char * const str_tmc_pcie_path[] = {
	[TMC_PCIE_SW_PATH]	= "sw",
	[TMC_PCIE_HW_PATH]	= "hw",
};


struct tmc_ipa_data {
	struct ipa_qdss_conn_out_params ipa_qdss_out;
	struct ipa_qdss_conn_in_params  ipa_qdss_in;
};

struct tmc_pcie_data {
	struct device		*dev;
	struct tmc_ipa_data	*ipa_data;
	enum tmc_pcie_path	pcie_path;
	struct tmc_usb_bam_data	*bamdata;
	struct tmc_drvdata	*tmcdrvdata;
	struct byte_cntr	*byte_cntr_data;
	struct coresight_csr	*csr;
	int			irqctrl_offset;
	struct mutex		pcie_lock;
	atomic_t		irq_cnt;
	loff_t			offset;
	bool			pcie_chan_opened;
	wait_queue_head_t	pcie_wait_wq;
	u32			pcie_out_chan;
	struct mhi_dev_client	*out_handle;
	struct work_struct	pcie_open_work;
	struct work_struct	pcie_write_work;
	struct workqueue_struct	*pcie_wq;
	void (*event_notifier)(struct mhi_dev_client_cb_reason *cb);
	bool			enable_to_bam;
	u32				buf_size;
	uint64_t		total_size;
	uint64_t		total_irq;
};

int tmc_pcie_init(struct amba_device *adev,
			struct tmc_drvdata *drvdata);
int tmc_pcie_enable(struct tmc_pcie_data *pcie_data);
void tmc_pcie_disable(struct tmc_pcie_data *pcie_data);

#endif
