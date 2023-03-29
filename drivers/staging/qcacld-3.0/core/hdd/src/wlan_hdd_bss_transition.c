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
 * DOC: wlan_hdd_bss_transition.c
 *
 * WLAN bss transition functions
 *
 */

#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <wlan_hdd_bss_transition.h>

/**
 * wlan_hdd_is_bt_in_progress() - check if bt activity is in progress
 * @hdd_ctx : HDD context
 *
 * Return: true if BT activity is in progress else false
 */
static bool wlan_hdd_is_bt_in_progress(struct hdd_context *hdd_ctx)
{
	if (hdd_ctx->bt_a2dp_active || hdd_ctx->bt_vo_active)
		return true;

	return false;
}

/**
 * wlan_hdd_fill_btm_resp() - Fill bss candidate response buffer
 * @reply_skb : pointer to reply_skb
 * @info : bss candidate information
 * @index : attribute type index for nla_next_start()
 *
 * Return : 0 on success and errno on failure
 */
static int wlan_hdd_fill_btm_resp(struct sk_buff *reply_skb,
				  struct bss_candidate_info *info,
				  int index)
{
	struct nlattr *attr;

	attr = nla_nest_start(reply_skb, index);
	if (!attr) {
		hdd_err("nla_nest_start failed");
		kfree_skb(reply_skb);
		return -EINVAL;
	}

	if (nla_put(reply_skb,
		  QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID,
		  ETH_ALEN, info->bssid.bytes) ||
	    nla_put_u32(reply_skb,
		 QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_STATUS,
		 info->status)) {
		hdd_err("nla_put failed");
		kfree_skb(reply_skb);
		return -EINVAL;
	}

	nla_nest_end(reply_skb, attr);

	return 0;
}

const struct nla_policy
btm_params_policy[QCA_WLAN_VENDOR_ATTR_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_BTM_MBO_TRANSITION_REASON] = {
						.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO] =
			VENDOR_NLA_POLICY_NESTED(btm_cand_list_policy),
};

const struct nla_policy
btm_cand_list_policy[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_MAX + 1]
	= {[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID] = {
					.len = QDF_MAC_ADDR_SIZE},
	   [QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_STATUS] = {
						.type = NLA_U32},
};

/**
 * __wlan_hdd_cfg80211_fetch_bss_transition_status() - fetch bss transition
 * status
 * @wiphy : WIPHY structure pointer
 * @wdev : Wireless device structure pointer
 * @data : Pointer to the data received
 * @data_len : Length of the data received
 *
 * This function is used to fetch transition status for candidate bss. The
 * transition status is either accept or reason for reject.
 *
 * Return : 0 on success and errno on failure
 */
static int
__wlan_hdd_cfg80211_fetch_bss_transition_status(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_MAX + 1];
	struct nlattr *tb_msg[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_MAX + 1];
	uint8_t transition_reason;
	struct nlattr *attr;
	struct sk_buff *reply_skb;
	int rem, j;
	int ret;
	bool is_bt_in_progress;
	struct bss_candidate_info candidate_info[MAX_CANDIDATE_INFO];
	uint16_t nof_candidates, i = 0;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_station_ctx *hdd_sta_ctx =
					WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	mac_handle_t mac_handle;
	QDF_STATUS status;

	hdd_enter();

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (adapter->device_mode != QDF_STA_MODE ||
	    hdd_sta_ctx->conn_info.conn_state != eConnectionState_Associated) {
		hdd_err("Command is either not invoked for STA mode (device mode: %d) or STA is not associated (Connection state: %d)",
			adapter->device_mode, hdd_sta_ctx->conn_info.conn_state);
		return -EINVAL;
	}

	ret = wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_MAX, data,
				      data_len, btm_params_policy);
	if (ret) {
		hdd_err("Attribute parse failed");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_BTM_MBO_TRANSITION_REASON] ||
	    !tb[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO]) {
		hdd_err("Missing attributes");
		return -EINVAL;
	}

	transition_reason = nla_get_u8(
			    tb[QCA_WLAN_VENDOR_ATTR_BTM_MBO_TRANSITION_REASON]);

	nla_for_each_nested(attr,
			    tb[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO],
			    rem) {
		ret = wlan_cfg80211_nla_parse_nested(tb_msg,
				    QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_MAX,
				    attr, btm_cand_list_policy);
		if (ret) {
			hdd_err("Attribute parse failed");
			return -EINVAL;
		}

		if (!tb_msg[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID]) {
			hdd_err("Missing BSSID attribute");
			return -EINVAL;
		}

		qdf_mem_copy((void *)candidate_info[i].bssid.bytes,
			     nla_data(tb_msg[
			     QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_BSSID]),
			     QDF_MAC_ADDR_SIZE);
		i++;
		if (i == MAX_CANDIDATE_INFO)
			break;
	}

	/*
	 * Determine status for each candidate and fill in the status field.
	 * Also arrange the candidates in the order of preference.
	 */
	nof_candidates = i;

	is_bt_in_progress = wlan_hdd_is_bt_in_progress(hdd_ctx);

	mac_handle = hdd_ctx->mac_handle;
	status = sme_get_bss_transition_status(mac_handle, transition_reason,
					       &hdd_sta_ctx->conn_info.bssid,
					       candidate_info,
					       nof_candidates,
					       is_bt_in_progress);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	/* Prepare the reply and send it to userspace */
	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
			((QDF_MAC_ADDR_SIZE + sizeof(uint32_t)) *
			 nof_candidates) + NLMSG_HDRLEN);
	if (!reply_skb) {
		hdd_err("reply buffer alloc failed");
		return -ENOMEM;
	}

	attr = nla_nest_start(reply_skb,
			      QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO);
	if (!attr) {
		hdd_err("nla_nest_start failed");
		kfree_skb(reply_skb);
		return -EINVAL;
	}

	/*
	 * Order candidates as - accepted candidate list followed by rejected
	 * candidate list
	 */
	for (i = 0, j = 0; i < nof_candidates; i++) {
		/* copy accepted candidate list */
		if (candidate_info[i].status == QCA_STATUS_ACCEPT) {
			if (wlan_hdd_fill_btm_resp(reply_skb,
						   &candidate_info[i], j))
				return -EINVAL;
			j++;
		}
	}
	for (i = 0; i < nof_candidates; i++) {
		/* copy rejected candidate list */
		if (candidate_info[i].status != QCA_STATUS_ACCEPT) {
			if (wlan_hdd_fill_btm_resp(reply_skb,
						   &candidate_info[i], j))
				return -EINVAL;
			j++;
		}
	}
	nla_nest_end(reply_skb, attr);

	hdd_exit();

	return cfg80211_vendor_cmd_reply(reply_skb);
}

int wlan_hdd_cfg80211_fetch_bss_transition_status(struct wiphy *wiphy,
						  struct wireless_dev *wdev,
						  const void *data,
						  int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_fetch_bss_transition_status(wiphy, wdev,
								data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

