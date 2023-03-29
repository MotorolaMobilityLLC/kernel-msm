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
 * DOC: Implements public API for action_oui to interact with target/WMI
 */

#include "wlan_action_oui_tgt_api.h"
#include "wlan_action_oui_main.h"
#include "wlan_action_oui_public_struct.h"

QDF_STATUS tgt_action_oui_send(struct wlan_objmgr_psoc *psoc,
			       struct action_oui_request *req)
{
	struct action_oui_tx_ops *tx_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	ACTION_OUI_ENTER();

	tx_ops = GET_ACTION_OUI_TX_OPS_FROM_PSOC(psoc);
	QDF_ASSERT(tx_ops->send_req);
	if (tx_ops->send_req)
		status = tx_ops->send_req(psoc, req);

	ACTION_OUI_EXIT();

	return status;
}
