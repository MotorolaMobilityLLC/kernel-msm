/*
 * Copyright (c) 2017-2018, 2020 The Linux Foundation. All rights reserved.
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
 * DOC: target_if_pmo_gtk.c
 *
 * Target interface file for pmo component to
 * send gtk offload related cmd and process event.
 */

#include "target_if.h"
#include "target_if_pmo.h"
#include "wmi_unified_api.h"

QDF_STATUS target_if_pmo_send_gtk_offload_req(struct wlan_objmgr_vdev *vdev,
		struct pmo_gtk_req *gtk_req)
{
	uint8_t vdev_id;
	QDF_STATUS status;
	uint32_t gtk_offload_opcode;
	struct wlan_objmgr_psoc *psoc;
	wmi_unified_t wmi_handle;

	if (!vdev) {
		target_if_err("vdev ptr passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	vdev_id = wlan_vdev_get_id(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (gtk_req->flags == PMO_GTK_OFFLOAD_ENABLE)
		gtk_offload_opcode = GTK_OFFLOAD_ENABLE_OPCODE;
	else
		gtk_offload_opcode = GTK_OFFLOAD_DISABLE_OPCODE;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_send_gtk_offload_cmd(wmi_handle,
						  vdev_id,
						  gtk_req,
						  gtk_req->flags,
						  gtk_offload_opcode);
	if (status)
		target_if_err("Failed to send gtk offload cmd to fw");

	return status;
}

QDF_STATUS target_if_pmo_send_gtk_response_req(struct wlan_objmgr_vdev *vdev)
{
	uint8_t vdev_id;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t offload_req_opcode;
	wmi_unified_t wmi_handle;

	if (!vdev) {
		target_if_err("vdev ptr passed is NULL");
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	vdev_id = wlan_vdev_get_id(vdev);
	if (!psoc) {
		target_if_err("psoc handle is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/* Request for GTK offload status */
	offload_req_opcode = GTK_OFFLOAD_REQUEST_STATUS_OPCODE;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	/* send the wmi command */
	status = wmi_unified_process_gtk_offload_getinfo_cmd(wmi_handle,
			vdev_id, offload_req_opcode);

	return status;
}

int target_if_pmo_gtk_offload_status_event(void *scn_handle,
	uint8_t *event, uint32_t len)
{
	struct pmo_gtk_rsp_params *gtk_rsp_param;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS ret;
	wmi_unified_t wmi_handle;

	TARGET_IF_ENTER();
	psoc = target_if_get_psoc_from_scn_hdl(scn_handle);
	if (!psoc) {
		target_if_err("psoc ptr is NULL");
		ret = -EINVAL;
		goto out;
	}

	gtk_rsp_param = qdf_mem_malloc(sizeof(*gtk_rsp_param));
	if (!gtk_rsp_param) {
		ret = -ENOMEM;
		goto out;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		qdf_mem_free(gtk_rsp_param);
		ret = -EINVAL;
		goto out;
	}

	if (wmi_extract_gtk_rsp_event(wmi_handle, event, gtk_rsp_param, len) !=
				      QDF_STATUS_SUCCESS) {
		target_if_err("Extraction of gtk rsp event failed");
		qdf_mem_free(gtk_rsp_param);
		ret = -EINVAL;
		goto out;
	}

	ret = pmo_tgt_gtk_rsp_evt(psoc, (void *)gtk_rsp_param);
	if (ret != QDF_STATUS_SUCCESS) {
		target_if_err("Failed to rx_gtk_rsp_event");
		ret = -EINVAL;
	}
	qdf_mem_free(gtk_rsp_param);
out:
	TARGET_IF_EXIT();

	return ret;
}

