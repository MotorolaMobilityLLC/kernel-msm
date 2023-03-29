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
 * DOC: contains interface manager public api
 */
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_if_mgr_public_struct.h"
#include "wlan_if_mgr_ap.h"
#include "wlan_if_mgr_roam.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_if_mgr_main.h"
#include "wlan_p2p_cfg_api.h"
#include "wlan_tdls_api.h"

QDF_STATUS if_mgr_ap_start_bss(struct wlan_objmgr_vdev *vdev,
			       struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	wlan_tdls_teardown_links_sync(psoc);

	if (policy_mgr_is_hw_mode_change_in_progress(psoc)) {
		if (!QDF_IS_STATUS_SUCCESS(
		    policy_mgr_wait_for_connection_update(psoc))) {
			ifmgr_err("qdf wait for event failed!!");
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (policy_mgr_is_sta_active_connection_exists(psoc))
		/* Disable Roaming on all vdev's before starting bss */
		if_mgr_disable_roaming(pdev, vdev, RSO_START_BSS);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
if_mgr_ap_start_bss_complete(struct wlan_objmgr_vdev *vdev,
			     struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Due to audio share glitch with P2P GO caused by
	 * roam scan on concurrent interface, disable
	 * roaming if "p2p_disable_roam" ini is enabled.
	 * Donot re-enable roaming again on other STA interface
	 * if p2p GO is active on any vdev.
	 */
	if (cfg_p2p_is_roam_config_disabled(psoc) &&
	    wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_GO_MODE) {
		ifmgr_debug("p2p go mode, keep roam disabled");
	} else {
		/* Enable Roaming after start bss in case of failure/success */
		if_mgr_enable_roaming(pdev, vdev, RSO_START_BSS);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS if_mgr_ap_stop_bss(struct wlan_objmgr_vdev *vdev,
			      struct if_mgr_event_data *event_data)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
if_mgr_ap_stop_bss_complete(struct wlan_objmgr_vdev *vdev,
			    struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Due to audio share glitch with P2P GO caused by
	 * roam scan on concurrent interface, disable
	 * roaming if "p2p_disable_roam" ini is enabled.
	 * Re-enable roaming on other STA interface if p2p GO
	 * is active on any vdev.
	 */
	if (cfg_p2p_is_roam_config_disabled(psoc) &&
	    wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_GO_MODE) {
		ifmgr_debug("p2p go disconnected enable roam");
		if_mgr_enable_roaming(pdev, vdev, RSO_START_BSS);
	}

	return QDF_STATUS_SUCCESS;
}

