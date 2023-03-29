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
 * DOC: wlan_hdd_rssi_monitor.c
 *
 * WLAN rssi monitoring functions
 *
 */

#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <wlan_hdd_ext_scan.h>
#include <wlan_hdd_rssi_monitor.h>

/*
 * define short names for the global vendor params
 * used by __wlan_hdd_cfg80211_monitor_rssi()
 */
#define PARAM_MAX QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX
#define PARAM_REQUEST_ID QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_REQUEST_ID
#define PARAM_CONTROL QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CONTROL
#define PARAM_MIN_RSSI QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MIN_RSSI
#define PARAM_MAX_RSSI QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX_RSSI

const struct nla_policy moitor_rssi_policy[
			QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_REQUEST_ID] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CONTROL] = { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MIN_RSSI] = { .type = NLA_S8 },
	[QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX_RSSI] = { .type = NLA_S8 },
};

/**
 * __wlan_hdd_cfg80211_monitor_rssi() - monitor rssi
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int
__wlan_hdd_cfg80211_monitor_rssi(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data,
				 int data_len)
{
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[PARAM_MAX + 1];
	struct rssi_monitor_param req;
	QDF_STATUS status;
	int ret;
	uint32_t control;
	mac_handle_t mac_handle;

	hdd_enter_dev(dev);

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (!hdd_conn_is_connected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
		hdd_err("Not in Connected state!");
		return -ENOTSUPP;
	}

	if (wlan_cfg80211_nla_parse(tb, PARAM_MAX, data, data_len,
				    moitor_rssi_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[PARAM_REQUEST_ID]) {
		hdd_err("attr request id failed");
		return -EINVAL;
	}

	if (!tb[PARAM_CONTROL]) {
		hdd_err("attr control failed");
		return -EINVAL;
	}

	req.request_id = nla_get_u32(tb[PARAM_REQUEST_ID]);
	req.vdev_id = adapter->vdev_id;
	control = nla_get_u32(tb[PARAM_CONTROL]);

	if (control == QCA_WLAN_RSSI_MONITORING_START) {
		req.control = true;
		if (!tb[PARAM_MIN_RSSI]) {
			hdd_err("attr min rssi failed");
			return -EINVAL;
		}

		if (!tb[PARAM_MAX_RSSI]) {
			hdd_err("attr max rssi failed");
			return -EINVAL;
		}

		req.min_rssi = nla_get_s8(tb[PARAM_MIN_RSSI]);
		req.max_rssi = nla_get_s8(tb[PARAM_MAX_RSSI]);

		if (!(req.min_rssi < req.max_rssi)) {
			hdd_warn("min_rssi: %d must be less than max_rssi: %d",
				 req.min_rssi, req.max_rssi);
			return -EINVAL;
		}
		hdd_debug("Min_rssi: %d Max_rssi: %d",
			  req.min_rssi, req.max_rssi);

	} else if (control == QCA_WLAN_RSSI_MONITORING_STOP) {
		req.control = false;
	} else {
		hdd_err("Invalid control cmd: %d", control);
		return -EINVAL;
	}
	hdd_debug("Request Id: %u vdev id: %d Control: %d",
		  req.request_id, req.vdev_id, req.control);

	mac_handle = hdd_ctx->mac_handle;
	status = sme_set_rssi_monitoring(mac_handle, &req);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_set_rssi_monitoring failed(err=%d)", status);
		return -EINVAL;
	}

	return 0;
}

/*
 * done with short names for the global vendor params
 * used by __wlan_hdd_cfg80211_monitor_rssi()
 */
#undef PARAM_MAX
#undef PARAM_CONTROL
#undef PARAM_REQUEST_ID
#undef PARAM_MAX_RSSI
#undef PARAM_MIN_RSSI

int
wlan_hdd_cfg80211_monitor_rssi(struct wiphy *wiphy, struct wireless_dev *wdev,
			       const void *data, int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_monitor_rssi(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

void hdd_rssi_threshold_breached(hdd_handle_t hdd_handle,
				 struct rssi_breach_event *data)
{
	struct hdd_context *hdd_ctx  = hdd_handle_to_context(hdd_handle);
	struct sk_buff *skb;

	hdd_enter();

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	skb = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
				  NULL,
				  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
				  QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI_INDEX,
				  GFP_KERNEL);

	if (!skb) {
		hdd_err("mem alloc failed");
		return;
	}

	hdd_debug("Req Id: %u Current rssi: %d",
		  data->request_id, data->curr_rssi);
	hdd_debug("Current BSSID: "QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(data->curr_bssid.bytes));

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_REQUEST_ID,
			data->request_id) ||
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CUR_BSSID,
		    sizeof(data->curr_bssid), data->curr_bssid.bytes) ||
	    nla_put_s8(skb, QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_CUR_RSSI,
		       data->curr_rssi)) {
		hdd_err("nla put fail");
		goto fail;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
	return;

fail:
	kfree_skb(skb);
}

