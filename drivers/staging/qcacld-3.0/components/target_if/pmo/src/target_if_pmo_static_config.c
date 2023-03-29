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
 * DOC: target_if_pmo_static.c
 *
 * Target interface file for pmo component to
 * send wow related cmd and process event.
 */


#include "target_if.h"
#include "target_if_pmo.h"
#include "wmi_unified_api.h"

QDF_STATUS target_if_pmo_send_ra_filter_req(struct wlan_objmgr_vdev *vdev,
		uint8_t default_pattern, uint16_t rate_limit_interval)
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

	status = wmi_unified_wow_sta_ra_filter_cmd(wmi_handle,
						   vdev_id,
						   default_pattern,
						   rate_limit_interval);
	if (status)
		target_if_err("Failed to send RA rate limit to fw");

	return status;
}

QDF_STATUS target_if_pmo_send_action_frame_patterns(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_action_wakeup_set_params *ip_cmd)
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

	status = wmi_unified_action_frame_patterns_cmd(wmi_handle, ip_cmd);
	if (status != QDF_STATUS_SUCCESS)
		target_if_err("Failed to config wow action frame map, ret %d",
			status);

	return status;
}

QDF_STATUS target_if_pmo_send_enhance_mc_offload_req(
		struct wlan_objmgr_vdev *vdev, bool enable)
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

	status = wmi_unified_enable_enhance_multicast_offload_cmd(wmi_handle,
								  vdev_id,
								  enable);
	if (status)
		target_if_err("Failed to config wow wakeup event");

	return status;
}

