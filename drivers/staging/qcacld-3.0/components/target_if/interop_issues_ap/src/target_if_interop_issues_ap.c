/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: target_if_interop_issues_ap.c
 *
 * This file provide definition for APIs registered through lmac Tx Ops
 */

#include <qdf_mem.h>
#include <qdf_status.h>
#include <qdf_types.h>
#include <target_if.h>
#include <wlan_tgt_def_config.h>
#include <wlan_osif_priv.h>
#include <wlan_interop_issues_ap_tgt_api.h>
#include <wlan_interop_issues_ap_api.h>
#include <target_if_interop_issues_ap.h>
#include <wmi_unified_interop_issues_ap_api.h>

/**
 * target_if_interop_issues_ap_event_handler() - callback for event
 * @event: firmware event
 * @len: the event length
 *
 * Return: 0 or error status
 */
static int target_if_interop_issues_ap_event_handler(ol_scn_t sc,
						     uint8_t *event,
						     uint32_t len)
{
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct wlan_interop_issues_ap_event data = {0};
	int ret;

	TARGET_IF_ENTER();

	psoc = target_if_get_psoc_from_scn_hdl(sc);
	if (!psoc) {
		target_if_err("psoc ptr is NULL");
		return -EINVAL;
	}
	data.psoc = psoc;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null.");
		return -EINVAL;
	}

	ret = wmi_extract_interop_issues_ap_ev_param(wmi_handle, event, &data);
	if (ret)
		return -EINVAL;

	target_if_debug("interop issues ap macaddr: " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(data.rap_addr.bytes));

	return tgt_interop_issues_ap_info_callback(psoc, &data);
}

/**
 * target_if_interop_issues_ap_register_event_handler() - register callback
 * @psoc: the pointer to psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_interop_issues_ap_register_event_handler(
						struct wlan_objmgr_psoc *psoc)
{
	int ret_val;
	struct wmi_unified *wmi_handle;

	if (!psoc) {
		target_if_err("PSOC is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return QDF_STATUS_E_INVAL;
	}
	ret_val =
	   wmi_unified_register_event_handler(wmi_handle,
				wmi_pdev_interop_issues_ap_event_id,
				target_if_interop_issues_ap_event_handler,
				WMI_RX_WORK_CTX);
	if (ret_val)
		target_if_err("Failed to register event cb");

	return qdf_status_from_os_return(ret_val);
}

/**
 * target_if_interop_issues_ap_unregister_event_handler() - unregister callback
 * @psoc: the pointer to psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_interop_issues_ap_unregister_event_handler(
						struct wlan_objmgr_psoc *psoc)
{
	struct wmi_unified *wmi_handle;

	if (!psoc) {
		target_if_err("PSOC is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null");
		return QDF_STATUS_E_INVAL;
	}
	wmi_unified_unregister_event_handler(wmi_handle,
					wmi_pdev_interop_issues_ap_event_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_set_interop_issues_ap_req() - API to send stats request to wmi
 * @psoc: pointer to psoc object
 * @raq: pointer to interop issues ap info
 *
 * Return: status of operation.
 */
static QDF_STATUS
target_if_set_interop_issues_ap_req(struct wlan_objmgr_psoc *psoc,
				    struct wlan_interop_issues_ap_info *rap)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("wmi_handle is null.");
		return QDF_STATUS_E_NULL_VALUE;
	}

	return wmi_unified_set_rap_ps_cmd(wmi_handle, rap);
}

QDF_STATUS
target_if_interop_issues_ap_register_tx_ops(struct wlan_objmgr_psoc *psoc,
				  struct wlan_interop_issues_ap_tx_ops *tx_ops)
{
	if (!tx_ops) {
		target_if_err("tx ops is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	tx_ops->set_rap_ps = target_if_set_interop_issues_ap_req;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
target_if_interop_issues_ap_unregister_tx_ops(struct wlan_objmgr_psoc *psoc,
				  struct wlan_interop_issues_ap_tx_ops *tx_ops)
{
	if (!tx_ops) {
		target_if_err("tx ops is NULL!");
		return QDF_STATUS_E_INVAL;
	}

	tx_ops->set_rap_ps = NULL;

	return QDF_STATUS_SUCCESS;
}
