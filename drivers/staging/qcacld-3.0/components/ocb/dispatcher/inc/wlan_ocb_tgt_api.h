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
 * DOC: OCB south bound interface declaration
 */
#ifndef _WLAN_OCB_TGT_API_H
#define _WLAN_OCB_TGT_API_H
#include <wlan_ocb_public_structs.h>

/**
 * tgt_ocb_register_ev_handler() - register south bound event handler
 * @pdev: pdev handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS tgt_ocb_register_ev_handler(struct wlan_objmgr_pdev *pdev);

/**
 * tgt_ocb_unregister_ev_handler() - unregister south bound event handler
 * @pdev: pdev handle
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS tgt_ocb_unregister_ev_handler(struct wlan_objmgr_pdev *pdev);

/**
 * tgt_ocb_register_rx_ops() - register OCB rx operations
 * @ocb_rxops: fps to rx operations
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
QDF_STATUS tgt_ocb_register_rx_ops(struct wlan_ocb_rx_ops *ocb_rxops);
#endif
