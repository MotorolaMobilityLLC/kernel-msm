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
 * DOC: lim_reassoc_utils.c
 *
 * Host based roaming re-association utilities
 */

#include "cds_api.h"
#include "ani_global.h"
#include "wni_api.h"
#include "sir_common.h"

#include "wni_cfg.h"

#include "sch_api.h"
#include "utils_api.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_admit_control.h"
#include "lim_send_messages.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "lim_process_fils.h"

#include "qdf_types.h"
#include "wma_types.h"
#include "lim_types.h"

/**
 * lim_update_re_assoc_globals() - Update reassoc global data
 * @mac: Global MAC context
 * @pAssocRsp: Reassociation response data
 * @pe_session: PE Session
 *
 * This function is called to Update the Globals (LIM) during ReAssoc.
 *
 * Return: None
 */

void lim_update_re_assoc_globals(struct mac_context *mac, tpSirAssocRsp pAssocRsp,
				 struct pe_session *pe_session)
{
	/* Update the current Bss Information */
	qdf_mem_copy(pe_session->bssId,
		     pe_session->limReAssocbssId, sizeof(tSirMacAddr));
	pe_session->curr_op_freq = pe_session->lim_reassoc_chan_freq;
	pe_session->htSecondaryChannelOffset =
		pe_session->reAssocHtSupportedChannelWidthSet;
	pe_session->htRecommendedTxWidthSet =
		pe_session->reAssocHtRecommendedTxWidthSet;
	pe_session->htSecondaryChannelOffset =
		pe_session->reAssocHtSecondaryChannelOffset;
	pe_session->limCurrentBssCaps = pe_session->limReassocBssCaps;
	pe_session->limCurrentBssQosCaps =
		pe_session->limReassocBssQosCaps;

	qdf_mem_copy((uint8_t *) &pe_session->ssId,
		     (uint8_t *) &pe_session->limReassocSSID,
		     pe_session->limReassocSSID.length + 1);

	/* Store assigned AID for TIM processing */
	pe_session->limAID = pAssocRsp->aid & 0x3FFF;
	/** Set the State Back to ReAssoc Rsp*/
	pe_session->limMlmState = eLIM_MLM_WT_REASSOC_RSP_STATE;
	MTRACE(mac_trace
		       (mac, TRACE_CODE_MLM_STATE, pe_session->peSessionId,
		       pe_session->limMlmState));

}

/**
 * @lim_handle_del_bss_in_re_assoc_context() - DEL BSS during reassociation
 * @mac: Global MAC Context
 * @sta: Station Hash entry
 * @pe_session: PE Session
 *
 * While Processing the ReAssociation Response Frame in STA,
 *     a.immediately after receiving the Reassoc Response the RxCleanUp is
 *        being issued and the end of DelBSS the new BSS is being added.
 *
 *     b. If an AP rejects the ReAssociation (Disassoc/Deauth) with some context
 *        change, We need to update CSR with ReAssocCNF Response with the
 *        ReAssoc Fail and the reason Code, that is also being handled in the
 *        DELBSS context only
 *
 * Return: None
 */
void lim_handle_del_bss_in_re_assoc_context(struct mac_context *mac,
		tpDphHashNode sta, struct pe_session *pe_session)
{
	tLimMlmReassocCnf mlmReassocCnf;
	struct bss_description *bss_desc;
	/*
	 * Skipped the DeleteDPH Hash Entry as we need it for the new BSS
	 * Set the MlmState to IDLE
	 */
	pe_session->limMlmState = eLIM_MLM_IDLE_STATE;
	/* Update PE session Id */
	mlmReassocCnf.sessionId = pe_session->peSessionId;
	switch (pe_session->limSmeState) {
	case eLIM_SME_WT_REASSOC_STATE:
	{
		tpSirAssocRsp assocRsp;
		tpDphHashNode sta;
		QDF_STATUS retStatus = QDF_STATUS_SUCCESS;
		tpSchBeaconStruct beacon_struct;

		beacon_struct = qdf_mem_malloc(sizeof(tSchBeaconStruct));
		if (!beacon_struct) {
			mlmReassocCnf.resultCode =
					eSIR_SME_RESOURCES_UNAVAILABLE;
			mlmReassocCnf.protStatusCode =
					STATUS_UNSPECIFIED_FAILURE;
			lim_delete_dph_hash_entry(mac, pe_session->bssId,
				DPH_STA_HASH_INDEX_PEER, pe_session);
			goto error;
		}
		/* Delete the older STA Table entry */
		lim_delete_dph_hash_entry(mac, pe_session->bssId,
				DPH_STA_HASH_INDEX_PEER, pe_session);
		/*
		 * Add an entry for AP to hash table
		 * maintained by DPH module
		 */
		sta = dph_add_hash_entry(mac,
				pe_session->limReAssocbssId,
				DPH_STA_HASH_INDEX_PEER,
				&pe_session->dph.dphHashTable);
		if (!sta) {
			/* Could not add hash table entry */
			pe_err("could not add hash entry at DPH for");
			lim_print_mac_addr(mac,
				pe_session->limReAssocbssId, LOGE);
			mlmReassocCnf.resultCode =
				eSIR_SME_RESOURCES_UNAVAILABLE;
			mlmReassocCnf.protStatusCode = eSIR_SME_SUCCESS;
			qdf_mem_free(beacon_struct);
			goto error;
		}
		/*
		 * While Processing the ReAssoc Response Frame the Rsp Frame
		 * is being stored to be used here for sending ADDBSS
		 */
		assocRsp =
			(tpSirAssocRsp) pe_session->limAssocResponseData;

		bss_desc = &pe_session->pLimReAssocReq->bssDescription;
		lim_extract_ap_capabilities(mac,
			(uint8_t *)bss_desc->ieFields,
			lim_get_ielen_from_bss_description(bss_desc),
			beacon_struct);

		lim_update_assoc_sta_datas(mac, sta, assocRsp,
			pe_session, beacon_struct);
		lim_update_re_assoc_globals(mac, assocRsp, pe_session);
		if (mac->lim.gLimProtectionControl !=
		    MLME_FORCE_POLICY_PROTECTION_DISABLE)
			lim_decide_sta_protection_on_assoc(mac,
				beacon_struct,
				pe_session);
		if (beacon_struct->erpPresent) {
			if (beacon_struct->erpIEInfo.barkerPreambleMode)
				pe_session->beaconParams.fShortPreamble = 0;
			else
				pe_session->beaconParams.fShortPreamble = 1;
		}
		/*
		 * updateBss flag is false, as in this case, PE is first
		 * deleting the existing BSS and then adding a new one
		 */
		if (QDF_STATUS_SUCCESS !=
		    lim_sta_send_add_bss(mac, assocRsp, beacon_struct,
				bss_desc,
				false, pe_session)) {
			pe_err("Posting ADDBSS in the ReAssocCtx Failed");
			retStatus = QDF_STATUS_E_FAILURE;
		}
		if (retStatus != QDF_STATUS_SUCCESS) {
			mlmReassocCnf.resultCode =
				eSIR_SME_RESOURCES_UNAVAILABLE;
			mlmReassocCnf.protStatusCode =
				STATUS_UNSPECIFIED_FAILURE;
			qdf_mem_free(assocRsp);
			mac->lim.gLimAssocResponseData = NULL;
			qdf_mem_free(beacon_struct);
			goto error;
		}
		qdf_mem_free(assocRsp->sha384_ft_subelem.gtk);
		qdf_mem_free(assocRsp->sha384_ft_subelem.igtk);
		qdf_mem_free(assocRsp);
		qdf_mem_free(beacon_struct);
		pe_session->limAssocResponseData = NULL;
	}
	break;
	default:
		pe_err("DelBss in wrong system Role and SME State");
		mlmReassocCnf.resultCode = eSIR_SME_REFUSED;
		mlmReassocCnf.protStatusCode =
			eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
		goto error;
	}
	return;
error:
	lim_post_sme_message(mac, LIM_MLM_REASSOC_CNF,
			     (uint32_t *) &mlmReassocCnf);
}

/**
 * @lim_handle_add_bss_in_re_assoc_context() - ADD BSS during reassociation
 * @mac: Global MAC Context
 * @sta: Station Hash entry
 * @pe_session: PE Session
 *
 * While Processing the ReAssociation Response Frame in STA,
 *     a. immediately after receiving the Reassoc Response the RxCleanUp is
 *         being issued and the end of DelBSS the new BSS is being added.
 *
 *     b. If an AP rejects the ReAssociation (Disassoc/Deauth) with some context
 *        change, We need to update CSR with ReAssocCNF Response with the
 *        ReAssoc Fail and the reason Code, that is also being handled in the
 *        DELBSS context only
 *
 * Return: None
 */
void lim_handle_add_bss_in_re_assoc_context(struct mac_context *mac,
		tpDphHashNode sta, struct pe_session *pe_session)
{
	tLimMlmReassocCnf mlmReassocCnf;
	/** Skipped the DeleteDPH Hash Entry as we need it for the new BSS*/
	/** Set the MlmState to IDLE*/
	pe_session->limMlmState = eLIM_MLM_IDLE_STATE;
	MTRACE(mac_trace
		       (mac, TRACE_CODE_MLM_STATE, pe_session->peSessionId,
		       pe_session->limMlmState));
	switch (pe_session->limSmeState) {
	case eLIM_SME_WT_REASSOC_STATE: {
		tpSirAssocRsp assocRsp;
		tpDphHashNode sta;
		QDF_STATUS retStatus = QDF_STATUS_SUCCESS;
		tSchBeaconStruct *pBeaconStruct;

		pBeaconStruct =
			qdf_mem_malloc(sizeof(tSchBeaconStruct));
		if (!pBeaconStruct) {
			mlmReassocCnf.resultCode =
				eSIR_SME_RESOURCES_UNAVAILABLE;
			mlmReassocCnf.protStatusCode =
				eSIR_SME_RESOURCES_UNAVAILABLE;
			goto Error;
		}
		/* Get the AP entry from DPH hash table */
		sta =
			dph_get_hash_entry(mac, DPH_STA_HASH_INDEX_PEER,
					   &pe_session->dph.dphHashTable);
		if (!sta) {
			pe_err("Fail to get STA PEER entry from hash");
			mlmReassocCnf.resultCode =
				eSIR_SME_RESOURCES_UNAVAILABLE;
			mlmReassocCnf.protStatusCode = eSIR_SME_SUCCESS;
			qdf_mem_free(pBeaconStruct);
			goto Error;
		}
		/*
		 * While Processing the ReAssoc Response Frame the Rsp Frame
		 * is being stored to be used here for sending ADDBSS
		 */
		assocRsp =
			(tpSirAssocRsp) pe_session->limAssocResponseData;
		lim_extract_ap_capabilities(mac,
				(uint8_t *)pe_session->pLimReAssocReq->bssDescription.ieFields,
				lim_get_ielen_from_bss_description
				(&pe_session->pLimReAssocReq->bssDescription),
				pBeaconStruct);
		lim_update_assoc_sta_datas(mac, sta, assocRsp,
					   pe_session, pBeaconStruct);
		lim_update_re_assoc_globals(mac, assocRsp, pe_session);
		if (mac->lim.gLimProtectionControl !=
		    MLME_FORCE_POLICY_PROTECTION_DISABLE)
			lim_decide_sta_protection_on_assoc(mac,
							   pBeaconStruct,
							   pe_session);

		if (pBeaconStruct->erpPresent) {
			if (pBeaconStruct->erpIEInfo.barkerPreambleMode)
				pe_session->beaconParams.
				fShortPreamble = 0;
			else
				pe_session->beaconParams.
				fShortPreamble = 1;
		}

		pe_session->isNonRoamReassoc = 1;
		if (QDF_STATUS_SUCCESS !=
		    lim_sta_send_add_bss(mac, assocRsp, pBeaconStruct,
					 &pe_session->pLimReAssocReq->
					 bssDescription, true,
					 pe_session)) {
			pe_err("Post ADDBSS in the ReAssocCtxt Failed");
			retStatus = QDF_STATUS_E_FAILURE;
		}
		if (retStatus != QDF_STATUS_SUCCESS) {
			mlmReassocCnf.resultCode =
				eSIR_SME_RESOURCES_UNAVAILABLE;
			mlmReassocCnf.protStatusCode =
				STATUS_UNSPECIFIED_FAILURE;
			qdf_mem_free(assocRsp);
			mac->lim.gLimAssocResponseData = NULL;
			qdf_mem_free(pBeaconStruct);
			goto Error;
		}
		qdf_mem_free(assocRsp->sha384_ft_subelem.gtk);
		qdf_mem_free(assocRsp->sha384_ft_subelem.igtk);
		qdf_mem_free(assocRsp);
		pe_session->limAssocResponseData = NULL;
		qdf_mem_free(pBeaconStruct);
	}
	break;
	default:
		pe_err("DelBss in the wrong system Role and SME State");
		mlmReassocCnf.resultCode = eSIR_SME_REFUSED;
		mlmReassocCnf.protStatusCode =
			eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
		goto Error;
	}
	return;
Error:
	lim_post_sme_message(mac, LIM_MLM_REASSOC_CNF,
			     (uint32_t *) &mlmReassocCnf);
}

/**
 * lim_is_reassoc_in_progress() - Check if reassoiciation is in progress
 * @mac: Global MAC Context
 * @pe_session: PE Session
 *
 * Return: true  When STA is waiting for Reassoc response from AP
 *         else false
 */
bool lim_is_reassoc_in_progress(struct mac_context *mac, struct pe_session *pe_session)
{
	if (!pe_session)
		return false;

	if (LIM_IS_STA_ROLE(pe_session) &&
	    (pe_session->limSmeState == eLIM_SME_WT_REASSOC_STATE))
		return true;

	return false;
}

/**
 * lim_add_ft_sta_self()- function to add STA once we have connected with a
 *          new AP
 * @mac_ctx: pointer to global mac structure
 * @assoc_id: association id for the station connection
 * @session_entry: pe session entr
 *
 * This function is called to add a STA once we have connected with a new
 * AP, that we have performed an FT to.
 *
 * The Add STA Response is created and now after the ADD Bss Is Successful
 * we add the self sta. We update with the association id from the reassoc
 * response from the AP.
 *
 * Return: QDF_STATUS_SUCCESS on success else QDF_STATUS failure codes
 */
QDF_STATUS lim_add_ft_sta_self(struct mac_context *mac_ctx, uint16_t assoc_id,
				struct pe_session *session_entry)
{
	tpAddStaParams add_sta_params = NULL;
	QDF_STATUS ret_code = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg_q = {0};
	tpDphHashNode sta_ds;

	sta_ds = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				    &session_entry->dph.dphHashTable);

	if (!sta_ds) {
		pe_err("Could not get hash entry at DPH");
		return QDF_STATUS_E_FAILURE;
	}

	add_sta_params = session_entry->ftPEContext.pAddStaReq;
	add_sta_params->assocId = assoc_id;
	add_sta_params->smesessionId = session_entry->smeSessionId;

	qdf_mem_copy(add_sta_params->supportedRates.supportedMCSSet,
		     sta_ds->supportedRates.supportedMCSSet,
		     SIR_MAC_MAX_SUPPORTED_MCS_SET);

	if (lim_is_fils_connection(session_entry))
		add_sta_params->no_ptk_4_way = true;

	msg_q.type = WMA_ADD_STA_REQ;
	msg_q.reserved = 0;
	msg_q.bodyptr = add_sta_params;
	msg_q.bodyval = 0;

	QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			"Sending WMA_ADD_STA_REQ (aid %d)",
			 add_sta_params->assocId);
	MTRACE(mac_trace_msg_tx(mac_ctx, session_entry->peSessionId,
			 msg_q.type));

	session_entry->limPrevMlmState = session_entry->limMlmState;
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_MLM_STATE,
		session_entry->peSessionId, eLIM_MLM_WT_ADD_STA_RSP_STATE));
	session_entry->limMlmState = eLIM_MLM_WT_ADD_STA_RSP_STATE;
	ret_code = wma_post_ctrl_msg(mac_ctx, &msg_q);
	if (QDF_STATUS_SUCCESS != ret_code) {
		pe_err("Posting WMA_ADD_STA_REQ to HAL failed, reason=%X",
			ret_code);
		qdf_mem_free(add_sta_params);
	}

	session_entry->ftPEContext.pAddStaReq = NULL;
	return ret_code;
}

/**
 * lim_restore_pre_reassoc_state() - Restore the pre-association context
 * @mac: Global MAC Context
 * @resultCode: Assoc response result
 * @protStatusCode: Internal protocol status code
 * @pe_session: PE Session
 *
 * This function is called on STA role whenever Reasociation
 * Response with a reject code is received from AP.
 * Reassociation failure timer is stopped, Old (or current) AP's
 * context is restored both at Polaris & software
 *
 * Return: None
 */

void
lim_restore_pre_reassoc_state(struct mac_context *mac,
		tSirResultCodes resultCode, uint16_t protStatusCode,
		struct pe_session *pe_session)
{
	tLimMlmReassocCnf mlmReassocCnf;

	pe_debug("sessionid: %d protStatusCode: %d resultCode: %d",
		pe_session->smeSessionId, protStatusCode, resultCode);

	pe_session->limMlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
	MTRACE(mac_trace
		       (mac, TRACE_CODE_MLM_STATE, pe_session->peSessionId,
		       eLIM_MLM_LINK_ESTABLISHED_STATE));

	/* 'Change' timer for future activations */
	lim_deactivate_and_change_timer(mac, eLIM_REASSOC_FAIL_TIMER);

	lim_send_switch_chnl_params(mac, pe_session);

	/* @ToDo:Need to Integrate the STOP the Dataxfer to AP from 11H code */

	mlmReassocCnf.resultCode = resultCode;
	mlmReassocCnf.protStatusCode = protStatusCode;
	mlmReassocCnf.sessionId = pe_session->peSessionId;
	lim_post_sme_message(mac,
			     LIM_MLM_REASSOC_CNF, (uint32_t *) &mlmReassocCnf);
}

/**
 * lim_post_reassoc_failure() - Post failure message to SME
 * @mac: Global MAC Context
 * @resultCode: Result Code
 * @protStatusCode: Protocol Status Code
 * @pe_session: PE Session
 *
 * Return: None
 */
void lim_post_reassoc_failure(struct mac_context *mac,
		tSirResultCodes resultCode, uint16_t protStatusCode,
		struct pe_session *pe_session)
{
	tLimMlmReassocCnf mlmReassocCnf;

	pe_session->limMlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
	MTRACE(mac_trace
		       (mac, TRACE_CODE_MLM_STATE, pe_session->peSessionId,
		       eLIM_MLM_LINK_ESTABLISHED_STATE));

	lim_deactivate_and_change_timer(mac, eLIM_REASSOC_FAIL_TIMER);

	mlmReassocCnf.resultCode = resultCode;
	mlmReassocCnf.protStatusCode = protStatusCode;
	mlmReassocCnf.sessionId = pe_session->peSessionId;
	lim_post_sme_message(mac,
			     LIM_MLM_REASSOC_CNF, (uint32_t *) &mlmReassocCnf);
}

