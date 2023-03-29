/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: Public API implementation of ftm time_sync called by north bound iface.
 */

#include "ftm_time_sync_ucfg_api.h"
#include "ftm_time_sync_main.h"
#include <qdf_str.h>

QDF_STATUS ucfg_ftm_time_sync_init(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_register_psoc_create_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_psoc_create_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ftm_time_sync_err("Failed to register psoc create handler");
		return status;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_psoc_destroy_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ftm_time_sync_err("Failed to register psoc delete handler");
		goto fail_destroy_psoc;
	}

	status = wlan_objmgr_register_vdev_create_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_vdev_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ftm_time_sync_err("Failed to register vdev create handler");
		goto fail_create_vdev;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_vdev_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		ftm_time_sync_err("Failed to register vdev destroy handler");
		goto fail_destroy_vdev;
	}
	return status;

fail_destroy_vdev:
	wlan_objmgr_unregister_vdev_create_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_vdev_create_notification, NULL);

fail_create_vdev:
	wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_psoc_destroy_notification, NULL);

fail_destroy_psoc:
	wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_psoc_create_notification, NULL);

	return status;
}

void ucfg_ftm_time_sync_deinit(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_unregister_vdev_destroy_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_vdev_destroy_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		ftm_time_sync_err("Failed to unregister vdev delete handler");

	status = wlan_objmgr_unregister_vdev_create_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_vdev_create_notification, NULL);
	if (!QDF_IS_STATUS_ERROR(status))
		ftm_time_sync_err("Failed to unregister vdev create handler");

	status = wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_psoc_destroy_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		ftm_time_sync_err("Failed to unregister psoc destroy handler");

	status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_FTM_TIME_SYNC,
				ftm_time_sync_psoc_create_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		ftm_time_sync_err("Failed to unregister psoc create handler");
}

bool  ucfg_is_ftm_time_sync_enable(struct wlan_objmgr_psoc *psoc)
{
	return ftm_time_sync_is_enable(psoc);
}

void ucfg_ftm_time_sync_set_enable(struct wlan_objmgr_psoc *psoc, bool enable)
{
	return ftm_time_sync_set_enable(psoc, enable);
}

void ucfg_ftm_time_sync_update_sta_connect_state(
					struct wlan_objmgr_vdev *vdev,
					enum ftm_time_sync_sta_state sta_state,
					struct qdf_mac_addr bssid)
{
	struct wlan_objmgr_psoc *psoc;
	enum ftm_time_sync_role role;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		ftm_time_sync_err("Failed to get psoc");
		return;
	}

	role = ftm_time_sync_get_role(psoc);
	if (role == FTM_TIMESYNC_SLAVE_ROLE) {
		if (sta_state == FTM_TIME_SYNC_STA_CONNECTED)
			ftm_time_sync_send_trigger(vdev);
		else
			ftm_time_sync_stop(vdev);

		ftm_time_sync_update_bssid(vdev, bssid);
	}
}

void ucfg_ftm_time_sync_update_bss_state(struct wlan_objmgr_vdev *vdev,
					 enum ftm_time_sync_bss_state ap_state)
{
	struct wlan_objmgr_psoc *psoc;
	enum ftm_time_sync_role role;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		ftm_time_sync_err("Failed to get psoc");
		return;
	}

	if (!ftm_time_sync_is_enable(psoc))
		return;

	role = ftm_time_sync_get_role(psoc);
	if (role == FTM_TIMESYNC_MASTER_ROLE) {
		if (ap_state == FTM_TIME_SYNC_BSS_STARTED)
			ftm_time_sync_send_trigger(vdev);
		else
			ftm_time_sync_stop(vdev);
	}
}

ssize_t ucfg_ftm_time_sync_show(struct wlan_objmgr_vdev *vdev, char *buf)
{
	return ftm_time_sync_show(vdev, buf);
}
