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
 * This file lim_process_beacon_frame.cc contains the code
 * for processing Received Beacon Frame.
 * Author:        Chandra Modumudi
 * Date:          03/01/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "wni_cfg.h"
#include "ani_global.h"
#include "sch_api.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_prop_exts_utils.h"
#include "lim_ser_des_utils.h"

/**
 * lim_process_beacon_frame() - to process beacon frames
 * @mac_ctx: Pointer to Global MAC structure
 * @rx_pkt_info: A pointer to RX packet info structure
 * @session: A pointer to session
 *
 * This function is called by limProcessMessageQueue() upon Beacon
 * frame reception.
 * Note:
 * 1. Beacons received in 'normal' state in IBSS are handled by
 *    Beacon Processing module.
 *
 * Return: none
 */

void
lim_process_beacon_frame(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
			 struct pe_session *session)
{
	tpSirMacMgmtHdr mac_hdr;
	tSchBeaconStruct *bcn_ptr;
	uint8_t *frame;
	const uint8_t *owe_transition_ie;
	uint16_t frame_len;

	mac_ctx->lim.gLimNumBeaconsRcvd++;

	/*
	 * here is it required to increment session specific heartBeat
	 * beacon counter
	 */
	mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	frame = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	frame_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);

	pe_debug("Beacon (len %d): " QDF_MAC_ADDR_FMT " RSSI %d",
		 WMA_GET_RX_MPDU_LEN(rx_pkt_info),
		 QDF_MAC_ADDR_REF(mac_hdr->sa),
		 (uint)abs((int8_t)
		 WMA_GET_RX_RSSI_NORMALIZED(rx_pkt_info)));

	/* Expect Beacon in any state as Scan is independent of LIM state */
	bcn_ptr = qdf_mem_malloc(sizeof(*bcn_ptr));
	if (!bcn_ptr)
		return;

	/* Parse received Beacon */
	if (sir_convert_beacon_frame2_struct(mac_ctx,
			rx_pkt_info, bcn_ptr) !=
			QDF_STATUS_SUCCESS) {
		/*
		 * Received wrongly formatted/invalid Beacon.
		 * Ignore it and move on.
		 */
		pe_warn("Received invalid Beacon in state: %X",
			session->limMlmState);
		lim_print_mlm_state(mac_ctx, LOGW,
			session->limMlmState);
		qdf_mem_free(bcn_ptr);
		return;
	}

	/*
	 * during scanning, when any session is active, and
	 * beacon/Pr belongs to one of the session, fill up the
	 * following, TBD - HB counter
	 */
	if (sir_compare_mac_addr(session->bssId,
				bcn_ptr->bssid)) {
		qdf_mem_copy((uint8_t *)&session->lastBeaconTimeStamp,
			(uint8_t *) bcn_ptr->timeStamp,
			sizeof(uint64_t));
		session->currentBssBeaconCnt++;
	}
	MTRACE(mac_trace(mac_ctx,
		TRACE_CODE_RX_MGMT_TSF, 0, bcn_ptr->timeStamp[0]));
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_RX_MGMT_TSF, 0,
		bcn_ptr->timeStamp[1]));

	if (session->limMlmState ==
			eLIM_MLM_WT_JOIN_BEACON_STATE) {
		owe_transition_ie = wlan_get_vendor_ie_ptr_from_oui(
					OWE_TRANSITION_OUI_TYPE,
					OWE_TRANSITION_OUI_SIZE,
					frame + SIR_MAC_B_PR_SSID_OFFSET,
					frame_len - SIR_MAC_B_PR_SSID_OFFSET);
		if (session->connected_akm == ANI_AKM_TYPE_OWE &&
		    owe_transition_ie) {
			pe_debug("vdev:%d Drop OWE rx beacon. Wait for probe for join success",
				 session->vdev_id);
			qdf_mem_free(bcn_ptr);
			return;
		}

		if (session->beacon) {
			qdf_mem_free(session->beacon);
			session->beacon = NULL;
			session->bcnLen = 0;
		}
		session->bcnLen = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);
		session->beacon = qdf_mem_malloc(session->bcnLen);
		if (session->beacon)
			/*
			 * Store the Beacon/ProbeRsp. This is sent to
			 * csr/hdd in join cnf response.
			 */
			qdf_mem_copy(session->beacon,
				WMA_GET_RX_MPDU_DATA(rx_pkt_info),
				session->bcnLen);

		lim_check_and_announce_join_success(mac_ctx, bcn_ptr,
				mac_hdr, session);
	}
	qdf_mem_free(bcn_ptr);
	return;
}
