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
 * DOC: target_if_pmo_wow.c
 *
 * Target interface file for pmo component to
 * send wow related cmd and process event.
 */


#include "target_if.h"
#include "target_if_pmo.h"
#include "wmi_unified_api.h"

QDF_STATUS target_if_pmo_enable_wow_wakeup_event(struct wlan_objmgr_vdev *vdev,
		uint32_t *bitmap)
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

	status = wmi_unified_add_wow_wakeup_event_cmd(wmi_handle, vdev_id,
						      bitmap, true);
	if (status)
		target_if_err("Failed to config wow wakeup event");

	return status;
}

QDF_STATUS target_if_pmo_disable_wow_wakeup_event(struct wlan_objmgr_vdev *vdev,
		uint32_t *bitmap)
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

	status = wmi_unified_add_wow_wakeup_event_cmd(wmi_handle, vdev_id,
						      bitmap, false);
	if (status)
		target_if_err("Failed to config wow wakeup event");

	return status;
}

QDF_STATUS target_if_pmo_send_wow_patterns_to_fw(struct wlan_objmgr_vdev *vdev,
		uint8_t ptrn_id,
		const uint8_t *ptrn, uint8_t ptrn_len,
		uint8_t ptrn_offset, const uint8_t *mask,
		uint8_t mask_len, bool user)
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

	status = wmi_unified_wow_patterns_to_fw_cmd(wmi_handle,
						    vdev_id, ptrn_id, ptrn,
						    ptrn_len, ptrn_offset,
						    mask, mask_len, user, 0);

	return status;
}

QDF_STATUS target_if_pmo_del_wow_patterns_to_fw(struct wlan_objmgr_vdev *vdev,
		uint8_t ptrn_id)
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

	status = wmi_unified_wow_delete_pattern_cmd(wmi_handle, ptrn_id,
						    vdev_id);

	return status;
}

