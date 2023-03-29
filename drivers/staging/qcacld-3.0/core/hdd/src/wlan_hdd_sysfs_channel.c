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
 * DOC: wlan_hdd_sysfs_channel.c
 *
 * implementation for creating sysfs file channel
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include "wlan_hdd_sysfs_channel.h"

static ssize_t __show_channel_number(struct net_device *net_dev, char *buf)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_context *hdd_ctx;
	struct hdd_ap_ctx *ap_ctx;
	int chan_num = 0;
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

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		chan_num = wlan_reg_freq_to_chan(hdd_ctx->pdev,
						 ap_ctx->operating_chan_freq);
		ret_val = scnprintf(buf, PAGE_SIZE, "%s    getchannel:%d\n",
				    net_dev->name, chan_num);
	}

exit:
	hdd_exit();
	return ret_val;
}

static ssize_t show_channel_number(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __show_channel_number(net_dev, buf);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(channel, 0444, show_channel_number, NULL);

void hdd_sysfs_channel_interface_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_channel);
	if (error)
		hdd_err("Could not create channel sysfs file");
}

void hdd_sysfs_channel_interface_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_channel);
}
