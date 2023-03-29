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
 * This file lim_send_sme_rspMessages.cc contains the functions
 * for sending SME response/notification messages to applications
 * above MAC software.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "qdf_types.h"
#include "wni_api.h"
#include "sir_common.h"
#include "ani_global.h"

#include "wni_cfg.h"
#include "sys_def.h"

#include "sch_api.h"
#include "utils_api.h"
#include "lim_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_send_sme_rsp_messages.h"
#include "lim_session_utils.h"
#include "lim_types.h"
#include "sir_api.h"
#include "cds_regdomain.h"
#include "lim_send_messages.h"
#include "nan_datapath.h"
#include "lim_assoc_utils.h"
#include "wlan_reg_services_api.h"
#include "wlan_utility.h"

#include "wlan_tdls_tgt_api.h"
#include "lim_process_fils.h"
#include "wma.h"

void lim_send_sme_rsp(struct mac_context *mac_ctx, uint16_t msg_type,
		      tSirResultCodes result_code, uint8_t vdev_id)
{
	struct scheduler_msg msg = {0};
	tSirSmeRsp *sme_rsp;

	pe_debug("Sending message: %s with reasonCode: %s",
		lim_msg_str(msg_type), lim_result_code_str(result_code));

	sme_rsp = qdf_mem_malloc(sizeof(tSirSmeRsp));
	if (!sme_rsp)
		return;

	sme_rsp->messageType = msg_type;
	sme_rsp->length = sizeof(tSirSmeRsp);
	sme_rsp->status_code = result_code;
	sme_rsp->vdev_id = vdev_id;

	msg.type = msg_type;
	msg.bodyptr = sme_rsp;
	msg.bodyval = 0;
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_TX_SME_MSG, vdev_id, msg.type));

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	switch (msg_type) {
	case eWNI_SME_STOP_BSS_RSP:
		lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_STOP_BSS_RSP_EVENT,
				NULL, (uint16_t) result_code, 0);
		break;
	}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_sys_process_mmh_msg_api(mac_ctx, &msg);
}

/**
 * lim_get_max_rate_flags() - Get rate flags
 * @mac_ctx: Pointer to global MAC structure
 * @sta_ds: Pointer to station ds structure
 *
 * This function is called to get the rate flags for a connection
 * from the station ds structure depending on the ht and the vht
 * channel width supported.
 *
 * Return: Returns the populated rate_flags
 */
uint32_t lim_get_max_rate_flags(struct mac_context *mac_ctx, tpDphHashNode sta_ds)
{
	uint32_t rate_flags = 0;

	if (!sta_ds) {
		pe_err("sta_ds is NULL");
		return rate_flags;
	}

	if (!sta_ds->mlmStaContext.htCapability &&
	    !sta_ds->mlmStaContext.vhtCapability) {
		rate_flags |= TX_RATE_LEGACY;
	} else {
		if (sta_ds->mlmStaContext.vhtCapability) {
			if (WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ ==
				sta_ds->vhtSupportedChannelWidthSet) {
				rate_flags |= TX_RATE_VHT80;
			} else if (WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ ==
					sta_ds->vhtSupportedChannelWidthSet) {
				if (sta_ds->htSupportedChannelWidthSet)
					rate_flags |= TX_RATE_VHT40;
				else
					rate_flags |= TX_RATE_VHT20;
			}
		} else if (sta_ds->mlmStaContext.htCapability) {
			if (sta_ds->htSupportedChannelWidthSet)
				rate_flags |= TX_RATE_HT40;
			else
				rate_flags |= TX_RATE_HT20;
		}
	}

	if (sta_ds->htShortGI20Mhz || sta_ds->htShortGI40Mhz)
		rate_flags |= TX_RATE_SGI;

	return rate_flags;
}

/**
 * lim_send_sme_join_reassoc_rsp_after_resume() - Send Response to SME
 * @mac_ctx:      Pointer to Global MAC structure
 * @status:       Resume link status
 * @sme_join_rsp: Join response to be sent
 *
 * This function is called to send Join/Reassoc rsp
 * message to SME after the resume link.
 *
 * Return: None
 */
static void
lim_send_sme_join_reassoc_rsp_after_resume(struct mac_context *mac_ctx,
					   QDF_STATUS status,
					   struct join_rsp *sme_join_rsp)
{
	struct scheduler_msg msg = {0};

	msg.type = sme_join_rsp->messageType;
	msg.bodyptr = sme_join_rsp;
	msg.bodyval = 0;
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_TX_SME_MSG, NO_SESSION, msg.type));
	lim_sys_process_mmh_msg_api(mac_ctx, &msg);
}

/**
 * lim_handle_join_rsp_status() - Handle the response.
 * @mac_ctx:            Pointer to Global MAC structure
 * @session_entry:      PE Session Info
 * @result_code:        Indicates the result of previously issued
 *                      eWNI_SME_msgType_REQ message
 * @sme_join_rsp        The received response.
 *
 * This function will handle both the success and failure status
 * of the received response.
 *
 * Return: None
 */
static void lim_handle_join_rsp_status(struct mac_context *mac_ctx,
				       struct pe_session *session_entry,
				       tSirResultCodes result_code,
				       struct join_rsp *sme_join_rsp)
{
	uint16_t bss_ie_len;
	void *bss_ies;
	bool is_vendor_ap_1_present;
	struct join_req *join_reassoc_req = NULL;

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	struct ht_profile *ht_profile;
#endif
	if (session_entry->beacon) {
		sme_join_rsp->beaconLength = session_entry->bcnLen;
		qdf_mem_copy(sme_join_rsp->frames,
			     session_entry->beacon,
			     sme_join_rsp->beaconLength);
		qdf_mem_free(session_entry->beacon);
		session_entry->beacon = NULL;
		session_entry->bcnLen = 0;
	}

	if (session_entry->assoc_req) {
		sme_join_rsp->assocReqLength =
			session_entry->assocReqLen;
		qdf_mem_copy(sme_join_rsp->frames +
			     sme_join_rsp->beaconLength,
			     session_entry->assoc_req,
			     sme_join_rsp->assocReqLength);
		qdf_mem_free(session_entry->assoc_req);
		session_entry->assoc_req = NULL;
		session_entry->assocReqLen = 0;
	}
	if (session_entry->assocRsp) {
		sme_join_rsp->assocRspLength =
			session_entry->assocRspLen;
		qdf_mem_copy(sme_join_rsp->frames +
			     sme_join_rsp->beaconLength +
			     sme_join_rsp->assocReqLength,
			     session_entry->assocRsp,
			     sme_join_rsp->assocRspLength);
		qdf_mem_free(session_entry->assocRsp);
		session_entry->assocRsp = NULL;
		session_entry->assocRspLen = 0;
	}

	pe_debug("Beacon len %d Assoc Req len %d Assoc Rsp len %d",
		 sme_join_rsp->beaconLength, sme_join_rsp->assocReqLength,
		 sme_join_rsp->assocRspLength);

	if (result_code == eSIR_SME_SUCCESS) {
		if (session_entry->ricData) {
			sme_join_rsp->parsedRicRspLen =
				session_entry->RICDataLen;
			qdf_mem_copy(sme_join_rsp->frames +
				sme_join_rsp->beaconLength +
				sme_join_rsp->assocReqLength +
				sme_join_rsp->assocRspLength,
				session_entry->ricData,
				sme_join_rsp->parsedRicRspLen);
			qdf_mem_free(session_entry->ricData);
			session_entry->ricData = NULL;
			session_entry->RICDataLen = 0;
			pe_debug("RicLength: %d",
				sme_join_rsp->parsedRicRspLen);
		}
#ifdef FEATURE_WLAN_ESE
		if (session_entry->tspecIes) {
			sme_join_rsp->tspecIeLen =
				session_entry->tspecLen;
			qdf_mem_copy(sme_join_rsp->frames +
				sme_join_rsp->beaconLength +
				sme_join_rsp->assocReqLength +
				sme_join_rsp->assocRspLength +
				sme_join_rsp->parsedRicRspLen,
				session_entry->tspecIes,
				sme_join_rsp->tspecIeLen);
			qdf_mem_free(session_entry->tspecIes);
			session_entry->tspecIes = NULL;
			session_entry->tspecLen = 0;
			pe_debug("ESE-TspecLen: %d",
				sme_join_rsp->tspecIeLen);
		}
#endif
		sme_join_rsp->aid = session_entry->limAID;
		sme_join_rsp->vht_channel_width =
			session_entry->ch_width;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		if (session_entry->cc_switch_mode !=
				QDF_MCC_TO_SCC_SWITCH_DISABLE) {
			ht_profile = &sme_join_rsp->ht_profile;
			ht_profile->htSupportedChannelWidthSet =
				session_entry->htSupportedChannelWidthSet;
			ht_profile->htRecommendedTxWidthSet =
				session_entry->htRecommendedTxWidthSet;
			ht_profile->htSecondaryChannelOffset =
				session_entry->htSecondaryChannelOffset;
			ht_profile->dot11mode = session_entry->dot11mode;
			ht_profile->htCapability = session_entry->htCapability;
			ht_profile->vhtCapability =
				session_entry->vhtCapability;
			ht_profile->apCenterChan = session_entry->ch_center_freq_seg0;
			ht_profile->apChanWidth = session_entry->ch_width;
		}
#endif
		pe_debug("lim_join_req:%pK, pLimReAssocReq:%pK",
			 session_entry->lim_join_req,
			 session_entry->pLimReAssocReq);

		if (session_entry->lim_join_req)
			join_reassoc_req = session_entry->lim_join_req;

		if (session_entry->pLimReAssocReq)
			join_reassoc_req = session_entry->pLimReAssocReq;

		if (!join_reassoc_req) {
			pe_err("both  lim_join_req and pLimReAssocReq NULL");
			return;
		}

		bss_ie_len = lim_get_ielen_from_bss_description(
				&join_reassoc_req->bssDescription);
		bss_ies = &join_reassoc_req->bssDescription.ieFields;
		is_vendor_ap_1_present = (wlan_get_vendor_ie_ptr_from_oui(
			SIR_MAC_VENDOR_AP_1_OUI, SIR_MAC_VENDOR_AP_1_OUI_LEN,
			bss_ies, bss_ie_len) != NULL);

		if (mac_ctx->roam.configParam.is_force_1x1 &&
		    is_vendor_ap_1_present && (session_entry->nss == 2) &&
		    (!mac_ctx->mlme_cfg->gen.as_enabled ||
		     wlan_reg_is_5ghz_ch_freq(session_entry->curr_op_freq))) {
			/* SET vdev param */
			pe_debug("sending SMPS intolrent vdev_param");
			wma_cli_set_command(session_entry->smeSessionId,
					   (int)WMI_VDEV_PARAM_SMPS_INTOLERANT,
					    1, VDEV_CMD);

		}
	} else {
		if (session_entry->beacon) {
			qdf_mem_free(session_entry->beacon);
			session_entry->beacon = NULL;
			session_entry->bcnLen = 0;
		}
		if (session_entry->assoc_req) {
			qdf_mem_free(session_entry->assoc_req);
			session_entry->assoc_req = NULL;
			session_entry->assocReqLen = 0;
		}
		if (session_entry->assocRsp) {
			qdf_mem_free(session_entry->assocRsp);
			session_entry->assocRsp = NULL;
			session_entry->assocRspLen = 0;
		}
		if (session_entry->ricData) {
			qdf_mem_free(session_entry->ricData);
			session_entry->ricData = NULL;
			session_entry->RICDataLen = 0;
		}
#ifdef FEATURE_WLAN_ESE
		if (session_entry->tspecIes) {
			qdf_mem_free(session_entry->tspecIes);
			session_entry->tspecIes = NULL;
			session_entry->tspecLen = 0;
		}
#endif
	}
}

#ifdef WLAN_FEATURE_11AX
static void lim_add_he_info(struct parsed_ies *parsed_ies,
			    struct join_rsp *sme_join_rsp)
{
	if (parsed_ies->he_operation.present)
		sme_join_rsp->he_operation = parsed_ies->he_operation;
}
#else
static inline void lim_add_he_info(struct parsed_ies *parsed_ies,
				   struct join_rsp *sme_join_rsp)
{
}
#endif

/**
 * lim_add_bss_info() - copy data from session entry to join rsp
 * @sta_ds: Station dph entry
 * @sme_join_rsp: Join response buffer to be filled up
 *
 * Return: None
 */
static void lim_add_bss_info(tpDphHashNode sta_ds,
			     struct join_rsp *sme_join_rsp)
{
	struct parsed_ies *parsed_ies = &sta_ds->parsed_ies;

	if (parsed_ies->hs20vendor_ie.present)
		sme_join_rsp->hs20vendor_ie = parsed_ies->hs20vendor_ie;
	if (parsed_ies->vht_caps.present)
		sme_join_rsp->vht_caps = parsed_ies->vht_caps;
	if (parsed_ies->ht_caps.present)
		sme_join_rsp->ht_caps = parsed_ies->ht_caps;
	if (parsed_ies->ht_operation.present)
		sme_join_rsp->ht_operation = parsed_ies->ht_operation;
	if (parsed_ies->vht_operation.present)
		sme_join_rsp->vht_operation = parsed_ies->vht_operation;
	lim_add_he_info(parsed_ies, sme_join_rsp);
}

#ifdef WLAN_FEATURE_FILS_SK
static void lim_update_fils_seq_num(struct join_rsp *sme_join_rsp,
				    struct pe_session *session_entry)
{
	sme_join_rsp->fils_seq_num =
		session_entry->fils_info->sequence_number;
}
#else
static inline void lim_update_fils_seq_num(struct join_rsp *sme_join_rsp,
					   struct pe_session *session_entry)
{}
#endif

void lim_send_sme_join_reassoc_rsp(struct mac_context *mac_ctx,
				   uint16_t msg_type,
				   tSirResultCodes result_code,
				   uint16_t prot_status_code,
				   struct pe_session *session_entry,
				   uint8_t vdev_id)
{
	struct join_rsp *sme_join_rsp;
	uint32_t rsp_len;
	tpDphHashNode sta_ds = NULL;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	if (msg_type == eWNI_SME_REASSOC_RSP)
		lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_REASSOC_RSP_EVENT,
			session_entry, (uint16_t) result_code, 0);
	else
		lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_JOIN_RSP_EVENT,
			session_entry, (uint16_t) result_code, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	pe_debug("Sending message: %s with reasonCode: %s",
		lim_msg_str(msg_type), lim_result_code_str(result_code));

	if (!session_entry) {
		rsp_len = sizeof(*sme_join_rsp);
		sme_join_rsp = qdf_mem_malloc(rsp_len);
		if (!sme_join_rsp)
			return;

		sme_join_rsp->beaconLength = 0;
		sme_join_rsp->assocReqLength = 0;
		sme_join_rsp->assocRspLength = 0;
	} else {
		rsp_len = session_entry->assocReqLen +
			session_entry->assocRspLen + session_entry->bcnLen +
			session_entry->RICDataLen +
#ifdef FEATURE_WLAN_ESE
			session_entry->tspecLen +
#endif
			sizeof(*sme_join_rsp) - sizeof(uint8_t);
		sme_join_rsp = qdf_mem_malloc(rsp_len);
		if (!sme_join_rsp)
			return;

		if (lim_is_fils_connection(session_entry)) {
			sme_join_rsp->is_fils_connection = true;
			lim_update_fils_seq_num(sme_join_rsp,
						session_entry);
		}

		if (result_code == eSIR_SME_SUCCESS) {
			sta_ds = dph_get_hash_entry(mac_ctx,
				DPH_STA_HASH_INDEX_PEER,
				&session_entry->dph.dphHashTable);
			if (!sta_ds) {
				pe_err("Get Self Sta Entry fail");
			} else {
				sme_join_rsp->timingMeasCap =
					sta_ds->timingMeasCap;
#ifdef FEATURE_WLAN_TDLS
				sme_join_rsp->tdls_prohibited =
					session_entry->tdls_prohibited;
				sme_join_rsp->tdls_chan_swit_prohibited =
				   session_entry->tdls_chan_swit_prohibited;
#endif
				sme_join_rsp->nss = sta_ds->nss;
				sme_join_rsp->max_rate_flags =
					lim_get_max_rate_flags(mac_ctx, sta_ds);
				lim_add_bss_info(sta_ds, sme_join_rsp);

				/* Copy FILS params only for Successful join */
				populate_fils_connect_params(mac_ctx,
						session_entry, sme_join_rsp);
			}
		}

		sme_join_rsp->beaconLength = 0;
		sme_join_rsp->assocReqLength = 0;
		sme_join_rsp->assocRspLength = 0;
		sme_join_rsp->parsedRicRspLen = 0;
#ifdef FEATURE_WLAN_ESE
		sme_join_rsp->tspecIeLen = 0;
#endif
		lim_handle_join_rsp_status(mac_ctx, session_entry, result_code,
			sme_join_rsp);
		sme_join_rsp->uapsd_mask = session_entry->gUapsdPerAcBitmask;
		/* Send supported NSS 1x1 to SME */
		sme_join_rsp->supported_nss_1x1 =
			session_entry->supported_nss_1x1;
		pe_debug("SME Join Rsp is supported NSS 1X1: %d",
		       sme_join_rsp->supported_nss_1x1);
	}

	sme_join_rsp->messageType = msg_type;
	sme_join_rsp->length = (uint16_t) rsp_len;
	sme_join_rsp->status_code = result_code;
	sme_join_rsp->protStatusCode = prot_status_code;

	sme_join_rsp->vdev_id = vdev_id;

	lim_send_sme_join_reassoc_rsp_after_resume(mac_ctx, QDF_STATUS_SUCCESS,
						   sme_join_rsp);
}

void lim_send_sme_start_bss_rsp(struct mac_context *mac,
				uint16_t msgType,
				tSirResultCodes resultCode,
				struct pe_session *pe_session,
				uint8_t smesessionId)
{

	uint16_t size = 0;
	struct scheduler_msg mmhMsg = {0};
	struct start_bss_rsp *pSirSmeRsp;
	uint16_t beacon_length, ieLen;
	uint16_t ieOffset, curLen;

	pe_debug("Sending message: %s with reasonCode: %s",
		       lim_msg_str(msgType), lim_result_code_str(resultCode));

	size = sizeof(struct start_bss_rsp);

	if (!pe_session) {
		pSirSmeRsp = qdf_mem_malloc(size);
		if (!pSirSmeRsp)
			return;
	} else {
		/* subtract size of beaconLength + Mac Hdr + Fixed Fields before SSID */
		ieOffset = sizeof(tAniBeaconStruct) + SIR_MAC_B_PR_SSID_OFFSET;
		beacon_length = pe_session->schBeaconOffsetBegin +
						pe_session->schBeaconOffsetEnd;
		ieLen = beacon_length - ieOffset;

		/* Invalidate for non-beaconing entities */
		if (beacon_length <= ieOffset)
			ieLen = ieOffset = 0;
		/* calculate the memory size to allocate */
		size += ieLen;

		pSirSmeRsp = qdf_mem_malloc(size);
		if (!pSirSmeRsp)
			return;
		size = sizeof(struct start_bss_rsp);
		if (resultCode == eSIR_SME_SUCCESS) {

			sir_copy_mac_addr(pSirSmeRsp->bssDescription.bssId,
					  pe_session->bssId);

			/* Read beacon interval from session */
			pSirSmeRsp->bssDescription.beaconInterval =
				(uint16_t) pe_session->beaconParams.
				beaconInterval;
			pSirSmeRsp->bssType = pe_session->bssType;

			if (lim_get_capability_info
				    (mac, &pSirSmeRsp->bssDescription.capabilityInfo,
				    pe_session)
			    != QDF_STATUS_SUCCESS)
				pe_err("could not retrieve Capabilities value");

			lim_get_phy_mode(mac,
					 (uint32_t *) &pSirSmeRsp->bssDescription.
					 nwType, pe_session);

			pSirSmeRsp->bssDescription.chan_freq =
				pe_session->curr_op_freq;

		if (!LIM_IS_NDI_ROLE(pe_session)) {
			curLen = pe_session->schBeaconOffsetBegin - ieOffset;
			qdf_mem_copy((uint8_t *) &pSirSmeRsp->bssDescription.
				     ieFields,
				     pe_session->pSchBeaconFrameBegin +
				     ieOffset, (uint32_t) curLen);

			qdf_mem_copy(((uint8_t *) &pSirSmeRsp->bssDescription.
				      ieFields) + curLen,
				     pe_session->pSchBeaconFrameEnd,
				     (uint32_t) pe_session->
				     schBeaconOffsetEnd);

			pSirSmeRsp->bssDescription.length = (uint16_t)
				(offsetof(struct bss_description, ieFields[0])
				- sizeof(pSirSmeRsp->bssDescription.length)
				+ ieLen);
			/* This is the size of the message, subtracting the size of the pointer to ieFields */
			size += ieLen - sizeof(uint32_t);
		}
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
			if (pe_session->cc_switch_mode
			    != QDF_MCC_TO_SCC_SWITCH_DISABLE) {
				pSirSmeRsp->ht_profile.
				htSupportedChannelWidthSet =
					pe_session->htSupportedChannelWidthSet;
				pSirSmeRsp->ht_profile.htRecommendedTxWidthSet =
					pe_session->htRecommendedTxWidthSet;
				pSirSmeRsp->ht_profile.htSecondaryChannelOffset =
					pe_session->htSecondaryChannelOffset;
				pSirSmeRsp->ht_profile.dot11mode =
					pe_session->dot11mode;
				pSirSmeRsp->ht_profile.htCapability =
					pe_session->htCapability;
				pSirSmeRsp->ht_profile.vhtCapability =
					pe_session->vhtCapability;
				pSirSmeRsp->ht_profile.apCenterChan =
					pe_session->ch_center_freq_seg0;
				pSirSmeRsp->ht_profile.apChanWidth =
					pe_session->ch_width;
			}
#endif
		}
	}
	pSirSmeRsp->messageType = msgType;
	pSirSmeRsp->length = size;
	pSirSmeRsp->sessionId = smesessionId;
	pSirSmeRsp->status_code = resultCode;

	mmhMsg.type = msgType;
	mmhMsg.bodyptr = pSirSmeRsp;
	mmhMsg.bodyval = 0;
	if (!pe_session) {
		MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
				 NO_SESSION, mmhMsg.type));
	} else {
		MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
				 pe_session->peSessionId, mmhMsg.type));
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_START_BSS_RSP_EVENT,
			      pe_session, (uint16_t) resultCode, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	lim_sys_process_mmh_msg_api(mac, &mmhMsg);
} /*** end lim_send_sme_start_bss_rsp() ***/

void lim_send_sme_disassoc_deauth_ntf(struct mac_context *mac,
				      QDF_STATUS status, uint32_t *pCtx)
{
	struct scheduler_msg mmhMsg = {0};
	struct scheduler_msg *pMsg = (struct scheduler_msg *) pCtx;

	mmhMsg.type = pMsg->type;
	mmhMsg.bodyptr = pMsg;
	mmhMsg.bodyval = 0;

	MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG, NO_SESSION, mmhMsg.type));

	lim_sys_process_mmh_msg_api(mac, &mmhMsg);
}

void lim_send_sme_disassoc_ntf(struct mac_context *mac,
			       tSirMacAddr peerMacAddr,
			       tSirResultCodes reasonCode,
			       uint16_t disassocTrigger,
			       uint16_t aid,
			       uint8_t smesessionId,
			       struct pe_session *pe_session)
{
	struct disassoc_rsp *pSirSmeDisassocRsp;
	struct disassoc_ind *pSirSmeDisassocInd;
	uint32_t *pMsg = NULL;
	bool failure = false;
	struct pe_session *session = NULL;
	uint16_t i, assoc_id;
	tpDphHashNode sta_ds = NULL;
	QDF_STATUS status;

	pe_debug("Disassoc Ntf with trigger : %d reasonCode: %d",
		disassocTrigger, reasonCode);

	switch (disassocTrigger) {
	case eLIM_DUPLICATE_ENTRY:
		/*
		 * Duplicate entry is removed at LIM.
		 * Initiate new entry for other session
		 */
		pe_debug("Rcvd eLIM_DUPLICATE_ENTRY for " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peerMacAddr));

		for (i = 0; i < mac->lim.maxBssId; i++) {
			session = &mac->lim.gpSession[i];
			if (session->valid &&
			    (session->opmode == QDF_SAP_MODE)) {
				/* Find the sta ds entry in another session */
				sta_ds = dph_lookup_hash_entry(mac,
						peerMacAddr, &assoc_id,
						&session->dph.dphHashTable);
				if (sta_ds)
					break;
			}
		}
		if (sta_ds) {
			if (lim_add_sta(mac, sta_ds, false, session) !=
					QDF_STATUS_SUCCESS)
					pe_err("could not Add STA with assocId: %d",
					sta_ds->assocId);
		}
		status = lim_prepare_disconnect_done_ind(mac, &pMsg,
							 smesessionId,
							 reasonCode,
							 &peerMacAddr[0]);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			pe_err("Failed to prepare message");
			return;
		}
		break;

	case eLIM_HOST_DISASSOC:
		/**
		 * Disassociation response due to
		 * host triggered disassociation
		 */

		pSirSmeDisassocRsp = qdf_mem_malloc(sizeof(struct disassoc_rsp));
		if (!pSirSmeDisassocRsp) {
			failure = true;
			goto error;
		}
		pe_debug("send eWNI_SME_DISASSOC_RSP with retCode: %d for "
			 QDF_MAC_ADDR_FMT,
			 reasonCode, QDF_MAC_ADDR_REF(peerMacAddr));
		pSirSmeDisassocRsp->messageType = eWNI_SME_DISASSOC_RSP;
		pSirSmeDisassocRsp->length = sizeof(struct disassoc_rsp);
		pSirSmeDisassocRsp->sessionId = smesessionId;
		pSirSmeDisassocRsp->status_code = reasonCode;
		qdf_mem_copy(pSirSmeDisassocRsp->peer_macaddr.bytes,
			     peerMacAddr, sizeof(tSirMacAddr));

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */

		lim_diag_event_report(mac, WLAN_PE_DIAG_DISASSOC_RSP_EVENT,
				      pe_session, (uint16_t) reasonCode, 0);
#endif
		pMsg = (uint32_t *) pSirSmeDisassocRsp;
		break;

	case eLIM_PEER_ENTITY_DISASSOC:
	case eLIM_LINK_MONITORING_DISASSOC:
		status = lim_prepare_disconnect_done_ind(mac, &pMsg,
						smesessionId,
						reasonCode, &peerMacAddr[0]);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			pe_err("Failed to prepare message");
			return;
		}
		break;

	default:
		/**
		 * Disassociation indication due to Disassociation
		 * frame reception from peer entity or due to
		 * loss of link with peer entity.
		 */
		pSirSmeDisassocInd =
				qdf_mem_malloc(sizeof(*pSirSmeDisassocInd));
		if (!pSirSmeDisassocInd) {
			failure = true;
			goto error;
		}
		pe_debug("send eWNI_SME_DISASSOC_IND with retCode: %d for "
			 QDF_MAC_ADDR_FMT,
			 reasonCode, QDF_MAC_ADDR_REF(peerMacAddr));
		pSirSmeDisassocInd->messageType = eWNI_SME_DISASSOC_IND;
		pSirSmeDisassocInd->length = sizeof(*pSirSmeDisassocInd);
		pSirSmeDisassocInd->vdev_id = smesessionId;
		pSirSmeDisassocInd->reasonCode = reasonCode;
		pSirSmeDisassocInd->status_code = reasonCode;
		qdf_mem_copy(pSirSmeDisassocInd->bssid.bytes,
			     pe_session->bssId, sizeof(tSirMacAddr));
		qdf_mem_copy(pSirSmeDisassocInd->peer_macaddr.bytes,
			     peerMacAddr, sizeof(tSirMacAddr));

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
		lim_diag_event_report(mac, WLAN_PE_DIAG_DISASSOC_IND_EVENT,
				      pe_session, (uint16_t) reasonCode, 0);
#endif
		pMsg = (uint32_t *) pSirSmeDisassocInd;

		break;
	}

error:
	/* Delete the PE session Created */
	if ((pe_session) && LIM_IS_STA_ROLE(pe_session))
		pe_delete_session(mac, pe_session);

	if (false == failure)
		lim_send_sme_disassoc_deauth_ntf(mac, QDF_STATUS_SUCCESS,
						 (uint32_t *) pMsg);
} /*** end lim_send_sme_disassoc_ntf() ***/

static bool lim_is_disconnect_from_ap(enum eLimDisassocTrigger trigger)
{
	if (trigger == eLIM_PEER_ENTITY_DEAUTH ||
	    trigger == eLIM_PEER_ENTITY_DISASSOC)
		return true;

	return false;
}
/** -----------------------------------------------------------------
   \brief lim_send_sme_disassoc_ind() - sends SME_DISASSOC_IND

   After receiving disassociation frame from peer entity, this
   function sends a eWNI_SME_DISASSOC_IND to SME with a specific
   reason code.

   \param mac - global mac structure
   \param sta - station dph hash node
   \return none
   \sa
   ----------------------------------------------------------------- */
void
lim_send_sme_disassoc_ind(struct mac_context *mac, tpDphHashNode sta,
			  struct pe_session *pe_session)
{
	struct scheduler_msg mmhMsg = {0};
	struct disassoc_ind *pSirSmeDisassocInd;

	pSirSmeDisassocInd = qdf_mem_malloc(sizeof(*pSirSmeDisassocInd));
	if (!pSirSmeDisassocInd)
		return;

	pSirSmeDisassocInd->messageType = eWNI_SME_DISASSOC_IND;
	pSirSmeDisassocInd->length = sizeof(*pSirSmeDisassocInd);

	pSirSmeDisassocInd->vdev_id = pe_session->smeSessionId;
	pSirSmeDisassocInd->status_code = eSIR_SME_DEAUTH_STATUS;
	pSirSmeDisassocInd->reasonCode = sta->mlmStaContext.disassocReason;

	qdf_mem_copy(pSirSmeDisassocInd->bssid.bytes, pe_session->bssId,
		     QDF_MAC_ADDR_SIZE);

	qdf_mem_copy(pSirSmeDisassocInd->peer_macaddr.bytes, sta->staAddr,
		     QDF_MAC_ADDR_SIZE);

	if (LIM_IS_STA_ROLE(pe_session))
		pSirSmeDisassocInd->from_ap =
		lim_is_disconnect_from_ap(sta->mlmStaContext.cleanupTrigger);

	mmhMsg.type = eWNI_SME_DISASSOC_IND;
	mmhMsg.bodyptr = pSirSmeDisassocInd;
	mmhMsg.bodyval = 0;

	MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
			 pe_session->peSessionId, mmhMsg.type));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_DISASSOC_IND_EVENT, pe_session,
			      0, (uint16_t) sta->mlmStaContext.disassocReason);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	lim_sys_process_mmh_msg_api(mac, &mmhMsg);

} /*** end lim_send_sme_disassoc_ind() ***/

/** -----------------------------------------------------------------
   \brief lim_send_sme_deauth_ind() - sends SME_DEAUTH_IND

   After receiving deauthentication frame from peer entity, this
   function sends a eWNI_SME_DEAUTH_IND to SME with a specific
   reason code.

   \param mac - global mac structure
   \param sta - station dph hash node
   \return none
   \sa
   ----------------------------------------------------------------- */
void
lim_send_sme_deauth_ind(struct mac_context *mac, tpDphHashNode sta,
			struct pe_session *pe_session)
{
	struct scheduler_msg mmhMsg = {0};
	struct deauth_ind *pSirSmeDeauthInd;

	pSirSmeDeauthInd = qdf_mem_malloc(sizeof(*pSirSmeDeauthInd));
	if (!pSirSmeDeauthInd)
		return;

	pSirSmeDeauthInd->messageType = eWNI_SME_DEAUTH_IND;
	pSirSmeDeauthInd->length = sizeof(*pSirSmeDeauthInd);

	pSirSmeDeauthInd->vdev_id = pe_session->smeSessionId;
	if (eSIR_INFRA_AP_MODE == pe_session->bssType) {
		pSirSmeDeauthInd->status_code =
			(tSirResultCodes) sta->mlmStaContext.cleanupTrigger;
	} else {
		/* Need to indicate the reason code over the air */
		pSirSmeDeauthInd->status_code =
			(tSirResultCodes) sta->mlmStaContext.disassocReason;
	}
	/* BSSID */
	qdf_mem_copy(pSirSmeDeauthInd->bssid.bytes, pe_session->bssId,
		     QDF_MAC_ADDR_SIZE);
	/* peerMacAddr */
	qdf_mem_copy(pSirSmeDeauthInd->peer_macaddr.bytes, sta->staAddr,
		     QDF_MAC_ADDR_SIZE);
	pSirSmeDeauthInd->reasonCode = sta->mlmStaContext.disassocReason;

	if (sta->mlmStaContext.disassocReason == REASON_STA_LEAVING)
		pSirSmeDeauthInd->rssi = sta->del_sta_ctx_rssi;

	if (LIM_IS_STA_ROLE(pe_session))
		pSirSmeDeauthInd->from_ap =
		lim_is_disconnect_from_ap(sta->mlmStaContext.cleanupTrigger);

	mmhMsg.type = eWNI_SME_DEAUTH_IND;
	mmhMsg.bodyptr = pSirSmeDeauthInd;
	mmhMsg.bodyval = 0;

	MTRACE(mac_trace_msg_tx(mac, pe_session->peSessionId, mmhMsg.type));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_DEAUTH_IND_EVENT, pe_session,
			      0, sta->mlmStaContext.cleanupTrigger);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	lim_sys_process_mmh_msg_api(mac, &mmhMsg);
	return;
} /*** end lim_send_sme_deauth_ind() ***/

#ifdef FEATURE_WLAN_TDLS
/**
 * lim_send_sme_tdls_del_sta_ind()
 *
 ***FUNCTION:
 * This function is called to send the TDLS STA context deletion to SME.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 * NA
 *
 * @param  mac   - Pointer to global MAC structure
 * @param  sta - Pointer to internal STA Datastructure
 * @param  pe_session - Pointer to the session entry
 * @param  reasonCode - Reason for TDLS sta deletion
 * @return None
 */
void
lim_send_sme_tdls_del_sta_ind(struct mac_context *mac, tpDphHashNode sta,
			      struct pe_session *pe_session, uint16_t reasonCode)
{
	struct tdls_event_info info;

	pe_debug("Delete TDLS Peer "QDF_MAC_ADDR_FMT "with reason code: %d",
			QDF_MAC_ADDR_REF(sta->staAddr), reasonCode);
	info.vdev_id = pe_session->smeSessionId;
	qdf_mem_copy(info.peermac.bytes, sta->staAddr, QDF_MAC_ADDR_SIZE);
	info.message_type = TDLS_PEER_DISCONNECTED;
	info.peer_reason = TDLS_DISCONNECTED_PEER_DELETE;

	tgt_tdls_event_handler(mac->psoc, &info);

	return;
} /*** end lim_send_sme_tdls_del_sta_ind() ***/

/**
 * lim_send_sme_mgmt_tx_completion()
 *
 ***FUNCTION:
 * This function is called to send the eWNI_SME_MGMT_FRM_TX_COMPLETION_IND
 * message to SME.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 *
 ***NOTE:
 * NA
 *
 * @param  mac   - Pointer to global MAC structure
 * @param  pe_session - Pointer to the session entry
 * @param  txCompleteStatus - TX Complete Status of Mgmt Frames
 * @return None
 */
void
lim_send_sme_mgmt_tx_completion(struct mac_context *mac,
				uint32_t vdev_id,
				uint32_t txCompleteStatus)
{
	struct scheduler_msg msg = {0};
	struct tdls_mgmt_tx_completion_ind *mgmt_tx_completion_ind;
	QDF_STATUS status;

	mgmt_tx_completion_ind =
		qdf_mem_malloc(sizeof(*mgmt_tx_completion_ind));
	if (!mgmt_tx_completion_ind)
		return;

	/* sessionId */
	mgmt_tx_completion_ind->vdev_id = vdev_id;

	mgmt_tx_completion_ind->tx_complete_status = txCompleteStatus;

	msg.type = eWNI_SME_MGMT_FRM_TX_COMPLETION_IND;
	msg.bodyptr = mgmt_tx_completion_ind;
	msg.bodyval = 0;

	mgmt_tx_completion_ind->psoc = mac->psoc;
	msg.callback = tgt_tdls_send_mgmt_tx_completion;
	status = scheduler_post_message(QDF_MODULE_ID_PE,
			       QDF_MODULE_ID_TDLS,
			       QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("post msg fail, %d", status);
		qdf_mem_free(mgmt_tx_completion_ind);
	}
} /*** end lim_send_sme_tdls_delete_all_peer_ind() ***/

#endif /* FEATURE_WLAN_TDLS */

QDF_STATUS lim_prepare_disconnect_done_ind(struct mac_context *mac_ctx,
					   uint32_t **msg,
					   uint8_t session_id,
					   tSirResultCodes reason_code,
					   uint8_t *peer_mac_addr)
{
	struct sir_sme_discon_done_ind *sir_sme_dis_ind;

	sir_sme_dis_ind = qdf_mem_malloc(sizeof(*sir_sme_dis_ind));
	if (!sir_sme_dis_ind)
		return QDF_STATUS_E_FAILURE;

	pe_debug("Prepare eWNI_SME_DISCONNECT_DONE_IND withretCode: %d",
		 reason_code);

	sir_sme_dis_ind->message_type = eWNI_SME_DISCONNECT_DONE_IND;
	sir_sme_dis_ind->length = sizeof(*sir_sme_dis_ind);
	sir_sme_dis_ind->session_id = session_id;
	if (peer_mac_addr)
		qdf_mem_copy(sir_sme_dis_ind->peer_mac,
			     peer_mac_addr, ETH_ALEN);

	/*
	 * Instead of sending deauth reason code as 505 which is
	 * internal value(eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE)
	 * Send reason code as zero to Supplicant
	 */
	if (reason_code == eSIR_SME_LOST_LINK_WITH_PEER_RESULT_CODE)
		sir_sme_dis_ind->reason_code = 0;
	else
		sir_sme_dis_ind->reason_code = reason_code;

	*msg = (uint32_t *)sir_sme_dis_ind;

	return QDF_STATUS_SUCCESS;
}

void lim_send_sme_deauth_ntf(struct mac_context *mac, tSirMacAddr peerMacAddr,
			     tSirResultCodes reasonCode, uint16_t deauthTrigger,
			     uint16_t aid, uint8_t smesessionId)
{
	uint8_t *pBuf;
	struct deauth_rsp *pSirSmeDeauthRsp;
	struct deauth_ind *pSirSmeDeauthInd;
	struct pe_session *pe_session;
	uint8_t sessionId;
	uint32_t *pMsg = NULL;
	QDF_STATUS status;

	pe_session = pe_find_session_by_bssid(mac, peerMacAddr, &sessionId);
	switch (deauthTrigger) {
	case eLIM_HOST_DEAUTH:
		/**
		 * Deauthentication response to host triggered
		 * deauthentication.
		 */
		pSirSmeDeauthRsp = qdf_mem_malloc(sizeof(*pSirSmeDeauthRsp));
		if (!pSirSmeDeauthRsp)
			return;
		pe_debug("send eWNI_SME_DEAUTH_RSP with retCode: %d for "
			 QDF_MAC_ADDR_FMT,
			 reasonCode, QDF_MAC_ADDR_REF(peerMacAddr));
		pSirSmeDeauthRsp->messageType = eWNI_SME_DEAUTH_RSP;
		pSirSmeDeauthRsp->length = sizeof(*pSirSmeDeauthRsp);
		pSirSmeDeauthRsp->status_code = reasonCode;
		pSirSmeDeauthRsp->sessionId = smesessionId;

		pBuf = (uint8_t *) pSirSmeDeauthRsp->peer_macaddr.bytes;
		qdf_mem_copy(pBuf, peerMacAddr, sizeof(tSirMacAddr));

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
		lim_diag_event_report(mac, WLAN_PE_DIAG_DEAUTH_RSP_EVENT,
				      pe_session, 0, (uint16_t) reasonCode);
#endif
		pMsg = (uint32_t *) pSirSmeDeauthRsp;

		break;

	case eLIM_PEER_ENTITY_DEAUTH:
	case eLIM_LINK_MONITORING_DEAUTH:
		status = lim_prepare_disconnect_done_ind(mac, &pMsg,
						smesessionId, reasonCode,
						&peerMacAddr[0]);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			pe_err("Failed to prepare message");
			return;
		}
		break;
	default:
		/**
		 * Deauthentication indication due to Deauthentication
		 * frame reception from peer entity or due to
		 * loss of link with peer entity.
		 */
		pSirSmeDeauthInd = qdf_mem_malloc(sizeof(*pSirSmeDeauthInd));
		if (!pSirSmeDeauthInd)
			return;
		pe_debug("send eWNI_SME_DEAUTH_IND with retCode: %d for "
			 QDF_MAC_ADDR_FMT,
			 reasonCode, QDF_MAC_ADDR_REF(peerMacAddr));
		pSirSmeDeauthInd->messageType = eWNI_SME_DEAUTH_IND;
		pSirSmeDeauthInd->length = sizeof(*pSirSmeDeauthInd);
		pSirSmeDeauthInd->reasonCode = REASON_UNSPEC_FAILURE;
		pSirSmeDeauthInd->vdev_id = smesessionId;
		pSirSmeDeauthInd->status_code = reasonCode;
		qdf_mem_copy(pSirSmeDeauthInd->bssid.bytes, pe_session->bssId,
			     sizeof(tSirMacAddr));
		qdf_mem_copy(pSirSmeDeauthInd->peer_macaddr.bytes, peerMacAddr,
			     QDF_MAC_ADDR_SIZE);

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
		lim_diag_event_report(mac, WLAN_PE_DIAG_DEAUTH_IND_EVENT,
				      pe_session, 0, (uint16_t) reasonCode);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
		pMsg = (uint32_t *) pSirSmeDeauthInd;

		break;
	}

	/*Delete the PE session  created */
	if (pe_session && LIM_IS_STA_ROLE(pe_session))
		pe_delete_session(mac, pe_session);

	lim_send_sme_disassoc_deauth_ntf(mac, QDF_STATUS_SUCCESS,
					 (uint32_t *) pMsg);

} /*** end lim_send_sme_deauth_ntf() ***/

/**
 * lim_send_sme_wm_status_change_ntf() - Send Notification
 * @mac_ctx:             Global MAC Context
 * @status_change_code:  Indicates the change in the wireless medium.
 * @status_change_info:  Indicates the information associated with
 *                       change in the wireless medium.
 * @info_len:            Indicates the length of status change information
 *                       being sent.
 * @session_id           SessionID
 *
 * This function is called by limProcessSmeMessages() to send
 * eWNI_SME_WM_STATUS_CHANGE_NTF message to host.
 *
 * Return: None
 */
void
lim_send_sme_wm_status_change_ntf(struct mac_context *mac_ctx,
	tSirSmeStatusChangeCode status_change_code,
	uint32_t *status_change_info, uint16_t info_len, uint8_t session_id)
{
	struct scheduler_msg msg = {0};
	struct wm_status_change_ntf *wm_status_change_ntf;
	uint32_t max_info_len;

	wm_status_change_ntf = qdf_mem_malloc(sizeof(*wm_status_change_ntf));
	if (!wm_status_change_ntf)
		return;

	msg.type = eWNI_SME_WM_STATUS_CHANGE_NTF;
	msg.bodyval = 0;
	msg.bodyptr = wm_status_change_ntf;

	switch (status_change_code) {
	case eSIR_SME_AP_CAPS_CHANGED:
		max_info_len = sizeof(struct ap_new_caps);
		break;
	default:
		max_info_len = sizeof(wm_status_change_ntf->statusChangeInfo);
		break;
	}

	switch (status_change_code) {
	case eSIR_SME_RADAR_DETECTED:
		break;
	default:
		wm_status_change_ntf->messageType =
			eWNI_SME_WM_STATUS_CHANGE_NTF;
		wm_status_change_ntf->statusChangeCode = status_change_code;
		wm_status_change_ntf->length = sizeof(*wm_status_change_ntf);
		wm_status_change_ntf->sessionId = session_id;
		if (info_len <= max_info_len && status_change_info) {
			qdf_mem_copy(
			    (uint8_t *) &wm_status_change_ntf->statusChangeInfo,
			    (uint8_t *) status_change_info, info_len);
		}
		pe_debug("StatusChg code: 0x%x length: %d",
			status_change_code, info_len);
		break;
	}

	MTRACE(mac_trace(mac_ctx, TRACE_CODE_TX_SME_MSG, session_id, msg.type));
	lim_sys_process_mmh_msg_api(mac_ctx, &msg);

} /*** end lim_send_sme_wm_status_change_ntf() ***/

void lim_send_sme_set_context_rsp(struct mac_context *mac,
				  struct qdf_mac_addr peer_macaddr,
				  uint16_t aid,
				  tSirResultCodes resultCode,
				  struct pe_session *pe_session,
				  uint8_t smesessionId)
{
	struct scheduler_msg mmhMsg = {0};
	struct set_context_rsp *set_context_rsp;

	set_context_rsp = qdf_mem_malloc(sizeof(*set_context_rsp));
	if (!set_context_rsp)
		return;

	set_context_rsp->messageType = eWNI_SME_SETCONTEXT_RSP;
	set_context_rsp->length = sizeof(*set_context_rsp);
	set_context_rsp->status_code = resultCode;

	qdf_copy_macaddr(&set_context_rsp->peer_macaddr, &peer_macaddr);

	set_context_rsp->sessionId = smesessionId;

	mmhMsg.type = eWNI_SME_SETCONTEXT_RSP;
	mmhMsg.bodyptr = set_context_rsp;
	mmhMsg.bodyval = 0;
	if (!pe_session) {
		MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
				 NO_SESSION, mmhMsg.type));
	} else {
		MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
				 pe_session->peSessionId, mmhMsg.type));
	}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_SETCONTEXT_RSP_EVENT,
			      pe_session, (uint16_t) resultCode, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	mac->lim.sme_msg_callback(mac, &mmhMsg);
} /*** end lim_send_sme_set_context_rsp() ***/

void lim_send_sme_addts_rsp(struct mac_context *mac,
			    uint8_t rspReqd, uint32_t status,
			    struct pe_session *pe_session,
			    struct mac_tspec_ie tspec,
			    uint8_t smesessionId)
{
	tpSirAddtsRsp rsp;
	struct scheduler_msg mmhMsg = {0};

	if (!rspReqd)
		return;

	rsp = qdf_mem_malloc(sizeof(tSirAddtsRsp));
	if (!rsp)
		return;

	rsp->messageType = eWNI_SME_ADDTS_RSP;
	rsp->rc = status;
	rsp->rsp.status = (enum wlan_status_code)status;
	rsp->rsp.tspec = tspec;
	rsp->sessionId = smesessionId;

	mmhMsg.type = eWNI_SME_ADDTS_RSP;
	mmhMsg.bodyptr = rsp;
	mmhMsg.bodyval = 0;
	if (!pe_session) {
		MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
				 NO_SESSION, mmhMsg.type));
	} else {
		MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
				 pe_session->peSessionId, mmhMsg.type));
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_ADDTS_RSP_EVENT, pe_session, 0,
			      0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	lim_sys_process_mmh_msg_api(mac, &mmhMsg);
	return;
}

void lim_send_sme_delts_rsp(struct mac_context *mac, tpSirDeltsReq delts,
			    uint32_t status, struct pe_session *pe_session,
			    uint8_t smesessionId)
{
	tpSirDeltsRsp rsp;
	struct scheduler_msg mmhMsg = {0};

	pe_debug("SendSmeDeltsRsp aid: %d tsid: %d up: %d status: %d",
		delts->aid,
		delts->req.tsinfo.traffic.tsid,
		delts->req.tsinfo.traffic.userPrio, status);
	if (!delts->rspReqd)
		return;

	rsp = qdf_mem_malloc(sizeof(tSirDeltsRsp));
	if (!rsp)
		return;

	if (pe_session) {

		rsp->aid = delts->aid;
		qdf_copy_macaddr(&rsp->macaddr, &delts->macaddr);
		qdf_mem_copy((uint8_t *) &rsp->rsp, (uint8_t *) &delts->req,
			     sizeof(struct delts_req_info));
	}

	rsp->messageType = eWNI_SME_DELTS_RSP;
	rsp->rc = status;
	rsp->sessionId = smesessionId;

	mmhMsg.type = eWNI_SME_DELTS_RSP;
	mmhMsg.bodyptr = rsp;
	mmhMsg.bodyval = 0;
	if (!pe_session) {
		MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
				 NO_SESSION, mmhMsg.type));
	} else {
		MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
				 pe_session->peSessionId, mmhMsg.type));
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_DELTS_RSP_EVENT, pe_session,
			      (uint16_t) status, 0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	lim_sys_process_mmh_msg_api(mac, &mmhMsg);
}

void
lim_send_sme_delts_ind(struct mac_context *mac, struct delts_req_info *delts,
		       uint16_t aid, struct pe_session *pe_session)
{
	tpSirDeltsRsp rsp;
	struct scheduler_msg mmhMsg = {0};

	pe_debug("SendSmeDeltsInd aid: %d tsid: %d up: %d",
		aid, delts->tsinfo.traffic.tsid, delts->tsinfo.traffic.userPrio);

	rsp = qdf_mem_malloc(sizeof(tSirDeltsRsp));
	if (!rsp)
		return;

	rsp->messageType = eWNI_SME_DELTS_IND;
	rsp->rc = QDF_STATUS_SUCCESS;
	rsp->aid = aid;
	qdf_mem_copy((uint8_t *) &rsp->rsp, (uint8_t *) delts, sizeof(*delts));
	rsp->sessionId = pe_session->smeSessionId;

	mmhMsg.type = eWNI_SME_DELTS_IND;
	mmhMsg.bodyptr = rsp;
	mmhMsg.bodyval = 0;
	MTRACE(mac_trace_msg_tx(mac, pe_session->peSessionId, mmhMsg.type));
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM    /* FEATURE_WLAN_DIAG_SUPPORT */
	lim_diag_event_report(mac, WLAN_PE_DIAG_DELTS_IND_EVENT, pe_session, 0,
			      0);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	lim_sys_process_mmh_msg_api(mac, &mmhMsg);
}

#ifdef FEATURE_WLAN_ESE
/**
 * lim_send_sme_pe_ese_tsm_rsp() - send tsm response
 * @mac:   Pointer to global mac structure
 * @pStats: Pointer to TSM Stats
 *
 * This function is called to send tsm stats response to HDD.
 * This function posts the result back to HDD. This is a response to
 * HDD's request to get tsm stats.
 *
 * Return: None
 */
void lim_send_sme_pe_ese_tsm_rsp(struct mac_context *mac,
				 tAniGetTsmStatsRsp *pStats)
{
	struct scheduler_msg mmhMsg = {0};
	uint8_t sessionId;
	tAniGetTsmStatsRsp *pPeStats = (tAniGetTsmStatsRsp *) pStats;
	struct pe_session *pPeSessionEntry = NULL;

	/* Get the Session Id based on Sta Id */
	pPeSessionEntry = pe_find_session_by_bssid(mac, pPeStats->bssid.bytes,
						   &sessionId);
	/* Fill the Session Id */
	if (pPeSessionEntry) {
		/* Fill the Session Id */
		pPeStats->sessionId = pPeSessionEntry->smeSessionId;
	} else {
		pe_err("Session not found for the Sta peer:" QDF_MAC_ADDR_FMT,
		       QDF_MAC_ADDR_REF(pPeStats->bssid.bytes));
		qdf_mem_free(pPeStats->tsmStatsReq);
		qdf_mem_free(pPeStats);
		return;
	}

	pPeStats->msgType = eWNI_SME_GET_TSM_STATS_RSP;
	pPeStats->tsmMetrics.RoamingCount
		= pPeSessionEntry->eseContext.tsm.tsmMetrics.RoamingCount;
	pPeStats->tsmMetrics.RoamingDly
		= pPeSessionEntry->eseContext.tsm.tsmMetrics.RoamingDly;

	mmhMsg.type = eWNI_SME_GET_TSM_STATS_RSP;
	mmhMsg.bodyptr = pStats;
	mmhMsg.bodyval = 0;
	MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG, sessionId, mmhMsg.type));
	lim_sys_process_mmh_msg_api(mac, &mmhMsg);

	return;
} /*** end lim_send_sme_pe_ese_tsm_rsp() ***/

#endif /* FEATURE_WLAN_ESE */

/**
 * lim_process_csa_wbw_ie() - Process CSA Wide BW IE
 * @mac_ctx:         pointer to global adapter context
 * @csa_params:      pointer to CSA parameters
 * @chnl_switch_info:pointer to channel switch parameters
 * @session_entry:   session pointer
 *
 * Return: None
 */
static QDF_STATUS lim_process_csa_wbw_ie(struct mac_context *mac_ctx,
		struct csa_offload_params *csa_params,
		tLimWiderBWChannelSwitchInfo *chnl_switch_info,
		struct pe_session *session_entry)
{
	struct ch_params ch_params = {0};
	uint8_t ap_new_ch_width;
	uint8_t center_freq_diff;
	uint32_t fw_vht_ch_wd = wma_get_vht_ch_width() + 1;
	uint32_t cent_freq1, cent_freq2;
	uint32_t csa_cent_freq, csa_cent_freq1 = 0, csa_cent_freq2 = 0;

	ap_new_ch_width = csa_params->new_ch_width + 1;

	if (!csa_params->new_ch_freq_seg1 && !csa_params->new_ch_freq_seg2) {
		pe_err("CSA wide BW IE has invalid center freq");
		return QDF_STATUS_E_INVAL;
	}

	csa_cent_freq = csa_params->csa_chan_freq;
	if (wlan_reg_is_6ghz_op_class(mac_ctx->pdev,
				      csa_params->new_op_class)) {
		cent_freq1 = wlan_reg_chan_opclass_to_freq(
					csa_params->new_ch_freq_seg1,
					csa_params->new_op_class, false);
		cent_freq2 = wlan_reg_chan_opclass_to_freq(
					csa_params->new_ch_freq_seg2,
					csa_params->new_op_class, false);
	} else {
		cent_freq1 = wlan_reg_legacy_chan_to_freq(mac_ctx->pdev,
					csa_params->new_ch_freq_seg1);
		cent_freq2 = wlan_reg_legacy_chan_to_freq(mac_ctx->pdev,
					csa_params->new_ch_freq_seg2);
	}

	switch (ap_new_ch_width) {
	case CH_WIDTH_80MHZ:
		csa_cent_freq1 = cent_freq1;
		if (csa_params->new_ch_freq_seg2) {
			center_freq_diff = abs(csa_params->new_ch_freq_seg2 -
					       csa_params->new_ch_freq_seg1);
			if (center_freq_diff == CENTER_FREQ_DIFF_160MHz) {
				ap_new_ch_width = CH_WIDTH_160MHZ;
				csa_cent_freq1 = cent_freq2;
				csa_params->new_ch_freq_seg1 =
						csa_params->new_ch_freq_seg2;
				csa_params->new_ch_freq_seg2 = 0;
			} else if (center_freq_diff > CENTER_FREQ_DIFF_160MHz) {
				ap_new_ch_width = CH_WIDTH_80P80MHZ;
				csa_cent_freq2 = cent_freq2;
			}
		}
		break;
	case CH_WIDTH_80P80MHZ:
		csa_cent_freq1 = cent_freq1;
		csa_cent_freq2 = cent_freq2;
		break;
	case CH_WIDTH_160MHZ:
		csa_cent_freq1 = cent_freq1;
		break;
	default:
		pe_err("CSA wide BW IE has wrong ch_width %d", ap_new_ch_width);
		return QDF_STATUS_E_INVAL;
	}

	/* Verify whether the bandwidth and channel segments are valid. */
	switch (ap_new_ch_width) {
	case CH_WIDTH_80MHZ:
		if (abs(csa_cent_freq1 - csa_cent_freq) != 10 &&
		    abs(csa_cent_freq1 - csa_cent_freq) != 30) {
			pe_err("CSA WBW 80MHz has invalid seg0 freq %d",
			       csa_cent_freq1);
			return QDF_STATUS_E_INVAL;
		}
		if (csa_cent_freq2) {
			pe_err("CSA WBW 80MHz has invalid seg1 freq %d",
			       csa_cent_freq2);
			return QDF_STATUS_E_INVAL;
		}
		break;
	case CH_WIDTH_80P80MHZ:
		if (abs(csa_cent_freq1 - csa_cent_freq) != 10 &&
		    abs(csa_cent_freq1 - csa_cent_freq) != 30) {
			pe_err("CSA WBW 80MHz has invalid seg0 freq %d",
			       csa_cent_freq1);
			return QDF_STATUS_E_INVAL;
		}
		if (!csa_cent_freq2) {
			pe_err("CSA WBW 80MHz has invalid seg1 freq %d",
			       csa_cent_freq2);
			return QDF_STATUS_E_INVAL;
		}
		/* adjacent is not allowed -- that's a 160 MHz channel */
		if (abs(csa_cent_freq1 - csa_cent_freq2) == 80) {
			pe_err("CSA WBW wrong bandwidth");
			return QDF_STATUS_E_INVAL;
		}
		break;
	case CH_WIDTH_160MHZ:
		if (abs(csa_cent_freq1 - csa_cent_freq) != 70 &&
		    abs(csa_cent_freq1 - csa_cent_freq) != 50 &&
		    abs(csa_cent_freq1 - csa_cent_freq) != 30 &&
		    abs(csa_cent_freq1 - csa_cent_freq) != 10) {
			pr_err("CSA WBW 160MHz has invalid seg0 freq %d",
			       csa_cent_freq1);
			return QDF_STATUS_E_INVAL;
		}
		if (csa_cent_freq2) {
			pe_err("CSA WBW 80MHz has invalid seg1 freq %d",
			       csa_cent_freq2);
			return QDF_STATUS_E_INVAL;
		}
		break;
	default:
		pe_err("CSA wide BW IE has wrong ch_width %d", ap_new_ch_width);
		return QDF_STATUS_E_INVAL;
	}

	if (ap_new_ch_width > fw_vht_ch_wd) {
		pe_debug("New BW is not supported, setting BW to %d",
			 fw_vht_ch_wd);
		ap_new_ch_width = fw_vht_ch_wd;
		ch_params.ch_width = ap_new_ch_width;
		wlan_reg_set_channel_params(mac_ctx->pdev,
					    csa_params->channel, 0, &ch_params);
		ap_new_ch_width = ch_params.ch_width;
		csa_params->new_ch_freq_seg1 = ch_params.center_freq_seg0;
		csa_params->new_ch_freq_seg2 = ch_params.center_freq_seg1;
	}

	session_entry->gLimChannelSwitch.state =
		eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY;

	chnl_switch_info->newChanWidth = ap_new_ch_width;
	chnl_switch_info->newCenterChanFreq0 = csa_params->new_ch_freq_seg1;
	chnl_switch_info->newCenterChanFreq1 = csa_params->new_ch_freq_seg2;

	if (session_entry->ch_width == ap_new_ch_width)
		goto prnt_log;

	if (session_entry->ch_width == CH_WIDTH_80MHZ) {
		chnl_switch_info->newChanWidth = CH_WIDTH_80MHZ;
		chnl_switch_info->newCenterChanFreq1 = 0;
	} else {
		session_entry->ch_width = ap_new_ch_width;
		chnl_switch_info->newChanWidth = ap_new_ch_width;
	}
prnt_log:

	return QDF_STATUS_SUCCESS;
}

static bool lim_is_csa_channel_allowed(struct mac_context *mac_ctx,
				       qdf_freq_t ch_freq1,
				       uint32_t ch_freq2)
{
	bool is_allowed = true;
	u32 cnx_count = 0;

	cnx_count = policy_mgr_get_connection_count(mac_ctx->psoc);
	if ((cnx_count > 1) && !policy_mgr_is_hw_dbs_capable(mac_ctx->psoc) &&
	    !policy_mgr_is_interband_mcc_supported(mac_ctx->psoc)) {
		is_allowed = wlan_reg_is_same_band_freqs(ch_freq1, ch_freq2);
	}

	return is_allowed;
}

/**
 * lim_handle_csa_offload_msg() - Handle CSA offload message
 * @mac_ctx:         pointer to global adapter context
 * @msg:             Message pointer.
 *
 * Return: None
 */
void lim_handle_csa_offload_msg(struct mac_context *mac_ctx,
				struct scheduler_msg *msg)
{
	struct pe_session *session_entry;
	struct csa_offload_params *csa_params =
				(struct csa_offload_params *) (msg->bodyptr);
	tpDphHashNode sta_ds = NULL;
	uint8_t session_id;
	uint16_t aid = 0;
	uint16_t chan_space = 0;
	struct ch_params ch_params = {0};
	uint32_t channel_bonding_mode;
	uint8_t country_code[CDS_COUNTRY_CODE_LEN + 1];

	tLimWiderBWChannelSwitchInfo *chnl_switch_info = NULL;
	tLimChannelSwitchInfo *lim_ch_switch = NULL;

	if (!csa_params) {
		pe_err("limMsgQ body ptr is NULL");
		return;
	}

	session_entry =
		pe_find_session_by_bssid(mac_ctx,
			csa_params->bssId, &session_id);
	if (!session_entry) {
		pe_err("Session does not exists for "QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(csa_params->bssId));
		goto err;
	}

	sta_ds = dph_lookup_hash_entry(mac_ctx, session_entry->bssId, &aid,
		&session_entry->dph.dphHashTable);

	if (!sta_ds) {
		pe_err("sta_ds does not exist");
		goto err;
	}

	if (!LIM_IS_STA_ROLE(session_entry)) {
		pe_debug("Invalid role to handle CSA");
		goto err;
	}

	if (!lim_is_csa_channel_allowed(mac_ctx, session_entry->curr_op_freq,
					csa_params->csa_chan_freq)) {
		pe_debug("Channel switch is not allowed");
		goto err;
	}
	/*
	 * on receiving channel switch announcement from AP, delete all
	 * TDLS peers before leaving BSS and proceed for channel switch
	 */

	lim_update_tdls_set_state_for_fw(session_entry, false);
	lim_delete_tdls_peers(mac_ctx, session_entry);

	lim_ch_switch = &session_entry->gLimChannelSwitch;
	session_entry->gLimChannelSwitch.switchMode =
		csa_params->switch_mode;
	/* timer already started by firmware, switch immediately */
	session_entry->gLimChannelSwitch.switchCount = 0;
	session_entry->gLimChannelSwitch.primaryChannel =
		csa_params->channel;
	session_entry->gLimChannelSwitch.sw_target_freq =
		csa_params->csa_chan_freq;
	session_entry->gLimChannelSwitch.state =
		eLIM_CHANNEL_SWITCH_PRIMARY_ONLY;
	session_entry->gLimChannelSwitch.ch_width = CH_WIDTH_20MHZ;
	lim_ch_switch->sec_ch_offset =
		session_entry->htSecondaryChannelOffset;
	session_entry->gLimChannelSwitch.ch_center_freq_seg0 = 0;
	session_entry->gLimChannelSwitch.ch_center_freq_seg1 = 0;
	chnl_switch_info =
		&session_entry->gLimWiderBWChannelSwitch;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(csa_params->csa_chan_freq)) {
		channel_bonding_mode =
			mac_ctx->roam.configParam.channelBondingMode24GHz;
	} else {
		channel_bonding_mode =
			mac_ctx->roam.configParam.channelBondingMode5GHz;
	}

	pe_debug("Session %d vdev %d: vht: %d ht: %d he %d cbmode %d",
		 session_entry->peSessionId, session_entry->vdev_id,
		 session_entry->vhtCapability,
		 session_entry->htSupportedChannelWidthSet,
		 lim_is_session_he_capable(session_entry),
		 channel_bonding_mode);

	session_entry->htSupportedChannelWidthSet = false;
	wlan_reg_read_current_country(mac_ctx->psoc, country_code);
	if (channel_bonding_mode &&
	    ((session_entry->vhtCapability && session_entry->htCapability) ||
	      lim_is_session_he_capable(session_entry))) {
		if ((csa_params->ies_present_flag & lim_wbw_ie_present) &&
			(QDF_STATUS_SUCCESS == lim_process_csa_wbw_ie(mac_ctx,
					csa_params, chnl_switch_info,
					session_entry))) {
			lim_ch_switch->sec_ch_offset =
				PHY_SINGLE_CHANNEL_CENTERED;
			if (chnl_switch_info->newChanWidth) {
				ch_params.ch_width =
					chnl_switch_info->newChanWidth;
				wlan_reg_set_channel_params(mac_ctx->pdev,
					csa_params->channel, 0, &ch_params);
				lim_ch_switch->sec_ch_offset =
					ch_params.sec_ch_offset;
				session_entry->htSupportedChannelWidthSet =
									true;
			}
		} else if (csa_params->ies_present_flag
				& lim_xcsa_ie_present) {
			uint32_t fw_vht_ch_wd = wma_get_vht_ch_width();

			if (wlan_reg_is_6ghz_op_class
				(mac_ctx->pdev, csa_params->new_op_class)) {
				chan_space = wlan_reg_get_op_class_width
					(mac_ctx->pdev,
					 csa_params->new_op_class, true);
			} else {
				chan_space =
				wlan_reg_dmn_get_chanwidth_from_opclass_auto(
						country_code,
						csa_params->channel,
						csa_params->new_op_class);
			}
			if (chan_space >= 160 && fw_vht_ch_wd <
					WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
				chan_space = 80;
			session_entry->gLimChannelSwitch.state =
				eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY;
			if (chan_space == 160) {
				chnl_switch_info->newChanWidth =
					CH_WIDTH_160MHZ;
			} else if (chan_space == 80) {
				chnl_switch_info->newChanWidth =
					CH_WIDTH_80MHZ;
				session_entry->htSupportedChannelWidthSet =
									true;
			} else if (chan_space == 40) {
				chnl_switch_info->newChanWidth =
					CH_WIDTH_40MHZ;
				session_entry->htSupportedChannelWidthSet =
									true;
			} else {
				chnl_switch_info->newChanWidth =
					CH_WIDTH_20MHZ;
				lim_ch_switch->state =
					eLIM_CHANNEL_SWITCH_PRIMARY_ONLY;
			}

			ch_params.ch_width =
				chnl_switch_info->newChanWidth;
			wlan_reg_set_channel_params_for_freq(
				mac_ctx->pdev, csa_params->csa_chan_freq, 0,
				&ch_params);
			chnl_switch_info->newCenterChanFreq0 =
				ch_params.center_freq_seg0;
			/*
			 * This is not applicable for 20/40/80 MHz.
			 * Only used when we support 80+80 MHz operation.
			 * In case of 80+80 MHz, this parameter indicates
			 * center channel frequency index of 80 MHz
			 * channel offrequency segment 1.
			 */
			chnl_switch_info->newCenterChanFreq1 =
				ch_params.center_freq_seg1;
			lim_ch_switch->sec_ch_offset =
				ch_params.sec_ch_offset;

		} else {
			lim_ch_switch->state =
				eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY;
			ch_params.ch_width = CH_WIDTH_40MHZ;
			wlan_reg_set_channel_params(mac_ctx->pdev,
					csa_params->channel, 0, &ch_params);
			lim_ch_switch->sec_ch_offset =
				ch_params.sec_ch_offset;
			chnl_switch_info->newChanWidth = CH_WIDTH_40MHZ;
			chnl_switch_info->newCenterChanFreq0 =
				ch_params.center_freq_seg0;
			chnl_switch_info->newCenterChanFreq1 = 0;
			session_entry->htSupportedChannelWidthSet = true;
		}
		session_entry->gLimChannelSwitch.ch_center_freq_seg0 =
			chnl_switch_info->newCenterChanFreq0;
		session_entry->gLimChannelSwitch.ch_center_freq_seg1 =
			chnl_switch_info->newCenterChanFreq1;
		session_entry->gLimChannelSwitch.ch_width =
			chnl_switch_info->newChanWidth;

	} else if (channel_bonding_mode && session_entry->htCapability) {
		if (csa_params->ies_present_flag
				& lim_xcsa_ie_present) {
			chan_space =
				wlan_reg_dmn_get_chanwidth_from_opclass_auto(
						country_code,
						csa_params->channel,
						csa_params->new_op_class);
			lim_ch_switch->state =
				eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY;
			if (chan_space == 40) {
				lim_ch_switch->ch_width =
					CH_WIDTH_40MHZ;
				chnl_switch_info->newChanWidth =
					CH_WIDTH_40MHZ;
				ch_params.ch_width =
					chnl_switch_info->newChanWidth;
				wlan_reg_set_channel_params(mac_ctx->pdev,
						csa_params->channel,
						0, &ch_params);
				lim_ch_switch->ch_center_freq_seg0 =
					ch_params.center_freq_seg0;
				lim_ch_switch->sec_ch_offset =
					ch_params.sec_ch_offset;
				session_entry->htSupportedChannelWidthSet =
									true;
			} else {
				lim_ch_switch->ch_width =
					CH_WIDTH_20MHZ;
				chnl_switch_info->newChanWidth =
					CH_WIDTH_20MHZ;
				lim_ch_switch->state =
					eLIM_CHANNEL_SWITCH_PRIMARY_ONLY;
				lim_ch_switch->sec_ch_offset =
					PHY_SINGLE_CHANNEL_CENTERED;
			}
		} else {
			lim_ch_switch->ch_width =
				CH_WIDTH_40MHZ;
			lim_ch_switch->state =
				eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY;
			ch_params.ch_width = CH_WIDTH_40MHZ;
			wlan_reg_set_channel_params(mac_ctx->pdev,
					csa_params->channel, 0, &ch_params);
			lim_ch_switch->ch_center_freq_seg0 =
				ch_params.center_freq_seg0;
			lim_ch_switch->sec_ch_offset =
				ch_params.sec_ch_offset;
			session_entry->htSupportedChannelWidthSet = true;
		}
	}
	pe_debug("new ch %d freq %d width: %d freq0 %d freq1 %d ht width %d",
		 session_entry->gLimChannelSwitch.primaryChannel,
		 session_entry->gLimChannelSwitch.sw_target_freq,
		 session_entry->gLimChannelSwitch.ch_width,
		 session_entry->gLimChannelSwitch.ch_center_freq_seg0,
		 session_entry->gLimChannelSwitch.ch_center_freq_seg1,
		 session_entry->gLimChannelSwitch.sec_ch_offset);

	if (session_entry->curr_op_freq == csa_params->csa_chan_freq &&
	    session_entry->ch_width ==
			session_entry->gLimChannelSwitch.ch_width) {
		pe_debug("Ignore CSA, no change in ch and bw");
		goto err;
	}

	if (WLAN_REG_IS_24GHZ_CH_FREQ(csa_params->csa_chan_freq) &&
	    session_entry->dot11mode == MLME_DOT11_MODE_11A)
		session_entry->dot11mode = MLME_DOT11_MODE_11G;
	else if (!WLAN_REG_IS_24GHZ_CH_FREQ(csa_params->csa_chan_freq) &&
		 ((session_entry->dot11mode == MLME_DOT11_MODE_11G) ||
		 (session_entry->dot11mode == MLME_DOT11_MODE_11G_ONLY)))
		session_entry->dot11mode = MLME_DOT11_MODE_11A;

	/* Send RSO Stop to FW before triggering the vdev restart for CSA */
	if (mac_ctx->lim.stop_roaming_callback)
		mac_ctx->lim.stop_roaming_callback(MAC_HANDLE(mac_ctx),
						   session_entry->smeSessionId,
						   REASON_DRIVER_DISABLED,
						   RSO_CHANNEL_SWITCH);

	lim_prepare_for11h_channel_switch(mac_ctx, session_entry);

	lim_flush_bssid(mac_ctx, session_entry->bssId);

#ifdef FEATURE_WLAN_DIAG_SUPPORT
	lim_diag_event_report(mac_ctx,
			WLAN_PE_DIAG_SWITCH_CHL_IND_EVENT, session_entry,
			QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
#endif

err:
	qdf_mem_free(csa_params);
}

/*--------------------------------------------------------------------------
   \brief pe_delete_session() - Handle the Delete BSS Response from HAL.

   \param mac                   - pointer to global adapter context
   \param sessionId             - Message pointer.

   \sa
   --------------------------------------------------------------------------*/

void lim_handle_delete_bss_rsp(struct mac_context *mac,
			       struct del_bss_resp *del_bss_rsp)
{
	struct pe_session *pe_session;

	pe_session =
	   pe_find_session_by_vdev_id_and_state(mac,
						del_bss_rsp->vdev_id,
						eLIM_MLM_WT_DEL_BSS_RSP_STATE);
	if (!pe_session) {
		qdf_mem_free(del_bss_rsp);
		return;
	}

	/*
	 * During DEL BSS handling, the PE Session will be deleted, but it is
	 * better to clear this flag if the session is hanging around due
	 * to some error conditions so that the next DEL_BSS request does
	 * not take the HO_FAIL path
	 */
	pe_session->process_ho_fail = false;
	if (LIM_IS_UNKNOWN_ROLE(pe_session))
		lim_process_sme_del_bss_rsp(mac, pe_session);
	else if (LIM_IS_NDI_ROLE(pe_session))
		lim_ndi_del_bss_rsp(mac, del_bss_rsp, pe_session);
	else
		lim_process_mlm_del_bss_rsp(mac, del_bss_rsp, pe_session);

	qdf_mem_free(del_bss_rsp);
}

/** -----------------------------------------------------------------
   \brief lim_send_sme_aggr_qos_rsp() - sends SME FT AGGR QOS RSP
 \      This function sends a eWNI_SME_FT_AGGR_QOS_RSP to SME.
 \      SME only looks at rc and tspec field.
   \param mac - global mac structure
   \param rspReqd - is SmeAddTsRsp required
   \param status - status code of eWNI_SME_FT_AGGR_QOS_RSP
   \return tspec
   \sa
   ----------------------------------------------------------------- */
void
lim_send_sme_aggr_qos_rsp(struct mac_context *mac, tpSirAggrQosRsp aggrQosRsp,
			  uint8_t smesessionId)
{
	struct scheduler_msg mmhMsg = {0};

	mmhMsg.type = eWNI_SME_FT_AGGR_QOS_RSP;
	mmhMsg.bodyptr = aggrQosRsp;
	mmhMsg.bodyval = 0;
	MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
			 smesessionId, mmhMsg.type));
	lim_sys_process_mmh_msg_api(mac, &mmhMsg);

	return;
}

void lim_send_sme_max_assoc_exceeded_ntf(struct mac_context *mac, tSirMacAddr peerMacAddr,
					 uint8_t smesessionId)
{
	struct scheduler_msg mmhMsg = {0};
	tSmeMaxAssocInd *pSmeMaxAssocInd;

	pSmeMaxAssocInd = qdf_mem_malloc(sizeof(tSmeMaxAssocInd));
	if (!pSmeMaxAssocInd)
		return;
	qdf_mem_copy((uint8_t *) pSmeMaxAssocInd->peer_mac.bytes,
		     (uint8_t *) peerMacAddr, QDF_MAC_ADDR_SIZE);
	pSmeMaxAssocInd->mesgType = eWNI_SME_MAX_ASSOC_EXCEEDED;
	pSmeMaxAssocInd->mesgLen = sizeof(tSmeMaxAssocInd);
	pSmeMaxAssocInd->sessionId = smesessionId;
	mmhMsg.type = pSmeMaxAssocInd->mesgType;
	mmhMsg.bodyptr = pSmeMaxAssocInd;
	pe_debug("msgType: %s peerMacAddr "QDF_MAC_ADDR_FMT "sme session id %d",
		"eWNI_SME_MAX_ASSOC_EXCEEDED", QDF_MAC_ADDR_REF(peerMacAddr),
		pSmeMaxAssocInd->sessionId);
	MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
			 smesessionId, mmhMsg.type));
	lim_sys_process_mmh_msg_api(mac, &mmhMsg);

	return;
}

/** -----------------------------------------------------------------
   \brief lim_send_sme_ap_channel_switch_resp() - sends
   eWNI_SME_CHANNEL_CHANGE_RSP
   After receiving WMA_SWITCH_CHANNEL_RSP indication this
   function sends a eWNI_SME_CHANNEL_CHANGE_RSP to SME to notify
   that the Channel change has been done to the specified target
   channel in the Channel change request
   \param mac - global mac structure
   \param pe_session - session info
   \param pChnlParams - Channel switch params
   --------------------------------------------------------------------*/
void
lim_send_sme_ap_channel_switch_resp(struct mac_context *mac,
				    struct pe_session *pe_session,
				    struct vdev_start_response *rsp)
{
	struct scheduler_msg mmhMsg = {0};
	struct sSirChanChangeResponse *chan_change_rsp;
	bool is_ch_dfs = false;
	enum phy_ch_width ch_width;
	uint32_t ch_cfreq1 = 0;
	enum reg_wifi_band band;

	qdf_runtime_pm_allow_suspend(&pe_session->ap_ecsa_runtime_lock);
	qdf_wake_lock_release(&pe_session->ap_ecsa_wakelock, 0);

	chan_change_rsp =
		qdf_mem_malloc(sizeof(struct sSirChanChangeResponse));
	if (!chan_change_rsp)
		return;

	chan_change_rsp->new_op_freq = pe_session->curr_op_freq;
	chan_change_rsp->channelChangeStatus = rsp->status;
	/*
	 * Pass the sme sessionID to SME instead
	 * PE session ID.
	 */
	chan_change_rsp->sessionId = rsp->vdev_id;

	mmhMsg.type = eWNI_SME_CHANNEL_CHANGE_RSP;
	mmhMsg.bodyptr = (void *)chan_change_rsp;
	mmhMsg.bodyval = 0;
	lim_sys_process_mmh_msg_api(mac, &mmhMsg);

	if (QDF_IS_STATUS_ERROR(rsp->status)) {
		pe_err("failed to change sap freq to %u",
		       pe_session->curr_op_freq);
		return;
	}

	/*
	 * We should start beacon transmission only if the new
	 * channel after channel change is Non-DFS. For a DFS
	 * channel, PE will receive an explicit request from
	 * upper layers to start the beacon transmission .
	 */
	ch_width = pe_session->ch_width;
	band = wlan_reg_freq_to_band(pe_session->curr_op_freq);
	if (pe_session->ch_center_freq_seg1)
		ch_cfreq1 = wlan_reg_chan_band_to_freq(
				mac->pdev,
				pe_session->ch_center_freq_seg1,
				BIT(band));

	if (ch_width == CH_WIDTH_160MHZ) {
		is_ch_dfs = true;
	} else if (ch_width == CH_WIDTH_80P80MHZ) {
		if (wlan_reg_get_channel_state_for_freq(
						mac->pdev,
						pe_session->curr_op_freq) ==
		    CHANNEL_STATE_DFS ||
		    wlan_reg_get_channel_state_for_freq(
						mac->pdev,
						ch_cfreq1) ==
				CHANNEL_STATE_DFS)
			is_ch_dfs = true;
	} else {
		if (wlan_reg_get_channel_state_for_freq(
						mac->pdev,
						pe_session->curr_op_freq) ==
		    CHANNEL_STATE_DFS)
			is_ch_dfs = true;
	}
	if (WLAN_REG_IS_6GHZ_CHAN_FREQ(pe_session->curr_op_freq))
		is_ch_dfs = false;

	if (is_ch_dfs) {
		lim_sap_move_to_cac_wait_state(pe_session);

	} else {
		lim_apply_configuration(mac, pe_session);
		lim_send_beacon(mac, pe_session);
		lim_obss_send_detection_cfg(mac, pe_session, true);
	}
	return;
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
/**
 *  lim_send_bss_color_change_ie_update() - update bss color change IE in
 *   beacon template
 *
 *  @mac_ctx: pointer to global adapter context
 *  @session: session pointer
 *
 *  Return: none
 */
static void
lim_send_bss_color_change_ie_update(struct mac_context *mac_ctx,
						struct pe_session *session)
{
	/* Update the beacon template and send to FW */
	if (sch_set_fixed_beacon_fields(mac_ctx, session) != QDF_STATUS_SUCCESS) {
		pe_err("Unable to set BSS color change IE in beacon");
		return;
	}

	/* Send update beacon template message */
	lim_send_beacon_ind(mac_ctx, session, REASON_COLOR_CHANGE);
	pe_debug("Updated BSS color change countdown = %d",
		 session->he_bss_color_change.countdown);
}

static void
lim_handle_bss_color_change_ie(struct mac_context *mac_ctx,
					struct pe_session *session)
{
	tUpdateBeaconParams beacon_params;

	/* handle bss color change IE */
	if (LIM_IS_AP_ROLE(session) &&
	    session->he_op.bss_col_disabled &&
	    session->he_bss_color_change.new_color) {
		pe_debug("countdown: %d, new_color: %d",
			 session->he_bss_color_change.countdown,
			 session->he_bss_color_change.new_color);
		if (session->he_bss_color_change.countdown > 0) {
			session->he_bss_color_change.countdown--;
		} else {
			session->bss_color_changing = 0;
			qdf_mem_zero(&beacon_params, sizeof(beacon_params));
			session->he_op.bss_col_disabled = 0;
			session->he_op.bss_color =
				session->he_bss_color_change.new_color;
			session->he_bss_color_change.new_color = 0;
			beacon_params.paramChangeBitmap |=
				PARAM_BSS_COLOR_CHANGED;
			beacon_params.bss_color_disabled = 0;
			beacon_params.bss_color = session->he_op.bss_color;
			lim_send_beacon_params(mac_ctx,
					       &beacon_params,
					       session);
			lim_send_obss_color_collision_cfg(
				mac_ctx, session,
				OBSS_COLOR_COLLISION_DETECTION);
		}
		lim_send_bss_color_change_ie_update(mac_ctx, session);
	}
}

#else
static void
lim_handle_bss_color_change_ie(struct mac_context *mac_ctx,
					struct pe_session *session)
{
}
#endif

void
lim_process_beacon_tx_success_ind(struct mac_context *mac_ctx, uint16_t msgType,
				  void *event)
{
	struct pe_session *session;
	bool csa_tx_offload;
	tpSirFirstBeaconTxCompleteInd bcn_ind =
		(tSirFirstBeaconTxCompleteInd *) event;

	session = pe_find_session_by_vdev_id(mac_ctx, bcn_ind->bss_idx);
	if (!session) {
		pe_err("Session Does not exist for given session id");
		return;
	}

	pe_debug("role: %d swIe: %d opIe: %d switch cnt:%d",
		 GET_LIM_SYSTEM_ROLE(session),
		 session->dfsIncludeChanSwIe,
		 session->gLimOperatingMode.present,
		 session->gLimChannelSwitch.switchCount);

	if (!LIM_IS_AP_ROLE(session))
		return;
	csa_tx_offload = wlan_psoc_nif_fw_ext_cap_get(mac_ctx->psoc,
						WLAN_SOC_CEXT_CSA_TX_OFFLOAD);
	if (session->dfsIncludeChanSwIe &&
	    (session->gLimChannelSwitch.switchCount ==
	    mac_ctx->sap.SapDfsInfo.sap_ch_switch_beacon_cnt) &&
	    !csa_tx_offload)
		lim_process_ap_ecsa_timeout(session);


	if (session->gLimOperatingMode.present)
		/* Done with nss update */
		session->gLimOperatingMode.present = 0;

	lim_handle_bss_color_change_ie(mac_ctx, session);

	return;
}
