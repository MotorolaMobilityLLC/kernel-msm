/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: defines driver functions interfacing with linux kernel
 */

#include <qdf_list.h>
#include <qdf_status.h>
#include <linux/wireless.h>
#include <linux/netdevice.h>
#include <wlan_cfg80211.h>
#include <wlan_osif_priv.h>
#include <wlan_interop_issues_ap_ucfg_api.h>
#include <wlan_cfg80211_interop_issues_ap.h>
#include <osif_psoc_sync.h>
#include <qdf_mem.h>
#include <wlan_utility.h>
#include "wlan_hdd_main.h"
#include "cfg_ucfg_api.h"
#include "wlan_hdd_object_manager.h"

const struct nla_policy
interop_issues_ap_policy[QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_TYPE] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t) },
	[QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_LIST] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t) },
	[QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_BSSID] =
						VENDOR_NLA_POLICY_MAC_ADDR,
};

/**
 * wlan_cfg80211_send_interop_issues_ap_cb() - report information
 * @data: interop issues ap mac received from fw
 *
 * Generate a wlan interop issues ap info package and send it to user
 * space daemon through netlink.
 *
 * Return: none
 */
static void
wlan_cfg80211_send_interop_issues_ap_cb(
				struct wlan_interop_issues_ap_event *data)
{
	struct wlan_objmgr_pdev *pdev;
	struct pdev_osif_priv *os_priv;
	struct sk_buff *skb;
	uint32_t index, len;

	if (!data) {
		osif_err("Invalid result.");
		return;
	}

	pdev = data->pdev;
	if (!pdev) {
		osif_err("pdev is null.");
		return;
	}
	os_priv = wlan_pdev_get_ospriv(pdev);
	if (!os_priv) {
		osif_err("os_priv is null.");
		return;
	}

	index = QCA_NL80211_VENDOR_SUBCMD_INTEROP_ISSUES_AP_INDEX;
	len = nla_total_size(QDF_MAC_ADDR_SIZE + NLMSG_HDRLEN);
	skb = cfg80211_vendor_event_alloc(os_priv->wiphy, NULL, len, index,
					  GFP_KERNEL);
	if (!skb) {
		osif_err("skb alloc failed");
		return;
	}

	osif_debug("interop issues ap mac:" QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(data->rap_addr.bytes));

	if (nla_put(skb,
		    QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_BSSID,
		    QDF_MAC_ADDR_SIZE, data->rap_addr.bytes)) {
		osif_err("nla put fail");
		kfree_skb(skb);
		return;
	}

	cfg80211_vendor_event(skb, GFP_KERNEL);
}

static void wlan_interop_issues_ap_register_cbk(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_interop_issues_ap_callbacks cb;

	cb.os_if_interop_issues_ap_event_handler =
					wlan_cfg80211_send_interop_issues_ap_cb;
	ucfg_register_interop_issues_ap_callback(pdev, &cb);
}

/**
 * wlan_parse_interop_issues_ap() - parse the interop issues ap info
 * @interop_issues_ap: the pointer of interop issues ap
 * @attr: list of attributes
 *
 * Return: 0 on success; error number on failure
 */
static int
wlan_parse_interop_issues_ap(struct qdf_mac_addr *interop_issues_ap,
			     struct nlattr *attr)
{
	struct nlattr *tb2[QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_MAX + 1];
	struct nlattr *curr_attr = NULL;
	uint32_t rem;
	qdf_size_t i = 0;

	nla_for_each_nested(curr_attr, attr, rem) {
		if (i == MAX_INTEROP_ISSUES_AP_NUM) {
			osif_err("Ignoring excess");
			break;
		}

		if (wlan_cfg80211_nla_parse(tb2,
				QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_MAX,
				nla_data(curr_attr),
				nla_len(curr_attr),
				interop_issues_ap_policy)) {
			osif_err("nla_parse failed");
			return -EINVAL;
		}
		if (!tb2[QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_BSSID]) {
			osif_err("attr addr failed");
			return -EINVAL;
		}
		nla_memcpy(interop_issues_ap[i].bytes,
			   tb2[QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_BSSID],
			   QDF_MAC_ADDR_SIZE);
		osif_debug(QDF_MAC_ADDR_FMT,
			   QDF_MAC_ADDR_REF(interop_issues_ap[i].bytes));
		i++;
	}

	return i;
}

/**
 * __wlan_cfg80211_set_interop_issues_ap_config() - set config status
 * @wiphy: WIPHY structure pointer
 * @wdev: Wireless device structure pointer
 * @data: Pointer to the data received
 * @data_len: Length of the data received
 *
 * Return: 0 on success and errno on failure
 */
static int
__wlan_cfg80211_set_interop_issues_ap_config(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_MAX + 1];
	struct nlattr *attr;
	uint32_t count = 0;
	struct wlan_interop_issues_ap_info interop_issues_ap = {0};
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		osif_err("Invalid vdev");
		return -EINVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	hdd_objmgr_put_vdev(vdev);
	if (!psoc) {
		osif_err("Invalid psoc");
		return -EINVAL;
	}

	if (wlan_cfg80211_nla_parse(tb,
				    QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_MAX,
				    data, data_len,
				    interop_issues_ap_policy)) {
		osif_err("Invalid ATTR");
		return -EINVAL;
	}

	attr = tb[QCA_WLAN_VENDOR_ATTR_INTEROP_ISSUES_AP_LIST];
	if (attr) {
		count =
		     wlan_parse_interop_issues_ap(interop_issues_ap.rap_items,
						  attr);
		if (count < 0)
			return -EINVAL;
	}

	osif_debug("Num of interop issues ap: %d", count);
	interop_issues_ap.count = count;
	interop_issues_ap.detect_enable = true;

	/*
	 * need to figure out a converged way of obtaining the vdev for
	 * a given netdev that doesn't involve the legacy mechanism.
	 */
	ucfg_set_interop_issues_ap_config(psoc, &interop_issues_ap);

	return 0;
}

int wlan_cfg80211_set_interop_issues_ap_config(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data, int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int ret;

	ret = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (ret)
		return ret;

	ret = __wlan_cfg80211_set_interop_issues_ap_config(wiphy, wdev,
							   data, data_len);
	osif_psoc_sync_op_stop(psoc_sync);

	return ret;
}

void wlan_cfg80211_init_interop_issues_ap(struct wlan_objmgr_pdev *pdev)
{
	/*
	 * the special mac is used to trigger uplayer sets
	 * interop issues ap list to fw when driver reloads but
	 * cnss-daemon does not restart.
	 */
	uint8_t fmac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	struct wlan_interop_issues_ap_info interop_issues_ap = {0};
	struct wlan_interop_issues_ap_event data;
	struct wlan_objmgr_psoc *psoc;

	wlan_interop_issues_ap_register_cbk(pdev);

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		osif_err("Invalid psoc");
		return;
	}
	interop_issues_ap.detect_enable = true;
	ucfg_set_interop_issues_ap_config(psoc, &interop_issues_ap);

	data.pdev = pdev;
	qdf_mem_copy(data.rap_addr.bytes, fmac, QDF_MAC_ADDR_SIZE);

	wlan_cfg80211_send_interop_issues_ap_cb(&data);
}
