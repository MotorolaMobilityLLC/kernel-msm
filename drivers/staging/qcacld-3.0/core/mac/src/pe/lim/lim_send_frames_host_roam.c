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
 * DOC: lim_send_frames_host_roam.c
 *
 * Send management frames for host based roaming
 */
#include "sir_api.h"
#include "ani_global.h"
#include "sir_mac_prot_def.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_security_utils.h"
#include "lim_prop_exts_utils.h"
#include "dot11f.h"
#include "sch_api.h"
#include "lim_send_messages.h"
#include "lim_assoc_utils.h"
#include "lim_ft.h"
#ifdef WLAN_FEATURE_11W
#include "wni_cfg.h"
#endif

#include "lim_ft_defs.h"
#include "lim_session.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "cds_utils.h"
#include "sme_trace.h"
#include "rrm_api.h"

#include "wma_types.h"
#include "wlan_utility.h"

/**
 * lim_send_reassoc_req_with_ft_ies_mgmt_frame() - Send Reassoc Req with FTIEs.
 *
 * @mac_ctx: Handle to mac context
 * @mlm_reassoc_req: Original reassoc request
 * @pe_session: PE session information
 *
 * It builds a reassoc request with FT IEs and sends it to AP through WMA.
 * Then it creates assoc request and stores it for sending after join
 * confirmation.
 *
 * Return: None
 */
void lim_send_reassoc_req_with_ft_ies_mgmt_frame(struct mac_context *mac_ctx,
	tLimMlmReassocReq *mlm_reassoc_req,
	struct pe_session *pe_session)
{
	tDot11fReAssocRequest *frm;
	uint16_t caps;
	uint8_t *frame;
	uint32_t bytes, payload, status;
	uint8_t qos_enabled, wme_enabled, wsm_enabled;
	void *packet;
	QDF_STATUS qdf_status;
	uint8_t power_caps_populated = false;
	uint16_t ft_ies_length = 0;
	uint8_t *body;
	uint16_t add_ie_len;
	uint8_t *add_ie;
	const uint8_t *wps_ie = NULL;
	uint8_t tx_flag = 0;
	uint8_t vdev_id = 0;
	bool vht_enabled = false;
	tpSirMacMgmtHdr mac_hdr;
	tftSMEContext *ft_sme_context;

	if (!pe_session)
		return;

	vdev_id = pe_session->vdev_id;

	/* check this early to avoid unncessary operation */
	if (!pe_session->pLimReAssocReq)
		return;

	frm = qdf_mem_malloc(sizeof(*frm));
	if (!frm)
		goto err;

	add_ie_len = pe_session->pLimReAssocReq->addIEAssoc.length;
	add_ie = pe_session->pLimReAssocReq->addIEAssoc.addIEdata;
	pe_debug("called in state: %d", pe_session->limMlmState);

	qdf_mem_zero((uint8_t *) frm, sizeof(*frm));

	caps = mlm_reassoc_req->capabilityInfo;
#if defined(FEATURE_WLAN_WAPI)
	/*
	 * According to WAPI standard:
	 * 7.3.1.4 Capability Information field
	 * In WAPI, non-AP STAs within an ESS set the Privacy subfield
	 * to 0 in transmitted Association or Reassociation management
	 * frames. APs ignore the Privacy subfield within received
	 * Association and Reassociation management frames.
	 */
	if (pe_session->encryptType == eSIR_ED_WPI)
		((tSirMacCapabilityInfo *) &caps)->privacy = 0;
#endif
	swap_bit_field16(caps, (uint16_t *) &frm->Capabilities);

	frm->ListenInterval.interval = mlm_reassoc_req->listenInterval;

	/*
	 * Get the old bssid of the older AP.
	 * The previous ap bssid is stored in the FT Session
	 * while creating the PE FT Session for reassociation.
	 */
	qdf_mem_copy((uint8_t *)frm->CurrentAPAddress.mac,
			pe_session->prev_ap_bssid, sizeof(tSirMacAddr));

	populate_dot11f_ssid2(mac_ctx, &frm->SSID);
	populate_dot11f_supp_rates(mac_ctx, POPULATE_DOT11F_RATES_OPERATIONAL,
		&frm->SuppRates, pe_session);

	qos_enabled = (pe_session->limQosEnabled) &&
		      SIR_MAC_GET_QOS(pe_session->limReassocBssCaps);

	wme_enabled = (pe_session->limWmeEnabled) &&
		      LIM_BSS_CAPS_GET(WME, pe_session->limReassocBssQosCaps);

	wsm_enabled = (pe_session->limWsmEnabled) && wme_enabled &&
		      LIM_BSS_CAPS_GET(WSM, pe_session->limReassocBssQosCaps);

	if (pe_session->lim11hEnable &&
	    pe_session->pLimReAssocReq->spectrumMgtIndicator == true) {
		power_caps_populated = true;

		populate_dot11f_power_caps(mac_ctx, &frm->PowerCaps,
					   LIM_REASSOC, pe_session);
		populate_dot11f_supp_channels(mac_ctx, &frm->SuppChannels,
			LIM_REASSOC, pe_session);
	}
	if (mac_ctx->rrm.rrmPEContext.rrmEnable &&
	    SIR_MAC_GET_RRM(pe_session->limCurrentBssCaps)) {
		if (power_caps_populated == false) {
			power_caps_populated = true;
			populate_dot11f_power_caps(mac_ctx, &frm->PowerCaps,
				LIM_REASSOC, pe_session);
		}
	}

	if (qos_enabled)
		populate_dot11f_qos_caps_station(mac_ctx, pe_session,
						&frm->QOSCapsStation);

	populate_dot11f_ext_supp_rates(mac_ctx,
		POPULATE_DOT11F_RATES_OPERATIONAL, &frm->ExtSuppRates,
		pe_session);

	if (mac_ctx->rrm.rrmPEContext.rrmEnable &&
	    SIR_MAC_GET_RRM(pe_session->limCurrentBssCaps))
		populate_dot11f_rrm_ie(mac_ctx, &frm->RRMEnabledCap, pe_session);

	/*
	 * Ideally this should be enabled for 11r also. But 11r does
	 * not follow the usual norm of using the Opaque object
	 * for rsnie and fties. Instead we just add the rsnie and fties
	 * at the end of the pack routine for 11r.
	 * This should ideally! be fixed.
	 */
	/*
	 * The join request *should* contain zero or one of the WPA and RSN
	 * IEs.  The payload send along with the request is a
	 * 'struct join_req'; the IE portion is held inside a 'tSirRSNie':
	 *
	 *     typedef struct sSirRSNie
	 *     {
	 *         uint16_t       length;
	 *         uint8_t        rsnIEdata[WLAN_MAX_IE_LEN+2];
	 *     } tSirRSNie, *tpSirRSNie;
	 *
	 * So, we should be able to make the following two calls harmlessly,
	 * since they do nothing if they don't find the given IE in the
	 * bytestream with which they're provided.
	 *
	 * The net effect of this will be to faithfully transmit whatever
	 * security IE is in the join request.

	 * However, if we're associating for the purpose of WPS
	 * enrollment, and we've been configured to indicate that by
	 * eliding the WPA or RSN IE, we just skip this:
	 */
	if (!pe_session->is11Rconnection) {
		if (add_ie_len && add_ie)
			wps_ie = limGetWscIEPtr(mac_ctx, add_ie, add_ie_len);
		if (!wps_ie) {
			populate_dot11f_rsn_opaque(mac_ctx,
				&(pe_session->pLimReAssocReq->rsnIE),
				&frm->RSNOpaque);
			populate_dot11f_wpa_opaque(mac_ctx,
				&(pe_session->pLimReAssocReq->rsnIE),
				&frm->WPAOpaque);
		}
#ifdef FEATURE_WLAN_ESE
		if (pe_session->pLimReAssocReq->cckmIE.length) {
			populate_dot11f_ese_cckm_opaque(mac_ctx,
				&(pe_session->pLimReAssocReq->cckmIE),
				&frm->ESECckmOpaque);
		}
#endif
	}
#ifdef FEATURE_WLAN_ESE
	/*
	 * ESE Version IE will be included in re-association request
	 * when ESE is enabled on DUT through ini and it is also
	 * advertised by the peer AP to which we are trying to
	 * associate to.
	 */
	if (pe_session->is_ese_version_ie_present &&
		mac_ctx->mlme_cfg->lfr.ese_enabled)
		populate_dot11f_ese_version(&frm->ESEVersion);
	/* For ESE Associations fill the ESE IEs */
	if (pe_session->isESEconnection &&
	    pe_session->pLimReAssocReq->isESEFeatureIniEnabled) {
#ifndef FEATURE_DISABLE_RM
		populate_dot11f_ese_rad_mgmt_cap(&frm->ESERadMgmtCap);
#endif
	}
#endif /* FEATURE_WLAN_ESE */

	/* include WME EDCA IE as well */
	if (wme_enabled) {
		populate_dot11f_wmm_info_station_per_session(mac_ctx,
			pe_session, &frm->WMMInfoStation);
		if (wsm_enabled)
			populate_dot11f_wmm_caps(&frm->WMMCaps);
#ifdef FEATURE_WLAN_ESE
		if (pe_session->isESEconnection) {
			uint32_t phymode;
			uint8_t rate;

			populate_dot11f_re_assoc_tspec(mac_ctx, frm,
				pe_session);

			/*
			 * Populate the TSRS IE if TSPEC is included in
			 * the reassoc request
			 */
			lim_get_phy_mode(mac_ctx, &phymode, pe_session);
			if (phymode == WNI_CFG_PHY_MODE_11G ||
			    phymode == WNI_CFG_PHY_MODE_11A)
				rate = TSRS_11AG_RATE_6MBPS;
			else
				rate = TSRS_11B_RATE_5_5MBPS;

			if (pe_session->pLimReAssocReq->eseTspecInfo.
			    numTspecs) {
				struct ese_tsrs_ie tsrs_ie;

				tsrs_ie.tsid = 0;
				tsrs_ie.rates[0] = rate;
				populate_dot11_tsrsie(mac_ctx, &tsrs_ie,
					&frm->ESETrafStrmRateSet,
					sizeof(uint8_t));
			}
		}
#endif
	}

	ft_sme_context = &mac_ctx->roam.roamSession[vdev_id].ftSmeContext;
	if (pe_session->htCapability &&
	    mac_ctx->lim.htCapabilityPresentInBeacon) {
		populate_dot11f_ht_caps(mac_ctx, pe_session, &frm->HTCaps);
	}
	if (pe_session->pLimReAssocReq->bssDescription.mdiePresent &&
	    (ft_sme_context->addMDIE == true)
#if defined FEATURE_WLAN_ESE
	    && !pe_session->isESEconnection
#endif
	    ) {
		populate_mdie(mac_ctx, &frm->MobilityDomain,
			pe_session->pLimReAssocReq->bssDescription.mdie);
	}
	if (pe_session->vhtCapability &&
	    pe_session->vhtCapabilityPresentInBeacon) {
		pe_debug("Populate VHT IEs in Re-Assoc Request");
		populate_dot11f_vht_caps(mac_ctx, pe_session, &frm->VHTCaps);
		vht_enabled = true;
		populate_dot11f_ext_cap(mac_ctx, vht_enabled, &frm->ExtCap,
			pe_session);
	}
	if (!vht_enabled &&
			pe_session->is_vendor_specific_vhtcaps) {
		pe_debug("Populate Vendor VHT IEs in Re-Assoc Request");
		frm->vendor_vht_ie.present = 1;
		frm->vendor_vht_ie.sub_type =
			pe_session->vendor_specific_vht_ie_sub_type;
		frm->vendor_vht_ie.VHTCaps.present = 1;
		populate_dot11f_vht_caps(mac_ctx, pe_session,
				&frm->vendor_vht_ie.VHTCaps);
		vht_enabled = true;
	}

	if (lim_is_session_he_capable(pe_session)) {
		pe_debug("Populate HE IEs");
		populate_dot11f_he_caps(mac_ctx, pe_session,
					&frm->he_cap);
		populate_dot11f_he_6ghz_cap(mac_ctx, pe_session,
					    &frm->he_6ghz_band_cap);
	}

	status = dot11f_get_packed_re_assoc_request_size(mac_ctx, frm,
			&payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failure in size calculation (0x%08x)", status);
		/* We'll fall back on the worst case scenario: */
		payload = sizeof(tDot11fReAssocRequest);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("Warnings in size calculation (0x%08x)", status);
	}

	bytes = payload + sizeof(tSirMacMgmtHdr) + add_ie_len;

	pe_debug("FT IE Reassoc Req %d",
		ft_sme_context->reassoc_ft_ies_length);

	if (pe_session->is11Rconnection)
		ft_ies_length = ft_sme_context->reassoc_ft_ies_length;

	qdf_status = cds_packet_alloc((uint16_t) bytes + ft_ies_length,
				 (void **)&frame, (void **)&packet);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_session->limMlmState = pe_session->limPrevMlmState;
		MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_MLM_STATE,
				 pe_session->peSessionId,
				 pe_session->limMlmState));
		pe_err("Failed to alloc memory %d", bytes);
		goto end;
	}
	/* Paranoia: */
	qdf_mem_zero(frame, bytes + ft_ies_length);

	lim_print_mac_addr(mac_ctx, pe_session->limReAssocbssId, LOGD);
	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac_ctx, frame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_REASSOC_REQ, pe_session->limReAssocbssId,
		pe_session->self_mac_addr);
	mac_hdr = (tpSirMacMgmtHdr) frame;
	/* That done, pack the ReAssoc Request: */
	status = dot11f_pack_re_assoc_request(mac_ctx, frm, frame +
					       sizeof(tSirMacMgmtHdr),
					       payload, &payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failure in pack (0x%08x)", status);
		cds_packet_free((void *)packet);
		goto end;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("Warnings in pack (0x%08x)", status);
	}

	pe_debug("*** Sending Re-Assoc Request length: %d %d to",
		       bytes, payload);

	if (pe_session->assoc_req) {
		qdf_mem_free(pe_session->assoc_req);
		pe_session->assoc_req = NULL;
		pe_session->assocReqLen = 0;
	}

	if (add_ie_len) {
		qdf_mem_copy(frame + sizeof(tSirMacMgmtHdr) + payload,
			     add_ie, add_ie_len);
		payload += add_ie_len;
	}

	pe_session->assoc_req = qdf_mem_malloc(payload);
	if (pe_session->assoc_req) {
		/*
		 * Store the Assoc request. This is sent to csr/hdd in
		 * join cnf response.
		 */
		qdf_mem_copy(pe_session->assoc_req,
			     frame + sizeof(tSirMacMgmtHdr), payload);
		pe_session->assocReqLen = payload;
	}

	if (pe_session->is11Rconnection && ft_sme_context->reassoc_ft_ies) {
		int i = 0;

		body = frame + bytes;
		for (i = 0; i < ft_ies_length; i++) {
			*body = ft_sme_context->reassoc_ft_ies[i];
			body++;
		}
	}
	pe_debug("Re-assoc Req Frame is:");
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   (uint8_t *) frame, (bytes + ft_ies_length));

	if ((pe_session->ftPEContext.pFTPreAuthReq) &&
	    (!wlan_reg_is_24ghz_ch_freq(
	     pe_session->ftPEContext.pFTPreAuthReq->pre_auth_channel_freq)))
		tx_flag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
	else if (wlan_reg_is_5ghz_ch_freq(pe_session->curr_op_freq) ||
		 pe_session->opmode == QDF_P2P_CLIENT_MODE ||
		 pe_session->opmode == QDF_P2P_GO_MODE)
		tx_flag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	if (pe_session->assoc_req) {
		qdf_mem_free(pe_session->assoc_req);
		pe_session->assoc_req = NULL;
		pe_session->assocReqLen = 0;
	}
	if (ft_ies_length) {
		pe_session->assoc_req = qdf_mem_malloc(ft_ies_length);
		if (!pe_session->assoc_req) {
			pe_session->assocReqLen = 0;
		} else {
			/*
			 * Store the FT IEs. This is sent to csr/hdd in
			 * join cnf response.
			 */
			qdf_mem_copy(pe_session->assoc_req,
				     ft_sme_context->reassoc_ft_ies,
				     ft_ies_length);
			pe_session->assocReqLen = ft_ies_length;
		}
	} else {
		pe_debug("FT IEs not present");
		pe_session->assocReqLen = 0;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, mac_hdr->fc.subType));
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_REASSOC_START_EVENT,
			      pe_session, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
	lim_diag_mgmt_tx_event_report(mac_ctx, mac_hdr,
				      pe_session, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
	qdf_status = wma_tx_frame(mac_ctx, packet,
				(uint16_t) (bytes + ft_ies_length),
				TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS, 7,
				lim_tx_complete, frame, tx_flag, vdev_id,
				0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
		       pe_session->peSessionId, qdf_status));
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to send Re-Assoc Request: %X!", qdf_status);
	}

end:
	qdf_mem_free(frm);
err:
	/* Free up buffer allocated for mlmAssocReq */
	qdf_mem_free(mlm_reassoc_req);
	pe_session->pLimMlmReassocReq = NULL;

}

/**
 * lim_send_retry_reassoc_req_frame() - Retry for reassociation
 * @mac: Global MAC Context
 * @pMlmReassocReq: Request buffer to be sent
 * @pe_session: PE Session
 *
 * Return: None
 */
void lim_send_retry_reassoc_req_frame(struct mac_context *mac,
				      tLimMlmReassocReq *pMlmReassocReq,
				      struct pe_session *pe_session)
{
	tLimMlmReassocCnf mlmReassocCnf;        /* keep sme */
	tLimMlmReassocReq *pTmpMlmReassocReq = NULL;

	if (!pTmpMlmReassocReq) {
		pTmpMlmReassocReq = qdf_mem_malloc(sizeof(tLimMlmReassocReq));
		if (!pTmpMlmReassocReq)
			goto end;
		qdf_mem_copy(pTmpMlmReassocReq, pMlmReassocReq,
			     sizeof(tLimMlmReassocReq));
	}
	/* Prepare and send Reassociation request frame */
	/* start reassoc timer. */
	mac->lim.lim_timers.gLimReassocFailureTimer.sessionId =
		pe_session->peSessionId;
	/* Start reassociation failure timer */
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TIMER_ACTIVATE,
			 pe_session->peSessionId, eLIM_REASSOC_FAIL_TIMER));
	if (tx_timer_activate(&mac->lim.lim_timers.gLimReassocFailureTimer)
	    != TX_SUCCESS) {
		/* Could not start reassoc failure timer. */
		/* Log error */
		pe_err("could not start Reassociation failure timer");
		/* Return Reassoc confirm with */
		/* Resources Unavailable */
		mlmReassocCnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;
		mlmReassocCnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;
		goto end;
	}

	lim_send_reassoc_req_with_ft_ies_mgmt_frame(mac, pTmpMlmReassocReq,
						    pe_session);
	return;

end:
	/* Free up buffer allocated for reassocReq */
	if (pMlmReassocReq) {
		qdf_mem_free(pMlmReassocReq);
		pMlmReassocReq = NULL;
	}
	if (pTmpMlmReassocReq) {
		qdf_mem_free(pTmpMlmReassocReq);
		pTmpMlmReassocReq = NULL;
	}
	mlmReassocCnf.resultCode = eSIR_SME_FT_REASSOC_FAILURE;
	mlmReassocCnf.protStatusCode = STATUS_UNSPECIFIED_FAILURE;
	/* Update PE sessio Id */
	mlmReassocCnf.sessionId = pe_session->peSessionId;

	lim_post_sme_message(mac, LIM_MLM_REASSOC_CNF,
			     (uint32_t *) &mlmReassocCnf);
}

/**
 * lim_send_reassoc_req_mgmt_frame() - Send the reassociation frame
 * @mac: Global MAC Context
 * @pMlmReassocReq: Reassociation request buffer to be sent
 * @pe_session: PE Session
 *
 * Return: None
 */
void lim_send_reassoc_req_mgmt_frame(struct mac_context *mac,
				tLimMlmReassocReq *pMlmReassocReq,
				struct pe_session *pe_session)
{
	tDot11fReAssocRequest *frm;
	uint16_t caps;
	uint8_t *pFrame;
	uint32_t nBytes, nPayload, nStatus;
	uint8_t fQosEnabled, fWmeEnabled, fWsmEnabled;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint16_t nAddIELen;
	uint8_t *pAddIE;
	const uint8_t *wpsIe = NULL;
	uint8_t txFlag = 0;
	uint8_t PowerCapsPopulated = false;
	uint8_t smeSessionId = 0;
	bool isVHTEnabled = false;
	tpSirMacMgmtHdr pMacHdr;

	if (!pe_session)
		return;

	smeSessionId = pe_session->smeSessionId;
	if (!pe_session->pLimReAssocReq)
		return;

	frm = qdf_mem_malloc(sizeof(*frm));
	if (!frm)
		goto err;
	nAddIELen = pe_session->pLimReAssocReq->addIEAssoc.length;
	pAddIE = pe_session->pLimReAssocReq->addIEAssoc.addIEdata;

	qdf_mem_zero((uint8_t *) frm, sizeof(*frm));

	caps = pMlmReassocReq->capabilityInfo;
#if defined(FEATURE_WLAN_WAPI)
	/*
	 * CR: 262463 :
	 * According to WAPI standard:
	 * 7.3.1.4 Capability Information field
	 * In WAPI, non-AP STAs within an ESS set the Privacy subfield to 0 in
	 * transmitted. Association or Reassociation management frames. APs
	 * ignore the Privacy subfield within received Association and
	 * Reassociation management frames.
	 */
	if (pe_session->encryptType == eSIR_ED_WPI)
		((tSirMacCapabilityInfo *) &caps)->privacy = 0;
#endif
	swap_bit_field16(caps, (uint16_t *) &frm->Capabilities);

	frm->ListenInterval.interval = pMlmReassocReq->listenInterval;

	qdf_mem_copy((uint8_t *) frm->CurrentAPAddress.mac,
		     (uint8_t *) pe_session->bssId, 6);

	populate_dot11f_ssid2(mac, &frm->SSID);
	populate_dot11f_supp_rates(mac, POPULATE_DOT11F_RATES_OPERATIONAL,
				   &frm->SuppRates, pe_session);

	fQosEnabled = (pe_session->limQosEnabled) &&
		      SIR_MAC_GET_QOS(pe_session->limReassocBssCaps);

	fWmeEnabled = (pe_session->limWmeEnabled) &&
		     LIM_BSS_CAPS_GET(WME, pe_session->limReassocBssQosCaps);

	fWsmEnabled = (pe_session->limWsmEnabled) && fWmeEnabled &&
		     LIM_BSS_CAPS_GET(WSM, pe_session->limReassocBssQosCaps);

	if (pe_session->lim11hEnable &&
	    pe_session->pLimReAssocReq->spectrumMgtIndicator == true) {
		PowerCapsPopulated = true;
		populate_dot11f_power_caps(mac, &frm->PowerCaps, LIM_REASSOC,
					   pe_session);
		populate_dot11f_supp_channels(mac, &frm->SuppChannels,
				LIM_REASSOC, pe_session);
	}
	if (mac->rrm.rrmPEContext.rrmEnable &&
	    SIR_MAC_GET_RRM(pe_session->limCurrentBssCaps)) {
		if (PowerCapsPopulated == false) {
			PowerCapsPopulated = true;
			populate_dot11f_power_caps(mac, &frm->PowerCaps,
						   LIM_REASSOC, pe_session);
		}
	}

	if (fQosEnabled)
		populate_dot11f_qos_caps_station(mac, pe_session,
						&frm->QOSCapsStation);

	populate_dot11f_ext_supp_rates(mac, POPULATE_DOT11F_RATES_OPERATIONAL,
				       &frm->ExtSuppRates, pe_session);

	if (mac->rrm.rrmPEContext.rrmEnable &&
	    SIR_MAC_GET_RRM(pe_session->limCurrentBssCaps))
		populate_dot11f_rrm_ie(mac, &frm->RRMEnabledCap, pe_session);
	/* The join request *should* contain zero or one of the WPA and RSN */
	/* IEs.  The payload send along with the request is a */
	/* 'struct join_req'; the IE portion is held inside a 'tSirRSNie': */

	/*     typedef struct sSirRSNie */
	/*     { */
	/*         uint16_t       length; */
	/*         uint8_t        rsnIEdata[WLAN_MAX_IE_LEN+2]; */
	/*     } tSirRSNie, *tpSirRSNie; */

	/* So, we should be able to make the following two calls harmlessly, */
	/* since they do nothing if they don't find the given IE in the */
	/* bytestream with which they're provided. */

	/* The net effect of this will be to faithfully transmit whatever */
	/* security IE is in the join request. */

	/**However*, if we're associating for the purpose of WPS */
	/* enrollment, and we've been configured to indicate that by */
	/* eliding the WPA or RSN IE, we just skip this: */
	if (nAddIELen && pAddIE)
		wpsIe = limGetWscIEPtr(mac, pAddIE, nAddIELen);
	if (!wpsIe) {
		populate_dot11f_rsn_opaque(mac,
				&(pe_session->pLimReAssocReq->rsnIE),
				&frm->RSNOpaque);
		populate_dot11f_wpa_opaque(mac,
				&(pe_session->pLimReAssocReq->rsnIE),
				&frm->WPAOpaque);
#if defined(FEATURE_WLAN_WAPI)
		populate_dot11f_wapi_opaque(mac,
					    &(pe_session->pLimReAssocReq->
					      rsnIE), &frm->WAPIOpaque);
#endif /* defined(FEATURE_WLAN_WAPI) */
	}
	/* include WME EDCA IE as well */
	if (fWmeEnabled) {
		populate_dot11f_wmm_info_station_per_session(mac,
				pe_session, &frm->WMMInfoStation);

		if (fWsmEnabled)
			populate_dot11f_wmm_caps(&frm->WMMCaps);
	}

	if (pe_session->htCapability &&
	    mac->lim.htCapabilityPresentInBeacon) {
		populate_dot11f_ht_caps(mac, pe_session, &frm->HTCaps);
	}
	if (pe_session->vhtCapability &&
	    pe_session->vhtCapabilityPresentInBeacon) {
		pe_warn("Populate VHT IEs in Re-Assoc Request");
		populate_dot11f_vht_caps(mac, pe_session, &frm->VHTCaps);
		isVHTEnabled = true;
	}
	populate_dot11f_ext_cap(mac, isVHTEnabled, &frm->ExtCap, pe_session);

	if (lim_is_session_he_capable(pe_session)) {
		pe_debug("Populate HE IEs");
		populate_dot11f_he_caps(mac, pe_session,
					&frm->he_cap);
		populate_dot11f_he_6ghz_cap(mac, pe_session,
					    &frm->he_6ghz_band_cap);
	}

	nStatus =
		dot11f_get_packed_re_assoc_request_size(mac, frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Fail to get size:ReassocReq: (0x%08x)", nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fReAssocRequest);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_err("warning for size:ReAssoc Req: (0x%08x)", nStatus);
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr) + nAddIELen;

	qdf_status = cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				      (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_session->limMlmState = pe_session->limPrevMlmState;
		MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_MLM_STATE,
				 pe_session->peSessionId,
				 pe_session->limMlmState));
		pe_err("Failed to alloc %d bytes for a ReAssociation Req",
			nBytes);
		goto end;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_REASSOC_REQ, pe_session->limReAssocbssId,
		pe_session->self_mac_addr);
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	/* That done, pack the Probe Request: */
	nStatus = dot11f_pack_re_assoc_request(mac, frm, pFrame +
					       sizeof(tSirMacMgmtHdr),
					       nPayload, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Fail to pack a Re-Assoc Req: (0x%08x)", nStatus);
		cds_packet_free((void *)pPacket);
		goto end;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("warning packing a Re-AssocReq: (0x%08x)", nStatus);
	}

	pe_debug("*** Sending Re-Association Request length: %d" "to", nBytes);

	if (pe_session->assoc_req) {
		qdf_mem_free(pe_session->assoc_req);
		pe_session->assoc_req = NULL;
		pe_session->assocReqLen = 0;
	}

	if (nAddIELen) {
		qdf_mem_copy(pFrame + sizeof(tSirMacMgmtHdr) + nPayload,
			     pAddIE, nAddIELen);
		nPayload += nAddIELen;
	}

	pe_session->assoc_req = qdf_mem_malloc(nPayload);
	if (pe_session->assoc_req) {
		/* Store the Assocrequest. It is sent to csr in joincnfrsp */
		qdf_mem_copy(pe_session->assoc_req,
			     pFrame + sizeof(tSirMacMgmtHdr), nPayload);
		pe_session->assocReqLen = nPayload;
	}

	if (wlan_reg_is_5ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	if (pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_STA_MODE)
		txFlag |= HAL_USE_PEER_STA_REQUESTED_MASK;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, pMacHdr->fc.subType));
	lim_diag_event_report(mac, WLAN_PE_DIAG_REASSOC_START_EVENT,
			      pe_session, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
	lim_diag_mgmt_tx_event_report(mac, pMacHdr,
				      pe_session, QDF_STATUS_SUCCESS,
				      QDF_STATUS_SUCCESS);
	qdf_status =
		wma_tx_frame(mac, pPacket,
			   (uint16_t) (sizeof(tSirMacMgmtHdr) + nPayload),
			   TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS, 7,
			   lim_tx_complete, pFrame, txFlag, smeSessionId, 0,
			   RATEID_DEFAULT, 0);
	MTRACE(qdf_trace
		       (QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
		       pe_session->peSessionId, qdf_status));
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to send Re-Association Request: %X!",
			qdf_status);
		/* Pkt will be freed up by the callback */
	}

end:
	qdf_mem_free(frm);
err:
	/* Free up buffer allocated for mlmAssocReq */
	qdf_mem_free(pMlmReassocReq);
	pe_session->pLimMlmReassocReq = NULL;

}

void lim_process_rx_scan_handler(struct wlan_objmgr_vdev *vdev,
				 struct scan_event *event,
				 void *arg)
{
	struct mac_context *mac_ctx;
	enum sir_scan_event_type event_type;

	QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
		  "event: %u, id: 0x%x, requestor: 0x%x, freq: %u, reason: %u",
		  event->type, event->scan_id, event->requester,
		  event->chan_freq, event->reason);

	mac_ctx = (struct mac_context *)arg;
	event_type = 0x1 << event->type;

	qdf_mtrace(QDF_MODULE_ID_SCAN, QDF_MODULE_ID_PE, event->type,
		   event->vdev_id, event->scan_id);

	switch (event_type) {
	case SIR_SCAN_EVENT_STARTED:
		break;
	case SIR_SCAN_EVENT_COMPLETED:
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  "No.of beacons and probe response received per scan %d",
			  mac_ctx->lim.beacon_probe_rsp_cnt_per_scan);
	/* Fall through */
	case SIR_SCAN_EVENT_FOREIGN_CHANNEL:
	case SIR_SCAN_EVENT_START_FAILED:
		if ((mac_ctx->lim.req_id | PREAUTH_REQUESTOR_ID) ==
		    event->requester)
			lim_preauth_scan_event_handler(mac_ctx,
						       event_type,
						       event->vdev_id,
						       event->scan_id);
		break;
	case SIR_SCAN_EVENT_BSS_CHANNEL:
	case SIR_SCAN_EVENT_DEQUEUED:
	case SIR_SCAN_EVENT_PREEMPTED:
	default:
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  "Received unhandled scan event %u",
			  event_type);
	}
}
