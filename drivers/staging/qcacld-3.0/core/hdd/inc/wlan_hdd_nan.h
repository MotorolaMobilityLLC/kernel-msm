/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_NAN_H
#define __WLAN_HDD_NAN_H

/**
 * DOC: wlan_hdd_nan.h
 *
 * WLAN Host Device Driver NAN API specification
 */

struct hdd_context;

#ifdef WLAN_FEATURE_NAN
struct wiphy;
struct wireless_dev;

bool wlan_hdd_nan_is_supported(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_cfg80211_nan_ext_request() - handle NAN Extended request
 * @wiphy:   pointer to wireless wiphy structure.
 * @wdev:    pointer to wireless_dev structure.
 * @data:    Pointer to the data to be passed via vendor interface
 * @data_len:Length of the data to be passed
 *
 * This function is called by userspace to send a NAN request to
 * firmware.  This is an SSR-protected wrapper function.
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_nan_ext_request(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len);

#define FEATURE_NAN_VENDOR_COMMANDS					\
	{                                                               \
		.info.vendor_id = QCA_NL80211_VENDOR_ID,                \
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_NAN_EXT,       \
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                   \
			 WIPHY_VENDOR_CMD_NEED_NETDEV |                 \
			 WIPHY_VENDOR_CMD_NEED_RUNNING,                 \
		.doit = wlan_hdd_cfg80211_nan_ext_request,		\
		vendor_command_policy(nan_attr_policy,			\
				      QCA_WLAN_VENDOR_ATTR_NAN_PARAMS_MAX) \
	},								\
	{                                                               \
		.info.vendor_id = QCA_NL80211_VENDOR_ID,                \
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_NDP,           \
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                   \
			WIPHY_VENDOR_CMD_NEED_NETDEV |                  \
			WIPHY_VENDOR_CMD_NEED_RUNNING,                  \
		.doit = wlan_hdd_cfg80211_process_ndp_cmd,		\
		vendor_command_policy(vendor_attr_policy,		\
				      QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX) \
	},
#else /* WLAN_FEATURE_NAN */
#define FEATURE_NAN_VENDOR_COMMANDS

static inline bool wlan_hdd_nan_is_supported(struct hdd_context *hdd_ctx)
{
	return false;
}
#endif /* WLAN_FEATURE_NAN */

/**
 * hdd_nan_concurrency_update() - NAN concurrency
 *
 * Return: None
 */
void hdd_nan_concurrency_update(void);

#endif /* __WLAN_HDD_NAN_H */
