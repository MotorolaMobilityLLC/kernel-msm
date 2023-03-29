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
 * DOC: wlan_hdd_sysfs_tdls_peers.c
 *
 * Implementation for creating sysfs file tdls_peers
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include "wlan_hdd_object_manager.h"
#include "wlan_hdd_sysfs_tdls_peers.h"

/**
 * __show_tdls_all_peers() - dump all TDLS peer info into output string
 * @net_dev: net device
 * @buf: output string buffer to hold the peer info
 *
 * Return: The size (in bytes) of the valid peer info in the output buffer
 */
static int __show_tdls_all_peers(struct net_device *net_dev, char *buf)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_vdev *vdev;
	int ret_val;

	hdd_enter_dev(net_dev);

	ret_val = scnprintf(buf, PAGE_SIZE, "%s     getTdlsPeers:",
			    net_dev->name);

	if (hdd_validate_adapter(adapter)) {
		ret_val += scnprintf(buf + ret_val, PAGE_SIZE - ret_val,
				     "\nHDD adapter is not valid\n");
		goto exit;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (0 != (wlan_hdd_validate_context(hdd_ctx))) {
		ret_val += scnprintf(buf + ret_val, PAGE_SIZE - ret_val,
				     "\nHDD context is not valid\n");
		goto exit;
	}

	if ((QDF_STA_MODE != adapter->device_mode) &&
	    (QDF_P2P_CLIENT_MODE != adapter->device_mode)) {
		ret_val += scnprintf(buf + ret_val, PAGE_SIZE - ret_val,
				     "\nNo TDLS support for this adapter\n");
		goto exit;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		ret_val += scnprintf(buf + ret_val, PAGE_SIZE - ret_val,
				     "\nVDEV is NULL\n");
		goto exit;
	}
	ret_val += wlan_cfg80211_tdls_get_all_peers(vdev, buf + ret_val,
						    PAGE_SIZE - ret_val);
	hdd_objmgr_put_vdev(vdev);

exit:
	if ((PAGE_SIZE - ret_val) > 0)
		ret_val += scnprintf(buf + ret_val, PAGE_SIZE - ret_val, "\n");

	hdd_exit();
	return ret_val;
}

static ssize_t show_tdls_all_peers(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __show_tdls_all_peers(net_dev, buf);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(tdls_peers, 0444, show_tdls_all_peers, NULL);

void hdd_sysfs_tdls_peers_interface_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev,
				   &dev_attr_tdls_peers);
	if (error)
		hdd_err("could not create tdls_peers sysfs file");
}

void hdd_sysfs_tdls_peers_interface_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_tdls_peers);
}
