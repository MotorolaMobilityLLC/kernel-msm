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
 * DOC: offload lmac interface APIs for tdls
 *
 */

#include <qdf_mem.h>
#include <target_if.h>
#include <qdf_status.h>
#include <wmi_unified_api.h>
#include <wmi_unified_priv.h>
#include <wmi_unified_param.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_tdls_tgt_api.h>
#include <target_if_tdls.h>
#include <cdp_txrx_peer_ops.h>
#include <wlan_utility.h>

static inline struct wlan_lmac_if_tdls_rx_ops *
target_if_tdls_get_rx_ops(struct wlan_objmgr_psoc *psoc)
{
	return &psoc->soc_cb.rx_ops->tdls_rx_ops;
}

static int
target_if_tdls_event_handler(ol_scn_t scn, uint8_t *data, uint32_t datalen)
{
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct wlan_lmac_if_tdls_rx_ops *tdls_rx_ops;
	struct tdls_event_info info;
	QDF_STATUS status;

	if (!scn || !data) {
		target_if_err("scn: 0x%pK, data: 0x%pK", scn, data);
		return -EINVAL;
	}
	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("null psoc");
		return -EINVAL;
	}
	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		target_if_err("null wmi_handle");
		return -EINVAL;
	}

	if (wmi_extract_vdev_tdls_ev_param(wmi_handle, data, &info)) {
		target_if_err("Failed to extract wmi tdls event");
		return -EINVAL;
	}

	tdls_rx_ops = target_if_tdls_get_rx_ops(psoc);
	if (tdls_rx_ops && tdls_rx_ops->tdls_ev_handler) {
		status = tdls_rx_ops->tdls_ev_handler(psoc, &info);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("fail to handle tdls event");
			return -EINVAL;
		}
	}

	return 0;
}

QDF_STATUS
target_if_tdls_update_fw_state(struct wlan_objmgr_psoc *psoc,
			       struct tdls_info *param)
{
	QDF_STATUS status;
	enum wmi_tdls_state tdls_state;
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid WMI handle");
		return QDF_STATUS_E_FAILURE;
	}

	if (TDLS_SUPPORT_EXP_TRIG_ONLY == param->tdls_state)
		tdls_state = WMI_TDLS_ENABLE_PASSIVE;
	else if (TDLS_SUPPORT_IMP_MODE == param->tdls_state ||
		 TDLS_SUPPORT_EXT_CONTROL == param->tdls_state)
		tdls_state = WMI_TDLS_ENABLE_CONNECTION_TRACKER_IN_HOST;
	else
		tdls_state = WMI_TDLS_DISABLE;

	status = wmi_unified_update_fw_tdls_state_cmd(wmi_handle,
						      param, tdls_state);

	target_if_debug("vdev_id %d", param->vdev_id);
	return status;
}

QDF_STATUS
target_if_tdls_update_peer_state(struct wlan_objmgr_psoc *psoc,
				 struct tdls_peer_update_state *peer_params)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
target_if_tdls_set_offchan_mode(struct wlan_objmgr_psoc *psoc,
				struct tdls_channel_switch_params *params)
{
	QDF_STATUS status;
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid WMI handle");
		return QDF_STATUS_E_FAILURE;
	}
	status = wmi_unified_set_tdls_offchan_mode_cmd(wmi_handle,
						       params);

	return status;
}

QDF_STATUS
target_if_tdls_register_event_handler(struct wlan_objmgr_psoc *psoc,
				      void *arg)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("null wmi_handle");
		return QDF_STATUS_E_INVAL;
	}
	return wmi_unified_register_event(wmi_handle,
					  wmi_tdls_peer_event_id,
					  target_if_tdls_event_handler);
}

QDF_STATUS
target_if_tdls_unregister_event_handler(struct wlan_objmgr_psoc *psoc,
					void *arg)
{
	struct wmi_unified *wmi_handle;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("null wmi_handle");
		return QDF_STATUS_E_INVAL;
	}
	return wmi_unified_unregister_event(wmi_handle,
					    wmi_tdls_peer_event_id);
}

QDF_STATUS
target_if_tdls_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_tdls_tx_ops *tdls_txops;

	tdls_txops = &tx_ops->tdls_tx_ops;

	tdls_txops->update_fw_state = target_if_tdls_update_fw_state;
	tdls_txops->update_peer_state = target_if_tdls_update_peer_state;
	tdls_txops->set_offchan_mode = target_if_tdls_set_offchan_mode;
	tdls_txops->tdls_reg_ev_handler = target_if_tdls_register_event_handler;
	tdls_txops->tdls_unreg_ev_handler =
		target_if_tdls_unregister_event_handler;

	return QDF_STATUS_SUCCESS;
}
