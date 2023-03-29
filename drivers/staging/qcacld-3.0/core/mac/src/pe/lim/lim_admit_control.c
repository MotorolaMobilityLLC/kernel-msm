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
 * This file contains TSPEC and STA admit control related functions
 * NOTE: applies only to AP builds
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "sys_def.h"
#include "lim_api.h"
#include "lim_trace.h"
#include "lim_send_sme_rsp_messages.h"
#include "lim_types.h"
#include "lim_admit_control.h"

/* total available bandwidth in bps in each phy mode
 * these should be defined in hal or dph - replace these later
 */
#define LIM_TOTAL_BW_11A   54000000
#define LIM_MIN_BW_11A     6000000
#define LIM_TOTAL_BW_11B   11000000
#define LIM_MIN_BW_11B     1000000
#define LIM_TOTAL_BW_11G   LIM_TOTAL_BW_11A
#define LIM_MIN_BW_11G     LIM_MIN_BW_11B

/* conversion factors */
#define LIM_CONVERT_SIZE_BITS(numBytes) ((numBytes) * 8)
#define LIM_CONVERT_RATE_MBPS(rate)     ((rate)/1000000)

/* ------------------------------------------------------------------------------ */
/* local protos */

static void lim_get_available_bw(struct mac_context *, uint32_t *, uint32_t *, uint32_t,
				 uint32_t);

/** -------------------------------------------------------------
   \fn lim_calculate_svc_int
   \brief TSPEC validation and servcie interval determination
   \param     struct mac_context *   mac
   \param         struct mac_tspec_ie *pTspec
   \param         uint32_t            *pSvcInt
   \return QDF_STATUS - status of the comparison
   -------------------------------------------------------------*/

static QDF_STATUS
lim_calculate_svc_int(struct mac_context *mac,
		      struct mac_tspec_ie *pTspec, uint32_t *pSvcInt)
{
	uint32_t msduSz, dataRate;
	*pSvcInt = 0;

	/* if a service interval is already specified, we are done */
	if ((pTspec->minSvcInterval != 0) || (pTspec->maxSvcInterval != 0)) {
		*pSvcInt = (pTspec->maxSvcInterval != 0)
			   ? pTspec->maxSvcInterval : pTspec->minSvcInterval;
		return QDF_STATUS_SUCCESS;
	}

	/* Masking off the fixed bits according to definition of MSDU size
	 * in IEEE 802.11-2007 spec (section 7.3.2.30). Nominal MSDU size
	 * is defined as:  Bit[0:14]=Size, Bit[15]=Fixed
	 */
	if (pTspec->nomMsduSz != 0)
		msduSz = (pTspec->nomMsduSz & 0x7fff);
	else if (pTspec->maxMsduSz != 0)
		msduSz = pTspec->maxMsduSz;
	else {
		pe_err("MsduSize not specified");
		return QDF_STATUS_E_FAILURE;
	}

	/* need to calculate a reasonable service interval
	 * this is simply the msduSz/meanDataRate
	 */
	if (pTspec->meanDataRate != 0)
		dataRate = pTspec->meanDataRate;
	else if (pTspec->peakDataRate != 0)
		dataRate = pTspec->peakDataRate;
	else if (pTspec->minDataRate != 0)
		dataRate = pTspec->minDataRate;
	else {
		pe_err("DataRate not specified");
		return QDF_STATUS_E_FAILURE;
	}

	*pSvcInt =
		LIM_CONVERT_SIZE_BITS(msduSz) / LIM_CONVERT_RATE_MBPS(dataRate);
	return QDF_STATUS_E_FAILURE;
}

/**
 * lim_validate_tspec_edca() - Validate the parameters
 * @mac_ctx: Global MAC context
 * @tspec:   Pointer to the TSPEC
 * @session_entry: Session Entry
 *
 * validate the parameters in the edca tspec
 * mandatory fields are derived from 11e Annex I (Table I.1)
 *
 * Return: Status
 **/
static QDF_STATUS
lim_validate_tspec_edca(struct mac_context *mac_ctx,
			struct mac_tspec_ie *tspec,
			struct pe_session *session_entry)
{
	uint32_t max_phy_rate, min_phy_rate;
	uint32_t phy_mode;
	QDF_STATUS retval = QDF_STATUS_SUCCESS;

	lim_get_phy_mode(mac_ctx, &phy_mode, session_entry);

	lim_get_available_bw(mac_ctx, &max_phy_rate, &min_phy_rate, phy_mode,
			     1 /* bandwidth mult factor */);
	/* mandatory fields are derived from 11e Annex I (Table I.1) */
	if ((tspec->nomMsduSz == 0) ||
	    (tspec->meanDataRate == 0) ||
	    (tspec->surplusBw == 0) ||
	    (tspec->minPhyRate == 0) ||
	    (tspec->minPhyRate > max_phy_rate)) {
		pe_warn("Invalid EDCA Tspec: NomMsdu: %d meanDataRate: %d surplusBw: %d min_phy_rate: %d",
			tspec->nomMsduSz, tspec->meanDataRate,
			tspec->surplusBw, tspec->minPhyRate);
		retval = QDF_STATUS_E_FAILURE;
	}

	pe_debug("return status: %d", retval);
	return retval;
}

/** -------------------------------------------------------------
   \fn lim_validate_tspec
   \brief validate the offered tspec
   \param   struct mac_context *mac
   \param         struct mac_tspec_ie *pTspec
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

static QDF_STATUS
lim_validate_tspec(struct mac_context *mac,
		   struct mac_tspec_ie *pTspec, struct pe_session *pe_session)
{
	QDF_STATUS retval = QDF_STATUS_SUCCESS;

	switch (pTspec->tsinfo.traffic.accessPolicy) {
	case SIR_MAC_ACCESSPOLICY_EDCA:
		retval = lim_validate_tspec_edca(mac, pTspec, pe_session);
		if (retval != QDF_STATUS_SUCCESS)
			pe_warn("EDCA tspec invalid");
			break;

	case SIR_MAC_ACCESSPOLICY_HCCA:
	case SIR_MAC_ACCESSPOLICY_BOTH:
	/* TBD: should we support hybrid tspec as well?? for now, just fall through */
	default:
		pe_warn("AccessType: %d not supported",
			pTspec->tsinfo.traffic.accessPolicy);
		retval = QDF_STATUS_E_FAILURE;
		break;
	}
	return retval;
}

/* ----------------------------------------------------------------------------- */
/* Admit Control Policy */

/** -------------------------------------------------------------
   \fn lim_compute_mean_bw_used
   \brief determime the used/allocated bandwidth
   \param   struct mac_context *mac
   \param       uint32_t              *pBw
   \param       uint32_t               phyMode
   \param       tpLimTspecInfo    pTspecInfo
   \return void
   -------------------------------------------------------------*/

static void
lim_compute_mean_bw_used(struct mac_context *mac,
			 uint32_t *pBw,
			 uint32_t phyMode,
			 tpLimTspecInfo pTspecInfo, struct pe_session *pe_session)
{
	uint32_t ctspec;
	*pBw = 0;
	for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecInfo++) {
		if (pTspecInfo->inuse) {
			tpDphHashNode pSta =
				dph_get_hash_entry(mac, pTspecInfo->assocId,
						   &pe_session->dph.dphHashTable);
			if (!pSta) {
				/* maybe we should delete the tspec?? */
				pe_err("Tspec: %d assocId: %d dphNode not found",
					ctspec, pTspecInfo->assocId);
				continue;
			}
			*pBw += pTspecInfo->tspec.meanDataRate;
		}
	}
}

/** -------------------------------------------------------------
   \fn lim_get_available_bw
   \brief based on the phy mode and the bw_factor, determine the total bandwidth that
       can be supported
   \param   struct mac_context *mac
   \param       uint32_t              *pMaxBw
   \param       uint32_t              *pMinBw
   \param       uint32_t               phyMode
   \param       uint32_t               bw_factor
   \return void
   -------------------------------------------------------------*/

static void
lim_get_available_bw(struct mac_context *mac,
		     uint32_t *pMaxBw,
		     uint32_t *pMinBw, uint32_t phyMode, uint32_t bw_factor)
{
	switch (phyMode) {
	case WNI_CFG_PHY_MODE_11B:
		*pMaxBw = LIM_TOTAL_BW_11B;
		*pMinBw = LIM_MIN_BW_11B;
		break;

	case WNI_CFG_PHY_MODE_11A:
		*pMaxBw = LIM_TOTAL_BW_11A;
		*pMinBw = LIM_MIN_BW_11A;
		break;

	case WNI_CFG_PHY_MODE_11G:
	case WNI_CFG_PHY_MODE_NONE:
	default:
		*pMaxBw = LIM_TOTAL_BW_11G;
		*pMinBw = LIM_MIN_BW_11G;
		break;
	}
	*pMaxBw *= bw_factor;
}

/**
 * lim_admit_policy_oversubscription() - Admission control policy
 * @mac_ctx:        Global MAC Context
 * @tspec:          Pointer to the tspec
 * @admit_policy:   Admission policy
 * @tspec_info:     TSPEC information
 * @session_entry:  Session Entry
 *
 * simple admission control policy based on oversubscription
 * if the total bandwidth of all admitted tspec's exceeds (factor * phy-bw) then
 * reject the tspec, else admit it. The phy-bw is the peak available bw in the
 * current phy mode. The 'factor' is the configured oversubscription factor.
 *
 * Return: Status
 **/
static QDF_STATUS
lim_admit_policy_oversubscription(struct mac_context *mac_ctx,
				  struct mac_tspec_ie *tspec,
				  tpLimAdmitPolicyInfo admit_policy,
				  tpLimTspecInfo tspec_info,
				  struct pe_session *session_entry)
{
	uint32_t totalbw, minbw, usedbw;
	uint32_t phy_mode;

	/* determine total bandwidth used so far */
	lim_get_phy_mode(mac_ctx, &phy_mode, session_entry);

	lim_compute_mean_bw_used(mac_ctx, &usedbw, phy_mode,
			tspec_info, session_entry);

	/* determine how much bw is available based on the current phy mode */
	lim_get_available_bw(mac_ctx, &totalbw, &minbw, phy_mode,
			     admit_policy->bw_factor);

	if (usedbw > totalbw)   /* this can't possibly happen */
		return QDF_STATUS_E_FAILURE;

	if ((totalbw - usedbw) < tspec->meanDataRate) {
		pe_warn("Total BW: %d Used: %d Tspec request: %d not possible",
			totalbw, usedbw, tspec->meanDataRate);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_admit_policy
   \brief determine the current admit control policy and apply it for the offered tspec
   \param   struct mac_context *mac
   \param         struct mac_tspec_ie   *pTspec
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

static QDF_STATUS lim_admit_policy(struct mac_context *mac,
				      struct mac_tspec_ie *pTspec,
				      struct pe_session *pe_session)
{
	QDF_STATUS retval = QDF_STATUS_E_FAILURE;
	tpLimAdmitPolicyInfo pAdmitPolicy = &mac->lim.admitPolicyInfo;

	switch (pAdmitPolicy->type) {
	case WNI_CFG_ADMIT_POLICY_ADMIT_ALL:
		retval = QDF_STATUS_SUCCESS;
		break;

	case WNI_CFG_ADMIT_POLICY_BW_FACTOR:
		retval = lim_admit_policy_oversubscription(mac, pTspec,
							   &mac->lim.
							   admitPolicyInfo,
							   &mac->lim.tspecInfo[0],
							   pe_session);
		if (retval != QDF_STATUS_SUCCESS)
			pe_err("rejected by BWFactor policy");
			break;

	case WNI_CFG_ADMIT_POLICY_REJECT_ALL:
		retval = QDF_STATUS_E_FAILURE;
		break;

	default:
		retval = QDF_STATUS_SUCCESS;
		pe_warn("Admit Policy: %d unknown, admitting all traffic",
			pAdmitPolicy->type);
		break;
	}
	return retval;
}

/** -------------------------------------------------------------
   \fn lim_tspec_delete
   \brief delete the specified tspec
   \param   struct mac_context *mac
   \param     tpLimTspecInfo pInfo
   \return void
   -------------------------------------------------------------*/

/* ----------------------------------------------------------------------------- */
/* delete the specified tspec */
static void lim_tspec_delete(struct mac_context *mac, tpLimTspecInfo pInfo)
{
	if (!pInfo)
		return;
	/* pierre */
	pe_debug("tspec entry: %d delete tspec: %pK", pInfo->idx, pInfo);
	pInfo->inuse = 0;

	return;
}

/** -------------------------------------------------------------
   \fn lim_tspec_find_by_sta_addr
   \brief Send halMsg_AddTs to HAL
   \param   struct mac_context *mac
   \param   \param       uint8_t               *pAddr
   \param       struct mac_tspec_ie    *pTspecIE
   \param       tpLimTspecInfo    pTspecList
   \param       tpLimTspecInfo   *ppInfo
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

/* find the specified tspec in the list */
static QDF_STATUS
lim_tspec_find_by_sta_addr(struct mac_context *mac,
			   uint8_t *pAddr,
			   struct mac_tspec_ie *pTspecIE,
			   tpLimTspecInfo pTspecList, tpLimTspecInfo *ppInfo)
{
	int ctspec;

	*ppInfo = NULL;

	for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++) {
		if ((pTspecList->inuse) &&
		    (!qdf_mem_cmp(pAddr, pTspecList->staAddr,
				  sizeof(pTspecList->staAddr))) &&
		    (!qdf_mem_cmp(pTspecIE, &pTspecList->tspec,
				  sizeof(*pTspecIE)))) {
			*ppInfo = pTspecList;
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}

/** -------------------------------------------------------------
   \fn lim_tspec_find_by_assoc_id
   \brief find tspec with matchin staid and Tspec
   \param   struct mac_context *mac
   \param       uint32_t               staid
   \param       struct mac_tspec_ie    *pTspecIE
   \param       tpLimTspecInfo    pTspecList
   \param       tpLimTspecInfo   *ppInfo
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

QDF_STATUS
lim_tspec_find_by_assoc_id(struct mac_context *mac,
			   uint16_t assocId,
			   struct mac_tspec_ie *pTspecIE,
			   tpLimTspecInfo pTspecList, tpLimTspecInfo *ppInfo)
{
	int ctspec;

	*ppInfo = NULL;

	pe_debug("Trying to find tspec entry for assocId: %d pTsInfo->traffic.direction: %d pTsInfo->traffic.tsid: %d",
		assocId, pTspecIE->tsinfo.traffic.direction,
		pTspecIE->tsinfo.traffic.tsid);

	for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++) {
		if ((pTspecList->inuse) &&
		    (assocId == pTspecList->assocId) &&
		    (!qdf_mem_cmp(pTspecIE, &pTspecList->tspec,
				  sizeof(*pTspecIE)))) {
			*ppInfo = pTspecList;
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}

/** -------------------------------------------------------------
   \fn lim_find_tspec
   \brief finding a TSPEC entry with assocId, tsinfo.direction and tsinfo.tsid
   \param    uint16_t               assocId
   \param     struct mac_context *   mac
   \param     struct mac_ts_info   *pTsInfo
   \param         tpLimTspecInfo    pTspecList
   \param         tpLimTspecInfo   *ppInfo
   \return QDF_STATUS - status of the comparison
   -------------------------------------------------------------*/

static QDF_STATUS
lim_find_tspec(struct mac_context *mac,
	       uint16_t assocId,
	       struct mac_ts_info *pTsInfo,
	       tpLimTspecInfo pTspecList, tpLimTspecInfo *ppInfo)
{
	int ctspec;

	*ppInfo = NULL;

	pe_debug("Trying to find tspec entry for assocId: %d pTsInfo->traffic.direction: %d pTsInfo->traffic.tsid: %d",
		assocId, pTsInfo->traffic.direction, pTsInfo->traffic.tsid);

	for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++) {
		if ((pTspecList->inuse)
		    && (assocId == pTspecList->assocId)
		    && (pTsInfo->traffic.direction ==
			pTspecList->tspec.tsinfo.traffic.direction)
		    && (pTsInfo->traffic.tsid ==
			pTspecList->tspec.tsinfo.traffic.tsid)) {
			*ppInfo = pTspecList;
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}

/** -------------------------------------------------------------
   \fn lim_tspec_add
   \brief add or update the specified tspec to the tspec list
   \param struct mac_context *   mac
   \param uint8_t               *pAddr
   \param uint16_t               assocId
   \param struct mac_tspec_ie   *pTspec
   \param uint32_t               interval
   \param tpLimTspecInfo   *ppInfo

   \return QDF_STATUS - status of the comparison
   -------------------------------------------------------------*/

QDF_STATUS lim_tspec_add(struct mac_context *mac,
			    uint8_t *pAddr,
			    uint16_t assocId,
			    struct mac_tspec_ie *pTspec,
			    uint32_t interval, tpLimTspecInfo *ppInfo)
{
	tpLimTspecInfo pTspecList = &mac->lim.tspecInfo[0];
	*ppInfo = NULL;

	/* validate the assocId */
	if (assocId >= mac->lim.maxStation) {
		pe_err("Invalid assocId 0x%x", assocId);
		return QDF_STATUS_E_FAILURE;
	}
	/* decide whether to add/update */
	{
		*ppInfo = NULL;

		if (QDF_STATUS_SUCCESS ==
		    lim_find_tspec(mac, assocId, &pTspec->tsinfo, pTspecList,
				   ppInfo)) {
			/* update this entry. */
			pe_debug("updating TSPEC table entry: %d",
				(*ppInfo)->idx);
		} else {
			/* We didn't find one to update. So find a free slot in the
			 * LIM TSPEC list and add this new entry
			 */
			uint8_t ctspec = 0;

			for (ctspec = 0, pTspecList = &mac->lim.tspecInfo[0];
			     ctspec < LIM_NUM_TSPEC_MAX;
			     ctspec++, pTspecList++) {
				if (!pTspecList->inuse) {
					pe_debug("Found free slot in TSPEC list. Add to TSPEC table entry: %d",
						ctspec);
					break;
				}
			}

			if (ctspec >= LIM_NUM_TSPEC_MAX)
				return QDF_STATUS_E_FAILURE;

			/* Record the new index entry */
			pTspecList->idx = ctspec;
		}
	}

	/* update the tspec info */
	pTspecList->tspec = *pTspec;
	pTspecList->assocId = assocId;
	qdf_mem_copy(pTspecList->staAddr, pAddr, sizeof(pTspecList->staAddr));

	/* for edca tspec's, we are all done */
	if (pTspec->tsinfo.traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_EDCA) {
		pTspecList->inuse = 1;
		*ppInfo = pTspecList;
		pe_debug("added entry for EDCA AccessPolicy");
		return QDF_STATUS_SUCCESS;
	}

	/*
	 * for hcca tspec's, must set the parameterized bit in the queues
	 * the 'ts' bit in the queue data structure indicates that the queue is
	 * parameterized (hcca). When the schedule is written this bit is used
	 * in the tsid field (bit 3) and the other three bits (0-2) are simply
	 * filled in as the user priority (or qid). This applies only to uplink
	 * polls where the qos control field must contain the tsid specified in the
	 * tspec.
	 */
	pTspecList->inuse = 1;
	*ppInfo = pTspecList;
	pe_debug("added entry for HCCA AccessPolicy");
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_validate_access_policy
   \brief Validates Access policy
   \param   struct mac_context *mac
   \param       uint8_t              accessPolicy
   \param       uint16_t             assocId
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

static QDF_STATUS
lim_validate_access_policy(struct mac_context *mac,
			   uint8_t accessPolicy,
			   uint16_t assocId, struct pe_session *pe_session)
{
	QDF_STATUS retval = QDF_STATUS_E_FAILURE;
	tpDphHashNode pSta =
		dph_get_hash_entry(mac, assocId, &pe_session->dph.dphHashTable);

	if ((!pSta) || (!pSta->valid)) {
		pe_err("invalid station address passed");
		return QDF_STATUS_E_FAILURE;
	}

	switch (accessPolicy) {
	case SIR_MAC_ACCESSPOLICY_EDCA:
		if (pSta->wmeEnabled || pSta->lleEnabled)
			retval = QDF_STATUS_SUCCESS;
		break;

	case SIR_MAC_ACCESSPOLICY_HCCA:
	case SIR_MAC_ACCESSPOLICY_BOTH:
	default:
		pe_err("Invalid accessPolicy: %d",
			       accessPolicy);
		break;
	}

	if (retval != QDF_STATUS_SUCCESS)
		pe_warn("accPol: %d lle: %d wme: %d wsm: %d sta mac "
			QDF_MAC_ADDR_FMT, accessPolicy, pSta->lleEnabled,
			pSta->wmeEnabled, pSta->wsmEnabled,
			QDF_MAC_ADDR_REF(pSta->staAddr));

	return retval;
}

/**
 * lim_admit_control_add_ts() -        Check if STA can be admitted
 * @mac:               Global MAC context
 * @pAddr:              Address
 * @pAddts:             ADD TS
 * @pQos:               QOS fields
 * @assocId:            Association ID
 * @alloc:              Allocate bandwidth for this tspec
 * @pSch:               Schedule IE
 * @pTspecIdx:          TSPEC index
 * @pe_session:      PE Session Entry
 *
 * Determine if STA with the specified TSPEC can be admitted. If it can,
 * a schedule element is provided
 *
 * Return: status
 **/
QDF_STATUS lim_admit_control_add_ts(struct mac_context *mac, uint8_t *pAddr,
		tSirAddtsReqInfo *pAddts, tSirMacQosCapabilityStaIE *pQos,
		uint16_t assocId, uint8_t alloc, tSirMacScheduleIE *pSch,
		uint8_t *pTspecIdx, struct pe_session *pe_session)
{
	tpLimTspecInfo pTspecInfo;
	QDF_STATUS retval;
	uint32_t svcInterval;
	(void)pQos;

	/* TBD: modify tspec as needed */
	/* EDCA: need to fill in the medium time and the minimum phy rate */
	/* to be consistent with the desired traffic parameters. */

	pe_debug("tsid: %d directn: %d start: %d intvl: %d accPolicy: %d up: %d",
		pAddts->tspec.tsinfo.traffic.tsid,
		pAddts->tspec.tsinfo.traffic.direction,
		pAddts->tspec.svcStartTime, pAddts->tspec.minSvcInterval,
		pAddts->tspec.tsinfo.traffic.accessPolicy,
		pAddts->tspec.tsinfo.traffic.userPrio);

	/* check for duplicate tspec */
	retval = (alloc)
		 ? lim_tspec_find_by_assoc_id(mac, assocId, &pAddts->tspec,
					      &mac->lim.tspecInfo[0], &pTspecInfo)
		 : lim_tspec_find_by_sta_addr(mac, pAddr, &pAddts->tspec,
					      &mac->lim.tspecInfo[0], &pTspecInfo);

	if (retval == QDF_STATUS_SUCCESS) {
		pe_err("duplicate tspec index: %d", pTspecInfo->idx);
		return QDF_STATUS_E_FAILURE;
	}
	/* check that the tspec's are well formed and acceptable */
	if (lim_validate_tspec(mac, &pAddts->tspec, pe_session) !=
	    QDF_STATUS_SUCCESS) {
		pe_warn("tspec validation failed");
		return QDF_STATUS_E_FAILURE;
	}
	/* determine a service interval for the tspec */
	if (lim_calculate_svc_int(mac, &pAddts->tspec, &svcInterval) !=
	    QDF_STATUS_SUCCESS) {
		pe_warn("SvcInt calculate failed");
		return QDF_STATUS_E_FAILURE;
	}
	/* determine if the tspec can be admitted or not based on current policy */
	if (lim_admit_policy(mac, &pAddts->tspec, pe_session) != QDF_STATUS_SUCCESS) {
		pe_warn("tspec rejected by admit control policy");
		return QDF_STATUS_E_FAILURE;
	}
	/* fill in a schedule if requested */
	if (pSch) {
		qdf_mem_zero((uint8_t *) pSch, sizeof(*pSch));
		pSch->svcStartTime = pAddts->tspec.svcStartTime;
		pSch->svcInterval = svcInterval;
		pSch->maxSvcDuration = (uint16_t) pSch->svcInterval;    /* use SP = SI */
		pSch->specInterval = 0x1000;    /* fixed for now: TBD */

		pSch->info.direction = pAddts->tspec.tsinfo.traffic.direction;
		pSch->info.tsid = pAddts->tspec.tsinfo.traffic.tsid;
		pSch->info.aggregation = 0;     /* no support for aggregation for now: TBD */
	}
	/* if no allocation is requested, done */
	if (!alloc)
		return QDF_STATUS_SUCCESS;

	/* check that we are in the proper mode to deal with the tspec type */
	if (lim_validate_access_policy
		    (mac, (uint8_t) pAddts->tspec.tsinfo.traffic.accessPolicy, assocId,
		    pe_session) != QDF_STATUS_SUCCESS) {
		pe_warn("AccessPolicy: %d is not valid in current mode",
			pAddts->tspec.tsinfo.traffic.accessPolicy);
		return QDF_STATUS_E_FAILURE;
	}
	/* add tspec to list */
	if (lim_tspec_add
		    (mac, pAddr, assocId, &pAddts->tspec, svcInterval, &pTspecInfo)
	    != QDF_STATUS_SUCCESS) {
		pe_err("no space in tspec list");
		return QDF_STATUS_E_FAILURE;
	}
	/* passing lim tspec table index to the caller */
	*pTspecIdx = pTspecInfo->idx;

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_admit_control_delete_ts
   \brief Delete the specified Tspec for the specified STA
   \param   struct mac_context *mac
   \param       uint16_t               assocId
   \param       struct mac_ts_info    *pTsInfo
   \param       uint8_t               *pTsStatus
   \param       uint8_t             *ptspecIdx
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

QDF_STATUS
lim_admit_control_delete_ts(struct mac_context *mac,
			    uint16_t assocId,
			    struct mac_ts_info *pTsInfo,
			    uint8_t *pTsStatus, uint8_t *ptspecIdx)
{
	tpLimTspecInfo pTspecInfo = NULL;

	if (pTsStatus)
		*pTsStatus = 0;

	if (lim_find_tspec
		    (mac, assocId, pTsInfo, &mac->lim.tspecInfo[0],
		    &pTspecInfo) == QDF_STATUS_SUCCESS) {
		if (pTspecInfo) {
			pe_debug("Tspec entry: %d found", pTspecInfo->idx);

			*ptspecIdx = pTspecInfo->idx;
			lim_tspec_delete(mac, pTspecInfo);
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}

/** -------------------------------------------------------------
   \fn lim_admit_control_delete_sta
   \brief Delete all TSPEC for the specified STA
   \param   struct mac_context *mac
   \param     uint16_t assocId
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

QDF_STATUS lim_admit_control_delete_sta(struct mac_context *mac, uint16_t assocId)
{
	tpLimTspecInfo pTspecInfo = &mac->lim.tspecInfo[0];
	int ctspec;

	for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecInfo++) {
		if (assocId == pTspecInfo->assocId) {
			lim_tspec_delete(mac, pTspecInfo);
			pe_debug("Deleting TSPEC: %d for assocId: %d", ctspec,
				assocId);
		}
	}
	pe_debug("assocId: %d done", assocId);

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_admit_control_init
   \brief init tspec table
   \param   struct mac_context *mac
   \return QDF_STATUS - status
   -------------------------------------------------------------*/
QDF_STATUS lim_admit_control_init(struct mac_context *mac)
{
	qdf_mem_zero(mac->lim.tspecInfo,
		    LIM_NUM_TSPEC_MAX * sizeof(tLimTspecInfo));
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_send_hal_msg_add_ts
   \brief Send halMsg_AddTs to HAL
   \param   struct mac_context *mac
   \param     uint8_t         tspecIdx
   \param       struct mac_tspec_ie tspecIE
   \param       tSirTclasInfo   *tclasInfo
   \param       uint8_t           tclasProc
   \param       uint16_t          tsm_interval
   \return QDF_STATUS - status
   -------------------------------------------------------------*/
#ifdef FEATURE_WLAN_ESE
QDF_STATUS
lim_send_hal_msg_add_ts(struct mac_context *mac,
			uint8_t tspecIdx,
			struct mac_tspec_ie tspecIE,
			uint8_t sessionId, uint16_t tsm_interval)
#else
QDF_STATUS
lim_send_hal_msg_add_ts(struct mac_context *mac,
			uint8_t tspecIdx,
			struct mac_tspec_ie tspecIE,
			uint8_t sessionId)
#endif
{
	struct scheduler_msg msg = {0};
	struct add_ts_param *pAddTsParam;

	struct pe_session *pe_session = pe_find_session_by_session_id(mac, sessionId);

	if (!pe_session) {
		pe_err("Unable to get Session for session Id: %d",
			sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pAddTsParam = qdf_mem_malloc(sizeof(*pAddTsParam));
	if (!pAddTsParam)
		return QDF_STATUS_E_NOMEM;

	pAddTsParam->tspec_idx = tspecIdx;
	qdf_mem_copy(&pAddTsParam->tspec, &tspecIE,
		     sizeof(struct mac_tspec_ie));
	pAddTsParam->pe_session_id = sessionId;
	pAddTsParam->vdev_id = pe_session->smeSessionId;

#ifdef FEATURE_WLAN_ESE
	pAddTsParam->tsm_interval = tsm_interval;
#endif
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (mac->mlme_cfg->lfr.lfr3_roaming_offload &&
	    pe_session->is11Rconnection)
		pAddTsParam->set_ric_params = true;
#endif

	msg.type = WMA_ADD_TS_REQ;
	msg.bodyptr = pAddTsParam;
	msg.bodyval = 0;

	/* We need to defer any incoming messages until we get a
	 * WMA_ADD_TS_RSP from HAL.
	 */
	SET_LIM_PROCESS_DEFD_MESGS(mac, false);
	MTRACE(mac_trace_msg_tx(mac, sessionId, msg.type));

	if (QDF_STATUS_SUCCESS != wma_post_ctrl_msg(mac, &msg)) {
		pe_warn("wma_post_ctrl_msg() failed");
		SET_LIM_PROCESS_DEFD_MESGS(mac, true);
		qdf_mem_free(pAddTsParam);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_send_hal_msg_del_ts
   \brief Send halMsg_AddTs to HAL
   \param   struct mac_context *mac
   \param     uint8_t         tspecIdx
   \param     tSirAddtsReqInfo addts
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

QDF_STATUS
lim_send_hal_msg_del_ts(struct mac_context *mac,
			uint8_t tspecIdx,
			struct delts_req_info delts,
			uint8_t sessionId, uint8_t *bssId)
{
	struct scheduler_msg msg = {0};
	struct del_ts_params *pDelTsParam;
	struct pe_session *pe_session = NULL;

	pDelTsParam = qdf_mem_malloc(sizeof(*pDelTsParam));
	if (!pDelTsParam)
		return QDF_STATUS_E_NOMEM;

	msg.type = WMA_DEL_TS_REQ;
	msg.bodyptr = pDelTsParam;
	msg.bodyval = 0;

	/* filling message parameters. */
	pDelTsParam->tspecIdx = tspecIdx;
	qdf_mem_copy(&pDelTsParam->bssId, bssId, sizeof(tSirMacAddr));

	pe_session = pe_find_session_by_session_id(mac, sessionId);
	if (!pe_session) {
		pe_err("Session does Not exist with given sessionId: %d",
			       sessionId);
		goto err;
	}
	pDelTsParam->sessionId = pe_session->smeSessionId;
	pDelTsParam->userPrio = delts.wmeTspecPresent ?
			delts.tspec.tsinfo.traffic.userPrio :
			delts.tsinfo.traffic.userPrio;

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (mac->mlme_cfg->lfr.lfr3_roaming_offload &&
	    pe_session->is11Rconnection) {
		qdf_mem_copy(&pDelTsParam->delTsInfo, &delts,
			     sizeof(struct delts_req_info));
		pDelTsParam->setRICparams = 1;
	}
#endif
	MTRACE(mac_trace_msg_tx(mac, sessionId, msg.type));

	if (QDF_STATUS_SUCCESS != wma_post_ctrl_msg(mac, &msg)) {
		pe_warn("wma_post_ctrl_msg() failed");
		goto err;
	}
	return QDF_STATUS_SUCCESS;

err:
	qdf_mem_free(pDelTsParam);
	return QDF_STATUS_E_FAILURE;
}

/** -------------------------------------------------------------
   \fn     lim_process_hal_add_ts_rsp
   \brief  This function process the WMA_ADD_TS_RSP from HAL.
 \       If response is successful, then send back SME_ADDTS_RSP.
 \       Otherwise, send DELTS action frame to peer and then
 \       then send back SME_ADDTS_RSP.
 \
   \param  struct mac_context * mac
   \param  struct scheduler_msg *limMsg
   -------------------------------------------------------------*/
void lim_process_hal_add_ts_rsp(struct mac_context *mac,
				struct scheduler_msg *limMsg)
{
	struct add_ts_param *pAddTsRspMsg = NULL;
	tpDphHashNode pSta = NULL;
	uint16_t assocId = 0;
	tSirMacAddr peerMacAddr;
	uint8_t rspReqd = 1;
	struct pe_session *pe_session = NULL;

	/* Need to process all the deferred messages enqueued
	 * since sending the WMA_ADD_TS_REQ.
	 */
	SET_LIM_PROCESS_DEFD_MESGS(mac, true);

	if (!limMsg->bodyptr) {
		pe_err("Received WMA_ADD_TS_RSP with NULL");
		goto end;
	}

	pAddTsRspMsg = limMsg->bodyptr;

	/* 090803: Use pe_find_session_by_session_id() to obtain the PE session context */
	/* from the sessionId in the Rsp Msg from HAL */
	pe_session = pe_find_session_by_session_id(mac,
						   pAddTsRspMsg->pe_session_id);

	if (!pe_session) {
		pe_err("Session does Not exist with given sessionId: %d",
			       pAddTsRspMsg->pe_session_id);
		lim_send_sme_addts_rsp(mac, rspReqd, eSIR_SME_ADDTS_RSP_FAILED,
				       pe_session, pAddTsRspMsg->tspec,
				       mac->lim.gLimAddtsReq.sessionId);
		goto end;
	}

	if (pAddTsRspMsg->status == QDF_STATUS_SUCCESS) {
		pe_debug("Received successful ADDTS response from HAL");
		/* Use the smesessionId and smetransactionId from the PE session context */
		lim_send_sme_addts_rsp(mac, rspReqd, eSIR_SME_SUCCESS,
				       pe_session, pAddTsRspMsg->tspec,
				       pe_session->smeSessionId);
		goto end;
	} else {
		pe_debug("Received failure ADDTS response from HAL");
		/* Send DELTS action frame to AP */
		/* 090803: Get peer MAC addr from session */
		sir_copy_mac_addr(peerMacAddr, pe_session->bssId);

		/* 090803: Add the SME Session ID */
		lim_send_delts_req_action_frame(mac, peerMacAddr, rspReqd,
						&pAddTsRspMsg->tspec.tsinfo,
						&pAddTsRspMsg->tspec, pe_session);

		/* Delete TSPEC */
		/* 090803: Pull the hash table from the session */
		pSta = dph_lookup_hash_entry(mac, peerMacAddr, &assocId,
					     &pe_session->dph.dphHashTable);
		if (pSta)
			lim_admit_control_delete_ts(mac, assocId,
						    &pAddTsRspMsg->tspec.tsinfo,
						    NULL,
						    (uint8_t *) &pAddTsRspMsg->
						    tspec_idx);

		/* Send SME_ADDTS_RSP */
		/* 090803: Use the smesessionId and smetransactionId from the PE session context */
		lim_send_sme_addts_rsp(mac, rspReqd, eSIR_SME_ADDTS_RSP_FAILED,
				       pe_session, pAddTsRspMsg->tspec,
				       pe_session->smeSessionId);
		goto end;
	}

end:
	if (pAddTsRspMsg)
		qdf_mem_free(pAddTsRspMsg);
	return;
}
