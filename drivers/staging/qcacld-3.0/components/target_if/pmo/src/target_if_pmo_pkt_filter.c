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
 * DOC: target_if_pmo_pkt_filter.c
 *
 * Target interface file for pmo component to
 * send packet filter related cmd and process event.
 */

#include "target_if.h"
#include "target_if_pmo.h"
#include "wmi_unified_pmo_api.h"
#include "wlan_pmo_pkt_filter_public_struct.h"

QDF_STATUS target_if_pmo_send_pkt_filter_req(struct wlan_objmgr_vdev *vdev,
			struct pmo_rcv_pkt_fltr_cfg *rcv_filter_param)
{
	uint8_t vdev_id;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;
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

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	/* send the command along with data */
	status = wmi_unified_config_packet_filter_cmd(wmi_handle, vdev_id,
			rcv_filter_param,
			rcv_filter_param->filter_id, true);
	if (status) {
		target_if_err("Failed to send pkt_filter cmd");
		return QDF_STATUS_E_INVAL;
	}

	/* Enable packet filter */
	status = wmi_unified_enable_disable_packet_filter_cmd(wmi_handle,
							      vdev_id, true);
	if (status)
		target_if_err("Failed to send packet filter wmi cmd to fw");

	return status;
}

QDF_STATUS target_if_pmo_clear_pkt_filter_req(struct wlan_objmgr_vdev *vdev,
			struct pmo_rcv_pkt_fltr_clear_param *rcv_clear_param)
{
	uint8_t vdev_id;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;
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

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	/* send the command along with data */
	status = wmi_unified_config_packet_filter_cmd(wmi_handle, vdev_id, NULL,
					rcv_clear_param->filter_id, false);

	if (status)
		target_if_err("Failed to clear filter cmd");

	return status;
}
