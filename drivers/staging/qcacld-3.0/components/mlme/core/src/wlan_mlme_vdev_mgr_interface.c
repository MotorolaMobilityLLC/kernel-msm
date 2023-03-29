/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
 * DOC: define internal APIs related to the mlme component, legacy APIs are
 *	called for the time being, but will be cleaned up after convergence
 */
#include "wlan_mlme_main.h"
#include "wlan_mlme_vdev_mgr_interface.h"
#include "lim_utils.h"
#include "wma_api.h"
#include "wma.h"
#include "lim_types.h"
#include <include/wlan_mlme_cmn.h>
#include <../../core/src/vdev_mgr_ops.h>
#include "wlan_psoc_mlme_api.h"
#include "target_if_cm_roam_offload.h"
#include "wlan_crypto_global_api.h"
#include "target_if_wfa_testcmd.h"
#include "csr_api.h"

static struct vdev_mlme_ops sta_mlme_ops;
static struct vdev_mlme_ops ap_mlme_ops;
static struct vdev_mlme_ops mon_mlme_ops;
static struct mlme_ext_ops ext_ops;

bool mlme_is_vdev_in_beaconning_mode(enum QDF_OPMODE vdev_opmode)
{
	switch (vdev_opmode) {
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_IBSS_MODE:
	case QDF_NDI_MODE:
		return true;
	default:
		return false;
	}
}

/**
 * mlme_get_global_ops() - Register ext global ops
 *
 * Return: ext_ops global ops
 */
static struct mlme_ext_ops *mlme_get_global_ops(void)
{
	return &ext_ops;
}

QDF_STATUS mlme_register_mlme_ext_ops(void)
{
	mlme_set_ops_register_cb(mlme_get_global_ops);
	return QDF_STATUS_SUCCESS;
}

/**
 * mlme_register_vdev_mgr_ops() - Register vdev mgr ops
 * @vdev_mlme: vdev mlme object
 *
 * This function is called to register vdev manager operations
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_register_vdev_mgr_ops(struct vdev_mlme_obj *vdev_mlme)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = vdev_mlme->vdev;

	if (mlme_is_vdev_in_beaconning_mode(vdev->vdev_mlme.vdev_opmode))
		vdev_mlme->ops = &ap_mlme_ops;
	else if (vdev->vdev_mlme.vdev_opmode == QDF_MONITOR_MODE)
		vdev_mlme->ops = &mon_mlme_ops;
	else
		vdev_mlme->ops = &sta_mlme_ops;

	return QDF_STATUS_SUCCESS;
}

/**
 * mlme_unregister_vdev_mgr_ops() - Unregister vdev mgr ops
 * @vdev_mlme: vdev mlme object
 *
 * This function is called to unregister vdev manager operations
 *
 * Return: QDF_STATUS
 */
QDF_STATUS mlme_unregister_vdev_mgr_ops(struct vdev_mlme_obj *vdev_mlme)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * sta_mlme_vdev_start_send() - MLME vdev start callback
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to initiate actions of VDEV.start
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sta_mlme_vdev_start_send(struct vdev_mlme_obj *vdev_mlme,
					   uint16_t event_data_len,
					   void *event_data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_sta_mlme_vdev_start_send(vdev_mlme, event_data_len,
					    event_data);
}

/**
 * sta_mlme_start_continue() - vdev start rsp calback
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to handle the VDEV START/RESTART calback
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sta_mlme_start_continue(struct vdev_mlme_obj *vdev_mlme,
					  uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_sta_mlme_vdev_start_continue(vdev_mlme, data_len, data);
}

/**
 * sta_mlme_vdev_restart_send() - MLME vdev restart send
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to initiate actions of VDEV.start
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sta_mlme_vdev_restart_send(struct vdev_mlme_obj *vdev_mlme,
					     uint16_t event_data_len,
					     void *event_data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_sta_mlme_vdev_restart_send(vdev_mlme, event_data_len,
					    event_data);
}

/**
 * sta_mlme_vdev_start_req_failed() - MLME start fail callback
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to send the vdev stop to firmware
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sta_mlme_vdev_start_req_failed(struct vdev_mlme_obj *vdev_mlme,
						 uint16_t data_len,
						 void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_sta_mlme_vdev_req_fail(vdev_mlme, data_len, data);
}

/**
 * sta_mlme_vdev_start_connection() - MLME vdev start callback
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to initiate actions of STA connection
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sta_mlme_vdev_start_connection(struct vdev_mlme_obj *vdev_mlme,
						 uint16_t event_data_len,
						 void *event_data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return QDF_STATUS_SUCCESS;
}

/**
 * sta_mlme_vdev_up_send() - MLME vdev UP callback
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to send the vdev up command
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sta_mlme_vdev_up_send(struct vdev_mlme_obj *vdev_mlme,
					uint16_t event_data_len,
					void *event_data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_sta_vdev_up_send(vdev_mlme, event_data_len, event_data);
}

/**
 * sta_mlme_vdev_notify_up_complete() - MLME vdev UP complete callback
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to VDEV MLME on moving
 *  to UP state
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sta_mlme_vdev_notify_up_complete(struct vdev_mlme_obj *vdev_mlme,
						   uint16_t event_data_len,
						   void *event_data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return QDF_STATUS_SUCCESS;
}

/**
 * sta_mlme_vdev_notify_roam_start() - MLME vdev Roam start callback
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to VDEV MLME on roaming
 *  to UP state
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS sta_mlme_vdev_notify_roam_start(struct vdev_mlme_obj *vdev_mlme,
					   uint16_t event_data_len,
					   void *event_data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_sta_mlme_vdev_roam_notify(vdev_mlme, event_data_len,
					     event_data);
}

/**
 * sta_mlme_vdev_disconnect_bss() - MLME vdev disconnect bss callback
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to disconnect BSS/send deauth to AP
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sta_mlme_vdev_disconnect_bss(struct vdev_mlme_obj *vdev_mlme,
					       uint16_t event_data_len,
					       void *event_data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_sta_mlme_vdev_disconnect_bss(vdev_mlme, event_data_len,
						event_data);
}

/**
 * sta_mlme_vdev_stop_send() - MLME vdev stop send callback
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to send the vdev stop to firmware
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sta_mlme_vdev_stop_send(struct vdev_mlme_obj *vdev_mlme,
					  uint16_t data_len,
					  void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_sta_mlme_vdev_stop_send(vdev_mlme, data_len, data);
}

/**
 * vdevmgr_mlme_stop_continue() - MLME vdev stop send callback
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to initiate operations on
 * LMAC/FW stop response such as remove peer.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS vdevmgr_mlme_stop_continue(struct vdev_mlme_obj *vdev_mlme,
					     uint16_t data_len,
					     void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_mlme_vdev_stop_continue(vdev_mlme, data_len, data);
}

/**
 * ap_mlme_vdev_start_send () - send vdev start req
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to initiate actions of VDEV start ie start bss
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_mlme_vdev_start_send(struct vdev_mlme_obj *vdev_mlme,
					  uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_ap_mlme_vdev_start_send(vdev_mlme, data_len, data);
}

/**
 * ap_start_continue () - vdev start rsp calback
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to handle the VDEV START/RESTART calback
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_mlme_start_continue(struct vdev_mlme_obj *vdev_mlme,
					 uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_ap_mlme_vdev_start_continue(vdev_mlme, data_len, data);
}

/**
 * ap_mlme_vdev_update_beacon() - callback to initiate beacon update
 * @vdev_mlme: vdev mlme object
 * @op: beacon operation
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to update beacon
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_mlme_vdev_update_beacon(struct vdev_mlme_obj *vdev_mlme,
					     enum beacon_update_op op,
					     uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_ap_mlme_vdev_update_beacon(vdev_mlme, op, data_len, data);
}

/**
 * ap_mlme_vdev_up_send() - callback to send vdev up
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to send vdev up req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_mlme_vdev_up_send(struct vdev_mlme_obj *vdev_mlme,
				       uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_ap_mlme_vdev_up_send(vdev_mlme, data_len, data);
}

/**
 * ap_mlme_vdev_notify_up_complete() - callback to notify up completion
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to indicate up is completed
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
ap_mlme_vdev_notify_up_complete(struct vdev_mlme_obj *vdev_mlme,
				uint16_t data_len, void *data)
{
	if (!vdev_mlme) {
		mlme_legacy_err("data is NULL");
		return QDF_STATUS_E_INVAL;
	}

	pe_debug("Vdev %d is up", wlan_vdev_get_id(vdev_mlme->vdev));

	return QDF_STATUS_SUCCESS;
}

/**
 * ap_mlme_vdev_disconnect_peers() - callback to disconnect all connected peers
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to disconnect all connected peers
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_mlme_vdev_disconnect_peers(struct vdev_mlme_obj *vdev_mlme,
						uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_ap_mlme_vdev_disconnect_peers(vdev_mlme, data_len, data);
}

/**
 * ap_mlme_vdev_stop_send() - callback to send stop vdev request
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to send stop vdev request
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_mlme_vdev_stop_send(struct vdev_mlme_obj *vdev_mlme,
					 uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_ap_mlme_vdev_stop_send(vdev_mlme, data_len, data);
}

/**
 * ap_mlme_vdev_is_newchan_no_cac - VDEV SM CSA complete notification
 * @vdev_mlme:  VDEV MLME comp object
 *
 * On CSA complete, checks whether Channel does not needs CAC period, if
 * it doesn't need cac return SUCCESS else FAILURE
 *
 * Return: SUCCESS if new channel doesn't need cac
 *         else FAILURE
 */
static QDF_STATUS
ap_mlme_vdev_is_newchan_no_cac(struct vdev_mlme_obj *vdev_mlme)
{
	bool cac_required;

	cac_required = mlme_get_cac_required(vdev_mlme->vdev);
	mlme_legacy_debug("vdev id = %d cac_required %d",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id, cac_required);

	if (!cac_required)
		return QDF_STATUS_SUCCESS;

	mlme_set_cac_required(vdev_mlme->vdev, false);

	return QDF_STATUS_E_FAILURE;
}

/**
 * ap_mlme_vdev_down_send() - callback to send vdev down req
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to send vdev down req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS vdevmgr_mlme_vdev_down_send(struct vdev_mlme_obj *vdev_mlme,
					      uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_ap_mlme_vdev_down_send(vdev_mlme, data_len, data);
}

/**
 * vdevmgr_notify_down_complete() - callback to indicate vdev down is completed
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to indicate vdev down is completed
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS vdevmgr_notify_down_complete(struct vdev_mlme_obj *vdev_mlme,
					       uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_mlme_vdev_notify_down_complete(vdev_mlme, data_len, data);
}

/**
 * ap_mlme_vdev_start_req_failed () - vdev start req fail callback
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to handle vdev start req/rsp failure
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_mlme_vdev_start_req_failed(struct vdev_mlme_obj *vdev_mlme,
						uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_ap_mlme_vdev_start_req_failed(vdev_mlme, data_len, data);
}

/**
 * ap_mlme_vdev_restart_send() a callback to send vdev restart
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to initiate and send vdev restart req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_mlme_vdev_restart_send(struct vdev_mlme_obj *vdev_mlme,
					    uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_ap_mlme_vdev_restart_send(vdev_mlme, data_len, data);
}

/**
 * ap_mlme_vdev_stop_start_send() - handle vdev stop during start req
 * @vdev_mlme: vdev mlme object
 * @type: restart req or start req
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to handle vdev stop during start req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_mlme_vdev_stop_start_send(struct vdev_mlme_obj *vdev_mlme,
					       enum vdev_cmd_type type,
					       uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_ap_mlme_vdev_stop_start_send(vdev_mlme, type,
						data_len, data);
}

QDF_STATUS mlme_set_chan_switch_in_progress(struct wlan_objmgr_vdev *vdev,
					       bool val)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->chan_switch_in_progress = val;

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_MSCS
QDF_STATUS mlme_set_is_mscs_req_sent(struct wlan_objmgr_vdev *vdev, bool val)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->mscs_req_info.is_mscs_req_sent = val;

	return QDF_STATUS_SUCCESS;
}

bool mlme_get_is_mscs_req_sent(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->mscs_req_info.is_mscs_req_sent;
}
#endif

bool mlme_is_chan_switch_in_progress(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->chan_switch_in_progress;
}

QDF_STATUS
ap_mlme_set_hidden_ssid_restart_in_progress(struct wlan_objmgr_vdev *vdev,
					    bool val)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->hidden_ssid_restart_in_progress = val;

	return QDF_STATUS_SUCCESS;
}

bool ap_mlme_is_hidden_ssid_restart_in_progress(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->hidden_ssid_restart_in_progress;
}

QDF_STATUS mlme_set_bigtk_support(struct wlan_objmgr_vdev *vdev, bool val)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->bigtk_vdev_support = val;

	return QDF_STATUS_SUCCESS;
}

bool mlme_get_bigtk_support(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->bigtk_vdev_support;
}

QDF_STATUS
mlme_set_roam_reason_better_ap(struct wlan_objmgr_vdev *vdev, bool val)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->roam_reason_better_ap = val;

	return QDF_STATUS_SUCCESS;
}

bool mlme_get_roam_reason_better_ap(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->roam_reason_better_ap;
}

QDF_STATUS
mlme_set_hb_ap_rssi(struct wlan_objmgr_vdev *vdev, uint32_t val)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->hb_failure_rssi = val;

	return QDF_STATUS_SUCCESS;
}

uint32_t mlme_get_hb_ap_rssi(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return 0;
	}

	return mlme_priv->hb_failure_rssi;
}


QDF_STATUS mlme_set_connection_fail(struct wlan_objmgr_vdev *vdev, bool val)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->connection_fail = val;

	return QDF_STATUS_SUCCESS;
}

bool mlme_is_connection_fail(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->connection_fail;
}

#ifdef FEATURE_WLAN_WAPI
static void mlme_is_sta_vdev_wapi(struct wlan_objmgr_pdev *pdev,
			   void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	int32_t keymgmt;
	bool *is_wapi_sta_exist = (bool *)arg;
	QDF_STATUS status;

	if (*is_wapi_sta_exist)
		return;
	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		return;

	status = wlan_vdev_is_up(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	keymgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (keymgmt < 0)
		return;

	if (keymgmt & ((1 << WLAN_CRYPTO_KEY_MGMT_WAPI_PSK) |
		       (1 << WLAN_CRYPTO_KEY_MGMT_WAPI_CERT))) {
		*is_wapi_sta_exist = true;
		mlme_debug("wapi exist for Vdev: %d",
			   wlan_vdev_get_id(vdev));
	}
}

bool mlme_is_wapi_sta_active(struct wlan_objmgr_pdev *pdev)
{
	bool is_wapi_sta_exist = false;

	wlan_objmgr_pdev_iterate_obj_list(pdev,
					  WLAN_VDEV_OP,
					  mlme_is_sta_vdev_wapi,
					  &is_wapi_sta_exist, 0,
					  WLAN_MLME_OBJMGR_ID);

	return is_wapi_sta_exist;
}
#endif

QDF_STATUS mlme_set_assoc_type(struct wlan_objmgr_vdev *vdev,
			       enum vdev_assoc_type assoc_type)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->assoc_type = assoc_type;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_get_vdev_bss_peer_mac_addr(
				struct wlan_objmgr_vdev *vdev,
				struct qdf_mac_addr *bss_peer_mac_address)
{
	struct wlan_objmgr_peer *peer;

	if (!vdev) {
		mlme_legacy_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_MLME_OBJMGR_ID);
	if (!peer) {
		mlme_legacy_err("peer is null");
		return QDF_STATUS_E_INVAL;
	}
	wlan_peer_obj_lock(peer);
	qdf_mem_copy(bss_peer_mac_address->bytes, wlan_peer_get_macaddr(peer),
		     QDF_MAC_ADDR_SIZE);
	wlan_peer_obj_unlock(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_OBJMGR_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_get_vdev_stop_type(struct wlan_objmgr_vdev *vdev,
				   uint32_t *vdev_stop_type)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	*vdev_stop_type = mlme_priv->vdev_stop_type;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_set_vdev_stop_type(struct wlan_objmgr_vdev *vdev,
				   uint32_t vdev_stop_type)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->vdev_stop_type = vdev_stop_type;

	return QDF_STATUS_SUCCESS;
}

enum vdev_assoc_type  mlme_get_assoc_type(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->assoc_type;
}

QDF_STATUS
mlme_set_vdev_start_failed(struct wlan_objmgr_vdev *vdev, bool val)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->vdev_start_failed = val;

	return QDF_STATUS_SUCCESS;
}

bool mlme_get_vdev_start_failed(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->vdev_start_failed;
}

QDF_STATUS mlme_set_cac_required(struct wlan_objmgr_vdev *vdev, bool val)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->cac_required_for_new_channel = val;

	return QDF_STATUS_SUCCESS;
}

bool mlme_get_cac_required(struct wlan_objmgr_vdev *vdev)
{
	struct mlme_legacy_priv *mlme_priv;

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_legacy_err("vdev legacy private object is NULL");
		return false;
	}

	return mlme_priv->cac_required_for_new_channel;
}

QDF_STATUS mlme_set_mbssid_info(struct wlan_objmgr_vdev *vdev,
				struct scan_mbssid_info *mbssid_info)
{
	struct vdev_mlme_obj *vdev_mlme;
	struct vdev_mlme_mbss_11ax *mbss_11ax;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_legacy_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mbss_11ax = &vdev_mlme->mgmt.mbss_11ax;
	mbss_11ax->profile_idx = mbssid_info->profile_num;
	mbss_11ax->profile_num = mbssid_info->profile_count;
	qdf_mem_copy(mbss_11ax->trans_bssid,
		     mbssid_info->trans_bssid, QDF_MAC_ADDR_SIZE);

	return QDF_STATUS_SUCCESS;
}

void mlme_get_mbssid_info(struct wlan_objmgr_vdev *vdev,
			  struct vdev_mlme_mbss_11ax *mbss_11ax)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_legacy_err("vdev component object is NULL");
		return;
	}

	mbss_11ax = &vdev_mlme->mgmt.mbss_11ax;
}

QDF_STATUS mlme_set_tx_power(struct wlan_objmgr_vdev *vdev,
			     int8_t tx_power)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);

	if (!vdev_mlme) {
		mlme_legacy_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_mlme->mgmt.generic.tx_power = tx_power;

	return QDF_STATUS_SUCCESS;
}

int8_t mlme_get_tx_power(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_legacy_err("vdev component object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	return vdev_mlme->mgmt.generic.tx_power;
}

QDF_STATUS mlme_set_max_reg_power(struct wlan_objmgr_vdev *vdev,
				 int8_t max_reg_power)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);

	if (!vdev_mlme) {
		mlme_legacy_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_mlme->mgmt.generic.maxregpower = max_reg_power;

	return QDF_STATUS_SUCCESS;
}

int8_t mlme_get_max_reg_power(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_legacy_err("vdev component object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	return vdev_mlme->mgmt.generic.maxregpower;
}

/**
 * mlme_get_vdev_types() - get vdev type and subtype from its operation mode
 * @mode: operation mode of vdev
 * @type: type of vdev
 * @sub_type: sub_type of vdev
 *
 * This API is called to get vdev type and subtype from its operation mode.
 * Vdev operation modes are defined in enum QDF_OPMODE.
 *
 * Type of vdev are WLAN_VDEV_MLME_TYPE_AP, WLAN_VDEV_MLME_TYPE_STA,
 * WLAN_VDEV_MLME_TYPE_IBSS, ,WLAN_VDEV_MLME_TYPE_MONITOR,
 * WLAN_VDEV_MLME_TYPE_NAN, WLAN_VDEV_MLME_TYPE_OCB, WLAN_VDEV_MLME_TYPE_NDI
 *
 * Sub_types of vdev are WLAN_VDEV_MLME_SUBTYPE_P2P_DEVICE,
 * WLAN_VDEV_MLME_SUBTYPE_P2P_CLIENT, WLAN_VDEV_MLME_SUBTYPE_P2P_GO,
 * WLAN_VDEV_MLME_SUBTYPE_PROXY_STA, WLAN_VDEV_MLME_SUBTYPE_MESH
 * Return: QDF_STATUS
 */

static QDF_STATUS mlme_get_vdev_types(enum QDF_OPMODE mode, uint8_t *type,
				      uint8_t *sub_type)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	*type = 0;
	*sub_type = 0;

	switch (mode) {
	case QDF_STA_MODE:
		*type = WLAN_VDEV_MLME_TYPE_STA;
		break;
	case QDF_SAP_MODE:
		*type = WLAN_VDEV_MLME_TYPE_AP;
		break;
	case QDF_P2P_DEVICE_MODE:
		*type = WLAN_VDEV_MLME_TYPE_AP;
		*sub_type = WLAN_VDEV_MLME_SUBTYPE_P2P_DEVICE;
		break;
	case QDF_P2P_CLIENT_MODE:
		*type = WLAN_VDEV_MLME_TYPE_STA;
		*sub_type = WLAN_VDEV_MLME_SUBTYPE_P2P_CLIENT;
		break;
	case QDF_P2P_GO_MODE:
		*type = WLAN_VDEV_MLME_TYPE_AP;
		*sub_type = WLAN_VDEV_MLME_SUBTYPE_P2P_GO;
		break;
	case QDF_OCB_MODE:
		*type = WLAN_VDEV_MLME_TYPE_OCB;
		break;
	case QDF_IBSS_MODE:
		*type = WLAN_VDEV_MLME_TYPE_IBSS;
		break;
	case QDF_MONITOR_MODE:
		*type = WMI_HOST_VDEV_TYPE_MONITOR;
		break;
	case QDF_NDI_MODE:
		*type = WLAN_VDEV_MLME_TYPE_NDI;
		break;
	case QDF_NAN_DISC_MODE:
		*type = WLAN_VDEV_MLME_TYPE_NAN;
		break;
	default:
		mlme_err("Invalid device mode %d", mode);
		status = QDF_STATUS_E_INVAL;
		break;
	}
	return status;
}

/**
 * vdevmgr_mlme_ext_hdl_create () - Create mlme legacy priv object
 * @vdev_mlme: vdev mlme object
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS vdevmgr_mlme_ext_hdl_create(struct vdev_mlme_obj *vdev_mlme)
{
	QDF_STATUS status;

	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	vdev_mlme->ext_vdev_ptr =
		qdf_mem_malloc(sizeof(struct mlme_legacy_priv));
	if (!vdev_mlme->ext_vdev_ptr)
		return QDF_STATUS_E_NOMEM;

	mlme_init_rate_config(vdev_mlme);
	vdev_mlme->ext_vdev_ptr->fils_con_info = NULL;

	sme_get_vdev_type_nss(wlan_vdev_mlme_get_opmode(vdev_mlme->vdev),
			      &vdev_mlme->proto.generic.nss_2g,
			      &vdev_mlme->proto.generic.nss_5g);

	status = mlme_get_vdev_types(wlan_vdev_mlme_get_opmode(vdev_mlme->vdev),
				     &vdev_mlme->mgmt.generic.type,
				     &vdev_mlme->mgmt.generic.subtype);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Get vdev type failed; status:%d", status);
		qdf_mem_free(vdev_mlme->ext_vdev_ptr);
		return status;
	}

	status = vdev_mgr_create_send(vdev_mlme);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Failed to create vdev for vdev id %d",
			 wlan_vdev_get_id(vdev_mlme->vdev));
		qdf_mem_free(vdev_mlme->ext_vdev_ptr);
		return status;
	}

	return status;
}

/**
 * vdevmgr_mlme_ext_hdl_destroy () - Destroy mlme legacy priv object
 * @vdev_mlme: vdev mlme object
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS vdevmgr_mlme_ext_hdl_destroy(struct vdev_mlme_obj *vdev_mlme)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct vdev_delete_response rsp;
	uint8_t vdev_id;

	vdev_id = vdev_mlme->vdev->vdev_objmgr.vdev_id;
	mlme_legacy_debug("Sending vdev delete to firmware for vdev id = %d ",
			  vdev_id);

	if (!vdev_mlme->ext_vdev_ptr)
		return status;

	status = vdev_mgr_delete_send(vdev_mlme);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Failed to send vdev delete to firmware");
			 rsp.vdev_id = vdev_id;
		wma_vdev_detach_callback(&rsp);
	}

	mlme_free_self_disconnect_ies(vdev_mlme->vdev);
	mlme_free_peer_disconnect_ies(vdev_mlme->vdev);
	mlme_free_sae_auth_retry(vdev_mlme->vdev);

	qdf_mem_free(vdev_mlme->ext_vdev_ptr->fils_con_info);
	vdev_mlme->ext_vdev_ptr->fils_con_info = NULL;

	qdf_mem_free(vdev_mlme->ext_vdev_ptr);
	vdev_mlme->ext_vdev_ptr = NULL;

	return QDF_STATUS_SUCCESS;
}

/**
 * ap_vdev_dfs_cac_timer_stop() – callback to stop cac timer
 * @vdev_mlme: vdev mlme object
 * @event_data_len: event data length
 * @event_data: event data
 *
 * This function is called to stop cac timer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_vdev_dfs_cac_timer_stop(struct vdev_mlme_obj *vdev_mlme,
					     uint16_t event_data_len,
					     void *event_data)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return QDF_STATUS_SUCCESS;
}

/**
 * mon_mlme_vdev_start_restart_send () - send vdev start/restart req
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to initiate actions of VDEV start/restart
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS mon_mlme_vdev_start_restart_send(
	struct vdev_mlme_obj *vdev_mlme,
	uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return lim_mon_mlme_vdev_start_send(vdev_mlme, data_len, data);
}

/**
 * mon_start_continue () - vdev start rsp calback
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to handle the VDEV START/RESTART calback
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS mon_mlme_start_continue(struct vdev_mlme_obj *vdev_mlme,
					  uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_mon_mlme_vdev_start_continue(vdev_mlme, data_len, data);
}

/**
 * mon_mlme_vdev_up_send() - callback to send vdev up
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to send vdev up req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS mon_mlme_vdev_up_send(struct vdev_mlme_obj *vdev_mlme,
					uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_mon_mlme_vdev_up_send(vdev_mlme, data_len, data);
}

/**
 * mon_mlme_vdev_disconnect_peers() - callback to disconnect all connected peers
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * montior mode no connected peers, only do VDEV state transition.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS mon_mlme_vdev_disconnect_peers(
		struct vdev_mlme_obj *vdev_mlme,
		uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wlan_vdev_mlme_sm_deliver_evt(
				vdev_mlme->vdev,
				WLAN_VDEV_SM_EV_DISCONNECT_COMPLETE,
				0, NULL);
}

/**
 * mon_mlme_vdev_stop_send() - callback to send stop vdev request
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to send stop vdev request
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS mon_mlme_vdev_stop_send(struct vdev_mlme_obj *vdev_mlme,
					  uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_mon_mlme_vdev_stop_send(vdev_mlme, data_len, data);
}

/**
 * mon_mlme_vdev_down_send() - callback to send vdev down req
 * @vdev_mlme: vdev mlme object
 * @data_len: event data length
 * @data: event data
 *
 * This function is called to send vdev down req
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS mon_mlme_vdev_down_send(struct vdev_mlme_obj *vdev_mlme,
					  uint16_t data_len, void *data)
{
	mlme_legacy_debug("vdev id = %d",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_mon_mlme_vdev_down_send(vdev_mlme, data_len, data);
}

/**
 * vdevmgr_vdev_delete_rsp_handle() - callback to handle vdev delete response
 * @vdev_mlme: vdev mlme object
 * @rsp: pointer to vdev delete response
 *
 * This function is called to handle vdev delete response and send result to
 * upper layer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
vdevmgr_vdev_delete_rsp_handle(struct wlan_objmgr_psoc *psoc,
			       struct vdev_delete_response *rsp)
{
	mlme_legacy_debug("vdev id = %d ", rsp->vdev_id);
	return wma_vdev_detach_callback(rsp);
}

/**
 * vdevmgr_vdev_stop_rsp_handle() - callback to handle vdev stop response
 * @vdev_mlme: vdev mlme object
 * @rsp: pointer to vdev stop response
 *
 * This function is called to handle vdev stop response and send result to
 * upper layer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
vdevmgr_vdev_stop_rsp_handle(struct vdev_mlme_obj *vdev_mlme,
			     struct vdev_stop_response *rsp)
{
	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	return wma_vdev_stop_resp_handler(vdev_mlme, rsp);
}

/**
 * psoc_mlme_ext_hdl_create() - Create mlme legacy priv object
 * @psoc_mlme: psoc mlme object
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS psoc_mlme_ext_hdl_create(struct psoc_mlme_obj *psoc_mlme)
{
	psoc_mlme->ext_psoc_ptr =
		qdf_mem_malloc(sizeof(struct wlan_mlme_psoc_ext_obj));
	if (!psoc_mlme->ext_psoc_ptr)
		return QDF_STATUS_E_NOMEM;

	target_if_cm_roam_register_tx_ops(
			&psoc_mlme->ext_psoc_ptr->rso_tx_ops);

	target_if_wfatestcmd_register_tx_ops(
			&psoc_mlme->ext_psoc_ptr->wfa_testcmd.tx_ops);

	return QDF_STATUS_SUCCESS;
}

/**
 * psoc_mlme_ext_hdl_destroy() - Destroy mlme legacy priv object
 * @psoc_mlme: psoc mlme object
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS psoc_mlme_ext_hdl_destroy(struct psoc_mlme_obj *psoc_mlme)
{
	if (!psoc_mlme) {
		mlme_err("PSOC MLME is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (psoc_mlme->ext_psoc_ptr) {
		qdf_mem_free(psoc_mlme->ext_psoc_ptr);
		psoc_mlme->ext_psoc_ptr = NULL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * vdevmgr_vdev_delete_rsp_handle() - callback to handle vdev delete response
 * @vdev_mlme: vdev mlme object
 * @rsp: pointer to vdev delete response
 *
 * This function is called to handle vdev delete response and send result to
 * upper layer
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
vdevmgr_vdev_start_rsp_handle(struct vdev_mlme_obj *vdev_mlme,
			      struct vdev_start_response *rsp)
{
	QDF_STATUS status;

	mlme_legacy_debug("vdev id = %d ",
			  vdev_mlme->vdev->vdev_objmgr.vdev_id);
	status =  wma_vdev_start_resp_handler(vdev_mlme, rsp);

	return status;
}

static QDF_STATUS
vdevmgr_vdev_peer_delete_all_rsp_handle(struct vdev_mlme_obj *vdev_mlme,
					struct peer_delete_all_response *rsp)
{
	QDF_STATUS status;

	status = lim_process_mlm_del_all_sta_rsp(vdev_mlme, rsp);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("Failed to call lim_process_mlm_del_all_sta_rsp");
	return status;
}

QDF_STATUS mlme_vdev_self_peer_create(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		mlme_err("Failed to get vdev mlme obj for vdev id %d",
			 wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_INVAL;
	}

	return wma_vdev_self_peer_create(vdev_mlme);
}

static
QDF_STATUS vdevmgr_mlme_ext_post_hdl_create(struct vdev_mlme_obj *vdev_mlme)
{
	return QDF_STATUS_SUCCESS;
}

bool mlme_vdev_uses_self_peer(uint32_t vdev_type, uint32_t vdev_subtype)
{
	switch (vdev_type) {
	case WMI_VDEV_TYPE_AP:
		return vdev_subtype == WMI_UNIFIED_VDEV_SUBTYPE_P2P_DEVICE;

	case WMI_VDEV_TYPE_MONITOR:
	case WMI_VDEV_TYPE_OCB:
		return true;

	default:
		return false;
	}
}

void mlme_vdev_del_resp(uint8_t vdev_id)
{
	sme_vdev_del_resp(vdev_id);
}

static
QDF_STATUS mlme_vdev_self_peer_delete_resp_flush_cb(struct scheduler_msg *msg)
{
	/*
	 * sme should be the last component to hold the reference invoke the
	 * same to release the reference gracefully
	 */
	sme_vdev_self_peer_delete_resp(msg->bodyptr);
	return QDF_STATUS_SUCCESS;
}

void mlme_vdev_self_peer_delete_resp(struct del_vdev_params *param)
{
	struct scheduler_msg peer_del_rsp = {0};
	QDF_STATUS status;

	peer_del_rsp.type = eWNI_SME_VDEV_DELETE_RSP;
	peer_del_rsp.bodyptr = param;
	peer_del_rsp.flush_callback = mlme_vdev_self_peer_delete_resp_flush_cb;

	status = scheduler_post_message(QDF_MODULE_ID_MLME,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &peer_del_rsp);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		/* In the error cases release the final sme referene */
		wlan_objmgr_vdev_release_ref(param->vdev, WLAN_LEGACY_SME_ID);
		qdf_mem_free(param);
	}
}

QDF_STATUS mlme_vdev_self_peer_delete(struct scheduler_msg *self_peer_del_msg)
{
	QDF_STATUS status;
	struct del_vdev_params *del_vdev = self_peer_del_msg->bodyptr;

	if (!del_vdev) {
		mlme_err("Invalid del self peer params");
		return QDF_STATUS_E_INVAL;
	}

	status = wma_vdev_detach(del_vdev);
	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("Failed to detach vdev");

	return status;
}

QDF_STATUS wlan_sap_disconnect_all_p2p_client(uint8_t vdev_id)
{
	return csr_mlme_vdev_disconnect_all_p2p_client_event(vdev_id);
}

QDF_STATUS wlan_sap_stop_bss(uint8_t vdev_id)
{
	return csr_mlme_vdev_stop_bss(vdev_id);
}

qdf_freq_t wlan_get_conc_freq(void)
{
	return csr_mlme_get_concurrent_operation_freq();
}

/**
 * ap_mlme_vdev_csa_complete() - callback to initiate csa complete
 *
 * @vdev_mlme: vdev mlme object
 *
 * This function is called for csa complete indication
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS ap_mlme_vdev_csa_complete(struct vdev_mlme_obj *vdev_mlme)

{
	uint8_t vdev_id;

	vdev_id = wlan_vdev_get_id(vdev_mlme->vdev);
	mlme_legacy_debug("vdev id = %d ", vdev_id);

	if (lim_is_csa_tx_pending(vdev_id))
		lim_send_csa_tx_complete(vdev_id);
	else
		mlme_legacy_debug("CSAIE_TX_COMPLETE_IND already sent");

	return QDF_STATUS_SUCCESS;
}

/**
 * struct sta_mlme_ops - VDEV MLME operation callbacks strucutre for sta
 * @mlme_vdev_start_send:               callback to initiate actions of VDEV
 *                                      MLME start operation
 * @mlme_vdev_restart_send:             callback to initiate actions of VDEV
 *                                      MLME restart operation
 * @mlme_vdev_stop_start_send:          callback to block start/restart VDEV
 *                                      request command
 * @mlme_vdev_sta_conn_start:           callback to initiate connection
 * @mlme_vdev_start_continue:           callback to initiate operations on
 *                                      LMAC/FW start response
 * @mlme_vdev_up_send:                  callback to initiate actions of VDEV
 *                                      MLME up operation
 * @mlme_vdev_notify_up_complete:       callback to notify VDEV MLME on moving
 *                                      to UP state
 * @mlme_vdev_update_beacon:            callback to initiate beacon update
 * @mlme_vdev_disconnect_peers:         callback to initiate disconnection of
 *                                      peers
 * @mlme_vdev_stop_send:                callback to initiate actions of VDEV
 *                                      MLME stop operation
 * @mlme_vdev_stop_continue:            callback to initiate operations on
 *                                      LMAC/FW stop response
 * @mlme_vdev_down_send:                callback to initiate actions of VDEV
 *                                      MLME down operation
 * @mlme_vdev_notify_down_complete:     callback to notify VDEV MLME on moving
 *                                      to INIT state
 */
static struct vdev_mlme_ops sta_mlme_ops = {
	.mlme_vdev_start_send = sta_mlme_vdev_start_send,
	.mlme_vdev_restart_send = sta_mlme_vdev_restart_send,
	.mlme_vdev_start_continue = sta_mlme_start_continue,
	.mlme_vdev_start_req_failed = sta_mlme_vdev_start_req_failed,
	.mlme_vdev_sta_conn_start = sta_mlme_vdev_start_connection,
	.mlme_vdev_up_send = sta_mlme_vdev_up_send,
	.mlme_vdev_notify_up_complete = sta_mlme_vdev_notify_up_complete,
	.mlme_vdev_notify_roam_start = sta_mlme_vdev_notify_roam_start,
	.mlme_vdev_disconnect_peers = sta_mlme_vdev_disconnect_bss,
	.mlme_vdev_stop_send = sta_mlme_vdev_stop_send,
	.mlme_vdev_stop_continue = vdevmgr_mlme_stop_continue,
	.mlme_vdev_down_send = vdevmgr_mlme_vdev_down_send,
	.mlme_vdev_notify_down_complete = vdevmgr_notify_down_complete,
	.mlme_vdev_ext_stop_rsp = vdevmgr_vdev_stop_rsp_handle,
	.mlme_vdev_ext_start_rsp = vdevmgr_vdev_start_rsp_handle,
};

/**
 * struct ap_mlme_ops - VDEV MLME operation callbacks strucutre for beaconing
 *                      interface
 * @mlme_vdev_start_send:               callback to initiate actions of VDEV
 *                                      MLME start operation
 * @mlme_vdev_restart_send:             callback to initiate actions of VDEV
 *                                      MLME restart operation
 * @mlme_vdev_stop_start_send:          callback to block start/restart VDEV
 *                                      request command
 * @mlme_vdev_start_continue:           callback to initiate operations on
 *                                      LMAC/FW start response
 * @mlme_vdev_up_send:                  callback to initiate actions of VDEV
 *                                      MLME up operation
 * @mlme_vdev_notify_up_complete:       callback to notify VDEV MLME on moving
 *                                      to UP state
 * @mlme_vdev_update_beacon:            callback to initiate beacon update
 * @mlme_vdev_disconnect_peers:         callback to initiate disconnection of
 *                                      peers
 * @mlme_vdev_dfs_cac_timer_stop:       callback to stop the DFS CAC timer
 * @mlme_vdev_stop_send:                callback to initiate actions of VDEV
 *                                      MLME stop operation
 * @mlme_vdev_stop_continue:            callback to initiate operations on
 *                                      LMAC/FW stop response
 * @mlme_vdev_down_send:                callback to initiate actions of VDEV
 *                                      MLME down operation
 * @mlme_vdev_notify_down_complete:     callback to notify VDEV MLME on moving
 *                                      to INIT state
 * @mlme_vdev_is_newchan_no_cac:        callback to check if new channel is DFS
 *                                      and cac is not required
 * @mlme_vdev_ext_peer_delete_all_rsp:  callback to handle vdev delete all peer
 *                                      response and send result to upper layer
 */
static struct vdev_mlme_ops ap_mlme_ops = {
	.mlme_vdev_start_send = ap_mlme_vdev_start_send,
	.mlme_vdev_restart_send = ap_mlme_vdev_restart_send,
	.mlme_vdev_stop_start_send = ap_mlme_vdev_stop_start_send,
	.mlme_vdev_start_continue = ap_mlme_start_continue,
	.mlme_vdev_start_req_failed = ap_mlme_vdev_start_req_failed,
	.mlme_vdev_up_send = ap_mlme_vdev_up_send,
	.mlme_vdev_notify_up_complete = ap_mlme_vdev_notify_up_complete,
	.mlme_vdev_update_beacon = ap_mlme_vdev_update_beacon,
	.mlme_vdev_disconnect_peers = ap_mlme_vdev_disconnect_peers,
	.mlme_vdev_dfs_cac_timer_stop = ap_vdev_dfs_cac_timer_stop,
	.mlme_vdev_stop_send = ap_mlme_vdev_stop_send,
	.mlme_vdev_stop_continue = vdevmgr_mlme_stop_continue,
	.mlme_vdev_down_send = vdevmgr_mlme_vdev_down_send,
	.mlme_vdev_notify_down_complete = vdevmgr_notify_down_complete,
	.mlme_vdev_is_newchan_no_cac = ap_mlme_vdev_is_newchan_no_cac,
	.mlme_vdev_ext_stop_rsp = vdevmgr_vdev_stop_rsp_handle,
	.mlme_vdev_ext_start_rsp = vdevmgr_vdev_start_rsp_handle,
	.mlme_vdev_ext_peer_delete_all_rsp =
				vdevmgr_vdev_peer_delete_all_rsp_handle,
	.mlme_vdev_csa_complete = ap_mlme_vdev_csa_complete,
};

static struct vdev_mlme_ops mon_mlme_ops = {
	.mlme_vdev_start_send = mon_mlme_vdev_start_restart_send,
	.mlme_vdev_restart_send = mon_mlme_vdev_start_restart_send,
	.mlme_vdev_start_continue = mon_mlme_start_continue,
	.mlme_vdev_up_send = mon_mlme_vdev_up_send,
	.mlme_vdev_disconnect_peers = mon_mlme_vdev_disconnect_peers,
	.mlme_vdev_stop_send = mon_mlme_vdev_stop_send,
	.mlme_vdev_down_send = mon_mlme_vdev_down_send,
	.mlme_vdev_ext_start_rsp = vdevmgr_vdev_start_rsp_handle,
};

static struct mlme_ext_ops ext_ops = {
	.mlme_psoc_ext_hdl_create = psoc_mlme_ext_hdl_create,
	.mlme_psoc_ext_hdl_destroy = psoc_mlme_ext_hdl_destroy,
	.mlme_vdev_ext_hdl_create = vdevmgr_mlme_ext_hdl_create,
	.mlme_vdev_ext_hdl_destroy = vdevmgr_mlme_ext_hdl_destroy,
	.mlme_vdev_ext_hdl_post_create = vdevmgr_mlme_ext_post_hdl_create,
	.mlme_vdev_ext_delete_rsp = vdevmgr_vdev_delete_rsp_handle,
};
