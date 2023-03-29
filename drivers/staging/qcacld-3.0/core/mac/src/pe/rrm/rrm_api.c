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

/**=========================================================================

   \file  rrm_api.c

   \brief implementation for PE RRM APIs

   ========================================================================*/

/* $Header$ */


/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include "cds_api.h"
#include "wni_api.h"
#include "sir_api.h"
#include "ani_global.h"
#include "wni_cfg.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_send_sme_rsp_messages.h"
#include "parser_api.h"
#include "lim_send_messages.h"
#include "rrm_global.h"
#include "rrm_api.h"
#include "wlan_lmac_if_def.h"
#include "wlan_reg_services_api.h"

/* -------------------------------------------------------------------- */
/**
 * rrm_cache_mgmt_tx_power
 **
 * FUNCTION:  Store Tx power for management frames.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pe_session session entry.
 * @return None
 */
void
rrm_cache_mgmt_tx_power(struct mac_context *mac, int8_t txPower,
			struct pe_session *pe_session)
{
	pe_debug("Cache Mgmt Tx Power: %d", txPower);

	if (!pe_session)
		mac->rrm.rrmPEContext.txMgmtPower = txPower;
	else
		pe_session->txMgmtPower = txPower;
}

/* -------------------------------------------------------------------- */
/**
 * rrm_get_mgmt_tx_power
 *
 * FUNCTION:  Get the Tx power for management frames.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pe_session session entry.
 * @return txPower
 */
int8_t rrm_get_mgmt_tx_power(struct mac_context *mac, struct pe_session *pe_session)
{
	if (!pe_session)
		return mac->rrm.rrmPEContext.txMgmtPower;

	pe_debug("tx mgmt pwr %d", pe_session->txMgmtPower);

	return pe_session->txMgmtPower;
}

/* -------------------------------------------------------------------- */
/**
 * rrm_send_set_max_tx_power_req
 *
 * FUNCTION:  Send WMA_SET_MAX_TX_POWER_REQ message to change the max tx power.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param txPower txPower to be set.
 * @param pe_session session entry.
 * @return None
 */
QDF_STATUS
rrm_send_set_max_tx_power_req(struct mac_context *mac, int8_t txPower,
			      struct pe_session *pe_session)
{
	tpMaxTxPowerParams pMaxTxParams;
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	struct scheduler_msg msgQ = {0};

	if (!pe_session) {
		pe_err("Invalid parameters");
		return QDF_STATUS_E_FAILURE;
	}
	pMaxTxParams = qdf_mem_malloc(sizeof(tMaxTxPowerParams));
	if (!pMaxTxParams)
		return QDF_STATUS_E_NOMEM;
	/* Allocated memory for pMaxTxParams...will be freed in other module */
	pMaxTxParams->power = txPower;
	qdf_mem_copy(pMaxTxParams->bssId.bytes, pe_session->bssId,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(pMaxTxParams->selfStaMacAddr.bytes,
			pe_session->self_mac_addr,
			QDF_MAC_ADDR_SIZE);

	msgQ.type = WMA_SET_MAX_TX_POWER_REQ;
	msgQ.reserved = 0;
	msgQ.bodyptr = pMaxTxParams;
	msgQ.bodyval = 0;

	pe_debug("Sending WMA_SET_MAX_TX_POWER_REQ with power(%d) to HAL",
		txPower);

	MTRACE(mac_trace_msg_tx(mac, pe_session->peSessionId, msgQ.type));
	retCode = wma_post_ctrl_msg(mac, &msgQ);
	if (QDF_STATUS_SUCCESS != retCode) {
		pe_err("Posting WMA_SET_MAX_TX_POWER_REQ to HAL failed, reason=%X",
			retCode);
		qdf_mem_free(pMaxTxParams);
		return retCode;
	}
	return retCode;
}

/* -------------------------------------------------------------------- */
/**
 * rrm_set_max_tx_power_rsp
 *
 * FUNCTION:  Process WMA_SET_MAX_TX_POWER_RSP message.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param txPower txPower to be set.
 * @param pe_session session entry.
 * @return None
 */
QDF_STATUS rrm_set_max_tx_power_rsp(struct mac_context *mac,
				    struct scheduler_msg *limMsgQ)
{
	QDF_STATUS retCode = QDF_STATUS_SUCCESS;
	tpMaxTxPowerParams pMaxTxParams = (tpMaxTxPowerParams) limMsgQ->bodyptr;
	struct pe_session *pe_session;
	uint8_t sessionId, i;

	if (qdf_is_macaddr_broadcast(&pMaxTxParams->bssId)) {
		for (i = 0; i < mac->lim.maxBssId; i++) {
			if (mac->lim.gpSession[i].valid == true) {
				pe_session = &mac->lim.gpSession[i];
				rrm_cache_mgmt_tx_power(mac, pMaxTxParams->power,
							pe_session);
			}
		}
	} else {
		pe_session = pe_find_session_by_bssid(mac,
							 pMaxTxParams->bssId.bytes,
							 &sessionId);
		if (!pe_session) {
			retCode = QDF_STATUS_E_FAILURE;
		} else {
			rrm_cache_mgmt_tx_power(mac, pMaxTxParams->power,
						pe_session);
		}
	}

	qdf_mem_free(limMsgQ->bodyptr);
	limMsgQ->bodyptr = NULL;
	return retCode;
}

/**
 * rrm_calculate_and_fill_rcpi() - calculates and fills RCPI value
 * @rcpi: pointer to hold calculated RCPI value
 * @cur_rssi: value of current RSSI
 *
 * @return None
 */
static void rrm_calculate_and_fill_rcpi(uint8_t *rcpi, int8_t cur_rssi)
{
	/* 2008 11k spec reference: 18.4.8.5 RCPI Measurement */
	if (cur_rssi <= RCPI_LOW_RSSI_VALUE)
		*rcpi = 0;
	else if ((cur_rssi > RCPI_LOW_RSSI_VALUE) && (cur_rssi <= 0))
		*rcpi = CALCULATE_RCPI(cur_rssi);
	else
		*rcpi = RCPI_MAX_VALUE;
}

/* -------------------------------------------------------------------- */
/**
 * rrm_process_link_measurement_request
 *
 * FUNCTION:  Processes the Link measurement request and send the report.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pBd pointer to BD to extract RSSI and SNR
 * @param pLinkReq pointer to the Link request frame structure.
 * @param pe_session session entry.
 * @return None
 */
QDF_STATUS
rrm_process_link_measurement_request(struct mac_context *mac,
				     uint8_t *pRxPacketInfo,
				     tDot11fLinkMeasurementRequest *pLinkReq,
				     struct pe_session *pe_session)
{
	tSirMacLinkReport LinkReport = {0};
	tpSirMacMgmtHdr pHdr;
	int8_t currentRSSI = 0;
	struct vdev_mlme_obj *mlme_obj;
	struct wlan_lmac_if_reg_tx_ops *tx_ops;
	uint8_t ap_pwr_constraint = 0;

	pe_debug("Received Link measurement request");

	if (!pRxPacketInfo || !pLinkReq || !pe_session) {
		pe_err("Invalid parameters - Ignoring the request");
		return QDF_STATUS_E_FAILURE;
	}
	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(pe_session->vdev);
	if (!mlme_obj) {
		pe_err("vdev component object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_reg_is_ext_tpc_supported(mac->psoc)) {
		ap_pwr_constraint = mlme_obj->reg_tpc_obj.ap_constraint_power;
		mlme_obj->reg_tpc_obj.ap_constraint_power =
				pLinkReq->MaxTxPower.maxTxPower;
		lim_calculate_tpc(mac, pe_session, true, 0, false);

		LinkReport.txPower =
			mlme_obj->reg_tpc_obj.chan_power_info[0].tx_power;
		if (LinkReport.txPower < MIN_TX_PWR_CAP)
			LinkReport.txPower = MIN_TX_PWR_CAP;
		else if (LinkReport.txPower > MAX_TX_PWR_CAP)
			LinkReport.txPower = MAX_TX_PWR_CAP;

		if (pLinkReq->MaxTxPower.maxTxPower != ap_pwr_constraint) {
			tx_ops = wlan_reg_get_tx_ops(mac->psoc);

			if (tx_ops->set_tpc_power)
				tx_ops->set_tpc_power(mac->psoc,
						      pe_session->vdev_id,
						      &mlme_obj->reg_tpc_obj);
		}
	} else {
		mlme_obj->reg_tpc_obj.reg_max[0] =
				pe_session->def_max_tx_pwr;
		mlme_obj->reg_tpc_obj.ap_constraint_power =
				pLinkReq->MaxTxPower.maxTxPower;

		LinkReport.txPower = lim_get_max_tx_power(mac, mlme_obj);

		/** If firmware updated max tx power is non zero, respond to
		 * rrm link  measurement request with min of firmware updated
		 * ap tx power and max power derived from lim_get_max_tx_power
		 * API.
		 */
		if (mlme_obj && mlme_obj->mgmt.generic.tx_pwrlimit)
			LinkReport.txPower = QDF_MIN(LinkReport.txPower,
					mlme_obj->mgmt.generic.tx_pwrlimit);

		if ((LinkReport.txPower != (uint8_t)pe_session->maxTxPower) &&
		    (QDF_STATUS_SUCCESS ==
			rrm_send_set_max_tx_power_req(mac, LinkReport.txPower,
						      pe_session))) {
			pe_warn("Local: %d", pe_session->maxTxPower);
			pe_session->maxTxPower = LinkReport.txPower;
		}
	}
	pe_warn("Link Request Tx Pwr: %d Link Report Tx Pwr: %d",
		pLinkReq->MaxTxPower.maxTxPower, LinkReport.txPower);

	LinkReport.dialogToken = pLinkReq->DialogToken.token;
	LinkReport.rxAntenna = 0;
	LinkReport.txAntenna = 0;
	currentRSSI = WMA_GET_RX_RSSI_RAW(pRxPacketInfo);

	pe_info("Received Link report frame with %d", currentRSSI);

	rrm_calculate_and_fill_rcpi(&LinkReport.rcpi, currentRSSI);
	LinkReport.rsni = WMA_GET_RX_SNR(pRxPacketInfo);

	pe_debug("Sending Link report frame");

	return lim_send_link_report_action_frame(mac, &LinkReport, pHdr->sa,
						 pe_session);
}

/* -------------------------------------------------------------------- */
/**
 * rrm_process_neighbor_report_response
 *
 * FUNCTION:  Processes the Neighbor Report response from the peer AP.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pNeighborRep pointer to the Neighbor report frame structure.
 * @param pe_session session entry.
 * @return None
 */
QDF_STATUS
rrm_process_neighbor_report_response(struct mac_context *mac,
				     tDot11fNeighborReportResponse *pNeighborRep,
				     struct pe_session *pe_session)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpSirNeighborReportInd pSmeNeighborRpt = NULL;
	uint16_t length;
	uint8_t i;
	struct scheduler_msg mmhMsg = {0};

	if (!pNeighborRep || !pe_session) {
		pe_err("Invalid parameters");
		return status;
	}

	pe_debug("Neighbor report response received");

	/* Dialog token */
	if (mac->rrm.rrmPEContext.DialogToken !=
	    pNeighborRep->DialogToken.token) {
		pe_err("Dialog token mismatch in the received Neighbor report");
		return QDF_STATUS_E_FAILURE;
	}
	if (pNeighborRep->num_NeighborReport == 0) {
		pe_err("No neighbor report in the frame...Dropping it");
		return QDF_STATUS_E_FAILURE;
	}
	pe_debug("RRM:received num neighbor reports: %d",
			pNeighborRep->num_NeighborReport);
	if (pNeighborRep->num_NeighborReport > MAX_SUPPORTED_NEIGHBOR_RPT)
		pNeighborRep->num_NeighborReport = MAX_SUPPORTED_NEIGHBOR_RPT;
	length = (sizeof(tSirNeighborReportInd)) +
		 (sizeof(tSirNeighborBssDescription) *
		  (pNeighborRep->num_NeighborReport - 1));

	/* Prepare the request to send to SME. */
	pSmeNeighborRpt = qdf_mem_malloc(length);
	if (!pSmeNeighborRpt)
		return QDF_STATUS_E_NOMEM;

	/* Allocated memory for pSmeNeighborRpt...will be freed by other module */

	for (i = 0; i < pNeighborRep->num_NeighborReport; i++) {
		pSmeNeighborRpt->sNeighborBssDescription[i].length = sizeof(tSirNeighborBssDescription);        /*+ any optional ies */
		qdf_mem_copy(pSmeNeighborRpt->sNeighborBssDescription[i].bssId,
			     pNeighborRep->NeighborReport[i].bssid,
			     sizeof(tSirMacAddr));
		pSmeNeighborRpt->sNeighborBssDescription[i].bssidInfo.rrmInfo.
		fApPreauthReachable =
			pNeighborRep->NeighborReport[i].APReachability;
		pSmeNeighborRpt->sNeighborBssDescription[i].bssidInfo.rrmInfo.
		fSameSecurityMode =
			pNeighborRep->NeighborReport[i].Security;
		pSmeNeighborRpt->sNeighborBssDescription[i].bssidInfo.rrmInfo.
		fSameAuthenticator =
			pNeighborRep->NeighborReport[i].KeyScope;
		pSmeNeighborRpt->sNeighborBssDescription[i].bssidInfo.rrmInfo.
		fCapSpectrumMeasurement =
			pNeighborRep->NeighborReport[i].SpecMgmtCap;
		pSmeNeighborRpt->sNeighborBssDescription[i].bssidInfo.rrmInfo.
		fCapQos = pNeighborRep->NeighborReport[i].QosCap;
		pSmeNeighborRpt->sNeighborBssDescription[i].bssidInfo.rrmInfo.
		fCapApsd = pNeighborRep->NeighborReport[i].apsd;
		pSmeNeighborRpt->sNeighborBssDescription[i].bssidInfo.rrmInfo.
		fCapRadioMeasurement = pNeighborRep->NeighborReport[i].rrm;
		pSmeNeighborRpt->sNeighborBssDescription[i].bssidInfo.rrmInfo.
		fCapDelayedBlockAck =
			pNeighborRep->NeighborReport[i].DelayedBA;
		pSmeNeighborRpt->sNeighborBssDescription[i].bssidInfo.rrmInfo.
		fCapImmediateBlockAck =
			pNeighborRep->NeighborReport[i].ImmBA;
		pSmeNeighborRpt->sNeighborBssDescription[i].bssidInfo.rrmInfo.
		fMobilityDomain =
			pNeighborRep->NeighborReport[i].MobilityDomain;

		if (!wlan_reg_is_6ghz_supported(mac->psoc) &&
		    (wlan_reg_is_6ghz_op_class(mac->pdev,
					       pNeighborRep->NeighborReport[i].
					       regulatoryClass))) {
			pe_err("channel belongs to 6 ghz spectrum, abort");
			qdf_mem_free(pSmeNeighborRpt);
			return QDF_STATUS_E_FAILURE;
		}

		pSmeNeighborRpt->sNeighborBssDescription[i].regClass =
			pNeighborRep->NeighborReport[i].regulatoryClass;
		pSmeNeighborRpt->sNeighborBssDescription[i].channel =
			pNeighborRep->NeighborReport[i].channel;
		pSmeNeighborRpt->sNeighborBssDescription[i].phyType =
			pNeighborRep->NeighborReport[i].PhyType;
	}

	pSmeNeighborRpt->messageType = eWNI_SME_NEIGHBOR_REPORT_IND;
	pSmeNeighborRpt->length = length;
	pSmeNeighborRpt->measurement_idx = DEFAULT_RRM_IDX;
	pSmeNeighborRpt->sessionId = pe_session->smeSessionId;
	pSmeNeighborRpt->numNeighborReports = pNeighborRep->num_NeighborReport;
	qdf_mem_copy(pSmeNeighborRpt->bssId, pe_session->bssId,
		     sizeof(tSirMacAddr));

	/* Send request to SME. */
	mmhMsg.type = pSmeNeighborRpt->messageType;
	mmhMsg.bodyptr = pSmeNeighborRpt;
	MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
			 pe_session->peSessionId, mmhMsg.type));
	lim_sys_process_mmh_msg_api(mac, &mmhMsg);

	return status;

}

/* -------------------------------------------------------------------- */
/**
 * rrm_process_neighbor_report_req
 *
 * FUNCTION:
 *
 * LOGIC: Create a Neighbor report request and send it to peer.
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pNeighborReq Neighbor report request params .
 * @return None
 */
QDF_STATUS
rrm_process_neighbor_report_req(struct mac_context *mac,
				tpSirNeighborReportReqInd pNeighborReq)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirMacNeighborReportReq NeighborReportReq;
	struct pe_session *pe_session;
	uint8_t sessionId;

	if (!pNeighborReq) {
		pe_err("NeighborReq is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	pe_session = pe_find_session_by_bssid(mac, pNeighborReq->bssId,
						 &sessionId);
	if (!pe_session) {
		pe_err("session does not exist for given bssId");
		return QDF_STATUS_E_FAILURE;
	}

	pe_debug("SSID present: %d", pNeighborReq->noSSID);

	qdf_mem_zero(&NeighborReportReq, sizeof(tSirMacNeighborReportReq));

	NeighborReportReq.dialogToken = ++mac->rrm.rrmPEContext.DialogToken;
	NeighborReportReq.ssid_present = !pNeighborReq->noSSID;
	if (NeighborReportReq.ssid_present) {
		qdf_mem_copy(&NeighborReportReq.ssid, &pNeighborReq->ucSSID,
			     sizeof(tSirMacSSid));
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_PE,
				   QDF_TRACE_LEVEL_DEBUG,
				   (uint8_t *) NeighborReportReq.ssid.ssId,
				   NeighborReportReq.ssid.length);
	}

	status =
		lim_send_neighbor_report_request_frame(mac, &NeighborReportReq,
						       pNeighborReq->bssId,
						       pe_session);

	return status;
}

/**
 * rrm_get_country_code_from_connected_profile() - Get country code
 * from connected profile
 * @mac: Mac context
 * @pe_session: pe session
 * @country_code: country code
 *
 * Return: void
 */
static inline void
rrm_get_country_code_from_connected_profile(
				struct mac_context *mac,
				struct pe_session *pe_session,
				uint8_t country_code[WNI_CFG_COUNTRY_CODE_LEN])
{
	uint8_t id;
	uint8_t *country;

	qdf_mem_zero(country_code, sizeof(country_code[0]) *
					WNI_CFG_COUNTRY_CODE_LEN);
	if (!pe_session) {
		pe_err("pe_session is NULL");
		return;
	}
	id = pe_session->smeSessionId;
	if (!CSR_IS_SESSION_VALID(mac, id)) {
		pe_err("smeSessionId %d is invalid", id);
		return;
	}
	country =
		mac->roam.roamSession[id].connectedProfile.country_code;
	if (country[0])
		qdf_mem_copy(country_code, country, sizeof(country_code[0]) *
						WNI_CFG_COUNTRY_CODE_LEN);
	else
		country_code[2] = OP_CLASS_GLOBAL;
}

#define ABS(x)      ((x < 0) ? -x : x)
/* -------------------------------------------------------------------- */
/**
 * rrm_process_beacon_report_req
 *
 * FUNCTION:  Processes the Beacon report request from the peer AP.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pCurrentReq pointer to the current Req comtext.
 * @param pBeaconReq pointer to the beacon report request IE from the peer.
 * @param pe_session session entry.
 * @return None
 */
static tRrmRetStatus
rrm_process_beacon_report_req(struct mac_context *mac,
			      tpRRMReq pCurrentReq,
			      tDot11fIEMeasurementRequest *pBeaconReq,
			      struct pe_session *pe_session)
{
	struct scheduler_msg mmh_msg = {0};
	tpSirBeaconReportReqInd psbrr;
	uint8_t num_rpt, idx_rpt;
	uint16_t measDuration, maxMeasduration;
	int8_t maxDuration;
	uint8_t sign;
	tDot11fIEAPChannelReport *ie_ap_chan_rpt;
	uint8_t tmp_idx, buf_left, buf_cons;
	uint16_t ch_ctr = 0;
	char ch_buf[RRM_CH_BUF_LEN];
	char *tmp_buf = NULL;
	uint8_t country[WNI_CFG_COUNTRY_CODE_LEN];

	if (!pe_session) {
		pe_err("pe_session is NULL");
		return eRRM_INCAPABLE;
	}

	if (pBeaconReq->measurement_request.Beacon.BeaconReporting.present &&
	    (pBeaconReq->measurement_request.Beacon.BeaconReporting.
	     reportingCondition != 0)) {
		/* Repeated measurement is not supported. This means number of repetitions should be zero.(Already checked) */
		/* All test case in VoWifi(as of version 0.36)  use zero for number of repetitions. */
		/* Beacon reporting should not be included in request if number of repetitons is zero. */
		/* IEEE Std 802.11k-2008 Table 7-29g and section 11.10.8.1 */

		pe_nofl_err("RX: [802.11 BCN_RPT] Dropping req: Reporting condition included is not zero");
		return eRRM_INCAPABLE;
	}

	/* The logic here is to check the measurement duration passed in the beacon request. Following are the cases handled.
	   Case 1: If measurement duration received in the beacon request is greater than the max measurement duration advertised
	   in the RRM capabilities(Assoc Req), and Duration Mandatory bit is set to 1, REFUSE the beacon request
	   Case 2: If measurement duration received in the beacon request is greater than the max measurement duration advertised
	   in the RRM capabilities(Assoc Req), and Duration Mandatory bit is set to 0, perform measurement for
	   the duration advertised in the RRM capabilities

	   maxMeasurementDuration = 2^(nonOperatingChanMax - 4) * BeaconInterval
	 */
	maxDuration =
		mac->rrm.rrmPEContext.rrmEnabledCaps.nonOperatingChanMax - 4;
	sign = (maxDuration < 0) ? 1 : 0;
	maxDuration = (1L << ABS(maxDuration));
	if (!sign)
		maxMeasduration =
			maxDuration * pe_session->beaconParams.beaconInterval;
	else
		maxMeasduration =
			pe_session->beaconParams.beaconInterval / maxDuration;

	if( pBeaconReq->measurement_request.Beacon.meas_mode ==
	   eSIR_PASSIVE_SCAN)
		maxMeasduration += 10;

	measDuration = pBeaconReq->measurement_request.Beacon.meas_duration;

	pe_nofl_info("RX: [802.11 BCN_RPT] seq:%d SSID:%.*s BSSID:"QDF_MAC_ADDR_FMT" Token:%d op_class:%d ch:%d meas_mode:%d meas_duration:%d max_dur: %d sign: %d max_meas_dur: %d",
		     mac->rrm.rrmPEContext.prev_rrm_report_seq_num,
		     pBeaconReq->measurement_request.Beacon.SSID.num_ssid,
		     pBeaconReq->measurement_request.Beacon.SSID.ssid,
		     QDF_MAC_ADDR_REF(pBeaconReq->measurement_request.Beacon.BSSID),
		     pBeaconReq->measurement_token,
		     pBeaconReq->measurement_request.Beacon.regClass,
		     pBeaconReq->measurement_request.Beacon.channel,
		     pBeaconReq->measurement_request.Beacon.meas_mode,
		     measDuration, maxDuration, sign, maxMeasduration);

	if (measDuration == 0 &&
	    pBeaconReq->measurement_request.Beacon.meas_mode !=
	    eSIR_BEACON_TABLE) {
		pe_nofl_err("RX: [802.11 BCN_RPT] Invalid measurement duration");
		return eRRM_REFUSED;
	}

	if (maxMeasduration < measDuration) {
		if (pBeaconReq->durationMandatory) {
			pe_nofl_err("RX: [802.11 BCN_RPT] Dropping the req: duration mandatory & maxduration > measduration");
			return eRRM_REFUSED;
		} else
			measDuration = maxMeasduration;
	}

	pe_debug("measurement duration %d", measDuration);

	/* Cache the data required for sending report. */
	pCurrentReq->request.Beacon.reportingDetail =
		pBeaconReq->measurement_request.Beacon.BcnReportingDetail.
		present ? pBeaconReq->measurement_request.Beacon.BcnReportingDetail.
		reportingDetail : BEACON_REPORTING_DETAIL_ALL_FF_IE;

	if (pBeaconReq->measurement_request.Beacon.
	    last_beacon_report_indication.present) {
		pCurrentReq->request.Beacon.last_beacon_report_indication =
			pBeaconReq->measurement_request.Beacon.
			last_beacon_report_indication.last_fragment;
		pe_debug("RX: [802.11 BCN_RPT] Last Bcn Report in the req: %d",
		     pCurrentReq->request.Beacon.last_beacon_report_indication);
	} else {
		pCurrentReq->request.Beacon.last_beacon_report_indication = 0;
		pe_debug("RX: [802.11 BCN_RPT] Last Bcn rpt ind not present");
	}

	if (pBeaconReq->measurement_request.Beacon.RequestedInfo.present) {
		if (!pBeaconReq->measurement_request.Beacon.RequestedInfo.
		    num_requested_eids) {
			pe_debug("RX: [802.11 BCN_RPT]: Requested num of EID is 0");
			return eRRM_FAILURE;
		}
		pCurrentReq->request.Beacon.reqIes.pElementIds =
			qdf_mem_malloc(sizeof(uint8_t) *
				       pBeaconReq->measurement_request.Beacon.
				       RequestedInfo.num_requested_eids);
		if (!pCurrentReq->request.Beacon.reqIes.pElementIds)
			return eRRM_FAILURE;

		pCurrentReq->request.Beacon.reqIes.num =
			pBeaconReq->measurement_request.Beacon.RequestedInfo.
			num_requested_eids;
		qdf_mem_copy(pCurrentReq->request.Beacon.reqIes.pElementIds,
			     pBeaconReq->measurement_request.Beacon.
			     RequestedInfo.requested_eids,
			     pCurrentReq->request.Beacon.reqIes.num);
	}

	/* Prepare the request to send to SME. */
	psbrr = qdf_mem_malloc(sizeof(tSirBeaconReportReqInd));
	if (!psbrr)
		return eRRM_FAILURE;

	/* Alloc memory for pSmeBcnReportReq, will be freed by other modules */
	qdf_mem_copy(psbrr->bssId, pe_session->bssId,
		     sizeof(tSirMacAddr));
	psbrr->messageType = eWNI_SME_BEACON_REPORT_REQ_IND;
	psbrr->length = sizeof(tSirBeaconReportReqInd);
	psbrr->uDialogToken = pBeaconReq->measurement_token;
	psbrr->msgSource = eRRM_MSG_SOURCE_11K;
	psbrr->randomizationInterval =
		SYS_TU_TO_MS(pBeaconReq->measurement_request.Beacon.randomization);
	psbrr->measurement_idx = pCurrentReq->measurement_idx;

	if (!wlan_reg_is_6ghz_supported(mac->psoc) &&
	    (wlan_reg_is_6ghz_op_class(mac->pdev,
			 pBeaconReq->measurement_request.Beacon.regClass))) {
		pe_nofl_err("RX: [802.11 BCN_RPT] Ch belongs to 6 ghz spectrum, abort");
		qdf_mem_free(psbrr);
		return eRRM_FAILURE;
	}

	rrm_get_country_code_from_connected_profile(mac, pe_session,
						    country);
	psbrr->channel_info.chan_num =
		pBeaconReq->measurement_request.Beacon.channel;
	psbrr->channel_info.reg_class =
		pBeaconReq->measurement_request.Beacon.regClass;
	if (psbrr->channel_info.chan_num &&
	    psbrr->channel_info.chan_num != 255) {
		psbrr->channel_info.chan_freq =
			wlan_reg_country_chan_opclass_to_freq(
				mac->pdev,
				country,
				psbrr->channel_info.chan_num,
				psbrr->channel_info.reg_class,
				false);
		if (!psbrr->channel_info.chan_freq) {
			pe_debug("invalid ch freq, chan_num %d",
				  psbrr->channel_info.chan_num);
			qdf_mem_free(psbrr);
			return eRRM_FAILURE;
		}
	} else {
		psbrr->channel_info.chan_freq = 0;
	}
	sme_debug("opclass %d, ch %d freq %d AP's country code %c%c 0x%x index:%d",
		  psbrr->channel_info.reg_class,
		  psbrr->channel_info.chan_num,
		  psbrr->channel_info.chan_freq,
		  country[0], country[1], country[2],
		  psbrr->measurement_idx);

	psbrr->measurementDuration[0] = measDuration;
	psbrr->fMeasurementtype[0] =
		pBeaconReq->measurement_request.Beacon.meas_mode;
	qdf_mem_copy(psbrr->macaddrBssid,
		     pBeaconReq->measurement_request.Beacon.BSSID,
		     sizeof(tSirMacAddr));

	if (pBeaconReq->measurement_request.Beacon.SSID.present) {
		psbrr->ssId.length =
			pBeaconReq->measurement_request.Beacon.SSID.num_ssid;
		qdf_mem_copy(psbrr->ssId.ssId,
			     pBeaconReq->measurement_request.Beacon.SSID.ssid,
			     psbrr->ssId.length);
	}

	pCurrentReq->token = pBeaconReq->measurement_token;

	num_rpt = pBeaconReq->measurement_request.Beacon.num_APChannelReport;
	for (idx_rpt = 0; idx_rpt < num_rpt; idx_rpt++) {
		ie_ap_chan_rpt =
			&pBeaconReq->measurement_request.Beacon.APChannelReport[idx_rpt];
		for (tmp_idx = 0;
		     tmp_idx < ie_ap_chan_rpt->num_channelList;
		     tmp_idx++) {
			if (!wlan_reg_is_6ghz_supported(mac->psoc) &&
			    (wlan_reg_is_6ghz_op_class(mac->pdev,
					    ie_ap_chan_rpt->regulatoryClass))) {
				pe_nofl_err("RX: [802.11 BCN_RPT] Ch belongs to 6 ghz spectrum, abort");
				qdf_mem_free(psbrr);
				return eRRM_FAILURE;
			}

			psbrr->channel_list.chan_freq_lst[ch_ctr++] =
				wlan_reg_country_chan_opclass_to_freq(
					mac->pdev, country,
					ie_ap_chan_rpt->channelList[tmp_idx],
					ie_ap_chan_rpt->regulatoryClass, true);

			if (ch_ctr >= QDF_ARRAY_SIZE(psbrr->channel_list.chan_freq_lst))
				break;
		}
		if (ch_ctr >= QDF_ARRAY_SIZE(psbrr->channel_list.chan_freq_lst))
			break;
	}

	psbrr->channel_list.num_channels = ch_ctr;
	buf_left = sizeof(ch_buf);
	tmp_buf = ch_buf;
	for (idx_rpt = 0; idx_rpt < ch_ctr; idx_rpt++) {
		buf_cons = qdf_snprint(tmp_buf, buf_left, "%d ",
				psbrr->channel_list.chan_freq_lst[idx_rpt]);
		buf_left -= buf_cons;
		tmp_buf += buf_cons;
	}

	if (ch_ctr)
		pe_nofl_info("RX: [802.11 BCN_RPT] Ch-list:%s", ch_buf);

	/* Send request to SME. */
	mmh_msg.type = eWNI_SME_BEACON_REPORT_REQ_IND;
	mmh_msg.bodyptr = psbrr;
	MTRACE(mac_trace(mac, TRACE_CODE_TX_SME_MSG,
			 pe_session->peSessionId, mmh_msg.type));
	lim_sys_process_mmh_msg_api(mac, &mmh_msg);
	return eRRM_SUCCESS;
}

/**
 * rrm_fill_beacon_ies() - Fills fixed fields and Ies in bss description to an
 * array of uint8_t.
 * @pIes - pointer to the buffer that should be populated with ies.
 * @pNumIes - returns the num of ies filled in this param.
 * @pIesMaxSize - Max size of the buffer pIes.
 * @eids - pointer to array of eids. If NULL, all ies will be populated.
 * @numEids - number of elements in array eids.
 * @start_offset: Offset from where the IEs in the bss_desc should be parsed
 * @bss_desc - pointer to Bss Description.
 *
 * Return: Remaining length of IEs in current bss_desc which are not included
 *	   in pIes.
 */
static uint16_t
rrm_fill_beacon_ies(struct mac_context *mac, uint8_t *pIes,
		    uint8_t *pNumIes, uint8_t pIesMaxSize, uint8_t *eids,
		    uint8_t numEids, uint16_t start_offset,
		    struct bss_description *bss_desc)
{
	uint8_t *pBcnIes, count = 0, i;
	uint16_t BcnNumIes, total_ies_len, len;
	uint16_t rem_len = 0;

	if ((!pIes) || (!pNumIes) || (!bss_desc)) {
		pe_err("Invalid parameters");
		return 0;
	}
	/* Make sure that if eid is null, numEids is set to zero. */
	numEids = (!eids) ? 0 : numEids;

	total_ies_len = GET_IE_LEN_IN_BSS(bss_desc->length);
	BcnNumIes = total_ies_len;
	if (start_offset > BcnNumIes) {
		pe_err("Invalid start offset %d Bcn IE len %d",
		       start_offset, total_ies_len);
		return 0;
	}

	pBcnIes = (uint8_t *)&bss_desc->ieFields[0];
	pBcnIes += start_offset;
	BcnNumIes = BcnNumIes - start_offset;

	*pNumIes = 0;

	/*
	 * If start_offset is 0, this is the first fragment of the current
	 * beacon. Include the Beacon Fixed Fields of length 12 bytes
	 * (BEACON_FRAME_IES_OFFSET) in the first fragment.
	 */
	if (start_offset == 0) {
		*((uint32_t *)pIes) = bss_desc->timeStamp[0];
		*pNumIes += sizeof(uint32_t);
		pIes += sizeof(uint32_t);
		*((uint32_t *)pIes) = bss_desc->timeStamp[1];
		*pNumIes += sizeof(uint32_t);
		pIes += sizeof(uint32_t);
		*((uint16_t *)pIes) = bss_desc->beaconInterval;
		*pNumIes += sizeof(uint16_t);
		pIes += sizeof(uint16_t);
		*((uint16_t *)pIes) = bss_desc->capabilityInfo;
		*pNumIes += sizeof(uint16_t);
		pIes += sizeof(uint16_t);
	}

	while (BcnNumIes >= 2) {
		len = *(pBcnIes + 1);
		len += 2;       /* element id + length. */
		pe_debug("EID = %d, len = %d total = %d",
			*pBcnIes, *(pBcnIes + 1), len);

		if (BcnNumIes < len || len <= 2) {
			pe_err("RRM: Invalid IE len:%d exp_len:%d",
			       len, BcnNumIes);
			break;
		}

		i = 0;
		do {
			if ((!eids) || (*pBcnIes == eids[i])) {
				if (((*pNumIes) + len) < pIesMaxSize) {
					qdf_mem_copy(pIes, pBcnIes, len);
					pIes += len;
					*pNumIes += len;
					count++;
				} else {
					/*
					 * If max size of fragment is reached,
					 * calculate the remaining length and
					 * break. For first fragment, account
					 * for the fixed fields also.
					 */
					rem_len = total_ies_len - *pNumIes -
						  start_offset;
					if (start_offset == 0)
						rem_len = rem_len +
						BEACON_FRAME_IES_OFFSET;
					pe_debug("rem_len %d ies added %d",
						 rem_len, *pNumIes);
				}
				break;
			}
			i++;
		} while (i < numEids);

		if (rem_len)
			break;

		pBcnIes += len;
		BcnNumIes -= len;
	}
	pe_debug("Total length of Ies added = %d rem_len %d",
		 *pNumIes, rem_len);

	return rem_len;
}

/**
 * rrm_process_beacon_report_xmit() - create a rrm action frame
 * @mac_ctx: Global pointer to MAC context
 * @beacon_xmit_ind: Data for beacon report IE from SME.
 *
 * Create a Radio measurement report action frame and send it to peer.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
rrm_process_beacon_report_xmit(struct mac_context *mac_ctx,
			       tpSirBeaconReportXmitInd beacon_xmit_ind)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirMacRadioMeasureReport *report = NULL;
	tSirMacBeaconReport *beacon_report;
	struct bss_description *bss_desc;
	tpRRMReq curr_req;
	struct pe_session *session_entry;
	uint8_t session_id, counter;
	uint8_t i, j;
	uint8_t bss_desc_count = 0;
	uint8_t report_index = 0;
	uint16_t rem_len = 0;
	uint16_t offset = 0;
	uint8_t frag_id = 0;
	uint8_t num_frames, num_reports_in_frame, final_measurement_index;
	uint32_t populated_beacon_report_size = 0;
	uint32_t max_reports_in_frame = 0;
	uint32_t radio_meas_rpt_size = 0, dot11_meas_rpt_size = 0;
	bool is_last_measurement_frame;


	if (!beacon_xmit_ind) {
		pe_err("Received beacon_xmit_ind is NULL in PE");
		return QDF_STATUS_E_FAILURE;
	}

	if (beacon_xmit_ind->measurement_idx >=
	    QDF_ARRAY_SIZE(mac_ctx->rrm.rrmPEContext.pCurrentReq)) {
		pe_err("Received measurement_idx is out of range: %u - %zu",
		       beacon_xmit_ind->measurement_idx,
		       QDF_ARRAY_SIZE(mac_ctx->rrm.rrmPEContext.pCurrentReq));
		return QDF_STATUS_E_FAILURE;
	}

	curr_req = mac_ctx->rrm.rrmPEContext.
		pCurrentReq[beacon_xmit_ind->measurement_idx];
	if (!curr_req) {
		pe_err("Received report xmit while there is no request pending in PE");
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	pe_debug("Received beacon report xmit indication on idx:%d",
		 beacon_xmit_ind->measurement_idx);

	/*
	 * Send empty report only if all channels on a measurement index has
	 * no scan results or if the AP requests for last beacon report
	 * indication and last channel of the last index has empty report
	 */
	if (beacon_xmit_ind->numBssDesc || curr_req->sendEmptyBcnRpt ||
	    (beacon_xmit_ind->fMeasureDone &&
	     curr_req->request.Beacon.last_beacon_report_indication &&
	     (mac_ctx->rrm.rrmPEContext.num_active_request - 1) == 0)) {
		beacon_xmit_ind->numBssDesc = (beacon_xmit_ind->numBssDesc ==
			RRM_BCN_RPT_NO_BSS_INFO) ? RRM_BCN_RPT_MIN_RPT :
			beacon_xmit_ind->numBssDesc;

		session_entry = pe_find_session_by_bssid(mac_ctx,
				beacon_xmit_ind->bssId, &session_id);
		if (!session_entry) {
			pe_err("TX: [802.11 BCN_RPT] Session does not exist for bssId:"QDF_MAC_ADDR_FMT"",
			       QDF_MAC_ADDR_REF(beacon_xmit_ind->bssId));
			status = QDF_STATUS_E_FAILURE;
			goto end;
		}

		report = qdf_mem_malloc(MAX_BEACON_REPORTS * sizeof(*report));
		if (!report) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}

		for (i = 0; i < MAX_BEACON_REPORTS &&
		     bss_desc_count < beacon_xmit_ind->numBssDesc; i++) {
			beacon_report = &report[i].report.beaconReport;
			/*
			 * If the scan result is NULL then send report request
			 * with option subelement as NULL.
			 */
			pe_debug("TX: [802.11 BCN_RPT] report %d bss %d", i,
				 bss_desc_count);
			bss_desc = beacon_xmit_ind->
				   pBssDescription[bss_desc_count];

			/* Prepare the beacon report and send it to the peer.*/
			report[i].token = beacon_xmit_ind->uDialogToken;
			report[i].refused = 0;
			report[i].incapable = 0;
			report[i].type = SIR_MAC_RRM_BEACON_TYPE;

			/*
			 * Valid response is included if the size of
			 * becon xmit is == size of beacon xmit ind + ies
			 */
			if (beacon_xmit_ind->length < sizeof(*beacon_xmit_ind))
				continue;
			beacon_report->regClass = beacon_xmit_ind->regClass;
			if (bss_desc) {
				beacon_report->channel =
					wlan_reg_freq_to_chan(
						mac_ctx->pdev,
						bss_desc->chan_freq);
				qdf_mem_copy(beacon_report->measStartTime,
					bss_desc->startTSF,
					sizeof(bss_desc->startTSF));
				beacon_report->measDuration =
					beacon_xmit_ind->duration;
				beacon_report->phyType = bss_desc->nwType;
				beacon_report->bcnProbeRsp = 1;
				beacon_report->rsni = bss_desc->sinr;

				rrm_calculate_and_fill_rcpi(&beacon_report->rcpi,
							    bss_desc->rssi);
				beacon_report->antennaId = 0;
				beacon_report->parentTSF = bss_desc->parentTSF;
				qdf_mem_copy(beacon_report->bssid,
					bss_desc->bssId, sizeof(tSirMacAddr));
			}

			pe_debug("TX: [802.11 BCN_RPT] reporting detail requested %d",
				 curr_req->request.Beacon.reportingDetail);
			switch (curr_req->request.Beacon.reportingDetail) {
			case BEACON_REPORTING_DETAIL_NO_FF_IE:
				/* 0: No need to include any elements. */
				break;
			case BEACON_REPORTING_DETAIL_ALL_FF_REQ_IE:
				/* 1: Include all FFs and Requested Ies. */
				if (!bss_desc)
					break;

				rem_len = rrm_fill_beacon_ies(mac_ctx,
					    (uint8_t *)&beacon_report->Ies[0],
					    (uint8_t *)&beacon_report->numIes,
					    BEACON_REPORT_MAX_IES,
					    curr_req->request.Beacon.reqIes.
					    pElementIds,
					    curr_req->request.Beacon.reqIes.num,
					    offset, bss_desc);
				break;
			case BEACON_REPORTING_DETAIL_ALL_FF_IE:
				/* 2: default - Include all FFs and all Ies. */
			default:
				if (!bss_desc)
					break;

				rem_len = rrm_fill_beacon_ies(mac_ctx,
					    (uint8_t *) &beacon_report->Ies[0],
					    (uint8_t *) &beacon_report->numIes,
					    BEACON_REPORT_MAX_IES,
					    NULL,
					    0,
					    offset, bss_desc);
				break;
			}
			beacon_report->frame_body_frag_id.id = bss_desc_count;
			beacon_report->frame_body_frag_id.frag_id = frag_id;
			/*
			 * If remaining length is non-zero, the beacon needs to
			 * be fragmented only if the current request supports
			 * last beacon report indication.
			 * If last beacon report indication is not supported,
			 * truncate and move on to the next beacon.
			 */
			if (rem_len &&
			    curr_req->request.Beacon.
			    last_beacon_report_indication) {
				offset = GET_IE_LEN_IN_BSS(
						bss_desc->length) - rem_len;
				pe_debug("TX: [802.11 BCN_RPT] offset %d ie_len %lu rem_len %d frag_id %d",
					 offset,
					 GET_IE_LEN_IN_BSS(bss_desc->length),
					 rem_len, frag_id);
				frag_id++;
				beacon_report->frame_body_frag_id.more_frags =
									true;
			} else {
				offset = 0;
				beacon_report->frame_body_frag_id.more_frags =
									false;
				frag_id = 0;
				bss_desc_count++;
				pe_debug("TX: [802.11 BCN_RPT] No remaining IEs");
			}

			if (curr_req->request.Beacon.last_beacon_report_indication)
				beacon_report->last_bcn_report_ind_support = 1;

		}

		pe_debug("TX: [802.11 BCN_RPT] Total reports filled %d, last bcn_rpt ind:%d",
			 i , curr_req->request.Beacon.last_beacon_report_indication);

		/* Calculate size of populated beacon reports */
		radio_meas_rpt_size =  sizeof(tSirMacRadioMeasureReport);
		populated_beacon_report_size = (i * radio_meas_rpt_size);

		/* Calculate num of mgmt frames to send */
		num_frames = populated_beacon_report_size / MAX_MGMT_MPDU_LEN;
		if (populated_beacon_report_size % MAX_MGMT_MPDU_LEN)
			num_frames++;

		/* Calculate num of maximum mgmt reports per frame */
		dot11_meas_rpt_size = sizeof(tDot11fRadioMeasurementReport);
		max_reports_in_frame = MAX_MGMT_MPDU_LEN / dot11_meas_rpt_size;

		for (j = 0; j < num_frames; j++) {
			num_reports_in_frame = QDF_MIN((i - report_index),
						max_reports_in_frame);

			final_measurement_index =
				mac_ctx->rrm.rrmPEContext.num_active_request;
			is_last_measurement_frame =
				((j == num_frames - 1) &&
				 beacon_xmit_ind->fMeasureDone &&
				 !(final_measurement_index - 1));

			lim_send_radio_measure_report_action_frame(mac_ctx,
				curr_req->dialog_token, num_reports_in_frame,
				is_last_measurement_frame,
				&report[report_index],
				beacon_xmit_ind->bssId, session_entry);
			report_index += num_reports_in_frame;
		}
		curr_req->sendEmptyBcnRpt = false;
	}

end:
	for (counter = 0; counter < beacon_xmit_ind->numBssDesc; counter++)
		qdf_mem_free(beacon_xmit_ind->pBssDescription[counter]);

	if (beacon_xmit_ind->fMeasureDone) {
		pe_debug("Measurement done idx:%d",
			 beacon_xmit_ind->measurement_idx);
		rrm_cleanup(mac_ctx, beacon_xmit_ind->measurement_idx);
	}

	if (report)
		qdf_mem_free(report);

	return status;
}

static void
rrm_process_beacon_request_failure(struct mac_context *mac,
				   struct pe_session *pe_session,
				   tSirMacAddr peer,
				   tRrmRetStatus status, uint8_t index)
{
	tpSirMacRadioMeasureReport pReport = NULL;
	tpRRMReq pCurrentReq = mac->rrm.rrmPEContext.pCurrentReq[index];

	if (!pCurrentReq) {
		pe_err("Current request is NULL");
		return;
	}

	pReport = qdf_mem_malloc(sizeof(tSirMacRadioMeasureReport));
	if (!pReport)
		return;
	pReport->token = pCurrentReq->token;
	pReport->type = SIR_MAC_RRM_BEACON_TYPE;

	pe_debug("Measurement index:%d status %d token %d", index, status,
		 pReport->token);

	switch (status) {
	case eRRM_REFUSED:
		pReport->refused = 1;
		break;
	case eRRM_INCAPABLE:
		pReport->incapable = 1;
		break;
	default:
		pe_err("RX [802.11 BCN_RPT] Beacon request processing failed no report sent");
		qdf_mem_free(pReport);
		return;
	}

	if (pCurrentReq->request.Beacon.last_beacon_report_indication)
		pReport->report.beaconReport.last_bcn_report_ind_support = 1;

	lim_send_radio_measure_report_action_frame(mac,
						   pCurrentReq->dialog_token,
						   1, true,
						   pReport, peer,
						   pe_session);

	qdf_mem_free(pReport);
	return;
}

/**
 * rrm_process_beacon_req() - Update curr_req and report
 * @mac_ctx: Global pointer to MAC context
 * @peer: Macaddress of the peer requesting the radio measurement
 * @session_entry: session entry
 * @radiomes_report: Pointer to radio measurement report
 * @rrm_req: Array of Measurement request IEs
 * @num_report: No.of reports
 * @index: Index for Measurement request
 *
 * Update structure sRRMReq and sSirMacRadioMeasureReport and pass it to
 * rrm_process_beacon_report_req().
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS rrm_process_beacon_req(struct mac_context *mac_ctx, tSirMacAddr peer,
				  struct pe_session *session_entry,
				  tpSirMacRadioMeasureReport *radiomes_report,
				  tDot11fRadioMeasurementRequest *rrm_req,
				  uint8_t *num_report, int index)
{
	tRrmRetStatus rrm_status = eRRM_SUCCESS;
	tpSirMacRadioMeasureReport report = NULL;
	tpRRMReq curr_req;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (index  >= MAX_MEASUREMENT_REQUEST) {
		status = rrm_reject_req(&report, rrm_req, num_report, index,
			       rrm_req->MeasurementRequest[0].
							measurement_type);
		return status;
	}

	curr_req = qdf_mem_malloc(sizeof(*curr_req));
	if (!curr_req) {
		mac_ctx->rrm.rrmPEContext.pCurrentReq[index] = NULL;
		return QDF_STATUS_E_NOMEM;
	}
	pe_debug("Processing Beacon Report request %d", index);
	curr_req->dialog_token = rrm_req->DialogToken.token;
	curr_req->token =
		rrm_req->MeasurementRequest[index].measurement_token;
	curr_req->sendEmptyBcnRpt = true;
	curr_req->measurement_idx = index;
	mac_ctx->rrm.rrmPEContext.pCurrentReq[index] = curr_req;
	mac_ctx->rrm.rrmPEContext.num_active_request++;
	pe_debug("Processing Bcn Report req %d num_active_req:%d",
		 index, mac_ctx->rrm.rrmPEContext.num_active_request);
	rrm_status = rrm_process_beacon_report_req(mac_ctx, curr_req,
		&rrm_req->MeasurementRequest[index], session_entry);
	if (eRRM_SUCCESS != rrm_status) {
		rrm_process_beacon_request_failure(mac_ctx,
			session_entry, peer, rrm_status, index);
		rrm_cleanup(mac_ctx, index);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * update_rrm_report() - Set incapable bit
 * @mac_ctx: Global pointer to MAC context
 * @report: Pointer to radio measurement report
 * @rrm_req: Array of Measurement request IEs
 * @num_report: No.of reports
 * @index: Index for Measurement request
 *
 * Send a report with incapabale bit set
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS update_rrm_report(struct mac_context *mac_ctx,
			     tpSirMacRadioMeasureReport *report,
			     tDot11fRadioMeasurementRequest *rrm_req,
			     uint8_t *num_report, int index)
{
	tpSirMacRadioMeasureReport rrm_report;

	if (!*report) {
		/*
		 * Allocate memory to send reports for
		 * any subsequent requests.
		 */
		*report = qdf_mem_malloc(sizeof(tSirMacRadioMeasureReport) *
			 (rrm_req->num_MeasurementRequest - index));
		if (!*report)
			return QDF_STATUS_E_NOMEM;
		pe_debug("rrm beacon type incapable of %d report", *num_report);
	}
	rrm_report = *report;
	rrm_report[*num_report].incapable = 1;
	rrm_report[*num_report].type =
		rrm_req->MeasurementRequest[index].measurement_type;
	rrm_report[*num_report].token =
		 rrm_req->MeasurementRequest[index].measurement_token;
	(*num_report)++;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS rrm_reject_req(tpSirMacRadioMeasureReport *radiomes_report,
			  tDot11fRadioMeasurementRequest *rrm_req,
			  uint8_t *num_report, uint8_t index,
			  uint8_t measurement_type)
{
	tpSirMacRadioMeasureReport report;

	if (!*radiomes_report) {
	/*
	 * Allocate memory to send reports for
	 * any subsequent requests.
	 */
		*radiomes_report = qdf_mem_malloc(sizeof(*report) *
				(rrm_req->num_MeasurementRequest - index));
		if (!*radiomes_report)
			return QDF_STATUS_E_NOMEM;

		pe_debug("rrm beacon refused of %d report, index: %d in beacon table",
			 *num_report, index);
	}
	report = *radiomes_report;
	report[*num_report].refused = 1;
	report[*num_report].type = measurement_type;
	report[*num_report].token =
			rrm_req->MeasurementRequest[index].measurement_token;
	(*num_report)++;

	return QDF_STATUS_SUCCESS;

}

/* -------------------------------------------------------------------- */
/**
 * rrm_process_radio_measurement_request - Process rrm request
 * @mac_ctx: Global pointer to MAC context
 * @peer: Macaddress of the peer requesting the radio measurement.
 * @rrm_req: Array of Measurement request IEs
 * @session_entry: session entry.
 *
 * Processes the Radio Resource Measurement request.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
rrm_process_radio_measurement_request(struct mac_context *mac_ctx,
				      tSirMacAddr peer,
				      tDot11fRadioMeasurementRequest *rrm_req,
				      struct pe_session *session_entry)
{
	uint8_t i, index;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpSirMacRadioMeasureReport report = NULL;
	uint8_t num_report = 0;
	bool reject = false;

	if (!rrm_req->num_MeasurementRequest) {
		report = qdf_mem_malloc(sizeof(tSirMacRadioMeasureReport));
		if (!report)
			return QDF_STATUS_E_NOMEM;
		pe_err("RX: [802.11 RRM] No requestIes in the measurement request, sending incapable report");
		report->incapable = 1;
		num_report = 1;
		lim_send_radio_measure_report_action_frame(mac_ctx,
			rrm_req->DialogToken.token, num_report, true,
			report, peer, session_entry);
		qdf_mem_free(report);
		return QDF_STATUS_E_FAILURE;
	}
	/* PF Fix */
	if (rrm_req->NumOfRepetitions.repetitions > 0) {
		pe_info("RX: [802.11 RRM] number of repetitions %d, sending incapable report",
			rrm_req->NumOfRepetitions.repetitions);
		/*
		 * Send a report with incapable bit set.
		 * Not supporting repetitions.
		 */
		report = qdf_mem_malloc(sizeof(tSirMacRadioMeasureReport));
		if (!report)
			return QDF_STATUS_E_NOMEM;
		report->incapable = 1;
		report->type = rrm_req->MeasurementRequest[0].measurement_type;
		num_report = 1;
		goto end;
	}

	for (index = 0; index < MAX_MEASUREMENT_REQUEST; index++) {
		if (mac_ctx->rrm.rrmPEContext.pCurrentReq[index]) {
			reject = true;
			pe_debug("RRM req for index: %d is already in progress",
				 index);
			break;
		}
	}

	if (reject) {
		for (i = 0; i < rrm_req->num_MeasurementRequest; i++) {
			status =
			    rrm_reject_req(&report, rrm_req, &num_report, i,
					   rrm_req->MeasurementRequest[i].
							measurement_type);
			if (QDF_IS_STATUS_ERROR(status)) {
				pe_debug("Fail to Reject rrm req for index: %d",
					 i);
				return status;
			}
		}

		goto end;
	}

	/*
	 * Clear global beacon_rpt_chan_list before processing every new
	 * beacon report request.
	 */
	qdf_mem_zero(mac_ctx->rrm.rrmPEContext.beacon_rpt_chan_list,
		     sizeof(uint8_t) * MAX_NUM_CHANNELS);
	mac_ctx->rrm.rrmPEContext.beacon_rpt_chan_num = 0;

	for (i = 0; i < rrm_req->num_MeasurementRequest; i++) {
		switch (rrm_req->MeasurementRequest[i].measurement_type) {
		case SIR_MAC_RRM_BEACON_TYPE:
			/* Process beacon request. */
			status = rrm_process_beacon_req(mac_ctx, peer,
							session_entry, &report,
							rrm_req, &num_report,
							i);
			if (QDF_IS_STATUS_ERROR(status))
				return status;
			break;
		case SIR_MAC_RRM_LCI_TYPE:
		case SIR_MAC_RRM_LOCATION_CIVIC_TYPE:
		case SIR_MAC_RRM_FINE_TIME_MEAS_TYPE:
			pe_debug("RRM with type: %d sent to userspace",
			    rrm_req->MeasurementRequest[i].measurement_type);
			break;
		default:
			/* Send a report with incapabale bit set. */
			status = update_rrm_report(mac_ctx, &report, rrm_req,
						   &num_report, i);
			if (QDF_STATUS_SUCCESS != status)
				return status;
			break;
		}
	}

end:
	if (report) {
		lim_send_radio_measure_report_action_frame(mac_ctx,
			rrm_req->DialogToken.token, num_report, true,
			report, peer, session_entry);
		qdf_mem_free(report);
	}
	return status;
}

/**
 * rrm_get_start_tsf
 *
 * FUNCTION:  Get the Start TSF.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param startTSF - store star TSF in this buffer.
 * @return txPower
 */
void rrm_get_start_tsf(struct mac_context *mac, uint32_t *pStartTSF)
{
	pStartTSF[0] = mac->rrm.rrmPEContext.startTSF[0];
	pStartTSF[1] = mac->rrm.rrmPEContext.startTSF[1];

}

/* -------------------------------------------------------------------- */
/**
 * rrm_get_capabilities
 *
 * FUNCTION:
 * Returns a pointer to tpRRMCaps with all the caps enabled in RRM
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pe_session
 * @return pointer to tRRMCaps
 */
tpRRMCaps rrm_get_capabilities(struct mac_context *mac, struct pe_session *pe_session)
{
	return &mac->rrm.rrmPEContext.rrmEnabledCaps;
}

/**
 * rrm_initialize() - Initialize PE RRM parameters
 * @mac: Pointer to mac context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS rrm_initialize(struct mac_context *mac)
{
	tpRRMCaps pRRMCaps = &mac->rrm.rrmPEContext.rrmEnabledCaps;
	uint8_t i;

	for (i = 0; i < MAX_MEASUREMENT_REQUEST; i++)
		mac->rrm.rrmPEContext.pCurrentReq[i] = NULL;

	mac->rrm.rrmPEContext.txMgmtPower = 0;
	mac->rrm.rrmPEContext.DialogToken = 0;

	mac->rrm.rrmPEContext.rrmEnable = 0;
	mac->rrm.rrmPEContext.prev_rrm_report_seq_num = 0xFFFF;
	mac->rrm.rrmPEContext.num_active_request = 0;

	qdf_mem_zero(pRRMCaps, sizeof(tRRMCaps));
	pRRMCaps->LinkMeasurement = 1;
	pRRMCaps->NeighborRpt = 1;
	pRRMCaps->BeaconPassive = 1;
	pRRMCaps->BeaconActive = 1;
	pRRMCaps->BeaconTable = 1;
	pRRMCaps->APChanReport = 1;
	pRRMCaps->fine_time_meas_rpt = 1;
	pRRMCaps->lci_capability = 1;

	pRRMCaps->operatingChanMax = 3;
	pRRMCaps->nonOperatingChanMax = 3;

	return QDF_STATUS_SUCCESS;
}

void rrm_cleanup(struct mac_context *mac, uint8_t idx)
{
	tpRRMReq cur_rrm_req = NULL;

	mac->rrm.rrmPEContext.num_active_request--;
	pe_debug("Beacon report cleanup idx:%d, num_active_request:%d",
		 idx, mac->rrm.rrmPEContext.num_active_request);
	cur_rrm_req = mac->rrm.rrmPEContext.pCurrentReq[idx];
	if (!cur_rrm_req)
		return;

	qdf_mem_free(cur_rrm_req->request.Beacon.reqIes.pElementIds);
	cur_rrm_req->request.Beacon.reqIes.pElementIds = NULL;
	cur_rrm_req->request.Beacon.reqIes.num = 0;

	qdf_mem_free(cur_rrm_req);
	mac->rrm.rrmPEContext.pCurrentReq[idx] = NULL;
}

/**
 * lim_update_rrm_capability() - Update PE context's rrm capability
 * @mac_ctx: Global pointer to MAC context
 * @join_req: Pointer to SME join request.
 *
 * Update PE context's rrm capability based on SME join request.
 *
 * Return: None
 */
void lim_update_rrm_capability(struct mac_context *mac_ctx,
			       struct join_req *join_req)
{
	mac_ctx->rrm.rrmPEContext.rrmEnable = join_req->rrm_config.rrm_enabled;
	qdf_mem_copy(&mac_ctx->rrm.rrmPEContext.rrmEnabledCaps,
		     &join_req->rrm_config.rm_capability,
		     RMENABLEDCAP_MAX_LEN);

	return;
}
