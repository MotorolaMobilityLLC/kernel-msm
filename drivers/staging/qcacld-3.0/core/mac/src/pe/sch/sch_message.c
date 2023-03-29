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

#include "cds_api.h"
#include "sir_common.h"

#include "wni_cfg.h"
#include "ani_global.h"
#include "lim_api.h"
#include "lim_send_messages.h"

#include "sch_api.h"
#include "wlan_mlme_api.h"
#include <wlan_reg_services_api.h>
#include "lim_utils.h"

/* / Minimum beacon interval allowed (in Kus) */
#define SCH_BEACON_INTERVAL_MIN  10

/* / Maximum beacon interval allowed (in Kus) */
#define SCH_BEACON_INTERVAL_MAX  10000

/* / convert the CW values into a uint16_t */
#define GET_CW(pCw) ((uint16_t) ((*(pCw) << 8) + *((pCw) + 1)))

/* local functions */
static QDF_STATUS
get_wmm_local_params(struct mac_context *mac,
		     uint32_t params[][CFG_EDCA_DATA_LEN]);
static void
set_sch_edca_params(struct mac_context *mac,
		    uint32_t params[][CFG_EDCA_DATA_LEN],
		    struct pe_session *pe_session);

/* -------------------------------------------------------------------- */
/**
 * sch_set_beacon_interval
 *
 * FUNCTION:
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @return None
 */

void sch_set_beacon_interval(struct mac_context *mac,
			     struct pe_session *pe_session)
{
	uint32_t bi;

	bi = pe_session->beaconParams.beaconInterval;

	if (bi < SCH_BEACON_INTERVAL_MIN || bi > SCH_BEACON_INTERVAL_MAX) {
		pe_debug("Invalid beacon interval %d (should be [%d,%d]", bi,
			SCH_BEACON_INTERVAL_MIN, SCH_BEACON_INTERVAL_MAX);
		return;
	}

	mac->sch.beacon_interval = (uint16_t) bi;
}

void sch_edca_profile_update_all(struct mac_context *pmac)
{
	uint32_t i;
	struct pe_session *psession_entry;

	for (i = 0; i < pmac->lim.maxBssId; i++) {
		psession_entry = &pmac->lim.gpSession[i];
		if (psession_entry->valid)
			sch_edca_profile_update(pmac, psession_entry);
	}
}

/**
 * sch_get_params() - get the local or broadcast parameters based on the profile
 * sepcified in the config params are delivered in this order: BE, BK, VI, VO
 */
static QDF_STATUS
sch_get_params(struct mac_context *mac,
	       uint32_t params[][CFG_EDCA_DATA_LEN],
	       uint8_t local)
{
	uint32_t val;
	uint32_t i, idx;
	uint32_t *prf;
	struct wlan_mlme_edca_params *edca_params;
	QDF_STATUS status;
	uint8_t country_code_str[REG_ALPHA2_LEN + 1];
	uint32_t ani_l[] = {edca_ani_acbe_local, edca_ani_acbk_local,
			    edca_ani_acvi_local, edca_ani_acvo_local};

	uint32_t wme_l[] = {edca_wme_acbe_local, edca_wme_acbk_local,
			    edca_wme_acvi_local, edca_wme_acvo_local};

	uint32_t etsi_l[] = {edca_etsi_acbe_local, edca_etsi_acbk_local,
			     edca_etsi_acvi_local, edca_etsi_acvo_local};

	uint32_t ani_b[] = {edca_ani_acbe_bcast, edca_ani_acbk_bcast,
			    edca_ani_acvi_bcast, edca_ani_acvo_bcast};

	uint32_t wme_b[] = {edca_wme_acbe_bcast, edca_wme_acbk_bcast,
			    edca_wme_acvi_bcast, edca_wme_acvo_bcast};

	uint32_t etsi_b[] = {edca_etsi_acbe_bcast, edca_etsi_acbk_bcast,
			     edca_etsi_acvi_bcast, edca_etsi_acvo_bcast};
	edca_params = &mac->mlme_cfg->edca_params;

	wlan_reg_get_cc_and_src(mac->psoc, country_code_str);

	if (cds_is_etsi_europe_country(country_code_str)) {
		val = WNI_CFG_EDCA_PROFILE_ETSI_EUROPE;
		pe_debug("switch to ETSI EUROPE profile country code %c%c",
			 country_code_str[0], country_code_str[1]);
	} else {
		val = mac->mlme_cfg->wmm_params.edca_profile;
	}
	if (val >= WNI_CFG_EDCA_PROFILE_MAX) {
		pe_warn("Invalid EDCA_PROFILE %d, using %d instead", val,
			WNI_CFG_EDCA_PROFILE_ANI);
		val = WNI_CFG_EDCA_PROFILE_ANI;
	}

	pe_debug("EdcaProfile: Using %d (%s)", val,
		((val == WNI_CFG_EDCA_PROFILE_WMM) ? "WMM" : "HiPerf"));

	if (local) {
		switch (val) {
		case WNI_CFG_EDCA_PROFILE_WMM:
			prf = &wme_l[0];
			break;
		case WNI_CFG_EDCA_PROFILE_ETSI_EUROPE:
			prf = &etsi_l[0];
			break;
		case WNI_CFG_EDCA_PROFILE_ANI:
		default:
			prf = &ani_l[0];
			break;
		}
	} else {
		switch (val) {
		case WNI_CFG_EDCA_PROFILE_WMM:
			prf = &wme_b[0];
			break;
		case WNI_CFG_EDCA_PROFILE_ETSI_EUROPE:
			prf = &etsi_b[0];
			break;
		case WNI_CFG_EDCA_PROFILE_ANI:
		default:
			prf = &ani_b[0];
			break;
		}
	}

	for (i = 0; i < 4; i++) {
		uint8_t data[CFG_EDCA_DATA_LEN];

		status = wlan_mlme_get_edca_params(edca_params,
						   (uint8_t *)&data[0],
						   (uint8_t)prf[i]);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("Get failed for ac:%d", i);
			return QDF_STATUS_E_FAILURE;
		}

		for (idx = 0; idx < CFG_EDCA_DATA_LEN; idx++)
			params[i][idx] = (uint32_t) data[idx];
	}
	pe_debug("GetParams: local=%d, profile = %d Done", local, val);

	return QDF_STATUS_SUCCESS;
}

/**
 * broadcast_wmm_of_concurrent_sta_session() - broadcasts wmm info
 * @mac_ctx:          mac global context
 * @session:       pesession entry
 *
 * Return: true if wmm param updated, false if wmm param not updated
 */
static bool
broadcast_wmm_of_concurrent_sta_session(struct mac_context *mac_ctx,
					struct pe_session *session)
{
	uint8_t i, j;
	struct pe_session *concurrent_session = NULL;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		/*
		 * Find another INFRA STA AP session on same operating channel.
		 * The session entry passed to this API is for GO/SoftAP session
		 * that is getting added currently
		 */
		if (!((mac_ctx->lim.gpSession[i].valid == true) &&
		    (mac_ctx->lim.gpSession[i].peSessionId !=
			session->peSessionId) &&
		    (mac_ctx->lim.gpSession[i].curr_op_freq ==
			session->curr_op_freq) &&
		    (mac_ctx->lim.gpSession[i].limSystemRole ==
			eLIM_STA_ROLE)))
			continue;

		concurrent_session = &(mac_ctx->lim.gpSession[i]);
		break;
	}

	if (!concurrent_session)
		return false;

	if (!qdf_mem_cmp(session->gLimEdcaParamsBC,
			 concurrent_session->gLimEdcaParams,
			 sizeof(concurrent_session->gLimEdcaParams)))
		return false;

	/*
	 * Once atleast one concurrent session on same channel is found and WMM
	 * broadcast params for current SoftAP/GO session updated, return
	 */
	for (j = 0; j < QCA_WLAN_AC_ALL; j++) {
		session->gLimEdcaParamsBC[j].aci.acm =
			concurrent_session->gLimEdcaParams[j].aci.acm;
		session->gLimEdcaParamsBC[j].aci.aifsn =
			concurrent_session->gLimEdcaParams[j].aci.aifsn;
		session->gLimEdcaParamsBC[j].cw.min =
			concurrent_session->gLimEdcaParams[j].cw.min;
		session->gLimEdcaParamsBC[j].cw.max =
			concurrent_session->gLimEdcaParams[j].cw.max;
		session->gLimEdcaParamsBC[j].txoplimit =
			concurrent_session->gLimEdcaParams[j].txoplimit;
		pe_debug("QoSUpdateBCast changed again due to concurrent INFRA STA session: AC :%d: AIFSN: %d, ACM %d, CWmin %d, CWmax %d, TxOp %d",
		       j, session->gLimEdcaParamsBC[j].aci.aifsn,
		       session->gLimEdcaParamsBC[j].aci.acm,
		       session->gLimEdcaParamsBC[j].cw.min,
		       session->gLimEdcaParamsBC[j].cw.max,
		       session->gLimEdcaParamsBC[j].txoplimit);
	}
	return true;
}

void sch_qos_update_broadcast(struct mac_context *mac, struct pe_session *pe_session)
{
	uint32_t params[4][CFG_EDCA_DATA_LEN];
	uint32_t cwminidx, cwmaxidx, txopidx;
	uint32_t phyMode;
	uint8_t i;
	bool updated = false;
	QDF_STATUS status;

	if (sch_get_params(mac, params, false) != QDF_STATUS_SUCCESS) {
		pe_debug("QosUpdateBroadcast: failed");
		return;
	}
	lim_get_phy_mode(mac, &phyMode, pe_session);

	pe_debug("QosUpdBcast: mode %d", phyMode);

	if (phyMode == WNI_CFG_PHY_MODE_11G) {
		cwminidx = CFG_EDCA_PROFILE_CWMING_IDX;
		cwmaxidx = CFG_EDCA_PROFILE_CWMAXG_IDX;
		txopidx = CFG_EDCA_PROFILE_TXOPG_IDX;
	} else if (phyMode == WNI_CFG_PHY_MODE_11B) {
		cwminidx = CFG_EDCA_PROFILE_CWMINB_IDX;
		cwmaxidx = CFG_EDCA_PROFILE_CWMAXB_IDX;
		txopidx = CFG_EDCA_PROFILE_TXOPB_IDX;
	} else {
		/* This can happen if mode is not set yet, assume 11a mode */
		cwminidx = CFG_EDCA_PROFILE_CWMINA_IDX;
		cwmaxidx = CFG_EDCA_PROFILE_CWMAXA_IDX;
		txopidx = CFG_EDCA_PROFILE_TXOPA_IDX;
	}

	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		if (pe_session->gLimEdcaParamsBC[i].aci.acm !=
			(uint8_t)params[i][CFG_EDCA_PROFILE_ACM_IDX]) {
			pe_session->gLimEdcaParamsBC[i].aci.acm =
			(uint8_t)params[i][CFG_EDCA_PROFILE_ACM_IDX];
			updated = true;
		}
		if (pe_session->gLimEdcaParamsBC[i].aci.aifsn !=
			(uint8_t)params[i][CFG_EDCA_PROFILE_AIFSN_IDX]) {
			pe_session->gLimEdcaParamsBC[i].aci.aifsn =
			(uint8_t)params[i][CFG_EDCA_PROFILE_AIFSN_IDX];
			updated = true;
		}
		if (pe_session->gLimEdcaParamsBC[i].cw.min !=
			convert_cw(GET_CW(&params[i][cwminidx]))) {
			pe_session->gLimEdcaParamsBC[i].cw.min =
			convert_cw(GET_CW(&params[i][cwminidx]));
			updated = true;
		}
		if (pe_session->gLimEdcaParamsBC[i].cw.max !=
			convert_cw(GET_CW(&params[i][cwmaxidx]))) {
			pe_session->gLimEdcaParamsBC[i].cw.max =
			convert_cw(GET_CW(&params[i][cwmaxidx]));
			updated = true;
		}
		if (pe_session->gLimEdcaParamsBC[i].txoplimit !=
			(uint16_t)params[i][txopidx]) {
			pe_session->gLimEdcaParamsBC[i].txoplimit =
			(uint16_t)params[i][txopidx];
			updated = true;
		}

		pe_debug("QoSUpdateBCast: AC :%d: AIFSN: %d, ACM %d, CWmin %d, CWmax %d, TxOp %d",
			       i, pe_session->gLimEdcaParamsBC[i].aci.aifsn,
			       pe_session->gLimEdcaParamsBC[i].aci.acm,
			       pe_session->gLimEdcaParamsBC[i].cw.min,
			       pe_session->gLimEdcaParamsBC[i].cw.max,
			       pe_session->gLimEdcaParamsBC[i].txoplimit);

	}

	/*
	 * If there exists a concurrent STA-AP session, use its WMM
	 * params to broadcast in beacons. WFA Wifi Direct test plan
	 * 6.1.14 requirement
	 */
	if (broadcast_wmm_of_concurrent_sta_session(mac, pe_session))
		updated = true;
	if (updated)
		pe_session->gLimEdcaParamSetCount++;

	status = sch_set_fixed_beacon_fields(mac, pe_session);
	if (QDF_IS_STATUS_ERROR(status))
		pe_err("Unable to set beacon fields!");
}

void sch_qos_update_local(struct mac_context *mac, struct pe_session *pe_session)
{

	uint32_t params[4][CFG_EDCA_DATA_LEN];
	QDF_STATUS status;

	status = sch_get_params(mac, params, true /*local */);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("sch_get_params(local) failed");
		return;
	}

	set_sch_edca_params(mac, params, pe_session);

	/* For AP, the bssID is stored in LIM Global context. */
	lim_send_edca_params(mac, pe_session->gLimEdcaParams,
			     pe_session->vdev_id, false);
}

/**
 * sch_set_default_edca_params() - This function sets the gLimEdcaParams to the
 * default local wmm profile.
 * @mac - Global mac context
 * @pe_session - PE session
 *
 * return none
 */
void sch_set_default_edca_params(struct mac_context *mac, struct pe_session *pe_session)
{
	uint32_t params[4][CFG_EDCA_DATA_LEN];

	if (get_wmm_local_params(mac, params) != QDF_STATUS_SUCCESS) {
		pe_err("get_wmm_local_params() failed");
		return;
	}

	set_sch_edca_params(mac, params, pe_session);
	return;
}

/**
 * set_sch_edca_params() - This function fills in the gLimEdcaParams structure
 * with the given edca params.
 * @mac - global mac context
 * @pe_session - PE session
 * @params - EDCA parameters
 *
 * Return  none
 */
static void
set_sch_edca_params(struct mac_context *mac,
		    uint32_t params[][CFG_EDCA_DATA_LEN],
		    struct pe_session *pe_session)
{
	uint32_t i;
	uint32_t cwminidx, cwmaxidx, txopidx;
	uint32_t phyMode;

	lim_get_phy_mode(mac, &phyMode, pe_session);

	pe_debug("lim_get_phy_mode() = %d", phyMode);
	/* if (mac->lim.gLimPhyMode == WNI_CFG_PHY_MODE_11G) */
	if (phyMode == WNI_CFG_PHY_MODE_11G) {
		cwminidx = CFG_EDCA_PROFILE_CWMING_IDX;
		cwmaxidx = CFG_EDCA_PROFILE_CWMAXG_IDX;
		txopidx = CFG_EDCA_PROFILE_TXOPG_IDX;
	}
	/* else if (mac->lim.gLimPhyMode == WNI_CFG_PHY_MODE_11B) */
	else if (phyMode == WNI_CFG_PHY_MODE_11B) {
		cwminidx = CFG_EDCA_PROFILE_CWMINB_IDX;
		cwmaxidx = CFG_EDCA_PROFILE_CWMAXB_IDX;
		txopidx = CFG_EDCA_PROFILE_TXOPB_IDX;
	} else {
		/* This can happen if mode is not set yet, assume 11a mode */
		cwminidx = CFG_EDCA_PROFILE_CWMINA_IDX;
		cwmaxidx = CFG_EDCA_PROFILE_CWMAXA_IDX;
		txopidx = CFG_EDCA_PROFILE_TXOPA_IDX;
	}

	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		pe_session->gLimEdcaParams[i].aci.acm =
			(uint8_t)params[i][CFG_EDCA_PROFILE_ACM_IDX];
		pe_session->gLimEdcaParams[i].aci.aifsn =
			(uint8_t)params[i][CFG_EDCA_PROFILE_AIFSN_IDX];
		pe_session->gLimEdcaParams[i].cw.min =
			convert_cw(GET_CW(&params[i][cwminidx]));
		pe_session->gLimEdcaParams[i].cw.max =
			convert_cw(GET_CW(&params[i][cwmaxidx]));
		pe_session->gLimEdcaParams[i].txoplimit =
			(uint16_t)params[i][txopidx];

		pe_debug("AC :%d: AIFSN: %d, ACM %d, CWmin %d, CWmax %d, TxOp %d",
			       i, pe_session->gLimEdcaParams[i].aci.aifsn,
			       pe_session->gLimEdcaParams[i].aci.acm,
			       pe_session->gLimEdcaParams[i].cw.min,
			       pe_session->gLimEdcaParams[i].cw.max,
			       pe_session->gLimEdcaParams[i].txoplimit);

	}
	return;
}

/**
 * get_wmm_local_params() - This function gets the WMM local edca parameters.
 * @mac
 * @params[][WNI_CFG_EDCA_ANI_ACBK_LOCAL_LEN]
 *
 * Return  none
 */
static QDF_STATUS
get_wmm_local_params(struct mac_context *mac_ctx,
		     uint32_t params[][CFG_EDCA_DATA_LEN])
{
	uint32_t i, idx;
	QDF_STATUS status;
	struct wlan_mlme_edca_params *edca_params;
	uint32_t wme_l[] = {edca_wme_acbe_local, edca_wme_acbk_local,
			    edca_wme_acvi_local, edca_wme_acvo_local};

	if (!mac_ctx->mlme_cfg) {
		pe_err("NULL mlme cfg");
		return QDF_STATUS_E_FAILURE;
	}

	edca_params = &mac_ctx->mlme_cfg->edca_params;
	for (i = 0; i < 4; i++) {
		uint8_t data[CFG_EDCA_DATA_LEN];

		status = wlan_mlme_get_edca_params(edca_params,
						   (uint8_t *)&data[0],
						   (uint8_t)wme_l[i]);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("Get failed for ac:[%d]", i);
			return QDF_STATUS_E_FAILURE;
		}
		for (idx = 0; idx < CFG_EDCA_DATA_LEN; idx++)
			params[i][idx] = (uint32_t) data[idx];
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * sch_qos_concurrency_update() - This function updates the local and
 *                                broadcast based on STA and SAP
 *                                concurrency. It also updates the
 * edcaParamSetCount, if Broadcast EDCA params are updated based on Concurrency.
 *
 * Return  none
 */
void sch_qos_concurrency_update(void)
{
	lim_send_conc_params_update();
}

/**
 * sch_edca_profile_update() - This function updates the local and broadcast
 * EDCA params in the gLimEdcaParams structure. It also updates the
 * edcaParamSetCount.
 *
 * @mac - global mac context
 *
 * Return  none
 */
void sch_edca_profile_update(struct mac_context *mac, struct pe_session *pe_session)
{
	if (LIM_IS_AP_ROLE(pe_session)) {
		sch_qos_update_local(mac, pe_session);
		sch_qos_update_broadcast(mac, pe_session);
		sch_qos_concurrency_update();
	}
}

/* -------------------------------------------------------------------- */
