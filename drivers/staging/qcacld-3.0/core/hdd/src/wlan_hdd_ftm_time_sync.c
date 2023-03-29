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

#include "wlan_hdd_ftm_time_sync.h"
#include "ftm_time_sync_ucfg_api.h"
#include "wlan_hdd_object_manager.h"

static ssize_t hdd_ftm_time_sync_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct hdd_station_ctx *hdd_sta_ctx;
	struct hdd_adapter *adapter;
	struct wlan_objmgr_vdev *vdev;
	ssize_t size = 0;

	struct net_device *net_dev = qdf_container_of(dev, struct net_device,
						      dev);

	adapter = (struct hdd_adapter *)(netdev_priv(net_dev));
	if (adapter->magic != WLAN_HDD_ADAPTER_MAGIC)
		return scnprintf(buf, PAGE_SIZE, "Invalid device\n");

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return -EINVAL;

	if (adapter->device_mode == QDF_STA_MODE)
		size = ucfg_ftm_time_sync_show(vdev, buf);

	hdd_objmgr_put_vdev(vdev);
	return size;
}

static DEVICE_ATTR(ftm_time_sync, 0400, hdd_ftm_time_sync_show, NULL);

void
hdd_ftm_time_sync_sta_state_notify(struct hdd_adapter *adapter,
				   enum ftm_time_sync_sta_state state)
{
	struct hdd_station_ctx *hdd_sta_ctx;
	struct wlan_objmgr_psoc *psoc;
	struct net_device *net_dev;
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc)
		goto out;

	if (!ucfg_is_ftm_time_sync_enable(psoc))
		goto out;

	net_dev = adapter->dev;

	if (net_dev) {
		if (state == FTM_TIME_SYNC_STA_CONNECTED)
			device_create_file(&net_dev->dev,
					   &dev_attr_ftm_time_sync);
		else
			device_remove_file(&net_dev->dev,
					   &dev_attr_ftm_time_sync);
	}

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	ucfg_ftm_time_sync_update_sta_connect_state(
						vdev, state,
						hdd_sta_ctx->conn_info.bssid);
out:
	hdd_objmgr_put_vdev(vdev);
}
