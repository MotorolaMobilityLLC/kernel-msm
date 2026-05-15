/* SPDX-License-Identifier: GPL-2.0-only */
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _QCOM_BWPROF_H
#define _QCOM_BWPROF_H

#include <linux/mailbox_client.h>

#define MIN_MS	1
#define MAX_MS	2000
#define MAX_MASTERS	3
#define MAX_BUCKETS	4
#define SAMPLING_1MS	1
#define SAMPLING_10MS	10
#define SAMPLING_100MS	100
#define SAMPLING_MS_GRANULARITY	50
#define MAX_NUM_SAMPLES	100
#define MAX_USER_BUCKETS	3
#define MAX_HIST_SAMPLES	10
#define SAMPLE_SIZE		(MAX_BUCKETS * MAX_MASTERS)
#define SCMI_VENDOR_MSG_MODULE_START 16

enum scmi_bwprof_cmd {
	BWPROF_SET_LOG_LEVEL = SCMI_VENDOR_MSG_MODULE_START,
	BWPROF_SET_SAMPLE_MS,
	BWPROF_MASTER_LIST,
	BWPROF_SET_ENABLE,
	BWPROF_SET_HIST_INFO
};

enum llcc_masters {
	LLCC_CPU = 0,
	LLCC_GPU,
	LLCC_TOTAL,
	LLCC_CAMERA,
	LLCC_DPU,
	LLCC_EVA,
	LLCC_VPU,
	LLCC_PCIe,
	MAX_LLCC_MASTERS
};

enum ddr_masters {
	DDR_CPU = 50,
	DDR_GPU,
	DDR_TOTAL,
	DDR_CAMERA,
	DDR_DPU,
	DDR_EVA,
	DDR_VPU,
	DDR_PCIe,
	MAX_DDR_MASTERS
};

enum bwprof_hw_type {
	BWPROF_DDR,
	BWPROF_LLCC,
	BWPROF_TOTAL_HW
};

enum bwprof_type {
	BWPROF_DEV,
	BWPROF_HW,
	BWPROF_SAMPLING,
	NUM_BWPROF_TYPES
};

enum bwprof_sampling_mode_type {
	BWPROF_1MS,
	BWPROF_10MS,
	BWPROF_100MS,
	BWPROF_HIST,
	TOTAL_SAMPLING_MODE_TYPES
};

struct bwprof_monitor_data {
	uint64_t ts;
	uint32_t mem_freq;
	uint32_t meas_mbps[MAX_MASTERS];
} __packed;

struct bwprof_hist_data {
	uint64_t ts;
	uint8_t sample[SAMPLE_SIZE];
} __packed;

struct bwprof_spec {
	enum bwprof_type type;
};

struct master_info {
	uint8_t cnt;
	uint8_t masters[MAX_MASTERS];
} __packed;

struct bucket_info {
	uint32_t buckets[MAX_BUCKETS];
} __packed;

struct sample_ms_info {
	uint8_t hist;
	uint16_t sample_ms;
} __packed;

struct sampling_mode_info {
	u16 sampling_ms;
	u8	enable;
	u8	num_masters;
	u8	*masters_list;
	u8	bucket_cnt;
	u32	buckets[MAX_USER_BUCKETS];
};

struct bwprof_hw_group {
	struct device	*dev;
	enum bwprof_hw_type	hw_type;
	struct sampling_mode_info	*mode[TOTAL_SAMPLING_MODE_TYPES];
	struct sampling_mode_info	*default_mode_val[TOTAL_SAMPLING_MODE_TYPES];
	struct config_group	ls_group;
	u32	bus_width;
	u32	sampling_cnt;
	u32	num_inited_samp_mode;
};

struct bwprof_dev_data {
	struct device	*dev;
	struct bwprof_hw_group	*hw_node[BWPROF_TOTAL_HW];
	struct config_group	*dev_group;
	const struct qcom_scmi_vendor_ops	*bwprof_ops;
	struct scmi_protocol_handle	*ph;
	struct smci_object bwprof_profiler;
	struct mbox_client	cl;
	struct mbox_chan	*ch;
	spinlock_t rx_lock;
	u32	hw_cnt;
	u32	num_inited_hw;
	u8	num_masters;
	u16	sample_ms;
	u8	log_level;
	char set_config_str[256];
	u8	masters_list[MAX_MASTERS];
	bool	inited;
	bool	is_sampling_enable;
	bool	is_hist_enable;
	bool	is_set_config;
};

#endif /* _QCOM_BWPROF_H */
