/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: Target interface file for pkt_capture component to
 * Implement api's which shall be used by pkt_capture component
 * in target_if internally.
 */

#include <target_if_pkt_capture.h>
#include <wlan_pkt_capture_tgt_api.h>
#include <wmi_unified_api.h>
#include <target_if.h>
#include <init_deinit_lmac.h>
#include <wlan_pkt_capture_api.h>

/**
 * target_if_set_packet_capture_mode() - set packet capture mode
 * @psoc: pointer to psoc object
 * @vdev_id: vdev id
 * @mode: mode to set
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_set_packet_capture_mode(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id,
				  enum pkt_capture_mode mode)
{
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct vdev_set_params param;

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	target_if_debug("psoc:%pK, vdev_id:%d mode:%d",
			psoc, vdev_id, mode);

	param.vdev_id = vdev_id;
	param.param_id = WMI_VDEV_PARAM_PACKET_CAPTURE_MODE;
	param.param_value = (uint32_t)mode;

	status = wmi_unified_vdev_set_param_send(wmi_handle, &param);
	if (QDF_IS_STATUS_SUCCESS(status))
		ucfg_pkt_capture_set_pktcap_mode(psoc, mode);
	else
		pkt_capture_err("failed to set packet capture mode");

	return status;
}

#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
/**
 * target_if_set_packet_capture_config() - set packet capture config
 * @psoc: pointer to psoc object
 * @vdev_id: vdev id
 * @config_value: config value
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_set_packet_capture_config
			(struct wlan_objmgr_psoc *psoc,
			 uint8_t vdev_id,
			 enum pkt_capture_config config_value)
{
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_set_params param;

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_PKT_CAPTURE_ID);
	if (!vdev) {
		pkt_capture_err("vdev is NULL");
		return QDF_STATUS_E_INVAL;
	}

	target_if_debug("psoc:%pK, vdev_id:%d config_value:%d",
			psoc, vdev_id, config_value);

	param.vdev_id = vdev_id;
	param.param_id = WMI_VDEV_PARAM_SMART_MONITOR_CONFIG;
	param.param_value = (uint32_t)config_value;

	status = wmi_unified_vdev_set_param_send(wmi_handle, &param);
	if (QDF_IS_STATUS_SUCCESS(status))
		ucfg_pkt_capture_set_pktcap_config(vdev, config_value);
	else
		pkt_capture_err("failed to set packet capture config");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_PKT_CAPTURE_ID);
	return status;
}
#else
static QDF_STATUS
target_if_set_packet_capture_config
			(struct wlan_objmgr_psoc *psoc,
			 uint8_t vdev_id,
			 enum pkt_capture_config config_value)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * target_if_set_packet_capture_beacon_interval() - set packet capture beacon
 * interval
 * @psoc: pointer to psoc object
 * @vdev_id: vdev id
 * @nth_value: Beacon report period
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_set_packet_capture_beacon_interval
			(struct wlan_objmgr_psoc *psoc,
			 uint8_t vdev_id,
			 uint32_t nth_value)
{
	wmi_unified_t wmi_handle = lmac_get_wmi_unified_hdl(psoc);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct vdev_set_params param;

	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	target_if_debug("psoc:%pK, vdev_id:%d nth_value:%d",
			psoc, vdev_id, nth_value);

	param.vdev_id = vdev_id;
	param.param_id = WMI_VDEV_PARAM_NTH_BEACON_TO_HOST;
	param.param_value = nth_value;

	status = wmi_unified_vdev_set_param_send(wmi_handle, &param);
	if (QDF_IS_STATUS_ERROR(status))
		pkt_capture_err("failed to set beacon interval");

	return status;
}

/**
 * target_if_mgmt_offload_data_event_handler() - offload event handler
 * @handle: scn handle
 * @data: mgmt data
 * @data_len: data length
 *
 * Process management offload frame.
 *
 * Return: 0 for success or error code
 */
static int
target_if_mgmt_offload_data_event_handler(void *handle, uint8_t *data,
					  uint32_t data_len)
{
	static uint8_t limit_prints_invalid_len = RATE_LIMIT - 1;
	struct mgmt_offload_event_params params;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	QDF_STATUS status;
	qdf_nbuf_t wbuf;

	psoc = target_if_get_psoc_from_scn_hdl(handle);
	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid WMI handle");
		return -EINVAL;
	}

	pdev = target_if_get_pdev_from_scn_hdl(handle);
	if (!pdev) {
		pkt_capture_err("pdev is NULL");
		return -EINVAL;
	}

	if (!(wlan_pkt_capture_is_tx_mgmt_enable(pdev)))
		return -EINVAL;

	status = wmi_unified_extract_vdev_mgmt_offload_event(wmi_handle, data,
							     &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Extract mgmt offload event failed");
		return -EINVAL;
	}

	if (!params.buf) {
		pkt_capture_err("Mgmt offload buf is NULL");
		return -EINVAL;
	}

	if (params.buf_len < sizeof(struct ieee80211_hdr_3addr) ||
	    params.buf_len > data_len) {
		limit_prints_invalid_len++;
		if (limit_prints_invalid_len == RATE_LIMIT) {
			pkt_capture_debug(
			"Invalid mgmt packet, data_len %u, params.buf_len %u",
			data_len, params.buf_len);
			limit_prints_invalid_len = 0;
		}
		return -EINVAL;
	}

	wbuf = qdf_nbuf_alloc(NULL,
			      roundup(params.buf_len + RESERVE_BYTES, 4),
			      RESERVE_BYTES, 4, false);
	if (!wbuf) {
		pkt_capture_err("Failed to allocate wbuf for mgmt pkt len(%u)",
				params.buf_len);
		return -ENOMEM;
	}

	qdf_nbuf_put_tail(wbuf, params.buf_len);
	qdf_nbuf_set_protocol(wbuf, ETH_P_CONTROL);
	qdf_mem_copy(qdf_nbuf_data(wbuf), params.buf, params.buf_len);

	status = params.tx_status;
	if (QDF_STATUS_SUCCESS !=
		ucfg_pkt_capture_process_mgmt_tx_data(pdev, &params,
						      wbuf, status))
		qdf_nbuf_free(wbuf);

	return 0;
}

/**
 * target_if_register_mgmt_data_offload_event() - Register mgmt data offload
 * event handler
 * @psoc: wlan psoc object
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_register_mgmt_data_offload_event(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;

	PKT_CAPTURE_ENTER();

	if (!psoc) {
		pkt_capture_err("psoc got NULL");
		return QDF_STATUS_E_FAILURE;
	}
	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		pkt_capture_err("wmi_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if ((ucfg_pkt_capture_get_mode(psoc) != PACKET_CAPTURE_MODE_DISABLE) &&
	    wmi_service_enabled(wmi_handle,
				wmi_service_packet_capture_support)) {
		uint8_t status;

		status = wmi_unified_register_event_handler(
				wmi_handle,
				wmi_mgmt_offload_data_event_id,
				target_if_mgmt_offload_data_event_handler,
				WMI_RX_WORK_CTX);
		if (status) {
			pkt_capture_err("Failed to register MGMT offload handler");
			return QDF_STATUS_E_FAILURE;
		}
	}

	PKT_CAPTURE_EXIT();

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_unregister_mgmt_data_offload_event() - Unregister mgmt data offload
 * event handler
 * @psoc: wlan psoc object
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_unregister_mgmt_data_offload_event(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;
	QDF_STATUS status;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		pkt_capture_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_unregister_event(wmi_handle,
					      wmi_mgmt_offload_data_event_id);
	if (status)
		pkt_capture_err("unregister mgmt data offload event cb failed");

	return status;
}

#ifdef WLAN_FEATURE_PKT_CAPTURE_V2
static int
target_if_smart_monitor_event_handler(void *handle, uint8_t *data,
				      uint32_t len)
{
	struct smu_event_params params;
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;

	psoc = target_if_get_psoc_from_scn_hdl(handle);
	if (!psoc) {
		pkt_capture_err("psoc is NULL");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid WMI handle");
		return -EINVAL;
	}

	if (!(ucfg_pkt_capture_get_pktcap_mode(psoc) &
	      PKT_CAPTURE_MODE_MGMT_ONLY))
		return -EINVAL;

	status = wmi_unified_extract_smart_monitor_event(wmi_handle, data,
							 &params);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Extract smart monitor event failed");
		return -EINVAL;
	}

	tgt_pkt_capture_smu_event(psoc, &params);
	return 0;
}

/**
 * target_if_register_smart_monitor_event() - Register smu event
 * @psoc: wlan psoc object
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_register_smart_monitor_event(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;

	if (!psoc) {
		pkt_capture_err("psoc got NULL");
		return QDF_STATUS_E_FAILURE;
	}
	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		pkt_capture_err("wmi_handle is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if ((ucfg_pkt_capture_get_mode(psoc) != PACKET_CAPTURE_MODE_DISABLE) &&
	    wmi_service_enabled(wmi_handle,
				wmi_service_packet_capture_support)) {
		uint8_t status;

		status = wmi_unified_register_event_handler(
				wmi_handle,
				wmi_vdev_smart_monitor_event_id,
				target_if_smart_monitor_event_handler,
				WMI_RX_WORK_CTX);
		if (status) {
			pkt_capture_err("Failed to register smart monitor handler");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * target_if_unregister_smart_monitor_event() - Unregister smu event
 * @psoc: wlan psoc object
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
target_if_unregister_smart_monitor_event(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_handle;
	QDF_STATUS status;

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		pkt_capture_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_unregister_event(wmi_handle,
					      wmi_vdev_smart_monitor_event_id);
	if (status)
		pkt_capture_err("unregister smart monitor event handler failed");

	return status;
}
#else
static QDF_STATUS
target_if_register_smart_monitor_event(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
target_if_unregister_smart_monitor_event(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}
#endif
void
target_if_pkt_capture_register_rx_ops(struct wlan_pkt_capture_rx_ops *rx_ops)
{
	if (!rx_ops) {
		target_if_err("packet capture rx_ops is null");
		return;
	}

	rx_ops->pkt_capture_register_ev_handlers =
				target_if_register_mgmt_data_offload_event;

	rx_ops->pkt_capture_unregister_ev_handlers =
				target_if_unregister_mgmt_data_offload_event;

	rx_ops->pkt_capture_register_smart_monitor_event =
				target_if_register_smart_monitor_event;

	rx_ops->pkt_capture_unregister_smart_monitor_event =
				target_if_unregister_smart_monitor_event;
}

void
target_if_pkt_capture_register_tx_ops(struct wlan_pkt_capture_tx_ops *tx_ops)
{
	if (!tx_ops) {
		target_if_err("packet capture tx_ops is null");
		return;
	}

	tx_ops->pkt_capture_send_mode = target_if_set_packet_capture_mode;
	tx_ops->pkt_capture_send_config = target_if_set_packet_capture_config;
	tx_ops->pkt_capture_send_beacon_interval =
				target_if_set_packet_capture_beacon_interval;
}
