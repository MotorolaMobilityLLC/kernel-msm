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

/**
 * \file lim_send_management_frames.c
 *
 * \brief Code for preparing and sending 802.11 Management frames
 *
 *
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
#include "qdf_crypto.h"

#include "wma_types.h"
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_peer_ops.h>
#include "lim_process_fils.h"
#include "wlan_utility.h"
#include <wlan_mlme_api.h>
#include <wlan_mlme_main.h>
#include "wlan_crypto_global_api.h"

/**
 *
 * \brief This function is called to add the sequence number to the
 * management frames
 *
 * \param  mac Pointer to Global MAC structure
 *
 * \param  pMacHdr Pointer to MAC management header
 *
 * The pMacHdr argument points to the MAC management header. The
 * sequence number stored in the mac structure will be incremented
 * and updated to the MAC management header. The start sequence
 * number is WLAN_HOST_SEQ_NUM_MIN and the end value is
 * WLAN_HOST_SEQ_NUM_MAX. After reaching the MAX value, the sequence
 * number will roll over.
 *
 */
static void lim_add_mgmt_seq_num(struct mac_context *mac, tpSirMacMgmtHdr pMacHdr)
{
	if (mac->mgmtSeqNum >= WLAN_HOST_SEQ_NUM_MAX) {
		mac->mgmtSeqNum = WLAN_HOST_SEQ_NUM_MIN - 1;
	}

	mac->mgmtSeqNum++;

	pMacHdr->seqControl.seqNumLo = (mac->mgmtSeqNum & LOW_SEQ_NUM_MASK);
	pMacHdr->seqControl.seqNumHi =
		((mac->mgmtSeqNum & HIGH_SEQ_NUM_MASK) >> HIGH_SEQ_NUM_OFFSET);
}

/**
 * lim_populate_mac_header() - Fill in 802.11 header of frame
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @buf: Pointer to the frame buffer that needs to be populate
 * @type: 802.11 Type of the frame
 * @sub_type: 802.11 Subtype of the frame
 * @peer_addr: dst address
 * @self_mac_addr: local mac address
 *
 * This function is called by various LIM modules to prepare the
 * 802.11 frame MAC header
 *
 * The buf argument points to the beginning of the frame buffer to
 * which - a) The 802.11 MAC header is set b) Following this MAC header
 * will be the MGMT frame payload The payload itself is populated by the
 * caller API
 *
 * Return: None
 */

void lim_populate_mac_header(struct mac_context *mac_ctx, uint8_t *buf,
		uint8_t type, uint8_t sub_type, tSirMacAddr peer_addr,
		tSirMacAddr self_mac_addr)
{
	tpSirMacMgmtHdr mac_hdr;

	/* Prepare MAC management header */
	mac_hdr = (tpSirMacMgmtHdr) (buf);

	/* Prepare FC */
	mac_hdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
	mac_hdr->fc.type = type;
	mac_hdr->fc.subType = sub_type;

	/* Prepare Address 1 */
	qdf_mem_copy((uint8_t *) mac_hdr->da,
		     (uint8_t *) peer_addr, sizeof(tSirMacAddr));

	/* Prepare Address 2 */
	sir_copy_mac_addr(mac_hdr->sa, self_mac_addr);

	/* Prepare Address 3 */
	qdf_mem_copy((uint8_t *) mac_hdr->bssId,
		     (uint8_t *) peer_addr, sizeof(tSirMacAddr));

	/* Prepare sequence number */
	lim_add_mgmt_seq_num(mac_ctx, mac_hdr);
	pe_debug("seqNumLo=%d, seqNumHi=%d, mgmtSeqNum=%d",
		mac_hdr->seqControl.seqNumLo,
		mac_hdr->seqControl.seqNumHi, mac_ctx->mgmtSeqNum);
}

/**
 * lim_send_probe_req_mgmt_frame() - send probe request management frame
 * @mac_ctx: Pointer to Global MAC structure
 * @ssid: SSID to be sent in Probe Request frame
 * @bssid: BSSID to be sent in Probe Request frame
 * @chan_freq: Channel frequency on which the Probe Request is going out
 * @self_macaddr: self MAC address
 * @dot11mode: self dotllmode
 * @additional_ielen: if non-zero, include additional_ie in the Probe Request
 *                   frame
 * @additional_ie: if additional_ielen is non zero, include this field in the
 *                Probe Request frame
 *
 * This function is called by various LIM modules to send Probe Request frame
 * during active scan/learn phase.
 * Probe request is sent out in the following scenarios:
 * --heartbeat failure:  session needed
 * --join req:           session needed
 * --foreground scan:    no session
 * --background scan:    no session
 * --sch_beacon_processing:  to get EDCA parameters:  session needed
 *
 * Return: QDF_STATUS (QDF_STATUS_SUCCESS on success and error codes otherwise)
 */
QDF_STATUS
lim_send_probe_req_mgmt_frame(struct mac_context *mac_ctx,
			      tSirMacSSid *ssid,
			      tSirMacAddr bssid,
			      qdf_freq_t chan_freq,
			      tSirMacAddr self_macaddr,
			      uint32_t dot11mode,
			      uint16_t *additional_ielen,
			      uint8_t *additional_ie)
{
	tDot11fProbeRequest *pr;
	uint32_t status, bytes, payload;
	uint8_t *frame;
	void *packet;
	QDF_STATUS qdf_status;
	struct pe_session *pesession;
	uint8_t sessionid;
	const uint8_t *p2pie = NULL;
	uint8_t txflag = 0;
	uint8_t vdev_id = 0;
	bool is_vht_enabled = false;
	uint8_t txPower;
	uint16_t addn_ielen = 0;
	bool extracted_ext_cap_flag = false;
	tDot11fIEExtCap extracted_ext_cap;
	QDF_STATUS sir_status;
	const uint8_t *qcn_ie = NULL;
	uint8_t channel;

	if (additional_ielen)
		addn_ielen = *additional_ielen;

	channel = wlan_reg_freq_to_chan(mac_ctx->pdev, chan_freq);
	/*
	 * The probe req should not send 11ac capabilities if band is
	 * 2.4GHz, unless gEnableVhtFor24GHzBand is enabled in INI. So
	 * if gEnableVhtFor24GHzBand is false and dot11mode is 11ac
	 * set it to 11n.
	 */
	if (wlan_reg_is_24ghz_ch_freq(chan_freq) &&
	    !mac_ctx->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band &&
	    (MLME_DOT11_MODE_11AC == dot11mode ||
	     MLME_DOT11_MODE_11AC_ONLY == dot11mode))
		dot11mode = MLME_DOT11_MODE_11N;
	/*
	 * session context may or may not be present, when probe request needs
	 * to be sent out. Following cases exist:
	 * --heartbeat failure:  session needed
	 * --join req:           session needed
	 * --foreground scan:    no session
	 * --background scan:    no session
	 * --sch_beacon_processing:  to get EDCA parameters:  session needed
	 * If session context does not exist, some IEs will be populated from
	 * CFGs, e.g. Supported and Extended rate set IEs
	 */
	pesession = pe_find_session_by_bssid(mac_ctx, bssid, &sessionid);

	if (pesession)
		vdev_id = pesession->vdev_id;

	pr = qdf_mem_malloc(sizeof(*pr));
	if (!pr)
		return QDF_STATUS_E_NOMEM;

	/* The scheme here is to fill out a 'tDot11fProbeRequest' structure */
	/* and then hand it off to 'dot11f_pack_probe_request' (for */
	/* serialization). */

	/* & delegating to assorted helpers: */
	populate_dot11f_ssid(mac_ctx, ssid, &pr->SSID);

	if (addn_ielen && additional_ie)
		p2pie = limGetP2pIEPtr(mac_ctx, additional_ie, addn_ielen);

	/*
	 * Don't include 11b rate if it is a P2P serach or probe request is
	 * sent by P2P Client
	 */
	if ((MLME_DOT11_MODE_11B != dot11mode) && (p2pie) &&
	    ((pesession) && (QDF_P2P_CLIENT_MODE == pesession->opmode))) {
		/*
		 * In the below API pass channel number > 14, do that it fills
		 * only 11a rates in supported rates
		 */
		populate_dot11f_supp_rates(mac_ctx, 15, &pr->SuppRates,
					   pesession);
	} else {
		populate_dot11f_supp_rates(mac_ctx, channel,
					   &pr->SuppRates, pesession);

		if (MLME_DOT11_MODE_11B != dot11mode) {
			populate_dot11f_ext_supp_rates1(mac_ctx, channel,
							&pr->ExtSuppRates);
		}
	}

	/*
	 * Table 7-14 in IEEE Std. 802.11k-2008 says
	 * DS params "can" be present in RRM is disabled and "is" present if
	 * RRM is enabled. It should be ok even if we add it into probe req when
	 * RRM is not enabled.
	 */
	populate_dot11f_ds_params(mac_ctx, &pr->DSParams,
				  chan_freq);
	/* Call RRM module to get the tx power for management used. */
	txPower = (uint8_t) rrm_get_mgmt_tx_power(mac_ctx, pesession);
	populate_dot11f_wfatpc(mac_ctx, &pr->WFATPC, txPower, 0);

	if (pesession) {
		/* Include HT Capability IE */
		if (pesession->htCapability &&
		    !(WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_freq)))
			populate_dot11f_ht_caps(mac_ctx, pesession,
						&pr->HTCaps);
	} else {                /* !pesession */
		if (IS_DOT11_MODE_HT(dot11mode) &&
		    !(WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_freq)))
			populate_dot11f_ht_caps(mac_ctx, NULL, &pr->HTCaps);
	}

	/*
	 * Set channelbonding information as "disabled" when tunned to a
	 * 2.4 GHz channel
	 */
	if (wlan_reg_is_24ghz_ch_freq(chan_freq)) {
		if (mac_ctx->roam.configParam.channelBondingMode24GHz
		    == PHY_SINGLE_CHANNEL_CENTERED) {
			pr->HTCaps.supportedChannelWidthSet =
				eHT_CHANNEL_WIDTH_20MHZ;
			pr->HTCaps.shortGI40MHz = 0;
		} else {
			pr->HTCaps.supportedChannelWidthSet =
				eHT_CHANNEL_WIDTH_40MHZ;
		}
	}
	if (pesession) {
		/* Include VHT Capability IE */
		if (pesession->vhtCapability &&
		    !(WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_freq))) {
			populate_dot11f_vht_caps(mac_ctx, pesession,
						 &pr->VHTCaps);
			is_vht_enabled = true;
		}
	} else {
		if (IS_DOT11_MODE_VHT(dot11mode) &&
		    !(WLAN_REG_IS_6GHZ_CHAN_FREQ(chan_freq))) {
			populate_dot11f_vht_caps(mac_ctx, pesession,
						 &pr->VHTCaps);
			is_vht_enabled = true;
		}
	}
	if (pesession)
		populate_dot11f_ext_cap(mac_ctx, is_vht_enabled, &pr->ExtCap,
					pesession);

	if (IS_DOT11_MODE_HE(dot11mode) && pesession)
		lim_update_session_he_capable(mac_ctx, pesession);

	populate_dot11f_he_caps(mac_ctx, pesession, &pr->he_cap);
	populate_dot11f_he_6ghz_cap(mac_ctx, pesession,
				    &pr->he_6ghz_band_cap);

	if (addn_ielen && additional_ie) {
		qdf_mem_zero((uint8_t *)&extracted_ext_cap,
			sizeof(tDot11fIEExtCap));
		sir_status = lim_strip_extcap_update_struct(mac_ctx,
					additional_ie,
					&addn_ielen,
					&extracted_ext_cap);
		if (QDF_STATUS_SUCCESS != sir_status) {
			pe_debug("Unable to Stripoff ExtCap IE from Probe Req");
		} else {
			struct s_ext_cap *p_ext_cap =
				(struct s_ext_cap *)
					extracted_ext_cap.bytes;
			if (p_ext_cap->interworking_service)
				p_ext_cap->qos_map = 1;
			extracted_ext_cap.num_bytes =
				lim_compute_ext_cap_ie_length
					(&extracted_ext_cap);
			extracted_ext_cap_flag =
				(extracted_ext_cap.num_bytes > 0);
			if (additional_ielen)
				*additional_ielen = addn_ielen;
		}
		qcn_ie = wlan_get_vendor_ie_ptr_from_oui(SIR_MAC_QCN_OUI_TYPE,
				SIR_MAC_QCN_OUI_TYPE_SIZE,
				additional_ie, addn_ielen);
	}

	/* Add qcn_ie only if qcn ie is not present in additional_ie */
	if (pesession) {
		if (!qcn_ie)
			populate_dot11f_qcn_ie(mac_ctx, pesession,
					       &pr->qcn_ie,
					       QCN_IE_ATTR_ID_ALL);
		else
			populate_dot11f_qcn_ie(mac_ctx, pesession,
					       &pr->qcn_ie,
					       QCN_IE_ATTR_ID_VHT_MCS11);
	}

	/*
	 * Extcap IE now support variable length, merge Extcap IE from addn_ie
	 * may change the frame size. Therefore, MUST merge ExtCap IE before
	 * dot11f get packed payload size.
	 */
	if (extracted_ext_cap_flag)
		lim_merge_extcap_struct(&pr->ExtCap, &extracted_ext_cap, true);

	/* That's it-- now we pack it.  First, how much space are we going to */
	status = dot11f_get_packed_probe_request_size(mac_ctx, pr, &payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to calculate the packed size for a Probe Request (0x%08x)",
			status);
		/* We'll fall back on the worst case scenario: */
		payload = sizeof(tDot11fProbeRequest);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while calculating the packed size for a Probe Request (0x%08x)",
			status);
	}

	bytes = payload + sizeof(tSirMacMgmtHdr) + addn_ielen;

	/* Ok-- try to allocate some memory: */
	qdf_status = cds_packet_alloc((uint16_t) bytes, (void **)&frame,
				      (void **)&packet);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a Probe Request", bytes);
		qdf_mem_free(pr);
		return QDF_STATUS_E_NOMEM;
	}
	/* Paranoia: */
	qdf_mem_zero(frame, bytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac_ctx, frame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_PROBE_REQ, bssid, self_macaddr);

	/* That done, pack the Probe Request: */
	status = dot11f_pack_probe_request(mac_ctx, pr, frame +
					    sizeof(tSirMacMgmtHdr),
					    payload, &payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack a Probe Request (0x%08x)", status);
		cds_packet_free((void *)packet);
		return QDF_STATUS_E_FAILURE;    /* allocated! */
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing a Probe Request (0x%08x)", status);
	}

	qdf_mem_free(pr);

	/* Append any AddIE if present. */
	if (addn_ielen) {
		qdf_mem_copy(frame + sizeof(tSirMacMgmtHdr) + payload,
			     additional_ie, addn_ielen);
		payload += addn_ielen;
	}
	pe_nofl_debug("Probe req TX: vdev %d seq num %d to " QDF_MAC_ADDR_FMT " len %d",
		      vdev_id, mac_ctx->mgmtSeqNum,
		      QDF_MAC_ADDR_REF(bssid),
		      (int)sizeof(tSirMacMgmtHdr) + payload);

	/* If this probe request is sent during P2P Search State, then we need
	 * to send it at OFDM rate.
	 */
	if ((REG_BAND_5G == lim_get_rf_band(chan_freq)) ||
		/*
		 * For unicast probe req mgmt from Join function we don't set
		 * above variables. So we need to add one more check whether it
		 * is opmode is P2P_CLIENT or not
		 */
	    ((pesession) && (QDF_P2P_CLIENT_MODE == pesession->opmode)))
		txflag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	qdf_status =
		wma_tx_frame(mac_ctx, packet,
			   (uint16_t) sizeof(tSirMacMgmtHdr) + payload,
			   TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS, 7,
			   lim_tx_complete, frame, txflag, vdev_id,
			   0, RATEID_DEFAULT, 0);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("could not send Probe Request frame!");
		/* Pkt will be freed up by the callback */
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
} /* End lim_send_probe_req_mgmt_frame. */

static QDF_STATUS lim_get_addn_ie_for_probe_resp(struct mac_context *mac,
					     uint8_t *addIE, uint16_t *addnIELen,
					     uint8_t probeReqP2pIe)
{
	/* If Probe request doesn't have P2P IE, then take out P2P IE
	   from additional IE */
	if (!probeReqP2pIe) {
		uint8_t *tempbuf = NULL;
		uint16_t tempLen = 0;
		int left = *addnIELen;
		uint8_t *ptr = addIE;
		uint8_t elem_id, elem_len;

		if (!addIE) {
			pe_err("NULL addIE pointer");
			return QDF_STATUS_E_FAILURE;
		}

		tempbuf = qdf_mem_malloc(left);
		if (!tempbuf)
			return QDF_STATUS_E_NOMEM;

		while (left >= 2) {
			elem_id = ptr[0];
			elem_len = ptr[1];
			left -= 2;
			if (elem_len > left) {
				pe_err("Invalid IEs eid: %d elem_len: %d left: %d",
					elem_id, elem_len, left);
				qdf_mem_free(tempbuf);
				return QDF_STATUS_E_FAILURE;
			}
			if (!((WLAN_ELEMID_VENDOR == elem_id) &&
			      (memcmp
				       (&ptr[2], SIR_MAC_P2P_OUI,
				       SIR_MAC_P2P_OUI_SIZE) == 0))) {
				qdf_mem_copy(tempbuf + tempLen, &ptr[0],
					     elem_len + 2);
				tempLen += (elem_len + 2);
			}
			left -= elem_len;
			ptr += (elem_len + 2);
		}
		qdf_mem_copy(addIE, tempbuf, tempLen);
		*addnIELen = tempLen;
		qdf_mem_free(tempbuf);
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * lim_add_additional_ie() - Add additional IE to management frame
 * @frame:          pointer to frame
 * @frame_offset:   current offset of frame
 * @add_ie:         pointer to addtional ie
 * @add_ie_len:     length of addtional ie
 * @p2p_ie:         pointer to p2p ie
 * @noa_ie:         pointer to noa ie, this is seperate p2p ie
 * @noa_ie_len:     length of noa ie
 * @noa_stream:     pointer to noa stream, this is noa attribute only
 * @noa_stream_len: length of noa stream
 *
 * This function adds additional IE to management frame.
 *
 * Return: None
 */
static void lim_add_additional_ie(uint8_t *frame, uint32_t frame_offset,
				  uint8_t *add_ie, uint32_t add_ie_len,
				  uint8_t *p2p_ie, uint8_t *noa_ie,
				  uint32_t noa_ie_len, uint8_t *noa_stream,
				  uint32_t noa_stream_len) {
	uint16_t p2p_ie_offset;

	if (!add_ie_len || !add_ie) {
		pe_debug("no valid addtional ie");
		return;
	}

	if (!noa_stream_len) {
		qdf_mem_copy(frame + frame_offset, &add_ie[0], add_ie_len);
		return;
	}

	if (noa_ie_len > (SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN)) {
		pe_err("Not able to insert NoA, len=%d", noa_ie_len);
		return;
	} else if (noa_ie_len > 0) {
		pe_debug("new p2p ie for noa attr");
		qdf_mem_copy(frame + frame_offset, &add_ie[0], add_ie_len);
		frame_offset += add_ie_len;
		qdf_mem_copy(frame + frame_offset, &noa_ie[0], noa_ie_len);
	} else {
		if (!p2p_ie || (p2p_ie < add_ie) ||
		    (p2p_ie > (add_ie + add_ie_len))) {
			pe_err("invalid p2p ie");
			return;
		}
		p2p_ie_offset = p2p_ie - add_ie + p2p_ie[1] + 2;
		if (p2p_ie_offset > add_ie_len) {
			pe_err("Invalid p2p ie");
			return;
		}
		pe_debug("insert noa attr to existed p2p ie");
		p2p_ie[1] = p2p_ie[1] + noa_stream_len;
		qdf_mem_copy(frame + frame_offset, &add_ie[0], p2p_ie_offset);
		frame_offset += p2p_ie_offset;
		qdf_mem_copy(frame + frame_offset, &noa_stream[0],
			     noa_stream_len);
		if (p2p_ie_offset < add_ie_len) {
			frame_offset += noa_stream_len;
			qdf_mem_copy(frame + frame_offset,
				     &add_ie[p2p_ie_offset],
				     add_ie_len - p2p_ie_offset);
		}
	}
}

void
lim_send_probe_rsp_mgmt_frame(struct mac_context *mac_ctx,
			      tSirMacAddr peer_macaddr,
			      tpAniSSID ssid,
			      struct pe_session *pe_session,
			      uint8_t preq_p2pie)
{
	tDot11fProbeResponse *frm;
	QDF_STATUS sir_status;
	uint32_t cfg, payload, bytes = 0, status;
	tpSirMacMgmtHdr mac_hdr;
	uint8_t *frame;
	void *packet = NULL;
	QDF_STATUS qdf_status;
	uint32_t addn_ie_present = false;

	uint16_t addn_ie_len = 0;
	bool wps_ap = 0;
	uint8_t tx_flag = 0;
	uint8_t *add_ie = NULL;
	uint8_t *p2p_ie = NULL;
	uint8_t noalen = 0;
	uint8_t total_noalen = 0;
	uint8_t noa_stream[SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN];
	uint8_t noa_ie[SIR_MAX_NOA_ATTR_LEN + SIR_P2P_IE_HEADER_LEN];
	uint8_t vdev_id = 0;
	bool is_vht_enabled = false;
	tDot11fIEExtCap extracted_ext_cap = {0};
	bool extracted_ext_cap_flag = false;

	/* We don't answer requests in this case*/
	if (ANI_DRIVER_TYPE(mac_ctx) == QDF_DRIVER_TYPE_MFG)
		return;

	if (!pe_session)
		return;

	/*
	 * In case when cac timer is running for this SAP session then
	 * avoid sending probe rsp out. It is violation of dfs specification.
	 */
	if (((pe_session->opmode == QDF_SAP_MODE) ||
	    (pe_session->opmode == QDF_P2P_GO_MODE)) &&
	    (true == mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running)) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO,
			  FL("CAC timer is running, probe response dropped"));
		return;
	}
	vdev_id = pe_session->vdev_id;
	frm = qdf_mem_malloc(sizeof(tDot11fProbeResponse));
	if (!frm)
		return;

	/*
	 * Fill out 'frm', after which we'll just hand the struct off to
	 * 'dot11f_pack_probe_response'.
	 */
	qdf_mem_zero((uint8_t *) frm, sizeof(tDot11fProbeResponse));

	/*
	 * Timestamp to be updated by TFP, below.
	 *
	 * Beacon Interval:
	 */
	if (LIM_IS_AP_ROLE(pe_session)) {
		frm->BeaconInterval.interval =
			mac_ctx->sch.beacon_interval;
	} else {
		cfg = mac_ctx->mlme_cfg->sap_cfg.beacon_interval;
		frm->BeaconInterval.interval = (uint16_t) cfg;
	}

	populate_dot11f_capabilities(mac_ctx, &frm->Capabilities, pe_session);
	populate_dot11f_ssid(mac_ctx, (tSirMacSSid *) ssid, &frm->SSID);
	populate_dot11f_supp_rates(mac_ctx, POPULATE_DOT11F_RATES_OPERATIONAL,
		&frm->SuppRates, pe_session);

	populate_dot11f_ds_params(
		mac_ctx, &frm->DSParams,
		pe_session->curr_op_freq);

	if (LIM_IS_AP_ROLE(pe_session)) {
		if (pe_session->wps_state != SAP_WPS_DISABLED)
			populate_dot11f_probe_res_wpsi_es(mac_ctx,
				&frm->WscProbeRes,
				pe_session);
	} else {
		wps_ap = mac_ctx->mlme_cfg->wps_params.enable_wps &
					    WNI_CFG_WPS_ENABLE_AP;
		if (wps_ap)
			populate_dot11f_wsc_in_probe_res(mac_ctx,
				&frm->WscProbeRes);

		if (mac_ctx->lim.wscIeInfo.probeRespWscEnrollmentState ==
		    eLIM_WSC_ENROLL_BEGIN) {
			populate_dot11f_wsc_registrar_info_in_probe_res(mac_ctx,
				&frm->WscProbeRes);
			mac_ctx->lim.wscIeInfo.probeRespWscEnrollmentState =
				eLIM_WSC_ENROLL_IN_PROGRESS;
		}

		if (mac_ctx->lim.wscIeInfo.wscEnrollmentState ==
		    eLIM_WSC_ENROLL_END) {
			de_populate_dot11f_wsc_registrar_info_in_probe_res(
				mac_ctx, &frm->WscProbeRes);
			mac_ctx->lim.wscIeInfo.probeRespWscEnrollmentState =
				eLIM_WSC_ENROLL_NOOP;
		}
	}

	populate_dot11f_country(mac_ctx, &frm->Country, pe_session);
	populate_dot11f_edca_param_set(mac_ctx, &frm->EDCAParamSet, pe_session);

	if (pe_session->dot11mode != MLME_DOT11_MODE_11B)
		populate_dot11f_erp_info(mac_ctx, &frm->ERPInfo, pe_session);

	populate_dot11f_ext_supp_rates(mac_ctx,
		POPULATE_DOT11F_RATES_OPERATIONAL,
		&frm->ExtSuppRates, pe_session);

	/* Populate HT IEs, when operating in 11n */
	if (pe_session->htCapability) {
		populate_dot11f_ht_caps(mac_ctx, pe_session, &frm->HTCaps);
		populate_dot11f_ht_info(mac_ctx, &frm->HTInfo, pe_session);
	}
	if (pe_session->vhtCapability) {
		pe_debug("Populate VHT IE in Probe Response");
		populate_dot11f_vht_caps(mac_ctx, pe_session, &frm->VHTCaps);
		populate_dot11f_vht_operation(mac_ctx, pe_session,
			&frm->VHTOperation);
		populate_dot11f_tx_power_env(mac_ctx,
					     &frm->transmit_power_env[0],
					     pe_session->ch_width,
					     pe_session->curr_op_freq,
					     &frm->num_transmit_power_env,
					     false);
		/*
		 * we do not support multi users yet.
		 * populate_dot11f_vht_ext_bss_load( mac_ctx,
		 *         &frm.VHTExtBssLoad );
		 */
		is_vht_enabled = true;
	}

	if (wlan_reg_is_6ghz_chan_freq(pe_session->curr_op_freq)) {
		populate_dot11f_tx_power_env(mac_ctx,
					     &frm->transmit_power_env[0],
					     pe_session->ch_width,
					     pe_session->curr_op_freq,
					     &frm->num_transmit_power_env,
					     false);
	}

	if (lim_is_session_he_capable(pe_session)) {
		pe_debug("Populate HE IEs");
		populate_dot11f_he_caps(mac_ctx, pe_session,
					&frm->he_cap);
		populate_dot11f_he_operation(mac_ctx, pe_session,
					     &frm->he_op);
		populate_dot11f_he_6ghz_cap(mac_ctx, pe_session,
					    &frm->he_6ghz_band_cap);
	}

	populate_dot11f_ext_cap(mac_ctx, is_vht_enabled, &frm->ExtCap,
		pe_session);

	if (pe_session->pLimStartBssReq) {
		populate_dot11f_wpa(mac_ctx,
			&(pe_session->pLimStartBssReq->rsnIE),
			&frm->WPA);
		populate_dot11f_rsn_opaque(mac_ctx,
			&(pe_session->pLimStartBssReq->rsnIE),
			&frm->RSNOpaque);
	}

	populate_dot11f_wmm(mac_ctx, &frm->WMMInfoAp, &frm->WMMParams,
		&frm->WMMCaps, pe_session);

#if defined(FEATURE_WLAN_WAPI)
	if (pe_session->pLimStartBssReq)
		populate_dot11f_wapi(mac_ctx,
			&(pe_session->pLimStartBssReq->rsnIE),
			&frm->WAPI);
#endif /* defined(FEATURE_WLAN_WAPI) */

	/*
	 * Only use CFG for non-listen mode. This CFG is not working for
	 * concurrency. In listening mode, probe rsp IEs is passed in
	 * the message from SME to PE.
	 */
	addn_ie_present =
		(pe_session->add_ie_params.probeRespDataLen != 0);

	if (addn_ie_present) {

		add_ie = qdf_mem_malloc(
				pe_session->add_ie_params.probeRespDataLen);
		if (!add_ie)
			goto err_ret;

		qdf_mem_copy(add_ie,
			     pe_session->add_ie_params.probeRespData_buff,
			     pe_session->add_ie_params.probeRespDataLen);
		addn_ie_len = pe_session->add_ie_params.probeRespDataLen;

		if (QDF_STATUS_SUCCESS != lim_get_addn_ie_for_probe_resp(mac_ctx,
					add_ie, &addn_ie_len, preq_p2pie)) {
			pe_err("Unable to get addn_ie");
			goto err_ret;
		}

		sir_status = lim_strip_extcap_update_struct(mac_ctx,
					add_ie, &addn_ie_len,
					&extracted_ext_cap);
		if (QDF_STATUS_SUCCESS != sir_status) {
			pe_debug("Unable to strip off ExtCap IE");
		} else {
			extracted_ext_cap_flag = true;
		}

		bytes = bytes + addn_ie_len;

		if (preq_p2pie)
			p2p_ie = (uint8_t *)limGetP2pIEPtr(mac_ctx, &add_ie[0],
							   addn_ie_len);

		if (p2p_ie) {
			/* get NoA attribute stream P2P IE */
			noalen = lim_get_noa_attr_stream(mac_ctx,
					noa_stream, pe_session);
			if (noalen) {
				if ((p2p_ie[1] + noalen) >
				    WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA_LEN) {
					total_noalen = lim_build_p2p_ie(
								mac_ctx,
								&noa_ie[0],
								&noa_stream[0],
								noalen);
					bytes = bytes + total_noalen;
				} else {
					bytes = bytes + noalen;
				}
			}
		}
	}

	/*
	 * Extcap IE now support variable length, merge Extcap IE from addn_ie
	 * may change the frame size. Therefore, MUST merge ExtCap IE before
	 * dot11f get packed payload size.
	 */
	if (extracted_ext_cap_flag)
		lim_merge_extcap_struct(&frm->ExtCap, &extracted_ext_cap,
					true);

	status = dot11f_get_packed_probe_response_size(mac_ctx, frm, &payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Probe Response size error (0x%08x)",
			status);
		/* We'll fall back on the worst case scenario: */
		payload = sizeof(tDot11fProbeResponse);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("Probe Response size warning (0x%08x)",
			status);
	}

	bytes += payload + sizeof(tSirMacMgmtHdr);

	qdf_status = cds_packet_alloc((uint16_t) bytes, (void **)&frame,
				      (void **)&packet);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Probe Response allocation failed");
		goto err_ret;
	}
	/* Paranoia: */
	qdf_mem_zero(frame, bytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac_ctx, frame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_PROBE_RSP, peer_macaddr,
		pe_session->self_mac_addr);

	mac_hdr = (tpSirMacMgmtHdr) frame;

	sir_copy_mac_addr(mac_hdr->bssId, pe_session->bssId);

	/* That done, pack the Probe Response: */
	status =
		dot11f_pack_probe_response(mac_ctx, frm,
			frame + sizeof(tSirMacMgmtHdr),
			payload, &payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Probe Response pack failure (0x%08x)",
			status);
			goto err_ret;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("Probe Response pack warning (0x%08x)", status);
	}

	pe_debug("Sending Probe Response frame to");
	lim_print_mac_addr(mac_ctx, peer_macaddr, LOGD);

	lim_add_additional_ie(frame, sizeof(tSirMacMgmtHdr) + payload, add_ie,
			      addn_ie_len, p2p_ie, noa_ie, total_noalen,
			      noa_stream, noalen);

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		tx_flag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	/* Queue Probe Response frame in high priority WQ */
	qdf_status = wma_tx_frame(mac_ctx, packet,
				  (uint16_t)bytes,
				  TXRX_FRM_802_11_MGMT,
				  ANI_TXDIR_TODS,
				  7, lim_tx_complete, frame, tx_flag,
				  vdev_id, 0, RATEID_DEFAULT, 0);

	/* Pkt will be freed up by the callback */
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		pe_err("Could not send Probe Response");

	if (add_ie)
		qdf_mem_free(add_ie);

	qdf_mem_free(frm);
	return;

err_ret:
	if (add_ie)
		qdf_mem_free(add_ie);
	if (frm)
		qdf_mem_free(frm);
	if (packet)
		cds_packet_free((void *)packet);
	return;

} /* End lim_send_probe_rsp_mgmt_frame. */

void
lim_send_addts_req_action_frame(struct mac_context *mac,
				tSirMacAddr peerMacAddr,
				tSirAddtsReqInfo *pAddTS, struct pe_session *pe_session)
{
	uint16_t i;
	uint8_t *pFrame;
	tDot11fAddTSRequest AddTSReq;
	tDot11fWMMAddTSRequest WMMAddTSReq;
	uint32_t nPayload, nBytes, nStatus;
	tpSirMacMgmtHdr pMacHdr;
	void *pPacket;
#ifdef FEATURE_WLAN_ESE
	uint32_t phyMode;
#endif
	QDF_STATUS qdf_status;
	uint8_t txFlag = 0;
	uint8_t smeSessionId = 0;

	if (!pe_session) {
		return;
	}

	smeSessionId = pe_session->smeSessionId;

	if (!pAddTS->wmeTspecPresent) {
		qdf_mem_zero((uint8_t *) &AddTSReq, sizeof(AddTSReq));

		AddTSReq.Action.action = QOS_ADD_TS_REQ;
		AddTSReq.DialogToken.token = pAddTS->dialogToken;
		AddTSReq.Category.category = ACTION_CATEGORY_QOS;
		if (pAddTS->lleTspecPresent) {
			populate_dot11f_tspec(&pAddTS->tspec, &AddTSReq.TSPEC);
		} else {
			populate_dot11f_wmmtspec(&pAddTS->tspec,
						 &AddTSReq.WMMTSPEC);
		}

		if (pAddTS->lleTspecPresent) {
			AddTSReq.num_WMMTCLAS = 0;
			AddTSReq.num_TCLAS = pAddTS->numTclas;
			for (i = 0; i < pAddTS->numTclas; ++i) {
				populate_dot11f_tclas(mac, &pAddTS->tclasInfo[i],
						      &AddTSReq.TCLAS[i]);
			}
		} else {
			AddTSReq.num_TCLAS = 0;
			AddTSReq.num_WMMTCLAS = pAddTS->numTclas;
			for (i = 0; i < pAddTS->numTclas; ++i) {
				populate_dot11f_wmmtclas(mac,
							 &pAddTS->tclasInfo[i],
							 &AddTSReq.WMMTCLAS[i]);
			}
		}

		if (pAddTS->tclasProcPresent) {
			if (pAddTS->lleTspecPresent) {
				AddTSReq.TCLASSPROC.processing =
					pAddTS->tclasProc;
				AddTSReq.TCLASSPROC.present = 1;
			} else {
				AddTSReq.WMMTCLASPROC.version = 1;
				AddTSReq.WMMTCLASPROC.processing =
					pAddTS->tclasProc;
				AddTSReq.WMMTCLASPROC.present = 1;
			}
		}

		nStatus =
			dot11f_get_packed_add_ts_request_size(mac, &AddTSReq, &nPayload);
		if (DOT11F_FAILED(nStatus)) {
			pe_err("Failed to calculate the packed size for an Add TS Request (0x%08x)",
				nStatus);
			/* We'll fall back on the worst case scenario: */
			nPayload = sizeof(tDot11fAddTSRequest);
		} else if (DOT11F_WARNED(nStatus)) {
			pe_warn("There were warnings while calculating the packed size for an Add TS Request (0x%08x)",
				nStatus);
		}
	} else {
		qdf_mem_zero((uint8_t *) &WMMAddTSReq, sizeof(WMMAddTSReq));

		WMMAddTSReq.Action.action = QOS_ADD_TS_REQ;
		WMMAddTSReq.DialogToken.token = pAddTS->dialogToken;
		WMMAddTSReq.Category.category = ACTION_CATEGORY_WMM;

		/* WMM spec 2.2.10 - status code is only filled in for ADDTS response */
		WMMAddTSReq.StatusCode.statusCode = 0;

		populate_dot11f_wmmtspec(&pAddTS->tspec, &WMMAddTSReq.WMMTSPEC);
#ifdef FEATURE_WLAN_ESE
		lim_get_phy_mode(mac, &phyMode, pe_session);

		if (phyMode == WNI_CFG_PHY_MODE_11G
		    || phyMode == WNI_CFG_PHY_MODE_11A) {
			pAddTS->tsrsIE.rates[0] = TSRS_11AG_RATE_6MBPS;
		} else {
			pAddTS->tsrsIE.rates[0] = TSRS_11B_RATE_5_5MBPS;
		}
		populate_dot11_tsrsie(mac, &pAddTS->tsrsIE,
				      &WMMAddTSReq.ESETrafStrmRateSet,
				      sizeof(uint8_t));
#endif
		/* fillWmeTspecIE */

		nStatus =
			dot11f_get_packed_wmm_add_ts_request_size(mac, &WMMAddTSReq,
								  &nPayload);
		if (DOT11F_FAILED(nStatus)) {
			pe_err("Failed to calculate the packed size for a WMM Add TS Request (0x%08x)",
				nStatus);
			/* We'll fall back on the worst case scenario: */
			nPayload = sizeof(tDot11fAddTSRequest);
		} else if (DOT11F_WARNED(nStatus)) {
			pe_warn("There were warnings while calculating the packed size for a WMM Add TS Request (0x%08x)",
				nStatus);
		}
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);

	qdf_status = cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				      (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for an Add TS Request",
			nBytes);
		return;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peerMacAddr, pe_session->self_mac_addr);
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);

	lim_set_protected_bit(mac, pe_session, peerMacAddr, pMacHdr);

	/* That done, pack the struct: */
	if (!pAddTS->wmeTspecPresent) {
		nStatus = dot11f_pack_add_ts_request(mac, &AddTSReq,
						     pFrame +
						     sizeof(tSirMacMgmtHdr),
						     nPayload, &nPayload);
		if (DOT11F_FAILED(nStatus)) {
			pe_err("Failed to pack an Add TS Request "
				"(0x%08x)", nStatus);
			cds_packet_free((void *)pPacket);
			return; /* allocated! */
		} else if (DOT11F_WARNED(nStatus)) {
			pe_warn("There were warnings while packing an Add TS Request (0x%08x)",
				nStatus);
		}
	} else {
		nStatus = dot11f_pack_wmm_add_ts_request(mac, &WMMAddTSReq,
							 pFrame +
							 sizeof(tSirMacMgmtHdr),
							 nPayload, &nPayload);
		if (DOT11F_FAILED(nStatus)) {
			pe_err("Failed to pack a WMM Add TS Request (0x%08x)",
				nStatus);
			cds_packet_free((void *)pPacket);
			return; /* allocated! */
		} else if (DOT11F_WARNED(nStatus)) {
			pe_warn("There were warnings while packing a WMM Add TS Request (0x%08x)",
				nStatus);
		}
	}

	pe_debug("Sending an Add TS Request frame to");
	lim_print_mac_addr(mac, peerMacAddr, LOGD);

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, pMacHdr->fc.subType));
	lim_diag_mgmt_tx_event_report(mac, pMacHdr,
				      pe_session, QDF_STATUS_SUCCESS,
				      QDF_STATUS_SUCCESS);

	/* Queue Addts Response frame in high priority WQ */
	qdf_status = wma_tx_frame(mac, pPacket, (uint16_t) nBytes,
				TXRX_FRM_802_11_MGMT,
				ANI_TXDIR_TODS,
				7, lim_tx_complete, pFrame, txFlag,
				smeSessionId, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			 pe_session->peSessionId, qdf_status));

	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		pe_err("Could not send an Add TS Request (%X",
			qdf_status);
} /* End lim_send_addts_req_action_frame. */

#ifdef WLAN_FEATURE_MSCS
/**
 * lim_mscs_req_tx_complete_cnf()- Confirmation for mscs req sent over the air
 * @context: pointer to global mac
 * @buf: buffer
 * @tx_complete : Sent status
 * @params; tx completion params
 *
 * Return: This returns QDF_STATUS
 */

static QDF_STATUS lim_mscs_req_tx_complete_cnf(void *context, qdf_nbuf_t buf,
					       uint32_t tx_complete,
					       void *params)
{
	uint16_t mscs_ack_status;
	uint16_t reason_code;

	pe_debug("mscs req TX: %s (%d)",
		 (tx_complete == WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK) ?
		      "success" : "fail", tx_complete);

	if (tx_complete == WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK) {
		mscs_ack_status = ACKED;
		reason_code = QDF_STATUS_SUCCESS;
	} else {
		mscs_ack_status = NOT_ACKED;
		reason_code = QDF_STATUS_E_FAILURE;
	}
	if (buf)
		qdf_nbuf_free(buf);

	return QDF_STATUS_SUCCESS;
}

void lim_send_mscs_req_action_frame(struct mac_context *mac,
				    struct qdf_mac_addr peer_mac,
				    struct mscs_req_info *mscs_req,
				    struct pe_session *pe_session)
{
	uint8_t *frame;
	tDot11fmscs_request_action_frame mscs_req_frame;
	uint32_t payload, bytes;
	tpSirMacMgmtHdr peer_mac_hdr;
	void *packet;
	QDF_STATUS qdf_status;
	tpSirMacMgmtHdr mac_hdr;

	qdf_mem_zero(&mscs_req_frame, sizeof(mscs_req_frame));

	mscs_req_frame.Action.action = MCSC_REQ;
	mscs_req_frame.DialogToken.token = mscs_req->dialog_token;
	mscs_req_frame.Category.category = ACTION_CATEGORY_RVS;
	populate_dot11f_mscs_dec_element(mscs_req, &mscs_req_frame);
	bytes = dot11f_get_packed_mscs_request_action_frameSize(mac,
					&mscs_req_frame, &payload);
	if (DOT11F_FAILED(bytes)) {
		pe_err("Failed to calculate the packed size for an MSCS Request (0x%08x)",
		       bytes);
		/* We'll fall back on the worst case scenario: */
		payload = sizeof(tDot11fmscs_request_action_frame);
	} else if (DOT11F_WARNED(bytes)) {
		pe_warn("There were warnings while calculating the packed size for MSCS Request (0x%08x)",
			bytes);
	}

	bytes = payload + sizeof(struct qdf_mac_addr);

	qdf_status = cds_packet_alloc((uint16_t) bytes, (void **)&frame,
				      (void **)&packet);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for an mscs request",
		       bytes);
		return;
	}
	/* Paranoia: */
	qdf_mem_zero(frame, bytes);

	lim_populate_mac_header(mac, frame, SIR_MAC_MGMT_FRAME,
				SIR_MAC_MGMT_ACTION,
				peer_mac.bytes, pe_session->self_mac_addr);
	peer_mac_hdr = (tpSirMacMgmtHdr) frame;

	qdf_mem_copy(peer_mac.bytes, pe_session->bssId, QDF_MAC_ADDR_SIZE);

	lim_set_protected_bit(mac, pe_session, peer_mac.bytes, peer_mac_hdr);

	bytes = dot11f_pack_mscs_request_action_frame(mac, &mscs_req_frame,
						      frame +
						      sizeof(tSirMacMgmtHdr),
						      payload, &payload);
	if (DOT11F_FAILED(bytes)) {
		pe_err("Failed to pack an mscs Request " "(0x%08x)", bytes);
		cds_packet_free((void *)packet);
			return; /* allocated! */
	} else if (DOT11F_WARNED(bytes)) {
			pe_warn("There were warnings while packing an mscs Request (0x%08x)",
				bytes);
	}

	mac_hdr = (tpSirMacMgmtHdr) frame;

	pe_debug("mscs req TX: vdev id: %d to "QDF_MAC_ADDR_FMT" seq num[%d], frame subtype:%d ",
		 mscs_req->vdev_id, QDF_MAC_ADDR_REF(peer_mac.bytes),
		 mac->mgmtSeqNum, mac_hdr->fc.subType);

	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   frame,
			   (uint16_t)(sizeof(tSirMacMgmtHdr) + payload));
	qdf_status =
		wma_tx_frameWithTxComplete(mac, packet,
			   (uint16_t) (sizeof(tSirMacMgmtHdr) + payload),
			   TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS, 7,
			   lim_tx_complete, frame, lim_mscs_req_tx_complete_cnf,
			   HAL_USE_PEER_STA_REQUESTED_MASK, pe_session->vdev_id,
			   false, 0, RATEID_DEFAULT, 0);
	if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
		mlme_set_is_mscs_req_sent(pe_session->vdev, true);
	} else {
		pe_err("Could not send an mscs Request (%X)", qdf_status);
	}

	/* Pkt will be freed up by the callback */
} /* End lim_send_mscs_req_action_frame */
#endif

/**
 * lim_assoc_rsp_tx_complete() - Confirmation for assoc rsp OTA
 * @context: pointer to global mac
 * @buf: buffer which is nothing but entire assoc rsp frame
 * @tx_complete : Sent status
 * @params; tx completion params
 *
 * Return: This returns QDF_STATUS
 */
static QDF_STATUS lim_assoc_rsp_tx_complete(
					void *context,
					qdf_nbuf_t buf,
					uint32_t tx_complete,
					void *params)
{
	struct mac_context *mac_ctx = (struct mac_context *)context;
	tSirMacMgmtHdr *mac_hdr;
	struct pe_session *session_entry;
	uint8_t session_id;
	tpLimMlmAssocInd lim_assoc_ind;
	tpDphHashNode sta_ds;
	uint16_t aid;
	uint8_t *data;
	struct assoc_ind *sme_assoc_ind;
	struct scheduler_msg msg;
	tpSirAssocReq assoc_req;

	if (!buf) {
		pe_err("Assoc rsp frame buffer is NULL");
		goto null_buf;
	}

	data = qdf_nbuf_data(buf);

	if (!data) {
		pe_err("Assoc rsp frame is NULL");
		goto end;
	}

	mac_hdr = (tSirMacMgmtHdr *)data;

	session_entry = pe_find_session_by_bssid(
				mac_ctx, mac_hdr->sa,
				&session_id);
	if (!session_entry) {
		pe_err("session entry is NULL");
		goto end;
	}

	sta_ds = dph_lookup_hash_entry(mac_ctx,
				       (uint8_t *)mac_hdr->da, &aid,
				       &session_entry->dph.dphHashTable);
	if (!sta_ds) {
		pe_err("sta_ds is NULL");
		goto end;
	}

	/* Get a copy of the already parsed Assoc Request */
	assoc_req =
		(tpSirAssocReq)session_entry->parsedAssocReq[sta_ds->assocId];

	if (!assoc_req) {
		pe_err("assoc req for assoc_id:%d is NULL", sta_ds->assocId);
		goto end;
	}

	if (tx_complete != WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK) {
		lim_send_disassoc_mgmt_frame(mac_ctx,
					     REASON_DISASSOC_DUE_TO_INACTIVITY,
					     sta_ds->staAddr,
					     session_entry, false);
		lim_trigger_sta_deletion(mac_ctx, sta_ds, session_entry);
		goto free_buffers;
	}

	lim_assoc_ind = qdf_mem_malloc(sizeof(tLimMlmAssocInd));
	if (!lim_assoc_ind)
		goto free_assoc_req;

	if (!lim_fill_lim_assoc_ind_params(lim_assoc_ind, mac_ctx,
					   sta_ds, session_entry)) {
		pe_err("lim assoc ind fill error");
		goto lim_assoc_ind;
	}

	sme_assoc_ind = qdf_mem_malloc(sizeof(struct assoc_ind));
	if (!sme_assoc_ind)
		goto lim_assoc_ind;

	sme_assoc_ind->messageType = eWNI_SME_ASSOC_IND_UPPER_LAYER;
	lim_fill_sme_assoc_ind_params(
				mac_ctx, lim_assoc_ind,
				sme_assoc_ind,
				session_entry, true);

	qdf_mem_zero(&msg, sizeof(struct scheduler_msg));
	msg.type = eWNI_SME_ASSOC_IND_UPPER_LAYER;
	msg.bodyptr = sme_assoc_ind;
	msg.bodyval = 0;
	sme_assoc_ind->reassocReq = sta_ds->mlmStaContext.subType;
	sme_assoc_ind->timingMeasCap = sta_ds->timingMeasCap;
	MTRACE(mac_trace_msg_tx(mac_ctx, session_entry->peSessionId, msg.type));
	lim_sys_process_mmh_msg_api(mac_ctx, &msg);

	qdf_mem_free(lim_assoc_ind);

free_buffers:
	if (assoc_req->assocReqFrame) {
		qdf_mem_free(assoc_req->assocReqFrame);
		assoc_req->assocReqFrame = NULL;
	}
	qdf_mem_free(session_entry->parsedAssocReq[sta_ds->assocId]);
	session_entry->parsedAssocReq[sta_ds->assocId] = NULL;
	qdf_nbuf_free(buf);

	return QDF_STATUS_SUCCESS;

lim_assoc_ind:
	qdf_mem_free(lim_assoc_ind);
free_assoc_req:
	if (assoc_req->assocReqFrame) {
		qdf_mem_free(assoc_req->assocReqFrame);
		assoc_req->assocReqFrame = NULL;
	}
	qdf_mem_free(session_entry->parsedAssocReq[sta_ds->assocId]);
	session_entry->parsedAssocReq[sta_ds->assocId] = NULL;
end:
	qdf_nbuf_free(buf);
null_buf:
	return QDF_STATUS_E_FAILURE;
}

void
lim_send_assoc_rsp_mgmt_frame(struct mac_context *mac_ctx,
			      uint16_t status_code, uint16_t aid,
			      tSirMacAddr peer_addr,
			      uint8_t subtype, tpDphHashNode sta,
			      struct pe_session *pe_session,
			      bool tx_complete)
{
	static tDot11fAssocResponse frm;
	uint8_t *frame;
	tpSirMacMgmtHdr mac_hdr;
	QDF_STATUS sir_status;
	uint8_t lle_mode = 0, addts;
	tHalBitVal qos_mode, wme_mode;
	uint32_t payload, bytes = 0, status;
	void *packet;
	QDF_STATUS qdf_status;
	tUpdateBeaconParams beacon_params;
	uint8_t tx_flag = 0;
	uint32_t addn_ie_len = 0;
	uint8_t *add_ie;
	tpSirAssocReq assoc_req = NULL;
	uint8_t sme_session = 0;
	bool is_vht = false;
	uint16_t stripoff_len = 0;
	tDot11fIEExtCap extracted_ext_cap;
	bool extracted_flag = false;
#ifdef WLAN_FEATURE_11W
	uint8_t retry_int;
	uint16_t max_retries;
#endif

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return;
	}

	sme_session = pe_session->smeSessionId;

	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));

	limGetQosMode(pe_session, &qos_mode);
	limGetWmeMode(pe_session, &wme_mode);

	/*
	 * An Add TS IE is added only if the AP supports it and
	 * the requesting STA sent a traffic spec.
	 */
	addts = (qos_mode && sta && sta->qos.addtsPresent) ? 1 : 0;

	frm.Status.status = status_code;

	frm.AID.associd = aid | LIM_AID_MASK;

	if (!sta) {
		populate_dot11f_supp_rates(mac_ctx,
			POPULATE_DOT11F_RATES_OPERATIONAL,
			&frm.SuppRates, pe_session);
		populate_dot11f_ext_supp_rates(mac_ctx,
			POPULATE_DOT11F_RATES_OPERATIONAL,
			&frm.ExtSuppRates, pe_session);
	} else {
		populate_dot11f_assoc_rsp_rates(mac_ctx, &frm.SuppRates,
			&frm.ExtSuppRates,
			sta->supportedRates.llbRates,
			sta->supportedRates.llaRates);
	}

	if (LIM_IS_AP_ROLE(pe_session) && sta &&
	    QDF_STATUS_SUCCESS == status_code) {
		assoc_req = (tpSirAssocReq)
			pe_session->parsedAssocReq[sta->assocId];
		/*
		 * populate P2P IE in AssocRsp when assoc_req from the peer
		 * includes P2P IE
		 */
		if (assoc_req && assoc_req->addIEPresent)
			populate_dot11_assoc_res_p2p_ie(mac_ctx,
				&frm.P2PAssocRes,
				assoc_req);
	}

	if (sta) {
		if (eHAL_SET == qos_mode) {
			if (sta->lleEnabled) {
				lle_mode = 1;
				populate_dot11f_edca_param_set(mac_ctx,
					&frm.EDCAParamSet, pe_session);
			}
		}

		if ((!lle_mode) && (eHAL_SET == wme_mode) && sta->wmeEnabled) {
			populate_dot11f_wmm_params(mac_ctx, &frm.WMMParams,
				pe_session);

			if (sta->wsmEnabled)
				populate_dot11f_wmm_caps(&frm.WMMCaps);
		}

		if (sta->mlmStaContext.htCapability &&
		    pe_session->htCapability) {
			pe_debug("Populate HT IEs in Assoc Response");
			populate_dot11f_ht_caps(mac_ctx, pe_session,
				&frm.HTCaps);
			/*
			 * Check the STA capability and
			 * update the HTCaps accordingly
			 */
			frm.HTCaps.supportedChannelWidthSet = (
				sta->htSupportedChannelWidthSet <
				     pe_session->htSupportedChannelWidthSet) ?
				      sta->htSupportedChannelWidthSet :
				       pe_session->htSupportedChannelWidthSet;
			if (!frm.HTCaps.supportedChannelWidthSet)
				frm.HTCaps.shortGI40MHz = 0;

			populate_dot11f_ht_info(mac_ctx, &frm.HTInfo,
						pe_session);
		}
		pe_debug("SupportedChnlWidth: %d, mimoPS: %d, GF: %d, short GI20:%d, shortGI40: %d, dsssCck: %d, AMPDU Param: %x",
			frm.HTCaps.supportedChannelWidthSet,
			frm.HTCaps.mimoPowerSave,
			frm.HTCaps.greenField, frm.HTCaps.shortGI20MHz,
			frm.HTCaps.shortGI40MHz,
			frm.HTCaps.dsssCckMode40MHz,
			frm.HTCaps.maxRxAMPDUFactor);

		if (sta->mlmStaContext.vhtCapability &&
		    pe_session->vhtCapability) {
			pe_debug("Populate VHT IEs in Assoc Response");
			populate_dot11f_vht_caps(mac_ctx, pe_session,
				&frm.VHTCaps);
			populate_dot11f_vht_operation(mac_ctx, pe_session,
					&frm.VHTOperation);
			is_vht = true;
		} else if (sta->mlmStaContext.force_1x1 &&
			   frm.HTCaps.present) {
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
			frm.HTInfo.basicMCSSet[1] = 0;
			frm.HTCaps.supportedMCSSet[1] = 0;
		}

		if (pe_session->vhtCapability &&
		    pe_session->vendor_vht_sap &&
		    (assoc_req) &&
		    assoc_req->vendor_vht_ie.VHTCaps.present) {
			pe_debug("Populate Vendor VHT IEs in Assoc Rsponse");
			frm.vendor_vht_ie.present = 1;
			frm.vendor_vht_ie.sub_type =
				pe_session->vendor_specific_vht_ie_sub_type;
			frm.vendor_vht_ie.VHTCaps.present = 1;
			populate_dot11f_vht_caps(mac_ctx, pe_session,
				&frm.vendor_vht_ie.VHTCaps);
			populate_dot11f_vht_operation(mac_ctx, pe_session,
					&frm.vendor_vht_ie.VHTOperation);
			is_vht = true;
		}
		populate_dot11f_ext_cap(mac_ctx, is_vht, &frm.ExtCap,
			pe_session);

		populate_dot11f_qcn_ie(mac_ctx, pe_session, &frm.qcn_ie,
				       QCN_IE_ATTR_ID_ALL);

		if (lim_is_sta_he_capable(sta) &&
		    lim_is_session_he_capable(pe_session)) {
			pe_debug("Populate HE IEs");
			populate_dot11f_he_caps(mac_ctx, pe_session,
						&frm.he_cap);
			populate_dot11f_he_operation(mac_ctx, pe_session,
						     &frm.he_op);
			populate_dot11f_he_6ghz_cap(mac_ctx, pe_session,
						    &frm.he_6ghz_band_cap);
		}
#ifdef WLAN_FEATURE_11W
		if (status_code == STATUS_ASSOC_REJECTED_TEMPORARILY) {
			max_retries =
			mac_ctx->mlme_cfg->gen.pmf_sa_query_max_retries;
			retry_int =
			mac_ctx->mlme_cfg->gen.pmf_sa_query_retry_interval;
			populate_dot11f_timeout_interval(mac_ctx,
							 &frm.TimeoutInterval,
						SIR_MAC_TI_TYPE_ASSOC_COMEBACK,
						(max_retries -
						sta->pmfSaQueryRetryCount)
						* retry_int);
		}
#endif

		if (LIM_IS_AP_ROLE(pe_session)  && sta->non_ecsa_capable)
			pe_session->lim_non_ecsa_cap_num++;
	}

	qdf_mem_zero((uint8_t *) &beacon_params, sizeof(beacon_params));

	if (LIM_IS_AP_ROLE(pe_session) &&
	    (pe_session->gLimProtectionControl !=
	     MLME_FORCE_POLICY_PROTECTION_DISABLE))
		lim_decide_ap_protection(mac_ctx, peer_addr, &beacon_params,
					 pe_session);

	lim_update_short_preamble(mac_ctx, peer_addr, &beacon_params,
		pe_session);
	lim_update_short_slot_time(mac_ctx, peer_addr, &beacon_params,
		pe_session);

	/*
	 * Populate Do11capabilities after updating session with
	 * Assos req details
	 */
	populate_dot11f_capabilities(mac_ctx, &frm.Capabilities, pe_session);

	beacon_params.bss_idx = pe_session->vdev_id;

	/* Send message to HAL about beacon parameter change. */
	if ((false == mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running)
	    && beacon_params.paramChangeBitmap) {
		sch_set_fixed_beacon_fields(mac_ctx, pe_session);
		lim_send_beacon_params(mac_ctx, &beacon_params, pe_session);
	}

	lim_obss_send_detection_cfg(mac_ctx, pe_session, false);

	add_ie = qdf_mem_malloc(WNI_CFG_ASSOC_RSP_ADDNIE_DATA_LEN);
	if (!add_ie)
		return;

	if (assoc_req) {
		addn_ie_len = pe_session->add_ie_params.assocRespDataLen;

		/* Nonzero length indicates Assoc rsp IE available */
		if (addn_ie_len > 0 &&
		    addn_ie_len <= WNI_CFG_ASSOC_RSP_ADDNIE_DATA_LEN &&
		    (bytes + addn_ie_len) <= SIR_MAX_PACKET_SIZE) {
			qdf_mem_copy(add_ie,
				pe_session->add_ie_params.assocRespData_buff,
				pe_session->add_ie_params.assocRespDataLen);

			qdf_mem_zero((uint8_t *) &extracted_ext_cap,
				    sizeof(extracted_ext_cap));

			stripoff_len = addn_ie_len;
			sir_status =
				lim_strip_extcap_update_struct
					(mac_ctx, &add_ie[0], &stripoff_len,
					&extracted_ext_cap);
			if (QDF_STATUS_SUCCESS != sir_status) {
				pe_debug("strip off extcap IE failed");
			} else {
				addn_ie_len = stripoff_len;
				extracted_flag = true;
			}
			bytes = bytes + addn_ie_len;
		}
		pe_debug("addn_ie_len: %d for Assoc Resp: %d",
			addn_ie_len, assoc_req->addIEPresent);
	}

	/*
	 * Extcap IE now support variable length, merge Extcap IE from addn_ie
	 * may change the frame size. Therefore, MUST merge ExtCap IE before
	 * dot11f get packed payload size.
	 */
	if (extracted_flag)
		lim_merge_extcap_struct(&(frm.ExtCap), &extracted_ext_cap,
					true);

	/* Allocate a buffer for this frame: */
	status = dot11f_get_packed_assoc_response_size(mac_ctx, &frm, &payload);
	if (DOT11F_FAILED(status)) {
		pe_err("get Association Response size failure (0x%08x)",
			status);
		goto error;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("get Association Response size warning (0x%08x)",
			status);
	}

	bytes += sizeof(tSirMacMgmtHdr) + payload;

	if (sta)
		bytes += sta->mlmStaContext.owe_ie_len;

	qdf_status = cds_packet_alloc((uint16_t) bytes, (void **)&frame,
				      (void **)&packet);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("cds_packet_alloc failed");
		goto error;
	}
	/* Paranoia: */
	qdf_mem_zero(frame, bytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac_ctx, frame, SIR_MAC_MGMT_FRAME,
		(LIM_ASSOC == subtype) ?
			SIR_MAC_MGMT_ASSOC_RSP : SIR_MAC_MGMT_REASSOC_RSP,
			peer_addr,
			pe_session->self_mac_addr);
	mac_hdr = (tpSirMacMgmtHdr) frame;

	sir_copy_mac_addr(mac_hdr->bssId, pe_session->bssId);

	status = dot11f_pack_assoc_response(mac_ctx, &frm,
					     frame + sizeof(tSirMacMgmtHdr),
					     payload, &payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Association Response pack failure(0x%08x)",
			status);
		cds_packet_free((void *)packet);
		goto error;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("Association Response pack warning (0x%08x)",
			status);
	}

	pe_nofl_debug("Assoc rsp TX: vdev %d subtype %d to "QDF_MAC_ADDR_FMT" seq num %d status %d aid %d",
		      pe_session->vdev_id, subtype,
		      QDF_MAC_ADDR_REF(mac_hdr->da),
		      mac_ctx->mgmtSeqNum, status_code, aid);

	if (addn_ie_len && addn_ie_len <= WNI_CFG_ASSOC_RSP_ADDNIE_DATA_LEN)
		qdf_mem_copy(frame + sizeof(tSirMacMgmtHdr) + payload,
			     &add_ie[0], addn_ie_len);

	if (sta && sta->mlmStaContext.owe_ie_len)
		qdf_mem_copy(frame + sizeof(tSirMacMgmtHdr) + payload
			     + addn_ie_len,
			     sta->mlmStaContext.owe_ie,
			     sta->mlmStaContext.owe_ie_len);

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		tx_flag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, mac_hdr->fc.subType));
	lim_diag_mgmt_tx_event_report(mac_ctx, mac_hdr,
				      pe_session, QDF_STATUS_SUCCESS, status_code);
	/* Queue Association Response frame in high priority WQ */
	if (tx_complete)
		qdf_status = wma_tx_frameWithTxComplete(
				mac_ctx, packet, (uint16_t)bytes,
				TXRX_FRM_802_11_MGMT,
				ANI_TXDIR_TODS,
				7, lim_tx_complete, frame,
				lim_assoc_rsp_tx_complete, tx_flag,
				sme_session, false, 0, RATEID_DEFAULT, 0);
	else
		qdf_status = wma_tx_frame(
				mac_ctx, packet, (uint16_t)bytes,
				TXRX_FRM_802_11_MGMT,
				ANI_TXDIR_TODS,
				7, lim_tx_complete, frame, tx_flag,
				sme_session, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			 pe_session->peSessionId, qdf_status));

	/* Pkt will be freed up by the callback */
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		pe_err("Could not Send Re/AssocRsp, retCode=%X",
			qdf_status);

	/*
	 * update the ANI peer station count.
	 * FIXME_PROTECTION : take care of different type of station
	 * counter inside this function.
	 */
	lim_util_count_sta_add(mac_ctx, sta, pe_session);
error:
	qdf_mem_free(add_ie);
}

void
lim_send_delts_req_action_frame(struct mac_context *mac,
				tSirMacAddr peer,
				uint8_t wmmTspecPresent,
				struct mac_ts_info *pTsinfo,
				struct mac_tspec_ie *pTspecIe,
				struct pe_session *pe_session)
{
	uint8_t *pFrame;
	tpSirMacMgmtHdr pMacHdr;
	tDot11fDelTS DelTS;
	tDot11fWMMDelTS WMMDelTS;
	uint32_t nBytes, nPayload, nStatus;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint8_t txFlag = 0;
	uint8_t smeSessionId = 0;

	if (!pe_session)
		return;

	smeSessionId = pe_session->smeSessionId;

	if (!wmmTspecPresent) {
		qdf_mem_zero((uint8_t *) &DelTS, sizeof(DelTS));

		DelTS.Category.category = ACTION_CATEGORY_QOS;
		DelTS.Action.action = QOS_DEL_TS_REQ;
		populate_dot11f_ts_info(pTsinfo, &DelTS.TSInfo);

		nStatus = dot11f_get_packed_del_ts_size(mac, &DelTS, &nPayload);
		if (DOT11F_FAILED(nStatus)) {
			pe_err("Failed to calculate the packed size for a Del TS (0x%08x)",
			       nStatus);
			/* We'll fall back on the worst case scenario: */
			nPayload = sizeof(tDot11fDelTS);
		} else if (DOT11F_WARNED(nStatus)) {
			pe_warn("There were warnings while calculating the packed size for a Del TS (0x%08x)",
				nStatus);
		}
	} else {
		qdf_mem_zero((uint8_t *) &WMMDelTS, sizeof(WMMDelTS));

		WMMDelTS.Category.category = ACTION_CATEGORY_WMM;
		WMMDelTS.Action.action = QOS_DEL_TS_REQ;
		WMMDelTS.DialogToken.token = 0;
		WMMDelTS.StatusCode.statusCode = 0;
		populate_dot11f_wmmtspec(pTspecIe, &WMMDelTS.WMMTSPEC);
		nStatus =
			dot11f_get_packed_wmm_del_ts_size(mac, &WMMDelTS, &nPayload);
		if (DOT11F_FAILED(nStatus)) {
			pe_err("Failed to calculate the packed size for a WMM Del TS (0x%08x)",
			       nStatus);
			/* We'll fall back on the worst case scenario: */
			nPayload = sizeof(tDot11fDelTS);
		} else if (DOT11F_WARNED(nStatus)) {
			pe_warn("There were warnings while calculating the packed size for a WMM Del TS (0x%08x)",
				nStatus);
		}
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);

	qdf_status =
		cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				 (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for an Add TS Response",
			nBytes);
		return;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer, pe_session->self_mac_addr);
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);

	lim_set_protected_bit(mac, pe_session, peer, pMacHdr);

	/* That done, pack the struct: */
	if (!wmmTspecPresent) {
		nStatus = dot11f_pack_del_ts(mac, &DelTS,
					     pFrame + sizeof(tSirMacMgmtHdr),
					     nPayload, &nPayload);
		if (DOT11F_FAILED(nStatus)) {
			pe_err("Failed to pack a Del TS frame (0x%08x)",
				nStatus);
			cds_packet_free((void *)pPacket);
			return; /* allocated! */
		} else if (DOT11F_WARNED(nStatus)) {
			pe_warn("There were warnings while packing a Del TS frame (0x%08x)",
				nStatus);
		}
	} else {
		nStatus = dot11f_pack_wmm_del_ts(mac, &WMMDelTS,
						 pFrame + sizeof(tSirMacMgmtHdr),
						 nPayload, &nPayload);
		if (DOT11F_FAILED(nStatus)) {
			pe_err("Failed to pack a WMM Del TS frame (0x%08x)",
				nStatus);
			cds_packet_free((void *)pPacket);
			return; /* allocated! */
		} else if (DOT11F_WARNED(nStatus)) {
			pe_warn("There were warnings while packing a WMM Del TS frame (0x%08x)",
				nStatus);
		}
	}

	pe_debug("Sending DELTS REQ (size %d) to ", nBytes);
	lim_print_mac_addr(mac, pMacHdr->da, LOGD);

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, pMacHdr->fc.subType));
	lim_diag_mgmt_tx_event_report(mac, pMacHdr,
				      pe_session, QDF_STATUS_SUCCESS,
				      QDF_STATUS_SUCCESS);
	qdf_status = wma_tx_frame(mac, pPacket, (uint16_t) nBytes,
				TXRX_FRM_802_11_MGMT,
				ANI_TXDIR_TODS,
				7, lim_tx_complete, pFrame, txFlag,
				smeSessionId, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			 pe_session->peSessionId, qdf_status));
	/* Pkt will be freed up by the callback */
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		pe_err("Failed to send Del TS (%X)!", qdf_status);

} /* End lim_send_delts_req_action_frame. */

/**
 * lim_assoc_tx_complete_cnf()- Confirmation for assoc sent over the air
 * @context: pointer to global mac
 * @buf: buffer
 * @tx_complete : Sent status
 * @params; tx completion params
 *
 * Return: This returns QDF_STATUS
 */

static QDF_STATUS lim_assoc_tx_complete_cnf(void *context,
					   qdf_nbuf_t buf,
					   uint32_t tx_complete,
					   void *params)
{
	uint16_t assoc_ack_status;
	uint16_t reason_code;
	struct mac_context *mac_ctx = (struct mac_context *)context;

	pe_nofl_rl_info("Assoc req TX: %s (%d)",
			(tx_complete == WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK) ?
			"success" : "fail", tx_complete);

	if (tx_complete == WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK) {
		assoc_ack_status = ACKED;
		reason_code = QDF_STATUS_SUCCESS;
		mac_ctx->assoc_ack_status = LIM_ACK_RCD_SUCCESS;
	} else if (tx_complete == WMI_MGMT_TX_COMP_TYPE_COMPLETE_NO_ACK) {
		assoc_ack_status = NOT_ACKED;
		reason_code = QDF_STATUS_E_FAILURE;
		mac_ctx->assoc_ack_status = LIM_ACK_RCD_FAILURE;
	} else {
		assoc_ack_status = SENT_FAIL;
		reason_code = QDF_STATUS_E_FAILURE;
		mac_ctx->assoc_ack_status = LIM_TX_FAILED;
	}

	if (buf)
		qdf_nbuf_free(buf);

	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_ASSOC_ACK_EVENT,
			NULL, assoc_ack_status, reason_code);
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_ADAPTIVE_11R
/**
 * lim_fill_adaptive_11r_ie() - Populate the Vendor secific adaptive 11r
 * IE to association request frame
 * @pe_session: pointer to PE session
 * @ie_buf: buffer to which Adaptive 11r IE will be copied
 * @ie_len: length of the Adaptive 11r Vendor specific IE
 *
 * Return QDF_STATUS
 */
static QDF_STATUS lim_fill_adaptive_11r_ie(struct pe_session *pe_session,
					   uint8_t **ie_buf, uint8_t *ie_len)
{
	uint8_t *buf = NULL, *adaptive_11r_ie = NULL;

	if (!pe_session->is_adaptive_11r_connection)
		return QDF_STATUS_SUCCESS;

	/*
	 * Vendor specific Adaptive 11r IE to be advertised in Assoc
	 * req:
	 * Type     0xDD
	 * Length   0x0B
	 * OUI      0x00 0x00 0x0F
	 * Type     0x22
	 * subtype  0x00
	 * Version  0x01
	 * Length   0x04
	 * Data     0x00 00 00 01(0th bit is 1 means adaptive 11r is
	 * supported)
	 */
	adaptive_11r_ie = qdf_mem_malloc(ADAPTIVE_11R_STA_IE_LEN + 2);
	if (!adaptive_11r_ie)
		return QDF_STATUS_E_FAILURE;

	/* Fill the Vendor IE Type (0xDD) */
	buf = adaptive_11r_ie;
	*buf = WLAN_ELEMID_VENDOR;
	buf++;

	/* Fill the Vendor IE length (0x0B) */
	*buf = ADAPTIVE_11R_STA_IE_LEN;
	buf++;

	/*
	 * Fill the Adaptive 11r Vendor specific OUI(0x00 0x00 0x0F 0x22)
	 */
	qdf_mem_copy(buf, ADAPTIVE_11R_STA_OUI, ADAPTIVE_11R_OUI_LEN);
	buf += ADAPTIVE_11R_OUI_LEN;

	/* Fill Adaptive 11r Vendor specific Subtype (0x00) */
	*buf = ADAPTIVE_11R_OUI_SUBTYPE;
	buf++;

	/* Fill Adaptive 11r Version (0x01) */
	*buf = ADAPTIVE_11R_OUI_VERSION;
	buf++;

	/* Fill Adaptive 11r IE Data length (0x04) */
	*buf = ADAPTIVE_11R_DATA_LEN;
	buf++;

	/* Fill Adaptive 11r IE Data (0x00 0x00 0x00 0x01) */
	qdf_mem_copy(buf, ADAPTIVE_11R_OUI_DATA, ADAPTIVE_11R_DATA_LEN);

	*ie_len = ADAPTIVE_11R_STA_IE_LEN + 2;
	*ie_buf = adaptive_11r_ie;

	return QDF_STATUS_SUCCESS;
}

#else
static inline
QDF_STATUS lim_fill_adaptive_11r_ie(struct pe_session *pe_session,
				    uint8_t **ie_buf, uint8_t *ie_len)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * lim_send_assoc_req_mgmt_frame() - Send association request
 * @mac_ctx: Handle to MAC context
 * @mlm_assoc_req: Association request information
 * @pe_session: PE session information
 *
 * Builds and transmits association request frame to AP.
 *
 * Return: Void
 */
void
lim_send_assoc_req_mgmt_frame(struct mac_context *mac_ctx,
			      tLimMlmAssocReq *mlm_assoc_req,
			      struct pe_session *pe_session)
{
	int ret;
	tDot11fAssocRequest *frm;
	uint16_t caps;
	uint8_t *frame, *rsnx_ie = NULL;
	QDF_STATUS sir_status;
	tLimMlmAssocCnf assoc_cnf;
	uint32_t bytes = 0, payload, status;
	uint8_t qos_enabled, wme_enabled, wsm_enabled;
	void *packet;
	QDF_STATUS qdf_status;
	uint16_t add_ie_len, current_len = 0, vendor_ie_len = 0;
	uint8_t *add_ie = NULL, *mscs_ext_ie = NULL;
	const uint8_t *wps_ie = NULL;
	uint8_t power_caps = false;
	uint8_t tx_flag = 0;
	uint8_t vdev_id = 0;
	bool vht_enabled = false;
	tDot11fIEExtCap extr_ext_cap;
	bool extr_ext_flag = true, is_open_auth = false;
	tpSirMacMgmtHdr mac_hdr;
	uint32_t ie_offset = 0;
	uint8_t *p_ext_cap = NULL;
	tDot11fIEExtCap bcn_ext_cap;
	uint8_t *bcn_ie = NULL;
	uint32_t bcn_ie_len = 0;
	uint32_t aes_block_size_len = 0;
	enum rateid min_rid = RATEID_DEFAULT;
	uint8_t *mbo_ie = NULL, *adaptive_11r_ie = NULL, *vendor_ies = NULL;
	uint8_t mbo_ie_len = 0, adaptive_11r_ie_len = 0, rsnx_ie_len = 0;
	uint8_t mscs_ext_ie_len = 0;
	bool bss_mfp_capable;
	int8_t peer_rssi = 0;

	if (!pe_session) {
		pe_err("pe_session is NULL");
		qdf_mem_free(mlm_assoc_req);
		return;
	}

	vdev_id = pe_session->vdev_id;

	/* check this early to avoid unncessary operation */
	if (!pe_session->lim_join_req) {
		pe_err("pe_session->lim_join_req is NULL");
		qdf_mem_free(mlm_assoc_req);
		return;
	}
	add_ie_len = pe_session->lim_join_req->addIEAssoc.length;
	if (add_ie_len) {
		add_ie = qdf_mem_malloc(add_ie_len);
		if (!add_ie) {
			qdf_mem_free(mlm_assoc_req);
			return;
		}
		/*
		 * copy the additional ie to local, as this func modify
		 * the IE, these IE will be required in assoc/re-assoc
		 * retry. So do not modify the original IE.
		 */
		qdf_mem_copy(add_ie, pe_session->lim_join_req->addIEAssoc.addIEdata,
			     add_ie_len);

	}

	frm = qdf_mem_malloc(sizeof(tDot11fAssocRequest));
	if (!frm) {
		qdf_mem_free(mlm_assoc_req);
		qdf_mem_free(add_ie);
		return;
	}
	qdf_mem_zero((uint8_t *) frm, sizeof(tDot11fAssocRequest));

	if (add_ie_len && pe_session->is_ext_caps_present) {
		qdf_mem_zero((uint8_t *) &extr_ext_cap,
				sizeof(tDot11fIEExtCap));
		sir_status = lim_strip_extcap_update_struct(mac_ctx,
					add_ie, &add_ie_len, &extr_ext_cap);
		if (QDF_STATUS_SUCCESS != sir_status) {
			extr_ext_flag = false;
			pe_debug("Unable to Stripoff ExtCap IE from Assoc Req");
		} else {
			struct s_ext_cap *p_ext_cap = (struct s_ext_cap *)
							extr_ext_cap.bytes;

			if (p_ext_cap->interworking_service)
				p_ext_cap->qos_map = 1;
			extr_ext_cap.num_bytes =
				lim_compute_ext_cap_ie_length(&extr_ext_cap);
			extr_ext_flag = (extr_ext_cap.num_bytes > 0);
		}
	} else {
		pe_debug("No addn IE or peer doesn't support addnIE for Assoc Req");
		extr_ext_flag = false;
	}

	caps = mlm_assoc_req->capabilityInfo;
#if defined(FEATURE_WLAN_WAPI)
	/*
	 * According to WAPI standard:
	 * 7.3.1.4 Capability Information field
	 * In WAPI, non-AP STAs within an ESS set the Privacy subfield to 0
	 * in transmitted Association or Reassociation management frames.
	 * APs ignore the Privacy subfield within received Association and
	 * Reassociation management frames.
	 */
	if (pe_session->encryptType == eSIR_ED_WPI)
		((tSirMacCapabilityInfo *) &caps)->privacy = 0;
#endif
	swap_bit_field16(caps, (uint16_t *) &frm->Capabilities);

	frm->ListenInterval.interval = mlm_assoc_req->listenInterval;
	populate_dot11f_ssid2(mac_ctx, &frm->SSID);
	populate_dot11f_supp_rates(mac_ctx, POPULATE_DOT11F_RATES_OPERATIONAL,
		&frm->SuppRates, pe_session);

	qos_enabled = (pe_session->limQosEnabled) &&
		      SIR_MAC_GET_QOS(pe_session->limCurrentBssCaps);

	wme_enabled = (pe_session->limWmeEnabled) &&
		      LIM_BSS_CAPS_GET(WME, pe_session->limCurrentBssQosCaps);

	/* We prefer .11e asociations: */
	if (qos_enabled)
		wme_enabled = false;

	wsm_enabled = (pe_session->limWsmEnabled) && wme_enabled &&
		      LIM_BSS_CAPS_GET(WSM, pe_session->limCurrentBssQosCaps);

	if (pe_session->lim11hEnable &&
	    pe_session->lim_join_req->spectrumMgtIndicator == true) {
		power_caps = true;

		populate_dot11f_power_caps(mac_ctx, &frm->PowerCaps,
			LIM_ASSOC, pe_session);
		populate_dot11f_supp_channels(mac_ctx, &frm->SuppChannels,
			LIM_ASSOC, pe_session);

	}
	if (mac_ctx->rrm.rrmPEContext.rrmEnable &&
	    SIR_MAC_GET_RRM(pe_session->limCurrentBssCaps)) {
		if (power_caps == false) {
			power_caps = true;
			populate_dot11f_power_caps(mac_ctx, &frm->PowerCaps,
				LIM_ASSOC, pe_session);
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
		populate_dot11f_rrm_ie(mac_ctx, &frm->RRMEnabledCap,
			pe_session);

	/*
	 * The join request *should* contain zero or one of the WPA and RSN
	 * IEs.  The payload send along with the request is a
	 * 'struct join_req'; the IE portion is held inside a 'tSirRSNie':
	 *     typedef struct sSirRSNie
	 *     {
	 *         uint16_t       length;
	 *         uint8_t        rsnIEdata[WLAN_MAX_IE_LEN+2];
	 *     } tSirRSNie, *tpSirRSNie;
	 * So, we should be able to make the following two calls harmlessly,
	 * since they do nothing if they don't find the given IE in the
	 * bytestream with which they're provided.
	 * The net effect of this will be to faithfully transmit whatever
	 * security IE is in the join request.
	 * However, if we're associating for the purpose of WPS
	 * enrollment, and we've been configured to indicate that by
	 * eliding the WPA or RSN IE, we just skip this:
	 */
	if (add_ie_len && add_ie)
		wps_ie = limGetWscIEPtr(mac_ctx, add_ie, add_ie_len);

	if (!wps_ie) {
		populate_dot11f_rsn_opaque(mac_ctx,
					   &pe_session->lim_join_req->rsnIE,
					   &frm->RSNOpaque);
		populate_dot11f_wpa_opaque(mac_ctx,
					   &pe_session->lim_join_req->rsnIE,
					   &frm->WPAOpaque);
#if defined(FEATURE_WLAN_WAPI)
		populate_dot11f_wapi_opaque(mac_ctx,
			&(pe_session->lim_join_req->rsnIE),
			&frm->WAPIOpaque);
#endif /* defined(FEATURE_WLAN_WAPI) */
	}
	/* include WME EDCA IE as well */
	if (wme_enabled) {
		populate_dot11f_wmm_info_station_per_session(mac_ctx,
			pe_session, &frm->WMMInfoStation);

		if (wsm_enabled)
			populate_dot11f_wmm_caps(&frm->WMMCaps);
	}

	/*
	 * Populate HT IEs, when operating in 11n and
	 * when AP is also operating in 11n mode
	 */
	if (pe_session->htCapability &&
	    mac_ctx->lim.htCapabilityPresentInBeacon) {
		pe_debug("Populate HT Caps in Assoc Request");
		populate_dot11f_ht_caps(mac_ctx, pe_session, &frm->HTCaps);
	} else if (pe_session->he_with_wep_tkip) {
		pe_debug("Populate HT Caps in Assoc Request with WEP/TKIP");
		populate_dot11f_ht_caps(mac_ctx, NULL, &frm->HTCaps);
	}

	if (pe_session->vhtCapability &&
	    pe_session->vhtCapabilityPresentInBeacon) {
		pe_debug("Populate VHT IEs in Assoc Request");
		populate_dot11f_vht_caps(mac_ctx, pe_session, &frm->VHTCaps);
		vht_enabled = true;
		if (pe_session->gLimOperatingMode.present &&
		    pe_session->ch_width == CH_WIDTH_20MHZ &&
		    frm->VHTCaps.present) {
			pe_debug("VHT OP mode IE in Assoc Req");
			populate_dot11f_operating_mode(mac_ctx,
					&frm->OperatingMode, pe_session);
		}
	} else if (pe_session->he_with_wep_tkip) {
		pe_debug("Populate VHT IEs in Assoc Request with WEP/TKIP");
		populate_dot11f_vht_caps(mac_ctx, NULL, &frm->VHTCaps);
	}

	if (!vht_enabled &&
			pe_session->is_vendor_specific_vhtcaps) {
		pe_debug("Populate Vendor VHT IEs in Assoc Request");
		frm->vendor_vht_ie.present = 1;
		frm->vendor_vht_ie.sub_type =
			pe_session->vendor_specific_vht_ie_sub_type;
		frm->vendor_vht_ie.VHTCaps.present = 1;
		if (!mac_ctx->mlme_cfg->vht_caps.vht_cap_info.vendor_vhtie &&
		    pe_session->vht_config.su_beam_formee) {
			pe_debug("Disable SU beamformee for vendor IE");
			pe_session->vht_config.su_beam_formee = 0;
		}
		populate_dot11f_vht_caps(mac_ctx, pe_session,
				&frm->vendor_vht_ie.VHTCaps);
		vht_enabled = true;
	}
	if (pe_session->is_ext_caps_present)
		populate_dot11f_ext_cap(mac_ctx, vht_enabled,
				&frm->ExtCap, pe_session);

	populate_dot11f_qcn_ie(mac_ctx, pe_session,
			       &frm->qcn_ie, QCN_IE_ATTR_ID_ALL);

	if (lim_is_session_he_capable(pe_session)) {
		pe_debug("Populate HE IEs");
		populate_dot11f_he_caps(mac_ctx, pe_session,
					&frm->he_cap);
		populate_dot11f_he_6ghz_cap(mac_ctx, pe_session,
					    &frm->he_6ghz_band_cap);
	} else if (pe_session->he_with_wep_tkip) {
		pe_debug("Populate HE IEs in Assoc Request with WEP/TKIP");
		populate_dot11f_he_caps(mac_ctx, NULL, &frm->he_cap);
		populate_dot11f_he_6ghz_cap(mac_ctx, pe_session,
					    &frm->he_6ghz_band_cap);
	}

	if (pe_session->lim_join_req->is11Rconnection) {
		struct bss_description *bssdescr;

		bssdescr = &pe_session->lim_join_req->bssDescription;
		pe_debug("mdie = %02x %02x %02x",
			(unsigned int) bssdescr->mdie[0],
			(unsigned int) bssdescr->mdie[1],
			(unsigned int) bssdescr->mdie[2]);
		populate_mdie(mac_ctx, &frm->MobilityDomain,
			pe_session->lim_join_req->bssDescription.mdie);

		/*
		 * IEEE80211-ai [13.2.4 FT initial mobility domain association
		 * over FILS in an RSN]
		 * Populate FT IE in association request. This FT IE should be
		 * same as the FT IE received in auth response frame during the
		 * FT-FILS authentication.
		 */
		if (lim_is_fils_connection(pe_session))
			populate_fils_ft_info(mac_ctx, &frm->FTInfo,
					      pe_session);
	}

#ifdef FEATURE_WLAN_ESE
	/*
	 * ESE Version IE will be included in association request
	 * when ESE is enabled on DUT through ini and it is also
	 * advertised by the peer AP to which we are trying to
	 * associate to.
	 */
	if (pe_session->is_ese_version_ie_present &&
		mac_ctx->mlme_cfg->lfr.ese_enabled)
		populate_dot11f_ese_version(&frm->ESEVersion);
	/* For ESE Associations fill the ESE IEs */
	if (pe_session->isESEconnection &&
	    pe_session->lim_join_req->isESEFeatureIniEnabled) {
#ifndef FEATURE_DISABLE_RM
		populate_dot11f_ese_rad_mgmt_cap(&frm->ESERadMgmtCap);
#endif
	}
#endif

	/*
	 * Extcap IE now support variable length, merge Extcap IE from addn_ie
	 * may change the frame size. Therefore, MUST merge ExtCap IE before
	 * dot11f get packed payload size.
	 */
	if (extr_ext_flag)
		lim_merge_extcap_struct(&frm->ExtCap, &extr_ext_cap, true);

	/* Clear the bits in EXTCAP IE if AP not advertise it in beacon */
	if (frm->ExtCap.present && pe_session->is_ext_caps_present) {
		ie_offset = DOT11F_FF_TIMESTAMP_LEN +
				DOT11F_FF_BEACONINTERVAL_LEN +
				DOT11F_FF_CAPABILITIES_LEN;

		qdf_mem_zero((uint8_t *)&bcn_ext_cap, sizeof(tDot11fIEExtCap));
		if (pe_session->beacon && pe_session->bcnLen > ie_offset) {
			bcn_ie = pe_session->beacon + ie_offset;
			bcn_ie_len = pe_session->bcnLen - ie_offset;
			p_ext_cap = (uint8_t *)wlan_get_ie_ptr_from_eid(
							DOT11F_EID_EXTCAP,
							bcn_ie, bcn_ie_len);
			lim_update_extcap_struct(mac_ctx, p_ext_cap,
							&bcn_ext_cap);
			lim_merge_extcap_struct(&frm->ExtCap, &bcn_ext_cap,
							false);
		}

		populate_dot11f_btm_extended_caps(mac_ctx, pe_session,
						  &frm->ExtCap);
		/*
		 * TWT extended capabilities should be populated after the
		 * intersection of beacon caps and self caps is done because
		 * the bits for TWT are unique to STA and AP and cannot be
		 * intersected.
		 */
		populate_dot11f_twt_extended_caps(mac_ctx, pe_session,
						  &frm->ExtCap);
	}

	if (QDF_STATUS_SUCCESS != lim_strip_supp_op_class_update_struct(mac_ctx,
			add_ie, &add_ie_len, &frm->SuppOperatingClasses))
		pe_debug("Unable to Stripoff supp op classes IE from Assoc Req");

	if (lim_is_fils_connection(pe_session)) {
		populate_dot11f_fils_params(mac_ctx, frm, pe_session);
		aes_block_size_len = AES_BLOCK_SIZE;
	}

	/* RSNX IE for SAE PWE derivation based on H2E */
	if (wlan_get_ie_ptr_from_eid(WLAN_ELEMID_RSNXE, add_ie, add_ie_len)) {
		rsnx_ie = qdf_mem_malloc(WLAN_MAX_IE_LEN + 2);
		if (!rsnx_ie)
			goto end;

		qdf_status = lim_strip_ie(mac_ctx, add_ie, &add_ie_len,
					  WLAN_ELEMID_RSNXE, ONE_BYTE,
					  NULL, 0, rsnx_ie, WLAN_MAX_IE_LEN);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			pe_err("Failed to strip Vendor IEs");
			goto end;
		}
		rsnx_ie_len = rsnx_ie[1] + 2;
	}
	/* MSCS ext ie */
	if (wlan_get_ext_ie_ptr_from_ext_id(MSCS_OUI_TYPE, MSCS_OUI_SIZE,
					    add_ie, add_ie_len)) {
		mscs_ext_ie = qdf_mem_malloc(WLAN_MAX_IE_LEN + 2);
		if (!mscs_ext_ie)
			goto end;

		qdf_status = lim_strip_ie(mac_ctx, add_ie, &add_ie_len,
					  WLAN_ELEMID_EXTN_ELEM, ONE_BYTE,
					  MSCS_OUI_TYPE, MSCS_OUI_SIZE,
					  mscs_ext_ie, WLAN_MAX_IE_LEN);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			pe_err("Failed to strip MSCS ext IE");
			goto end;
		}
		mscs_ext_ie_len = mscs_ext_ie[1] + 2;
	}

	/*
	 * MBO IE needs to be appendded at the end of the assoc request
	 * frame and is not parsed and unpacked by the frame parser
	 * as the supplicant can send multiple TLVs with same Attribute
	 * in the MBO IE and the frame parser does not support multiple
	 * TLVs with same attribute in a single IE.
	 * Strip off the MBO IE from add_ie and append it at the end.
	 */
	if (wlan_get_vendor_ie_ptr_from_oui(SIR_MAC_MBO_OUI,
	    SIR_MAC_MBO_OUI_SIZE, add_ie, add_ie_len)) {
		mbo_ie = qdf_mem_malloc(DOT11F_IE_MBO_IE_MAX_LEN + 2);
		if (!mbo_ie)
			goto end;

		qdf_status = lim_strip_ie(mac_ctx, add_ie, &add_ie_len,
					  WLAN_ELEMID_VENDOR, ONE_BYTE,
					  SIR_MAC_MBO_OUI,
					  SIR_MAC_MBO_OUI_SIZE,
					  mbo_ie, DOT11F_IE_MBO_IE_MAX_LEN);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			pe_err("Failed to strip MBO IE");
			goto end;
		}

		/* Include the EID and length fields */
		mbo_ie_len = mbo_ie[1] + 2;

		if (pe_session->connected_akm == ANI_AKM_TYPE_NONE)
			is_open_auth = true;

		pe_debug("Stripped MBO IE of length %d is_open_auth:%d",
			 mbo_ie_len, is_open_auth);

		if (!is_open_auth) {
			bss_mfp_capable =
				lim_get_vdev_rmf_capable(mac_ctx, pe_session);
			if (!bss_mfp_capable) {
				pe_debug("Peer doesn't support PMF, Don't add MBO IE");
				qdf_mem_free(mbo_ie);
				mbo_ie = NULL;
				mbo_ie_len = 0;
			}
		}
	}

	/*
	 * Strip rest of the vendor IEs and append to the assoc request frame.
	 * Append the IEs just before MBO IEs as MBO IEs have to be at the
	 * end of the frame.
	 */
	if (wlan_get_ie_ptr_from_eid(WLAN_ELEMID_VENDOR, add_ie, add_ie_len)) {
		vendor_ies = qdf_mem_malloc(MAX_VENDOR_IES_LEN + 2);
		if (vendor_ies) {
			current_len = add_ie_len;
			qdf_status = lim_strip_ie(mac_ctx, add_ie, &add_ie_len,
						  WLAN_ELEMID_VENDOR, ONE_BYTE,
						  NULL,
						  0,
						  vendor_ies,
						  MAX_VENDOR_IES_LEN);
			if (QDF_IS_STATUS_ERROR(qdf_status)) {
				pe_err("Failed to strip Vendor IEs");
				goto end;
			}

			vendor_ie_len = current_len - add_ie_len;
			pe_debug("Stripped vendor IEs of size: %u",
				 current_len);
		}
	}

	qdf_status = lim_fill_adaptive_11r_ie(pe_session, &adaptive_11r_ie,
					      &adaptive_11r_ie_len);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		pe_err("Failed to fill adaptive 11r IE");
		goto end;
	}

	/*
	 * Do unpack to populate the add_ie buffer to frm structure
	 * before packing the frm structure. In this way, the IE ordering
	 * which the latest 802.11 spec mandates is maintained.
	 */
	if (add_ie_len) {
		ret = dot11f_unpack_assoc_request(mac_ctx, add_ie,
					    add_ie_len, frm, true);
		if (DOT11F_FAILED(ret)) {
			pe_err("unpack failed, ret: 0x%x", ret);
			goto end;
		}
	}

	status = dot11f_get_packed_assoc_request_size(mac_ctx, frm, &payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Association Request packet size failure(0x%08x)",
			status);
		/* We'll fall back on the worst case scenario: */
		payload = sizeof(tDot11fAssocRequest);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("Association request packet size warning (0x%08x)",
			status);
	}

	bytes = payload + sizeof(tSirMacMgmtHdr) + aes_block_size_len +
		rsnx_ie_len + mbo_ie_len + adaptive_11r_ie_len +
		mscs_ext_ie_len + vendor_ie_len;

	qdf_status = cds_packet_alloc((uint16_t) bytes, (void **)&frame,
				(void **)&packet);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes", bytes);

		pe_session->limMlmState = pe_session->limPrevMlmState;
		MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_MLM_STATE,
				 pe_session->peSessionId,
				 pe_session->limMlmState));

		/* Update PE session id */
		assoc_cnf.sessionId = pe_session->peSessionId;

		assoc_cnf.resultCode = eSIR_SME_RESOURCES_UNAVAILABLE;

		lim_post_sme_message(mac_ctx, LIM_MLM_ASSOC_CNF,
			(uint32_t *) &assoc_cnf);

		goto end;
	}
	/* Paranoia: */
	qdf_mem_zero(frame, bytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac_ctx, frame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ASSOC_REQ, pe_session->bssId,
		pe_session->self_mac_addr);
	/* That done, pack the Assoc Request: */
	status = dot11f_pack_assoc_request(mac_ctx, frm,
			frame + sizeof(tSirMacMgmtHdr), payload, &payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Assoc request pack failure (0x%08x)", status);
		cds_packet_free((void *)packet);
		goto end;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("Assoc request pack warning (0x%08x)", status);
	}

	if (rsnx_ie && rsnx_ie_len) {
		qdf_mem_copy(frame + sizeof(tSirMacMgmtHdr) + payload,
			     rsnx_ie, rsnx_ie_len);
		payload = payload + rsnx_ie_len;
	}

	if (mscs_ext_ie && mscs_ext_ie_len) {
		qdf_mem_copy(frame + sizeof(tSirMacMgmtHdr) + payload,
			     mscs_ext_ie, mscs_ext_ie_len);
		payload = payload + mscs_ext_ie_len;
	}

	/* Copy the vendor IEs to the end of the frame */
	qdf_mem_copy(frame + sizeof(tSirMacMgmtHdr) + payload,
		     vendor_ies, vendor_ie_len);
	payload = payload + vendor_ie_len;

	/* Copy the MBO IE to the end of the frame */
	qdf_mem_copy(frame + sizeof(tSirMacMgmtHdr) + payload,
		     mbo_ie, mbo_ie_len);
	payload = payload + mbo_ie_len;

	/*
	 * Copy the Vendor specific Adaptive 11r IE to end of the
	 * assoc request frame
	 */
	qdf_mem_copy(frame + sizeof(tSirMacMgmtHdr) + payload,
		     adaptive_11r_ie, adaptive_11r_ie_len);
	payload = payload + adaptive_11r_ie_len;

	if (pe_session->assoc_req) {
		qdf_mem_free(pe_session->assoc_req);
		pe_session->assoc_req = NULL;
		pe_session->assocReqLen = 0;
	}

	if (lim_is_fils_connection(pe_session)) {
		qdf_status = aead_encrypt_assoc_req(mac_ctx, pe_session,
						    frame, &payload);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			cds_packet_free((void *)packet);
			goto end;
		}
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

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		tx_flag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	if (pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_STA_MODE)
		tx_flag |= HAL_USE_PEER_STA_REQUESTED_MASK;

	mac_hdr = (tpSirMacMgmtHdr) frame;
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, mac_hdr->fc.subType));

	pe_nofl_info("Assoc req TX: vdev %d to "QDF_MAC_ADDR_FMT" seq num %d", pe_session->vdev_id,
		     QDF_MAC_ADDR_REF(pe_session->bssId),
		     mac_ctx->mgmtSeqNum);
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  frame, (uint16_t)(sizeof(tSirMacMgmtHdr) + payload));

	min_rid = lim_get_min_session_txrate(pe_session);
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_ASSOC_START_EVENT,
			      pe_session, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
	lim_diag_mgmt_tx_event_report(mac_ctx, mac_hdr,
				      pe_session, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);

	peer_rssi = mac_ctx->lim.bss_rssi;

	qdf_status =
		wma_tx_frameWithTxComplete(mac_ctx, packet,
			   (uint16_t) (sizeof(tSirMacMgmtHdr) + payload),
			   TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS, 7,
			   lim_tx_complete, frame, lim_assoc_tx_complete_cnf,
			   tx_flag, vdev_id, false, 0, min_rid, peer_rssi);
	MTRACE(qdf_trace
		       (QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
		       pe_session->peSessionId, qdf_status));

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to send Association Request (%X)!",
			qdf_status);
		mac_ctx->assoc_ack_status = LIM_TX_FAILED;
		lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_ASSOC_ACK_EVENT,
				pe_session, SENT_FAIL, QDF_STATUS_E_FAILURE);
		/* Pkt will be freed up by the callback */
	}

end:
	qdf_mem_free(rsnx_ie);
	qdf_mem_free(vendor_ies);
	qdf_mem_free(mbo_ie);
	qdf_mem_free(mscs_ext_ie);

	/* Free up buffer allocated for mlm_assoc_req */
	qdf_mem_free(adaptive_11r_ie);
	qdf_mem_free(mlm_assoc_req);
	qdf_mem_free(add_ie);
	mlm_assoc_req = NULL;
	qdf_mem_free(frm);
	return;
}

/**
 * lim_addba_rsp_tx_complete_cnf() - Confirmation for add BA response OTA
 * @context: pointer to global mac
 * @buf: buffer which is nothing but entire ADD BA frame
 * @tx_complete : Sent status
 * @params; tx completion params
 *
 * Return: This returns QDF_STATUS
 */
static QDF_STATUS lim_addba_rsp_tx_complete_cnf(void *context,
						qdf_nbuf_t buf,
						uint32_t tx_complete,
						void *params)
{
	struct mac_context *mac_ctx = (struct mac_context *)context;
	tSirMacMgmtHdr *mac_hdr;
	tDot11faddba_rsp rsp;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint32_t frame_len;
	QDF_STATUS status;
	uint8_t *data;
	struct wmi_mgmt_params *mgmt_params = (struct wmi_mgmt_params *)params;

	if (tx_complete == WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK)
		pe_debug("Add ba response successfully sent");
	else
		pe_debug("Fail to send add ba response");

	if (!buf) {
		pe_err("Addba response frame buffer is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	data = qdf_nbuf_data(buf);

	if (!data) {
		pe_err("Addba response frame is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	mac_hdr = (tSirMacMgmtHdr *)data;
	qdf_mem_zero((void *)&rsp, sizeof(tDot11faddba_rsp));
	frame_len = sizeof(rsp);
	status = dot11f_unpack_addba_rsp(mac_ctx, (uint8_t *)data +
					 sizeof(*mac_hdr), frame_len,
					 &rsp, false);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to unpack and parse (0x%08x, %d bytes)",
			status, frame_len);
		goto error;
	}

	cdp_addba_resp_tx_completion(soc, mac_hdr->da, mgmt_params->vdev_id,
				     rsp.addba_param_set.tid, tx_complete);
error:
	if (buf)
		qdf_nbuf_free(buf);

	return QDF_STATUS_SUCCESS;
}

#define SAE_AUTH_ALGO_LEN 2
#define SAE_AUTH_ALGO_OFFSET 0
static bool lim_is_ack_for_sae_auth(qdf_nbuf_t buf)
{
	tpSirMacMgmtHdr mac_hdr;
	uint16_t *sae_auth, min_len;

	if (!buf) {
		pe_debug("buf is NULL");
		return false;
	}

	min_len = sizeof(tSirMacMgmtHdr) + SAE_AUTH_ALGO_LEN;
	if (qdf_nbuf_len(buf) < min_len) {
		pe_debug("buf_len %d less than min_len %d",
			 (uint32_t)qdf_nbuf_len(buf), min_len);
		return false;
	}

	mac_hdr = (tpSirMacMgmtHdr)(qdf_nbuf_data(buf));
	if (mac_hdr->fc.subType == SIR_MAC_MGMT_AUTH) {
		sae_auth = (uint16_t *)((uint8_t *)mac_hdr +
					sizeof(tSirMacMgmtHdr));
		if (sae_auth[SAE_AUTH_ALGO_OFFSET] ==
		    eSIR_AUTH_TYPE_SAE)
			return true;
	}

	return false;
}

/**
 * lim_auth_tx_complete_cnf()- Confirmation for auth sent over the air
 * @context: pointer to global mac
 * @buf: buffer
 * @tx_complete : Sent status
 * @params; tx completion params
 *
 * Return: This returns QDF_STATUS
 */
static QDF_STATUS lim_auth_tx_complete_cnf(void *context,
					   qdf_nbuf_t buf,
					   uint32_t tx_complete,
					   void *params)
{
	struct mac_context *mac_ctx = (struct mac_context *)context;
	uint16_t auth_ack_status;
	uint16_t reason_code;
	bool sae_auth_acked;

	pe_nofl_rl_info("Auth TX: %s (%d)",
			(tx_complete == WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK) ?
			"success" : "fail", tx_complete);
	if (tx_complete == WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK) {
		mac_ctx->auth_ack_status = LIM_ACK_RCD_SUCCESS;
		auth_ack_status = ACKED;
		reason_code = QDF_STATUS_SUCCESS;
		sae_auth_acked = lim_is_ack_for_sae_auth(buf);
		/*
		 * 'Change' timer for future activations only if ack
		 * received is not for WPA SAE auth frames.
		 */
		if (!sae_auth_acked)
			lim_deactivate_and_change_timer(mac_ctx,
							eLIM_AUTH_RETRY_TIMER);
	} else if (tx_complete == WMI_MGMT_TX_COMP_TYPE_COMPLETE_NO_ACK) {
		mac_ctx->auth_ack_status = LIM_ACK_RCD_FAILURE;
		auth_ack_status = NOT_ACKED;
		reason_code = QDF_STATUS_E_FAILURE;
	} else {
		mac_ctx->auth_ack_status = LIM_TX_FAILED;
		auth_ack_status = SENT_FAIL;
		reason_code = QDF_STATUS_E_FAILURE;
	}

	if (buf)
		qdf_nbuf_free(buf);

	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_AUTH_ACK_EVENT,
				NULL, auth_ack_status, reason_code);

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_send_auth_mgmt_frame() - Send an Authentication frame
 *
 * @mac_ctx: Pointer to Global MAC structure
 * @auth_frame: Pointer to Authentication frame structure
 * @peer_addr: MAC address of destination peer
 * @wep_bit: wep bit in frame control for Authentication frame3
 * @session: PE session information
 *
 * This function is called by lim_process_mlm_messages(). Authentication frame
 * is formatted and sent when this function is called.
 *
 * Return: void
 */
void
lim_send_auth_mgmt_frame(struct mac_context *mac_ctx,
			 tpSirMacAuthFrameBody auth_frame,
			 tSirMacAddr peer_addr,
			 uint8_t wep_challenge_len,
			 struct pe_session *session)
{
	uint8_t *frame, *body;
	uint32_t frame_len = 0, body_len = 0;
	tpSirMacMgmtHdr mac_hdr;
	void *packet;
	QDF_STATUS qdf_status, status;
	uint8_t tx_flag = 0;
	uint8_t vdev_id = 0;
	uint16_t ft_ies_length = 0;
	bool challenge_req = false;
	enum rateid min_rid = RATEID_DEFAULT;
	uint16_t ch_freq_tx_frame = 0;
	int8_t peer_rssi = 0;

	if (!session) {
		pe_err("Error: psession Entry is NULL");
		return;
	}

	vdev_id = session->vdev_id;

	if (wep_challenge_len) {
		/*
		 * Auth frame3 to be sent with encrypted framebody
		 *
		 * Allocate buffer for Authenticaton frame of size
		 * equal to management frame header length plus 2 bytes
		 * each for auth algorithm number, transaction number,
		 * status code, 128 bytes for challenge text and
		 * 4 bytes each for IV & ICV.
		 */
		body_len = wep_challenge_len + LIM_ENCR_AUTH_INFO_LEN;
		frame_len = sizeof(tSirMacMgmtHdr) + body_len;

		goto alloc_packet;
	}

	switch (auth_frame->authTransactionSeqNumber) {
	case SIR_MAC_AUTH_FRAME_1:
		/*
		 * Allocate buffer for Authenticaton frame of size
		 * equal to management frame header length plus 2 bytes
		 * each for auth algorithm number, transaction number
		 * and status code.
		 */

		body_len = SIR_MAC_AUTH_FRAME_INFO_LEN;
		frame_len = sizeof(tSirMacMgmtHdr) + body_len;

		status = lim_create_fils_auth_data(mac_ctx, auth_frame,
						   session, &frame_len);
		if (QDF_IS_STATUS_ERROR(status))
			return;

		if (auth_frame->authAlgoNumber == eSIR_FT_AUTH) {
			if (session->ftPEContext.pFTPreAuthReq &&
			    0 != session->ftPEContext.pFTPreAuthReq->
				ft_ies_length) {
				ft_ies_length = session->ftPEContext.
					pFTPreAuthReq->ft_ies_length;
				frame_len += ft_ies_length;
				pe_debug("Auth frame, FTIES length added=%d",
					ft_ies_length);
			} else {
				pe_debug("Auth frame, Does not contain FTIES!");
				frame_len += (2 + SIR_MDIE_SIZE);
			}
		}

		/* include MDIE in FILS authentication frame */
		if (session->lim_join_req &&
		    session->lim_join_req->is11Rconnection &&
		    auth_frame->authAlgoNumber == SIR_FILS_SK_WITHOUT_PFS &&
		    session->lim_join_req->bssDescription.mdiePresent)
			frame_len += (2 + SIR_MDIE_SIZE);
		break;

	case SIR_MAC_AUTH_FRAME_2:
		if ((auth_frame->authAlgoNumber == eSIR_OPEN_SYSTEM) ||
		    ((auth_frame->authAlgoNumber == eSIR_SHARED_KEY) &&
			(auth_frame->authStatusCode != STATUS_SUCCESS))) {
			/*
			 * Allocate buffer for Authenticaton frame of size
			 * equal to management frame header length plus
			 * 2 bytes each for auth algorithm number,
			 * transaction number and status code.
			 */

			body_len = SIR_MAC_AUTH_FRAME_INFO_LEN;
			frame_len = sizeof(tSirMacMgmtHdr) + body_len;
		} else {
			/*
			 * Shared Key algorithm with challenge text
			 * to be sent.
			 *
			 * Allocate buffer for Authenticaton frame of size
			 * equal to management frame header length plus
			 * 2 bytes each for auth algorithm number,
			 * transaction number, status code and 128 bytes
			 * for challenge text.
			 */

			challenge_req = true;
			body_len = SIR_MAC_AUTH_FRAME_INFO_LEN +
				   SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH +
				   SIR_MAC_CHALLENGE_ID_LEN;
			frame_len = sizeof(tSirMacMgmtHdr) + body_len;
		}
		break;

	case SIR_MAC_AUTH_FRAME_3:
		/*
		 * Auth frame3 to be sent without encrypted framebody
		 *
		 * Allocate buffer for Authenticaton frame of size equal
		 * to management frame header length plus 2 bytes each
		 * for auth algorithm number, transaction number and
		 * status code.
		 */

		body_len = SIR_MAC_AUTH_FRAME_INFO_LEN;
		frame_len = sizeof(tSirMacMgmtHdr) + body_len;
		break;

	case SIR_MAC_AUTH_FRAME_4:
		/*
		 * Allocate buffer for Authenticaton frame of size equal
		 * to management frame header length plus 2 bytes each
		 * for auth algorithm number, transaction number and
		 * status code.
		 */

		body_len = SIR_MAC_AUTH_FRAME_INFO_LEN;
		frame_len = sizeof(tSirMacMgmtHdr) + body_len;

		break;
	default:
		pe_err("Invalid auth transaction seq num");
		return;
	} /* switch (auth_frame->authTransactionSeqNumber) */

alloc_packet:
	qdf_status = cds_packet_alloc((uint16_t) frame_len, (void **)&frame,
				 (void **)&packet);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("call to bufAlloc failed for AUTH frame");
		return;
	}

	qdf_mem_zero(frame, frame_len);

	/* Prepare BD */
	lim_populate_mac_header(mac_ctx, frame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_AUTH, peer_addr, session->self_mac_addr);
	mac_hdr = (tpSirMacMgmtHdr) frame;
	if (wep_challenge_len)
		mac_hdr->fc.wep = LIM_WEP_IN_FC;
	else
		mac_hdr->fc.wep = LIM_NO_WEP_IN_FC;

	/* Prepare BSSId */
	if (LIM_IS_AP_ROLE(session))
		qdf_mem_copy((uint8_t *) mac_hdr->bssId,
			     (uint8_t *) session->bssId,
			     sizeof(tSirMacAddr));

	/* Prepare Authentication frame body */
	body = frame + sizeof(tSirMacMgmtHdr);

	if (wep_challenge_len) {
		qdf_mem_copy(body, (uint8_t *) auth_frame, body_len);
	} else {
		*((uint16_t *) (body)) =
			sir_swap_u16if_needed(auth_frame->authAlgoNumber);
		body += sizeof(uint16_t);
		body_len -= sizeof(uint16_t);

		*((uint16_t *) (body)) =
			sir_swap_u16if_needed(
				auth_frame->authTransactionSeqNumber);
		body += sizeof(uint16_t);
		body_len -= sizeof(uint16_t);

		*((uint16_t *) (body)) =
			sir_swap_u16if_needed(auth_frame->authStatusCode);
		body += sizeof(uint16_t);
		body_len -= sizeof(uint16_t);

		if (challenge_req) {
			if (body_len < SIR_MAC_AUTH_CHALLENGE_BODY_LEN) {
				/* copy challenge IE id, len, challenge text */
				*body = auth_frame->type;
				body++;
				body_len -= sizeof(uint8_t);
				*body = auth_frame->length;
				body++;
				body_len -= sizeof(uint8_t);
				qdf_mem_copy(body, auth_frame->challengeText,
					     body_len);
				pe_err("Incomplete challenge info: length: %d, expected: %d",
				       body_len,
				       SIR_MAC_AUTH_CHALLENGE_BODY_LEN);
				body += body_len;
				body_len = 0;
			} else {
				/* copy challenge IE id, len, challenge text */
				*body = auth_frame->type;
				body++;
				*body = auth_frame->length;
				body++;
				qdf_mem_copy(body, auth_frame->challengeText,
					     SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH);
				body += SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH;
				body_len -= SIR_MAC_SAP_AUTH_CHALLENGE_LENGTH +
					    SIR_MAC_CHALLENGE_ID_LEN;
			}
		}

		if ((auth_frame->authAlgoNumber == eSIR_FT_AUTH) &&
		    (auth_frame->authTransactionSeqNumber ==
		     SIR_MAC_AUTH_FRAME_1) &&
		     (session->ftPEContext.pFTPreAuthReq)) {

			if (ft_ies_length > 0) {
				qdf_mem_copy(body,
					session->ftPEContext.
						pFTPreAuthReq->ft_ies,
					ft_ies_length);
				pe_debug("Auth1 Frame FTIE is: ");
				QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE,
						   QDF_TRACE_LEVEL_DEBUG,
						   (uint8_t *) body,
						   ft_ies_length);
			} else if (session->ftPEContext.
					pFTPreAuthReq->pbssDescription) {
				/* MDID attr is 54 */
				*body = WLAN_ELEMID_MOBILITY_DOMAIN;
				body++;
				*body = SIR_MDIE_SIZE;
				body++;
				qdf_mem_copy(body,
					&session->ftPEContext.pFTPreAuthReq->
						pbssDescription->mdie[0],
					SIR_MDIE_SIZE);
			}
		} else if ((auth_frame->authAlgoNumber ==
					SIR_FILS_SK_WITHOUT_PFS) &&
			   (auth_frame->authTransactionSeqNumber ==
						SIR_MAC_AUTH_FRAME_1)) {
			pe_debug("FILS: appending fils Auth data");
			lim_add_fils_data_to_auth_frame(session, body);
		}
	}

	pe_nofl_info("Auth TX: seq %d seq num %d status %d WEP %d to " QDF_MAC_ADDR_FMT,
		     auth_frame->authTransactionSeqNumber, mac_ctx->mgmtSeqNum,
		     auth_frame->authStatusCode, mac_hdr->fc.wep,
		     QDF_MAC_ADDR_REF(mac_hdr->da));
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			   frame, frame_len);

	if ((session->ftPEContext.pFTPreAuthReq) &&
	    (!wlan_reg_is_24ghz_ch_freq(
	     session->ftPEContext.pFTPreAuthReq->pre_auth_channel_freq)))
		tx_flag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
	else if (!wlan_reg_is_24ghz_ch_freq(session->curr_op_freq) ||
		 session->opmode == QDF_P2P_CLIENT_MODE ||
		 session->opmode == QDF_P2P_GO_MODE)
		tx_flag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	if (session->opmode == QDF_P2P_CLIENT_MODE ||
	    session->opmode == QDF_STA_MODE)
		tx_flag |= HAL_USE_PEER_STA_REQUESTED_MASK;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 session->peSessionId, mac_hdr->fc.subType));

	mac_ctx->auth_ack_status = LIM_ACK_NOT_RCD;
	min_rid = lim_get_min_session_txrate(session);
	peer_rssi = mac_ctx->lim.bss_rssi;
	lim_diag_mgmt_tx_event_report(mac_ctx, mac_hdr,
				      session, QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);

	if (session->ftPEContext.pFTPreAuthReq)
		ch_freq_tx_frame = session->ftPEContext.
				pFTPreAuthReq->pre_auth_channel_freq;

	qdf_status = wma_tx_frameWithTxComplete(mac_ctx, packet,
				 (uint16_t)frame_len,
				 TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS,
				 7, lim_tx_complete, frame,
				 lim_auth_tx_complete_cnf,
				 tx_flag, vdev_id, false,
				 ch_freq_tx_frame, min_rid, peer_rssi);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
		session->peSessionId, qdf_status));
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("*** Could not send Auth frame, retCode=%X ***",
			qdf_status);
		mac_ctx->auth_ack_status = LIM_TX_FAILED;
		lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_AUTH_ACK_EVENT,
				session, SENT_FAIL, QDF_STATUS_E_FAILURE);
	/* Pkt will be freed up by the callback */
	}
	return;
}

QDF_STATUS lim_send_deauth_cnf(struct mac_context *mac_ctx)
{
	uint16_t aid;
	tpDphHashNode sta_ds;
	tLimMlmDeauthReq *deauth_req;
	tLimMlmDeauthCnf deauth_cnf;
	struct pe_session *session_entry;
	QDF_STATUS qdf_status;

	deauth_req = mac_ctx->lim.limDisassocDeauthCnfReq.pMlmDeauthReq;
	if (deauth_req) {
		if (tx_timer_running(
			&mac_ctx->lim.lim_timers.gLimDeauthAckTimer))
			lim_deactivate_and_change_timer(mac_ctx,
							eLIM_DEAUTH_ACK_TIMER);

		session_entry = pe_find_session_by_session_id(mac_ctx,
					deauth_req->sessionId);
		if (!session_entry) {
			pe_err("session does not exist for given sessionId");
			deauth_cnf.resultCode =
				eSIR_SME_INVALID_PARAMETERS;
			goto end;
		}
		if (qdf_is_macaddr_broadcast(&deauth_req->peer_macaddr) &&
		    mac_ctx->mlme_cfg->sap_cfg.is_sap_bcast_deauth_enabled) {
			qdf_status = lim_del_sta_all(mac_ctx, session_entry);
			qdf_mem_free(deauth_req);
			mac_ctx->lim.limDisassocDeauthCnfReq.pMlmDeauthReq =
									 NULL;
			return qdf_status;
		}

		sta_ds =
			dph_lookup_hash_entry(mac_ctx,
					      deauth_req->peer_macaddr.bytes,
					      &aid,
					      &session_entry->
					      dph.dphHashTable);
		if (!sta_ds) {
			deauth_cnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
			goto end;
		}

		/* / Receive path cleanup with dummy packet */
		lim_ft_cleanup_pre_auth_info(mac_ctx, session_entry);
		lim_cleanup_rx_path(mac_ctx, sta_ds, session_entry, true);
		if ((session_entry->limSystemRole == eLIM_STA_ROLE) &&
		    (
#ifdef FEATURE_WLAN_ESE
		    (session_entry->isESEconnection) ||
#endif
		    (session_entry->isFastRoamIniFeatureEnabled) ||
		    (session_entry->is11Rconnection))) {
			pe_debug("FT Preauth (%pK,%d) Deauth rc %d src = %d",
				 session_entry,
				 session_entry->peSessionId,
				 deauth_req->reasonCode,
				 deauth_req->deauthTrigger);
			lim_ft_cleanup(mac_ctx, session_entry);
		} else {
#ifdef FEATURE_WLAN_ESE
			pe_debug("No FT Preauth Session Cleanup in role %d"
				 " isESE %d"
				 " isLFR %d"
				 " is11r %d, Deauth reason %d Trigger = %d",
				 session_entry->limSystemRole,
				 session_entry->isESEconnection,
				 session_entry->isFastRoamIniFeatureEnabled,
				 session_entry->is11Rconnection,
				 deauth_req->reasonCode,
				 deauth_req->deauthTrigger);
#else
			pe_debug("No FT Preauth Session Cleanup in role %d"
				 " isLFR %d"
				 " is11r %d, Deauth reason %d Trigger = %d",
				 session_entry->limSystemRole,
				 session_entry->isFastRoamIniFeatureEnabled,
				 session_entry->is11Rconnection,
				 deauth_req->reasonCode,
				 deauth_req->deauthTrigger);
#endif
		}
		/* Free up buffer allocated for mlmDeauthReq */
		qdf_mem_free(deauth_req);
		mac_ctx->lim.limDisassocDeauthCnfReq.pMlmDeauthReq = NULL;
	}
	return QDF_STATUS_SUCCESS;
end:
	qdf_copy_macaddr(&deauth_cnf.peer_macaddr,
			 &deauth_req->peer_macaddr);
	deauth_cnf.deauthTrigger = deauth_req->deauthTrigger;
	deauth_cnf.aid = deauth_req->aid;
	deauth_cnf.sessionId = deauth_req->sessionId;

	/* Free up buffer allocated */
	/* for mlmDeauthReq */
	qdf_mem_free(deauth_req);
	mac_ctx->lim.limDisassocDeauthCnfReq.pMlmDeauthReq = NULL;

	lim_post_sme_message(mac_ctx,
			     LIM_MLM_DEAUTH_CNF, (uint32_t *) &deauth_cnf);
	return QDF_STATUS_SUCCESS;
}

/**
 * lim_send_disassoc_cnf() - Send disassoc confirmation to SME
 *
 * @mac_ctx: Handle to MAC context
 *
 * Sends disassoc confirmation to SME. Removes disassoc request stored
 * in lim.
 *
 * Return: QDF_STATUS_SUCCESS
 */

QDF_STATUS lim_send_disassoc_cnf(struct mac_context *mac_ctx)
{
	uint16_t aid;
	tpDphHashNode sta_ds;
	tLimMlmDisassocCnf disassoc_cnf;
	struct pe_session *pe_session;
	tLimMlmDisassocReq *disassoc_req;

	disassoc_req = mac_ctx->lim.limDisassocDeauthCnfReq.pMlmDisassocReq;
	if (disassoc_req) {
		if (tx_timer_running(
			&mac_ctx->lim.lim_timers.gLimDisassocAckTimer))
			lim_deactivate_and_change_timer(mac_ctx,
				eLIM_DISASSOC_ACK_TIMER);

		pe_session = pe_find_session_by_session_id(
					mac_ctx, disassoc_req->sessionId);
		if (!pe_session) {
			pe_err("No session for given sessionId");
			disassoc_cnf.resultCode =
				eSIR_SME_INVALID_PARAMETERS;
			goto end;
		}

		sta_ds = dph_lookup_hash_entry(mac_ctx,
				disassoc_req->peer_macaddr.bytes, &aid,
				&pe_session->dph.dphHashTable);
		if (!sta_ds) {
			pe_err("StaDs Null");
			disassoc_cnf.resultCode = eSIR_SME_INVALID_PARAMETERS;
			goto end;
		}
		/* Receive path cleanup with dummy packet */
		if (QDF_STATUS_SUCCESS !=
		    lim_cleanup_rx_path(mac_ctx, sta_ds, pe_session, true)) {
			disassoc_cnf.resultCode =
				eSIR_SME_RESOURCES_UNAVAILABLE;
			pe_err("cleanup_rx_path error");
			goto end;
		}
		if (LIM_IS_STA_ROLE(pe_session) &&
		    (disassoc_req->reasonCode !=
		     REASON_AUTHORIZED_ACCESS_LIMIT_REACHED)) {
			pe_debug("FT Preauth Session (%pK %d) Clean up",
				 pe_session, pe_session->peSessionId);

			/* Delete FT session if there exists one */
			lim_ft_cleanup_pre_auth_info(mac_ctx, pe_session);
		}
		/* Free up buffer allocated for mlmDisassocReq */
		qdf_mem_free(disassoc_req);
		mac_ctx->lim.limDisassocDeauthCnfReq.pMlmDisassocReq = NULL;
		return QDF_STATUS_SUCCESS;
	} else {
		return QDF_STATUS_SUCCESS;
	}
end:
	qdf_mem_copy((uint8_t *) &disassoc_cnf.peerMacAddr,
		     (uint8_t *) disassoc_req->peer_macaddr.bytes,
		     QDF_MAC_ADDR_SIZE);
	disassoc_cnf.aid = disassoc_req->aid;
	disassoc_cnf.disassocTrigger = disassoc_req->disassocTrigger;

	/* Update PE session ID */
	disassoc_cnf.sessionId = disassoc_req->sessionId;

	if (disassoc_req) {
		/* / Free up buffer allocated for mlmDisassocReq */
		qdf_mem_free(disassoc_req);
		mac_ctx->lim.limDisassocDeauthCnfReq.pMlmDisassocReq = NULL;
	}

	lim_post_sme_message(mac_ctx, LIM_MLM_DISASSOC_CNF,
		(uint32_t *) &disassoc_cnf);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_disassoc_tx_complete_cnf(void *context,
					uint32_t tx_success,
					void *params)
{
	struct mac_context *max_ctx = (struct mac_context *)context;

	pe_debug("tx_success: %d", tx_success);

	return lim_send_disassoc_cnf(max_ctx);
}

static QDF_STATUS lim_disassoc_tx_complete_cnf_handler(void *context,
						       qdf_nbuf_t buf,
						       uint32_t tx_success,
						       void *params)
{
	struct mac_context *max_ctx = (struct mac_context *)context;
	QDF_STATUS status_code;
	struct scheduler_msg msg = {0};

	pe_debug("tx_success: %d", tx_success);

	if (buf)
		qdf_nbuf_free(buf);
	msg.type = (uint16_t) WMA_DISASSOC_TX_COMP;
	msg.bodyptr = params;
	msg.bodyval = tx_success;

	status_code = lim_post_msg_high_priority(max_ctx, &msg);
	if (status_code != QDF_STATUS_SUCCESS)
		pe_err("posting message: %X to LIM failed, reason: %d",
		       msg.type, status_code);
	return status_code;
}

QDF_STATUS lim_deauth_tx_complete_cnf(void *context,
				      uint32_t tx_success,
				      void *params)
{
	struct mac_context *mac_ctx = (struct mac_context *)context;

	pe_debug("tx_success: %d", tx_success);

	return lim_send_deauth_cnf(mac_ctx);
}

static QDF_STATUS lim_deauth_tx_complete_cnf_handler(void *context,
						     qdf_nbuf_t buf,
						     uint32_t tx_success,
						     void *params)
{
	struct mac_context *mac_ctx = (struct mac_context *)context;
	QDF_STATUS status_code;
	struct scheduler_msg msg = {0};
	tLimMlmDeauthReq *deauth_req;
	struct pe_session *session = NULL;

	deauth_req = mac_ctx->lim.limDisassocDeauthCnfReq.pMlmDeauthReq;

	pe_debug("tx_complete = %s tx_success = %d",
		(tx_success == WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK) ?
		 "success" : "fail", tx_success);

	if (buf)
		qdf_nbuf_free(buf);
	if (deauth_req)
		session = pe_find_session_by_session_id(mac_ctx,
				deauth_req->sessionId);
	if (tx_success != WMI_MGMT_TX_COMP_TYPE_COMPLETE_OK && session &&
	    session->deauth_retry.retry_cnt) {
		if (tx_timer_running(
			&mac_ctx->lim.lim_timers.gLimDeauthAckTimer))
			lim_deactivate_and_change_timer(mac_ctx,
							eLIM_DEAUTH_ACK_TIMER);
		lim_send_deauth_mgmt_frame(mac_ctx,
				session->deauth_retry.reason_code,
				session->deauth_retry.peer_macaddr.bytes,
				session, true);
		session->deauth_retry.retry_cnt--;
		return QDF_STATUS_SUCCESS;
	}
	msg.type = (uint16_t) WMA_DEAUTH_TX_COMP;
	msg.bodyptr = params;
	msg.bodyval = tx_success;

	status_code = lim_post_msg_high_priority(mac_ctx, &msg);
	if (status_code != QDF_STATUS_SUCCESS)
		pe_err("posting message: %X to LIM failed, reason: %d",
		       msg.type, status_code);
	return status_code;
}

/**
 * lim_append_ies_to_frame() - Append IEs to the frame
 *
 * @frame: Pointer to the frame buffer that needs to be populated
 * @frame_len: Pointer for current frame length
 * @ie: pointer for disconnect IEs
 *
 * This function is called by lim_send_disassoc_mgmt_frame and
 * lim_send_deauth_mgmt_frame APIs as part of disconnection.
 * Append IEs and update frame length.
 *
 * Return: None
 */
static void
lim_append_ies_to_frame(uint8_t *frame, uint32_t *frame_len,
			struct wlan_ies *ie)
{
	if (!ie || !ie->len || !ie->data)
		return;
	qdf_mem_copy(frame, ie->data, ie->len);
	*frame_len += ie->len;
	pe_debug("Appended IEs len: %u", ie->len);
}

/**
 * \brief This function is called to send Disassociate frame.
 *
 *
 * \param mac Pointer to Global MAC structure
 *
 * \param nReason Indicates the reason that need to be sent in
 * Disassociation frame
 *
 * \param peerMacAddr MAC address of the STA to which Disassociation frame is
 * sent
 *
 *
 */

void
lim_send_disassoc_mgmt_frame(struct mac_context *mac,
			     uint16_t nReason,
			     tSirMacAddr peer,
			     struct pe_session *pe_session, bool waitForAck)
{
	tDot11fDisassociation frm;
	uint8_t *pFrame;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t nBytes, nPayload, nStatus;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint8_t txFlag = 0;
	uint32_t val = 0;
	uint8_t smeSessionId = 0;
	struct wlan_ies *discon_ie;

	if (!pe_session) {
		return;
	}

	/*
	 * In case when cac timer is running for this SAP session then
	 * avoid sending disassoc out. It is violation of dfs specification.
	 */
	if (((pe_session->opmode == QDF_SAP_MODE) ||
	     (pe_session->opmode == QDF_P2P_GO_MODE)) &&
	    (true == mac->sap.SapDfsInfo.is_dfs_cac_timer_running)) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO,
			  FL("CAC timer is running, drop disassoc from going out"));
		if (waitForAck)
			lim_send_disassoc_cnf(mac);
		return;
	}
	smeSessionId = pe_session->smeSessionId;

	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));

	frm.Reason.code = nReason;

	nStatus = dot11f_get_packed_disassociation_size(mac, &frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to calculate the packed size for a Disassociation (0x%08x)",
			nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fDisassociation);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while calculating the packed size for a Disassociation (0x%08x)",
			nStatus);
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);

	discon_ie = mlme_get_self_disconnect_ies(pe_session->vdev);
	if (discon_ie && discon_ie->len)
		nBytes += discon_ie->len;

	qdf_status = cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				      (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a Disassociation",
			nBytes);
		if (waitForAck)
			lim_send_disassoc_cnf(mac);
		return;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_DISASSOC, peer, pe_session->self_mac_addr);
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	/* Prepare the BSSID */
	sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);

	if (mac->is_usr_cfg_pmf_wep != PMF_WEP_DISABLE)
		lim_set_protected_bit(mac, pe_session, peer, pMacHdr);
	else
		pe_debug("Skip WEP bit setting per usr cfg");

	if (mac->is_usr_cfg_pmf_wep == PMF_INCORRECT_KEY)
		txFlag |= HAL_USE_INCORRECT_KEY_PMF;

	nStatus = dot11f_pack_disassociation(mac, &frm, pFrame +
					     sizeof(tSirMacMgmtHdr),
					     nPayload, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to pack a Disassociation (0x%08x)",
			nStatus);
		cds_packet_free((void *)pPacket);
		if (waitForAck)
			lim_send_disassoc_cnf(mac);
		return;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while packing a Disassociation (0x%08x)",
			nStatus);
	}

	/* Copy disconnect IEs to the end of the frame */
	lim_append_ies_to_frame(pFrame + sizeof(tSirMacMgmtHdr) + nPayload,
				&nPayload, discon_ie);
	mlme_free_self_disconnect_ies(pe_session->vdev);

	pe_nofl_info("Disassoc TX: vdev %d seq %d reason %u and waitForAck %d to " QDF_MAC_ADDR_FMT " From " QDF_MAC_ADDR_FMT,
		     pe_session->vdev_id, mac->mgmtSeqNum, nReason, waitForAck,
		     QDF_MAC_ADDR_REF(pMacHdr->da),
		     QDF_MAC_ADDR_REF(pe_session->self_mac_addr));

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	txFlag |= HAL_USE_PEER_STA_REQUESTED_MASK;

	if (waitForAck) {
		MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
				 pe_session->peSessionId,
				 pMacHdr->fc.subType));
		lim_diag_mgmt_tx_event_report(mac, pMacHdr,
					      pe_session, QDF_STATUS_SUCCESS,
					      QDF_STATUS_SUCCESS);
		/* Queue Disassociation frame in high priority WQ */
		/* get the duration from the request */
		qdf_status =
			wma_tx_frameWithTxComplete(mac, pPacket, (uint16_t) nBytes,
					 TXRX_FRM_802_11_MGMT,
					 ANI_TXDIR_TODS, 7, lim_tx_complete,
					 pFrame, lim_disassoc_tx_complete_cnf_handler,
					 txFlag, smeSessionId, false, 0,
					 RATEID_DEFAULT, 0);
		MTRACE(qdf_trace
			       (QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			       pe_session->peSessionId, qdf_status));

		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			pe_err("Failed to send disassoc frame");
			lim_send_disassoc_cnf(mac);
			return;
		}

		val = SYS_MS_TO_TICKS(LIM_DISASSOC_DEAUTH_ACK_TIMEOUT);

		if (tx_timer_change
			    (&mac->lim.lim_timers.gLimDisassocAckTimer, val, 0)
		    != TX_SUCCESS) {
			pe_err("Unable to change Disassoc ack Timer val");
			return;
		} else if (TX_SUCCESS !=
			   tx_timer_activate(&mac->lim.lim_timers.
					     gLimDisassocAckTimer)) {
			pe_err("Unable to activate Disassoc ack Timer");
			lim_deactivate_and_change_timer(mac,
							eLIM_DISASSOC_ACK_TIMER);
			return;
		}
	} else {
		MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
				 pe_session->peSessionId,
				 pMacHdr->fc.subType));
		lim_diag_mgmt_tx_event_report(mac, pMacHdr,
					      pe_session,
					      QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
		/* Queue Disassociation frame in high priority WQ */
		qdf_status = wma_tx_frame(mac, pPacket, (uint16_t) nBytes,
					TXRX_FRM_802_11_MGMT,
					ANI_TXDIR_TODS,
					7,
					lim_tx_complete, pFrame, txFlag,
					smeSessionId, 0, RATEID_DEFAULT, 0);
		MTRACE(qdf_trace
			       (QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			       pe_session->peSessionId, qdf_status));
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			pe_err("Failed to send Disassociation (%X)!",
				qdf_status);
			/* Pkt will be freed up by the callback */
		}
	}
} /* End lim_send_disassoc_mgmt_frame. */

/**
 * \brief This function is called to send a Deauthenticate frame
 *
 *
 * \param mac Pointer to global MAC structure
 *
 * \param nReason Indicates the reason that need to be sent in the
 * Deauthenticate frame
 *
 * \param peeer address of the STA to which the frame is to be sent
 *
 *
 */

void
lim_send_deauth_mgmt_frame(struct mac_context *mac,
			   uint16_t nReason,
			   tSirMacAddr peer,
			   struct pe_session *pe_session, bool waitForAck)
{
	tDot11fDeAuth frm;
	uint8_t *pFrame;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t nBytes, nPayload, nStatus;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint8_t txFlag = 0;
	uint32_t val = 0;
#ifdef FEATURE_WLAN_TDLS
	uint16_t aid;
	tpDphHashNode sta;
#endif
	uint8_t smeSessionId = 0;
	struct wlan_ies *discon_ie;

	if (!pe_session) {
		return;
	}

	/*
	 * In case when cac timer is running for this SAP session then
	 * avoid deauth frame out. It is violation of dfs specification.
	 */
	if (((pe_session->opmode == QDF_SAP_MODE) ||
	    (pe_session->opmode == QDF_P2P_GO_MODE)) &&
	    (true == mac->sap.SapDfsInfo.is_dfs_cac_timer_running)) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_INFO,
			  FL
				  ("CAC timer is running, drop the deauth from going out"));
		if (waitForAck)
			lim_send_deauth_cnf(mac);
		return;
	}
	smeSessionId = pe_session->smeSessionId;

	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));

	frm.Reason.code = nReason;

	nStatus = dot11f_get_packed_de_auth_size(mac, &frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to calculate the packed size for a De-Authentication (0x%08x)",
			nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fDeAuth);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while calculating the packed size for a De-Authentication (0x%08x)",
			nStatus);
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);
	discon_ie = mlme_get_self_disconnect_ies(pe_session->vdev);
	if (discon_ie && discon_ie->len)
		nBytes += discon_ie->len;

	qdf_status = cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				      (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a De-Authentication",
			nBytes);
		if (waitForAck)
			lim_send_deauth_cnf(mac);
		return;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_DEAUTH, peer, pe_session->self_mac_addr);
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	/* Prepare the BSSID */
	sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);

	lim_set_protected_bit(mac, pe_session, peer, pMacHdr);

	nStatus = dot11f_pack_de_auth(mac, &frm, pFrame +
				      sizeof(tSirMacMgmtHdr), nPayload, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to pack a DeAuthentication (0x%08x)",
			nStatus);
		cds_packet_free((void *)pPacket);
		if (waitForAck)
			lim_send_deauth_cnf(mac);
		return;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while packing a De-Authentication (0x%08x)",
			nStatus);
	}

	/* Copy disconnect IEs to the end of the frame */
	lim_append_ies_to_frame(pFrame + sizeof(tSirMacMgmtHdr) + nPayload,
				&nPayload, discon_ie);
	mlme_free_self_disconnect_ies(pe_session->vdev);

	pe_nofl_rl_info("Deauth TX: vdev %d seq_num %d reason %u waitForAck %d to " QDF_MAC_ADDR_FMT " from " QDF_MAC_ADDR_FMT,
			pe_session->vdev_id, mac->mgmtSeqNum, nReason,
			waitForAck, QDF_MAC_ADDR_REF(pMacHdr->da),
			QDF_MAC_ADDR_REF(pe_session->self_mac_addr));

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	txFlag |= HAL_USE_PEER_STA_REQUESTED_MASK;
#ifdef FEATURE_WLAN_TDLS
	sta =
		dph_lookup_hash_entry(mac, peer, &aid,
				      &pe_session->dph.dphHashTable);
#endif

	if (waitForAck) {
		MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
				 pe_session->peSessionId,
				 pMacHdr->fc.subType));
		lim_diag_mgmt_tx_event_report(mac, pMacHdr,
					      pe_session,
					      QDF_STATUS_SUCCESS, QDF_STATUS_SUCCESS);
		if (pe_session->opmode == QDF_STA_MODE &&
		    mac->mlme_cfg->sta.deauth_retry_cnt &&
		    !pe_session->deauth_retry.retry_cnt) {
			pe_session->deauth_retry.retry_cnt =
				mac->mlme_cfg->sta.deauth_retry_cnt;
			pe_session->deauth_retry.reason_code = nReason;
			qdf_mem_copy(pe_session->deauth_retry.peer_macaddr.bytes,
					 peer, QDF_MAC_ADDR_SIZE);
		}
		/* Queue Disassociation frame in high priority WQ */
		qdf_status =
			wma_tx_frameWithTxComplete(mac, pPacket, (uint16_t) nBytes,
					 TXRX_FRM_802_11_MGMT,
					 ANI_TXDIR_TODS, 7, lim_tx_complete,
					 pFrame, lim_deauth_tx_complete_cnf_handler,
					 txFlag, smeSessionId, false, 0,
					 RATEID_DEFAULT, 0);
		MTRACE(qdf_trace
			       (QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			       pe_session->peSessionId, qdf_status));
		/* Pkt will be freed up by the callback lim_tx_complete */
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			pe_err("Failed to send De-Authentication (%X)!",
				qdf_status);

			/* Call lim_process_deauth_ack_timeout which will send
			 * DeauthCnf for this frame
			 */
			lim_process_deauth_ack_timeout(mac);
			return;
		}

		val = SYS_MS_TO_TICKS(LIM_DISASSOC_DEAUTH_ACK_TIMEOUT);

		if (tx_timer_change
			    (&mac->lim.lim_timers.gLimDeauthAckTimer, val, 0)
		    != TX_SUCCESS) {
			pe_err("Unable to change Deauth ack Timer val");
			return;
		} else if (TX_SUCCESS !=
			   tx_timer_activate(&mac->lim.lim_timers.
					     gLimDeauthAckTimer)) {
			pe_err("Unable to activate Deauth ack Timer");
			lim_deactivate_and_change_timer(mac,
							eLIM_DEAUTH_ACK_TIMER);
			return;
		}
	} else {
		MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
				 pe_session->peSessionId,
				 pMacHdr->fc.subType));
#ifdef FEATURE_WLAN_TDLS
		if ((sta)
		    && (STA_ENTRY_TDLS_PEER == sta->staType)) {
			/* Queue Disassociation frame in high priority WQ */
			lim_diag_mgmt_tx_event_report(mac, pMacHdr,
						      pe_session,
						      QDF_STATUS_SUCCESS,
						      QDF_STATUS_SUCCESS);
			qdf_status =
				wma_tx_frame(mac, pPacket, (uint16_t) nBytes,
					   TXRX_FRM_802_11_MGMT, ANI_TXDIR_IBSS,
					   7, lim_tx_complete, pFrame, txFlag,
					   smeSessionId, 0, RATEID_DEFAULT, 0);
		} else {
#endif
		lim_diag_mgmt_tx_event_report(mac, pMacHdr,
					      pe_session,
					      QDF_STATUS_SUCCESS,
					      QDF_STATUS_SUCCESS);
		/* Queue Disassociation frame in high priority WQ */
		qdf_status =
			wma_tx_frame(mac, pPacket, (uint16_t) nBytes,
				   TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS,
				   7, lim_tx_complete, pFrame, txFlag,
				   smeSessionId, 0, RATEID_DEFAULT, 0);
#ifdef FEATURE_WLAN_TDLS
	}
#endif
		MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
				 pe_session->peSessionId, qdf_status));
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			pe_err("Failed to send De-Authentication (%X)!",
				qdf_status);
			/* Pkt will be freed up by the callback */
		}
	}

} /* End lim_send_deauth_mgmt_frame. */

#ifdef ANI_SUPPORT_11H
/**
 * \brief Send a Measurement Report Action frame
 *
 *
 * \param mac Pointer to the global MAC structure
 *
 * \param pMeasReqFrame Address of a tSirMacMeasReqActionFrame
 *
 * \return QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE else
 *
 *
 */

QDF_STATUS
lim_send_meas_report_frame(struct mac_context *mac,
			   tpSirMacMeasReqActionFrame pMeasReqFrame,
			   tSirMacAddr peer, struct pe_session *pe_session)
{
	tDot11fMeasurementReport frm;
	uint8_t *pFrame;
	QDF_STATUS nSirStatus;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t nBytes, nPayload, nStatus;
	void *pPacket;
	QDF_STATUS qdf_status;

	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));

	frm.Category.category = ACTION_CATEGORY_SPECTRUM_MGMT;
	frm.Action.action = ACTION_SPCT_MSR_RPRT;
	frm.DialogToken.token = pMeasReqFrame->actionHeader.dialogToken;

	switch (pMeasReqFrame->measReqIE.measType) {
	case SIR_MAC_BASIC_MEASUREMENT_TYPE:
		nSirStatus =
			populate_dot11f_measurement_report0(mac, pMeasReqFrame,
							    &frm.MeasurementReport);
		break;
	case SIR_MAC_CCA_MEASUREMENT_TYPE:
		nSirStatus =
			populate_dot11f_measurement_report1(mac, pMeasReqFrame,
							    &frm.MeasurementReport);
		break;
	case SIR_MAC_RPI_MEASUREMENT_TYPE:
		nSirStatus =
			populate_dot11f_measurement_report2(mac, pMeasReqFrame,
							    &frm.MeasurementReport);
		break;
	default:
		pe_err("Unknown measurement type %d in limSendMeasReportFrame",
			pMeasReqFrame->measReqIE.measType);
		return QDF_STATUS_E_FAILURE;
	}

	if (QDF_STATUS_SUCCESS != nSirStatus)
		return QDF_STATUS_E_FAILURE;

	nStatus = dot11f_get_packed_measurement_report_size(mac, &frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to calculate the packed size for a Measurement Report (0x%08x)",
			nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fMeasurementReport);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while calculating the packed size for a Measurement Report (0x%08x)",
			nStatus);
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);

	qdf_status =
		cds_packet_alloc(mac->hdd_handle, TXRX_FRM_802_11_MGMT,
				 (uint16_t) nBytes, (void **)&pFrame,
				 (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a De-Authentication",
		       nBytes);
		return QDF_STATUS_E_FAILURE;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer);
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	qdf_mem_copy(pMacHdr->bssId, pe_session->bssId, sizeof(tSirMacAddr));

	lim_set_protected_bit(mac, pe_session, peer, pMacHdr);

	nStatus = dot11f_pack_measurement_report(mac, &frm, pFrame +
						 sizeof(tSirMacMgmtHdr),
						 nPayload, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to pack a Measurement Report (0x%08x)",
			nStatus);
		cds_packet_free(mac->hdd_handle, TXRX_FRM_802_11_MGMT,
				(void *)pFrame, (void *)pPacket);
		return QDF_STATUS_E_FAILURE;    /* allocated! */
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while packing a Measurement Report (0x%08x)",
			nStatus);
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 ((pe_session) ? pe_session->
			  peSessionId : NO_SESSION), pMacHdr->fc.subType));
	qdf_status =
		wma_tx_frame(mac, pPacket, (uint16_t) nBytes,
			   TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS, 7,
			   lim_tx_complete, pFrame, 0, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace
		       (QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
		       ((pe_session) ? pe_session->peSessionId : NO_SESSION),
		       qdf_status));
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to send a Measurement Report (%X)!",
			qdf_status);
		/* Pkt will be freed up by the callback */
		return QDF_STATUS_E_FAILURE;    /* just allocated... */
	}

	return QDF_STATUS_SUCCESS;

} /* End lim_send_meas_report_frame. */

/**
 * \brief Send a TPC Report Action frame
 *
 *
 * \param mac Pointer to the global MAC datastructure
 *
 * \param pTpcReqFrame Pointer to the received TPC Request
 *
 * \return QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE else
 *
 *
 */

QDF_STATUS
lim_send_tpc_report_frame(struct mac_context *mac,
			  tpSirMacTpcReqActionFrame pTpcReqFrame,
			  tSirMacAddr peer, struct pe_session *pe_session)
{
	tDot11fTPCReport frm;
	uint8_t *pFrame;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t nBytes, nPayload, nStatus;
	void *pPacket;
	QDF_STATUS qdf_status;
	struct vdev_mlme_obj *mlme_obj;

	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));

	frm.Category.category = ACTION_CATEGORY_SPECTRUM_MGMT;
	frm.Action.action = ACTION_SPCT_TPC_RPRT;
	frm.DialogToken.token = pTpcReqFrame->actionHeader.dialogToken;

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(pe_session->vdev);
	if (mlme_obj)
		frm.TPCReport.tx_power = mlme_obj->mgmt.generic.tx_pwrlimit;

	frm.TPCReport.link_margin = 0;
	frm.TPCReport.present = 1;

	nStatus = dot11f_get_packed_tpc_report_size(mac, &frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to calculate the packed size for a TPC Report (0x%08x)", nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fTPCReport);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while calculating the packed size for a TPC Report (0x%08x)",
			nStatus);
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);

	qdf_status =
		cds_packet_alloc(mac->hdd_handle, TXRX_FRM_802_11_MGMT,
				 (uint16_t) nBytes, (void **)&pFrame,
				 (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a TPC"
			" Report", nBytes);
		return QDF_STATUS_E_FAILURE;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer);

	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	qdf_mem_copy(pMacHdr->bssId, pe_session->bssId, sizeof(tSirMacAddr));

	lim_set_protected_bit(mac, pe_session, peer, pMacHdr);

	nStatus = dot11f_pack_tpc_report(mac, &frm, pFrame +
					 sizeof(tSirMacMgmtHdr),
					 nPayload, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to pack a TPC Report (0x%08x)",
			nStatus);
		cds_packet_free(mac->hdd_handle, TXRX_FRM_802_11_MGMT,
				(void *)pFrame, (void *)pPacket);
		return QDF_STATUS_E_FAILURE;    /* allocated! */
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while packing a TPC Report (0x%08x)",
			nStatus);

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 ((pe_session) ? pe_session->
			  peSessionId : NO_SESSION), pMacHdr->fc.subType));
	qdf_status =
		wma_tx_frame(mac, pPacket, (uint16_t) nBytes,
			   TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS, 7,
			   lim_tx_complete, pFrame, 0, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace
		(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
		((pe_session) ? pe_session->peSessionId : NO_SESSION),
		qdf_status));
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to send a TPC Report (%X)!",
			qdf_status);
		/* Pkt will be freed up by the callback */
		return QDF_STATUS_E_FAILURE;    /* just allocated... */
	}

	return QDF_STATUS_SUCCESS;

} /* End lim_send_tpc_report_frame. */
#endif /* ANI_SUPPORT_11H */

/**
 * \brief Send a Channel Switch Announcement
 *
 *
 * \param mac Pointer to the global MAC datastructure
 *
 * \param peer MAC address to which this frame will be sent
 *
 * \param nMode
 *
 * \param nNewChannel
 *
 * \param nCount
 *
 * \return QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE else
 *
 *
 */

QDF_STATUS
lim_send_channel_switch_mgmt_frame(struct mac_context *mac,
				   tSirMacAddr peer,
				   uint8_t nMode,
				   uint8_t nNewChannel,
				   uint8_t nCount, struct pe_session *pe_session)
{
	tDot11fChannelSwitch frm;
	uint8_t *pFrame;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t nBytes, nPayload, nStatus;     /* , nCfg; */
	void *pPacket;
	QDF_STATUS qdf_status;
	uint8_t txFlag = 0;

	uint8_t smeSessionId = 0;

	if (!pe_session) {
		pe_err("Session entry is NULL!!!");
		return QDF_STATUS_E_FAILURE;
	}
	smeSessionId = pe_session->smeSessionId;

	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));

	frm.Category.category = ACTION_CATEGORY_SPECTRUM_MGMT;
	frm.Action.action = ACTION_SPCT_CHL_SWITCH;
	frm.ChanSwitchAnn.switchMode = nMode;
	frm.ChanSwitchAnn.newChannel = nNewChannel;
	frm.ChanSwitchAnn.switchCount = nCount;
	frm.ChanSwitchAnn.present = 1;

	nStatus = dot11f_get_packed_channel_switch_size(mac, &frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to calculate the packed size for a Channel Switch (0x%08x)",
			nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fChannelSwitch);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while calculating the packed size for a Channel Switch (0x%08x)",
			nStatus);
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);

	qdf_status =
		cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				 (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a TPC Report", nBytes);
		return QDF_STATUS_E_FAILURE;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Next, we fill out the buffer descriptor: */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer,
		pe_session->self_mac_addr);
	pMacHdr = (tpSirMacMgmtHdr) pFrame;
	qdf_mem_copy((uint8_t *) pMacHdr->bssId,
		     (uint8_t *) pe_session->bssId, sizeof(tSirMacAddr));

	lim_set_protected_bit(mac, pe_session, peer, pMacHdr);

	nStatus = dot11f_pack_channel_switch(mac, &frm, pFrame +
					     sizeof(tSirMacMgmtHdr),
					     nPayload, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to pack a Channel Switch (0x%08x)",
			nStatus);
		cds_packet_free((void *)pPacket);
		return QDF_STATUS_E_FAILURE;    /* allocated! */
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while packing a Channel Switch (0x%08x)",
			nStatus);
	}

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, pMacHdr->fc.subType));
	qdf_status = wma_tx_frame(mac, pPacket, (uint16_t) nBytes,
				TXRX_FRM_802_11_MGMT,
				ANI_TXDIR_TODS,
				7, lim_tx_complete, pFrame, txFlag,
				smeSessionId, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			 pe_session->peSessionId, qdf_status));
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to send a Channel Switch (%X)!",
			qdf_status);
		/* Pkt will be freed up by the callback */
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;

} /* End lim_send_channel_switch_mgmt_frame. */

/**
 * lim_send_extended_chan_switch_action_frame()- function to send ECSA
 * action frame over the air .
 * @mac_ctx: pointer to global mac structure
 * @peer: Destination mac.
 * @mode: channel switch mode
 * @new_op_class: new op class
 * @new_channel: new channel to switch
 * @count: channel switch count
 *
 * This function is called to send ECSA frame.
 *
 * Return: success if frame is sent else return failure
 */

QDF_STATUS
lim_send_extended_chan_switch_action_frame(struct mac_context *mac_ctx,
		tSirMacAddr peer, uint8_t mode, uint8_t new_op_class,
		uint8_t new_channel, uint8_t count, struct pe_session *session_entry)
{
	tDot11fext_channel_switch_action_frame frm;
	uint8_t                  *frame;
	tpSirMacMgmtHdr          mac_hdr;
	uint32_t                 num_bytes, n_payload, status;
	void                     *packet;
	QDF_STATUS               qdf_status;
	uint8_t                  txFlag = 0;
	uint8_t                  vdev_id = 0;
	uint8_t                  ch_spacing;
	tLimWiderBWChannelSwitchInfo *wide_bw_ie;

	if (!session_entry) {
		pe_err("Session entry is NULL!!!");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_id = session_entry->smeSessionId;

	qdf_mem_zero(&frm, sizeof(frm));

	frm.Category.category     = ACTION_CATEGORY_PUBLIC;
	frm.Action.action         = SIR_MAC_ACTION_EXT_CHANNEL_SWITCH_ID;

	frm.ext_chan_switch_ann_action.switch_mode = mode;
	frm.ext_chan_switch_ann_action.op_class = new_op_class;
	frm.ext_chan_switch_ann_action.new_channel = new_channel;
	frm.ext_chan_switch_ann_action.switch_count = count;

	ch_spacing = wlan_reg_dmn_get_chanwidth_from_opclass(
			mac_ctx->scan.countryCodeCurrent, new_channel,
			new_op_class);

	if ((ch_spacing == 80) || (ch_spacing == 160)) {
		wide_bw_ie = &session_entry->gLimWiderBWChannelSwitch;
		frm.WiderBWChanSwitchAnn.newChanWidth =
			wide_bw_ie->newChanWidth;
		frm.WiderBWChanSwitchAnn.newCenterChanFreq0 =
			wide_bw_ie->newCenterChanFreq0;
		frm.WiderBWChanSwitchAnn.newCenterChanFreq1 =
			wide_bw_ie->newCenterChanFreq1;
		frm.WiderBWChanSwitchAnn.present = 1;
		pe_debug("wrapper: width:%d f0:%d f1:%d",
			 frm.WiderBWChanSwitchAnn.newChanWidth,
			 frm.WiderBWChanSwitchAnn.newCenterChanFreq0,
			 frm.WiderBWChanSwitchAnn.newCenterChanFreq1);
	}

	status = dot11f_get_packed_ext_channel_switch_action_frame_size(mac_ctx,
							    &frm, &n_payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to get packed size for Channel Switch 0x%08x",
				 status);
		/* We'll fall back on the worst case scenario*/
		n_payload = sizeof(tDot11fext_channel_switch_action_frame);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while calculating the packed size for a Ext Channel Switch (0x%08x)",
		 status);
	}

	num_bytes = n_payload + sizeof(tSirMacMgmtHdr);

	qdf_status = cds_packet_alloc((uint16_t)num_bytes,
				(void **) &frame, (void **) &packet);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a Ext Channel Switch",
								 num_bytes);
		return QDF_STATUS_E_FAILURE;
	}

	/* Paranoia*/
	qdf_mem_zero(frame, num_bytes);

	/* Next, we fill out the buffer descriptor */
	lim_populate_mac_header(mac_ctx, frame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer, session_entry->self_mac_addr);
	mac_hdr = (tpSirMacMgmtHdr) frame;
	qdf_mem_copy((uint8_t *) mac_hdr->bssId,
				   (uint8_t *) session_entry->bssId,
				   sizeof(tSirMacAddr));

	lim_set_protected_bit(mac_ctx, session_entry, peer, mac_hdr);

	status = dot11f_pack_ext_channel_switch_action_frame(mac_ctx, &frm,
		frame + sizeof(tSirMacMgmtHdr), n_payload, &n_payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack a Channel Switch 0x%08x", status);
		cds_packet_free((void *)packet);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing a Channel Switch 0x%08x",
		 status);
	}

	if (!wlan_reg_is_24ghz_ch_freq(session_entry->curr_op_freq) ||
	    session_entry->opmode == QDF_P2P_CLIENT_MODE ||
	    session_entry->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	pe_debug("ECSA frame to :"QDF_MAC_ADDR_FMT" count %d mode %d chan %d op class %d",
		 QDF_MAC_ADDR_REF(mac_hdr->da),
		 frm.ext_chan_switch_ann_action.switch_count,
		 frm.ext_chan_switch_ann_action.switch_mode,
		 frm.ext_chan_switch_ann_action.new_channel,
		 frm.ext_chan_switch_ann_action.op_class);

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			session_entry->peSessionId, mac_hdr->fc.subType));
	qdf_status = wma_tx_frame(mac_ctx, packet, (uint16_t) num_bytes,
						 TXRX_FRM_802_11_MGMT,
						 ANI_TXDIR_TODS,
						 7,
						 lim_tx_complete, frame,
						 txFlag, vdev_id, 0,
						 RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			session_entry->peSessionId, qdf_status));
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to send a Ext Channel Switch %X!",
							 qdf_status);
		/* Pkt will be freed up by the callback */
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
} /* End lim_send_extended_chan_switch_action_frame */


/**
 * lim_oper_chan_change_confirm_tx_complete_cnf()- Confirmation for oper_chan_change_confirm
 * sent over the air
 *
 * @context: pointer to global mac
 * @buf: buffer
 * @tx_complete : Sent status
 * @params: tx completion params
 *
 * Return: This returns QDF_STATUS
 */

static QDF_STATUS lim_oper_chan_change_confirm_tx_complete_cnf(
			void *context,
			qdf_nbuf_t buf,
			uint32_t tx_complete,
			void *params)
{
	pe_debug("tx_complete: %d", tx_complete);
	if (buf)
		qdf_nbuf_free(buf);
	return QDF_STATUS_SUCCESS;
}

/**
 * lim_p2p_oper_chan_change_confirm_action_frame()- function to send
 * p2p oper chan change confirm action frame
 * @mac_ctx: pointer to global mac structure
 * @peer: Destination mac.
 * @session_entry: session entry
 *
 * This function is called to send p2p oper chan change confirm action frame.
 *
 * Return: success if frame is sent else return failure
 */

QDF_STATUS
lim_p2p_oper_chan_change_confirm_action_frame(struct mac_context *mac_ctx,
		tSirMacAddr peer, struct pe_session *session_entry)
{
	tDot11fp2p_oper_chan_change_confirm frm;
	uint8_t                  *frame;
	tpSirMacMgmtHdr          mac_hdr;
	uint32_t                 num_bytes, n_payload, status;
	void                     *packet;
	QDF_STATUS               qdf_status;
	uint8_t                  tx_flag = 0;
	uint8_t                  vdev_id = 0;

	if (!session_entry) {
		pe_err("Session entry is NULL!!!");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_id = session_entry->smeSessionId;

	qdf_mem_zero(&frm, sizeof(frm));

	frm.Category.category     = SIR_MAC_ACTION_VENDOR_SPECIFIC_CATEGORY;

	qdf_mem_copy(frm.p2p_action_oui.oui_data,
		SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE);
	frm.p2p_action_subtype.subtype = 0x04;
	frm.DialogToken.token = 0x0;

	if (session_entry->htCapability) {
		pe_debug("Populate HT Caps in Assoc Request");
		populate_dot11f_ht_caps(mac_ctx, session_entry, &frm.HTCaps);
	}

	if (session_entry->vhtCapability) {
		pe_debug("Populate VHT Caps in Assoc Request");
		populate_dot11f_vht_caps(mac_ctx, session_entry, &frm.VHTCaps);
		populate_dot11f_operating_mode(mac_ctx,
					&frm.OperatingMode, session_entry);
	}

	status = dot11f_get_packed_p2p_oper_chan_change_confirmSize(mac_ctx,
							    &frm, &n_payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to get packed size 0x%08x", status);
		/* We'll fall back on the worst case scenario*/
		n_payload = sizeof(tDot11fp2p_oper_chan_change_confirm);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while calculating the packed size (0x%08x)",
			status);
	}

	num_bytes = n_payload + sizeof(tSirMacMgmtHdr);

	qdf_status = cds_packet_alloc((uint16_t)num_bytes,
				(void **) &frame, (void **) &packet);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes", num_bytes);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_zero(frame, num_bytes);

	/* Next, fill out the buffer descriptor */
	lim_populate_mac_header(mac_ctx, frame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer, session_entry->self_mac_addr);
	mac_hdr = (tpSirMacMgmtHdr) frame;
	qdf_mem_copy((uint8_t *) mac_hdr->bssId,
				   (uint8_t *) session_entry->bssId,
				   sizeof(tSirMacAddr));

	status = dot11f_pack_p2p_oper_chan_change_confirm(mac_ctx, &frm,
		frame + sizeof(tSirMacMgmtHdr), n_payload, &n_payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack 0x%08x", status);
		cds_packet_free((void *)packet);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing 0x%08x",
		 status);
	}

	if (!wlan_reg_is_24ghz_ch_freq(session_entry->curr_op_freq) ||
	    session_entry->opmode == QDF_P2P_CLIENT_MODE ||
	    session_entry->opmode == QDF_P2P_GO_MODE) {
		tx_flag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
	}
	pe_debug("Send frame on channel freq %d to mac "
		QDF_MAC_ADDR_FMT, session_entry->curr_op_freq,
		QDF_MAC_ADDR_REF(peer));

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			session_entry->peSessionId, mac_hdr->fc.subType));

	qdf_status = wma_tx_frameWithTxComplete(mac_ctx, packet,
			(uint16_t)num_bytes,
			TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS,
			7, lim_tx_complete, frame,
			lim_oper_chan_change_confirm_tx_complete_cnf,
			tx_flag, vdev_id, false, 0, RATEID_DEFAULT, 0);

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			session_entry->peSessionId, qdf_status));
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to send status %X!", qdf_status);
		/* Pkt will be freed up by the callback */
		return QDF_STATUS_E_FAILURE;
	}
		return QDF_STATUS_SUCCESS;
}


/**
 * \brief Send a Neighbor Report Request Action frame
 *
 *
 * \param mac Pointer to the global MAC structure
 *
 * \param pNeighborReq Address of a tSirMacNeighborReportReq
 *
 * \param peer mac address of peer station.
 *
 * \param pe_session address of session entry.
 *
 * \return QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE else
 *
 *
 */

QDF_STATUS
lim_send_neighbor_report_request_frame(struct mac_context *mac,
				       tpSirMacNeighborReportReq pNeighborReq,
				       tSirMacAddr peer, struct pe_session *pe_session)
{
	QDF_STATUS status_code = QDF_STATUS_SUCCESS;
	tDot11fNeighborReportRequest frm;
	uint8_t *pFrame;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t nBytes, nPayload, nStatus;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint8_t txFlag = 0;
	uint8_t smeSessionId = 0;

	if (!pe_session) {
		pe_err("(!psession) in Request to send Neighbor Report request action frame");
		return QDF_STATUS_E_FAILURE;
	}
	smeSessionId = pe_session->smeSessionId;
	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));

	frm.Category.category = ACTION_CATEGORY_RRM;
	frm.Action.action = RRM_NEIGHBOR_REQ;
	frm.DialogToken.token = pNeighborReq->dialogToken;

	if (pNeighborReq->ssid_present) {
		populate_dot11f_ssid(mac, &pNeighborReq->ssid, &frm.SSID);
	}

	nStatus =
		dot11f_get_packed_neighbor_report_request_size(mac, &frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to calculate the packed size for a Neighbor Report Request(0x%08x)",
			nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fNeighborReportRequest);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while calculating the packed size for a Neighbor Report Request(0x%08x)",
			nStatus);
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);

	qdf_status =
		cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				 (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a Neighbor "
			   "Report Request", nBytes);
		return QDF_STATUS_E_FAILURE;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Copy necessary info to BD */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer, pe_session->self_mac_addr);

	/* Update A3 with the BSSID */
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);

	lim_set_protected_bit(mac, pe_session, peer, pMacHdr);

	/* Now, we're ready to "pack" the frames */
	nStatus = dot11f_pack_neighbor_report_request(mac,
						      &frm,
						      pFrame +
						      sizeof(tSirMacMgmtHdr),
						      nPayload, &nPayload);

	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to pack an Neighbor Report Request (0x%08x)",
			nStatus);

		/* FIXME - Need to convert to QDF_STATUS */
		status_code = QDF_STATUS_E_FAILURE;
		goto returnAfterError;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while packing Neighbor Report Request (0x%08x)",
			nStatus);
	}

	pe_debug("Sending a Neighbor Report Request to");
	lim_print_mac_addr(mac, peer, LOGD);

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, pMacHdr->fc.subType));
	qdf_status = wma_tx_frame(mac,
				pPacket,
				(uint16_t) nBytes,
				TXRX_FRM_802_11_MGMT,
				ANI_TXDIR_TODS,
				7, lim_tx_complete, pFrame, txFlag,
				smeSessionId, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			 pe_session->peSessionId, qdf_status));
	if (QDF_STATUS_SUCCESS != qdf_status) {
		pe_err("wma_tx_frame FAILED! Status [%d]", qdf_status);
		status_code = QDF_STATUS_E_FAILURE;
		/* Pkt will be freed up by the callback */
		return status_code;
	} else
		return QDF_STATUS_SUCCESS;

returnAfterError:
	cds_packet_free((void *)pPacket);

	return status_code;
} /* End lim_send_neighbor_report_request_frame. */

QDF_STATUS
lim_send_link_report_action_frame(struct mac_context *mac,
				  tpSirMacLinkReport pLinkReport,
				  tSirMacAddr peer, struct pe_session *pe_session)
{
	QDF_STATUS status_code = QDF_STATUS_SUCCESS;
	tDot11fLinkMeasurementReport frm;
	uint8_t *pFrame;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t nBytes, nPayload, nStatus;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint8_t txFlag = 0;
	uint8_t vdev_id = 0;

	if (!pe_session) {
		pe_err("RRM: Send link report: NULL PE session");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_id = pe_session->vdev_id;
	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));

	frm.Category.category = ACTION_CATEGORY_RRM;
	frm.Action.action = RRM_LINK_MEASUREMENT_RPT;
	frm.DialogToken.token = pLinkReport->dialogToken;

	/* IEEE Std. 802.11 7.3.2.18. for the report element. */
	/* Even though TPC report an IE, it is represented using fixed fields since it is positioned */
	/* in the middle of other fixed fields in the link report frame(IEEE Std. 802.11k section7.4.6.4 */
	/* and frame parser always expects IEs to come after all fixed fields. It is easier to handle */
	/* such case this way than changing the frame parser. */
	frm.TPCEleID.TPCId = WLAN_ELEMID_TPCREP;
	frm.TPCEleLen.TPCLen = 2;
	frm.TxPower.txPower = pLinkReport->txPower;
	frm.LinkMargin.linkMargin = 0;

	frm.RxAntennaId.antennaId = pLinkReport->rxAntenna;
	frm.TxAntennaId.antennaId = pLinkReport->txAntenna;
	frm.RCPI.rcpi = pLinkReport->rcpi;
	frm.RSNI.rsni = pLinkReport->rsni;

	nStatus =
		dot11f_get_packed_link_measurement_report_size(mac, &frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to calculate the packed size for a Link Report (0x%08x)", nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fLinkMeasurementReport);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while calculating the packed size for a Link Report (0x%08x)",
			nStatus);
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);

	qdf_status =
		cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				 (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a Link "
			"Report", nBytes);
		return QDF_STATUS_E_FAILURE;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Copy necessary info to BD */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer, pe_session->self_mac_addr);

	/* Update A3 with the BSSID */
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);

	lim_set_protected_bit(mac, pe_session, peer, pMacHdr);

	/* Now, we're ready to "pack" the frames */
	nStatus = dot11f_pack_link_measurement_report(mac,
						      &frm,
						      pFrame +
						      sizeof(tSirMacMgmtHdr),
						      nPayload, &nPayload);

	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to pack an Link Report (0x%08x)", nStatus);

		/* FIXME - Need to convert to QDF_STATUS */
		status_code = QDF_STATUS_E_FAILURE;
		goto returnAfterError;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while packing Link Report (0x%08x)",
			nStatus);
	}

	pe_warn("RRM: Sending Link Report to "QDF_MAC_ADDR_FMT" on vdev[%d]",
		QDF_MAC_ADDR_REF(peer), vdev_id);

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, pMacHdr->fc.subType));
	qdf_status = wma_tx_frame(mac,
				pPacket,
				(uint16_t) nBytes,
				TXRX_FRM_802_11_MGMT,
				ANI_TXDIR_TODS,
				7, lim_tx_complete, pFrame, txFlag,
				vdev_id, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			 pe_session->peSessionId, qdf_status));
	if (QDF_STATUS_SUCCESS != qdf_status) {
		pe_err("wma_tx_frame FAILED! Status [%d]", qdf_status);
		status_code = QDF_STATUS_E_FAILURE;
		/* Pkt will be freed up by the callback */
		return status_code;
	} else
		return QDF_STATUS_SUCCESS;

returnAfterError:
	cds_packet_free((void *)pPacket);

	return status_code;
} /* End lim_send_link_report_action_frame. */

QDF_STATUS
lim_send_radio_measure_report_action_frame(struct mac_context *mac,
				uint8_t dialog_token,
				uint8_t num_report,
				bool is_last_frame,
				tpSirMacRadioMeasureReport pRRMReport,
				tSirMacAddr peer,
				struct pe_session *pe_session)
{
	QDF_STATUS status_code = QDF_STATUS_SUCCESS;
	uint8_t *pFrame;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t nBytes, nPayload, nStatus;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint8_t i;
	uint8_t txFlag = 0;
	uint8_t smeSessionId = 0;
	bool is_last_report = false;

	/* Malloc size of (tDot11fIEMeasurementReport) * (num_report - 1)
	 * as memory for one Dot11fIEMeasurementReport is already calculated.
	 */
	tDot11fRadioMeasurementReport *frm =
		qdf_mem_malloc(sizeof(tDot11fRadioMeasurementReport) +
		(sizeof(tDot11fIEMeasurementReport) * (num_report - 1)));
	if (!frm)
		return QDF_STATUS_E_NOMEM;

	if (!pe_session) {
		pe_err("session not found");
		qdf_mem_free(frm);
		return QDF_STATUS_E_FAILURE;
	}

	smeSessionId = pe_session->smeSessionId;

	frm->Category.category = ACTION_CATEGORY_RRM;
	frm->Action.action = RRM_RADIO_MEASURE_RPT;
	frm->DialogToken.token = dialog_token;

	frm->num_MeasurementReport = num_report;

	for (i = 0; i < frm->num_MeasurementReport; i++) {
		frm->MeasurementReport[i].type = pRRMReport[i].type;
		frm->MeasurementReport[i].token = pRRMReport[i].token;
		frm->MeasurementReport[i].late = 0;     /* IEEE 802.11k section 7.3.22. (always zero in rrm) */
		switch (pRRMReport[i].type) {
		case SIR_MAC_RRM_BEACON_TYPE:
			/*
			 * Last beacon report indication needs to be set to 1
			 * only for the last report in the last frame
			 */
			if (is_last_frame &&
			    (i == (frm->num_MeasurementReport - 1)))
				is_last_report = true;

			populate_dot11f_beacon_report(mac,
						     &frm->MeasurementReport[i],
						     &pRRMReport[i].report.
						     beaconReport,
						     is_last_report);
			frm->MeasurementReport[i].incapable =
				pRRMReport[i].incapable;
			frm->MeasurementReport[i].refused =
				pRRMReport[i].refused;
			frm->MeasurementReport[i].present = 1;
			break;
		default:
			frm->MeasurementReport[i].incapable =
				pRRMReport[i].incapable;
			frm->MeasurementReport[i].refused =
				pRRMReport[i].refused;
			frm->MeasurementReport[i].present = 1;
			break;
		}
	}


	nStatus =
		dot11f_get_packed_radio_measurement_report_size(mac, frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_nofl_err("TX: [802.11 RRM] Failed to get packed size for RM Report (0x%08x)",
		       nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fLinkMeasurementReport);
		qdf_mem_free(frm);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("Warnings while calculating the size for Radio Measure Report (0x%08x)",
			nStatus);
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);

	qdf_status =
		cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				 (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_nofl_err("TX: [802.11 RRM] Allocation of %d bytes failed for RM"
			   "Report", nBytes);
		qdf_mem_free(frm);
		return QDF_STATUS_E_FAILURE;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Copy necessary info to BD */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer, pe_session->self_mac_addr);

	/* Update A3 with the BSSID */
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);

	lim_set_protected_bit(mac, pe_session, peer, pMacHdr);

	/* Now, we're ready to "pack" the frames */
	nStatus = dot11f_pack_radio_measurement_report(mac,
						       frm,
						       pFrame +
						       sizeof(tSirMacMgmtHdr),
						       nPayload, &nPayload);

	if (DOT11F_FAILED(nStatus)) {
		pe_nofl_err("Failed to pack an Radio Measure Report (0x%08x)",
			nStatus);

		/* FIXME - Need to convert to QDF_STATUS */
		status_code = QDF_STATUS_E_FAILURE;
		goto returnAfterError;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("Warnings while packing Radio Measure Report (0x%08x)",
			nStatus);
	}

	pe_nofl_info("TX: %s seq_no:%d dialog_token:%d no. of APs:%d is_last_rpt:%d num_report: %d peer:"QDF_MAC_ADDR_FMT,
		     frm->MeasurementReport[0].type == SIR_MAC_RRM_BEACON_TYPE ?
		     "[802.11 BCN_RPT]" : "[802.11 RRM]",
		     (pMacHdr->seqControl.seqNumHi << HIGH_SEQ_NUM_OFFSET |
		     pMacHdr->seqControl.seqNumLo),
		     dialog_token, frm->num_MeasurementReport,
		     is_last_report, num_report,
		     QDF_MAC_ADDR_REF(peer));

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, pMacHdr->fc.subType));
	qdf_status = wma_tx_frame(mac, pPacket, (uint16_t)nBytes,
				  TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS,
				  7, lim_tx_complete, pFrame, txFlag,
				  smeSessionId, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			 pe_session->peSessionId, qdf_status));
	if (QDF_STATUS_SUCCESS != qdf_status) {
		pe_nofl_err("TX: [802.11 RRM] Send FAILED! err_status [%d]",
		       qdf_status);
		status_code = QDF_STATUS_E_FAILURE;
		/* Pkt will be freed up by the callback */
	}

	qdf_mem_free(frm);
	return status_code;

returnAfterError:
	qdf_mem_free(frm);
	cds_packet_free((void *)pPacket);
	return status_code;
}

static bool
lim_is_self_and_peer_ocv_capable(struct mac_context *mac,
				 uint8_t *peer,
				 struct pe_session *pe_session)
{
	uint16_t self_rsn_cap, aid;
	tpDphHashNode sta_ds;

	self_rsn_cap = wlan_crypto_get_param(pe_session->vdev,
					     WLAN_CRYPTO_PARAM_RSN_CAP);
	if (!(self_rsn_cap & WLAN_CRYPTO_RSN_CAP_OCV_SUPPORTED))
		return false;

	sta_ds = dph_lookup_hash_entry(mac, peer, &aid,
				       &pe_session->dph.dphHashTable);

	if (sta_ds && sta_ds->ocv_enabled)
		return true;

	return false;
}

static void
lim_fill_oci_params(struct mac_context *mac, struct pe_session *session,
		    tDot11fIEoci *oci)
{
	uint8_t ch_offset;
	uint8_t prim_ch_num = wlan_reg_freq_to_chan(mac->pdev,
						    session->curr_op_freq);
	if (session->ch_width == CH_WIDTH_80MHZ) {
		ch_offset = BW80;
	} else {
		switch (session->htSecondaryChannelOffset) {
		case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
			ch_offset = BW40_HIGH_PRIMARY;
			break;
		case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
			ch_offset = BW40_LOW_PRIMARY;
			break;
		default:
			ch_offset = BW20;
			break;
		}
	}
	oci->op_class = lim_op_class_from_bandwidth(mac,
						    session->curr_op_freq,
						    session->ch_width,
						    ch_offset);
	oci->prim_ch_num = prim_ch_num;
	oci->freq_seg_1_ch_num = session->ch_center_freq_seg1;
	oci->present = 1;
}

#ifdef WLAN_FEATURE_11W
/**
 * \brief Send SA query request action frame to peer
 *
 * \sa lim_send_sa_query_request_frame
 *
 *
 * \param mac    The global struct mac_context *object
 *
 * \param transId Transaction identifier
 *
 * \param peer    The Mac address of the station to which this action frame is addressed
 *
 * \param pe_session The PE session entry
 *
 * \return QDF_STATUS_SUCCESS if setup completes successfully
 *         QDF_STATUS_E_FAILURE is some problem is encountered
 */

QDF_STATUS lim_send_sa_query_request_frame(struct mac_context *mac, uint8_t *transId,
					      tSirMacAddr peer,
					      struct pe_session *pe_session)
{

	tDot11fSaQueryReq frm;  /* SA query request action frame */
	uint8_t *pFrame;
	QDF_STATUS nSirStatus;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t nBytes, nPayload, nStatus;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint8_t txFlag = 0;
	uint8_t smeSessionId = 0;

	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));
	frm.Category.category = ACTION_CATEGORY_SA_QUERY;
	/* 11w action  field is :
	   action: 0 --> SA Query Request action frame
	   action: 1 --> SA Query Response action frame */
	frm.Action.action = SA_QUERY_REQUEST;
	/* 11w SA Query Request transId */
	qdf_mem_copy(&frm.TransactionId.transId[0], &transId[0], 2);

	if (lim_is_self_and_peer_ocv_capable(mac, peer, pe_session))
		lim_fill_oci_params(mac, pe_session, &frm.oci);

	nStatus = dot11f_get_packed_sa_query_req_size(mac, &frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to calculate the packed size for an SA Query Request (0x%08x)",
			nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fSaQueryReq);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while calculating the packed size for an SA Query Request (0x%08x)",
			nStatus);
	}

	nBytes = nPayload + sizeof(tSirMacMgmtHdr);
	qdf_status =
		cds_packet_alloc(nBytes, (void **)&pFrame, (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a SA Query Request "
			   "action frame", nBytes);
		return QDF_STATUS_E_FAILURE;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Copy necessary info to BD */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer, pe_session->self_mac_addr);

	/* Update A3 with the BSSID */
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);

	/* Since this is a SA Query Request, set the "protect" (aka WEP) bit */
	/* in the FC */
	lim_set_protected_bit(mac, pe_session, peer, pMacHdr);

	/* Pack 11w SA Query Request frame */
	nStatus = dot11f_pack_sa_query_req(mac,
					   &frm,
					   pFrame + sizeof(tSirMacMgmtHdr),
					   nPayload, &nPayload);

	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to pack an SA Query Request (0x%08x)",
			nStatus);
		/* FIXME - Need to convert to QDF_STATUS */
		nSirStatus = QDF_STATUS_E_FAILURE;
		goto returnAfterError;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while packing SA Query Request (0x%08x)",
			nStatus);
	}

	pe_debug("Sending an SA Query Request to");
	lim_print_mac_addr(mac, peer, LOGD);
	pe_debug("Sending an SA Query Request from ");
	lim_print_mac_addr(mac, pe_session->self_mac_addr, LOGD);

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	smeSessionId = pe_session->smeSessionId;

	qdf_status = wma_tx_frame(mac,
				pPacket,
				(uint16_t) nBytes,
				TXRX_FRM_802_11_MGMT,
				ANI_TXDIR_TODS,
				7, lim_tx_complete, pFrame, txFlag,
				smeSessionId, 0, RATEID_DEFAULT, 0);
	if (QDF_STATUS_SUCCESS != qdf_status) {
		pe_err("wma_tx_frame FAILED! Status [%d]", qdf_status);
		nSirStatus = QDF_STATUS_E_FAILURE;
		/* Pkt will be freed up by the callback */
		return nSirStatus;
	} else {
		return QDF_STATUS_SUCCESS;
	}

returnAfterError:
	cds_packet_free((void *)pPacket);
	return nSirStatus;
} /* End lim_send_sa_query_request_frame */

/**
 * \brief Send SA query response action frame to peer
 *
 * \sa lim_send_sa_query_response_frame
 *
 *
 * \param mac    The global struct mac_context *object
 *
 * \param transId Transaction identifier received in SA query request action frame
 *
 * \param peer    The Mac address of the AP to which this action frame is addressed
 *
 * \param pe_session The PE session entry
 *
 * \return QDF_STATUS_SUCCESS if setup completes successfully
 *         QDF_STATUS_E_FAILURE is some problem is encountered
 */

QDF_STATUS lim_send_sa_query_response_frame(struct mac_context *mac,
					       uint8_t *transId, tSirMacAddr peer,
					       struct pe_session *pe_session)
{

	tDot11fSaQueryRsp frm;  /* SA query response action frame */
	uint8_t *pFrame;
	QDF_STATUS nSirStatus;
	tpSirMacMgmtHdr pMacHdr;
	uint32_t nBytes, nPayload, nStatus;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint8_t txFlag = 0;
	uint8_t smeSessionId = 0;

	smeSessionId = pe_session->smeSessionId;

	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));
	frm.Category.category = ACTION_CATEGORY_SA_QUERY;
	/*11w action  field is :
	   action: 0 --> SA query request action frame
	   action: 1 --> SA query response action frame */
	frm.Action.action = SA_QUERY_RESPONSE;
	/*11w SA query response transId is same as
	   SA query request transId */
	qdf_mem_copy(&frm.TransactionId.transId[0], &transId[0], 2);
	if (lim_is_self_and_peer_ocv_capable(mac, peer, pe_session))
		lim_fill_oci_params(mac, pe_session, &frm.oci);

	nStatus = dot11f_get_packed_sa_query_rsp_size(mac, &frm, &nPayload);
	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to calculate the packed size for a SA Query Response (0x%08x)",
			nStatus);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fSaQueryRsp);
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while calculating the packed size for an SA Query Response (0x%08x)",
			nStatus);
	}
	nBytes = nPayload + sizeof(tSirMacMgmtHdr);
	qdf_status =
		cds_packet_alloc(nBytes, (void **)&pFrame, (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a SA query response"
			   " action frame", nBytes);
		return QDF_STATUS_E_FAILURE;
	}
	/* Paranoia: */
	qdf_mem_zero(pFrame, nBytes);

	/* Copy necessary info to BD */
	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer, pe_session->self_mac_addr);

	/* Update A3 with the BSSID */
	pMacHdr = (tpSirMacMgmtHdr) pFrame;

	sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);

	/* Since this is a SA Query Response, set the "protect" (aka WEP) bit */
	/* in the FC */
	lim_set_protected_bit(mac, pe_session, peer, pMacHdr);

	/* Pack 11w SA query response frame */
	nStatus = dot11f_pack_sa_query_rsp(mac,
					   &frm,
					   pFrame + sizeof(tSirMacMgmtHdr),
					   nPayload, &nPayload);

	if (DOT11F_FAILED(nStatus)) {
		pe_err("Failed to pack an SA Query Response (0x%08x)",
			nStatus);
		/* FIXME - Need to convert to QDF_STATUS */
		nSirStatus = QDF_STATUS_E_FAILURE;
		goto returnAfterError;
	} else if (DOT11F_WARNED(nStatus)) {
		pe_warn("There were warnings while packing SA Query Response (0x%08x)",
			nStatus);
	}

	pe_debug("Sending SA Query Response to "QDF_MAC_ADDR_FMT" op_class %d prim_ch_num %d freq_seg_1_ch_num %d oci_present %d",
		 QDF_MAC_ADDR_REF(peer), frm.oci.op_class,
		 frm.oci.prim_ch_num, frm.oci.freq_seg_1_ch_num,
		 frm.oci.present);

	if (!wlan_reg_is_24ghz_ch_freq(pe_session->curr_op_freq) ||
	    pe_session->opmode == QDF_P2P_CLIENT_MODE ||
	    pe_session->opmode == QDF_P2P_GO_MODE)
		txFlag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 pe_session->peSessionId, pMacHdr->fc.subType));
	qdf_status = wma_tx_frame(mac,
				pPacket,
				(uint16_t) nBytes,
				TXRX_FRM_802_11_MGMT,
				ANI_TXDIR_TODS,
				7, lim_tx_complete, pFrame, txFlag,
				smeSessionId, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			 pe_session->peSessionId, qdf_status));
	if (QDF_STATUS_SUCCESS != qdf_status) {
		pe_err("wma_tx_frame FAILED! Status [%d]", qdf_status);
		nSirStatus = QDF_STATUS_E_FAILURE;
		/* Pkt will be freed up by the callback */
		return nSirStatus;
	} else {
		return QDF_STATUS_SUCCESS;
	}

returnAfterError:
	cds_packet_free((void *)pPacket);
	return nSirStatus;
} /* End lim_send_sa_query_response_frame */
#endif

#if defined(QCA_WIFI_QCA6290) || defined(QCA_WIFI_QCA6390) || \
    defined(QCA_WIFI_QCA6490) || defined(QCA_WIFI_QCA6750)
#ifdef WLAN_FEATURE_11AX
#define IS_PE_SESSION_HE_MODE(_session) ((_session)->he_capable)
#else
#define IS_PE_SESSION_HE_MODE(_session) false
#endif /* WLAN_FEATURE_11AX */
#else
#define IS_PE_SESSION_HE_MODE(_session) false
#endif

QDF_STATUS lim_send_addba_response_frame(struct mac_context *mac_ctx,
		tSirMacAddr peer_mac, uint16_t tid,
		struct pe_session *session, uint8_t addba_extn_present,
		uint8_t amsdu_support, uint8_t is_wep, uint16_t calc_buff_size)
{

	tDot11faddba_rsp frm;
	uint8_t *frame_ptr;
	tpSirMacMgmtHdr mgmt_hdr;
	uint32_t num_bytes, payload_size, status;
	void *pkt_ptr = NULL;
	QDF_STATUS qdf_status;
	uint8_t tx_flag = 0;
	uint8_t vdev_id = 0;
	uint16_t buff_size, status_code, batimeout;
	uint8_t dialog_token;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint8_t he_frag = 0;
	tpDphHashNode sta_ds = NULL;
	uint16_t aid;
	bool he_cap = false;
	struct wlan_mlme_qos *qos_aggr;

	vdev_id = session->vdev_id;

	cdp_addba_responsesetup(soc, peer_mac, vdev_id, tid,
				&dialog_token, &status_code, &buff_size,
				&batimeout);

	qos_aggr = &mac_ctx->mlme_cfg->qos_mlme_params;
	qdf_mem_zero((uint8_t *) &frm, sizeof(frm));
	frm.Category.category = ACTION_CATEGORY_BACK;
	frm.Action.action = ADDBA_RESPONSE;

	frm.DialogToken.token = dialog_token;
	frm.Status.status = status_code;

	if ((LIM_IS_STA_ROLE(session) && qos_aggr->rx_aggregation_size == 1 &&
	    !mac_ctx->usr_cfg_ba_buff_size) || mac_ctx->reject_addba_req) {
		frm.Status.status = STATUS_REQUEST_DECLINED;
		pe_err("refused addba req for rx_aggregation_size: %d mac_ctx->reject_addba_req: %d",
		       qos_aggr->rx_aggregation_size, mac_ctx->reject_addba_req);
	} else if (LIM_IS_STA_ROLE(session) && !mac_ctx->usr_cfg_ba_buff_size) {
		frm.addba_param_set.buff_size =
			QDF_MIN(qos_aggr->rx_aggregation_size, calc_buff_size);
	} else {
		sta_ds = dph_lookup_hash_entry(mac_ctx, peer_mac, &aid,
					       &session->dph.dphHashTable);
		if (sta_ds && lim_is_session_he_capable(session))
			he_cap = lim_is_sta_he_capable(sta_ds);

		if (sta_ds && sta_ds->staType == STA_ENTRY_NDI_PEER)
			frm.addba_param_set.buff_size = calc_buff_size;
		else if (he_cap)
			frm.addba_param_set.buff_size = MAX_BA_BUFF_SIZE;
		else
			frm.addba_param_set.buff_size =
					SIR_MAC_BA_DEFAULT_BUFF_SIZE;

		if (mac_ctx->usr_cfg_ba_buff_size)
			frm.addba_param_set.buff_size =
					mac_ctx->usr_cfg_ba_buff_size;

		if (frm.addba_param_set.buff_size > MAX_BA_BUFF_SIZE)
			frm.addba_param_set.buff_size = MAX_BA_BUFF_SIZE;

		if (frm.addba_param_set.buff_size > SIR_MAC_BA_DEFAULT_BUFF_SIZE) {
			if (session->active_ba_64_session) {
				frm.addba_param_set.buff_size =
					SIR_MAC_BA_DEFAULT_BUFF_SIZE;
			}
		} else if (!session->active_ba_64_session) {
			session->active_ba_64_session = true;
		}
		if (buff_size && (frm.addba_param_set.buff_size > buff_size)) {
			pe_debug("buff size: %d larger than peer's capability: %d",
				 frm.addba_param_set.buff_size, buff_size);
			frm.addba_param_set.buff_size = buff_size;
		}
	}

	frm.addba_param_set.tid = tid;
	/* Enable RX AMSDU only in HE mode if supported */
	if (mac_ctx->is_usr_cfg_amsdu_enabled &&
	    ((IS_PE_SESSION_HE_MODE(session) &&
	      WLAN_REG_IS_24GHZ_CH_FREQ(session->curr_op_freq)) ||
	     !WLAN_REG_IS_24GHZ_CH_FREQ(session->curr_op_freq)))
		frm.addba_param_set.amsdu_supp = amsdu_support;
	else
		frm.addba_param_set.amsdu_supp = 0;

	frm.addba_param_set.policy = SIR_MAC_BA_POLICY_IMMEDIATE;
	frm.ba_timeout.timeout = batimeout;
	if (addba_extn_present) {
		frm.addba_extn_element.present = 1;
		frm.addba_extn_element.no_fragmentation = 1;
		if (lim_is_session_he_capable(session)) {
			he_frag = lim_get_session_he_frag_cap(session);
			if (he_frag != 0) {
				frm.addba_extn_element.no_fragmentation = 0;
				frm.addba_extn_element.he_frag_operation =
					he_frag;
			}
		}
	}

	pe_debug("Sending a ADDBA Response from "QDF_MAC_ADDR_FMT" to "QDF_MAC_ADDR_FMT,
		 QDF_MAC_ADDR_REF(session->self_mac_addr),
		 QDF_MAC_ADDR_REF(peer_mac));
	pe_debug("tid %d dialog_token %d status %d buff_size %d amsdu_supp %d",
		 tid, frm.DialogToken.token, frm.Status.status,
		 frm.addba_param_set.buff_size,
		 frm.addba_param_set.amsdu_supp);
	pe_debug("addba_extn %d he_capable %d no_frag %d he_frag %d",
		addba_extn_present,
		lim_is_session_he_capable(session),
		frm.addba_extn_element.no_fragmentation,
		frm.addba_extn_element.he_frag_operation);

	status = dot11f_get_packed_addba_rsp_size(mac_ctx, &frm, &payload_size);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to calculate the packed size for a ADDBA Response (0x%08x).",
			status);
		/* We'll fall back on the worst case scenario: */
		payload_size = sizeof(tDot11faddba_rsp);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while calculating the packed size for a ADDBA Response (0x%08x).", status);
	}

	num_bytes = payload_size + sizeof(*mgmt_hdr);
	qdf_status = cds_packet_alloc(num_bytes, (void **)&frame_ptr,
				      (void **)&pkt_ptr);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status) || (!pkt_ptr)) {
		pe_err("Failed to allocate %d bytes for a ADDBA response action frame",
			num_bytes);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_zero(frame_ptr, num_bytes);

	lim_populate_mac_header(mac_ctx, frame_ptr, SIR_MAC_MGMT_FRAME,
		SIR_MAC_MGMT_ACTION, peer_mac, session->self_mac_addr);

	/* Update A3 with the BSSID */
	mgmt_hdr = (tpSirMacMgmtHdr) frame_ptr;
	sir_copy_mac_addr(mgmt_hdr->bssId, session->bssId);

	/* ADDBA Response is a robust mgmt action frame,
	 * set the "protect" (aka WEP) bit in the FC
	 */
	lim_set_protected_bit(mac_ctx, session, peer_mac, mgmt_hdr);

	if (is_wep && sta_ds && sta_ds->staType == STA_ENTRY_NDI_PEER) {
		mgmt_hdr->fc.wep = 1;
		tx_flag |= HAL_USE_PMF;
	}

	status = dot11f_pack_addba_rsp(mac_ctx, &frm,
			frame_ptr + sizeof(tSirMacMgmtHdr), payload_size,
			&payload_size);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack a ADDBA Response (0x%08x)",
			status);
		qdf_status = QDF_STATUS_E_FAILURE;
		goto error_addba_rsp;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing ADDBA Response (0x%08x)",
			status);
	}


	if (!wlan_reg_is_24ghz_ch_freq(session->curr_op_freq) ||
	    session->opmode == QDF_P2P_CLIENT_MODE ||
	    session->opmode == QDF_P2P_GO_MODE)
		tx_flag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;

	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 session->peSessionId, mgmt_hdr->fc.subType));
	qdf_status = wma_tx_frameWithTxComplete(mac_ctx, pkt_ptr,
						(uint16_t)num_bytes,
						TXRX_FRM_802_11_MGMT,
						ANI_TXDIR_TODS, 7,
						NULL, frame_ptr,
						lim_addba_rsp_tx_complete_cnf,
						tx_flag, vdev_id,
						false, 0, RATEID_DEFAULT, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
			 session->peSessionId, qdf_status));
	if (QDF_STATUS_SUCCESS != qdf_status) {
		pe_err("wma_tx_frame FAILED! Status [%d]",
			qdf_status);
		return QDF_STATUS_E_FAILURE;
	} else {
		return QDF_STATUS_SUCCESS;
	}

error_addba_rsp:
	cds_packet_free((void *)pkt_ptr);
	return qdf_status;
}

/**
 * lim_delba_tx_complete_cnf() - Confirmation for Delba OTA
 * @context: pointer to global mac
 * @buf: netbuf of Del BA frame
 * @tx_complete : Sent status
 * @params; tx completion params
 *
 * Return: This returns QDF_STATUS
 */
static QDF_STATUS lim_delba_tx_complete_cnf(void *context,
					    qdf_nbuf_t buf,
					    uint32_t tx_complete,
					    void *params)
{
	struct mac_context *mac_ctx = (struct mac_context *)context;
	tSirMacMgmtHdr *mac_hdr;
	struct sDot11fdelba_req frm;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint32_t frame_len;
	QDF_STATUS status;
	uint8_t *data;
	struct wmi_mgmt_params *mgmt_params = (struct wmi_mgmt_params *)params;

	if (!mgmt_params || !mac_ctx || !buf || !soc) {
		pe_err("delba tx cnf invalid parameters");
		goto error;
	}
	data = qdf_nbuf_data(buf);
	if (!data) {
		pe_err("Delba frame is NULL");
		goto error;
	}

	mac_hdr = (tSirMacMgmtHdr *)data;
	qdf_mem_zero((void *)&frm, sizeof(struct sDot11fdelba_req));
	frame_len = sizeof(frm);
	status = dot11f_unpack_delba_req(mac_ctx, (uint8_t *)data +
					 sizeof(*mac_hdr), frame_len,
					 &frm, false);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to unpack and parse delba (0x%08x, %d bytes)",
		       status, frame_len);
		goto error;
	}
	pe_debug("delba ota done vdev %d "QDF_MAC_ADDR_FMT" tid %d desc_id %d status %d",
		 mgmt_params->vdev_id,
		 QDF_MAC_ADDR_REF(mac_hdr->da), frm.delba_param_set.tid,
		 mgmt_params->desc_id, tx_complete);
	cdp_delba_tx_completion(soc, mac_hdr->da, mgmt_params->vdev_id,
				frm.delba_param_set.tid, tx_complete);

error:
	if (buf)
		qdf_nbuf_free(buf);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_send_delba_action_frame(struct mac_context *mac_ctx,
				       uint8_t vdev_id, uint8_t *peer_macaddr,
				       uint8_t tid, uint8_t reason_code)
{
	struct pe_session *session;
	struct sDot11fdelba_req frm;
	QDF_STATUS qdf_status;
	tpSirMacMgmtHdr mgmt_hdr;
	uint32_t num_bytes, payload_size = 0;
	uint32_t status;
	void *pkt_ptr = NULL;
	uint8_t *frame_ptr;
	uint8_t tx_flag = 0;

	session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session) {
		pe_debug("delba invalid vdev id %d ", vdev_id);
		return QDF_STATUS_E_INVAL;
	}
	pe_debug("send delba vdev %d "QDF_MAC_ADDR_FMT" tid %d reason %d", vdev_id,
		 QDF_MAC_ADDR_REF(peer_macaddr), tid, reason_code);
	qdf_mem_zero((uint8_t *)&frm, sizeof(frm));
	frm.Category.category = ACTION_CATEGORY_BACK;
	frm.Action.action = DELBA;
	frm.delba_param_set.initiator = 0;
	frm.delba_param_set.tid = tid;
	frm.Reason.code = reason_code;

	status = dot11f_get_packed_delba_req_size(mac_ctx, &frm, &payload_size);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to calculate the packed size for a DELBA(0x%08x).",
		       status);
		/* We'll fall back on the worst case scenario: */
		payload_size = sizeof(struct sDot11fdelba_req);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("Warnings in calculating the packed size for a DELBA (0x%08x).",
			status);
	}
	num_bytes = payload_size + sizeof(*mgmt_hdr);
	qdf_status = cds_packet_alloc(num_bytes, (void **)&frame_ptr,
				      (void **)&pkt_ptr);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status) || !pkt_ptr) {
		pe_err("Failed to allocate %d bytes for a DELBA",
		       num_bytes);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_zero(frame_ptr, num_bytes);

	lim_populate_mac_header(mac_ctx, frame_ptr, SIR_MAC_MGMT_FRAME,
				SIR_MAC_MGMT_ACTION, peer_macaddr,
				session->self_mac_addr);

	/* Update A3 with the BSSID */
	mgmt_hdr = (tpSirMacMgmtHdr)frame_ptr;
	sir_copy_mac_addr(mgmt_hdr->bssId, session->bssId);

	/* DEL is a robust mgmt action frame,
	 * set the "protect" (aka WEP) bit in the FC
	 */
	lim_set_protected_bit(mac_ctx, session, peer_macaddr, mgmt_hdr);

	status = dot11f_pack_delba_req(mac_ctx, &frm,
				       frame_ptr + sizeof(tSirMacMgmtHdr),
				       payload_size, &payload_size);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack a DELBA (0x%08x)",
		       status);
		qdf_status = QDF_STATUS_E_FAILURE;
		goto error_delba;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing DELBA (0x%08x)",
			status);
	}
	if (wlan_reg_is_5ghz_ch_freq(session->curr_op_freq) ||
	    wlan_reg_is_6ghz_chan_freq(session->curr_op_freq) ||
	    session->opmode == QDF_P2P_CLIENT_MODE ||
	    session->opmode == QDF_P2P_GO_MODE)
		tx_flag |= HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME;
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_MGMT,
			 session->peSessionId, mgmt_hdr->fc.subType));
	qdf_status = wma_tx_frameWithTxComplete(mac_ctx, pkt_ptr,
						(uint16_t)num_bytes,
						TXRX_FRM_802_11_MGMT,
						ANI_TXDIR_TODS, 7,
						NULL, frame_ptr,
						lim_delba_tx_complete_cnf,
						tx_flag, vdev_id,
						false, 0, RATEID_DEFAULT, 0);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		pe_err("delba wma_tx_frame FAILED! Status [%d]", qdf_status);
		return qdf_status;
	} else {
		return QDF_STATUS_SUCCESS;
	}

error_delba:
	if (pkt_ptr)
		cds_packet_free((void *)pkt_ptr);

	return qdf_status;
}

/**
 * lim_tx_mgmt_frame() - Transmits Auth mgmt frame
 * @mac_ctx Pointer to Global MAC structure
 * @vdev_id: vdev id
 * @msg_len: Received message length
 * @packet: Packet to be transmitted
 * @frame: Received frame
 *
 * Return: None
 */
static void lim_tx_mgmt_frame(struct mac_context *mac_ctx, uint8_t vdev_id,
			      uint32_t msg_len, void *packet, uint8_t *frame)
{
	tpSirMacFrameCtl fc = (tpSirMacFrameCtl)frame;
	QDF_STATUS qdf_status;
	struct pe_session *session;
	uint16_t auth_ack_status;
	enum rateid min_rid = RATEID_DEFAULT;

	session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session) {
		cds_packet_free((void *)packet);
		pe_err("session not found for given vdev_id %d",
		       vdev_id);
		return;
	}

	qdf_mtrace(QDF_MODULE_ID_PE, QDF_MODULE_ID_WMA, TRACE_CODE_TX_MGMT,
		   session->peSessionId, 0);

	mac_ctx->auth_ack_status = LIM_ACK_NOT_RCD;
	min_rid = lim_get_min_session_txrate(session);

	qdf_status = wma_tx_frameWithTxComplete(mac_ctx, packet,
					 (uint16_t)msg_len,
					 TXRX_FRM_802_11_MGMT, ANI_TXDIR_TODS,
					 7, lim_tx_complete, frame,
					 lim_auth_tx_complete_cnf,
					 0, vdev_id, false, 0, min_rid, 0);
	MTRACE(qdf_trace(QDF_MODULE_ID_PE, TRACE_CODE_TX_COMPLETE,
		session->peSessionId, qdf_status));
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("*** Could not send Auth frame (subType: %d), retCode=%X ***",
			fc->subType, qdf_status);
		mac_ctx->auth_ack_status = LIM_TX_FAILED;
		auth_ack_status = SENT_FAIL;
		lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_AUTH_ACK_EVENT,
				session, auth_ack_status, QDF_STATUS_E_FAILURE);
		/* Pkt will be freed up by the callback */
	}
}

static void
lim_handle_sae_auth_retry(struct mac_context *mac_ctx, uint8_t vdev_id,
			  uint8_t *frame, uint32_t frame_len)
{
	struct pe_session *session;
	struct sae_auth_retry *sae_retry;
	uint8_t retry_count = 0;
	uint32_t val = 0;

	session = pe_find_session_by_vdev_id(mac_ctx, vdev_id);
	if (!session) {
		pe_err("session not found for given vdev_id %d",
		       vdev_id);
		return;
	}
	if ((session->opmode != QDF_STA_MODE) &&
	    (session->opmode != QDF_P2P_CLIENT_MODE))
		return;

	if (session->limMlmState == eLIM_MLM_WT_SAE_AUTH_STATE)
		wlan_mlme_get_sae_auth_retry_count(mac_ctx->psoc, &retry_count);
	else
		wlan_mlme_get_sae_roam_auth_retry_count(mac_ctx->psoc,
							&retry_count);
	if (!retry_count) {
		pe_debug("vdev %d: SAE Auth retry disabled", vdev_id);
		return;
	}

	sae_retry = mlme_get_sae_auth_retry(session->vdev);
	if (!sae_retry) {
		pe_err("sae retry pointer is NULL for vdev_id %d",
		       vdev_id);
		return;
	}

	if (sae_retry->sae_auth.data)
		lim_sae_auth_cleanup_retry(mac_ctx, vdev_id);

	sae_retry->sae_auth.data = qdf_mem_malloc(frame_len);
	if (!sae_retry->sae_auth.data)
		return;

	pe_debug("SAE auth frame queued vdev_id %d seq_num %d",
		 vdev_id, mac_ctx->mgmtSeqNum + 1);
	qdf_mem_copy(sae_retry->sae_auth.data, frame, frame_len);
	mac_ctx->lim.lim_timers.g_lim_periodic_auth_retry_timer.sessionId =
					session->peSessionId;
	sae_retry->sae_auth.len = frame_len;
	sae_retry->sae_auth_max_retry = retry_count;

	val = mac_ctx->mlme_cfg->timeouts.sae_auth_failure_timeout;

	tx_timer_change(
		&mac_ctx->lim.lim_timers.g_lim_periodic_auth_retry_timer,
		SYS_MS_TO_TICKS(val), 0);
	/* Activate Auth Retry timer */
	if (tx_timer_activate(
	    &mac_ctx->lim.lim_timers.g_lim_periodic_auth_retry_timer) !=
	    TX_SUCCESS) {
		pe_err("failed to start periodic auth retry timer");
		lim_sae_auth_cleanup_retry(mac_ctx, vdev_id);
	}
}

void lim_send_frame(struct mac_context *mac_ctx, uint8_t vdev_id, uint8_t *buf,
		    uint16_t buf_len)
{
	QDF_STATUS qdf_status;
	uint8_t *frame;
	void *packet;
	tpSirMacFrameCtl fc = (tpSirMacFrameCtl)buf;
	tpSirMacMgmtHdr mac_hdr = (tpSirMacMgmtHdr)buf;

	pe_debug("sending fc->type: %d fc->subType: %d",
		 fc->type, fc->subType);

	lim_add_mgmt_seq_num(mac_ctx, mac_hdr);
	qdf_status = cds_packet_alloc(buf_len, (void **)&frame,
				      (void **)&packet);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("call to bufAlloc failed for AUTH frame");
		return;
	}

	qdf_mem_copy(frame, buf, buf_len);
	lim_tx_mgmt_frame(mac_ctx, vdev_id, buf_len, packet, frame);
}

void lim_send_mgmt_frame_tx(struct mac_context *mac_ctx,
			    struct scheduler_msg *msg)
{
	struct sir_mgmt_msg *mb_msg = (struct sir_mgmt_msg *)msg->bodyptr;
	uint32_t msg_len;
	tpSirMacFrameCtl fc = (tpSirMacFrameCtl)mb_msg->data;
	uint8_t vdev_id;
	uint16_t auth_algo;

	msg_len = mb_msg->msg_len - sizeof(*mb_msg);
	vdev_id = mb_msg->vdev_id;

	if (fc->subType == SIR_MAC_MGMT_AUTH) {
		auth_algo = *(uint16_t *)(mb_msg->data +
					sizeof(tSirMacMgmtHdr));
		if (auth_algo == eSIR_AUTH_TYPE_SAE)
			lim_handle_sae_auth_retry(mac_ctx, vdev_id,
						  mb_msg->data, msg_len);
	}

	lim_send_frame(mac_ctx, vdev_id, mb_msg->data, msg_len);
}
