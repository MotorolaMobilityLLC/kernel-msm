/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

/*===========================================================================
 * lim_process_tdls.c
 * OVERVIEW:
 *
 * DEPENDENCIES:
 *
 * Are listed for each API below.
 * ===========================================================================*/

/*===========================================================================

 *                      EDIT HISTORY FOR FILE

 *  This section contains comments describing changes made to the module.
 *  Notice that changes are listed in reverse chronological order.

 *  $Header$$DateTime$$Author$

 *  when        who     what, where, why
 *  ----------    ---    ------------------------------------------------------
 *  05/05/2010   Ashwani    Initial Creation, added TDLS action frame
 *  functionality,TDLS message exchange with SME..etc..

   ===========================================================================*/

/**
 * \file lim_process_tdls.c
 *
 * \brief Code for preparing,processing and sending 802.11z action frames
 *
 */

#ifdef FEATURE_WLAN_TDLS

#include "sir_api.h"
#include "ani_global.h"
#include "sir_mac_prot_def.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_security_utils.h"
#include "dot11f.h"
#include "sch_api.h"
#include "lim_send_messages.h"
#include "utils_parser.h"
#include "lim_assoc_utils.h"
#include "lim_prop_exts_utils.h"
#include "dph_hash_table.h"
#include "wma_types.h"
#include "cds_regdomain.h"
#include "cds_utils.h"
#include "wlan_reg_services_api.h"
#include "wlan_tdls_tgt_api.h"
#include "wlan_mlme_public_struct.h"
#include "wlan_mlme_api.h"
#include "wlan_tdls_public_structs.h"

/* define NO_PAD_TDLS_MIN_8023_SIZE to NOT padding: See CR#447630
   There was IOT issue with cisco 1252 open mode, where it pads
   discovery req/teardown frame with some junk value up to min size.
   To avoid this issue, we pad QCOM_VENDOR_IE.
   If there is other IOT issue because of this bandage, define NO_PAD...
 */
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
#define MIN_IEEE_8023_SIZE              46
#define MIN_VENDOR_SPECIFIC_IE_SIZE     5
#endif

/*
 * TDLS data frames will go out/come in as non-qos data.
 * so, eth_890d_header will be aligned access..
 */
static const uint8_t eth_890d_header[] = {
	0xaa, 0xaa, 0x03, 0x00,
	0x00, 0x00, 0x89, 0x0d,
};

/*
 * type of links used in TDLS
 */
enum tdlsLinks {
	TDLS_LINK_AP,
	TDLS_LINK_DIRECT
} e_tdls_link;

enum tdlsReqType {
	TDLS_INITIATOR,
	TDLS_RESPONDER
} e_tdls_req_type;

typedef enum tdlsLinkSetupStatus {
	TDLS_SETUP_STATUS_SUCCESS = 0,
	TDLS_SETUP_STATUS_FAILURE = 37
} etdlsLinkSetupStatus;

/* These maps to Kernel TDLS peer capability
 * flags and should get changed as and when necessary
 */
enum tdls_peer_capability {
	TDLS_PEER_HT_CAP = 0,
	TDLS_PEER_VHT_CAP = 1,
	TDLS_PEER_WMM_CAP = 2,
	TDLS_PEER_HE_CAP = 3
} e_tdls_peer_capability;

#define LINK_IDEN_ADDR_OFFSET(x) (&x.LinkIdentifier)

/* TODO, Move this parameters to configuration */
#define PEER_PSM_SUPPORT          (0)
#define TDLS_SUPPORT              (1)
#define TDLS_PROHIBITED           (0)
#define TDLS_CH_SWITCH_PROHIBITED (1)
/** @brief Set bit manipulation macro */
#define SET_BIT(value, mask)       ((value) |= (1 << (mask)))
/** @brief Clear bit manipulation macro */
#define CLEAR_BIT(value, mask)     ((value) &= ~(1 << (mask)))
/** @brief Check bit manipulation macro */
#define CHECK_BIT(value, mask)    ((value) & (1 << (mask)))

#define SET_PEER_AID_BITMAP(peer_bitmap, aid) \
	do { \
	if ((aid) < (sizeof(uint32_t) << 3)) \
		SET_BIT(peer_bitmap[0], (aid));	\
	else if ((aid) < (sizeof(uint32_t) << 4)) \
		SET_BIT(peer_bitmap[1], ((aid) - (sizeof(uint32_t) << 3)));\
	} while (0);

#define CLEAR_PEER_AID_BITMAP(peer_bitmap, aid)	\
	do { \
	if ((aid) < (sizeof(uint32_t) << 3)) \
		CLEAR_BIT(peer_bitmap[0], (aid)); \
	else if ((aid) < (sizeof(uint32_t) << 4)) \
		CLEAR_BIT(peer_bitmap[1], ((aid) - (sizeof(uint32_t) << 3)));\
	} while (0);

#define IS_QOS_ENABLED(pe_session) ((((pe_session)->limQosEnabled) && \
					SIR_MAC_GET_QOS((pe_session)->limCurrentBssCaps)) ||	\
				       (((pe_session)->limWmeEnabled) && \
					LIM_BSS_CAPS_GET(WME, (pe_session)->limCurrentBssQosCaps)))

#define TID_AC_VI                  4
#define TID_AC_BK                  1

static const uint8_t *lim_trace_tdls_action_string(uint8_t tdlsActionCode)
{
	switch (tdlsActionCode) {
		CASE_RETURN_STRING(TDLS_SETUP_REQUEST);
		CASE_RETURN_STRING(TDLS_SETUP_RESPONSE);
		CASE_RETURN_STRING(TDLS_SETUP_CONFIRM);
		CASE_RETURN_STRING(TDLS_TEARDOWN);
		CASE_RETURN_STRING(TDLS_PEER_TRAFFIC_INDICATION);
		CASE_RETURN_STRING(TDLS_CHANNEL_SWITCH_REQUEST);
		CASE_RETURN_STRING(TDLS_CHANNEL_SWITCH_RESPONSE);
		CASE_RETURN_STRING(TDLS_PEER_TRAFFIC_RESPONSE);
		CASE_RETURN_STRING(TDLS_DISCOVERY_REQUEST);
		CASE_RETURN_STRING(TDLS_DISCOVERY_RESPONSE);
	}
	return (const uint8_t *)"UNKNOWN";
}

/*
 * initialize TDLS setup list and related data structures.
 */
void lim_init_tdls_data(struct mac_context *mac, struct pe_session *pe_session)
{
	lim_init_peer_idxpool(mac, pe_session);

	return;
}

static void populate_dot11f_tdls_offchannel_params(
				struct mac_context *mac,
				struct pe_session *pe_session,
				tDot11fIESuppChannels *suppChannels,
				tDot11fIESuppOperatingClasses *suppOperClasses)
{
	uint32_t numChans = CFG_VALID_CHANNEL_LIST_LEN;
	uint8_t validChan[CFG_VALID_CHANNEL_LIST_LEN];
	uint8_t i;
	uint8_t valid_count = 0;
	uint8_t chanOffset;
	uint8_t op_class;
	uint8_t numClasses;
	uint8_t classes[REG_MAX_SUPP_OPER_CLASSES];
	uint32_t band;
	uint8_t nss_2g;
	uint8_t nss_5g;
	qdf_freq_t ch_freq;

	numChans = mac->mlme_cfg->reg.valid_channel_list_num;

	for (i = 0; i < mac->mlme_cfg->reg.valid_channel_list_num; i++) {
		validChan[i] = wlan_reg_freq_to_chan(mac->pdev,
						     mac->mlme_cfg->reg.valid_channel_freq_list[i]);
	}

	if (wlan_reg_is_5ghz_ch_freq(pe_session->curr_op_freq))
		band = BAND_5G;
	else
		band = BAND_2G;

	nss_5g = QDF_MIN(mac->vdev_type_nss_5g.tdls,
			 mac->user_configured_nss);
	nss_2g = QDF_MIN(mac->vdev_type_nss_2g.tdls,
			 mac->user_configured_nss);

	/* validating the channel list for DFS and 2G channels */
	for (i = 0U; i < numChans; i++) {
		ch_freq = wlan_reg_legacy_chan_to_freq(mac->pdev, validChan[i]);
		if ((band == BAND_5G) &&
		    (NSS_2x2_MODE == nss_5g) &&
		    (NSS_1x1_MODE == nss_2g) &&
		    (wlan_reg_is_dfs_for_freq(mac->pdev, ch_freq))) {
			pe_debug("skipping channel: %d, nss_5g: %d, nss_2g: %d",
				validChan[i], nss_5g, nss_2g);
			continue;
		} else {
			if (wlan_reg_is_dsrc_chan(mac->pdev, validChan[i])) {
				pe_debug("skipping channel: %d from the valid channel list",
					validChan[i]);
				continue;
			}
		}

		if (valid_count >= ARRAY_SIZE(suppChannels->bands))
			break;
		suppChannels->bands[valid_count][0] = validChan[i];
		suppChannels->bands[valid_count][1] = 1;
		valid_count++;
	}

	suppChannels->num_bands = valid_count;
	suppChannels->present = 1;

	/* find channel offset and get op class for current operating channel */
	switch (pe_session->htSecondaryChannelOffset) {
	case PHY_SINGLE_CHANNEL_CENTERED:
		chanOffset = BW20;
		break;

	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		chanOffset = BW40_LOW_PRIMARY;
		break;

	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		chanOffset = BW40_HIGH_PRIMARY;
		break;

	default:
		chanOffset = BWALL;
		break;
	}

	op_class = wlan_reg_dmn_get_opclass_from_channel(
		mac->scan.countryCodeCurrent,
		wlan_reg_freq_to_chan(mac->pdev, pe_session->curr_op_freq),
		chanOffset);

	pe_debug("countryCodeCurrent: %s, curr_op_freq: %d, htSecondaryChannelOffset: %d, chanOffset: %d op class: %d",
		 mac->scan.countryCodeCurrent,
		 pe_session->curr_op_freq,
		 pe_session->htSecondaryChannelOffset,
		 chanOffset, op_class);
	suppOperClasses->present = 1;
	suppOperClasses->classes[0] = op_class;

	wlan_reg_dmn_get_curr_opclasses(&numClasses, &classes[0]);

	for (i = 0; i < numClasses; i++)
		suppOperClasses->classes[i + 1] = classes[i];

	/* add one for present operating class, added in the beginning */
	suppOperClasses->num_classes = numClasses + 1;

	return;
}

/*
 * FUNCTION: Populate Link Identifier element IE
 *
 */

static void populate_dot11f_link_iden(struct mac_context *mac,
				      struct pe_session *pe_session,
				      tDot11fIELinkIdentifier *linkIden,
				      struct qdf_mac_addr peer_mac,
				      uint8_t reqType)
{
	uint8_t *initStaAddr = NULL;
	uint8_t *respStaAddr = NULL;

	(reqType == TDLS_INITIATOR) ? ((initStaAddr = linkIden->InitStaAddr),
				       (respStaAddr = linkIden->RespStaAddr))
	: ((respStaAddr = linkIden->InitStaAddr),
	   (initStaAddr = linkIden->RespStaAddr));
	qdf_mem_copy((uint8_t *) linkIden->bssid,
		     (uint8_t *) pe_session->bssId, QDF_MAC_ADDR_SIZE);

	qdf_mem_copy((uint8_t *) initStaAddr,
		     pe_session->self_mac_addr, QDF_MAC_ADDR_SIZE);

	qdf_mem_copy((uint8_t *) respStaAddr, (uint8_t *) peer_mac.bytes,
		     QDF_MAC_ADDR_SIZE);

	linkIden->present = 1;
	return;

}

static void populate_dot11f_tdls_ext_capability(struct mac_context *mac,
						struct pe_session *pe_session,
						tDot11fIEExtCap *extCapability)
{
	struct s_ext_cap *p_ext_cap = (struct s_ext_cap *)extCapability->bytes;

	p_ext_cap->tdls_peer_psm_supp = PEER_PSM_SUPPORT;
	p_ext_cap->tdls_peer_uapsd_buffer_sta = mac->lim.gLimTDLSBufStaEnabled;

	/*
	 * Set TDLS channel switching bits only if offchannel is enabled
	 * and TDLS Channel Switching is not prohibited by AP in ExtCap
	 * IE in assoc/re-assoc response.
	 */
	if ((1 == mac->lim.gLimTDLSOffChannelEnabled) &&
	    (!pe_session->tdls_chan_swit_prohibited)) {
		p_ext_cap->tdls_channel_switching = 1;
		p_ext_cap->tdls_chan_swit_prohibited = 0;
	} else {
	    p_ext_cap->tdls_channel_switching = 0;
	    p_ext_cap->tdls_chan_swit_prohibited = TDLS_CH_SWITCH_PROHIBITED;
	}
	p_ext_cap->tdls_support = TDLS_SUPPORT;
	p_ext_cap->tdls_prohibited = TDLS_PROHIBITED;

	extCapability->present = 1;
	extCapability->num_bytes = lim_compute_ext_cap_ie_length(extCapability);

	return;
}

/*
 * prepare TDLS frame header, it includes
 * |             |              |                |
 * |802.11 header|RFC1042 header|TDLS_PYLOAD_TYPE|PAYLOAD
 * |             |              |                |
 */
static uint32_t lim_prepare_tdls_frame_header(struct mac_context *mac, uint8_t *pFrame,
					      tDot11fIELinkIdentifier *link_iden,
					      uint8_t tdlsLinkType, uint8_t reqType,
					      uint8_t tid,
					      struct pe_session *pe_session)
{
	tpSirMacDataHdr3a pMacHdr;
	uint32_t header_offset = 0;
	uint8_t *addr1 = NULL;
	uint8_t *addr3 = NULL;
	uint8_t toDs = (tdlsLinkType == TDLS_LINK_AP)
		       ? ANI_TXDIR_TODS : ANI_TXDIR_IBSS;
	uint8_t *peerMac = (reqType == TDLS_INITIATOR)
			   ? link_iden->RespStaAddr : link_iden->InitStaAddr;
	uint8_t *staMac = (reqType == TDLS_INITIATOR)
			  ? link_iden->InitStaAddr : link_iden->RespStaAddr;
	tpDphHashNode sta_ds;
	uint16_t aid = 0;
	uint8_t qos_mode = 0;

	pMacHdr = (tpSirMacDataHdr3a) (pFrame);

	/*
	 * if TDLS frame goes through the AP link, it follows normal address
	 * pattern, if TDLS frame goes thorugh the direct link, then
	 * A1--> Peer STA addr, A2-->Self STA address, A3--> BSSID
	 */
	(tdlsLinkType == TDLS_LINK_AP) ? ((addr1 = (link_iden->bssid)),
					  (addr3 = (peerMac)))
	: ((addr1 = (peerMac)), (addr3 = (link_iden->bssid)));
	/*
	 * prepare 802.11 header
	 */
	pMacHdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
	pMacHdr->fc.type = SIR_MAC_DATA_FRAME;

	sta_ds = dph_lookup_hash_entry(mac, peerMac, &aid,
					&pe_session->dph.dphHashTable);
	if (sta_ds)
		qos_mode = sta_ds->qosMode;

	pMacHdr->fc.subType =
		((IS_QOS_ENABLED(pe_session) &&
		(tdlsLinkType == TDLS_LINK_AP)) ||
		((tdlsLinkType == TDLS_LINK_DIRECT) && qos_mode))
		? SIR_MAC_DATA_QOS_DATA : SIR_MAC_DATA_DATA;

	/*
	 * TL is not setting up below fields, so we are doing it here
	 */
	pMacHdr->fc.toDS = toDs;
	pMacHdr->fc.powerMgmt = 0;
	pMacHdr->fc.wep = (pe_session->encryptType == eSIR_ED_NONE) ? 0 : 1;

	qdf_mem_copy((uint8_t *) pMacHdr->addr1,
		     (uint8_t *) addr1, sizeof(tSirMacAddr));
	qdf_mem_copy((uint8_t *) pMacHdr->addr2,
		     (uint8_t *) staMac, sizeof(tSirMacAddr));

	qdf_mem_copy((uint8_t *) pMacHdr->addr3,
		     (uint8_t *) (addr3), sizeof(tSirMacAddr));

	pe_debug("Preparing TDLS frame header to %s A1:"
		   QDF_MAC_ADDR_FMT", A2:"QDF_MAC_ADDR_FMT", A3:"
		   QDF_MAC_ADDR_FMT,
		(tdlsLinkType == TDLS_LINK_AP) ? "AP" : "DIRECT",
		QDF_MAC_ADDR_REF(pMacHdr->addr1),
		QDF_MAC_ADDR_REF(pMacHdr->addr2),
		QDF_MAC_ADDR_REF(pMacHdr->addr3));

	if (pMacHdr->fc.subType == SIR_MAC_DATA_QOS_DATA) {
		pMacHdr->qosControl.tid = tid;
		header_offset += sizeof(tSirMacDataHdr3a);
	} else
		header_offset += sizeof(tSirMacMgmtHdr);

	/*
	 * Now form RFC1042 header
	 */
	qdf_mem_copy((uint8_t *) (pFrame + header_offset),
		     (uint8_t *) eth_890d_header, sizeof(eth_890d_header));

	header_offset += sizeof(eth_890d_header);

	/* add payload type as TDLS */
	*(pFrame + header_offset) = PAYLOAD_TYPE_TDLS;
	header_offset += PAYLOAD_TYPE_TDLS_SIZE;
	return header_offset;
}

/**
 * lim_mgmt_tdls_tx_complete - callback to indicate Tx completion
 * @context: pointer to mac structure
 * @buf: buffer
 * @tx_complete: indicates tx success/failure
 * @params: tx completion params
 *
 * function will be invoked on receiving tx completion indication
 *
 * return: success: eHAL_STATUS_SUCCESS failure: eHAL_STATUS_FAILURE
 */
static QDF_STATUS lim_mgmt_tdls_tx_complete(void *context,
					    qdf_nbuf_t buf,
					    uint32_t tx_complete,
					    void *params)
{
	struct mac_context *mac_ctx = (struct mac_context *)context;

	pe_debug("tdls_frm_session_id: %x tx_complete: %x",
		mac_ctx->lim.tdls_frm_session_id, tx_complete);

	if (NO_SESSION != mac_ctx->lim.tdls_frm_session_id) {
		lim_send_sme_mgmt_tx_completion(mac_ctx,
				mac_ctx->lim.tdls_frm_session_id,
				tx_complete);
		mac_ctx->lim.tdls_frm_session_id = NO_SESSION;
	}

	if (buf)
		qdf_nbuf_free(buf);

	return QDF_STATUS_SUCCESS;
}

/*
 * This function can be used for bacst or unicast discovery request
 * We are not differentiating it here, it will all depnds on peer MAC address,
 */
static QDF_STATUS lim_send_tdls_dis_req_frame(struct mac_context *mac,
					      struct qdf_mac_addr peer_mac,
					      uint8_t dialog,
					      struct pe_session *pe_session,
					      enum wifi_traffic_ac ac)
{
	tDot11fTDLSDisReq *tdls_dis_req;
	uint32_t status = 0;
	uint32_t nPayload = 0;
	uint32_t size = 0;
	uint32_t nBytes = 0;
	uint32_t header_offset = 0;
	uint8_t *pFrame;
	void *pPacket;
	QDF_STATUS qdf_status;
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
	uint32_t padLen = 0;
#endif
	uint8_t smeSessionId = 0;

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	tdls_dis_req = qdf_mem_malloc(sizeof(*tdls_dis_req));
	if (!tdls_dis_req)
		return QDF_STATUS_E_NOMEM;

	smeSessionId = pe_session->smeSessionId;
	/*
	 * The scheme here is to fill out a 'tDot11fProbeRequest' structure
	 * and then hand it off to 'dot11f_pack_probe_request' (for
	 * serialization).
	 */

	/*
	 * setup Fixed fields,
	 */
	tdls_dis_req->Category.category = ACTION_CATEGORY_TDLS;
	tdls_dis_req->Action.action = TDLS_DISCOVERY_REQUEST;
	tdls_dis_req->DialogToken.token = dialog;

	size = sizeof(tSirMacAddr);

	populate_dot11f_link_iden(mac, pe_session,
				  &tdls_dis_req->LinkIdentifier,
				  peer_mac, TDLS_INITIATOR);

	/*
	 * now we pack it.  First, how much space are we going to need?
	 */
	status = dot11f_get_packed_tdls_dis_req_size(mac, tdls_dis_req,
						     &nPayload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to calculate the packed size for a discovery Request (0x%08x)",
			status);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fTDLSDisReq);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while calculating the packed size for a discovery Request (0x%08x)",
			status);
	}

	/*
	 * This frame is going out from PE as data frames with special ethertype
	 * 89-0d.
	 * 8 bytes of RFC 1042 header
	 */

	nBytes = nPayload + ((IS_QOS_ENABLED(pe_session))
			     ? sizeof(tSirMacDataHdr3a) :
			     sizeof(tSirMacMgmtHdr))
		 + sizeof(eth_890d_header)
		 + PAYLOAD_TYPE_TDLS_SIZE;

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
	/* IOT issue with some AP : some AP doesn't like the data packet size < minimum 802.3 frame length (64)
	   Hence AP itself padding some bytes, which caused teardown packet is dropped at
	   receiver side. To avoid such IOT issue, we added some extra bytes to meet data frame size >= 64
	 */
	if (nPayload + PAYLOAD_TYPE_TDLS_SIZE < MIN_IEEE_8023_SIZE) {
		padLen =
			MIN_IEEE_8023_SIZE - (nPayload + PAYLOAD_TYPE_TDLS_SIZE);

		/* if padLen is less than minimum vendorSpecific (5), pad up to 5 */
		if (padLen < MIN_VENDOR_SPECIFIC_IE_SIZE)
			padLen = MIN_VENDOR_SPECIFIC_IE_SIZE;

		nBytes += padLen;
	}
#endif

	/* Ok-- try to allocate memory from MGMT PKT pool */

	qdf_status = cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				      (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate: %d bytes for a TDLS Discovery Request",
			nBytes);
		qdf_mem_free(tdls_dis_req);
		return QDF_STATUS_E_NOMEM;
	}

	/* zero out the memory */
	qdf_mem_zero(pFrame, nBytes);

	/*
	 * IE formation, memory allocation is completed, Now form TDLS discovery
	 * request frame
	 */

	/* fill out the buffer descriptor */

	header_offset = lim_prepare_tdls_frame_header(mac, pFrame,
			      &tdls_dis_req->LinkIdentifier, TDLS_LINK_AP,
			      TDLS_INITIATOR,
			      (ac == WIFI_AC_VI) ? TID_AC_VI : TID_AC_BK,
			      pe_session);

	status = dot11f_pack_tdls_dis_req(mac, tdls_dis_req, pFrame
					  + header_offset, nPayload, &nPayload);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack a TDLS discovery req (0x%08x)",
			status);
		cds_packet_free((void *)pPacket);
		qdf_mem_free(tdls_dis_req);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing TDLS Discovery Request (0x%08x)",
			status);
	}
	qdf_mem_free(tdls_dis_req);

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
	if (padLen != 0) {
		/* QCOM VENDOR OUI = { 0x00, 0xA0, 0xC6, type = 0x0000 }; */
		uint8_t *padVendorSpecific = pFrame + header_offset + nPayload;
		/* make QCOM_VENDOR_OUI, and type = 0x0000, and all the payload to be zero */
		padVendorSpecific[0] = 221;
		padVendorSpecific[1] = padLen - 2;
		padVendorSpecific[2] = 0x00;
		padVendorSpecific[3] = 0xA0;
		padVendorSpecific[4] = 0xC6;

		pe_debug("Padding Vendor Specific Ie Len: %d", padLen);

		/* padding zero if more than 5 bytes are required */
		if (padLen > MIN_VENDOR_SPECIFIC_IE_SIZE)
			qdf_mem_zero(pFrame + header_offset + nPayload +
				    MIN_VENDOR_SPECIFIC_IE_SIZE,
				    padLen - MIN_VENDOR_SPECIFIC_IE_SIZE);
	}
#endif

	pe_debug("[TDLS] action: %d (%s) -AP-> OTA peer="QDF_MAC_ADDR_FMT,
		TDLS_DISCOVERY_REQUEST,
		lim_trace_tdls_action_string(TDLS_DISCOVERY_REQUEST),
		QDF_MAC_ADDR_REF(peer_mac.bytes));

	mac->lim.tdls_frm_session_id = pe_session->smeSessionId;
	lim_diag_mgmt_tx_event_report(mac, (tpSirMacMgmtHdr) pFrame,
				      pe_session, QDF_STATUS_SUCCESS,
				      QDF_STATUS_SUCCESS);
	qdf_status = wma_tx_frameWithTxComplete(mac, pPacket,
					(uint16_t) nBytes,
					TXRX_FRM_802_11_DATA,
					ANI_TXDIR_TODS,
					TID_AC_VI,
					lim_tx_complete, pFrame,
					lim_mgmt_tdls_tx_complete,
					HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME |
					HAL_USE_PEER_STA_REQUESTED_MASK,
					smeSessionId, false, 0,
					RATEID_DEFAULT, 0);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		mac->lim.tdls_frm_session_id = NO_SESSION;
		pe_err("could not send TDLS Discovery Request frame");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;

}

/*
 * This static function is consistent with any kind of TDLS management
 * frames we are sending. Currently it is being used by lim_send_tdls_dis_rsp_frame,
 * lim_send_tdls_link_setup_req_frame and lim_send_tdls_setup_rsp_frame
 */
static void populate_dot11f_tdls_ht_vht_cap(struct mac_context *mac,
					    uint32_t selfDot11Mode,
					    tDot11fIEHTCaps *htCap,
					    tDot11fIEVHTCaps *vhtCap,
					    struct pe_session *pe_session)
{
	uint8_t nss;
	qdf_size_t val_len;
	struct mlme_vht_capabilities_info *vht_cap_info;

	vht_cap_info = &mac->mlme_cfg->vht_caps.vht_cap_info;

	if (wlan_reg_is_5ghz_ch_freq(pe_session->curr_op_freq))
		nss = mac->vdev_type_nss_5g.tdls;
	else
		nss = mac->vdev_type_nss_2g.tdls;

	nss = QDF_MIN(nss, mac->user_configured_nss);
	if (IS_DOT11_MODE_HT(selfDot11Mode) &&
	    !lim_is_he_6ghz_band(pe_session)) {
		/* Include HT Capability IE */
		populate_dot11f_ht_caps(mac, pe_session, htCap);
		val_len = SIZE_OF_SUPPORTED_MCS_SET;
		wlan_mlme_get_cfg_str(&htCap->supportedMCSSet[0],
				      &mac->mlme_cfg->rates.supported_mcs_set,
				      &val_len);
		if (NSS_1x1_MODE == nss)
			htCap->supportedMCSSet[1] = 0;
		/*
		 * Advertise ht capability and max supported channel bandwidth
		 * when populating HT IE in TDLS Setup Request/Setup Response/
		 * Setup Confirmation frames.
		 * 11.21.6.2 Setting up a 40 MHz direct link: A 40 MHz
		 * off-channel direct link may be started if both TDLS peer STAs
		 * indicated 40 MHz support in the Supported Channel Width Set
		 * field of the HT Capabilities element (which is included in
		 * the TDLS Setup Request frame and the TDLS Setup Response
		 * frame). Switching to a 40 MHz off-channel direct link is
		 * achieved by including the following information in the TDLS
		 * Channel Switch Request
		 * 11.21.1 General: The channel width of the TDLS direct link on
		 * the base channel shall not exceed the channel width of the
		 * BSS to which the TDLS peer STAs are associated.
		 * Select supportedChannelWidthSet based on channel bonding
		 * settings for each band
		 */
	} else {
		htCap->present = 0;
	}
	pe_debug("HT present: %hu, Chan Width: %hu",
		htCap->present, htCap->supportedChannelWidthSet);

	if ((WLAN_REG_IS_24GHZ_CH_FREQ(pe_session->curr_op_freq) &&
	     vht_cap_info->b24ghz_band) ||
	    WLAN_REG_IS_5GHZ_CH_FREQ(pe_session->curr_op_freq)) {
		if (IS_DOT11_MODE_VHT(selfDot11Mode) &&
		    IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/* Include VHT Capability IE */
			populate_dot11f_vht_caps(mac, pe_session, vhtCap);

			/*
			 * Set to 0 if the TDLS STA does not support either 160
			 * or 80+80 MHz.
			 * Set to 1 if the TDLS STA supports 160 MHz.
			 * Set to 2 if the TDLS STA supports 160 MHz and
			 * 80+80 MHz.
			 * The value 3 is reserved
			 */
			vhtCap->supportedChannelWidthSet = 0;

			vhtCap->suBeamformeeCap = 0;
			vhtCap->suBeamFormerCap = 0;
			vhtCap->muBeamformeeCap = 0;
			vhtCap->muBeamformerCap = 0;

			vhtCap->rxMCSMap = vht_cap_info->rx_mcs_map;

			vhtCap->rxHighSupDataRate =
				vht_cap_info->rx_supp_data_rate;
			vhtCap->txMCSMap = vht_cap_info->tx_mcs_map;
			vhtCap->txSupDataRate = vht_cap_info->tx_supp_data_rate;
			if (nss == NSS_1x1_MODE) {
				vhtCap->txMCSMap |= DISABLE_NSS2_MCS;
				vhtCap->rxMCSMap |= DISABLE_NSS2_MCS;
				vhtCap->txSupDataRate =
					VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
				vhtCap->rxHighSupDataRate =
					VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
			}
		} else {
			vhtCap->present = 0;
		}
	} else {
		/* Vht Disable from ini in 2.4 GHz */
		vhtCap->present = 0;
	}
	pe_debug("VHT present: %hu, Chan Width: %hu",
		 vhtCap->present, vhtCap->supportedChannelWidthSet);
}

#ifdef WLAN_FEATURE_11AX
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static void
lim_tdls_populate_dot11f_6hgz_he_caps(struct mac_context *mac,
				      struct tdls_add_sta_req *add_sta_req,
				      tDot11fIEhe_6ghz_band_cap *pDot11f)
{
	union {
		struct hecap_6ghz he_6ghz_cap;
		struct he_6ghz_capability_info dot11f;
	} peer_cfg;

	if (!add_sta_req->he_6ghz_cap.a_mpdu_params &&
	    !add_sta_req->he_6ghz_cap.info) {
		tdls_debug("6Ghz HE capabilities not present");
		return;
	}

	qdf_mem_copy(&peer_cfg.he_6ghz_cap,
		     &add_sta_req->he_6ghz_cap,
		     sizeof(add_sta_req->he_6ghz_cap));

	pDot11f->present = 1;
	pDot11f->min_mpdu_start_spacing =
				peer_cfg.dot11f.min_mpdu_start_spacing;
	pDot11f->max_ampdu_len_exp = peer_cfg.dot11f.max_ampdu_len_exp;
	pDot11f->max_mpdu_len = peer_cfg.dot11f.max_mpdu_len;
	pDot11f->sm_pow_save = peer_cfg.dot11f.sm_pow_save;
	pDot11f->rd_responder = peer_cfg.dot11f.rd_responder;
	pDot11f->rx_ant_pattern_consistency =
				peer_cfg.dot11f.rx_ant_pattern_consistency;
	pDot11f->tx_ant_pattern_consistency =
				peer_cfg.dot11f.tx_ant_pattern_consistency;

	lim_log_he_6g_cap(mac, pDot11f);
}

static void lim_populate_tdls_setup_6g_cap(struct mac_context *mac,
					   tDot11fIEhe_6ghz_band_cap *hecap_6g,
					   struct pe_session *session)
{
	if (hecap_6g)
		populate_dot11f_he_6ghz_cap(mac, session, hecap_6g);
}

#else
static void
lim_tdls_populate_dot11f_6hgz_he_caps(struct mac_context *mac,
				      struct tdls_add_sta_req *add_sta_req,
				      tDot11fIEhe_6ghz_band_cap *pDot11f)
{
}

static void lim_populate_tdls_setup_6g_cap(struct mac_context *mac,
					   tDot11fIEhe_6ghz_band_cap *hecap_6g,
					   struct pe_session *session)
{
}
#endif

static void lim_tdls_set_he_chan_width(tDot11fIEhe_cap *heCap,
				       struct pe_session *session)
{
	if (lim_is_session_he_capable(session)) {
		heCap->chan_width_0 = session->he_config.chan_width_0 &
				      heCap->chan_width_0;
		heCap->chan_width_1 = session->he_config.chan_width_1 &
				      heCap->chan_width_1;
		heCap->chan_width_2 = session->he_config.chan_width_2 &
				      heCap->chan_width_2;
		heCap->chan_width_3 = session->he_config.chan_width_3 &
				      heCap->chan_width_3;
		heCap->chan_width_4 = session->he_config.chan_width_4 &
				      heCap->chan_width_4;
		heCap->chan_width_5 = session->he_config.chan_width_5 &
				      heCap->chan_width_5;
		heCap->chan_width_6 = session->he_config.chan_width_6 &
				      heCap->chan_width_6;
	} else {
		if (session->ch_width == CH_WIDTH_20MHZ) {
			heCap->chan_width_0 = 0;
			heCap->chan_width_1 = 0;
		}
	/* Right now, no support for ch_width 160 Mhz or 80P80 Mhz in 5 Ghz*/
		heCap->chan_width_2 = 0;
		heCap->chan_width_3 = 0;
	}
}

static void populate_dot11f_set_tdls_he_cap(struct mac_context *mac,
					    uint32_t selfDot11Mode,
					    tDot11fIEhe_cap *heCap,
					    tDot11fIEhe_6ghz_band_cap *hecap_6g,
					    struct pe_session *session)
{
	if (IS_DOT11_MODE_HE(selfDot11Mode)) {
		populate_dot11f_he_caps(mac, NULL, heCap);
		lim_tdls_set_he_chan_width(heCap, session);
		lim_log_he_cap(mac, heCap);
		lim_populate_tdls_setup_6g_cap(mac, hecap_6g, session);
	} else {
		pe_debug("Not populating he cap as SelfDot11Mode not HE %d",
			 selfDot11Mode);
	}
}

static void lim_tdls_fill_dis_rsp_he_cap(struct mac_context *mac,
					 uint32_t selfDot11Mode,
					 tDot11fTDLSDisRsp *tdls_dis_rsp,
					 struct pe_session *pe_session)
{
	populate_dot11f_set_tdls_he_cap(mac, selfDot11Mode,
					&tdls_dis_rsp->he_cap, NULL,
					pe_session);
}

static void lim_tdls_fill_setup_req_he_cap(struct mac_context *mac,
					   uint32_t selfDot11Mode,
					   tDot11fTDLSSetupReq *tdls_setup_req,
					   struct pe_session *pe_session)
{
	populate_dot11f_set_tdls_he_cap(mac, selfDot11Mode,
					&tdls_setup_req->he_cap,
					&tdls_setup_req->he_6ghz_band_cap,
					pe_session);
}

static void lim_tdls_fill_setup_rsp_he_cap(struct mac_context *mac,
					   uint32_t selfDot11Mode,
					   tDot11fTDLSSetupRsp *tdls_setup_rsp,
					   struct pe_session *pe_session)
{
	populate_dot11f_set_tdls_he_cap(mac, selfDot11Mode,
					&tdls_setup_rsp->he_cap,
					&tdls_setup_rsp->he_6ghz_band_cap,
					pe_session);
}

static void lim_tdls_populate_he_operations(struct mac_context *mac,
					    struct pe_session *pe_session,
					    tDot11fIEhe_op *he_op)
{
	struct wlan_mlme_he_caps *he_cap_info;
	uint16_t mcs_set = 0;

	he_cap_info = &mac->mlme_cfg->he_caps;
	he_op->co_located_bss = 0;
	he_op->bss_color = pe_session->he_bss_color_change.new_color;
	if (!he_op->bss_color)
		he_op->bss_col_disabled = 1;

	mcs_set = (uint16_t)he_cap_info->he_ops_basic_mcs_nss;
	if (pe_session->nss == NSS_1x1_MODE)
		mcs_set |= 0xFFFC;
	else
		mcs_set |= 0xFFF0;

	*((uint16_t *)he_op->basic_mcs_nss) = mcs_set;
	populate_dot11f_he_operation(mac,
				     pe_session,
				     he_op);
}

static void lim_tdls_fill_setup_cnf_he_op(struct mac_context *mac,
					  uint32_t peer_capability,
					  tDot11fTDLSSetupCnf *tdls_setup_cnf,
					  struct pe_session *pe_session)
{
	if (CHECK_BIT(peer_capability, TDLS_PEER_HE_CAP))
		lim_tdls_populate_he_operations(mac,
						pe_session,
						&tdls_setup_cnf->he_op);
}

static void lim_tdls_populate_he_matching_rate_set(struct mac_context *mac_ctx,
						   tpDphHashNode stads,
						   uint8_t nss,
						   struct pe_session *session)
{
	lim_populate_he_mcs_set(mac_ctx, &stads->supportedRates,
				&stads->he_config, session, nss);
}

static QDF_STATUS
lim_tdls_populate_dot11f_he_caps(struct mac_context *mac,
				 struct tdls_add_sta_req *add_sta_req,
				 tDot11fIEhe_cap *pDot11f)
{
	uint32_t chan_width;
	union {
		struct hecap nCfgValue;
		struct he_capability_info he_cap;
	} uHECapInfo;

	if (add_sta_req->he_cap_len < MIN_TDLS_HE_CAP_LEN) {
		pe_debug("He_capability invalid");
		return QDF_STATUS_E_INVAL;
	}

	qdf_mem_copy(&uHECapInfo.nCfgValue, &add_sta_req->he_cap,
		     sizeof(uHECapInfo.nCfgValue));

	pDot11f->present = 1;
	pDot11f->htc_he = uHECapInfo.he_cap.htc_he;
	pDot11f->twt_request = uHECapInfo.he_cap.twt_request;
	pDot11f->twt_responder = uHECapInfo.he_cap.twt_responder;
	pDot11f->fragmentation = uHECapInfo.he_cap.fragmentation;
	pDot11f->max_num_frag_msdu_amsdu_exp =
		uHECapInfo.he_cap.max_num_frag_msdu_amsdu_exp;
	pDot11f->min_frag_size = uHECapInfo.he_cap.min_frag_size;
	pDot11f->trigger_frm_mac_pad = uHECapInfo.he_cap.trigger_frm_mac_pad;
	pDot11f->multi_tid_aggr_rx_supp =
		uHECapInfo.he_cap.multi_tid_aggr_rx_supp;
	pDot11f->he_link_adaptation = uHECapInfo.he_cap.he_link_adaptation;
	pDot11f->all_ack = uHECapInfo.he_cap.all_ack;
	pDot11f->trigd_rsp_sched = uHECapInfo.he_cap.trigd_rsp_sched;
	pDot11f->a_bsr = uHECapInfo.he_cap.a_bsr;
	pDot11f->broadcast_twt = uHECapInfo.he_cap.broadcast_twt;
	pDot11f->ba_32bit_bitmap = uHECapInfo.he_cap.ba_32bit_bitmap;
	pDot11f->mu_cascade = uHECapInfo.he_cap.mu_cascade;
	pDot11f->ack_enabled_multitid = uHECapInfo.he_cap.ack_enabled_multitid;
	pDot11f->omi_a_ctrl = uHECapInfo.he_cap.omi_a_ctrl;
	pDot11f->ofdma_ra = uHECapInfo.he_cap.ofdma_ra;
	pDot11f->max_ampdu_len_exp_ext =
		uHECapInfo.he_cap.max_ampdu_len_exp_ext;
	pDot11f->amsdu_frag = uHECapInfo.he_cap.amsdu_frag;
	pDot11f->flex_twt_sched = uHECapInfo.he_cap.flex_twt_sched;
	pDot11f->rx_ctrl_frame = uHECapInfo.he_cap.rx_ctrl_frame;

	pDot11f->bsrp_ampdu_aggr = uHECapInfo.he_cap.bsrp_ampdu_aggr;
	pDot11f->qtp = uHECapInfo.he_cap.qtp;
	pDot11f->a_bqr = uHECapInfo.he_cap.a_bqr;
	pDot11f->spatial_reuse_param_rspder =
		uHECapInfo.he_cap.spatial_reuse_param_rspder;
	pDot11f->ops_supp = uHECapInfo.he_cap.ops_supp;
	pDot11f->ndp_feedback_supp = uHECapInfo.he_cap.ndp_feedback_supp;
	pDot11f->amsdu_in_ampdu = uHECapInfo.he_cap.amsdu_in_ampdu;

	chan_width = uHECapInfo.he_cap.chan_width;
	pDot11f->chan_width_0 = HE_CH_WIDTH_GET_BIT(chan_width, 0);
	pDot11f->chan_width_1 = HE_CH_WIDTH_GET_BIT(chan_width, 1);
	pDot11f->chan_width_2 = HE_CH_WIDTH_GET_BIT(chan_width, 2);
	pDot11f->chan_width_3 = HE_CH_WIDTH_GET_BIT(chan_width, 3);
	pDot11f->chan_width_4 = HE_CH_WIDTH_GET_BIT(chan_width, 4);
	pDot11f->chan_width_5 = HE_CH_WIDTH_GET_BIT(chan_width, 5);
	pDot11f->chan_width_6 = HE_CH_WIDTH_GET_BIT(chan_width, 6);

	pDot11f->rx_pream_puncturing = uHECapInfo.he_cap.rx_pream_puncturing;
	pDot11f->device_class = uHECapInfo.he_cap.device_class;
	pDot11f->ldpc_coding = uHECapInfo.he_cap.ldpc_coding;
	pDot11f->he_1x_ltf_800_gi_ppdu =
		uHECapInfo.he_cap.he_1x_ltf_800_gi_ppdu;
	pDot11f->midamble_tx_rx_max_nsts =
		uHECapInfo.he_cap.midamble_tx_rx_max_nsts;
	pDot11f->he_4x_ltf_3200_gi_ndp =
		uHECapInfo.he_cap.he_4x_ltf_3200_gi_ndp;
	pDot11f->tb_ppdu_tx_stbc_lt_80mhz =
		uHECapInfo.he_cap.tb_ppdu_tx_stbc_lt_80mhz;
	pDot11f->rx_stbc_lt_80mhz = uHECapInfo.he_cap.rx_stbc_lt_80mhz;
	pDot11f->tb_ppdu_tx_stbc_gt_80mhz =
		uHECapInfo.he_cap.tb_ppdu_tx_stbc_gt_80mhz;
	pDot11f->rx_stbc_gt_80mhz = uHECapInfo.he_cap.rx_stbc_gt_80mhz;
	pDot11f->doppler = uHECapInfo.he_cap.doppler;
	pDot11f->ul_mu = uHECapInfo.he_cap.ul_mu;
	pDot11f->dcm_enc_tx = uHECapInfo.he_cap.dcm_enc_tx;
	pDot11f->dcm_enc_rx = uHECapInfo.he_cap.dcm_enc_rx;
	pDot11f->ul_he_mu = uHECapInfo.he_cap.ul_he_mu;
	pDot11f->su_beamformer = uHECapInfo.he_cap.su_beamformer;

	pDot11f->su_beamformee = uHECapInfo.he_cap.su_beamformee;
	pDot11f->mu_beamformer = uHECapInfo.he_cap.mu_beamformer;
	pDot11f->bfee_sts_lt_80 = uHECapInfo.he_cap.bfee_sts_lt_80;
	pDot11f->bfee_sts_gt_80 = uHECapInfo.he_cap.bfee_sts_gt_80;
	pDot11f->num_sounding_lt_80 = uHECapInfo.he_cap.num_sounding_lt_80;
	pDot11f->num_sounding_gt_80 = uHECapInfo.he_cap.num_sounding_gt_80;
	pDot11f->su_feedback_tone16 = uHECapInfo.he_cap.su_feedback_tone16;
	pDot11f->mu_feedback_tone16 = uHECapInfo.he_cap.mu_feedback_tone16;
	pDot11f->codebook_su = uHECapInfo.he_cap.codebook_su;
	pDot11f->codebook_mu = uHECapInfo.he_cap.codebook_mu;
	pDot11f->beamforming_feedback = uHECapInfo.he_cap.beamforming_feedback;
	pDot11f->he_er_su_ppdu = uHECapInfo.he_cap.he_er_su_ppdu;
	pDot11f->dl_mu_mimo_part_bw = uHECapInfo.he_cap.dl_mu_mimo_part_bw;
	pDot11f->ppet_present = uHECapInfo.he_cap.ppet_present;
	pDot11f->srp = uHECapInfo.he_cap.srp;
	pDot11f->power_boost = uHECapInfo.he_cap.power_boost;

	pDot11f->he_ltf_800_gi_4x = uHECapInfo.he_cap.he_ltf_800_gi_4x;
	pDot11f->max_nc = uHECapInfo.he_cap.max_nc;
	pDot11f->er_he_ltf_800_gi_4x = uHECapInfo.he_cap.er_he_ltf_800_gi_4x;
	pDot11f->he_ppdu_20_in_40Mhz_2G =
				uHECapInfo.he_cap.he_ppdu_20_in_40Mhz_2G;
	pDot11f->he_ppdu_20_in_160_80p80Mhz =
				uHECapInfo.he_cap.he_ppdu_20_in_160_80p80Mhz;
	pDot11f->he_ppdu_80_in_160_80p80Mhz =
				uHECapInfo.he_cap.he_ppdu_80_in_160_80p80Mhz;
	pDot11f->er_1x_he_ltf_gi =
				uHECapInfo.he_cap.er_1x_he_ltf_gi;
	pDot11f->midamble_tx_rx_1x_he_ltf =
				uHECapInfo.he_cap.midamble_tx_rx_1x_he_ltf;
	pDot11f->reserved2 = uHECapInfo.he_cap.reserved2;

	pDot11f->rx_he_mcs_map_lt_80 = uHECapInfo.he_cap.rx_he_mcs_map_lt_80;
	pDot11f->tx_he_mcs_map_lt_80 = uHECapInfo.he_cap.tx_he_mcs_map_lt_80;
	if (pDot11f->chan_width_2) {
		*((uint16_t *)pDot11f->rx_he_mcs_map_160) =
			uHECapInfo.he_cap.rx_he_mcs_map_160;
		*((uint16_t *)pDot11f->tx_he_mcs_map_160) =
			uHECapInfo.he_cap.tx_he_mcs_map_160;
	}
	if (pDot11f->chan_width_3) {
		*((uint16_t *)pDot11f->rx_he_mcs_map_80_80) =
			uHECapInfo.he_cap.rx_he_mcs_map_80_80;
		*((uint16_t *)pDot11f->tx_he_mcs_map_80_80) =
			uHECapInfo.he_cap.tx_he_mcs_map_80_80;
	}

	return QDF_STATUS_SUCCESS;
}

static void lim_tdls_check_and_force_he_ldpc_cap(struct pe_session *pe_session,
						 tDphHashNode *sta)
{
	if (pe_session && sta->he_config.present)
		lim_check_and_force_he_ldpc_cap(pe_session, &sta->he_config);
}

static void lim_tdls_update_node_he_caps(struct mac_context *mac,
					 struct tdls_add_sta_req *add_sta_req,
					 tDphHashNode *sta,
					 struct pe_session *pe_session)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pe_debug("Populating HE IEs");
	status = lim_tdls_populate_dot11f_he_caps(mac, add_sta_req,
						  &sta->he_config);

	if (status != QDF_STATUS_SUCCESS)
		return;

	if (sta->he_config.present)
		sta->mlmStaContext.he_capable = 1;

	if (pe_session && sta->he_config.present)
		lim_tdls_set_he_chan_width(&sta->he_config, pe_session);

	lim_log_he_cap(mac, &sta->he_config);

	if (lim_is_he_6ghz_band(pe_session)) {
		lim_tdls_populate_dot11f_6hgz_he_caps(mac, add_sta_req,
						      &sta->he_6g_band_cap);
		/*
		 * In 6Ghz, vht and ht ie may not present, peer channel width
		 * is populated while extracting HT and VHT cap itself. So,
		 * incase of 6ghz fill the chan_width.
		 */
		lim_update_stads_he_6ghz_op(pe_session, sta);
	}
}

#else
static void lim_tdls_fill_dis_rsp_he_cap(struct mac_context *mac,
					 uint32_t selfDot11Mode,
					 tDot11fTDLSDisRsp *tdls_dis_rsp,
					 struct pe_session *pe_session)
{
}

static void lim_tdls_fill_setup_req_he_cap(struct mac_context *mac,
					   uint32_t selfDot11Mode,
					   tDot11fTDLSSetupReq *tdls_setup_req,
					   struct pe_session *pe_session)
{
}

static void lim_tdls_fill_setup_rsp_he_cap(struct mac_context *mac,
					   uint32_t selfDot11Mode,
					   tDot11fTDLSSetupRsp *tdls_setup_rsp,
					   struct pe_session *pe_session)
{
}

static void lim_tdls_fill_setup_cnf_he_op(struct mac_context *mac,
					  uint32_t peer_capability,
					  tDot11fTDLSSetupCnf *tdls_setup_cnf,
					  struct pe_session *pe_session)
{
}

static void lim_tdls_populate_he_matching_rate_set(struct mac_context *mac_ctx,
						   tpDphHashNode stads,
						   uint8_t nss,
						   struct pe_session *session)
{
}

static void lim_tdls_update_node_he_caps(struct mac_context *mac,
					 struct tdls_add_sta_req *add_sta_req,
					 tDphHashNode *sta,
					 struct pe_session *pe_session)
{
}

static void lim_tdls_check_and_force_he_ldpc_cap(struct pe_session *pe_session,
						 tDphHashNode *sta)
{
}

#endif

/*
 * Send TDLS discovery response frame on direct link.
 */

static QDF_STATUS lim_send_tdls_dis_rsp_frame(struct mac_context *mac,
					      struct qdf_mac_addr peer_mac,
					      uint8_t dialog,
					      struct pe_session *pe_session,
					      uint8_t *addIe,
					      uint16_t addIeLen)
{
	tDot11fTDLSDisRsp *tdls_dis_rsp;
	uint16_t caps = 0;
	uint32_t status = 0;
	uint32_t nPayload = 0;
	uint32_t nBytes = 0;
	uint8_t *pFrame;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint32_t selfDot11Mode;
/*  Placeholder to support different channel bonding mode of TDLS than AP. */
/*  Today, WNI_CFG_CHANNEL_BONDING_MODE will be overwritten when connecting to AP */
/*  To support this feature, we need to introduce WNI_CFG_TDLS_CHANNEL_BONDING_MODE */
/*  As of now, we hardcoded to max channel bonding of dot11Mode (i.e HT80 for 11ac/HT40 for 11n) */
/*  uint32_t tdlsChannelBondingMode; */
	uint8_t smeSessionId = 0;

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	tdls_dis_rsp = qdf_mem_malloc(sizeof(*tdls_dis_rsp));
	if (!tdls_dis_rsp)
		return QDF_STATUS_E_NOMEM;

	smeSessionId = pe_session->smeSessionId;

	/*
	 * The scheme here is to fill out a 'tDot11fProbeRequest' structure
	 * and then hand it off to 'dot11f_pack_probe_request' (for
	 * serialization).
	 */

	/*
	 * setup Fixed fields,
	 */
	tdls_dis_rsp->Category.category = ACTION_CATEGORY_PUBLIC;
	tdls_dis_rsp->Action.action = TDLS_DISCOVERY_RESPONSE;
	tdls_dis_rsp->DialogToken.token = dialog;

	populate_dot11f_link_iden(mac, pe_session,
				  &tdls_dis_rsp->LinkIdentifier,
				  peer_mac, TDLS_RESPONDER);

	if (lim_get_capability_info(mac, &caps, pe_session) !=
	    QDF_STATUS_SUCCESS) {
		/*
		 * Could not get Capabilities value
		 * from CFG. Log error.
		 */
		pe_err("could not retrieve Capabilities value");
	}
	swap_bit_field16(caps, (uint16_t *)&tdls_dis_rsp->Capabilities);

	/* populate supported rate and ext supported rate IE */
	if (QDF_STATUS_E_FAILURE == populate_dot11f_rates_tdls(mac,
					&tdls_dis_rsp->SuppRates,
					&tdls_dis_rsp->ExtSuppRates,
					wlan_reg_freq_to_chan(
					mac->pdev, pe_session->curr_op_freq)))
		pe_err("could not populate supported data rates");

	/* populate extended capability IE */
	populate_dot11f_tdls_ext_capability(mac,
					    pe_session,
					    &tdls_dis_rsp->ExtCap);

	selfDot11Mode = mac->mlme_cfg->dot11_mode.dot11_mode;

	/* Populate HT/VHT Capabilities */
	populate_dot11f_tdls_ht_vht_cap(mac, selfDot11Mode,
					&tdls_dis_rsp->HTCaps,
					&tdls_dis_rsp->VHTCaps, pe_session);

	lim_tdls_fill_dis_rsp_he_cap(mac, selfDot11Mode, tdls_dis_rsp,
				     pe_session);

	/* Populate TDLS offchannel param only if offchannel is enabled
	 * and TDLS Channel Switching is not prohibited by AP in ExtCap
	 * IE in assoc/re-assoc response.
	 */
	if ((1 == mac->lim.gLimTDLSOffChannelEnabled) &&
	    (!pe_session->tdls_chan_swit_prohibited)) {
		populate_dot11f_tdls_offchannel_params(mac, pe_session,
					&tdls_dis_rsp->SuppChannels,
					&tdls_dis_rsp->SuppOperatingClasses);
		if (mac->mlme_cfg->gen.band_capability != BIT(REG_BAND_2G)) {
			tdls_dis_rsp->ht2040_bss_coexistence.present = 1;
			tdls_dis_rsp->ht2040_bss_coexistence.info_request = 1;
		}
	} else {
		pe_debug("TDLS offchan not enabled, or channel switch prohibited by AP, gLimTDLSOffChannelEnabled: %d tdls_chan_swit_prohibited: %d",
			mac->lim.gLimTDLSOffChannelEnabled,
			pe_session->tdls_chan_swit_prohibited);
	}
	/*
	 * now we pack it.  First, how much space are we going to need?
	 */
	status = dot11f_get_packed_tdls_dis_rsp_size(mac, tdls_dis_rsp,
						     &nPayload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to calculate the packed size for a Discovery Response (0x%08x)",
			status);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fProbeRequest);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while calculating the packed size for a Discovery Response (0x%08x)",
			status);
	}

	/*
	 * This frame is going out from PE as data frames with special ethertype
	 * 89-0d.
	 * 8 bytes of RFC 1042 header
	 */

	nBytes = nPayload + sizeof(tSirMacMgmtHdr) + addIeLen;

	/* Ok-- try to allocate memory from MGMT PKT pool */
	qdf_status = cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				(void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a TDLS Discovery Request",
			nBytes);
		qdf_mem_free(tdls_dis_rsp);
		return QDF_STATUS_E_NOMEM;
	}

	/* zero out the memory */
	qdf_mem_zero(pFrame, nBytes);

	/*
	 * IE formation, memory allocation is completed, Now form TDLS discovery
	 * response frame
	 */

	/* Make public Action Frame */

	lim_populate_mac_header(mac, pFrame, SIR_MAC_MGMT_FRAME,
				SIR_MAC_MGMT_ACTION, peer_mac.bytes,
				pe_session->self_mac_addr);

	{
		tpSirMacMgmtHdr pMacHdr;

		pMacHdr = (tpSirMacMgmtHdr) pFrame;
		pMacHdr->fc.toDS = ANI_TXDIR_IBSS;
		pMacHdr->fc.powerMgmt = 0;
		sir_copy_mac_addr(pMacHdr->bssId, pe_session->bssId);
	}

	status = dot11f_pack_tdls_dis_rsp(mac, tdls_dis_rsp, pFrame +
					  sizeof(tSirMacMgmtHdr),
					  nPayload, &nPayload);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack a TDLS discovery response (0x%08x)",
			status);
		cds_packet_free((void *)pPacket);
		qdf_mem_free(tdls_dis_rsp);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing TDLS Discovery Response (0x%08x)",
			status);
	}

	qdf_mem_free(tdls_dis_rsp);

	if (0 != addIeLen) {
		pe_debug("Copy Additional Ie Len: %d", addIeLen);
		qdf_mem_copy(pFrame + sizeof(tSirMacMgmtHdr) + nPayload, addIe,
			     addIeLen);
	}
	pe_debug("[TDLS] action: %d (%s) -DIRECT-> OTA peer="QDF_MAC_ADDR_FMT,
		TDLS_DISCOVERY_RESPONSE,
		lim_trace_tdls_action_string(TDLS_DISCOVERY_RESPONSE),
		QDF_MAC_ADDR_REF(peer_mac.bytes));

	mac->lim.tdls_frm_session_id = pe_session->smeSessionId;
	lim_diag_mgmt_tx_event_report(mac, (tpSirMacMgmtHdr) pFrame,
				      pe_session, QDF_STATUS_SUCCESS,
				      QDF_STATUS_SUCCESS);
	/*
	 * Transmit Discovery response and watch if this is delivered to
	 * peer STA.
	 */
	/* In CLD 2.0, pass Discovery Response as mgmt frame so that
	 * wma does not do header conversion to 802.3 before calling tx/rx
	 * routine and subsequenly target also sends frame as is OTA
	 */
	qdf_status = wma_tx_frameWithTxComplete(mac, pPacket, (uint16_t) nBytes,
					      TXRX_FRM_802_11_MGMT,
					      ANI_TXDIR_IBSS,
					      0,
					      lim_tx_complete, pFrame,
					      lim_mgmt_tdls_tx_complete,
					      HAL_USE_SELF_STA_REQUESTED_MASK,
					      smeSessionId, false, 0,
					      RATEID_DEFAULT, 0);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		mac->lim.tdls_frm_session_id = NO_SESSION;
		pe_err("could not send TDLS Discovery Response frame!");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;

}

/*
 * This static function is currently used by lim_send_tdls_link_setup_req_frame and
 * lim_send_tdls_setup_rsp_frame to populate the AID if device is 11ac capable.
 */
static void populate_dotf_tdls_vht_aid(struct mac_context *mac, uint32_t selfDot11Mode,
				       struct qdf_mac_addr peerMac,
				       tDot11fIEAID *Aid,
				       struct pe_session *pe_session)
{
	if (((wlan_reg_freq_to_chan(mac->pdev, pe_session->curr_op_freq) <=
		SIR_11B_CHANNEL_END) &&
	     mac->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band) ||
	    (wlan_reg_freq_to_chan(mac->pdev, pe_session->curr_op_freq) >=
		SIR_11B_CHANNEL_END)) {
		if (IS_DOT11_MODE_VHT(selfDot11Mode) &&
		    IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {

			uint16_t aid;
			tpDphHashNode sta;

			sta =
				dph_lookup_hash_entry(mac, peerMac.bytes, &aid,
						      &pe_session->dph.
						      dphHashTable);
			if (sta) {
				Aid->present = 1;
				Aid->assocId = aid | LIM_AID_MASK;      /* set bit 14 and 15 1's */
			} else {
				Aid->present = 0;
				pe_err("sta is NULL for "
					   QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(peerMac.bytes));
			}
		}
	} else {
		Aid->present = 0;
		pe_warn("Vht not enable from ini for 2.4GHz");
	}
}

#ifdef CONFIG_HL_SUPPORT

/**
 * wma_tx_frame_with_tx_complete_send() - Send tx frames on Direct link or AP link
 *				       depend on reason code
 * @mac: pointer to MAC Sirius parameter structure
 * @pPacket: pointer to mgmt packet
 * @nBytes: number of bytes to send
 * @tid:tid value for AC
 * @pFrame: pointer to tdls frame
 * @smeSessionId:session id
 * @flag: tdls flag
 *
 * Send TDLS Teardown frame on Direct link or AP link, depends on reason code.
 *
 * Return: None
 */
static inline QDF_STATUS
wma_tx_frame_with_tx_complete_send(struct mac_context *mac, void *pPacket,
				uint16_t nBytes,
				uint8_t tid,
				uint8_t *pFrame,
				uint8_t smeSessionId, bool flag)
{
	return wma_tx_frameWithTxComplete(mac, pPacket,
					  (uint16_t) nBytes,
					  TXRX_FRM_802_11_DATA,
					  ANI_TXDIR_TODS,
					  tid,
					  lim_tx_complete, pFrame,
					  lim_mgmt_tdls_tx_complete,
					  HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME
					  | HAL_USE_PEER_STA_REQUESTED_MASK,
					  smeSessionId, flag, 0,
					  RATEID_DEFAULT, 0);
}
#else

static inline QDF_STATUS
wma_tx_frame_with_tx_complete_send(struct mac_context *mac, void *pPacket,
				uint16_t nBytes,
				uint8_t tid,
				uint8_t *pFrame,
				uint8_t smeSessionId, bool flag)
{
	return wma_tx_frameWithTxComplete(mac, pPacket,
					  (uint16_t) nBytes,
					  TXRX_FRM_802_11_DATA,
					  ANI_TXDIR_TODS,
					  tid,
					  lim_tx_complete, pFrame,
					  lim_mgmt_tdls_tx_complete,
					  HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME
					  | HAL_USE_PEER_STA_REQUESTED_MASK,
					  smeSessionId, false, 0,
					  RATEID_DEFAULT, 0);
}
#endif

void lim_set_tdls_flags(struct roam_offload_synch_ind *roam_sync_ind_ptr,
		   struct pe_session *ft_session_ptr)
{
	roam_sync_ind_ptr->join_rsp->tdls_prohibited =
		ft_session_ptr->tdls_prohibited;
	roam_sync_ind_ptr->join_rsp->tdls_chan_swit_prohibited =
		ft_session_ptr->tdls_chan_swit_prohibited;
}

/*
 * TDLS setup Request frame on AP link
 */
static
QDF_STATUS lim_send_tdls_link_setup_req_frame(struct mac_context *mac,
					      struct qdf_mac_addr peer_mac,
					      uint8_t dialog,
					      struct pe_session *pe_session,
					      uint8_t *addIe,
					      uint16_t addIeLen,
					      enum wifi_traffic_ac ac)
{
	tDot11fTDLSSetupReq *tdls_setup_req;
	uint16_t caps = 0;
	uint32_t status = 0;
	uint32_t payload = 0;
	uint32_t nbytes = 0;
	uint32_t header_offset = 0;
	uint8_t *frame;
	void *packet;
	QDF_STATUS qdf_status;
	uint32_t selfDot11Mode;
	uint8_t smeSessionId = 0;
	uint8_t sp_length = 0;

/*  Placeholder to support different channel bonding mode of TDLS than AP. */
/*  Today, WNI_CFG_CHANNEL_BONDING_MODE will be overwritten when connecting to AP */
/*  To support this feature, we need to introduce WNI_CFG_TDLS_CHANNEL_BONDING_MODE */
/*  As of now, we hardcoded to max channel bonding of dot11Mode (i.e HT80 for 11ac/HT40 for 11n) */
/*  uint32_t tdlsChannelBondingMode; */

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	tdls_setup_req = qdf_mem_malloc(sizeof(*tdls_setup_req));
	if (!tdls_setup_req)
		return QDF_STATUS_E_NOMEM;

	/*
	 * The scheme here is to fill out a 'tDot11fProbeRequest' structure
	 * and then hand it off to 'dot11f_pack_probe_request' (for
	 * serialization).
	 */
	smeSessionId = pe_session->smeSessionId;

	tdls_setup_req->Category.category = ACTION_CATEGORY_TDLS;
	tdls_setup_req->Action.action = TDLS_SETUP_REQUEST;
	tdls_setup_req->DialogToken.token = dialog;

	populate_dot11f_link_iden(mac, pe_session,
				  &tdls_setup_req->LinkIdentifier, peer_mac,
				  TDLS_INITIATOR);

	if (lim_get_capability_info(mac, &caps, pe_session) !=
	    QDF_STATUS_SUCCESS) {
		/*
		 * Could not get Capabilities value
		 * from CFG. Log error.
		 */
		pe_err("could not retrieve Capabilities value");
	}
	swap_bit_field16(caps, (uint16_t *)&tdls_setup_req->Capabilities);

	/* populate supported rate and ext supported rate IE */
	if (QDF_STATUS_E_FAILURE == populate_dot11f_rates_tdls(mac,
					&tdls_setup_req->SuppRates,
					&tdls_setup_req->ExtSuppRates,
					wlan_reg_freq_to_chan(
					mac->pdev, pe_session->curr_op_freq)))
		pe_err("could not populate supported data rates");

	/* Populate extended capability IE */
	populate_dot11f_tdls_ext_capability(mac,
					    pe_session,
					    &tdls_setup_req->ExtCap);

	if (1 == mac->lim.gLimTDLSWmmMode) {

		pe_debug("populate WMM IE in Setup Request Frame");
		sp_length = mac->mlme_cfg->wmm_params.max_sp_length;
		/* include WMM IE */
		tdls_setup_req->WMMInfoStation.version = SIR_MAC_OUI_VERSION_1;
		tdls_setup_req->WMMInfoStation.acvo_uapsd =
			(mac->lim.gLimTDLSUapsdMask & 0x01);
		tdls_setup_req->WMMInfoStation.acvi_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x02) >> 1);
		tdls_setup_req->WMMInfoStation.acbk_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x04) >> 2);
		tdls_setup_req->WMMInfoStation.acbe_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x08) >> 3);
		tdls_setup_req->WMMInfoStation.max_sp_length = sp_length;
		tdls_setup_req->WMMInfoStation.present = 1;
	} else {
		/*
		 * TODO: we need to see if we have to support conditions where
		 * we have EDCA parameter info element is needed a) if we need
		 * different QOS parameters for off channel operations or QOS
		 * is not supported on AP link and we wanted to QOS on direct
		 * link.
		 */

		/* Populate QOS info, needed for Peer U-APSD session */

		/*
		 * TODO: Now hardcoded, since populate_dot11f_qos_caps_station()
		 * depends on AP's capability, and TDLS doesn't want to depend
		 * on AP's capability
		 */

		pe_debug("populate QOS IE in Setup Request Frame");
		tdls_setup_req->QOSCapsStation.present = 1;
		tdls_setup_req->QOSCapsStation.max_sp_length = 0;
		tdls_setup_req->QOSCapsStation.qack = 0;
		tdls_setup_req->QOSCapsStation.acbe_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x08) >> 3);
		tdls_setup_req->QOSCapsStation.acbk_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x04) >> 2);
		tdls_setup_req->QOSCapsStation.acvi_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x02) >> 1);
		tdls_setup_req->QOSCapsStation.acvo_uapsd =
			(mac->lim.gLimTDLSUapsdMask & 0x01);
	}

	/*
	 * we will always try to init TDLS link with 11n capabilities
	 * let TDLS setup response to come, and we will set our caps based
	 * of peer caps
	 */

	selfDot11Mode =  mac->mlme_cfg->dot11_mode.dot11_mode;

	/* Populate HT/VHT Capabilities */
	populate_dot11f_tdls_ht_vht_cap(mac, selfDot11Mode,
					&tdls_setup_req->HTCaps,
					&tdls_setup_req->VHTCaps, pe_session);
	lim_tdls_fill_setup_req_he_cap(mac, selfDot11Mode, tdls_setup_req,
				       pe_session);

	/* Populate AID */
	populate_dotf_tdls_vht_aid(mac, selfDot11Mode, peer_mac,
				   &tdls_setup_req->AID, pe_session);

	/* Populate TDLS offchannel param only if offchannel is enabled
	 * and TDLS Channel Switching is not prohibited by AP in ExtCap
	 * IE in assoc/re-assoc response.
	 */
	if ((1 == mac->lim.gLimTDLSOffChannelEnabled) &&
	    (!pe_session->tdls_chan_swit_prohibited)) {
		populate_dot11f_tdls_offchannel_params(mac, pe_session,
					&tdls_setup_req->SuppChannels,
					&tdls_setup_req->SuppOperatingClasses);
		if (mac->mlme_cfg->gen.band_capability != BIT(REG_BAND_2G)) {
			tdls_setup_req->ht2040_bss_coexistence.present = 1;
			tdls_setup_req->ht2040_bss_coexistence.info_request = 1;
		}
	} else {
		pe_debug("TDLS offchan not enabled, or channel switch prohibited by AP, gLimTDLSOffChannelEnabled: %d tdls_chan_swit_prohibited: %d",
			mac->lim.gLimTDLSOffChannelEnabled,
			pe_session->tdls_chan_swit_prohibited);
	}
	/*
	 * now we pack it.  First, how much space are we going to need?
	 */
	status = dot11f_get_packed_tdls_setup_req_size(mac, tdls_setup_req,
						       &payload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to calculate the packed size for a Setup Request (0x%08x)",
			status);
		/* We'll fall back on the worst case scenario: */
		payload = sizeof(tDot11fProbeRequest);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while calculating the packed size for a Setup Request (0x%08x)",
			status);
	}

	/*
	 * This frame is going out from PE as data frames with special ethertype
	 * 89-0d.
	 * 8 bytes of RFC 1042 header
	 */

	nbytes = payload + ((IS_QOS_ENABLED(pe_session))
			     ? sizeof(tSirMacDataHdr3a) :
			     sizeof(tSirMacMgmtHdr))
		 + sizeof(eth_890d_header)
		 + PAYLOAD_TYPE_TDLS_SIZE + addIeLen;

	/* Ok-- try to allocate memory from MGMT PKT pool */
	qdf_status = cds_packet_alloc((uint16_t)nbytes, (void **)&frame,
				      (void **)&packet);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a TDLS Setup Request",
			nbytes);
		qdf_mem_free(tdls_setup_req);
		return QDF_STATUS_E_NOMEM;
	}

	/* zero out the memory */
	qdf_mem_zero(frame, nbytes);

	/*
	 * IE formation, memory allocation is completed, Now form TDLS discovery
	 * request frame
	 */

	/* fill out the buffer descriptor */

	header_offset = lim_prepare_tdls_frame_header(mac, frame,
				&tdls_setup_req->LinkIdentifier,
				TDLS_LINK_AP, TDLS_INITIATOR,
				(ac == WIFI_AC_VI) ? TID_AC_VI : TID_AC_BK,
				pe_session);

	pe_debug("SupportedChnlWidth: %x rxMCSMap: %x rxMCSMap: %x txSupDataRate: %x",
		tdls_setup_req->VHTCaps.supportedChannelWidthSet,
		tdls_setup_req->VHTCaps.rxMCSMap,
		tdls_setup_req->VHTCaps.txMCSMap,
		tdls_setup_req->VHTCaps.txSupDataRate);

	status = dot11f_pack_tdls_setup_req(mac, tdls_setup_req,
					    frame + header_offset,
					    payload, &payload);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack a TDLS Setup request (0x%08x)",
			status);
		cds_packet_free((void *)packet);
		qdf_mem_free(tdls_setup_req);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing TDLS Setup Request (0x%08x)",
			status);
	}

	qdf_mem_free(tdls_setup_req);

	/* Copy the additional IE. */
	/* TODO : addIe is added at the end of the frame. This means it doesn't */
	/* follow the order. This should be ok, but we should consider changing this */
	/* if there is any IOT issue. */
	if (addIeLen != 0) {
		pe_debug("Copy Additional Ie Len = %d", addIeLen);
		qdf_mem_copy(frame + header_offset + payload, addIe,
			     addIeLen);
	}

	pe_debug("[TDLS] action: %d (%s) -AP-> OTA peer="QDF_MAC_ADDR_FMT,
		TDLS_SETUP_REQUEST,
		lim_trace_tdls_action_string(TDLS_SETUP_REQUEST),
		QDF_MAC_ADDR_REF(peer_mac.bytes));

	mac->lim.tdls_frm_session_id = pe_session->smeSessionId;
	lim_diag_mgmt_tx_event_report(mac, (tpSirMacMgmtHdr) frame,
				      pe_session, QDF_STATUS_SUCCESS,
				      QDF_STATUS_SUCCESS);

	qdf_status = wma_tx_frame_with_tx_complete_send(mac, packet,
							(uint16_t)nbytes,
							TID_AC_VI,
							frame,
							smeSessionId, true);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		mac->lim.tdls_frm_session_id = NO_SESSION;
		pe_err("could not send TDLS Setup Request frame!");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;

}

/*
 * Send TDLS Teardown frame on Direct link or AP link, depends on reason code.
 */
static
QDF_STATUS lim_send_tdls_teardown_frame(struct mac_context *mac,
					   struct qdf_mac_addr peer_mac,
					   uint16_t reason,
					   uint8_t responder,
					   struct pe_session *pe_session,
					   uint8_t *addIe, uint16_t addIeLen,
					   enum wifi_traffic_ac ac)
{
	tDot11fTDLSTeardown teardown;
	uint32_t status = 0;
	uint32_t nPayload = 0;
	uint32_t nBytes = 0;
	uint32_t header_offset = 0;
	uint8_t *pFrame;
	void *pPacket;
	QDF_STATUS qdf_status;
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
	uint32_t padLen = 0;
#endif
	uint8_t smeSessionId = 0;
	tpDphHashNode sta_ds;
	uint16_t aid = 0;
	uint8_t qos_mode = 0;
	uint8_t tdls_link_type;

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	smeSessionId = pe_session->smeSessionId;
	/*
	 * The scheme here is to fill out a 'tDot11fProbeRequest' structure
	 * and then hand it off to 'dot11f_pack_probe_request' (for
	 * serialization).  We start by zero-initializing the structure:
	 */
	qdf_mem_zero((uint8_t *) &teardown, sizeof(tDot11fTDLSTeardown));
	teardown.Category.category = ACTION_CATEGORY_TDLS;
	teardown.Action.action = TDLS_TEARDOWN;
	teardown.Reason.code = reason;

	populate_dot11f_link_iden(mac, pe_session, &teardown.LinkIdentifier,
				  peer_mac,
				  (responder ==
				   true) ? TDLS_RESPONDER : TDLS_INITIATOR);

	/*
	 * now we pack it.  First, how much space are we going to need?
	 */
	status = dot11f_get_packed_tdls_teardown_size(mac, &teardown, &nPayload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to calculate the packed size for a discovery Request (0x%08x)",
			status);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fProbeRequest);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while calculating the packed size for a discovery Request (0x%08x)",
			status);
	}

	/*
	 * This frame is going out from PE as data frames with special ethertype
	 * 89-0d.
	 * 8 bytes of RFC 1042 header
	 */
	sta_ds = dph_lookup_hash_entry(mac, pe_session->bssId, &aid,
					&pe_session->dph.dphHashTable);
	if (sta_ds)
		qos_mode = sta_ds->qosMode;
	tdls_link_type = (reason == REASON_TDLS_PEER_UNREACHABLE)
				? TDLS_LINK_AP : TDLS_LINK_DIRECT;
	nBytes = nPayload + (((IS_QOS_ENABLED(pe_session) &&
			     (tdls_link_type == TDLS_LINK_AP)) ||
			     ((tdls_link_type == TDLS_LINK_DIRECT) && qos_mode))
			     ? sizeof(tSirMacDataHdr3a) :
			     sizeof(tSirMacMgmtHdr))
		 + sizeof(eth_890d_header)
		 + PAYLOAD_TYPE_TDLS_SIZE + addIeLen;

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
	/* IOT issue with some AP : some AP doesn't like the data packet size < minimum 802.3 frame length (64)
	   Hence AP itself padding some bytes, which caused teardown packet is dropped at
	   receiver side. To avoid such IOT issue, we added some extra bytes to meet data frame size >= 64
	 */
	if (nPayload + PAYLOAD_TYPE_TDLS_SIZE < MIN_IEEE_8023_SIZE) {
		padLen =
			MIN_IEEE_8023_SIZE - (nPayload + PAYLOAD_TYPE_TDLS_SIZE);

		/* if padLen is less than minimum vendorSpecific (5), pad up to 5 */
		if (padLen < MIN_VENDOR_SPECIFIC_IE_SIZE)
			padLen = MIN_VENDOR_SPECIFIC_IE_SIZE;

		nBytes += padLen;
	}
#endif

	/* Ok-- try to allocate memory from MGMT PKT pool */
	qdf_status = cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
			(void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a TDLS Teardown Frame.",
			nBytes);
		return QDF_STATUS_E_NOMEM;
	}

	/* zero out the memory */
	qdf_mem_zero(pFrame, nBytes);

	/*
	 * IE formation, memory allocation is completed, Now form TDLS discovery
	 * request frame
	 */

	/* fill out the buffer descriptor */
	pe_debug("Reason of TDLS Teardown: %d", reason);
	header_offset = lim_prepare_tdls_frame_header(mac, pFrame,
			LINK_IDEN_ADDR_OFFSET(teardown),
			(reason == REASON_TDLS_PEER_UNREACHABLE) ?
			TDLS_LINK_AP : TDLS_LINK_DIRECT,
			(responder == true) ? TDLS_RESPONDER : TDLS_INITIATOR,
			(ac == WIFI_AC_VI) ? TID_AC_VI : TID_AC_BK,
			pe_session);

	status = dot11f_pack_tdls_teardown(mac, &teardown, pFrame
					   + header_offset, nPayload, &nPayload);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack a TDLS Teardown frame (0x%08x)",
			status);
		cds_packet_free((void *)pPacket);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing TDLS Teardown frame (0x%08x)",
			status);
	}

	if (addIeLen != 0) {
		pe_debug("Copy Additional Ie Len = %d", addIeLen);
		qdf_mem_copy(pFrame + header_offset + nPayload, addIe,
			     addIeLen);
	}
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
	if (padLen != 0) {
		/* QCOM VENDOR OUI = { 0x00, 0xA0, 0xC6, type = 0x0000 }; */
		uint8_t *padVendorSpecific =
			pFrame + header_offset + nPayload + addIeLen;
		/* make QCOM_VENDOR_OUI, and type = 0x0000, and all the payload to be zero */
		padVendorSpecific[0] = 221;
		padVendorSpecific[1] = padLen - 2;
		padVendorSpecific[2] = 0x00;
		padVendorSpecific[3] = 0xA0;
		padVendorSpecific[4] = 0xC6;

		pe_debug("Padding Vendor Specific Ie Len = %d", padLen);

		/* padding zero if more than 5 bytes are required */
		if (padLen > MIN_VENDOR_SPECIFIC_IE_SIZE)
			qdf_mem_zero(pFrame + header_offset + nPayload +
				    addIeLen + MIN_VENDOR_SPECIFIC_IE_SIZE,
				    padLen - MIN_VENDOR_SPECIFIC_IE_SIZE);
	}
#endif
	pe_debug("[TDLS] action: %d (%s) -%s-> OTA peer="QDF_MAC_ADDR_FMT,
		TDLS_TEARDOWN,
		lim_trace_tdls_action_string(TDLS_TEARDOWN),
		((reason == REASON_TDLS_PEER_UNREACHABLE) ? "AP" :
		    "DIRECT"),
		QDF_MAC_ADDR_REF(peer_mac.bytes));

	mac->lim.tdls_frm_session_id = pe_session->smeSessionId;
	lim_diag_mgmt_tx_event_report(mac, (tpSirMacMgmtHdr) pFrame,
				      pe_session, QDF_STATUS_SUCCESS,
				      QDF_STATUS_SUCCESS);

	qdf_status = wma_tx_frame_with_tx_complete_send(mac, pPacket,
						     (uint16_t) nBytes,
						     TID_AC_VI,
						     pFrame,
						     smeSessionId,
						     (reason == REASON_TDLS_PEER_UNREACHABLE)
						     ? true : false);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		mac->lim.tdls_frm_session_id = NO_SESSION;
		pe_err("could not send TDLS Teardown frame");
		return QDF_STATUS_E_FAILURE;

	}

	return QDF_STATUS_SUCCESS;
}

/*
 * Send Setup RSP frame on AP link.
 */
static QDF_STATUS
lim_send_tdls_setup_rsp_frame(struct mac_context *mac,
			      struct qdf_mac_addr peer_mac,
			      uint8_t dialog,
			      struct pe_session *pe_session,
			      etdlsLinkSetupStatus setupStatus,
			      uint8_t *addIe,
			      uint16_t addIeLen,
			      enum wifi_traffic_ac ac)
{
	tDot11fTDLSSetupRsp *setup_rsp;
	uint32_t status = 0;
	uint16_t caps = 0;
	uint32_t nPayload = 0;
	uint32_t header_offset = 0;
	uint32_t nBytes = 0;
	uint8_t *pFrame;
	void *pPacket;
	QDF_STATUS qdf_status;
	uint32_t selfDot11Mode;
	uint8_t max_sp_length = 0;
/*  Placeholder to support different channel bonding mode of TDLS than AP. */
/*  Today, WNI_CFG_CHANNEL_BONDING_MODE will be overwritten when connecting to AP */
/*  To support this feature, we need to introduce WNI_CFG_TDLS_CHANNEL_BONDING_MODE */
/*  As of now, we hardcoded to max channel bonding of dot11Mode (i.e HT80 for 11ac/HT40 for 11n) */
/*  uint32_t tdlsChannelBondingMode; */
	uint8_t smeSessionId = 0;

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	setup_rsp = qdf_mem_malloc(sizeof(*setup_rsp));
	if (!setup_rsp)
		return QDF_STATUS_E_NOMEM;

	smeSessionId = pe_session->smeSessionId;

	/*
	 * The scheme here is to fill out a 'tDot11fProbeRequest' structure
	 * and then hand it off to 'dot11f_pack_probe_request' (for
	 * serialization).
	 */

	/*
	 * setup Fixed fields,
	 */
	setup_rsp->Category.category = ACTION_CATEGORY_TDLS;
	setup_rsp->Action.action = TDLS_SETUP_RESPONSE;
	setup_rsp->DialogToken.token = dialog;

	populate_dot11f_link_iden(mac, pe_session,
				  &setup_rsp->LinkIdentifier, peer_mac,
				  TDLS_RESPONDER);

	if (lim_get_capability_info(mac, &caps, pe_session) !=
	    QDF_STATUS_SUCCESS) {
		/*
		 * Could not get Capabilities value
		 * from CFG. Log error.
		 */
		pe_err("could not retrieve Capabilities value");
	}
	swap_bit_field16(caps, (uint16_t *)&setup_rsp->Capabilities);

	/* populate supported rate and ext supported rate IE */
	if (QDF_STATUS_E_FAILURE == populate_dot11f_rates_tdls(mac,
					&setup_rsp->SuppRates,
					&setup_rsp->ExtSuppRates,
					wlan_reg_freq_to_chan(
					mac->pdev, pe_session->curr_op_freq)))
		pe_err("could not populate supported data rates");

	/* Populate extended capability IE */
	populate_dot11f_tdls_ext_capability(mac,
					    pe_session,
					    &setup_rsp->ExtCap);

	if (1 == mac->lim.gLimTDLSWmmMode) {

		pe_debug("populate WMM IE in Setup Response frame");
		max_sp_length = mac->mlme_cfg->wmm_params.max_sp_length;
		/* include WMM IE */
		setup_rsp->WMMInfoStation.version = SIR_MAC_OUI_VERSION_1;
		setup_rsp->WMMInfoStation.acvo_uapsd =
			(mac->lim.gLimTDLSUapsdMask & 0x01);
		setup_rsp->WMMInfoStation.acvi_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x02) >> 1);
		setup_rsp->WMMInfoStation.acbk_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x04) >> 2);
		setup_rsp->WMMInfoStation.acbe_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x08) >> 3);
		setup_rsp->WMMInfoStation.max_sp_length = max_sp_length;
		setup_rsp->WMMInfoStation.present = 1;
	} else {
		/*
		 * TODO: we need to see if we have to support conditions where
		 * we have EDCA parameter info element is needed a) if we need
		 * different QOS parameters for off channel operations or QOS
		 * is not supported on AP link and we wanted to QOS on direct
		 * link.
		 */
		/* Populate QOS info, needed for Peer U-APSD session */
		/*
		 * TODO: Now hardcoded, because
		 * populate_dot11f_qos_caps_station() depends on AP's
		 * capability, and TDLS doesn't want to depend on AP's
		 * capability
		 */
		pe_debug("populate QOS IE in Setup Response frame");
		setup_rsp->QOSCapsStation.present = 1;
		setup_rsp->QOSCapsStation.max_sp_length = 0;
		setup_rsp->QOSCapsStation.qack = 0;
		setup_rsp->QOSCapsStation.acbe_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x08) >> 3);
		setup_rsp->QOSCapsStation.acbk_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x04) >> 2);
		setup_rsp->QOSCapsStation.acvi_uapsd =
			((mac->lim.gLimTDLSUapsdMask & 0x02) >> 1);
		setup_rsp->QOSCapsStation.acvo_uapsd =
			(mac->lim.gLimTDLSUapsdMask & 0x01);
	}

	selfDot11Mode = mac->mlme_cfg->dot11_mode.dot11_mode;

	/* Populate HT/VHT Capabilities */
	populate_dot11f_tdls_ht_vht_cap(mac, selfDot11Mode,
					&setup_rsp->HTCaps,
					&setup_rsp->VHTCaps, pe_session);
	lim_tdls_fill_setup_rsp_he_cap(mac, selfDot11Mode, setup_rsp,
				       pe_session);

	/* Populate AID */
	populate_dotf_tdls_vht_aid(mac, selfDot11Mode, peer_mac,
				   &setup_rsp->AID, pe_session);

	/* Populate TDLS offchannel param only if offchannel is enabled
	 * and TDLS Channel Switching is not prohibited by AP in ExtCap
	 * IE in assoc/re-assoc response.
	 */
	if ((1 == mac->lim.gLimTDLSOffChannelEnabled) &&
	    (!pe_session->tdls_chan_swit_prohibited)) {
		populate_dot11f_tdls_offchannel_params(mac, pe_session,
						    &setup_rsp->SuppChannels,
						    &setup_rsp->
						    SuppOperatingClasses);
		if (mac->mlme_cfg->gen.band_capability != BIT(REG_BAND_2G)) {
			setup_rsp->ht2040_bss_coexistence.present = 1;
			setup_rsp->ht2040_bss_coexistence.info_request = 1;
		}
	} else {
		pe_debug("TDLS offchan not enabled, or channel switch prohibited by AP, gLimTDLSOffChannelEnabled: %d tdls_chan_swit_prohibited: %d",
			mac->lim.gLimTDLSOffChannelEnabled,
			pe_session->tdls_chan_swit_prohibited);
	}
	setup_rsp->Status.status = setupStatus;
	/*
	 * now we pack it.  First, how much space are we going to need?
	 */
	status = dot11f_get_packed_tdls_setup_rsp_size(mac, setup_rsp,
						       &nPayload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to calculate the packed size for a Setup Response (0x%08x)",
			status);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fProbeRequest);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while calculating the packed size for Setup Response (0x%08x)",
			status);
	}

	/*
	 * This frame is going out from PE as data frames with special ethertype
	 * 89-0d.
	 * 8 bytes of RFC 1042 header
	 */

	nBytes = nPayload + ((IS_QOS_ENABLED(pe_session))
			     ? sizeof(tSirMacDataHdr3a) :
			     sizeof(tSirMacMgmtHdr))
		 + sizeof(eth_890d_header)
		 + PAYLOAD_TYPE_TDLS_SIZE + addIeLen;

	/* Ok-- try to allocate memory from MGMT PKT pool */
	qdf_status = cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				      (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a TDLS Setup Response",
			nBytes);
		qdf_mem_free(setup_rsp);
		return QDF_STATUS_E_NOMEM;
	}

	/* zero out the memory */
	qdf_mem_zero(pFrame, nBytes);

	/*
	 * IE formation, memory allocation is completed, Now form TDLS discovery
	 * request frame
	 */

	/* fill out the buffer descriptor */

	header_offset = lim_prepare_tdls_frame_header(mac, pFrame,
			&setup_rsp->LinkIdentifier, TDLS_LINK_AP,
			TDLS_RESPONDER,
			(ac == WIFI_AC_VI) ? TID_AC_VI : TID_AC_BK,
			pe_session);

	pe_debug("SupportedChnlWidth: %x rxMCSMap: %x rxMCSMap: %x txSupDataRate: %x",
		setup_rsp->VHTCaps.supportedChannelWidthSet,
		setup_rsp->VHTCaps.rxMCSMap,
		setup_rsp->VHTCaps.txMCSMap,
		setup_rsp->VHTCaps.txSupDataRate);
	status = dot11f_pack_tdls_setup_rsp(mac, setup_rsp,
					    pFrame + header_offset,
					    nPayload, &nPayload);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack a TDLS Setup Response (0x%08x)",
			status);
		cds_packet_free((void *)pPacket);
		qdf_mem_free(setup_rsp);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing TDLS Setup Response (0x%08x)",
			status);
	}

	qdf_mem_free(setup_rsp);

	/* Copy the additional IE. */
	/* TODO : addIe is added at the end of the frame. This means it doesn't */
	/* follow the order. This should be ok, but we should consider changing this */
	/* if there is any IOT issue. */
	if (addIeLen != 0) {
		qdf_mem_copy(pFrame + header_offset + nPayload, addIe,
			     addIeLen);
	}

	pe_debug("[TDLS] action: %d (%s) -AP-> OTA peer="QDF_MAC_ADDR_FMT,
		TDLS_SETUP_RESPONSE,
		lim_trace_tdls_action_string(TDLS_SETUP_RESPONSE),
		QDF_MAC_ADDR_REF(peer_mac.bytes));

	mac->lim.tdls_frm_session_id = pe_session->smeSessionId;
	lim_diag_mgmt_tx_event_report(mac, (tpSirMacMgmtHdr) pFrame,
				      pe_session, QDF_STATUS_SUCCESS,
				      QDF_STATUS_SUCCESS);

	qdf_status = wma_tx_frame_with_tx_complete_send(mac, pPacket,
						     (uint16_t) nBytes,
						     TID_AC_VI,
						     pFrame,
						     smeSessionId, true);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		mac->lim.tdls_frm_session_id = NO_SESSION;
		pe_err("could not send TDLS Dis Request frame!");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/*
 * Send TDLS setup CNF frame on AP link
 */
static
QDF_STATUS lim_send_tdls_link_setup_cnf_frame(struct mac_context *mac,
					      struct qdf_mac_addr peer_mac,
					      uint8_t dialog,
					      uint32_t peerCapability,
					      struct pe_session *pe_session,
					      uint8_t *addIe,
					      uint16_t addIeLen,
					      enum wifi_traffic_ac ac)
{
	tDot11fTDLSSetupCnf *setup_cnf;
	uint32_t status = 0;
	uint32_t nPayload = 0;
	uint32_t nBytes = 0;
	uint32_t header_offset = 0;
	uint8_t *pFrame;
	void *pPacket;
	QDF_STATUS qdf_status;
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
	uint32_t padLen = 0;
#endif
	uint8_t smeSessionId = 0;

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	setup_cnf = qdf_mem_malloc(sizeof(*setup_cnf));
	if (!setup_cnf)
		return QDF_STATUS_E_NOMEM;

	/*
	 * The scheme here is to fill out a 'tDot11fProbeRequest' structure
	 * and then hand it off to 'dot11f_pack_probe_request' (for
	 * serialization).  We start by zero-initializing the structure:
	 */
	smeSessionId = pe_session->smeSessionId;

	/*
	 * setup Fixed fields,
	 */
	setup_cnf->Category.category = ACTION_CATEGORY_TDLS;
	setup_cnf->Action.action = TDLS_SETUP_CONFIRM;
	setup_cnf->DialogToken.token = dialog;

	populate_dot11f_link_iden(mac, pe_session,
				  &setup_cnf->LinkIdentifier, peer_mac,
				  TDLS_INITIATOR);
	/*
	 * TODO: we need to see if we have to support conditions where we have
	 * EDCA parameter info element is needed a) if we need different QOS
	 * parameters for off channel operations or QOS is not supported on
	 * AP link and we wanted to QOS on direct link.
	 */

	/* Check self and peer WMM capable */
	if ((1 == mac->lim.gLimTDLSWmmMode) &&
	    (CHECK_BIT(peerCapability, TDLS_PEER_WMM_CAP))) {
		pe_debug("populate WMM praram in Setup Confirm");
		populate_dot11f_wmm_params(mac, &setup_cnf->WMMParams,
					   pe_session);
	}

	/* Check peer is VHT capable */
	if (CHECK_BIT(peerCapability, TDLS_PEER_VHT_CAP)) {
		populate_dot11f_vht_operation(mac,
					      pe_session,
					      &setup_cnf->VHTOperation);
		populate_dot11f_ht_info(mac, &setup_cnf->HTInfo, pe_session);
	} else if (CHECK_BIT(peerCapability, TDLS_PEER_HT_CAP)) {       /* Check peer is HT capable */
		populate_dot11f_ht_info(mac, &setup_cnf->HTInfo, pe_session);
	}

	lim_tdls_fill_setup_cnf_he_op(mac, peerCapability, setup_cnf,
				      pe_session);

	/*
	 * now we pack it.  First, how much space are we going to need?
	 */
	status = dot11f_get_packed_tdls_setup_cnf_size(mac, setup_cnf,
						       &nPayload);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to calculate the packed size for a Setup Confirm (0x%08x)",
			status);
		/* We'll fall back on the worst case scenario: */
		nPayload = sizeof(tDot11fProbeRequest);
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while calculating the packed size for Setup Confirm (0x%08x)",
			status);
	}

	/*
	 * This frame is going out from PE as data frames with special ethertype
	 * 89-0d.
	 * 8 bytes of RFC 1042 header
	 */

	nBytes = nPayload + ((IS_QOS_ENABLED(pe_session))
			     ? sizeof(tSirMacDataHdr3a) :
			     sizeof(tSirMacMgmtHdr))
		 + sizeof(eth_890d_header)
		 + PAYLOAD_TYPE_TDLS_SIZE + addIeLen;

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
	/* IOT issue with some AP : some AP doesn't like the data packet size < minimum 802.3 frame length (64)
	   Hence AP itself padding some bytes, which caused teardown packet is dropped at
	   receiver side. To avoid such IOT issue, we added some extra bytes to meet data frame size >= 64
	 */
	if (nPayload + PAYLOAD_TYPE_TDLS_SIZE < MIN_IEEE_8023_SIZE) {
		padLen =
			MIN_IEEE_8023_SIZE - (nPayload + PAYLOAD_TYPE_TDLS_SIZE);

		/* if padLen is less than minimum vendorSpecific (5), pad up to 5 */
		if (padLen < MIN_VENDOR_SPECIFIC_IE_SIZE)
			padLen = MIN_VENDOR_SPECIFIC_IE_SIZE;

		nBytes += padLen;
	}
#endif

	/* Ok-- try to allocate memory from MGMT PKT pool */
	qdf_status = cds_packet_alloc((uint16_t) nBytes, (void **)&pFrame,
				      (void **)&pPacket);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		pe_err("Failed to allocate %d bytes for a TDLS Setup Confirm",
			nBytes);
		qdf_mem_free(setup_cnf);
		return QDF_STATUS_E_NOMEM;
	}

	/* zero out the memory */
	qdf_mem_zero(pFrame, nBytes);

	/*
	 * IE formation, memory allocation is completed, Now form TDLS discovery
	 * request frame
	 */

	/* fill out the buffer descriptor */

	header_offset = lim_prepare_tdls_frame_header(mac, pFrame,
				&setup_cnf->LinkIdentifier,
				TDLS_LINK_AP,
				TDLS_INITIATOR,
				(ac == WIFI_AC_VI) ? TID_AC_VI : TID_AC_BK,
				pe_session);

	status = dot11f_pack_tdls_setup_cnf(mac, setup_cnf, pFrame
					    + header_offset, nPayload, &nPayload);

	if (DOT11F_FAILED(status)) {
		pe_err("Failed to pack a TDLS discovery req (0x%08x)", status);
		cds_packet_free((void *)pPacket);
		qdf_mem_free(setup_cnf);
		return QDF_STATUS_E_FAILURE;
	} else if (DOT11F_WARNED(status)) {
		pe_warn("There were warnings while packing TDLS Discovery Request (0x%08x)",
			status);
	}

	qdf_mem_free(setup_cnf);

	/* Copy the additional IE. */
	/* TODO : addIe is added at the end of the frame. This means it doesn't */
	/* follow the order. This should be ok, but we should consider changing this */
	/* if there is any IOT issue. */
	if (addIeLen != 0) {
		qdf_mem_copy(pFrame + header_offset + nPayload, addIe,
			     addIeLen);
	}
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
	if (padLen != 0) {
		/* QCOM VENDOR OUI = { 0x00, 0xA0, 0xC6, type = 0x0000 }; */
		uint8_t *padVendorSpecific =
			pFrame + header_offset + nPayload + addIeLen;
		/* make QCOM_VENDOR_OUI, and type = 0x0000, and all the payload to be zero */
		padVendorSpecific[0] = 221;
		padVendorSpecific[1] = padLen - 2;
		padVendorSpecific[2] = 0x00;
		padVendorSpecific[3] = 0xA0;
		padVendorSpecific[4] = 0xC6;

		pe_debug("Padding Vendor Specific Ie Len: %d", padLen);

		/* padding zero if more than 5 bytes are required */
		if (padLen > MIN_VENDOR_SPECIFIC_IE_SIZE)
			qdf_mem_zero(pFrame + header_offset + nPayload +
				    addIeLen + MIN_VENDOR_SPECIFIC_IE_SIZE,
				    padLen - MIN_VENDOR_SPECIFIC_IE_SIZE);
	}
#endif

	pe_debug("[TDLS] action: %d (%s) -AP-> OTA peer="QDF_MAC_ADDR_FMT,
		TDLS_SETUP_CONFIRM,
		lim_trace_tdls_action_string(TDLS_SETUP_CONFIRM),
	       QDF_MAC_ADDR_REF(peer_mac.bytes));

	mac->lim.tdls_frm_session_id = pe_session->smeSessionId;
	lim_diag_mgmt_tx_event_report(mac, (tpSirMacMgmtHdr) pFrame,
				      pe_session, QDF_STATUS_SUCCESS,
				      QDF_STATUS_SUCCESS);

	qdf_status = wma_tx_frame_with_tx_complete_send(mac, pPacket,
						     (uint16_t) nBytes,
						     TID_AC_VI,
						     pFrame,
						     smeSessionId, true);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		mac->lim.tdls_frm_session_id = NO_SESSION;
		pe_err("could not send TDLS Setup Confirm frame");
		return QDF_STATUS_E_FAILURE;

	}

	return QDF_STATUS_SUCCESS;
}

/* This Function is similar to populate_dot11f_ht_caps, except that
 * the HT Capabilities are considered from the AddStaReq rather from
 * the cfg.dat as in populate_dot11f_ht_caps
 */
static QDF_STATUS
lim_tdls_populate_dot11f_ht_caps(struct mac_context *mac,
				 struct pe_session *pe_session,
				 struct tdls_add_sta_req *add_sta_req,
				 tDot11fIEHTCaps *pDot11f)
{
	uint32_t nCfgValue;
	uint8_t nCfgValue8;
	tSirMacHTParametersInfo *pHTParametersInfo;
	union {
		uint16_t nCfgValue16;
		struct mlme_ht_capabilities_info ht_cap_info;
		tSirMacExtendedHTCapabilityInfo extHtCapInfo;
	} uHTCapabilityInfo;

	tSirMacTxBFCapabilityInfo *pTxBFCapabilityInfo;
	tSirMacASCapabilityInfo *pASCapabilityInfo;

	nCfgValue = add_sta_req->ht_cap.hc_cap;

	uHTCapabilityInfo.nCfgValue16 = nCfgValue & 0xFFFF;

	pDot11f->advCodingCap = uHTCapabilityInfo.ht_cap_info.adv_coding_cap;
	pDot11f->mimoPowerSave = uHTCapabilityInfo.ht_cap_info.mimo_power_save;
	pDot11f->greenField = uHTCapabilityInfo.ht_cap_info.green_field;
	pDot11f->shortGI20MHz = uHTCapabilityInfo.ht_cap_info.short_gi_20_mhz;
	pDot11f->shortGI40MHz = uHTCapabilityInfo.ht_cap_info.short_gi_40_mhz;
	pDot11f->txSTBC = uHTCapabilityInfo.ht_cap_info.tx_stbc;
	pDot11f->rxSTBC = uHTCapabilityInfo.ht_cap_info.rx_stbc;
	pDot11f->delayedBA = uHTCapabilityInfo.ht_cap_info.delayed_ba;
	pDot11f->maximalAMSDUsize =
		uHTCapabilityInfo.ht_cap_info.maximal_amsdu_size;
	pDot11f->dsssCckMode40MHz =
		uHTCapabilityInfo.ht_cap_info.dsss_cck_mode_40_mhz;
	pDot11f->psmp = uHTCapabilityInfo.ht_cap_info.psmp;
	pDot11f->stbcControlFrame =
		uHTCapabilityInfo.ht_cap_info.stbc_control_frame;
	pDot11f->lsigTXOPProtection =
		uHTCapabilityInfo.ht_cap_info.l_sig_tx_op_protection;

	/*
	 * All sessionized entries will need the check below
	 * Only in case of NO session
	 */
	if (!pe_session) {
		pDot11f->supportedChannelWidthSet =
			uHTCapabilityInfo.ht_cap_info.
			supported_channel_width_set;
	} else {
		pDot11f->supportedChannelWidthSet =
			pe_session->htSupportedChannelWidthSet;
	}

	/* Ensure that shortGI40MHz is Disabled if supportedChannelWidthSet is
	   eHT_CHANNEL_WIDTH_20MHZ */
	if (pDot11f->supportedChannelWidthSet == eHT_CHANNEL_WIDTH_20MHZ) {
		pDot11f->shortGI40MHz = 0;
	}

	pe_debug("SupportedChnlWidth: %d, mimoPS: %d, GF: %d, shortGI20:%d, shortGI40: %d, dsssCck: %d",
		pDot11f->supportedChannelWidthSet,
		pDot11f->mimoPowerSave,
		pDot11f->greenField,
		pDot11f->shortGI20MHz,
		pDot11f->shortGI40MHz,
		pDot11f->dsssCckMode40MHz);

	nCfgValue = add_sta_req->ht_cap.ampdu_param;

	nCfgValue8 = (uint8_t) nCfgValue;
	pHTParametersInfo = (tSirMacHTParametersInfo *) &nCfgValue8;

	pDot11f->maxRxAMPDUFactor = pHTParametersInfo->maxRxAMPDUFactor;
	pDot11f->mpduDensity = pHTParametersInfo->mpduDensity;
	pDot11f->reserved1 = pHTParametersInfo->reserved;

	pe_debug("AMPDU Param: %x", nCfgValue);
	qdf_mem_copy(pDot11f->supportedMCSSet, add_sta_req->ht_cap.mcsset,
		     SIZE_OF_SUPPORTED_MCS_SET);

	nCfgValue = add_sta_req->ht_cap.extcap;

	uHTCapabilityInfo.nCfgValue16 = nCfgValue & 0xFFFF;

	pDot11f->pco = uHTCapabilityInfo.extHtCapInfo.pco;
	pDot11f->transitionTime = uHTCapabilityInfo.extHtCapInfo.transitionTime;
	pDot11f->mcsFeedback = uHTCapabilityInfo.extHtCapInfo.mcsFeedback;

	nCfgValue = add_sta_req->ht_cap.txbf_cap;

	pTxBFCapabilityInfo = (tSirMacTxBFCapabilityInfo *) &nCfgValue;
	pDot11f->txBF = pTxBFCapabilityInfo->txBF;
	pDot11f->rxStaggeredSounding = pTxBFCapabilityInfo->rxStaggeredSounding;
	pDot11f->txStaggeredSounding = pTxBFCapabilityInfo->txStaggeredSounding;
	pDot11f->rxZLF = pTxBFCapabilityInfo->rxZLF;
	pDot11f->txZLF = pTxBFCapabilityInfo->txZLF;
	pDot11f->implicitTxBF = pTxBFCapabilityInfo->implicitTxBF;
	pDot11f->calibration = pTxBFCapabilityInfo->calibration;
	pDot11f->explicitCSITxBF = pTxBFCapabilityInfo->explicitCSITxBF;
	pDot11f->explicitUncompressedSteeringMatrix =
		pTxBFCapabilityInfo->explicitUncompressedSteeringMatrix;
	pDot11f->explicitBFCSIFeedback =
		pTxBFCapabilityInfo->explicitBFCSIFeedback;
	pDot11f->explicitUncompressedSteeringMatrixFeedback =
		pTxBFCapabilityInfo->explicitUncompressedSteeringMatrixFeedback;
	pDot11f->explicitCompressedSteeringMatrixFeedback =
		pTxBFCapabilityInfo->explicitCompressedSteeringMatrixFeedback;
	pDot11f->csiNumBFAntennae = pTxBFCapabilityInfo->csiNumBFAntennae;
	pDot11f->uncompressedSteeringMatrixBFAntennae =
		pTxBFCapabilityInfo->uncompressedSteeringMatrixBFAntennae;
	pDot11f->compressedSteeringMatrixBFAntennae =
		pTxBFCapabilityInfo->compressedSteeringMatrixBFAntennae;

	nCfgValue = add_sta_req->ht_cap.antenna;

	nCfgValue8 = (uint8_t) nCfgValue;

	pASCapabilityInfo = (tSirMacASCapabilityInfo *) &nCfgValue8;
	pDot11f->antennaSelection = pASCapabilityInfo->antennaSelection;
	pDot11f->explicitCSIFeedbackTx =
		pASCapabilityInfo->explicitCSIFeedbackTx;
	pDot11f->antennaIndicesFeedbackTx =
		pASCapabilityInfo->antennaIndicesFeedbackTx;
	pDot11f->explicitCSIFeedback = pASCapabilityInfo->explicitCSIFeedback;
	pDot11f->antennaIndicesFeedback =
		pASCapabilityInfo->antennaIndicesFeedback;
	pDot11f->rxAS = pASCapabilityInfo->rxAS;
	pDot11f->txSoundingPPDUs = pASCapabilityInfo->txSoundingPPDUs;

	pDot11f->present = add_sta_req->htcap_present;

	return QDF_STATUS_SUCCESS;

}

static QDF_STATUS
lim_tdls_populate_dot11f_vht_caps(struct mac_context *mac,
				  struct tdls_add_sta_req *add_sta_req,
				  tDot11fIEVHTCaps *pDot11f)
{
	uint32_t nCfgValue = 0;
	union {
		uint32_t nCfgValue32;
		tSirMacVHTCapabilityInfo vhtCapInfo;
	} uVHTCapabilityInfo;
	union {
		uint16_t nCfgValue16;
		tSirMacVHTTxSupDataRateInfo vhtTxSupDataRateInfo;
		tSirMacVHTRxSupDataRateInfo vhtRxsupDataRateInfo;
	} uVHTSupDataRateInfo;

	pDot11f->present = add_sta_req->vhtcap_present;

	nCfgValue = add_sta_req->vht_cap.vht_capinfo;
	uVHTCapabilityInfo.nCfgValue32 = nCfgValue;

	pDot11f->maxMPDULen = uVHTCapabilityInfo.vhtCapInfo.maxMPDULen;
	pDot11f->supportedChannelWidthSet =
		uVHTCapabilityInfo.vhtCapInfo.supportedChannelWidthSet;
	pDot11f->ldpcCodingCap = uVHTCapabilityInfo.vhtCapInfo.ldpcCodingCap;
	pDot11f->shortGI80MHz = uVHTCapabilityInfo.vhtCapInfo.shortGI80MHz;
	pDot11f->shortGI160and80plus80MHz =
		uVHTCapabilityInfo.vhtCapInfo.shortGI160and80plus80MHz;
	pDot11f->txSTBC = uVHTCapabilityInfo.vhtCapInfo.txSTBC;
	pDot11f->rxSTBC = uVHTCapabilityInfo.vhtCapInfo.rxSTBC;
	pDot11f->suBeamFormerCap = 0;
	pDot11f->suBeamformeeCap = 0;
	pDot11f->csnofBeamformerAntSup =
		uVHTCapabilityInfo.vhtCapInfo.csnofBeamformerAntSup;
	pDot11f->numSoundingDim = uVHTCapabilityInfo.vhtCapInfo.numSoundingDim;
	pDot11f->muBeamformerCap = 0;
	pDot11f->muBeamformeeCap = 0;
	pDot11f->vhtTXOPPS = uVHTCapabilityInfo.vhtCapInfo.vhtTXOPPS;
	pDot11f->htcVHTCap = uVHTCapabilityInfo.vhtCapInfo.htcVHTCap;
	pDot11f->maxAMPDULenExp = uVHTCapabilityInfo.vhtCapInfo.maxAMPDULenExp;
	pDot11f->vhtLinkAdaptCap =
		uVHTCapabilityInfo.vhtCapInfo.vhtLinkAdaptCap;
	pDot11f->rxAntPattern = uVHTCapabilityInfo.vhtCapInfo.rxAntPattern;
	pDot11f->txAntPattern = uVHTCapabilityInfo.vhtCapInfo.txAntPattern;
	pDot11f->extended_nss_bw_supp =
		uVHTCapabilityInfo.vhtCapInfo.extended_nss_bw_supp;

	pDot11f->rxMCSMap = add_sta_req->vht_cap.supp_mcs.rx_mcs_map;

	nCfgValue = add_sta_req->vht_cap.supp_mcs.rx_highest;
	uVHTSupDataRateInfo.nCfgValue16 = nCfgValue & 0xffff;
	pDot11f->rxHighSupDataRate =
		uVHTSupDataRateInfo.vhtRxsupDataRateInfo.rxSupDataRate;
	pDot11f->max_nsts_total =
		uVHTSupDataRateInfo.vhtRxsupDataRateInfo.max_nsts_total;

	pDot11f->txMCSMap = add_sta_req->vht_cap.supp_mcs.tx_mcs_map;

	nCfgValue = add_sta_req->vht_cap.supp_mcs.tx_highest;
	uVHTSupDataRateInfo.nCfgValue16 = nCfgValue & 0xffff;
	pDot11f->txSupDataRate =
		uVHTSupDataRateInfo.vhtTxSupDataRateInfo.txSupDataRate;

	pDot11f->vht_extended_nss_bw_cap =
	uVHTSupDataRateInfo.vhtTxSupDataRateInfo.vht_extended_nss_bw_cap;

	lim_log_vht_cap(mac, pDot11f);

	return QDF_STATUS_SUCCESS;

}

/**
 * lim_tdls_populate_matching_rate_set() - populate matching rate set
 *
 * @mac_ctx  - global MAC context
 * @stads - station hash entry
 * @supp_rate_set - pointer to supported rate set
 * @supp_rates_len - length of the supported rates
 * @supp_mcs_set - pointer to supported MSC set
 * @session_entry - pointer to PE session entry
 * @vht_caps - pointer to VHT capability
 *
 *
 * This function gets set of available rates from the config and compare them
 * against the set of received supported rates. After the comparison station
 * entry's rates is populated with 11A rates and 11B rates.
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_FAILURE on failure.
 */
static QDF_STATUS
lim_tdls_populate_matching_rate_set(struct mac_context *mac_ctx,
				    tpDphHashNode stads,
				    uint8_t *supp_rate_set,
				    uint8_t supp_rates_len,
				    uint8_t *supp_mcs_set,
				    struct pe_session *session_entry,
				    tDot11fIEVHTCaps *vht_caps)
{
	tSirMacRateSet temp_rate_set;
	uint32_t i, j, is_a_rate;
	uint32_t phymode;
	uint8_t mcsSet[SIZE_OF_SUPPORTED_MCS_SET];
	struct supported_rates *rates;
	uint8_t a_rateindex = 0;
	uint8_t b_rateindex = 0;
	uint8_t nss;
	qdf_size_t val_len;

	is_a_rate = 0;

	lim_get_phy_mode(mac_ctx, &phymode, NULL);

	/**
	 * Copy received rates in temp_rate_set, the parser has ensured
	 * unicity of the rates so there cannot be more than 12 .
	 */
	if (supp_rates_len > WLAN_SUPPORTED_RATES_IE_MAX_LEN) {
		pe_warn("Supported rates length: %d more than the Max limit, reset to Max",
			supp_rates_len);
		supp_rates_len = WLAN_SUPPORTED_RATES_IE_MAX_LEN;
	}

	for (i = 0; i < supp_rates_len; i++)
		temp_rate_set.rate[i] = supp_rate_set[i];

	temp_rate_set.numRates = supp_rates_len;

	rates = &stads->supportedRates;
	qdf_mem_zero(rates, sizeof(*rates));

	for (j = 0; j < temp_rate_set.numRates; j++) {
		if ((b_rateindex > SIR_NUM_11B_RATES) ||
		    (a_rateindex > SIR_NUM_11A_RATES)) {
			pe_warn("Invalid number of rates (11b->%d, 11a->%d)",
				b_rateindex, a_rateindex);
			return QDF_STATUS_E_FAILURE;
		}
		if (sirIsArate(temp_rate_set.rate[j] & 0x7f)) {
			is_a_rate = 1;
			if (a_rateindex < SIR_NUM_11A_RATES)
				rates->llaRates[a_rateindex++] =
						temp_rate_set.rate[j];
		} else {
			if (b_rateindex < SIR_NUM_11B_RATES)
				rates->llbRates[b_rateindex++] =
						temp_rate_set.rate[j];
		}
	}

	if (wlan_reg_is_5ghz_ch_freq(session_entry->curr_op_freq))
		nss = mac_ctx->vdev_type_nss_5g.tdls;
	else
		nss = mac_ctx->vdev_type_nss_2g.tdls;

	nss = QDF_MIN(nss, mac_ctx->user_configured_nss);

	/* compute the matching MCS rate set, if peer is 11n capable and self mode is 11n */
#ifdef FEATURE_WLAN_TDLS
	if (stads->mlmStaContext.htCapability)
#else
	if (IS_DOT11_MODE_HT(session_entry->dot11mode) &&
	    (stads->mlmStaContext.htCapability))
#endif
	{
		val_len = SIZE_OF_SUPPORTED_MCS_SET;
		if (wlan_mlme_get_cfg_str(
			mcsSet,
			&mac_ctx->mlme_cfg->rates.supported_mcs_set,
			&val_len) != QDF_STATUS_SUCCESS) {
			/* Could not get rateset from CFG. Log error. */
			pe_err("could not retrieve supportedMCSSet");
			return QDF_STATUS_E_FAILURE;
		}

		if (NSS_1x1_MODE == nss)
			mcsSet[1] = 0;
		for (i = 0; i < val_len; i++)
			stads->supportedRates.supportedMCSSet[i] =
				mcsSet[i] & supp_mcs_set[i];

		pe_debug("MCS Rate Set Bitmap from CFG and DPH");
		for (i = 0; i < SIR_MAC_MAX_SUPPORTED_MCS_SET; i++) {
			pe_debug("%x %x", mcsSet[i],
				stads->supportedRates.supportedMCSSet[i]);
		}
	}
	lim_populate_vht_mcs_set(mac_ctx, &stads->supportedRates, vht_caps,
				 session_entry, nss, NULL);

	lim_tdls_populate_he_matching_rate_set(mac_ctx, stads, nss,
					       session_entry);
	/**
	 * Set the erpEnabled bit if the phy is in G mode and at least
	 * one A rate is supported
	 */
	if ((phymode == WNI_CFG_PHY_MODE_11G) && is_a_rate)
		stads->erpEnabled = eHAL_SET;

	return QDF_STATUS_SUCCESS;
}

/*
 * update HASH node entry info
 */
static void lim_tdls_update_hash_node_info(struct mac_context *mac,
					   tDphHashNode *sta,
					   struct tdls_add_sta_req *add_sta_req,
					   struct pe_session *pe_session)
{
	tDot11fIEHTCaps htCap = {0,};
	tDot11fIEHTCaps *htCaps;
	tDot11fIEVHTCaps *pVhtCaps = NULL;
	tDot11fIEVHTCaps *pVhtCaps_txbf = NULL;
	tDot11fIEVHTCaps vhtCap;
	uint8_t cbMode, selfDot11Mode;

	if (add_sta_req->tdls_oper == TDLS_OPER_ADD) {
		populate_dot11f_ht_caps(mac, pe_session, &htCap);
	} else if (add_sta_req->tdls_oper == TDLS_OPER_UPDATE) {
		lim_tdls_populate_dot11f_ht_caps(mac, NULL,
						 add_sta_req, &htCap);
		sta->rmfEnabled = add_sta_req->is_pmf;
	}
	htCaps = &htCap;
	if (htCaps->present) {
		sta->mlmStaContext.htCapability = 1;
		sta->htGreenfield = htCaps->greenField;
		/*
		 * sta->htSupportedChannelWidthSet should have the base
		 * channel capability. The htSupportedChannelWidthSet of the
		 * TDLS link on base channel should be less than or equal to
		 * channel width of STA-AP link. So take this setting from the
		 * pe_session.
		 */
		pe_debug("supportedChannelWidthSet: 0x%x htSupportedChannelWidthSet: 0x%x",
				htCaps->supportedChannelWidthSet,
				pe_session->htSupportedChannelWidthSet);
		sta->htSupportedChannelWidthSet =
				(htCaps->supportedChannelWidthSet <
				 pe_session->htSupportedChannelWidthSet) ?
				htCaps->supportedChannelWidthSet :
				pe_session->htSupportedChannelWidthSet;
		pe_debug("sta->htSupportedChannelWidthSet: 0x%x",
				sta->htSupportedChannelWidthSet);

		sta->ch_width = sta->htSupportedChannelWidthSet;
		sta->htMIMOPSState = htCaps->mimoPowerSave;
		sta->htMaxAmsduLength = htCaps->maximalAMSDUsize;
		sta->htAMpduDensity = htCaps->mpduDensity;
		sta->htDsssCckRate40MHzSupport = htCaps->dsssCckMode40MHz;
		sta->htShortGI20Mhz = htCaps->shortGI20MHz;
		sta->htShortGI40Mhz = htCaps->shortGI40MHz;
		sta->htMaxRxAMpduFactor = htCaps->maxRxAMPDUFactor;
		lim_fill_rx_highest_supported_rate(mac,
						   &sta->supportedRates.
						   rxHighestDataRate,
						   htCaps->supportedMCSSet);
		sta->ht_caps = add_sta_req->ht_cap.hc_cap;
	} else {
		sta->mlmStaContext.htCapability = 0;
	}
	lim_tdls_populate_dot11f_vht_caps(mac, add_sta_req, &vhtCap);
	pVhtCaps = &vhtCap;
	if (pVhtCaps->present) {
		sta->mlmStaContext.vhtCapability = 1;

		/*
		 * 11.21.1 General: The channel width of the TDLS direct
		 * link on the base channel shall not exceed the channel
		 * width of the BSS to which the TDLS peer STAs are
		 * associated.
		 */
		if (pe_session->ch_width)
			sta->vhtSupportedChannelWidthSet =
					pe_session->ch_width - 1;
		else
			sta->vhtSupportedChannelWidthSet =
					pe_session->ch_width;

		pe_debug("vhtSupportedChannelWidthSet: %hu htSupportedChannelWidthSet: %hu",
			sta->vhtSupportedChannelWidthSet,
			sta->htSupportedChannelWidthSet);

		sta->vhtLdpcCapable = pVhtCaps->ldpcCodingCap;
		sta->vhtBeamFormerCapable = 0;
		pVhtCaps_txbf = (tDot11fIEVHTCaps *) (&add_sta_req->vht_cap);
		pVhtCaps_txbf->suBeamformeeCap = 0;
		pVhtCaps_txbf->suBeamFormerCap = 0;
		pVhtCaps_txbf->muBeamformerCap = 0;
		pVhtCaps_txbf->muBeamformeeCap = 0;
		sta->vht_caps = add_sta_req->vht_cap.vht_capinfo;
	} else {
		sta->mlmStaContext.vhtCapability = 0;
		sta->vhtSupportedChannelWidthSet =
			WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
	}
	selfDot11Mode = mac->mlme_cfg->dot11_mode.dot11_mode;
	if (IS_DOT11_MODE_HE(selfDot11Mode))
		lim_tdls_update_node_he_caps(mac, add_sta_req, sta, pe_session);
	else
		pe_debug("Not populating he cap as SelfDot11Mode not HE %d",
			 selfDot11Mode);
	/*
	 * Calculate the Secondary Coannel Offset if our
	 * own channel bonding state is enabled
	 */
	if (pe_session->htSupportedChannelWidthSet) {
		cbMode = lim_select_cb_mode(sta, pe_session,
				    wlan_reg_freq_to_chan(
				    mac->pdev, pe_session->curr_op_freq),
				    sta->vhtSupportedChannelWidthSet);

		if (sta->mlmStaContext.vhtCapability)
			sta->htSecondaryChannelOffset =
					lim_get_htcb_state(cbMode);
		else
			sta->htSecondaryChannelOffset = cbMode;
	}
	/* Lets enable QOS parameter */
	sta->qosMode = (add_sta_req->capability & CAPABILITIES_QOS_OFFSET)
				|| add_sta_req->htcap_present;
	sta->wmeEnabled = 1;
	sta->lleEnabled = 0;
	/*  TDLS Dummy AddSTA does not have qosInfo , is it OK ??
	 */
	sta->qos.capability.qosInfo =
		(*(tSirMacQosInfoStation *) &add_sta_req->uapsd_queues);

	/* populate matching rate set */

	/* TDLS Dummy AddSTA does not have HTCap,VHTCap,Rates info , is it OK ??
	 */
	lim_tdls_populate_matching_rate_set(mac, sta,
					    add_sta_req->supported_rates,
					    add_sta_req->supported_rates_length,
					    add_sta_req->ht_cap.mcsset,
					    pe_session, pVhtCaps);

	lim_tdls_check_and_force_he_ldpc_cap(pe_session, sta);

	/*  TDLS Dummy AddSTA does not have right capability , is it OK ??
	 */
	sta->mlmStaContext.capabilityInfo =
		(*(tSirMacCapabilityInfo *) &add_sta_req->capability);

	return;
}

/*
 * Add STA for TDLS setup procedure
 */
static QDF_STATUS lim_tdls_setup_add_sta(struct mac_context *mac,
					    struct tdls_add_sta_req *pAddStaReq,
					    struct pe_session *pe_session)
{
	tpDphHashNode sta = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint16_t aid = 0;

	sta = dph_lookup_hash_entry(mac, pAddStaReq->peermac.bytes, &aid,
				       &pe_session->dph.dphHashTable);
	if (!sta && pAddStaReq->tdls_oper == TDLS_OPER_UPDATE) {
		pe_err("TDLS update peer is given without peer creation");
		return QDF_STATUS_E_FAILURE;
	}
	if (sta && pAddStaReq->tdls_oper == TDLS_OPER_ADD) {
		pe_err("TDLS entry for peer: "QDF_MAC_ADDR_FMT " already exist, cannot add new entry",
			QDF_MAC_ADDR_REF(pAddStaReq->peermac.bytes));
			return QDF_STATUS_E_FAILURE;
	}

	if (sta && sta->staType != STA_ENTRY_TDLS_PEER) {
		pe_err("Non TDLS entry for peer: "QDF_MAC_ADDR_FMT " already exist",
			QDF_MAC_ADDR_REF(pAddStaReq->peermac.bytes));
			return QDF_STATUS_E_FAILURE;
	}

	if (!sta) {
		aid = lim_assign_peer_idx(mac, pe_session);

		if (!aid) {
			pe_err("No more free AID for peer: "QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(pAddStaReq->peermac.bytes));
			return QDF_STATUS_E_FAILURE;
		}

		/* Set the aid in peerAIDBitmap as it has been assigned to TDLS peer */
		SET_PEER_AID_BITMAP(pe_session->peerAIDBitmap, aid);

		pe_debug("Aid: %d, for peer: " QDF_MAC_ADDR_FMT,
			aid, QDF_MAC_ADDR_REF(pAddStaReq->peermac.bytes));
		sta =
			dph_get_hash_entry(mac, aid,
					   &pe_session->dph.dphHashTable);

		if (sta) {
			(void)lim_del_sta(mac, sta, false /*asynchronous */,
					  pe_session);
			lim_delete_dph_hash_entry(mac, sta->staAddr, aid,
						  pe_session);
		}

		sta = dph_add_hash_entry(mac, pAddStaReq->peermac.bytes,
					 aid, &pe_session->dph.dphHashTable);

		if (!sta) {
			pe_err("add hash entry failed");
			QDF_ASSERT(0);
			return QDF_STATUS_E_FAILURE;
		}
	}

	lim_tdls_update_hash_node_info(mac, sta, pAddStaReq, pe_session);

	sta->staType = STA_ENTRY_TDLS_PEER;

	status =
		lim_add_sta(mac, sta,
			    (pAddStaReq->tdls_oper ==
			     TDLS_OPER_UPDATE) ? true : false, pe_session);

	if (QDF_STATUS_SUCCESS != status) {
		/* should not fail */
		QDF_ASSERT(0);
	}
	return status;
}

/*
 * Del STA, after Link is teardown or discovery response sent on direct link
 */
static QDF_STATUS lim_tdls_del_sta(struct mac_context *mac,
				      struct qdf_mac_addr peerMac,
				      struct pe_session *pe_session,
				      bool resp_reqd)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint16_t peerIdx = 0;
	tpDphHashNode sta;

	sta = dph_lookup_hash_entry(mac, peerMac.bytes, &peerIdx,
				       &pe_session->dph.dphHashTable);

	if (sta && sta->staType == STA_ENTRY_TDLS_PEER)
		status = lim_del_sta(mac, sta, resp_reqd, pe_session);
	else
		pe_debug("TDLS peer "QDF_MAC_ADDR_FMT" not found",
			 QDF_MAC_ADDR_REF(peerMac.bytes));

	return status;
}

/*
 * Once Link is setup with PEER, send Add STA ind to SME
 */
static QDF_STATUS lim_send_sme_tdls_add_sta_rsp(struct mac_context *mac,
						uint8_t sessionId,
						tSirMacAddr peerMac,
						uint8_t updateSta,
						tDphHashNode *sta, uint8_t status)
{
	struct scheduler_msg msg = { 0 };
	struct tdls_add_sta_rsp *add_sta_rsp;
	QDF_STATUS ret;

	msg.type = eWNI_SME_TDLS_ADD_STA_RSP;

	add_sta_rsp = qdf_mem_malloc(sizeof(*add_sta_rsp));
	if (!add_sta_rsp)
		return QDF_STATUS_E_NOMEM;

	add_sta_rsp->session_id = sessionId;
	add_sta_rsp->status_code = status;

	if (peerMac) {
		qdf_mem_copy(add_sta_rsp->peermac.bytes,
			     (uint8_t *) peerMac, QDF_MAC_ADDR_SIZE);
	}
	if (updateSta)
		add_sta_rsp->tdls_oper = TDLS_OPER_UPDATE;
	else
		add_sta_rsp->tdls_oper = TDLS_OPER_ADD;

	add_sta_rsp->psoc = mac->psoc;
	msg.bodyptr = add_sta_rsp;
	msg.callback = tgt_tdls_add_peer_rsp;

	ret = scheduler_post_message(QDF_MODULE_ID_PE,
				     QDF_MODULE_ID_TDLS,
				     QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(ret)) {
		pe_err("post msg fail, %d", ret);
		qdf_mem_free(add_sta_rsp);
	}

	return ret;
}

/*
 * STA RSP received from HAL
 */
QDF_STATUS lim_process_tdls_add_sta_rsp(struct mac_context *mac, void *msg,
					struct pe_session *pe_session)
{
	tAddStaParams *pAddStaParams = (tAddStaParams *) msg;
	uint8_t status = QDF_STATUS_SUCCESS;
	tDphHashNode *sta = NULL;
	uint16_t aid = 0;

	SET_LIM_PROCESS_DEFD_MESGS(mac, true);
	pe_debug("staMac: "QDF_MAC_ADDR_FMT,
	       QDF_MAC_ADDR_REF(pAddStaParams->staMac));

	if (pAddStaParams->status != QDF_STATUS_SUCCESS) {
		QDF_ASSERT(0);
		pe_err("Add sta failed ");
		status = QDF_STATUS_E_FAILURE;
		goto add_sta_error;
	}

	sta = dph_lookup_hash_entry(mac, pAddStaParams->staMac, &aid,
				       &pe_session->dph.dphHashTable);
	if (!sta) {
		pe_err("sta is NULL ");
		status = QDF_STATUS_E_FAILURE;
		goto add_sta_error;
	}

	sta->mlmStaContext.mlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
	sta->valid = 1;
add_sta_error:
	status = lim_send_sme_tdls_add_sta_rsp(mac, pe_session->smeSessionId,
					       pAddStaParams->staMac,
					       pAddStaParams->updateSta, sta,
					       status);
	qdf_mem_free(pAddStaParams);
	return status;
}

/**
 * lim_send_tdls_comp_mgmt_rsp() - Send Response to upper layers
 * @mac_ctx:          Pointer to Global MAC structure
 * @msg_type:         Indicates message type
 * @result_code:       Indicates the result of previously issued
 *                    eWNI_SME_msg_type_REQ message
 * @vdev_id: vdev id
 *
 * This function is called by lim_process_sme_req_messages() to send
 * eWNI_SME_START_RSP, eWNI_SME_STOP_BSS_RSP
 * or eWNI_SME_SWITCH_CHL_RSP messages to applications above MAC
 * Software.
 *
 * Return: None
 */

static void
lim_send_tdls_comp_mgmt_rsp(struct mac_context *mac_ctx, uint16_t msg_type,
	 tSirResultCodes result_code, uint8_t vdev_id)
{
	struct scheduler_msg msg = {0};
	struct tdls_send_mgmt_rsp *sme_rsp;
	QDF_STATUS status;

	pe_debug("Sending message %s with reasonCode %s",
		lim_msg_str(msg_type), lim_result_code_str(result_code));

	sme_rsp = qdf_mem_malloc(sizeof(*sme_rsp));
	if (!sme_rsp)
		return;

	sme_rsp->status_code = (enum legacy_result_code)result_code;
	sme_rsp->vdev_id = vdev_id;
	sme_rsp->psoc = mac_ctx->psoc;

	msg.type = msg_type;
	msg.bodyptr = sme_rsp;
	msg.callback = tgt_tdls_send_mgmt_rsp;
	status = scheduler_post_message(QDF_MODULE_ID_PE,
					QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("post msg fail, %d", status);
		qdf_mem_free(sme_rsp);
	}
}

QDF_STATUS lim_process_sme_tdls_mgmt_send_req(struct mac_context *mac_ctx,
					      void *msg)
{
	struct tdls_send_mgmt_request *send_req = msg;
	struct pe_session *session_entry;
	uint8_t session_id;
	uint16_t ie_len;
	tSirResultCodes result_code = eSIR_SME_INVALID_PARAMETERS;

	pe_debug("Send Mgmt Received");
	session_entry = pe_find_session_by_bssid(mac_ctx,
						 send_req->bssid.bytes,
						 &session_id);
	if (!session_entry) {
		pe_err("PE Session does not exist for given sme session_id %d",
		       send_req->session_id);
		goto lim_tdls_send_mgmt_error;
	}

	/* check if we are in proper state to work as TDLS client */
	if (!LIM_IS_STA_ROLE(session_entry)) {
		pe_err("send mgmt received in wrong system Role: %d",
		       GET_LIM_SYSTEM_ROLE(session_entry));
		goto lim_tdls_send_mgmt_error;
	}

	if (lim_is_roam_synch_in_progress(mac_ctx->psoc, session_entry)) {
		pe_err("roaming in progress, reject mgmt! for session %d",
		       send_req->session_id);
		result_code = eSIR_SME_REFUSED;
		goto lim_tdls_send_mgmt_error;
	}

	/*
	 * if we are still good, go ahead and check if we are in proper state to
	 * do TDLS discovery req/rsp/....frames.
	 */
	if ((session_entry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
	    (session_entry->limSmeState != eLIM_SME_LINK_EST_STATE)) {
		pe_err("send mgmt received in invalid LIMsme state: %d",
		       session_entry->limSmeState);
		goto lim_tdls_send_mgmt_error;
	}

	cds_tdls_tx_rx_mgmt_event(ACTION_CATEGORY_TDLS,
		SIR_MAC_ACTION_TX, SIR_MAC_MGMT_ACTION,
		send_req->req_type, send_req->peer_mac.bytes);

	ie_len = send_req->length - sizeof(*send_req);

	switch (send_req->req_type) {
	case TDLS_DISCOVERY_REQUEST:
		pe_debug("Transmit Discovery Request Frame");
		/* format TDLS discovery request frame and transmit it */
		lim_send_tdls_dis_req_frame(mac_ctx, send_req->peer_mac,
					    send_req->dialog, session_entry,
					    send_req->ac);
		result_code = eSIR_SME_SUCCESS;
		break;
	case TDLS_DISCOVERY_RESPONSE:
		pe_debug("Transmit Discovery Response Frame");
		/* Send a response mgmt action frame */
		lim_send_tdls_dis_rsp_frame(mac_ctx, send_req->peer_mac,
					    send_req->dialog, session_entry,
					    send_req->add_ie, ie_len);
		result_code = eSIR_SME_SUCCESS;
		break;
	case TDLS_SETUP_REQUEST:
		pe_debug("Transmit Setup Request Frame");
		lim_send_tdls_link_setup_req_frame(mac_ctx,
						   send_req->peer_mac,
						   send_req->dialog,
						   session_entry,
						   send_req->add_ie, ie_len,
						   send_req->ac);
		result_code = eSIR_SME_SUCCESS;
		break;
	case TDLS_SETUP_RESPONSE:
		pe_debug("Transmit Setup Response Frame");
		lim_send_tdls_setup_rsp_frame(mac_ctx,
					      send_req->peer_mac,
					      send_req->dialog, session_entry,
					      send_req->status_code,
					      send_req->add_ie, ie_len,
					      send_req->ac);
		result_code = eSIR_SME_SUCCESS;
		break;
	case TDLS_SETUP_CONFIRM:
		pe_debug("Transmit Setup Confirm Frame");
		lim_send_tdls_link_setup_cnf_frame(mac_ctx,
						   send_req->peer_mac,
						   send_req->dialog,
						   send_req->peer_capability,
						   session_entry,
						   send_req->add_ie, ie_len,
						   send_req->ac);
		result_code = eSIR_SME_SUCCESS;
		break;
	case TDLS_TEARDOWN:
		pe_debug("Transmit Teardown Frame");
		lim_send_tdls_teardown_frame(mac_ctx,
					     send_req->peer_mac,
					     send_req->status_code,
					     send_req->responder,
					     session_entry,
					     send_req->add_ie, ie_len,
					     send_req->ac);
		result_code = eSIR_SME_SUCCESS;
		break;
	case TDLS_PEER_TRAFFIC_INDICATION:
		break;
	case TDLS_CHANNEL_SWITCH_REQUEST:
		break;
	case TDLS_CHANNEL_SWITCH_RESPONSE:
		break;
	case TDLS_PEER_TRAFFIC_RESPONSE:
		break;
	default:
		break;
	}

lim_tdls_send_mgmt_error:
	lim_send_tdls_comp_mgmt_rsp(mac_ctx, eWNI_SME_TDLS_SEND_MGMT_RSP,
				    result_code, send_req->session_id);

	return QDF_STATUS_SUCCESS;
}

/*
 * Once link is teardown, send Del Peer Ind to SME
 */
static QDF_STATUS lim_send_sme_tdls_del_sta_rsp(struct mac_context *mac,
						uint8_t sessionId,
						struct qdf_mac_addr peerMac,
						tDphHashNode *sta, uint8_t status)
{
	struct scheduler_msg msg = { 0 };
	struct tdls_del_sta_rsp *del_sta_rsp;
	QDF_STATUS ret;

	msg.type = eWNI_SME_TDLS_DEL_STA_RSP;

	del_sta_rsp = qdf_mem_malloc(sizeof(*del_sta_rsp));
	if (!del_sta_rsp)
		return QDF_STATUS_E_NOMEM;

	del_sta_rsp->session_id = sessionId;
	del_sta_rsp->status_code = status;

	qdf_copy_macaddr(&del_sta_rsp->peermac, &peerMac);

	del_sta_rsp->psoc = mac->psoc;
	msg.bodyptr = del_sta_rsp;
	msg.callback = tgt_tdls_del_peer_rsp;
	ret = scheduler_post_message(QDF_MODULE_ID_PE,
				     QDF_MODULE_ID_TDLS,
				     QDF_MODULE_ID_TARGET_IF, &msg);
	if (QDF_IS_STATUS_ERROR(ret)) {
		pe_err("post msg fail, %d", ret);
		qdf_mem_free(del_sta_rsp);
	}

	return ret;
}

QDF_STATUS lim_process_sme_tdls_add_sta_req(struct mac_context *mac,
					    void *msg)
{
	struct tdls_add_sta_req *add_sta_req = msg;
	struct pe_session *pe_session;
	uint8_t session_id;

	pe_debug("TDLS Add STA Request Received");
	pe_session =
		pe_find_session_by_bssid(mac, add_sta_req->bssid.bytes,
					 &session_id);
	if (!pe_session) {
		pe_err("PE Session does not exist for given sme sessionId: %d",
		       add_sta_req->session_id);
		goto lim_tdls_add_sta_error;
	}

	/* check if we are in proper state to work as TDLS client */
	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("send mgmt received in wrong system Role: %d",
			  GET_LIM_SYSTEM_ROLE(pe_session));
		goto lim_tdls_add_sta_error;
	}

	if (lim_is_roam_synch_in_progress(mac->psoc, pe_session)) {
		pe_err("roaming in progress, reject add sta! for session %d",
		       add_sta_req->session_id);
		goto lim_tdls_add_sta_error;
	}

	/*
	 * if we are still good, go ahead and check if we are in proper state to
	 * do TDLS discovery req/rsp/....frames.
	 */
	if ((pe_session->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
	    (pe_session->limSmeState != eLIM_SME_LINK_EST_STATE)) {
		pe_err("send mgmt received in invalid LIMsme state: %d",
			pe_session->limSmeState);
		goto lim_tdls_add_sta_error;
	}


	/* To start with, send add STA request to HAL */
	if (QDF_STATUS_E_FAILURE == lim_tdls_setup_add_sta(mac, add_sta_req, pe_session)) {
		pe_err("Add TDLS Station request failed");
		goto lim_tdls_add_sta_error;
	}
	return QDF_STATUS_SUCCESS;
lim_tdls_add_sta_error:
	lim_send_sme_tdls_add_sta_rsp(mac,
				      add_sta_req->session_id,
				      add_sta_req->peermac.bytes,
				      (add_sta_req->tdls_oper == TDLS_OPER_UPDATE),
				      NULL, QDF_STATUS_E_FAILURE);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_process_sme_tdls_del_sta_req(struct mac_context *mac,
					    void *msg)
{
	struct tdls_del_sta_req *del_sta_req = msg;
	struct pe_session *pe_session;
	uint8_t session_id;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	pe_debug("TDLS Delete STA Request Received");
	pe_session =
		pe_find_session_by_bssid(mac, del_sta_req->bssid.bytes,
					 &session_id);
	if (!pe_session) {
		pe_err("PE Session does not exist for given vdev id: %d",
		       del_sta_req->session_id);
		lim_send_sme_tdls_del_sta_rsp(mac, del_sta_req->session_id,
					      del_sta_req->peermac, NULL,
					      QDF_STATUS_E_FAILURE);
		return QDF_STATUS_E_FAILURE;
	}

	/* check if we are in proper state to work as TDLS client */
	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("Del sta received in wrong system Role %d",
		       GET_LIM_SYSTEM_ROLE(pe_session));
		goto lim_tdls_del_sta_error;
	}

	if (lim_is_roam_synch_in_progress(mac->psoc, pe_session)) {
		pe_err("roaming in progress, reject del sta! for session %d",
		       del_sta_req->session_id);
		lim_send_sme_tdls_del_sta_rsp(mac, del_sta_req->session_id,
					      del_sta_req->peermac, NULL,
					      QDF_STATUS_E_FAILURE);
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * if we are still good, go ahead and check if we are in proper state to
	 * do TDLS discovery req/rsp/....frames.
	 */
	if ((pe_session->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
	    (pe_session->limSmeState != eLIM_SME_LINK_EST_STATE)) {

		pe_err("Del Sta received in invalid LIMsme state: %d",
		       pe_session->limSmeState);
		goto lim_tdls_del_sta_error;
	}

	status = lim_tdls_del_sta(mac, del_sta_req->peermac,
				  pe_session, true);
	if (status == QDF_STATUS_SUCCESS)
		return status;

lim_tdls_del_sta_error:
	lim_send_sme_tdls_del_sta_rsp(mac, pe_session->smeSessionId,
				      del_sta_req->peermac, NULL, QDF_STATUS_E_FAILURE);

	return status;
}

/**
 * lim_check_aid_and_delete_peer() - Function to check aid and delete peer
 * @p_mac: pointer to mac context
 * @session_entry: pointer to PE session
 *
 * This function verifies aid and delete's peer with that aid from hash table
 *
 * Return: None
 */
static void lim_check_aid_and_delete_peer(struct mac_context *p_mac,
					  struct pe_session *session_entry)
{
	tpDphHashNode stads = NULL;
	int i, aid;
	size_t aid_bitmap_size = sizeof(session_entry->peerAIDBitmap);
	struct qdf_mac_addr mac_addr;
	QDF_STATUS status;
	/*
	 * Check all the set bit in peerAIDBitmap and delete the peer
	 * (with that aid) entry from the hash table and add the aid
	 * in free pool
	 */
	for (i = 0; i < aid_bitmap_size / sizeof(uint32_t); i++) {
		for (aid = 0; aid < (sizeof(uint32_t) << 3); aid++) {
			if (!CHECK_BIT(session_entry->peerAIDBitmap[i], aid))
				continue;
			stads = dph_get_hash_entry(p_mac,
					(aid + i * (sizeof(uint32_t) << 3)),
					&session_entry->dph.dphHashTable);

			if (!stads)
				goto skip;

			pe_debug("Deleting "QDF_MAC_ADDR_FMT,
				QDF_MAC_ADDR_REF(stads->staAddr));

			if (!lim_is_roam_synch_in_progress(p_mac->psoc,
							   session_entry)) {
				lim_send_deauth_mgmt_frame(p_mac,
					REASON_DEAUTH_NETWORK_LEAVING,
					stads->staAddr, session_entry, false);
			}
			/* Delete TDLS peer */
			qdf_mem_copy(mac_addr.bytes, stads->staAddr,
				     QDF_MAC_ADDR_SIZE);

			status = lim_tdls_del_sta(p_mac, mac_addr,
						  session_entry, false);

			dph_delete_hash_entry(p_mac,
				stads->staAddr, stads->assocId,
				&session_entry->dph.dphHashTable);
skip:
			lim_release_peer_idx(p_mac,
				(aid + i * (sizeof(uint32_t) << 3)),
				session_entry);
			CLEAR_BIT(session_entry->peerAIDBitmap[i], aid);
		}
	}
}

void lim_update_tdls_set_state_for_fw(struct pe_session *session_entry,
				      bool value)
{
	session_entry->tdls_send_set_state_disable  = value;
}

/**
 * lim_delete_tdls_peers() - delete tdls peers
 *
 * @mac_ctx - global MAC context
 * @session_entry - PE session entry
 *
 * Delete all the TDLS peer connected before leaving the BSS
 *
 * Return: QDF_STATUS_SUCCESS on success, error code otherwise
 */
QDF_STATUS lim_delete_tdls_peers(struct mac_context *mac_ctx,
				    struct pe_session *session_entry)
{

	if (!session_entry) {
		pe_err("NULL session_entry");
		return QDF_STATUS_E_FAILURE;
	}

	lim_check_aid_and_delete_peer(mac_ctx, session_entry);

	tgt_tdls_delete_all_peers_indication(mac_ctx->psoc,
					     session_entry->smeSessionId);

	if (lim_is_roam_synch_in_progress(mac_ctx->psoc, session_entry))
		return QDF_STATUS_SUCCESS;

	/* In case of CSA, Only peers in lim and TDLS component
	 * needs to be removed and set state disable command
	 * should not be sent to fw as there is no way to enable
	 * TDLS in FW after vdev restart.
	 */
	if (session_entry->tdls_send_set_state_disable) {
		tgt_tdls_peers_deleted_notification(mac_ctx->psoc,
						    session_entry->
						    smeSessionId);
	}

	/* reset the set_state_disable flag */
	session_entry->tdls_send_set_state_disable = true;
	return QDF_STATUS_SUCCESS;
}

/**
 * lim_process_sme_del_all_tdls_peers(): process delete tdls peers
 * @p_mac: pointer to mac context
 * @msg_buf: message buffer
 *
 * This function processes request to delete tdls peers
 *
 * Return: Success: QDF_STATUS_SUCCESS Failure: Error value
 */
QDF_STATUS lim_process_sme_del_all_tdls_peers(struct mac_context *p_mac,
						 uint32_t *msg_buf)
{
	struct tdls_del_all_tdls_peers *msg;
	struct pe_session *session_entry;
	uint8_t session_id;

	msg = (struct tdls_del_all_tdls_peers *)msg_buf;
	if (!msg) {
		pe_err("NULL msg");
		return QDF_STATUS_E_FAILURE;
	}

	session_entry = pe_find_session_by_bssid(p_mac,
						 msg->bssid.bytes, &session_id);
	if (!session_entry) {
		pe_debug("NULL pe_session");
		return QDF_STATUS_E_FAILURE;
	}

	lim_check_aid_and_delete_peer(p_mac, session_entry);

	tgt_tdls_peers_deleted_notification(p_mac->psoc,
					    session_entry->smeSessionId);

	return QDF_STATUS_SUCCESS;
}

/**
 * lim_process_tdls_del_sta_rsp() - Handle WDA_DELETE_STA_RSP for TDLS
 * @mac_ctx: Global MAC context
 * @lim_msg: LIM message
 * @pe_session: PE session
 *
 * Return: None
 */
void lim_process_tdls_del_sta_rsp(struct mac_context *mac_ctx,
				  struct scheduler_msg *lim_msg,
				  struct pe_session *session_entry)
{
	tpDeleteStaParams del_sta_params = (tpDeleteStaParams) lim_msg->bodyptr;
	tpDphHashNode sta_ds;
	uint16_t peer_idx = 0;
	struct qdf_mac_addr peer_mac;

	if (!del_sta_params) {
		pe_err("del_sta_params is NULL");
		return;
	}

	qdf_mem_copy(peer_mac.bytes,
		     del_sta_params->staMac, QDF_MAC_ADDR_SIZE);

	sta_ds = dph_lookup_hash_entry(mac_ctx, del_sta_params->staMac,
			&peer_idx, &session_entry->dph.dphHashTable);
	if (!sta_ds) {
		pe_err("DPH Entry for STA: %X is missing release the serialization command",
		       DPH_STA_HASH_INDEX_PEER);
		lim_send_sme_tdls_del_sta_rsp(mac_ctx,
					      session_entry->smeSessionId,
					      peer_mac, NULL,
					      QDF_STATUS_SUCCESS);
		goto skip_event;
	}

	if (QDF_STATUS_SUCCESS != del_sta_params->status) {
		pe_err("DEL STA failed!");
		lim_send_sme_tdls_del_sta_rsp(mac_ctx,
				      session_entry->smeSessionId,
				      peer_mac, NULL, QDF_STATUS_E_FAILURE);
		goto skip_event;
	}

	pe_debug("DEL STA success");

	/* now send indication to SME-->HDD->TL to remove STA from TL */

	lim_send_sme_tdls_del_sta_rsp(mac_ctx, session_entry->smeSessionId,
				      peer_mac, sta_ds,
				      QDF_STATUS_SUCCESS);
	lim_release_peer_idx(mac_ctx, sta_ds->assocId, session_entry);

	/* Clear the aid in peerAIDBitmap as this aid is now in freepool */
	CLEAR_PEER_AID_BITMAP(session_entry->peerAIDBitmap,
			      sta_ds->assocId);
	lim_delete_dph_hash_entry(mac_ctx, sta_ds->staAddr, sta_ds->assocId,
				  session_entry);

skip_event:
	qdf_mem_free(del_sta_params);
	lim_msg->bodyptr = NULL;
}


#endif
