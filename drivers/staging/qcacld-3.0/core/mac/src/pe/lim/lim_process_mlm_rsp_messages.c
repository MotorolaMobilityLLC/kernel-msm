/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

#include "wni_api.h"
#include "wni_cfg.h"
#include "sir_api.h"
#include "sch_api.h"
#include "utils_api.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_timer_utils.h"
#include "lim_send_messages.h"
#include "lim_admit_control.h"
#include "lim_send_messages.h"
#include "lim_ft.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "lim_session_utils.h"
#include "rrm_api.h"
#include "wma_types.h"
#include "cds_utils.h"
#include "lim_types.h"
#include "wlan_policy_mgr_api.h"
#include "nan_datapath.h"
#include "wlan_reg_services_api.h"
#include "wma.h"
#include "wlan_pkt_capture_ucfg_api.h"
#include "wlan_lmac_if_def.h"

#define MAX_SUPPORTED_PEERS_WEP 16

void lim_process_mlm_join_cnf(struct mac_context *, uint32_t *);
void lim_process_mlm_auth_cnf(struct mac_context *, uint32_t *);
void lim_process_mlm_assoc_ind(struct mac_context *, uint32_t *);
void lim_process_mlm_assoc_cnf(struct mac_context *, uint32_t *);
void lim_process_mlm_set_keys_cnf(struct mac_context *, uint32_t *);
void lim_process_mlm_disassoc_ind(struct mac_context *, uint32_t *);
void lim_process_mlm_disassoc_cnf(struct mac_context *, uint32_t *);
static void lim_process_mlm_deauth_ind(struct mac_context *, tLimMlmDeauthInd *);
void lim_process_mlm_deauth_cnf(struct mac_context *, uint32_t *);
void lim_process_mlm_purge_sta_ind(struct mac_context *, uint32_t *);

/**
 * lim_process_mlm_rsp_messages()
 *
 ***FUNCTION:
 * This function is called to processes various MLM response (CNF/IND
 * messages from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param mac       Pointer to Global MAC structure
 * @param  msgType   Indicates the MLM message type
 * @param  *msg_buf  A pointer to the MLM message buffer
 *
 * @return None
 */
void
lim_process_mlm_rsp_messages(struct mac_context *mac, uint32_t msgType,
			     uint32_t *msg_buf)
{

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	MTRACE(mac_trace(mac, TRACE_CODE_TX_LIM_MSG, 0, msgType));
	switch (msgType) {
	case LIM_MLM_AUTH_CNF:
		lim_process_mlm_auth_cnf(mac, msg_buf);
		break;
	case LIM_MLM_ASSOC_CNF:
		lim_process_mlm_assoc_cnf(mac, msg_buf);
		break;
	case LIM_MLM_START_CNF:
		lim_process_mlm_start_cnf(mac, msg_buf);
		break;
	case LIM_MLM_JOIN_CNF:
		lim_process_mlm_join_cnf(mac, msg_buf);
		break;
	case LIM_MLM_ASSOC_IND:
		lim_process_mlm_assoc_ind(mac, msg_buf);
		break;
	case LIM_MLM_REASSOC_CNF:
		lim_process_mlm_reassoc_cnf(mac, msg_buf);
		break;
	case LIM_MLM_DISASSOC_CNF:
		lim_process_mlm_disassoc_cnf(mac, msg_buf);
		break;
	case LIM_MLM_DISASSOC_IND:
		lim_process_mlm_disassoc_ind(mac, msg_buf);
		break;
	case LIM_MLM_PURGE_STA_IND:
		lim_process_mlm_purge_sta_ind(mac, msg_buf);
		break;
	case LIM_MLM_DEAUTH_CNF:
		lim_process_mlm_deauth_cnf(mac, msg_buf);
		break;
	case LIM_MLM_DEAUTH_IND:
		lim_process_mlm_deauth_ind(mac, (tLimMlmDeauthInd *)msg_buf);
		break;
	case LIM_MLM_SETKEYS_CNF:
		lim_process_mlm_set_keys_cnf(mac, msg_buf);
		break;
	case LIM_MLM_TSPEC_CNF:
		break;
	default:
		break;
	} /* switch (msgType) */
	return;
} /*** end lim_process_mlm_rsp_messages() ***/

/**
 * lim_process_mlm_start_cnf()
 *
 ***FUNCTION:
 * This function is called to processes MLM_START_CNF
 * message from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param mac       Pointer to Global MAC structure
 * @param msg_buf    A pointer to the MLM message buffer
 *
 * @return None
 */
void lim_process_mlm_start_cnf(struct mac_context *mac, uint32_t *msg_buf)
{
	struct pe_session *pe_session = NULL;
	tLimMlmStartCnf *pLimMlmStartCnf;
	uint8_t smesessionId;
	uint32_t chan_freq, ch_cfreq1 = 0;
	uint8_t send_bcon_ind = false;
	enum reg_wifi_band band;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	pLimMlmStartCnf = (tLimMlmStartCnf *)msg_buf;
	pe_session = pe_find_session_by_session_id(mac,
				pLimMlmStartCnf->sessionId);
	if (!pe_session) {
		pe_err("Session does Not exist with given sessionId");
		return;
	}
	smesessionId = pe_session->smeSessionId;

	if (pe_session->limSmeState != eLIM_SME_WT_START_BSS_STATE) {
		/*
		 * Should not have received Start confirm from MLM
		 * in other states. Log error.
		 */
		pe_err("received unexpected MLM_START_CNF in state %X",
				pe_session->limSmeState);
		return;
	}
	if (((tLimMlmStartCnf *)msg_buf)->resultCode == eSIR_SME_SUCCESS) {

		/*
		 * Update global SME state so that Beacon Generation
		 * module starts writing Beacon frames into TFP's
		 * Beacon file register.
		 */
		pe_session->limSmeState = eLIM_SME_NORMAL_STATE;
		MTRACE(mac_trace
			       (mac, TRACE_CODE_SME_STATE, pe_session->peSessionId,
			       pe_session->limSmeState));
		if (pe_session->bssType == eSIR_INFRA_AP_MODE)
			pe_debug("*** Started BSS in INFRA AP SIDE***");
		else if (pe_session->bssType == eSIR_NDI_MODE)
			pe_debug("*** Started BSS in NDI mode ***");
		else
			pe_debug("*** Started BSS ***");
	} else {
		/* Start BSS is a failure */
		pe_delete_session(mac, pe_session);
		pe_session = NULL;
		pe_err("Start BSS Failed");
	}
	/* Send response to Host */
	lim_send_sme_start_bss_rsp(mac, eWNI_SME_START_BSS_RSP,
				((tLimMlmStartCnf *)msg_buf)->resultCode,
				pe_session, smesessionId);
	if (pe_session &&
	    (((tLimMlmStartCnf *)msg_buf)->resultCode == eSIR_SME_SUCCESS)) {
		lim_ndi_mlme_vdev_up_transition(pe_session);

		/* We should start beacon transmission only if the channel
		 * on which we are operating is non-DFS until the channel
		 * availability check is done. The PE will receive an explicit
		 * request from upper layers to start the beacon transmission
		 */
		chan_freq = pe_session->curr_op_freq;
		band = wlan_reg_freq_to_band(pe_session->curr_op_freq);
		if (pe_session->ch_center_freq_seg1)
			ch_cfreq1 = wlan_reg_chan_band_to_freq(
					mac->pdev,
					pe_session->ch_center_freq_seg1,
					BIT(band));

		if (!LIM_IS_AP_ROLE(pe_session))
			return;
		if (pe_session->ch_width == CH_WIDTH_160MHZ) {
			if (wlan_reg_get_bonded_channel_state_for_freq(
					mac->pdev, chan_freq,
					pe_session->ch_width, 0) !=
					CHANNEL_STATE_DFS)
				send_bcon_ind = true;
		} else if (pe_session->ch_width == CH_WIDTH_80P80MHZ) {
			if ((wlan_reg_get_channel_state_for_freq(
					mac->pdev, chan_freq) !=
					CHANNEL_STATE_DFS) &&
			    (wlan_reg_get_channel_state_for_freq(
					mac->pdev, ch_cfreq1) !=
					CHANNEL_STATE_DFS))
				send_bcon_ind = true;
		} else {
			if (wlan_reg_get_channel_state_for_freq(mac->pdev,
								chan_freq)
					!= CHANNEL_STATE_DFS)
				send_bcon_ind = true;
		}
		if (WLAN_REG_IS_6GHZ_CHAN_FREQ(pe_session->curr_op_freq))
			send_bcon_ind = true;

		if (send_bcon_ind) {
			/* Configure beacon and send beacons to HAL */
			QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
					"Start Beacon with ssid %s Ch freq %d",
					pe_session->ssId.ssId,
					pe_session->curr_op_freq);
			lim_send_beacon(mac, pe_session);
			lim_enable_obss_detection_config(mac, pe_session);
			lim_send_obss_color_collision_cfg(mac, pe_session,
					OBSS_COLOR_COLLISION_DETECTION);
		} else {
			lim_sap_move_to_cac_wait_state(pe_session);
		}
	}
}

/*** end lim_process_mlm_start_cnf() ***/

/**
 * lim_process_mlm_join_cnf() - Processes join confirmation
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: A pointer to the MLM message buffer
 *
 * This Function handles the join confirmation message
 * LIM_MLM_JOIN_CNF.
 *
 * Return: None
 */
void lim_process_mlm_join_cnf(struct mac_context *mac_ctx,
	uint32_t *msg)
{
	tSirResultCodes result_code;
	tLimMlmJoinCnf *join_cnf;
	struct pe_session *session_entry;

	join_cnf = (tLimMlmJoinCnf *) msg;
	session_entry = pe_find_session_by_session_id(mac_ctx,
		join_cnf->sessionId);
	if (!session_entry) {
		pe_err("SessionId:%d does not exist", join_cnf->sessionId);
		return;
	}

	if (session_entry->limSmeState != eLIM_SME_WT_JOIN_STATE) {
		pe_err("received unexpected MLM_JOIN_CNF in state %X",
			session_entry->limSmeState);
		return;
	}

	result_code = ((tLimMlmJoinCnf *) msg)->resultCode;
	/* Process Join confirm from MLM */
	if (result_code == eSIR_SME_SUCCESS) {
		/* Setup hardware upfront */
		if (lim_sta_send_add_bss_pre_assoc(mac_ctx,
						   session_entry) ==
		    QDF_STATUS_SUCCESS)
			return;
		else
			result_code = eSIR_SME_REFUSED;
	}

	/*  Join failure */
	session_entry->limSmeState = eLIM_SME_JOIN_FAILURE_STATE;
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
		session_entry->peSessionId,
		session_entry->limSmeState));
	/* Send Join response to Host */
	lim_handle_sme_join_result(mac_ctx, result_code,
		((tLimMlmJoinCnf *) msg)->protStatusCode, session_entry);
	return;
}

/**
 * lim_send_mlm_assoc_req() - Association request will be processed
 * mac_ctx:  Pointer to Global MAC structure
 * session_entry:  Pointer to session etnry
 *
 * This function is sends ASSOC request MLM message to MLM State machine.
 * ASSOC request packet would be by picking parameters from pe_session
 * automatically based on the current state of MLM state machine.
 * ASSUMPTIONS:
 * this function is called in middle of connection state machine and is
 * expected to be called after auth cnf has been received or after ASSOC rsp
 * with TRY_AGAIN_LATER was received and required time has elapsed after that.
 *
 * Return: None
 */

static void lim_send_mlm_assoc_req(struct mac_context *mac_ctx,
	struct pe_session *session_entry)
{
	tLimMlmAssocReq *assoc_req;
	uint32_t val;
	uint16_t caps;
	uint32_t tele_bcn = 0;
	tpSirMacCapabilityInfo cap_info;

	if (!session_entry->lim_join_req) {
		pe_err("Join Request is NULL");
		/* No need to Assert. JOIN timeout will handle this error */
		return;
	}

	assoc_req = qdf_mem_malloc(sizeof(tLimMlmAssocReq));
	if (!assoc_req) {
		pe_err("call to AllocateMemory failed for mlmAssocReq");
		return;
	}
	val = sizeof(tSirMacAddr);
	sir_copy_mac_addr(assoc_req->peerMacAddr, session_entry->bssId);

	if (lim_get_capability_info(mac_ctx, &caps, session_entry)
			!= QDF_STATUS_SUCCESS)
		/* Could not get Capabilities value from CFG.*/
		pe_err("could not retrieve Capabilities value");

	/* Clear spectrum management bit if AP doesn't support it */
	if (!(session_entry->lim_join_req->bssDescription.capabilityInfo &
		LIM_SPECTRUM_MANAGEMENT_BIT_MASK))
		/*
		 * AP doesn't support spectrum management
		 * clear spectrum management bit
		 */
		caps &= (~LIM_SPECTRUM_MANAGEMENT_BIT_MASK);

	/* Clear rrm bit if AP doesn't support it */
	if (!(session_entry->lim_join_req->bssDescription.capabilityInfo &
		LIM_RRM_BIT_MASK))
		caps &= (~LIM_RRM_BIT_MASK);

	/* Clear short preamble bit if AP does not support it */
	if (!(session_entry->lim_join_req->bssDescription.capabilityInfo &
		(LIM_SHORT_PREAMBLE_BIT_MASK))) {
		caps &= (~LIM_SHORT_PREAMBLE_BIT_MASK);
		pe_debug("Clearing short preamble:no AP support");
	}

	/* Clear immediate block ack bit if AP does not support it */
	if (!(session_entry->lim_join_req->bssDescription.capabilityInfo &
		(LIM_IMMEDIATE_BLOCK_ACK_MASK))) {
		caps &= (~LIM_IMMEDIATE_BLOCK_ACK_MASK);
		pe_debug("Clearing Immed Blk Ack:no AP support");
	}

	assoc_req->capabilityInfo = caps;
	cap_info = ((tpSirMacCapabilityInfo) &assoc_req->capabilityInfo);

	/*
	 * If telescopic beaconing is enabled, set listen interval to
	 * CFG_TELE_BCN_MAX_LI
	 */
	tele_bcn = mac_ctx->mlme_cfg->sap_cfg.tele_bcn_wakeup_en;
	if (tele_bcn)
		val = mac_ctx->mlme_cfg->sap_cfg.tele_bcn_max_li;
	else
		val = mac_ctx->mlme_cfg->sap_cfg.listen_interval;

#ifdef FEATURE_WLAN_DIAG_SUPPORT
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_ASSOC_REQ_EVENT,
		session_entry, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
#endif
	assoc_req->listenInterval = (uint16_t) val;
	/* Update PE session ID */
	assoc_req->sessionId = session_entry->peSessionId;
	session_entry->limPrevSmeState = session_entry->limSmeState;
	session_entry->limSmeState = eLIM_SME_WT_ASSOC_STATE;
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
		session_entry->peSessionId, session_entry->limSmeState));
	lim_post_mlm_message(mac_ctx, LIM_MLM_ASSOC_REQ,
		(uint32_t *) assoc_req);
}

#ifdef WLAN_FEATURE_11W
/**
 * lim_pmf_comeback_timer_callback() -PMF callback handler
 * @context: Timer context
 *
 * This function is called to processes the PMF comeback
 * callback
 *
 * Return: None
 */
void lim_pmf_comeback_timer_callback(void *context)
{
	struct comeback_timer_info *info =
			(struct comeback_timer_info *)context;
	struct mac_context *mac_ctx = info->mac;
	struct pe_session *session;

	session = pe_find_session_by_vdev_id(mac_ctx, info->vdev_id);
	if (!session) {
		pe_err("no session found for vdev %d", info->vdev_id);
		return;
	}

	if (session->limMlmState != eLIM_MLM_WT_ASSOC_RSP_STATE) {
		pe_debug("Don't send assoc req, timer expire when limMlmState %d vdev id %d",
			 session->limMlmState, session->vdev_id);
		return;
	}

	pe_info("comeback later timer expired. sending MLM ASSOC req for vdev %d, session limMlmState %d, info lim_mlm_state %d",
		session->vdev_id, session->limMlmState, info->lim_mlm_state);

	/* set MLM state such that ASSOC REQ packet will be sent out */
	session->limPrevMlmState = info->lim_prev_mlm_state;
	session->limMlmState = info->lim_mlm_state;
	lim_send_mlm_assoc_req(mac_ctx, session);
}
#endif /* WLAN_FEATURE_11W */

/**
 * lim_process_mlm_auth_cnf()-Process Auth confirmation
 * @mac_ctx:  Pointer to Global MAC structure
 * @msg: A pointer to the MLM message buffer
 *
 * This function is called to processes MLM_AUTH_CNF
 * message from MLM State machine.
 *
 * Return: None
 */
void lim_process_mlm_auth_cnf(struct mac_context *mac_ctx, uint32_t *msg)
{
	tAniAuthType auth_type, auth_mode;
	tLimMlmAuthReq *auth_req;
	tLimMlmAuthCnf *auth_cnf;
	struct pe_session *session_entry;

	if (!msg) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	auth_cnf = (tLimMlmAuthCnf *) msg;
	session_entry = pe_find_session_by_session_id(mac_ctx,
			auth_cnf->sessionId);
	if (!session_entry) {
		pe_err("SessionId:%d session doesn't exist",
			auth_cnf->sessionId);
		return;
	}

	if ((session_entry->limSmeState != eLIM_SME_WT_AUTH_STATE &&
		session_entry->limSmeState != eLIM_SME_WT_PRE_AUTH_STATE) ||
		LIM_IS_AP_ROLE(session_entry)) {
		/**
		 * Should not have received AUTH confirm
		 * from MLM in other states or on AP.
		 * Log error
		 */
		pe_err("SessionId:%d received MLM_AUTH_CNF in state %X",
			session_entry->peSessionId, session_entry->limSmeState);
		return;
	}

	if (auth_cnf->resultCode == eSIR_SME_SUCCESS) {
		if (session_entry->limSmeState == eLIM_SME_WT_AUTH_STATE) {
			lim_send_mlm_assoc_req(mac_ctx, session_entry);
		} else {
			/*
			 * Successful Pre-authentication. Send
			 * Pre-auth response to host
			 */
			session_entry->limSmeState =
				session_entry->limPrevSmeState;
			MTRACE(mac_trace
				(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));
		}
		/* Return for success case */
		return;
	}
	/*
	 * Failure case handle:
	 * Process AUTH confirm from MLM
	 */
	if (session_entry->limSmeState == eLIM_SME_WT_AUTH_STATE)
		auth_type = mac_ctx->mlme_cfg->wep_params.auth_type;
	else
		auth_type = mac_ctx->lim.gLimPreAuthType;

	if ((auth_type == eSIR_AUTO_SWITCH) &&
		(auth_cnf->authType == eSIR_SHARED_KEY) &&
		((auth_cnf->protStatusCode == STATUS_NOT_SUPPORTED_AUTH_ALG) ||
		(auth_cnf->resultCode == eSIR_SME_AUTH_TIMEOUT_RESULT_CODE))) {
		/*
		 * When shared authentication fails with reason
		 * code "13" and authType set to 'auto switch',
		 * Try with open Authentication
		 */
		auth_mode = eSIR_OPEN_SYSTEM;
		/* Trigger MAC based Authentication */
		auth_req = qdf_mem_malloc(sizeof(tLimMlmAuthReq));
		if (!auth_req) {
			pe_err("mlmAuthReq :Memory alloc failed");
			return;
		}
		if (session_entry->limSmeState ==
			eLIM_SME_WT_AUTH_STATE) {
			sir_copy_mac_addr(auth_req->peerMacAddr,
				session_entry->bssId);
		} else {
			qdf_mem_copy((uint8_t *)&auth_req->peerMacAddr,
			(uint8_t *)&mac_ctx->lim.gLimPreAuthPeerAddr,
			sizeof(tSirMacAddr));
		}
		auth_req->authType = auth_mode;
		/* Update PE session Id */
		auth_req->sessionId = auth_cnf->sessionId;
		lim_post_mlm_message(mac_ctx, LIM_MLM_AUTH_REQ,
			(uint32_t *) auth_req);
		return;
	} else {
		/* MAC based authentication failure */
		if (session_entry->limSmeState ==
			eLIM_SME_WT_AUTH_STATE) {
			pe_err("Auth Failure occurred");
			session_entry->limSmeState =
				eLIM_SME_JOIN_FAILURE_STATE;
			MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));
			session_entry->limMlmState =
				eLIM_MLM_IDLE_STATE;
			MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
				session_entry->peSessionId,
				session_entry->limMlmState));

			/* WAR for some IOT issue on specific AP:
			 * STA failed to connect to SAE AP due to incorrect
			 * password is used. Then AP will still reject the
			 * authentication even correct password is used unless
			 * STA send deauth to AP upon authentication failure.
			 */
			if (auth_type == eSIR_AUTH_TYPE_SAE) {
				pe_debug("Send deauth for SAE auth failure");
				lim_send_deauth_mgmt_frame(mac_ctx,
						       auth_cnf->protStatusCode,
						       auth_cnf->peerMacAddr,
						       session_entry, false);
			}

			/*
			 * Need to send Join response with
			 * auth failure to Host.
			 */
			lim_handle_sme_join_result(mac_ctx,
				auth_cnf->resultCode,
				auth_cnf->protStatusCode,
				session_entry);
		} else {
			/*
			 * Pre-authentication failure.
			 * Send Pre-auth failure response to host
			 */
			session_entry->limSmeState =
				session_entry->limPrevSmeState;
			MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));
		}
	}
}

/**
 * lim_process_mlm_assoc_cnf() - Process association confirmation
 * @mac_ctx:  Pointer to Global MAC structure
 * @msg:  A pointer to the MLM message buffer
 *
 * This function is called to processes MLM_ASSOC_CNF
 * message from MLM State machine.
 *
 * Return: None
 */
void lim_process_mlm_assoc_cnf(struct mac_context *mac_ctx,
	uint32_t *msg)
{
	struct pe_session *session_entry;
	tLimMlmAssocCnf *assoc_cnf;

	if (!msg) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	assoc_cnf = (tLimMlmAssocCnf *) msg;
	session_entry = pe_find_session_by_session_id(mac_ctx,
				assoc_cnf->sessionId);
	if (!session_entry) {
		pe_err("SessionId:%d Session does not exist",
			assoc_cnf->sessionId);
		return;
	}
	if (session_entry->limSmeState != eLIM_SME_WT_ASSOC_STATE ||
		 LIM_IS_AP_ROLE(session_entry)) {
		/*
		 * Should not have received Assocication confirm
		 * from MLM in other states OR on AP.
		 * Log error
		 */
		pe_err("SessionId:%d Received MLM_ASSOC_CNF in state %X",
			session_entry->peSessionId, session_entry->limSmeState);
		return;
	}
	if (((tLimMlmAssocCnf *) msg)->resultCode != eSIR_SME_SUCCESS) {
		/* Association failure */
		pe_err("SessionId:%d Association failure resultCode: %d limSmeState:%d",
			session_entry->peSessionId,
			((tLimMlmAssocCnf *) msg)->resultCode,
			session_entry->limSmeState);

		/* If driver gets deauth when its waiting for ADD_STA_RSP then
		 * we need to do DEL_STA followed by DEL_BSS. So based on below
		 * reason-code here we decide whether to do only DEL_BSS or
		 * DEL_STA + DEL_BSS.
		 */
		if (((tLimMlmAssocCnf *) msg)->resultCode !=
		    eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA)
			session_entry->limSmeState =
				eLIM_SME_JOIN_FAILURE_STATE;

		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
			session_entry->peSessionId, mac_ctx->lim.gLimSmeState));
		/*
		 * Need to send Join response with
		 * Association failure to Host.
		 */
		lim_handle_sme_join_result(mac_ctx,
			((tLimMlmAssocCnf *) msg)->resultCode,
			((tLimMlmAssocCnf *) msg)->protStatusCode,
			session_entry);
	} else {
		/* Successful Association */
		pe_debug("SessionId:%d Associated with BSS",
			session_entry->peSessionId);
		session_entry->limSmeState = eLIM_SME_LINK_EST_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
			session_entry->peSessionId,
			session_entry->limSmeState));
		/**
		 * Need to send Join response with
		 * Association success to Host.
		 */
		lim_handle_sme_join_result(mac_ctx,
			((tLimMlmAssocCnf *) msg)->resultCode,
			((tLimMlmAssocCnf *) msg)->protStatusCode,
			session_entry);
	}
}

void
lim_fill_sme_assoc_ind_params(
	struct mac_context *mac_ctx,
	tpLimMlmAssocInd assoc_ind, struct assoc_ind *sme_assoc_ind,
	struct pe_session *session_entry, bool assoc_req_alloc)
{
	sme_assoc_ind->length = sizeof(struct assoc_ind);
	sme_assoc_ind->sessionId = session_entry->smeSessionId;

	/* Required for indicating the frames to upper layer */
	sme_assoc_ind->assocReqLength = assoc_ind->assocReqLength;
	if (assoc_req_alloc && assoc_ind->assocReqLength) {
		sme_assoc_ind->assocReqPtr = qdf_mem_malloc(
					     assoc_ind->assocReqLength);
		qdf_mem_copy(sme_assoc_ind->assocReqPtr, assoc_ind->assocReqPtr,
			     assoc_ind->assocReqLength);
	} else {
		sme_assoc_ind->assocReqPtr = assoc_ind->assocReqPtr;
	}

	sme_assoc_ind->beaconPtr = session_entry->beacon;
	sme_assoc_ind->beaconLength = session_entry->bcnLen;

	/* Fill in peerMacAddr */
	qdf_mem_copy(sme_assoc_ind->peerMacAddr, assoc_ind->peerMacAddr,
		sizeof(tSirMacAddr));

	/* Fill in aid */
	sme_assoc_ind->aid = assoc_ind->aid;
	/* Fill in bssId */
	qdf_mem_copy(sme_assoc_ind->bssId, session_entry->bssId,
		sizeof(tSirMacAddr));
	/* Fill in authType */
	sme_assoc_ind->authType = assoc_ind->authType;
	/* Fill in rsn_akm_type */
	sme_assoc_ind->akm_type = assoc_ind->akm_type;
	/* Fill in ssId */
	qdf_mem_copy((uint8_t *) &sme_assoc_ind->ssId,
		(uint8_t *) &(assoc_ind->ssId), assoc_ind->ssId.length + 1);
	sme_assoc_ind->rsnIE.length = assoc_ind->rsnIE.length;
	qdf_mem_copy((uint8_t *) &sme_assoc_ind->rsnIE.rsnIEdata,
		(uint8_t *) &(assoc_ind->rsnIE.rsnIEdata),
		assoc_ind->rsnIE.length);

#ifdef FEATURE_WLAN_WAPI
	sme_assoc_ind->wapiIE.length = assoc_ind->wapiIE.length;
	qdf_mem_copy((uint8_t *) &sme_assoc_ind->wapiIE.wapiIEdata,
		(uint8_t *) &(assoc_ind->wapiIE.wapiIEdata),
		assoc_ind->wapiIE.length);
#endif
	sme_assoc_ind->addIE.length = assoc_ind->addIE.length;
	qdf_mem_copy((uint8_t *) &sme_assoc_ind->addIE.addIEdata,
		(uint8_t *) &(assoc_ind->addIE.addIEdata),
		assoc_ind->addIE.length);

	/* Copy the new TITAN capabilities */
	sme_assoc_ind->spectrumMgtIndicator = assoc_ind->spectrumMgtIndicator;
	if (assoc_ind->spectrumMgtIndicator == true) {
		sme_assoc_ind->powerCap.minTxPower =
			assoc_ind->powerCap.minTxPower;
		sme_assoc_ind->powerCap.maxTxPower =
			assoc_ind->powerCap.maxTxPower;
		sme_assoc_ind->supportedChannels.numChnl =
			assoc_ind->supportedChannels.numChnl;
		qdf_mem_copy((uint8_t *) &sme_assoc_ind->supportedChannels.
			channelList,
			(uint8_t *) &(assoc_ind->supportedChannels.channelList),
			assoc_ind->supportedChannels.numChnl);
	}
	qdf_mem_copy(&sme_assoc_ind->chan_info, &assoc_ind->chan_info,
		sizeof(struct oem_channel_info));
	/* Fill in WmmInfo */
	sme_assoc_ind->wmmEnabledSta = assoc_ind->WmmStaInfoPresent;
	sme_assoc_ind->ampdu = assoc_ind->ampdu;
	sme_assoc_ind->sgi_enable = assoc_ind->sgi_enable;
	sme_assoc_ind->tx_stbc = assoc_ind->tx_stbc;
	sme_assoc_ind->rx_stbc = assoc_ind->rx_stbc;
	sme_assoc_ind->ch_width = assoc_ind->ch_width;
	sme_assoc_ind->mode = assoc_ind->mode;
	sme_assoc_ind->max_supp_idx = assoc_ind->max_supp_idx;
	sme_assoc_ind->max_ext_idx = assoc_ind->max_ext_idx;
	sme_assoc_ind->max_mcs_idx = assoc_ind->max_mcs_idx;
	sme_assoc_ind->rx_mcs_map = assoc_ind->rx_mcs_map;
	sme_assoc_ind->tx_mcs_map = assoc_ind->tx_mcs_map;
	sme_assoc_ind->ecsa_capable = assoc_ind->ecsa_capable;

	if (assoc_ind->ht_caps.present)
		sme_assoc_ind->HTCaps = assoc_ind->ht_caps;
	if (assoc_ind->vht_caps.present)
		sme_assoc_ind->VHTCaps = assoc_ind->vht_caps;
	sme_assoc_ind->capability_info = assoc_ind->capabilityInfo;
	sme_assoc_ind->he_caps_present = assoc_ind->he_caps_present;
	sme_assoc_ind->is_sae_authenticated = assoc_ind->is_sae_authenticated;
}

/**
 * lim_process_mlm_assoc_ind() - This function is called to processes MLM_ASSOC_IND
 * message from MLM State machine.
 * @mac       Pointer to Global MAC structure
 * @msg_buf   A pointer to the MLM message buffer
 *
 * Return: None
 */
void lim_process_mlm_assoc_ind(struct mac_context *mac, uint32_t *msg_buf)
{
	uint32_t len;
	struct scheduler_msg msg = {0};
	struct assoc_ind *pSirSmeAssocInd;
	tpDphHashNode sta = 0;
	struct pe_session *pe_session;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	pe_session = pe_find_session_by_session_id(mac,
				((tpLimMlmAssocInd) msg_buf)->sessionId);
	if (!pe_session) {
		pe_err("Session Does not exist for given sessionId");
		return;
	}
	/* / Inform Host of STA association */
	len = sizeof(struct assoc_ind);
	pSirSmeAssocInd = qdf_mem_malloc(len);
	if (!pSirSmeAssocInd) {
		pe_err("call to AllocateMemory failed for eWNI_SME_ASSOC_IND");
		return;
	}

	pSirSmeAssocInd->messageType = eWNI_SME_ASSOC_IND;
	lim_fill_sme_assoc_ind_params(mac, (tpLimMlmAssocInd)msg_buf,
				      pSirSmeAssocInd,
				      pe_session, false);
	msg.type = eWNI_SME_ASSOC_IND;
	msg.bodyptr = pSirSmeAssocInd;
	msg.bodyval = 0;
	sta = dph_get_hash_entry(mac,
				    ((tpLimMlmAssocInd) msg_buf)->aid,
				    &pe_session->dph.dphHashTable);
	if (!sta) {
		pe_err("MLM AssocInd: Station context no longer valid (aid %d)",
			((tpLimMlmAssocInd) msg_buf)->aid);
		qdf_mem_free(pSirSmeAssocInd);

		return;
	}

	pSirSmeAssocInd->reassocReq = sta->mlmStaContext.subType;
	pSirSmeAssocInd->timingMeasCap = sta->timingMeasCap;
	MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
			 pe_session->peSessionId, msg.type));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_ASSOC_IND_EVENT, pe_session, 0,
			      0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
	pe_debug("Create CNF_WAIT_TIMER after received LIM_MLM_ASSOC_IND");
	/*
	** turn on a timer to detect the loss of ASSOC CNF
	**/
	lim_activate_cnf_timer(mac,
			       (uint16_t) ((tpLimMlmAssocInd)msg_buf)->aid,
			       pe_session);

	mac->lim.sme_msg_callback(mac, &msg);
} /*** end lim_process_mlm_assoc_ind() ***/

/**
 * lim_process_mlm_disassoc_ind() - This function is called to processes
 * MLM_DISASSOC_IND message from MLM State machine.
 * @mac:       Pointer to Global MAC structure
 * @msg_buf:    A pointer to the MLM message buffer
 *
 * Return None
 */
void lim_process_mlm_disassoc_ind(struct mac_context *mac, uint32_t *msg_buf)
{
	tLimMlmDisassocInd *pMlmDisassocInd;
	struct pe_session *pe_session;

	pMlmDisassocInd = (tLimMlmDisassocInd *)msg_buf;
	pe_session = pe_find_session_by_session_id(mac,
				pMlmDisassocInd->sessionId);
	if (!pe_session) {
		pe_err("Session Does not exist for given sessionID");
		return;
	}
	switch (GET_LIM_SYSTEM_ROLE(pe_session)) {
	case eLIM_STA_ROLE:
		pe_session->limSmeState = eLIM_SME_WT_DISASSOC_STATE;
		MTRACE(mac_trace
			       (mac, TRACE_CODE_SME_STATE, pe_session->peSessionId,
			       pe_session->limSmeState));
		break;
	default:        /* eLIM_AP_ROLE */
		pe_debug("*** Peer staId=%d Disassociated ***",
			       pMlmDisassocInd->aid);
		/* Send SME_DISASOC_IND after Polaris cleanup */
		/* (after receiving LIM_MLM_PURGE_STA_IND) */
		break;
	} /* end switch (GET_LIM_SYSTEM_ROLE(pe_session)) */
} /*** end lim_process_mlm_disassoc_ind() ***/

/**
 * lim_process_mlm_disassoc_cnf() - Processes disassociation
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: A pointer to the MLM message buffer
 *
 * This function is called to processes MLM_DISASSOC_CNF
 * message from MLM State machine.
 *
 * Return: None
 */
void lim_process_mlm_disassoc_cnf(struct mac_context *mac_ctx,
	uint32_t *msg)
{
	tSirResultCodes result_code;
	tLimMlmDisassocCnf *disassoc_cnf;
	struct pe_session *session_entry;

	disassoc_cnf = (tLimMlmDisassocCnf *) msg;

	session_entry =
		pe_find_session_by_session_id(mac_ctx, disassoc_cnf->sessionId);
	if (!session_entry) {
		pe_err("session Does not exist for given session Id");
		return;
	}
	result_code = (tSirResultCodes)(disassoc_cnf->disassocTrigger ==
		eLIM_LINK_MONITORING_DISASSOC) ?
		eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE :
		disassoc_cnf->resultCode;
	if (LIM_IS_STA_ROLE(session_entry)) {
		/* Disassociate Confirm from MLM */
		if ((session_entry->limSmeState != eLIM_SME_WT_DISASSOC_STATE)
			&& (session_entry->limSmeState !=
			eLIM_SME_WT_DEAUTH_STATE)) {
			/*
			 * Should not have received
			 * Disassocate confirm
			 * from MLM in other states.Log error
			 */
			pe_err("received MLM_DISASSOC_CNF in state %X",
				session_entry->limSmeState);
			return;
		}
		if (mac_ctx->lim.gLimRspReqd)
			mac_ctx->lim.gLimRspReqd = false;
		if (disassoc_cnf->disassocTrigger ==
			eLIM_PROMISCUOUS_MODE_DISASSOC) {
			if (disassoc_cnf->resultCode != eSIR_SME_SUCCESS)
				session_entry->limSmeState =
					session_entry->limPrevSmeState;
			else
				session_entry->limSmeState =
					eLIM_SME_OFFLINE_STATE;
			MTRACE(mac_trace
				(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));
		} else {
			if (disassoc_cnf->resultCode != eSIR_SME_SUCCESS)
				session_entry->limSmeState =
					session_entry->limPrevSmeState;
			else
				session_entry->limSmeState =
					eLIM_SME_IDLE_STATE;
			MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				session_entry->peSessionId,
				session_entry->limSmeState));
			lim_send_sme_disassoc_ntf(mac_ctx,
				disassoc_cnf->peerMacAddr, result_code,
				disassoc_cnf->disassocTrigger,
				disassoc_cnf->aid, session_entry->smeSessionId,
				session_entry);
		}
	} else if (LIM_IS_AP_ROLE(session_entry)) {
		lim_send_sme_disassoc_ntf(mac_ctx, disassoc_cnf->peerMacAddr,
			result_code, disassoc_cnf->disassocTrigger,
			disassoc_cnf->aid, session_entry->smeSessionId,
			session_entry);
	}
}

/**
 * lim_process_mlm_deauth_ind() - processes MLM_DEAUTH_IND
 * @mac_ctx: global mac structure
 * @deauth_ind: deauth indication
 *
 * This function is called to processes MLM_DEAUTH_IND
 * message from MLM State machine.
 *
 * Return: None
 */
static void lim_process_mlm_deauth_ind(struct mac_context *mac_ctx,
				       tLimMlmDeauthInd *deauth_ind)
{
	struct pe_session *session;
	uint8_t session_id;
	enum eLimSystemRole role;

	if (!deauth_ind) {
		pe_err("deauth_ind is null");
		return;
	}
	session = pe_find_session_by_bssid(mac_ctx,
					   deauth_ind->peerMacAddr,
					   &session_id);
	if (!session) {
		pe_err("session does not exist for Addr:" QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(deauth_ind->peerMacAddr));
		return;
	}
	role = GET_LIM_SYSTEM_ROLE(session);
	if (role == eLIM_STA_ROLE) {
		session->limSmeState = eLIM_SME_WT_DEAUTH_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
				 session->peSessionId, session->limSmeState));
	}
}

/**
 * lim_process_mlm_deauth_cnf()
 *
 ***FUNCTION:
 * This function is called to processes MLM_DEAUTH_CNF
 * message from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param mac       Pointer to Global MAC structure
 * @param msg_buf    A pointer to the MLM message buffer
 *
 * @return None
 */
void lim_process_mlm_deauth_cnf(struct mac_context *mac, uint32_t *msg_buf)
{
	uint16_t aid;
	tSirResultCodes resultCode;
	tLimMlmDeauthCnf *pMlmDeauthCnf;
	struct pe_session *pe_session;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	pMlmDeauthCnf = (tLimMlmDeauthCnf *)msg_buf;
	pe_session = pe_find_session_by_session_id(mac,
				pMlmDeauthCnf->sessionId);
	if (!pe_session) {
		pe_err("session does not exist for given session Id");
		return;
	}

	resultCode = (tSirResultCodes)
		     (pMlmDeauthCnf->deauthTrigger ==
		      eLIM_LINK_MONITORING_DEAUTH) ?
		     eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE :
		     pMlmDeauthCnf->resultCode;
	aid = LIM_IS_AP_ROLE(pe_session) ? pMlmDeauthCnf->aid : 1;
	if (LIM_IS_STA_ROLE(pe_session)) {
		/* Deauth Confirm from MLM */
		if ((pe_session->limSmeState != eLIM_SME_WT_DISASSOC_STATE)
			&& pe_session->limSmeState !=
					eLIM_SME_WT_DEAUTH_STATE) {
			/**
			 * Should not have received Deauth confirm
			 * from MLM in other states.
			 * Log error
			 */
			pe_err("received unexpected MLM_DEAUTH_CNF in state %X",
				       pe_session->limSmeState);
			return;
		}
		if (pMlmDeauthCnf->resultCode == eSIR_SME_SUCCESS) {
			pe_session->limSmeState = eLIM_SME_IDLE_STATE;
		} else
			pe_session->limSmeState =
				pe_session->limPrevSmeState;
		MTRACE(mac_trace
			       (mac, TRACE_CODE_SME_STATE, pe_session->peSessionId,
			       pe_session->limSmeState));

		if (mac->lim.gLimRspReqd)
			mac->lim.gLimRspReqd = false;
	}
	/* On STA or on BASIC AP, send SME_DEAUTH_RSP to host */
	lim_send_sme_deauth_ntf(mac, pMlmDeauthCnf->peer_macaddr.bytes,
				resultCode,
				pMlmDeauthCnf->deauthTrigger,
				aid, pe_session->smeSessionId);
} /*** end lim_process_mlm_deauth_cnf() ***/

/**
 * lim_process_mlm_purge_sta_ind()
 *
 ***FUNCTION:
 * This function is called to processes MLM_PURGE_STA_IND
 * message from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param mac       Pointer to Global MAC structure
 * @param msg_buf    A pointer to the MLM message buffer
 *
 * @return None
 */
void lim_process_mlm_purge_sta_ind(struct mac_context *mac, uint32_t *msg_buf)
{
	tSirResultCodes resultCode;
	tpLimMlmPurgeStaInd pMlmPurgeStaInd;
	struct pe_session *pe_session;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	pMlmPurgeStaInd = (tpLimMlmPurgeStaInd)msg_buf;
	pe_session = pe_find_session_by_session_id(mac,
				pMlmPurgeStaInd->sessionId);
	if (!pe_session) {
		pe_err("session does not exist for given bssId");
		return;
	}
	/* Purge STA indication from MLM */
	resultCode = (tSirResultCodes) pMlmPurgeStaInd->reasonCode;
	switch (GET_LIM_SYSTEM_ROLE(pe_session)) {
	case eLIM_STA_ROLE:
	default:        /* eLIM_AP_ROLE */
		if (LIM_IS_STA_ROLE(pe_session) &&
		   (pe_session->limSmeState !=
			eLIM_SME_WT_DISASSOC_STATE) &&
		   (pe_session->limSmeState != eLIM_SME_WT_DEAUTH_STATE)) {
			/**
			 * Should not have received
			 * Purge STA indication
			 * from MLM in other states.
			 * Log error
			 */
			pe_err("received unexpected MLM_PURGE_STA_IND in state %X",
				       pe_session->limSmeState);
			break;
		}
		pe_debug("*** Cleanup completed for staId=%d ***",
			       pMlmPurgeStaInd->aid);
		if (LIM_IS_STA_ROLE(pe_session)) {
			pe_session->limSmeState = eLIM_SME_IDLE_STATE;
			MTRACE(mac_trace
				       (mac, TRACE_CODE_SME_STATE,
				       pe_session->peSessionId,
				       pe_session->limSmeState));

		}
		if (pMlmPurgeStaInd->purgeTrigger == eLIM_PEER_ENTITY_DEAUTH) {
			lim_send_sme_deauth_ntf(mac,
						pMlmPurgeStaInd->peerMacAddr,
						resultCode,
						pMlmPurgeStaInd->purgeTrigger,
						pMlmPurgeStaInd->aid,
						pe_session->smeSessionId);
		} else
			lim_send_sme_disassoc_ntf(mac,
						  pMlmPurgeStaInd->peerMacAddr,
						  resultCode,
						  pMlmPurgeStaInd->purgeTrigger,
						  pMlmPurgeStaInd->aid,
						  pe_session->smeSessionId,
						  pe_session);
	} /* end switch (GET_LIM_SYSTEM_ROLE(pe_session)) */
} /*** end lim_process_mlm_purge_sta_ind() ***/

/**
 * lim_process_mlm_set_keys_cnf()
 *
 ***FUNCTION:
 * This function is called to processes MLM_SETKEYS_CNF
 * message from MLM State machine.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param mac       Pointer to Global MAC structure
 * @param msg_buf    A pointer to the MLM message buffer
 *
 * @return None
 */
void lim_process_mlm_set_keys_cnf(struct mac_context *mac, uint32_t *msg_buf)
{
	/* Prepare and send SME_SETCONTEXT_RSP message */
	tLimMlmSetKeysCnf *pMlmSetKeysCnf;
	struct pe_session *pe_session;
	uint16_t aid;
	tpDphHashNode sta_ds;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	pMlmSetKeysCnf = (tLimMlmSetKeysCnf *)msg_buf;
	pe_session = pe_find_session_by_session_id(mac,
					   pMlmSetKeysCnf->sessionId);
	if (!pe_session) {
		pe_err("session does not exist for given sessionId");
		return;
	}
	pe_session->is_key_installed = 0;
	pe_debug("Received MLM_SETKEYS_CNF with resultCode = %d",
		pMlmSetKeysCnf->resultCode);
	/* if the status is success keys are installed in the
	* Firmware so we can set the protection bit
	*/
	if (eSIR_SME_SUCCESS == pMlmSetKeysCnf->resultCode) {
		if (pMlmSetKeysCnf->key_len_nonzero)
			pe_session->is_key_installed = 1;
		sta_ds = dph_lookup_hash_entry(mac,
				pMlmSetKeysCnf->peer_macaddr.bytes,
				&aid, &pe_session->dph.dphHashTable);
		if (sta_ds && pMlmSetKeysCnf->key_len_nonzero)
			sta_ds->is_key_installed = 1;
	}
	pe_debug("is_key_installed = %d", pe_session->is_key_installed);

	lim_send_sme_set_context_rsp(mac,
				     pMlmSetKeysCnf->peer_macaddr,
				     1,
				     (tSirResultCodes) pMlmSetKeysCnf->resultCode,
				     pe_session, pe_session->smeSessionId);
} /*** end lim_process_mlm_set_keys_cnf() ***/

void lim_join_result_callback(struct mac_context *mac,
			      uint8_t vdev_id)
{
	struct pe_session *session;

	session = pe_find_session_by_vdev_id(mac, vdev_id);
	if (!session) {
		return;
	}
	lim_send_sme_join_reassoc_rsp(mac, eWNI_SME_JOIN_RSP,
				      session->result_code,
				      session->prot_status_code,
				      session, vdev_id);
	pe_delete_session(mac, session);
}

QDF_STATUS lim_sta_handle_connect_fail(join_params *param)
{
	struct pe_session *session;
	struct mac_context *mac_ctx;
	tpDphHashNode sta_ds = NULL;

	if (!param) {
		pe_err("param is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("Mac context is NULL");
		return QDF_STATUS_E_INVAL;
	}
	session = pe_find_session_by_session_id(mac_ctx, param->pe_session_id);
	if (!session) {
		pe_err("session is NULL");
		return QDF_STATUS_E_INVAL;
	}

	sta_ds = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				    &session->dph.dphHashTable);
	if (sta_ds) {
		sta_ds->mlmStaContext.disassocReason =
			REASON_UNSPEC_FAILURE;
		sta_ds->mlmStaContext.cleanupTrigger =
			eLIM_JOIN_FAILURE;
		sta_ds->mlmStaContext.resultCode = param->result_code;
		sta_ds->mlmStaContext.protStatusCode = param->prot_status_code;
		/*
		 * FIX_ME: at the end of lim_cleanup_rx_path,
		 * make sure PE is sending eWNI_SME_JOIN_RSP
		 * to SME
		 */
		lim_cleanup_rx_path(mac_ctx, sta_ds, session, true);
		qdf_mem_free(session->lim_join_req);
		session->lim_join_req = NULL;
		/* Cleanup if add bss failed */
		if (session->add_bss_failed) {
			dph_delete_hash_entry(mac_ctx,
				 sta_ds->staAddr, sta_ds->assocId,
				 &session->dph.dphHashTable);
			goto error;
		}
		return QDF_STATUS_SUCCESS;
	}
	qdf_mem_free(session->lim_join_req);
	session->lim_join_req = NULL;

error:
	/*
	 * Delete the session if JOIN failure occurred.
	 * if the peer is not created, then there is no
	 * need to send down the set link state which will
	 * try to delete the peer. Instead a join response
	 * failure should be sent to the upper layers.
	 */
	if (param->result_code != eSIR_SME_PEER_CREATE_FAILED) {
		QDF_STATUS status;

		session->prot_status_code = param->prot_status_code;
		session->result_code = param->result_code;

		status = wma_send_vdev_stop(session->smeSessionId);
		if (QDF_IS_STATUS_ERROR(status)) {
			lim_join_result_callback(mac_ctx,
						 session->smeSessionId);
		}

		return status;
	}


	lim_send_sme_join_reassoc_rsp(mac_ctx, eWNI_SME_JOIN_RSP,
				      param->result_code,
				      param->prot_status_code,
				      session, session->smeSessionId);
	if (param->result_code == eSIR_SME_PEER_CREATE_FAILED)
		pe_delete_session(mac_ctx, session);

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_handle_sme_join_result() - Handles sme join result
 * @mac_ctx:  Pointer to Global MAC structure
 * @result_code: Failure code to be sent
 * @prot_status_code : Protocol status code
 * @session_entry: PE session handle
 *
 * This function is called to process join/auth/assoc failures
 * upon receiving MLM_JOIN/AUTH/ASSOC_CNF with a failure code or
 * MLM_ASSOC_CNF with a success code in case of STA role and
 * MLM_JOIN_CNF with success in case of STA in IBSS role.
 *
 * Return: None
 */
void lim_handle_sme_join_result(struct mac_context *mac_ctx,
	tSirResultCodes result_code, uint16_t prot_status_code,
	struct pe_session *session)
{
	join_params param;
	QDF_STATUS status;

	if (!session) {
		pe_err("session is NULL");
		return;
	}

	if (result_code == eSIR_SME_SUCCESS) {
		wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					      WLAN_VDEV_SM_EV_START_SUCCESS,
					      0, NULL);
		return lim_send_sme_join_reassoc_rsp(mac_ctx, eWNI_SME_JOIN_RSP,
						     result_code,
						     prot_status_code, session,
						     session->smeSessionId);
	}

	param.result_code = result_code;
	param.prot_status_code = prot_status_code;
	param.pe_session_id = session->peSessionId;

	mlme_set_connection_fail(session->vdev, true);
	if (wlan_vdev_mlme_get_substate(session->vdev) ==
	    WLAN_VDEV_SS_START_START_PROGRESS)
		status = wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					       WLAN_VDEV_SM_EV_START_REQ_FAIL,
					       sizeof(param), &param);
	else
		status = wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					       WLAN_VDEV_SM_EV_CONNECTION_FAIL,
					       sizeof(param), &param);

	return;
}


/**
 * lim_process_mlm_add_sta_rsp()
 *
 ***FUNCTION:
 * This function is called to process a WMA_ADD_STA_RSP from HAL.
 * Upon receipt of this message from HAL, MLME -
 * > Determines the "state" in which this message was received
 * > Forwards it to the appropriate callback
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 *
 * @param  mac      Pointer to Global MAC structure
 * @param  struct scheduler_msg  The MsgQ header, which contains the
 *  response buffer
 *
 * @return None
 */
void lim_process_mlm_add_sta_rsp(struct mac_context *mac,
				 struct scheduler_msg *limMsgQ,
				 struct pe_session *pe_session)
{
	/* we need to process the deferred message since the initiating req. there might be nested request. */
	/* in the case of nested request the new request initiated from the response will take care of resetting */
	/* the deffered flag. */
	SET_LIM_PROCESS_DEFD_MESGS(mac, true);
	if (LIM_IS_AP_ROLE(pe_session)) {
		lim_process_ap_mlm_add_sta_rsp(mac, limMsgQ, pe_session);
		return;
	}
	lim_process_sta_mlm_add_sta_rsp(mac, limMsgQ, pe_session);
}

/**
 * lim_process_sta_mlm_add_sta_rsp () - Process add sta response
 * @mac_ctx:  Pointer to mac context
 * @msg:  struct scheduler_msg *an Message structure
 * @session_entry: PE session entry
 *
 * Process ADD STA response sent from WMA and posts results
 * to SME.
 *
 * Return: Null
 */

void lim_process_sta_mlm_add_sta_rsp(struct mac_context *mac_ctx,
	struct scheduler_msg *msg, struct pe_session *session_entry)
{
	tLimMlmAssocCnf mlm_assoc_cnf;
	tpDphHashNode sta_ds;
	uint32_t msg_type = LIM_MLM_ASSOC_CNF;
	tpAddStaParams add_sta_params = (tpAddStaParams) msg->bodyptr;

	if (!add_sta_params) {
		pe_err("Encountered NULL Pointer");
		return;
	}

	if (session_entry->limSmeState == eLIM_SME_WT_REASSOC_STATE)
		msg_type = LIM_MLM_REASSOC_CNF;

	if (true == session_entry->fDeauthReceived) {
		pe_err("Received Deauth frame in ADD_STA_RESP state");
		if (QDF_STATUS_SUCCESS == add_sta_params->status) {
			pe_err("ADD_STA success, send update result code with eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA limMlmState: %d bssid "QDF_MAC_ADDR_FMT,
				session_entry->limMlmState,
				QDF_MAC_ADDR_REF(add_sta_params->staMac));

			if (session_entry->limSmeState ==
					eLIM_SME_WT_REASSOC_STATE)
				msg_type = LIM_MLM_REASSOC_CNF;
			/*
			 * We are sending result code
			 * eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA which
			 * will trigger proper cleanup (DEL_STA/DEL_BSS both
			 * required) in either assoc cnf or reassoc cnf handler.
			 */
			mlm_assoc_cnf.resultCode =
				eSIR_SME_JOIN_DEAUTH_FROM_AP_DURING_ADD_STA;
			mlm_assoc_cnf.protStatusCode =
					   STATUS_UNSPECIFIED_FAILURE;
			goto end;
		}
	}

	if (QDF_STATUS_SUCCESS == add_sta_params->status) {
		if (eLIM_MLM_WT_ADD_STA_RSP_STATE !=
			session_entry->limMlmState) {
			pe_err("Received WMA_ADD_STA_RSP in state %X",
				session_entry->limMlmState);
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_REFUSED;
			goto end;
		}
		/*
		 * Update the DPH Hash Entry for this STA
		 * with proper state info
		 */
		sta_ds =
			dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				&session_entry->dph.dphHashTable);
		if (sta_ds) {
			sta_ds->mlmStaContext.mlmState =
				eLIM_MLM_LINK_ESTABLISHED_STATE;
			sta_ds->nss = add_sta_params->nss;
		} else
			pe_warn("Fail to get DPH Hash Entry for AID - %d",
				DPH_STA_HASH_INDEX_PEER);
		session_entry->limMlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
			session_entry->peSessionId,
			session_entry->limMlmState));

#ifdef WLAN_DEBUG
		mac_ctx->lim.gLimNumLinkEsts++;
#endif
#ifdef FEATURE_WLAN_TDLS
		/* initialize TDLS peer related data */
		lim_init_tdls_data(mac_ctx, session_entry);
#endif
		/*
		 * Return Assoc confirm to SME with success
		 * FIXME - Need the correct ASSOC RSP code to
		 * be passed in here
		 */
		mlm_assoc_cnf.resultCode = (tSirResultCodes) eSIR_SME_SUCCESS;
		lim_send_obss_color_collision_cfg(mac_ctx, session_entry,
					OBSS_COLOR_COLLISION_DETECTION);
		if (lim_is_session_he_capable(session_entry) && sta_ds) {
			if (mac_ctx->usr_cfg_mu_edca_params) {
				pe_debug("Send user cfg MU EDCA params to FW");
				lim_send_edca_params(mac_ctx,
					     mac_ctx->usr_mu_edca_params,
					     session_entry->vdev_id, true);
			} else if (session_entry->mu_edca_present) {
				pe_debug("Send MU EDCA params to FW");
				lim_send_edca_params(mac_ctx,
					session_entry->ap_mu_edca_params,
					session_entry->vdev_id, true);
			}
		}
	} else {
		pe_err("ADD_STA failed!");
		if (session_entry->limSmeState == eLIM_SME_WT_REASSOC_STATE)
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_FT_REASSOC_FAILURE;
		else
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_REFUSED;
		mlm_assoc_cnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;
	}
end:
	if (msg->bodyptr) {
		qdf_mem_free(add_sta_params);
		msg->bodyptr = NULL;
	}
	/* Updating PE session Id */
	mlm_assoc_cnf.sessionId = session_entry->peSessionId;
	lim_post_sme_message(mac_ctx, msg_type, (uint32_t *) &mlm_assoc_cnf);
	if (true == session_entry->fDeauthReceived)
		session_entry->fDeauthReceived = false;
	return;
}

void lim_process_mlm_del_bss_rsp(struct mac_context *mac,
				 struct del_bss_resp *vdev_stop_rsp,
				 struct pe_session *pe_session)
{
	/* we need to process the deferred message since the initiating req. there might be nested request. */
	/* in the case of nested request the new request initiated from the response will take care of resetting */
	/* the deffered flag. */
	SET_LIM_PROCESS_DEFD_MESGS(mac, true);
	mac->sys.gSysFrameCount[SIR_MAC_MGMT_FRAME][SIR_MAC_MGMT_DEAUTH] = 0;

	if (LIM_IS_AP_ROLE(pe_session) &&
	    (pe_session->statypeForBss == STA_ENTRY_SELF)) {
		lim_process_ap_mlm_del_bss_rsp(mac, vdev_stop_rsp, pe_session);
		return;
	}
	lim_process_sta_mlm_del_bss_rsp(mac, vdev_stop_rsp, pe_session);

#ifdef WLAN_FEATURE_11W
	if (pe_session->limRmfEnabled) {
		if (QDF_STATUS_SUCCESS !=
		    lim_send_exclude_unencrypt_ind(mac, true, pe_session)) {
			pe_err("Could not send down Exclude Unencrypted Indication!");
		}
	}
#endif
}

void lim_process_sta_mlm_del_bss_rsp(struct mac_context *mac,
				     struct del_bss_resp *vdev_stop_rsp,
				     struct pe_session *pe_session)
{
	tpDphHashNode sta =
		dph_get_hash_entry(mac, DPH_STA_HASH_INDEX_PEER,
				   &pe_session->dph.dphHashTable);
	tSirResultCodes status_code = eSIR_SME_SUCCESS;

	if (!vdev_stop_rsp) {
		pe_err("Invalid body pointer in message");
		goto end;
	}
	if (vdev_stop_rsp->status == QDF_STATUS_SUCCESS) {
		if (!sta) {
			pe_err("DPH Entry for STA 1 missing");
			status_code = eSIR_SME_REFUSED;
			goto end;
		}
		if (eLIM_MLM_WT_DEL_BSS_RSP_STATE !=
		    sta->mlmStaContext.mlmState) {
			pe_err("Received unexpected WMA_DEL_BSS_RSP in state %X",
				       sta->mlmStaContext.mlmState);
			status_code = eSIR_SME_REFUSED;
			goto end;
		}
		pe_debug("STA AssocID %d MAC "QDF_MAC_ADDR_FMT, sta->assocId,
			 QDF_MAC_ADDR_REF(sta->staAddr));
	} else {
		pe_err("DEL BSS failed!");
		status_code = eSIR_SME_STOP_BSS_FAILURE;
	}
end:
	if (!sta)
		return;
	if ((LIM_IS_STA_ROLE(pe_session)) &&
	    (pe_session->limSmeState !=
			eLIM_SME_WT_DISASSOC_STATE &&
	    pe_session->limSmeState !=
			eLIM_SME_WT_DEAUTH_STATE) &&
	    sta->mlmStaContext.cleanupTrigger !=
			eLIM_JOIN_FAILURE) {
		/** The Case where the DelBss is invoked from
		 *  context of other than normal DisAssoc / Deauth OR
		 *  as part of Join Failure.
		 */
		lim_handle_del_bss_in_re_assoc_context(mac, sta, pe_session);
		return;
	}
	lim_prepare_and_send_del_sta_cnf(mac, sta, status_code, pe_session);
	return;
}

void lim_process_ap_mlm_del_bss_rsp(struct mac_context *mac,
				    struct del_bss_resp *vdev_stop_rsp,
				    struct pe_session *pe_session)
{
	tSirResultCodes rc = eSIR_SME_SUCCESS;

	if (!pe_session) {
		pe_err("Session entry passed is NULL");
		if (vdev_stop_rsp)
			qdf_mem_free(vdev_stop_rsp);
		return;
	}

	if (!vdev_stop_rsp) {
		pe_err("BSS: DEL_BSS_RSP with no body!");
		rc = eSIR_SME_REFUSED;
		goto end;
	}
	mac->lim.gLimMlmState = eLIM_MLM_IDLE_STATE;
	MTRACE(mac_trace
		       (mac, TRACE_CODE_MLM_STATE, NO_SESSION,
		       mac->lim.gLimMlmState));

	if (eLIM_MLM_WT_DEL_BSS_RSP_STATE != pe_session->limMlmState) {
		pe_err("Received unexpected WMA_DEL_BSS_RSP in state %X",
			pe_session->limMlmState);
		rc = eSIR_SME_REFUSED;
		goto end;
	}
	if (vdev_stop_rsp->status != QDF_STATUS_SUCCESS) {
		pe_err("BSS: DEL_BSS_RSP error (%x)", vdev_stop_rsp->status);
		rc = eSIR_SME_STOP_BSS_FAILURE;
		goto end;
	}
	/** Softmac may send all the buffered packets right after resuming the transmission hence
	 * to occupy the medium during non channel occupancy period. So resume the transmission after
	 * HAL gives back the response.
	 */
	dph_hash_table_init(mac, &pe_session->dph.dphHashTable);
	lim_delete_pre_auth_list(mac);
	/* Initialize number of associated stations during cleanup */
	pe_session->gLimNumOfCurrentSTAs = 0;
end:
	lim_send_sme_rsp(mac, eWNI_SME_STOP_BSS_RSP, rc,
			 pe_session->smeSessionId);
	pe_delete_session(mac, pe_session);
}

/**
 * lim_process_mlm_del_all_sta_rsp() - Process DEL STA response
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: The MsgQ header, which contains the response buffer
 *
 * This function is called to process a WMA_DEL_ALL_STA_RSP from
 * WMA Upon receipt of this message from FW.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
lim_process_mlm_del_all_sta_rsp(struct vdev_mlme_obj *vdev_mlme,
				struct peer_delete_all_response *rsp)
{
	struct pe_session *session_entry;
	tSirResultCodes status_code = eSIR_SME_SUCCESS;
	struct mac_context *mac_ctx;
	struct wlan_objmgr_vdev *vdev;
	uint8_t vdev_id;

	vdev = vdev_mlme->vdev;
	vdev_id = wlan_vdev_get_id(vdev);

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("mac_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	SET_LIM_PROCESS_DEFD_MESGS(mac_ctx, true);

	session_entry = pe_find_session_by_vdev_id(mac_ctx,
						   vdev_id);
	if (!session_entry) {
		pe_err("Session Doesn't exist: %d", vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	lim_prepare_and_send_del_all_sta_cnf(mac_ctx, status_code,
					     session_entry);
	return QDF_STATUS_SUCCESS;
}

/**
 * lim_process_mlm_del_sta_rsp() - Process DEL STA response
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: The MsgQ header, which contains the response buffer
 *
 * This function is called to process a WMA_DEL_STA_RSP from
 * WMA Upon receipt of this message from FW.
 *
 * Return: None
 */
void lim_process_mlm_del_sta_rsp(struct mac_context *mac_ctx,
	struct scheduler_msg *msg)
{
	/*
	 * we need to process the deferred message since the
	 * initiating req. there might be nested request
	 * in the case of nested request the new request
	 * initiated from the response will take care of resetting
	 * the deffered flag.
	 */
	struct pe_session *session_entry;
	tpDeleteStaParams del_sta_params;

	del_sta_params = (tpDeleteStaParams) msg->bodyptr;
	if (!del_sta_params) {
		pe_err("null pointer del_sta_params msg");
		return;
	}
	SET_LIM_PROCESS_DEFD_MESGS(mac_ctx, true);

	session_entry = pe_find_session_by_session_id(mac_ctx,
				del_sta_params->sessionId);
	if (!session_entry) {
		pe_err("Session Doesn't exist: %d",
			del_sta_params->sessionId);
		qdf_mem_free(del_sta_params);
		msg->bodyptr = NULL;
		return;
	}

	if (LIM_IS_AP_ROLE(session_entry)) {
		lim_process_ap_mlm_del_sta_rsp(mac_ctx, msg,
				session_entry);
		return;
	}
	if (LIM_IS_NDI_ROLE(session_entry)) {
		lim_process_ndi_del_sta_rsp(mac_ctx, msg, session_entry);
		return;
	}
	lim_process_sta_mlm_del_sta_rsp(mac_ctx, msg, session_entry);
}

/**
 * lim_process_ap_mlm_del_sta_rsp() - Process WMA_DEL_STA_RSP
 * @mac_ctx: Global pointer to MAC context
 * @msg: Received message
 * @session_entry: Session entry
 *
 * Process WMA_DEL_STA_RSP for AP role
 *
 * Retunrn: None
 */
void lim_process_ap_mlm_del_sta_rsp(struct mac_context *mac_ctx,
					   struct scheduler_msg *msg,
					   struct pe_session *session_entry)
{
	tpDeleteStaParams del_sta_params = (tpDeleteStaParams) msg->bodyptr;
	tpDphHashNode sta_ds;
	tSirResultCodes status_code = eSIR_SME_SUCCESS;

	if (!msg->bodyptr) {
		pe_err("msg->bodyptr NULL");
		return;
	}

	sta_ds = dph_get_hash_entry(mac_ctx, del_sta_params->assocId,
				    &session_entry->dph.dphHashTable);
	if (!sta_ds) {
		pe_err("DPH Entry for STA %X missing",
			del_sta_params->assocId);
		status_code = eSIR_SME_REFUSED;
		qdf_mem_free(del_sta_params);
		msg->bodyptr = NULL;
		return;
	}
	pe_debug("Received del Sta Rsp in StaD MlmState: %d",
		sta_ds->mlmStaContext.mlmState);
	if (QDF_STATUS_SUCCESS != del_sta_params->status) {
		pe_warn("DEL STA failed!");
		status_code = eSIR_SME_REFUSED;
		goto end;
	}

	pe_debug("AP received the DEL_STA_RSP for assocID: %X sta mac "
		QDF_MAC_ADDR_FMT, del_sta_params->assocId,
		QDF_MAC_ADDR_REF(sta_ds->staAddr));
	if ((eLIM_MLM_WT_DEL_STA_RSP_STATE != sta_ds->mlmStaContext.mlmState) &&
	    (eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE !=
	     sta_ds->mlmStaContext.mlmState)) {
		pe_err("Received unexpected WMA_DEL_STA_RSP in state %s for assocId %d",
		       lim_mlm_state_str(sta_ds->mlmStaContext.mlmState),
			sta_ds->assocId);
		status_code = eSIR_SME_REFUSED;
		goto end;
	}

	pe_debug("Deleted STA AssocID %d", sta_ds->assocId);
	lim_print_mac_addr(mac_ctx, sta_ds->staAddr, LOGD);
	if (eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE ==
	    sta_ds->mlmStaContext.mlmState) {
		qdf_mem_free(del_sta_params);
		msg->bodyptr = NULL;
		if (lim_add_sta(mac_ctx, sta_ds, false, session_entry) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("could not Add STA with assocId: %d",
				sta_ds->assocId);
			/*
			 * delete the TS if it has already been added.
			 * send the response with error status.
			 */
			if (sta_ds->qos.addtsPresent) {
				tpLimTspecInfo pTspecInfo;

				if (QDF_STATUS_SUCCESS ==
				    lim_tspec_find_by_assoc_id(mac_ctx,
					sta_ds->assocId,
					&sta_ds->qos.addts.tspec,
					&mac_ctx->lim.tspecInfo[0],
					&pTspecInfo)) {
					lim_admit_control_delete_ts(mac_ctx,
						sta_ds->assocId,
						&sta_ds->qos.addts.tspec.tsinfo,
						NULL,
						&pTspecInfo->idx);
				}
			}
			lim_reject_association(mac_ctx, sta_ds->staAddr,
				sta_ds->mlmStaContext.subType, true,
				sta_ds->mlmStaContext.authType, sta_ds->assocId,
				true,
				STATUS_UNSPECIFIED_FAILURE,
				session_entry);
		}
		return;
	}
end:
	qdf_mem_free(del_sta_params);
	msg->bodyptr = NULL;
	if (eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE !=
	    sta_ds->mlmStaContext.mlmState) {
		lim_prepare_and_send_del_sta_cnf(mac_ctx, sta_ds, status_code,
						 session_entry);
	}
	return;
}

void lim_process_sta_mlm_del_sta_rsp(struct mac_context *mac,
				     struct scheduler_msg *limMsgQ,
				     struct pe_session *pe_session)
{
	tSirResultCodes status_code = eSIR_SME_SUCCESS;
	tpDeleteStaParams pDelStaParams = (tpDeleteStaParams) limMsgQ->bodyptr;

	if (!pDelStaParams) {
		pe_err("Encountered NULL Pointer");
		goto end;
	}
	pe_debug("Del STA RSP received. Status: %d AssocID: %d",
			pDelStaParams->status, pDelStaParams->assocId);

#ifdef FEATURE_WLAN_TDLS
	if (pDelStaParams->staType == STA_ENTRY_TDLS_PEER) {
		pe_debug("TDLS Del STA RSP received");
		lim_process_tdls_del_sta_rsp(mac, limMsgQ, pe_session);
		return;
	}
#endif
	if (QDF_STATUS_SUCCESS != pDelStaParams->status)
		pe_err("Del STA failed! Status:%d, proceeding with Del BSS",
			pDelStaParams->status);

	if (eLIM_MLM_WT_DEL_STA_RSP_STATE != pe_session->limMlmState) {
		pe_err("Received unexpected WDA_DELETE_STA_RSP in state %s",
			lim_mlm_state_str(pe_session->limMlmState));
		status_code = eSIR_SME_REFUSED;
		goto end;
	}
	/*
	 * we must complete all cleanup related to delSta before
	 * calling limDelBSS.
	 */
	if (0 != limMsgQ->bodyptr) {
		qdf_mem_free(pDelStaParams);
		limMsgQ->bodyptr = NULL;
	}

	lim_disconnect_complete(pe_session, true);

	return;
end:
	if (0 != limMsgQ->bodyptr) {
		qdf_mem_free(pDelStaParams);
		limMsgQ->bodyptr = NULL;
	}
	return;
}

void lim_process_ap_mlm_add_sta_rsp(struct mac_context *mac,
				    struct scheduler_msg *limMsgQ,
				    struct pe_session *pe_session)
{
	tpAddStaParams pAddStaParams = (tpAddStaParams) limMsgQ->bodyptr;
	tpDphHashNode sta = NULL;

	if (!pAddStaParams) {
		pe_err("Invalid body pointer in message");
		goto end;
	}

	sta =
		dph_get_hash_entry(mac, pAddStaParams->assocId,
				   &pe_session->dph.dphHashTable);
	if (!sta) {
		pe_err("DPH Entry for STA %X missing", pAddStaParams->assocId);
		goto end;
	}

	if (eLIM_MLM_WT_ADD_STA_RSP_STATE != sta->mlmStaContext.mlmState) {
		pe_err("Received unexpected WMA_ADD_STA_RSP in state %X",
			sta->mlmStaContext.mlmState);
		goto end;
	}
	if (QDF_STATUS_SUCCESS != pAddStaParams->status) {
		pe_err("Error! rcvd delSta rsp from HAL with status %d",
			       pAddStaParams->status);
		lim_reject_association(mac, sta->staAddr,
				       sta->mlmStaContext.subType,
				       true, sta->mlmStaContext.authType,
				       sta->assocId, true,
				       STATUS_UNSPECIFIED_FAILURE,
				       pe_session);
		goto end;
	}
	sta->nss = pAddStaParams->nss;
	/* if the AssocRsp frame is not acknowledged, then keep alive timer will take care of the state */
	sta->valid = 1;
	sta->mlmStaContext.mlmState = eLIM_MLM_WT_ASSOC_CNF_STATE;
	pe_debug("AddStaRsp Success.STA AssocID %d sta mac" QDF_MAC_ADDR_FMT,
		 sta->assocId, QDF_MAC_ADDR_REF(sta->staAddr));
	lim_print_mac_addr(mac, sta->staAddr, LOGD);

	/* For BTAMP-AP, the flow sequence shall be:
	 * 1) PE sends eWNI_SME_ASSOC_IND to SME
	 * 2) PE receives eWNI_SME_ASSOC_CNF from SME
	 * 3) BTAMP-AP sends Re/Association Response to BTAMP-STA
	 */
	if (lim_send_mlm_assoc_ind(mac, sta, pe_session) != QDF_STATUS_SUCCESS)
		lim_reject_association(mac, sta->staAddr,
				       sta->mlmStaContext.subType,
				       true, sta->mlmStaContext.authType,
				       sta->assocId, true,
				       STATUS_UNSPECIFIED_FAILURE,
				       pe_session);

	/* fall though to reclaim the original Add STA Response message */
end:
	if (0 != limMsgQ->bodyptr) {
		qdf_mem_free(pAddStaParams);
		limMsgQ->bodyptr = NULL;
	}
	return;
}

static void lim_process_ap_mlm_add_bss_rsp(struct mac_context *mac,
					   struct add_bss_rsp *add_bss_rsp)
{
	tLimMlmStartCnf mlmStartCnf;
	struct pe_session *pe_session;
	uint8_t isWepEnabled = false;

	if (!add_bss_rsp) {
		pe_err("Encountered NULL Pointer");
		return;
	}
	/* TBD: free the memory before returning, do it for all places where lookup fails. */
	pe_session = pe_find_session_by_vdev_id(mac, add_bss_rsp->vdev_id);
	if (!pe_session) {
		pe_err("session does not exist for vdev_id %d",
		       add_bss_rsp->vdev_id);
		return;
	}
	/* Update PE session Id */
	mlmStartCnf.sessionId = pe_session->peSessionId;
	if (QDF_IS_STATUS_SUCCESS(add_bss_rsp->status)) {
		pe_debug("WMA_ADD_BSS_RSP returned with QDF_STATUS_SUCCESS");
		/* Set MLME state */
		pe_session->limMlmState = eLIM_MLM_BSS_STARTED_STATE;
		pe_session->chainMask = add_bss_rsp->chain_mask;
		pe_session->smpsMode = add_bss_rsp->smps_mode;
		MTRACE(mac_trace
			       (mac, TRACE_CODE_MLM_STATE, pe_session->peSessionId,
			       pe_session->limMlmState));
		pe_session->limSystemRole = eLIM_AP_ROLE;

		sch_edca_profile_update(mac, pe_session);
		/* For dual AP case, delete pre auth node if any */
		lim_delete_pre_auth_list(mac);
		/* Check the SAP security configuration.If configured to
		 * WEP then max clients supported is 16
		 */
		if (pe_session->privacy) {
			if ((pe_session->gStartBssRSNIe.present)
			    || (pe_session->gStartBssWPAIe.present))
				pe_debug("WPA/WPA2 SAP configuration");
			else {
				if (mac->mlme_cfg->sap_cfg.assoc_sta_limit >
				    MAX_SUPPORTED_PEERS_WEP) {
					pe_debug("WEP SAP Configuration");
					mac->mlme_cfg->sap_cfg.assoc_sta_limit
					= MAX_SUPPORTED_PEERS_WEP;
					isWepEnabled = true;
				}
			}
		}
		lim_init_peer_idxpool(mac, pe_session);

		/* Start OLBC timer */
		if (tx_timer_activate
			    (&mac->lim.lim_timers.gLimUpdateOlbcCacheTimer) !=
		    TX_SUCCESS) {
			pe_err("tx_timer_activate failed");
		}

		/* Apply previously set configuration at HW */
		lim_apply_configuration(mac, pe_session);

		/* In lim_apply_configuration gLimAssocStaLimit is assigned from cfg.
		 * So update the value to 16 in case SoftAP is configured in WEP.
		 */
		if ((mac->mlme_cfg->sap_cfg.assoc_sta_limit >
		    MAX_SUPPORTED_PEERS_WEP)
		    && (isWepEnabled))
			mac->mlme_cfg->sap_cfg.assoc_sta_limit =
			MAX_SUPPORTED_PEERS_WEP;
		mlmStartCnf.resultCode = eSIR_SME_SUCCESS;
	} else {
		pe_err("WMA_ADD_BSS_REQ failed with status %d",
			add_bss_rsp->status);
		mlmStartCnf.resultCode = eSIR_SME_HAL_SEND_MESSAGE_FAIL;
	}

	lim_send_start_bss_confirm(mac, &mlmStartCnf);
}

#ifdef WLAN_FEATURE_FILS_SK
/*
 * lim_update_fils_auth_mode: API to update Auth mode in case of fils session
 * @session_entry: pe session entry
 * @auth_mode: auth mode needs to be updated
 *
 * Return: None
 */
static void lim_update_fils_auth_mode(struct pe_session *session_entry,
			tAniAuthType *auth_mode)
{
	if (!session_entry->fils_info)
		return;

	if (session_entry->fils_info->is_fils_connection)
		*auth_mode = session_entry->fils_info->auth;
}
#else
static void lim_update_fils_auth_mode(struct pe_session *session_entry,
			tAniAuthType *auth_mode)
{ }
#endif

void lim_process_sta_add_bss_rsp_pre_assoc(struct mac_context *mac_ctx,
					   struct bss_params *add_bss_params,
					   struct pe_session *session_entry,
					   QDF_STATUS status)
{
	tAniAuthType cfgAuthType, authMode;
	tLimMlmAuthReq *pMlmAuthReq;
	tpDphHashNode sta = NULL;

	if (!add_bss_params) {
		pe_err("Invalid body pointer in message");
		goto joinFailure;
	}
	if (QDF_IS_STATUS_SUCCESS(status)) {
		sta = dph_add_hash_entry(mac_ctx,
				add_bss_params->staContext.staMac,
				DPH_STA_HASH_INDEX_PEER,
				&session_entry->dph.dphHashTable);
		if (!sta) {
			/* Could not add hash table entry */
			pe_err("could not add hash entry at DPH for");
			lim_print_mac_addr(mac_ctx,
				add_bss_params->staContext.staMac, LOGE);
			goto joinFailure;
		}
		/* Success, handle below */
		/* Trigger Authentication with AP */
		cfgAuthType = mac_ctx->mlme_cfg->wep_params.auth_type;

		/* Try shared Authentication first */
		if (cfgAuthType == eSIR_AUTO_SWITCH)
			authMode = eSIR_SHARED_KEY;
		else
			authMode = cfgAuthType;

		lim_update_fils_auth_mode(session_entry, &authMode);
		/* Trigger MAC based Authentication */
		pMlmAuthReq = qdf_mem_malloc(sizeof(tLimMlmAuthReq));
		if (!pMlmAuthReq) {
			pe_err("Allocate Memory failed for mlmAuthReq");
			return;
		}
		sir_copy_mac_addr(pMlmAuthReq->peerMacAddr,
			session_entry->bssId);

		pMlmAuthReq->authType = authMode;
		session_entry->limMlmState = eLIM_MLM_JOINED_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
			session_entry->peSessionId, eLIM_MLM_JOINED_STATE));
		pMlmAuthReq->sessionId = session_entry->peSessionId;
		session_entry->limPrevSmeState = session_entry->limSmeState;
		session_entry->limSmeState = eLIM_SME_WT_AUTH_STATE;

		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
			session_entry->peSessionId,
			session_entry->limSmeState));
		lim_post_mlm_message(mac_ctx, LIM_MLM_AUTH_REQ,
			(uint32_t *) pMlmAuthReq);
		return;
	}

joinFailure:
	{
		session_entry->limSmeState = eLIM_SME_JOIN_FAILURE_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_SME_STATE,
			session_entry->peSessionId,
			session_entry->limSmeState));

		/* Send Join response to Host */
		lim_handle_sme_join_result(mac_ctx, eSIR_SME_REFUSED,
			STATUS_UNSPECIFIED_FAILURE, session_entry);
	}

}

static void lim_process_sta_mlm_add_bss_rsp(struct mac_context *mac_ctx,
					    struct add_bss_rsp *add_bss_rsp,
					    struct pe_session *session_entry)
{
	tLimMlmAssocCnf mlm_assoc_cnf;
	uint32_t msg_type = LIM_MLM_ASSOC_CNF;
	uint32_t sub_type = LIM_ASSOC;
	tpDphHashNode sta_ds = NULL;
	uint8_t update_sta = false;

	mlm_assoc_cnf.resultCode = eSIR_SME_SUCCESS;
	if (eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE == session_entry->limMlmState
		|| (eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE ==
		session_entry->limMlmState)) {
		msg_type = LIM_MLM_REASSOC_CNF;
		sub_type = LIM_REASSOC;
		/*
		 * If Reassoc is happening for the same BSS, then
		 * use the existing StaId and indicate to HAL to update
		 * the existing STA entry.
		 * If Reassoc is happening for the new BSS, then
		 * old BSS and STA entry would have been already deleted
		 * before PE tries to add BSS for the new BSS, so set the
		 * updateSta to false and pass INVALID STA Index.
		 */
		if (sir_compare_mac_addr(session_entry->bssId,
			session_entry->limReAssocbssId)) {
			update_sta = true;
		}
	}

	if (QDF_IS_STATUS_SUCCESS(add_bss_rsp->status)) {
		if (eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE ==
			session_entry->limMlmState) {
			pe_debug("Mlm=%d %d", session_entry->limMlmState,
				eLIM_MLM_WT_ADD_BSS_RSP_REASSOC_STATE);
			lim_process_sta_mlm_add_bss_rsp_ft(mac_ctx,
							   add_bss_rsp,
							   session_entry);
			return;
		}

		/* Set MLME state */
		session_entry->limMlmState = eLIM_MLM_WT_ADD_STA_RSP_STATE;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
			session_entry->peSessionId,
			session_entry->limMlmState));
		/* to know the session  started for self or for  peer  */
		session_entry->statypeForBss = STA_ENTRY_PEER;
		sta_ds =
			dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				&session_entry->dph.dphHashTable);
		if (!sta_ds) {
			pe_err("Session:%d Fail to add Self Entry for STA",
				session_entry->peSessionId);
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_REFUSED;
		} else {
			/* Success, handle below */
			/* Downgrade the EDCA parameters if needed */
			lim_set_active_edca_params(mac_ctx,
				session_entry->gLimEdcaParams, session_entry);
			lim_send_edca_params(mac_ctx,
				session_entry->gLimEdcaParamsActive,
				session_entry->vdev_id, false);
			if (lim_add_sta_self(mac_ctx, update_sta,
				session_entry) != QDF_STATUS_SUCCESS) {
				/* Add STA context at HW */
				pe_err("Session:%d could not Add Self"
					"Entry for the station",
					session_entry->peSessionId);
				mlm_assoc_cnf.resultCode =
					(tSirResultCodes) eSIR_SME_REFUSED;
			}
		}
	} else {
		pe_err("SessionId: %d ADD_BSS failed!",
			session_entry->peSessionId);
		mlm_assoc_cnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;
		/* Return Assoc confirm to SME with failure */
		if (eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE ==
				session_entry->limMlmState)
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_FT_REASSOC_FAILURE;
		else
			mlm_assoc_cnf.resultCode =
				(tSirResultCodes) eSIR_SME_REFUSED;
		session_entry->add_bss_failed = true;
	}

	if (mlm_assoc_cnf.resultCode != eSIR_SME_SUCCESS) {
		session_entry->limMlmState = eLIM_MLM_IDLE_STATE;
		/* Update PE session Id */
		mlm_assoc_cnf.sessionId = session_entry->peSessionId;
		lim_post_sme_message(mac_ctx, msg_type,
			(uint32_t *) &mlm_assoc_cnf);
	}
}

void lim_handle_add_bss_rsp(struct mac_context *mac_ctx,
			    struct add_bss_rsp *add_bss_rsp)
{
	tLimMlmStartCnf mlm_start_cnf;
	struct pe_session *session_entry;
	enum bss_type bss_type;
	struct wlan_lmac_if_reg_tx_ops *tx_ops;
	struct vdev_mlme_obj *mlme_obj;

	if (!add_bss_rsp) {
		pe_err("add_bss_rsp is NULL");
		return;
	}

	/*
	 * we need to process the deferred message since the
	 * initiating req.there might be nested request.
	 * in the case of nested request the new request initiated
	 * from the response will take care of resetting the deffered
	 * flag.
	 */
	SET_LIM_PROCESS_DEFD_MESGS(mac_ctx, true);
	/* Validate SME/LIM/MLME state */
	session_entry = pe_find_session_by_vdev_id(mac_ctx,
						   add_bss_rsp->vdev_id);
	if (!session_entry) {
		pe_err("vdev id:%d Session Doesn't exist",
		       add_bss_rsp->vdev_id);
		goto err;
	}
	if (LIM_IS_AP_ROLE(session_entry)) {
		if (wlan_reg_is_ext_tpc_supported(mac_ctx->psoc)) {
			mlme_obj =
			wlan_vdev_mlme_get_cmpt_obj(session_entry->vdev);
			if (!mlme_obj) {
				pe_err("vdev component object is NULL");
				goto err;
			}
			tx_ops = wlan_reg_get_tx_ops(mac_ctx->psoc);

			lim_calculate_tpc(mac_ctx, session_entry, false, 0,
					  false);

			if (tx_ops->set_tpc_power)
				tx_ops->set_tpc_power(mac_ctx->psoc,
						      session_entry->vdev_id,
						      &mlme_obj->reg_tpc_obj);
		}
	}
	bss_type = session_entry->bssType;
	/* update PE session Id */
	mlm_start_cnf.sessionId = session_entry->peSessionId;
	if (eSIR_NDI_MODE == session_entry->bssType) {
		lim_process_ndi_mlm_add_bss_rsp(mac_ctx, add_bss_rsp,
						session_entry);
	} else {
		if (eLIM_SME_WT_START_BSS_STATE == session_entry->limSmeState) {
			if (eLIM_MLM_WT_ADD_BSS_RSP_STATE !=
				session_entry->limMlmState) {
				pe_err("SessionId:%d Received "
					" WMA_ADD_BSS_RSP in state %X",
					session_entry->peSessionId,
					session_entry->limMlmState);
				mlm_start_cnf.resultCode =
					eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED;

				lim_send_start_bss_confirm(mac_ctx, &mlm_start_cnf);
			}
				lim_process_ap_mlm_add_bss_rsp(mac_ctx,
							       add_bss_rsp);
		} else {
			/* Called while processing assoc response */
			lim_process_sta_mlm_add_bss_rsp(mac_ctx, add_bss_rsp,
							session_entry);
		}
	}

#ifdef WLAN_FEATURE_11W
	if (session_entry->limRmfEnabled) {
		if (QDF_STATUS_SUCCESS !=
			lim_send_exclude_unencrypt_ind(mac_ctx, false,
				session_entry)) {
			pe_err("Failed to send Exclude Unencrypted Ind");
		}
	}
#endif
err:
	qdf_mem_free(add_bss_rsp);
}

void lim_process_mlm_update_hidden_ssid_rsp(struct mac_context *mac_ctx,
	uint8_t vdev_id)
{
	struct pe_session *session_entry;
	struct scheduler_msg message = {0};
	QDF_STATUS status;

	pe_debug("hidden ssid resp for vdev_id:%d ", vdev_id);

	session_entry = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session_entry) {
		pe_err("vdev_id:%d Session Doesn't exist",
		       vdev_id);
		return;
	}
	/* Update beacon */
	sch_set_fixed_beacon_fields(mac_ctx, session_entry);
	lim_send_beacon(mac_ctx, session_entry);

	message.type = eWNI_SME_HIDDEN_SSID_RESTART_RSP;
	message.bodyval = vdev_id;
	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_SME,
					QDF_MODULE_ID_SME, &message);

	if (status != QDF_STATUS_SUCCESS)
		pe_err("Failed to post message %u", status);
}

/**
 * lim_process_mlm_set_sta_key_rsp() - Process STA key response
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @msg: The MsgQ header, which contains the response buffer
 *
 * This function is called to process the following two
 * messages from HAL:
 * 1) WMA_SET_BSSKEY_RSP
 * 2) WMA_SET_STAKEY_RSP
 * 3) WMA_SET_STA_BCASTKEY_RSP
 * Upon receipt of this message from HAL,
 * MLME -
 * > Determines the "state" in which this message was received
 * > Forwards it to the appropriate callback
 * LOGIC:
 * WMA_SET_BSSKEY_RSP/WMA_SET_STAKEY_RSP can be
 * received by MLME while in the following state:
 * MLME state = eLIM_MLM_WT_SET_BSS_KEY_STATE --OR--
 * MLME state = eLIM_MLM_WT_SET_STA_KEY_STATE --OR--
 * MLME state = eLIM_MLM_WT_SET_STA_BCASTKEY_STATE
 * Based on this state, this API will determine where to
 * route the message to
 * Assumption:
 * ONLY the MLME state is being taken into account for now.
 * This is because, it appears that the handling of the
 * SETKEYS REQ is handled symmetrically on both the AP & STA
 *
 * Return: None
 */
void lim_process_mlm_set_sta_key_rsp(struct mac_context *mac_ctx,
	struct scheduler_msg *msg)
{
	uint8_t resp_reqd = 1;
	struct sLimMlmSetKeysCnf mlm_set_key_cnf;
	uint8_t session_id = 0;
	uint8_t vdev_id;
	struct pe_session *session_entry;
	uint16_t key_len;
	uint16_t result_status;
	tSetStaKeyParams *set_key_params;

	SET_LIM_PROCESS_DEFD_MESGS(mac_ctx, true);
	qdf_mem_zero((void *)&mlm_set_key_cnf, sizeof(tLimMlmSetKeysCnf));
	if (!msg->bodyptr) {
		pe_err("msg bodyptr is NULL");
		return;
	}
	set_key_params = msg->bodyptr;
	vdev_id = set_key_params->vdev_id;
	session_entry = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session_entry) {
		pe_err("session does not exist for given vdev_id %d", vdev_id);
		qdf_mem_zero(msg->bodyptr, sizeof(*set_key_params));
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		lim_send_sme_set_context_rsp(mac_ctx,
					     mlm_set_key_cnf.peer_macaddr,
					     0, eSIR_SME_INVALID_SESSION, NULL,
					     vdev_id);
		return;
	}

	session_id = session_entry->peSessionId;
	pe_debug("PE session ID %d, vdev_id %d", session_id, vdev_id);
	result_status = set_key_params->status;
	key_len = set_key_params->key[0].keyLength;

	if (result_status == eSIR_SME_SUCCESS && key_len)
		mlm_set_key_cnf.key_len_nonzero = true;
	else
		mlm_set_key_cnf.key_len_nonzero = false;


	MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
		session_entry->peSessionId, session_entry->limMlmState));
	if (resp_reqd) {
		tpLimMlmSetKeysReq lpLimMlmSetKeysReq =
			(tpLimMlmSetKeysReq) mac_ctx->lim.gpLimMlmSetKeysReq;
		/* Prepare and Send LIM_MLM_SETKEYS_CNF */
		if (lpLimMlmSetKeysReq) {
			qdf_copy_macaddr(&mlm_set_key_cnf.peer_macaddr,
					 &lpLimMlmSetKeysReq->peer_macaddr);
			/*
			 * Free the buffer cached for the global
			 * mac_ctx->lim.gpLimMlmSetKeysReq
			 */
			qdf_mem_zero(mac_ctx->lim.gpLimMlmSetKeysReq,
				     sizeof(tpLimMlmSetKeysReq));
			qdf_mem_free(mac_ctx->lim.gpLimMlmSetKeysReq);
			mac_ctx->lim.gpLimMlmSetKeysReq = NULL;
		} else {
			qdf_copy_macaddr(&mlm_set_key_cnf.peer_macaddr,
					 &set_key_params->macaddr);
		}
		mlm_set_key_cnf.sessionId = session_id;
		lim_post_sme_message(mac_ctx, LIM_MLM_SETKEYS_CNF,
			(uint32_t *) &mlm_set_key_cnf);
	}
	qdf_mem_zero(msg->bodyptr, sizeof(*set_key_params));
	qdf_mem_free(msg->bodyptr);
	msg->bodyptr = NULL;
}

/**
 * lim_process_mlm_set_bss_key_rsp() - handles BSS key
 *
 * @mac_ctx: A pointer to Global MAC structure
 * @msg: Message from SME
 *
 * This function processes BSS key response and updates
 * PE status accordingly.
 *
 * Return: NULL
 */
void lim_process_mlm_set_bss_key_rsp(struct mac_context *mac_ctx,
	struct scheduler_msg *msg)
{
	struct sLimMlmSetKeysCnf set_key_cnf;
	uint16_t result_status;
	uint8_t session_id = 0;
	uint8_t vdev_id;
	struct pe_session *session_entry;
	tpLimMlmSetKeysReq set_key_req;
	uint16_t key_len;

	SET_LIM_PROCESS_DEFD_MESGS(mac_ctx, true);
	qdf_mem_zero((void *)&set_key_cnf, sizeof(tLimMlmSetKeysCnf));
	if (!msg->bodyptr) {
		pe_err("msg bodyptr is null");
		return;
	}
	vdev_id = ((tpSetBssKeyParams) msg->bodyptr)->vdev_id;
	session_entry = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session_entry) {
		pe_err("session does not exist for vdev_id %d", vdev_id);
		qdf_mem_zero(msg->bodyptr, sizeof(tSetBssKeyParams));
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		lim_send_sme_set_context_rsp(mac_ctx, set_key_cnf.peer_macaddr,
					     0, eSIR_SME_INVALID_SESSION, NULL,
					     vdev_id);
		return;
	}

	session_id = session_entry->peSessionId;
	pe_debug("PE session ID %d, vdev_id %d", session_id, vdev_id);
	if (eLIM_MLM_WT_SET_BSS_KEY_STATE == session_entry->limMlmState) {
		result_status =
			(uint16_t)(((tpSetBssKeyParams)msg->bodyptr)->status);
		key_len = ((tpSetBssKeyParams)msg->bodyptr)->key[0].keyLength;
	} else {
		result_status =
			(uint16_t)(((tpSetBssKeyParams)msg->bodyptr)->status);
		key_len = ((tpSetBssKeyParams)msg->bodyptr)->key[0].keyLength;
	}

	pe_debug("limMlmState %d status %d key_len %d",
		 session_entry->limMlmState, result_status, key_len);

	if (result_status == eSIR_SME_SUCCESS && key_len)
		set_key_cnf.key_len_nonzero = true;
	else
		set_key_cnf.key_len_nonzero = false;

	MTRACE(mac_trace
		(mac_ctx, TRACE_CODE_MLM_STATE, session_entry->peSessionId,
		session_entry->limMlmState));
	set_key_req =
		(tpLimMlmSetKeysReq) mac_ctx->lim.gpLimMlmSetKeysReq;
	set_key_cnf.sessionId = session_id;

	/* Prepare and Send LIM_MLM_SETKEYS_CNF */
	if (set_key_req) {
		qdf_copy_macaddr(&set_key_cnf.peer_macaddr,
				 &set_key_req->peer_macaddr);
		/*
		 * Free the buffer cached for the
		 * global mac_ctx->lim.gpLimMlmSetKeysReq
		 */
		qdf_mem_zero(mac_ctx->lim.gpLimMlmSetKeysReq,
			     sizeof(*set_key_req));
		qdf_mem_free(mac_ctx->lim.gpLimMlmSetKeysReq);
		mac_ctx->lim.gpLimMlmSetKeysReq = NULL;
	} else {
		qdf_copy_macaddr(&set_key_cnf.peer_macaddr,
				 &((tpSetBssKeyParams)msg->bodyptr)->macaddr);
	}
	qdf_mem_zero(msg->bodyptr, sizeof(tSetBssKeyParams));
	qdf_mem_free(msg->bodyptr);
	msg->bodyptr = NULL;

	lim_post_sme_message(mac_ctx, LIM_MLM_SETKEYS_CNF,
		(uint32_t *) &set_key_cnf);
}

/**
 * lim_process_switch_channel_re_assoc_req()
 *
 ***FUNCTION:
 * This function is called to send the reassoc req mgmt frame after the
 * switchChannelRsp message is received from HAL.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  mac          - Pointer to Global MAC structure.
 * @param  pe_session - session related information.
 * @param  status        - channel switch success/failure.
 *
 * @return None
 */
static void lim_process_switch_channel_re_assoc_req(struct mac_context *mac,
						    struct pe_session *pe_session,
						    QDF_STATUS status)
{
	tLimMlmReassocCnf mlmReassocCnf;
	tLimMlmReassocReq *pMlmReassocReq;

	pMlmReassocReq =
		(tLimMlmReassocReq *) (pe_session->pLimMlmReassocReq);
	if (!pMlmReassocReq) {
		pe_err("pLimMlmReassocReq does not exist for given switchChanSession");
		mlmReassocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
		goto end;
	}

	if (status != QDF_STATUS_SUCCESS) {
		pe_err("Change channel failed!!");
		mlmReassocCnf.resultCode = eSIR_SME_CHANNEL_SWITCH_FAIL;
		goto end;
	}
	/* / Start reassociation failure timer */
	MTRACE(mac_trace
		       (mac, TRACE_CODE_TIMER_ACTIVATE, pe_session->peSessionId,
		       eLIM_REASSOC_FAIL_TIMER));
	if (tx_timer_activate(&mac->lim.lim_timers.gLimReassocFailureTimer)
	    != TX_SUCCESS) {
		pe_err("could not start Reassociation failure timer");
		/* Return Reassoc confirm with */
		/* Resources Unavailable */
		mlmReassocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
		goto end;
	}
	/* / Prepare and send Reassociation request frame */
	lim_send_reassoc_req_mgmt_frame(mac, pMlmReassocReq, pe_session);
	return;
end:
	/* Free up buffer allocated for reassocReq */
	if (pMlmReassocReq) {
		/* Update PE session Id */
		mlmReassocCnf.sessionId = pMlmReassocReq->sessionId;
		qdf_mem_free(pMlmReassocReq);
		pe_session->pLimMlmReassocReq = NULL;
	} else {
		mlmReassocCnf.sessionId = 0;
	}

	mlmReassocCnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;
	/* Update PE sessio Id */
	mlmReassocCnf.sessionId = pe_session->peSessionId;

	lim_post_sme_message(mac, LIM_MLM_REASSOC_CNF,
			     (uint32_t *) &mlmReassocCnf);
}


/**
 * lim_process_switch_channel_join_req() -Initiates probe request
 *
 * @mac_ctx - A pointer to Global MAC structure
 * @pe_session - session related information.
 * @status        - channel switch success/failure
 *
 * This function is called to send the probe req mgmt frame
 * after the switchChannelRsp message is received from HAL.
 *
 * Return None
 */
static void lim_process_switch_channel_join_req(
	struct mac_context *mac_ctx, struct pe_session *session_entry,
	QDF_STATUS status)
{
	tSirMacSSid ssId;
	tLimMlmJoinCnf join_cnf;
	uint8_t nontx_bss_id = 0;
	struct bss_description *bss;
	struct vdev_mlme_obj *mlme_obj;
	struct wlan_lmac_if_reg_tx_ops *tx_ops;
	bool tpe_change = false;

	if (status != QDF_STATUS_SUCCESS) {
		pe_err("Change channel failed!!");
		goto error;
	}

	if ((!session_entry) || (!session_entry->pLimMlmJoinReq) ||
	    (!session_entry->lim_join_req)) {
		pe_err("invalid pointer!!");
		goto error;
	}

	bss = &session_entry->lim_join_req->bssDescription;
	nontx_bss_id = bss->mbssid_info.profile_num;

	session_entry->limPrevMlmState = session_entry->limMlmState;
	session_entry->limMlmState = eLIM_MLM_WT_JOIN_BEACON_STATE;

	/* Apply previously set configuration at HW */
	lim_apply_configuration(mac_ctx, session_entry);

	/*
	* If deauth_before_connection is enabled, Send Deauth first to AP if
	* last disconnection was caused by HB failure.
	*/
	if (mac_ctx->mlme_cfg->sta.deauth_before_connection) {
		int apCount;

		for (apCount = 0; apCount < 2; apCount++) {

			if (!qdf_mem_cmp(session_entry->pLimMlmJoinReq->bssDescription.bssId,
				mac_ctx->lim.gLimHeartBeatApMac[apCount], sizeof(tSirMacAddr))) {

				pe_err("Index %d Sessionid: %d Send deauth on "
				"channel freq %d to BSSID: " QDF_MAC_ADDR_FMT,
				apCount,
				session_entry->peSessionId,
				session_entry->curr_op_freq,
				QDF_MAC_ADDR_REF(
				session_entry->pLimMlmJoinReq->bssDescription.bssId));

				lim_send_deauth_mgmt_frame(mac_ctx, REASON_UNSPEC_FAILURE,
					session_entry->pLimMlmJoinReq->bssDescription.bssId,
					session_entry, false);

				qdf_mem_zero(mac_ctx->lim.gLimHeartBeatApMac[apCount],
					sizeof(tSirMacAddr));
				break;
			}
		}
	}

	/*
	 * MBSSID: Non Tx BSS may or may not respond to unicast
	 * probe request.So dont send unicast probe request
	 * and wait for the probe response/ beacon to post JOIN CNF
	 */
	if (nontx_bss_id) {
		session_entry->limMlmState = eLIM_MLM_JOINED_STATE;
		join_cnf.sessionId = session_entry->peSessionId;
		join_cnf.resultCode = eSIR_SME_SUCCESS;
		join_cnf.protStatusCode = STATUS_SUCCESS;
		lim_post_sme_message(mac_ctx, LIM_MLM_JOIN_CNF,
				     (uint32_t *)&join_cnf);
		return;
	}

	/* Wait for Beacon to announce join success */
	qdf_mem_copy(ssId.ssId,
		session_entry->ssId.ssId, session_entry->ssId.length);
	ssId.length = session_entry->ssId.length;

	lim_deactivate_and_change_timer(mac_ctx,
		eLIM_PERIODIC_JOIN_PROBE_REQ_TIMER);

	/* assign appropriate sessionId to the timer object */
	mac_ctx->lim.lim_timers.gLimPeriodicJoinProbeReqTimer.sessionId =
		session_entry->peSessionId;
	pe_debug("vdev %d Send Probe req on freq %d %.*s  " QDF_MAC_ADDR_FMT, session_entry->vdev_id,
		 session_entry->curr_op_freq, ssId.length, ssId.ssId,
		 QDF_MAC_ADDR_REF(
		 session_entry->pLimMlmJoinReq->bssDescription.bssId));

	/*
	 * We need to wait for probe response, so start join
	 * timeout timer.This timer will be deactivated once
	 * we receive probe response.
	 */
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_TIMER_ACTIVATE,
		session_entry->peSessionId, eLIM_JOIN_FAIL_TIMER));
	if (tx_timer_activate(&mac_ctx->lim.lim_timers.gLimJoinFailureTimer) !=
		TX_SUCCESS) {
		pe_err("couldn't activate Join failure timer");
		session_entry->limMlmState = session_entry->limPrevMlmState;
		MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
			 session_entry->peSessionId,
			 mac_ctx->lim.gLimMlmState));
		goto error;
	}

	if (wlan_reg_is_ext_tpc_supported(mac_ctx->psoc)) {
		tx_ops = wlan_reg_get_tx_ops(mac_ctx->psoc);

		lim_process_tpe_ie_from_beacon(mac_ctx, session_entry, bss,
					       &tpe_change);

		mlme_obj = wlan_vdev_mlme_get_cmpt_obj(session_entry->vdev);
		if (!mlme_obj) {
			pe_err("vdev component object is NULL");
			goto error;
		}

		lim_calculate_tpc(mac_ctx, session_entry, false, 0, false);

		if (tx_ops->set_tpc_power)
			tx_ops->set_tpc_power(mac_ctx->psoc,
					      session_entry->vdev_id,
					      &mlme_obj->reg_tpc_obj);
	}
	/* include additional IE if there is */
	lim_send_probe_req_mgmt_frame(mac_ctx, &ssId,
		session_entry->pLimMlmJoinReq->bssDescription.bssId,
		session_entry->curr_op_freq,
		session_entry->self_mac_addr,
		session_entry->dot11mode,
		&session_entry->lim_join_req->addIEScan.length,
		session_entry->lim_join_req->addIEScan.addIEdata);

	/* Activate Join Periodic Probe Req timer */
	if (tx_timer_activate
		(&mac_ctx->lim.lim_timers.gLimPeriodicJoinProbeReqTimer)
		!= TX_SUCCESS) {
		pe_err("Periodic JoinReq timer activate failed");
		goto error;
	}

	return;
error:
	if (session_entry) {
		if (session_entry->pLimMlmJoinReq) {
			qdf_mem_free(session_entry->pLimMlmJoinReq);
			session_entry->pLimMlmJoinReq = NULL;
		}
		if (session_entry->lim_join_req) {
			qdf_mem_free(session_entry->lim_join_req);
			session_entry->lim_join_req = NULL;
		}
		join_cnf.sessionId = session_entry->peSessionId;
	} else {
		join_cnf.sessionId = 0;
	}
	join_cnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
	join_cnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;
	lim_post_sme_message(mac_ctx, LIM_MLM_JOIN_CNF, (uint32_t *)&join_cnf);
}

static void lim_handle_mon_switch_channel_rsp(struct pe_session *session,
					      QDF_STATUS status)
{
	struct scheduler_msg message = {0};

	if (session->bssType != eSIR_MONITOR_MODE)
		return;

	if (QDF_IS_STATUS_ERROR(status)) {
		enum wlan_vdev_sm_evt event = WLAN_VDEV_SM_EV_START_REQ_FAIL;

		pe_err("Set channel failed for monitor mode vdev substate %d",
			wlan_vdev_mlme_get_substate(session->vdev));

		if (QDF_IS_STATUS_SUCCESS(
		    wlan_vdev_is_restart_progress(session->vdev)))
			event = WLAN_VDEV_SM_EV_RESTART_REQ_FAIL;

		wlan_vdev_mlme_sm_deliver_evt(session->vdev, event, 0, NULL);

		return;
	}

	wlan_vdev_mlme_sm_deliver_evt(session->vdev,
				      WLAN_VDEV_SM_EV_START_SUCCESS, 0, NULL);

	message.type = eWNI_SME_MONITOR_MODE_VDEV_UP;
	message.bodyval = session->vdev_id;
	pe_debug("vdev id %d ", session->vdev_id);

	if (QDF_STATUS_SUCCESS !=
	    scheduler_post_message(QDF_MODULE_ID_PE,
				   QDF_MODULE_ID_SME,
				   QDF_MODULE_ID_SME, &message)) {
		pe_err("Failed to post message montior mode vdev up");
	}
}

/**
 * lim_process_switch_channel_rsp()
 *
 ***FUNCTION:
 * This function is called to process switchChannelRsp message from HAL.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  mac    - Pointer to Global MAC structure
 * @param  body - message body.
 *
 * @return None
 */
void lim_process_switch_channel_rsp(struct mac_context *mac,
				    struct vdev_start_response *rsp)
{
	QDF_STATUS status;
	uint16_t channelChangeReasonCode;
	struct pe_session *pe_session;
	struct wlan_channel *vdev_chan;
	/* we need to process the deferred message since the initiating req. there might be nested request. */
	/* in the case of nested request the new request initiated from the response will take care of resetting */
	/* the deffered flag. */
	SET_LIM_PROCESS_DEFD_MESGS(mac, true);
	status = rsp->status;

	pe_session = pe_find_session_by_vdev_id(mac, rsp->vdev_id);
	if (!pe_session) {
		pe_err("session does not exist for given sessionId");
		return;
	}
	pe_session->ch_switch_in_progress = false;
	channelChangeReasonCode = pe_session->channelChangeReasonCode;
	/* initialize it back to invalid id */
	pe_session->chainMask = rsp->chain_mask;
	pe_session->smpsMode = rsp->smps_mode;
	pe_session->channelChangeReasonCode = 0xBAD;

	vdev_chan = wlan_vdev_mlme_get_des_chan(pe_session->vdev);

	if (WLAN_REG_IS_24GHZ_CH_FREQ(vdev_chan->ch_freq)) {
		if (vdev_chan->ch_phymode == WLAN_PHYMODE_11B)
			pe_session->nwType = eSIR_11B_NW_TYPE;
		else
			pe_session->nwType = eSIR_11G_NW_TYPE;
	} else {
		pe_session->nwType = eSIR_11A_NW_TYPE;
	}
	pe_debug("new network type for peer: %d", pe_session->nwType);
	switch (channelChangeReasonCode) {
	case LIM_SWITCH_CHANNEL_REASSOC:
		lim_process_switch_channel_re_assoc_req(mac, pe_session, status);
		break;
	case LIM_SWITCH_CHANNEL_JOIN:
		lim_process_switch_channel_join_req(mac, pe_session, status);
		break;

	case LIM_SWITCH_CHANNEL_OPERATION:
	case LIM_SWITCH_CHANNEL_HT_WIDTH:
		/*
		 * The above code should also use the callback.
		 * mechanism below, there is scope for cleanup here.
		 * THat way all this response handler does is call the call back
		 * We can get rid of the reason code here.
		 */
		if (mac->lim.gpchangeChannelCallback)
			mac->lim.gpchangeChannelCallback(mac, status,
							  mac->lim.
							  gpchangeChannelData,
							  pe_session);

		/* If MCC upgrade/DBS downgrade happended during channel switch,
		 * the policy manager connection table needs to be updated.
		 */
		policy_mgr_update_connection_info(mac->psoc,
			pe_session->smeSessionId);
		if (pe_session->opmode == QDF_P2P_CLIENT_MODE) {
			pe_debug("Send p2p operating channel change conf action frame once first beacon is received on new channel");
			pe_session->send_p2p_conf_frame = true;
		}

		if (ucfg_pkt_capture_get_pktcap_mode(mac->psoc))
			ucfg_pkt_capture_record_channel(pe_session->vdev);
		break;
	case LIM_SWITCH_CHANNEL_SAP_DFS:
		/* Note: This event code specific to SAP mode
		 * When SAP session issues channel change as performing
		 * DFS, we will come here. Other sessions, for e.g. P2P
		 * will have to define their own event code and channel
		 * switch handler. This is required since the SME may
		 * require completely different information for P2P unlike
		 * SAP.
		 */
		lim_send_sme_ap_channel_switch_resp(mac, pe_session, rsp);
		/* If MCC upgrade/DBS downgrade happended during channel switch,
		 * the policy manager connection table needs to be updated.
		 */
		policy_mgr_update_connection_info(mac->psoc,
						pe_session->smeSessionId);
		break;
	case LIM_SWITCH_CHANNEL_MONITOR:
		lim_handle_mon_switch_channel_rsp(pe_session, status);
		/*
		 * If MCC upgrade/DBS downgrade happended during channel switch,
		 * the policy manager connection table needs to be updated.
		 */
		policy_mgr_update_connection_info(mac->psoc,
						  pe_session->smeSessionId);
		if (ucfg_pkt_capture_get_pktcap_mode(mac->psoc))
			ucfg_pkt_capture_record_channel(pe_session->vdev);
		break;
	default:
		break;
	}
}

QDF_STATUS lim_send_beacon_ind(struct mac_context *mac,
			       struct pe_session *pe_session,
			       enum sir_bcn_update_reason reason)
{
	struct beacon_gen_params *params;
	struct scheduler_msg msg = {0};

	if (!pe_session) {
		pe_err("Error:Unable to get the PESessionEntry");
		return QDF_STATUS_E_INVAL;
	}
	params = qdf_mem_malloc(sizeof(*params));
	if (!params)
		return QDF_STATUS_E_NOMEM;
	qdf_mem_copy(params->bssid, pe_session->bssId, QDF_MAC_ADDR_SIZE);
	msg.bodyptr = params;

	return sch_process_pre_beacon_ind(mac, &msg, reason);
}

void lim_process_rx_channel_status_event(struct mac_context *mac_ctx, void *buf)
{
	struct lim_channel_status *chan_status = buf;

	if (!chan_status) {
		QDF_TRACE(QDF_MODULE_ID_PE,
			  QDF_TRACE_LEVEL_ERROR,
			  "%s: ACS evt report buf NULL", __func__);
		return;
	}

	if (mac_ctx->sap.acs_with_more_param)
		lim_add_channel_status_info(mac_ctx, chan_status,
					    chan_status->channel_id);
	else
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_WARN,
			  "%s: Error evt report", __func__);

	qdf_mem_free(buf);

	return;
}
