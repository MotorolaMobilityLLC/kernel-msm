/*
 * Copyright (c) 2015-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_subnet_detect.c
 *
 * WLAN Host Device Driver subnet detect API implementation
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/cfg80211.h>
#include <ani_global.h>
#include "sme_api.h"
#include "osif_sync.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_subnet_detect.h"
#include <qca_vendor.h>

/*
 * define short names for the global vendor params
 * used by __wlan_hdd_cfg80211_set_gateway_params()
 */
#define PARAM_MAC_ADDR QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_GW_MAC_ADDR
#define PARAM_IPV4_ADDR QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_IPV4_ADDR
#define PARAM_IPV6_ADDR QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_IPV6_ADDR

const struct nla_policy subnet_detect_policy[
			QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_MAX + 1] = {
		[QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_GW_MAC_ADDR] =
				VENDOR_NLA_POLICY_MAC_ADDR,
		[QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_IPV4_ADDR] = {
				.type = NLA_EXACT_LEN,
				.len = QDF_IPV4_ADDR_SIZE
		},
		[QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_IPV6_ADDR] = {
				.type = NLA_EXACT_LEN,
				.len = QDF_IPV6_ADDR_SIZE
		},
};

/**
 * __wlan_hdd_cfg80211_set_gateway_params() - set gateway params
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_set_gateway_params(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_MAX + 1];
	struct gateway_update_req_param req = { 0 };
	int ret;
	QDF_STATUS status;
	bool subnet_detection_enabled;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	/* user may have disabled the feature in INI */
	ucfg_mlme_is_subnet_detection_enabled(hdd_ctx->psoc,
					      &subnet_detection_enabled);
	if (!subnet_detection_enabled) {
		hdd_debug("LFR Subnet Detection disabled in INI");
		return -ENOTSUPP;
	}

	/* The gateway parameters are only valid in the STA persona
	 * and only in the connected state.
	 */
	if (QDF_STA_MODE != adapter->device_mode) {
		hdd_debug("Received GW param update for non-STA mode adapter");
		return -ENOTSUPP;
	}

	if (!hdd_conn_is_connected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
		hdd_debug("Received GW param update in disconnected state!");
		return -ENOTSUPP;
	}

	/* Extract NL parameters
	 * mac_addr:  6 bytes
	 * ipv4 addr: 4 bytes
	 * ipv6 addr: 16 bytes
	 */
	if (wlan_cfg80211_nla_parse(tb,
				    QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_MAX,
				    data, data_len, subnet_detect_policy)) {
		hdd_err("Invalid ATTR list");
		return -EINVAL;
	}

	if (!tb[PARAM_MAC_ADDR]) {
		hdd_err("request mac addr failed");
		return -EINVAL;
	}
	nla_memcpy(req.gw_mac_addr.bytes, tb[PARAM_MAC_ADDR],
			QDF_MAC_ADDR_SIZE);

	/* req ipv4_addr_type and ipv6_addr_type are initially false due
	 * to zeroing the struct
	 */
	if (tb[PARAM_IPV4_ADDR]) {
		nla_memcpy(req.ipv4_addr, tb[PARAM_IPV4_ADDR],
			QDF_IPV4_ADDR_SIZE);
		req.ipv4_addr_type = true;
	}

	if (tb[PARAM_IPV6_ADDR]) {
		nla_memcpy(&req.ipv6_addr, tb[PARAM_IPV6_ADDR],
			QDF_IPV6_ADDR_SIZE);
		req.ipv6_addr_type = true;
	}

	if (!req.ipv4_addr_type && !req.ipv6_addr_type) {
		hdd_err("invalid ipv4 or ipv6 gateway address");
		return -EINVAL;
	}

	req.max_retries = 3;
	req.timeout = 100;   /* in milliseconds */
	req.vdev_id = adapter->vdev_id;

	hdd_debug("Configuring gateway for session %d", req.vdev_id);
	hdd_debug("mac:"QDF_MAC_ADDR_FMT", ipv4:%pI4 (type %d), ipv6:%pI6c (type %d)",
		  QDF_MAC_ADDR_REF(req.gw_mac_addr.bytes),
		  req.ipv4_addr, req.ipv4_addr_type,
		  req.ipv6_addr, req.ipv6_addr_type);

	hdd_nud_set_gateway_addr(adapter, req.gw_mac_addr);

	status = sme_gateway_param_update(hdd_ctx->mac_handle, &req);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_gateway_param_update failed(err=%d)", status);
		ret = -EINVAL;
	}

	hdd_exit();
	return ret;
}

/**
 * wlan_hdd_cfg80211_set_gateway_params() - set gateway parameters
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * The API is invoked by the user space to set the gateway parameters
 * such as mac address and the IP address which is used for detecting
 * the IP subnet change
 *
 * Return: 0 on success; errno on failure
 */
int wlan_hdd_cfg80211_set_gateway_params(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_gateway_params(wiphy, wdev,
						       data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
#undef PARAM_MAC_ADDR
#undef PARAM_IPV4_ADDR
#undef PARAM_IPV6_ADDR
