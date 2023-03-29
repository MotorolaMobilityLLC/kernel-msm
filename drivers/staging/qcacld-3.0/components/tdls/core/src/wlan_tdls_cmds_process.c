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
 * DOC: wlan_tdls_cmds_process.c
 *
 * TDLS north bound commands implementation
 */
#include <qdf_types.h>
#include <qdf_status.h>
#include <wlan_cmn.h>
#include <reg_services_public_struct.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_reg_services_api.h>
#include <wlan_serialization_api.h>
#include "wlan_tdls_main.h"
#include "wlan_tdls_peer.h"
#include "wlan_tdls_ct.h"
#include "wlan_tdls_mgmt.h"
#include "wlan_tdls_cmds_process.h"
#include "wlan_tdls_tgt_api.h"
#include "wlan_policy_mgr_api.h"
#include "nan_ucfg_api.h"

static uint16_t tdls_get_connected_peer(struct tdls_soc_priv_obj *soc_obj)
{
	return soc_obj->connected_peer_count;
}

/**
 * tdls_decrement_peer_count() - decrement connected TDLS peer counter
 * @soc_obj: TDLS soc object
 *
 * Used in scheduler thread context, no lock needed.
 *
 * Return: None.
 */
void tdls_decrement_peer_count(struct tdls_soc_priv_obj *soc_obj)
{
	if (soc_obj->connected_peer_count)
		soc_obj->connected_peer_count--;

	tdls_debug("Connected peer count %d", soc_obj->connected_peer_count);
}

/**
 * tdls_increment_peer_count() - increment connected TDLS peer counter
 * @soc_obj: TDLS soc object
 *
 * Used in scheduler thread context, no lock needed.
 *
 * Return: None.
 */
static void tdls_increment_peer_count(struct tdls_soc_priv_obj *soc_obj)
{
	soc_obj->connected_peer_count++;
	tdls_debug("Connected peer count %d", soc_obj->connected_peer_count);
}

/**
 * tdls_validate_current_mode() - check current TDL mode
 * @soc_obj: TDLS soc object
 *
 * Return: QDF_STATUS_SUCCESS if TDLS enabled, other for disabled
 */
static QDF_STATUS tdls_validate_current_mode(struct tdls_soc_priv_obj *soc_obj)
{
	if (soc_obj->tdls_current_mode == TDLS_SUPPORT_DISABLED ||
	    soc_obj->tdls_current_mode == TDLS_SUPPORT_SUSPENDED) {
		tdls_err("TDLS mode disabled OR not enabled, current mode %d",
			 soc_obj->tdls_current_mode);
		return QDF_STATUS_E_NOSUPPORT;
	}
	return QDF_STATUS_SUCCESS;
}

static char *tdls_get_ser_cmd_str(enum  wlan_serialization_cmd_type type)
{
	switch (type) {
	case WLAN_SER_CMD_TDLS_ADD_PEER:
		return "TDLS_ADD_PEER_CMD";
	case WLAN_SER_CMD_TDLS_DEL_PEER:
		return "TDLS_DEL_PEER_CMD";
	case WLAN_SER_CMD_TDLS_SEND_MGMT:
		return "TDLS_SEND_MGMT_CMD";
	default:
		return "UNKNOWN";
	}
}

void
tdls_release_serialization_command(struct wlan_objmgr_vdev *vdev,
				   enum wlan_serialization_cmd_type type)
{
	struct wlan_serialization_queued_cmd_info cmd = {0};

	cmd.cmd_type = type;
	cmd.cmd_id = 0;
	cmd.vdev = vdev;

	tdls_debug("release %s", tdls_get_ser_cmd_str(type));
	/* Inform serialization for command completion */
	wlan_serialization_remove_cmd(&cmd);
}

/**
 * tdls_pe_add_peer() - send TDLS add peer request to PE
 * @req: TDL add peer request
 *
 * Return: QDF_STATUS_SUCCESS for success; other values if failed
 */
static QDF_STATUS tdls_pe_add_peer(struct tdls_add_peer_request *req)
{
	struct tdls_add_sta_req *addstareq;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *peer;
	struct tdls_soc_priv_obj *soc_obj;
	struct scheduler_msg msg = {0,};
	QDF_STATUS status;

	addstareq = qdf_mem_malloc(sizeof(*addstareq));
	if (!addstareq)
		return QDF_STATUS_E_NOMEM;

	vdev = req->vdev;
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (!soc_obj) {
		tdls_err("NULL tdls soc object");
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	addstareq->tdls_oper = TDLS_OPER_ADD;
	addstareq->transaction_id = 0;

	addstareq->session_id = wlan_vdev_get_id(vdev);
	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_TDLS_NB_ID);
	if (!peer) {
		tdls_err("bss peer is NULL");
		status = QDF_STATUS_E_INVAL;
		goto error;
	}
	wlan_peer_obj_lock(peer);
	qdf_mem_copy(addstareq->bssid.bytes,
		     wlan_peer_get_macaddr(peer), QDF_MAC_ADDR_SIZE);
	wlan_peer_obj_unlock(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_TDLS_NB_ID);
	qdf_mem_copy(addstareq->peermac.bytes, req->add_peer_req.peer_addr,
		     QDF_MAC_ADDR_SIZE);

	tdls_debug("for " QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(addstareq->peermac.bytes));
	msg.type = soc_obj->tdls_add_sta_req;
	msg.bodyptr = addstareq;
	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("fail to post pe msg to add peer");
		goto error;
	}
	return status;
error:
	qdf_mem_free(addstareq);
	return status;
}

/**
 * tdls_pe_del_peer() - send TDLS delete peer request to PE
 * @req: TDLS delete peer request
 *
 * Return: QDF_STATUS_SUCCESS for success; other values if failed
 */
QDF_STATUS tdls_pe_del_peer(struct tdls_del_peer_request *req)
{
	struct tdls_del_sta_req *delstareq;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *peer;
	struct tdls_soc_priv_obj *soc_obj;
	struct scheduler_msg msg = {0,};
	QDF_STATUS status;

	delstareq = qdf_mem_malloc(sizeof(*delstareq));
	if (!delstareq)
		return QDF_STATUS_E_NOMEM;

	vdev = req->vdev;
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (!soc_obj) {
		tdls_err("NULL tdls soc object");
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	delstareq->transaction_id = 0;

	delstareq->session_id = wlan_vdev_get_id(vdev);
	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_TDLS_NB_ID);
	if (!peer) {
		tdls_err("bss peer is NULL");
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	wlan_peer_obj_lock(peer);
	qdf_mem_copy(delstareq->bssid.bytes,
		     wlan_peer_get_macaddr(peer), QDF_MAC_ADDR_SIZE);
	wlan_peer_obj_unlock(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_TDLS_NB_ID);
	qdf_mem_copy(delstareq->peermac.bytes, req->del_peer_req.peer_addr,
		     QDF_MAC_ADDR_SIZE);

	tdls_debug("for " QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(delstareq->peermac.bytes));
	msg.type = soc_obj->tdls_del_sta_req;
	msg.bodyptr = delstareq;
	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("fail to post pe msg to del peer");
		goto error;
	}
	return status;
error:
	qdf_mem_free(delstareq);
	return status;
}

#ifdef WLAN_FEATURE_11AX
static void tdls_pe_update_peer_6ghz_capa(struct tdls_add_sta_req *addstareq,
				struct tdls_update_peer_params *update_peer)
{
	qdf_mem_copy(&addstareq->he_6ghz_cap, &update_peer->he_6ghz_cap,
		     sizeof(update_peer->he_6ghz_cap));
}

static void tdls_pe_update_peer_he_capa(struct tdls_add_sta_req *addstareq,
				struct tdls_update_peer_params *update_peer)
{
	addstareq->he_cap_len = update_peer->he_cap_len;
	qdf_mem_copy(&addstareq->he_cap,
		     &update_peer->he_cap,
		     sizeof(update_peer->he_cap));

	tdls_pe_update_peer_6ghz_capa(addstareq, update_peer);
}
#else
static void tdls_pe_update_peer_he_capa(struct tdls_add_sta_req *addstareq,
				struct tdls_update_peer_params *update_peer)
{
}
#endif

/**
 * tdls_pe_update_peer() - send TDLS update peer request to PE
 * @req: TDLS update peer request
 *
 * Return: QDF_STATUS_SUCCESS for success; other values if failed
 */
static QDF_STATUS tdls_pe_update_peer(struct tdls_update_peer_request *req)
{
	struct tdls_add_sta_req *addstareq;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_peer *peer;
	struct tdls_soc_priv_obj *soc_obj;
	struct scheduler_msg msg = {0,};
	struct tdls_update_peer_params *update_peer;
	QDF_STATUS status;

	addstareq = qdf_mem_malloc(sizeof(*addstareq));
	if (!addstareq)
		return QDF_STATUS_E_NOMEM;

	vdev = req->vdev;
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (!soc_obj) {
		tdls_err("NULL tdls soc object");
		status = QDF_STATUS_E_INVAL;
		goto error;
	}
	update_peer = &req->update_peer_req;

	addstareq->tdls_oper = TDLS_OPER_UPDATE;
	addstareq->transaction_id = 0;

	addstareq->session_id = wlan_vdev_get_id(vdev);
	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_TDLS_NB_ID);
	if (!peer) {
		tdls_err("bss peer is NULL");
		status = QDF_STATUS_E_INVAL;
		goto error;
	}
	wlan_peer_obj_lock(peer);
	qdf_mem_copy(addstareq->bssid.bytes,
		     wlan_peer_get_macaddr(peer), QDF_MAC_ADDR_SIZE);
	wlan_peer_obj_unlock(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_TDLS_NB_ID);
	qdf_mem_copy(addstareq->peermac.bytes, update_peer->peer_addr,
		     QDF_MAC_ADDR_SIZE);
	addstareq->capability = update_peer->capability;
	addstareq->uapsd_queues = update_peer->uapsd_queues;
	addstareq->max_sp = update_peer->max_sp;

	qdf_mem_copy(addstareq->extn_capability,
		     update_peer->extn_capability, WLAN_MAC_MAX_EXTN_CAP);
	addstareq->htcap_present = update_peer->htcap_present;
	qdf_mem_copy(&addstareq->ht_cap,
		     &update_peer->ht_cap,
		     sizeof(update_peer->ht_cap));
	addstareq->vhtcap_present = update_peer->vhtcap_present;
	qdf_mem_copy(&addstareq->vht_cap,
		     &update_peer->vht_cap,
		     sizeof(update_peer->vht_cap));
	tdls_pe_update_peer_he_capa(addstareq, update_peer);
	addstareq->supported_rates_length = update_peer->supported_rates_len;
	addstareq->is_pmf = update_peer->is_pmf;
	qdf_mem_copy(&addstareq->supported_rates,
		     update_peer->supported_rates,
		     update_peer->supported_rates_len);
	tdls_debug("for " QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(addstareq->peermac.bytes));

	msg.type = soc_obj->tdls_add_sta_req;
	msg.bodyptr = addstareq;
	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("fail to post pe msg to update peer");
		goto error;
	}
	return status;
error:
	qdf_mem_free(addstareq);
	return status;
}

static QDF_STATUS
tdls_internal_add_peer_rsp(struct tdls_add_peer_request *req,
			   QDF_STATUS status)
{
	struct tdls_soc_priv_obj *soc_obj;
	struct wlan_objmgr_vdev *vdev;
	struct tdls_osif_indication ind;
	QDF_STATUS ret;

	if (!req || !req->vdev) {
		tdls_err("req: %pK", req);
		return QDF_STATUS_E_INVAL;
	}
	vdev = req->vdev;
	ret = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_SB_ID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		tdls_err("can't get vdev object");
		return ret;
	}

	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (soc_obj && soc_obj->tdls_event_cb) {
		ind.vdev = vdev;
		ind.status = status;
		soc_obj->tdls_event_cb(soc_obj->tdls_evt_cb_data,
				       TDLS_EVENT_ADD_PEER, &ind);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
tdls_internal_update_peer_rsp(struct tdls_update_peer_request *req,
			      QDF_STATUS status)
{
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_osif_indication ind;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS ret;

	if (!req || !req->vdev) {
		tdls_err("req: %pK", req);
		return QDF_STATUS_E_INVAL;
	}
	vdev = req->vdev;
	ret = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_SB_ID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		tdls_err("can't get vdev object");
		return ret;
	}

	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (soc_obj && soc_obj->tdls_event_cb) {
		ind.vdev = vdev;
		ind.status = status;
		soc_obj->tdls_event_cb(soc_obj->tdls_evt_cb_data,
				       TDLS_EVENT_ADD_PEER, &ind);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS tdls_internal_del_peer_rsp(struct tdls_oper_request *req)
{
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_osif_indication ind;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	if (!req || !req->vdev) {
		tdls_err("req: %pK", req);
		return QDF_STATUS_E_INVAL;
	}
	vdev = req->vdev;
	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_TDLS_NB_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev object");
		return status;
	}

	soc_obj = wlan_vdev_get_tdls_soc_obj(req->vdev);
	if (soc_obj && soc_obj->tdls_event_cb) {
		ind.vdev = req->vdev;
		soc_obj->tdls_event_cb(soc_obj->tdls_evt_cb_data,
				       TDLS_EVENT_DEL_PEER, &ind);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS tdls_activate_add_peer(struct tdls_add_peer_request *req)
{
	QDF_STATUS status;
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct tdls_peer *peer;
	uint16_t curr_tdls_peers;
	const uint8_t *mac;
	struct tdls_osif_indication ind;

	if (!req->vdev) {
		tdls_err("vdev null when add tdls peer");
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	mac = req->add_peer_req.peer_addr;
	soc_obj = wlan_vdev_get_tdls_soc_obj(req->vdev);
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(req->vdev);

	if (!soc_obj || !vdev_obj) {
		tdls_err("soc_obj: %pK, vdev_obj: %pK", soc_obj, vdev_obj);
		return QDF_STATUS_E_INVAL;
	}
	status = tdls_validate_current_mode(soc_obj);
	if (QDF_IS_STATUS_ERROR(status))
		goto addrsp;

	peer = tdls_get_peer(vdev_obj, mac);
	if (!peer) {
		tdls_err("peer: " QDF_MAC_ADDR_FMT " not exist. invalid",
			 QDF_MAC_ADDR_REF(mac));
		status = QDF_STATUS_E_INVAL;
		goto addrsp;
	}

	/* in add station, we accept existing valid sta_id if there is */
	if ((peer->link_status > TDLS_LINK_CONNECTING) ||
	    (peer->valid_entry)) {
		tdls_notice("link_status %d add peer ignored",
			    peer->link_status);
		status = QDF_STATUS_SUCCESS;
		goto addrsp;
	}

	/* when others are on-going, we want to change link_status to idle */
	if (tdls_is_progress(vdev_obj, mac, true)) {
		tdls_notice(QDF_MAC_ADDR_FMT " TDLS setuping. Req declined.",
			    QDF_MAC_ADDR_REF(mac));
		status = QDF_STATUS_E_PERM;
		goto setlink;
	}

	/* first to check if we reached to maximum supported TDLS peer. */
	curr_tdls_peers = tdls_get_connected_peer(soc_obj);
	if (soc_obj->max_num_tdls_sta <= curr_tdls_peers) {
		tdls_err(QDF_MAC_ADDR_FMT
			 " Request declined. Current %d, Max allowed %d.",
			 QDF_MAC_ADDR_REF(mac), curr_tdls_peers,
			 soc_obj->max_num_tdls_sta);
		status = QDF_STATUS_E_PERM;
		goto setlink;
	}

	tdls_set_peer_link_status(peer,
				  TDLS_LINK_CONNECTING, TDLS_LINK_SUCCESS);

	status = tdls_pe_add_peer(req);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err(QDF_MAC_ADDR_FMT " add peer failed with status %d",
			 QDF_MAC_ADDR_REF(mac), status);
		goto setlink;
	}

	return QDF_STATUS_SUCCESS;

setlink:
	tdls_set_link_status(vdev_obj, mac, TDLS_LINK_IDLE,
			     TDLS_LINK_UNSPECIFIED);
addrsp:
	if (soc_obj->tdls_event_cb) {
		ind.status = status;
		ind.vdev = req->vdev;
		soc_obj->tdls_event_cb(soc_obj->tdls_evt_cb_data,
				       TDLS_EVENT_ADD_PEER, &ind);
	}

	return QDF_STATUS_E_PERM;
}

static QDF_STATUS
tdls_add_peer_serialize_callback(struct wlan_serialization_command *cmd,
				 enum wlan_serialization_cb_reason reason)
{
	struct tdls_add_peer_request *req;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!cmd || !cmd->umac_cmd) {
		tdls_err("cmd: %pK, reason: %d", cmd, reason);
		return QDF_STATUS_E_NULL_VALUE;
	}

	req = cmd->umac_cmd;

	switch (reason) {
	case WLAN_SER_CB_ACTIVATE_CMD:
		/* command moved to active list
		 */
		status = tdls_activate_add_peer(req);
		break;

	case WLAN_SER_CB_CANCEL_CMD:
		/* command removed from pending list.
		 * notify os interface the status
		 */
		status = tdls_internal_add_peer_rsp(req, QDF_STATUS_E_FAILURE);
		break;

	case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
		/* active command time out. */
		status = tdls_internal_add_peer_rsp(req, QDF_STATUS_E_FAILURE);
		break;

	case WLAN_SER_CB_RELEASE_MEM_CMD:
		/* command successfully completed.
		 * release memory & vdev reference count
		 */
		wlan_objmgr_vdev_release_ref(req->vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(req);
		break;

	default:
		/* Do nothing but logging */
		QDF_ASSERT(0);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

void tdls_reset_nss(struct tdls_soc_priv_obj *tdls_soc,
				  uint8_t action_code)
{
	if (!tdls_soc)
		return;

	if (TDLS_TEARDOWN != action_code ||
	    !tdls_soc->tdls_nss_switch_in_progress)
		return;

	if (tdls_soc->tdls_teardown_peers_cnt != 0)
		tdls_soc->tdls_teardown_peers_cnt--;
	if (tdls_soc->tdls_teardown_peers_cnt == 0) {
		if (tdls_soc->tdls_nss_transition_mode ==
		    TDLS_NSS_TRANSITION_S_1x1_to_2x2) {
			/* TDLS NSS switch is fully completed, so
			 * reset the flags.
			 */
			tdls_notice("TDLS NSS switch is fully completed");
			tdls_soc->tdls_nss_switch_in_progress = false;
			tdls_soc->tdls_nss_teardown_complete = false;
		} else {
			/* TDLS NSS switch is not yet completed, but
			 * tdls teardown is completed for all the
			 * peers.
			 */
			tdls_notice("teardown done & NSS switch in progress");
			tdls_soc->tdls_nss_teardown_complete = true;
		}
		tdls_soc->tdls_nss_transition_mode =
			TDLS_NSS_TRANSITION_S_UNKNOWN;
	}

}

/**
 * tdls_set_cap() - set TDLS capability type
 * @tdls_vdev: tdls vdev object
 * @mac: peer mac address
 * @cap: TDLS capability type
 *
 * Return: 0 if successful or negative errno otherwise
 */
int tdls_set_cap(struct tdls_vdev_priv_obj *tdls_vdev, const uint8_t *mac,
			  enum tdls_peer_capab cap)
{
	struct tdls_peer *curr_peer;

	curr_peer = tdls_get_peer(tdls_vdev, mac);
	if (!curr_peer) {
		tdls_err("curr_peer is NULL");
		return -EINVAL;
	}

	curr_peer->tdls_support = cap;
	return 0;
}

static int tdls_validate_setup_frames(struct tdls_soc_priv_obj *tdls_soc,
				struct tdls_validate_action_req *tdls_validate)
{
	/* supplicant still sends tdls_mgmt(SETUP_REQ)
	 * even after we return error code at
	 * 'add_station()'. Hence we have this check
	 * again in addition to add_station().	Anyway,
	 * there is no harm to double-check.
	 */
	if (TDLS_SETUP_REQUEST == tdls_validate->action_code) {
		tdls_err(QDF_MAC_ADDR_FMT " TDLS Max peer already connected. action (%d) declined. Num of peers (%d), Max allowed (%d).",
			 QDF_MAC_ADDR_REF(tdls_validate->peer_mac),
			 tdls_validate->action_code,
			 tdls_soc->connected_peer_count,
			 tdls_soc->max_num_tdls_sta);
		return -EINVAL;
	}
	/* maximum reached. tweak to send
	 * error code to peer and return error
	 * code to supplicant
	 */
	tdls_validate->status_code = QDF_STATUS_E_RESOURCES;
	tdls_err(QDF_MAC_ADDR_FMT " TDLS Max peer already connected, send response status (%d). Num of peers (%d), Max allowed (%d).",
		 QDF_MAC_ADDR_REF(tdls_validate->peer_mac),
		 tdls_validate->action_code,
		 tdls_soc->connected_peer_count,
		 tdls_soc->max_num_tdls_sta);

	return -EPERM;
}

int tdls_validate_mgmt_request(struct tdls_action_frame_request *tdls_mgmt_req)
{
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct tdls_soc_priv_obj *tdls_soc;
	struct tdls_peer *curr_peer;
	struct tdls_peer *temp_peer;
	QDF_STATUS status;
	uint8_t vdev_id;

	struct wlan_objmgr_vdev *vdev = tdls_mgmt_req->vdev;
	struct tdls_validate_action_req *tdls_validate =
		&tdls_mgmt_req->chk_frame;

	if (QDF_STATUS_SUCCESS != tdls_get_vdev_objects(vdev,
							&tdls_vdev,
							&tdls_soc))
		return -ENOTSUPP;

	/*
	 * STA or P2P client should be connected and authenticated before
	 *  sending any TDLS frames
	 */
	if ((wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) ||
	    !tdls_is_vdev_authenticated(vdev)) {
		tdls_err("STA is not connected or not authenticated.");
		return -EAGAIN;
	}

	/* other than teardown frame, mgmt frames are not sent if disabled */
	if (TDLS_TEARDOWN != tdls_validate->action_code) {
		if (ucfg_is_nan_disc_active(tdls_soc->soc)) {
			tdls_err("NAN active. NAN+TDLS not supported");
			return -EPERM;
		}

		if (!tdls_check_is_tdls_allowed(vdev)) {
			tdls_err("TDLS not allowed, reject MGMT, action = %d",
				tdls_validate->action_code);
			return -EPERM;
		}
		/* if tdls_mode is disabled, then decline the peer's request */
		if (TDLS_SUPPORT_DISABLED == tdls_soc->tdls_current_mode ||
		    TDLS_SUPPORT_SUSPENDED == tdls_soc->tdls_current_mode) {
			tdls_notice(QDF_MAC_ADDR_FMT
				" TDLS mode is disabled. action %d declined.",
				QDF_MAC_ADDR_REF(tdls_validate->peer_mac),
				tdls_validate->action_code);
			return -ENOTSUPP;
		}
		if (tdls_soc->tdls_nss_switch_in_progress) {
			tdls_err("nss switch in progress, action %d declined "
				QDF_MAC_ADDR_FMT,
				tdls_validate->action_code,
				QDF_MAC_ADDR_REF(tdls_validate->peer_mac));
			return -EAGAIN;
		}
	}
	/*
	 * In case another tdls request comes while tdls setup is already
	 * ongoing with one peer. Reject only when status code is 0. If status
	 * code is non-zero, it means supplicant already rejected it and
	 * the same should be notified to peer.
	 */
	if (TDLS_IS_SETUP_ACTION(tdls_validate->action_code)) {
		if (tdls_is_progress(tdls_vdev, tdls_validate->peer_mac,
				     true) &&
				     tdls_validate->status_code == 0) {
			tdls_err("setup is ongoing. action %d declined for "
				 QDF_MAC_ADDR_FMT,
				 tdls_validate->action_code,
				 QDF_MAC_ADDR_REF(tdls_validate->peer_mac));
			return -EPERM;
		}
	}

	/*  call hdd_wmm_is_acm_allowed() */
	vdev_id = wlan_vdev_get_id(vdev);
	if (!tdls_soc->tdls_wmm_cb(vdev_id)) {
		tdls_debug("admission ctrl set to VI, send the frame with least AC (BK) for action %d",
			   tdls_validate->action_code);
		tdls_mgmt_req->use_default_ac = false;
	} else {
		tdls_mgmt_req->use_default_ac = true;
	}

	if (TDLS_SETUP_REQUEST == tdls_validate->action_code ||
	    TDLS_SETUP_RESPONSE == tdls_validate->action_code) {
		if (tdls_soc->max_num_tdls_sta <=
			tdls_soc->connected_peer_count) {
			status = tdls_validate_setup_frames(tdls_soc,
							    tdls_validate);
			if (QDF_STATUS_SUCCESS != status)
				return status;
			/* fall through to send setup resp
			 * with failure status code
			 */
		} else {
			curr_peer =
				tdls_find_peer(tdls_vdev,
					       tdls_validate->peer_mac);
			if (curr_peer) {
				if (TDLS_IS_LINK_CONNECTED(curr_peer)) {
					tdls_err(QDF_MAC_ADDR_FMT " already connected action %d declined.",
						QDF_MAC_ADDR_REF(
						tdls_validate->peer_mac),
						tdls_validate->action_code);

					return -EPERM;
				}
			}
		}
	}

	tdls_debug("tdls_mgmt " QDF_MAC_ADDR_FMT " action %d, dialog_token %d status %d, len = %zu",
		   QDF_MAC_ADDR_REF(tdls_validate->peer_mac),
		   tdls_validate->action_code, tdls_validate->dialog_token,
		   tdls_validate->status_code, tdls_validate->len);

	/*Except teardown responder will not be used so just make 0 */
	tdls_validate->responder = 0;
	if (TDLS_TEARDOWN == tdls_validate->action_code) {
		temp_peer = tdls_find_peer(tdls_vdev, tdls_validate->peer_mac);
		if (!temp_peer) {
			tdls_err(QDF_MAC_ADDR_FMT " peer doesn't exist",
				     QDF_MAC_ADDR_REF(
				     tdls_validate->peer_mac));
			return -EPERM;
		}

		if (TDLS_IS_LINK_CONNECTED(temp_peer))
			tdls_validate->responder = temp_peer->is_responder;
		else {
			tdls_err(QDF_MAC_ADDR_FMT " peer doesn't exist or not connected %d dialog_token %d status %d, tdls_validate->len = %zu",
				 QDF_MAC_ADDR_REF(tdls_validate->peer_mac),
				 temp_peer->link_status,
				 tdls_validate->dialog_token,
				 tdls_validate->status_code,
				 tdls_validate->len);
			return -EPERM;
		}
	}

	/* For explicit trigger of DIS_REQ come out of BMPS for
	 * successfully receiving DIS_RSP from peer.
	 */
	if ((TDLS_SETUP_RESPONSE == tdls_validate->action_code) ||
	    (TDLS_SETUP_CONFIRM == tdls_validate->action_code) ||
	    (TDLS_DISCOVERY_RESPONSE == tdls_validate->action_code) ||
	    (TDLS_DISCOVERY_REQUEST == tdls_validate->action_code)) {
		/* Fw will take care if PS offload is enabled. */
		if (TDLS_DISCOVERY_REQUEST != tdls_validate->action_code)
			tdls_set_cap(tdls_vdev, tdls_validate->peer_mac,
					      TDLS_CAP_SUPPORTED);
	}
	return 0;
}

QDF_STATUS tdls_process_add_peer(struct tdls_add_peer_request *req)
{
	struct wlan_serialization_command cmd = {0,};
	enum wlan_serialization_status ser_cmd_status;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct wlan_objmgr_psoc *psoc;

	if (!req || !req->vdev) {
		tdls_err("req: %pK", req);
		goto free_req;
	}
	vdev = req->vdev;
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		tdls_err("can't get psoc");
		goto error;
	}
	if (ucfg_is_nan_disc_active(psoc)) {
		tdls_err("NAN active. NAN+TDLS not supported");
		goto error;
	}
	status = QDF_STATUS_SUCCESS;

	cmd.cmd_type = WLAN_SER_CMD_TDLS_ADD_PEER;
	cmd.cmd_id = 0;
	cmd.cmd_cb = tdls_add_peer_serialize_callback;
	cmd.umac_cmd = req;
	cmd.source = WLAN_UMAC_COMP_TDLS;
	cmd.is_high_priority = false;
	cmd.cmd_timeout_duration = TDLS_DEFAULT_SERIALIZE_CMD_TIMEOUT;
	cmd.vdev = vdev;
	cmd.is_blocking = true;

	ser_cmd_status = wlan_serialization_request(&cmd);
	tdls_debug("req: 0x%pK wlan_serialization_request status:%d", req,
		   ser_cmd_status);

	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
		/* command moved to pending list. Do nothing */
		break;
	case WLAN_SER_CMD_ACTIVE:
		/* command moved to active list. Do nothing */
		break;
	default:
		goto error;
	}

	return status;
error:
	/* notify os interface about internal error*/
	status = tdls_internal_add_peer_rsp(req, QDF_STATUS_E_FAILURE);
	wlan_objmgr_vdev_release_ref(req->vdev, WLAN_TDLS_NB_ID);
free_req:
	qdf_mem_free(req);
	return status;
}

static QDF_STATUS
tdls_activate_update_peer(struct tdls_update_peer_request *req)
{
	QDF_STATUS status;
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct wlan_objmgr_vdev *vdev;
	struct tdls_peer *curr_peer;
	uint16_t curr_tdls_peers;
	const uint8_t *mac;
	struct tdls_update_peer_params *update_peer;
	struct tdls_osif_indication ind;

	if (!req->vdev) {
		tdls_err("vdev object NULL when add TDLS peer");
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}

	mac = req->update_peer_req.peer_addr;
	vdev = req->vdev;
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(vdev);
	if (!soc_obj || !vdev_obj) {
		tdls_err("soc_obj: %pK, vdev_obj: %pK", soc_obj, vdev_obj);
		return QDF_STATUS_E_INVAL;
	}

	status = tdls_validate_current_mode(soc_obj);
	if (QDF_IS_STATUS_ERROR(status))
		goto updatersp;

	curr_peer = tdls_find_peer(vdev_obj, mac);
	if (!curr_peer) {
		tdls_err(QDF_MAC_ADDR_FMT " not exist. return invalid",
			 QDF_MAC_ADDR_REF(mac));
		status = QDF_STATUS_E_INVAL;
		goto updatersp;
	}

	/* in change station, we accept only when sta_id is valid */
	if (curr_peer->link_status ==  TDLS_LINK_TEARING ||
	    !curr_peer->valid_entry) {
		tdls_err(QDF_MAC_ADDR_FMT " link %d. update peer rejected",
			 QDF_MAC_ADDR_REF(mac), curr_peer->link_status);
		status = QDF_STATUS_E_PERM;
		goto updatersp;
	}

	if (curr_peer->link_status ==  TDLS_LINK_CONNECTED &&
	    curr_peer->valid_entry) {
		tdls_err(QDF_MAC_ADDR_FMT " link %d. update peer is igonored as tdls state is already connected ",
			 QDF_MAC_ADDR_REF(mac), curr_peer->link_status);
		status = QDF_STATUS_SUCCESS;
		goto updatersp;
	}

	/* when others are on-going, we want to change link_status to idle */
	if (tdls_is_progress(vdev_obj, mac, true)) {
		tdls_notice(QDF_MAC_ADDR_FMT " TDLS setuping. Req declined.",
			    QDF_MAC_ADDR_REF(mac));
		status = QDF_STATUS_E_PERM;
		goto setlink;
	}

	curr_tdls_peers = tdls_get_connected_peer(soc_obj);
	if (soc_obj->max_num_tdls_sta <= curr_tdls_peers) {
		tdls_err(QDF_MAC_ADDR_FMT
			 " Request declined. Current: %d, Max allowed: %d.",
			 QDF_MAC_ADDR_REF(mac), curr_tdls_peers,
			 soc_obj->max_num_tdls_sta);
		status = QDF_STATUS_E_PERM;
		goto setlink;
	}
	update_peer = &req->update_peer_req;

	if (update_peer->htcap_present)
		curr_peer->spatial_streams = update_peer->ht_cap.mcsset[1];

	tdls_set_peer_caps(vdev_obj, mac, &req->update_peer_req);
	status = tdls_pe_update_peer(req);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err(QDF_MAC_ADDR_FMT " update peer failed with status %d",
			 QDF_MAC_ADDR_REF(mac), status);
		goto setlink;
	}

	return QDF_STATUS_SUCCESS;

setlink:
	tdls_set_link_status(vdev_obj, mac, TDLS_LINK_IDLE,
			     TDLS_LINK_UNSPECIFIED);
updatersp:
	if (soc_obj->tdls_event_cb) {
		ind.status = status;
		ind.vdev = vdev;
		soc_obj->tdls_event_cb(soc_obj->tdls_evt_cb_data,
				       TDLS_EVENT_ADD_PEER, &ind);
	}

	return QDF_STATUS_E_PERM;
}

static QDF_STATUS
tdls_update_peer_serialize_callback(struct wlan_serialization_command *cmd,
				    enum wlan_serialization_cb_reason reason)
{
	struct tdls_update_peer_request *req;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!cmd || !cmd->umac_cmd) {
		tdls_err("cmd: %pK, reason: %d", cmd, reason);
		return QDF_STATUS_E_NULL_VALUE;
	}

	req = cmd->umac_cmd;

	switch (reason) {
	case WLAN_SER_CB_ACTIVATE_CMD:
		/* command moved to active list
		 */
		status = tdls_activate_update_peer(req);
		break;

	case WLAN_SER_CB_CANCEL_CMD:
		/* command removed from pending list.
		 * notify os interface the status
		 */
		status = tdls_internal_update_peer_rsp(req,
						       QDF_STATUS_E_FAILURE);
		break;

	case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
		/* active command time out. */
		status = tdls_internal_update_peer_rsp(req,
						       QDF_STATUS_E_FAILURE);
		break;

	case WLAN_SER_CB_RELEASE_MEM_CMD:
		/* command successfully completed.
		 * release memory & release reference count
		 */
		wlan_objmgr_vdev_release_ref(req->vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(req);
		break;

	default:
		/* Do nothing but logging */
		QDF_ASSERT(0);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

QDF_STATUS tdls_process_update_peer(struct tdls_update_peer_request *req)
{
	struct wlan_serialization_command cmd = {0,};
	enum wlan_serialization_status ser_cmd_status;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!req || !req->vdev) {
		tdls_err("req: %pK", req);
		status = QDF_STATUS_E_FAILURE;
		goto free_req;
	}

	vdev = req->vdev;
	cmd.cmd_type = WLAN_SER_CMD_TDLS_ADD_PEER;
	cmd.cmd_id = 0;
	cmd.cmd_cb = tdls_update_peer_serialize_callback;
	cmd.umac_cmd = req;
	cmd.source = WLAN_UMAC_COMP_TDLS;
	cmd.is_high_priority = false;
	cmd.cmd_timeout_duration = TDLS_DEFAULT_SERIALIZE_CMD_TIMEOUT;
	cmd.vdev = req->vdev;
	cmd.is_blocking = true;

	ser_cmd_status = wlan_serialization_request(&cmd);
	tdls_debug("req: 0x%pK wlan_serialization_request status:%d", req,
		   ser_cmd_status);

	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
		/* command moved to pending list. Do nothing */
		break;
	case WLAN_SER_CMD_ACTIVE:
		/* command moved to active list. Do nothing */
		break;
	default:
		goto error;
	}

	return status;
error:
	/* notify os interface about internal error*/
	status = tdls_internal_update_peer_rsp(req, QDF_STATUS_E_FAILURE);
	wlan_objmgr_vdev_release_ref(req->vdev, WLAN_TDLS_NB_ID);
free_req:
	qdf_mem_free(req);
	return status;
}

static QDF_STATUS tdls_activate_del_peer(struct tdls_oper_request *req)
{
	struct tdls_del_peer_request request = {0,};

	request.vdev = req->vdev;
	request.del_peer_req.peer_addr = req->peer_addr;

	return tdls_pe_del_peer(&request);
}

static QDF_STATUS
tdls_del_peer_serialize_callback(struct wlan_serialization_command *cmd,
				 enum wlan_serialization_cb_reason reason)
{
	struct tdls_oper_request *req;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!cmd || !cmd->umac_cmd) {
		tdls_err("cmd: %pK, reason: %d", cmd, reason);
		return QDF_STATUS_E_NULL_VALUE;
	}

	req = cmd->umac_cmd;

	switch (reason) {
	case WLAN_SER_CB_ACTIVATE_CMD:
		/* command moved to active list
		 */
		status = tdls_activate_del_peer(req);
		break;

	case WLAN_SER_CB_CANCEL_CMD:
		/* command removed from pending list.
		 * notify os interface the status
		 */
		status = tdls_internal_del_peer_rsp(req);
		break;

	case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
		/* active command time out. */
		status = tdls_internal_del_peer_rsp(req);
		break;

	case WLAN_SER_CB_RELEASE_MEM_CMD:
		/* command successfully completed.
		 * release memory & vdev reference count
		 */
		wlan_objmgr_vdev_release_ref(req->vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(req);
		break;

	default:
		/* Do nothing but logging */
		QDF_ASSERT(0);
		status = QDF_STATUS_E_INVAL;
		break;
	}

	return status;
}

QDF_STATUS tdls_process_del_peer(struct tdls_oper_request *req)
{
	struct wlan_serialization_command cmd = {0,};
	enum wlan_serialization_status ser_cmd_status;
	struct wlan_objmgr_vdev *vdev;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct tdls_soc_priv_obj *soc_obj;
	uint8_t *mac;
	struct tdls_peer *peer;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!req || !req->vdev) {
		tdls_err("req: %pK", req);
		status = QDF_STATUS_E_INVAL;
		goto free_req;
	}

	vdev = req->vdev;

	/* vdev reference cnt is acquired in ucfg_tdls_oper */
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(vdev);
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);

	if (!vdev_obj || !soc_obj) {
		tdls_err("tdls vdev_obj: %pK soc_obj: %pK", vdev_obj, soc_obj);
		status = QDF_STATUS_E_NULL_VALUE;
		goto error;
	}

	mac = req->peer_addr;
	peer = tdls_find_peer(vdev_obj, mac);
	if (!peer) {
		tdls_err(QDF_MAC_ADDR_FMT
			 " not found, ignore NL80211_TDLS_ENABLE_LINK",
			 QDF_MAC_ADDR_REF(mac));
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	if (!peer->valid_entry) {
		tdls_err("invalid peer:" QDF_MAC_ADDR_FMT " link state %d",
			 QDF_MAC_ADDR_REF(mac), peer->link_status);
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	if (soc_obj->tdls_dp_vdev_update)
		soc_obj->tdls_dp_vdev_update(&soc_obj->soc,
					     wlan_vdev_get_id(vdev),
					     soc_obj->tdls_update_dp_vdev_flags,
					     false);

	cmd.cmd_type = WLAN_SER_CMD_TDLS_DEL_PEER;
	cmd.cmd_id = 0;
	cmd.cmd_cb = tdls_del_peer_serialize_callback;
	cmd.umac_cmd = req;
	cmd.source = WLAN_UMAC_COMP_TDLS;
	cmd.is_high_priority = false;
	cmd.cmd_timeout_duration = TDLS_DELETE_PEER_CMD_TIMEOUT;
	cmd.vdev = vdev;
	cmd.is_blocking = true;

	ser_cmd_status = wlan_serialization_request(&cmd);
	tdls_debug("req: 0x%pK wlan_serialization_request status:%d", req,
		   ser_cmd_status);

	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
		/* command moved to pending list. Do nothing */
		break;
	case WLAN_SER_CMD_ACTIVE:
		/* command moved to active list. Do nothing */
		break;
	default:
		goto error;
	}

	return status;
error:
	/* notify os interface about internal error*/
	status = tdls_internal_del_peer_rsp(req);
	wlan_objmgr_vdev_release_ref(req->vdev, WLAN_TDLS_NB_ID);
free_req:
	qdf_mem_free(req);
	return status;
}

/**
 * tdls_process_add_peer_rsp() - handle response for update TDLS peer
 * @rsp: TDLS add peer response
 *
 * Return: QDF_STATUS_SUCCESS for success; other values if failed
 */
static QDF_STATUS tdls_update_peer_rsp(struct tdls_add_sta_rsp *rsp)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	struct tdls_soc_priv_obj *soc_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct tdls_osif_indication ind;

	psoc = rsp->psoc;
	if (!psoc) {
		tdls_err("psoc is NULL");
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, rsp->session_id,
						    WLAN_TDLS_SB_ID);
	if (!vdev) {
		tdls_err("invalid vdev: %d", rsp->session_id);
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	tdls_release_serialization_command(vdev, WLAN_SER_CMD_TDLS_ADD_PEER);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);
error:
	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (soc_obj && soc_obj->tdls_event_cb) {
		ind.status = rsp->status_code;
		ind.vdev = vdev;
		soc_obj->tdls_event_cb(soc_obj->tdls_evt_cb_data,
				       TDLS_EVENT_ADD_PEER, &ind);
	}
	qdf_mem_free(rsp);

	return status;
}

/**
 * tdls_process_send_mgmt_rsp() - handle response for send mgmt
 * @rsp: TDLS send mgmt response
 *
 * Return: QDF_STATUS_SUCCESS for success; other values if failed
 */
QDF_STATUS tdls_process_send_mgmt_rsp(struct tdls_send_mgmt_rsp *rsp)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct tdls_soc_priv_obj *tdls_soc = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct tdls_osif_indication ind;

	psoc = rsp->psoc;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, rsp->vdev_id,
						    WLAN_TDLS_SB_ID);
	if (!vdev) {
		tdls_err("invalid vdev");
		status =  QDF_STATUS_E_INVAL;
		qdf_mem_free(rsp);
		return status;
	}
	tdls_soc = wlan_psoc_get_tdls_soc_obj(psoc);
	tdls_vdev = wlan_vdev_get_tdls_vdev_obj(vdev);
	if (!tdls_soc || !tdls_vdev) {
		tdls_err("soc object:%pK, vdev object:%pK", tdls_soc, tdls_vdev);
		status = QDF_STATUS_E_FAILURE;
	}

	tdls_release_serialization_command(vdev, WLAN_SER_CMD_TDLS_SEND_MGMT);

	if (legacy_result_success == rsp->status_code)
		goto free_rsp;
	tdls_err("send mgmt failed. status code(=%d)", rsp->status_code);
	status = QDF_STATUS_E_FAILURE;

	if (tdls_soc && tdls_soc->tdls_event_cb) {
		ind.vdev = vdev;
		ind.status = status;
		tdls_soc->tdls_event_cb(tdls_soc->tdls_evt_cb_data,
				       TDLS_EVENT_MGMT_TX_ACK_CNF, &ind);
	}

free_rsp:
	qdf_mem_free(rsp);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);
	return status;
}

/**
 * tdls_send_mgmt_tx_completion() - process tx completion
 * @tx_complete: TDLS mgmt completion info
 *
 * Return: QDF_STATUS_SUCCESS for success; other values if failed
 */
QDF_STATUS tdls_send_mgmt_tx_completion(
			struct tdls_mgmt_tx_completion_ind *tx_complete)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct tdls_soc_priv_obj *tdls_soc = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct tdls_osif_indication ind;

	psoc = tx_complete->psoc;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    tx_complete->vdev_id,
						    WLAN_TDLS_SB_ID);

	if (!vdev) {
		tdls_err("invalid vdev");
		status =  QDF_STATUS_E_INVAL;
		goto free_tx_complete;
	}

	tdls_soc = wlan_psoc_get_tdls_soc_obj(psoc);
	tdls_vdev = wlan_vdev_get_tdls_vdev_obj(vdev);

	if (!tdls_soc || !tdls_vdev) {
		tdls_err("soc object:%pK, vdev object:%pK", tdls_soc, tdls_vdev);
		status = QDF_STATUS_E_FAILURE;
	}

	if (tdls_soc && tdls_soc->tdls_event_cb) {
		ind.vdev = vdev;
		ind.status = tx_complete->tx_complete_status;
		tdls_soc->tdls_event_cb(tdls_soc->tdls_evt_cb_data,
			       TDLS_EVENT_MGMT_TX_ACK_CNF, &ind);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);
free_tx_complete:
	qdf_mem_free(tx_complete);
	return status;
}

/**
 * tdls_add_peer_rsp() - handle response for add TDLS peer
 * @rsp: TDLS add peer response
 *
 * Return: QDF_STATUS_SUCCESS for success; other values if failed
 */
static QDF_STATUS tdls_add_peer_rsp(struct tdls_add_sta_rsp *rsp)
{
	uint8_t sta_idx;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct tdls_soc_priv_obj *soc_obj = NULL;
	struct tdls_conn_info *conn_rec;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct tdls_osif_indication ind;

	psoc = rsp->psoc;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, rsp->session_id,
						    WLAN_TDLS_SB_ID);
	if (!vdev) {
		tdls_err("invalid vdev: %d", rsp->session_id);
		status =  QDF_STATUS_E_INVAL;
		goto error;
	}
	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(vdev);
	if (!soc_obj || !vdev_obj) {
		tdls_err("soc object:%pK, vdev object:%pK", soc_obj, vdev_obj);
		status = QDF_STATUS_E_FAILURE;
		goto cmddone;
	}
	if (rsp->status_code) {
		tdls_err("add sta failed. status code(=%d)", rsp->status_code);
		status = QDF_STATUS_E_FAILURE;
	} else {
		conn_rec = soc_obj->tdls_conn_info;
		for (sta_idx = 0; sta_idx < soc_obj->max_num_tdls_sta;
		     sta_idx++) {
			if (!conn_rec[sta_idx].valid_entry) {
				conn_rec[sta_idx].session_id = rsp->session_id;
				conn_rec[sta_idx].valid_entry = true;
				conn_rec[sta_idx].index = sta_idx;
				qdf_copy_macaddr(&conn_rec[sta_idx].peer_mac,
						 &rsp->peermac);
				tdls_debug("TDLS: Add sta mac at idx %d"
					   QDF_MAC_ADDR_FMT, sta_idx,
					   QDF_MAC_ADDR_REF
					   (rsp->peermac.bytes));
				break;
			}
		}

		if (sta_idx < soc_obj->max_num_tdls_sta) {
			status = tdls_set_valid(vdev_obj, rsp->peermac.bytes);
			if (QDF_IS_STATUS_ERROR(status)) {
				tdls_err("set staid failed");
				status = QDF_STATUS_E_FAILURE;
			}
		} else {
			status = QDF_STATUS_E_FAILURE;
		}
	}

cmddone:
	tdls_release_serialization_command(vdev, WLAN_SER_CMD_TDLS_ADD_PEER);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);
error:
	if (soc_obj && soc_obj->tdls_event_cb) {
		ind.vdev = vdev;
		ind.status = rsp->status_code;
		soc_obj->tdls_event_cb(soc_obj->tdls_evt_cb_data,
				       TDLS_EVENT_ADD_PEER, &ind);
	}
	qdf_mem_free(rsp);

	return status;
}

QDF_STATUS tdls_process_add_peer_rsp(struct tdls_add_sta_rsp *rsp)
{
	tdls_debug("peer oper %d", rsp->tdls_oper);

	if (rsp->tdls_oper == TDLS_OPER_ADD)
		return tdls_add_peer_rsp(rsp);
	else if (rsp->tdls_oper == TDLS_OPER_UPDATE)
		return tdls_update_peer_rsp(rsp);

	return QDF_STATUS_E_INVAL;
}

QDF_STATUS tdls_process_del_peer_rsp(struct tdls_del_sta_rsp *rsp)
{
	uint8_t sta_idx, id;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct tdls_soc_priv_obj *soc_obj = NULL;
	struct tdls_conn_info *conn_rec;
	struct tdls_peer *curr_peer = NULL;
	const uint8_t *macaddr;
	struct tdls_osif_indication ind;

	tdls_debug("del peer rsp: vdev %d  peer " QDF_MAC_ADDR_FMT,
		   rsp->session_id, QDF_MAC_ADDR_REF(rsp->peermac.bytes));
	psoc = rsp->psoc;
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, rsp->session_id,
						    WLAN_TDLS_SB_ID);
	if (!vdev) {
		tdls_err("invalid vdev: %d", rsp->session_id);
		status = QDF_STATUS_E_INVAL;
		goto error;
	}
	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(vdev);
	if (!soc_obj || !vdev_obj) {
		tdls_err("soc object:%pK, vdev object:%pK", soc_obj, vdev_obj);
		status = QDF_STATUS_E_FAILURE;
		goto cmddone;
	}

	conn_rec = soc_obj->tdls_conn_info;
	for (sta_idx = 0; sta_idx < soc_obj->max_num_tdls_sta; sta_idx++) {
		if (conn_rec[sta_idx].session_id != rsp->session_id ||
			qdf_mem_cmp(conn_rec[sta_idx].peer_mac.bytes,
				    rsp->peermac.bytes, QDF_MAC_ADDR_SIZE))
			continue;

		macaddr = rsp->peermac.bytes;
		tdls_debug("TDLS: del STA with sta_idx %d", sta_idx);
		curr_peer = tdls_find_peer(vdev_obj, macaddr);
		if (curr_peer) {
			tdls_debug(QDF_MAC_ADDR_FMT " status is %d",
				   QDF_MAC_ADDR_REF(macaddr),
				   curr_peer->link_status);

			id = wlan_vdev_get_id(vdev);

			if (TDLS_IS_LINK_CONNECTED(curr_peer))
				tdls_decrement_peer_count(soc_obj);
		}
		tdls_reset_peer(vdev_obj, macaddr);
		conn_rec[sta_idx].valid_entry = false;
		conn_rec[sta_idx].session_id = 0xff;
		conn_rec[sta_idx].index = INVALID_TDLS_PEER_INDEX;
		qdf_mem_zero(&conn_rec[sta_idx].peer_mac,
			     QDF_MAC_ADDR_SIZE);

		status = QDF_STATUS_SUCCESS;
		break;
	}
	macaddr = rsp->peermac.bytes;
	if (!curr_peer) {
		curr_peer = tdls_find_peer(vdev_obj, macaddr);

		if (curr_peer)
			tdls_set_peer_link_status(curr_peer, TDLS_LINK_IDLE,
						  (curr_peer->link_status ==
						   TDLS_LINK_TEARING) ?
						  TDLS_LINK_UNSPECIFIED :
						  TDLS_LINK_DROPPED_BY_REMOTE);
	}

cmddone:
	tdls_release_serialization_command(vdev, WLAN_SER_CMD_TDLS_DEL_PEER);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);
error:

	if (soc_obj && soc_obj->tdls_event_cb) {
		ind.vdev = vdev;
		soc_obj->tdls_event_cb(soc_obj->tdls_evt_cb_data,
				       TDLS_EVENT_DEL_PEER, &ind);
	}
	qdf_mem_free(rsp);

	return status;
}

static QDF_STATUS
tdls_wma_update_peer_state(struct tdls_soc_priv_obj *soc_obj,
			   struct tdls_peer_update_state *peer_state)
{
	struct scheduler_msg msg = {0,};
	QDF_STATUS status;

	tdls_debug("update TDLS peer " QDF_MAC_ADDR_FMT " vdev %d, state %d",
		   QDF_MAC_ADDR_REF(peer_state->peer_macaddr),
		   peer_state->vdev_id, peer_state->peer_state);
	msg.type = soc_obj->tdls_update_peer_state;
	msg.reserved = 0;
	msg.bodyptr = peer_state;

	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("scheduler_post_msg failed");
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

QDF_STATUS tdls_process_enable_link(struct tdls_oper_request *req)
{
	struct tdls_peer *peer;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct tdls_soc_priv_obj *soc_obj;
	struct wlan_objmgr_vdev *vdev;
	uint8_t *mac;
	struct tdls_peer_update_state *peer_update_param;
	QDF_STATUS status;
	uint32_t feature;
	uint8_t id;

	vdev = req->vdev;
	if (!vdev) {
		tdls_err("NULL vdev object");
		qdf_mem_free(req);
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* vdev reference cnt is acquired in ucfg_tdls_oper */
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(vdev);
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);

	if (!vdev_obj || !soc_obj) {
		tdls_err("tdls vdev_obj: %pK soc_obj: %pK", vdev_obj, soc_obj);
		status = QDF_STATUS_E_NULL_VALUE;
		goto error;
	}

	mac = req->peer_addr;
	peer = tdls_find_peer(vdev_obj, mac);
	if (!peer) {
		tdls_err(QDF_MAC_ADDR_FMT
			 " not found, ignore NL80211_TDLS_ENABLE_LINK",
			 QDF_MAC_ADDR_REF(mac));
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	tdls_debug("enable link for peer " QDF_MAC_ADDR_FMT " link state %d",
		   QDF_MAC_ADDR_REF(mac), peer->link_status);
	if (!peer->valid_entry) {
		tdls_err("invalid entry " QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(mac));
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	peer->tdls_support = TDLS_CAP_SUPPORTED;
	if (TDLS_LINK_CONNECTED != peer->link_status)
		tdls_set_peer_link_status(peer, TDLS_LINK_CONNECTED,
					  TDLS_LINK_SUCCESS);

	id = wlan_vdev_get_id(vdev);
	status = soc_obj->tdls_reg_peer(soc_obj->tdls_peer_context,
					id, mac, peer->qos);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("TDLS register peer fail, status %d", status);
		goto error;
	}

	peer_update_param = qdf_mem_malloc(sizeof(*peer_update_param));
	if (!peer_update_param) {
		status = QDF_STATUS_E_NOMEM;
		goto error;
	}

	tdls_extract_peer_state_param(peer_update_param, peer);

	status = tdls_wma_update_peer_state(soc_obj, peer_update_param);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(peer_update_param);
		status = QDF_STATUS_E_PERM;
		goto error;
	}

	tdls_increment_peer_count(soc_obj);
	feature = soc_obj->tdls_configs.tdls_feature_flags;

	if (soc_obj->tdls_dp_vdev_update)
		soc_obj->tdls_dp_vdev_update(
				&soc_obj->soc,
				wlan_vdev_get_id(vdev),
				soc_obj->tdls_update_dp_vdev_flags,
				((peer->link_status == TDLS_LINK_CONNECTED) ?
				 true : false));

	tdls_debug("TDLS buffer sta: %d, uapsd_mask %d",
		   TDLS_IS_BUFFER_STA_ENABLED(feature),
		   soc_obj->tdls_configs.tdls_uapsd_mask);

error:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	qdf_mem_free(req);

	return status;
}

/**
 * tdls_config_force_peer() - configure an externally controllable TDLS peer
 * @req: TDLS operation request
 *
 * This is not the tdls_process_cmd function. No need to acquire the reference
 * count, release reference count  and free the request, the caller handle it
 * correctly.
 *
 * Return: QDF_STATUS_SUCCESS if success; other values if failed
 */
static QDF_STATUS tdls_config_force_peer(
	struct tdls_oper_config_force_peer_request *req)
{
	struct tdls_peer *peer;
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_vdev *vdev;
	const uint8_t *macaddr;
	uint32_t feature;
	QDF_STATUS status;
	uint32_t chan_freq;
	struct tdls_peer_update_state *peer_update_param;

	macaddr = req->peer_addr;

	vdev = req->vdev;
	pdev = wlan_vdev_get_pdev(vdev);
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(vdev);
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (!pdev || !vdev_obj || !soc_obj) {
		tdls_err("pdev: %pK, vdev_obj: %pK, soc_obj: %pK",
			 pdev, vdev_obj, soc_obj);
		return QDF_STATUS_E_INVAL;
	}

	feature = soc_obj->tdls_configs.tdls_feature_flags;
	if (!(TDLS_IS_EXTERNAL_CONTROL_ENABLED(feature) ||
	    TDLS_IS_LIBERAL_EXTERNAL_CONTROL_ENABLED(feature)) ||
	    !TDLS_IS_IMPLICIT_TRIG_ENABLED(feature)) {
		tdls_err("TDLS ext ctrl or Imp Trig not enabled, %x", feature);
		return QDF_STATUS_E_NOSUPPORT;
	}

	/*
	 * In case of liberal external mode, supplicant will provide peer mac
	 * address but driver has to behave similar to implict mode ie
	 * establish tdls link with any peer that supports tdls and meets stats
	 */
	if (TDLS_IS_LIBERAL_EXTERNAL_CONTROL_ENABLED(feature)) {
		tdls_debug("liberal mode set");
		return QDF_STATUS_SUCCESS;
	}

	peer_update_param = qdf_mem_malloc(sizeof(*peer_update_param));
	if (!peer_update_param)
		return QDF_STATUS_E_NOMEM;

	peer = tdls_get_peer(vdev_obj, macaddr);
	if (!peer) {
		tdls_err("peer " QDF_MAC_ADDR_FMT " does not exist",
			 QDF_MAC_ADDR_REF(macaddr));
		status = QDF_STATUS_E_NULL_VALUE;
		goto error;
	}
	status = tdls_set_force_peer(vdev_obj, macaddr, true);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("set force peer failed");
		goto error;
	}

	/* Update the peer mac to firmware, so firmware could update the
	 * connection table
	 */
	peer_update_param->vdev_id = wlan_vdev_get_id(vdev);
	qdf_mem_copy(peer_update_param->peer_macaddr,
		     macaddr, QDF_MAC_ADDR_SIZE);
	peer_update_param->peer_state = TDLS_PEER_ADD_MAC_ADDR;

	status = tdls_wma_update_peer_state(soc_obj, peer_update_param);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("update peer state failed");
		goto error;
	}

	soc_obj->tdls_external_peer_count++;
	chan_freq = wlan_reg_legacy_chan_to_freq(pdev, req->chan);

	/* Validate if off channel is DFS channel */
	if (wlan_reg_is_dfs_for_freq(pdev, chan_freq)) {
		tdls_err("Resetting TDLS off-channel from %d to %d",
			 req->chan, WLAN_TDLS_PREFERRED_OFF_CHANNEL_NUM_DEF);
		req->chan = WLAN_TDLS_PREFERRED_OFF_CHANNEL_NUM_DEF;
	}
	tdls_set_extctrl_param(peer, req->chan, req->max_latency, req->op_class,
			       req->min_bandwidth);

	tdls_set_callback(peer, req->callback);

	tdls_set_ct_mode(soc_obj->soc);
	if (soc_obj->enable_tdls_connection_tracker)
		tdls_implicit_enable(vdev_obj);

	return status;
error:
	qdf_mem_free(peer_update_param);
	return status;
}

/**
 * tdls_process_setup_peer() - process configure an externally
 *                                    controllable TDLS peer
 * @req: TDLS operation request
 *
 * Return: QDF_STATUS_SUCCESS if success; other values if failed
 */
QDF_STATUS tdls_process_setup_peer(struct tdls_oper_request *req)
{
	struct tdls_oper_config_force_peer_request peer_req;
	struct tdls_soc_priv_obj *soc_obj;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	tdls_debug("Configure external TDLS peer " QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(req->peer_addr));

	/* reference cnt is acquired in ucfg_tdls_oper */
	vdev = req->vdev;
	if (!vdev) {
		tdls_err("NULL vdev object");
		status = QDF_STATUS_E_NULL_VALUE;
		goto freereq;
	}

	qdf_mem_zero(&peer_req, sizeof(peer_req));
	peer_req.vdev = vdev;
	qdf_mem_copy(peer_req.peer_addr, req->peer_addr, QDF_MAC_ADDR_SIZE);

	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (!soc_obj) {
		tdls_err("NULL soc object");
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	peer_req.chan = soc_obj->tdls_configs.tdls_pre_off_chan_num;

	status = tdls_config_force_peer(&peer_req);
error:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
freereq:
	qdf_mem_free(req);

	return status;
}

QDF_STATUS tdls_process_remove_force_peer(struct tdls_oper_request *req)
{
	struct tdls_peer *peer;
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct wlan_objmgr_vdev *vdev;
	const uint8_t *macaddr;
	uint32_t feature;
	QDF_STATUS status;
	struct tdls_peer_update_state *peer_update_param;
	struct tdls_osif_indication ind;

	macaddr = req->peer_addr;
	tdls_debug("NL80211_TDLS_TEARDOWN for " QDF_MAC_ADDR_FMT,
		   QDF_MAC_ADDR_REF(macaddr));

	vdev = req->vdev;
	if (!vdev) {
		tdls_err("NULL vdev object");
		qdf_mem_free(req);
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* reference cnt is acquired in ucfg_tdls_oper */
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(req->vdev);
	soc_obj = wlan_vdev_get_tdls_soc_obj(req->vdev);
	if (!soc_obj || !vdev_obj) {
		tdls_err("soc_obj: %pK, vdev_obj: %pK", soc_obj, vdev_obj);
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	feature = soc_obj->tdls_configs.tdls_feature_flags;
	if (!(TDLS_IS_EXTERNAL_CONTROL_ENABLED(feature) ||
	    TDLS_IS_LIBERAL_EXTERNAL_CONTROL_ENABLED(feature)) ||
	    !TDLS_IS_IMPLICIT_TRIG_ENABLED(feature)) {
		tdls_err("TDLS ext ctrl or Imp Trig not enabled, %x", feature);
		status = QDF_STATUS_E_NOSUPPORT;
		goto error;
	}

	peer = tdls_find_peer(vdev_obj, macaddr);
	if (!peer) {
		tdls_err("peer matching " QDF_MAC_ADDR_FMT " not found",
			 QDF_MAC_ADDR_REF(macaddr));
		status = QDF_STATUS_E_NULL_VALUE;
		goto error;
	}
	if (peer->link_status == TDLS_LINK_CONNECTED)
		tdls_set_peer_link_status(peer, TDLS_LINK_TEARING,
					  TDLS_LINK_UNSPECIFIED);

	if (soc_obj->tdls_dp_vdev_update)
		soc_obj->tdls_dp_vdev_update(
				&soc_obj->soc,
				wlan_vdev_get_id(vdev),
				soc_obj->tdls_update_dp_vdev_flags,
				false);

	if (soc_obj->tdls_event_cb) {
		qdf_mem_copy(ind.peer_mac, macaddr, QDF_MAC_ADDR_SIZE);
		ind.vdev = vdev;
		ind.reason = TDLS_TEARDOWN_PEER_UNSPEC_REASON;
		soc_obj->tdls_event_cb(soc_obj->tdls_evt_cb_data,
				       TDLS_EVENT_TEARDOWN_REQ, &ind);
	}

	status = tdls_set_force_peer(vdev_obj, macaddr, false);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("set force peer failed");
		status = QDF_STATUS_E_INVAL;
		goto error;
	}

	if (soc_obj->tdls_external_peer_count)
		soc_obj->tdls_external_peer_count--;

	tdls_set_callback(peer, NULL);
	peer_update_param = qdf_mem_malloc(sizeof(*peer_update_param));
	if (!peer_update_param) {
		status = QDF_STATUS_E_NOMEM;
		goto error;
	}

	peer_update_param->vdev_id = wlan_vdev_get_id(vdev);
	qdf_mem_copy(peer_update_param->peer_macaddr,
		     macaddr, QDF_MAC_ADDR_SIZE);
	peer_update_param->peer_state = TDLS_PEER_REMOVE_MAC_ADDR;
	status = tdls_wma_update_peer_state(soc_obj, peer_update_param);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(peer_update_param);
		goto error;
	}
	tdls_set_ct_mode(soc_obj->soc);
	if (!soc_obj->enable_tdls_connection_tracker)
		tdls_implicit_disable(vdev_obj);

error:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	qdf_mem_free(req);

	return status;
}

static const char *tdls_evt_to_str(enum tdls_event_msg_type type)
{
	switch (type) {
	case TDLS_SHOULD_DISCOVER:
		return "SHOULD_DISCOVER";
	case TDLS_SHOULD_TEARDOWN:
		return "SHOULD_TEARDOWN";
	case TDLS_PEER_DISCONNECTED:
		return "SHOULD_PEER_DISCONNECTED";
	case TDLS_CONNECTION_TRACKER_NOTIFY:
		return "CONNECTION_TRACKER_NOTIFICATION";
	default:
		return "INVALID_TYPE";
	}
}

QDF_STATUS tdls_process_should_discover(struct wlan_objmgr_vdev *vdev,
					struct tdls_event_info *evt)
{
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct tdls_peer *curr_peer;
	uint32_t feature;
	uint16_t type;

	/*TODO ignore this if any concurrency detected*/
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(vdev);
	type = evt->message_type;

	tdls_debug("TDLS %s: " QDF_MAC_ADDR_FMT "reason %d",
		   tdls_evt_to_str(type),
		   QDF_MAC_ADDR_REF(evt->peermac.bytes),
		   evt->peer_reason);
	if (!soc_obj || !vdev_obj) {
		tdls_err("soc_obj: %pK, vdev_obj: %pK, ignore %s",
			 soc_obj, vdev_obj, tdls_evt_to_str(type));
		return QDF_STATUS_E_NULL_VALUE;
	}
	if (soc_obj->tdls_nss_switch_in_progress) {
		tdls_err("TDLS antenna switching, ignore %s",
			 tdls_evt_to_str(type));
		return QDF_STATUS_SUCCESS;
	}

	curr_peer = tdls_get_peer(vdev_obj, evt->peermac.bytes);
	if (!curr_peer) {
		tdls_notice("curr_peer is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (TDLS_LINK_CONNECTED == curr_peer->link_status) {
		tdls_err("TDLS link status is connected, ignore");
		return QDF_STATUS_SUCCESS;
	}

	feature = soc_obj->tdls_configs.tdls_feature_flags;
	if (TDLS_IS_EXTERNAL_CONTROL_ENABLED(feature) &&
	    !curr_peer->is_forced_peer) {
		tdls_debug("curr_peer is not forced, ignore %s",
			   tdls_evt_to_str(type));
		return QDF_STATUS_SUCCESS;
	}

	tdls_debug("initiate TDLS setup on %s, ext: %d, force: %d, reason: %d",
		   tdls_evt_to_str(type),
		   TDLS_IS_EXTERNAL_CONTROL_ENABLED(feature),
		   curr_peer->is_forced_peer, evt->peer_reason);
	vdev_obj->curr_candidate = curr_peer;
	tdls_implicit_send_discovery_request(vdev_obj);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_process_should_teardown(struct wlan_objmgr_vdev *vdev,
					struct tdls_event_info *evt)
{
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct tdls_peer *curr_peer;
	uint32_t reason;
	uint16_t type;

	type = evt->message_type;
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(vdev);

	tdls_debug("TDLS %s: " QDF_MAC_ADDR_FMT "reason %d",
		   tdls_evt_to_str(type),
		   QDF_MAC_ADDR_REF(evt->peermac.bytes), evt->peer_reason);

	if (!soc_obj || !vdev_obj) {
		tdls_err("soc_obj: %pK, vdev_obj: %pK, ignore %s",
			 soc_obj, vdev_obj, tdls_evt_to_str(type));
		return QDF_STATUS_E_NULL_VALUE;
	}

	curr_peer = tdls_find_peer(vdev_obj, evt->peermac.bytes);
	if (!curr_peer) {
		tdls_notice("curr_peer is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	reason = evt->peer_reason;
	if (TDLS_LINK_CONNECTED == curr_peer->link_status) {
		tdls_err("%s reason: %d for" QDF_MAC_ADDR_FMT,
			 tdls_evt_to_str(type), evt->peer_reason,
			 QDF_MAC_ADDR_REF(evt->peermac.bytes));
		if (reason == TDLS_TEARDOWN_RSSI ||
		    reason == TDLS_DISCONNECTED_PEER_DELETE ||
		    reason == TDLS_TEARDOWN_PTR_TIMEOUT ||
		    reason == TDLS_TEARDOWN_NO_RSP)
			reason = TDLS_TEARDOWN_PEER_UNREACHABLE;
		else
			reason = TDLS_TEARDOWN_PEER_UNSPEC_REASON;

		tdls_indicate_teardown(vdev_obj, curr_peer, reason);
	} else {
		tdls_err("TDLS link is not connected, ignore %s",
			 tdls_evt_to_str(type));
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_process_connection_tracker_notify(struct wlan_objmgr_vdev *vdev,
						  struct tdls_event_info *evt)
{
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_vdev_priv_obj *vdev_obj;
	uint16_t type;

	type = evt->message_type;
	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	vdev_obj = wlan_vdev_get_tdls_vdev_obj(vdev);

	if (!soc_obj || !vdev_obj) {
		tdls_err("soc_obj: %pK, vdev_obj: %pK, ignore %s",
			 soc_obj, vdev_obj, tdls_evt_to_str(type));
		return QDF_STATUS_E_NULL_VALUE;
	}

	/*TODO connection tracker update*/
	return QDF_STATUS_SUCCESS;
}

/**
 * tdls_process_set_responder() - Set/clear TDLS peer's responder role
 * @set_req: set responder request
 *
 * Return: 0 for success or -EINVAL otherwise
 */
static
int tdls_process_set_responder(struct tdls_set_responder_req *set_req)
{
	struct tdls_peer *curr_peer;
	struct tdls_vdev_priv_obj *tdls_vdev;

	tdls_vdev = wlan_vdev_get_tdls_vdev_obj(set_req->vdev);
	if (!tdls_vdev) {
		tdls_err("tdls vdev obj is NULL");
		return -EINVAL;
	}
	curr_peer = tdls_get_peer(tdls_vdev, set_req->peer_mac);
	if (!curr_peer) {
		tdls_err("curr_peer is NULL");
		return -EINVAL;
	}

	curr_peer->is_responder = set_req->responder;
	return 0;
}


/**
 * tdls_set_responder() - Set/clear TDLS peer's responder role
 * @set_req: set responder request
 *
 * Return: 0 for success or -EINVAL otherwise
 */
int tdls_set_responder(struct tdls_set_responder_req *set_req)
{
	int status;

	if (!set_req) {
		tdls_err("Invalid input params");
		return  -EINVAL;
	}

	if (!set_req->vdev) {
		tdls_err("Invalid input params %pK", set_req);
		status = -EINVAL;
		goto free_req;
	}

	status = wlan_objmgr_vdev_try_get_ref(set_req->vdev, WLAN_TDLS_NB_ID);
	if (QDF_STATUS_SUCCESS != status) {
		tdls_err("vdev object is deleted");
		status = -EINVAL;
		goto error;
	}

	status = tdls_process_set_responder(set_req);

error:
	wlan_objmgr_vdev_release_ref(set_req->vdev, WLAN_TDLS_NB_ID);
free_req:
	qdf_mem_free(set_req);
	return status;
}

static int tdls_teardown_links(struct tdls_soc_priv_obj *soc_obj, uint32_t mode)
{
	uint8_t staidx;
	struct tdls_peer *curr_peer;
	struct tdls_conn_info *conn_rec;
	int ret = 0;

	conn_rec = soc_obj->tdls_conn_info;
	for (staidx = 0; staidx < soc_obj->max_num_tdls_sta; staidx++) {
		if (!conn_rec[staidx].valid_entry)
			continue;

		curr_peer = tdls_find_all_peer(soc_obj,
					       conn_rec[staidx].peer_mac.bytes);
		if (!curr_peer)
			continue;

		/* if supported only 1x1, skip it */
		if (curr_peer->spatial_streams == HW_MODE_SS_1x1)
			continue;

		tdls_debug("Indicate TDLS teardown peer bssid "
			   QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(
			   curr_peer->peer_mac.bytes));
		tdls_indicate_teardown(curr_peer->vdev_priv, curr_peer,
				       TDLS_TEARDOWN_PEER_UNSPEC_REASON);

		soc_obj->tdls_teardown_peers_cnt++;
	}

	if (soc_obj->tdls_teardown_peers_cnt >= 1) {
		soc_obj->tdls_nss_switch_in_progress = true;
		tdls_debug("TDLS peers to be torn down = %d",
			   soc_obj->tdls_teardown_peers_cnt);

		/* set the antenna switch transition mode */
		if (mode == HW_MODE_SS_1x1) {
			soc_obj->tdls_nss_transition_mode =
				TDLS_NSS_TRANSITION_S_2x2_to_1x1;
			ret = -EAGAIN;
		} else {
			soc_obj->tdls_nss_transition_mode =
				TDLS_NSS_TRANSITION_S_1x1_to_2x2;
			ret = 0;
		}
		tdls_debug("TDLS teardown for antenna switch operation starts");
	}

	return ret;
}

QDF_STATUS tdls_process_antenna_switch(struct tdls_antenna_switch_request *req)
{
	QDF_STATUS status;
	struct tdls_soc_priv_obj *soc_obj;
	struct tdls_vdev_priv_obj *vdev_obj;
	struct wlan_objmgr_vdev *vdev = NULL;
	uint32_t vdev_nss;
	int ant_switch_state = 0;
	uint32_t vdev_id;
	enum QDF_OPMODE opmode;
	qdf_freq_t freq;
	struct tdls_osif_indication ind;
	enum policy_mgr_con_mode mode;

	if (!req) {
		tdls_err("null req");
		return QDF_STATUS_E_INVAL;
	}

	vdev = req->vdev;
	if (!vdev) {
		tdls_err("null vdev");
		qdf_mem_free(req);
		return QDF_STATUS_E_INVAL;
	}

	status = tdls_get_vdev_objects(vdev, &vdev_obj, &soc_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("can't get vdev_obj & soc_obj");
		goto get_obj_err;
	}

	if (soc_obj->connected_peer_count == 0)
		goto ant_sw_done;

	if (soc_obj->tdls_nss_switch_in_progress) {
		if (!soc_obj->tdls_nss_teardown_complete) {
			tdls_err("TDLS antenna switch is in progress");
			goto ant_sw_in_progress;
		} else {
			goto ant_sw_done;
		}
	}

	vdev_id = wlan_vdev_get_id(vdev);
	opmode = wlan_vdev_mlme_get_opmode(vdev);
	mode = policy_mgr_convert_device_mode_to_qdf_type(opmode);
	freq = policy_mgr_get_channel(soc_obj->soc,
				      mode,
				      &vdev_id);

	/* Check supported nss for TDLS, if is 1x1, no need to teardown links */
	if (WLAN_REG_IS_24GHZ_CH_FREQ(freq))
		vdev_nss = soc_obj->tdls_configs.tdls_vdev_nss_2g;
	else
		vdev_nss = soc_obj->tdls_configs.tdls_vdev_nss_5g;

	if (vdev_nss == HW_MODE_SS_1x1) {
		tdls_debug("Supported NSS is 1x1, no need to teardown TDLS links");
		goto ant_sw_done;
	}

	if (tdls_teardown_links(soc_obj, req->mode) == 0)
		goto ant_sw_done;

ant_sw_in_progress:
	ant_switch_state = -EAGAIN;
ant_sw_done:
	if (soc_obj->tdls_event_cb) {
		ind.vdev = vdev;
		ind.status = ant_switch_state;
		soc_obj->tdls_event_cb(soc_obj->tdls_evt_cb_data,
				       TDLS_EVENT_ANTENNA_SWITCH, &ind);
	}

	if (soc_obj->tdls_nss_switch_in_progress &&
	    soc_obj->tdls_nss_teardown_complete) {
		soc_obj->tdls_nss_switch_in_progress = false;
		soc_obj->tdls_nss_teardown_complete = false;
	}
	tdls_debug("tdls_nss_switch_in_progress: %d tdls_nss_teardown_complete: %d",
		   soc_obj->tdls_nss_switch_in_progress,
		   soc_obj->tdls_nss_teardown_complete);

get_obj_err:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	qdf_mem_free(req);

	return status;
}

QDF_STATUS tdls_antenna_switch_flush_callback(struct scheduler_msg *msg)
{
	struct tdls_antenna_switch_request *req;

	if (!msg || !msg->bodyptr) {
		tdls_err("msg: 0x%pK", msg);
		return QDF_STATUS_E_NULL_VALUE;
	}
	req = msg->bodyptr;
	wlan_objmgr_vdev_release_ref(req->vdev, WLAN_TDLS_NB_ID);
	qdf_mem_free(req);

	return QDF_STATUS_SUCCESS;
}

void wlan_tdls_offchan_parms_callback(struct wlan_objmgr_vdev *vdev)
{
	if (!vdev) {
		tdls_err("vdev is NULL");
		return;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
}

int tdls_process_set_offchannel(struct tdls_set_offchannel *req)
{
	int status;
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_soc_priv_obj *tdls_soc_obj;

	if (tdls_get_vdev_objects(req->vdev, &tdls_vdev_obj, &tdls_soc_obj) !=
		QDF_STATUS_SUCCESS) {
		status = -ENOTSUPP;
		goto free;
	}

	tdls_debug("TDLS offchannel to be configured %d", req->offchannel);

	if (req->offchannel)
		status = tdls_set_tdls_offchannel(tdls_soc_obj,
						  req->offchannel);
	else
		status = -ENOTSUPP;

free:

	if (req->callback)
		req->callback(req->vdev);
	qdf_mem_free(req);

	return status;
}

int tdls_process_set_offchan_mode(struct tdls_set_offchanmode *req)
{
	int status;

	tdls_debug("TDLS offchan mode to be configured %d", req->offchan_mode);
	status = tdls_set_tdls_offchannelmode(req->vdev, req->offchan_mode);

	if (req->callback)
		req->callback(req->vdev);
	qdf_mem_free(req);

	return status;
}

int tdls_process_set_secoffchanneloffset(
		struct tdls_set_secoffchanneloffset *req)
{
	int status;
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_soc_priv_obj *tdls_soc_obj;

	if (tdls_get_vdev_objects(req->vdev, &tdls_vdev_obj, &tdls_soc_obj) !=
		QDF_STATUS_SUCCESS) {
		status = -ENOTSUPP;
		goto free;
	}

	tdls_debug("TDLS offchannel offset to be configured %d",
		   req->offchan_offset);
	status = tdls_set_tdls_secoffchanneloffset(tdls_soc_obj,
						   req->offchan_offset);

free:

	if (req->callback)
		req->callback(req->vdev);
	qdf_mem_free(req);

	return status;
}
