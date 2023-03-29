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
 * DOC: wlan_hdd_fw_state.h
 *
 * Get firmware state related API's and definitions
 */

#ifndef __WLAN_HDD_FW_STATE_H
#define __WLAN_HDD_FW_STATE_H

#ifdef FEATURE_FW_STATE
#include <net/cfg80211.h>
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
				   int data_len);

#define FEATURE_FW_STATE_COMMANDS                                       \
{                                                                       \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                        \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_GET_FW_STATE,          \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                           \
		WIPHY_VENDOR_CMD_NEED_NETDEV,                           \
	.doit = wlan_hdd_cfg80211_get_fw_state,                         \
	vendor_command_policy(VENDOR_CMD_RAW_DATA, 0)                   \
},
#else /* FEATURE_FW_STATE */
#define FEATURE_FW_STATE_COMMANDS
#endif /* FEATURE_FW_STATE */
#endif /* __WLAN_HDD_FW_STATE_H */
