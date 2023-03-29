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
 * DOC: wlan_hdd_concurrency_matrix.c
 *
 * WLAN concurrency matrix functions
 *
 */

#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <wlan_hdd_concurrency_matrix.h>

#define CDS_MAX_FEATURE_SET   8
#define MAX_CONCURRENT_MATRIX \
	QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_MAX
#define MATRIX_CONFIG_PARAM_SET_SIZE_MAX \
	QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_CONFIG_PARAM_SET_SIZE_MAX

const struct nla_policy
wlan_hdd_get_concurrency_matrix_policy[MAX_CONCURRENT_MATRIX + 1] = {
	[MATRIX_CONFIG_PARAM_SET_SIZE_MAX] = {.type = NLA_U32},
};

/**
 * __wlan_hdd_cfg80211_get_concurrency_matrix() - to retrieve concurrency matrix
 * @wiphy: pointer phy adapter
 * @wdev: pointer to wireless device structure
 * @data: pointer to data buffer
 * @data_len: length of data
 *
 * This routine will give concurrency matrix
 *
 * Return: int status code
 */
static int
__wlan_hdd_cfg80211_get_concurrency_matrix(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	uint32_t feature_set_matrix[CDS_MAX_FEATURE_SET] = {0};
	uint8_t i, feature_sets, max_feature_sets;
	struct nlattr *tb[MAX_CONCURRENT_MATRIX + 1];
	struct sk_buff *reply_skb;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	int ret;

	hdd_enter_dev(wdev->netdev);

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (wlan_cfg80211_nla_parse(tb, MAX_CONCURRENT_MATRIX, data, data_len,
				    wlan_hdd_get_concurrency_matrix_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	/* Parse and fetch max feature set */
	if (!tb[MATRIX_CONFIG_PARAM_SET_SIZE_MAX]) {
		hdd_err("Attr max feature set size failed");
		return -EINVAL;
	}
	max_feature_sets = nla_get_u32(tb[MATRIX_CONFIG_PARAM_SET_SIZE_MAX]);
	hdd_debug("Max feature set size: %d", max_feature_sets);

	/* Fill feature combination matrix */
	feature_sets = 0;
	feature_set_matrix[feature_sets++] = WIFI_FEATURE_INFRA |
						WIFI_FEATURE_P2P;
	feature_set_matrix[feature_sets++] = WIFI_FEATURE_INFRA |
						WIFI_FEATURE_NAN;
	/* Add more feature combinations here */

	feature_sets = QDF_MIN(feature_sets, max_feature_sets);
	hdd_debug("Number of feature sets: %d", feature_sets);
	hdd_debug("Feature set matrix");
	for (i = 0; i < feature_sets; i++)
		hdd_debug("[%d] 0x%02X", i, feature_set_matrix[i]);

	reply_skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u32) +
			sizeof(u32) * feature_sets + NLMSG_HDRLEN);
	if (!reply_skb) {
		hdd_err("Feature set matrix: buffer alloc fail");
		return -ENOMEM;
	}

	if (nla_put_u32(reply_skb,
		QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_RESULTS_SET_SIZE,
		feature_sets) ||
	    nla_put(reply_skb,
		    QCA_WLAN_VENDOR_ATTR_GET_CONCURRENCY_MATRIX_RESULTS_SET,
		    sizeof(u32) * feature_sets,
		    feature_set_matrix)) {
		hdd_err("nla put fail");
		kfree_skb(reply_skb);
		return -EINVAL;
	}
	return cfg80211_vendor_cmd_reply(reply_skb);
}

#undef MAX_CONCURRENT_MATRIX
#undef MATRIX_CONFIG_PARAM_SET_SIZE_MAX

int wlan_hdd_cfg80211_get_concurrency_matrix(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_get_concurrency_matrix(wiphy, wdev,
							   data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

