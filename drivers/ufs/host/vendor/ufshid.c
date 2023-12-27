// SPDX-License-Identifier: GPL-2.0
/*
 * Universal Flash Storage Host Initiated Defrag
 *
 * Copyright (C) 2019 Samsung Electronics Co., Ltd.
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

#include <ufs/ufshcd.h>
#include "../core/ufshcd-priv.h"
#include "ufsfeature.h"
#include "ufshid.h"

static int ufshid_create_sysfs(struct ufshid_dev *hid);

static inline int ufshid_schedule_delayed_work(struct delayed_work *work,
					       unsigned long delay)
{
	return queue_delayed_work(system_freezable_wq, work, delay);
}

inline int ufshid_get_state(struct ufsf_feature *ufsf)
{
	return atomic_read(&ufsf->hid_state);
}

inline void ufshid_set_state(struct ufsf_feature *ufsf, int state)
{
	atomic_set(&ufsf->hid_state, state);
}

static inline int ufshid_is_not_present(struct ufshid_dev *hid)
{
	enum UFSHID_STATE cur_state = ufshid_get_state(hid->ufsf);

	if (cur_state != HID_PRESENT) {
		INFO_MSG("hid_state != HID_PRESENT (%d)", cur_state);
		return -ENODEV;
	}
	return 0;
}

static int ufshid_read_attr(struct ufshid_dev *hid, u8 idn, u32 *attr_val)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	int ret = 0;

	ufshcd_rpm_get_sync(hba);

	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, idn, 0,
				      0, attr_val);
	if (ret) {
		ERR_MSG("read attr [0x%.2X] fail. (%d)", idn, ret);
		goto err_out;
	}

	HID_DEBUG(hid, "hid_attr read [0x%.2X] %u (0x%X)", idn, *attr_val,
		  *attr_val);
err_out:
	ufsf_rpm_put_noidle(hba);

	return ret;
}

static int ufshid_write_attr(struct ufshid_dev *hid, u8 idn, u32 val)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	int ret = 0;

	ufshcd_rpm_get_sync(hba);

	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, idn, 0,
				      0, &val);
	if (ret) {
		ERR_MSG("write attr [0x%.2X] fail. (%d)", idn, ret);
		goto err_out;
	}

	HID_DEBUG(hid, "hid_attr write [0x%.2X] %u (0x%X)", idn, val, val);
err_out:
	ufsf_rpm_put_noidle(hba);

	return ret;
}

static inline int ufshid_version_check(int spec_version)
{
	INFO_MSG("Support HID Spec : Driver = (%.4x), Device = (%.4x)",
		 UFSHID_VER, spec_version);
	INFO_MSG("HID Driver version (%.6X%s)",
		 UFSHID_DD_VER, UFSHID_DD_VER_POST);

	if (spec_version != UFSHID_VER) {
		ERR_MSG("UFS HID version mismatched");
		return -ENODEV;
	}
	return 0;
}

void ufshid_get_geo_info(struct ufsf_feature *ufsf, u8 *geo_buf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	hid->max_lba_range_size = get_unaligned_be32(geo_buf +
					GEOMETRY_DESC_HID_MAX_LBA_RANGE_SIZE);
	hid->max_lba_range_cnt = geo_buf[GEOMETRY_DESC_HID_MAX_LBA_RANGE_CNT];
	INFO_MSG("[0x%.2x] dMaxHIDLBARangeSize (%u)",
			GEOMETRY_DESC_HID_MAX_LBA_RANGE_SIZE,
			hid->max_lba_range_size);
	INFO_MSG("[0x%.2x] bMaxHIDLBARangeCount (%u)",
			GEOMETRY_DESC_HID_MAX_LBA_RANGE_CNT,
			hid->max_lba_range_cnt);
}

void ufshid_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf)
{
	int ret = 0, spec_version;

	ufsf->hid_dev = NULL;

	if (!(get_unaligned_be32(desc_buf + DEVICE_DESC_PARAM_SAMSUNG_SUP) &
	      UFS_FEATURE_SUPPORT_HID_BIT)) {
		INFO_MSG("bUFSExFeaturesSupport: HID not support");
		goto err_out;
	}

	INFO_MSG("bUFSExFeaturesSupport: HID support");
	spec_version = get_unaligned_be16(desc_buf + DEVICE_DESC_PARAM_HID_VER);
	ret = ufshid_version_check(spec_version);
	if (ret)
		goto err_out;

	ufsf->hid_dev = kzalloc(sizeof(struct ufshid_dev), GFP_KERNEL);
	if (!ufsf->hid_dev) {
		ERR_MSG("hid_dev memalloc fail");
		goto err_out;
	}

	ufsf->hid_dev->ufsf = ufsf;
	return;
err_out:
	ufshid_set_state(ufsf, HID_FAILED);
}

static inline void ufshid_set_wb_cmd(unsigned char *cdb, size_t len)
{
	cdb[0] = WRITE_BUFFER;
	cdb[1] = 0x1D;
	put_unaligned_be24(len, cdb + 6);
}

static int ufshid_issue_lba_list(struct ufshid_dev *hid)
{
	struct ufsf_feature *ufsf = hid->ufsf;
	struct ufshid_req *hid_req = &hid->hid_req;
	unsigned char cdb[10] = { 0 };
	struct scsi_device *sdev;
	struct scsi_sense_hdr sshdr;
	int ret = 0, retries;
	const struct scsi_exec_args args = {
		.sshdr = &sshdr,
	};

	if (!hid->lba_trigger_mode)
		return 0;

	if (!hid_req->buf_size) {
		ERR_MSG("buf_size is 0. check it (%lu)", hid_req->buf_size);
		return -EINVAL;
	}

	sdev = ufsf->sdev_ufs_lu[hid_req->lun];
	if (!sdev) {
		ERR_MSG("cannot find scsi_device [%d]", hid_req->lun);
		return -ENODEV;
	}

	ufshid_set_wb_cmd(cdb, hid_req->buf_size);

	for (retries = 0; retries < 3; retries++) {
		ret = scsi_execute_cmd(sdev, cdb, REQ_OP_DRV_OUT, hid_req->buf,
				   hid_req->buf_size, msecs_to_jiffies(30000),
				   0, &args);
		if (ret)
			ERR_MSG("WB for HID failed. (%d) retries %d",
				ret, retries);
		else
			break;
	}

	INFO_MSG("WB for HID %s", ret ? "failed" : "success");

	if (ret) {
		ERR_MSG("code %x sense_key %x asc %x ascq %x",
			sshdr.response_code,
			sshdr.sense_key, sshdr.asc, sshdr.ascq);
		ERR_MSG("byte4 %x byte5 %x byte6 %x additional_len %x",
			sshdr.byte4, sshdr.byte5,
			sshdr.byte6, sshdr.additional_length);
	}

	return ret;
}

static inline void ufshid_init_lba_trigger_mode(struct ufshid_dev *hid)
{
	hid->lba_trigger_mode = false;
}

static inline void ufshid_set_lba_trigger_mode(struct ufshid_dev *hid,
					       int lun, unsigned char *buf,
					       __u16 size)
{
	struct ufshid_req *hid_req = &hid->hid_req;

	hid_req->lun = lun;
	memcpy(hid_req->buf, buf, size);
	hid_req->buf_size = size;
	hid->lba_trigger_mode = true;
}

static inline void ufshid_issue_disable(struct ufshid_dev *hid)
{
	ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, HID_OP_DISABLE);
}

static int ufshid_analyze_and_get_attr(struct ufshid_dev *hid, __u8 idn,
				       u32 *attr_val)
{
	int ret;
	u32 hid_state;

	ret = ufshid_issue_lba_list(hid);
	if (ret)
		return ret;

	if (ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION,
			      HID_OP_ANALYZE))
		return -EINVAL;

	msleep(200);

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_STATE, &hid_state))
		return -EINVAL;

	/*
	 * If defrag is not required, the analysis context disappears from the
	 * device as it enters the init state.
	 */
	if (hid_state != HID_DEFRAG_REQUIRED) {
		*attr_val = RESULT_NOT_DEFRAG_REQUIRED;
		return -EINVAL;
	}

	return ufshid_read_attr(hid, idn, attr_val);
}

static bool ufshid_is_in_progress(struct ufshid_dev *hid)
{
	u32 state;
	int ret;

	ret = ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_STATE, &state);
	if (ret)
		return false;

	return state == HID_ANALYSIS_IN_PROGRESS ||
		state == HID_DEFRAG_IN_PROGRESS;
}

static int ufshid_get_frag_lvl_of_file(struct ufshid_dev *hid,
				       unsigned char *buf)
{
	u32 attr_val;
	int ret;

	ret = ufshid_analyze_and_get_attr(hid,
					  QUERY_ATTR_IDN_HID_L2P_FRAG_LEVEL,
					  &attr_val);
	if (ret) {
		if (attr_val == RESULT_NOT_DEFRAG_REQUIRED) {
			buf[0] = HID_L2P_DEFRAG_LVL_UNKNOWN;
			/* Returns 0 to copy "buf[0]" to user */
			return 0;
		}

		return ret;
	}

	INFO_MSG("L2P Fragment Level is %u", attr_val);

	buf[0] = attr_val;

	/* Initialize HID state */
	ufshid_issue_disable(hid);

	return 0;
}

/*
 * Lock status: hid_sysfs lock was held when called.
 */
static void ufshid_auto_hibern8_enable(struct ufshid_dev *hid,
				       unsigned int val)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	unsigned long flags;
	u32 reg;

	val = !!val;

	/* Update auto hibern8 timer value if supported */
	if (!ufshcd_is_auto_hibern8_supported(hba))
		return;

	ufshcd_rpm_get_sync(hba);
	ufshcd_hold(hba, false);
	ufsf_scsi_block_requests(hba);
	/* wait for all the outstanding requests to finish */
	ufsf_wait_for_doorbell_clr(hba, U64_MAX);
	spin_lock_irqsave(hba->host->host_lock, flags);

	reg = ufshcd_readl(hba, REG_AUTO_HIBERNATE_IDLE_TIMER);
	INFO_MSG("ahit-reg 0x%X", reg);

	if (val ^ (reg != 0)) {
		if (val) {
			hba->ahit = hid->ahit;
		} else {
			/*
			 * Store current ahit value.
			 * We don't know who set the ahit value to different
			 * from the initial value
			 */
			hid->ahit = reg;
			hba->ahit = 0;
		}

		ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);

		/* Make sure the timer gets applied before further operations */
		mb();

		INFO_MSG("[Before] is_auto_enabled %d", hid->is_auto_enabled);
		hid->is_auto_enabled = val;

		reg = ufshcd_readl(hba, REG_AUTO_HIBERNATE_IDLE_TIMER);
		INFO_MSG("[After] is_auto_enabled %d ahit-reg 0x%X",
			 hid->is_auto_enabled, reg);
	} else {
		INFO_MSG("is_auto_enabled %d. so it does not changed",
			 hid->is_auto_enabled);
	}

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufsf_scsi_unblock_requests(hba);
	ufshcd_release(hba);
	ufsf_rpm_put_noidle(hba);
}

static void ufshid_block_enter_suspend(struct ufshid_dev *hid)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	struct device *dev = &hba->ufs_device_wlun->sdev_gendev;
	unsigned long flags;

#if defined(CONFIG_UFSHID_POC)
	if (unlikely(hid->block_suspend))
		return;

	hid->block_suspend = true;
#endif
	ufshcd_rpm_get_sync(hba);
	ufshcd_hold(hba, false);

	spin_lock_irqsave(hba->host->host_lock, flags);
	HID_DEBUG(hid,
		  "dev->power.usage_count %d hba->clk_gating.active_reqs %d",
		  atomic_read(&dev->power.usage_count),
		  hba->clk_gating.active_reqs);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

/*
 * If the return value is not err, pm_runtime_put_noidle() must be called once.
 * IMPORTANT : ufshid_hold_runtime_pm() & ufshid_release_runtime_pm() pair.
 */
static int ufshid_hold_runtime_pm(struct ufshid_dev *hid)
{
	struct ufs_hba *hba = hid->ufsf->hba;

	if (ufshid_get_state(hid->ufsf) == HID_SUSPEND) {
		/* Check that device was suspended by System PM */
		ufshcd_rpm_get_sync(hba);

		/* Guaranteed that ufsf_resume() is completed */
		down(&hba->host_sem);
		up(&hba->host_sem);

		/* If it success, device was suspended by Runtime PM */
		if (ufshid_get_state(hid->ufsf) == HID_PRESENT &&
		    hba->curr_dev_pwr_mode == UFS_ACTIVE_PWR_MODE &&
		    hba->uic_link_state == UIC_LINK_ACTIVE_STATE)
			goto resume_success;

		INFO_MSG("RPM resume failed. Maybe it was SPM suspend");
		INFO_MSG("UFS state (POWER = %d LINK = %d)",
			 hba->curr_dev_pwr_mode, hba->uic_link_state);

		ufsf_rpm_put_noidle(hba);
		return -ENODEV;
	}

	if (ufshid_is_not_present(hid))
		return -ENODEV;

	ufshcd_rpm_get_sync(hba);
resume_success:
	return 0;
}

static inline void ufshid_release_runtime_pm(struct ufshid_dev *hid)
{
	struct ufs_hba *hba = hid->ufsf->hba;

	ufsf_rpm_put_noidle(hba);
}

static void ufshid_allow_enter_suspend(struct ufshid_dev *hid)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	struct device *dev = &hba->ufs_device_wlun->sdev_gendev;
	unsigned long flags;

#if defined(CONFIG_UFSHID_POC)
	if (unlikely(!hid->block_suspend))
		return;

	hid->block_suspend = false;
#endif
	ufshcd_release(hba);
	ufsf_rpm_put_noidle(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	HID_DEBUG(hid,
		  "dev->power.usage_count %d hba->clk_gating.active_reqs %d",
		  atomic_read(&dev->power.usage_count),
		  hba->clk_gating.active_reqs);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

/*
 * Lock status: hid_sysfs lock was held when called.
 */
static int ufshid_trigger_off(struct ufshid_dev *hid)
	__must_hold(&hid->sysfs_lock)
{
	int ret;

	if (!hid->hid_trigger)
		return 0;

	ret = ufshid_hold_runtime_pm(hid);
	if (ret)
		return ret;

	hid->hid_trigger = false;
	HID_DEBUG(hid, "hid_trigger 1 -> 0");

	/*
	 * disable param mode before issue hid off operation
	 */
	ufshid_init_lba_trigger_mode(hid);
	ufshid_issue_disable(hid);

	ufshid_auto_hibern8_enable(hid, 1);

	ufshid_allow_enter_suspend(hid);

	ufshid_release_runtime_pm(hid);

	return 0;
}

static int ufshid_execute_query_op(struct ufshid_dev *hid)
{
	int ret;
	enum UFSHID_OP op;

	ret = ufshid_issue_lba_list(hid);
	if (ret)
		return ret;

	op = hid->lba_trigger_mode ? HID_OP_LBA_EXECUTE : HID_OP_EXECUTE;

	return ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, op);
}

/*
 * Lock status: hid_sysfs lock was held when called.
 */
static int ufshid_trigger_on(struct ufshid_dev *hid)
	__must_hold(&hid->sysfs_lock)
{
	int ret;

	if (hid->hid_trigger)
		return 0;

	ret = ufshid_hold_runtime_pm(hid);
	if (ret)
		return ret;

	hid->hid_trigger = true;
	HID_DEBUG(hid, "trigger 0 -> 1");

	ufshid_block_enter_suspend(hid);

	ufshid_auto_hibern8_enable(hid, 0);

	ret = ufshid_execute_query_op(hid);
	if (ret) {
		ufshid_release_runtime_pm(hid);
		goto err_out;
	}

	ufshid_schedule_delayed_work(&hid->hid_trigger_work, 0);

	ufshid_release_runtime_pm(hid);

	return 0;

err_out:
	ret = ufshid_trigger_off(hid);
	if (unlikely(ret))
		ERR_MSG("trigger off fail ret (%d)", ret);

	return ret;
}

static int ufshid_check_file_info_buf(struct ufshid_dev *hid,
				      unsigned char *buf, __u16 size)
{
	struct ufshid_blk_desc_header *desc_header;
	struct ufshid_blk_desc *desc, *comp_desc;
	const char *p = buf;
	int desc_cnt, total_desc, comp_cnt, desc_size, header_size;
	u32 lba, comp_lba;
	u32 blk_cnt, comp_blk_cnt;

	INFO_MSG("buf size %d", size);

	desc_header = (struct ufshid_blk_desc_header *)p;
	total_desc = desc_header->hid_blk_desc_cnt;
	if (!total_desc || total_desc > hid->max_lba_range_cnt ||
	    total_desc > HID_MAX_RANGE_CNT) {
		ERR_MSG("total_desc (%d). so check it", total_desc);
		return -EINVAL;
	}

	header_size = sizeof(struct ufshid_blk_desc_header);
	desc_size = sizeof(struct ufshid_blk_desc);

	p += header_size;
	for (desc_cnt = 0; desc_cnt < total_desc; desc_cnt++, p += desc_size) {
		const char *comp_p;

		desc = (struct ufshid_blk_desc *)p;
		lba = get_unaligned_be32(&desc->lba);
		blk_cnt = get_unaligned_be32(&desc->blk_cnt);

		INFO_MSG("desc_cnt[%d] lba %u blk_cnt %u",
			 desc_cnt, lba, blk_cnt);

		if (!lba || !blk_cnt) {
			ERR_MSG("desc[%d] info is not valid", desc_cnt);
			return -EINVAL;
		}

		if (blk_cnt > hid->max_lba_range_size) {
			ERR_MSG("desc[%d] blk_cnt (%d) is wrong. max %d",
				desc_cnt, blk_cnt, hid->max_lba_range_size);
			return -EINVAL;
		}

		comp_p = buf + header_size;
		for (comp_cnt = 0; comp_cnt < desc_cnt;
		     comp_cnt++, comp_p += desc_size) {
			comp_desc = (struct ufshid_blk_desc *)comp_p;
			comp_lba = get_unaligned_be32(&comp_desc->lba);
			comp_blk_cnt = get_unaligned_be32(&comp_desc->blk_cnt);

			if (lba + blk_cnt - 1 >= comp_lba &&
			    lba <= comp_lba + comp_blk_cnt - 1) {
				ERR_MSG("Overlapped: lba %u blk_cnt %u comp_lba %u comp_blk_cnt %u",
					lba, blk_cnt, comp_lba, comp_blk_cnt);
				return -EINVAL;
			}
		}
	}

	return 0;
}

void ufshid_clear_buffer(struct ufshid_dev *hid, unsigned char *buf, __u16 size)
{
	/*
	 * If it is the same as the file information currently being processed,
	 * keep the device's existing write buffer as it is.
	 */
	if (hid->hid_req.buf_size == size &&
	    !memcmp(hid->hid_req.buf, buf, size))
		return;

	ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, HID_OP_DISABLE);
}

int ufshid_send_file_info(struct ufshid_dev *hid, int lun, unsigned char *buf,
			  __u16 size, __u8 idn)
{
	int ret;

	if (!hid->l2p_defrag_sup)
		return -EINVAL;

	ret = ufshid_check_file_info_buf(hid, buf, size);
	if (ret)
		return ret;

	mutex_lock(&hid->sysfs_lock);
	if (hid->hid_trigger) {
		INFO_MSG("HID is in progress...");
		mutex_unlock(&hid->sysfs_lock);
		return -EPERM;
	}

	ufshid_clear_buffer(hid, buf, size);
	ufshid_set_lba_trigger_mode(hid, lun, buf, size);

	switch (idn) {
	case QUERY_ATTR_IDN_HID_OPERATION:
		ret = ufshid_trigger_on(hid);
		if (ret)
			INFO_MSG("trigger on is fail (%d)", ret);
		break;

	case QUERY_ATTR_IDN_HID_FRAG_LEVEL:
		ret = ufshid_get_frag_lvl_of_file(hid, buf);
		ufshid_init_lba_trigger_mode(hid);
		break;

	default:
		break;
	}
	mutex_unlock(&hid->sysfs_lock);

	return ret;
}

static inline bool ufshid_check_progress_end(u32 val)
{
	/*
	 * "val == 100" means defrag completed.
	 *
	 * There are several cases that "val == 0".
	 *  - Case 1: When HID is not required.
	 *  - Case 2: When a write type command is issued after defrag is completed.
	 *  - Case 3: When a write type query request not related to HID is issued.
	 */
	return val == 100 || val == 0;
}

static void ufshid_trigger_work_fn(struct work_struct *dwork)
{
	struct ufshid_dev *hid;
	u32 attr_val;
	int ret;

	hid = container_of(dwork, struct ufshid_dev, hid_trigger_work.work);

	if (ufshid_is_not_present(hid))
		return;

	HID_DEBUG(hid, "start hid_trigger_work_fn");

	mutex_lock(&hid->sysfs_lock);
	if (!hid->hid_trigger) {
		HID_DEBUG(hid, "hid_trigger == false, return");
		goto finish_work;
	}

	if (ufshid_is_in_progress(hid)) {
		HID_DEBUG(hid, "HID is in progress, so re-sched (%d ms)",
			  hid->hid_trigger_delay);
		goto resched;
	}

	ret = ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_PROGRESS_RATIO,
			       &attr_val);
	if (!ret && !ufshid_check_progress_end(attr_val)) {
		HID_DEBUG(hid, "HID is on-going(%d), so re-sched (%d ms)",
			  ret, hid->hid_trigger_delay);
		goto resched;
	}

	HID_DEBUG(hid, "HID is ended or err (%d), so trigger off", ret);

	ufshid_issue_disable(hid);
	msleep(200);
	ret = ufshid_analyze_and_get_attr(hid, QUERY_ATTR_IDN_HID_FRAG_LEVEL,
					  &attr_val);
	if (!ret)
		HID_DEBUG(hid, "Frag_lv %d Frag_mode %d Frag_stat %d HID_need_exec %d",
			  attr_val & HID_FRAG_LEVEL_MASK,
			  HID_FRAG_UPDATE_MODE(attr_val),
			  HID_FRAG_UPDATE_STAT(attr_val),
			  HID_EXECUTE_REQ_STAT(attr_val));

	ret = ufshid_trigger_off(hid);
	if (ret)
		WARN_MSG("trigger off fail.. must check it");

finish_work:
	mutex_unlock(&hid->sysfs_lock);

	return;

resched:
	mutex_unlock(&hid->sysfs_lock);

	ufshid_schedule_delayed_work(&hid->hid_trigger_work,
				     msecs_to_jiffies(hid->hid_trigger_delay));

	HID_DEBUG(hid, "end hid_trigger_work_fn");
}

static void ufshid_init_attr(struct ufshid_dev *hid)
{
	u32 attr_val;

	ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_FEAT_SUP, &attr_val);
	hid->l2p_defrag_sup = attr_val & HID_L2P_DEFRAG_SUP_MASK;

	if (hid->l2p_defrag_sup) {
		hid->l2p_defrag_threshold = HID_L2P_DEFRAG_THRESHOLD_DEFAULT;
		ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_L2P_DEFRAG_THRESHOLD,
				  hid->l2p_defrag_threshold);
	}

	hid->hid_size = HID_SIZE_DEFAULT;
	ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_SIZE, hid->hid_size);
}

void ufshid_init(struct ufsf_feature *ufsf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;
	int ret;

	INFO_MSG("HID_INIT_START");

	if (!hid) {
		ERR_MSG("hid is not found. it is very weired. must check it");
		ufshid_set_state(ufsf, HID_FAILED);
		return;
	}

	hid->hid_trigger = false;
	hid->hid_trigger_delay = HID_TRIGGER_WORKER_DELAY_MS_DEFAULT;
	INIT_DELAYED_WORK(&hid->hid_trigger_work, ufshid_trigger_work_fn);

	/* for LBA mode */
	ufshid_init_lba_trigger_mode(hid);

	hid->hid_debug = false;
#if defined(CONFIG_UFSHID_POC)
	hid->hid_debug = true;
	hid->block_suspend = false;
#endif

	/* If HCI supports auto hibern8, UFS Driver use it default */
	if (ufshcd_is_auto_hibern8_supported(ufsf->hba))
		hid->is_auto_enabled = true;
	else
		hid->is_auto_enabled = false;

	/* Save default Auto-Hibernate Idle Timer register value */
	hid->ahit = ufsf->hba->ahit;

	ufshid_init_attr(hid);

	ret = ufshid_create_sysfs(hid);
	if (ret) {
		ERR_MSG("sysfs init fail. so hid driver disabled");
		kfree(hid);
		ufshid_set_state(ufsf, HID_FAILED);
		return;
	}

	INFO_MSG("UFS HID create sysfs finished");

	ufshid_set_state(ufsf, HID_PRESENT);
}

void ufshid_reset_host(struct ufsf_feature *ufsf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (!hid)
		return;

	ufshid_set_state(ufsf, HID_RESET);
	cancel_delayed_work_sync(&hid->hid_trigger_work);
}

void ufshid_reset(struct ufsf_feature *ufsf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (!hid)
		return;

	ufshid_set_state(ufsf, HID_PRESENT);

	/*
	 * hid_trigger will be checked under sysfs_lock in worker.
	 */
	if (hid->hid_trigger)
		ufshid_schedule_delayed_work(&hid->hid_trigger_work, 0);

	if (hid->l2p_defrag_sup)
		/* Since it is a volatile attribute, write it again */
		ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_L2P_DEFRAG_THRESHOLD,
				  hid->l2p_defrag_threshold);

	INFO_MSG("reset completed.");
}

static inline void ufshid_remove_sysfs(struct ufshid_dev *hid)
{
	int ret;

	ret = kobject_uevent(&hid->kobj, KOBJ_REMOVE);
	INFO_MSG("kobject removed (%d)", ret);
	kobject_del(&hid->kobj);
}

void ufshid_remove(struct ufsf_feature *ufsf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;
	int ret;

	if (!hid)
		return;

	INFO_MSG("start HID release");

	mutex_lock(&hid->sysfs_lock);

	ret = ufshid_trigger_off(hid);
	if (unlikely(ret))
		ERR_MSG("trigger off fail ret (%d)", ret);

	ufshid_remove_sysfs(hid);

	ufshid_set_state(ufsf, HID_FAILED);

	mutex_unlock(&hid->sysfs_lock);

	cancel_delayed_work_sync(&hid->hid_trigger_work);

	kfree(hid);

	INFO_MSG("end HID release");
}

void ufshid_suspend(struct ufsf_feature *ufsf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (!hid)
		return;

	if (unlikely(hid->hid_trigger))
		ERR_MSG("hid_trigger was set to block the suspend. so weird");
	ufshid_set_state(ufsf, HID_SUSPEND);

	cancel_delayed_work_sync(&hid->hid_trigger_work);
}

/* Since it is a volatile attribute, write it again */
static void ufshid_restore_attr(struct ufshid_dev *hid)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	u32 attr_val = (u32)hid->l2p_defrag_threshold;
	u8 idn = QUERY_ATTR_IDN_HID_L2P_DEFRAG_THRESHOLD;
	int ret;

	pm_runtime_get(&hba->ufs_device_wlun->sdev_gendev);
	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, idn, 0,
				      0, &attr_val);
	ufsf_rpm_put_noidle(hba);

	if (!ret)
		HID_DEBUG(hid, "Restore attribute [0x%.2x] %u", idn, attr_val);
}

void ufshid_resume(struct ufsf_feature *ufsf, bool is_link_off)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (!hid)
		return;

	if (unlikely(hid->hid_trigger))
		ERR_MSG("hid_trigger need to off");
	ufshid_set_state(ufsf, HID_PRESENT);

	if (is_link_off && hid->l2p_defrag_sup)
		ufshid_restore_attr(hid);
}

/* sysfs function */
static ssize_t ufshid_sysfs_show_version(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("HID version (%.4X) D/D version (%.6X%s)",
		 UFSHID_VER, UFSHID_DD_VER, UFSHID_DD_VER_POST);

	return sysfs_emit(buf, "HID version (%.4X) D/D version (%.6X%s)\n",
			  UFSHID_VER, UFSHID_DD_VER, UFSHID_DD_VER_POST);
}

static ssize_t ufshid_sysfs_show_trigger(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("hid_trigger %d", hid->hid_trigger);

	return sysfs_emit(buf, "%d\n", hid->hid_trigger);
}

static ssize_t ufshid_sysfs_store_trigger(struct ufshid_dev *hid,
					  const char *buf, size_t count)
{
	unsigned long val;
	ssize_t ret;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	INFO_MSG("HID_trigger %lu", val);

	if (val == hid->hid_trigger)
		return count;

	if (val)
		ret = ufshid_trigger_on(hid);
	else
		ret = ufshid_trigger_off(hid);

	if (ret) {
		INFO_MSG("Changing trigger val %lu is fail (%ld)", val, ret);
		return ret;
	}

	return count;
}

static ssize_t ufshid_sysfs_show_trigger_interval(struct ufshid_dev *hid,
						  char *buf)
{
	INFO_MSG("hid_trigger_interval %d", hid->hid_trigger_delay);

	return sysfs_emit(buf, "%d\n", hid->hid_trigger_delay);
}

static ssize_t ufshid_sysfs_store_trigger_interval(struct ufshid_dev *hid,
						   const char *buf,
						   size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val < HID_TRIGGER_WORKER_DELAY_MS_MIN ||
	    val > HID_TRIGGER_WORKER_DELAY_MS_MAX) {
		INFO_MSG("hid_trigger_interval (min) %4dms ~ (max) %4dms",
			 HID_TRIGGER_WORKER_DELAY_MS_MIN,
			 HID_TRIGGER_WORKER_DELAY_MS_MAX);
		return -EINVAL;
	}

	hid->hid_trigger_delay = val;
	INFO_MSG("hid_trigger_interval %d", hid->hid_trigger_delay);

	return count;
}

static ssize_t ufshid_sysfs_show_hid_size(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("hid_size %llu KB", (u64)hid->hid_size * KB_PER_HID_SIZE_UNIT);

	return sysfs_emit(buf, "%llu KB\n",
			  (u64)hid->hid_size * KB_PER_HID_SIZE_UNIT);
}

static ssize_t ufshid_sysfs_store_hid_size(struct ufshid_dev *hid,
					   const char *buf, size_t count)
{
	u32 val;
	int ret;

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	ret = ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_SIZE, val);
	if (ret)
		return -EINVAL;

	hid->hid_size = val;

	INFO_MSG("hid_size %llu KB", (u64)hid->hid_size * KB_PER_HID_SIZE_UNIT);

	return count;
}

static ssize_t ufshid_sysfs_show_l2p_defrag_threshold(struct ufshid_dev *hid,
						      char *buf)
{
	INFO_MSG("l2p_defrag_threshold %u", hid->l2p_defrag_threshold);

	return sysfs_emit(buf, "%u\n", hid->l2p_defrag_threshold);
}

static ssize_t ufshid_sysfs_store_l2p_defrag_threshold(struct ufshid_dev *hid,
						       const char *buf,
						       size_t count)
{
	int ret;
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (!hid->l2p_defrag_sup)
		return -EINVAL;

	if (val > HID_L2P_MAX_THRESHOLD)
		return -EINVAL;

	ret = ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_L2P_DEFRAG_THRESHOLD,
				val);
	if (ret)
		return -EINVAL;

	hid->l2p_defrag_threshold = val;

	INFO_MSG("l2p_defrag_threshold %u", hid->l2p_defrag_threshold);

	return count;
}

static ssize_t ufshid_sysfs_show_debug(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("debug %d", hid->hid_debug);

	return sysfs_emit(buf, "%d\n", hid->hid_debug);
}

static ssize_t ufshid_sysfs_store_debug(struct ufshid_dev *hid, const char *buf,
					size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	hid->hid_debug = val ? true : false;

	INFO_MSG("debug %d", hid->hid_debug);

	return count;
}

static ssize_t ufshid_sysfs_show_color(struct ufshid_dev *hid, char *buf)
{
	u32 attr_val;
	int frag_level;
	bool param_mode;
	int ret;

	if (hid->hid_trigger) {
		INFO_MSG("HID is in progress...");
		return sysfs_emit(buf, "UNKNOWN\n");
	}

	ret = ufshid_analyze_and_get_attr(hid, QUERY_ATTR_IDN_HID_FRAG_LEVEL,
					  &attr_val);
	if (ret) {
		if (attr_val == RESULT_NOT_DEFRAG_REQUIRED) {
			INFO_MSG("Defrag is not required...");
			return sysfs_emit(buf, "UNKNOWN\n");
		}

		return ret;
	}

	frag_level = attr_val & HID_FRAG_LEVEL_MASK;
	param_mode = HID_FRAG_UPDATE_MODE(attr_val);

	HID_DEBUG(hid, "Frag_lv %d Frag_mode %d Frag_stat %d HID_need_exec %d",
		  frag_level, param_mode, HID_FRAG_UPDATE_STAT(attr_val),
		  HID_EXECUTE_REQ_STAT(attr_val));

	if ((hid->lba_trigger_mode && param_mode == HID_NO_PARAM) ||
	    (!hid->lba_trigger_mode && param_mode == HID_WITH_PARAM))
		frag_level = HID_LEV_UNKNOWN;

	return sysfs_emit(buf, "%s\n", frag_level == HID_LEV_RED ? "RED" :
			  frag_level == HID_LEV_YELLOW ? "YELLOW" :
			  frag_level == HID_LEV_GREEN ? "GREEN" :
			  frag_level == HID_LEV_GRAY ? "GRAY" : "UNKNOWN");
}

static ssize_t ufshid_sysfs_show_max_lba_range_size(struct ufshid_dev *hid,
						    char *buf)
{
	INFO_MSG("max_lba_range_size %u", hid->max_lba_range_size);

	return sysfs_emit(buf, "%u\n", hid->max_lba_range_size);
}

static ssize_t ufshid_sysfs_show_max_lba_range_cnt(struct ufshid_dev *hid,
						   char *buf)
{
	INFO_MSG("max_lba_range_cnt %u", hid->max_lba_range_cnt);

	return sysfs_emit(buf, "%u\n", hid->max_lba_range_cnt);
}

static ssize_t ufshid_sysfs_show_progress_ratio(struct ufshid_dev *hid,
						char *buf)
{
	u32 attr_val;

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_PROGRESS_RATIO, &attr_val))
		return -EINVAL;

	INFO_MSG("progress_ratio %u", attr_val);

	return sysfs_emit(buf, "%u\n", attr_val);
}

static ssize_t ufshid_sysfs_show_available_size(struct ufshid_dev *hid,
						char *buf)
{
	u32 attr_val;

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_AVAIL_SIZE, &attr_val))
		return -EINVAL;

	INFO_MSG("available_size %u", attr_val);

	return sysfs_emit(buf, "%u\n", attr_val);
}

#if defined(CONFIG_UFSHID_POC)
static ssize_t ufshid_sysfs_show_hid_state(struct ufshid_dev *hid, char *buf)
{
	static const char *const states[] = {
		"Analysis Required",
		"Analysis in Progress",
		"Defrag Required",
		"Defrag in Progress",
		"Defrag Completed or Not Required",
	};
	u32 attr_val;

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_STATE, &attr_val))
		return -EINVAL;

	if (attr_val >= HID_NUM_DEV_STATES)
		return -EINVAL;

	INFO_MSG("hid_state %s", states[attr_val]);

	return sysfs_emit(buf, "%s\n", states[attr_val]);
}

static ssize_t ufshid_sysfs_show_debug_op(struct ufshid_dev *hid, char *buf)
{
	u32 attr_val;

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, &attr_val))
		return -EINVAL;

	INFO_MSG("hid_op %d", attr_val);

	return sysfs_emit(buf, "%d\n", attr_val);
}

static ssize_t ufshid_sysfs_store_debug_op(struct ufshid_dev *hid,
					   const char *buf, size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val >= HID_OP_MAX)
		return -EINVAL;

	if (hid->hid_trigger) {
		ERR_MSG("debug_op cannot change, current hid_trigger is ON");
		return -EINVAL;
	}

	if (ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, val))
		return -EINVAL;

	INFO_MSG("hid_op %ld is set!", val);
	return count;
}

static ssize_t ufshid_sysfs_show_block_suspend(struct ufshid_dev *hid,
					       char *buf)
{
	INFO_MSG("block suspend %d", hid->block_suspend);

	return sysfs_emit(buf, "%d\n", hid->block_suspend);
}

static ssize_t ufshid_sysfs_store_block_suspend(struct ufshid_dev *hid,
						const char *buf, size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	INFO_MSG("HID_block_suspend %lu", val);

	if (val == hid->block_suspend)
		return count;

	if (val)
		ufshid_block_enter_suspend(hid);
	else
		ufshid_allow_enter_suspend(hid);

	hid->block_suspend = val ? true : false;

	return count;
}

static ssize_t ufshid_sysfs_show_auto_hibern8_enable(struct ufshid_dev *hid,
						     char *buf)
{
	INFO_MSG("HCI auto hibern8 %d", hid->is_auto_enabled);

	return sysfs_emit(buf, "%d\n", hid->is_auto_enabled);
}

static ssize_t ufshid_sysfs_store_auto_hibern8_enable(struct ufshid_dev *hid,
						      const char *buf,
						      size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	ufshid_auto_hibern8_enable(hid, val);

	return count;
}
#endif

/* SYSFS DEFINE */
#define define_sysfs_ro(_name) __ATTR(_name, 0444,			\
				      ufshid_sysfs_show_##_name, NULL)
#define define_sysfs_rw(_name) __ATTR(_name, 0664,			\
				      ufshid_sysfs_show_##_name,	\
				      ufshid_sysfs_store_##_name)

static struct ufshid_sysfs_entry ufshid_sysfs_entries[] = {
	define_sysfs_ro(version),
	define_sysfs_ro(color),
	define_sysfs_ro(max_lba_range_size),
	define_sysfs_ro(max_lba_range_cnt),
	define_sysfs_ro(progress_ratio),
	define_sysfs_ro(available_size),

	define_sysfs_rw(trigger),
	define_sysfs_rw(trigger_interval),
	define_sysfs_rw(hid_size),
	define_sysfs_rw(l2p_defrag_threshold),

	/* debug */
	define_sysfs_rw(debug),
#if defined(CONFIG_UFSHID_POC)
	define_sysfs_ro(hid_state),

	/* Attribute (RAW) */
	define_sysfs_rw(debug_op),
	define_sysfs_rw(block_suspend),
	define_sysfs_rw(auto_hibern8_enable),
#endif
	__ATTR_NULL
};

static ssize_t ufshid_attr_show(struct kobject *kobj, struct attribute *attr,
				char *page)
{
	struct ufshid_sysfs_entry *entry;
	struct ufshid_dev *hid;
	ssize_t error;

	entry = container_of(attr, struct ufshid_sysfs_entry, attr);
	if (!entry->show)
		return -EIO;

	hid = container_of(kobj, struct ufshid_dev, kobj);
	error = ufshid_hold_runtime_pm(hid);
	if (error)
		return error;

	mutex_lock(&hid->sysfs_lock);
	error = entry->show(hid, page);
	mutex_unlock(&hid->sysfs_lock);

	ufshid_release_runtime_pm(hid);
	return error;
}

static ssize_t ufshid_attr_store(struct kobject *kobj, struct attribute *attr,
				 const char *page, size_t length)
{
	struct ufshid_sysfs_entry *entry;
	struct ufshid_dev *hid;
	ssize_t error;

	entry = container_of(attr, struct ufshid_sysfs_entry, attr);
	if (!entry->store)
		return -EIO;

	hid = container_of(kobj, struct ufshid_dev, kobj);
	error = ufshid_hold_runtime_pm(hid);
	if (error)
		return error;

	mutex_lock(&hid->sysfs_lock);
	error = entry->store(hid, page, length);
	mutex_unlock(&hid->sysfs_lock);

	ufshid_release_runtime_pm(hid);
	return error;
}

static const struct sysfs_ops ufshid_sysfs_ops = {
	.show = ufshid_attr_show,
	.store = ufshid_attr_store,
};

static struct kobj_type ufshid_ktype = {
	.sysfs_ops = &ufshid_sysfs_ops,
	.release = NULL,
};

static int ufshid_create_sysfs(struct ufshid_dev *hid)
{
	struct device *dev = hid->ufsf->hba->dev;
	struct ufshid_sysfs_entry *entry;
	int err;

	hid->sysfs_entries = ufshid_sysfs_entries;

	kobject_init(&hid->kobj, &ufshid_ktype);
	mutex_init(&hid->sysfs_lock);

	INFO_MSG("ufshid creates sysfs ufshid %p dev->kobj %p",
		 &hid->kobj, &dev->kobj);

	err = kobject_add(&hid->kobj, kobject_get(&dev->kobj), "ufshid");
	if (!err) {
		for (entry = hid->sysfs_entries; entry->attr.name != NULL;
		     entry++) {
			INFO_MSG("ufshid sysfs attr creates: %s",
				 entry->attr.name);
			err = sysfs_create_file(&hid->kobj, &entry->attr);
			if (err) {
				ERR_MSG("create entry(%s) failed",
					entry->attr.name);
				goto kobj_del;
			}
		}
		kobject_uevent(&hid->kobj, KOBJ_ADD);
	} else {
		ERR_MSG("kobject_add failed");
	}

	return err;
kobj_del:
	err = kobject_uevent(&hid->kobj, KOBJ_REMOVE);
	INFO_MSG("kobject removed (%d)", err);
	kobject_del(&hid->kobj);
	return -EINVAL;
}

MODULE_LICENSE("GPL v2");
