/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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
 * DOC: Implements arp offload feature API's
 */

#include "qdf_lock.h"
#include "wlan_pmo_hw_filter.h"
#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_main.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"

QDF_STATUS pmo_core_enable_hw_filter_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	struct pmo_psoc_priv_obj *psoc_priv;
	enum pmo_hw_filter_mode mode_bitmap;
	struct pmo_hw_filter_params req = {0};

	pmo_enter();

	status = pmo_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto exit_with_status;

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_NOSUPPORT;
		wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
		goto exit_with_status;
	}

	psoc_priv = pmo_vdev_get_psoc_priv(vdev);
	qdf_spin_lock_bh(&psoc_priv->lock);
	mode_bitmap = psoc_priv->psoc_cfg.hw_filter_mode_bitmap;
	qdf_spin_unlock_bh(&psoc_priv->lock);

	req.vdev_id = pmo_vdev_get_id(vdev);
	req.mode_bitmap = psoc_priv->psoc_cfg.hw_filter_mode_bitmap;
	req.enable = true;
	status = pmo_tgt_conf_hw_filter(pmo_vdev_get_psoc(vdev), &req);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);

exit_with_status:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_disable_hw_filter_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	struct pmo_psoc_priv_obj *psoc_priv;
	enum pmo_hw_filter_mode mode_bitmap;
	struct pmo_hw_filter_params req = {0};

	pmo_enter();

	status = pmo_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto exit_with_status;

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_NOSUPPORT;
		wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
		goto exit_with_status;
	}

	psoc_priv = pmo_vdev_get_psoc_priv(vdev);
	qdf_spin_lock_bh(&psoc_priv->lock);
	mode_bitmap = psoc_priv->psoc_cfg.hw_filter_mode_bitmap;
	qdf_spin_unlock_bh(&psoc_priv->lock);

	req.vdev_id = pmo_vdev_get_id(vdev);
	req.mode_bitmap = mode_bitmap;
	req.enable = false;
	status = pmo_tgt_conf_hw_filter(pmo_vdev_get_psoc(vdev), &req);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);

exit_with_status:
	pmo_exit();

	return status;
}
