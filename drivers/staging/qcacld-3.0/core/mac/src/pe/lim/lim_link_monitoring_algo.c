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

/*
 * This file lim_link_monitoring_algo.cc contains the code for
 * Link monitoring algorithm on AP and heart beat failure
 * handling on STA.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "ani_global.h"
#include "wni_cfg.h"

#include "sch_api.h"
#include "utils_api.h"
#include "lim_assoc_utils.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_prop_exts_utils.h"

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
#include "host_diag_core_log.h"
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "lim_ser_des_utils.h"
#include "wlan_blm_api.h"

/**
 * lim_delete_sta_util - utility function for deleting station context
 *
 * @mac_ctx: global MAC context
 * @msg: pointer to delte station context
 * @session_entry: PE session entry
 *
 * utility function called to clear up station context.
 *
 * Return: None.
 */
static void lim_delete_sta_util(struct mac_context *mac_ctx, tpDeleteStaContext msg,
				struct pe_session *session_entry)
{
	tpDphHashNode stads;

	pe_debug("Deleting station: reasonCode: %d", msg->reasonCode);

	stads = dph_lookup_hash_entry(mac_ctx, msg->addr2, &msg->assocId,
				      &session_entry->dph.dphHashTable);

	if (!stads) {
		pe_err("Invalid STA limSystemRole: %d",
			GET_LIM_SYSTEM_ROLE(session_entry));
		return;
	}
	stads->del_sta_ctx_rssi = msg->rssi;

	if (LIM_IS_AP_ROLE(session_entry)) {
		pe_debug("Delete Station assocId: %d", msg->assocId);
		/*
		 * Check if Deauth/Disassoc is triggered from Host.
		 * If mlmState is in some transient state then
		 * don't trigger STA deletion to avoid the race
		 * condition.
		 */
		if ((stads &&
		     ((stads->mlmStaContext.mlmState !=
			eLIM_MLM_LINK_ESTABLISHED_STATE) &&
		      (stads->mlmStaContext.mlmState !=
			eLIM_MLM_WT_ASSOC_CNF_STATE) &&
		      (stads->mlmStaContext.mlmState !=
			eLIM_MLM_ASSOCIATED_STATE)))) {
			pe_err("Inv Del STA assocId: %d", msg->assocId);
			return;
		} else {
			lim_send_disassoc_mgmt_frame(mac_ctx,
				REASON_DISASSOC_DUE_TO_INACTIVITY,
				stads->staAddr, session_entry, false);
			lim_trigger_sta_deletion(mac_ctx, stads, session_entry);
		}
	} else {
#ifdef FEATURE_WLAN_TDLS
		if (LIM_IS_STA_ROLE(session_entry) &&
		    STA_ENTRY_TDLS_PEER == stads->staType) {
			/*
			 * TeardownLink with PEER reason code
			 * HAL_DEL_STA_REASON_CODE_KEEP_ALIVE means
			 * eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE
			 */
			lim_send_sme_tdls_del_sta_ind(mac_ctx, stads,
			    session_entry, REASON_TDLS_PEER_UNREACHABLE);
		} else {
#endif
		/* TearDownLink with AP */
		tLimMlmDeauthInd mlm_deauth_ind;

		pe_debug("Delete Station (assocId: %d)", msg->assocId);

		if ((stads &&
			((stads->mlmStaContext.mlmState !=
					eLIM_MLM_LINK_ESTABLISHED_STATE) &&
			(stads->mlmStaContext.mlmState !=
					eLIM_MLM_WT_ASSOC_CNF_STATE) &&
			(stads->mlmStaContext.mlmState !=
					eLIM_MLM_ASSOCIATED_STATE)))) {

			/*
			 * Received SIR_LIM_DELETE_STA_CONTEXT_IND for STA that
			 * does not have context or in some transit state.
			 * Log error
			 */
			pe_debug("Received SIR_LIM_DELETE_STA_CONTEXT_IND for "
					"STA that either has no context or "
					"in some transit state, Addr = "
					QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(msg->bssId));
			return;
		}

		stads->mlmStaContext.disassocReason =
			REASON_DISASSOC_DUE_TO_INACTIVITY;
		stads->mlmStaContext.cleanupTrigger =
			eLIM_LINK_MONITORING_DEAUTH;

		/* Issue Deauth Indication to SME. */
		qdf_mem_copy((uint8_t *) &mlm_deauth_ind.peerMacAddr,
			     stads->staAddr, sizeof(tSirMacAddr));
		mlm_deauth_ind.reasonCode =
			(uint8_t) stads->mlmStaContext.disassocReason;
		mlm_deauth_ind.deauthTrigger =
			stads->mlmStaContext.cleanupTrigger;

#ifdef FEATURE_WLAN_TDLS
		/* Delete all TDLS peers connected before leaving BSS */
		lim_delete_tdls_peers(mac_ctx, session_entry);
#endif
		if (LIM_IS_STA_ROLE(session_entry))
			lim_post_sme_message(mac_ctx, LIM_MLM_DEAUTH_IND,
				     (uint32_t *) &mlm_deauth_ind);

		lim_send_sme_deauth_ind(mac_ctx, stads,	session_entry);
#ifdef FEATURE_WLAN_TDLS
	}
#endif
	}
}

/**
 * lim_delete_sta_context() - delete sta context.
 *
 * @mac_ctx: global mac_ctx context
 * @lim_msg: lim message.
 *
 * This function handles the message from HAL: WMA_DELETE_STA_CONTEXT_IND.
 * This function validates that the given station id exist, and if so,
 * deletes the station by calling lim_trigger_sta_deletion.
 *
 * Return: none
 */
void lim_delete_sta_context(struct mac_context *mac_ctx,
			    struct scheduler_msg *lim_msg)
{
	tpDeleteStaContext msg = (tpDeleteStaContext) lim_msg->bodyptr;
	struct pe_session *session_entry;
	tpDphHashNode sta_ds;
	enum wlan_reason_code reason_code;
	struct reject_ap_info ap_info;

	if (!msg) {
		pe_err("Invalid body pointer in message");
		return;
	}
	session_entry = pe_find_session_by_vdev_id(mac_ctx, msg->vdev_id);
	if (!session_entry) {
		pe_err("session not found for given sme session");
		qdf_mem_free(msg);
		return;
	}

	switch (msg->reasonCode) {
	case HAL_DEL_STA_REASON_CODE_KEEP_ALIVE:
	case HAL_DEL_STA_REASON_CODE_SA_QUERY_TIMEOUT:
	case HAL_DEL_STA_REASON_CODE_XRETRY:
		if (LIM_IS_STA_ROLE(session_entry) && !msg->is_tdls) {
			if (!((session_entry->limMlmState ==
			    eLIM_MLM_LINK_ESTABLISHED_STATE) &&
			    (session_entry->limSmeState !=
			    eLIM_SME_WT_DISASSOC_STATE) &&
			    (session_entry->limSmeState !=
			    eLIM_SME_WT_DEAUTH_STATE))) {
				pe_err("Do not process in limMlmState %s(%x) limSmeState %s(%x)",
				  lim_mlm_state_str(session_entry->limMlmState),
				  session_entry->limMlmState,
				  lim_sme_state_str(session_entry->limSmeState),
				  session_entry->limSmeState);
				qdf_mem_free(msg);
				return;
			}
			sta_ds = dph_get_hash_entry(mac_ctx,
					DPH_STA_HASH_INDEX_PEER,
					&session_entry->dph.dphHashTable);
			if (!sta_ds) {
				pe_err("Dph entry not found");
				qdf_mem_free(msg);
				return;
			}
			lim_send_deauth_mgmt_frame(mac_ctx,
				REASON_DISASSOC_DUE_TO_INACTIVITY,
				msg->addr2, session_entry, false);
			if (msg->reasonCode ==
				HAL_DEL_STA_REASON_CODE_SA_QUERY_TIMEOUT)
				reason_code = REASON_SA_QUERY_TIMEOUT;
			else if (msg->reasonCode ==
				HAL_DEL_STA_REASON_CODE_XRETRY)
				reason_code = REASON_PEER_XRETRY_FAIL;
			else
				reason_code = REASON_PEER_INACTIVITY;
			lim_tear_down_link_with_ap(mac_ctx,
						   session_entry->peSessionId,
						   reason_code,
						   eLIM_LINK_MONITORING_DEAUTH);
			qdf_mem_copy(&ap_info.bssid, msg->addr2,
				     QDF_MAC_ADDR_SIZE);
			ap_info.reject_ap_type = DRIVER_AVOID_TYPE;
			ap_info.reject_reason = REASON_STA_KICKOUT;
			ap_info.source = ADDED_BY_DRIVER;
			wlan_blm_add_bssid_to_reject_list(mac_ctx->pdev,
							  &ap_info);

			/* only break for STA role (non TDLS) */
			break;
		}
		lim_delete_sta_util(mac_ctx, msg, session_entry);
		break;

	case HAL_DEL_STA_REASON_CODE_UNKNOWN_A2:
		pe_err("Deleting Unknown station");
		lim_print_mac_addr(mac_ctx, msg->addr2, LOGE);
		lim_send_deauth_mgmt_frame(mac_ctx,
			REASON_CLASS3_FRAME_FROM_NON_ASSOC_STA,
			msg->addr2, session_entry, false);
		break;

	case HAL_DEL_STA_REASON_CODE_BTM_DISASSOC_IMMINENT:
		if (session_entry->limMlmState !=
		    eLIM_MLM_LINK_ESTABLISHED_STATE) {
			pe_err("BTM request received in state %s",
				lim_mlm_state_str(session_entry->limMlmState));
			qdf_mem_free(msg);
			lim_msg->bodyptr = NULL;
			return;
		}
		lim_send_deauth_mgmt_frame(mac_ctx,
				REASON_DISASSOC_BSS_TRANSITION ,
				session_entry->bssId, session_entry, false);
		lim_tear_down_link_with_ap(mac_ctx, session_entry->peSessionId,
					   REASON_DISASSOC_BSS_TRANSITION ,
					   eLIM_LINK_MONITORING_DEAUTH);
		break;

	default:
		pe_err("Unknown reason code");
		break;
	}
	qdf_mem_free(msg);
	lim_msg->bodyptr = NULL;
	return;
}

/**
 * lim_trigger_sta_deletion() -
 *          This function is called to trigger STA context deletion.
 *
 * @param  mac_ctx   - Pointer to global MAC structure
 * @param  sta_ds - Pointer to internal STA Datastructure
 * @session_entry: PE session entry

 * @return None
 */
void
lim_trigger_sta_deletion(struct mac_context *mac_ctx, tpDphHashNode sta_ds,
			 struct pe_session *session_entry)
{
	tLimMlmDisassocInd mlm_disassoc_ind;

	if (!sta_ds) {
		pe_warn("Skip STA deletion (invalid STA)");
		return;
	}

	if ((sta_ds->mlmStaContext.mlmState == eLIM_MLM_WT_DEL_STA_RSP_STATE) ||
		(sta_ds->mlmStaContext.mlmState ==
			eLIM_MLM_WT_DEL_BSS_RSP_STATE) ||
		sta_ds->sta_deletion_in_progress) {
		/* Already in the process of deleting context for the peer */
		pe_debug("Deletion is in progress (%d) for peer:%pK in mlmState %d",
			sta_ds->sta_deletion_in_progress, sta_ds->staAddr,
			sta_ds->mlmStaContext.mlmState);
		return;
	}
	sta_ds->sta_deletion_in_progress = true;

	sta_ds->mlmStaContext.disassocReason =
		REASON_DISASSOC_DUE_TO_INACTIVITY;
	sta_ds->mlmStaContext.cleanupTrigger = eLIM_LINK_MONITORING_DISASSOC;
	qdf_mem_copy(&mlm_disassoc_ind.peerMacAddr, sta_ds->staAddr,
		sizeof(tSirMacAddr));
	mlm_disassoc_ind.reasonCode =
		REASON_DISASSOC_DUE_TO_INACTIVITY;
	mlm_disassoc_ind.disassocTrigger = eLIM_LINK_MONITORING_DISASSOC;

	/* Update PE session Id */
	mlm_disassoc_ind.sessionId = session_entry->peSessionId;
	lim_post_sme_message(mac_ctx, LIM_MLM_DISASSOC_IND,
			(uint32_t *) &mlm_disassoc_ind);
	if (mac_ctx->mlme_cfg->gen.fatal_event_trigger)
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
				WLAN_LOG_INDICATOR_HOST_DRIVER,
				WLAN_LOG_REASON_HB_FAILURE,
				false, false);
	/* Issue Disassoc Indication to SME */
	lim_send_sme_disassoc_ind(mac_ctx, sta_ds, session_entry);
} /*** end lim_trigger_st_adeletion() ***/

void
lim_tear_down_link_with_ap(struct mac_context *mac, uint8_t sessionId,
			   enum wlan_reason_code reasonCode,
			   enum eLimDisassocTrigger trigger)
{
	tpDphHashNode sta = NULL;

	/* tear down the following pe_session */
	struct pe_session *pe_session;

	pe_session = pe_find_session_by_session_id(mac, sessionId);
	if (!pe_session) {
		pe_err("Session Does not exist for given sessionID");
		return;
	}
	/**
	 * Heart beat failed for upto threshold value
	 * and AP did not respond for Probe request.
	 * Trigger link tear down.
	 */
	pe_session->pmmOffloadInfo.bcnmiss = false;

	pe_info("Session %d Vdev %d reason code %d trigger %d",
		pe_session->peSessionId, pe_session->vdev_id, reasonCode,
		trigger);

	/* Announce loss of link to Roaming algorithm */
	/* and cleanup by sending SME_DISASSOC_REQ to SME */

	sta =
		dph_get_hash_entry(mac, DPH_STA_HASH_INDEX_PEER,
				   &pe_session->dph.dphHashTable);

	if (sta) {
		tLimMlmDeauthInd mlmDeauthInd;

		if ((sta->mlmStaContext.disassocReason ==
		    REASON_DEAUTH_NETWORK_LEAVING) ||
		    (sta->mlmStaContext.cleanupTrigger ==
		    eLIM_HOST_DEAUTH)) {
			pe_err("Host already issued deauth, do nothing");
			return;
		}

#ifdef FEATURE_WLAN_TDLS
		/* Delete all TDLS peers connected before leaving BSS */
		lim_delete_tdls_peers(mac, pe_session);
#endif

		sta->mlmStaContext.disassocReason = reasonCode;
		sta->mlmStaContext.cleanupTrigger = trigger;
		/* / Issue Deauth Indication to SME. */
		qdf_mem_copy((uint8_t *) &mlmDeauthInd.peerMacAddr,
			     sta->staAddr, sizeof(tSirMacAddr));

		/*
		 * if deauth_before_connection is enabled and reasoncode is
		 * Beacon Missed Store the MAC of AP in the flip flop
		 * buffer. This MAC will be used to send Deauth before
		 * connection, if we connect to same AP after HB failure.
		 */
		if (mac->mlme_cfg->sta.deauth_before_connection &&
		    reasonCode == REASON_BEACON_MISSED) {
			int apCount = mac->lim.gLimHeartBeatApMacIndex;

			if (mac->lim.gLimHeartBeatApMacIndex)
				mac->lim.gLimHeartBeatApMacIndex = 0;
			else
				mac->lim.gLimHeartBeatApMacIndex = 1;

			pe_debug("HB Failure on MAC "
				 QDF_MAC_ADDR_FMT" Store it on Index %d",
				 QDF_MAC_ADDR_REF(sta->staAddr), apCount);

			sir_copy_mac_addr(mac->lim.gLimHeartBeatApMac[apCount],
					  sta->staAddr);
		}

		mlmDeauthInd.reasonCode =
			(uint8_t) sta->mlmStaContext.disassocReason;
		mlmDeauthInd.deauthTrigger =
			sta->mlmStaContext.cleanupTrigger;

		if (LIM_IS_STA_ROLE(pe_session))
			lim_post_sme_message(mac, LIM_MLM_DEAUTH_IND,
				     (uint32_t *) &mlmDeauthInd);
		if (mac->mlme_cfg->gen.fatal_event_trigger)
			cds_flush_logs(WLAN_LOG_TYPE_FATAL,
					WLAN_LOG_INDICATOR_HOST_DRIVER,
					WLAN_LOG_REASON_HB_FAILURE,
					false, false);

		lim_send_sme_deauth_ind(mac, sta, pe_session);
	}
} /*** lim_tear_down_link_with_ap() ***/

/**
 * lim_handle_heart_beat_failure() - handle hear beat failure in STA
 *
 * @mac_ctx: global MAC context
 * @session: PE session entry
 *
 * This function is called when heartbeat (beacon reception)
 * fails on STA
 *
 * Return: None
 */

void lim_handle_heart_beat_failure(struct mac_context *mac_ctx,
				   struct pe_session *session)
{
	uint8_t curr_chan;
	tpSirAddie scan_ie = NULL;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	host_log_beacon_update_pkt_type *log_ptr = NULL;
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	WLAN_HOST_DIAG_LOG_ALLOC(log_ptr, host_log_beacon_update_pkt_type,
				 LOG_WLAN_BEACON_UPDATE_C);
	if (log_ptr)
		log_ptr->bcn_rx_cnt = session->LimRxedBeaconCntDuringHB;
	WLAN_HOST_DIAG_LOG_REPORT(log_ptr);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	/* Ensure HB Status for the session has been reseted */
	session->LimHBFailureStatus = false;

	if (LIM_IS_STA_ROLE(session) &&
	    (session->limMlmState == eLIM_MLM_LINK_ESTABLISHED_STATE) &&
	    (session->limSmeState != eLIM_SME_WT_DISASSOC_STATE) &&
	    (session->limSmeState != eLIM_SME_WT_DEAUTH_STATE)) {
		if (!mac_ctx->sys.gSysEnableLinkMonitorMode) {
			goto hb_handler_fail;
		}

		/* Ignore HB if channel switch is in progress */
		if (session->gLimSpecMgmt.dot11hChanSwState ==
		   eLIM_11H_CHANSW_RUNNING) {
			pe_debug("Ignore Heartbeat failure as Channel switch is in progress");
			session->pmmOffloadInfo.bcnmiss = false;
			goto hb_handler_fail;
		}
		/* Beacon frame not received within heartbeat timeout. */
		pe_warn("Heartbeat Failure");
		mac_ctx->lim.gLimHBfailureCntInLinkEstState++;

		/*
		 * Check if connected on the DFS channel, if not connected on
		 * DFS channel then only send the probe request otherwise tear
		 * down the link
		 */
		curr_chan = wlan_reg_freq_to_chan(
					mac_ctx->pdev, session->curr_op_freq);
		if (!lim_isconnected_on_dfs_channel(mac_ctx, curr_chan)) {
			/* Detected continuous Beacon Misses */
			session->LimHBFailureStatus = true;

			/*Reset the HB packet count before sending probe*/
			limResetHBPktCount(session);
			/**
			 * Send Probe Request frame to AP to see if
			 * it is still around. Wait until certain
			 * timeout for Probe Response from AP.
			 */
			pe_debug("HB missed from AP. Sending Probe Req");
			/* for searching AP, we don't include any more IE */
			if (session->lim_join_req) {
				scan_ie = &session->lim_join_req->addIEScan;
				lim_send_probe_req_mgmt_frame(mac_ctx,
					&session->ssId,
					session->bssId, session->curr_op_freq,
					session->self_mac_addr,
					session->dot11mode,
					&scan_ie->length, scan_ie->addIEdata);
			} else {
				lim_send_probe_req_mgmt_frame(mac_ctx,
					&session->ssId,
					session->bssId, session->curr_op_freq,
					session->self_mac_addr,
					session->dot11mode, NULL, NULL);
			}
		} else {
			/*
			 * Connected on DFS channel so should not send the
			 * probe request tear down the link directly
			 */
			lim_tear_down_link_with_ap(mac_ctx,
				session->peSessionId,
				REASON_BEACON_MISSED,
				eLIM_LINK_MONITORING_DEAUTH);
		}
	} else {
		/**
		 * Heartbeat timer may have timed out
		 * while we're doing background scanning/learning
		 * or in states other than link-established state.
		 * Log error.
		 */
		pe_debug("received heartbeat timeout in state %X",
			session->limMlmState);
		lim_print_mlm_state(mac_ctx, LOG1, session->limMlmState);
		mac_ctx->lim.gLimHBfailureCntInOtherStates++;
	}

hb_handler_fail:
	if (mac_ctx->sme.tx_queue_cb)
		mac_ctx->sme.tx_queue_cb(mac_ctx->hdd_handle,
					 session->smeSessionId,
					 WLAN_WAKE_ALL_NETIF_QUEUE,
					 WLAN_CONTROL_PATH);
}

void lim_rx_invalid_peer_process(struct mac_context *mac_ctx,
				 struct scheduler_msg *lim_msg)
{
	struct ol_rx_inv_peer_params *msg =
			(struct ol_rx_inv_peer_params *)lim_msg->bodyptr;
	struct pe_session *session_entry;
	uint16_t reason_code = REASON_CLASS3_FRAME_FROM_NON_ASSOC_STA;

	if (!msg) {
		pe_err("Invalid body pointer in message");
		return;
	}

	session_entry = pe_find_session_by_vdev_id(mac_ctx, msg->vdev_id);
	if (!session_entry) {
		pe_err_rl("session not found for given sme session");
		qdf_mem_free(msg);
		return;
	}

	/* only if SAP mode */
	if (session_entry->bssType == eSIR_INFRA_AP_MODE) {
		pe_debug("send deauth frame to non-assoc STA");
		lim_send_deauth_mgmt_frame(mac_ctx,
					   reason_code,
					   msg->ta,
					   session_entry,
					   false);
	}

	qdf_mem_free(msg);
	lim_msg->bodyptr = NULL;
}

void lim_req_send_delba_ind_process(struct mac_context *mac_ctx,
				    struct scheduler_msg *lim_msg)
{
	struct lim_delba_req_info *req =
			(struct lim_delba_req_info *)lim_msg->bodyptr;
	QDF_STATUS status;
	void *dp_soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!req) {
		pe_err("Invalid body pointer in message");
		return;
	}

	status = lim_send_delba_action_frame(mac_ctx, req->vdev_id,
					     req->peer_macaddr,
					     req->tid, req->reason_code);
	if (status != QDF_STATUS_SUCCESS)
		cdp_delba_tx_completion(dp_soc, req->peer_macaddr,
					req->vdev_id, req->tid,
					WMI_MGMT_TX_COMP_TYPE_DISCARD);
	qdf_mem_free(req);
	lim_msg->bodyptr = NULL;
}
