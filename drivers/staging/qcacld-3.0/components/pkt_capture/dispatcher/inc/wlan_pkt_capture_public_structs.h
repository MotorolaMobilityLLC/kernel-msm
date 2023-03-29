/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WLAN_PKT_CAPTURE_PUBLIC_STRUCTS_H_
#define _WLAN_PKT_CAPTURE_PUBLIC_STRUCTS_H_

#define PACKET_CAPTURE_DATA_MAX_FILTER BIT(18)
#define PACKET_CAPTURE_MGMT_MAX_FILTER BIT(5)
#define PACKET_CAPTURE_CTRL_MAX_FILTER BIT(3)

/**
 * enum pkt_capture_mode - packet capture modes
 * @PACKET_CAPTURE_MODE_DISABLE: packet capture mode disable
 * @PACKET_CAPTURE_MODE_MGMT_ONLY: capture mgmt packets only
 * @PACKET_CAPTURE_MODE_DATA_ONLY: capture data packets only
 */
enum pkt_capture_mode {
	PACKET_CAPTURE_MODE_DISABLE = 0,
	PACKET_CAPTURE_MODE_MGMT_ONLY = BIT(0),
	PACKET_CAPTURE_MODE_DATA_ONLY = BIT(1),
};

/**
 * enum pkt_capture_config - packet capture config
 * @PACKET_CAPTURE_CONFIG_TRIGGER_ENABLE: enable capture for trigger frames only
 * @PACKET_CAPTURE_CONFIG_QOS_ENABLE: enable capture for qos frames only
 * @PACKET_CAPTURE_CONFIG_CONNECT_NO_BEACON_ENABLE: drop all beacons, when
 *                                                  device in connected state
 * @PACKET_CAPTURE_CONFIG_CONNECT_BEACON_ENABLE: enable only connected BSSID
 *                                      beacons, when device in connected state
 * @PACKET_CAPTURE_CONFIG_CONNECT_OFF_CHANNEL_BEACON_ENABLE: enable off channel
 *                                      beacons, when device in connected state
 */
enum pkt_capture_config {
	PACKET_CAPTURE_CONFIG_TRIGGER_ENABLE = BIT(0),
	PACKET_CAPTURE_CONFIG_QOS_ENABLE = BIT(1),
	PACKET_CAPTURE_CONFIG_BEACON_ENABLE = BIT(2),
	PACKET_CAPTURE_CONFIG_OFF_CHANNEL_BEACON_ENABLE = BIT(3),
	PACKET_CAPTURE_CONFIG_NO_BEACON_ENABLE = BIT(4),
};

/**
 * struct mgmt_offload_event_params - Management offload event params
 * @tsf_l32: The lower 32 bits of the TSF
 * @chan_freq: channel frequency in MHz
 * @rate_kbps: Rate kbps
 * @rssi: combined RSSI, i.e. the sum of the snr + noise floor (dBm units)
 * @buf_len: length of the frame in bytes
 * @tx_status: 0: xmit ok
 *             1: excessive retries
 *             2: blocked by tx filtering
 *             4: fifo underrun
 *             8: swabort
 * @buf: management frame buffer
 * @tx_retry_cnt: tx retry count
 */
struct mgmt_offload_event_params {
	uint32_t tsf_l32;
	uint32_t chan_freq;
	uint32_t rate_kbps;
	uint32_t rssi;
	uint32_t buf_len;
	uint32_t tx_status;
	uint8_t *buf;
	uint8_t tx_retry_cnt;
};

struct smu_event_params {
	uint32_t vdev_id;
	int32_t rx_avg_rssi;
};

/**
 * struct pkt_capture_callbacks - callbacks to non-converged driver
 * @get_rmf_status: callback to get rmf status
 */
struct pkt_capture_callbacks {
	int (*get_rmf_status)(uint8_t vdev_id);
};

/**
 * struct wlan_pkt_capture_tx_ops - structure of tx operation function
 * pointers for packet capture component
 * @pkt_capture_send_mode: send packet capture mode
 * @pkt_capture_send_config: send packet capture config
 *
 */
struct wlan_pkt_capture_tx_ops {
	QDF_STATUS (*pkt_capture_send_mode)(struct wlan_objmgr_psoc *psoc,
					    uint8_t vdev_id,
					    enum pkt_capture_mode mode);
	QDF_STATUS (*pkt_capture_send_config)
				(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id,
				 enum pkt_capture_config config);
	QDF_STATUS (*pkt_capture_send_beacon_interval)
				(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id,
				 uint32_t nth_value);
};

/**
 * struct wlan_pkt_capture_rx_ops - structure of rx operation function
 * pointers for packet capture component
 * @pkt_capture_register_ev_handlers: register mgmt offload event
 * @pkt_capture_unregister_ev_handlers: unregister mgmt offload event
 * @pkt_capture_register_smart_monitor_event: register smu event
 * @pkt_capture_unregister_smart_monitor_event: unregister smu event
 */
struct wlan_pkt_capture_rx_ops {
	QDF_STATUS (*pkt_capture_register_ev_handlers)
					(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*pkt_capture_unregister_ev_handlers)
					(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*pkt_capture_register_smart_monitor_event)
					(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*pkt_capture_unregister_smart_monitor_event)
					(struct wlan_objmgr_psoc *psoc);
};

/**
 * pkt_capture_data_frame_type - Represent the various
 * data types to be filtered in packet capture.
 */
enum pkt_capture_data_frame_type {
	PKT_CAPTURE_DATA_FRAME_TYPE_ALL = BIT(0),
	/* valid only if PKT_CAPTURE_DATA_DATA_FRAME_TYPE_ALL is not set */
	PKT_CAPTURE_DATA_FRAME_TYPE_ARP = BIT(1),
	PKT_CAPTURE_DATA_FRAME_TYPE_DHCPV4 = BIT(2),
	PKT_CAPTURE_DATA_FRAME_TYPE_DHCPV6 = BIT(3),
	PKT_CAPTURE_DATA_FRAME_TYPE_EAPOL = BIT(4),
	PKT_CAPTURE_DATA_FRAME_TYPE_DNSV4 = BIT(5),
	PKT_CAPTURE_DATA_FRAME_TYPE_DNSV6 = BIT(6),
	PKT_CAPTURE_DATA_FRAME_TYPE_TCP_SYN = BIT(7),
	PKT_CAPTURE_DATA_FRAME_TYPE_TCP_SYNACK = BIT(8),
	PKT_CAPTURE_DATA_FRAME_TYPE_TCP_FIN = BIT(9),
	PKT_CAPTURE_DATA_FRAME_TYPE_TCP_FINACK = BIT(10),
	PKT_CAPTURE_DATA_FRAME_TYPE_TCP_ACK = BIT(11),
	PKT_CAPTURE_DATA_FRAME_TYPE_TCP_RST = BIT(12),
	PKT_CAPTURE_DATA_FRAME_TYPE_ICMPV4 = BIT(13),
	PKT_CAPTURE_DATA_FRAME_TYPE_ICMPV6 = BIT(14),
	PKT_CAPTURE_DATA_FRAME_TYPE_RTP = BIT(15),
	PKT_CAPTURE_DATA_FRAME_TYPE_SIP = BIT(16),
	PKT_CAPTURE_DATA_FRAME_QOS_NULL = BIT(17),
};

/**
 * pkt_capture_mgmt_frame_type - Represent the various
 * mgmt types to be sent over the monitor interface.
 * @PKT_CAPTURE_MGMT_FRAME_TYPE_ALL: All the MGMT Frames.
 * @PKT_CAPTURE_MGMT_CONNECT_NO_BEACON: All the MGMT Frames
 * except the Beacons. Valid only in the Connect state.
 * @PKT_CAPTURE_MGMT_CONNECT_BEACON: Only the connected
 * BSSID Beacons. Valid only in the Connect state.
 * @PKT_CAPTURE_MONITOR_MGMT_CONNECT_SCAN_BEACON: Represents
 * the Beacons obtained during the scan (off channel and connected channel)
 * when in connected state.
 */

enum pkt_capture_mgmt_frame_type {
	PKT_CAPTURE_MGMT_FRAME_TYPE_ALL = BIT(0),
	/* valid only if PKT_CAPTURE_MGMT_FRAME_TYPE_ALL is not set */
	PKT_CAPTURE_MGMT_CONNECT_NO_BEACON = BIT(1),
	PKT_CAPTURE_MGMT_CONNECT_BEACON = BIT(2),
	PKT_CAPTURE_MGMT_CONNECT_SCAN_BEACON = BIT(3),
};

/**
 * pkt_capture_ctrl_frame_type - Represent the various
 * ctrl types to be sent over the monitor interface.
 * @PKT_CAPTURE_CTRL_FRAME_TYPE_ALL: All the ctrl Frames.
 * @PKT_CAPTURE_CTRL_TRIGGER_FRAME: Trigger Frame.
 */
enum pkt_capture_ctrl_frame_type {
	PKT_CAPTURE_CTRL_FRAME_TYPE_ALL = BIT(0),
	/* valid only if PKT_CAPTURE_CTRL_FRAME_TYPE_ALL is not set */
	PKT_CAPTURE_CTRL_TRIGGER_FRAME = BIT(1),
};

/**
 * enum pkt_capture_attr_set_monitor_mode - Used by the
 * vendor command QCA_NL80211_VENDOR_SUBCMD_SET_MONITOR_MODE to set the
 * monitor mode.
 *
 * @PKT_CAPTURE_ATTR_SET_MONITOR_MODE_DATA_TX_FRAME_TYPE: u32 attribute,
 * Represents the tx data packet type to be monitored (u32). These data packets
 * are represented by enum pkt_capture_data_frame_type.
 *
 * @PKT_CAPTURE_ATTR_SET_MONITOR_MODE_DATA_RX_FRAME_TYPE: u32 attribute,
 * Represents the tx data packet type to be monitored (u32). These data packets
 * are represented by enum pkt_capture_data_frame_type.
 *
 * @PKT_CAPTURE_ATTR_SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE: u32 attribute,
 * Represents the tx data packet type to be monitored (u32). These mgmt packets
 * are represented by enum pkt_capture_mgmt_frame_type.
 *
 * @PKT_CAPTURE_ATTR_SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE: u32 attribute,
 * Represents the tx data packet type to be monitored (u32). These mgmt packets
 * are represented by enum pkt_capture_mgmt_frame_type.
 *
 * @PKT_CAPTURE_ATTR_SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE: u32 attribute,
 * Represents the tx data packet type to be monitored (u32). These ctrl packets
 * are represented by enum pkt_capture_ctrl_frame_type.
 *
 * @PKT_CAPTURE_ATTR_SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE: u32 attribute,
 * Represents the tx data packet type to be monitored (u32). These ctrl packets
 * are represented by enum pkt_capture_ctrl_frame_type.
 *
 * @PKT_CAPTURE_ATTR_SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL: u32 attribute,
 * An interval only for the connected beacon interval, which expects that the
 * connected BSSID's beacons shall be sent on the monitor interface only on this
 * specific interval.
 */
enum pkt_capture_attr_set_monitor_mode {
	PKT_CAPTURE_ATTR_SET_MONITOR_MODE_INVALID = 0,
	PKT_CAPTURE_ATTR_SET_MONITOR_MODE_DATA_TX_FRAME_TYPE = 1,
	PKT_CAPTURE_ATTR_SET_MONITOR_MODE_DATA_RX_FRAME_TYPE = 2,
	PKT_CAPTURE_ATTR_SET_MONITOR_MODE_MGMT_TX_FRAME_TYPE = 3,
	PKT_CAPTURE_ATTR_SET_MONITOR_MODE_MGMT_RX_FRAME_TYPE = 4,
	PKT_CAPTURE_ATTR_SET_MONITOR_MODE_CTRL_TX_FRAME_TYPE = 5,
	PKT_CAPTURE_ATTR_SET_MONITOR_MODE_CTRL_RX_FRAME_TYPE = 6,
	PKT_CAPTURE_ATTR_SET_MONITOR_MODE_CONNECTED_BEACON_INTERVAL = 7,

	/* keep last */
	PKT_CAPTURE_ATTR_SET_MONITOR_MODE_AFTER_LAST,
	PKT_CAPTURE_ATTR_SET_MONITOR_MODE_MAX =
		PKT_CAPTURE_ATTR_SET_MONITOR_MODE_AFTER_LAST - 1,

};

struct pkt_capture_frame_filter {
	enum pkt_capture_data_frame_type data_tx_frame_filter;
	enum pkt_capture_data_frame_type data_rx_frame_filter;
	enum pkt_capture_mgmt_frame_type mgmt_tx_frame_filter;
	enum pkt_capture_mgmt_frame_type mgmt_rx_frame_filter;
	enum pkt_capture_ctrl_frame_type ctrl_tx_frame_filter;
	enum pkt_capture_ctrl_frame_type ctrl_rx_frame_filter;
	uint32_t connected_beacon_interval;
	uint8_t vendor_attr_to_set;
};
#endif /* _WLAN_PKT_CAPTURE_PUBLIC_STRUCTS_H_ */
