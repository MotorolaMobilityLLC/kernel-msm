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
 * This file lim_process_disassoc_frame.cc contains the code
 * for processing Disassocation Frame.
 * Author:        Chandra Modumudi
 * Date:          03/24/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "cds_api.h"
#include "wni_api.h"
#include "sir_api.h"
#include "ani_global.h"
#include "wni_cfg.h"

#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_send_messages.h"
#include "sch_api.h"
#include "wlan_blm_api.h"

/**
 * lim_process_disassoc_frame
 *
 ***FUNCTION:
 * This function is called by limProcessMessageQueue() upon
 * Disassociation frame reception.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * DPH drops packets for STA with 'valid' bit in sta set to '0'.
 *
 ***NOTE:
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  *pRxPacketInfo - A pointer to Rx packet info structure
 * @return None
 */
void
lim_process_disassoc_frame(struct mac_context *mac, uint8_t *pRxPacketInfo,
			   struct pe_session *pe_session)
{
	uint8_t *pBody;
	uint16_t aid, reasonCode;
	tpSirMacMgmtHdr pHdr;
	tpDphHashNode sta;
	uint32_t frame_len;
	int32_t frame_rssi;

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo);

	frame_rssi = (int32_t)WMA_GET_RX_RSSI_NORMALIZED(pRxPacketInfo);

	if (IEEE80211_IS_MULTICAST(pHdr->sa)) {
		/* Received Disassoc frame from a BC/MC address */
		/* Log error and ignore it */
		pe_err("received Disassoc frame from a BC/MC address");
		return;
	}

	if (IEEE80211_IS_MULTICAST(pHdr->da) &&
	    !QDF_IS_ADDR_BROADCAST(pHdr->da)) {
		/* Received Disassoc frame for a MC address */
		/* Log error and ignore it */
		pe_err("received Disassoc frame for a MC address");
		return;
	}
	if (!lim_validate_received_frame_a1_addr(mac,
			pHdr->da, pe_session)) {
		pe_err("rx frame doesn't have valid a1 address, drop it");
		return;
	}

	if (LIM_IS_STA_ROLE(pe_session) &&
		((eLIM_SME_WT_DISASSOC_STATE == pe_session->limSmeState) ||
		(eLIM_SME_WT_DEAUTH_STATE == pe_session->limSmeState))) {
		if (!(mac->lim.disassocMsgCnt & 0xF)) {
			pe_debug("received Disassoc frame in %s"
				"already processing previously received Disassoc frame, dropping this %d",
				 lim_sme_state_str(pe_session->limSmeState),
				 ++mac->lim.disassocMsgCnt);
		} else {
			mac->lim.disassocMsgCnt++;
		}
		return;
	}
#ifdef WLAN_FEATURE_11W
	/* PMF: If this session is a PMF session, then ensure that this frame was protected */
	if (pe_session->limRmfEnabled
	    && (WMA_GET_RX_DPU_FEEDBACK(pRxPacketInfo) &
		DPU_FEEDBACK_UNPROTECTED_ERROR)) {
		pe_err("received an unprotected disassoc from AP");
		/*
		 * When 11w offload is enabled then
		 * firmware should not fwd this frame
		 */
		if (LIM_IS_STA_ROLE(pe_session) &&  mac->pmf_offload) {
			pe_err("11w offload is enable,unprotected disassoc is not expected");
			return;
		}

		/* If the frame received is unprotected, forward it to the supplicant to initiate */
		/* an SA query */
		/* send the unprotected frame indication to SME */
		lim_send_sme_unprotected_mgmt_frame_ind(mac, pHdr->fc.subType,
							(uint8_t *) pHdr,
							(frame_len +
							 sizeof(tSirMacMgmtHdr)),
							pe_session->smeSessionId,
							pe_session);
		return;
	}
#endif

	if (frame_len < 2) {
		pe_err("frame len less than 2");
		return;
	}

	/* Get reasonCode from Disassociation frame body */
	reasonCode = sir_read_u16(pBody);

	pe_nofl_rl_info("Disassoc RX: vdev %d from "QDF_MAC_ADDR_FMT" for "QDF_MAC_ADDR_FMT" RSSI = %d reason %d mlm state = %d, sme state = %d systemrole = %d ",
			pe_session->vdev_id, QDF_MAC_ADDR_REF(pHdr->sa),
			QDF_MAC_ADDR_REF(pHdr->da), frame_rssi,
			reasonCode, pe_session->limMlmState,
			pe_session->limSmeState,
			GET_LIM_SYSTEM_ROLE(pe_session));
	lim_diag_event_report(mac, WLAN_PE_DIAG_DISASSOC_FRAME_EVENT,
		pe_session, 0, reasonCode);

	/**
	 * Extract 'associated' context for STA, if any.
	 * This is maintained by DPH and created by LIM.
	 */
	sta =
		dph_lookup_hash_entry(mac, pHdr->sa, &aid,
				      &pe_session->dph.dphHashTable);

	if (!sta) {
		/**
		 * Disassociating STA is not associated.
		 * Log error.
		 */
		pe_err("received Disassoc frame from STA that does not have context"
			"reasonCode=%d, addr " QDF_MAC_ADDR_FMT,
			reasonCode, QDF_MAC_ADDR_REF(pHdr->sa));
		return;
	}

	if (lim_check_disassoc_deauth_ack_pending(mac, (uint8_t *) pHdr->sa)) {
		pe_err("Ignore the DisAssoc received, while waiting for ack of disassoc/deauth");
		lim_clean_up_disassoc_deauth_req(mac, (uint8_t *) pHdr->sa, 1);
		return;
	}

	if (mac->lim.disassocMsgCnt != 0) {
		mac->lim.disassocMsgCnt = 0;
	}

	/** If we are in the Wait for ReAssoc Rsp state */
	if (lim_is_reassoc_in_progress(mac, pe_session)) {
		/*
		 * For LFR3, the roaming bssid is not known during ROAM_START,
		 * so check if the disassoc is received from current AP when
		 * roaming is being done in the firmware
		 */
		if (MLME_IS_ROAMING_IN_PROG(mac->psoc, pe_session->vdev_id) &&
		    IS_CURRENT_BSSID(mac, pHdr->sa, pe_session)) {
			pe_debug("Dropping disassoc frame from connected AP");
			pe_session->recvd_disassoc_while_roaming = true;
			pe_session->deauth_disassoc_rc = reasonCode;
			return;
		}
		/** If we had received the DisAssoc from,
		 *     a. the Current AP during ReAssociate to different AP in same ESS
		 *     b. Unknown AP
		 *   drop/ignore the DisAssoc received
		 */
		if (!IS_REASSOC_BSSID(mac, pHdr->sa, pe_session)) {
			pe_err("Ignore DisAssoc while Processing ReAssoc");
			return;
		}
		/** If the Disassoc is received from the new AP to which we tried to ReAssociate
		 *  Drop ReAssoc and Restore the Previous context( current connected AP).
		 */
		if (!IS_CURRENT_BSSID(mac, pHdr->sa, pe_session)) {
			pe_debug("received Disassoc from the New AP to which ReAssoc is sent");
			lim_restore_pre_reassoc_state(mac,
						      eSIR_SME_REASSOC_REFUSED,
						      reasonCode,
						      pe_session);
			return;
		}
	}

	if (LIM_IS_AP_ROLE(pe_session)) {
		switch (reasonCode) {
		case REASON_UNSPEC_FAILURE:
		case REASON_DISASSOC_DUE_TO_INACTIVITY:
		case REASON_DISASSOC_NETWORK_LEAVING:
		case REASON_MIC_FAILURE:
		case REASON_4WAY_HANDSHAKE_TIMEOUT :
		case REASON_GROUP_KEY_UPDATE_TIMEOUT:
		case REASON_IN_4WAY_DIFFERS:
		case REASON_1X_AUTH_FAILURE:
			/* Valid reasonCode in received Disassociation frame */
			break;

		default:
			/* Invalid reasonCode in received Disassociation frame */
			pe_warn("received Disassoc frame with invalid reasonCode: %d from " QDF_MAC_ADDR_FMT,
				reasonCode, QDF_MAC_ADDR_REF(pHdr->sa));
			break;
		}
	} else if (LIM_IS_STA_ROLE(pe_session) &&
		   ((pe_session->limSmeState != eLIM_SME_WT_JOIN_STATE) &&
		   (pe_session->limSmeState != eLIM_SME_WT_AUTH_STATE) &&
		   (pe_session->limSmeState != eLIM_SME_WT_ASSOC_STATE) &&
		   (pe_session->limSmeState != eLIM_SME_WT_REASSOC_STATE))) {
		switch (reasonCode) {
		case REASON_DEAUTH_NETWORK_LEAVING:
		case REASON_DISASSOC_NETWORK_LEAVING:
		case REASON_POOR_RSSI_CONDITIONS:
			/* Valid reasonCode in received Disassociation frame */
			/* as long as we're not about to channel switch */
			if (pe_session->gLimChannelSwitch.state !=
			    eLIM_CHANNEL_SWITCH_IDLE) {
				pe_err("Ignoring disassoc frame due to upcoming channel switch, from "QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(pHdr->sa));
				return;
			}
			break;

		default:
			break;
		}
	} else {
		/* Received Disassoc in un-known role. Log and ignore it */
		pe_err("received Disassoc frame with invalid reasonCode: %d in role:"
				"%d in sme state: %d from " QDF_MAC_ADDR_FMT, reasonCode,
			GET_LIM_SYSTEM_ROLE(pe_session), pe_session->limSmeState,
			QDF_MAC_ADDR_REF(pHdr->sa));

		return;
	}

	if ((sta->mlmStaContext.mlmState == eLIM_MLM_WT_DEL_STA_RSP_STATE) ||
	    (sta->mlmStaContext.mlmState == eLIM_MLM_WT_DEL_BSS_RSP_STATE) ||
	    sta->sta_deletion_in_progress) {
		/**
		 * Already in the process of deleting context for the peer
		 * and received Disassociation frame. Log and Ignore.
		 */
		pe_debug("Deletion is in progress (%d) for peer:"QDF_MAC_ADDR_FMT" in mlmState %d",
			 sta->sta_deletion_in_progress,
			 QDF_MAC_ADDR_REF(pHdr->sa),
			 sta->mlmStaContext.mlmState);
		return;
	}
	sta->sta_deletion_in_progress = true;
	lim_disassoc_tdls_peers(mac, pe_session, pHdr->sa);
	if (sta->mlmStaContext.mlmState != eLIM_MLM_LINK_ESTABLISHED_STATE) {
		/**
		 * Requesting STA is in some 'transient' state?
		 * Log error.
		 */
		if (sta->mlmStaContext.mlmState ==
		    eLIM_MLM_WT_ASSOC_CNF_STATE)
			sta->mlmStaContext.updateContext = 1;

		pe_err("received Disassoc frame from peer that is in state: %X, addr "QDF_MAC_ADDR_FMT,
			sta->mlmStaContext.mlmState,
			       QDF_MAC_ADDR_REF(pHdr->sa));

	} /* if (sta->mlmStaContext.mlmState != eLIM_MLM_LINK_ESTABLISHED_STATE) */

	if (reasonCode == REASON_POOR_RSSI_CONDITIONS) {
		struct sir_rssi_disallow_lst ap_info = {{0}};

		ap_info.retry_delay = 0;
		ap_info.expected_rssi = frame_rssi +
			wlan_blm_get_rssi_blacklist_threshold(mac->pdev);
		qdf_mem_copy(ap_info.bssid.bytes, pHdr->sa, QDF_MAC_ADDR_SIZE);
		ap_info.reject_reason = REASON_ASSOC_REJECT_POOR_RSSI;
		ap_info.source = ADDED_BY_DRIVER;
		ap_info.original_timeout = ap_info.retry_delay;
		ap_info.received_time = qdf_mc_timer_get_system_time();

		lim_add_bssid_to_reject_list(mac->pdev, &ap_info);
	}
	lim_extract_ies_from_deauth_disassoc(pe_session, (uint8_t *)pHdr,
					WMA_GET_RX_MPDU_LEN(pRxPacketInfo));
	lim_perform_disassoc(mac, frame_rssi, reasonCode,
			     pe_session, pHdr->sa);

	if (mac->mlme_cfg->gen.fatal_event_trigger &&
	    (reasonCode != REASON_UNSPEC_FAILURE &&
	    reasonCode != REASON_DEAUTH_NETWORK_LEAVING &&
	    reasonCode != REASON_DISASSOC_NETWORK_LEAVING)) {
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
			       WLAN_LOG_INDICATOR_HOST_DRIVER,
			       WLAN_LOG_REASON_DISCONNECT,
			       false, false);
	}
} /*** end lim_process_disassoc_frame() ***/

#ifdef FEATURE_WLAN_TDLS
void lim_disassoc_tdls_peers(struct mac_context *mac_ctx,
				    struct pe_session *pe_session, tSirMacAddr addr)
{
	tpDphHashNode sta_ds;
	uint16_t aid;

	sta_ds = dph_lookup_hash_entry(mac_ctx, addr, &aid,
				       &pe_session->dph.dphHashTable);
	if (!sta_ds) {
		pe_debug("Hash entry not found");
		return;
	}
	/**
	 *  Delete all the TDLS peers only if Disassoc is received
	 *  from the AP
	 */
	if ((LIM_IS_STA_ROLE(pe_session)) &&
	    ((sta_ds->mlmStaContext.mlmState ==
	      eLIM_MLM_LINK_ESTABLISHED_STATE) ||
	     (sta_ds->mlmStaContext.mlmState ==
	      eLIM_MLM_IDLE_STATE)) &&
	    (IS_CURRENT_BSSID(mac_ctx, addr, pe_session)))
		lim_delete_tdls_peers(mac_ctx, pe_session);
}
#endif

void lim_perform_disassoc(struct mac_context *mac_ctx, int32_t frame_rssi,
			  uint16_t rc, struct pe_session *pe_session, tSirMacAddr addr)
{
	tLimMlmDisassocInd mlmDisassocInd;
	uint16_t aid;
	tpDphHashNode sta_ds;
	tpSirAssocRsp assoc_rsp;

	sta_ds = dph_lookup_hash_entry(mac_ctx, addr, &aid,
				       &pe_session->dph.dphHashTable);
	if (!sta_ds) {
		pe_debug("Hash entry not found");
		return;
	}
	sta_ds->mlmStaContext.cleanupTrigger = eLIM_PEER_ENTITY_DISASSOC;
	sta_ds->mlmStaContext.disassocReason = rc;

	/* Issue Disassoc Indication to SME. */
	qdf_mem_copy((uint8_t *) &mlmDisassocInd.peerMacAddr,
			(uint8_t *) sta_ds->staAddr, sizeof(tSirMacAddr));
	mlmDisassocInd.reasonCode =
		(uint8_t) sta_ds->mlmStaContext.disassocReason;
	mlmDisassocInd.disassocTrigger = eLIM_PEER_ENTITY_DISASSOC;

	/* Update PE session Id  */
	mlmDisassocInd.sessionId = pe_session->peSessionId;
	if (LIM_IS_STA_ROLE(pe_session) &&
	    (pe_session->limMlmState == eLIM_MLM_WT_ASSOC_RSP_STATE))
		lim_stop_pmfcomeback_timer(pe_session);

	if (lim_is_reassoc_in_progress(mac_ctx, pe_session)) {

		/* If we're in the middle of ReAssoc and received disassoc from
		 * the ReAssoc AP, then notify SME by sending REASSOC_RSP with
		 * failure result code. By design, SME will then issue "Disassoc"
		 * and cleanup will happen at that time.
		 */
		pe_debug("received Disassoc from AP while waiting for Reassoc Rsp");

		if (pe_session->limAssocResponseData) {
			assoc_rsp = (tpSirAssocRsp) pe_session->
						limAssocResponseData;
			qdf_mem_free(assoc_rsp->sha384_ft_subelem.gtk);
			qdf_mem_free(assoc_rsp->sha384_ft_subelem.igtk);
			qdf_mem_free(pe_session->limAssocResponseData);
			pe_session->limAssocResponseData = NULL;
		}

		lim_restore_pre_reassoc_state(mac_ctx, eSIR_SME_REASSOC_REFUSED,
				rc, pe_session);
		return;
	}

	lim_update_lost_link_info(mac_ctx, pe_session, frame_rssi);
	lim_post_sme_message(mac_ctx, LIM_MLM_DISASSOC_IND,
			(uint32_t *) &mlmDisassocInd);

	/* send eWNI_SME_DISASSOC_IND to SME */
	lim_send_sme_disassoc_ind(mac_ctx, sta_ds, pe_session);

	return;
}
