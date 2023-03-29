/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
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
 * DOC: declare VDEV Manager interface APIs exposed by the mlme component
 */

#ifndef _WLAN_MLME_VDEV_MGR_INT_API_H_
#define _WLAN_MLME_VDEV_MGR_INT_API_H_

#include <wlan_objmgr_vdev_obj.h>
#include "include/wlan_vdev_mlme.h"
#include "wlan_mlme_main.h"
#include "wma_if.h"

/**
 * mlme_register_mlme_ext_ops() - Register mlme ext ops
 *
 * This function is called to register mlme ext operations
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_register_mlme_ext_ops(void);
/**
 * mlme_register_vdev_mgr_ops() - Register vdev mgr ops
 * @vdev_mlme: vdev mlme object
 *
 * This function is called to register vdev manager operations
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_register_vdev_mgr_ops(struct vdev_mlme_obj *vdev_mlme);
/**
 * mlme_unregister_vdev_mgr_ops() - Unregister vdev mgr ops
 * @vdev_mlme: vdev mlme object
 *
 * This function is called to unregister vdev manager operations
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_unregister_vdev_mgr_ops(struct vdev_mlme_obj *vdev_mlme);

/**
 * mlme_set_chan_switch_in_progress() - set mlme priv restart in progress
 * @vdev: vdev pointer
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_set_chan_switch_in_progress(struct wlan_objmgr_vdev *vdev,
					       bool val);

#ifdef WLAN_FEATURE_MSCS
/**
 * mlme_set_is_mscs_req_sent() - set mscs frame req flag
 * @vdev: vdev pointer
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_set_is_mscs_req_sent(struct wlan_objmgr_vdev *vdev, bool val);

/**
 * mlme_get_is_mscs_req_sent() - get mscs frame req flag
 * @vdev: vdev pointer
 *
 * Return: value of mscs flag
 */
bool mlme_get_is_mscs_req_sent(struct wlan_objmgr_vdev *vdev);
#else
static inline
QDF_STATUS mlme_set_is_mscs_req_sent(struct wlan_objmgr_vdev *vdev, bool val)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
bool mlme_get_is_mscs_req_sent(struct wlan_objmgr_vdev *vdev)
{
	return false;
}
#endif

/**
 * mlme_is_chan_switch_in_progress() - get mlme priv restart in progress
 * @vdev: vdev pointer
 *
 * Return: value of mlme priv restart in progress
 */
bool mlme_is_chan_switch_in_progress(struct wlan_objmgr_vdev *vdev);

/**
 * ap_mlme_set_hidden_ssid_restart_in_progress() - set mlme priv hidden ssid
 * restart in progress
 * @vdev: vdev pointer
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ap_mlme_set_hidden_ssid_restart_in_progress(struct wlan_objmgr_vdev *vdev,
					    bool val);

/**
 * ap_mlme_is_hidden_ssid_restart_in_progress() - get mlme priv hidden ssid
 * restart in progress
 * @vdev: vdev pointer
 *
 * Return: value of mlme priv hidden ssid restart in progress
 */
bool ap_mlme_is_hidden_ssid_restart_in_progress(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_vdev_start_failed() - set mlme priv vdev restart fail flag
 * @vdev: vdev pointer
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlme_set_vdev_start_failed(struct wlan_objmgr_vdev *vdev, bool val);

/**
 * mlme_is_connection_fail() - get connection fail flag
 * @vdev: vdev pointer
 *
 * Return: value of vdev connection failure flag
 */
bool mlme_is_connection_fail(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_is_wapi_sta_active() - check sta with wapi security exists and is active
 * @pdev: pdev pointer
 *
 * Return: true if sta with wapi security exists
 */
#ifdef FEATURE_WLAN_WAPI
bool mlme_is_wapi_sta_active(struct wlan_objmgr_pdev *pdev);
#else
static inline bool mlme_is_wapi_sta_active(struct wlan_objmgr_pdev *pdev)
{
	return false;
}
#endif

QDF_STATUS mlme_set_bigtk_support(struct wlan_objmgr_vdev *vdev, bool val);

bool mlme_get_bigtk_support(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_roam_reason_better_ap() - set roam reason better AP
 * @vdev: vdev pointer
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlme_set_roam_reason_better_ap(struct wlan_objmgr_vdev *vdev, bool val);

/**
 * mlme_get_roam_reason_better_ap() - get roam reason better AP
 * @vdev: vdev pointer
 *
 * Return: bool
 */
bool mlme_get_roam_reason_better_ap(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_hb_ap_rssi() - set hb ap RSSI
 * @vdev: vdev pointer
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_set_hb_ap_rssi(struct wlan_objmgr_vdev *vdev, uint32_t val);

/**
 * mlme_get_hb_ap_rssi() - get HB AP RSSIc
 * @vdev: vdev pointer
 *
 * Return: rssi value
 */
uint32_t mlme_get_hb_ap_rssi(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_connection_fail() - set connection failure flag
 * @vdev: vdev pointer
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlme_set_connection_fail(struct wlan_objmgr_vdev *vdev, bool val);

/**
 * mlme_get_vdev_start_failed() - get mlme priv vdev restart fail flag
 * @vdev: vdev pointer
 *
 * Return: value of mlme priv vdev restart fail flag
 */
bool mlme_get_vdev_start_failed(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_get_cac_required() - get if cac is required for new channel
 * @vdev: vdev pointer
 *
 * Return: if cac is required
 */
bool mlme_get_cac_required(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_cac_required() - set if cac is required for new channel
 * @vdev: vdev pointer
 * @val: value to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlme_set_cac_required(struct wlan_objmgr_vdev *vdev, bool val);

/**
 * mlme_set_mbssid_info() - save mbssid info
 * @vdev: vdev pointer
 * @mbssid_info: mbssid info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
mlme_set_mbssid_info(struct wlan_objmgr_vdev *vdev,
		     struct scan_mbssid_info *mbssid_info);

/**
 * mlme_get_mbssid_info() - get mbssid info
 * @vdev: vdev pointer
 * @mbss_11ax: mbss 11ax info
 *
 * Return: None
 */
void mlme_get_mbssid_info(struct wlan_objmgr_vdev *vdev,
			  struct vdev_mlme_mbss_11ax *mbss_11ax);

/**
 * mlme_set_tx_power() - set tx power
 * @vdev: vdev pointer
 * @tx_power: tx power to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_set_tx_power(struct wlan_objmgr_vdev *vdev,
			     int8_t tx_power);

/**
 * mlme_get_tx_power() - get tx power
 * @vdev: vdev pointer
 * @tx_power: tx power info
 *
 * Return: None
 */
int8_t mlme_get_tx_power(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_get_max_reg_power() - get max reg power
 * @vdev: vdev pointer
 *
 * Return: max reg power
 */
int8_t mlme_get_max_reg_power(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_set_max_reg_power() - set max reg power
 * @vdev: vdev pointer
 * @max_tx_power: max tx power to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_set_max_reg_power(struct wlan_objmgr_vdev *vdev,
				 int8_t max_reg_power);

/**
 * mlme_is_vdev_in_beaconning_mode() - check if vdev is beaconing mode
 * @vdev_opmode: vdev opmode
 *
 * To check if vdev is operating in beaconing mode or not.
 *
 * Return: true or false
 */
bool mlme_is_vdev_in_beaconning_mode(enum QDF_OPMODE vdev_opmode);

/**
 * mlme_set_assoc_type() - set associate type
 * @vdev: vdev pointer
 * @assoc_type: type to be set
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_set_assoc_type(struct wlan_objmgr_vdev *vdev,
			       enum vdev_assoc_type assoc_type);

/**
 * mlme_get_vdev_bss_peer_mac_addr() - to get peer mac address
 * @vdev: pointer to vdev
 * @bss_peer_mac_address: pointer to bss_peer_mac_address
 *
 * This API is used to get mac address of peer.
 *
 * Return: QDF_STATUS based on overall success
 */
QDF_STATUS mlme_get_vdev_bss_peer_mac_addr(
		struct wlan_objmgr_vdev *vdev,
		struct qdf_mac_addr *bss_peer_mac_address);

/**
 * mlme_get_vdev_stop_type() - to get vdev stop type
 * @vdev: vdev pointer
 * @vdev_stop_type: vdev stop type
 *
 * This API will get vdev stop type from mlme legacy priv.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_get_vdev_stop_type(struct wlan_objmgr_vdev *vdev,
				   uint32_t *vdev_stop_type);

/**
 * mlme_set_vdev_stop_type() - to set vdev stop type
 * @vdev: vdev pointer
 * @vdev_stop_type: vdev stop type
 *
 * This API will set vdev stop type from mlme legacy priv.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_set_vdev_stop_type(struct wlan_objmgr_vdev *vdev,
				   uint32_t vdev_stop_type);

/**
 * mlme_get_assoc_type() - get associate type
 * @vdev: vdev pointer
 *
 * Return: associate type
 */
enum vdev_assoc_type  mlme_get_assoc_type(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_vdev_create_send() - function to send the vdev create to firmware
 * @vdev: vdev pointer
 *
 * Return: QDF_STATUS_SUCCESS when the command has been successfully sent
 * to firmware or QDF_STATUS_E_** when there is a failure in sending the command
 * to firmware.
 */
QDF_STATUS mlme_vdev_create_send(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_vdev_self_peer_create() - function to send the vdev create self peer
 * @vdev: vdev pointer
 *
 * Return: QDF_STATUS_SUCCESS when the self peer is successfully created
 * to firmware or QDF_STATUS_E_** when there is a failure.
 */
QDF_STATUS mlme_vdev_self_peer_create(struct wlan_objmgr_vdev *vdev);

/**
 * mlme_vdev_self_peer_delete() - function to delete vdev self peer
 * @self_peer_del_msg: scheduler message containing the del_vdev_params
 *
 * Return: QDF_STATUS_SUCCESS when the self peer is successfully deleted
 * to firmware or QDF_STATUS_E_** when there is a failure.
 */
QDF_STATUS mlme_vdev_self_peer_delete(struct scheduler_msg *self_peer_del_msg);

/**
 * mlme_vdev_uses_self_peer() - send vdev del resp to Upper layer
 * @vdev_type: params of del vdev response
 *
 * Return: boolean
 */
bool mlme_vdev_uses_self_peer(uint32_t vdev_type, uint32_t vdev_subtype);

/**
 * mlme_vdev_self_peer_delete_resp() - send vdev self peer delete resp to Upper
 * layer
 * @param: params of del vdev response
 *
 * Return: none
 */
void mlme_vdev_self_peer_delete_resp(struct del_vdev_params *param);

/**
 * mlme_vdev_del_resp() - send vdev delete resp to Upper layer
 * @vdev_id: vdev id for which del vdev response is received
 *
 * Return: none
 */
void mlme_vdev_del_resp(uint8_t vdev_id);

/**
 * wlan_sap_disconnect_all_p2p_client() - send SAP disconnect all P2P
 *	client event to the SAP event handler
 * @vdev_id: vdev id of SAP
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_sap_disconnect_all_p2p_client(uint8_t vdev_id);

/**
 * wlan_sap_stop_bss() - send SAP stop bss event to the SAP event
 *	handler
 * @vdev_id: vdev id of SAP
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_sap_stop_bss(uint8_t vdev_id);

/**
 * wlan_get_conc_freq() - get concurrent operation frequency
 *
 * Return: concurrent frequency
 */
qdf_freq_t wlan_get_conc_freq(void);

#endif
