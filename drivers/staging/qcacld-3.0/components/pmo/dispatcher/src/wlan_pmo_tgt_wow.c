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

QDF_STATUS pmo_tgt_enable_wow_wakeup_event(
		struct wlan_objmgr_vdev *vdev,
		uint32_t *bitmap)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;
	int vdev_id;

	psoc = pmo_vdev_get_psoc(vdev);
	vdev_id = pmo_vdev_get_id(vdev);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_enable_wow_wakeup_event_req) {
		pmo_err("send_enable_wow_wakeup_event_req is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	pmo_debug("Enable wakeup events 0x%08x%08x%08x%08x for vdev_id %d",
		  bitmap[3], bitmap[2], bitmap[1], bitmap[0], vdev_id);

	status = pmo_tx_ops.send_enable_wow_wakeup_event_req(vdev, bitmap);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to enable wow wakeup event");
out:

	return status;
}

QDF_STATUS pmo_tgt_disable_wow_wakeup_event(
		struct wlan_objmgr_vdev *vdev,
		uint32_t *bitmap)
{
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;
	int vdev_id;

	psoc = pmo_vdev_get_psoc(vdev);
	vdev_id = pmo_vdev_get_id(vdev);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_disable_wow_wakeup_event_req) {
		pmo_err("send_disable_wow_wakeup_event_req is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	pmo_debug("Disable wakeup events 0x%x%x%x%x for vdev_id %d",
		  bitmap[3], bitmap[2], bitmap[1], bitmap[0], vdev_id);

	status = pmo_tx_ops.send_disable_wow_wakeup_event_req(vdev, bitmap);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to disable wow wakeup event");
out:

	return status;
}

QDF_STATUS pmo_tgt_send_wow_patterns_to_fw(
		struct wlan_objmgr_vdev *vdev, uint8_t ptrn_id,
		const uint8_t *ptrn, uint8_t ptrn_len,
		uint8_t ptrn_offset, const uint8_t *mask,
		uint8_t mask_len, bool user)
{
	QDF_STATUS status;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	psoc = pmo_vdev_get_psoc(vdev);

	vdev_ctx = pmo_vdev_get_priv(vdev);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_add_wow_pattern) {
		pmo_err("send_add_wow_pattern is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}
	status = pmo_tx_ops.send_add_wow_pattern(
				vdev, ptrn_id, ptrn,
				ptrn_len, ptrn_offset, mask,
				mask_len, user);
	if (status != QDF_STATUS_SUCCESS) {
		if (!user)
			pmo_decrement_wow_default_ptrn(vdev_ctx);
		pmo_err("Failed to send wow pattern event");
		goto out;
	}

	if (user)
		pmo_increment_wow_user_ptrn(vdev_ctx);
out:

	return status;
}

QDF_STATUS pmo_tgt_del_wow_pattern(
		struct wlan_objmgr_vdev *vdev, uint8_t ptrn_id,
		bool user)
{
	QDF_STATUS status;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	psoc = pmo_vdev_get_psoc(vdev);
	vdev_ctx = pmo_vdev_get_priv(vdev);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.del_wow_pattern) {
		pmo_err("del_wow_pattern is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}
	status = pmo_tx_ops.del_wow_pattern(vdev, ptrn_id);
	if (status) {
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	if (user)
		pmo_decrement_wow_user_ptrn(vdev_ctx);
out:

	return status;
}

