// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Pixel-Specific UFS feature support
 *
 * Copyright 2020 Google LLC
 *
 * Authors: Jaegeuk Kim <jaegeuk@google.com>
 */

#include "ufs-qcom.h"
#include <trace/hooks/ufshcd.h>
#include <ufshcd-priv.h>
#include "ufs_hid_jedec.h"

extern struct workqueue_struct *system_highpri_wq;

//----------------------------
static const char *moto_hid_state_to_string(enum moto_hid_state state)
{
	if (state < MOTO_NUM_UFS_HID_STATES)
		return moto_hid_states[state];

	return "unknown";
}

static int hid_query_attr(struct ufs_hba *hba, enum query_opcode opcode,
			enum attr_idn idn, u32 *attr_val)
{
	int ret;

	down(&hba->host_sem);
	if (!ufshcd_is_user_access_allowed(hba)) {
		up(&hba->host_sem);
		return -EBUSY;
	}

	ufshcd_rpm_get_sync(hba);
	ret = ufshcd_query_attr(hba, opcode, idn, 0, 0, attr_val);
	ufshcd_rpm_put_sync(hba);

	up(&hba->host_sem);
	return ret;
}

static ssize_t analysis_trigger_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	int mode;
	int ret;

	if (sysfs_streq(buf, "enable"))
		mode = MOTO_HID_ANALYSIS_ENABLE;
	else if (sysfs_streq(buf, "disable"))
		mode = MOTO_HID_ANALYSIS_AND_DEFRAG_DISABLE;
	else
		return -EINVAL;

	ret = hid_query_attr(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
			MOTO_QUERY_ATTR_IDN_HID_DEFRAG_OPERATION, &mode);

	return ret < 0 ? ret : count;
}

static DEVICE_ATTR_WO(analysis_trigger);

static ssize_t defrag_trigger_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	int mode;
	int ret;

	if (sysfs_streq(buf, "enable"))
		mode = MOTO_HID_ANALYSIS_AND_DEFRAG_ENABLE;
	else if (sysfs_streq(buf, "disable"))
		mode = MOTO_HID_ANALYSIS_AND_DEFRAG_DISABLE;
	else
		return -EINVAL;

	ret = hid_query_attr(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
			MOTO_QUERY_ATTR_IDN_HID_DEFRAG_OPERATION, &mode);

	return ret < 0 ? ret : count;
}

static DEVICE_ATTR_WO(defrag_trigger);

static ssize_t fragmented_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int ret;

	ret = hid_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			MOTO_QUERY_ATTR_IDN_HID_AVAILABLE_SIZE, &value);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%u\n", value);
}

static DEVICE_ATTR_RO(fragmented_size);

static ssize_t defrag_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int ret;

	ret = hid_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			MOTO_QUERY_ATTR_IDN_HID_SIZE, &value);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%u\n", value);
}

static ssize_t defrag_size_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int ret;

	if (kstrtou32(buf, 0, &value))
		return -EINVAL;

	ret = hid_query_attr(hba, UPIU_QUERY_OPCODE_WRITE_ATTR,
			MOTO_QUERY_ATTR_IDN_HID_SIZE, &value);

	return ret < 0 ? ret : count;
}

static DEVICE_ATTR_RW(defrag_size);

static ssize_t progress_ratio_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int ret;

	ret = hid_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			MOTO_QUERY_ATTR_IDN_HID_PROGRESS_RATIO, &value);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%u\n", value);
}

static DEVICE_ATTR_RO(progress_ratio);

static ssize_t state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	u32 value;
	int ret;

	ret = hid_query_attr(hba, UPIU_QUERY_OPCODE_READ_ATTR,
			MOTO_QUERY_ATTR_IDN_HID_STATE, &value);
	if (ret)
		return ret;

	return sysfs_emit(buf, "%s\n", moto_hid_state_to_string(value));
}

static DEVICE_ATTR_RO(state);

static struct attribute *ufs_sysfs_hid[] = {
	&dev_attr_analysis_trigger.attr,
	&dev_attr_defrag_trigger.attr,
	&dev_attr_fragmented_size.attr,
	&dev_attr_defrag_size.attr,
	&dev_attr_progress_ratio.attr,
	&dev_attr_state.attr,
	NULL,
};

static umode_t ufs_sysfs_hid_is_visible(struct kobject *kobj,
		struct attribute *attr, int n)
{
	u8 *desc_buf;
	int err;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);

	desc_buf = kzalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!desc_buf) {
		goto out;
	}

	err = ufshcd_read_desc_param(hba, QUERY_DESC_IDN_DEVICE, 0, 0, desc_buf,
				     QUERY_DESC_MAX_SIZE);
	if (err) {
		dev_err(hba->dev, "%s: Failed reading Device Desc. err = %d\n",
			__func__, err);
		goto out;
	}

	ufs->hid_jedec.hid_sup = get_unaligned_be32(desc_buf +
			DEVICE_DESC_PARAM_EXT_UFS_FEATURE_SUP) &
			MOTO_UFS_DEV_HID_SUPPORT;

	dev_err(dev,"%s:  get hid jedec support = %d \n",__func__, ufs->hid_jedec.hid_sup);

	out:
	kfree(desc_buf);

	return ufs->hid_jedec.hid_sup ? attr->mode : 0;
	//return	hba->dev_info.hid_sup ? attr->mode : 0;
}

static const struct attribute_group ufs_sysfs_hid_group = {
	.name = "motohid",
	.attrs = ufs_sysfs_hid,
	.is_visible = ufs_sysfs_hid_is_visible,
};
static const struct attribute_group *ufs_sysfs_hid_groups[] = {
	&ufs_sysfs_hid_group,
	NULL,
};

void moto_sysfs_update_hid(struct ufs_hba *hba)
{
	int ret;
	struct device *dev = hba->dev;
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);
	struct moto_hid_jedec_feature *hid_jedec = &ufs->hid_jedec;

    if (hid_jedec->hid_sup) {
        sysfs_remove_group(&dev->kobj, &ufs_sysfs_hid_group);
        ret = sysfs_create_group(&dev->kobj, &ufs_sysfs_hid_group);
		if (ret) {
			dev_err(dev,
				"%s: hid_group creation failed (err = %d)\n",
				__func__, ret);
			return;
		}
    }
}

static void hid_jedec_ufs_update_sysfs_work(struct work_struct *work)
{
	struct ufs_qcom_host *ufs = container_of(work, struct ufs_qcom_host,
						update_sysfs_work);
	struct ufs_hba *hba = ufs->hba;
	int err;

	err = sysfs_create_groups(&hba->dev->kobj,
				ufs_sysfs_hid_groups);
	if (err)
		dev_err(hba->dev, "%s: Failed to add a pixel group\n",
				__func__);
}

static void hid_jedec_ufs_update_sysfs(void *data, struct ufs_hba *hba)
{
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);

	// queue_delayed_work(system_highpri_wq, &ufs->update_sysfs_work, msecs_to_jiffies(100));
	queue_work(system_highpri_wq, &ufs->update_sysfs_work);
}

int moto_hid_jedec_init(struct ufs_hba *hba)
{
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);
	int ret;

	ret = register_trace_android_vh_ufs_update_sysfs(
				hid_jedec_ufs_update_sysfs, NULL);
	if (ret)
		return ret;

	INIT_WORK(&ufs->update_sysfs_work, hid_jedec_ufs_update_sysfs_work);
	return 0;
}
