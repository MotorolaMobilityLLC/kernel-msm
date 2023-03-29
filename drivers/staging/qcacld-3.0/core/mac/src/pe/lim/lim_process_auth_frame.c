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
 *
 * This file lim_process_auth_frame.cc contains the code
 * for processing received Authentication Frame.
 * Author:        Chandra Modumudi
 * Date:          03/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 * 05/12/2010     js             To support Shared key authentication at AP side
 *
 */

#include "wni_api.h"
#include "wni_cfg.h"
#include "ani_global.h"
#include "cfg_ucfg_api.h"
#include "utils_api.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_ft.h"
#include "cds_utils.h"
#include "lim_send_messages.h"
#include "lim_process_fils.h"
#include "wlan_mlme_api.h"

/**
 * is_auth_valid
 *
 ***FUNCTION:
 * This function is called by lim_process_auth_frame() upon Authentication
 * frame reception.
 *
 ***LOGIC:
 * This function is used to test validity of auth frame:
 * - AUTH1 and AUTH3 must be received in AP mode
 * - AUTH2 and AUTH4 must be received in STA mode
 * - AUTH3 and AUTH4 must have challenge text IE, that is,'type' field has been set to
 *                 WLAN_ELEMID_CHALLENGE by parser
 * -
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  *auth - Pointer to extracted auth frame body
 *
 * @return 0 or 1 (Valid)
 */

static inline unsigned int is_auth_valid(struct mac_context *mac,
					 tpSirMacAuthFrameBody auth,
					 struct pe_session *pe_session)
{
	unsigned int valid = 1;

	if (((auth->authTransactionSeqNumber == SIR_MAC_AUTH_FRAME_1) ||
	    (auth->authTransactionSeqNumber == SIR_MAC_AUTH_FRAME_3)) &&
	    (LIM_IS_STA_ROLE(pe_session)))
		valid = 0;

	if (((auth->authTransactionSeqNumber == SIR_MAC_AUTH_FRAME_2) ||
	    (auth->authTransactionSeqNumber == SIR_MAC_AUTH_FRAME_4)) &&
	    (LIM_IS_AP_ROLE(pe_session)))
		valid = 0;

	if (((auth->authTransactionSeqNumber == SIR_MAC_AUTH_FRAME_3) ||
	    (auth->authTransactionSeqNumber == SIR_MAC_AUTH_FRAME_4)) &&
	    (auth->type != WLAN_ELEMID_CHALLENGE) &&
	    (auth->authAlgoNumber != eSIR_SHARED_KEY))
		valid = 0;

	return valid;
}

/**
 * lim_get_wep_key_sap() - get sap's wep key for shared wep auth
 * @pe_session: pointer to pe session
 * @wep_params: pointer to wlan_mlme_wep_cfg
 * @key_id: key id
 * @default_key: output of the key
 * @key_len: output of ket length
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS lim_get_wep_key_sap(struct pe_session *pe_session,
				      struct wlan_mlme_wep_cfg *wep_params,
				      uint8_t key_id,
				      uint8_t *default_key,
				      qdf_size_t *key_len)
{
	return mlme_get_wep_key(pe_session->vdev,
				wep_params,
				(MLME_WEP_DEFAULT_KEY_1 + key_id),
				default_key,
				key_len);
}

static void lim_process_auth_shared_system_algo(struct mac_context *mac_ctx,
		tpSirMacMgmtHdr mac_hdr,
		tSirMacAuthFrameBody *rx_auth_frm_body,
		tSirMacAuthFrameBody *auth_frame,
		struct pe_session *pe_session)
{
	uint32_t val;
	uint8_t cfg_privacy_opt_imp;
	struct tLimPreAuthNode *auth_node;
	uint8_t challenge_txt_arr[SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH] = {0};

	pe_debug("=======> eSIR_SHARED_KEY");
	if (LIM_IS_AP_ROLE(pe_session))
		val = pe_session->privacy;
	else
		val = mac_ctx->mlme_cfg->wep_params.is_privacy_enabled;

	cfg_privacy_opt_imp = (uint8_t) val;
	if (!cfg_privacy_opt_imp) {
		pe_err("rx Auth frame for unsupported auth algorithm %d "
			QDF_MAC_ADDR_FMT,
			rx_auth_frm_body->authAlgoNumber,
			QDF_MAC_ADDR_REF(mac_hdr->sa));

		/*
		 * Authenticator does not have WEP
		 * implemented.
		 * Reject by sending Authentication frame
		 * with Auth algorithm not supported status
		 * code.
		 */
		auth_frame->authAlgoNumber = rx_auth_frm_body->authAlgoNumber;
		auth_frame->authTransactionSeqNumber =
			rx_auth_frm_body->authTransactionSeqNumber + 1;
		auth_frame->authStatusCode =
			STATUS_NOT_SUPPORTED_AUTH_ALG;

		lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
		return;
	} else {
		/* Create entry for this STA in pre-auth list */
		auth_node = lim_acquire_free_pre_auth_node(mac_ctx,
					&mac_ctx->lim.gLimPreAuthTimerTable);
		if (!auth_node) {
			pe_warn("Max preauth-nodes reached");
			lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGW);
			return;
		}

		qdf_mem_copy((uint8_t *) auth_node->peerMacAddr, mac_hdr->sa,
				sizeof(tSirMacAddr));
		auth_node->mlmState = eLIM_MLM_WT_AUTH_FRAME3_STATE;
		auth_node->authType =
			(tAniAuthType) rx_auth_frm_body->authAlgoNumber;
		auth_node->fSeen = 0;
		auth_node->fTimerStarted = 0;
		auth_node->seq_num = ((mac_hdr->seqControl.seqNumHi << 4) |
						(mac_hdr->seqControl.seqNumLo));
		auth_node->timestamp = qdf_mc_timer_get_system_ticks();
		lim_add_pre_auth_node(mac_ctx, auth_node);

		pe_debug("Alloc new data: %pK id: %d peer ",
			auth_node, auth_node->authNodeIdx);
		lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGD);
		/* / Create and activate Auth Response timer */
		if (tx_timer_change_context(&auth_node->timer,
				auth_node->authNodeIdx) != TX_SUCCESS) {
			/* Could not start Auth response timer. Log error */
			pe_warn("Unable to chg context auth response timer for peer");
			lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGW);

			/*
			 * Send Auth frame with unspecified failure status code.
			 */

			auth_frame->authAlgoNumber =
				rx_auth_frm_body->authAlgoNumber;
			auth_frame->authTransactionSeqNumber =
				rx_auth_frm_body->authTransactionSeqNumber + 1;
			auth_frame->authStatusCode =
				STATUS_UNSPECIFIED_FAILURE;

			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			lim_delete_pre_auth_node(mac_ctx, mac_hdr->sa);
			return;
		}

		/*
		 * get random bytes and use as challenge text.
		 */
		get_random_bytes(challenge_txt_arr,
				 SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH);
		qdf_mem_zero(auth_node->challengeText,
			     SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH);
		if (!qdf_mem_cmp(challenge_txt_arr,
				 auth_node->challengeText,
				 SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH)) {
			pe_err("Challenge text preparation failed");
			lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGW);
			auth_frame->authAlgoNumber =
				rx_auth_frm_body->authAlgoNumber;
			auth_frame->authTransactionSeqNumber =
				rx_auth_frm_body->authTransactionSeqNumber + 1;
			auth_frame->authStatusCode =
				STATUS_ASSOC_REJECTED_TEMPORARILY;
			lim_send_auth_mgmt_frame(mac_ctx,
						 auth_frame,
						 mac_hdr->sa,
						 LIM_NO_WEP_IN_FC,
						 pe_session);
			lim_delete_pre_auth_node(mac_ctx, mac_hdr->sa);
			return;
		}

		lim_activate_auth_rsp_timer(mac_ctx, auth_node);
		auth_node->fTimerStarted = 1;

		qdf_mem_copy(auth_node->challengeText,
			     challenge_txt_arr,
			     sizeof(challenge_txt_arr));
		/*
		 * Sending Authenticaton frame with challenge.
		 */
		auth_frame->authAlgoNumber = rx_auth_frm_body->authAlgoNumber;
		auth_frame->authTransactionSeqNumber =
			rx_auth_frm_body->authTransactionSeqNumber + 1;
		auth_frame->authStatusCode = STATUS_SUCCESS;
		auth_frame->type = WLAN_ELEMID_CHALLENGE;
		auth_frame->length = SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH;
		qdf_mem_copy(auth_frame->challengeText,
				auth_node->challengeText,
				SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH);
		lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
	}
}

static void lim_process_auth_open_system_algo(struct mac_context *mac_ctx,
		tpSirMacMgmtHdr mac_hdr,
		tSirMacAuthFrameBody *rx_auth_frm_body,
		tSirMacAuthFrameBody *auth_frame,
		struct pe_session *pe_session)
{
	struct tLimPreAuthNode *auth_node;

	pe_debug("=======> eSIR_OPEN_SYSTEM");
	/* Create entry for this STA in pre-auth list */
	auth_node = lim_acquire_free_pre_auth_node(mac_ctx,
				&mac_ctx->lim.gLimPreAuthTimerTable);
	if (!auth_node) {
		pe_warn("Max pre-auth nodes reached ");
		lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGW);
		return;
	}
	pe_debug("Alloc new data: %pK peer", auth_node);
	lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGD);
	qdf_mem_copy((uint8_t *) auth_node->peerMacAddr,
			mac_hdr->sa, sizeof(tSirMacAddr));
	auth_node->mlmState = eLIM_MLM_AUTHENTICATED_STATE;
	auth_node->authType = (tAniAuthType) rx_auth_frm_body->authAlgoNumber;
	auth_node->fSeen = 0;
	auth_node->fTimerStarted = 0;
	auth_node->seq_num = ((mac_hdr->seqControl.seqNumHi << 4) |
				(mac_hdr->seqControl.seqNumLo));
	auth_node->timestamp = qdf_mc_timer_get_system_ticks();
	lim_add_pre_auth_node(mac_ctx, auth_node);
	/*
	 * Send Authenticaton frame with Success
	 * status code.
	 */
	auth_frame->authAlgoNumber = rx_auth_frm_body->authAlgoNumber;
	auth_frame->authTransactionSeqNumber =
			rx_auth_frm_body->authTransactionSeqNumber + 1;
	auth_frame->authStatusCode = STATUS_SUCCESS;
	lim_send_auth_mgmt_frame(mac_ctx, auth_frame, mac_hdr->sa,
					LIM_NO_WEP_IN_FC,
					pe_session);
}

#ifdef WLAN_FEATURE_SAE

/**
 * lim_external_auth_add_pre_auth_node()- Add preauth node for the peer
 *					  performing external authentication
 * @mac_ctx: MAC context
 * @mac_hdr: Mac header of the packet
 * @mlm_state: MLM state to be marked to track SAE authentication
 *
 * Return: None
 */
static void lim_external_auth_add_pre_auth_node(struct mac_context *mac_ctx,
						tpSirMacMgmtHdr mac_hdr,
						tLimMlmStates mlm_state)
{
	struct tLimPreAuthNode *auth_node;
	tpLimPreAuthTable preauth_table = &mac_ctx->lim.gLimPreAuthTimerTable;

	pe_debug("=======> eSIR_AUTH_TYPE_SAE");
	/* Create entry for this STA in pre-auth list */
	auth_node = lim_acquire_free_pre_auth_node(mac_ctx, preauth_table);
	if (!auth_node) {
		pe_debug("Max pre-auth nodes reached " QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(mac_hdr->sa));
		return;
	}
	pe_debug("Creating preauth node for SAE peer " QDF_MAC_ADDR_FMT,
		 QDF_MAC_ADDR_REF(mac_hdr->sa));
	qdf_mem_copy((uint8_t *)auth_node->peerMacAddr,
		     mac_hdr->sa, sizeof(tSirMacAddr));
	auth_node->mlmState = mlm_state;
	auth_node->authType = eSIR_AUTH_TYPE_SAE;
	auth_node->timestamp = qdf_mc_timer_get_system_ticks();
	auth_node->seq_num = ((mac_hdr->seqControl.seqNumHi << 4) |
			      (mac_hdr->seqControl.seqNumLo));
	auth_node->assoc_req.present = false;
	lim_add_pre_auth_node(mac_ctx, auth_node);
}

void lim_sae_auth_cleanup_retry(struct mac_context *mac_ctx,
				uint8_t vdev_id)
{
	struct pe_session *pe_session;

	pe_session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!pe_session) {
		pe_err("session not found for given vdev_id %d", vdev_id);
		return;
	}

	pe_debug("sae auth cleanup for vdev_id %d", vdev_id);
	lim_deactivate_and_change_timer(mac_ctx, eLIM_AUTH_RETRY_TIMER);
	mlme_free_sae_auth_retry(pe_session->vdev);
}

#define SAE_AUTH_ALGO_BYTES 2
#define SAE_AUTH_SEQ_NUM_BYTES 2
#define SAE_AUTH_SEQ_OFFSET 1

/**
 * lim_is_sae_auth_algo_match()- Match SAE auth seq in queued SAE auth and
 * SAE auth rx frame
 * @queued_frame: Pointer to queued SAE auth retry frame
 * @q_len: length of queued sae auth retry frame
 * @rx_pkt_info: Rx packet
 *
 * Return: True if SAE auth seq is mached else false
 */
static bool lim_is_sae_auth_algo_match(uint8_t *queued_frame, uint16_t q_len,
				       uint8_t *rx_pkt_info)
{
	tpSirMacMgmtHdr qmac_hdr = (tpSirMacMgmtHdr)queued_frame;
	uint16_t *rxbody_ptr, *qbody_ptr, rxframe_len, min_len;

	min_len = sizeof(tSirMacMgmtHdr) + SAE_AUTH_ALGO_BYTES +
			SAE_AUTH_SEQ_NUM_BYTES;

	rxframe_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);
	if (rxframe_len < min_len || q_len < min_len) {
		pe_debug("rxframe_len %d, queued_frame_len %d, min_len %d",
			 rxframe_len, q_len, min_len);
		return false;
	}

	rxbody_ptr = (uint16_t *)WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	qbody_ptr = (uint16_t *)((uint8_t *)qmac_hdr + sizeof(tSirMacMgmtHdr));

	pe_debug("sae_auth : rx pkt auth seq %d queued pkt auth seq %d",
		 rxbody_ptr[SAE_AUTH_SEQ_OFFSET],
		 qbody_ptr[SAE_AUTH_SEQ_OFFSET]);
	if (rxbody_ptr[SAE_AUTH_SEQ_OFFSET] ==
	    qbody_ptr[SAE_AUTH_SEQ_OFFSET])
		return true;

	return false;
}

/**
 * lim_process_sae_auth_frame()-Process SAE authentication frame
 * @mac_ctx: MAC context
 * @rx_pkt_info: Rx packet
 * @pe_session: PE session
 *
 * Return: None
 */
static void lim_process_sae_auth_frame(struct mac_context *mac_ctx,
		uint8_t *rx_pkt_info, struct pe_session *pe_session)
{
	tpSirMacMgmtHdr mac_hdr;
	uint32_t frame_len;
	uint8_t *body_ptr;
	enum rxmgmt_flags rx_flags = RXMGMT_FLAG_NONE;
	struct sae_auth_retry *sae_retry;

	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	pe_nofl_rl_info("SAE Auth RX type %d subtype %d from "QDF_MAC_ADDR_FMT,
			mac_hdr->fc.type, mac_hdr->fc.subType,
			QDF_MAC_ADDR_REF(mac_hdr->sa));

	if (LIM_IS_STA_ROLE(pe_session) &&
	    pe_session->limMlmState != eLIM_MLM_WT_SAE_AUTH_STATE)
		pe_err("SAE auth response for STA in unexpected state %x",
		       pe_session->limMlmState);

	if (LIM_IS_AP_ROLE(pe_session)) {
		struct tLimPreAuthNode *sta_pre_auth_ctx;

		rx_flags = RXMGMT_FLAG_EXTERNAL_AUTH;
		/* Add preauth node when the first SAE authentication frame
		 * is received and mark state as authenticating.
		 * It's not good to track SAE authentication frames with
		 * authTransactionSeqNumber as it's subjected to
		 * SAE protocol optimizations.
		 */
		/* Extract pre-auth context for the STA, if any. */
		sta_pre_auth_ctx = lim_search_pre_auth_list(mac_ctx,
							    mac_hdr->sa);
		if (!sta_pre_auth_ctx ||
		    (sta_pre_auth_ctx->mlmState != eLIM_MLM_WT_SAE_AUTH_STATE &&
		     sta_pre_auth_ctx->mlmState !=
		     eLIM_MLM_AUTHENTICATED_STATE)) {
			lim_external_auth_add_pre_auth_node(mac_ctx, mac_hdr,
						eLIM_MLM_WT_SAE_AUTH_STATE);
		}
	}

	sae_retry = mlme_get_sae_auth_retry(pe_session->vdev);
	if (LIM_IS_STA_ROLE(pe_session) && sae_retry &&
	    sae_retry->sae_auth.data) {
		if (lim_is_sae_auth_algo_match(
		    sae_retry->sae_auth.data, sae_retry->sae_auth.len,
		     rx_pkt_info))
			lim_sae_auth_cleanup_retry(mac_ctx,
						   pe_session->vdev_id);
	}
	lim_send_sme_mgmt_frame_ind(mac_ctx, mac_hdr->fc.subType,
				    (uint8_t *)mac_hdr,
				    frame_len + sizeof(tSirMacMgmtHdr),
				    pe_session->smeSessionId,
				    WMA_GET_RX_FREQ(rx_pkt_info), pe_session,
				    WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info),
				    rx_flags);
}
#else
static inline void  lim_process_sae_auth_frame(struct mac_context *mac_ctx,
		uint8_t *rx_pkt_info, struct pe_session *pe_session)
{}
#endif

static void lim_process_auth_frame_type1(struct mac_context *mac_ctx,
		tpSirMacMgmtHdr mac_hdr,
		tSirMacAuthFrameBody *rx_auth_frm_body,
		uint8_t *rx_pkt_info, uint16_t curr_seq_num,
		tSirMacAuthFrameBody *auth_frame, struct pe_session *pe_session)
{
	tpDphHashNode sta_ds_ptr = NULL;
	struct tLimPreAuthNode *auth_node;
	uint32_t maxnum_preauth;
	uint16_t associd = 0;

	/* AuthFrame 1 */
	sta_ds_ptr = dph_lookup_hash_entry(mac_ctx, mac_hdr->sa,
				&associd, &pe_session->dph.dphHashTable);
	if (sta_ds_ptr) {
		tLimMlmDisassocReq *pMlmDisassocReq = NULL;
		tLimMlmDeauthReq *pMlmDeauthReq = NULL;
		bool is_connected = true;

		pMlmDisassocReq =
			mac_ctx->lim.limDisassocDeauthCnfReq.pMlmDisassocReq;
		if (pMlmDisassocReq &&
			(!qdf_mem_cmp((uint8_t *) mac_hdr->sa, (uint8_t *)
				&pMlmDisassocReq->peer_macaddr.bytes,
				QDF_MAC_ADDR_SIZE))) {
			pe_debug("TODO:Ack for disassoc frame is pending Issue delsta for "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(
					pMlmDisassocReq->peer_macaddr.bytes));
			lim_process_disassoc_ack_timeout(mac_ctx);
			is_connected = false;
		}
		pMlmDeauthReq =
			mac_ctx->lim.limDisassocDeauthCnfReq.pMlmDeauthReq;
		if (pMlmDeauthReq &&
			(!qdf_mem_cmp((uint8_t *) mac_hdr->sa, (uint8_t *)
				&pMlmDeauthReq->peer_macaddr.bytes,
				QDF_MAC_ADDR_SIZE))) {
			pe_debug("TODO:Ack for deauth frame is pending Issue delsta for "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(
					pMlmDeauthReq->peer_macaddr.bytes));
			lim_process_deauth_ack_timeout(mac_ctx);
			is_connected = false;
		}

		/*
		 * pStaDS != NULL and is_connected = 1 means the STA is already
		 * connected, But SAP received the Auth from that station.
		 * For non PMF connection send Deauth frame as STA will retry
		 * to connect back. The reason for above logic is captured in
		 * CR620403. If we silently drop the auth, the subsequent EAPOL
		 * exchange will fail & peer STA will keep trying until DUT
		 * SAP/GO gets a kickout event from FW & cleans up.
		 *
		 * For PMF connection the AP should not tear down or otherwise
		 * modify the state of the existing association until the
		 * SA-Query procedure determines that the original SA is
		 * invalid.
		 */
		if (is_connected
#ifdef WLAN_FEATURE_11W
			&& !sta_ds_ptr->rmfEnabled
#endif
		   ) {
			pe_err("STA is already connected but received auth frame"
			       "Send the Deauth and lim Delete Station Context"
			       "(associd: %d) sta mac" QDF_MAC_ADDR_FMT,
			       associd, QDF_MAC_ADDR_REF(mac_hdr->sa));
			lim_send_deauth_mgmt_frame(mac_ctx,
				REASON_UNSPEC_FAILURE,
				(uint8_t *) mac_hdr->sa,
				pe_session, false);
			lim_trigger_sta_deletion(mac_ctx, sta_ds_ptr,
				pe_session);
			return;
		}
	}
	/* Check if there exists pre-auth context for this STA */
	auth_node = lim_search_pre_auth_list(mac_ctx, mac_hdr->sa);
	if (auth_node) {
		/* Pre-auth context exists for the STA */
		if (!(mac_hdr->fc.retry == 0 ||
					auth_node->seq_num != curr_seq_num)) {
			/*
			 * This can happen when first authentication frame is
			 * received but ACK lost at STA side, in this case 2nd
			 * auth frame is already in transmission queue
			 */
			pe_warn("STA is initiating Auth after ACK lost");
			return;
		}
		/*
		 * STA is initiating brand-new Authentication
		 * sequence after local Auth Response timeout Or STA
		 * retrying to transmit First Auth frame due to packet
		 * drop OTA Delete Pre-auth node and fall through.
		 */
		if (auth_node->fTimerStarted)
			lim_deactivate_and_change_per_sta_id_timer(
					mac_ctx, eLIM_AUTH_RSP_TIMER,
					auth_node->authNodeIdx);
		pe_debug("STA is initiating brand-new Auth");
		lim_delete_pre_auth_node(mac_ctx, mac_hdr->sa);
		/*
		 *  SAP Mode:Disassociate the station and
		 *  delete its entry if we have its entry
		 *  already and received "auth" from the
		 *  same station.
		 *  SAP dphHashTable.size = 8
		 */
		for (associd = 0; associd < pe_session->dph.dphHashTable.size;
			associd++) {
			sta_ds_ptr = dph_get_hash_entry(mac_ctx, associd,
						&pe_session->dph.dphHashTable);
			if (!sta_ds_ptr)
				continue;
			if (sta_ds_ptr->valid && (!qdf_mem_cmp(
					(uint8_t *)&sta_ds_ptr->staAddr,
					(uint8_t *) &(mac_hdr->sa),
					(uint8_t) sizeof(tSirMacAddr))))
				break;
			sta_ds_ptr = NULL;
		}

		if (sta_ds_ptr
#ifdef WLAN_FEATURE_11W
			&& !sta_ds_ptr->rmfEnabled
#endif
		   ) {
			pe_debug("lim Del Sta Ctx associd: %d sta mac"
				 QDF_MAC_ADDR_FMT, associd,
				 QDF_MAC_ADDR_REF(sta_ds_ptr->staAddr));
			lim_send_deauth_mgmt_frame(mac_ctx,
				REASON_UNSPEC_FAILURE,
				(uint8_t *)auth_node->peerMacAddr,
				pe_session, false);
			lim_trigger_sta_deletion(mac_ctx, sta_ds_ptr,
				pe_session);
			return;
		}
	}
	maxnum_preauth = mac_ctx->mlme_cfg->lfr.max_num_pre_auth;
	if (mac_ctx->lim.gLimNumPreAuthContexts == maxnum_preauth &&
			!lim_delete_open_auth_pre_auth_node(mac_ctx)) {
		pe_err("Max no of preauth context reached");
		/*
		 * Maximum number of pre-auth contexts reached.
		 * Send Authentication frame with unspecified failure
		 */
		auth_frame->authAlgoNumber = rx_auth_frm_body->authAlgoNumber;
		auth_frame->authTransactionSeqNumber =
			rx_auth_frm_body->authTransactionSeqNumber + 1;
		auth_frame->authStatusCode =
			STATUS_UNSPECIFIED_FAILURE;

		lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
		return;
	}
	/* No Pre-auth context exists for the STA. */
	if (lim_is_auth_algo_supported(mac_ctx,
			(tAniAuthType) rx_auth_frm_body->authAlgoNumber,
			pe_session)) {
		struct wlan_objmgr_vdev *vdev;

		vdev =
		  wlan_objmgr_get_vdev_by_macaddr_from_pdev(mac_ctx->pdev,
							    mac_hdr->sa,
							    WLAN_LEGACY_MAC_ID);
		/* SA is same as any of the device vdev, return failure */
		if (vdev) {
			wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
			auth_frame->authAlgoNumber =
				rx_auth_frm_body->authAlgoNumber;
			auth_frame->authTransactionSeqNumber =
				rx_auth_frm_body->authTransactionSeqNumber + 1;
			auth_frame->authStatusCode =
				STATUS_UNSPECIFIED_FAILURE;

			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			return;
		}

		switch (rx_auth_frm_body->authAlgoNumber) {
		case eSIR_OPEN_SYSTEM:
			lim_process_auth_open_system_algo(mac_ctx, mac_hdr,
				rx_auth_frm_body, auth_frame, pe_session);
			break;

		case eSIR_SHARED_KEY:
			lim_process_auth_shared_system_algo(mac_ctx, mac_hdr,
				rx_auth_frm_body, auth_frame, pe_session);
			break;
		default:
			pe_err("rx Auth frm for unsupported auth algo %d "
				QDF_MAC_ADDR_FMT,
				rx_auth_frm_body->authAlgoNumber,
				QDF_MAC_ADDR_REF(mac_hdr->sa));

			/*
			 * Responding party does not support the
			 * authentication algorithm requested by
			 * sending party.
			 * Reject by sending Authentication frame
			 * with auth algorithm not supported status code
			 */
			auth_frame->authAlgoNumber =
				rx_auth_frm_body->authAlgoNumber;
			auth_frame->authTransactionSeqNumber =
				rx_auth_frm_body->authTransactionSeqNumber + 1;
			auth_frame->authStatusCode =
				STATUS_NOT_SUPPORTED_AUTH_ALG;
			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			return;
		}
	} else {
		pe_err("received Authentication frame for unsupported auth algorithm %d "
			QDF_MAC_ADDR_FMT,
			rx_auth_frm_body->authAlgoNumber,
			QDF_MAC_ADDR_REF(mac_hdr->sa));

		/*
		 * Responding party does not support the
		 * authentication algorithm requested by sending party.
		 * Reject Authentication with StatusCode=13.
		 */
		auth_frame->authAlgoNumber = rx_auth_frm_body->authAlgoNumber;
		auth_frame->authTransactionSeqNumber =
			rx_auth_frm_body->authTransactionSeqNumber + 1;
		auth_frame->authStatusCode =
			STATUS_NOT_SUPPORTED_AUTH_ALG;

		lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa,
				LIM_NO_WEP_IN_FC,
				pe_session);
		return;
	}
}

static void lim_process_auth_frame_type2(struct mac_context *mac_ctx,
		tpSirMacMgmtHdr mac_hdr,
		tSirMacAuthFrameBody *rx_auth_frm_body,
		tSirMacAuthFrameBody *auth_frame,
		uint8_t *plainbody,
		uint8_t *body_ptr, uint16_t frame_len,
		struct pe_session *pe_session)
{
	uint8_t key_id, cfg_privacy_opt_imp;
	uint32_t key_length = 8;
	qdf_size_t val;
	uint8_t defaultkey[SIR_MAC_KEY_LENGTH];
	struct tLimPreAuthNode *auth_node;
	uint8_t *encr_auth_frame;
	struct wlan_mlme_wep_cfg *wep_params = &mac_ctx->mlme_cfg->wep_params;
	QDF_STATUS qdf_status;

	/* AuthFrame 2 */
	if (pe_session->limMlmState != eLIM_MLM_WT_AUTH_FRAME2_STATE) {
		/**
		 * Check if a Reassociation is in progress and this is a
		 * Pre-Auth frame
		 */
		if (LIM_IS_STA_ROLE(pe_session) &&
		    (pe_session->limSmeState == eLIM_SME_WT_REASSOC_STATE) &&
		    (rx_auth_frm_body->authStatusCode == STATUS_SUCCESS) &&
		    (pe_session->ftPEContext.pFTPreAuthReq) &&
		    (!qdf_mem_cmp(
			pe_session->ftPEContext.pFTPreAuthReq->preAuthbssId,
			mac_hdr->sa, sizeof(tSirMacAddr)))) {

			/* Update the FTIEs in the saved auth response */
			pe_warn("rx PreAuth frm2 in smestate: %d from: "QDF_MAC_ADDR_FMT,
				pe_session->limSmeState,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			pe_session->ftPEContext.saved_auth_rsp_length = 0;

			if ((body_ptr) && (frame_len < MAX_FTIE_SIZE)) {
				qdf_mem_copy(
					pe_session->ftPEContext.saved_auth_rsp,
					body_ptr, frame_len);
				pe_session->ftPEContext.saved_auth_rsp_length =
					frame_len;
			}
		} else {
			/*
			 * Received Auth frame2 in an unexpected state.
			 * Log error and ignore the frame.
			 */
			pe_debug("rx Auth frm2 from peer in state: %d addr",
				pe_session->limMlmState);
			lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGD);
		}
		return;
	}

	if (qdf_mem_cmp((uint8_t *) mac_hdr->sa,
			(uint8_t *) &mac_ctx->lim.gpLimMlmAuthReq->peerMacAddr,
			sizeof(tSirMacAddr))) {
		/*
		 * Received Authentication frame from an entity
		 * other than one request was initiated.
		 * Wait until Authentication Failure Timeout.
		 */

		pe_warn("received Auth frame2 from unexpected peer"
			QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(mac_hdr->sa));
		return;
	}

	if (rx_auth_frm_body->authStatusCode ==
			STATUS_NOT_SUPPORTED_AUTH_ALG) {
		/*
		 * Interoperability workaround: Linksys WAP4400N is returning
		 * wrong authType in OpenAuth response in case of
		 * SharedKey AP configuration. Pretend we don't see that,
		 * so upper layer can fallback to SharedKey authType,
		 * and successfully connect to the AP.
		 */
		if (rx_auth_frm_body->authAlgoNumber !=
				mac_ctx->lim.gpLimMlmAuthReq->authType) {
			rx_auth_frm_body->authAlgoNumber =
				mac_ctx->lim.gpLimMlmAuthReq->authType;
		}
	}

	if (rx_auth_frm_body->authAlgoNumber !=
			mac_ctx->lim.gpLimMlmAuthReq->authType) {
		/*
		 * Auth algo is open in rx auth frame when auth type is SAE and
		 * PMK is cached as driver sent auth algo as open in tx frame
		 * as well.
		 */
		if ((mac_ctx->lim.gpLimMlmAuthReq->authType ==
		    eSIR_AUTH_TYPE_SAE) && pe_session->sae_pmk_cached) {
			pe_debug("rx Auth frame2 auth algo %d in SAE PMK case",
				rx_auth_frm_body->authAlgoNumber);
		} else {
			/*
			 * Received Authentication frame with an auth
			 * algorithm other than one requested.
			 * Wait until Authentication Failure Timeout.
			 */

			pe_warn("rx Auth frame2 for unexpected auth algo %d"
				QDF_MAC_ADDR_FMT,
				rx_auth_frm_body->authAlgoNumber,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			return;
		}
	}

	if (rx_auth_frm_body->authStatusCode != STATUS_SUCCESS) {
		/*
		 * Authentication failure.
		 * Return Auth confirm with received failure code to SME
		 */
		pe_err("rx Auth frame from peer with failure code %d "
			QDF_MAC_ADDR_FMT,
			rx_auth_frm_body->authStatusCode,
			QDF_MAC_ADDR_REF(mac_hdr->sa));
		lim_restore_from_auth_state(mac_ctx, eSIR_SME_AUTH_REFUSED,
			rx_auth_frm_body->authStatusCode,
			pe_session);
		return;
	}

	if (lim_process_fils_auth_frame2(mac_ctx, pe_session,
					 rx_auth_frm_body)) {
		lim_restore_from_auth_state(mac_ctx, eSIR_SME_SUCCESS,
				rx_auth_frm_body->authStatusCode, pe_session);
		return;
	}

	if (rx_auth_frm_body->authAlgoNumber == eSIR_OPEN_SYSTEM) {
		pe_session->limCurrentAuthType = eSIR_OPEN_SYSTEM;
		auth_node = lim_acquire_free_pre_auth_node(mac_ctx,
				&mac_ctx->lim.gLimPreAuthTimerTable);
		if (!auth_node) {
			pe_warn("Max pre-auth nodes reached");
			lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGW);
			return;
		}

		pe_debug("add new auth node: for "QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(mac_hdr->sa));
		qdf_mem_copy((uint8_t *) auth_node->peerMacAddr,
				mac_ctx->lim.gpLimMlmAuthReq->peerMacAddr,
				sizeof(tSirMacAddr));
		auth_node->fTimerStarted = 0;
		auth_node->authType =
			mac_ctx->lim.gpLimMlmAuthReq->authType;
		auth_node->seq_num =
			((mac_hdr->seqControl.seqNumHi << 4) |
			 (mac_hdr->seqControl.seqNumLo));
		auth_node->timestamp = qdf_mc_timer_get_system_ticks();
		lim_add_pre_auth_node(mac_ctx, auth_node);
		lim_restore_from_auth_state(mac_ctx, eSIR_SME_SUCCESS,
				rx_auth_frm_body->authStatusCode, pe_session);
	} else {
		/* Shared key authentication */
		if (LIM_IS_AP_ROLE(pe_session))
			cfg_privacy_opt_imp = pe_session->privacy;
		else
			cfg_privacy_opt_imp = wep_params->is_privacy_enabled;

		if (!cfg_privacy_opt_imp) {
			/*
			 * Requesting STA does not have WEP implemented.
			 * Reject with unsupported authentication algo
			 * Status code & wait until auth failure timeout
			 */

			pe_err("rx Auth frm from peer for unsupported auth algo %d "
						QDF_MAC_ADDR_FMT,
					rx_auth_frm_body->authAlgoNumber,
					QDF_MAC_ADDR_REF(mac_hdr->sa));

			auth_frame->authAlgoNumber =
				rx_auth_frm_body->authAlgoNumber;
			auth_frame->authTransactionSeqNumber =
				rx_auth_frm_body->authTransactionSeqNumber + 1;
			auth_frame->authStatusCode =
				STATUS_NOT_SUPPORTED_AUTH_ALG;
			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
					mac_hdr->sa, LIM_NO_WEP_IN_FC,
					pe_session);
			return;
		}
		if (rx_auth_frm_body->type != WLAN_ELEMID_CHALLENGE) {
			pe_err("rx auth frm with invalid challenge txtie");
			return;
		}

		key_id = mac_ctx->mlme_cfg->wep_params.wep_default_key_id;
		val = SIR_MAC_KEY_LENGTH;
		if (LIM_IS_AP_ROLE(pe_session)) {
			qdf_status = lim_get_wep_key_sap(pe_session,
							 wep_params,
							 key_id,
							 defaultkey,
							 &val);
		} else {
			qdf_status = mlme_get_wep_key(pe_session->vdev,
						      wep_params,
						      (MLME_WEP_DEFAULT_KEY_1 +
						       key_id), defaultkey,
						      &val);
			if (QDF_IS_STATUS_ERROR(qdf_status)) {
				pe_warn("cant retrieve Defaultkey");

				auth_frame->authAlgoNumber =
					rx_auth_frm_body->authAlgoNumber;
				auth_frame->authTransactionSeqNumber =
				rx_auth_frm_body->authTransactionSeqNumber + 1;

				auth_frame->authStatusCode =
					STATUS_CHALLENGE_FAIL;

				lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
							 mac_hdr->sa,
							 LIM_NO_WEP_IN_FC,
							 pe_session);
				lim_restore_from_auth_state(mac_ctx,
					eSIR_SME_INVALID_WEP_DEFAULT_KEY,
					REASON_UNSPEC_FAILURE,
					pe_session);
				return;
			}
		}
		key_length = val;
		((tpSirMacAuthFrameBody)plainbody)->authAlgoNumber =
			sir_swap_u16if_needed(rx_auth_frm_body->authAlgoNumber);
		((tpSirMacAuthFrameBody)plainbody)->authTransactionSeqNumber =
			sir_swap_u16if_needed((uint16_t)(
				rx_auth_frm_body->authTransactionSeqNumber
				+ 1));
		((tpSirMacAuthFrameBody)plainbody)->authStatusCode =
			STATUS_SUCCESS;
		((tpSirMacAuthFrameBody)plainbody)->type =
			WLAN_ELEMID_CHALLENGE;
		((tpSirMacAuthFrameBody)plainbody)->length =
			rx_auth_frm_body->length;
		qdf_mem_copy((uint8_t *) (
			(tpSirMacAuthFrameBody)plainbody)->challengeText,
			rx_auth_frm_body->challengeText,
			rx_auth_frm_body->length);
		encr_auth_frame = qdf_mem_malloc(rx_auth_frm_body->length +
						 LIM_ENCR_AUTH_INFO_LEN);
		if (!encr_auth_frame)
			return;
		lim_encrypt_auth_frame(mac_ctx, key_id,
				defaultkey, plainbody,
				encr_auth_frame, key_length);
		pe_session->limMlmState = eLIM_MLM_WT_AUTH_FRAME4_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
					pe_session->peSessionId,
					pe_session->limMlmState));
		lim_send_auth_mgmt_frame(mac_ctx,
				(tpSirMacAuthFrameBody)encr_auth_frame,
				mac_hdr->sa, rx_auth_frm_body->length,
				pe_session);
		qdf_mem_free(encr_auth_frame);
		return;
	}
}

static void lim_process_auth_frame_type3(struct mac_context *mac_ctx,
		tpSirMacMgmtHdr mac_hdr,
		tSirMacAuthFrameBody *rx_auth_frm_body,
		tSirMacAuthFrameBody *auth_frame,
		struct pe_session *pe_session)
{
	struct tLimPreAuthNode *auth_node;

	/* AuthFrame 3 */
	if (rx_auth_frm_body->authAlgoNumber != eSIR_SHARED_KEY) {
		pe_err("rx Auth frame3 from peer with auth algo number %d "
			QDF_MAC_ADDR_FMT,
			rx_auth_frm_body->authAlgoNumber,
			QDF_MAC_ADDR_REF(mac_hdr->sa));
		/*
		 * Received Authentication frame3 with algorithm other than
		 * Shared Key authentication type. Reject with Auth frame4
		 * with 'out of sequence' status code.
		 */
		auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
		auth_frame->authTransactionSeqNumber = SIR_MAC_AUTH_FRAME_4;
		auth_frame->authStatusCode =
			STATUS_UNKNOWN_AUTH_TRANSACTION;
		lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
			mac_hdr->sa, LIM_NO_WEP_IN_FC,
			pe_session);
		return;
	}

	if (LIM_IS_AP_ROLE(pe_session)) {
		/*
		 * Check if wep bit was set in FC. If not set,
		 * reject with Authentication frame4 with
		 * 'challenge failure' status code.
		 */
		if (!mac_hdr->fc.wep) {
			pe_err("received Auth frame3 from peer with no WEP bit set "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			/* WEP bit is not set in FC of Auth Frame3 */
			auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
			auth_frame->authTransactionSeqNumber =
				SIR_MAC_AUTH_FRAME_4;
			auth_frame->authStatusCode =
				STATUS_CHALLENGE_FAIL;
			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
					mac_hdr->sa,
					LIM_NO_WEP_IN_FC,
					pe_session);
			return;
		}

		auth_node = lim_search_pre_auth_list(mac_ctx, mac_hdr->sa);
		if (!auth_node) {
			pe_warn("received AuthFrame3 from peer that has no preauth context "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			/*
			 * No 'pre-auth' context exists for this STA that sent
			 * an Authentication frame3. Send Auth frame4 with
			 * 'out of sequence' status code.
			 */
			auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
			auth_frame->authTransactionSeqNumber =
				SIR_MAC_AUTH_FRAME_4;
			auth_frame->authStatusCode =
				STATUS_UNKNOWN_AUTH_TRANSACTION;
			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			return;
		}

		if (auth_node->mlmState == eLIM_MLM_AUTH_RSP_TIMEOUT_STATE) {
			pe_warn("auth response timer timedout for peer "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			/*
			 * Received Auth Frame3 after Auth Response timeout.
			 * Reject by sending Auth Frame4 with
			 * Auth respone timeout Status Code.
			 */
			auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
			auth_frame->authTransactionSeqNumber =
				SIR_MAC_AUTH_FRAME_4;
			auth_frame->authStatusCode =
				STATUS_AUTH_TIMEOUT;

			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			/* Delete pre-auth context of STA */
			lim_delete_pre_auth_node(mac_ctx, mac_hdr->sa);
			return;
		}
		if (rx_auth_frm_body->authStatusCode !=
				STATUS_SUCCESS) {
			/*
			 * Received Authenetication Frame 3 with status code
			 * other than success. Wait until Auth response timeout
			 * to delete STA context.
			 */
			pe_err("rx Auth frm3 from peer with status code %d "
				QDF_MAC_ADDR_FMT,
				rx_auth_frm_body->authStatusCode,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
				return;
		}
		/*
		 * Check if received challenge text is same as one sent in
		 * Authentication frame3
		 */
		if (!qdf_mem_cmp(rx_auth_frm_body->challengeText,
					auth_node->challengeText,
					SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH)) {
			/*
			 * Challenge match. STA is autheticated
			 * Delete Authentication response timer if running
			 */
			lim_deactivate_and_change_per_sta_id_timer(mac_ctx,
				eLIM_AUTH_RSP_TIMER, auth_node->authNodeIdx);

			auth_node->fTimerStarted = 0;
			auth_node->mlmState = eLIM_MLM_AUTHENTICATED_STATE;

			/*
			 * Send Auth Frame4 with 'success' Status Code.
			 */
			auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
			auth_frame->authTransactionSeqNumber =
				SIR_MAC_AUTH_FRAME_4;
			auth_frame->authStatusCode =
				STATUS_SUCCESS;
			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			return;
		} else {
			pe_warn("Challenge failure for peer "QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			/*
			 * Challenge Failure.
			 * Send Authentication frame4 with 'challenge failure'
			 * status code and wait until Auth response timeout to
			 * delete STA context.
			 */
			auth_frame->authAlgoNumber =
				rx_auth_frm_body->authAlgoNumber;
			auth_frame->authTransactionSeqNumber =
				SIR_MAC_AUTH_FRAME_4;
			auth_frame->authStatusCode =
				STATUS_CHALLENGE_FAIL;
			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			return;
		}
	}
}

static void lim_process_auth_frame_type4(struct mac_context *mac_ctx,
		tpSirMacMgmtHdr mac_hdr,
		tSirMacAuthFrameBody *rx_auth_frm_body,
		struct pe_session *pe_session)
{
	struct tLimPreAuthNode *auth_node;

	if (pe_session->limMlmState != eLIM_MLM_WT_AUTH_FRAME4_STATE) {
		/*
		 * Received Authentication frame4 in an unexpected state.
		 * Log error and ignore the frame.
		 */
		pe_warn("received unexpected Auth frame4 from peer in state %d, addr "
			QDF_MAC_ADDR_FMT,
			pe_session->limMlmState,
			QDF_MAC_ADDR_REF(mac_hdr->sa));
		return;
	}

	if (rx_auth_frm_body->authAlgoNumber != eSIR_SHARED_KEY) {
		/*
		 * Received Authentication frame4 with algorithm other than
		 * Shared Key authentication type.
		 * Wait until Auth failure timeout to report authentication
		 * failure to SME.
		 */
		pe_err("received Auth frame4 from peer with invalid auth algo %d"
			QDF_MAC_ADDR_FMT,
			rx_auth_frm_body->authAlgoNumber,
			QDF_MAC_ADDR_REF(mac_hdr->sa));
		return;
	}

	if (qdf_mem_cmp((uint8_t *) mac_hdr->sa,
			(uint8_t *) &mac_ctx->lim.gpLimMlmAuthReq->peerMacAddr,
			sizeof(tSirMacAddr))) {
		/*
		 * Received Authentication frame from an entity
		 * other than one to which request was initiated.
		 * Wait until Authentication Failure Timeout.
		 */

		pe_warn("received Auth frame4 from unexpected peer "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mac_hdr->sa));
		return;
	}

	if (rx_auth_frm_body->authAlgoNumber !=
			mac_ctx->lim.gpLimMlmAuthReq->authType) {
		/*
		 * Received Authentication frame with an auth algorithm
		 * other than one requested.
		 * Wait until Authentication Failure Timeout.
		 */

		pe_err("received Authentication frame from peer with invalid auth seq number %d "
			QDF_MAC_ADDR_FMT,
			rx_auth_frm_body->authTransactionSeqNumber,
			QDF_MAC_ADDR_REF(mac_hdr->sa));
		return;
	}

	if (rx_auth_frm_body->authStatusCode == STATUS_SUCCESS) {
		/*
		 * Authentication Success, Inform SME of same.
		 */
		pe_session->limCurrentAuthType = eSIR_SHARED_KEY;
		auth_node = lim_acquire_free_pre_auth_node(mac_ctx,
					&mac_ctx->lim.gLimPreAuthTimerTable);
		if (!auth_node) {
			pe_warn("Max pre-auth nodes reached");
			lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGW);
			return;
		}
		pe_debug("Alloc new data: %pK peer", auth_node);
		lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGD);
		qdf_mem_copy((uint8_t *) auth_node->peerMacAddr,
				mac_ctx->lim.gpLimMlmAuthReq->peerMacAddr,
				sizeof(tSirMacAddr));
		auth_node->fTimerStarted = 0;
		auth_node->authType = mac_ctx->lim.gpLimMlmAuthReq->authType;
		auth_node->seq_num = ((mac_hdr->seqControl.seqNumHi << 4) |
					(mac_hdr->seqControl.seqNumLo));
		auth_node->timestamp = qdf_mc_timer_get_system_ticks();
		lim_add_pre_auth_node(mac_ctx, auth_node);
		lim_restore_from_auth_state(mac_ctx, eSIR_SME_SUCCESS,
				rx_auth_frm_body->authStatusCode, pe_session);
	} else {
		/*
		 * Authentication failure.
		 * Return Auth confirm with received failure code to SME
		 */
		pe_err("Authentication failure from peer "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mac_hdr->sa));
		lim_restore_from_auth_state(mac_ctx, eSIR_SME_AUTH_REFUSED,
				rx_auth_frm_body->authStatusCode,
				pe_session);
	}
}

/**
 * lim_process_auth_frame() - to process auth frame
 * @mac_ctx - Pointer to Global MAC structure
 * @rx_pkt_info - A pointer to Rx packet info structure
 * @session - A pointer to session
 *
 * This function is called by limProcessMessageQueue() upon Authentication
 * frame reception.
 *
 * LOGIC:
 * This function processes received Authentication frame and responds
 * with either next Authentication frame in sequence to peer MAC entity
 * or LIM_MLM_AUTH_IND on AP or LIM_MLM_AUTH_CNF on STA.
 *
 * NOTE:
 * 1. Authentication failures are reported to SME with same status code
 *    received from the peer MAC entity.
 * 2. Authentication frame2/4 received with alogirthm number other than
 *    one requested in frame1/3 are logged with an error and auth confirm
 *    will be sent to SME only after auth failure timeout.
 * 3. Inconsistency in the spec:
 *    On receiving Auth frame2, specs says that if WEP key mapping key
 *    or default key is NULL, Auth frame3 with a status code 15 (challenge
 *    failure to be returned to peer entity. However, section 7.2.3.10,
 *    table 14 says that status code field is 'reserved' for frame3 !
 *    In the current implementation, Auth frame3 is returned with status
 *    code 15 overriding section 7.2.3.10.
 * 4. If number pre-authentications reach configrable max limit,
 *    Authentication frame with 'unspecified failure' status code is
 *    returned to requesting entity.
 *
 * Return: None
 */
void
lim_process_auth_frame(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
		       struct pe_session *pe_session)
{
	uint8_t *body_ptr, key_id, cfg_privacy_opt_imp;
	uint8_t defaultkey[SIR_MAC_KEY_LENGTH];
	uint8_t *plainbody = NULL;
	uint8_t decrypt_result;
	uint16_t frame_len, curr_seq_num = 0, auth_alg;
	uint32_t key_length = 8;
	qdf_size_t val;
	tSirMacAuthFrameBody *rx_auth_frm_body, *rx_auth_frame, *auth_frame;
	tpSirMacMgmtHdr mac_hdr;
	struct tLimPreAuthNode *auth_node;
	struct wlan_mlme_wep_cfg *wep_params = &mac_ctx->mlme_cfg->wep_params;
	QDF_STATUS qdf_status;

	/* Get pointer to Authentication frame header and body */
	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	if (!frame_len) {
		/* Log error */
		pe_err("received Auth frame with no body from: "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mac_hdr->sa));
		return;
	}

	if (IEEE80211_IS_MULTICAST(mac_hdr->sa)) {
		/*
		 * Received Auth frame from a BC/MC address
		 * Log error and ignore it
		 */
		pe_err("received Auth frame from a BC/MC addr: "QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(mac_hdr->sa));
		return;
	}
	curr_seq_num = (mac_hdr->seqControl.seqNumHi << 4) |
		(mac_hdr->seqControl.seqNumLo);

	if (pe_session->prev_auth_seq_num == curr_seq_num &&
	    !qdf_mem_cmp(pe_session->prev_auth_mac_addr, &mac_hdr->sa,
			 ETH_ALEN) &&
	    mac_hdr->fc.retry) {
		pe_debug("auth frame, seq num: %d is already processed, drop it",
			 curr_seq_num);
		return;
	}

	/* save seq number and mac_addr in pe_session */
	pe_session->prev_auth_seq_num = curr_seq_num;
	qdf_mem_copy(pe_session->prev_auth_mac_addr, mac_hdr->sa, ETH_ALEN);

	body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);

	if (frame_len < 2) {
		pe_err("invalid frame len: %d", frame_len);
		return;
	}
	auth_alg = *(uint16_t *) body_ptr;

	pe_nofl_rl_info("Auth RX: vdev %d sys role %d lim_state %d from " QDF_MAC_ADDR_FMT " rssi %d auth_alg %d seq %d",
			pe_session->vdev_id, GET_LIM_SYSTEM_ROLE(pe_session),
			pe_session->limMlmState,
			QDF_MAC_ADDR_REF(mac_hdr->sa),
			WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info),
			auth_alg, curr_seq_num);

	/* Restore default failure timeout */
	if (QDF_P2P_CLIENT_MODE == pe_session->opmode &&
	    pe_session->defaultAuthFailureTimeout) {
		pe_debug("Restore default failure timeout");
		if (cfg_in_range(CFG_AUTH_FAILURE_TIMEOUT,
				 pe_session->defaultAuthFailureTimeout)) {
			mac_ctx->mlme_cfg->timeouts.auth_failure_timeout =
				pe_session->defaultAuthFailureTimeout;
		} else {
			mac_ctx->mlme_cfg->timeouts.auth_failure_timeout =
				cfg_default(CFG_AUTH_FAILURE_TIMEOUT);
			pe_session->defaultAuthFailureTimeout =
				cfg_default(CFG_AUTH_FAILURE_TIMEOUT);
		}
	}

	rx_auth_frame = qdf_mem_malloc(sizeof(tSirMacAuthFrameBody));
	if (!rx_auth_frame)
		return;

	auth_frame = qdf_mem_malloc(sizeof(tSirMacAuthFrameBody));
	if (!auth_frame)
		goto free;

	plainbody = qdf_mem_malloc(LIM_ENCR_AUTH_BODY_LEN);
	if (!plainbody)
		goto free;

	qdf_mem_zero(plainbody, LIM_ENCR_AUTH_BODY_LEN);

	/*
	 * Determine if WEP bit is set in the FC or received MAC header
	 * Note: WEP bit is set in FC of MAC header.
	 */
	if (mac_hdr->fc.wep) {
		if (frame_len < 4) {
			pe_err("invalid frame len: %d", frame_len);
			goto free;
		}
		/* Extract key ID from IV (most 2 bits of 4th byte of IV) */
		key_id = (*(body_ptr + 3)) >> 6;

		/*
		 * On STA in infrastructure BSS, Authentication frames received
		 * with WEP bit set in the FC must be rejected with challenge
		 * failure status code (weird thing in the spec - this should've
		 * been rejected with unspecified failure/unexpected assertion
		 * of wep bit (this status code does not exist though) or
		 * Out-of-sequence-Authentication-Frame status code.
		 */
		if (LIM_IS_STA_ROLE(pe_session)) {
			auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
			auth_frame->authTransactionSeqNumber =
				SIR_MAC_AUTH_FRAME_4;
			auth_frame->authStatusCode =
				STATUS_CHALLENGE_FAIL;
			/* Log error */
			pe_err("rx Auth frm with wep bit set role: %d "QDF_MAC_ADDR_FMT,
				GET_LIM_SYSTEM_ROLE(pe_session),
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			goto free;
		}

		if ((frame_len < LIM_ENCR_AUTH_BODY_LEN_SAP) ||
		    (frame_len > LIM_ENCR_AUTH_BODY_LEN)) {
			/* Log error */
			pe_err("Not enough size: %d to decry rx Auth frm",
				frame_len);
			lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGE);
			goto free;
		}

		/*
		 * Accept Authentication frame only if Privacy is
		 * implemented
		 */
		if (LIM_IS_AP_ROLE(pe_session))
			cfg_privacy_opt_imp = pe_session->privacy;
		else
			cfg_privacy_opt_imp = wep_params->is_privacy_enabled;

		if (!cfg_privacy_opt_imp) {
			pe_err("received Authentication frame3 from peer that while privacy option is turned OFF "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			/*
			 * Privacy option is not implemented.
			 * So reject Authentication frame received with
			 * WEP bit set by sending Authentication frame
			 * with 'challenge failure' status code. This is
			 * another strange thing in the spec. Status code
			 * should have been 'unsupported algorithm' status code.
			 */
			auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
			auth_frame->authTransactionSeqNumber =
				SIR_MAC_AUTH_FRAME_4;
			auth_frame->authStatusCode =
				STATUS_CHALLENGE_FAIL;
			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			goto free;
		}

		/*
		 * Privacy option is implemented. Check if the received frame is
		 * Authentication frame3 and there is a context for requesting
		 * STA. If not, reject with unspecified failure status code
		 */
		auth_node = lim_search_pre_auth_list(mac_ctx, mac_hdr->sa);
		if (!auth_node) {
			pe_err("rx Auth frame with no preauth ctx with WEP bit set "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			/*
			 * No 'pre-auth' context exists for this STA
			 * that sent an Authentication frame with FC
			 * bit set. Send Auth frame4 with
			 * 'out of sequence' status code.
			 */
			auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
			auth_frame->authTransactionSeqNumber =
				SIR_MAC_AUTH_FRAME_4;
			auth_frame->authStatusCode =
				STATUS_UNKNOWN_AUTH_TRANSACTION;
			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			goto free;
		}
		/* Change the auth-response timeout */
		lim_deactivate_and_change_per_sta_id_timer(mac_ctx,
				eLIM_AUTH_RSP_TIMER, auth_node->authNodeIdx);

		/* 'Pre-auth' status exists for STA */
		if ((auth_node->mlmState != eLIM_MLM_WT_AUTH_FRAME3_STATE) &&
			(auth_node->mlmState !=
				eLIM_MLM_AUTH_RSP_TIMEOUT_STATE)) {
			pe_err("received Authentication frame from peer that is in state %d "
				QDF_MAC_ADDR_FMT,
				auth_node->mlmState,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			/*
			 * Should not have received Authentication frame
			 * with WEP bit set in FC in other states.
			 * Reject by sending Authenticaton frame with
			 * out of sequence Auth frame status code.
			 */
			auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
			auth_frame->authTransactionSeqNumber =
				SIR_MAC_AUTH_FRAME_4;
			auth_frame->authStatusCode =
				STATUS_UNKNOWN_AUTH_TRANSACTION;

			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			goto free;
		}

		val = SIR_MAC_KEY_LENGTH;

		if (LIM_IS_AP_ROLE(pe_session)) {
			qdf_status = lim_get_wep_key_sap(pe_session,
							 wep_params,
							 key_id,
							 defaultkey,
							 &val);
		} else {
			qdf_status = mlme_get_wep_key(pe_session->vdev,
						      wep_params,
						      (MLME_WEP_DEFAULT_KEY_1 +
						      key_id), defaultkey,
						      &val);
			if (QDF_IS_STATUS_ERROR(qdf_status)) {
				pe_warn("could not retrieve Default key");

				/*
				 * Send Authentication frame
				 * with challenge failure status code
				 */
				auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
				auth_frame->authTransactionSeqNumber =
							SIR_MAC_AUTH_FRAME_4;
				auth_frame->authStatusCode =
					STATUS_CHALLENGE_FAIL;
				lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
						mac_hdr->sa, LIM_NO_WEP_IN_FC,
						pe_session);
				goto free;
			}
		}

		key_length = val;
		decrypt_result = lim_decrypt_auth_frame(mac_ctx, defaultkey,
					body_ptr, plainbody, key_length,
					(uint16_t) (frame_len -
							SIR_MAC_WEP_IV_LENGTH));
		if (decrypt_result == LIM_DECRYPT_ICV_FAIL) {
			pe_err("received Authentication frame from peer that failed decryption: "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(mac_hdr->sa));
			/* ICV failure */
			lim_delete_pre_auth_node(mac_ctx, mac_hdr->sa);
			auth_frame->authAlgoNumber = eSIR_SHARED_KEY;
			auth_frame->authTransactionSeqNumber =
				SIR_MAC_AUTH_FRAME_4;
			auth_frame->authStatusCode = STATUS_CHALLENGE_FAIL;

			lim_send_auth_mgmt_frame(mac_ctx, auth_frame,
				mac_hdr->sa, LIM_NO_WEP_IN_FC,
				pe_session);
			goto free;
		}
		if ((sir_convert_auth_frame2_struct(mac_ctx, plainbody,
				frame_len - 8, rx_auth_frame) != QDF_STATUS_SUCCESS)
				|| (!is_auth_valid(mac_ctx, rx_auth_frame,
							pe_session))) {
			pe_err("failed to convert Auth Frame to structure or Auth is not valid");
			goto free;
		}
	} else if (auth_alg == eSIR_AUTH_TYPE_SAE) {
		if (LIM_IS_STA_ROLE(pe_session) ||
		    (LIM_IS_AP_ROLE(pe_session) &&
		     mac_ctx->mlme_cfg->sap_cfg.sap_sae_enabled))
			lim_process_sae_auth_frame(mac_ctx, rx_pkt_info,
						   pe_session);
		goto free;
	} else if ((sir_convert_auth_frame2_struct(mac_ctx, body_ptr,
				frame_len, rx_auth_frame) != QDF_STATUS_SUCCESS)
				|| (!is_auth_valid(mac_ctx, rx_auth_frame,
						pe_session))) {
		pe_err("failed to convert Auth Frame to structure or Auth is not valid");
		goto free;
	}

	rx_auth_frm_body = rx_auth_frame;

	if (!lim_is_valid_fils_auth_frame(mac_ctx, pe_session,
			rx_auth_frm_body)) {
		pe_err("Received invalid FILS auth packet");
		goto free;
	}

	/*
	 * IOT Workaround: with invalid WEP key, some APs reply
	 * AuthFrame 4 with invalid seqNumber. This AuthFrame
	 * will be dropped by driver, thus driver sends the
	 * generic status code instead of protocol status code.
	 * As a workaround, override AuthFrame 4's seqNumber.
	 */
	if ((pe_session->limMlmState ==
		eLIM_MLM_WT_AUTH_FRAME4_STATE) &&
		(rx_auth_frm_body->authTransactionSeqNumber !=
		SIR_MAC_AUTH_FRAME_1) &&
		(rx_auth_frm_body->authTransactionSeqNumber !=
		SIR_MAC_AUTH_FRAME_2) &&
		(rx_auth_frm_body->authTransactionSeqNumber !=
		SIR_MAC_AUTH_FRAME_3)) {
		pe_warn("Override AuthFrame 4's seqNumber to 4");
		rx_auth_frm_body->authTransactionSeqNumber =
			SIR_MAC_AUTH_FRAME_4;
	}


	switch (rx_auth_frm_body->authTransactionSeqNumber) {
	case SIR_MAC_AUTH_FRAME_1:
		lim_process_auth_frame_type1(mac_ctx,
			mac_hdr, rx_auth_frm_body, rx_pkt_info,
			curr_seq_num, auth_frame, pe_session);
		break;
	case SIR_MAC_AUTH_FRAME_2:
		lim_process_auth_frame_type2(mac_ctx,
			mac_hdr, rx_auth_frm_body, auth_frame, plainbody,
			body_ptr, frame_len, pe_session);
		break;
	case SIR_MAC_AUTH_FRAME_3:
		lim_process_auth_frame_type3(mac_ctx,
			mac_hdr, rx_auth_frm_body, auth_frame, pe_session);
		break;
	case SIR_MAC_AUTH_FRAME_4:
		lim_process_auth_frame_type4(mac_ctx,
			mac_hdr, rx_auth_frm_body, pe_session);
		break;
	default:
		/* Invalid Authentication Frame received. Ignore it. */
		pe_warn("rx auth frm with invalid authseq no: %d from: "QDF_MAC_ADDR_FMT,
			rx_auth_frm_body->authTransactionSeqNumber,
			QDF_MAC_ADDR_REF(mac_hdr->sa));
		break;
	}
free:
	if (auth_frame)
		qdf_mem_free(auth_frame);
	if (rx_auth_frame)
		qdf_mem_free(rx_auth_frame);
	if (plainbody)
		qdf_mem_free(plainbody);
}

/**
 * lim_process_sae_preauth_frame() - Send the WPA3 preauth SAE frame received
 * to the user space.
 * @mac: Global mac context
 * @rx_pkt: Received auth packet
 *
 * SAE auth frame will be received with no session if its SAE preauth during
 * roaming offloaded to the host. Forward this frame to the wpa supplicant.
 *
 * Return: True if auth algo is SAE else false
 */
static
bool lim_process_sae_preauth_frame(struct mac_context *mac, uint8_t *rx_pkt)
{
	tpSirMacMgmtHdr dot11_hdr;
	uint16_t auth_alg, frm_len;
	uint8_t *frm_body, pdev_id;
	struct wlan_objmgr_vdev *vdev;

	dot11_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt);
	frm_body = WMA_GET_RX_MPDU_DATA(rx_pkt);
	frm_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt);

	if (frm_len < 2) {
		pe_debug("LFR3: Invalid auth frame len:%d", frm_len);
		return false;
	}

	auth_alg = *(uint16_t *)frm_body;
	if (auth_alg != eSIR_AUTH_TYPE_SAE)
		return false;

	pe_debug("LFR3: SAE auth frame: seq_ctrl:0x%X auth_transaction_num:%d",
		 ((dot11_hdr->seqControl.seqNumHi << 8) |
		  (dot11_hdr->seqControl.seqNumLo << 4) |
		  (dot11_hdr->seqControl.fragNum)), *(uint16_t *)(frm_body + 2));
	pdev_id = wlan_objmgr_pdev_get_pdev_id(mac->pdev);
	vdev = wlan_objmgr_get_vdev_by_macaddr_from_psoc(
			mac->psoc, pdev_id, dot11_hdr->da, WLAN_LEGACY_SME_ID);
	if (vdev) {
		lim_sae_auth_cleanup_retry(mac, vdev->vdev_objmgr.vdev_id);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	}

	lim_send_sme_mgmt_frame_ind(mac, dot11_hdr->fc.subType,
				    (uint8_t *)dot11_hdr,
				    frm_len + sizeof(tSirMacMgmtHdr),
				    SME_SESSION_ID_ANY,
				    WMA_GET_RX_FREQ(rx_pkt), NULL,
				    WMA_GET_RX_RSSI_NORMALIZED(rx_pkt),
				    RXMGMT_FLAG_NONE);
	return true;
}

/**
 *
 * Pass the received Auth frame. This is possibly the pre-auth from the
 * neighbor AP, in the same mobility domain.
 * This will be used in case of 11r FT.
 *
 ***----------------------------------------------------------------------
 */
QDF_STATUS lim_process_auth_frame_no_session(struct mac_context *mac,
					     uint8_t *pBd, void *body)
{
	tpSirMacMgmtHdr pHdr;
	struct pe_session *pe_session = NULL;
	uint8_t *pBody;
	uint16_t frameLen;
	tSirMacAuthFrameBody *rx_auth_frame;
	QDF_STATUS ret_status = QDF_STATUS_E_FAILURE;
	int i;
	bool sae_auth_frame;

	pHdr = WMA_GET_RX_MAC_HEADER(pBd);
	pBody = WMA_GET_RX_MPDU_DATA(pBd);
	frameLen = WMA_GET_RX_PAYLOAD_LEN(pBd);

	pe_debug("Auth Frame Received: BSSID " QDF_MAC_ADDR_FMT " (RSSI %d)",
		 QDF_MAC_ADDR_REF(pHdr->bssId),
		 (uint) abs((int8_t) WMA_GET_RX_RSSI_NORMALIZED(pBd)));

	if (frameLen == 0) {
		pe_err("Frame len = 0");
		return QDF_STATUS_E_FAILURE;
	}

	sae_auth_frame = lim_process_sae_preauth_frame(mac, pBd);
	if (sae_auth_frame)
		return QDF_STATUS_SUCCESS;

	/* Auth frame has come on a new BSS, however, we need to find the session
	 * from where the auth-req was sent to the new AP
	 */
	for (i = 0; i < mac->lim.maxBssId; i++) {
		/* Find first free room in session table */
		if (mac->lim.gpSession[i].valid == true &&
		    mac->lim.gpSession[i].ftPEContext.ftPreAuthSession ==
		    true) {
			/* Found the session */
			pe_session = &mac->lim.gpSession[i];
			mac->lim.gpSession[i].ftPEContext.ftPreAuthSession =
				false;
		}
	}

	if (!pe_session) {
		pe_debug("cannot find session id in FT pre-auth phase");
		return QDF_STATUS_E_FAILURE;
	}

	if (!pe_session->ftPEContext.pFTPreAuthReq) {
		pe_err("Error: No FT");
		/* No FT in progress. */
		return QDF_STATUS_E_FAILURE;
	}

	lim_print_mac_addr(mac, pHdr->bssId, LOGD);
	lim_print_mac_addr(mac,
			   pe_session->ftPEContext.pFTPreAuthReq->preAuthbssId,
			   LOGD);
	pe_debug("seqControl: 0x%X",
		((pHdr->seqControl.seqNumHi << 8) |
		 (pHdr->seqControl.seqNumLo << 4) |
		 (pHdr->seqControl.fragNum)));

	/* Check that its the same bssId we have for preAuth */
	if (qdf_mem_cmp
		    (pe_session->ftPEContext.pFTPreAuthReq->preAuthbssId,
		    pHdr->bssId, sizeof(tSirMacAddr))) {
		pe_err("Error: Same bssid as preauth BSSID");
		/* In this case SME if indeed has triggered a */
		/* pre auth it will time out. */
		return QDF_STATUS_E_FAILURE;
	}

	if (true ==
	    pe_session->ftPEContext.pFTPreAuthReq->bPreAuthRspProcessed) {
		/*
		 * This is likely a duplicate for the same pre-auth request.
		 * PE/LIM already posted a response to SME. Hence, drop it.
		 * TBD:
		 * 1) How did we even receive multiple auth responses?
		 * 2) Do we need to delete pre-auth session? Suppose we
		 * previously received an auth resp with failure which
		 * would not have created the session and forwarded to SME.
		 * And, we subsequently received an auth resp with success
		 * which would have created the session. This will now be
		 * dropped without being forwarded to SME! However, it is
		 * very unlikely to receive auth responses from the same
		 * AP with different reason codes.
		 * NOTE: return QDF_STATUS_SUCCESS so that the packet is dropped
		 * as this was indeed a response from the BSSID we tried to
		 * pre-auth.
		 */
		pe_debug("Auth rsp already posted to SME"
			       " (session %pK, FT session %pK)", pe_session,
			       pe_session);
		return QDF_STATUS_SUCCESS;
	} else {
		pe_warn("Auth rsp not yet posted to SME"
			       " (session %pK, FT session %pK)", pe_session,
			       pe_session);
		pe_session->ftPEContext.pFTPreAuthReq->bPreAuthRspProcessed =
			true;
	}

	/* Stopping timer now, that we have our unicast from the AP */
	/* of our choice. */
	lim_deactivate_and_change_timer(mac, eLIM_FT_PREAUTH_RSP_TIMER);

	rx_auth_frame = qdf_mem_malloc(sizeof(*rx_auth_frame));
	if (!rx_auth_frame) {
		lim_handle_ft_pre_auth_rsp(mac, QDF_STATUS_E_FAILURE, NULL, 0,
					   pe_session);
		return QDF_STATUS_E_NOMEM;
	}

	/* Save off the auth resp. */
	if ((sir_convert_auth_frame2_struct(mac, pBody, frameLen,
					    rx_auth_frame) !=
					    QDF_STATUS_SUCCESS)) {
		pe_err("failed to convert Auth frame to struct");
		lim_handle_ft_pre_auth_rsp(mac, QDF_STATUS_E_FAILURE, NULL, 0,
					   pe_session);
		qdf_mem_free(rx_auth_frame);
		return QDF_STATUS_E_FAILURE;
	}

	pe_debug("Received Auth frame with type: %d seqnum: %d status: %d %d",
		       (uint32_t)rx_auth_frame->authAlgoNumber,
		       (uint32_t)rx_auth_frame->authTransactionSeqNumber,
		       (uint32_t)rx_auth_frame->authStatusCode,
		       (uint32_t)mac->lim.gLimNumPreAuthContexts);
	switch (rx_auth_frame->authTransactionSeqNumber) {
	case SIR_MAC_AUTH_FRAME_2:
		if (rx_auth_frame->authStatusCode != STATUS_SUCCESS) {
			pe_err("Auth status code received is %d",
				(uint32_t)rx_auth_frame->authStatusCode);
			if (STATUS_AP_UNABLE_TO_HANDLE_NEW_STA ==
			    rx_auth_frame->authStatusCode)
				ret_status = QDF_STATUS_E_NOSPC;
		} else {
			ret_status = QDF_STATUS_SUCCESS;
		}
		break;

	default:
		pe_warn("Seq. no incorrect expected 2 received %d",
			(uint32_t)rx_auth_frame->authTransactionSeqNumber);
		break;
	}

	/* Send the Auth response to SME */
	lim_handle_ft_pre_auth_rsp(mac, ret_status, pBody,
				   frameLen, pe_session);
	qdf_mem_free(rx_auth_frame);

	return ret_status;
}
