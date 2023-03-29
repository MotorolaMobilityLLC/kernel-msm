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

#ifndef __WLAN_HDD_RSSI_MONITOR_H
#define __WLAN_HDD_RSSI_MONITOR_H

/**
 * DOC: wlan_hdd_rssi_monitor_h
 *
 * WLAN Host Device Driver RSSI monitoring API specification
 */

#ifdef FEATURE_RSSI_MONITOR

/* QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI policy */
extern const struct nla_policy moitor_rssi_policy[
			QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX + 1];

/**
 * wlan_hdd_cfg80211_monitor_rssi() - SSR wrapper to rssi monitoring
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */
int
wlan_hdd_cfg80211_monitor_rssi(struct wiphy *wiphy, struct wireless_dev *wdev,
			       const void *data, int data_len);

/**
 * hdd_rssi_threshold_breached() - rssi breached NL event
 * @hdd_handle: HDD handle
 * @data: rssi breached event data
 *
 * This function reads the rssi breached event %data and fill in the skb with
 * NL attributes and send up the NL event.
 *
 * Return: none
 */
void hdd_rssi_threshold_breached(hdd_handle_t hdd_handle,
				 struct rssi_breach_event *data);

#define FEATURE_RSSI_MONITOR_VENDOR_EVENTS                      \
[QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI_INDEX] = {              \
	.vendor_id = QCA_NL80211_VENDOR_ID,                     \
	.subcmd = QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI        \
},

#define FEATURE_RSSI_MONITOR_VENDOR_COMMANDS                            \
{                                                                       \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                        \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_MONITOR_RSSI,          \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                           \
		WIPHY_VENDOR_CMD_NEED_NETDEV |                          \
		WIPHY_VENDOR_CMD_NEED_RUNNING,                          \
	.doit = wlan_hdd_cfg80211_monitor_rssi,                         \
	vendor_command_policy(moitor_rssi_policy,                       \
			      QCA_WLAN_VENDOR_ATTR_RSSI_MONITORING_MAX) \
},

#else /* FEATURE_RSSI_MONITOR */
static inline
void hdd_rssi_threshold_breached(hdd_handle_t hdd_handle,
				 struct rssi_breach_event *data)
{
}

#define FEATURE_RSSI_MONITOR_VENDOR_EVENTS
#define FEATURE_RSSI_MONITOR_VENDOR_COMMANDS
#endif /* FEATURE_RSSI_MONITOR */

#endif /* __WLAN_HDD_RSSI_MONITOR_H */

