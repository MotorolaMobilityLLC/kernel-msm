/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_sysfs_dump_in_progress.c
 *
 * Dump in progress sysfs implementation
 */

#include <linux/kobject.h>

#include "wlan_hdd_includes.h"
#include "wlan_hdd_sysfs_dump_in_progress.h"
#include "wlan_hdd_sysfs.h"
#include "osif_sync.h"

static ssize_t
__hdd_sysfs_dump_in_progress_store(struct hdd_context *hdd_ctx,
				   struct kobj_attribute *attr,
				   char const *buf, size_t count)
{
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	char *sptr, *token;
	int value, ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);
	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = buf_local;
	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	hdd_debug_rl("dump in progress %d", value);
	if (value < 0 || value > 1)
		return -EINVAL;

	hdd_ctx->dump_in_progress = value;

	return count;
}

static ssize_t hdd_sysfs_dump_in_progress_store(struct kobject *kobj,
						struct kobj_attribute *attr,
						char const *buf, size_t count)
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

	errno_size = __hdd_sysfs_dump_in_progress_store(hdd_ctx, attr,
							buf, count);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno_size;
}

static ssize_t  __hdd_sysfs_dump_in_progress_show(struct hdd_context *hdd_ctx,
						  struct kobj_attribute *attr,
						  char *buf)
{
	ssize_t ret_val;

	hdd_debug_rl("dump in progress %d", hdd_ctx->dump_in_progress);
	ret_val = scnprintf(buf, PAGE_SIZE, "%d\n", hdd_ctx->dump_in_progress);

	return ret_val;
}

static ssize_t hdd_sysfs_dump_in_progress_show(struct kobject *kobj,
					       struct kobj_attribute *attr,
					       char *buf)
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

	errno_size = __hdd_sysfs_dump_in_progress_show(hdd_ctx, attr, buf);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno_size;
}

static struct kobj_attribute dump_in_progress_attribute =
	__ATTR(dump_in_progress, 0660, hdd_sysfs_dump_in_progress_show,
	       hdd_sysfs_dump_in_progress_store);

void hdd_sysfs_create_dump_in_progress_interface(struct kobject *wifi_kobject)
{
	int error;

	if (!wifi_kobject) {
		hdd_err("could not get wifi kobject!");
		return;
	}
	error = sysfs_create_file(wifi_kobject,
				  &dump_in_progress_attribute.attr);
	if (error)
		hdd_err("could not create dump in progress sysfs file");
}

void hdd_sysfs_destroy_dump_in_progress_interface(struct kobject *wifi_kobject)
{
	if (!wifi_kobject) {
		hdd_err("could not get wifi kobject!");
		return;
	}

	sysfs_remove_file(wifi_kobject,
			  &dump_in_progress_attribute.attr);
}

