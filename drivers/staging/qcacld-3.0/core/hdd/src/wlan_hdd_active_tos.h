/*
 * Copyright (c) 2012-2018, 2020 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_ACTIVE_TOS_H
#define __WLAN_HDD_ACTIVE_TOS_H

/**
 * DOC: wlan_hdd_active_tos_h
 *
 * WLAN Host Device Driver ACTIVE TOS API specification
 */

#ifdef FEATURE_ACTIVE_TOS
/**
 * wlan_hdd_cfg80211_set_limit_offchan_param() - set limit off-channel cmd
 * parameters
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: pointer to limit off-channel command parameters.
 * @data_len: the length in byte of  limit off-channel command parameters.
 *
 * This is called when application wants to limit the off channel time due to
 * active voip traffic.
 *
 * Return: An error code or 0 on success.
 */
int wlan_hdd_cfg80211_set_limit_offchan_param(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data, int data_len);

extern const struct nla_policy
	wlan_hdd_set_limit_off_channel_param_policy
	[QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS_MAX + 1];

#define FEATURE_ACTIVE_TOS_VENDOR_COMMANDS				   \
{									   \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			   \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_ACTIVE_TOS,		   \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				   \
		WIPHY_VENDOR_CMD_NEED_NETDEV |				   \
		WIPHY_VENDOR_CMD_NEED_RUNNING,				   \
	.doit = wlan_hdd_cfg80211_set_limit_offchan_param,		   \
	vendor_command_policy(wlan_hdd_set_limit_off_channel_param_policy, \
			      QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS_MAX)	   \
},
#else /* FEATURE_ACTIVE_TOS */
#define FEATURE_ACTIVE_TOS_VENDOR_COMMANDS
#endif /* FEATURE_ACTIVE_TOS */

#endif /* __WLAN_HDD_ACTIVE_TOS_H */

