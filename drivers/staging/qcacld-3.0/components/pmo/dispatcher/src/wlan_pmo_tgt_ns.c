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
 * DOC: Implements public API for PMO NS offload feature to interact
 * with target/wmi.
 */

#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_arp_public_struct.h"
#include "wlan_pmo_ns_public_struct.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"
#include "wlan_pmo_main.h"

QDF_STATUS pmo_tgt_enable_ns_offload_req(struct wlan_objmgr_vdev *vdev,
		uint8_t vdev_id)
{
	struct pmo_arp_offload_params *arp_offload_req = NULL;
	struct pmo_ns_offload_params *ns_offload_req = NULL;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	psoc = pmo_vdev_get_psoc(vdev);

	arp_offload_req = qdf_mem_malloc(sizeof(*arp_offload_req));
	if (!arp_offload_req) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	ns_offload_req = qdf_mem_malloc(sizeof(*ns_offload_req));
	if (!ns_offload_req) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	qdf_mem_copy(arp_offload_req, &vdev_ctx->vdev_arp_req,
		sizeof(*arp_offload_req));
	qdf_mem_copy(ns_offload_req, &vdev_ctx->vdev_ns_req,
		sizeof(*ns_offload_req));
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	pmo_debug("vdev_id: %d: ARP offload %d NS offload  %d ns_count %u",
		  vdev_id,
		  arp_offload_req->enable, ns_offload_req->enable,
		  ns_offload_req->num_ns_offload_count);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_ns_offload_req) {
		pmo_err("send_ns_offload_req is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}
	status = pmo_tx_ops.send_ns_offload_req(
			vdev, arp_offload_req, ns_offload_req);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("Failed to send NS offload");
		goto out;
	}

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	if (vdev_ctx->vdev_arp_req.enable)
		vdev_ctx->vdev_arp_req.is_offload_applied = true;
	if (vdev_ctx->vdev_ns_req.enable)
		vdev_ctx->vdev_ns_req.is_offload_applied = true;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

out:
	if (arp_offload_req)
		qdf_mem_free(arp_offload_req);
	if (ns_offload_req)
		qdf_mem_free(ns_offload_req);

	return status;
}

QDF_STATUS pmo_tgt_disable_ns_offload_req(struct wlan_objmgr_vdev *vdev,
		uint8_t vdev_id)
{
	struct pmo_arp_offload_params *arp_offload_req = NULL;
	struct pmo_ns_offload_params *ns_offload_req = NULL;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();

	vdev_ctx = pmo_vdev_get_priv(vdev);

	psoc = pmo_vdev_get_psoc(vdev);

	arp_offload_req = qdf_mem_malloc(sizeof(*arp_offload_req));
	if (!arp_offload_req) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	ns_offload_req = qdf_mem_malloc(sizeof(*ns_offload_req));
	if (!ns_offload_req) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	qdf_mem_copy(arp_offload_req, &vdev_ctx->vdev_arp_req,
		sizeof(*arp_offload_req));
	qdf_mem_copy(ns_offload_req, &vdev_ctx->vdev_ns_req,
		sizeof(*ns_offload_req));
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	pmo_debug("ARP Offload vdev_id: %d enable: %d",
		vdev_id,
		arp_offload_req->enable);
	pmo_debug("NS Offload vdev_id: %d enable: %d ns_count: %u",
		vdev_id,
		ns_offload_req->enable,
		ns_offload_req->num_ns_offload_count);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_ns_offload_req) {
		pmo_err("send_ns_offload_req is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}
	status = pmo_tx_ops.send_ns_offload_req(
			vdev, arp_offload_req, ns_offload_req);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to send NS offload");

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->vdev_arp_req.is_offload_applied = false;
	vdev_ctx->vdev_ns_req.is_offload_applied = false;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

out:
	if (arp_offload_req)
		qdf_mem_free(arp_offload_req);
	if (ns_offload_req)
		qdf_mem_free(ns_offload_req);
	pmo_exit();

	return status;
}

