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
 * DOC: Implements mc addr filtering offload feature API's
 */

#include "wlan_pmo_mc_addr_filtering.h"
#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_main.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"

#define PMO_INVALID_MC_ADDR_COUNT (-1)

static void pmo_core_fill_mc_list(struct pmo_vdev_priv_obj **vdev_ctx,
	struct pmo_mc_addr_list_params *ip)
{
	struct pmo_mc_addr_list *op_list;
	int i, j = 0;
	static const uint8_t ipv6_rs[] = {
		0x33, 0x33, 0x00, 0x00, 0x00, 0x02};
	struct pmo_vdev_priv_obj *temp_ctx;
	uint8_t addr_fp;

	temp_ctx = *vdev_ctx;
	addr_fp = temp_ctx->addr_filter_pattern;
	op_list = &temp_ctx->vdev_mc_list_req;

	qdf_spin_lock_bh(&temp_ctx->pmo_vdev_lock);
	op_list->mc_cnt = ip->count;
	qdf_spin_unlock_bh(&temp_ctx->pmo_vdev_lock);

	for (i = 0; i < ip->count; i++) {
		/*
		 * Skip following addresses:
		 * 1)IPv6 router solicitation address
		 * 2)Any other address pattern if its set during
		 *  RXFILTER REMOVE driver command based on
		 *  addr_filter_pattern
		 */
		if ((!qdf_mem_cmp(ip->mc_addr[i].bytes, ipv6_rs,
			QDF_MAC_ADDR_SIZE)) ||
		   (addr_fp &&
		   (!qdf_mem_cmp(ip->mc_addr[i].bytes, &addr_fp, 1)))) {
			pmo_debug("MC/BC filtering Skip addr "QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(ip->mc_addr[i].bytes));
			qdf_spin_lock_bh(&temp_ctx->pmo_vdev_lock);
			op_list->mc_cnt--;
			qdf_spin_unlock_bh(&temp_ctx->pmo_vdev_lock);
			continue;
		}
		qdf_spin_lock_bh(&temp_ctx->pmo_vdev_lock);
		qdf_mem_zero(&op_list->mc_addr[j].bytes,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(&op_list->mc_addr[j].bytes,
			     ip->mc_addr[i].bytes, QDF_MAC_ADDR_SIZE);
		qdf_spin_unlock_bh(&temp_ctx->pmo_vdev_lock);
		pmo_debug("Index = %d, mac["QDF_MAC_ADDR_FMT"]", j,
			  QDF_MAC_ADDR_REF(op_list->mc_addr[i].bytes));
		j++;
	}
}

static QDF_STATUS pmo_core_cache_mc_addr_list_in_vdev_priv(
		struct pmo_mc_addr_list_params *mc_list_config,
		struct wlan_objmgr_vdev *vdev)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	pmo_core_fill_mc_list(&vdev_ctx, mc_list_config);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS pmo_core_flush_mc_addr_list_from_vdev_priv(
			struct wlan_objmgr_vdev *vdev)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	qdf_mem_zero(&vdev_ctx->vdev_mc_list_req,
		sizeof(vdev_ctx->vdev_mc_list_req));
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_core_enhanced_mc_filter_enable(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;

	status = pmo_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto exit_with_status;

	pmo_tgt_send_enhance_multicast_offload_req(vdev, true);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);

exit_with_status:

	return status;
}

QDF_STATUS pmo_core_enhanced_mc_filter_disable(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;

	pmo_enter();

	status = pmo_vdev_get_ref(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		goto exit_with_status;

	pmo_tgt_send_enhance_multicast_offload_req(vdev, false);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);

exit_with_status:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_set_mc_filter_req(struct wlan_objmgr_vdev *vdev,
	struct pmo_mc_addr_list *mc_list)
{
	int i;

	if (pmo_tgt_get_multiple_mc_filter_support(vdev)) {
		pmo_debug("FW supports multiple mcast filter");
		pmo_tgt_set_multiple_mc_filter_req(vdev, mc_list);
	} else {
		pmo_debug("FW does not support multiple mcast filter");
		for (i = 0; i < mc_list->mc_cnt; i++)
			pmo_tgt_set_mc_filter_req(vdev, mc_list->mc_addr[i]);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS pmo_core_clear_mc_filter_req(struct wlan_objmgr_vdev *vdev,
	struct pmo_mc_addr_list *mc_list)
{
	int i;

	if (pmo_tgt_get_multiple_mc_filter_support(vdev)) {
		pmo_debug("FW supports multiple mcast filter");
		pmo_tgt_clear_multiple_mc_filter_req(vdev, mc_list);
	} else {
		pmo_debug("FW does not support multiple mcast filter");
		for (i = 0; i < mc_list->mc_cnt; i++)
			pmo_tgt_clear_mc_filter_req(vdev, mc_list->mc_addr[i]);
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS pmo_core_do_enable_mc_addr_list(struct wlan_objmgr_vdev *vdev,
	struct pmo_vdev_priv_obj *vdev_ctx,
	struct pmo_mc_addr_list *op_mc_list_req)
{
	QDF_STATUS status;

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	if (!vdev_ctx->vdev_mc_list_req.mc_cnt) {
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
		pmo_err("mc_cnt is zero so skip to add mc list");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}
	qdf_mem_copy(op_mc_list_req, &vdev_ctx->vdev_mc_list_req,
		sizeof(*op_mc_list_req));
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	status = pmo_core_set_mc_filter_req(vdev, op_mc_list_req);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_err("cannot apply mc filter request");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->vdev_mc_list_req.is_filter_applied = true;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
out:

	return status;
}

static QDF_STATUS pmo_core_do_disable_mc_addr_list(
	struct wlan_objmgr_vdev *vdev,
	struct pmo_vdev_priv_obj *vdev_ctx,
	struct pmo_mc_addr_list *op_mc_list_req)
{
	QDF_STATUS status;

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	/* validate filter is applied before clearing in fwr */
	if (!vdev_ctx->vdev_mc_list_req.is_filter_applied) {
		qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
		pmo_debug("mc filter is not applied in fwr");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}
	qdf_mem_copy(op_mc_list_req, &vdev_ctx->vdev_mc_list_req,
		sizeof(*op_mc_list_req));
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	status = pmo_core_clear_mc_filter_req(vdev, op_mc_list_req);
	if (status != QDF_STATUS_SUCCESS) {
		pmo_debug("cannot apply mc filter request");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->vdev_mc_list_req.is_filter_applied = false;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);
out:

	return status;
}

uint8_t pmo_core_max_mc_addr_supported(struct wlan_objmgr_psoc *psoc)
{
	return PMO_MAX_MC_ADDR_LIST;
}

int pmo_core_get_mc_addr_list_count(struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct pmo_vdev_priv_obj *vdev_ctx;
	uint8_t mc_cnt;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id, WLAN_PMO_ID);
	if (!vdev) {
		pmo_err("vdev is NULL");
		return PMO_INVALID_MC_ADDR_COUNT;
	}

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	mc_cnt = vdev_ctx->vdev_mc_list_req.mc_cnt;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);

	return mc_cnt;
}

void pmo_core_set_mc_addr_list_count(struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id, uint8_t count)
{
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id, WLAN_PMO_ID);
	if (!vdev) {
		pmo_err("vdev is NULL");
		return;
	}

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	vdev_ctx->vdev_mc_list_req.mc_cnt = count;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
}

static QDF_STATUS pmo_core_mc_addr_flitering_sanity(
			struct wlan_objmgr_vdev *vdev)
{
	struct pmo_vdev_priv_obj *vdev_ctx;

	if (!vdev) {
		pmo_err("vdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_ctx = pmo_vdev_get_priv(vdev);

	/* Check if INI is enabled or not, otherwise just return */
	if (!vdev_ctx->pmo_psoc_ctx->psoc_cfg.enable_mc_list) {
		pmo_debug("user disabled mc_addr_list using INI");
		return QDF_STATUS_E_INVAL;
	}

	if (!pmo_core_is_vdev_supports_offload(vdev)) {
		pmo_debug("vdev in invalid opmode for mc addr filtering %d",
			  pmo_get_vdev_opmode(vdev));
		return QDF_STATUS_E_INVAL;
	}

	if (vdev_ctx->vdev_mc_list_req.mc_cnt > PMO_MAX_MC_ADDR_LIST) {
		pmo_debug("Passed more than max supported MC address count :%d",
			  vdev_ctx->vdev_mc_list_req.mc_cnt);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}
QDF_STATUS pmo_core_cache_mc_addr_list(
		struct pmo_mc_addr_list_params *mc_list_config)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status;

	if (!mc_list_config->psoc) {
		pmo_err("psoc is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mc_list_config->psoc,
						    mc_list_config->vdev_id,
						    WLAN_PMO_ID);
	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_core_mc_addr_flitering_sanity(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto dec_ref;

	pmo_debug("Cache mc addr list for vdev id: %d psoc: %pK",
		  mc_list_config->vdev_id, mc_list_config->psoc);

	status = pmo_core_cache_mc_addr_list_in_vdev_priv(mc_list_config, vdev);

dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:

	return status;
}

QDF_STATUS pmo_core_flush_mc_addr_list(struct wlan_objmgr_psoc *psoc,
	uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!psoc) {
		pmo_err("psoc is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id, WLAN_PMO_ID);
	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_core_mc_addr_flitering_sanity(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto dec_ref;

	pmo_debug("Flush mc addr list for vdev id: %d psoc: %pK",
		  vdev_id, psoc);

	status = pmo_core_flush_mc_addr_list_from_vdev_priv(vdev);

dec_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);
out:

	return status;
}

static QDF_STATUS pmo_core_handle_enable_mc_list_trigger(
			struct wlan_objmgr_vdev *vdev,
			enum pmo_offload_trigger trigger)
{
	struct pmo_vdev_priv_obj *vdev_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pmo_mc_addr_list *op_mc_list_req;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	op_mc_list_req = qdf_mem_malloc(sizeof(*op_mc_list_req));
	if (!op_mc_list_req) {
		status = QDF_STATUS_E_NOMEM;
		goto exit_with_status;
	}

	switch (trigger) {
	case pmo_mc_list_change_notify:
		if (!vdev_ctx->pmo_psoc_ctx->psoc_cfg.active_mode_offload) {
			pmo_debug("active offload is disabled, skip in mode %d",
				  trigger);
			goto free_req;
		}
		status = pmo_core_do_enable_mc_addr_list(vdev, vdev_ctx,
							 op_mc_list_req);
		break;
	case pmo_apps_suspend:
		if (vdev_ctx->pmo_psoc_ctx->psoc_cfg.active_mode_offload) {
			pmo_debug("active offload is enabled, skip in mode %d",
				  trigger);
			goto free_req;
		}
		status = pmo_core_do_enable_mc_addr_list(vdev, vdev_ctx,
							 op_mc_list_req);
		break;
	default:
		status = QDF_STATUS_E_INVAL;
		pmo_err("invalid pmo trigger for enable mc list");
		break;
	}

free_req:
	qdf_mem_free(op_mc_list_req);

exit_with_status:

	return status;
}

QDF_STATUS pmo_core_enable_mc_addr_filtering_in_fwr(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id,
		enum pmo_offload_trigger trigger)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	status = pmo_psoc_get_ref(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto exit_with_status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id, WLAN_PMO_ID);
	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_INVAL;
		goto put_psoc;
	}

	status = pmo_core_mc_addr_flitering_sanity(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto put_vdev;

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_INVAL;
		goto put_vdev;
	}

	pmo_debug("enable mclist trigger: %d", trigger);
	status = pmo_core_handle_enable_mc_list_trigger(vdev, trigger);

put_vdev:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);

put_psoc:
	pmo_psoc_put_ref(psoc);

exit_with_status:

	return status;
}

static QDF_STATUS pmo_core_handle_disable_mc_list_trigger(
			struct wlan_objmgr_vdev *vdev,
			enum pmo_offload_trigger trigger)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pmo_vdev_priv_obj *vdev_ctx;
	struct pmo_mc_addr_list *op_mc_list_req;

	vdev_ctx = pmo_vdev_get_priv(vdev);

	op_mc_list_req = qdf_mem_malloc(sizeof(*op_mc_list_req));
	if (!op_mc_list_req) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	switch (trigger) {
	case pmo_peer_disconnect:
	case pmo_mc_list_change_notify:
		if (!vdev_ctx->pmo_psoc_ctx->psoc_cfg.active_mode_offload) {
			pmo_debug("active offload is disabled, skip in mode %d",
				  trigger);
			goto free_req;
		}
		status = pmo_core_do_disable_mc_addr_list(vdev, vdev_ctx,
							  op_mc_list_req);
		break;
	case pmo_apps_resume:
		if (vdev_ctx->pmo_psoc_ctx->psoc_cfg.active_mode_offload) {
			pmo_debug("active offload is enabled, skip in mode %d",
				  trigger);
			goto free_req;
		}
		status = pmo_core_do_disable_mc_addr_list(vdev, vdev_ctx,
							  op_mc_list_req);
		break;
	default:
		status = QDF_STATUS_E_INVAL;
		pmo_err("invalid pmo trigger for disable mc list");
		break;
	}

free_req:
	qdf_mem_free(op_mc_list_req);

out:
	return status;
}

QDF_STATUS pmo_core_disable_mc_addr_filtering_in_fwr(
		struct wlan_objmgr_psoc *psoc,
		uint8_t vdev_id,
		enum pmo_offload_trigger trigger)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	if (!psoc) {
		pmo_err("psoc is NULL");
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id, WLAN_PMO_ID);
	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_core_mc_addr_flitering_sanity(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto put_ref;

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_INVAL;
		goto put_ref;
	}

	pmo_debug("disable mclist trigger: %d", trigger);

	status = pmo_core_handle_disable_mc_list_trigger(vdev, trigger);

put_ref:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);

out:

	return status;
}

QDF_STATUS
pmo_core_get_mc_addr_list(struct wlan_objmgr_psoc *psoc,
			  uint8_t vdev_id,
			  struct pmo_mc_addr_list *mc_list_req)
{
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;
	struct pmo_vdev_priv_obj *vdev_ctx;

	pmo_enter();

	if (!mc_list_req)
		return QDF_STATUS_E_INVAL;

	qdf_mem_zero(mc_list_req, sizeof(*mc_list_req));

	status = pmo_psoc_get_ref(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		goto exit_with_status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id, WLAN_PMO_ID);
	if (!vdev) {
		pmo_err("vdev is NULL");
		status = QDF_STATUS_E_INVAL;
		goto put_psoc;
	}

	status = pmo_core_mc_addr_flitering_sanity(vdev);
	if (status != QDF_STATUS_SUCCESS)
		goto put_vdev;

	vdev_ctx = pmo_vdev_get_priv(vdev);
	qdf_spin_lock_bh(&vdev_ctx->pmo_vdev_lock);
	*mc_list_req = vdev_ctx->vdev_mc_list_req;
	qdf_spin_unlock_bh(&vdev_ctx->pmo_vdev_lock);

put_vdev:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_PMO_ID);

put_psoc:
	pmo_psoc_put_ref(psoc);

exit_with_status:
	pmo_exit();

	return status;
}
