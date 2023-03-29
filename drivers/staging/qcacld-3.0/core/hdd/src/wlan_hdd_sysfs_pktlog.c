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
#include <wlan_hdd_sysfs_pkt_log.h>
#include "osif_sync.h"

#define MAX_USER_COMMAND_SIZE_PKT_LOG_CMD 4

static ssize_t __hdd_sysfs_pkt_log_cmd_store(struct hdd_context *hdd_ctx,
					     const char *buf, size_t count)
{
	char buf_local[MAX_USER_COMMAND_SIZE_PKT_LOG_CMD + 1];
	char *sptr, *token;
	uint32_t val1, val2;
	int ret;

	hdd_enter();

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);
	if (ret)
		return -EINVAL;

	sptr = buf_local;
	/* Get val1 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val1))
		return -EINVAL;

	sptr = buf_local;
	/* Get val2 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val2))
		return -EINVAL;

	ret = hdd_process_pktlog_command(hdd_ctx, val1, val2);

	hdd_exit();
	return count;
}

static ssize_t hdd_sysfs_pkt_log_cmd_store(struct kobject *kobj,
					   struct kobj_attribute *attr,
					   const char *buf,
					   size_t count)
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

	err_size = __hdd_sysfs_pkt_log_cmd_store(hdd_ctx, buf, count);

	osif_psoc_sync_op_stop(psoc_sync);
	return err_size;
}

static struct kobj_attribute pktlog_attribute =
	__ATTR(pktlog, 0220, NULL,
	       hdd_sysfs_pkt_log_cmd_store);

void hdd_sysfs_pktlog_create(struct kobject *driver_kobject)
{
	if (sysfs_create_file(driver_kobject, &pktlog_attribute.attr))
		hdd_err("Failed to create pktlog sysfs entry");
}

void hdd_sysfs_pktlog_destroy(struct kobject *driver_kobject)
{
	sysfs_remove_file(driver_kobject, &pktlog_attribute.attr);
}
