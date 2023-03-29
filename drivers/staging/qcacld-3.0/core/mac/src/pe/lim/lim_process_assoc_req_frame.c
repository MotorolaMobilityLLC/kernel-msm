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
 * This file lim_process_assoc_req_frame.c contains the code
 * for processing Re/Association Request Frame.
 */
#include "cds_api.h"
#include "ani_global.h"
#include "wni_cfg.h"
#include "sir_api.h"
#include "cfg_ucfg_api.h"

#include "sch_api.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_security_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_admit_control.h"
#include "cds_packet.h"
#include "lim_session_utils.h"
#include "utils_parser.h"
#include "wlan_p2p_api.h"

#include "qdf_types.h"
#include "cds_utils.h"
#include "wlan_utility.h"
#include "wlan_crypto_global_api.h"

/**
 * lim_convert_supported_channels - Parses channel support IE
 * @mac_ctx: A pointer to Global MAC structure
 * @assoc_ind: A pointer to SME ASSOC/REASSOC IND
 * @assoc_req: A pointer to ASSOC/REASSOC Request frame
 *
 * This function is called by lim_process_assoc_req_frame() to
 * parse the channel support IE in the Assoc/Reassoc Request
 * frame, and send relevant information in the SME_ASSOC_IND
 *
 * Return: None
 */
static void lim_convert_supported_channels(struct mac_context *mac_ctx,
					   tpLimMlmAssocInd assoc_ind,
					   tSirAssocReq *assoc_req)
{
	uint16_t i, j, index = 0;
	uint8_t first_ch_no;
	uint8_t chn_count;
	uint8_t next_ch_no;
	uint8_t channel_offset = 0;
	uint32_t chan_freq;

	if (assoc_req->supportedChannels.length >=
		SIR_MAX_SUPPORTED_CHANNEL_LIST) {
		pe_err("Number of supported channels: %d is more than MAX",
			assoc_req->supportedChannels.length);
		assoc_ind->supportedChannels.numChnl = 0;
		return;
	}

	for (i = 0; i < (assoc_req->supportedChannels.length); i++) {
		/* Get First Channel Number */
		first_ch_no = assoc_req->supportedChannels.supportedChannels[i];
		assoc_ind->supportedChannels.channelList[index] = first_ch_no;
		i++;
		index++;

		/* Get Number of Channels in a Subband */
		chn_count = assoc_req->supportedChannels.supportedChannels[i];
		pe_debug("Rcv assoc_req: chnl: %d numOfChnl: %d",
			first_ch_no, chn_count);
		if (index >= SIR_MAX_SUPPORTED_CHANNEL_LIST) {
			pe_warn("Ch count > max supported: %d", chn_count);
			assoc_ind->supportedChannels.numChnl = 0;
			return;
		}
		if (chn_count <= 1)
			continue;
		next_ch_no = first_ch_no;
		chan_freq = wlan_reg_legacy_chan_to_freq(mac_ctx->pdev,
			first_ch_no);

		if (REG_BAND_5G == lim_get_rf_band(chan_freq))
			channel_offset =  SIR_11A_FREQUENCY_OFFSET;
		else if (REG_BAND_2G == lim_get_rf_band(chan_freq))
			channel_offset = SIR_11B_FREQUENCY_OFFSET;
		else
			continue;

		for (j = 1; j < chn_count; j++) {
			next_ch_no += channel_offset;
			assoc_ind->supportedChannels.channelList[index]
				= next_ch_no;
			index++;
			if (index >= SIR_MAX_SUPPORTED_CHANNEL_LIST) {
				pe_warn("Ch count > supported: %d", chn_count);
				assoc_ind->supportedChannels.numChnl = 0;
				return;
			}
		}
	}

	assoc_ind->supportedChannels.numChnl = (uint8_t) index;
	pe_debug("Send AssocInd to WSM: minPwr: %d maxPwr: %d numChnl: %d",
		assoc_ind->powerCap.minTxPower,
		assoc_ind->powerCap.maxTxPower,
		assoc_ind->supportedChannels.numChnl);
}

/**
 * lim_check_sta_in_pe_entries() - checks if sta exists in any dph tables.
 * @mac_ctx: Pointer to Global MAC structure
 * @hdr: A pointer to the MAC header
 * @sessionid - session id for which session is initiated
 * @dup_entry: pointer for duplicate entry found
 *
 * This function is called by lim_process_assoc_req_frame() to check if STA
 * entry already exists in any of the PE entries of the AP. If it exists, deauth
 * will be sent on that session and the STA deletion will happen. After this,
 * the ASSOC request will be processed. If the STA is already in deleting phase
 * this will return failure so that assoc req will be rejected till STA is
 * deleted.
 *
 * Return: QDF_STATUS.
 */
static QDF_STATUS lim_check_sta_in_pe_entries(struct mac_context *mac_ctx,
					      tpSirMacMgmtHdr hdr,
					       uint16_t sessionid,
					       bool *dup_entry)
{
	uint8_t i;
	uint16_t assoc_id = 0;
	tpDphHashNode sta_ds = NULL;
	struct pe_session *session;

	*dup_entry = false;
	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		session = &mac_ctx->lim.gpSession[i];
		if (session->valid &&
		    (session->opmode == QDF_SAP_MODE)) {
			sta_ds = dph_lookup_hash_entry(mac_ctx, hdr->sa,
					&assoc_id, &session->dph.dphHashTable);
			if (sta_ds
#ifdef WLAN_FEATURE_11W
				&& (!sta_ds->rmfEnabled ||
				    (sessionid != session->peSessionId))
#endif
			    ) {
				if (sta_ds->mlmStaContext.mlmState ==
				    eLIM_MLM_WT_DEL_STA_RSP_STATE ||
				    sta_ds->mlmStaContext.mlmState ==
				    eLIM_MLM_WT_DEL_BSS_RSP_STATE ||
				    sta_ds->sta_deletion_in_progress) {
					pe_debug(
					"Deletion is in progress (%d) for peer:"QDF_MAC_ADDR_FMT" in mlmState %d",
					sta_ds->sta_deletion_in_progress,
					QDF_MAC_ADDR_REF(sta_ds->staAddr),
					sta_ds->mlmStaContext.mlmState);
					*dup_entry = true;
					return QDF_STATUS_E_AGAIN;
				}
				sta_ds->sta_deletion_in_progress = true;
				pe_err("Sending Disassoc and Deleting existing STA entry:"
				       QDF_MAC_ADDR_FMT,
				       QDF_MAC_ADDR_REF(
						session->self_mac_addr));
				lim_send_disassoc_mgmt_frame(mac_ctx,
					REASON_UNSPEC_FAILURE,
					(uint8_t *) hdr->sa, session, false);
				/*
				 * Cleanup Rx path posts eWNI_SME_DISASSOC_RSP
				 * msg to SME after delete sta which will update
				 * the userspace with disconnect
				 */
				sta_ds->mlmStaContext.cleanupTrigger =
							eLIM_DUPLICATE_ENTRY;
				sta_ds->mlmStaContext.disassocReason =
				REASON_DISASSOC_DUE_TO_INACTIVITY;
				lim_send_sme_disassoc_ind(mac_ctx, sta_ds,
					session);
				*dup_entry = true;
				break;
			}
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_chk_sa_da() - checks source addr to destination addr of assoc req frame
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 *
 * Checks source addr to destination addr of assoc req frame
 *
 * Return: true if source and destination address are different
 */
static bool lim_chk_sa_da(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			  struct pe_session *session, uint8_t sub_type)
{
	if (qdf_mem_cmp((uint8_t *) hdr->sa,
					(uint8_t *) hdr->da,
					(uint8_t) (sizeof(tSirMacAddr))))
		return true;

	pe_err("Assoc Req rejected: wlan.sa = wlan.da");
	lim_send_assoc_rsp_mgmt_frame(mac_ctx, STATUS_UNSPECIFIED_FAILURE,
				      1, hdr->sa, sub_type, 0, session, false);
	return false;
}

/**
 * lim_chk_assoc_req_parse_error() - checks for error in frame parsing
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 * @frm_body: frame body
 * @frame_len: frame len
 *
 * Checks for error in frame parsing
 *
 * Return: true of no error, false otherwise
 */
static bool lim_chk_assoc_req_parse_error(struct mac_context *mac_ctx,
					  tpSirMacMgmtHdr hdr,
					  struct pe_session *session,
					  tpSirAssocReq assoc_req,
					  uint8_t sub_type, uint8_t *frm_body,
					  uint32_t frame_len)
{
	QDF_STATUS status;

	if (sub_type == LIM_ASSOC)
		status = sir_convert_assoc_req_frame2_struct(mac_ctx, frm_body,
							     frame_len,
							     assoc_req);
	else
		status = sir_convert_reassoc_req_frame2_struct(mac_ctx,
						frm_body, frame_len, assoc_req);

	if (status == QDF_STATUS_SUCCESS)
		return true;

	pe_warn("Assoc Req rejected: frame parsing error. source addr:"
			QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(hdr->sa));
	lim_send_assoc_rsp_mgmt_frame(mac_ctx, STATUS_UNSPECIFIED_FAILURE,
				      1, hdr->sa, sub_type, 0, session, false);
	return false;
}

/**
 * lim_chk_capab() - checks for capab match
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 * @local_cap: local capabilities of SAP
 *
 * Checks for capab match
 *
 * Return: true of no error, false otherwise
 */
static bool lim_chk_capab(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			  struct pe_session *session, tpSirAssocReq assoc_req,
			  uint8_t sub_type, tSirMacCapabilityInfo *local_cap)
{
	uint16_t temp;

	if (lim_get_capability_info(mac_ctx, &temp, session) !=
	    QDF_STATUS_SUCCESS) {
		pe_err("could not retrieve Capabilities");
		return false;
	}

	lim_copy_u16((uint8_t *) local_cap, temp);

	if (lim_compare_capabilities(mac_ctx, assoc_req,
				     local_cap, session) == false) {
		pe_warn("Rcvd %s Req with unsupported capab from"
				QDF_MAC_ADDR_FMT,
			(LIM_ASSOC == sub_type) ? "Assoc" : "ReAssoc",
			QDF_MAC_ADDR_REF(hdr->sa));
		/*
		 * Capabilities of requesting STA does not match with
		 * local capabilities. Respond with 'unsupported capabilities'
		 * status code.
		 */
		lim_send_assoc_rsp_mgmt_frame(
			mac_ctx, STATUS_CAPS_UNSUPPORTED,
			1, hdr->sa, sub_type, 0, session, false);
		return false;
	}
	return true;
}

/**
 * lim_chk_ssid() - checks for SSID match
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 *
 * Checks for SSID match
 *
 * Return: true of no error, false otherwise
 */
static bool lim_chk_ssid(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			 struct pe_session *session, tpSirAssocReq assoc_req,
			 uint8_t sub_type)
{
	if (!lim_cmp_ssid(&assoc_req->ssId, session))
		return true;

	pe_err("%s Req with ssid wrong(Rcvd: %.*s self: %.*s) from "
			QDF_MAC_ADDR_FMT,
		(LIM_ASSOC == sub_type) ? "Assoc" : "ReAssoc",
		assoc_req->ssId.length, assoc_req->ssId.ssId,
		session->ssId.length, session->ssId.ssId,
		QDF_MAC_ADDR_REF(hdr->sa));

	/*
	 * Received Re/Association Request with either Broadcast SSID OR with
	 * SSID that does not match with local one. Respond with unspecified
	 * status code.
	 */
	lim_send_assoc_rsp_mgmt_frame(mac_ctx, STATUS_UNSPECIFIED_FAILURE,
				      1, hdr->sa, sub_type, 0, session, false);
	return false;
}

/**
 * lim_chk_rates() - checks for supported rates
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 *
 * Checks for supported rates
 *
 * Return: true of no error, false otherwise
 */
static bool lim_chk_rates(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			  struct pe_session *session, tpSirAssocReq assoc_req,
			  uint8_t sub_type)
{
	uint8_t i = 0, j = 0;
	tSirMacRateSet basic_rates;
	/*
	 * Verify if the requested rates are available in supported rate
	 * set or Extended rate set. Some APs are adding basic rates in
	 * Extended rateset IE
	 */
	basic_rates.numRates = 0;

	for (i = 0; i < assoc_req->supportedRates.numRates
			&& (i < WLAN_SUPPORTED_RATES_IE_MAX_LEN); i++) {
		basic_rates.rate[i] = assoc_req->supportedRates.rate[i];
		basic_rates.numRates++;
	}

	for (j = 0; (j < assoc_req->extendedRates.numRates)
			&& (i < WLAN_SUPPORTED_RATES_IE_MAX_LEN); i++, j++) {
		basic_rates.rate[i] = assoc_req->extendedRates.rate[j];
		basic_rates.numRates++;
	}

	if (lim_check_rx_basic_rates(mac_ctx, basic_rates, session) == true)
		return true;

	pe_warn("Assoc Req rejected: unsupported rates, soruce addr: %s"
			QDF_MAC_ADDR_FMT,
		(LIM_ASSOC == sub_type) ? "Assoc" : "ReAssoc",
		QDF_MAC_ADDR_REF(hdr->sa));
	/*
	 * Requesting STA does not support ALL BSS basic rates. Respond with
	 * 'basic rates not supported' status code.
	 */
	lim_send_assoc_rsp_mgmt_frame(
			mac_ctx, STATUS_ASSOC_DENIED_RATES, 1,
			hdr->sa, sub_type, 0, session, false);
	return false;
}

/**
 * lim_chk_11g_only() - checks for non 11g STA
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 *
 * Checks for non 11g STA
 *
 * Return: true of no error, false otherwise
 */
static bool lim_chk_11g_only(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			     struct pe_session *session, tpSirAssocReq assoc_req,
			     uint8_t sub_type)
{
	if (LIM_IS_AP_ROLE(session) &&
	    (session->dot11mode == MLME_DOT11_MODE_11G_ONLY) &&
	    (assoc_req->HTCaps.present)) {
		pe_err("SOFTAP was in 11G only mode, rejecting legacy STA: "
				QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(hdr->sa));
		lim_send_assoc_rsp_mgmt_frame(
			mac_ctx, STATUS_CAPS_UNSUPPORTED,
			1, hdr->sa, sub_type, 0, session, false);
		return false;
	}
	return true;
}

/**
 * lim_chk_11n_only() - checks for non 11n STA
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 *
 * Checks for non 11n STA
 *
 * Return: true of no error, false otherwise
 */
static bool lim_chk_11n_only(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			     struct pe_session *session, tpSirAssocReq assoc_req,
			     uint8_t sub_type)
{
	if (LIM_IS_AP_ROLE(session) &&
	    (session->dot11mode == MLME_DOT11_MODE_11N_ONLY) &&
	    (!assoc_req->HTCaps.present)) {
		pe_err("SOFTAP was in 11N only mode, rejecting legacy STA: "
				QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(hdr->sa));
		lim_send_assoc_rsp_mgmt_frame(
			mac_ctx, STATUS_CAPS_UNSUPPORTED,
			1, hdr->sa, sub_type, 0, session, false);
		return false;
	}
	return true;
}

/**
 * lim_chk_11ac_only() - checks for non 11ac STA
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 *
 * Checks for non 11ac STA
 *
 * Return: true of no error, false otherwise
 */
static bool lim_chk_11ac_only(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			      struct pe_session *session, tpSirAssocReq assoc_req,
			      uint8_t sub_type)
{
	tDot11fIEVHTCaps *vht_caps;

	if (assoc_req->VHTCaps.present)
		vht_caps = &assoc_req->VHTCaps;
	else if (assoc_req->vendor_vht_ie.VHTCaps.present &&
		 session->vendor_vht_sap)
		vht_caps = &assoc_req->vendor_vht_ie.VHTCaps;
	else
		vht_caps = NULL;

	if (LIM_IS_AP_ROLE(session) &&
		(session->dot11mode == MLME_DOT11_MODE_11AC_ONLY) &&
		((!vht_caps) || ((vht_caps) && (!vht_caps->present)))) {
		lim_send_assoc_rsp_mgmt_frame(
			mac_ctx, STATUS_CAPS_UNSUPPORTED,
			1, hdr->sa, sub_type, 0, session, false);
		pe_err("SOFTAP was in 11AC only mode, reject");
		return false;
	}
	return true;
}

/**
 * lim_chk_11ax_only() - checks for non 11ax STA
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 *
 * Checks for non 11ax STA
 *
 * Return: true of no error, false otherwise
 */
#ifdef WLAN_FEATURE_11AX
static bool lim_chk_11ax_only(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			      struct pe_session *session, tpSirAssocReq assoc_req,
			      uint8_t sub_type)
{
	if (LIM_IS_AP_ROLE(session) &&
		(session->dot11mode == MLME_DOT11_MODE_11AX_ONLY) &&
		 !assoc_req->he_cap.present) {
		lim_send_assoc_rsp_mgmt_frame(
			mac_ctx, STATUS_CAPS_UNSUPPORTED,
			1, hdr->sa, sub_type, 0, session, false);
		pe_err("SOFTAP was in 11AX only mode, reject");
		return false;
	}
	return true;
}

/**
 * lim_check_11ax_basic_mcs() - checks for 11ax basic MCS rates
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 *
 * Checks for non 11ax STA
 *
 * Return: true of no error, false otherwise
 */
static bool lim_check_11ax_basic_mcs(struct mac_context *mac_ctx,
				     tpSirMacMgmtHdr hdr,
				     struct pe_session *session,
				     tpSirAssocReq assoc_req,
				     uint8_t sub_type)
{
	uint16_t basic_mcs, sta_mcs, rx_mcs, tx_mcs, final_mcs;

	if (LIM_IS_AP_ROLE(session) &&
	    assoc_req->he_cap.present) {
		rx_mcs = assoc_req->he_cap.rx_he_mcs_map_lt_80;
		tx_mcs = assoc_req->he_cap.tx_he_mcs_map_lt_80;
		sta_mcs = HE_INTERSECT_MCS(rx_mcs, tx_mcs);
		basic_mcs =
		(uint16_t)mac_ctx->mlme_cfg->he_caps.he_ops_basic_mcs_nss;
		final_mcs = HE_INTERSECT_MCS(sta_mcs, basic_mcs);
		if (final_mcs != basic_mcs) {
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx,
				STATUS_CAPS_UNSUPPORTED,
				1, hdr->sa, sub_type, 0, session, false);
			pe_err("STA did not support basic MCS required by SAP");
			return false;
		}
	}
	return true;
}

#else
static bool lim_chk_11ax_only(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			      struct pe_session *session, tpSirAssocReq assoc_req,
			      uint8_t sub_type)
{
	return true;
}

static bool lim_check_11ax_basic_mcs(struct mac_context *mac_ctx,
				     tpSirMacMgmtHdr hdr,
				     struct pe_session *session,
				     tpSirAssocReq assoc_req,
				     uint8_t sub_type)
{
	return true;
}
#endif

/**
 * lim_process_for_spectrum_mgmt() - process assoc req for spectrum mgmt
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 * @local_cap: local capabilities of SAP
 *
 * Checks for SSID match
 *
 * process assoc req for spectrum mgmt
 */
static void
lim_process_for_spectrum_mgmt(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			      struct pe_session *session, tpSirAssocReq assoc_req,
			      uint8_t sub_type, tSirMacCapabilityInfo local_cap)
{
	if (local_cap.spectrumMgt) {
		QDF_STATUS status = QDF_STATUS_SUCCESS;
		/*
		 * If station is 11h capable, then it SHOULD send all mandatory
		 * IEs in assoc request frame. Let us verify that
		 */
		if (assoc_req->capabilityInfo.spectrumMgt) {
			if (!((assoc_req->powerCapabilityPresent)
			     && (assoc_req->supportedChannelsPresent))) {
				/*
				 * One or more required information elements are
				 * missing, log the peers error
				 */
				if (!assoc_req->powerCapabilityPresent) {
					pe_warn("LIM Info: Missing Power capability IE in %s Req from "
							QDF_MAC_ADDR_FMT,
						(LIM_ASSOC == sub_type) ?
							"Assoc" : "ReAssoc",
						QDF_MAC_ADDR_REF(hdr->sa));
				}
				if (!assoc_req->supportedChannelsPresent) {
					pe_warn("LIM Info: Missing Supported channel IE in %s Req from "
							QDF_MAC_ADDR_FMT,
						(LIM_ASSOC == sub_type) ?
							"Assoc" : "ReAssoc",
						QDF_MAC_ADDR_REF(hdr->sa));
				}
			} else {
				/* Assoc request has mandatory fields */
				status =
				    lim_is_dot11h_power_capabilities_in_range(
					mac_ctx, assoc_req, session);
				if (QDF_STATUS_SUCCESS != status) {
					pe_warn("LIM Info: MinTxPower(STA) > MaxTxPower(AP) in %s Req from "
						QDF_MAC_ADDR_FMT,
						(LIM_ASSOC == sub_type) ?
							"Assoc" : "ReAssoc",
						QDF_MAC_ADDR_REF(hdr->sa));
				}
				status = lim_is_dot11h_supported_channels_valid(
							mac_ctx, assoc_req);
				if (QDF_STATUS_SUCCESS != status) {
					pe_warn("LIM Info: wrong supported channels (STA) in %s Req from "
						QDF_MAC_ADDR_FMT,
						(LIM_ASSOC == sub_type) ?
							"Assoc" : "ReAssoc",
						QDF_MAC_ADDR_REF(hdr->sa));
				}
				/* IEs are valid, use them if needed */
			}
		} /* if(assoc.capabilityInfo.spectrumMgt) */
		else {
			/*
			 * As per the capabiities, the spectrum management is
			 * not enabled on the station. The AP may allow the
			 * associations to happen even if spectrum management
			 * is not allowed, if the transmit power of station is
			 * below the regulatory maximum
			 */

			/*
			 * TODO: presently, this is not handled. In the current
			 * implementation, the AP would allow the station to
			 * associate even if it doesn't support spectrum
			 * management.
			 */
		}
	} /* end of spectrum management related processing */
}

/**
 * lim_chk_mcs() - checks for supported MCS
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 *
 * Checks for supported MCS
 *
 * Return: true of no error, false otherwise
 */
static bool lim_chk_mcs(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			struct pe_session *session, tpSirAssocReq assoc_req,
			uint8_t sub_type)
{
	if ((assoc_req->HTCaps.present) && (lim_check_mcs_set(mac_ctx,
			assoc_req->HTCaps.supportedMCSSet) == false)) {
		pe_warn("rcvd %s req with unsupported MCS Rate Set from "
				QDF_MAC_ADDR_FMT,
			(LIM_ASSOC == sub_type) ? "Assoc" : "ReAssoc",
			QDF_MAC_ADDR_REF(hdr->sa));
		/*
		 * Requesting STA does not support ALL BSS MCS basic Rate set
		 * rates. Spec does not define any status code for this
		 * scenario.
		 */
		lim_send_assoc_rsp_mgmt_frame(
			mac_ctx, STATUS_ASSOC_DENIED_UNSPEC,
			1, hdr->sa, sub_type, 0, session, false);
		return false;
	}
	return true;
}

/**
 * lim_chk_is_11b_sta_supported() - checks if STA is 11b
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 * @phy_mode: phy mode
 *
 * Checks if STA is 11b
 *
 * Return: true of no error, false otherwise
 */
static bool lim_chk_is_11b_sta_supported(struct mac_context *mac_ctx,
					 tpSirMacMgmtHdr hdr,
					 struct pe_session *session,
					 tpSirAssocReq assoc_req,
					 uint8_t sub_type, uint32_t phy_mode)
{
	uint32_t cfg_11g_only;

	if (phy_mode == WNI_CFG_PHY_MODE_11G) {
		cfg_11g_only = mac_ctx->mlme_cfg->sap_cfg.sap_11g_policy;
		if (!assoc_req->extendedRatesPresent && cfg_11g_only) {
			/*
			 * Received Re/Association Request from 11b STA when 11g
			 * only policy option is set. Reject with unspecified
			 * status code.
			 */
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx,
				STATUS_ASSOC_DENIED_RATES,
				1, hdr->sa, sub_type, 0, session, false);

			pe_warn("Rejecting Re/Assoc req from 11b STA:");
			lim_print_mac_addr(mac_ctx, hdr->sa, LOGW);

#ifdef WLAN_DEBUG
			mac_ctx->lim.gLim11bStaAssocRejectCount++;
#endif
			return false;
		}
	}
	return true;
}

/**
 * lim_print_ht_cap() - prints HT caps
 * @mac_ctx: pointer to Global MAC structure
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 *
 * Prints HT caps
 *
 * Return: void
 */
static void lim_print_ht_cap(struct mac_context *mac_ctx, struct pe_session *session,
			     tpSirAssocReq assoc_req)
{
	if (!session->htCapability)
		return;

	if (assoc_req->HTCaps.present) {
		/* The station *does* support 802.11n HT capability... */
		pe_debug("AdvCodingCap:%d ChaWidthSet:%d PowerSave:%d greenField:%d shortGI20:%d shortGI40:%d txSTBC:%d rxSTBC:%d delayBA:%d maxAMSDUsize:%d DSSS/CCK:%d  PSMP:%d stbcCntl:%d lsigTXProt:%d",
			assoc_req->HTCaps.advCodingCap,
			assoc_req->HTCaps.supportedChannelWidthSet,
			assoc_req->HTCaps.mimoPowerSave,
			assoc_req->HTCaps.greenField,
			assoc_req->HTCaps.shortGI20MHz,
			assoc_req->HTCaps.shortGI40MHz,
			assoc_req->HTCaps.txSTBC,
			assoc_req->HTCaps.rxSTBC,
			assoc_req->HTCaps.delayedBA,
			assoc_req->HTCaps.maximalAMSDUsize,
			assoc_req->HTCaps.dsssCckMode40MHz,
			assoc_req->HTCaps.psmp,
			assoc_req->HTCaps.stbcControlFrame,
			assoc_req->HTCaps.lsigTXOPProtection);
		/*
		 * Make sure the STA's caps are compatible with our own:
		 * 11.15.2 Support of DSSS/CCK in 40 MHz the AP shall refuse
		 * association requests from an HT STA that has the DSSS/CCK
		 * Mode in 40 MHz subfield set to 1;
		 */
	}
}

static enum wlan_status_code
lim_check_crypto_param(tpSirAssocReq assoc_req,
		       struct wlan_crypto_params *peer_crypto_params)
{
	/* TKIP/WEP is not allowed in HT/VHT mode*/
	if (assoc_req->HTCaps.present) {
		if ((peer_crypto_params->ucastcipherset &
			(1 << WLAN_CRYPTO_CIPHER_TKIP)) ||
		    (peer_crypto_params->ucastcipherset &
			(1 << WLAN_CRYPTO_CIPHER_WEP))) {
			pe_info("TKIP/WEP cipher with HT supported client, reject assoc");
			return STATUS_INVALID_IE;
		}
	}
	return STATUS_SUCCESS;
}

static
enum wlan_status_code lim_check_rsn_ie(struct pe_session *session,
				      struct mac_context *mac_ctx,
				      tpSirAssocReq assoc_req,
				      bool *pmf_connection)
{
	struct wlan_objmgr_vdev *vdev;
	tSirMacRsnInfo rsn_ie;
	struct wlan_crypto_params peer_crypto_params;

	rsn_ie.info[0] = WLAN_ELEMID_RSN;
	rsn_ie.info[1] = assoc_req->rsn.length;

	rsn_ie.length = assoc_req->rsn.length + 2;
	qdf_mem_copy(&rsn_ie.info[2], assoc_req->rsn.info,
		     assoc_req->rsn.length);
	if (wlan_crypto_check_rsn_match(mac_ctx->psoc, session->smeSessionId,
					&rsn_ie.info[0], rsn_ie.length,
					&peer_crypto_params)) {
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							session->smeSessionId,
							WLAN_LEGACY_MAC_ID);
		if (!vdev) {
			pe_err("vdev is NULL");
			return STATUS_UNSPECIFIED_FAILURE;
		}
		if ((peer_crypto_params.rsn_caps &
		    WLAN_CRYPTO_RSN_CAP_MFP_ENABLED) &&
		    wlan_crypto_vdev_is_pmf_enabled(vdev))
			*pmf_connection = true;

		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
		return lim_check_crypto_param(assoc_req, &peer_crypto_params);

	} else {
		return STATUS_INVALID_IE;
	}

	return STATUS_SUCCESS;
}

static enum wlan_status_code lim_check_wpa_ie(struct pe_session *session,
					     struct mac_context *mac_ctx,
					     tpSirAssocReq assoc_req,
					     tDot11fIEWPA *wpa)
{
	uint8_t buffer[WLAN_MAX_IE_LEN];
	uint32_t dot11f_status, written = 0, nbuffer = WLAN_MAX_IE_LEN;
	tSirMacRsnInfo wpa_ie = {0};
	struct wlan_crypto_params peer_crypto_params;

	dot11f_status = dot11f_pack_ie_wpa(mac_ctx, wpa, buffer,
					   nbuffer, &written);
	if (DOT11F_FAILED(dot11f_status)) {
		pe_err("Failed to re-pack the RSN IE (0x%0x8)", dot11f_status);
		return STATUS_INVALID_IE;
	}

	wpa_ie.length = (uint8_t) written;
	qdf_mem_copy(&wpa_ie.info[0], buffer, wpa_ie.length);
	if (wlan_crypto_check_wpa_match(mac_ctx->psoc, session->smeSessionId,
					&wpa_ie.info[0], wpa_ie.length,
					&peer_crypto_params)) {
		return lim_check_crypto_param(assoc_req, &peer_crypto_params);
	}

	return STATUS_INVALID_IE;
}

/**
  * lim_check_sae_pmf_cap() - check pmf capability for SAE STA
  * @session: pointer to pe session entry
  * @rsn: pointer to RSN
  * @akm_type: AKM type
  *
  * This function checks if SAE STA is pmf capable when SAE SAP is pmf
  * capable. Reject with eSIR_MAC_ROBUST_MGMT_FRAMES_POLICY_VIOLATION
  * if SAE STA is pmf disable.
  *
  * Return: wlan_status_code
  */
#if defined(WLAN_FEATURE_SAE) && defined(WLAN_FEATURE_11W)
static enum wlan_status_code lim_check_sae_pmf_cap(struct pe_session *session,
						  tDot11fIERSN *rsn,
						  enum ani_akm_type akm_type)
{
	enum wlan_status_code status = STATUS_SUCCESS;

	if (session->pLimStartBssReq->pmfCapable &&
	    (rsn->RSN_Cap[0] & WLAN_CRYPTO_RSN_CAP_MFP_ENABLED) == 0 &&
	    akm_type == ANI_AKM_TYPE_SAE)
		status = STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION;

	return status;
}
#else
static enum wlan_status_code lim_check_sae_pmf_cap(struct pe_session *session,
						  tDot11fIERSN *rsn,
						  enum ani_akm_type akm_type)
{
	return STATUS_SUCCESS;
}
#endif

/**
  * lim_check_wpa_rsn_ie() - wpa and rsn ie related checks
  * @session: pointer to pe session entry
  * @mac_ctx: pointer to Global MAC structure
  * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
  * @hdr: pointer to the MAC head
  * @assoc_req: pointer to ASSOC/REASSOC Request frame
  * @pmf_connection: flag indicating pmf connection
  * @akm_type: AKM type
  *
  * This function checks if wpa/rsn IE is present and validates
  * ie version, length and mismatch.
  *
  * Return: true if no error, false otherwise
  */
static bool lim_check_wpa_rsn_ie(struct pe_session *session,
				 struct mac_context *mac_ctx,
				 uint8_t sub_type, tpSirMacMgmtHdr hdr,
				 tpSirAssocReq assoc_req, bool *pmf_connection,
				 enum ani_akm_type *akm_type)
{
	uint32_t ret;
	tDot11fIEWPA dot11f_ie_wpa = {0};
	tDot11fIERSN dot11f_ie_rsn = {0};
	enum wlan_status_code status = STATUS_SUCCESS;

	/*
	 * Clear the buffers so that frame parser knows that there isn't a
	 * previously decoded IE in these buffers
	 */
	qdf_mem_zero((uint8_t *) &dot11f_ie_rsn, sizeof(dot11f_ie_rsn));
	qdf_mem_zero((uint8_t *) &dot11f_ie_wpa, sizeof(dot11f_ie_wpa));
	pe_debug("RSN enabled auth, Re/Assoc req from STA: "
		 QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(hdr->sa));

	if (assoc_req->rsnPresent) {
		if (!(assoc_req->rsn.length)) {
			pe_warn("Re/Assoc rejected from: "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(hdr->sa));
			/*
			 * rcvd Assoc req frame with RSN IE but
			 * length is zero
			 */
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx, STATUS_INVALID_IE, 1,
				hdr->sa, sub_type, 0, session, false);
			return false;
		}

		/* Unpack the RSN IE */
		ret = dot11f_unpack_ie_rsn(mac_ctx,
					   &assoc_req->rsn.info[0],
					   assoc_req->rsn.length,
					   &dot11f_ie_rsn, false);
		if (!DOT11F_SUCCEEDED(ret)) {
			pe_err("Invalid RSN IE");
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx, STATUS_INVALID_IE, 1,
				hdr->sa, sub_type, 0, session, false);
			return false;
		}

		/* Check if the RSN version is supported */
		if (SIR_MAC_OUI_VERSION_1 == dot11f_ie_rsn.version) {
			/* check the groupwise and pairwise cipher suites */
			status = lim_check_rsn_ie(session, mac_ctx, assoc_req,
						  pmf_connection);
			if (status != STATUS_SUCCESS) {
				pe_warn("Re/Assoc rejected from: "
					QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(hdr->sa));

				lim_send_assoc_rsp_mgmt_frame(
					mac_ctx, status, 1, hdr->sa, sub_type,
					0, session, false);
				return false;
			}
		} else {
			pe_warn("Re/Assoc rejected from: " QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(hdr->sa));
			/*
			 * rcvd Assoc req frame with RSN IE but
			 * IE version is wrong
			 */
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx,
				STATUS_UNSUPPORTED_RSN_IE_VERSION,
				1, hdr->sa, sub_type, 0, session, false);
			return false;
		}
		*akm_type = lim_translate_rsn_oui_to_akm_type(
						    dot11f_ie_rsn.akm_suite[0]);

		status = lim_check_sae_pmf_cap(session, &dot11f_ie_rsn,
					       *akm_type);
		if (status != STATUS_SUCCESS) {
			/* Reject pmf disable SAE STA */
			pe_warn("Re/Assoc rejected from: " QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(hdr->sa));
			lim_send_assoc_rsp_mgmt_frame(mac_ctx, status,
						      1, hdr->sa, sub_type,
						      0, session, false);
			return false;
		}

	} else if (assoc_req->wpaPresent) {
		if (!(assoc_req->wpa.length)) {
			pe_warn("Re/Assoc rejected from: "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(hdr->sa));

			/* rcvd Assoc req frame with invalid WPA IE length */
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx, STATUS_INVALID_IE, 1,
				hdr->sa, sub_type, 0, session, false);
			return false;
		}
		/* Unpack the WPA IE */
		ret = dot11f_unpack_ie_wpa(mac_ctx,
					   &assoc_req->wpa.info[4],
					   (assoc_req->wpa.length - 4),
					   &dot11f_ie_wpa, false);
		if (!DOT11F_SUCCEEDED(ret)) {
			pe_err("Invalid WPA IE");
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx, STATUS_INVALID_IE, 1,
				hdr->sa, sub_type, 0, session, false);
			return false;
		}

		/* check the groupwise and pairwise cipher suites*/
		status = lim_check_wpa_ie(session, mac_ctx, assoc_req,
					  &dot11f_ie_wpa);
		if (status != STATUS_SUCCESS) {
			pe_warn("Re/Assoc rejected from: "
				QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(hdr->sa));
			/*
			 * rcvd Assoc req frame with WPA IE
			 * but there is mismatch
			 */
			lim_send_assoc_rsp_mgmt_frame(
					mac_ctx, status, 1,
					hdr->sa, sub_type, 0, session, false);
			return false;
		}
		*akm_type = lim_translate_rsn_oui_to_akm_type(
						  dot11f_ie_wpa.auth_suites[0]);
	}

	return true;
}

/**
 * lim_chk_n_process_wpa_rsn_ie() - wpa ie related checks
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 * @pmf_connection: flag indicating pmf connection
 * @akm_type: AKM type
 *
 * wpa ie related checks
 *
 * Return: true if no error, false otherwise
 */
static bool lim_chk_n_process_wpa_rsn_ie(struct mac_context *mac_ctx,
					 tpSirMacMgmtHdr hdr,
					 struct pe_session *session,
					 tpSirAssocReq assoc_req,
					 uint8_t sub_type,
					 bool *pmf_connection,
					 enum ani_akm_type *akm_type)
{
	const uint8_t *wps_ie = NULL;

	/* if additional IE is present, check if it has WscIE */
	if (assoc_req->addIEPresent && assoc_req->addIE.length)
		wps_ie = limGetWscIEPtr(mac_ctx, assoc_req->addIE.addIEdata,
					assoc_req->addIE.length);
	else
		pe_debug("Assoc req addIEPresent: %d addIE length: %d",
			 assoc_req->addIEPresent, assoc_req->addIE.length);

	/* when wps_ie is present, RSN/WPA IE is ignored */
	if (!wps_ie) {
		/* check whether RSN IE is present */
		if (LIM_IS_AP_ROLE(session) &&
		    session->pLimStartBssReq->privacy &&
		    session->pLimStartBssReq->rsnIE.length)
			return lim_check_wpa_rsn_ie(session, mac_ctx, sub_type,
						 hdr, assoc_req, pmf_connection,
						 akm_type);
	} else {
		pe_debug("Assoc req WSE IE is present");
	}
	return true;
}

/**
 * lim_process_assoc_req_no_sta_ctx() - process assoc req for no sta ctx present
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 * @sta_pre_auth_ctx: sta pre auth context
 * @sta_ds: station dph entry
 * @auth_type: indicates security type
 *
 * Process assoc req for no sta ctx present
 *
 * Return: true of no error, false otherwise
 */
static bool lim_process_assoc_req_no_sta_ctx(struct mac_context *mac_ctx,
				tpSirMacMgmtHdr hdr, struct pe_session *session,
				tpSirAssocReq assoc_req, uint8_t sub_type,
				struct tLimPreAuthNode *sta_pre_auth_ctx,
				tpDphHashNode sta_ds, tAniAuthType *auth_type)
{
	/* Requesting STA is not currently associated */
	if (pe_get_current_stas_count(mac_ctx) ==
			mac_ctx->mlme_cfg->sap_cfg.assoc_sta_limit) {
		/*
		 * Maximum number of STAs that AP can handle reached.
		 * Send Association response to peer MAC entity
		 */
		pe_err("Max Sta count reached : %d",
				mac_ctx->lim.maxStation);
		lim_reject_association(mac_ctx, hdr->sa, sub_type, false,
			(tAniAuthType) 0, 0, false,
			STATUS_UNSPECIFIED_FAILURE,
			session);
		return false;
	}
	/* Check if STA is pre-authenticated. */
	if ((!sta_pre_auth_ctx) || (sta_pre_auth_ctx &&
		(sta_pre_auth_ctx->mlmState != eLIM_MLM_AUTHENTICATED_STATE))) {
		/*
		 * STA is not pre-authenticated yet requesting Re/Association
		 * before Authentication. OR STA is in the process of getting
		 * authenticated and sent Re/Association request. Send
		 * Deauthentication frame with 'prior authentication required'
		 * reason code.
		 */
		lim_send_deauth_mgmt_frame(mac_ctx,
				REASON_STA_NOT_AUTHENTICATED,
				hdr->sa, session, false);

		pe_warn("rcvd %s req, sessionid: %d, without pre-auth ctx"
				QDF_MAC_ADDR_FMT,
			(LIM_ASSOC == sub_type) ? "Assoc" : "ReAssoc",
			session->peSessionId, QDF_MAC_ADDR_REF(hdr->sa));
		return false;
	}
	/* Delete 'pre-auth' context of STA */
	*auth_type = sta_pre_auth_ctx->authType;
	if (sta_pre_auth_ctx->authType == eSIR_AUTH_TYPE_SAE)
		assoc_req->is_sae_authenticated = true;
	lim_delete_pre_auth_node(mac_ctx, hdr->sa);
	/* All is well. Assign AID (after else part) */
	return true;
}

#ifdef WLAN_DEBUG
static inline void
lim_update_assoc_drop_count(struct mac_context *mac_ctx, uint8_t sub_type)
{
	if (sub_type == LIM_ASSOC)
		mac_ctx->lim.gLimNumAssocReqDropInvldState++;
	else
		mac_ctx->lim.gLimNumReassocReqDropInvldState++;
}
#else
static inline void
lim_update_assoc_drop_count(struct mac_context *mac_ctx, uint8_t sub_type) {}
#endif

#ifdef WLAN_FEATURE_11W
static inline void
lim_delete_pmf_query_timer(tpDphHashNode sta_ds)
{
	if (!sta_ds->rmfEnabled)
		return;

	if (tx_timer_running(&sta_ds->pmfSaQueryTimer))
		tx_timer_deactivate(&sta_ds->pmfSaQueryTimer);
	tx_timer_delete(&sta_ds->pmfSaQueryTimer);
}
#else
static inline void
lim_delete_pmf_query_timer(tpDphHashNode sta_ds) {}
#endif

/**
 * lim_process_assoc_req_sta_ctx() - process assoc req for sta context present
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 * @sta_pre_auth_ctx: sta pre auth context
 * @sta_ds: station dph entry
 * @peer_idx: peer index
 * @auth_type: indicates security type
 * @update_ctx: indicates if STA context already exist
 *
 * Process assoc req for sta context present
 *
 * Return: true of no error, false otherwise
 */
static bool lim_process_assoc_req_sta_ctx(struct mac_context *mac_ctx,
				tpSirMacMgmtHdr hdr, struct pe_session *session,
				tpSirAssocReq assoc_req, uint8_t sub_type,
				struct tLimPreAuthNode *sta_pre_auth_ctx,
				tpDphHashNode sta_ds, uint16_t peer_idx,
				tAniAuthType *auth_type, uint8_t *update_ctx)
{
	/* Drop if STA deletion is in progress or not in established state */
	if (sta_ds->sta_deletion_in_progress ||
	    (sta_ds->mlmStaContext.mlmState !=
	     eLIM_MLM_LINK_ESTABLISHED_STATE)) {
		pe_debug("%s: peer:"QDF_MAC_ADDR_FMT" in mlmState %d (%s) and sta del %d",
			 (sub_type == LIM_ASSOC) ? "Assoc" : "ReAssoc",
			 QDF_MAC_ADDR_REF(sta_ds->staAddr),
			 sta_ds->mlmStaContext.mlmState,
			 lim_mlm_state_str(sta_ds->mlmStaContext.mlmState),
			 sta_ds->sta_deletion_in_progress);
		lim_update_assoc_drop_count(mac_ctx, sub_type);
		return false;
	}

	/* STA sent assoc req frame while already in 'associated' state */

#ifdef WLAN_FEATURE_11W
	pe_debug("Re/Assoc request from station that is already associated");
	pe_debug("PMF enabled: %d, SA Query state: %d",
		sta_ds->rmfEnabled, sta_ds->pmfSaQueryState);
	if (sta_ds->rmfEnabled) {
		switch (sta_ds->pmfSaQueryState) {
		/*
		 * start SA Query procedure, respond to Association Request with
		 * try again later
		 */
		case DPH_SA_QUERY_NOT_IN_PROGRESS:
			/*
			 * We should reset the retry counter before we start
			 * the SA query procedure, otherwise in next set of SA
			 * query procedure we will end up using the stale value.
			 */
			sta_ds->pmfSaQueryRetryCount = 0;
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx, STATUS_ASSOC_REJECTED_TEMPORARILY, 1,
				hdr->sa, sub_type, sta_ds, session, false);
			lim_send_sa_query_request_frame(mac_ctx,
				(uint8_t *) &(sta_ds->pmfSaQueryCurrentTransId),
				hdr->sa, session);
			sta_ds->pmfSaQueryStartTransId =
				sta_ds->pmfSaQueryCurrentTransId;
			sta_ds->pmfSaQueryCurrentTransId++;

			/* start timer for SA Query retry */
			if (tx_timer_activate(&sta_ds->pmfSaQueryTimer)
					!= TX_SUCCESS) {
				pe_err("PMF SA Query timer start failed!");
				return false;
			}
			sta_ds->pmfSaQueryState = DPH_SA_QUERY_IN_PROGRESS;
			return false;
		/*
		 * SA Query procedure still going, respond to Association
		 * Request with try again later
		 */
		case DPH_SA_QUERY_IN_PROGRESS:
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx, STATUS_ASSOC_REJECTED_TEMPORARILY, 1,
				hdr->sa, sub_type, 0, session, false);
			return false;

		/*
		 * SA Query procedure timed out, accept Association
		 * Request normally
		 */
		case DPH_SA_QUERY_TIMED_OUT:
			sta_ds->pmfSaQueryState = DPH_SA_QUERY_NOT_IN_PROGRESS;
			break;
		}
	}
#endif

	/* no change in the capability so drop the frame */
	if ((sub_type == LIM_ASSOC) &&
		(!qdf_mem_cmp(&sta_ds->mlmStaContext.capabilityInfo,
			&assoc_req->capabilityInfo,
			sizeof(tSirMacCapabilityInfo)))) {
		pe_err("Received Assoc req in state: %X STAid: %d",
			sta_ds->mlmStaContext.mlmState, peer_idx);
		return false;
	}

	/*
	 * STA sent Re/association Request frame while already in
	 * 'associated' state. Update STA capabilities and send
	 * Association response frame with same AID
	 */
	pe_debug("Rcvd Assoc req from STA already connected");
	sta_ds->mlmStaContext.capabilityInfo =
		assoc_req->capabilityInfo;
	if (sta_pre_auth_ctx && (sta_pre_auth_ctx->mlmState ==
		eLIM_MLM_AUTHENTICATED_STATE)) {
		/* STA has triggered pre-auth again */
		*auth_type = sta_pre_auth_ctx->authType;
		lim_delete_pre_auth_node(mac_ctx, hdr->sa);
	} else {
		*auth_type = sta_ds->mlmStaContext.authType;
	}

	*update_ctx = true;
	/* Free pmf query timer before resetting the sta_ds */
	lim_delete_pmf_query_timer(sta_ds);
	if (dph_init_sta_state(mac_ctx, hdr->sa, peer_idx,
			       &session->dph.dphHashTable) == NULL) {
		pe_err("could not Init STAid: %d", peer_idx);
		return false;
	}

	return true;
}

/**
 * lim_chk_wmm() - wmm related checks
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 * @qos_mode: qos mode
 *
 * wmm related checks
 *
 * Return: true of no error, false otherwise
 */
static bool lim_chk_wmm(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			struct pe_session *session, tpSirAssocReq assoc_req,
			uint8_t sub_type, tHalBitVal qos_mode)
{
	tHalBitVal wme_mode;

	limGetWmeMode(session, &wme_mode);
	if ((qos_mode == eHAL_SET) || (wme_mode == eHAL_SET)) {
		/*
		 * for a qsta, check if the requested Traffic spec is admissible
		 * for a non-qsta check if the sta can be admitted
		 */
		if (assoc_req->addtsPresent) {
			uint8_t tspecIdx = 0;

			if (lim_admit_control_add_ts(mac_ctx, hdr->sa,
				&(assoc_req->addtsReq),
				&(assoc_req->qosCapability),
				0, false, NULL, &tspecIdx, session) !=
					QDF_STATUS_SUCCESS) {
				pe_warn("AdmitControl: TSPEC rejected");
				lim_send_assoc_rsp_mgmt_frame(
					mac_ctx, REASON_NO_BANDWIDTH,
					1, hdr->sa, sub_type, 0, session,
					false);
#ifdef WLAN_DEBUG
				mac_ctx->lim.gLimNumAssocReqDropACRejectTS++;
#endif
				return false;
			}
		} else if (lim_admit_control_add_sta(mac_ctx, hdr->sa, false)
				!= QDF_STATUS_SUCCESS) {
			pe_warn("AdmitControl: Sta rejected");
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx, REASON_NO_BANDWIDTH, 1,
				hdr->sa, sub_type, 0, session, false);
#ifdef WLAN_DEBUG
			mac_ctx->lim.gLimNumAssocReqDropACRejectSta++;
#endif
			return false;
		}
		/* else all ok */
		pe_debug("AdmitControl: Sta OK!");
	}
	return true;
}

static void lim_update_sta_ds_op_classes(tpSirAssocReq assoc_req,
					 tpDphHashNode sta_ds)
{
	qdf_mem_copy(&sta_ds->supp_operating_classes,
		     &assoc_req->supp_operating_classes,
		     sizeof(tDot11fIESuppOperatingClasses));
}

static bool lim_is_ocv_enable_in_assoc_req(struct mac_context *mac_ctx,
					   struct sSirAssocReq *assoc_req)
{
	uint32_t ret;
	tDot11fIERSN dot11f_ie_rsn = {0};

	if ((assoc_req->rsnPresent) && !(assoc_req->rsn.length))
		return false;

	/* Unpack the RSN IE */
	ret = dot11f_unpack_ie_rsn(mac_ctx, &assoc_req->rsn.info[0],
				   assoc_req->rsn.length, &dot11f_ie_rsn,
				   false);
	if (!DOT11F_SUCCEEDED(ret))
		return false;

	if (*(uint16_t *)&dot11f_ie_rsn.RSN_Cap &
	    WLAN_CRYPTO_RSN_CAP_OCV_SUPPORTED)
		return true;

	return false;
}

/**
 * lim_update_sta_ds() - updates ds dph entry
 * @mac_ctx: pointer to Global MAC structure
 * @hdr: pointer to the MAC head
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame pointer
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 * @sta_ds: station dph entry
 * @auth_type: indicates security type
 * @akm_type: indicates security type in akm
 * @assoc_req_copied: boolean to indicate if assoc req was copied to tmp above
 * @peer_idx: peer index
 * @qos_mode: qos mode
 * @pmf_connection: flag indicating pmf connection
 * @force_1x1: Flag to check if the HT capable STA needs to be downgraded to 1x1
 * nss.
 *
 * Updates ds dph entry
 *
 * Return: true of no error, false otherwise
 */
static bool lim_update_sta_ds(struct mac_context *mac_ctx, tpSirMacMgmtHdr hdr,
			      struct pe_session *session,
			      tpSirAssocReq assoc_req,
			      uint8_t sub_type, tpDphHashNode sta_ds,
			      tAniAuthType auth_type,
			      enum ani_akm_type akm_type,
			      bool *assoc_req_copied, uint16_t peer_idx,
			      tHalBitVal qos_mode, bool pmf_connection,
			      bool force_1x1)
{
	tHalBitVal wme_mode, wsm_mode;
	uint8_t *ht_cap_ie = NULL;
#ifdef WLAN_FEATURE_11W
	tPmfSaQueryTimerId timer_id;
	uint16_t retry_interval;
#endif
	tDot11fIEVHTCaps *vht_caps;
	tpSirAssocReq tmp_assoc_req;

	if (assoc_req->VHTCaps.present)
		vht_caps = &assoc_req->VHTCaps;
	else if (assoc_req->vendor_vht_ie.VHTCaps.present &&
		 session->vendor_vht_sap)
		vht_caps = &assoc_req->vendor_vht_ie.VHTCaps;
	else
		vht_caps = NULL;

	/*
	 * check here if the parsedAssocReq already pointing to the assoc_req
	 * and free it before assigning this new assoc_req
	 */
	if (session->parsedAssocReq) {
		tmp_assoc_req = session->parsedAssocReq[sta_ds->assocId];
		if (tmp_assoc_req) {
			if (tmp_assoc_req->assocReqFrame) {
				qdf_mem_free(tmp_assoc_req->assocReqFrame);
				tmp_assoc_req->assocReqFrame = NULL;
				tmp_assoc_req->assocReqFrameLength = 0;
			}
			qdf_mem_free(tmp_assoc_req);
			tmp_assoc_req = NULL;
		}

		session->parsedAssocReq[sta_ds->assocId] = assoc_req;
		*assoc_req_copied = true;
	}

	if (!assoc_req->wmeInfoPresent) {
		sta_ds->mlmStaContext.htCapability = 0;
		sta_ds->mlmStaContext.vhtCapability = 0;
	} else {
		sta_ds->mlmStaContext.htCapability = assoc_req->HTCaps.present;
		if ((vht_caps) && vht_caps->present)
			sta_ds->mlmStaContext.vhtCapability = vht_caps->present;
		else
			sta_ds->mlmStaContext.vhtCapability = false;
	}

	lim_update_stads_he_capable(sta_ds, assoc_req);
	sta_ds->qos.addtsPresent =
		(assoc_req->addtsPresent == 0) ? false : true;
	sta_ds->qos.addts = assoc_req->addtsReq;
	sta_ds->qos.capability = assoc_req->qosCapability;
	/*
	 * short slot and short preamble should be updated before doing
	 * limaddsta
	 */
	sta_ds->shortPreambleEnabled =
		(uint8_t) assoc_req->capabilityInfo.shortPreamble;
	sta_ds->shortSlotTimeEnabled =
		(uint8_t) assoc_req->capabilityInfo.shortSlotTime;

	sta_ds->valid = 0;
	sta_ds->mlmStaContext.authType = auth_type;
	sta_ds->mlmStaContext.akm_type = akm_type;
	sta_ds->staType = STA_ENTRY_PEER;
	sta_ds->mlmStaContext.force_1x1 = force_1x1;

	pe_debug("auth_type = %d, akm_type = %d", auth_type, akm_type);

	/*
	 * TODO: If listen interval is more than certain limit, reject the
	 * association. Need to check customer requirements and then implement.
	 */
	sta_ds->mlmStaContext.listenInterval = assoc_req->listenInterval;
	sta_ds->mlmStaContext.capabilityInfo = assoc_req->capabilityInfo;

	if (IS_DOT11_MODE_HT(session->dot11mode) &&
	    sta_ds->mlmStaContext.htCapability) {
		sta_ds->htGreenfield = (uint8_t) assoc_req->HTCaps.greenField;
		sta_ds->htAMpduDensity = assoc_req->HTCaps.mpduDensity;
		sta_ds->htDsssCckRate40MHzSupport =
			(uint8_t) assoc_req->HTCaps.dsssCckMode40MHz;
		sta_ds->htLsigTXOPProtection =
			(uint8_t) assoc_req->HTCaps.lsigTXOPProtection;
		sta_ds->htMaxAmsduLength =
			(uint8_t) assoc_req->HTCaps.maximalAMSDUsize;
		sta_ds->htMaxRxAMpduFactor = assoc_req->HTCaps.maxRxAMPDUFactor;
		sta_ds->htMIMOPSState = assoc_req->HTCaps.mimoPowerSave;

		/* assoc_req will be copied to session->parsedAssocReq later */
		ht_cap_ie = ((uint8_t *) &assoc_req->HTCaps) + 1;

		if (session->ht_config.ht_sgi20) {
			sta_ds->htShortGI20Mhz =
				(uint8_t)assoc_req->HTCaps.shortGI20MHz;
		} else {
			/* Unset htShortGI20Mhz in ht_caps*/
			*ht_cap_ie &= ~(1 << SIR_MAC_HT_CAP_SHORTGI20MHZ_S);
			sta_ds->htShortGI20Mhz = 0;
		}

		if (session->ht_config.ht_sgi40) {
			sta_ds->htShortGI40Mhz =
				(uint8_t)assoc_req->HTCaps.shortGI40MHz;
		} else {
			/* Unset htShortGI40Mhz in ht_caps */
			*ht_cap_ie &= ~(1 << SIR_MAC_HT_CAP_SHORTGI40MHZ_S);
			sta_ds->htShortGI40Mhz = 0;
		}

		sta_ds->htSupportedChannelWidthSet =
			(uint8_t) assoc_req->HTCaps.supportedChannelWidthSet;
		if (session->ch_width > CH_WIDTH_20MHZ &&
		    session->ch_width <= CH_WIDTH_80P80MHZ &&
		    sta_ds->htSupportedChannelWidthSet) {
			/*
			 * peer just follows AP; so when we are softAP/GO,
			 * we just store our session entry's secondary channel
			 * offset here in peer INFRA STA. However, if peer's
			 * 40MHz channel width support is disabled then
			 * secondary channel will be zero
			 */
			sta_ds->htSecondaryChannelOffset =
				session->htSecondaryChannelOffset;
			sta_ds->ch_width = CH_WIDTH_40MHZ;
			if (sta_ds->mlmStaContext.vhtCapability) {
				if (assoc_req->operMode.present) {
					sta_ds->ch_width =
						assoc_req->operMode.chanWidth;
				} else if (vht_caps->supportedChannelWidthSet ==
					   VHT_CAP_160_AND_80P80_SUPP) {
					sta_ds->ch_width = CH_WIDTH_80P80MHZ;
				} else if (vht_caps->supportedChannelWidthSet ==
					   VHT_CAP_160_SUPP) {
					if (vht_caps->vht_extended_nss_bw_cap &&
					    vht_caps->extended_nss_bw_supp)
						sta_ds->ch_width =
							CH_WIDTH_80P80MHZ;
					else
						sta_ds->ch_width =
							CH_WIDTH_160MHZ;
				} else if (vht_caps->vht_extended_nss_bw_cap) {
					if (vht_caps->extended_nss_bw_supp ==
					    VHT_EXTD_NSS_80_HALF_NSS_160)
						sta_ds->ch_width =
								CH_WIDTH_160MHZ;
					else if (vht_caps->extended_nss_bw_supp >
						 VHT_EXTD_NSS_80_HALF_NSS_160)
						sta_ds->ch_width =
							CH_WIDTH_80P80MHZ;
					else
						sta_ds->ch_width =
							CH_WIDTH_80MHZ;
				} else {
					sta_ds->ch_width = CH_WIDTH_80MHZ;
				}

				sta_ds->ch_width = QDF_MIN(sta_ds->ch_width,
							   session->ch_width);
			}
		} else {
			sta_ds->htSupportedChannelWidthSet = 0;
			sta_ds->htSecondaryChannelOffset = 0;
			sta_ds->ch_width = CH_WIDTH_20MHZ;
		}
		sta_ds->htLdpcCapable =
			(uint8_t) assoc_req->HTCaps.advCodingCap;
	}

	if (assoc_req->ExtCap.present)
		sta_ds->non_ecsa_capable =
		    !((struct s_ext_cap *)assoc_req->ExtCap.bytes)->
		    ext_chan_switch;
	else
		sta_ds->non_ecsa_capable = 1;

	if (sta_ds->mlmStaContext.vhtCapability &&
	    session->vhtCapability) {
		sta_ds->htMaxRxAMpduFactor =
				vht_caps->maxAMPDULenExp;
		sta_ds->vhtLdpcCapable =
			(uint8_t)vht_caps->ldpcCodingCap;
		if (session->vht_config.su_beam_formee &&
				vht_caps->suBeamFormerCap)
			sta_ds->vhtBeamFormerCapable = 1;
		else
			sta_ds->vhtBeamFormerCapable = 0;
		if (session->vht_config.su_beam_former &&
				vht_caps->suBeamformeeCap)
			sta_ds->vht_su_bfee_capable = 1;
		else
			sta_ds->vht_su_bfee_capable = 0;

		pe_debug("peer_caps: suBformer: %d, suBformee: %d",
			 vht_caps->suBeamFormerCap,
			 vht_caps->suBeamformeeCap);
		pe_debug("self_cap: suBformer: %d, suBformee: %d",
			 session->vht_config.su_beam_former,
			 session->vht_config.su_beam_formee);
		pe_debug("connection's final cap: suBformer: %d, suBformee: %d",
			 sta_ds->vhtBeamFormerCapable,
			 sta_ds->vht_su_bfee_capable);
	}

	sta_ds->vht_mcs_10_11_supp = 0;
	if (IS_DOT11_MODE_HT(session->dot11mode) &&
	    sta_ds->mlmStaContext.vhtCapability) {
		if (mac_ctx->mlme_cfg->vht_caps.vht_cap_info.
		    vht_mcs_10_11_supp &&
		    assoc_req->qcn_ie.present &&
		    assoc_req->qcn_ie.vht_mcs11_attr.present)
			sta_ds->vht_mcs_10_11_supp =
				assoc_req->qcn_ie.vht_mcs11_attr.
				vht_mcs_10_11_supp;
	}
	lim_intersect_sta_he_caps(mac_ctx, assoc_req, session, sta_ds);

	if (lim_populate_matching_rate_set(mac_ctx, sta_ds,
			&(assoc_req->supportedRates),
			&(assoc_req->extendedRates),
			assoc_req->HTCaps.supportedMCSSet,
			session, vht_caps,
			&assoc_req->he_cap) != QDF_STATUS_SUCCESS) {
		/* Could not update hash table entry at DPH with rateset */
		pe_err("Couldn't update hash entry for aid: %d MacAddr: "
		       QDF_MAC_ADDR_FMT,
		       peer_idx, QDF_MAC_ADDR_REF(hdr->sa));

		/* Release AID */
		lim_release_peer_idx(mac_ctx, peer_idx, session);

		lim_reject_association(mac_ctx, hdr->sa,
			sub_type, true, auth_type, peer_idx, false,
			STATUS_UNSPECIFIED_FAILURE,
			session);
		pe_err("Delete dph hash entry");
		if (dph_delete_hash_entry(mac_ctx, hdr->sa, sta_ds->assocId,
			 &session->dph.dphHashTable) != QDF_STATUS_SUCCESS)
			pe_err("error deleting hash entry");
		return false;
	}
	if (assoc_req->operMode.present) {
		sta_ds->vhtSupportedRxNss = assoc_req->operMode.rxNSS + 1;
	} else {
		sta_ds->vhtSupportedRxNss =
			 ((sta_ds->supportedRates.vhtTxMCSMap & MCSMAPMASK2x2)
			  == MCSMAPMASK2x2) ? 1 : 2;
	}
	lim_update_stads_he_6ghz_op(session, sta_ds);
	lim_update_sta_ds_op_classes(assoc_req, sta_ds);
	/* Add STA context at MAC HW (BMU, RHP & TFP) */
	sta_ds->qosMode = false;
	sta_ds->lleEnabled = false;
	if (assoc_req->capabilityInfo.qos && (qos_mode == eHAL_SET)) {
		sta_ds->lleEnabled = true;
		sta_ds->qosMode = true;
	}

	sta_ds->wmeEnabled = false;
	sta_ds->wsmEnabled = false;
	limGetWmeMode(session, &wme_mode);
	if ((!sta_ds->lleEnabled) && assoc_req->wmeInfoPresent
	    && (wme_mode == eHAL_SET)) {
		sta_ds->wmeEnabled = true;
		sta_ds->qosMode = true;
		limGetWsmMode(session, &wsm_mode);
		/*
		 * WMM_APSD - WMM_SA related processing should be separate;
		 * WMM_SA and WMM_APSD can coexist
		 */
		if (assoc_req->WMMInfoStation.present) {
			/* check whether AP supports or not */
			if (LIM_IS_AP_ROLE(session) &&
				(session->apUapsdEnable == 0) &&
				(assoc_req->WMMInfoStation.acbe_uapsd ||
					assoc_req->WMMInfoStation.acbk_uapsd ||
					assoc_req->WMMInfoStation.acvo_uapsd ||
					assoc_req->WMMInfoStation.acvi_uapsd)) {
				/*
				 * Rcvd Re/Assoc Req from STA when UPASD is
				 * not supported.
				 */
				pe_err("UAPSD not supported, reply accordingly");
				/* update UAPSD and send it to LIM to add STA */
				sta_ds->qos.capability.qosInfo.acbe_uapsd = 0;
				sta_ds->qos.capability.qosInfo.acbk_uapsd = 0;
				sta_ds->qos.capability.qosInfo.acvo_uapsd = 0;
				sta_ds->qos.capability.qosInfo.acvi_uapsd = 0;
				sta_ds->qos.capability.qosInfo.maxSpLen = 0;
			} else {
				/* update UAPSD and send it to LIM to add STA */
				sta_ds->qos.capability.qosInfo.acbe_uapsd =
					assoc_req->WMMInfoStation.acbe_uapsd;
				sta_ds->qos.capability.qosInfo.acbk_uapsd =
					assoc_req->WMMInfoStation.acbk_uapsd;
				sta_ds->qos.capability.qosInfo.acvo_uapsd =
					assoc_req->WMMInfoStation.acvo_uapsd;
				sta_ds->qos.capability.qosInfo.acvi_uapsd =
					assoc_req->WMMInfoStation.acvi_uapsd;
				sta_ds->qos.capability.qosInfo.maxSpLen =
					assoc_req->WMMInfoStation.max_sp_length;
			}
		}
		if (assoc_req->wsmCapablePresent && (wsm_mode == eHAL_SET))
			sta_ds->wsmEnabled = true;
	}
	/* Re/Assoc Response frame to requesting STA */
	sta_ds->mlmStaContext.subType = sub_type;

#ifdef WLAN_FEATURE_11W
	sta_ds->rmfEnabled = (pmf_connection) ? 1 : 0;
	sta_ds->pmfSaQueryState = DPH_SA_QUERY_NOT_IN_PROGRESS;
	timer_id.fields.sessionId = session->peSessionId;
	timer_id.fields.peerIdx = peer_idx;
	retry_interval = mac_ctx->mlme_cfg->gen.pmf_sa_query_retry_interval;
	if (cfg_min(CFG_PMF_SA_QUERY_RETRY_INTERVAL) > retry_interval) {
		retry_interval = cfg_default(CFG_PMF_SA_QUERY_RETRY_INTERVAL);
	}
	if (sta_ds->rmfEnabled) {
		sta_ds->ocv_enabled = lim_is_ocv_enable_in_assoc_req(mac_ctx,
								     assoc_req);
		/* Try to delete it before, creating.*/
		lim_delete_pmf_query_timer(sta_ds);
		if (tx_timer_create(mac_ctx, &sta_ds->pmfSaQueryTimer,
		    "PMF SA Query timer", lim_pmf_sa_query_timer_handler,
		    timer_id.value,
		    SYS_MS_TO_TICKS((retry_interval * 1024) / 1000),
		    0, TX_NO_ACTIVATE) != TX_SUCCESS) {
			pe_err("could not create PMF SA Query timer");
			lim_reject_association(mac_ctx, hdr->sa, sub_type,
					       true, auth_type, peer_idx, false,
					       STATUS_UNSPECIFIED_FAILURE,
					       session);
			return false;
		}
		pe_debug("Created pmf timer assoc-id:%d sta mac" QDF_MAC_ADDR_FMT,
			 sta_ds->assocId, QDF_MAC_ADDR_REF(sta_ds->staAddr));
	}
#endif

	if (assoc_req->ExtCap.present) {
		lim_set_stads_rtt_cap(sta_ds,
			(struct s_ext_cap *) assoc_req->ExtCap.bytes, mac_ctx);
	} else {
		sta_ds->timingMeasCap = 0;
		pe_debug("ExtCap not present");
	}
	lim_ap_check_6g_compatible_peer(mac_ctx, session);
	return true;
}

/**
 * lim_update_sta_ctx() - add/del sta depending on connection state machine
 * @mac_ctx: pointer to Global MAC structure
 * @session: pointer to pe session entry
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @sub_type: Assoc(=0) or Reassoc(=1) Requestframe
 * @sta_ds: station dph entry
 * @update_ctx: indicates if STA context already exist
 *
 * Checks for SSID match
 *
 * Return: true of no error, false otherwise
 */
static bool lim_update_sta_ctx(struct mac_context *mac_ctx, struct pe_session *session,
			       tpSirAssocReq assoc_req, uint8_t sub_type,
			       tpDphHashNode sta_ds, uint8_t update_ctx)
{
	tLimMlmStates mlm_prev_state;
	/*
	 * BTAMP: If STA context already exist (ie. update_ctx = 1) for this STA
	 * then we should delete the old one, and add the new STA. This is taken
	 * care of in the lim_del_sta() routine.
	 *
	 * Prior to BTAMP, we were setting this flag so that when PE receives
	 * SME_ASSOC_CNF, and if this flag is set, then PE shall delete the old
	 * station and then add. But now in BTAMP, we're directly adding station
	 * before waiting for SME_ASSOC_CNF, so we can do this now.
	 */
	if (!(update_ctx)) {
		sta_ds->mlmStaContext.updateContext = 0;

		/*
		 * BTAMP: Add STA context at HW - issue WMA_ADD_STA_REQ to HAL
		 */
		if (lim_add_sta(mac_ctx, sta_ds, false, session) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("could not Add STA with assocId: %d",
				sta_ds->assocId);
			lim_reject_association(mac_ctx, sta_ds->staAddr,
				sta_ds->mlmStaContext.subType, true,
				sta_ds->mlmStaContext.authType,
				sta_ds->assocId, true,
				STATUS_UNSPECIFIED_FAILURE,
				session);

			if (session->parsedAssocReq)
				assoc_req =
				    session->parsedAssocReq[sta_ds->assocId];
			return false;
		}
	} else {
		sta_ds->mlmStaContext.updateContext = 1;
		mlm_prev_state = sta_ds->mlmStaContext.mlmState;

		/*
		 * As per the HAL/FW needs the reassoc req need not be calling
		 * lim_del_sta
		 */
		if (sub_type != LIM_REASSOC) {
			/*
			 * we need to set the mlmState here in order
			 * differentiate in lim_del_sta.
			 */
			sta_ds->mlmStaContext.mlmState =
				eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE;
			if (lim_del_sta(mac_ctx, sta_ds, true, session)
					!= QDF_STATUS_SUCCESS) {
				pe_err("Couldn't DEL STA, assocId: %d sta mac"
				       QDF_MAC_ADDR_FMT, sta_ds->assocId,
				       QDF_MAC_ADDR_REF(sta_ds->staAddr));
				lim_reject_association(mac_ctx, sta_ds->staAddr,
					sta_ds->mlmStaContext.subType, true,
					sta_ds->mlmStaContext.authType,
					sta_ds->assocId, true,
					STATUS_UNSPECIFIED_FAILURE,
					session);

				/* Restoring the state back. */
				sta_ds->mlmStaContext.mlmState = mlm_prev_state;
				if (session->parsedAssocReq)
					assoc_req = session->parsedAssocReq[
						sta_ds->assocId];
				return false;
			}
		} else {
			/*
			 * mlmState is changed in lim_add_sta context use the
			 * same AID, already allocated
			 */
			if (lim_add_sta(mac_ctx, sta_ds, false, session)
				!= QDF_STATUS_SUCCESS) {
				pe_err("UPASD not supported, REASSOC Failed");
				lim_reject_association(mac_ctx, sta_ds->staAddr,
					sta_ds->mlmStaContext.subType, true,
					sta_ds->mlmStaContext.authType,
					sta_ds->assocId, true,
					STATUS_TDLS_WAKEUP_REJECT,
					session);

				/* Restoring the state back. */
				sta_ds->mlmStaContext.mlmState = mlm_prev_state;
				if (session->parsedAssocReq)
					assoc_req = session->parsedAssocReq[
							sta_ds->assocId];
				return false;
			}
		}
	}
	return true;
}

void lim_process_assoc_cleanup(struct mac_context *mac_ctx,
			       struct pe_session *session,
			       tpSirAssocReq assoc_req,
			       tpDphHashNode sta_ds,
			       bool assoc_req_copied)
{
	tpSirAssocReq tmp_assoc_req;

	if (assoc_req) {
		if (assoc_req->assocReqFrame) {
			qdf_mem_free(assoc_req->assocReqFrame);
			assoc_req->assocReqFrame = NULL;
			assoc_req->assocReqFrameLength = 0;
		}

		qdf_mem_free(assoc_req);
		/* to avoid double free */
		if (assoc_req_copied && session->parsedAssocReq && sta_ds)
			session->parsedAssocReq[sta_ds->assocId] = NULL;
	}

	/* If it is not duplicate Assoc request then only make to Null */
	if ((sta_ds) &&
	    (sta_ds->mlmStaContext.mlmState != eLIM_MLM_WT_ADD_STA_RSP_STATE)) {
		if (session->parsedAssocReq) {
			tmp_assoc_req =
				session->parsedAssocReq[sta_ds->assocId];
			if (tmp_assoc_req) {
				if (tmp_assoc_req->assocReqFrame) {
					qdf_mem_free(
						tmp_assoc_req->assocReqFrame);
					tmp_assoc_req->assocReqFrame = NULL;
					tmp_assoc_req->assocReqFrameLength = 0;
				}
				qdf_mem_free(tmp_assoc_req);
				session->parsedAssocReq[sta_ds->assocId] = NULL;
			}
		}
	}
}

/**
 * lim_defer_sme_indication() - Defer assoc indication to SME
 * @mac_ctx: Pointer to Global MAC structure
 * @session: pe session entry
 * @sub_type: Indicates whether it is Association Request(=0) or Reassociation
 *            Request(=1) frame
 * @hdr: A pointer to the MAC header
 * @assoc_req: pointer to ASSOC/REASSOC Request frame
 * @pmf_connection: flag indicating pmf connection
 * @assoc_req_copied: boolean to indicate if assoc req was copied to tmp above
 * @dup_entry: flag indicating if duplicate entry found
 *
 * Defer Initialization of PE data structures and wait for an external event.
 * lim_send_assoc_ind_to_sme() will be called to initialize PE data structures
 * when the expected event is received.
 *
 * Return: void
 */
static void lim_defer_sme_indication(struct mac_context *mac_ctx,
				     struct pe_session *session,
				     uint8_t sub_type,
				     tpSirMacMgmtHdr hdr,
				     struct sSirAssocReq *assoc_req,
				     bool pmf_connection,
				     bool assoc_req_copied,
				     bool dup_entry,
				     struct sDphHashNode *sta_ds)
{
	struct tLimPreAuthNode *sta_pre_auth_ctx;
	struct lim_assoc_data *cached_req;

	/* Extract pre-auth context for the STA, if any. */
	sta_pre_auth_ctx = lim_search_pre_auth_list(mac_ctx, hdr->sa);
	if (sta_pre_auth_ctx->assoc_req.present) {
		pe_debug("Free the cached assoc req as a new one is received");
		cached_req = &sta_pre_auth_ctx->assoc_req;
		lim_process_assoc_cleanup(mac_ctx, session,
					  cached_req->assoc_req,
					  cached_req->sta_ds,
					  cached_req->assoc_req_copied);
	}

	sta_pre_auth_ctx->assoc_req.present = true;
	sta_pre_auth_ctx->assoc_req.sub_type = sub_type;
	qdf_mem_copy(&sta_pre_auth_ctx->assoc_req.hdr, hdr,
		     sizeof(tSirMacMgmtHdr));
	sta_pre_auth_ctx->assoc_req.assoc_req = assoc_req;
	sta_pre_auth_ctx->assoc_req.pmf_connection = pmf_connection;
	sta_pre_auth_ctx->assoc_req.assoc_req_copied = assoc_req_copied;
	sta_pre_auth_ctx->assoc_req.dup_entry = dup_entry;
	sta_pre_auth_ctx->assoc_req.sta_ds = sta_ds;
}

bool lim_send_assoc_ind_to_sme(struct mac_context *mac_ctx,
			       struct pe_session *session,
			       uint8_t sub_type, tpSirMacMgmtHdr hdr,
			       tpSirAssocReq assoc_req,
			       enum ani_akm_type akm_type,
			       bool pmf_connection, bool *assoc_req_copied,
			       bool dup_entry, bool force_1x1)
{
	uint16_t peer_idx;
	struct tLimPreAuthNode *sta_pre_auth_ctx;
	tpDphHashNode sta_ds = NULL;
	tHalBitVal qos_mode;
	tAniAuthType auth_type;
	uint8_t update_ctx = false;

	limGetQosMode(session, &qos_mode);
	/* Extract 'associated' context for STA, if any. */
	sta_ds = dph_lookup_hash_entry(mac_ctx, hdr->sa, &peer_idx,
				       &session->dph.dphHashTable);

	/* Extract pre-auth context for the STA, if any. */
	sta_pre_auth_ctx = lim_search_pre_auth_list(mac_ctx, hdr->sa);

	if (!sta_ds) {
		if (!lim_process_assoc_req_no_sta_ctx(mac_ctx, hdr, session,
						      assoc_req, sub_type,
						      sta_pre_auth_ctx, sta_ds,
						      &auth_type))
			return false;
	} else {
		if (!lim_process_assoc_req_sta_ctx(mac_ctx, hdr, session,
						   assoc_req, sub_type,
						   sta_pre_auth_ctx, sta_ds,
						   peer_idx, &auth_type,
						   &update_ctx))
			return false;
		goto send_ind_to_sme;
	}

	/* check if sta is allowed per QoS AC rules */
	if (!lim_chk_wmm(mac_ctx, hdr, session, assoc_req, sub_type, qos_mode))
		return false;

	/* STA is Associated ! */
	pe_debug("Received: %s Req  successful from " QDF_MAC_ADDR_FMT,
		 (sub_type == LIM_ASSOC) ? "Assoc" : "ReAssoc",
		 QDF_MAC_ADDR_REF(hdr->sa));

	/*
	 * AID for this association will be same as the peer Index used in DPH
	 * table. Assign unused/least recently used peer Index from perStaDs.
	 * NOTE: lim_assign_peer_idx() assigns AID values ranging between
	 * 1 - cfg_item(WNI_CFG_ASSOC_STA_LIMIT)
	 */

	peer_idx = lim_assign_peer_idx(mac_ctx, session);

	if (!peer_idx) {
		/* Could not assign AID. Reject association */
		pe_err("PeerIdx not avaialble. Reject associaton");
		lim_reject_association(mac_ctx, hdr->sa, sub_type,
				       true, auth_type, peer_idx, false,
				       STATUS_UNSPECIFIED_FAILURE,
				       session);
		return false;
	}

	/* Add an entry to hash table maintained by DPH module */

	sta_ds = dph_add_hash_entry(mac_ctx, hdr->sa, peer_idx,
				    &session->dph.dphHashTable);

	if (!sta_ds) {
		/* Could not add hash table entry at DPH */
		pe_err("couldn't add hash entry at DPH for aid: %d MacAddr:"
			   QDF_MAC_ADDR_FMT, peer_idx, QDF_MAC_ADDR_REF(hdr->sa));

		/* Release AID */
		lim_release_peer_idx(mac_ctx, peer_idx, session);

		lim_reject_association(mac_ctx, hdr->sa, sub_type,
				       true, auth_type, peer_idx, false,
				       STATUS_UNSPECIFIED_FAILURE,
			session);
		return false;
	}

	if (LIM_IS_AP_ROLE(session)) {
		if ((assoc_req->wpaPresent || assoc_req->rsnPresent) &&
		    !session->privacy) {
			lim_reject_association(mac_ctx, hdr->sa, sub_type,
					       true, auth_type, peer_idx,
					       true,
					       STATUS_UNSPECIFIED_FAILURE,
					       session);
			return false;
		}
	}

send_ind_to_sme:
	if (!lim_update_sta_ds(mac_ctx, hdr, session, assoc_req,
			       sub_type, sta_ds, auth_type, akm_type,
			       assoc_req_copied, peer_idx, qos_mode,
			       pmf_connection, force_1x1))
		return false;

	/* BTAMP: Storing the parsed assoc request in the session array */
	if (session->parsedAssocReq)
		session->parsedAssocReq[sta_ds->assocId] = assoc_req;
	*assoc_req_copied = true;

	/* If it is duplicate entry wait till the peer is deleted */
	if (!dup_entry) {
		if (!lim_update_sta_ctx(mac_ctx, session, assoc_req,
					sub_type, sta_ds, update_ctx))
			return false;
	}

	/* AddSta is success here */
	if (LIM_IS_AP_ROLE(session) && IS_DOT11_MODE_HT(session->dot11mode) &&
	    assoc_req->HTCaps.present && assoc_req->wmeInfoPresent) {
		/*
		 * Update in the HAL Sta Table for the Update of the Protection
		 * Mode
		 */
		lim_post_sm_state_update(mac_ctx,
					 sta_ds->htMIMOPSState, sta_ds->staAddr,
					 session->smeSessionId);
	}

	return true;
}

/**
 * lim_peer_present_on_any_sta() - Check if Same MAC is connected with STA, i.e.
 * duplicate mac detection.
 * @mac_ctx: Pointer to Global MAC structure
 * @peer_addr: peer address to check
 *
 * This function will return true if a peer STA and AP are using same mac
 * address.
 *
 * @Return: bool
 */
static bool
lim_peer_present_on_any_sta(struct mac_context *mac_ctx, uint8_t *peer_addr)
{
	struct wlan_objmgr_peer *peer;
	bool sta_peer_present = false;
	enum QDF_OPMODE mode;
	uint8_t peer_vdev_id;

	peer = wlan_objmgr_get_peer_by_mac(mac_ctx->psoc, peer_addr,
					   WLAN_LEGACY_MAC_ID);
	if (!peer)
		return sta_peer_present;

	peer_vdev_id = wlan_vdev_get_id(wlan_peer_get_vdev(peer));
	mode = wlan_vdev_mlme_get_opmode(wlan_peer_get_vdev(peer));
	if (mode == QDF_STA_MODE || mode == QDF_P2P_CLIENT_MODE) {
		pe_debug("duplicate mac detected!!! Peer " QDF_MAC_ADDR_FMT " present on STA vdev %d",
			 QDF_MAC_ADDR_REF(peer_addr), peer_vdev_id);
		sta_peer_present = true;
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_MAC_ID);

	return sta_peer_present;
}

/**
 * lim_process_assoc_req_frame() - Process RE/ASSOC Request frame.
 * @mac_ctx: Pointer to Global MAC structure
 * @rx_pkt_info: A pointer to Buffer descriptor + associated PDUs
 * @sub_type: Indicates whether it is Association Request(=0) or Reassociation
 *            Request(=1) frame
 * @session: pe session entry
 *
 * This function is called to process RE/ASSOC Request frame.
 *
 * @Return: void
 */
void lim_process_assoc_req_frame(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
				 uint8_t sub_type, struct pe_session *session)
{
	bool pmf_connection = false, assoc_req_copied = false;
	uint8_t *frm_body;
	uint16_t assoc_id = 0;
	uint32_t frame_len;
	uint32_t phy_mode;
	tHalBitVal qos_mode;
	tpSirMacMgmtHdr hdr;
	struct tLimPreAuthNode *sta_pre_auth_ctx;
	enum ani_akm_type akm_type = ANI_AKM_TYPE_NONE;
	tSirMacCapabilityInfo local_cap;
	tpDphHashNode sta_ds = NULL;
	tpSirAssocReq assoc_req;
	bool dup_entry = false, force_1x1 = false;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *vdev;

	lim_get_phy_mode(mac_ctx, &phy_mode, session);

	limGetQosMode(session, &qos_mode);

	hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	pe_nofl_rl_debug("Assoc req RX: subtype %d vdev %d sys role %d lim state %d rssi %d from " QDF_MAC_ADDR_FMT,
			 sub_type, session->vdev_id, GET_LIM_SYSTEM_ROLE(session),
			 session->limMlmState,
			 WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info),
			 QDF_MAC_ADDR_REF(hdr->sa));

	if (LIM_IS_STA_ROLE(session)) {
		pe_err("Rcvd unexpected ASSOC REQ, sessionid: %d sys sub_type: %d for role: %d from: "
			   QDF_MAC_ADDR_FMT,
			session->peSessionId, sub_type,
			GET_LIM_SYSTEM_ROLE(session),
			QDF_MAC_ADDR_REF(hdr->sa));
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
				   WMA_GET_RX_MPDU_DATA(rx_pkt_info),
				   frame_len);
		return;
	}
	if (session->limMlmState == eLIM_MLM_WT_DEL_BSS_RSP_STATE) {
		pe_err("drop ASSOC REQ on sessionid: %d "
			"role: %d from: "QDF_MAC_ADDR_FMT" in limMlmState: %d",
			session->peSessionId,
			GET_LIM_SYSTEM_ROLE(session),
			QDF_MAC_ADDR_REF(hdr->sa),
			eLIM_MLM_WT_DEL_BSS_RSP_STATE);
		return;
	}
	vdev = session->vdev;
	if (!vdev) {
		pe_err("vdev is NULL");
		return;
	}

	if (wlan_vdev_mlme_get_state(vdev) != WLAN_VDEV_S_UP) {
		pe_err("SAP is not up, drop ASSOC REQ on sessionid: %d",
		       session->peSessionId);

		return;
	}

	if (lim_peer_present_on_any_sta(mac_ctx, hdr->sa))
		/*
		 * This mean a AP and STA have same mac address and device STA
		 * is already connected to the AP, and STA is now trying to
		 * connect to device SAP. So ignore association.
		 */
		return;

	/*
	 * If a STA is already present in DPH and it is initiating a Assoc
	 * re-transmit, do not process it. This can happen when first Assoc Req
	 * frame is received but ACK lost at STA side. The ACK for this dropped
	 * Assoc Req frame should be sent by HW. Host simply does not process it
	 * once the entry for the STA is already present in DPH.
	 */
	sta_ds = dph_lookup_hash_entry(mac_ctx, hdr->sa, &assoc_id,
				&session->dph.dphHashTable);
	if (sta_ds) {
		if (hdr->fc.retry > 0) {
			pe_err("STA is initiating Assoc Req after ACK lost. Do not process sessionid: %d sys sub_type=%d for role=%d from: "
				QDF_MAC_ADDR_FMT, session->peSessionId,
			sub_type, GET_LIM_SYSTEM_ROLE(session),
			QDF_MAC_ADDR_REF(hdr->sa));
			return;
		} else if (!sta_ds->rmfEnabled && (sub_type == LIM_REASSOC)) {
			/*
			 * SAP should send reassoc response with reject code
			 * to avoid IOT issues. as per the specification SAP
			 * should do 4-way handshake after reassoc response and
			 * some STA doesn't like 4way handshake after reassoc
			 * where some STA does expect 4-way handshake.
			 */
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx, STATUS_ASSOC_DENIED_UNSPEC,
				sta_ds->assocId, sta_ds->staAddr,
				sub_type, sta_ds, session, false);
			pe_err("Rejecting reassoc req from STA");
			return;
		} else if (!sta_ds->rmfEnabled) {
			/*
			 * Do this only for non PMF case.
			 * STA might have missed the assoc response, so it is
			 * sending assoc request frame again.
			 */
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx, QDF_STATUS_SUCCESS,
				sta_ds->assocId, sta_ds->staAddr,
				sub_type,
				sta_ds, session, false);
			pe_err("DUT already received an assoc request frame and STA is sending another assoc req.So, do not Process sessionid: %d sys sub_type: %d for role: %d from: "
					QDF_MAC_ADDR_FMT,
				session->peSessionId, sub_type,
				session->limSystemRole,
				QDF_MAC_ADDR_REF(hdr->sa));
			return;
		}
	}

	status = lim_check_sta_in_pe_entries(mac_ctx, hdr, session->peSessionId,
					     &dup_entry);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Reject assoc as duplicate entry is present and is already being deleted, assoc will be accepted once deletion is completed");
		/*
		 * This mean that the duplicate entry is present on other vdev
		 * and is already being deleted, so reject the assoc and lets
		 * peer try again to connect, once peer is deleted from
		 * other vdev.
		 */
		lim_send_assoc_rsp_mgmt_frame(
			mac_ctx,
			STATUS_UNSPECIFIED_FAILURE,
			1, hdr->sa,
			sub_type, 0, session, false);
		return;
	}

	/* Get pointer to Re/Association Request frame body */
	frm_body = WMA_GET_RX_MPDU_DATA(rx_pkt_info);

	if (IEEE80211_IS_MULTICAST(hdr->sa)) {
		/*
		 * Rcvd Re/Assoc Req frame from BC/MC address Log error and
		 * ignore it
		 */
		pe_err("Rcvd: %s Req, sessionid: %d from a BC/MC address"
				QDF_MAC_ADDR_FMT,
			(LIM_ASSOC == sub_type) ? "Assoc" : "ReAssoc",
			session->peSessionId, QDF_MAC_ADDR_REF(hdr->sa));
		return;
	}

	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
				(uint8_t *) frm_body, frame_len);

	if (false == lim_chk_sa_da(mac_ctx, hdr, session, sub_type))
		return;

	/* check for the presence of vendor IE */
	if ((session->access_policy_vendor_ie) &&
		(session->access_policy ==
		LIM_ACCESS_POLICY_RESPOND_IF_IE_IS_PRESENT)) {
		if (frame_len <= LIM_ASSOC_REQ_IE_OFFSET) {
			pe_debug("Received action frame of invalid len %d",
				 frame_len);
			return;
		}
		if (!wlan_get_vendor_ie_ptr_from_oui(
				&session->access_policy_vendor_ie[2],
				3, frm_body + LIM_ASSOC_REQ_IE_OFFSET,
				 frame_len - LIM_ASSOC_REQ_IE_OFFSET)) {
			pe_err("Vendor ie not present and access policy is %x, Rejected association",
				session->access_policy);
			lim_send_assoc_rsp_mgmt_frame(
				mac_ctx, STATUS_UNSPECIFIED_FAILURE,
				1, hdr->sa, sub_type, 0, session, false);
			return;
		}
	}
	/* Allocate memory for the Assoc Request frame */
	assoc_req = qdf_mem_malloc(sizeof(*assoc_req));
	if (!assoc_req)
		return;

	/* Parse Assoc Request frame */
	if (false == lim_chk_assoc_req_parse_error(mac_ctx, hdr, session,
				assoc_req, sub_type, frm_body, frame_len))
		goto error;

	assoc_req->assocReqFrame = qdf_mem_malloc(frame_len);
	if (!assoc_req->assocReqFrame)
		goto error;

	qdf_mem_copy((uint8_t *) assoc_req->assocReqFrame,
		(uint8_t *) frm_body, frame_len);
	assoc_req->assocReqFrameLength = frame_len;

	if (false == lim_chk_capab(mac_ctx, hdr, session, assoc_req,
				sub_type, &local_cap))
		goto error;

	if (false == lim_chk_ssid(mac_ctx, hdr, session, assoc_req, sub_type))
		goto error;

	if (false == lim_chk_rates(mac_ctx, hdr, session, assoc_req, sub_type))
		goto error;

	if (false == lim_chk_11g_only(mac_ctx, hdr, session, assoc_req,
				sub_type))
		goto error;

	if (false == lim_chk_11n_only(mac_ctx, hdr, session, assoc_req,
				sub_type))
		goto error;

	if (false == lim_chk_11ac_only(mac_ctx, hdr, session, assoc_req,
				sub_type))
		goto error;

	if (false == lim_chk_11ax_only(mac_ctx, hdr, session, assoc_req,
				       sub_type))
		goto error;

	if (false == lim_check_11ax_basic_mcs(mac_ctx, hdr, session, assoc_req,
					      sub_type))
		goto error;

	/* Spectrum Management (11h) specific checks */
	lim_process_for_spectrum_mgmt(mac_ctx, hdr, session,
				assoc_req, sub_type, local_cap);

	if (false == lim_chk_mcs(mac_ctx, hdr, session, assoc_req, sub_type))
		goto error;

	if (false == lim_chk_is_11b_sta_supported(mac_ctx, hdr, session,
				assoc_req, sub_type, phy_mode))
		goto error;

	/*
	 * Check for 802.11n HT caps compatibility; are HT Capabilities
	 * turned on in lim?
	 */
	lim_print_ht_cap(mac_ctx, session, assoc_req);

	if (false == lim_chk_n_process_wpa_rsn_ie(mac_ctx, hdr, session,
						  assoc_req, sub_type,
						  &pmf_connection,
						  &akm_type))
		goto error;

	/* Extract pre-auth context for the STA, if any. */
	sta_pre_auth_ctx = lim_search_pre_auth_list(mac_ctx, hdr->sa);

	/* SAE authentication is offloaded to hostapd. Hostapd sends
	 * authentication status to driver after completing SAE
	 * authentication (after sending out 4/4 SAE auth frame).
	 * There is a possible race condition where driver gets
	 * assoc request from SAE station before getting authentication
	 * status from hostapd. Don't reject the association in such
	 * cases and defer the processing of assoc request frame by caching
	 * the frame and process it when the auth status is received.
	 */
	if (sta_pre_auth_ctx &&
	    sta_pre_auth_ctx->authType == eSIR_AUTH_TYPE_SAE &&
	    sta_pre_auth_ctx->mlmState == eLIM_MLM_WT_SAE_AUTH_STATE) {
		pe_debug("Received assoc request frame while SAE authentication is in progress; Defer association request handling till SAE auth status is received");
		lim_defer_sme_indication(mac_ctx, session, sub_type, hdr,
					 assoc_req, pmf_connection,
					 assoc_req_copied, dup_entry, sta_ds);
		return;
	}

	if (session->opmode == QDF_P2P_GO_MODE) {
		/*
		 * WAR: In P2P GO mode, if the P2P client device
		 * is only HT capable and not VHT capable, but the P2P
		 * GO device is VHT capable and advertises 2x2 NSS with
		 * HT capablity client device, which results in IOT
		 * issues.
		 * When GO is operating in DBS mode, GO beacons
		 * advertise 2x2 capability but include OMN IE to
		 * indicate current operating mode of 1x1. But here
		 * peer device is only HT capable and will not
		 * understand OMN IE.
		 */
		force_1x1 = wlan_p2p_check_oui_and_force_1x1(
				frm_body + LIM_ASSOC_REQ_IE_OFFSET,
				frame_len - LIM_ASSOC_REQ_IE_OFFSET);
	}

	/* Send assoc indication to SME */
	if (!lim_send_assoc_ind_to_sme(mac_ctx, session, sub_type, hdr,
				       assoc_req, akm_type, pmf_connection,
				       &assoc_req_copied, dup_entry, force_1x1))
		goto error;

	return;

error:
	lim_process_assoc_cleanup(mac_ctx, session, assoc_req, sta_ds,
				  assoc_req_copied);
	return;
}

#ifdef FEATURE_WLAN_WAPI
/**
 * lim_fill_assoc_ind_wapi_info()- Updates WAPI data in assoc indication
 * @mac_ctx: Global Mac context
 * @assoc_req: pointer to association request
 * @assoc_ind: Pointer to association indication
 * @wpsie: WPS IE
 *
 * This function updates WAPI meta data in association indication message
 * sent to SME.
 *
 * Return: None
 */
static void lim_fill_assoc_ind_wapi_info(struct mac_context *mac_ctx,
	tpSirAssocReq assoc_req, tpLimMlmAssocInd assoc_ind,
	const uint8_t *wpsie)
{
	if (assoc_req->wapiPresent && (!wpsie)) {
		pe_debug("Received WAPI IE length in Assoc Req is %d",
			assoc_req->wapi.length);
		assoc_ind->wapiIE.wapiIEdata[0] = WLAN_ELEMID_WAPI;
		assoc_ind->wapiIE.wapiIEdata[1] = assoc_req->wapi.length;
		qdf_mem_copy(&assoc_ind->wapiIE.wapiIEdata[2],
			assoc_req->wapi.info, assoc_req->wapi.length);
		assoc_ind->wapiIE.length =
			2 + assoc_req->wapi.length;
	}
	return;
}
#else
static void lim_fill_assoc_ind_wapi_info(
	struct mac_context *mac_ctx,
	tpSirAssocReq assoc_req, tpLimMlmAssocInd assoc_ind,
	const uint8_t *wpsie)
{
}
#endif

/**
 * lim_fill_assoc_ind_vht_info() - Updates VHT information in assoc indication
 * @mac_ctx: Global Mac context
 * @assoc_req: pointer to association request
 * @session_entry: PE session entry
 * @assoc_ind: Pointer to association indication
 *
 * This function updates VHT information in association indication message
 * sent to SME.
 *
 * Return: None
 */
static void lim_fill_assoc_ind_vht_info(struct mac_context *mac_ctx,
					struct pe_session *session_entry,
					tpSirAssocReq assoc_req,
					tpLimMlmAssocInd assoc_ind,
					tpDphHashNode sta_ds)
{
	uint8_t chan;
	uint8_t i;
	bool nw_type_11b = true;

	if (session_entry->limRFBand == REG_BAND_2G) {
		if (session_entry->vhtCapability && assoc_req->VHTCaps.present)
			assoc_ind->chan_info.info = MODE_11AC_VHT20_2G;
		else if (session_entry->htCapability
			    && assoc_req->HTCaps.present)
			assoc_ind->chan_info.info = MODE_11NG_HT20;
		else {
			for (i = 0; i < SIR_NUM_11A_RATES; i++) {
				if (sirIsArate(sta_ds->
					       supportedRates.llaRates[i]
					       & 0x7F)) {
					assoc_ind->chan_info.info = MODE_11G;
					nw_type_11b = false;
					break;
				}
			}
			if (nw_type_11b)
				assoc_ind->chan_info.info = MODE_11B;
		}
		return;
	}

	if (session_entry->vhtCapability && assoc_req->VHTCaps.present) {
		if ((session_entry->ch_width > CH_WIDTH_40MHZ)
		    && assoc_req->HTCaps.supportedChannelWidthSet) {
			chan = session_entry->ch_center_freq_seg0;
			assoc_ind->chan_info.band_center_freq1 =
				cds_chan_to_freq(chan);
			assoc_ind->chan_info.info = MODE_11AC_VHT80;
			return;
		}

		if ((session_entry->ch_width == CH_WIDTH_40MHZ)
			&& assoc_req->HTCaps.supportedChannelWidthSet) {
			assoc_ind->chan_info.info = MODE_11AC_VHT40;
			if (session_entry->htSecondaryChannelOffset ==
			    PHY_DOUBLE_CHANNEL_LOW_PRIMARY)
				assoc_ind->chan_info.band_center_freq1 += 10;
			else
				assoc_ind->chan_info.band_center_freq1 -= 10;
			return;
		}

		assoc_ind->chan_info.info = MODE_11AC_VHT20;
		return;
	}

	if (session_entry->htCapability && assoc_req->HTCaps.present) {
		if ((session_entry->ch_width == CH_WIDTH_40MHZ)
		    && assoc_req->HTCaps.supportedChannelWidthSet) {
			assoc_ind->chan_info.info = MODE_11NA_HT40;
			if (session_entry->htSecondaryChannelOffset ==
			    PHY_DOUBLE_CHANNEL_LOW_PRIMARY)
				assoc_ind->chan_info.band_center_freq1 += 10;
			else
				assoc_ind->chan_info.band_center_freq1 -= 10;
			return;
		}

		assoc_ind->chan_info.info = MODE_11NA_HT20;
		return;
	}

	assoc_ind->chan_info.info = MODE_11A;
	return;
}

static uint8_t lim_get_max_rate_idx(tSirMacRateSet *rateset)
{
	uint8_t maxidx;
	int i;

	maxidx = rateset->rate[0] & 0x7f;
	for (i = 1; i < rateset->numRates; i++) {
		if ((rateset->rate[i] & 0x7f) > maxidx)
			maxidx = rateset->rate[i] & 0x7f;
	}

	return maxidx;
}

static void fill_mlm_assoc_ind_vht(tpSirAssocReq assocreq,
		tpDphHashNode stads,
		tpLimMlmAssocInd assocind)
{
	if (stads->mlmStaContext.vhtCapability) {
		/* ampdu */
		assocind->ampdu = true;

		/* sgi */
		if (assocreq->VHTCaps.shortGI80MHz ||
		    assocreq->VHTCaps.shortGI160and80plus80MHz)
			assocind->sgi_enable = true;

		/* stbc */
		assocind->tx_stbc = assocreq->VHTCaps.txSTBC;
		assocind->rx_stbc = assocreq->VHTCaps.rxSTBC;

		/* ch width */
		assocind->ch_width = stads->vhtSupportedChannelWidthSet ?
			eHT_CHANNEL_WIDTH_80MHZ :
			stads->htSupportedChannelWidthSet ?
			eHT_CHANNEL_WIDTH_40MHZ : eHT_CHANNEL_WIDTH_20MHZ;

		/* mode */
		assocind->mode = SIR_SME_PHY_MODE_VHT;
		assocind->rx_mcs_map = assocreq->VHTCaps.rxMCSMap & 0xff;
		assocind->tx_mcs_map = assocreq->VHTCaps.txMCSMap & 0xff;
	}
}

/**
 *lim_convert_channel_width_enum() - map between two channel width enums
 *@ch_width: channel width of enum type phy_ch_width
 *
 *Return: channel width of enum type tSirMacHTChannelWidth
 */
static tSirMacHTChannelWidth
lim_convert_channel_width_enum(enum phy_ch_width ch_width)
{
	switch (ch_width) {
	case CH_WIDTH_20MHZ:
		return eHT_CHANNEL_WIDTH_20MHZ;
	case CH_WIDTH_40MHZ:
		return eHT_CHANNEL_WIDTH_40MHZ;
	case CH_WIDTH_80MHZ:
		return eHT_CHANNEL_WIDTH_80MHZ;
	case CH_WIDTH_160MHZ:
		return eHT_CHANNEL_WIDTH_160MHZ;
	case CH_WIDTH_80P80MHZ:
		return eHT_CHANNEL_WIDTH_80P80MHZ;
	case CH_WIDTH_MAX:
		return eHT_MAX_CHANNEL_WIDTH;
	case CH_WIDTH_5MHZ:
		break;
	case CH_WIDTH_10MHZ:
		break;
	case CH_WIDTH_INVALID:
		break;
	}
	pe_debug("invalid enum: %d", ch_width);
	return eHT_CHANNEL_WIDTH_20MHZ;
}

bool lim_fill_lim_assoc_ind_params(
		tpLimMlmAssocInd assoc_ind,
		struct mac_context *mac_ctx,
		tpDphHashNode sta_ds,
		struct pe_session *session_entry)
{
	tpSirAssocReq assoc_req;
	uint16_t rsn_len;
	uint32_t phy_mode;
	const uint8_t *wpsie = NULL;
	uint8_t maxidx, i;
	bool wme_enable;

	if (!session_entry->parsedAssocReq) {
		pe_err(" Parsed Assoc req is NULL");
		return false;
	}

	/* Get a copy of the already parsed Assoc Request */
	assoc_req =
		(tpSirAssocReq)session_entry->parsedAssocReq[sta_ds->assocId];

	if (!assoc_req) {
		pe_err("assoc req for assoc_id:%d is NULL", sta_ds->assocId);
		return false;
	}

	/* Get the phy_mode */
	lim_get_phy_mode(mac_ctx, &phy_mode, session_entry);

	qdf_mem_copy((uint8_t *)assoc_ind->peerMacAddr,
		     (uint8_t *)sta_ds->staAddr, sizeof(tSirMacAddr));
	assoc_ind->aid = sta_ds->assocId;
	qdf_mem_copy((uint8_t *)&assoc_ind->ssId,
		     (uint8_t *)&assoc_req->ssId,
		     assoc_req->ssId.length + 1);
	assoc_ind->sessionId = session_entry->peSessionId;
	assoc_ind->authType = sta_ds->mlmStaContext.authType;
	assoc_ind->akm_type = sta_ds->mlmStaContext.akm_type;
	assoc_ind->capabilityInfo = assoc_req->capabilityInfo;

	/* Fill in RSN IE information */
	assoc_ind->rsnIE.length = 0;
	/* if WPS IE is present, ignore RSN IE */
	if (assoc_req->addIEPresent && assoc_req->addIE.length) {
		wpsie = limGetWscIEPtr(
			mac_ctx,
			assoc_req->addIE.addIEdata,
			assoc_req->addIE.length);
	}
	if (assoc_req->rsnPresent && !wpsie) {
		pe_debug("Assoc Req RSN IE len: %d",
			 assoc_req->rsn.length);
		assoc_ind->rsnIE.length = 2 + assoc_req->rsn.length;
		assoc_ind->rsnIE.rsnIEdata[0] = WLAN_ELEMID_RSN;
		assoc_ind->rsnIE.rsnIEdata[1] =
			assoc_req->rsn.length;
		qdf_mem_copy(
			&assoc_ind->rsnIE.rsnIEdata[2],
			assoc_req->rsn.info,
			assoc_req->rsn.length);
	}
	/* Fill in 802.11h related info */
	if (assoc_req->powerCapabilityPresent &&
	    assoc_req->supportedChannelsPresent) {
		assoc_ind->spectrumMgtIndicator = true;
		assoc_ind->powerCap.minTxPower =
			assoc_req->powerCapability.minTxPower;
		assoc_ind->powerCap.maxTxPower =
			assoc_req->powerCapability.maxTxPower;
		lim_convert_supported_channels(
			mac_ctx, assoc_ind,
			assoc_req);
	} else {
		assoc_ind->spectrumMgtIndicator = false;
	}

	/* This check is to avoid extra Sec IEs present incase of WPS */
	if (assoc_req->wpaPresent && !wpsie) {
		rsn_len = assoc_ind->rsnIE.length;
		if ((rsn_len + assoc_req->wpa.length)
		    >= WLAN_MAX_IE_LEN) {
			pe_err("rsnIEdata index out of bounds: %d",
			       rsn_len);
			return false;
		}
		assoc_ind->rsnIE.rsnIEdata[rsn_len] =
			SIR_MAC_WPA_EID;
		assoc_ind->rsnIE.rsnIEdata[rsn_len + 1]
			= assoc_req->wpa.length;
		qdf_mem_copy(
			&assoc_ind->rsnIE.rsnIEdata[rsn_len + 2],
			assoc_req->wpa.info, assoc_req->wpa.length);
		assoc_ind->rsnIE.length += 2 + assoc_req->wpa.length;
	}
	lim_fill_assoc_ind_wapi_info(mac_ctx, assoc_req, assoc_ind, wpsie);

	assoc_ind->addIE.length = 0;
	if (assoc_req->addIEPresent) {
		qdf_mem_copy(
			&assoc_ind->addIE.addIEdata,
			assoc_req->addIE.addIEdata,
			assoc_req->addIE.length);
		assoc_ind->addIE.length = assoc_req->addIE.length;
	}
	/*
	 * Add HT Capabilities into addIE for OBSS
	 * processing in hostapd
	 */
	if (assoc_req->HTCaps.present) {
		qdf_mem_copy(&assoc_ind->ht_caps, &assoc_req->HTCaps,
			     sizeof(tDot11fIEHTCaps));
		rsn_len = assoc_ind->addIE.length;
		if (assoc_ind->addIE.length + DOT11F_IE_HTCAPS_MIN_LEN
		    + 2 < WLAN_MAX_IE_LEN) {
			assoc_ind->addIE.addIEdata[rsn_len] =
				WLAN_ELEMID_HTCAP_ANA;
			assoc_ind->addIE.addIEdata[rsn_len + 1] =
				DOT11F_IE_HTCAPS_MIN_LEN;
			qdf_mem_copy(
				&assoc_ind->addIE.addIEdata[rsn_len + 2],
				((uint8_t *)&assoc_req->HTCaps) + 1,
				DOT11F_IE_HTCAPS_MIN_LEN);
			assoc_ind->addIE.length +=
				2 + DOT11F_IE_HTCAPS_MIN_LEN;
		} else {
			pe_err("Fail:HT capabilities IE to addIE");
		}
	}

	if (assoc_req->wmeInfoPresent) {
		/* Set whether AP is enabled with WMM or not */
		wme_enable = mac_ctx->mlme_cfg->wmm_params.wme_enabled;
		assoc_ind->WmmStaInfoPresent = wme_enable;
		/*
		 * Note: we are not rejecting association here
		 * because IOT will fail
		 */
	}
	/* Required for indicating the frames to upper layer */
	assoc_ind->assocReqLength = assoc_req->assocReqFrameLength;
	assoc_ind->assocReqPtr = assoc_req->assocReqFrame;
	assoc_ind->beaconPtr = session_entry->beacon;
	assoc_ind->beaconLength = session_entry->bcnLen;

	assoc_ind->chan_info.mhz = session_entry->curr_op_freq;
	assoc_ind->chan_info.band_center_freq1 =
		session_entry->curr_op_freq;
	assoc_ind->chan_info.band_center_freq2 = 0;
	assoc_ind->chan_info.reg_info_1 =
		(session_entry->maxTxPower << 16);
	assoc_ind->chan_info.reg_info_2 =
		(session_entry->maxTxPower << 8);
	assoc_ind->chan_info.nss = sta_ds->nss;
	assoc_ind->chan_info.rate_flags =
		lim_get_max_rate_flags(mac_ctx, sta_ds);
	assoc_ind->ampdu = false;
	assoc_ind->sgi_enable = false;
	assoc_ind->tx_stbc = false;
	assoc_ind->rx_stbc = false;
	assoc_ind->ch_width = eHT_CHANNEL_WIDTH_20MHZ;
	assoc_ind->mode = SIR_SME_PHY_MODE_LEGACY;
	assoc_ind->max_supp_idx = 0xff;
	assoc_ind->max_ext_idx = 0xff;
	assoc_ind->max_mcs_idx = 0xff;
	assoc_ind->rx_mcs_map = 0xff;
	assoc_ind->tx_mcs_map = 0xff;

	if (assoc_req->supportedRates.numRates)
		assoc_ind->max_supp_idx =
			lim_get_max_rate_idx(&assoc_req->supportedRates);
	if (assoc_req->extendedRates.numRates)
		assoc_ind->max_ext_idx =
			lim_get_max_rate_idx(&assoc_req->extendedRates);

	if (sta_ds->mlmStaContext.htCapability) {
		/* ampdu */
		assoc_ind->ampdu = true;

		/* sgi */
		if (sta_ds->htShortGI20Mhz || sta_ds->htShortGI40Mhz)
			assoc_ind->sgi_enable = true;

		/* stbc */
		assoc_ind->tx_stbc = assoc_req->HTCaps.txSTBC;
		assoc_ind->rx_stbc = assoc_req->HTCaps.rxSTBC;

		/* ch width */
		assoc_ind->ch_width =
			sta_ds->htSupportedChannelWidthSet ?
			eHT_CHANNEL_WIDTH_40MHZ :
			eHT_CHANNEL_WIDTH_20MHZ;
		/* mode */
		assoc_ind->mode = SIR_SME_PHY_MODE_HT;
		maxidx = 0;
		for (i = 0; i < 8; i++) {
			if (assoc_req->HTCaps.supportedMCSSet[0] &
			    (1 << i))
				maxidx = i;
		}
		assoc_ind->max_mcs_idx = maxidx;
	}
	fill_mlm_assoc_ind_vht(assoc_req, sta_ds, assoc_ind);
	if (assoc_req->ExtCap.present)
		assoc_ind->ecsa_capable =
		((struct s_ext_cap *)assoc_req->ExtCap.bytes)->ext_chan_switch;
	/* updates VHT information in assoc indication */
	if (assoc_req->VHTCaps.present)
		qdf_mem_copy(&assoc_ind->vht_caps, &assoc_req->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
	else if (assoc_req->vendor_vht_ie.VHTCaps.present)
		qdf_mem_copy(&assoc_ind->vht_caps,
			     &assoc_req->vendor_vht_ie.VHTCaps,
			     sizeof(tDot11fIEVHTCaps));

	lim_fill_assoc_ind_vht_info(mac_ctx, session_entry, assoc_req,
				    assoc_ind, sta_ds);
	assoc_ind->he_caps_present = assoc_req->he_cap.present;
	assoc_ind->is_sae_authenticated =
				assoc_req->is_sae_authenticated;
	/* updates HE bandwidth in assoc indication */
	if (wlan_reg_is_6ghz_chan_freq(session_entry->curr_op_freq))
		assoc_ind->ch_width =
			lim_convert_channel_width_enum(sta_ds->ch_width);
	return true;
}

QDF_STATUS lim_send_mlm_assoc_ind(struct mac_context *mac_ctx,
				  tpDphHashNode sta_ds,
				  struct pe_session *session_entry)
{
	tpLimMlmAssocInd assoc_ind;
	tpSirAssocReq assoc_req;
	uint16_t temp;
	uint32_t phy_mode;
	uint8_t sub_type;

	if (!session_entry->parsedAssocReq) {
		pe_err(" Parsed Assoc req is NULL");
		return QDF_STATUS_E_INVAL;
	}

	/* Get a copy of the already parsed Assoc Request */
	assoc_req =
		(tpSirAssocReq) session_entry->parsedAssocReq[sta_ds->assocId];

	if (!assoc_req) {
		pe_err("assoc req for assoc_id:%d is NULL", sta_ds->assocId);
		return QDF_STATUS_E_INVAL;
	}

	/* Get the phy_mode */
	lim_get_phy_mode(mac_ctx, &phy_mode, session_entry);

	/* Determine if its Assoc or ReAssoc Request */
	if (assoc_req->reassocRequest == 1)
		sub_type = LIM_REASSOC;
	else
		sub_type = LIM_ASSOC;

	pe_debug("Sessionid: %d ssid: %s sub_type: %d Associd: %d staAddr: "
		 QDF_MAC_ADDR_FMT, session_entry->peSessionId,
		 assoc_req->ssId.ssId, sub_type, sta_ds->assocId,
		 QDF_MAC_ADDR_REF(sta_ds->staAddr));

	if (sub_type == LIM_ASSOC || sub_type == LIM_REASSOC) {
		temp = sizeof(tLimMlmAssocInd);

		assoc_ind = qdf_mem_malloc(temp);
		if (!assoc_ind) {
			lim_release_peer_idx(mac_ctx, sta_ds->assocId,
					     session_entry);
			return QDF_STATUS_E_INVAL;
		}
		if (!lim_fill_lim_assoc_ind_params(assoc_ind, mac_ctx,
						   sta_ds, session_entry)) {
			qdf_mem_free(assoc_ind);
			return QDF_STATUS_E_INVAL;
		}
		lim_post_sme_message(mac_ctx, LIM_MLM_ASSOC_IND,
				     (uint32_t *)assoc_ind);
		qdf_mem_free(assoc_ind);
	}

	return QDF_STATUS_SUCCESS;
}
