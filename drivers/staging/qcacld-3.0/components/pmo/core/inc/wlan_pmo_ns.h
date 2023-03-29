/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC: Declare ns offload feature API's
 */

#ifndef _WLAN_PMO_NS_H_
#define _WLAN_PMO_NS_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_pmo_common_public_struct.h"
#include "wlan_pmo_ns_public_struct.h"

/**
 * pmo_core_cache_ns_offload_req() - API to cache ns req in pmo vdev priv ctx
 * @ns_req: ns offload request
 *
 * API to cache ns offload in pmo vdev priv ctx
 *
 * Return:QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_cache_ns_offload_req(struct pmo_ns_req *ns_req);

/**
 * pmo_core_ns_check_offload() - API to check if offload cache/send is required
 * @psoc: objmgr psoc handle
 * @trigger: trigger reason enable ns offload
 * @vdev_id: vdev id
 *
 * Return:QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_ns_check_offload(struct wlan_objmgr_psoc *psoc,
				     enum pmo_offload_trigger trigger,
				     uint8_t vdev_id);

/**
 * pmo_core_flush_ns_offload_req() - API to flush ns req from pmo vdev priv ctx
 * @vdev: vdev objmgr handle
 *
 * API to flush ns offload from pmo vdev priv ctx
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_flush_ns_offload_req(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_core_enable_ns_offload_in_fwr() -  API to enable ns offload in fwr
 * @vdev: objmgr vdev
 * @trigger: trigger reason enable ns offload
 *
 * API to enable ns offload in fwr from vdev priv ctx
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_enable_ns_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
		enum pmo_offload_trigger trigger);

/**
 * pmo_core_disable_ns_offload_in_fwr() - API to disable ns offload in fwr
 * @vdev: objmgr vdev
 * @trigger: trigger reason disable ns offload
 *
 *  API to disable arp offload in fwr
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_disable_ns_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
		enum pmo_offload_trigger trigger);

/**
 * pmo_core_get_ns_offload_params() - API to get ns offload params
 * @vdev: objmgr vdev
 * @params: output pointer to hold offload params
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS
pmo_core_get_ns_offload_params(struct wlan_objmgr_vdev *vdev,
			       struct pmo_ns_offload_params *params);

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* end  of _WLAN_PMO_NS_H_ */
