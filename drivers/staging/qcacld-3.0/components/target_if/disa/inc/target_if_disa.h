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
 * DOC: Declare various api/struct which shall be used
 * by disa component for wmi cmd (tx path) and
 * event (rx) handling.
 */

#ifndef _TARGET_IF_DISA_H_
#define _TARGET_IF_DISA_H_

#include <wlan_objmgr_psoc_obj.h>
#include <wmi_unified_param.h>
#include "wlan_disa_obj_mgmt_public_struct.h"

/**
 * target_if_disa_encrypt_decrypt_req() - Send encrypt/decrypt request to
 * target.
 * @psoc: objmgr psoc handle
 * @req: Encrypt/decrypt request params
 *
 * Return: QDF status
 */
QDF_STATUS target_if_disa_encrypt_decrypt_req(struct wlan_objmgr_psoc *psoc,
				struct disa_encrypt_decrypt_req_params *req);

/**
 * target_if_encrypt_decrypt_event_handler() - Collect encrypt/decrypt request
 * event from the target and pass on the data to tgt api of DISA.
 * @scn_handle: target handle
 * @data: event data
 * @data_len: data length
 *
 * Return: QDF status
 */
int target_if_encrypt_decrypt_event_handler(ol_scn_t scn_handle, uint8_t *data,
			uint32_t data_len);

/**
 * target_if_disa_register_tx_ops() - Register DISA component TX OPS
 * @tx_ops: DISA if transmit ops
 *
 * Return: None
 */
void target_if_disa_register_tx_ops(struct wlan_disa_tx_ops *tx_ops);

/**
 * target_if_disa_register_ev_handler() -  Register disa event handlers.
 * @psoc:objmgr psoc handle
 *
 * Return: QDF status
 */
QDF_STATUS
target_if_disa_register_ev_handlers(struct wlan_objmgr_psoc *psoc);

/**
 * target_if_disa_register_ev_handler() -  Unregister disa event handlers.
 * @psoc:objmgr psoc handle
 *
 * Return: QDF status
 */
QDF_STATUS
target_if_disa_unregister_ev_handlers(struct wlan_objmgr_psoc *psoc);
#endif

