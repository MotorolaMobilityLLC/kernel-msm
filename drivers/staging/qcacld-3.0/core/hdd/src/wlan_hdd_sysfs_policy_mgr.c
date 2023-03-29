/*
 * Copyright (c) 2011-2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <wlan_hdd_main.h>
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_policy_mgr.h>
#include "osif_sync.h"
#include <wlan_policy_mgr_api.h>
#include <wma_api.h>
#include <wlan_policy_mgr_ucfg.h>

static ssize_t hdd_pm_cinfo_show(struct hdd_context *hdd_ctx)
{
	struct policy_mgr_conc_connection_info *conn_info;
	uint32_t i = 0, len = 0;

	hdd_enter();

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	conn_info = policy_mgr_get_conn_info(&len);
	pr_info("+--------------------------+\n");
	for (i = 0; i < len; i++) {
		if (!conn_info->in_use)
			continue;

		pr_info("|table_index[%d]\t\t\n", i);
		pr_info("|\t|vdev_id - %-10d|\n", conn_info->vdev_id);
		pr_info("|\t|freq    - %-10d|\n", conn_info->freq);
		pr_info("|\t|bw      - %-10d|\n", conn_info->bw);
		pr_info("|\t|mode    - %-10d|\n", conn_info->mode);
		pr_info("|\t|mac_id  - %-10d|\n", conn_info->mac);
		pr_info("|\t|in_use  - %-10d|\n", conn_info->in_use);
		pr_info("+--------------------------+\n");
		conn_info++;
	}

	pr_info("|\t|current state dbs - %-10d|\n",
		policy_mgr_is_current_hwmode_dbs(hdd_ctx->psoc));

	hdd_exit();
	return 0;
}

static ssize_t hdd_sysfs_pm_cminfo_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	ssize_t err_size;

	if (wlan_hdd_validate_context(hdd_ctx))
		return 0;

	err_size = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy),
					   &psoc_sync);
	if (err_size)
		return err_size;

	err_size = hdd_pm_cinfo_show(hdd_ctx);

	osif_psoc_sync_op_stop(psoc_sync);
	return err_size;
}

static ssize_t
__hdd_sysfs_pm_pcl_store(struct hdd_context *hdd_ctx,
			 struct kobj_attribute *attr,
			 const char *buf,
			 size_t count)
{
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	uint8_t weight_list[NUM_CHANNELS] = {0};
	uint32_t pcl[NUM_CHANNELS] = {0};
	uint32_t pcl_len = 0, val, i = 0;
	char *sptr, *token;
	QDF_STATUS status;
	int ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);

	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = buf_local;
	hdd_debug("pm_pcl: count %zu buf_local:(%s)",
		  count, buf_local);

	/* Get val */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val))
		return -EINVAL;

	if (!hdd_ctx->config->is_unit_test_framework_enabled) {
		hdd_warn_rl("UT framework is disabled");
		return -EINVAL;
	}

	status = policy_mgr_get_pcl(hdd_ctx->psoc, val,
				    pcl, &pcl_len,
				    weight_list, QDF_ARRAY_SIZE(weight_list));

	if (status != QDF_STATUS_SUCCESS)
		hdd_err("can't get pcl policy manager");

	hdd_debug("PCL Freq list for role[%d] is {", val);
	for (i = 0 ; i < pcl_len; i++)
		hdd_debug(" %d, ", pcl[i]);
	hdd_debug("}--------->\n");

	return count;
}

static ssize_t
hdd_sysfs_pm_pcl_store(struct kobject *kobj,
		       struct kobj_attribute *attr,
		       const char *buf,
		       size_t count)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	ssize_t errno_size;
	int ret;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	errno_size = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy),
					     &psoc_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_pm_pcl_store(hdd_ctx, attr,
					      buf, count);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_pm_dbs_store(struct hdd_context *hdd_ctx,
			 struct kobj_attribute *attr,
			 const char *buf,
			 size_t count)
{
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	char *sptr, *token;
	QDF_STATUS status;
	int ret, value;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);
	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	if (!hdd_ctx->config->is_unit_test_framework_enabled) {
		hdd_warn_rl("UT framework is disabled");
		return -EINVAL;
	}

	sptr = buf_local;
	hdd_debug("pm_pcl: count %zu buf_local:(%s)",
		  count, buf_local);

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	switch (value) {
	case 0:
	case 1:
		wma_set_dbs_capability_ut(value);
		break;
	default:
		hdd_err_rl("invalid value %d", value);
		return -EINVAL;
	}

	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	switch (value) {
	case PM_THROUGHPUT:
	case PM_POWERSAVE:
	case PM_LATENCY:
		status = ucfg_policy_mgr_set_sys_pref(hdd_ctx->psoc, value);
		if (status != QDF_STATUS_SUCCESS) {
			hdd_err("ucfg_policy_mgr_set_sys_pref failed");
			return -EINVAL;
		}
		break;
	default:
		hdd_err_rl("invalid value %d", value);
		return -EINVAL;
	}

	return count;
}

static ssize_t
hdd_sysfs_pm_dbs_store(struct kobject *kobj,
		       struct kobj_attribute *attr,
		       const char *buf,
		       size_t count)
{
	struct osif_psoc_sync *psoc_sync;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	ssize_t errno_size;
	int ret;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	errno_size = osif_psoc_sync_op_start(wiphy_dev(hdd_ctx->wiphy),
					     &psoc_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_pm_dbs_store(hdd_ctx, attr,
					      buf, count);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno_size;
}

static struct kobj_attribute pm_pcl_attribute =
	__ATTR(pm_pcl, 0220, NULL,
	       hdd_sysfs_pm_pcl_store);

static struct kobj_attribute pm_cinfo_attribute =
	__ATTR(pm_cinfo, 0440,
	       hdd_sysfs_pm_cminfo_show, NULL);

static struct kobj_attribute pm_dbs_attribute =
	__ATTR(pm_dbs, 0220, NULL,
	       hdd_sysfs_pm_dbs_store);

int hdd_sysfs_pm_pcl_create(struct kobject *driver_kobject)
{
	int error;

	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return -EINVAL;
	}

	error = sysfs_create_file(driver_kobject,
				  &pm_pcl_attribute.attr);
	if (error)
		hdd_err("could not create pm_pcl sysfs file");

	return error;
}

void
hdd_sysfs_pm_pcl_destroy(struct kobject *driver_kobject)
{
	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return;
	}
	sysfs_remove_file(driver_kobject, &pm_pcl_attribute.attr);
}

void hdd_sysfs_pm_cinfo_create(struct kobject *driver_kobject)
{
	if (sysfs_create_file(driver_kobject, &pm_cinfo_attribute.attr))
		hdd_err("Failed to create pktlog sysfs entry");
}

void hdd_sysfs_pm_cinfo_destroy(struct kobject *driver_kobject)
{
	sysfs_remove_file(driver_kobject, &pm_cinfo_attribute.attr);
}

int hdd_sysfs_pm_dbs_create(struct kobject *driver_kobject)
{
	int error;

	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return -EINVAL;
	}

	error = sysfs_create_file(driver_kobject,
				  &pm_dbs_attribute.attr);
	if (error)
		hdd_err("could not create pm_dbs sysfs file");

	return error;
}

void
hdd_sysfs_pm_dbs_destroy(struct kobject *driver_kobject)
{
	if (!driver_kobject) {
		hdd_err("could not get driver kobject!");
		return;
	}
	sysfs_remove_file(driver_kobject, &pm_dbs_attribute.attr);
}
