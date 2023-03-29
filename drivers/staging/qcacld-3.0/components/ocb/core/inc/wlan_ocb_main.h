/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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

/*
 * DOC: contains ocb init/deinit public api
 */

#ifndef _WLAN_OCB_MAIN_API_H_
#define _WLAN_OCB_MAIN_API_H_

#include <qdf_atomic.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_ocb_public_structs.h>

#define ocb_alert_rl(params...) QDF_TRACE_FATAL_RL(QDF_MODULE_ID_OCB, params)
#define ocb_err_rl(params...) QDF_TRACE_ERROR_RL(QDF_MODULE_ID_OCB, params)
#define ocb_warn_rl(params...) QDF_TRACE_WARN_RL(QDF_MODULE_ID_OCB, params)
#define ocb_info_rl(params...) QDF_TRACE_INFO_RL(QDF_MODULE_ID_OCB, params)
#define ocb_debug_rl(params...) QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_OCB, params)

#define ocb_alert(params...) \
	QDF_TRACE_FATAL(QDF_MODULE_ID_OCB, params)
#define ocb_err(params...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_OCB, params)
#define ocb_warn(params...) \
	QDF_TRACE_WARN(QDF_MODULE_ID_OCB, params)
#define ocb_notice(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_OCB, params)
#define ocb_info(params...) \
	QDF_TRACE_INFO(QDF_MODULE_ID_OCB, params)
#define ocb_debug(params...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_OCB, params)

#define ocb_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_OCB, params)
#define ocb_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_OCB, params)
#define ocb_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_OCB, params)
#define ocb_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_OCB, params)
#define ocb_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_OCB, params)

/**
 * enum ocb_southbound_event - OCB south bound event type
 * @OCB_CHANNEL_CONFIG_STATUS: set channel config response
 * @OCB_TSF_TIMER: get TSF timer response
 * @OCB_DCC_STATS_RESPONSE: get DCC stats response
 * @OCB_NDL_RESPONSE: NDL update response
 * @OCB_DCC_INDICATION: DCC stats indication
 */
enum ocb_southbound_event {
	OCB_CHANNEL_CONFIG_STATUS,
	OCB_TSF_TIMER,
	OCB_DCC_STATS_RESPONSE,
	OCB_NDL_RESPONSE,
	OCB_DCC_INDICATION,
};

/**
 * struct ocb_pdev_obj - ocb pdev object
 * @pdev: pdev handle
 * @ocb_mac: MAC address for different channels
 * @ocb_channel_count: channel count
 * @channel_config: current channel configurations
 * @dp_soc: psoc data path handle
 * @dp_pdev_id: pdev data path ID
 * @ocb_cbs: legacy callback functions
 * @ocb_txops: tx opertions for target interface
 * @ocb_rxops: rx opertions for target interface
 */
struct ocb_pdev_obj {
	struct wlan_objmgr_pdev *pdev;
	struct qdf_mac_addr ocb_mac[QDF_MAX_CONCURRENCY_PERSONA];
	uint32_t ocb_channel_count;
	struct ocb_config *channel_config;
	void *dp_soc;
	uint8_t dp_pdev_id;
	struct ocb_callbacks ocb_cbs;
	struct wlan_ocb_tx_ops ocb_txops;
	struct wlan_ocb_rx_ops ocb_rxops;
};

/**
 * struct ocb_rx_event - event from south bound
 * @psoc: psoc handle
 * @vdev: vdev handle
 * @evt_id: event ID
 * @channel_cfg_rsp: set channel config status
 * @tsf_timer: get TSF timer response
 * @ndl: NDL DCC response
 * @dcc_stats: DCC stats
 */
struct ocb_rx_event {
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	uint32_t evt_id;
	union event {
		struct ocb_set_config_response channel_cfg_rsp;
		struct ocb_get_tsf_timer_response tsf_timer;
		struct ocb_dcc_update_ndl_response ndl;
		struct ocb_dcc_get_stats_response dcc_stats;
	} rsp;
};

/**
 * wlan_get_pdev_ocb_obj() - private API to get ocb pdev object
 * @pdev: pdev object
 *
 * Return: ocb object
 */
static inline struct ocb_pdev_obj *
wlan_get_pdev_ocb_obj(struct wlan_objmgr_pdev *pdev)
{
	struct ocb_pdev_obj *pdev_obj;

	pdev_obj = (struct ocb_pdev_obj *)
		wlan_objmgr_pdev_get_comp_private_obj(pdev,
				WLAN_UMAC_COMP_OCB);

	return pdev_obj;
}

/**
 * wlan_ocb_get_callbacks() - get legacy layer callbacks
 * @pdev: pdev handle
 *
 * Return: legacy layer callbacks
 */
static inline struct ocb_callbacks *
wlan_ocb_get_callbacks(struct wlan_objmgr_pdev *pdev)
{
	struct ocb_pdev_obj *pdev_obj;

	pdev_obj = wlan_get_pdev_ocb_obj(pdev);

	if (pdev_obj)
		return &pdev_obj->ocb_cbs;
	else
		return NULL;
}

/**
 * wlan_pdev_get_ocb_tx_ops() - get OCB tx operations
 * @pdev: pdev handle
 *
 * Return: fps to OCB tx operations
 */
static inline struct wlan_ocb_tx_ops *
wlan_pdev_get_ocb_tx_ops(struct wlan_objmgr_pdev *pdev)
{
	struct ocb_pdev_obj *ocb_obj;

	ocb_obj = wlan_get_pdev_ocb_obj(pdev);

	return &ocb_obj->ocb_txops;
}

/**
 * wlan_ocb_release_rx_event() - Release OCB RX event
 * @event: OCB RX event
 *
 * Return: none
 */
static inline void wlan_ocb_release_rx_event(struct ocb_rx_event *event)
{

	if (!event) {
		ocb_err("event is NULL");
		return;
	}

	if (event->vdev)
		wlan_objmgr_vdev_release_ref(event->vdev, WLAN_OCB_SB_ID);
	if (event->psoc)
		wlan_objmgr_psoc_release_ref(event->psoc, WLAN_OCB_SB_ID);
	qdf_mem_free(event);
}

/**
 * ocb_pdev_obj_create_notification() - OCB pdev object creation notification
 * @pdev: pdev handle
 * @arg_list: arguments list
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ocb_pdev_obj_create_notification(struct wlan_objmgr_pdev *pdev,
					    void *arg_list);

/**
 * ocb_pdev_obj_destroy_notification() - OCB pdev object destroy notification
 * @pdev: pdev handle
 * @arg_list: arguments list
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ocb_pdev_obj_destroy_notification(struct wlan_objmgr_pdev *pdev,
					     void *arg_list);

/**
 * ocb_config_new() - Creates a new OCB configuration
 * @num_channels: the number of channels
 * @num_schedule: the schedule size
 * @ndl_chan_list_len: length in bytes of the NDL chan blob
 * @ndl_active_state_list_len: length in bytes of the active state blob
 *
 * Return: A pointer to the OCB configuration struct, NULL on failure.
 */
struct ocb_config *ocb_config_new(uint32_t num_channels,
				  uint32_t num_schedule,
				  uint32_t ndl_chan_list_len,
				  uint32_t ndl_active_state_list_len);

/**
 * ocb_copy_config() - Backup current config parameters
 * @src: current config parameters
 *
 * Return: A pointer to the OCB configuration struct, NULL on failure.
 */
struct ocb_config *ocb_copy_config(struct ocb_config *src);

/**
 * ocb_process_evt() - API to process event from south bound
 * @msg: south bound message
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ocb_process_evt(struct scheduler_msg *msg);

/**
 * ocb_vdev_start() - start OCB vdev
 * @ocb_obj: OCB object
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ocb_vdev_start(struct ocb_pdev_obj *ocb_obj);
#endif
