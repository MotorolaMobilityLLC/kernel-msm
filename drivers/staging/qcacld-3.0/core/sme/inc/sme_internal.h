/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
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

#if !defined(__SMEINTERNAL_H)
#define __SMEINTERNAL_H

/**
 * \file  sme_internal.h
 *
 * \brief prototype for SME internal structures and APIs used for SME and MAC
 */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "qdf_status.h"
#include "qdf_lock.h"
#include "qdf_trace.h"
#include "qdf_mem.h"
#include "qdf_types.h"
#include "host_diag_core_event.h"
#include "csr_link_list.h"
#include "sme_power_save.h"
#include "wmi_unified.h"
#include "wmi_unified_param.h"

struct twt_add_dialog_complete_event;
struct wmi_twt_add_dialog_complete_event_param;
struct wmi_twt_enable_complete_event_param;
/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/

/* Mask can be only have one bit set */
typedef enum eSmeCommandType {
	eSmeNoCommand = 0,
	/* this is not a command, it is to identify this is a CSR command */
	eSmeCsrCommandMask = 0x10000,
	eSmeCommandRoam,
	eSmeCommandWmStatusChange,
	eSmeCommandGetdisconnectStats,
	/* QOS */
	eSmeQosCommandMask = 0x40000,   /* To identify Qos commands */
	eSmeCommandAddTs,
	eSmeCommandDelTs,
	e_sme_command_set_hw_mode,
	e_sme_command_nss_update,
	e_sme_command_set_dual_mac_config,
	e_sme_command_set_antenna_mode,
} eSmeCommandType;

typedef enum eSmeState {
	SME_STATE_STOP,
	SME_STATE_START,
	SME_STATE_READY,
} eSmeState;

#define SME_IS_START(mac)  (SME_STATE_STOP != (mac)->sme.state)
#define SME_IS_READY(mac)  (SME_STATE_READY == (mac)->sme.state)

/**
 * struct stats_ext_event - stats_ext_event payload
 * @vdev_id: ID of the vdev for the stats
 * @event_data_len: length of the @event_data
 * @event_data: actual ext stats
 */
struct stats_ext_event {
	uint32_t vdev_id;
	uint32_t event_data_len;
	uint8_t event_data[];
};

/**
 * typedef stats_ext_cb - stats ext callback
 * @hdd_handle: Opaque handle to the HDD context
 * @data: stats ext payload from firmware
 */
typedef void (*stats_ext_cb)(hdd_handle_t hdd_handle,
			     struct stats_ext_event *data);

/**
 * typedef stats_ext2_cb - stats ext2 callback
 * @hdd_handle: Opaque handle to the HDD context
 * @data: stats ext2 payload from firmware
 */
typedef void (*stats_ext2_cb)(hdd_handle_t hdd_handle,
			      struct sir_sme_rx_aggr_hole_ind *data);

#define MAX_ACTIVE_CMD_STATS    16

typedef struct sActiveCmdStats {
	eSmeCommandType command;
	uint32_t reason;
	uint32_t sessionId;
	uint64_t timestamp;
} tActiveCmdStats;

typedef struct sSelfRecoveryStats {
	tActiveCmdStats activeCmdStats[MAX_ACTIVE_CMD_STATS];
	uint8_t cmdStatsIndx;
} tSelfRecoveryStats;

typedef void (*link_layer_stats_cb)(hdd_handle_t hdd_handle,
				    int indication_type,
				    tSirLLStatsResults *results,
				    void *cookie);

typedef void (*ext_scan_ind_cb)(hdd_handle_t hdd_handle,
				const uint16_t, void *);

/**
 * typedef sme_link_speed_cb - sme_get_link_speed() callback function
 * @info: link speed information
 * @context: user context supplied to sme_get_link_speed()
 *
 * This is the signature of a callback function whose addresses is
 * passed as the asynchronous callback function to sme_get_link_speed().
 */

typedef void (*sme_link_speed_cb)(struct link_speed_info *info,
				  void *context);

typedef void (*ocb_callback)(void *context, void *response);
typedef void (*sme_set_thermal_level_callback)(hdd_handle_t hdd_handle,
					       u_int8_t level);
typedef void (*p2p_lo_callback)(void *context,
				struct sir_p2p_lo_event *event);
#ifdef FEATURE_OEM_DATA_SUPPORT
typedef void (*sme_send_oem_data_rsp_msg)(struct oem_data_rsp *);
#endif

#ifdef WLAN_SUPPORT_TWT
/**
 * typedef twt_enable_cb - TWT enable callback signature.
 * @hdd_handle: Opaque HDD handle
 * @params: TWT enable complete event parameters.
 */
typedef
void (*twt_enable_cb)(hdd_handle_t hdd_handle,
		      struct wmi_twt_enable_complete_event_param *params);

/**
 * typedef twt_disable_cb - TWT enable callback signature.
 * @hdd_handle: Opaque HDD handle
 */
typedef void (*twt_disable_cb)(hdd_handle_t hdd_handle);

/**
 * typedef twt_add_dialog_cb - TWT add dialog callback signature.
 * @psoc: Pointer to global psoc
 * @add_dialog_evt: pointer to event buf containing twt response parameters
 * @renego_fail: Flag to indicate if its re-negotiation failure case
 */
typedef
void (*twt_add_dialog_cb)(struct wlan_objmgr_psoc *psoc,
			  struct twt_add_dialog_complete_event *add_dialog_evt,
			  bool renego_fail);

/**
 * typedef twt_del_dialog_cb - TWT delete dialog callback signature.
 * @psoc: Pointer to global psoc
 * @params: TWT delete dialog complete event parameters.
 */
typedef void (*twt_del_dialog_cb)(
	struct wlan_objmgr_psoc *psoc,
	struct wmi_twt_del_dialog_complete_event_param *params);

/**
 * typedef twt_pause_dialog_cb - TWT pause dialog callback signature.
 * @psoc: Pointer to global psoc
 * @params: TWT pause dialog complete event parameters.
 */
typedef
void (*twt_pause_dialog_cb)(struct wlan_objmgr_psoc *psoc,
			    struct wmi_twt_pause_dialog_complete_event_param *params);

/**
 * typedef twt_nudge_dialog_cb - TWT nudge dialog callback signature.
 * @psoc: Pointer to global psoc
 * @params: TWT nudge dialog complete event parameters.
 */
typedef
void (*twt_nudge_dialog_cb)(struct wlan_objmgr_psoc *psoc,
		      struct wmi_twt_nudge_dialog_complete_event_param *params);

/**
 * typedef twt_resume_dialog_cb - TWT resume dialog callback signature.
 * @psoc: Pointer to global psoc
 * @params: TWT resume dialog complete event parameters.
 */
typedef
void (*twt_resume_dialog_cb)(struct wlan_objmgr_psoc *psoc,
			     struct wmi_twt_resume_dialog_complete_event_param *params);

/**
 * typedef twt_notify_cb - TWT notify callback signature.
 * @psoc: Pointer to global psoc
 * @params: TWT twt notify event parameters.
 */
typedef
void (*twt_notify_cb)(struct wlan_objmgr_psoc *psoc,
		      struct wmi_twt_notify_event_param *params);

/**
 * typedef twt_ack_comp_cb - TWT ack callback signature.
 * @params: TWT ack complete event parameters.
 * @context: TWT context
 */
typedef
void (*twt_ack_comp_cb)(struct wmi_twt_ack_complete_event_param *params,
			void *context);

/**
 * struct twt_callbacks - TWT response callback pointers
 * @twt_enable_cb: TWT enable completion callback
 * @twt_disable_cb: TWT disable completion callback
 * @twt_add_dialog_cb: TWT add dialog completion callback
 * @twt_del_dialog_cb: TWT delete dialog completion callback
 * @twt_pause_dialog_cb: TWT pause dialog completion callback
 * @twt_resume_dialog_cb: TWT resume dialog completion callback
 * @twt_notify_cb: TWT notify event callback
 * @twt_nudge_dialog_cb: TWT nudge dialog completion callback
 * @twt_ack_comp_cb: TWT ack completion callback
 */
struct twt_callbacks {
	void (*twt_enable_cb)(hdd_handle_t hdd_handle,
			      struct wmi_twt_enable_complete_event_param *params);
	void (*twt_disable_cb)(hdd_handle_t hdd_handle);
	void (*twt_add_dialog_cb)(struct wlan_objmgr_psoc *psoc,
				  struct twt_add_dialog_complete_event *add_dialog_event,
				  bool renego);
	void (*twt_del_dialog_cb)(struct wlan_objmgr_psoc *psoc,
				  struct wmi_twt_del_dialog_complete_event_param *params);
	void (*twt_pause_dialog_cb)(struct wlan_objmgr_psoc *psoc,
				    struct wmi_twt_pause_dialog_complete_event_param *params);
	void (*twt_resume_dialog_cb)(struct wlan_objmgr_psoc *psoc,
				     struct wmi_twt_resume_dialog_complete_event_param *params);
	void (*twt_notify_cb)(struct wlan_objmgr_psoc *psoc,
			      struct wmi_twt_notify_event_param *params);
	void (*twt_nudge_dialog_cb)(struct wlan_objmgr_psoc *psoc,
		    struct wmi_twt_nudge_dialog_complete_event_param *params);
	void (*twt_ack_comp_cb)(struct wmi_twt_ack_complete_event_param *params,
				void *context);
};
#endif

#ifdef FEATURE_WLAN_APF
/**
 * typedef apf_get_offload_cb - APF offload callback signature
 * @context: Opaque context that the client can use to associate the
 *    callback with the request
 * @caps: APF offload capabilities as reported by firmware
 */
struct sir_apf_get_offload;
typedef void (*apf_get_offload_cb)(void *context,
				   struct sir_apf_get_offload *caps);

/**
 * typedef apf_read_mem_cb - APF read memory response callback
 * @context: Opaque context that the client can use to associate the
 *    callback with the request
 * @evt: APF read memory response event parameters
 */
typedef void (*apf_read_mem_cb)(void *context,
				struct wmi_apf_read_memory_resp_event_params
									  *evt);
#endif /* FEATURE_WLAN_APF */

/**
 * typedef rssi_threshold_breached_cb - RSSI threshold breach callback
 * @hdd_handle: Opaque handle to the HDD context
 * @event: The RSSI breach event
 */
typedef void (*rssi_threshold_breached_cb)(hdd_handle_t hdd_handle,
					   struct rssi_breach_event *event);

/**
 * typedef get_chain_rssi_callback - get chain rssi callback
 * @context: Opaque context that the client can use to associate the
 *    callback with the request
 * @data: chain rssi result reported by firmware
 */
struct chain_rssi_result;
typedef void (*get_chain_rssi_callback)(void *context,
					struct chain_rssi_result *data);

#ifdef FEATURE_FW_STATE
/**
 * typedef fw_state_callback - get firmware state callback
 * @context: Opaque context that the client can use to associate the
 *    callback with the request
 */
typedef void (*fw_state_callback)(void *context);
#endif /* FEATURE_FW_STATE */

typedef void (*tx_queue_cb)(hdd_handle_t hdd_handle, uint32_t vdev_id,
			    enum netif_action_type action,
			    enum netif_reason_type reason);

/**
 * typedef pwr_save_fail_cb - power save fail callback function
 * @hdd_handle: HDD handle registered with SME
 * @params: failure parameters
 */
struct chip_pwr_save_fail_detected_params;
typedef void (*pwr_save_fail_cb)(hdd_handle_t hdd_handle,
			struct chip_pwr_save_fail_detected_params *params);

/**
 * typedef bt_activity_info_cb - bluetooth activity callback function
 * @hdd_handle: HDD handle registered with SME
 * @bt_activity: bluetooth activity information
 */
typedef void (*bt_activity_info_cb)(hdd_handle_t hdd_handle,
				    uint32_t bt_activity);

/**
 * typedef rso_cmd_status_cb - RSO command status  callback function
 * @hdd_handle: HDD handle registered with SME
 * @rso_status: Status of the operation
 */
typedef void (*rso_cmd_status_cb)(hdd_handle_t hdd_handle,
				  struct rso_cmd_status *rso_status);

/**
 * typedef lost_link_info_cb - lost link indication callback function
 * @hdd_handle: HDD handle registered with SME
 * @lost_link_info: Information about the lost link
 */
typedef void (*lost_link_info_cb)(hdd_handle_t hdd_handle,
				  struct sir_lost_link_info *lost_link_info);
/**
 * typedef hidden_ssid_cb - hidden ssid rsp callback fun
 * @hdd_handle: HDD handle registered with SME
 * @vdev_id: Vdev Id
 */
typedef void (*hidden_ssid_cb)(hdd_handle_t hdd_handle,
				uint8_t vdev_id);

/**
 * typedef bcn_report_cb - recv bcn callback fun
 * @hdd_handle: HDD handle registered with SME
 * @beacon_report: Beacon report structure
 */
typedef QDF_STATUS (*beacon_report_cb)
	(hdd_handle_t hdd_handle, struct wlan_beacon_report *beacon_report);

/**
 * beacon_pause_cb : scan start callback fun
 * @hdd_handler: HDD handler
 * @vdev_id: vdev id
 * @type: scan event type
 * @is_disconnected: Driver is in dis connected state or not
 */
typedef void (*beacon_pause_cb)(hdd_handle_t hdd_handle,
				uint8_t vdev_id,
				enum scan_event_type type,
				bool is_disconnected);

/**
 * typedef sme_get_isolation_cb - get isolation callback fun
 * @param: isolation result reported by firmware
 * @pcontext: Opaque context that the client can use to associate the
 *    callback with the request
 */
typedef void (*sme_get_isolation_cb)(struct sir_isolation_resp *param,
				     void *pcontext);

#ifdef WLAN_FEATURE_MOTION_DETECTION
typedef QDF_STATUS (*md_host_evt_cb)(void *hdd_ctx, struct sir_md_evt *event);
typedef QDF_STATUS (*md_bl_evt_cb)(void *hdd_ctx, struct sir_md_bl_evt *event);
#endif /* WLAN_FEATURE_MOTION_DETECTION */

struct sme_context {
	eSmeState state;
	qdf_mutex_t sme_global_lock;
	uint32_t sme_cmd_count;
	/* following pointer contains array of pointers for tSmeCmd* */
	void **sme_cmd_buf_addr;
	tDblLinkList sme_cmd_freelist;    /* preallocated roam cmd list */
	enum QDF_OPMODE curr_device_mode;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	host_event_wlan_status_payload_type eventPayload;
#endif
	void *ll_stats_context;
	link_layer_stats_cb link_layer_stats_cb;
	void (*link_layer_stats_ext_cb)(hdd_handle_t callback_ctx,
					tSirLLStatsResults *rsp);
#ifdef WLAN_POWER_DEBUG
	void *power_debug_stats_context;
	void (*power_stats_resp_callback)(struct power_stats_response *rsp,
						void *callback_context);
	void (*sme_power_debug_stats_callback)(
					struct mac_context *mac,
					struct power_stats_response *response);
#endif
#ifdef WLAN_FEATURE_BEACON_RECEPTION_STATS
	void *beacon_stats_context;
	void (*beacon_stats_resp_callback)(struct bcn_reception_stats_rsp *rsp,
					   void *callback_context);
#endif
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	void (*auto_shutdown_cb)(void);
#endif
	/* Maximum interfaces allowed by the host */
	uint8_t max_intf_count;
	stats_ext_cb stats_ext_cb;
	stats_ext2_cb stats_ext2_cb;
	/* linkspeed callback */
	sme_link_speed_cb link_speed_cb;
	void *link_speed_context;

	sme_get_isolation_cb get_isolation_cb;
	void *get_isolation_cb_context;
#ifdef FEATURE_WLAN_EXTSCAN
	ext_scan_ind_cb ext_scan_ind_cb;
#endif /* FEATURE_WLAN_EXTSCAN */
	csr_link_status_callback link_status_callback;
	void *link_status_context;
	int (*get_tsf_cb)(void *pcb_cxt, struct stsf *ptsf);
	void *get_tsf_cxt;
	/* get temperature event context and callback */
	void *temperature_cb_context;
	void (*temperature_cb)(int temperature, void *context);
	uint8_t miracast_value;
	struct ps_global_info  ps_global_info;
	rssi_threshold_breached_cb rssi_threshold_breached_cb;
	sme_set_thermal_level_callback set_thermal_level_cb;
	void *apf_get_offload_context;
#ifdef FEATURE_P2P_LISTEN_OFFLOAD
	p2p_lo_callback p2p_lo_event_callback;
	void *p2p_lo_event_context;
#endif
#ifdef FEATURE_OEM_DATA_SUPPORT
	sme_send_oem_data_rsp_msg oem_data_rsp_callback;
#endif
	lost_link_info_cb lost_link_info_cb;

	bool (*set_connection_info_cb)(bool);
	bool (*get_connection_info_cb)(uint8_t *session_id,
			enum scan_reject_states *reason);
	rso_cmd_status_cb rso_cmd_status_cb;
	pwr_save_fail_cb chip_power_save_fail_cb;
	bt_activity_info_cb bt_activity_info_cb;
	void *get_arp_stats_context;
	void (*get_arp_stats_cb)(void *, struct rsp_stats *, void *);
	get_chain_rssi_callback get_chain_rssi_cb;
	void *get_chain_rssi_context;
#ifdef FEATURE_FW_STATE
	fw_state_callback fw_state_cb;
	void *fw_state_context;
#endif /* FEATURE_FW_STATE */
	tx_queue_cb tx_queue_cb;
#ifdef WLAN_SUPPORT_TWT
	twt_enable_cb twt_enable_cb;
	twt_disable_cb twt_disable_cb;
	twt_add_dialog_cb twt_add_dialog_cb;
	twt_del_dialog_cb twt_del_dialog_cb;
	twt_pause_dialog_cb twt_pause_dialog_cb;
	twt_nudge_dialog_cb twt_nudge_dialog_cb;
	twt_resume_dialog_cb twt_resume_dialog_cb;
	twt_notify_cb twt_notify_cb;
	twt_ack_comp_cb twt_ack_comp_cb;
	void *twt_ack_context_cb;
#endif
#ifdef FEATURE_WLAN_APF
	apf_get_offload_cb apf_get_offload_cb;
	apf_read_mem_cb apf_read_mem_cb;
#endif
#ifdef WLAN_FEATURE_MOTION_DETECTION
	md_host_evt_cb md_host_evt_cb;
	md_bl_evt_cb md_bl_evt_cb;
	void *md_ctx;
#endif /* WLAN_FEATURE_MOTION_DETECTION */
	/* hidden ssid rsp callback */
	hidden_ssid_cb hidden_ssid_cb;
#ifdef WLAN_MWS_INFO_DEBUGFS
	void *mws_coex_info_ctx;
	void (*mws_coex_info_state_resp_callback)(void *coex_info_data,
						  void *context,
						  wmi_mws_coex_cmd_id cmd_id);
#endif /* WLAN_MWS_INFO_DEBUGFS */

#ifdef WLAN_BCN_RECV_FEATURE
	beacon_report_cb beacon_report_cb;
	beacon_pause_cb beacon_pause_cb;
#endif
#ifdef FEATURE_OEM_DATA
	void (*oem_data_event_handler_cb)
			(const struct oem_data *oem_event_data,
			 uint8_t vdev_id);
	uint8_t oem_data_vdev_id;
#endif
	sme_get_raom_scan_ch_callback roam_scan_ch_callback;
	void *roam_scan_ch_get_context;
#ifdef FEATURE_MONITOR_MODE_SUPPORT
	void (*monitor_mode_cb)(uint8_t vdev_id);
#endif
#if defined(CLD_PM_QOS) && defined(WLAN_FEATURE_LL_MODE)
	void (*beacon_latency_event_cb)(uint32_t latency_level);
#endif
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	void (*roam_rt_stats_cb)(hdd_handle_t hdd_handle,
				 struct mlme_roam_debug_info *roam_stats);
#endif
};

#endif /* #if !defined( __SMEINTERNAL_H ) */
