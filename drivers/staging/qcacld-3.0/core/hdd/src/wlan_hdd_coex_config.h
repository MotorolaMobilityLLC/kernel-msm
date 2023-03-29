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
 * DOC: wlan_hdd_coex_config.h
 *
 * Add Vendor subcommand QCA_NL80211_VENDOR_SUBCMD_COEX_CONFIG
 */

#ifndef __WLAN_HDD_COEX_CONFIG_H
#define __WLAN_HDD_COEX_CONFIG_H

#ifdef FEATURE_COEX_CONFIG
#include <net/cfg80211.h>

/**
 * wlan_hdd_cfg80211_set_coex_config() - set coex configuration
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: pointer to limit off-channel command parameters.
 * @data_len: the length in byte of  limit off-channel command parameters.
 *
 * Return: An error code or 0 on success.
 */
int wlan_hdd_cfg80211_set_coex_config(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int data_len);

extern const struct nla_policy
coex_config_three_way_policy[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_MAX + 1];

#define FEATURE_COEX_CONFIG_COMMANDS                                     \
{                                                                        \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                         \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_COEX_CONFIG,            \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                            \
		WIPHY_VENDOR_CMD_NEED_NETDEV |                           \
		WIPHY_VENDOR_CMD_NEED_RUNNING,                           \
	.doit = wlan_hdd_cfg80211_set_coex_config,                       \
	vendor_command_policy(coex_config_three_way_policy,              \
			      QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_MAX) \
},
#else /* FEATURE_COEX_CONFIG */
#define FEATURE_COEX_CONFIG_COMMANDS
#endif /* FEATURE_COEX_CONFIG */

#endif /* __WLAN_HDD_COEX_CONFIG_H */
