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
 * This file lim_process_probe_req_frame.cc contains the code
 * for processing Probe Request Frame.
 * Author:        Chandra Modumudi
 * Date:          02/28/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wni_cfg.h"
#include "ani_global.h"

#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_ser_des_utils.h"
#include "parser_api.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "wlan_utility.h"

void

lim_send_sme_probe_req_ind(struct mac_context *mac,
			   tSirMacAddr peerMacAddr,
			   uint8_t *pProbeReqIE,
			   uint32_t ProbeReqIELen, struct pe_session *pe_session);

/**
 * lim_remove_timeout_pbc_sessions() - remove pbc probe req entries.
 * @mac - Pointer to Global MAC structure
 * @pbc - The beginning entry in WPS PBC probe request link list
 *
 * This function is called to remove the WPS PBC probe request entries from
 * specific entry to end.
 *
 * Return - None
 */
static void lim_remove_timeout_pbc_sessions(struct mac_context *mac,
					    tSirWPSPBCSession *pbc)
{
	tSirWPSPBCSession *prev;

	while (pbc) {
		prev = pbc;
		pbc = pbc->next;
		pe_debug("WPS PBC sessions remove");
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   prev->addr.bytes, QDF_MAC_ADDR_SIZE);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   prev->uuid_e, SIR_WPS_UUID_LEN);

		qdf_mem_free(prev);
	}
}

/**
 * lim_update_pbc_session_entry
 *
 ***FUNCTION:
 * This function is called when probe request with WPS PBC IE is received
 *
 ***LOGIC:
 * This function add the WPS PBC probe request in the WPS PBC probe request link list
 * The link list is in decreased time order of probe request that is received.
 * The entry that is more than 120 second is removed.
 *
 ***ASSUMPTIONS:
 *
 *
 ***NOTE:
 *
 * @param  mac   Pointer to Global MAC structure
 * @param  addr   A pointer to probe request source MAC address
 * @param  uuid_e A pointer to UUIDE element of WPS IE
 * @param  pe_session   A pointer to station PE session
 *
 * @return None
 */

static void lim_update_pbc_session_entry(struct mac_context *mac,
					 uint8_t *addr, uint8_t *uuid_e,
					 struct pe_session *pe_session)
{
	tSirWPSPBCSession *pbc, *prev = NULL;

	uint32_t curTime;

	curTime =
		(uint32_t) (qdf_mc_timer_get_system_ticks() /
			    QDF_TICKS_PER_SECOND);

	pe_debug("Receive WPS probe reques curTime: %d", curTime);
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   addr, QDF_MAC_ADDR_SIZE);
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   uuid_e, SIR_WPS_UUID_LEN);

	pbc = pe_session->pAPWPSPBCSession;

	while (pbc) {
		if ((!qdf_mem_cmp
			    ((uint8_t *) pbc->addr.bytes, (uint8_t *) addr,
			    QDF_MAC_ADDR_SIZE))
		    && (!qdf_mem_cmp((uint8_t *) pbc->uuid_e,
				       (uint8_t *) uuid_e, SIR_WPS_UUID_LEN))) {
			if (prev)
				prev->next = pbc->next;
			else
				pe_session->pAPWPSPBCSession = pbc->next;
			break;
		}
		prev = pbc;
		pbc = pbc->next;
	}

	if (!pbc) {
		pbc = qdf_mem_malloc(sizeof(tSirWPSPBCSession));
		if (!pbc)
			return;
		qdf_mem_copy((uint8_t *) pbc->addr.bytes, (uint8_t *) addr,
			     QDF_MAC_ADDR_SIZE);

		if (uuid_e)
			qdf_mem_copy((uint8_t *) pbc->uuid_e,
				     (uint8_t *) uuid_e, SIR_WPS_UUID_LEN);
	}

	pbc->next = pe_session->pAPWPSPBCSession;
	pe_session->pAPWPSPBCSession = pbc;
	pbc->timestamp = curTime;

	/* remove entries that have timed out */
	prev = pbc;
	pbc = pbc->next;

	while (pbc) {
		if (curTime > pbc->timestamp + SIR_WPS_PBC_WALK_TIME) {
			prev->next = NULL;
			lim_remove_timeout_pbc_sessions(mac, pbc);
			break;
		}
		prev = pbc;
		pbc = pbc->next;
	}
}

/**
 * lim_wpspbc_close
 *
 ***FUNCTION:
 * This function is called when BSS is closed
 *
 ***LOGIC:
 * This function remove all the WPS PBC entries
 *
 ***ASSUMPTIONS:
 *
 *
 ***NOTE:
 *
 * @param  mac   Pointer to Global MAC structure
 * @param  pe_session   A pointer to station PE session
 *
 * @return None
 */

void lim_wpspbc_close(struct mac_context *mac, struct pe_session *pe_session)
{

	lim_remove_timeout_pbc_sessions(mac, pe_session->pAPWPSPBCSession);

}

/**
 * lim_check11b_rates
 *
 ***FUNCTION:
 * This function is called by lim_process_probe_req_frame() upon
 * Probe Request frame reception.
 *
 ***LOGIC:
 * This function check 11b rates in supportedRates and extendedRates rates
 *
 ***NOTE:
 *
 * @param  rate
 *
 * @return BOOLEAN
 */

static bool lim_check11b_rates(uint8_t rate)
{
	if ((0x02 == (rate))
	    || (0x04 == (rate))
	    || (0x0b == (rate))
	    || (0x16 == (rate))
	    ) {
		return true;
	}
	return false;
}

/**
 * lim_process_probe_req_frame: to process probe req frame
 * @mac_ctx: Pointer to Global MAC structure
 * @rx_pkt_info: A pointer to Buffer descriptor + associated PDUs
 * @session: a ponter to session entry
 *
 * This function is called by limProcessMessageQueue() upon
 * Probe Request frame reception. This function processes received
 * Probe Request frame and responds with Probe Response.
 * Only AP or STA in IBSS mode that sent last Beacon will respond to
 * Probe Request.
 * ASSUMPTIONS:
 * 1. AP or STA in IBSS mode that sent last Beacon will always respond
 *    to Probe Request received with broadcast SSID.
 * NOTE:
 * 1. Dunno what to do with Rates received in Probe Request frame
 * 2. Frames with out-of-order fields/IEs are dropped.
 *
 *
 * Return: none
 */

void
lim_process_probe_req_frame(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
		struct pe_session *session)
{
	uint8_t *body_ptr;
	tpSirMacMgmtHdr mac_hdr;
	uint32_t frame_len;
	tSirProbeReq probe_req;
	tAniSSID ssid;

	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	if (LIM_IS_AP_ROLE(session)) {
		frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

		pe_debug("Received Probe Request: %d bytes from",
			frame_len);
			lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGD);
		/* Get pointer to Probe Request frame body */
		body_ptr = WMA_GET_RX_MPDU_DATA(rx_pkt_info);

		/* check for vendor IE presence */
		if ((session->access_policy_vendor_ie) &&
			(session->access_policy ==
			LIM_ACCESS_POLICY_RESPOND_IF_IE_IS_PRESENT)) {
			if (!wlan_get_vendor_ie_ptr_from_oui(
				&session->access_policy_vendor_ie[2],
				3, body_ptr, frame_len)) {
				pe_warn("Vendor IE is not present and access policy is: %x dropping probe request",
					session->access_policy);
				return;
			}
		}

		/* Parse Probe Request frame */
		if (sir_convert_probe_req_frame2_struct(mac_ctx, body_ptr,
				frame_len, &probe_req) == QDF_STATUS_E_FAILURE) {
			pe_err("Parse error ProbeReq, length: %d, SA is: "
					QDF_MAC_ADDR_FMT, frame_len,
					QDF_MAC_ADDR_REF(mac_hdr->sa));
			return;
		}
		if (session->opmode == QDF_P2P_GO_MODE) {
			uint8_t i = 0, rate_11b = 0, other_rates = 0;
			/* Check 11b rates in supported rates */
			for (i = 0; i < probe_req.supportedRates.numRates;
				i++) {
				if (lim_check11b_rates(
					probe_req.supportedRates.rate[i] &
								0x7f))
					rate_11b++;
				else
					other_rates++;
			}

			/* Check 11b rates in extended rates */
			for (i = 0; i < probe_req.extendedRates.numRates; i++) {
				if (lim_check11b_rates(
					probe_req.extendedRates.rate[i] & 0x7f))
					rate_11b++;
				else
					other_rates++;
			}

			if ((rate_11b > 0) && (other_rates == 0)) {
				pe_debug("Received a probe req frame with only 11b rates, SA is: ");
					lim_print_mac_addr(mac_ctx,
						mac_hdr->sa, LOGD);
					return;
			}
		}
		if (LIM_IS_AP_ROLE(session) &&
			((session->APWPSIEs.SirWPSProbeRspIE.FieldPresent
				& SIR_WPS_PROBRSP_VER_PRESENT)
			&& (probe_req.wscIePresent == 1)
			&& (probe_req.probeReqWscIeInfo.DevicePasswordID.id ==
				WSC_PASSWD_ID_PUSH_BUTTON)
			&& (probe_req.probeReqWscIeInfo.UUID_E.present == 1))) {
			if (session->fwdWPSPBCProbeReq) {
				QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE,
						   QDF_TRACE_LEVEL_DEBUG,
						   mac_hdr->sa,
						   QDF_MAC_ADDR_SIZE);
				QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE,
						   QDF_TRACE_LEVEL_DEBUG,
						   body_ptr, frame_len);
				lim_send_sme_probe_req_ind(mac_ctx, mac_hdr->sa,
					body_ptr, frame_len, session);
			} else {
				lim_update_pbc_session_entry(mac_ctx,
					mac_hdr->sa,
					probe_req.probeReqWscIeInfo.UUID_E.uuid,
					session);
			}
		}
		ssid.length = session->ssId.length;
		/* Copy the SSID from sessio entry to local variable */
		qdf_mem_copy(ssid.ssId, session->ssId.ssId,
				session->ssId.length);

		/*
		 * Compare received SSID with current SSID. If they match,
		 * reply with Probe Response
		 */
		if (probe_req.ssId.length) {
			if (!ssid.length)
				goto multipleSSIDcheck;

			if (!qdf_mem_cmp((uint8_t *) &ssid,
						(uint8_t *) &(probe_req.ssId),
						(uint8_t) (ssid.length + 1))) {
				lim_send_probe_rsp_mgmt_frame(mac_ctx,
						mac_hdr->sa, &ssid,
						session,
						probe_req.p2pIePresent);
				return;
			} else if (session->opmode ==
					QDF_P2P_GO_MODE) {
				uint8_t direct_ssid[7] = "DIRECT-";
				uint8_t direct_ssid_len = 7;

				if (!qdf_mem_cmp((uint8_t *) &direct_ssid,
					(uint8_t *) &(probe_req.ssId.ssId),
					(uint8_t) (direct_ssid_len))) {
					lim_send_probe_rsp_mgmt_frame(mac_ctx,
							mac_hdr->sa,
							&ssid,
							session,
							probe_req.p2pIePresent);
					return;
				}
			} else {
				pe_debug("Ignore ProbeReq frm with unmatch SSID received from");
					lim_print_mac_addr(mac_ctx, mac_hdr->sa,
						LOGD);
			}
		} else {
			/*
			 * Broadcast SSID in the Probe Request.
			 * Reply with SSID we're configured with.
			 * Turn off the SSID length to 0 if hidden SSID feature
			 * is present
			 */
			if (session->ssidHidden)
				/*
				 * We are returning from here as probe request
				 * contains the broadcast SSID. So no need to
				 * send the probe resp
				 */
				return;
			lim_send_probe_rsp_mgmt_frame(mac_ctx, mac_hdr->sa,
					&ssid,
					session,
					probe_req.p2pIePresent);
			return;
		}
multipleSSIDcheck:
		pe_debug("Ignore ProbeReq frm with unmatch SSID rcved from");
			lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGD);
	} else {
		/* Ignore received Probe Request frame */
		pe_debug("Ignoring Probe Request frame received from");
		lim_print_mac_addr(mac_ctx, mac_hdr->sa, LOGD);
	}
	return;
}

/**
 * lim_indicate_probe_req_to_hdd
 *
 ***FUNCTION:
 * This function is called by lim_process_probe_req_frame_multiple_bss() upon
 * Probe Request frame reception.
 *
 ***LOGIC:
 * This function processes received Probe Request frame and Pass
 * Probe Request Frame to HDD.
 *
 * @param  mac              Pointer to Global MAC structure
 * @param  *pBd              A pointer to Buffer descriptor + associated PDUs
 * @param  pe_session     A pointer to PE session
 *
 * @return None
 */

static void
lim_indicate_probe_req_to_hdd(struct mac_context *mac, uint8_t *pBd,
			      struct pe_session *pe_session)
{
	tpSirMacMgmtHdr pHdr;
	uint32_t frameLen;

	pe_debug("Received a probe request frame");

	pHdr = WMA_GET_RX_MAC_HEADER(pBd);
	frameLen = WMA_GET_RX_PAYLOAD_LEN(pBd);

	/* send the probe req to SME. */
	lim_send_sme_mgmt_frame_ind(mac, pHdr->fc.subType,
				    (uint8_t *) pHdr,
				    (frameLen + sizeof(tSirMacMgmtHdr)),
				    pe_session->smeSessionId,
				    WMA_GET_RX_FREQ(pBd),
				    pe_session,
				    WMA_GET_RX_RSSI_NORMALIZED(pBd),
				    RXMGMT_FLAG_NONE);
} /*** end lim_indicate_probe_req_to_hdd() ***/

/**
 * lim_process_probe_req_frame_multiple_bss() - to process probe req
 * @mac_ctx: Pointer to Global MAC structure
 * @buf_descr: A pointer to Buffer descriptor + associated PDUs
 * @session: A pointer to PE session
 *
 * This function is called by limProcessMessageQueue() upon
 * Probe Request frame reception. This function call
 * lim_indicate_probe_req_to_hdd function to indicate
 * Probe Request frame to HDD. It also call lim_process_probe_req_frame
 * function which process received Probe Request frame and responds
 * with Probe Response.
 *
 * @return None
 */
void
lim_process_probe_req_frame_multiple_bss(struct mac_context *mac_ctx,
			uint8_t *buf_descr, struct pe_session *session)
{
	uint8_t i;

	if (session) {
		if (LIM_IS_AP_ROLE(session)) {
			lim_indicate_probe_req_to_hdd(mac_ctx,
					buf_descr, session);
		}
		lim_process_probe_req_frame(mac_ctx, buf_descr, session);
		return;
	}

	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		session = pe_find_session_by_session_id(mac_ctx, i);
		if (!session)
			continue;
		if (LIM_IS_AP_ROLE(session))
			lim_indicate_probe_req_to_hdd(mac_ctx,
					buf_descr, session);
		if (LIM_IS_AP_ROLE(session))
			lim_process_probe_req_frame(mac_ctx,
					buf_descr, session);
	}
}

/**
 * lim_send_sme_probe_req_ind()
 *
 ***FUNCTION:
 * This function is to send
 *  eWNI_SME_WPS_PBC_PROBE_REQ_IND message to host
 *
 ***PARAMS:
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * This function is used for sending  eWNI_SME_WPS_PBC_PROBE_REQ_IND
 * to host.
 *
 * @param peerMacAddr       Indicates the peer MAC addr that the probe request
 *                          is generated.
 * @param pProbeReqIE       pointer to RAW probe request IE
 * @param ProbeReqIELen     The length of probe request IE.
 * @param pe_session     A pointer to PE session
 *
 * @return None
 */
void
lim_send_sme_probe_req_ind(struct mac_context *mac,
			   tSirMacAddr peerMacAddr,
			   uint8_t *pProbeReqIE,
			   uint32_t ProbeReqIELen, struct pe_session *pe_session)
{
	tSirSmeProbeReqInd *pSirSmeProbeReqInd;
	struct scheduler_msg msgQ = {0};

	pSirSmeProbeReqInd = qdf_mem_malloc(sizeof(tSirSmeProbeReqInd));
	if (!pSirSmeProbeReqInd)
		return;

	msgQ.type = eWNI_SME_WPS_PBC_PROBE_REQ_IND;
	msgQ.bodyval = 0;
	msgQ.bodyptr = pSirSmeProbeReqInd;

	pSirSmeProbeReqInd->messageType = eWNI_SME_WPS_PBC_PROBE_REQ_IND;
	pSirSmeProbeReqInd->length = sizeof(*pSirSmeProbeReqInd);
	pSirSmeProbeReqInd->sessionId = pe_session->smeSessionId;

	qdf_mem_copy(pSirSmeProbeReqInd->bssid.bytes, pe_session->bssId,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(pSirSmeProbeReqInd->WPSPBCProbeReq.peer_macaddr.bytes,
		     peerMacAddr, QDF_MAC_ADDR_SIZE);

	MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
				pe_session->peSessionId, msgQ.type));

	if (ProbeReqIELen > sizeof(pSirSmeProbeReqInd->WPSPBCProbeReq.
	    probeReqIE)) {
		ProbeReqIELen = sizeof(pSirSmeProbeReqInd->WPSPBCProbeReq.
				       probeReqIE);
	}

	pSirSmeProbeReqInd->WPSPBCProbeReq.probeReqIELen =
		(uint16_t) ProbeReqIELen;
	qdf_mem_copy(pSirSmeProbeReqInd->WPSPBCProbeReq.probeReqIE, pProbeReqIE,
		     ProbeReqIELen);

	lim_sys_process_mmh_msg_api(mac, &msgQ);

} /*** end lim_send_sme_probe_req_ind() ***/
