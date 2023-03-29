/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
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
 * DOC: target interface APIs for fw offload
 *
 */

#include "qdf_mem.h"
#include "target_if.h"
#include "qdf_status.h"
#include "wmi_unified_api.h"
#include "wmi_unified_priv.h"
#include "wmi_unified_param.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_utility.h"
#include "wlan_defs.h"
#include "wlan_fwol_public_structs.h"
#include "wlan_fw_offload_main.h"
#include "target_if_fwol.h"

#ifdef WLAN_FEATURE_ELNA
/**
 * target_if_fwol_set_elna_bypass() - send set eLNA bypass request to FW
 * @psoc: pointer to PSOC object
 * @req: set eLNA bypass request
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_fwol_set_elna_bypass(struct wlan_objmgr_psoc *psoc,
			       struct set_elna_bypass_request *req)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_send_set_elna_bypass_cmd(wmi_handle, req);
	if (status)
		target_if_err("Failed to set eLNA bypass %d", status);

	return status;
}

/**
 * target_if_fwol_get_elna_bypass() - send get eLNA bypass request to FW
 * @psoc: pointer to PSOC object
 * @req: get eLNA bypass request
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_fwol_get_elna_bypass(struct wlan_objmgr_psoc *psoc,
			       struct get_elna_bypass_request *req)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_send_get_elna_bypass_cmd(wmi_handle, req);
	if (status)
		target_if_err("Failed to set eLNA bypass %d", status);

	return status;
}

/**
 * target_if_fwol_get_elna_bypass_resp() - handler for get eLNA bypass response
 * @scn: scn handle
 * @event_buf: pointer to the event buffer
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
static int target_if_fwol_get_elna_bypass_resp(ol_scn_t scn, uint8_t *event_buf,
					       uint32_t len)
{
	QDF_STATUS status;
	struct get_elna_bypass_response resp;
	struct wlan_objmgr_psoc *psoc;
	wmi_unified_t wmi_handle;
	struct wlan_fwol_psoc_obj *fwol_obj;
	struct wlan_fwol_rx_ops *rx_ops;

	target_if_debug("scn:%pK, data:%pK, datalen:%d", scn, event_buf, len);
	if (!scn || !event_buf) {
		target_if_err("scn: 0x%pK, data: 0x%pK", scn, event_buf);
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("null psoc");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi_handle");
		return -EINVAL;
	}

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		target_if_err("Failed to get FWOL Obj");
		return -EINVAL;
	}

	rx_ops = &fwol_obj->rx_ops;
	if (rx_ops->get_elna_bypass_resp) {
		status = wmi_extract_get_elna_bypass_resp(wmi_handle,
							  event_buf, &resp);
		if (QDF_IS_STATUS_ERROR(status)) {
			target_if_err("Failed to extract eLNA bypass");
			return -EINVAL;
		}
		status = rx_ops->get_elna_bypass_resp(psoc, &resp);
		if (status != QDF_STATUS_SUCCESS) {
			target_if_err("get_elna_bypass_resp failed.");
			return -EINVAL;
		}
	} else {
		target_if_fatal("No get_elna_bypass_resp callback");
		return -EINVAL;
	}

	return 0;
};

static void
target_if_fwol_register_elna_event_handler(struct wlan_objmgr_psoc *psoc,
					   void *arg)
{
	int rc;

	rc = wmi_unified_register_event(get_wmi_unified_hdl_from_psoc(psoc),
					wmi_get_elna_bypass_event_id,
					target_if_fwol_get_elna_bypass_resp);
	if (rc)
		target_if_debug("Failed to register get eLNA bypass event cb");
}

static void
target_if_fwol_unregister_elna_event_handler(struct wlan_objmgr_psoc *psoc,
					     void *arg)
{
	int rc;

	rc = wmi_unified_unregister_event_handler(
					    get_wmi_unified_hdl_from_psoc(psoc),
					    wmi_get_elna_bypass_event_id);
	if (rc)
		target_if_debug("Failed to unregister get eLNA bypass event cb");
}

static void
target_if_fwol_register_elna_tx_ops(struct wlan_fwol_tx_ops *tx_ops)
{
	tx_ops->set_elna_bypass = target_if_fwol_set_elna_bypass;
	tx_ops->get_elna_bypass = target_if_fwol_get_elna_bypass;
}
#else
static void
target_if_fwol_register_elna_event_handler(struct wlan_objmgr_psoc *psoc,
					   void *arg)
{
}

static void
target_if_fwol_unregister_elna_event_handler(struct wlan_objmgr_psoc *psoc,
					     void *arg)
{
}

static void
target_if_fwol_register_elna_tx_ops(struct wlan_fwol_tx_ops *tx_ops)
{
}
#endif /* WLAN_FEATURE_ELNA */

#ifdef WLAN_SEND_DSCP_UP_MAP_TO_FW
/**
 * target_if_fwol_send_dscp_up_map_to_fw() - send dscp up map to FW
 * @psoc: pointer to PSOC object
 * @dscp_to_up_map: DSCP to UP map array
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_fwol_send_dscp_up_map_to_fw(struct wlan_objmgr_psoc *psoc,
				     uint32_t *dscp_to_up_map)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_send_dscp_tip_map_cmd(wmi_handle, dscp_to_up_map);
	if (status)
		target_if_err("Failed to send dscp_up_map_to_fw %d", status);

	return status;
}

static void
target_if_fwol_register_dscp_up_tx_ops(struct wlan_fwol_tx_ops *tx_ops)
{
	tx_ops->send_dscp_up_map_to_fw = target_if_fwol_send_dscp_up_map_to_fw;
}
#else
static void
target_if_fwol_register_dscp_up_tx_ops(struct wlan_fwol_tx_ops *tx_ops)
{
}
#endif

#ifdef THERMAL_STATS_SUPPORT
/**
 * target_if_fwol_get_thermal_stats() - send get thermal stats request to FW
 * @psoc: pointer to PSOC object
 * @req_type: get thermal stats request type
 * @therm_stats_offset: thermal temp stats offset for each temp range
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_fwol_get_thermal_stats(struct wlan_objmgr_psoc *psoc,
				 enum thermal_stats_request_type req_type,
				 uint8_t therm_stats_offset)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);

	if (!wmi_handle) {
		target_if_err("Invalid wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_send_get_thermal_stats_cmd(wmi_handle, req_type,
							therm_stats_offset);
	if (status)
		target_if_err("Failed to send get thermal stats cmd %d",
			      status);

	return status;
}

static void
target_if_fwol_register_thermal_stats_tx_ops(struct wlan_fwol_tx_ops *tx_ops)
{
	tx_ops->get_thermal_stats = target_if_fwol_get_thermal_stats;
}

static QDF_STATUS
target_if_fwol_handle_thermal_lvl_stats_evt(struct wlan_objmgr_psoc *psoc,
					    struct wlan_fwol_rx_ops *rx_ops,
					    struct thermal_throttle_info *info)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (rx_ops->get_thermal_stats_resp && info->therm_throt_levels)
		status = rx_ops->get_thermal_stats_resp(psoc, info);

	return status;
}

static bool
target_if_fwol_is_thermal_stats_enable(struct wlan_fwol_psoc_obj *fwol_obj)
{
	return (fwol_obj->capability_info.fw_thermal_stats_cap &&
		fwol_obj->cfg.thermal_temp_cfg.therm_stats_offset);
}

/**
 * target_if_fwol_thermal_throttle_event_handler() - handler for thermal
 *  throttle event
 * @scn: scn handle
 * @event_buf: pointer to the event buffer
 * @len: length of the buffer
 *
 * Return: 0 on success
 */
static int
target_if_fwol_thermal_throttle_event_handler(ol_scn_t scn, uint8_t *event_buf,
					      uint32_t len)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct thermal_throttle_info info = {0};
	struct wlan_objmgr_psoc *psoc;
	wmi_unified_t wmi_handle;
	struct wlan_fwol_psoc_obj *fwol_obj;
	struct wlan_fwol_rx_ops *rx_ops;

	target_if_debug("scn:%pK, data:%pK, datalen:%d", scn, event_buf, len);
	if (!scn || !event_buf)
		return -EINVAL;

	psoc = target_if_get_psoc_from_scn_hdl(scn);
	if (!psoc) {
		target_if_err("null psoc");
		return -EINVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi_handle");
		return -EINVAL;
	}

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		target_if_err("Failed to get FWOL Obj");
		return -EINVAL;
	}

	status = wmi_extract_thermal_stats(wmi_handle,
					   event_buf,
					   &info.temperature,
					   &info.level,
					   &info.therm_throt_levels,
					   info.level_info,
					   &info.pdev_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		target_if_debug("Failed to convert thermal target level");
		return -EINVAL;
	}
	rx_ops = &fwol_obj->rx_ops;
	if (!rx_ops) {
		target_if_debug("rx_ops Null");
		return -EINVAL;
	}

	status = target_if_fwol_handle_thermal_lvl_stats_evt(psoc, rx_ops,
							     &info);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_debug("thermal stats level response failed.");

	return 0;
}

/**
 * target_if_fwol_register_thermal_throttle_handler() - Register handler for
 * thermal throttle stats firmware event
 * @psoc: psoc object
 *
 * Return: void
 */
static void
target_if_fwol_register_thermal_throttle_handler(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;
	struct wlan_fwol_psoc_obj *fwol_obj;

	fwol_obj = fwol_get_psoc_obj(psoc);
	if (!fwol_obj) {
		target_if_err("Failed to get FWOL Obj");
		return;
	}
	if (!target_if_fwol_is_thermal_stats_enable(fwol_obj)) {
		target_if_debug("thermal stats offload not enabled");
		return;
	}
	status = wmi_unified_register_event_handler(
				get_wmi_unified_hdl_from_psoc(psoc),
				wmi_tt_stats_event_id,
				target_if_fwol_thermal_throttle_event_handler,
				WMI_RX_SERIALIZER_CTX);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_debug("Failed to register thermal stats event cb");
}

/**
 * target_if_fwol_unregister_thermal_stats_handler() - Register handler for
 * thermal throttle stats firmware event
 * @psoc: psoc object
 *
 * Return: void
 */
static void
target_if_fwol_unregister_thermal_throttle_handler(
					struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;

	status = wmi_unified_unregister_event_handler(
				get_wmi_unified_hdl_from_psoc(psoc),
				wmi_tt_stats_event_id);
	if (QDF_IS_STATUS_ERROR(status))
		target_if_debug("Failed to unregister thermal stats event cb");
}

#else
static void
target_if_fwol_register_thermal_throttle_handler(struct wlan_objmgr_psoc *psoc)
{
}

static void
target_if_fwol_unregister_thermal_throttle_handler(
					struct wlan_objmgr_psoc *psoc)
{
}
static void
target_if_fwol_register_thermal_stats_tx_ops(struct wlan_fwol_tx_ops *tx_ops)
{
}
#endif

QDF_STATUS target_if_fwol_register_event_handler(struct wlan_objmgr_psoc *psoc,
						 void *arg)
{
	target_if_fwol_register_elna_event_handler(psoc, arg);
	target_if_fwol_register_thermal_throttle_handler(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
target_if_fwol_unregister_event_handler(struct wlan_objmgr_psoc *psoc,
					void *arg)
{
	target_if_fwol_unregister_elna_event_handler(psoc, arg);
	target_if_fwol_unregister_thermal_throttle_handler(psoc);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS target_if_fwol_register_tx_ops(struct wlan_fwol_tx_ops *tx_ops)
{
	target_if_fwol_register_elna_tx_ops(tx_ops);
	target_if_fwol_register_dscp_up_tx_ops(tx_ops);
	target_if_fwol_register_thermal_stats_tx_ops(tx_ops);

	tx_ops->reg_evt_handler = target_if_fwol_register_event_handler;
	tx_ops->unreg_evt_handler = target_if_fwol_unregister_event_handler;

	return QDF_STATUS_SUCCESS;
}

