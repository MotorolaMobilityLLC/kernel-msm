/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

/*
 * DOC: wlan_cm_roam_api.c
 *
 * Implementation for the Common Roaming interfaces.
 */

#include "wlan_cm_roam_api.h"
#include "wlan_vdev_mlme_api.h"
#include "wlan_mlme_main.h"
#include "wlan_policy_mgr_api.h"
#include <wmi_unified_priv.h>

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
QDF_STATUS
wlan_cm_enable_roaming_on_connected_sta(struct wlan_objmgr_pdev *pdev,
					uint8_t vdev_id)
{
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t sta_vdev_id = WLAN_INVALID_VDEV_ID;
	uint32_t count;
	uint32_t idx;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	sta_vdev_id = policy_mgr_get_roam_enabled_sta_session_id(psoc, vdev_id);
	if (sta_vdev_id != WLAN_UMAC_VDEV_ID_MAX)
		return QDF_STATUS_E_FAILURE;

	count = policy_mgr_get_mode_specific_conn_info(psoc,
						       op_ch_freq_list,
						       vdev_id_list,
						       PM_STA_MODE);

	if (!count)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Loop through all connected STA vdevs and roaming will be enabled
	 * on the STA that has a different vdev id from the one passed as
	 * input and has non zero roam trigger value.
	 */
	for (idx = 0; idx < count; idx++) {
		if (vdev_id_list[idx] != vdev_id &&
		    mlme_get_roam_trigger_bitmap(psoc, vdev_id_list[idx])) {
			sta_vdev_id = vdev_id_list[idx];
			break;
		}
	}

	if (sta_vdev_id == WLAN_INVALID_VDEV_ID)
		return QDF_STATUS_E_FAILURE;

	mlme_debug("ROAM: Enabling roaming on vdev[%d]", sta_vdev_id);

	return cm_roam_state_change(pdev,
				    sta_vdev_id,
				    WLAN_ROAM_RSO_ENABLED,
				    REASON_CTX_INIT);
}
#endif

char *cm_roam_get_requestor_string(enum wlan_cm_rso_control_requestor requestor)
{
	switch (requestor) {
	case RSO_INVALID_REQUESTOR:
	default:
		return "No requestor";
	case RSO_START_BSS:
		return "SAP start";
	case RSO_CHANNEL_SWITCH:
		return "CSA";
	case RSO_CONNECT_START:
		return "STA connection";
	case RSO_SAP_CHANNEL_CHANGE:
		return "SAP Ch switch";
	case RSO_NDP_CON_ON_NDI:
		return "NDP connection";
	case RSO_SET_PCL:
		return "Set PCL";
	}
}

QDF_STATUS
wlan_cm_rso_set_roam_trigger(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			     struct wlan_roam_triggers *trigger)
{
	QDF_STATUS status;

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	status = cm_rso_set_roam_trigger(pdev, vdev_id, trigger);

	cm_roam_release_lock();

	return status;
}

QDF_STATUS wlan_cm_disable_rso(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			       enum wlan_cm_rso_control_requestor requestor,
			       uint8_t reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	if (reason == REASON_DRIVER_DISABLED && requestor)
		mlme_set_operations_bitmap(psoc, vdev_id, requestor, false);

	mlme_debug("ROAM_CONFIG: vdev[%d] Disable roaming - requestor:%s",
		   vdev_id, cm_roam_get_requestor_string(requestor));

	status = cm_roam_state_change(pdev, vdev_id, WLAN_ROAM_RSO_STOPPED,
				      REASON_DRIVER_DISABLED);
	cm_roam_release_lock();

	return status;
}

QDF_STATUS wlan_cm_enable_rso(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
			      enum wlan_cm_rso_control_requestor requestor,
			      uint8_t reason)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	if (reason == REASON_DRIVER_ENABLED && requestor)
		mlme_set_operations_bitmap(psoc, vdev_id, requestor, true);

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	mlme_debug("ROAM_CONFIG: vdev[%d] Enable roaming - requestor:%s",
		   vdev_id, cm_roam_get_requestor_string(requestor));

	status = cm_roam_state_change(pdev, vdev_id, WLAN_ROAM_RSO_ENABLED,
				      REASON_DRIVER_ENABLED);
	cm_roam_release_lock();

	return status;
}

QDF_STATUS wlan_cm_abort_rso(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return QDF_STATUS_E_FAILURE;

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id) ||
	    wlan_cm_neighbor_roam_in_progress(psoc, vdev_id)) {
		cm_roam_release_lock();
		return QDF_STATUS_E_BUSY;
	}

	/* RSO stop cmd will be issued with lock held to avoid
	 * any racing conditions with wma/csr layer
	 */
	wlan_cm_disable_rso(pdev, vdev_id, REASON_DRIVER_DISABLED,
			    RSO_INVALID_REQUESTOR);

	cm_roam_release_lock();
	return QDF_STATUS_SUCCESS;
}

bool wlan_cm_roaming_in_progress(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	status = cm_roam_acquire_lock();
	if (QDF_IS_STATUS_ERROR(status))
		return false;

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(psoc, vdev_id) ||
	    MLME_IS_ROAMING_IN_PROG(psoc, vdev_id) ||
	    mlme_is_roam_invoke_in_progress(psoc, vdev_id) ||
	    wlan_cm_neighbor_roam_in_progress(psoc, vdev_id)) {
		cm_roam_release_lock();
		return true;
	}

	cm_roam_release_lock();

	return false;
}

QDF_STATUS wlan_cm_roam_state_change(struct wlan_objmgr_pdev *pdev,
				     uint8_t vdev_id,
				     enum roam_offload_state requested_state,
				     uint8_t reason)
{
	return cm_roam_state_change(pdev, vdev_id, requested_state, reason);
}

QDF_STATUS wlan_cm_roam_send_rso_cmd(struct wlan_objmgr_psoc *psoc,
				     uint8_t vdev_id, uint8_t rso_command,
				     uint8_t reason)
{
	return cm_roam_send_rso_cmd(psoc, vdev_id, rso_command, reason);
}

QDF_STATUS wlan_cm_roam_stop_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 uint8_t reason)
{
	return cm_roam_stop_req(psoc, vdev_id, reason);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
wlan_cm_roam_extract_btm_response(wmi_unified_t wmi, void *evt_buf,
				  struct roam_btm_response_data *dst,
				  uint8_t idx)
{
	if (wmi->ops->extract_roam_btm_response_stats)
		return wmi->ops->extract_roam_btm_response_stats(wmi, evt_buf,
								 dst, idx);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wlan_cm_roam_extract_roam_initial_info(wmi_unified_t wmi, void *evt_buf,
				       struct roam_initial_data *dst,
				       uint8_t idx)
{
	if (wmi->ops->extract_roam_initial_info)
		return wmi->ops->extract_roam_initial_info(wmi, evt_buf,
							   dst, idx);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wlan_cm_roam_extract_roam_msg_info(wmi_unified_t wmi, void *evt_buf,
				   struct roam_msg_info *dst, uint8_t idx)
{
	if (wmi->ops->extract_roam_msg_info)
		return wmi->ops->extract_roam_msg_info(wmi, evt_buf, dst, idx);

	return QDF_STATUS_E_FAILURE;
}

void wlan_cm_roam_activate_pcl_per_vdev(struct wlan_objmgr_psoc *psoc,
					uint8_t vdev_id, bool pcl_per_vdev)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	/* value - true (vdev pcl) false - pdev pcl */
	mlme_priv->cm_roam.pcl_vdev_cmd_active = pcl_per_vdev;
	mlme_debug("CM_ROAM: vdev[%d] SET PCL cmd level - [%s]", vdev_id,
		   pcl_per_vdev ? "VDEV" : "PDEV");
}

bool wlan_cm_roam_is_pcl_per_vdev_active(struct wlan_objmgr_psoc *psoc,
					 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return false;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return false;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return mlme_priv->cm_roam.pcl_vdev_cmd_active;
}

bool
wlan_cm_dual_sta_is_freq_allowed(struct wlan_objmgr_psoc *psoc,
				 uint32_t freq,
				 enum QDF_OPMODE opmode)
{
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	enum reg_wifi_band band;
	uint32_t count, connected_sta_freq;

	/*
	 * Check if already there is 1 STA connected. If this API is
	 * called for 2nd STA and if dual sta roaming is enabled, then
	 * don't allow the intra band frequencies of the 1st sta for
	 * connection on 2nd STA.
	 */
	count = policy_mgr_get_mode_specific_conn_info(psoc, op_ch_freq_list,
						       vdev_id_list,
						       PM_STA_MODE);
	if (!count || !wlan_mlme_get_dual_sta_roaming_enabled(psoc) ||
	    opmode != QDF_STA_MODE)
		return true;

	connected_sta_freq = op_ch_freq_list[0];
	band = wlan_reg_freq_to_band(connected_sta_freq);
	if ((band == REG_BAND_2G && WLAN_REG_IS_24GHZ_CH_FREQ(freq)) ||
	    (band == REG_BAND_5G && !WLAN_REG_IS_24GHZ_CH_FREQ(freq)))
		return false;

	return true;
}

void
wlan_cm_dual_sta_roam_update_connect_channels(struct wlan_objmgr_psoc *psoc,
					      struct scan_filter *filter)
{
	uint32_t i, num_channels = 0;
	uint32_t *channel_list;
	bool is_ch_allowed;
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_mlme_cfg *mlme_cfg;
	uint32_t buff_len;
	char *chan_buff;
	uint32_t len = 0;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	mlme_cfg = &mlme_obj->cfg;

	if (!wlan_mlme_get_dual_sta_roaming_enabled(psoc))
		return;

	/*
	 * Get Reg domain valid channels and update to the scan filter
	 * if already 1st sta is in connected state. Don't allow channels
	 * on which the 1st STA is connected.
	 */
	num_channels = mlme_cfg->reg.valid_channel_list_num;
	channel_list = mlme_cfg->reg.valid_channel_freq_list;

	/*
	 * Buffer of (num channl * 5) + 1  to consider the 4 char freq,
	 * 1 space after it for each channel and 1 to end the string
	 * with NULL.
	 */
	buff_len = (num_channels * 5) + 1;
	chan_buff = qdf_mem_malloc(buff_len);
	if (!chan_buff)
		return;

	filter->num_of_channels = 0;
	for (i = 0; i < num_channels; i++) {
		is_ch_allowed =
			wlan_cm_dual_sta_is_freq_allowed(psoc, channel_list[i],
							 QDF_STA_MODE);
		if (!is_ch_allowed)
			continue;

		filter->chan_freq_list[filter->num_of_channels] =
					channel_list[i];
		filter->num_of_channels++;

		len += qdf_scnprintf(chan_buff + len, buff_len - len,
				     "%d ", channel_list[i]);
	}

	if (filter->num_of_channels)
		mlme_debug("Freq list (%d): %s", filter->num_of_channels,
			   chan_buff);

	qdf_mem_free(chan_buff);
}

void
wlan_cm_roam_disable_vendor_btm(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	/* Set default value of reason code */
	mlme_obj->cfg.lfr.vendor_btm_param.user_roam_reason =
					DISABLE_VENDOR_BTM_CONFIG;
}

void
wlan_cm_roam_set_vendor_btm_params(struct wlan_objmgr_psoc *psoc,
				   struct wlan_cm_roam_vendor_btm_params *param)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	qdf_mem_copy(&mlme_obj->cfg.lfr.vendor_btm_param, param,
		     sizeof(struct wlan_cm_roam_vendor_btm_params));
}

void
wlan_cm_roam_get_vendor_btm_params(struct wlan_objmgr_psoc *psoc,
				   struct wlan_cm_roam_vendor_btm_params *param)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	qdf_mem_copy(param, &mlme_obj->cfg.lfr.vendor_btm_param,
		     sizeof(struct wlan_cm_roam_vendor_btm_params));
}

void
wlan_cm_roam_get_score_delta_params(struct wlan_objmgr_psoc *psoc,
				    struct wlan_roam_triggers *params)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	params->score_delta_param[IDLE_ROAM_TRIGGER] =
			mlme_obj->cfg.trig_score_delta[IDLE_ROAM_TRIGGER];
	params->score_delta_param[BTM_ROAM_TRIGGER] =
			mlme_obj->cfg.trig_score_delta[BTM_ROAM_TRIGGER];
}

void
wlan_cm_roam_get_min_rssi_params(struct wlan_objmgr_psoc *psoc,
				 struct wlan_roam_triggers *params)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj)
		return;

	params->min_rssi_params[DEAUTH_MIN_RSSI] =
			mlme_obj->cfg.trig_min_rssi[DEAUTH_MIN_RSSI];
	params->min_rssi_params[BMISS_MIN_RSSI] =
			mlme_obj->cfg.trig_min_rssi[BMISS_MIN_RSSI];
	params->min_rssi_params[MIN_RSSI_2G_TO_5G_ROAM] =
			mlme_obj->cfg.trig_min_rssi[MIN_RSSI_2G_TO_5G_ROAM];
}
#endif

QDF_STATUS wlan_cm_roam_cfg_get_value(struct wlan_objmgr_psoc *psoc,
				      uint8_t vdev_id,
				      enum roam_cfg_param roam_cfg_type,
				      struct cm_roam_values_copy *dst_config)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_cm_rso_configs *src_config;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	src_config = &mlme_priv->cm_roam.vdev_rso_config;
	switch (roam_cfg_type) {
	case RSSI_CHANGE_THRESHOLD:
		dst_config->int_value = src_config->rescan_rssi_delta;
		break;
	case BEACON_RSSI_WEIGHT:
		dst_config->uint_value = src_config->beacon_rssi_weight;
		break;
	case HI_RSSI_DELAY_BTW_SCANS:
		dst_config->uint_value = src_config->hi_rssi_scan_delay;
		break;
	default:
		mlme_err("Invalid roam config requested:%d", roam_cfg_type);
		status = QDF_STATUS_E_FAILURE;
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return status;
}

QDF_STATUS
wlan_cm_roam_cfg_set_value(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   enum roam_cfg_param roam_cfg_type,
			   struct cm_roam_values_copy *src_config)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_cm_rso_configs *dst_config;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	dst_config = &mlme_priv->cm_roam.vdev_rso_config;
	switch (roam_cfg_type) {
	case RSSI_CHANGE_THRESHOLD:
		dst_config->rescan_rssi_delta  = src_config->uint_value;
		break;
	case BEACON_RSSI_WEIGHT:
		dst_config->beacon_rssi_weight = src_config->uint_value;
		break;
	case HI_RSSI_DELAY_BTW_SCANS:
		dst_config->hi_rssi_scan_delay = src_config->uint_value;
		break;
	default:
		mlme_err("Invalid roam config requested:%d", roam_cfg_type);
		status = QDF_STATUS_E_FAILURE;
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return status;
}

#ifdef WLAN_FEATURE_FILS_SK
QDF_STATUS wlan_cm_update_mlme_fils_connection_info(
		struct wlan_objmgr_psoc *psoc,
		struct wlan_fils_connection_info *src_fils_info,
		uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	if (!src_fils_info) {
		mlme_debug("FILS: vdev:%d Clear fils info", vdev_id);
		qdf_mem_free(mlme_priv->fils_con_info);
		mlme_priv->fils_con_info = NULL;
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_SUCCESS;
	}

	if (mlme_priv->fils_con_info)
		qdf_mem_free(mlme_priv->fils_con_info);

	mlme_priv->fils_con_info =
		qdf_mem_malloc(sizeof(struct wlan_fils_connection_info));
	if (!mlme_priv->fils_con_info) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_NOMEM;
	}

	mlme_debug("FILS: vdev:%d update fils info", vdev_id);
	qdf_mem_copy(mlme_priv->fils_con_info, src_fils_info,
		     sizeof(struct wlan_fils_connection_info));

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

struct wlan_fils_connection_info *wlan_cm_get_fils_connection_info(
				struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_fils_connection_info *fils_info;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return NULL;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return NULL;
	}

	fils_info = mlme_priv->fils_con_info;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return fils_info;
}

QDF_STATUS wlan_cm_update_fils_ft(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id, uint8_t *fils_ft,
				  uint8_t fils_ft_len)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);
	if (!vdev) {
		mlme_err("vdev object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev legacy private object is NULL");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	if (!mlme_priv->fils_con_info || !fils_ft || !fils_ft_len ||
	    !mlme_priv->fils_con_info->is_fils_connection) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->fils_con_info->fils_ft_len = fils_ft_len;
	qdf_mem_copy(mlme_priv->fils_con_info->fils_ft, fils_ft, fils_ft_len);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS
wlan_cm_update_roam_scan_scheme_bitmap(struct wlan_objmgr_psoc *psoc,
				       uint8_t vdev_id,
				       uint32_t roam_scan_scheme_bitmap)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev%d: vdev legacy private object is NULL", vdev_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv->cm_roam.vdev_rso_config.roam_scan_scheme_bitmap =
						roam_scan_scheme_bitmap;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

uint32_t wlan_cm_get_roam_scan_scheme_bitmap(struct wlan_objmgr_psoc *psoc,
					     uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	uint32_t roam_scan_scheme_bitmap;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return 0;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev%d: vdev legacy private object is NULL", vdev_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return 0;
	}

	roam_scan_scheme_bitmap =
		mlme_priv->cm_roam.vdev_rso_config.roam_scan_scheme_bitmap;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return roam_scan_scheme_bitmap;
}

QDF_STATUS
wlan_cm_update_roam_states(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   uint32_t value, enum roam_fail_params states)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	struct wlan_cm_rso_configs *rso_config;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev%d: vdev legacy private object is NULL", vdev_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	rso_config = &mlme_priv->cm_roam.vdev_rso_config;
	switch (states) {
	case ROAM_TRIGGER_REASON:
		rso_config->roam_trigger_reason = value;
		break;
	case ROAM_INVOKE_FAIL_REASON:
		rso_config->roam_invoke_fail_reason = value;
		break;
	case ROAM_FAIL_REASON:
		rso_config->roam_fail_reason = value;
		break;
	default:
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return QDF_STATUS_SUCCESS;
}

uint32_t wlan_cm_get_roam_states(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				 enum roam_fail_params states)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;
	uint32_t roam_states = 0;
	struct wlan_cm_rso_configs *rso_config;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_MLME_NB_ID);

	if (!vdev) {
		mlme_err("vdev%d: vdev object is NULL", vdev_id);
		return 0;
	}

	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		mlme_err("vdev%d: vdev legacy private object is NULL", vdev_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return 0;
	}

	rso_config = &mlme_priv->cm_roam.vdev_rso_config;
	switch (states) {
	case ROAM_TRIGGER_REASON:
		roam_states = rso_config->roam_trigger_reason;
		break;
	case ROAM_INVOKE_FAIL_REASON:
		roam_states = rso_config->roam_invoke_fail_reason;
		break;
	case ROAM_FAIL_REASON:
		roam_states = rso_config->roam_fail_reason;
		break;
	default:
		break;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);

	return roam_states;
}

QDF_STATUS
wlan_cm_update_roam_rt_stats(struct wlan_objmgr_psoc *psoc,
			     uint8_t value, enum roam_rt_stats_params stats)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_cm_roam_rt_stats *roam_rt_stats;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_FAILURE;
	}

	roam_rt_stats = &mlme_obj->cfg.lfr.roam_rt_stats;

	switch (stats) {
	case ROAM_RT_STATS_ENABLE:
		roam_rt_stats->roam_stats_enabled = value;
		break;
	case ROAM_RT_STATS_SUSPEND_MODE_ENABLE:
		roam_rt_stats->roam_stats_wow_sent = value;
		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

uint8_t wlan_cm_get_roam_rt_stats(struct wlan_objmgr_psoc *psoc,
				  enum roam_rt_stats_params stats)
{
	struct wlan_mlme_psoc_ext_obj *mlme_obj;
	struct wlan_cm_roam_rt_stats *roam_rt_stats;
	uint8_t rstats_value = 0;

	mlme_obj = mlme_get_psoc_ext_obj(psoc);
	if (!mlme_obj) {
		mlme_legacy_err("Failed to get MLME Obj");
		return QDF_STATUS_E_FAILURE;
	}

	roam_rt_stats = &mlme_obj->cfg.lfr.roam_rt_stats;
	switch (stats) {
	case ROAM_RT_STATS_ENABLE:
		rstats_value = roam_rt_stats->roam_stats_enabled;
		break;
	case ROAM_RT_STATS_SUSPEND_MODE_ENABLE:
		rstats_value = roam_rt_stats->roam_stats_wow_sent;
		break;
	default:
		break;
	}

	return rstats_value;
}
#endif
