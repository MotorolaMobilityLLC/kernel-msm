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
 * DOC: Defines RoC API & structures
 */

#ifndef _WLAN_P2P_ROC_H_
#define _WLAN_P2P_ROC_H_

#include <qdf_types.h>
#include <qdf_mc_timer.h>
#include <qdf_list.h>

#define P2P_EVENT_PROPAGATE_TIME 10
#define P2P_WAIT_CANCEL_ROC      1000
#define P2P_WAIT_CLEANUP_ROC     2000
#define P2P_MAX_ROC_DURATION     1500
#define P2P_MAX_ROC_DURATION_DBS_NDP_PRESENT      400
#define P2P_MAX_ROC_DURATION_NON_DBS_NDP_PRESENT  250
#define P2P_MAX_ROC_DURATION_DBS_NAN_PRESENT      450
#define P2P_MAX_ROC_DURATION_NON_DBS_NAN_PRESENT  300

#define P2P_ROC_DURATION_MULTI_GO_PRESENT   6
#define P2P_ROC_DURATION_MULTI_GO_ABSENT    10
#define P2P_ACTION_FRAME_DEFAULT_WAIT       200
#define P2P_ROC_DEFAULT_DURATION            200

struct wlan_objmgr_vdev;
struct scan_event;

/**
 * enum roc_type - user requested or off channel tx
 * @USER_REQUESTED:   Requested by supplicant
 * @OFF_CHANNEL_TX:   Issued internally for off channel tx
 */
enum roc_type {
	USER_REQUESTED,
	OFF_CHANNEL_TX,
};

/**
 * enum roc_state - P2P RoC state
 * @ROC_STATE_IDLE:           RoC not yet started or completed
 * @ROC_STATE_REQUESTED:      Sent scan command to scan manager
 * @ROC_STATE_STARTED:        Got started event from scan manager
 * @ROC_STATE_ON_CHAN:        Got foreign channel event from SCM
 * @ROC_STATE_CANCEL_IN_PROG: Requested abort scan to SCM
 * @ROC_STATE_INVALID:        We should not come to this state
 */
enum roc_state {
	ROC_STATE_IDLE = 0,
	ROC_STATE_REQUESTED,
	ROC_STATE_STARTED,
	ROC_STATE_ON_CHAN,
	ROC_STATE_CANCEL_IN_PROG,
	ROC_STATE_INVALID,
};

/**
 * struct p2p_roc_context - RoC request context
 * @node:        Node for next element in the list
 * @p2p_soc_obj: Pointer to SoC global p2p private object
 * @vdev_id:     Vdev id on which this request has come
 * @scan_id:     Scan id given by scan component for this roc req
 * @tx_ctx:      TX context if this ROC is for tx MGMT
 * @chan:        Chan for which this RoC has been requested
 * @phy_mode:    PHY mode
 * @duration:    Duration for the RoC
 * @roc_type:    RoC type  User requested or internal
 * @roc_timer:   RoC timer
 * @roc_state:   Roc state
 * @id:          identifier of roc
 */
struct p2p_roc_context {
	qdf_list_node_t node;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	uint32_t vdev_id;
	uint32_t scan_id;
	void *tx_ctx;
	uint8_t chan;
	uint8_t phy_mode;
	uint32_t duration;
	enum roc_type roc_type;
	qdf_mc_timer_t roc_timer;
	enum roc_state roc_state;
	int32_t id;
};

/**
 * struct cancel_roc_context - p2p cancel roc context
 * @p2p_soc_obj:      Pointer to SoC global p2p private object
 * @cookie:           Cookie which is given by supplicant
 */
struct cancel_roc_context {
	struct p2p_soc_priv_obj *p2p_soc_obj;
	uint64_t cookie;
};

/**
 * struct p2p_cleanup_param - p2p cleanup parameters
 * @p2p_soc_obj:      Pointer to SoC global p2p private object
 * @vdev_id:          vdev id
 */
struct p2p_cleanup_param {
	struct p2p_soc_priv_obj *p2p_soc_obj;
	uint32_t vdev_id;
};

/**
 * p2p_mgmt_rx_action_ops() - register or unregister rx action callback
 * @psoc: psoc object
 * @isregister: register if true, unregister if false
 *
 * This function registers or unregisters rx action frame callback to
 * mgmt txrx component.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_mgmt_rx_action_ops(struct wlan_objmgr_psoc *psoc,
	bool isregister);

/**
 * p2p_find_current_roc_ctx() - Find out roc context in progressing
 * @p2p_soc_obj: p2p psoc private object
 *
 * This function finds out roc context in progressing from p2p psoc
 * private object
 *
 * Return: Pointer to roc context - success
 *         NULL                   - failure
 */
struct p2p_roc_context *p2p_find_current_roc_ctx(
	struct p2p_soc_priv_obj *p2p_soc_obj);

/**
 * p2p_find_roc_by_tx_ctx() - Find out roc context by tx context
 * @p2p_soc_obj: p2p psoc private object
 * @cookie: cookie is the key to find out roc context
 *
 * This function finds out roc context by tx context from p2p psoc
 * private object
 *
 * Return: Pointer to roc context - success
 *         NULL                   - failure
 */
struct p2p_roc_context *p2p_find_roc_by_tx_ctx(
	struct p2p_soc_priv_obj *p2p_soc_obj, uint64_t cookie);

/**
 * p2p_find_roc_by_chan() - Find out roc context by channel
 * @p2p_soc_obj: p2p psoc private object
 * @chan: channel of the ROC
 *
 * This function finds out roc context by channel from p2p psoc
 * private object
 *
 * Return: Pointer to roc context - success
 *         NULL                   - failure
 */
struct p2p_roc_context *p2p_find_roc_by_chan(
	struct p2p_soc_priv_obj *p2p_soc_obj, uint8_t chan);

/**
 * p2p_restart_roc_timer() - Restarts roc timer
 * @roc_ctx: remain on channel context
 *
 * This function restarts roc timer with updated duration.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_restart_roc_timer(struct p2p_roc_context *roc_ctx);

/**
 * p2p_cleanup_roc_sync() - Cleanup roc context in queue
 * @p2p_soc_obj: p2p psoc private object
 * @vdev:        vdev object
 *
 * This function cleanup roc context in queue, include the roc
 * context in progressing until cancellation done. To avoid deadlock,
 * don't call from scheduler thread.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_cleanup_roc_sync(
	struct p2p_soc_priv_obj *p2p_soc_obj,
	struct wlan_objmgr_vdev *vdev);

/**
 * p2p_process_cleanup_roc_queue() - process the message to cleanup roc
 * @param: pointer to cleanup parameters
 *
 * This function process the message to cleanup roc context in queue,
 * include the roc context in progressing.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_process_cleanup_roc_queue(
	struct p2p_cleanup_param *param);

/**
 * p2p_process_roc_req() - Process roc request
 * @roc_ctx: roc request context
 *
 * This function handles roc request. It will call API from scan/mgmt
 * txrx component.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_process_roc_req(struct p2p_roc_context *roc_ctx);

/**
 * p2p_process_cancel_roc_req() - Process cancel roc request
 * @cancel_roc_ctx: cancel roc request context
 *
 * This function cancel roc request by cookie.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_process_cancel_roc_req(
	struct cancel_roc_context *cancel_roc_ctx);

/**
 * p2p_scan_event_cb() - Process scan event
 * @vdev: vdev associated to this scan event
 * @event: event information
 * @arg: registered arguments
 *
 * This function handles P2P scan event and deliver P2P event to HDD
 * layer by registered callback.
 *
 * Return: None
 */
void p2p_scan_event_cb(struct wlan_objmgr_vdev *vdev,
	struct scan_event *event, void *arg);

#endif /* _WLAN_P2P_ROC_H_ */
