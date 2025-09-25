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

const struct ufshid_offset ufshid_idn[] = {
	/* UFS 4.0 / 3.1 Use Separate HID IDN */
	/* Attribute */
	[QUERY_ATTR_IDN_HID_OPERATION] = { 0x80 },
	[QUERY_ATTR_IDN_HID_OPERATION_3_1] = { 0x20 },
	[QUERY_ATTR_IDN_HID_FRAG_LEVEL] = { 0x81 },
	[QUERY_ATTR_IDN_HID_FRAG_LEVEL_3_1] = { 0x21 },
#if defined(CONFIG_MICRON_UFSHID)
	[QUERY_ATTR_IDN_HID_FRAG_STATUS] = { 0x81 },
	[QUERY_ATTR_IDN_HID_PROGRESS] = { 0x82 },
#endif
	[HID_SEPARATION_BOUNDARY] = { 0x00 },
	/* UFS 4.0 / 3.1 Use Common HID IDN */
	/* Attribute */
	[QUERY_ATTR_IDN_HID_SIZE] = { 0x8A },
	[QUERY_ATTR_IDN_HID_AVAIL_SIZE] = { 0x8B },
	[QUERY_ATTR_IDN_HID_PROGRESS_RATIO] = { 0x8C },
	[QUERY_ATTR_IDN_HID_STATE] = { 0x8D },
	[QUERY_ATTR_IDN_HID_L2P_FRAG_LEVEL] = { 0x8E },
	[QUERY_ATTR_IDN_HID_L2P_DEFRAG_THRESHOLD] = { 0x8F },
	[QUERY_ATTR_IDN_HID_FEAT_SUP] = { 0x90 },
};

const struct ufshid_offset ufshid_desc[] = {
	/* UFS 4.0 / 3.1 Use Separate HID Dev/Geometry Desc */
	/* Device */
	[DEVICE_DESC_PARAM_HID_VER] = { 0xF7 },
	[DEVICE_DESC_PARAM_HID_VER_3_1] = { 0x59 },
	/* Geometry */
	[GEOMETRY_DESC_HID_MAX_LBA_RANGE_CNT] = { 0xF8 },
	[GEOMETRY_DESC_HID_MAX_LBA_RANGE_CNT_3_1] = { 0x5D },
	[GEOMETRY_DESC_HID_MAX_LBA_RANGE_SIZE] = { 0xF9 },
	[GEOMETRY_DESC_HID_MAX_LBA_RANGE_SIZE_3_1] = { 0x59 },
};

/* HID Spec check Version */
static inline u32 ufshid_spec_chk(struct ufshid_dev *hid)
{
	return hid->hid_ver & UFSHID_SPEC_VER_MASK;
}

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

static inline u8 ufshid_get_idn(struct ufshid_dev *hid,
				enum ufshid_idn_indx name)
{
	struct ufsf_feature *ufsf = hid->ufsf;

	return name < HID_SEPARATION_BOUNDARY && ufsf->samsung_sel ?
			ufshid_idn[name + ufsf->samsung_sel].offset :
			ufshid_idn[name].offset;
}

static inline u8 ufshid_get_desc(struct ufshid_dev *hid,
				 enum ufshid_desc_indx name)
{
	struct ufsf_feature *ufsf = hid->ufsf;

	return ufsf->samsung_sel ? ufshid_desc[name + ufsf->samsung_sel].offset :
				ufshid_desc[name].offset;
}

static int ufshid_read_attr(struct ufshid_dev *hid, enum ufshid_idn_indx name,
			    u32 *attr_val)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	struct ufsf_feature *ufsf = hid->ufsf;
	int ret = 0;
	u8 idn;

	ufshcd_rpm_get_sync(hba);

	idn = ufshid_get_idn(hid, name);
	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_READ_ATTR, idn, 0,
				      ufsf->samsung_sel, attr_val);
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

static int ufshid_write_attr(struct ufshid_dev *hid, enum ufshid_idn_indx name,
			     u32 val)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	struct ufsf_feature *ufsf = hid->ufsf;
	u8 idn;
	int ret = 0;

	ufshcd_rpm_get_sync(hba);

	idn = ufshid_get_idn(hid, name);
	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, idn, 0,
				      ufsf->samsung_sel, &val);
	if (ret) {
		ERR_MSG("write attr [0x%.2X] fail. (%d)", idn, ret);
		goto err_out;
	}

	HID_DEBUG(hid, "hid_attr write [0x%.2X] %u (0x%X)", idn, val, val);

err_out:
	ufsf_rpm_put_noidle(hba);

	return ret;
}

#if defined(CONFIG_MICRON_UFSHID)
static int ufshid_set_flag(struct ufshid_dev *hid, u8 idn)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	int ret = 0;
	bool flag_result;
	ufshcd_rpm_get_sync(hba);

	ret = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_SET_FLAG, idn, 0,&flag_result);

	if (ret) {
		ERR_MSG("set flag [0x%.2X] fail. (%d)", idn, ret);
		goto err_out;
	}

	HID_DEBUG(hid, "hid_flag set [0x%.2X] result [0x%.2X] ", idn, flag_result);
err_out:
	pm_runtime_mark_last_busy(hba->dev);
	ufsf_rpm_put_noidle(hba);

	return ret;
}

static int ufshid_clear_flag(struct ufshid_dev *hid, u8 idn)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	int ret = 0;

	ufshcd_rpm_get_sync(hba);

	ret = ufshcd_query_flag_retry(hba, UPIU_QUERY_OPCODE_CLEAR_FLAG, idn, 0,NULL);
	if (ret) {
		ERR_MSG("clear flag [0x%.2X] fail. (%d)", idn, ret);
		goto err_out;
	}

	HID_DEBUG(hid, "hid_flag set [0x%.2X] ", idn);
err_out:
	pm_runtime_mark_last_busy(hba->dev);
	ufsf_rpm_put_noidle(hba);

	return ret;
}
#endif

static inline void ufshid_version_print(int spec_version)
{
	INFO_MSG("Support HID Spec : Driver = (%.4x), Device = (%.4x)",
		 UFSHID_VER, spec_version);
	INFO_MSG("HID Driver version (%.6X%s)",
		 UFSHID_DD_VER, UFSHID_DD_VER_POST);
}

void ufshid_get_geo_info(struct ufsf_feature *ufsf, u8 *geo_buf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;
	u8 max_size_offset, max_cnt_offset;

	if (ufshid_spec_chk(hid) == HID_1_0_VER)
		return;

	max_size_offset = ufshid_get_desc(hid,
			GEOMETRY_DESC_HID_MAX_LBA_RANGE_SIZE);
	max_cnt_offset = ufshid_get_desc(hid,
			GEOMETRY_DESC_HID_MAX_LBA_RANGE_CNT);

	hid->max_lba_range_size = get_unaligned_be32(geo_buf + max_size_offset);
	hid->max_lba_range_cnt = geo_buf[max_cnt_offset];

	INFO_MSG("[0x%.2x] dMaxHIDLBARangeSize (%u)", max_size_offset,
			hid->max_lba_range_size);
	INFO_MSG("[0x%.2x] bMaxHIDLBARangeCount (%u)", max_cnt_offset,
			hid->max_lba_range_cnt);
}

void ufshid_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf)
{
	u8 ver_offset;

	ufsf->hid_dev = NULL;
	struct ufs_hba *hba = ufsf->hba;

	if (hba->dev_info.wmanufacturerid == UFS_VENDOR_SAMSUNG) {
		if (!ufsf->samsung_sel) {
			if (!(get_unaligned_be32(desc_buf +	DEVICE_DESC_PARAM_SAMSUNG_SUP) &
					UFS_FEATURE_SUPPORT_HID_BIT_4_0)) {
				INFO_MSG("bUFSExFeaturesSupport: HID not support");
				goto err_out;
			}
		} else {
			if (!(get_unaligned_be32(desc_buf +
					DEVICE_DESC_PARAM_EX_FEAT_SUP) &
					UFS_FEATURE_SUPPORT_HID_BIT_3_1)) {
				INFO_MSG("bUFSExFeaturesSupport: HID not support");
				goto err_out;
			}
		}
		INFO_MSG("bUFSExFeaturesSupport: HID support");
	}
#ifndef CONFIG_MICRON_UFSHID
	else if (hba->dev_info.wmanufacturerid == UFS_VENDOR_MICRON) {
		INFO_MSG(" ufshid support Micron HID");
	} else {
		INFO_MSG("ufshid can not support this ufs !!!");
		goto err_out;
	}
#endif

	ufsf->hid_dev = kzalloc(sizeof(struct ufshid_dev), GFP_KERNEL);
	if (!ufsf->hid_dev) {
		ERR_MSG("hid_dev memalloc fail");
		goto err_out;
	}

	ufsf->hid_dev->ufsf = ufsf;

	ver_offset = ufshid_get_desc(ufsf->hid_dev, DEVICE_DESC_PARAM_HID_VER);
	ufsf->hid_dev->hid_ver = get_unaligned_be16(desc_buf + ver_offset);
	ufshid_version_print(ufsf->hid_dev->hid_ver);

	return;

err_out:
	ufshid_set_state(ufsf, HID_FAILED);
}

static inline void ufshid_set_wb_cmd(unsigned char *cdb, size_t len)
{
	cdb[0] = WRITE_BUFFER;
	cdb[1] = HID_L2P_COMMAND_MODE;
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

static int ufshid_execute_query_op_and_frag_chk(struct ufshid_dev *hid,
		enum UFSHID_OP op, u32 *attr_val)
{
	if (ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, op))
		return -EINVAL;

	msleep(200);

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_FRAG_LEVEL, attr_val))
		return -EINVAL;

	HID_DEBUG(hid, "Frag_lv %d Frag_mode %d Frag_stat %d HID_need_exec %d",
		  HID_GET_FRAG_LEVEL(*attr_val),
		  HID_FRAG_UPDATE_MODE(*attr_val),
		  HID_FRAG_UPDATE_STAT(*attr_val),
		  HID_EXECUTE_REQ_STAT(*attr_val));

	return 0;
}

static int ufshid_execute_query_hid_op(struct ufshid_dev *hid, enum UFSHID_OP op,
				       u32 *attr_val)
{
	if (op != HID_OP_DISABLE) {
		op = hid->lba_trigger_mode ?
			HID_OP_LBA_EXECUTE : HID_OP_EXECUTE;
	}

	return ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, op);

}

static int ufshid_execute_query_op(struct ufshid_dev *hid, enum UFSHID_OP op,
				   u32 *attr_val)
{
	int ret = 0;

	if (hid->ufshid_temp->issue_lba_list) {
		ret = hid->ufshid_temp->issue_lba_list(hid);
		if (ret)
			return ret;
	}

	ret = hid->ufshid_temp->excute_query_op(hid, op, attr_val);

	return ret;
}

static inline void ufshid_clear_lba_param(struct ufshid_dev *hid)
{
	hid->lba_trigger_mode = false;
	memset(&hid->hid_req, 0, sizeof(struct ufshid_req));
}

static inline void ufshid_set_lba_param(struct ufshid_dev *hid,
					       int lun, unsigned char *buf,
					       __u16 size)
{
	struct ufshid_req *hid_req = &hid->hid_req;

	hid_req->lun = lun;
	memcpy(hid_req->buf, buf, size);
	hid_req->buf_size = size;
	hid->is_need_param = true;
}

static int ufshid_get_param_mode(struct ufshid_dev *hid, u32 attr_val)
{
	bool param_mode;

	param_mode = HID_FRAG_UPDATE_MODE(attr_val);

	hid->is_need_param =
		hid->lba_trigger_mode && param_mode == HID_NO_PARAM;

	if (hid->is_need_param)
		return -EAGAIN;

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

	if (hid->lba_trigger_mode)
		if(ufshid_get_param_mode(hid, attr_val))
			return -EAGAIN;

	return (HID_EXECUTE_REQ_STAT(attr_val)) ?
		HID_REQUIRED : HID_NOT_REQUIRED;
}

#if defined(CONFIG_MICRON_UFSHID)
static int ufshid_get_analyze_and_issue_execute_for_micro(struct ufshid_dev *hid)
{
	u32 attr_val;
	int frag_level;
	//get micron ufs frag level
	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_FRAG_STATUS, &frag_level))
		return -EINVAL;
	//get micron ufs hid execution progress
	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_PROGRESS, &attr_val))
		return -EINVAL;
	HID_DEBUG(hid, "micron frag_level= %d attr_val= %d",frag_level,attr_val);

	if (attr_val != HID_PROG_ONGOING) {
		if(frag_level!= HID_LEV_GREEN_MICRON) {
			ufshid_set_flag(hid, QUERY_FLAG_IDN_HID_EN);
			return HID_REQUIRED;
		} else {
			return HID_NOT_REQUIRED;
		}
	} else {
		return HID_REQUIRED;
	}
}
#endif

static inline void ufshid_issue_disable(struct ufshid_dev *hid)
{
	u32 attr_val;
	struct ufs_hba *hba = hid->ufsf->hba;

	if(hba->dev_info.wmanufacturerid == UFS_VENDOR_MICRON) {
#if defined(CONFIG_MICRON_UFSHID)
		//get micron ufs hid execution progress
		if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_PROGRESS, &attr_val))
			return;
		HID_DEBUG(hid, "micron hid progress = %d",attr_val);

		if(attr_val == HID_PROG_ONGOING) {
			if (ufshid_clear_flag(hid, QUERY_FLAG_IDN_HID_EN))
				return;
		}
#endif
	}else if(hba->dev_info.wmanufacturerid == UFS_VENDOR_SAMSUNG) {
		ufshid_execute_query_op(hid, HID_OP_DISABLE, &attr_val);
	}
	return;
}

/* HID 3.0 */
static int ufshid_analyze_and_get_attr(struct ufshid_dev *hid,
				       enum ufshid_idn_indx name, u32 *attr_val)
{
	int ret;
	u32 hid_state;

	ret = ufshid_issue_lba_list(hid);
	if (ret)
		return ret;

	if (ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_OPERATION, HID_OP_ANALYZE))
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

	return ufshid_read_attr(hid, name, attr_val);
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
	if (!ufshcd_is_auto_hibern8_supported(hba)) {
		/* If the ahit value is not 0,
		 * the hibern8 is being controlled.
		 * so UFSHCD AutoHibern Register control is necessary.
		 */
		if (hid->ahit == 0)
			return;
	}

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

#ifdef CONFIG_UFS_SHID_POC
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

#ifdef CONFIG_UFS_SHID_POC
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
	ufshid_clear_lba_param(hid);

	ufshid_issue_disable(hid);

	ufshid_auto_hibern8_enable(hid, 1);

	ufshid_allow_enter_suspend(hid);

	ufshid_release_runtime_pm(hid);

	return 0;
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

	if (ufshid_spec_chk(hid) >= HID_3_0_VER) {
		ret = ufshid_execute_query_op(hid, HID_OP_NONE, NULL);
		if (ret) {
			ufshid_release_runtime_pm(hid);
			goto err_out;
		}
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

static int ufshid_check_file_info_buf_2_0(struct ufshid_dev *hid,
				      unsigned char *buf, __u16 size)
{
	struct ufshid_blk_desc_header *desc_header;
	struct ufshid_blk_desc_2_0 *desc, *comp_desc;
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
	desc_size = sizeof(struct ufshid_blk_desc_2_0);

	p += header_size;
	for (desc_cnt = 0; desc_cnt < total_desc; desc_cnt++, p += desc_size) {
		const char *comp_p;

		desc = (struct ufshid_blk_desc_2_0 *)p;
		lba = get_unaligned_be64(&desc->lba);
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
			comp_desc = (struct ufshid_blk_desc_2_0 *)comp_p;
			comp_lba = get_unaligned_be64(&comp_desc->lba);
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


static int ufshid_check_file_info_buf_3_0(struct ufshid_dev *hid,
				      unsigned char *buf, __u16 size)
{
	struct ufshid_blk_desc_header *desc_header;
	struct ufshid_blk_desc_3_0 *desc, *comp_desc;
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
	desc_size = sizeof(struct ufshid_blk_desc_3_0);

	p += header_size;
	for (desc_cnt = 0; desc_cnt < total_desc; desc_cnt++, p += desc_size) {
		const char *comp_p;

		desc = (struct ufshid_blk_desc_3_0 *)p;
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
			comp_desc = (struct ufshid_blk_desc_3_0 *)comp_p;
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

/*
 * This is for saving the L2P param.
 * If the parameter is not explicitly cleared by the user,
 * the driver saves and uses it.
 *
 * In the device, the parameter is deleted in the following cases.
 *   - bDefragOperation is set to 0 by the host.
 *   - After the HID analysis(when the analysis result is defrag not required)
 *     or the HID execution is completed.
 *   - The device receives a Query Request(WRITE) unrelated to the HID.
 */
void ufshid_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	struct ufshid_dev *hid = ufsf->hid_dev;
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct request *rq = scsi_cmd_to_rq(cmd);
	struct bio *bio = rq->bio;
	struct page *page;
	int len;

	if (cmd->cmnd[0] != WRITE_BUFFER ||
	    cmd->cmnd[1] != HID_L2P_COMMAND_MODE ||
	    !hid->l2p_defrag_sup)
		return;

	page = bio->bi_io_vec->bv_page;
	len = bio->bi_io_vec->bv_len;

	/*
	 * If it is the same as the file information currently being processed,
	 * keep the device's existing write buffer as it is.
	 */
	if (hid->hid_req.buf_size == len &&
		!memcmp(hid->hid_req.buf, page_address(page), len)) {
		INFO_MSG("L2P parameter is same as before.");
		return;
	}

	if (hid->ufshid_temp->check_file_info_buf(hid,
						  page_address(page), len)) {
		ERR_MSG("L2P parameter has a problem. so skip it.");
		return;
	}

	ufshid_set_lba_param(hid, lrbp->lun, page_address(page), len);

	return;
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

static bool ufshid_hid_progress_chk_in_work(struct ufshid_dev *hid)
{
	u32 attr_val;
	int ret;

	if (ufshid_is_in_progress(hid)) {
		HID_DEBUG(hid, "HID is in progress, so re-sched (%d ms)",
				hid->hid_trigger_delay);
		return true;
	};

	ret = ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_PROGRESS_RATIO,
			       &attr_val);
	if (!ret && !ufshid_check_progress_end(attr_val)) {
		HID_DEBUG(hid, "HID is on-going(%d), so re-sched (%d ms)",
			  ret, hid->hid_trigger_delay);
		return true;
	}

	HID_DEBUG(hid, "HID is ended or err (%d), so trigger off", ret);

	ufshid_issue_disable(hid);
	msleep(200);

	ret = ufshid_analyze_and_get_attr(hid, QUERY_ATTR_IDN_HID_FRAG_LEVEL, &attr_val);
	if (!ret)
		HID_DEBUG(hid, "Frag_lv %d Frag_mode %d Frag_stat %d HID_need_exec %d",
			  attr_val & HID_FRAG_LEVEL_MASK,
			  HID_FRAG_UPDATE_MODE(attr_val),
			  HID_FRAG_UPDATE_STAT(attr_val),
			  HID_EXECUTE_REQ_STAT(attr_val));

	ret = ufshid_trigger_off(hid);

	if (ret)
		WARN_MSG("trigger off fail.. must check it");

	return 0;
}

static bool ufshid_get_analyze_and_in_work(struct ufshid_dev *hid)
{
	int ret;

	ret = ufshid_get_analyze_and_issue_execute(hid);

	if (ret == HID_REQUIRED || ret == -EAGAIN) {
		HID_DEBUG(hid, "REQUIRED or AGAIN (%d), so re-sched (%d ms)",
			  ret, hid->hid_trigger_delay);
		return true;
	}

	HID_DEBUG(hid, "NOT_REQUIRED or err (%d), so trigger off", ret);

	ret = ufshid_trigger_off(hid);
	if (ret)
		WARN_MSG("trigger off fail.. must check it");

	return 0;
}

static void ufshid_trigger_work_fn(struct work_struct *dwork)
{
	struct ufshid_dev *hid;

	hid = container_of(dwork, struct ufshid_dev, hid_trigger_work.work);

	if (ufshid_is_not_present(hid))
		return;

	HID_DEBUG(hid, "start hid_trigger_work_fn");

	mutex_lock(&hid->sysfs_lock);

	if (!hid->hid_trigger) {
		HID_DEBUG(hid, "hid_trigger == false, return");
		goto finish_work;
	}

	if (hid->ufshid_temp->trigger_work_action(hid))
		goto resched;

finish_work:
	mutex_unlock(&hid->sysfs_lock);

	return;

resched:
	mutex_unlock(&hid->sysfs_lock);

	ufshid_schedule_delayed_work(&hid->hid_trigger_work,
				     msecs_to_jiffies(hid->hid_trigger_delay));

	HID_DEBUG(hid, "end hid_trigger_work_fn");
}

#if defined(CONFIG_MICRON_UFSHID)
static void ufshid_trigger_work_fn_for_micro(struct work_struct *dwork)
{
	struct ufshid_dev *hid;
	int ret;

	hid = container_of(dwork, struct ufshid_dev, hid_trigger_work.work);

	if (ufshid_is_not_present(hid))
		return;

	HID_DEBUG(hid, "start hid_trigger_work_fn");

	ret = ufshid_get_analyze_and_issue_execute_for_micro(hid);

	mutex_lock(&hid->sysfs_lock);
	if (!hid->hid_trigger) {
		HID_DEBUG(hid, "hid_trigger == false, return");
		goto finish_work;
	}

	if (ret == HID_NOT_REQUIRED) {
		ret = ufshid_trigger_off(hid);
		if (likely(!ret))
			goto finish_work;

		WARN_MSG("trigger off fail.. must check it");

	} else if (ret == HID_REQUIRED) {
		HID_DEBUG(hid, "HID_REQUIRED, so sched (%d ms)",
			  hid->hid_trigger_delay);

	} else {
		HID_DEBUG(hid, "issue_HID ERR(%X), so resched for retry", ret);
	}
	mutex_unlock(&hid->sysfs_lock);

	ufshid_schedule_delayed_work(&hid->hid_trigger_work,
			      msecs_to_jiffies(hid->hid_trigger_delay));

	HID_DEBUG(hid, "end hid_trigger_work_fn");
	return;
finish_work:
	mutex_unlock(&hid->sysfs_lock);
}
#else
static void ufshid_trigger_work_fn_for_micro(struct work_struct *dwork)
{
	return;
}
#endif

static void ufshid_init_attr(struct ufshid_dev *hid)
{
	u32 attr_val;

	ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_FEAT_SUP, &attr_val);
	hid->l2p_defrag_sup = attr_val & HID_L2P_DEFRAG_SUP_MASK;

	if (hid->l2p_defrag_sup) {
		ufshid_clear_lba_param(hid);
		hid->l2p_defrag_threshold = HID_L2P_DEFRAG_THRESHOLD_DEFAULT;
		ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_L2P_DEFRAG_THRESHOLD,
				  hid->l2p_defrag_threshold);
	}

	hid->hid_size = HID_SIZE_DEFAULT;
	ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_SIZE, hid->hid_size);
}

/* Since it is a volatile attribute, write it again */
static void ufshid_restore_attr(struct ufshid_dev *hid)
{
	struct ufs_hba *hba = hid->ufsf->hba;
	u32 attr_val = (u32)hid->l2p_defrag_threshold;
	u8 idn = ufshid_get_idn(hid, QUERY_ATTR_IDN_HID_L2P_DEFRAG_THRESHOLD);
	int ret;

	pm_runtime_get(&hba->ufs_device_wlun->sdev_gendev);
	ret = ufshcd_query_attr_retry(hba, UPIU_QUERY_OPCODE_WRITE_ATTR, idn, 0,
				      0, &attr_val);
	ufsf_rpm_put_noidle(hba);

	if (!ret)
		HID_DEBUG(hid, "Restore attribute [0x%.2x] %u", idn, attr_val);
}

#define SPM_ACTIVE_POWER_LEVEL			1
void ufshid_suspend_3_0(struct ufsf_feature *ufsf, bool is_system_pm)
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

void ufshid_resume_3_0(struct ufsf_feature *ufsf, bool is_link_off)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (!hid)
		return;

	ufshid_set_state(ufsf, HID_PRESENT);

	complete(&hid->resume_compl);

	if (is_link_off && hid->l2p_defrag_sup)
		ufshid_restore_attr(hid);

	if (hid->hid_trigger)
		ufshid_schedule_delayed_work(&hid->hid_trigger_work,
				msecs_to_jiffies(hid->hid_trigger_delay));
}

/* HID 1.0,  HID 2.0 */
void ufshid_suspend_1_0(struct ufsf_feature *ufsf, bool is_system_pm)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (!hid)
		return;

	if (unlikely(hid->hid_trigger))
		ERR_MSG("hid_trigger was set to block the suspend. so weird");
	ufshid_set_state(ufsf, HID_SUSPEND);

	cancel_delayed_work_sync(&hid->hid_trigger_work);
}

/* HID 1.0, HID 2.0 */
void ufshid_resume_1_0(struct ufsf_feature *ufsf, bool is_link_off)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (!hid)
		return;

	if (unlikely(hid->hid_trigger))
		ERR_MSG("hid_trigger need to off");
	ufshid_set_state(ufsf, HID_PRESENT);
}

static struct ufshid_template ufshid_1_0 = {
	.check_file_info_buf = NULL,
	.suspend = ufshid_suspend_1_0,
	.resume = ufshid_resume_1_0,
	.issue_lba_list = NULL,
	.excute_query_op = ufshid_execute_query_op_and_frag_chk,
	.trigger_work_action = ufshid_get_analyze_and_in_work,
};

static struct ufshid_template ufshid_2_0 = {
	.check_file_info_buf = ufshid_check_file_info_buf_2_0,
	.suspend = ufshid_suspend_1_0,
	.resume = ufshid_resume_1_0,
	.issue_lba_list = ufshid_issue_lba_list,
	.excute_query_op = ufshid_execute_query_op_and_frag_chk,
	.trigger_work_action = ufshid_get_analyze_and_in_work,
};

static struct ufshid_template ufshid_3_0 = {
	.check_file_info_buf = ufshid_check_file_info_buf_3_0,
	.suspend = ufshid_suspend_3_0,
	.resume = ufshid_resume_3_0,
	.issue_lba_list = ufshid_issue_lba_list,
	.excute_query_op = ufshid_execute_query_hid_op,
	.trigger_work_action = ufshid_hid_progress_chk_in_work,
};

void ufshid_init(struct ufsf_feature *ufsf)
{
	struct ufshid_dev *hid = ufsf->hid_dev;
	int ret;
	u32 spec_ver;
	struct ufs_hba *hba = ufsf->hba;

	INFO_MSG("HID_INIT_START");

	if (!hid) {
		ERR_MSG("hid is not found. it is very weired. must check it");
		ufshid_set_state(ufsf, HID_FAILED);
		return;
	}

	hid->hid_trigger = false;
	hid->hid_trigger_delay = HID_TRIGGER_WORKER_DELAY_MS_DEFAULT;
	spec_ver = ufshid_spec_chk(hid);

	hid->hid_on_idle_delay = HID_ON_IDLE_DELAY_MS_DEFAULT;

	if (spec_ver == HID_1_0_VER) {
		hid->ufshid_temp = &ufshid_1_0;
	} else if (spec_ver == HID_2_0_VER) {
		hid->ufshid_temp = &ufshid_2_0;
		hid->l2p_defrag_sup = true;
		ufshid_clear_lba_param(hid);
	} else {
		hid->ufshid_temp = &ufshid_3_0;
		if (hba->dev_info.wmanufacturerid == UFS_VENDOR_SAMSUNG) {
			ufshid_init_attr(hid);
		}
	}

	if (hba->dev_info.wmanufacturerid == UFS_VENDOR_SAMSUNG) {
		INIT_DELAYED_WORK(&hid->hid_trigger_work, ufshid_trigger_work_fn);
	} else if (hba->dev_info.wmanufacturerid == UFS_VENDOR_MICRON) {
		INIT_DELAYED_WORK(&hid->hid_trigger_work, ufshid_trigger_work_fn_for_micro);
	}

	hid->hid_debug = false;
#ifdef CONFIG_UFS_SHID_POC
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
	struct ufs_hba *hba = ufsf->hba;

	if (!hid)
		return;

	ufshid_set_state(ufsf, HID_PRESENT);

	/*
	 * hid_trigger will be checked under sysfs_lock in worker.
	 */
	if (hid->hid_trigger)
		ufshid_schedule_delayed_work(&hid->hid_trigger_work, 0);

	if (hba->dev_info.wmanufacturerid == UFS_VENDOR_SAMSUNG) {
		if (hid->l2p_defrag_sup)
			/* Since it is a volatile attribute, write it again */
			ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_L2P_DEFRAG_THRESHOLD,
					hid->l2p_defrag_threshold);
	}

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

void ufshid_suspend(struct ufsf_feature *ufsf, bool is_system_pm)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (hid->ufshid_temp->suspend)
		hid->ufshid_temp->suspend(ufsf, is_system_pm);
}

void ufshid_resume(struct ufsf_feature *ufsf, bool is_link_off)
{
	struct ufshid_dev *hid = ufsf->hid_dev;

	if (hid->ufshid_temp->resume)
		hid->ufshid_temp->resume(ufsf, is_link_off);
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

	/* HID 3.0 not working */
	if (ufshid_spec_chk(hid) == HID_3_0_VER)
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
		 hid->hid_ver, UFSHID_DD_VER, UFSHID_DD_VER_POST);

	return snprintf(buf, PAGE_SIZE,
			"HID version (%.4X) D/D version (%.6X%s)\n",
			hid->hid_ver, UFSHID_DD_VER, UFSHID_DD_VER_POST);
}

static ssize_t ufshid_sysfs_show_trigger(struct ufshid_dev *hid, char *buf)
{
	INFO_MSG("hid_trigger %d", hid->hid_trigger);

	return snprintf(buf, PAGE_SIZE, "%d\n", hid->hid_trigger);
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

static ssize_t ufshid_sysfs_show_on_idle_delay(struct ufshid_dev *hid,
					       char *buf)
{
	INFO_MSG("hid_on_idle_delay %d", hid->hid_on_idle_delay);

	return snprintf(buf, PAGE_SIZE, "%d\n", hid->hid_on_idle_delay);
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
	INFO_MSG("hid_on_idle_delay %d", hid->hid_on_idle_delay);

	return count;
}

static ssize_t ufshid_sysfs_show_hid_size(struct ufshid_dev *hid, char *buf)
{
	struct ufs_hba *hba = hid->ufsf->hba;

	if (hba->dev_info.wmanufacturerid != UFS_VENDOR_SAMSUNG) {
		return -EINVAL;
	}

	if (ufshid_spec_chk(hid) < HID_3_0_VER) {
		INFO_MSG("do not support hid version %.4x", hid->hid_ver);
		return snprintf(buf, PAGE_SIZE, "hid ver %.4x\n", hid->hid_ver);
	}

	INFO_MSG("hid_size %llu KB", (u64)hid->hid_size * KB_PER_HID_SIZE_UNIT);

	return snprintf(buf, PAGE_SIZE, "%llu KB\n",
			(u64)hid->hid_size * KB_PER_HID_SIZE_UNIT);
}

static ssize_t ufshid_sysfs_store_hid_size(struct ufshid_dev *hid,
					   const char *buf, size_t count)
{
	u32 val;
	int ret;
	struct ufs_hba *hba = hid->ufsf->hba;

	if (hba->dev_info.wmanufacturerid != UFS_VENDOR_SAMSUNG) {
		return -EINVAL;
	}

	if (kstrtou32(buf, 0, &val))
		return -EINVAL;

	if (ufshid_spec_chk(hid) < HID_3_0_VER) {
		INFO_MSG("do not support hid version %.4x", hid->hid_ver);
		return -EINVAL;
	}

	ret = ufshid_write_attr(hid, QUERY_ATTR_IDN_HID_SIZE, val);
	if (ret)
		return -EINVAL;

	hid->hid_size = val;

	INFO_MSG("hid_size %llu KB", (u64)hid->hid_size * KB_PER_HID_SIZE_UNIT);

	return count;
}

static ssize_t ufshid_sysfs_show_lba_trigger_mode(struct ufshid_dev *hid,
						  char *buf)
{
	struct ufs_hba *hba = hid->ufsf->hba;

	if (hba->dev_info.wmanufacturerid != UFS_VENDOR_SAMSUNG) {
		return -EINVAL;
	}

	INFO_MSG("lba_trigger_mode %d", hid->lba_trigger_mode);

	return snprintf(buf, PAGE_SIZE, "%d\n", hid->lba_trigger_mode);
}

static ssize_t ufshid_sysfs_store_lba_trigger_mode(struct ufshid_dev *hid,
						   const char *buf,
						   size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val != 0 && val != 1)
		return -EINVAL;

	if (hid->hid_trigger) {
		INFO_MSG("HID is in progress...");
		return -EBUSY;
	}

	if (!hid->l2p_defrag_sup) {
		INFO_MSG("l2p defrag is not supported");
		return -EPERM;
	}

	if (val)
		hid->lba_trigger_mode = true;
	else
		ufshid_clear_lba_param(hid);

	return count;
}

static ssize_t ufshid_sysfs_show_l2p_frag_lvl(struct ufshid_dev *hid, char *buf)
{
	u32 attr_val;
	int ret;

	if (ufshid_spec_chk(hid) < HID_3_0_VER) {
		INFO_MSG("do not support hid version %.4x", hid->hid_ver);
		return snprintf(buf, PAGE_SIZE, "hid ver %.4x\n", hid->hid_ver);
	}

	if (!hid->lba_trigger_mode) {
		INFO_MSG("lba_trigger_mode is disabled");
		return snprintf(buf, PAGE_SIZE, "L2P DISABLED\n");
	}

	if (!hid->hid_req.buf_size) {
		INFO_MSG("lba param is empty");
		return snprintf(buf, PAGE_SIZE, "PARAM EMPTY\n");
	}

	if (hid->hid_trigger) {
		INFO_MSG("HID is in progress...");
		return snprintf(buf, PAGE_SIZE, "NOT PERMITTED\n");
	}

	ret = ufshid_analyze_and_get_attr(hid,
					  QUERY_ATTR_IDN_HID_L2P_FRAG_LEVEL,
					  &attr_val);
	if (ret) {
		if (attr_val == RESULT_NOT_DEFRAG_REQUIRED) {
			INFO_MSG("Defrag is not required...");
			return snprintf(buf, PAGE_SIZE, "UNKNOWN: Defrag is not required\n");
		}

		return ret;
	}

	/* Initialize HID state */
	ufshid_issue_disable(hid);

	INFO_MSG("L2P Fragment Level is %u", attr_val);

	return snprintf(buf, PAGE_SIZE, "%u\n", attr_val);
}

static ssize_t ufshid_sysfs_show_l2p_defrag_threshold(struct ufshid_dev *hid,
						      char *buf)
{
	if (ufshid_spec_chk(hid) < HID_3_0_VER) {
		INFO_MSG("do not support hid version %.4x", hid->hid_ver);
		return snprintf(buf, PAGE_SIZE, "hid ver %.4x\n", hid->hid_ver);
	}

	INFO_MSG("l2p_defrag_threshold %u", hid->l2p_defrag_threshold);
	return snprintf(buf, PAGE_SIZE, "%u\n", hid->l2p_defrag_threshold);
}

static ssize_t ufshid_sysfs_store_l2p_defrag_threshold(struct ufshid_dev *hid,
						       const char *buf,
						       size_t count)
{
	int ret;
	u8 val;
	struct ufs_hba *hba = hid->ufsf->hba;

	if (hba->dev_info.wmanufacturerid != UFS_VENDOR_SAMSUNG) {
		return -EINVAL;
	}

	if (ufshid_spec_chk(hid) < HID_3_0_VER) {
		INFO_MSG("do not support hid version %.4x", hid->hid_ver);
		return -EINVAL;
	}

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

static ssize_t ufshid_sysfs_show_l2p_param_print(struct ufshid_dev *hid,
						 char *buf)
{
	ufsf_print_query_buf(hid->hid_req.buf, hid->hid_req.buf_size);

	return snprintf(buf, PAGE_SIZE, "%s\n", hid->hid_req.buf);
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

static ssize_t ufshid_sysfs_show_color(struct ufshid_dev *hid, char *buf)
{
	u32 attr_val;
	int frag_level;
	bool param_mode;
	int ret = 0;
	u32 spec_ver;

#if defined(CONFIG_MICRON_UFSHID)
	struct ufs_hba *hba = hid->ufsf->hba;
	if(hba->dev_info.wmanufacturerid == UFS_VENDOR_MICRON){
		if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_FRAG_STATUS, &attr_val))
			return -EINVAL;
		frag_level = attr_val;

		HID_DEBUG(hid, "hid stauts:%s\n", ((frag_level == HID_LEV_GREEN_MICRON)) ? "GREEN" :
			((frag_level ==HID_LEV_RED_MICRON))?"RED":"UNKNOWN");

		/*Micron only has two levels RED & GREEN*/
		return snprintf(buf, PAGE_SIZE, "%s\n",
			((frag_level == HID_LEV_GREEN_MICRON)) ? "GREEN" :
			((frag_level ==HID_LEV_RED_MICRON))?"RED":"UNKNOWN");
	}
#endif

	if (hid->hid_trigger) {
		INFO_MSG("HID is in progress...");
		return snprintf(buf, PAGE_SIZE, "RED\n");
	}

	spec_ver = ufshid_spec_chk(hid);
	if (spec_ver >= HID_3_0_VER) {
		/*HID 3.0 */
		ret = ufshid_analyze_and_get_attr(hid,
				QUERY_ATTR_IDN_HID_FRAG_LEVEL, &attr_val);
		if (ret) {
			if (attr_val == RESULT_NOT_DEFRAG_REQUIRED) {
				INFO_MSG("Defrag is not required...");
				return snprintf(buf, PAGE_SIZE, "UNKNOWN: Defrag is not required\n");
			}
			goto err;
		}

	} else {
		ret = ufshid_execute_query_op(hid, HID_OP_ANALYZE, &attr_val);
		if (ret)
			goto err;

	}

	frag_level = attr_val & HID_FRAG_LEVEL_MASK;

	if (spec_ver == HID_1_0_VER)
		goto out;

	param_mode = HID_FRAG_UPDATE_MODE(attr_val);

	HID_DEBUG(hid, "Frag_lv %d Frag_mode %d Frag_stat %d HID_need_exec %d",
		  frag_level, param_mode, HID_FRAG_UPDATE_STAT(attr_val),
		  HID_EXECUTE_REQ_STAT(attr_val));

	if ((hid->lba_trigger_mode && param_mode == HID_NO_PARAM) ||
	    (!hid->lba_trigger_mode && param_mode == HID_WITH_PARAM))
		frag_level = HID_LEV_UNKNOWN;
out:
	return snprintf(buf, PAGE_SIZE, "%s\n",
			frag_level == HID_LEV_RED ? "RED" :
			frag_level == HID_LEV_YELLOW ? "YELLOW" :
			frag_level == HID_LEV_GREEN ? "GREEN" :
			frag_level == HID_LEV_GRAY ? "GRAY" : "UNKNOWN");

err:
	return snprintf(buf, PAGE_SIZE, "UNKOWN: Error\n");
}

static ssize_t ufshid_sysfs_show_max_lba_range_size(struct ufshid_dev *hid,
						    char *buf)
{
	if (ufshid_spec_chk(hid) < HID_2_0_VER) {
		INFO_MSG("do not support hid version %.4x", hid->hid_ver);
		return snprintf(buf, PAGE_SIZE, "hid ver %.4x\n", hid->hid_ver);
	}

	INFO_MSG("max_lba_range_size %u", hid->max_lba_range_size);
	return snprintf(buf, PAGE_SIZE, "%u\n", hid->max_lba_range_size);
}

static ssize_t ufshid_sysfs_show_max_lba_range_cnt(struct ufshid_dev *hid,
						   char *buf)
{
	if (ufshid_spec_chk(hid) < HID_2_0_VER) {
		INFO_MSG("do not support hid version %.4x", hid->hid_ver);
		return snprintf(buf, PAGE_SIZE, "hid ver %.4x\n", hid->hid_ver);
	}

	INFO_MSG("max_lba_range_cnt %u", hid->max_lba_range_cnt);
	return snprintf(buf, PAGE_SIZE, "%u\n", hid->max_lba_range_cnt);
}

static ssize_t ufshid_sysfs_show_progress_ratio(struct ufshid_dev *hid,
						char *buf)
{
	u32 attr_val = 0;
	if (ufshid_spec_chk(hid) < HID_3_0_VER) {
		INFO_MSG("do not support hid version %.4x", hid->hid_ver);
		return snprintf(buf, PAGE_SIZE, "hid ver %.4x\n", hid->hid_ver);
	}

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_PROGRESS_RATIO, &attr_val))
		return -EINVAL;

	INFO_MSG("progress_ratio %u", attr_val);
	return snprintf(buf, PAGE_SIZE, "%u\n", attr_val);
}

static ssize_t ufshid_sysfs_show_available_size(struct ufshid_dev *hid,
						char *buf)
{
	u32 attr_val = 0;
	if (ufshid_spec_chk(hid) < HID_3_0_VER) {
		INFO_MSG("do not support hid version %.4x", hid->hid_ver);
		return snprintf(buf, PAGE_SIZE, "hid ver %.4x\n", hid->hid_ver);
	}

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_AVAIL_SIZE, &attr_val))
		return -EINVAL;

	INFO_MSG("available_size %u", attr_val);
	return snprintf(buf, PAGE_SIZE, "%u\n", attr_val);
}

#ifdef CONFIG_UFS_SHID_POC
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

	if (ufshid_spec_chk(hid) < HID_3_0_VER) {
		INFO_MSG("do not support hid version %.4x", hid->hid_ver);
		return snprintf(buf, PAGE_SIZE, "hid ver %.4x\n", hid->hid_ver);
	}

	if (ufshid_read_attr(hid, QUERY_ATTR_IDN_HID_STATE, &attr_val))
		return -EINVAL;

	if (attr_val >= HID_NUM_DEV_STATES)
		return -EINVAL;

	INFO_MSG("hid_state %s", states[attr_val]);

	return snprintf(buf, PAGE_SIZE, "%s\n", states[attr_val]);
}

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

	if (ufshid_write_attr(hid,QUERY_ATTR_IDN_HID_OPERATION, val))
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
	define_sysfs_ro(version),
	define_sysfs_ro(color),
	define_sysfs_rw(trigger),
	define_sysfs_rw(trigger_interval),
	define_sysfs_rw(on_idle_delay),

	/* for LBA Mode (L2P) */
	define_sysfs_rw(lba_trigger_mode),
	define_sysfs_ro(max_lba_range_size),
	define_sysfs_ro(max_lba_range_cnt),
	define_sysfs_ro(l2p_frag_lvl),
	define_sysfs_rw(l2p_defrag_threshold),
	define_sysfs_ro(l2p_param_print),

	/* HID 3.0 */
	define_sysfs_ro(progress_ratio),
	define_sysfs_ro(available_size),
	define_sysfs_rw(hid_size),

	/* debug */
	define_sysfs_rw(debug),
#ifdef CONFIG_UFS_SHID_POC
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

inline int ufshid_is_hid_req(struct scsi_cmnd *scmd)
{
	return scmd->cmnd[0] == WRITE_BUFFER && scmd->cmnd[1] == 0x1D;
}

MODULE_LICENSE("GPL v2");
