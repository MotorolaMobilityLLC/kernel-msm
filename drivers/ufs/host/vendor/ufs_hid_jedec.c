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
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ufs_hba *hba = dev_get_drvdata(dev);

	return to_hba_priv(hba)->hid_sup ? attr->mode : 0;
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

int moto_hid_jedec_init(struct ufs_hba *hba)
{
	int err;

	dev_info(hba->dev,"moto ufs hid init\n");

	err = sysfs_create_groups(&hba->dev->kobj,
				ufs_sysfs_hid_groups);
	if (err)
		dev_err(hba->dev, "%s: Failed to add a pixel group\n",
				__func__);
	return 0;
}