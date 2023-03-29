/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_txrx_fw_stats.c
 *
 * implementation for creating sysfs file txrx_fw_stats
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_txrx_fw_stats.h>
#include "wma.h"
#include "wma_api.h"

static int hdd_sysfs_set_vdev(struct hdd_adapter *adapter,
			      int id, const char *id_string,
			      int value)
{
	int errno;

	hdd_debug("%s %d", id_string, value);
	errno = wma_cli_set_command(adapter->vdev_id, id, value, VDEV_CMD);
	if (errno)
		hdd_err("Failed to set firmware, errno %d", errno);

	return errno;
}

#define hdd_sysfs_set_vdev(adapter, id, value) \
			   hdd_sysfs_set_vdev(adapter, id, #id, value)

static ssize_t
__hdd_sysfs_txrx_fw_stats_store(struct net_device *net_dev,
				char const *buf, size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	struct hdd_context *hdd_ctx;
	char *sptr, *token;
	uint32_t value;
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

	sptr = buf_local;
	hdd_debug("txrx_fw_stats: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	ret = hdd_sysfs_set_vdev(adapter,
				 WMA_VDEV_TXRX_FWSTATS_ENABLE_CMDID, value);

	if (ret) {
		hdd_err_rl("failed to set txrx fw stats: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_txrx_fw_stats_store(struct device *dev,
			      struct device_attribute *attr,
			      char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_txrx_fw_stats_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(txrx_fw_stats, 0220,
		   NULL, hdd_sysfs_txrx_fw_stats_store);

int hdd_sysfs_txrx_fw_stats_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_txrx_fw_stats);
	if (error)
		hdd_err("could not create txrx_fw_stats sysfs file");

	return error;
}

void hdd_sysfs_txrx_fw_stats_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_txrx_fw_stats);
}
