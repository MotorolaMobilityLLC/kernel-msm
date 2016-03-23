/*
 * Copyright (c) 2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
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
 * DOC: wlan_hdd_nan_datapath.h
 *
 * WLAN Host Device Driver nan datapath API specification
 */
#ifndef __WLAN_HDD_NAN_DATAPATH_H
#define __WLAN_HDD_NAN_DATAPATH_H

struct hdd_context_s;
struct hdd_tgt_cfg;
struct hdd_config;
struct hdd_adapter_s;
struct wireless_dev;

/* NAN Social channels */
#define NAN_SOCIAL_CHANNEL_2_4GHZ 6
#define NAN_SOCIAL_CHANNEL_5GHZ_LOWER_BAND 44
#define NAN_SOCIAL_CHANNEL_5GHZ_UPPER_BAND 149

#define NDP_APP_INFO_LEN 255
#define NDP_QOS_INFO_LEN 255

#ifdef WLAN_FEATURE_NAN_DATAPATH
#define WLAN_HDD_NDP_GET_SSID(adapter) ( \
	&(adapter)->sessionCtx.ndp_ctx.conn_info.SSID)

#define WLAN_HDD_NDP_GET_BSSID(adapter) ( \
	&(adapter)->sessionCtx.ndp_ctx.conn_info.bssId)
#else
#define WLAN_HDD_NDP_GET_SSID(adapter)  (NULL)
#define WLAN_HDD_NDP_GET_BSSID(adapter) (NULL)
#endif /* WLAN_FEATURE_NAN_DATAPATH */

/**
 * enum qca_wlan_vendor_attr_ndp_params - vendor attribute parameters
 * @QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD: NDP Sub command
 * @QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID: Transaction id reference
 * @QCA_WLAN_VENDOR_ATTR_NDP_STATUS_ID: NDP status id
 * @QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID: Service instance id
 * @QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_SPEC_CHANNEL: Requested channel
 * @QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR: Peer discovery mac addr
 * @QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR: Iface name
 * @QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_SECURITY: Security configuration
 * @QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS: Qos configuration
 * @QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO_LEN: Application info length
 * @QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO: Application info
 * @QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID: NDP instance id
 * @QCA_WLAN_VENDOR_ATTR_NDP_NUM_INSTANCE_ID: Number of NDP instance ids
 * @QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY: NDP instance id array
 * @QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_RESPONSE_CODE: Schedule response
 * @QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_STATUS_CODE: schedule status
 * @QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR: NDI mac address
 * @QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_TYPE: Driver return status
 * @QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE: Driver return value
 */
enum qca_wlan_vendor_attr_ndp_params {
	QCA_WLAN_VENDOR_ATTR_NDP_PARAM_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
	QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
	QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID,
	QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_SPEC_CHANNEL,
	QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR,
	QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR,
	QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_SECURITY,
	QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS,
	QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO_LEN,
	QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO,
	QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID,
	QCA_WLAN_VENDOR_ATTR_NDP_NUM_INSTANCE_ID,
	QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY,
	QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_RESPONSE_CODE,
	QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_STATUS_CODE,
	QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR,
	QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_TYPE,
	QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,

	QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX =
		QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ndp_cfg_security - vendor security attribute
 * @QCA_WLAN_VENDOR_ATTR_NDP_SECURITY_ENABLE: Security enabled
 */
enum qca_wlan_vendor_attr_ndp_cfg_security {
	QCA_WLAN_VENDOR_ATTR_NDP_SECURITY_ENABLE = 1,
};

/**
 * enum qca_wlan_vendor_attr_ndp_qos - vendor qos attribute
 * @QCA_WLAN_VENDOR_ATTR_NDP_QOS_CONFIG: NDP QoS configuration
 */
enum qca_wlan_vendor_attr_ndp_qos {
	QCA_WLAN_VENDOR_ATTR_NDP_QOS_CONFIG = 1,
};

/**
 * enum qca_wlan_vendor_attr_ndp_sub_cmd_value - NDP subcmd value
 * @QCA_WLAN_VENDOR_ATTR_NDP_INVALID: Unused subcmd value
 * @QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE: iface create
 * @QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE: iface delete
 * @QCA_WLAN_VENDOR_ATTR_NDP_INITIATOR_REQUEST: NDP initiator request
 * @QCA_WLAN_VENDOR_ATTR_NDP_INITIATOR_RESPONSE: NDP initiator response
 * @QCA_WLAN_VENDOR_ATTR_NDP_RESPONDER_REQUEST: NDP responder request
 * @QCA_WLAN_VENDOR_ATTR_NDP_RESPONDER_RESPONSE: NDP responder response
 * @QCA_WLAN_VENDOR_ATTR_NDP_END_REQUEST: NDP end request
 * @QCA_WLAN_VENDOR_ATTR_NDP_END_RESPONSE: NDP end response
 * @QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_REQUEST: NDP update request
 * @QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_RESPONSE: NDP update response
 * @QCA_WLAN_VENDOR_ATTR_NDP_REQUEST_IND: NDP request indication
 * @QCA_WLAN_VENDOR_ATTR_NDP_CONFIRM_IND: NDP confirm indication
 * @QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_IND: NDP sched update indication
 * @QCA_WLAN_VENDOR_ATTR_NDP_END_IND: NDP End indication
 */
enum qca_wlan_vendor_attr_ndp_sub_cmd_value {
	QCA_WLAN_VENDOR_ATTR_NDP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE = 1,
	QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE = 2,
	QCA_WLAN_VENDOR_ATTR_NDP_INITIATOR_REQUEST = 3,
	QCA_WLAN_VENDOR_ATTR_NDP_INITIATOR_RESPONSE = 4,
	QCA_WLAN_VENDOR_ATTR_NDP_RESPONDER_REQUEST = 5,
	QCA_WLAN_VENDOR_ATTR_NDP_RESPONDER_RESPONSE = 6,
	QCA_WLAN_VENDOR_ATTR_NDP_END_REQUEST = 7,
	QCA_WLAN_VENDOR_ATTR_NDP_END_RESPONSE = 8,
	QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_REQUEST = 9,
	QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_RESPONSE = 10,
	QCA_WLAN_VENDOR_ATTR_NDP_REQUEST_IND = 11,
	QCA_WLAN_VENDOR_ATTR_NDP_CONFIRM_IND = 12,
	QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_IND = 13,
	QCA_WLAN_VENDOR_ATTR_NDP_END_IND = 14
};

/** enum nan_datapath_state - NAN datapath states
 * @NAN_DATA_NDI_CREATING_STATE: NDI create is in progress
 * @NAN_DATA_NDI_CREATED_STATE: NDI successfully crated
 * @NAN_DATA_NDI_DELETING_STATE: NDI delete is in progress
 * @NAN_DATA_NDI_DELETED_STATE: NDI delete is in progress
 * @NAN_DATA_PEER_CREATE_STATE: Peer create is in progress
 * @NAN_DATA_PEER_DELETE_STATE: Peer delete is in progrss
 * @NAN_DATA_CONNECTING_STATE: Data connection in progress
 * @NAN_DATA_CONNECTED_STATE: Data connection successful
 * @NAN_DATA_END_STATE: NDP end is in progress
 * @NAN_DATA_DISCONNECTED_STATE: NDP is in disconnected state
 */
enum nan_datapath_state {
	NAN_DATA_NDI_CREATING_STATE = 0,
	NAN_DATA_NDI_CREATED_STATE = 1,
	NAN_DATA_NDI_DELETING_STATE = 2,
	NAN_DATA_NDI_DELETED_STATE = 3,
	NAN_DATA_PEER_CREATE_STATE = 4,
	NAN_DATA_PEER_DELETE_STATE = 5,
	NAN_DATA_CONNECTING_STATE = 6,
	NAN_DATA_CONNECTED_STATE = 7,
	NAN_DATA_END_STATE = 8,
	NAN_DATA_DISCONNECTED_STATE = 9,
};

/**
 * struct nan_datapath_ctx - context for nan data path
 * @state: Current state of NDP
 * @active_ndp_sessions: active ndp sessions per adapter
 * @ndp_create_transaction_id: transaction id for create req
 * @ndp_delete_transaction_id: transaction id for delete req
 * @wext_state: Wext state variable
 * @conn_info: NDP connection info
 * @roam_info: NDP roam info
 * @gtk_offload_req_params: GTK offload request params
 * @ndp_key_installed: NDP security key installed
 * @ndp_enc_key: NDP encryption key info
 * @ndp_debug_state: debug state info
 */
struct nan_datapath_ctx {
	enum nan_datapath_state state;
	uint32_t active_ndp_sessions;
	uint16_t ndp_create_transaction_id;
	uint16_t ndp_delete_transaction_id;
	hdd_wext_state_t wext_state;
	connection_info_t conn_info;
#ifdef WLAN_FEATURE_GTK_OFFLOAD
	tSirGtkOffloadParams gtk_offload_req_params;
#endif
	bool ndp_key_installed;
	tCsrRoamSetKey ndp_enc_key;
	uint32_t ndp_debug_state;
};

#ifdef WLAN_FEATURE_NAN_DATAPATH
void hdd_ndp_print_ini_config(struct hdd_context_s *hdd_ctx);
void hdd_nan_datapath_target_config(hdd_context_t *hdd_ctx,
				struct hdd_tgt_cfg *cfg);
void hdd_ndp_event_handler(struct hdd_adapter_s *adapter,
	tCsrRoamInfo *roam_info, uint32_t roam_id, eRoamCmdStatus roam_status,
	eCsrRoamResult roam_result);
int wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len);
int hdd_init_nan_data_mode(struct hdd_adapter_s *adapter);
void hdd_ndp_session_end_handler(hdd_adapter_t *adapter);
#else
static inline void hdd_ndp_print_ini_config(struct hdd_context_s *hdd_ctx)
{
}
static inline void hdd_nan_datapath_target_config(struct hdd_context_s *hdd_ctx,
						struct hdd_tgt_cfg *cfg)
{
}
static inline void hdd_ndp_event_handler(struct hdd_adapter_s *adapter,
	tCsrRoamInfo *roam_info, uint32_t roam_id, eRoamCmdStatus roam_status,
	eCsrRoamResult roam_result)
{
}
static inline int wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int data_len)
{
	return 0;
}
static inline int hdd_init_nan_data_mode(struct hdd_adapter_s *adapter)
{
	return 0;
}
static inline void hdd_ndp_session_end_handler(hdd_adapter_t *adapter)
{
}
#endif /* WLAN_FEATURE_NAN_DATAPATH */

#endif /* __WLAN_HDD_NAN_DATAPATH_H */
