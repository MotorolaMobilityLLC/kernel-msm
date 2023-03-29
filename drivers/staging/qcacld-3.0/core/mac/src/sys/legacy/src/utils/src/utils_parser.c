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
 * This file utils_parser.cc contains the code for parsing
 * 802.11 messages.
 * Author:        Pierre Vandwalle
 * Date:          03/18/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "ani_global.h"
#include "utils_parser.h"
#include "lim_ser_des_utils.h"

void convert_ssid(struct mac_context *mac, tSirMacSSid *pOld, tDot11fIESSID *pNew)
{
	pOld->length = pNew->num_ssid;
	qdf_mem_copy(pOld->ssId, pNew->ssid, pNew->num_ssid);
}

void convert_supp_rates(struct mac_context *mac,
			tSirMacRateSet *pOld, tDot11fIESuppRates *pNew)
{
	pOld->numRates = pNew->num_rates;
	qdf_mem_copy(pOld->rate, pNew->rates, pNew->num_rates);
}

void convert_ext_supp_rates(struct mac_context *mac,
			    tSirMacRateSet *pOld, tDot11fIEExtSuppRates *pNew)
{
	pOld->numRates = pNew->num_rates;
	qdf_mem_copy(pOld->rate, pNew->rates, pNew->num_rates);
}

void convert_qos_caps(struct mac_context *mac,
		      tSirMacQosCapabilityIE *pOld, tDot11fIEQOSCapsAp *pNew)
{
	pOld->type = 46;
	pOld->length = 1;

	pOld->qosInfo.count = pNew->count;
}

void convert_qos_caps_station(struct mac_context *mac,
			      tSirMacQosCapabilityStaIE *pOld,
			      tDot11fIEQOSCapsStation *pNew)
{
	pOld->type = 46;
	pOld->length = 1;

	pOld->qosInfo.moreDataAck = pNew->more_data_ack;
	pOld->qosInfo.maxSpLen = pNew->max_sp_length;
	pOld->qosInfo.qack = pNew->qack;
	pOld->qosInfo.acbe_uapsd = pNew->acbe_uapsd;
	pOld->qosInfo.acbk_uapsd = pNew->acbk_uapsd;
	pOld->qosInfo.acvi_uapsd = pNew->acvi_uapsd;
	pOld->qosInfo.acvo_uapsd = pNew->acvo_uapsd;
}

QDF_STATUS convert_wpa(struct mac_context *mac,
		       tSirMacWpaInfo *pOld, tDot11fIEWPA *pNew)
{
	/* This is awful, I know, but the old code just rammed the IE into an */
	/* array... */
	uint8_t buffer[257];
	uint32_t status, written = 0, nbuffer = 257;

	status = dot11f_pack_ie_wpa(mac, pNew, buffer, nbuffer, &written);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to re-pack the WPA IE (0x%0x8)", status);
		return QDF_STATUS_E_FAILURE;
	}

	pOld->length = (uint8_t) written - 2;
	qdf_mem_copy(pOld->info, buffer + 2, pOld->length);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS convert_wpa_opaque(struct mac_context *mac,
			      tSirMacWpaInfo *pOld, tDot11fIEWPAOpaque *pNew)
{
	/* This is awful, I know, but the old code just rammed the IE into */
	/* an opaque array.  Note that we need to explicitly add the OUI! */
	pOld->length = pNew->num_data + 4;
	pOld->info[0] = 0x00;
	pOld->info[1] = 0x50;
	pOld->info[2] = 0xf2;
	pOld->info[3] = 0x01;
	qdf_mem_copy(pOld->info + 4, pNew->data, pNew->num_data);

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_WAPI
QDF_STATUS convert_wapi_opaque(struct mac_context *mac,
			       tSirMacWapiInfo *pOld,
			       tDot11fIEWAPIOpaque *pNew)
{
	/* This is awful, I know, but the old code just rammed the IE into */
	/* an opaque array.  Note that we need to explicitly add the OUI! */
	pOld->length = pNew->num_data;
	qdf_mem_copy(pOld->info, pNew->data, pNew->num_data);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS convert_wsc_opaque(struct mac_context *mac,
			      tSirAddie *pOld, tDot11fIEWscIEOpaque *pNew)
{
	/* This is awful, I know, but the old code just rammed the IE into */
	/* an opaque array.  Note that we need to explicitly add the vendorIE and OUI ! */
	uint16_t curAddIELen = pOld->length;

	pOld->length = curAddIELen + pNew->num_data + 6;
	pOld->addIEdata[curAddIELen++] = 0xdd;
	pOld->addIEdata[curAddIELen++] = pNew->num_data + 4;
	pOld->addIEdata[curAddIELen++] = 0x00;
	pOld->addIEdata[curAddIELen++] = 0x50;
	pOld->addIEdata[curAddIELen++] = 0xf2;
	pOld->addIEdata[curAddIELen++] = 0x04;
	qdf_mem_copy(pOld->addIEdata + curAddIELen, pNew->data, pNew->num_data);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS convert_p2p_opaque(struct mac_context *mac,
			      tSirAddie *pOld, tDot11fIEP2PIEOpaque *pNew)
{
	/* This is awful, I know, but the old code just rammed the IE into */
	/* an opaque array.  Note that we need to explicitly add the vendorIE and OUI ! */
	uint16_t curAddIELen = pOld->length;

	pOld->length = curAddIELen + pNew->num_data + 6;
	pOld->addIEdata[curAddIELen++] = 0xdd;
	pOld->addIEdata[curAddIELen++] = pNew->num_data + 4;
	pOld->addIEdata[curAddIELen++] = 0x50;
	pOld->addIEdata[curAddIELen++] = 0x6f;
	pOld->addIEdata[curAddIELen++] = 0x9A;
	pOld->addIEdata[curAddIELen++] = 0x09;
	qdf_mem_copy(pOld->addIEdata + curAddIELen, pNew->data, pNew->num_data);

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_WFD
QDF_STATUS convert_wfd_opaque(struct mac_context *mac,
			      tSirAddie *pOld, tDot11fIEWFDIEOpaque *pNew)
{
	/* This is awful, I know, but the old code just rammed the IE into */
	/* an opaque array.  Note that we need to explicitly add the vendorIE and OUI ! */
	uint16_t curAddIELen = pOld->length;

	pOld->length = curAddIELen + pNew->num_data + 6;
	pOld->addIEdata[curAddIELen++] = 0xdd;
	pOld->addIEdata[curAddIELen++] = pNew->num_data + 4;
	pOld->addIEdata[curAddIELen++] = 0x50;
	pOld->addIEdata[curAddIELen++] = 0x6f;
	pOld->addIEdata[curAddIELen++] = 0x9A;
	pOld->addIEdata[curAddIELen++] = 0x0a;
	qdf_mem_copy(pOld->addIEdata + curAddIELen, pNew->data, pNew->num_data);

	return QDF_STATUS_SUCCESS;
}
#endif

QDF_STATUS convert_rsn(struct mac_context *mac,
		       tSirMacRsnInfo *pOld, tDot11fIERSN *pNew)
{
	uint8_t buffer[257];
	uint32_t status, written = 0, nbuffer = 257;

	status = dot11f_pack_ie_rsn(mac, pNew, buffer, nbuffer, &written);
	if (DOT11F_FAILED(status)) {
		pe_err("Failed to re-pack the RSN IE (0x%0x8)", status);
		return QDF_STATUS_E_FAILURE;
	}

	pOld->length = (uint8_t) written - 2;
	qdf_mem_copy(pOld->info, buffer + 2, pOld->length);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS convert_rsn_opaque(struct mac_context *mac,
			      tSirMacRsnInfo *pOld, tDot11fIERSNOpaque *pNew)
{
	/* This is awful, I know, but the old code just rammed the IE into */
	/* an opaque array. */
	pOld->length = pNew->num_data;
	qdf_mem_copy(pOld->info, pNew->data, pOld->length);

	return QDF_STATUS_SUCCESS;
}

void convert_power_caps(struct mac_context *mac,
			tSirMacPowerCapabilityIE *pOld,
			tDot11fIEPowerCaps *pNew)
{
	pOld->type = 33;
	pOld->length = 2;
	pOld->minTxPower = pNew->minTxPower;
	pOld->maxTxPower = pNew->maxTxPower;
}

void convert_supp_channels(struct mac_context *mac,
			   tSirMacSupportedChannelIE *pOld,
			   tDot11fIESuppChannels *pNew)
{
	pOld->type = 36;
	pOld->length = (pNew->num_bands * 2);
	qdf_mem_copy((uint8_t *) pOld->supportedChannels,
		     (uint8_t *) pNew->bands, pOld->length);
}

void convert_cf_params(struct mac_context *mac,
		       tSirMacCfParamSet *pOld, tDot11fIECFParams *pNew)
{
	pOld->cfpCount = pNew->cfp_count;
	pOld->cfpPeriod = pNew->cfp_period;
	pOld->cfpMaxDuration = pNew->cfp_maxduration;
	pOld->cfpDurRemaining = pNew->cfp_durremaining;
}

void convert_fh_params(struct mac_context *mac,
		       tSirMacFHParamSet *pOld, tDot11fIEFHParamSet *pNew)
{
	pOld->dwellTime = pNew->dwell_time;
	pOld->hopSet = pNew->hop_set;
	pOld->hopPattern = pNew->hop_pattern;
	pOld->hopIndex = pNew->hop_index;
}

void convert_tim(struct mac_context *mac, tSirMacTim *pOld, tDot11fIETIM *pNew)
{
	pOld->dtimCount = pNew->dtim_count;
	pOld->dtimPeriod = pNew->dtim_period;
	pOld->bitmapControl = pNew->bmpctl;
	pOld->bitmapLength = pNew->num_vbmp;

	qdf_mem_copy(pOld->bitmap, pNew->vbmp, pNew->num_vbmp);
}

void convert_country(struct mac_context *mac,
		     tSirCountryInformation *pOld, tDot11fIECountry *pNew)
{
	uint8_t i = 0;

	qdf_mem_copy(pOld->countryString, pNew->country, COUNTRY_STRING_LENGTH);

	pOld->numIntervals = pNew->num_more_triplets;

	pOld->channelTransmitPower[i].channelNumber = pNew->first_triplet[0];
	pOld->channelTransmitPower[i].numChannel = pNew->first_triplet[1];
	pOld->channelTransmitPower[i].maxTransmitPower = pNew->first_triplet[2];

	for (i = 0; i < pNew->num_more_triplets; i++) {
		pOld->channelTransmitPower[i + 1].channelNumber =
				pNew->more_triplets[i][0];
		pOld->channelTransmitPower[i + 1].numChannel =
				pNew->more_triplets[i][1];
		pOld->channelTransmitPower[i + 1].maxTransmitPower =
				pNew->more_triplets[i][2];
	}
}

void convert_wmm_params(struct mac_context *mac,
			tSirMacEdcaParamSetIE *pOld, tDot11fIEWMMParams *pNew)
{
	pOld->type = 221;
	pOld->length = 24;

	qdf_mem_copy((uint8_t *) &pOld->qosInfo, (uint8_t *) &pNew->qosInfo,
		     1);

	pOld->acbe.aci.aifsn = pNew->acbe_aifsn;
	pOld->acbe.aci.acm = pNew->acbe_acm;
	pOld->acbe.aci.aci = pNew->acbe_aci;
	pOld->acbe.cw.min = pNew->acbe_acwmin;
	pOld->acbe.cw.max = pNew->acbe_acwmax;
	pOld->acbe.txoplimit = pNew->acbe_txoplimit;

	pOld->acbk.aci.aifsn = pNew->acbk_aifsn;
	pOld->acbk.aci.acm = pNew->acbk_acm;
	pOld->acbk.aci.aci = pNew->acbk_aci;
	pOld->acbk.cw.min = pNew->acbk_acwmin;
	pOld->acbk.cw.max = pNew->acbk_acwmax;
	pOld->acbk.txoplimit = pNew->acbk_txoplimit;

	pOld->acvi.aci.aifsn = pNew->acvi_aifsn;
	pOld->acvi.aci.acm = pNew->acvi_acm;
	pOld->acvi.aci.aci = pNew->acvi_aci;
	pOld->acvi.cw.min = pNew->acvi_acwmin;
	pOld->acvi.cw.max = pNew->acvi_acwmax;
	pOld->acvi.txoplimit = pNew->acvi_txoplimit;

	pOld->acvo.aci.aifsn = pNew->acvo_aifsn;
	pOld->acvo.aci.acm = pNew->acvo_acm;
	pOld->acvo.aci.aci = pNew->acvo_aci;
	pOld->acvo.cw.min = pNew->acvo_acwmin;
	pOld->acvo.cw.max = pNew->acvo_acwmax;
	pOld->acvo.txoplimit = pNew->acvo_txoplimit;
}

void convert_erp_info(struct mac_context *mac,
		      tSirMacErpInfo *pOld, tDot11fIEERPInfo *pNew)
{
	pOld->nonErpPresent = pNew->non_erp_present;
	pOld->useProtection = pNew->use_prot;
	pOld->barkerPreambleMode = pNew->barker_preamble;
}

void convert_edca_param(struct mac_context *mac,
			tSirMacEdcaParamSetIE *pOld,
			tDot11fIEEDCAParamSet *pNew)
{
	pOld->type = 12;
	pOld->length = 20;

	qdf_mem_copy((uint8_t *) &pOld->qosInfo, (uint8_t *) &pNew->qos, 1);

	pOld->acbe.aci.aifsn = pNew->acbe_aifsn;
	pOld->acbe.aci.acm = pNew->acbe_acm;
	pOld->acbe.aci.aci = pNew->acbe_aci;
	pOld->acbe.cw.min = pNew->acbe_acwmin;
	pOld->acbe.cw.max = pNew->acbe_acwmax;
	pOld->acbe.txoplimit = pNew->acbe_txoplimit;

	pOld->acbk.aci.aifsn = pNew->acbk_aifsn;
	pOld->acbk.aci.acm = pNew->acbk_acm;
	pOld->acbk.aci.aci = pNew->acbk_aci;
	pOld->acbk.cw.min = pNew->acbk_acwmin;
	pOld->acbk.cw.max = pNew->acbk_acwmax;
	pOld->acbk.txoplimit = pNew->acbk_txoplimit;

	pOld->acvi.aci.aifsn = pNew->acvi_aifsn;
	pOld->acvi.aci.acm = pNew->acvi_acm;
	pOld->acvi.aci.aci = pNew->acvi_aci;
	pOld->acvi.cw.min = pNew->acvi_acwmin;
	pOld->acvi.cw.max = pNew->acvi_acwmax;
	pOld->acvi.txoplimit = pNew->acvi_txoplimit;

	pOld->acvo.aci.aifsn = pNew->acvo_aifsn;
	pOld->acvo.aci.acm = pNew->acvo_acm;
	pOld->acvo.aci.aci = pNew->acvo_aci;
	pOld->acvo.cw.min = pNew->acvo_acwmin;
	pOld->acvo.cw.max = pNew->acvo_acwmax;
	pOld->acvo.txoplimit = pNew->acvo_txoplimit;

}

void convert_mu_edca_param(struct mac_context *mac_ctx,
			tSirMacEdcaParamSetIE *mu_edca,
			tDot11fIEmu_edca_param_set *ie)
{
	qdf_mem_copy((uint8_t *) &mu_edca->qosInfo, (uint8_t *) &ie->qos, 1);

	mu_edca->acbe.aci.aifsn = ie->acbe_aifsn;
	mu_edca->acbe.aci.acm = ie->acbe_acm;
	mu_edca->acbe.aci.aci = ie->acbe_aci;
	mu_edca->acbe.cw.min = ie->acbe_acwmin;
	mu_edca->acbe.cw.max = ie->acbe_acwmax;
	mu_edca->acbe.mu_edca_timer = ie->acbe_muedca_timer;

	mu_edca->acbk.aci.aifsn = ie->acbk_aifsn;
	mu_edca->acbk.aci.acm = ie->acbk_acm;
	mu_edca->acbk.aci.aci = ie->acbk_aci;
	mu_edca->acbk.cw.min = ie->acbk_acwmin;
	mu_edca->acbk.cw.max = ie->acbk_acwmax;
	mu_edca->acbk.mu_edca_timer = ie->acbk_muedca_timer;

	mu_edca->acvi.aci.aifsn = ie->acvi_aifsn;
	mu_edca->acvi.aci.acm = ie->acvi_acm;
	mu_edca->acvi.aci.aci = ie->acvi_aci;
	mu_edca->acvi.cw.min = ie->acvi_acwmin;
	mu_edca->acvi.cw.max = ie->acvi_acwmax;
	mu_edca->acvi.mu_edca_timer = ie->acvi_muedca_timer;

	mu_edca->acvo.aci.aifsn = ie->acvo_aifsn;
	mu_edca->acvo.aci.acm = ie->acvo_acm;
	mu_edca->acvo.aci.aci = ie->acvo_aci;
	mu_edca->acvo.cw.min = ie->acvo_acwmin;
	mu_edca->acvo.cw.max = ie->acvo_acwmax;
	mu_edca->acvo.mu_edca_timer = ie->acvo_muedca_timer;

}

void convert_tspec(struct mac_context *mac,
		   struct mac_tspec_ie *pOld, tDot11fIETSPEC *pNew)
{
	pOld->tsinfo.traffic.trafficType = (uint16_t) pNew->traffic_type;
	pOld->tsinfo.traffic.tsid = (uint16_t) pNew->tsid;
	pOld->tsinfo.traffic.direction = (uint16_t) pNew->direction;
	pOld->tsinfo.traffic.accessPolicy = (uint16_t) pNew->access_policy;
	pOld->tsinfo.traffic.aggregation = (uint16_t) pNew->aggregation;
	pOld->tsinfo.traffic.psb = (uint16_t) pNew->psb;
	pOld->tsinfo.traffic.userPrio = (uint16_t) pNew->user_priority;
	pOld->tsinfo.traffic.ackPolicy = (uint16_t) pNew->tsinfo_ack_pol;

	pOld->tsinfo.schedule.schedule = (uint8_t) pNew->schedule;

	pOld->nomMsduSz = pNew->size;
	pOld->maxMsduSz = pNew->max_msdu_size;
	pOld->minSvcInterval = pNew->min_service_int;
	pOld->maxSvcInterval = pNew->max_service_int;
	pOld->inactInterval = pNew->inactivity_int;
	pOld->suspendInterval = pNew->suspension_int;
	pOld->svcStartTime = pNew->service_start_time;
	pOld->minDataRate = pNew->min_data_rate;
	pOld->meanDataRate = pNew->mean_data_rate;
	pOld->peakDataRate = pNew->peak_data_rate;
	pOld->maxBurstSz = pNew->burst_size;
	pOld->delayBound = pNew->delay_bound;
	pOld->minPhyRate = pNew->min_phy_rate;
	pOld->surplusBw = pNew->surplus_bw_allowance;
	pOld->mediumTime = pNew->medium_time;
}

QDF_STATUS convert_tclas(struct mac_context *mac,
			 tSirTclasInfo *pOld, tDot11fIETCLAS *pNew)
{
	uint32_t length = 0;

	if (DOT11F_FAILED(dot11f_get_packed_ietclas(mac, pNew, &length))) {
		return QDF_STATUS_E_FAILURE;
	}

	pOld->tclas.type = DOT11F_EID_TCLAS;
	pOld->tclas.length = (uint8_t) length;
	pOld->tclas.userPrio = pNew->user_priority;
	pOld->tclas.classifierType = pNew->classifier_type;
	pOld->tclas.classifierMask = pNew->classifier_mask;

	switch (pNew->classifier_type) {
	case 0:
		qdf_mem_copy(pOld->tclasParams.eth.srcAddr,
			     pNew->info.EthParams.source, 6);
		qdf_mem_copy(pOld->tclasParams.eth.dstAddr,
			     pNew->info.EthParams.dest, 6);
		pOld->tclasParams.eth.type = pNew->info.EthParams.type;
		break;
	case 1:
		pOld->version = pNew->info.IpParams.version;
		if (4 == pNew->info.IpParams.version) {
			pOld->tclasParams.ipv4.version = 4;
			qdf_mem_copy(pOld->tclasParams.ipv4.srcIpAddr,
				     pNew->info.IpParams.params.IpV4Params.
				     source, 4);
			qdf_mem_copy(pOld->tclasParams.ipv4.dstIpAddr,
				     pNew->info.IpParams.params.IpV4Params.dest,
				     4);
			pOld->tclasParams.ipv4.srcPort =
				pNew->info.IpParams.params.IpV4Params.src_port;
			pOld->tclasParams.ipv4.dstPort =
				pNew->info.IpParams.params.IpV4Params.dest_port;
			pOld->tclasParams.ipv4.dscp =
				pNew->info.IpParams.params.IpV4Params.DSCP;
			pOld->tclasParams.ipv4.protocol =
				pNew->info.IpParams.params.IpV4Params.proto;
			pOld->tclasParams.ipv4.rsvd =
				pNew->info.IpParams.params.IpV4Params.reserved;
		} else if (6 == pNew->info.IpParams.version) {
			pOld->tclasParams.ipv6.version = 6;
			qdf_mem_copy((uint8_t *) pOld->tclasParams.ipv6.
				     srcIpAddr,
				     (uint8_t *) pNew->info.IpParams.params.
				     IpV6Params.source, 16);
			qdf_mem_copy((uint8_t *) pOld->tclasParams.ipv6.
				     dstIpAddr,
				     (uint8_t *) pNew->info.IpParams.params.
				     IpV6Params.dest, 16);
			pOld->tclasParams.ipv6.srcPort =
				pNew->info.IpParams.params.IpV6Params.src_port;
			pOld->tclasParams.ipv6.dstPort =
				pNew->info.IpParams.params.IpV6Params.dest_port;
			qdf_mem_copy((uint8_t *) pOld->tclasParams.ipv6.
				     flowLabel,
				     (uint8_t *) pNew->info.IpParams.params.
				     IpV6Params.flow_label, 3);
		} else {
			return QDF_STATUS_E_FAILURE;
		}
		break;
	case 2:
		pOld->tclasParams.t8021dq.tag =
			pNew->info.Params8021dq.tag_type;
		break;
	default:
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void convert_wmmtspec(struct mac_context *mac,
		      struct mac_tspec_ie *pOld, tDot11fIEWMMTSPEC *pNew)
{
	pOld->tsinfo.traffic.trafficType = (uint16_t) pNew->traffic_type;
	pOld->tsinfo.traffic.tsid = (uint16_t) pNew->tsid;
	pOld->tsinfo.traffic.direction = (uint16_t) pNew->direction;
	pOld->tsinfo.traffic.accessPolicy = (uint16_t) pNew->access_policy;
	pOld->tsinfo.traffic.aggregation = (uint16_t) pNew->aggregation;
	pOld->tsinfo.traffic.psb = (uint16_t) pNew->psb;
	pOld->tsinfo.traffic.userPrio = (uint16_t) pNew->user_priority;
	pOld->tsinfo.traffic.ackPolicy = (uint16_t) pNew->tsinfo_ack_pol;
	pOld->nomMsduSz = (pNew->fixed << 15) | pNew->size;
	pOld->maxMsduSz = pNew->max_msdu_size;
	pOld->minSvcInterval = pNew->min_service_int;
	pOld->maxSvcInterval = pNew->max_service_int;
	pOld->inactInterval = pNew->inactivity_int;
	pOld->suspendInterval = pNew->suspension_int;
	pOld->svcStartTime = pNew->service_start_time;
	pOld->minDataRate = pNew->min_data_rate;
	pOld->meanDataRate = pNew->mean_data_rate;
	pOld->peakDataRate = pNew->peak_data_rate;
	pOld->maxBurstSz = pNew->burst_size;
	pOld->delayBound = pNew->delay_bound;
	pOld->minPhyRate = pNew->min_phy_rate;
	pOld->surplusBw = pNew->surplus_bw_allowance;
	pOld->mediumTime = pNew->medium_time;
}

QDF_STATUS convert_wmmtclas(struct mac_context *mac,
			    tSirTclasInfo *pOld, tDot11fIEWMMTCLAS *pNew)
{
	uint32_t length = 0;

	if (DOT11F_FAILED(dot11f_get_packed_iewmmtclas(mac, pNew, &length))) {
		return QDF_STATUS_E_FAILURE;
	}

	pOld->tclas.type = DOT11F_EID_WMMTCLAS;
	pOld->tclas.length = (uint8_t) length;
	pOld->tclas.userPrio = pNew->user_priority;
	pOld->tclas.classifierType = pNew->classifier_type;
	pOld->tclas.classifierMask = pNew->classifier_mask;

	switch (pNew->classifier_type) {
	case 0:
		qdf_mem_copy(pOld->tclasParams.eth.srcAddr,
			     pNew->info.EthParams.source, 6);
		qdf_mem_copy(pOld->tclasParams.eth.dstAddr,
			     pNew->info.EthParams.dest, 6);
		pOld->tclasParams.eth.type = pNew->info.EthParams.type;
		break;
	case 1:
		pOld->version = pNew->info.IpParams.version;
		if (4 == pNew->info.IpParams.version) {
			pOld->tclasParams.ipv4.version = 4;
			qdf_mem_copy(pOld->tclasParams.ipv4.srcIpAddr,
				     pNew->info.IpParams.params.IpV4Params.
				     source, 4);
			qdf_mem_copy(pOld->tclasParams.ipv4.dstIpAddr,
				     pNew->info.IpParams.params.IpV4Params.dest,
				     4);
			pOld->tclasParams.ipv4.srcPort =
				pNew->info.IpParams.params.IpV4Params.src_port;
			pOld->tclasParams.ipv4.dstPort =
				pNew->info.IpParams.params.IpV4Params.dest_port;
			pOld->tclasParams.ipv4.dscp =
				pNew->info.IpParams.params.IpV4Params.DSCP;
			pOld->tclasParams.ipv4.protocol =
				pNew->info.IpParams.params.IpV4Params.proto;
			pOld->tclasParams.ipv4.rsvd =
				pNew->info.IpParams.params.IpV4Params.reserved;
		} else if (6 == pNew->info.IpParams.version) {
			pOld->tclasParams.ipv6.version = 6;
			qdf_mem_copy((uint8_t *) pOld->tclasParams.ipv6.
				     srcIpAddr,
				     (uint8_t *) pNew->info.IpParams.params.
				     IpV6Params.source, 16);
			qdf_mem_copy((uint8_t *) pOld->tclasParams.ipv6.
				     dstIpAddr,
				     (uint8_t *) pNew->info.IpParams.params.
				     IpV6Params.dest, 16);
			pOld->tclasParams.ipv6.srcPort =
				pNew->info.IpParams.params.IpV6Params.src_port;
			pOld->tclasParams.ipv6.dstPort =
				pNew->info.IpParams.params.IpV6Params.dest_port;
			qdf_mem_copy((uint8_t *) pOld->tclasParams.ipv6.
				     flowLabel,
				     (uint8_t *) pNew->info.IpParams.params.
				     IpV6Params.flow_label, 3);
		} else {
			return QDF_STATUS_E_FAILURE;
		}
		break;
	case 2:
		pOld->tclasParams.t8021dq.tag =
			pNew->info.Params8021dq.tag_type;
		break;
	default:
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

void convert_ts_delay(struct mac_context *mac,
		      tSirMacTsDelayIE *pOld, tDot11fIETSDelay *pNew)
{
	pOld->type = DOT11F_EID_TSDELAY;
	pOld->length = 4U;
	pOld->delay = pNew->delay;
}

void convert_schedule(struct mac_context *mac,
		      tSirMacScheduleIE *pOld, tDot11fIESchedule *pNew)
{
	pOld->type = DOT11F_EID_SCHEDULE;
	pOld->length = DOT11F_IE_SCHEDULE_MIN_LEN;

	pOld->info.aggregation = pNew->aggregation;
	pOld->info.tsid = pNew->tsid;
	pOld->info.direction = pNew->direction;

	pOld->svcStartTime = pNew->service_start_time;
	pOld->svcInterval = pNew->service_interval;
	pOld->specInterval = pNew->spec_interval;
}

void convert_wmm_schedule(struct mac_context *mac,
			  tSirMacScheduleIE *pOld, tDot11fIEWMMSchedule *pNew)
{
	pOld->type = DOT11F_EID_WMMSCHEDULE;
	pOld->length = DOT11F_IE_WMMSCHEDULE_MIN_LEN;

	pOld->info.aggregation = pNew->aggregation;
	pOld->info.tsid = pNew->tsid;
	pOld->info.direction = pNew->direction;

	pOld->svcStartTime = pNew->service_start_time;
	pOld->svcInterval = pNew->service_interval;
	pOld->specInterval = pNew->spec_interval;
}

void convert_qos_mapset_frame(struct mac_context *mac, struct qos_map_set *qos,
			      tDot11fIEQosMapSet *dot11f_ie)
{
	uint8_t i, j = 0;
	uint8_t dot11_dscp_exception_sz;

	if (dot11f_ie->num_dscp_exceptions < DOT11F_IE_QOSMAPSET_MIN_LEN ||
	    dot11f_ie->num_dscp_exceptions % 2) {
		pe_debug("Invalid num_dscp_exceptions %d",
			 dot11f_ie->num_dscp_exceptions);
		return;
	}

	dot11_dscp_exception_sz = dot11f_ie->num_dscp_exceptions -
				  DOT11F_IE_QOSMAPSET_MIN_LEN;
	qos->num_dscp_exceptions = dot11_dscp_exception_sz / 2;
	if (qos->num_dscp_exceptions > QOS_MAP_MAX_EX)
		qos->num_dscp_exceptions = QOS_MAP_MAX_EX;

	for (i = 0; i < qos->num_dscp_exceptions &&
	     j < dot11_dscp_exception_sz - 1; i++) {
		qos->dscp_exceptions[i][0] = dot11f_ie->dscp_exceptions[j++];
		qos->dscp_exceptions[i][1] = dot11f_ie->dscp_exceptions[j++];
	}

	for (i = 0; i < QOS_MAP_RANGE_NUM &&
	     j < dot11f_ie->num_dscp_exceptions - 1; i++) {
		qos->dscp_range[i][0] = dot11f_ie->dscp_exceptions[j++];
		qos->dscp_range[i][1] = dot11f_ie->dscp_exceptions[j++];
	}
}

/* utils_parser.c ends here. */
