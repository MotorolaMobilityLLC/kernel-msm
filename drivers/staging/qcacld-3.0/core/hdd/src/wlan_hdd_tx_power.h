/*
 * Copyright (c) 2012-2018,2020 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_TX_POWER_H
#define __WLAN_HDD_TX_POWER_H

/**
 * DOC: wlan_hdd_tx_power_h
 *
 * WLAN Host Device Driver TX power setting API specification
 */

#ifdef FEATURE_TX_POWER

extern const struct nla_policy txpower_scale_policy[
			QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX + 1];

extern const struct nla_policy txpower_scale_decr_db_policy[
			QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB_MAX + 1];

/**
 * wlan_hdd_cfg80211_txpower_scale () - txpower scaling
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_txpower_scale(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data,
				    int data_len);

/**
 * wlan_hdd_cfg80211_txpower_scale_decr_db () - txpower scaling
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_txpower_scale_decr_db(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len);

#define FEATURE_TX_POWER_VENDOR_COMMANDS				\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_TXPOWER_SCALE,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				\
			 WIPHY_VENDOR_CMD_NEED_NETDEV |			\
			 WIPHY_VENDOR_CMD_NEED_RUNNING,			\
	.doit = wlan_hdd_cfg80211_txpower_scale,			\
	vendor_command_policy(txpower_scale_policy,                     \
			      QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_MAX)   \
},									\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd =							\
		QCA_NL80211_VENDOR_SUBCMD_SET_TXPOWER_SCALE_DECR_DB,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				\
			 WIPHY_VENDOR_CMD_NEED_NETDEV |			\
			 WIPHY_VENDOR_CMD_NEED_RUNNING,			\
	.doit = wlan_hdd_cfg80211_txpower_scale_decr_db,                \
	vendor_command_policy(txpower_scale_decr_db_policy,             \
			      QCA_WLAN_VENDOR_ATTR_TXPOWER_SCALE_DECR_DB_MAX) \
},
#else /* FEATURE_TX_POWER */
#define FEATURE_TX_POWER_VENDOR_COMMANDS
#endif /* FEATURE_TX_POWER */

#endif /* __WLAN_HDD_TX_POWER_H */

