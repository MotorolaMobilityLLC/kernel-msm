/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_IF_MGR_ROAM_H_
#define _WLAN_IF_MGR_ROAM_H_

#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_if_mgr_public_struct.h"
#include "wlan_if_mgr_roam.h"

/**
 * struct change_roam_state_arg - Contains roam state arguments
 * @requestor: Driver disabled roaming requestor
 * @curr_vdev_id: virtual device ID
 *
 * This structure is used to pass the roam state change information to the
 * callback
 */
struct change_roam_state_arg {
	enum wlan_cm_rso_control_requestor requestor;
	uint8_t curr_vdev_id;
};

/**
 * struct bssid_search_arg - Contains candidate validation arguments
 * @peer_addr: MAC address of the BSS
 * @vdev_id: virtual device ID
 *
 * This structure is used to pass the candidate validation information to the
 * callback
 */
struct bssid_search_arg {
	struct qdf_mac_addr peer_addr;
	uint8_t vdev_id;
};

/**
 * allow_mcc_go_diff_bi_definition - Defines the config values for allowing
 * different beacon intervals between P2P-G0 and STA
 * @ALLOW_MCC_GO_DIFF_BI_WFA_CERT: GO Beacon interval is not changed.
 *	MCC GO doesn't work well in optimized way. In worst scenario, it may
 *	invite STA disconnection.
 * @ALLOW_MCC_GO_DIFF_BI_WORKAROUND: Workaround 1 disassoc all the clients and
 *	update beacon Interval.
 * @ALLOW_MCC_GO_DIFF_BI_TEAR_DOWN: Tear down the P2P link in
 *	auto/Non-autonomous -GO case.
 * @ALLOW_MCC_GO_DIFF_BI_NO_DISCONNECT: Don't disconnect the P2P client in
 *	autonomous/Non-autonomous -GO case update the BI dynamically
 */
enum allow_mcc_go_diff_bi_definition {
	ALLOW_MCC_GO_DIFF_BI_WFA_CERT = 1,
	ALLOW_MCC_GO_DIFF_BI_WORKAROUND,
	ALLOW_MCC_GO_DIFF_BI_TEAR_DOWN,
	ALLOW_MCC_GO_DIFF_BI_NO_DISCONNECT,
};

/**
 * struct beacon_interval_arg - Contains beacon interval validation arguments
 * @curr_vdev_id: current iterator vdev ID
 * @curr_bss_opmode: current iterator BSS's opmode
 * @ch_freq: current operating channel frequency
 * @bss_beacon_interval: beacon interval that can be updated by callee
 * @status: status to be filled by callee
 * @is_done: boolean to stop iterating
 * @update_beacon_interval: boolean to mark beacon interval as updated by callee
 *
 * This structure is used to pass the candidate validation information to the
 * callback
 */
struct beacon_interval_arg {
	uint8_t curr_vdev_id;
	enum QDF_OPMODE curr_bss_opmode;
	qdf_freq_t ch_freq;
	uint16_t bss_beacon_interval;
	QDF_STATUS status;
	bool is_done;
	bool update_beacon_interval;
};

/**
 * if_mgr_enable_roaming() - interface manager enable roaming
 * @pdev: pdev object
 * @vdev: vdev object
 * @requestor: RSO enable requestor
 *
 * Interface manager api to enable roaming for all other active vdev id's
 *
 * Context: It should run in thread context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS if_mgr_enable_roaming(struct wlan_objmgr_pdev *pdev,
				 struct wlan_objmgr_vdev *vdev,
				 enum wlan_cm_rso_control_requestor requestor);

/**
 * if_mgr_disable_roaming() - interface manager disable roaming
 * @pdev: pdev object
 * @vdev: vdev object
 * @requestor: RSO disable requestor
 *
 * Interface manager api to disable roaming for all other active vdev id's
 *
 * Context: It should run in thread context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS if_mgr_disable_roaming(struct wlan_objmgr_pdev *pdev,
				  struct wlan_objmgr_vdev *vdev,
				  enum wlan_cm_rso_control_requestor requestor);

/**
 * if_mgr_enable_roaming_on_connected_sta() - interface manager disable roaming
 * on connected STA
 * @pdev: pdev object
 * @vdev: vdev object
 *
 * Loops through connected vdevs and disables roaming if it is STA
 *
 * Context: It should run in thread context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
if_mgr_enable_roaming_on_connected_sta(struct wlan_objmgr_pdev *pdev,
				       struct wlan_objmgr_vdev *vdev);

/**
 * if_mgr_enable_roaming_after_p2p_disconnect() - interface manager enable
 * roaming after p2p disconnect
 * @pdev: pdev object
 * @vdev: vdev object
 * @requestor: RSO enable requestor
 *
 * Disables roaming on p2p vdevs if the state is disconnected
 *
 * Context: It should run in thread context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS if_mgr_enable_roaming_after_p2p_disconnect(
				struct wlan_objmgr_pdev *pdev,
				struct wlan_objmgr_vdev *vdev,
				enum wlan_cm_rso_control_requestor requestor);

/**
 * if_mgr_is_beacon_interval_valid() - checks if the concurrent session is
 * valid
 * @pdev: pdev object
 * @vdev_id: vdev ID
 * @candidate: concurrent candidate info
 *
 * This function validates the beacon interval with all other active vdevs.
 *
 * Context: It should run in thread context
 *
 * Return: true if session is valid, false if not
 */
bool if_mgr_is_beacon_interval_valid(struct wlan_objmgr_pdev *pdev,
				     uint8_t vdev_id,
				     struct validate_bss_data *candidate);

/**
 * if_mgr_validate_candidate() - validate candidate event handler
 * @vdev: vdev object
 * @event_data: Interface mgr event data
 *
 * This function will validate the candidate to see if it is a suitable option
 * for roaming to.
 *
 * Context: It should run in thread context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS if_mgr_validate_candidate(struct wlan_objmgr_vdev *vdev,
				     struct if_mgr_event_data *event_data);

#endif
