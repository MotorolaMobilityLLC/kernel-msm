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
#include "wlan_if_mgr_sta.h"
#include "wlan_if_mgr_roam.h"
#include "wlan_if_mgr_main.h"
#include "nan_ucfg_api.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_p2p_ucfg_api.h"
#include "wlan_tdls_ucfg_api.h"
#include "wlan_tdls_api.h"

QDF_STATUS if_mgr_connect_start(struct wlan_objmgr_vdev *vdev,
				struct if_mgr_event_data *event_data)
{
	uint8_t sta_cnt, sap_cnt;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	enum QDF_OPMODE op_mode;
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS], i;
	bool disable_nan = true;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Disable NAN Discovery if incoming connection is P2P or if a STA
	 * connection already exists and if this is a case of STA+STA
	 * or SAP+STA concurrency
	 */
	sta_cnt = policy_mgr_get_mode_specific_conn_info(psoc, NULL,
							 vdev_id_list,
							 PM_STA_MODE);
	sap_cnt = policy_mgr_get_mode_specific_conn_info(psoc, NULL,
							 &vdev_id_list[sta_cnt],
							 PM_SAP_MODE);
	op_mode = wlan_vdev_mlme_get_opmode(vdev);
	if (op_mode == QDF_P2P_CLIENT_MODE || sap_cnt || sta_cnt) {
		for (i = 0; i < sta_cnt + sap_cnt; i++)
			if (vdev_id_list[i] == wlan_vdev_get_id(vdev))
				disable_nan = false;
		if (disable_nan)
			ucfg_nan_disable_concurrency(psoc);
	}

	/*
	 * STA+NDI concurrency gets preference over NDI+NDI. Disable
	 * first NDI in case an NDI+NDI concurrency exists if FW does
	 * not support 4 port concurrency of two NDI + NAN with STA.
	 */
	if (!ucfg_nan_is_sta_nan_ndi_4_port_allowed(psoc))
		ucfg_nan_check_and_disable_unsupported_ndi(psoc,
							   false);

	/*
	 * In case of STA+STA concurrency, firmware might try to roam
	 * to same AP where host is trying to do association on the other
	 * STA iface. Roaming is disabled on all the ifaces to avoid
	 * this scenario.
	 */
	if_mgr_disable_roaming(pdev, vdev, RSO_CONNECT_START);

	wlan_tdls_teardown_links_sync(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS if_mgr_connect_complete(struct wlan_objmgr_vdev *vdev,
				   struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status = event_data->status;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	if (QDF_IS_STATUS_SUCCESS(status)) {
		/*
		 * Due to audio share glitch with P2P clients caused by roam
		 * scan on concurrent interface, disable roaming if
		 * "p2p_disable_roam" ini is enabled. Donot re-enable roaming
		 * again on other STA interface if p2p client connection is
		 * active on any vdev.
		 */
		if (ucfg_p2p_is_roam_config_disabled(psoc) &&
		    wlan_vdev_mlme_get_opmode(vdev) == QDF_P2P_CLIENT_MODE) {
			ifmgr_debug("p2p client active, keep roam disabled");
		} else {
			policy_mgr_set_pcl_for_connected_vdev(psoc,
							      vdev->vdev_objmgr.
							      vdev_id, false);
			/*
			 * Enable roaming on other STA iface except this one.
			 * Firmware doesn't support connection on one STA iface
			 * while roaming on other STA iface.
			 */
			if_mgr_enable_roaming(pdev, vdev, RSO_CONNECT_START);
		}
	} else {
		/* notify connect failure on final failure */
		ucfg_tdls_notify_connect_failure(psoc);

		/*
		 * Enable roaming on other STA iface except this one.
		 * Firmware doesn't support connection on one STA iface
		 * while roaming on other STA iface.
		 */
		if_mgr_enable_roaming(pdev, vdev, RSO_CONNECT_START);
	}

	policy_mgr_check_n_start_opportunistic_timer(psoc);

	policy_mgr_check_concurrent_intf_and_restart_sap(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS if_mgr_disconnect_start(struct wlan_objmgr_vdev *vdev,
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

	/* Leaving as stub to fill in later */

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS if_mgr_disconnect_complete(struct wlan_objmgr_vdev *vdev,
				      struct if_mgr_event_data *event_data)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status;

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc)
		return QDF_STATUS_E_FAILURE;

	status = if_mgr_enable_roaming_after_p2p_disconnect(pdev, vdev,
							RSO_INVALID_REQUESTOR);
	if (status) {
		ifmgr_err("Failed to enable roaming after p2p disconnect");
		return status;
	}

	policy_mgr_check_concurrent_intf_and_restart_sap(psoc);

	status = if_mgr_enable_roaming_on_connected_sta(pdev, vdev);
	if (status) {
		ifmgr_err("Failed to enable roaming on connected sta");
		return status;
	}

	return QDF_STATUS_SUCCESS;
}
