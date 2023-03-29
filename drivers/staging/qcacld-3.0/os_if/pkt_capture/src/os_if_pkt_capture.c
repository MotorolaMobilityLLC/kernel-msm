/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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

#include "osif_sync.h"
#include "qdf_str.h"
#include "qdf_trace.h"
#include "qdf_types.h"
#include "wlan_osif_priv.h"
#include <net/cfg80211.h>
#include "wlan_cfg80211.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_utility.h"
#include "wlan_osif_request_manager.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_pkt_capture_ucfg_api.h"
#include "os_if_pkt_capture.h"
#include "wlan_hdd_main.h"
#include "cfg_ucfg_api.h"
#include "wlan_hdd_object_manager.h"

#ifdef WLAN_FEATURE_PKT_CAPTURE

const struct nla_policy
set_monitor_mode_policy[SET_MONITOR_MODE_CONFIG_MAX + 1] = {
	[SET_MONITOR_MODE_INVALID] = {
		.type = NLA_U32
	},
	[SET_MONITOR_MODE_DATA_TX_FRAME_TYPE] = {
		.type = NLA_U32
	},
	[SET_MONITOR_MODE_DATA_RX_FRAME_TYPE] = {
		.type = NLA_U32
	},
	[SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE] = {
		.type = NLA_U32
	},
	[SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE] = {
		.type = NLA_U32
	},
	[SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE] = {
		.type = NLA_U32
	},
	[SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE] = {
		.type = NLA_U32
	},
	[SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL] = {
		.type = NLA_U32
	},
};

QDF_STATUS os_if_monitor_mode_configure(struct hdd_adapter *adapter,
					const void *data, int data_len)
{
	struct pkt_capture_frame_filter frame_filter = {0};
	struct wlan_objmgr_vdev *vdev;
	struct nlattr *tb[SET_MONITOR_MODE_CONFIG_MAX + 1];
	QDF_STATUS status;

	os_if_pkt_enter();

	if (tb[SET_MONITOR_MODE_INVALID])
		return QDF_STATUS_E_FAILURE;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return QDF_STATUS_E_INVAL;

	if (wlan_cfg80211_nla_parse(tb, SET_MONITOR_MODE_CONFIG_MAX,
				    data, data_len, set_monitor_mode_policy)) {
		osif_err("invalid monitor attr");
		hdd_objmgr_put_vdev(vdev);
		return QDF_STATUS_E_INVAL;
	}

	if (tb[SET_MONITOR_MODE_DATA_TX_FRAME_TYPE] &&
	    nla_get_u32(tb[SET_MONITOR_MODE_DATA_TX_FRAME_TYPE]) <
	    PACKET_CAPTURE_DATA_MAX_FILTER) {
		frame_filter.data_tx_frame_filter =
			nla_get_u32(tb[SET_MONITOR_MODE_DATA_TX_FRAME_TYPE]);
		frame_filter.vendor_attr_to_set =
			BIT(SET_MONITOR_MODE_DATA_TX_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_DATA_RX_FRAME_TYPE] &&
	    nla_get_u32(tb[SET_MONITOR_MODE_DATA_RX_FRAME_TYPE]) <
	    PACKET_CAPTURE_DATA_MAX_FILTER) {
		frame_filter.data_rx_frame_filter =
			nla_get_u32(tb[SET_MONITOR_MODE_DATA_RX_FRAME_TYPE]);
		frame_filter.vendor_attr_to_set |=
			BIT(SET_MONITOR_MODE_DATA_RX_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE] &&
	    nla_get_u32(tb[SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE]) <
	    PACKET_CAPTURE_MGMT_MAX_FILTER) {
		frame_filter.mgmt_tx_frame_filter =
			nla_get_u32(tb[SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE]);
		frame_filter.vendor_attr_to_set |=
			BIT(SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE] &&
	    nla_get_u32(tb[SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE]) <
	    PACKET_CAPTURE_MGMT_MAX_FILTER) {
		frame_filter.mgmt_rx_frame_filter =
			nla_get_u32(tb[SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE]);
		frame_filter.vendor_attr_to_set |=
			BIT(SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE] &&
	    nla_get_u32(tb[SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE]) <
	    PACKET_CAPTURE_CTRL_MAX_FILTER) {
		frame_filter.ctrl_tx_frame_filter =
			nla_get_u32(tb[SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE]);
		frame_filter.vendor_attr_to_set |=
			BIT(SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE] &&
	    nla_get_u32(tb[SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE]) <
	    PACKET_CAPTURE_CTRL_MAX_FILTER) {
		frame_filter.ctrl_rx_frame_filter =
			nla_get_u32(tb[SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE]);
		frame_filter.vendor_attr_to_set |=
			BIT(SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE);
	}

	if (tb[SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL]) {
		frame_filter.connected_beacon_interval =
		nla_get_u32(tb[SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL]);
		frame_filter.vendor_attr_to_set |=
			BIT(SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL);
	}

	osif_debug("Monitor mode config %s data tx %d data rx %d mgmt tx %d mgmt rx %d ctrl tx %d ctrl rx %d bi %d\n",
		   frame_filter.data_tx_frame_filter,
		   frame_filter.data_rx_frame_filter,
		   frame_filter.mgmt_tx_frame_filter,
		   frame_filter.mgmt_rx_frame_filter,
		   frame_filter.ctrl_tx_frame_filter,
		   frame_filter.ctrl_rx_frame_filter,
		   frame_filter.connected_beacon_interval);

	status = ucfg_pkt_capture_set_filter(frame_filter, vdev);
	hdd_objmgr_put_vdev(vdev);

	os_if_pkt_exit();

	return status;
}

#undef SET_MONITOR_MODE_CONFIG_MAX
#undef SET_MONITOR_MODE_INVALID
#undef SET_MONITOR_MODE_DATA_TX_FRAME_TYPE
#undef SET_MONITOR_MODE_DATA_RX_FRAME_TYPE
#undef SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE
#undef SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE
#undef SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE
#undef SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE
#undef SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL

#endif
