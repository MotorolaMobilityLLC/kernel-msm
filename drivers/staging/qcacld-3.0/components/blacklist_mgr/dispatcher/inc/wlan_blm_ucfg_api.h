/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: declare UCFG APIs exposed by the blacklist manager component
 */

#ifndef _WLAN_BLM_UCFG_H_
#define _WLAN_BLM_UCFG_H_

#include "qdf_types.h"
#include "wlan_objmgr_psoc_obj.h"
#include <wlan_blm_public_struct.h>

#ifdef FEATURE_BLACKLIST_MGR

/**
 * ucfg_blm_init() - initialize blacklist mgr context
 *
 * This function initializes the blacklist mgr context
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_blm_init(void);

/**
 * ucfg_blm_deinit() - De initialize blacklist mgr context
 *
 * This function De initializes blacklist mgr context
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_blm_deinit(void);

/**
 * ucfg_blm_psoc_set_suspended() - API to set blacklist mgr state suspended
 * @psoc: pointer to psoc object
 * @state: state to be set
 *
 * This function sets blacklist mgr state to suspended
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_blm_psoc_set_suspended(struct wlan_objmgr_psoc *psoc,
				       bool state);

/**
 * ucfg_blm_psoc_get_suspended() - API to get blacklist mgr state suspended
 * @psoc: pointer to psoc object
 * @state: pointer to get suspend state of blacklist manager
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_blm_psoc_get_suspended(struct wlan_objmgr_psoc *psoc,
				       bool *state);

/**
 * ucfg_blm_psoc_open() - API to initialize the cfg when psoc is initialized.
 * @psoc: psoc object
 *
 * This function initializes the config of blacklist mgr.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_blm_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_blm_psoc_close() - API to deinit the blm when psoc is deinitialized.
 * @psoc: psoc object
 *
 * This function deinits the blm psoc object.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error
 */
QDF_STATUS ucfg_blm_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_blm_add_userspace_black_list() - Clear already existing userspace BSSID,
 * and add the new ones to blacklist manager.
 * @pdev: pdev object
 * @bssid_black_list: BSSIDs to be blacklisted by userspace.
 * @num_of_bssid: num of bssids to be blacklisted.
 *
 * This API clear already existing userspace BSSID, and add the new ones to
 * blacklist manager
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error.
 */
QDF_STATUS
ucfg_blm_add_userspace_black_list(struct wlan_objmgr_pdev *pdev,
				  struct qdf_mac_addr *bssid_black_list,
				  uint8_t num_of_bssid);

/**
 * ucfg_blm_dump_black_list_ap() - get blacklisted bssid.
 * @pdev: pdev object
 *
 * This API dumps blacklist ap
 *
 * Return: None
 */
void ucfg_blm_dump_black_list_ap(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_blm_update_bssid_connect_params() - Inform the BLM about connect or
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
void
ucfg_blm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr bssid,
				     enum blm_connection_state con_state);

/**
 * ucfg_blm_add_bssid_to_reject_list() - Add BSSID to the specific reject list.
 * @pdev: Pdev object
 * @ap_info: Ap info params such as BSSID, and the type of rejection to be done
 *
 * This API will add the BSSID to the reject AP list maintained by the blacklist
 * manager.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success else return error.
 */
QDF_STATUS
ucfg_blm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
				  struct reject_ap_info *ap_info);

/**
 * ucfg_blm_wifi_off() - Inform the blacklist manager about wifi off
 * @blm_ctx: blacklist manager pdev priv object
 *
 * This API will inform the blacklist manager that the user has turned wifi off
 * from the UI, and the blacklist manager can take action based upon this.
 *
 * Return: None
 */
void
ucfg_blm_wifi_off(struct wlan_objmgr_pdev *pdev);

#else
static inline
QDF_STATUS ucfg_blm_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_blm_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_blm_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_blm_psoc_close(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_blm_dump_black_list_ap(struct wlan_objmgr_pdev *pdev)
{}

static inline
QDF_STATUS
ucfg_blm_add_bssid_to_reject_list(struct wlan_objmgr_pdev *pdev,
				  struct reject_ap_info *ap_info)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
ucfg_blm_add_userspace_black_list(struct wlan_objmgr_pdev *pdev,
				  struct qdf_mac_addr *bssid_black_list,
				  uint8_t num_of_bssid)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
ucfg_blm_update_bssid_connect_params(struct wlan_objmgr_pdev *pdev,
				     struct qdf_mac_addr bssid,
				     enum blm_connection_state con_state)
{
}

static inline
void ucfg_blm_wifi_off(struct wlan_objmgr_pdev *pdev)
{
}

#endif
#endif /* _WLAN_BLM_UCFG_H_ */
