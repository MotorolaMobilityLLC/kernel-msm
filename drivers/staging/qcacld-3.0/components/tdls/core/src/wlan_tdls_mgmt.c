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
 * DOC: wlan_tdls_mgmt.c
 *
 * TDLS management frames implementation
 */

#include "wlan_tdls_main.h"
#include "wlan_tdls_tgt_api.h"
#include <wlan_serialization_api.h>
#include "wlan_mgmt_txrx_utils_api.h"
#include "wlan_tdls_peer.h"
#include "wlan_tdls_ct.h"
#include "wlan_tdls_cmds_process.h"
#include "wlan_tdls_mgmt.h"

static
const char *const tdls_action_frames_type[] = { "TDLS Setup Request",
					 "TDLS Setup Response",
					 "TDLS Setup Confirm",
					 "TDLS Teardown",
					 "TDLS Peer Traffic Indication",
					 "TDLS Channel Switch Request",
					 "TDLS Channel Switch Response",
					 "TDLS Peer PSM Request",
					 "TDLS Peer PSM Response",
					 "TDLS Peer Traffic Response",
					 "TDLS Discovery Request"};

QDF_STATUS tdls_set_rssi(struct wlan_objmgr_vdev *vdev,
			 uint8_t *mac, int8_t rssi)
{
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct tdls_peer *curr_peer;

	if (!vdev || !mac) {
		tdls_err("null pointer");
		return QDF_STATUS_E_INVAL;
	}

	tdls_debug("rssi %d, peer " QDF_MAC_ADDR_FMT,
		   rssi, QDF_MAC_ADDR_REF(mac));

	tdls_vdev = wlan_objmgr_vdev_get_comp_private_obj(
			vdev, WLAN_UMAC_COMP_TDLS);

	if (!tdls_vdev) {
		tdls_err("null tdls vdev");
		return QDF_STATUS_E_EXISTS;
	}

	curr_peer = tdls_find_peer(tdls_vdev, mac);
	if (!curr_peer) {
		tdls_debug("null peer");
		return QDF_STATUS_E_EXISTS;
	}

	curr_peer->rssi = rssi;

	return QDF_STATUS_SUCCESS;
}

/**
 * tdls_process_rx_mgmt() - process tdls rx mgmt frames
 * @rx_mgmt_event: tdls rx mgmt event
 * @tdls_vdev: tdls vdev object
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tdls_process_rx_mgmt(
	struct tdls_rx_mgmt_event *rx_mgmt_event,
	struct tdls_vdev_priv_obj *tdls_vdev)
{
	struct tdls_rx_mgmt_frame *rx_mgmt;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	uint8_t *mac;
	enum tdls_actioncode action_frame_type;

	if (!rx_mgmt_event)
		return QDF_STATUS_E_INVAL;

	tdls_soc_obj = rx_mgmt_event->tdls_soc_obj;
	rx_mgmt = rx_mgmt_event->rx_mgmt;

	if (!tdls_soc_obj || !rx_mgmt) {
		tdls_err("invalid psoc object or rx mgmt");
		return QDF_STATUS_E_INVAL;
	}

	tdls_debug("soc:%pK, frame_len:%d, rx_freq:%d, vdev_id:%d, frm_type:%d, rx_rssi:%d, buf:%pK",
		   tdls_soc_obj->soc, rx_mgmt->frame_len,
		   rx_mgmt->rx_freq, rx_mgmt->vdev_id, rx_mgmt->frm_type,
		   rx_mgmt->rx_rssi, rx_mgmt->buf);

	if (rx_mgmt->buf[TDLS_PUBLIC_ACTION_FRAME_OFFSET + 1] ==
						TDLS_PUBLIC_ACTION_DISC_RESP) {
		mac = &rx_mgmt->buf[TDLS_80211_PEER_ADDR_OFFSET];
		tdls_notice("[TDLS] TDLS Discovery Response,"
		       QDF_MAC_ADDR_FMT " RSSI[%d] <--- OTA",
		       QDF_MAC_ADDR_REF(mac), rx_mgmt->rx_rssi);
			tdls_recv_discovery_resp(tdls_vdev, mac);
			tdls_set_rssi(tdls_vdev->vdev, mac, rx_mgmt->rx_rssi);
	}

	if (rx_mgmt->buf[TDLS_PUBLIC_ACTION_FRAME_OFFSET] ==
	    TDLS_ACTION_FRAME) {
		action_frame_type =
			rx_mgmt->buf[TDLS_PUBLIC_ACTION_FRAME_OFFSET + 1];
		if (action_frame_type >= TDLS_ACTION_FRAME_TYPE_MAX) {
			tdls_debug("[TDLS] unknown[%d] <--- OTA",
				   action_frame_type);
		} else {
			tdls_notice("[TDLS] %s <--- OTA",
				   tdls_action_frames_type[action_frame_type]);
		}
	}

	/* tdls_soc_obj->tdls_rx_cb ==> wlan_cfg80211_tdls_rx_callback() */
	if (tdls_soc_obj && tdls_soc_obj->tdls_rx_cb)
		tdls_soc_obj->tdls_rx_cb(tdls_soc_obj->tdls_rx_cb_data,
					 rx_mgmt);
	else
		tdls_debug("rx mgmt, but no valid up layer callback");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_process_rx_frame(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev;
	struct tdls_rx_mgmt_event *tdls_rx;
	struct tdls_vdev_priv_obj *tdls_vdev;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!(msg->bodyptr)) {
		tdls_err("invalid message body");
		return QDF_STATUS_E_INVAL;
	}

	tdls_rx = (struct tdls_rx_mgmt_event *) msg->bodyptr;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(tdls_rx->tdls_soc_obj->soc,
				tdls_rx->rx_mgmt->vdev_id, WLAN_TDLS_NB_ID);

	if (vdev) {
		tdls_debug("tdls rx mgmt frame received");
		tdls_vdev = wlan_objmgr_vdev_get_comp_private_obj(vdev,
							WLAN_UMAC_COMP_TDLS);
		if (tdls_vdev)
			status = tdls_process_rx_mgmt(tdls_rx, tdls_vdev);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	}

	qdf_mem_free(tdls_rx->rx_mgmt);
	qdf_mem_free(msg->bodyptr);
	msg->bodyptr = NULL;

	return status;
}

QDF_STATUS tdls_mgmt_rx_ops(struct wlan_objmgr_psoc *psoc,
	bool isregister)
{
	struct mgmt_txrx_mgmt_frame_cb_info frm_cb_info;
	QDF_STATUS status;
	int num_of_entries;

	tdls_debug("psoc:%pK, is register rx:%d", psoc, isregister);

	frm_cb_info.frm_type = MGMT_ACTION_TDLS_DISCRESP;
	frm_cb_info.mgmt_rx_cb = tgt_tdls_mgmt_frame_rx_cb;
	num_of_entries = 1;

	if (isregister)
		status = wlan_mgmt_txrx_register_rx_cb(psoc,
				WLAN_UMAC_COMP_TDLS, &frm_cb_info,
				num_of_entries);
	else
		status = wlan_mgmt_txrx_deregister_rx_cb(psoc,
				WLAN_UMAC_COMP_TDLS, &frm_cb_info,
				num_of_entries);

	return status;
}

static QDF_STATUS
tdls_internal_send_mgmt_tx_done(struct tdls_action_frame_request *req,
				QDF_STATUS status)
{
	struct tdls_soc_priv_obj *tdls_soc_obj;
	struct tdls_osif_indication indication;

	if (!req || !req->vdev)
		return QDF_STATUS_E_NULL_VALUE;

	indication.status = status;
	indication.vdev = req->vdev;
	tdls_soc_obj = wlan_vdev_get_tdls_soc_obj(req->vdev);
	if (tdls_soc_obj && tdls_soc_obj->tdls_event_cb)
		tdls_soc_obj->tdls_event_cb(tdls_soc_obj->tdls_evt_cb_data,
			TDLS_EVENT_MGMT_TX_ACK_CNF, &indication);
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS tdls_activate_send_mgmt_request_flush_cb(
	struct scheduler_msg *msg)
{
	struct tdls_send_mgmt_request *tdls_mgmt_req;

	tdls_mgmt_req = msg->bodyptr;

	qdf_mem_free(tdls_mgmt_req);
	msg->bodyptr = NULL;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS tdls_activate_send_mgmt_request(
				struct tdls_action_frame_request *action_req)
{
	struct wlan_objmgr_peer *peer;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};
	struct tdls_send_mgmt_request *tdls_mgmt_req;

	if (!action_req || !action_req->vdev)
		return QDF_STATUS_E_NULL_VALUE;

	tdls_soc_obj = wlan_vdev_get_tdls_soc_obj(action_req->vdev);
	if (!tdls_soc_obj) {
		status = QDF_STATUS_E_NULL_VALUE;
		goto release_cmd;
	}

	tdls_mgmt_req = qdf_mem_malloc(sizeof(struct tdls_send_mgmt_request) +
				action_req->tdls_mgmt.len);
	if (!tdls_mgmt_req) {
		status = QDF_STATUS_E_NOMEM;
		goto release_cmd;
	}

	tdls_debug("session_id %d "
		   "tdls_mgmt.dialog %d "
		   "tdls_mgmt.frame_type %d "
		   "tdls_mgmt.status_code %d "
		   "tdls_mgmt.responder %d "
		   "tdls_mgmt.peer_capability %d",
		   action_req->session_id,
		   action_req->tdls_mgmt.dialog,
		   action_req->tdls_mgmt.frame_type,
		   action_req->tdls_mgmt.status_code,
		   action_req->tdls_mgmt.responder,
		   action_req->tdls_mgmt.peer_capability);

	tdls_mgmt_req->session_id = action_req->session_id;
	tdls_mgmt_req->req_type = action_req->tdls_mgmt.frame_type;
	tdls_mgmt_req->dialog = action_req->tdls_mgmt.dialog;
	tdls_mgmt_req->status_code = action_req->tdls_mgmt.status_code;
	tdls_mgmt_req->responder = action_req->tdls_mgmt.responder;
	tdls_mgmt_req->peer_capability = action_req->tdls_mgmt.peer_capability;

	peer = wlan_objmgr_vdev_try_get_bsspeer(action_req->vdev,
						WLAN_TDLS_SB_ID);
	if (!peer) {
		tdls_err("bss peer is null");
		qdf_mem_free(tdls_mgmt_req);
		goto release_cmd;
	}

	qdf_mem_copy(tdls_mgmt_req->bssid.bytes,
		     wlan_peer_get_macaddr(peer), QDF_MAC_ADDR_SIZE);

	wlan_objmgr_peer_release_ref(peer, WLAN_TDLS_SB_ID);

	qdf_mem_copy(tdls_mgmt_req->peer_mac.bytes,
		     action_req->tdls_mgmt.peer_mac.bytes, QDF_MAC_ADDR_SIZE);

	if (action_req->tdls_mgmt.len) {
		qdf_mem_copy(tdls_mgmt_req->add_ie, action_req->tdls_mgmt.buf,
			     action_req->tdls_mgmt.len);
	}

	tdls_mgmt_req->length = sizeof(struct tdls_send_mgmt_request) +
				action_req->tdls_mgmt.len;
	if (action_req->use_default_ac)
		tdls_mgmt_req->ac = WIFI_AC_VI;
	else
		tdls_mgmt_req->ac = WIFI_AC_BK;

	/* Send the request to PE. */
	qdf_mem_zero(&msg, sizeof(msg));

	tdls_debug("sending TDLS Mgmt Frame req to PE ");
	tdls_mgmt_req->message_type = tdls_soc_obj->tdls_send_mgmt_req;

	msg.type = tdls_soc_obj->tdls_send_mgmt_req;
	msg.bodyptr = tdls_mgmt_req;
	msg.flush_callback = tdls_activate_send_mgmt_request_flush_cb;

	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status))
		qdf_mem_free(tdls_mgmt_req);

release_cmd:
	/*update tdls nss infornation based on action code */
	tdls_reset_nss(tdls_soc_obj, action_req->chk_frame.action_code);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_internal_send_mgmt_tx_done(action_req, status);
		tdls_release_serialization_command(action_req->vdev,
						   WLAN_SER_CMD_TDLS_SEND_MGMT);
	}

	return status;
}

static QDF_STATUS
tdls_send_mgmt_serialize_callback(struct wlan_serialization_command *cmd,
	 enum wlan_serialization_cb_reason reason)
{
	struct tdls_action_frame_request *req;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!cmd || !cmd->umac_cmd) {
		tdls_err("invalid params cmd: %pK, ", cmd);
		return QDF_STATUS_E_NULL_VALUE;
	}
	req = cmd->umac_cmd;

	switch (reason) {
	case WLAN_SER_CB_ACTIVATE_CMD:
		/* command moved to active list */
		status = tdls_activate_send_mgmt_request(req);
		break;

	case WLAN_SER_CB_CANCEL_CMD:
	case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
		/* command removed from pending list.
		 * notify status complete with failure
		 */
		status = tdls_internal_send_mgmt_tx_done(req,
				QDF_STATUS_E_FAILURE);
		break;

	case WLAN_SER_CB_RELEASE_MEM_CMD:
		/* command successfully completed.
		 * release tdls_action_frame_request memory
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

QDF_STATUS tdls_process_mgmt_req(
			struct tdls_action_frame_request *tdls_mgmt_req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_serialization_command cmd = {0, };
	enum wlan_serialization_status ser_cmd_status;

	/* If connected and in Infra. Only then allow this */
	status = tdls_validate_mgmt_request(tdls_mgmt_req);
	if (status != QDF_STATUS_SUCCESS) {
		status = tdls_internal_send_mgmt_tx_done(tdls_mgmt_req,
							 status);
		goto error_mgmt;
	}

	/* update the responder, status code information
	 * after the  cmd validation
	 */
	tdls_mgmt_req->tdls_mgmt.responder =
			!tdls_mgmt_req->chk_frame.responder;
	tdls_mgmt_req->tdls_mgmt.status_code =
			tdls_mgmt_req->chk_frame.status_code;

	cmd.cmd_type = WLAN_SER_CMD_TDLS_SEND_MGMT;
	/* Cmd Id not applicable for non scan cmds */
	cmd.cmd_id = 0;
	cmd.cmd_cb = tdls_send_mgmt_serialize_callback;
	cmd.umac_cmd = tdls_mgmt_req;
	cmd.source = WLAN_UMAC_COMP_TDLS;
	cmd.is_high_priority = false;
	cmd.cmd_timeout_duration = TDLS_DEFAULT_SERIALIZE_CMD_TIMEOUT;

	cmd.vdev = tdls_mgmt_req->vdev;
	cmd.is_blocking = true;

	ser_cmd_status = wlan_serialization_request(&cmd);
	tdls_debug("wlan_serialization_request status:%d", ser_cmd_status);

	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
		/* command moved to pending list.Do nothing */
		break;
	case WLAN_SER_CMD_ACTIVE:
		/* command moved to active list. Do nothing */
		break;
	case WLAN_SER_CMD_DENIED_LIST_FULL:
	case WLAN_SER_CMD_DENIED_RULES_FAILED:
	case WLAN_SER_CMD_DENIED_UNSPECIFIED:
		status = QDF_STATUS_E_FAILURE;
		goto error_mgmt;
	default:
		QDF_ASSERT(0);
		status = QDF_STATUS_E_INVAL;
		goto error_mgmt;
	}
	return status;

error_mgmt:
	wlan_objmgr_vdev_release_ref(tdls_mgmt_req->vdev, WLAN_TDLS_NB_ID);
	qdf_mem_free(tdls_mgmt_req);
	return status;
}
