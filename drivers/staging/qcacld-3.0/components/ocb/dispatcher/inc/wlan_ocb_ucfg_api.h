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

/**
 * DOC: Contains OCB north bound interface definitions
 */

#ifndef _WLAN_OCB_UCFG_API_H_
#define _WLAN_OCB_UCFG_API_H_
#include <qdf_types.h>
#include "wlan_ocb_public_structs.h"

#ifdef WLAN_FEATURE_DSRC
/**
 * ucfg_ocb_set_channel_config() - send OCB config request
 * @vdev: vdev handle
 * @config: config parameters
 * @set_config_cb: callback for set channel config
 * @arg: arguments for the callback
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_set_channel_config(struct wlan_objmgr_vdev *vdev,
				       struct ocb_config *config,
				       ocb_sync_callback set_config_cb,
				       void *arg);

/**
 * ucfg_ocb_set_utc_time() - UCFG API to set UTC time
 * @vdev: vdev handle
 * @utc: UTC time
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_set_utc_time(struct wlan_objmgr_vdev *vdev,
				 struct ocb_utc_param *utc);

/**
 * ucfg_ocb_start_timing_advert() - ucfg API to start TA
 * @vdev: vdev handle
 * @ta: timing advertisement parameters
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_start_timing_advert(struct wlan_objmgr_vdev *vdev,
					struct ocb_timing_advert_param *ta);

/**
 * ucfg_ocb_stop_timing_advert() - ucfg API to stop TA
 * @vdev: vdev handle
 * @ta: timing advertisement parameters
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_stop_timing_advert(struct wlan_objmgr_vdev *vdev,
				       struct ocb_timing_advert_param *ta);

/**
 * ucfg_ocb_get_tsf_timer() - ucfg API to get tsf timer
 * @vdev: vdev handle
 * @request: request for TSF timer
 * @get_tsf_cb: callback for TSF timer response
 * @arg: argument for the ccallback
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_get_tsf_timer(struct wlan_objmgr_vdev *vdev,
				  struct ocb_get_tsf_timer_param *request,
				  ocb_sync_callback get_tsf_cb,
				  void *arg);

/**
 * ucfg_ocb_dcc_get_stats() - get DCC stats
 * @vdev: vdev handle
 * @request: request for dcc stats
 * @dcc_get_stats_cb: callback for get dcc stats response
 * @arg: argument for the ccallback
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_dcc_get_stats(struct wlan_objmgr_vdev *vdev,
				  struct ocb_dcc_get_stats_param *request,
				  ocb_sync_callback dcc_get_stats_cb,
				  void *arg);

/**
 * ucfg_ocb_dcc_clear_stats() - Clear DCC stats
 * @vdev: vdev handle
 * @vdev_id: vdev ID
 * @bitmap: bitmap for stats to be cleared
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_dcc_clear_stats(struct wlan_objmgr_vdev *vdev,
				    uint16_t vdev_id,
				    uint32_t bitmap);

/**
 * ucfg_ocb_dcc_update_ndl() - ucfg API to update NDL
 * @vdev: vdev handle
 * @request: request parameters
 * @dcc_update_ndl_cb: callback for update resposne
 * @arg: argument for the callback
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_dcc_update_ndl(struct wlan_objmgr_vdev *vdev,
				   struct ocb_dcc_update_ndl_param *request,
				   ocb_sync_callback dcc_update_ndl_cb,
				   void *arg);

/**
 * ucfg_ocb_register_for_dcc_stats_event() - register dcc stats
 * events callback
 * @pdev: pdev handle
 * @context: argument for the callback
 * @dcc_stats_cb: callback for dcc stats event
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_register_for_dcc_stats_event(struct wlan_objmgr_pdev *pdev,
				void *ctx, ocb_sync_callback dcc_stats_cb);
/**
 * ucfg_ocb_init() - OCB module initialization
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_init(void);

/**
 * ucfg_ocb_deinit() - OCB module deinitialization
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_deinit(void);

/**
 * ucfg_ocb_config_channel() - Set channel config using after OCB started
 * @pdev: pdev handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_config_channel(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_ocb_register_vdev_start() - register callback to start ocb vdev
 * @pdev: pdev handle
 * @ocb_start: legacy callback to start ocb vdev
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_register_vdev_start(struct wlan_objmgr_pdev *pdev,
				QDF_STATUS (*ocb_start)(struct ocb_config *));

/**
 * ocb_psoc_enable() - Trigger psoc enable for ocb
 * @psoc: objmgr psoc object
 *
 * Return: QDF status success or failure
 */
QDF_STATUS ocb_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ocb_psoc_disable() - Trigger psoc disable for ocb
 * @psoc: objmgr psoc object
 *
 * Return: QDF status success or failure
 */
QDF_STATUS ocb_psoc_disable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_ocb_update_dp_handle() - register DP handle
 * @soc: soc handle
 * @dp_soc: data path soc handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_update_dp_handle(struct wlan_objmgr_psoc *soc,
				     void *dp_soc);

/**
 * ucfg_ocb_set_txrx_pdev_id() - register txrx pdev id
 * @soc: soc handle
 * @pdev_id: data path pdev ID
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS ucfg_ocb_set_txrx_pdev_id(struct wlan_objmgr_psoc *psoc,
				     uint8_t pdev_id);
#else
/**
 * ucfg_ocb_init() - OCB module initialization
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static inline QDF_STATUS ucfg_ocb_init(void)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * ucfg_ocb_deinit() - OCB module deinitialization
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static inline QDF_STATUS ucfg_ocb_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * ucfg_ocb_config_channel() - Set channel config using after OCB started
 * @pdev: pdev handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static inline QDF_STATUS
ucfg_ocb_config_channel(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * ocb_psoc_enable() - Trigger psoc enable for ocb
 * @psoc: objmgr psoc object
 *
 * Return: QDF status success or failure
 */
static inline
QDF_STATUS ocb_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * ocb_psoc_disable() - Trigger psoc disable for ocb
 * @psoc: objmgr psoc object
 *
 * Return: QDF status success or failure
 */
static inline
QDF_STATUS ocb_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * ucfg_ocb_update_dp_handle() - register DP handle
 * @soc: soc handle
 * @dp_soc: data path soc handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static inline
QDF_STATUS ucfg_ocb_update_dp_handle(struct wlan_objmgr_psoc *soc,
				     void *dp_soc)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * ucfg_ocb_set_txrx_pdev_id() - register txrx pdev id
 * @soc: soc handle
 * @pdev_id: data path pdev ID
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static inline
QDF_STATUS ucfg_ocb_set_txrx_pdev_id(struct wlan_objmgr_psoc *psoc,
				     uint8_t pdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif
#endif
