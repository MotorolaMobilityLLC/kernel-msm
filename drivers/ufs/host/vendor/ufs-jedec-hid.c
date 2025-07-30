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
#include "ufshcd-priv.h"
#include "ufsfeature.h"
#include "ufs-jedec-hid.h"

static int ufshid_create_sysfs(struct ufshid_dev *hid);

const struct ufshid_offset ufshid_idn[] = {
	/* Attribute */
	[QUERY_ATTR_IDN_HID_OPERATION] = { 0x35 },
	[QUERY_ATTR_IDN_HID_AVAIL_SIZE] = { 0x36 },
	[QUERY_ATTR_IDN_HID_SIZE] = { 0x37 },
	[QUERY_ATTR_IDN_HID_PROGRESS_RATIO] = { 0x38 },
	[QUERY_ATTR_IDN_HID_STATE] = { 0x39 },
};

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

static inline u8 ufshid_hid_get_idn(struct ufshid_dev *hid,
				enum ufshid_idn_indx name)
{
	return ufshid_idn[name].offset;
}

static int ufshid_read_attr(struct ufshid_dev *hid, enum ufshid_idn_indx name, u32 *attr_val)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	int ret = 0;
	u8 idn;

	ufshcd_rpm_get_sync(hba);

	idn = ufshid_hid_get_idn(hid, name);
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

static int ufshid_write_attr(struct ufshid_dev *hid, enum ufshid_idn_indx name, u32 val)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	int ret = 0;
	u8 idn;

	ufshcd_rpm_get_sync(hba);

	idn = ufshid_hid_get_idn(hid, name);
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

void ufshid_get_dev_info(struct ufsf_feature *ufsf)
{
	u8 desc_buf[QUERY_DESC_MAX_SIZE];
	int ret = 0;
	struct ufs_hba *hba = ufsf->hba;
	int buff_len = QUERY_DESC_MAX_SIZE;

	ufsf->hid_dev = NULL;

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_query_descriptor_retry(hba, UPIU_QUERY_OPCODE_READ_DESC,
					QUERY_DESC_IDN_DEVICE, 0, 0, desc_buf, &buff_len);
	ufshcd_rpm_put_sync(hba);
	if (ret)
		goto err_out;

	if (!(get_unaligned_be32(desc_buf + DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP) &
	      UFS_FEATURE_SUPPORT_HID_BIT)) {
		INFO_MSG("bUFSExFeaturesSupport: HID not support");
		goto err_out;
	}

	INFO_MSG("bUFSExFeaturesSupport: HID support");

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

static inline void ufshid_issue_disable(struct ufshid_dev *hid)
{
	ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, HID_OP_DISABLE);
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
	ufshcd_hold(hba);
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
	ufshcd_hold(hba);

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

	/* Case of system suspend */
	if (ufshid_get_state(hid->ufsf) == HID_SUSPEND &&
	    !pm_runtime_suspended(&hba->ufs_device_wlun->sdev_gendev))
		return -ENODEV;

	/*
	 * After calling ufshcd_rpm_get_sync(),
	 * it is guaranteed that the wlun device is RPM_ACTIVE.
	 */
	ufshcd_rpm_get_sync(hba);

	/*
	 * Since HID resume is performed by a separate worker,
	 * it is sometimes judged to be in HID_SUSPEND state.
	 * Therefore, wait until ufshid_resume() changes the state
	 * of HID to HID_PRESENT.
	 */
	if (ufshid_get_state(hid->ufsf) == HID_SUSPEND &&
	    !wait_for_completion_timeout(&hid->resume_compl,
					 WAIT_HID_RESUME_TIMEOUT)) {
		WARN_MSG("Waiting for HID resume times out");
		return -ETIMEDOUT;
	}

	if (ufshid_is_not_present(hid))
		return -ENODEV;

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

	hid->is_analyze ? hid->is_analyze = false : ufshid_issue_disable(hid);

	ufshid_auto_hibern8_enable(hid, 1);

	ufshid_allow_enter_suspend(hid);

	ufshid_release_runtime_pm(hid);

	return 0;
}

/*
 * Lock status: hid_sysfs lock was held when called.
 */
static int ufshid_trigger_on(struct ufshid_dev *hid, enum UFSHID_OP op)
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

	ret = ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, op);
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

	hid->hid_size = HID_SIZE_DEFAULT;
	ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_SIZE, hid->hid_size);

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

#define SPM_ACTIVE_POWER_LEVEL			1
void ufshid_suspend(struct ufsf_feature *ufsf, bool is_system_pm)
{
	struct ufshid_dev *hid = ufsf->hid_dev;
	struct ufs_hba *hba = hid->ufsf->hba;
	int ret;

	if (!hid)
		return;

	if (!hid->hid_trigger)
		goto out;

	if (is_system_pm) {
		if (hba->spm_lvl <= SPM_ACTIVE_POWER_LEVEL) {
			if (ufshid_is_in_progress(hid))
				HID_DEBUG(hid, "HID is in progress");
		} else {
			HID_DEBUG(hid, "SPM Level is not 0 or 1. So HID will be off");
			ret = ufshid_trigger_off(hid);
			if (unlikely(ret))
				ERR_MSG("trigger off fail ret (%d)", ret);
		}
	} else {
		ERR_MSG("hid_trigger was set to block the runtime suspend. so weird");
	}

out:
	ufshid_set_state(ufsf, HID_SUSPEND);

	init_completion(&hid->resume_compl);

	cancel_delayed_work_sync(&hid->hid_trigger_work);
}

void ufshid_resume(struct ufsf_feature *ufsf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (!hid)
		return;

	ufshid_set_state(ufsf, HID_PRESENT);

	complete(&hid->resume_compl);

	if (hid->hid_trigger)
		ufshid_schedule_delayed_work(&hid->hid_trigger_work,
				msecs_to_jiffies(hid->hid_trigger_delay));
}

/* sysfs function */
static ssize_t ufshid_trigger_control(struct ufshid_dev *hid, unsigned long val,
				      enum UFSHID_OP op)
{
	ssize_t ret;

	if (val)
		ret = ufshid_trigger_on(hid, op);
	else
		ret = ufshid_trigger_off(hid);

	if (ret)
		INFO_MSG("Changing trigger val %lu is fail (%ld)", val, ret);

	return ret;
}

static ssize_t ufshid_sysfs_show_analyze(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("[Analyze Only] hid_trigger %d", hid->hid_trigger);

	return snprintf(buf, PAGE_SIZE, "%d\n", hid->hid_trigger);
}

static ssize_t ufshid_sysfs_store_analyze(struct ufshid_dev *hid,
					  const char *buf, size_t count)
{
	unsigned long val;
	ssize_t ret;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	INFO_MSG("HID_Analyze %lu", val);

	if (val == hid->hid_trigger)
		return count;

	/*
	 * This variable prevents HID_OP_DISABLE from being sent to the device
	 * when trigger off is performed after trigger operation.
         * During the evaluation process, it is necessary to check
         * the Defrag Required state after analysis.
	 */
	hid->is_analyze = true;

	ret = ufshid_trigger_control(hid, val, HID_OP_ANALYZE);
	if (ret)
		return ret;

	return count;
}

static ssize_t ufshid_sysfs_show_execute(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("[Analze & Execute] hid_trigger %d", hid->hid_trigger);

	return snprintf(buf, PAGE_SIZE, "%d\n", hid->hid_trigger);
}

static ssize_t ufshid_sysfs_store_execute(struct ufshid_dev *hid,
					  const char *buf, size_t count)
{
	unsigned long val;
	ssize_t ret;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	INFO_MSG("HID_Excute %lu", val);

	if (val == hid->hid_trigger)
		return count;

	ret = ufshid_trigger_control(hid, val, HID_OP_EXECUTE);
	if (ret)
		return ret;

	return count;
}

static ssize_t ufshid_sysfs_show_trigger(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("[Analyze & Execute] hid_trigger %d", hid->hid_trigger);

	return snprintf(buf, PAGE_SIZE, "%d\n", hid->hid_trigger);
}

static ssize_t ufshid_sysfs_store_trigger(struct ufshid_dev *hid,
					  const char *buf, size_t count)
{
	unsigned long val;
	ssize_t ret;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1 && val != 2)
		return -EINVAL;

	INFO_MSG("HID operation %lu", val);

	if (val == hid->hid_trigger)
		return count;

	if (val == 0) {
		INFO_MSG("HID stop HID operation!");
		ret = ufshid_trigger_off(hid);
	} else if (val == 1) {
		INFO_MSG("HID trigger analyze!");
		ret = ufshid_trigger_on(hid, HID_OP_ANALYZE);
	} else {
		INFO_MSG("HID trigger execute!");
		ret = ufshid_trigger_on(hid, HID_OP_EXECUTE);
	}
	if (ret)
		return ret;

	return count;
}

static ssize_t ufshid_sysfs_show_trigger_interval(struct ufshid_dev *hid,
						  char *buf)
{
	INFO_MSG("hid_trigger_interval %d", hid->hid_trigger_delay);

	return snprintf(buf, PAGE_SIZE, "%d\n", hid->hid_trigger_delay);
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

	return snprintf(buf, PAGE_SIZE, "%llu KB\n",
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

static ssize_t ufshid_sysfs_show_debug(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("debug %d", hid->hid_debug);

	return snprintf(buf, PAGE_SIZE, "%d\n", hid->hid_debug);
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

static ssize_t ufshid_sysfs_show_progress_ratio(struct ufshid_dev *hid,
						char *buf)
{
	u32 attr_val;

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_PROGRESS_RATIO, &attr_val))
		return -EINVAL;

	INFO_MSG("progress_ratio %u", attr_val);

	return snprintf(buf, PAGE_SIZE, "%u\n", attr_val);
}

static ssize_t ufshid_sysfs_show_available_size(struct ufshid_dev *hid,
						char *buf)
{
	u32 attr_val;

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_AVAIL_SIZE, &attr_val))
		return -EINVAL;

	if (attr_val == AVAIL_ANALYSIS_REQUIRED)
		return snprintf(buf, PAGE_SIZE, "Analysis Required\n");

	INFO_MSG("available_size %u", attr_val);

	return snprintf(buf, PAGE_SIZE, "%u\n", attr_val);
}

static ssize_t ufshid_sysfs_show_hid_state(struct ufshid_dev *hid, char *buf)
{
	static const char *const states[] = {
		"Analysis Required",
		"Analysis in Progress",
		"Defrag Required",
		"Defrag in Progress",
		"Defrag Completed",
		"Defrag is Not Required",
	};
	u32 attr_val;

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_STATE, &attr_val))
		return -EINVAL;

	if (attr_val >= HID_NUM_DEV_STATES)
		return -EINVAL;

	INFO_MSG("hid_state %s", states[attr_val]);

	return snprintf(buf, PAGE_SIZE, "%u\n", attr_val);
}

#if defined(CONFIG_UFSHID_POC)
static ssize_t ufshid_sysfs_show_debug_op(struct ufshid_dev *hid, char *buf)
{
	u32 attr_val;

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, &attr_val))
		return -EINVAL;

	INFO_MSG("hid_op %d", attr_val);

	return snprintf(buf, PAGE_SIZE, "%u\n", attr_val);
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

	return snprintf(buf, PAGE_SIZE, "%d\n", hid->block_suspend);
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

	return snprintf(buf, PAGE_SIZE, "%d\n", hid->is_auto_enabled);
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
#define define_sysfs_rw(_name) __ATTR(_name, 0644,			\
				      ufshid_sysfs_show_##_name,	\
				      ufshid_sysfs_store_##_name)

static struct ufshid_sysfs_entry ufshid_sysfs_entries[] = {
	define_sysfs_ro(progress_ratio),
	define_sysfs_ro(available_size),
	define_sysfs_rw(trigger),
	define_sysfs_rw(execute),
	define_sysfs_rw(trigger_interval),
	define_sysfs_rw(analyze),
	define_sysfs_rw(hid_size),
	define_sysfs_ro(hid_state),

	/* debug */
	define_sysfs_rw(debug),
#if defined(CONFIG_UFSHID_POC)

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
