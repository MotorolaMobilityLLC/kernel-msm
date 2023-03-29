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
 * DOC: Implements public API for pmo to interact with target/WMI
 */

#include "wlan_pmo_tgt_api.h"
#include "wlan_pmo_pkt_filter_public_struct.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"
#include "wlan_pmo_main.h"

QDF_STATUS pmo_tgt_set_pkt_filter(struct wlan_objmgr_vdev *vdev,
		struct pmo_rcv_pkt_fltr_cfg *pmo_set_pkt_fltr_req,
		uint8_t vdev_id)
{
	QDF_STATUS status;
	struct pmo_rcv_pkt_fltr_cfg *request_buf = NULL;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;
	struct qdf_mac_addr peer_bssid;

	pmo_enter();

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		pmo_err("psoc unavailable for vdev %pK", vdev);
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	pmo_debug("filter_type=%d, filter_id = %d",
		  pmo_set_pkt_fltr_req->filter_type,
		  pmo_set_pkt_fltr_req->filter_id);

	request_buf = qdf_mem_malloc(sizeof(*request_buf));
	if (!request_buf) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	status = pmo_get_vdev_bss_peer_mac_addr(vdev,
			&peer_bssid);
	if (status != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	qdf_mem_copy(request_buf, pmo_set_pkt_fltr_req, sizeof(*request_buf));

	qdf_mem_copy(&request_buf->self_macaddr.bytes,
			  wlan_vdev_mlme_get_macaddr(vdev),
			  QDF_MAC_ADDR_SIZE);

	qdf_copy_macaddr(&pmo_set_pkt_fltr_req->bssid,
				&peer_bssid);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_set_pkt_filter) {
		pmo_err("send_set_pkt_filter is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_tx_ops.send_set_pkt_filter(vdev, request_buf);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

out:
	if (request_buf)
		qdf_mem_free(request_buf);
	pmo_exit();

	return status;
}

QDF_STATUS pmo_tgt_clear_pkt_filter(struct wlan_objmgr_vdev *vdev,
		struct pmo_rcv_pkt_fltr_clear_param *pmo_clr_pkt_fltr_param,
		uint8_t vdev_id)
{
	QDF_STATUS status;
	struct pmo_rcv_pkt_fltr_clear_param *request_buf = NULL;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_pmo_tx_ops pmo_tx_ops;
	struct qdf_mac_addr peer_bssid;

	pmo_enter();

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		pmo_err("psoc unavailable for vdev %pK", vdev);
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	pmo_debug("filter_id = %d", pmo_clr_pkt_fltr_param->filter_id);

	request_buf = qdf_mem_malloc(sizeof(*request_buf));
	if (!request_buf) {
		status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	status = pmo_get_vdev_bss_peer_mac_addr(vdev,
			&peer_bssid);
	if (status != QDF_STATUS_SUCCESS) {
		status = QDF_STATUS_E_INVAL;
		goto out;
	}

	qdf_mem_copy(request_buf, pmo_clr_pkt_fltr_param, sizeof(*request_buf));

	qdf_mem_copy(&request_buf->self_macaddr.bytes,
			  wlan_vdev_mlme_get_macaddr(vdev),
			  QDF_MAC_ADDR_SIZE);

	qdf_copy_macaddr(&pmo_clr_pkt_fltr_param->bssid,
			 &peer_bssid);

	pmo_tx_ops = GET_PMO_TX_OPS_FROM_PSOC(psoc);
	if (!pmo_tx_ops.send_clear_pkt_filter) {
		pmo_err("send_clear_pkt_filter is null");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	status = pmo_tx_ops.send_clear_pkt_filter(vdev, request_buf);
	if (status != QDF_STATUS_SUCCESS)
		goto out;

out:
	if (request_buf)
		qdf_mem_free(request_buf);
	pmo_exit();

	return status;
}

