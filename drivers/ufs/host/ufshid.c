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
				      UFSFEATURE_SELECTOR, attr_val);
	if (ret) {
		ERR_MSG("read attr [0x%.2X] fail. (%d)", idn, ret);
		goto err_out;
	}

	HID_DEBUG(hid, "hid_attr read [0x%.2X] %u (0x%X)", idn, *attr_val,
		  *attr_val);
	TMSG(hid->ufsf, 0, "[ufshid] read_attr IDN %s (%d)",
	     idn == QUERY_ATTR_IDN_HID_OPERATION ? "HID_OP" :
	     idn == QUERY_ATTR_IDN_HID_FRAG_LEVEL ? "HID_LEV" : "UNKNOWN", idn);
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
				      UFSFEATURE_SELECTOR, &val);
	if (ret) {
		ERR_MSG("write attr [0x%.2X] fail. (%d)", idn, ret);
		goto err_out;
	}

	HID_DEBUG(hid, "hid_attr write [0x%.2X] %u (0x%X)", idn, val, val);
	TMSG(hid->ufsf, 0, "[ufshid] write_attr IDN %s (%d)",
	     idn == QUERY_ATTR_IDN_HID_OPERATION ? "HID_OP" :
	     idn == QUERY_ATTR_IDN_HID_FRAG_LEVEL ? "HID_LEV" : "UNKNOWN", idn);
err_out:
	ufsf_rpm_put_noidle(hba);

	return ret;
}

static inline void ufshid_version_check(int spec_version)
{
	INFO_MSG("Support HID Spec : Driver = (%.4x), Device = (%.4x)",
		 UFSHID_VER, spec_version);
	INFO_MSG("HID Driver version (%.6X%s)",
		 UFSHID_DD_VER, UFSHID_DD_VER_POST);
}

void ufshid_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf)
{
	int spec_version;

	ufsf->hid_dev = NULL;

	if (!(get_unaligned_be32(desc_buf + DEVICE_DESC_PARAM_EX_FEAT_SUP) &
	      UFS_FEATURE_SUPPORT_HID_BIT)) {
		INFO_MSG("bUFSExFeaturesSupport: HID not support");
		goto err_out;
	}

	INFO_MSG("bUFSExFeaturesSupport: HID support");
	spec_version = get_unaligned_be16(desc_buf + DEVICE_DESC_PARAM_HID_VER);
	ufshid_version_check(spec_version);

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

static int ufshid_execute_query_op(struct ufshid_dev *hid, enum UFSHID_OP op,
				   u32 *attr_val)
{
	if (ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, op))
		return -EINVAL;

	msleep(200);

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_FRAG_LEVEL, attr_val))
		return -EINVAL;

	HID_DEBUG(hid, "Frag_lv %d Frag_stat %d HID_need_exec %d",
		  HID_GET_FRAG_LEVEL(*attr_val),
		  HID_FRAG_UPDATE_STAT(*attr_val),
		  HID_EXECUTE_REQ_STAT(*attr_val));

	return 0;
}

static int ufshid_get_analyze_and_issue_execute(struct ufshid_dev *hid)
{
	u32 attr_val;
	int ret;

	ret = ufshid_execute_query_op(hid, HID_OP_EXECUTE, &attr_val);
	if (ret)
		return ret;

	if (HID_GET_FRAG_LEVEL(attr_val) == HID_LEV_GRAY)
		return -EAGAIN;

	return (HID_EXECUTE_REQ_STAT(attr_val)) ?
		HID_REQUIRED : HID_NOT_REQUIRED;
}

static inline void ufshid_issue_disable(struct ufshid_dev *hid)
{
	u32 attr_val;

	ufshid_execute_query_op(hid, HID_OP_DISABLE, &attr_val);
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

/*
 *  The method using HID for each vendor may be different.
 *  So, the functions were exposed to direct control in other kernel processes.
 *
 *  Lock status: hid->sysfs_lock was held when called.
 */
int ufshid_trigger_on(struct ufshid_dev *hid) __must_hold(&hid->sysfs_lock)
{
	int ret;

	lockdep_assert_held(&hid->sysfs_lock);

	if (hid->hid_trigger)
		return 0;

	ret = ufshid_hold_runtime_pm(hid);
	if (ret)
		return ret;

	hid->hid_trigger = true;
	HID_DEBUG(hid, "trigger 0 -> 1");

	ufshid_block_enter_suspend(hid);

	ufshid_auto_hibern8_enable(hid, 0);

	ufshid_schedule_delayed_work(&hid->hid_trigger_work, 0);

	ufshid_release_runtime_pm(hid);

	return 0;
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
 *  The method using HID for each vendor may be different.
 *  So, the functions were exposed to direct control in other kernel processes.
 *
 *  Lock status: hid->sysfs_lock was held when called.
 */
int ufshid_trigger_off(struct ufshid_dev *hid) __must_hold(&hid->sysfs_lock)
{
	int ret;

	lockdep_assert_held(&hid->sysfs_lock);

	if (!hid->hid_trigger)
		return 0;

	ret = ufshid_hold_runtime_pm(hid);
	if (ret)
		return ret;

	hid->hid_trigger = false;
	HID_DEBUG(hid, "hid_trigger 1 -> 0");

	ufshid_issue_disable(hid);

	ufshid_auto_hibern8_enable(hid, 1);

	ufshid_allow_enter_suspend(hid);

	ufshid_release_runtime_pm(hid);

	return 0;
}

#if defined(CONFIG_UFSHID_DEBUG)
static void ufshid_print_hid_info(struct ufshid_dev *hid)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	unsigned long flags;

	spin_lock_irqsave(hba->host->host_lock, flags);
	HID_DEBUG(hid, "r_cnt %llu w_cnt %llu r_sectors %llu w_sectors %llu "
		  "w_query_cnt %llu", hid->read_cnt, hid->write_cnt,
		  hid->read_sec, hid->write_sec, hid->write_query_cnt);

	hid->read_cnt = hid->write_cnt = hid->read_sec = hid->write_sec = 0;
	hid->write_query_cnt = 0;
	spin_unlock_irqrestore(hba->host->host_lock, flags);
}

/*
 * Lock status: hba->host->host_lock was held when called.
 * So, Don't need to use atomic operation for stats.
 */
void ufshid_acc_io_stat_during_trigger(struct ufsf_feature *ufsf,
				       struct ufshcd_lrb *lrbp)
				       __must_hold(ufsf->hba->host->host_lock)
{
	struct ufs_hba *hba = ufsf->hba;
	struct scsi_cmnd *scmd = lrbp->cmd;
	struct ufshid_dev *hid = ufsf->hid_dev;
	struct ufs_query_req *request;
	struct request *req;

	lockdep_assert_held(hba->host->host_lock);

	if (!hid || !hid->hid_trigger)
		return;

	if (scmd) {
		req = scsi_cmd_to_rq(scmd);
		if (scmd->sc_data_direction == DMA_FROM_DEVICE) {
			hid->read_cnt++;
			hid->read_sec += blk_rq_sectors(req);
		} else {
			hid->write_cnt++;
			hid->write_sec += blk_rq_sectors(req);
		}
	} else {
		request = &hba->dev_cmd.query.request;

		switch (request->upiu_req.opcode) {
		case UPIU_QUERY_OPCODE_WRITE_DESC:
		case UPIU_QUERY_OPCODE_WRITE_ATTR:
			if (request->upiu_req.idn ==
			    QUERY_ATTR_IDN_HID_OPERATION)
				break;
			fallthrough;
		case UPIU_QUERY_OPCODE_SET_FLAG:
		case UPIU_QUERY_OPCODE_CLEAR_FLAG:
		case UPIU_QUERY_OPCODE_TOGGLE_FLAG:
			hid->write_query_cnt++;
			break;
		default:
			break;
		}
	}
}
#endif

static void ufshid_trigger_work_fn(struct work_struct *dwork)
{
	struct ufshid_dev *hid;
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

	ret = ufshid_get_analyze_and_issue_execute(hid);
	if (ret == HID_REQUIRED || ret == -EAGAIN) {
		HID_DEBUG(hid, "REQUIRED or AGAIN (%d), so re-sched (%u ms)",
			  ret, hid->hid_trigger_delay);
	} else {
		HID_DEBUG(hid, "NOT_REQUIRED or err (%d), so trigger off", ret);
		ret = ufshid_trigger_off(hid);
		if (likely(!ret))
			goto finish_work;

		WARN_MSG("trigger off fail.. must check it");
	}
	mutex_unlock(&hid->sysfs_lock);
#if defined(CONFIG_UFSHID_DEBUG)
	ufshid_print_hid_info(hid);
#endif
	ufshid_schedule_delayed_work(&hid->hid_trigger_work,
				     msecs_to_jiffies(hid->hid_trigger_delay));

	HID_DEBUG(hid, "end hid_trigger_work_fn");
	return;
finish_work:
	mutex_unlock(&hid->sysfs_lock);
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
	hid->hid_on_idle_delay = HID_ON_IDLE_DELAY_MS_DEFAULT;
	INIT_DELAYED_WORK(&hid->hid_trigger_work, ufshid_trigger_work_fn);

	hid->hid_debug = false;
#if defined(CONFIG_UFSHID_POC)
	hid->hid_debug = true;
	hid->block_suspend = false;
#endif

	/* If HCI supports auto hibern8, UFS Driver use it default */
	hid->is_auto_enabled = ufshcd_is_auto_hibern8_supported(ufsf->hba);

	/* Save default Auto-Hibernate Idle Timer register value */
	hid->ahit = ufsf->hba->ahit;

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

void ufshid_resume(struct ufsf_feature *ufsf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (!hid)
		return;

	if (unlikely(hid->hid_trigger))
		ERR_MSG("hid_trigger need to off");
	ufshid_set_state(ufsf, HID_PRESENT);
}

/*
 * this function is called in irq context.
 * so cancel_delayed_work_sync() do not use due to waiting.
 */
void ufshid_on_idle(struct ufsf_feature *ufsf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (!hid)
		return;
	/*
	 * When hid_trigger_work will be scheduled,
	 * check hid_trigger under sysfs_lock.
	 */
	if (!hid->hid_trigger)
		return;

	if (delayed_work_pending(&hid->hid_trigger_work))
		cancel_delayed_work(&hid->hid_trigger_work);

	ufshid_schedule_delayed_work(&hid->hid_trigger_work,
				     hid->hid_on_idle_delay);
}

/* sysfs function */
static ssize_t ufshid_sysfs_show_version(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("HID version (%.4X) D/D version (%.6X%s)",
		 UFSHID_VER, UFSHID_DD_VER, UFSHID_DD_VER_POST);

	return snprintf(buf, PAGE_SIZE,
			"HID version (%.4X) D/D version (%.6X%s)\n",
			UFSHID_VER, UFSHID_DD_VER, UFSHID_DD_VER_POST);
}

static ssize_t ufshid_sysfs_show_trigger(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("hid_trigger %u", hid->hid_trigger);

	return snprintf(buf, PAGE_SIZE, "%u\n", hid->hid_trigger);
}

static ssize_t ufshid_sysfs_store_trigger(struct ufshid_dev *hid,
					  const char *buf, size_t count)
{
	unsigned int val;
	ssize_t ret;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	INFO_MSG("HID_trigger %u", val);

	if (val == hid->hid_trigger)
		return count;

	ret = val ? ufshid_trigger_on(hid) : ufshid_trigger_off(hid);

	if (ret) {
		INFO_MSG("Changing trigger val %u is fail (%ld)", val, ret);
		return ret;
	}

	return count;
}

static ssize_t ufshid_sysfs_show_trigger_interval(struct ufshid_dev *hid,
						  char *buf)
{
	INFO_MSG("hid_trigger_interval %u", hid->hid_trigger_delay);

	return snprintf(buf, PAGE_SIZE, "%u\n", hid->hid_trigger_delay);
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
	INFO_MSG("hid_trigger_interval %u", hid->hid_trigger_delay);

	return count;
}

static ssize_t ufshid_sysfs_show_on_idle_delay(struct ufshid_dev *hid,
					       char *buf)
{
	INFO_MSG("hid_on_idle_delay %u", hid->hid_on_idle_delay);

	return snprintf(buf, PAGE_SIZE, "%u\n", hid->hid_on_idle_delay);
}

static ssize_t ufshid_sysfs_store_on_idle_delay(struct ufshid_dev *hid,
						const char *buf,
						size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val < HID_ON_IDLE_DELAY_MS_MIN || val > HID_ON_IDLE_DELAY_MS_MAX) {
		INFO_MSG("hid_on_idle_delay (min) %4dms ~ (max) %4dms",
			 HID_ON_IDLE_DELAY_MS_MIN,
			 HID_ON_IDLE_DELAY_MS_MAX);
		return -EINVAL;
	}

	hid->hid_on_idle_delay = val;
	INFO_MSG("hid_on_idle_delay %u", hid->hid_on_idle_delay);

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
	unsigned int val;

	if (kstrtouint(buf, 0, &val))
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
	int ret;

	ret = ufshid_execute_query_op(hid, HID_OP_ANALYZE, &attr_val);
	if (ret)
		return ret;

	frag_level = HID_GET_FRAG_LEVEL(attr_val);

	return snprintf(buf, PAGE_SIZE, "%s\n",
			frag_level == HID_LEV_RED ? "RED" :
			frag_level == HID_LEV_YELLOW ? "YELLOW" :
			frag_level == HID_LEV_GREEN ? "GREEN" :
			frag_level == HID_LEV_GRAY ? "GRAY" : "UNKNOWN");
}

#if defined(CONFIG_UFSHID_POC)
static ssize_t ufshid_sysfs_show_debug_op(struct ufshid_dev *hid, char *buf)
{
	u32 attr_val;

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, &attr_val))
		return -EINVAL;

	INFO_MSG("hid_op %u", attr_val);

	return snprintf(buf, PAGE_SIZE, "%u\n", attr_val);
}

static ssize_t ufshid_sysfs_store_debug_op(struct ufshid_dev *hid,
					   const char *buf, size_t count)
{
	unsigned int val;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val >= HID_OP_MAX)
		return -EINVAL;

	if (hid->hid_trigger) {
		ERR_MSG("debug_op cannot change, current hid_trigger is ON");
		return -EINVAL;
	}

	if (ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, val))
		return -EINVAL;

	INFO_MSG("hid_op %u is set!", val);
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
	unsigned int val;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	INFO_MSG("HID_block_suspend %u", val);

	if (val == hid->block_suspend)
		return count;

	if (val) {
		ufshid_block_enter_suspend(hid);
		hid->block_suspend = true;
	} else {
		ufshid_allow_enter_suspend(hid);
		hid->block_suspend = false;
	}

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
	define_sysfs_ro(version),
	define_sysfs_ro(color),

	define_sysfs_rw(trigger),
	define_sysfs_rw(trigger_interval),
	define_sysfs_rw(on_idle_delay),

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
