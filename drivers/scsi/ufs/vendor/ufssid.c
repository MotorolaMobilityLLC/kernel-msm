/*
 * Universal Flash Storage Stream ID (UFS SID)
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
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
#include "ufssid.h"

static int ufssid_create_sysfs(struct ufssid_dev *sid);

static inline int ufssid_version_check(int spec_version)
{
	INFO_MSG("Support SID Spec: Driver = (%.4x), Device = (%.4x)",
		 UFSSID_VER, spec_version);
	INFO_MSG("SID Driver version (%.6X%s)",
		 UFSSID_DD_VER, UFSSID_DD_VER_POST);

	if (spec_version != UFSSID_VER) {
		ERR_MSG("UFS SID version mismatched");
		return -ENODEV;
	}

	return 0;
}

void ufssid_get_dev_info(struct ufsf_feature *ufsf, u8 *desc_buf)
{
	int spec_version, ret = 0;

	if (!(get_unaligned_be32(&desc_buf[DEVICE_DESC_PARAM_SAMSUNG_SUP]) &
	    UFS_FEATURE_SUPPORT_SID_BIT)) {
		INFO_MSG("bUFSExFeaturesSupport: SID not support");
		return;
	}

	INFO_MSG("bUFSExFeaturesSupport: SID support");

	spec_version = get_unaligned_be16(&desc_buf[DEVICE_DESC_PARAM_SID_VER]);
	ret = ufssid_version_check(spec_version);
	if (ret)
		return;

	ufsf->sid_dev = kzalloc(sizeof(struct ufssid_dev), GFP_KERNEL);
	if (!ufsf->sid_dev) {
		ERR_MSG("sid_dev memory allocation failed");
		return;
	}

	ufsf->sid_dev->ufsf = ufsf;
}

void ufssid_prep_fn(struct ufsf_feature *ufsf, struct ufshcd_lrb *lrbp)
{
	struct ufssid_dev *sid = ufsf->sid_dev;
	struct scsi_cmnd *cmd = lrbp->cmd;
	struct request *request = scsi_cmd_to_rq(cmd);

	if (sid->stream_id_enabled && request->bio) {
		if (cmd->cmnd[0] == WRITE_10)
			cmd->cmnd[6] = request->bio->bi_write_hint;
		if (cmd->cmnd[0] == WRITE_16)
			cmd->cmnd[14] = request->bio->bi_write_hint;
	}
}

void ufssid_init(struct ufsf_feature *ufsf)
{
	struct ufssid_dev *sid;
	int ret;

	sid = ufsf->sid_dev;
	if (!sid) {
		ERR_MSG("sid is NULL. so SID driver is DISABLED");
		return;
	}

	ret = ufshcd_query_flag_retry(ufsf->hba, UPIU_QUERY_OPCODE_READ_FLAG,
				      QUERY_FLAG_IDN_STREAM_ID_EN, 0,
				      &sid->stream_id_enabled);
	if (ret) {
		sid->stream_id_enabled = false;
		ERR_MSG("Read query failed. So Stream ID is not enabled");
	}

	INFO_MSG("Stream ID is %s",
		 sid->stream_id_enabled ? "ENABLED" : "DISABLED");

	ret = ufssid_create_sysfs(sid);
	if (ret) {
		ERR_MSG("sysfs init failed. So SID driver is DISABLED");
		kfree(sid);
		return;
	}

	INFO_MSG("UFS SID create sysfs finished");
}

static inline void ufssid_remove_sysfs(struct ufssid_dev *sid)
{
	int ret;

	ret = kobject_uevent(&sid->kobj, KOBJ_REMOVE);
	INFO_MSG("kobject remove (%d)", ret);
	kobject_del(&sid->kobj);
}

void ufssid_remove(struct ufsf_feature *ufsf)
{
	struct ufssid_dev *sid = ufsf->sid_dev;

	INFO_MSG("Start SID release");

	ufssid_remove_sysfs(sid);

	kfree(sid);

	INFO_MSG("End SID release");
}

static ssize_t ufssid_sysfs_show_stream_id_enabled(struct ufssid_dev *sid,
						   char *buf)
{
	INFO_MSG("stream_id_enabled %d", sid->stream_id_enabled);

	return snprintf(buf, PAGE_SIZE, "%d\n", sid->stream_id_enabled);
}

static ssize_t ufssid_sysfs_store_stream_id_enabled(struct ufssid_dev *sid,
						    const char *buf,
						    size_t count)
{
	struct ufs_hba *hba = sid->ufsf->hba;
	unsigned long value;
	int ret;
	enum query_opcode opcode;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	if (!(value == 0 || value == 1))
		return -EINVAL;

	if (value == sid->stream_id_enabled)
		goto out;

	opcode = value ?
		UPIU_QUERY_OPCODE_SET_FLAG : UPIU_QUERY_OPCODE_CLEAR_FLAG;

	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_query_flag(hba, opcode, QUERY_FLAG_IDN_STREAM_ID_EN, 0,
				NULL);
	pm_runtime_put_sync(hba->dev);
	if (ret)
		return -EINVAL;

	sid->stream_id_enabled = value ? true : false;

out:
	INFO_MSG("stream_id_enabled %d", sid->stream_id_enabled);

	return count;
}

static struct ufssid_sysfs_entry ufssid_sysfs_entries[] = {
	/* Flag */
	__ATTR(stream_id_enabled, 0644,
	       ufssid_sysfs_show_stream_id_enabled,
	       ufssid_sysfs_store_stream_id_enabled),
	__ATTR_NULL
};

static ssize_t ufssid_attr_show(struct kobject *kobj, struct attribute *attr,
				char *page)
{
	struct ufssid_sysfs_entry *entry;
	struct ufssid_dev *sid;
	ssize_t error;

	entry = container_of(attr, struct ufssid_sysfs_entry, attr);
	if (!entry->show)
		return -EIO;

	sid = container_of(kobj, struct ufssid_dev, kobj);

	mutex_lock(&sid->sysfs_lock);
	error = entry->show(sid, page);
	mutex_unlock(&sid->sysfs_lock);
	return error;
}

static ssize_t ufssid_attr_store(struct kobject *kobj, struct attribute *attr,
				 const char *page, size_t length)
{
	struct ufssid_sysfs_entry *entry;
	struct ufssid_dev *sid;
	ssize_t error;

	entry = container_of(attr, struct ufssid_sysfs_entry, attr);
	if (!entry->store)
		return -EIO;

	sid = container_of(kobj, struct ufssid_dev, kobj);

	mutex_lock(&sid->sysfs_lock);
	error = entry->store(sid, page, length);
	mutex_unlock(&sid->sysfs_lock);
	return error;
}

static const struct sysfs_ops ufssid_sysfs_ops = {
	.show = ufssid_attr_show,
	.store = ufssid_attr_store,
};

static struct kobj_type ufssid_ktype = {
	.sysfs_ops = &ufssid_sysfs_ops,
	.release = NULL,
};

static int ufssid_create_sysfs(struct ufssid_dev *sid)
{
	struct device *dev = sid->ufsf->hba->dev;
	struct ufssid_sysfs_entry *entry;
	int err;

	sid->sysfs_entries = ufssid_sysfs_entries;

	kobject_init(&sid->kobj, &ufssid_ktype);
	mutex_init(&sid->sysfs_lock);

	INFO_MSG("ufssid creates sysfs ufssid %p dev->kobj %p",
		 &sid->kobj, &dev->kobj);

	err = kobject_add(&sid->kobj, kobject_get(&dev->kobj), "ufssid");
	if (!err) {
		for (entry = sid->sysfs_entries; entry->attr.name != NULL;
		     entry++) {
			INFO_MSG("ufssid sysfs attr creates: %s",
				 entry->attr.name);
			err = sysfs_create_file(&sid->kobj, &entry->attr);
			if (err) {
				ERR_MSG("create entry(%s) failed",
					entry->attr.name);
				goto kobj_del;
			}
		}
		kobject_uevent(&sid->kobj, KOBJ_ADD);
	} else {
		ERR_MSG("kobject_add failed");
	}

	return err;
kobj_del:
	err = kobject_uevent(&sid->kobj, KOBJ_REMOVE);
	INFO_MSG("kobject removed (%d)", err);
	kobject_del(&sid->kobj);
	return -EINVAL;
}

MODULE_LICENSE("GPL v2");
