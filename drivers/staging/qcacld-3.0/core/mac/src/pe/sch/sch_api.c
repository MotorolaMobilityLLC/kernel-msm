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
 * This file sch_api.cc contains functions related to the API exposed
 * by scheduler module
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "cds_api.h"
#include "ani_global.h"
#include "wni_cfg.h"

#include "sir_mac_prot_def.h"
#include "sir_mac_prop_exts.h"
#include "sir_common.h"

#include "lim_api.h"

#include "sch_api.h"

#include "lim_trace.h"
#include "lim_types.h"
#include "lim_utils.h"

#include "wma_types.h"

QDF_STATUS sch_send_beacon_req(struct mac_context *mac, uint8_t *beaconPayload,
			       uint16_t size, struct pe_session *pe_session,
			       enum sir_bcn_update_reason reason)
{
	struct scheduler_msg msgQ = {0};
	tpSendbeaconParams beaconParams = NULL;
	QDF_STATUS retCode;

	pe_debug("Indicating HAL to copy the beacon template [%d bytes] to memory, reason %d",
		size, reason);

	if (LIM_IS_AP_ROLE(pe_session) &&
	   (mac->sch.beacon_changed)) {
		retCode = lim_send_probe_rsp_template_to_hal(mac,
				pe_session,
				&pe_session->DefProbeRspIeBitmap[0]);
		if (QDF_STATUS_SUCCESS != retCode)
			pe_err("FAILED to send probe response template with retCode %d",
				retCode);
	}

	beaconParams = qdf_mem_malloc(sizeof(tSendbeaconParams));
	if (!beaconParams)
		return QDF_STATUS_E_NOMEM;

	msgQ.type = WMA_SEND_BEACON_REQ;

	/* No Dialog Token reqd, as a response is not solicited */
	msgQ.reserved = 0;

	/* Fill in tSendbeaconParams members */
	qdf_mem_copy(beaconParams->bssId, pe_session->bssId,
		     sizeof(pe_session->bssId));


	beaconParams->timIeOffset = pe_session->schBeaconOffsetBegin;
	if (pe_session->dfsIncludeChanSwIe) {
		beaconParams->csa_count_offset = mac->sch.csa_count_offset;
		beaconParams->ecsa_count_offset = mac->sch.ecsa_count_offset;
	}

	beaconParams->vdev_id = pe_session->smeSessionId;
	beaconParams->reason = reason;

	/* p2pIeOffset should be atleast greater than timIeOffset */
	if ((mac->sch.p2p_ie_offset != 0) &&
	    (mac->sch.p2p_ie_offset <
	     pe_session->schBeaconOffsetBegin)) {
		pe_err("Invalid p2pIeOffset:[%d]",
			mac->sch.p2p_ie_offset);
		QDF_ASSERT(0);
		qdf_mem_free(beaconParams);
		return QDF_STATUS_E_FAILURE;
	}
	beaconParams->p2pIeOffset = mac->sch.p2p_ie_offset;

	if (size > SIR_MAX_BEACON_SIZE) {
		pe_err("beacon size (%d) exceed host limit %d",
		       size, SIR_MAX_BEACON_SIZE);
		QDF_ASSERT(0);
		qdf_mem_free(beaconParams);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_copy(beaconParams->beacon, beaconPayload, size);

	beaconParams->beaconLength = (uint32_t) size;
	msgQ.bodyptr = beaconParams;
	msgQ.bodyval = 0;

	/* Keep a copy of recent beacon frame sent */

	/* free previous copy of the beacon */
	if (pe_session->beacon) {
		qdf_mem_free(pe_session->beacon);
	}

	pe_session->bcnLen = 0;
	pe_session->beacon = NULL;

	pe_session->beacon = qdf_mem_malloc(size);
	if (pe_session->beacon) {
		qdf_mem_copy(pe_session->beacon, beaconPayload, size);
		pe_session->bcnLen = size;
	}

	MTRACE(mac_trace_msg_tx(mac, pe_session->peSessionId, msgQ.type));
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode)
		pe_err("Posting SEND_BEACON_REQ to HAL failed, reason=%X",
			retCode);

	return retCode;
}

static uint32_t lim_remove_p2p_ie_from_add_ie(struct mac_context *mac,
					      struct pe_session *pe_session,
					      uint8_t *addIeWoP2pIe,
					      uint32_t *addnIELenWoP2pIe)
{
	uint32_t left = pe_session->add_ie_params.probeRespDataLen;
	uint8_t *ptr = pe_session->add_ie_params.probeRespData_buff;
	uint8_t elem_id, elem_len;
	uint32_t offset = 0;
	uint8_t eid = 0xDD;

	qdf_mem_copy(addIeWoP2pIe, ptr, left);
	*addnIELenWoP2pIe = left;

	if (addIeWoP2pIe) {
		while (left >= 2) {
			elem_id  = ptr[0];
			elem_len = ptr[1];
			left -= 2;
			if (elem_len > left) {
				pe_err("Invalid IEs");
				return QDF_STATUS_E_FAILURE;
			}
			if ((elem_id == eid) &&
				(!qdf_mem_cmp(&ptr[2],
					"\x50\x6f\x9a\x09", 4))) {
				left -= elem_len;
				ptr += (elem_len + 2);
				qdf_mem_copy(&addIeWoP2pIe[offset], ptr, left);
				*addnIELenWoP2pIe -= (2 + elem_len);
			} else {
				left -= elem_len;
				ptr += (elem_len + 2);
				offset += 2 + elem_len;
			}
		}
	}
	return QDF_STATUS_SUCCESS;
}

uint32_t lim_send_probe_rsp_template_to_hal(struct mac_context *mac,
					    struct pe_session *pe_session,
					    uint32_t *IeBitmap)
{
	struct scheduler_msg msgQ = {0};
	uint8_t *pFrame2Hal = pe_session->pSchProbeRspTemplate;
	tpSendProbeRespParams pprobeRespParams = NULL;
	uint32_t retCode = QDF_STATUS_E_FAILURE;
	uint32_t nPayload, nBytes = 0, nStatus;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t addnIEPresent = false;
	uint8_t *addIE = NULL;
	uint8_t *addIeWoP2pIe = NULL;
	uint32_t addnIELenWoP2pIe = 0;
	uint32_t retStatus;
	tDot11fIEExtCap extracted_extcap;
	bool extcap_present = false;
	tDot11fProbeResponse *prb_rsp_frm;
	QDF_STATUS status;
	uint16_t addn_ielen = 0;

	/* Check if probe response IE is present or not */
	addnIEPresent = (pe_session->add_ie_params.probeRespDataLen != 0);
	if (addnIEPresent) {
		/*
		* probe response template should not have P2P IE.
		* In case probe request has P2P IE or WPS IE, the
		* probe request will be forwarded to the Host and
		* Host will send the probe response. In other cases
		* FW will send the probe response. So, if the template
		* has P2P IE, the probe response sent to non P2P devices
		* by the FW, may also have P2P IE which will fail
		* P2P cert case 6.1.3
		*/
		addIeWoP2pIe = qdf_mem_malloc(pe_session->add_ie_params.
						probeRespDataLen);
		if (!addIeWoP2pIe)
			return QDF_STATUS_E_NOMEM;

		retStatus = lim_remove_p2p_ie_from_add_ie(mac, pe_session,
					addIeWoP2pIe, &addnIELenWoP2pIe);
		if (retStatus != QDF_STATUS_SUCCESS) {
			qdf_mem_free(addIeWoP2pIe);
			return QDF_STATUS_E_FAILURE;
		}

		/* Probe rsp IE available */
		/*need to check the data length */
		addIE = qdf_mem_malloc(addnIELenWoP2pIe);
		if (!addIE) {
			qdf_mem_free(addIeWoP2pIe);
			return QDF_STATUS_E_NOMEM;
		}
		addn_ielen = addnIELenWoP2pIe;

		if (addn_ielen <= WNI_CFG_PROBE_RSP_ADDNIE_DATA1_LEN &&
		    addn_ielen && (nBytes + addn_ielen) <= SIR_MAX_PACKET_SIZE)
			qdf_mem_copy(addIE, addIeWoP2pIe, addnIELenWoP2pIe);

		qdf_mem_free(addIeWoP2pIe);

		qdf_mem_zero((uint8_t *)&extracted_extcap,
			     sizeof(tDot11fIEExtCap));
		status = lim_strip_extcap_update_struct(mac, addIE,
				&addn_ielen, &extracted_extcap);
		if (QDF_STATUS_SUCCESS != status) {
			pe_debug("extcap not extracted");
		} else {
			extcap_present = true;
		}
	}

	/*
	 * Extcap IE now support variable length, merge Extcap IE from addn_ie
	 * may change the frame size. Therefore, MUST merge ExtCap IE before
	 * dot11f get packed payload size.
	 */
	prb_rsp_frm = &pe_session->probeRespFrame;
	if (extcap_present)
		lim_merge_extcap_struct(&prb_rsp_frm->ExtCap,
					&extracted_extcap,
					true);

	nStatus = dot11f_get_packed_probe_response_size(mac,
			&pe_session->probeRespFrame, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to calculate the packed size for a Probe Response (0x%08x)",
			nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fProbeResponse);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_err("There were warnings while calculating the packed size for a Probe Response (0x%08x)",
			nStatus);
	}

	nBytes += nPayload + sizeof(tSirMacMgmtHdr);

	if (addnIEPresent) {
		if ((nBytes + addn_ielen) <= SIR_MAX_PROBE_RESP_SIZE)
			nBytes += addn_ielen;
		else
			addnIEPresent = false;  /* Dont include the IE. */
	}

	/* Make sure we are not exceeding allocated len */
	if (nBytes > SIR_MAX_PROBE_RESP_SIZE) {
		pe_err("nBytes %d greater than max size", nBytes);
		qdf_mem_free(addIE);
		return QDF_STATUS_E_FAILURE;
	}

	/* Paranoia: */
	qdf_mem_zero(pFrame2Hal, nBytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac, pFrame2Hal, SIR_MAC_MGMT_FRAME,
					     SIR_MAC_MGMT_PROBE_RSP,
					     pe_session->self_mac_addr,
					     pe_session->self_mac_addr);

	pMacHdr = (tpSirMacMgmtHdr) pFrame2Hal;

	sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);

	/* That done, pack the Probe Response: */
	nStatus =
		dot11f_pack_probe_response(mac, &pe_session->probeRespFrame,
					   pFrame2Hal + sizeof(tSirMacMgmtHdr),
					   nPayload, &nPayload);

	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to pack a Probe Response (0x%08x)",
			nStatus);

		qdf_mem_free(addIE);
		return retCode; /* allocated! */
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while packing a P"
			"robe Response (0x%08x)", nStatus);
	}

	if (addnIEPresent) {
		qdf_mem_copy(&pFrame2Hal[nBytes - addn_ielen],
			     &addIE[0], addn_ielen);
	}

	qdf_mem_free(addIE);

	pprobeRespParams = qdf_mem_malloc(sizeof(tSendProbeRespParams));
	if (!pprobeRespParams) {
		pe_err("malloc failed for bytes %d", nBytes);
	} else {
		sir_copy_mac_addr(pprobeRespParams->bssId, pe_session->bssId);
		qdf_mem_copy(pprobeRespParams->probeRespTemplate,
			     pFrame2Hal, nBytes);
		pprobeRespParams->probeRespTemplateLen = nBytes;
		qdf_mem_copy(pprobeRespParams->ucProxyProbeReqValidIEBmap,
			     IeBitmap, (sizeof(uint32_t) * 8));
		msgQ.type = WMA_SEND_PROBE_RSP_TMPL;
		msgQ.reserved = 0;
		msgQ.bodyptr = pprobeRespParams;
		msgQ.bodyval = 0;

		retCode = wma_post_ctrl_msg(mac, &msgQ);
		if (QDF_STATUS_SUCCESS != retCode) {
			pe_err("FAIL bytes %d retcode[%X]", nBytes, retCode);
			qdf_mem_free(pprobeRespParams);
		} else {
			pe_debug("Probe response template msg posted to HAL of bytes %d",
				nBytes);
		}
	}

	return retCode;
}

/**
 * sch_gen_timing_advert_frame() - Generate the TA frame and populate the buffer
 * @mac: the global MAC context
 * @self_addr: the self MAC address
 * @buf: the buffer that will contain the frame
 * @timestamp_offset: return for the offset of the timestamp field
 * @time_value_offset: return for the time_value field in the TA IE
 *
 * Return: the length of the buffer.
 */
int sch_gen_timing_advert_frame(struct mac_context *mac_ctx, tSirMacAddr self_addr,
	uint8_t **buf, uint32_t *timestamp_offset, uint32_t *time_value_offset)
{
	tDot11fTimingAdvertisementFrame frame;
	uint32_t payload_size, buf_size;
	int status;
	struct qdf_mac_addr wildcard_bssid = {
		{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
	};

	qdf_mem_zero((uint8_t *)&frame, sizeof(tDot11fTimingAdvertisementFrame));

	/* Populate the TA fields */
	status = populate_dot11f_timing_advert_frame(mac_ctx, &frame);
	if (status) {
		pe_err("Error populating TA frame %x", status);
		return status;
	}

	status = dot11f_get_packed_timing_advertisement_frame_size(mac_ctx,
		&frame, &payload_size);
	if (DOT11F_FAILED(status)) {
		pe_err("Error getting packed frame size %x", status);
		return status;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("Warning getting packed frame size");
	}

	buf_size = sizeof(tSirMacMgmtHdr) + payload_size;
	*buf = qdf_mem_malloc(buf_size);
	if (!*buf)
		return QDF_STATUS_E_FAILURE;

	payload_size = 0;
	status = dot11f_pack_timing_advertisement_frame(mac_ctx, &frame,
		*buf + sizeof(tSirMacMgmtHdr), buf_size -
		sizeof(tSirMacMgmtHdr), &payload_size);
	pe_err("TA payload size2 = %d", payload_size);
	if (DOT11F_FAILED(status)) {
		pe_err("Error packing frame %x", status);
		goto fail;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("Warning packing frame");
	}

	lim_populate_mac_header(mac_ctx, *buf, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_TIME_ADVERT, wildcard_bssid.bytes, self_addr);

	/* The timestamp field is right after the header */
	*timestamp_offset = sizeof(tSirMacMgmtHdr);

	*time_value_offset = sizeof(tSirMacMgmtHdr) +
		sizeof(tDot11fFfTimeStamp) + sizeof(tDot11fFfCapabilities);

	/* Add the Country IE length */
	dot11f_get_packed_ie_country(mac_ctx, &frame.Country,
		time_value_offset);
	/* Add 2 for Country IE EID and Length fields */
	*time_value_offset += 2;

	/* Add the PowerConstraint IE size */
	if (frame.Country.present == 1)
		*time_value_offset += 3;

	/* Add the offset inside TA IE */
	*time_value_offset += 3;

	return payload_size + sizeof(tSirMacMgmtHdr);

fail:
	if (*buf)
		qdf_mem_free(*buf);
	return status;
}
