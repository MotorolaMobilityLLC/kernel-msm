/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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
 * DOC: offload lmac interface APIs for P2P
 */

#ifndef _TARGET_IF_P2P_H_
#define _TARGET_IF_P2P_H_

#include <qdf_types.h>

struct wlan_objmgr_psoc;
struct p2p_ps_config;
struct p2p_lo_start;

#ifdef FEATURE_P2P_LISTEN_OFFLOAD

/**
 * target_if_p2p_register_lo_event_handler() - Register lo event handler
 * @psoc: soc object
 * @arg: additional argument
 *
 * Target interface API to register P2P listen offload event handler.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS target_if_p2p_register_lo_event_handler(
	struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * target_if_p2p_unregister_lo_event_handler() - Unregister lo event handler
 * @psoc: soc object
 * @arg: additional argument
 *
 * Target interface API to unregister P2P listen offload event handler.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS target_if_p2p_unregister_lo_event_handler(
	struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * target_if_p2p_lo_start() - Start listen offload
 * @psoc: soc object
 * @lo_start: lo start information
 *
 * Target interface API to start listen offload.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS target_if_p2p_lo_start(struct wlan_objmgr_psoc *psoc,
				  struct p2p_lo_start *lo_start);

/**
 * target_if_p2p_lo_stop() - Stop listen offload
 * @psoc: soc object
 * @vdev_id: vdev id
 *
 * Target interface API to stop listen offload.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS target_if_p2p_lo_stop(struct wlan_objmgr_psoc *psoc,
				 uint32_t vdev_id);
#endif

/**
 * target_if_p2p_register_tx_ops() - Register P2P component TX OPS
 * @tx_ops: lmac if transmit ops
 *
 * Return: None
 */
void target_if_p2p_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);

/**
 * target_if_p2p_register_noa_event_handler() - Register noa event handler
 * @psoc: soc object
 * @arg: additional argument
 *
 * Target interface API to register P2P noa event handler.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS target_if_p2p_register_noa_event_handler(
	struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * target_if_p2p_unregister_noa_event_handler() - Unregister noa event handler
 * @psoc: soc object
 * @arg: additional argument
 *
 * Target interface API to unregister P2P listen offload event handler.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS target_if_p2p_unregister_noa_event_handler(
	struct wlan_objmgr_psoc *psoc, void *arg);

/**
 * target_if_p2p_set_ps() - Set power save
 * @psoc: soc object
 * @arg: additional argument
 *
 * Target interface API to set power save.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS target_if_p2p_set_ps(struct wlan_objmgr_psoc *psoc,
				struct p2p_ps_config *ps_config);

/**
 * target_if_p2p_set_noa() - Disable / Enable NOA
 * @psoc: soc object
 * @vdev_id: vdev id
 * @disable_noa: TRUE - Disable NoA, FALSE - Enable NoA
 *
 * Target interface API to Disable / Enable P2P NOA.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS target_if_p2p_set_noa(struct wlan_objmgr_psoc *psoc,
	uint32_t vdev_id, bool disable_noa);

#endif /* _TARGET_IF_P2P_H_ */
