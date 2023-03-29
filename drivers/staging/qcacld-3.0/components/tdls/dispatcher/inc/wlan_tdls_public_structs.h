/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_tdls_public_structs.h
 *
 * TDLS public structure definations
 */

#ifndef _WLAN_TDLS_STRUCTS_H_
#define _WLAN_TDLS_STRUCTS_H_
#include <qdf_timer.h>
#include <qdf_list.h>
#include <qdf_mc_timer.h>
#include <wlan_cmn.h>
#include <wlan_cmn_ieee80211.h>
#ifdef FEATURE_RUNTIME_PM
#include <wlan_pmo_common_public_struct.h>
#endif

#define WLAN_TDLS_STA_MAX_NUM                        8
#define WLAN_TDLS_STA_P_UAPSD_OFFCHAN_MAX_NUM        1
#define WLAN_TDLS_PEER_LIST_SIZE                     16
#define WLAN_TDLS_CT_TABLE_SIZE                      8
#define WLAN_TDLS_PEER_SUB_LIST_SIZE                 10
#define WLAN_MAC_MAX_EXTN_CAP                        8
#define WLAN_MAC_MAX_SUPP_CHANNELS                   100
#define WLAN_MAC_WMI_MAX_SUPP_CHANNELS               128
#define WLAN_MAX_SUPP_OPER_CLASSES                   32
#define WLAN_MAC_MAX_SUPP_RATES                      32
#define WLAN_CHANNEL_14                              14
#define ENABLE_CHANSWITCH                            1
#define DISABLE_CHANSWITCH                           2
#define WLAN_TDLS_PREFERRED_OFF_CHANNEL_NUM_MIN      1
#define WLAN_TDLS_PREFERRED_OFF_CHANNEL_NUM_MAX      165
#define WLAN_TDLS_PREFERRED_OFF_CHANNEL_NUM_DEF      36

#define AC_PRIORITY_NUM                 4

/* Default tdls serialize timeout is set to 4 (peer delete) + 1 secs */
#ifdef FEATURE_RUNTIME_PM
/* Add extra PMO_RESUME_TIMEOUT for runtime PM resume timeout */
#define TDLS_DELETE_PEER_CMD_TIMEOUT (4000 + 1000 + PMO_RESUME_TIMEOUT)
#else
#define TDLS_DELETE_PEER_CMD_TIMEOUT (4000 + 1000)
#endif

/** Maximum time(ms) to wait for tdls del sta to complete **/
#define WAIT_TIME_TDLS_DEL_STA  (TDLS_DELETE_PEER_CMD_TIMEOUT + 1000)

#define TDLS_DEFAULT_SERIALIZE_CMD_TIMEOUT (4000)

/** Maximum time(ms) to wait for tdls add sta to complete **/
#define WAIT_TIME_TDLS_ADD_STA  (TDLS_DEFAULT_SERIALIZE_CMD_TIMEOUT + 1000)

/** Maximum time(ms) to wait for Link Establish Req to complete **/
#define WAIT_TIME_TDLS_LINK_ESTABLISH_REQ      1500

/** Maximum time(ms) to wait for tdls mgmt to complete **/
#define WAIT_TIME_FOR_TDLS_MGMT         3000

/** Maximum time(ms) to wait for tdls mgmt to complete **/
#define WAIT_TIME_FOR_TDLS_USER_CMD     11000

/** Maximum waittime for TDLS teardown links **/
#define WAIT_TIME_FOR_TDLS_TEARDOWN_LINKS 10000

/** Maximum waittime for TDLS antenna switch **/
#define WAIT_TIME_FOR_TDLS_ANTENNA_SWITCH 1000

#define TDLS_TEARDOWN_PEER_UNREACHABLE   25
#define TDLS_TEARDOWN_PEER_UNSPEC_REASON 26

#define INVALID_TDLS_PEER_INDEX 0xFF

#ifdef WLAN_FEATURE_11AX
#define MIN_TDLS_HE_CAP_LEN 17
#define MAX_TDLS_HE_CAP_LEN 29
#endif

/**
 * enum tdls_add_oper - add peer type
 * @TDLS_OPER_NONE: none
 * @TDLS_OPER_ADD: add new peer
 * @TDLS_OPER_UPDATE: used to update peer
 */
enum tdls_add_oper {
	TDLS_OPER_NONE,
	TDLS_OPER_ADD,
	TDLS_OPER_UPDATE
};

/**
 * enum tdls_conc_cap - tdls concurrency support
 * @TDLS_SUPPORTED_ONLY_ON_STA: only support sta tdls
 * @TDLS_SUPPORTED_ONLY_ON_P2P_CLIENT: only support p2p client tdls
 */
enum tdls_conc_cap {
	TDLS_SUPPORTED_ONLY_ON_STA = 0,
	TDLS_SUPPORTED_ONLY_ON_P2P_CLIENT,
};

/**
 * enum tdls_peer_capab - tdls capability type
 * @TDLS_CAP_NOT_SUPPORTED: tdls not supported
 * @TDLS_CAP_UNKNOWN: unknown capability
 * @TDLS_CAP_SUPPORTED: tdls capability supported
 */
enum tdls_peer_capab {
	TDLS_CAP_NOT_SUPPORTED = -1,
	TDLS_CAP_UNKNOWN       = 0,
	TDLS_CAP_SUPPORTED     = 1,
};

/**
 * enum tdls_peer_state - tdls peer state
 * @TDLS_PEER_STATE_PEERING: tdls connection in progress
 * @TDLS_PEER_STATE_CONNECTED: tdls peer is connected
 * @TDLS_PEER_STATE_TEARDOWN: tdls peer is tear down
 * @TDLS_PEER_ADD_MAC_ADDR: add peer mac into connection table
 * @TDLS_PEER_REMOVE_MAC_ADDR: remove peer mac from connection table
 */
enum tdls_peer_state {
	TDLS_PEER_STATE_PEERING,
	TDLS_PEER_STATE_CONNECTED,
	TDLS_PEER_STATE_TEARDOWN,
	TDLS_PEER_ADD_MAC_ADDR,
	TDLS_PEER_REMOVE_MAC_ADDR
};

/**
 * enum tdls_link_state - tdls link state
 * @TDLS_LINK_IDLE: tdls link idle
 * @TDLS_LINK_DISCOVERING: tdls link discovering
 * @TDLS_LINK_DISCOVERED: tdls link discovered
 * @TDLS_LINK_CONNECTING: tdls link connecting
 * @TDLS_LINK_CONNECTED: tdls link connected
 * @TDLS_LINK_TEARING: tdls link tearing
 */
enum tdls_link_state {
	TDLS_LINK_IDLE = 0,
	TDLS_LINK_DISCOVERING,
	TDLS_LINK_DISCOVERED,
	TDLS_LINK_CONNECTING,
	TDLS_LINK_CONNECTED,
	TDLS_LINK_TEARING,
};

/**
 * enum tdls_link_state_reason - tdls link reason
 * @TDLS_LINK_SUCCESS: Success
 * @TDLS_LINK_UNSPECIFIED: Unspecified reason
 * @TDLS_LINK_NOT_SUPPORTED: Remote side doesn't support TDLS
 * @TDLS_LINK_UNSUPPORTED_BAND: Remote side doesn't support this band
 * @TDLS_LINK_NOT_BENEFICIAL: Going to AP is better than direct
 * @TDLS_LINK_DROPPED_BY_REMOTE: Remote side doesn't want it anymore
 */
enum tdls_link_state_reason {
	TDLS_LINK_SUCCESS,
	TDLS_LINK_UNSPECIFIED         = -1,
	TDLS_LINK_NOT_SUPPORTED       = -2,
	TDLS_LINK_UNSUPPORTED_BAND    = -3,
	TDLS_LINK_NOT_BENEFICIAL      = -4,
	TDLS_LINK_DROPPED_BY_REMOTE   = -5,
};

/**
 * enum tdls_feature_mode - TDLS support mode
 * @TDLS_SUPPORT_DISABLED: Disabled in ini or FW
 * @TDLS_SUPPORT_SUSPENDED: TDLS supported by ini and FW, but disabled
 *            temporarily due to off-channel operations or due to other reasons
 * @TDLS_SUPPORT_EXP_TRIG_ONLY: Explicit trigger mode
 * @TDLS_SUPPORT_IMP_MODE: Implicit mode
 * @TDLS_SUPPORT_EXT_CONTROL: External control mode
 */
enum tdls_feature_mode {
	TDLS_SUPPORT_DISABLED = 0,
	TDLS_SUPPORT_SUSPENDED,
	TDLS_SUPPORT_EXP_TRIG_ONLY,
	TDLS_SUPPORT_IMP_MODE,
	TDLS_SUPPORT_EXT_CONTROL,
};

/**
 * enum tdls_command_type - TDLS command type
 * @TDLS_CMD_TX_ACTION: send tdls action frame
 * @TDLS_CMD_ADD_STA: add tdls peer
 * @TDLS_CMD_CHANGE_STA: change tdls peer
 * @TDLS_CMD_ENABLE_LINK: enable tdls link
 * @TDLS_CMD_DISABLE_LINK: disable tdls link
 * @TDLS_CMD_CONFIG_FORCE_PEER: config external peer
 * @TDLS_CMD_REMOVE_FORCE_PEER: remove external peer
 * @TDLS_CMD_STATS_UPDATE: update tdls stats
 * @TDLS_CMD_CONFIG_UPDATE: config tdls
 * @TDLS_CMD_SCAN_DONE: scon done event
 * @TDLS_CMD_SET_RESPONDER: responder event
 * @TDLS_NOTIFY_STA_CONNECTION: notify sta connection
 * @TDLS_NOTIFY_STA_DISCONNECTION: notify sta disconnection
 * @TDLS_CMD_SET_TDLS_MODE: set the tdls mode
 * @TDLS_CMD_SESSION_INCREMENT: notify session increment
 * @TDLS_CMD_SESSION_DECREMENT: notify session decrement
 * @TDLS_CMD_TEARDOWN_LINKS: notify teardown
 * @TDLS_NOTIFY_RESET_ADAPTERS: notify adapter reset
 * @TDLS_CMD_GET_ALL_PEERS: get all the tdls peers from the list
 * @TDLS_CMD_ANTENNA_SWITCH: dynamic tdls antenna switch
 * @TDLS_CMD_SET_OFFCHANNEL: tdls offchannel
 * @TDLS_CMD_SET_OFFCHANMODE: tdls offchannel mode
 * @TDLS_CMD_SET_SECOFFCHANOFFSET: tdls secondary offchannel offset
 * @TDLS_DELETE_ALL_PEERS_INDICATION: tdls delete all peers indication
 */
enum tdls_command_type {
	TDLS_CMD_TX_ACTION = 1,
	TDLS_CMD_ADD_STA,
	TDLS_CMD_CHANGE_STA,
	TDLS_CMD_ENABLE_LINK,
	TDLS_CMD_DISABLE_LINK,
	TDLS_CMD_CONFIG_FORCE_PEER,
	TDLS_CMD_REMOVE_FORCE_PEER,
	TDLS_CMD_STATS_UPDATE,
	TDLS_CMD_CONFIG_UPDATE,
	TDLS_CMD_SCAN_DONE,
	TDLS_CMD_SET_RESPONDER,
	TDLS_NOTIFY_STA_CONNECTION,
	TDLS_NOTIFY_STA_DISCONNECTION,
	TDLS_CMD_SET_TDLS_MODE,
	TDLS_CMD_SESSION_INCREMENT,
	TDLS_CMD_SESSION_DECREMENT,
	TDLS_CMD_TEARDOWN_LINKS,
	TDLS_NOTIFY_RESET_ADAPTERS,
	TDLS_CMD_GET_ALL_PEERS,
	TDLS_CMD_ANTENNA_SWITCH,
	TDLS_CMD_SET_OFFCHANNEL,
	TDLS_CMD_SET_OFFCHANMODE,
	TDLS_CMD_SET_SECOFFCHANOFFSET,
	TDLS_DELETE_ALL_PEERS_INDICATION
};

/**
 * enum tdls_event_type - TDLS event type
 * @TDLS_EVENT_VDEV_STATE_CHANGE: umac connect/disconnect event
 * @TDLS_EVENT_MGMT_TX_ACK_CNF: tx tdls frame ack event
 * @TDLS_EVENT_RX_MGMT: rx discovery response frame
 * @TDLS_EVENT_ADD_PEER: add peer or update peer
 * @TDLS_EVENT_DEL_PEER: delete peer
 * @TDLS_EVENT_DISCOVERY_REQ: dicovery request
 * @TDLS_EVENT_TEARDOWN_REQ: teardown request
 * @TDLS_EVENT_SETUP_REQ: setup request
 * @TDLS_EVENT_TEARDOWN_LINKS_DONE: teardown completion event
 * @TDLS_EVENT_USER_CMD: tdls user command
 * @TDLS_EVENT_ANTENNA_SWITCH: antenna switch event
 */
enum tdls_event_type {
	TDLS_EVENT_VDEV_STATE_CHANGE = 0,
	TDLS_EVENT_MGMT_TX_ACK_CNF,
	TDLS_EVENT_RX_MGMT,
	TDLS_EVENT_ADD_PEER,
	TDLS_EVENT_DEL_PEER,
	TDLS_EVENT_DISCOVERY_REQ,
	TDLS_EVENT_TEARDOWN_REQ,
	TDLS_EVENT_SETUP_REQ,
	TDLS_EVENT_TEARDOWN_LINKS_DONE,
	TDLS_EVENT_USER_CMD,
	TDLS_EVENT_ANTENNA_SWITCH,
};

/**
 * enum tdls_state_t - tdls state
 * @QCA_WIFI_HAL_TDLS_DISABLED: TDLS is not enabled, or is disabled now
 * @QCA_WIFI_HAL_TDLS_ENABLED: TDLS is enabled, but not yet tried
 * @QCA_WIFI_HAL_TDLS_ESTABLISHED: Direct link is established
 * @QCA_WIFI_HAL_TDLS_ESTABLISHED_OFF_CHANNEL: Direct link established using MCC
 * @QCA_WIFI_HAL_TDLS_DROPPED: Direct link was established, but is now dropped
 * @QCA_WIFI_HAL_TDLS_FAILED: Direct link failed
 */
enum tdls_state_t {
	QCA_WIFI_HAL_TDLS_S_DISABLED = 1,
	QCA_WIFI_HAL_TDLS_S_ENABLED,
	QCA_WIFI_HAL_TDLS_S_ESTABLISHED,
	QCA_WIFI_HAL_TDLS_S_ESTABLISHED_OFF_CHANNEL,
	QCA_WIFI_HAL_TDLS_S_DROPPED,
	QCA_WIFI_HAL_TDLS_S_FAILED,
};

/**
 * enum tdls_off_chan_mode - mode for WMI_TDLS_SET_OFFCHAN_MODE_CMDID
 * @TDLS_ENABLE_OFFCHANNEL: enable off channel
 * @TDLS_DISABLE_OFFCHANNEL: disable off channel
 */
enum tdls_off_chan_mode {
	TDLS_ENABLE_OFFCHANNEL,
	TDLS_DISABLE_OFFCHANNEL
};

/**
 * enum tdls_event_msg_type - TDLS event message type
 * @TDLS_SHOULD_DISCOVER: should do discover for peer (based on tx bytes per
 * second > tx_discover threshold)
 * @TDLS_SHOULD_TEARDOWN: recommend teardown the link for peer due to tx bytes
 * per second below tx_teardown_threshold
 * @TDLS_PEER_DISCONNECTED: tdls peer disconnected
 * @TDLS_CONNECTION_TRACKER_NOTIFY: TDLS/BT role change notification for
 * connection tracker
 */
enum tdls_event_msg_type {
	TDLS_SHOULD_DISCOVER = 0,
	TDLS_SHOULD_TEARDOWN,
	TDLS_PEER_DISCONNECTED,
	TDLS_CONNECTION_TRACKER_NOTIFY
};

/**
 * enum tdls_event_reason - TDLS event reason
 * @TDLS_TEARDOWN_TX: tdls teardown recommended due to low transmits
 * @TDLS_TEARDOWN_RSSI: tdls link tear down recommended due to poor RSSI
 * @TDLS_TEARDOWN_SCAN: tdls link tear down recommended due to offchannel scan
 * @TDLS_TEARDOWN_PTR_TIMEOUT: tdls peer disconnected due to PTR timeout
 * @TDLS_TEARDOWN_BAD_PTR: tdls peer disconnected due wrong PTR format
 * @TDLS_TEARDOWN_NO_RSP: tdls peer not responding
 * @TDLS_DISCONNECTED_PEER_DELETE: tdls peer disconnected due to peer deletion
 * @TDLS_PEER_ENTER_BUF_STA: tdls entered buffer STA role, TDLS connection
 * tracker needs to handle this
 * @TDLS_PEER_EXIT_BUF_STA: tdls exited buffer STA role, TDLS connection tracker
 * needs to handle this
 * @TDLS_ENTER_BT_BUSY: BT entered busy mode, TDLS connection tracker needs to
 * handle this
 * @TDLS_EXIT_BT_BUSY: BT exited busy mode, TDLS connection tracker needs to
 * handle this
 * @DLS_SCAN_STARTED: TDLS module received a scan start event, TDLS connection
 * tracker needs to handle this
 * @TDLS_SCAN_COMPLETED: TDLS module received a scan complete event, TDLS
 * connection tracker needs to handle this
 */
enum tdls_event_reason {
	TDLS_TEARDOWN_TX,
	TDLS_TEARDOWN_RSSI,
	TDLS_TEARDOWN_SCAN,
	TDLS_TEARDOWN_PTR_TIMEOUT,
	TDLS_TEARDOWN_BAD_PTR,
	TDLS_TEARDOWN_NO_RSP,
	TDLS_DISCONNECTED_PEER_DELETE,
	TDLS_PEER_ENTER_BUF_STA,
	TDLS_PEER_EXIT_BUF_STA,
	TDLS_ENTER_BT_BUSY,
	TDLS_EXIT_BT_BUSY,
	TDLS_SCAN_STARTED,
	TDLS_SCAN_COMPLETED,
};

/**
 * enum tdls_disable_sources - TDLS disable sources
 * @TDLS_SET_MODE_SOURCE_USER: disable from user
 * @TDLS_SET_MODE_SOURCE_SCAN: disable during scan
 * @TDLS_SET_MODE_SOURCE_OFFCHANNEL: disable during offchannel
 * @TDLS_SET_MODE_SOURCE_BTC: disable during bluetooth
 * @TDLS_SET_MODE_SOURCE_P2P: disable during p2p
 */
enum tdls_disable_sources {
	TDLS_SET_MODE_SOURCE_USER = 0,
	TDLS_SET_MODE_SOURCE_SCAN,
	TDLS_SET_MODE_SOURCE_OFFCHANNEL,
	TDLS_SET_MODE_SOURCE_BTC,
	TDLS_SET_MODE_SOURCE_P2P,
};

/**
 * struct tdls_osif_indication - tdls indication to os if layer
 * @vdev: vdev object
 * @reason: used with teardown indication
 * @peer_mac: MAC address of the TDLS peer
 */
struct tdls_osif_indication {
	struct wlan_objmgr_vdev *vdev;
	uint16_t reason;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	QDF_STATUS status;
};

/**
 * struct tx_frame - tx frame
 * @buf: frame buffer
 * @buf_len: buffer length
 * @tx_timer: tx send timer
 */
struct tx_frame {
	uint8_t *buf;
	size_t buf_len;
	qdf_timer_t tx_timer;
};

enum tdls_configured_external_control {
	TDLS_STRICT_EXTERNAL_CONTROL = 1,
	TDLS_LIBERAL_EXTERNAL_CONTROL = 2,
};

/**
 * enum tdls_feature_bit
 * @TDLS_FEATURE_OFF_CHANNEL: tdls off channel
 * @TDLS_FEATURE_WMM: tdls wmm
 * @TDLS_FEATURE_BUFFER_STA: tdls buffer sta
 * @TDLS_FEATURE_SLEEP_STA: tdls sleep sta feature
 * @TDLS_FEATURE_SCAN: tdls scan
 * @TDLS_FEATURE_ENABLE: tdls enabled
 * @TDLS_FEAUTRE_IMPLICIT_TRIGGER: tdls implicit trigger
 * @TDLS_FEATURE_EXTERNAL_CONTROL: enforce strict tdls external control
 * @TDLS_FEATURE_LIBERAL_EXTERNAL_CONTROL: liberal external tdls control
 */
enum tdls_feature_bit {
	TDLS_FEATURE_OFF_CHANNEL,
	TDLS_FEATURE_WMM,
	TDLS_FEATURE_BUFFER_STA,
	TDLS_FEATURE_SLEEP_STA,
	TDLS_FEATURE_SCAN,
	TDLS_FEATURE_ENABLE,
	TDLS_FEAUTRE_IMPLICIT_TRIGGER,
	TDLS_FEATURE_EXTERNAL_CONTROL,
	TDLS_FEATURE_LIBERAL_EXTERNAL_CONTROL,
};

#define TDLS_IS_OFF_CHANNEL_ENABLED(flags) \
	CHECK_BIT(flags, TDLS_FEATURE_OFF_CHANNEL)
#define TDLS_IS_WMM_ENABLED(flags) \
	CHECK_BIT(flags, TDLS_FEATURE_WMM)
#define TDLS_IS_BUFFER_STA_ENABLED(flags) \
	CHECK_BIT(flags, TDLS_FEATURE_BUFFER_STA)
#define TDLS_IS_SLEEP_STA_ENABLED(flags) \
	CHECK_BIT(flags, TDLS_FEATURE_SLEEP_STA)
#define TDLS_IS_SCAN_ENABLED(flags) \
	CHECK_BIT(flags, TDLS_FEATURE_SCAN)
#define TDLS_IS_ENABLED(flags) \
	CHECK_BIT(flags, TDLS_FEATURE_ENABLE)
#define TDLS_IS_IMPLICIT_TRIG_ENABLED(flags) \
	CHECK_BIT(flags, TDLS_FEAUTRE_IMPLICIT_TRIGGER)
#define TDLS_IS_EXTERNAL_CONTROL_ENABLED(flags) \
	CHECK_BIT(flags, TDLS_FEATURE_EXTERNAL_CONTROL)
#define TDLS_IS_LIBERAL_EXTERNAL_CONTROL_ENABLED(flags) \
	CHECK_BIT(flags, TDLS_FEATURE_LIBERAL_EXTERNAL_CONTROL)

/**
 * struct tdls_user_config - TDLS user configuration
 * @tdls_tx_states_period: tdls tx states period
 * @tdls_tx_pkt_threshold: tdls tx packets threshold
 * @tdls_rx_pkt_threshold: tdls rx packets threshold
 * @tdls_max_discovery_attempt: tdls discovery max times
 * @tdls_idle_timeout: tdls idle timeout
 * @tdls_idle_pkt_threshold: tdls idle packets threshold
 * @tdls_rssi_trigger_threshold: tdls rssi trigger threshold
 * @tdls_rssi_teardown_threshold: tdls rssi tear down threshold
 * @tdls_rssi_delta: tdls rssi delta
 * @tdls_uapsd_mask: tdls uapsd mask
 * @tdls_uapsd_inactivity_time: tdls uapsd inactivity time
 * @tdls_uapsd_pti_window: tdls peer traffic indication window
 * @tdls_uapsd_ptr_timeout: tdls peer response timeout
 * @tdls_feature_flags: tdls feature flags
 * @tdls_pre_off_chan_num: tdls off channel number
 * @tdls_pre_off_chan_bw: tdls off channel bandwidth
 * @tdls_peer_kickout_threshold: sta kickout threshold for tdls peer
 * @tdls_discovery_wake_timeout: tdls discovery wake timeout
 * @delayed_trig_framint: delayed trigger frame interval
 * @tdls_vdev_nss_2g: tdls NSS setting for 2G band
 * @tdls_vdev_nss_5g: tdls NSS setting for 5G band
 * @tdls_buffer_sta_enable: tdls buffer station enable
 * @tdls_off_chan_enable: tdls off channel enable
 * @tdls_off_chan_enable_orig: original tdls off channel enable
 * @tdls_wmm_mode_enable: tdls wmm mode enable
 * @tdls_external_control: tdls external control enable
 * @tdls_implicit_trigger_enable: tdls implicit trigger enable
 * @tdls_scan_enable: tdls scan enable
 * @tdls_sleep_sta_enable: tdls sleep sta enable
 * @tdls_support_enable: tdls support enable
 */
struct tdls_user_config {
	uint32_t tdls_tx_states_period;
	uint32_t tdls_tx_pkt_threshold;
	uint32_t tdls_rx_pkt_threshold;
	uint32_t tdls_max_discovery_attempt;
	uint32_t tdls_idle_timeout;
	uint32_t tdls_idle_pkt_threshold;
	int32_t tdls_rssi_trigger_threshold;
	int32_t tdls_rssi_teardown_threshold;
	uint32_t tdls_rssi_delta;
	uint32_t tdls_uapsd_mask;
	uint32_t tdls_uapsd_inactivity_time;
	uint32_t tdls_uapsd_pti_window;
	uint32_t tdls_uapsd_ptr_timeout;
	uint32_t tdls_feature_flags;
	uint32_t tdls_pre_off_chan_num;
	uint32_t tdls_pre_off_chan_bw;
	uint32_t tdls_peer_kickout_threshold;
	uint32_t tdls_discovery_wake_timeout;
	uint32_t delayed_trig_framint;
	uint8_t tdls_vdev_nss_2g;
	uint8_t tdls_vdev_nss_5g;
	bool tdls_buffer_sta_enable;
	bool tdls_off_chan_enable;
	bool tdls_off_chan_enable_orig;
	bool tdls_wmm_mode_enable;
	uint8_t tdls_external_control;
	bool tdls_implicit_trigger_enable;
	bool tdls_scan_enable;
	bool tdls_sleep_sta_enable;
	bool tdls_support_enable;
};

/**
 * struct tdls_config_params - tdls configure paramets
 * @tdls: tdls support mode
 * @tx_period_t: tdls tx stats period
 * @tx_packet_n: tdls tx packets number threshold
 * @discovery_tries_n: tdls max discovery attempt count
 * @idle_timeout_t: tdls idle time timeout
 * @idle_packet_n: tdls idle pkt threshold
 * @rssi_trigger_threshold: tdls rssi trigger threshold, checked before setup
 * @rssi_teardown_threshold: tdls rssi teardown threshold
 * @rssi_delta: rssi delta
 */
struct tdls_config_params {
	uint32_t tdls;
	uint32_t tx_period_t;
	uint32_t tx_packet_n;
	uint32_t discovery_tries_n;
	uint32_t idle_timeout_t;
	uint32_t idle_packet_n;
	int32_t rssi_trigger_threshold;
	int32_t rssi_teardown_threshold;
	int32_t rssi_delta;
};

/**
 * struct tdls_tx_cnf: tdls tx ack
 * @vdev_id: vdev id
 * @action_cookie: frame cookie
 * @buf: frame buf
 * @buf_len: buffer length
 * @status: tx send status
 */
struct tdls_tx_cnf {
	int vdev_id;
	uint64_t action_cookie;
	void *buf;
	size_t buf_len;
	int status;
};

/**
 * struct tdls_rx_mgmt_frame - rx mgmt frame structure
 * @frame_len: frame length
 * @rx_freq: rx freq
 * @vdev_id: vdev id
 * @frm_type: frame type
 * @rx_rssi: rx rssi
 * @buf: buffer address
 */
struct tdls_rx_mgmt_frame {
	uint32_t frame_len;
	uint32_t rx_freq;
	uint32_t vdev_id;
	uint32_t frm_type;
	uint32_t rx_rssi;
	uint8_t buf[1];
};

/**
 * tdls_rx_callback() - Callback for rx mgmt frame
 * @user_data: user data associated to this rx mgmt frame.
 * @rx_frame: RX mgmt frame
 *
 * This callback will be used to give rx frames to hdd.
 *
 * Return: None
 */
typedef void (*tdls_rx_callback)(void *user_data,
	struct tdls_rx_mgmt_frame *rx_frame);

/**
 * tdls_wmm_check() - Callback for wmm info
 * @psoc: psoc object
 *
 * This callback will be used to check wmm information
 *
 * Return: true or false
 */
typedef bool (*tdls_wmm_check)(uint8_t vdev_id);


/* This callback is used to report state change of peer to wpa_supplicant */
typedef int (*tdls_state_change_callback)(const uint8_t *mac,
					  uint32_t opclass,
					  uint32_t channel,
					  uint32_t state,
					  int32_t reason, void *ctx);

/* This callback is used to report events to os_if layer */
typedef void (*tdls_evt_callback) (void *data,
				   enum tdls_event_type ev_type,
				   struct tdls_osif_indication *event);

/* This callback is used to register TDLS peer with the datapath */
typedef QDF_STATUS (*tdls_register_peer_callback)(void *userdata,
						  uint32_t vdev_id,
						  const uint8_t *mac,
						  uint8_t qos);

/* This callback is used to deregister TDLS peer from the datapath */
typedef QDF_STATUS
(*tdls_deregister_peer_callback)(void *userdata,
				 uint32_t vdev_id,
				 struct qdf_mac_addr *peer_mac);

/* This callback is used to update datapath vdev flags */
typedef QDF_STATUS
(*tdls_dp_vdev_update_flags_callback)(void *cbk_data,
				      uint8_t vdev_id,
				      uint32_t vdev_param,
				      bool is_link_up);

/* This callback is to release vdev ref for tdls offchan param related msg */
typedef void (*tdls_offchan_parms_callback)(struct wlan_objmgr_vdev *vdev);

/**
 * tdls_vdev_init_cb() - Callback for initializing the tdls private structure
 * @vdev: vdev object
 *
 * This callback will be used to create the vdev private object and store
 * in os_priv.
 *
 * Return: QDF_STATUS
 */
typedef QDF_STATUS (*tdls_vdev_init_cb)(struct wlan_objmgr_vdev *vdev);
/**
 * tdls_vdev_deinit_cb() - Callback for deinitializing the tdls private struct
 * @vdev: vdev object
 *
 * This callback will be used to destroy the vdev private object.
 *
 * Return: None
 */
typedef void (*tdls_vdev_deinit_cb)(struct wlan_objmgr_vdev *vdev);

/**
 * struct tdls_start_params - tdls start params
 * @config: tdls user config
 * @tdls_send_mgmt_req: pass eWNI_SME_TDLS_SEND_MGMT_REQ value
 * @tdls_add_sta_req: pass eWNI_SME_TDLS_ADD_STA_REQ value
 * @tdls_del_sta_req: pass eWNI_SME_TDLS_DEL_STA_REQ value
 * @tdls_update_peer_state: pass WMA_UPDATE_TDLS_PEER_STATE value
 * @tdls_del_all_peers: pass eWNI_SME_DEL_ALL_TDLS_PEERS
 * @tdls_update_dp_vdev_flags: pass CDP_UPDATE_TDLS_FLAGS
 * @tdls_event_cb: tdls event callback
 * @tdls_evt_cb_data: tdls event data
 * @tdls_peer_context: userdata for register/deregister TDLS peer
 * @tdls_reg_peer: register tdls peer with datapath
 * @tdls_dp_vdev_update: update vdev flags in datapath
 * @tdls_osif_init_cb: callback to initialize the tdls priv
 * @tdls_osif_deinit_cb: callback to deinitialize the tdls priv
 */
struct tdls_start_params {
	struct tdls_user_config config;
	uint16_t tdls_send_mgmt_req;
	uint16_t tdls_add_sta_req;
	uint16_t tdls_del_sta_req;
	uint16_t tdls_update_peer_state;
	uint16_t tdls_del_all_peers;
	uint32_t tdls_update_dp_vdev_flags;
	tdls_rx_callback tdls_rx_cb;
	void *tdls_rx_cb_data;
	tdls_wmm_check tdls_wmm_cb;
	void *tdls_wmm_cb_data;
	tdls_evt_callback tdls_event_cb;
	void *tdls_evt_cb_data;
	void *tdls_peer_context;
	tdls_register_peer_callback tdls_reg_peer;
	tdls_dp_vdev_update_flags_callback tdls_dp_vdev_update;
	tdls_vdev_init_cb tdls_osif_init_cb;
	tdls_vdev_deinit_cb tdls_osif_deinit_cb;
};

/**
 * struct tdls_add_peer_params - add peer request parameter
 * @peer_addr: peer mac addr
 * @peer_type: peer type
 * @vdev_id: vdev id
 */
struct tdls_add_peer_params {
	uint8_t peer_addr[QDF_MAC_ADDR_SIZE];
	uint32_t peer_type;
	uint32_t vdev_id;
};

/**
 * struct tdls_add_peer_request - peer add request
 * @vdev: vdev
 * @add_peer_req: add peer request parameters
 */
struct tdls_add_peer_request {
	struct wlan_objmgr_vdev *vdev;
	struct tdls_add_peer_params add_peer_req;
};

/**
 * struct tdls_del_peer_params - delete peer request parameter
 * @peer_addr: peer mac addr
 * @peer_type: peer type
 * @vdev_id: vdev id
 */
struct tdls_del_peer_params {
	const uint8_t *peer_addr;
	uint32_t peer_type;
	uint32_t vdev_id;
};

/**
 * struct tdls_del_peer_request - peer delete request
 * @vdev: vdev
 * @del_peer_req: delete peer request parameters
 */
struct tdls_del_peer_request {
	struct wlan_objmgr_vdev *vdev;
	struct tdls_del_peer_params del_peer_req;
};

/**
 * struct vhgmcsinfo - VHT MCS information
 * @rx_mcs_map: RX MCS map 2 bits for each stream, total 8 streams
 * @rx_highest: Indicates highest long GI VHT PPDU data rate
 *      STA can receive. Rate expressed in units of 1 Mbps.
 *      If this field is 0 this value should not be used to
 *      consider the highest RX data rate supported.
 * @tx_mcs_map: TX MCS map 2 bits for each stream, total 8 streams
 * @tx_highest: Indicates highest long GI VHT PPDU data rate
 *      STA can transmit. Rate expressed in units of 1 Mbps.
 *      If this field is 0 this value should not be used to
 *      consider the highest TX data rate supported.
 */
struct vhtmcsinfo {
	uint16_t rx_mcs_map;
	uint16_t rx_highest;
	uint16_t tx_mcs_map;
	uint16_t tx_highest;
};

/**
 * struct vhtcap - VHT capabilities
 *
 * This structure is the "VHT capabilities element" as
 * described in 802.11ac D3.0 8.4.2.160
 * @vht_cap_info: VHT capability info
 * @supp_mcs: VHT MCS supported rates
 */
struct vhtcap {
	uint32_t vht_capinfo;
	struct vhtmcsinfo supp_mcs;
};

/**
 * struct hecap - HE capabilities
 *
 * This structure is the "HE capabilities element" as
 * described in 802.11ax D4.0 section 9.4.2.232.3
 * @mac_cap_info: MAC capability info
 * @phycap_info: Phy Capability info
 */
#ifdef WLAN_FEATURE_11AX
struct hecap {
	uint8_t mac_cap_info[6];
	uint8_t phycap_info[11];
	struct {
		uint16_t rx_he_mcs_map_lt_80;
		uint16_t tx_he_mcs_map_lt_80;
		uint16_t rx_he_mcs_map_160;
		uint16_t tx_he_mcs_map_160;
		uint16_t rx_he_mcs_map_80_80;
		uint16_t tx_he_mcs_map_80_80;
	} he_cap_mcs_info;
} qdf_packed;

struct hecap_6ghz {
	/* Minimum MPDU Start Spacing B0..B2
	 * Maximum A-MPDU Length Exponent B3..B5
	 * Maximum MPDU Length B6..B7 */
	uint8_t a_mpdu_params; /* B0..B7 */
	uint8_t info; /* B8..B15 */
};

#endif

struct tdls_update_peer_params {
	uint8_t peer_addr[QDF_MAC_ADDR_SIZE];
	uint32_t peer_type;
	uint32_t vdev_id;
	uint16_t capability;
	uint8_t extn_capability[WLAN_MAC_MAX_EXTN_CAP];
	uint8_t supported_rates_len;
	uint8_t supported_rates[WLAN_MAC_MAX_SUPP_RATES];
	uint8_t htcap_present;
	struct htcap_cmn_ie ht_cap;
	uint8_t vhtcap_present;
	struct vhtcap vht_cap;
#ifdef WLAN_FEATURE_11AX
	uint8_t he_cap_len;
	struct hecap he_cap;
	struct hecap_6ghz he_6ghz_cap;
#endif
	uint8_t uapsd_queues;
	uint8_t max_sp;
	uint8_t supported_channels_len;
	uint8_t supported_channels[WLAN_MAC_MAX_SUPP_CHANNELS];
	uint8_t supported_oper_classes_len;
	uint8_t supported_oper_classes[WLAN_MAX_SUPP_OPER_CLASSES];
	bool is_qos_wmm_sta;
	bool is_pmf;
};

struct tdls_update_peer_request {
	struct wlan_objmgr_vdev *vdev;
	struct tdls_update_peer_params update_peer_req;
};

/**
 * struct tdls_oper_request - tdls operation request
 * @vdev: vdev object
 * @peer_addr: MAC address of the TDLS peer
 */
struct tdls_oper_request {
	struct wlan_objmgr_vdev *vdev;
	uint8_t peer_addr[QDF_MAC_ADDR_SIZE];
};

/**
 * struct tdls_oper_config_force_peer_request - tdls enable force peer request
 * @vdev: vdev object
 * @peer_addr: MAC address of the TDLS peer
 * @chan: channel
 * @max_latency: maximum latency
 * @op_class: operation class
 * @min_bandwidth: minimal bandwidth
 * @callback: state change callback
 */
struct tdls_oper_config_force_peer_request {
	struct wlan_objmgr_vdev *vdev;
	uint8_t peer_addr[QDF_MAC_ADDR_SIZE];
	uint32_t chan;
	uint32_t max_latency;
	uint32_t op_class;
	uint32_t min_bandwidth;
	tdls_state_change_callback callback;
};

/**
 * struct tdls_info - tdls info
 *
 * @vdev_id: vdev id
 * @tdls_state: tdls state
 * @notification_interval_ms: notification interval in ms
 * @tx_discovery_threshold: tx discovery threshold
 * @tx_teardown_threshold: tx teardown threshold
 * @rssi_teardown_threshold: rx teardown threshold
 * @rssi_delta: rssi delta
 * @tdls_options: tdls options
 * @peer_traffic_ind_window: peer traffic indication window
 * @peer_traffic_response_timeout: peer traffic response timeout
 * @puapsd_mask: puapsd mask
 * @puapsd_inactivity_time: puapsd inactivity time
 * @puapsd_rx_frame_threshold: puapsd rx frame threshold
 * @teardown_notification_ms: tdls teardown notification interval
 * @tdls_peer_kickout_threshold: tdls packets threshold
 *    for peer kickout operation
 * @tdls_discovery_wake_timeout: tdls discovery wake timeout
 */
struct tdls_info {
	uint32_t vdev_id;
	uint32_t tdls_state;
	uint32_t notification_interval_ms;
	uint32_t tx_discovery_threshold;
	uint32_t tx_teardown_threshold;
	int32_t rssi_teardown_threshold;
	int32_t rssi_delta;
	uint32_t tdls_options;
	uint32_t peer_traffic_ind_window;
	uint32_t peer_traffic_response_timeout;
	uint32_t puapsd_mask;
	uint32_t puapsd_inactivity_time;
	uint32_t puapsd_rx_frame_threshold;
	uint32_t teardown_notification_ms;
	uint32_t tdls_peer_kickout_threshold;
	uint32_t tdls_discovery_wake_timeout;
};

/**
 * struct tdls_ch_params - channel parameters
 * @chan_id: ID of the channel
 * @pwr: power level
 * @dfs_set: is dfs supported or not
 * @half_rate: is the channel operating at 10MHz
 * @quarter_rate: is the channel operating at 5MHz
 */
struct tdls_ch_params {
	uint8_t chan_id;
	uint8_t pwr;
	bool dfs_set;
	bool half_rate;
	bool quarter_rate;
};

/**
 * struct tdls_peer_params - TDLS peer capablities parameters
 * @is_peer_responder: is peer responder or not
 * @peer_uapsd_queue: peer uapsd queue
 * @peer_max_sp: peer max SP value
 * @peer_buff_sta_support: peer buffer sta supported or not
 * @peer_off_chan_support: peer offchannel support
 * @peer_curr_operclass: peer current operating class
 * @self_curr_operclass: self current operating class
 * @peer_chanlen: peer channel length
 * @peer_chan: peer channel list
 * @peer_oper_classlen: peer operating class length
 * @peer_oper_class: peer operating class
 * @pref_off_channum: preferred offchannel number
 * @pref_off_chan_bandwidth: peer offchannel bandwidth
 * @opclass_for_prefoffchan: operating class for offchannel
 * @pref_offchan_freq: preferred offchannel frequency
 */
struct tdls_peer_params {
	uint8_t is_peer_responder;
	uint8_t peer_uapsd_queue;
	uint8_t peer_max_sp;
	uint8_t peer_buff_sta_support;
	uint8_t peer_off_chan_support;
	uint8_t peer_curr_operclass;
	uint8_t self_curr_operclass;
	uint8_t peer_chanlen;
	struct tdls_ch_params peer_chan[WLAN_MAC_WMI_MAX_SUPP_CHANNELS];
	uint8_t peer_oper_classlen;
	uint8_t peer_oper_class[WLAN_MAX_SUPP_OPER_CLASSES];
	uint8_t pref_off_channum;
	uint8_t pref_off_chan_bandwidth;
	uint8_t opclass_for_prefoffchan;
	uint32_t pref_offchan_freq;
};

/**
 * struct tdls_peer_update_state - TDLS peer state parameters
 * @vdev_id: vdev id
 * @peer_macaddr: peer mac address
 * @peer_cap: peer capabality
 * @resp_reqd: response needed
 */
struct tdls_peer_update_state {
	uint32_t vdev_id;
	uint8_t peer_macaddr[QDF_MAC_ADDR_SIZE];
	uint32_t peer_state;
	struct tdls_peer_params peer_cap;
	bool resp_reqd;
};

/**
 * struct tdls_channel_switch_params - channel switch parameter structure
 * @vdev_id: vdev ID
 * @peer_mac_addr: Peer mac address
 * @tdls_off_ch_bw_offset: Target off-channel bandwitdh offset
 * @tdls_off_ch: Target Off Channel
 * @oper_class: Operating class for target channel
 * @is_responder: Responder or initiator
 * @tdls_off_chan_freq: Target Off Channel frequency
 */
struct tdls_channel_switch_params {
	uint32_t    vdev_id;
	uint8_t     peer_mac_addr[QDF_MAC_ADDR_SIZE];
	uint8_t    tdls_off_ch_bw_offset;
	uint8_t     tdls_off_ch;
	uint8_t     tdls_sw_mode;
	uint8_t     oper_class;
	uint8_t     is_responder;
	uint32_t    tdls_off_chan_freq;
};

/**
 * enum uapsd_access_cat - U-APSD Access Categories
 * @UAPSD_AC_BE: best effort
 * @UAPSD_AC_BK: back ground
 * @UAPSD_AC_VI: video
 * @UAPSD_AC_VO: voice
 */
enum uapsd_access_cat {
	UAPSD_AC_BE,
	UAPSD_AC_BK,
	UAPSD_AC_VI,
	UAPSD_AC_VO
};

/**
 * enum tspec_dir_type - TSPEC Direction type
 * @TX_DIR: uplink
 * @RX_DIR: downlink
 * @BI_DIR: bidirectional
 */
enum tspec_dir_type {
	TX_DIR = 0,
	RX_DIR = 1,
	BI_DIR = 2,
};

/**
 * struct tdls_event_info - firmware tdls event
 * @vdev_id: vdev id
 * @peermac: peer mac address
 * @message_type: message type
 * @peer_reason: reason
 */
struct tdls_event_info {
	uint8_t vdev_id;
	struct qdf_mac_addr peermac;
	uint16_t message_type;
	uint32_t peer_reason;
};

/**
 * struct tdls_event_notify - tdls event notify
 * @vdev: vdev object
 * @event: tdls event
 */
struct tdls_event_notify {
	struct wlan_objmgr_vdev *vdev;
	struct tdls_event_info event;
};

/**
 * struct tdls_event_notify - tdls event notify
 * @peer_mac: peer's mac address
 * @frame_type: Type of TDLS mgmt frame to be sent
 * @dialog: dialog token used in the frame.
 * @status_code: status to be incuded in the frame
 * @responder: Tdls request type
 * @peer_capability: peer cpabilities
 * @len: length of additional Ies
 * @buf: additional IEs to be included
 */
struct tdls_send_mgmt {
	struct qdf_mac_addr peer_mac;
	uint8_t frame_type;
	uint8_t dialog;
	uint16_t status_code;
	uint8_t responder;
	uint32_t peer_capability;
	uint8_t len;
	/* Variable length, do not add anything after this */
	uint8_t buf[];
};

/**
 * struct tdls_validate_action_req - tdls validate mgmt request
 * @action_code: action code
 * @peer_mac: peer mac address
 * @dialog_token: dialog code
 * @status_code: status code to add
 * @len: len of the frame
 * @responder: whether to respond or not
 */
struct tdls_validate_action_req {
	uint8_t action_code;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	uint8_t dialog_token;
	uint8_t status_code;
	size_t len;
	int responder;
};

/**
 * struct tdls_get_all_peers - get all peers from the list
 * @vdev: vdev object
 * @buf: output string buffer to hold the peer info
 * @buf_len: the size of output string buffer
 */
struct tdls_get_all_peers {
	struct wlan_objmgr_vdev *vdev;
	char *buf;
	int buf_len;
};

/**
 * struct tdls_send_action_frame_request - tdls send mgmt request
 * @vdev: vdev object
 * @chk_frame: This struct used to validate mgmt frame
 * @session_id: session id
 * @vdev_id: vdev id
 * @cmd_buf: cmd buffer
 * @len: length of the frame
 * @use_default_ac: access category
 * @tdls_mgmt: tdls management
 */
struct tdls_action_frame_request {
	struct wlan_objmgr_vdev *vdev;
	struct tdls_validate_action_req chk_frame;
	uint8_t session_id;
	uint8_t vdev_id;
	const uint8_t *cmd_buf;
	uint8_t len;
	bool use_default_ac;
	/* Variable length, do not add anything after this */
	struct tdls_send_mgmt tdls_mgmt;
};

/**
 * struct tdls_set_responder_req - tdls set responder in peer
 * @vdev: vdev object
 * @peer_mac: peer mac address
 * @responder: whether to respond or not
 */
struct tdls_set_responder_req {
	struct wlan_objmgr_vdev *vdev;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	uint8_t responder;
};

/**
 * struct tdls_sta_notify_params - STA connection notify info
 * @vdev: vdev object
 * @tdls_prohibited: peer mac addr
 * @tdls_chan_swit_prohibited: peer type
 * @lfr_roam: is trigger due to lfr
 * @session_id: session id
 */
struct tdls_sta_notify_params {
	struct wlan_objmgr_vdev *vdev;
	bool tdls_prohibited;
	bool tdls_chan_swit_prohibited;
	bool lfr_roam;
	bool user_disconnect;
	uint8_t session_id;
};

/**
 * struct tdls_delete_all_peers_params - TDLS set mode params
 * @vdev: vdev object
 */
struct tdls_delete_all_peers_params {
	struct wlan_objmgr_vdev *vdev;
};

/**
 * struct tdls_set_mode_params - TDLS set mode params
 * @vdev: vdev object
 * @tdls_mode: tdls mode to set
 * @update_last: inform to update last tdls mode
 * @source: mode change requester
 */
struct tdls_set_mode_params {
	struct wlan_objmgr_vdev *vdev;
	enum tdls_feature_mode tdls_mode;
	bool update_last;
	enum tdls_disable_sources source;
};

/**
 * struct tdls_del_all_tdls_peers - delete all tdls peers
 * @msg_type: type of message
 * @msg_len: length of message
 * @bssid: bssid of peer device
 */
struct tdls_del_all_tdls_peers {
	uint16_t msg_type;
	uint16_t msg_len;
	struct qdf_mac_addr bssid;
};

/**
 * struct tdls_antenna_switch_request - TDLS antenna switch request
 * @vdev: vdev object
 * @mode: antenna mode, 1x1 or 2x2
 */
struct tdls_antenna_switch_request {
	struct wlan_objmgr_vdev *vdev;
	uint32_t mode;
};

/**
 * struct tdls_set_offchannel - TDLS set offchannel
 * @vdev: vdev object
 * @offchannel: Updated tdls offchannel value.
 * @callback: callback to release vdev ref.
 */
struct tdls_set_offchannel {
	struct wlan_objmgr_vdev *vdev;
	uint16_t offchannel;
	tdls_offchan_parms_callback callback;
};

/**
 * struct tdls_set_offchan_mode - TDLS set offchannel mode
 * @vdev: vdev object
 * @offchan_mode: Updated tdls offchannel mode value.
 * @callback: callback to release vdev ref.
 */
struct tdls_set_offchanmode {
	struct wlan_objmgr_vdev *vdev;
	uint8_t offchan_mode;
	tdls_offchan_parms_callback callback;
};

/**
 * struct tdls_set_offchan_offset - TDLS set offchannel mode
 * @vdev: vdev object
 * @offchan_offset: Offchan offset value.
 * @callback: callback to release vdev ref.
 */
struct tdls_set_secoffchanneloffset {
	struct wlan_objmgr_vdev *vdev;
	int offchan_offset;
	tdls_offchan_parms_callback callback;
};

/**
 * enum legacy_result_code - defined to comply with tSirResultCodes, need refine
 *                           when mlme converged.
 * @legacy_result_success: success
 * @legacy_result_max: max result value
 */
enum legacy_result_code {
	legacy_result_success,
	legacy_result_max = 0x7FFFFFFF
};

/**
 * struct tdls_send_mgmt_rsp - TDLS Response struct PE --> TDLS module
 * @vdev_id: vdev id
 * @status_code: status code as tSirResultCodes
 * @psoc: soc object
 */
struct tdls_send_mgmt_rsp {
	uint8_t vdev_id;
	enum legacy_result_code status_code;
	struct wlan_objmgr_psoc *psoc;
};

/**
 * struct tdls_mgmt_tx_completion_ind - TDLS TX completion PE --> TDLS module
 * @vdev_id: vdev_id
 * @tx_complete_status: tx complete status
 * @psoc: soc object
 */
struct tdls_mgmt_tx_completion_ind {
	uint8_t vdev_id;
	uint32_t tx_complete_status;
	struct wlan_objmgr_psoc *psoc;
};

/**
 * struct tdls_add_sta_rsp - TDLS Response struct PE --> TDLS module
 * @status_code: status code as tSirResultCodes
 * @peermac: MAC address of the TDLS peer
 * @session_id: session id
 * @sta_type: sta type
 * @tdls_oper: add peer type
 * @psoc: soc object
 */
struct tdls_add_sta_rsp {
	QDF_STATUS status_code;
	struct qdf_mac_addr peermac;
	uint8_t session_id;
	uint16_t sta_type;
	enum tdls_add_oper tdls_oper;
	struct wlan_objmgr_psoc *psoc;
};

/**
 * struct tdls_del_sta_rsp - TDLS Response struct PE --> TDLS module
 * @session_id: session id
 * @status_code: status code as tSirResultCodes
 * @peermac: MAC address of the TDLS peer
 * @psoc: soc object
 */
struct tdls_del_sta_rsp {
	uint8_t session_id;
	QDF_STATUS status_code;
	struct qdf_mac_addr peermac;
	struct wlan_objmgr_psoc *psoc;
};

/*
 * struct tdls_send_mgmt_request - tdls management request
 * @message_type: type of pe message
 * @length: length of the frame.
 * @session_id: session id
 * @req_type: type of action frame
 * @dialog: dialog token used in the frame.
 * @status_code: status to be incuded in the frame.
 * @responder: tdls request type
 * @peer_capability: peer capability information
 * @bssid: bssid
 * @peer_mac: mac address of the peer
 * @add_ie: additional ie's to be included
 */
struct tdls_send_mgmt_request {
	uint16_t message_type;
	uint16_t length;
	uint8_t session_id;
	uint8_t req_type;
	uint8_t dialog;
	uint16_t status_code;
	uint8_t responder;
	uint32_t peer_capability;
	struct qdf_mac_addr bssid;
	struct qdf_mac_addr peer_mac;
	enum wifi_traffic_ac ac;
	/* Variable length. Dont add any field after this. */
	uint8_t add_ie[1];
};

/**
 * struct tdls_add_sta_req - TDLS request struct TDLS module --> PE
 * @message_type: eWNI_SME_TDLS_ADD_STA_REQ
 * @length: message length
 * @session_id: session id
 * @transaction_id: transaction id for cmd
 * @bssid: bssid
 * @tdls_oper: add peer type
 * @peermac: MAC address for TDLS peer
 * @capability: mac capability as sSirMacCapabilityInfo
 * @extn_capability: extent capability
 * @supported_rates_length: rates length
 * @supported_rates: supported rates
 * @htcap_present: ht capability present
 * @ht_cap: ht capability
 * @vhtcap_present: vht capability present
 * @vht_cap: vht capability
 * @he_cap_len: he capability length
 * @he_cap: he capability
 * @uapsd_queues: uapsd queue as sSirMacQosInfoStation
 * @max_sp: maximum service period
 */
struct tdls_add_sta_req {
	uint16_t message_type;
	uint16_t length;
	uint8_t session_id;
	uint16_t transaction_id;
	struct qdf_mac_addr bssid;
	enum tdls_add_oper tdls_oper;
	struct qdf_mac_addr peermac;
	uint16_t capability;
	uint8_t extn_capability[WLAN_MAC_MAX_EXTN_CAP];
	uint8_t supported_rates_length;
	uint8_t supported_rates[WLAN_MAC_MAX_SUPP_RATES];
	uint8_t htcap_present;
	struct htcap_cmn_ie ht_cap;
	uint8_t vhtcap_present;
	struct vhtcap vht_cap;
#ifdef WLAN_FEATURE_11AX
	uint8_t he_cap_len;
	struct hecap he_cap;
	struct hecap_6ghz he_6ghz_cap;
#endif
	uint8_t uapsd_queues;
	uint8_t max_sp;
	bool is_pmf;
};

/**
 * struct tdls_del_sta_req - TDLS Request struct TDLS module --> PE
 * @message_type: message type eWNI_SME_TDLS_DEL_STA_REQ
 * @length: message length
 * @session_id: session id
 * @transaction_id: transaction id for cmd
 * @bssid: bssid
 * @peermac: MAC address of the TDLS peer
 */
struct tdls_del_sta_req {
	uint16_t message_type;
	uint16_t length;
	uint8_t session_id;
	uint16_t transaction_id;
	struct qdf_mac_addr bssid;
	struct qdf_mac_addr peermac;
};

/**
 * struct tdls_link_teardown - TDLS link teardown struct
 * @psoc: soc object
 */
struct tdls_link_teardown {
	struct wlan_objmgr_psoc *psoc;
};

#endif
