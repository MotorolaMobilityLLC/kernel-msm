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

#include "wlan_pmo_arp.h"
#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_main.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"

static QDF_STATUS pmo_core_cache_arp_in_vdev_priv(
			struct pmo_arp_req *arp_req,
			struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pmo_arp_offload_params *request = NULL;
	struct pmo_vdev_priv_obj *vdev_ctx;
	int index;
	struct qdf_mac_addr peer_bssid;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	request = qdf_mem_malloc(sizeof(*request));
	if (!request) {
		status = QDF_STATUS_E_NOMEM;
		goto exit_with_status;
	}

	status = pmo_get_vdev_bss_peer_mac_addr(vdev, &peer_bssid);
	if (status != QDF_STATUS_SUCCESS)
		goto free_req;

	qdf_mem_copy(&request->bssid.bytes, &peer_bssid.bytes,
			QDF_MAC_ADDR_SIZE);
	pmo_debug("vdev self mac addr: "QDF_MAC_ADDR_FMT" bss peer mac addr: "QDF_MAC_ADDR_FMT,
		QDF_MAC_ADDR_REF(wlan_vdev_mlme_get_macaddr(vdev)),
		QDF_MAC_ADDR_REF(peer_bssid.bytes));

	request->enable = PMO_OFFLOAD_ENABLE;
	request->is_offload_applied = false;
	/* converting u32 to IPV4 address */
	for (index = 0; index < QDF_IPV4_ADDR_SIZE; index++)
		request->host_ipv4_addr[index] =
		(arp_req->ipv4_addr >> (index * 8)) & 0xFF;

	/* cache arp request */
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	qdf_mem_copy(&vdev_ctx->vdev_arp_req, request,
		     sizeof(vdev_ctx->vdev_arp_req));
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	pmo_debug("cached arp offload; addr:" QDF_IPV4_ADDR_STR ", enable:%d",
		  QDF_IPV4_ADDR_ARRAY(request->host_ipv4_addr),
		  request->enable);

free_req:
	qdf_mem_free(request);

exit_with_status:

	return status;
}

static QDF_STATUS pmo_core_flush_arp_from_vdev_priv(
			struct wlan_objmgr_vdev *vdev)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	pmo_enter();

	vdev_ctx = pmo_vdev_get_priv(vdev);

	/* clear arp request */
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	qdf_mem_zero(&vdev_ctx->vdev_arp_req, sizeof(vdev_ctx->vdev_arp_req));
	vdev_ctx->vdev_arp_req.enable = PMO_OFFLOAD_DISABLE;
	vdev_ctx->vdev_arp_req.is_offload_applied = false;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	pmo_exit();

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
pmo_core_do_enable_arp_offload(struct wlan_objmgr_vdev *vdev,
			       uint8_t vdev_id,
			       enum pmo_offload_trigger trigger)
{
	QDF_STATUS status;
	struct pmo_psoc_priv_obj *psoc_ctx;
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	psoc_ctx = vdev_ctx->pmo_psoc_ctx;
	if (!psoc_ctx) {
		pmo_err("psoc_ctx is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	switch (trigger) {
	case pmo_ipv4_change_notify:
		if (!psoc_ctx->psoc_cfg.active_mode_offload) {
			pmo_debug("active offload is disabled, skip in mode %d",
				  trigger);
			status = QDF_STATUS_SUCCESS;
			goto out;
		}
		/* enable arp when active offload is true (ipv4 notifier) */
		status = pmo_tgt_enable_arp_offload_req(vdev, vdev_id);
		break;
	case pmo_apps_suspend:
		/* enable arp when active offload is false (apps suspend) */
		status = pmo_tgt_enable_arp_offload_req(vdev, vdev_id);
		break;
	default:
		status = QDF_STATUS_E_INVAL;
		pmo_err("invalid pmo trigger");
		break;
	}
out:

	return status;
}

static QDF_STATUS pmo_core_do_disable_arp_offload(struct wlan_objmgr_vdev *vdev,
		uint8_t vdev_id, enum pmo_offload_trigger trigger)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pmo_psoc_priv_obj *psoc_ctx;
	struct pmo_vdev_priv_obj *vdev_ctx;

	pmo_enter();

	vdev_ctx = pmo_vdev_get_priv(vdev);

	psoc_ctx = vdev_ctx->pmo_psoc_ctx;
	if (!psoc_ctx) {
		pmo_err("psoc_ctx is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	switch (trigger) {
	case pmo_apps_resume:
		/* disable arp on apps resume when active offload is disable */
		status = pmo_tgt_disable_arp_offload_req(vdev, vdev_id);
		break;
	default:
		status = QDF_STATUS_E_INVAL;
		pmo_err("invalid pmo trigger");
		break;
	}
out:
	pmo_exit();

	return status;
}

static QDF_STATUS pmo_core_arp_offload_sanity(
			struct wlan_objmgr_vdev *vdev)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	if (!vdev) {
		pmo_err("vdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_ctx = pmo_vdev_get_priv(vdev);
	if (!vdev_ctx->pmo_psoc_ctx->psoc_cfg.arp_offload_enable) {
		pmo_err("user disabled arp offload using ini");
		return QDF_STATUS_E_INVAL;
	}

	if (!pmo_core_is_vdev_supports_offload(vdev)) {
		pmo_debug("vdev in invalid opmode for arp offload %d",
			pmo_get_vdev_opmode(vdev));
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_INVAL;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_core_arp_check_offload(struct wlan_objmgr_psoc *psoc,
				      enum pmo_offload_trigger trigger,
				      uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pmo_psoc_priv_obj *psoc_ctx;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct wlan_objmgr_vdev *vdev;
	bool active_offload_cond, is_applied_cond;
	enum QDF_OPMODE opmode;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id, WLAN_PMO_ID);
	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	opmode = pmo_get_vdev_opmode(vdev);
	if (opmode == QDF_NDI_MODE) {
		pmo_debug("ARP offload is not supported in NaN mode");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
		return QDF_STATUS_E_INVAL;
	}

	vdev_ctx = pmo_vdev_get_priv(vdev);
	psoc_ctx = vdev_ctx->pmo_psoc_ctx;

	if (trigger == pmo_apps_suspend || trigger == pmo_apps_resume) {
		active_offload_cond = psoc_ctx->psoc_cfg.active_mode_offload;

		qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
		is_applied_cond = vdev_ctx->vdev_arp_req.enable &&
				  vdev_ctx->vdev_arp_req.is_offload_applied;
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

		if (active_offload_cond && is_applied_cond) {
			pmo_debug("active offload is enabled and offload already sent");
			wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
			return QDF_STATUS_E_INVAL;
		}
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:
	return status;
}

QDF_STATUS pmo_core_cache_arp_offload_req(struct pmo_arp_req *arp_req)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	if (!arp_req) {
		pmo_err("arp_req is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	if (!arp_req->psoc) {
		pmo_err("psoc is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(arp_req->psoc,
						    arp_req->vdev_id,
						    WLAN_PMO_ID);
	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	status = pmo_core_arp_offload_sanity(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto dec_ref;

	pmo_debug("Cache arp for vdev id: %d psoc: %pK vdev: %pK",
			arp_req->vdev_id, arp_req->psoc, vdev);

	status = pmo_core_cache_arp_in_vdev_priv(arp_req, vdev);
dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:

	return status;
}

QDF_STATUS pmo_core_flush_arp_offload_req(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	uint8_t vdev_id;

	pmo_enter();
	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_vdev_get_ref(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	status = pmo_core_arp_offload_sanity(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto def_ref;

	vdev_id = pmo_vdev_get_id(vdev);
	pmo_debug("Flush arp for vdev id: %d vdev: %pK", vdev_id, vdev);

	status = pmo_core_flush_arp_from_vdev_priv(vdev);
def_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_enable_arp_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
		enum pmo_offload_trigger trigger)
{
	QDF_STATUS status;
	uint8_t vdev_id;

	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_vdev_get_ref(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	status = pmo_core_arp_offload_sanity(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto put_ref;

	vdev_id = pmo_vdev_get_id(vdev);
	pmo_debug("Enable arp offload in fwr vdev id: %d vdev: %pK",
		vdev_id, vdev);

	status = pmo_core_do_enable_arp_offload(vdev, vdev_id, trigger);

put_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:;

	return status;
}

QDF_STATUS pmo_core_disable_arp_offload_in_fwr(struct wlan_objmgr_vdev *vdev,
		enum pmo_offload_trigger trigger)
{
	QDF_STATUS status;
	uint8_t vdev_id;

	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_vdev_get_ref(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	status = pmo_core_arp_offload_sanity(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto def_ref;

	vdev_id = pmo_vdev_get_id(vdev);
	pmo_debug("Disable arp offload in fwr vdev id: %d vdev: %pK",
		vdev_id, vdev);

	status = pmo_core_do_disable_arp_offload(vdev, vdev_id, trigger);
def_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:

	return status;
}

QDF_STATUS
pmo_core_get_arp_offload_params(struct wlan_objmgr_vdev *vdev,
				struct pmo_arp_offload_params *params)
{
	QDF_STATUS status;
	struct pmo_vdev_priv_obj *vdev_ctx;
	uint8_t vdev_id;

	pmo_enter();

	if (!params)
		return QDF_STATUS_E_INVAL;

	qdf_mem_zero(params, sizeof(*params));

	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_vdev_get_ref(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

	status = pmo_core_arp_offload_sanity(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto put_ref;

	vdev_id = pmo_vdev_get_id(vdev);
	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	*params = vdev_ctx->vdev_arp_req;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

put_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:
	pmo_exit();

	return status;
}
