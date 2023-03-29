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
 * DOC: Implements public API for PMO GTK offload feature to interact
 * with target/wmi.
 */

#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_gtk_public_struct.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"
#include "wlan_pmo_main.h"

QDF_STATUS pmo_tgt_send_gtk_offload_req(struct wlan_objmgr_vdev *vdev,
		struct pmo_gtk_req *gtk_req)
{
	struct pmo_gtk_req *op_gtk_req = NULL;
	QDF_STATUS status;
	struct wlan_objmgr_psoc *psoc;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		pmo_err("Failed to find psoc from from vdev:%pK",
			vdev);
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	vdev_ctx = pmo_vdev_get_priv(vdev);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_gtk_offload_req) {
		pmo_err("send_gtk_offload_req is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	op_gtk_req = qdf_mem_malloc(sizeof(*op_gtk_req));
	if (!op_gtk_req) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	if (gtk_req->flags == PMO_GTK_OFFLOAD_ENABLE) {
		qdf_atomic_set(&vdev_ctx->gtk_err_enable, true);
		qdf_mem_copy(op_gtk_req->kck, gtk_req->kck,
			     gtk_req->kck_len);
		qdf_mem_copy(op_gtk_req->kek, gtk_req->kek,
			     PMO_KEK_LEN);
		qdf_mem_copy(&op_gtk_req->replay_counter,
			&gtk_req->replay_counter, PMO_REPLAY_COUNTER_LEN);
	} else {
		qdf_atomic_set(&vdev_ctx->gtk_err_enable, false);
	}

	pmo_debug("replay counter %llu", op_gtk_req->replay_counter);
	op_gtk_req->flags = gtk_req->flags;
	status = pmo_tx_ops.send_gtk_offload_req(vdev, op_gtk_req);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to send gtk offload req event");
out:
	if (op_gtk_req)
		qdf_mem_free(op_gtk_req);
	pmo_exit();

	return status;
}

QDF_STATUS pmo_tgt_get_gtk_rsp(struct wlan_objmgr_vdev *vdev)
{

	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		pmo_err("Failed to find psoc from from vdev:%pK",
			vdev);
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_get_gtk_rsp_cmd) {
		pmo_err("send_get_gtk_rsp_cmd is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}
	status = pmo_tx_ops.send_get_gtk_rsp_cmd(vdev);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to send_get_gtk_rsp_cmd  event");
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_tgt_gtk_rsp_evt(struct wlan_objmgr_psoc *psoc,
			struct pmo_gtk_rsp_params *rsp_param)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	struct pmo_vdev_priv_obj *vdev_ctx;

	pmo_enter();
	if (!rsp_param) {
		pmo_err("gtk rsp param is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, rsp_param->vdev_id,
						    WLAN_PMO_ID);
	if (!vdev) {
		pmo_err("vdev is null vdev_id:%d psoc:%pK",
			rsp_param->vdev_id, psoc);
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	vdev_ctx = pmo_vdev_get_priv(vdev);

	status = pmo_get_vdev_bss_peer_mac_addr(vdev, &rsp_param->bssid);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("cannot find peer mac address");
		goto dec_ref;
	}

	/* update cached replay counter */
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->vdev_gtk_req.replay_counter = rsp_param->replay_counter;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	if (vdev_ctx->vdev_gtk_rsp_req.callback) {
		pmo_debug("callback:%pK context:%pK psoc:%pK vdev_id:%d",
			vdev_ctx->vdev_gtk_rsp_req.callback,
			vdev_ctx->vdev_gtk_rsp_req.callback_context,
			psoc, rsp_param->vdev_id);
		vdev_ctx->vdev_gtk_rsp_req.callback(
			vdev_ctx->vdev_gtk_rsp_req.callback_context,
			rsp_param);
	} else {
		pmo_err("gtk rsp callback is null for vdev_id:%d psoc %pK",
			rsp_param->vdev_id,
			psoc);
	}

dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:
	pmo_exit();

	return status;
}

