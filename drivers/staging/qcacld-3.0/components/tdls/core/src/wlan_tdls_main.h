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
 * DOC: wlan_tdls_main.h
 *
 * TDLS core function declaration
 */

#if !defined(_WLAN_TDLS_MAIN_H_)
#define _WLAN_TDLS_MAIN_H_

#include <qdf_trace.h>
#include <qdf_list.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_tdls_public_structs.h>
#include <scheduler_api.h>
#include "wlan_serialization_api.h"
#include <wlan_utility.h>


/* Bit mask flag for tdls_option to FW */
#define ENA_TDLS_OFFCHAN      (1 << 0)  /* TDLS Off Channel support */
#define ENA_TDLS_BUFFER_STA   (1 << 1)  /* TDLS Buffer STA support */
#define ENA_TDLS_SLEEP_STA    (1 << 2)  /* TDLS Sleep STA support */

#define BW_20_OFFSET_BIT   0
#define BW_40_OFFSET_BIT   1
#define BW_80_OFFSET_BIT   2
#define BW_160_OFFSET_BIT  3

#define TDLS_SEC_OFFCHAN_OFFSET_0        0
#define TDLS_SEC_OFFCHAN_OFFSET_40PLUS   40
#define TDLS_SEC_OFFCHAN_OFFSET_40MINUS  (-40)
#define TDLS_SEC_OFFCHAN_OFFSET_80       80
#define TDLS_SEC_OFFCHAN_OFFSET_160      160
/*
 * Before UpdateTimer expires, we want to timeout discovery response
 * should not be more than 2000.
 */
#define TDLS_DISCOVERY_TIMEOUT_BEFORE_UPDATE     1000
#define TDLS_SCAN_REJECT_MAX            5
#define TDLS_MAX_CONNECTED_PEERS_TO_ALLOW_SCAN   1

#define tdls_debug(params...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_TDLS, params)
#define tdls_debug_rl(params...) \
	QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_TDLS, params)

#define tdls_notice(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_TDLS, params)
#define tdls_warn(params...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_TDLS, params)
#define tdls_err(params...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_TDLS, params)
#define tdls_alert(params...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_TDLS, params)

#define tdls_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_TDLS, params)
#define tdls_nofl_notice(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_TDLS, params)
#define tdls_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_TDLS, params)
#define tdls_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_TDLS, params)
#define tdls_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_TDLS, params)

#define TDLS_IS_LINK_CONNECTED(peer)  \
	((TDLS_LINK_CONNECTED == (peer)->link_status) || \
	 (TDLS_LINK_TEARING == (peer)->link_status))

#define SET_BIT(value, mask) ((value) |= (1 << (mask)))
#define CLEAR_BIT(value, mask) ((value) &= ~(1 << (mask)))
#define CHECK_BIT(value, mask) ((value) & (1 << (mask)))
/**
 * struct tdls_conn_info - TDLS connection record
 * @session_id: session id
 * @valid_entry: valid entry(set to true upon peer create resp from firmware)
 * @peer_mac: peer address
 * @index: index to store array offset.
 */
struct tdls_conn_info {
	uint8_t session_id;
	bool valid_entry;
	uint8_t index;
	struct qdf_mac_addr peer_mac;
};

/**
 * enum tdls_nss_transition_state - TDLS NSS transition states
 * @TDLS_NSS_TRANSITION_UNKNOWN: default state
 * @TDLS_NSS_TRANSITION_2x2_to_1x1: transition from 2x2 to 1x1 stream
 * @TDLS_NSS_TRANSITION_1x1_to_2x2: transition from 1x1 to 2x2 stream
 */
enum tdls_nss_transition_state {
	TDLS_NSS_TRANSITION_S_UNKNOWN = 0,
	TDLS_NSS_TRANSITION_S_2x2_to_1x1,
	TDLS_NSS_TRANSITION_S_1x1_to_2x2,
};

/**
 * struct tdls_conn_tracker_mac_table - connection tracker peer table
 * @mac_address: peer mac address
 * @tx_packet_cnt: number of tx pkts
 * @rx_packet_cnt: number of rx pkts
 * @peer_timestamp_ms: time stamp of latest peer traffic
 */
struct tdls_conn_tracker_mac_table {
	struct qdf_mac_addr mac_address;
	uint32_t tx_packet_cnt;
	uint32_t rx_packet_cnt;
	uint32_t peer_timestamp_ms;
};

/**
 * struct tdls_set_state_db - to record set tdls state command, we need to
 * set correct tdls state to firmware:
 * 1. enable tdls in firmware before tdls connection;
 * 2. disable tdls if concurrency happen, before disable tdls, all active peer
 * should be deleted in firmware.
 *
 * @set_state_cnt: tdls set state count
 * @vdev_id: vdev id of last set state command
 */
struct tdls_set_state_info {
	uint8_t set_state_cnt;
	uint8_t vdev_id;
};

/**
 * struct tdls_psoc_priv_ctx - tdls context
 * @soc: objmgr psoc
 * @tdls_current_mode: current tdls mode
 * @tdls_last_mode: last tdls mode
 * @scan_reject_count: number of times scan rejected due to TDLS
 * @tdls_source_bitmap: bit map to set/reset TDLS by different sources
 * @tdls_conn_info: this tdls_conn_info can be removed and we can use peer type
 *                of peer object to get the active tdls peers
 * @tdls_configs: tdls user configure
 * @max_num_tdls_sta: maximum TDLS station number allowed upon runtime condition
 * @connected_peer_count: tdls peer connected count
 * @tdls_off_channel: tdls off channel number
 * @tdls_channel_offset: tdls channel offset
 * @tdls_fw_off_chan_mode: tdls fw off channel mode
 * @enable_tdls_connection_tracker: enable tdls connection tracker
 * @tdls_external_peer_count: external tdls peer count
 * @tdls_nss_switch_in_progress: tdls antenna switch in progress
 * @tdls_nss_teardown_complete: tdls tear down complete
 * @tdls_disable_in_progress: tdls is disable in progress
 * @tdls_nss_transition_mode: tdls nss transition mode
 * @tdls_teardown_peers_cnt: tdls tear down peer count
 * @set_state_info: set tdls state info
 * @tdls_event_cb: tdls event callback
 * @tdls_evt_cb_data: tdls event user data
 * @tdls_peer_context: userdata for register/deregister TDLS peer
 * @tdls_reg_peer: register tdls peer with datapath
 * @tx_q_ack: queue for tx frames waiting for ack
 * @tdls_con_cap: tdls concurrency support
 * @tdls_send_mgmt_req: store eWNI_SME_TDLS_SEND_MGMT_REQ value
 * @tdls_add_sta_req: store eWNI_SME_TDLS_ADD_STA_REQ value
 * @tdls_del_sta_req: store eWNI_SME_TDLS_DEL_STA_REQ value
 * @tdls_update_peer_state: store WMA_UPDATE_TDLS_PEER_STATE value
 * @tdls_del_all_peers:store eWNI_SME_DEL_ALL_TDLS_PEERS
 * @tdls_update_dp_vdev_flags store CDP_UPDATE_TDLS_FLAGS
 * @tdls_idle_peer_data: provide information about idle peer
 * @tdls_ct_spinlock: connection tracker spin lock
 * @is_prevent_suspend: prevent suspend or not
 * @is_drv_supported: platform supports drv or not, enable/disable tdls wow
 * based on this flag.
 * @wake_lock: wake lock
 * @runtime_lock: runtime lock
 * @tdls_osif_init_cb: Callback to initialize the tdls private
 * @tdls_osif_deinit_cb: Callback to deinitialize the tdls private
 * @fw_tdls_11ax_capablity: bool for tdls 11ax fw capability
 */
struct tdls_soc_priv_obj {
	struct wlan_objmgr_psoc *soc;
	enum tdls_feature_mode tdls_current_mode;
	enum tdls_feature_mode tdls_last_mode;
	int scan_reject_count;
	unsigned long tdls_source_bitmap;
	struct tdls_conn_info tdls_conn_info[WLAN_TDLS_STA_MAX_NUM];
	struct tdls_user_config tdls_configs;
	uint16_t max_num_tdls_sta;
	uint16_t connected_peer_count;
	uint8_t tdls_off_channel;
	uint16_t tdls_channel_offset;
	int32_t tdls_fw_off_chan_mode;
	bool enable_tdls_connection_tracker;
	uint8_t tdls_external_peer_count;
	bool tdls_nss_switch_in_progress;
	bool tdls_nss_teardown_complete;
	bool tdls_disable_in_progress;
	enum tdls_nss_transition_state tdls_nss_transition_mode;
	int32_t tdls_teardown_peers_cnt;
	struct tdls_set_state_info set_state_info;
	tdls_rx_callback tdls_rx_cb;
	void *tdls_rx_cb_data;
	tdls_wmm_check tdls_wmm_cb;
	void *tdls_wmm_cb_data;
	tdls_evt_callback tdls_event_cb;
	void *tdls_evt_cb_data;
	void *tdls_peer_context;
	tdls_register_peer_callback tdls_reg_peer;
	tdls_dp_vdev_update_flags_callback tdls_dp_vdev_update;
	qdf_list_t tx_q_ack;
	enum tdls_conc_cap tdls_con_cap;
	uint16_t tdls_send_mgmt_req;
	uint16_t tdls_add_sta_req;
	uint16_t tdls_del_sta_req;
	uint16_t tdls_update_peer_state;
	uint16_t tdls_del_all_peers;
	uint32_t tdls_update_dp_vdev_flags;
	qdf_spinlock_t tdls_ct_spinlock;
#ifdef TDLS_WOW_ENABLED
	bool is_prevent_suspend;
	bool is_drv_supported;
	qdf_wake_lock_t wake_lock;
	qdf_runtime_lock_t runtime_lock;
#endif
	tdls_vdev_init_cb tdls_osif_init_cb;
	tdls_vdev_deinit_cb tdls_osif_deinit_cb;
#ifdef WLAN_FEATURE_11AX
	bool fw_tdls_11ax_capability;
#endif
};

/**
 * struct tdls_vdev_priv_obj - tdls private vdev object
 * @vdev: vdev objmgr object
 * @peer_list: tdls peer list on this vdev
 * @peer_update_timer: connection tracker timer
 * @peer_dicovery_timer: peer discovery timer
 * @threshold_config: threshold config
 * @discovery_peer_cnt: discovery peer count
 * @discovery_sent_cnt: discovery sent count
 * @curr_candidate: current candidate
 * @ct_peer_table: linear mac address table for counting the packets
 * @valid_mac_entries: number of valid mac entry in @ct_peer_mac_table
 * @magic: magic
 * @tx_queue: tx frame queue
 * @tdls_teardown_comp: tdls teardown completion
 */
struct tdls_vdev_priv_obj {
	struct wlan_objmgr_vdev *vdev;
	qdf_list_t peer_list[WLAN_TDLS_PEER_LIST_SIZE];
	qdf_mc_timer_t peer_update_timer;
	qdf_mc_timer_t peer_discovery_timer;
	struct tdls_config_params threshold_config;
	int32_t discovery_peer_cnt;
	uint32_t discovery_sent_cnt;
	struct tdls_peer *curr_candidate;
	struct tdls_conn_tracker_mac_table
			ct_peer_table[WLAN_TDLS_CT_TABLE_SIZE];
	uint8_t valid_mac_entries;
	uint32_t magic;
	uint8_t session_id;
	qdf_list_t tx_queue;
	qdf_event_t tdls_teardown_comp;
};

/**
 * struct tdls_peer_mlme_info - tdls peer mlme info
 **/
struct tdls_peer_mlme_info {
};

/**
 * struct tdls_peer - tdls peer data
 * @node: node
 * @vdev_priv: tdls vdev priv obj
 * @peer_mac: peer mac address
 * @valid_entry: entry valid or not (set to true when peer create resp is
 *               received from FW)
 * @rssi: rssi
 * @tdls_support: tdls support
 * @link_status: tdls link status
 * @is_responder: is responder
 * @discovery_processed: dicovery processed
 * @discovery_attempt: discovery attempt
 * @tx_pkt: tx packet
 * @rx_pkt: rx packet
 * @uapsd_queues: uapsd queues
 * @max_sp: max sp
 * @buf_sta_capable: is buffer sta
 * @off_channel_capable: is offchannel supported flag
 * @supported_channels_len: supported channels length
 * @supported_channels: supported channels
 * @supported_oper_classes_len: supported operation classes length
 * @supported_oper_classes: supported operation classes
 * @is_forced_peer: is forced peer
 * @op_class_for_pref_off_chan: op class for preferred off channel
 * @pref_off_chan_num: preferred off channel number
 * @peer_idle_timer: time to check idle traffic in tdls peers
 * @is_peer_idle_timer_initialised: Flag to check idle timer init
 * @spatial_streams: Number of TX/RX spatial streams for TDLS
 * @reason: reason
 * @state_change_notification: state change notification
 * @qos: QOS capability of TDLS link
 */
struct tdls_peer {
	qdf_list_node_t node;
	struct tdls_vdev_priv_obj *vdev_priv;
	struct qdf_mac_addr peer_mac;
	bool valid_entry;
	int8_t rssi;
	enum tdls_peer_capab tdls_support;
	enum tdls_link_state link_status;
	uint8_t is_responder;
	uint8_t discovery_processed;
	uint16_t discovery_attempt;
	uint16_t tx_pkt;
	uint16_t rx_pkt;
	uint8_t uapsd_queues;
	uint8_t max_sp;
	uint8_t buf_sta_capable;
	uint8_t off_channel_capable;
	uint8_t supported_channels_len;
	uint8_t supported_channels[WLAN_MAC_MAX_SUPP_CHANNELS];
	uint8_t supported_oper_classes_len;
	uint8_t supported_oper_classes[WLAN_MAX_SUPP_OPER_CLASSES];
	bool is_forced_peer;
	uint8_t op_class_for_pref_off_chan;
	uint8_t pref_off_chan_num;
	qdf_mc_timer_t peer_idle_timer;
	bool is_peer_idle_timer_initialised;
	uint8_t spatial_streams;
	enum tdls_link_state_reason reason;
	tdls_state_change_callback state_change_notification;
	uint8_t qos;
	struct tdls_peer_mlme_info *tdls_info;
};

/**
 * struct tdls_os_if_event - TDLS os event info
 * @type: type of event
 * @info: pointer to event information
 */
struct tdls_os_if_event {
	uint32_t type;
	void *info;
};

/**
 * enum tdls_os_if_notification - TDLS notification from OS IF
 * @TDLS_NOTIFY_STA_SESSION_INCREMENT: sta session count incremented
 * @TDLS_NOTIFY_STA_SESSION_DECREMENT: sta session count decremented
 */
enum tdls_os_if_notification {
	TDLS_NOTIFY_STA_SESSION_INCREMENT,
	TDLS_NOTIFY_STA_SESSION_DECREMENT
};
/**
 * wlan_vdev_get_tdls_soc_obj - private API to get tdls soc object from vdev
 * @vdev: vdev object
 *
 * Return: tdls soc object
 */
static inline struct tdls_soc_priv_obj *
wlan_vdev_get_tdls_soc_obj(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct tdls_soc_priv_obj *soc_obj;

	if (!vdev) {
		tdls_err("NULL vdev");
		return NULL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		tdls_err("can't get psoc");
		return NULL;
	}

	soc_obj = (struct tdls_soc_priv_obj *)
		wlan_objmgr_psoc_get_comp_private_obj(psoc,
						      WLAN_UMAC_COMP_TDLS);

	return soc_obj;
}

/**
 * wlan_psoc_get_tdls_soc_obj - private API to get tdls soc object from psoc
 * @psoc: psoc object
 *
 * Return: tdls soc object
 */
static inline struct tdls_soc_priv_obj *
wlan_psoc_get_tdls_soc_obj(struct wlan_objmgr_psoc *psoc)
{
	struct tdls_soc_priv_obj *soc_obj;
	if (!psoc) {
		tdls_err("NULL psoc");
		return NULL;
	}
	soc_obj = (struct tdls_soc_priv_obj *)
		wlan_objmgr_psoc_get_comp_private_obj(psoc,
						      WLAN_UMAC_COMP_TDLS);

	return soc_obj;
}

/**
 * wlan_vdev_get_tdls_vdev_obj - private API to get tdls vdev object from vdev
 * @vdev: vdev object
 *
 * Return: tdls vdev object
 */
static inline struct tdls_vdev_priv_obj *
wlan_vdev_get_tdls_vdev_obj(struct wlan_objmgr_vdev *vdev)
{
	struct tdls_vdev_priv_obj *vdev_obj;

	if (!vdev) {
		tdls_err("NULL vdev");
		return NULL;
	}

	vdev_obj = (struct tdls_vdev_priv_obj *)
		wlan_objmgr_vdev_get_comp_private_obj(vdev,
						      WLAN_UMAC_COMP_TDLS);

	return vdev_obj;
}

/**
 * tdls_set_link_status - tdls set link status
 * @vdev: vdev object
 * @mac: mac address of tdls peer
 * @link_state: tdls link state
 * @link_reason: reason
 */
void tdls_set_link_status(struct tdls_vdev_priv_obj *vdev,
			  const uint8_t *mac,
			  enum tdls_link_state link_state,
			  enum tdls_link_state_reason link_reason);
/**
 * tdls_psoc_obj_create_notification() - tdls psoc create notification handler
 * @psoc: psoc object
 * @arg_list: Argument list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_psoc_obj_create_notification(struct wlan_objmgr_psoc *psoc,
					     void *arg_list);

/**
 * tdls_psoc_obj_destroy_notification() - tdls psoc destroy notification handler
 * @psoc: psoc object
 * @arg_list: Argument list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_psoc_obj_destroy_notification(struct wlan_objmgr_psoc *psoc,
					      void *arg_list);

/**
 * tdls_vdev_obj_create_notification() - tdls vdev create notification handler
 * @vdev: vdev object
 * @arg_list: Argument list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_vdev_obj_create_notification(struct wlan_objmgr_vdev *vdev,
					     void *arg_list);

/**
 * tdls_vdev_obj_destroy_notification() - tdls vdev destroy notification handler
 * @vdev: vdev object
 * @arg_list: Argument list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_vdev_obj_destroy_notification(struct wlan_objmgr_vdev *vdev,
					      void *arg_list);

/**
 * tdls_process_cmd() - tdls main command process function
 * @msg: scheduler msg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_process_cmd(struct scheduler_msg *msg);

/**
 * tdls_process_evt() - tdls main event process function
 * @msg: scheduler msg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_process_evt(struct scheduler_msg *msg);

/**
 * tdls_timer_restart() - restart TDLS timer
 * @vdev: VDEV object manager
 * @timer: timer to restart
 * @expiration_time: new expiration time to set for the timer
 *
 * Return: Void
 */
void tdls_timer_restart(struct wlan_objmgr_vdev *vdev,
				 qdf_mc_timer_t *timer,
				 uint32_t expiration_time);

/**
 * wlan_hdd_tdls_timers_stop() - stop all the tdls timers running
 * @tdls_vdev: TDLS vdev
 *
 * Return: none
 */
void tdls_timers_stop(struct tdls_vdev_priv_obj *tdls_vdev);

/**
 * tdls_get_vdev_objects() - Get TDLS private objects
 * @vdev: VDEV object manager
 * @tdls_vdev_obj: tdls vdev object
 * @tdls_soc_obj: tdls soc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_get_vdev_objects(struct wlan_objmgr_vdev *vdev,
				   struct tdls_vdev_priv_obj **tdls_vdev_obj,
				   struct tdls_soc_priv_obj **tdls_soc_obj);

/**
 * cds_set_tdls_ct_mode() - Set the tdls connection tracker mode
 * @hdd_ctx: hdd context
 *
 * This routine is called to set the tdls connection tracker operation status
 *
 * Return: NONE
 */
void tdls_set_ct_mode(struct wlan_objmgr_psoc *psoc);

/**
 * tdls_set_operation_mode() - set tdls operating mode
 * @tdls_set_mode: tdls mode set params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_set_operation_mode(struct tdls_set_mode_params *tdls_set_mode);

/**
 * tdls_notify_sta_connect() - Update tdls state for every
 * connect event.
 * @notify: sta connect params
 *
 * After every connect event in the system, check whether TDLS
 * can be enabled in the system. If TDLS can be enabled, update the
 * TDLS state as needed.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_notify_sta_connect(struct tdls_sta_notify_params *notify);

/**
 * tdls_notify_sta_disconnect() - Update tdls state for every
 * disconnect event.
 * @notify: sta disconnect params
 *
 * After every disconnect event in the system, check whether TDLS
 * can be disabled/enabled in the system and update the
 * TDLS state as needed.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_notify_sta_disconnect(struct tdls_sta_notify_params *notify);

/**
 * tdls_notify_reset_adapter() - notify reset adapter
 * @vdev: vdev object
 *
 * Notify TDLS about the adapter reset
 *
 * Return: None
 */
void tdls_notify_reset_adapter(struct wlan_objmgr_vdev *vdev);

/**
 * tdls_peers_deleted_notification() - peer delete notification
 * @psoc: soc object
 * @vdev_id: vdev id
 *
 * Legacy lim layer will delete tdls peers for roaming and heart beat failures
 * and notify the component about the delete event to update the tdls.
 * state.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_peers_deleted_notification(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id);

/**
 * tdls_notify_decrement_session() - Notify the session decrement
 * @psoc: psoc  object manager
 *
 * Policy manager notify TDLS about session decrement
 *
 * Return: None
 */
void tdls_notify_decrement_session(struct wlan_objmgr_psoc *psoc);

/**
 * tdls_send_update_to_fw - update tdls status info
 * @tdls_vdev_obj: tdls vdev private object.
 * @tdls_prohibited: indicates whether tdls is prohibited.
 * @tdls_chan_swit_prohibited: indicates whether tdls channel switch
 *                             is prohibited.
 * @sta_connect_event: indicate sta connect or disconnect event
 * @session_id: session id
 *
 * Normally an AP does not influence TDLS connection between STAs
 * associated to it. But AP may set bits for TDLS Prohibited or
 * TDLS Channel Switch Prohibited in Extended Capability IE in
 * Assoc/Re-assoc response to STA. So after STA is connected to
 * an AP, call this function to update TDLS status as per those
 * bits set in Ext Cap IE in received Assoc/Re-assoc response
 * from AP.
 *
 * Return: None.
 */
void tdls_send_update_to_fw(struct tdls_vdev_priv_obj *tdls_vdev_obj,
			    struct tdls_soc_priv_obj *tdls_soc_obj,
			    bool tdls_prohibited,
			    bool tdls_chan_swit_prohibited,
			    bool sta_connect_event,
			    uint8_t session_id);

/**
 * tdls_notify_increment_session() - Notify the session increment
 * @psoc: psoc  object manager
 *
 * Policy manager notify TDLS about session increment
 *
 * Return: None
 */
void tdls_notify_increment_session(struct wlan_objmgr_psoc *psoc);

/**
 * tdls_check_is_tdls_allowed() - check is tdls allowed or not
 * @vdev: vdev object
 *
 * Function determines the whether TDLS allowed in the system
 *
 * Return: true or false
 */
bool tdls_check_is_tdls_allowed(struct wlan_objmgr_vdev *vdev);

/**
 * tdls_get_vdev() - Get tdls specific vdev object manager
 * @psoc: wlan psoc object manager
 * @dbg_id: debug id
 *
 * If TDLS possible, return the corresponding vdev
 * to enable TDLS in the system.
 *
 * Return: vdev manager pointer or NULL.
 */
struct wlan_objmgr_vdev *tdls_get_vdev(struct wlan_objmgr_psoc *psoc,
					  wlan_objmgr_ref_dbgid dbg_id);

/**
 * tdls_process_policy_mgr_notification() - process policy manager notification
 * @psoc: soc object manager
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tdls_process_policy_mgr_notification(struct wlan_objmgr_psoc *psoc);

/**
 * tdls_process_decrement_active_session() - process policy manager decrement
 * sessions.
 * @psoc: soc object manager
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tdls_process_decrement_active_session(struct wlan_objmgr_psoc *psoc);
/**
 * tdls_scan_complete_event_handler() - scan complete event handler for tdls
 * @vdev: vdev object
 * @event: scan event
 * @arg: tdls soc object
 *
 * Return: None
 */
void tdls_scan_complete_event_handler(struct wlan_objmgr_vdev *vdev,
			struct scan_event *event,
			void *arg);

/**
 * tdls_scan_callback() - callback for TDLS scan operation
 * @soc: tdls soc pvt object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_scan_callback(struct tdls_soc_priv_obj *tdls_soc);

/**
 * wlan_hdd_tdls_scan_done_callback() - callback for tdls scan done event
 * @tdls_soc: tdls soc object
 *
 * Return: Void
 */
void tdls_scan_done_callback(struct tdls_soc_priv_obj *tdls_soc);

/**
 * tdls_scan_serialization_comp_info_cb() - callback for scan start
 * @vdev: VDEV on which the scan command is being processed
 * @comp_info: serialize rules info
 *
 * Return: negative = caller should stop and return error code immediately
 *         1 = caller can continue to scan
 */
void tdls_scan_serialization_comp_info_cb(struct wlan_objmgr_vdev *vdev,
		union wlan_serialization_rules_info *comp_info,
		struct wlan_serialization_command *cmd);

/**
 * tdls_set_offchan_mode() - update tdls status info
 * @psoc: soc object
 * @param: channel switch params
 *
 * send message to WMI to set TDLS off channel in f/w
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS tdls_set_offchan_mode(struct wlan_objmgr_psoc *psoc,
				     struct tdls_channel_switch_params *param);

/**
 * tdls_delete_all_peers_indication() - update tdls status info
 * @psoc: soc object
 * @vdev_id: vdev id
 *
 * Notify tdls component to cleanup all peers
 *
 * Return: QDF_STATUS.
 */

QDF_STATUS tdls_delete_all_peers_indication(struct wlan_objmgr_psoc *psoc,
					    uint8_t vdev_id);

/**
 * tdls_get_opclass_from_bandwidth() - Return opclass for corresponding BW and
 * channel.
 * @soc_obj: tdls soc object.
 * @channel: Channel number.
 * @bw_offset: Bandwidth offset.
 * @reg_bw_offset: enum offset_t type bandwidth
 *
 * To return the opclas.
 *
 * Return: opclass
 */
uint8_t tdls_get_opclass_from_bandwidth(struct tdls_soc_priv_obj *soc_obj,
					uint8_t channel, uint8_t bw_offset,
					uint8_t *reg_bw_offset);
#endif
