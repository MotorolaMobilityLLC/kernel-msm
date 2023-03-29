/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_tx_power.c
 *
 * WLAN tx power setting functions
 *
 */

#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <wma_api.h>
#include <wlan_hdd_tx_power.h>

#define MAX_TXPOWER_SCALE 4

const struct nla_policy
txpower_scale_policy[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE] = { .type = NLA_U8 },
};

/**
 * __wlan_hdd_cfg80211_txpower_scale () - txpower scaling
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_txpower_scale(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	int ret;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX + 1];
	uint8_t scale_value;
	QDF_STATUS status;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX,
				    data, data_len, txpower_scale_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE]) {
		hdd_err("attr tx power scale failed");
		return -EINVAL;
	}

	scale_value = nla_get_u8(tb
		    [QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE]);

	if (scale_value > MAX_TXPOWER_SCALE) {
		hdd_err("Invalid tx power scale level");
		return -EINVAL;
	}

	status = wma_set_tx_power_scale(adapter->vdev_id, scale_value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Set tx power scale failed");
		return -EINVAL;
	}

	return 0;
}

int wlan_hdd_cfg80211_txpower_scale(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data,
				    int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_txpower_scale(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

const struct nla_policy txpower_scale_decr_db_policy
[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB] = { .type = NLA_U8 },
};

/**
 * __wlan_hdd_cfg80211_txpower_scale_decr_db () - txpower scaling
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_txpower_scale_decr_db(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data,
					  int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	int ret;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB_MAX + 1];
	uint8_t scale_value;
	QDF_STATUS status;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	if (wlan_cfg80211_nla_parse(tb,
				QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB_MAX,
				data, data_len,
				txpower_scale_decr_db_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB]) {
		hdd_err("attr tx power decrease db value failed");
		return -EINVAL;
	}

	scale_value = nla_get_u8(tb
		    [QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB]);

	status = wma_set_tx_power_scale_decr_db(adapter->vdev_id,
						scale_value);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Set tx power decrease db failed");
		return -EINVAL;
	}

	return 0;
}

int wlan_hdd_cfg80211_txpower_scale_decr_db(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_txpower_scale_decr_db(wiphy, wdev,
							  data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

