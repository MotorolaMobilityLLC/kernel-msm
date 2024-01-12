/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Universal Flash Storage Feature Support
 *
 * Copyright (C) 2017-2018 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Yongmyung Lee <ymhungry.lee@samsung.com>
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

#ifndef _UFSFEATURE_H_
#define _UFSFEATURE_H_

#include <scsi/scsi_cmnd.h>
#include <asm/unaligned.h>

#include <ufs/ufs.h>

#include "ufshid.h"
#include "ufssid.h"

#define UFS_UPIU_MAX_GENERAL_LUN		8

/* UFSHCD error handling flags */
enum {
	UFSHCD_EH_IN_PROGRESS = (1 << 0),		/* ufshcd.c */
};
#define ufshcd_eh_in_progress(h) \
	((h)->eh_flags & UFSHCD_EH_IN_PROGRESS)		/* ufshcd.c */


#define UFSFEATURE_QUERY_OPCODE			0x5500

/* Version info */
#define UFSFEATURE_DD_VER			0x030400
#define UFSFEATURE_DD_VER_POST			""

/* For read10 debug */
#define READ10_DEBUG_LUN			0x7F
#define READ10_DEBUG_LBA			0x48504230

/* For Chip Crack Detection */
#define VENDOR_OP                               0xC0
#define VENDOR_CCD                              0x51
#define CCD_DATA_SEG_LEN                        0x08
#define CCD_SENSE_DATA_LEN                      0x06
#define CCD_DESC_TYPE                           0x81

#define IOCTL_DEV_CTX_MAX_SIZE			OS_PAGE_SIZE
#define OS_PAGE_SIZE				4096
#define OS_PAGE_SHIFT				12

/* Description */
#define UFSF_QUERY_DESC_DEVICE_MAX_SIZE		0xFF
#define UFSF_QUERY_DESC_CONFIGURAION_MAX_SIZE	0xE6
#define UFSF_QUERY_DESC_UNIT_MAX_SIZE		0x2D
#define UFSF_QUERY_DESC_GEOMETRY_MAX_SIZE	0xFF
#define UFSF_QUERY_DESC_FBO_MAX_SIZE		0x12

/* Descriptor idn for Query Request */
#define UFSF_QUERY_DESC_IDN_VENDOR_DEVICE	0xF0
#define UFSF_QUERY_DESC_IDN_VENDOR_GEOMETRY	0xF7
#define UFSF_QUERY_DESC_IDN_FBO			0x0A

/* query_flag  */
#define MASK_QUERY_UPIU_FLAG_LOC		0xFF

/* For read10 debug */
#define READ10_DEBUG_LUN			0x7F
#define READ10_DEBUG_LBA			0x48504230

/* For ioctl-based provisioning tool */
#define CONF_DESC_INDEX_OFFSET			0xF0

#define INFO_MSG(msg, args...)		pr_info("%s:%d info: " msg "\n", \
					       __func__, __LINE__, ##args)
#define ERR_MSG(msg, args...)		pr_err("%s:%d err: " msg "\n", \
					       __func__, __LINE__, ##args)
#define WARN_MSG(msg, args...)		pr_warn("%s:%d warn: " msg "\n", \
					       __func__, __LINE__, ##args)

#define seq_scan_lu(lun) for (lun = 0; lun < UFS_UPIU_MAX_GENERAL_LUN; lun++)

#define TMSG(ufsf, lun, msg, args...)					\
	do { if (ufsf->sdev_ufs_lu[lun] &&				\
		 ufsf->sdev_ufs_lu[lun]->request_queue)			\
		blk_add_trace_msg(					\
			ufsf->sdev_ufs_lu[lun]->request_queue,		\
			msg, ##args);					\
	} while (0)							\

struct ufsf_lu_desc {
	/* Common info */
	int lu_enable;		/* 03h bLUEnable */
	int lu_queue_depth;	/* 06h lu queue depth info*/
	int lu_logblk_size;	/* 0Ah bLogicalBlockSize. default 0x0C = 4KB */
	u64 lu_logblk_cnt;	/* 0Bh qLogicalBlockCount. */
};

struct ufsf_feature {
	struct ufs_hba *hba;
	int num_lu;
	int slave_conf_cnt;
	struct scsi_device *sdev_ufs_lu[UFS_UPIU_MAX_GENERAL_LUN];
	bool issue_ioctl;
	bool check_init;
	struct work_struct device_check_work;
	struct mutex device_check_lock;

	struct work_struct reset_wait_work;
	struct work_struct resume_work;
#if defined(CONFIG_UFSHID)
	atomic_t hid_state;
	struct ufshid_dev *hid_dev;
#endif
#if defined(CONFIG_UFSSID)
	struct ufssid_dev *sid_dev;
#endif
};

struct ufs_hba;
struct ufshcd_lrb;
struct ufs_ioctl_query_data;

void ufsf_device_check(struct ufs_hba *hba);
int ufsf_query_ioctl(struct ufsf_feature *ufsf, int lun, void __user *buffer,
		     struct ufs_ioctl_query_data *ioctl_data);
void ufsf_upiu_check_for_ccd(struct ufshcd_lrb *lrbp);
int ufsf_get_scsi_device(struct ufs_hba *hba, struct scsi_device *sdev);
bool ufsf_is_valid_lun(int lun);
void ufsf_slave_configure(struct ufsf_feature *ufsf, struct scsi_device *sdev);
void ufsf_change_lun(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);

int ufsf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsf_reset_lu(struct ufsf_feature *ufsf);
void ufsf_reset_host(struct ufsf_feature *ufsf);
void ufsf_init(struct ufsf_feature *ufsf);
void ufsf_reset(struct ufsf_feature *ufsf);
void ufsf_remove(struct ufsf_feature *ufsf);
void ufsf_set_init_state(struct ufs_hba *hba);
void ufsf_suspend(struct ufsf_feature *ufsf);
void ufsf_resume(struct ufsf_feature *ufsf, bool is_link_off);

/* mimic */
void ufsf_scsi_unblock_requests(struct ufs_hba *hba);
void ufsf_scsi_block_requests(struct ufs_hba *hba);
int ufsf_wait_for_doorbell_clr(struct ufs_hba *hba, u64 wait_timeout_us);
void ufsf_rpm_put_noidle(struct ufs_hba *hba);
int ufsf_get_bkops_status(struct ufs_hba *hba, u32 *status);

/* Flag idn for Query Requests*/
#if defined(CONFIG_UFSSID)
#define QUERY_FLAG_IDN_STREAM_ID_EN			0x81
#endif
#if defined(CONFIG_UFSHID)
#if defined(CONFIG_MICRON_UFSHID)
#define QUERY_FLAG_IDN_HID_EN_MICRON_UFSV4		0x83
#endif
#endif

/* Attribute idn for Query requests */
#if defined(CONFIG_UFSHID)
#define QUERY_ATTR_IDN_HID_OPERATION			0x80
#define QUERY_ATTR_IDN_HID_FRAG_LEVEL			0x81
#if defined(CONFIG_MICRON_UFSHID)
#define QUERY_ATTR_IDN_HID_FRAG_STATUS_MICRON_UFSV4	0x81
#define QUERY_ATTR_IDN_HID_PROGRESS_MICRON_UFSV4	0x82
#endif
#define QUERY_ATTR_IDN_HID_SIZE				0x8A
#define QUERY_ATTR_IDN_HID_AVAIL_SIZE			0x8B
#define QUERY_ATTR_IDN_HID_PROGRESS_RATIO		0x8C
#define QUERY_ATTR_IDN_HID_STATE			0x8D
#define QUERY_ATTR_IDN_HID_L2P_FRAG_LEVEL		0x8E
#define QUERY_ATTR_IDN_HID_L2P_DEFRAG_THRESHOLD		0x8F
#define QUERY_ATTR_IDN_HID_FEAT_SUP			0x90
#endif
#define QUERY_ATTR_IDN_VENDOR_EE_CONTROL		0x97
#define QUERY_ATTR_IDN_VENDOR_EE_STATUS			0x98

/* Device descriptor parameters offsets in bytes*/
#define DEVICE_DESC_PARAM_EX_FEAT_SUP			0x4F
#define DEVICE_DESC_PARAM_SAMSUNG_SUP			0xFB
#if defined(CONFIG_UFSHID)
#define DEVICE_DESC_PARAM_HID_VER			0xF7
#endif
#if defined(CONFIG_UFSSID)
#define DEVICE_DESC_PARAM_SID_VER			0xEF
#endif
/* Geometry descriptor parameters offsets in bytes*/
#if defined(CONFIG_UFSHID)
#define GEOMETRY_DESC_HID_MAX_LBA_RANGE_CNT		0xF8
#define GEOMETRY_DESC_HID_MAX_LBA_RANGE_SIZE		0xF9
#endif

/**
 * struct utp_upiu_task_req - Task request UPIU structure
 * @header - UPIU header structure DW0 to DW-2
 * @input_param1: Input parameter 1 DW-3
 * @input_param2: Input parameter 2 DW-4
 * @input_param3: Input parameter 3 DW-5
 * @reserved: Reserved double words DW-6 to DW-7
 */
struct utp_upiu_task_req {
	struct utp_upiu_header header;
	__be32 input_param1;
	__be32 input_param2;
	__be32 input_param3;
	__be32 reserved[2];
};

static inline int ufsf_check_query(__u32 opcode)
{
	return (opcode & 0xffff0000) >> 16 == UFSFEATURE_QUERY_OPCODE;
}

#endif /* End of Header */
