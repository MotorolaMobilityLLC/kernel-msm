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
 * DOC: wlan_hdd_hw_capability.h
 *
 * Add Vendor subcommand QCA_NL80211_VENDOR_SUBCMD_GET_HW_CAPABILITY
 */

#ifndef __WLAN_HDD_HW_CAPABILITY_H
#define __WLAN_HDD_HW_CAPABILITY_H

#ifdef FEATURE_HW_CAPABILITY
#include <net/cfg80211.h>

/**
 * wlan_hdd_cfg80211_get_hw_capability() - get hardware capability
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Return: 0 on success; error number otherwise.
 */
int wlan_hdd_cfg80211_get_hw_capability(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len);

#define FEATURE_HW_CAPABILITY_COMMANDS					\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_HW_CAPABILITY,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				\
		WIPHY_VENDOR_CMD_NEED_NETDEV |				\
		WIPHY_VENDOR_CMD_NEED_RUNNING,				\
	.doit = wlan_hdd_cfg80211_get_hw_capability,			\
	vendor_command_policy(VENDOR_CMD_RAW_DATA, 0)			\
},
#else /* FEATURE_HW_CAPABILITY */
#define FEATURE_HW_CAPABILITY_COMMANDS
#endif /* FEATURE_HW_CAPABILITY */

#endif /* __WLAN_HDD_HW_CAPABILITY_H */
