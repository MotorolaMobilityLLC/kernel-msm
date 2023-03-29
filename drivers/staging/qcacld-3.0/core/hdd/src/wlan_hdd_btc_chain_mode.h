/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_btc_chain_mode.h
 *
 * Add Vendor subcommand QCA_NL80211_VENDOR_SUBCMD_BTC_CHAIN_MODE
 */

#ifndef __WLAN_HDD_BTC_CHAIN_MODE_H
#define __WLAN_HDD_BTC_CHAIN_MODE_H

#ifdef FEATURE_BTC_CHAIN_MODE
#include <net/cfg80211.h>

/**
 * wlan_hdd_register_btc_chain_mode_handler() - register handler for BT coex
 * chain mode notification.
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
void wlan_hdd_register_btc_chain_mode_handler(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_hdd_cfg80211_set_btc_chain_mode() - set btc chain mode
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: pointer to btc chain mode command parameters.
 * @data_len: the length in byte of btc chain mode command parameters.
 *
 * Return: An error code or 0 on success.
 */
int wlan_hdd_cfg80211_set_btc_chain_mode(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int data_len);

extern const struct nla_policy
btc_chain_mode_policy[QCA_VENDOR_ATTR_BTC_CHAIN_MODE_MAX + 1];

#define FEATURE_BTC_CHAIN_MODE_COMMANDS					\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_BTC_CHAIN_MODE,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				\
		WIPHY_VENDOR_CMD_NEED_NETDEV |				\
		WIPHY_VENDOR_CMD_NEED_RUNNING,				\
	.doit = wlan_hdd_cfg80211_set_btc_chain_mode,			\
	vendor_command_policy(btc_chain_mode_policy,			\
			      QCA_VENDOR_ATTR_BTC_CHAIN_MODE_MAX)	\
},
#else /* FEATURE_BTC_CHAIN_MODE */
static inline void
wlan_hdd_register_btc_chain_mode_handler(struct wlan_objmgr_psoc *psoc) {}

#define FEATURE_BTC_CHAIN_MODE_COMMANDS
#endif /* FEATURE_BTC_CHAIN_MODE */

#endif /* __WLAN_HDD_BTC_CHAIN_MODE_H */
