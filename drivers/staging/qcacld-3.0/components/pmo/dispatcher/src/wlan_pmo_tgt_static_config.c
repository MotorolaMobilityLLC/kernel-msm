/*
 * Copyright (c) 2017-2018, 2020 The Linux Foundation. All rights reserved.
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
 * DOC: Implements public API for pmo to interact with target/WMI
 */

#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_wow.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"
#include "wlan_pmo_main.h"

QDF_STATUS pmo_tgt_send_enhance_multicast_offload_req(
	struct wlan_objmgr_vdev *vdev,
	uint8_t action)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	psoc = pmo_vdev_get_psoc(vdev);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_enhance_mc_offload_req) {
		pmo_err("send_enhance_multicast_offload is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_tx_ops.send_enhance_mc_offload_req(vdev, action);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to config enhance multicast offload");
out:

	return status;
}

QDF_STATUS pmo_tgt_send_ra_filter_req(struct wlan_objmgr_vdev *vdev)
{

	QDF_STATUS status;
	uint8_t default_pattern;
	uint16_t ra_interval;
	struct pmo_vdev_priv_obj *vdev_ctx;
	uint8_t vdev_id;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();

	psoc = pmo_vdev_get_psoc(vdev);

	vdev_ctx = pmo_vdev_get_priv(vdev);

	vdev_id = pmo_vdev_get_id(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	ra_interval = vdev_ctx->pmo_psoc_ctx->psoc_cfg.ra_ratelimit_interval;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	pmo_debug("send RA rate limit [%d] to fw vdev = %d",
		 ra_interval, vdev_id);

	default_pattern = pmo_get_and_increment_wow_default_ptrn(vdev_ctx);
	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_ra_filter_req) {
		pmo_err("send_ra_filter_cmd is null");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	status = pmo_tx_ops.send_ra_filter_req(
			vdev, default_pattern, ra_interval);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("Failed to send RA rate limit to fw");
		pmo_decrement_wow_default_ptrn(vdev_ctx);
	}
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_tgt_send_action_frame_pattern_req(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_action_wakeup_set_params *cmd)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();

	psoc = pmo_vdev_get_psoc(vdev);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_action_frame_pattern_req) {
		pmo_err("send_add_action_frame_pattern_cmd is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_tx_ops.send_action_frame_pattern_req(vdev, cmd);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to add  filter");
out:
	pmo_exit();

	return status;
}

