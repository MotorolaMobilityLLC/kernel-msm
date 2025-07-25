/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Universal Flash Storage Jedec Host Initiated Defrag
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Jinyoung Choi <j-young.choi@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * See the COPYING file in the top-level directory or visit
 * <http://www.gnu.org/licenses/gpl-2.0.html>
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * This program is provided "AS IS" and "WITH ALL FAULTS" and
 * without warranty of any kind. You are solely responsible for
 * determining the appropriateness of using and distributing
 * the program and assume all risks associated with your exercise
 * of rights with respect to the program, including but not limited
 * to infringement of third party rights, the risks and costs of
 * program errors, damage to or loss of data, programs or equipment,
 * and unavailability or interruption of operations. Under no
 * circumstances will the contributor of this Program be liable for
 * any damages of any kind arising from your use or distribution of
 * this program.
 *
 * The Linux Foundation chooses to take subject only to the GPLv2
 * license terms, and distributes only under these terms.
 */

#ifndef _UFS_HID_H_
#define _UFS_HID_H_

#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/blktrace_api.h>
#include <linux/blkdev.h>
#include <linux/bitfield.h>
#include <scsi/scsi_cmnd.h>
#include <linux/delay.h>
#include "block/blk.h"

#define UFS_FEATURE_SUPPORT_HID_BIT		(1 << 13)

#define HID_TRIGGER_WORKER_DELAY_MS_DEFAULT	2000
#define HID_TRIGGER_WORKER_DELAY_MS_MIN		100
#define HID_TRIGGER_WORKER_DELAY_MS_MAX		10000

#define HID_SIZE_DEFAULT			0xFFFFFFFF
#define HID_SIZE_UNIT				4096
#define KB_PER_HID_SIZE_UNIT			(HID_SIZE_UNIT / 1024)

#define RESULT_NOT_DEFRAG_REQUIRED		1
#define AVAIL_ANALYSIS_REQUIRED			0xFFFFFFFF

#define WAIT_HID_RESUME_TIMEOUT			(2 * HZ)

#define HID_DEBUG(hid, msg, args...)					\
	do { if (hid->hid_debug)					\
		pr_err("%40s:%3d [%01d%02d%02d] " msg "\n",		\
		       __func__, __LINE__,				\
		       hid->hid_trigger,				\
		       atomic_read(&hid->ufsf->hba->dev->power.usage_count),\
		       hid->ufsf->hba->clk_gating.active_reqs, ##args);	\
	} while (0)

struct ufshid_offset {
	u8 offset;
};

enum ufshid_idn_indx {
	/* Attribute */
	QUERY_ATTR_IDN_HID_OPERATION,
	QUERY_ATTR_IDN_HID_SIZE,
	QUERY_ATTR_IDN_HID_AVAIL_SIZE,
	QUERY_ATTR_IDN_HID_PROGRESS_RATIO,
	QUERY_ATTR_IDN_HID_STATE,
	HID_IDN_INDX_END,
};

enum UFSHID_STATE {
	HID_NEED_INIT = 0,
	HID_PRESENT = 1,
	HID_SUSPEND = 2,
	HID_FAILED = -2,
	HID_RESET = -3,
};

enum UFSHID_DEV_STATE {
	HID_ANALYSIS_REQUIRED		= 0x0,
	HID_ANALYSIS_IN_PROGRESS	= 0x1,
	HID_DEFRAG_REQUIRED		= 0x2,
	HID_DEFRAG_IN_PROGRESS		= 0x3,
	HID_DEFRAG_COMPLETION		= 0x4,
	HID_DEFRAG_IS_NOT_REQUIRED	= 0x5,
	HID_NUM_DEV_STATES		= 0x6,
};

enum UFSHID_OP {
	HID_OP_DISABLE		= 0,
	HID_OP_ANALYZE		= 1,
	HID_OP_EXECUTE		= 2,
	HID_OP_MAX
};

struct ufshid_dev {
	struct ufsf_feature *ufsf;

	unsigned int hid_trigger;   /* default value is false */
	struct delayed_work hid_trigger_work;
	unsigned int hid_trigger_delay;

	u32 ahit;			/* to restore ahit value */
	bool is_auto_enabled;

	u32 hid_size;

	/* for sysfs */
	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufshid_sysfs_entry *sysfs_entries;

	bool is_analyze;

	/* for debug */
	bool hid_debug;
#ifdef CONFIG_UFS_JEDEC_HID_POC
	bool block_suspend;
#endif
	struct completion resume_compl;
};

struct ufshid_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct ufshid_dev *hid, char *buf);
	ssize_t (*store)(struct ufshid_dev *hid, const char *buf, size_t count);
};

struct ufshcd_lrb;

int ufshid_get_state(struct ufsf_feature *ufsf);
void ufshid_set_state(struct ufsf_feature *ufsf, int state);
void ufshid_get_dev_info(struct ufsf_feature *ufsf);
void ufshid_set_init_state(struct ufsf_feature *ufsf);
void ufshid_init(struct ufsf_feature *ufsf);
void ufshid_reset(struct ufsf_feature *ufsf);
void ufshid_reset_host(struct ufsf_feature *ufsf);
void ufshid_remove(struct ufsf_feature *ufsf);
void ufshid_suspend(struct ufsf_feature *ufsf, bool is_system_pm);
void ufshid_resume(struct ufsf_feature *ufsf);
#endif /* End of Header */
