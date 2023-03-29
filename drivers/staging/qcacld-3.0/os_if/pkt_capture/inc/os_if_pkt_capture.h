/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

#ifndef _OS_IF_PKT_CAPTURE_H_
#define _OS_IF_PKT_CAPTURE_H_

#include "qdf_types.h"
#include "qca_vendor.h"
#include "wlan_hdd_main.h"

#ifdef WLAN_FEATURE_PKT_CAPTURE

#define os_if_pkt_enter() QDF_TRACE_ENTER(QDF_MODULE_ID_HDD, "enter")
#define os_if_pkt_exit() QDF_TRACE_EXIT(QDF_MODULE_ID_HDD, "exit")

#define FEATURE_MONITOR_MODE_VENDOR_COMMANDS				   \
	{								   \
		.info.vendor_id = QCA_NL80211_VENDOR_ID,		   \
		.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE, \
		.flags = WIPHY_VENDOR_CMD_NEED_WDEV |			   \
			WIPHY_VENDOR_CMD_NEED_NETDEV |			   \
			WIPHY_VENDOR_CMD_NEED_RUNNING,			   \
		.doit = wlan_hdd_cfg80211_set_monitor_mode,		   \
		vendor_command_policy(set_monitor_mode_policy,		   \
				QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MAX) \
	},

/* Short name for QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE command */

#define SET_MONITOR_MODE_CONFIG_MAX \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MAX
#define SET_MONITOR_MODE_INVALID \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_INVALID
#define SET_MONITOR_MODE_DATA_TX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_DATA_TX_FRAME_TYPE
#define SET_MONITOR_MODE_DATA_RX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_DATA_RX_FRAME_TYPE
#define SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE
#define SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE
#define SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE
#define SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE
#define SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL \
	QCA_WLAN_VENDOR_ATTR_SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL

extern const struct nla_policy
set_monitor_mode_policy[SET_MONITOR_MODE_CONFIG_MAX + 1];

/**
 * os_if_monitor_mode_configure() - Process monitor mode configuration
 * operation in the received vendor command
 * @adapter: adapter pointer
 * @tb: nl attributes Handles QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE
 *
 * Return: 0 for Success and negative value for failure
 */
QDF_STATUS os_if_monitor_mode_configure(struct hdd_adapter *adapter,
					const void *data, int data_len);
#endif
#endif
