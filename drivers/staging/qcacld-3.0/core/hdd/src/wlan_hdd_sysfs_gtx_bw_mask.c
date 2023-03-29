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
 * DOC: wlan_hdd_sysfs_gtx_bw_mask.c
 *
 * implementation for creating sysfs file gtx_bw_mask
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_gtx_bw_mask.h>
#include "wma.h"
#include "wma_api.h"

static int hdd_sysfs_set_green_tx_param(struct hdd_adapter *adapter,
					green_tx_param id,
					const char *id_string,
					int value)
{
	int errno;

	hdd_debug("%s %d", id_string, value);
	errno = wma_cli_set_command(adapter->vdev_id, id, value, GTX_CMD);
	if (errno)
		hdd_err("Failed to set firmware, errno %d", errno);

	return errno;
}

#define hdd_sysfs_set_green_tx_param(adapter, id, value) \
			hdd_sysfs_set_green_tx_param(adapter, id, #id, value)

static int hdd_sysfs_get_green_tx_param(struct hdd_adapter *adapter,
					green_tx_param id,
					const char *id_string)
{
	int value;

	value = wma_cli_get_command(adapter->vdev_id, id, GTX_CMD);

	hdd_debug("%s %d", id_string, value);

	return value;
}

#define hdd_sysfs_get_green_tx_param(adapter, id) \
			hdd_sysfs_get_green_tx_param(adapter, id, #id)

static ssize_t
__hdd_sysfs_gtx_bw_mask_store(struct net_device *net_dev,
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
	hdd_debug("gtx_bw_mask: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &value))
		return -EINVAL;

	ret = hdd_sysfs_set_green_tx_param(adapter,
					   WMI_VDEV_PARAM_GTX_BW_MASK,
					   value);

	if (ret) {
		hdd_err_rl("failed to set green tx BW Mask: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_gtx_bw_mask_store(struct device *dev,
			    struct device_attribute *attr,
			    char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_gtx_bw_mask_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static ssize_t
__hdd_sysfs_gtx_bw_mask_show(struct net_device *net_dev, char *buf)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_context *hdd_ctx;
	int ret, value;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	value = hdd_sysfs_get_green_tx_param(adapter,
					     WMI_VDEV_PARAM_GTX_BW_MASK);
	if (value < 0) {
		hdd_err_rl("failed to get green tx BW Mask");
		return -EINVAL;
	}

	return scnprintf(buf, PAGE_SIZE, "%d", value);
}

static ssize_t
hdd_sysfs_gtx_bw_mask_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_gtx_bw_mask_show(net_dev, buf);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(gtx_bw_mask, 0660, hdd_sysfs_gtx_bw_mask_show,
		   hdd_sysfs_gtx_bw_mask_store);

int hdd_sysfs_gtx_bw_mask_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_gtx_bw_mask);
	if (error)
		hdd_err("could not create gtx_bw_mask sysfs file");

	return error;
}

void hdd_sysfs_gtx_bw_mask_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_gtx_bw_mask);
}
