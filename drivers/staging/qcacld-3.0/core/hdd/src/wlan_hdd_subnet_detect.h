/*
 * Copyright (c) 2015,2020 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_SUBNET_DETECT_H
#define __WLAN_HDD_SUBNET_DETECT_H

/**
 * DOC: wlan_hdd_subnet_detect.h
 *
 * WLAN Host Device Driver subnet detect API specification
 */

#ifdef FEATURE_LFR_SUBNET_DETECTION
struct wiphy;
struct wireless_dev;

/* QCA_NL80211_VENDOR_SUBCMD_GW_PARAM_CONFIG policy */
extern const struct nla_policy subnet_detect_policy[
			QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_MAX + 1];

#define FEATURE_LFR_SUBNET_DETECT_VENDOR_COMMANDS \
	{ \
		.info.vendor_id = QCA_NL80211_VENDOR_ID, \
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GW_PARAM_CONFIG, \
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV | \
				WIPHY_VENDOR_CMD_NEED_NETDEV | \
				WIPHY_VENDOR_CMD_NEED_RUNNING, \
		.doit = wlan_hdd_cfg80211_set_gateway_params, \
		vendor_command_policy(subnet_detect_policy, \
				      QCA_WLAN_VENDOR_ATTR_GW_PARAM_CONFIG_MAX)\
	},

int wlan_hdd_cfg80211_set_gateway_params(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len);
#endif /* FEATURE_LFR_SUBNET_DETECTION */
#endif /* __WLAN_HDD_SUBNET_DETECT_H */
