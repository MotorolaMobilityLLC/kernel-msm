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


extern struct workqueue_struct *system_highpri_wq;

/* for manual gc */
static ssize_t manual_gc_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);
	u32 status = MANUAL_GC_OFF;
	if (ufs->manual_gc.state == MANUAL_GC_DISABLE)
		return scnprintf(buf, PAGE_SIZE, "%s", "disabled\n");
	if (ufs->manual_gc.hagc_support) {
		int err;
		if (!ufshcd_eh_in_progress(hba)) {
			pm_runtime_get_sync(hba->dev);
			err = ufshcd_query_attr_retry(hba,
				UPIU_QUERY_OPCODE_READ_ATTR,
				QUERY_ATTR_IDN_MANUAL_GC_STATUS, 0, 0, &status);
			pm_runtime_put_sync(hba->dev);
			ufs->manual_gc.hagc_support = err ? false: true;
		}
	}
	if (!ufs->manual_gc.hagc_support)
		return scnprintf(buf, PAGE_SIZE, "%s", "bkops\n");
	return scnprintf(buf, PAGE_SIZE, "%s",
			status == MANUAL_GC_OFF ? "off\n" : "on\n");
}

static int manual_gc_enable(struct ufs_hba *hba, u32 *value)
{
	int ret;
	if (ufshcd_eh_in_progress(hba))
		return -EBUSY;
	pm_runtime_get_sync(hba->dev);
	ret = ufshcd_query_attr_retry(hba,
				UPIU_QUERY_OPCODE_WRITE_ATTR,
				QUERY_ATTR_IDN_MANUAL_GC_CONT, 0, 0,
				value);
	pm_runtime_put_sync(hba->dev);
	return ret;
}

static ssize_t manual_gc_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);
	u32 value;
	int err = 0;
	if (kstrtou32(buf, 0, &value))
		return -EINVAL;
	if (value >= MANUAL_GC_MAX)
		return -EINVAL;
	if (ufshcd_eh_in_progress(hba))
		return -EBUSY;
	if (value == MANUAL_GC_DISABLE || value == MANUAL_GC_ENABLE) {
		ufs->manual_gc.state = value;
		return count;
	}
	if (ufs->manual_gc.state == MANUAL_GC_DISABLE)
		return count;
	if (ufs->manual_gc.hagc_support)
		ufs->manual_gc.hagc_support =
			manual_gc_enable(hba, &value) ? false : true;
	pm_runtime_get_sync(hba->dev);

	if (!ufs->manual_gc.hagc_support) {
		err = ufshcd_bkops_ctrl(hba, (value == MANUAL_GC_ON) ?
					BKOPS_STATUS_NON_CRITICAL:
					BKOPS_STATUS_CRITICAL);
		if (!hba->auto_bkops_enabled)
			err = -EAGAIN;
	}
	/* flush wb buffer */
	if (hba->dev_info.wspecversion >= 0x0310) {
		enum query_opcode opcode = (value == MANUAL_GC_ON) ?
						UPIU_QUERY_OPCODE_SET_FLAG:
						UPIU_QUERY_OPCODE_CLEAR_FLAG;
		u8 index = ufshcd_wb_get_query_index(hba);

		ufshcd_query_flag_retry(hba, opcode,
				QUERY_FLAG_IDN_WB_BUFF_FLUSH_DURING_HIBERN8,
				index, NULL);
		ufshcd_query_flag_retry(hba, opcode,
				QUERY_FLAG_IDN_WB_BUFF_FLUSH_EN, index, NULL);
	}
	if (err || hrtimer_active(&ufs->manual_gc.hrtimer)) {
		pm_runtime_put_sync(hba->dev);
		return count;
	} else {
		/* pm_runtime_put_sync in delay_ms */
		hrtimer_start(&ufs->manual_gc.hrtimer,
			ms_to_ktime(ufs->manual_gc.delay_ms),
			HRTIMER_MODE_REL);
	}
	return count;
}

/* for manual gc */
static ssize_t manual_gc_status_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);
	u32 status = MANUAL_GC_OFF;
	if (ufs->manual_gc.state == MANUAL_GC_DISABLE)
		return scnprintf(buf, PAGE_SIZE, "%s", "disabled\n");
	if (ufs->manual_gc.hagc_support) {
		int err;
		if (!ufshcd_eh_in_progress(hba)) {
			pm_runtime_get_sync(hba->dev);
			err = ufshcd_query_attr_retry(hba,
				UPIU_QUERY_OPCODE_READ_ATTR,
				QUERY_ATTR_IDN_MANUAL_GC_STATUS, 0, 0, &status);
			pm_runtime_put_sync(hba->dev);
			ufs->manual_gc.hagc_support = err ? false: true;
		}
	}
	if (!ufs->manual_gc.hagc_support)
		return scnprintf(buf, PAGE_SIZE, "%s", "bkops\n");
	return scnprintf(buf, PAGE_SIZE, "%u", status);
}

static ssize_t manual_gc_hold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);
	return snprintf(buf, PAGE_SIZE, "%lu\n", ufs->manual_gc.delay_ms);
}

static ssize_t manual_gc_hold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);
	unsigned long value;
	if (kstrtoul(buf, 0, &value))
		return -EINVAL;
	ufs->manual_gc.delay_ms = value;
	return count;
}

static enum hrtimer_restart pixel_mgc_hrtimer_handler(struct hrtimer *timer)
{
	struct ufs_qcom_host *ufs = container_of(timer, struct ufs_qcom_host,
					manual_gc.hrtimer);
	queue_work(ufs->manual_gc.mgc_workq, &ufs->manual_gc.hibern8_work);
	return HRTIMER_NORESTART;
}

static void pixel_mgc_hibern8_work(struct work_struct *work)
{
	struct ufs_qcom_host *ufs = container_of(work, struct ufs_qcom_host,
					manual_gc.hibern8_work);
	pm_runtime_put_sync(ufs->hba->dev);
	/* bkops will be disabled when power down */
}

void pixel_init_manual_gc(struct ufs_hba *hba)
{
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);
	struct ufs_manual_gc *mgc = &ufs->manual_gc;
	char wq_name[sizeof("ufs_mgc_hibern8_work")];
	mgc->state = MANUAL_GC_ENABLE;
	mgc->hagc_support = true;
	mgc->delay_ms = UFSHCD_MANUAL_GC_HOLD_HIBERN8;

	hrtimer_init(&mgc->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	mgc->hrtimer.function = pixel_mgc_hrtimer_handler;

	INIT_WORK(&mgc->hibern8_work, pixel_mgc_hibern8_work);
	snprintf(wq_name, ARRAY_SIZE(wq_name), "ufs_mgc_hibern8_work_%d",
			hba->host->host_no);
	ufs->manual_gc.mgc_workq = create_singlethread_workqueue(wq_name);
}

static DEVICE_ATTR_RW(manual_gc);
static DEVICE_ATTR_RO(manual_gc_status);
static DEVICE_ATTR_RW(manual_gc_hold);

static struct attribute *pixel_sysfs_ufshcd_attrs[] = {
	&dev_attr_manual_gc.attr,
	&dev_attr_manual_gc_hold.attr,
	NULL
};

static struct attribute *pixel_sysfs_ufshcd_hagc_status_attrs[] = {
	&dev_attr_manual_gc_status.attr,
	NULL
};

static const struct attribute_group pixel_sysfs_hagc_status_group = {
	.name = "attributes_hid",
	.attrs = pixel_sysfs_ufshcd_hagc_status_attrs,
};

static const struct attribute_group pixel_sysfs_default_group = {
	.attrs = pixel_sysfs_ufshcd_attrs,
};

static const struct attribute_group *pixel_sysfs_hid_groups[] = {
	&pixel_sysfs_default_group,
	&pixel_sysfs_hagc_status_group,
	NULL,
};


static void pixel_ufs_update_sysfs_work(struct work_struct *work)
{
	struct ufs_qcom_host *ufs = container_of(work, struct ufs_qcom_host,
						update_sysfs_work);
	struct ufs_hba *hba = ufs->hba;
	int err;
	err = sysfs_create_groups(&hba->dev->kobj,
				pixel_sysfs_hid_groups);
	if (err)
		dev_err(hba->dev, "%s: Failed to add a pixel group\n",
				__func__);
}

static void pixel_ufs_update_sysfs(void *data, struct ufs_hba *hba)
{
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);
	queue_work(system_highpri_wq, &ufs->update_sysfs_work);
}

int pixel_init(struct ufs_hba *hba)
{
	struct ufs_qcom_host *ufs = ufshcd_get_variant(hba);
	int ret;
	ret = register_trace_android_vh_ufs_update_sysfs(
				pixel_ufs_update_sysfs, NULL);
	if (ret)
		return ret;
	INIT_WORK(&ufs->update_sysfs_work, pixel_ufs_update_sysfs_work);
	return 0;
}
