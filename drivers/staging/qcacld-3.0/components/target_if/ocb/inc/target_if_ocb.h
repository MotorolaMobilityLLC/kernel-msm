/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
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
 * DOC: offload lmac interface APIs for ocb
 *
 */

#ifndef __TARGET_IF_OCB_H__
#define __TARGET_IF_OCB_H__

/**
 * target_if_ocb_register_event_handler() - lmac handler to register ocb event
 * handler
 * @psoc : psoc object
 * @arg: argument passed to lmac
 *
 * Return: QDF_STATUS
 */
QDF_STATUS target_if_ocb_register_event_handler(struct wlan_objmgr_psoc *psoc,
						void *arg);

/**
 * target_if_ocb_unregister_event_handler() - lmac handler to unregister ocb
 * event handler
 * @psoc : psoc object
 * @arg: argument passed to lmac
 *
 * Return: QDF_STATUS
 */
QDF_STATUS target_if_ocb_unregister_event_handler(struct wlan_objmgr_psoc *psoc,
						  void *arg);
/**
 * target_if_ocb_register_tx_ops() - lmac handler to register ocb tx ops
 * callback functions
 * @tx_ops: ocb tx operations
 *
 * Return: QDF_STATUS
 */
QDF_STATUS target_if_ocb_register_tx_ops(
		struct wlan_ocb_tx_ops *tx_ops);

#endif
