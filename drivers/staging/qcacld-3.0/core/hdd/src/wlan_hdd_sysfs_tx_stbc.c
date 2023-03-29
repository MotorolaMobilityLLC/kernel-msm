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
 * DOC: wlan_hdd_sysfs_tx_stbc.c
 *
 * implementation for creating sysfs file tx_stbc
 */

#include <wlan_hdd_includes.h>
#include <wlan_hdd_sysfs.h>
#include "osif_vdev_sync.h"
#include "sme_api.h"
#include <wlan_hdd_sysfs_tx_stbc.h>

static int hdd_sysfs_get_tx_stbc(struct hdd_adapter *adapter, int *value)
{
	mac_handle_t mac_handle = adapter->hdd_ctx->mac_handle;
	int ret;

	hdd_enter();
	ret = sme_get_ht_config(mac_handle, adapter->vdev_id,
				WNI_CFG_HT_CAP_INFO_TX_STBC);
	if (ret < 0) {
		hdd_err("Failed to get TX STBC value");
	} else {
		*value = ret;
		ret = 0;
	}

	return ret;
}

static ssize_t
__hdd_sysfs_tx_stbc_show(struct net_device *net_dev, char *buf)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_context *hdd_ctx;
	int ret;
	int value;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_get_tx_stbc(adapter, &value);
	if (ret) {
		hdd_err_rl("Get_TX_STBC failed %d", ret);
		return ret;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", value);
}

static ssize_t
hdd_sysfs_tx_stbc_show(struct device *dev,
		       struct device_attribute *attr,
		       char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __hdd_sysfs_tx_stbc_show(net_dev, buf);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(tx_stbc, 0440,
		   hdd_sysfs_tx_stbc_show, NULL);

int hdd_sysfs_tx_stbc_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_tx_stbc);
	if (error)
		hdd_err("could not create tx_stbc sysfs file");

	return error;
}

void hdd_sysfs_tx_stbc_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_tx_stbc);
}
