/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
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
 * This file lim_api.cc contains the functions that are
 * exported by LIM to other modules.
 *
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "cds_api.h"
#include "wni_cfg.h"
#include "wni_api.h"
#include "sir_common.h"
#include "sir_debug.h"

#include "sch_api.h"
#include "utils_api.h"
#include "lim_api.h"
#include "lim_global.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_prop_exts_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_admit_control.h"
#include "lim_send_sme_rsp_messages.h"
#include "lim_security_utils.h"
#include "wmm_apsd.h"
#include "lim_trace.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "wma_types.h"
#include "wlan_crypto_global_api.h"

#include "rrm_api.h"

#include <lim_ft.h>
#include "qdf_types.h"
#include "cds_packet.h"
#include "cds_utils.h"
#include "sys_startup.h"
#include "cds_api.h"
#include "wlan_policy_mgr_api.h"
#include "nan_datapath.h"
#include "wma.h"
#include "wlan_mgmt_txrx_utils_api.h"
#include "wlan_objmgr_psoc_obj.h"
#include "os_if_nan.h"
#include <wlan_scan_ucfg_api.h>
#include <wlan_scan_public_structs.h>
#include <wlan_p2p_ucfg_api.h>
#include "wlan_utility.h"
#include <wlan_tdls_cfg_api.h>
#include "cfg_ucfg_api.h"
#include "wlan_mlme_public_struct.h"
#include "wlan_mlme_twt_api.h"
#include "wlan_scan_utils_api.h"
#include <qdf_hang_event_notifier.h>
#include <qdf_notifier.h>
#include "wlan_pkt_capture_ucfg_api.h"
#include "wlan_crypto_def_i.h"

struct pe_hang_event_fixed_param {
	uint16_t tlv_header;
	uint8_t vdev_id;
	uint8_t limmlmstate;
	uint8_t limprevmlmstate;
	uint8_t limsmestate;
	uint8_t limprevsmestate;
} qdf_packed;

static void __lim_init_bss_vars(struct mac_context *mac)
{
	qdf_mem_zero((void *)mac->lim.gpSession,
		    sizeof(*mac->lim.gpSession) * mac->lim.maxBssId);

	/* This is for testing purposes only, be default should always be off */
	mac->lim.gpLimMlmSetKeysReq = NULL;
}

static void __lim_init_stats_vars(struct mac_context *mac)
{
	mac->lim.gLimNumBeaconsRcvd = 0;
	mac->lim.gLimNumBeaconsIgnored = 0;

	mac->lim.gLimNumDeferredMsgs = 0;

	/* / Variable to keep track of number of currently associated STAs */
	mac->lim.gLimNumOfAniSTAs = 0; /* count of ANI peers */

	qdf_mem_zero(mac->lim.gLimHeartBeatApMac[0],
			sizeof(tSirMacAddr));
	qdf_mem_zero(mac->lim.gLimHeartBeatApMac[1],
			sizeof(tSirMacAddr));
	mac->lim.gLimHeartBeatApMacIndex = 0;

	/* Statistics to keep track of no. beacons rcvd in heart beat interval */
	qdf_mem_zero(mac->lim.gLimHeartBeatBeaconStats,
		    sizeof(mac->lim.gLimHeartBeatBeaconStats));

#ifdef WLAN_DEBUG
	/* Debug counters */
	mac->lim.numTot = 0;
	mac->lim.numBbt = 0;
	mac->lim.numProtErr = 0;
	mac->lim.numLearn = 0;
	mac->lim.numLearnIgnore = 0;
	mac->lim.numSme = 0;
	qdf_mem_zero(mac->lim.numMAC, sizeof(mac->lim.numMAC));
	mac->lim.gLimNumAssocReqDropInvldState = 0;
	mac->lim.gLimNumAssocReqDropACRejectTS = 0;
	mac->lim.gLimNumAssocReqDropACRejectSta = 0;
	mac->lim.gLimNumReassocReqDropInvldState = 0;
	mac->lim.gLimNumHashMissIgnored = 0;
	mac->lim.gLimUnexpBcnCnt = 0;
	mac->lim.gLimBcnSSIDMismatchCnt = 0;
	mac->lim.gLimNumLinkEsts = 0;
	mac->lim.gLimNumRxCleanup = 0;
	mac->lim.gLim11bStaAssocRejectCount = 0;
#endif
}

static void __lim_init_states(struct mac_context *mac)
{
	/* Counts Heartbeat failures */
	mac->lim.gLimHBfailureCntInLinkEstState = 0;
	mac->lim.gLimProbeFailureAfterHBfailedCnt = 0;
	mac->lim.gLimHBfailureCntInOtherStates = 0;
	mac->lim.gLimRspReqd = 0;
	mac->lim.gLimPrevSmeState = eLIM_SME_OFFLINE_STATE;

	/* / MLM State visible across all Sirius modules */
	MTRACE(mac_trace
		       (mac, TRACE_CODE_MLM_STATE, NO_SESSION, eLIM_MLM_IDLE_STATE));
	mac->lim.gLimMlmState = eLIM_MLM_IDLE_STATE;

	/* / Previous MLM State */
	mac->lim.gLimPrevMlmState = eLIM_MLM_OFFLINE_STATE;

	/**
	 * Initialize state to eLIM_SME_OFFLINE_STATE
	 */
	mac->lim.gLimSmeState = eLIM_SME_OFFLINE_STATE;

	/**
	 * By default assume 'unknown' role. This will be updated
	 * when SME_START_BSS_REQ is received.
	 */

	qdf_mem_zero(&mac->lim.gLimNoShortParams, sizeof(tLimNoShortParams));
	qdf_mem_zero(&mac->lim.gLimNoShortSlotParams,
		    sizeof(tLimNoShortSlotParams));

	mac->lim.gLimPhyMode = 0;
}

static void __lim_init_vars(struct mac_context *mac)
{
	/* Place holder for Measurement Req/Rsp/Ind related info */


	/* Deferred Queue Parameters */
	qdf_mem_zero(&mac->lim.gLimDeferredMsgQ, sizeof(tSirAddtsReq));

	/* addts request if any - only one can be outstanding at any time */
	qdf_mem_zero(&mac->lim.gLimAddtsReq, sizeof(tSirAddtsReq));
	mac->lim.gLimAddtsSent = 0;
	mac->lim.gLimAddtsRspTimerCount = 0;

	/* protection related config cache */
	qdf_mem_zero(&mac->lim.cfgProtection, sizeof(tCfgProtection));
	mac->lim.gLimProtectionControl = 0;
	SET_LIM_PROCESS_DEFD_MESGS(mac, true);

	/* WMM Related Flag */
	mac->lim.gUapsdEnable = 0;

	/* QoS-AC Downgrade: Initially, no AC is admitted */
	mac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] = 0;
	mac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] = 0;

	/* dialogue token List head/tail for Action frames request sent. */
	mac->lim.pDialogueTokenHead = NULL;
	mac->lim.pDialogueTokenTail = NULL;

	qdf_mem_zero(&mac->lim.tspecInfo,
		    sizeof(tLimTspecInfo) * LIM_NUM_TSPEC_MAX);

	/* admission control policy information */
	qdf_mem_zero(&mac->lim.admitPolicyInfo, sizeof(tLimAdmitPolicyInfo));
}

static void __lim_init_assoc_vars(struct mac_context *mac)
{
	mac->lim.gLimIbssStaLimit = 0;
	/* Place holder for current authentication request */
	/* being handled */
	mac->lim.gpLimMlmAuthReq = NULL;

	/* / MAC level Pre-authentication related globals */
	mac->lim.gLimPreAuthChannelNumber = 0;
	mac->lim.gLimPreAuthType = eSIR_OPEN_SYSTEM;
	qdf_mem_zero(&mac->lim.gLimPreAuthPeerAddr, sizeof(tSirMacAddr));
	mac->lim.gLimNumPreAuthContexts = 0;
	qdf_mem_zero(&mac->lim.gLimPreAuthTimerTable, sizeof(tLimPreAuthTable));

	/* Place holder for Pre-authentication node list */
	mac->lim.pLimPreAuthList = NULL;

	/* One cache for each overlap and associated case. */
	qdf_mem_zero(mac->lim.protStaOverlapCache,
		    sizeof(tCacheParams) * LIM_PROT_STA_OVERLAP_CACHE_SIZE);
	qdf_mem_zero(mac->lim.protStaCache,
		    sizeof(tCacheParams) * LIM_PROT_STA_CACHE_SIZE);

	mac->lim.pe_session = NULL;
	mac->lim.reAssocRetryAttempt = 0;

}

static void __lim_init_ht_vars(struct mac_context *mac)
{
	mac->lim.htCapabilityPresentInBeacon = 0;
	mac->lim.gHTGreenfield = 0;
	mac->lim.gHTShortGI40Mhz = 0;
	mac->lim.gHTShortGI20Mhz = 0;
	mac->lim.gHTMaxAmsduLength = 0;
	mac->lim.gHTDsssCckRate40MHzSupport = 0;
	mac->lim.gHTPSMPSupport = 0;
	mac->lim.gHTLsigTXOPProtection = 0;
	mac->lim.gHTMIMOPSState = eSIR_HT_MIMO_PS_STATIC;
	mac->lim.gHTAMpduDensity = 0;

	mac->lim.gMaxAmsduSizeEnabled = false;
	mac->lim.gHTMaxRxAMpduFactor = 0;
	mac->lim.gHTServiceIntervalGranularity = 0;
	mac->lim.gHTControlledAccessOnly = 0;
	mac->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;
	mac->lim.gHTPCOActive = 0;

	mac->lim.gHTPCOPhase = 0;
	mac->lim.gHTSecondaryBeacon = 0;
	mac->lim.gHTDualCTSProtection = 0;
	mac->lim.gHTSTBCBasicMCS = 0;
}

static QDF_STATUS __lim_init_config(struct mac_context *mac)
{
	struct mlme_ht_capabilities_info *ht_cap_info;
#ifdef FEATURE_WLAN_TDLS
	QDF_STATUS status;
	uint32_t val1;
	bool valb;
#endif

	/* Read all the CFGs here that were updated before pe_start is called */
	/* All these CFG READS/WRITES are only allowed in init, at start when there is no session
	 * and they will be used throughout when there is no session
	 */
	mac->lim.gLimIbssStaLimit = mac->mlme_cfg->sap_cfg.assoc_sta_limit;
	ht_cap_info = &mac->mlme_cfg->ht_caps.ht_cap_info;

	/* channel bonding mode could be set to anything from 0 to 4(Titan had these */
	/* modes But for Taurus we have only two modes: enable(>0) or disable(=0) */
	ht_cap_info->supported_channel_width_set =
			mac->mlme_cfg->feature_flags.channel_bonding_mode ?
			WNI_CFG_CHANNEL_BONDING_MODE_ENABLE :
			WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;

	mac->mlme_cfg->ht_caps.info_field_1.recommended_tx_width_set =
		ht_cap_info->supported_channel_width_set;

	if (!mac->mlme_cfg->timeouts.heart_beat_threshold) {
		mac->sys.gSysEnableLinkMonitorMode = 0;
	} else {
		/* No need to activate the timer during init time. */
		mac->sys.gSysEnableLinkMonitorMode = 1;
	}

	/* WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA - not needed */

	/* This was initially done after resume notification from HAL. Now, DAL is
	   started before PE so this can be done here */
	handle_ht_capabilityand_ht_info(mac, NULL);
#ifdef FEATURE_WLAN_TDLS
	status = cfg_tdls_get_buffer_sta_enable(mac->psoc, &valb);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("cfg get LimTDLSBufStaEnabled failed");
		return QDF_STATUS_E_FAILURE;
	}
	mac->lim.gLimTDLSBufStaEnabled = (uint8_t)valb;

	status = cfg_tdls_get_uapsd_mask(mac->psoc, &val1);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("cfg get LimTDLSUapsdMask failed");
		return QDF_STATUS_E_FAILURE;
	}
	mac->lim.gLimTDLSUapsdMask = (uint8_t)val1;

	status = cfg_tdls_get_off_channel_enable(mac->psoc, &valb);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("cfg get LimTDLSUapsdMask failed");
		return QDF_STATUS_E_FAILURE;
	}
	mac->lim.gLimTDLSOffChannelEnabled = (uint8_t)valb;

	status = cfg_tdls_get_wmm_mode_enable(mac->psoc, &valb);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("cfg get LimTDLSWmmMode failed");
		return QDF_STATUS_E_FAILURE;
	}
	mac->lim.gLimTDLSWmmMode = (uint8_t)valb;
#endif

	return QDF_STATUS_SUCCESS;
}

/*
   lim_start
   This function is to replace the __lim_process_sme_start_req since there is no
   eWNI_SME_START_REQ post to PE.
 */
QDF_STATUS lim_start(struct mac_context *mac)
{
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;

	pe_debug("enter");

	if (mac->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE) {
		mac->lim.gLimSmeState = eLIM_SME_IDLE_STATE;

		MTRACE(mac_trace
			       (mac, TRACE_CODE_SME_STATE, NO_SESSION,
			       mac->lim.gLimSmeState));

		/* Initialize MLM state machine */
		if (QDF_STATUS_SUCCESS != lim_init_mlm(mac)) {
			pe_err("Init MLM failed");
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		/**
		 * Should not have received eWNI_SME_START_REQ in states
		 * other than OFFLINE. Return response to host and
		 * log error
		 */
		pe_warn("Invalid SME state: %X",
			mac->lim.gLimSmeState);
		retCode = QDF_STATUS_E_FAILURE;
	}

	mac->lim.req_id =
		ucfg_scan_register_requester(mac->psoc,
					     "LIM",
					     lim_process_rx_scan_handler,
					     mac);
	return retCode;
}

/**
 * lim_initialize()
 *
 ***FUNCTION:
 * This function is called from LIM thread entry function.
 * LIM related global data structures are initialized in this function.
 *
 ***LOGIC:
 * NA
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  mac - Pointer to global MAC structure
 * @return None
 */

QDF_STATUS lim_initialize(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	mac->lim.tdls_frm_session_id = NO_SESSION;
	mac->lim.deferredMsgCnt = 0;
	mac->lim.retry_packet_cnt = 0;
	mac->lim.deauthMsgCnt = 0;
	mac->lim.disassocMsgCnt = 0;

	__lim_init_assoc_vars(mac);
	__lim_init_vars(mac);
	__lim_init_states(mac);
	__lim_init_stats_vars(mac);
	__lim_init_bss_vars(mac);
	__lim_init_ht_vars(mac);

	rrm_initialize(mac);

	if (QDF_IS_STATUS_ERROR(qdf_mutex_create(
					&mac->lim.lim_frame_register_lock))) {
		pe_err("lim lock init failed!");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_list_create(&mac->lim.gLimMgmtFrameRegistratinQueue, 0);

	/* initialize the TSPEC admission control table. */
	/* Note that this was initially done after resume notification from HAL. */
	/* Now, DAL is started before PE so this can be done here */
	lim_admit_control_init(mac);
	return status;

} /*** end lim_initialize() ***/

/**
 * lim_cleanup()
 *
 ***FUNCTION:
 * This function is called upon reset or persona change
 * to cleanup LIM state
 *
 ***LOGIC:
 * NA
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  mac - Pointer to Global MAC structure
 * @return None
 */

void lim_cleanup(struct mac_context *mac)
{
	uint8_t i;
	qdf_list_node_t *lst_node;

	/*
	 * Before destroying the list making sure all the nodes have been
	 * deleted
	 */
	while (qdf_list_remove_front(
			&mac->lim.gLimMgmtFrameRegistratinQueue,
			&lst_node) == QDF_STATUS_SUCCESS) {
		qdf_mem_free(lst_node);
	}
	qdf_list_destroy(&mac->lim.gLimMgmtFrameRegistratinQueue);
	qdf_mutex_destroy(&mac->lim.lim_frame_register_lock);

	pe_deregister_mgmt_rx_frm_callback(mac);

	/* free up preAuth table */
	if (mac->lim.gLimPreAuthTimerTable.pTable) {
		for (i = 0; i < mac->lim.gLimPreAuthTimerTable.numEntry; i++)
			qdf_mem_free(mac->lim.gLimPreAuthTimerTable.pTable[i]);
		qdf_mem_free(mac->lim.gLimPreAuthTimerTable.pTable);
		mac->lim.gLimPreAuthTimerTable.pTable = NULL;
		mac->lim.gLimPreAuthTimerTable.numEntry = 0;
	}

	if (mac->lim.pDialogueTokenHead) {
		lim_delete_dialogue_token_list(mac);
	}

	if (mac->lim.pDialogueTokenTail) {
		qdf_mem_free(mac->lim.pDialogueTokenTail);
		mac->lim.pDialogueTokenTail = NULL;
	}

	if (mac->lim.gpLimMlmSetKeysReq) {
		qdf_mem_zero(mac->lim.gpLimMlmSetKeysReq,
			     sizeof(tLimMlmSetKeysReq));
		qdf_mem_free(mac->lim.gpLimMlmSetKeysReq);
		mac->lim.gpLimMlmSetKeysReq = NULL;
	}

	if (mac->lim.gpLimMlmAuthReq) {
		qdf_mem_free(mac->lim.gpLimMlmAuthReq);
		mac->lim.gpLimMlmAuthReq = NULL;
	}

	if (mac->lim.limDisassocDeauthCnfReq.pMlmDisassocReq) {
		qdf_mem_free(mac->lim.limDisassocDeauthCnfReq.pMlmDisassocReq);
		mac->lim.limDisassocDeauthCnfReq.pMlmDisassocReq = NULL;
	}

	if (mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq) {
		qdf_mem_free(mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq);
		mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq = NULL;
	}

	/* Now, finally reset the deferred message queue pointers */
	lim_reset_deferred_msg_q(mac);

	for (i = 0; i < MAX_MEASUREMENT_REQUEST; i++)
		rrm_cleanup(mac, i);

	lim_ft_cleanup_all_ft_sessions(mac);

	ucfg_scan_unregister_requester(mac->psoc, mac->lim.req_id);
} /*** end lim_cleanup() ***/

#ifdef WLAN_FEATURE_MEMDUMP_ENABLE
/**
 * lim_state_info_dump() - print state information of lim layer
 * @buf: buffer pointer
 * @size: size of buffer to be filled
 *
 * This function is used to print state information of lim layer
 *
 * Return: None
 */
static void lim_state_info_dump(char **buf_ptr, uint16_t *size)
{
	struct mac_context *mac;
	uint16_t len = 0;
	char *buf = *buf_ptr;

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		QDF_ASSERT(0);
		return;
	}

	pe_debug("size of buffer: %d", *size);

	len += qdf_scnprintf(buf + len, *size - len,
		"\n SmeState: %d", mac->lim.gLimSmeState);
	len += qdf_scnprintf(buf + len, *size - len,
		"\n PrevSmeState: %d", mac->lim.gLimPrevSmeState);
	len += qdf_scnprintf(buf + len, *size - len,
		"\n MlmState: %d", mac->lim.gLimMlmState);
	len += qdf_scnprintf(buf + len, *size - len,
		"\n PrevMlmState: %d", mac->lim.gLimPrevMlmState);
	len += qdf_scnprintf(buf + len, *size - len,
		"\n ProcessDefdMsgs: %d", mac->lim.gLimProcessDefdMsgs);

	*size -= len;
	*buf_ptr += len;
}

/**
 * lim_register_debug_callback() - registration function for lim layer
 * to print lim state information
 *
 * Return: None
 */
static void lim_register_debug_callback(void)
{
	qdf_register_debug_callback(QDF_MODULE_ID_PE, &lim_state_info_dump);
}
#else /* WLAN_FEATURE_MEMDUMP_ENABLE */
static void lim_register_debug_callback(void)
{
}
#endif /* WLAN_FEATURE_MEMDUMP_ENABLE */

#ifdef WLAN_FEATURE_NAN
static void lim_nan_register_callbacks(struct mac_context *mac_ctx)
{
	struct nan_callbacks cb_obj = {0};

	cb_obj.add_ndi_peer = lim_add_ndi_peer_converged;
	cb_obj.ndp_delete_peers = lim_ndp_delete_peers_converged;
	cb_obj.delete_peers_by_addr = lim_ndp_delete_peers_by_addr_converged;

	ucfg_nan_register_lim_callbacks(mac_ctx->psoc, &cb_obj);
}
#else
static inline void lim_nan_register_callbacks(struct mac_context *mac_ctx)
{
}
#endif

#ifdef WLAN_FEATURE_11W
void lim_stop_pmfcomeback_timer(struct pe_session *session)
{
	if (session->opmode != QDF_STA_MODE)
		return;

	qdf_mc_timer_stop(&session->pmf_retry_timer);
	session->pmf_retry_timer_info.retried = false;
}
#endif

/*
 * pe_shutdown_notifier_cb - Shutdown notifier callback
 * @ctx: Pointer to Global MAC structure
 *
 * Return: None
 */
static void pe_shutdown_notifier_cb(void *ctx)
{
	struct mac_context *mac_ctx = (struct mac_context *)ctx;
	struct pe_session *session;
	uint8_t i;

	lim_deactivate_timers(mac_ctx);
	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		session = &mac_ctx->lim.gpSession[i];
		if (session->valid == true) {
			if (LIM_IS_AP_ROLE(session))
				qdf_mc_timer_stop(&session->
						 protection_fields_reset_timer);
			lim_stop_pmfcomeback_timer(session);
		}
	}
}

#ifdef WLAN_FEATURE_11W
/**
 * is_mgmt_protected - check RMF enabled for the peer
 * @vdev_id: vdev id
 * @peer_mac_addr: peer mac address
 *
 * The function check the mgmt frame protection enabled or not
 * for station mode and AP mode
 *
 * Return: true, if the connection is RMF enabled.
 */
static bool is_mgmt_protected(uint32_t vdev_id,
				  const uint8_t *peer_mac_addr)
{
	uint16_t aid;
	tpDphHashNode sta_ds;
	struct pe_session *session;
	bool protected = false;
	struct mac_context *mac_ctx = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac_ctx)
		return false;

	session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session) {
		/* couldn't find session */
		pe_err("Session not found for vdev_id: %d", vdev_id);
		return false;
	}

	sta_ds = dph_lookup_hash_entry(mac_ctx, (uint8_t *)peer_mac_addr, &aid,
				       &session->dph.dphHashTable);
	if (sta_ds) {
		/* rmfenabled will be set at the time of addbss.
		 * but sometimes EAP auth fails and keys are not
		 * installed then if we send any management frame
		 * like deauth/disassoc with this bit set then
		 * firmware crashes. so check for keys are
		 * installed or not also before setting the bit
		 */
		if (sta_ds->rmfEnabled && sta_ds->is_key_installed)
			protected = true;
	}

	return protected;
}

#else
/**
 * is_mgmt_protected - check RMF enabled for the peer
 * @vdev_id: vdev id
 * @peer_mac_addr: peer mac address
 *
 * The function check the mgmt frame protection enabled or not
 * for station mode and AP mode
 *
 * Return: true, if the connection is RMF enabled.
 */
static bool is_mgmt_protected(uint32_t vdev_id,
				  const uint8_t *peer_mac_addr)
{
	return false;
}
#endif

static void p2p_register_callbacks(struct mac_context *mac_ctx)
{
	struct p2p_protocol_callbacks p2p_cb = {0};

	p2p_cb.is_mgmt_protected = is_mgmt_protected;
	ucfg_p2p_register_callbacks(mac_ctx->psoc, &p2p_cb);
}

/*
 * lim_register_sap_bcn_callback(): Register a callback with scan module for SAP
 * @mac_ctx: pointer to the global mac context
 *
 * Registers the function lim_handle_sap_beacon as callback with the Scan
 * module to handle beacon frames for SAP sessions
 *
 * Return: QDF Status
 */
static QDF_STATUS lim_register_sap_bcn_callback(struct mac_context *mac_ctx)
{
	QDF_STATUS status;

	status = ucfg_scan_register_bcn_cb(mac_ctx->psoc,
			lim_handle_sap_beacon,
			SCAN_CB_TYPE_UPDATE_BCN);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("failed with status code %08d [x%08x]",
			status, status);
	}

	return status;
}

/*
 * lim_unregister_sap_bcn_callback(): Unregister the callback with scan module
 * @mac_ctx: pointer to the global mac context
 *
 * Unregisters the callback registered with the Scan
 * module to handle beacon frames for SAP sessions
 *
 * Return: QDF Status
 */
static QDF_STATUS lim_unregister_sap_bcn_callback(struct mac_context *mac_ctx)
{
	QDF_STATUS status;

	status = ucfg_scan_register_bcn_cb(mac_ctx->psoc,
			NULL, SCAN_CB_TYPE_UPDATE_BCN);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("failed with status code %08d [x%08x]",
			status, status);
	}

	return status;
}

static void lim_register_policy_mgr_callback(struct wlan_objmgr_psoc *psoc)
{
	struct policy_mgr_conc_cbacks conc_cbacks;

	qdf_mem_zero(&conc_cbacks, sizeof(conc_cbacks));
	conc_cbacks.connection_info_update = lim_send_conc_params_update;

	if (QDF_STATUS_SUCCESS != policy_mgr_register_conc_cb(psoc,
							      &conc_cbacks)) {
		pe_err("failed to register policy manager callbacks");
	}
}

static int pe_hang_event_notifier_call(struct notifier_block *block,
				       unsigned long state,
				       void *data)
{
	qdf_notif_block *notif_block = qdf_container_of(block, qdf_notif_block,
							notif_block);
	struct mac_context *mac;
	struct pe_session *session;
	struct qdf_notifer_data *pe_hang_data = data;
	uint8_t *pe_data;
	uint8_t i;
	struct pe_hang_event_fixed_param *cmd;
	size_t size;

	if (!data)
		return NOTIFY_STOP_MASK;

	mac = notif_block->priv_data;
	if (!mac)
		return NOTIFY_STOP_MASK;

	size = sizeof(*cmd);
	for (i = 0; i < mac->lim.maxBssId; i++) {
		session = &mac->lim.gpSession[i];
		if (!session->valid)
			continue;
		if (pe_hang_data->offset + size > QDF_WLAN_HANG_FW_OFFSET)
			return NOTIFY_STOP_MASK;

		pe_data = (pe_hang_data->hang_data + pe_hang_data->offset);
		cmd = (struct pe_hang_event_fixed_param *)pe_data;
		QDF_HANG_EVT_SET_HDR(&cmd->tlv_header, HANG_EVT_TAG_LEGACY_MAC,
				     QDF_HANG_GET_STRUCT_TLVLEN(*cmd));
		cmd->vdev_id = session->vdev_id;
		cmd->limmlmstate = session->limMlmState;
		cmd->limprevmlmstate = session->limPrevMlmState;
		cmd->limsmestate = session->limSmeState;
		cmd->limprevsmestate = session->limPrevSmeState;
		pe_hang_data->offset += size;
	}

	return NOTIFY_OK;
}

static qdf_notif_block pe_hang_event_notifier = {
	.notif_block.notifier_call = pe_hang_event_notifier_call,
};

/** -------------------------------------------------------------
   \fn pe_open
   \brief will be called in Open sequence from mac_open
   \param   struct mac_context *mac
   \param   tHalOpenParameters *pHalOpenParam
   \return  QDF_STATUS
   -------------------------------------------------------------*/

QDF_STATUS pe_open(struct mac_context *mac, struct cds_config_info *cds_cfg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (QDF_DRIVER_TYPE_MFG == cds_cfg->driver_type)
		return QDF_STATUS_SUCCESS;

	mac->lim.maxBssId = cds_cfg->max_bssid;
	mac->lim.maxStation = cds_cfg->max_station;
	mac->lim.max_sta_of_pe_session =
			(cds_cfg->max_station > SIR_SAP_MAX_NUM_PEERS) ?
				SIR_SAP_MAX_NUM_PEERS : cds_cfg->max_station;
	qdf_spinlock_create(&mac->sys.bbt_mgmt_lock);

	if ((mac->lim.maxBssId == 0) || (mac->lim.maxStation == 0)) {
		pe_err("max number of Bssid or Stations cannot be zero!");
		return QDF_STATUS_E_FAILURE;
	}

	if (!QDF_IS_STATUS_SUCCESS(pe_allocate_dph_node_array_buffer())) {
		pe_err("g_dph_node_array memory allocate failed!");
		return QDF_STATUS_E_NOMEM;
	}

	mac->lim.lim_timers.gpLimCnfWaitTimer =
		qdf_mem_malloc(sizeof(TX_TIMER) * (mac->lim.maxStation + 1));
	if (!mac->lim.lim_timers.gpLimCnfWaitTimer) {
		status = QDF_STATUS_E_NOMEM;
		goto pe_open_timer_fail;
	}

	mac->lim.gpSession =
		qdf_mem_malloc(sizeof(struct pe_session) * mac->lim.maxBssId);
	if (!mac->lim.gpSession) {
		status = QDF_STATUS_E_NOMEM;
		goto pe_open_psession_fail;
	}

	status = lim_initialize(mac);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("lim_initialize failed!");
		status = QDF_STATUS_E_FAILURE;
		goto  pe_open_lock_fail;
	}

	/*
	 * pe_open is successful by now, so it is right time to initialize
	 * MTRACE for PE module. if LIM_TRACE_RECORD is not defined in build
	 * file then nothing will be logged for PE module.
	 */
#ifdef LIM_TRACE_RECORD
	MTRACE(lim_trace_init(mac));
#endif
	lim_register_debug_callback();
	lim_nan_register_callbacks(mac);
	p2p_register_callbacks(mac);
	lim_register_sap_bcn_callback(mac);
	if (mac->mlme_cfg->edca_params.enable_edca_params)
		lim_register_policy_mgr_callback(mac->psoc);

	if (!QDF_IS_STATUS_SUCCESS(
	    cds_shutdown_notifier_register(pe_shutdown_notifier_cb, mac))) {
		pe_err("Shutdown notifier register failed");
	}

	pe_hang_event_notifier.priv_data = mac;
	qdf_hang_event_register_notifier(&pe_hang_event_notifier);

	return status; /* status here will be QDF_STATUS_SUCCESS */

pe_open_lock_fail:
	qdf_mem_free(mac->lim.gpSession);
	mac->lim.gpSession = NULL;
pe_open_psession_fail:
	qdf_mem_free(mac->lim.lim_timers.gpLimCnfWaitTimer);
	mac->lim.lim_timers.gpLimCnfWaitTimer = NULL;
pe_open_timer_fail:
	pe_free_dph_node_array_buffer();

	return status;
}

/** -------------------------------------------------------------
   \fn pe_close
   \brief will be called in close sequence from mac_close
   \param   struct mac_context *mac
   \return  QDF_STATUS
   -------------------------------------------------------------*/

QDF_STATUS pe_close(struct mac_context *mac)
{
	uint8_t i;

	if (ANI_DRIVER_TYPE(mac) == QDF_DRIVER_TYPE_MFG)
		return QDF_STATUS_SUCCESS;

	qdf_hang_event_unregister_notifier(&pe_hang_event_notifier);
	lim_cleanup(mac);
	lim_unregister_sap_bcn_callback(mac);

	if (mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq) {
		qdf_mem_free(mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq);
		mac->lim.limDisassocDeauthCnfReq.pMlmDeauthReq = NULL;
	}

	qdf_spinlock_destroy(&mac->sys.bbt_mgmt_lock);
	for (i = 0; i < mac->lim.maxBssId; i++) {
		if (mac->lim.gpSession[i].valid == true)
			pe_delete_session(mac, &mac->lim.gpSession[i]);
	}
	qdf_mem_free(mac->lim.lim_timers.gpLimCnfWaitTimer);
	mac->lim.lim_timers.gpLimCnfWaitTimer = NULL;

	qdf_mem_free(mac->lim.gpSession);
	mac->lim.gpSession = NULL;

	pe_free_dph_node_array_buffer();

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn pe_start
   \brief will be called in start sequence from mac_start
   \param   struct mac_context *mac
   \return QDF_STATUS_SUCCESS on success, other QDF_STATUS on error
   -------------------------------------------------------------*/

QDF_STATUS pe_start(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	status = lim_start(mac);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("lim_start failed!");
		return status;
	}
	/* Initialize the configurations needed by PE */
	if (QDF_STATUS_E_FAILURE == __lim_init_config(mac)) {
		pe_err("lim init config failed!");
		/* We need to undo everything in lim_start */
		lim_cleanup_mlm(mac);
		return QDF_STATUS_E_FAILURE;
	}

	return status;
}

/** -------------------------------------------------------------
   \fn pe_stop
   \brief will be called in stop sequence from mac_stop
   \param   struct mac_context *mac
   \return none
   -------------------------------------------------------------*/

void pe_stop(struct mac_context *mac)
{
	lim_cleanup_mlm(mac);
	pe_debug(" PE STOP: Set LIM state to eLIM_MLM_OFFLINE_STATE");
	SET_LIM_MLM_STATE(mac, eLIM_MLM_OFFLINE_STATE);
	return;
}

static void pe_free_nested_messages(struct scheduler_msg *msg)
{
	switch (msg->type) {
	default:
		break;
	}
}

/** -------------------------------------------------------------
   \fn pe_free_msg
   \brief Called by CDS scheduler (function cds_sched_flush_mc_mqs)
 \      to free a given PE message on the TX and MC thread.
 \      This happens when there are messages pending in the PE
 \      queue when system is being stopped and reset.
   \param   struct mac_context *mac
   \param   struct scheduler_msg       pMsg
   \return none
   -----------------------------------------------------------------*/
void pe_free_msg(struct mac_context *mac, struct scheduler_msg *pMsg)
{
	if (pMsg) {
		if (pMsg->bodyptr) {
			if (SIR_BB_XPORT_MGMT_MSG == pMsg->type) {
				cds_pkt_return_packet((cds_pkt_t *) pMsg->
						      bodyptr);
			} else {
				pe_free_nested_messages(pMsg);
				qdf_mem_free((void *)pMsg->bodyptr);
			}
		}
		pMsg->bodyptr = 0;
		pMsg->bodyval = 0;
		pMsg->type = 0;
	}
	return;
}

QDF_STATUS lim_post_msg_api(struct mac_context *mac, struct scheduler_msg *msg)
{
	return scheduler_post_message(QDF_MODULE_ID_PE,
				      QDF_MODULE_ID_PE,
				      QDF_MODULE_ID_PE, msg);
}

QDF_STATUS lim_post_msg_high_priority(struct mac_context *mac,
				      struct scheduler_msg *msg)
{
	return scheduler_post_msg_by_priority(QDF_MODULE_ID_PE,
					       msg, true);
}

QDF_STATUS pe_mc_process_handler(struct scheduler_msg *msg)
{
	struct mac_context *mac_ctx = cds_get_context(QDF_MODULE_ID_PE);

	if (!mac_ctx)
		return QDF_STATUS_E_FAILURE;

	if (ANI_DRIVER_TYPE(mac_ctx) == QDF_DRIVER_TYPE_MFG)
		return QDF_STATUS_SUCCESS;

	lim_message_processor(mac_ctx, msg);

	return QDF_STATUS_SUCCESS;
}

/**
 * pe_drop_pending_rx_mgmt_frames: To drop pending RX mgmt frames
 * @mac_ctx: Pointer to global MAC structure
 * @hdr: Management header
 * @cds_pkt: Packet
 *
 * This function is used to drop RX pending mgmt frames if pe mgmt queue
 * reaches threshold
 *
 * Return: QDF_STATUS_SUCCESS on success or QDF_STATUS_E_FAILURE on failure
 */
static QDF_STATUS pe_drop_pending_rx_mgmt_frames(struct mac_context *mac_ctx,
				tpSirMacMgmtHdr hdr, cds_pkt_t *cds_pkt)
{
	qdf_spin_lock(&mac_ctx->sys.bbt_mgmt_lock);
	if (mac_ctx->sys.sys_bbt_pending_mgmt_count >=
	     MGMT_RX_PACKETS_THRESHOLD) {
		qdf_spin_unlock(&mac_ctx->sys.bbt_mgmt_lock);
		pe_debug("No.of pending RX management frames reaches to threshold, dropping management frames");
		cds_pkt_return_packet(cds_pkt);
		cds_pkt = NULL;
		mac_ctx->rx_packet_drop_counter++;
		return QDF_STATUS_E_FAILURE;
	} else if (mac_ctx->sys.sys_bbt_pending_mgmt_count >
		   (MGMT_RX_PACKETS_THRESHOLD / 2)) {
		/* drop all probereq, proberesp and beacons */
		if (hdr->fc.subType == SIR_MAC_MGMT_BEACON ||
		    hdr->fc.subType == SIR_MAC_MGMT_PROBE_REQ ||
		    hdr->fc.subType == SIR_MAC_MGMT_PROBE_RSP) {
			qdf_spin_unlock(&mac_ctx->sys.bbt_mgmt_lock);
			if (!(mac_ctx->rx_packet_drop_counter % 100))
				pe_debug("No.of pending RX mgmt frames reaches 1/2 thresh, dropping frame subtype: %d rx_packet_drop_counter: %d",
					hdr->fc.subType,
					mac_ctx->rx_packet_drop_counter);
			mac_ctx->rx_packet_drop_counter++;
			cds_pkt_return_packet(cds_pkt);
			cds_pkt = NULL;
			return QDF_STATUS_E_FAILURE;
		}
	}
	mac_ctx->sys.sys_bbt_pending_mgmt_count++;
	qdf_spin_unlock(&mac_ctx->sys.bbt_mgmt_lock);
	if (mac_ctx->sys.sys_bbt_pending_mgmt_count ==
	    (MGMT_RX_PACKETS_THRESHOLD / 4)) {
		if (!(mac_ctx->rx_packet_drop_counter % 100))
			pe_debug("No.of pending RX management frames reaches to 1/4th of threshold, rx_packet_drop_counter: %d",
				mac_ctx->rx_packet_drop_counter);
			mac_ctx->rx_packet_drop_counter++;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * pe_is_ext_scan_bcn_probe_rsp - Check if the beacon or probe response
 * is from Ext or EPNO scan
 *
 * @hdr: pointer to the 802.11 header of the frame
 * @rx_pkt_info: pointer to the rx packet meta
 *
 * Checks if the beacon or probe response is from Ext Scan or EPNO scan
 *
 * Return: true or false
 */
#ifdef FEATURE_WLAN_EXTSCAN
static inline bool pe_is_ext_scan_bcn_probe_rsp(tpSirMacMgmtHdr hdr,
				uint8_t *rx_pkt_info)
{
	if ((hdr->fc.subType == SIR_MAC_MGMT_BEACON ||
	     hdr->fc.subType == SIR_MAC_MGMT_PROBE_RSP) &&
	    (WMA_IS_EXTSCAN_SCAN_SRC(rx_pkt_info) ||
	    WMA_IS_EPNO_SCAN_SRC(rx_pkt_info)))
		return true;

	return false;
}
#else
static inline bool pe_is_ext_scan_bcn_probe_rsp(tpSirMacMgmtHdr hdr,
				uint8_t *rx_pkt_info)
{
	return false;
}
#endif

/**
 * pe_filter_drop_bcn_probe_frame - Apply filter on the received frame
 *
 * @mac_ctx: pointer to the global mac context
 * @hdr: pointer to the 802.11 header of the frame
 * @rx_pkt_info: pointer to the rx packet meta
 *
 * Applies the filter from global mac context on the received beacon/
 * probe response frame before posting it to the PE queue
 *
 * Return: true if frame is allowed, false if frame is to be dropped.
 */
static bool pe_filter_bcn_probe_frame(struct mac_context *mac_ctx,
					tpSirMacMgmtHdr hdr,
					uint8_t *rx_pkt_info)
{
	uint8_t session_id;
	struct mgmt_beacon_probe_filter *filter;

	if (pe_is_ext_scan_bcn_probe_rsp(hdr, rx_pkt_info))
		return true;

	filter = &mac_ctx->bcn_filter;

	/*
	 * If any STA session exists and beacon source matches any of the
	 * STA BSSIDs, allow the frame
	 */
	if (filter->num_sta_sessions) {
		for (session_id = 0; session_id < WLAN_MAX_VDEVS;
		     session_id++) {
			if (sir_compare_mac_addr(filter->sta_bssid[session_id],
			    hdr->bssId)) {
				return true;
			}
		}
	}

	return false;
}

static QDF_STATUS pe_handle_probe_req_frames(struct mac_context *mac_ctx,
					cds_pkt_t *pkt)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	uint32_t scan_queue_size = 0;

	/* Check if the probe request frame can be posted in the scan queue */
	status = scheduler_get_queue_size(QDF_MODULE_ID_SCAN, &scan_queue_size);
	if (!QDF_IS_STATUS_SUCCESS(status) ||
	    scan_queue_size > MAX_BCN_PROBE_IN_SCAN_QUEUE) {
		pe_debug_rl("Dropping probe req frame, queue size %d",
			    scan_queue_size);
		return QDF_STATUS_E_FAILURE;
	}

	/* Forward to MAC via mesg = SIR_BB_XPORT_MGMT_MSG */
	msg.type = SIR_BB_XPORT_MGMT_MSG;
	msg.bodyptr = pkt;
	msg.bodyval = 0;
	msg.callback = pe_mc_process_handler;

	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_SCAN, &msg);

	return status;
}

/* --------------------------------------------------------------------------- */
/**
 * pe_handle_mgmt_frame() - Process the Management frames from TXRX
 * @psoc: psoc context
 * @peer: peer
 * @buf: buffer
 * @mgmt_rx_params; rx event params
 * @frm_type: frame type
 *
 * This function handles the mgmt rx frame from mgmt txrx component and forms
 * a cds packet and schedule it in controller thread for further processing.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS pe_handle_mgmt_frame(struct wlan_objmgr_psoc *psoc,
			struct wlan_objmgr_peer *peer, qdf_nbuf_t buf,
			struct mgmt_rx_event_params *mgmt_rx_params,
			enum mgmt_frame_type frm_type)
{
	struct mac_context *mac;
	tpSirMacMgmtHdr mHdr;
	struct scheduler_msg msg = {0};
	cds_pkt_t *pVosPkt;
	QDF_STATUS qdf_status;
	uint8_t *pRxPacketInfo;
	int ret;

	/* skip offload packets */
	if ((ucfg_pkt_capture_get_mode(psoc) != PACKET_CAPTURE_MODE_DISABLE) &&
	    mgmt_rx_params->status & WMI_RX_OFFLOAD_MON_MODE) {
		qdf_nbuf_free(buf);
		return QDF_STATUS_SUCCESS;
	}

	mac = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac) {
		/* cannot log a failure without a valid mac */
		qdf_nbuf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	pVosPkt = qdf_mem_malloc_atomic(sizeof(*pVosPkt));
	if (!pVosPkt) {
		qdf_nbuf_free(buf);
		return QDF_STATUS_E_NOMEM;
	}

	ret = wma_form_rx_packet(buf, mgmt_rx_params, pVosPkt);
	if (ret) {
		pe_debug_rl("Failed to fill cds packet from event buffer");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_status =
		wma_ds_peek_rx_packet_info(pVosPkt, (void *)&pRxPacketInfo, false);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		cds_pkt_return_packet(pVosPkt);
		pVosPkt = NULL;
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * The MPDU header is now present at a certain "offset" in
	 * the BD and is specified in the BD itself
	 */

	mHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);

	/*
	 * Filter the beacon/probe response frames before posting it
	 * on the PE queue
	 */
	if ((mHdr->fc.subType == SIR_MAC_MGMT_BEACON ||
	    mHdr->fc.subType == SIR_MAC_MGMT_PROBE_RSP) &&
	    !pe_filter_bcn_probe_frame(mac, mHdr, pRxPacketInfo)) {
		cds_pkt_return_packet(pVosPkt);
		pVosPkt = NULL;
		return QDF_STATUS_SUCCESS;
	}

	/*
	 * Post Probe Req frames to Scan queue and return
	 */
	if (mHdr->fc.subType == SIR_MAC_MGMT_PROBE_REQ) {
		qdf_status = pe_handle_probe_req_frames(mac, pVosPkt);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			cds_pkt_return_packet(pVosPkt);
			pVosPkt = NULL;
		}
		return qdf_status;
	}

	if (QDF_STATUS_SUCCESS !=
	    pe_drop_pending_rx_mgmt_frames(mac, mHdr, pVosPkt))
		return QDF_STATUS_E_FAILURE;

	/* Forward to MAC via mesg = SIR_BB_XPORT_MGMT_MSG */
	msg.type = SIR_BB_XPORT_MGMT_MSG;
	msg.bodyptr = pVosPkt;
	msg.bodyval = 0;

	if (QDF_STATUS_SUCCESS != sys_bbt_process_message_core(mac,
							 &msg,
							 mHdr->fc.type,
							 mHdr->fc.subType)) {
		cds_pkt_return_packet(pVosPkt);
		pVosPkt = NULL;
		/*
		 * Decrement sys_bbt_pending_mgmt_count if packet
		 * is dropped before posting to LIM
		 */
		lim_decrement_pending_mgmt_count(mac);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void pe_register_mgmt_rx_frm_callback(struct mac_context *mac_ctx)
{
	QDF_STATUS status;
	struct mgmt_txrx_mgmt_frame_cb_info frm_cb_info;

	frm_cb_info.frm_type = MGMT_FRAME_TYPE_ALL;
	frm_cb_info.mgmt_rx_cb = pe_handle_mgmt_frame;

	status = wlan_mgmt_txrx_register_rx_cb(mac_ctx->psoc,
					 WLAN_UMAC_COMP_MLME, &frm_cb_info, 1);
	if (status != QDF_STATUS_SUCCESS)
		pe_err("Registering the PE Handle with MGMT TXRX layer has failed");

	wma_register_mgmt_frm_client();
}

void pe_deregister_mgmt_rx_frm_callback(struct mac_context *mac_ctx)
{
	QDF_STATUS status;
	struct mgmt_txrx_mgmt_frame_cb_info frm_cb_info;

	frm_cb_info.frm_type = MGMT_FRAME_TYPE_ALL;
	frm_cb_info.mgmt_rx_cb = pe_handle_mgmt_frame;

	status = wlan_mgmt_txrx_deregister_rx_cb(mac_ctx->psoc,
					 WLAN_UMAC_COMP_MLME, &frm_cb_info, 1);
	if (status != QDF_STATUS_SUCCESS)
		pe_err("Deregistering the PE Handle with MGMT TXRX layer has failed");

	wma_de_register_mgmt_frm_client();
}


/**
 * pe_register_callbacks_with_wma() - register SME and PE callback functions to
 * WMA.
 * (function documentation in lim_api.h)
 */
void pe_register_callbacks_with_wma(struct mac_context *mac,
				    struct sme_ready_req *ready_req)
{
	QDF_STATUS status;

	status = wma_register_roaming_callbacks(
			ready_req->csr_roam_synch_cb,
			ready_req->csr_roam_auth_event_handle_cb,
			ready_req->pe_roam_synch_cb,
			ready_req->pe_disconnect_cb,
			ready_req->csr_roam_pmkid_req_cb);
	if (status != QDF_STATUS_SUCCESS)
		pe_err("Registering roaming callbacks with WMA failed");
}

void
lim_received_hb_handler(struct mac_context *mac, uint32_t chan_freq,
			struct pe_session *pe_session)
{
	if (chan_freq == 0 || chan_freq == pe_session->curr_op_freq)
		pe_session->LimRxedBeaconCntDuringHB++;

	pe_session->pmmOffloadInfo.bcnmiss = false;
} /*** lim_init_wds_info_params() ***/

/** -------------------------------------------------------------
   \fn lim_update_overlap_sta_param
   \brief Updates overlap cache and param data structure
   \param      struct mac_context *   mac
   \param      tSirMacAddr bssId
   \param      tpLimProtStaParams pStaParams
   \return      None
   -------------------------------------------------------------*/
void
lim_update_overlap_sta_param(struct mac_context *mac, tSirMacAddr bssId,
			     tpLimProtStaParams pStaParams)
{
	int i;

	if (!pStaParams->numSta) {
		qdf_mem_copy(mac->lim.protStaOverlapCache[0].addr,
			     bssId, sizeof(tSirMacAddr));
		mac->lim.protStaOverlapCache[0].active = true;

		pStaParams->numSta = 1;

		return;
	}

	for (i = 0; i < LIM_PROT_STA_OVERLAP_CACHE_SIZE; i++) {
		if (mac->lim.protStaOverlapCache[i].active) {
			if (!qdf_mem_cmp
				    (mac->lim.protStaOverlapCache[i].addr, bssId,
				    sizeof(tSirMacAddr))) {
				return;
			}
		} else
			break;
	}

	if (i == LIM_PROT_STA_OVERLAP_CACHE_SIZE) {
		pe_debug("Overlap cache is full");
	} else {
		qdf_mem_copy(mac->lim.protStaOverlapCache[i].addr,
			     bssId, sizeof(tSirMacAddr));
		mac->lim.protStaOverlapCache[i].active = true;

		pStaParams->numSta++;
	}
}

/**
 * lim_enc_type_matched() - matches security type of incoming beracon with
 * current
 * @mac_ctx      Pointer to Global MAC structure
 * @bcn          Pointer to parsed Beacon structure
 * @session      PE session entry
 *
 * This function matches security type of incoming beracon with current
 *
 * @return true if matched, false otherwise
 */
static bool
lim_enc_type_matched(struct mac_context *mac_ctx,
		     tpSchBeaconStruct bcn,
		     struct pe_session *session)
{
	if (!bcn || !session)
		return false;

	/*
	 * This is handled by sending probe req due to IOT issues so
	 * return TRUE
	 */
	if ((bcn->capabilityInfo.privacy) !=
		SIR_MAC_GET_PRIVACY(session->limCurrentBssCaps)) {
		pe_warn("Privacy bit miss match");
		return true;
	}

	/* Open */
	if ((bcn->capabilityInfo.privacy == 0) &&
	    (session->encryptType == eSIR_ED_NONE))
		return true;

	/* WEP */
	if ((bcn->capabilityInfo.privacy == 1) &&
	    (bcn->wpaPresent == 0) && (bcn->rsnPresent == 0) &&
	    ((session->encryptType == eSIR_ED_WEP40) ||
		(session->encryptType == eSIR_ED_WEP104)
#ifdef FEATURE_WLAN_WAPI
		|| (session->encryptType == eSIR_ED_WPI)
#endif
	    ))
		return true;

	/* WPA OR RSN*/
	if ((bcn->capabilityInfo.privacy == 1) &&
	    ((bcn->wpaPresent == 1) || (bcn->rsnPresent == 1)) &&
	    ((session->encryptType == eSIR_ED_TKIP) ||
		(session->encryptType == eSIR_ED_CCMP) ||
		(session->encryptType == eSIR_ED_GCMP) ||
		(session->encryptType == eSIR_ED_GCMP_256) ||
		(session->encryptType == eSIR_ED_AES_128_CMAC)))
		return true;

	/*
	 * For HS2.0, RSN ie is not present
	 * in beacon. Therefore no need to
	 * check for security type in case
	 * OSEN session.
	 * For WPS registration session no need to detect
	 * detect security mismatch as it wont match and
	 * driver may end up sending probe request without
	 * WPS IE during WPS registration process.
	 */
	if (session->isOSENConnection ||
	   session->wps_registration)
		return true;

	pe_debug("AP:: Privacy %d WPA %d RSN %d, SELF:: Privacy %d Enc %d OSEN %d WPS %d",
		 bcn->capabilityInfo.privacy, bcn->wpaPresent, bcn->rsnPresent,
		 SIR_MAC_GET_PRIVACY(session->limCurrentBssCaps),
		 session->encryptType, session->isOSENConnection,
		 session->wps_registration);

	return false;
}

/**
 * lim_detect_change_in_ap_capabilities()
 *
 ***FUNCTION:
 * This function is called while SCH is processing
 * received Beacon from AP on STA to detect any
 * change in AP's capabilities. If there any change
 * is detected, Roaming is informed of such change
 * so that it can trigger reassociation.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 * Notification is enabled for STA product only since
 * it is not a requirement on BP side.
 *
 * @param  mac      Pointer to Global MAC structure
 * @param  pBeacon   Pointer to parsed Beacon structure
 * @return None
 */

void
lim_detect_change_in_ap_capabilities(struct mac_context *mac,
				     tpSirProbeRespBeacon pBeacon,
				     struct pe_session *pe_session)
{
	uint8_t len;
	struct ap_new_caps apNewCaps;
	uint32_t new_chan_freq;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool security_caps_matched = true;

	apNewCaps.capabilityInfo =
		lim_get_u16((uint8_t *) &pBeacon->capabilityInfo);
	new_chan_freq = pBeacon->chan_freq;

	security_caps_matched = lim_enc_type_matched(mac, pBeacon,
						     pe_session);
	if ((false == pe_session->limSentCapsChangeNtf) &&
	    (((!lim_is_null_ssid(&pBeacon->ssId)) &&
	       lim_cmp_ssid(&pBeacon->ssId, pe_session)) ||
	     ((SIR_MAC_GET_ESS(apNewCaps.capabilityInfo) !=
	       SIR_MAC_GET_ESS(pe_session->limCurrentBssCaps)) ||
	      (SIR_MAC_GET_PRIVACY(apNewCaps.capabilityInfo) !=
	       SIR_MAC_GET_PRIVACY(pe_session->limCurrentBssCaps)) ||
	      (SIR_MAC_GET_QOS(apNewCaps.capabilityInfo) !=
	       SIR_MAC_GET_QOS(pe_session->limCurrentBssCaps)) ||
	      ((new_chan_freq != pe_session->curr_op_freq) &&
		(new_chan_freq != 0)) ||
	      (false == security_caps_matched)
	     ))) {
		if (false == pe_session->fWaitForProbeRsp) {
			/* If Beacon capabilities is not matching with the current capability,
			 * then send unicast probe request to AP and take decision after
			 * receiving probe response */
			if (true == pe_session->fIgnoreCapsChange) {
				pe_debug("Ignoring the Capability change as it is false alarm");
				return;
			}
			pe_session->fWaitForProbeRsp = true;
			pe_warn("AP capabilities are not matching, sending directed probe request");
			status =
				lim_send_probe_req_mgmt_frame(
					mac, &pe_session->ssId,
					pe_session->bssId,
					pe_session->curr_op_freq,
					pe_session->self_mac_addr,
					pe_session->dot11mode,
					NULL, NULL);

			if (QDF_STATUS_SUCCESS != status) {
				pe_err("send ProbeReq failed");
				pe_session->fWaitForProbeRsp = false;
			}
			return;
		}
		/**
		 * BSS capabilities have changed.
		 * Inform Roaming.
		 */
		len = sizeof(tSirMacCapabilityInfo) + sizeof(tSirMacAddr) + sizeof(uint8_t) + 3 * sizeof(uint8_t) + /* reserved fields */
		      pBeacon->ssId.length + 1;

		qdf_mem_copy(apNewCaps.bssId.bytes,
			     pe_session->bssId, QDF_MAC_ADDR_SIZE);
		if (new_chan_freq != pe_session->curr_op_freq) {
			pe_err("Channel freq Change from %d --> %d Ignoring beacon!",
			       pe_session->curr_op_freq, new_chan_freq);
			return;
		}

		/**
		 * When Cisco 1262 Enterprise APs are configured with WPA2-PSK with
		 * AES+TKIP Pairwise ciphers and WEP-40 Group cipher, they do not set
		 * the privacy bit in Beacons (wpa/rsnie is still present in beacons),
		 * the privacy bit is set in Probe and association responses.
		 * Due to this anomaly, we detect a change in
		 * AP capabilities when we receive a beacon after association and
		 * disconnect from the AP. The following check makes sure that we can
		 * connect to such APs
		 */
		else if ((SIR_MAC_GET_PRIVACY(apNewCaps.capabilityInfo) == 0) &&
			 (pBeacon->rsnPresent || pBeacon->wpaPresent)) {
			pe_err("BSS Caps (Privacy) bit 0 in beacon, but WPA or RSN IE present, Ignore Beacon!");
			return;
		}
		qdf_mem_copy((uint8_t *) &apNewCaps.ssId,
			     (uint8_t *) &pBeacon->ssId,
			     pBeacon->ssId.length + 1);

		pe_session->fIgnoreCapsChange = false;
		pe_session->fWaitForProbeRsp = false;
		pe_session->limSentCapsChangeNtf = true;
		lim_send_sme_wm_status_change_ntf(mac, eSIR_SME_AP_CAPS_CHANGED,
						  (uint32_t *) &apNewCaps,
						  len, pe_session->smeSessionId);
	} else if (true == pe_session->fWaitForProbeRsp) {
		/* Only for probe response frames and matching capabilities the control
		 * will come here. If beacon is with broadcast ssid then fWaitForProbeRsp
		 * will be false, the control will not come here*/

		pe_debug("capabilities in probe response are"
				       "matching with the current setting,"
				       "Ignoring subsequent capability"
				       "mismatch");
		pe_session->fIgnoreCapsChange = true;
		pe_session->fWaitForProbeRsp = false;
	}

} /*** lim_detect_change_in_ap_capabilities() ***/

/* --------------------------------------------------------------------- */
/**
 * lim_update_short_slot
 *
 * FUNCTION:
 * Enable/Disable short slot
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param enable        Flag to enable/disable short slot
 * @return None
 */

QDF_STATUS lim_update_short_slot(struct mac_context *mac,
				    tpSirProbeRespBeacon pBeacon,
				    tpUpdateBeaconParams pBeaconParams,
				    struct pe_session *pe_session)
{

	struct ap_new_caps apNewCaps;
	uint32_t nShortSlot;
	uint32_t phyMode;

	/* Check Admin mode first. If it is disabled just return */
	if (!mac->mlme_cfg->feature_flags.enable_short_slot_time_11g)
		return QDF_STATUS_SUCCESS;

	/* Check for 11a mode or 11b mode. In both cases return since slot time is constant and cannot/should not change in beacon */
	lim_get_phy_mode(mac, &phyMode, pe_session);
	if ((phyMode == WNI_CFG_PHY_MODE_11A)
	    || (phyMode == WNI_CFG_PHY_MODE_11B))
		return QDF_STATUS_SUCCESS;

	apNewCaps.capabilityInfo =
		lim_get_u16((uint8_t *) &pBeacon->capabilityInfo);

	/*  Earlier implementation: determine the appropriate short slot mode based on AP advertised modes */
	/* when erp is present, apply short slot always unless, prot=on  && shortSlot=off */
	/* if no erp present, use short slot based on current ap caps */

	/* Issue with earlier implementation : Cisco 1231 BG has shortSlot = 0, erpIEPresent and useProtection = 0 (Case4); */

	/* Resolution : always use the shortSlot setting the capability info to decide slot time. */
	/* The difference between the earlier implementation and the new one is only Case4. */
	/*
	   ERP IE Present  |   useProtection   |   shortSlot   =   QC STA Short Slot
	   Case1        1                                   1                       1                       1           //AP should not advertise this combination.
	   Case2        1                                   1                       0                       0
	   Case3        1                                   0                       1                       1
	   Case4        1                                   0                       0                       0
	   Case5        0                                   1                       1                       1
	   Case6        0                                   1                       0                       0
	   Case7        0                                   0                       1                       1
	   Case8        0                                   0                       0                       0
	 */
	nShortSlot = SIR_MAC_GET_SHORT_SLOT_TIME(apNewCaps.capabilityInfo);

	if (nShortSlot != pe_session->shortSlotTimeSupported) {
		/* Short slot time capability of AP has changed. Adopt to it. */
		pe_debug("Shortslot capability of AP changed: %d",
			       nShortSlot);
			((tpSirMacCapabilityInfo) & pe_session->
			limCurrentBssCaps)->shortSlotTime = (uint16_t) nShortSlot;
		pe_session->shortSlotTimeSupported = nShortSlot;
		pBeaconParams->fShortSlotTime = (uint8_t) nShortSlot;
		pBeaconParams->paramChangeBitmap |=
			PARAM_SHORT_SLOT_TIME_CHANGED;
	}
	return QDF_STATUS_SUCCESS;
}


void lim_send_heart_beat_timeout_ind(struct mac_context *mac,
				     struct pe_session *pe_session)
{
	QDF_STATUS status;
	struct scheduler_msg msg = {0};

	/* Prepare and post message to LIM Message Queue */
	msg.type = (uint16_t) SIR_LIM_HEART_BEAT_TIMEOUT;
	msg.bodyptr = pe_session;
	msg.bodyval = 0;
	pe_err("Heartbeat failure from Fw");

	status = lim_post_msg_api(mac, &msg);

	if (status != QDF_STATUS_SUCCESS) {
		pe_err("posting message: %X to LIM failed, reason: %d",
			msg.type, status);
	}
}

void lim_ps_offload_handle_missed_beacon_ind(struct mac_context *mac,
					     struct scheduler_msg *msg)
{
	struct missed_beacon_ind *missed_beacon_ind = msg->bodyptr;
	struct pe_session *pe_session =
		pe_find_session_by_vdev_id(mac, missed_beacon_ind->bss_idx);

	if (!pe_session) {
		pe_err("session does not exist for vdev_id %d",
			missed_beacon_ind->bss_idx);
		return;
	}

	/* Set Beacon Miss in Powersave Offload */
	pe_session->pmmOffloadInfo.bcnmiss = true;
	pe_err("Received Heart Beat Failure");

	/*  Do AP probing immediately */
	lim_send_heart_beat_timeout_ind(mac, pe_session);
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * lim_fill_join_rsp_ht_caps() - Fill the HT caps in join response
 * @session: PE Session
 * @join_rsp: Join response buffer to be filled up.
 *
 * Return: None
 */
void lim_fill_join_rsp_ht_caps(struct pe_session *session,
			       struct join_rsp *join_rsp)
{
	struct ht_profile *ht_profile;

	if (!session) {
		pe_err("Invalid Session");
		return;
	}
	if (!join_rsp) {
		pe_err("Invalid Join Response");
		return;
	}

	if (session->cc_switch_mode == QDF_MCC_TO_SCC_SWITCH_DISABLE)
		return;

	ht_profile = &join_rsp->ht_profile;
	ht_profile->htSupportedChannelWidthSet =
		session->htSupportedChannelWidthSet;
	ht_profile->htRecommendedTxWidthSet =
		session->htRecommendedTxWidthSet;
	ht_profile->htSecondaryChannelOffset =
		session->htSecondaryChannelOffset;
	ht_profile->dot11mode = session->dot11mode;
	ht_profile->htCapability = session->htCapability;
	ht_profile->vhtCapability = session->vhtCapability;
	ht_profile->apCenterChan = session->ch_center_freq_seg0;
	ht_profile->apChanWidth = session->ch_width;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifdef WLAN_FEATURE_11W
static void pe_set_rmf_caps(struct mac_context *mac_ctx,
			    struct pe_session *ft_session,
			    struct roam_offload_synch_ind *roam_synch)
{
	uint8_t *assoc_body;
	uint16_t len;
	tDot11fReAssocRequest *assoc_req;
	uint32_t status;
	tSirMacRsnInfo rsn_ie;
	uint32_t value = WPA_TYPE_OUI;

	assoc_body = (uint8_t *)roam_synch + roam_synch->reassoc_req_offset +
			sizeof(tSirMacMgmtHdr);
	len = roam_synch->reassoc_req_length - sizeof(tSirMacMgmtHdr);

	assoc_req = qdf_mem_malloc(sizeof(*assoc_req));
	if (!assoc_req)
		return;

	/* delegate to the framesc-generated code, */
	status = dot11f_unpack_re_assoc_request(mac_ctx, assoc_body, len,
						assoc_req, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse a Re-association Request (0x%08x, %d bytes):",
		       status, len);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_INFO,
				   assoc_body, len);
		qdf_mem_free(assoc_req);
		return;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("There were warnings while unpacking a Re-association Request (0x%08x, %d bytes):",
			 status, len);
	}
	ft_session->limRmfEnabled = false;
	if (!assoc_req->RSNOpaque.present && !assoc_req->WPAOpaque.present) {
		qdf_mem_free(assoc_req);
		return;
	}

	if (assoc_req->RSNOpaque.present) {
		rsn_ie.info[0] = WLAN_ELEMID_RSN;
		rsn_ie.info[1] = assoc_req->RSNOpaque.num_data;

		rsn_ie.length = assoc_req->RSNOpaque.num_data + 2;
		qdf_mem_copy(&rsn_ie.info[2], assoc_req->RSNOpaque.data,
			     assoc_req->RSNOpaque.num_data);
	} else if (assoc_req->WPAOpaque.present) {
		rsn_ie.info[0] = WLAN_ELEMID_VENDOR;
		rsn_ie.info[1] = WLAN_OUI_SIZE + assoc_req->WPAOpaque.num_data;

		rsn_ie.length = WLAN_OUI_SIZE +
				assoc_req->WPAOpaque.num_data + 2;

		qdf_mem_copy(&rsn_ie.info[2], (uint8_t *)&value, WLAN_OUI_SIZE);
		qdf_mem_copy(&rsn_ie.info[WLAN_OUI_SIZE + 2],
			     assoc_req->WPAOpaque.data,
			     assoc_req->WPAOpaque.num_data);
	}

	qdf_mem_free(assoc_req);
	wlan_set_vdev_crypto_prarams_from_ie(ft_session->vdev, rsn_ie.info,
					     rsn_ie.length);

	ft_session->limRmfEnabled =
		lim_get_vdev_rmf_capable(mac_ctx, ft_session);
}
#else
static inline void pe_set_rmf_caps(struct mac_context *mac_ctx,
				   struct pe_session *ft_session,
				   struct roam_offload_synch_ind *roam_synch)
{
}
#endif

/**
 * sir_parse_bcn_fixed_fields() - Parse fixed fields in Beacon IE's
 *
 * @mac_ctx: MAC Context
 * @beacon_struct: Beacon/Probe Response structure
 * @buf: Fixed Fields buffer
 */
static void sir_parse_bcn_fixed_fields(struct mac_context *mac_ctx,
					tpSirProbeRespBeacon beacon_struct,
					uint8_t *buf)
{
	tDot11fFfCapabilities dst;

	beacon_struct->timeStamp[0] = lim_get_u32(buf);
	beacon_struct->timeStamp[1] = lim_get_u32(buf + 4);
	buf += 8;

	beacon_struct->beaconInterval = lim_get_u16(buf);
	buf += 2;

	dot11f_unpack_ff_capabilities(mac_ctx, buf, &dst);

	sir_copy_caps_info(mac_ctx, dst, beacon_struct);
}

static QDF_STATUS
lim_roam_gen_mbssid_beacon(struct mac_context *mac,
			   struct roam_offload_synch_ind *roam_ind,
			   tpSirProbeRespBeacon parsed_frm,
			   uint8_t **ie, uint32_t *ie_len)
{
	qdf_list_t *scan_list;
	struct mgmt_rx_event_params rx_param;
	uint8_t list_count = 0, i;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	qdf_list_node_t *next_node = NULL, *cur_node = NULL;
	struct scan_cache_node *scan_node;
	struct scan_cache_entry *scan_entry;
	uint8_t *bcn_prb_ptr;
	uint32_t nontx_bcn_prbrsp_len = 0, offset, length;
	uint8_t *nontx_bcn_prbrsp = NULL;
	uint8_t ie_offset;

	ie_offset = SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_SSID_OFFSET;
	bcn_prb_ptr = (uint8_t *)roam_ind +
				roam_ind->beaconProbeRespOffset;

	rx_param.chan_freq = roam_ind->chan_freq;
	rx_param.pdev_id = wlan_objmgr_pdev_get_pdev_id(mac->pdev);
	rx_param.rssi = roam_ind->rssi;

	scan_list = util_scan_unpack_beacon_frame(mac->pdev, bcn_prb_ptr,
						  roam_ind->beaconProbeRespLength,
						  MGMT_SUBTYPE_BEACON, &rx_param);
	if (!scan_list) {
		pe_err("failed to parse");
		return QDF_STATUS_E_FAILURE;
	}

	list_count = qdf_list_size(scan_list);
	status = qdf_list_peek_front(scan_list, &cur_node);
	if (QDF_IS_STATUS_ERROR(status) || !cur_node) {
		pe_debug("list peek front failure. list size %d", list_count);
		goto error;
	}

	for (i = 1; i < list_count; i++) {
		scan_node = qdf_container_of(cur_node,
					     struct scan_cache_node, node);
		scan_entry = scan_node->entry;
		if (qdf_is_macaddr_equal(&roam_ind->bssid,
					 &scan_entry->bssid)) {
			pe_debug("matched BSSID "QDF_MAC_ADDR_FMT" bcn len %d profiles %d",
				 QDF_MAC_ADDR_REF(scan_entry->bssid.bytes),
				 scan_entry->raw_frame.len,
				 list_count);
			nontx_bcn_prbrsp = scan_entry->raw_frame.ptr;
			nontx_bcn_prbrsp_len = scan_entry->raw_frame.len;
			QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE,
					   QDF_TRACE_LEVEL_DEBUG,
					   scan_entry->raw_frame.ptr,
					   nontx_bcn_prbrsp_len);
			break;
		}
		status = qdf_list_peek_next(scan_list, cur_node, &next_node);
		if (QDF_IS_STATUS_ERROR(status) || !next_node) {
			pe_debug("list remove failure i:%d, lsize:%d",
				 i, list_count);
			goto error;
		}
		cur_node = next_node;
	}

	if (!nontx_bcn_prbrsp_len) {
		pe_debug("failed to generate/find MBSSID beacon");
		goto error;
	}

	if (roam_ind->isBeacon) {
		offset = SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_SSID_OFFSET;
		length = nontx_bcn_prbrsp_len - SIR_MAC_HDR_LEN_3A;
		if (sir_parse_beacon_ie(mac, parsed_frm,
					&nontx_bcn_prbrsp[offset],
					length) != QDF_STATUS_SUCCESS ||
			!parsed_frm->ssidPresent) {
			pe_err("Parse error Beacon, length: %d",
			       roam_ind->beaconProbeRespLength);
			status =  QDF_STATUS_E_FAILURE;
			goto error;
		}
	} else {
		offset = SIR_MAC_HDR_LEN_3A;
		length = nontx_bcn_prbrsp_len - SIR_MAC_HDR_LEN_3A;
		if (sir_convert_probe_frame2_struct(mac,
					    &nontx_bcn_prbrsp[offset],
					    length,
					    parsed_frm) != QDF_STATUS_SUCCESS ||
			!parsed_frm->ssidPresent) {
			pe_err("Parse error ProbeResponse, length: %d",
			       roam_ind->beaconProbeRespLength);
			status = QDF_STATUS_E_FAILURE;
			goto error;
		}
	}

	*ie_len = nontx_bcn_prbrsp_len - ie_offset;
	if (*ie_len) {
		*ie = qdf_mem_malloc(*ie_len);
		if (!*ie)
			return QDF_STATUS_E_NOMEM;
		qdf_mem_copy(*ie, nontx_bcn_prbrsp + ie_offset, *ie_len);
		pe_debug("beacon/probe Ie length: %d", *ie_len);
	}
error:
	for (i = 0; i < list_count; i++) {
		status = qdf_list_remove_front(scan_list, &next_node);
		if (QDF_IS_STATUS_ERROR(status) || !next_node) {
			pe_debug("list remove failure i:%d, lsize:%d",
				 i, list_count);
			break;
		}
		scan_node = qdf_container_of(next_node,
					     struct scan_cache_node, node);
		util_scan_free_cache_entry(scan_node->entry);
		qdf_mem_free(scan_node);
	}
	qdf_mem_free(scan_list);

	return status;
}

static QDF_STATUS
lim_roam_gen_beacon_descr(struct mac_context *mac,
			  struct roam_offload_synch_ind *roam_ind,
			  tpSirProbeRespBeacon parsed_frm,
			  uint8_t **ie, uint32_t *ie_len)
{
	QDF_STATUS status;
	uint8_t *bcn_prb_ptr;
	tpSirMacMgmtHdr mac_hdr;
	uint8_t ie_offset;

	bcn_prb_ptr = (uint8_t *)roam_ind +
		roam_ind->beaconProbeRespOffset;
	mac_hdr = (tpSirMacMgmtHdr)bcn_prb_ptr;
	ie_offset = SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_SSID_OFFSET;

	if (qdf_is_macaddr_zero((struct qdf_mac_addr *)mac_hdr->bssId)) {
		pe_debug("bssid is 0 in beacon/probe update it with bssId "QDF_MAC_ADDR_FMT" in sync ind",
			QDF_MAC_ADDR_REF(roam_ind->bssid.bytes));
		qdf_mem_copy(mac_hdr->bssId, roam_ind->bssid.bytes,
			     sizeof(tSirMacAddr));
	}

	if (qdf_mem_cmp(&roam_ind->bssid.bytes,
			&mac_hdr->bssId, QDF_MAC_ADDR_SIZE) != 0) {
		pe_debug("LFR3:MBSSID Beacon/Prb Rsp: %d bssid "QDF_MAC_ADDR_FMT,
			 roam_ind->isBeacon,
			 QDF_MAC_ADDR_REF(mac_hdr->bssId));
		/*
		 * Its a MBSSID non-tx BSS roaming scenario.
		 * Generate non tx BSS beacon/probe response
		 */
		status = lim_roam_gen_mbssid_beacon(mac,
						    roam_ind,
						    parsed_frm,
						    ie, ie_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("failed to gen mbssid beacon");
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		if (roam_ind->isBeacon) {
			if (sir_parse_beacon_ie(mac, parsed_frm,
				&bcn_prb_ptr[SIR_MAC_HDR_LEN_3A +
				SIR_MAC_B_PR_SSID_OFFSET],
				roam_ind->beaconProbeRespLength -
				SIR_MAC_HDR_LEN_3A) != QDF_STATUS_SUCCESS ||
				!parsed_frm->ssidPresent) {
				pe_err("Parse error Beacon, length: %d",
				       roam_ind->beaconProbeRespLength);
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			if (sir_convert_probe_frame2_struct(mac,
				&bcn_prb_ptr[SIR_MAC_HDR_LEN_3A],
				roam_ind->beaconProbeRespLength -
				SIR_MAC_HDR_LEN_3A, parsed_frm) !=
				QDF_STATUS_SUCCESS ||
				!parsed_frm->ssidPresent) {
				pe_err("Parse error ProbeResponse, length: %d",
				       roam_ind->beaconProbeRespLength);
				return QDF_STATUS_E_FAILURE;
			}
		}
		/* 24 byte MAC header and 12 byte to ssid IE */
		if (roam_ind->beaconProbeRespLength > ie_offset) {
			*ie_len = roam_ind->beaconProbeRespLength - ie_offset;
			*ie = qdf_mem_malloc(*ie_len);
			if (!*ie)
				return QDF_STATUS_E_NOMEM;
			qdf_mem_copy(*ie, bcn_prb_ptr + ie_offset, *ie_len);
			pe_debug("beacon/probe Ie length: %d", *ie_len);
		}
	}
	/*
	 * For probe response, unpack core parses beacon interval, capabilities,
	 * timestamp. For beacon IEs, these fields are not parsed.
	 */
	if (roam_ind->isBeacon)
		sir_parse_bcn_fixed_fields(mac, parsed_frm,
			&bcn_prb_ptr[SIR_MAC_HDR_LEN_3A]);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
lim_roam_fill_bss_descr(struct mac_context *mac,
			struct roam_offload_synch_ind *roam_synch_ind_ptr,
			struct bss_description *bss_desc_ptr)
{
	uint32_t ie_len = 0;
	tpSirProbeRespBeacon parsed_frm_ptr;
	tpSirMacMgmtHdr mac_hdr;
	uint8_t *bcn_proberesp_ptr;
	QDF_STATUS status;
	uint8_t *ie = NULL;

	bcn_proberesp_ptr = (uint8_t *)roam_synch_ind_ptr +
		roam_synch_ind_ptr->beaconProbeRespOffset;
	mac_hdr = (tpSirMacMgmtHdr)bcn_proberesp_ptr;
	parsed_frm_ptr = qdf_mem_malloc(sizeof(tSirProbeRespBeacon));
	if (!parsed_frm_ptr)
		return QDF_STATUS_E_NOMEM;

	if (roam_synch_ind_ptr->beaconProbeRespLength <=
			SIR_MAC_HDR_LEN_3A) {
		pe_err("very few bytes in synchInd beacon / probe resp frame! length: %d",
		       roam_synch_ind_ptr->beaconProbeRespLength);
		qdf_mem_free(parsed_frm_ptr);
		return QDF_STATUS_E_FAILURE;
	}
	pe_debug("LFR3:Beacon/Prb Rsp: %d len %d bssid "QDF_MAC_ADDR_FMT" beacon "QDF_MAC_ADDR_FMT,
		 roam_synch_ind_ptr->isBeacon,
		 roam_synch_ind_ptr->beaconProbeRespLength,
		 QDF_MAC_ADDR_REF(roam_synch_ind_ptr->bssid.bytes),
		 QDF_MAC_ADDR_REF(mac_hdr->bssId));
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   bcn_proberesp_ptr,
			   roam_synch_ind_ptr->beaconProbeRespLength);
	status = lim_roam_gen_beacon_descr(mac,
					   roam_synch_ind_ptr,
					   parsed_frm_ptr,
					   &ie, &ie_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Failed to parse beacon");
		qdf_mem_free(parsed_frm_ptr);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * Length of BSS desription is without length of
	 * length itself and length of pointer
	 * that holds ieFields
	 *
	 * struct bss_description
	 * +--------+---------------------------------+---------------+
	 * | length | other fields                    | pointer to IEs|
	 * +--------+---------------------------------+---------------+
	 *                                            ^
	 *                                            ieFields
	 */
	bss_desc_ptr->length = (uint16_t) (offsetof(struct bss_description,
					   ieFields[0]) -
				sizeof(bss_desc_ptr->length) + ie_len);

	bss_desc_ptr->fProbeRsp = !roam_synch_ind_ptr->isBeacon;
	bss_desc_ptr->rssi = roam_synch_ind_ptr->rssi;
	/* Copy Timestamp */
	bss_desc_ptr->scansystimensec = qdf_get_monotonic_boottime_ns();
	if (parsed_frm_ptr->he_op.oper_info_6g_present) {
		bss_desc_ptr->chan_freq = wlan_reg_chan_band_to_freq(mac->pdev,
						parsed_frm_ptr->he_op.oper_info_6g.info.primary_ch,
						BIT(REG_BAND_6G));
	} else if (parsed_frm_ptr->dsParamsPresent) {
		bss_desc_ptr->chan_freq = parsed_frm_ptr->chan_freq;
	} else if (parsed_frm_ptr->HTInfo.present) {
		bss_desc_ptr->chan_freq =
			wlan_reg_legacy_chan_to_freq(mac->pdev,
						     parsed_frm_ptr->HTInfo.primaryChannel);
	} else {
		/*
		 * If DS Params or HTIE is not present in the probe resp or
		 * beacon, then use the channel frequency provided by firmware
		 * to fill the channel in the BSS descriptor.*/
		bss_desc_ptr->chan_freq = roam_synch_ind_ptr->chan_freq;
	}

	bss_desc_ptr->nwType = lim_get_nw_type(
			mac,
			bss_desc_ptr->chan_freq,
			SIR_MAC_MGMT_FRAME,
			parsed_frm_ptr);

	bss_desc_ptr->sinr = 0;
	bss_desc_ptr->beaconInterval = parsed_frm_ptr->beaconInterval;
	bss_desc_ptr->timeStamp[0]   = parsed_frm_ptr->timeStamp[0];
	bss_desc_ptr->timeStamp[1]   = parsed_frm_ptr->timeStamp[1];
	qdf_mem_copy(&bss_desc_ptr->capabilityInfo,
	&bcn_proberesp_ptr[SIR_MAC_HDR_LEN_3A + SIR_MAC_B_PR_CAPAB_OFFSET], 2);

	qdf_mem_copy((uint8_t *) &bss_desc_ptr->bssId,
		     (uint8_t *)roam_synch_ind_ptr->bssid.bytes,
		     sizeof(tSirMacAddr));

	qdf_mem_copy((uint8_t *)&bss_desc_ptr->seq_ctrl,
		     (uint8_t *)&mac_hdr->seqControl,
		     sizeof(tSirMacSeqCtl));

	bss_desc_ptr->received_time =
		      (uint64_t)qdf_mc_timer_get_system_time();
	if (parsed_frm_ptr->mdiePresent) {
		bss_desc_ptr->mdiePresent = parsed_frm_ptr->mdiePresent;
		qdf_mem_copy((uint8_t *)bss_desc_ptr->mdie,
				(uint8_t *)parsed_frm_ptr->mdie,
				SIR_MDIE_SIZE);
	}
	pe_debug("chan: %d rssi: %d desc len %d",
		 bss_desc_ptr->chan_freq,
		 bss_desc_ptr->rssi,
		 bss_desc_ptr->length);

	qdf_mem_free(parsed_frm_ptr);
	if (ie_len) {
		qdf_mem_copy(&bss_desc_ptr->ieFields,
			     ie, ie_len);
		qdf_mem_free(ie);
	} else {
		pe_err("Beacon/Probe rsp doesn't have any IEs");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_FEATURE_FILS_SK)
/**
 * lim_copy_and_free_hlp_data_from_session - Copy HLP info
 * @session_ptr: PE session
 * @roam_sync_ind_ptr: Roam Synch Indication pointer
 *
 * This API is used to copy the parsed HLP info from PE session
 * to roam synch indication data. THe HLP info is expected to be
 * parsed/stored in PE session already from assoc IE's received
 * from fw as part of Roam Synch Indication.
 *
 * Return: None
 */
static void
lim_copy_and_free_hlp_data_from_session(struct pe_session *session_ptr,
					struct roam_offload_synch_ind
					*roam_sync_ind_ptr)
{
	if (session_ptr->fils_info->hlp_data &&
	    session_ptr->fils_info->hlp_data_len) {
		cds_copy_hlp_info(&session_ptr->fils_info->dst_mac,
				  &session_ptr->fils_info->src_mac,
				  session_ptr->fils_info->hlp_data_len,
				  session_ptr->fils_info->hlp_data,
				  &roam_sync_ind_ptr->dst_mac,
				  &roam_sync_ind_ptr->src_mac,
				  &roam_sync_ind_ptr->hlp_data_len,
				  roam_sync_ind_ptr->hlp_data);

		qdf_mem_free(session_ptr->fils_info->hlp_data);
		session_ptr->fils_info->hlp_data = NULL;
		session_ptr->fils_info->hlp_data_len = 0;
	}
}
#else
static inline void
lim_copy_and_free_hlp_data_from_session(struct pe_session *session_ptr,
					struct roam_offload_synch_ind
					*roam_sync_ind_ptr)
{}
#endif

static
uint8_t *lim_process_rmf_disconnect_frame(struct mac_context *mac,
					  struct pe_session *session,
					  uint8_t *deauth_disassoc_frame,
					  uint16_t deauth_disassoc_frame_len,
					  uint16_t *extracted_length)
{
	struct wlan_frame_hdr *mac_hdr;
	uint8_t mic_len, hdr_len, pdev_id;
	uint8_t *orig_ptr, *efrm;
	int32_t mgmtcipherset;
	uint32_t mmie_len;
	QDF_STATUS status;

	mac_hdr = (struct wlan_frame_hdr *)deauth_disassoc_frame;
	orig_ptr = (uint8_t *)mac_hdr;

	if (mac_hdr->i_fc[1] & IEEE80211_FC1_WEP) {
		if (QDF_IS_ADDR_BROADCAST(mac_hdr->i_addr1) ||
		    IEEE80211_IS_MULTICAST(mac_hdr->i_addr1)) {
			pe_err("Encrypted BC/MC frame dropping the frame");
			*extracted_length = 0;
			return NULL;
		}

		pdev_id = wlan_objmgr_pdev_get_pdev_id(mac->pdev);
		status = mlme_get_peer_mic_len(mac->psoc, pdev_id,
					       mac_hdr->i_addr2, &mic_len,
					       &hdr_len);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("Failed to get mic hdr and length");
			*extracted_length = 0;
			return NULL;
		}

		if (deauth_disassoc_frame_len <
		    (sizeof(*mac_hdr) + hdr_len + mic_len)) {
			pe_err("Frame len less than expected %d",
			       deauth_disassoc_frame_len);
			*extracted_length = 0;
			return NULL;
		}

		/*
		 * Strip the privacy headers and trailer
		 * for the received deauth/disassoc frame
		 */
		qdf_mem_move(orig_ptr + hdr_len, mac_hdr,
			     sizeof(*mac_hdr));
		*extracted_length = deauth_disassoc_frame_len -
				    (hdr_len + mic_len);
		return orig_ptr + hdr_len;
	}

	if (!(QDF_IS_ADDR_BROADCAST(mac_hdr->i_addr1) ||
	      IEEE80211_IS_MULTICAST(mac_hdr->i_addr1))) {
		pe_err("Rx unprotected unicast mgmt frame");
		*extracted_length = 0;
		return NULL;
	}

	mgmtcipherset = wlan_crypto_get_param(session->vdev,
					      WLAN_CRYPTO_PARAM_MGMT_CIPHER);
	if (mgmtcipherset < 0) {
		pe_err("Invalid mgmt cipher");
		*extracted_length = 0;
		return NULL;
	}

	mmie_len = (mgmtcipherset & (1 << WLAN_CRYPTO_CIPHER_AES_CMAC) ?
		    cds_get_mmie_size() : cds_get_gmac_mmie_size());

	efrm = orig_ptr + deauth_disassoc_frame_len;
	if (!mac->pmf_offload &&
	    !wlan_crypto_is_mmie_valid(session->vdev, orig_ptr, efrm)) {
		pe_err("Invalid MMIE");
		*extracted_length = 0;
		return NULL;
	}

	*extracted_length = deauth_disassoc_frame_len - mmie_len;

	return deauth_disassoc_frame;
}

QDF_STATUS
pe_disconnect_callback(struct mac_context *mac, uint8_t vdev_id,
		       uint8_t *deauth_disassoc_frame,
		       uint16_t deauth_disassoc_frame_len,
		       uint16_t reason_code)
{
	struct pe_session *session;
	uint8_t *extracted_frm = NULL;
	uint16_t extracted_frm_len;
	bool is_pmf_connection;

	session = pe_find_session_by_vdev_id(mac, vdev_id);
	if (!session) {
		pe_err("LFR3: Vdev %d doesn't exist", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (!((session->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE) &&
	      (session->limSmeState != eLIM_SME_WT_DISASSOC_STATE) &&
	      (session->limSmeState != eLIM_SME_WT_DEAUTH_STATE))) {
		pe_info("Cannot handle in mlmstate %d sme state %d as vdev_id:%d is not in connected state",
			session->limMlmState, session->limSmeState, vdev_id);
		return QDF_STATUS_SUCCESS;
	}

	if (!(deauth_disassoc_frame ||
	      deauth_disassoc_frame_len > SIR_MAC_MIN_IE_LEN))
		goto end;

	/*
	 * Use vdev pmf status instead of peer pmf capability as
	 * the firmware might roam to new AP in powersave case and
	 * roam synch can come before emergency deauth event.
	 * In that case, get peer will fail and reason code received
	 * from the WMI_ROAM_EVENTID  will be sent to upper layers.
	 */
	is_pmf_connection = lim_get_vdev_rmf_capable(mac, session);
	if (is_pmf_connection) {
		extracted_frm = lim_process_rmf_disconnect_frame(
						mac, session,
						deauth_disassoc_frame,
						deauth_disassoc_frame_len,
						&extracted_frm_len);
		if (!extracted_frm) {
			pe_err("PMF frame validation failed");
			goto end;
		}
	} else {
		extracted_frm = deauth_disassoc_frame;
		extracted_frm_len = deauth_disassoc_frame_len;
	}

	lim_extract_ies_from_deauth_disassoc(session, extracted_frm,
					     extracted_frm_len);

	reason_code = sir_read_u16(extracted_frm +
				   sizeof(struct wlan_frame_hdr));
end:
	lim_tear_down_link_with_ap(mac, session->peSessionId,
				   reason_code,
				   eLIM_PEER_ENTITY_DEAUTH);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_FILS_SK
static void
lim_fill_fils_ft(struct pe_session *src_session,
		 struct pe_session *dst_session)
{
      if (src_session->fils_info &&
          src_session->fils_info->fils_ft_len) {
              dst_session->fils_info->fils_ft_len =
                      src_session->fils_info->fils_ft_len;
              qdf_mem_copy(dst_session->fils_info->fils_ft,
                           src_session->fils_info->fils_ft,
                           src_session->fils_info->fils_ft_len);
      }
}
#else
static inline void
lim_fill_fils_ft(struct pe_session *src_session,
		 struct pe_session *dst_session)
{}
#endif

#ifdef WLAN_SUPPORT_TWT
void
lim_fill_roamed_peer_twt_caps(struct mac_context *mac_ctx,
			      uint8_t vdev_id,
			      struct roam_offload_synch_ind *roam_synch)
{
	uint8_t *reassoc_body;
	uint16_t len;
	uint32_t status;
	tDot11fReAssocResponse *reassoc_rsp;
	struct pe_session *pe_session;

	pe_session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!pe_session) {
		pe_err("session not found for given vdev_id %d", vdev_id);
		return;
	}

	reassoc_rsp = qdf_mem_malloc(sizeof(*reassoc_rsp));
	if (!reassoc_rsp)
		return;

	len = roam_synch->reassocRespLength - sizeof(tSirMacMgmtHdr);
	reassoc_body = (uint8_t *)roam_synch + sizeof(tSirMacMgmtHdr) +
			roam_synch->reassocRespOffset;

	status = dot11f_unpack_re_assoc_response(mac_ctx, reassoc_body, len,
						 reassoc_rsp, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to parse a Re-association Rsp (0x%08x, %d bytes):",
		       status, len);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_INFO,
				   reassoc_body, len);
		qdf_mem_free(reassoc_rsp);
		return;
	} else if (DOT11F_WARNED(status)) {
		pe_debug("Warnings while unpacking a Re-association Rsp (0x%08x, %d bytes):",
			 status, len);
	}

	if (lim_is_session_he_capable(pe_session))
		mlme_set_twt_peer_capabilities(mac_ctx->psoc,
					       &roam_synch->bssid,
					       &reassoc_rsp->he_cap,
					       &reassoc_rsp->he_op);
	qdf_mem_free(reassoc_rsp);
}
#endif

/**
 * lim_check_ft_initial_im_association() - To check FT initial mobility(im)
 * association
 * @roam_synch: A pointer to roam sync ind structure
 * @session_entry: pe session
 *
 * This function is to check ft_initial_im_association.
 *
 * Return: None
 */
void
lim_check_ft_initial_im_association(struct roam_offload_synch_ind *roam_synch,
				    struct pe_session *session_entry)
{
	tpSirMacMgmtHdr hdr;
	uint8_t *assoc_req_ptr;

	assoc_req_ptr = (uint8_t *) roam_synch + roam_synch->reassoc_req_offset;
	hdr = (tpSirMacMgmtHdr) assoc_req_ptr;

	if (hdr->fc.type == SIR_MAC_MGMT_FRAME &&
	    hdr->fc.subType == SIR_MAC_MGMT_ASSOC_REQ &&
	    session_entry->is11Rconnection) {
		pe_debug("Frame subtype: %d and connection is %d",
			 hdr->fc.subType, session_entry->is11Rconnection);
		roam_synch->is_ft_im_roam = true;
	}
}

QDF_STATUS
pe_roam_synch_callback(struct mac_context *mac_ctx,
		       struct roam_offload_synch_ind *roam_sync_ind_ptr,
		       struct bss_description *bss_desc,
		       enum sir_roam_op_code reason)
{
	struct pe_session *session_ptr;
	struct pe_session *ft_session_ptr;
	uint8_t session_id;
	uint8_t *reassoc_resp;
	tpDphHashNode curr_sta_ds;
	uint16_t aid;
	struct bss_params *add_bss_params;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint16_t join_rsp_len;

	if (!roam_sync_ind_ptr) {
		pe_err("LFR3:roam_sync_ind_ptr is NULL");
		return status;
	}
	session_ptr = pe_find_session_by_vdev_id(mac_ctx,
				roam_sync_ind_ptr->roamed_vdev_id);
	if (!session_ptr) {
		pe_err("LFR3:Unable to find session");
		return status;
	}

	if (!LIM_IS_STA_ROLE(session_ptr)) {
		pe_err("LFR3:session is not in STA mode");
		return status;
	}

	pe_debug("LFR3: PE callback reason: %d", reason);
	switch (reason) {
	case SIR_ROAMING_ABORT:
		/*
		 * If there was a disassoc or deauth that was received
		 * during roaming and it was not honored, then we have
		 * to internally initiate a disconnect because with
		 * ROAM_ABORT we come back to original AP.
		 */
		if (session_ptr->recvd_deauth_while_roaming)
			lim_perform_deauth(mac_ctx, session_ptr,
					   session_ptr->deauth_disassoc_rc,
					   session_ptr->bssId, 0);
		if (session_ptr->recvd_disassoc_while_roaming) {
			lim_disassoc_tdls_peers(mac_ctx, session_ptr,
						session_ptr->bssId);
			lim_perform_disassoc(mac_ctx, 0,
					     session_ptr->deauth_disassoc_rc,
					     session_ptr, session_ptr->bssId);
		}
		return QDF_STATUS_SUCCESS;
	case SIR_ROAM_SYNCH_PROPAGATION:
		break;
	default:
		return status;
	}

	pe_debug("LFR3:Received ROAM SYNCH IND bssid "QDF_MAC_ADDR_FMT" auth: %d vdevId: %d",
		 QDF_MAC_ADDR_REF(roam_sync_ind_ptr->bssid.bytes),
		 roam_sync_ind_ptr->authStatus,
		 roam_sync_ind_ptr->roamed_vdev_id);

	/*
	 * If deauth from AP already in progress, ignore Roam Synch Indication
	 * from firmware.
	 */
	if (session_ptr->limSmeState != eLIM_SME_LINK_EST_STATE) {
		pe_err("LFR3: Not in Link est state");
		return status;
	}
	status = lim_roam_fill_bss_descr(mac_ctx, roam_sync_ind_ptr, bss_desc);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		pe_err("LFR3:Failed to fill Bss Descr");
		return status;
	}
	status = QDF_STATUS_E_FAILURE;
	ft_session_ptr = pe_create_session(mac_ctx, bss_desc->bssId,
					   &session_id,
					   mac_ctx->lim.max_sta_of_pe_session,
					   session_ptr->bssType,
					   session_ptr->vdev_id,
					   session_ptr->opmode);
	if (!ft_session_ptr) {
		pe_err("LFR3:Cannot create PE Session");
		lim_print_mac_addr(mac_ctx, bss_desc->bssId, LOGE);
		return status;
	}
	/* Update the beacon/probe filter in mac_ctx */
	lim_set_bcn_probe_filter(mac_ctx, ft_session_ptr, 0);

	sir_copy_mac_addr(ft_session_ptr->self_mac_addr,
			  session_ptr->self_mac_addr);
	sir_copy_mac_addr(roam_sync_ind_ptr->self_mac.bytes,
			session_ptr->self_mac_addr);
	sir_copy_mac_addr(ft_session_ptr->limReAssocbssId, bss_desc->bssId);
	session_ptr->bRoamSynchInProgress = true;
	ft_session_ptr->bRoamSynchInProgress = true;
	ft_session_ptr->limSystemRole = eLIM_STA_ROLE;
	sir_copy_mac_addr(session_ptr->limReAssocbssId, bss_desc->bssId);
	ft_session_ptr->csaOffloadEnable = session_ptr->csaOffloadEnable;

	/* Next routine will update nss and vdev_nss with AP's capabilities */
	lim_fill_ft_session(mac_ctx, bss_desc, ft_session_ptr,
			    session_ptr, roam_sync_ind_ptr->phy_mode);
	pe_set_rmf_caps(mac_ctx, ft_session_ptr, roam_sync_ind_ptr);
	/* Next routine may update nss based on dot11Mode */
	lim_ft_prepare_add_bss_req(mac_ctx, ft_session_ptr, bss_desc);
	if (session_ptr->is11Rconnection)
		lim_fill_fils_ft(session_ptr, ft_session_ptr);

	roam_sync_ind_ptr->add_bss_params =
		(struct bss_params *) ft_session_ptr->ftPEContext.pAddBssReq;
	add_bss_params = ft_session_ptr->ftPEContext.pAddBssReq;
	lim_delete_tdls_peers(mac_ctx, session_ptr);
	curr_sta_ds = dph_lookup_hash_entry(mac_ctx, session_ptr->bssId, &aid,
					    &session_ptr->dph.dphHashTable);
	if (!curr_sta_ds) {
		pe_err("LFR3:failed to lookup hash entry");
		ft_session_ptr->bRoamSynchInProgress = false;
		return status;
	}
	session_ptr->limSmeState = eLIM_SME_IDLE_STATE;
	lim_cleanup_rx_path(mac_ctx, curr_sta_ds, session_ptr, false);
	lim_delete_dph_hash_entry(mac_ctx, curr_sta_ds->staAddr, aid,
				  session_ptr);
	pe_delete_session(mac_ctx, session_ptr);
	session_ptr = NULL;
	curr_sta_ds = dph_add_hash_entry(mac_ctx,
					 roam_sync_ind_ptr->bssid.bytes,
					 DPH_STA_HASH_INDEX_PEER,
					 &ft_session_ptr->dph.dphHashTable);
	if (!curr_sta_ds) {
		pe_err("LFR3:failed to add hash entry for "QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(add_bss_params->staContext.staMac));
		ft_session_ptr->bRoamSynchInProgress = false;
		return status;
	}


	if (roam_sync_ind_ptr->authStatus ==
	    CSR_ROAM_AUTH_STATUS_AUTHENTICATED) {
		ft_session_ptr->is_key_installed = true;
		curr_sta_ds->is_key_installed = true;
	}

	reassoc_resp = (uint8_t *)roam_sync_ind_ptr +
			roam_sync_ind_ptr->reassocRespOffset;
	lim_process_assoc_rsp_frame(mac_ctx, reassoc_resp,
				    roam_sync_ind_ptr->reassocRespLength,
				    LIM_REASSOC, ft_session_ptr);

	lim_check_ft_initial_im_association(roam_sync_ind_ptr, ft_session_ptr);

	lim_copy_and_free_hlp_data_from_session(ft_session_ptr,
						roam_sync_ind_ptr);

	roam_sync_ind_ptr->aid = ft_session_ptr->limAID;
	curr_sta_ds->mlmStaContext.mlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
	curr_sta_ds->nss = ft_session_ptr->nss;
	roam_sync_ind_ptr->nss = ft_session_ptr->nss;
	ft_session_ptr->limMlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
	ft_session_ptr->limPrevMlmState = ft_session_ptr->limMlmState;
	lim_init_tdls_data(mac_ctx, ft_session_ptr);
	join_rsp_len = ft_session_ptr->RICDataLen +
			sizeof(struct join_rsp) - sizeof(uint8_t);

#ifdef FEATURE_WLAN_ESE
	join_rsp_len += ft_session_ptr->tspecLen;
	pe_debug("LFR3: tspecLen: %d", ft_session_ptr->tspecLen);
#endif

	roam_sync_ind_ptr->join_rsp = qdf_mem_malloc(join_rsp_len);
	if (!roam_sync_ind_ptr->join_rsp) {
		ft_session_ptr->bRoamSynchInProgress = false;
		return QDF_STATUS_E_NOMEM;
	}

	pe_debug("LFR3: Session RicLength: %d", ft_session_ptr->RICDataLen);
	if (ft_session_ptr->ricData) {
		roam_sync_ind_ptr->join_rsp->parsedRicRspLen =
					ft_session_ptr->RICDataLen;
		qdf_mem_copy(roam_sync_ind_ptr->join_rsp->frames,
			     ft_session_ptr->ricData,
			     roam_sync_ind_ptr->join_rsp->parsedRicRspLen);
		qdf_mem_free(ft_session_ptr->ricData);
		ft_session_ptr->ricData = NULL;
		ft_session_ptr->RICDataLen = 0;
	}

#ifdef FEATURE_WLAN_ESE
	if (ft_session_ptr->tspecIes) {
		roam_sync_ind_ptr->join_rsp->tspecIeLen =
					ft_session_ptr->tspecLen;
		qdf_mem_copy(roam_sync_ind_ptr->join_rsp->frames +
			     roam_sync_ind_ptr->join_rsp->parsedRicRspLen,
			     ft_session_ptr->tspecIes,
			     roam_sync_ind_ptr->join_rsp->tspecIeLen);
		qdf_mem_free(ft_session_ptr->tspecIes);
		ft_session_ptr->tspecIes = NULL;
		ft_session_ptr->tspecLen = 0;
	}
#endif

	roam_sync_ind_ptr->join_rsp->vht_channel_width =
					ft_session_ptr->ch_width;
	roam_sync_ind_ptr->join_rsp->timingMeasCap = curr_sta_ds->timingMeasCap;
	roam_sync_ind_ptr->join_rsp->nss = curr_sta_ds->nss;
	roam_sync_ind_ptr->join_rsp->max_rate_flags =
			lim_get_max_rate_flags(mac_ctx, curr_sta_ds);
	lim_set_tdls_flags(roam_sync_ind_ptr, ft_session_ptr);
	roam_sync_ind_ptr->join_rsp->aid = ft_session_ptr->limAID;
	lim_fill_join_rsp_ht_caps(ft_session_ptr, roam_sync_ind_ptr->join_rsp);
	ft_session_ptr->limSmeState = eLIM_SME_LINK_EST_STATE;
	ft_session_ptr->limPrevSmeState = ft_session_ptr->limSmeState;
	ft_session_ptr->bRoamSynchInProgress = false;

	return QDF_STATUS_SUCCESS;
}
#endif

static bool lim_is_beacon_miss_scenario(struct mac_context *mac,
					uint8_t *pRxPacketInfo)
{
	tpSirMacMgmtHdr pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	uint8_t sessionId;
	struct pe_session *pe_session =
		pe_find_session_by_bssid(mac, pHdr->bssId, &sessionId);

	if (pe_session && pe_session->pmmOffloadInfo.bcnmiss)
		return true;
	return false;
}

/** -----------------------------------------------------------------
   \brief lim_is_pkt_candidate_for_drop() - decides whether to drop the frame or not

   This function is called before enqueuing the frame to PE queue for further processing.
   This prevents unnecessary frames getting into PE Queue and drops them right away.
   Frames will be droped in the following scenarios:

   - In Scan State, drop the frames which are not marked as scan frames
   - In non-Scan state, drop the frames which are marked as scan frames.

   \param mac - global mac structure
   \return - none
   \sa
   ----------------------------------------------------------------- */

tMgmtFrmDropReason lim_is_pkt_candidate_for_drop(struct mac_context *mac,
						 uint8_t *pRxPacketInfo,
						 uint32_t subType)
{
	uint32_t framelen;
	uint8_t *pBody;
	tSirMacCapabilityInfo capabilityInfo;
	tpSirMacMgmtHdr pHdr = NULL;
	struct pe_session *pe_session = NULL;
	uint8_t sessionId;

	/*
	 *
	 * In scan mode, drop only Beacon/Probe Response which are NOT marked as scan-frames.
	 * In non-scan mode, drop only Beacon/Probe Response which are marked as scan frames.
	 * Allow other mgmt frames, they must be from our own AP, as we don't allow
	 * other than beacons or probe responses in scan state.
	 */
	if ((subType == SIR_MAC_MGMT_BEACON) ||
	    (subType == SIR_MAC_MGMT_PROBE_RSP)) {
		if (lim_is_beacon_miss_scenario(mac, pRxPacketInfo)) {
			MTRACE(mac_trace(mac, TRACE_CODE_INFO_LOG, 0,
					 eLOG_NODROP_MISSED_BEACON_SCENARIO));
			return eMGMT_DROP_NO_DROP;
		}

		framelen = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
		pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
		/* drop the frame if length is less than 12 */
		if (framelen < LIM_MIN_BCN_PR_LENGTH)
			return eMGMT_DROP_INVALID_SIZE;

		*((uint16_t *) &capabilityInfo) =
			sir_read_u16(pBody + LIM_BCN_PR_CAPABILITY_OFFSET);

		/* Note sure if this is sufficient, basically this condition allows all probe responses and
		 *   beacons from an infrastructure network
		 */
		if (!capabilityInfo.ibss)
			return eMGMT_DROP_NO_DROP;

		/* Drop INFRA Beacons and Probe Responses in IBSS Mode */
		/* This can be enhanced to even check the SSID before deciding to enque the frame. */
		if (capabilityInfo.ess)
			return eMGMT_DROP_INFRA_BCN_IN_IBSS;

	} else if (subType == SIR_MAC_MGMT_AUTH) {
		uint16_t curr_seq_num = 0;
		struct tLimPreAuthNode *auth_node;

		pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
		pe_session = pe_find_session_by_bssid(mac, pHdr->bssId,
							 &sessionId);
		if (!pe_session)
			return eMGMT_DROP_NO_DROP;

		curr_seq_num = ((pHdr->seqControl.seqNumHi << 4) |
				(pHdr->seqControl.seqNumLo));
		auth_node = lim_search_pre_auth_list(mac, pHdr->sa);
		if (auth_node && pHdr->fc.retry &&
		    (auth_node->seq_num == curr_seq_num)) {
			pe_err_rl("auth frame, seq num: %d is already processed, drop it",
				  curr_seq_num);
			return eMGMT_DROP_DUPLICATE_AUTH_FRAME;
		}
	} else if ((subType == SIR_MAC_MGMT_ASSOC_REQ) ||
		   (subType == SIR_MAC_MGMT_DISASSOC) ||
		   (subType == SIR_MAC_MGMT_DEAUTH)) {
		struct peer_mlme_priv_obj *peer_priv;
		struct wlan_objmgr_peer *peer;
		qdf_time_t *timestamp;

		pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
		pe_session = pe_find_session_by_bssid(mac, pHdr->bssId,
				&sessionId);
		if (!pe_session)
			return eMGMT_DROP_SPURIOUS_FRAME;

		peer = wlan_objmgr_get_peer_by_mac(mac->psoc,
						   pHdr->sa,
						   WLAN_LEGACY_MAC_ID);
		if (!peer) {
			if (subType == SIR_MAC_MGMT_ASSOC_REQ)
				return eMGMT_DROP_NO_DROP;

			return eMGMT_DROP_SPURIOUS_FRAME;
		}

		peer_priv = wlan_objmgr_peer_get_comp_private_obj(peer,
							WLAN_UMAC_COMP_MLME);
		if (!peer_priv) {
			wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
			if (subType == SIR_MAC_MGMT_ASSOC_REQ)
				return eMGMT_DROP_NO_DROP;

			return eMGMT_DROP_SPURIOUS_FRAME;
		}

		if (subType == SIR_MAC_MGMT_ASSOC_REQ)
			timestamp =
			   &peer_priv->last_assoc_received_time;
		else
			timestamp =
			   &peer_priv->last_disassoc_deauth_received_time;

		if (*timestamp > 0 &&
		    qdf_system_time_before(qdf_get_system_timestamp(),
					   *timestamp +
					   LIM_DOS_PROTECTION_TIME)) {
			pe_debug_rl(FL("Dropping subtype 0x%x frame. %s %d ms %s %d ms"),
				    subType, "It is received after",
				    (int)(qdf_get_system_timestamp() - *timestamp),
				    "of last frame. Allow it only after",
				    LIM_DOS_PROTECTION_TIME);
			wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);
			return eMGMT_DROP_EXCESSIVE_MGMT_FRAME;
		}

		*timestamp = qdf_get_system_timestamp();
		wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);

	}

	return eMGMT_DROP_NO_DROP;
}

void lim_update_lost_link_info(struct mac_context *mac, struct pe_session *session,
				int32_t rssi)
{
	struct sir_lost_link_info *lost_link_info;
	struct scheduler_msg mmh_msg = {0};

	if ((!mac) || (!session)) {
		pe_err("parameter NULL");
		return;
	}
	if (!LIM_IS_STA_ROLE(session))
		return;

	lost_link_info = qdf_mem_malloc(sizeof(*lost_link_info));
	if (!lost_link_info)
		return;

	lost_link_info->vdev_id = session->smeSessionId;
	lost_link_info->rssi = rssi;
	mmh_msg.type = eWNI_SME_LOST_LINK_INFO_IND;
	mmh_msg.bodyptr = lost_link_info;
	mmh_msg.bodyval = 0;
	pe_debug("post eWNI_SME_LOST_LINK_INFO_IND, bss_idx: %d rssi: %d",
		lost_link_info->vdev_id, lost_link_info->rssi);

	lim_sys_process_mmh_msg_api(mac, &mmh_msg);
}

/**
 * lim_mon_init_session() - create PE session for monitor mode operation
 * @mac_ptr: mac pointer
 * @msg: Pointer to struct sir_create_session type.
 *
 * Return: NONE
 */
void lim_mon_init_session(struct mac_context *mac_ptr,
			  struct sir_create_session *msg)
{
	struct pe_session *psession_entry;
	uint8_t session_id;

	psession_entry = pe_create_session(mac_ptr, msg->bss_id.bytes,
					   &session_id,
					   mac_ptr->lim.max_sta_of_pe_session,
					   eSIR_MONITOR_MODE,
					   msg->vdev_id,
					   QDF_MONITOR_MODE);
	if (!psession_entry) {
		pe_err("Monitor mode: Session Can not be created");
		lim_print_mac_addr(mac_ptr, msg->bss_id.bytes, LOGE);
		return;
	}
	psession_entry->vhtCapability = 1;
}

void lim_mon_deinit_session(struct mac_context *mac_ptr,
			    struct sir_delete_session *msg)
{
	struct pe_session *session;

	session = pe_find_session_by_vdev_id(mac_ptr, msg->vdev_id);

	if (session && session->bssType == eSIR_MONITOR_MODE)
		pe_delete_session(mac_ptr, session);
}

/**
 * lim_update_ext_cap_ie() - Update Extended capabilities IE(if present)
 *          with capabilities of Fine Time measurements(FTM) if set in driver
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @ie_data: Default Scan IE data
 * @local_ie_buf: Local Scan IE data
 * @local_ie_len: Pointer to length of @ie_data
 * @session: Pointer to pe session
 *
 * Return: QDF_STATUS
 */
QDF_STATUS lim_update_ext_cap_ie(struct mac_context *mac_ctx, uint8_t *ie_data,
				 uint8_t *local_ie_buf, uint16_t *local_ie_len,
				 struct pe_session *session)
{
	uint32_t dot11mode;
	bool vht_enabled = false;
	tDot11fIEExtCap default_scan_ext_cap = {0}, driver_ext_cap = {0};
	QDF_STATUS status;

	status = lim_strip_extcap_update_struct(mac_ctx, ie_data,
				   local_ie_len, &default_scan_ext_cap);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Strip ext cap fails %d", status);
		return QDF_STATUS_E_FAILURE;
	}

	if ((*local_ie_len) > (MAX_DEFAULT_SCAN_IE_LEN - EXT_CAP_IE_HDR_LEN)) {
		pe_err("Invalid Scan IE length");
		return QDF_STATUS_E_FAILURE;
	}
	/* copy ie prior to ext cap to local buffer */
	qdf_mem_copy(local_ie_buf, ie_data, (*local_ie_len));

	/* from here ext cap ie starts, set EID */
	local_ie_buf[*local_ie_len] = DOT11F_EID_EXTCAP;

	dot11mode = mac_ctx->mlme_cfg->dot11_mode.dot11_mode;
	if (IS_DOT11_MODE_VHT(dot11mode))
		vht_enabled = true;

	status = populate_dot11f_ext_cap(mac_ctx, vht_enabled,
					&driver_ext_cap, NULL);
	if (QDF_STATUS_SUCCESS != status) {
		pe_err("Failed %d to create ext cap IE. Use default value instead",
				status);
		local_ie_buf[*local_ie_len + 1] = DOT11F_IE_EXTCAP_MAX_LEN;

		if ((*local_ie_len) > (MAX_DEFAULT_SCAN_IE_LEN -
		    (DOT11F_IE_EXTCAP_MAX_LEN + EXT_CAP_IE_HDR_LEN))) {
			pe_err("Invalid Scan IE length");
			return QDF_STATUS_E_FAILURE;
		}
		(*local_ie_len) += EXT_CAP_IE_HDR_LEN;
		qdf_mem_copy(local_ie_buf + (*local_ie_len),
				default_scan_ext_cap.bytes,
				DOT11F_IE_EXTCAP_MAX_LEN);
		(*local_ie_len) += DOT11F_IE_EXTCAP_MAX_LEN;
		return QDF_STATUS_SUCCESS;
	}
	lim_merge_extcap_struct(&driver_ext_cap, &default_scan_ext_cap, true);

	if (session)
		populate_dot11f_twt_extended_caps(mac_ctx, session,
						  &driver_ext_cap);
	else
		pe_debug("Session NULL, cannot set TWT caps");

	local_ie_buf[*local_ie_len + 1] = driver_ext_cap.num_bytes;

	if ((*local_ie_len) > (MAX_DEFAULT_SCAN_IE_LEN -
	    (EXT_CAP_IE_HDR_LEN + driver_ext_cap.num_bytes))) {
		pe_err("Invalid Scan IE length");
		return QDF_STATUS_E_FAILURE;
	}
	(*local_ie_len) += EXT_CAP_IE_HDR_LEN;
	qdf_mem_copy(local_ie_buf + (*local_ie_len),
			driver_ext_cap.bytes, driver_ext_cap.num_bytes);
	(*local_ie_len) += driver_ext_cap.num_bytes;
	return QDF_STATUS_SUCCESS;
}

#define LIM_RSN_OUI_SIZE 4

struct rsn_oui_akm_type_map {
	enum ani_akm_type akm_type;
	uint8_t rsn_oui[LIM_RSN_OUI_SIZE];
};

static const struct rsn_oui_akm_type_map rsn_oui_akm_type_mapping_table[] = {
	{ANI_AKM_TYPE_RSN,                  {0x00, 0x0F, 0xAC, 0x01} },
	{ANI_AKM_TYPE_RSN_PSK,              {0x00, 0x0F, 0xAC, 0x02} },
	{ANI_AKM_TYPE_FT_RSN,               {0x00, 0x0F, 0xAC, 0x03} },
	{ANI_AKM_TYPE_FT_RSN_PSK,           {0x00, 0x0F, 0xAC, 0x04} },
	{ANI_AKM_TYPE_RSN_8021X_SHA256,     {0x00, 0x0F, 0xAC, 0x05} },
	{ANI_AKM_TYPE_RSN_PSK_SHA256,       {0x00, 0x0F, 0xAC, 0x06} },
#ifdef WLAN_FEATURE_SAE
	{ANI_AKM_TYPE_SAE,                  {0x00, 0x0F, 0xAC, 0x08} },
	{ANI_AKM_TYPE_FT_SAE,               {0x00, 0x0F, 0xAC, 0x09} },
#endif
	{ANI_AKM_TYPE_SUITEB_EAP_SHA256,    {0x00, 0x0F, 0xAC, 0x0B} },
	{ANI_AKM_TYPE_SUITEB_EAP_SHA384,    {0x00, 0x0F, 0xAC, 0x0C} },
	{ANI_AKM_TYPE_FT_SUITEB_EAP_SHA384, {0x00, 0x0F, 0xAC, 0x0D} },
	{ANI_AKM_TYPE_FILS_SHA256,          {0x00, 0x0F, 0xAC, 0x0E} },
	{ANI_AKM_TYPE_FILS_SHA384,          {0x00, 0x0F, 0xAC, 0x0F} },
	{ANI_AKM_TYPE_FT_FILS_SHA256,       {0x00, 0x0F, 0xAC, 0x10} },
	{ANI_AKM_TYPE_FT_FILS_SHA384,       {0x00, 0x0F, 0xAC, 0x11} },
	{ANI_AKM_TYPE_OWE,                  {0x00, 0x0F, 0xAC, 0x12} },
#ifdef FEATURE_WLAN_ESE
	{ANI_AKM_TYPE_CCKM,                 {0x00, 0x40, 0x96, 0x00} },
#endif
	{ANI_AKM_TYPE_OSEN,                 {0x50, 0x6F, 0x9A, 0x01} },
	{ANI_AKM_TYPE_DPP_RSN,              {0x50, 0x6F, 0x9A, 0x02} },
	{ANI_AKM_TYPE_WPA,                  {0x00, 0x50, 0xF2, 0x01} },
	{ANI_AKM_TYPE_WPA_PSK,              {0x00, 0x50, 0xF2, 0x02} },
	/* Add akm type above here */
	{ANI_AKM_TYPE_UNKNOWN, {0} },
};

enum ani_akm_type lim_translate_rsn_oui_to_akm_type(uint8_t auth_suite[4])
{
	const struct rsn_oui_akm_type_map *map;
	enum ani_akm_type akm_type;

	map = rsn_oui_akm_type_mapping_table;
	while (true) {
		akm_type = map->akm_type;
		if ((akm_type == ANI_AKM_TYPE_UNKNOWN) ||
		    (qdf_mem_cmp(auth_suite, map->rsn_oui, 4) == 0))
			break;
		map++;
	}

	pe_debug("akm_type: %d", akm_type);

	return akm_type;
}
