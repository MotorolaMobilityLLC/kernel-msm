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
 * DOC: Declare hardware filter offload feature APIs
 */

#ifndef _WLAN_PMO_HW_FILTER_H_
#define _WLAN_PMO_HW_FILTER_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "qdf_status.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_pmo_hw_filter_public_struct.h"

/**
 * pmo_core_enable_hw_filter_in_fwr() - enable previously configured hw filter
 * @vdev: objmgr vdev to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_core_enable_hw_filter_in_fwr(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_core_disable_hw_filter_in_fwr() - disable previously configured hw filter
 * @vdev: objmgr vdev to configure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_core_disable_hw_filter_in_fwr(struct wlan_objmgr_vdev *vdev);

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* _WLAN_PMO_HW_FILTER_H_*/
