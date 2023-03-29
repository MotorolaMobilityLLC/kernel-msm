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

/**
 * DOC: wlan_hdd_dcs.c
 *
 * DCS implementation.
 *
 */

#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_dcs.h>
#include <wlan_hdd_includes.h>
#include <wlan_dcs_ucfg_api.h>
/**
 * hdd_dcs_cb() - hdd dcs specific callback
 * @psoc: psoc
 * @mac_id: mac_id
 * @interference_type: wlan or continuous wave interference type
 * @arg: List of arguments
 *
 * This callback is registered with dcs component to start acs operation
 *
 * Return: None
 */
static void hdd_dcs_cb(struct wlan_objmgr_psoc *psoc, uint8_t mac_id,
		       uint8_t interference_type, void *arg)
{
	struct hdd_context *hdd_ctx = (struct hdd_context *)arg;
	struct hdd_adapter *adapter;
	uint32_t count;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t index;

	/*
	 * so far CAP_DCS_CWIM interference mitigation is not supported
	 */
	if (interference_type == CAP_DCS_CWIM) {
		hdd_debug("CW interference mitigation is not supported");
		return;
	}

	if (policy_mgr_is_force_scc(psoc) &&
	    policy_mgr_is_sta_gc_active_on_mac(psoc, mac_id)) {
		hdd_debug("force scc %d, mac id %d sta gc count %d",
			  policy_mgr_is_force_scc(psoc), mac_id,
			  policy_mgr_is_sta_gc_active_on_mac(psoc, mac_id));
		return;
	}

	count = policy_mgr_get_sap_go_count_on_mac(psoc, list, mac_id);
	for (index = 0; index < count; index++) {
		adapter = hdd_get_adapter_by_vdev(hdd_ctx, list[index]);
		if (!adapter) {
			hdd_err("vdev_id %u does not exist with host",
				list[index]);
			return;
		}

		if (wlansap_dcs_is_wlan_interference_mitigation_enabled(
			WLAN_HDD_GET_SAP_CTX_PTR(adapter))) {
			hdd_debug("DCS triggers ACS on vdev_id=%u, mac_id=%u",
				  list[index], mac_id);
			wlan_hdd_cfg80211_start_acs(adapter);
			return;
		}
	}
}

void hdd_dcs_register_cb(struct hdd_context *hdd_ctx)
{
	ucfg_dcs_register_cb(hdd_ctx->psoc, hdd_dcs_cb, hdd_ctx);
}

void hdd_dcs_hostapd_set_chan(struct hdd_context *hdd_ctx,
			      uint8_t vdev_id,
			      qdf_freq_t dcs_ch_freq)
{
	QDF_STATUS status;
	uint8_t mac_id;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t conn_index, count;
	struct hdd_adapter *adapter;
	uint32_t dcs_ch = wlan_reg_freq_to_chan(hdd_ctx->pdev, dcs_ch_freq);

	status = policy_mgr_get_mac_id_by_session_id(hdd_ctx->psoc, vdev_id,
						     &mac_id);

	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("get mac id failed");
		return;
	}
	count = policy_mgr_get_sap_go_count_on_mac(hdd_ctx->psoc, list, mac_id);

	/*
	 * Dcs can only be enabled after all vdev finish csa.
	 * Set vdev starting for every vdev before doing csa.
	 * The CSA triggered by DCS will be done in serial.
	 */
	for (conn_index = 0; conn_index < count; conn_index++) {
		adapter = hdd_get_adapter_by_vdev(hdd_ctx, list[conn_index]);
		if (!adapter) {
			hdd_err("vdev_id %u does not exist with host",
				list[conn_index]);
			return;
		}

		if (adapter->session.ap.operating_chan_freq != dcs_ch_freq)
			wlansap_dcs_set_vdev_starting(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter), true);
		else
			wlansap_dcs_set_vdev_starting(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter), false);
	}
	for (conn_index = 0; conn_index < count; conn_index++) {
		adapter = hdd_get_adapter_by_vdev(hdd_ctx, list[conn_index]);
		if (!adapter) {
			hdd_err("vdev_id %u does not exist with host",
				list[conn_index]);
			return;
		}

		if (adapter->session.ap.operating_chan_freq != dcs_ch_freq) {
			hdd_ctx->acs_policy.acs_chan_freq = AUTO_CHANNEL_SELECT;
			hdd_debug("dcs triggers old ch:%d new ch:%d",
				  adapter->session.ap.operating_chan_freq,
				  dcs_ch_freq);
			wlan_hdd_set_sap_csa_reason(hdd_ctx->psoc,
						    adapter->vdev_id,
						    CSA_REASON_DCS);
			hdd_switch_sap_channel(adapter, dcs_ch, true);

			return;
		}
	}
}

/**
 * hdd_dcs_hostapd_enable_wlan_interference_mitigation() - enable wlan
 * interference mitigation
 * @hdd_ctx: hdd ctx
 * @vdev_id: vdev id
 *
 * This function is used to enable wlan interference mitigation through
 * send dcs command.
 *
 * Return: None
 */
static void hdd_dcs_hostapd_enable_wlan_interference_mitigation(
					struct hdd_context *hdd_ctx,
					uint8_t vdev_id)
{
	QDF_STATUS status;
	uint8_t mac_id;
	struct hdd_adapter *adapter;

	status = policy_mgr_get_mac_id_by_session_id(hdd_ctx->psoc, vdev_id,
						     &mac_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("get mac id failed");
		return;
	}
	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("vdev_id %u does not exist with host", vdev_id);
		return;
	}

	if (wlansap_dcs_is_wlan_interference_mitigation_enabled(
			WLAN_HDD_GET_SAP_CTX_PTR(adapter)) &&
	    wlan_reg_is_5ghz_ch_freq(adapter->session.ap.operating_chan_freq))
		ucfg_config_dcs_enable(hdd_ctx->psoc, mac_id, CAP_DCS_WLANIM);
	ucfg_wlan_dcs_cmd(hdd_ctx->psoc, mac_id, true);
}

void hdd_dcs_chan_select_complete(struct hdd_adapter *adapter)
{
	qdf_freq_t dcs_freq;
	struct hdd_context *hdd_ctx;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD context pointer");
		return;
	}

	dcs_freq = wlansap_dcs_get_freq(WLAN_HDD_GET_SAP_CTX_PTR(adapter));
	if (dcs_freq && dcs_freq != adapter->session.ap.operating_chan_freq)
		hdd_dcs_hostapd_set_chan(hdd_ctx, adapter->vdev_id, dcs_freq);
	else
		hdd_dcs_hostapd_enable_wlan_interference_mitigation(
							hdd_ctx,
							adapter->vdev_id);
}

void hdd_dcs_clear(struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	uint8_t mac_id;
	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_psoc *psoc;
	uint32_t list[MAX_NUMBER_OF_CONC_CONNECTIONS];

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (!hdd_ctx) {
		hdd_err("Invalid HDD context pointer");
		return;
	}

	psoc = hdd_ctx->psoc;

	if (wlansap_dcs_is_wlan_interference_mitigation_enabled(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter))) {
		status = policy_mgr_get_mac_id_by_session_id(psoc,
							     adapter->vdev_id,
							     &mac_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("get mac id failed");
			return;
		}
		if (policy_mgr_get_sap_go_count_on_mac(psoc, list, mac_id) <= 1)
			ucfg_dcs_clear(psoc, mac_id);
	}

	wlansap_dcs_set_vdev_wlan_interference_mitigation(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter), false);
	wlansap_dcs_set_vdev_starting(
				WLAN_HDD_GET_SAP_CTX_PTR(adapter), false);
}
