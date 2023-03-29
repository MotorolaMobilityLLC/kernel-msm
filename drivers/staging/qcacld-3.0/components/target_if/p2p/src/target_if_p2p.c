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
 * DOC: offload lmac interface APIs definitions for P2P
 */

#include <wmi_unified_api.h>
#include <wlan_p2p_public_struct.h>
#include "target_if.h"
#include "target_if_p2p.h"
#include "init_deinit_lmac.h"

static inline struct wlan_lmac_if_p2p_rx_ops *
target_if_psoc_get_p2p_rx_ops(struct wlan_objmgr_psoc *psoc)
{
	return &(psoc->soc_cb.rx_ops->p2p);
}

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
static inline void
target_if_p2p_lo_register_tx_ops(struct wlan_lmac_if_p2p_tx_ops *p2p_tx_ops)
{
	p2p_tx_ops->lo_start = target_if_p2p_lo_start;
	p2p_tx_ops->lo_stop = target_if_p2p_lo_stop;
	p2p_tx_ops->reg_lo_ev_handler =
			target_if_p2p_register_lo_event_handler;
	p2p_tx_ops->unreg_lo_ev_handler =
			target_if_p2p_unregister_lo_event_handler;
}

/**
 * target_p2p_lo_event_handler() - WMI callback for lo stop event
 * @scn:       pointer to scn
 * @event_buf: event buffer
 * @len:       buffer length
 *
 * This function gets called from WMI when triggered wmi event
 * wmi_p2p_lo_stop_event_id.
 *
 * Return: 0 - success
 * others - failure
 */
static int target_p2p_lo_event_handler(ol_scn_t scn, uint8_t *data,
	uint32_t datalen)
{
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct p2p_lo_event *event_info;
	struct wlan_lmac_if_p2p_rx_ops *p2p_rx_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	target_if_debug("scn:%pK, data:%pK, datalen:%d", scn, data, datalen);

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
		target_if_err("null wmi handle");
		return -EINVAL;
	}

	event_info = qdf_mem_malloc(sizeof(*event_info));
	if (!event_info)
		return -ENOMEM;

	if (wmi_extract_p2p_lo_stop_ev_param(wmi_handle, data,
			event_info)) {
		target_if_err("Failed to extract wmi p2p lo stop event");
		qdf_mem_free(event_info);
		return -EINVAL;
	}

	p2p_rx_ops = target_if_psoc_get_p2p_rx_ops(psoc);
	if (p2p_rx_ops->lo_ev_handler) {
		status = p2p_rx_ops->lo_ev_handler(psoc, event_info);
		target_if_debug("call lo event handler, status:%d",
			status);
	} else {
		qdf_mem_free(event_info);
		target_if_debug("no valid lo event handler");
	}

	return qdf_status_to_os_return(status);
}

QDF_STATUS target_if_p2p_register_lo_event_handler(
	struct wlan_objmgr_psoc *psoc, void *arg)
{
	int status;
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);

	target_if_debug("psoc:%pK, arg:%pK", psoc, arg);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_register_event(wmi_handle,
					    wmi_p2p_lo_stop_event_id,
					    target_p2p_lo_event_handler);

	target_if_debug("wmi register lo event handle, status:%d", status);

	return status == 0 ? QDF_STATUS_SUCCESS : QDF_STATUS_E_FAILURE;
}

QDF_STATUS target_if_p2p_unregister_lo_event_handler(
	struct wlan_objmgr_psoc *psoc, void *arg)
{
	int status;
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);

	target_if_debug("psoc:%pK, arg:%pK", psoc, arg);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_unregister_event(wmi_handle,
					      wmi_p2p_lo_stop_event_id);

	target_if_debug("wmi unregister lo event handle, status:%d", status);

	return status == 0 ? QDF_STATUS_SUCCESS : QDF_STATUS_E_FAILURE;
}

QDF_STATUS target_if_p2p_lo_start(struct wlan_objmgr_psoc *psoc,
				  struct p2p_lo_start *lo_start)
{
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	if (!lo_start) {
		target_if_err("lo start parameters is null");
		return QDF_STATUS_E_INVAL;
	}
	target_if_debug("psoc:%pK, vdev_id:%d", psoc, lo_start->vdev_id);

	return wmi_unified_p2p_lo_start_cmd(wmi_handle, lo_start);
}

QDF_STATUS target_if_p2p_lo_stop(struct wlan_objmgr_psoc *psoc,
				 uint32_t vdev_id)
{
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);

	target_if_debug("psoc:%pK, vdev_id:%d", psoc, vdev_id);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_p2p_lo_stop_cmd(wmi_handle,
			(uint8_t)vdev_id);
}
#else
static inline void
target_if_p2p_lo_register_tx_ops(struct wlan_lmac_if_p2p_tx_ops *p2p_tx_ops)
{
}
#endif /* FEATURE_P2P_LISTEN_OFFLOAD */

/**
 * target_p2p_noa_event_handler() - WMI callback for noa event
 * @scn:       pointer to scn
 * @event_buf: event buffer
 * @len:       buffer length
 *
 * This function gets called from WMI when triggered WMI event
 * wmi_p2p_noa_event_id.
 *
 * Return: 0 - success
 * others - failure
 */
static int target_p2p_noa_event_handler(ol_scn_t scn, uint8_t *data,
	uint32_t datalen)
{
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct p2p_noa_info *event_info;
	struct wlan_lmac_if_p2p_rx_ops *p2p_rx_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	target_if_debug("scn:%pK, data:%pK, datalen:%d", scn, data, datalen);

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
		target_if_err("null wmi handle");
		return -EINVAL;
	}

	event_info = qdf_mem_malloc(sizeof(*event_info));
	if (!event_info)
		return -ENOMEM;

	if (wmi_extract_p2p_noa_ev_param(wmi_handle, data,
			event_info)) {
		target_if_err("failed to extract wmi p2p noa event");
		qdf_mem_free(event_info);
		return -EINVAL;
	}

	p2p_rx_ops = target_if_psoc_get_p2p_rx_ops(psoc);
	if (p2p_rx_ops->noa_ev_handler) {
		status = p2p_rx_ops->noa_ev_handler(psoc, event_info);
		target_if_debug("call noa event handler, status:%d",
			status);
	} else {
		qdf_mem_free(event_info);
		target_if_debug("no valid noa event handler");
	}

	return qdf_status_to_os_return(status);
}

QDF_STATUS target_if_p2p_register_noa_event_handler(
	struct wlan_objmgr_psoc *psoc, void *arg)
{
	int status;
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);

	target_if_debug("psoc:%pK, arg:%pK", psoc, arg);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_register_event(wmi_handle,
			wmi_p2p_noa_event_id,
			target_p2p_noa_event_handler);

	target_if_debug("wmi register noa event handle, status:%d",
		status);

	return status == 0 ? QDF_STATUS_SUCCESS : QDF_STATUS_E_FAILURE;
}

QDF_STATUS target_if_p2p_unregister_noa_event_handler(
	struct wlan_objmgr_psoc *psoc, void *arg)
{
	int status;
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);

	target_if_debug("psoc:%pK, arg:%pK", psoc, arg);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_unregister_event(wmi_handle,
			wmi_p2p_noa_event_id);

	target_if_debug("wmi unregister noa event handle, status:%d",
		status);

	return status == 0 ? QDF_STATUS_SUCCESS : QDF_STATUS_E_FAILURE;
}

QDF_STATUS target_if_p2p_set_ps(struct wlan_objmgr_psoc *psoc,
	struct p2p_ps_config *ps_config)
{
	struct p2p_ps_params cmd;
	QDF_STATUS status;
	 wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	if (!ps_config) {
		target_if_err("ps config parameters is null");
		return QDF_STATUS_E_INVAL;
	}

	target_if_debug("psoc:%pK, vdev_id:%d, opp_ps:%d", psoc,
			ps_config->vdev_id, ps_config->opp_ps);

	cmd.opp_ps = ps_config->opp_ps;
	cmd.ctwindow = ps_config->ct_window;
	cmd.count = ps_config->count;
	cmd.duration = ps_config->duration;
	cmd.interval = ps_config->interval;
	cmd.single_noa_duration = ps_config->single_noa_duration;
	cmd.ps_selection = ps_config->ps_selection;
	cmd.session_id =  ps_config->vdev_id;

	if (ps_config->opp_ps)
		status = wmi_unified_set_p2pgo_oppps_req(wmi_handle,
				   &cmd);
	else
		status = wmi_unified_set_p2pgo_noa_req_cmd(wmi_handle,
				   &cmd);

	if (status != QDF_STATUS_SUCCESS)
		target_if_err("Failed to send set uapsd param, %d",
				status);

	return status;
}

QDF_STATUS target_if_p2p_set_noa(struct wlan_objmgr_psoc *psoc,
	uint32_t vdev_id, bool disable_noa)
{
	struct vdev_set_params param;
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	target_if_debug("psoc:%pK, vdev_id:%d disable_noa:%d",
				psoc, vdev_id, disable_noa);
	param.vdev_id = vdev_id;
	param.param_id = WMI_VDEV_PARAM_DISABLE_NOA_P2P_GO;
	param.param_value = (uint32_t)disable_noa;

	return wmi_unified_vdev_set_param_send(wmi_handle, &param);
}

static int target_p2p_mac_rx_filter_event_handler(ol_scn_t scn, uint8_t *data,
						  uint32_t datalen)
{
	struct wlan_objmgr_psoc *psoc;
	struct wmi_unified *wmi_handle;
	struct p2p_set_mac_filter_evt event_info;
	struct wlan_lmac_if_p2p_rx_ops *p2p_rx_ops;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

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
		target_if_err("null wmi handle");
		return -EINVAL;
	}

	if (wmi_extract_mac_addr_rx_filter_evt_param(wmi_handle, data,
						     &event_info)) {
		target_if_err("failed to extract wmi p2p noa event");
		return -EINVAL;
	}
	target_if_debug("vdev_id %d status %d", event_info.vdev_id,
			event_info.status);
	p2p_rx_ops = target_if_psoc_get_p2p_rx_ops(psoc);
	if (p2p_rx_ops && p2p_rx_ops->add_mac_addr_filter_evt_handler)
		status = p2p_rx_ops->add_mac_addr_filter_evt_handler(
					psoc, &event_info);
	else
		target_if_debug("no add mac addr filter event handler");

	return qdf_status_to_os_return(status);
}

static QDF_STATUS target_if_p2p_register_macaddr_rx_filter_evt_handler(
		struct wlan_objmgr_psoc *psoc, bool reg)
{
	int status;
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);

	target_if_debug("psoc:%pK, register %d mac addr rx evt", psoc, reg);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}
	if (reg)
		status = wmi_unified_register_event(
				wmi_handle,
				wmi_vdev_add_macaddr_rx_filter_event_id,
				target_p2p_mac_rx_filter_event_handler);
	else
		status = wmi_unified_unregister_event(
				wmi_handle,
				wmi_vdev_add_macaddr_rx_filter_event_id);

	return status == 0 ? QDF_STATUS_SUCCESS : QDF_STATUS_E_FAILURE;
}

static QDF_STATUS target_if_p2p_set_mac_addr_rx_filter_cmd(
	struct wlan_objmgr_psoc *psoc, struct p2p_set_mac_filter *param)
{
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_send_set_mac_addr_rx_filter_cmd(wmi_handle, param);
}

void target_if_p2p_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
	struct wlan_lmac_if_p2p_tx_ops *p2p_tx_ops;

	if (!tx_ops) {
		target_if_err("lmac tx_ops is null");
		return;
	}

	p2p_tx_ops = &tx_ops->p2p;
	p2p_tx_ops->set_ps = target_if_p2p_set_ps;
	p2p_tx_ops->set_noa = target_if_p2p_set_noa;
	p2p_tx_ops->reg_noa_ev_handler =
			target_if_p2p_register_noa_event_handler;
	p2p_tx_ops->unreg_noa_ev_handler =
			target_if_p2p_unregister_noa_event_handler;
	p2p_tx_ops->reg_mac_addr_rx_filter_handler =
		target_if_p2p_register_macaddr_rx_filter_evt_handler;
	p2p_tx_ops->set_mac_addr_rx_filter_cmd =
		target_if_p2p_set_mac_addr_rx_filter_cmd;
	/* register P2P listen offload callbacks */
	target_if_p2p_lo_register_tx_ops(p2p_tx_ops);
}
