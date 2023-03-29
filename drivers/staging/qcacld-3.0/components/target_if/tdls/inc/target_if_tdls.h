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
 * DOC: offload lmac interface APIs for tdls
 *
 */

#ifndef __TARGET_IF_TDLS_H__
#define __TARGET_IF_TDLS_H__

struct tdls_info;
struct wlan_objmgr_psoc;
struct tdls_peer_update_state;
struct tdls_channel_switch_params;
struct sta_uapsd_trig_params;

/**
 * target_if_tdls_update_fw_state() - lmac handler to update tdls fw state
 * @psoc: psoc object
 * @param: tdls state parameter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_tdls_update_fw_state(struct wlan_objmgr_psoc *psoc,
			       struct tdls_info *param);

/**
 * target_if_tdls_update_peer_state() - lmac handler to update tdls peer state
 * @psoc: psoc object
 * @peer_params: tdls peer state params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_tdls_update_peer_state(struct wlan_objmgr_psoc *psoc,
				 struct tdls_peer_update_state *peer_params);

/**
 * target_if_tdls_set_offchan_mode() - lmac handler to set tdls off channel mode
 * @psoc: psoc object
 * @params: tdls channel swithc params
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_tdls_set_offchan_mode(struct wlan_objmgr_psoc *psoc,
				struct tdls_channel_switch_params *params);

/**
 * target_if_tdls_register_event_handler() - lmac handler to register tdls event
 * handler
 * @psoc : psoc object
 * @arg: argument passed to lmac
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_tdls_register_event_handler(struct wlan_objmgr_psoc *psoc,
				      void *arg);

/**
 * target_if_tdls_unregister_event_handler() - lmac handler to unregister tdls
 * event handler
 * @psoc : psoc object
 * @arg: argument passed to lmac
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_tdls_unregister_event_handler(struct wlan_objmgr_psoc *psoc,
					void *arg);

/**
 * target_if_tdls_register_tx_ops() - lmac handler to register tdls tx ops
 * callback functions
 * @tx_ops: wlan_lmac_if_tx_ops object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_tdls_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);
#endif
