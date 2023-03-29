/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
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
 *  DOC: wlan_hdd_sysfs_mem_stats.c
 *
 *  Implementation to add sysfs node wlan_mem_stats
 *
 */

#include <wlan_hdd_includes.h>
#include "osif_psoc_sync.h"
#include <wlan_hdd_sysfs.h>
#include <qdf_mem.h>
#include <wlan_hdd_sysfs_mem_stats.h>

static ssize_t __hdd_wlan_mem_stats_show(char *buf)
{
	return scnprintf(buf, PAGE_SIZE,
			 "DMA = %u | Kmalloc = %u | SKB = %u\n",
			 qdf_dma_mem_stats_read(),
			 qdf_heap_mem_stats_read(),
			 qdf_skb_mem_stats_read());
}

static ssize_t hdd_wlan_mem_stats_show(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       char *buf)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct osif_psoc_sync *psoc_sync;
	ssize_t length;
	int errno;

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	errno = osif_psoc_sync_op_start(hdd_ctx->parent_dev, &psoc_sync);
	if (errno)
		return errno;

	length = __hdd_wlan_mem_stats_show(buf);

	osif_psoc_sync_op_stop(psoc_sync);

	return length;
}

static struct kobj_attribute mem_stats_attribute =
	__ATTR(wlan_mem_stats, 0440, hdd_wlan_mem_stats_show, NULL);

int hdd_sysfs_mem_stats_create(struct kobject *wlan_kobject)
{
	int error;

	if (!wlan_kobject) {
		hdd_err("Could not get wlan kobject!");
		return -EINVAL;
	}
	error = sysfs_create_file(wlan_kobject, &mem_stats_attribute.attr);
	if (error)
		hdd_err("Failed to create sysfs file wlan_mem_stats");

	return error;
}

void hdd_sysfs_mem_stats_destroy(struct kobject *wlan_kobject)
{
	if (!wlan_kobject) {
		hdd_err("Could not get wlan kobject!");
		return;
	}
	sysfs_remove_file(wlan_kobject, &mem_stats_attribute.attr);
}

