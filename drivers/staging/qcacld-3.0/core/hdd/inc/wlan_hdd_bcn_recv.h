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
 * DOC: feature_bcn_recv
 * Feature for receiving beacons of connected AP and sending select
 * params to upper layer via vendor event
 */

#ifdef WLAN_BCN_RECV_FEATURE

struct wireless_dev;
struct wiphy;

/**
 * wlan_hdd_cfg80211_bcn_rcv_op() - Process beacon report operations
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Wrapper function of __wlan_hdd_cfg80211_bcn_rcv_op()
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_bcn_rcv_op(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len);

/**
 * hdd_beacon_recv_pause_indication()- Send vendor event to user space
 * to inform SCAN started indication
 * @hdd_handle: hdd handler
 * @vdev_id: vdev id
 * @type: scan event type
 * @is_disconnected: Connection state of driver
 *
 * Return: None
 */
void hdd_beacon_recv_pause_indication(hdd_handle_t hdd_handle,
				      uint8_t vdev_id,
				      enum scan_event_type type,
				      bool is_disconnected);

extern const struct nla_policy
	beacon_reporting_params_policy
	[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_MAX + 1];

#define BCN_RECV_FEATURE_VENDOR_COMMANDS				\
{									\
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING,	\
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				\
		 WIPHY_VENDOR_CMD_NEED_NETDEV |				\
		 WIPHY_VENDOR_CMD_NEED_RUNNING,				\
	.doit = wlan_hdd_cfg80211_bcn_rcv_op,				\
	vendor_command_policy(beacon_reporting_params_policy,		\
			      QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_MAX)\
},

#define BCN_RECV_FEATURE_VENDOR_EVENTS			\
[QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING_INDEX] = {		\
	.vendor_id = QCA_NL80211_VENDOR_ID,			\
	.subcmd = QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING	\
},
#else
#define BCN_RECV_FEATURE_VENDOR_COMMANDS
#define BCN_RECV_FEATURE_VENDOR_EVENTS

static inline
void hdd_beacon_recv_pause_indication(hdd_handle_t hdd_handle,
				      uint8_t vdev_id,
				      enum scan_event_type type,
				      bool is_disconnected)
{
}
#endif

