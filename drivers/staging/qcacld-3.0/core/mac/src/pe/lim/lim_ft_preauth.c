/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
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

/**
 * DOC: lim_ft_preauth.c
 *
 * Pre-Authentication implementation for host based roaming
 */
#include <lim_send_messages.h>
#include <lim_types.h>
#include <lim_ft.h>
#include <lim_ft_defs.h>
#include <lim_utils.h>
#include <lim_prop_exts_utils.h>
#include <lim_assoc_utils.h>
#include <lim_session.h>
#include <lim_session_utils.h>
#include <lim_admit_control.h>
#include <wlan_scan_ucfg_api.h>
#include "wma.h"

/**
 * lim_ft_cleanup_pre_auth_info() - Cleanup preauth related information
 * @mac: Global MAC Context
 * @pe_session: PE Session
 *
 * This routine is called to free the FT context, session and other
 * information used during preauth operation.
 *
 * Return: None
 */
void lim_ft_cleanup_pre_auth_info(struct mac_context *mac,
		struct pe_session *pe_session)
{
	struct pe_session *pReAssocSessionEntry = NULL;
	uint8_t sessionId = 0;

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return;
	}

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("pe_session is not in STA mode");
		return;
	}

	if (pe_session->ftPEContext.pFTPreAuthReq) {
		pReAssocSessionEntry =
			pe_find_session_by_bssid(mac,
						 pe_session->ftPEContext.
						 pFTPreAuthReq->preAuthbssId,
						 &sessionId);

		if (pe_session->ftPEContext.pFTPreAuthReq->
		    pbssDescription) {
			qdf_mem_free(pe_session->ftPEContext.pFTPreAuthReq->
				     pbssDescription);
			pe_session->ftPEContext.pFTPreAuthReq->
			pbssDescription = NULL;
		}
		qdf_mem_free(pe_session->ftPEContext.pFTPreAuthReq);
		pe_session->ftPEContext.pFTPreAuthReq = NULL;
	}

	if (pe_session->ftPEContext.pAddBssReq) {
		qdf_mem_free(pe_session->ftPEContext.pAddBssReq);
		pe_session->ftPEContext.pAddBssReq = NULL;
	}

	if (pe_session->ftPEContext.pAddStaReq) {
		qdf_mem_free(pe_session->ftPEContext.pAddStaReq);
		pe_session->ftPEContext.pAddStaReq = NULL;
	}

	/* The session is being deleted, cleanup the contents */
	qdf_mem_zero(&pe_session->ftPEContext, sizeof(tftPEContext));

	/* Delete the session created while handling pre-auth response */
	if (pReAssocSessionEntry) {
		/* If we have successful pre-auth response, then we would have
		 * created a session on which reassoc request will be sent
		 */
		if (pReAssocSessionEntry->valid &&
		    pReAssocSessionEntry->limSmeState ==
		    eLIM_SME_WT_REASSOC_STATE) {
			QDF_TRACE(QDF_MODULE_ID_PE,
				  QDF_TRACE_LEVEL_DEBUG,
				  FL("Deleting Preauth session(%d)"),
				  pReAssocSessionEntry->peSessionId);
			pe_delete_session(mac, pReAssocSessionEntry);
		}
	}
}

/*
 * lim_process_ft_pre_auth_req() - process ft pre auth req
 *
 * @mac_ctx:    global mac ctx
 * @msg:        pointer to message
 *
 * In this function, we process the FT Pre Auth Req:
 *   We receive Pre-Auth, suspend link, register a call back. In the call back,
 *   we will need to accept frames from the new bssid. Send out the auth req to
 *   new AP. Start timer and when the timer is done or if we receive the Auth
 *   response. We change channel. Resume link
 *
 * Return: value to indicate if buffer was consumed
 */
int lim_process_ft_pre_auth_req(struct mac_context *mac_ctx,
				struct scheduler_msg *msg)
{
	int buf_consumed = false;
	struct pe_session *session;
	uint8_t session_id;
	tpSirFTPreAuthReq ft_pre_auth_req = (tSirFTPreAuthReq *) msg->bodyptr;

	if (!ft_pre_auth_req) {
		pe_err("tSirFTPreAuthReq is NULL");
		return buf_consumed;
	}

	/* Get the current session entry */
	session = pe_find_session_by_bssid(mac_ctx,
					   ft_pre_auth_req->currbssId,
					   &session_id);
	if (!session) {
		pe_err("Unable to find session for the bssid "
			QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(ft_pre_auth_req->currbssId));
		/* Post the FT Pre Auth Response to SME */
		lim_post_ft_pre_auth_rsp(mac_ctx, QDF_STATUS_E_FAILURE, NULL, 0,
					 session);
		buf_consumed = true;
		return buf_consumed;
	}

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(session)) {
		pe_err("session is not in STA mode");
		buf_consumed = true;
		return buf_consumed;
	}

	/* Can set it only after sending auth */
	session->ftPEContext.ftPreAuthStatus = QDF_STATUS_E_FAILURE;
	session->ftPEContext.ftPreAuthSession = true;

	/* Indicate that this is the session on which preauth is being done */
	if (session->ftPEContext.pFTPreAuthReq) {
		if (session->ftPEContext.pFTPreAuthReq->pbssDescription) {
			qdf_mem_free(
			  session->ftPEContext.pFTPreAuthReq->pbssDescription);
			session->ftPEContext.pFTPreAuthReq->pbssDescription =
									NULL;
		}
		qdf_mem_free(session->ftPEContext.pFTPreAuthReq);
		session->ftPEContext.pFTPreAuthReq = NULL;
	}

	/* We need information from the Pre-Auth Req. Lets save that */
	session->ftPEContext.pFTPreAuthReq = ft_pre_auth_req;

	pe_debug("PRE Auth ft_ies_length=%02x%02x%02x",
		session->ftPEContext.pFTPreAuthReq->ft_ies[0],
		session->ftPEContext.pFTPreAuthReq->ft_ies[1],
		session->ftPEContext.pFTPreAuthReq->ft_ies[2]);
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_PRE_AUTH_REQ_EVENT,
			      session, 0, 0);
#endif

	/*
	 * Dont need to suspend if APs are in same channel and DUT
	 * is not in MCC state
	 */
	if ((session->curr_op_freq !=
	     session->ftPEContext.pFTPreAuthReq->pre_auth_channel_freq)
	    || lim_is_in_mcc(mac_ctx)) {
		/* Need to suspend link only if the channels are different */
		pe_debug("Performing pre-auth on diff channel(session %pK)",
			session);
		lim_send_preauth_scan_offload(mac_ctx, session,
					session->ftPEContext.pFTPreAuthReq);
	} else {
		pe_debug("Performing pre-auth on same channel (session %pK)",
			session);
		/* We are in the same channel. Perform pre-auth */
		lim_perform_ft_pre_auth(mac_ctx, QDF_STATUS_SUCCESS, NULL,
					session);
	}

	return buf_consumed;
}

/**
 * lim_perform_ft_pre_auth() - Perform preauthentication
 * @mac: Global MAC Context
 * @status: Status Code
 * @data: pre-auth data
 * @pe_session: PE Session
 *
 * This routine will trigger the sending of authentication frame
 * to the peer.
 *
 * Return: None
 */
void lim_perform_ft_pre_auth(struct mac_context *mac, QDF_STATUS status,
			     uint32_t *data, struct pe_session *pe_session)
{
	tSirMacAuthFrameBody authFrame;
	unsigned int session_id;
	enum csr_akm_type auth_type;

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return;
	}
	session_id = pe_session->smeSessionId;
	auth_type =
		mac->roam.roamSession[session_id].connectedProfile.AuthType;

	if (pe_session->is11Rconnection &&
	    pe_session->ftPEContext.pFTPreAuthReq) {
		/* Only 11r assoc has FT IEs */
		if ((auth_type != eCSR_AUTH_TYPE_OPEN_SYSTEM) &&
			(pe_session->ftPEContext.pFTPreAuthReq->ft_ies_length
									== 0)) {
			pe_err("FTIEs for Auth Req Seq 1 is absent");
			goto preauth_fail;
		}
	}

	if (status != QDF_STATUS_SUCCESS) {
		pe_err("Change channel not successful for FT pre-auth");
		goto preauth_fail;
	}

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("pe_session is not in STA mode");
		return;
	}
	pe_debug("Entered wait auth2 state for FT (old session %pK)",
			pe_session);
	if (pe_session->is11Rconnection) {
		/* Now we are on the right channel and need to send out Auth1
		 * and receive Auth2
		 */
		authFrame.authAlgoNumber = eSIR_FT_AUTH;
	} else {
		/* Will need to make isESEconnection a enum may be for further
		 * improvements to this to match this algorithm number
		 */
		authFrame.authAlgoNumber = eSIR_OPEN_SYSTEM;
	}
	authFrame.authTransactionSeqNumber = SIR_MAC_AUTH_FRAME_1;
	authFrame.authStatusCode = 0;

	mac->lim.lim_timers.g_lim_periodic_auth_retry_timer.sessionId =
				pe_session->peSessionId;

	/* Start timer here to come back to operating channel */
	mac->lim.lim_timers.gLimFTPreAuthRspTimer.sessionId =
		pe_session->peSessionId;
	if (TX_SUCCESS !=
	    tx_timer_activate(&mac->lim.lim_timers.gLimFTPreAuthRspTimer)) {
		pe_err("FT Auth Rsp Timer Start Failed");
		goto preauth_fail;
	}
	MTRACE(mac_trace(mac, TRACE_CODE_TIMER_ACTIVATE,
		pe_session->peSessionId, eLIM_FT_PREAUTH_RSP_TIMER));

	pe_debug("FT Auth Rsp Timer Started");
#ifdef FEATURE_WLAN_DIAG_SUPPORT
	lim_diag_event_report(mac, WLAN_PE_DIAG_ROAM_AUTH_START_EVENT,
			mac->lim.pe_session, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
#endif
	if (pe_session->ftPEContext.pFTPreAuthReq)
		lim_send_auth_mgmt_frame(mac, &authFrame,
			 pe_session->ftPEContext.pFTPreAuthReq->preAuthbssId,
			 LIM_NO_WEP_IN_FC, pe_session);

	return;

preauth_fail:
	lim_handle_ft_pre_auth_rsp(mac, QDF_STATUS_E_FAILURE, NULL, 0, pe_session);
	return;
}

/**
 * lim_ft_setup_auth_session() - Fill the FT Session
 * @mac: Global MAC Context
 * @pe_session: PE Session
 *
 * Setup the session and the add bss req for the pre-auth AP.
 *
 * Return: Success or Failure Status
 */
QDF_STATUS lim_ft_setup_auth_session(struct mac_context *mac,
					struct pe_session *pe_session)
{
	struct pe_session *ft_session = NULL;
	uint8_t sessionId = 0;
	struct sSirFTPreAuthReq *req;

	ft_session =
		pe_find_session_by_bssid(mac, pe_session->limReAssocbssId,
					 &sessionId);
	if (!ft_session) {
		pe_err("No session found for bssid");
		lim_print_mac_addr(mac, pe_session->limReAssocbssId, LOGE);
		return QDF_STATUS_E_FAILURE;
	}

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("pe_session is not in STA mode");
		return QDF_STATUS_E_FAILURE;
	}

	req = pe_session->ftPEContext.pFTPreAuthReq;
	if (req && req->pbssDescription) {
		lim_fill_ft_session(mac,
				    req->pbssDescription, ft_session,
				    pe_session, WLAN_PHYMODE_AUTO);
		lim_ft_prepare_add_bss_req(mac, ft_session,
					   req->pbssDescription);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_ft_process_pre_auth_result() - Process the Auth frame
 * @mac: Global MAC context
 * @pe_session: PE Session
 *
 * Return: None
 */
static void lim_ft_process_pre_auth_result(struct mac_context *mac,
					   struct pe_session *pe_session)
{
	if (!pe_session ||
	    !pe_session->ftPEContext.pFTPreAuthReq)
		return;

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("pe_session is not in STA mode");
		return;
	}

	if (pe_session->ftPEContext.ftPreAuthStatus == QDF_STATUS_SUCCESS) {
		pe_session->ftPEContext.ftPreAuthStatus =
			lim_ft_setup_auth_session(mac, pe_session);
	}
	/* Post the FT Pre Auth Response to SME */
	lim_post_ft_pre_auth_rsp(mac,
		pe_session->ftPEContext.ftPreAuthStatus,
		pe_session->ftPEContext.saved_auth_rsp,
		pe_session->ftPEContext.saved_auth_rsp_length,
		pe_session);
}

/**
 * lim_handle_ft_pre_auth_rsp() - Handle the Auth response
 * @mac: Global MAC Context
 * @status: Status Code
 * @auth_rsp: Auth Response
 * @auth_rsp_length: Auth response length
 * @pe_session: PE Session
 *
 * Send the FT Pre Auth Response to SME whenever we have a status
 * ready to be sent to SME
 *
 * SME will be the one to send it up to the supplicant to receive
 * FTIEs which will be required for Reassoc Req.
 *
 * @Return: None
 */
void lim_handle_ft_pre_auth_rsp(struct mac_context *mac, QDF_STATUS status,
				uint8_t *auth_rsp, uint16_t auth_rsp_length,
				struct pe_session *pe_session)
{
	struct pe_session *ft_session = NULL;
	uint8_t sessionId = 0;
	struct bss_description *pbssDescription = NULL;
#ifdef FEATURE_WLAN_DIAG_SUPPORT
	lim_diag_event_report(mac, WLAN_PE_DIAG_PRE_AUTH_RSP_EVENT,
			      pe_session, (uint16_t) status, 0);
#endif

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("pe_session is not in STA mode");
		return;
	}

	/* Save the status of pre-auth */
	pe_session->ftPEContext.ftPreAuthStatus = status;

	/* Save the auth rsp, so we can send it to
	 * SME once we resume link
	 */
	pe_session->ftPEContext.saved_auth_rsp_length = 0;
	if ((auth_rsp) && (auth_rsp_length < MAX_FTIE_SIZE)) {
		qdf_mem_copy(pe_session->ftPEContext.saved_auth_rsp,
			     auth_rsp, auth_rsp_length);
		pe_session->ftPEContext.saved_auth_rsp_length =
			auth_rsp_length;
	}

	if (!pe_session->ftPEContext.pFTPreAuthReq ||
	    !pe_session->ftPEContext.pFTPreAuthReq->pbssDescription) {
		pe_err("pFTPreAuthReq or pbssDescription is NULL");
		return;
	}

	/* Create FT session for the re-association at this point */
	if (pe_session->ftPEContext.ftPreAuthStatus == QDF_STATUS_SUCCESS) {
		pbssDescription =
		      pe_session->ftPEContext.pFTPreAuthReq->pbssDescription;
		ft_session =
			pe_create_session(mac, pbssDescription->bssId,
					  &sessionId,
					  mac->lim.max_sta_of_pe_session,
					  pe_session->bssType,
					  pe_session->vdev_id,
					  pe_session->opmode);
		if (!ft_session) {
			pe_err("Session not created for pre-auth 11R AP");
			status = QDF_STATUS_E_FAILURE;
			pe_session->ftPEContext.ftPreAuthStatus = status;
			goto send_rsp;
		}

		sir_copy_mac_addr(ft_session->self_mac_addr,
				  pe_session->self_mac_addr);
		sir_copy_mac_addr(ft_session->limReAssocbssId,
				  pbssDescription->bssId);

		/* Update the beacon/probe filter in mac_ctx */
		lim_set_bcn_probe_filter(mac, ft_session, 0);

		if (ft_session->bssType == eSIR_INFRASTRUCTURE_MODE)
			ft_session->limSystemRole = eLIM_STA_ROLE;
		else
			pe_err("Invalid bss type");

		ft_session->limPrevSmeState = ft_session->limSmeState;
		ft_session->ht_config = pe_session->ht_config;
		ft_session->limSmeState = eLIM_SME_WT_REASSOC_STATE;

		if (wlan_reg_is_24ghz_ch_freq(pe_session->ftPEContext.
		    pFTPreAuthReq->pre_auth_channel_freq))
			ft_session->vdev_nss = mac->vdev_type_nss_2g.sta;
		else
			ft_session->vdev_nss = mac->vdev_type_nss_5g.sta;

		pe_debug("created session (%pK) with id = %d",
			ft_session, ft_session->peSessionId);

		/* Update the ReAssoc BSSID of the current session */
		sir_copy_mac_addr(pe_session->limReAssocbssId,
				  pbssDescription->bssId);
		lim_print_mac_addr(mac, pe_session->limReAssocbssId, LOGD);
	}
send_rsp:
	if ((pe_session->curr_op_freq !=
	     pe_session->ftPEContext.pFTPreAuthReq->pre_auth_channel_freq) ||
	    lim_is_in_mcc(mac)) {
		/* Need to move to the original AP channel */
		lim_process_abort_scan_ind(mac, pe_session->smeSessionId,
			pe_session->ftPEContext.pFTPreAuthReq->scan_id,
			mac->lim.req_id | PREAUTH_REQUESTOR_ID);
	} else {
		pe_debug("Pre auth on same freq as connected AP freq %d and no mcc pe sessions exist",
			 pe_session->ftPEContext.pFTPreAuthReq->
			 pre_auth_channel_freq);
		lim_ft_process_pre_auth_result(mac, pe_session);
	}
}

/*
 * lim_process_ft_preauth_rsp_timeout() - process ft preauth rsp timeout
 *
 * @mac_ctx:		global mac ctx
 *
 * This function is called if preauth response is not received from the AP
 * within this timeout while FT in progress
 *
 * Return: void
 */
void lim_process_ft_preauth_rsp_timeout(struct mac_context *mac_ctx)
{
	struct pe_session *session;

	/*
	 * We have failed pre auth. We need to resume link and get back on
	 * home channel
	 */
	pe_err("FT Pre-Auth Time Out!!!!");
	session = pe_find_session_by_session_id(mac_ctx,
		     mac_ctx->lim.lim_timers.gLimFTPreAuthRspTimer.sessionId);
	if (!session) {
		pe_err("Session Does not exist for given sessionID");
		return;
	}

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(session)) {
		pe_err("session is not in STA mode");
		return;
	}

	/* Reset the flag to indicate preauth request session */
	session->ftPEContext.ftPreAuthSession = false;

	if (!session->ftPEContext.pFTPreAuthReq) {
		/* Auth Rsp might already be posted to SME and ftcleanup done */
		pe_err("pFTPreAuthReq is NULL sessionId: %d",
		       mac_ctx->lim.lim_timers.gLimFTPreAuthRspTimer.sessionId);
		return;
	}

	/*
	 * To handle the race condition where we receive preauth rsp after
	 * timer has expired.
	 */
	if (true ==
	    session->ftPEContext.pFTPreAuthReq->bPreAuthRspProcessed) {
		pe_err("Auth rsp already posted to SME (session %pK)",
			session);
		return;
	} else {
		/*
		 * Here we are sending preauth rsp with failure state
		 * and which is forwarded to SME. Now, if we receive an preauth
		 * resp from AP with success it would create a FT pesession, but
		 * will be dropped in SME leaving behind the pesession. Mark
		 * Preauth rsp processed so that any rsp from AP is dropped in
		 * lim_process_auth_frame_no_session.
		 */
		pe_debug("Auth rsp not yet posted to SME (session %pK)",
			session);
		session->ftPEContext.pFTPreAuthReq->bPreAuthRspProcessed = true;
	}

	/*
	 * Attempted at Pre-Auth and failed. If we are off channel. We need
	 * to get back to home channel
	 */
	lim_handle_ft_pre_auth_rsp(mac_ctx, QDF_STATUS_E_FAILURE, NULL, 0, session);
}

/*
 * lim_post_ft_pre_auth_rsp() - post ft pre auth response to SME.
 *
 * @mac_ctx:		global mac ctx
 * @status:		status code to post in auth rsp
 * @auth_rsp:		pointer to auth rsp FT ie
 * @auth_rsp_length:	len of the IE field
 * @session:	        pe session
 *
 * post pre auth response to SME.
 *
 * Return: void
 */
void lim_post_ft_pre_auth_rsp(struct mac_context *mac_ctx,
			      QDF_STATUS status,
			      uint8_t *auth_rsp,
			      uint16_t auth_rsp_length,
			      struct pe_session *session)
{
	tpSirFTPreAuthRsp ft_pre_auth_rsp;
	struct scheduler_msg mmh_msg = {0};
	uint16_t rsp_len = sizeof(tSirFTPreAuthRsp);

	ft_pre_auth_rsp = qdf_mem_malloc(rsp_len);
	if (!ft_pre_auth_rsp) {
		QDF_ASSERT(ft_pre_auth_rsp);
		return;
	}

	pe_debug("Auth Rsp = %pK", ft_pre_auth_rsp);
	if (session) {
		/* Nothing to be done if the session is not in STA mode */
		if (!LIM_IS_STA_ROLE(session)) {
			pe_err("session is not in STA mode");
			qdf_mem_free(ft_pre_auth_rsp);
			return;
		}
		ft_pre_auth_rsp->vdev_id = session->vdev_id;
		/* The bssid of the AP we are sending Auth1 to. */
		if (session->ftPEContext.pFTPreAuthReq)
			sir_copy_mac_addr(ft_pre_auth_rsp->preAuthbssId,
			    session->ftPEContext.pFTPreAuthReq->preAuthbssId);
	}

	ft_pre_auth_rsp->messageType = eWNI_SME_FT_PRE_AUTH_RSP;
	ft_pre_auth_rsp->length = (uint16_t) rsp_len;
	ft_pre_auth_rsp->status = status;

	/* Attach the auth response now back to SME */
	ft_pre_auth_rsp->ft_ies_length = 0;
	if ((auth_rsp) && (auth_rsp_length < MAX_FTIE_SIZE)) {
		/* Only 11r assoc has FT IEs */
		qdf_mem_copy(ft_pre_auth_rsp->ft_ies,
			     auth_rsp, auth_rsp_length);
		ft_pre_auth_rsp->ft_ies_length = auth_rsp_length;
	}

	if (status != QDF_STATUS_SUCCESS) {
		/*
		 * Ensure that on Pre-Auth failure the cached Pre-Auth Req and
		 * other allocated memory is freed up before returning.
		 */
		pe_debug("Pre-Auth Failed, Cleanup!");
		lim_ft_cleanup(mac_ctx, session);
	}

	mmh_msg.type = ft_pre_auth_rsp->messageType;
	mmh_msg.bodyptr = ft_pre_auth_rsp;
	mmh_msg.bodyval = 0;

	pe_debug("Posted Auth Rsp to SME with status of 0x%x", status);
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	if (status == QDF_STATUS_SUCCESS)
		lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_PREAUTH_DONE,
				      session, status, 0);
#endif
	lim_sys_process_mmh_msg_api(mac_ctx, &mmh_msg);
}

/**
 * lim_send_preauth_scan_offload() - Send scan command to handle preauth.
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @session_entry: pe session
 * @ft_preauth_req: Preauth request with parameters
 *
 * Builds a single channel scan request and sends it to scan module.
 * Scan dwell time is the time allocated to go to preauth candidate
 * channel for auth frame exchange.
 *
 * Return: Status of sending message to scan module.
 */
QDF_STATUS lim_send_preauth_scan_offload(struct mac_context *mac_ctx,
					 struct pe_session *session_entry,
					 tSirFTPreAuthReq *ft_preauth_req)
{
	struct scan_start_request *req;
	struct wlan_objmgr_vdev *vdev;
	uint8_t session_id;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!session_entry) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
			  FL("Session entry is NULL"));
		return QDF_STATUS_E_FAILURE;
	}

	session_id = session_entry->smeSessionId;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		return QDF_STATUS_E_NOMEM;

	qdf_mem_zero(req, sizeof(*req));

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac_ctx->pdev,
						    session_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
			  FL("vdev object is NULL"));
		qdf_mem_free(req);
		return QDF_STATUS_E_FAILURE;
	}

	ucfg_scan_init_default_params(vdev, req);

	qdf_mem_copy(req->scan_req.bssid_list,
		     (uint8_t *)ft_preauth_req->currbssId,
		     QDF_MAC_ADDR_SIZE);

	req->scan_req.scan_id = ucfg_scan_get_scan_id(mac_ctx->psoc);
	if (!req->scan_req.scan_id) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
		qdf_mem_free(req);
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_ERROR,
			  FL("Invalid scan ID"));
		return QDF_STATUS_E_FAILURE;
	}
	ft_preauth_req->scan_id = req->scan_req.scan_id;
	req->scan_req.vdev_id = session_id;
	req->scan_req.scan_req_id = mac_ctx->lim.req_id | PREAUTH_REQUESTOR_ID;
	req->scan_req.scan_priority = SCAN_PRIORITY_VERY_HIGH;
	req->scan_req.scan_f_passive = true;

	req->scan_req.chan_list.num_chan = 1;
	req->scan_req.chan_list.chan[0].freq =
			ft_preauth_req->pre_auth_channel_freq;

	req->scan_req.dwell_time_active = LIM_FT_PREAUTH_SCAN_TIME;
	req->scan_req.dwell_time_passive = LIM_FT_PREAUTH_SCAN_TIME;

	status = ucfg_scan_start(req);
	if (status != QDF_STATUS_SUCCESS)
		/* Don't free req here, ucfg_scan_start will do free */
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_INFO_HIGH,
			  FL("Issue scan req failed"));
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
	return status;
}

void lim_preauth_scan_event_handler(struct mac_context *mac_ctx,
				    enum sir_scan_event_type event,
				    uint8_t vdev_id, uint32_t scan_id)
{
	struct pe_session *session_entry;

	session_entry = pe_find_session_by_scan_id(mac_ctx, scan_id);
	/* Pre-auth request is sent */
	if (session_entry) {
		if ((event == SIR_SCAN_EVENT_FOREIGN_CHANNEL) &&
		    (session_entry->ftPEContext.ftPreAuthStatus
		     == QDF_STATUS_SUCCESS)) {
			pe_err("Pre-auth is done, skip sending pre-auth req");
			return;
		}
	} else {
		/* For the first pre-auth request
		 * need to get it by sme session id (vdev id)
		 */
		session_entry = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	}

	if (!session_entry) {
		pe_err("vdev_id :%d PeSessionId:%d does not exist", vdev_id,
			mac_ctx->lim.lim_timers.gLimFTPreAuthRspTimer.sessionId);
		return;
	}

	switch (event) {
	case SIR_SCAN_EVENT_START_FAILED:
		/* Scan command is rejected by firmware */
		pe_err("Failed to start preauth scan");
		lim_post_ft_pre_auth_rsp(mac_ctx, QDF_STATUS_E_FAILURE, NULL, 0,
					 session_entry);
		return;

	case SIR_SCAN_EVENT_COMPLETED:
		/*
		 * Scan either completed successfully or or got terminated
		 * after successful auth, or timed out. Either way, STA
		 * is back to home channel. Data traffic can continue.
		 */
		lim_ft_process_pre_auth_result(mac_ctx, session_entry);
		break;

	case SIR_SCAN_EVENT_FOREIGN_CHANNEL:
		/* Sta is on candidate channel. Send auth */
		lim_perform_ft_pre_auth(mac_ctx, QDF_STATUS_SUCCESS, NULL,
					session_entry);
		break;
	default:
		/* Don't print message for scan events that are ignored */
		break;
	}
}

