/*
 * Universal Flash Storage File Base Optimization
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * Author:
 *	Keoseong Park <keosung.park@samsung.com>
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

#ifndef _UFSFBO_H_
#define _UFSFBO_H_

#include <linux/interrupt.h>
#include <linux/sysfs.h>
#include <linux/blktrace_api.h>
#include <linux/blkdev.h>
#include <linux/bitfield.h>
#include <scsi/scsi_cmnd.h>

#include "../../../block/blk.h"

#define UFSFBO_VER				0x0100
#define UFSFBO_DD_VER				0x010100
#define UFSFBO_DD_VER_POST			""

#define UFS_FEATURE_SUPPORT_FBO_BIT		BIT(17)

#define FBO_TRIGGER_WORKER_DELAY_MS_DEFAULT	500
#define FBO_TRIGGER_WORKER_DELAY_MS_MIN		100
#define FBO_TRIGGER_WORKER_DELAY_MS_MAX		10000

#define FBO_EXECUTE_THRESHOLD_DEFAULT		0x02

#define FBO_REQ_TIMEOUT				(10 * HZ)
#define FBO_MAX_RANGE_COUNT			(1 << 8)

#define FBO_MAX_EXECUTE_THRESHOLD		0xA

#define WRITE_BUFFER_DATA_MODE			0x2
#define WRITE_BUFFER_ID				0x1
#define READ_BUFFER_DATA_MODE			0x2
#define READ_BUFFER_ID				0x2

#define BYTE_TO_BLK_SHIFT			12

#define RUN_OPTIMIZATION			1

#define FBO_DEBUG(fbo, msg, args...)					\
	do { if (fbo->fbo_debug)					\
		pr_err("%40s:%3d [%01d%02d%02d] " msg "\n",		\
		       __func__, __LINE__,				\
		       fbo->fbo_trigger,				\
		       atomic_read(&fbo->ufsf->hba->dev->power.usage_count),\
		       fbo->ufsf->hba->clk_gating.active_reqs, ##args);	\
	} while (0)

enum UFSFBO_STATE {
	FBO_NEED_INIT = 0,
	FBO_PRESENT = 1,
	FBO_SUSPEND = 2,
	FBO_FAILED = -2,
	FBO_RESET = -3,
};

enum UFSFBO_OP {
	FBO_OP_DISABLE	= 0,
	FBO_OP_ANALYZE	= 1,
	FBO_OP_EXECUTE	= 2,
	FBO_OP_MAX
};

enum UFSFBO_OP_STATE {
	FBO_OP_STATE_IDLE		= 0x00,
	FBO_OP_STATE_ON_GOING		= 0x01,
	FBO_OP_STATE_COMPL_ANALYSIS	= 0x02,
	FBO_OP_STATE_COMPL_OPTIMIZATION	= 0x03,
	FBO_OP_STATE_ERROR		= 0xFF,
};

struct ufsfbo_wb_entry {
	__be32 start_lba;
	__be32 length:24;
	__be32 reserved:8;
} __packed;

struct ufsfbo_wb_body {
	__u8 version;
	__u8 num_buffer_entries;
	__u8 cal_all_ranges;
	__u8 reserved[5];
};

struct ufsfbo_rb_entry {
	__be32 start_lba;
	__be32 length:24;
	__be32 reg_level:8;
} __packed;

struct ufsfbo_rb_body {
	__u8 version;
	__u8 num_buffer_entries;
	__u8 cal_all_ranges;
	__u8 all_reg_level;
	__u8 reserved[4];
};

struct ufsfbo_buffer_header {
	__u8 fbo_type;
	__u8 reserved[3];
};

struct ufsfbo_req {
	int lun;
	u8 buf[PAGE_SIZE];
	size_t buf_size;
};

struct ufsfbo_dev {
	struct ufsf_feature *ufsf;

	unsigned int fbo_trigger;
	struct delayed_work fbo_trigger_work;
	unsigned int fbo_trigger_delay;

	u32 ahit;			/* to restore ahit value */
	bool is_auto_enabled;

	bool analysis_only;

	u32 rec_lba_range_size;
	u32 max_lba_range_size;
	u32 min_lba_range_size;
	u8 max_lba_range_count;
	u16 lba_range_alignment;

	u8 execute_threshold;

	struct ufsfbo_req write_buffer;
	struct ufsfbo_req read_buffer;

#if defined(CONFIG_UFSFBO_POC)
	bool get_pm;
#endif

	struct mutex trigger_lock;

	/* for sysfs */
	struct kobject kobj;
	struct mutex sysfs_lock;
	struct ufsfbo_sysfs_entry *sysfs_entries;

	/* for debug */
	bool fbo_debug;
};

struct ufsfbo_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct ufsfbo_dev *fbo, char *buf);
	ssize_t (*store)(struct ufsfbo_dev *fbo, const char *buf, size_t count);
};

void ufsfbo_init(struct ufsf_feature *ufsf);
void ufsfbo_reset(struct ufsf_feature *ufsf);
void ufsfbo_reset_host(struct ufsf_feature *ufsf);
void ufsfbo_remove(struct ufsf_feature *ufsf);
void ufsfbo_suspend(struct ufsf_feature *ufsf);
void ufsfbo_resume(struct ufsf_feature *ufsf, bool is_link_off);
int ufsfbo_send_file_info(struct ufsfbo_dev *fbo, int lun, unsigned char *buf,
			  __u16 size, __u8 idn, int opcode);
int ufsfbo_get_state(struct ufsf_feature *ufsf);
void ufsfbo_set_state(struct ufsf_feature *ufsf, int state);
void ufsfbo_get_dev_info(struct ufsf_feature *ufsf);
void ufsfbo_read_fbo_desc(struct ufsf_feature *ufsf);
#endif /* End of Header */
