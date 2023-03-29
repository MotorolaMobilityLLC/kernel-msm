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
 * DOC: wlan_hdd_sysfs_txrx_stats.c
 *
 * implementation for creating sysfs file txrx_stats
 */

#include <wlan_hdd_includes.h>
#include <wlan_hdd_main.h>
#include "osif_vdev_sync.h"
#include <wlan_hdd_sysfs.h>
#include <wlan_hdd_sysfs_txrx_stats.h>
#include "cds_api.h"
#include "cdp_txrx_cmn_struct.h"
#include "cdp_txrx_cmn.h"

static ssize_t
__hdd_sysfs_txrx_stats_store(struct net_device *net_dev,
			     char const *buf, size_t count)
{
	struct hdd_adapter *adapter = netdev_priv(net_dev);
	char buf_local[MAX_SYSFS_USER_COMMAND_SIZE_LENGTH + 1];
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct cdp_txrx_stats_req req = {0};
	struct hdd_station_ctx *sta_ctx;
	struct hdd_context *hdd_ctx;
	char *sptr, *token;
	uint32_t val1;
	uint8_t val2;
	int ret;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return -EINVAL;

	ret = hdd_sysfs_validate_and_copy_buf(buf_local, sizeof(buf_local),
					      buf, count);

	if (ret) {
		hdd_err_rl("invalid input");
		return ret;
	}

	sptr = buf_local;
	hdd_debug("txrx_stats: count %zu buf_local:(%s) net_devname %s",
		  count, buf_local, net_dev->name);

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
	if (kstrtou8(token, 0, &val2))
		return -EINVAL;

	req.stats = val1;
	/* default value of secondary parameter is 0(mac_id) */
	req.mac_id = val2;

	hdd_debug("WE_SET_TXRX_STATS stats cmd: %d mac_id: %d",
		  req.stats, req.mac_id);
	if (qdf_unlikely(!soc)) {
		hdd_err("soc is NULL");
		return -EINVAL;
	}

	if (val1 == CDP_TXRX_STATS_28) {
		if (sta_ctx->conn_info.is_authenticated) {
			hdd_debug("ap mac addr: %pM",
				  (void *)&sta_ctx->conn_info.bssid);
			req.peer_addr =
				(char *)&sta_ctx->conn_info.bssid;
		}
	}
	ret = cdp_txrx_stats_request(soc, adapter->vdev_id, &req);

	if (ret) {
		hdd_err_rl("failed to set txrx stats: %d", ret);
		return ret;
	}

	return count;
}

static ssize_t
hdd_sysfs_txrx_stats_store(struct device *dev,
			   struct device_attribute *attr,
			   char const *buf, size_t count)
{
	struct net_device *net_dev = container_of(dev, struct net_device, dev);
	struct osif_vdev_sync *vdev_sync;
	ssize_t errno_size;

	errno_size = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno_size)
		return errno_size;

	errno_size = __hdd_sysfs_txrx_stats_store(net_dev, buf, count);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno_size;
}

static DEVICE_ATTR(txrx_stats, 0220,
		   NULL, hdd_sysfs_txrx_stats_store);

int hdd_sysfs_txrx_stats_create(struct hdd_adapter *adapter)
{
	int error;

	error = device_create_file(&adapter->dev->dev, &dev_attr_txrx_stats);
	if (error)
		hdd_err("could not create txrx_stats sysfs file");

	return error;
}

void hdd_sysfs_txrx_stats_destroy(struct hdd_adapter *adapter)
{
	device_remove_file(&adapter->dev->dev, &dev_attr_txrx_stats);
}
