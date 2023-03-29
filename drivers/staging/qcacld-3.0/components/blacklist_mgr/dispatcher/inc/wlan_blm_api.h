/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
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
 * DOC: declare public APIs exposed by the blacklist manager component
 */

#ifndef _WLAN_BLM_API_H_
#define _WLAN_BLM_API_H_

#include "qdf_types.h"
#include "wlan_objmgr_pdev_obj.h"
#include <wlan_blm_public_struct.h>

#ifdef FEATURE_BLACKLIST_MGR
#include "wlan_blm_core.h"

/**
 * wlan_blm_add_bssid_to_reject_list() - Add BSSID to the specific reject list.
 * @pdev: Pdev object
 * @ap_info: Ap info params such as BSSID, and the type of rejection to be done
 *
 * This API will add the BSSID to the reject AP list maintained by the blacklist
 * manager.
 */
static inline QDF_STATUS
wlan_blm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
				  struct reject_ap_info *ap_info)
{
	return blm_add_bssid_to_reject_list(pdev, ap_info);
}

/**
 * wlan_blm_update_bssid_connect_params() - Inform the BLM about connect or
 * disconnect with the current AP.
 * @pdev: pdev object
 * @bssid: BSSID of the AP
 * @con_state: Connection stae (connected/disconnected)
 *
 * This API will inform the BLM about the state with the AP so that if the AP
 * is selected, and the connection went through, and the connection did not
 * face any data stall till the bad bssid reset timer, BLM can remove the
 * AP from the reject ap list maintained by it.
 *
 * Return: None
 */
static inline void
wlan_blm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr bssid,
				     enum blm_connection_state con_state)
{
	return blm_update_bssid_connect_params(pdev, bssid, con_state);
}

/**
 * wlan_blm_get_bssid_reject_list() - Get the BSSIDs in reject list from BLM
 * @pdev: pdev object
 * @reject_list: reject list to be filled (passed by caller)
 * @max_bssid_to_be_filled: num of bssids filled in reject list by BLM
 * @reject_ap_type: reject ap type of the BSSIDs to be filled.
 *
 * This API is a wrapper to an API of blacklist manager which will fill the
 * reject ap list requested by caller of type given as argument reject_ap_type,
 * and will return the number of BSSIDs filled.
 *
 * Return: Unsigned integer (number of BSSIDs filled by the blacklist manager)
 */
static inline uint8_t
wlan_blm_get_bssid_reject_list(struct wlan_objmgr_pdev *pdev,
			       struct reject_ap_config_params *reject_list,
			       uint8_t max_bssid_to_be_filled,
			       enum blm_reject_ap_type reject_ap_type)
{
	return blm_get_bssid_reject_list(pdev, reject_list,
					 max_bssid_to_be_filled,
					 reject_ap_type);
}

/**
 * wlan_blm_dump_blcklist_bssid() - dump the blacklisted BSSIDs from BLM
 * @pdev: pdev object
 *
 * Return: None
 */
static inline void
wlan_blm_dump_blcklist_bssid(struct wlan_objmgr_pdev *pdev)
{
	return blm_dump_blacklist_bssid(pdev);
}

/**
 * wlan_blm_get_rssi_blacklist_threshold() - Get the RSSI blacklist threshold
 * @pdev: pdev object
 *
 * This API will get the rssi blacklist threshold value configured via
 * CFG_BLACKLIST_RSSI_THRESHOLD.
 *
 * Return: int32_t (threshold value)
 */
static inline int32_t
wlan_blm_get_rssi_blacklist_threshold(struct wlan_objmgr_pdev *pdev)
{
	return blm_get_rssi_blacklist_threshold(pdev);
}

#else

static inline QDF_STATUS
wlan_blm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
				  struct reject_ap_info *ap_info)
{
	return QDF_STATUS_SUCCESS;
}

static inline uint8_t
wlan_blm_get_bssid_reject_list(struct wlan_objmgr_pdev *pdev,
			       struct reject_ap_config_params *reject_list,
			       uint8_t max_bssid_to_be_filled,
			       enum blm_reject_ap_type reject_ap_type)
{
	return 0;
}

static inline void
wlan_blm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr bssid,
				     enum blm_connection_state con_state)
{
}

static inline int32_t
wlan_blm_get_rssi_blacklist_threshold(struct wlan_objmgr_pdev *pdev)
{
	return 0;
}
#endif
#endif
