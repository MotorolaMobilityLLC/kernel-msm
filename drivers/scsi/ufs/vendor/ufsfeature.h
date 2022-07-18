/*
 * Universal Flash Storage Feature Support
 *
 * Copyright (C) 2017-2022 Samsung Electronics Co., Ltd.
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

#include "ufs.h"

#include "ufshid.h"
#include "ufssid.h"

#define UFS_UPIU_MAX_GENERAL_LUN		8
#define UFSHCD_STATE_OPERATIONAL		2	/* ufshcd.c */

/* UFSHCD error handling flags */
enum {
	UFSHCD_EH_IN_PROGRESS = (1 << 0),		/* ufshcd.c */
};
#define ufshcd_eh_in_progress(h) \
	((h)->eh_flags & UFSHCD_EH_IN_PROGRESS)		/* ufshcd.c */

#define UFSFEATURE_QUERY_OPCODE			0x5500

/* Version info */
#define UFSFEATURE_DD_VER			0x020001
#define UFSFEATURE_DD_VER_POST			""

/* For read10 debug */
#define READ10_DEBUG_LUN			0x7F
#define READ10_DEBUG_LBA			0x48504230

#define IOCTL_DEV_CTX_MAX_SIZE			OS_PAGE_SIZE
#define OS_PAGE_SIZE				4096
#define OS_PAGE_SHIFT				12

/* Description */
#define UFSF_QUERY_DESC_DEVICE_MAX_SIZE		0x65

#define UFSFEATURE_SELECTOR			0x01

/* query_flag  */
#define MASK_QUERY_UPIU_FLAG_LOC		0xFF

/* BIG -> LI */
#define LI_EN_16(x)				be16_to_cpu(*(__be16 *)(x))
#define LI_EN_32(x)				be32_to_cpu(*(__be32 *)(x))
#define LI_EN_64(x)				be64_to_cpu(*(__be64 *)(x))

/* LI -> BIG  */
#define GET_BYTE_0(num)			(((num) >> 0) & 0xff)
#define GET_BYTE_1(num)			(((num) >> 8) & 0xff)
#define GET_BYTE_2(num)			(((num) >> 16) & 0xff)
#define GET_BYTE_3(num)			(((num) >> 24) & 0xff)
#define GET_BYTE_4(num)			(((num) >> 32) & 0xff)
#define GET_BYTE_5(num)			(((num) >> 40) & 0xff)
#define GET_BYTE_6(num)			(((num) >> 48) & 0xff)
#define GET_BYTE_7(num)			(((num) >> 56) & 0xff)

#define INFO_MSG(msg, args...)		pr_info("%s:%d info: " msg "\n", \
					       __func__, __LINE__, ##args)
#define ERR_MSG(msg, args...)		pr_err("%s:%d err: " msg "\n", \
					       __func__, __LINE__, ##args)
#define WARN_MSG(msg, args...)		pr_warn("%s:%d warn: " msg "\n", \
					       __func__, __LINE__, ##args)

#define TMSG(ufsf, lun, msg, args...)					\
	do { if (ufsf->sdev_ufs_lu[lun] &&				\
		 ufsf->sdev_ufs_lu[lun]->request_queue)			\
		blk_add_trace_msg(					\
			ufsf->sdev_ufs_lu[lun]->request_queue,		\
			msg, ##args);					\
	} while (0)							\

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

#if defined(CONFIG_UFSHID)
	struct work_struct on_idle_work;
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
int ufsf_check_query(__u32 opcode);
int ufsf_query_ioctl(struct ufsf_feature *ufsf, int lun, void __user *buffer,
		     struct ufs_ioctl_query_data *ioctl_data,
		     u8 selector);
bool ufsf_is_valid_lun(int lun);
void ufsf_slave_configure(struct ufsf_feature *ufsf, struct scsi_device *sdev);
void ufsf_change_lun(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);

int ufsf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);
void ufsf_reset_host(struct ufsf_feature *ufsf);
void ufsf_init(struct ufsf_feature *ufsf);
void ufsf_reset(struct ufsf_feature *ufsf);
void ufsf_remove(struct ufsf_feature *ufsf);
void ufsf_set_init_state(struct ufs_hba *hba);
void ufsf_suspend(struct ufsf_feature *ufsf);
void ufsf_resume(struct ufsf_feature *ufsf, bool is_link_off);

/* mimic */
int ufsf_query_flag(struct ufs_hba *hba, enum query_opcode opcode,
		    enum flag_idn idn, u8 index, u8 selector, bool *flag_res);
int ufsf_query_flag_retry(struct ufs_hba *hba, enum query_opcode opcode,
			  enum flag_idn idn, u8 index, u8 selector,
			  bool *flag_res);
void ufsf_scsi_unblock_requests(struct ufs_hba *hba);
void ufsf_scsi_block_requests(struct ufs_hba *hba);
int ufsf_wait_for_doorbell_clr(struct ufs_hba *hba, u64 wait_timeout_us);

/* for hid */
void ufsf_hid_acc_io_stat(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp);

/* Flag idn for Query Requests*/
#if defined(CONFIG_UFSSID)
#define QUERY_FLAG_IDN_STREAM_ID_EN			0x15
#endif

#if defined(CONFIG_MICRON_UFSHID)
#define QUERY_FLAG_IDN_HID_EN                           0x13
#endif
/* Attribute idn for Query requests */
#if defined(CONFIG_UFSHID)
#define QUERY_ATTR_IDN_HID_OPERATION			0x20
#define QUERY_ATTR_IDN_HID_FRAG_LEVEL			0x21
#endif
#if defined(CONFIG_MICRON_UFSHID)
#define QUERY_ATTR_IDN_HID_FRAG_STATUS                  0x31
#define QUERY_ATTR_IDN_HID_PROGRESS                     0x32
#endif
#define QUERY_ATTR_IDN_SUP_VENDOR_OPTIONS		0xFF

/* Device descriptor parameters offsets in bytes*/
#define DEVICE_DESC_PARAM_EX_FEAT_SUP			0x4F
#if defined(CONFIG_UFSHID)
#define DEVICE_DESC_PARAM_HID_VER			0x59
#endif
#if defined(CONFIG_UFSSID)
#define DEVICE_DESC_PARAM_SID_VER			0x63
#endif
#endif /* End of Header */
