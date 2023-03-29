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
 * DOC: Declare mc addr filtering offload feature API's
 */

#ifndef _WLAN_PMO_MC_ADDR_FILTERING_H_
#define _WLAN_PMO_MC_ADDR_FILTERING_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_pmo_common_public_struct.h"
#include "wlan_pmo_mc_addr_filtering_public_struct.h"

/**
 * pmo_core_set_mc_filter_req() -send mc filter set request
 * @vdev: objmgr vdev
 * @mc_list: a list of mc addresses to set in fwr
 *
 * Return: QDF_STATUS_SUCCESS in success else error codes
 */
QDF_STATUS pmo_core_set_mc_filter_req(struct wlan_objmgr_vdev *vdev,
	struct pmo_mc_addr_list *mc_list);

/**
 * pmo_clear_mc_filter_req() -send mc filter clear request
 * @vdev: objmgr vdev
 * @mc_list: a list of mc addresses to clear in fwr
 *
 * Return: QDF_STATUS_SUCCESS in success else error codes
 */
QDF_STATUS pmo_core_clear_mc_filter_req(struct wlan_objmgr_vdev *vdev,
	struct pmo_mc_addr_list *mc_list);

/**
 * pmo_core_cache_mc_addr_list(): API to cache mc addr list in pmo vdev priv obj
 * @psoc: objmgr psoc handle
 * @vdev_id: vdev id
 * @gtk_req: pmo gtk req param
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS pmo_core_cache_mc_addr_list(
		struct pmo_mc_addr_list_params *mc_list_config);

/**
 * pmo_core_flush_mc_addr_list(): API to flush mc addr list in pmo vdev priv obj
 * @psoc: objmgr psoc handle
 * @vdev_id: vdev id
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS pmo_core_flush_mc_addr_list(struct wlan_objmgr_psoc *psoc,
	uint8_t vdev_id);

/**
 * pmo_core_enhance_mc_filter_enable() - enable enhanced multicast filtering
 * @vdev: the vdev to enable enhanced multicast filtering for
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_core_enhanced_mc_filter_enable(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_core_enhance_mc_filter_disable() - disable enhanced multicast filtering
 * @vdev: the vdev to disable enhanced multicast filtering for
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_core_enhanced_mc_filter_disable(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_core_enable_mc_addr_filtering_in_fwr(): Enable cached mc add list in fwr
 * @psoc: objmgr psoc handle
 * @vdev_id: vdev id
 * @gtk_req: pmo gtk req param
 * @action: true for enable els false
 *
 * API to enable cached mc add list in fwr
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS pmo_core_enable_mc_addr_filtering_in_fwr(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id,
		enum pmo_offload_trigger trigger);

/**
 * pmo_core_disable_mc_addr_filtering_in_fwr(): Disable cached mc addr list
 * @psoc: objmgr psoc handle
 * @vdev_id: vdev id
 * @gtk_req: pmo gtk req param
 * @action: true for enable els false
 *
 * API to disable cached mc add list in fwr
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS pmo_core_disable_mc_addr_filtering_in_fwr(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id,
		enum pmo_offload_trigger trigger);

/**
 * pmo_core_get_mc_addr_list_count() -set  mc address count
 * @psoc: objmgr psoc
 * @vdev_id: vdev id
 *
 * Return: set mc address count
 */
void pmo_core_set_mc_addr_list_count(struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id, uint8_t count);

/**
 * pmo_core_get_mc_addr_list_count() -get current mc address count
 * @psoc: objmgr psoc
 * @vdev_id: vdev id
 *
 * Return: current mc address count
 */
int pmo_core_get_mc_addr_list_count(struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id);

/**
 * pmo_core_max_mc_addr_supported() -get max supported mc addresses
 * @psoc: objmgr psoc
 *
 * Return: max supported mc addresses
 */
uint8_t pmo_core_max_mc_addr_supported(struct wlan_objmgr_psoc *psoc);

/**
 * pmo_core_get_mc_addr_list() - Get mc addr list configured
 * @psoc: objmgr psoc
 * @vdev_id: vdev identifier
 * @mc_list_req: output pointer to hold mc addr list params
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS
pmo_core_get_mc_addr_list(struct wlan_objmgr_psoc *psoc,
			  uint8_t vdev_id,
			  struct pmo_mc_addr_list *mc_list_req);

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* end  of _WLAN_PMO_MC_ADDR_FILTERING_H_ */
