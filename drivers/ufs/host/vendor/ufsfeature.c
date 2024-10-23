// SPDX-License-Identifier: GPL-2.0
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

#include "ufsfeature.h"
#include <ufs/ufshcd.h>
#include "../core/ufshcd-priv.h"
#include "ufs-qcom.h"

#if defined(CONFIG_SCSI_SKHID)
extern char storage_mfrid[32];
#endif

static int ufsf_read_desc(struct ufs_hba *hba, u8 desc_id, u8 desc_index,
			  u8 selector, u8 *desc_buf, u32 size)
{
	int err = 0;

	ufshcd_rpm_get_sync(hba);

	err = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    desc_id, desc_index, selector,
					    desc_buf, &size);
	if (err)
		ERR_MSG("reading Device Desc failed. err = %d", err);

	ufshcd_rpm_put_sync(hba);

	return err;
}

static int ufsf_read_dev_desc(struct ufsf_feature *ufsf)
{
	u8 desc_buf[UFSF_QUERY_DESC_DEVICE_MAX_SIZE];
	int ret;

	ret = ufsf_read_desc(ufsf->hba, QUERY_DESC_IDN_DEVICE, 0,
			     UFSFEATURE_SELECTOR, desc_buf,
			     UFSF_QUERY_DESC_DEVICE_MAX_SIZE);
	if (ret)
		return ret;

	INFO_MSG("device lu count %d", desc_buf[DEVICE_DESC_PARAM_NUM_LU]);

	INFO_MSG("length=%u(0x%x) bSupport=0x%.2x, extend=0x%.2x_%.2x",
		  desc_buf[DEVICE_DESC_PARAM_LEN],
		  desc_buf[DEVICE_DESC_PARAM_LEN],
		  desc_buf[DEVICE_DESC_PARAM_UFS_FEAT],
		  desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP+2],
		  desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP+3]);

	INFO_MSG("samsung extend=0x%.2x_%.2x",
		  desc_buf[DEVICE_DESC_PARAM_SAMSUNG_SUP+2],
		  desc_buf[DEVICE_DESC_PARAM_SAMSUNG_SUP+3]);

	INFO_MSG("Driver Feature Version : (%.6X%s)", UFSFEATURE_DD_VER,
		 UFSFEATURE_DD_VER_POST);

#if defined(CONFIG_UFSHID)
	ufshid_get_dev_info(ufsf, desc_buf);
#endif
	return 0;
}

void ufsf_device_check(struct ufs_hba *hba)
{
	struct ufsf_feature *ufsf = ufs_qcom_get_ufsf(hba);

	if (ufsf_read_dev_desc(ufsf))
		return;
}

inline void ufsf_rpm_put_noidle(struct ufs_hba *hba)
{
	pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
}

#define PRINT_BUF_SIZE	1024
void ufsf_print_buf(const unsigned char *field, int size)
{
	unsigned char buf[PRINT_BUF_SIZE];
	int count;
	int i;

	if (size > PRINT_BUF_SIZE)
		size = PRINT_BUF_SIZE;

	count = snprintf(buf, 10, "(0x0000):");

	for (i = 0; i < size; i++) {
		count += snprintf(buf + count, 4, " %02X", field[i]);

		if ((i + 1) % 16 == 0) {
			buf[count] = '\n';
			buf[count + 1] = '\0';
			printk(buf);
			count = 0;
			count += snprintf(buf, 10, "(0x%04X):", i + 1);
		} else if ((i + 1) % 4 == 0)
			count += snprintf(buf + count, 3, " :");
	}
	buf[count] = '\n';
	buf[count + 1] = '\0';
	printk(buf);
}

/*
 * Mimic ufshcd_copy_sense_data()
 */
#define UFS_SENSE_SIZE	18
static inline void ufsf_copy_sense_data(struct ufshcd_lrb *lrbp)
{
	u8 *const sense_buffer = lrbp->cmd->sense_buffer;
	u16 resp_len;
	int len;

	resp_len = be16_to_cpu(lrbp->ucd_rsp_ptr->header.data_segment_length);
	if (sense_buffer && resp_len) {
		int len_to_copy;

		len = be16_to_cpu(lrbp->ucd_rsp_ptr->sr.sense_data_len);
		len_to_copy = min_t(int, UFS_SENSE_SIZE, len);

		memcpy(sense_buffer, lrbp->ucd_rsp_ptr->sr.sense_data,
		       len_to_copy);
	}
}

void ufsf_upiu_check_for_ccd(struct ufshcd_lrb *lrbp)
{
	unsigned char *cdb = lrbp->cmd->cmnd;
	int data_seg_len, sense_data_len;
	struct utp_cmd_rsp *sr = &lrbp->ucd_rsp_ptr->sr;

	if (cdb[0] != VENDOR_OP || cdb[1] != VENDOR_CCD)
		return;

	data_seg_len =
		be16_to_cpu(lrbp->ucd_rsp_ptr->header.data_segment_length);

	sense_data_len = be16_to_cpu(lrbp->ucd_rsp_ptr->sr.sense_data_len);

	if (data_seg_len != CCD_DATA_SEG_LEN ||
	    sense_data_len != CCD_SENSE_DATA_LEN) {
		WARN_MSG("CCD info is wrong. so check it.");
		WARN_MSG("CCD data_seg_len = %d, sense_data_len %d",
			 data_seg_len, sense_data_len);
	} else {
		INFO_MSG("CCD info is correct!!");
	}

	INFO_MSG("sense : %02X %02X %02X %02X %02X %02X\n",
		 sr->sense_data[0], sr->sense_data[1], sr->sense_data[2],
		 sr->sense_data[3], sr->sense_data[4], sr->sense_data[5]);

	/*
	 * sense_len will be not set as Descriptor Type isn't 0x70
	 * if not set sense_len, sense will not be able to copy
	 * in sg_scsi_ioctl()
	 */
	lrbp->cmd->sense_len = CCD_SENSE_DATA_LEN;

	ufsf_copy_sense_data(lrbp);
}

inline bool ufsf_is_valid_lun(int lun)
{
	return lun < UFS_UPIU_MAX_GENERAL_LUN;
}

inline void ufsf_slave_configure(struct ufsf_feature *ufsf,
				 struct scsi_device *sdev)
{
	if (!ufsf_is_valid_lun(sdev->lun))
		return;

	ufsf->sdev_ufs_lu[sdev->lun] = sdev;
	INFO_MSG("lun[%d] sdev(%p) q(%p)", (int)sdev->lun, sdev,
		 sdev->request_queue);

	if (!ufsf->check_init)
		schedule_work(&ufsf->device_check_work);
}

inline int ufsf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	int ret = 0;

	/* This is for READ10_DEBUG (ufs-util) */
	if (get_unaligned_be32(lrbp->cmd->cmnd + 2) == READ10_DEBUG_LBA) {
		lrbp->lun = READ10_DEBUG_LUN;
		INFO_MSG("Change lun to 0x%X", lrbp->lun);
		return ret;
	}

	return ret;
}

/*
 * called by ufshcd_vops_device_reset()
 */
inline void ufsf_reset_host(struct ufsf_feature *ufsf)
{
	struct ufs_hba *hba = ufsf->hba;
	struct Scsi_Host *host = hba->host;
	unsigned long flags;
	u32 eh_flags;

	if (!ufsf->check_init)
		return;

	/*
	 * Check if it is error handling(eh) context.
	 *
	 * In the following cases, we can enter here even though it is not in eh
	 * context.
	 *  - when ufshcd_is_link_off() is true in ufshcd_resume()
	 *  - when ufshcd_vops_suspend() fails in ufshcd_suspend()
	 */
	spin_lock_irqsave(host->host_lock, flags);
	eh_flags = ufshcd_eh_in_progress(hba);
	spin_unlock_irqrestore(host->host_lock, flags);
	if (!eh_flags)
		return;

#if defined(CONFIG_UFSHID)
	INFO_MSG("run reset_host.. hid_state(%d) -> HID_RESET",
		 ufshid_get_state(ufsf));
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_reset_host(ufsf);
#endif

	schedule_work(&ufsf->reset_wait_work);
}

inline void ufsf_init(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_NEED_INIT)
		ufshid_init(ufsf);
#endif

	ufsf->check_init = true;
}

inline void ufsf_reset(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_RESET)
		ufshid_reset(ufsf);
#endif
}

inline void ufsf_remove(struct ufsf_feature *ufsf)
{
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_remove(ufsf);
#endif
}

static void ufsf_device_check_work_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf;

	ufsf = container_of(work, struct ufsf_feature, device_check_work);
#if defined(CONFIG_SCSI_SKHID)
	if ((IS_SKHYNIX_DEVICE(storage_mfrid)) || (IS_HYNIX_DEVICE(storage_mfrid)))
		return;
#endif

	if (ufsf->check_init)
		return;

	ufsf_device_check(ufsf->hba);
	ufsf_init(ufsf);
}

/*
 * worker to change the feature state to present after processing the error handler.
 */
static void ufsf_reset_wait_work_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf;
	struct ufs_hba *hba;
	struct Scsi_Host *host;
	u32 ufshcd_state;
	unsigned long flags;

	ufsf = container_of(work, struct ufsf_feature, reset_wait_work);
	hba = ufsf->hba;
	host = hba->host;

	/*
	 * Wait completion of hba->eh_work.
	 *
	 * reset_wait_work is scheduled at ufsf_reset_host(),
	 * so it can be waken up before eh_work is completed.
	 *
	 * ufsf_reset must be called after eh_work has completed.
	 */
	flush_work(&hba->eh_work);

	spin_lock_irqsave(host->host_lock, flags);
	ufshcd_state = hba->ufshcd_state;
	spin_unlock_irqrestore(host->host_lock, flags);

	if (ufshcd_state == UFSHCD_STATE_OPERATIONAL)
		ufsf_reset(ufsf);
}

static void ufsf_resume_work_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf = container_of(work, struct ufsf_feature, resume_work);
	struct ufs_hba *hba = ufsf->hba;
	bool is_link_off = ufshcd_is_link_off(hba);

	/*
	 * Resume of UFS feature should be called after power & link state
	 * are changed to active. Therefore, it is synchronized as follows.
	 *
	 * System PM: waits to acquire the semaphore used by ufshcd_wl_resume()
	 * Runtime PM: resume using ufshcd_rpm_get_sync()
	 */
	down(&hba->host_sem);
	ufshcd_rpm_get_sync(hba);

	if (ufshcd_is_ufs_dev_active(hba) && ufshcd_is_link_active(hba))
		ufsf_resume(ufsf, is_link_off);

	ufshcd_rpm_put(hba);
	up(&hba->host_sem);
}

#if defined(CONFIG_UFSHID)
static void ufsf_on_idle(struct work_struct *work)
{
	struct ufsf_feature *ufsf;

	ufsf = container_of(work, struct ufsf_feature, on_idle_work);
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_on_idle(ufsf);
}
#endif

inline void ufsf_set_init_state(struct ufs_hba *hba)
{
	struct ufsf_feature *ufsf = ufs_qcom_get_ufsf(hba);

	ufsf->hba = hba;

	INIT_WORK(&ufsf->device_check_work, ufsf_device_check_work_handler);
	INIT_WORK(&ufsf->reset_wait_work, ufsf_reset_wait_work_handler);
	INIT_WORK(&ufsf->resume_work, ufsf_resume_work_handler);
#if defined(CONFIG_UFSHID)
	INIT_WORK(&ufsf->on_idle_work, ufsf_on_idle);
	ufshid_set_state(ufsf, HID_NEED_INIT);
#endif
}

inline void ufsf_suspend(struct ufsf_feature *ufsf, bool is_system_pm)
{
	/*
	 * Wait completion of reset_wait_work.
	 *
	 * When suspend occurrs immediately after reset
	 * and reset_wait_work is executed late,
	 * we can enter here before ufsf_reset() cleans up the feature's reset sequence.
	 */
	flush_work(&ufsf->reset_wait_work);

#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_suspend(ufsf, is_system_pm);
#endif
}

inline void ufsf_resume(struct ufsf_feature *ufsf, bool is_link_off)
{
#if defined(CONFIG_UFSHID)
	if (ufshid_get_state(ufsf) == HID_SUSPEND)
		ufshid_resume(ufsf, is_link_off);
#endif
}

MODULE_LICENSE("GPL v2");
