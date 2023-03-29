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
 * DOC: contains nan definitions exposed to other modules
 */

#ifndef _NAN_PUBLIC_STRUCTS_H_
#define _NAN_PUBLIC_STRUCTS_H_

#include "qdf_types.h"
#include "qdf_status.h"
#include "scheduler_api.h"
#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif

struct wlan_objmgr_psoc;
struct wlan_objmgr_vdev;

#define IFACE_NAME_SIZE 64
#define NDP_QOS_INFO_LEN 255
#define NDP_APP_INFO_LEN 255
#define NDP_PMK_LEN 32
#define NDP_SCID_BUF_LEN 256
#define NDP_NUM_INSTANCE_ID 255
#define NAN_MAX_SERVICE_NAME_LEN 255
#define NAN_PASSPHRASE_MIN_LEN 8
#define NAN_PASSPHRASE_MAX_LEN 63
#define NAN_CH_INFO_MAX_CHANNELS 4
#define WLAN_WAIT_TIME_NDP_END 4000

#define NAN_PSEUDO_VDEV_ID CFG_TGT_NUM_VDEV

#define NAN_SER_CMD_TIMEOUT 4000

/**
 * enum nan_discovery_msg_type - NAN msg type
 * @NAN_GENERIC_REQ: Type for all the NAN requests other than enable/disable
 * @NAN_ENABLE_REQ: Request type for enabling the NAN Discovery
 * @NAN_DISABLE_REQ: Request type for disabling the NAN Discovery
 */
enum nan_discovery_msg_type {
	NAN_GENERIC_REQ = 0,
	NAN_ENABLE_REQ  = 1,
	NAN_DISABLE_REQ = 2,
};

/**
 * enum nan_datapath_msg_type - NDP msg type
 * @NAN_DATAPATH_INF_CREATE_REQ: ndi create request
 * @NAN_DATAPATH_INF_CREATE_RSP: ndi create response
 * @NAN_DATAPATH_INF_DELETE_REQ: ndi delete request
 * @NAN_DATAPATH_INF_DELETE_RSP: ndi delete response
 * @NDP_INITIATOR_REQ: ndp initiator request
 * @NDP_INITIATOR_RSP: ndp initiator response
 * @NDP_RESPONDER_REQ: ndp responder request
 * @NDP_RESPONDER_RSP: ndp responder response
 * @NDP_END_REQ: ndp end request
 * @NDP_END_RSP: ndp end response
 * @NDP_INDICATION: ndp indication
 * @NDP_CONFIRM: ndp confirm
 * @NDP_END_IND: ndp end indication
 * @NDP_NEW_PEER: ndp new peer created
 * @NDP_PEER_DEPARTED: ndp peer departed/deleted
 * @NDP_SCHEDULE_UPDATE: ndp schedule update
 * @NDP_END_ALL: end all NDPs request
 * @NDP_HOST_UPDATE: update host about ndp status
 */
enum nan_datapath_msg_type {
	NAN_DATAPATH_INF_CREATE_REQ  = 0,
	NAN_DATAPATH_INF_CREATE_RSP  = 1,
	NAN_DATAPATH_INF_DELETE_REQ  = 2,
	NAN_DATAPATH_INF_DELETE_RSP  = 3,
	NDP_INITIATOR_REQ            = 4,
	NDP_INITIATOR_RSP            = 5,
	NDP_RESPONDER_REQ            = 6,
	NDP_RESPONDER_RSP            = 7,
	NDP_END_REQ                  = 8,
	NDP_END_RSP                  = 9,
	NDP_INDICATION               = 10,
	NDP_CONFIRM                  = 11,
	NDP_END_IND                  = 12,
	NDP_NEW_PEER                 = 13,
	NDP_PEER_DEPARTED            = 14,
	NDP_SCHEDULE_UPDATE          = 15,
	NDP_END_ALL                  = 16,
	NDP_HOST_UPDATE              = 17,
};

/**
 * enum nan_datapath_status_type - NDP status type
 * @NAN_DATAPATH_RSP_STATUS_SUCCESS: request was successful
 * @NAN_DATAPATH_RSP_STATUS_ERROR: request failed
 */
enum nan_datapath_status_type {
	NAN_DATAPATH_RSP_STATUS_SUCCESS = 0x00,
	NAN_DATAPATH_RSP_STATUS_ERROR = 0x01,
};

/**
 * enum nan_datapath_reason_code - NDP command rsp reason code value
 * @NDP_UNSUPPORTED_CONCURRENCY: Will be used in unsupported concurrency cases
 * @NDP_NAN_DATA_IFACE_CREATE_FAILED: ndi create failed
 * @NDP_NAN_DATA_IFACE_DELETE_FAILED: ndi delete failed
 * @NDP_DATA_INITIATOR_REQ_FAILED: data initiator request failed
 * @NDP_DATA_RESPONDER_REQ_FAILED: data responder request failed
 * @NDP_INVALID_SERVICE_INSTANCE_ID: invalid service instance id
 * @NDP_INVALID_NDP_INSTANCE_ID: invalid ndp instance id
 * @NDP_INVALID_RSP_CODE: invalid response code in ndp responder request
 * @NDP_INVALID_APP_INFO_LEN: invalid app info length
 * @NDP_NMF_REQ_FAIL: OTA nan mgmt frame failure for data request
 * @NDP_NMF_RSP_FAIL: OTA nan mgmt frame failure for data response
 * @NDP_NMF_CNF_FAIL: OTA nan mgmt frame failure for confirm
 * @NDP_END_FAILED: ndp end failed
 * @NDP_NMF_END_REQ_FAIL: OTA nan mgmt frame failure for data end
 * @NDP_VENDOR_SPECIFIC_ERROR: other vendor specific failures
 */
enum nan_datapath_reason_code {
	NAN_DATAPATH_UNSUPPORTED_CONCURRENCY = 9000,
	NAN_DATAPATH_NAN_DATA_IFACE_CREATE_FAILED = 9001,
	NAN_DATAPATH_NAN_DATA_IFACE_DELETE_FAILED = 9002,
	NAN_DATAPATH_DATA_INITIATOR_REQ_FAILED = 9003,
	NAN_DATAPATH_DATA_RESPONDER_REQ_FAILED = 9004,
	NAN_DATAPATH_INVALID_SERVICE_INSTANCE_ID = 9005,
	NAN_DATAPATH_INVALID_NDP_INSTANCE_ID = 9006,
	NAN_DATAPATH_INVALID_RSP_CODE = 9007,
	NAN_DATAPATH_INVALID_APP_INFO_LEN = 9008,
	NAN_DATAPATH_NMF_REQ_FAIL = 9009,
	NAN_DATAPATH_NMF_RSP_FAIL = 9010,
	NAN_DATAPATH_NMF_CNF_FAIL = 9011,
	NAN_DATAPATH_END_FAILED = 9012,
	NAN_DATAPATH_NMF_END_REQ_FAIL = 9013,
	/* 9500 onwards vendor specific error codes */
	NAN_DATAPATH_VENDOR_SPECIFIC_ERROR = 9500,
};

/**
 * enum nan_datapath_response_code - responder's response code to nan data path
 * request
 * @NAN_DATAPATH_RESPONSE_ACCEPT: ndp request accepted
 * @NAN_DATAPATH_RESPONSE_REJECT: ndp request rejected
 * @NAN_DATAPATH_RESPONSE_DEFER: ndp request deferred until later (response to
 * follow any time later)
 */
enum nan_datapath_response_code {
	NAN_DATAPATH_RESPONSE_ACCEPT = 0,
	NAN_DATAPATH_RESPONSE_REJECT = 1,
	NAN_DATAPATH_RESPONSE_DEFER = 2,
};

/**
 * enum nan_datapath_accept_policy - nan data path accept policy
 * @NAN_DATAPATH_ACCEPT_POLICY_NONE: the framework will decide the policy
 * @NAN_DATAPATH_ACCEPT_POLICY_ALL: accept policy offloaded to fw
 */
enum nan_datapath_accept_policy {
	NAN_DATAPATH_ACCEPT_POLICY_NONE = 0,
	NAN_DATAPATH_ACCEPT_POLICY_ALL = 1,
};

/**
 * enum nan_datapath_self_role - nan data path role
 * @NAN_DATAPATH_ROLE_INITIATOR: initiator of nan data path request
 * @NAN_DATAPATH_ROLE_RESPONDER: responder to nan data path request
 */
enum nan_datapath_self_role {
	NAN_DATAPATH_ROLE_INITIATOR = 0,
	NAN_DATAPATH_ROLE_RESPONDER = 1,
};

/**
 * enum nan_datapath_end_type - NDP end type
 * @NAN_DATAPATH_END_TYPE_UNSPECIFIED: type is unspecified
 * @NAN_DATAPATH_END_TYPE_PEER_UNAVAILABLE: type is peer unavailable
 * @NAN_DATAPATH_END_TYPE_OTA_FRAME: NDP end frame received from peer
 */
enum nan_datapath_end_type {
	NAN_DATAPATH_END_TYPE_UNSPECIFIED = 0x00,
	NAN_DATAPATH_END_TYPE_PEER_UNAVAILABLE = 0x01,
	NAN_DATAPATH_END_TYPE_OTA_FRAME = 0x02,
};

/**
 * enum nan_datapath_end_reason_code - NDP end reason code
 * @NAN_DATAPATH_END_REASON_UNSPECIFIED: reason is unspecified
 * @NAN_DATAPATH_END_REASON_INACTIVITY: reason is peer inactivity
 * @NAN_DATAPATH_END_REASON_PEER_DATA_END: data end indication received from
 * peer
 */
enum nan_datapath_end_reason_code {
	NAN_DATAPATH_END_REASON_UNSPECIFIED = 0x00,
	NAN_DATAPATH_END_REASON_INACTIVITY = 0x01,
	NAN_DATAPATH_END_REASON_PEER_DATA_END = 0x02,
};

/**
 * enum nan_datapath_state - NAN datapath states
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
	NAN_DATA_INVALID_STATE = -1,
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
 * struct nan_datapath_app_info - application info shared during ndp setup
 * @ndp_app_info_len: ndp app info length
 * @ndp_app_info: variable length application information
 */
struct nan_datapath_app_info {
	uint32_t ndp_app_info_len;
	uint8_t ndp_app_info[NDP_APP_INFO_LEN];
};

/**
 * struct nan_datapath_cfg - ndp configuration
 * @ndp_cfg_len: ndp configuration length
 * @ndp_cfg: variable length ndp configuration
 */
struct nan_datapath_cfg {
	uint32_t ndp_cfg_len;
	uint8_t ndp_cfg[NDP_QOS_INFO_LEN];
};

/**
 * struct nan_datapath_pmk - structure to hold pairwise master key
 * @pmk_len: length of pairwise master key
 * @pmk: buffer containing pairwise master key
 */
struct nan_datapath_pmk {
	uint32_t pmk_len;
	uint8_t pmk[NDP_PMK_LEN];
};

/**
 * struct nan_datapath_scid - structure to hold sceurity context identifier
 * @scid_len: length of scid
 * @scid: scid
 */
struct nan_datapath_scid {
	uint32_t scid_len;
	uint8_t scid[NDP_SCID_BUF_LEN];
};

/**
 * struct ndp_passphrase - structure to hold passphrase
 * @passphrase_len: length of passphrase
 * @passphrase: buffer containing passphrase
 */
struct ndp_passphrase {
	uint32_t passphrase_len;
	uint8_t passphrase[NAN_PASSPHRASE_MAX_LEN];
};

/**
 * struct ndp_service_name - structure to hold service_name
 * @service_name_len: length of service_name
 * @service_name: buffer containing service_name
 */
struct ndp_service_name {
	uint32_t service_name_len;
	uint8_t service_name[NAN_MAX_SERVICE_NAME_LEN];
};

/**
 * struct peer_nan_datapath_map  - mapping of NDP instances to peer to VDEV
 * @vdev_id: session id of the interface over which ndp is being created
 * @peer_ndi_mac_addr: peer NDI mac address
 * @num_active_ndp_sessions: number of active NDP sessions on the peer
 * @type: NDP end indication type
 * @reason_code: NDP end indication reason code
 * @ndp_instance_id: NDP instance ID
 */
struct peer_nan_datapath_map {
	uint32_t vdev_id;
	struct qdf_mac_addr peer_ndi_mac_addr;
	uint32_t num_active_ndp_sessions;
	enum nan_datapath_end_type type;
	enum nan_datapath_end_reason_code reason_code;
	uint32_t ndp_instance_id;
};

/**
 * struct nan_datapath_channel_info - ndp channel and channel bandwidth
 * @freq: channel freq in mhz of the ndp connection
 * @ch_width: channel width (wmi_channel_width) of the ndp connection
 * @nss: nss used for ndp connection
 * @mac_id: MAC ID associated with the NDP channel
 */
struct nan_datapath_channel_info {
	uint32_t freq;
	uint32_t ch_width;
	uint32_t nss;
	uint8_t mac_id;
};

#define NAN_CH_INFO_MAX_LEN \
	(NAN_CH_INFO_MAX_CHANNELS * sizeof(struct nan_datapath_channel_info))

/**
 * struct nan_datapath_inf_create_req - ndi create request params
 * @transaction_id: unique identifier
 * @iface_name: interface name
 */
struct nan_datapath_inf_create_req {
	uint32_t transaction_id;
	char  iface_name[IFACE_NAME_SIZE];
};

/**
 * struct nan_datapath_inf_create_rsp - ndi create response params
 * @status: request status
 * @reason: reason if any
 */
struct nan_datapath_inf_create_rsp {
	uint32_t status;
	uint32_t reason;
	uint8_t sta_id;
};

/**
 * struct nan_datapath_inf_delete_rsp - ndi delete response params
 * @status: request status
 * @reason: reason if any
 */
struct nan_datapath_inf_delete_rsp {
	uint32_t status;
	uint32_t reason;
};

/**
 * struct nan_datapath_initiator_req - ndp initiator request params
 * @vdev: pointer to vdev object
 * @transaction_id: unique identifier
 * @channel: suggested channel for ndp creation
 * @channel_cfg: channel config, 0=no channel, 1=optional, 2=mandatory
 * @service_instance_id: Service identifier
 * @peer_discovery_mac_addr: Peer's discovery mac address
 * @self_ndi_mac_addr: self NDI mac address
 * @ndp_config: ndp configuration params
 * @ndp_info: ndp application info
 * @ncs_sk_type: indicates NCS_SK_128 or NCS_SK_256
 * @pmk: pairwise master key
 * @passphrase: passphrase
 * @service_name: service name
 * @is_ipv6_addr_present: indicates if following ipv6 address is valid
 * @ipv6_addr: ipv6 address address used by ndp
 */
struct nan_datapath_initiator_req {
	struct wlan_objmgr_vdev *vdev;
	uint32_t transaction_id;
	uint32_t channel;
	uint32_t channel_cfg;
	uint32_t service_instance_id;
	uint32_t ncs_sk_type;
	struct qdf_mac_addr peer_discovery_mac_addr;
	struct qdf_mac_addr self_ndi_mac_addr;
	struct nan_datapath_cfg ndp_config;
	struct nan_datapath_app_info ndp_info;
	struct nan_datapath_pmk pmk;
	struct ndp_passphrase passphrase;
	struct ndp_service_name service_name;
	bool is_ipv6_addr_present;
	uint8_t ipv6_addr[QDF_IPV6_ADDR_SIZE];
};

/**
 * struct nan_datapath_initiator_rsp - response event from FW
 * @vdev: pointer to vdev object
 * @transaction_id: unique identifier
 * @ndp_instance_id: locally created NDP instance ID
 * @status: status of the ndp request
 * @reason: reason for failure if any
 */
struct nan_datapath_initiator_rsp {
	struct wlan_objmgr_vdev *vdev;
	uint32_t transaction_id;
	uint32_t ndp_instance_id;
	uint32_t status;
	uint32_t reason;
};

/**
 * struct nan_datapath_responder_req - responder's response to ndp create
 * request
 * @vdev: pointer to vdev object
 * @transaction_id: unique identifier
 * @ndp_instance_id: locally created NDP instance ID
 * @ndp_rsp: response to the ndp create request
 * @ndp_config: ndp configuration params
 * @ndp_info: ndp application info
 * @pmk: pairwise master key
 * @ncs_sk_type: indicates NCS_SK_128 or NCS_SK_256
 * @passphrase: passphrase
 * @service_name: service name
 * @is_ipv6_addr_present: indicates if following ipv6 address is valid
 * @ipv6_addr: ipv6 address address used by ndp
 * @is_port_present: indicates if following port is valid
 * @port: port specified by for this NDP
 * @is_protocol_present: indicates if following protocol is valid
 * @protocol: protocol used by this NDP
 */
struct nan_datapath_responder_req {
	struct wlan_objmgr_vdev *vdev;
	uint32_t transaction_id;
	uint32_t ndp_instance_id;
	enum nan_datapath_response_code ndp_rsp;
	struct nan_datapath_cfg ndp_config;
	struct nan_datapath_app_info ndp_info;
	struct nan_datapath_pmk pmk;
	uint32_t ncs_sk_type;
	struct ndp_passphrase passphrase;
	struct ndp_service_name service_name;
	bool is_ipv6_addr_present;
	uint8_t ipv6_addr[QDF_IPV6_ADDR_SIZE];
	bool is_port_present;
	uint16_t port;
	bool is_protocol_present;
	uint8_t protocol;
};

/**
 * struct nan_datapath_responder_rsp - response to responder's request
 * @vdev: pointer to vdev object
 * @transaction_id: unique identifier
 * @status: command status
 * @reason: reason for failure if any
 * @peer_mac_addr: Peer's mac address
 * @create_peer: Flag to indicate to create peer
 */
struct nan_datapath_responder_rsp {
	struct wlan_objmgr_vdev *vdev;
	uint32_t transaction_id;
	uint32_t status;
	uint32_t reason;
	struct qdf_mac_addr peer_mac_addr;
	bool create_peer;
};

/**
 * struct nan_datapath_end_req - ndp end request
 * @vdev: pointer to vdev object
 * @transaction_id: unique transaction identifier
 * @num_ndp_instances: number of ndp instances to be terminated
 * @ndp_ids: array of ndp_instance_id to be terminated
 */
struct nan_datapath_end_req {
	struct wlan_objmgr_vdev *vdev;
	uint32_t transaction_id;
	uint32_t num_ndp_instances;
	uint32_t ndp_ids[NDP_NUM_INSTANCE_ID];
};

/**
 * struct nan_datapath_end_all_ndps - Datapath termination request to end all
 * NDPs on the given vdev
 * @vdev: pointer to vdev object
 */
struct nan_datapath_end_all_ndps {
	struct wlan_objmgr_vdev *vdev;
};

/**
 * enum nan_event_id_types - NAN event ID types
 * @nan_event_id_error_rsp: NAN event indicating error
 * @nan_event_id_enable_rsp: NAN Enable Response event ID
 * @nan_event_id_disable_ind: NAN Disable Indication event ID
 * @nan_event_id_generic_rsp: All remaining NAN events, treated as passthrough
 */
enum nan_event_id_types {
	nan_event_id_error_rsp = 0,
	nan_event_id_enable_rsp,
	nan_event_id_disable_ind,
	nan_event_id_generic_rsp,
};

/**
 * struct nan_event_params - NAN event received from the Target
 * @psoc: Pointer to the psoc object
 * @evt_type: NAN Discovery event type
 * @is_nan_enable_success: Status from the NAN Enable Response event
 * @mac_id: MAC ID associated with NAN Discovery from NAN Enable Response event
 * @vdev_id: vdev id of the interface created for NAN discovery
 * @buf_len: Event buffer length
 * @buf: Event buffer starts here
 */
struct nan_event_params {
	struct wlan_objmgr_psoc *psoc;
	enum nan_event_id_types evt_type;
	bool is_nan_enable_success;
	uint8_t mac_id;
	uint8_t vdev_id;
	uint32_t buf_len;
	/* Variable length, do not add anything after this */
	uint8_t buf[];
};

#define NAN_MSG_ID_DISABLE_INDICATION 26
/**
 * struct nan_msg_hdr - NAN msg header to be sent to userspace
 * @msg_version: NAN msg version
 * @msg_id: NAN message id
 * @reserved: Reserved for now to avoid padding
 *
 * 8-byte control message header used by NAN
 *
 */
struct nan_msg_hdr {
	uint16_t msg_version:4;
	uint16_t msg_id:12;
	uint16_t reserved[3];
};

#define NAN_STATUS_SUCCESS 0
#define NAN_STATUS_UNSUPPORTED_CONCURRENCY_NAN_DISABLED 12

/**
 * struct nan_disable_ind_msg - NAN disable ind params
 * @msg_hdr: NAN msg header
 * @reason: NAN disable reason, below are valid reasons for NAN disable ind
 *          NAN_STATUS_SUCCESS
 *          NAN_STATUS_UNSUPPORTED_CONCURRENCY_NAN_DISABLED
 * @reserved: Reserved for now to avoid padding
 */
struct nan_disable_ind_msg {
	struct nan_msg_hdr msg_hdr;
	uint16_t reason;
	uint16_t reserved;
};

/**
 * struct nan_msg_params - NAN request params
 * @request_data_len: request data length
 * @request_data: request data
 * @rtt_cap: indicate if responder/initiator role is supported
 * @disable_6g_nan: Disable NAN in 6Ghz
 */
struct nan_msg_params {
	uint16_t request_data_len;
	uint32_t rtt_cap;
	bool disable_6g_nan;
	/* Variable length, do not add anything after this */
	uint8_t request_data[];
};

/**
 * struct nan_generic_req - A NAN request for the Target
 * @psoc: Pointer to the psoc object
 * @params: NAN request structure containing message fr the Target
 */
struct nan_generic_req {
	struct wlan_objmgr_psoc *psoc;
	/* Variable length, do not add anything after this */
	struct nan_msg_params params;
};

/**
 * struct nan_disable_req - NAN request to disable NAN Discovery
 * @psoc: Pointer to the psoc object
 * @disable_2g_discovery: Flag for disabling Discovery in 2G band
 * @disable_5g_discovery: Flag for disabling Discovery in 5G band
 * @params: NAN request structure containing message for the target
 */
struct nan_disable_req {
	struct wlan_objmgr_psoc *psoc;
	bool disable_2g_discovery;
	bool disable_5g_discovery;
	/* Variable length, do not add anything after this */
	struct nan_msg_params params;
};

/**
 * struct nan_enable_req - NAN request to enable NAN Discovery
 * @psoc: Pointer to the psoc object
 * @social_chan_2g_freq: Social channel in 2G band for the NAN Discovery
 * @social_chan_5g_freq: Social channel in 5G band for the NAN Discovery
 * @params: NAN request structure containing message for the target
 */
struct nan_enable_req {
	struct wlan_objmgr_psoc *psoc;
	uint32_t social_chan_2g_freq;
	uint32_t social_chan_5g_freq;
	/* Variable length, do not add anything after this */
	struct nan_msg_params params;
};

/**
 * struct nan_datapath_end_rsp_event  - firmware response to ndp end request
 * @vdev: pointer to vdev object
 * @transaction_id: unique identifier for the request
 * @status: status of operation
 * @reason: reason(opaque to host driver)
 */
struct nan_datapath_end_rsp_event {
	struct wlan_objmgr_vdev *vdev;
	uint32_t transaction_id;
	uint32_t status;
	uint32_t reason;
};

/**
 * struct nan_datapath_end_indication_event - ndp termination notification from
 * FW
 * @vdev: pointer to vdev object
 * @num_ndp_ids: number of NDP ids
 * @ndp_map: mapping of NDP instances to peer and vdev
 */
struct nan_datapath_end_indication_event {
	struct wlan_objmgr_vdev *vdev;
	uint32_t num_ndp_ids;
	struct peer_nan_datapath_map ndp_map[];
};

/**
 * struct nan_datapath_peer_ind - ndp peer indication
 * @msg: msg received by FW
 * @data_len: data length
 *
 */
struct nan_dump_msg {
	uint8_t *msg;
	uint32_t data_len;
};

/**
 * struct nan_datapath_confirm_event - ndp confirmation event from FW
 * @vdev: pointer to vdev object
 * @ndp_instance_id: ndp instance id for which confirm is being generated
 * @reason_code : reason code(opaque to driver)
 * @num_active_ndps_on_peer: number of ndp instances on peer
 * @peer_ndi_mac_addr: peer NDI mac address
 * @rsp_code: ndp response code
 * @num_channels: num channels
 * @ch: channel info struct array
 * @ndp_info: ndp application info
 * @is_ipv6_addr_present: indicates if following ipv6 address is valid
 * @ipv6_addr: ipv6 address address used by ndp
 * @is_port_present: indicates if following port is valid
 * @port: port specified by for this NDP
 * @is_protocol_present: indicates if following protocol is valid
 * @protocol: protocol used by this NDP
 */
struct nan_datapath_confirm_event {
	struct wlan_objmgr_vdev *vdev;
	uint32_t ndp_instance_id;
	uint32_t reason_code;
	uint32_t num_active_ndps_on_peer;
	struct qdf_mac_addr peer_ndi_mac_addr;
	enum nan_datapath_response_code rsp_code;
	uint32_t num_channels;
	struct nan_datapath_channel_info ch[NAN_CH_INFO_MAX_CHANNELS];
	struct nan_datapath_app_info ndp_info;
	bool is_ipv6_addr_present;
	uint8_t ipv6_addr[QDF_IPV6_ADDR_SIZE];
	bool is_port_present;
	uint16_t port;
	bool is_protocol_present;
	uint8_t protocol;
};

/**
 * struct nan_datapath_indication_event - create ndp indication on the responder
 * @vdev: pointer to vdev object
 * @service_instance_id: Service identifier
 * @peer_discovery_mac_addr: Peer's discovery mac address
 * @peer_mac_addr: Peer's NDI mac address
 * @ndp_initiator_mac_addr: NDI mac address of the peer initiating NDP
 * @ndp_instance_id: locally created NDP instance ID
 * @role: self role for NDP
 * @ndp_accept_policy: accept policy configured by the upper layer
 * @ndp_config: ndp configuration params
 * @ndp_info: ndp application info
 * @ncs_sk_type: indicates NCS_SK_128 or NCS_SK_256
 * @scid: security context identifier
 * @is_ipv6_addr_present: indicates if following ipv6 address is valid
 * @ipv6_addr: ipv6 address address used by ndp
 */
struct nan_datapath_indication_event {
	struct wlan_objmgr_vdev *vdev;
	uint32_t service_instance_id;
	struct qdf_mac_addr peer_discovery_mac_addr;
	struct qdf_mac_addr peer_mac_addr;
	uint32_t ndp_instance_id;
	enum nan_datapath_self_role role;
	enum nan_datapath_accept_policy policy;
	struct nan_datapath_cfg ndp_config;
	struct nan_datapath_app_info ndp_info;
	uint32_t ncs_sk_type;
	struct nan_datapath_scid scid;
	bool is_ipv6_addr_present;
	uint8_t ipv6_addr[QDF_IPV6_ADDR_SIZE];
};

/**
 * struct nan_datapath_peer_ind - ndp peer indication
 * @peer_mac_addr: peer mac address
 * @sta_id: station id
 *
 */
struct nan_datapath_peer_ind {
	struct qdf_mac_addr peer_mac_addr;
	uint16_t sta_id;
};

/**
 * struct nan_datapath_sch_update_event - ndp schedule update indication
 * @vdev: vdev schedule update was received
 * @peer_addr: peer for which schedule update was received
 * @flags: reason for sch update (opaque to driver)
 * @num_channels: num of channels
 * @num_ndp_instances: num of ndp instances
 * @ch: channel info array
 * @ndp_instances: array of ndp instances
 */
struct nan_datapath_sch_update_event {
	struct wlan_objmgr_vdev *vdev;
	struct qdf_mac_addr peer_addr;
	uint32_t flags;
	uint32_t num_channels;
	uint32_t num_ndp_instances;
	struct nan_datapath_channel_info ch[NAN_CH_INFO_MAX_CHANNELS];
	uint32_t ndp_instances[NDP_NUM_INSTANCE_ID];
};

/**
 * struct nan_datapath_host_event - ndp host event parameters
 * @vdev: vdev obj associated with the ndp
 * @ndp_termination_in_progress: flag that indicates whether NDPs associated
 * with the given vdev are being terminated
 */
struct nan_datapath_host_event {
	struct wlan_objmgr_vdev *vdev;
	bool ndp_termination_in_progress;
};

/**
 * struct nan_callbacks - struct containing callback to non-converged driver
 * @os_if_nan_event_handler: OS IF Callback for handling NAN Discovery events
 * @os_if_ndp_event_handler: OS IF Callback for handling NAN Datapath events
 * @ucfg_nan_request_process_cb: Callback to indicate NAN enable/disable
 * request processing is complete
 * @ndi_open: HDD callback for creating the NAN Datapath Interface
 * @ndi_start: HDD callback for starting the NAN Datapath Interface
 * @ndi_close: HDD callback for closing the NAN Datapath Interface
 * @ndi_delete: HDD callback for deleting the NAN Datapath Interface
 * @drv_ndi_create_rsp_handler: HDD callback for handling NDI interface creation
 * @drv_ndi_delete_rsp_handler: HDD callback for handling NDI interface deletion
 * @new_peer_ind: HDD callback for handling new NDP peer
 * @add_ndi_peer: LIM callback for adding NDP peer
 * @peer_departed_ind: HDD callback for handling departing of NDP peer
 * @ndp_delete_peers: LIM callback for deleting NDP peer
 * @delete_peers_by_addr: LIM callback for deleting peer by MAC address
 * @update_ndi_conn: WMA callback to update NDI's connection info
 * @nan_concurrency_update: Callback to handle nan concurrency
 */
struct nan_callbacks {
	/* callback to os_if layer from umac */
	void (*os_if_nan_event_handler)(struct nan_event_params *nan_event);
	void (*os_if_ndp_event_handler)(struct wlan_objmgr_psoc *psoc,
					struct wlan_objmgr_vdev *vdev,
					uint32_t type, void *msg);
	void (*ucfg_nan_request_process_cb)(void *cookie);
	int (*ndi_open)(char *iface_name);
	int (*ndi_start)(char *iface_name, uint16_t);
	void (*ndi_close)(uint8_t);
	int (*ndi_delete)(uint8_t, char *iface_name, uint16_t transaction_id);
	void (*drv_ndi_create_rsp_handler)
				(uint8_t, struct nan_datapath_inf_create_rsp *);
	void (*drv_ndi_delete_rsp_handler)(uint8_t);
	int (*new_peer_ind)(uint8_t, uint16_t, struct qdf_mac_addr *, bool);
	QDF_STATUS (*add_ndi_peer)(uint32_t, struct qdf_mac_addr);
	void (*peer_departed_ind)(uint8_t, uint16_t, struct qdf_mac_addr *,
				  bool);
	void (*ndp_delete_peers)(struct peer_nan_datapath_map*, uint8_t);
	void (*delete_peers_by_addr)(uint8_t, struct qdf_mac_addr);
	QDF_STATUS (*update_ndi_conn)(uint8_t vdev_id,
				      struct nan_datapath_channel_info
								    *chan_info);
	void (*nan_concurrency_update)(void);
};

/**
 * struct wlan_nan_tx_ops - structure of tx function pointers for nan component
 * @nan_discovery_req_tx: Msg handler for TX operations for the NAN Discovery
 * @nan_datapath_req_tx: Msg handler for TX operations for the NAN Datapath
 */
struct wlan_nan_tx_ops {
	QDF_STATUS (*nan_discovery_req_tx)(void *nan_req, uint32_t req_type);
	QDF_STATUS (*nan_datapath_req_tx)(void *req, uint32_t req_id);
};

/**
 * struct wlan_nan_rx_ops - structure of rx function pointers for nan component
 * @nan_discovery_event_rx: Evt handler for RX operations for the NAN Discovery
 * @nan_datapath_event_rx: Evt handler for RX operations for the NAN Datapath
 */
struct wlan_nan_rx_ops {
	QDF_STATUS (*nan_discovery_event_rx)(struct scheduler_msg *event);
	QDF_STATUS (*nan_datapath_event_rx)(struct scheduler_msg *event);
};

/**
 * struct nan_tgt_caps - NAN Target capabilities
 * @nan_conc_control: Target supports disabling NAN Discovery from host
 *		      so that host is able to handle(disable) NAN
 *		      concurrencies.
 * @nan_dbs_supported: Target supports NAN Discovery with DBS
 * @ndi_dbs_supported: Target supports NAN Datapath with DBS
 * @nan_sap_supported: Target supports NAN Discovery with SAP concurrency
 * @ndi_sap_supported: Target supports NAN Datapath with SAP concurrency
 * @nan_vdev_allowed: Allow separate vdev creation for NAN discovery
 * @sta_nan_ndi_ndi_allowed: 4 port concurrency of STA+NAN+NDI+NDI is supported
 * @ndi_txbf_supported: Target supports NAN Datapath with TX beamforming
 * by Fw or not.
 */
struct nan_tgt_caps {
	uint32_t nan_conc_control:1;
	uint32_t nan_dbs_supported:1;
	uint32_t ndi_dbs_supported:1;
	uint32_t nan_sap_supported:1;
	uint32_t ndi_sap_supported:1;
	uint32_t nan_vdev_allowed:1;
	uint32_t sta_nan_ndi_ndi_allowed:1;
	uint32_t ndi_txbf_supported:1;
};

#endif
