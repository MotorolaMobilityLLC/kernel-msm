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
 *
 * This file lim_process_deauth_frame.cc contains the code
 * for processing Deauthentication Frame.
 * Author:        Chandra Modumudi
 * Date:          03/24/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "cds_api.h"
#include "ani_global.h"

#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "sch_api.h"
#include "lim_send_messages.h"

/**
 * lim_process_deauth_frame
 *
 ***FUNCTION:
 * This function is called by limProcessMessageQueue() upon
 * Deauthentication frame reception.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - A pointer to Buffer descriptor + associated PDUs
 * @return None
 */

void
lim_process_deauth_frame(struct mac_context *mac, uint8_t *pRxPacketInfo,
			 struct pe_session *pe_session)
{
	uint8_t *pBody;
	uint16_t reasonCode;
	tpSirMacMgmtHdr pHdr;
	struct pe_session *pRoamSessionEntry = NULL;
	uint8_t roamSessionId;
#ifdef WLAN_FEATURE_11W
	uint32_t frameLen;
#endif
	int32_t frame_rssi;

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);

	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	frame_rssi = (int32_t)WMA_GET_RX_RSSI_NORMALIZED(pRxPacketInfo);
#ifdef WLAN_FEATURE_11W
	frameLen = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);
	if (frameLen < sizeof(reasonCode)) {
		pe_err("Deauth Frame length invalid %d", frameLen);
		return ;
	}
#endif

	if (LIM_IS_STA_ROLE(pe_session) &&
	    ((eLIM_SME_WT_DISASSOC_STATE == pe_session->limSmeState) ||
	     (eLIM_SME_WT_DEAUTH_STATE == pe_session->limSmeState))) {
		/*Every 15th deauth frame will be logged in kmsg */
		if (!(mac->lim.deauthMsgCnt & 0xF)) {
			pe_debug("received Deauth frame in DEAUTH_WT_STATE"
				"(already processing previously received DEAUTH frame)"
				"Dropping this.. Deauth Failed %d",
				       ++mac->lim.deauthMsgCnt);
		} else {
			mac->lim.deauthMsgCnt++;
		}
		return;
	}

	if (IEEE80211_IS_MULTICAST(pHdr->sa)) {
		/* Received Deauth frame from a BC/MC address */
		/* Log error and ignore it */
		pe_debug("received Deauth frame from a BC/MC address");
		return;
	}

	if (IEEE80211_IS_MULTICAST(pHdr->da) &&
	    !QDF_IS_ADDR_BROADCAST(pHdr->da)) {
		/* Received Deauth frame for a MC address */
		/* Log error and ignore it */
		pe_debug("received Deauth frame for a MC address");
		return;
	}
	if (!lim_validate_received_frame_a1_addr(mac,
			pHdr->da, pe_session)) {
		pe_err("rx frame doesn't have valid a1 address, drop it");
		return;
	}
#ifdef WLAN_FEATURE_11W
	/* PMF: If this session is a PMF session, then ensure that this frame was protected */
	if (pe_session->limRmfEnabled &&
	    pe_session->is_key_installed &&
	    (WMA_GET_RX_DPU_FEEDBACK(pRxPacketInfo) &
		DPU_FEEDBACK_UNPROTECTED_ERROR)) {
		pe_debug("received an unprotected deauth from AP");
		/*
		 * When 11w offload is enabled then
		 * firmware should not fwd this frame
		 */
		if (LIM_IS_STA_ROLE(pe_session) && mac->pmf_offload) {
			pe_err("11w offload is enable,unprotected deauth is not expected");
			return;
		}

		/* If the frame received is unprotected, forward it to the supplicant to initiate */
		/* an SA query */

		/* send the unprotected frame indication to SME */
		lim_send_sme_unprotected_mgmt_frame_ind(mac, pHdr->fc.subType,
							(uint8_t *) pHdr,
							(frameLen +
							 sizeof(tSirMacMgmtHdr)),
							pe_session->smeSessionId,
							pe_session);
		return;
	}
#endif

	/* Get reasonCode from Deauthentication frame body */
	reasonCode = sir_read_u16(pBody);

	pe_nofl_rl_info("Deauth RX: vdev %d from "QDF_MAC_ADDR_FMT" for "QDF_MAC_ADDR_FMT" RSSI = %d reason %d mlm state = %d, sme state = %d systemrole = %d ",
			pe_session->vdev_id, QDF_MAC_ADDR_REF(pHdr->sa),
			QDF_MAC_ADDR_REF(pHdr->da), frame_rssi,
			reasonCode, pe_session->limMlmState,
			pe_session->limSmeState,
			GET_LIM_SYSTEM_ROLE(pe_session));

	lim_diag_event_report(mac, WLAN_PE_DIAG_DEAUTH_FRAME_EVENT,
		pe_session, 0, reasonCode);

	if (lim_check_disassoc_deauth_ack_pending(mac, (uint8_t *) pHdr->sa)) {
		pe_debug("Ignore the Deauth received, while waiting for ack of "
			"disassoc/deauth");
		lim_clean_up_disassoc_deauth_req(mac, (uint8_t *) pHdr->sa, 1);
		return;
	}

	if (LIM_IS_AP_ROLE(pe_session)) {
		switch (reasonCode) {
		case REASON_UNSPEC_FAILURE:
		case REASON_DEAUTH_NETWORK_LEAVING:
			/* Valid reasonCode in received Deauthentication frame */
			break;

		default:
			/* Invalid reasonCode in received Deauthentication frame */
			/* Log error and ignore the frame */
			pe_err("received Deauth frame with invalid reasonCode %d from "
				       QDF_MAC_ADDR_FMT, reasonCode,
				       QDF_MAC_ADDR_REF(pHdr->sa));

			break;
		}
	} else if (LIM_IS_STA_ROLE(pe_session)) {
		switch (reasonCode) {
		case REASON_UNSPEC_FAILURE:
		case REASON_PREV_AUTH_NOT_VALID:
		case REASON_DEAUTH_NETWORK_LEAVING:
		case REASON_CLASS2_FRAME_FROM_NON_AUTH_STA:
		case REASON_CLASS3_FRAME_FROM_NON_ASSOC_STA:
		case REASON_STA_NOT_AUTHENTICATED:
			/* Valid reasonCode in received Deauth frame */
			break;

		default:
			/* Invalid reasonCode in received Deauth frame */
			/* Log error and ignore the frame */
			pe_err("received Deauth frame with invalid reasonCode %d from "
				       QDF_MAC_ADDR_FMT, reasonCode,
				       QDF_MAC_ADDR_REF(pHdr->sa));

			break;
		}
	} else {
		/* Received Deauth frame un-known role. Log and ignore it */
		pe_err("received Deauth frame with reasonCode %d in role %d from "
			QDF_MAC_ADDR_FMT, reasonCode,
			GET_LIM_SYSTEM_ROLE(pe_session),
			QDF_MAC_ADDR_REF(pHdr->sa));

		return;
	}

	/** If we are in the middle of ReAssoc, a few things could happen:
	 *  - STA is reassociating to current AP, and receives deauth from:
	 *         a) current AP
	 *         b) other AP
	 *  - STA is reassociating to a new AP, and receives deauth from:
	 *         c) current AP
	 *         d) reassoc AP
	 *         e) other AP
	 *
	 *  The logic is:
	 *  1) If rcv deauth from an AP other than the one we're trying to
	 *     reassociate with, then drop the deauth frame (case b, c, e)
	 *  2) If rcv deauth from the "new" reassoc AP (case d), then restore
	 *     context with previous AP and send SME_REASSOC_RSP failure.
	 *  3) If rcv deauth from the reassoc AP, which is also the same
	 *     AP we're currently associated with (case a), then proceed
	 *     with normal deauth processing.
	 */
	pRoamSessionEntry =
		pe_find_session_by_bssid(mac, pe_session->limReAssocbssId,
							&roamSessionId);

	if (lim_is_reassoc_in_progress(mac, pe_session) ||
	    lim_is_reassoc_in_progress(mac, pRoamSessionEntry) ||
	    MLME_IS_ROAMING_IN_PROG(mac->psoc, pe_session->vdev_id)) {
		/*
		 * For LFR3, the roaming bssid is not known during ROAM_START,
		 * so check if the deauth is received from current AP when
		 * roaming is being done in the firmware
		 */
		if (MLME_IS_ROAMING_IN_PROG(mac->psoc, pe_session->vdev_id) &&
		    IS_CURRENT_BSSID(mac, pHdr->sa, pe_session)) {
			pe_debug("LFR3: Drop deauth frame from connected AP");
			/*
			 * recvd_deauth_while_roaming will be stored in the
			 * current AP session amd if roaming has been aborted
			 * for some reason and come back to same AP, then issue
			 * a disconnect internally if this flag is true. There
			 * is no need to reset this flag to false, because if
			 * roaming succeeds, then this session gets deleted and
			 * new session is created.
			 */
			pe_session->recvd_deauth_while_roaming = true;
			pe_session->deauth_disassoc_rc = reasonCode;
			return;
		}
		if (!IS_REASSOC_BSSID(mac, pHdr->sa, pe_session)) {
			pe_debug("Rcv Deauth from unknown/different "
				"AP while ReAssoc. Ignore " QDF_MAC_ADDR_FMT
				"limReAssocbssId : " QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(pHdr->sa),
				QDF_MAC_ADDR_REF(pe_session->limReAssocbssId));
			return;
		}

		/** Received deauth from the new AP to which we tried to ReAssociate.
		 *  Drop ReAssoc and Restore the Previous context( current connected AP).
		 */
		if (!IS_CURRENT_BSSID(mac, pHdr->sa, pe_session)) {
			pe_debug("received DeAuth from the New AP to "
				"which ReAssoc is sent " QDF_MAC_ADDR_FMT
				"pe_session->bssId: " QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(pHdr->sa),
				QDF_MAC_ADDR_REF(pe_session->bssId));

			lim_restore_pre_reassoc_state(mac,
						      eSIR_SME_REASSOC_REFUSED,
						      reasonCode,
						      pe_session);
			return;
		}
	}

	/* If received DeAuth from AP other than the one we're trying to join with
	 * nor associated with, then ignore deauth and delete Pre-auth entry.
	 */
	if (!LIM_IS_AP_ROLE(pe_session)) {
		if (!IS_CURRENT_BSSID(mac, pHdr->bssId, pe_session)) {
			pe_err("received DeAuth from an AP other "
				"than we're trying to join. Ignore. "
				QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(pHdr->sa));

			if (lim_search_pre_auth_list(mac, pHdr->sa)) {
				pe_debug("Preauth entry exist. Deleting");
				lim_delete_pre_auth_node(mac, pHdr->sa);
			}
			return;
		}
	}

	lim_extract_ies_from_deauth_disassoc(pe_session, (uint8_t *)pHdr,
					WMA_GET_RX_MPDU_LEN(pRxPacketInfo));
	lim_perform_deauth(mac, pe_session, reasonCode, pHdr->sa,
			   frame_rssi);

	if (mac->mlme_cfg->gen.fatal_event_trigger &&
	    (reasonCode != REASON_UNSPEC_FAILURE &&
	    reasonCode != REASON_DEAUTH_NETWORK_LEAVING &&
	    reasonCode != REASON_DISASSOC_NETWORK_LEAVING)) {
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
			       WLAN_LOG_INDICATOR_HOST_DRIVER,
			       WLAN_LOG_REASON_DISCONNECT,
			       false, false);
	}

} /*** end lim_process_deauth_frame() ***/

void lim_perform_deauth(struct mac_context *mac_ctx, struct pe_session *pe_session,
			uint16_t rc, tSirMacAddr addr, int32_t frame_rssi)
{
	tLimMlmDeauthInd mlmDeauthInd;
	tLimMlmAssocCnf mlmAssocCnf;
	uint16_t aid;
	tpDphHashNode sta_ds;
	tpSirAssocRsp assoc_rsp;

	sta_ds = dph_lookup_hash_entry(mac_ctx, addr, &aid,
				       &pe_session->dph.dphHashTable);
	if (!sta_ds) {
		pe_debug("Hash entry not found");
		return;
	}
	/* Check for pre-assoc states */
	switch (GET_LIM_SYSTEM_ROLE(pe_session)) {
	case eLIM_STA_ROLE:
		switch (pe_session->limMlmState) {
		case eLIM_MLM_WT_AUTH_FRAME2_STATE:
			/**
			 * AP sent Deauth frame while waiting
			 * for Auth frame2. Report Auth failure
			 * to SME.
			 */

			pe_debug("received Deauth frame state %X with failure "
				"code %d from " QDF_MAC_ADDR_FMT,
				pe_session->limMlmState, rc,
				QDF_MAC_ADDR_REF(addr));

			lim_restore_from_auth_state(mac_ctx,
				eSIR_SME_DEAUTH_WHILE_JOIN,
				rc, pe_session);

			return;

		case eLIM_MLM_AUTHENTICATED_STATE:
			pe_debug("received Deauth frame state %X with "
				"reasonCode=%d from " QDF_MAC_ADDR_FMT,
				pe_session->limMlmState, rc,
				QDF_MAC_ADDR_REF(addr));
			/* / Issue Deauth Indication to SME. */
			qdf_mem_copy((uint8_t *) &mlmDeauthInd.peerMacAddr,
				addr, sizeof(tSirMacAddr));
				mlmDeauthInd.reasonCode = rc;

			pe_session->limMlmState = eLIM_MLM_IDLE_STATE;
			MTRACE(mac_trace
				(mac_ctx, TRACE_CODE_MLM_STATE,
				 pe_session->peSessionId,
				 pe_session->limMlmState));

			lim_post_sme_message(mac_ctx,
					LIM_MLM_DEAUTH_IND,
					(uint32_t *) &mlmDeauthInd);
			return;

		case eLIM_MLM_WT_ASSOC_RSP_STATE:
			/**
			 * AP may have 'aged-out' our Pre-auth
			 * context. Delete local pre-auth context
			 * if any and issue ASSOC_CNF to SME.
			 */
			pe_debug("received Deauth frame state %X with "
				"reasonCode=%d from " QDF_MAC_ADDR_FMT,
				pe_session->limMlmState, rc,
				QDF_MAC_ADDR_REF(addr));
			if (lim_search_pre_auth_list(mac_ctx, addr))
				lim_delete_pre_auth_node(mac_ctx, addr);

			lim_stop_pmfcomeback_timer(pe_session);
			if (pe_session->pLimMlmJoinReq) {
				qdf_mem_free(pe_session->pLimMlmJoinReq);
				pe_session->pLimMlmJoinReq = NULL;
			}

			mlmAssocCnf.resultCode = eSIR_SME_DEAUTH_WHILE_JOIN;
			mlmAssocCnf.protStatusCode = rc;

			/* PE session Id */
			mlmAssocCnf.sessionId = pe_session->peSessionId;

			pe_session->limMlmState =
			pe_session->limPrevMlmState;
			MTRACE(mac_trace
				(mac_ctx, TRACE_CODE_MLM_STATE,
				 pe_session->peSessionId,
				 pe_session->limMlmState));

			/* Deactive Association response timeout */
			lim_deactivate_and_change_timer(mac_ctx,
					eLIM_ASSOC_FAIL_TIMER);

			lim_post_sme_message(mac_ctx,
					LIM_MLM_ASSOC_CNF,
			(uint32_t *) &mlmAssocCnf);

			return;

		case eLIM_MLM_WT_ADD_STA_RSP_STATE:
			pe_session->fDeauthReceived = true;
			pe_debug("Received Deauth frame in state %X with Reason "
				"Code %d from Peer" QDF_MAC_ADDR_FMT,
				pe_session->limMlmState, rc,
				QDF_MAC_ADDR_REF(addr));
			return;

		case eLIM_MLM_IDLE_STATE:
		case eLIM_MLM_LINK_ESTABLISHED_STATE:
#ifdef FEATURE_WLAN_TDLS
			if ((sta_ds)
				&& (STA_ENTRY_TDLS_PEER == sta_ds->staType)) {
				pe_err("received Deauth frame in state %X with "
					"reason code %d from Tdls peer"
					QDF_MAC_ADDR_FMT,
					pe_session->limMlmState, rc,
					QDF_MAC_ADDR_REF(addr));
			lim_send_sme_tdls_del_sta_ind(mac_ctx, sta_ds,
							pe_session,
							rc);
			return;
			} else {

			/*
			 * Delete all the TDLS peers only if Deauth
			 * is received from the AP
			 */
				if (IS_CURRENT_BSSID(mac_ctx, addr, pe_session))
					lim_delete_tdls_peers(mac_ctx, pe_session);
#endif
			/**
			 * This could be Deauthentication frame from
			 * a BSS with which pre-authentication was
			 * performed. Delete Pre-auth entry if found.
			 */
			if (lim_search_pre_auth_list(mac_ctx, addr))
				lim_delete_pre_auth_node(mac_ctx, addr);
#ifdef FEATURE_WLAN_TDLS
			}
#endif
			break;

		case eLIM_MLM_WT_REASSOC_RSP_STATE:
			pe_err("received Deauth frame state %X with "
				"reasonCode=%d from " QDF_MAC_ADDR_FMT,
				pe_session->limMlmState, rc,
				QDF_MAC_ADDR_REF(addr));
			break;

		case eLIM_MLM_WT_FT_REASSOC_RSP_STATE:
			pe_err("received Deauth frame in FT state %X with "
				"reasonCode=%d from " QDF_MAC_ADDR_FMT,
				pe_session->limMlmState, rc,
				QDF_MAC_ADDR_REF(addr));
			break;

		default:
			pe_err("received Deauth frame in state %X with "
				"reasonCode=%d from " QDF_MAC_ADDR_FMT,
				pe_session->limMlmState, rc,
				QDF_MAC_ADDR_REF(addr));
			return;
		}
		break;

	case eLIM_AP_ROLE:
		break;

	default:
		return;
	} /* end switch (mac->lim.gLimSystemRole) */

	/**
	 * Extract 'associated' context for STA, if any.
	 * This is maintained by DPH and created by LIM.
	 */
	if (!sta_ds) {
		pe_err("sta_ds is NULL");
		return;
	}

	if ((sta_ds->mlmStaContext.mlmState == eLIM_MLM_WT_DEL_STA_RSP_STATE) ||
	    (sta_ds->mlmStaContext.mlmState == eLIM_MLM_WT_DEL_BSS_RSP_STATE) ||
	    sta_ds->sta_deletion_in_progress) {
		/**
		 * Already in the process of deleting context for the peer
		 * and received Deauthentication frame. Log and Ignore.
		 */
		pe_debug("Deletion is in progress (%d) for peer:"QDF_MAC_ADDR_FMT" in mlmState %d",
			 sta_ds->sta_deletion_in_progress,
			 QDF_MAC_ADDR_REF(addr),
			 sta_ds->mlmStaContext.mlmState);
		return;
	}
	sta_ds->mlmStaContext.disassocReason = rc;
	sta_ds->mlmStaContext.cleanupTrigger = eLIM_PEER_ENTITY_DEAUTH;
	sta_ds->sta_deletion_in_progress = true;

	/* / Issue Deauth Indication to SME. */
	qdf_mem_copy((uint8_t *) &mlmDeauthInd.peerMacAddr,
			sta_ds->staAddr, sizeof(tSirMacAddr));
	mlmDeauthInd.reasonCode =
		(uint8_t) sta_ds->mlmStaContext.disassocReason;
	mlmDeauthInd.deauthTrigger = eLIM_PEER_ENTITY_DEAUTH;

	/*
	 * If we're in the middle of ReAssoc and received deauth from
	 * the ReAssoc AP, then notify SME by sending REASSOC_RSP with
	 * failure result code. SME will post the disconnect to the
	 * supplicant and the latter would start a fresh assoc.
	 */
	if (lim_is_reassoc_in_progress(mac_ctx, pe_session)) {
		/**
		 * AP may have 'aged-out' our Pre-auth
		 * context. Delete local pre-auth context
		 * if any and issue REASSOC_CNF to SME.
		 */
		if (lim_search_pre_auth_list(mac_ctx, addr))
			lim_delete_pre_auth_node(mac_ctx, addr);

		if (pe_session->limAssocResponseData) {
			assoc_rsp = (tpSirAssocRsp) pe_session->
					limAssocResponseData;
			qdf_mem_free(assoc_rsp->sha384_ft_subelem.gtk);
			qdf_mem_free(assoc_rsp->sha384_ft_subelem.igtk);
			qdf_mem_free(pe_session->limAssocResponseData);
			pe_session->limAssocResponseData = NULL;
		}

		pe_debug("Rcv Deauth from ReAssoc AP Issue REASSOC_CNF");
		/*
		 * TODO: Instead of overloading eSIR_SME_FT_REASSOC_TIMEOUT_FAILURE
		 * it would have been good to define/use a different failure type.
		 * Using eSIR_SME_FT_REASSOC_FAILURE does not seem to clean-up
		 * properly and we end up seeing "transmit queue timeout".
		 */
		lim_post_reassoc_failure(mac_ctx,
				eSIR_SME_FT_REASSOC_TIMEOUT_FAILURE,
				STATUS_UNSPECIFIED_FAILURE,
				pe_session);
		return;
	}
	/* reset the deauthMsgCnt here since we are able to Process
	 * the deauth frame and sending up the indication as well */
	if (mac_ctx->lim.deauthMsgCnt != 0) {
		mac_ctx->lim.deauthMsgCnt = 0;
	}
	if (LIM_IS_STA_ROLE(pe_session))
		wma_tx_abort(pe_session->smeSessionId);

	lim_update_lost_link_info(mac_ctx, pe_session, frame_rssi);

	/* / Deauthentication from peer MAC entity */
	if (LIM_IS_STA_ROLE(pe_session))
		lim_post_sme_message(mac_ctx, LIM_MLM_DEAUTH_IND,
				(uint32_t *) &mlmDeauthInd);

	/* send eWNI_SME_DEAUTH_IND to SME */
	lim_send_sme_deauth_ind(mac_ctx, sta_ds, pe_session);
	return;

}
