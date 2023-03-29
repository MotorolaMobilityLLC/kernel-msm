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
 * DOC: wlan_hdd_fw_state.c
 *
 * The implementation for getting firmware state
 */

#include "osif_sync.h"
#include "qca_vendor.h"
#include "wlan_hdd_fw_state.h"
#include "wlan_hdd_main.h"
#include "wlan_osif_request_manager.h"
#include "wmi_unified_param.h"

struct fw_state {
	bool fw_active;
};

/**
 * hdd_get_fw_state_cb() - Callback function to get fw state
 * @context: opaque context originally passed to SME. HDD always passes
 * a cookie for the request context
 *
 * This function receives the response/data from the lower layer and
 * checks to see if the thread is still waiting then post the results to
 * upper layer, if the request has timed out then ignore.
 *
 * Return: None
 */
static void hdd_get_fw_state_cb(void *context)
{
	struct osif_request *request;
	struct fw_state *priv;

	hdd_enter();

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	priv->fw_active = true;
	osif_request_complete(request);
	osif_request_put(request);
}

/**
 * hdd_post_get_fw_state_rsp - send rsp to user space
 * @hdd_ctx: pointer to hdd context
 * @state: true for fw active, false for fw error state
 *
 * Return: 0 for success, non-zero for failure
 */
static int hdd_post_get_fw_state_rsp(struct hdd_context *hdd_ctx,
				     bool state)
{
	struct sk_buff *skb;
	enum qca_wlan_vendor_attr_fw_state fw_state;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
						  sizeof(uint8_t) +
						  NLA_HDRLEN +
						  NLMSG_HDRLEN);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return -ENOMEM;
	}

	if (state)
		fw_state = QCA_WLAN_VENDOR_ATTR_FW_STATE_ACTIVE;
	else
		fw_state = QCA_WLAN_VENDOR_ATTR_FW_STATE_ERROR;

	if (nla_put_u8(skb, QCA_WLAN_VENDOR_ATTR_FW_STATE,
		       (uint8_t)fw_state)) {
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
 * __wlan_hdd_cfg80211_get_fw_state() - get fw state
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This function sends a request to fw and waits on a timer to
 * invoke the callback. if the callback is invoked then true
 * will be returned or otherwise fail status will be returned.
 *
 * Return: 0 on success; error number otherwise.
 **/
static int __wlan_hdd_cfg80211_get_fw_state(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	mac_handle_t mac_handle;
	QDF_STATUS status;
	int retval;
	void *cookie;
	struct osif_request *request;
	struct fw_state *priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_STATS,
	};
	bool state = false;

	hdd_enter_dev(wdev->netdev);

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (retval)
		return retval;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -ENOMEM;
	}
	cookie = osif_request_cookie(request);

	mac_handle = hdd_ctx->mac_handle;
	status = sme_get_fw_state(mac_handle,
				  hdd_get_fw_state_cb,
				  cookie);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Unable to get fw state");
		retval = qdf_status_to_os_return(status);
	} else {
		retval = osif_request_wait_for_response(request);
		if (retval) {
			hdd_err("Target response timed out");
			state = false;
		} else {
			priv = osif_request_priv(request);
			state = priv->fw_active;
		}
	}
	retval = hdd_post_get_fw_state_rsp(hdd_ctx, state);
	if (retval)
		hdd_err("Failed to post fw state");

	osif_request_put(request);

	hdd_exit();
	return retval;
}

/**
 * wlan_hdd_cfg80211_get_fw_status() - get fw state
 * @wiphy: wiphy pointer
 * @wdev: pointer to struct wireless_dev
 * @data: pointer to incoming NL vendor data
 * @data_len: length of @data
 *
 * Return: 0 on success; error number otherwise.
 */
int wlan_hdd_cfg80211_get_fw_state(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_get_fw_state(wiphy, wdev, data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}
