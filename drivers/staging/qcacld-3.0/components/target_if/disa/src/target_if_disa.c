/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
 * DOC: Target interface file for disa component to
 * Implement api's which shall be used by disa component
 * in target if internally.
 */

#include "target_if.h"
#include "target_if_disa.h"
#include "wlan_disa_tgt_api.h"
#include "wlan_disa_public_struct.h"
#include <wmi_unified_api.h>

int
target_if_encrypt_decrypt_event_handler(ol_scn_t scn_handle, uint8_t *data,
					uint32_t data_len)
{
	struct disa_encrypt_decrypt_resp_params resp;
	struct wlan_objmgr_psoc *psoc;
	wmi_unified_t wmi_handle;

	if (!data) {
		target_if_err("Invalid pointer");
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(scn_handle);
	if (!psoc) {
		target_if_err("psoc ptr is NULL");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return -EINVAL;
	}

	if (wmi_extract_encrypt_decrypt_resp_params(wmi_handle, data, &resp) !=
						    QDF_STATUS_SUCCESS) {
		target_if_err("Extraction of encrypt decrypt resp params failed");
		return -EINVAL;
	}

	tgt_disa_encrypt_decrypt_resp(psoc, &resp);

	return 0;
}

QDF_STATUS
target_if_disa_register_ev_handlers(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_register_event(wmi_handle,
				wmi_vdev_encrypt_decrypt_data_rsp_event_id,
				target_if_encrypt_decrypt_event_handler);
	if (status) {
		target_if_err("Failed to register Scan match event cb");
		return QDF_STATUS_E_FAILURE;
	}

	return status;
}

QDF_STATUS
target_if_disa_unregister_ev_handlers(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_unregister_event(wmi_handle,
				wmi_vdev_encrypt_decrypt_data_rsp_event_id);
	if (status) {
		target_if_err("Failed to unregister Scan match event cb");
		return QDF_STATUS_E_FAILURE;
	}

	return status;
}

QDF_STATUS
target_if_disa_encrypt_decrypt_req(struct wlan_objmgr_psoc *psoc,
		struct disa_encrypt_decrypt_req_params *req)
{
	wmi_unified_t wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_encrypt_decrypt_send_cmd(wmi_handle, req);
}


void target_if_disa_register_tx_ops(struct wlan_disa_tx_ops *disa_tx_ops)
{
	if (!disa_tx_ops) {
		target_if_err("disa_tx_ops is null");
		return;
	}

	disa_tx_ops->disa_encrypt_decrypt_req =
		target_if_disa_encrypt_decrypt_req;
	disa_tx_ops->disa_register_ev_handlers =
		target_if_disa_register_ev_handlers;
	disa_tx_ops->disa_unregister_ev_handlers =
		target_if_disa_unregister_ev_handlers;
}
