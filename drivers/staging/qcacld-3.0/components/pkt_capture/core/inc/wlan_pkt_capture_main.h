/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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
 * DOC: Declare private API which shall be used internally only
 * in pkt_capture component. This file shall include prototypes of
 * various notification handlers and logging functions.
 *
 * Note: This API should be never accessed out of pkt_capture component.
 */

#ifndef _WLAN_PKT_CAPTURE_MAIN_H_
#define _WLAN_PKT_CAPTURE_MAIN_H_

#include <qdf_types.h>
#include "wlan_pkt_capture_priv.h"
#include "wlan_pkt_capture_objmgr.h"
#include "wlan_objmgr_vdev_obj.h"
#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
#include "cdp_txrx_stats_struct.h"
#endif

#define pkt_capture_log(level, args...) \
	QDF_TRACE(QDF_MODULE_ID_PKT_CAPTURE, level, ## args)

#define pkt_capture_logfl(level, format, args...) \
	pkt_capture_log(level, FL(format), ## args)

#define pkt_capture_fatal(format, args...) \
		pkt_capture_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define pkt_capture_err(format, args...) \
		pkt_capture_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define pkt_capture_warn(format, args...) \
		pkt_capture_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define pkt_capture_info(format, args...) \
		pkt_capture_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define pkt_capture_debug(format, args...) \
		pkt_capture_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

#define PKT_CAPTURE_ENTER() pkt_capture_debug("enter")
#define PKT_CAPTURE_EXIT() pkt_capture_debug("exit")

/**
 * enum pkt_capture_tx_status - packet capture tx status
 * @pktcapture_tx_status_ok: successfully sent + acked
 * @pktcapture_tx_status_discard: discard - not sent
 * @pktcapture_tx_status_no_ack: no_ack - sent, but no ack
 *
 * This enum has tx status types for packet capture mode
 */
enum pkt_capture_tx_status {
	pkt_capture_tx_status_ok,
	pkt_capture_tx_status_discard,
	pkt_capture_tx_status_no_ack,
};

/**
 * pkt_capture_get_vdev() - Get pkt capture objmgr vdev.
 *
 * Return: pkt capture objmgr vdev
 */
struct wlan_objmgr_vdev *pkt_capture_get_vdev(void);

/**
 * pkt_capture_vdev_create_notification() - Handler for vdev create notify.
 * @vdev: vdev which is going to be created by objmgr
 * @arg: argument for notification handler.
 *
 * Allocate and attach vdev private object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pkt_capture_vdev_create_notification(struct wlan_objmgr_vdev *vdev, void *arg);

/**
 * pkt_capture_vdev_destroy_notification() - Handler for vdev destroy notify.
 * @vdev: vdev which is going to be destroyed by objmgr
 * @arg: argument for notification handler.
 *
 * Deallocate and detach vdev private object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pkt_capture_vdev_destroy_notification(struct wlan_objmgr_vdev *vdev, void *arg);

/**
 * pkt_capture_get_mode() - get packet capture mode
 * @psoc: pointer to psoc object
 *
 * Return: enum pkt_capture_mode
 */
enum pkt_capture_mode pkt_capture_get_mode(struct wlan_objmgr_psoc *psoc);

/**
 * pkt_capture_psoc_create_notification() - Handler for psoc create notify.
 * @psoc: psoc which is going to be created by objmgr
 * @arg: argument for notification handler.
 *
 * Allocate and attach psoc private object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pkt_capture_psoc_create_notification(struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * pkt_capture_psoc_destroy_notification() - Handler for psoc destroy notify.
 * @psoc: psoc which is going to be destroyed by objmgr
 * @arg: argument for notification handler.
 *
 * Deallocate and detach psoc private object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pkt_capture_psoc_destroy_notification(struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * pkt_capture_register_callbacks - Register packet capture callbacks
 * @vdev: pointer to wlan vdev object manager
 * @mon_cb: callback to call
 * @context: callback context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pkt_capture_register_callbacks(struct wlan_objmgr_vdev *vdev,
			       QDF_STATUS (*mon_cb)(void *, qdf_nbuf_t),
			       void *context);

/**
 * pkt_capture_deregister_callbacks - De-register packet capture callbacks
 * @vdev: pointer to wlan vdev object manager
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pkt_capture_deregister_callbacks(struct wlan_objmgr_vdev *vdev);

/**
 * pkt_capture_set_pktcap_mode - Set packet capture mode
 * @psoc: pointer to psoc object
 * @mode: mode to be set
 *
 * Return: None
 */
void pkt_capture_set_pktcap_mode(struct wlan_objmgr_psoc *psoc,
				 enum pkt_capture_mode mode);

/**
 * pkt_capture_get_pktcap_mode - Get packet capture mode
 * @psoc: pointer to psoc object
 *
 * Return: enum pkt_capture_mode
 */
enum pkt_capture_mode
pkt_capture_get_pktcap_mode(struct wlan_objmgr_psoc *psoc);

/**
 * pkt_capture_set_pktcap_config - Set packet capture config
 * @vdev: pointer to vdev object
 * @config: config to be set
 *
 * Return: None
 */
void pkt_capture_set_pktcap_config(struct wlan_objmgr_vdev *vdev,
				   enum pkt_capture_config config);

/**
 * pkt_capture_get_pktcap_config - Get packet capture config
 * @vdev: pointer to vdev object
 *
 * Return: config value
 */
enum pkt_capture_config
pkt_capture_get_pktcap_config(struct wlan_objmgr_vdev *vdev);

/**
 * pkt_capture_drop_nbuf_list() - drop an nbuf list
 * @buf_list: buffer list to be dropepd
 *
 * Return: number of buffers dropped
 */
uint32_t pkt_capture_drop_nbuf_list(qdf_nbuf_t buf_list);

/**
 * pkt_capture_record_channel() - Update Channel Information
 * for packet capture mode
 * @vdev: pointer to vdev
 *
 * Return: None
 */
void pkt_capture_record_channel(struct wlan_objmgr_vdev *vdev);

/**
 * pkt_capture_mon() - Wrapper function to invoke mon cb
 * @cb_ctx: packet capture callback context
 * @msdu: packet
 * @vdev: pointer to vdev
 * @ch_freq: channel frequency
 *
 * Return: None
 */
void pkt_capture_mon(struct pkt_capture_cb_context *cb_ctx, qdf_nbuf_t msdu,
		     struct wlan_objmgr_vdev *vdev, uint16_t ch_freq);

/**
 * pkt_capture_set_filter - Set packet capture frame filter
 * @frame_filter: pkt capture frame filter data
 * @vdev: pointer to vdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pkt_capture_set_filter(struct pkt_capture_frame_filter frame_filter,
				  struct wlan_objmgr_vdev *vdev);

/**
 * pkt_capture_is_tx_mgmt_enable - Check if tx mgmt frames enabled
 * @pdev: pointer to pdev
 *
 * Return: bool
 */
bool pkt_capture_is_tx_mgmt_enable(struct wlan_objmgr_pdev *pdev);

#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * pkt_capture_get_pktcap_mode_v2 - Get packet capture mode
 *
 * Return: enum pkt_capture_mode
 */
enum pkt_capture_mode
pkt_capture_get_pktcap_mode_v2(void);

/**
 * pkt_capture_callback() - callback function for dp wdi events
 * @soc: dp_soc handle
 * @event: wdi event
 * @log_data: nbuf data
 * @peer_id: peer id
 * @status: status
 *
 * Return: None
 */
void pkt_capture_callback(void *soc, enum WDI_EVENT event, void *log_data,
			  u_int16_t peer_id, uint32_t status);
#endif
#endif /* end of _WLAN_PKT_CAPTURE_MAIN_H_ */
