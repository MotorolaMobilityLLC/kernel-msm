/*
 * Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: Implement API's specific to ROAMING component.
 */

#ifndef _WMI_UNIFIED_ROAM_API_H_
#define _WMI_UNIFIED_ROAM_API_H_

#include <wmi_unified_roam_param.h>
#include "wlan_cm_roam_public_struct.h"

#ifdef FEATURE_LFR_SUBNET_DETECTION
/**
 * wmi_unified_set_gateway_params_cmd() - set gateway parameters
 * @wmi_handle: wmi handle
 * @req: gateway parameter update request structure
 *
 * This function reads the incoming @req and fill in the destination
 * WMI structure and sends down the gateway configs down to the firmware
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failures;
 *         error number otherwise
 */
QDF_STATUS
wmi_unified_set_gateway_params_cmd(wmi_unified_t wmi_handle,
				   struct gateway_update_req_param *req);
#endif

#ifdef FEATURE_RSSI_MONITOR
/**
 * wmi_unified_set_rssi_monitoring_cmd() - set rssi monitoring
 * @wmi_handle: wmi handle
 * @req: rssi monitoring request structure
 *
 * This function reads the incoming @req and fill in the destination
 * WMI structure and send down the rssi monitoring configs down to the firmware
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failures;
 *         error number otherwise
 */
QDF_STATUS
wmi_unified_set_rssi_monitoring_cmd(wmi_unified_t wmi_handle,
				    struct rssi_monitor_param *req);
#endif

/**
 * wmi_unified_roam_scan_offload_rssi_thresh_cmd() - set roam scan rssi
 *							parameters
 * @wmi_handle: wmi handle
 * @roam_req: roam rssi related parameters
 *
 * This function reads the incoming @roam_req and fill in the destination
 * WMI structure and send down the roam scan rssi configs down to the firmware
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_roam_scan_offload_rssi_thresh_cmd(
		wmi_unified_t wmi_handle,
		struct wlan_roam_offload_scan_rssi_params *roam_req);

/**
 * wmi_unified_roam_scan_offload_scan_period() - set roam offload scan period
 * @wmi_handle: wmi handle
 * @param: pointer to roam scan period params to be sent to fw
 *
 * Send WMI_ROAM_SCAN_PERIOD parameters to fw.
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_roam_scan_offload_scan_period(
	wmi_unified_t wmi_handle, struct wlan_roam_scan_period_params *param);

/**
 * wmi_unified_roam_mawc_params_cmd() - configure roaming MAWC parameters
 * @wmi_handle: wmi handle
 * @params: Parameters to be configured
 *
 * Pass the MAWC(Motion Aided wireless connectivity) related roaming
 * parameters from the host to the target
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_unified_roam_mawc_params_cmd(wmi_unified_t wmi_handle,
				 struct wlan_roam_mawc_params *params);

/**
 * wmi_unified_roam_scan_filter_cmd() - send roam scan whitelist,
 *                                      blacklist and preferred list
 * @wmi_handle: wmi handle
 * @roam_req: roam scan lists related parameters
 *
 * This function reads the incoming @roam_req and fill in the destination
 * WMI structure and send down the different roam scan lists down to the fw
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_unified_roam_scan_filter_cmd(wmi_unified_t wmi_handle,
				 struct roam_scan_filter_params *roam_req);

#ifdef FEATURE_WLAN_ESE
/**
 * wmi_unified_plm_stop_cmd() - plm stop request
 * @wmi_handle: wmi handle
 * @plm: plm request parameters
 *
 * This function request FW to stop PLM.
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_plm_stop_cmd(wmi_unified_t wmi_handle,
				    const struct plm_req_params *plm);

/**
 * wmi_unified_plm_start_cmd() - plm start request
 * @wmi_handle: wmi handle
 * @plm: plm request parameters
 *
 * This function request FW to start PLM.
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_plm_start_cmd(wmi_unified_t wmi_handle,
				     const struct plm_req_params *plm);
#endif /* FEATURE_WLAN_ESE */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/* wmi_unified_set_ric_req_cmd() - set ric request element
 * @wmi_handle: wmi handle
 * @msg: message
 * @is_add_ts: is addts required
 *
 * This function sets ric request element for 11r roaming.
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_set_ric_req_cmd(wmi_unified_t wmi_handle, void *msg,
				       uint8_t is_add_ts);

/**
 * wmi_unified_roam_synch_complete_cmd() - roam synch complete command to fw.
 * @wmi_handle: wmi handle
 * @vdev_id: vdev id
 *
 * This function sends roam synch complete event to fw.
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_roam_synch_complete_cmd(wmi_unified_t wmi_handle,
					       uint8_t vdev_id);

/**
 * wmi_unified__roam_invoke_cmd() - send roam invoke command to fw.
 * @wmi_handle: wmi handle
 * @roaminvoke: roam invoke command
 * @ch_hz: channel
 *
 * Send roam invoke command to fw for fastreassoc.
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_unified_roam_invoke_cmd(wmi_unified_t wmi_handle,
			    struct wmi_roam_invoke_cmd *roaminvoke,
			    uint32_t ch_hz);

/**
 * wmi_unified_set_roam_triggers() - send roam trigger bitmap
 * @wmi_handle: wmi handle
 * @triggers: Roam trigger bitmap params as defined @roam_control_trigger_reason
 *
 * This function passes the roam trigger bitmap to fw
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_set_roam_triggers(wmi_unified_t wmi_handle,
					 struct wlan_roam_triggers *triggers);

/**
 * wmi_unified_send_disconnect_roam_params() - Send disconnect roam trigger
 * parameters to firmware
 * @wmi_hdl:  wmi handle
 * @params: pointer to wlan_roam_disconnect_params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wmi_unified_send_disconnect_roam_params(wmi_unified_t wmi_handle,
				struct wlan_roam_disconnect_params *req);

/**
 * wmi_unified_send_idle_roam_params() - Send idle roam trigger params to fw
 * @wmi_hdl:  wmi handle
 * @params: pointer to wlan_roam_idle_params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wmi_unified_send_idle_roam_params(wmi_unified_t wmi_handle,
				  struct wlan_roam_idle_params *req);

/**
 * wmi_unified_send_roam_preauth_status() - Send roam preauthentication status
 * to target.
 * @wmi_handle: wmi handle
 * @param: Roam auth status params
 *
 * This function passes preauth status of WPA3 SAE auth to firmware. It is
 * called when external_auth_status event is received from userspace.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wmi_unified_send_roam_preauth_status(wmi_unified_t wmi_handle,
				     struct wmi_roam_auth_status_params *param);

/**
 * wmi_unified_vdev_set_pcl_cmd  - Send Vdev PCL command to fw
 * @wmi_handle: WMI handle
 * @params: Set VDEV pcl parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wmi_unified_vdev_set_pcl_cmd(wmi_unified_t wmi_handle,
					struct set_pcl_cmd_params *params);
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

/**
 * wmi_unified_roam_scan_offload_mode_cmd() - set roam scan parameters
 * @wmi_handle: wmi handle
 * @scan_cmd_fp: scan related parameters
 * @rso_cfg: roam scan offload parameters
 *
 * This function reads the incoming @rso_cfg and fill in the destination
 * WMI structure and send down the roam scan configs down to the firmware
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_roam_scan_offload_mode_cmd(
			wmi_unified_t wmi_handle,
			struct wlan_roam_scan_offload_params *rso_cfg);

/**
 * wmi_unified_send_roam_scan_offload_ap_cmd() - set roam ap profile in fw
 * @wmi_handle: wmi handle
 * @ap_profile: ap profile params
 *
 * Send WMI_ROAM_AP_PROFILE to firmware
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_send_roam_scan_offload_ap_cmd(
				wmi_unified_t wmi_handle,
				struct ap_profile_params *ap_profile);

/**
 * wmi_unified_roam_scan_offload_cmd() - set roam offload command
 * @wmi_handle: wmi handle
 * @command: command
 * @vdev_id: vdev id
 *
 * This function set roam offload command to fw.
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_roam_scan_offload_cmd(wmi_unified_t wmi_handle,
					     uint32_t command,
					     uint32_t vdev_id);

/**
 * wmi_unified_roam_scan_offload_chan_list_cmd  - Roam scan offload channel
 * list command
 * @wmi_handle: wmi handle
 * @rso_ch_info: roam scan offload channel info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wmi_unified_roam_scan_offload_chan_list_cmd(wmi_unified_t wmi_handle,
			struct wlan_roam_scan_channel_list *rso_ch_info);

/**
 * wmi_unified_roam_scan_offload_rssi_change_cmd() - set roam offload RSSI
 * threshold
 * @wmi_handle: wmi handle
 * @params: RSSI change params
 *
 * Send WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD parameters to fw.
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_unified_roam_scan_offload_rssi_change_cmd(
		wmi_unified_t wmi_handle,
		struct wlan_roam_rssi_change_params *params);

/**
 * wmi_unified_set_per_roam_config() - set PER roam config in FW
 * @wmi_handle: wmi handle
 * @req_buf: per roam config request buffer
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_unified_set_per_roam_config(wmi_unified_t wmi_handle,
				struct wlan_per_roam_config_req *req_buf);

/**
 * wmi_unified_send_limit_off_chan_cmd() - send wmi cmd of limit off channel
 * configuration params
 * @wmi_handle:  wmi handler
 * @wmi_param: pointer to wmi_limit_off_chan_param
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF failure reason code on failure
 */
QDF_STATUS wmi_unified_send_limit_off_chan_cmd(
		wmi_unified_t wmi_handle,
		struct wmi_limit_off_chan_param *wmi_param);

#ifdef WLAN_FEATURE_FILS_SK
/*
 * wmi_unified_roam_send_hlp_cmd() -send HLP command info
 * @wmi_handle: wma handle
 * @req_buf: Pointer to HLP params
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_roam_send_hlp_cmd(wmi_unified_t wmi_handle,
					 struct hlp_params *req_buf);
#endif /* WLAN_FEATURE_FILS_SK */

/**
 * wmi_unified_send_btm_config() - Send BTM config to fw
 * @wmi_handle:  wmi handle
 * @params: pointer to wlan_roam_btm_config
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wmi_unified_send_btm_config(wmi_unified_t wmi_handle,
				       struct wlan_roam_btm_config *params);

/**
 * wmi_unified_send_bss_load_config() - Send bss load trigger params to fw
 * @wmi_handle:  wmi handle
 * @params: pointer to wlan_roam_bss_load_config
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wmi_unified_send_bss_load_config(
				wmi_unified_t wmi_handle,
				struct wlan_roam_bss_load_config *params);

/**
 * wmi_unified_offload_11k_cmd() - send 11k offload command
 * @wmi_handle: wmi handle
 * @params: 11k offload params
 *
 * This function passes the 11k offload command params to FW
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_unified_offload_11k_cmd(wmi_unified_t wmi_handle,
			    struct wlan_roam_11k_offload_params *params);
/**
 * wmi_unified_invoke_neighbor_report_cmd() - send invoke neighbor report cmd
 * @wmi_handle: wmi handle
 * @params: invoke neighbor report params
 *
 * This function passes the invoke neighbor report command to fw
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_invoke_neighbor_report_cmd(
			wmi_unified_t wmi_handle,
			struct wmi_invoke_neighbor_report_params *params);

/**
 * wmi_unified_get_roam_scan_ch_list() - send roam scan channel list get cmd
 * @wmi_handle: wmi handle
 * @vdev_id: vdev id
 *
 * This function sends roam scan channel list get command to firmware.
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_unified_get_roam_scan_ch_list(wmi_unified_t wmi_handle,
					     uint8_t vdev_id);

#endif /* _WMI_UNIFIED_ROAM_API_H_ */
