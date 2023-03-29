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
 * DOC: Declare arp offload feature API's
 */

#ifndef _WLAN_PMO_ARP_H_
#define _WLAN_PMO_ARP_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_pmo_arp_public_struct.h"

/**
 * pmo_core_cache_arp_offload_req() - API to cache arp req in pmo vdev priv ctx
 * @arp_req: arp offload request
 *
 * API To cache ARP offload in pmo vdev priv ctx
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_cache_arp_offload_req(struct pmo_arp_req *arp_req);

/**
 * pmo_core_arp_check_offload(): API to check if arp offload cache/send is req
 * @psoc: objmgr psoc handle
 * @trigger: trigger reason
 * @vdev_id: vdev id
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS pmo_core_arp_check_offload(struct wlan_objmgr_psoc *psoc,
				      enum pmo_offload_trigger trigger,
				      uint8_t vdev_id);

/**
 * pmo_core_flush_arp_offload_req() - API to flush arp req from pmo vdev ctx
 * @vdev: objmgr vdev
 *
 * API To flush saved ARP request from pmo vdev prov ctx
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_flush_arp_offload_req(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_core_enable_arp_offload_in_fwr() - API to enable arp offload in fwr
 * @vdev: objmgr vdev
 * @trigger: trigger reason
 *
 *  API to enable arp offload in fwr from vdev priv ctx
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_enable_arp_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
		enum pmo_offload_trigger trigger);

/**
 * pmo_core_disable_arp_offload_in_fwr() - API to disable arp offload in fwr
 * @vdev: objmgr vdev
 * @trigger: trigger reason
 *
 *  API to disable arp offload in fwr
 *
 * Return: QQDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS pmo_core_disable_arp_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
		enum pmo_offload_trigger trigger);

/**
 * pmo_core_get_arp_offload_params() - API to get arp offload params
 * @vdev: objmgr vdev
 * @params: output pointer to hold offload params
 *
 * Return: QDF_STATUS_SUCCESS in case of success else return error
 */
QDF_STATUS
pmo_core_get_arp_offload_params(struct wlan_objmgr_vdev *vdev,
				struct pmo_arp_offload_params *params);

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* end  of _WLAN_PMO_ARP_H_ */
