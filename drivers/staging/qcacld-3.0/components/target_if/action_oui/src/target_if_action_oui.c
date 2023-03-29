/*
 * Copyright (c) 2013-2018 The Linux Foundation. All rights reserved.
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
 * DOC: Target interface file for action_oui component to
 * Implement api's which shall be used by action_oui component
 * in target_if internally.
 */

#include "target_if_action_oui.h"
#include "wlan_action_oui_tgt_api.h"
#include "wlan_action_oui_public_struct.h"

static QDF_STATUS
target_if_action_oui_send_req(struct wlan_objmgr_psoc *psoc,
			      struct action_oui_request *req)
{
	void *wmi_hdl;

	wmi_hdl = GET_WMI_HDL_FROM_PSOC(psoc);
	if (!wmi_hdl)
		return QDF_STATUS_E_FAILURE;

	return wmi_unified_send_action_oui_cmd(wmi_hdl, req);
}

void
target_if_action_oui_register_tx_ops(struct action_oui_tx_ops *tx_ops)
{
	if (!tx_ops) {
		target_if_err("action_oui tx_ops is null");
		return;
	}

	tx_ops->send_req = target_if_action_oui_send_req;
}
