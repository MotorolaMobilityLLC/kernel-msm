/*
 * Copyright (c) 2016-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wma_nan_datapath.c
 *
 * WMA NAN Data path API implementation
 */

#include "wma.h"
#include "wma_api.h"
#include "wmi_unified_api.h"
#include "wmi_unified.h"
#include "wma_nan_datapath.h"
#include "wma_internal.h"
#include "cds_utils.h"
#include "cdp_txrx_peer_ops.h"
#include "cdp_txrx_tx_delay.h"
#include "cdp_txrx_misc.h"
#include <cdp_txrx_handle.h>

/**
 * wma_add_sta_ndi_mode() - Process ADD_STA for NaN Data path
 * @wma: wma handle
 * @add_sta: Parameters of ADD_STA command
 *
 * Sends CREATE_PEER command to firmware
 * Return: void
 */
void wma_add_sta_ndi_mode(tp_wma_handle wma, tpAddStaParams add_sta)
{
	enum ol_txrx_peer_state state = OL_TXRX_PEER_STATE_CONN;
	uint8_t pdev_id = WMI_PDEV_ID_SOC;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	QDF_STATUS status;
	struct wma_txrx_node *iface;

	iface = &wma->interfaces[add_sta->smesessionId];
	wma_debug("vdev: %d, peer_mac_addr: "QDF_MAC_ADDR_FMT,
		add_sta->smesessionId, QDF_MAC_ADDR_REF(add_sta->staMac));

	if (cdp_find_peer_exist_on_vdev(soc, add_sta->smesessionId,
					add_sta->staMac)) {
		wma_err("NDI peer already exists, peer_addr "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(add_sta->staMac));
		add_sta->status = QDF_STATUS_E_EXISTS;
		goto send_rsp;
	}

	/*
	 * The code above only checks the peer existence on its own vdev.
	 * Need to check whether the peer exists on other vDevs because firmware
	 * can't create the peer if the peer with same MAC address already
	 * exists on the pDev. As this peer belongs to other vDevs, just return
	 * here.
	 */
	if (cdp_find_peer_exist(soc, pdev_id, add_sta->staMac)) {
		wma_err("peer exists on other vdev with peer_addr "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(add_sta->staMac));
		add_sta->status = QDF_STATUS_E_EXISTS;
		goto send_rsp;
	}

	status = wma_create_peer(wma, add_sta->staMac,
				 WMI_PEER_TYPE_NAN_DATA, add_sta->smesessionId);
	if (status != QDF_STATUS_SUCCESS) {
		wma_err("Failed to create peer for "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(add_sta->staMac));
		add_sta->status = status;
		goto send_rsp;
	}

	if (!cdp_find_peer_exist_on_vdev(soc, add_sta->smesessionId,
					 add_sta->staMac)) {
		wma_err("Failed to find peer handle using peer mac "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(add_sta->staMac));
		add_sta->status = QDF_STATUS_E_FAILURE;
		wma_remove_peer(wma, add_sta->staMac, add_sta->smesessionId,
				false);
		goto send_rsp;
	}

	wma_debug("Moving peer "QDF_MAC_ADDR_FMT" to state %d",
		  QDF_MAC_ADDR_REF(add_sta->staMac), state);
	cdp_peer_state_update(soc, add_sta->staMac, state);

	add_sta->nss    = iface->nss;
	add_sta->status = QDF_STATUS_SUCCESS;
send_rsp:
	wma_debug("Sending add sta rsp to umac (mac:"QDF_MAC_ADDR_FMT", status:%d)",
		  QDF_MAC_ADDR_REF(add_sta->staMac), add_sta->status);
	wma_send_msg_high_priority(wma, WMA_ADD_STA_RSP, (void *)add_sta, 0);
}

/**
 * wma_delete_sta_req_ndi_mode() - Process DEL_STA request for NDI data peer
 * @wma: WMA context
 * @del_sta: DEL_STA parameters from LIM
 *
 * Removes wma/txrx peer entry for the NDI STA
 *
 * Return: None
 */
void wma_delete_sta_req_ndi_mode(tp_wma_handle wma,
					tpDeleteStaParams del_sta)
{
	wma_remove_peer(wma, del_sta->staMac,
			del_sta->smesessionId, false);
	del_sta->status = QDF_STATUS_SUCCESS;

	if (del_sta->respReqd) {
		wma_debug("Sending del rsp to umac (status: %d)",
				del_sta->status);
		wma_send_msg_high_priority(wma, WMA_DELETE_STA_RSP, del_sta, 0);
	} else {
		wma_debug("NDI Del Sta resp not needed");
		qdf_mem_free(del_sta);
	}

}
