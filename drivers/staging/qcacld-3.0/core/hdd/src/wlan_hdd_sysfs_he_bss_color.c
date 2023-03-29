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

/**
 * DOC: wlan_hdd_sysfs_he_bss_color.c
 *
 * implementation for creating sysfs file he_bss_color
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include <wlan_hdd_sysfs.h>
#include "wlan_hdd_sysfs_he_bss_color.h"
#include <sme_api.h>

static ssize_t
__hdd_sysfs_he_bss_color_store(struct net_device *net_dev,
			       char const *buf, size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	struct hdd_context *hdd_ctx;
	char *sptr, *token;
	QDF_STATUS status;
	int value, ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);

	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = buf_local;
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	status = sme_set_he_bss_color(hdd_ctx->mac_handle, adapter->vdev_id,
				      value);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err_rl("Failed to set HE BSS color, status %d", status);
		return -EINVAL;
	}

	return count;
}

static ssize_t
hdd_sysfs_he_bss_color_store(struct device *dev,
			     struct device_attribute *attr,
			     char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_he_bss_color_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(he_bss_color, 0220,
		   NULL, hdd_sysfs_he_bss_color_store);

int hdd_sysfs_he_bss_color_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_he_bss_color);
	if (error)
		hdd_err("could not create he_bss_color sysfs file");

	return error;
}

void hdd_sysfs_he_bss_color_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_he_bss_color);
}
