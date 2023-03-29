/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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
 *  DOC:    wma_dev_if.c
 *  This file contains vdev & peer related operations.
 */

/* Header files */

#include "wma.h"
#include "wma_api.h"
#include "cds_api.h"
#include "wmi_unified_api.h"
#include "wlan_qct_sys.h"
#include "wni_api.h"
#include "ani_global.h"
#include "wmi_unified.h"
#include "wni_cfg.h"

#include "qdf_nbuf.h"
#include "qdf_types.h"
#include "qdf_mem.h"

#include "wma_types.h"
#include "lim_api.h"
#include "lim_session_utils.h"

#include "cds_utils.h"

#if !defined(REMOVE_PKT_LOG)
#include "pktlog_ac.h"
#endif /* REMOVE_PKT_LOG */

#include "dbglog_host.h"
#include "csr_api.h"

#include "wma_internal.h"

#include "wma_ocb.h"
#include "cdp_txrx_cfg.h"
#include "cdp_txrx_flow_ctrl_legacy.h"
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_cfg.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_misc.h>
#include <cdp_txrx_ctrl.h>

#include "wlan_policy_mgr_api.h"
#include "wma_nan_datapath.h"
#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif
#include <wlan_dfs_tgt_api.h>
#include <cdp_txrx_handle.h>
#include "wlan_pmo_ucfg_api.h"
#include "wlan_reg_services_api.h"
#include <include/wlan_vdev_mlme.h>
#include "wma_he.h"
#include "wlan_roam_debug.h"
#include "wlan_ocb_ucfg_api.h"
#include "init_deinit_lmac.h"
#include <target_if.h>
#include "wlan_policy_mgr_ucfg.h"
#include "wlan_mlme_public_struct.h"
#include "wlan_mlme_api.h"
#include "wlan_mlme_main.h"
#include "wlan_mlme_ucfg_api.h"
#include <wlan_dfs_utils_api.h>
#include "../../core/src/vdev_mgr_ops.h"
#include "wlan_utility.h"
#include "wlan_coex_ucfg_api.h"
#include <wlan_cp_stats_mc_ucfg_api.h>
#include "wmi_unified_vdev_api.h"
#include "wlan_nan_api.h"
#ifdef DCS_INTERFERENCE_DETECTION
#include <wlan_dcs_ucfg_api.h>
#endif

QDF_STATUS wma_find_vdev_id_by_addr(tp_wma_handle wma, uint8_t *addr,
				    uint8_t *vdev_id)
{
	uint8_t i;
	struct wlan_objmgr_vdev *vdev;

	for (i = 0; i < wma->max_bssid; i++) {
		vdev = wma->interfaces[i].vdev;
		if (!vdev)
			continue;

		if (qdf_is_macaddr_equal(
			(struct qdf_mac_addr *)wlan_vdev_mlme_get_macaddr(vdev),
			(struct qdf_mac_addr *)addr) == true) {
			*vdev_id = i;
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}


/**
 * wma_is_vdev_in_ap_mode() - check that vdev is in ap mode or not
 * @wma: wma handle
 * @vdev_id: vdev id
 *
 * Helper function to know whether given vdev id
 * is in AP mode or not.
 *
 * Return: True/False
 */
bool wma_is_vdev_in_ap_mode(tp_wma_handle wma, uint8_t vdev_id)
{
	struct wma_txrx_node *intf = wma->interfaces;

	if (vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev_id %hu", vdev_id);
		QDF_ASSERT(0);
		return false;
	}

	if ((intf[vdev_id].type == WMI_VDEV_TYPE_AP) &&
	    ((intf[vdev_id].sub_type == WMI_UNIFIED_VDEV_SUBTYPE_P2P_GO) ||
	     (intf[vdev_id].sub_type == 0)))
		return true;

	return false;
}

uint8_t *wma_get_vdev_bssid(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *mlme_obj;

	if (!vdev) {
		wma_err("vdev is NULL");
		return NULL;
	}

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!mlme_obj) {
		wma_err("Failed to get mlme_obj");
		return NULL;
	}

	return mlme_obj->mgmt.generic.bssid;
}

QDF_STATUS wma_find_vdev_id_by_bssid(tp_wma_handle wma, uint8_t *bssid,
				     uint8_t *vdev_id)
{
	int i;
	uint8_t *bssid_addr;

	for (i = 0; i < wma->max_bssid; i++) {
		if (!wma->interfaces[i].vdev)
			continue;
		bssid_addr = wma_get_vdev_bssid(wma->interfaces[i].vdev);
		if (!bssid_addr)
			continue;

		if (qdf_is_macaddr_equal(
			(struct qdf_mac_addr *)bssid_addr,
			(struct qdf_mac_addr *)bssid) == true) {
			*vdev_id = i;
			return QDF_STATUS_SUCCESS;
		}
	}

	return QDF_STATUS_E_FAILURE;
}

/**
 * wma_find_req_on_timer_expiry() - find request by address
 * @wma: wma handle
 * @req: pointer to the target request
 *
 * On timer expiry, the pointer to the req message is received from the
 * timer callback. Lookup the wma_hold_req_queue for the request with the
 * same address and return success if found.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS wma_find_req_on_timer_expiry(tp_wma_handle wma,
					       struct wma_target_req *req)
{
	struct wma_target_req *req_msg = NULL;
	bool found = false;
	qdf_list_node_t *cur_node = NULL, *next_node = NULL;
	QDF_STATUS status;

	qdf_spin_lock_bh(&wma->wma_hold_req_q_lock);
	if (QDF_STATUS_SUCCESS != qdf_list_peek_front(&wma->wma_hold_req_queue,
						      &next_node)) {
		qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
		wma_err("unable to get msg node from request queue");
		return QDF_STATUS_E_FAILURE;
	}

	do {
		cur_node = next_node;
		req_msg = qdf_container_of(cur_node,
					   struct wma_target_req, node);
		if (req_msg != req)
			continue;

		found = true;
		status = qdf_list_remove_node(&wma->wma_hold_req_queue,
					      cur_node);
		if (QDF_STATUS_SUCCESS != status) {
			qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
			wma_debug("Failed to remove request for req %pK", req);
			return QDF_STATUS_E_FAILURE;
		}
		break;
	} while (QDF_STATUS_SUCCESS  ==
		 qdf_list_peek_next(&wma->wma_hold_req_queue,
				    cur_node, &next_node));

	qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
	if (!found) {
		wma_err("target request not found for req %pK", req);
		return QDF_STATUS_E_INVAL;
	}

	wma_debug("target request found for vdev id: %d type %d",
		 req_msg->vdev_id, req_msg->type);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_find_req() - find target request for vdev id
 * @wma: wma handle
 * @vdev_id: vdev id
 * @type: request type
 *
 * Find target request for given vdev id & type of request.
 * Remove that request from active list.
 *
 * Return: return target request if found or NULL.
 */
static struct wma_target_req *wma_find_req(tp_wma_handle wma,
					   uint8_t vdev_id, uint8_t type)
{
	struct wma_target_req *req_msg = NULL;
	bool found = false;
	qdf_list_node_t *node1 = NULL, *node2 = NULL;
	QDF_STATUS status;

	qdf_spin_lock_bh(&wma->wma_hold_req_q_lock);
	if (QDF_STATUS_SUCCESS != qdf_list_peek_front(&wma->wma_hold_req_queue,
						      &node2)) {
		qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
		wma_err("unable to get msg node from request queue");
		return NULL;
	}

	do {
		node1 = node2;
		req_msg = qdf_container_of(node1, struct wma_target_req, node);
		if (req_msg->vdev_id != vdev_id)
			continue;
		if (req_msg->type != type)
			continue;

		found = true;
		status = qdf_list_remove_node(&wma->wma_hold_req_queue, node1);
		if (QDF_STATUS_SUCCESS != status) {
			qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
			wma_debug("Failed to remove request for vdev_id %d type %d",
				 vdev_id, type);
			return NULL;
		}
		break;
	} while (QDF_STATUS_SUCCESS  ==
			qdf_list_peek_next(&wma->wma_hold_req_queue, node1,
					   &node2));

	qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
	if (!found) {
		wma_err("target request not found for vdev_id %d type %d",
			 vdev_id, type);
		return NULL;
	}

	wma_debug("target request found for vdev id: %d type %d",
		 vdev_id, type);

	return req_msg;
}

/**
 * wma_find_remove_req_msgtype() - find and remove request for vdev id
 * @wma: wma handle
 * @vdev_id: vdev id
 * @msg_type: message request type
 *
 * Find target request for given vdev id & sub type of request.
 * Remove the same from active list.
 *
 * Return: Success if request found, failure other wise
 */
static struct wma_target_req *wma_find_remove_req_msgtype(tp_wma_handle wma,
					   uint8_t vdev_id, uint32_t msg_type)
{
	struct wma_target_req *req_msg = NULL;
	bool found = false;
	qdf_list_node_t *node1 = NULL, *node2 = NULL;
	QDF_STATUS status;

	qdf_spin_lock_bh(&wma->wma_hold_req_q_lock);
	if (QDF_STATUS_SUCCESS != qdf_list_peek_front(&wma->wma_hold_req_queue,
						      &node2)) {
		qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
		wma_err("unable to get msg node from request queue");
		return NULL;
	}

	do {
		node1 = node2;
		req_msg = qdf_container_of(node1, struct wma_target_req, node);
		if (req_msg->vdev_id != vdev_id)
			continue;
		if (req_msg->msg_type != msg_type)
			continue;

		found = true;
		status = qdf_list_remove_node(&wma->wma_hold_req_queue, node1);
		if (QDF_STATUS_SUCCESS != status) {
			qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
			wma_debug("Failed to remove request. vdev_id %d type %d",
				 vdev_id, msg_type);
			return NULL;
		}
		break;
	} while (QDF_STATUS_SUCCESS  ==
			qdf_list_peek_next(&wma->wma_hold_req_queue, node1,
					   &node2));

	qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
	if (!found) {
		wma_err("target request not found for vdev_id %d type %d",
			 vdev_id, msg_type);
		return NULL;
	}

	wma_debug("target request found for vdev id: %d type %d",
		 vdev_id, msg_type);

	return req_msg;
}

QDF_STATUS wma_vdev_detach_callback(struct vdev_delete_response *rsp)
{
	tp_wma_handle wma;
	struct wma_txrx_node *iface = NULL;

	wma = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma) {
		wma_err("wma handle is NULL for VDEV_%d", rsp->vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	/* Sanitize the vdev id*/
	if (rsp->vdev_id > wma->max_bssid) {
		wma_err("vdev delete response with invalid vdev_id :%d",
			rsp->vdev_id);
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	iface = &wma->interfaces[rsp->vdev_id];

	wma_debug("vdev del response received for VDEV_%d", rsp->vdev_id);
	iface->del_staself_req = NULL;

	if (iface->roam_scan_stats_req) {
		struct sir_roam_scan_stats *roam_scan_stats_req =
						iface->roam_scan_stats_req;

		iface->roam_scan_stats_req = NULL;
		qdf_mem_free(roam_scan_stats_req);
	}

	wma_vdev_deinit(iface);
	qdf_mem_zero(iface, sizeof(*iface));
	wma_vdev_init(iface);

	mlme_vdev_del_resp(rsp->vdev_id);

	return QDF_STATUS_SUCCESS;
}

static void
wma_cdp_vdev_detach(ol_txrx_soc_handle soc, tp_wma_handle wma_handle,
		    uint8_t vdev_id)
{
	struct wma_txrx_node *iface = &wma_handle->interfaces[vdev_id];
	struct wlan_objmgr_vdev *vdev = iface->vdev;

	if (!vdev) {
		wma_err("vdev is NULL");
		return;
	}

	if (soc && wlan_vdev_get_id(vdev) != WLAN_INVALID_VDEV_ID)
		cdp_vdev_detach(soc, vdev_id, NULL, NULL);
}

/**
 * wma_release_vdev_ref() - Release vdev object reference count
 * @iface: wma interface txrx node
 *
 * Purpose of this function is to release vdev object reference count
 * from wma interface txrx node.
 *
 * Return: None
 */
static void
wma_release_vdev_ref(struct wma_txrx_node *iface)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = iface->vdev;

	iface->vdev_active = false;
	iface->vdev = NULL;
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_WMA_ID);
}

/**
 * wma_handle_monitor_mode_vdev_detach() - Stop and down monitor mode vdev
 * @wma_handle: wma handle
 * @vdev_id: used to get wma interface txrx node
 *
 * Monitor mode is unconneted mode, so do explicit vdev stop and down
 *
 * Return: None
 */
static void wma_handle_monitor_mode_vdev_detach(tp_wma_handle wma,
						uint8_t vdev_id)
{
	struct wma_txrx_node *iface;

	iface = &wma->interfaces[vdev_id];
	wlan_vdev_mlme_sm_deliver_evt(iface->vdev,
				      WLAN_VDEV_SM_EV_DOWN,
				      0, NULL);
	iface->vdev_active = false;
}

/**
 * wma_handle_vdev_detach() - wma vdev detach handler
 * @wma_handle: pointer to wma handle
 * @del_vdev_req_param: pointer to del req param
 *
 * Return: none.
 */
static QDF_STATUS wma_handle_vdev_detach(tp_wma_handle wma_handle,
			struct del_vdev_params *del_vdev_req_param)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t vdev_id = del_vdev_req_param->vdev_id;
	struct wma_txrx_node *iface = &wma_handle->interfaces[vdev_id];
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	int i;

	if (!soc) {
		wma_err("SOC context is NULL");
		status = QDF_STATUS_E_FAILURE;
		goto rel_ref;
	}

	if ((cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE) ||
	    (policy_mgr_is_sta_mon_concurrency(wma_handle->psoc) &&
	    wlan_vdev_mlme_get_opmode(iface->vdev) == QDF_MONITOR_MODE))
		wma_handle_monitor_mode_vdev_detach(wma_handle, vdev_id);

rel_ref:
	wma_cdp_vdev_detach(soc, wma_handle, vdev_id);
	if (qdf_is_recovering())
		wlan_mgmt_txrx_vdev_drain(iface->vdev,
					  wma_mgmt_frame_fill_peer_cb, &i);
	wma_debug("Releasing wma reference for vdev:%d", vdev_id);
	wma_release_vdev_ref(iface);
	return status;
}

/**
 * wma_self_peer_remove() - Self peer remove handler
 * @wma: wma handle
 * @del_vdev_req_param: vdev id
 * @generate_vdev_rsp: request type
 *
 * Return: success if peer delete command sent to firmware, else failure.
 */
static QDF_STATUS wma_self_peer_remove(tp_wma_handle wma_handle,
				       struct del_vdev_params *del_vdev_req)
{
	QDF_STATUS qdf_status;
	uint8_t vdev_id = del_vdev_req->vdev_id;
	struct wma_target_req *msg = NULL;
	struct del_sta_self_rsp_params *sta_self_wmi_rsp;

	wma_debug("P2P Device: removing self peer "QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(del_vdev_req->self_mac_addr));

	qdf_status = wma_remove_peer(wma_handle, del_vdev_req->self_mac_addr,
				     vdev_id, false);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		wma_err("wma_remove_peer is failed");
		goto error;
	}

	if (wmi_service_enabled(wma_handle->wmi_handle,
				wmi_service_sync_delete_cmds)) {
		sta_self_wmi_rsp =
			qdf_mem_malloc(sizeof(struct del_sta_self_rsp_params));
		if (!sta_self_wmi_rsp) {
			qdf_status = QDF_STATUS_E_NOMEM;
			goto error;
		}

		sta_self_wmi_rsp->self_sta_param = del_vdev_req;
		msg = wma_fill_hold_req(wma_handle, vdev_id,
					WMA_DELETE_STA_REQ,
					WMA_DEL_P2P_SELF_STA_RSP_START,
					sta_self_wmi_rsp,
					WMA_DELETE_STA_TIMEOUT);
		if (!msg) {
			wma_err("Failed to allocate request for vdev_id %d",
				vdev_id);
			wma_remove_req(wma_handle, vdev_id,
				       WMA_DEL_P2P_SELF_STA_RSP_START);
			qdf_mem_free(sta_self_wmi_rsp);
			qdf_status = QDF_STATUS_E_FAILURE;
			goto error;
		}
	}

error:
	return qdf_status;
}

/**
 * wma_remove_objmgr_peer() - remove objmgr peer information from host driver
 * @wma: wma handle
 * @vdev_id: vdev id
 * @peer_addr: peer mac address
 *
 * Return: none
 */
static void wma_remove_objmgr_peer(tp_wma_handle wma,
				   struct wlan_objmgr_vdev *obj_vdev,
				   uint8_t *peer_addr)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_peer *obj_peer;
	struct wlan_objmgr_pdev *obj_pdev;
	uint8_t pdev_id = 0;

	psoc = wma->psoc;
	if (!psoc) {
		wma_err("PSOC is NULL");
		return;
	}

	obj_pdev = wlan_vdev_get_pdev(obj_vdev);
	pdev_id = wlan_objmgr_pdev_get_pdev_id(obj_pdev);
	obj_peer = wlan_objmgr_get_peer(psoc, pdev_id, peer_addr,
					WLAN_LEGACY_WMA_ID);
	if (obj_peer) {
		wlan_objmgr_peer_obj_delete(obj_peer);
		/* Unref to decrement ref happened in find_peer */
		wlan_objmgr_peer_release_ref(obj_peer, WLAN_LEGACY_WMA_ID);
	} else {
		wma_nofl_err("Peer "QDF_MAC_ADDR_FMT" not found",
			 QDF_MAC_ADDR_REF(peer_addr));
	}

}

static QDF_STATUS wma_check_for_deffered_peer_delete(tp_wma_handle wma_handle,
						     struct del_vdev_params
						     *pdel_vdev_req_param)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t vdev_id = pdel_vdev_req_param->vdev_id;
	struct wma_txrx_node *iface = &wma_handle->interfaces[vdev_id];
	uint32_t vdev_stop_type;

	if (qdf_atomic_read(&iface->bss_status) == WMA_BSS_STATUS_STARTED) {
		status = mlme_get_vdev_stop_type(iface->vdev, &vdev_stop_type);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("Failed to get wma req msg_type for vdev_id: %d",
				vdev_id);
			status = QDF_STATUS_E_INVAL;
			return status;
		}

		if (vdev_stop_type != WMA_DELETE_BSS_REQ) {
			status = QDF_STATUS_E_INVAL;
			return status;
		}

		wma_debug("BSS is not yet stopped. Defering vdev(vdev id %x) deletion",
			  vdev_id);
		iface->del_staself_req = pdel_vdev_req_param;
		iface->is_del_sta_defered = true;
	}

	return status;
}

static QDF_STATUS wma_vdev_self_peer_delete(tp_wma_handle wma_handle,
					    struct del_vdev_params
					    *pdel_vdev_req_param)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t vdev_id = pdel_vdev_req_param->vdev_id;
	struct wma_txrx_node *iface = &wma_handle->interfaces[vdev_id];

	if (mlme_vdev_uses_self_peer(iface->type, iface->sub_type)) {
		status = wma_self_peer_remove(wma_handle, pdel_vdev_req_param);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("can't remove selfpeer, send rsp session: %d",
				vdev_id);
			wma_handle_vdev_detach(wma_handle, pdel_vdev_req_param);
			mlme_vdev_self_peer_delete_resp(pdel_vdev_req_param);
			cds_trigger_recovery(QDF_REASON_UNSPECIFIED);
			return status;
		}
	} else if (iface->type == WMI_VDEV_TYPE_STA) {
		wma_remove_objmgr_peer(wma_handle, iface->vdev,
				       pdel_vdev_req_param->self_mac_addr);
	}

	return status;
}

QDF_STATUS wma_vdev_detach(struct del_vdev_params *pdel_vdev_req_param)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t vdev_id;
	struct wma_txrx_node *iface = NULL;
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle)
		return QDF_STATUS_E_INVAL;

	vdev_id = wlan_vdev_get_id(pdel_vdev_req_param->vdev);
	iface = &wma_handle->interfaces[vdev_id];
	if (!iface->vdev) {
		wma_err("vdev %d is NULL", vdev_id);
		mlme_vdev_self_peer_delete_resp(pdel_vdev_req_param);
		return status;
	}

	status = wma_check_for_deffered_peer_delete(wma_handle,
						    pdel_vdev_req_param);
	if (QDF_IS_STATUS_ERROR(status))
		goto  send_fail_rsp;

	if (iface->is_del_sta_defered)
		return status;

	iface->is_del_sta_defered = false;
	iface->del_staself_req = NULL;

	status = wma_vdev_self_peer_delete(wma_handle, pdel_vdev_req_param);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to send self peer delete:%d", status);
		status = QDF_STATUS_E_INVAL;
		return status;
	}

	if (iface->type != WMI_VDEV_TYPE_MONITOR)
		iface->vdev_active = false;

	if (!mlme_vdev_uses_self_peer(iface->type, iface->sub_type) ||
	    !wmi_service_enabled(wma_handle->wmi_handle,
	    wmi_service_sync_delete_cmds)) {
		status = wma_handle_vdev_detach(wma_handle,
						pdel_vdev_req_param);
		pdel_vdev_req_param->status = status;
		mlme_vdev_self_peer_delete_resp(pdel_vdev_req_param);
	}

	return status;

send_fail_rsp:
	wma_err("rcvd del_self_sta without del_bss; vdev_id:%d", vdev_id);
	cds_trigger_recovery(QDF_REASON_UNSPECIFIED);
	status = QDF_STATUS_E_FAILURE;
	return status;
}

/**
 * wma_send_start_resp() - send vdev start response to upper layer
 * @wma: wma handle
 * @add_bss: add bss params
 * @resp_event: response params
 *
 * Return: none
 */
static void wma_send_start_resp(tp_wma_handle wma,
				struct add_bss_rsp *add_bss_rsp,
				struct vdev_start_response *rsp)
{
	struct wma_txrx_node *iface = &wma->interfaces[rsp->vdev_id];
	QDF_STATUS status;

	if (QDF_IS_STATUS_SUCCESS(rsp->status) &&
	    QDF_IS_STATUS_SUCCESS(add_bss_rsp->status)) {
		status =
		  wlan_vdev_mlme_sm_deliver_evt(iface->vdev,
						WLAN_VDEV_SM_EV_START_RESP,
						sizeof(*add_bss_rsp),
						add_bss_rsp);
		if (QDF_IS_STATUS_SUCCESS(status))
			return;

		add_bss_rsp->status = status;
	}

	/* Send vdev stop if vdev start was success */
	if (QDF_IS_STATUS_ERROR(add_bss_rsp->status) &&
	    QDF_IS_STATUS_SUCCESS(rsp->status)) {
		wlan_vdev_mlme_sm_deliver_evt(iface->vdev,
					      WLAN_VDEV_SM_EV_DOWN,
					      sizeof(*add_bss_rsp),
					      add_bss_rsp);
		return;
	}

	wma_remove_bss_peer_on_vdev_start_failure(wma, rsp->vdev_id);

	wma_debug("Sending add bss rsp to umac(vdev %d status %d)",
		 rsp->vdev_id, add_bss_rsp->status);
	lim_handle_add_bss_rsp(wma->mac_context, add_bss_rsp);
}

/**
 * wma_vdev_start_rsp() - send vdev start response to upper layer
 * @wma: wma handle
 * @vdev: vdev
 * @resp_event: response params
 *
 * Return: none
 */
static void wma_vdev_start_rsp(tp_wma_handle wma, struct wlan_objmgr_vdev *vdev,
			       struct vdev_start_response *rsp)
{
	struct beacon_info *bcn;
	enum QDF_OPMODE opmode;
	struct add_bss_rsp *add_bss_rsp;

	opmode = wlan_vdev_mlme_get_opmode(vdev);

	add_bss_rsp = qdf_mem_malloc(sizeof(*add_bss_rsp));
	if (!add_bss_rsp)
		return;

	add_bss_rsp->vdev_id = rsp->vdev_id;
	add_bss_rsp->status = rsp->status;
	add_bss_rsp->chain_mask = rsp->chain_mask;
	add_bss_rsp->smps_mode  = host_map_smps_mode(rsp->smps_mode);

	if (rsp->status)
		goto send_fail_resp;

	if (opmode == QDF_P2P_GO_MODE || opmode == QDF_SAP_MODE) {
		wma->interfaces[rsp->vdev_id].beacon =
			qdf_mem_malloc(sizeof(struct beacon_info));

		bcn = wma->interfaces[rsp->vdev_id].beacon;
		if (!bcn) {
			add_bss_rsp->status = QDF_STATUS_E_NOMEM;
			goto send_fail_resp;
		}
		bcn->buf = qdf_nbuf_alloc(NULL, SIR_MAX_BEACON_SIZE, 0,
					  sizeof(uint32_t), 0);
		if (!bcn->buf) {
			qdf_mem_free(bcn);
			add_bss_rsp->status = QDF_STATUS_E_FAILURE;
			goto send_fail_resp;
		}
		bcn->seq_no = MIN_SW_SEQ;
		qdf_spinlock_create(&bcn->lock);
		qdf_atomic_set(&wma->interfaces[rsp->vdev_id].bss_status,
			       WMA_BSS_STATUS_STARTED);
		wma_debug("AP mode (type %d subtype %d) BSS is started",
			 wma->interfaces[rsp->vdev_id].type,
			 wma->interfaces[rsp->vdev_id].sub_type);
	}

send_fail_resp:
	wma_send_start_resp(wma, add_bss_rsp, rsp);
}

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/**
 * wma_find_mcc_ap() - finds if device is operating AP in MCC mode or not
 * @wma: wma handle.
 * @vdev_id: vdev ID of device for which MCC has to be checked
 * @add: flag indicating if current device is added or deleted
 *
 * This function parses through all the interfaces in wma and finds if
 * any of those devces are in MCC mode with AP. If such a vdev is found
 * involved AP vdevs are sent WDA_UPDATE_Q2Q_IE_IND msg to update their
 * beacon template to include Q2Q IE.
 *
 * Return: none
 */
static void wma_find_mcc_ap(tp_wma_handle wma, uint8_t vdev_id, bool add)
{
	uint8_t i;
	uint16_t prev_ch_freq = 0;
	bool is_ap = false;
	bool result = false;
	uint8_t *ap_vdev_ids = NULL;
	uint8_t num_ch = 0;

	ap_vdev_ids = qdf_mem_malloc(wma->max_bssid);
	if (!ap_vdev_ids)
		return;

	for (i = 0; i < wma->max_bssid; i++) {
		ap_vdev_ids[i] = -1;
		if (add == false && i == vdev_id)
			continue;

		if (wma_is_vdev_up(vdev_id) || (i == vdev_id && add)) {
			if (wma->interfaces[i].type == WMI_VDEV_TYPE_AP) {
				is_ap = true;
				ap_vdev_ids[i] = i;
			}

			if (wma->interfaces[i].ch_freq != prev_ch_freq) {
				num_ch++;
				prev_ch_freq = wma->interfaces[i].ch_freq;
			}
		}
	}

	if (is_ap && (num_ch > 1))
		result = true;
	else
		result = false;

	wma_send_msg(wma, WMA_UPDATE_Q2Q_IE_IND, (void *)ap_vdev_ids, result);
}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

/**
 * wma_handle_hidden_ssid_restart() - handle hidden ssid restart
 * @wma: wma handle
 * @iface: interfcae pointer
 *
 * Return: none
 */
static void wma_handle_hidden_ssid_restart(tp_wma_handle wma,
					   struct wma_txrx_node *iface)
{
	wlan_vdev_mlme_sm_deliver_evt(iface->vdev,
				      WLAN_VDEV_SM_EV_RESTART_RESP,
				      0, NULL);
}

static void wma_peer_send_phymode(struct wlan_objmgr_vdev *vdev,
				  void *object, void *arg)
{
	struct wlan_objmgr_peer *peer = object;
	enum wlan_phymode old_peer_phymode;
	struct wlan_channel *vdev_chan;
	enum wlan_phymode new_phymode;
	tSirNwType nw_type;
	uint32_t fw_phymode;
	uint32_t max_ch_width_supported;
	tp_wma_handle wma;
	uint8_t *peer_mac_addr;
	uint8_t vdev_id;

	if (wlan_peer_get_peer_type(peer) == WLAN_PEER_SELF)
		return;

	wma = cds_get_context(QDF_MODULE_ID_WMA);

	if(!wma) {
		wma_err("wma handle NULL");
		return;
	}

	old_peer_phymode = wlan_peer_get_phymode(peer);
	vdev_chan = wlan_vdev_mlme_get_des_chan(vdev);

	peer_mac_addr = wlan_peer_get_macaddr(peer);

	if (WLAN_REG_IS_24GHZ_CH_FREQ(vdev_chan->ch_freq)) {
		if (vdev_chan->ch_phymode == WLAN_PHYMODE_11B ||
		    old_peer_phymode == WLAN_PHYMODE_11B)
			nw_type = eSIR_11B_NW_TYPE;
		else
			nw_type = eSIR_11G_NW_TYPE;
	} else {
		nw_type = eSIR_11A_NW_TYPE;
	}
	new_phymode = wma_peer_phymode(nw_type, STA_ENTRY_PEER,
				       IS_WLAN_PHYMODE_HT(old_peer_phymode),
				       vdev_chan->ch_width,
				       IS_WLAN_PHYMODE_VHT(old_peer_phymode),
				       IS_WLAN_PHYMODE_HE(old_peer_phymode));

	if (new_phymode == old_peer_phymode) {
		wma_debug("Ignore update as old %d and new %d phymode are same for mac "QDF_MAC_ADDR_FMT,
			  old_peer_phymode, new_phymode,
			  QDF_MAC_ADDR_REF(peer_mac_addr));
		return;
	}
	wlan_peer_set_phymode(peer, new_phymode);

	fw_phymode = wma_host_to_fw_phymode(new_phymode);
	vdev_id = wlan_vdev_get_id(vdev);

	wma_set_peer_param(wma, peer_mac_addr, WMI_PEER_PHYMODE,
			   fw_phymode, vdev_id);

	max_ch_width_supported =
		wmi_get_ch_width_from_phy_mode(wma->wmi_handle, fw_phymode);
	wma_set_peer_param(wma, peer_mac_addr, WMI_PEER_CHWIDTH,
			   max_ch_width_supported, vdev_id);

	wma_debug("FW phymode %d old phymode %d new phymode %d bw %d macaddr "QDF_MAC_ADDR_FMT,
		  fw_phymode, old_peer_phymode, new_phymode,
		  max_ch_width_supported, QDF_MAC_ADDR_REF(peer_mac_addr));
}

static
void wma_update_rate_flags_after_vdev_restart(tp_wma_handle wma,
						struct wma_txrx_node *iface)
{
	struct vdev_mlme_obj *vdev_mlme;
	uint32_t rate_flags = 0;
	enum wlan_phymode bss_phymode;
	struct wlan_channel *des_chan;

	if (iface->type != WMI_VDEV_TYPE_STA)
		return;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(iface->vdev);
	if (!vdev_mlme)
		return;

	des_chan = wlan_vdev_mlme_get_des_chan(iface->vdev);
	bss_phymode = des_chan->ch_phymode;

	if (IS_WLAN_PHYMODE_HE(bss_phymode)) {
		rate_flags = wma_get_he_rate_flags(des_chan->ch_width);
	} else if (IS_WLAN_PHYMODE_VHT(bss_phymode)) {
		rate_flags = wma_get_vht_rate_flags(des_chan->ch_width);
	} else if (IS_WLAN_PHYMODE_HT(bss_phymode)) {
		rate_flags = wma_get_ht_rate_flags(des_chan->ch_width);
	} else {
		rate_flags = TX_RATE_LEGACY;
	}

	vdev_mlme->mgmt.rate_info.rate_flags = rate_flags;

	wma_debug("bss phymode %d rate_flags %x, ch_width %d",
		  bss_phymode, rate_flags, des_chan->ch_width);

	ucfg_mc_cp_stats_set_rate_flags(iface->vdev, rate_flags);
}

QDF_STATUS wma_handle_channel_switch_resp(tp_wma_handle wma,
					  struct vdev_start_response *rsp)
{
	enum wlan_vdev_sm_evt  event;
	struct wma_txrx_node *iface;

	iface = &wma->interfaces[rsp->vdev_id];
	wma_debug("Send channel switch resp vdev %d status %d",
		 rsp->vdev_id, rsp->status);

	/* Indicate channel switch failure to LIM */
	if (QDF_IS_STATUS_ERROR(rsp->status) &&
	    (iface->type == WMI_VDEV_TYPE_MONITOR ||
	     wma_is_vdev_in_ap_mode(wma, rsp->vdev_id) ||
	     mlme_is_chan_switch_in_progress(iface->vdev))) {
		mlme_set_chan_switch_in_progress(iface->vdev, false);
		lim_process_switch_channel_rsp(wma->mac_context, rsp);
		return QDF_STATUS_SUCCESS;
	}

	if (QDF_IS_STATUS_SUCCESS(rsp->status) &&
	    rsp->resp_type == WMI_VDEV_RESTART_RESP_EVENT) {
		wlan_objmgr_iterate_peerobj_list(iface->vdev,
					 wma_peer_send_phymode, NULL,
					 WLAN_LEGACY_WMA_ID);
		wma_update_rate_flags_after_vdev_restart(wma, iface);
	}

	if (wma_is_vdev_in_ap_mode(wma, rsp->vdev_id) ||
	    mlme_is_chan_switch_in_progress(iface->vdev))
		event = WLAN_VDEV_SM_EV_RESTART_RESP;
	else
		event = WLAN_VDEV_SM_EV_START_RESP;
	wlan_vdev_mlme_sm_deliver_evt(iface->vdev, event,
				      sizeof(rsp), rsp);

	return QDF_STATUS_SUCCESS;
}

#ifdef DCS_INTERFERENCE_DETECTION
/**
 * wma_dcs_clear_vdev_starting() - clear vdev starting within dcs information
 * @mac_ctx: mac context
 * @vdev_id: vdev id
 *
 * This function is used to clear vdev starting within dcs information
 *
 * Return: None
 */
static void wma_dcs_clear_vdev_starting(struct mac_context *mac_ctx,
					uint32_t vdev_id)
{
	mac_ctx->sap.dcs_info.is_vdev_starting[vdev_id] = false;
}

/**
 * wma_dcs_wlan_interference_mitigation_enable() - enable wlan
 * interference mitigation
 * @mac_ctx: mac context
 * @mac_id: mac id
 * @vdev_id: vdev id
 *
 * This function is used to enable wlan interference mitigation through
 * send dcs command
 *
 * Return: None
 */
static void wma_dcs_wlan_interference_mitigation_enable(
					struct mac_context *mac_ctx,
					uint32_t mac_id,
					uint32_t vdev_id)
{
	int vdev_index;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t count;
	bool wlan_interference_mitigation_enable =
			mac_ctx->sap.dcs_info.
				wlan_interference_mitigation_enable[vdev_id];

	count = policy_mgr_get_sap_go_count_on_mac(
			mac_ctx->psoc, list, mac_id);

	for (vdev_index = 0; vdev_index < count; vdev_index++) {
		if (mac_ctx->sap.dcs_info.is_vdev_starting[list[vdev_index]]) {
			wma_err("vdev %d: does not finish restart",
				 list[vdev_index]);
			return;
		}
		wlan_interference_mitigation_enable =
			wlan_interference_mitigation_enable ||
		mac_ctx->sap.dcs_info.
			wlan_interference_mitigation_enable[list[vdev_index]];
	}

	if (wlan_interference_mitigation_enable)
		ucfg_config_dcs_enable(
			mac_ctx->psoc, mac_id, CAP_DCS_WLANIM);
	ucfg_wlan_dcs_cmd(mac_ctx->psoc, mac_id, true);
}
#else
static void wma_dcs_wlan_interference_mitigation_enable(
					struct mac_context *mac_ctx,
					uint32_t mac_id,
					uint32_t vdev_id)
{
}


static void wma_dcs_clear_vdev_starting(struct mac_context *mac_ctx,
					uint32_t vdev_id)
{
}
#endif

/*
 * wma_get_ratemask_type() - convert user input ratemask type to FW type
 * @type: User input ratemask type maintained in HDD
 * @fwtype: Value return arg for fw ratemask type value
 *
 * Return: FW configurable ratemask type
 */
static QDF_STATUS wma_get_ratemask_type(enum wlan_mlme_ratemask_type type,
					uint8_t *fwtype)
{
	switch (type) {
	case WLAN_MLME_RATEMASK_TYPE_CCK:
		*fwtype = WMI_RATEMASK_TYPE_CCK;
		break;
	case WLAN_MLME_RATEMASK_TYPE_HT:
		*fwtype = WMI_RATEMASK_TYPE_HT;
		break;
	case WLAN_MLME_RATEMASK_TYPE_VHT:
		*fwtype = WMI_RATEMASK_TYPE_VHT;
		break;
	case WLAN_MLME_RATEMASK_TYPE_HE:
		*fwtype = WMI_RATEMASK_TYPE_HE;
		break;
	default:
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_vdev_start_resp_handler(struct vdev_mlme_obj *vdev_mlme,
				       struct vdev_start_response *rsp)
{
	tp_wma_handle wma;
	struct wma_txrx_node *iface;
	target_resource_config *wlan_res_cfg;
	struct wlan_objmgr_psoc *psoc;
	struct mac_context *mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	QDF_STATUS status;
	enum vdev_assoc_type assoc_type = VDEV_ASSOC;
	struct vdev_mlme_obj *mlme_obj;
	struct wlan_mlme_psoc_ext_obj *mlme_psoc_obj;
	const struct wlan_mlme_ratemask *ratemask_cfg;
	struct config_ratemask_params rparams = {0};

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("wma wma is NULL for VDEV_%d", rsp->vdev_id);
		return QDF_STATUS_E_FAILURE;
	}
	psoc = wma->psoc;
	if (!psoc) {
		wma_err("psoc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_psoc_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_psoc_obj) {
		wma_err("Failed to get mlme_psoc");
		return QDF_STATUS_E_FAILURE;
	}

	ratemask_cfg = &mlme_psoc_obj->cfg.ratemask_cfg;

	if (!mac_ctx) {
		wma_err("Failed to get mac_ctx");
		return QDF_STATUS_E_FAILURE;
	}

	wlan_res_cfg = lmac_get_tgt_res_cfg(psoc);
	if (!wlan_res_cfg) {
		wma_err("Wlan resource config is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (rsp->vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev id received from firmware");
		return QDF_STATUS_E_FAILURE;
	}

	if (wma_is_vdev_in_ap_mode(wma, rsp->vdev_id))
		tgt_dfs_radar_enable(wma->pdev, 0, 0, true);

	iface = &wma->interfaces[rsp->vdev_id];
	if (!iface->vdev) {
		wma_err("Invalid vdev");
		return QDF_STATUS_E_FAILURE;
	}

	if (rsp->status == QDF_STATUS_SUCCESS) {
		wma->interfaces[rsp->vdev_id].tx_streams =
			rsp->cfgd_tx_streams;

		if (wlan_res_cfg->use_pdev_id) {
			if (rsp->mac_id == OL_TXRX_PDEV_ID) {
				wma_err("soc level id received for mac id");
				return -QDF_STATUS_E_INVAL;
			}
			wma->interfaces[rsp->vdev_id].mac_id =
				WMA_PDEV_TO_MAC_MAP(rsp->mac_id);
		} else {
			wma->interfaces[rsp->vdev_id].mac_id =
			rsp->mac_id;
		}

		wma_debug("vdev:%d tx ss=%d rx ss=%d chain mask=%d mac=%d",
				rsp->vdev_id,
				rsp->cfgd_tx_streams,
				rsp->cfgd_rx_streams,
				rsp->chain_mask,
				wma->interfaces[rsp->vdev_id].mac_id);

		/* Fill bss_chan after vdev start */
		qdf_mem_copy(iface->vdev->vdev_mlme.bss_chan,
			     iface->vdev->vdev_mlme.des_chan,
			     sizeof(struct wlan_channel));
	}

	if (wma_is_vdev_in_ap_mode(wma, rsp->vdev_id)) {
		wma_dcs_clear_vdev_starting(mac_ctx, rsp->vdev_id);
		wma_dcs_wlan_interference_mitigation_enable(mac_ctx,
							    iface->mac_id,
							    rsp->vdev_id);
	}

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	if (rsp->status == QDF_STATUS_SUCCESS
		&& mac_ctx->sap.sap_channel_avoidance)
		wma_find_mcc_ap(wma, rsp->vdev_id, true);
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

	if (wma_get_hidden_ssid_restart_in_progress(iface) &&
	    wma_is_vdev_in_ap_mode(wma, rsp->vdev_id)) {
		wma_handle_hidden_ssid_restart(wma, iface);
		return QDF_STATUS_SUCCESS;
	}

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(iface->vdev);
	if (!mlme_obj)
		return QDF_STATUS_E_INVAL;

	mlme_obj->mgmt.generic.tx_pwrlimit = rsp->max_allowed_tx_power;
	if (iface->type == WMI_VDEV_TYPE_STA)
		assoc_type = mlme_get_assoc_type(vdev_mlme->vdev);

	if (mlme_is_chan_switch_in_progress(iface->vdev) ||
	    iface->type == WMI_VDEV_TYPE_MONITOR ||
	    (iface->type == WMI_VDEV_TYPE_STA &&
	     (assoc_type == VDEV_ASSOC || assoc_type == VDEV_REASSOC))) {
		status = wma_handle_channel_switch_resp(wma,
							rsp);
		if (QDF_IS_STATUS_ERROR(status))
			return QDF_STATUS_E_FAILURE;
	}  else if (iface->type == WMI_VDEV_TYPE_OCB) {
		mlme_obj->proto.sta.assoc_id = iface->aid;
		if (vdev_mgr_up_send(mlme_obj) != QDF_STATUS_SUCCESS) {
			wma_err("failed to send vdev up");
			return QDF_STATUS_E_FAILURE;
		}
		ucfg_ocb_config_channel(wma->pdev);
	} else {
		struct qdf_mac_addr bss_peer;

		status =
			mlme_get_vdev_bss_peer_mac_addr(iface->vdev, &bss_peer);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("Failed to get bssid");
			return QDF_STATUS_E_INVAL;
		}
		qdf_mem_copy(mlme_obj->mgmt.generic.bssid, bss_peer.bytes,
			     QDF_MAC_ADDR_SIZE);
		wma_vdev_start_rsp(wma, vdev_mlme->vdev, rsp);
	}
	if (iface->type == WMI_VDEV_TYPE_AP && wma_is_vdev_up(rsp->vdev_id))
		wma_set_sap_keepalive(wma, rsp->vdev_id);

	/* Send ratemask to firmware */
	if ((ratemask_cfg->type > WLAN_MLME_RATEMASK_TYPE_NO_MASK) &&
	    (ratemask_cfg->type < WLAN_MLME_RATEMASK_TYPE_MAX)) {
		struct wmi_unified *wmi_handle = wma->wmi_handle;

		if (!wmi_handle) {
			wma_err(FL("wmi_handle is null"));
			return QDF_STATUS_E_INVAL;
		}

		rparams.vdev_id = rsp->vdev_id;
		status = wma_get_ratemask_type(ratemask_cfg->type,
					       &rparams.type);

		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err(FL("unable to map ratemask"));
			/* don't fail, default rates will still work */
			return QDF_STATUS_SUCCESS;
		}

		rparams.lower32 = ratemask_cfg->lower32;
		rparams.higher32 = ratemask_cfg->higher32;
		rparams.lower32_2 = ratemask_cfg->lower32_2;
		rparams.higher32_2 = ratemask_cfg->higher32_2;

		status = wmi_unified_vdev_config_ratemask_cmd_send(wmi_handle,
								   &rparams);
		/* Only log failure. Do not abort */
		if (QDF_IS_STATUS_ERROR(status))
			wma_err(FL("failed to send ratemask"));
	}

	return QDF_STATUS_SUCCESS;
}

bool wma_is_vdev_valid(uint32_t vdev_id)
{
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		wma_debug("vdev_id: %d, null wma_handle", vdev_id);
		return false;
	}

	/* No of interface are allocated based on max_bssid value */
	if (vdev_id >= wma_handle->max_bssid) {
		wma_debug("vdev_id: %d is invalid, max_bssid: %d",
			 vdev_id, wma_handle->max_bssid);
		return false;
	}

	return wma_handle->interfaces[vdev_id].vdev_active;
}

/**
 * wma_vdev_set_param() - set per vdev params in fw
 * @wmi_handle: wmi handle
 * @if_id: vdev id
 * @param_id: parameter id
 * @param_value: parameter value
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS
wma_vdev_set_param(wmi_unified_t wmi_handle, uint32_t if_id,
				uint32_t param_id, uint32_t param_value)
{
	struct vdev_set_params param = {0};

	if (!wma_is_vdev_valid(if_id)) {
		wma_err("vdev_id: %d is not active reject the req: param id %d val %d",
			if_id, param_id, param_value);
		return QDF_STATUS_E_INVAL;
	}

	param.vdev_id = if_id;
	param.param_id = param_id;
	param.param_value = param_value;

	return wmi_unified_vdev_set_param_send(wmi_handle, &param);
}

/**
 * wma_set_peer_authorized_cb() - set peer authorized callback function
 * @wma_ctx: wma handle
 * @auth_cb: peer authorized callback
 *
 * Return: none
 */
void wma_set_peer_authorized_cb(void *wma_ctx, wma_peer_authorized_fp auth_cb)
{
	tp_wma_handle wma_handle = (tp_wma_handle) wma_ctx;

	wma_handle->peer_authorized_cb = auth_cb;
}

/**
 * wma_set_peer_param() - set peer parameter in fw
 * @wma_ctx: wma handle
 * @peer_addr: peer mac address
 * @param_id: parameter id
 * @param_value: parameter value
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
QDF_STATUS wma_set_peer_param(void *wma_ctx, uint8_t *peer_addr,
			      uint32_t param_id, uint32_t param_value,
			      uint32_t vdev_id)
{
	tp_wma_handle wma_handle = (tp_wma_handle) wma_ctx;
	struct peer_set_params param = {0};
	QDF_STATUS status;

	param.vdev_id = vdev_id;
	param.param_value = param_value;
	param.param_id = param_id;

	status = wmi_set_peer_param_send(wma_handle->wmi_handle,
					 peer_addr,
					 &param);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("vdev_id: %d peer set failed, id %d, val %d",
			 vdev_id, param_id, param_value);
	return status;
}

/**
 * wma_peer_unmap_conf_send - send peer unmap conf cmnd to fw
 * @wma_ctx: wma handle
 * @msg: peer unmap conf params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_peer_unmap_conf_send(tp_wma_handle wma,
				    struct send_peer_unmap_conf_params *msg)
{
	QDF_STATUS qdf_status;

	if (!msg) {
		wma_err("null input params");
		return QDF_STATUS_E_INVAL;
	}

	qdf_status = wmi_unified_peer_unmap_conf_send(
					wma->wmi_handle,
					msg->vdev_id,
					msg->peer_id_cnt,
					msg->peer_id_list);

	if (qdf_status != QDF_STATUS_SUCCESS)
		wma_err("peer_unmap_conf_send failed %d", qdf_status);

	qdf_mem_free(msg->peer_id_list);
	msg->peer_id_list = NULL;

	return qdf_status;
}

/**
 * wma_peer_unmap_conf_cb - send peer unmap conf cmnd to fw
 * @vdev_id: vdev id
 * @peer_id_cnt: no of peer id
 * @peer_id_list: list of peer ids
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_peer_unmap_conf_cb(uint8_t vdev_id,
				  uint32_t peer_id_cnt,
				  uint16_t *peer_id_list)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	QDF_STATUS qdf_status;

	if (!wma) {
		wma_err("peer_id_cnt: %d, null wma_handle", peer_id_cnt);
		return QDF_STATUS_E_INVAL;
	}

	qdf_status = wmi_unified_peer_unmap_conf_send(
						wma->wmi_handle,
						vdev_id, peer_id_cnt,
						peer_id_list);

	if (qdf_status == QDF_STATUS_E_BUSY) {
		QDF_STATUS retcode;
		struct scheduler_msg msg = {0};
		struct send_peer_unmap_conf_params *peer_unmap_conf_req;
		void *mac_ctx = cds_get_context(QDF_MODULE_ID_PE);

		wma_debug("post unmap_conf cmd to MC thread");

		if (!mac_ctx) {
			wma_err("mac_ctx is NULL");
			return QDF_STATUS_E_FAILURE;
		}

		peer_unmap_conf_req = qdf_mem_malloc(sizeof(
					struct send_peer_unmap_conf_params));

		if (!peer_unmap_conf_req)
			return QDF_STATUS_E_NOMEM;

		peer_unmap_conf_req->vdev_id = vdev_id;
		peer_unmap_conf_req->peer_id_cnt = peer_id_cnt;
		peer_unmap_conf_req->peer_id_list =  qdf_mem_malloc(
					sizeof(uint16_t) * peer_id_cnt);
		if (!peer_unmap_conf_req->peer_id_list) {
			qdf_mem_free(peer_unmap_conf_req);
			peer_unmap_conf_req = NULL;
			return QDF_STATUS_E_NOMEM;
		}
		qdf_mem_copy(peer_unmap_conf_req->peer_id_list,
			     peer_id_list, sizeof(uint16_t) * peer_id_cnt);

		msg.type = WMA_SEND_PEER_UNMAP_CONF;
		msg.reserved = 0;
		msg.bodyptr = peer_unmap_conf_req;
		msg.bodyval = 0;

		retcode = wma_post_ctrl_msg(mac_ctx, &msg);
		if (retcode != QDF_STATUS_SUCCESS) {
			wma_err("wma_post_ctrl_msg failed");
			qdf_mem_free(peer_unmap_conf_req->peer_id_list);
			qdf_mem_free(peer_unmap_conf_req);
			return QDF_STATUS_E_FAILURE;
		}
	}

	return qdf_status;
}

bool wma_objmgr_peer_exist(tp_wma_handle wma,
			   uint8_t *peer_addr, uint8_t *peer_vdev_id)
{
	struct wlan_objmgr_peer *peer;

	peer = wlan_objmgr_get_peer_by_mac(wma->psoc, peer_addr,
					   WLAN_LEGACY_WMA_ID);
	if (!peer)
		return false;

	if (peer_vdev_id)
		*peer_vdev_id = wlan_vdev_get_id(wlan_peer_get_vdev(peer));

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);

	return true;
}

/**
 * wma_remove_peer() - remove peer information from host driver and fw
 * @wma: wma handle
 * @mac_addr: peer mac address, to be removed
 * @vdev_id: vdev id
 * @no_fw_peer_delete: If true dont send peer delete to firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wma_remove_peer(tp_wma_handle wma, uint8_t *mac_addr,
			   uint8_t vdev_id, bool no_fw_peer_delete)
{
#define PEER_ALL_TID_BITMASK 0xffffffff
	uint32_t peer_tid_bitmap = PEER_ALL_TID_BITMASK;
	uint8_t *peer_addr = mac_addr;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE] = {0};
	struct peer_flush_params param = {0};
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	uint32_t bitmap = 1 << CDP_PEER_DELETE_NO_SPECIAL;
	bool peer_unmap_conf_support_enabled;
	uint8_t peer_vdev_id;

	if (!wma->interfaces[vdev_id].peer_count) {
		wma_err("Can't remove peer with peer_addr "QDF_MAC_ADDR_FMT" vdevid %d peer_count %d",
			QDF_MAC_ADDR_REF(peer_addr), vdev_id,
			wma->interfaces[vdev_id].peer_count);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (!soc) {
		wma_err("SOC context is NULL");
		QDF_BUG(0);
		return QDF_STATUS_E_INVAL;
	}

	if (!wma_objmgr_peer_exist(wma, peer_addr, &peer_vdev_id)) {
		wma_err("peer doesn't exist peer_addr "QDF_MAC_ADDR_FMT" vdevid %d peer_count %d",
			 QDF_MAC_ADDR_REF(peer_addr), vdev_id,
			 wma->interfaces[vdev_id].peer_count);
		return QDF_STATUS_E_INVAL;
	}

	if (peer_vdev_id != vdev_id) {
		wma_err("peer "QDF_MAC_ADDR_FMT" is on vdev id %d but delete req on vdevid %d peer_count %d",
			 QDF_MAC_ADDR_REF(peer_addr), peer_vdev_id, vdev_id,
			 wma->interfaces[vdev_id].peer_count);
		return QDF_STATUS_E_INVAL;
	}
	peer_unmap_conf_support_enabled =
				cdp_cfg_get_peer_unmap_conf_support(soc);

	cdp_peer_teardown(soc, vdev_id, peer_addr);

	if (no_fw_peer_delete)
		goto peer_detach;

	/* Flush all TIDs except MGMT TID for this peer in Target */
	peer_tid_bitmap &= ~(0x1 << WMI_MGMT_TID);
	param.peer_tid_bitmap = peer_tid_bitmap;
	param.vdev_id = vdev_id;
	if (!wmi_service_enabled(wma->wmi_handle,
				 wmi_service_peer_delete_no_peer_flush_tids_cmd))
		wmi_unified_peer_flush_tids_send(wma->wmi_handle, mac_addr,
						 &param);

	/* peer->ref_cnt is not visible in WMA */
	wlan_roam_debug_log(vdev_id, DEBUG_PEER_DELETE_SEND,
			    DEBUG_INVALID_PEER_ID, peer_addr, NULL,
			    0, 0);
	qdf_status = wmi_unified_peer_delete_send(wma->wmi_handle, peer_addr,
						  vdev_id);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		wma_err("Peer delete could not be sent to firmware %d",
			qdf_status);
		/* Clear default bit and set to NOT_START_UNMAP */
		bitmap = 1 << CDP_PEER_DO_NOT_START_UNMAP_TIMER;
		qdf_status = QDF_STATUS_E_FAILURE;
	}

peer_detach:
	wma_debug("vdevid %d is detaching with peer_addr "QDF_MAC_ADDR_FMT" peer_count %d",
		vdev_id, QDF_MAC_ADDR_REF(peer_addr),
		wma->interfaces[vdev_id].peer_count);
	/* Copy peer mac to find and delete objmgr peer */
	qdf_mem_copy(peer_mac, peer_addr, QDF_MAC_ADDR_SIZE);
	if (no_fw_peer_delete &&
	    is_cdp_peer_detach_force_delete_supported(soc)) {
		if (!peer_unmap_conf_support_enabled) {
			wma_debug("LFR3: trigger force delete for peer "QDF_MAC_ADDR_FMT,
				 QDF_MAC_ADDR_REF(peer_addr));
			cdp_peer_detach_force_delete(soc, vdev_id, peer_addr);
		} else {
			cdp_peer_delete_sync(soc, vdev_id, peer_addr,
					     wma_peer_unmap_conf_cb,
					     bitmap);
		}
	} else {
		if (no_fw_peer_delete)
			wma_debug("LFR3: Delete the peer "QDF_MAC_ADDR_FMT,
				  QDF_MAC_ADDR_REF(peer_addr));

		if (peer_unmap_conf_support_enabled)
			cdp_peer_delete_sync(soc, vdev_id, peer_addr,
					     wma_peer_unmap_conf_cb,
					     bitmap);
		else
			cdp_peer_delete(soc, vdev_id, peer_addr, bitmap);
	}

	wma_remove_objmgr_peer(wma, wma->interfaces[vdev_id].vdev, peer_mac);

	wma->interfaces[vdev_id].peer_count--;
#undef PEER_ALL_TID_BITMASK

	return qdf_status;
}

/**
 * wma_get_peer_type() - Determine the type of peer(eg. STA/AP) and return it
 * @wma: wma handle
 * @vdev_id: vdev id
 * @peer_addr: peer mac address
 * @wma_peer_type: wma peer type
 *
 * Return: Peer type
 */
static int wma_get_obj_mgr_peer_type(tp_wma_handle wma, uint8_t vdev_id,
				     uint8_t *peer_addr, uint32_t wma_peer_type)

{
	uint32_t obj_peer_type = 0;
	struct wlan_objmgr_vdev *vdev;
	uint8_t *addr;

	vdev = wma->interfaces[vdev_id].vdev;
	if (!vdev) {
		wma_err("Couldnt find vdev for VDEV_%d", vdev_id);
		return obj_peer_type;
	}
	addr = wlan_vdev_mlme_get_macaddr(vdev);

	if (wma_peer_type == WMI_PEER_TYPE_TDLS)
		return WLAN_PEER_TDLS;

	if (!qdf_mem_cmp(addr, peer_addr, QDF_MAC_ADDR_SIZE)) {
		obj_peer_type = WLAN_PEER_SELF;
	} else if (wma->interfaces[vdev_id].type == WMI_VDEV_TYPE_STA) {
		if (wma->interfaces[vdev_id].sub_type ==
					WMI_UNIFIED_VDEV_SUBTYPE_P2P_CLIENT)
			obj_peer_type = WLAN_PEER_P2P_GO;
		else
			obj_peer_type = WLAN_PEER_AP;
	} else if (wma->interfaces[vdev_id].type == WMI_VDEV_TYPE_AP) {
			obj_peer_type = WLAN_PEER_STA;
	} else if (wma->interfaces[vdev_id].type == WMI_VDEV_TYPE_IBSS) {
		obj_peer_type = WLAN_PEER_IBSS;
	} else if (wma->interfaces[vdev_id].type == WMI_VDEV_TYPE_NDI) {
		obj_peer_type = WLAN_PEER_NDP;
	} else {
		wma_err("Couldnt find peertype for type %d and sub type %d",
			 wma->interfaces[vdev_id].type,
			 wma->interfaces[vdev_id].sub_type);
	}

	return obj_peer_type;

}

/**
 * wma_create_objmgr_peer() - create objmgr peer information in host driver
 * @wma: wma handle
 * @vdev_id: vdev id
 * @peer_addr: peer mac address
 * @wma_peer_type: peer type
 *
 * Return: objmgr peer pointer
 */

static struct wlan_objmgr_peer *wma_create_objmgr_peer(tp_wma_handle wma,
						       uint8_t vdev_id,
						       uint8_t *peer_addr,
						       uint32_t wma_peer_type)
{
	uint32_t obj_peer_type = 0;
	struct wlan_objmgr_peer *obj_peer = NULL;
	struct wlan_objmgr_vdev *obj_vdev = NULL;
	struct wlan_objmgr_psoc *psoc = wma->psoc;
	uint8_t peer_vdev_id;

	/*
	 * Check if peer with same MAC exist on any Vdev, If so avoid
	 * adding this peer.
	 */
	if (wma_objmgr_peer_exist(wma, peer_addr, &peer_vdev_id)) {
		wma_info("Peer "QDF_MAC_ADDR_FMT" already exist on vdev %d, current vdev %d",
			  QDF_MAC_ADDR_REF(peer_addr), peer_vdev_id, vdev_id);
		return NULL;
	}

	obj_peer_type = wma_get_obj_mgr_peer_type(wma, vdev_id, peer_addr,
						  wma_peer_type);
	if (!obj_peer_type) {
		wma_err("Invalid obj peer type. Unable to create peer %d",
			obj_peer_type);
		return NULL;
	}

	/* Create obj_mgr peer */
	obj_vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_LEGACY_WMA_ID);

	if (!obj_vdev) {
		wma_err("Invalid obj vdev. Unable to create peer %d",
			obj_peer_type);
		return NULL;
	}

	obj_peer = wlan_objmgr_peer_obj_create(obj_vdev, obj_peer_type,
						peer_addr);
	wlan_objmgr_vdev_release_ref(obj_vdev, WLAN_LEGACY_WMA_ID);

	return obj_peer;

}

/**
 * wma_increment_peer_count() - Increment the vdev peer
 * count
 * @wma: wma handle
 * @vdev_id: vdev id
 *
 * Return: None
 */
static void
wma_increment_peer_count(tp_wma_handle wma, uint8_t vdev_id)
{
	wma->interfaces[vdev_id].peer_count++;
}

/**
 * wma_add_peer() - send peer create command to fw
 * @wma: wma handle
 * @peer_addr: peer mac addr
 * @peer_type: peer type
 * @vdev_id: vdev id
 *
 * Return: QDF status
 */
static
QDF_STATUS wma_add_peer(tp_wma_handle wma,
			uint8_t peer_addr[QDF_MAC_ADDR_SIZE],
			uint32_t peer_type, uint8_t vdev_id)
{
	struct peer_create_params param = {0};
	void *dp_soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_objmgr_psoc *psoc = wma->psoc;
	target_resource_config *wlan_res_cfg;
	struct wlan_objmgr_peer *obj_peer = NULL;
	QDF_STATUS status;

	if (!psoc) {
		wma_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	wlan_res_cfg = lmac_get_tgt_res_cfg(psoc);
	if (!wlan_res_cfg) {
		wma_err("psoc target res cfg is null");
		return QDF_STATUS_E_INVAL;
	}

	if (wma->interfaces[vdev_id].peer_count >=
	    wlan_res_cfg->num_peers) {
		wma_err("the peer count exceeds the limit %d",
			 wma->interfaces[vdev_id].peer_count);
		return QDF_STATUS_E_FAILURE;
	}

	if (!dp_soc)
		return QDF_STATUS_E_FAILURE;

	if (qdf_is_macaddr_group((struct qdf_mac_addr *)peer_addr) ||
	    qdf_is_macaddr_zero((struct qdf_mac_addr *)peer_addr)) {
		wma_err("Invalid peer address received reject it");
		return QDF_STATUS_E_FAILURE;
	}

	obj_peer = wma_create_objmgr_peer(wma, vdev_id, peer_addr, peer_type);
	if (!obj_peer)
		return QDF_STATUS_E_FAILURE;

	/* The peer object should be created before sending the WMI peer
	 * create command to firmware. This is to prevent a race condition
	 * where the HTT peer map event is received before the peer object
	 * is created in the data path
	 */
	status = cdp_peer_create(dp_soc, vdev_id, peer_addr);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Unable to attach peer "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peer_addr));
		wlan_objmgr_peer_obj_delete(obj_peer);
		return QDF_STATUS_E_FAILURE;
	}

	if (peer_type == WMI_PEER_TYPE_TDLS)
		cdp_peer_set_peer_as_tdls(dp_soc, vdev_id, peer_addr, true);

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(wma->psoc, vdev_id)) {
		wma_debug("LFR3: Created peer "QDF_MAC_ADDR_FMT" vdev_id %d, peer_count %d",
			 QDF_MAC_ADDR_REF(peer_addr), vdev_id,
			 wma->interfaces[vdev_id].peer_count + 1);
		return QDF_STATUS_SUCCESS;
	}
	param.peer_addr = peer_addr;
	param.peer_type = peer_type;
	param.vdev_id = vdev_id;
	if (wmi_unified_peer_create_send(wma->wmi_handle,
					 &param) != QDF_STATUS_SUCCESS) {
		wma_err("Unable to create peer in Target");
		if (cdp_cfg_get_peer_unmap_conf_support(dp_soc))
			cdp_peer_delete_sync(
				dp_soc, vdev_id, peer_addr,
				wma_peer_unmap_conf_cb,
				1 << CDP_PEER_DO_NOT_START_UNMAP_TIMER);
		else
			cdp_peer_delete(
				dp_soc, vdev_id, peer_addr,
				1 << CDP_PEER_DO_NOT_START_UNMAP_TIMER);
		wlan_objmgr_peer_obj_delete(obj_peer);

		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("Created peer peer_addr "QDF_MAC_ADDR_FMT" vdev_id %d, peer_count - %d",
		  QDF_MAC_ADDR_REF(peer_addr), vdev_id,
		  wma->interfaces[vdev_id].peer_count + 1);

	wlan_roam_debug_log(vdev_id, DEBUG_PEER_CREATE_SEND,
			    DEBUG_INVALID_PEER_ID, peer_addr, NULL, 0, 0);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wma_create_peer(tp_wma_handle wma,
			   uint8_t peer_addr[QDF_MAC_ADDR_SIZE],
			   uint32_t peer_type, uint8_t vdev_id)
{
	void *dp_soc = cds_get_context(QDF_MODULE_ID_SOC);
	QDF_STATUS status;

	if (!dp_soc)
		return QDF_STATUS_E_FAILURE;

	status = wma_add_peer(wma, peer_addr, peer_type, vdev_id);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	wma_increment_peer_count(wma, vdev_id);
	cdp_peer_setup(dp_soc, vdev_id, peer_addr);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_create_sta_mode_bss_peer() - send peer create command to fw
 * and start peer create response timer
 * @wma: wma handle
 * @peer_addr: peer mac address
 * @peer_type: peer type
 * @vdev_id: vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wma_create_sta_mode_bss_peer(tp_wma_handle wma,
			     uint8_t peer_addr[QDF_MAC_ADDR_SIZE],
			     uint32_t peer_type, uint8_t vdev_id)
{
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	struct wma_target_req *msg = NULL;
	struct peer_create_rsp_params *peer_create_rsp = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	bool is_tgt_peer_conf_supported = false;

	if (!mac) {
		wma_err("vdev%d: Mac context is null", vdev_id);
		return status;
	}

	/*
	 * If fw doesn't advertise peer create confirm event support,
	 * use the legacy peer create API
	 */
	is_tgt_peer_conf_supported =
		wlan_psoc_nif_fw_ext_cap_get(wma->psoc,
					     WLAN_SOC_F_PEER_CREATE_RESP);
	if (!is_tgt_peer_conf_supported) {
		status = wma_create_peer(wma, peer_addr, peer_type, vdev_id);
		goto end;
	}

	peer_create_rsp = qdf_mem_malloc(sizeof(*peer_create_rsp));
	if (!peer_create_rsp)
		goto end;

	wma_acquire_wakelock(&wma->wmi_cmd_rsp_wake_lock,
			     WMA_PEER_CREATE_RESPONSE_TIMEOUT);

	status = wma_add_peer(wma, peer_addr, peer_type, vdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_release_wakelock(&wma->wmi_cmd_rsp_wake_lock);
		goto end;
	}

	wma_increment_peer_count(wma, vdev_id);
	qdf_mem_copy(peer_create_rsp->peer_mac.bytes, peer_addr,
		     QDF_MAC_ADDR_SIZE);

	msg = wma_fill_hold_req(wma, vdev_id, WMA_PEER_CREATE_REQ,
				WMA_PEER_CREATE_RESPONSE,
				(void *)peer_create_rsp,
				WMA_PEER_CREATE_RESPONSE_TIMEOUT);
	if (!msg) {
		wma_err("vdev:%d failed to fill peer create req", vdev_id);
		wma_remove_req(wma, vdev_id, WMA_PEER_CREATE_RESPONSE);
		wma_remove_peer(wma, peer_addr, vdev_id, false);
		wma_release_wakelock(&wma->wmi_cmd_rsp_wake_lock);
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	return status;

end:
	lim_post_join_set_link_state_callback(mac, vdev_id, status);
	qdf_mem_free(peer_create_rsp);

	return status;
}

/**
 * wma_remove_bss_peer() - remove BSS peer
 * @wma: pointer to WMA handle
 * @vdev_id: vdev id on which delete BSS request was received
 * @vdev_stop_resp: pointer to Delete BSS response
 *
 * This function is called on receiving vdev stop response from FW or
 * vdev stop response timeout. In case of NDI, use vdev's self MAC
 * for removing the peer. In case of STA/SAP use bssid passed as part of
 * delete STA parameter.
 *
 * Return: 0 on success, ERROR code on failure
 */
static int wma_remove_bss_peer(tp_wma_handle wma, uint32_t vdev_id,
			       struct del_bss_resp *vdev_stop_resp,
			       uint8_t type)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t *mac_addr = NULL;
	struct wma_target_req *del_req;
	int ret_value = 0;
	QDF_STATUS qdf_status;
	struct qdf_mac_addr bssid;

	if (WMA_IS_VDEV_IN_NDI_MODE(wma->interfaces, vdev_id)) {
		mac_addr = cdp_get_vdev_mac_addr(soc, vdev_id);
		if (!mac_addr) {
			wma_err("mac_addr is NULL for vdev_id = %d", vdev_id);
			return -EINVAL;
		}
	} else {
		qdf_status = mlme_get_vdev_bss_peer_mac_addr(
				wma->interfaces[vdev_id].vdev,
				&bssid);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			wma_err("Failed to get bssid for vdev_id: %d", vdev_id);
			return -EINVAL;
		}
		mac_addr = bssid.bytes;
	}

	qdf_status = wma_remove_peer(wma, mac_addr, vdev_id, false);

	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		wma_err("wma_remove_peer failed vdev_id:%d", vdev_id);
		return -EINVAL;
	}

	if (cds_is_driver_recovering())
		return -EINVAL;

	if (wmi_service_enabled(wma->wmi_handle,
				wmi_service_sync_delete_cmds)) {
		wma_debug("Wait for the peer delete. vdev_id %d", vdev_id);
		del_req = wma_fill_hold_req(wma, vdev_id,
					    WMA_DELETE_STA_REQ,
					    type,
					    vdev_stop_resp,
					    WMA_DELETE_STA_TIMEOUT);
		if (!del_req) {
			wma_err("Failed to allocate request. vdev_id %d", vdev_id);
			vdev_stop_resp->status = QDF_STATUS_E_NOMEM;
			ret_value = -EINVAL;
		}
	}

	return ret_value;
}

#ifdef FEATURE_WLAN_APF
/*
 * get_fw_active_apf_mode() - convert HDD APF mode to FW configurable APF
 * mode
 * @mode: APF mode maintained in HDD
 *
 * Return: FW configurable BP mode
 */
static enum wmi_host_active_apf_mode
get_fw_active_apf_mode(enum active_apf_mode mode)
{
	switch (mode) {
	case ACTIVE_APF_DISABLED:
		return WMI_HOST_ACTIVE_APF_DISABLED;
	case ACTIVE_APF_ENABLED:
		return WMI_HOST_ACTIVE_APF_ENABLED;
	case ACTIVE_APF_ADAPTIVE:
		return WMI_HOST_ACTIVE_APF_ADAPTIVE;
	default:
		wma_err("Invalid Active APF Mode %d; Using 'disabled'", mode);
		return WMI_HOST_ACTIVE_APF_DISABLED;
	}
}

/**
 * wma_config_active_apf_mode() - Config active APF mode in FW
 * @wma: the WMA handle
 * @vdev_id: the Id of the vdev for which the configuration should be applied
 *
 * Return: QDF status
 */
static QDF_STATUS wma_config_active_apf_mode(t_wma_handle *wma, uint8_t vdev_id)
{
	enum wmi_host_active_apf_mode uc_mode, mcbc_mode;

	uc_mode = get_fw_active_apf_mode(wma->active_uc_apf_mode);
	mcbc_mode = get_fw_active_apf_mode(wma->active_mc_bc_apf_mode);

	wma_debug("Configuring Active APF Mode UC:%d MC/BC:%d for vdev %u",
		 uc_mode, mcbc_mode, vdev_id);

	return wmi_unified_set_active_apf_mode_cmd(wma->wmi_handle, vdev_id,
						   uc_mode, mcbc_mode);
}
#else /* FEATURE_WLAN_APF */
static QDF_STATUS wma_config_active_apf_mode(t_wma_handle *wma, uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_WLAN_APF */

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/**
 * wma_check_and_find_mcc_ap() - finds if device is operating AP
 * in MCC mode or not
 * @wma: wma handle.
 * @vdev_id: vdev ID of device for which MCC has to be checked
 *
 * This function internally calls wma_find_mcc_ap finds if
 * device is operating AP in MCC mode or not
 *
 * Return: none
 */
static void
wma_check_and_find_mcc_ap(tp_wma_handle wma, uint8_t vdev_id)
{
	struct mac_context *mac_ctx = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac_ctx) {
		wma_err("Failed to get mac_ctx");
		return;
	}
	if (mac_ctx->sap.sap_channel_avoidance)
		wma_find_mcc_ap(wma, vdev_id, false);
}
#else
static inline void
wma_check_and_find_mcc_ap(tp_wma_handle wma, uint8_t vdev_id)
{}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

void wma_send_del_bss_response(tp_wma_handle wma, struct del_bss_resp *resp)
{
	struct wma_txrx_node *iface;
	struct beacon_info *bcn;
	uint8_t vdev_id;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!resp) {
		wma_err("req is NULL");
		return;
	}

	vdev_id = resp->vdev_id;
	iface = &wma->interfaces[vdev_id];

	if (!iface->vdev) {
		wma_err("vdev id %d iface->vdev is NULL", vdev_id);
		if (resp)
			qdf_mem_free(resp);
		return;
	}

	cdp_fc_vdev_flush(soc, vdev_id);
	wma_debug("vdev_id: %d, un-pausing tx_ll_queue for VDEV_STOP rsp",
		 vdev_id);
	cdp_fc_vdev_unpause(soc, vdev_id, OL_TXQ_PAUSE_REASON_VDEV_STOP, 0);
	wma_vdev_clear_pause_bit(vdev_id, PAUSE_TYPE_HOST);
	qdf_atomic_set(&iface->bss_status, WMA_BSS_STATUS_STOPPED);
	wma_debug("(type %d subtype %d) BSS is stopped",
		 iface->type, iface->sub_type);

	bcn = wma->interfaces[vdev_id].beacon;
	if (bcn) {
		wma_debug("Freeing beacon struct %pK, template memory %pK",
			 bcn, bcn->buf);
		if (bcn->dma_mapped)
			qdf_nbuf_unmap_single(wma->qdf_dev, bcn->buf,
					  QDF_DMA_TO_DEVICE);
		qdf_nbuf_free(bcn->buf);
		qdf_mem_free(bcn);
		wma->interfaces[vdev_id].beacon = NULL;
	}

	/* Timeout status means its WMA generated DEL BSS REQ when ADD
	 * BSS REQ was timed out to stop the VDEV in this case no need
	 * to send response to UMAC
	 */
	if (resp->status == QDF_STATUS_FW_MSG_TIMEDOUT) {
		qdf_mem_free(resp);
		wma_err("DEL BSS from ADD BSS timeout do not send resp to UMAC (vdev id %x)",
			vdev_id);
	} else {
		resp->status = QDF_STATUS_SUCCESS;
		wma_send_msg_high_priority(wma, WMA_DELETE_BSS_RSP,
					   (void *)resp, 0);
	}

	if (iface->del_staself_req && iface->is_del_sta_defered) {
		iface->is_del_sta_defered = false;
		wma_nofl_alert("scheduling defered deletion (vdev id %x)",
			      vdev_id);
		wma_vdev_detach(iface->del_staself_req);
	}
}

void wma_send_vdev_down(tp_wma_handle wma, struct del_bss_resp *resp)
{
	uint8_t vdev_id;
	struct wma_txrx_node *iface = &wma->interfaces[resp->vdev_id];
	uint32_t vdev_stop_type;
	QDF_STATUS status;

	if (!resp) {
		wma_err("req is NULL");
		return;
	}

	vdev_id = resp->vdev_id;
	status = mlme_get_vdev_stop_type(iface->vdev, &vdev_stop_type);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to get vdev stop type");
		qdf_mem_free(resp);
		return;
	}

	if (vdev_stop_type != WMA_DELETE_BSS_HO_FAIL_REQ) {
		if (wma_send_vdev_down_to_fw(wma, vdev_id) !=
		    QDF_STATUS_SUCCESS)
			wma_err("Failed to send vdev down cmd: vdev %d", vdev_id);
		else
			wma_check_and_find_mcc_ap(wma, vdev_id);
	}
	wlan_vdev_mlme_sm_deliver_evt(iface->vdev,
				      WLAN_VDEV_SM_EV_DOWN_COMPLETE,
				      sizeof(*resp), resp);
}

/**
 * wma_send_vdev_down_req() - handle vdev down req
 * @wma: wma handle
 * @resp: pointer to vde del bss response
 *
 * Return: none
 */
static void wma_send_vdev_down_req(tp_wma_handle wma,
				   struct del_bss_resp *resp)
{
	struct wma_txrx_node *iface = &wma->interfaces[resp->vdev_id];

	wlan_vdev_mlme_sm_deliver_evt(iface->vdev,
				      WLAN_VDEV_SM_EV_MLME_DOWN_REQ,
				      sizeof(*resp), resp);
}

QDF_STATUS
__wma_handle_vdev_stop_rsp(struct vdev_stop_response *resp_event)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *iface;
	int status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr bssid;
	uint32_t vdev_stop_type;
	struct del_bss_resp *vdev_stop_resp;

	if (!wma) {
		wma_err("wma is null");
		return QDF_STATUS_E_INVAL;
	}

	/* Ignore stop_response in Monitor mode */
	if (cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE)
		return  QDF_STATUS_SUCCESS;

	iface = &wma->interfaces[resp_event->vdev_id];

	/* vdev in stopped state, no more waiting for key */
	iface->is_waiting_for_key = false;

	/*
	 * Reset the rmfEnabled as there might be MGMT action frames
	 * sent on this vdev before the next session is established.
	 */
	if (iface->rmfEnabled) {
		iface->rmfEnabled = 0;
		wma_debug("Reset rmfEnabled for vdev %d",
			 resp_event->vdev_id);
	}

	status = mlme_get_vdev_bss_peer_mac_addr(iface->vdev, &bssid);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to get bssid");
		return QDF_STATUS_E_INVAL;
	}

	status = mlme_get_vdev_stop_type(iface->vdev, &vdev_stop_type);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to get wma req msg type for vdev id %d",
			resp_event->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	vdev_stop_resp = qdf_mem_malloc(sizeof(*vdev_stop_resp));
	if (!vdev_stop_resp)
		return QDF_STATUS_E_NOMEM;

	if (vdev_stop_type == WMA_DELETE_BSS_HO_FAIL_REQ) {
		status = wma_remove_peer(wma, bssid.bytes,
					 resp_event->vdev_id, true);
		if (QDF_IS_STATUS_ERROR(status))
			goto free_params;

		vdev_stop_resp->status = status;
		vdev_stop_resp->vdev_id = resp_event->vdev_id;
		wma_send_vdev_down_req(wma, vdev_stop_resp);
	} else if (vdev_stop_type == WMA_DELETE_BSS_REQ ||
	    vdev_stop_type == WMA_SET_LINK_STATE) {
		uint8_t type;

		/* CCA is required only for sta interface */
		if (iface->type == WMI_VDEV_TYPE_STA)
			wma_get_cca_stats(wma, resp_event->vdev_id);
		if (vdev_stop_type == WMA_DELETE_BSS_REQ)
			type = WMA_DELETE_PEER_RSP;
		else
			type = WMA_SET_LINK_PEER_RSP;

		vdev_stop_resp->vdev_id = resp_event->vdev_id;
		vdev_stop_resp->status = status;
		status = wma_remove_bss_peer(wma, resp_event->vdev_id,
					     vdev_stop_resp, type);
		if (status) {
			wma_err("Del bss failed vdev:%d", resp_event->vdev_id);
			wma_send_vdev_down_req(wma, vdev_stop_resp);
			return status;
		}

		if (wmi_service_enabled(wma->wmi_handle,
					wmi_service_sync_delete_cmds))
			return status;

		wma_send_vdev_down_req(wma, vdev_stop_resp);
	}

	return status;

free_params:
	qdf_mem_free(vdev_stop_resp);
	return status;
}

/**
 * wma_handle_vdev_stop_rsp() - handle vdev stop resp
 * @wma: wma handle
 * @resp_event: fw resp
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wma_handle_vdev_stop_rsp(tp_wma_handle wma,
			 struct vdev_stop_response *resp_event)
{
	struct wma_txrx_node *iface;

	iface = &wma->interfaces[resp_event->vdev_id];
	return wlan_vdev_mlme_sm_deliver_evt(iface->vdev,
					     WLAN_VDEV_SM_EV_STOP_RESP,
					     sizeof(*resp_event), resp_event);
}

QDF_STATUS wma_vdev_stop_resp_handler(struct vdev_mlme_obj *vdev_mlme,
				      struct vdev_stop_response *rsp)
{
	tp_wma_handle wma;
	struct wma_txrx_node *iface = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("wma handle is NULL for VDEV_%d", rsp->vdev_id);
		return status;
	}

	iface = &wma->interfaces[vdev_mlme->vdev->vdev_objmgr.vdev_id];

	if (rsp->vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev_id %d from FW", rsp->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	status = wma_handle_vdev_stop_rsp(wma, rsp);

	return status;
}

void wma_cleanup_vdev(struct wlan_objmgr_vdev *vdev)
{
	tp_wma_handle wma_handle;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct vdev_mlme_obj *vdev_mlme;

	if (!soc) {
		wma_err("SOC handle is NULL");
		return;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		wma_err("WMA context is invalid");
		return;
	}

	if (!wma_handle->interfaces[vdev_id].vdev) {
		wma_err("vdev is NULL");
		return;
	}

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		wma_err("Failed to get vdev mlme obj for vdev id %d", vdev_id);
		return;
	}

	wma_cdp_vdev_detach(soc, wma_handle, vdev_id);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_WMA_ID);
	wma_handle->interfaces[vdev_id].vdev = NULL;
	wma_handle->interfaces[vdev_id].vdev_active = false;
}

QDF_STATUS wma_vdev_self_peer_create(struct vdev_mlme_obj *vdev_mlme)
{
	struct wlan_objmgr_peer *obj_peer;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_objmgr_vdev *vdev = vdev_mlme->vdev;
	tp_wma_handle wma_handle;

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		wma_err("WMA context is invalid");
		return QDF_STATUS_E_FAILURE;
	}

	if (mlme_vdev_uses_self_peer(vdev_mlme->mgmt.generic.type,
				     vdev_mlme->mgmt.generic.subtype)) {
		status = wma_create_peer(wma_handle,
					 vdev->vdev_mlme.macaddr,
					 WMI_PEER_TYPE_DEFAULT,
					 wlan_vdev_get_id(vdev));
		if (QDF_IS_STATUS_ERROR(status))
			wma_err("Failed to create peer %d", status);
	} else if (vdev_mlme->mgmt.generic.type == WMI_VDEV_TYPE_STA) {
		obj_peer = wma_create_objmgr_peer(wma_handle,
						  wlan_vdev_get_id(vdev),
						  vdev->vdev_mlme.macaddr,
						  WMI_PEER_TYPE_DEFAULT);
		if (!obj_peer) {
			wma_err("Failed to create obj mgr peer for self");
			status = QDF_STATUS_E_INVAL;
		}
	}

	return status;
}

QDF_STATUS wma_post_vdev_create_setup(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	uint32_t cfg_val;
	bool mcc_adapt_sch = false;
	QDF_STATUS ret;
	struct mlme_ht_capabilities_info *ht_cap_info;
	u_int8_t vdev_id;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wlan_mlme_qos *qos_aggr;
	struct vdev_mlme_obj *vdev_mlme;
	tp_wma_handle wma_handle;

	if (!mac) {
		wma_err("Failed to get mac");
		return QDF_STATUS_E_FAILURE;
	}

	wma_handle = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma_handle) {
		wma_err("WMA context is invalid");
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc) {
		wma_err("SOC context is invalid");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_vdev_try_get_ref(vdev, WLAN_LEGACY_WMA_ID) !=
		QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_FAILURE;

	vdev_id = wlan_vdev_get_id(vdev);
	wma_handle->interfaces[vdev_id].vdev = vdev;
	wma_handle->interfaces[vdev_id].vdev_active = true;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		wma_err("Failed to get vdev mlme obj!");
		goto end;
	}

	wma_vdev_update_pause_bitmap(vdev_id, 0);

	wma_handle->interfaces[vdev_id].type =
		vdev_mlme->mgmt.generic.type;
	wma_handle->interfaces[vdev_id].sub_type =
		vdev_mlme->mgmt.generic.subtype;

	qos_aggr = &mac->mlme_cfg->qos_mlme_params;
	if (vdev_mlme->mgmt.generic.type == WMI_VDEV_TYPE_STA) {
		wma_set_sta_keep_alive(
				wma_handle, vdev_id,
				SIR_KEEP_ALIVE_NULL_PKT,
				mac->mlme_cfg->sta.sta_keep_alive_period,
				NULL, NULL, NULL);

		/* offload STA SA query related params to fwr */
		if (wmi_service_enabled(wma_handle->wmi_handle,
					wmi_service_sta_pmf_offload)) {
			wma_set_sta_sa_query_param(wma_handle, vdev_id);
		}

		status = wma_set_sw_retry_threshold(
					vdev_id,
					qos_aggr->tx_aggr_sw_retry_threshold,
					WMI_PDEV_PARAM_AGG_SW_RETRY_TH);
		if (QDF_IS_STATUS_ERROR(status))
			wma_err("failed to set sw retry threshold (status = %d)",
				status);

		status = wma_set_sw_retry_threshold(
				vdev_id,
				qos_aggr->tx_non_aggr_sw_retry_threshold,
				WMI_PDEV_PARAM_NON_AGG_SW_RETRY_TH);
		if (QDF_IS_STATUS_ERROR(status))
			wma_err("failed to set sw retry threshold tx non aggr(status = %d)",
				status);

		status = wma_set_sw_retry_threshold_per_ac(wma_handle, vdev_id,
							   qos_aggr);
		if (QDF_IS_STATUS_ERROR(status))
			wma_err("failed to set sw retry threshold per ac(status = %d)",
				 status);
	}

	wma_debug("Setting WMI_VDEV_PARAM_DISCONNECT_TH: %d",
		 mac->mlme_cfg->gen.dropped_pkt_disconnect_thresh);
	status  = wma_vdev_set_param(
			wma_handle->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_DISCONNECT_TH,
			mac->mlme_cfg->gen.dropped_pkt_disconnect_thresh);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("failed to set DISCONNECT_TH(status = %d)", status);

	status = wma_vdev_set_param(
			wma_handle->wmi_handle,
			vdev_id,
			WMI_VDEV_PARAM_MCC_RTSCTS_PROTECTION_ENABLE,
			mac->roam.configParam.mcc_rts_cts_prot_enable);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("failed to set MCC_RTSCTS_PROTECTION_ENABLE(status = %d)",
			status);

	status = wma_vdev_set_param(
			wma_handle->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_MCC_BROADCAST_PROBE_ENABLE,
			mac->roam.configParam.mcc_bcast_prob_resp_enable);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("failed to set MCC_BROADCAST_PROBE_ENABLE(status = %d)",
			status);

	if (wlan_mlme_get_rts_threshold(mac->psoc,
					&cfg_val) ==
					QDF_STATUS_SUCCESS) {
		status = wma_vdev_set_param(wma_handle->wmi_handle,
					    vdev_id,
					    WMI_VDEV_PARAM_RTS_THRESHOLD,
					    cfg_val);
		if (QDF_IS_STATUS_ERROR(status))
			wma_err("failed to set RTS_THRESHOLD(status = %d)",
				status);
	} else {
		wma_err("Fail to get val for rts threshold, leave unchanged");
	}

	if (wlan_mlme_get_frag_threshold(mac->psoc,
					 &cfg_val) ==
					 QDF_STATUS_SUCCESS) {
		wma_vdev_set_param(wma_handle->wmi_handle,
				   vdev_id,
				   WMI_VDEV_PARAM_FRAGMENTATION_THRESHOLD,
				   cfg_val);
	} else {
		wma_err("Fail to get val for frag threshold, leave unchanged");
	}

	ht_cap_info = &mac->mlme_cfg->ht_caps.ht_cap_info;
	status = wma_vdev_set_param(wma_handle->wmi_handle,
				    vdev_id,
				    WMI_VDEV_PARAM_TX_STBC,
				    ht_cap_info->tx_stbc);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("failed to set TX_STBC(status = %d)", status);

	wma_set_vdev_mgmt_rate(wma_handle, vdev_id);
	if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX))
		wma_set_he_txbf_cfg(mac, vdev_id);

	if (wlan_nan_is_beamforming_supported(mac->psoc))
		wma_set_vht_txbf_cfg(mac, vdev_id);

	/* Initialize roaming offload state */
	if (vdev_mlme->mgmt.generic.type == WMI_VDEV_TYPE_STA &&
	    vdev_mlme->mgmt.generic.subtype == 0) {
		/* Pass down enable/disable bcast probe rsp to FW */
		ret = wma_vdev_set_param(
				wma_handle->wmi_handle,
				vdev_id,
				WMI_VDEV_PARAM_ENABLE_BCAST_PROBE_RESPONSE,
				mac->mlme_cfg->oce.enable_bcast_probe_rsp);
		if (QDF_IS_STATUS_ERROR(ret))
			wma_err("Failed to set WMI_VDEV_PARAM_ENABLE_BCAST_PROBE_RESPONSE");

		/* Pass down the FILS max channel guard time to FW */
		ret = wma_vdev_set_param(
				wma_handle->wmi_handle,
				vdev_id,
				WMI_VDEV_PARAM_FILS_MAX_CHANNEL_GUARD_TIME,
				mac->mlme_cfg->sta.fils_max_chan_guard_time);
		if (QDF_IS_STATUS_ERROR(ret))
			wma_err("Failed to set WMI_VDEV_PARAM_FILS_MAX_CHANNEL_GUARD_TIME");

		/* Pass down the Probe Request tx delay(in ms) to FW */
		ret = wma_vdev_set_param(
				wma_handle->wmi_handle,
				vdev_id,
				WMI_VDEV_PARAM_PROBE_DELAY,
				PROBE_REQ_TX_DELAY);
		if (QDF_IS_STATUS_ERROR(ret))
			wma_err("Failed to set WMI_VDEV_PARAM_PROBE_DELAY");

		/* Pass down the probe request tx time gap(in ms) to FW */
		ret = wma_vdev_set_param(
				wma_handle->wmi_handle,
				vdev_id,
				WMI_VDEV_PARAM_REPEAT_PROBE_TIME,
				PROBE_REQ_TX_TIME_GAP);
		if (QDF_IS_STATUS_ERROR(ret))
			wma_err("Failed to set WMI_VDEV_PARAM_REPEAT_PROBE_TIME");
	}

	if ((vdev_mlme->mgmt.generic.type == WMI_VDEV_TYPE_STA ||
	     vdev_mlme->mgmt.generic.type == WMI_VDEV_TYPE_AP) &&
	     vdev_mlme->mgmt.generic.subtype == 0) {
		wma_vdev_set_param(wma_handle->wmi_handle,
				   vdev_id,
				   WMI_VDEV_PARAM_ENABLE_DISABLE_OCE_FEATURES,
				   mac->mlme_cfg->oce.feature_bitmap);
	}

	/* Initialize BMISS parameters */
	if (vdev_mlme->mgmt.generic.type == WMI_VDEV_TYPE_STA &&
	    vdev_mlme->mgmt.generic.subtype == 0)
		wma_roam_scan_bmiss_cnt(wma_handle,
		mac->mlme_cfg->lfr.roam_bmiss_first_bcnt,
		mac->mlme_cfg->lfr.roam_bmiss_final_bcnt,
		vdev_id);

	if (policy_mgr_get_dynamic_mcc_adaptive_sch(mac->psoc,
						    &mcc_adapt_sch) ==
	    QDF_STATUS_SUCCESS) {
		wma_debug("setting ini value for WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED: %d",
			 mcc_adapt_sch);
		ret =
		wma_set_enable_disable_mcc_adaptive_scheduler(mcc_adapt_sch);
		if (QDF_IS_STATUS_ERROR(ret)) {
			wma_err("Failed to set WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED");
		}
	} else {
		wma_err("Failed to get value for WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, leaving unchanged");
	}

	if (vdev_mlme->mgmt.generic.type == WMI_VDEV_TYPE_STA &&
	    ucfg_pmo_is_apf_enabled(wma_handle->psoc)) {
		ret = wma_config_active_apf_mode(wma_handle,
						 vdev_id);
		if (QDF_IS_STATUS_ERROR(ret))
			wma_err("Failed to configure active APF mode");
	}

	cdp_data_tx_cb_set(soc, vdev_id,
			   wma_data_tx_ack_comp_hdlr,
			   wma_handle);

	return QDF_STATUS_SUCCESS;

end:
	wma_cleanup_vdev(vdev);
	return QDF_STATUS_E_FAILURE;
}

enum mlme_bcn_tx_rate_code wma_get_bcn_rate_code(uint16_t rate)
{
	/* rate in multiples of 100 Kbps */
	switch (rate) {
	case WMA_BEACON_TX_RATE_1_M:
		return MLME_BCN_TX_RATE_CODE_1_M;
	case WMA_BEACON_TX_RATE_2_M:
		return MLME_BCN_TX_RATE_CODE_2_M;
	case WMA_BEACON_TX_RATE_5_5_M:
		return MLME_BCN_TX_RATE_CODE_5_5_M;
	case WMA_BEACON_TX_RATE_11_M:
		return MLME_BCN_TX_RATE_CODE_11M;
	case WMA_BEACON_TX_RATE_6_M:
		return MLME_BCN_TX_RATE_CODE_6_M;
	case WMA_BEACON_TX_RATE_9_M:
		return MLME_BCN_TX_RATE_CODE_9_M;
	case WMA_BEACON_TX_RATE_12_M:
		return MLME_BCN_TX_RATE_CODE_12_M;
	case WMA_BEACON_TX_RATE_18_M:
		return MLME_BCN_TX_RATE_CODE_18_M;
	case WMA_BEACON_TX_RATE_24_M:
		return MLME_BCN_TX_RATE_CODE_24_M;
	case WMA_BEACON_TX_RATE_36_M:
		return MLME_BCN_TX_RATE_CODE_36_M;
	case WMA_BEACON_TX_RATE_48_M:
		return MLME_BCN_TX_RATE_CODE_48_M;
	case WMA_BEACON_TX_RATE_54_M:
		return MLME_BCN_TX_RATE_CODE_54_M;
	default:
		return MLME_BCN_TX_RATE_CODE_1_M;
	}
}

QDF_STATUS wma_vdev_pre_start(uint8_t vdev_id, bool restart)
{
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *intr;
	struct mac_context *mac_ctx =  cds_get_context(QDF_MODULE_ID_PE);
	struct wlan_mlme_nss_chains *ini_cfg;
	struct vdev_mlme_obj *mlme_obj;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_channel *des_chan;
	QDF_STATUS status;
	uint8_t btc_chain_mode;
	struct wlan_mlme_qos *qos_aggr;
	uint8_t amsdu_val;

	if (!wma) {
		wma_err("Invalid wma handle");
		return QDF_STATUS_E_FAILURE;
	}
	if (!mac_ctx) {
		wma_err("Invalid mac context");
		return QDF_STATUS_E_FAILURE;
	}
	intr = wma->interfaces;
	if (!intr) {
		wma_err("Invalid interface");
		return QDF_STATUS_E_FAILURE;
	}
	if (vdev_id >= WLAN_MAX_VDEVS) {
		wma_err("Invalid vdev id");
		return QDF_STATUS_E_INVAL;
	}
	vdev = intr[vdev_id].vdev;
	if (!vdev) {
		wma_err("Invalid vdev");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!mlme_obj) {
		wma_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	des_chan = vdev->vdev_mlme.des_chan;

	ini_cfg = mlme_get_ini_vdev_config(vdev);
	if (!ini_cfg) {
		wma_err("nss chain ini config NULL");
		return QDF_STATUS_E_FAILURE;
	}

	intr[vdev_id].config.gtx_info.gtxRTMask[0] =
		CFG_TGT_DEFAULT_GTX_HT_MASK;
	intr[vdev_id].config.gtx_info.gtxRTMask[1] =
		CFG_TGT_DEFAULT_GTX_VHT_MASK;

	intr[vdev_id].config.gtx_info.gtxUsrcfg =
		mac_ctx->mlme_cfg->sta.tgt_gtx_usr_cfg;

	intr[vdev_id].config.gtx_info.gtxPERThreshold =
		CFG_TGT_DEFAULT_GTX_PER_THRESHOLD;
	intr[vdev_id].config.gtx_info.gtxPERMargin =
		CFG_TGT_DEFAULT_GTX_PER_MARGIN;
	intr[vdev_id].config.gtx_info.gtxTPCstep =
		CFG_TGT_DEFAULT_GTX_TPC_STEP;
	intr[vdev_id].config.gtx_info.gtxTPCMin =
		CFG_TGT_DEFAULT_GTX_TPC_MIN;
	intr[vdev_id].config.gtx_info.gtxBWMask =
		CFG_TGT_DEFAULT_GTX_BW_MASK;
	intr[vdev_id].chan_width = des_chan->ch_width;
	intr[vdev_id].ch_freq = des_chan->ch_freq;
	intr[vdev_id].ch_flagext = des_chan->ch_flagext;

	/*
	 * If the channel has DFS set, flip on radar reporting.
	 *
	 * It may be that this should only be done for hostap operation
	 * as this flag may be interpreted (at some point in the future)
	 * by the firmware as "oh, and please do radar DETECTION."
	 *
	 * If that is ever the case we would insert the decision whether to
	 * enable the firmware flag here.
	 */
	if (QDF_GLOBAL_MONITOR_MODE != cds_get_conparam() &&
	    utils_is_dfs_chan_for_freq(wma->pdev, des_chan->ch_freq))
		mlme_obj->mgmt.generic.disable_hw_ack = true;

	if (mlme_obj->mgmt.rate_info.bcn_tx_rate) {
		wma_debug("beacon tx rate [%u * 100 Kbps]",
			  mlme_obj->mgmt.rate_info.bcn_tx_rate);
		/*
		 * beacon_tx_rate is in multiples of 100 Kbps.
		 * Convert the data rate to hw rate code.
		 */
		mlme_obj->mgmt.rate_info.bcn_tx_rate =
		wma_get_bcn_rate_code(mlme_obj->mgmt.rate_info.bcn_tx_rate);
	}

	if (!restart) {
		wma_debug("vdev_id: %d, unpausing tx_ll_queue at VDEV_START",
			 vdev_id);

		cdp_fc_vdev_unpause(cds_get_context(QDF_MODULE_ID_SOC),
				    vdev_id, 0xffffffff, 0);
		wma_vdev_update_pause_bitmap(vdev_id, 0);
	}

	/* Send the dynamic nss chain params before vdev start to fw */
	if (wma->dynamic_nss_chains_support && !restart)
		wma_vdev_nss_chain_params_send(vdev_id, ini_cfg);

	status = ucfg_coex_psoc_get_btc_chain_mode(wma->psoc, &btc_chain_mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to get btc chain mode");
		return QDF_STATUS_E_FAILURE;
	}

	if (btc_chain_mode != WLAN_COEX_BTC_CHAIN_MODE_UNSETTLED) {
		status = ucfg_coex_send_btc_chain_mode(vdev, btc_chain_mode);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("Failed to send btc chain mode %d",
				btc_chain_mode);
			return QDF_STATUS_E_FAILURE;
		}
	}

	qos_aggr = &mac_ctx->mlme_cfg->qos_mlme_params;
	status = wma_set_tx_rx_aggr_size(vdev_id, qos_aggr->tx_aggregation_size,
					 qos_aggr->rx_aggregation_size,
					 WMI_VDEV_CUSTOM_AGGR_TYPE_AMPDU);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("failed to set aggregation sizes(status = %d)", status);

	status = wlan_mlme_get_max_amsdu_num(wma->psoc, &amsdu_val);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("failed to get amsdu aggr.size(status = %d)", status);
	} else {
		status = wma_set_tx_rx_aggr_size(vdev_id, amsdu_val, amsdu_val,
					WMI_VDEV_CUSTOM_AGGR_TYPE_AMSDU);
		if (QDF_IS_STATUS_ERROR(status))
			wma_err("failed to set amsdu aggr.size(status = %d)",
				status);
	}

	if (mlme_obj->mgmt.generic.type == WMI_VDEV_TYPE_STA) {
		status = wma_set_tx_rx_aggr_size_per_ac(wma, vdev_id, qos_aggr,
					WMI_VDEV_CUSTOM_AGGR_TYPE_AMPDU);

		if (QDF_IS_STATUS_ERROR(status))
			wma_err("failed to set aggr size per ac(status = %d)",
				status);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_peer_assoc_conf_handler() - peer assoc conf handler
 * @handle: wma handle
 * @cmd_param_info: event buffer
 * @len: buffer length
 *
 * Return: 0 for success or error code
 */
int wma_peer_assoc_conf_handler(void *handle, uint8_t *cmd_param_info,
				uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_PEER_ASSOC_CONF_EVENTID_param_tlvs *param_buf;
	wmi_peer_assoc_conf_event_fixed_param *event;
	struct wma_target_req *req_msg;
	uint8_t macaddr[QDF_MAC_ADDR_SIZE];
	int status = 0;

	param_buf = (WMI_PEER_ASSOC_CONF_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid peer assoc conf event buffer");
		return -EINVAL;
	}

	event = param_buf->fixed_param;
	if (!event) {
		wma_err("Invalid peer assoc conf event buffer");
		return -EINVAL;
	}

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&event->peer_macaddr, macaddr);
	wma_debug("peer assoc conf for vdev:%d mac="QDF_MAC_ADDR_FMT,
		 event->vdev_id, QDF_MAC_ADDR_REF(macaddr));

	req_msg = wma_find_req(wma, event->vdev_id,
				    WMA_PEER_ASSOC_CNF_START);

	if (!req_msg) {
		wma_err("Failed to lookup request message for vdev %d",
			event->vdev_id);
		return -EINVAL;
	}

	qdf_mc_timer_stop(&req_msg->event_timeout);

	if (req_msg->msg_type == WMA_ADD_STA_REQ) {
		tpAddStaParams params = (tpAddStaParams)req_msg->user_data;

		if (!params) {
			wma_err("add STA params is NULL for vdev %d",
				 event->vdev_id);
			status = -EINVAL;
			goto free_req_msg;
		}

		/* peer assoc conf event means the cmd succeeds */
		params->status = event->status;
		wma_debug("Send ADD_STA_RSP: statype %d vdev_id %d aid %d bssid "QDF_MAC_ADDR_FMT" status %d",
			 params->staType, params->smesessionId,
			 params->assocId, QDF_MAC_ADDR_REF(params->bssId),
			 params->status);
		wma_send_msg_high_priority(wma, WMA_ADD_STA_RSP,
					   (void *)params, 0);
	} else if (req_msg->msg_type == WMA_ADD_BSS_REQ) {
		wma_send_add_bss_resp(wma, event->vdev_id, event->status);
	} else {
		wma_err("Unhandled request message type: %d", req_msg->msg_type);
	}

free_req_msg:
	qdf_mc_timer_destroy(&req_msg->event_timeout);
	qdf_mem_free(req_msg);

	return status;
}

int wma_peer_create_confirm_handler(void *handle, uint8_t *evt_param_info,
				    uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	wmi_peer_create_conf_event_fixed_param *peer_create_rsp;
	WMI_PEER_CREATE_CONF_EVENTID_param_tlvs *param_buf;
	struct wma_target_req *req_msg = NULL;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	struct peer_create_rsp_params *rsp_data;
	void *dp_soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct qdf_mac_addr peer_mac;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	int ret = -EINVAL;

	param_buf = (WMI_PEER_CREATE_CONF_EVENTID_param_tlvs *)evt_param_info;
	if (!param_buf) {
		wma_err("Invalid peer create conf evt buffer");
		return -EINVAL;
	}

	peer_create_rsp = param_buf->fixed_param;

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&peer_create_rsp->peer_macaddr,
				   peer_mac.bytes);
	if (qdf_is_macaddr_zero(&peer_mac) ||
	    qdf_is_macaddr_broadcast(&peer_mac) ||
	    qdf_is_macaddr_group(&peer_mac)) {
		wma_err("Invalid bssid");
		return -EINVAL;
	}

	wma_debug("vdev:%d Peer create confirm for bssid: " QDF_MAC_ADDR_FMT,
		  peer_create_rsp->vdev_id, QDF_MAC_ADDR_REF(peer_mac.bytes));
	req_msg = wma_find_remove_req_msgtype(wma, peer_create_rsp->vdev_id,
					      WMA_PEER_CREATE_REQ);
	if (!req_msg) {
		wma_err("vdev:%d Failed to lookup peer create request message",
			peer_create_rsp->vdev_id);
		return -EINVAL;
	}

	wma_release_wakelock(&wma->wmi_cmd_rsp_wake_lock);

	rsp_data = (struct peer_create_rsp_params *)req_msg->user_data;

	qdf_mc_timer_stop(&req_msg->event_timeout);
	qdf_mc_timer_destroy(&req_msg->event_timeout);
	qdf_mem_free(rsp_data);
	qdf_mem_free(req_msg);

	if (!peer_create_rsp->status) {
		if (!dp_soc) {
			wma_err("DP SOC context is NULL");
			goto fail;
		}

		cdp_peer_setup(dp_soc, peer_create_rsp->vdev_id,
			       peer_mac.bytes);

		status = QDF_STATUS_SUCCESS;
		ret = 0;
	}

fail:
	if (QDF_IS_STATUS_ERROR(status))
		wma_remove_peer(wma, peer_mac.bytes, peer_create_rsp->vdev_id,
				(peer_create_rsp->status > 0) ? true : false);

	if (mac)
		lim_post_join_set_link_state_callback(mac,
						      peer_create_rsp->vdev_id,
						      status);

	return ret;
}

/**
 * wma_peer_delete_handler() - peer delete response handler
 * @handle: wma handle
 * @cmd_param_info: event buffer
 * @len: buffer length
 *
 * Return: 0 for success or error code
 */
int wma_peer_delete_handler(void *handle, uint8_t *cmd_param_info,
				uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_PEER_DELETE_RESP_EVENTID_param_tlvs *param_buf;
	wmi_peer_delete_cmd_fixed_param *event;
	struct wma_target_req *req_msg;
	tDeleteStaParams *del_sta;
	uint8_t macaddr[QDF_MAC_ADDR_SIZE];
	int status = 0;

	param_buf = (WMI_PEER_DELETE_RESP_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_buf) {
		wma_err("Invalid vdev delete event buffer");
		return -EINVAL;
	}

	event = (wmi_peer_delete_cmd_fixed_param *)param_buf->fixed_param;
	if (!event) {
		wma_err("Invalid vdev delete event buffer");
		return -EINVAL;
	}

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&event->peer_macaddr, macaddr);
	wma_debug("Peer Delete Response, vdev %d Peer "QDF_MAC_ADDR_FMT,
			event->vdev_id, QDF_MAC_ADDR_REF(macaddr));
	wlan_roam_debug_log(event->vdev_id, DEBUG_PEER_DELETE_RESP,
			    DEBUG_INVALID_PEER_ID, macaddr, NULL, 0, 0);
	req_msg = wma_find_remove_req_msgtype(wma, event->vdev_id,
					WMA_DELETE_STA_REQ);
	if (!req_msg) {
		wma_debug("Peer Delete response is not handled");
		return -EINVAL;
	}

	wma_release_wakelock(&wma->wmi_cmd_rsp_wake_lock);

	/* Cleanup timeout handler */
	qdf_mc_timer_stop(&req_msg->event_timeout);
	qdf_mc_timer_destroy(&req_msg->event_timeout);

	if (req_msg->type == WMA_DELETE_STA_RSP_START) {
		del_sta = req_msg->user_data;
		if (del_sta->respReqd) {
			wma_debug("Sending peer del rsp to umac");
			wma_send_msg_high_priority(wma, WMA_DELETE_STA_RSP,
				(void *)del_sta, QDF_STATUS_SUCCESS);
		} else {
			qdf_mem_free(del_sta);
		}
	} else if (req_msg->type == WMA_DEL_P2P_SELF_STA_RSP_START) {
		struct del_sta_self_rsp_params *data;

		data = (struct del_sta_self_rsp_params *)req_msg->user_data;
		wma_debug("Calling vdev detach handler");
		wma_handle_vdev_detach(wma, data->self_sta_param);
		mlme_vdev_self_peer_delete_resp(data->self_sta_param);
		qdf_mem_free(data);
	} else if (req_msg->type == WMA_SET_LINK_PEER_RSP ||
		   req_msg->type == WMA_DELETE_PEER_RSP) {
		wma_send_vdev_down_req(wma, req_msg->user_data);
	}
	qdf_mem_free(req_msg);

	return status;
}

static
void wma_trigger_recovery_assert_on_fw_timeout(uint16_t wma_msg,
					       enum qdf_hang_reason reason)
{
	wma_err("%s timed out, triggering recovery",
		 mac_trace_get_wma_msg_string(wma_msg));
	qdf_trigger_self_recovery(NULL, reason);
}

static inline bool wma_crash_on_fw_timeout(bool crash_enabled)
{
	/* Discard FW timeouts and dont crash during SSR */
	if (cds_is_driver_recovering())
		return false;

	/* Firmware is down send failure response */
	if (cds_is_fw_down())
		return false;

	if (cds_is_driver_unloading())
		return false;

	return crash_enabled;
}

/**
 * wma_hold_req_timer() - wma hold request timeout function
 * @data: target request params
 *
 * Return: none
 */
void wma_hold_req_timer(void *data)
{
	tp_wma_handle wma;
	struct wma_target_req *tgt_req = (struct wma_target_req *)data;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	QDF_STATUS status;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Failed to get wma");
		return;
	}

	status = wma_find_req_on_timer_expiry(wma, tgt_req);

	if (QDF_IS_STATUS_ERROR(status)) {
		/*
		 * if find request failed, then firmware rsp should have
		 * consumed the buffer. Do not free.
		 */
		wma_debug("Failed to lookup request message - %pK", tgt_req);
		return;
	}
	wma_alert("request %d is timed out for vdev_id - %d",
		 tgt_req->msg_type, tgt_req->vdev_id);

	if (tgt_req->msg_type == WMA_ADD_STA_REQ) {
		tpAddStaParams params = (tpAddStaParams) tgt_req->user_data;

		params->status = QDF_STATUS_E_TIMEOUT;
		wma_alert("WMA_ADD_STA_REQ timed out");
		wma_debug("Sending add sta rsp to umac (mac:"QDF_MAC_ADDR_FMT", status:%d)",
			 QDF_MAC_ADDR_REF(params->staMac), params->status);
		if (wma_crash_on_fw_timeout(wma->fw_timeout_crash))
			wma_trigger_recovery_assert_on_fw_timeout(
				WMA_ADD_STA_REQ,
				QDF_AP_STA_CONNECT_REQ_TIMEOUT);
		wma_send_msg_high_priority(wma, WMA_ADD_STA_RSP,
					   (void *)params, 0);
	} else if (tgt_req->msg_type == WMA_ADD_BSS_REQ) {

		wma_alert("WMA_ADD_BSS_REQ timed out");
		wma_debug("Sending add bss rsp to umac (vdev %d, status:%d)",
			 tgt_req->vdev_id, QDF_STATUS_E_TIMEOUT);

		if (wma_crash_on_fw_timeout(wma->fw_timeout_crash))
			wma_trigger_recovery_assert_on_fw_timeout(
				WMA_ADD_BSS_REQ,
				QDF_STA_AP_CONNECT_REQ_TIMEOUT);

		wma_send_add_bss_resp(wma, tgt_req->vdev_id,
				      QDF_STATUS_E_TIMEOUT);
	} else if ((tgt_req->msg_type == WMA_DELETE_STA_REQ) &&
		(tgt_req->type == WMA_DELETE_STA_RSP_START)) {
		tpDeleteStaParams params =
				(tpDeleteStaParams) tgt_req->user_data;
		params->status = QDF_STATUS_E_TIMEOUT;
		wma_err("WMA_DEL_STA_REQ timed out");
		wma_debug("Sending del sta rsp to umac (mac:"QDF_MAC_ADDR_FMT", status:%d)",
			 QDF_MAC_ADDR_REF(params->staMac), params->status);

		if (wma_crash_on_fw_timeout(wma->fw_timeout_crash))
			wma_trigger_recovery_assert_on_fw_timeout(
				WMA_DELETE_STA_REQ,
				QDF_PEER_DELETION_TIMEDOUT);
		wma_send_msg_high_priority(wma, WMA_DELETE_STA_RSP,
					   (void *)params, 0);
	} else if ((tgt_req->msg_type == WMA_DELETE_STA_REQ) &&
		(tgt_req->type == WMA_DEL_P2P_SELF_STA_RSP_START)) {
		struct del_sta_self_rsp_params *del_sta;

		del_sta = (struct del_sta_self_rsp_params *)tgt_req->user_data;

		del_sta->self_sta_param->status = QDF_STATUS_E_TIMEOUT;
		wma_alert("wma delete sta p2p request timed out");

		if (wma_crash_on_fw_timeout(wma->fw_timeout_crash))
			wma_trigger_recovery_assert_on_fw_timeout(
				WMA_DELETE_STA_REQ,
				QDF_PEER_DELETION_TIMEDOUT);
		wma_handle_vdev_detach(wma, del_sta->self_sta_param);
		mlme_vdev_self_peer_delete_resp(del_sta->self_sta_param);
		qdf_mem_free(tgt_req->user_data);
	} else if ((tgt_req->msg_type == WMA_DELETE_STA_REQ) &&
		   (tgt_req->type == WMA_SET_LINK_PEER_RSP ||
		    tgt_req->type == WMA_DELETE_PEER_RSP)) {
		struct del_bss_resp *params =
			(struct del_bss_resp *)tgt_req->user_data;

		params->status = QDF_STATUS_E_TIMEOUT;
		wma_err("wma delete peer for del bss req timed out");

		if (wma_crash_on_fw_timeout(wma->fw_timeout_crash))
			wma_trigger_recovery_assert_on_fw_timeout(
				WMA_DELETE_STA_REQ,
				QDF_PEER_DELETION_TIMEDOUT);
		wma_send_vdev_down_req(wma, params);
	} else if ((tgt_req->msg_type == SIR_HAL_PDEV_SET_HW_MODE) &&
			(tgt_req->type == WMA_PDEV_SET_HW_MODE_RESP)) {
		struct sir_set_hw_mode_resp *params =
			qdf_mem_malloc(sizeof(*params));

		wma_err("set hw mode req timed out");

		if (wma_crash_on_fw_timeout(wma->fw_timeout_crash))
			wma_trigger_recovery_assert_on_fw_timeout(
			  SIR_HAL_PDEV_SET_HW_MODE,
			  QDF_MAC_HW_MODE_CHANGE_TIMEOUT);
		if (!params)
			goto timer_destroy;

		params->status = SET_HW_MODE_STATUS_ECANCELED;
		params->cfgd_hw_mode_index = 0;
		params->num_vdev_mac_entries = 0;
		wma_send_msg_high_priority(wma, SIR_HAL_PDEV_SET_HW_MODE_RESP,
					   params, 0);
	} else if ((tgt_req->msg_type == SIR_HAL_PDEV_DUAL_MAC_CFG_REQ) &&
			(tgt_req->type == WMA_PDEV_MAC_CFG_RESP)) {
		struct sir_dual_mac_config_resp *resp =
						qdf_mem_malloc(sizeof(*resp));

		wma_err("set dual mac config timeout");
		if (wma_crash_on_fw_timeout(wma->fw_timeout_crash))
			wma_trigger_recovery_assert_on_fw_timeout(
				SIR_HAL_PDEV_DUAL_MAC_CFG_REQ,
				QDF_MAC_HW_MODE_CONFIG_TIMEOUT);
		if (!resp)
			goto timer_destroy;

		resp->status = SET_HW_MODE_STATUS_ECANCELED;
		wma_send_msg_high_priority(wma, SIR_HAL_PDEV_MAC_CFG_RESP,
					   resp, 0);
	} else if ((tgt_req->msg_type == WMA_PEER_CREATE_REQ) &&
		   (tgt_req->type == WMA_PEER_CREATE_RESPONSE)) {
		struct peer_create_rsp_params *peer_create_rsp;
		struct qdf_mac_addr *peer_mac;

		if (wma_crash_on_fw_timeout(wma->fw_timeout_crash))
			wma_trigger_recovery_assert_on_fw_timeout(
				WMA_PEER_CREATE_RESPONSE,
				WMA_PEER_CREATE_RESPONSE_TIMEOUT);

		peer_create_rsp =
			(struct peer_create_rsp_params *)tgt_req->user_data;
		peer_mac = &peer_create_rsp->peer_mac;
		wma_remove_peer(wma, peer_mac->bytes,
				tgt_req->vdev_id, false);
		if (!mac) {
			qdf_mem_free(tgt_req->user_data);
			goto timer_destroy;
		}

		lim_post_join_set_link_state_callback(mac, tgt_req->vdev_id,
						      QDF_STATUS_E_FAILURE);

		qdf_mem_free(tgt_req->user_data);
	} else {
		wma_err("Unhandled timeout for msg_type:%d and type:%d",
				tgt_req->msg_type, tgt_req->type);
		QDF_BUG(0);
	}

timer_destroy:
	qdf_mc_timer_destroy(&tgt_req->event_timeout);
	qdf_mem_free(tgt_req);
}

/**
 * wma_fill_hold_req() - fill wma request
 * @wma: wma handle
 * @msg_type: message type
 * @type: request type
 * @params: request params
 * @timeout: timeout value
 *
 * Return: wma_target_req ptr
 */
struct wma_target_req *wma_fill_hold_req(tp_wma_handle wma,
					 uint8_t vdev_id,
					 uint32_t msg_type, uint8_t type,
					 void *params, uint32_t timeout)
{
	struct wma_target_req *req;
	QDF_STATUS status;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return NULL;

	wma_debug("vdev_id %d msg %d type %d", vdev_id, msg_type, type);
	qdf_spin_lock_bh(&wma->wma_hold_req_q_lock);
	req->vdev_id = vdev_id;
	req->msg_type = msg_type;
	req->type = type;
	req->user_data = params;
	status = qdf_list_insert_back(&wma->wma_hold_req_queue, &req->node);
	if (QDF_STATUS_SUCCESS != status) {
		qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
		wma_err("Failed add request in queue");
		qdf_mem_free(req);
		return NULL;
	}
	qdf_spin_unlock_bh(&wma->wma_hold_req_q_lock);
	qdf_mc_timer_init(&req->event_timeout, QDF_TIMER_TYPE_SW,
			  wma_hold_req_timer, req);
	qdf_mc_timer_start(&req->event_timeout, timeout);
	return req;
}

/**
 * wma_remove_req() - remove request
 * @wma: wma handle
 * @vdev_id: vdev id
 * @type: type
 *
 * Return: none
 */
void wma_remove_req(tp_wma_handle wma, uint8_t vdev_id,
		    uint8_t type)
{
	struct wma_target_req *req_msg;

	wma_debug("Remove req for vdev: %d type: %d", vdev_id, type);
	req_msg = wma_find_req(wma, vdev_id, type);
	if (!req_msg) {
		wma_err("target req not found for vdev: %d type: %d",
			 vdev_id, type);
		return;
	}

	qdf_mc_timer_stop(&req_msg->event_timeout);
	qdf_mc_timer_destroy(&req_msg->event_timeout);
	qdf_mem_free(req_msg);
}

/**
 * wma_vdev_set_bss_params() - BSS set params functions
 * @wma: wma handle
 * @vdev_id: vdev id
 * @beaconInterval: beacon interval
 * @dtimPeriod: DTIM period
 * @shortSlotTimeSupported: short slot time
 * @llbCoexist: llbCoexist
 * @maxTxPower: max tx power
 *
 * Return: none
 */
static void
wma_vdev_set_bss_params(tp_wma_handle wma, int vdev_id,
			tSirMacBeaconInterval beaconInterval,
			uint8_t dtimPeriod, uint8_t shortSlotTimeSupported,
			uint8_t llbCoexist, int8_t maxTxPower)
{
	QDF_STATUS ret;
	uint32_t slot_time;
	struct wma_txrx_node *intr = wma->interfaces;

	/* Beacon Interval setting */
	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_BEACON_INTERVAL,
					      beaconInterval);

	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("failed to set WMI_VDEV_PARAM_BEACON_INTERVAL");
	ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle, vdev_id,
						&intr[vdev_id].config.gtx_info);
	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("failed to set WMI_VDEV_PARAM_DTIM_PERIOD");
	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_DTIM_PERIOD,
					      dtimPeriod);
	intr[vdev_id].dtimPeriod = dtimPeriod;
	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("failed to set WMI_VDEV_PARAM_DTIM_PERIOD");

	if (!wlan_reg_is_ext_tpc_supported(wma->psoc)) {
		if (!maxTxPower)
			wma_warn("Setting Tx power limit to 0");

		wma_nofl_debug("TXP[W][set_bss_params]: %d", maxTxPower);

		if (maxTxPower != INVALID_TXPOWER) {
			ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
						 WMI_VDEV_PARAM_TX_PWRLIMIT,
						 maxTxPower);
			if (QDF_IS_STATUS_ERROR(ret))
				wma_err("failed to set VDEV_PARAM_TX_PWRLMT");
			else
				mlme_set_max_reg_power(intr[vdev_id].vdev,
						       maxTxPower);
		} else {
			wma_err("Invalid max Tx power");
		}
	}
	/* Slot time */
	if (shortSlotTimeSupported)
		slot_time = WMI_VDEV_SLOT_TIME_SHORT;
	else
		slot_time = WMI_VDEV_SLOT_TIME_LONG;

	ret = wma_vdev_set_param(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_SLOT_TIME,
					      slot_time);
	if (QDF_IS_STATUS_ERROR(ret))
		wma_err("failed to set WMI_VDEV_PARAM_SLOT_TIME");

	/* Initialize protection mode in case of coexistence */
	wma_update_protection_mode(wma, vdev_id, llbCoexist);

}

#ifdef WLAN_FEATURE_11W
static void wma_set_mgmt_frame_protection(tp_wma_handle wma)
{
	struct pdev_params param = {0};
	QDF_STATUS ret;

	/*
	 * when 802.11w PMF is enabled for hw encr/decr
	 * use hw MFP Qos bits 0x10
	 */
	param.param_id = WMI_PDEV_PARAM_PMF_QOS;
	param.param_value = true;
	ret = wmi_unified_pdev_param_send(wma->wmi_handle,
					 &param, WMA_WILDCARD_PDEV_ID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wma_err("Failed to set QOS MFP/PMF (%d)", ret);
	} else {
		wma_debug("QOS MFP/PMF set");
	}
}

/**
 * wma_set_peer_pmf_status() - Get the peer and update PMF capability of it
 * @wma: wma handle
 * @peer_mac: peer mac addr
 * @is_pmf_enabled: Carries the status whether PMF is enabled or not
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wma_set_peer_pmf_status(tp_wma_handle wma, uint8_t *peer_mac,
			bool is_pmf_enabled)
{
	struct wlan_objmgr_peer *peer;

	peer = wlan_objmgr_get_peer(wma->psoc,
				    wlan_objmgr_pdev_get_pdev_id(wma->pdev),
				    peer_mac, WLAN_LEGACY_WMA_ID);
	if (!peer) {
		wma_err("Peer of peer_mac "QDF_MAC_ADDR_FMT" not found",
			QDF_MAC_ADDR_REF(peer_mac));
		return QDF_STATUS_E_INVAL;
	}
	mlme_set_peer_pmf_status(peer, is_pmf_enabled);
	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_WMA_ID);
	wma_debug("set is_pmf_enabled %d for "QDF_MAC_ADDR_FMT,
		  is_pmf_enabled, QDF_MAC_ADDR_REF(peer_mac));

	return QDF_STATUS_SUCCESS;
}
#else
static inline void wma_set_mgmt_frame_protection(tp_wma_handle wma)
{
}

static QDF_STATUS
wma_set_peer_pmf_status(tp_wma_handle wma, uint8_t *peer_mac,
			bool is_pmf_enabled)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_11W */

QDF_STATUS wma_pre_vdev_start_setup(uint8_t vdev_id,
				    struct bss_params *add_bss)
{
	QDF_STATUS status;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct wma_txrx_node *iface;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct vdev_mlme_obj *mlme_obj;
	uint8_t *mac_addr;

	if (!soc) {
		wma_err("Invalid soc");
		return QDF_STATUS_E_FAILURE;
	}
	if (!wma) {
		wma_err("Invalid wma handle");
		return QDF_STATUS_E_FAILURE;
	}
	iface = &wma->interfaces[vdev_id];

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(iface->vdev);
	if (!mlme_obj) {
		wma_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	wma_set_bss_rate_flags(wma, vdev_id, add_bss);
	if (wlan_vdev_mlme_get_opmode(iface->vdev) == QDF_NDI_MODE ||
	    wlan_vdev_mlme_get_opmode(iface->vdev) == QDF_IBSS_MODE)
		mac_addr = mlme_obj->mgmt.generic.bssid;
	else
		mac_addr = wlan_vdev_mlme_get_macaddr(iface->vdev);

	status = wma_create_peer(wma, mac_addr,
				 WMI_PEER_TYPE_DEFAULT, vdev_id);
	if (status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to create peer");
		return status;
	}

	if (!cdp_find_peer_exist(soc, OL_TXRX_PDEV_ID, mac_addr)) {
		wma_err("Failed to find peer "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(mac_addr));
		return QDF_STATUS_E_FAILURE;
	}

	iface->rmfEnabled = add_bss->rmfEnabled;
	if (add_bss->rmfEnabled)
		wma_set_mgmt_frame_protection(wma);

	return status;
}

QDF_STATUS wma_post_vdev_start_setup(uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tp_wma_handle wma = cds_get_context(QDF_MODULE_ID_WMA);
	struct wma_txrx_node *intr;
	struct vdev_mlme_obj *mlme_obj;
	struct wlan_objmgr_vdev *vdev;
	uint8_t bss_power = 0;

	if (!wma) {
		wma_err("Invalid wma handle");
		return QDF_STATUS_E_FAILURE;
	}
	intr = &wma->interfaces[vdev_id];
	if (!intr) {
		wma_err("Invalid interface");
		return QDF_STATUS_E_FAILURE;
	}
	vdev = intr->vdev;
	if (!vdev) {
		wma_err("vdev is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_NDI_MODE ||
	    wlan_vdev_mlme_get_opmode(vdev) == QDF_IBSS_MODE) {
		/* Initialize protection mode to no protection */
		wma_vdev_set_param(wma->wmi_handle, vdev_id,
				   WMI_VDEV_PARAM_PROTECTION_MODE,
				   IEEE80211_PROT_NONE);
		return status;
	}
	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!mlme_obj) {
		wma_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/* Fill bss_chan after vdev start */
	qdf_mem_copy(vdev->vdev_mlme.bss_chan,
		     vdev->vdev_mlme.des_chan,
		     sizeof(struct wlan_channel));

	if (!wlan_reg_is_ext_tpc_supported(wma->psoc))
		bss_power = wlan_reg_get_channel_reg_power_for_freq(
				wma->pdev, vdev->vdev_mlme.bss_chan->ch_freq);

	wma_vdev_set_bss_params(wma, vdev_id,
				mlme_obj->proto.generic.beacon_interval,
				mlme_obj->proto.generic.dtim_period,
				mlme_obj->proto.generic.slot_time,
				mlme_obj->proto.generic.protection_mode,
				bss_power);

	wma_vdev_set_he_bss_params(wma, vdev_id,
				   &mlme_obj->proto.he_ops_info);

	return status;
}

static QDF_STATUS wma_update_iface_params(tp_wma_handle wma,
					  struct bss_params *add_bss)
{
	struct wma_txrx_node *iface;
	uint8_t vdev_id;

	vdev_id = add_bss->staContext.smesessionId;
	iface = &wma->interfaces[vdev_id];
	wma_set_bss_rate_flags(wma, vdev_id, add_bss);

	if (iface->addBssStaContext)
		qdf_mem_free(iface->addBssStaContext);
	iface->addBssStaContext = qdf_mem_malloc(sizeof(tAddStaParams));
	if (!iface->addBssStaContext)
		return QDF_STATUS_E_RESOURCES;

	*iface->addBssStaContext = add_bss->staContext;
	/* Save parameters later needed by WMA_ADD_STA_REQ */
	iface->rmfEnabled = add_bss->rmfEnabled;
	if (add_bss->rmfEnabled)
		wma_set_peer_pmf_status(wma, add_bss->bssId, true);
	iface->beaconInterval = add_bss->beaconInterval;
	iface->llbCoexist = add_bss->llbCoexist;
	iface->shortSlotTimeSupported = add_bss->shortSlotTimeSupported;
	iface->nwType = add_bss->nwType;

	return QDF_STATUS_SUCCESS;
}

static inline
bool wma_cdp_find_peer_by_addr(uint8_t *peer_addr)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t pdev_id = OL_TXRX_PDEV_ID;

	if (!soc || pdev_id == OL_TXRX_INVALID_PDEV_ID) {
		wma_err("Failed to get pdev/soc");
		return NULL;
	}

	return cdp_find_peer_exist(soc, pdev_id, peer_addr);
}

static
QDF_STATUS wma_save_bss_params(tp_wma_handle wma, struct bss_params *add_bss)
{
	QDF_STATUS status;

	wma_vdev_set_he_config(wma, add_bss->staContext.smesessionId, add_bss);
	if (!wma_cdp_find_peer_by_addr(add_bss->bssId))
		status = QDF_STATUS_E_FAILURE;
	else
		status = QDF_STATUS_SUCCESS;
	qdf_mem_copy(add_bss->staContext.staMac, add_bss->bssId,
		     sizeof(add_bss->staContext.staMac));

	wma_debug("update_bss %d nw_type %d bssid "QDF_MAC_ADDR_FMT" status %d",
		 add_bss->updateBss, add_bss->nwType,
		 QDF_MAC_ADDR_REF(add_bss->bssId),
		 status);

	return status;
}

QDF_STATUS wma_pre_assoc_req(struct bss_params *add_bss)
{
	QDF_STATUS status;
	tp_wma_handle wma;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Invalid wma");
		return QDF_STATUS_E_INVAL;
	}

	status = wma_update_iface_params(wma, add_bss);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = wma_save_bss_params(wma, add_bss);

	return status;
}

void wma_add_bss_lfr3(tp_wma_handle wma, struct bss_params *add_bss)
{
	QDF_STATUS status;

	status = wma_update_iface_params(wma, add_bss);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	if (!wma_cdp_find_peer_by_addr(add_bss->bssId)) {
		wma_err("Failed to find peer "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(add_bss->bssId));
		return;
	}
	wma_debug("LFR3: bssid "QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(add_bss->bssId));
}


#if defined(QCA_LL_LEGACY_TX_FLOW_CONTROL) || defined(QCA_LL_TX_FLOW_CONTROL_V2)
static
QDF_STATUS wma_set_cdp_vdev_pause_reason(tp_wma_handle wma, uint8_t vdev_id)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	cdp_fc_vdev_pause(soc, vdev_id,
			  OL_TXQ_PAUSE_REASON_PEER_UNAUTHORIZED, 0);

	return QDF_STATUS_SUCCESS;
}
#else
static inline
QDF_STATUS wma_set_cdp_vdev_pause_reason(tp_wma_handle wma, uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

void wma_send_add_bss_resp(tp_wma_handle wma, uint8_t vdev_id,
			   QDF_STATUS status)
{
	struct add_bss_rsp *add_bss_rsp;

	add_bss_rsp = qdf_mem_malloc(sizeof(*add_bss_rsp));
	if (!add_bss_rsp)
		return;

	add_bss_rsp->vdev_id = vdev_id;
	add_bss_rsp->status = status;
	lim_handle_add_bss_rsp(wma->mac_context, add_bss_rsp);
}

#ifdef WLAN_FEATURE_HOST_ROAM
QDF_STATUS wma_add_bss_lfr2_vdev_start(struct wlan_objmgr_vdev *vdev,
				       struct bss_params *add_bss)
{
	tp_wma_handle wma;
	QDF_STATUS status;
	struct vdev_mlme_obj *mlme_obj;
	uint8_t vdev_id;
	bool peer_exist = false;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma || !vdev) {
		wma_err("Invalid wma or vdev");
		return QDF_STATUS_E_INVAL;
	}

	vdev_id = vdev->vdev_objmgr.vdev_id;
	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!mlme_obj) {
		wma_err("vdev component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = wma_update_iface_params(wma, add_bss);
	if (QDF_IS_STATUS_ERROR(status))
		goto send_fail_resp;

	peer_exist = wma_cdp_find_peer_by_addr(mlme_obj->mgmt.generic.bssid);
	if (!peer_exist) {
		wma_err("Failed to find peer "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mlme_obj->mgmt.generic.bssid));
		goto send_fail_resp;
	}

	status = wma_vdev_pre_start(vdev_id, false);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("failed, status: %d", status);
		goto peer_cleanup;
	}
	status = vdev_mgr_start_send(mlme_obj, false);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("failed, status: %d", status);
		goto peer_cleanup;
	}
	status = wma_set_cdp_vdev_pause_reason(wma, vdev_id);
	if (QDF_IS_STATUS_ERROR(status))
		goto peer_cleanup;

	/* ADD_BSS_RESP will be deferred to completion of VDEV_START */
	return QDF_STATUS_SUCCESS;

peer_cleanup:
	if (peer_exist)
		wma_remove_peer(wma, mlme_obj->mgmt.generic.bssid, vdev_id,
				false);

send_fail_resp:
	wma_send_add_bss_resp(wma, vdev_id, QDF_STATUS_E_FAILURE);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS wma_send_peer_assoc_req(struct bss_params *add_bss)
{
	struct wma_target_req *msg;
	tp_wma_handle wma;
	uint8_t vdev_id;
	bool peer_exist;
	QDF_STATUS status;
	struct wma_txrx_node *iface;
	int pps_val = 0;
	struct vdev_mlme_obj *mlme_obj;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Invalid wma");
		return QDF_STATUS_E_INVAL;
	}
	if (!mac) {
		wma_err("Invalid mac context");
		return QDF_STATUS_E_INVAL;
	}

	vdev_id = add_bss->staContext.smesessionId;

	iface = &wma->interfaces[vdev_id];
	status = wma_update_iface_params(wma, add_bss);
	if (QDF_IS_STATUS_ERROR(status))
		goto send_resp;

	peer_exist = wma_cdp_find_peer_by_addr(add_bss->bssId);
	if (add_bss->nonRoamReassoc && peer_exist)
		goto send_resp;

	if (!add_bss->updateBss)
		goto send_resp;

	if (!peer_exist) {
		wma_err("Failed to find peer "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(add_bss->bssId));
		status = QDF_STATUS_E_FAILURE;
		goto send_resp;
	}


	if (add_bss->staContext.encryptType == eSIR_ED_NONE) {
		wma_debug("Update peer("QDF_MAC_ADDR_FMT") state into auth",
			 QDF_MAC_ADDR_REF(add_bss->bssId));
		cdp_peer_state_update(soc, add_bss->bssId,
				      OL_TXRX_PEER_STATE_AUTH);
	} else {
		wma_debug("Update peer("QDF_MAC_ADDR_FMT") state into conn",
			 QDF_MAC_ADDR_REF(add_bss->bssId));
		cdp_peer_state_update(soc, add_bss->bssId,
				      OL_TXRX_PEER_STATE_CONN);
		status = wma_set_cdp_vdev_pause_reason(wma, vdev_id);
		if (QDF_IS_STATUS_ERROR(status))
			goto send_resp;
	}

	wmi_unified_send_txbf(wma, &add_bss->staContext);

	pps_val = ((mac->mlme_cfg->sta.enable_5g_ebt << 31) &
		 0xffff0000) | (PKT_PWR_SAVE_5G_EBT & 0xffff);
	status = wma_vdev_set_param(wma->wmi_handle, vdev_id,
				    WMI_VDEV_PARAM_PACKET_POWERSAVE,
				    pps_val);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to send wmi packet power save cmd");
	else
		wma_debug("Sent packet power save %x", pps_val);

	add_bss->staContext.no_ptk_4_way = add_bss->no_ptk_4_way;

	status = wma_send_peer_assoc(wma, add_bss->nwType,
				     &add_bss->staContext);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to send peer assoc status:%d", status);
		goto send_resp;
	}

	/* we just had peer assoc, so install key will be done later */
	if (add_bss->staContext.encryptType != eSIR_ED_NONE)
		iface->is_waiting_for_key = true;

	if (add_bss->rmfEnabled)
		wma_set_mgmt_frame_protection(wma);

	if (wlan_reg_is_ext_tpc_supported(wma->psoc))
		add_bss->maxTxPower = 0;

	wma_vdev_set_bss_params(wma, add_bss->staContext.smesessionId,
				add_bss->beaconInterval,
				add_bss->dtimPeriod,
				add_bss->shortSlotTimeSupported,
				add_bss->llbCoexist,
				add_bss->maxTxPower);

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(iface->vdev);
	if (!mlme_obj) {
		wma_err("Failed to mlme obj");
		status = QDF_STATUS_E_FAILURE;
		goto send_resp;
	}
	/*
	 * Store the bssid in interface table, bssid will
	 * be used during group key setting sta mode.
	 */
	qdf_mem_copy(mlme_obj->mgmt.generic.bssid,
		     add_bss->bssId, QDF_MAC_ADDR_SIZE);

	wma_save_bss_params(wma, add_bss);

	if (!wmi_service_enabled(wma->wmi_handle,
				 wmi_service_peer_assoc_conf)) {
		wma_debug("WMI_SERVICE_PEER_ASSOC_CONF not enabled");
		goto send_resp;
	}

	msg = wma_fill_hold_req(wma, vdev_id, WMA_ADD_BSS_REQ,
				WMA_PEER_ASSOC_CNF_START, NULL,
				WMA_PEER_ASSOC_TIMEOUT);
	if (!msg) {
		wma_err("Failed to allocate request for vdev_id %d", vdev_id);
		wma_remove_req(wma, vdev_id, WMA_PEER_ASSOC_CNF_START);
		status = QDF_STATUS_E_FAILURE;
		goto send_resp;
	}

	return QDF_STATUS_SUCCESS;

send_resp:
	wma_send_add_bss_resp(wma, vdev_id, status);

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_add_sta_req_ap_mode() - process add sta request in ap mode
 * @wma: wma handle
 * @add_sta: add sta params
 *
 * Return: none
 */
static void wma_add_sta_req_ap_mode(tp_wma_handle wma, tpAddStaParams add_sta)
{
	enum ol_txrx_peer_state state = OL_TXRX_PEER_STATE_CONN;
	uint8_t pdev_id = OL_TXRX_PDEV_ID;
	QDF_STATUS status;
	int32_t ret;
	struct wma_txrx_node *iface = NULL;
	struct wma_target_req *msg;
	bool peer_assoc_cnf = false;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint32_t i, j;
	uint16_t mcs_limit;
	uint8_t *rate_pos;
	struct mac_context *mac = wma->mac_context;

	/* UMAC sends WMA_ADD_STA_REQ msg twice to WMA when the station
	 * associates. First WMA_ADD_STA_REQ will have staType as
	 * STA_ENTRY_PEER and second posting will have STA_ENTRY_SELF.
	 * Peer creation is done in first WMA_ADD_STA_REQ and second
	 * WMA_ADD_STA_REQ which has STA_ENTRY_SELF is ignored and
	 * send fake response with success to UMAC. Otherwise UMAC
	 * will get blocked.
	 */
	if (add_sta->staType != STA_ENTRY_PEER) {
		add_sta->status = QDF_STATUS_SUCCESS;
		goto send_rsp;
	}

	iface = &wma->interfaces[add_sta->smesessionId];

	if (cdp_find_peer_exist_on_vdev(soc, add_sta->smesessionId,
					add_sta->staMac)) {
		wma_remove_peer(wma, add_sta->staMac, add_sta->smesessionId,
				false);
		wma_err("Peer already exists, Deleted peer with peer_addr "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(add_sta->staMac));
	}
	/* The code above only checks the peer existence on its own vdev.
	 * Need to check whether the peer exists on other vDevs because firmware
	 * can't create the peer if the peer with same MAC address already
	 * exists on the pDev. As this peer belongs to other vDevs, just return
	 * here.
	 */
	if (cdp_find_peer_exist(soc, pdev_id, add_sta->staMac)) {
		wma_err("My vdev id %d, but Peer exists on other vdev with peer_addr "QDF_MAC_ADDR_FMT,
			 add_sta->smesessionId,
			 QDF_MAC_ADDR_REF(add_sta->staMac));
		add_sta->status = QDF_STATUS_E_FAILURE;
		goto send_rsp;
	}

	wma_delete_invalid_peer_entries(add_sta->smesessionId, add_sta->staMac);

	status = wma_create_peer(wma, add_sta->staMac, WMI_PEER_TYPE_DEFAULT,
				 add_sta->smesessionId);
	if (status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to create peer for "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(add_sta->staMac));
		add_sta->status = status;
		goto send_rsp;
	}

	if (!cdp_find_peer_exist_on_vdev(soc, add_sta->smesessionId,
					 add_sta->staMac)) {
		wma_err("Failed to find peer handle using peer mac "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(add_sta->staMac));
		add_sta->status = QDF_STATUS_E_FAILURE;
		wma_remove_peer(wma, add_sta->staMac, add_sta->smesessionId,
				false);
		goto send_rsp;
	}

	wmi_unified_send_txbf(wma, add_sta);

	/*
	 * Get MCS limit from ini configure, and map it to rate parameters
	 * This will limit HT rate upper bound. CFG_CTRL_MASK is used to
	 * check whether ini config is enabled and CFG_DATA_MASK to get the
	 * MCS value.
	 */
#define CFG_CTRL_MASK              0xFF00
#define CFG_DATA_MASK              0x00FF

	mcs_limit = mac->mlme_cfg->rates.sap_max_mcs_txdata;

	if (mcs_limit & CFG_CTRL_MASK) {
		wma_debug("set mcs_limit %x", mcs_limit);

		mcs_limit &= CFG_DATA_MASK;
		rate_pos = (u_int8_t *)add_sta->supportedRates.supportedMCSSet;
		for (i = 0, j = 0; i < MAX_SUPPORTED_RATES;) {
			if (j < mcs_limit / 8) {
				rate_pos[j] = 0xff;
				j++;
				i += 8;
			} else if (j < mcs_limit / 8 + 1) {
				if (i <= mcs_limit)
					rate_pos[i / 8] |= 1 << (i % 8);
				else
					rate_pos[i / 8] &= ~(1 << (i % 8));
				i++;

				if (i >= (j + 1) * 8)
					j++;
			} else {
				rate_pos[j++] = 0;
				i += 8;
			}
		}
	}

	if (wmi_service_enabled(wma->wmi_handle,
				    wmi_service_peer_assoc_conf)) {
		peer_assoc_cnf = true;
		msg = wma_fill_hold_req(wma, add_sta->smesessionId,
				   WMA_ADD_STA_REQ, WMA_PEER_ASSOC_CNF_START,
				   add_sta, WMA_PEER_ASSOC_TIMEOUT);
		if (!msg) {
			wma_err("Failed to alloc request for vdev_id %d",
				add_sta->smesessionId);
			add_sta->status = QDF_STATUS_E_FAILURE;
			wma_remove_req(wma, add_sta->smesessionId,
				       WMA_PEER_ASSOC_CNF_START);
			wma_remove_peer(wma, add_sta->staMac,
					add_sta->smesessionId, false);
			peer_assoc_cnf = false;
			goto send_rsp;
		}
	} else {
		wma_err("WMI_SERVICE_PEER_ASSOC_CONF not enabled");
	}

	ret = wma_send_peer_assoc(wma, add_sta->nwType, add_sta);
	if (ret) {
		add_sta->status = QDF_STATUS_E_FAILURE;
		wma_remove_peer(wma, add_sta->staMac, add_sta->smesessionId,
				false);
		goto send_rsp;
	}

	if (add_sta->rmfEnabled)
		wma_set_peer_pmf_status(wma, add_sta->staMac, true);

	if (add_sta->uAPSD) {
		status = wma_set_ap_peer_uapsd(wma, add_sta->smesessionId,
					    add_sta->staMac,
					    add_sta->uAPSD, add_sta->maxSPLen);
		if (QDF_IS_STATUS_ERROR(status)) {
			wma_err("Failed to set peer uapsd param for "QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(add_sta->staMac));
			add_sta->status = QDF_STATUS_E_FAILURE;
			wma_remove_peer(wma, add_sta->staMac,
					add_sta->smesessionId, false);
			goto send_rsp;
		}
	}

	wma_debug("Moving peer "QDF_MAC_ADDR_FMT" to state %d",
		  QDF_MAC_ADDR_REF(add_sta->staMac), state);
	cdp_peer_state_update(soc, add_sta->staMac, state);

	add_sta->nss    = iface->nss;
	add_sta->status = QDF_STATUS_SUCCESS;
send_rsp:
	/* Do not send add stat resp when peer assoc cnf is enabled */
	if (peer_assoc_cnf) {
		wma_debug("WMI_SERVICE_PEER_ASSOC_CONF is enabled");
		return;
	}

	wma_debug("statype %d vdev_id %d aid %d bssid "QDF_MAC_ADDR_FMT" status %d",
		 add_sta->staType, add_sta->smesessionId,
		 add_sta->assocId, QDF_MAC_ADDR_REF(add_sta->bssId),
		 add_sta->status);
	wma_send_msg_high_priority(wma, WMA_ADD_STA_RSP, (void *)add_sta, 0);
}

#ifdef FEATURE_WLAN_TDLS

/**
 * wma_add_tdls_sta() - process add sta request in TDLS mode
 * @wma: wma handle
 * @add_sta: add sta params
 *
 * Return: none
 */
static void wma_add_tdls_sta(tp_wma_handle wma, tpAddStaParams add_sta)
{
	QDF_STATUS status;
	int32_t ret;
	struct tdls_peer_update_state *peer_state;
	struct wma_target_req *msg;
	bool peer_assoc_cnf = false;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t pdev_id = OL_TXRX_PDEV_ID;

	wma_debug("staType: %d, updateSta: %d, bssId: "QDF_MAC_ADDR_FMT", staMac: "QDF_MAC_ADDR_FMT,
		 add_sta->staType,
		 add_sta->updateSta, QDF_MAC_ADDR_REF(add_sta->bssId),
		 QDF_MAC_ADDR_REF(add_sta->staMac));

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(wma->psoc, add_sta->smesessionId) ||
	    wma_is_roam_in_progress(add_sta->smesessionId)) {
		wma_err("roaming in progress, reject add sta!");
		add_sta->status = QDF_STATUS_E_PERM;
		goto send_rsp;
	}

	if (0 == add_sta->updateSta) {
		/* its a add sta request * */

		cdp_peer_copy_mac_addr_raw(soc, add_sta->smesessionId,
					   add_sta->bssId);

		wma_debug("addSta, calling wma_create_peer for "QDF_MAC_ADDR_FMT", vdev_id %hu",
			  QDF_MAC_ADDR_REF(add_sta->staMac),
			  add_sta->smesessionId);

		status = wma_create_peer(wma, add_sta->staMac,
					 WMI_PEER_TYPE_TDLS,
					 add_sta->smesessionId);
		if (status != QDF_STATUS_SUCCESS) {
			wma_err("Failed to create peer for "QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(add_sta->staMac));
			add_sta->status = status;
			goto send_rsp;
		}

		wma_debug("addSta, after calling cdp_local_peer_id, staMac: "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(add_sta->staMac));

		peer_state = qdf_mem_malloc(sizeof(*peer_state));
		if (!peer_state) {
			add_sta->status = QDF_STATUS_E_NOMEM;
			goto send_rsp;
		}

		peer_state->peer_state = WMI_TDLS_PEER_STATE_PEERING;
		peer_state->vdev_id = add_sta->smesessionId;
		qdf_mem_copy(&peer_state->peer_macaddr,
			     &add_sta->staMac, sizeof(tSirMacAddr));
		wma_update_tdls_peer_state(wma, peer_state);
	} else {
		if (wmi_service_enabled(wma->wmi_handle,
					    wmi_service_peer_assoc_conf)) {
			wma_err("WMI_SERVICE_PEER_ASSOC_CONF is enabled");
			peer_assoc_cnf = true;
			msg = wma_fill_hold_req(wma, add_sta->smesessionId,
				WMA_ADD_STA_REQ, WMA_PEER_ASSOC_CNF_START,
				add_sta, WMA_PEER_ASSOC_TIMEOUT);
			if (!msg) {
				wma_err("Failed to alloc request for vdev_id %d",
					add_sta->smesessionId);
				add_sta->status = QDF_STATUS_E_FAILURE;
				wma_remove_req(wma, add_sta->smesessionId,
					       WMA_PEER_ASSOC_CNF_START);
				wma_remove_peer(wma, add_sta->staMac,
						add_sta->smesessionId, false);
				peer_assoc_cnf = false;
				goto send_rsp;
			}
		} else {
			wma_err("WMI_SERVICE_PEER_ASSOC_CONF not enabled");
		}

		wma_debug("changeSta, calling wma_send_peer_assoc");
		if (add_sta->rmfEnabled)
			wma_set_peer_pmf_status(wma, add_sta->staMac, true);

		ret =
			wma_send_peer_assoc(wma, add_sta->nwType, add_sta);
		if (ret) {
			add_sta->status = QDF_STATUS_E_FAILURE;
			wma_remove_peer(wma, add_sta->staMac,
					add_sta->smesessionId, false);
			cdp_peer_add_last_real_peer(soc, pdev_id,
						    add_sta->smesessionId);
			wma_remove_req(wma, add_sta->smesessionId,
				       WMA_PEER_ASSOC_CNF_START);
			peer_assoc_cnf = false;

			goto send_rsp;
		}
	}

send_rsp:
	/* Do not send add stat resp when peer assoc cnf is enabled */
	if (peer_assoc_cnf)
		return;

	wma_debug("statype %d vdev_id %d aid %d bssid "QDF_MAC_ADDR_FMT" status %d",
		 add_sta->staType, add_sta->smesessionId,
		 add_sta->assocId, QDF_MAC_ADDR_REF(add_sta->bssId),
		 add_sta->status);
	wma_send_msg_high_priority(wma, WMA_ADD_STA_RSP, (void *)add_sta, 0);
}
#endif

/**
 * wma_send_bss_color_change_enable() - send bss color change enable cmd.
 * @wma: wma handle
 * @params: add sta params
 *
 * Send bss color change command to firmware, to enable firmware to update
 * internally if any change in bss color in advertised by associated AP.
 *
 * Return: none
 */
#ifdef WLAN_FEATURE_11AX
static void wma_send_bss_color_change_enable(tp_wma_handle wma,
					     tpAddStaParams params)
{
	QDF_STATUS status;
	uint32_t vdev_id = params->smesessionId;

	if (!params->he_capable) {
		wma_debug("he_capable is not set for vdev_id:%d", vdev_id);
		return;
	}

	status = wmi_unified_send_bss_color_change_enable_cmd(wma->wmi_handle,
							      vdev_id,
							      true);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to enable bss color change offload, vdev:%d",
			vdev_id);
	}

	return;
}
#else
static void wma_send_bss_color_change_enable(tp_wma_handle wma,
					     tpAddStaParams params)
{
}
#endif

/**
 * wma_add_sta_req_sta_mode() - process add sta request in sta mode
 * @wma: wma handle
 * @params: add sta params
 *
 * Return: none
 */
static void wma_add_sta_req_sta_mode(tp_wma_handle wma, tpAddStaParams params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wma_txrx_node *iface;
	int8_t maxTxPower = 0;
	int ret = 0;
	struct wma_target_req *msg;
	bool peer_assoc_cnf = false;
	int smps_param;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

#ifdef FEATURE_WLAN_TDLS
	if (STA_ENTRY_TDLS_PEER == params->staType) {
		wma_add_tdls_sta(wma, params);
		return;
	}
#endif

	iface = &wma->interfaces[params->smesessionId];
	if (params->staType != STA_ENTRY_SELF) {
		wma_err("unsupported station type %d", params->staType);
		goto out;
	}
	if (params->nonRoamReassoc) {
		cdp_peer_state_update(soc, params->bssId,
				      OL_TXRX_PEER_STATE_AUTH);
		qdf_atomic_set(&iface->bss_status, WMA_BSS_STATUS_STARTED);
		iface->aid = params->assocId;
		goto out;
	}

	if (wma_is_vdev_up(params->smesessionId)) {
		wma_debug("vdev id %d is already UP for "QDF_MAC_ADDR_FMT,
			 params->smesessionId,
			 QDF_MAC_ADDR_REF(params->bssId));
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	if (cdp_peer_state_get(soc, params->smesessionId,
			       params->bssId) == OL_TXRX_PEER_STATE_DISC) {
		/*
		 * This is the case for reassociation.
		 * peer state update and peer_assoc is required since it
		 * was not done by WMA_ADD_BSS_REQ.
		 */

		/* Update peer state */
		if (params->encryptType == eSIR_ED_NONE) {
			wma_debug("Update peer("QDF_MAC_ADDR_FMT") state into auth",
				  QDF_MAC_ADDR_REF(params->bssId));
			cdp_peer_state_update(soc, params->bssId,
					      OL_TXRX_PEER_STATE_AUTH);
		} else {
			wma_debug("Update peer("QDF_MAC_ADDR_FMT") state into conn",
				  QDF_MAC_ADDR_REF(params->bssId));
			cdp_peer_state_update(soc, params->bssId,
					      OL_TXRX_PEER_STATE_CONN);
		}

		if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(wma->psoc,
						   params->smesessionId)) {
			/* iface->nss = params->nss; */
			/*In LFR2.0, the following operations are performed as
			 * part of wma_send_peer_assoc. As we are
			 * skipping this operation, we are just executing the
			 * following which are useful for LFR3.0
			 */
			cdp_peer_state_update(soc, params->bssId,
					      OL_TXRX_PEER_STATE_AUTH);
			qdf_atomic_set(&iface->bss_status,
				       WMA_BSS_STATUS_STARTED);
			iface->aid = params->assocId;
			wma_debug("LFR3:statype %d vdev %d aid %d bssid "QDF_MAC_ADDR_FMT,
					params->staType, params->smesessionId,
					params->assocId,
					QDF_MAC_ADDR_REF(params->bssId));
			return;
		}
		wmi_unified_send_txbf(wma, params);

		if (wmi_service_enabled(wma->wmi_handle,
					    wmi_service_peer_assoc_conf)) {
			peer_assoc_cnf = true;
			msg = wma_fill_hold_req(wma, params->smesessionId,
				WMA_ADD_STA_REQ, WMA_PEER_ASSOC_CNF_START,
				params, WMA_PEER_ASSOC_TIMEOUT);
			if (!msg) {
				wma_debug("Failed to alloc request for vdev_id %d",
					 params->smesessionId);
				params->status = QDF_STATUS_E_FAILURE;
				wma_remove_req(wma, params->smesessionId,
					       WMA_PEER_ASSOC_CNF_START);
				wma_remove_peer(wma, params->bssId,
						params->smesessionId, false);
				peer_assoc_cnf = false;
				goto out;
			}
		} else {
			wma_debug("WMI_SERVICE_PEER_ASSOC_CONF not enabled");
		}

		((tAddStaParams *)iface->addBssStaContext)->no_ptk_4_way =
						params->no_ptk_4_way;

		qdf_mem_copy(((tAddStaParams *)iface->addBssStaContext)->
			     supportedRates.supportedMCSSet,
			     params->supportedRates.supportedMCSSet,
			     SIR_MAC_MAX_SUPPORTED_MCS_SET);


		ret = wma_send_peer_assoc(wma,
				iface->nwType,
				(tAddStaParams *) iface->addBssStaContext);
		if (ret) {
			status = QDF_STATUS_E_FAILURE;
			wma_remove_peer(wma, params->bssId,
					params->smesessionId, false);
			goto out;
		}

		if (params->rmfEnabled) {
			wma_set_mgmt_frame_protection(wma);
			wma_set_peer_pmf_status(wma, params->bssId, true);
		}
	}

	if (!wlan_reg_is_ext_tpc_supported(wma->psoc))
		maxTxPower = params->maxTxPower;

	wma_vdev_set_bss_params(wma, params->smesessionId,
				iface->beaconInterval, iface->dtimPeriod,
				iface->shortSlotTimeSupported,
				iface->llbCoexist, maxTxPower);

	params->csaOffloadEnable = 0;
	if (wmi_service_enabled(wma->wmi_handle,
				   wmi_service_csa_offload)) {
		params->csaOffloadEnable = 1;
		if (wma_unified_csa_offload_enable(wma, params->smesessionId) <
		    0) {
			wma_err("Unable to enable CSA offload for vdev_id:%d",
				params->smesessionId);
		}
	}

	if (wmi_service_enabled(wma->wmi_handle,
				wmi_service_filter_ipsec_natkeepalive)) {
		if (wmi_unified_nat_keepalive_en_cmd(wma->wmi_handle,
						     params->smesessionId)) {
			wma_err("Unable to enable NAT keepalive for vdev_id:%d",
				params->smesessionId);
		}
	}
	qdf_atomic_set(&iface->bss_status, WMA_BSS_STATUS_STARTED);
	/* Sta is now associated, configure various params */

	/* Send SMPS force command to FW to send the required
	 * action frame only when SM power save is enbaled in
	 * from INI. In case dynamic antenna selection, the
	 * action frames are sent by the chain mask manager
	 * In addition to the action frames, The SM power save is
	 * published in the assoc request HT SMPS IE for both cases.
	 */
	if ((params->enableHtSmps) && (params->send_smps_action)) {
		smps_param = wma_smps_mode_to_force_mode_param(
			params->htSmpsconfig);
		if (smps_param >= 0) {
			wma_debug("Send SMPS force mode: %d",
				 params->htSmpsconfig);
			wma_set_mimops(wma, params->smesessionId,
				smps_param);
		}
	}

	wma_send_bss_color_change_enable(wma, params);

	/* Partial AID match power save, enable when SU bformee */
	if (params->enableVhtpAid && params->vhtTxBFCapable)
		wma_set_ppsconfig(params->smesessionId,
				  WMA_VHT_PPS_PAID_MATCH, 1);

	/* Enable AMPDU power save, if htCapable/vhtCapable */
	if (params->enableAmpduPs && (params->htCapable || params->vhtCapable))
		wma_set_ppsconfig(params->smesessionId,
				  WMA_VHT_PPS_DELIM_CRC_FAIL, 1);
	if (wmi_service_enabled(wma->wmi_handle,
				wmi_service_listen_interval_offload_support)) {
		wma_debug("listen interval offload enabled, setting params");
		status = wma_vdev_set_param(wma->wmi_handle,
					    params->smesessionId,
					    WMI_VDEV_PARAM_MAX_LI_OF_MODDTIM,
					    wma->staMaxLIModDtim);
		if (status != QDF_STATUS_SUCCESS) {
			wma_err("can't set MAX_LI for session: %d",
				params->smesessionId);
		}
		status = wma_vdev_set_param(wma->wmi_handle,
					    params->smesessionId,
					    WMI_VDEV_PARAM_DYNDTIM_CNT,
					    wma->staDynamicDtim);
		if (status != QDF_STATUS_SUCCESS) {
			wma_err("can't set DYNDTIM_CNT for session: %d",
				params->smesessionId);
		}
		status  = wma_vdev_set_param(wma->wmi_handle,
					     params->smesessionId,
					     WMI_VDEV_PARAM_MODDTIM_CNT,
					     wma->staModDtim);
		if (status != QDF_STATUS_SUCCESS) {
			wma_err("can't set DTIM_CNT for session: %d",
				params->smesessionId);
		}

	} else {
		wma_debug("listen interval offload is not set");
	}

	iface->aid = params->assocId;
	params->nss = iface->nss;
out:
	/* Do not send add stat resp when peer assoc cnf is enabled */
	if (peer_assoc_cnf)
		return;

	params->status = status;
	wma_debug("vdev_id %d aid %d sta mac " QDF_MAC_ADDR_FMT " status %d",
		  params->smesessionId, params->assocId,
		  QDF_MAC_ADDR_REF(params->bssId), params->status);
	/* Don't send a response during roam sync operation */
	if (!MLME_IS_ROAM_SYNCH_IN_PROGRESS(wma->psoc, params->smesessionId))
		wma_send_msg_high_priority(wma, WMA_ADD_STA_RSP,
					   (void *)params, 0);
}

/**
 * wma_delete_sta_req_ap_mode() - process delete sta request from UMAC in AP mode
 * @wma: wma handle
 * @del_sta: delete sta params
 *
 * Return: none
 */
static void wma_delete_sta_req_ap_mode(tp_wma_handle wma,
				       tpDeleteStaParams del_sta)
{
	struct wma_target_req *msg;
	QDF_STATUS qdf_status;

	qdf_status = wma_remove_peer(wma, del_sta->staMac,
				     del_sta->smesessionId, false);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		wma_err("wma_remove_peer failed");
		del_sta->status = QDF_STATUS_E_FAILURE;
		goto send_del_rsp;
	}
	del_sta->status = QDF_STATUS_SUCCESS;

	if (wmi_service_enabled(wma->wmi_handle,
				    wmi_service_sync_delete_cmds)) {
		msg = wma_fill_hold_req(wma, del_sta->smesessionId,
				   WMA_DELETE_STA_REQ,
				   WMA_DELETE_STA_RSP_START, del_sta,
				   WMA_DELETE_STA_TIMEOUT);
		if (!msg) {
			wma_err("Failed to allocate request. vdev_id %d",
				 del_sta->smesessionId);
			wma_remove_req(wma, del_sta->smesessionId,
				WMA_DELETE_STA_RSP_START);
			del_sta->status = QDF_STATUS_E_NOMEM;
			goto send_del_rsp;
		}

		wma_acquire_wakelock(&wma->wmi_cmd_rsp_wake_lock,
				     WMA_FW_RSP_EVENT_WAKE_LOCK_DURATION);

		return;
	}

send_del_rsp:
	if (del_sta->respReqd) {
		wma_debug("Sending del rsp to umac (status: %d)",
			 del_sta->status);
		wma_send_msg_high_priority(wma, WMA_DELETE_STA_RSP,
					   (void *)del_sta, 0);
	}
}

#ifdef FEATURE_WLAN_TDLS
/**
 * wma_del_tdls_sta() - process delete sta request from UMAC in TDLS
 * @wma: wma handle
 * @del_sta: delete sta params
 *
 * Return: none
 */
static void wma_del_tdls_sta(tp_wma_handle wma, tpDeleteStaParams del_sta)
{
	struct tdls_peer_update_state *peer_state;
	struct wma_target_req *msg;
	int status;

	peer_state = qdf_mem_malloc(sizeof(*peer_state));
	if (!peer_state) {
		del_sta->status = QDF_STATUS_E_NOMEM;
		goto send_del_rsp;
	}

	peer_state->peer_state = TDLS_PEER_STATE_TEARDOWN;
	peer_state->vdev_id = del_sta->smesessionId;
	peer_state->resp_reqd = del_sta->respReqd;
	qdf_mem_copy(&peer_state->peer_macaddr,
		     &del_sta->staMac, sizeof(tSirMacAddr));

	wma_debug("sending tdls_peer_state for peer mac: "QDF_MAC_ADDR_FMT", peerState: %d",
		  QDF_MAC_ADDR_REF(peer_state->peer_macaddr),
		 peer_state->peer_state);

	status = wma_update_tdls_peer_state(wma, peer_state);

	if (status < 0) {
		wma_err("wma_update_tdls_peer_state returned failure");
		del_sta->status = QDF_STATUS_E_FAILURE;
		goto send_del_rsp;
	}

	if (del_sta->respReqd &&
			wmi_service_enabled(wma->wmi_handle,
				wmi_service_sync_delete_cmds)) {
		del_sta->status = QDF_STATUS_SUCCESS;
		msg = wma_fill_hold_req(wma,
				del_sta->smesessionId,
				WMA_DELETE_STA_REQ,
				WMA_DELETE_STA_RSP_START, del_sta,
				WMA_DELETE_STA_TIMEOUT);
		if (!msg) {
			wma_err("Failed to allocate vdev_id %d",
				del_sta->smesessionId);
			wma_remove_req(wma,
					del_sta->smesessionId,
					WMA_DELETE_STA_RSP_START);
			del_sta->status = QDF_STATUS_E_NOMEM;
			goto send_del_rsp;
		}

		wma_acquire_wakelock(&wma->wmi_cmd_rsp_wake_lock,
				WMA_FW_RSP_EVENT_WAKE_LOCK_DURATION);
	}

	return;

send_del_rsp:
	if (del_sta->respReqd) {
		wma_debug("Sending del rsp to umac (status: %d)",
			 del_sta->status);
		wma_send_msg_high_priority(wma, WMA_DELETE_STA_RSP,
					   (void *)del_sta, 0);
	}
}
#endif

/**
 * wma_delete_sta_req_sta_mode() - process delete sta request from UMAC
 * @wma: wma handle
 * @params: delete sta params
 *
 * Return: none
 */
static void wma_delete_sta_req_sta_mode(tp_wma_handle wma,
					tpDeleteStaParams params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wma_txrx_node *iface;

	iface = &wma->interfaces[params->smesessionId];
	iface->uapsd_cached_val = 0;
#ifdef FEATURE_WLAN_TDLS
	if (STA_ENTRY_TDLS_PEER == params->staType) {
		wma_del_tdls_sta(wma, params);
		return;
	}
#endif
	params->status = status;
	if (params->respReqd) {
		wma_debug("vdev_id %d status %d",
			 params->smesessionId, status);
		wma_send_msg_high_priority(wma, WMA_DELETE_STA_RSP,
					   (void *)params, 0);
	}
}

static void wma_sap_prevent_runtime_pm(tp_wma_handle wma)
{
	qdf_runtime_pm_prevent_suspend(&wma->sap_prevent_runtime_pm_lock);
}

static void wma_sap_allow_runtime_pm(tp_wma_handle wma)
{
	qdf_runtime_pm_allow_suspend(&wma->sap_prevent_runtime_pm_lock);
}

static bool wma_is_vdev_in_sap_mode(tp_wma_handle wma, uint8_t vdev_id)
{
	struct wma_txrx_node *intf = wma->interfaces;

	if (vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev_id %hu", vdev_id);
		QDF_ASSERT(0);
		return false;
	}

	if ((intf[vdev_id].type == WMI_VDEV_TYPE_AP) &&
	    (intf[vdev_id].sub_type == 0))
		return true;

	return false;
}

static bool wma_is_vdev_in_go_mode(tp_wma_handle wma, uint8_t vdev_id)
{
	struct wma_txrx_node *intf = wma->interfaces;

	if (vdev_id >= wma->max_bssid) {
		wma_err("Invalid vdev_id %hu", vdev_id);
		QDF_ASSERT(0);
		return false;
	}

	if ((intf[vdev_id].type == WMI_VDEV_TYPE_AP) &&
	    (intf[vdev_id].sub_type == WMI_UNIFIED_VDEV_SUBTYPE_P2P_GO))
		return true;

	return false;
}

void wma_add_sta(tp_wma_handle wma, tpAddStaParams add_sta)
{
	uint8_t oper_mode = BSS_OPERATIONAL_MODE_STA;
	void *htc_handle;

	htc_handle = lmac_get_htc_hdl(wma->psoc);
	if (!htc_handle) {
		wma_err("HTC handle is NULL");
		return;
	}

	wma_debug("Vdev %d BSSID "QDF_MAC_ADDR_FMT, add_sta->smesessionId,
		  QDF_MAC_ADDR_REF(add_sta->bssId));

	if (wma_is_vdev_in_ap_mode(wma, add_sta->smesessionId))
		oper_mode = BSS_OPERATIONAL_MODE_AP;

	if (WMA_IS_VDEV_IN_NDI_MODE(wma->interfaces, add_sta->smesessionId))
		oper_mode = BSS_OPERATIONAL_MODE_NDI;
	switch (oper_mode) {
	case BSS_OPERATIONAL_MODE_STA:
		wma_add_sta_req_sta_mode(wma, add_sta);
		break;

	case BSS_OPERATIONAL_MODE_AP:
		wma_add_sta_req_ap_mode(wma, add_sta);
		break;
	case BSS_OPERATIONAL_MODE_NDI:
		wma_add_sta_ndi_mode(wma, add_sta);
		break;
	}

	/* handle wow for sap with 1 or more peer in same way */
	if (wma_is_vdev_in_sap_mode(wma, add_sta->smesessionId)) {
		bool is_bus_suspend_allowed_in_sap_mode =
			(wlan_pmo_get_sap_mode_bus_suspend(wma->psoc) &&
				wmi_service_enabled(wma->wmi_handle,
					wmi_service_sap_connected_d3_wow));
		if (!is_bus_suspend_allowed_in_sap_mode) {
			htc_vote_link_up(htc_handle);
			wmi_info("sap d0 wow");
		} else {
			wmi_info("sap d3 wow");
		}
		wma_sap_prevent_runtime_pm(wma);

		return;
	}

	/* handle wow for p2pgo with 1 or more peer in same way */
	if (wma_is_vdev_in_go_mode(wma, add_sta->smesessionId)) {
		bool is_bus_suspend_allowed_in_go_mode =
			(wlan_pmo_get_go_mode_bus_suspend(wma->psoc) &&
				wmi_service_enabled(wma->wmi_handle,
					wmi_service_go_connected_d3_wow));
		if (!is_bus_suspend_allowed_in_go_mode) {
			htc_vote_link_up(htc_handle);
			wmi_info("p2p go d0 wow");
		} else {
			wmi_info("p2p go d3 wow");
		}
		wma_sap_prevent_runtime_pm(wma);

		return;
	}

	/* handle wow for nan with 1 or more peer in same way */
	if (BSS_OPERATIONAL_MODE_NDI == oper_mode) {
		wma_debug("disable runtime pm and vote for link up");
		htc_vote_link_up(htc_handle);
		wma_sap_prevent_runtime_pm(wma);
	}
}

void wma_delete_sta(tp_wma_handle wma, tpDeleteStaParams del_sta)
{
	uint8_t oper_mode = BSS_OPERATIONAL_MODE_STA;
	uint8_t vdev_id = del_sta->smesessionId;
	bool rsp_requested = del_sta->respReqd;
	void *htc_handle;

	htc_handle = lmac_get_htc_hdl(wma->psoc);
	if (!htc_handle) {
		wma_err("HTC handle is NULL");
		return;
	}

	if (wma_is_vdev_in_ap_mode(wma, vdev_id))
		oper_mode = BSS_OPERATIONAL_MODE_AP;
	if (del_sta->staType == STA_ENTRY_NDI_PEER)
		oper_mode = BSS_OPERATIONAL_MODE_NDI;

	wma_debug("vdev %d oper_mode %d", vdev_id, oper_mode);

	switch (oper_mode) {
	case BSS_OPERATIONAL_MODE_STA:
		if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(wma->psoc, vdev_id)) {
			wma_debug("LFR3: Del STA on vdev_id %d", vdev_id);
			qdf_mem_free(del_sta);
			return;
		}
		wma_delete_sta_req_sta_mode(wma, del_sta);
		if (!rsp_requested)
			qdf_mem_free(del_sta);

		break;

	case BSS_OPERATIONAL_MODE_AP:
		wma_delete_sta_req_ap_mode(wma, del_sta);
		/* free the memory here only if sync feature is not enabled */
		if (!rsp_requested &&
		    !wmi_service_enabled(wma->wmi_handle,
					 wmi_service_sync_delete_cmds))
			qdf_mem_free(del_sta);
		else if (!rsp_requested &&
			 (del_sta->status != QDF_STATUS_SUCCESS))
			qdf_mem_free(del_sta);
		break;
	case BSS_OPERATIONAL_MODE_NDI:
		wma_delete_sta_req_ndi_mode(wma, del_sta);
		break;
	default:
		wma_err("Incorrect oper mode %d", oper_mode);
		qdf_mem_free(del_sta);
	}

	if (wma_is_vdev_in_sap_mode(wma, vdev_id)) {
		bool is_bus_suspend_allowed_in_sap_mode =
			(wlan_pmo_get_sap_mode_bus_suspend(wma->psoc) &&
				wmi_service_enabled(wma->wmi_handle,
					wmi_service_sap_connected_d3_wow));
		if (!is_bus_suspend_allowed_in_sap_mode) {
			htc_vote_link_down(htc_handle);
			wmi_info("sap d0 wow");
		} else {
			wmi_info("sap d3 wow");
		}
		wma_sap_allow_runtime_pm(wma);

		return;
	}

	if (wma_is_vdev_in_go_mode(wma, vdev_id)) {
		bool is_bus_suspend_allowed_in_go_mode =
			(wlan_pmo_get_go_mode_bus_suspend(wma->psoc) &&
				wmi_service_enabled(wma->wmi_handle,
					wmi_service_go_connected_d3_wow));
		if (!is_bus_suspend_allowed_in_go_mode) {
			htc_vote_link_down(htc_handle);
			wmi_info("p2p go d0 wow");
		} else {
			wmi_info("p2p go d3 wow");
		}
		wma_sap_allow_runtime_pm(wma);

		return;
	}

	if (BSS_OPERATIONAL_MODE_NDI == oper_mode) {
		wma_debug("allow runtime pm and vote for link down");
		htc_vote_link_down(htc_handle);
		wma_sap_allow_runtime_pm(wma);
	}
}

void wma_delete_bss_ho_fail(tp_wma_handle wma, uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wma_txrx_node *iface;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct vdev_stop_response resp_event;
	struct del_bss_resp *vdev_stop_resp;
	uint8_t *bssid;

	iface = &wma->interfaces[vdev_id];
	if (!iface) {
		wma_err("iface for vdev_id %d is already deleted", vdev_id);
		goto fail_del_bss_ho_fail;
	}
	bssid = wma_get_vdev_bssid(iface->vdev);
	if (!bssid) {
		wma_err("Invalid bssid");
		status = QDF_STATUS_E_FAILURE;
		goto fail_del_bss_ho_fail;
	}
	qdf_mem_zero(bssid, QDF_MAC_ADDR_SIZE);

	if (iface->psnr_req) {
		qdf_mem_free(iface->psnr_req);
		iface->psnr_req = NULL;
	}

	if (iface->rcpi_req) {
		struct sme_rcpi_req *rcpi_req = iface->rcpi_req;

		iface->rcpi_req = NULL;
		qdf_mem_free(rcpi_req);
	}

	if (iface->roam_scan_stats_req) {
		struct sir_roam_scan_stats *roam_scan_stats_req =
						iface->roam_scan_stats_req;

		iface->roam_scan_stats_req = NULL;
		qdf_mem_free(roam_scan_stats_req);
	}

	wma_debug("vdev_id: %d, pausing tx_ll_queue for VDEV_STOP (del_bss)",
		 vdev_id);
	cdp_fc_vdev_pause(soc, vdev_id, OL_TXQ_PAUSE_REASON_VDEV_STOP, 0);
	wma_vdev_set_pause_bit(vdev_id, PAUSE_TYPE_HOST);
	cdp_fc_vdev_flush(soc, vdev_id);
	wma_debug("vdev_id: %d, un-pausing tx_ll_queue for VDEV_STOP rsp",
		 vdev_id);
	cdp_fc_vdev_unpause(soc, vdev_id, OL_TXQ_PAUSE_REASON_VDEV_STOP, 0);
	wma_vdev_clear_pause_bit(vdev_id, PAUSE_TYPE_HOST);
	qdf_atomic_set(&iface->bss_status, WMA_BSS_STATUS_STOPPED);
	wma_debug("(type %d subtype %d) BSS is stopped",
			iface->type, iface->sub_type);

	status = mlme_set_vdev_stop_type(iface->vdev,
					 WMA_DELETE_BSS_HO_FAIL_REQ);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to set wma req msg_type for vdev_id: %d",
			vdev_id);
		goto fail_del_bss_ho_fail;
	}

	/* Try to use the vdev stop response path */
	resp_event.vdev_id = vdev_id;
	status = wma_handle_vdev_stop_rsp(wma, &resp_event);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to handle vdev stop rsp for vdev_id %d",
			vdev_id);
		goto fail_del_bss_ho_fail;
	}

	return;

fail_del_bss_ho_fail:
	vdev_stop_resp = qdf_mem_malloc(sizeof(*vdev_stop_resp));
	if (!vdev_stop_resp)
		return;

	vdev_stop_resp->vdev_id = vdev_id;
	vdev_stop_resp->status = status;
	wma_send_msg_high_priority(wma, WMA_DELETE_BSS_HO_FAIL_RSP,
				   (void *)vdev_stop_resp, 0);
}

/**
 * wma_wait_tx_complete() - Wait till tx packets are drained
 * @wma: wma handle
 * @session_id: vdev id
 *
 * Return: none
 */
static void wma_wait_tx_complete(tp_wma_handle wma,
				uint32_t session_id)
{
	uint8_t max_wait_iterations = 0, delay = 0;
	cdp_config_param_type val;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	QDF_STATUS status;

	if (!wma_is_vdev_valid(session_id)) {
		wma_err("Vdev is not valid: %d", session_id);
		return;
	}

	status = ucfg_mlme_get_delay_before_vdev_stop(wma->psoc, &delay);
	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to get delay before vdev stop");

	max_wait_iterations = delay / WMA_TX_Q_RECHECK_TIMER_WAIT;
	if (cdp_txrx_get_pdev_param(soc,
				    wlan_objmgr_pdev_get_pdev_id(wma->pdev),
				    CDP_TX_PENDING, &val))
		return;
	while (val.cdp_pdev_param_tx_pending && max_wait_iterations) {
		wma_warn("Waiting for outstanding packet to drain");
		qdf_wait_for_event_completion(&wma->tx_queue_empty_event,
				      WMA_TX_Q_RECHECK_TIMER_WAIT);
		if (cdp_txrx_get_pdev_param(
					soc,
					wlan_objmgr_pdev_get_pdev_id(wma->pdev),
					CDP_TX_PENDING, &val))
			return;
		max_wait_iterations--;
	}
}

void wma_delete_bss(tp_wma_handle wma, uint8_t vdev_id)
{
	bool peer_exist = false;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t tx_pending = 0;
	cdp_config_param_type val;
	bool roam_synch_in_progress = false;
	struct wma_txrx_node *iface;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct qdf_mac_addr bssid;
	struct del_bss_resp *params;
	uint8_t *addr, *bssid_addr;

	iface = &wma->interfaces[vdev_id];
	if (!iface || !iface->vdev) {
		wma_err("vdev id %d is already deleted", vdev_id);
		goto out;
	}

	status = mlme_get_vdev_bss_peer_mac_addr(iface->vdev, &bssid);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("vdev id %d : failed to get bssid", vdev_id);
		goto out;
	}

	addr = wlan_vdev_mlme_get_macaddr(iface->vdev);
	if (!addr) {
		wma_err("vdev id %d : failed to get macaddr", vdev_id);
		goto out;
	}

	if (WMA_IS_VDEV_IN_NDI_MODE(wma->interfaces,
			vdev_id))
		/* In ndi case, self mac is used to create the self peer */
		peer_exist = wma_cdp_find_peer_by_addr(addr);
	else
		peer_exist = wma_cdp_find_peer_by_addr(bssid.bytes);
	if (!peer_exist) {
		wma_err("Failed to find peer "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(bssid.bytes));
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}
	bssid_addr = wma_get_vdev_bssid(wma->interfaces[vdev_id].vdev);
	if (!bssid_addr) {
		wma_err("Failed to bssid for vdev_%d", vdev_id);
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}
	qdf_mem_zero(bssid_addr,
		     QDF_MAC_ADDR_SIZE);

	wma_delete_invalid_peer_entries(vdev_id, NULL);

	if (iface->psnr_req) {
		qdf_mem_free(iface->psnr_req);
		iface->psnr_req = NULL;
	}

	if (iface->rcpi_req) {
		struct sme_rcpi_req *rcpi_req = iface->rcpi_req;

		iface->rcpi_req = NULL;
		qdf_mem_free(rcpi_req);
	}

	if (iface->roam_scan_stats_req) {
		struct sir_roam_scan_stats *roam_scan_stats_req =
						iface->roam_scan_stats_req;

		iface->roam_scan_stats_req = NULL;
		qdf_mem_free(roam_scan_stats_req);
	}

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(wma->psoc, vdev_id)) {
		roam_synch_in_progress = true;
		wma_debug("LFR3: Setting vdev_up to FALSE for vdev:%d",
			  vdev_id);

		goto detach_peer;
	}

	status = mlme_set_vdev_stop_type(iface->vdev,
					 WMA_DELETE_BSS_REQ);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("Failed to set wma req msg_type for vdev_id: %d",
			vdev_id);
		goto out;
	}

	cdp_txrx_get_pdev_param(soc, wlan_objmgr_pdev_get_pdev_id(wma->pdev),
				CDP_TX_PENDING, &val);
	tx_pending = val.cdp_pdev_param_tx_pending;
	wma_debug("Outstanding msdu packets: %u", tx_pending);
	wma_wait_tx_complete(wma, vdev_id);

	cdp_txrx_get_pdev_param(soc, wlan_objmgr_pdev_get_pdev_id(wma->pdev),
				CDP_TX_PENDING, &val);
	if (tx_pending) {
		wma_debug("Outstanding msdu packets before VDEV_STOP : %u",
			 tx_pending);
	}

	wma_debug("vdev_id: %d, pausing tx_ll_queue for VDEV_STOP (del_bss)",
		 vdev_id);
	wma_vdev_set_pause_bit(vdev_id, PAUSE_TYPE_HOST);
	cdp_fc_vdev_pause(soc, vdev_id,
			  OL_TXQ_PAUSE_REASON_VDEV_STOP, 0);

	if (wma_send_vdev_stop_to_fw(wma, vdev_id)) {
		wma_err("Failed to send vdev stop");
		status = QDF_STATUS_E_FAILURE;
		qdf_atomic_set(&iface->bss_status, WMA_BSS_STATUS_STOPPED);
		goto detach_peer;
	}
	wma_debug("bssid "QDF_MAC_ADDR_FMT" vdev_id %d",
		  QDF_MAC_ADDR_REF(bssid.bytes), vdev_id);

	return;

detach_peer:
	wma_remove_peer(wma, bssid.bytes, vdev_id, roam_synch_in_progress);
	if (roam_synch_in_progress)
		return;

out:
	params = qdf_mem_malloc(sizeof(*params));
	if (!params)
		return;

	params->vdev_id = vdev_id;
	params->status = status;
	wma_send_msg_high_priority(wma, WMA_DELETE_BSS_RSP, params, 0);
}

/**
 * wma_find_ibss_vdev() - This function finds vdev_id based on input type
 * @wma: wma handle
 * @type: vdev type
 *
 * Return: vdev id
 */
int32_t wma_find_vdev_by_type(tp_wma_handle wma, int32_t type)
{
	int32_t vdev_id = 0;
	struct wma_txrx_node *intf = wma->interfaces;

	for (vdev_id = 0; vdev_id < wma->max_bssid; vdev_id++) {
		if (intf) {
			if (intf[vdev_id].type == type)
				return vdev_id;
		}
	}

	return -EFAULT;
}

void wma_set_vdev_intrabss_fwd(tp_wma_handle wma_handle,
				      tpDisableIntraBssFwd pdis_intra_fwd)
{
	struct wlan_objmgr_vdev *vdev;

	wma_debug("intra_fwd:vdev(%d) intrabss_dis=%s",
		 pdis_intra_fwd->sessionId,
		 (pdis_intra_fwd->disableintrabssfwd ? "true" : "false"));

	vdev = wma_handle->interfaces[pdis_intra_fwd->sessionId].vdev;
	cdp_cfg_vdev_rx_set_intrabss_fwd(cds_get_context(QDF_MODULE_ID_SOC),
					 pdis_intra_fwd->sessionId,
					 pdis_intra_fwd->disableintrabssfwd);
}

/**
 * wma_get_pdev_from_scn_handle() - API to get pdev from scn handle
 * @scn_handle: opaque wma handle
 *
 * API to get pdev from scn handle
 *
 * Return: None
 */
static struct wlan_objmgr_pdev *wma_get_pdev_from_scn_handle(void *scn_handle)
{
	tp_wma_handle wma_handle;

	if (!scn_handle) {
		wma_err("invalid scn handle");
		return NULL;
	}
	wma_handle = (tp_wma_handle)scn_handle;

	return wma_handle->pdev;
}

void wma_store_pdev(void *wma_ctx, struct wlan_objmgr_pdev *pdev)
{
	tp_wma_handle wma = (tp_wma_handle)wma_ctx;
	QDF_STATUS status;

	status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_LEGACY_WMA_ID);
	if (QDF_STATUS_SUCCESS != status) {
		wma->pdev = NULL;
		return;
	}

	wma->pdev = pdev;

	target_if_store_pdev_target_if_ctx(wma_get_pdev_from_scn_handle);
	target_pdev_set_wmi_handle(wma->pdev->tgt_if_handle,
				   wma->wmi_handle);
}

/**
 * wma_vdev_reset_beacon_interval_timer() - reset beacon interval back
 * to its original value after the channel switch.
 *
 * @data: data
 *
 * Return: void
 */
static void wma_vdev_reset_beacon_interval_timer(void *data)
{
	tp_wma_handle wma;
	struct wma_beacon_interval_reset_req *req =
		(struct wma_beacon_interval_reset_req *)data;
	uint16_t beacon_interval = req->interval;
	uint8_t vdev_id = req->vdev_id;

	wma = (tp_wma_handle)cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Failed to get wma");
		goto end;
	}

	/* Change the beacon interval back to its original value */
	wma_debug("Change beacon interval back to %d", beacon_interval);
	wma_update_beacon_interval(wma, vdev_id, beacon_interval);

end:
	qdf_timer_stop(&req->event_timeout);
	qdf_timer_free(&req->event_timeout);
	qdf_mem_free(req);
}

int wma_fill_beacon_interval_reset_req(tp_wma_handle wma, uint8_t vdev_id,
				uint16_t beacon_interval, uint32_t timeout)
{
	struct wma_beacon_interval_reset_req *req;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return -ENOMEM;

	wma_debug("vdev_id %d ", vdev_id);
	req->vdev_id = vdev_id;
	req->interval = beacon_interval;
	qdf_timer_init(NULL, &req->event_timeout,
		wma_vdev_reset_beacon_interval_timer, req, QDF_TIMER_TYPE_SW);
	qdf_timer_start(&req->event_timeout, timeout);

	return 0;
}

QDF_STATUS wma_set_wlm_latency_level(void *wma_ptr,
			struct wlm_latency_level_param *latency_params)
{
	QDF_STATUS ret;
	tp_wma_handle wma = (tp_wma_handle)wma_ptr;

	wma_debug("set latency level %d, flags flag 0x%x",
		 latency_params->wlm_latency_level,
		 latency_params->wlm_latency_flags);

	ret = wmi_unified_wlm_latency_level_cmd(wma->wmi_handle,
						latency_params);
	if (QDF_IS_STATUS_ERROR(ret))
		wma_warn("Failed to set latency level");

	return ret;
}

QDF_STATUS wma_add_bss_peer_sta(uint8_t vdev_id, uint8_t *bssid,
				bool is_resp_required)
{
	tp_wma_handle wma;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Invalid wma");
		goto err;
	}

	if (is_resp_required)
		status = wma_create_sta_mode_bss_peer(wma, bssid,
						      WMI_PEER_TYPE_DEFAULT,
						      vdev_id);
	else
		status = wma_create_peer(wma, bssid, WMI_PEER_TYPE_DEFAULT,
					 vdev_id);
err:
	return status;
}

QDF_STATUS wma_send_vdev_stop(uint8_t vdev_id)
{
	tp_wma_handle wma;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	QDF_STATUS status;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Invalid wma");
		return QDF_STATUS_E_FAILURE;
	}

	wma_debug("vdev_id: %d, pausing tx_ll_queue for VDEV_STOP", vdev_id);
	cdp_fc_vdev_pause(soc, vdev_id,
			  OL_TXQ_PAUSE_REASON_VDEV_STOP, 0);

	status = mlme_set_vdev_stop_type(
				wma->interfaces[vdev_id].vdev,
				WMA_SET_LINK_STATE);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_alert("Failed to set wma req msg_type for vdev_id %d",
			 vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	wma_vdev_set_pause_bit(vdev_id, PAUSE_TYPE_HOST);

	status = wma_send_vdev_stop_to_fw(wma, vdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		struct vdev_stop_response resp_event;

		wma_info("vdev %d Failed to send vdev stop", vdev_id);
		resp_event.vdev_id = vdev_id;
		mlme_set_connection_fail(wma->interfaces[vdev_id].vdev, false);
		wma_handle_vdev_stop_rsp(wma, &resp_event);
	}

	/*
	 * Remove peer, Vdev down and sending set link
	 * response will be handled in vdev stop response
	 * handler
	 */

	return QDF_STATUS_SUCCESS;
}
