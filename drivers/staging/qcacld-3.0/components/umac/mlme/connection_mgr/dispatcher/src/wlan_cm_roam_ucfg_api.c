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

/*
 * DOC: wlan_cm_roam_ucfg_api.c
 *
 * Implementation for roaming ucfg public functionality.
 */

#include "wlan_mlme_ucfg_api.h"
#include "wlan_cm_roam_ucfg_api.h"
#include "../../core/src/wlan_cm_roam_offload.h"

bool ucfg_is_roaming_enabled(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	enum roam_offload_state cur_state;

	cur_state = mlme_get_roam_state(psoc, vdev_id);
	if (cur_state == WLAN_ROAM_RSO_ENABLED ||
	    cur_state == WLAN_ROAMING_IN_PROG ||
	    cur_state == WLAN_ROAM_SYNCH_IN_PROG)
		return true;

	return false;
}

QDF_STATUS
ucfg_user_space_enable_disable_rso(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id,
				   const bool is_fast_roam_enabled)
{
	bool supplicant_disabled_roaming;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;
	bool lfr_enabled;
	enum roam_offload_state state;
	uint32_t set_val = 0;
	enum roam_offload_state  cur_state;

	/*
	 * If the ini "FastRoamEnabled" is disabled, don't allow the
	 * userspace to enable roam offload
	 */
	ucfg_mlme_is_lfr_enabled(psoc, &lfr_enabled);
	if (!lfr_enabled) {
		mlme_debug("ROAM_CONFIG: Fast roam ini is disabled. is_fast_roam_enabled %d",
			   is_fast_roam_enabled);
		if (!is_fast_roam_enabled)
			return QDF_STATUS_SUCCESS;

		return  QDF_STATUS_E_FAILURE;
	}

	cur_state = mlme_get_roam_state(psoc, vdev_id);
	if (cur_state == WLAN_ROAM_INIT) {
		if (!is_fast_roam_enabled)
			set_val =
			WMI_VDEV_ROAM_11KV_CTRL_DISABLE_FW_TRIGGER_ROAMING;

		status = cm_roam_send_disable_config(psoc, vdev_id, set_val);
		if (!QDF_IS_STATUS_SUCCESS(status))
			mlme_err("ROAM: update fast roaming failed, status: %d",
				 status);
	}
	wlan_mlme_set_usr_disabled_roaming(psoc, !is_fast_roam_enabled);

	/*
	 * Supplicant_disabled_roaming flag is the global flag to control
	 * roam offload from supplicant. Driver cannot enable roaming if
	 * supplicant disabled roaming is set.
	 * is_fast_roam_enabled: true - enable RSO if not disabled by driver
	 *                       false - Disable RSO. Send RSO stop if false
	 *                       is set.
	 */
	supplicant_disabled_roaming =
		mlme_get_supplicant_disabled_roaming(psoc, vdev_id);
	if (!is_fast_roam_enabled && supplicant_disabled_roaming) {
		mlme_debug("ROAM_CONFIG: RSO already disabled by supplicant");
		return QDF_STATUS_E_ALREADY;
	}

	mlme_set_supplicant_disabled_roaming(psoc, vdev_id,
					     !is_fast_roam_enabled);

	state = (is_fast_roam_enabled) ?
		WLAN_ROAM_RSO_ENABLED : WLAN_ROAM_RSO_STOPPED;
	status = cm_roam_state_change(pdev, vdev_id, state,
				      REASON_SUPPLICANT_DISABLED_ROAMING);

	return status;
}

QDF_STATUS ucfg_cm_abort_roam_scan(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	bool roam_scan_offload_enabled;

	ucfg_mlme_is_roam_scan_offload_enabled(psoc,
					       &roam_scan_offload_enabled);
	if (!roam_scan_offload_enabled)
		return QDF_STATUS_SUCCESS;

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	status = cm_roam_send_rso_cmd(psoc, vdev_id,
				      ROAM_SCAN_OFFLOAD_ABORT_SCAN,
				      REASON_ROAM_ABORT_ROAM_SCAN);
	cm_roam_release_lock();

	return status;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
ucfg_cm_roam_send_rt_stats_config(struct wlan_objmgr_pdev *pdev,
				  uint8_t vdev_id, uint8_t param_value)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	return cm_roam_send_rt_stats_config(psoc, vdev_id, param_value);
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */
