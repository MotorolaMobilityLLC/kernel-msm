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
 * DOC: wlan_tdls_main.c
 *
 * TDLS core function definitions
 */

#include "wlan_tdls_main.h"
#include "wlan_tdls_cmds_process.h"
#include "wlan_tdls_peer.h"
#include "wlan_tdls_ct.h"
#include "wlan_tdls_mgmt.h"
#include "wlan_tdls_tgt_api.h"
#include "wlan_policy_mgr_public_struct.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_scan_ucfg_api.h"


/* Global tdls soc pvt object
 * this is useful for some functions which does not receive either vdev or psoc
 * objects.
 */
static struct tdls_soc_priv_obj *tdls_soc_global;

#ifdef WLAN_DEBUG
/**
 * tdls_get_cmd_type_str() - parse cmd to string
 * @cmd_type: tdls cmd type
 *
 * This function parse tdls cmd to string.
 *
 * Return: command string
 */
static char *tdls_get_cmd_type_str(enum tdls_command_type cmd_type)
{
	switch (cmd_type) {
	case TDLS_CMD_TX_ACTION:
		return "TDLS_CMD_TX_ACTION";
	case TDLS_CMD_ADD_STA:
		return "TDLS_CMD_ADD_STA";
	case TDLS_CMD_CHANGE_STA:
		return "TDLS_CMD_CHANGE_STA";
	case TDLS_CMD_ENABLE_LINK:
		return "TDLS_CMD_ENABLE_LINK";
	case TDLS_CMD_DISABLE_LINK:
		return "TDLS_CMD_DISABLE_LINK";
	case TDLS_CMD_CONFIG_FORCE_PEER:
		return "TDLS_CMD_CONFIG_FORCE_PEER";
	case TDLS_CMD_REMOVE_FORCE_PEER:
		return "TDLS_CMD_REMOVE_FORCE_PEER";
	case TDLS_CMD_STATS_UPDATE:
		return "TDLS_CMD_STATS_UPDATE";
	case TDLS_CMD_CONFIG_UPDATE:
		return "TDLS_CMD_CONFIG_UPDATE";
	case TDLS_CMD_SET_RESPONDER:
		return "TDLS_CMD_SET_RESPONDER";
	case TDLS_CMD_SCAN_DONE:
		return "TDLS_CMD_SCAN_DONE";
	case TDLS_NOTIFY_STA_CONNECTION:
		return "TDLS_NOTIFY_STA_CONNECTION";
	case TDLS_NOTIFY_STA_DISCONNECTION:
		return "TDLS_NOTIFY_STA_DISCONNECTION";
	case TDLS_CMD_SET_TDLS_MODE:
		return "TDLS_CMD_SET_TDLS_MODE";
	case TDLS_CMD_SESSION_DECREMENT:
		return "TDLS_CMD_SESSION_DECREMENT";
	case TDLS_CMD_SESSION_INCREMENT:
		return "TDLS_CMD_SESSION_INCREMENT";
	case TDLS_CMD_TEARDOWN_LINKS:
		return "TDLS_CMD_TEARDOWN_LINKS";
	case TDLS_NOTIFY_RESET_ADAPTERS:
		return "TDLS_NOTIFY_RESET_ADAPTERS";
	case TDLS_CMD_ANTENNA_SWITCH:
		return "TDLS_CMD_ANTENNA_SWITCH";

	default:
		return "Invalid TDLS command";
	}
}

/**
 * tdls_get_event_type_str() - parase event to string
 * @event_type: tdls event type
 *
 * This function parse tdls event to string.
 *
 * Return: event string
 */
static char *tdls_get_event_type_str(enum tdls_event_type event_type)
{
	switch (event_type) {
	case TDLS_SHOULD_DISCOVER:
		return "TDLS_SHOULD_DISCOVER";
	case TDLS_SHOULD_TEARDOWN:
		return "TDLS_SHOULD_TEARDOWN";
	case TDLS_PEER_DISCONNECTED:
		return "TDLS_PEER_DISCONNECTED";
	case TDLS_CONNECTION_TRACKER_NOTIFY:
		return "TDLS_CONNECTION_TRACKER_NOTIFY";

	default:
		return "Invalid TDLS event";
	}
}
#else
static char *tdls_get_cmd_type_str(enum tdls_command_type cmd_type)
{
	return "";
}

static char *tdls_get_event_type_str(enum tdls_event_type event_type)
{
	return "";
}
#endif

QDF_STATUS tdls_psoc_obj_create_notification(struct wlan_objmgr_psoc *psoc,
					     void *arg_list)
{
	QDF_STATUS status;
	struct tdls_soc_priv_obj *tdls_soc_obj;

	tdls_soc_obj = qdf_mem_malloc(sizeof(*tdls_soc_obj));
	if (!tdls_soc_obj)
		return QDF_STATUS_E_NOMEM;

	tdls_soc_obj->soc = psoc;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
						       WLAN_UMAC_COMP_TDLS,
						       (void *)tdls_soc_obj,
						       QDF_STATUS_SUCCESS);

	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("Failed to attach psoc tdls component");
		qdf_mem_free(tdls_soc_obj);
		return status;
	}

	tdls_soc_global = tdls_soc_obj;
	tdls_notice("TDLS obj attach to psoc successfully");

	return status;
}

QDF_STATUS tdls_psoc_obj_destroy_notification(struct wlan_objmgr_psoc *psoc,
					      void *arg_list)
{
	QDF_STATUS status;
	struct tdls_soc_priv_obj *tdls_soc_obj;

	tdls_soc_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
						WLAN_UMAC_COMP_TDLS);
	if (!tdls_soc_obj) {
		tdls_err("Failed to get tdls obj in psoc");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
						       WLAN_UMAC_COMP_TDLS,
						       tdls_soc_obj);

	if (QDF_IS_STATUS_ERROR(status))
		tdls_err("Failed to detach psoc tdls component");
	qdf_mem_free(tdls_soc_obj);

	return status;
}

static QDF_STATUS tdls_vdev_init(struct tdls_vdev_priv_obj *vdev_obj)
{
	uint8_t i;
	struct tdls_config_params *config;
	struct tdls_user_config *user_config;
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_vdev_get_tdls_soc_obj(vdev_obj->vdev);
	if (!soc_obj) {
		tdls_err("tdls soc obj NULL");
		return QDF_STATUS_E_FAILURE;
	}

	config = &vdev_obj->threshold_config;
	user_config = &soc_obj->tdls_configs;
	config->tx_period_t = user_config->tdls_tx_states_period;
	config->tx_packet_n = user_config->tdls_tx_pkt_threshold;
	config->discovery_tries_n = user_config->tdls_max_discovery_attempt;
	config->idle_timeout_t = user_config->tdls_idle_timeout;
	config->idle_packet_n = user_config->tdls_idle_pkt_threshold;
	config->rssi_trigger_threshold =
		user_config->tdls_rssi_trigger_threshold;
	config->rssi_teardown_threshold =
		user_config->tdls_rssi_teardown_threshold;
	config->rssi_delta = user_config->tdls_rssi_delta;

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		qdf_list_create(&vdev_obj->peer_list[i],
				WLAN_TDLS_PEER_SUB_LIST_SIZE);
	}
	qdf_mc_timer_init(&vdev_obj->peer_update_timer, QDF_TIMER_TYPE_SW,
			  tdls_ct_handler, soc_obj->soc);
	qdf_mc_timer_init(&vdev_obj->peer_discovery_timer, QDF_TIMER_TYPE_SW,
			  tdls_discovery_timeout_peer_cb, soc_obj->soc);

	return QDF_STATUS_SUCCESS;
}

static void tdls_vdev_deinit(struct tdls_vdev_priv_obj *vdev_obj)
{
	qdf_mc_timer_stop_sync(&vdev_obj->peer_update_timer);
	qdf_mc_timer_stop_sync(&vdev_obj->peer_discovery_timer);

	qdf_mc_timer_destroy(&vdev_obj->peer_update_timer);
	qdf_mc_timer_destroy(&vdev_obj->peer_discovery_timer);

	tdls_peer_idle_timers_destroy(vdev_obj);
	tdls_free_peer_list(vdev_obj);
}

QDF_STATUS tdls_vdev_obj_create_notification(struct wlan_objmgr_vdev *vdev,
					     void *arg)
{
	QDF_STATUS status;
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct wlan_objmgr_pdev *pdev;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	uint32_t tdls_feature_flags;

	tdls_debug("tdls vdev mode %d", wlan_vdev_mlme_get_opmode(vdev));
	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE &&
	    wlan_vdev_mlme_get_opmode(vdev) != QDF_P2P_CLIENT_MODE)
		return QDF_STATUS_SUCCESS;

	tdls_soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (!tdls_soc_obj) {
		tdls_err("get soc by vdev failed");
		return QDF_STATUS_E_NOMEM;
	}

	tdls_feature_flags = tdls_soc_obj->tdls_configs.tdls_feature_flags;
	if (!TDLS_IS_ENABLED(tdls_feature_flags)) {
		tdls_debug("disabled in ini");
		return QDF_STATUS_E_NOSUPPORT;
	}

	if (tdls_soc_obj->tdls_osif_init_cb) {
		status = tdls_soc_obj->tdls_osif_init_cb(vdev);
		if (QDF_IS_STATUS_ERROR(status))
			return status;
	}

	/* TODO: Add concurrency check */

	tdls_vdev_obj = qdf_mem_malloc(sizeof(*tdls_vdev_obj));
	if (!tdls_vdev_obj) {
		status = QDF_STATUS_E_NOMEM;
		goto err_attach;
	}

	status = wlan_objmgr_vdev_component_obj_attach(vdev,
						       WLAN_UMAC_COMP_TDLS,
						       (void *)tdls_vdev_obj,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("Failed to attach vdev tdls component");
		goto err_attach;
	}
	tdls_vdev_obj->vdev = vdev;
	status = tdls_vdev_init(tdls_vdev_obj);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_vdev_init;

	status = qdf_event_create(&tdls_vdev_obj->tdls_teardown_comp);
	if (QDF_IS_STATUS_ERROR(status))
		goto err_event_create;

	pdev = wlan_vdev_get_pdev(vdev);

	status = ucfg_scan_register_event_handler(pdev,
				tdls_scan_complete_event_handler,
				tdls_soc_obj);

	if (QDF_STATUS_SUCCESS != status) {
		tdls_err("scan event register failed ");
		goto err_register;
	}

	tdls_debug("tdls object attach to vdev successfully");
	return status;

err_register:
	qdf_event_destroy(&tdls_vdev_obj->tdls_teardown_comp);
err_event_create:
	tdls_vdev_deinit(tdls_vdev_obj);
err_vdev_init:
	wlan_objmgr_vdev_component_obj_detach(vdev,
					      WLAN_UMAC_COMP_TDLS,
					      (void *)tdls_vdev_obj);
err_attach:
	if (tdls_soc_obj->tdls_osif_deinit_cb)
		tdls_soc_obj->tdls_osif_deinit_cb(vdev);
	if (tdls_vdev_obj) {
		qdf_mem_free(tdls_vdev_obj);
		tdls_vdev_obj = NULL;
	}

	return status;
}

QDF_STATUS tdls_vdev_obj_destroy_notification(struct wlan_objmgr_vdev *vdev,
					      void *arg)
{
	QDF_STATUS status;
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	uint32_t tdls_feature_flags;

	tdls_debug("tdls vdev mode %d", wlan_vdev_mlme_get_opmode(vdev));
	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE &&
	    wlan_vdev_mlme_get_opmode(vdev) != QDF_P2P_CLIENT_MODE)
		return QDF_STATUS_SUCCESS;

	tdls_soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (!tdls_soc_obj) {
		tdls_err("get soc by vdev failed");
		return QDF_STATUS_E_NOMEM;
	}

	tdls_feature_flags = tdls_soc_obj->tdls_configs.tdls_feature_flags;
	if (!TDLS_IS_ENABLED(tdls_feature_flags)) {
		tdls_debug("disabled in ini");
		return QDF_STATUS_E_NOSUPPORT;
	}

	tdls_vdev_obj = wlan_objmgr_vdev_get_comp_private_obj(vdev,
							WLAN_UMAC_COMP_TDLS);
	if (!tdls_vdev_obj) {
		tdls_err("Failed to get tdls vdev object");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_event_destroy(&tdls_vdev_obj->tdls_teardown_comp);
	tdls_vdev_deinit(tdls_vdev_obj);

	status = wlan_objmgr_vdev_component_obj_detach(vdev,
						       WLAN_UMAC_COMP_TDLS,
						       tdls_vdev_obj);
	if (QDF_IS_STATUS_ERROR(status))
		tdls_err("Failed to detach vdev tdls component");

	if (tdls_soc_obj->tdls_osif_deinit_cb)
		tdls_soc_obj->tdls_osif_deinit_cb(vdev);
	qdf_mem_free(tdls_vdev_obj);

	return status;
}

/**
 * __tdls_get_all_peers_from_list() - get all the tdls peers from the list
 * @get_tdls_peers: get_tdls_peers object
 *
 * Return: int
 */
static int __tdls_get_all_peers_from_list(
			struct tdls_get_all_peers *get_tdls_peers)
{
	int i;
	int len, init_len;
	qdf_list_t *head;
	qdf_list_node_t *p_node;
	struct tdls_peer *curr_peer;
	char *buf;
	int buf_len;
	struct tdls_vdev_priv_obj *tdls_vdev;
	QDF_STATUS status;

	tdls_notice("Enter ");

	buf = get_tdls_peers->buf;
	buf_len = get_tdls_peers->buf_len;

	if (wlan_vdev_is_up(get_tdls_peers->vdev) != QDF_STATUS_SUCCESS) {
		len = qdf_scnprintf(buf, buf_len,
				"\nSTA is not associated\n");
		return len;
	}

	tdls_vdev = wlan_vdev_get_tdls_vdev_obj(get_tdls_peers->vdev);

	if (!tdls_vdev) {
		len = qdf_scnprintf(buf, buf_len, "TDLS not enabled\n");
		return len;
	}

	init_len = buf_len;
	len = qdf_scnprintf(buf, buf_len,
			"\n%-18s%-3s%-4s%-3s%-5s\n",
			"MAC", "Id", "cap", "up", "RSSI");
	buf += len;
	buf_len -= len;
	len = qdf_scnprintf(buf, buf_len,
			    "---------------------------------\n");
	buf += len;
	buf_len -= len;

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &tdls_vdev->peer_list[i];
		status = qdf_list_peek_front(head, &p_node);
		while (QDF_IS_STATUS_SUCCESS(status)) {
			curr_peer = qdf_container_of(p_node,
						     struct tdls_peer, node);
			if (buf_len < 32 + 1)
				break;
			len = qdf_scnprintf(buf, buf_len,
				QDF_MAC_ADDR_FMT "%4s%3s%5d\n",
				QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes),
				(curr_peer->tdls_support ==
				 TDLS_CAP_SUPPORTED) ? "Y" : "N",
				TDLS_IS_LINK_CONNECTED(curr_peer) ? "Y" :
				"N", curr_peer->rssi);
			buf += len;
			buf_len -= len;
			status = qdf_list_peek_next(head, p_node, &p_node);
		}
	}

	tdls_notice("Exit ");
	return init_len - buf_len;
}

/**
 * tdls_get_all_peers_from_list() - get all the tdls peers from the list
 * @get_tdls_peers: get_tdls_peers object
 *
 * Return: None
 */
static void tdls_get_all_peers_from_list(
			struct tdls_get_all_peers *get_tdls_peers)
{
	int32_t len;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	struct tdls_osif_indication indication;

	if (!get_tdls_peers->vdev) {
		qdf_mem_free(get_tdls_peers);
		return;
	}
	len = __tdls_get_all_peers_from_list(get_tdls_peers);

	indication.status = len;
	indication.vdev = get_tdls_peers->vdev;

	tdls_soc_obj = wlan_vdev_get_tdls_soc_obj(get_tdls_peers->vdev);
	if (tdls_soc_obj && tdls_soc_obj->tdls_event_cb)
		tdls_soc_obj->tdls_event_cb(tdls_soc_obj->tdls_evt_cb_data,
			TDLS_EVENT_USER_CMD, &indication);

	qdf_mem_free(get_tdls_peers);
}

/**
 * tdls_process_reset_all_peers() - Reset all tdls peers
 * @delete_all_peers_ind: Delete all peers indication
 *
 * This function is called to reset all tdls peers and
 * notify upper layers of teardown inidcation
 *
 * Return: QDF_STATUS
 */

static QDF_STATUS tdls_process_reset_all_peers(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t staidx;
	struct tdls_peer *curr_peer = NULL;
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct tdls_soc_priv_obj *tdls_soc;
	uint8_t reset_session_id;

	status = tdls_get_vdev_objects(vdev, &tdls_vdev, &tdls_soc);
	if (QDF_STATUS_SUCCESS != status) {
		tdls_err("tdls objects are NULL ");
		return status;
	}

	reset_session_id = tdls_vdev->session_id;
	for (staidx = 0; staidx < tdls_soc->max_num_tdls_sta;
							staidx++) {
		if (!tdls_soc->tdls_conn_info[staidx].valid_entry)
			continue;
		if (tdls_soc->tdls_conn_info[staidx].session_id !=
		    reset_session_id)
			continue;

		curr_peer =
		tdls_find_all_peer(tdls_soc,
				   tdls_soc->tdls_conn_info[staidx].
				   peer_mac.bytes);
		if (!curr_peer)
			continue;

		tdls_notice("indicate TDLS teardown "QDF_MAC_ADDR_FMT,
			    QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes));

		/* Indicate teardown to supplicant */
		tdls_indicate_teardown(tdls_vdev,
				       curr_peer,
				       TDLS_TEARDOWN_PEER_UNSPEC_REASON);

		tdls_reset_peer(tdls_vdev, curr_peer->peer_mac.bytes);

		tdls_decrement_peer_count(tdls_soc);
		tdls_soc->tdls_conn_info[staidx].valid_entry = false;
		tdls_soc->tdls_conn_info[staidx].session_id = 255;
		tdls_soc->tdls_conn_info[staidx].index =
					INVALID_TDLS_PEER_INDEX;

		qdf_mem_zero(&tdls_soc->tdls_conn_info[staidx].peer_mac,
			     sizeof(struct qdf_mac_addr));
	}
	return status;
}

/**
 * tdls_reset_all_peers() - Reset all tdls peers
 * @delete_all_peers_ind: Delete all peers indication
 *
 * This function is called to reset all tdls peers and
 * notify upper layers of teardown inidcation
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS tdls_reset_all_peers(
		struct tdls_delete_all_peers_params *delete_all_peers_ind)
{
	QDF_STATUS status;

	if (!delete_all_peers_ind || !delete_all_peers_ind->vdev) {
		tdls_err("invalid param");
		qdf_mem_free(delete_all_peers_ind);
		return QDF_STATUS_E_INVAL;
	}

	status = tdls_process_reset_all_peers(delete_all_peers_ind->vdev);

	wlan_objmgr_vdev_release_ref(delete_all_peers_ind->vdev,
				     WLAN_TDLS_SB_ID);
	qdf_mem_free(delete_all_peers_ind);

	return status;
}

QDF_STATUS tdls_process_cmd(struct scheduler_msg *msg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!msg || !msg->bodyptr) {
		tdls_err("msg: 0x%pK", msg);
		QDF_ASSERT(0);
		return QDF_STATUS_E_NULL_VALUE;
	}
	tdls_debug("TDLS process command: %s(%d)",
		   tdls_get_cmd_type_str(msg->type), msg->type);

	switch (msg->type) {
	case TDLS_CMD_TX_ACTION:
		tdls_process_mgmt_req(msg->bodyptr);
		break;
	case TDLS_CMD_ADD_STA:
		tdls_process_add_peer(msg->bodyptr);
		break;
	case TDLS_CMD_CHANGE_STA:
		tdls_process_update_peer(msg->bodyptr);
		break;
	case TDLS_CMD_ENABLE_LINK:
		tdls_process_enable_link(msg->bodyptr);
		break;
	case TDLS_CMD_DISABLE_LINK:
		tdls_process_del_peer(msg->bodyptr);
		break;
	case TDLS_CMD_CONFIG_FORCE_PEER:
		tdls_process_setup_peer(msg->bodyptr);
		break;
	case TDLS_CMD_REMOVE_FORCE_PEER:
		tdls_process_remove_force_peer(msg->bodyptr);
		break;
	case TDLS_CMD_STATS_UPDATE:
		break;
	case TDLS_CMD_CONFIG_UPDATE:
		break;
	case TDLS_CMD_SET_RESPONDER:
		tdls_set_responder(msg->bodyptr);
		break;
	case TDLS_CMD_SCAN_DONE:
		tdls_scan_done_callback(msg->bodyptr);
		break;
	case TDLS_NOTIFY_STA_CONNECTION:
		tdls_notify_sta_connect(msg->bodyptr);
		break;
	case TDLS_NOTIFY_STA_DISCONNECTION:
		tdls_notify_sta_disconnect(msg->bodyptr);
		break;
	case TDLS_CMD_SET_TDLS_MODE:
		tdls_set_operation_mode(msg->bodyptr);
		break;
	case TDLS_CMD_SESSION_DECREMENT:
		tdls_process_decrement_active_session(msg->bodyptr);
		/* take decision on connection tracker */
		/* fallthrough */
	case TDLS_CMD_SESSION_INCREMENT:
		tdls_process_policy_mgr_notification(msg->bodyptr);
		break;
	case TDLS_CMD_TEARDOWN_LINKS:
		tdls_teardown_connections(msg->bodyptr);
		break;
	case TDLS_NOTIFY_RESET_ADAPTERS:
		tdls_notify_reset_adapter(msg->bodyptr);
		break;
	case TDLS_CMD_ANTENNA_SWITCH:
		tdls_process_antenna_switch(msg->bodyptr);
		break;
	case TDLS_CMD_GET_ALL_PEERS:
		tdls_get_all_peers_from_list(msg->bodyptr);
		break;
	case TDLS_CMD_SET_OFFCHANNEL:
		tdls_process_set_offchannel(msg->bodyptr);
		break;
	case TDLS_CMD_SET_OFFCHANMODE:
		tdls_process_set_offchan_mode(msg->bodyptr);
		break;
	case TDLS_CMD_SET_SECOFFCHANOFFSET:
		tdls_process_set_secoffchanneloffset(msg->bodyptr);
		break;
	case TDLS_DELETE_ALL_PEERS_INDICATION:
		tdls_reset_all_peers(msg->bodyptr);
		break;
	default:
		break;
	}

	return status;
}

QDF_STATUS tdls_process_evt(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev;
	struct tdls_event_notify *notify;
	struct tdls_event_info *event;

	if (!msg || !msg->bodyptr) {
		tdls_err("msg is not valid: %pK", msg);
		return QDF_STATUS_E_NULL_VALUE;
	}
	notify = msg->bodyptr;
	vdev = notify->vdev;
	if (!vdev) {
		tdls_err("NULL vdev object");
		qdf_mem_free(notify);
		return QDF_STATUS_E_NULL_VALUE;
	}
	event = &notify->event;

	tdls_debug("evt type: %s(%d)",
		   tdls_get_event_type_str(event->message_type),
		   event->message_type);

	switch (event->message_type) {
	case TDLS_SHOULD_DISCOVER:
		tdls_process_should_discover(vdev, event);
		break;
	case TDLS_SHOULD_TEARDOWN:
	case TDLS_PEER_DISCONNECTED:
		tdls_process_should_teardown(vdev, event);
		break;
	case TDLS_CONNECTION_TRACKER_NOTIFY:
		tdls_process_connection_tracker_notify(vdev, event);
		break;
	default:
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);
	qdf_mem_free(notify);

	return QDF_STATUS_SUCCESS;
}

void tdls_timer_restart(struct wlan_objmgr_vdev *vdev,
				 qdf_mc_timer_t *timer,
				 uint32_t expiration_time)
{
	if (QDF_TIMER_STATE_RUNNING !=
	    qdf_mc_timer_get_current_state(timer))
		qdf_mc_timer_start(timer, expiration_time);
}

/**
 * wlan_hdd_tdls_monitor_timers_stop() - stop all monitoring timers
 * @hdd_tdls_ctx: TDLS context
 *
 * Return: none
 */
static void tdls_monitor_timers_stop(struct tdls_vdev_priv_obj *tdls_vdev)
{
	qdf_mc_timer_stop(&tdls_vdev->peer_discovery_timer);
}

/**
 * tdls_peer_idle_timers_stop() - stop peer idle timers
 * @tdls_vdev: TDLS vdev object
 *
 * Loop through the idle peer list and stop their timers
 *
 * Return: None
 */
static void tdls_peer_idle_timers_stop(struct tdls_vdev_priv_obj *tdls_vdev)
{
	int i;
	qdf_list_t *head;
	qdf_list_node_t *p_node;
	struct tdls_peer *curr_peer;
	QDF_STATUS status;

	tdls_vdev->discovery_peer_cnt = 0;

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &tdls_vdev->peer_list[i];
		status = qdf_list_peek_front(head, &p_node);
		while (QDF_IS_STATUS_SUCCESS(status)) {
			curr_peer = qdf_container_of(p_node, struct tdls_peer,
						     node);
			if (curr_peer->is_peer_idle_timer_initialised)
				qdf_mc_timer_stop(&curr_peer->peer_idle_timer);
			status = qdf_list_peek_next(head, p_node, &p_node);
		}
	}

}

/**
 * wlan_hdd_tdls_ct_timers_stop() - stop tdls connection tracker timers
 * @tdls_vdev: TDLS vdev
 *
 * Return: None
 */
static void tdls_ct_timers_stop(struct tdls_vdev_priv_obj *tdls_vdev)
{
	qdf_mc_timer_stop(&tdls_vdev->peer_update_timer);
	tdls_peer_idle_timers_stop(tdls_vdev);
}

/**
 * wlan_hdd_tdls_timers_stop() - stop all the tdls timers running
 * @tdls_vdev: TDLS vdev
 *
 * Return: none
 */
void tdls_timers_stop(struct tdls_vdev_priv_obj *tdls_vdev)
{
	tdls_monitor_timers_stop(tdls_vdev);
	tdls_ct_timers_stop(tdls_vdev);
}

QDF_STATUS tdls_get_vdev_objects(struct wlan_objmgr_vdev *vdev,
				   struct tdls_vdev_priv_obj **tdls_vdev_obj,
				   struct tdls_soc_priv_obj **tdls_soc_obj)
{
	enum QDF_OPMODE device_mode;

	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	*tdls_vdev_obj = wlan_vdev_get_tdls_vdev_obj(vdev);
	if (NULL == (*tdls_vdev_obj))
		return QDF_STATUS_E_FAILURE;

	*tdls_soc_obj = wlan_vdev_get_tdls_soc_obj(vdev);
	if (NULL == (*tdls_soc_obj))
		return QDF_STATUS_E_FAILURE;

	device_mode = wlan_vdev_mlme_get_opmode(vdev);

	if (device_mode != QDF_STA_MODE &&
	    device_mode != QDF_P2P_CLIENT_MODE)
		return QDF_STATUS_E_FAILURE;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_set_offchan_mode(struct wlan_objmgr_psoc *psoc,
				     struct tdls_channel_switch_params *param)
{
	QDF_STATUS status;

	/*  wmi_unified_set_tdls_offchan_mode_cmd() will be called directly */
	status = tgt_tdls_set_offchan_mode(psoc, param);

	if (!QDF_IS_STATUS_SUCCESS(status))
		status = QDF_STATUS_E_FAILURE;

	return status;
}

/**
 * tdls_update_fw_tdls_state() - update tdls status info
 * @tdls_soc_obj: TDLS soc object
 * @tdls_info_to_fw: TDLS state info to update in f/w.
 *
 * send message to WMA to set TDLS state in f/w
 *
 * Return: QDF_STATUS.
 */
static
QDF_STATUS tdls_update_fw_tdls_state(struct tdls_soc_priv_obj *tdls_soc_obj,
				     struct tdls_info *tdls_info_to_fw)
{
	QDF_STATUS status;

	/*  wmi_unified_update_fw_tdls_state_cmd() will be called directly */
	status = tgt_tdls_set_fw_state(tdls_soc_obj->soc, tdls_info_to_fw);

	if (!QDF_IS_STATUS_SUCCESS(status))
		status = QDF_STATUS_E_FAILURE;

	return status;
}

bool tdls_check_is_tdls_allowed(struct wlan_objmgr_vdev *vdev)
{
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	bool state = false;

	if (QDF_STATUS_SUCCESS != wlan_objmgr_vdev_try_get_ref(vdev,
							       WLAN_TDLS_NB_ID))
		return state;

	if (QDF_STATUS_SUCCESS != tdls_get_vdev_objects(vdev, &tdls_vdev_obj,
						   &tdls_soc_obj)) {
		wlan_objmgr_vdev_release_ref(vdev,
					     WLAN_TDLS_NB_ID);
		return state;
	}

	if (policy_mgr_get_connection_count(tdls_soc_obj->soc) == 1)
		state = true;
	else
		tdls_warn("Concurrent sessions are running or TDLS disabled");
	/* If any concurrency is detected */
	/* print session information */
	wlan_objmgr_vdev_release_ref(vdev,
				     WLAN_TDLS_NB_ID);
	return state;
}

/**
 * cds_set_tdls_ct_mode() - Set the tdls connection tracker mode
 * @hdd_ctx: hdd context
 *
 * This routine is called to set the tdls connection tracker operation status
 *
 * Return: NONE
 */
void tdls_set_ct_mode(struct wlan_objmgr_psoc *psoc)
{
	bool state = false;
	struct tdls_soc_priv_obj *tdls_soc_obj;

	tdls_soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!tdls_soc_obj)
		return;

	/* If any concurrency is detected, skip tdls pkt tracker */
	if (policy_mgr_get_connection_count(psoc) > 1) {
		state = false;
		goto set_state;
	}

	if (TDLS_SUPPORT_DISABLED == tdls_soc_obj->tdls_current_mode ||
	    TDLS_SUPPORT_SUSPENDED == tdls_soc_obj->tdls_current_mode ||
	    !TDLS_IS_IMPLICIT_TRIG_ENABLED(
			tdls_soc_obj->tdls_configs.tdls_feature_flags)) {
		state = false;
		goto set_state;
	} else if (policy_mgr_mode_specific_connection_count(psoc,
							     PM_STA_MODE,
							     NULL) == 1) {
		state = true;
	} else if (policy_mgr_mode_specific_connection_count(psoc,
							     PM_P2P_CLIENT_MODE,
							     NULL) == 1){
		state = true;
	} else {
		state = false;
		goto set_state;
	}

	/* In case of TDLS external control, peer should be added
	 * by the user space to start connection tracker.
	 */
	if (TDLS_IS_EXTERNAL_CONTROL_ENABLED(
			tdls_soc_obj->tdls_configs.tdls_feature_flags)) {
		if (tdls_soc_obj->tdls_external_peer_count)
			state = true;
		else
			state = false;
	}

set_state:
	tdls_soc_obj->enable_tdls_connection_tracker = state;

	tdls_debug("enable_tdls_connection_tracker %d",
		 tdls_soc_obj->enable_tdls_connection_tracker);
}

QDF_STATUS
tdls_process_policy_mgr_notification(struct wlan_objmgr_psoc *psoc)
{
	struct tdls_vdev_priv_obj *tdls_priv_vdev;
	struct wlan_objmgr_vdev *tdls_obj_vdev;
	struct tdls_soc_priv_obj *tdls_priv_soc;

	if (!psoc) {
		tdls_err("psoc: %pK", psoc);
		return QDF_STATUS_E_NULL_VALUE;
	}
	tdls_obj_vdev = tdls_get_vdev(psoc, WLAN_TDLS_NB_ID);
	tdls_debug("enter ");
	tdls_set_ct_mode(psoc);
	if (tdls_obj_vdev && (tdls_get_vdev_objects(tdls_obj_vdev,
	    &tdls_priv_vdev, &tdls_priv_soc) == QDF_STATUS_SUCCESS) &&
	    tdls_priv_soc->enable_tdls_connection_tracker)
		tdls_implicit_enable(tdls_priv_vdev);

	if (tdls_obj_vdev)
		wlan_objmgr_vdev_release_ref(tdls_obj_vdev, WLAN_TDLS_NB_ID);

	tdls_debug("exit ");
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
tdls_process_decrement_active_session(struct wlan_objmgr_psoc *psoc)
{
	struct tdls_soc_priv_obj *tdls_priv_soc;
	struct tdls_vdev_priv_obj *tdls_priv_vdev;
	struct wlan_objmgr_vdev *tdls_obj_vdev;
	uint8_t vdev_id;

	tdls_debug("Enter");
	if (!psoc)
		return QDF_STATUS_E_NULL_VALUE;
	if(!policy_mgr_is_hw_dbs_2x2_capable(psoc) &&
	   !policy_mgr_is_hw_dbs_required_for_band(
				psoc, HW_MODE_MAC_BAND_2G) &&
	   policy_mgr_is_current_hwmode_dbs(psoc)) {
		tdls_debug("Current HW mode is 1*1 DBS. Wait for Opportunistic timer to expire to enable TDLS in FW");
		return QDF_STATUS_SUCCESS;
	}
	tdls_obj_vdev = tdls_get_vdev(psoc, WLAN_TDLS_NB_ID);
	if (tdls_obj_vdev) {
		tdls_debug("Enable TDLS in FW and host as only one active sta/p2p_cli interface is present");
		vdev_id = wlan_vdev_get_id(tdls_obj_vdev);
		if (tdls_get_vdev_objects(tdls_obj_vdev, &tdls_priv_vdev,
		    &tdls_priv_soc) == QDF_STATUS_SUCCESS)
			tdls_send_update_to_fw(tdls_priv_vdev, tdls_priv_soc,
					       false, false, true, vdev_id);
		wlan_objmgr_vdev_release_ref(tdls_obj_vdev, WLAN_TDLS_NB_ID);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * tdls_get_vdev() - Get tdls specific vdev object manager
 * @psoc: wlan psoc object manager
 * @dbg_id: debug id
 *
 * If TDLS possible, return the corresponding vdev
 * to enable TDLS in the system.
 *
 * Return: vdev manager pointer or NULL.
 */
struct wlan_objmgr_vdev *tdls_get_vdev(struct wlan_objmgr_psoc *psoc,
					  wlan_objmgr_ref_dbgid dbg_id)
{
	uint32_t vdev_id;

	if (policy_mgr_get_connection_count(psoc) > 1)
		return NULL;

	vdev_id = policy_mgr_mode_specific_vdev_id(psoc, PM_STA_MODE);

	if (WLAN_INVALID_VDEV_ID != vdev_id)
		return wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
							    vdev_id,
							    dbg_id);

	vdev_id = policy_mgr_mode_specific_vdev_id(psoc, PM_P2P_CLIENT_MODE);

	if (WLAN_INVALID_VDEV_ID != vdev_id)
		return wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
							    vdev_id,
							    dbg_id);

	return NULL;
}

static QDF_STATUS tdls_post_msg_flush_cb(struct scheduler_msg *msg)
{
	void *ptr = msg->bodyptr;
	struct wlan_objmgr_vdev *vdev = NULL;

	switch (msg->type) {
	case TDLS_NOTIFY_STA_DISCONNECTION:
		vdev = ((struct tdls_sta_notify_params *)ptr)->vdev;
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(ptr);
		break;

	case TDLS_DELETE_ALL_PEERS_INDICATION:
		vdev = ((struct tdls_delete_all_peers_params *)ptr)->vdev;
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);
		qdf_mem_free(ptr);
		break;

	case TDLS_CMD_SCAN_DONE:
	case TDLS_CMD_SESSION_INCREMENT:
	case TDLS_CMD_SESSION_DECREMENT:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * tdls_process_session_update() - update session count information
 * @psoc: soc object
 * @notification: TDLS os if notification
 *
 * update the session information in connection tracker
 *
 * Return: None
 */
static void tdls_process_session_update(struct wlan_objmgr_psoc *psoc,
				 enum tdls_command_type cmd_type)
{
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	msg.bodyptr = psoc;
	msg.callback = tdls_process_cmd;
	msg.flush_callback = tdls_post_msg_flush_cb;
	msg.type = (uint16_t)cmd_type;

	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status))
		tdls_alert("message post failed ");
}

void tdls_notify_increment_session(struct wlan_objmgr_psoc *psoc)
{
	tdls_process_session_update(psoc, TDLS_CMD_SESSION_INCREMENT);
}

void tdls_notify_decrement_session(struct wlan_objmgr_psoc *psoc)
{
	tdls_process_session_update(psoc, TDLS_CMD_SESSION_DECREMENT);
}

void tdls_send_update_to_fw(struct tdls_vdev_priv_obj *tdls_vdev_obj,
			    struct tdls_soc_priv_obj *tdls_soc_obj,
			    bool tdls_prohibited,
			    bool tdls_chan_swit_prohibited,
			    bool sta_connect_event,
			    uint8_t session_id)
{
	struct tdls_info *tdls_info_to_fw;
	struct tdls_config_params *threshold_params;
	uint32_t tdls_feature_flags;
	QDF_STATUS status;
	uint8_t set_state_cnt;

	tdls_feature_flags = tdls_soc_obj->tdls_configs.tdls_feature_flags;
	if (!TDLS_IS_ENABLED(tdls_feature_flags)) {
		tdls_debug("TDLS mode is not enabled");
		return;
	}

	set_state_cnt = tdls_soc_obj->set_state_info.set_state_cnt;
	if ((set_state_cnt == 0 && !sta_connect_event) ||
	    (set_state_cnt && sta_connect_event)) {
		tdls_debug("FW TDLS state is already in requested state");
		return;
	}

	/* If AP or caller indicated TDLS Prohibited then disable tdls mode */
	if (sta_connect_event) {
		if (tdls_prohibited) {
			tdls_soc_obj->tdls_current_mode =
					TDLS_SUPPORT_DISABLED;
		} else {
			if (!TDLS_IS_IMPLICIT_TRIG_ENABLED(tdls_feature_flags))
				tdls_soc_obj->tdls_current_mode =
					TDLS_SUPPORT_EXP_TRIG_ONLY;
			else if (TDLS_IS_EXTERNAL_CONTROL_ENABLED(
				tdls_feature_flags))
				tdls_soc_obj->tdls_current_mode =
					TDLS_SUPPORT_EXT_CONTROL;
			else
				tdls_soc_obj->tdls_current_mode =
					TDLS_SUPPORT_IMP_MODE;
		}
	} else {
		tdls_soc_obj->tdls_current_mode =
				TDLS_SUPPORT_DISABLED;
	}

	tdls_info_to_fw = qdf_mem_malloc(sizeof(struct tdls_info));
	if (!tdls_info_to_fw)
		return;

	threshold_params = &tdls_vdev_obj->threshold_config;

	tdls_info_to_fw->notification_interval_ms =
		threshold_params->tx_period_t;
	tdls_info_to_fw->tx_discovery_threshold =
		threshold_params->tx_packet_n;
	tdls_info_to_fw->tx_teardown_threshold =
		threshold_params->idle_packet_n;
	tdls_info_to_fw->rssi_teardown_threshold =
		threshold_params->rssi_teardown_threshold;
	tdls_info_to_fw->rssi_delta = threshold_params->rssi_delta;
	tdls_info_to_fw->vdev_id = session_id;

	/* record the session id in vdev context */
	tdls_vdev_obj->session_id = session_id;
	tdls_info_to_fw->tdls_state = tdls_soc_obj->tdls_current_mode;
	tdls_info_to_fw->tdls_options = 0;

	/* Do not enable TDLS offchannel, if AP prohibited TDLS
	 * channel switch
	 */
	if (TDLS_IS_OFF_CHANNEL_ENABLED(tdls_feature_flags) &&
		(!tdls_chan_swit_prohibited))
		tdls_info_to_fw->tdls_options = ENA_TDLS_OFFCHAN;

	if (TDLS_IS_BUFFER_STA_ENABLED(tdls_feature_flags))
		tdls_info_to_fw->tdls_options |= ENA_TDLS_BUFFER_STA;
	if (TDLS_IS_SLEEP_STA_ENABLED(tdls_feature_flags))
		tdls_info_to_fw->tdls_options |=  ENA_TDLS_SLEEP_STA;


	tdls_info_to_fw->peer_traffic_ind_window =
		tdls_soc_obj->tdls_configs.tdls_uapsd_pti_window;
	tdls_info_to_fw->peer_traffic_response_timeout =
		tdls_soc_obj->tdls_configs.tdls_uapsd_ptr_timeout;
	tdls_info_to_fw->puapsd_mask =
		tdls_soc_obj->tdls_configs.tdls_uapsd_mask;
	tdls_info_to_fw->puapsd_inactivity_time =
		tdls_soc_obj->tdls_configs.tdls_uapsd_inactivity_time;
	tdls_info_to_fw->puapsd_rx_frame_threshold =
		tdls_soc_obj->tdls_configs.tdls_rx_pkt_threshold;
	tdls_info_to_fw->teardown_notification_ms =
		tdls_soc_obj->tdls_configs.tdls_idle_timeout;
	tdls_info_to_fw->tdls_peer_kickout_threshold =
		tdls_soc_obj->tdls_configs.tdls_peer_kickout_threshold;
	tdls_info_to_fw->tdls_discovery_wake_timeout =
		tdls_soc_obj->tdls_configs.tdls_discovery_wake_timeout;

	/**
	 * set_state_cnt should always decrement in case of sta disconnection.
	 * If it is not, then in case where STA disconnection happens due to
	 * SSR where tdls_update_fw_tdls_state() will return failure,
	 * tdls count has to be decremented irrepective of success or failure
	 */
	if (!sta_connect_event)
		tdls_soc_obj->set_state_info.set_state_cnt--;

	status = tdls_update_fw_tdls_state(tdls_soc_obj, tdls_info_to_fw);
	if (QDF_STATUS_SUCCESS != status)
		goto done;

	if (sta_connect_event) {
		tdls_soc_obj->set_state_info.set_state_cnt++;
		tdls_soc_obj->set_state_info.vdev_id = session_id;
	}

	tdls_debug("TDLS Set state cnt %d",
		tdls_soc_obj->set_state_info.set_state_cnt);
done:
	qdf_mem_free(tdls_info_to_fw);
	return;
}

static QDF_STATUS
tdls_process_sta_connect(struct tdls_sta_notify_params *notify)
{
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_soc_priv_obj *tdls_soc_obj;

	if (QDF_STATUS_SUCCESS != tdls_get_vdev_objects(notify->vdev,
							&tdls_vdev_obj,
							&tdls_soc_obj))
		return QDF_STATUS_E_INVAL;


	if (policy_mgr_get_connection_count(tdls_soc_obj->soc) > 1) {
		tdls_debug("Concurrent sessions exist, TDLS can't be enabled");
		return QDF_STATUS_SUCCESS;
	}

	/* Association event */
	if (!tdls_soc_obj->tdls_disable_in_progress) {
		tdls_send_update_to_fw(tdls_vdev_obj,
				   tdls_soc_obj,
				   notify->tdls_prohibited,
				   notify->tdls_chan_swit_prohibited,
				   true,
				   notify->session_id);
	}

	/* check and set the connection tracker */
	tdls_set_ct_mode(tdls_soc_obj->soc);
	if (tdls_soc_obj->enable_tdls_connection_tracker)
		tdls_implicit_enable(tdls_vdev_obj);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_notify_sta_connect(struct tdls_sta_notify_params *notify)
{
	QDF_STATUS status;

	if (!notify) {
		tdls_err("invalid param");
		return QDF_STATUS_E_INVAL;
	}

	if (!notify->vdev) {
		tdls_err("invalid param");
		qdf_mem_free(notify);
		return QDF_STATUS_E_INVAL;
	}

	status = tdls_process_sta_connect(notify);

	wlan_objmgr_vdev_release_ref(notify->vdev, WLAN_TDLS_NB_ID);
	qdf_mem_free(notify);

	return status;
}

static QDF_STATUS
tdls_process_sta_disconnect(struct tdls_sta_notify_params *notify)
{
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_vdev_priv_obj *curr_tdls_vdev;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	struct tdls_soc_priv_obj *curr_tdls_soc;
	struct wlan_objmgr_vdev *temp_vdev = NULL;
	uint8_t vdev_id;


	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (QDF_STATUS_SUCCESS != tdls_get_vdev_objects(notify->vdev,
							&tdls_vdev_obj,
							&tdls_soc_obj))
		return QDF_STATUS_E_INVAL;

	/* if the disconnect comes from user space, we have to delete all the
	 * tdls peers before sending the set state cmd.
	 */
	if (notify->user_disconnect)
		return tdls_delete_all_tdls_peers(notify->vdev, tdls_soc_obj);

	tdls_debug("Check and update TDLS state");

	curr_tdls_vdev = tdls_vdev_obj;
	curr_tdls_soc = tdls_soc_obj;

	/* Disassociation event */
	if (!tdls_soc_obj->tdls_disable_in_progress)
		tdls_send_update_to_fw(tdls_vdev_obj, tdls_soc_obj, false,
				       false, false, notify->session_id);

	/* If concurrency is not marked, then we have to
	 * check, whether TDLS could be enabled in the
	 * system after this disassoc event.
	 */
	if (!notify->lfr_roam && !tdls_soc_obj->tdls_disable_in_progress) {
		temp_vdev = tdls_get_vdev(tdls_soc_obj->soc, WLAN_TDLS_NB_ID);
		if (temp_vdev) {
			vdev_id = wlan_vdev_get_id(temp_vdev);
			status = tdls_get_vdev_objects(temp_vdev,
						       &tdls_vdev_obj,
						       &tdls_soc_obj);
			if (QDF_STATUS_SUCCESS == status) {
				tdls_send_update_to_fw(tdls_vdev_obj,
						       tdls_soc_obj,
						       false,
						       false,
						       true,
						       vdev_id);
				curr_tdls_vdev = tdls_vdev_obj;
				curr_tdls_soc = tdls_soc_obj;
			}
		}
	}

	/* Check and set the connection tracker and implicit timers */
	tdls_set_ct_mode(curr_tdls_soc->soc);
	if (curr_tdls_soc->enable_tdls_connection_tracker)
		tdls_implicit_enable(curr_tdls_vdev);
	else
		tdls_implicit_disable(curr_tdls_vdev);

	/* release the vdev ref , if temp vdev was acquired */
	if (temp_vdev)
		wlan_objmgr_vdev_release_ref(temp_vdev,
					     WLAN_TDLS_NB_ID);

	return status;
}

QDF_STATUS tdls_notify_sta_disconnect(struct tdls_sta_notify_params *notify)
{
	QDF_STATUS status;

	if (!notify) {
		tdls_err("invalid param");
		return QDF_STATUS_E_INVAL;
	}

	if (!notify->vdev) {
		tdls_err("invalid param");
		qdf_mem_free(notify);
		return QDF_STATUS_E_INVAL;
	}

	status = tdls_process_sta_disconnect(notify);

	wlan_objmgr_vdev_release_ref(notify->vdev, WLAN_TDLS_NB_ID);
	qdf_mem_free(notify);

	return status;
}

static void tdls_process_reset_adapter(struct wlan_objmgr_vdev *vdev)
{
	struct tdls_vdev_priv_obj *tdls_vdev;

	tdls_vdev = wlan_vdev_get_tdls_vdev_obj(vdev);
	if (!tdls_vdev)
		return;
	tdls_timers_stop(tdls_vdev);
}

void tdls_notify_reset_adapter(struct wlan_objmgr_vdev *vdev)
{
	if (!vdev) {
		QDF_ASSERT(0);
		return;
	}

	if (QDF_STATUS_SUCCESS != wlan_objmgr_vdev_try_get_ref(vdev,
						WLAN_TDLS_NB_ID))
		return;

	tdls_process_reset_adapter(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
}

QDF_STATUS tdls_peers_deleted_notification(struct wlan_objmgr_psoc *psoc,
					   uint8_t vdev_id)
{
	struct scheduler_msg msg = {0, };
	struct tdls_sta_notify_params *notify;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	notify = qdf_mem_malloc(sizeof(*notify));
	if (!notify)
		return QDF_STATUS_E_NULL_VALUE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    vdev_id,
						    WLAN_TDLS_NB_ID);

	if (!vdev) {
		tdls_err("vdev not exist for the vdev id %d",
			 vdev_id);
		qdf_mem_free(notify);
		return QDF_STATUS_E_INVAL;
	}

	notify->lfr_roam = true;
	notify->tdls_chan_swit_prohibited = false;
	notify->tdls_prohibited = false;
	notify->session_id = vdev_id;
	notify->vdev = vdev;
	notify->user_disconnect = false;

	msg.bodyptr = notify;
	msg.callback = tdls_process_cmd;
	msg.flush_callback = tdls_post_msg_flush_cb;
	msg.type = TDLS_NOTIFY_STA_DISCONNECTION;

	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
		qdf_mem_free(notify);
		tdls_alert("message post failed ");

		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS tdls_delete_all_peers_indication(struct wlan_objmgr_psoc *psoc,
					    uint8_t vdev_id)
{
	struct scheduler_msg msg = {0, };
	struct tdls_delete_all_peers_params *indication;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	indication = qdf_mem_malloc(sizeof(*indication));
	if (!indication)
		return QDF_STATUS_E_NULL_VALUE;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
						    vdev_id,
						    WLAN_TDLS_SB_ID);

	if (!vdev) {
		tdls_err("vdev not exist for the session id %d",
			 vdev_id);
		qdf_mem_free(indication);
		return QDF_STATUS_E_INVAL;
	}

	indication->vdev = vdev;

	msg.bodyptr = indication;
	msg.callback = tdls_process_cmd;
	msg.type = TDLS_DELETE_ALL_PEERS_INDICATION;
	msg.flush_callback = tdls_post_msg_flush_cb;

	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_SB_ID);
		qdf_mem_free(indication);
		tdls_alert("message post failed ");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * tdls_set_mode_in_vdev() - set TDLS mode
 * @tdls_vdev: tdls vdev object
 * @tdls_soc: tdls soc object
 * @tdls_mode: TDLS mode
 * @source: TDLS disable source enum values
 *
 * Return: Void
 */
static void tdls_set_mode_in_vdev(struct tdls_vdev_priv_obj *tdls_vdev,
				  struct tdls_soc_priv_obj *tdls_soc,
				  enum tdls_feature_mode tdls_mode,
				  enum tdls_disable_sources source)
{
	if (!tdls_vdev)
		return;
	tdls_debug("enter tdls mode is %d", tdls_mode);

	if (TDLS_SUPPORT_IMP_MODE == tdls_mode ||
	    TDLS_SUPPORT_EXT_CONTROL == tdls_mode) {
		clear_bit((unsigned long)source,
			  &tdls_soc->tdls_source_bitmap);
		/*
		 * Check if any TDLS source bit is set and if
		 * bitmap is not zero then we should not
		 * enable TDLS
		 */
		if (tdls_soc->tdls_source_bitmap) {
			tdls_notice("Don't enable TDLS, source bitmap: %lu",
				tdls_soc->tdls_source_bitmap);
			return;
		}
		tdls_implicit_enable(tdls_vdev);
		/* tdls implicit mode is enabled, so
		 * enable the connection tracker
		 */
		tdls_soc->enable_tdls_connection_tracker =
			true;
	} else if (TDLS_SUPPORT_DISABLED == tdls_mode) {
		set_bit((unsigned long)source,
			&tdls_soc->tdls_source_bitmap);
		tdls_implicit_disable(tdls_vdev);
		/* If tdls implicit mode is disabled, then
		 * stop the connection tracker.
		 */
		tdls_soc->enable_tdls_connection_tracker =
			false;
	} else if (TDLS_SUPPORT_EXP_TRIG_ONLY ==
		   tdls_mode) {
		clear_bit((unsigned long)source,
			  &tdls_soc->tdls_source_bitmap);
		tdls_implicit_disable(tdls_vdev);
		/* If tdls implicit mode is disabled, then
		 * stop the connection tracker.
		 */
		tdls_soc->enable_tdls_connection_tracker =
			false;

		/*
		 * Check if any TDLS source bit is set and if
		 * bitmap is not zero then we should not
		 * enable TDLS
		 */
		if (tdls_soc->tdls_source_bitmap)
			return;
	}
	tdls_debug("exit ");

}

/**
 * tdls_set_current_mode() - set TDLS mode
 * @tdls_soc: tdls soc object
 * @tdls_mode: TDLS mode
 * @update_last: indicate to record the last tdls mode
 * @source: TDLS disable source enum values
 *
 * Return: Void
 */
static void tdls_set_current_mode(struct tdls_soc_priv_obj *tdls_soc,
				   enum tdls_feature_mode tdls_mode,
				   bool update_last,
				   enum tdls_disable_sources source)
{
	struct wlan_objmgr_vdev *vdev;
	struct tdls_vdev_priv_obj *tdls_vdev;

	if (!tdls_soc)
		return;

	tdls_debug("mode %d", (int)tdls_mode);

	if (update_last)
		tdls_soc->tdls_last_mode = tdls_mode;

	if (tdls_soc->tdls_current_mode == tdls_mode) {
		tdls_debug("already in mode %d", tdls_mode);

		switch (tdls_mode) {
		/* TDLS is already enabled hence clear source mask, return */
		case TDLS_SUPPORT_IMP_MODE:
		case TDLS_SUPPORT_EXP_TRIG_ONLY:
		case TDLS_SUPPORT_EXT_CONTROL:
			clear_bit((unsigned long)source,
				  &tdls_soc->tdls_source_bitmap);
			tdls_debug("clear source mask:%d", source);
			return;
		/* TDLS is already disabled hence set source mask, return */
		case TDLS_SUPPORT_DISABLED:
			set_bit((unsigned long)source,
				&tdls_soc->tdls_source_bitmap);
			tdls_debug("set source mask:%d", source);
			return;
		default:
			return;
		}
	}

	/* get sta vdev */
	vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(tdls_soc->soc,
							QDF_STA_MODE,
							WLAN_TDLS_NB_ID);
	if (vdev) {
		tdls_debug("set mode in tdls vdev ");
		tdls_vdev = wlan_vdev_get_tdls_vdev_obj(vdev);
		if (tdls_vdev)
			tdls_set_mode_in_vdev(tdls_vdev, tdls_soc,
					      tdls_mode, source);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	}

	/* get p2p client vdev */
	vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(tdls_soc->soc,
							QDF_P2P_CLIENT_MODE,
							WLAN_TDLS_NB_ID);
	if (vdev) {
		tdls_debug("set mode in tdls vdev ");
		tdls_vdev = wlan_vdev_get_tdls_vdev_obj(vdev);
		if (tdls_vdev)
			tdls_set_mode_in_vdev(tdls_vdev, tdls_soc,
					      tdls_mode, source);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);
	}

	if (!update_last)
		tdls_soc->tdls_last_mode = tdls_soc->tdls_current_mode;

	tdls_soc->tdls_current_mode = tdls_mode;

}

QDF_STATUS tdls_set_operation_mode(struct tdls_set_mode_params *tdls_set_mode)
{
	struct tdls_soc_priv_obj *tdls_soc;
	struct tdls_vdev_priv_obj *tdls_vdev;
	QDF_STATUS status;

	if (!tdls_set_mode)
		return QDF_STATUS_E_INVAL;

	if (!tdls_set_mode->vdev) {
		qdf_mem_free(tdls_set_mode);
		return QDF_STATUS_E_INVAL;
	}

	status = tdls_get_vdev_objects(tdls_set_mode->vdev,
				       &tdls_vdev, &tdls_soc);

	if (QDF_IS_STATUS_ERROR(status))
		goto release_mode_ref;

	tdls_set_current_mode(tdls_soc,
			      tdls_set_mode->tdls_mode,
			      tdls_set_mode->update_last,
			      tdls_set_mode->source);

release_mode_ref:
	wlan_objmgr_vdev_release_ref(tdls_set_mode->vdev, WLAN_TDLS_NB_ID);
	qdf_mem_free(tdls_set_mode);
	return status;
}

/**
 * wlan_hdd_tdls_scan_done_callback() - callback for tdls scan done event
 * @pAdapter: HDD adapter
 *
 * Return: Void
 */
void tdls_scan_done_callback(struct tdls_soc_priv_obj *tdls_soc)
{
	if (!tdls_soc)
		return;

	/* if tdls was enabled before scan, re-enable tdls mode */
	if (TDLS_SUPPORT_IMP_MODE == tdls_soc->tdls_last_mode ||
	    TDLS_SUPPORT_EXT_CONTROL == tdls_soc->tdls_last_mode ||
	    TDLS_SUPPORT_EXP_TRIG_ONLY == tdls_soc->tdls_last_mode)
		tdls_set_current_mode(tdls_soc, tdls_soc->tdls_last_mode,
				      false, TDLS_SET_MODE_SOURCE_SCAN);
}

/**
 * tdls_post_scan_done_msg() - post scan done message to tdls cmd queue
 * @tdls_soc: tdls soc object
 *
 * Return: QDF_STATUS_SUCCESS or QDF_STATUS_E_NULL_VALUE
 */
static QDF_STATUS tdls_post_scan_done_msg(struct tdls_soc_priv_obj *tdls_soc)
{
	struct scheduler_msg msg = {0, };

	if (!tdls_soc) {
		tdls_err("tdls_soc: %pK ", tdls_soc);
		return QDF_STATUS_E_NULL_VALUE;
	}

	msg.bodyptr = tdls_soc;
	msg.callback = tdls_process_cmd;
	msg.flush_callback = tdls_post_msg_flush_cb;
	msg.type = TDLS_CMD_SCAN_DONE;

	return scheduler_post_message(QDF_MODULE_ID_TDLS,
				      QDF_MODULE_ID_TDLS,
				      QDF_MODULE_ID_OS_IF, &msg);
}

void tdls_scan_complete_event_handler(struct wlan_objmgr_vdev *vdev,
			struct scan_event *event,
			void *arg)
{
	enum QDF_OPMODE device_mode;
	struct tdls_soc_priv_obj *tdls_soc;

	if (!vdev || !event || !arg)
		return;

	if (SCAN_EVENT_TYPE_COMPLETED != event->type)
		return;

	device_mode = wlan_vdev_mlme_get_opmode(vdev);

	tdls_soc = (struct tdls_soc_priv_obj *) arg;
	tdls_post_scan_done_msg(tdls_soc);
}

/**
 * tdls_check_peer_buf_capable() - Check buffer sta capable of tdls peers
 * @tdls_vdev: TDLS vdev object
 *
 * Used in scheduler thread context, no lock needed.
 *
 * Return: false if there is connected peer and not support buffer sta.
 */
static bool tdls_check_peer_buf_capable(struct tdls_vdev_priv_obj *tdls_vdev)
{
	uint16_t i;
	struct tdls_peer *peer;
	qdf_list_t *head;
	qdf_list_node_t *p_node;
	QDF_STATUS status;

	if (!tdls_vdev) {
		tdls_err("invalid tdls vdev object");
		return false;
	}

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &tdls_vdev->peer_list[i];

		status = qdf_list_peek_front(head, &p_node);
		while (QDF_IS_STATUS_SUCCESS(status)) {
			peer = qdf_container_of(p_node, struct tdls_peer, node);

			if (peer &&
			    (TDLS_LINK_CONNECTED == peer->link_status) &&
			    (!peer->buf_sta_capable))
				return false;

			status = qdf_list_peek_next(head, p_node, &p_node);
		}
	}

	return true;
}

QDF_STATUS tdls_scan_callback(struct tdls_soc_priv_obj *tdls_soc)
{
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct wlan_objmgr_vdev *vdev;
	uint16_t tdls_peer_count;
	uint32_t feature;
	bool peer_buf_capable;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* if tdls is not enabled, then continue scan */
	if (TDLS_SUPPORT_DISABLED == tdls_soc->tdls_current_mode)
		return status;

	/* Get the vdev based on vdev operating mode*/
	vdev = tdls_get_vdev(tdls_soc->soc, WLAN_TDLS_NB_ID);
	if (!vdev)
		return status;

	tdls_vdev = wlan_vdev_get_tdls_vdev_obj(vdev);
	if (!tdls_vdev)
		goto  return_success;

	if (tdls_is_progress(tdls_vdev, NULL, 0)) {
		if (tdls_soc->scan_reject_count++ >= TDLS_SCAN_REJECT_MAX) {
			tdls_notice("Allow this scan req. as already max no of scan's are rejected");
			tdls_soc->scan_reject_count = 0;
			status = QDF_STATUS_SUCCESS;

		} else {
			tdls_warn("tdls in progress. scan rejected %d",
				  tdls_soc->scan_reject_count);
			status = QDF_STATUS_E_BUSY;
		}
	}

	tdls_peer_count = tdls_soc->connected_peer_count;
	if (!tdls_peer_count)
		goto disable_tdls;

	feature = tdls_soc->tdls_configs.tdls_feature_flags;
	if (TDLS_IS_SCAN_ENABLED(feature)) {
		tdls_debug("TDLS Scan enabled, keep tdls link and allow scan, connected tdls peers: %d",
			   tdls_peer_count);
		goto disable_tdls;
	}

	if (TDLS_IS_BUFFER_STA_ENABLED(feature) &&
	    (tdls_peer_count <= TDLS_MAX_CONNECTED_PEERS_TO_ALLOW_SCAN)) {
		peer_buf_capable = tdls_check_peer_buf_capable(tdls_vdev);
		if (peer_buf_capable) {
			tdls_debug("All peers (num %d) bufSTAs, we can be sleep sta, so allow scan, tdls mode changed to %d",
				   tdls_peer_count,
				   tdls_soc->tdls_current_mode);
			goto disable_tdls;
		}
	}

	tdls_disable_offchan_and_teardown_links(vdev);

disable_tdls:
	tdls_set_current_mode(tdls_soc, TDLS_SUPPORT_DISABLED,
			      false, TDLS_SET_MODE_SOURCE_SCAN);

return_success:
	wlan_objmgr_vdev_release_ref(vdev,
				     WLAN_TDLS_NB_ID);
	return status;
}

void tdls_scan_serialization_comp_info_cb(struct wlan_objmgr_vdev *vdev,
		union wlan_serialization_rules_info *comp_info,
		struct wlan_serialization_command *cmd)
{
	struct tdls_soc_priv_obj *tdls_soc;
	QDF_STATUS status;
	if (!comp_info)
		return;

	tdls_soc = tdls_soc_global;
	comp_info->scan_info.is_tdls_in_progress = false;
	status = tdls_scan_callback(tdls_soc);
	if (QDF_STATUS_E_BUSY == status)
		comp_info->scan_info.is_tdls_in_progress = true;
}


uint8_t tdls_get_opclass_from_bandwidth(struct tdls_soc_priv_obj *soc_obj,
					uint8_t channel, uint8_t bw_offset,
					uint8_t *reg_bw_offset)
{
	uint8_t opclass;

	if (bw_offset & (1 << BW_80_OFFSET_BIT)) {
		opclass = tdls_find_opclass(soc_obj->soc,
					    channel, BW80);
		*reg_bw_offset = BW80;
	} else if (bw_offset & (1 << BW_40_OFFSET_BIT)) {
		opclass = tdls_find_opclass(soc_obj->soc,
					    channel, BW40_LOW_PRIMARY);
		*reg_bw_offset = BW40_LOW_PRIMARY;
		if (!opclass) {
			opclass = tdls_find_opclass(soc_obj->soc,
						    channel, BW40_HIGH_PRIMARY);
			*reg_bw_offset = BW40_HIGH_PRIMARY;
		}
	} else if (bw_offset & (1 << BW_20_OFFSET_BIT)) {
		opclass = tdls_find_opclass(soc_obj->soc,
					    channel, BW20);
		*reg_bw_offset = BW20;
	} else {
		opclass = tdls_find_opclass(soc_obj->soc,
					    channel, BWALL);
		*reg_bw_offset = BWALL;
	}

	return opclass;
}
