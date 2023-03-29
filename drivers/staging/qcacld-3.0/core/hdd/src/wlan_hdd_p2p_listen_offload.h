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

#ifndef __WLAN_HDD_P2P_LISTEN_OFFLOAD_H
#define __WLAN_HDD_P2P_LISTEN_OFFLOAD_H

/**
 * DOC: wlan_hdd_p2p_listen_offload_h
 *
 * WLAN Host Device Driver p2p listen offload API specification
 */

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
/**
 * __wlan_hdd_cfg80211_p2p_lo_start() - start P2P Listen Offload
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * This function is to process the p2p listen offload start vendor
 * command. It parses the input parameters and invoke WMA API to
 * send the command to firmware.
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_p2p_lo_start(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len);

/**
 * wlan_hdd_cfg80211_p2p_lo_stop() - stop P2P Listen Offload
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * This function inovkes internal __wlan_hdd_cfg80211_p2p_lo_stop()
 * to process p2p listen offload stop vendor command.
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_p2p_lo_stop(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data,
				  int data_len);

extern const struct nla_policy
p2p_listen_offload_policy[QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_MAX + 1];

#define FEATURE_P2P_LISTEN_OFFLOAD_VENDOR_COMMANDS			\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd =							\
		QCA_NL80211_VENDOR_SUBCMD_P2P_LISTEN_OFFLOAD_START,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				\
			WIPHY_VENDOR_CMD_NEED_NETDEV |			\
			WIPHY_VENDOR_CMD_NEED_RUNNING,			\
	.doit = wlan_hdd_cfg80211_p2p_lo_start,				\
	vendor_command_policy(p2p_listen_offload_policy,		\
			      QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_MAX)\
},									\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd =							\
		QCA_NL80211_VENDOR_SUBCMD_P2P_LISTEN_OFFLOAD_STOP,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				\
			WIPHY_VENDOR_CMD_NEED_NETDEV |			\
			WIPHY_VENDOR_CMD_NEED_RUNNING,			\
	.doit = wlan_hdd_cfg80211_p2p_lo_stop,				\
	vendor_command_policy(p2p_listen_offload_policy,		\
			      QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_MAX)\
},
#else /* FEATURE_P2P_LISTEN_OFFLOAD */
#define FEATURE_P2P_LISTEN_OFFLOAD_VENDOR_COMMANDS
#endif /* FEATURE_P2P_LISTEN_OFFLOAD */

#endif /* __WLAN_HDD_P2P_LISTEN_OFFLOAD_H */

