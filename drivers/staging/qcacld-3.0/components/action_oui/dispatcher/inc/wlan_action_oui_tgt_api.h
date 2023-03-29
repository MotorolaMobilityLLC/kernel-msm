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
 * DOC: Declare public API for action_oui to interact with target/WMI
 */

#ifndef _WLAN_ACTION_OUI_TGT_API_H_
#define _WLAN_ACTION_OUI_TGT_API_H_

#include "wlan_action_oui_public_struct.h"
#include "wlan_action_oui_objmgr.h"

#define GET_ACTION_OUI_TX_OPS_FROM_PSOC(psoc) \
	(&action_oui_psoc_get_priv(psoc)->tx_ops)

/**
 * tgt_action_oui_send() - Send request to target if
 * @psoc: objmgr psoc object
 * @req: action_oui request to be send
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_action_oui_send(struct wlan_objmgr_psoc *psoc,
				struct action_oui_request *req);

/**
 * struct action_oui_tx_ops - structure of tx operations
 * @send_req: Pointer to hold target_if send function
 */
struct action_oui_tx_ops {
	QDF_STATUS (*send_req)(struct wlan_objmgr_psoc *psoc,
			       struct action_oui_request *req);
};

#endif /* _WLAN_ACTION_OUI_TGT_API_H_ */
