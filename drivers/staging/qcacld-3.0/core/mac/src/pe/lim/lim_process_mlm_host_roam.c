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
 * DOC: lim_process_mlm_host_roam.c
 *
 * Host based roaming MLM implementation
 */
#include "cds_api.h"
#include "wni_cfg.h"
#include "ani_global.h"
#include "sir_api.h"
#include "sir_params.h"

#include "sch_api.h"
#include "utils_api.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_prop_exts_utils.h"
#include "lim_security_utils.h"
#include "lim_send_messages.h"
#include "lim_send_messages.h"
#include "lim_session_utils.h"
#include <lim_ft.h>
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
#include "host_diag_core_log.h"
#endif
#include "wma_if.h"
#include "rrm_api.h"
#include "wma.h"

static void lim_handle_sme_reaasoc_result(struct mac_context *, tSirResultCodes,
		uint16_t, struct pe_session *);
/**
 * lim_process_mlm_reassoc_req() - process mlm reassoc request.
 *
 * @mac_ctx:     pointer to Global MAC structure
 * @msg:  pointer to the MLM message buffer
 *
 * This function is called to process MLM_REASSOC_REQ message
 * from SME
 *
 * Return: None
 */
void lim_process_mlm_reassoc_req(struct mac_context *mac_ctx,
				 tLimMlmReassocReq *reassoc_req)
{
	struct tLimPreAuthNode *auth_node;
	tLimMlmReassocCnf reassoc_cnf;
	struct pe_session *session;

	session = pe_find_session_by_session_id(mac_ctx,
			reassoc_req->sessionId);
	if (!session) {
		pe_err("Session Does not exist for given sessionId: %d",
			reassoc_req->sessionId);
		qdf_mem_free(reassoc_req);
		return;
	}

	pe_debug("ReAssoc Req on session: %d role: %d mlm: %d " QDF_MAC_ADDR_FMT,
		reassoc_req->sessionId, GET_LIM_SYSTEM_ROLE(session),
		session->limMlmState,
		QDF_MAC_ADDR_REF(reassoc_req->peerMacAddr));

	if (LIM_IS_AP_ROLE(session) ||
		(session->limMlmState !=
		eLIM_MLM_LINK_ESTABLISHED_STATE)) {
		/*
		 * Received Reassoc request in invalid state or
		 * in AP role.Return Reassoc confirm with Invalid
		 * parameters code.
		 */

		pe_warn("unexpect msg state: %X role: %d MAC" QDF_MAC_ADDR_FMT,
			session->limMlmState, GET_LIM_SYSTEM_ROLE(session),
			QDF_MAC_ADDR_REF(reassoc_req->peerMacAddr));
		lim_print_mlm_state(mac_ctx, LOGW, session->limMlmState);
		reassoc_cnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
		reassoc_cnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;
		goto end;
	}

	if (session->pLimMlmReassocReq)
		qdf_mem_free(session->pLimMlmReassocReq);

	/*
	 * Hold Re-Assoc request as part of Session, knock-out mac_ctx
	 * Hold onto Reassoc request parameters
	 */
	session->pLimMlmReassocReq = reassoc_req;

	/* See if we have pre-auth context with new AP */
	auth_node = lim_search_pre_auth_list(mac_ctx, session->limReAssocbssId);

	if (!auth_node && (qdf_mem_cmp(reassoc_req->peerMacAddr,
					    session->bssId,
					    sizeof(tSirMacAddr)))) {
		/*
		 * Either pre-auth context does not exist AND
		 * we are not reassociating with currently
		 * associated AP.
		 * Return Reassoc confirm with not authenticated
		 */
		reassoc_cnf.resultCode = eSIR_SME_STA_NOT_AUTHENTICATED;
		reassoc_cnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;

		goto end;
	}
	/* assign the sessionId to the timer object */
	mac_ctx->lim.lim_timers.gLimReassocFailureTimer.sessionId =
		reassoc_req->sessionId;
	session->limPrevMlmState = session->limMlmState;
	session->limMlmState = eLIM_MLM_WT_REASSOC_RSP_STATE;
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE, session->peSessionId,
			 session->limMlmState));

	/* Apply previously set configuration at HW */
	lim_apply_configuration(mac_ctx, session);

	/* store the channel switch reason code in the lim global var */
	session->channelChangeReasonCode = LIM_SWITCH_CHANNEL_REASSOC;
	mlme_set_chan_switch_in_progress(session->vdev, true);
	wlan_vdev_mlme_sm_deliver_evt(session->vdev,
				      WLAN_VDEV_SM_EV_FW_VDEV_RESTART,
				      sizeof(struct pe_session), session);
	return;
end:
	reassoc_cnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;
	/* Update PE sessio Id */
	reassoc_cnf.sessionId = reassoc_req->sessionId;
	/* Free up buffer allocated for reassocReq */
	qdf_mem_free(reassoc_req);
	session->pLimReAssocReq = NULL;
	lim_post_sme_message(mac_ctx, LIM_MLM_REASSOC_CNF,
			     (uint32_t *) &reassoc_cnf);
}

/**
 * lim_handle_sme_reaasoc_result() - Handle the reassoc result
 * @mac: Global MAC Context
 * @resultCode: Result code
 * @protStatusCode: Protocol Status Code
 * @pe_session: PE Session
 *
 * This function is called to process reassoc failures
 * upon receiving REASSOC_CNF with a failure code or
 * MLM_REASSOC_CNF with a success code in case of STA role
 *
 * Return: None
 */
static void lim_handle_sme_reaasoc_result(struct mac_context *mac,
		tSirResultCodes resultCode, uint16_t protStatusCode,
		struct pe_session *pe_session)
{
	tpDphHashNode sta = NULL;
	uint8_t smesessionId;

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return;
	}
	smesessionId = pe_session->smeSessionId;
	if (resultCode != eSIR_SME_SUCCESS) {
		sta =
			dph_get_hash_entry(mac, DPH_STA_HASH_INDEX_PEER,
					   &pe_session->dph.dphHashTable);
		if (sta) {
			sta->mlmStaContext.disassocReason =
				REASON_UNSPEC_FAILURE;
			sta->mlmStaContext.cleanupTrigger =
				eLIM_JOIN_FAILURE;
			sta->mlmStaContext.resultCode = resultCode;
			sta->mlmStaContext.protStatusCode = protStatusCode;
			lim_cleanup_rx_path(mac, sta, pe_session, true);
			/* Cleanup if add bss failed */
			if (pe_session->add_bss_failed) {
				dph_delete_hash_entry(mac,
					 sta->staAddr, sta->assocId,
					 &pe_session->dph.dphHashTable);
				goto error;
			}
			return;
		}
	}
error:
	/* Delete the session if REASSOC failure occurred. */
	if (resultCode != eSIR_SME_SUCCESS) {
		if (pe_session) {
			pe_delete_session(mac, pe_session);
			pe_session = NULL;
		}
	}
	lim_send_sme_join_reassoc_rsp(mac, eWNI_SME_REASSOC_RSP, resultCode,
		protStatusCode, pe_session, smesessionId);
}

/**
 * lim_process_mlm_reassoc_cnf() - process mlm reassoc cnf msg
 *
 * @mac_ctx:       Pointer to Global MAC structure
 * @msg_buf:       A pointer to the MLM message buffer
 *
 * This function is called to process MLM_REASSOC_CNF message from MLM State
 * machine.
 *
 * @Return: void
 */
void lim_process_mlm_reassoc_cnf(struct mac_context *mac_ctx, uint32_t *msg_buf)
{
	struct pe_session *session;
	tLimMlmReassocCnf *lim_mlm_reassoc_cnf;
	struct reassoc_params param;
	QDF_STATUS status;

	if (!msg_buf) {
		pe_err("Buffer is Pointing to NULL");
		return;
	}
	lim_mlm_reassoc_cnf = (tLimMlmReassocCnf *)msg_buf;
	session = pe_find_session_by_session_id(
					mac_ctx,
					lim_mlm_reassoc_cnf->sessionId);
	if (!session) {
		pe_err("session Does not exist for given session Id");
		return;
	}
	if (session->limSmeState != eLIM_SME_WT_REASSOC_STATE ||
	    LIM_IS_AP_ROLE(session)) {
		/*
		 * Should not have received Reassocication confirm
		 * from MLM in other states OR on AP.
		 */
		pe_err("Rcv unexpected MLM_REASSOC_CNF role: %d sme 0x%X",
		       GET_LIM_SYSTEM_ROLE(session), session->limSmeState);
		return;
	}

	/*
	 * Upon Reassoc success or failure, freeup the cached preauth request,
	 * to ensure that channel switch is now allowed following any change in
	 * HT params.
	 */
	if (session->ftPEContext.pFTPreAuthReq) {
		pe_debug("Freeing pFTPreAuthReq: %pK",
			 session->ftPEContext.pFTPreAuthReq);
		if (session->ftPEContext.pFTPreAuthReq->pbssDescription) {
			qdf_mem_free(
			  session->ftPEContext.pFTPreAuthReq->pbssDescription);
			session->ftPEContext.pFTPreAuthReq->pbssDescription =
									NULL;
		}
		qdf_mem_free(session->ftPEContext.pFTPreAuthReq);
		session->ftPEContext.pFTPreAuthReq = NULL;
		session->ftPEContext.ftPreAuthSession = false;
	}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (session->bRoamSynchInProgress) {
		pe_debug("LFR3:Re-set the LIM Ctxt Roam Synch In Progress");
		session->bRoamSynchInProgress = false;
	}
#endif

	pe_debug("Rcv MLM_REASSOC_CNF with result code: %d",
		 lim_mlm_reassoc_cnf->resultCode);
	if (lim_mlm_reassoc_cnf->resultCode == eSIR_SME_SUCCESS) {
		/* Successful Reassociation */
		pe_debug("*** Reassociated with new BSS ***");

		session->limSmeState = eLIM_SME_LINK_EST_STATE;
		MTRACE(mac_trace(
			mac_ctx, TRACE_CODE_SME_STATE,
			session->peSessionId, session->limSmeState));

		wlan_vdev_mlme_sm_deliver_evt(session->vdev,
					      WLAN_VDEV_SM_EV_START_SUCCESS,
					      0, NULL);

		/* Need to send Reassoc rsp with Reassoc success to Host. */
		lim_send_sme_join_reassoc_rsp(
					mac_ctx, eWNI_SME_REASSOC_RSP,
					lim_mlm_reassoc_cnf->resultCode,
					lim_mlm_reassoc_cnf->protStatusCode,
					session, session->smeSessionId);
	} else {
		param.result_code = lim_mlm_reassoc_cnf->resultCode;
		param.prot_status_code = lim_mlm_reassoc_cnf->protStatusCode;
		param.session = session;

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
	}

	if (session->pLimReAssocReq) {
		qdf_mem_free(session->pLimReAssocReq);
		session->pLimReAssocReq = NULL;
	}
}

QDF_STATUS lim_sta_reassoc_error_handler(struct reassoc_params *param)
{
	struct pe_session *session;
	struct mac_context *mac_ctx;

	if (!param) {
		pe_err("param is NULL");
		return QDF_STATUS_E_INVAL;
	}

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		pe_err("mac_ctx is NULL");
		return QDF_STATUS_E_INVAL;
	}

	session = param->session;
	if (param->result_code
			== eSIR_SME_REASSOC_REFUSED) {
		/*
		 * Reassociation failure With the New AP but we still have the
		 * link with the Older AP
		 */
		session->limSmeState = eLIM_SME_LINK_EST_STATE;
		MTRACE(mac_trace(
			mac_ctx, TRACE_CODE_SME_STATE,
			session->peSessionId, session->limSmeState));

		/* Need to send Reassoc rsp with Assoc failure to Host. */
		lim_send_sme_join_reassoc_rsp(
					mac_ctx, eWNI_SME_REASSOC_RSP,
					param->result_code,
					param->prot_status_code,
					session, session->smeSessionId);
	} else {
		/* Reassociation failure */
		session->limSmeState = eLIM_SME_JOIN_FAILURE_STATE;
		MTRACE(mac_trace(
			mac_ctx, TRACE_CODE_SME_STATE,
			session->peSessionId, session->limSmeState));
		/* Need to send Reassoc rsp with Assoc failure to Host. */
		lim_handle_sme_reaasoc_result(
					mac_ctx,
					param->result_code,
					param->prot_status_code,
					session);
	}
	return QDF_STATUS_SUCCESS;
}

void lim_process_sta_mlm_add_bss_rsp_ft(struct mac_context *mac,
					struct add_bss_rsp *add_bss_rsp,
					struct pe_session *pe_session)
{
	tLimMlmReassocCnf mlmReassocCnf; /* keep sme */
	tpDphHashNode sta = NULL;
	tpAddStaParams pAddStaParams = NULL;
	uint32_t listenInterval = MLME_CFG_LISTEN_INTERVAL;
	struct bss_description *bss_desc = NULL;

	/* Sanity Checks */

	if (!add_bss_rsp) {
		pe_err("add_bss_rsp is NULL");
		goto end;
	}
	if (eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE !=
	    pe_session->limMlmState) {
		goto end;
	}

	sta = dph_add_hash_entry(mac, pe_session->bssId,
				 DPH_STA_HASH_INDEX_PEER,
				 &pe_session->dph.dphHashTable);
	if (!sta) {
		/* Could not add hash table entry */
		pe_err("could not add hash entry at DPH for "QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(pe_session->bssId));
		goto end;
	}

	/* Prepare and send Reassociation request frame */
	/* start reassoc timer. */
	mac->lim.lim_timers.gLimReassocFailureTimer.sessionId =
			pe_session->peSessionId;
	/* / Start reassociation failure timer */
	MTRACE(mac_trace
		(mac, TRACE_CODE_TIMER_ACTIVATE,
		 pe_session->peSessionId, eLIM_REASSOC_FAIL_TIMER));

	if (tx_timer_activate(&mac->lim.lim_timers.gLimReassocFailureTimer) !=
	    TX_SUCCESS) {
		/* / Could not start reassoc failure timer. */
		/* Log error */
		pe_err("could not start Reassoc failure timer");
		/* Return Reassoc confirm with */
		/* Resources Unavailable */
		mlmReassocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
		mlmReassocCnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;
		goto end;
	}

	mac->lim.pe_session = pe_session;
	if (!mac->lim.pe_session->pLimMlmReassocRetryReq) {
		/* Take a copy of reassoc request for retrying */
		mac->lim.pe_session->pLimMlmReassocRetryReq =
			qdf_mem_malloc(sizeof(tLimMlmReassocReq));
		if (!mac->lim.pe_session->pLimMlmReassocRetryReq)
			goto end;

		qdf_mem_copy(mac->lim.pe_session->pLimMlmReassocRetryReq,
			     pe_session->pLimMlmReassocReq,
			     sizeof(tLimMlmReassocReq));
	}
	mac->lim.reAssocRetryAttempt = 0;
	lim_send_reassoc_req_with_ft_ies_mgmt_frame(
		mac, pe_session->pLimMlmReassocReq, pe_session);

	pe_session->limPrevMlmState = pe_session->limMlmState;
	pe_session->limMlmState = eLIM_MLM_WT_FT_REASSOC_RSP_STATE;
	MTRACE(mac_trace
		       (mac, TRACE_CODE_MLM_STATE, pe_session->peSessionId,
		       eLIM_MLM_WT_FT_REASSOC_RSP_STATE));
	pe_debug("Set the mlm state: %d session: %d",
		       pe_session->limMlmState, pe_session->peSessionId);

	/* Success, handle below */

	pAddStaParams = qdf_mem_malloc(sizeof(tAddStaParams));
	if (!pAddStaParams)
		goto end;

	/* / Add STA context at MAC HW (BMU, RHP & TFP) */
	qdf_mem_copy((uint8_t *)pAddStaParams->staMac,
		     (uint8_t *)pe_session->self_mac_addr,
		     sizeof(tSirMacAddr));

	qdf_mem_copy((uint8_t *)pAddStaParams->bssId,
		     pe_session->bssId, sizeof(tSirMacAddr));

	pAddStaParams->staType = STA_ENTRY_SELF;
	pAddStaParams->status = QDF_STATUS_SUCCESS;

	/* Update  PE session ID */
	pAddStaParams->sessionId = pe_session->peSessionId;
	pAddStaParams->smesessionId = pe_session->smeSessionId;

	pAddStaParams->updateSta = false;

	if (pe_session->lim_join_req)
		bss_desc = &pe_session->lim_join_req->bssDescription;

	lim_populate_peer_rate_set(mac, &pAddStaParams->supportedRates, NULL,
				   false, pe_session, NULL, NULL, NULL,
				   bss_desc);

	if (pe_session->htCapability) {
		pAddStaParams->htCapable = pe_session->htCapability;
		pAddStaParams->vhtCapable = pe_session->vhtCapability;
		pAddStaParams->ch_width = pe_session->ch_width;

		pAddStaParams->mimoPS =
			lim_get_ht_capability(mac, eHT_MIMO_POWER_SAVE,
					      pe_session);
		pAddStaParams->maxAmpduDensity =
			lim_get_ht_capability(mac, eHT_MPDU_DENSITY,
					pe_session);
		pAddStaParams->maxAmpduSize =
			lim_get_ht_capability(mac, eHT_MAX_RX_AMPDU_FACTOR,
					      pe_session);
		pAddStaParams->fShortGI20Mhz =
			lim_get_ht_capability(mac, eHT_SHORT_GI_20MHZ,
					pe_session);
		pAddStaParams->fShortGI40Mhz =
			lim_get_ht_capability(mac, eHT_SHORT_GI_40MHZ,
					pe_session);
	}

	listenInterval = mac->mlme_cfg->sap_cfg.listen_interval;
	pAddStaParams->listenInterval = (uint16_t) listenInterval;

	pAddStaParams->encryptType = pe_session->encryptType;
	pAddStaParams->maxTxPower = pe_session->maxTxPower;

	/* Lets save this for when we receive the Reassoc Rsp */
	pe_session->ftPEContext.pAddStaReq = pAddStaParams;

	return;

end:
	/* Free up buffer allocated for reassocReq */
	if (pe_session)
		if (pe_session->pLimMlmReassocReq) {
			qdf_mem_free(pe_session->pLimMlmReassocReq);
			pe_session->pLimMlmReassocReq = NULL;
		}

	mlmReassocCnf.resultCode = eSIR_SME_FT_REASSOC_FAILURE;
	mlmReassocCnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;
	/* Update PE session Id */
	if (pe_session)
		mlmReassocCnf.sessionId = pe_session->peSessionId;
	else
		mlmReassocCnf.sessionId = 0;

	lim_post_sme_message(mac, LIM_MLM_REASSOC_CNF,
			     (uint32_t *) &mlmReassocCnf);
}

void lim_process_mlm_ft_reassoc_req(struct mac_context *mac,
				    tLimMlmReassocReq *reassoc_req)
{
	struct pe_session *session;
	uint16_t caps;
	uint32_t val;
	QDF_STATUS status;
	uint32_t teleBcnEn = 0;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_mlme_obj *mlme_obj;

	if (!reassoc_req) {
		pe_err("reassoc_req is NULL");
		return;
	}

	session = pe_find_session_by_session_id(mac, reassoc_req->sessionId);
	if (!session) {
		pe_err("session Does not exist for given session Id");
		qdf_mem_free(reassoc_req);
		return;
	}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_REASSOCIATING,
			session, 0, 0);
#endif

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(session)) {
		pe_err("pe_session is not in STA mode");
		qdf_mem_free(reassoc_req);
		return;
	}

	if (!session->ftPEContext.pAddBssReq) {
		pe_err("pAddBssReq is NULL");
		return;
	}

	qdf_mem_copy(reassoc_req->peerMacAddr,
		     session->bssId, sizeof(tSirMacAddr));

	if (lim_get_capability_info(mac, &caps, session) !=
			QDF_STATUS_SUCCESS) {
		/**
		 * Could not get Capabilities value
		 * from CFG. Log error.
		 */
		pe_err("could not get Capabilities value");
		qdf_mem_free(reassoc_req);
		return;
	}

	lim_update_caps_info_for_bss(mac, &caps,
		session->pLimReAssocReq->bssDescription.capabilityInfo);
	pe_debug("Capabilities info FT Reassoc: 0x%X", caps);

	reassoc_req->capabilityInfo = caps;

	/* If telescopic beaconing is enabled, set listen interval
	   to CFG_TELE_BCN_MAX_LI
	 */
	teleBcnEn = mac->mlme_cfg->sap_cfg.tele_bcn_wakeup_en;
	if (teleBcnEn)
		val = mac->mlme_cfg->sap_cfg.tele_bcn_max_li;
	else
		val = mac->mlme_cfg->sap_cfg.listen_interval;

	status = wma_add_bss_peer_sta(session->vdev_id, session->bssId, false);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(reassoc_req);
		return;
	}

	reassoc_req->listenInterval = (uint16_t) val;

	vdev = session->vdev;
	if (!vdev) {
		pe_err("vdev is NULL");
		qdf_mem_free(reassoc_req);
		return;
	}
	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!mlme_obj) {
		pe_err("vdev component object is NULL");
		return;
	}

	status = lim_pre_vdev_start(mac, mlme_obj, session);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_mem_free(reassoc_req);
		return;
	}
	qdf_mem_copy(mlme_obj->mgmt.generic.bssid, session->bssId,
		     QDF_MAC_ADDR_SIZE);

	session->pLimMlmReassocReq = reassoc_req;
	/* we need to defer the message until we get response back from HAL */
	SET_LIM_PROCESS_DEFD_MESGS(mac, false);
	status = wma_add_bss_lfr2_vdev_start(session->vdev,
					     session->ftPEContext.pAddBssReq);
	if (QDF_IS_STATUS_ERROR(status)) {
		SET_LIM_PROCESS_DEFD_MESGS(mac, true);
		pe_err("wma_add_bss_lfr2_vdev_start, reason: %X", status);
		session->pLimMlmReassocReq = NULL;
		qdf_mem_free(reassoc_req);
	}
	qdf_mem_free(session->ftPEContext.pAddBssReq);

	session->ftPEContext.pAddBssReq = NULL;
	return;
}

