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
 * DOC: Implements public API for pmo low power hear beat feature
 * to interact with target/WMI.
 */

#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_lphb_public_struct.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"
#include "wlan_pmo_main.h"

QDF_STATUS pmo_tgt_send_lphb_enable(struct wlan_objmgr_psoc *psoc,
		struct pmo_lphb_enable_req *ts_lphb_enable)
{
	QDF_STATUS status;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();
	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_lphb_enable) {
		pmo_err("send_lphb_enable is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}
	status = pmo_tx_ops.send_lphb_enable(psoc, ts_lphb_enable);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to send lphb enable");
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_tgt_send_lphb_tcp_params(struct wlan_objmgr_psoc *psoc,
		struct pmo_lphb_tcp_params *ts_lphb_tcp_param)
{
	QDF_STATUS status;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();
	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_lphb_tcp_params) {
		pmo_err("send_lphb_tcp_params is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}
	status = pmo_tx_ops.send_lphb_tcp_params(psoc, ts_lphb_tcp_param);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to send lphb tcp params");
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_tgt_send_lphb_tcp_pkt_filter(struct wlan_objmgr_psoc *psoc,
		struct pmo_lphb_tcp_filter_req *ts_lphb_tcp_filter)
{
	QDF_STATUS status;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();
	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_lphb_tcp_filter_req) {
		pmo_err("send_lphb_tcp_filter_req is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}
	status = pmo_tx_ops.send_lphb_tcp_filter_req(psoc, ts_lphb_tcp_filter);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to send lphb tcp filter req");
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_tgt_send_lphb_udp_params(struct wlan_objmgr_psoc *psoc,
		struct pmo_lphb_udp_params *ts_lphb_udp_param)
{
	QDF_STATUS status;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();
	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_lphb_upd_params) {
		pmo_err("send_lphb_upd_params is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}
	status = pmo_tx_ops.send_lphb_upd_params(psoc, ts_lphb_udp_param);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to send lphb udp param");
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_tgt_send_lphb_udp_pkt_filter(struct wlan_objmgr_psoc *psoc,
		struct pmo_lphb_udp_filter_req *ts_lphb_udp_filter)
{
	QDF_STATUS status;
	struct wlan_pmo_tx_ops pmo_tx_ops;

	pmo_enter();
	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_lphb_udp_filter_req) {
		pmo_err("send_lphb_udp_filter_req is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}
	status = pmo_tx_ops.send_lphb_udp_filter_req(psoc, ts_lphb_udp_filter);
	if (status != QDF_STATUS_SUCCESS)
		pmo_err("Failed to send lphb udp filter req");
out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_tgt_lphb_rsp_evt(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_rsp *rsp_param)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	psoc_ctx = pmo_psoc_get_priv(psoc);
	if (psoc_ctx->wow.lphb_cb && psoc_ctx->wow.lphb_cb_ctx) {
		psoc_ctx->wow.lphb_cb(psoc_ctx->wow.lphb_cb_ctx, rsp_param);
	} else {
		pmo_err("lphb rsp callback/context is null for psoc %pK",
			psoc);
	}

	return QDF_STATUS_SUCCESS;
}

