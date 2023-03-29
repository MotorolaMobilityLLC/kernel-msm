/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_crash_inject.c
 *
 * implementation for creating sysfs file crash_inject
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include "wlan_hdd_sysfs.h"
#include "wlan_hdd_sysfs_crash_inject.h"

static ssize_t __hdd_sysfs_crash_inject_store(
		struct net_device *net_dev,
		const char *buf, size_t count)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(net_dev);
	struct hdd_context *hdd_ctx;
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	char *sptr, *token;
	uint32_t val1, val2;
	int ret;

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

	hdd_nofl_info("crash_inject: count %zu buf_local:(%s) net_devname %s",
		      count, buf_local, net_dev->name);

	sptr = buf_local;
	/* Get val1 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val1))
		return -EINVAL;

	/* Get val2 */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &val2))
		return -EINVAL;

	ret = hdd_crash_inject(adapter, val1, val2);
	if (ret != 0) {
		hdd_err_rl("hdd_crash_inject returned %d", ret);
		return -EINVAL;
	}

	return count;
}

static ssize_t hdd_sysfs_crash_inject_store(struct device *dev,
					    struct device_attribute *attr,
					    char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_crash_inject_store(
				net_dev, buf, count);
	if (errno_size < 0)
		hdd_err_rl("errno_size %zd", errno_size);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(crash_inject, 0220,
		   NULL, hdd_sysfs_crash_inject_store);

int hdd_sysfs_crash_inject_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_crash_inject);
	if (error)
		hdd_err("could not create crash_inject sysfs file");

	return error;
}

void hdd_sysfs_crash_inject_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_crash_inject);
}
