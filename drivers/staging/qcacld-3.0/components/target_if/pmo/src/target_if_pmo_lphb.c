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
 * DOC: target_if_pmo_lphb.c
 *
 * Target interface file for pmo component to
 * send lphb offload related cmd and process event.
 */
#ifdef FEATURE_WLAN_LPHB

#include "target_if.h"
#include "target_if_pmo.h"
#include "wmi_unified_pmo_api.h"

QDF_STATUS target_if_pmo_send_lphb_enable(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_enable_req *ts_lphb_enable)
{
	wmi_hb_set_enable_cmd_fixed_param hb_enable_fp;
	wmi_unified_t wmi_handle;

	if (!ts_lphb_enable) {
		target_if_err("LPHB Enable configuration is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	target_if_info("PMO_HB_SET_ENABLE enable=%d, item=%d, session=%d",
		ts_lphb_enable->enable,
		ts_lphb_enable->item, ts_lphb_enable->session);

	if ((ts_lphb_enable->item != 1) && (ts_lphb_enable->item != 2)) {
		target_if_err("LPHB configuration wrong item %d",
			ts_lphb_enable->item);
		return QDF_STATUS_E_FAILURE;
	}

	/* fill in values */
	hb_enable_fp.vdev_id = ts_lphb_enable->session;
	hb_enable_fp.enable = ts_lphb_enable->enable;
	hb_enable_fp.item = ts_lphb_enable->item;
	hb_enable_fp.session = ts_lphb_enable->session;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_lphb_config_hbenable_cmd(wmi_handle, &hb_enable_fp);
}

QDF_STATUS target_if_pmo_send_lphb_tcp_params(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_tcp_params *ts_lphb_tcp_param)
{
	wmi_hb_set_tcp_params_cmd_fixed_param hb_tcp_params_fp = {0};
	wmi_unified_t wmi_handle;

	if (!ts_lphb_tcp_param) {
		target_if_err("TCP params LPHB configuration is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	target_if_info("PMO --> WMI_HB_SET_TCP_PARAMS srv_ip=%08x, "
		"dev_ip=%08x, src_port=%d, dst_port=%d, timeout=%d, "
		"session=%d, gateway_mac= "QDF_MAC_ADDR_FMT", time_period_sec=%d,"
		"tcp_sn=%d", ts_lphb_tcp_param->srv_ip,
		ts_lphb_tcp_param->dev_ip, ts_lphb_tcp_param->src_port,
		ts_lphb_tcp_param->dst_port, ts_lphb_tcp_param->timeout,
		ts_lphb_tcp_param->session,
		QDF_MAC_ADDR_REF(ts_lphb_tcp_param->gateway_mac.bytes),
		ts_lphb_tcp_param->time_period_sec, ts_lphb_tcp_param->tcp_sn);

	/* fill in values */
	hb_tcp_params_fp.vdev_id = ts_lphb_tcp_param->session;
	hb_tcp_params_fp.srv_ip = ts_lphb_tcp_param->srv_ip;
	hb_tcp_params_fp.dev_ip = ts_lphb_tcp_param->dev_ip;
	hb_tcp_params_fp.seq = ts_lphb_tcp_param->tcp_sn;
	hb_tcp_params_fp.src_port = ts_lphb_tcp_param->src_port;
	hb_tcp_params_fp.dst_port = ts_lphb_tcp_param->dst_port;
	hb_tcp_params_fp.interval = ts_lphb_tcp_param->time_period_sec;
	hb_tcp_params_fp.timeout = ts_lphb_tcp_param->timeout;
	hb_tcp_params_fp.session = ts_lphb_tcp_param->session;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(ts_lphb_tcp_param->gateway_mac.bytes,
				   &hb_tcp_params_fp.gateway_mac);

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_lphb_config_tcp_params_cmd(wmi_handle,
						      &hb_tcp_params_fp);
}

QDF_STATUS target_if_pmo_send_lphb_tcp_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_tcp_filter_req *ts_lphb_tcp_filter)
{
	wmi_hb_set_tcp_pkt_filter_cmd_fixed_param hb_tcp_filter_fp = {0};
	wmi_unified_t wmi_handle;

	if (!ts_lphb_tcp_filter) {
		target_if_err("TCP PKT FILTER LPHB configuration is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	target_if_info("SET_TCP_PKT_FILTER length=%d, offset=%d, session=%d, "
		"filter=%2x:%2x:%2x:%2x:%2x:%2x ...",
		ts_lphb_tcp_filter->length, ts_lphb_tcp_filter->offset,
		ts_lphb_tcp_filter->session, ts_lphb_tcp_filter->filter[0],
		ts_lphb_tcp_filter->filter[1], ts_lphb_tcp_filter->filter[2],
		ts_lphb_tcp_filter->filter[3], ts_lphb_tcp_filter->filter[4],
		ts_lphb_tcp_filter->filter[5]);

	/* fill in values */
	hb_tcp_filter_fp.vdev_id = ts_lphb_tcp_filter->session;
	hb_tcp_filter_fp.length = ts_lphb_tcp_filter->length;
	hb_tcp_filter_fp.offset = ts_lphb_tcp_filter->offset;
	hb_tcp_filter_fp.session = ts_lphb_tcp_filter->session;
	memcpy((void *)&hb_tcp_filter_fp.filter,
	       (void *)&ts_lphb_tcp_filter->filter,
	       WMI_WLAN_HB_MAX_FILTER_SIZE);

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_lphb_config_tcp_pkt_filter_cmd(wmi_handle,
							  &hb_tcp_filter_fp);
}

QDF_STATUS target_if_pmo_send_lphb_udp_params(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_udp_params *ts_lphb_udp_param)
{
	wmi_hb_set_udp_params_cmd_fixed_param hb_udp_params_fp = {0};
	wmi_unified_t wmi_handle;

	if (!ts_lphb_udp_param) {
		target_if_err("UDP param for LPHB configuration is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	target_if_info("HB_SET_UDP_PARAMS srv_ip=%d, dev_ip=%d, src_port=%d, "
		"dst_port=%d, interval=%d, timeout=%d, session=%d, "
		"gateway_mac= "QDF_MAC_ADDR_FMT,
		ts_lphb_udp_param->srv_ip, ts_lphb_udp_param->dev_ip,
		ts_lphb_udp_param->src_port, ts_lphb_udp_param->dst_port,
		ts_lphb_udp_param->interval, ts_lphb_udp_param->timeout,
		ts_lphb_udp_param->session,
		QDF_MAC_ADDR_REF(ts_lphb_udp_param->gateway_mac.bytes));

	/* fill in values */
	hb_udp_params_fp.vdev_id = ts_lphb_udp_param->session;
	hb_udp_params_fp.srv_ip = ts_lphb_udp_param->srv_ip;
	hb_udp_params_fp.dev_ip = ts_lphb_udp_param->dev_ip;
	hb_udp_params_fp.src_port = ts_lphb_udp_param->src_port;
	hb_udp_params_fp.dst_port = ts_lphb_udp_param->dst_port;
	hb_udp_params_fp.interval = ts_lphb_udp_param->interval;
	hb_udp_params_fp.timeout = ts_lphb_udp_param->timeout;
	hb_udp_params_fp.session = ts_lphb_udp_param->session;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(ts_lphb_udp_param->gateway_mac.bytes,
				   &hb_udp_params_fp.gateway_mac);

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_lphb_config_udp_params_cmd(wmi_handle,
						      &hb_udp_params_fp);
}

/**
 * target_if_pmo_lphb_send_udp_pkt_filter() - Send LPHB udp pkt filter req
 * @psoc: objmgr psoc handle
 * @ts_lphb_udp_filter: lphb udp filter request which needs to configure in fwr
 *
 * Return: QDF status
 */
QDF_STATUS target_if_pmo_send_lphb_udp_pkt_filter(struct wlan_objmgr_psoc *psoc,
			struct pmo_lphb_udp_filter_req *ts_lphb_udp_filter)
{
	wmi_hb_set_udp_pkt_filter_cmd_fixed_param hb_udp_filter_fp = {0};
	wmi_unified_t wmi_handle;

	if (!ts_lphb_udp_filter) {
		target_if_err("LPHB UDP packet filter configuration is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	target_if_info("SET_UDP_PKT_FILTER length=%d, offset=%d, session=%d, "
		"filter=%2x:%2x:%2x:%2x:%2x:%2x ...",
		ts_lphb_udp_filter->length, ts_lphb_udp_filter->offset,
		ts_lphb_udp_filter->session, ts_lphb_udp_filter->filter[0],
		ts_lphb_udp_filter->filter[1], ts_lphb_udp_filter->filter[2],
		ts_lphb_udp_filter->filter[3], ts_lphb_udp_filter->filter[4],
		ts_lphb_udp_filter->filter[5]);

	/* fill in values */
	hb_udp_filter_fp.vdev_id = ts_lphb_udp_filter->session;
	hb_udp_filter_fp.length = ts_lphb_udp_filter->length;
	hb_udp_filter_fp.offset = ts_lphb_udp_filter->offset;
	hb_udp_filter_fp.session = ts_lphb_udp_filter->session;
	qdf_mem_copy(&hb_udp_filter_fp.filter, &ts_lphb_udp_filter->filter,
		     WMI_WLAN_HB_MAX_FILTER_SIZE);

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	return wmi_unified_lphb_config_udp_pkt_filter_cmd(wmi_handle,
							  &hb_udp_filter_fp);
}

QDF_STATUS target_if_pmo_lphb_evt_handler(struct wlan_objmgr_psoc *psoc,
		uint8_t *event)
{
	wmi_hb_ind_event_fixed_param *hb_fp;
	struct pmo_lphb_rsp *slphb_indication = NULL;
	QDF_STATUS qdf_status;

	TARGET_IF_ENTER();
	if (!psoc) {
		target_if_err("psoc ptr is NULL");
		qdf_status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	hb_fp = (wmi_hb_ind_event_fixed_param *) event;
	if (!hb_fp) {
		target_if_err("Invalid wmi_hb_ind_event_fixed_param buffer");
		qdf_status = QDF_STATUS_E_INVAL;
		goto out;
	}

	target_if_debug("lphb indication received with\n"
		  "vdev_id=%d, session=%d, reason=%d",
		hb_fp->vdev_id, hb_fp->session, hb_fp->reason);

	slphb_indication = (struct pmo_lphb_rsp *)qdf_mem_malloc(
				sizeof(struct pmo_lphb_rsp));
	if (!slphb_indication) {
		qdf_status = QDF_STATUS_E_NOMEM;
		goto out;
	}

	slphb_indication->session_idx = hb_fp->session;
	slphb_indication->protocol_type = hb_fp->reason;
	slphb_indication->event_reason = hb_fp->reason;

	qdf_status = pmo_tgt_lphb_rsp_evt(psoc, slphb_indication);
	if (qdf_status != QDF_STATUS_SUCCESS)
		target_if_err("Failed to lphb_rsp_event");
out:
	if (slphb_indication)
		qdf_mem_free(slphb_indication);

	return qdf_status;
}
#endif

