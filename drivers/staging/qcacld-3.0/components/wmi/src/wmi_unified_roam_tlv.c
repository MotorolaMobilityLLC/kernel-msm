/*
 * Copyright (c) 2013-2021, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: Implement API's specific to roaming component.
 */

#include <wmi_unified_priv.h>
#include <wmi_unified_roam_api.h>
#include "wmi.h"

#ifdef FEATURE_LFR_SUBNET_DETECTION
/**
 * send_set_gateway_params_cmd_tlv() - set gateway parameters
 * @wmi_handle: wmi handle
 * @req: gateway parameter update request structure
 *
 * This function reads the incoming @req and fill in the destination
 * WMI structure and sends down the gateway configs down to the firmware
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS send_set_gateway_params_cmd_tlv(wmi_unified_t wmi_handle,
				struct gateway_update_req_param *req)
{
	wmi_roam_subnet_change_config_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS ret;
	int len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_roam_subnet_change_config_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_roam_subnet_change_config_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_roam_subnet_change_config_fixed_param));

	cmd->vdev_id = req->vdev_id;
	qdf_mem_copy(&cmd->inet_gw_ip_v4_addr, req->ipv4_addr,
		     QDF_IPV4_ADDR_SIZE);
	qdf_mem_copy(&cmd->inet_gw_ip_v6_addr, req->ipv6_addr,
		     QDF_IPV6_ADDR_SIZE);
	WMI_CHAR_ARRAY_TO_MAC_ADDR(req->gw_mac_addr.bytes,
				   &cmd->inet_gw_mac_addr);
	cmd->max_retries = req->max_retries;
	cmd->timeout = req->timeout;
	cmd->num_skip_subnet_change_detection_bssid_list = 0;
	cmd->flag = 0;
	if (req->ipv4_addr_type)
		WMI_SET_ROAM_SUBNET_CHANGE_FLAG_IP4_ENABLED(cmd->flag);

	if (req->ipv6_addr_type)
		WMI_SET_ROAM_SUBNET_CHANGE_FLAG_IP6_ENABLED(cmd->flag);

	wmi_mtrace(WMI_ROAM_SUBNET_CHANGE_CONFIG_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_ROAM_SUBNET_CHANGE_CONFIG_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send gw config parameter to fw, ret: %d",
			ret);
		wmi_buf_free(buf);
	}

	return ret;
}

void wmi_lfr_subnet_detection_attach_tlv(struct wmi_unified *wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->send_set_gateway_params_cmd = send_set_gateway_params_cmd_tlv;
}
#endif /* FEATURE_LFR_SUBNET_DETECTION */

#ifdef FEATURE_RSSI_MONITOR
/**
 * send_set_rssi_monitoring_cmd_tlv() - set rssi monitoring
 * @wmi_handle: wmi handle
 * @req: rssi monitoring request structure
 *
 * This function reads the incoming @req and fill in the destination
 * WMI structure and send down the rssi monitoring configs down to the firmware
 *
 * Return: 0 on success; error number otherwise
 */
static QDF_STATUS send_set_rssi_monitoring_cmd_tlv(wmi_unified_t wmi_handle,
					struct rssi_monitor_param *req)
{
	wmi_rssi_breach_monitor_config_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS ret;
	uint32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_rssi_breach_monitor_config_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_rssi_breach_monitor_config_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_rssi_breach_monitor_config_fixed_param));

	cmd->vdev_id = req->vdev_id;
	cmd->request_id = req->request_id;
	cmd->lo_rssi_reenable_hysteresis = 0;
	cmd->hi_rssi_reenable_histeresis = 0;
	cmd->min_report_interval = 0;
	cmd->max_num_report = 1;
	if (req->control) {
		/* enable one threshold for each min/max */
		cmd->enabled_bitmap = 0x09;
		cmd->low_rssi_breach_threshold[0] = req->min_rssi;
		cmd->hi_rssi_breach_threshold[0] = req->max_rssi;
	} else {
		cmd->enabled_bitmap = 0;
		cmd->low_rssi_breach_threshold[0] = 0;
		cmd->hi_rssi_breach_threshold[0] = 0;
	}

	wmi_mtrace(WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID");
		wmi_buf_free(buf);
	}

	wmi_debug("Sent WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID to FW");

	return ret;
}

void wmi_rssi_monitor_attach_tlv(struct wmi_unified *wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->send_set_rssi_monitoring_cmd = send_set_rssi_monitoring_cmd_tlv;
}
#endif /* FEATURE_RSSI_MONITOR */

/**
 * send_roam_scan_offload_rssi_thresh_cmd_tlv() - set scan offload
 *                                                rssi threashold
 * @wmi_handle: wmi handle
 * @roam_req:   Roaming request buffer
 *
 * Send WMI_ROAM_SCAN_RSSI_THRESHOLD TLV to firmware
 *
 * Return: QDF status
 */
static QDF_STATUS send_roam_scan_offload_rssi_thresh_cmd_tlv(
			wmi_unified_t wmi_handle,
			struct wlan_roam_offload_scan_rssi_params *roam_req)
{
	wmi_buf_t buf = NULL;
	QDF_STATUS status;
	int len;
	uint8_t *buf_ptr;
	wmi_roam_scan_rssi_threshold_fixed_param *rssi_threshold_fp;
	wmi_roam_scan_extended_threshold_param *ext_thresholds = NULL;
	wmi_roam_earlystop_rssi_thres_param *early_stop_thresholds = NULL;
	wmi_roam_dense_thres_param *dense_thresholds = NULL;
	wmi_roam_bg_scan_roaming_param *bg_scan_params = NULL;
	wmi_roam_data_rssi_roaming_param *data_rssi_param = NULL;

	len = sizeof(wmi_roam_scan_rssi_threshold_fixed_param);
	len += WMI_TLV_HDR_SIZE; /* TLV for ext_thresholds*/
	len += sizeof(wmi_roam_scan_extended_threshold_param);
	len += WMI_TLV_HDR_SIZE;
	len += sizeof(wmi_roam_earlystop_rssi_thres_param);
	len += WMI_TLV_HDR_SIZE; /* TLV for dense thresholds*/
	len += sizeof(wmi_roam_dense_thres_param);
	len += WMI_TLV_HDR_SIZE; /* TLV for BG Scan*/
	len += sizeof(wmi_roam_bg_scan_roaming_param);
	len += WMI_TLV_HDR_SIZE; /* TLV for data RSSI*/
	len += sizeof(wmi_roam_data_rssi_roaming_param);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	rssi_threshold_fp =
		(wmi_roam_scan_rssi_threshold_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(
		&rssi_threshold_fp->tlv_header,
		WMITLV_TAG_STRUC_wmi_roam_scan_rssi_threshold_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN
				(wmi_roam_scan_rssi_threshold_fixed_param));
	/* fill in threshold values */
	rssi_threshold_fp->vdev_id = roam_req->vdev_id;
	rssi_threshold_fp->roam_scan_rssi_thresh = roam_req->rssi_thresh;
	rssi_threshold_fp->roam_rssi_thresh_diff = roam_req->rssi_thresh_diff;
	rssi_threshold_fp->hirssi_scan_max_count =
			roam_req->hi_rssi_scan_max_count;
	rssi_threshold_fp->hirssi_scan_delta =
			roam_req->hi_rssi_scan_rssi_delta;
	rssi_threshold_fp->hirssi_upper_bound = roam_req->hi_rssi_scan_rssi_ub;
	rssi_threshold_fp->rssi_thresh_offset_5g =
		roam_req->rssi_thresh_offset_5g;

	buf_ptr += sizeof(wmi_roam_scan_rssi_threshold_fixed_param);
	WMITLV_SET_HDR(buf_ptr,
		       WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_roam_scan_extended_threshold_param));
	buf_ptr += WMI_TLV_HDR_SIZE;
	ext_thresholds = (wmi_roam_scan_extended_threshold_param *)buf_ptr;

	ext_thresholds->penalty_threshold_5g = roam_req->penalty_threshold_5g;
	if (roam_req->raise_rssi_thresh_5g >= WMI_NOISE_FLOOR_DBM_DEFAULT)
		ext_thresholds->boost_threshold_5g =
					roam_req->boost_threshold_5g;

	ext_thresholds->boost_algorithm_5g =
		WMI_ROAM_5G_BOOST_PENALIZE_ALGO_LINEAR;
	ext_thresholds->boost_factor_5g = roam_req->raise_factor_5g;
	ext_thresholds->penalty_algorithm_5g =
		WMI_ROAM_5G_BOOST_PENALIZE_ALGO_LINEAR;
	ext_thresholds->penalty_factor_5g = roam_req->drop_factor_5g;
	ext_thresholds->max_boost_5g = roam_req->max_raise_rssi_5g;
	ext_thresholds->max_penalty_5g = roam_req->max_drop_rssi_5g;
	ext_thresholds->good_rssi_threshold = roam_req->good_rssi_threshold;

	WMITLV_SET_HDR(&ext_thresholds->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_scan_extended_threshold_param,
		       WMITLV_GET_STRUCT_TLVLEN
				(wmi_roam_scan_extended_threshold_param));
	buf_ptr += sizeof(wmi_roam_scan_extended_threshold_param);
	WMITLV_SET_HDR(buf_ptr,
		       WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_roam_earlystop_rssi_thres_param));
	buf_ptr += WMI_TLV_HDR_SIZE;
	early_stop_thresholds = (wmi_roam_earlystop_rssi_thres_param *)buf_ptr;
	early_stop_thresholds->roam_earlystop_thres_min =
		roam_req->roam_earlystop_thres_min;
	early_stop_thresholds->roam_earlystop_thres_max =
		roam_req->roam_earlystop_thres_max;
	WMITLV_SET_HDR(&early_stop_thresholds->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_earlystop_rssi_thres_param,
		       WMITLV_GET_STRUCT_TLVLEN
				(wmi_roam_earlystop_rssi_thres_param));

	buf_ptr += sizeof(wmi_roam_earlystop_rssi_thres_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_roam_dense_thres_param));
	buf_ptr += WMI_TLV_HDR_SIZE;
	dense_thresholds = (wmi_roam_dense_thres_param *)buf_ptr;
	dense_thresholds->roam_dense_rssi_thres_offset =
			roam_req->dense_rssi_thresh_offset;
	dense_thresholds->roam_dense_min_aps = roam_req->dense_min_aps_cnt;
	dense_thresholds->roam_dense_traffic_thres =
			roam_req->traffic_threshold;
	dense_thresholds->roam_dense_status = roam_req->initial_dense_status;
	WMITLV_SET_HDR(&dense_thresholds->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_dense_thres_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_roam_dense_thres_param));

	buf_ptr += sizeof(wmi_roam_dense_thres_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_roam_bg_scan_roaming_param));
	buf_ptr += WMI_TLV_HDR_SIZE;
	bg_scan_params = (wmi_roam_bg_scan_roaming_param *)buf_ptr;
	bg_scan_params->roam_bg_scan_bad_rssi_thresh =
		roam_req->bg_scan_bad_rssi_thresh;
	bg_scan_params->roam_bg_scan_client_bitmap =
		roam_req->bg_scan_client_bitmap;
	bg_scan_params->bad_rssi_thresh_offset_2g =
		roam_req->roam_bad_rssi_thresh_offset_2g;

	bg_scan_params->flags = 0;
	if (roam_req->roam_bad_rssi_thresh_offset_2g)
		bg_scan_params->flags |= WMI_ROAM_BG_SCAN_FLAGS_2G_TO_5G_ONLY;
	WMITLV_SET_HDR(&bg_scan_params->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_bg_scan_roaming_param,
		       WMITLV_GET_STRUCT_TLVLEN
		       (wmi_roam_bg_scan_roaming_param));

	buf_ptr += sizeof(wmi_roam_bg_scan_roaming_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_roam_data_rssi_roaming_param));
	buf_ptr += WMI_TLV_HDR_SIZE;
	data_rssi_param = (wmi_roam_data_rssi_roaming_param *)buf_ptr;
	data_rssi_param->flags =
		roam_req->roam_data_rssi_threshold_triggers;
	data_rssi_param->roam_data_rssi_thres =
		roam_req->roam_data_rssi_threshold;
	data_rssi_param->rx_inactivity_ms =
		roam_req->rx_data_inactivity_time;
	WMITLV_SET_HDR(&data_rssi_param->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_data_rssi_roaming_param,
		       WMITLV_GET_STRUCT_TLVLEN
		       (wmi_roam_data_rssi_roaming_param));
	wmi_debug("vdev %d Data rssi threshold: %d, triggers: 0x%x, rx time: %d",
		  rssi_threshold_fp->vdev_id,
		  data_rssi_param->roam_data_rssi_thres,
		  data_rssi_param->flags,
		  data_rssi_param->rx_inactivity_ms);

	wmi_mtrace(WMI_ROAM_SCAN_RSSI_THRESHOLD, NO_SESSION, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf,
				      len, WMI_ROAM_SCAN_RSSI_THRESHOLD);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_err("cmd WMI_ROAM_SCAN_RSSI_THRESHOLD returned Error %d",
			status);
		wmi_buf_free(buf);
	}

	return status;
}

/**
 * send_roam_scan_offload_scan_period_cmd_tlv() - set roam offload scan period
 * @wmi_handle: wmi handle
 * @param: roam scan parameters to be sent to firmware
 *
 * Send WMI_ROAM_SCAN_PERIOD parameters to fw.
 *
 * Return: QDF status
 */
static QDF_STATUS
send_roam_scan_offload_scan_period_cmd_tlv(
		wmi_unified_t wmi_handle,
		struct wlan_roam_scan_period_params *param)
{
	QDF_STATUS status;
	wmi_buf_t buf = NULL;
	int len;
	uint8_t *buf_ptr;
	wmi_roam_scan_period_fixed_param *scan_period_fp;

	/* Send scan period values */
	len = sizeof(wmi_roam_scan_period_fixed_param);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	scan_period_fp = (wmi_roam_scan_period_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&scan_period_fp->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_scan_period_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_roam_scan_period_fixed_param));
	/* fill in scan period values */
	scan_period_fp->vdev_id = param->vdev_id;
	scan_period_fp->roam_scan_period = param->scan_period;
	scan_period_fp->roam_scan_age = param->scan_age;
	scan_period_fp->inactivity_time_period =
			param->roam_scan_inactivity_time;
	scan_period_fp->roam_inactive_count =
			param->roam_inactive_data_packet_count;
	scan_period_fp->roam_scan_period_after_inactivity =
			param->roam_scan_period_after_inactivity;
	/* Firmware expects the full scan preriod in msec whereas host
	 * provides the same in seconds.
	 * Convert it to msec and send to firmware
	 */
	scan_period_fp->roam_full_scan_period = param->full_scan_period * 1000;

	wmi_debug("roam_scan_period=%d, roam_scan_age=%d, full_scan_period= %u",
		  scan_period_fp->roam_scan_period,
		  scan_period_fp->roam_scan_age,
		  scan_period_fp->roam_full_scan_period);

	wmi_debug("inactiviy time:%d inactive cnt:%d time after inactivity:%d",
		  scan_period_fp->inactivity_time_period,
		  scan_period_fp->roam_inactive_count,
		  scan_period_fp->roam_scan_period_after_inactivity);

	wmi_mtrace(WMI_ROAM_SCAN_PERIOD, NO_SESSION, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf, len,
				      WMI_ROAM_SCAN_PERIOD);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_buf_free(buf);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS send_roam_mawc_params_cmd_tlv(
				wmi_unified_t wmi_handle,
				struct wlan_roam_mawc_params *params)
{
	wmi_buf_t buf = NULL;
	QDF_STATUS status;
	int len;
	uint8_t *buf_ptr;
	wmi_roam_configure_mawc_cmd_fixed_param *wmi_roam_mawc_params;

	len = sizeof(*wmi_roam_mawc_params);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	wmi_roam_mawc_params =
		(wmi_roam_configure_mawc_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&wmi_roam_mawc_params->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_configure_mawc_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_roam_configure_mawc_cmd_fixed_param));
	wmi_roam_mawc_params->vdev_id = params->vdev_id;
	if (params->enable)
		wmi_roam_mawc_params->enable = 1;
	else
		wmi_roam_mawc_params->enable = 0;
	wmi_roam_mawc_params->traffic_load_threshold =
		params->traffic_load_threshold;
	wmi_roam_mawc_params->best_ap_rssi_threshold =
		params->best_ap_rssi_threshold;
	wmi_roam_mawc_params->rssi_stationary_high_adjust =
		params->rssi_stationary_high_adjust;
	wmi_roam_mawc_params->rssi_stationary_low_adjust =
		params->rssi_stationary_low_adjust;
	wmi_debug("MAWC roam en=%d, vdev=%d, tr=%d, ap=%d, high=%d, low=%d",
		wmi_roam_mawc_params->enable, wmi_roam_mawc_params->vdev_id,
		wmi_roam_mawc_params->traffic_load_threshold,
		wmi_roam_mawc_params->best_ap_rssi_threshold,
		wmi_roam_mawc_params->rssi_stationary_high_adjust,
		wmi_roam_mawc_params->rssi_stationary_low_adjust);

	wmi_mtrace(WMI_ROAM_CONFIGURE_MAWC_CMDID, NO_SESSION, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf,
				      len, WMI_ROAM_CONFIGURE_MAWC_CMDID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_err("WMI_ROAM_CONFIGURE_MAWC_CMDID failed, Error %d",
			status);
		wmi_buf_free(buf);
		return status;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_roam_scan_filter_cmd_tlv() - Filter to be applied while roaming
 * @wmi_handle:     wmi handle
 * @roam_req:       Request which contains the filters
 *
 * There are filters such as whitelist, blacklist and preferred
 * list that need to be applied to the scan results to form the
 * probable candidates for roaming.
 *
 * Return: Return success upon successfully passing the
 *         parameters to the firmware, otherwise failure.
 */
static QDF_STATUS send_roam_scan_filter_cmd_tlv(wmi_unified_t wmi_handle,
				struct roam_scan_filter_params *roam_req)
{
	wmi_buf_t buf = NULL;
	QDF_STATUS status;
	uint32_t i;
	uint32_t len, blist_len = 0;
	uint8_t *buf_ptr;
	wmi_roam_filter_fixed_param *roam_filter;
	uint8_t *bssid_src_ptr = NULL;
	wmi_mac_addr *bssid_dst_ptr = NULL;
	wmi_ssid *ssid_ptr = NULL;
	uint32_t *bssid_preferred_factor_ptr = NULL;
	wmi_roam_lca_disallow_config_tlv_param *blist_param;
	wmi_roam_rssi_rejection_oce_config_param *rssi_rej;

	len = sizeof(wmi_roam_filter_fixed_param);

	len += WMI_TLV_HDR_SIZE;
	if (roam_req->num_bssid_black_list)
		len += roam_req->num_bssid_black_list * sizeof(wmi_mac_addr);
	len += WMI_TLV_HDR_SIZE;
	if (roam_req->num_ssid_white_list)
		len += roam_req->num_ssid_white_list * sizeof(wmi_ssid);
	len += 2 * WMI_TLV_HDR_SIZE;
	if (roam_req->num_bssid_preferred_list) {
		len += (roam_req->num_bssid_preferred_list *
			sizeof(wmi_mac_addr));
		len += roam_req->num_bssid_preferred_list * sizeof(uint32_t);
	}
	len += WMI_TLV_HDR_SIZE;
	if (roam_req->lca_disallow_config_present) {
		len += sizeof(*blist_param);
		blist_len = sizeof(*blist_param);
	}

	len += WMI_TLV_HDR_SIZE;
	if (roam_req->num_rssi_rejection_ap)
		len += roam_req->num_rssi_rejection_ap * sizeof(*rssi_rej);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (u_int8_t *)wmi_buf_data(buf);
	roam_filter = (wmi_roam_filter_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&roam_filter->tlv_header,
		WMITLV_TAG_STRUC_wmi_roam_filter_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_roam_filter_fixed_param));
	/* fill in fixed values */
	roam_filter->vdev_id = roam_req->vdev_id;
	roam_filter->flags = 0;
	roam_filter->op_bitmap = roam_req->op_bitmap;
	roam_filter->num_bssid_black_list = roam_req->num_bssid_black_list;
	roam_filter->num_ssid_white_list = roam_req->num_ssid_white_list;
	roam_filter->num_bssid_preferred_list =
			roam_req->num_bssid_preferred_list;
	roam_filter->num_rssi_rejection_ap =
			roam_req->num_rssi_rejection_ap;
	roam_filter->delta_rssi = roam_req->delta_rssi;
	buf_ptr += sizeof(wmi_roam_filter_fixed_param);

	WMITLV_SET_HDR((buf_ptr),
		WMITLV_TAG_ARRAY_FIXED_STRUC,
		(roam_req->num_bssid_black_list * sizeof(wmi_mac_addr)));
	bssid_src_ptr = (uint8_t *)&roam_req->bssid_avoid_list;
	bssid_dst_ptr = (wmi_mac_addr *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; i < roam_req->num_bssid_black_list; i++) {
		WMI_CHAR_ARRAY_TO_MAC_ADDR(bssid_src_ptr, bssid_dst_ptr);
		bssid_src_ptr += ATH_MAC_LEN;
		bssid_dst_ptr++;
	}
	buf_ptr += WMI_TLV_HDR_SIZE +
		(roam_req->num_bssid_black_list * sizeof(wmi_mac_addr));
	WMITLV_SET_HDR((buf_ptr),
		       WMITLV_TAG_ARRAY_FIXED_STRUC,
		       (roam_req->num_ssid_white_list * sizeof(wmi_ssid)));
	ssid_ptr = (wmi_ssid *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; i < roam_req->num_ssid_white_list; i++) {
		qdf_mem_copy(&ssid_ptr->ssid,
			&roam_req->ssid_allowed_list[i].ssid,
			roam_req->ssid_allowed_list[i].length);
		ssid_ptr->ssid_len = roam_req->ssid_allowed_list[i].length;
		ssid_ptr++;
	}
	buf_ptr += WMI_TLV_HDR_SIZE + (roam_req->num_ssid_white_list *
							sizeof(wmi_ssid));
	WMITLV_SET_HDR((buf_ptr),
		WMITLV_TAG_ARRAY_FIXED_STRUC,
		(roam_req->num_bssid_preferred_list * sizeof(wmi_mac_addr)));
	bssid_src_ptr = (uint8_t *)&roam_req->bssid_favored;
	bssid_dst_ptr = (wmi_mac_addr *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; i < roam_req->num_bssid_preferred_list; i++) {
		WMI_CHAR_ARRAY_TO_MAC_ADDR(bssid_src_ptr,
					   (wmi_mac_addr *)bssid_dst_ptr);
		bssid_src_ptr += ATH_MAC_LEN;
		bssid_dst_ptr++;
	}
	buf_ptr += WMI_TLV_HDR_SIZE +
		(roam_req->num_bssid_preferred_list * sizeof(wmi_mac_addr));
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		(roam_req->num_bssid_preferred_list * sizeof(uint32_t)));
	bssid_preferred_factor_ptr = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; i < roam_req->num_bssid_preferred_list; i++) {
		*bssid_preferred_factor_ptr =
			roam_req->bssid_favored_factor[i];
		bssid_preferred_factor_ptr++;
	}
	buf_ptr += WMI_TLV_HDR_SIZE +
		(roam_req->num_bssid_preferred_list * sizeof(uint32_t));

	WMITLV_SET_HDR(buf_ptr,
			WMITLV_TAG_ARRAY_STRUC, blist_len);
	buf_ptr += WMI_TLV_HDR_SIZE;
	if (roam_req->lca_disallow_config_present) {
		blist_param =
			(wmi_roam_lca_disallow_config_tlv_param *)buf_ptr;
		WMITLV_SET_HDR(&blist_param->tlv_header,
			WMITLV_TAG_STRUC_wmi_roam_lca_disallow_config_tlv_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_roam_lca_disallow_config_tlv_param));

		blist_param->disallow_duration = roam_req->disallow_duration;
		blist_param->rssi_channel_penalization =
				roam_req->rssi_channel_penalization;
		blist_param->num_disallowed_aps = roam_req->num_disallowed_aps;
		blist_param->disallow_lca_enable_source_bitmap =
			(WMI_ROAM_LCA_DISALLOW_SOURCE_PER |
			WMI_ROAM_LCA_DISALLOW_SOURCE_BACKGROUND);
		buf_ptr += (sizeof(wmi_roam_lca_disallow_config_tlv_param));
	}

	WMITLV_SET_HDR(buf_ptr,
		       WMITLV_TAG_ARRAY_STRUC,
		       (roam_req->num_rssi_rejection_ap * sizeof(*rssi_rej)));
	buf_ptr += WMI_TLV_HDR_SIZE;
	for (i = 0; i < roam_req->num_rssi_rejection_ap; i++) {
		rssi_rej =
		(wmi_roam_rssi_rejection_oce_config_param *)buf_ptr;

		WMITLV_SET_HDR(&rssi_rej->tlv_header,
		WMITLV_TAG_STRUC_wmi_roam_rssi_rejection_oce_config_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_roam_rssi_rejection_oce_config_param));

		WMI_CHAR_ARRAY_TO_MAC_ADDR(
			roam_req->rssi_rejection_ap[i].bssid.bytes,
			&rssi_rej->bssid);
		rssi_rej->remaining_disallow_duration =
			roam_req->rssi_rejection_ap[i].reject_duration;
		rssi_rej->requested_rssi =
			(int32_t)roam_req->rssi_rejection_ap[i].expected_rssi;
		buf_ptr +=
			(sizeof(wmi_roam_rssi_rejection_oce_config_param));
	}

	wmi_mtrace(WMI_ROAM_FILTER_CMDID, NO_SESSION, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf,
				      len, WMI_ROAM_FILTER_CMDID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_err("cmd WMI_ROAM_FILTER_CMDID returned Error %d",
			status);
		wmi_buf_free(buf);
	}

	return status;
}

#ifdef FEATURE_WLAN_ESE
/**
 * send_plm_stop_cmd_tlv() - plm stop request
 * @wmi_handle: wmi handle
 * @plm: plm request parameters
 *
 * This function request FW to stop PLM.
 *
 * Return: CDF status
 */
static QDF_STATUS send_plm_stop_cmd_tlv(wmi_unified_t wmi_handle,
			  const struct plm_req_params *plm)
{
	wmi_vdev_plmreq_stop_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	int ret;

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_vdev_plmreq_stop_cmd_fixed_param *)wmi_buf_data(buf);

	buf_ptr = (uint8_t *)cmd;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_plmreq_stop_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
				(wmi_vdev_plmreq_stop_cmd_fixed_param));

	cmd->vdev_id = plm->vdev_id;

	cmd->meas_token = plm->meas_token;
	wmi_debug("vdev %d meas token %d", cmd->vdev_id, cmd->meas_token);

	wmi_mtrace(WMI_VDEV_PLMREQ_STOP_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_VDEV_PLMREQ_STOP_CMDID);
	if (ret) {
		wmi_err("Failed to send plm stop wmi cmd");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_plm_start_cmd_tlv() - plm start request
 * @wmi_handle: wmi handle
 * @plm: plm request parameters
 *
 * This function request FW to start PLM.
 *
 * Return: CDF status
 */
static QDF_STATUS send_plm_start_cmd_tlv(wmi_unified_t wmi_handle,
					 const struct plm_req_params *plm)
{
	wmi_vdev_plmreq_start_cmd_fixed_param *cmd;
	uint32_t *channel_list;
	int32_t len;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint8_t count;
	int ret;

	/* TLV place holder for channel_list */
	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
	len += sizeof(uint32_t) * plm->plm_num_ch;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_vdev_plmreq_start_cmd_fixed_param *)wmi_buf_data(buf);

	buf_ptr = (uint8_t *)cmd;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_plmreq_start_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_vdev_plmreq_start_cmd_fixed_param));

	cmd->vdev_id = plm->vdev_id;

	cmd->meas_token = plm->meas_token;
	cmd->dialog_token = plm->diag_token;
	cmd->number_bursts = plm->num_bursts;
	cmd->burst_interval = WMI_SEC_TO_MSEC(plm->burst_int);
	cmd->off_duration = plm->meas_duration;
	cmd->burst_cycle = plm->burst_len;
	cmd->tx_power = plm->desired_tx_pwr;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(plm->mac_addr.bytes, &cmd->dest_mac);
	cmd->num_chans = plm->plm_num_ch;

	buf_ptr += sizeof(wmi_vdev_plmreq_start_cmd_fixed_param);

	wmi_debug("vdev: %d measu token: %d dialog_token: %d number_bursts: %d burst_interval: %d off_duration: %d burst_cycle: %d tx_power: %d Number of channels: %d",
		 cmd->vdev_id, cmd->meas_token, cmd->dialog_token,
		 cmd->number_bursts, cmd->burst_interval, cmd->off_duration,
		 cmd->burst_cycle, cmd->tx_power, cmd->num_chans);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       (cmd->num_chans * sizeof(uint32_t)));

	buf_ptr += WMI_TLV_HDR_SIZE;
	if (cmd->num_chans) {
		channel_list = (uint32_t *)buf_ptr;
		for (count = 0; count < cmd->num_chans; count++) {
			channel_list[count] = plm->plm_ch_freq_list[count];
			wmi_debug("Ch[%d]: %d MHz", count, channel_list[count]);
		}
		buf_ptr += cmd->num_chans * sizeof(uint32_t);
	}

	wmi_mtrace(WMI_VDEV_PLMREQ_START_CMDID, cmd->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_VDEV_PLMREQ_START_CMDID);
	if (ret) {
		wmi_err("Failed to send plm start wmi cmd");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void wmi_ese_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->send_plm_stop_cmd = send_plm_stop_cmd_tlv;
	ops->send_plm_start_cmd = send_plm_start_cmd_tlv;
}
#endif /* FEATURE_WLAN_ESE */

/**
 * convert_roam_trigger_reason() - Function to convert unified Roam trigger
 * enum to TLV specific WMI_ROAM_TRIGGER_REASON_ID
 * @reason: Roam trigger reason
 *
 * Return: WMI roam trigger reason
 */
static uint32_t
convert_roam_trigger_reason(enum roam_trigger_reason trigger_reason)
{
	switch (trigger_reason) {
	case ROAM_TRIGGER_REASON_NONE:
		return WMI_ROAM_TRIGGER_REASON_NONE;
	case ROAM_TRIGGER_REASON_PER:
		return WMI_ROAM_TRIGGER_REASON_PER;
	case ROAM_TRIGGER_REASON_BMISS:
		return WMI_ROAM_TRIGGER_REASON_BMISS;
	case ROAM_TRIGGER_REASON_LOW_RSSI:
		return WMI_ROAM_TRIGGER_REASON_LOW_RSSI;
	case ROAM_TRIGGER_REASON_HIGH_RSSI:
		return WMI_ROAM_TRIGGER_REASON_HIGH_RSSI;
	case ROAM_TRIGGER_REASON_PERIODIC:
		return WMI_ROAM_TRIGGER_REASON_PERIODIC;
	case ROAM_TRIGGER_REASON_MAWC:
		return WMI_ROAM_TRIGGER_REASON_MAWC;
	case ROAM_TRIGGER_REASON_DENSE:
		return WMI_ROAM_TRIGGER_REASON_DENSE;
	case ROAM_TRIGGER_REASON_BACKGROUND:
		return WMI_ROAM_TRIGGER_REASON_BACKGROUND;
	case ROAM_TRIGGER_REASON_FORCED:
		return WMI_ROAM_TRIGGER_REASON_FORCED;
	case ROAM_TRIGGER_REASON_BTM:
		return WMI_ROAM_TRIGGER_REASON_BTM;
	case ROAM_TRIGGER_REASON_UNIT_TEST:
		return WMI_ROAM_TRIGGER_REASON_UNIT_TEST;
	case ROAM_TRIGGER_REASON_BSS_LOAD:
		return WMI_ROAM_TRIGGER_REASON_BSS_LOAD;
	case ROAM_TRIGGER_REASON_DEAUTH:
		return WMI_ROAM_TRIGGER_REASON_DEAUTH;
	case ROAM_TRIGGER_REASON_IDLE:
		return WMI_ROAM_TRIGGER_REASON_IDLE;
	case ROAM_TRIGGER_REASON_STA_KICKOUT:
		return WMI_ROAM_TRIGGER_REASON_STA_KICKOUT;
	case ROAM_TRIGGER_REASON_ESS_RSSI:
		return WMI_ROAM_TRIGGER_REASON_ESS_RSSI;
	case ROAM_TRIGGER_REASON_WTC_BTM:
		return WMI_ROAM_TRIGGER_REASON_WTC_BTM;
	case ROAM_TRIGGER_REASON_PMK_TIMEOUT:
		return WMI_ROAM_TRIGGER_REASON_PMK_TIMEOUT;
	case ROAM_TRIGGER_REASON_MAX:
		return WMI_ROAM_TRIGGER_REASON_MAX;
	default:
		return WMI_ROAM_TRIGGER_REASON_NONE;
	}
}


#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/* send_set_ric_req_cmd_tlv() - set ric request element
 * @wmi_handle: wmi handle
 * @msg: message
 * @is_add_ts: is addts required
 *
 * This function sets ric request element for 11r roaming.
 *
 * Return: CDF status
 */
static QDF_STATUS send_set_ric_req_cmd_tlv(wmi_unified_t wmi_handle,
			void *msg, uint8_t is_add_ts)
{
	wmi_ric_request_fixed_param *cmd;
	wmi_ric_tspec *tspec_param;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	struct mac_tspec_ie *tspec_ie = NULL;
	int32_t len = sizeof(wmi_ric_request_fixed_param) +
		      WMI_TLV_HDR_SIZE + sizeof(wmi_ric_tspec);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);

	cmd = (wmi_ric_request_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_ric_request_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_ric_request_fixed_param));
	if (is_add_ts)
		cmd->vdev_id = ((struct add_ts_param *)msg)->vdev_id;
	else
		cmd->vdev_id = ((struct del_ts_params *)msg)->sessionId;
	cmd->num_ric_request = 1;
	cmd->is_add_ric = is_add_ts;

	buf_ptr += sizeof(wmi_ric_request_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, sizeof(wmi_ric_tspec));

	buf_ptr += WMI_TLV_HDR_SIZE;
	tspec_param = (wmi_ric_tspec *)buf_ptr;
	WMITLV_SET_HDR(&tspec_param->tlv_header,
		       WMITLV_TAG_STRUC_wmi_ric_tspec,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_ric_tspec));

	if (is_add_ts)
		tspec_ie = &(((struct add_ts_param *)msg)->tspec);
	else
		tspec_ie = &(((struct del_ts_params *)msg)->delTsInfo.tspec);
	if (tspec_ie) {
		/* Fill the tsinfo in the format expected by firmware */
#ifndef ANI_LITTLE_BIT_ENDIAN
		qdf_mem_copy(((uint8_t *)&tspec_param->ts_info) + 1,
			     ((uint8_t *)&tspec_ie->tsinfo) + 1, 2);
#else
		qdf_mem_copy(((uint8_t *)&tspec_param->ts_info),
			     ((uint8_t *)&tspec_ie->tsinfo) + 1, 2);
#endif /* ANI_LITTLE_BIT_ENDIAN */

		tspec_param->nominal_msdu_size = tspec_ie->nomMsduSz;
		tspec_param->maximum_msdu_size = tspec_ie->maxMsduSz;
		tspec_param->min_service_interval = tspec_ie->minSvcInterval;
		tspec_param->max_service_interval = tspec_ie->maxSvcInterval;
		tspec_param->inactivity_interval = tspec_ie->inactInterval;
		tspec_param->suspension_interval = tspec_ie->suspendInterval;
		tspec_param->svc_start_time = tspec_ie->svcStartTime;
		tspec_param->min_data_rate = tspec_ie->minDataRate;
		tspec_param->mean_data_rate = tspec_ie->meanDataRate;
		tspec_param->peak_data_rate = tspec_ie->peakDataRate;
		tspec_param->max_burst_size = tspec_ie->maxBurstSz;
		tspec_param->delay_bound = tspec_ie->delayBound;
		tspec_param->min_phy_rate = tspec_ie->minPhyRate;
		tspec_param->surplus_bw_allowance = tspec_ie->surplusBw;
		tspec_param->medium_time = 0;
	}
	wmi_debug("Set RIC Req is_add_ts: %d", is_add_ts);

	wmi_mtrace(WMI_ROAM_SET_RIC_REQUEST_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_ROAM_SET_RIC_REQUEST_CMDID)) {
		wmi_err("Failed to send vdev Set RIC Req command");
		if (is_add_ts)
			((struct add_ts_param *)msg)->status =
					    QDF_STATUS_E_FAILURE;
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_process_roam_synch_complete_cmd_tlv() - roam synch complete command to
 * fw.
 * @wmi_handle: wmi handle
 * @vdev_id: vdev id
 *
 * This function sends roam synch complete event to fw.
 *
 * Return: QDF STATUS
 */
static QDF_STATUS
send_process_roam_synch_complete_cmd_tlv(wmi_unified_t wmi_handle,
					 uint8_t vdev_id)
{
	wmi_roam_synch_complete_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint8_t *buf_ptr;
	uint16_t len;
	len = sizeof(wmi_roam_synch_complete_fixed_param);

	wmi_buf = wmi_buf_alloc(wmi_handle, len);
	if (!wmi_buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_roam_synch_complete_fixed_param *)wmi_buf_data(wmi_buf);
	buf_ptr = (uint8_t *)cmd;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_synch_complete_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_roam_synch_complete_fixed_param));
	cmd->vdev_id = vdev_id;
	wmi_mtrace(WMI_ROAM_SYNCH_COMPLETE, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, wmi_buf, len,
				 WMI_ROAM_SYNCH_COMPLETE)) {
		wmi_err("Failed to send roam synch confirmation");
		wmi_buf_free(wmi_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_roam_invoke_cmd_tlv() - send roam invoke command to fw.
 * @wmi_handle: wma handle
 * @roaminvoke: roam invoke command
 *
 * Send roam invoke command to fw for fastreassoc.
 *
 * Return: CDF STATUS
 */
static QDF_STATUS send_roam_invoke_cmd_tlv(wmi_unified_t wmi_handle,
		struct wmi_roam_invoke_cmd *roaminvoke,
		uint32_t ch_hz)
{
	wmi_roam_invoke_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	u_int8_t *buf_ptr;
	u_int16_t len, args_tlv_len;
	uint32_t *channel_list;
	wmi_mac_addr *bssid_list;
	wmi_tlv_buf_len_param *buf_len_tlv;

	args_tlv_len = (4 * WMI_TLV_HDR_SIZE) + sizeof(uint32_t) +
			sizeof(wmi_mac_addr) + sizeof(wmi_tlv_buf_len_param) +
			roundup(roaminvoke->frame_len, sizeof(uint32_t));
	len = sizeof(wmi_roam_invoke_cmd_fixed_param) + args_tlv_len;
	wmi_buf = wmi_buf_alloc(wmi_handle, len);
	if (!wmi_buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_roam_invoke_cmd_fixed_param *)wmi_buf_data(wmi_buf);
	buf_ptr = (u_int8_t *)cmd;
	WMITLV_SET_HDR(&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_roam_invoke_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN(wmi_roam_invoke_cmd_fixed_param));
	cmd->vdev_id = roaminvoke->vdev_id;
	cmd->flags |= (1 << WMI_ROAM_INVOKE_FLAG_REPORT_FAILURE);
	if (roaminvoke->is_same_bssid)
		cmd->flags |= (1 << WMI_ROAM_INVOKE_FLAG_NO_NULL_FRAME_TO_AP);

	if (roaminvoke->frame_len) {
		cmd->roam_scan_mode = WMI_ROAM_INVOKE_SCAN_MODE_SKIP;
		/* packing 1 beacon/probe_rsp frame with WMI cmd */
		cmd->num_buf = 1;
	} else {
		cmd->roam_scan_mode = WMI_ROAM_INVOKE_SCAN_MODE_FIXED_CH;
		cmd->num_buf = 0;
	}

	cmd->roam_ap_sel_mode = 0;
	cmd->roam_delay = 0;
	cmd->num_chan = 1;
	cmd->num_bssid = 1;

	if (roaminvoke->forced_roaming) {
		cmd->num_chan = 0;
		cmd->num_bssid = 0;
		cmd->roam_scan_mode = WMI_ROAM_INVOKE_SCAN_MODE_CACHE_MAP;
		cmd->flags |=
			(1 << WMI_ROAM_INVOKE_FLAG_FULL_SCAN_IF_NO_CANDIDATE);
		cmd->reason = ROAM_INVOKE_REASON_NUD_FAILURE;
	} else {
		cmd->reason = ROAM_INVOKE_REASON_USER_SPACE;
	}

	buf_ptr += sizeof(wmi_roam_invoke_cmd_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       (sizeof(u_int32_t)));
	channel_list = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	*channel_list = ch_hz;
	buf_ptr += sizeof(uint32_t) + WMI_TLV_HDR_SIZE;
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_FIXED_STRUC,
		       (sizeof(wmi_mac_addr)));
	bssid_list = (wmi_mac_addr *)(buf_ptr + WMI_TLV_HDR_SIZE);
	WMI_CHAR_ARRAY_TO_MAC_ADDR(roaminvoke->bssid, bssid_list);

	/* move to next tlv i.e. bcn_prb_buf_list */
	buf_ptr += WMI_TLV_HDR_SIZE + sizeof(wmi_mac_addr);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_FIXED_STRUC,
		       sizeof(wmi_tlv_buf_len_param));

	buf_len_tlv = (wmi_tlv_buf_len_param *)(buf_ptr + WMI_TLV_HDR_SIZE);
	WMITLV_SET_HDR(&buf_len_tlv->tlv_header,
		       WMITLV_TAG_STRUC_wmi_tlv_buf_len_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_tlv_buf_len_param));
	buf_len_tlv->buf_len = roaminvoke->frame_len;

	/* move to next tlv i.e. bcn_prb_frm */
	buf_ptr += WMI_TLV_HDR_SIZE + sizeof(wmi_tlv_buf_len_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       roundup(roaminvoke->frame_len, sizeof(uint32_t)));

	/* copy frame after the header */
	qdf_mem_copy(buf_ptr + WMI_TLV_HDR_SIZE,
		     roaminvoke->frame_buf,
		     roaminvoke->frame_len);

	wmi_debug("flag:%d, MODE:%d, ap:%d, dly:%d, n_ch:%d, n_bssid:%d, ch:%d, is_same_bss:%d",
		  cmd->flags, cmd->roam_scan_mode,
		  cmd->roam_ap_sel_mode, cmd->roam_delay,
		  cmd->num_chan, cmd->num_bssid, ch_hz,
		  roaminvoke->is_same_bssid);

	wmi_mtrace(WMI_ROAM_INVOKE_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, wmi_buf, len,
				 WMI_ROAM_INVOKE_CMDID)) {
		wmi_err("Failed to send roam invoke command");
		wmi_buf_free(wmi_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * convert_control_roam_trigger_reason_bitmap() - Convert roam trigger bitmap
 *
 * @trigger_reason_bitmap: Roam trigger reason bitmap received from upper layers
 *
 * Converts the controlled roam trigger reason bitmap of
 * type @roam_control_trigger_reason to firmware trigger
 * reason bitmap as defined in
 * trigger_reason_bitmask @wmi_roam_enable_disable_trigger_reason_fixed_param
 *
 * Return: trigger_reason_bitmask as defined in
 *	   wmi_roam_enable_disable_trigger_reason_fixed_param
 */
static uint32_t
convert_control_roam_trigger_reason_bitmap(uint32_t trigger_reason_bitmap)
{
	uint32_t fw_trigger_bitmap = 0, all_bitmap;

	/* Enable the complete trigger bitmap when all bits are set in
	 * the control config bitmap
	 */
	all_bitmap = BIT(ROAM_TRIGGER_REASON_MAX) - 1;
	if (trigger_reason_bitmap == all_bitmap)
		return BIT(WMI_ROAM_TRIGGER_EXT_REASON_MAX) - 1;

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_NONE))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_NONE);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_PER))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_PER);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_BMISS))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_BMISS);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_LOW_RSSI))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_LOW_RSSI);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_HIGH_RSSI))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_HIGH_RSSI);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_PERIODIC))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_PERIODIC);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_MAWC))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_MAWC);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_DENSE))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_DENSE);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_BACKGROUND))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_BACKGROUND);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_FORCED))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_FORCED);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_BTM))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_BTM);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_UNIT_TEST))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_UNIT_TEST);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_BSS_LOAD))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_BSS_LOAD);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_DEAUTH))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_DEAUTH);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_IDLE))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_IDLE);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_STA_KICKOUT))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_STA_KICKOUT);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_ESS_RSSI))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_ESS_RSSI);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_WTC_BTM))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_WTC_BTM);

	if (trigger_reason_bitmap & BIT(ROAM_TRIGGER_REASON_PMK_TIMEOUT))
		fw_trigger_bitmap |= BIT(WMI_ROAM_TRIGGER_REASON_PMK_TIMEOUT);

	return fw_trigger_bitmap;
}

/**
 * get_internal_mandatory_roam_triggers() - Internal triggers to be added
 *
 * Return: the bitmap of mandatory triggers to be sent to firmware but not given
 * by user.
 */
static uint32_t
get_internal_mandatory_roam_triggers(void)
{
	return BIT(WMI_ROAM_TRIGGER_REASON_FORCED);
}

/**
 * convert_roam_trigger_scan_mode() - Function to convert unified Roam trigger
 * scan mode enum to TLV specific ROAM_TRIGGER_SCAN_MODE
 * @scan_freq_scheme: scan freq scheme coming from userspace
 *
 * Return: ROAM_TRIGGER_SCAN_MODE
 */
static WMI_ROAM_TRIGGER_SCAN_MODE
convert_roam_trigger_scan_mode(enum roam_scan_freq_scheme scan_freq_scheme)
{
	switch (scan_freq_scheme) {
	case ROAM_SCAN_FREQ_SCHEME_NO_SCAN:
		return ROAM_TRIGGER_SCAN_MODE_NO_SCAN_DISCONNECTION;
	case ROAM_SCAN_FREQ_SCHEME_PARTIAL_SCAN:
		return ROAM_TRIGGER_SCAN_MODE_PARTIAL;
	case ROAM_SCAN_FREQ_SCHEME_FULL_SCAN:
		return ROAM_TRIGGER_SCAN_MODE_FULL;
	default:
		return ROAM_TRIGGER_SCAN_MODE_NONE;
	}
}

/**
 * wmi_fill_default_roam_trigger_parameters() - Fill the default parameters
 * for wmi_configure_roam_trigger_parameters tlv.
 * @roam_trigger_params: pointer to wmi_configure_roam_trigger_parameters tlv
 * to be filled.
 * @roam_trigger: Roam trigger reason
 *
 * Return: None
 */
static void wmi_fill_default_roam_trigger_parameters(
		wmi_configure_roam_trigger_parameters *roam_trigger_params,
		uint32_t roam_trigger)
{
	WMITLV_SET_HDR(&roam_trigger_params->tlv_header,
		       WMITLV_TAG_STRUC_wmi_configure_roam_trigger_parameters,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_configure_roam_trigger_parameters));

	roam_trigger_params->trigger_reason = roam_trigger;
	roam_trigger_params->enable = 1;
	roam_trigger_params->scan_mode = ROAM_TRIGGER_SCAN_MODE_NONE;
	roam_trigger_params->trigger_rssi_threshold =
			ROAM_MAX_CFG_VALUE;
	roam_trigger_params->cand_ap_min_rssi_threshold =
			ROAM_MAX_CFG_VALUE;
	roam_trigger_params->cand_ap_min_rssi_threshold_5g =
			ROAM_MAX_CFG_VALUE;
	roam_trigger_params->cand_ap_min_rssi_threshold_6g =
			ROAM_MAX_CFG_VALUE;
	roam_trigger_params->roam_score_delta_percentage =
			ROAM_MAX_CFG_VALUE;
	roam_trigger_params->reason_code = ROAM_MAX_CFG_VALUE;
}

static void wmi_fill_score_delta_params(
		wmi_configure_roam_trigger_parameters *roam_trigger_params,
		struct wlan_roam_triggers *triggers,
		uint8_t trig_index)
{
	enum roam_trigger_reason trig_reason;

	if (trig_index >= NUM_OF_ROAM_TRIGGERS)
		return;

	trig_reason =
		triggers->score_delta_param[trig_index].trigger_reason;
	wmi_fill_default_roam_trigger_parameters(
		roam_trigger_params,
		convert_roam_trigger_reason(trig_reason));
	roam_trigger_params->roam_score_delta_percentage =
		triggers->score_delta_param[trig_index].roam_score_delta;

	wmi_debug("RSO_CFG: Score delta per: %d converted trig_reason: %d",
		  roam_trigger_params->roam_score_delta_percentage,
		  convert_roam_trigger_reason(trig_reason));

}

static void wmi_fill_min_rssi_params(
		wmi_configure_roam_trigger_parameters *roam_trigger_params,
		struct wlan_roam_triggers *triggers,
		uint8_t trig_index)
{
	enum roam_trigger_reason trig_reason;

	if (trig_index >= NUM_OF_ROAM_MIN_RSSI)
		return;

	trig_reason =
		triggers->min_rssi_params[trig_index].trigger_reason;
	wmi_fill_default_roam_trigger_parameters(
		roam_trigger_params,
		convert_roam_trigger_reason(trig_reason));
	roam_trigger_params->cand_ap_min_rssi_threshold =
		triggers->min_rssi_params[trig_index].min_rssi;
	roam_trigger_params->cand_ap_min_rssi_threshold_5g =
		triggers->min_rssi_params[trig_index].min_rssi;
	roam_trigger_params->cand_ap_min_rssi_threshold_6g =
		triggers->min_rssi_params[trig_index].min_rssi;

	wmi_debug("RSO_CFG: Min rssi thresh: %d converted trig_reason: %d",
		  roam_trigger_params->cand_ap_min_rssi_threshold,
		  convert_roam_trigger_reason(trig_reason));
}

/**
 * send_set_roam_trigger_cmd_tlv() - send set roam triggers to fw
 * @wmi_handle: wmi handle
 * @vdev_id: vdev id
 * @trigger_bitmap: roam trigger bitmap to be enabled
 *
 * Send WMI_ROAM_ENABLE_DISABLE_TRIGGER_REASON_CMDID to fw.
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS send_set_roam_trigger_cmd_tlv(wmi_unified_t wmi_handle,
					struct wlan_roam_triggers *triggers)
{
	wmi_buf_t buf;
	wmi_roam_enable_disable_trigger_reason_fixed_param *cmd;
	uint32_t len = sizeof(*cmd);
	int ret;
	uint8_t *buf_ptr;
	wmi_configure_roam_trigger_parameters
					*roam_trigger_parameters;
	uint32_t num_triggers_enabled = 0;
	uint32_t roam_scan_scheme_bitmap = triggers->roam_scan_scheme_bitmap;
	uint32_t total_tlv_len = 0;

	if (BIT(ROAM_TRIGGER_REASON_PER) & roam_scan_scheme_bitmap)
		num_triggers_enabled++;

	if (BIT(ROAM_TRIGGER_REASON_BMISS) & roam_scan_scheme_bitmap)
		num_triggers_enabled++;

	if (BIT(ROAM_TRIGGER_REASON_LOW_RSSI) & roam_scan_scheme_bitmap)
		num_triggers_enabled++;

	if (BIT(ROAM_TRIGGER_REASON_BTM) & roam_scan_scheme_bitmap)
		num_triggers_enabled++;

	if (BIT(ROAM_TRIGGER_REASON_BSS_LOAD) & roam_scan_scheme_bitmap)
		num_triggers_enabled++;

	if (wmi_service_enabled(wmi_handle,
			wmi_service_configure_roam_trigger_param_support))
		total_tlv_len += (NUM_OF_ROAM_TRIGGERS + NUM_OF_ROAM_MIN_RSSI) *
			sizeof(wmi_configure_roam_trigger_parameters);

	total_tlv_len += 2 * sizeof(wmi_configure_roam_trigger_parameters) +
			num_triggers_enabled *
			sizeof(wmi_configure_roam_trigger_parameters);
	len += WMI_TLV_HDR_SIZE + total_tlv_len;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("Failed to allocate wmi buffer");
		return QDF_STATUS_E_NOMEM;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);

	cmd = (wmi_roam_enable_disable_trigger_reason_fixed_param *)
					wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_roam_enable_disable_trigger_reason_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN(wmi_roam_enable_disable_trigger_reason_fixed_param));

	cmd->vdev_id = triggers->vdev_id;
	cmd->trigger_reason_bitmask =
	   convert_control_roam_trigger_reason_bitmap(triggers->trigger_bitmap);
	wmi_debug("RSO_CFG: Received trigger bitmap: 0x%x converted trigger_bitmap: 0x%x",
		  triggers->trigger_bitmap, cmd->trigger_reason_bitmask);
	cmd->trigger_reason_bitmask |= get_internal_mandatory_roam_triggers();
	wmi_debug("RSO_CFG: vdev id: %d final trigger_bitmap: 0x%x roam_scan_scheme:0x%x num_triggers_enabled:%d",
		  cmd->vdev_id, cmd->trigger_reason_bitmask,
		  roam_scan_scheme_bitmap, num_triggers_enabled);

	buf_ptr += sizeof(wmi_roam_enable_disable_trigger_reason_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, total_tlv_len);
	buf_ptr += WMI_TLV_HDR_SIZE;

	roam_trigger_parameters =
		(wmi_configure_roam_trigger_parameters *)buf_ptr;

	WMITLV_SET_HDR(&roam_trigger_parameters->tlv_header,
		WMITLV_TAG_STRUC_wmi_configure_roam_trigger_parameters,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_configure_roam_trigger_parameters));
	roam_trigger_parameters->trigger_reason =
			WMI_ROAM_TRIGGER_REASON_WTC_BTM;
	if (triggers->vendor_btm_param.user_roam_reason == 0)
		roam_trigger_parameters->enable = 1;
	roam_trigger_parameters->scan_mode = convert_roam_trigger_scan_mode(
				triggers->vendor_btm_param.scan_freq_scheme);
	roam_trigger_parameters->trigger_rssi_threshold =
		triggers->vendor_btm_param.connected_rssi_threshold;
	roam_trigger_parameters->cand_ap_min_rssi_threshold =
		triggers->vendor_btm_param.candidate_rssi_threshold_2g;
	roam_trigger_parameters->cand_ap_min_rssi_threshold_5g =
		triggers->vendor_btm_param.candidate_rssi_threshold_5g;
	roam_trigger_parameters->cand_ap_min_rssi_threshold_6g =
		triggers->vendor_btm_param.candidate_rssi_threshold_6g;
	roam_trigger_parameters->roam_score_delta_percentage =
			triggers->roam_score_delta;
	roam_trigger_parameters->reason_code =
			triggers->vendor_btm_param.user_roam_reason;
	roam_trigger_parameters++;

	if (wmi_service_enabled(wmi_handle,
			wmi_service_configure_roam_trigger_param_support)) {
		wmi_fill_score_delta_params(roam_trigger_parameters,
					    triggers,
					    IDLE_ROAM_TRIGGER);
		roam_trigger_parameters++;

		wmi_fill_score_delta_params(roam_trigger_parameters,
					    triggers,
					    BTM_ROAM_TRIGGER);
		roam_trigger_parameters++;

		wmi_fill_min_rssi_params(roam_trigger_parameters,
					 triggers,
					 DEAUTH_MIN_RSSI);
		roam_trigger_parameters++;

		wmi_fill_min_rssi_params(roam_trigger_parameters,
					 triggers,
					 BMISS_MIN_RSSI);
		roam_trigger_parameters++;

		wmi_fill_min_rssi_params(roam_trigger_parameters,
					 triggers,
					 MIN_RSSI_2G_TO_5G_ROAM);
		roam_trigger_parameters++;
	}

	wmi_fill_default_roam_trigger_parameters(
			roam_trigger_parameters,
			WMI_ROAM_TRIGGER_REASON_PMK_TIMEOUT);

	if (cmd->trigger_reason_bitmask & BIT(WMI_ROAM_TRIGGER_REASON_PMK_TIMEOUT))
		roam_trigger_parameters->enable = 1;
	else
		roam_trigger_parameters->enable = 0;

	roam_trigger_parameters->roam_score_delta_percentage = 0;
	roam_trigger_parameters++;

	if (num_triggers_enabled == 0)
		goto send;

	if (BIT(ROAM_TRIGGER_REASON_PER) & roam_scan_scheme_bitmap) {
		wmi_fill_default_roam_trigger_parameters(
				roam_trigger_parameters,
				WMI_ROAM_TRIGGER_REASON_PER);
		roam_trigger_parameters->scan_mode =
			ROAM_TRIGGER_SCAN_MODE_PARTIAL;

		roam_trigger_parameters++;
	}

	if (BIT(ROAM_TRIGGER_REASON_BMISS) & roam_scan_scheme_bitmap) {
		wmi_fill_default_roam_trigger_parameters(
				roam_trigger_parameters,
				WMI_ROAM_TRIGGER_REASON_BMISS);
		roam_trigger_parameters->scan_mode =
				ROAM_TRIGGER_SCAN_MODE_PARTIAL;

		roam_trigger_parameters++;
	}

	if (BIT(ROAM_TRIGGER_REASON_LOW_RSSI) & roam_scan_scheme_bitmap) {
		wmi_fill_default_roam_trigger_parameters(
				roam_trigger_parameters,
				WMI_ROAM_TRIGGER_REASON_LOW_RSSI);
		roam_trigger_parameters->scan_mode =
				ROAM_TRIGGER_SCAN_MODE_PARTIAL;

		roam_trigger_parameters++;
	}

	if (BIT(ROAM_TRIGGER_REASON_BTM) & roam_scan_scheme_bitmap) {
		wmi_fill_default_roam_trigger_parameters(
				roam_trigger_parameters,
				WMI_ROAM_TRIGGER_REASON_BTM);
		roam_trigger_parameters->scan_mode =
				ROAM_TRIGGER_SCAN_MODE_PARTIAL;

		roam_trigger_parameters++;
	}

	if (BIT(ROAM_TRIGGER_REASON_BSS_LOAD) & roam_scan_scheme_bitmap) {
		wmi_fill_default_roam_trigger_parameters(
				roam_trigger_parameters,
				WMI_ROAM_TRIGGER_REASON_BSS_LOAD);
		roam_trigger_parameters->scan_mode =
				ROAM_TRIGGER_SCAN_MODE_PARTIAL;

		roam_trigger_parameters++;
	}

send:
	wmi_mtrace(WMI_ROAM_ENABLE_DISABLE_TRIGGER_REASON_CMDID,
		   triggers->vdev_id, 0);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				WMI_ROAM_ENABLE_DISABLE_TRIGGER_REASON_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send set roam triggers command ret = %d",
			ret);
		wmi_buf_free(buf);
	}
	return ret;
}

/**
 * send_vdev_set_pcl_cmd_tlv() - Send WMI_VDEV_SET_PCL_CMDID to FW
 * @wmi_handle: wmi handle
 * @params: Set VDEV PCL params
 *
 * WMI_VDEV_SET_PCL_CMDID provides the Preferred Channel List (PCL) to WLAN
 * firmware. The roaming module is the consumer of this information
 * in the WLAN firmware. The channel list will be used when a VDEV needs
 * to migrate to a new channel without host driver involvement. An example of
 * this behavior is Legacy Fast Roaming (LFR 3.0).
 *
 * WMI_VDEV_SET_PCL_CMDID will carry only the weight list and not the actual
 * channel list. The weights corresponds to the channels sent in
 * WMI_SCAN_CHAN_LIST_CMDID. The channels from PCL would be having a higher
 * weightage compared to the non PCL channels.
 *
 * When roaming is enabled on STA 1, PDEV pcl will be sent. When STA2 is
 * up, VDEV pcl will be sent on STA 1 after calculating pcl again applying
 * the bandmask and VDEV pcl will be sent for STA2. When one of the STA
 * is disconnected, PDEV pcl will be sent on the other STA again.
 *
 * Return: Success if the cmd is sent successfully to the firmware
 */
static QDF_STATUS
send_vdev_set_pcl_cmd_tlv(wmi_unified_t wmi_handle,
			  struct set_pcl_cmd_params *params)
{
	wmi_vdev_set_pcl_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint32_t *ch_weight, i;
	size_t len;
	uint32_t chan_len;

	chan_len = params->weights->saved_num_chan;
	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE + (chan_len * sizeof(uint32_t));

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_vdev_set_pcl_cmd_fixed_param *)wmi_buf_data(buf);
	buf_ptr = (uint8_t *)cmd;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_vdev_set_pcl_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_set_pcl_cmd_fixed_param));
	cmd->vdev_id = params->vdev_id;
	buf_ptr += sizeof(wmi_vdev_set_pcl_cmd_fixed_param);

	/* Channel weights uint32 Array TLV */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       (chan_len * sizeof(uint32_t)));
	ch_weight = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; i < chan_len; i++)
		ch_weight[i] = params->weights->weighed_valid_list[i];

	wmi_mtrace(WMI_VDEV_SET_PCL_CMDID, params->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_VDEV_SET_PCL_CMDID)) {
		wmi_err("Failed to send WMI_VDEV_SET_PCL_CMDID");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_roam_btm_response_stats_tlv() - Extract the btm rsp stats
 * from the WMI_ROAM_STATS_EVENTID
 * @wmi_handle: wmi handle
 * @evt_buf:    Pointer to the event buffer
 * @dst:        Pointer to destination structure to fill data
 * @idx:        TLV id
 */
static QDF_STATUS
extract_roam_btm_response_stats_tlv(wmi_unified_t wmi_handle, void *evt_buf,
				    struct roam_btm_response_data *dst,
				    uint8_t idx)
{
	WMI_ROAM_STATS_EVENTID_param_tlvs *param_buf;
	wmi_roam_btm_response_info *src_data = NULL;

	param_buf = (WMI_ROAM_STATS_EVENTID_param_tlvs *)evt_buf;

	if (!param_buf || !param_buf->roam_btm_response_info ||
	    !param_buf->num_roam_btm_response_info ||
	    idx >= param_buf->num_roam_btm_response_info) {
		wmi_debug("Empty btm response param buf");
		return QDF_STATUS_SUCCESS;
	}

	src_data = &param_buf->roam_btm_response_info[idx];

	dst->present = true;
	dst->btm_status = src_data->btm_status;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&src_data->target_bssid,
				   dst->target_bssid.bytes);
	dst->vsie_reason = src_data->vsie_reason;
	dst->timestamp = src_data->timestamp;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_roam_initial_info_tlv() - Extract the roam initial info
 * from the WMI_ROAM_STATS_EVENTID
 * @wmi_handle: wmi handle
 * @evt_buf:    Pointer to the event buffer
 * @dst:        Pointer to destination structure to fill data
 * @idx:        TLV id
 */
static QDF_STATUS
extract_roam_initial_info_tlv(wmi_unified_t wmi_handle, void *evt_buf,
			      struct roam_initial_data *dst, uint8_t idx)
{
	WMI_ROAM_STATS_EVENTID_param_tlvs *param_buf;
	wmi_roam_initial_info *src_data = NULL;

	param_buf = (WMI_ROAM_STATS_EVENTID_param_tlvs *)evt_buf;

	if (!param_buf || !param_buf->roam_initial_info ||
	    !param_buf->num_roam_initial_info ||
	    idx >= param_buf->num_roam_initial_info) {
		wmi_debug("Empty roam_initial_info param buf");
		return QDF_STATUS_SUCCESS;
	}

	src_data = &param_buf->roam_initial_info[idx];

	dst->present = true;
	dst->roam_full_scan_count = src_data->roam_full_scan_count;
	dst->rssi_th = src_data->rssi_th;
	dst->cu_th = src_data->cu_th;
	dst->fw_cancel_timer_bitmap = src_data->timer_canceled;

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_roam_msg_info_tlv() - Extract the roam message info
 * from the WMI_ROAM_STATS_EVENTID
 * @wmi_handle: wmi handle
 * @evt_buf:    Pointer to the event buffer
 * @dst:        Pointer to destination structure to fill data
 * @idx:        TLV id
 */
static QDF_STATUS
extract_roam_msg_info_tlv(wmi_unified_t wmi_handle, void *evt_buf,
			  struct roam_msg_info *dst, uint8_t idx)
{
	WMI_ROAM_STATS_EVENTID_param_tlvs *param_buf;
	wmi_roam_msg_info *src_data = NULL;

	param_buf = (WMI_ROAM_STATS_EVENTID_param_tlvs *)evt_buf;

	if (!param_buf || !param_buf->roam_msg_info ||
	    !param_buf->num_roam_msg_info ||
	    idx >= param_buf->num_roam_msg_info) {
		wmi_debug("Empty roam_msg_info param buf");
		return QDF_STATUS_SUCCESS;
	}

	src_data = &param_buf->roam_msg_info[idx];

	dst->present = true;
	dst->timestamp = src_data->timestamp;
	dst->msg_id = src_data->msg_id;
	dst->msg_param1 = src_data->msg_param1;
	dst->msg_param2 = src_data->msg_param2;

	return QDF_STATUS_SUCCESS;
}

void wmi_roam_offload_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->extract_roam_btm_response_stats =
				extract_roam_btm_response_stats_tlv;
	ops->extract_roam_initial_info = extract_roam_initial_info_tlv;
	ops->extract_roam_msg_info = extract_roam_msg_info_tlv;

	ops->send_set_ric_req_cmd = send_set_ric_req_cmd_tlv;
	ops->send_process_roam_synch_complete_cmd =
			send_process_roam_synch_complete_cmd_tlv;
	ops->send_roam_invoke_cmd = send_roam_invoke_cmd_tlv;
	ops->send_vdev_set_pcl_cmd = send_vdev_set_pcl_cmd_tlv;
	ops->send_set_roam_trigger_cmd = send_set_roam_trigger_cmd_tlv;
}
#else
static inline QDF_STATUS
extract_roam_btm_response_stats_tlv(wmi_unified_t wmi_handle, void *evt_buf,
				    struct roam_btm_response_data *dst,
				    uint8_t idx)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
extract_roam_initial_info_tlv(wmi_unified_t wmi_handle, void *evt_buf,
			      struct roam_initial_data *dst, uint8_t idx)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static inline QDF_STATUS
extract_roam_msg_info_tlv(wmi_unified_t wmi_handle, void *evt_buf,
			  struct roam_msg_info *dst, uint8_t idx)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

#define ROAM_OFFLOAD_PMK_EXT_BYTES 16

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * wmi_fill_roam_offload_11r_params() - Fill roam scan params to send it to fw
 * @akm: Authentication key management type
 * @roam_offload_11r: TLV to be filled with 11r params
 * @roam_req: roam request param
 */
static void wmi_fill_roam_offload_11r_params(
		uint32_t akm,
		wmi_roam_11r_offload_tlv_param *roam_offload_11r,
		struct wlan_roam_scan_offload_params *roam_req)
{
	struct wlan_rso_11r_params *src_11r_params;
	uint8_t *psk_msk, len;

	src_11r_params = &roam_req->rso_11r_info;

	if (akm == WMI_AUTH_FT_RSNA_FILS_SHA256 ||
	    akm == WMI_AUTH_FT_RSNA_FILS_SHA384) {
		psk_msk = roam_req->fils_roam_config.fils_ft;
		len = roam_req->fils_roam_config.fils_ft_len;
	} else {
		psk_msk = src_11r_params->psk_pmk;
		len = src_11r_params->pmk_len;
	}

	/*
	 * For SHA384 based akm, the pmk length is 48 bytes. So fill
	 * first 32 bytes in roam_offload_11r->psk_msk and the remaining
	 * bytes in roam_offload_11r->psk_msk_ext buffer
	 */
	roam_offload_11r->psk_msk_len = len > ROAM_OFFLOAD_PSK_MSK_BYTES ?
					ROAM_OFFLOAD_PSK_MSK_BYTES : len;
	qdf_mem_copy(roam_offload_11r->psk_msk, psk_msk,
		     roam_offload_11r->psk_msk_len);
	roam_offload_11r->psk_msk_ext_len = 0;

	if (len > ROAM_OFFLOAD_PSK_MSK_BYTES) {
		roam_offload_11r->psk_msk_ext_len =
					len - roam_offload_11r->psk_msk_len;
		qdf_mem_copy(roam_offload_11r->psk_msk_ext,
			     &psk_msk[roam_offload_11r->psk_msk_len],
			     roam_offload_11r->psk_msk_ext_len);
	}
}

/**
 * wmi_is_ft_akm() - Check if the akm is FT akm. Based on the AKM 11r params
 * will be sent for lfr-3.0 roaming offload
 * @akm: AKM negotiated for the connection
 * @roam_req: roam request sent to firmware
 *
 * Return: true if the akm is 11r based
 */
static bool wmi_is_ft_akm(int akm,
			  struct wlan_roam_scan_offload_params *roam_req)
{
	switch (akm) {
	case WMI_AUTH_FT_RSNA:
	case WMI_AUTH_FT_RSNA_PSK:
	case WMI_AUTH_FT_RSNA_SAE:
	case WMI_AUTH_FT_RSNA_SUITE_B_8021X_SHA384:
	case WMI_AUTH_FT_RSNA_FILS_SHA256:
	case WMI_AUTH_FT_RSNA_FILS_SHA384:
		return true;
	case WMI_AUTH_OPEN:
		if (roam_req->rso_11r_info.mdid.mdie_present &&
		    roam_req->rso_11r_info.is_11r_assoc)
			return true;

		break;
	default:
		return false;
	}

	return false;
}

/**
 * wmi_get_rso_cmd_buf_len() - calculate the length needed to allocate buffer
 * for RSO mode command
 * @roam_req: roam request parameters
 */
static uint32_t
wmi_get_rso_buf_len(struct wlan_roam_scan_offload_params *roam_req)
{
	wmi_tlv_buf_len_param *assoc_ies;
	uint32_t buf_len;
	uint32_t fils_tlv_len = 0;
	int akm = roam_req->akm;

	/*
	 * Allocate room for wmi_roam_offload_tlv_param and
	 * 11i or 11r or ese roam offload tlv param
	 * Todo: Test if below headroom of 2 TLV header is needed
	 */
	buf_len = (2 * WMI_TLV_HDR_SIZE);

	if (roam_req->is_rso_stop ||
	    !roam_req->roam_offload_enabled) {
		buf_len += (4 * WMI_TLV_HDR_SIZE);

		if (!roam_req->is_rso_stop)
			wmi_debug("vdev[%d]: %s roam offload: %d",
				  roam_req->vdev_id,
				  roam_req->is_rso_stop ? "RSO stop cmd." : "",
				  roam_req->roam_offload_enabled);

		return buf_len;
	}

	wmi_debug("wmi akm = %d", akm);

	buf_len += sizeof(wmi_roam_offload_tlv_param);
	buf_len += 2 * WMI_TLV_HDR_SIZE;

	if ((akm != WMI_AUTH_OPEN || roam_req->rso_ese_info.is_ese_assoc ||
	     wmi_is_ft_akm(akm, roam_req)) && akm != WMI_AUTH_NONE) {
		if (roam_req->rso_ese_info.is_ese_assoc)
			buf_len += sizeof(wmi_roam_ese_offload_tlv_param);
		else if (wmi_is_ft_akm(akm, roam_req))
			buf_len += sizeof(wmi_roam_11r_offload_tlv_param);
		else
			buf_len += sizeof(wmi_roam_11i_offload_tlv_param);
	}

	buf_len += (sizeof(*assoc_ies) + (2 * WMI_TLV_HDR_SIZE) +
		    roundup(roam_req->assoc_ie_length, sizeof(uint32_t)));

	/* Fils TLV */
	buf_len += WMI_TLV_HDR_SIZE;
	if (roam_req->add_fils_tlv) {
		fils_tlv_len = sizeof(wmi_roam_fils_offload_tlv_param);
		buf_len += fils_tlv_len;
	}

	if (roam_req->rso_11i_info.is_sae_same_pmk)
		buf_len += WMI_TLV_HDR_SIZE +
			   sizeof(wmi_roam_sae_offload_tlv_param);

	roam_req->rso_mode_info.roam_scan_mode |=
			WMI_ROAM_SCAN_MODE_ROAMOFFLOAD;

	return buf_len;
}

#if defined(WLAN_FEATURE_FILS_SK)
/**
 * wmi_add_fils_tlv() - Add FILS TLV to roam scan offload command
 * @wmi_handle: wmi handle
 * @roam_req: Roam scan offload params
 * @buf_ptr: command buffer to send
 * @fils_tlv_len: fils tlv length
 *
 * Return: Updated buffer pointer
 */
static uint8_t *wmi_add_fils_tlv(wmi_unified_t wmi_handle,
				 struct wlan_roam_scan_offload_params *roam_req,
				 uint8_t *buf_ptr, uint32_t fils_tlv_len)
{
	wmi_roam_fils_offload_tlv_param *fils_tlv;
	wmi_erp_info *erp_info;
	struct wlan_roam_fils_params *roam_fils_params;

	if (!roam_req->add_fils_tlv) {
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
		buf_ptr += WMI_TLV_HDR_SIZE;
		return buf_ptr;
	}

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(*fils_tlv));
	buf_ptr += WMI_TLV_HDR_SIZE;

	fils_tlv = (wmi_roam_fils_offload_tlv_param *)buf_ptr;
	WMITLV_SET_HDR(&fils_tlv->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_fils_offload_tlv_param,
		       WMITLV_GET_STRUCT_TLVLEN
				(wmi_roam_fils_offload_tlv_param));

	roam_fils_params = &roam_req->fils_roam_config;
	erp_info = (wmi_erp_info *)(&fils_tlv->vdev_erp_info);

	erp_info->username_length = roam_fils_params->username_length;
	qdf_mem_copy(erp_info->username, roam_fils_params->username,
		     erp_info->username_length);

	erp_info->next_erp_seq_num = roam_fils_params->next_erp_seq_num;

	erp_info->rRk_length = roam_fils_params->rrk_length;
	qdf_mem_copy(erp_info->rRk, roam_fils_params->rrk,
		     erp_info->rRk_length);

	erp_info->rIk_length = roam_fils_params->rik_length;
	qdf_mem_copy(erp_info->rIk, roam_fils_params->rik,
		     erp_info->rIk_length);

	erp_info->realm_len = roam_fils_params->realm_len;
	qdf_mem_copy(erp_info->realm, roam_fils_params->realm,
		     erp_info->realm_len);

	buf_ptr += sizeof(*fils_tlv);
	wmi_debug("RSO_CFG: ERP: usrname_len:%d next_erp_seq_num:%d rRk_len:%d rIk_len:%d realm_len:%d",
		  erp_info->username_length, erp_info->next_erp_seq_num,
		  erp_info->rRk_length, erp_info->rIk_length,
		  erp_info->realm_len);
	return buf_ptr;
}
#else
static inline
uint8_t *wmi_add_fils_tlv(wmi_unified_t wmi_handle,
			  struct wlan_roam_scan_offload_params *roam_req,
			  uint8_t *buf_ptr, uint32_t fils_tlv_len)
{
	return buf_ptr;
}
#endif

#ifdef WLAN_SAE_SINGLE_PMK
static inline void
wmi_fill_sae_single_pmk_param(struct wlan_rso_11i_params *src_11i,
			      wmi_roam_11i_offload_tlv_param *roam_offload_11i)
{
	if (src_11i->is_sae_same_pmk)
		roam_offload_11i->flags |=
			1 << WMI_ROAM_OFFLOAD_FLAG_SAE_SAME_PMKID;
}

static uint8_t *wmi_fill_sae_single_pmk_tlv(
	struct wlan_roam_scan_offload_params *roam_req, uint8_t *buf_ptr)
{
	wmi_roam_sae_offload_tlv_param *sae_offload_param;

	if (!roam_req->rso_11i_info.is_sae_same_pmk)
		return buf_ptr;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_roam_sae_offload_tlv_param));
	buf_ptr += WMI_TLV_HDR_SIZE;

	sae_offload_param = (wmi_roam_sae_offload_tlv_param *)buf_ptr;
	WMITLV_SET_HDR(&sae_offload_param->tlv_header,
	WMITLV_TAG_STRUC_wmi_roam_sae_offload_tlv_param,
	WMITLV_GET_STRUCT_TLVLEN(wmi_roam_sae_offload_tlv_param));

	sae_offload_param->spmk_timeout =
			roam_req->sae_offload_params.spmk_timeout;
	wmi_debug("spmk_timeout:%d seconds", sae_offload_param->spmk_timeout);

	buf_ptr += sizeof(*sae_offload_param);

	return buf_ptr;
}
#else
static inline void
wmi_fill_sae_single_pmk_param(struct wlan_rso_11i_params *src_11i,
			      wmi_roam_11i_offload_tlv_param *roam_offload_11i)
{}

static inline uint8_t *wmi_fill_sae_single_pmk_tlv(
		struct wlan_roam_scan_offload_params *roam_req,
		uint8_t *buf_ptr)
{
	return buf_ptr;
}

#endif

static QDF_STATUS
wmi_fill_rso_tlvs(wmi_unified_t wmi_handle, uint8_t *buf,
		  struct wlan_roam_scan_offload_params *roam_req)
{
	wmi_roam_offload_tlv_param *roam_offload_params;
	wmi_roam_11i_offload_tlv_param *roam_offload_11i;
	wmi_roam_11r_offload_tlv_param *roam_offload_11r;
	wmi_roam_ese_offload_tlv_param *roam_offload_ese;
	wmi_tlv_buf_len_param *assoc_ies;
	uint32_t fils_tlv_len = 0;
	int akm = roam_req->akm;
	struct wlan_rso_lfr3_params *src_lfr3_params =
			&roam_req->rso_lfr3_params;
	struct wlan_rso_lfr3_caps *src_lfr3_caps =
			&roam_req->rso_lfr3_caps;
	struct wlan_rso_11i_params *src_11i_info =
			&roam_req->rso_11i_info;
	struct wlan_rso_ese_params *src_ese_info =
			&roam_req->rso_ese_info;
	struct wlan_rso_11r_params *src_11r_info =
			&roam_req->rso_11r_info;

	/* For RSO stop command, dont fill 11i, 11r or ese tlv */
	if (roam_req->is_rso_stop || !roam_req->roam_offload_enabled) {
		WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC,
			       WMITLV_GET_STRUCT_TLVLEN(0));
		buf += WMI_TLV_HDR_SIZE;
		WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC,
			       WMITLV_GET_STRUCT_TLVLEN(0));
		buf += WMI_TLV_HDR_SIZE;
		WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC,
			       WMITLV_GET_STRUCT_TLVLEN(0));
		buf += WMI_TLV_HDR_SIZE;

		WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC,
			       WMITLV_GET_STRUCT_TLVLEN(0));
		buf += WMI_TLV_HDR_SIZE;
		WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC,
			       WMITLV_GET_STRUCT_TLVLEN(0));
		buf += WMI_TLV_HDR_SIZE;
		WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_BYTE,
			       WMITLV_GET_STRUCT_TLVLEN(0));

		return QDF_STATUS_SUCCESS;
	}

	WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_roam_offload_tlv_param));

	buf += WMI_TLV_HDR_SIZE;
	roam_offload_params = (wmi_roam_offload_tlv_param *)buf;
	WMITLV_SET_HDR(buf,
		       WMITLV_TAG_STRUC_wmi_roam_offload_tlv_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_roam_offload_tlv_param));

	roam_offload_params->prefer_5g = src_lfr3_params->prefer_5ghz;
	roam_offload_params->rssi_cat_gap = src_lfr3_params->roam_rssi_cat_gap;
	roam_offload_params->select_5g_margin =
			src_lfr3_params->select_5ghz_margin;
	roam_offload_params->handoff_delay_for_rx =
			src_lfr3_params->ho_delay_for_rx;
	roam_offload_params->max_mlme_sw_retries =
			src_lfr3_params->roam_retry_count;
	roam_offload_params->no_ack_timeout =
			src_lfr3_params->roam_preauth_no_ack_timeout;
	roam_offload_params->reassoc_failure_timeout =
			src_lfr3_params->reassoc_failure_timeout;
	roam_offload_params->roam_candidate_validity_time =
			src_lfr3_params->rct_validity_timer;
	roam_offload_params->roam_to_current_bss_disable =
			src_lfr3_params->disable_self_roam;
	wmi_debug("RSO_CFG: prefer_5g:%d rssi_cat_gap:%d select_5g_margin:%d ho_delay:%d max_sw_retry:%d no_ack_timeout:%d",
		  roam_offload_params->prefer_5g,
		  roam_offload_params->rssi_cat_gap,
		  roam_offload_params->select_5g_margin,
		  roam_offload_params->handoff_delay_for_rx,
		  roam_offload_params->max_mlme_sw_retries,
		  roam_offload_params->no_ack_timeout);
	wmi_debug("RSO_CFG: reassoc_fail_timeout:%d rct_validity_time:%d disable_self_roam:%d",
		  roam_offload_params->reassoc_failure_timeout,
		  roam_offload_params->roam_candidate_validity_time,
		  roam_offload_params->roam_to_current_bss_disable);

	/* Fill the capabilities */
	roam_offload_params->capability = src_lfr3_caps->capability;
	roam_offload_params->ht_caps_info = src_lfr3_caps->ht_caps_info;
	roam_offload_params->ampdu_param = src_lfr3_caps->ampdu_param;
	roam_offload_params->ht_ext_cap = src_lfr3_caps->ht_ext_cap;
	roam_offload_params->ht_txbf = src_lfr3_caps->ht_txbf;
	roam_offload_params->asel_cap = src_lfr3_caps->asel_cap;
	roam_offload_params->qos_caps = src_lfr3_caps->qos_caps;
	roam_offload_params->qos_enabled = src_lfr3_caps->qos_enabled;
	roam_offload_params->wmm_caps = src_lfr3_caps->wmm_caps;
	qdf_mem_copy((uint8_t *)roam_offload_params->mcsset,
		     (uint8_t *)src_lfr3_caps->mcsset,
		     ROAM_OFFLOAD_NUM_MCS_SET);
	wmi_debug("RSO_CFG: capability:0x%x ht_caps:0x%x ampdu_param:0%x ht_ext_cap:0x%x ht_txbf:0x%x asel_cap:0x%x qos_caps:0x%x qos_en:%d wmm_caps:0x%x",
		  roam_offload_params->capability,
		  roam_offload_params->ht_caps_info,
		  roam_offload_params->ampdu_param,
		  roam_offload_params->ht_ext_cap,
		  roam_offload_params->ht_txbf, roam_offload_params->asel_cap,
		  roam_offload_params->qos_caps,
		  roam_offload_params->qos_enabled,
		  roam_offload_params->wmm_caps);

	buf += sizeof(wmi_roam_offload_tlv_param);
	/*
	 * The TLV's are in the order of 11i, 11R, ESE. Hence,
	 * they are filled in the same order.Depending on the
	 * authentication type, the other mode TLV's are nullified
	 * and only headers are filled.
	 */
	if ((akm != WMI_AUTH_OPEN || roam_req->rso_ese_info.is_ese_assoc ||
	     wmi_is_ft_akm(akm, roam_req)) && akm != WMI_AUTH_NONE) {
		if (roam_req->rso_ese_info.is_ese_assoc) {
			/* Fill the length of 11i, 11r TLV as 0 */
			WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC, 0);
			buf += WMI_TLV_HDR_SIZE;

			WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC, 0);
			buf += WMI_TLV_HDR_SIZE;

			/* Start filling the ESE TLV */
			WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC,
				       sizeof(wmi_roam_ese_offload_tlv_param));
			buf += WMI_TLV_HDR_SIZE;
			roam_offload_ese =
					(wmi_roam_ese_offload_tlv_param *)buf;
			qdf_mem_copy(roam_offload_ese->krk, src_ese_info->krk,
				     sizeof(src_ese_info->krk));
			qdf_mem_copy(roam_offload_ese->btk, src_ese_info->btk,
				     sizeof(src_ese_info->btk));

			WMITLV_SET_HDR(&roam_offload_ese->tlv_header,
			WMITLV_TAG_STRUC_wmi_roam_ese_offload_tlv_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_roam_ese_offload_tlv_param));

			buf += sizeof(wmi_roam_ese_offload_tlv_param);
		} else if (wmi_is_ft_akm(akm, roam_req)) {
			WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC, 0);
			buf += WMI_TLV_HDR_SIZE;

			WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC,
				       sizeof(wmi_roam_11r_offload_tlv_param));
			buf += WMI_TLV_HDR_SIZE;

			roam_offload_11r =
				(wmi_roam_11r_offload_tlv_param *)buf;

			roam_offload_11r->r0kh_id_len =
				src_11r_info->r0kh_id_length;
			qdf_mem_copy(roam_offload_11r->r0kh_id,
				     src_11r_info->r0kh_id,
				     src_11r_info->r0kh_id_length);

			wmi_fill_roam_offload_11r_params(akm, roam_offload_11r,
							 roam_req);

			roam_offload_11r->mdie_present =
				src_11r_info->mdid.mdie_present;
			roam_offload_11r->mdid =
				src_11r_info->mdid.mobility_domain;
			roam_offload_11r->adaptive_11r =
				src_11r_info->is_adaptive_11r;
			roam_offload_11r->ft_im_for_deauth =
				src_11r_info->enable_ft_im_roaming;

			if (akm == WMI_AUTH_OPEN) {
				/*
				 * If FT-Open ensure pmk length
				 * and r0khid len are zero
				 */
				roam_offload_11r->r0kh_id_len = 0;
				roam_offload_11r->psk_msk_len = 0;
			}

			WMITLV_SET_HDR(&roam_offload_11r->tlv_header,
			WMITLV_TAG_STRUC_wmi_roam_11r_offload_tlv_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_roam_11r_offload_tlv_param));

			buf += sizeof(wmi_roam_11r_offload_tlv_param);
			/* Set ESE TLV len to 0*/
			WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC, 0);
			buf += WMI_TLV_HDR_SIZE;

			wmi_debug("RSO_CFG: vdev[%d] 11r TLV psk_msk_len = %d psk_msk_ext:%d md:0x%x",
				  roam_req->vdev_id,
				  roam_offload_11r->psk_msk_len,
				  roam_offload_11r->psk_msk_ext_len,
				  roam_offload_11r->mdid);
			if (roam_offload_11r->psk_msk_len)
				QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_WMI,
						   QDF_TRACE_LEVEL_DEBUG,
						   roam_offload_11r->psk_msk,
						   WLAN_MAX_PMK_DUMP_BYTES);
		} else {
			WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC,
				       sizeof(wmi_roam_11i_offload_tlv_param));
			buf += WMI_TLV_HDR_SIZE;

			roam_offload_11i =
				(wmi_roam_11i_offload_tlv_param *)buf;

			if (src_11i_info->roam_key_mgmt_offload_enabled &&
			    src_11i_info->fw_okc)
				WMI_SET_ROAM_OFFLOAD_OKC_ENABLED(
						   roam_offload_11i->flags);
			else
				WMI_SET_ROAM_OFFLOAD_OKC_DISABLED(
						roam_offload_11i->flags);

			if (src_11i_info->roam_key_mgmt_offload_enabled &&
			    src_11i_info->fw_pmksa_cache)
				WMI_SET_ROAM_OFFLOAD_PMK_CACHE_ENABLED(
						roam_offload_11i->flags);
			else
				WMI_SET_ROAM_OFFLOAD_PMK_CACHE_DISABLED(
						roam_offload_11i->flags);

			wmi_fill_sae_single_pmk_param(src_11i_info,
						      roam_offload_11i);

			roam_offload_11i->pmk_len =
				src_11i_info->pmk_len > ROAM_OFFLOAD_PMK_BYTES ?
				ROAM_OFFLOAD_PMK_BYTES : src_11i_info->pmk_len;
			qdf_mem_copy(roam_offload_11i->pmk,
				     src_11i_info->psk_pmk,
				     roam_offload_11i->pmk_len);

			roam_offload_11i->pmk_ext_len =
			    src_11i_info->pmk_len > ROAM_OFFLOAD_PMK_BYTES ?
			    ROAM_OFFLOAD_PMK_EXT_BYTES : 0;
			qdf_mem_copy(
				roam_offload_11i->pmk_ext,
				&src_11i_info->psk_pmk[ROAM_OFFLOAD_PMK_BYTES],
				roam_offload_11i->pmk_ext_len);

			WMITLV_SET_HDR(&roam_offload_11i->tlv_header,
			WMITLV_TAG_STRUC_wmi_roam_11i_offload_tlv_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_roam_11i_offload_tlv_param));

			buf += sizeof(wmi_roam_11i_offload_tlv_param);

			/*
			 * Set 11r TLV len to 0, since security profile is not
			 * FT
			 */
			WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC, 0);
			buf += WMI_TLV_HDR_SIZE;

			/*
			 * Set ESE TLV len to 0 since security profile is not
			 * ESE
			 */
			WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC, 0);
			buf += WMI_TLV_HDR_SIZE;

			wmi_info("RSO_CFG: vdev:%d pmk_len = %d pmksa caching:%d OKC:%d sae_same_pmk:%d key_mgmt_offload:%d",
				 roam_req->vdev_id, roam_offload_11i->pmk_len,
				 src_11i_info->fw_pmksa_cache,
				 src_11i_info->fw_okc,
				 src_11i_info->is_sae_same_pmk,
				 src_11i_info->roam_key_mgmt_offload_enabled);
			if (roam_offload_11i->pmk_len)
				QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_WMI,
						   QDF_TRACE_LEVEL_DEBUG,
						   roam_offload_11i->pmk,
						   WLAN_MAX_PMK_DUMP_BYTES);
		}
	} else {
		WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC, 0);
		buf += WMI_TLV_HDR_SIZE;

		WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC, 0);
		buf += WMI_TLV_HDR_SIZE;

		WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC, 0);
		buf += WMI_TLV_HDR_SIZE;
	}

	WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_STRUC, sizeof(*assoc_ies));
	buf += WMI_TLV_HDR_SIZE;

	assoc_ies = (wmi_tlv_buf_len_param *)buf;
	WMITLV_SET_HDR(&assoc_ies->tlv_header,
		       WMITLV_TAG_STRUC_wmi_tlv_buf_len_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_tlv_buf_len_param));
	assoc_ies->buf_len = roam_req->assoc_ie_length;

	buf += sizeof(*assoc_ies);

	WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_BYTE,
		       roundup(assoc_ies->buf_len, sizeof(uint32_t)));
	buf += WMI_TLV_HDR_SIZE;

	wmi_debug("RSO_CFG: akm:%d assoc_ies len:%d", akm, assoc_ies->buf_len);
	if (assoc_ies->buf_len)
		qdf_mem_copy(buf, roam_req->assoc_ie, assoc_ies->buf_len);

	buf += qdf_roundup(assoc_ies->buf_len, sizeof(uint32_t));
	buf = wmi_add_fils_tlv(wmi_handle, roam_req, buf, fils_tlv_len);

	buf = wmi_fill_sae_single_pmk_tlv(roam_req, buf);

	return QDF_STATUS_SUCCESS;
}
#else
static inline
uint32_t wmi_get_rso_buf_len(struct wlan_roam_scan_offload_params *roam_req)
{
	return 0;
}

static inline QDF_STATUS
wmi_fill_rso_tlvs(wmi_unified_t wmi_handle, uint8_t *buf_ptr,
		  struct wlan_roam_scan_offload_params *roam_req)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS
wmi_fill_rso_start_scan_tlv(struct wlan_roam_scan_offload_params *rso_req,
			    wmi_start_scan_cmd_fixed_param *scan_tlv)
{
	struct wlan_roam_scan_params *src_scan_params;

	src_scan_params = &rso_req->rso_scan_params;
	scan_tlv->scan_ctrl_flags = WMI_SCAN_ADD_CCK_RATES |
				    WMI_SCAN_ADD_OFDM_RATES |
				    WMI_SCAN_ADD_DS_IE_IN_PROBE_REQ;
	if (rso_req->is_rso_stop) {
		scan_tlv->dwell_time_active =
			ROAM_SCAN_DWELL_TIME_ACTIVE_DEFAULT;
		scan_tlv->dwell_time_passive =
			ROAM_SCAN_DWELL_TIME_PASSIVE_DEFAULT;
		scan_tlv->min_rest_time = ROAM_SCAN_MIN_REST_TIME_DEFAULT;
		scan_tlv->max_rest_time = ROAM_SCAN_MAX_REST_TIME_DEFAULT;
		scan_tlv->repeat_probe_time = 0;
		scan_tlv->probe_spacing_time = 0;
		scan_tlv->probe_delay = 0;
		scan_tlv->max_scan_time = ROAM_SCAN_HW_DEF_SCAN_MAX_DURATION;
		scan_tlv->idle_time = src_scan_params->min_rest_time;
		scan_tlv->burst_duration = 0;

		return QDF_STATUS_SUCCESS;
	}

	scan_tlv->dwell_time_active = src_scan_params->dwell_time_active;
	scan_tlv->dwell_time_passive = src_scan_params->dwell_time_passive;
	scan_tlv->burst_duration = src_scan_params->burst_duration;
	scan_tlv->min_rest_time = src_scan_params->min_rest_time;
	scan_tlv->max_rest_time = src_scan_params->max_rest_time;
	scan_tlv->repeat_probe_time = src_scan_params->repeat_probe_time;
	scan_tlv->probe_spacing_time = src_scan_params->probe_spacing_time;
	scan_tlv->probe_delay = src_scan_params->probe_delay;
	scan_tlv->max_scan_time = ROAM_SCAN_HW_DEF_SCAN_MAX_DURATION;
	scan_tlv->idle_time = src_scan_params->idle_time;
	scan_tlv->n_probes = src_scan_params->n_probes;
	scan_tlv->scan_ctrl_flags |= src_scan_params->scan_ctrl_flags;

	WMI_SCAN_SET_DWELL_MODE(scan_tlv->scan_ctrl_flags,
				src_scan_params->rso_adaptive_dwell_mode);

	/* Configure roaming scan behavior (DBS/Non-DBS scan) */
	if (rso_req->roaming_scan_policy)
		scan_tlv->scan_ctrl_flags_ext |=
			WMI_SCAN_DBS_POLICY_FORCE_NONDBS;
	else
		scan_tlv->scan_ctrl_flags_ext |=
			WMI_SCAN_DBS_POLICY_DEFAULT;

	wmi_debug("RSO_CFG: dwell time: active %d passive %d, minrest %d max rest %d repeat probe time %d probe_spacing:%d",
		  scan_tlv->dwell_time_active, scan_tlv->dwell_time_passive,
		  scan_tlv->min_rest_time, scan_tlv->max_rest_time,
		  scan_tlv->repeat_probe_time, scan_tlv->probe_spacing_time);
	wmi_debug("RSO_CFG: ctrl_flags:0x%x probe_delay:%d max_scan_time:%d idle_time:%d n_probes:%d",
		  scan_tlv->scan_ctrl_flags_ext, scan_tlv->probe_delay,
		  scan_tlv->max_scan_time, scan_tlv->idle_time,
		  scan_tlv->n_probes);

	return QDF_STATUS_SUCCESS;
}

/**
 * send_roam_scan_offload_mode_cmd_tlv() - send roam scan mode request to fw
 * @wmi_handle: wmi handle
 * @scan_cmd_fp: start scan command ptr
 * @roam_req: roam request param
 *
 * send WMI_ROAM_SCAN_MODE TLV to firmware. It has a piggyback
 * of WMI_ROAM_SCAN_MODE.
 *
 * Return: QDF status
 */
static QDF_STATUS
send_roam_scan_offload_mode_cmd_tlv(
			wmi_unified_t wmi_handle,
			struct wlan_roam_scan_offload_params *rso_req)
{
	wmi_buf_t buf = NULL;
	QDF_STATUS status;
	size_t len;
	uint8_t *buf_ptr;
	wmi_roam_scan_mode_fixed_param *roam_scan_mode_fp;
	wmi_start_scan_cmd_fixed_param *scan_cmd_fp;
	struct wlan_roam_scan_mode_params *src_rso_mode_info = NULL;

	/*
	 * Need to create a buf with roam_scan command at
	 * front and piggyback with scan command
	 */
	len = sizeof(wmi_roam_scan_mode_fixed_param) +
	      sizeof(wmi_start_scan_cmd_fixed_param);
	len += wmi_get_rso_buf_len(rso_req);

	if (rso_req->rso_mode_info.roam_scan_mode ==
	    (WMI_ROAM_SCAN_MODE_NONE | WMI_ROAM_SCAN_MODE_ROAMOFFLOAD))
		len = sizeof(wmi_roam_scan_mode_fixed_param);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);

	src_rso_mode_info = &rso_req->rso_mode_info;
	roam_scan_mode_fp = (wmi_roam_scan_mode_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(
		&roam_scan_mode_fp->tlv_header,
		WMITLV_TAG_STRUC_wmi_roam_scan_mode_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_roam_scan_mode_fixed_param));

	roam_scan_mode_fp->min_delay_roam_trigger_reason_bitmask =
			src_rso_mode_info->min_delay_roam_trigger_bitmask;
	roam_scan_mode_fp->min_delay_btw_scans =
		WMI_SEC_TO_MSEC(src_rso_mode_info->min_delay_btw_scans);
	roam_scan_mode_fp->roam_scan_mode = src_rso_mode_info->roam_scan_mode;
	roam_scan_mode_fp->vdev_id = rso_req->vdev_id;
	wmi_debug("RSO_CFG: vdev_id:%d roam scan mode:0x%x min_delay_bitmap:0x%x min_delay_btw_scans:%d",
		  rso_req->vdev_id,
		  roam_scan_mode_fp->roam_scan_mode,
		  roam_scan_mode_fp->min_delay_roam_trigger_reason_bitmask,
		  roam_scan_mode_fp->min_delay_btw_scans);
	/*
	 * For supplicant disabled roaming, all other roam triggers are disabled
	 * so send only roam scan mode Fixed param in the command
	 */
	if (src_rso_mode_info->roam_scan_mode ==
	    (WMI_ROAM_SCAN_MODE_NONE | WMI_ROAM_SCAN_MODE_ROAMOFFLOAD)) {
		roam_scan_mode_fp->flags |=
				WMI_ROAM_SCAN_MODE_FLAG_REPORT_STATUS;
		goto send_roam_scan_mode_cmd;
	}

	/* Fill in scan parameters suitable for roaming scan */
	buf_ptr += sizeof(wmi_roam_scan_mode_fixed_param);
	WMITLV_SET_HDR(
		buf_ptr,
		WMITLV_TAG_STRUC_wmi_start_scan_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_start_scan_cmd_fixed_param));
	scan_cmd_fp = (wmi_start_scan_cmd_fixed_param *)buf_ptr;
	wmi_fill_rso_start_scan_tlv(rso_req, scan_cmd_fp);

	/* Ensure there is no additional IEs */
	scan_cmd_fp->ie_len = 0;
	buf_ptr += sizeof(wmi_start_scan_cmd_fixed_param);

	status = wmi_fill_rso_tlvs(wmi_handle, buf_ptr, rso_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_buf_free(buf);
		return status;
	}

send_roam_scan_mode_cmd:
	wmi_mtrace(WMI_ROAM_SCAN_MODE, rso_req->vdev_id, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf,
				      len, WMI_ROAM_SCAN_MODE);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);

	return status;
}

/**
 * send_roam_scan_offload_ap_profile_cmd_tlv() - set roam ap profile in fw
 * @wmi_handle: wmi handle
 * @ap_profile_p: ap profile
 *
 * Send WMI_ROAM_AP_PROFILE to firmware
 *
 * Return: CDF status
 */
static QDF_STATUS
send_roam_scan_offload_ap_profile_cmd_tlv(wmi_unified_t wmi_handle,
					  struct ap_profile_params *ap_profile)
{
	wmi_buf_t buf = NULL;
	QDF_STATUS status;
	size_t len;
	uint8_t *buf_ptr;
	wmi_roam_ap_profile_fixed_param *roam_ap_profile_fp;
	wmi_roam_cnd_scoring_param *score_param;
	wmi_ap_profile *profile;
	wmi_roam_score_delta_param *score_delta_param;
	wmi_roam_cnd_min_rssi_param *min_rssi_param;
	enum roam_trigger_reason trig_reason;

	len = sizeof(wmi_roam_ap_profile_fixed_param) + sizeof(wmi_ap_profile);
	len += sizeof(*score_param);

	if (!wmi_service_enabled(wmi_handle,
			wmi_service_configure_roam_trigger_param_support)) {
		len += WMI_TLV_HDR_SIZE;
		len += NUM_OF_ROAM_TRIGGERS * sizeof(*score_delta_param);
		len += WMI_TLV_HDR_SIZE;
		len += NUM_OF_ROAM_MIN_RSSI * sizeof(*min_rssi_param);
	}
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	roam_ap_profile_fp = (wmi_roam_ap_profile_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&roam_ap_profile_fp->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_ap_profile_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_roam_ap_profile_fixed_param));
	/* fill in threshold values */
	roam_ap_profile_fp->vdev_id = ap_profile->vdev_id;
	roam_ap_profile_fp->id = 0;
	buf_ptr += sizeof(wmi_roam_ap_profile_fixed_param);

	profile = (wmi_ap_profile *)buf_ptr;
	WMITLV_SET_HDR(&profile->tlv_header,
		       WMITLV_TAG_STRUC_wmi_ap_profile,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_ap_profile));
	profile->flags = ap_profile->profile.flags;
	profile->rssi_threshold = ap_profile->profile.rssi_threshold;
	profile->bg_rssi_threshold = ap_profile->profile.bg_rssi_threshold;
	profile->ssid.ssid_len = ap_profile->profile.ssid.length;
	qdf_mem_copy(profile->ssid.ssid, ap_profile->profile.ssid.ssid,
		     profile->ssid.ssid_len);
	profile->rsn_authmode = ap_profile->profile.rsn_authmode;
	profile->rsn_ucastcipherset = ap_profile->profile.rsn_ucastcipherset;
	profile->rsn_mcastcipherset = ap_profile->profile.rsn_mcastcipherset;
	profile->rsn_mcastmgmtcipherset =
				ap_profile->profile.rsn_mcastmgmtcipherset;
	profile->rssi_abs_thresh = ap_profile->profile.rssi_abs_thresh;

	wmi_debug("vdev %d AP PROFILE: flags:%x rssi_thres:%d bg_rssi_thres:%d ssid:%.*s authmode:%d uc cipher:%d mc cipher:%d mc mgmt cipher:%d rssi abs thresh:%d",
		 roam_ap_profile_fp->vdev_id,
		 profile->flags, profile->rssi_threshold,
		 profile->bg_rssi_threshold,
		 profile->ssid.ssid_len, ap_profile->profile.ssid.ssid,
		 profile->rsn_authmode, profile->rsn_ucastcipherset,
		 profile->rsn_mcastcipherset, profile->rsn_mcastmgmtcipherset,
		 profile->rssi_abs_thresh);

	buf_ptr += sizeof(wmi_ap_profile);

	score_param = (wmi_roam_cnd_scoring_param *)buf_ptr;
	WMITLV_SET_HDR(&score_param->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_cnd_scoring_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_roam_cnd_scoring_param));
	score_param->disable_bitmap = ap_profile->param.disable_bitmap;
	score_param->rssi_weightage_pcnt =
			ap_profile->param.rssi_weightage;
	score_param->ht_weightage_pcnt = ap_profile->param.ht_weightage;
	score_param->vht_weightage_pcnt = ap_profile->param.vht_weightage;
	score_param->he_weightage_pcnt = ap_profile->param.he_weightage;
	score_param->bw_weightage_pcnt = ap_profile->param.bw_weightage;
	score_param->band_weightage_pcnt = ap_profile->param.band_weightage;
	score_param->nss_weightage_pcnt = ap_profile->param.nss_weightage;
	score_param->esp_qbss_weightage_pcnt =
			ap_profile->param.esp_qbss_weightage;
	score_param->beamforming_weightage_pcnt =
			ap_profile->param.beamforming_weightage;
	score_param->pcl_weightage_pcnt = ap_profile->param.pcl_weightage;
	score_param->oce_wan_weightage_pcnt =
			ap_profile->param.oce_wan_weightage;
	score_param->oce_ap_tx_pwr_weightage_pcnt =
				ap_profile->param.oce_ap_tx_pwr_weightage;
	score_param->oce_ap_subnet_id_weightage_pcnt =
				ap_profile->param.oce_subnet_id_weightage;
	score_param->vendor_roam_score_algorithm_id =
			ap_profile->param.vendor_roam_score_algorithm;
	score_param->sae_pk_ap_weightage_pcnt =
				ap_profile->param.sae_pk_ap_weightage;

	wmi_debug("Score params weightage: disable_bitmap %x rssi %d ht %d vht %d he %d BW %d band %d NSS %d ESP %d BF %d PCL %d OCE WAN %d APTX %d roam score algo %d subnet id %d sae-pk %d",
		 score_param->disable_bitmap, score_param->rssi_weightage_pcnt,
		 score_param->ht_weightage_pcnt,
		 score_param->vht_weightage_pcnt,
		 score_param->he_weightage_pcnt, score_param->bw_weightage_pcnt,
		 score_param->band_weightage_pcnt,
		 score_param->nss_weightage_pcnt,
		 score_param->esp_qbss_weightage_pcnt,
		 score_param->beamforming_weightage_pcnt,
		 score_param->pcl_weightage_pcnt,
		 score_param->oce_wan_weightage_pcnt,
		 score_param->oce_ap_tx_pwr_weightage_pcnt,
		 score_param->vendor_roam_score_algorithm_id,
		 score_param->oce_ap_subnet_id_weightage_pcnt,
		 score_param->sae_pk_ap_weightage_pcnt);

	score_param->bw_scoring.score_pcnt = ap_profile->param.bw_index_score;
	score_param->band_scoring.score_pcnt =
			ap_profile->param.band_index_score;
	score_param->nss_scoring.score_pcnt =
			ap_profile->param.nss_index_score;

	wmi_debug("bw_index_score %x band_index_score %x nss_index_score %x",
		  score_param->bw_scoring.score_pcnt,
		  score_param->band_scoring.score_pcnt,
		  score_param->nss_scoring.score_pcnt);

	score_param->rssi_scoring.best_rssi_threshold =
		(-1) * ap_profile->param.rssi_scoring.best_rssi_threshold;
	score_param->rssi_scoring.good_rssi_threshold =
		(-1) * ap_profile->param.rssi_scoring.good_rssi_threshold;
	score_param->rssi_scoring.bad_rssi_threshold =
		(-1) * ap_profile->param.rssi_scoring.bad_rssi_threshold;
	score_param->rssi_scoring.good_rssi_pcnt =
		ap_profile->param.rssi_scoring.good_rssi_pcnt;
	score_param->rssi_scoring.bad_rssi_pcnt =
		ap_profile->param.rssi_scoring.bad_rssi_pcnt;
	score_param->rssi_scoring.good_bucket_size =
		ap_profile->param.rssi_scoring.good_rssi_bucket_size;
	score_param->rssi_scoring.bad_bucket_size =
		ap_profile->param.rssi_scoring.bad_rssi_bucket_size;
	score_param->rssi_scoring.rssi_pref_5g_rssi_thresh =
		(-1) * ap_profile->param.rssi_scoring.rssi_pref_5g_rssi_thresh;

	wmi_debug("Rssi scoring threshold: best RSSI %d good RSSI %d bad RSSI %d prefer 5g threshold %d",
		 score_param->rssi_scoring.best_rssi_threshold,
		 score_param->rssi_scoring.good_rssi_threshold,
		 score_param->rssi_scoring.bad_rssi_threshold,
		 score_param->rssi_scoring.rssi_pref_5g_rssi_thresh);
	wmi_debug("Good RSSI score for each slot %d bad RSSI score for each slot %d good bucket %d bad bucket %d",
		 score_param->rssi_scoring.good_rssi_pcnt,
		 score_param->rssi_scoring.bad_rssi_pcnt,
		 score_param->rssi_scoring.good_bucket_size,
		 score_param->rssi_scoring.bad_bucket_size);

	score_param->esp_qbss_scoring.num_slot =
			ap_profile->param.esp_qbss_scoring.num_slot;
	score_param->esp_qbss_scoring.score_pcnt3_to_0 =
			ap_profile->param.esp_qbss_scoring.score_pcnt3_to_0;
	score_param->esp_qbss_scoring.score_pcnt7_to_4 =
			ap_profile->param.esp_qbss_scoring.score_pcnt7_to_4;
	score_param->esp_qbss_scoring.score_pcnt11_to_8 =
			ap_profile->param.esp_qbss_scoring.score_pcnt11_to_8;
	score_param->esp_qbss_scoring.score_pcnt15_to_12 =
			ap_profile->param.esp_qbss_scoring.score_pcnt15_to_12;

	wmi_debug("ESP QBSS index weight: slots %d weight 0to3 %x weight 4to7 %x weight 8to11 %x weight 12to15 %x",
		 score_param->esp_qbss_scoring.num_slot,
		 score_param->esp_qbss_scoring.score_pcnt3_to_0,
		 score_param->esp_qbss_scoring.score_pcnt7_to_4,
		 score_param->esp_qbss_scoring.score_pcnt11_to_8,
		 score_param->esp_qbss_scoring.score_pcnt15_to_12);

	score_param->oce_wan_scoring.num_slot =
			ap_profile->param.oce_wan_scoring.num_slot;
	score_param->oce_wan_scoring.score_pcnt3_to_0 =
			ap_profile->param.oce_wan_scoring.score_pcnt3_to_0;
	score_param->oce_wan_scoring.score_pcnt7_to_4 =
			ap_profile->param.oce_wan_scoring.score_pcnt7_to_4;
	score_param->oce_wan_scoring.score_pcnt11_to_8 =
			ap_profile->param.oce_wan_scoring.score_pcnt11_to_8;
	score_param->oce_wan_scoring.score_pcnt15_to_12 =
			ap_profile->param.oce_wan_scoring.score_pcnt15_to_12;

	wmi_debug("OCE WAN index weight: slots %d weight 0to3 %x weight 4to7 %x weight 8to11 %x weight 12to15 %x",
		 score_param->oce_wan_scoring.num_slot,
		 score_param->oce_wan_scoring.score_pcnt3_to_0,
		 score_param->oce_wan_scoring.score_pcnt7_to_4,
		 score_param->oce_wan_scoring.score_pcnt11_to_8,
		 score_param->oce_wan_scoring.score_pcnt15_to_12);

	score_param->roam_score_delta_pcnt = ap_profile->param.roam_score_delta;
	score_param->roam_score_delta_mask =
				ap_profile->param.roam_trigger_bitmap;
	score_param->candidate_min_roam_score_delta =
				ap_profile->param.cand_min_roam_score_delta;
	wmi_debug("Roam score delta:%d Roam_trigger_bitmap:%x cand min score delta = %d",
		 score_param->roam_score_delta_pcnt,
		 score_param->roam_score_delta_mask,
		 score_param->candidate_min_roam_score_delta);

	buf_ptr += sizeof(*score_param);

	if (!wmi_service_enabled(wmi_handle,
			wmi_service_configure_roam_trigger_param_support)) {
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			       (NUM_OF_ROAM_TRIGGERS * sizeof(*score_delta_param)));
		buf_ptr += WMI_TLV_HDR_SIZE;

		score_delta_param = (wmi_roam_score_delta_param *)buf_ptr;
		WMITLV_SET_HDR(&score_delta_param->tlv_header,
			       WMITLV_TAG_STRUC_wmi_roam_score_delta_param,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_roam_score_delta_param));
		trig_reason =
			ap_profile->score_delta_param[IDLE_ROAM_TRIGGER].trigger_reason;
		score_delta_param->roam_trigger_reason =
			convert_roam_trigger_reason(trig_reason);
		score_delta_param->roam_score_delta =
			ap_profile->score_delta_param[IDLE_ROAM_TRIGGER].roam_score_delta;

		buf_ptr += sizeof(*score_delta_param);
		score_delta_param = (wmi_roam_score_delta_param *)buf_ptr;
		WMITLV_SET_HDR(&score_delta_param->tlv_header,
			       WMITLV_TAG_STRUC_wmi_roam_score_delta_param,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_roam_score_delta_param));
		trig_reason =
			ap_profile->score_delta_param[BTM_ROAM_TRIGGER].trigger_reason;
		score_delta_param->roam_trigger_reason =
			convert_roam_trigger_reason(trig_reason);
		score_delta_param->roam_score_delta =
			ap_profile->score_delta_param[BTM_ROAM_TRIGGER].roam_score_delta;

		buf_ptr += sizeof(*score_delta_param);
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			       (NUM_OF_ROAM_MIN_RSSI * sizeof(*min_rssi_param)));
		buf_ptr += WMI_TLV_HDR_SIZE;

		min_rssi_param = (wmi_roam_cnd_min_rssi_param *)buf_ptr;
		WMITLV_SET_HDR(&min_rssi_param->tlv_header,
			       WMITLV_TAG_STRUC_wmi_roam_cnd_min_rssi_param,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_roam_cnd_min_rssi_param));
		trig_reason =
			ap_profile->min_rssi_params[DEAUTH_MIN_RSSI].trigger_reason;
		min_rssi_param->roam_trigger_reason =
			convert_roam_trigger_reason(trig_reason);
		min_rssi_param->candidate_min_rssi =
			ap_profile->min_rssi_params[DEAUTH_MIN_RSSI].min_rssi;

		buf_ptr += sizeof(*min_rssi_param);
		min_rssi_param = (wmi_roam_cnd_min_rssi_param *)buf_ptr;
		WMITLV_SET_HDR(&min_rssi_param->tlv_header,
			       WMITLV_TAG_STRUC_wmi_roam_cnd_min_rssi_param,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_roam_cnd_min_rssi_param));
		trig_reason =
			ap_profile->min_rssi_params[BMISS_MIN_RSSI].trigger_reason;
		min_rssi_param->roam_trigger_reason =
			convert_roam_trigger_reason(trig_reason);
		min_rssi_param->candidate_min_rssi =
			ap_profile->min_rssi_params[BMISS_MIN_RSSI].min_rssi;

		buf_ptr += sizeof(*min_rssi_param);
		min_rssi_param = (wmi_roam_cnd_min_rssi_param *)buf_ptr;
		WMITLV_SET_HDR(&min_rssi_param->tlv_header,
			       WMITLV_TAG_STRUC_wmi_roam_cnd_min_rssi_param,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_roam_cnd_min_rssi_param));
		trig_reason =
		     ap_profile->min_rssi_params[MIN_RSSI_2G_TO_5G_ROAM].trigger_reason;
		min_rssi_param->roam_trigger_reason =
			convert_roam_trigger_reason(trig_reason);
		min_rssi_param->candidate_min_rssi =
			ap_profile->min_rssi_params[MIN_RSSI_2G_TO_5G_ROAM].min_rssi;
	}
	wmi_mtrace(WMI_ROAM_AP_PROFILE, NO_SESSION, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf,
				      len, WMI_ROAM_AP_PROFILE);
	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);

	return status;
}

/**
 * send_roam_scan_offload_cmd_tlv() - set roam offload command
 * @wmi_handle: wmi handle
 * @command: command
 * @vdev_id: vdev id
 *
 * This function set roam offload command to fw.
 *
 * Return: QDF status
 */
static QDF_STATUS
send_roam_scan_offload_cmd_tlv(wmi_unified_t wmi_handle,
			       uint32_t command, uint32_t vdev_id)
{
	QDF_STATUS status;
	wmi_roam_scan_cmd_fixed_param *cmd_fp;
	wmi_buf_t buf = NULL;
	int len;
	uint8_t *buf_ptr;

	len = sizeof(wmi_roam_scan_cmd_fixed_param);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);

	cmd_fp = (wmi_roam_scan_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd_fp->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_scan_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_roam_scan_cmd_fixed_param));
	cmd_fp->vdev_id = vdev_id;
	cmd_fp->command_arg = command;

	wmi_mtrace(WMI_ROAM_SCAN_CMD, NO_SESSION, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf,
				      len, WMI_ROAM_SCAN_CMD);
	if (QDF_IS_STATUS_ERROR(status))
		goto error;

	wmi_info("WMI --> WMI_ROAM_SCAN_CMD");
	return QDF_STATUS_SUCCESS;

error:
	wmi_buf_free(buf);

	return status;
}

/**
 * send_roam_scan_offload_chan_list_cmd_tlv() - set roam offload channel list
 * @wmi_handle: wmi handle
 *
 * Set roam offload channel list.
 *
 * Return: QDF status
 */
static QDF_STATUS send_roam_scan_offload_chan_list_cmd_tlv(
			wmi_unified_t wmi_handle,
			struct wlan_roam_scan_channel_list *rso_ch_info)
{
	wmi_buf_t buf = NULL;
	QDF_STATUS status;
	int len, list_tlv_len;
	int i;
	uint8_t *buf_ptr;
	wmi_roam_chan_list_fixed_param *chan_list_fp;
	uint32_t *roam_chan_list_array;
	uint8_t chan_count = rso_ch_info->chan_count;
	uint32_t *chan_list = rso_ch_info->chan_freq_list;

	/* Channel list is a table of 2 TLV's */
	list_tlv_len = WMI_TLV_HDR_SIZE + chan_count * sizeof(uint32_t);
	len = sizeof(wmi_roam_chan_list_fixed_param) + list_tlv_len;
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	chan_list_fp = (wmi_roam_chan_list_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&chan_list_fp->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_chan_list_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_roam_chan_list_fixed_param));
	chan_list_fp->vdev_id = rso_ch_info->vdev_id;
	chan_list_fp->num_chan = chan_count;
	if (rso_ch_info->chan_cache_type == WMI_CHANNEL_LIST_STATIC)
		/* external app is controlling channel list */
		chan_list_fp->chan_list_type =
			WMI_ROAM_SCAN_CHAN_LIST_TYPE_STATIC;
	else
		/* umac supplied occupied channel list in LFR */
		chan_list_fp->chan_list_type =
			WMI_ROAM_SCAN_CHAN_LIST_TYPE_DYNAMIC;

	buf_ptr += sizeof(wmi_roam_chan_list_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       (chan_list_fp->num_chan * sizeof(uint32_t)));
	roam_chan_list_array = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i = 0; ((i < chan_list_fp->num_chan) &&
		     (i < WMI_ROAM_MAX_CHANNELS)); i++)
		roam_chan_list_array[i] = chan_list[i];

	wmi_debug("RSO_CFG: vdev:%d num_chan:%d cache_type:%d",
		  chan_list_fp->vdev_id, chan_list_fp->num_chan,
		  rso_ch_info->chan_cache_type);
	wmi_mtrace(WMI_ROAM_CHAN_LIST, NO_SESSION, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf,
				      len, WMI_ROAM_CHAN_LIST);
	if (QDF_IS_STATUS_ERROR(status))
		goto error;

	return QDF_STATUS_SUCCESS;
error:
	wmi_buf_free(buf);

	return status;
}

/**
 * send_roam_scan_offload_rssi_change_cmd_tlv() - set roam offload RSSI th
 * @wmi_handle: wmi handle
 * @rssi_change_thresh: RSSI Change threshold
 * @bcn_rssi_weight: beacon RSSI weight
 * @vdev_id: vdev id
 *
 * Send WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD parameters to fw.
 *
 * Return: CDF status
 */
static QDF_STATUS send_roam_scan_offload_rssi_change_cmd_tlv(
		wmi_unified_t wmi_handle,
		struct wlan_roam_rssi_change_params *params)
{
	wmi_buf_t buf = NULL;
	QDF_STATUS status;
	int len;
	uint8_t *buf_ptr;
	wmi_roam_scan_rssi_change_threshold_fixed_param *rssi_change_fp;

	/* Send rssi change parameters */
	len = sizeof(wmi_roam_scan_rssi_change_threshold_fixed_param);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	rssi_change_fp =
		(wmi_roam_scan_rssi_change_threshold_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&rssi_change_fp->tlv_header,
		WMITLV_TAG_STRUC_wmi_roam_scan_rssi_change_threshold_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN
		       (wmi_roam_scan_rssi_change_threshold_fixed_param));
	/* fill in rssi change threshold (hysteresis) values */
	rssi_change_fp->vdev_id = params->vdev_id;
	rssi_change_fp->roam_scan_rssi_change_thresh =
				params->rssi_change_thresh;
	rssi_change_fp->bcn_rssi_weight = params->bcn_rssi_weight;
	rssi_change_fp->hirssi_delay_btw_scans = params->hirssi_delay_btw_scans;

	wmi_nofl_debug("RSO_CFG: vdev %d rssi_change_thresh:%d bcn_rssi_weight:%d hirssi_delay_btw_scans:%d",
		       rssi_change_fp->vdev_id,
		       rssi_change_fp->roam_scan_rssi_change_thresh,
		       rssi_change_fp->bcn_rssi_weight,
		       rssi_change_fp->hirssi_delay_btw_scans);

	wmi_mtrace(WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD,
		   rssi_change_fp->vdev_id, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf, len,
				      WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD);
	if (QDF_IS_STATUS_ERROR(status))
		goto error;

	return QDF_STATUS_SUCCESS;
error:
	wmi_buf_free(buf);

	return status;
}

/**
 * send_per_roam_config_cmd_tlv() - set per roaming config to FW
 * @wmi_handle: wmi handle
 * @req_buf: per roam config buffer
 *
 * Return: QDF status
 */
static QDF_STATUS
send_per_roam_config_cmd_tlv(wmi_unified_t wmi_handle,
			     struct wlan_per_roam_config_req *req_buf)
{
	wmi_buf_t buf = NULL;
	QDF_STATUS status;
	int len;
	uint8_t *buf_ptr;
	wmi_roam_per_config_fixed_param *wmi_per_config;

	len = sizeof(wmi_roam_per_config_fixed_param);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	wmi_per_config =
		(wmi_roam_per_config_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&wmi_per_config->tlv_header,
		       WMITLV_TAG_STRUC_wmi_roam_per_config_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
		       (wmi_roam_per_config_fixed_param));

	/* fill in per roam config values */
	wmi_per_config->vdev_id = req_buf->vdev_id;

	wmi_per_config->enable = req_buf->per_config.enable;
	wmi_per_config->high_rate_thresh =
		(req_buf->per_config.tx_high_rate_thresh << 16) |
		(req_buf->per_config.rx_high_rate_thresh & 0x0000ffff);
	wmi_per_config->low_rate_thresh =
		(req_buf->per_config.tx_low_rate_thresh << 16) |
		(req_buf->per_config.rx_low_rate_thresh & 0x0000ffff);
	wmi_per_config->pkt_err_rate_thresh_pct =
		(req_buf->per_config.tx_rate_thresh_percnt << 16) |
		(req_buf->per_config.rx_rate_thresh_percnt & 0x0000ffff);
	wmi_per_config->per_rest_time = req_buf->per_config.per_rest_time;
	wmi_per_config->pkt_err_rate_mon_time =
			(req_buf->per_config.tx_per_mon_time << 16) |
			(req_buf->per_config.rx_per_mon_time & 0x0000ffff);
	wmi_per_config->min_candidate_rssi =
			req_buf->per_config.min_candidate_rssi;

	/* Send per roam config parameters */
	wmi_mtrace(WMI_ROAM_PER_CONFIG_CMDID, NO_SESSION, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf,
				      len, WMI_ROAM_PER_CONFIG_CMDID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_err("WMI_ROAM_PER_CONFIG_CMDID failed, Error %d", status);
		wmi_buf_free(buf);
		return status;
	}
	wmi_debug("per roam enable=%d, vdev=%d",
		 req_buf->per_config.enable, req_buf->vdev_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * send_limit_off_chan_cmd_tlv() - send wmi cmd of limit off chan
 * configuration params
 * @wmi_handle: wmi handler
 * @limit_off_chan_param: pointer to wmi_off_chan_param
 *
 * Return: 0 for success and non zero for failure
 */
static QDF_STATUS send_limit_off_chan_cmd_tlv(
			wmi_unified_t wmi_handle,
			struct wmi_limit_off_chan_param *limit_off_chan_param)
{
	wmi_vdev_limit_offchan_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len = sizeof(*cmd);
	int err;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_vdev_limit_offchan_cmd_fixed_param *)wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_vdev_limit_offchan_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_vdev_limit_offchan_cmd_fixed_param));

	cmd->vdev_id = limit_off_chan_param->vdev_id;

	cmd->flags &= 0;
	if (limit_off_chan_param->status)
		cmd->flags |= WMI_VDEV_LIMIT_OFFCHAN_ENABLE;
	if (limit_off_chan_param->skip_dfs_chans)
		cmd->flags |= WMI_VDEV_LIMIT_OFFCHAN_SKIP_DFS;

	cmd->max_offchan_time = limit_off_chan_param->max_offchan_time;
	cmd->rest_time = limit_off_chan_param->rest_time;

	wmi_debug("vdev_id=%d, flags =%x, max_offchan_time=%d, rest_time=%d",
		 cmd->vdev_id, cmd->flags, cmd->max_offchan_time,
		 cmd->rest_time);

	wmi_mtrace(WMI_VDEV_LIMIT_OFFCHAN_CMDID, cmd->vdev_id, 0);
	err = wmi_unified_cmd_send(wmi_handle, buf,
				   len, WMI_VDEV_LIMIT_OFFCHAN_CMDID);
	if (QDF_IS_STATUS_ERROR(err)) {
		wmi_err("Failed to send limit off chan cmd err=%d", err);
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_FILS_SK
static QDF_STATUS send_roam_scan_send_hlp_cmd_tlv(wmi_unified_t wmi_handle,
						  struct hlp_params *params)
{
	uint32_t len;
	uint8_t *buf_ptr;
	wmi_buf_t buf = NULL;
	wmi_pdev_update_fils_hlp_pkt_cmd_fixed_param *hlp_params;

	len = sizeof(wmi_pdev_update_fils_hlp_pkt_cmd_fixed_param);
	len += WMI_TLV_HDR_SIZE;
	len += qdf_roundup(params->hlp_ie_len, sizeof(uint32_t));

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	hlp_params = (wmi_pdev_update_fils_hlp_pkt_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&hlp_params->tlv_header,
		WMITLV_TAG_STRUC_wmi_pdev_update_fils_hlp_pkt_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_pdev_update_fils_hlp_pkt_cmd_fixed_param));

	hlp_params->vdev_id = params->vdev_id;
	hlp_params->size = params->hlp_ie_len;
	hlp_params->pkt_type = WMI_FILS_HLP_PKT_TYPE_DHCP_DISCOVER;

	buf_ptr += sizeof(*hlp_params);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       round_up(params->hlp_ie_len, sizeof(uint32_t)));

	buf_ptr += WMI_TLV_HDR_SIZE;
	qdf_mem_copy(buf_ptr, params->hlp_ie, params->hlp_ie_len);

	wmi_debug("send FILS HLP pkt vdev %d len %d",
		 hlp_params->vdev_id, hlp_params->size);
	wmi_mtrace(WMI_PDEV_UPDATE_FILS_HLP_PKT_CMDID, NO_SESSION, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_PDEV_UPDATE_FILS_HLP_PKT_CMDID)) {
		wmi_err("Failed to send FILS HLP pkt cmd");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void wmi_fils_sk_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->send_roam_scan_hlp_cmd = send_roam_scan_send_hlp_cmd_tlv;
}
#endif /* WLAN_FEATURE_FILS_SK */

/*
 * send_btm_config_cmd_tlv() - Send wmi cmd for BTM config
 * @wmi_handle: wmi handle
 * @params: pointer to wlan_roam_btm_config
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS send_btm_config_cmd_tlv(wmi_unified_t wmi_handle,
					  struct wlan_roam_btm_config *params)
{
	wmi_btm_config_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len;

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_btm_config_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_btm_config_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_btm_config_fixed_param));
	cmd->vdev_id = params->vdev_id;
	cmd->flags = params->btm_offload_config;
	cmd->max_attempt_cnt = params->btm_max_attempt_cnt;
	cmd->solicited_timeout_ms = params->btm_solicited_timeout;
	cmd->stick_time_seconds = params->btm_sticky_time;
	cmd->disassoc_timer_threshold = params->disassoc_timer_threshold;
	cmd->btm_bitmap = params->btm_query_bitmask;
	cmd->btm_candidate_min_score = params->btm_candidate_min_score;

	wmi_debug("RSO_CFG: vdev_id:%u btm_offload:%u btm_query_bitmask:%u btm_candidate_min_score:%u",
		  cmd->vdev_id, cmd->flags, cmd->btm_bitmap,
		  cmd->btm_candidate_min_score);
	wmi_debug("RSO_CFG: btm_solicited_timeout:%u btm_max_attempt_cnt:%u btm_sticky_time:%u disassoc_timer_threshold:%u",
		  cmd->solicited_timeout_ms, cmd->max_attempt_cnt,
		  cmd->stick_time_seconds, cmd->disassoc_timer_threshold);

	wmi_mtrace(WMI_ROAM_BTM_CONFIG_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_ROAM_BTM_CONFIG_CMDID)) {
		wmi_err("Failed to send WMI_ROAM_BTM_CONFIG_CMDID");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_roam_bss_load_config_tlv() - send roam load bss trigger configuration
 * @wmi_handle: wmi handle
 * @parms: pointer to wlan_roam_bss_load_config
 *
 * This function sends the roam load bss trigger configuration to fw.
 * the bss_load_threshold parameter is used to configure the maximum
 * bss load percentage, above which the firmware should trigger roaming
 *
 * Return: QDF status
 */
static QDF_STATUS
send_roam_bss_load_config_tlv(wmi_unified_t wmi_handle,
			      struct wlan_roam_bss_load_config *params)
{
	wmi_roam_bss_load_config_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len;

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_roam_bss_load_config_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(
	    &cmd->tlv_header,
	    WMITLV_TAG_STRUC_wmi_roam_bss_load_config_cmd_fixed_param,
	    WMITLV_GET_STRUCT_TLVLEN(wmi_roam_bss_load_config_cmd_fixed_param));

	cmd->vdev_id = params->vdev_id;
	cmd->bss_load_threshold = params->bss_load_threshold;
	cmd->monitor_time_window = params->bss_load_sample_time;
	cmd->rssi_2g_threshold = params->rssi_threshold_24ghz;
	cmd->rssi_5g_threshold = params->rssi_threshold_5ghz;

	wmi_debug("RSO_CFG: vdev:%d bss_load_thres:%d monitor_time:%d rssi_2g:%d rssi_5g:%d",
		  cmd->vdev_id, cmd->bss_load_threshold,
		  cmd->monitor_time_window, cmd->rssi_2g_threshold,
		  cmd->rssi_5g_threshold);

	wmi_mtrace(WMI_ROAM_BSS_LOAD_CONFIG_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_ROAM_BSS_LOAD_CONFIG_CMDID)) {
		wmi_err("Failed to send WMI_ROAM_BSS_LOAD_CONFIG_CMDID");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * send_disconnect_roam_params_tlv() - send disconnect roam trigger parameters
 * @wmi_handle: wmi handle
 * @disconnect_roam: pointer to wlan_roam_disconnect_params which carries the
 * disconnect_roam_trigger parameters from CSR
 *
 * This function sends the disconnect roam trigger parameters to fw.
 *
 * Return: QDF status
 */
static QDF_STATUS
send_disconnect_roam_params_tlv(wmi_unified_t wmi_handle,
				struct wlan_roam_disconnect_params *req)
{
	wmi_roam_deauth_config_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len;

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_roam_deauth_config_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(
	    &cmd->tlv_header,
	    WMITLV_TAG_STRUC_wmi_roam_deauth_config_cmd_fixed_param,
	    WMITLV_GET_STRUCT_TLVLEN(wmi_roam_deauth_config_cmd_fixed_param));

	cmd->vdev_id = req->vdev_id;
	cmd->enable = req->enable;
	wmi_debug("RSO_CFG: vdev_id:%d enable:%d", cmd->vdev_id, cmd->enable);

	wmi_mtrace(WMI_ROAM_DEAUTH_CONFIG_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_ROAM_DEAUTH_CONFIG_CMDID)) {
		wmi_err("Failed to send WMI_ROAM_DEAUTH_CONFIG_CMDID");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_idle_roam_params_tlv() - send idle roam trigger parameters
 * @wmi_handle: wmi handle
 * @idle_roam_params: pointer to wlan_roam_idle_params which carries the
 * idle roam parameters from CSR
 *
 * This function sends the idle roam trigger parameters to fw.
 *
 * Return: QDF status
 */
static QDF_STATUS
send_idle_roam_params_tlv(wmi_unified_t wmi_handle,
			  struct wlan_roam_idle_params *idle_roam_params)
{
	wmi_roam_idle_config_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len;

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_roam_idle_config_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(
	    &cmd->tlv_header,
	    WMITLV_TAG_STRUC_wmi_roam_idle_config_cmd_fixed_param,
	    WMITLV_GET_STRUCT_TLVLEN(wmi_roam_idle_config_cmd_fixed_param));

	cmd->vdev_id = idle_roam_params->vdev_id;
	cmd->enable = idle_roam_params->enable;
	cmd->band = idle_roam_params->band;
	cmd->rssi_delta = idle_roam_params->conn_ap_rssi_delta;
	cmd->min_rssi = idle_roam_params->conn_ap_min_rssi;
	cmd->idle_time = idle_roam_params->inactive_time;
	cmd->data_packet_count = idle_roam_params->data_pkt_count;
	wmi_debug("RSO_CFG: vdev_id:%d enable:%d band:%d rssi_delta:%d min_rssi:%d idle_time:%d data_pkt:%d",
		 cmd->vdev_id, cmd->enable,
		 cmd->band, cmd->rssi_delta, cmd->min_rssi,
		 cmd->idle_time, cmd->data_packet_count);

	wmi_mtrace(WMI_ROAM_IDLE_CONFIG_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_ROAM_IDLE_CONFIG_CMDID)) {
		wmi_err("Failed to send WMI_ROAM_IDLE_CONFIG_CMDID");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_roam_preauth_status_tlv() - send roam pre-authentication status
 * @wmi_handle: wmi handle
 * @params: pre-auth status params
 *
 * This function sends the roam pre-authentication status for WPA3 SAE
 * pre-auth to target.
 *
 * Return: QDF status
 */
static QDF_STATUS
send_roam_preauth_status_tlv(wmi_unified_t wmi_handle,
			     struct wmi_roam_auth_status_params *params)
{
	wmi_roam_preauth_status_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len;
	uint8_t *buf_ptr;

	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE + PMKID_LEN;
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_roam_preauth_status_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(
	    &cmd->tlv_header,
	    WMITLV_TAG_STRUC_wmi_roam_preauth_status_cmd_fixed_param,
	    WMITLV_GET_STRUCT_TLVLEN(wmi_roam_preauth_status_cmd_fixed_param));

	cmd->vdev_id = params->vdev_id;
	cmd->preauth_status = params->preauth_status;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(params->bssid.bytes,
				   &cmd->candidate_ap_bssid);

	buf_ptr += sizeof(wmi_roam_preauth_status_cmd_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, PMKID_LEN);
	buf_ptr += WMI_TLV_HDR_SIZE;

	qdf_mem_copy(buf_ptr, params->pmkid, PMKID_LEN);
	wmi_debug("vdev_id:%d status:%d bssid:"QDF_MAC_ADDR_FMT,
		 cmd->vdev_id, cmd->preauth_status,
		 QDF_MAC_ADDR_REF(params->bssid.bytes));

	wmi_mtrace(WMI_ROAM_PREAUTH_STATUS_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				 WMI_ROAM_PREAUTH_STATUS_CMDID)) {
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
send_disconnect_roam_params_tlv(wmi_unified_t wmi_handle,
				struct wlan_roam_disconnect_params *req)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
send_idle_roam_params_tlv(wmi_unified_t wmi_handle,
			  struct wlan_roam_idle_params *idle_roam_params)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
send_roam_preauth_status_tlv(wmi_unified_t wmi_handle,
			     struct wmi_roam_auth_status_params *params)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

/**
 * send_offload_11k_cmd_tlv() - send wmi cmd with 11k offload params
 * @wmi_handle: wmi handler
 * @params: pointer to 11k offload params
 *
 * Return: 0 for success and non zero for failure
 */
static QDF_STATUS
send_offload_11k_cmd_tlv(wmi_unified_t wmi_handle,
			 struct wlan_roam_11k_offload_params *params)
{
	wmi_11k_offload_report_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS status;
	uint8_t *buf_ptr;
	wmi_neighbor_report_11k_offload_tlv_param
					*neighbor_report_offload_params;
	wmi_neighbor_report_offload *neighbor_report_offload;
	uint32_t len = sizeof(*cmd);

	if (params->offload_11k_bitmask &
	    WMI_11K_OFFLOAD_BITMAP_NEIGHBOR_REPORT_REQ)
		len += WMI_TLV_HDR_SIZE +
			sizeof(wmi_neighbor_report_11k_offload_tlv_param);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_11k_offload_report_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_offload_11k_report_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_11k_offload_report_fixed_param));

	cmd->vdev_id = params->vdev_id;
	cmd->offload_11k = params->offload_11k_bitmask;

	if (params->offload_11k_bitmask &
	    WMI_11K_OFFLOAD_BITMAP_NEIGHBOR_REPORT_REQ) {
		buf_ptr += sizeof(wmi_11k_offload_report_fixed_param);

		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			sizeof(wmi_neighbor_report_11k_offload_tlv_param));
		buf_ptr += WMI_TLV_HDR_SIZE;

		neighbor_report_offload_params =
			(wmi_neighbor_report_11k_offload_tlv_param *)buf_ptr;
		WMITLV_SET_HDR(&neighbor_report_offload_params->tlv_header,
			WMITLV_TAG_STRUC_wmi_neighbor_report_offload_tlv_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_neighbor_report_11k_offload_tlv_param));

		neighbor_report_offload = &neighbor_report_offload_params->
			neighbor_rep_ofld_params;

		neighbor_report_offload->time_offset =
			params->neighbor_report_params.time_offset;
		neighbor_report_offload->low_rssi_offset =
			params->neighbor_report_params.low_rssi_offset;
		neighbor_report_offload->bmiss_count_trigger =
			params->neighbor_report_params.bmiss_count_trigger;
		neighbor_report_offload->per_threshold_offset =
			params->neighbor_report_params.per_threshold_offset;
		neighbor_report_offload->neighbor_report_cache_timeout =
			params->neighbor_report_params.
			neighbor_report_cache_timeout;
		neighbor_report_offload->max_neighbor_report_req_cap =
			params->neighbor_report_params.
			max_neighbor_report_req_cap;
		neighbor_report_offload->ssid.ssid_len =
			params->neighbor_report_params.ssid.length;
		qdf_mem_copy(neighbor_report_offload->ssid.ssid,
			     &params->neighbor_report_params.ssid.ssid,
			     neighbor_report_offload->ssid.ssid_len);
	}

	wmi_debug("RSO_CFG: vdev %d 11k_bitmask:%u time_offset:%u low_rssi_offset:%u bmiss_count_trigger:%u per_threshold_offset%u",
		  cmd->vdev_id, params->offload_11k_bitmask,
		  params->neighbor_report_params.time_offset,
		  params->neighbor_report_params.low_rssi_offset,
		  params->neighbor_report_params.bmiss_count_trigger,
		  params->neighbor_report_params.per_threshold_offset);
	wmi_debug("RSO_CFG: neighbor_report_cache_timeout:%u max_neighbor_report_req_cap:%u SSID:%.*s",
		  params->neighbor_report_params.neighbor_report_cache_timeout,
		  params->neighbor_report_params.max_neighbor_report_req_cap,
		  params->neighbor_report_params.ssid.length,
		  params->neighbor_report_params.ssid.ssid);

	wmi_mtrace(WMI_11K_OFFLOAD_REPORT_CMDID, cmd->vdev_id, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf, len,
				      WMI_11K_OFFLOAD_REPORT_CMDID);
	if (status != QDF_STATUS_SUCCESS) {
		wmi_err("Failed to send 11k offload command %d", status);
		wmi_buf_free(buf);
	}

	return status;
}

/**
 * send_invoke_neighbor_report_cmd_tlv() - send invoke 11k neighbor report
 * command
 * @wmi_handle: wmi handler
 * @params: pointer to neighbor report invoke params
 *
 * Return: 0 for success and non zero for failure
 */
static QDF_STATUS send_invoke_neighbor_report_cmd_tlv(
		wmi_unified_t wmi_handle,
		struct wmi_invoke_neighbor_report_params *params)
{
	wmi_11k_offload_invoke_neighbor_report_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS status;
	uint8_t *buf_ptr;
	uint32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_11k_offload_invoke_neighbor_report_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		 WMITLV_TAG_STRUC_wmi_invoke_neighbor_report_fixed_param,
		 WMITLV_GET_STRUCT_TLVLEN(
			wmi_11k_offload_invoke_neighbor_report_fixed_param));

	cmd->vdev_id = params->vdev_id;
	cmd->flags = params->send_resp_to_host;

	cmd->ssid.ssid_len = params->ssid.length;
	qdf_mem_copy(cmd->ssid.ssid, &params->ssid.ssid, cmd->ssid.ssid_len);

	wmi_mtrace(WMI_11K_INVOKE_NEIGHBOR_REPORT_CMDID, cmd->vdev_id, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf, len,
				      WMI_11K_INVOKE_NEIGHBOR_REPORT_CMDID);
	if (status != QDF_STATUS_SUCCESS) {
		wmi_err("Failed to send invoke neighbor report command %d",
			status);
		wmi_buf_free(buf);
	}

	return status;
}

void wmi_roam_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->send_roam_scan_offload_rssi_thresh_cmd =
			send_roam_scan_offload_rssi_thresh_cmd_tlv;
	ops->send_roam_mawc_params_cmd = send_roam_mawc_params_cmd_tlv;
	ops->send_roam_scan_filter_cmd =
			send_roam_scan_filter_cmd_tlv;
	ops->send_roam_scan_offload_mode_cmd =
			send_roam_scan_offload_mode_cmd_tlv;
	ops->send_roam_scan_offload_ap_profile_cmd =
			send_roam_scan_offload_ap_profile_cmd_tlv;
	ops->send_roam_scan_offload_cmd = send_roam_scan_offload_cmd_tlv;
	ops->send_roam_scan_offload_scan_period_cmd =
			send_roam_scan_offload_scan_period_cmd_tlv;
	ops->send_roam_scan_offload_chan_list_cmd =
			send_roam_scan_offload_chan_list_cmd_tlv;
	ops->send_roam_scan_offload_rssi_change_cmd =
			send_roam_scan_offload_rssi_change_cmd_tlv;
	ops->send_per_roam_config_cmd = send_per_roam_config_cmd_tlv;
	ops->send_limit_off_chan_cmd = send_limit_off_chan_cmd_tlv;
	ops->send_btm_config = send_btm_config_cmd_tlv;
	ops->send_offload_11k_cmd = send_offload_11k_cmd_tlv;
	ops->send_invoke_neighbor_report_cmd =
			send_invoke_neighbor_report_cmd_tlv;
	ops->send_roam_bss_load_config = send_roam_bss_load_config_tlv;
	ops->send_idle_roam_params = send_idle_roam_params_tlv;
	ops->send_disconnect_roam_params = send_disconnect_roam_params_tlv;
	ops->send_roam_preauth_status = send_roam_preauth_status_tlv;

	wmi_lfr_subnet_detection_attach_tlv(wmi_handle);
	wmi_rssi_monitor_attach_tlv(wmi_handle);
	wmi_ese_attach_tlv(wmi_handle);
	wmi_roam_offload_attach_tlv(wmi_handle);
	wmi_fils_sk_attach_tlv(wmi_handle);
}

