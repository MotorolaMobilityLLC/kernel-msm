
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

#include <wma_coex.h>
#include <wma.h>
#include "wmi_unified.h"

/**
 * wma_mws_coex_state_host_event_handler - Coex state Event Handler
 * @handle: pointer to scn handle
 * @event: Coex state event
 * @len: Length of cmd
 *
 * Return: 0 on success, else error on failure
 */
static int wma_mws_coex_state_host_event_handler(void *handle, uint8_t *event,
						 uint32_t len)
{
	WMI_VDEV_GET_MWS_COEX_STATE_EVENTID_param_tlvs *param_tlvs;
	wmi_vdev_get_mws_coex_state_fixed_param *param_buf;
	struct mws_coex_state coex_state;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	wma_debug("Enter");
	param_tlvs =
	   (WMI_VDEV_GET_MWS_COEX_STATE_EVENTID_param_tlvs *)event;
	if (!param_tlvs) {
		wma_err("Invalid stats event");
		return -EINVAL;
	}

	param_buf = param_tlvs->fixed_param;
	if (!param_buf || !mac || !mac->sme.mws_coex_info_state_resp_callback) {
		wma_debug("NULL mac ptr or HDD callback is null");
		return -EINVAL;
	}

	if (!param_buf) {
		wma_debug("NULL mws coes state event fixed param");
		return -EINVAL;
	}

	coex_state.vdev_id = param_buf->vdev_id;
	coex_state.coex_scheme_bitmap = param_buf->coex_scheme_bitmap;
	coex_state.active_conflict_count = param_buf->active_conflict_count;
	coex_state.potential_conflict_count =
				param_buf->potential_conflict_count;
	coex_state.chavd_group0_bitmap = param_buf->chavd_group0_bitmap;
	coex_state.chavd_group1_bitmap = param_buf->chavd_group1_bitmap;
	coex_state.chavd_group2_bitmap = param_buf->chavd_group2_bitmap;
	coex_state.chavd_group3_bitmap = param_buf->chavd_group3_bitmap;
	mac->sme.mws_coex_info_state_resp_callback(&coex_state,
						   mac->sme.mws_coex_info_ctx,
						   WMI_MWS_COEX_STATE);
	wma_debug("vdev_id = %u coex_scheme_bitmap = %u active_conflict_count = %u potential_conflict_count = %u chavd_group0_bitmap = %u chavd_group1_bitmap = %u chavd_group2_bitmap = %u chavd_group3_bitmap = %u",
		  param_buf->vdev_id, param_buf->coex_scheme_bitmap,
		  param_buf->active_conflict_count,
		  param_buf->potential_conflict_count,
		  param_buf->chavd_group0_bitmap,
		  param_buf->chavd_group1_bitmap,
		  param_buf->chavd_group2_bitmap,
		  param_buf->chavd_group3_bitmap);

	wma_debug("Exit");
	return 0;
}

/**
 * wma_mws_coex_state_dpwb_event_handler - DPWB state Event Handler
 * @handle: handle to scn
 * @event: DPWB state event
 * @len: Length of cmd
 *
 * Return: 0 on success, else error on failure
 */
static int wma_mws_coex_state_dpwb_event_handler(void *handle, uint8_t *event,
						 uint32_t len)
{
	WMI_VDEV_GET_MWS_COEX_DPWB_STATE_EVENTID_param_tlvs *param_tlvs;
	wmi_vdev_get_mws_coex_dpwb_state_fixed_param *param_buf;
	struct mws_coex_dpwb_state coex_dpwb;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);

	wma_debug("Enter");
	param_tlvs =
	   (WMI_VDEV_GET_MWS_COEX_DPWB_STATE_EVENTID_param_tlvs *)event;
	if (!param_tlvs) {
		wma_err("Invalid coex mws event");
		return -EINVAL;
	}

	param_buf = param_tlvs->fixed_param;
	if (!param_buf || !mac || !mac->sme.mws_coex_info_state_resp_callback) {
		wma_debug("NULL mac ptr or HDD callback is null");
		return -EINVAL;
	}

	if (!param_buf) {
		wma_err("NULL mws dpwb info event fixed param");
		return -EINVAL;
	}

	coex_dpwb.vdev_id = param_buf->vdev_id;
	coex_dpwb.current_dpwb_state = param_buf->current_dpwb_state;
	coex_dpwb.pnp1_value = param_buf->pnp1_value;
	coex_dpwb.lte_dutycycle = param_buf->lte_dutycycle;
	coex_dpwb.sinr_wlan_on = param_buf->sinr_wlan_on;
	coex_dpwb.sinr_wlan_off = param_buf->sinr_wlan_off;
	coex_dpwb.bler_count = param_buf->bler_count;
	coex_dpwb.block_count = param_buf->block_count;
	coex_dpwb.wlan_rssi_level = param_buf->wlan_rssi_level;
	coex_dpwb.wlan_rssi = param_buf->wlan_rssi;
	coex_dpwb.is_tdm_running = param_buf->is_tdm_running;
	mac->sme.mws_coex_info_state_resp_callback(&coex_dpwb,
						   mac->sme.mws_coex_info_ctx,
						   WMI_MWS_COEX_DPWB_STATE);
	wma_debug("vdev_id = %u current_dpwb_state = %d pnp1_value = %d lte_dutycycle = %d sinr_wlan_on = %d sinr_wlan_off = %d bler_count = %u block_count = %u wlan_rssi_level = %u wlan_rssi = %d is_tdm_running = %u",
		  param_buf->vdev_id, param_buf->current_dpwb_state,
		  param_buf->pnp1_value,
		  param_buf->lte_dutycycle, param_buf->sinr_wlan_on,
		  param_buf->sinr_wlan_off, param_buf->bler_count,
		  param_buf->block_count, param_buf->wlan_rssi_level,
		  param_buf->wlan_rssi, param_buf->is_tdm_running);

	wma_debug("Exit");
	return 0;
}

/**
 * wma_mws_coex_tdm_event_handler - TDM state Event Handler
 * @handle: handle to scn
 * @event: TDM state event
 * @len: Length of cmd
 *
 * Return: 0 on success, else error on failure
 */
static int wma_mws_coex_tdm_event_handler(void *handle, uint8_t *event,
					  uint32_t len)
{
	WMI_VDEV_GET_MWS_COEX_TDM_STATE_EVENTID_param_tlvs *param_tlvs;
	wmi_vdev_get_mws_coex_tdm_state_fixed_param *param_buf;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	struct mws_coex_tdm_state coex_tdm;

	wma_debug("Enter");
	param_tlvs =
	   (WMI_VDEV_GET_MWS_COEX_TDM_STATE_EVENTID_param_tlvs *)event;
	if (!param_tlvs) {
		wma_err("Invalid MWS coex event");
		return -EINVAL;
	}

	param_buf = param_tlvs->fixed_param;
	if (!param_buf || !mac || !mac->sme.mws_coex_info_state_resp_callback) {
		wma_debug("NULL mac ptr or HDD callback is null");
		return -EINVAL;
	}

	if (!param_buf) {
		wma_debug("NULL mws tdm info event fixed param");
		return -EINVAL;
	}

	coex_tdm.vdev_id = param_buf->vdev_id;
	coex_tdm.tdm_policy_bitmap = param_buf->tdm_policy_bitmap;
	coex_tdm.tdm_sf_bitmap = param_buf->tdm_sf_bitmap;

	mac->sme.mws_coex_info_state_resp_callback(&coex_tdm,
						   mac->sme.mws_coex_info_ctx,
						   WMI_MWS_COEX_TDM_STATE);
	wma_debug("vdev_id = %u tdm_policy_bitmap = %u tdm_sf_bitmap = %u",
		  param_buf->vdev_id, param_buf->tdm_policy_bitmap,
		  param_buf->tdm_sf_bitmap);

	wma_debug("Exit");
	return 0;
}

/**
 * wma_mws_coex_idrx_event_handler - IDRX state Event Handler
 * @handle: handle to scn
 * @event: IDRX state event
 * @len: Length of cmd
 *
 * Return: 0 on success, else error on failure
 */
static int wma_mws_coex_idrx_event_handler(void *handle, uint8_t *event,
					   uint32_t len)
{
	WMI_VDEV_GET_MWS_COEX_IDRX_STATE_EVENTID_param_tlvs *param_tlvs;
	wmi_vdev_get_mws_coex_idrx_state_fixed_param *param_buf;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	struct mws_coex_idrx_state coex_idrx;

	wma_debug("Enter");
	param_tlvs =
		(WMI_VDEV_GET_MWS_COEX_IDRX_STATE_EVENTID_param_tlvs *)event;
	if (!param_tlvs) {
		wma_err("Invalid stats event");
		return -EINVAL;
	}

	param_buf = param_tlvs->fixed_param;
	if (!param_buf || !mac || !mac->sme.mws_coex_info_state_resp_callback) {
		wma_debug("NULL mac ptr or HDD callback is null");
		return -EINVAL;
	}

	if (!param_buf) {
		wma_debug("NULL MWS Coex event fixed param");
		return -EINVAL;
	}

	coex_idrx.vdev_id = param_buf->vdev_id;
	coex_idrx.sub0_techid = param_buf->sub0_techid;
	coex_idrx.sub0_policy = param_buf->sub0_policy;
	coex_idrx.sub0_is_link_critical = param_buf->sub0_is_link_critical;
	coex_idrx.sub0_static_power = param_buf->sub0_static_power;
	coex_idrx.sub0_rssi = param_buf->sub0_rssi;
	coex_idrx.sub1_techid = param_buf->sub1_techid;
	coex_idrx.sub1_policy = param_buf->sub1_policy;
	coex_idrx.sub1_is_link_critical = param_buf->sub1_is_link_critical;
	coex_idrx.sub1_static_power = param_buf->sub1_static_power;
	coex_idrx.sub1_rssi = param_buf->sub1_rssi;
	mac->sme.mws_coex_info_state_resp_callback(&coex_idrx,
						   mac->sme.mws_coex_info_ctx,
						   WMI_MWS_COEX_IDRX_STATE);

	wma_debug("vdev_id = %u sub0_techid = %u sub0_policy = %u sub0_is_link_critical = %u sub0_static_power = %u sub0_rssi = %d sub1_techid = %d  sub1_policy = %d sub1_is_link_critical = %d sub1_static_power = %u sub1_rssi= %d",
		  param_buf->vdev_id, param_buf->sub0_techid,
		  param_buf->sub0_policy, param_buf->sub0_is_link_critical,
		  param_buf->sub0_static_power, param_buf->sub0_rssi,
		  param_buf->sub1_techid, param_buf->sub1_policy,
		  param_buf->sub1_is_link_critical,
		  param_buf->sub1_static_power, param_buf->sub1_rssi);

	wma_debug("EXIT");
	return 0;
}

/**
 * wma_mws_coex_antenna_sharing_event_handler - Antenna sharing Event Handler
 * @handle: handle to scn
 * @event: Antenna sharing state event
 * @len: Length of cmd
 *
 * Return: 0 on success, else error on failure
 */
static int wma_mws_coex_antenna_sharing_event_handler(void *handle,
						      uint8_t *event,
						      uint32_t len)
{
	WMI_VDEV_GET_MWS_COEX_ANTENNA_SHARING_STATE_EVENTID_param_tlvs
	*param_tlvs =
	(WMI_VDEV_GET_MWS_COEX_ANTENNA_SHARING_STATE_EVENTID_param_tlvs *)event;
	wmi_vdev_get_mws_coex_antenna_sharing_state_fixed_param *param_buf;
	struct mac_context *mac = cds_get_context(QDF_MODULE_ID_PE);
	struct mws_antenna_sharing_info *antenna_sharing;

	wma_debug("Enter");
	if (!param_tlvs) {
		wma_err("Invalid stats event");
		return -EINVAL;
	}

	param_buf = param_tlvs->fixed_param;
	if (!param_buf || !mac || !mac->sme.mws_coex_info_state_resp_callback) {
		wma_debug("NULL mac ptr or HDD callback is null");
		return -EINVAL;
	}

	if (!param_buf) {
		wma_debug("NULL MWS event fixed param");
		return -EINVAL;
	}

	antenna_sharing = qdf_mem_malloc(sizeof(*antenna_sharing));
	if (!antenna_sharing)
		return -ENOMEM;

	antenna_sharing->vdev_id = param_buf->vdev_id;
	antenna_sharing->coex_flags = param_buf->coex_flags;
	antenna_sharing->coex_config = param_buf->coex_config;
	antenna_sharing->tx_chain_mask = param_buf->tx_chain_mask;
	antenna_sharing->rx_chain_mask = param_buf->rx_chain_mask;
	antenna_sharing->rx_nss = param_buf->rx_nss;
	antenna_sharing->force_mrc = param_buf->force_mrc;
	antenna_sharing->rssi_type = param_buf->rssi_type;
	antenna_sharing->chain0_rssi = param_buf->chain0_rssi;
	antenna_sharing->chain1_rssi = param_buf->chain1_rssi;
	antenna_sharing->combined_rssi = param_buf->combined_rssi;
	antenna_sharing->imbalance = param_buf->imbalance;
	antenna_sharing->mrc_threshold = param_buf->mrc_threshold;
	antenna_sharing->grant_duration = param_buf->grant_duration;

	mac->sme.mws_coex_info_state_resp_callback(antenna_sharing,
						   mac->sme.mws_coex_info_ctx,
					WMI_MWS_COEX_ANTENNA_SHARING_STATE);
	wma_debug("vdev_id = %u coex_flags = %u coex_config = %u tx_chain_mask = %u rx_chain_mask = %u rx_nss = %u force_mrc = %u rssi_type = %u chain0_rssi = %d chain1_rssi = %d chain0_rssi = %d imbalance = %u mrc_threshold = %d grant_duration = %u",
		  param_buf->vdev_id, param_buf->coex_flags,
		  param_buf->coex_config, param_buf->tx_chain_mask,
		  param_buf->rx_chain_mask,
		  param_buf->rx_nss, param_buf->force_mrc,
		  param_buf->rssi_type, param_buf->chain0_rssi,
		  param_buf->chain1_rssi, param_buf->combined_rssi,
		  param_buf->imbalance, param_buf->mrc_threshold,
		  param_buf->grant_duration);

	qdf_mem_free(antenna_sharing);
	wma_debug("EXIT");
	return 0;
}

void wma_get_mws_coex_info_req(tp_wma_handle wma_handle,
			       struct sir_get_mws_coex_info *req)
{
	QDF_STATUS status;

	status = wmi_unified_send_mws_coex_req_cmd(wma_handle->wmi_handle,
						   req->vdev_id, req->cmd_id);

	if (QDF_IS_STATUS_ERROR(status))
		wma_err("Failed to send mws coex info");
}

void wma_register_mws_coex_events(tp_wma_handle wma_handle)
{
	if (!wma_handle) {
		wma_err("wma_handle is NULL");
		return;
	}

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				wmi_vdev_get_mws_coex_state_eventid,
				wma_mws_coex_state_host_event_handler,
				WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(
				wma_handle->wmi_handle,
				wmi_vdev_get_mws_coex_dpwb_state_eventid,
				wma_mws_coex_state_dpwb_event_handler,
				WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(
				wma_handle->wmi_handle,
				wmi_vdev_get_mws_coex_tdm_state_eventid,
				wma_mws_coex_tdm_event_handler,
				WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(
				wma_handle->wmi_handle,
				wmi_vdev_get_mws_coex_idrx_state_eventid,
				wma_mws_coex_idrx_event_handler,
				WMA_RX_SERIALIZER_CTX);

	wmi_unified_register_event_handler(
			wma_handle->wmi_handle,
			wmi_vdev_get_mws_coex_antenna_sharing_state_eventid,
			wma_mws_coex_antenna_sharing_event_handler,
			WMA_RX_SERIALIZER_CTX);
}
