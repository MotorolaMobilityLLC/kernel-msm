/*
 * Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
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
 * DOC: offload lmac interface APIs for ocb
 *
 */

#include <qdf_mem.h>
#include <target_if.h>
#include <qdf_status.h>
#include <wmi_unified_api.h>
#include <wmi_unified_priv.h>
#include <wmi_unified_param.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_utility.h>
#include <wlan_defs.h>
#include <wlan_ocb_public_structs.h>
#include <wlan_ocb_main.h>
#include <target_if_ocb.h>

/**
 * target_if_ocb_get_rx_ops() - get target interface RX operations
 * @pdev: pdev handle
 *
 * Return: fp to target interface RX operations
 */
static inline struct wlan_ocb_rx_ops *
target_if_ocb_get_rx_ops(struct wlan_objmgr_pdev *pdev)
{
	struct ocb_pdev_obj *ocb_obj;

	ocb_obj = wlan_get_pdev_ocb_obj(pdev);

	return &ocb_obj->ocb_rxops;
}

/**
 * target_if_ocb_set_config() - send the OCB config to the FW
 * @psoc: pointer to PSOC object
 * @config: OCB channel configuration
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS target_if_ocb_set_config(struct wlan_objmgr_psoc *psoc,
					   struct ocb_config *config)
{
	QDF_STATUS status;

	status = wmi_unified_ocb_set_config(get_wmi_unified_hdl_from_psoc(psoc),
					    config);
	if (status)
		target_if_err("Failed to set OCB config %d", status);

	return status;
}

/**
 * target_if_ocb_set_utc_time() - send the UTC time to the firmware
 * @psoc: pointer to PSOC object
 * @utc: pointer to the UTC time structure
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS target_if_ocb_set_utc_time(struct wlan_objmgr_psoc *psoc,
					     struct ocb_utc_param *utc)
{
	QDF_STATUS status;

	status = wmi_unified_ocb_set_utc_time_cmd(
			get_wmi_unified_hdl_from_psoc(psoc), utc);
	if (status)
		target_if_err("Failed to set OCB UTC time %d", status);

	return status;
}

/**
 * target_if_ocb_start_timing_advert() - start sending the timing
 *  advertisement frame on a channel
 * @psoc: pointer to PSOC object
 * @ta: pointer to the timing advertisement
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_ocb_start_timing_advert(struct wlan_objmgr_psoc *psoc,
				  struct ocb_timing_advert_param *ta)
{
	QDF_STATUS status;

	status = wmi_unified_ocb_start_timing_advert(
			get_wmi_unified_hdl_from_psoc(psoc), ta);
	if (status)
		target_if_err("Failed to start OCB timing advert %d", status);

	return status;
}

/**
 * target_if_ocb_stop_timing_advert() - stop sending the timing
 *   advertisement frame on a channel
 * @psoc: pointer to PSOC object
 * @ta: pointer to the timing advertisement
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_ocb_stop_timing_advert(struct wlan_objmgr_psoc *psoc,
				 struct ocb_timing_advert_param *ta)
{
	QDF_STATUS status;

	status =
		wmi_unified_ocb_stop_timing_advert(
				get_wmi_unified_hdl_from_psoc(psoc), ta);
	if (status)
		target_if_err("Failed to stop OCB timing advert %d", status);

	return status;
}

/**
 * target_if_ocb_get_tsf_timer() - get tsf timer
 * @psoc: pointer to PSOC object
 * @request: pointer to the request
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_ocb_get_tsf_timer(struct wlan_objmgr_psoc *psoc,
			    struct ocb_get_tsf_timer_param *request)
{
	QDF_STATUS status;

	status = wmi_unified_ocb_get_tsf_timer(
			get_wmi_unified_hdl_from_psoc(psoc), request);
	if (status)
		target_if_err("Failed to send get tsf timer cmd: %d", status);

	return status;
}

/**
 * target_if_dcc_get_stats() - get the DCC channel stats
 * @psoc: pointer to PSOC object
 * @get_stats_param: pointer to the dcc stats request
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_dcc_get_stats(struct wlan_objmgr_psoc *psoc,
			struct ocb_dcc_get_stats_param *get_stats_param)
{
	QDF_STATUS status;

	status = wmi_unified_dcc_get_stats_cmd(
			get_wmi_unified_hdl_from_psoc(psoc), get_stats_param);
	if (status)
		target_if_err("Failed to send get DCC stats cmd: %d", status);

	return status;
}

/**
 * target_if_dcc_clear_stats() - send command to clear the DCC stats
 * @psoc: pointer to PSOC object
 * @clear_stats_param: parameters to the command
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_dcc_clear_stats(struct wlan_objmgr_psoc *psoc,
			  struct ocb_dcc_clear_stats_param *clear_stats_param)
{
	QDF_STATUS status;

	status = wmi_unified_dcc_clear_stats(
			get_wmi_unified_hdl_from_psoc(psoc), clear_stats_param);
	if (status)
		target_if_err("Failed to send clear DCC stats cmd: %d", status);

	return status;
}

/**
 * target_if_dcc_update_ndl() - command to update the NDL data
 * @psoc: pointer to PSOC object
 * @update_ndl_param: pointer to the request parameters
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_dcc_update_ndl(struct wlan_objmgr_psoc *psoc,
			 struct ocb_dcc_update_ndl_param *update_ndl_param)
{
	QDF_STATUS status;

	/* Send the WMI command */
	status = wmi_unified_dcc_update_ndl(get_wmi_unified_hdl_from_psoc(psoc),
					    update_ndl_param);
	if (status)
		target_if_err("Failed to send NDL update cmd: %d", status);

	return status;
}

/**
 * target_if_ocb_set_config_resp() - handler for channel config response
 * @scn: scn handle
 * @event_buf: pointer to the event buffer
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
static int
target_if_ocb_set_config_resp(ol_scn_t scn, uint8_t *event_buf,
			      uint32_t len)
{
	int rc;
	QDF_STATUS status;
	uint32_t resp;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_ocb_rx_ops *ocb_rx_ops;

	target_if_debug("scn:%pK, data:%pK, datalen:%d",
			scn, event_buf, len);
	if (!scn || !event_buf) {
		target_if_err("scn: 0x%pK, data: 0x%pK", scn, event_buf);
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("null psoc");
		return -EINVAL;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_OCB_SB_ID);
	if (!pdev) {
		target_if_err("pdev is NULL");
		return -EINVAL;
	}

	ocb_rx_ops = target_if_ocb_get_rx_ops(pdev);
	if (ocb_rx_ops->ocb_set_config_status) {
		status = wmi_extract_ocb_set_channel_config_resp(
					get_wmi_unified_hdl_from_psoc(psoc),
					event_buf, &resp);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("Failed to extract config status");
			rc = -EINVAL;
			goto exit;
		}
		status = ocb_rx_ops->ocb_set_config_status(psoc, resp);
		if (status != QDF_STATUS_SUCCESS) {
			target_if_err("ocb_set_config_status failed.");
			rc = -EINVAL;
			goto exit;
		}
		rc = 0;
	} else {
		target_if_fatal("No ocb_set_config_status callback");
		rc = -EINVAL;
	}
exit:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_OCB_SB_ID);

	return rc;
};

/**
 * target_if_ocb_get_tsf_timer_resp() - handler for TSF timer response
 * @scn: scn handle
 * @event_buf: pointer to the event buffer
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
static int target_if_ocb_get_tsf_timer_resp(ol_scn_t scn,
					    uint8_t *event_buf,
					    uint32_t len)
{
	int rc;
	QDF_STATUS status;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	struct ocb_get_tsf_timer_response response;
	struct wlan_ocb_rx_ops *ocb_rx_ops;

	target_if_debug("scn:%pK, data:%pK, datalen:%d",
			scn, event_buf, len);

	if (!scn || !event_buf) {
		target_if_err("scn: 0x%pK, data: 0x%pK", scn, event_buf);
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("null psoc");
		return -EINVAL;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_OCB_SB_ID);
	if (!pdev) {
		target_if_err("pdev is NULL");
		return -EINVAL;
	}

	ocb_rx_ops = target_if_ocb_get_rx_ops(pdev);
	if (ocb_rx_ops->ocb_tsf_timer) {
		status = wmi_extract_ocb_tsf_timer(
			get_wmi_unified_hdl_from_psoc(psoc),
			event_buf, &response);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("Failed to extract tsf timer");
			rc = -EINVAL;
			goto exit;
		}
		status = ocb_rx_ops->ocb_tsf_timer(psoc, &response);
		if (status != QDF_STATUS_SUCCESS) {
			target_if_err("ocb_tsf_timer failed.");
			rc = -EINVAL;
			goto exit;
		}
		rc = 0;
	} else {
		target_if_fatal("No ocb_tsf_timer callback");
		rc = -EINVAL;
	}
exit:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_OCB_SB_ID);

	return rc;
}

/**
 * target_if_dcc_update_ndl_resp() - handler for update NDL response
 * @scn: scn handle
 * @event_buf: pointer to the event buffer
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
static int target_if_dcc_update_ndl_resp(ol_scn_t scn,
					 uint8_t *event_buf,
					 uint32_t len)
{
	int rc;
	QDF_STATUS status;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	struct ocb_dcc_update_ndl_response *resp;
	struct wlan_ocb_rx_ops *ocb_rx_ops;

	target_if_debug("scn:%pK, data:%pK, datalen:%d",
			scn, event_buf, len);

	if (!scn || !event_buf) {
		target_if_err("scn: 0x%pK, data: 0x%pK", scn, event_buf);
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("null psoc");
		return -EINVAL;
	}
	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_OCB_SB_ID);
	if (!pdev) {
		target_if_err("pdev is NULL");
		return -EINVAL;
	}

	/* Allocate and populate the response */
	resp = qdf_mem_malloc(sizeof(*resp));
	if (!resp) {
		rc = -ENOMEM;
		goto exit;
	}

	ocb_rx_ops = target_if_ocb_get_rx_ops(pdev);
	if (ocb_rx_ops->ocb_dcc_ndl_update) {
		status = wmi_extract_dcc_update_ndl_resp(
					get_wmi_unified_hdl_from_psoc(psoc),
					event_buf, resp);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("Failed to extract ndl status");
			rc = -EINVAL;
			goto exit;
		}
		status = ocb_rx_ops->ocb_dcc_ndl_update(psoc, resp);
		if (status != QDF_STATUS_SUCCESS) {
			target_if_err("dcc_ndl_update failed.");
			rc = -EINVAL;
			goto exit;
		}
		rc = 0;
	} else {
		target_if_fatal("No dcc_ndl_update callback");
		rc = -EINVAL;
	}
exit:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_OCB_SB_ID);
	if (resp)
		qdf_mem_free(resp);

	return rc;
}

/**
 * target_if_dcc_get_stats_resp() - handler for get stats response
 * @scn: scn handle
 * @event_buf: pointer to the event buffer
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
static int target_if_dcc_get_stats_resp(ol_scn_t scn,
					uint8_t *event_buf,
					uint32_t len)
{
	int rc;
	QDF_STATUS status;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	struct ocb_dcc_get_stats_response *response;
	struct wlan_ocb_rx_ops *ocb_rx_ops;

	target_if_debug("scn:%pK, data:%pK, datalen:%d",
			scn, event_buf, len);

	if (!scn || !event_buf) {
		target_if_err("scn: 0x%pK, data: 0x%pK", scn, event_buf);
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("null psoc");
		return -EINVAL;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_OCB_SB_ID);
	if (!pdev) {
		target_if_err("pdev is NULL");
		return -EINVAL;
	}

	ocb_rx_ops = target_if_ocb_get_rx_ops(pdev);
	if (ocb_rx_ops->ocb_dcc_stats_indicate) {
		status = wmi_extract_dcc_stats(
			get_wmi_unified_hdl_from_psoc(psoc),
			event_buf, &response);
		if (!response || QDF_IS_STATUS_ERROR(status)) {
			target_if_err("Cannot get DCC stats");
			rc = -ENOMEM;
			goto exit;
		}

		status = ocb_rx_ops->ocb_dcc_stats_indicate(psoc,
							    response,
							    true);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("dcc_stats_indicate failed.");
			rc = -EINVAL;
			goto exit;
		}
		rc = 0;
	} else {
		target_if_fatal("No dcc_stats_indicate callback");
		rc = -EINVAL;
	}
exit:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_OCB_SB_ID);
	if (response)
		qdf_mem_free(response);

	return rc;
}

/**
 * target_if_dcc_stats_resp() - handler for DCC stats indication event
 * @scn: scn handle
 * @event_buf: pointer to the event buffer
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
static int target_if_dcc_stats_resp(ol_scn_t scn, uint8_t *event_buf,
				    uint32_t len)
{
	int rc;
	QDF_STATUS status;
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	struct ocb_dcc_get_stats_response *response;
	struct wlan_ocb_rx_ops *ocb_rx_ops;

	target_if_debug("scn:%pK, data:%pK, datalen:%d",
			scn, event_buf, len);

	if (!scn || !event_buf) {
		target_if_err("scn: 0x%pK, data: 0x%pK", scn, event_buf);
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("null psoc");
		return -EINVAL;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, 0,
					  WLAN_OCB_SB_ID);
	if (!pdev) {
		target_if_err("pdev is NULL");
		return -EINVAL;
	}

	ocb_rx_ops = target_if_ocb_get_rx_ops(pdev);
	if (ocb_rx_ops->ocb_dcc_stats_indicate) {
		status = wmi_extract_dcc_stats(
			get_wmi_unified_hdl_from_psoc(psoc),
			event_buf, &response);
		if (!response || QDF_IS_STATUS_ERROR(status)) {
			target_if_err("Cannot get DCC stats");
			rc = -ENOMEM;
			goto exit;
		}
		status = ocb_rx_ops->ocb_dcc_stats_indicate(psoc,
							    response,
							    false);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("dcc_stats_indicate failed.");
			rc = -EINVAL;
			goto exit;
		}
		rc = 0;
	} else {
		target_if_fatal("dcc_stats_indicate failed.");
		response = NULL;
		rc = -EINVAL;
	}
exit:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_OCB_SB_ID);
	if (response)
		qdf_mem_free(response);

	return rc;
}

QDF_STATUS target_if_ocb_register_event_handler(struct wlan_objmgr_psoc *psoc,
						void *arg)
{
	int rc;

	/* Initialize the members in WMA used by wma_ocb */
	rc = wmi_unified_register_event(get_wmi_unified_hdl_from_psoc(psoc),
			wmi_ocb_set_config_resp_event_id,
			target_if_ocb_set_config_resp);
	if (rc) {
		target_if_err("Failed to register OCB config resp event cb");
		return QDF_STATUS_E_FAILURE;
	}

	rc = wmi_unified_register_event(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_ocb_get_tsf_timer_resp_event_id,
			target_if_ocb_get_tsf_timer_resp);
	if (rc) {
		target_if_err("Failed to register OCB TSF resp event cb");
		goto unreg_set_config;
	}

	rc = wmi_unified_register_event(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_dcc_get_stats_resp_event_id,
			target_if_dcc_get_stats_resp);
	if (rc) {
		target_if_err("Failed to register DCC get stats resp event cb");
		goto unreg_tsf_timer;
	}

	rc = wmi_unified_register_event(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_dcc_update_ndl_resp_event_id,
			target_if_dcc_update_ndl_resp);
	if (rc) {
		target_if_err("Failed to register NDL update event cb");
		goto unreg_get_stats;
	}

	rc = wmi_unified_register_event(get_wmi_unified_hdl_from_psoc(psoc),
			wmi_dcc_stats_event_id,
			target_if_dcc_stats_resp);
	if (rc) {
		target_if_err("Failed to register DCC stats event cb");
		goto unreg_ndl;
	}

	return QDF_STATUS_SUCCESS;

unreg_ndl:
	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_dcc_update_ndl_resp_event_id);
unreg_get_stats:
	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_dcc_get_stats_resp_event_id);
unreg_tsf_timer:
	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_ocb_get_tsf_timer_resp_event_id);
unreg_set_config:
	wmi_unified_unregister_event(get_wmi_unified_hdl_from_psoc(psoc),
			wmi_ocb_set_config_resp_event_id);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
target_if_ocb_unregister_event_handler(struct wlan_objmgr_psoc *psoc,
				       void *arg)
{
	int rc;

	rc = wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_dcc_stats_event_id);
	if (rc)
		target_if_err("Failed to unregister DCC stats event cb");

	rc = wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_dcc_update_ndl_resp_event_id);
	if (rc)
		target_if_err("Failed to unregister NDL update event cb");

	rc = wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_dcc_get_stats_resp_event_id);
	if (rc)
		target_if_err("Failed to unregister DCC get stats resp cb");

	rc = wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_ocb_get_tsf_timer_resp_event_id);
	if (rc)
		target_if_err("Failed to unregister OCB TSF resp event cb");

	rc = wmi_unified_unregister_event(get_wmi_unified_hdl_from_psoc(psoc),
			wmi_ocb_set_config_resp_event_id);
	if (rc)
		target_if_err("Failed to unregister OCB config resp event cb");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
target_if_ocb_register_tx_ops(struct wlan_ocb_tx_ops *ocb_txops)
{
	ocb_txops->ocb_set_config = target_if_ocb_set_config;
	ocb_txops->ocb_set_utc_time = target_if_ocb_set_utc_time;
	ocb_txops->ocb_start_timing_advert = target_if_ocb_start_timing_advert;
	ocb_txops->ocb_stop_timing_advert = target_if_ocb_stop_timing_advert;
	ocb_txops->ocb_get_tsf_timer = target_if_ocb_get_tsf_timer;
	ocb_txops->ocb_dcc_get_stats = target_if_dcc_get_stats;
	ocb_txops->ocb_dcc_clear_stats = target_if_dcc_clear_stats;
	ocb_txops->ocb_dcc_update_ndl = target_if_dcc_update_ndl;
	ocb_txops->ocb_reg_ev_handler = target_if_ocb_register_event_handler;
	ocb_txops->ocb_unreg_ev_handler =
			target_if_ocb_unregister_event_handler;

	return QDF_STATUS_SUCCESS;
}
