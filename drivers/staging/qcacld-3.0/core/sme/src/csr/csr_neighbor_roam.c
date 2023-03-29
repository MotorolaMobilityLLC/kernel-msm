/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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

/*
 * DOC: csr_neighbor_roam.c
 *
 * Implementation for the simple roaming algorithm for 802.11r Fast
 * transitions and Legacy roaming for Android platform.
 */

#include "wma_types.h"
#include "csr_inside_api.h"
#include "sme_qos_internal.h"
#include "sme_inside.h"
#include "host_diag_core_event.h"
#include "host_diag_core_log.h"
#include "csr_api.h"
#include "sme_api.h"
#include "csr_neighbor_roam.h"
#include "mac_trace.h"
#include "wlan_policy_mgr_api.h"
#include "wlan_mlme_api.h"

static const char *lfr_get_config_item_string(uint8_t reason)
{
	switch (reason) {
	CASE_RETURN_STRING(REASON_LOOKUP_THRESH_CHANGED);
	CASE_RETURN_STRING(REASON_OPPORTUNISTIC_THRESH_DIFF_CHANGED);
	CASE_RETURN_STRING(REASON_ROAM_RESCAN_RSSI_DIFF_CHANGED);
	CASE_RETURN_STRING(REASON_ROAM_BMISS_FIRST_BCNT_CHANGED);
	CASE_RETURN_STRING(REASON_ROAM_BMISS_FINAL_BCNT_CHANGED);
	default:
		return "unknown";
	}
}

static void csr_neighbor_roam_reset_channel_info(tpCsrNeighborRoamChannelInfo
						 rChInfo);

void csr_neighbor_roam_state_transition(struct mac_context *mac_ctx,
		uint8_t newstate, uint8_t session)
{
	mac_ctx->roam.neighborRoamInfo[session].prevNeighborRoamState =
		mac_ctx->roam.neighborRoamInfo[session].neighborRoamState;
	mac_ctx->roam.neighborRoamInfo[session].neighborRoamState = newstate;
	sme_nofl_debug("NeighborRoam transition: vdev %d %s --> %s",
		session, csr_neighbor_roam_state_to_string(
		mac_ctx->roam.neighborRoamInfo[session].prevNeighborRoamState),
		csr_neighbor_roam_state_to_string(newstate));
}

uint8_t *csr_neighbor_roam_state_to_string(uint8_t state)
{
	switch (state) {
		CASE_RETURN_STRING(eCSR_NEIGHBOR_ROAM_STATE_CLOSED);
		CASE_RETURN_STRING(eCSR_NEIGHBOR_ROAM_STATE_INIT);
		CASE_RETURN_STRING(eCSR_NEIGHBOR_ROAM_STATE_CONNECTED);
		CASE_RETURN_STRING(eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING);
		CASE_RETURN_STRING(eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING);
		CASE_RETURN_STRING(eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE);
	default:
		return "eCSR_NEIGHBOR_ROAM_STATE_UNKNOWN";
	}

}

#ifdef FEATURE_WLAN_LFR_METRICS
/**
 * csr_neighbor_roam_send_lfr_metric_event() - Send event of LFR metric
 * @mac_ctx: Handle returned by mac_open.
 * @session_id: Session information
 * @bssid: BSSID of attempted AP
 * @status: Result of LFR operation
 *
 * LFR metrics - pre-auth completion metric
 * Send the event to supplicant indicating pre-auth result
 *
 * Return: void
 */

void csr_neighbor_roam_send_lfr_metric_event(
				struct mac_context *mac_ctx,
				uint8_t session_id,
				tSirMacAddr bssid,
				eRoamCmdStatus status)
{
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(struct csr_roam_info));
	if (roam_info) {
		qdf_mem_copy((void *)roam_info->bssid.bytes,
			     (void *)bssid, sizeof(struct qdf_mac_addr));
		csr_roam_call_callback(mac_ctx, session_id, roam_info, 0,
			status, 0);
		qdf_mem_free(roam_info);
	}
}
#endif

/**
 * csr_neighbor_roam_update_fast_roaming_enabled() - update roaming capability
 *
 * @mac_ctx: Global MAC context
 * @session_id: Session
 * @fast_roam_enabled: Is fast roaming enabled on this device?
 *                     This capability can be changed dynamically.
 *
 * Return: None
 */
QDF_STATUS csr_neighbor_roam_update_fast_roaming_enabled(struct mac_context *mac_ctx,
						uint8_t session_id,
						const bool fast_roam_enabled)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	tpCsrNeighborRoamControlInfo neighbor_roam_info =
		&mac_ctx->roam.neighborRoamInfo[session_id];

	switch (neighbor_roam_info->neighborRoamState) {
	case eCSR_NEIGHBOR_ROAM_STATE_CONNECTED:
		qdf_status = sme_acquire_global_lock(&mac_ctx->sme);
		if (QDF_IS_STATUS_ERROR(qdf_status))
			break;

		mlme_set_supplicant_disabled_roaming(mac_ctx->psoc, session_id,
						     !fast_roam_enabled);
		if (fast_roam_enabled) {
			csr_post_roam_state_change(mac_ctx, session_id,
						   WLAN_ROAM_RSO_ENABLED,
						   REASON_CONNECT);
		} else {
			csr_post_roam_state_change(mac_ctx, session_id,
					    WLAN_ROAM_RSO_STOPPED,
					    REASON_SUPPLICANT_DISABLED_ROAMING);
		}
		sme_release_global_lock(&mac_ctx->sme);
		break;
	case eCSR_NEIGHBOR_ROAM_STATE_INIT:
		sme_debug("Currently in INIT state, Nothing to do");
		break;
	default:
		sme_err("Unexpected state %s, returning failure",
			    mac_trace_get_neighbour_roam_state
			    (neighbor_roam_info->neighborRoamState));
		qdf_status = QDF_STATUS_E_FAILURE;
		break;
	}
	return qdf_status;
}

QDF_STATUS csr_neighbor_roam_update_config(struct mac_context *mac_ctx,
		uint8_t session_id, uint8_t value, uint8_t reason)
{
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
	    &mac_ctx->roam.neighborRoamInfo[session_id];
	tpCsrNeighborRoamCfgParams cfg_params;
	struct cm_roam_values_copy src_cfg;
	eCsrNeighborRoamState state;
	uint8_t old_value;

	if (!pNeighborRoamInfo) {
		sme_err("Invalid Session ID %d", session_id);
		return QDF_STATUS_E_FAILURE;
	}
	cfg_params = &pNeighborRoamInfo->cfgParams;
	state = pNeighborRoamInfo->neighborRoamState;
	if ((state == eCSR_NEIGHBOR_ROAM_STATE_CONNECTED) ||
			state == eCSR_NEIGHBOR_ROAM_STATE_INIT) {
		switch (reason) {
		case REASON_LOOKUP_THRESH_CHANGED:
			old_value = cfg_params->neighborLookupThreshold;
			cfg_params->neighborLookupThreshold = value;
			pNeighborRoamInfo->currentNeighborLookupThreshold =
				value;
			break;
		case REASON_OPPORTUNISTIC_THRESH_DIFF_CHANGED:
			old_value = cfg_params->nOpportunisticThresholdDiff;
			cfg_params->nOpportunisticThresholdDiff = value;
			pNeighborRoamInfo->currentOpportunisticThresholdDiff =
				value;
			break;
		case REASON_ROAM_RESCAN_RSSI_DIFF_CHANGED:
			old_value = cfg_params->nRoamRescanRssiDiff;
			cfg_params->nRoamRescanRssiDiff = value;
			pNeighborRoamInfo->currentRoamRescanRssiDiff = value;
			src_cfg.uint_value = value;
			wlan_cm_roam_cfg_set_value(mac_ctx->psoc, session_id,
						   RSSI_CHANGE_THRESHOLD,
						   &src_cfg);
			break;
		case REASON_ROAM_BMISS_FIRST_BCNT_CHANGED:
			old_value = cfg_params->nRoamBmissFirstBcnt;
			cfg_params->nRoamBmissFirstBcnt = value;
			pNeighborRoamInfo->currentRoamBmissFirstBcnt = value;
			break;
		case REASON_ROAM_BMISS_FINAL_BCNT_CHANGED:
			old_value = cfg_params->nRoamBmissFinalBcnt;
			cfg_params->nRoamBmissFinalBcnt = value;
			pNeighborRoamInfo->currentRoamBmissFinalBcnt = value;
			break;
		default:
			sme_debug("Unknown update cfg reason");
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		sme_err("Unexpected state %s, return fail",
			mac_trace_get_neighbour_roam_state(state));
		return QDF_STATUS_E_FAILURE;
	}
	if (state == eCSR_NEIGHBOR_ROAM_STATE_CONNECTED) {
		sme_debug("CONNECTED, send update cfg cmd");
		csr_roam_update_cfg(mac_ctx, session_id, reason);
	}
	sme_debug("LFR config for %s changed from %d to %d",
			lfr_get_config_item_string(reason), old_value, value);
	return QDF_STATUS_SUCCESS;
}

/*CleanUP Routines*/
static void csr_neighbor_roam_reset_channel_info(tpCsrNeighborRoamChannelInfo
						 rChInfo)
{
	if ((rChInfo->IAPPNeighborListReceived == false) &&
	    (rChInfo->currentChannelListInfo.numOfChannels)) {
		rChInfo->currentChanIndex =
			CSR_NEIGHBOR_ROAM_INVALID_CHANNEL_INDEX;
		rChInfo->currentChannelListInfo.numOfChannels = 0;
		if (rChInfo->currentChannelListInfo.freq_list)
			qdf_mem_free(rChInfo->currentChannelListInfo.freq_list);
		rChInfo->currentChannelListInfo.freq_list = NULL;
	} else {
		rChInfo->currentChanIndex = 0;
	}
}

/**
 * csr_neighbor_roam_reset_connected_state_control_info()
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @sessionId : session id
 *
 * This function will reset the neighbor roam control info data structures.
 * This function should be invoked whenever we move to CONNECTED state from
 * any state other than INIT state
 *
 * Return: None
 */
static void csr_neighbor_roam_reset_connected_state_control_info(
							struct mac_context *mac,
							uint8_t sessionId)
{
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[sessionId];

	csr_neighbor_roam_reset_channel_info(&pNeighborRoamInfo->
					roamChannelInfo);
	csr_neighbor_roam_free_roamable_bss_list(mac,
					&pNeighborRoamInfo->roamableAPList);

	/* Do not free up the preauth done list here */
	pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum = 0;
	pNeighborRoamInfo->FTRoamInfo.neighborRptPending = false;
	pNeighborRoamInfo->FTRoamInfo.numPreAuthRetries = 0;
	pNeighborRoamInfo->FTRoamInfo.preauthRspPending = 0;
	pNeighborRoamInfo->uOsRequestedHandoff = 0;
	qdf_mem_zero(&pNeighborRoamInfo->handoffReqInfo,
		     sizeof(tCsrHandoffRequest));
}

static void csr_neighbor_roam_reset_report_scan_state_control_info(
							struct mac_context *mac,
							uint8_t sessionId)
{
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[sessionId];

	qdf_zero_macaddr(&pNeighborRoamInfo->currAPbssid);
#ifdef FEATURE_WLAN_ESE
	pNeighborRoamInfo->isESEAssoc = false;
	pNeighborRoamInfo->isVOAdmitted = false;
	pNeighborRoamInfo->MinQBssLoadRequired = 0;
#endif

	/* Purge roamable AP list */
	csr_neighbor_roam_free_roamable_bss_list(mac,
					&pNeighborRoamInfo->roamableAPList);
}

/**
 * csr_neighbor_roam_reset_init_state_control_info()
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @sessionId : session id
 *
 * This function will reset the neighbor roam control info data structures.
 * This function should be invoked whenever we move to CONNECTED state from
 * INIT state
 *
 * Return: None
 */
static void csr_neighbor_roam_reset_init_state_control_info(struct mac_context *mac,
							    uint8_t sessionId)
{
	csr_neighbor_roam_reset_connected_state_control_info(mac, sessionId);

	/* In addition to the above resets,
	 * we should clear off the curAPBssId/Session ID in the timers
	 */
	csr_neighbor_roam_reset_report_scan_state_control_info(mac, sessionId);
}

#ifdef WLAN_FEATURE_11W
void
csr_update_pmf_cap_from_connected_profile(tCsrRoamConnectedProfile *profile,
					  struct scan_filter *filter)
{
	if (profile->MFPCapable || profile->MFPEnabled)
		filter->pmf_cap = WLAN_PMF_CAPABLE;
	if (profile->MFPRequired)
		filter->pmf_cap = WLAN_PMF_REQUIRED;
}
#else
void
csr_update_pmf_cap_from_connected_profile(tCsrRoamConnectedProfile *profile,
					  struct scan_filter *filter)
{}
#endif

QDF_STATUS
csr_neighbor_roam_get_scan_filter_from_profile(struct mac_context *mac,
					       struct scan_filter *filter,
					       uint8_t vdev_id)
{
	tpCsrNeighborRoamControlInfo nbr_roam_info;
	tCsrRoamConnectedProfile *profile;
	struct roam_ext_params *roam_params;
	tCsrChannelInfo *chan_info;
	uint8_t num_ch = 0;
	QDF_STATUS status;

	if (!filter)
		return QDF_STATUS_E_FAILURE;
	if (!CSR_IS_SESSION_VALID(mac, vdev_id))
		return QDF_STATUS_E_FAILURE;

	qdf_mem_zero(filter, sizeof(*filter));
	nbr_roam_info = &mac->roam.neighborRoamInfo[vdev_id];
	profile = &mac->roam.roamSession[vdev_id].connectedProfile;
	roam_params = &mac->roam.configParam.roam_params;

	/* only for HDD requested handoff fill in the BSSID in the filter */
	if (nbr_roam_info->uOsRequestedHandoff) {
		sme_debug("OS Requested Handoff");
		filter->num_of_bssid = 1;
		qdf_mem_copy(filter->bssid_list[0].bytes,
			     &nbr_roam_info->handoffReqInfo.bssid.bytes,
			     QDF_MAC_ADDR_SIZE);
	}
	sme_debug("No of Allowed SSID List:%d",
		  roam_params->num_ssid_allowed_list);

	if (roam_params->num_ssid_allowed_list) {
		csr_copy_ssids_from_roam_params(roam_params, filter);
	} else {
		filter->num_of_ssid = 1;

		filter->ssid_list[0].length = profile->SSID.length;
		if (filter->ssid_list[0].length > WLAN_SSID_MAX_LEN)
			filter->ssid_list[0].length = WLAN_SSID_MAX_LEN;
		qdf_mem_copy(filter->ssid_list[0].ssid,
			     profile->SSID.ssId,
			     filter->ssid_list[0].length);

		sme_debug("Filtering for SSID %.*s,length of SSID = %u",
			  filter->ssid_list[0].length,
			  filter->ssid_list[0].ssid,
			  filter->ssid_list[0].length);
	}

	status = csr_fill_filter_from_vdev_crypto(mac, filter, vdev_id);

	if (QDF_IS_STATUS_ERROR(status))
		return status;

	chan_info = &nbr_roam_info->roamChannelInfo.currentChannelListInfo;
	num_ch = chan_info->numOfChannels;
	if (num_ch) {
		filter->num_of_channels = num_ch;
		if (filter->num_of_channels > NUM_CHANNELS)
			filter->num_of_channels = NUM_CHANNELS;
		qdf_mem_copy(filter->chan_freq_list, chan_info->freq_list,
			     filter->num_of_channels *
			     sizeof(filter->chan_freq_list[0]));
	}

	if (nbr_roam_info->is11rAssoc)
		/*
		 * MDIE should be added as a part of profile. This should be
		 * added as a part of filter as well
		 */
		filter->mobility_domain = profile->mdid.mobility_domain;

	csr_update_pmf_cap_from_connected_profile(profile, filter);

	filter->enable_adaptive_11r =
		wlan_mlme_adaptive_11r_enabled(mac->psoc);
	csr_update_scan_filter_dot11mode(mac, filter);

	return QDF_STATUS_SUCCESS;
}

enum band_info csr_get_rf_band(uint8_t channel)
{
	if ((channel >= SIR_11A_CHANNEL_BEGIN) &&
	    (channel <= SIR_11A_CHANNEL_END))
		return BAND_5G;

	if ((channel >= SIR_11B_CHANNEL_BEGIN) &&
	    (channel <= SIR_11B_CHANNEL_END))
		return BAND_2G;

	return BAND_UNKNOWN;
}

/**
 * csr_neighbor_roam_channels_filter_by_current_band()
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @session_id: Session ID
 * @input_chan_freq_list: The input channel list
 * @input_num_of_ch: The number of channels in input channel list
 * @out_chan_freq_list: The output channel list
 * @output_num_of_ch: The number of channels in output channel list
 * @merged_output_num_of_ch: The final number of channels in the
 *				output channel list.
 *
 * This function is used to filter out the channels based on the
 * currently associated AP channel
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE otherwise
 */
QDF_STATUS csr_neighbor_roam_channels_filter_by_current_band(struct mac_context *
						mac,
						uint8_t sessionId,
						uint32_t *input_chan_freq_list,
						uint8_t inputNumOfChannels,
						uint32_t *out_chan_freq_list,
						uint8_t *
						pMergedOutputNumOfChannels)
{
	uint8_t i = 0;
	uint8_t numChannels = 0;
	uint32_t curr_ap_op_chan_freq =
		mac->roam.neighborRoamInfo[sessionId].curr_ap_op_chan_freq;
	/* Check for NULL pointer */
	if (!input_chan_freq_list)
		return QDF_STATUS_E_INVAL;

	/* Check for NULL pointer */
	if (!out_chan_freq_list)
		return QDF_STATUS_E_INVAL;

	if (inputNumOfChannels > CFG_VALID_CHANNEL_LIST_LEN) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Wrong Number of Input Channels %d",
			  __func__, inputNumOfChannels);
		return QDF_STATUS_E_INVAL;
	}
	for (i = 0; i < inputNumOfChannels; i++) {
		if (WLAN_REG_IS_SAME_BAND_FREQS(
				curr_ap_op_chan_freq,
				input_chan_freq_list[i])) {
			out_chan_freq_list[numChannels] =
				input_chan_freq_list[i];
			numChannels++;
		}
	}

	/* Return final number of channels */
	*pMergedOutputNumOfChannels = numChannels;

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_neighbor_roam_channels_filter_by_current_band()
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @pinput_chan_freq_list: The additional channels to merge in
 *          to the "merged" channels list.
 * @input_num_of_ch: The number of additional channels.
 * @out_chan_freq_list: The place to put the "merged" channel list.
 * @output_num_of_ch: The original number of channels in the
 *			"merged" channels list.
 * @merged_output_num_of_ch: The final number of channels in the
 *				"merged" channel list.
 *
 * This function is used to merge two channel list.
 * NB: If called with outputNumOfChannels == 0, this routines simply
 * copies the input channel list to the output channel list. if number
 * of merged channels are more than 100, num of channels set to 100
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE otherwise
 */
QDF_STATUS csr_neighbor_roam_merge_channel_lists(struct mac_context *mac,
						 uint32_t *pinput_chan_freq_list,
						 uint8_t inputNumOfChannels,
						 uint32_t *out_chan_freq_list,
						 uint8_t outputNumOfChannels,
						 uint8_t *
						 pMergedOutputNumOfChannels)
{
	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t numChannels = outputNumOfChannels;

	/* Check for NULL pointer */
	if (!pinput_chan_freq_list)
		return QDF_STATUS_E_INVAL;

	/* Check for NULL pointer */
	if (!out_chan_freq_list)
		return QDF_STATUS_E_INVAL;

	if (inputNumOfChannels > CFG_VALID_CHANNEL_LIST_LEN) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Wrong Number of Input Channels %d",
			  __func__, inputNumOfChannels);
		return QDF_STATUS_E_INVAL;
	}
	if (outputNumOfChannels >= CFG_VALID_CHANNEL_LIST_LEN) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "%s: Wrong Number of Output Channels %d",
			  __func__, outputNumOfChannels);
		return QDF_STATUS_E_INVAL;
	}
	/* Add the "new" channels in the input list to the end of the
	 * output list.
	 */
	for (i = 0; i < inputNumOfChannels; i++) {
		for (j = 0; j < outputNumOfChannels; j++) {
			if (pinput_chan_freq_list[i]
				== out_chan_freq_list[j])
				break;
		}
		if (j == outputNumOfChannels) {
			if (pinput_chan_freq_list[i]) {
				QDF_TRACE(QDF_MODULE_ID_SME,
					  QDF_TRACE_LEVEL_DEBUG,
					  "%s: [INFOLOG] Adding extra %d to Neighbor channel list",
					  __func__, pinput_chan_freq_list[i]);
				out_chan_freq_list[numChannels] =
					pinput_chan_freq_list[i];
				numChannels++;
			}
		}
		if (numChannels >= CFG_VALID_CHANNEL_LIST_LEN) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				  "%s: Merge Neighbor channel list reached Max limit %d",
				__func__, numChannels);
			break;
		}
	}

	/* Return final number of channels */
	*pMergedOutputNumOfChannels = numChannels;

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_roam_reset_roam_params - API to reset the roaming parameters
 * @mac_ctx:          Pointer to the global MAC structure
 *
 * The BSSID blacklist should not be cleared since it has to
 * be used across connections. These parameters will be cleared
 * and sent to firmware with with the roaming STOP command.
 *
 * Return: VOID
 */
void csr_roam_reset_roam_params(struct mac_context *mac_ctx)
{
	struct roam_ext_params *roam_params = NULL;

	/*
	 * clear all the whitelist parameters and remaining
	 * needs to be retained across connections.
	 */
	roam_params = &mac_ctx->roam.configParam.roam_params;
	roam_params->num_ssid_allowed_list = 0;
	qdf_mem_zero(&roam_params->ssid_allowed_list,
			sizeof(tSirMacSSid) * MAX_SSID_ALLOWED_LIST);
}

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void csr_roam_restore_default_config(struct mac_context *mac_ctx,
					    uint8_t vdev_id)
{
	struct wlan_roam_triggers triggers;

	sme_set_roam_config_enable(MAC_HANDLE(mac_ctx), vdev_id, 0);

	triggers.vdev_id = vdev_id;
	triggers.trigger_bitmap = wlan_mlme_get_roaming_triggers(mac_ctx->psoc);
	sme_debug("Reset roam trigger bitmap to 0x%x", triggers.trigger_bitmap);
	wlan_cm_rso_set_roam_trigger(mac_ctx->pdev, vdev_id, &triggers);
	sme_roam_control_restore_default_config(MAC_HANDLE(mac_ctx),
						vdev_id);
}
#else
static void csr_roam_restore_default_config(struct mac_context *mac_ctx,
					    uint8_t vdev_id)
{
}
#endif

/**
 * csr_neighbor_roam_indicate_disconnect() - To indicate disconnect
 * @mac: The handle returned by mac_open
 * @sessionId: CSR session id that got disconnected
 *
 * This function is called by CSR as soon as the station disconnects
 * from the AP. This function does the necessary cleanup of neighbor roam
 * data structures. Neighbor roam state transitions to INIT state whenever
 * this function is called except if the current state is REASSOCIATING
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_neighbor_roam_indicate_disconnect(struct mac_context *mac,
						 uint8_t sessionId)
{
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
			&mac->roam.neighborRoamInfo[sessionId];
	tCsrRoamConnectedProfile *pPrevProfile =
			&pNeighborRoamInfo->prevConnProfile;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	struct csr_roam_session *roam_session = NULL;

	if (!pSession) {
		sme_err("pSession is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Disconn ind on session %d in state %d from bss :"
			QDF_MAC_ADDR_FMT), sessionId,
			pNeighborRoamInfo->neighborRoamState,
			QDF_MAC_ADDR_REF(pSession->connectedProfile.bssid.bytes));
	/*
	 * Free the current previous profile and move
	 * the current profile to prev profile.
	 */
	csr_roam_free_connect_profile(pPrevProfile);
	csr_roam_copy_connect_profile(mac, sessionId, pPrevProfile);

	if (pSession) {
		roam_session = &mac->roam.roamSession[sessionId];
		if (pSession->pCurRoamProfile && (QDF_STA_MODE !=
			roam_session->pCurRoamProfile->csrPersona)) {
			sme_err("Ignore disconn ind rcvd from nonSTA persona sessionId: %d csrPersonna %d",
				sessionId,
				(int)roam_session->pCurRoamProfile->csrPersona);
			return QDF_STATUS_SUCCESS;
		}
#ifdef FEATURE_WLAN_ESE
		if (pSession->connectedProfile.isESEAssoc) {
			qdf_mem_copy(&pSession->prevApSSID,
				&pSession->connectedProfile.SSID,
				sizeof(tSirMacSSid));
			qdf_copy_macaddr(&pSession->prevApBssid,
					&pSession->connectedProfile.bssid);
			pSession->isPrevApInfoValid = true;
			pSession->roamTS1 = qdf_mc_timer_get_system_time();
		}
#endif
	}

	switch (pNeighborRoamInfo->neighborRoamState) {
	case eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING:
		/*
		 * Stop scan and neighbor refresh timers.
		 * These are indeed not required when we'r in reassoc state.
		 */
		if (!CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(mac, sessionId)) {
			/*
			 * Disconnect indication during Disassoc Handoff
			 * sub-state is received when we are trying to
			 * disconnect with the old AP during roam.
			 * BUT, if receive a disconnect indication outside of
			 * Disassoc Handoff sub-state, then it means that
			 * this is a genuine disconnect and we need to clean up.
			 * Otherwise, we will be stuck in reassoc state which'll
			 * in-turn block scans.
			 */
		csr_neighbor_roam_state_transition(mac,
				eCSR_NEIGHBOR_ROAM_STATE_INIT, sessionId);
			pNeighborRoamInfo->roamChannelInfo.
				IAPPNeighborListReceived = false;
			pNeighborRoamInfo->uOsRequestedHandoff = 0;
		}
		break;

	case eCSR_NEIGHBOR_ROAM_STATE_INIT:
		csr_neighbor_roam_reset_init_state_control_info(mac,
				sessionId);
		break;

	case eCSR_NEIGHBOR_ROAM_STATE_CONNECTED:
		csr_neighbor_roam_state_transition(mac,
				eCSR_NEIGHBOR_ROAM_STATE_INIT, sessionId);
		pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived =
				false;
		csr_neighbor_roam_reset_connected_state_control_info(mac,
				sessionId);
		break;

	case eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE:
		/* Stop pre-auth to reassoc interval timer */
		qdf_mc_timer_stop(&pSession->ftSmeContext.
				preAuthReassocIntvlTimer);
		/* fallthrough */
	case eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING:
		csr_neighbor_roam_state_transition(mac,
				eCSR_NEIGHBOR_ROAM_STATE_INIT, sessionId);
		pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived =
				false;
		csr_neighbor_roam_reset_preauth_control_info(mac, sessionId);
		csr_neighbor_roam_reset_report_scan_state_control_info(mac,
				sessionId);
		break;
	default:
		sme_debug("Received disconnect event in state %s",
			mac_trace_get_neighbour_roam_state(
				pNeighborRoamInfo->neighborRoamState));
		sme_debug("Transit to INIT state");
		csr_neighbor_roam_state_transition(mac,
				eCSR_NEIGHBOR_ROAM_STATE_INIT, sessionId);
			pNeighborRoamInfo->roamChannelInfo.
			IAPPNeighborListReceived = false;
			pNeighborRoamInfo->uOsRequestedHandoff = 0;
		break;
	}

	/*
	 * clear the roaming parameters that are per connection.
	 * For a new connection, they have to be programmed again.
	 */
	if (!csr_neighbor_middle_of_roaming(mac, sessionId)) {
		csr_roam_reset_roam_params(mac);
		csr_roam_restore_default_config(mac, sessionId);
	}

	/*Inform the Firmware to STOP Scanning as the host has a disconnect. */
	if (csr_roam_is_sta_mode(mac, sessionId))
		csr_post_roam_state_change(mac, sessionId, WLAN_ROAM_DEINIT,
					   REASON_DISCONNECTED);

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_neighbor_roam_info_ctx_init() - Initialize neighbor roam struct
 * @mac: mac context
 * @session_id: Session Id
 *
 * This initializes all the necessary data structures related to the
 * associated AP.
 *
 * Return: QDF status
 */
static void csr_neighbor_roam_info_ctx_init(struct mac_context *mac,
					    uint8_t session_id)
{
	tpCsrNeighborRoamControlInfo ngbr_roam_info =
		&mac->roam.neighborRoamInfo[session_id];
	struct cm_roam_values_copy src_cfg;
	struct csr_roam_session *session = &mac->roam.roamSession[session_id];
	int init_ft_flag = false;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct vdev_mlme_obj *vdev_mlme;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, session_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		QDF_BUG(0);
		return;
	}

	/* update the EXT cap IE with union of driver populated
	 * values and the values sent from userspace
	 */
	csr_send_set_ie(vdev_mlme->mgmt.generic.type,
			vdev_mlme->mgmt.generic.subtype, session_id);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	csr_init_occupied_channels_list(mac, session_id);
	csr_neighbor_roam_state_transition(mac,
			eCSR_NEIGHBOR_ROAM_STATE_CONNECTED, session_id);

	qdf_copy_macaddr(&ngbr_roam_info->currAPbssid,
			&session->connectedProfile.bssid);
	ngbr_roam_info->curr_ap_op_chan_freq =
				      session->connectedProfile.op_freq;
	ngbr_roam_info->currentNeighborLookupThreshold =
		ngbr_roam_info->cfgParams.neighborLookupThreshold;
	ngbr_roam_info->currentOpportunisticThresholdDiff =
		ngbr_roam_info->cfgParams.nOpportunisticThresholdDiff;
	ngbr_roam_info->currentRoamRescanRssiDiff =
		ngbr_roam_info->cfgParams.nRoamRescanRssiDiff;
	ngbr_roam_info->currentRoamBmissFirstBcnt =
		ngbr_roam_info->cfgParams.nRoamBmissFirstBcnt;
	ngbr_roam_info->currentRoamBmissFinalBcnt =
		ngbr_roam_info->cfgParams.nRoamBmissFinalBcnt;

	/*
	 * Update RSSI change params to vdev
	 */
	src_cfg.uint_value = mac->mlme_cfg->lfr.roam_rescan_rssi_diff;
	wlan_cm_roam_cfg_set_value(mac->psoc, session_id,
				   RSSI_CHANGE_THRESHOLD, &src_cfg);

	src_cfg.uint_value = mac->mlme_cfg->lfr.roam_scan_hi_rssi_delay;
	wlan_cm_roam_cfg_set_value(mac->psoc, session_id,
				   HI_RSSI_DELAY_BTW_SCANS, &src_cfg);

	wlan_cm_update_roam_scan_scheme_bitmap(mac->psoc, session_id,
					       DEFAULT_ROAM_SCAN_SCHEME_BITMAP);

	/*
	 * Now we can clear the preauthDone that
	 * was saved as we are connected afresh
	 */
	csr_neighbor_roam_free_roamable_bss_list(mac,
		&ngbr_roam_info->FTRoamInfo.preAuthDoneList);

	/* Based on the auth scheme tell if we are 11r */
	if (csr_is_auth_type11r
		(mac, session->connectedProfile.AuthType,
		session->connectedProfile.mdid.mdie_present)) {
		if (mac->mlme_cfg->lfr.fast_transition_enabled)
			init_ft_flag = true;
		ngbr_roam_info->is11rAssoc = true;
	} else
		ngbr_roam_info->is11rAssoc = false;

#ifdef FEATURE_WLAN_ESE
	/* Based on the auth scheme tell if we are 11r */
	if (session->connectedProfile.isESEAssoc) {
		if (mac->mlme_cfg->lfr.fast_transition_enabled)
			init_ft_flag = true;
		ngbr_roam_info->isESEAssoc = true;
	} else
		ngbr_roam_info->isESEAssoc = false;
#endif
	/* If "FastRoamEnabled" ini is enabled */
	if (csr_roam_is_fast_roam_enabled(mac, session_id))
		init_ft_flag = true;

	if (init_ft_flag) {
		/* Initialize all the data needed for the 11r FT Preauth */
		ngbr_roam_info->FTRoamInfo.currentNeighborRptRetryNum = 0;
		csr_neighbor_roam_purge_preauth_failed_list(mac);
	}

	if (!csr_roam_is_roam_offload_scan_enabled(mac))
		return;
	/*
	 * Store the current PMK info of the AP
	 * to the single pmk global cache if the BSS allows
	 * single pmk roaming capable.
	 */
	csr_store_sae_single_pmk_to_global_cache(mac, session,
						 session_id);

	/*
	 * If this is not a INFRA type BSS, then do not send the command
	 * down to firmware.Do not send the START command for
	 * other session connections.
	 */
	if (!csr_roam_is_sta_mode(mac, session_id)) {
		sme_debug("Wrong Mode %d", session->connectedProfile.BSSType);
		return;
	}

	ngbr_roam_info->uOsRequestedHandoff = 0;
	if (!MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac->psoc, session_id))
		csr_post_roam_state_change(mac, session_id,
					   WLAN_ROAM_RSO_ENABLED,
					   REASON_CTX_INIT);
}

/**
* csr_neighbor_roam_indicate_connect()
* @mac: mac context
 * @session_id: Session Id
 * @qdf_status: QDF status
 *
 * This function is called by CSR as soon as the station connects to an AP.
 * This initializes all the necessary data structures related to the
 * associated AP and transitions the state to CONNECTED state
 *
 * Return: QDF status
 */
QDF_STATUS csr_neighbor_roam_indicate_connect(
		struct mac_context *mac, uint8_t session_id,
		QDF_STATUS qdf_status)
{
	tpCsrNeighborRoamControlInfo ngbr_roam_info =
		&mac->roam.neighborRoamInfo[session_id];
	struct csr_roam_session *session = &mac->roam.roamSession[session_id];
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	struct csr_roam_info *roam_info;
#endif
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* if session id invalid then we need return failure */
	if (!ngbr_roam_info || !CSR_IS_SESSION_VALID(mac, session_id)
	|| (!mac->roam.roamSession[session_id].pCurRoamProfile)) {
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("Connect ind, vdev id %d in state %s",
		session_id, mac_trace_get_neighbour_roam_state(
			ngbr_roam_info->neighborRoamState));

	/* Bail out if this is NOT a STA persona */
	if (mac->roam.roamSession[session_id].pCurRoamProfile->csrPersona !=
	QDF_STA_MODE) {
		sme_debug("Ignoring Connect ind received from a non STA. session_id: %d, csrPersonna %d",
			  session_id,
			  (int)session->pCurRoamProfile->csrPersona);
		return QDF_STATUS_SUCCESS;
	}
	/* if a concurrent session is running */
	if ((false ==
		CSR_IS_FASTROAM_IN_CONCURRENCY_INI_FEATURE_ENABLED(mac)) &&
		(csr_is_concurrent_session_running(mac))) {
		sme_err("Ignoring Connect ind. received in multisession %d",
			csr_is_concurrent_session_running(mac));
		return QDF_STATUS_SUCCESS;
	}
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac->psoc, session_id) &&
	    eSIR_ROAM_AUTH_STATUS_AUTHENTICATED ==
	     session->roam_synch_data->authStatus) {
		sme_debug("LFR3: Authenticated");
		roam_info = qdf_mem_malloc(sizeof(*roam_info));
		if (!roam_info)
			return QDF_STATUS_E_NOMEM;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &session->connectedProfile.bssid);
		csr_roam_call_callback(mac, session_id, roam_info, 0,
				       eCSR_ROAM_SET_KEY_COMPLETE,
				       eCSR_ROAM_RESULT_AUTHENTICATED);
		csr_neighbor_roam_reset_init_state_control_info(mac,
								session_id);
		csr_neighbor_roam_info_ctx_init(mac, session_id);
		qdf_mem_free(roam_info);
		return status;
	}
#endif

	switch (ngbr_roam_info->neighborRoamState) {
	case eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING:
		if (QDF_STATUS_SUCCESS != qdf_status) {
			/**
			 * Just transition the state to INIT state.Rest of the
			 * clean up happens when we get next connect indication
			 */
			csr_neighbor_roam_state_transition(mac,
				eCSR_NEIGHBOR_ROAM_STATE_INIT, session_id);
			ngbr_roam_info->roamChannelInfo.
					IAPPNeighborListReceived = false;
			ngbr_roam_info->uOsRequestedHandoff = 0;
			break;
		}
		/* if the status is SUCCESS */
		/* fallthrough */
	case eCSR_NEIGHBOR_ROAM_STATE_INIT:
		/* Reset all the data structures here */
		csr_neighbor_roam_reset_init_state_control_info(mac,
			session_id);
		csr_neighbor_roam_info_ctx_init(mac, session_id);
		break;
	default:
		sme_err("Connect evt received in invalid state %s Ignoring",
			mac_trace_get_neighbour_roam_state(
			ngbr_roam_info->neighborRoamState));
		break;
	}
	return status;
}

/*
 * csr_neighbor_roam_init11r_assoc_info -
 * This function initializes 11r related neighbor roam data structures
 *
 * @mac: The handle returned by mac_open.
 *
 * return QDF_STATUS_SUCCESS on success, corresponding error code otherwise
 */
static QDF_STATUS csr_neighbor_roam_init11r_assoc_info(struct mac_context *mac)
{
	QDF_STATUS status;
	uint8_t i;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;
	tpCsr11rAssocNeighborInfo pFTRoamInfo = NULL;

	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		pNeighborRoamInfo = &mac->roam.neighborRoamInfo[i];
		pFTRoamInfo = &pNeighborRoamInfo->FTRoamInfo;

		pNeighborRoamInfo->is11rAssoc = false;

		pFTRoamInfo->neighborReportTimeout =
			CSR_NEIGHBOR_ROAM_REPORT_QUERY_TIMEOUT;
		pFTRoamInfo->neighborRptPending = false;
		pFTRoamInfo->preauthRspPending = false;

		pFTRoamInfo->currentNeighborRptRetryNum = 0;

		status = csr_ll_open(&pFTRoamInfo->preAuthDoneList);
		if (QDF_STATUS_SUCCESS != status) {
			sme_err("LL Open of preauth done AP List failed");
			return QDF_STATUS_E_RESOURCES;
		}
	}
	return status;
}

/*
 * csr_neighbor_roam_init() -
 * This function initializes neighbor roam data structures
 *
 * @mac: The handle returned by mac_open.
 * sessionId: Session identifier
 *
 * Return QDF_STATUS_SUCCESS on success, corresponding error code otherwise
 */
QDF_STATUS csr_neighbor_roam_init(struct mac_context *mac, uint8_t sessionId)
{
	QDF_STATUS status;
	tCsrChannelInfo *specific_chan_info;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[sessionId];

	pNeighborRoamInfo->neighborRoamState = eCSR_NEIGHBOR_ROAM_STATE_CLOSED;
	pNeighborRoamInfo->prevNeighborRoamState =
		eCSR_NEIGHBOR_ROAM_STATE_CLOSED;
	pNeighborRoamInfo->cfgParams.maxChannelScanTime =
		mac->mlme_cfg->lfr.neighbor_scan_max_chan_time;
	pNeighborRoamInfo->cfgParams.minChannelScanTime =
		mac->mlme_cfg->lfr.neighbor_scan_min_chan_time;
	pNeighborRoamInfo->cfgParams.neighborLookupThreshold =
		mac->mlme_cfg->lfr.neighbor_lookup_rssi_threshold;
	pNeighborRoamInfo->cfgParams.rssi_thresh_offset_5g =
		mac->mlme_cfg->lfr.rssi_threshold_offset_5g;
	pNeighborRoamInfo->cfgParams.delay_before_vdev_stop =
		mac->mlme_cfg->lfr.delay_before_vdev_stop;
	pNeighborRoamInfo->cfgParams.nOpportunisticThresholdDiff =
		mac->mlme_cfg->lfr.opportunistic_scan_threshold_diff;
	pNeighborRoamInfo->cfgParams.nRoamRescanRssiDiff =
		mac->mlme_cfg->lfr.roam_rescan_rssi_diff;

	pNeighborRoamInfo->cfgParams.nRoamBmissFirstBcnt =
		mac->mlme_cfg->lfr.roam_bmiss_first_bcnt;
	pNeighborRoamInfo->cfgParams.nRoamBmissFinalBcnt =
		mac->mlme_cfg->lfr.roam_bmiss_final_bcnt;

	pNeighborRoamInfo->cfgParams.neighborScanPeriod =
		mac->mlme_cfg->lfr.neighbor_scan_timer_period;
	pNeighborRoamInfo->cfgParams.neighbor_scan_min_period =
		mac->mlme_cfg->lfr.neighbor_scan_min_timer_period;
	pNeighborRoamInfo->cfgParams.neighborResultsRefreshPeriod =
		mac->mlme_cfg->lfr.neighbor_scan_results_refresh_period;
	pNeighborRoamInfo->cfgParams.emptyScanRefreshPeriod =
		mac->mlme_cfg->lfr.empty_scan_refresh_period;
	pNeighborRoamInfo->cfgParams.full_roam_scan_period =
		mac->mlme_cfg->lfr.roam_full_scan_period;
	pNeighborRoamInfo->cfgParams.enable_scoring_for_roam =
		mac->mlme_cfg->roam_scoring.enable_scoring_for_roam;
	pNeighborRoamInfo->cfgParams.roam_scan_n_probes =
		mac->mlme_cfg->lfr.roam_scan_n_probes;
	pNeighborRoamInfo->cfgParams.roam_scan_home_away_time =
		mac->mlme_cfg->lfr.roam_scan_home_away_time;
	pNeighborRoamInfo->cfgParams.roam_scan_inactivity_time =
		mac->mlme_cfg->lfr.roam_scan_inactivity_time;
	pNeighborRoamInfo->cfgParams.roam_inactive_data_packet_count =
		mac->mlme_cfg->lfr.roam_inactive_data_packet_count;
	pNeighborRoamInfo->cfgParams.roam_scan_period_after_inactivity =
		mac->mlme_cfg->lfr.roam_scan_period_after_inactivity;

	specific_chan_info = &pNeighborRoamInfo->cfgParams.specific_chan_info;
	specific_chan_info->numOfChannels =
		mac->mlme_cfg->lfr.neighbor_scan_channel_list_num;
	sme_debug("number of channels: %u", specific_chan_info->numOfChannels);
	if (specific_chan_info->numOfChannels != 0) {
		specific_chan_info->freq_list =
			qdf_mem_malloc(sizeof(uint32_t) *
				       specific_chan_info->numOfChannels);
		if (!specific_chan_info->freq_list) {
			specific_chan_info->numOfChannels = 0;
			return QDF_STATUS_E_NOMEM;
		}

	} else {
		specific_chan_info->freq_list = NULL;
	}

	/* Update the roam global structure from CFG */
	sme_chan_to_freq_list(mac->pdev,
			      specific_chan_info->freq_list,
			      mac->mlme_cfg->lfr.neighbor_scan_channel_list,
			      mac->mlme_cfg->lfr.
			      neighbor_scan_channel_list_num);

	pNeighborRoamInfo->cfgParams.hi_rssi_scan_max_count =
		mac->mlme_cfg->lfr.roam_scan_hi_rssi_maxcount;
	pNeighborRoamInfo->cfgParams.hi_rssi_scan_rssi_delta =
		mac->mlme_cfg->lfr.roam_scan_hi_rssi_delta;

	pNeighborRoamInfo->cfgParams.hi_rssi_scan_delay =
		mac->mlme_cfg->lfr.roam_scan_hi_rssi_delay;

	pNeighborRoamInfo->cfgParams.hi_rssi_scan_rssi_ub =
		mac->mlme_cfg->lfr.roam_scan_hi_rssi_ub;
	pNeighborRoamInfo->cfgParams.roam_rssi_diff =
		mac->mlme_cfg->lfr.roam_rssi_diff;
	pNeighborRoamInfo->cfgParams.bg_rssi_threshold =
		mac->mlme_cfg->lfr.bg_rssi_threshold;

	qdf_zero_macaddr(&pNeighborRoamInfo->currAPbssid);
	pNeighborRoamInfo->currentNeighborLookupThreshold =
		pNeighborRoamInfo->cfgParams.neighborLookupThreshold;
	pNeighborRoamInfo->currentOpportunisticThresholdDiff =
		pNeighborRoamInfo->cfgParams.nOpportunisticThresholdDiff;
	pNeighborRoamInfo->currentRoamRescanRssiDiff =
		pNeighborRoamInfo->cfgParams.nRoamRescanRssiDiff;
	pNeighborRoamInfo->currentRoamBmissFirstBcnt =
		pNeighborRoamInfo->cfgParams.nRoamBmissFirstBcnt;
	pNeighborRoamInfo->currentRoamBmissFinalBcnt =
		pNeighborRoamInfo->cfgParams.nRoamBmissFinalBcnt;
	qdf_mem_zero(&pNeighborRoamInfo->prevConnProfile,
		    sizeof(tCsrRoamConnectedProfile));

	status = csr_ll_open(&pNeighborRoamInfo->roamableAPList);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("LL Open of roam able AP List failed");
		qdf_mem_free(specific_chan_info->freq_list);
		specific_chan_info->freq_list = NULL;
		specific_chan_info->numOfChannels = 0;
		return QDF_STATUS_E_RESOURCES;
	}

	pNeighborRoamInfo->roamChannelInfo.currentChanIndex =
		CSR_NEIGHBOR_ROAM_INVALID_CHANNEL_INDEX;
	pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.
	numOfChannels = 0;
	pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.freq_list =
		NULL;
	pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived = false;

	status = csr_neighbor_roam_init11r_assoc_info(mac);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("LL Open of roam able AP List failed");
		specific_chan_info->freq_list = NULL;
		specific_chan_info->numOfChannels = 0;
		csr_ll_close(&pNeighborRoamInfo->roamableAPList);
		return QDF_STATUS_E_RESOURCES;
	}

	csr_neighbor_roam_state_transition(mac,
			eCSR_NEIGHBOR_ROAM_STATE_INIT, sessionId);
	pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived = false;
	pNeighborRoamInfo->uOsRequestedHandoff = 0;
	/* Set the Last Sent Cmd as RSO_STOP */
	pNeighborRoamInfo->last_sent_cmd = ROAM_SCAN_OFFLOAD_STOP;
	return QDF_STATUS_SUCCESS;
}

/*
 * csr_neighbor_roam_close() -
 * This function closes/frees all the neighbor roam data structures
 *
 * @mac: The handle returned by mac_open.
 * @sessionId: Session identifier
 *
 * Return VOID
 */
void csr_neighbor_roam_close(struct mac_context *mac, uint8_t sessionId)
{
	tCsrChannelInfo *current_channel_list_info;
	tCsrNeighborRoamCfgParams *cfg_params;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[sessionId];

	if (eCSR_NEIGHBOR_ROAM_STATE_CLOSED ==
	    pNeighborRoamInfo->neighborRoamState) {
		sme_warn("Neighbor Roam Algorithm Already Closed");
		return;
	}
	cfg_params = &pNeighborRoamInfo->cfgParams;
	if (cfg_params->specific_chan_info.freq_list)
		qdf_mem_free(cfg_params->specific_chan_info.freq_list);
	pNeighborRoamInfo->cfgParams.specific_chan_info.freq_list = NULL;
	pNeighborRoamInfo->cfgParams.specific_chan_info.numOfChannels = 0;

	/* Should free up the nodes in the list before closing the
	 * double Linked list
	 */
	csr_neighbor_roam_free_roamable_bss_list(mac,
					&pNeighborRoamInfo->roamableAPList);
	csr_ll_close(&pNeighborRoamInfo->roamableAPList);

	current_channel_list_info =
		&pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo;
	if (current_channel_list_info->freq_list)
		qdf_mem_free(current_channel_list_info->freq_list);

	current_channel_list_info->freq_list = NULL;
	pNeighborRoamInfo->roamChannelInfo.currentChanIndex =
		CSR_NEIGHBOR_ROAM_INVALID_CHANNEL_INDEX;
	current_channel_list_info->numOfChannels = 0;
	pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived = false;

	/* Free the profile.. */
	csr_release_profile(mac, &pNeighborRoamInfo->csrNeighborRoamProfile);
	csr_roam_free_connect_profile(&pNeighborRoamInfo->prevConnProfile);
	pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum = 0;
	csr_neighbor_roam_free_roamable_bss_list(mac,
						 &pNeighborRoamInfo->FTRoamInfo.
						 preAuthDoneList);
	csr_ll_close(&pNeighborRoamInfo->FTRoamInfo.preAuthDoneList);

	csr_neighbor_roam_state_transition(mac,
		eCSR_NEIGHBOR_ROAM_STATE_CLOSED, sessionId);

}

/**
 * csr_neighbor_roam_is11r_assoc() - Check if association type is 11R
 * @mac_ctx: MAC Global context
 * @session_id: Session ID
 *
 * Return: true if 11r Association, false otherwise.
 */
bool csr_neighbor_roam_is11r_assoc(struct mac_context *mac_ctx, uint8_t session_id)
{
	return mac_ctx->roam.neighborRoamInfo[session_id].is11rAssoc;
}

/*
 * csr_neighbor_middle_of_roaming() -
 * This function returns true if STA is in the middle of roaming states
 *
 * @halHandle: The handle from HDD context.
 * @sessionId: Session identifier
 *
 * Return bool
 */
bool csr_neighbor_middle_of_roaming(struct mac_context *mac, uint8_t sessionId)
{
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[sessionId];
	bool val = (eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING ==
		    pNeighborRoamInfo->neighborRoamState) ||
		   (eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING ==
		    pNeighborRoamInfo->neighborRoamState) ||
		   (eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE ==
		    pNeighborRoamInfo->neighborRoamState);
	return val;
}

bool
wlan_cm_neighbor_roam_in_progress(struct wlan_objmgr_psoc *psoc,
				  uint8_t vdev_id)
{
	struct csr_roam_session *session;
	struct mac_context *mac_ctx;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return false;
	}

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("session is null %d", vdev_id);
		return false;
	}

	return csr_neighbor_middle_of_roaming(mac_ctx, vdev_id);
}

/**
 * csr_neighbor_roam_process_handoff_req - Processes handoff request
 *
 * @mac_ctx  Pointer to mac context
 * @session_id  SME session id
 *
 * This function is called start with the handoff process. First do a
 * SSID scan for the BSSID provided.
 *
 * Return: status
 */
static QDF_STATUS csr_neighbor_roam_process_handoff_req(
			struct mac_context *mac_ctx,
			uint8_t session_id)
{
	tpCsrNeighborRoamControlInfo roam_ctrl_info =
		&mac_ctx->roam.neighborRoamInfo[session_id];
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t roam_id;
	struct csr_roam_profile *profile = NULL;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	uint8_t i = 0;
	uint8_t roamable_ap_count = 0;
	struct scan_filter *scan_filter;
	tScanResultHandle       scan_result;

	if (!session) {
		sme_err("session is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	roam_id = GET_NEXT_ROAM_ID(&mac_ctx->roam);
	profile = qdf_mem_malloc(sizeof(struct csr_roam_profile));
	if (!profile)
		return QDF_STATUS_E_NOMEM;

	status =
		csr_roam_copy_profile(mac_ctx, profile,
				      session->pCurRoamProfile, session_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Profile copy failed");
		goto end;
	}
	/* Add the BSSID & Channel */
	profile->BSSIDs.numOfBSSIDs = 1;
	if (!profile->BSSIDs.bssid) {
		profile->BSSIDs.bssid =
			qdf_mem_malloc(sizeof(tSirMacAddr) *
				profile->BSSIDs.numOfBSSIDs);
		if (!profile->BSSIDs.bssid) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
	}

	/* Populate the BSSID from handoff info received from HDD */
	for (i = 0; i < profile->BSSIDs.numOfBSSIDs; i++) {
		qdf_copy_macaddr(&profile->BSSIDs.bssid[i],
				&roam_ctrl_info->handoffReqInfo.bssid);
	}

	profile->ChannelInfo.numOfChannels = 1;
	if (!profile->ChannelInfo.freq_list) {
		profile->ChannelInfo.freq_list =
			qdf_mem_malloc(sizeof(*profile->ChannelInfo.freq_list) *
				       profile->ChannelInfo.numOfChannels);
		if (!profile->ChannelInfo.freq_list) {
			profile->ChannelInfo.numOfChannels = 0;
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
	}

	profile->ChannelInfo.freq_list[0] =
				      roam_ctrl_info->handoffReqInfo.ch_freq;

	/*
	 * For User space connect requests, the scan has already been done.
	 * So, check if the BSS descriptor exists in the scan cache and
	 * proceed with the handoff instead of a redundant scan again.
	 */
	if (roam_ctrl_info->handoffReqInfo.src == CONNECT_CMD_USERSPACE) {
		sme_debug("Connect cmd with bssid within same ESS");
		scan_filter = qdf_mem_malloc(sizeof(*scan_filter));
		if (!scan_filter) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		status = csr_neighbor_roam_get_scan_filter_from_profile(mac_ctx,
								scan_filter,
								session_id);
		sme_debug("Filter creation status: %d", status);
		status = csr_scan_get_result(mac_ctx, scan_filter,
					     &scan_result, true);
		qdf_mem_free(scan_filter);
		csr_neighbor_roam_process_scan_results(mac_ctx, session_id,
							&scan_result);
		roamable_ap_count = csr_ll_count(
					&roam_ctrl_info->roamableAPList);
		sme_debug("roamable_ap_count=%d", roamable_ap_count);
	}
	if (roamable_ap_count) {
		csr_neighbor_roam_trigger_handoff(mac_ctx, session_id);
	} else {
		status = csr_scan_for_ssid(mac_ctx, session_id, profile,
					   roam_id, false);
		if (status != QDF_STATUS_SUCCESS)
			sme_err("SSID scan failed");
	}

end:
	if (profile) {
		csr_release_profile(mac_ctx, profile);
		qdf_mem_free(profile);
	}

	return status;
}

/*
 * csr_neighbor_roam_sssid_scan_done() -
 * This function is called once SSID scan is done. If SSID scan failed
 * to find our candidate add an entry to csr scan cache ourself before starting
 * the handoff process

 * @mac: The handle returned by mac_open.
 * @session_id  SME session id
 *
 * Return QDF_STATUS_SUCCESS on success, corresponding error code otherwise
 */
QDF_STATUS csr_neighbor_roam_sssid_scan_done(struct mac_context *mac,
					   uint8_t sessionId, QDF_STATUS status)
{
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[sessionId];
	QDF_STATUS hstatus;

	/* we must be in connected state, if not ignore it */
	if (eCSR_NEIGHBOR_ROAM_STATE_CONNECTED !=
	    pNeighborRoamInfo->neighborRoamState) {
		sme_err("Received in not CONNECTED state. Ignore it");
		return QDF_STATUS_E_FAILURE;
	}
	/* if SSID scan failed to find our candidate add an entry to
	 * csr scan cache ourself
	 */
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Add an entry to csr scan cache");
		hstatus = csr_scan_create_entry_in_scan_cache(mac, sessionId,
				     pNeighborRoamInfo->handoffReqInfo.bssid,
				     pNeighborRoamInfo->handoffReqInfo.ch_freq);
		if (QDF_STATUS_SUCCESS != hstatus) {
			sme_err(
				"csr_scan_create_entry_in_scan_cache failed with status %d",
				hstatus);
			return QDF_STATUS_E_FAILURE;
		}
	}

	/* Now we have completed scanning for the candidate provided by HDD.
	 * Let move on to HO
	 */
	hstatus = csr_neighbor_roam_process_scan_complete(mac, sessionId);

	if (QDF_STATUS_SUCCESS != hstatus) {
		sme_err("Neighbor scan process complete failed with status %d",
			hstatus);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}


/**
 * csr_neighbor_roam_handoff_req_hdlr - Processes handoff request
 * @mac_ctx  Pointer to mac context
 * @msg  message sent to HDD
 *
 * This function is called by CSR as soon as it gets a handoff request
 * to SME via MC thread
 *
 * Return: status
 */
QDF_STATUS csr_neighbor_roam_handoff_req_hdlr(
			struct mac_context *mac_ctx, void *msg)
{
	tAniHandoffReq *handoff_req = (tAniHandoffReq *) msg;
	uint32_t session_id = handoff_req->sessionId;
	tpCsrNeighborRoamControlInfo roam_ctrl_info =
		&mac_ctx->roam.neighborRoamInfo[session_id];
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* we must be in connected state, if not ignore it */
	if (eCSR_NEIGHBOR_ROAM_STATE_CONNECTED !=
		roam_ctrl_info->neighborRoamState) {
		sme_err("Received in not CONNECTED state. Ignore it");
		return QDF_STATUS_E_FAILURE;
	}

	/* save the handoff info came from HDD as part of the reassoc req */
	handoff_req = (tAniHandoffReq *) msg;
	if (!handoff_req) {
		sme_err("Received msg is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/* sanity check */
	if (!qdf_mem_cmp(handoff_req->bssid,
		roam_ctrl_info->currAPbssid.bytes,
		sizeof(tSirMacAddr))) {
		sme_err("Received req has same BSSID as current AP!!");
		return QDF_STATUS_E_FAILURE;
	}
	roam_ctrl_info->handoffReqInfo.ch_freq = handoff_req->ch_freq;
	roam_ctrl_info->handoffReqInfo.src =
		handoff_req->handoff_src;
	qdf_mem_copy(&roam_ctrl_info->handoffReqInfo.bssid.bytes,
			&handoff_req->bssid, QDF_MAC_ADDR_SIZE);
	roam_ctrl_info->uOsRequestedHandoff = 1;

	status = csr_post_roam_state_change(mac_ctx, session_id,
					    WLAN_ROAM_RSO_STOPPED,
					    REASON_OS_REQUESTED_ROAMING_NOW);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("ROAM: RSO stop failed");
		roam_ctrl_info->uOsRequestedHandoff = 0;
	}
	return status;
}

/**
 * csr_neighbor_roam_proceed_with_handoff_req()
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @session_id: Session ID
 *
 * This function is called by CSR as soon as it gets rsp back for
 * ROAM_SCAN_OFFLOAD_STOP with reason REASON_OS_REQUESTED_ROAMING_NOW
 *
 * Return: QDF_STATUS_SUCCESS on success, corresponding error code otherwise
 */
QDF_STATUS csr_neighbor_roam_proceed_with_handoff_req(struct mac_context *mac,
						      uint8_t sessionId)
{
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[sessionId];
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	/* we must be in connected state, if not ignore it */
	if ((eCSR_NEIGHBOR_ROAM_STATE_CONNECTED !=
	     pNeighborRoamInfo->neighborRoamState)
	    || (!pNeighborRoamInfo->uOsRequestedHandoff)) {
		sme_err(
			"Received in not CONNECTED state(%d) or uOsRequestedHandoff(%d) is not set. Ignore it",
			pNeighborRoamInfo->neighborRoamState,
			pNeighborRoamInfo->uOsRequestedHandoff);
		status = QDF_STATUS_E_FAILURE;
	} else
		/* Let's go ahead with handoff */
		status = csr_neighbor_roam_process_handoff_req(mac, sessionId);

	if (!QDF_IS_STATUS_SUCCESS(status))
		pNeighborRoamInfo->uOsRequestedHandoff = 0;

	return status;
}

/*
 * csr_neighbor_roam_start_lfr_scan() -
 * This function is called if HDD requested handoff failed for some
 * reason. start the LFR logic at that point.By the time, this function is
 * called, a STOP command has already been issued.

 * @mac: The handle returned by mac_open.
 * @session_id: Session ID
 *
 * Return QDF_STATUS_SUCCESS on success, corresponding error code otherwise
 */
QDF_STATUS csr_neighbor_roam_start_lfr_scan(struct mac_context *mac,
						uint8_t sessionId)
{
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[sessionId];
	pNeighborRoamInfo->uOsRequestedHandoff = 0;
	/* There is no candidate or We are not roaming Now.
	 * Inform the FW to restart Roam Offload Scan
	 */
	csr_post_roam_state_change(mac, sessionId, WLAN_ROAM_RSO_ENABLED,
				   REASON_NO_CAND_FOUND_OR_NOT_ROAMING_NOW);

	return QDF_STATUS_SUCCESS;



}
