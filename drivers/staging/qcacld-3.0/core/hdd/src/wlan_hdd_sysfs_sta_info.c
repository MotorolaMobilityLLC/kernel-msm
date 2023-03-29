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
 * DOC: wlan_hdd_sysfs_sta_info.c
 *
 * implementation for creating sysfs file sta_info
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include "wlan_hdd_sysfs_sta_info.h"

static ssize_t __show_sta_info(struct net_device *net_dev, char *buf)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_context *hdd_ctx;
	struct hdd_station_info *sta;
	int ret_val;

	hdd_enter_dev(net_dev);

	ret_val = hdd_validate_adapter(adapter);
	if (0 != ret_val)
		goto exit;

	if (adapter->device_mode != QDF_SAP_MODE)
		goto exit;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret_val)
		goto exit;

	ret_val = scnprintf(buf, PAGE_SIZE,
			    "%s    get_sta_info:\nstaAddress\n",
			    net_dev->name);

	hdd_for_each_sta_ref(adapter->sta_info_list, sta, STA_INFO_SHOW) {
		if (QDF_IS_ADDR_BROADCAST(sta->sta_mac.bytes)) {
			hdd_put_sta_info_ref(&adapter->sta_info_list, &sta,
					     true, STA_INFO_SHOW);
			continue;
		}
		ret_val += scnprintf(buf + ret_val, PAGE_SIZE - ret_val,
				     QDF_FULL_MAC_FMT " ecsa=%d\n",
				     QDF_FULL_MAC_REF(sta->sta_mac.bytes),
				     sta->ecsa_capable);

		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta, true,
				     STA_INFO_SHOW);
	}

exit:
	hdd_exit();
	return ret_val;
}

static ssize_t show_sta_info(struct device *dev,
			     struct device_attribute *attr,
			     char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __show_sta_info(net_dev, buf);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(sta_info, 0444, show_sta_info, NULL);

void hdd_sysfs_sta_info_interface_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_sta_info);
	if (error)
		hdd_err("Could not create sta_info sysfs file");
}

void hdd_sysfs_sta_info_interface_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_sta_info);
}
