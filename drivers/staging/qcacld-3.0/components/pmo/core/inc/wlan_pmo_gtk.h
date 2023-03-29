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
 * DOC: Declare gtk offload feature API's
 */

#ifndef _WLAN_PMO_GTK_H_
#define _WLAN_PMO_GTK_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_pmo_gtk_public_struct.h"

/**
 * pmo_core_cache_gtk_offload_req(): API to cache gtk req in pmo vdev priv obj
 * @vdev: objmgr vdev handle
 * @gtk_req: pmo gtk req param
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS pmo_core_cache_gtk_offload_req(struct wlan_objmgr_vdev *vdev,
		struct pmo_gtk_req *gtk_req);

/**
 * pmo_core_flush_gtk_offload_req(): Flush saved gtk req from pmo vdev priv obj
 * @vdev: objmgr vdev handle
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS pmo_core_flush_gtk_offload_req(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_core_enable_gtk_offload_in_fwr(): enable cached gtk request in fwr
 * @vdev: objmgr vdev handle
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS pmo_core_enable_gtk_offload_in_fwr(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_core_disable_gtk_offload_in_fwr(): disable cached gtk request in fwr
 * @vdev: objmgr vdev handle
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS pmo_core_disable_gtk_offload_in_fwr(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_core_get_gtk_rsp(): API to send gtk response request to fwr
 * @vdev: objmgr vdev handle
 * @gtk_rsp: pmo gtk response request
 *
 * This api will send gtk response request to fwr
 *
 * Return QDF_STATUS_SUCCESS -in case of success else return error
 */
QDF_STATUS pmo_core_get_gtk_rsp(struct wlan_objmgr_vdev *vdev,
			struct pmo_gtk_rsp_req *gtk_rsp_req);

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* end  of _WLAN_PMO_GTK_H_ */

