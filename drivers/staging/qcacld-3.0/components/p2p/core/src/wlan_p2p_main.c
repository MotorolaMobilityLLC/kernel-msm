/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains main P2P function definitions
 */

#include <scheduler_api.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_scan_ucfg_api.h>
#include "wlan_p2p_public_struct.h"
#include "wlan_p2p_ucfg_api.h"
#include "wlan_p2p_tgt_api.h"
#include "wlan_p2p_main.h"
#include "wlan_p2p_roc.h"
#include "wlan_p2p_off_chan_tx.h"
#include "wlan_p2p_cfg.h"
#include "cfg_ucfg_api.h"

/**
 * p2p_get_cmd_type_str() - parse cmd to string
 * @cmd_type: P2P cmd type
 *
 * This function parse P2P cmd to string.
 *
 * Return: command string
 */
static char *p2p_get_cmd_type_str(enum p2p_cmd_type cmd_type)
{
	switch (cmd_type) {
	case P2P_ROC_REQ:
		return "P2P roc request";
	case P2P_CANCEL_ROC_REQ:
		return "P2P cancel roc request";
	case P2P_MGMT_TX:
		return "P2P mgmt tx request";
	case P2P_MGMT_TX_CANCEL:
		return "P2P cancel mgmt tx request";
	case P2P_CLEANUP_ROC:
		return "P2P cleanup roc";
	case P2P_CLEANUP_TX:
		return "P2P cleanup tx";
	case P2P_SET_RANDOM_MAC:
		return "P2P set random mac";
	default:
		return "Invalid P2P command";
	}
}

/**
 * p2p_get_event_type_str() - parase event to string
 * @event_type: P2P event type
 *
 * This function parse P2P event to string.
 *
 * Return: event string
 */
static char *p2p_get_event_type_str(enum p2p_event_type event_type)
{
	switch (event_type) {
	case P2P_EVENT_SCAN_EVENT:
		return "P2P scan event";
	case P2P_EVENT_MGMT_TX_ACK_CNF:
		return "P2P mgmt tx ack event";
	case P2P_EVENT_RX_MGMT:
		return "P2P mgmt rx event";
	case P2P_EVENT_LO_STOPPED:
		return "P2P lo stop event";
	case P2P_EVENT_NOA:
		return "P2P noa event";
	case P2P_EVENT_ADD_MAC_RSP:
		return "P2P add mac filter resp event";
	default:
		return "Invalid P2P event";
	}
}

/**
 * p2p_psoc_obj_create_notification() - Function to allocate per P2P
 * soc private object
 * @soc: soc context
 * @data: Pointer to data
 *
 * This function gets called from object manager when psoc is being
 * created and creates p2p soc context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_psoc_obj_create_notification(
	struct wlan_objmgr_psoc *soc, void *data)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	QDF_STATUS status;

	if (!soc) {
		p2p_err("psoc context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = qdf_mem_malloc(sizeof(*p2p_soc_obj));
	if (!p2p_soc_obj)
		return QDF_STATUS_E_NOMEM;

	p2p_soc_obj->soc = soc;

	status = wlan_objmgr_psoc_component_obj_attach(soc,
				WLAN_UMAC_COMP_P2P, p2p_soc_obj,
				QDF_STATUS_SUCCESS);
	if (status != QDF_STATUS_SUCCESS) {
		qdf_mem_free(p2p_soc_obj);
		p2p_err("Failed to attach p2p component, %d", status);
		return status;
	}

	p2p_debug("p2p soc object create successful, %pK", p2p_soc_obj);

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_psoc_obj_destroy_notification() - Free soc private object
 * @soc: soc context
 * @data: Pointer to data
 *
 * This function gets called from object manager when psoc is being
 * deleted and delete p2p soc context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_psoc_obj_destroy_notification(
	struct wlan_objmgr_psoc *soc, void *data)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	QDF_STATUS status;

	if (!soc) {
		p2p_err("psoc context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(soc,
			WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj) {
		p2p_err("p2p soc private object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	p2p_soc_obj->soc = NULL;

	status = wlan_objmgr_psoc_component_obj_detach(soc,
				WLAN_UMAC_COMP_P2P, p2p_soc_obj);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to detach p2p component, %d", status);
		return status;
	}

	p2p_debug("destroy p2p soc object, %pK", p2p_soc_obj);

	qdf_mem_free(p2p_soc_obj);

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_vdev_obj_create_notification() - Allocate per p2p vdev object
 * @vdev: vdev context
 * @data: Pointer to data
 *
 * This function gets called from object manager when vdev is being
 * created and creates p2p vdev context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_vdev_obj_create_notification(
	struct wlan_objmgr_vdev *vdev, void *data)
{
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	QDF_STATUS status;
	enum QDF_OPMODE mode;

	if (!vdev) {
		p2p_err("vdev context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	p2p_debug("vdev mode:%d", mode);
	if (mode != QDF_P2P_GO_MODE &&
	    mode != QDF_STA_MODE &&
	    mode != QDF_P2P_CLIENT_MODE &&
	    mode != QDF_P2P_DEVICE_MODE) {
		p2p_debug("won't create p2p vdev private object for mode %d",
			  mode);
		return QDF_STATUS_SUCCESS;
	}

	p2p_vdev_obj =
		qdf_mem_malloc(sizeof(*p2p_vdev_obj));
	if (!p2p_vdev_obj)
		return QDF_STATUS_E_NOMEM;

	p2p_vdev_obj->vdev = vdev;
	p2p_vdev_obj->noa_status = true;
	p2p_vdev_obj->non_p2p_peer_count = 0;
	p2p_init_random_mac_vdev(p2p_vdev_obj);

	status = wlan_objmgr_vdev_component_obj_attach(vdev,
				WLAN_UMAC_COMP_P2P, p2p_vdev_obj,
				QDF_STATUS_SUCCESS);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_deinit_random_mac_vdev(p2p_vdev_obj);
		qdf_mem_free(p2p_vdev_obj);
		p2p_err("Failed to attach p2p component to vdev, %d",
			status);
		return status;
	}

	p2p_debug("p2p vdev object create successful, %pK", p2p_vdev_obj);

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_vdev_obj_destroy_notification() - Free per P2P vdev object
 * @vdev: vdev context
 * @data: Pointer to data
 *
 * This function gets called from object manager when vdev is being
 * deleted and delete p2p vdev context.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_vdev_obj_destroy_notification(
	struct wlan_objmgr_vdev *vdev, void *data)
{
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	QDF_STATUS status;
	enum QDF_OPMODE mode;

	if (!vdev) {
		p2p_err("vdev context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	p2p_debug("vdev mode:%d", mode);
	if (mode != QDF_P2P_GO_MODE &&
	    mode != QDF_STA_MODE &&
	    mode != QDF_P2P_CLIENT_MODE &&
	    mode != QDF_P2P_DEVICE_MODE){
		p2p_debug("no p2p vdev private object for mode %d", mode);
		return QDF_STATUS_SUCCESS;
	}

	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(vdev,
			WLAN_UMAC_COMP_P2P);
	if (!p2p_vdev_obj) {
		p2p_debug("p2p vdev object is NULL");
		return QDF_STATUS_SUCCESS;
	}
	p2p_deinit_random_mac_vdev(p2p_vdev_obj);

	p2p_vdev_obj->vdev = NULL;

	status = wlan_objmgr_vdev_component_obj_detach(vdev,
				WLAN_UMAC_COMP_P2P, p2p_vdev_obj);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to detach p2p component, %d", status);
		return status;
	}

	p2p_debug("destroy p2p vdev object, p2p vdev obj:%pK, noa info:%pK",
		p2p_vdev_obj, p2p_vdev_obj->noa_info);

	if (p2p_vdev_obj->noa_info)
		qdf_mem_free(p2p_vdev_obj->noa_info);

	qdf_mem_free(p2p_vdev_obj);

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_peer_obj_create_notification() - manages peer details per vdev
 * @peer: peer object
 * @arg: Pointer to private argument - NULL
 *
 * This function gets called from object manager when peer is being
 * created.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_peer_obj_create_notification(
	struct wlan_objmgr_peer *peer, void *arg)
{
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE mode;

	if (!peer) {
		p2p_err("peer context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	vdev = wlan_peer_get_vdev(peer);
	mode = wlan_vdev_mlme_get_opmode(vdev);
	if (mode != QDF_P2P_GO_MODE)
		return QDF_STATUS_SUCCESS;

	p2p_debug("p2p peer object create successful");

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_peer_obj_destroy_notification() - clears peer details per vdev
 * @peer: peer object
 * @arg: Pointer to private argument - NULL
 *
 * This function gets called from object manager when peer is being
 * destroyed.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_peer_obj_destroy_notification(
	struct wlan_objmgr_peer *peer, void *arg)
{
	struct wlan_objmgr_vdev *vdev;
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	struct wlan_objmgr_psoc *psoc;
	enum QDF_OPMODE mode;
	enum wlan_peer_type peer_type;
	uint8_t vdev_id;

	if (!peer) {
		p2p_err("peer context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	vdev = wlan_peer_get_vdev(peer);
	mode = wlan_vdev_mlme_get_opmode(vdev);
	if (mode != QDF_P2P_GO_MODE)
		return QDF_STATUS_SUCCESS;

	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(vdev,
						WLAN_UMAC_COMP_P2P);
	psoc = wlan_vdev_get_psoc(vdev);
	if (!p2p_vdev_obj || !psoc) {
		p2p_debug("p2p_vdev_obj:%pK psoc:%pK", p2p_vdev_obj, psoc);
		return QDF_STATUS_E_INVAL;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);

	peer_type = wlan_peer_get_peer_type(peer);

	if ((peer_type == WLAN_PEER_STA) && (mode == QDF_P2P_GO_MODE)) {

		p2p_vdev_obj->non_p2p_peer_count--;

		if (!p2p_vdev_obj->non_p2p_peer_count &&
		    (p2p_vdev_obj->noa_status == false)) {

			vdev_id = wlan_vdev_get_id(vdev);

			if (ucfg_p2p_set_noa(psoc, vdev_id,
				 false)	== QDF_STATUS_SUCCESS)
				p2p_vdev_obj->noa_status = true;
			else
				p2p_vdev_obj->noa_status = false;

			p2p_debug("Non p2p peer disconnected from GO,NOA status: %d.",
				p2p_vdev_obj->noa_status);
		}
		p2p_debug("Non P2P peer count: %d",
			   p2p_vdev_obj->non_p2p_peer_count);
	}
	p2p_debug("p2p peer object destroy successful");

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_send_noa_to_pe() - send noa information to pe
 * @noa_info: vdev context
 *
 * This function sends noa information to pe since MCL layer need noa
 * event.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS p2p_send_noa_to_pe(struct p2p_noa_info *noa_info)
{
	struct p2p_noa_attr *noa_attr;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	if (!noa_info) {
		p2p_err("noa info is null");
		return QDF_STATUS_E_INVAL;
	}

	noa_attr = qdf_mem_malloc(sizeof(*noa_attr));
	if (!noa_attr)
		return QDF_STATUS_E_NOMEM;

	noa_attr->index = noa_info->index;
	noa_attr->opps_ps = noa_info->opps_ps;
	noa_attr->ct_win = noa_info->ct_window;
	if (!noa_info->num_desc) {
		p2p_debug("Zero noa descriptors");
	} else {
		p2p_debug("%d noa descriptors", noa_info->num_desc);

		noa_attr->noa1_count =
			noa_info->noa_desc[0].type_count;
		noa_attr->noa1_duration =
			noa_info->noa_desc[0].duration;
		noa_attr->noa1_interval =
			noa_info->noa_desc[0].interval;
		noa_attr->noa1_start_time =
			noa_info->noa_desc[0].start_time;
		if (noa_info->num_desc > 1) {
			noa_attr->noa2_count =
				noa_info->noa_desc[1].type_count;
			noa_attr->noa2_duration =
				noa_info->noa_desc[1].duration;
			noa_attr->noa2_interval =
				noa_info->noa_desc[1].interval;
			noa_attr->noa2_start_time =
				noa_info->noa_desc[1].start_time;
		}
	}

	p2p_debug("Sending P2P_NOA_ATTR_IND to pe");

	msg.type = P2P_NOA_ATTR_IND;
	msg.bodyval = 0;
	msg.bodyptr = noa_attr;
	status = scheduler_post_message(QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_P2P,
					QDF_MODULE_ID_PE,
					&msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(noa_attr);
		p2p_err("post msg fail:%d", status);
	}

	return status;
}

/**
 * process_peer_for_noa() - disable NoA
 * @vdev: vdev object
 * @psoc: soc object
 * @peer: peer object
 *
 * This function disables NoA
 *
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS process_peer_for_noa(struct wlan_objmgr_vdev *vdev,
				struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_peer *peer)
{
	struct p2p_vdev_priv_obj *p2p_vdev_obj = NULL;
	enum QDF_OPMODE mode;
	enum wlan_peer_type peer_type;
	bool disable_noa;
	uint8_t vdev_id;

	if (!vdev || !psoc || !peer) {
		p2p_err("vdev:%pK psoc:%pK peer:%pK", vdev, psoc, peer);
		return QDF_STATUS_E_INVAL;
	}
	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(vdev,
						WLAN_UMAC_COMP_P2P);
	if (!p2p_vdev_obj)
		return QDF_STATUS_E_INVAL;

	mode = wlan_vdev_mlme_get_opmode(vdev);

	peer_type = wlan_peer_get_peer_type(peer);
	if (peer_type == WLAN_PEER_STA)
		p2p_vdev_obj->non_p2p_peer_count++;

	disable_noa = ((mode == QDF_P2P_GO_MODE)
			&& p2p_vdev_obj->non_p2p_peer_count
			&& p2p_vdev_obj->noa_status);

	if (disable_noa && (peer_type == WLAN_PEER_STA)) {

		vdev_id = wlan_vdev_get_id(vdev);

		if (ucfg_p2p_set_noa(psoc, vdev_id,
				true) == QDF_STATUS_SUCCESS) {
			p2p_vdev_obj->noa_status = false;
		} else {
			p2p_vdev_obj->noa_status = true;
		}
		p2p_debug("NoA status: %d", p2p_vdev_obj->noa_status);
	}
	p2p_debug("process_peer_for_noa");

	return QDF_STATUS_SUCCESS;
}

/**
 * p2p_object_init_params() - init parameters for p2p object
 * @psoc:        pointer to psoc object
 * @p2p_soc_obj: pointer to p2p psoc object
 *
 * This function init parameters for p2p object
 */
static QDF_STATUS p2p_object_init_params(
	struct wlan_objmgr_psoc *psoc,
	struct p2p_soc_priv_obj *p2p_soc_obj)
{
	if (!psoc || !p2p_soc_obj) {
		p2p_err("invalid param");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj->param.go_keepalive_period =
			cfg_get(psoc, CFG_GO_KEEP_ALIVE_PERIOD);
	p2p_soc_obj->param.go_link_monitor_period =
			cfg_get(psoc, CFG_GO_LINK_MONITOR_PERIOD);
	p2p_soc_obj->param.p2p_device_addr_admin =
			cfg_get(psoc, CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED);
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_P2P_DEBUG
/**
 * wlan_p2p_init_connection_status() - init connection status
 * @p2p_soc_obj: pointer to p2p psoc object
 *
 * This function initial p2p connection status.
 *
 * Return: None
 */
static void wlan_p2p_init_connection_status(
		struct p2p_soc_priv_obj *p2p_soc_obj)
{
	if (!p2p_soc_obj) {
		p2p_err("invalid p2p soc obj");
		return;
	}

	p2p_soc_obj->connection_status = P2P_NOT_ACTIVE;
}
#else
static void wlan_p2p_init_connection_status(
		struct p2p_soc_priv_obj *p2p_soc_obj)
{
}
#endif /* WLAN_FEATURE_P2P_DEBUG */

QDF_STATUS p2p_component_init(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_register_psoc_create_handler(
				WLAN_UMAC_COMP_P2P,
				p2p_psoc_obj_create_notification,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to register p2p obj create handler");
		goto err_reg_psoc_create;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
				WLAN_UMAC_COMP_P2P,
				p2p_psoc_obj_destroy_notification,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to register p2p obj delete handler");
		goto err_reg_psoc_delete;
	}

	status = wlan_objmgr_register_vdev_create_handler(
				WLAN_UMAC_COMP_P2P,
				p2p_vdev_obj_create_notification,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to register p2p vdev create handler");
		goto err_reg_vdev_create;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(
				WLAN_UMAC_COMP_P2P,
				p2p_vdev_obj_destroy_notification,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to register p2p vdev delete handler");
		goto err_reg_vdev_delete;
	}

	status = wlan_objmgr_register_peer_create_handler(
				WLAN_UMAC_COMP_P2P,
				p2p_peer_obj_create_notification,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to register p2p peer create handler");
		goto err_reg_peer_create;
	}

	status = wlan_objmgr_register_peer_destroy_handler(
				WLAN_UMAC_COMP_P2P,
				p2p_peer_obj_destroy_notification,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to register p2p peer destroy handler");
		goto err_reg_peer_destroy;
	}

	p2p_debug("Register p2p obj handler successful");

	return QDF_STATUS_SUCCESS;
err_reg_peer_destroy:
	wlan_objmgr_unregister_peer_create_handler(WLAN_UMAC_COMP_P2P,
			p2p_peer_obj_create_notification, NULL);
err_reg_peer_create:
	wlan_objmgr_unregister_vdev_destroy_handler(WLAN_UMAC_COMP_P2P,
			p2p_vdev_obj_destroy_notification, NULL);
err_reg_vdev_delete:
	wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_P2P,
			p2p_vdev_obj_create_notification, NULL);
err_reg_vdev_create:
	wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_P2P,
			p2p_psoc_obj_destroy_notification, NULL);
err_reg_psoc_delete:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_P2P,
			p2p_psoc_obj_create_notification, NULL);
err_reg_psoc_create:
	return status;
}

QDF_STATUS p2p_component_deinit(void)
{
	QDF_STATUS status;
	QDF_STATUS ret_status = QDF_STATUS_SUCCESS;

	status = wlan_objmgr_unregister_vdev_create_handler(
				WLAN_UMAC_COMP_P2P,
				p2p_vdev_obj_create_notification,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to unregister p2p vdev create handler, %d",
			status);
		ret_status = status;
	}

	status = wlan_objmgr_unregister_vdev_destroy_handler(
				WLAN_UMAC_COMP_P2P,
				p2p_vdev_obj_destroy_notification,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to unregister p2p vdev delete handler, %d",
			status);
		ret_status = status;
	}

	status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_P2P,
				p2p_psoc_obj_create_notification,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to unregister p2p obj create handler, %d",
			status);
		ret_status = status;
	}

	status = wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_UMAC_COMP_P2P,
				p2p_psoc_obj_destroy_notification,
				NULL);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("Failed to unregister p2p obj delete handler, %d",
			status);
		ret_status = status;
	}

	p2p_debug("Unregister p2p obj handler complete");

	return ret_status;
}

QDF_STATUS p2p_psoc_object_open(struct wlan_objmgr_psoc *soc)
{
	QDF_STATUS status;
	struct p2p_soc_priv_obj *p2p_soc_obj;

	if (!soc) {
		p2p_err("psoc context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(soc,
			WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj) {
		p2p_err("p2p soc priviate object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	p2p_object_init_params(soc, p2p_soc_obj);
	qdf_list_create(&p2p_soc_obj->roc_q, MAX_QUEUE_LENGTH);
	qdf_list_create(&p2p_soc_obj->tx_q_roc, MAX_QUEUE_LENGTH);
	qdf_list_create(&p2p_soc_obj->tx_q_ack, MAX_QUEUE_LENGTH);

	status = qdf_event_create(&p2p_soc_obj->cleanup_roc_done);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("failed to create cleanup roc done event");
		goto fail_cleanup_roc;
	}

	status = qdf_event_create(&p2p_soc_obj->cleanup_tx_done);
	if (status != QDF_STATUS_SUCCESS) {
		p2p_err("failed to create cleanup roc done event");
		goto fail_cleanup_tx;
	}

	qdf_runtime_lock_init(&p2p_soc_obj->roc_runtime_lock);
	p2p_soc_obj->cur_roc_vdev_id = P2P_INVALID_VDEV_ID;
	qdf_idr_create(&p2p_soc_obj->p2p_idr);

	p2p_debug("p2p psoc object open successful");

	return QDF_STATUS_SUCCESS;

fail_cleanup_tx:
	qdf_event_destroy(&p2p_soc_obj->cleanup_roc_done);

fail_cleanup_roc:
	qdf_list_destroy(&p2p_soc_obj->tx_q_ack);
	qdf_list_destroy(&p2p_soc_obj->tx_q_roc);
	qdf_list_destroy(&p2p_soc_obj->roc_q);

	return status;
}

QDF_STATUS p2p_psoc_object_close(struct wlan_objmgr_psoc *soc)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;

	if (!soc) {
		p2p_err("psoc context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(soc,
			WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj) {
		p2p_err("p2p soc object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_idr_destroy(&p2p_soc_obj->p2p_idr);
	qdf_runtime_lock_deinit(&p2p_soc_obj->roc_runtime_lock);
	qdf_event_destroy(&p2p_soc_obj->cleanup_tx_done);
	qdf_event_destroy(&p2p_soc_obj->cleanup_roc_done);
	qdf_list_destroy(&p2p_soc_obj->tx_q_ack);
	qdf_list_destroy(&p2p_soc_obj->tx_q_roc);
	qdf_list_destroy(&p2p_soc_obj->roc_q);

	p2p_debug("p2p psoc object close successful");

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
static inline void p2p_init_lo_event(struct p2p_start_param *start_param,
				     struct p2p_start_param *req)
{
	start_param->lo_event_cb = req->lo_event_cb;
	start_param->lo_event_cb_data = req->lo_event_cb_data;
}
#else
static inline void p2p_init_lo_event(struct p2p_start_param *start_param,
				     struct p2p_start_param *req)
{
}
#endif

QDF_STATUS p2p_psoc_start(struct wlan_objmgr_psoc *soc,
	struct p2p_start_param *req)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_start_param *start_param;

	if (!soc) {
		p2p_err("psoc context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(soc,
			WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj) {
		p2p_err("P2P soc object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	start_param = qdf_mem_malloc(sizeof(*start_param));
	if (!start_param)
		return QDF_STATUS_E_NOMEM;

	start_param->rx_cb = req->rx_cb;
	start_param->rx_cb_data = req->rx_cb_data;
	start_param->event_cb = req->event_cb;
	start_param->event_cb_data = req->event_cb_data;
	start_param->tx_cnf_cb = req->tx_cnf_cb;
	start_param->tx_cnf_cb_data = req->tx_cnf_cb_data;
	p2p_init_lo_event(start_param, req);
	p2p_soc_obj->start_param = start_param;

	wlan_p2p_init_connection_status(p2p_soc_obj);

	/* register p2p lo stop and noa event */
	tgt_p2p_register_lo_ev_handler(soc);
	tgt_p2p_register_noa_ev_handler(soc);
	tgt_p2p_register_macaddr_rx_filter_evt_handler(soc, true);

	/* register scan request id */
	p2p_soc_obj->scan_req_id = ucfg_scan_register_requester(
		soc, P2P_MODULE_NAME, tgt_p2p_scan_event_cb,
		p2p_soc_obj);

	/* register rx action frame */
	p2p_mgmt_rx_action_ops(soc, true);

	p2p_debug("p2p psoc start successful, scan request id:%d",
		p2p_soc_obj->scan_req_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS p2p_psoc_stop(struct wlan_objmgr_psoc *soc)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_start_param *start_param;

	if (!soc) {
		p2p_err("psoc context passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(soc,
			WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj) {
		p2p_err("P2P soc object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	start_param = p2p_soc_obj->start_param;
	p2p_soc_obj->start_param = NULL;
	if (!start_param) {
		p2p_err("start parameters is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/* unregister rx action frame */
	p2p_mgmt_rx_action_ops(soc, false);

	/* clean up queue of p2p psoc private object */
	p2p_cleanup_tx_sync(p2p_soc_obj, NULL);
	p2p_cleanup_roc_sync(p2p_soc_obj, NULL);

	/* unrgister scan request id*/
	ucfg_scan_unregister_requester(soc, p2p_soc_obj->scan_req_id);

	/* unregister p2p lo stop and noa event */
	tgt_p2p_register_macaddr_rx_filter_evt_handler(soc, false);
	tgt_p2p_unregister_lo_ev_handler(soc);
	tgt_p2p_unregister_noa_ev_handler(soc);

	start_param->rx_cb = NULL;
	start_param->rx_cb_data = NULL;
	start_param->event_cb = NULL;
	start_param->event_cb_data = NULL;
	start_param->tx_cnf_cb = NULL;
	start_param->tx_cnf_cb_data = NULL;
	qdf_mem_free(start_param);

	p2p_debug("p2p psoc stop successful");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS p2p_process_cmd(struct scheduler_msg *msg)
{
	QDF_STATUS status;

	p2p_debug("msg type %d, %s", msg->type,
		p2p_get_cmd_type_str(msg->type));

	if (!(msg->bodyptr)) {
		p2p_err("Invalid message body");
		return QDF_STATUS_E_INVAL;
	}
	switch (msg->type) {
	case P2P_ROC_REQ:
		status = p2p_process_roc_req(
				(struct p2p_roc_context *)
				msg->bodyptr);
		break;
	case P2P_CANCEL_ROC_REQ:
		status = p2p_process_cancel_roc_req(
				(struct cancel_roc_context *)
				msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case P2P_MGMT_TX:
		status = p2p_process_mgmt_tx(
				(struct tx_action_context *)
				msg->bodyptr);
		break;
	case P2P_MGMT_TX_CANCEL:
		status = p2p_process_mgmt_tx_cancel(
				(struct cancel_roc_context *)
				msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case P2P_CLEANUP_ROC:
		status = p2p_process_cleanup_roc_queue(
				(struct p2p_cleanup_param *)
				msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case P2P_CLEANUP_TX:
		status = p2p_process_cleanup_tx_queue(
				(struct p2p_cleanup_param *)
				msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;
	case P2P_SET_RANDOM_MAC:
		status = p2p_process_set_rand_mac(msg->bodyptr);
		qdf_mem_free(msg->bodyptr);
		break;

	default:
		p2p_err("drop unexpected message received %d",
			msg->type);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

QDF_STATUS p2p_process_evt(struct scheduler_msg *msg)
{
	QDF_STATUS status;

	p2p_debug("msg type %d, %s", msg->type,
		p2p_get_event_type_str(msg->type));

	if (!(msg->bodyptr)) {
		p2p_err("Invalid message body");
		return QDF_STATUS_E_INVAL;
	}

	switch (msg->type) {
	case P2P_EVENT_MGMT_TX_ACK_CNF:
		status = p2p_process_mgmt_tx_ack_cnf(
				(struct p2p_tx_conf_event *)
				msg->bodyptr);
		break;
	case P2P_EVENT_RX_MGMT:
		status  = p2p_process_rx_mgmt(
				(struct p2p_rx_mgmt_event *)
				msg->bodyptr);
		break;
	case P2P_EVENT_LO_STOPPED:
		status = p2p_process_lo_stop(
				(struct p2p_lo_stop_event *)
				msg->bodyptr);
		break;
	case P2P_EVENT_NOA:
		status = p2p_process_noa(
				(struct p2p_noa_event *)
				msg->bodyptr);
		break;
	case P2P_EVENT_ADD_MAC_RSP:
		status = p2p_process_set_rand_mac_rsp(
				(struct p2p_mac_filter_rsp *)
				msg->bodyptr);
		break;
	default:
		p2p_err("Drop unexpected message received %d",
			msg->type);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	qdf_mem_free(msg->bodyptr);
	msg->bodyptr = NULL;

	return status;
}

QDF_STATUS p2p_msg_flush_callback(struct scheduler_msg *msg)
{
	struct tx_action_context *tx_action;

	if (!msg || !(msg->bodyptr)) {
		p2p_err("invalid msg");
		return QDF_STATUS_E_INVAL;
	}

	p2p_debug("flush msg, type:%d", msg->type);
	switch (msg->type) {
	case P2P_MGMT_TX:
		tx_action = (struct tx_action_context *)msg->bodyptr;
		qdf_mem_free(tx_action->buf);
		qdf_mem_free(tx_action);
		break;
	default:
		qdf_mem_free(msg->bodyptr);
		break;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS p2p_event_flush_callback(struct scheduler_msg *msg)
{
	struct p2p_noa_event *noa_event;
	struct p2p_rx_mgmt_event *rx_mgmt_event;
	struct p2p_tx_conf_event *tx_conf_event;
	struct p2p_lo_stop_event *lo_stop_event;

	if (!msg || !(msg->bodyptr)) {
		p2p_err("invalid msg");
		return QDF_STATUS_E_INVAL;
	}

	p2p_debug("flush event, type:%d", msg->type);
	switch (msg->type) {
	case P2P_EVENT_NOA:
		noa_event = (struct p2p_noa_event *)msg->bodyptr;
		qdf_mem_free(noa_event->noa_info);
		qdf_mem_free(noa_event);
		break;
	case P2P_EVENT_RX_MGMT:
		rx_mgmt_event = (struct p2p_rx_mgmt_event *)msg->bodyptr;
		qdf_mem_free(rx_mgmt_event->rx_mgmt);
		qdf_mem_free(rx_mgmt_event);
		break;
	case P2P_EVENT_MGMT_TX_ACK_CNF:
		tx_conf_event = (struct p2p_tx_conf_event *)msg->bodyptr;
		qdf_nbuf_free(tx_conf_event->nbuf);
		qdf_mem_free(tx_conf_event);
		break;
	case P2P_EVENT_LO_STOPPED:
		lo_stop_event = (struct p2p_lo_stop_event *)msg->bodyptr;
		qdf_mem_free(lo_stop_event->lo_event);
		qdf_mem_free(lo_stop_event);
		break;
	default:
		qdf_mem_free(msg->bodyptr);
		break;
	}

	return QDF_STATUS_SUCCESS;
}

bool p2p_check_oui_and_force_1x1(uint8_t *assoc_ie, uint32_t assoc_ie_len)
{
	const uint8_t *vendor_ie, *p2p_ie, *pos;
	uint8_t rem_len, attr;
	uint16_t attr_len;

	vendor_ie = (uint8_t *)p2p_get_p2pie_ptr(assoc_ie, assoc_ie_len);
	if (!vendor_ie) {
		p2p_debug("P2P IE not found");
		return false;
	}

	rem_len = vendor_ie[1];
	if (rem_len < (2 + OUI_SIZE_P2P) || rem_len > WLAN_MAX_IE_LEN) {
		p2p_err("Invalid IE len %d", rem_len);
		return false;
	}

	p2p_ie = vendor_ie + HEADER_LEN_P2P_IE;
	rem_len -= OUI_SIZE_P2P;

	while (rem_len) {
		attr = p2p_ie[0];
		attr_len = LE_READ_2(&p2p_ie[1]);
		if (attr_len > rem_len)  {
			p2p_err("Invalid len %d for elem:%d", attr_len, attr);
			return false;
		}

		switch (attr) {
		case P2P_ATTR_CAPABILITY:
		case P2P_ATTR_DEVICE_ID:
		case P2P_ATTR_GROUP_OWNER_INTENT:
		case P2P_ATTR_STATUS:
		case P2P_ATTR_LISTEN_CHANNEL:
		case P2P_ATTR_OPERATING_CHANNEL:
		case P2P_ATTR_GROUP_INFO:
		case P2P_ATTR_MANAGEABILITY:
		case P2P_ATTR_CHANNEL_LIST:
			break;

		case P2P_ATTR_DEVICE_INFO:
			if (attr_len < (QDF_MAC_ADDR_SIZE +
					MAX_CONFIG_METHODS_LEN + 8 +
					DEVICE_CATEGORY_MAX_LEN)) {
				p2p_err("Invalid Device info attr len %d",
					attr_len);
				return false;
			}

			/* move by attr id and 2 bytes of attr len */
			pos = p2p_ie + 3;

			/*
			 * the P2P Device info is of format:
			 * attr_id - 1 byte
			 * attr_len - 2 bytes
			 * device mac addr - 6 bytes
			 * config methods - 2 bytes
			 * primary device type - 8bytes
			 *  -primary device type category - 1 byte
			 *  -primary device type oui - 4bytes
			 * number of secondary device type - 2 bytes
			 */
			pos += ETH_ALEN + MAX_CONFIG_METHODS_LEN +
			       DEVICE_CATEGORY_MAX_LEN;

			if (!qdf_mem_cmp(pos, P2P_1X1_WAR_OUI,
					 P2P_1X1_OUI_LEN))
				return true;

			break;
		default:
			p2p_err("Invalid P2P attribute");
			break;
		}
		p2p_ie += (3 + attr_len);
		rem_len -= (3 + attr_len);
	}

	return false;
}

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
QDF_STATUS p2p_process_lo_stop(
	struct p2p_lo_stop_event *lo_stop_event)
{
	struct p2p_lo_event *lo_evt;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct p2p_start_param *start_param;

	if (!lo_stop_event) {
		p2p_err("invalid lo stop event");
		return QDF_STATUS_E_INVAL;
	}

	lo_evt = lo_stop_event->lo_event;
	if (!lo_evt) {
		p2p_err("invalid lo event");
		return QDF_STATUS_E_INVAL;
	}

	p2p_soc_obj = lo_stop_event->p2p_soc_obj;

	p2p_debug("vdev_id %d, reason %d",
		lo_evt->vdev_id, lo_evt->reason_code);

	if (!p2p_soc_obj || !(p2p_soc_obj->start_param)) {
		p2p_err("Invalid p2p soc object or start parameters");
		qdf_mem_free(lo_evt);
		return QDF_STATUS_E_INVAL;
	}
	start_param = p2p_soc_obj->start_param;
	if (start_param->lo_event_cb)
		start_param->lo_event_cb(
			start_param->lo_event_cb_data, lo_evt);
	else
		p2p_err("Invalid p2p soc obj or hdd lo event callback");

	qdf_mem_free(lo_evt);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS p2p_process_noa(struct p2p_noa_event *noa_event)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct p2p_noa_info *noa_info;
	struct p2p_vdev_priv_obj *p2p_vdev_obj;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	enum QDF_OPMODE mode;

	if (!noa_event) {
		p2p_err("invalid noa event");
		return QDF_STATUS_E_INVAL;
	}
	noa_info = noa_event->noa_info;
	p2p_soc_obj = noa_event->p2p_soc_obj;
	psoc = p2p_soc_obj->soc;

	p2p_debug("psoc:%pK, index:%d, opps_ps:%d, ct_window:%d, num_desc:%d, vdev_id:%d",
		psoc, noa_info->index, noa_info->opps_ps,
		noa_info->ct_window, noa_info->num_desc,
		noa_info->vdev_id);

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
			noa_info->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		p2p_err("vdev obj is NULL");
		qdf_mem_free(noa_event->noa_info);
		return QDF_STATUS_E_INVAL;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	p2p_debug("vdev mode:%d", mode);
	if (mode != QDF_P2P_GO_MODE) {
		p2p_err("invalid p2p vdev mode:%d", mode);
		status = QDF_STATUS_E_INVAL;
		goto fail;
	}

	/* must send noa to pe since of limitation*/
	p2p_send_noa_to_pe(noa_info);

	p2p_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(vdev,
			WLAN_UMAC_COMP_P2P);
	if (!(p2p_vdev_obj->noa_info)) {
		p2p_vdev_obj->noa_info =
			qdf_mem_malloc(sizeof(struct p2p_noa_info));
		if (!(p2p_vdev_obj->noa_info)) {
			status = QDF_STATUS_E_NOMEM;
			goto fail;
		}
	}
	qdf_mem_copy(p2p_vdev_obj->noa_info, noa_info,
		sizeof(struct p2p_noa_info));
fail:
	qdf_mem_free(noa_event->noa_info);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);

	return status;
}

void p2p_peer_authorized(struct wlan_objmgr_vdev *vdev, uint8_t *mac_addr)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_peer *peer;
	uint8_t pdev_id;

	if (!vdev) {
		p2p_err("vdev:%pK", vdev);
		return;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		p2p_err("psoc:%pK", psoc);
		return;
	}
	pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	peer = wlan_objmgr_get_peer(psoc, pdev_id,  mac_addr, WLAN_P2P_ID);
	if (!peer) {
		p2p_debug("peer info not found");
		return;
	}
	status = process_peer_for_noa(vdev, psoc, peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_P2P_ID);

	if (status != QDF_STATUS_SUCCESS)
		return;

	p2p_debug("peer is authorized");
}

#ifdef WLAN_FEATURE_P2P_DEBUG
static struct p2p_soc_priv_obj *
get_p2p_soc_obj_by_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	struct wlan_objmgr_psoc *soc;

	if (!vdev) {
		p2p_err("vdev context passed is NULL");
		return NULL;
	}

	soc = wlan_vdev_get_psoc(vdev);
	if (!soc) {
		p2p_err("soc context is NULL");
		return NULL;
	}

	p2p_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(soc,
			WLAN_UMAC_COMP_P2P);
	if (!p2p_soc_obj)
		p2p_err("P2P soc context is NULL");

	return p2p_soc_obj;
}

QDF_STATUS p2p_status_scan(struct wlan_objmgr_vdev *vdev)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	enum QDF_OPMODE mode;

	p2p_soc_obj = get_p2p_soc_obj_by_vdev(vdev);
	if (!p2p_soc_obj) {
		p2p_err("P2P soc context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	if (mode != QDF_P2P_CLIENT_MODE &&
	    mode != QDF_P2P_DEVICE_MODE) {
		p2p_debug("this is not P2P CLIENT or DEVICE, mode:%d",
			mode);
		return QDF_STATUS_SUCCESS;
	}

	p2p_debug("connection status:%d", p2p_soc_obj->connection_status);
	switch (p2p_soc_obj->connection_status) {
	case P2P_GO_NEG_COMPLETED:
	case P2P_GO_NEG_PROCESS:
		p2p_soc_obj->connection_status =
				P2P_CLIENT_CONNECTING_STATE_1;
		p2p_debug("[P2P State] Changing state from Go nego completed to Connection is started");
		p2p_debug("P2P Scanning is started for 8way Handshake");
		break;
	case P2P_CLIENT_DISCONNECTED_STATE:
		p2p_soc_obj->connection_status =
				P2P_CLIENT_CONNECTING_STATE_2;
		p2p_debug("[P2P State] Changing state from Disconnected state to Connection is started");
		p2p_debug("P2P Scanning is started for 4way Handshake");
		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS p2p_status_connect(struct wlan_objmgr_vdev *vdev)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	enum QDF_OPMODE mode;

	p2p_soc_obj = get_p2p_soc_obj_by_vdev(vdev);
	if (!p2p_soc_obj) {
		p2p_err("P2P soc context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	if (mode != QDF_P2P_CLIENT_MODE)
		return QDF_STATUS_SUCCESS;

	p2p_debug("connection status:%d", p2p_soc_obj->connection_status);
	switch (p2p_soc_obj->connection_status) {
	case P2P_CLIENT_CONNECTING_STATE_1:
		p2p_soc_obj->connection_status =
				P2P_CLIENT_CONNECTED_STATE_1;
		p2p_debug("[P2P State] Changing state from Connecting state to Connected State for 8-way Handshake");
		break;
	case P2P_CLIENT_DISCONNECTED_STATE:
		p2p_debug("No scan before 4-way handshake");
		/*
		 * since no scan before 4-way handshake and
		 * won't enter state P2P_CLIENT_CONNECTING_STATE_2:
		 */
		/* fallthrough */
	case P2P_CLIENT_CONNECTING_STATE_2:
		p2p_soc_obj->connection_status =
				P2P_CLIENT_COMPLETED_STATE;
		p2p_debug("[P2P State] Changing state from Connecting state to P2P Client Connection Completed");
		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS p2p_status_disconnect(struct wlan_objmgr_vdev *vdev)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	enum QDF_OPMODE mode;

	p2p_soc_obj = get_p2p_soc_obj_by_vdev(vdev);
	if (!p2p_soc_obj) {
		p2p_err("P2P soc context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	if (mode != QDF_P2P_CLIENT_MODE)
		return QDF_STATUS_SUCCESS;

	p2p_debug("connection status:%d", p2p_soc_obj->connection_status);
	switch (p2p_soc_obj->connection_status) {
	case P2P_CLIENT_CONNECTED_STATE_1:
		p2p_soc_obj->connection_status =
				P2P_CLIENT_DISCONNECTED_STATE;
		p2p_debug("[P2P State] 8 way Handshake completed and moved to disconnected state");
		break;
	case P2P_CLIENT_COMPLETED_STATE:
		p2p_soc_obj->connection_status = P2P_NOT_ACTIVE;
		p2p_debug("[P2P State] P2P Client is removed and moved to inactive state");
		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS p2p_status_start_bss(struct wlan_objmgr_vdev *vdev)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	enum QDF_OPMODE mode;

	p2p_soc_obj = get_p2p_soc_obj_by_vdev(vdev);
	if (!p2p_soc_obj) {
		p2p_err("P2P soc context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	if (mode != QDF_P2P_GO_MODE) {
		p2p_debug("this is not P2P GO, mode:%d", mode);
		return QDF_STATUS_SUCCESS;
	}

	p2p_debug("connection status:%d", p2p_soc_obj->connection_status);
	switch (p2p_soc_obj->connection_status) {
	case P2P_GO_NEG_COMPLETED:
		p2p_soc_obj->connection_status =
				P2P_GO_COMPLETED_STATE;
		p2p_debug("[P2P State] From Go nego completed to Non-autonomous Group started");
		break;
	case P2P_NOT_ACTIVE:
		p2p_soc_obj->connection_status =
				P2P_GO_COMPLETED_STATE;
		p2p_debug("[P2P State] From Inactive to Autonomous Group started");
		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS p2p_status_stop_bss(struct wlan_objmgr_vdev *vdev)
{
	struct p2p_soc_priv_obj *p2p_soc_obj;
	enum QDF_OPMODE mode;

	p2p_soc_obj = get_p2p_soc_obj_by_vdev(vdev);
	if (!p2p_soc_obj) {
		p2p_err("P2P soc context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mode = wlan_vdev_mlme_get_opmode(vdev);
	if (mode != QDF_P2P_GO_MODE) {
		p2p_debug("this is not P2P GO, mode:%d", mode);
		return QDF_STATUS_SUCCESS;
	}

	p2p_debug("connection status:%d", p2p_soc_obj->connection_status);
	if (p2p_soc_obj->connection_status == P2P_GO_COMPLETED_STATE) {
		p2p_soc_obj->connection_status = P2P_NOT_ACTIVE;
		p2p_debug("[P2P State] From GO completed to Inactive state GO got removed");
	}

	return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_FEATURE_P2P_DEBUG */
