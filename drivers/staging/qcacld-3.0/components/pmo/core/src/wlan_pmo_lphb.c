/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC: Implements low power heart beat offload feature API's
 */

#include "wlan_pmo_main.h"
#include "wlan_pmo_lphb.h"
#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"

#ifdef FEATURE_WLAN_LPHB
/**
 * pmo_core_send_lphb_enable() - enable command of LPHB configuration requests
 * @psoc: objmgr psoc handle
 * @psoc_ctx: pmo private psoc ctx
 * @lphb_conf_req: lphb request which need s to configure in fwr
 * @by_user: whether this call is from user or cached resent
 *
 * Return: QDF status
 */
static QDF_STATUS pmo_core_send_lphb_enable(struct wlan_objmgr_psoc *psoc,
			struct pmo_psoc_priv_obj *psoc_ctx,
			struct pmo_lphb_req *lphb_conf_req, bool by_user)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct pmo_lphb_enable_req *ts_lphb_enable;
	int i;

	if (!lphb_conf_req) {
		pmo_err("LPHB configuration is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ts_lphb_enable = &(lphb_conf_req->params.lphb_enable_req);
	qdf_status = pmo_tgt_send_lphb_enable(psoc, ts_lphb_enable);
	if (qdf_status != QDF_STATUS_SUCCESS)
		goto out;

	/* No need to cache non user request */
	if (!by_user) {
		qdf_status = QDF_STATUS_SUCCESS;
		goto out;
	}

	/* target already configured, now cache command status */
	if (ts_lphb_enable->enable && ts_lphb_enable->item > 0) {
		i = ts_lphb_enable->item - 1;
		qdf_spin_lock_bh(&psoc_ctx->lock);
		psoc_ctx->wow.lphb_cache[i].cmd
			= pmo_lphb_set_en_param_indid;
		psoc_ctx->wow.lphb_cache[i].params.lphb_enable_req.enable =
			ts_lphb_enable->enable;
		psoc_ctx->wow.lphb_cache[i].params.lphb_enable_req.item =
			ts_lphb_enable->item;
		psoc_ctx->wow.lphb_cache[i].params.lphb_enable_req.session =
			ts_lphb_enable->session;
		qdf_spin_unlock_bh(&psoc_ctx->lock);
		pmo_debug("cached LPHB status in WMA context for item %d", i);
	} else {
		qdf_spin_lock_bh(&psoc_ctx->lock);
		qdf_mem_zero((void *)&psoc_ctx->wow.lphb_cache,
				sizeof(psoc_ctx->wow.lphb_cache));
		qdf_spin_unlock_bh(&psoc_ctx->lock);
		pmo_debug("cleared all cached LPHB status in WMA context");
	}

out:
	return qdf_status;
}

/**
 * pmo_core_send_lphb_tcp_params() - Send tcp params of LPHB requests
 * @psoc: objmgr psoc handle
 * @lphb_conf_req: lphb request which needs to be configured in fwr
 *
 * Return: QDF status
 */
static
QDF_STATUS pmo_core_send_lphb_tcp_params(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_req *lphb_conf_req)
{
	return pmo_tgt_send_lphb_tcp_params(psoc,
			&lphb_conf_req->params.lphb_tcp_params);

}

/**
 * pmo_core_send_lphb_tcp_pkt_filter() - Send tcp packet filter command of LPHB
 * @psoc: objmgr psoc handle
 * @lphb_conf_req: lphb request which needs to be configured in fwr
 *
 * Return: QDF status
 */
static
QDF_STATUS pmo_core_send_lphb_tcp_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_req *lphb_conf_req)
{
	return pmo_tgt_send_lphb_tcp_pkt_filter(psoc,
			&lphb_conf_req->params.lphb_tcp_filter_req);
}

/**
 * pmo_core_send_lphb_udp_params() - Send udp param command of LPHB
 * @psoc: objmgr psoc handle
 * @lphb_conf_req: lphb request which needs to be configured in fwr
 *
 * Return: QDF status
 */
static
QDF_STATUS pmo_core_send_lphb_udp_params(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_req *lphb_conf_req)
{
	return pmo_tgt_send_lphb_udp_params(psoc,
			&lphb_conf_req->params.lphb_udp_params);
}

/**
 * pmo_core_send_lphb_udp_pkt_filter() - Send udp pkt filter command of LPHB
 * @psoc: objmgr psoc handle
 * @lphb_conf_req: lphb request which need s to configure in fwr
 *
 * Return: QDF status
 */
static
QDF_STATUS pmo_core_send_lphb_udp_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_req *lphb_conf_req)
{
	return pmo_tgt_send_lphb_udp_pkt_filter(psoc,
			&lphb_conf_req->params.lphb_udp_filter_req);
}

/**
 * pmo_process_lphb_conf_req() - handle LPHB configuration requests
 * @psoc: objmgr psoc handle
 * @psoc_ctx: pmo private psoc ctx
 * @lphb_conf_req: lphb request which needs to be configured in fwr
 *
 * Return: QDF status
 */
static QDF_STATUS pmo_process_lphb_conf_req(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_priv_obj *psoc_ctx,
		struct pmo_lphb_req *lphb_conf_req)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_debug("LPHB configuration cmd id is %d", lphb_conf_req->cmd);
	switch (lphb_conf_req->cmd) {
	case pmo_lphb_set_en_param_indid:
		status = pmo_core_send_lphb_enable(psoc, psoc_ctx,
					lphb_conf_req, true);
		break;

	case pmo_lphb_set_tcp_pararm_indid:
		status = pmo_core_send_lphb_tcp_params(psoc, lphb_conf_req);
		break;

	case pmo_lphb_set_tcp_pkt_filter_indid:
		status = pmo_core_send_lphb_tcp_pkt_filter(psoc, lphb_conf_req);
		break;

	case pmo_lphb_set_udp_pararm_indid:
		status = pmo_core_send_lphb_udp_params(psoc, lphb_conf_req);
		break;

	case pmo_lphb_set_udp_pkt_filter_indid:
		status = pmo_core_send_lphb_udp_pkt_filter(psoc, lphb_conf_req);
		break;

	case pmo_lphb_set_network_info_indid:
	default:
		break;
	}

	return status;
}

void pmo_core_apply_lphb(struct wlan_objmgr_psoc *psoc)
{
	int i;
	struct pmo_psoc_priv_obj *psoc_ctx;

	psoc_ctx = pmo_psoc_get_priv(psoc);

	pmo_debug("checking LPHB cache");
	for (i = 0; i < 2; i++) {
		if (psoc_ctx->wow.lphb_cache[i].params.lphb_enable_req.enable) {
			pmo_debug("LPHB cache for item %d is marked as enable",
				i + 1);
			pmo_core_send_lphb_enable(psoc, psoc_ctx,
				&(psoc_ctx->wow.lphb_cache[i]), false);
		}
	}
}

QDF_STATUS pmo_core_lphb_config_req(struct wlan_objmgr_psoc *psoc,
		struct pmo_lphb_req *lphb_req, void *lphb_cb_ctx,
		pmo_lphb_callback callback)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	if (!lphb_req) {
		pmo_err("LPHB configuration is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	psoc_ctx = pmo_psoc_get_priv(psoc);

	if (pmo_lphb_set_en_param_indid == lphb_req->cmd) {
		if (!lphb_cb_ctx) {
			pmo_err("lphb callback context is null");
			return QDF_STATUS_E_NULL_VALUE;
		}
		if (!callback) {
			pmo_err("lphb callback function is null");
			return QDF_STATUS_E_NULL_VALUE;
		}
		qdf_spin_lock_bh(&psoc_ctx->lock);
		psoc_ctx->wow.lphb_cb_ctx = lphb_cb_ctx;
		psoc_ctx->wow.lphb_cb = callback;
		qdf_spin_unlock_bh(&psoc_ctx->lock);
	}

	return pmo_process_lphb_conf_req(psoc, psoc_ctx, lphb_req);
}

#endif /* FEATURE_WLAN_LPHB */

