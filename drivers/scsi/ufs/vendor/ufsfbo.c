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

#include "ufshcd.h"
#include "ufsfeature.h"
#include "ufsfbo.h"

static int ufsfbo_create_sysfs(struct ufsfbo_dev *fbo);

static inline void ufsfbo_rpm_put_noidle(struct ufs_hba *hba)
{
	pm_runtime_put_noidle(&hba->sdev_ufs_device->sdev_gendev);
}

inline int ufsfbo_get_state(struct ufsf_feature *ufsf)
{
	return atomic_read(&ufsf->fbo_state);
}

inline void ufsfbo_set_state(struct ufsf_feature *ufsf, int state)
{
	atomic_set(&ufsf->fbo_state, state);
}

static inline int ufsfbo_is_not_present(struct ufsfbo_dev *fbo)
{
	enum UFSFBO_STATE cur_state = ufsfbo_get_state(fbo->ufsf);

	if (cur_state != FBO_PRESENT) {
		INFO_MSG("fbo_state != FBO_PRESENT (%d)", cur_state);
		return -ENODEV;
	}
	return 0;
}

static int ufsfbo_write_attr(struct ufsfbo_dev *fbo, u8 idn, u32 val)
{
	struct ufs_hba *hba = fbo->ufsf->hba;
	int ret;

	ufshcd_rpm_get_sync(hba);

	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, idn, 0,
				      0, &val);

	ufsfbo_rpm_put_noidle(hba);

	if (ret)
		ERR_MSG("write attr [0x%.2X] failed. (%d)", idn, ret);
	else
		FBO_DEBUG(fbo, "fbo_attr write [0x%.2X] %u (0x%X)", idn, val,
			  val);

	return ret;
}

static int ufsfbo_read_attr(struct ufsfbo_dev *fbo, u8 idn, u32 *val)
{
	struct ufs_hba *hba = fbo->ufsf->hba;
	int ret;

	ufshcd_rpm_get_sync(hba);

	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, idn, 0,
				      0, val);

	ufsfbo_rpm_put_noidle(hba);

	if (ret)
		ERR_MSG("read attr [0x%.2X] failed. (%d)", idn, ret);
	else
		FBO_DEBUG(fbo, "fbo_attr read [0x%.2X] %u (0x%X)", idn, *val,
			  *val);

	return ret;
}

static int ufsfbo_read_desc(struct ufs_hba *hba, u8 desc_id, u8 desc_index,
			    u8 *desc_buf, int size)
{
	int ret;

	pm_runtime_get_sync(hba->dev);

	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					    desc_id, desc_index, 0, desc_buf,
					    &size);

	pm_runtime_put_noidle(hba->dev);

	if (ret)
		ERR_MSG("Reading desc failed. (%d)", ret);

	return ret;
}

static inline int ufsfbo_version_check(int spec_version)
{
	INFO_MSG("Support FBO Spec: Driver = (%.4x), Device = (%.4x)",
		 UFSFBO_VER, spec_version);
	INFO_MSG("FBO Driver version (%.6X%s)",
		 UFSFBO_DD_VER, UFSFBO_DD_VER_POST);

	if (spec_version != UFSFBO_VER) {
		ERR_MSG("UFS FBO version mismatched");
		return -ENODEV;
	}

	return 0;
}

void ufsfbo_read_fbo_desc(struct ufsf_feature *ufsf)
{
	u8 desc_buf[UFSF_QUERY_DESC_FBO_MAX_SIZE];
	struct ufsfbo_dev *fbo;
	int ret, spec_version;

	ufsf->fbo_dev = NULL;

	if (!is_vendor_device(ufsf->hba, UFS_VENDOR_SAMSUNG) &&
		    !is_vendor_device(ufsf->hba, UFS_VENDOR_TOSHIBA)){
		INFO_MSG("this fbo driver is not support");
		return;
	}

	ret = ufsfbo_read_desc(ufsf->hba, UFSF_QUERY_DESC_IDN_FBO, 0, desc_buf,
			       UFSF_QUERY_DESC_FBO_MAX_SIZE);
	if (ret) {
		ERR_MSG("reading FBO Desc failed. (%d)", ret);
		goto err_out;
	}

	spec_version = get_unaligned_be16(desc_buf + FBO_DESC_PARAM_FBO_VERSION);
	ret = ufsfbo_version_check(spec_version);
	if (ret)
		goto err_out;

	ufsf->fbo_dev = kzalloc(sizeof(struct ufsfbo_dev), GFP_KERNEL);
	if (!ufsf->fbo_dev) {
		ERR_MSG("fbo_dev memalloc failed");
		goto err_out;
	}

	fbo = ufsf->fbo_dev;

	fbo->rec_lba_range_size = get_unaligned_be32(desc_buf +
				FBO_DESC_PARAM_FBO_RECOMMENDED_LBA_RANGE_SIZE);
	fbo->max_lba_range_size = get_unaligned_be32(desc_buf +
					FBO_DESC_PARAM_FBO_MAX_LBA_RANGE_SIZE);
	fbo->min_lba_range_size = get_unaligned_be32(desc_buf +
					FBO_DESC_PARAM_FBO_MIN_LBA_RANGE_SIZE);
	fbo->max_lba_range_count = desc_buf[FBO_DESC_PARAM_FBO_MAX_LBA_RANGE_COUNT];
	fbo->lba_range_alignment = get_unaligned_be16(desc_buf +
					FBO_DESC_PARAM_FBO_LBA_RANGE_ALIGNMENT);
	INFO_MSG("[0x%.2x] dFBORecommendedLBARangeSize (%u)",
		 FBO_DESC_PARAM_FBO_RECOMMENDED_LBA_RANGE_SIZE,
		 fbo->rec_lba_range_size);
	INFO_MSG("[0x%.2x] dFBOMaxLBARangeSize (%u)",
		 FBO_DESC_PARAM_FBO_MAX_LBA_RANGE_SIZE,
		 fbo->max_lba_range_size);
	INFO_MSG("[0x%.2x] dFBOMinLBARangeSize (%u)",
		 FBO_DESC_PARAM_FBO_MIN_LBA_RANGE_SIZE,
		 fbo->min_lba_range_size);
	INFO_MSG("[0x%.2x] dFBOMaxLBARangeCount (%u)",
		 FBO_DESC_PARAM_FBO_MAX_LBA_RANGE_COUNT,
		 fbo->max_lba_range_count);
	INFO_MSG("[0x%.2x] wFBOLBARangeAlignment (%u)",
		 FBO_DESC_PARAM_FBO_LBA_RANGE_ALIGNMENT,
		 fbo->lba_range_alignment);

	fbo->ufsf = ufsf;

	return;
err_out:
	ufsfbo_set_state(ufsf, FBO_FAILED);
}

void ufsfbo_get_dev_info(struct ufsf_feature *ufsf)
{
	u8 desc_buf[QUERY_DESC_MAX_SIZE];
	struct ufs_hba *hba = ufsf->hba;
	int ret;

	ret = ufsfbo_read_desc(hba, QUERY_DESC_IDN_DEVICE, 0, desc_buf,
			       QUERY_DESC_MAX_SIZE);
	if (ret) {
		ERR_MSG("reading Device Desc failed. (%d)", ret);
		goto err_out;
	}

	if (!(get_unaligned_be32(desc_buf + DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP)
	      & UFS_FEATURE_SUPPORT_FBO_BIT)) {
		INFO_MSG("dExtendedUFSFeaturesSupport: FBO not support");
		goto err_out;
	}

	INFO_MSG("dExtendedUFSFeaturesSupport: FBO support");

	return;
err_out:
	ufsfbo_set_state(ufsf, FBO_FAILED);
}

static void ufsfbo_init_write_buffer(struct ufsfbo_dev *fbo, int lun,
				     unsigned char *buf, __u16 size)
{
	struct ufsfbo_req *wb = &fbo->write_buffer;

	wb->lun= lun;
	memcpy(wb->buf, buf, size);
	wb->buf_size = size;
}

static void ufsfbo_init_read_buffer(struct ufsfbo_dev *fbo, int lun,
				    unsigned char *buf, __u16 size)
{
	struct ufsfbo_req *rb = &fbo->read_buffer;

	rb->lun = lun;
	rb->buf_size = size;
}

static inline void ufsfbo_init_buffers(struct ufsfbo_dev *fbo, int lun,
				      unsigned char *buf,
				      __u16 size)
{
	ufsfbo_init_write_buffer(fbo, lun, buf, size);
	ufsfbo_init_read_buffer(fbo, lun, buf, size);
}

/*
 * If the return value is not err, pm_runtime_put_noidle() must be called once.
 * IMPORTANT : ufsfbo_hold_runtime_pm() & ufsfbo_release_runtime_pm() pair.
 */
static int ufsfbo_hold_runtime_pm(struct ufsfbo_dev *fbo)
{
	struct ufs_hba *hba = fbo->ufsf->hba;

	if (ufsfbo_get_state(fbo->ufsf) == FBO_SUSPEND) {
		/* Check that device was suspended by System PM */
		ufshcd_rpm_get_sync(hba);

		/*
		 * Guaranteed that ufsf_resume() is completed
		 */
		down(&hba->host_sem);
		up(&hba->host_sem);

		/* If it success, device was suspended by Runtime PM */
		if (ufsfbo_get_state(fbo->ufsf) == FBO_PRESENT &&
		    hba->curr_dev_pwr_mode == UFS_ACTIVE_PWR_MODE &&
		    hba->uic_link_state == UIC_LINK_ACTIVE_STATE)
			goto resume_success;

		INFO_MSG("RPM resume failed. Maybe it was SPM suspend");
		INFO_MSG("UFS state (POWER = %d LINK = %d)",
			 hba->curr_dev_pwr_mode, hba->uic_link_state);

		ufsfbo_rpm_put_noidle(hba);
		return -ENODEV;
	}

	if (ufsfbo_is_not_present(fbo))
		return -ENODEV;

	ufshcd_rpm_get_sync(hba);
resume_success:
	return 0;
}

static inline void ufsfbo_release_runtime_pm(struct ufsfbo_dev *fbo)
{
	struct ufs_hba *hba = fbo->ufsf->hba;

	ufsfbo_rpm_put_noidle(hba);
}

static int __ufsfbo_issue_buffer(struct ufsfbo_dev *fbo, unsigned char *cdb,
				 struct ufsfbo_req *fbo_req,
				 enum dma_data_direction dir)
{
	struct ufsf_feature *ufsf = fbo->ufsf;
	struct scsi_device *sdev;
	struct scsi_sense_hdr sshdr;
	int ret = 0, retries;

	sdev = ufsf->sdev_ufs_lu[fbo_req->lun];
	if (!sdev) {
		ERR_MSG("cannot find scsi_device [%d]", fbo_req->lun);
		return -ENODEV;
	}

	for (retries = 0; retries < 3; retries++) {
		ret = scsi_execute(sdev, cdb, dir, fbo_req->buf,
				   fbo_req->buf_size, NULL, &sshdr,
				   msecs_to_jiffies(30000), 0, 0, RQF_QUIET,
				   NULL);
		if (ret)
			ERR_MSG("%s for FBO failed. (%d) retries %d",
				dir == DMA_TO_DEVICE ? "WB" : "RB", ret, retries);
		else
			break;
	}

	INFO_MSG("%s for FBO %s", dir == DMA_TO_DEVICE ? "WB" : "RB",
		 ret ? "failed" : "success");

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

static int ufsfbo_issue_write_buffer(struct ufsfbo_dev *fbo)
{
	struct ufsfbo_req *fbo_req = &fbo->write_buffer;
	unsigned char cdb[10] = { 0 };
	int ret;

	if (!fbo_req->buf_size) {
		ERR_MSG("buf_size is 0. check it (%lu)", fbo_req->buf_size);
		return -EINVAL;
	}

	cdb[0] = WRITE_BUFFER;
	cdb[1] = WRITE_BUFFER_DATA_MODE;
	cdb[2] = WRITE_BUFFER_ID;
	put_unaligned_be24(fbo_req->buf_size, cdb + 6);

	ret = __ufsfbo_issue_buffer(fbo, cdb, fbo_req, DMA_TO_DEVICE);

	return ret;
}

static int ufsfbo_issue_read_buffer(struct ufsfbo_dev *fbo)
{
	struct ufsfbo_req *fbo_req = &fbo->read_buffer;
	unsigned char cdb[10] = { 0 };
	int ret;

	if (!fbo_req->buf_size) {
		ERR_MSG("buf_size is 0. check it (%lu)", fbo_req->buf_size);
		return -EINVAL;
	}

	cdb[0] = READ_BUFFER;
	cdb[1] = READ_BUFFER_DATA_MODE;
	cdb[2] = READ_BUFFER_ID;
	put_unaligned_be24(fbo_req->buf_size, cdb + 6);

	ret = __ufsfbo_issue_buffer(fbo, cdb, fbo_req, DMA_FROM_DEVICE);

	return ret;
}

static inline int ufsfbo_check_progress_state(struct ufsfbo_dev *fbo,
					      u32 *attr_val)
{
	if (ufsfbo_read_attr(fbo, QUERY_ATTR_IDN_FBO_PROGRESS_STATE, attr_val))
		return -EINVAL;

	FBO_DEBUG(fbo, "bFBOProgressState: %u", *attr_val);

	return 0;
}

static int ufsfbo_execute_analysis(struct ufsfbo_dev *fbo)
{
	int ret;

	ret = ufsfbo_issue_write_buffer(fbo);
	if (ret)
		return ret;

	if (ufsfbo_write_attr(fbo, QUERY_ATTR_IDN_FBO_CONTROL, FBO_OP_ANALYZE))
		return -EINVAL;

	return 0;
}

static void ufsfbo_block_enter_suspend(struct ufsfbo_dev *fbo)
{
	struct ufs_hba *hba = fbo->ufsf->hba;
	struct device *dev = &hba->sdev_ufs_device->sdev_gendev;
	unsigned long flags;

	ufshcd_rpm_get_sync(hba);
	ufshcd_hold(hba, false);

	spin_lock_irqsave(hba->host->host_lock, flags);
	FBO_DEBUG(fbo,
		  "dev->power.usage_count %d hba->clk_gating.active_reqs %d",
		  atomic_read(&dev->power.usage_count),
		  hba->clk_gating.active_reqs);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

/*
 * Lock status: fbo trigger_lock was held when called.
 */
static void ufsfbo_auto_hibern8_enable(struct ufsfbo_dev *fbo,
				       unsigned int val)
{
	struct ufs_hba *hba = fbo->ufsf->hba;
	unsigned long flags;
	u32 reg;

	val = !!val;

	/* Update auto hibern8 timer value if supported */
	if (!ufshcd_is_auto_hibern8_supported(hba))
		return;

	ufshcd_rpm_get_sync(hba);
	ufshcd_hold(hba, false);
	down_write(&hba->clk_scaling_lock);
	ufsf_scsi_block_requests(hba);
	/* wait for all the outstanding requests to finish */
	ufsf_wait_for_doorbell_clr(hba, U64_MAX);
	spin_lock_irqsave(hba->host->host_lock, flags);

	reg = ufshcd_readl(hba, REG_AUTO_HIBERNATE_IDLE_TIMER);
	INFO_MSG("ahit-reg 0x%X", reg);

	if (val ^ (reg != 0)) {
		if (val) {
			hba->ahit = fbo->ahit;
		} else {
			/*
			 * Store current ahit value.
			 * We don't know who set the ahit value to different
			 * from the initial value
			 */
			fbo->ahit = reg;
			hba->ahit = 0;
		}

		ufshcd_writel(hba, hba->ahit, REG_AUTO_HIBERNATE_IDLE_TIMER);

		/* Make sure the timer gets applied before further operations */
		mb();

		INFO_MSG("[Before] is_auto_enabled %d", fbo->is_auto_enabled);
		fbo->is_auto_enabled = val;

		reg = ufshcd_readl(hba, REG_AUTO_HIBERNATE_IDLE_TIMER);
		INFO_MSG("[After] is_auto_enabled %d ahit-reg 0x%X",
			 fbo->is_auto_enabled, reg);
	} else {
		INFO_MSG("is_auto_enabled %d. so it does not changed",
			 fbo->is_auto_enabled);
	}

	spin_unlock_irqrestore(hba->host->host_lock, flags);
	ufsf_scsi_unblock_requests(hba);
	up_write(&hba->clk_scaling_lock);
	ufshcd_release(hba);
	ufsfbo_rpm_put_noidle(hba);
}

static void ufsfbo_allow_enter_suspend(struct ufsfbo_dev *fbo)
{
	struct ufs_hba *hba = fbo->ufsf->hba;
	struct device *dev = &hba->sdev_ufs_device->sdev_gendev;
	unsigned long flags;

	ufshcd_release(hba);
	ufsfbo_rpm_put_noidle(hba);

	spin_lock_irqsave(hba->host->host_lock, flags);
	FBO_DEBUG(fbo,
		  "dev->power.usage_count %d hba->clk_gating.active_reqs %d",
		  atomic_read(&dev->power.usage_count),
		  hba->clk_gating.active_reqs);
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

static inline void ufsfbo_issue_disable(struct ufsfbo_dev *fbo)
{
	ufsfbo_write_attr(fbo, QUERY_ATTR_IDN_FBO_CONTROL, FBO_OP_DISABLE);
}

/*
 * Lock status: fbo trigger_lock was held when called.
 */
static int ufsfbo_trigger_off(struct ufsfbo_dev *fbo, bool init_control)
	__must_hold(&fbo->trigger_lock)
{
	int ret;

	if (!fbo->fbo_trigger)
		return 0;

	ret = ufsfbo_hold_runtime_pm(fbo);
	if (ret)
		return ret;

	fbo->fbo_trigger = false;
	FBO_DEBUG(fbo, "fbo_trigger 1 -> 0");

	if (init_control)
		ufsfbo_issue_disable(fbo);

	ufsfbo_auto_hibern8_enable(fbo, 1);

	ufsfbo_allow_enter_suspend(fbo);

	ufsfbo_release_runtime_pm(fbo);

	return 0;
}

static void ufsfbo_print_read_buffer_result(struct ufsfbo_dev *fbo)
{
	int total_entry, entry_cnt, entry_size;
	struct ufsfbo_rb_body *rb_body;
	struct ufsfbo_rb_entry *rb_entry;
	const char *rp = fbo->read_buffer.buf;
	__u8 cal_all_ranges;

	rp += sizeof(struct ufsfbo_buffer_header);

	rb_body = (struct ufsfbo_rb_body *)rp;
	cal_all_ranges = rb_body->cal_all_ranges;
	total_entry = rb_body->num_buffer_entries;

	if (cal_all_ranges)
		INFO_MSG("CAR %u, All Ranges Regression Level %u",
			 cal_all_ranges, rb_body->all_reg_level);

	rp += sizeof(struct ufsfbo_rb_body);

	entry_size = sizeof(struct ufsfbo_rb_entry);
	for (entry_cnt = 0; entry_cnt < total_entry; entry_cnt++, rp += entry_size) {
		rb_entry = (struct ufsfbo_rb_entry *)rp;
		INFO_MSG("CAR %u, entry_cnt %d Regression Level %u",
			 cal_all_ranges, entry_cnt + 1, rb_entry->reg_level);
	}
}

static void ufsfbo_parsing_read_buffer_data(struct ufsfbo_dev *fbo,
					    char *buf)
{
	int total_entry, entry_cnt, entry_size;
	struct ufsfbo_rb_body *rb_body;
	struct ufsfbo_rb_entry *rb_entry;
	const char *rp = fbo->read_buffer.buf;
	__u8 cal_all_ranges;
	int count = 0;

	rp += sizeof(struct ufsfbo_buffer_header);

	rb_body = (struct ufsfbo_rb_body *)rp;
	cal_all_ranges = rb_body->cal_all_ranges;
	total_entry = rb_body->num_buffer_entries;

	if (cal_all_ranges)
		count += snprintf(buf + count, 32,
				  "All Ranges Regression Level %2u\n",
				  rb_body->all_reg_level);

	rp += sizeof(struct ufsfbo_rb_body);

	count += snprintf(buf + count, 7, "CAR %1u\n", cal_all_ranges);

	entry_size = sizeof(struct ufsfbo_rb_entry);
	for (entry_cnt = 0; entry_cnt < total_entry; entry_cnt++, rp += entry_size) {
		rb_entry = (struct ufsfbo_rb_entry *)rp;
		count += snprintf(buf + count, 14, "N %3d lvl %2u\n",
				  entry_cnt + 1, rb_entry->reg_level);
	}
}

static void ufsfbo_trigger_fbo_work_fn(struct work_struct *dwork)
{
	bool init_control = true;
	struct ufsfbo_dev *fbo;
	u32 attr_val;
	int ret;

	fbo = container_of(dwork, struct ufsfbo_dev, fbo_trigger_work.work);

	if (ufsfbo_is_not_present(fbo))
		return;

	FBO_DEBUG(fbo, "start FBO worker");

	mutex_lock(&fbo->trigger_lock);
	ret = ufsfbo_check_progress_state(fbo, &attr_val);
	if (ret) {
		FBO_DEBUG(fbo, "check state err (%d), so trigger off", ret);
		goto finish_work;
	}

	switch (attr_val) {
	case FBO_OP_STATE_IDLE:
		ret = ufsfbo_execute_analysis(fbo);
		if (!ret)
			goto resched;

		FBO_DEBUG(fbo, "execute analysis err (%d), so trigger off", ret);
		break;

	case FBO_OP_STATE_ON_GOING:
		FBO_DEBUG(fbo, "on going operation, so re-sched (%u ms)",
			  fbo->fbo_trigger_delay);
		goto resched;

	case FBO_OP_STATE_COMPL_ANALYSIS:
		if (fbo->analysis_only) {
			init_control = false;
			FBO_DEBUG(fbo, "Complete analysis, so issue read buffer via sysfs");
			break;
		}

		ret = ufsfbo_issue_read_buffer(fbo);
		if (ret) {
			FBO_DEBUG(fbo, "issue read buffer err (%d), so trigger off", ret);
			break;
		}
		ufsfbo_print_read_buffer_result(fbo);

		ret = ufsfbo_write_attr(fbo, QUERY_ATTR_IDN_FBO_CONTROL,
					FBO_OP_EXECUTE);
		if (!ret)
			goto resched;

		FBO_DEBUG(fbo, "write attr err (%d), so trigger off", ret);
		break;

	case FBO_OP_STATE_COMPL_OPTIMIZATION:
		FBO_DEBUG(fbo, "complete optimization");
		break;

	case FBO_OP_STATE_ERROR:
		FBO_DEBUG(fbo, "General err (%d), so trigger off", ret);
		break;
	default:
		FBO_DEBUG(fbo, "invalid attr val (%d), so trigger off", attr_val);
	}
finish_work:
	ufsfbo_trigger_off(fbo, init_control);

	mutex_unlock(&fbo->trigger_lock);

	FBO_DEBUG(fbo, "end FBO worker");
	return;

resched:
	mutex_unlock(&fbo->trigger_lock);

	schedule_delayed_work(&fbo->fbo_trigger_work,
			      msecs_to_jiffies(fbo->fbo_trigger_delay));

	FBO_DEBUG(fbo, "re-schedule FBO worker");
}

static void ufsfbo_restore_attr(struct ufsf_feature *ufsf)
{
	struct ufsfbo_dev *fbo = ufsf->fbo_dev;
	int err;

	err = ufsfbo_write_attr(fbo, QUERY_ATTR_IDN_FBO_EXECUTE_THRESHOLD,
				fbo->execute_threshold);
	if (err)
		ERR_MSG("Write attr failed");
}

static void ufsfbo_init_threshold_attr(struct ufsfbo_dev *fbo)
{
	u8 idn = QUERY_ATTR_IDN_FBO_EXECUTE_THRESHOLD;
	struct ufs_hba *hba = fbo->ufsf->hba;
	struct device *dev = hba->dev;
	u32 val;
	int ret;

	/* Set default excution threshold value */
	fbo->execute_threshold = FBO_EXECUTE_THRESHOLD_DEFAULT;
	val = (u32)fbo->execute_threshold;

	pm_runtime_get_sync(dev);
	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
				      idn, 0, 0, &val);
	pm_runtime_put_noidle(dev);

	if (ret)
		ERR_MSG("write attr [0x%.2X] failed. (%d)", idn, ret);
	else
		FBO_DEBUG(fbo, "fbo_attr write [0x%.2X] %u (0x%X)", idn, val,
			  val);
}

void ufsfbo_init(struct ufsf_feature *ufsf)
{
	struct ufsfbo_dev *fbo = ufsf->fbo_dev;
	int ret;


	INFO_MSG("FBO_INIT_START");

	if (!fbo) {
		INFO_MSG("samsung fbo is not found. it may not support samsung fbo feature. check it");
		ufsfbo_set_state(ufsf, FBO_FAILED);
		return;
	}

	fbo->fbo_trigger = false;
	fbo->fbo_trigger_delay = FBO_TRIGGER_WORKER_DELAY_MS_DEFAULT;
	INIT_DELAYED_WORK(&fbo->fbo_trigger_work, ufsfbo_trigger_fbo_work_fn);

	ufsfbo_init_threshold_attr(fbo);

#if defined(CONFIG_UFSFBO_POC)
	fbo->get_pm = false;
#endif
	fbo->fbo_debug = false;

    //kioxia have different understand of length of LBARange.so just adjust to kioxia UFS.
	if(is_vendor_device(ufsf->hba,UFS_VENDOR_TOSHIBA)){
		fbo->transfer_len = 0;
	}else{
		fbo->transfer_len = BYTE_TO_BLK_SHIFT;
	}
	/* If HCI supports auto hibern8, UFS Driver use it default */
	if (ufshcd_is_auto_hibern8_supported(ufsf->hba))
		fbo->is_auto_enabled = true;
	else
		fbo->is_auto_enabled = false;

	/* Save default Auto-Hibernate Idle Timer register value */
	fbo->ahit = ufsf->hba->ahit;

	mutex_init(&fbo->trigger_lock);

	ret = ufsfbo_create_sysfs(fbo);
	if (ret) {
		ERR_MSG("sysfs init failed. So FBO driver disabled");
		kfree(fbo);
		ufsfbo_set_state(ufsf, FBO_FAILED);
		return;
	}

	INFO_MSG("UFS FBO create sysfs finished");

	ufsfbo_set_state(ufsf, FBO_PRESENT);
}

static int ufsfbo_check_lba_range_info_buf(struct ufsfbo_dev *fbo,
					   unsigned char *buf, __u16 size)
{
	struct ufsfbo_buffer_header *buffer_header;
	struct ufsfbo_wb_body *buffer_body;
	struct ufsfbo_wb_entry *entry, *comp_entry;
	const char *p = buf;
	int entry_cnt, total_entry, comp_cnt, entry_size, header_size, body_size;
	u32 lba, comp_lba, len, comp_len;

	INFO_MSG("buf size %d", size);

	buffer_header = (struct ufsfbo_buffer_header *)p;
	if (buffer_header->fbo_type != 0x0)
		return -EINVAL;

	header_size = sizeof(struct ufsfbo_buffer_header);
	p += header_size;

	buffer_body = (struct ufsfbo_wb_body *)p;
	total_entry = buffer_body->num_buffer_entries;
	if (!total_entry || total_entry > fbo->max_lba_range_count ||
	    total_entry > FBO_MAX_RANGE_COUNT) {
		ERR_MSG("total_entry (%d). so check it", total_entry);
		return -EINVAL;
	}

	body_size = sizeof(struct ufsfbo_wb_body);
	p += body_size;

	entry_size = sizeof(struct ufsfbo_wb_entry);
	for (entry_cnt = 0; entry_cnt < total_entry; entry_cnt++, p += entry_size) {
		const char *comp_p;
		__be32 length;

		entry = (struct ufsfbo_wb_entry *)p;
		length = (__be32)entry->length;
		lba = get_unaligned_be32(&entry->start_lba);
		len = get_unaligned_be24(&length) >> fbo->transfer_len;

		if (!lba || !len) {
			ERR_MSG("entry[%d] info is not valid", entry_cnt);
			return -EINVAL;
		}

		if (len > fbo->max_lba_range_size || len < fbo->min_lba_range_size) {
			ERR_MSG("entry[%d] len (%u) is wrong. max %u, min %u",
				entry_cnt, len, fbo->max_lba_range_size,
				fbo->min_lba_range_size);
			return -EINVAL;
		}

		INFO_MSG("entry_cnt[%d] lba %u len %u", entry_cnt, lba, len);

		comp_p = buf + header_size + body_size;
		for (comp_cnt = 0; comp_cnt < entry_cnt;
		     comp_cnt++, comp_p += entry_size) {
			__be32 comp_length;

			comp_entry = (struct ufsfbo_wb_entry *)comp_p;
			comp_length = (__be32)comp_entry->length;
			comp_lba = get_unaligned_be32(&comp_entry->start_lba);
			comp_len = get_unaligned_be24(&comp_length) >> fbo->transfer_len;

			if (lba + len - 1 >= comp_lba &&
			    lba <= comp_lba + comp_len - 1) {
				ERR_MSG("Overlapped: lba %u len %u comp_lba %u comp_len %u",
					lba, len, comp_lba, comp_len);
				return -EINVAL;
			}
		}
	}

	return 0;
}

/*
 * Lock status: fbo trigger_lock was held when called.
 */
static int ufsfbo_trigger_on(struct ufsfbo_dev *fbo)
	__must_hold(&fbo->trigger_lock)
{
	int ret;

	ret = ufsfbo_hold_runtime_pm(fbo);
	if (ret)
		return ret;

	if (fbo->fbo_trigger) {
		schedule_delayed_work(&fbo->fbo_trigger_work, 0);
		ufsfbo_release_runtime_pm(fbo);
		return 0;
	}

	fbo->fbo_trigger = true;
	FBO_DEBUG(fbo, "trigger 0 -> 1");

	ufsfbo_block_enter_suspend(fbo);

	ufsfbo_auto_hibern8_enable(fbo, 0);

	schedule_delayed_work(&fbo->fbo_trigger_work, 0);

	ufsfbo_release_runtime_pm(fbo);

	return 0;
}

int ufsfbo_send_file_info(struct ufsfbo_dev *fbo, int lun, unsigned char *buf,
			  __u16 size, __u8 idn, int opcode)
{
	int ret;

	ret = ufsfbo_check_lba_range_info_buf(fbo, buf, size);
	if (ret)
		return ret;

	mutex_lock(&fbo->trigger_lock);
	ufsfbo_init_buffers(fbo, lun, buf, size);

	ufsfbo_issue_disable(fbo);

	/*
	 * For analysis only, the opcode is set to UPIU_QUERY_OPCODE_READ_ATTR
	 * via ioctl
	 */
	fbo->analysis_only = (opcode == UPIU_QUERY_OPCODE_READ_ATTR);

	ret = ufsfbo_trigger_on(fbo);
	if (ret)
		INFO_MSG("Trigger on is fail (%d)", ret);
	mutex_unlock(&fbo->trigger_lock);

	return ret;
}

void ufsfbo_reset_host(struct ufsf_feature *ufsf)
{
	struct ufsfbo_dev *fbo = ufsf->fbo_dev;

	if (!fbo)
		return;

	ufsfbo_set_state(ufsf, FBO_RESET);
	cancel_delayed_work_sync(&fbo->fbo_trigger_work);
}

void ufsfbo_reset(struct ufsf_feature *ufsf)
{
	struct ufsfbo_dev *fbo = ufsf->fbo_dev;

	if (!fbo)
		return;

	ufsfbo_set_state(ufsf, FBO_PRESENT);

	/*
	 * fbo_trigger will be checked under trigger_lock in worker.
	 */
	if (fbo->fbo_trigger)
		schedule_delayed_work(&fbo->fbo_trigger_work, 0);

	ufsfbo_restore_attr(ufsf);

	INFO_MSG("reset completed.");
}

static inline void ufsfbo_remove_sysfs(struct ufsfbo_dev *fbo)
{
	int ret;

	ret = kobject_uevent(&fbo->kobj, KOBJ_REMOVE);
	INFO_MSG("kobject removed (%d)", ret);
	kobject_del(&fbo->kobj);
}

void ufsfbo_remove(struct ufsf_feature *ufsf)
{
	struct ufsfbo_dev *fbo = ufsf->fbo_dev;
	int ret;

	if (!fbo)
		return;

	INFO_MSG("start FBO release");

	mutex_lock(&fbo->trigger_lock);
	ret = ufsfbo_trigger_off(fbo, true);
	if (unlikely(ret))
		ERR_MSG("trigger off fail ret (%d)", ret);
	ufsfbo_set_state(ufsf, FBO_FAILED);
	mutex_unlock(&fbo->trigger_lock);

	mutex_lock(&fbo->sysfs_lock);
	ufsfbo_remove_sysfs(fbo);
	mutex_unlock(&fbo->sysfs_lock);

	cancel_delayed_work_sync(&fbo->fbo_trigger_work);

	kfree(fbo);

	INFO_MSG("end FBO release");
}

void ufsfbo_suspend(struct ufsf_feature *ufsf)
{
	struct ufsfbo_dev *fbo = ufsf->fbo_dev;

	if (!fbo)
		return;

	if (unlikely(fbo->fbo_trigger))
		ERR_MSG("fbo_trigger was set to block the suspend.");

	ufsfbo_set_state(ufsf, FBO_SUSPEND);

	cancel_delayed_work_sync(&fbo->fbo_trigger_work);
}

void ufsfbo_resume(struct ufsf_feature *ufsf, bool is_link_off)
{
	struct ufsfbo_dev *fbo = ufsf->fbo_dev;

	if (!fbo)
		return;

	if (is_link_off)
		ufsfbo_restore_attr(ufsf);

	ufsfbo_set_state(ufsf, FBO_PRESENT);

	/*
	 * If trigger off fails due to System PM, call ufsfbo_trigger_off() to
	 * release the blocked suspend.
	 */
	mutex_lock(&fbo->trigger_lock);
	if (unlikely(fbo->fbo_trigger))
		ufsfbo_trigger_off(fbo, true);
	mutex_unlock(&fbo->trigger_lock);
}

/* sysfs function */
static ssize_t ufsfbo_sysfs_show_version(struct ufsfbo_dev *fbo, char *buf)
{
	INFO_MSG("FBO version (%.4X) D/D version (%.6X%s)",
		 UFSFBO_VER, UFSFBO_DD_VER, UFSFBO_DD_VER_POST);

	return sysfs_emit(buf, "FBO version (%.4X) D/D version (%.6X%s)\n",
			  UFSFBO_VER, UFSFBO_DD_VER, UFSFBO_DD_VER_POST);
}

static ssize_t ufsfbo_sysfs_show_max_lba_range_size(struct ufsfbo_dev *fbo,
						    char *buf)
{
	INFO_MSG("max_lba_range_size %d", fbo->max_lba_range_size);

	return sysfs_emit(buf, "%d\n", fbo->max_lba_range_size);
}

static ssize_t ufsfbo_sysfs_show_min_lba_range_size(struct ufsfbo_dev *fbo,
						    char *buf)
{
	INFO_MSG("min_lba_range_size %d", fbo->min_lba_range_size);

	return sysfs_emit(buf, "%d\n", fbo->min_lba_range_size);
}

static ssize_t ufsfbo_sysfs_show_max_lba_range_count(struct ufsfbo_dev *fbo,
						     char *buf)
{
	INFO_MSG("max_lba_range_count %d", fbo->max_lba_range_count);

	return sysfs_emit(buf, "%d\n", fbo->max_lba_range_count);
}

static ssize_t ufsfbo_sysfs_show_lba_range_alignment(struct ufsfbo_dev *fbo,
						 char *buf)
{
	INFO_MSG("lba_range_alignment %d", fbo->lba_range_alignment);

	return sysfs_emit(buf, "%d\n", fbo->lba_range_alignment);
}

static ssize_t ufsfbo_sysfs_show_progress_state(struct ufsfbo_dev *fbo,
						char *buf)
{
	u32 attr_val;

	mutex_lock(&fbo->trigger_lock);
	if (ufsfbo_check_progress_state(fbo, &attr_val)) {
		mutex_unlock(&fbo->trigger_lock);
		return -EINVAL;
	}

#if defined(CONFIG_UFSFBO_POC)
	/*
	 * Paired with ufsfbo_sysfs_store_optimize()
	 */
	if (fbo->get_pm &&
	    (attr_val == FBO_OP_STATE_COMPL_OPTIMIZATION ||
	     attr_val == FBO_OP_STATE_ERROR)) {
		ufsfbo_auto_hibern8_enable(fbo, 1);
		ufsfbo_allow_enter_suspend(fbo);
		fbo->get_pm = false;
	}
#endif

	mutex_unlock(&fbo->trigger_lock);

	return sysfs_emit(buf, "%s\n",
			  attr_val == FBO_OP_STATE_IDLE ? "IDLE" :
			  attr_val == FBO_OP_STATE_ON_GOING ? "ON_GOING" :
			  attr_val == FBO_OP_STATE_COMPL_ANALYSIS ? "COMPL_ANALYSIS" :
			  attr_val == FBO_OP_STATE_COMPL_OPTIMIZATION ? "COMPL_OPTIMIZATION" :
			  "STATE_ERROR");
}

static ssize_t ufsfbo_sysfs_show_analyze(struct ufsfbo_dev *fbo, char *buf)
{
	u32 attr_val;
	int ret;

	mutex_lock(&fbo->trigger_lock);
	if (ufsfbo_check_progress_state(fbo, &attr_val))
		goto error;

	if (attr_val != FBO_OP_STATE_COMPL_ANALYSIS) {
		ERR_MSG("State error, check current state (%d)", attr_val);
		goto error;
	}

	ret = ufsfbo_issue_read_buffer(fbo);
	if (ret)
		goto error;

	ufsfbo_parsing_read_buffer_data(fbo, buf);
	mutex_unlock(&fbo->trigger_lock);

	return sysfs_emit(buf, "%s\n", buf);
error:
	mutex_unlock(&fbo->trigger_lock);
	return -EINVAL;
}

static ssize_t ufsfbo_sysfs_show_trigger_interval(struct ufsfbo_dev *fbo,
						  char *buf)
{
	INFO_MSG("fbo_trigger_interval %d", fbo->fbo_trigger_delay);

	return sysfs_emit(buf, "%d\n", fbo->fbo_trigger_delay);
}

static ssize_t ufsfbo_sysfs_store_trigger_interval(struct ufsfbo_dev *fbo,
						   const char *buf,
						   size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val < FBO_TRIGGER_WORKER_DELAY_MS_MIN ||
	    val > FBO_TRIGGER_WORKER_DELAY_MS_MAX) {
		INFO_MSG("fbo_trigger_interval (min) %4dms ~ (max) %4dms",
			 FBO_TRIGGER_WORKER_DELAY_MS_MIN,
			 FBO_TRIGGER_WORKER_DELAY_MS_MAX);
		return -EINVAL;
	}

	fbo->fbo_trigger_delay = val;
	INFO_MSG("fbo_trigger_interval %d", fbo->fbo_trigger_delay);

	return count;
}

static ssize_t ufsfbo_sysfs_show_execute_threshold(struct ufsfbo_dev *fbo,
						   char *buf)
{
	INFO_MSG("execute_threshold %u", fbo->execute_threshold);

	return sysfs_emit(buf, "%u\n", fbo->execute_threshold);
}

static ssize_t ufsfbo_sysfs_store_execute_threshold(struct ufsfbo_dev *fbo,
						    const char *buf,
						    size_t count)
{
	int ret;
	u8 val;

	if (kstrtou8(buf, 0, &val))
		return -EINVAL;

	if (val > FBO_MAX_EXECUTE_THRESHOLD)
		return -EINVAL;

	ret = ufsfbo_write_attr(fbo, QUERY_ATTR_IDN_FBO_EXECUTE_THRESHOLD, val);
	if (ret)
		return -EINVAL;

	fbo->execute_threshold = val;

	INFO_MSG("execute_threshold %u", fbo->execute_threshold);

	return count;
}

#if defined(CONFIG_UFSFBO_POC)
static ssize_t ufsfbo_sysfs_store_optimize(struct ufsfbo_dev *fbo,
					   const char *buf,
					   size_t count)
{
	unsigned int val;
	u32 attr_val;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val != RUN_OPTIMIZATION)
		return -EINVAL;

	mutex_lock(&fbo->trigger_lock);
	if (ufsfbo_check_progress_state(fbo, &attr_val))
		goto error;

	if (attr_val != FBO_OP_STATE_COMPL_ANALYSIS) {
		ERR_MSG("First, need to run analysis in FBO application");
		goto error;
	}

	/*
	 * It must be released in the ufsfbo_sysfs_show_progress_state()
	 */
	if (!fbo->get_pm) {
		fbo->get_pm = true;
		ufsfbo_block_enter_suspend(fbo);
		ufsfbo_auto_hibern8_enable(fbo, 0);
	}

	if (ufsfbo_write_attr(fbo, QUERY_ATTR_IDN_FBO_CONTROL, FBO_OP_EXECUTE))
		goto error;

	mutex_unlock(&fbo->trigger_lock);

	INFO_MSG("Success: Run optimization");

	return count;
error:
	mutex_unlock(&fbo->trigger_lock);
	return -EINVAL;
}
#endif

static ssize_t ufsfbo_sysfs_show_debug(struct ufsfbo_dev *fbo, char *buf)
{
	INFO_MSG("debug %d", fbo->fbo_debug);

	return sysfs_emit(buf, "%d\n", fbo->fbo_debug);
}

static ssize_t ufsfbo_sysfs_store_debug(struct ufsfbo_dev *fbo, const char *buf,
					size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	fbo->fbo_debug = val ? true : false;

	INFO_MSG("debug %d", fbo->fbo_debug);

	return count;
}

/* SYSFS DEFINE */
#define define_sysfs_ro(_name) __ATTR(_name, 0444,			\
				      ufsfbo_sysfs_show_##_name, NULL)
#define define_sysfs_rw(_name) __ATTR(_name, 0644,			\
				      ufsfbo_sysfs_show_##_name,	\
				      ufsfbo_sysfs_store_##_name)
#define define_sysfs_wo(_name) __ATTR(_name, 0220, NULL,		\
				      ufsfbo_sysfs_store_##_name)

static struct ufsfbo_sysfs_entry ufsfbo_sysfs_entries[] = {
	define_sysfs_ro(version),
	define_sysfs_ro(max_lba_range_size),
	define_sysfs_ro(min_lba_range_size),
	define_sysfs_ro(max_lba_range_count),
	define_sysfs_ro(lba_range_alignment),
	define_sysfs_ro(progress_state),
	define_sysfs_ro(analyze),

	define_sysfs_rw(trigger_interval),
	define_sysfs_rw(execute_threshold),

#if defined(CONFIG_UFSFBO_POC)
	define_sysfs_wo(optimize),
#endif

	/* debug */
	define_sysfs_rw(debug),
	__ATTR_NULL,
};

static ssize_t ufsfbo_attr_show(struct kobject *kobj, struct attribute *attr,
				char *page)
{
	struct ufsfbo_sysfs_entry *entry;
	struct ufsfbo_dev *fbo;
	ssize_t error;

	entry = container_of(attr, struct ufsfbo_sysfs_entry, attr);
	if (!entry->show)
		return -EIO;

	fbo = container_of(kobj, struct ufsfbo_dev, kobj);
	error = ufsfbo_hold_runtime_pm(fbo);
	if (error)
		return error;

	mutex_lock(&fbo->sysfs_lock);
	error = entry->show(fbo, page);
	mutex_unlock(&fbo->sysfs_lock);

	ufsfbo_release_runtime_pm(fbo);
	return error;
}

static ssize_t ufsfbo_attr_store(struct kobject *kobj, struct attribute *attr,
				 const char *page, size_t length)
{
	struct ufsfbo_sysfs_entry *entry;
	struct ufsfbo_dev *fbo;
	ssize_t error;

	entry = container_of(attr, struct ufsfbo_sysfs_entry, attr);
	if (!entry->store)
		return -EIO;

	fbo = container_of(kobj, struct ufsfbo_dev, kobj);
	error = ufsfbo_hold_runtime_pm(fbo);
	if (error)
		return error;

	mutex_lock(&fbo->sysfs_lock);
	error = entry->store(fbo, page, length);
	mutex_unlock(&fbo->sysfs_lock);

	ufsfbo_release_runtime_pm(fbo);
	return error;
}

static const struct sysfs_ops ufsfbo_sysfs_ops = {
	.show = ufsfbo_attr_show,
	.store = ufsfbo_attr_store,
};

static struct kobj_type ufsfbo_ktype = {
	.sysfs_ops = &ufsfbo_sysfs_ops,
	.release = NULL,
};

static int ufsfbo_create_sysfs(struct ufsfbo_dev *fbo)
{
	struct device *dev = fbo->ufsf->hba->dev;
	struct ufsfbo_sysfs_entry *entry;
	int err;

	fbo->sysfs_entries = ufsfbo_sysfs_entries;

	kobject_init(&fbo->kobj, &ufsfbo_ktype);
	mutex_init(&fbo->sysfs_lock);

	INFO_MSG("ufsfbo creates sysfs ufsfbo %p dev->kobj %p",
		 &fbo->kobj, &dev->kobj);

	err = kobject_add(&fbo->kobj, kobject_get(&dev->kobj), "ufsfbo");
	if (!err) {
		for (entry = fbo->sysfs_entries; entry->attr.name != NULL;
		     entry++) {
			INFO_MSG("ufsfbo sysfs attr creates: %s",
				 entry->attr.name);
			err = sysfs_create_file(&fbo->kobj, &entry->attr);
			if (err) {
				ERR_MSG("create entry(%s) failed",
					entry->attr.name);
				goto kobj_del;
			}
		}
		kobject_uevent(&fbo->kobj, KOBJ_ADD);
	} else {
		ERR_MSG("kobject_add failed");
	}

	return err;
kobj_del:
	err = kobject_uevent(&fbo->kobj, KOBJ_REMOVE);
	INFO_MSG("kobject removed (%d)", err);
	kobject_del(&fbo->kobj);
	return -EINVAL;
}

MODULE_LICENSE("GPL v2");
