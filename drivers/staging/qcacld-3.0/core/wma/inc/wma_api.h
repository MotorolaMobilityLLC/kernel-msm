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

#ifndef WMA_API_H
#define WMA_API_H

#include "osdep.h"
#include "ani_global.h"
#include "a_types.h"
#include "osapi_linux.h"
#include "wmi_unified.h"
#ifdef NOT_YET
#include "htc_api.h"
#endif
#include "lim_global.h"
#include "cds_utils.h"
#include "scheduler_api.h"
#include "wlan_policy_mgr_api.h"
#include "wma_sar_public_structs.h"
#include <cdp_txrx_ops.h>
#include "include/wlan_vdev_mlme.h"
#include "wlan_mlme_vdev_mgr_interface.h"

typedef void *WMA_HANDLE;

/**
 * enum GEN_PARAM - general parameters
 * @GEN_VDEV_PARAM_AMPDU: Set ampdu size
 * @GEN_VDEV_PARAM_AMSDU: Set amsdu size
 * @GEN_PARAM_CRASH_INJECT: inject crash
 * @GEN_PARAM_CAPTURE_TSF: read tsf
 * @GEN_PARAM_RESET_TSF_GPIO: reset tsf gpio
 * @GEN_VDEV_ROAM_SYNCH_DELAY: roam sync delay
 * @GEN_VDEV_PARAM_TX_AMPDU: Set tx ampdu size
 * @GEN_VDEV_PARAM_RX_AMPDU: Set rx ampdu size
 * @GEN_VDEV_PARAM_TX_AMSDU: Set tx amsdu size
 * @GEN_VDEV_PARAM_RX_AMSDU: Set rx amsdu size
 */
enum GEN_PARAM {
	GEN_VDEV_PARAM_AMPDU = 0x1,
	GEN_VDEV_PARAM_AMSDU,
	GEN_PARAM_CRASH_INJECT,
	GEN_PARAM_CAPTURE_TSF,
	GEN_PARAM_RESET_TSF_GPIO,
	GEN_VDEV_ROAM_SYNCH_DELAY,
	GEN_VDEV_PARAM_TX_AMPDU,
	GEN_VDEV_PARAM_RX_AMPDU,
	GEN_VDEV_PARAM_TX_AMSDU,
	GEN_VDEV_PARAM_RX_AMSDU,
};

/**
 * struct wma_caps_per_phy - various caps per phy
 * @ht_2g: entire HT cap for 2G band in terms of 32 bit flag
 * @ht_5g: entire HT cap for 5G band in terms of 32 bit flag
 * @vht_2g: entire VHT cap for 2G band in terms of 32 bit flag
 * @vht_5g: entire VHT cap for 5G band in terms of 32 bit flag
 * @he_2g: entire HE cap for 2G band in terms of 32 bit flag
 * @he_5g: entire HE cap for 5G band in terms of 32 bit flag
 * @tx_chain_mask_2G: tx chain mask for 2g
 * @rx_chain_mask_2G: rx chain mask for 2g
 * @tx_chain_mask_5G: tx chain mask for 5g
 * @rx_chain_mask_5G: rx chain mask for 5g
 */
struct wma_caps_per_phy {
	uint32_t ht_2g;
	uint32_t ht_5g;
	uint32_t vht_2g;
	uint32_t vht_5g;
	uint32_t he_2g[PSOC_HOST_MAX_MAC_SIZE];
	uint32_t he_5g[PSOC_HOST_MAX_MAC_SIZE];
	uint32_t tx_chain_mask_2G;
	uint32_t rx_chain_mask_2G;
	uint32_t tx_chain_mask_5G;
	uint32_t rx_chain_mask_5G;
};


#define VDEV_CMD 1
#define PDEV_CMD 2
#define GEN_CMD  3
#define DBG_CMD  4
#define PPS_CMD  5
#define QPOWER_CMD 6
#define GTX_CMD  7

typedef void (*wma_peer_authorized_fp) (uint32_t vdev_id);


QDF_STATUS wma_pre_start(void);

QDF_STATUS wma_mc_process_handler(struct scheduler_msg *msg);

QDF_STATUS wma_start(void);

/**
 * wma_stop() - wma stop function.
 *
 * Performs all of the operations required to stop the WMA layer
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF Error on failure
 */
QDF_STATUS wma_stop(void);

QDF_STATUS wma_close(void);

QDF_STATUS wma_wmi_service_close(void);

QDF_STATUS wma_wmi_work_close(void);

int wma_rx_ready_event(void *handle, uint8_t *ev, uint32_t len);

int  wma_rx_service_ready_event(void *handle, uint8_t *ev, uint32_t len);

int wma_rx_service_ready_ext_event(void *handle, uint8_t *ev, uint32_t len);

QDF_STATUS wma_wait_for_ready_event(WMA_HANDLE handle);

int wma_cli_get_command(int vdev_id, int param_id, int vpdev);
int wma_cli_set_command(int vdev_id, int param_id, int sval, int vpdev);
int wma_cli_set2_command(int vdev_id, int param_id, int sval1,
			 int sval2, int vpdev);

/**
 * wma_get_fw_phy_mode_for_freq_cb() - Callback to get current PHY Mode.
 * @freq: channel freq
 * @chan_width: maximum channel width possible
 * @phy_mode: firmware PHY Mode
 *
 * Return: None
 */
void wma_get_fw_phy_mode_for_freq_cb(uint32_t freq, uint32_t chan_width,
				     uint32_t  *phy_mode);
/**
 * wma_get_phy_mode_cb() - Callback to get current PHY Mode.
 * @freq: channel frequency
 * @chan_width: maximum channel width possible
 * @phy_mode: PHY Mode
 *
 * Return: None
 */
void wma_get_phy_mode_cb(qdf_freq_t freq, uint32_t chan_width,
			 enum wlan_phymode  *phy_mode);

QDF_STATUS wma_set_htconfig(uint8_t vdev_id, uint16_t ht_capab, int value);

void wma_set_peer_authorized_cb(void *wma_ctx, wma_peer_authorized_fp auth_cb);
QDF_STATUS wma_set_peer_param(void *wma_ctx, uint8_t *peer_addr,
		  uint32_t param_id,
		  uint32_t param_value, uint32_t vdev_id);
QDF_STATUS wma_get_link_speed(WMA_HANDLE handle,
			      struct link_speed_info *pLinkSpeed);
#ifdef NOT_YET
QDF_STATUS wma_update_channel_list(WMA_HANDLE handle, void *scan_chan_info);
#endif

/**
 * wma_get_vdev_address_by_vdev_id() - lookup MAC address from vdev ID
 * @vdev_id: vdev id
 *
 * Return: mac address
 */
uint8_t *wma_get_vdev_address_by_vdev_id(uint8_t vdev_id);
struct wma_txrx_node *wma_get_interface_by_vdev_id(uint8_t vdev_id);
QDF_STATUS wma_get_connection_info(uint8_t vdev_id,
		struct policy_mgr_vdev_entry_info *conn_table_entry);
QDF_STATUS wma_ndi_update_connection_info(uint8_t vdev_id,
		struct nan_datapath_channel_info *ndp_chan_info);

#ifdef WLAN_FEATURE_PKT_CAPTURE
int wma_get_rmf_status(uint8_t vdev_id);
#endif

bool wma_is_vdev_up(uint8_t vdev_id);

void *wma_get_beacon_buffer_by_vdev_id(uint8_t vdev_id, uint32_t *buffer_size);

bool wma_get_fw_wlan_feat_caps(enum cap_bitmap feature);
void wma_set_fw_wlan_feat_caps(enum cap_bitmap feature);

QDF_STATUS wma_post_ctrl_msg(struct mac_context *mac, struct scheduler_msg *pMsg);

void wma_update_intf_hw_mode_params(uint32_t vdev_id, uint32_t mac_id,
				uint32_t cfgd_hw_mode_index);
#ifdef MPC_UT_FRAMEWORK
void wma_set_dbs_capability_ut(uint32_t dbs);
#else
static inline void wma_set_dbs_capability_ut(uint32_t dbs) {}
#endif
QDF_STATUS wma_get_caps_for_phyidx_hwmode(struct wma_caps_per_phy *caps_per_phy,
		enum hw_mode_dbs_capab hw_mode, enum cds_band_type band);
bool wma_is_rx_ldpc_supported_for_channel(uint32_t ch_freq);

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
int wma_unified_radio_tx_mem_free(void *handle);

/*
 * wma_unified_link_stats_results_mem_free() - Free the memory for
 * link_stats_results->results allocated when event comes.
 * @link_stats_results: pointer to the memory that is to be freed
 *
 * Return: None
 */
void
wma_unified_link_stats_results_mem_free(tSirLLStatsResults *link_stats_results);

#else /* WLAN_FEATURE_LINK_LAYER_STATS */
static inline int wma_unified_radio_tx_mem_free(void *handle)
{
	return 0;
}
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

/**
 * wma_form_unit_test_cmd_and_send() - to form a wma command and send it to FW
 * @session_id: wma session id to be filled while forming the command
 * @module_id: module id given by user to be filled in the command
 * @arg_count: number of argument count
 * @arg: pointer to argument list
 *
 * This API exposed to HDD layer which takes the argument from user and forms
 * the wma unit test command to be sent down to firmware
 *
 * Return: QDF_STATUS based on overall success
 */
QDF_STATUS wma_form_unit_test_cmd_and_send(uint32_t vdev_id,
		uint32_t module_id, uint32_t arg_count, uint32_t *arg);

/**
 * wma_lro_init() - sends LRO configuration to FW
 * @lro_config:         pointer to the config parameters
 *
 * This function ends LRO configuration to FW.
 *
 * Return: 0 for success or reasons for failure
 */
int wma_lro_init(struct cdp_lro_hash_config *lro_config);

QDF_STATUS wma_remove_beacon_filter(WMA_HANDLE wma,
				struct beacon_filter_param *filter_params);

QDF_STATUS wma_add_beacon_filter(WMA_HANDLE wma,
				struct beacon_filter_param *filter_params);
QDF_STATUS wma_send_adapt_dwelltime_params(WMA_HANDLE handle,
			struct adaptive_dwelltime_params *dwelltime_params);

/**
 * wma_send_dbs_scan_selection_params() - send DBS scan selection configuration
 * params to firmware
 * @handle: wma handler
 * @dbs_scan_params: pointer to wmi_dbs_scan_sel_params
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF failure reason code for failure
 */
QDF_STATUS wma_send_dbs_scan_selection_params(WMA_HANDLE handle,
			struct wmi_dbs_scan_sel_params *dbs_scan_params);
QDF_STATUS wma_set_tx_power_scale(uint8_t vdev_id, int value);
QDF_STATUS wma_set_tx_power_scale_decr_db(uint8_t vdev_id, int value);

bool wma_is_csa_offload_enabled(void);
/**
 * wma_is_mbssid_enabled - checks MBSSID support
 *
 * Return: true or false
 */
bool wma_is_mbssid_enabled(void);

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
bool wma_is_p2p_lo_capable(void);
#else
static inline bool wma_is_p2p_lo_capable(void)
{
	return 0;
}
#endif
bool wma_capability_enhanced_mcast_filter(void);
void wma_process_pdev_hw_mode_trans_ind(void *wma,
	wmi_pdev_hw_mode_transition_event_fixed_param *fixed_param,
	wmi_pdev_set_hw_mode_response_vdev_mac_entry *vdev_mac_entry,
	struct sir_hw_mode_trans_ind *hw_mode_trans_ind);

/**
 * wma_set_cts2self_for_p2p_go() - set CTS2SELF command for P2P GO.
 * @wma_handle:                  pointer to wma handle.
 * @cts2self_for_p2p_go:         value needs to set to firmware.
 *
 * At the time of driver startup, inform about ini parma to FW that
 * if legacy client connects to P2P GO, stop using NOA for P2P GO.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wma_set_cts2self_for_p2p_go(void *wma_handle,
		uint32_t cts2self_for_p2p_go);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wma_get_roam_scan_ch() - API to get roam scan channel list.
 * @wma_handle: pointer to wma handle.
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wma_get_roam_scan_ch(wmi_unified_t wma,
				uint8_t vdev_id);
#else
static inline
QDF_STATUS wma_get_roam_scan_ch(wmi_unified_t wma,
				uint8_t vdev_id)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

/**
 * wma_set_tx_rx_aggr_size() - set tx rx aggregation size
 * @vdev_id: vdev id
 * @tx_size: tx aggr size
 * @rx_size: rx aggr size
 * @aggr_type: aggregation type
 *
 *  This function try to set the aggregation size.
 *
 *  Return: QDF_STATUS enumeration
 */
QDF_STATUS wma_set_tx_rx_aggr_size(uint8_t vdev_id,
				   uint32_t tx_size,
				   uint32_t rx_size,
				   wmi_vdev_custom_aggr_type_t aggr_type);
/**
 * wma_set_tx_rx_aggr_size_per_ac() - set aggregation size per ac
 * @wma_handle: pointer to wma handle.
 * @vdev_id: vdev_id
 * @qos_aggr: QoS data
 * @aggr_type: aggregation type
 *
 * This function try to set the aggregation size per AC.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS
wma_set_tx_rx_aggr_size_per_ac(WMA_HANDLE wma_handle,
			       uint8_t vdev_id,
			       struct wlan_mlme_qos *qos_aggr,
			       wmi_vdev_custom_aggr_type_t aggr_type);

/**
 * wma_set_sw_retry_threshold() - set sw retry threshold per vdev
 * @vdev_id: vdev id
 * @sw_retry_count: sw retry number
 * @retry_type: SW vdev retry type
 *
 * This function sends WMI command to set the sw retry threshold per vdev.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wma_set_vdev_sw_retry_th(uint8_t vdev_id, uint8_t sw_retry_count,
				    wmi_vdev_custom_sw_retry_type_t retry_type);

/**
 * wma_set_sw_retry_threshold_per_ac() - set sw retry threshold per AC for tx
 * @handle: wma handle
 * @vdev_id: vdev id
 * @qos_aggr: pointer to QOS TX/RX aggregation values
 *
 * This function sends WMI command to set the sw retry threshold per AC
 * for Tx.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wma_set_sw_retry_threshold_per_ac
	(WMA_HANDLE handle,
	 uint8_t vdev_id, struct wlan_mlme_qos *qos_aggr);
/**
 * wma_set_sw_retry_threshold() - set sw retry threshold for tx
 * @vdev_id: vdev
 * @retry: retry number
 * @param_id: aggregrate sw retry threshold param id
 *
 * This function sends WMI command to set the sw retry threshold for Tx.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wma_set_sw_retry_threshold(uint8_t vdev_id, uint32_t retry,
				      uint32_t param_id);

/**
 * wma_get_sar_limit() - get SAR limits from the target
 * @handle: wma handle
 * @callback: Callback function to invoke with the results
 * @context: Opaque context to pass back to caller in the callback
 *
 *  This function sends WMI command to get SAR limits.
 *
 *  Return: QDF_STATUS enumeration
 */
QDF_STATUS wma_get_sar_limit(WMA_HANDLE handle,
			     wma_sar_cb callback, void *context);

/**
 * wma_set_sar_limit() - set sar limits in the target
 * @handle: wma handle
 * @sar_limit_cmd_params: sar limit cmd params
 *
 *  This function sends WMI command to set SAR limits.
 *
 *  Return: QDF_STATUS enumeration
 */
QDF_STATUS wma_set_sar_limit(WMA_HANDLE handle,
		struct sar_limit_cmd_params *sar_limit_params);

/**
 * wma_send_coex_config_cmd() - Send coex config params
 * @wma_handle: wma handle
 * @coex_cfg_params: struct to coex cofig params
 *
 * This function sends WMI command to send coex cofig params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_send_coex_config_cmd(WMA_HANDLE wma_handle,
				    struct coex_config_params *coex_cfg_params);

/**
 * wma_set_power_config() - update power config in wma
 * @vdev_id:	the Id of the vdev to configure
 * @power:	new power value
 *
 * Return: QDF_STATUS_SUCCESS on success, error number otherwise
 */
QDF_STATUS wma_set_power_config(uint8_t vdev_id, enum powersave_mode power);

#ifdef FEATURE_WLAN_D0WOW
static inline bool wma_d0_wow_is_supported(void)
{
	return true;
}
#else
static inline bool wma_d0_wow_is_supported(void)
{
	return false;
}
#endif

/**
 * wma_store_pdev() - store pdev
 * @wma_ctx:	wma context
 * @pdev:	pdev context
 *
 * Return: void
 */
void wma_store_pdev(void *wma_ctx, struct wlan_objmgr_pdev *pdev);

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
/**
 * wmi_to_sir_peer_type() - convert peer type from WMI to SIR enum
 * @type: enum wmi_peer_type
 *
 * Return: tSirWifiPeerType
 */
tSirWifiPeerType wmi_to_sir_peer_type(enum wmi_peer_type type);
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

QDF_STATUS wma_crash_inject(WMA_HANDLE wma_handle, uint32_t type,
			    uint32_t delay_time_ms);

/**
 * wma_critical_events_in_flight() - get the number of critical events in flight
 *
 * This API gets the number of events in flight which should prevent power
 * collapse.
 *
 * Return: the number of critical events in flight
 */
uint32_t wma_critical_events_in_flight(void);

#ifdef FEATURE_WLAN_DYNAMIC_CVM
/**
 * wma_set_vc_mode_config() - set voltage corner mode config to FW.
 * @wma_handle:	pointer to wma handle.
 * @vc_bitmap:	value needs to set to firmware.
 *
 * At the time of driver startup, set operating voltage corner mode
 * for differenet phymode and bw configurations.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS wma_set_vc_mode_config(void *wma_handle,
		uint32_t vc_bitmap);
#endif

QDF_STATUS wma_process_dhcp_ind(WMA_HANDLE wma_handle,
				tAniDHCPInd *ta_dhcp_ind);

/**
 * wma_wmi_stop() - send wmi stop cmd
 *
 *  Return: None
 */
void wma_wmi_stop(void);

/**
 * wma_get_mcs_idx() - get mcs index
 * @raw_rate: raw rate from fw
 * @rate_flags: rate flags
 * @is_he_mcs_12_13_supported: is he mcs12/13 supported
 * @nss: nss
 * @dcm: dcm
 * @guard_interval: guard interval
 * @mcs_rate_flag: mcs rate flags
 *
 *  Return: mcs index
 */
uint8_t wma_get_mcs_idx(uint16_t raw_rate, enum tx_rate_info rate_flags,
			bool is_he_mcs_12_13_supported,
			uint8_t *nss, uint8_t *dcm,
			enum txrate_gi *guard_interval,
			enum tx_rate_info *mcs_rate_flag);

/**
 * wma_get_hidden_ssid_restart_in_progress() - check if hidden ssid restart is
 * in progress
 * @iface: iface pointer
 *
 * Return: true if hidden ssid restart is in progress else false
 */
bool wma_get_hidden_ssid_restart_in_progress(struct wma_txrx_node *iface);

/**
 * wma_get_channel_switch_in_progress() - check if channel switch is in progress
 * @iface: iface pointer
 *
 * Return: true if channel switch is in progress else false
 */
bool wma_get_channel_switch_in_progress(struct wma_txrx_node *iface);

/**
 * wma_sta_mlme_vdev_start_continue() - VDEV start response handling
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV start response actions
 *
 * Return: SUCCESS on successful completion of start response operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_sta_mlme_vdev_start_continue(struct vdev_mlme_obj *vdev_mlme,
					    uint16_t data_len, void *data);

/**
 * wma_sta_mlme_vdev_roam_notify() - VDEV roam notify handling
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV roam event handling
 *
 * Return: SUCCESS on successful completion of roam event handling
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_sta_mlme_vdev_roam_notify(struct vdev_mlme_obj *vdev_mlme,
					 uint16_t data_len, void *data);

/**
 * wma_ap_mlme_vdev_start_continue() - VDEV start response handling
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV start response actions
 *
 * Return: SUCCESS on successful completion of start response operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_ap_mlme_vdev_start_continue(struct vdev_mlme_obj *vdev_mlme,
					   uint16_t data_len, void *data);

/**
 * wma_sta_vdev_up_send() - Send VDEV UP command
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV UP Command
 *
 * Return: SUCCESS on successful completion of start response operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_sta_vdev_up_send(struct vdev_mlme_obj *vdev_mlme,
				uint16_t data_len, void *data);

/**
 * wma_mlme_vdev_stop_continue() - VDEV stop response handling
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV stop response actions
 *
 * Return: SUCCESS on successful completion of stop response operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_mlme_vdev_stop_continue(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data);

/**
 * wma_ap_mlme_vdev_down_send() - VDEV down operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV down operation
 *
 * Return: SUCCESS on successful completion of VDEV down operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_ap_mlme_vdev_down_send(struct vdev_mlme_obj *vdev_mlme,
				      uint16_t data_len, void *data);

/**
 * wma_mlme_vdev_notify_down_complete() - VDEV init state transition
 * notification
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API notifies MLME on moving to INIT state
 *
 * Return: SUCCESS on successful completion of down notification
 *         FAILURE, if it fails due to any
 */
QDF_STATUS
wma_mlme_vdev_notify_down_complete(struct vdev_mlme_obj *vdev_mlme,
				   uint16_t data_len, void *data);

/**
 * wma_ap_mlme_vdev_stop_start_send() - handle vdev stop during start req
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @type: restart req or start req
 * @data_len: data size
 * @data: event data
 *
 * API handle vdev stop during start req
 *
 * Return: SUCCESS alsways
 */
QDF_STATUS wma_ap_mlme_vdev_stop_start_send(struct vdev_mlme_obj *vdev_mlme,
					    enum vdev_cmd_type type,
					    uint16_t data_len, void *data);

/**
 * wma_sta_mlme_vdev_down_send() - VDEV down operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV down operation
 *
 * Return: SUCCESS on successful completion of VDEV down operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_sta_mlme_vdev_down_send(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data);
/**
 * wma_post_vdev_create_setup() - Post vdev create setup
 * @vdev: vdev obj
 *
 * This API is invoked after vded is created to perform post
 * vdev create operations i.e. creating peer and setting vdev params.
 *
 * Return: SUCCESS on successful post vdev operations, FAILURE, if it
 *         fails due to any
 */
QDF_STATUS wma_post_vdev_create_setup(struct wlan_objmgr_vdev *vdev);

/**
 * wma_mon_mlme_vdev_start_continue() - VDEV start response handling
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV start response actions
 *
 * Return: SUCCESS on successful completion of start response operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_mon_mlme_vdev_start_continue(struct vdev_mlme_obj *vdev_mlme,
					    uint16_t data_len, void *data);

/**
 * wma_mon_mlme_vdev_up_send() - Send VDEV UP command
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV UP Command
 *
 * Return: SUCCESS on successful completion of start response operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_mon_mlme_vdev_up_send(struct vdev_mlme_obj *vdev_mlme,
				     uint16_t data_len, void *data);

/**
 * wma_mon_mlme_vdev_stop_send() - VDEV stop operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV stop operation
 *
 * Return: SUCCESS on successful completion of VDEV stop operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_mon_mlme_vdev_stop_send(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data);

/**
 * wma_mon_mlme_vdev_down_send() - VDEV down operation
 * @vdev_mlme_obj:  VDEV MLME comp object
 * @data_len: data size
 * @data: event data
 *
 * API invokes VDEV down operation
 *
 * Return: SUCCESS on successful completion of VDEV down operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_mon_mlme_vdev_down_send(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data);

/**
 * wma_vdev_detach_callback() - VDEV delete response handler
 * @rsp: pointer to vdev delete response
 *
 * This API proccesses vdev delete response and gives to upper layers
 *
 * Return: SUCCESS on successful completion of VDEV delete operation
 *         FAILURE, if it fails due to any
 */
QDF_STATUS wma_vdev_detach_callback(struct vdev_delete_response *rsp);

/**
 * wma_vdev_stop_resp_handler() - vdev stop response handler
 * @handle: wma handle
 * @cmd_param_info: event buffer
 * @len: buffer length
 *
 * Return: 0 for success or error code
 */
QDF_STATUS wma_vdev_stop_resp_handler(struct vdev_mlme_obj *vdev_mlme,
				struct vdev_stop_response *rsp);

/**
 * wma_vdev_start_resp_handler() - vdev start response handler
 * @vdev_mlme: vdev mlme obj
 * @rsp: vdev start response
 *
 * Return: QDF status
 */
QDF_STATUS wma_vdev_start_resp_handler(struct vdev_mlme_obj *vdev_mlme,
				       struct vdev_start_response *rsp);

#ifdef FEATURE_WLM_STATS
/**
 * typedef wma_wlm_stats_cb() - Callback function for WLM stats
 * @cookie: Cookie provided by client during callback registration
 * @data: Hex ASCII representation of the WLM stats
 */
typedef void (*wma_wlm_stats_cb)(void *cookie, const char *data);

/**
 * wma_wlm_stats_req() - Send a req to WLAN Latency Manager in FW
 * @vdev_id: vdev id to be sent to FW's WLM
 * @bitmask: A bitmask which is requested by user to be sent to FW's WLM
 * @max_size: Size of user's buffer to store the response
 * @cb: A callback to be called to once response is available
 * @cookie: A cookie to be used by callback to retrieve the context of req
 *
 * This API is used to send a message to WLAN latency manager component
 * in FW to retrieve some latency related data and send it to user space.
 * Driver is just a pass-through for user to interract with FW.
 *
 * Return: 0 on success and non-zero for error
 */
int wma_wlm_stats_req(int vdev_id, uint32_t bitmask, uint32_t max_size,
		      wma_wlm_stats_cb cb, void *cookie);

/**
 * wma_wlm_stats_rsp() - Handler to handle the response from FW's WLM component
 * @wma_ctx: WMA context
 * @event: WMI TLV event data
 * @len: WMI TLV length
 *
 * This API is registered with WMI component in order to handle the response
 * coming from FW's WLM correspondence to WLM REQ to FW. This API takes the
 * data coming as HEX stream and write in to CHAR buffer as HEX CHAR stream
 * and send this char buffer to user space through callback.
 *
 * Return: 0 on success and non-zero for error
 */
int wma_wlm_stats_rsp(void *wma_ctx, uint8_t *event, uint32_t len);
#endif /* FEATURE_WLM_STATS */

/**
 * wma_self_peer_create() - create self peer in objmgr
 * @vdev_mlme: vdev mlme component private object
 *
 * Create the self peer in firmware for beaconing vdev's and create then
 * object manager self-peer for the vdev.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_vdev_self_peer_create(struct vdev_mlme_obj *vdev_mlme);

/**
 * wma_cleanup_vdev() - cleanup wma layers vdev
 * @vdev: Object manager vdev
 *
 * This function cleansup the wma layers vdev related data.
 *
 * Return: None
 */
void wma_cleanup_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * wma_set_wakeup_logs_to_console() - Enable/disable wakeup logs to console
 * @value: boolean value
 *
 * API to enable/disable wow host wakeup event logs to console.
 *
 * Return: None
 */
void wma_set_wakeup_logs_to_console(bool value);

#endif /* WMA_API_H */
