/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_tdls_cmds_process.h
 *
 * TDLS north bound commands include file
 */

#ifndef _WLAN_TDLS_CMDS_PROCESS_H_
#define _WLAN_TDLS_CMDS_PROCESS_H_

#define TDLS_IS_SETUP_ACTION(action) \
	((TDLS_SETUP_REQUEST <= action) && \
	(TDLS_SETUP_CONFIRM >= action))

/**
 * tdls_process_add_peer() - add TDLS peer
 * @req: TDLS add peer request
 *
 * Return: QDF_STATUS_SUCCESS if success; other value if failed
 */
QDF_STATUS tdls_process_add_peer(struct tdls_add_peer_request *req);

/**
 * tdls_process_del_peer() - del TDLS peer
 * @req: TDLS del peer request
 *
 * Return: QDF_STATUS_SUCCESS if success; other value if failed
 */
QDF_STATUS tdls_process_del_peer(struct tdls_oper_request *req);

/**
 * tdls_process_enable_link() - enable TDLS link
 * @req: TDLS enable link request
 *
 * Return: QDF_STATUS_SUCCESS if success; other value if failed
 */
QDF_STATUS tdls_process_enable_link(struct tdls_oper_request *req);

/**
 * tdls_process_setup_peer() - process configure an externally
 *                                    controllable TDLS peer
 * @req: TDLS configure force peer request
 *
 * Return: QDF_STATUS_SUCCESS if success; other values if failed
 */
QDF_STATUS tdls_process_setup_peer(struct tdls_oper_request *req);

/**
 * tdls_process_remove_force_peer() - process remove an externally controllable
 *                                    TDLS peer
 * @req: TDLS operation request
 *
 * Return: QDF_STATUS_SUCCESS if success; other values if failed
 */
QDF_STATUS tdls_process_remove_force_peer(struct tdls_oper_request *req);

/**
 * tdls_process_update_peer() - update TDLS peer
 * @req: TDLS update peer request
 *
 * Return: QDF_STATUS_SUCCESS if success; other value if failed
 */
QDF_STATUS tdls_process_update_peer(struct tdls_update_peer_request *req);

/**
 * tdls_process_antenna_switch() - handle TDLS antenna switch
 * @req: TDLS antenna switch request
 *
 * Rely on callback to indicate the antenna switch state to caller.
 *
 * Return: QDF_STATUS_SUCCESS if success; other value if failed.
 */
QDF_STATUS tdls_process_antenna_switch(struct tdls_antenna_switch_request *req);

/**
 * tdls_antenna_switch_flush_callback() - flush TDLS antenna switch request
 * @msg: scheduler message contains tdls antenna switch event
 *
 * This function call is invoked when scheduler thread is going down
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_antenna_switch_flush_callback(struct scheduler_msg *msg);

/**
 * tdls_pe_del_peer() - send TDLS delete peer request to PE
 * @req: TDLS delete peer request
 *
 * Return: QDF status
 */
QDF_STATUS tdls_pe_del_peer(struct tdls_del_peer_request *req);

/**
 * tdls_process_add_peer_rsp() - handle response for add or update TDLS peer
 * @rsp: TDLS add peer response
 *
 * Return: QDF status
 */
QDF_STATUS tdls_process_add_peer_rsp(struct tdls_add_sta_rsp *rsp);

/**
 * tdls_reset_nss() - reset tdls nss parameters
 * @tdls_soc: TDLS soc object
 * @action_code: action code
 *
 * Return: None
 */
void tdls_reset_nss(struct tdls_soc_priv_obj *tdls_soc,
				  uint8_t action_code);

/**
 * tdls_release_serialization_command() - TDLS wrapper to
 * relases serialization command.
 * @vdev: Object manager vdev
 * @type: command to release.
 *
 * Return: None
 */

void
tdls_release_serialization_command(struct wlan_objmgr_vdev *vdev,
				   enum wlan_serialization_cmd_type type);

/**
 * tdls_set_cap() - set TDLS capability type
 * @tdls_vdev: tdls vdev object
 * @mac: peer mac address
 * @cap: TDLS capability type
 *
 * Return: 0 if successful or negative errno otherwise
 */
int tdls_set_cap(struct tdls_vdev_priv_obj *tdls_vdev, const uint8_t *mac,
			  enum tdls_peer_capab cap);

/**
 * tdls_process_send_mgmt_rsp() - handle response for send mgmt
 * @rsp: TDLS send mgmt response
 *
 * Return: QDF_STATUS_SUCCESS for success; other values if failed
 */
QDF_STATUS tdls_process_send_mgmt_rsp(struct tdls_send_mgmt_rsp *rsp);

/**
 * tdls_send_mgmt_tx_completion() - process tx completion
 * @tx_complete: TDLS mgmt completion info
 *
 * Return: QDF_STATUS_SUCCESS for success; other values if failed
 */
QDF_STATUS tdls_send_mgmt_tx_completion(
			struct tdls_mgmt_tx_completion_ind *tx_complete);

/**
 * tdls_process_add_peer_rsp() - handle response for delete TDLS peer
 * @rsp: TDLS delete peer response
 *
 * Return: QDF status
 */
QDF_STATUS tdls_process_del_peer_rsp(struct tdls_del_sta_rsp *rsp);

/**
 * tdls_process_should_discover() - handle tdls should_discover event
 * @vdev: vdev object
 * @evt: event info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_process_should_discover(struct wlan_objmgr_vdev *vdev,
					struct tdls_event_info *evt);

/**
 * tdls_process_should_teardown() - handle tdls should_teardown event
 * @vdev: vdev object
 * @evt: event info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_process_should_teardown(struct wlan_objmgr_vdev *vdev,
					struct tdls_event_info *evt);

/**
 * tdls_process_connection_tracker_notify() -handle tdls connect tracker notify
 * @vdev: vdev object
 * @evt: event info
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_process_connection_tracker_notify(struct wlan_objmgr_vdev *vdev,
						  struct tdls_event_info *evt);

/**
 * tdls_validate_mgmt_request() -validate mgmt request
 * @tdls_validate: action frame request
 *
 * Return: 0 for success or -EINVAL otherwise
 */
int tdls_validate_mgmt_request(struct tdls_action_frame_request *tdls_mgmt_req);

/**
 * tdls_set_responder() - Set/clear TDLS peer's responder role
 * @set_req: set responder request
 *
 * Return: 0 for success or -EINVAL otherwise
 */
int tdls_set_responder(struct tdls_set_responder_req *set_req);

/**
 * tdls_decrement_peer_count() - decrement connected TDLS peer counter
 * @soc_obj: TDLS soc object
 *
 * Used in scheduler thread context, no lock needed.
 *
 * Return: None.
 */
void tdls_decrement_peer_count(struct tdls_soc_priv_obj *soc_obj);

/**
 * wlan_tdls_offchan_parms_callback() - Callback to release ref count
 * @vdev: vdev object
 *
 * Return: none
 */
void wlan_tdls_offchan_parms_callback(struct wlan_objmgr_vdev *vdev);

/**
 * tdls_process_set_offchannel() - Handle set offchannel request for TDLS
 * @req: TDLS set offchannel request
 *
 * Return: int status
 */
int tdls_process_set_offchannel(struct tdls_set_offchannel *req);

/**
 * tdls_process_set_offchan_mode() - Handle set offchan mode request for TDLS
 * @req: TDLS set offchannel mode request
 *
 * Return: int status
 */
int tdls_process_set_offchan_mode(struct tdls_set_offchanmode *req);

/**
 * tdls_process_set_secoffchanneloffset() - Handle set sec offchannel
 * offset request for TDLS
 * @req: TDLS set secoffchannel offchannel request
 *
 * Return: int status
 */
int tdls_process_set_secoffchanneloffset(
		struct tdls_set_secoffchanneloffset *req);

#endif
