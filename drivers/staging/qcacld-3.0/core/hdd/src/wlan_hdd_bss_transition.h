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

#ifndef __WLAN_HDD_BSS_TRANSITION_H
#define __WLAN_HDD_BSS_TRANSITION_H

/**
 * DOC: wlan_hdd_bss_transition_h
 *
 * WLAN Host Device Driver BSS transition API specification
 */

#ifdef FEATURE_BSS_TRANSITION
/**
 * wlan_hdd_cfg80211_fetch_bss_transition_status() - fetch bss transition status
 * @wiphy: WIPHY structure pointer
 * @wdev: Wireless device structure pointer
 * @data: Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function is used to fetch transition status for candidate bss. The
 * transition status is either accept or reason for reject.
 *
 * Return: 0 on success and errno on failure
 */
int
wlan_hdd_cfg80211_fetch_bss_transition_status(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data, int data_len);

extern const struct nla_policy
	btm_params_policy
	[QCA_WLAN_VENDOR_ATTR_MAX + 1];

extern const struct nla_policy
	btm_cand_list_policy
	[QCA_WLAN_VENDOR_ATTR_BTM_CANDIDATE_INFO_MAX + 1];

#define FEATURE_BSS_TRANSITION_VENDOR_COMMANDS				\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd =							\
		QCA_NL80211_VENDOR_SUBCMD_FETCH_BSS_TRANSITION_STATUS,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				\
		 WIPHY_VENDOR_CMD_NEED_NETDEV |				\
		 WIPHY_VENDOR_CMD_NEED_RUNNING,				\
	.doit = wlan_hdd_cfg80211_fetch_bss_transition_status,		\
	vendor_command_policy(btm_params_policy,			\
			      QCA_WLAN_VENDOR_ATTR_MAX)			\
},
#else /* FEATURE_BSS_TRANSITION */
#define FEATURE_BSS_TRANSITION_VENDOR_COMMANDS
#endif /* FEATURE_BSS_TRANSITION */

#endif /* __WLAN_HDD_BSS_TRANSITION_H */

