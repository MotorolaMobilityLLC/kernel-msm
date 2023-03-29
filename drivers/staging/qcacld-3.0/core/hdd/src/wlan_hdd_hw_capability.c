/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_hw_capability.c
 *
 * The implementation of get hw capability
 *
 */

#include "wlan_hdd_main.h"
#include "wmi_unified_param.h"
#include "wlan_hdd_hw_capability.h"
#include "qca_vendor.h"
#include "wlan_osif_request_manager.h"
#include "osif_sync.h"

/**
 * hdd_get_isolation_cb - Callback function to get isolation information
 * @context: opaque context originally passed to SME. HDD always passes
 * a cookie for the request context
 * @isolation: pointer of isolation information
 *
 * This function will fill isolation information to isolation priv adapter
 *
 * Return: None
 */
static void hdd_get_isolation_cb(struct sir_isolation_resp *isolation,
				 void *context)
{
	struct osif_request *request;
	struct sir_isolation_resp *priv;

	if (!isolation) {
		hdd_err("Bad param");
		return;
	}

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	priv->isolation_chain0 = isolation->isolation_chain0;
	priv->isolation_chain1 = isolation->isolation_chain1;
	priv->isolation_chain2 = isolation->isolation_chain2;
	priv->isolation_chain3 = isolation->isolation_chain3;

	osif_request_complete(request);
	osif_request_put(request);
}

/**
 * hdd_post_isolation - send rsp to user space
 * @hdd_ctx: pointer to hdd context
 * @isolation: antenna isolation information
 *
 * Return: 0 for success, non-zero for failure
 */
static int hdd_post_isolation(struct hdd_context *hdd_ctx,
			      struct sir_isolation_resp *isolation)
{
	struct sk_buff *skb;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
						  (sizeof(u8) + NLA_HDRLEN) +
						  (sizeof(u8) + NLA_HDRLEN) +
						  (sizeof(u8) + NLA_HDRLEN) +
						  (sizeof(u8) + NLA_HDRLEN) +
						  NLMSG_HDRLEN);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return -ENOMEM;
	}

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_ANTENNA_ISOLATION,
		       isolation->isolation_chain0)) {
		hdd_err("put fail");
		goto nla_put_failure;
	}

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_ANTENNA_ISOLATION,
		       isolation->isolation_chain1)) {
		hdd_err("put fail");
		goto nla_put_failure;
	}

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_ANTENNA_ISOLATION,
		       isolation->isolation_chain2)) {
		hdd_err("put fail");
		goto nla_put_failure;
	}

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_ANTENNA_ISOLATION,
		       isolation->isolation_chain3)) {
		hdd_err("put fail");
		goto nla_put_failure;
	}

	cfg80211_vendor_cmd_reply(skb);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -EINVAL;
}

/**
 * __wlan_hdd_cfg80211_get_hw_capability() - get hw capability
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Return: 0 on success; error number otherwise.
 *
 */
static int __wlan_hdd_cfg80211_get_hw_capability(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct osif_request *request;
	struct sir_isolation_resp *priv;
	mac_handle_t mac_handle;
	void *cookie;
	QDF_STATUS status;
	int ret;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_ANTENNA_ISOLATION,
	};

	hdd_enter();

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -ENOMEM;
	}

	cookie = osif_request_cookie(request);

	mac_handle = hdd_ctx->mac_handle;
	status = sme_get_isolation(mac_handle,
				   cookie,
				   hdd_get_isolation_cb);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to retrieve isolation");
		ret = -EFAULT;
	} else {
		ret = osif_request_wait_for_response(request);
		if (ret) {
			hdd_err("SME timed out while retrieving isolation");
			ret = -ETIMEDOUT;
		} else {
			priv = osif_request_priv(request);
			hdd_post_isolation(hdd_ctx, priv);
			ret = 0;
		}
	}
	osif_request_put(request);

	return ret;
}

int wlan_hdd_cfg80211_get_hw_capability(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_get_hw_capability(wiphy, wdev,
						      data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
