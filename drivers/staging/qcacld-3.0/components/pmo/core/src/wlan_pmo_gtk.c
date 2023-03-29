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
 * DOC: Implements gtk offload feature API's
 */

#include "wlan_pmo_gtk.h"
#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_main.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"

static QDF_STATUS pmo_core_cache_gtk_req_in_vdev_priv(
		struct wlan_objmgr_vdev *vdev,
		struct pmo_gtk_req *gtk_req)
{
	struct pmo_vdev_priv_obj *vdev_ctx;
	QDF_STATUS status;
	struct qdf_mac_addr peer_bssid;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	status = pmo_get_vdev_bss_peer_mac_addr(vdev, &peer_bssid);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	qdf_mem_copy(&vdev_ctx->vdev_gtk_req, gtk_req,
		sizeof(vdev_ctx->vdev_gtk_req));
	qdf_mem_copy(&vdev_ctx->vdev_gtk_req.bssid,
		&peer_bssid, QDF_MAC_ADDR_SIZE);
	vdev_ctx->vdev_gtk_req.flags = PMO_GTK_OFFLOAD_ENABLE;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS pmo_core_flush_gtk_req_from_vdev_priv(
		struct wlan_objmgr_vdev *vdev)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	qdf_mem_zero(&vdev_ctx->vdev_gtk_req, sizeof(vdev_ctx->vdev_gtk_req));
	vdev_ctx->vdev_gtk_req.flags = PMO_GTK_OFFLOAD_DISABLE;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS pmo_core_do_enable_gtk_offload(
			struct wlan_objmgr_vdev *vdev,
			struct pmo_vdev_priv_obj *vdev_ctx,
			struct pmo_gtk_req *op_gtk_req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t vdev_id;
	enum QDF_OPMODE op_mode;

	op_mode = pmo_get_vdev_opmode(vdev);
	if (QDF_NDI_MODE == op_mode) {
		pmo_debug("gtk offload is not supported in NaN mode");
		return QDF_STATUS_E_INVAL;
	}

	if (!pmo_core_is_vdev_supports_offload(vdev)) {
		pmo_debug("vdev in invalid opmode for gtk offload %d",
			pmo_get_vdev_opmode(vdev));
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_INVAL;

	vdev_id = pmo_vdev_get_id(vdev);

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	qdf_mem_copy(op_gtk_req, &vdev_ctx->vdev_gtk_req,
		sizeof(*op_gtk_req));
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	if ((op_gtk_req->flags == PMO_GTK_OFFLOAD_ENABLE) &&
	    (qdf_atomic_read(&vdev_ctx->gtk_err_enable) == 1)) {
		pmo_debug("GTK Offload already enabled, Disabling vdev_id: %d",
			vdev_id);
		op_gtk_req->flags = PMO_GTK_OFFLOAD_DISABLE;
		status = pmo_tgt_send_gtk_offload_req(vdev, op_gtk_req);
		if (status != QDF_STATUS_SUCCESS) {
			pmo_err("Failed to disable GTK Offload");
			goto out;
		}
		pmo_debug("Enable GTK Offload again with updated inputs");
		op_gtk_req->flags = PMO_GTK_OFFLOAD_ENABLE;
	}

	status = pmo_tgt_send_gtk_offload_req(vdev, op_gtk_req);
out:

	return status;
}

static QDF_STATUS pmo_core_is_gtk_enabled_in_fwr(
			struct wlan_objmgr_vdev *vdev,
			struct pmo_vdev_priv_obj *vdev_ctx)
{
	QDF_STATUS status;
	struct qdf_mac_addr peer_bssid;

	if (!pmo_core_is_vdev_supports_offload(vdev)) {
		pmo_debug("vdev in invalid opmode for gtk offload enable %d",
			pmo_get_vdev_opmode(vdev));
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_INVAL;

	status = pmo_get_vdev_bss_peer_mac_addr(vdev,
			&peer_bssid);
	if (status != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_INVAL;

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	if (qdf_mem_cmp(&vdev_ctx->vdev_gtk_req.bssid,
		&peer_bssid, QDF_MAC_ADDR_SIZE)) {
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
		pmo_err("cache request mac:"QDF_MAC_ADDR_FMT", peer mac:"QDF_MAC_ADDR_FMT" are not same",
			QDF_MAC_ADDR_REF(vdev_ctx->vdev_gtk_req.bssid.bytes),
			QDF_MAC_ADDR_REF(peer_bssid.bytes));
		return QDF_STATUS_E_INVAL;
	}

	if (vdev_ctx->vdev_gtk_req.flags != PMO_GTK_OFFLOAD_ENABLE) {
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
		pmo_err("gtk flag is disabled hence no gtk rsp required");
		return QDF_STATUS_E_INVAL;
	}
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS pmo_core_do_disable_gtk_offload(
			struct wlan_objmgr_vdev *vdev,
			struct pmo_vdev_priv_obj *vdev_ctx,
			struct pmo_gtk_req *op_gtk_req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum QDF_OPMODE op_mode;

	op_mode = pmo_get_vdev_opmode(vdev);
	if (QDF_NDI_MODE == op_mode) {
		pmo_debug("gtk offload is not supported in NaN mode");
		return QDF_STATUS_E_INVAL;
	}

	status = pmo_core_is_gtk_enabled_in_fwr(vdev, vdev_ctx);
	if (status != QDF_STATUS_SUCCESS)
		return status;

	op_gtk_req->flags = PMO_GTK_OFFLOAD_DISABLE;
	status = pmo_tgt_send_gtk_offload_req(vdev, op_gtk_req);

	return status;
}

QDF_STATUS pmo_core_cache_gtk_offload_req(struct wlan_objmgr_vdev *vdev,
		struct pmo_gtk_req *gtk_req)
{
	QDF_STATUS status;
	enum QDF_OPMODE opmode;
	uint8_t vdev_id;

	if (!gtk_req) {
		pmo_err("gtk_req is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	status = pmo_vdev_get_ref(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	opmode = pmo_get_vdev_opmode(vdev);
	vdev_id = pmo_vdev_get_id(vdev);
	pmo_debug("vdev opmode: %d vdev_id: %d", opmode, vdev_id);
	if (!pmo_core_is_vdev_supports_offload(vdev)) {
		pmo_debug("vdev in invalid opmode for caching gtk request %d",
			opmode);
		status = QDF_STATUS_E_INVAL;
		goto dec_ref;
	}

	status = pmo_core_cache_gtk_req_in_vdev_priv(vdev, gtk_req);
dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:

	return status;
}

QDF_STATUS pmo_core_flush_gtk_offload_req(struct wlan_objmgr_vdev *vdev)
{
	enum QDF_OPMODE opmode;
	uint8_t vdev_id;
	QDF_STATUS status;

	if (!vdev) {
		pmo_err("psoc is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	status = pmo_vdev_get_ref(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	opmode = pmo_get_vdev_opmode(vdev);
	vdev_id = pmo_vdev_get_id(vdev);
	pmo_debug("vdev opmode: %d vdev_id: %d", opmode, vdev_id);
	if (!pmo_core_is_vdev_supports_offload(vdev)) {
		pmo_debug("vdev in invalid opmode for flushing gtk request %d",
			opmode);
		status = QDF_STATUS_E_INVAL;
		goto dec_ref;
	}

	status = pmo_core_flush_gtk_req_from_vdev_priv(vdev);
dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:

	return status;
}

QDF_STATUS pmo_core_enable_gtk_offload_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct pmo_gtk_req *op_gtk_req = NULL;

	pmo_enter();
	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	status = pmo_vdev_get_ref(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	op_gtk_req = qdf_mem_malloc(sizeof(*op_gtk_req));
	if (!op_gtk_req) {
		status = QDF_STATUS_E_INVAL;
		goto dec_ref;
	}
	status = pmo_core_do_enable_gtk_offload(vdev, vdev_ctx, op_gtk_req);
dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:
	if (op_gtk_req)
		qdf_mem_free(op_gtk_req);
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_disable_gtk_offload_in_fwr(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct pmo_gtk_req *op_gtk_req = NULL;

	pmo_enter();
	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	status = pmo_vdev_get_ref(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	op_gtk_req = qdf_mem_malloc(sizeof(*op_gtk_req));
	if (!op_gtk_req) {
		status = QDF_STATUS_E_NOMEM;
		goto dec_ref;
	}

	status = pmo_core_do_disable_gtk_offload(vdev, vdev_ctx, op_gtk_req);
dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:
	if (op_gtk_req)
		qdf_mem_free(op_gtk_req);
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_get_gtk_rsp(struct wlan_objmgr_vdev *vdev,
			struct pmo_gtk_rsp_req *gtk_rsp_req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pmo_vdev_priv_obj *vdev_ctx;

	pmo_enter();
	if (!gtk_rsp_req || !vdev) {
		pmo_err("%s is null", !vdev ? "vdev":"gtk_rsp_req");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	status = pmo_vdev_get_ref(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	status = pmo_core_is_gtk_enabled_in_fwr(vdev, vdev_ctx);
	if (status != QDF_STATUS_SUCCESS)
		goto dec_ref;

	/* cache gtk rsp request */
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	qdf_mem_copy(&vdev_ctx->vdev_gtk_rsp_req, gtk_rsp_req,
		sizeof(vdev_ctx->vdev_gtk_rsp_req));
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
	/* send cmd to fwr */
	status = pmo_tgt_get_gtk_rsp(vdev);
dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:
	pmo_exit();

	return status;
}

