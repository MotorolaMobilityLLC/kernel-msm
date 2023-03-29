/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

/**
 * DOC: contains declaration of common utility APIs and private structs to be
 * used in NAN modules
 */

#ifdef WLAN_FEATURE_NAN
#ifndef _WLAN_NAN_MAIN_I_H_
#define _WLAN_NAN_MAIN_I_H_

#include "qdf_types.h"
#include "qdf_status.h"
#include "nan_public_structs.h"
#include "wlan_objmgr_cmn.h"
#include "cfg_nan.h"

struct wlan_objmgr_vdev;
struct wlan_objmgr_psoc;
struct scheduler_msg;

#define nan_alert(params...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_NAN, params)
#define nan_err(params...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_NAN, params)
#define nan_warn(params...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_NAN, params)
#define nan_notice(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_NAN, params)
#define nan_info(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_NAN, params)
#define nan_debug(params...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_NAN, params)

#define nan_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_NAN, params)
#define nan_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_NAN, params)
#define nan_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_NAN, params)
#define nan_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_NAN, params)
#define nan_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_NAN, params)

/**
 * enum nan_disc_state - NAN Discovery states
 * @NAN_DISC_DISABLED: NAN Discovery is disabled
 * @NAN_DISC_ENABLE_IN_PROGRESS: NAN Discovery enable is in progress
 * @NAN_DISC_ENABLED: NAN Discovery is enabled
 * @NAN_DISC_DISABLE_IN_PROGRESS: NAN Discovery disable is in progress
 */
enum nan_disc_state {
	NAN_DISC_DISABLED,
	NAN_DISC_ENABLE_IN_PROGRESS,
	NAN_DISC_ENABLED,
	NAN_DISC_DISABLE_IN_PROGRESS,
};

/**
 * struct nan_cfg_params - NAN INI config params
 * @enable: NAN feature enable
 * @dp_enable: NAN Datapath feature enable
 * @ndi_mac_randomize: Randomize NAN datapath interface MAC
 * @ndp_inactivity_timeout: NDP inactivity timeout
 * @nan_separate_iface_support: To supports separate iface creation for NAN
 * @ndp_keep_alive_period: To configure duration of how many seconds to
 * wait to kickout peer if peer is not reachable
 * @support_mp0_discovery: To support discovery of NAN cluster with Master
 * Preference (MP) as 0 when a new device is enabling NAN
 * @max_ndp_sessions: max ndp sessions host supports
 * @max_ndi: max number of ndi host supports
 * @nan_feature_config: Bitmap to enable/disable a particular NAN feature
 *                      configuration in firmware. It's sent to firmware through
 *                      WMI_VDEV_PARAM_ENABLE_DISABLE_NAN_CONFIG_FEATURES
 * @disable_6g_nan: Disable NAN in 6GHz frequency band
 */
struct nan_cfg_params {
	bool enable;
	bool dp_enable;
	bool ndi_mac_randomize;
	uint16_t ndp_inactivity_timeout;
	bool nan_separate_iface_support;
	uint16_t ndp_keep_alive_period;
	bool support_mp0_discovery;
	uint32_t max_ndp_sessions;
	uint32_t max_ndi;
	uint32_t nan_feature_config;
	bool disable_6g_nan;
};

/**
 * struct nan_psoc_priv_obj - nan private psoc obj
 * @lock: lock to be acquired before reading or writing to object
 * @cb_obj: struct contaning callback pointers
 * @cfg_param: NAN Config parameters in INI
 * @nan_caps: NAN Target capabilities
 * @tx_ops: Tx ops registered with Target IF interface
 * @rx_ops: Rx  ops registered with Target IF interface
 * @disc_state: Present NAN Discovery state
 * @nan_social_ch_2g_freq: NAN 2G Social channel for discovery
 * @nan_social_ch_5g_freq: NAN 5G Social channel for discovery
 * @nan_disc_mac_id: MAC id used for NAN Discovery
 * @is_explicit_disable: Flag to indicate that NAN is being explicitly
 * disabled by driver or user-space
 * @ndp_request_ctx: NDP request context
 * @nan_disc_request_ctx: NAN discovery enable/disable request context
 */
struct nan_psoc_priv_obj {
	qdf_spinlock_t lock;
	struct nan_callbacks cb_obj;
	struct nan_cfg_params cfg_param;
	struct nan_tgt_caps nan_caps;
	struct wlan_nan_tx_ops tx_ops;
	struct wlan_nan_rx_ops rx_ops;
	enum nan_disc_state disc_state;
	uint32_t nan_social_ch_2g_freq;
	uint32_t nan_social_ch_5g_freq;
	uint8_t nan_disc_mac_id;
	bool is_explicit_disable;
	void *ndp_request_ctx;
	void *nan_disc_request_ctx;
};

/**
 * struct nan_vdev_priv_obj - nan private vdev obj
 * @lock: lock to be acquired before reading or writing to object
 * @state: Current state of NDP
 * @active_ndp_peers: number of active ndp peers
 * @ndp_create_transaction_id: transaction id for create req
 * @ndp_delete_transaction_id: transaction id for delete req
 * @ndi_delete_rsp_reason: reason code for ndi_delete rsp
 * @ndi_delete_rsp_status: status for ndi_delete rsp
 * @primary_peer_mac: Primary NDP Peer mac address for the vdev
 * @disable_context: Disable all NDP's operation context
 * @ndp_init_done: Flag to indicate NDP initialization complete after first peer
 *		   connection.
 * @peer_mc_addr_list: Peer multicast address list
 */
struct nan_vdev_priv_obj {
	qdf_spinlock_t lock;
	enum nan_datapath_state state;
	uint32_t active_ndp_peers;
	uint16_t ndp_create_transaction_id;
	uint16_t ndp_delete_transaction_id;
	uint32_t ndi_delete_rsp_reason;
	uint32_t ndi_delete_rsp_status;
	struct qdf_mac_addr primary_peer_mac;
	void *disable_context;
	bool ndp_init_done;
	struct qdf_mac_addr peer_mc_addr_list[MAX_NDP_SESSIONS];
};

/**
 * struct nan_peer_priv_obj - nan private peer obj
 * @lock: lock to be acquired before reading or writing to object
 * @active_ndp_sessions: number of active ndp sessions for this peer
 * @home_chan_info: Home channel info for the NDP associated with the Peer
 */
struct nan_peer_priv_obj {
	qdf_spinlock_t lock;
	uint32_t active_ndp_sessions;
	struct nan_datapath_channel_info home_chan_info;
};

/**
 * nan_release_cmd: frees resources for NAN command.
 * @in_req: pointer to msg buffer to be freed
 * @req_type: type of request
 *
 * Return: None
 */
void nan_release_cmd(void *in_req, uint32_t req_type);

/**
 * nan_scheduled_msg_handler: callback pointer to be called when scheduler
 * starts executing enqueued NAN command.
 * @msg: pointer to msg
 *
 * Return: status of operation
 */
QDF_STATUS nan_scheduled_msg_handler(struct scheduler_msg *msg);

/**
 * nan_discovery_flush_callback: callback to flush the NAN scheduler msg
 * @msg: pointer to msg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS nan_discovery_flush_callback(struct scheduler_msg *msg);

/**
 * nan_discovery_scheduled_handler: callback pointer to be called when scheduler
 * starts executing enqueued NAN command.
 * @msg: pointer to msg
 *
 * Return: status of operation
 */
QDF_STATUS nan_discovery_scheduled_handler(struct scheduler_msg *msg);

/*
 * nan_discovery_event_handler: function to process NAN events from firmware
 * @msg: message received from Target IF
 *
 * Return: status of operation
 */
QDF_STATUS nan_discovery_event_handler(struct scheduler_msg *msg);

/*
 * nan_datapath_event_handler: function to process NDP events from firmware
 * @msg: message received from Target IF
 *
 * Return: status of operation
 */
QDF_STATUS nan_datapath_event_handler(struct scheduler_msg *msg);

/*
 * nan_set_discovery_state: Attempts to set NAN Discovery state as the given one
 * @psoc: PSOC object
 * @new_state: Attempting to this NAN discovery state
 *
 * Return: status of operation
 */
QDF_STATUS nan_set_discovery_state(struct wlan_objmgr_psoc *psoc,
				   enum nan_disc_state new_state);

/*
 * nan_discovery_pre_enable: Takes steps before sending NAN Enable to Firmware
 * @psoc: PSOC object
 * @nan_ch_freq: Primary social channel for NAN Discovery
 *
 * Return: status of operation
 */
QDF_STATUS nan_discovery_pre_enable(struct wlan_objmgr_psoc *psoc,
				    uint32_t nan_ch_freq);

/*
 * nan_get_discovery_state: Returns the current NAN Discovery state
 * @psoc: PSOC object
 *
 * Return: Current NAN Discovery state
 */
enum nan_disc_state nan_get_discovery_state(struct wlan_objmgr_psoc *psoc);

/*
 * nan_is_enable_allowed: Queries whether NAN Discovery is allowed
 * @psoc: PSOC object
 * @nan_ch_freq: Possible primary social channel for NAN Discovery
 *
 * Return: True if NAN Enable is allowed on given channel, False otherwise
 */
bool nan_is_enable_allowed(struct wlan_objmgr_psoc *psoc, uint32_t nan_ch_freq);

/*
 * nan_is_disc_active: Queries whether NAN Discovery is active
 * @psoc: PSOC object
 *
 * Return: True if NAN Disc is active, False otherwise
 */
bool nan_is_disc_active(struct wlan_objmgr_psoc *psoc);

/*
 * nan_get_connection_info: Gets connection info of the NAN Discovery interface
 * @psoc: PSOC object
 * @chan: NAN Social channel to be returned
 * @mac_if: MAC ID associated with NAN Discovery
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
nan_get_connection_info(struct wlan_objmgr_psoc *psoc, uint8_t *chan,
			uint8_t *mac_id);

#endif /* _WLAN_NAN_MAIN_I_H_ */
#endif /* WLAN_FEATURE_NAN */
