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
 * DOC: wlan_hdd_sysfs_reassoc.c
 *
 * implementation for creating sysfs file reassoc
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include "wlan_hdd_sysfs_reassoc.h"

static ssize_t
__wlan_hdd_store_reassoc_sysfs(struct net_device *net_dev, char const *buf,
			       size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	tCsrRoamModifyProfileFields mod_fields;
	uint32_t roam_id = INVALID_ROAM_ID;
	struct hdd_context *hdd_ctx;
	mac_handle_t mac_handle;
	uint32_t operating_ch;
	tSirMacAddr bssid;
	int ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	mac_handle = hdd_ctx->mac_handle;
	operating_ch = wlan_reg_freq_to_chan(hdd_ctx->pdev,
				adapter->session.station.conn_info.chan_freq);

	hdd_debug("reassoc: net_devname %s", net_dev->name);

	sme_get_modify_profile_fields(mac_handle, adapter->vdev_id,
				      &mod_fields);

	if (roaming_offload_enabled(hdd_ctx)) {
		qdf_mem_copy(bssid,
			     &adapter->session.station.conn_info.bssid,
			     sizeof(bssid));
		hdd_wma_send_fastreassoc_cmd(adapter,
					     bssid, operating_ch);
	} else {
		sme_roam_reassoc(mac_handle, adapter->vdev_id,
				 NULL, mod_fields, &roam_id, 1);
	}

	return count;
}

static ssize_t wlan_hdd_store_reassoc_sysfs(struct device *dev,
					    struct device_attribute *attr,
					    char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t err_size;

	err_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (err_size)
		return err_size;

	err_size = __wlan_hdd_store_reassoc_sysfs(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return err_size;
}

static DEVICE_ATTR(reassoc, 0220,
		   NULL, wlan_hdd_store_reassoc_sysfs);

int hdd_sysfs_reassoc_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_reassoc);
	if (error)
		hdd_err("could not create reassoc sysfs file");

	return error;
}

void hdd_sysfs_reassoc_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_reassoc);
}
