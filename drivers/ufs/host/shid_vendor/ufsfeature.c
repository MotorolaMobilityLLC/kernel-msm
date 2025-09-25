// SPDX-License-Identifier: GPL-2.0
/*
 * Universal Flash Storage Feature Support
 *
 * Copyright (C) 2017-2025 Samsung Electronics Co., Ltd.
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
#include "../ufs-qcom.h"
#include <trace/hooks/ufshcd.h>
#include <linux/utsname.h>
#include <linux/string.h>

const struct ufsf_offset ufsf_idn[] = {
	[DESC_IDN_DEVICE] = { 0xF0 },
	[DESC_IDN_DEVICE_3_1] = { 0x00 },
	[DESC_IDN_GEOMETRY] = { 0xF7 },
	[DESC_IDN_GEOMETRY_3_1] = { 0x07 },
};

const struct ufsf_offset ufsf_desc[] = {
	/* Device Descriptor */
	[DESC_DEVICE_MAX_SIZE] = { 0xFF },
	[DESC_DEVICE_MAX_SIZE_3_1] = { 0x65 },
	/* Configuration Descriptor */
	[DESC_CONFIGURAION_MAX_SIZE] =	{ 0xE6 },
	[DESC_CONFIGURAION_MAX_SIZE_3_1] = { 0xE6 },
	/* Unit Descriptor */
	[DESC_UNIT_MAX_SIZE] = { 0x2D },
	[DESC_UNIT_MAX_SIZE_3_1] = { 0x2D },
	/* Geometry Descriptor */
	[GEOMETRY_MAX_SIZE] = { 0xFF },
	[GEOMETRY_MAX_SIZE_3_1] = { 0x5E },
};

static inline u8 ufsf_get_idn(struct ufsf_feature *ufsf,
				enum ufsf_idn_indx name)
{
	return ufsf->samsung_sel ?
			ufsf_idn[name + ufsf->samsung_sel].offset :
			ufsf_idn[name].offset;
}

static inline u8 ufsf_get_desc(struct ufsf_feature *ufsf,
				 enum ufsf_desc_indx name)
{
	return ufsf->samsung_sel ? ufsf_desc[name + ufsf->samsung_sel].offset :
				ufsf_desc[name].offset;
}

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

static int ufsf_read_dev_desc(struct ufsf_feature *ufsf, u8 selector)
{
	u8 *desc_buf;
	int ret;
	u32 max_size = ufsf_get_desc(ufsf, DESC_DEVICE_MAX_SIZE);
	u32 idn = ufsf_get_idn(ufsf, DESC_IDN_DEVICE);

	desc_buf = kmalloc(max_size, GFP_KERNEL);
	if (!desc_buf) {
		ret = -ENOMEM;
		goto free;
	}

	ret = ufsf_read_desc(ufsf->hba, idn, 0, selector, desc_buf, max_size);
	if (ret)
		goto free;

	ufsf->num_lu = desc_buf[DEVICE_DESC_PARAM_NUM_LU];
	INFO_MSG("device lu count %d", ufsf->num_lu);

	INFO_MSG("sel=%u length=%u(0x%x) bSupport=0x%.2x, extend=0x%.2x_%.2x",
		  selector, desc_buf[DEVICE_DESC_PARAM_LEN],
		  desc_buf[DEVICE_DESC_PARAM_LEN],
		  desc_buf[DEVICE_DESC_PARAM_UFS_FEAT],
		  desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP+2],
		  desc_buf[DEVICE_DESC_PARAM_EX_FEAT_SUP+3]);

	if (!selector) {
		INFO_MSG("samsung extend=0x%.2x_%.2x",
				desc_buf[DEVICE_DESC_PARAM_SAMSUNG_SUP+2],
				desc_buf[DEVICE_DESC_PARAM_SAMSUNG_SUP+3]);
	}

	INFO_MSG("One Driver Feature Version : (%.6X%s)", UFSFEATURE_DD_VER,
		 UFSFEATURE_DD_VER_POST);

#ifdef CONFIG_UFS_SHID
	ufshid_get_dev_info(ufsf, desc_buf);
#endif

free:
	kfree(desc_buf);
	return ret;
}

static int ufsf_read_geo_desc(struct ufsf_feature *ufsf, u8 selector)
{
	u8 *geo_buf;
	int ret;
	u32 max_size = ufsf_get_desc(ufsf, GEOMETRY_MAX_SIZE);
	u32 idn = ufsf_get_idn(ufsf, DESC_IDN_GEOMETRY);

	geo_buf = kmalloc(max_size, GFP_KERNEL);
	if (!geo_buf) {
		ret = -ENOMEM;
		goto free;
	}

	ret = ufsf_read_desc(ufsf->hba, idn, 0, selector, geo_buf, max_size);
	if (ret)
		goto free;

#ifdef CONFIG_UFS_SHID
	if (ufshid_get_state(ufsf) == HID_NEED_INIT)
		ufshid_get_geo_info(ufsf, geo_buf);
#endif

free:
	kfree(geo_buf);

	return ret;
}

static void ufsf_read_unit_desc(struct ufsf_feature *ufsf, int lun, u8 selector)
{
	u8 *unit_buf;
	int lu_enable, ret;
	u32 max_size = ufsf_get_desc(ufsf, DESC_UNIT_MAX_SIZE);

	unit_buf = kmalloc(max_size, GFP_KERNEL);
	if (!unit_buf) {
		ret = -ENOMEM;
		goto free;
	}

	ret = ufsf_read_desc(ufsf->hba, QUERY_DESC_IDN_UNIT, lun, selector,
			     unit_buf, max_size);
			if (ret) {
		ERR_MSG("read unit desc failed. ret (%d)", ret);
		goto free;
	}

	lu_enable = unit_buf[UNIT_DESC_PARAM_LU_ENABLE];
	if (!lu_enable)
		goto free;

free:
	kfree(unit_buf);
}

static inline void ufsf_init_ufs_ver_info(struct ufs_hba *hba)
{
	struct ufs_dev_info *dev_info = &hba->dev_info;
	struct ufsf_feature *ufsf = ufs_qcom_get_ufsf(hba);

	INFO_MSG("UFS DEVICE SPEC is %x", dev_info->wspecversion);

	switch (dev_info->wspecversion) {
		case 0x220:
		case 0x310:
			ufsf->samsung_sel = SELECTOR;
			break;
		case 0x400:
		case 0x410:
			ufsf->samsung_sel = NON_SELECTOR;
			break;
		default:
			ufsf->samsung_sel = NOT_SUPPORT_VER;
			break;
	}
}

#define UTS_LEN 64
void ufsf_get_kernel_version(struct ufsf_feature *ufsf)
{
	char kernel_version[UTS_LEN + 1];
	char *buf;
	char *token;
	int version, patch_lvl;

	buf = memcpy(kernel_version, init_utsname()->release, UTS_LEN);

	token = strsep(&buf, ".");
	version = simple_strtol(token, NULL, 10);

	token = strsep(&buf, ".");
	patch_lvl = simple_strtol(token, NULL, 10);

	INFO_MSG("KERNEL VERSION major : %d, minor : %d\n", version, patch_lvl);

	ufsf->kernel_ver = (version * 100) + patch_lvl;
}

void ufsf_device_check(struct ufs_hba *hba)
{
	struct ufsf_feature *ufsf = ufs_qcom_get_ufsf(hba);
	int lun;

	ufsf_init_ufs_ver_info(hba);

	INFO_MSG("UFS FEATURE SELECTOR D/D %d", ufsf->samsung_sel);

	if (ufsf_read_dev_desc(ufsf, ufsf->samsung_sel))
		return;

	if (ufsf_read_geo_desc(ufsf, ufsf->samsung_sel))
		return;

	seq_scan_lu(lun)
		ufsf_read_unit_desc(ufsf, lun, ufsf->samsung_sel);

}

inline void ufsf_rpm_put_noidle(struct ufs_hba *hba)
{
	pm_runtime_put_noidle(&hba->ufs_device_wlun->sdev_gendev);
}

#define PRINT_QUERY_BUF_SIZE	255
void ufsf_print_query_buf(unsigned char *field, int size)
{
	unsigned char buf[PRINT_QUERY_BUF_SIZE];
	int count;
	int i;

	if (size > PRINT_QUERY_BUF_SIZE)
		size = PRINT_QUERY_BUF_SIZE;

	count = snprintf(buf, 8, "(0x00):");

	for (i = 0; i < size; i++) {
		count += snprintf(buf + count, 4, " %.2X", field[i]);

		if ((i + 1) % 16 == 0) {
			buf[count] = '\n';
			buf[count + 1] = '\0';
			printk(buf);
			count = 0;
			count += snprintf(buf, 8, "(0x%.2X):", i + 1);
		} else if ((i + 1) % 4 == 0)
			count += snprintf(buf + count, 3, " :");
	}
	buf[count] = '\n';
	buf[count + 1] = '\0';
	printk(buf);
}

#define UFS_SENSE_SIZE	18
static inline void ufsf_copy_sense_data(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	u8 *const sense_buffer = lrbp->cmd->sense_buffer;
	int len;

	if (sense_buffer &&
		ufsf->ufsf_temp->get_data_seg_len(lrbp->ucd_rsp_ptr)) {
		int len_to_copy;

		len = be16_to_cpu(lrbp->ucd_rsp_ptr->sr.sense_data_len);
		len_to_copy = min_t(int, UFS_SENSE_SIZE, len);

		memcpy(sense_buffer, lrbp->ucd_rsp_ptr->sr.sense_data,
		       len_to_copy);
	}
}

void ufsf_upiu_check_for_ccd(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	unsigned char *cdb = lrbp->cmd->cmnd;
	int data_seg_len, sense_data_len;
	struct utp_cmd_rsp *sr = &lrbp->ucd_rsp_ptr->sr;

	if (cdb[0] != VENDOR_OP || cdb[1] != VENDOR_CCD)
		return;

	data_seg_len = ufsf->ufsf_temp->get_data_seg_len(lrbp->ucd_rsp_ptr);
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

	ufsf_copy_sense_data(ufsf, lrbp);
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
	ufsf->slave_conf_cnt++;
	INFO_MSG("lun[%d] sdev(%p) q(%p) slave_conf_cnt(%d)",
		 (int)sdev->lun, sdev, sdev->request_queue,
		 ufsf->slave_conf_cnt);

	schedule_work(&ufsf->device_check_work);
}

inline int ufsf_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	int ret = 0;

	/* This is for READ10_DEBUG (ufs-util) */
	if (READ10_DEBUG_LBA == get_unaligned_be32(lrbp->cmd->cmnd + 2)) {
		lrbp->lun = READ10_DEBUG_LUN;
		INFO_MSG("Change lun to 0x%X", lrbp->lun);
		return ret;
	}
#ifdef CONFIG_UFS_SHID
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_prep_fn(ufsf, lrbp);
#endif
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

#ifdef CONFIG_UFS_SHID
	INFO_MSG("run reset_host.. hid_state(%d) -> HID_RESET",
		 ufshid_get_state(ufsf));
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_reset_host(ufsf);
#endif
	schedule_work(&ufsf->reset_wait_work);
}

u32 ufsf_get_data_seg_len_6_1(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	struct ufsf_utp_upiu_header *header = (struct ufsf_utp_upiu_header *)
						&ucd_rsp_ptr->header;
	return be32_to_cpu(header->dword_2) & MASK_UFSF_RSP_UPIU_DATA_SEG_LEN;
}

u32 ufsf_get_req_rsp_6_1(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	struct ufsf_utp_upiu_header *header = (struct ufsf_utp_upiu_header *)
						&ucd_rsp_ptr->header;
	return be32_to_cpu(header->dword_0) >> 24;
}

u32 ufsf_get_rsp_upiu_result_6_1(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	struct ufsf_utp_upiu_header *header = (struct ufsf_utp_upiu_header *)
						&ucd_rsp_ptr->header;
	return be32_to_cpu(header->dword_1) & MASK_UFSF_RSP_UPIU_RESULT;
}

u32 ufsf_get_data_seg_len_6_6(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	struct ufsf_utp_upiu_header *header = (struct ufsf_utp_upiu_header *)
						&ucd_rsp_ptr->header;
	return (u32)be16_to_cpu(header->data_segment_length);
}

u32 ufsf_get_req_rsp_6_6(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	struct ufsf_utp_upiu_header *header = (struct ufsf_utp_upiu_header *)
						&ucd_rsp_ptr->header;
	return (u32)header->transaction_code;
}

u32 ufsf_get_rsp_upiu_result_6_6(struct utp_upiu_rsp *ucd_rsp_ptr)
{
	struct ufsf_utp_upiu_header *header = (struct ufsf_utp_upiu_header *)
						&ucd_rsp_ptr->header;
	return (u32)header->response;
}

inline void ufsf_compl_command(struct ufs_hba *hba, struct ufshcd_lrb *lrbp)
{
	struct ufsf_feature *ufsf = ufs_qcom_get_ufsf(hba);
	struct scsi_cmnd *cmd = lrbp->cmd;
	int scsi_status, result, ocs;
	unsigned long *outstanding_reqs;
	unsigned long out_tasks = 0;
	unsigned long ongoing_cnt = 0;
	int tmp_tag, nr_tag;

	if (!cmd)
		return;

	if (!ufsf->ufsf_temp)
		return;

	ocs = ufsf_get_tr_ocs(lrbp, NULL);
	if (ocs != OCS_SUCCESS)
		goto check_last_req;

	result = ufsf->ufsf_temp->get_req_rsp(lrbp->ucd_rsp_ptr);
	if (result != UPIU_TRANSACTION_RESPONSE)
		goto check_last_req;

	scsi_status = ufsf->ufsf_temp->get_rsp_upiu_result(lrbp->ucd_rsp_ptr);
	if (scsi_status != SAM_STAT_GOOD)
		goto check_last_req;

	ufsf_upiu_check_for_ccd(ufsf, lrbp);

	outstanding_reqs = &hba->outstanding_reqs;
	nr_tag = hba->nutrs;

	for_each_set_bit(tmp_tag, outstanding_reqs, nr_tag) {
		ongoing_cnt = 1;
		break;
	}

	out_tasks = hba->outstanding_tasks;

check_last_req:
#ifdef CONFIG_UFS_SHID
	/* Check if it is the last request to be completed */
	if (!out_tasks && !ongoing_cnt && !ufshid_is_hid_req(cmd))
		schedule_work(&ufsf->on_idle_work);
#endif
	return;
}

static void ufs_vh_prep_fn(void *data, struct ufs_hba *hba,
			struct request *rq, struct ufshcd_lrb *lrbp, int *err)
{
	*err = ufsf_prep_fn(ufs_qcom_get_ufsf(hba), lrbp);
}

static void ufs_vh_compl_command(void *data, struct ufs_hba *hba,
			struct ufshcd_lrb *lrbp)
{
	ufsf_compl_command(hba, lrbp);
}

static void ufs_vh_update_sdev(void *data, struct scsi_device *sdev)
{
	struct ufs_hba *hba = shost_priv(sdev->host);
	struct ufsf_feature *ufsf = ufs_qcom_get_ufsf(hba);

	ufsf_slave_configure(ufsf, sdev);
}

void ufs_samsung_register_hooks(void)
{
	register_trace_android_vh_ufs_prepare_command(ufs_vh_prep_fn, NULL);
	register_trace_android_vh_ufs_compl_command(ufs_vh_compl_command, NULL);
	register_trace_android_vh_ufs_update_sdev(ufs_vh_update_sdev, NULL);
}

static struct ufsf_template ufsf_6_1 = {
	.get_data_seg_len = ufsf_get_data_seg_len_6_1,
	.get_req_rsp = ufsf_get_req_rsp_6_1,
	.get_rsp_upiu_result = ufsf_get_rsp_upiu_result_6_1,
};

static struct ufsf_template ufsf_6_6 = {
	.get_data_seg_len = ufsf_get_data_seg_len_6_6,
	.get_req_rsp = ufsf_get_req_rsp_6_6,
	.get_rsp_upiu_result = ufsf_get_rsp_upiu_result_6_6,
};

inline void ufsf_init(struct ufsf_feature *ufsf)
{
	if (ufsf->kernel_ver == KERNEL_6_1) {
		ufsf->ufsf_temp = &ufsf_6_1;
	} else if (ufsf->kernel_ver == KERNEL_6_6) {
		ufsf->ufsf_temp = &ufsf_6_6;
	} else {
		ERR_MSG("Not Support Kernel Ver by One Driver");
		goto ver_miss;
	}

	if (ufsf->samsung_sel == NOT_SUPPORT_VER) {
		ERR_MSG("Not Support Device Ver by One Driver");
		goto ver_miss;
	}

#ifdef CONFIG_UFS_SHID
	if (ufshid_get_state(ufsf) == HID_NEED_INIT)
		ufshid_init(ufsf);
#endif

ver_miss:
	ufsf->check_init = true;
}

inline void ufsf_reset(struct ufsf_feature *ufsf)
{

#ifdef CONFIG_UFS_SHID
	if (ufshid_get_state(ufsf) == HID_RESET)
		ufshid_reset(ufsf);
#endif
}

inline void ufsf_remove(struct ufsf_feature *ufsf)
{
#ifdef CONFIG_UFS_SHID
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_remove(ufsf);
#endif
}

static void ufsf_device_check_work_handler(struct work_struct *work)
{
	struct ufsf_feature *ufsf;

	ufsf = container_of(work, struct ufsf_feature, device_check_work);

	mutex_lock(&ufsf->device_check_lock);
	if (!ufsf->check_init) {
		ufsf_device_check(ufsf->hba);
		ufsf_get_kernel_version(ufsf);
		ufsf_init(ufsf);
	}
	mutex_unlock(&ufsf->device_check_lock);
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
#ifdef CONFIG_UFS_SHID
static void ufsf_on_idle(struct work_struct *work)
{
	struct ufsf_feature *ufsf;

	ufsf = container_of(work, struct ufsf_feature, on_idle_work);
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_on_idle(ufsf);
}
#endif

inline void ufsf_set_init_state(struct ufsf_feature *ufsf)
{
#ifdef CONFIG_UFS_SHID
	INIT_WORK(&ufsf->on_idle_work, ufsf_on_idle);
	ufshid_set_state(ufsf, HID_NEED_INIT);
#endif
}

inline void ufsf_set_init(struct ufs_hba *hba)
{
	struct ufsf_feature *ufsf = ufs_qcom_get_ufsf(hba);

	ufsf->hba = hba;
	ufsf->slave_conf_cnt = 0;

	mutex_init(&ufsf->device_check_lock);
	INIT_WORK(&ufsf->device_check_work, ufsf_device_check_work_handler);
	INIT_WORK(&ufsf->reset_wait_work, ufsf_reset_wait_work_handler);
	INIT_WORK(&ufsf->resume_work, ufsf_resume_work_handler);

	ufsf_set_init_state(ufsf);
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

#ifdef CONFIG_UFS_SHID
	if (ufshid_get_state(ufsf) == HID_PRESENT)
		ufshid_suspend(ufsf, is_system_pm);
#endif
}

inline void ufsf_resume(struct ufsf_feature *ufsf, bool is_link_off)
{
#ifdef CONFIG_UFS_SHID
	if (ufshid_get_state(ufsf) == HID_SUSPEND)
		ufshid_resume(ufsf, is_link_off);
#endif
}

/*
 * Modified Exception Event handler for UFS Feature.
 *
 * It does exactly same operation with ufshcd_exception_event_handler(),
 * and also check Vendor Exception Event Status.
 */
void ufsf_vendor_exception_event_handler(struct work_struct *work)
{
	struct ufs_hba *hba;
	struct ufsf_feature *ufsf;

	hba = container_of(work, struct ufs_hba, eeh_work);
	ufsf = ufs_qcom_get_ufsf(hba);

	/* mimic function of ufshcd_exception_event_handler() */
	ufsf_exception_event_handler(hba);
}

void ufs_host_exception_event_hook(struct ufs_hba *hba)
{
	INIT_WORK(&hba->eeh_work, ufsf_vendor_exception_event_handler);
}

/*
 * Wrapper function for query Attributes & Flags.
 */
int ufsf_query_attr_retry(struct ufs_hba *hba,
	enum query_opcode opcode, enum attr_idn idn, u8 index, u32 *attr_val)
{
	struct ufsf_feature *ufsf = ufs_qcom_get_ufsf(hba);
	int selector = ufsf->samsung_sel;

	return ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, idn,
				       index, selector, attr_val);
}

MODULE_LICENSE("GPL v2");
