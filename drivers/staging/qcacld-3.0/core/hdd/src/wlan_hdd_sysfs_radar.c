/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sysfs_radar.c
 *
 * Implementation for creating sysfs file radar
 */

#include <wlan_hdd_includes.h>
#include "osif_vdev_sync.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_radar.h>
#include "wlan_dfs_tgt_api.h"

static ssize_t
__hdd_sysfs_radar_store(struct net_device *net_dev,
			char const *buf, size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	struct hdd_ap_ctx *ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	struct wlan_objmgr_pdev *pdev;
	struct radar_found_info radar;
	struct hdd_context *hdd_ctx;
	char *sptr, *token;
	int set_value;
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
	hdd_debug("set_radar: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

	/* Get set_value */
	token = strsep(&sptr, " ");
	if (!token)
		return -EINVAL;
	if (kstrtou32(token, 0, &set_value))
		return -EINVAL;

	hdd_debug("Set QCASAP_SET_RADAR_CMD val %d", set_value);

	pdev = hdd_ctx->pdev;
	if (!pdev) {
		hdd_err("null pdev");
		return -EINVAL;
	}

	qdf_mem_zero(&radar, sizeof(radar));
	if (policy_mgr_get_dfs_beaconing_session_id(hdd_ctx->psoc) !=
	    WLAN_UMAC_VDEV_ID_MAX)
		tgt_dfs_process_radar_ind(pdev, &radar);
	else
		hdd_debug("Ignore set radar, op ch_freq(%d) is not dfs",
			  ap_ctx->operating_chan_freq);

	return count;
}

static ssize_t
hdd_sysfs_radar_store(struct device *dev,
		      struct device_attribute *attr,
		      char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_radar_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(radar, 0220,
		   NULL, hdd_sysfs_radar_store);

int hdd_sysfs_radar_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_radar);
	if (error)
		hdd_err("could not create radar sysfs file");

	return error;
}

void hdd_sysfs_radar_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_radar);
}
