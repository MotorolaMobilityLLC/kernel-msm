/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

/*===========================================================================

			s a p A p i L i n k C n t l . C

   OVERVIEW:

   This software unit holds the implementation of the WLAN SAP modules
   Link Control functions.

   The functions externalized by this module are to be called ONLY by other
   WLAN modules (HDD)

   DEPENDENCIES:

   Are listed for each API below.
   ===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "qdf_trace.h"
/* Pick up the CSR callback definition */
#include "csr_api.h"
#include "ani_global.h"
#include "csr_inside_api.h"
#include "sme_api.h"
/* SAP Internal API header file */
#include "sap_internal.h"
#include "wlan_policy_mgr_api.h"
#include "wma.h"
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include "wlan_reg_services_api.h"
#include <wlan_scan_ucfg_api.h>
#include <wlan_scan_utils_api.h>

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define SAP_DEBUG

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static Function Declarations and Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Function Declarations and Documentation
 * -------------------------------------------------------------------------*/

/**
 * sap_config_acs_result : Generate ACS result params based on ch constraints
 * @sap_ctx: pointer to SAP context data struct
 * @mac_handle: Opaque handle to the global MAC context
 * @sec_ch: Secondary channel
 *
 * This function calculates the ACS result params: ht sec channel, vht channel
 * information and channel bonding based on selected ACS channel.
 *
 * Return: None
 */

void sap_config_acs_result(mac_handle_t mac_handle,
			   struct sap_context *sap_ctx,
			   uint32_t sec_ch_freq)
{
	struct ch_params ch_params = {0};
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	ch_params.ch_width = sap_ctx->acs_cfg->ch_width;
	wlan_reg_set_channel_params_for_freq(
			mac_ctx->pdev, sap_ctx->acs_cfg->pri_ch_freq,
			sec_ch_freq, &ch_params);
	sap_ctx->acs_cfg->ch_width = ch_params.ch_width;
	if (sap_ctx->acs_cfg->ch_width > CH_WIDTH_40MHZ ||
	    WLAN_REG_IS_6GHZ_CHAN_FREQ(sap_ctx->acs_cfg->pri_ch_freq))
		sap_ctx->acs_cfg->vht_seg0_center_ch_freq =
						ch_params.mhz_freq_seg0;
	else
		sap_ctx->acs_cfg->vht_seg0_center_ch_freq = 0;

	if (sap_ctx->acs_cfg->ch_width == CH_WIDTH_80P80MHZ ||
	   (sap_ctx->acs_cfg->ch_width == CH_WIDTH_160MHZ))
		sap_ctx->acs_cfg->vht_seg1_center_ch_freq =
						ch_params.mhz_freq_seg1;
	else
		sap_ctx->acs_cfg->vht_seg1_center_ch_freq = 0;

	if (ch_params.sec_ch_offset == PHY_DOUBLE_CHANNEL_HIGH_PRIMARY)
		sap_ctx->acs_cfg->ht_sec_ch_freq =
				sap_ctx->acs_cfg->pri_ch_freq - 20;
	else if (ch_params.sec_ch_offset == PHY_DOUBLE_CHANNEL_LOW_PRIMARY)
		sap_ctx->acs_cfg->ht_sec_ch_freq =
				sap_ctx->acs_cfg->pri_ch_freq + 20;
	else
		sap_ctx->acs_cfg->ht_sec_ch_freq = 0;
}

/**
 * sap_hdd_signal_event_handler() - routine to inform hostapd via callback
 *
 * ctx: pointer to sap context which was passed to callback
 *
 * this routine will be registered as callback to sme_close_session, so upon
 * closure of sap session it notifies the hostapd
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sap_hdd_signal_event_handler(void *ctx)
{
	struct sap_context *sap_ctx = ctx;
	QDF_STATUS status;

	if (!sap_ctx) {
		sap_err("sap context is not valid");
		return QDF_STATUS_E_FAILURE;
	}
	status = sap_signal_hdd_event(sap_ctx, NULL,
			sap_ctx->sap_state,
			(void *) sap_ctx->sap_status);
	return status;
}

/**
 * acs_scan_done_status_str() - parse scan status to string
 * @status: scan status
 *
 * This function parse scan status to string
 *
 * Return: status string
 *
 */
static const char *acs_scan_done_status_str(eCsrScanStatus status)
{
	switch (status) {
	case eCSR_SCAN_SUCCESS:
		return "Success";
	case eCSR_SCAN_FAILURE:
		return "Failure";
	case eCSR_SCAN_ABORT:
		return "Abort";
	case eCSR_SCAN_FOUND_PEER:
		return "Found peer";
	default:
		return "Unknown";
	}
}

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
static void wlansap_send_acs_success_event(struct sap_context *sap_ctx,
					   uint32_t scan_id)
{
	if (scan_id) {
		sap_debug("Sending ACS Scan skip event");
		sap_signal_hdd_event(sap_ctx, NULL,
				     eSAP_ACS_SCAN_SUCCESS_EVENT,
				     (void *)eSAP_STATUS_SUCCESS);
	} else {
		sap_debug("ACS scanid: %d (skipped ACS SCAN)", scan_id);
	}
}
#else
static inline void wlansap_send_acs_success_event(struct sap_context *sap_ctx,
						  uint32_t scan_id)
{
}
#endif

static uint32_t
wlansap_calculate_chan_from_scan_result(mac_handle_t mac_handle,
					struct sap_context *sap_ctx,
					uint32_t scan_id)
{
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	qdf_list_t *list = NULL;
	struct scan_filter *filter;
	uint32_t oper_channel = SAP_CHANNEL_NOT_SELECTED;

	filter = qdf_mem_malloc(sizeof(*filter));

	if (filter)
		filter->age_threshold = qdf_get_time_of_the_day_ms() -
						sap_ctx->acs_req_timestamp;

	list = ucfg_scan_get_result(mac_ctx->pdev, filter);

	if (filter)
		qdf_mem_free(filter);

	if (list)
		sap_debug("num_entries %d", qdf_list_size(list));

	wlansap_send_acs_success_event(sap_ctx, scan_id);

	oper_channel = sap_select_channel(mac_handle, sap_ctx, list);
	ucfg_scan_purge_results(list);

	return oper_channel;
}

static void
wlansap_filter_unsafe_ch(struct wlan_objmgr_psoc *psoc,
			 struct sap_context *sap_ctx)
{
	uint16_t i;
	uint16_t num_safe_ch = 0;
	uint32_t freq;

	/*
	 * There are two channel list, one acs cfg channel list, and one
	 * sap_ctx->freq_list, the unsafe channels for acs cfg is updated here
	 * and the sap_ctx->freq list would be handled in sap_chan_sel_init
	 * which would consider more params other than unsafe channels.
	 * So the two lists now would be in sync. But in case the ACS weight
	 * calculation does not get through due to no scan result or no chan
	 * selected, or any other reason, the default channel is chosen which
	 * would contain the channels in acs cfg. Now since the scan takes time
	 * there could be channels present in acs cfg that could become unsafe
	 * in the mean time, so it is better to filter out those channels from
	 * the acs channel list before chosing one of them as a default channel
	 */
	for (i = 0; i < sap_ctx->acs_cfg->ch_list_count; i++) {
		freq = sap_ctx->acs_cfg->freq_list[i];
		if (!policy_mgr_is_sap_freq_allowed(psoc, freq)) {
			sap_debug("remove freq %d from acs list", freq);
			continue;
		}
		/* Add only allowed channels to the acs cfg ch list */
		sap_ctx->acs_cfg->freq_list[num_safe_ch++] =
						sap_ctx->acs_cfg->freq_list[i];
	}

	sap_debug("Updated ACS ch list len %d", num_safe_ch);
	sap_ctx->acs_cfg->ch_list_count = num_safe_ch;
}

static void
wlan_sap_filter_non_preferred_channels(struct wlan_objmgr_pdev *pdev,
				       struct sap_context *sap_ctx)
{
	uint16_t i;
	uint16_t num_ch = 0;
	bool preferred_freq_found = false;

	for (i = 0; i < sap_ctx->acs_cfg->ch_list_count; i++) {
		if (sap_ctx->acs_cfg->freq_list[i] == 2467 ||
		    sap_ctx->acs_cfg->freq_list[i] == 2472 ||
		    sap_ctx->acs_cfg->freq_list[i] == 2477) {
			sap_debug("Skip freq %d if preferred freq present",
				  sap_ctx->acs_cfg->freq_list[i]);
			continue;
		}
		sap_ctx->acs_cfg->freq_list[num_ch++] =
						sap_ctx->acs_cfg->freq_list[i];
		preferred_freq_found = true;
	}

	if (!preferred_freq_found) {
		sap_debug("No preferred freq, list unchanged");
		return;
	}
	sap_debug("preferred frequencies found updated ACS ch list len %d",
		  num_ch);
	sap_ctx->acs_cfg->ch_list_count = num_ch;
}

QDF_STATUS wlansap_pre_start_bss_acs_scan_callback(mac_handle_t mac_handle,
						   struct sap_context *sap_ctx,
						   uint8_t sessionid,
						   uint32_t scanid,
						   eCsrScanStatus scan_status)
{
	uint32_t oper_channel = SAP_CHANNEL_NOT_SELECTED;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);

	host_log_acs_scan_done(acs_scan_done_status_str(scan_status),
			  sessionid, scanid);

	/* This has to be done before the ACS selects default channel */
	wlansap_filter_unsafe_ch(mac_ctx->psoc, sap_ctx);

	wlan_sap_filter_non_preferred_channels(mac_ctx->pdev, sap_ctx);
	if (!sap_ctx->acs_cfg->ch_list_count) {
		oper_channel =
			sap_select_default_oper_chan(mac_ctx,
						     sap_ctx->acs_cfg);
		sap_ctx->chan_freq = oper_channel;
		sap_ctx->acs_cfg->pri_ch_freq = oper_channel;
		sap_config_acs_result(mac_handle, sap_ctx,
				      sap_ctx->acs_cfg->ht_sec_ch_freq);
		sap_ctx->sap_state = eSAP_ACS_CHANNEL_SELECTED;
		sap_ctx->sap_status = eSAP_STATUS_SUCCESS;
		goto close_session;
	}
	if (eCSR_SCAN_SUCCESS != scan_status) {
		sap_err("CSR scan_status = eCSR_SCAN_ABORT/FAILURE (%d), choose default channel",
			scan_status);
		oper_channel =
			sap_select_default_oper_chan(mac_ctx,
						     sap_ctx->acs_cfg);
		wlansap_set_acs_ch_freq(sap_ctx, oper_channel);
		sap_ctx->acs_cfg->pri_ch_freq = oper_channel;
		sap_config_acs_result(mac_handle, sap_ctx,
				      sap_ctx->acs_cfg->ht_sec_ch_freq);
		sap_ctx->sap_state = eSAP_ACS_CHANNEL_SELECTED;
		sap_ctx->sap_status = eSAP_STATUS_SUCCESS;
		goto close_session;
	}
	sap_debug("CSR scan_status = eCSR_SCAN_SUCCESS (%d)", scan_status);

	oper_channel = wlansap_calculate_chan_from_scan_result(mac_handle,
							       sap_ctx, scanid);

	if (oper_channel == SAP_CHANNEL_NOT_SELECTED) {
		sap_info("No suitable channel, so select default channel");
		oper_channel = sap_select_default_oper_chan(mac_ctx,
							    sap_ctx->acs_cfg);
	}

	wlansap_set_acs_ch_freq(sap_ctx, oper_channel);
	sap_ctx->acs_cfg->pri_ch_freq = oper_channel;
	sap_config_acs_result(mac_handle, sap_ctx,
			      sap_ctx->acs_cfg->ht_sec_ch_freq);

	wlansap_dump_acs_ch_freq(sap_ctx);

	sap_ctx->sap_state = eSAP_ACS_CHANNEL_SELECTED;
	sap_ctx->sap_status = eSAP_STATUS_SUCCESS;
close_session:
#ifdef SOFTAP_CHANNEL_RANGE
	if (sap_ctx->freq_list) {
		/*
		* Always free up the memory for
		* channel selection whatever
		* the result
		*/
		qdf_mem_free(sap_ctx->freq_list);
		sap_ctx->freq_list = NULL;
		sap_ctx->num_of_channel = 0;
	}
#endif
	sap_hdd_signal_event_handler(sap_ctx);

	return QDF_STATUS_SUCCESS;
}

/**
 * wlansap_roam_process_ch_change_success() - handles the case for
 * eCSR_ROAM_RESULT_CHANNEL_CHANGE_SUCCESS in function wlansap_roam_callback()
 *
 * @mac_ctx:        mac global context
 * @sap_ctx:        sap context
 * @csr_roam_info:  raom info struct
 * @ret_status:     update return status
 *
 * Return: void
 */
static void
wlansap_roam_process_ch_change_success(struct mac_context *mac_ctx,
				      struct sap_context *sap_ctx,
				      struct csr_roam_info *csr_roam_info,
				      QDF_STATUS *ret_status)
{
	struct sap_sm_event sap_event;
	QDF_STATUS qdf_status;
	bool is_ch_dfs = false;
	uint32_t target_chan_freq;
	/*
	 * Channel change is successful. If the new channel is a DFS channel,
	 * then we will to perform channel availability check for 60 seconds
	 */
	sap_nofl_debug("sapdfs: SAP CSA: freq to [%d] state %d",
		       mac_ctx->sap.SapDfsInfo.target_chan_freq,
		       sap_ctx->fsm_state);
	target_chan_freq = mac_ctx->sap.SapDfsInfo.target_chan_freq;

	/* If SAP is not in starting or started state don't proceed further */
	if (sap_ctx->fsm_state == SAP_INIT ||
	    sap_ctx->fsm_state == SAP_STOPPING) {
		sap_info("sapdfs: state [%d] not starting SAP after channel change",
			  sap_ctx->fsm_state);
		return;
	}

	if (sap_ctx->ch_params.ch_width == CH_WIDTH_160MHZ) {
		is_ch_dfs = true;
	} else if (sap_ctx->ch_params.ch_width == CH_WIDTH_80P80MHZ) {
		if (wlan_reg_get_channel_state_for_freq(
						mac_ctx->pdev,
						target_chan_freq) ==
		    CHANNEL_STATE_DFS ||
		    wlan_reg_get_channel_state_for_freq(
					mac_ctx->pdev,
					sap_ctx->ch_params.mhz_freq_seg1) ==
				CHANNEL_STATE_DFS)
			is_ch_dfs = true;
	} else {
		if (wlan_reg_get_channel_state_for_freq(
						mac_ctx->pdev,
						target_chan_freq) ==
		    CHANNEL_STATE_DFS)
			is_ch_dfs = true;
	}
	if (WLAN_REG_IS_6GHZ_CHAN_FREQ(sap_ctx->chan_freq))
		is_ch_dfs = false;

	sap_ctx->chan_freq = target_chan_freq;
	/* check if currently selected channel is a DFS channel */
	if (is_ch_dfs && sap_ctx->pre_cac_complete) {
		/* Start beaconing on the new pre cac channel */
		wlansap_start_beacon_req(sap_ctx);
		sap_ctx->fsm_state = SAP_STARTING;
		mac_ctx->sap.SapDfsInfo.sap_radar_found_status = false;
		sap_event.event = eSAP_MAC_START_BSS_SUCCESS;
		sap_event.params = csr_roam_info;
		sap_event.u1 = eCSR_ROAM_INFRA_IND;
		sap_event.u2 = eCSR_ROAM_RESULT_INFRA_STARTED;
	} else if (is_ch_dfs) {
		if ((false == mac_ctx->sap.SapDfsInfo.ignore_cac)
		    && (eSAP_DFS_DO_NOT_SKIP_CAC ==
			mac_ctx->sap.SapDfsInfo.cac_state) &&
		    policy_mgr_get_dfs_master_dynamic_enabled(
					mac_ctx->psoc,
					sap_ctx->sessionId)) {
			sap_ctx->fsm_state = SAP_INIT;
			/* DFS Channel */
			sap_event.event = eSAP_DFS_CHANNEL_CAC_START;
			sap_event.params = csr_roam_info;
			sap_event.u1 = 0;
			sap_event.u2 = 0;
		} else {
			/* Start beaconing on the new channel */
			wlansap_start_beacon_req(sap_ctx);
			sap_ctx->fsm_state = SAP_STARTING;
			mac_ctx->sap.SapDfsInfo.sap_radar_found_status = false;
			sap_event.event = eSAP_MAC_START_BSS_SUCCESS;
			sap_event.params = csr_roam_info;
			sap_event.u1 = eCSR_ROAM_INFRA_IND;
			sap_event.u2 = eCSR_ROAM_RESULT_INFRA_STARTED;
		}
	} else {
		/* non-DFS channel */
		sap_ctx->fsm_state = SAP_STARTING;
		mac_ctx->sap.SapDfsInfo.sap_radar_found_status = false;
		sap_event.event = eSAP_MAC_START_BSS_SUCCESS;
		sap_event.params = csr_roam_info;
		sap_event.u1 = eCSR_ROAM_INFRA_IND;
		sap_event.u2 = eCSR_ROAM_RESULT_INFRA_STARTED;
	}

	/* Handle the event */
	qdf_status = sap_fsm(sap_ctx, &sap_event);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		*ret_status = QDF_STATUS_E_FAILURE;
}

/**
 * wlansap_roam_process_dfs_chansw_update() - handles the case for
 * eCSR_ROAM_RESULT_DFS_CHANSW_UPDATE_SUCCESS in wlansap_roam_callback()
 *
 * @mac_handle:    opaque handle to the global MAC context
 * @sap_ctx:       sap context
 * @ret_status:    update return status
 *
 * Return: void
 */
static void
wlansap_roam_process_dfs_chansw_update(mac_handle_t mac_handle,
				       struct sap_context *sap_ctx,
				       QDF_STATUS *ret_status)
{
	uint8_t intf;
	QDF_STATUS qdf_status;
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	uint8_t dfs_beacon_start_req = 0;
	bool sap_scc_dfs;

	if (mac_ctx->mlme_cfg->dfs_cfg.dfs_disable_channel_switch) {
		sap_err("sapdfs: DFS channel switch disabled");
		/*
		 * Send a beacon start request to PE. CSA IE required flag from
		 * beacon template will be cleared by now. A new beacon template
		 * with no CSA IE will be sent to firmware.
		 */
		dfs_beacon_start_req = true;
		sap_ctx->pre_cac_complete = false;
		*ret_status = sme_roam_start_beacon_req(mac_handle,
							sap_ctx->bssid,
							dfs_beacon_start_req);
		return;
	}
	/*
	 * Irrespective of whether the channel switch IE was sent out
	 * successfully or not, SAP should still vacate the channel immediately
	 */
	if (sap_ctx->fsm_state != SAP_STARTED) {
		/* Further actions to be taken here */
		sap_warn("eCSR_ROAM_RESULT_DFS_RADAR_FOUND_IND received in (%d) state",
			  sap_ctx->fsm_state);
		return;
	}

	sap_debug("sapdfs: from state SAP_STARTED => SAP_STOPPING");
	sap_ctx->is_chan_change_inprogress = true;
	/*
	 * The associated stations have been informed to move to a different
	 * channel. However, the AP may not always select the advertised channel
	 * for operation if the radar is seen. In that case, the stations will
	 * experience link-loss and return back through scanning if they wish to
	 */

	/*
	 * Send channel change request. From spec it is required that the AP
	 * should continue to operate in the same mode as it is operating
	 * currently. For e.g. 20/40/80 MHz operation
	 */
	if (mac_ctx->sap.SapDfsInfo.target_chan_freq)
		wlan_reg_set_channel_params_for_freq(mac_ctx->pdev,
				mac_ctx->sap.SapDfsInfo.target_chan_freq,
				0, &sap_ctx->ch_params);

	/*
	 * Fetch the number of SAP interfaces. If the number of sap Interface
	 * more than one then we will make is_sap_ready_for_chnl_chng to true
	 * for that sapctx. If there is only one SAP interface then process
	 * immediately. If Dual BAND SAP is enabled then also process
	 * immediately, as in this case the both SAP will be in different band
	 * and channel change on one SAP doesn't mean channel change on
	 * other interface.
	 *
	 * For example,
	 * Let's say SAP(2G) + SAP(5G-DFS) is initial connection which triggered
	 * DualBand HW mode and if SAP(5G-DFS) is moving to some channel then
	 * SAP(2G) doesn't need to move.
	 *
	 * If both SAPs are not doing SCC DFS then each of them can change the
	 * channel independently. Channel change of one SAP became dependent
	 * second SAP's channel change due to some previous platform's single
	 * radio limitation.
	 *
	 * For DCS case, SAP will do channel switch one by one.
	 *
	 */
	sap_scc_dfs = sap_is_conc_sap_doing_scc_dfs(mac_handle, sap_ctx);
	if (sap_get_total_number_sap_intf(mac_handle) <= 1 ||
	    policy_mgr_is_current_hwmode_dbs(mac_ctx->psoc) ||
	    sap_ctx->csa_reason == CSA_REASON_DCS ||
	    !sap_scc_dfs) {
		sap_get_cac_dur_dfs_region(sap_ctx,
			&sap_ctx->csr_roamProfile.cac_duration_ms,
			&sap_ctx->csr_roamProfile.dfs_regdomain);
		/*
		 * Most likely, radar has been detected and SAP wants to
		 * change the channel
		 */
		qdf_status = wlansap_channel_change_request(sap_ctx,
			mac_ctx->sap.SapDfsInfo.target_chan_freq);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			*ret_status = QDF_STATUS_E_FAILURE;
		return;
	}

	sap_ctx->is_sap_ready_for_chnl_chng = true;
	/*
	 * now check if the con-current sap interface is ready
	 * for channel change. If yes then we issue channel change for
	 * both the SAPs. If no then simply return success & we will
	 * issue channel change when second AP's 5 CSA beacon Tx is
	 * completed.
	 *
	 * This check is added to take care of following scenario:
	 * if SAP1 + SAP2 is doing DFS SCC and radar is detected on that channel
	 * then SAP1 sends 5 beacons with CSA/ECSA IE and wait for SAP2 to
	 * finish sending 5 beacons. if SAP1 changes channel before SAP2 finish
	 * sending beacons then it ends up in
	 * (SAP1 new channel + SAP2 old channel) MCC with DFS scenario
	 * which causes some of the stability issues in old platforms.
	 */
	if (false ==
	    is_concurrent_sap_ready_for_channel_change(mac_handle, sap_ctx)) {
		sap_debug("sapdfs: sapctx[%pK] ready but not concurrent sap",
			  sap_ctx);
		*ret_status = QDF_STATUS_SUCCESS;
		return;
	}

	/* Issue channel change req for each sapctx */
	for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
		struct sap_context *sap_context;

		if (!((QDF_SAP_MODE == mac_ctx->sap.sapCtxList[intf].sapPersona)
		    && (mac_ctx->sap.sapCtxList[intf].sap_context)))
			continue;
		sap_context = mac_ctx->sap.sapCtxList[intf].sap_context;
		sap_debug("sapdfs:issue chnl change for sapctx[%pK]",
			  sap_context);
		sap_get_cac_dur_dfs_region(sap_context,
			&sap_context->csr_roamProfile.cac_duration_ms,
			&sap_context->csr_roamProfile.dfs_regdomain);
		/*
		 * Most likely, radar has been detected and SAP wants to
		 * change the channel
		 */
		qdf_status = wlansap_channel_change_request(sap_context,
				mac_ctx->sap.SapDfsInfo.target_chan_freq);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			sap_err("post chnl chng req failed, sap[%pK]", sap_ctx);
			*ret_status = QDF_STATUS_E_FAILURE;
		} else {
			sap_context->is_sap_ready_for_chnl_chng = false;
		}
	}
	return;
}

/**
 * wlansap_roam_process_dfs_radar_found() - handles the case for
 * eCSR_ROAM_RESULT_DFS_RADAR_FOUND_IND in wlansap_roam_callback()
 *
 * @mac_ctx:       mac global context
 * @sap_ctx:       sap context
 * @ret_status:    update return status
 *
 * Return: result of operation
 */
static void
wlansap_roam_process_dfs_radar_found(struct mac_context *mac_ctx,
				     struct sap_context *sap_ctx,
				     QDF_STATUS *ret_status)
{
	QDF_STATUS qdf_status;
	struct sap_sm_event sap_event;

	if (sap_is_dfs_cac_wait_state(sap_ctx)) {
		if (mac_ctx->mlme_cfg->dfs_cfg.dfs_disable_channel_switch) {
			sap_err("sapdfs: DFS channel switch disabled");
			return;
		}
		if (false == mac_ctx->sap.SapDfsInfo.sap_radar_found_status) {
			sap_err("sapdfs: sap_radar_found_status is false");
			return;
		}
		sap_debug("sapdfs:Posting event eSAP_DFS_CHANNEL_CAC_RADAR_FOUND");
		/*
		 * If Radar is found, while in DFS CAC WAIT State then post stop
		 * and destroy the CAC timer and post a
		 * eSAP_DFS_CHANNEL_CAC_RADAR_FOUND  to sapFsm.
		 */
		if (!sap_ctx->dfs_cac_offload) {
			qdf_mc_timer_stop(&mac_ctx->
					sap.SapDfsInfo.sap_dfs_cac_timer);
			qdf_mc_timer_destroy(&mac_ctx->
					sap.SapDfsInfo.sap_dfs_cac_timer);
		}
		mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running = false;

		/*
		 * User space is already indicated the CAC start and if
		 * CAC end on this channel is not indicated, the user
		 * space will be in some undefined state (e.g., UI frozen)
		 */
		qdf_status = sap_signal_hdd_event(sap_ctx, NULL,
				eSAP_DFS_CAC_INTERRUPTED,
				(void *) eSAP_STATUS_SUCCESS);
		if (QDF_STATUS_SUCCESS != qdf_status) {
			sap_err("Failed to send CAC end");
			/* Want to still proceed and try to switch channel.
			 * Lets try not to be on the DFS channel
			 */
		}

		sap_event.event = eSAP_DFS_CHANNEL_CAC_RADAR_FOUND;
		sap_event.params = 0;
		sap_event.u1 = 0;
		sap_event.u2 = 0;
		qdf_status = sap_fsm(sap_ctx, &sap_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			*ret_status = QDF_STATUS_E_FAILURE;
		return;
	}
	if (sap_ctx->fsm_state == SAP_STARTED) {
		sap_debug("sapdfs:Posting event eSAP_DFS_CHNL_SWITCH_ANNOUNCEMENT_START");

		/*
		 * Radar found on the operating channel in STARTED state,
		 * new operating channel has already been selected. Send
		 * request to SME-->PE for sending CSA IE
		 */
		sap_event.event = eSAP_DFS_CHNL_SWITCH_ANNOUNCEMENT_START;
		sap_event.params = 0;
		sap_event.u1 = 0;
		sap_event.u2 = 0;
		qdf_status = sap_fsm(sap_ctx, &sap_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			*ret_status = QDF_STATUS_E_FAILURE;
		return;
	}
	/* Further actions to be taken here */
	sap_err("eCSR_ROAM_RESULT_DFS_RADAR_FOUND_IND received in (%d) state",
		 sap_ctx->fsm_state);

	return;
}

/**
 * wlansap_roam_process_infra_assoc_ind() - handles the case for
 * eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND in wlansap_roam_callback()
 *
 * @sap_ctx:       sap context
 * @roam_result:   roam result
 * @csr_roam_info: roam info struct
 * @ret_status:    update return status
 *
 * Return: result of operation
 */
static void
wlansap_roam_process_infra_assoc_ind(struct sap_context *sap_ctx,
				     eCsrRoamResult roam_result,
				     struct csr_roam_info *csr_roam_info,
				     QDF_STATUS *ret_status)
{
	QDF_STATUS qdf_status;

	sap_debug("CSR roam_result = eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND (%d)",
		  roam_result);
	sap_ctx->nStaWPARSnReqIeLength = csr_roam_info->rsnIELen;
	if (sap_ctx->nStaWPARSnReqIeLength)
		qdf_mem_copy(sap_ctx->pStaWpaRsnReqIE, csr_roam_info->prsnIE,
			     sap_ctx->nStaWPARSnReqIeLength);
	/* MAC filtering */
	qdf_status = sap_is_peer_mac_allowed(sap_ctx,
				     (uint8_t *) csr_roam_info->peerMac.bytes);

	if (QDF_STATUS_SUCCESS == qdf_status) {
		qdf_status = sap_signal_hdd_event(sap_ctx,
				csr_roam_info, eSAP_STA_ASSOC_IND,
				(void *) eSAP_STATUS_SUCCESS);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			sap_err("CSR roam_result = (%d) MAC ("QDF_MAC_ADDR_FMT") fail",
				  roam_result, QDF_MAC_ADDR_REF(
					csr_roam_info->peerMac.bytes));
		*ret_status = QDF_STATUS_E_FAILURE;
		}
	} else {
		sap_warn("CSR roam_result = (%d) MAC ("QDF_MAC_ADDR_FMT") not allowed",
			  roam_result,
			  QDF_MAC_ADDR_REF(csr_roam_info->peerMac.bytes));
		*ret_status = QDF_STATUS_E_FAILURE;
	}
	return;
}

static void wlansap_update_vendor_acs_chan(struct mac_context *mac_ctx,
				struct sap_context *sap_ctx)
{
	int intf;

	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		return;
	}

	mac_ctx->sap.SapDfsInfo.target_chan_freq =
				wlan_reg_chan_to_freq(mac_ctx->pdev, sap_ctx->dfs_vendor_channel);

	mac_ctx->sap.SapDfsInfo.new_chanWidth =
				sap_ctx->dfs_vendor_chan_bw;
	mac_ctx->sap.SapDfsInfo.new_ch_params.ch_width =
				sap_ctx->dfs_vendor_chan_bw;

	if (mac_ctx->sap.SapDfsInfo.target_chan_freq != 0) {
		mac_ctx->sap.SapDfsInfo.cac_state =
			eSAP_DFS_DO_NOT_SKIP_CAC;
		sap_cac_reset_notify(MAC_HANDLE(mac_ctx));
		return;
	}
	/* App failed to provide new channel, try random channel algo */
	sap_warn("Failed to get channel from userspace");

	/* Issue stopbss for each sapctx */
	for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
		struct sap_context *sap_context;

		if (((QDF_SAP_MODE ==
		    mac_ctx->sap.sapCtxList[intf].sapPersona) ||
		    (QDF_P2P_GO_MODE ==
		    mac_ctx->sap.sapCtxList[intf].sapPersona)) &&
		    mac_ctx->sap.sapCtxList[intf].sap_context !=
		    NULL) {
			sap_context =
			    mac_ctx->sap.sapCtxList[intf].sap_context;
			sap_err("sapdfs: no available channel for sapctx[%pK], StopBss",
				 sap_context);
			wlansap_stop_bss(sap_context);
		}
	}
}

QDF_STATUS wlansap_roam_callback(void *ctx,
				 struct csr_roam_info *csr_roam_info,
				 uint32_t roam_id,
				 eRoamCmdStatus roam_status,
				 eCsrRoamResult roam_result)
{
	/* sap_ctx value */
	struct sap_context *sap_ctx = ctx;
	/* State machine event */
	struct sap_sm_event sap_event;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_ret_status = QDF_STATUS_SUCCESS;
	mac_handle_t mac_handle;
	struct mac_context *mac_ctx;
	uint8_t intf;

	if (QDF_IS_STATUS_ERROR(wlansap_context_get(sap_ctx)))
		return QDF_STATUS_E_FAILURE;

	mac_ctx = sap_get_mac_context();
	if (!mac_ctx) {
		sap_err("Invalid MAC context");
		wlansap_context_put(sap_ctx);
		return QDF_STATUS_E_NOMEM;
	}
	sap_debug("CSR roam_status = %s(%d), roam_result = %s(%d)",
		  get_e_roam_cmd_status_str(roam_status), roam_status,
		  get_e_csr_roam_result_str(roam_result), roam_result);

	mac_handle = MAC_HANDLE(mac_ctx);

	switch (roam_status) {
	case eCSR_ROAM_INFRA_IND:
		if (roam_result == eCSR_ROAM_RESULT_INFRA_START_FAILED) {
			/* Fill in the event structure */
			sap_event.event = eSAP_MAC_START_FAILS;
			sap_event.params = csr_roam_info;
			sap_event.u1 = roam_status;
			sap_event.u2 = roam_result;
			/* Handle event */
			qdf_status = sap_fsm(sap_ctx, &sap_event);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status))
				qdf_ret_status = QDF_STATUS_E_FAILURE;
		}
		break;
	case eCSR_ROAM_LOSTLINK:
		break;
	case eCSR_ROAM_MIC_ERROR_IND:
		break;
	case eCSR_ROAM_SET_KEY_COMPLETE:
		if (roam_result == eCSR_ROAM_RESULT_FAILURE)
			sap_signal_hdd_event(sap_ctx, csr_roam_info,
					     eSAP_STA_SET_KEY_EVENT,
					     (void *) eSAP_STATUS_FAILURE);
		break;
	case eCSR_ROAM_ASSOCIATION_COMPLETION:
		if (roam_result == eCSR_ROAM_RESULT_FAILURE)
			sap_signal_hdd_event(sap_ctx, csr_roam_info,
					     eSAP_STA_REASSOC_EVENT,
					     (void *) eSAP_STATUS_FAILURE);
		break;
	case eCSR_ROAM_DISASSOCIATED:
		if (roam_result == eCSR_ROAM_RESULT_MIC_FAILURE)
			sap_signal_hdd_event(sap_ctx, csr_roam_info,
					     eSAP_STA_MIC_FAILURE_EVENT,
					     (void *) eSAP_STATUS_FAILURE);
		break;
	case eCSR_ROAM_WPS_PBC_PROBE_REQ_IND:
		break;
	case eCSR_ROAM_DISCONNECT_ALL_P2P_CLIENTS:
		sap_signal_hdd_event(sap_ctx, csr_roam_info,
				     eSAP_DISCONNECT_ALL_P2P_CLIENT,
				     (void *) eSAP_STATUS_SUCCESS);
		break;
	case eCSR_ROAM_SEND_P2P_STOP_BSS:
		sap_debug("Received stopbss");
		sap_signal_hdd_event(sap_ctx, csr_roam_info,
				     eSAP_MAC_TRIG_STOP_BSS_EVENT,
				     (void *) eSAP_STATUS_SUCCESS);
		break;
	case eCSR_ROAM_CHANNEL_COMPLETE_IND:
		sap_debug("Received new channel from app");
		wlansap_update_vendor_acs_chan(mac_ctx, sap_ctx);
		break;

	case eCSR_ROAM_DFS_RADAR_IND:
		sap_debug("Rcvd Radar Indication on sap ch freq %d, session %d",
			  sap_ctx->chan_freq, sap_ctx->sessionId);

		if (!policy_mgr_get_dfs_master_dynamic_enabled(
				mac_ctx->psoc, sap_ctx->sessionId)) {
			sap_debug("Ignore the Radar indication");
			goto EXIT;
		}

		if (sap_ctx->fsm_state != SAP_STARTED &&
		    !sap_is_dfs_cac_wait_state(sap_ctx)) {
			sap_debug("Ignore Radar event in sap state %d cac wait state %d",
				  sap_ctx->fsm_state,
				  sap_is_dfs_cac_wait_state(sap_ctx));
			goto EXIT;
		}

		if (!sap_chan_bond_dfs_sub_chan(
			sap_ctx, wlan_reg_freq_to_chan(mac_ctx->pdev,
						       sap_ctx->chan_freq),
			PHY_CHANNEL_BONDING_STATE_MAX))  {
			sap_debug("Ignore Radar event for sap ch freq: %d",
				  sap_ctx->chan_freq);
			goto EXIT;
		}

		if (sap_ctx->is_pre_cac_on) {
			sap_debug("sapdfs: Radar detect on pre cac:%d",
				  sap_ctx->sessionId);
			if (!sap_ctx->dfs_cac_offload) {
				qdf_mc_timer_stop(
				&mac_ctx->sap.SapDfsInfo.sap_dfs_cac_timer);
				qdf_mc_timer_destroy(
				&mac_ctx->sap.SapDfsInfo.sap_dfs_cac_timer);
			}
			mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running =
				false;
			sap_signal_hdd_event(sap_ctx, NULL,
					eSAP_DFS_RADAR_DETECT_DURING_PRE_CAC,
					(void *) eSAP_STATUS_SUCCESS);
			break;
		}

		sap_debug("sapdfs: Indicate eSAP_DFS_RADAR_DETECT to HDD");
		sap_signal_hdd_event(sap_ctx, NULL, eSAP_DFS_RADAR_DETECT,
				     (void *) eSAP_STATUS_SUCCESS);
		mac_ctx->sap.SapDfsInfo.target_chan_freq =
			sap_indicate_radar(sap_ctx);

		/* if there is an assigned next channel hopping */
		if (0 < mac_ctx->sap.SapDfsInfo.user_provided_target_chan_freq) {
			mac_ctx->sap.SapDfsInfo.target_chan_freq =
			   mac_ctx->sap.SapDfsInfo.user_provided_target_chan_freq;
			mac_ctx->sap.SapDfsInfo.user_provided_target_chan_freq =
			   0;
		}
		/* if external acs enabled */
		if (sap_ctx->vendor_acs_dfs_lte_enabled &&
		    !mac_ctx->sap.SapDfsInfo.target_chan_freq) {
			/* Return from here, processing will be done later */
			goto EXIT;
		}
		if (mac_ctx->sap.SapDfsInfo.target_chan_freq != 0) {
			mac_ctx->sap.SapDfsInfo.cac_state =
				eSAP_DFS_DO_NOT_SKIP_CAC;
			sap_cac_reset_notify(mac_handle);
			break;
		}
		/* Issue stopbss for each sapctx */
		for (intf = 0; intf < SAP_MAX_NUM_SESSION; intf++) {
			struct sap_context *sap_context;
			struct csr_roam_profile *profile;

			if (((QDF_SAP_MODE ==
			    mac_ctx->sap.sapCtxList[intf].sapPersona) ||
			    (QDF_P2P_GO_MODE ==
			    mac_ctx->sap.sapCtxList[intf].sapPersona)) &&
			    mac_ctx->sap.sapCtxList[intf].sap_context !=
			    NULL) {
				sap_context =
				    mac_ctx->sap.sapCtxList[intf].sap_context;
				profile = &sap_context->csr_roamProfile;
				if (!wlan_reg_is_passive_or_disable_for_freq(
						mac_ctx->pdev,
						profile->op_freq))
					continue;
				sap_debug("Vdev %d no channel available , stop bss",
					  sap_context->sessionId);
				sap_signal_hdd_event(sap_context, NULL,
					eSAP_STOP_BSS_DUE_TO_NO_CHNL,
					(void *) eSAP_STATUS_SUCCESS);
			}
		}
		break;
	case eCSR_ROAM_DFS_CHAN_SW_NOTIFY:
		sap_debug("Received Chan Sw Update Notification");
		break;
	case eCSR_ROAM_SET_CHANNEL_RSP:
		sap_debug("Received set channel response");
		break;
	case eCSR_ROAM_CAC_COMPLETE_IND:
		sap_debug("Received cac complete indication");
		break;
	case eCSR_ROAM_EXT_CHG_CHNL_IND:
		sap_debug("Received set channel Indication");
		break;
	default:
		break;
	}

	switch (roam_result) {
	case eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND:
		if (csr_roam_info)
			wlansap_roam_process_infra_assoc_ind(sap_ctx,
						roam_result,
						csr_roam_info, &qdf_ret_status);
		break;
	case eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF:
		if (!csr_roam_info) {
			sap_err("csr_roam_info is NULL");
			qdf_ret_status = QDF_STATUS_E_NULL_VALUE;
			break;
		}
		sap_ctx->nStaWPARSnReqIeLength = csr_roam_info->rsnIELen;
		if (sap_ctx->nStaWPARSnReqIeLength)
			qdf_mem_copy(sap_ctx->pStaWpaRsnReqIE,
				     csr_roam_info->prsnIE,
				     sap_ctx->nStaWPARSnReqIeLength);

		/* Fill in the event structure */
		qdf_status = sap_signal_hdd_event(sap_ctx, csr_roam_info,
					eSAP_STA_ASSOC_EVENT,
					(void *) eSAP_STATUS_SUCCESS);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			qdf_ret_status = QDF_STATUS_E_FAILURE;
		break;
	case eCSR_ROAM_RESULT_DEAUTH_IND:
	case eCSR_ROAM_RESULT_DISASSOC_IND:
		/* Fill in the event structure */
		qdf_status = sap_signal_hdd_event(sap_ctx, csr_roam_info,
					eSAP_STA_DISASSOC_EVENT,
					(void *) eSAP_STATUS_SUCCESS);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			qdf_ret_status = QDF_STATUS_E_FAILURE;
		break;
	case eCSR_ROAM_RESULT_MIC_ERROR_GROUP:
		/*
		 * Fill in the event structure
		 * TODO: support for group key MIC failure event to be handled
		 */
		qdf_status = sap_signal_hdd_event(sap_ctx, csr_roam_info,
						eSAP_STA_MIC_FAILURE_EVENT,
						(void *) NULL);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			qdf_ret_status = QDF_STATUS_E_FAILURE;
		break;
	case eCSR_ROAM_RESULT_MIC_ERROR_UNICAST:
		/*
		 * Fill in the event structure
		 * TODO: support for unicast key MIC failure event to be handled
		 */
		qdf_status =
			sap_signal_hdd_event(sap_ctx, csr_roam_info,
					  eSAP_STA_MIC_FAILURE_EVENT,
					  (void *) NULL);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			qdf_ret_status = QDF_STATUS_E_FAILURE;
		}
		break;
	case eCSR_ROAM_RESULT_AUTHENTICATED:
		/* Fill in the event structure */
		sap_signal_hdd_event(sap_ctx, csr_roam_info,
				  eSAP_STA_SET_KEY_EVENT,
				  (void *) eSAP_STATUS_SUCCESS);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			qdf_ret_status = QDF_STATUS_E_FAILURE;
		break;
	case eCSR_ROAM_RESULT_ASSOCIATED:
		/* Fill in the event structure */
		sap_signal_hdd_event(sap_ctx, csr_roam_info,
				     eSAP_STA_REASSOC_EVENT,
				     (void *) eSAP_STATUS_SUCCESS);
		break;
	case eCSR_ROAM_RESULT_INFRA_STARTED:
		if (!csr_roam_info) {
			sap_err("csr_roam_info is NULL");
			qdf_ret_status = QDF_STATUS_E_NULL_VALUE;
			break;
		}
		/*
		 * In the current implementation, hostapd is not aware that
		 * drive will support DFS. Hence, driver should inform
		 * eSAP_MAC_START_BSS_SUCCESS to upper layers and then perform
		 * CAC underneath
		 */
		sap_event.event = eSAP_MAC_START_BSS_SUCCESS;
		sap_event.params = csr_roam_info;
		sap_ctx->sap_sta_id = csr_roam_info->staId;
		sap_event.u1 = roam_status;
		sap_event.u2 = roam_result;
		qdf_status = sap_fsm(sap_ctx, &sap_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			qdf_ret_status = QDF_STATUS_E_FAILURE;
		break;
	case eCSR_ROAM_RESULT_INFRA_STOPPED:
		/* Fill in the event structure */
		sap_event.event = eSAP_MAC_READY_FOR_CONNECTIONS;
		sap_event.params = csr_roam_info;
		sap_event.u1 = roam_status;
		sap_event.u2 = roam_result;
		/* Handle event */
		qdf_status = sap_fsm(sap_ctx, &sap_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			qdf_ret_status = QDF_STATUS_E_FAILURE;
		break;
	case eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND:
		/*
		 * Fill in the event structure
		 * TODO: support for group key MIC failure event to be handled
		 */
		qdf_status = sap_signal_hdd_event(sap_ctx, csr_roam_info,
						eSAP_WPS_PBC_PROBE_REQ_EVENT,
						(void *) NULL);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			qdf_ret_status = QDF_STATUS_E_FAILURE;
		break;
	case eCSR_ROAM_RESULT_FORCED:
		/*
		 * This event can be used to inform hdd about user triggered
		 * disassoc event
		 * Fill in the event structure
		 */
		sap_signal_hdd_event(sap_ctx, csr_roam_info,
				     eSAP_STA_DISASSOC_EVENT,
				     (void *) eSAP_STATUS_SUCCESS);
		break;
	case eCSR_ROAM_RESULT_NONE:
		/*
		 * This event can be used to inform hdd about user triggered
		 * disassoc event
		 * Fill in the event structure
		 */
		if (roam_status == eCSR_ROAM_SET_KEY_COMPLETE)
			sap_signal_hdd_event(sap_ctx, csr_roam_info,
					     eSAP_STA_SET_KEY_EVENT,
					     (void *) eSAP_STATUS_SUCCESS);
		break;
	case eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED:
		/* Fill in the event structure */
		qdf_status = sap_signal_hdd_event(sap_ctx, csr_roam_info,
						  eSAP_MAX_ASSOC_EXCEEDED,
						  NULL);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			qdf_ret_status = QDF_STATUS_E_FAILURE;

		break;
	case eCSR_ROAM_RESULT_DFS_RADAR_FOUND_IND:
		if (!policy_mgr_get_dfs_master_dynamic_enabled(
				mac_ctx->psoc, sap_ctx->sessionId))
			break;
		wlansap_roam_process_dfs_radar_found(mac_ctx, sap_ctx,
						&qdf_ret_status);
		break;
	case eCSR_ROAM_RESULT_CSA_RESTART_RSP:
		qdf_ret_status = wlansap_dfs_send_csa_ie_request(sap_ctx);

		if (!QDF_IS_STATUS_SUCCESS(qdf_ret_status))
			sap_debug("CSR roam_result = eCSR_ROAM_RESULT_CSA_RESTART_RSP %d",
				  roam_result);
		break;
	case eCSR_ROAM_RESULT_DFS_CHANSW_UPDATE_SUCCESS:
		wlansap_roam_process_dfs_chansw_update(mac_handle, sap_ctx,
						       &qdf_ret_status);
		break;
	case eCSR_ROAM_RESULT_CAC_END_IND:
		sap_dfs_cac_timer_callback(mac_handle);
		break;
	case eCSR_ROAM_RESULT_CHANNEL_CHANGE_SUCCESS:
		wlansap_roam_process_ch_change_success(mac_ctx, sap_ctx,
						csr_roam_info, &qdf_ret_status);

		if (QDF_IS_STATUS_ERROR(qdf_ret_status))
			qdf_ret_status =
				sap_signal_hdd_event(sap_ctx, csr_roam_info,
						     eSAP_CHANNEL_CHANGE_RESP,
						   (void *)eSAP_STATUS_FAILURE);
		else
			qdf_ret_status =
				sap_signal_hdd_event(sap_ctx, csr_roam_info,
						     eSAP_CHANNEL_CHANGE_RESP,
						   (void *)QDF_STATUS_SUCCESS);
		break;
	case eCSR_ROAM_RESULT_CHANNEL_CHANGE_FAILURE:
		/* This is much more serious issue, we have to vacate the
		 * channel due to the presence of radar but our channel change
		 * failed, stop the BSS operation completely and inform hostapd
		 */
		qdf_ret_status = wlansap_stop_bss(sap_ctx);

		qdf_ret_status =
			sap_signal_hdd_event(sap_ctx, csr_roam_info,
					     eSAP_CHANNEL_CHANGE_RESP,
					     (void *)QDF_STATUS_E_FAILURE);
		break;
	case eCSR_ROAM_EXT_CHG_CHNL_UPDATE_IND:
		qdf_status = sap_signal_hdd_event(sap_ctx, csr_roam_info,
				   eSAP_ECSA_CHANGE_CHAN_IND, NULL);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			qdf_ret_status = QDF_STATUS_E_FAILURE;
		break;
	default:
		sap_err("CSR roam_result = %s (%d) not handled",
			 get_e_csr_roam_result_str(roam_result),
			 roam_result);
		break;
	}
EXIT:
	wlansap_context_put(sap_ctx);
	return qdf_ret_status;
}

void sap_scan_event_callback(struct wlan_objmgr_vdev *vdev,
			struct scan_event *event, void *arg)
{
	uint32_t scan_id;
	uint8_t session_id;
	bool success = false;
	eCsrScanStatus scan_status = eCSR_SCAN_FAILURE;
	mac_handle_t mac_handle;
	QDF_STATUS status;

	/*
	 * It may happen that the SAP was deleted before the scan
	 * cb was called. Here the sap context which was passed as an
	 * arg to the ACS cb is used after free then, and there is no way
	 * currently to validate the pointer. Now try get vdev ref before
	 * the weight calculation algo kicks in, and return if the
	 * reference cannot be taken to avoid use after free for SAP-context
	 */
	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_LEGACY_SAP_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		sap_err("Hotspot fail, vdev ref get error");
		return;
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SAP_ID);

	session_id = wlan_vdev_get_id(vdev);
	scan_id = event->scan_id;
	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	if (!mac_handle) {
		sap_alert("invalid MAC handle");
		return;
	}

	qdf_mtrace(QDF_MODULE_ID_SCAN, QDF_MODULE_ID_SAP, event->type,
		   event->vdev_id, event->scan_id);

	if (!util_is_scan_completed(event, &success))
		return;

	if (success)
		scan_status = eCSR_SCAN_SUCCESS;

	wlansap_pre_start_bss_acs_scan_callback(mac_handle,
						arg, session_id,
						scan_id, scan_status);
}
