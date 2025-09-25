/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Universal Flash Storage Feature Support
 *
 * Copyright (C) 2017-2025 Samsung Electronics Co., Ltd.
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

#ifndef _UFSFEATURE_H_
#define _UFSFEATURE_H_

#include <scsi/scsi_cmnd.h>
#include <asm/unaligned.h>
#include <ufs/ufs.h>
#ifdef CONFIG_UFS_SHID
#include "ufshid.h"
#endif
#include <ufs/ufshci.h>

#define UFS_UPIU_MAX_GENERAL_LUN		8

#define KERNEL_6_1				601
#define KERNEL_6_6				606

#define MASK_UFSF_RSP_UPIU_RESULT		0xFFFF
#define MASK_UFSF_RSP_UPIU_DATA_SEG_LEN		0xFFFF

/* Version info */
#define UFSFEATURE_DD_VER			0x010001
#define UFSFEATURE_DD_VER_POST			""

/* For read10 debug */
#define READ10_DEBUG_LUN                        0x7F
#define READ10_DEBUG_LBA                        0x48504230

/* For Chip Crack Detection */
#define VENDOR_OP                               0xC0
#define VENDOR_CCD                              0x51
#define CCD_DATA_SEG_LEN                        0x08
#define CCD_SENSE_DATA_LEN                      0x06
#define CCD_DESC_TYPE                           0x81

enum {
	NON_SELECTOR	= 0,
	SELECTOR,
	NOT_SUPPORT_VER = 0xFF,
};

enum ufs_ver {
	UFS_2_2		= 0,
	UFS_3_1,
	UFS_4_0,
	UFS_NOT,
};

enum ufsf_idn_indx {
	DESC_IDN_DEVICE = 0,
	DESC_IDN_DEVICE_3_1,
	DESC_IDN_GEOMETRY,
	DESC_IDN_GEOMETRY_3_1,
	IDN_END,
};

enum ufsf_desc_indx {
	DESC_DEVICE_MAX_SIZE = 0,
	DESC_DEVICE_MAX_SIZE_3_1,
	DESC_CONFIGURAION_MAX_SIZE,
	DESC_CONFIGURAION_MAX_SIZE_3_1,
	DESC_UNIT_MAX_SIZE,
	DESC_UNIT_MAX_SIZE_3_1,
	GEOMETRY_MAX_SIZE,
	GEOMETRY_MAX_SIZE_3_1,
	DESC_END,
};

struct ufsf_offset {
	u32 offset;
};

/* Device descriptor parameters offsets in bytes*/
#define DEVICE_DESC_PARAM_EX_FEAT_SUP			0x4F
#define DEVICE_DESC_PARAM_SAMSUNG_SUP			0xFB

/* query_flag  */
#define MASK_QUERY_UPIU_FLAG_LOC		0xFF

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
	} while (0)

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
	bool check_init;
	struct work_struct device_check_work;
	struct mutex device_check_lock;

	struct work_struct reset_wait_work;
	struct work_struct resume_work;
	u8 samsung_sel;

	const struct ufsf_offset *desc;
	const struct ufsf_offset *idn_device;

#ifdef CONFIG_UFS_SHID
	struct work_struct on_idle_work;
	atomic_t hid_state;
	struct ufshid_dev *hid_dev;
#endif
	u32 kernel_ver;
	struct ufsf_template *ufsf_temp;
};

struct ufsf_utp_upiu_header {
	union {
		struct {
			__be32 dword_0;
			__be32 dword_1;
			__be32 dword_2;
		};
		struct {
			__u8 transaction_code;
			__u8 flags;
			__u8 lun;
			__u8 task_tag;
#if defined(__BIG_ENDIAN)
			__u8 iid: 4;
			__u8 command_set_type: 4;
#elif defined(__LITTLE_ENDIAN)
			__u8 command_set_type: 4;
			__u8 iid: 4;
#else
#error
#endif
			union {
				__u8 tm_function;
				__u8 query_function;
			} __attribute__((packed));
			__u8 response;
			__u8 status;
			__u8 ehs_length;
			__u8 device_information;
			__be16 data_segment_length;
		};
	};
};

struct ufsf_template {
	u32 (*get_data_seg_len)(struct utp_upiu_rsp *ucd_rsp_ptr);
	u32 (*get_req_rsp)(struct utp_upiu_rsp *ucd_rsp_ptr);
	u32 (*get_rsp_upiu_result)(struct utp_upiu_rsp *ucd_rsp_ptr);
};

struct ufs_hba;
struct ufshcd_lrb;

void ufsf_device_check(struct ufs_hba *hba);
void ufsf_upiu_check_for_ccd(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
bool ufsf_is_valid_lun(int lun);
void ufsf_slave_configure(struct ufsf_feature *ufsf, struct scsi_device *sdev);
void ufsf_change_lun(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufs_samsung_register_hooks(void);
void ufs_host_exception_event_hook(struct ufs_hba *hba);
int ufsf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsf_reset_host(struct ufsf_feature *ufsf);
void ufsf_init(struct ufsf_feature *ufsf);
void ufsf_reset(struct ufsf_feature *ufsf);
void ufsf_remove(struct ufsf_feature *ufsf);
void ufsf_set_init(struct ufs_hba *hba);
void ufsf_suspend(struct ufsf_feature *ufsf, bool is_system_pm);
void ufsf_resume(struct ufsf_feature *ufsf, bool is_link_off);
void ufsf_vendor_exception_event_handler(struct work_struct *work);
int ufsf_query_attr_retry(struct ufs_hba *hba, enum query_opcode opcode,
			  enum attr_idn idn, u8 index, u32 *attr_val);
void ufsf_print_query_buf(unsigned char *field, int size);
/* mimic */
int ufsf_issue_tm_cmd(struct ufs_hba *hba, int lun_id, int task_id,
		      u8 tm_function, u8 *tm_response);
void ufsf_scsi_unblock_requests(struct ufs_hba *hba);
void ufsf_scsi_block_requests(struct ufs_hba *hba);
int ufsf_wait_for_doorbell_clr(struct ufs_hba *hba, u64 wait_timeout_us);
void ufsf_rpm_put_noidle(struct ufs_hba *hba);
int ufsf_get_bkops_status(struct ufs_hba *hba, u32 *status);
int ufsf_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
		    enum flag_idn idn, u8 index, u8 selector, bool *flag_res);
int ufsf_enable_ee(struct ufs_hba *hba, u16 mask);
int ufsf_disable_ee(struct ufs_hba *hba, u16 mask);
void ufsf_exception_event_handler(struct ufs_hba *hba);
enum utp_ocs ufsf_get_tr_ocs(struct ufshcd_lrb *lrbp, struct cq_entry *cqe);

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

/* Query request retries */
#define QUERY_REQ_RETRIES 3
/* Query request timeout */
#define QUERY_REQ_TIMEOUT 1500 /* 1.5 seconds */
#endif /* End of Header */
