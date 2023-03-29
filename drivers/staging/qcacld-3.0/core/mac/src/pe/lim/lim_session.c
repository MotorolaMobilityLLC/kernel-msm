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

/**=========================================================================

   \file  lim_session.c

   \brief implementation for lim Session related APIs

   \author Sunit Bhatia

   ========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include "ani_global.h"
#include "lim_ft_defs.h"
#include "lim_ft.h"
#include "lim_session.h"
#include "lim_utils.h"

#include "sch_api.h"
#include "lim_send_messages.h"
#include "cfg_ucfg_api.h"

#ifdef WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY
static struct sDphHashNode *g_dph_node_array;

QDF_STATUS pe_allocate_dph_node_array_buffer(void)
{
	uint32_t buf_size;

	buf_size = WLAN_MAX_VDEVS * (SIR_SAP_MAX_NUM_PEERS + 1) *
		sizeof(struct sDphHashNode);
	g_dph_node_array = qdf_mem_malloc(buf_size);
	if (!g_dph_node_array)
		return QDF_STATUS_E_NOMEM;

	return QDF_STATUS_SUCCESS;
}

void pe_free_dph_node_array_buffer(void)
{
	qdf_mem_free(g_dph_node_array);
	g_dph_node_array = NULL;
}

static inline
struct sDphHashNode *pe_get_session_dph_node_array(uint8_t session_id)
{
	return &g_dph_node_array[session_id * (SIR_SAP_MAX_NUM_PEERS + 1)];
}

#else /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */
static struct sDphHashNode
	g_dph_node_array[WLAN_MAX_VDEVS][SIR_SAP_MAX_NUM_PEERS + 1];

static inline
struct sDphHashNode *pe_get_session_dph_node_array(uint8_t session_id)
{
	return g_dph_node_array[session_id];
}
#endif /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */

/*--------------------------------------------------------------------------

   \brief pe_init_beacon_params() - Initialize the beaconParams structure

   \param struct pe_session *         - pointer to the session context or NULL if session can not be created.
   \return void
   \sa

   --------------------------------------------------------------------------*/

static void pe_init_beacon_params(struct mac_context *mac,
				  struct pe_session *pe_session)
{
	pe_session->beaconParams.beaconInterval = 0;
	pe_session->beaconParams.fShortPreamble = 0;
	pe_session->beaconParams.llaCoexist = 0;
	pe_session->beaconParams.llbCoexist = 0;
	pe_session->beaconParams.llgCoexist = 0;
	pe_session->beaconParams.ht20Coexist = 0;
	pe_session->beaconParams.llnNonGFCoexist = 0;
	pe_session->beaconParams.fRIFSMode = 0;
	pe_session->beaconParams.fLsigTXOPProtectionFullSupport = 0;
	pe_session->beaconParams.gHTObssMode = 0;

	/* Number of legacy STAs associated */
	qdf_mem_zero((void *)&pe_session->gLim11bParams,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&pe_session->gLim11aParams,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&pe_session->gLim11gParams,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&pe_session->gLimNonGfParams,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&pe_session->gLimHt20Params,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&pe_session->gLimLsigTxopParams,
		    sizeof(tLimProtStaParams));
	qdf_mem_zero((void *)&pe_session->gLimOlbcParams,
		    sizeof(tLimProtStaParams));
}

/*
 * pe_reset_protection_callback() - resets protection structs so that when an AP
 * causing use of protection goes away, corresponding protection bit can be
 * reset
 * @ptr:        pointer to pe_session
 *
 * This function resets protection structs so that when an AP causing use of
 * protection goes away, corresponding protection bit can be reset. This allowes
 * protection bits to be reset once legacy overlapping APs are gone.
 *
 * Return: void
 */
static void pe_reset_protection_callback(void *ptr)
{
	struct pe_session *pe_session_entry = (struct pe_session *)ptr;
	struct mac_context *mac_ctx = pe_session_entry->mac_ctx;
	int8_t i = 0;
	tUpdateBeaconParams beacon_params;
	uint16_t current_protection_state = 0;
	tpDphHashNode station_hash_node = NULL;
	tSirMacHTOperatingMode old_op_mode;
	bool bcn_prms_changed = false;

	if (pe_session_entry->valid == false) {
		pe_err("session already deleted. exiting timer callback");
		return;
	}

	/*
	 * During CAC period, if the callback is triggered, the beacon
	 * template may get updated. Subsequently if the vdev is not up, the
	 * vdev would be made up -- which should not happen during the CAC
	 * period. To avoid this, ignore the protection callback if the session
	 * is not yet up.
	 */
	if (!wma_is_vdev_up(pe_session_entry->smeSessionId)) {
		pe_err("session is not up yet. exiting timer callback");
		return;
	}

	/*
	 * If dfsIncludeChanSwIe is set restrat timer as we are going to change
	 * channel and no point in checking protection mode for this channel.
	 */
	if (pe_session_entry->dfsIncludeChanSwIe) {
		pe_err("CSA going on restart timer");
		goto restart_timer;
	}
	current_protection_state |=
	       pe_session_entry->gLimOverlap11gParams.protectionEnabled        |
	       pe_session_entry->gLimOverlap11aParams.protectionEnabled   << 1 |
	       pe_session_entry->gLimOverlapHt20Params.protectionEnabled  << 2 |
	       pe_session_entry->gLimOverlapNonGfParams.protectionEnabled << 3 |
	       pe_session_entry->gLimOlbcParams.protectionEnabled         << 4;

	pe_debug("old protection state: 0x%04X, new protection state: 0x%04X",
		  pe_session_entry->old_protection_state,
		  current_protection_state);

	qdf_mem_zero(&pe_session_entry->gLimOverlap11gParams,
		     sizeof(pe_session_entry->gLimOverlap11gParams));
	qdf_mem_zero(&pe_session_entry->gLimOverlap11aParams,
		     sizeof(pe_session_entry->gLimOverlap11aParams));
	qdf_mem_zero(&pe_session_entry->gLimOverlapHt20Params,
		     sizeof(pe_session_entry->gLimOverlapHt20Params));
	qdf_mem_zero(&pe_session_entry->gLimOverlapNonGfParams,
		     sizeof(pe_session_entry->gLimOverlapNonGfParams));

	qdf_mem_zero(&pe_session_entry->gLimOlbcParams,
		     sizeof(pe_session_entry->gLimOlbcParams));

	/*
	 * Do not reset fShortPreamble and beaconInterval, as they
	 * are not updated.
	 */
	pe_session_entry->beaconParams.llaCoexist = 0;
	pe_session_entry->beaconParams.llbCoexist = 0;
	pe_session_entry->beaconParams.llgCoexist = 0;
	pe_session_entry->beaconParams.ht20Coexist = 0;
	pe_session_entry->beaconParams.llnNonGFCoexist = 0;
	pe_session_entry->beaconParams.fRIFSMode = 0;
	pe_session_entry->beaconParams.fLsigTXOPProtectionFullSupport = 0;
	pe_session_entry->beaconParams.gHTObssMode = 0;


	old_op_mode = pe_session_entry->htOperMode;
	pe_session_entry->htOperMode = eSIR_HT_OP_MODE_PURE;
	mac_ctx->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;

	qdf_mem_zero(&beacon_params, sizeof(tUpdateBeaconParams));
	/* index 0, is self node, peers start from 1 */
	for (i = 1 ; i <= mac_ctx->lim.max_sta_of_pe_session; i++) {
		station_hash_node = dph_get_hash_entry(mac_ctx, i,
					&pe_session_entry->dph.dphHashTable);
		if (!station_hash_node)
			continue;
		lim_decide_ap_protection(mac_ctx, station_hash_node->staAddr,
		&beacon_params, pe_session_entry);
	}

	if (pe_session_entry->htOperMode != old_op_mode)
		bcn_prms_changed = true;

	if ((current_protection_state !=
		pe_session_entry->old_protection_state) &&
		(false == mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running)) {
		pe_debug("protection changed, update beacon template");
		/* update beacon fix params and send update to FW */
		qdf_mem_zero(&beacon_params, sizeof(tUpdateBeaconParams));
		beacon_params.bss_idx = pe_session_entry->vdev_id;
		beacon_params.fShortPreamble =
				pe_session_entry->beaconParams.fShortPreamble;
		beacon_params.beaconInterval =
				pe_session_entry->beaconParams.beaconInterval;
		beacon_params.llaCoexist =
				pe_session_entry->beaconParams.llaCoexist;
		beacon_params.llbCoexist =
				pe_session_entry->beaconParams.llbCoexist;
		beacon_params.llgCoexist =
				pe_session_entry->beaconParams.llgCoexist;
		beacon_params.ht20MhzCoexist =
				pe_session_entry->beaconParams.ht20Coexist;
		beacon_params.llnNonGFCoexist =
				pe_session_entry->beaconParams.llnNonGFCoexist;
		beacon_params.fLsigTXOPProtectionFullSupport =
				pe_session_entry->beaconParams.
					fLsigTXOPProtectionFullSupport;
		beacon_params.fRIFSMode =
				pe_session_entry->beaconParams.fRIFSMode;
		beacon_params.vdev_id =
				pe_session_entry->vdev_id;
		beacon_params.paramChangeBitmap |= PARAM_llBCOEXIST_CHANGED;
		bcn_prms_changed = true;
	}

	if (bcn_prms_changed) {
		sch_set_fixed_beacon_fields(mac_ctx, pe_session_entry);
		lim_send_beacon_params(mac_ctx, &beacon_params, pe_session_entry);
	}

	pe_session_entry->old_protection_state = current_protection_state;
restart_timer:
	if (qdf_mc_timer_start(&pe_session_entry->
				protection_fields_reset_timer,
				SCH_PROTECTION_RESET_TIME)
		!= QDF_STATUS_SUCCESS) {
		pe_err("cannot create or start protectionFieldsResetTimer");
	}
}

#ifdef WLAN_FEATURE_11W
/**
 * pe_init_pmf_comeback_timer: init PMF comeback timer
 * @mac_ctx: pointer to global adapter context
 * @session: pe session
 *
 * Return: void
 */
static void
pe_init_pmf_comeback_timer(tpAniSirGlobal mac_ctx, struct pe_session *session)
{
	QDF_STATUS status;

	if (session->opmode != QDF_STA_MODE)
		return;

	pe_debug("init pmf comeback timer for vdev %d", session->vdev_id);
	session->pmf_retry_timer_info.mac = mac_ctx;
	session->pmf_retry_timer_info.vdev_id = session->vdev_id;
	session->pmf_retry_timer_info.retried = false;
	status = qdf_mc_timer_init(
			&session->pmf_retry_timer, QDF_TIMER_TYPE_SW,
			lim_pmf_comeback_timer_callback,
			(void *)&session->pmf_retry_timer_info);
	if (!QDF_IS_STATUS_SUCCESS(status))
		pe_err("cannot init pmf comeback timer");
}
#else
static inline void
pe_init_pmf_comeback_timer(tpAniSirGlobal mac_ctx, struct pe_session *session,
			   uint8_t vdev_id)
{
}
#endif

#ifdef WLAN_FEATURE_FILS_SK
/**
 * pe_delete_fils_info: API to delete fils session info
 * @session: pe session
 *
 * Return: void
 */
void pe_delete_fils_info(struct pe_session *session)
{
	struct pe_fils_session *fils_info;

	if (!session || (session && !session->valid)) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  FL("session is not valid"));
		return;
	}
	fils_info = session->fils_info;
	if (!fils_info) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  FL("fils info not found"));
		return;
	}
	if (fils_info->keyname_nai_data)
		qdf_mem_free(fils_info->keyname_nai_data);
	if (fils_info->fils_erp_reauth_pkt)
		qdf_mem_free(fils_info->fils_erp_reauth_pkt);
	if (fils_info->fils_rrk)
		qdf_mem_free(fils_info->fils_rrk);
	if (fils_info->fils_rik)
		qdf_mem_free(fils_info->fils_rik);
	if (fils_info->fils_eap_finish_pkt)
		qdf_mem_free(fils_info->fils_eap_finish_pkt);
	if (fils_info->fils_rmsk)
		qdf_mem_free(fils_info->fils_rmsk);
	if (fils_info->fils_pmk)
		qdf_mem_free(fils_info->fils_pmk);
	if (fils_info->auth_info.keyname)
		qdf_mem_free(fils_info->auth_info.keyname);
	if (fils_info->auth_info.domain_name)
		qdf_mem_free(fils_info->auth_info.domain_name);
	if (fils_info->hlp_data)
		qdf_mem_free(fils_info->hlp_data);
	qdf_mem_free(fils_info);
	session->fils_info = NULL;
}
/**
 * pe_init_fils_info: API to initialize fils session info elements to null
 * @session: pe session
 *
 * Return: void
 */
static void pe_init_fils_info(struct pe_session *session)
{
	struct pe_fils_session *fils_info;

	if (!session || (session && !session->valid)) {
		QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_DEBUG,
			  FL("session is not valid"));
		return;
	}
	session->fils_info = qdf_mem_malloc(sizeof(struct pe_fils_session));
	fils_info = session->fils_info;
	if (!fils_info)
		return;
	fils_info->keyname_nai_data = NULL;
	fils_info->fils_erp_reauth_pkt = NULL;
	fils_info->fils_rrk = NULL;
	fils_info->fils_rik = NULL;
	fils_info->fils_eap_finish_pkt = NULL;
	fils_info->fils_rmsk = NULL;
	fils_info->fils_pmk = NULL;
	fils_info->auth_info.keyname = NULL;
	fils_info->auth_info.domain_name = NULL;
}
#else
static void pe_delete_fils_info(struct pe_session *session) { }
static void pe_init_fils_info(struct pe_session *session) { }
#endif

/**
 * lim_get_peer_idxpool_size: get number of peer idx pool size
 * @num_sta: Max number of STA
 * @bss_type: BSS type
 *
 * The peer index start from 1 and thus index 0 is not used, so
 * add 1 to the max sta. For STA if TDLS is enabled add 2 as
 * index 1 is reserved for peer BSS.
 *
 * Return: number of peer idx pool size
 */
#ifdef FEATURE_WLAN_TDLS
static inline uint8_t
lim_get_peer_idxpool_size(uint16_t num_sta, enum bss_type bss_type)
{
	/*
	 * In station role, index 1 is reserved for peer
	 * corresponding to AP. For TDLS the index should
	 * start from 2
	 */
	if (bss_type == eSIR_INFRASTRUCTURE_MODE)
		return num_sta + 2;
	else
		return num_sta + 1;

}
#else
static inline uint8_t
lim_get_peer_idxpool_size(uint16_t num_sta, enum bss_type bss_type)
{
	return num_sta + 1;
}
#endif

void lim_set_bcn_probe_filter(struct mac_context *mac_ctx,
				struct pe_session *session,
				uint8_t sap_channel)
{
	struct mgmt_beacon_probe_filter *filter;
	enum bss_type bss_type;
	uint8_t session_id;
	tSirMacAddr *bssid;

	if (!session) {
		pe_err("Invalid session pointer");
		return;
	}

	bss_type = session->bssType;
	session_id = session->peSessionId;
	bssid = &session->bssId;

	if (session_id >= WLAN_MAX_VDEVS) {
		pe_err("Invalid session_id %d of type %d",
			session_id, bss_type);
		return;
	}

	filter = &mac_ctx->bcn_filter;

	if (eSIR_INFRASTRUCTURE_MODE == bss_type) {
		filter->num_sta_sessions++;
		sir_copy_mac_addr(filter->sta_bssid[session_id], *bssid);
		pe_debug("Set filter for STA Session %d bssid "QDF_MAC_ADDR_FMT,
			session_id, QDF_MAC_ADDR_REF(*bssid));
	} else if (eSIR_INFRA_AP_MODE == bss_type) {
		if (!sap_channel) {
			pe_err("SAP Type with invalid channel");
			goto done;
		}
		filter->num_sap_sessions++;
		filter->sap_channel[session_id] = sap_channel;
		pe_debug("Set filter for SAP session %d channel %d",
			session_id, sap_channel);
	}

done:
	pe_debug("sta %d sap %d", filter->num_sta_sessions,
		 filter->num_sap_sessions);
}

void lim_reset_bcn_probe_filter(struct mac_context *mac_ctx,
				struct pe_session *session)
{
	struct mgmt_beacon_probe_filter *filter;
	enum bss_type bss_type;
	uint8_t session_id;

	if (!session) {
		pe_err("Invalid session pointer");
		return;
	}

	bss_type = session->bssType;
	session_id = session->peSessionId;

	if (session_id >= WLAN_MAX_VDEVS) {
		pe_err("Invalid session_id %d of type %d",
			session_id, bss_type);
		return;
	}

	filter = &mac_ctx->bcn_filter;

	if (eSIR_INFRASTRUCTURE_MODE == bss_type) {
		if (filter->num_sta_sessions)
			filter->num_sta_sessions--;
		qdf_mem_zero(&filter->sta_bssid[session_id],
			    sizeof(tSirMacAddr));
		pe_debug("Cleared STA Filter for session %d", session_id);
	} else if (eSIR_INFRA_AP_MODE == bss_type) {
		if (filter->num_sap_sessions)
			filter->num_sap_sessions--;
		filter->sap_channel[session_id] = 0;
		pe_debug("Cleared SAP Filter for session %d", session_id);
	}

	pe_debug("sta %d sap %d", filter->num_sta_sessions,
		 filter->num_sap_sessions);
}

void lim_update_bcn_probe_filter(struct mac_context *mac_ctx,
					struct pe_session *session)
{
	struct mgmt_beacon_probe_filter *filter;
	enum bss_type bss_type;
	uint8_t session_id;

	if (!session) {
		pe_err("Invalid session pointer");
		return;
	}

	bss_type = session->bssType;
	session_id = session->peSessionId;

	if (session_id >= WLAN_MAX_VDEVS) {
		pe_err("Invalid session_id %d of type %d",
			session_id, bss_type);
		return;
	}

	filter = &mac_ctx->bcn_filter;

	if (eSIR_INFRA_AP_MODE == bss_type) {
		filter->sap_channel[session_id] = wlan_reg_freq_to_chan(
			mac_ctx->pdev, session->curr_op_freq);
		pe_debug("Updated SAP Filter for session %d channel %d",
			session_id, filter->sap_channel[session_id]);
	} else {
		pe_debug("Invalid session type %d session id %d",
			bss_type, session_id);
	}

	pe_debug("sta %d sap %d", filter->num_sta_sessions,
		 filter->num_sap_sessions);
}

struct pe_session *pe_create_session(struct mac_context *mac,
				     uint8_t *bssid, uint8_t *sessionId,
				     uint16_t numSta, enum bss_type bssType,
				     uint8_t vdev_id, enum QDF_OPMODE opmode)
{
	QDF_STATUS status;
	uint8_t i;
	struct pe_session *session_ptr;
	struct wlan_objmgr_vdev *vdev;

	for (i = 0; i < mac->lim.maxBssId; i++) {
		/* Find first free room in session table */
		if (mac->lim.gpSession[i].valid == true)
			continue;
		break;
	}

	if (i == mac->lim.maxBssId) {
		pe_err("Session can't be created. Reached max sessions");
		return NULL;
	}

	session_ptr = &mac->lim.gpSession[i];
	qdf_mem_zero((void *)session_ptr, sizeof(struct pe_session));
	/* Allocate space for Station Table for this session. */
	session_ptr->dph.dphHashTable.pHashTable =
		qdf_mem_malloc(sizeof(tpDphHashNode) * (numSta + 1));
	if (!session_ptr->dph.dphHashTable.pHashTable)
		return NULL;

	session_ptr->dph.dphHashTable.pDphNodeArray =
					pe_get_session_dph_node_array(i);
	session_ptr->dph.dphHashTable.size = numSta + 1;
	dph_hash_table_init(mac, &session_ptr->dph.dphHashTable);
	session_ptr->gpLimPeerIdxpool = qdf_mem_malloc(
		sizeof(*(session_ptr->gpLimPeerIdxpool)) *
		lim_get_peer_idxpool_size(numSta, bssType));
	if (!session_ptr->gpLimPeerIdxpool)
		goto free_dp_hash_table;

	session_ptr->freePeerIdxHead = 0;
	session_ptr->freePeerIdxTail = 0;
	session_ptr->gLimNumOfCurrentSTAs = 0;
	/* Copy the BSSID to the session table */
	sir_copy_mac_addr(session_ptr->bssId, bssid);
	if (bssType == eSIR_MONITOR_MODE)
		sir_copy_mac_addr(mac->lim.gpSession[i].self_mac_addr, bssid);
	session_ptr->valid = true;
	/* Initialize the SME and MLM states to IDLE */
	session_ptr->limMlmState = eLIM_MLM_IDLE_STATE;
	session_ptr->limSmeState = eLIM_SME_IDLE_STATE;
	session_ptr->limCurrentAuthType = eSIR_OPEN_SYSTEM;
	pe_init_beacon_params(mac, &mac->lim.gpSession[i]);
	session_ptr->is11Rconnection = false;
#ifdef FEATURE_WLAN_ESE
	session_ptr->isESEconnection = false;
#endif
	session_ptr->isFastTransitionEnabled = false;
	session_ptr->isFastRoamIniFeatureEnabled = false;
	*sessionId = i;
	session_ptr->peSessionId = i;
	session_ptr->bssType = bssType;
	session_ptr->opmode = opmode;
	session_ptr->gLimPhyMode = WNI_CFG_PHY_MODE_11G;
	/* Initialize CB mode variables when session is created */
	session_ptr->htSupportedChannelWidthSet = 0;
	session_ptr->htRecommendedTxWidthSet = 0;
	session_ptr->htSecondaryChannelOffset = 0;
#ifdef FEATURE_WLAN_TDLS
	qdf_mem_zero(session_ptr->peerAIDBitmap,
		    sizeof(session_ptr->peerAIDBitmap));
	session_ptr->tdls_prohibited = false;
	session_ptr->tdls_chan_swit_prohibited = false;
#endif
	lim_update_tdls_set_state_for_fw(session_ptr, true);
	session_ptr->fWaitForProbeRsp = 0;
	session_ptr->fIgnoreCapsChange = 0;
	session_ptr->is_session_obss_color_collision_det_enabled =
		mac->mlme_cfg->obss_ht40.obss_color_collision_offload_enabled;

	pe_debug("Create PE session: %d opmode %d vdev_id %d  BSSID: "QDF_MAC_ADDR_FMT" Max No of STA: %d",
		 *sessionId, opmode, vdev_id, QDF_MAC_ADDR_REF(bssid),
		 numSta);

	if (bssType == eSIR_INFRA_AP_MODE) {
		session_ptr->pSchProbeRspTemplate =
			qdf_mem_malloc(SIR_MAX_PROBE_RESP_SIZE);
		session_ptr->pSchBeaconFrameBegin =
			qdf_mem_malloc(SIR_MAX_BEACON_SIZE);
		session_ptr->pSchBeaconFrameEnd =
			qdf_mem_malloc(SIR_MAX_BEACON_SIZE);
		if ((!session_ptr->pSchProbeRspTemplate)
		    || (!session_ptr->pSchBeaconFrameBegin)
		    || (!session_ptr->pSchBeaconFrameEnd)) {
			goto free_session_attrs;
		}
	}

	/*
	 * Get vdev object from soc which automatically increments
	 * reference count.
	 */
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc,
						    vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		pe_err("vdev is NULL for vdev_id: %u", vdev_id);
		goto free_session_attrs;
	}
	session_ptr->vdev = vdev;
	session_ptr->vdev_id = vdev_id;
	session_ptr->mac_ctx = mac;

	if (eSIR_INFRASTRUCTURE_MODE == bssType)
		lim_ft_open(mac, &mac->lim.gpSession[i]);

	if (eSIR_MONITOR_MODE == bssType)
		lim_ft_open(mac, &mac->lim.gpSession[i]);

	if (eSIR_INFRA_AP_MODE == bssType) {
		session_ptr->old_protection_state = 0;
		session_ptr->is_session_obss_offload_enabled = false;
		session_ptr->is_obss_reset_timer_initialized = false;

		status = qdf_mc_timer_init(&session_ptr->
					   protection_fields_reset_timer,
					   QDF_TIMER_TYPE_SW,
					   pe_reset_protection_callback,
					   (void *)&mac->lim.gpSession[i]);

		if (QDF_IS_STATUS_ERROR(status))
			pe_err("cannot create protection fields reset timer");
		else
			session_ptr->is_obss_reset_timer_initialized = true;

		qdf_wake_lock_create(&session_ptr->ap_ecsa_wakelock,
				     "ap_ecsa_wakelock");
		qdf_runtime_lock_init(&session_ptr->ap_ecsa_runtime_lock);
		status = qdf_mc_timer_init(&session_ptr->ap_ecsa_timer,
					   QDF_TIMER_TYPE_WAKE_APPS,
					   lim_process_ap_ecsa_timeout,
					   (void *)&mac->lim.gpSession[i]);
		if (status != QDF_STATUS_SUCCESS)
			pe_err("cannot create ap_ecsa_timer");
	}
	if (session_ptr->opmode == QDF_STA_MODE)
		session_ptr->is_session_obss_color_collision_det_enabled =
			mac->mlme_cfg->obss_ht40.bss_color_collision_det_sta;
	pe_init_fils_info(session_ptr);
	pe_init_pmf_comeback_timer(mac, session_ptr);
	session_ptr->ht_client_cnt = 0;
	/* following is invalid value since seq number is 12 bit */
	session_ptr->prev_auth_seq_num = 0xFFFF;

	return &mac->lim.gpSession[i];

free_session_attrs:
	qdf_mem_free(session_ptr->gpLimPeerIdxpool);
	qdf_mem_free(session_ptr->pSchProbeRspTemplate);
	qdf_mem_free(session_ptr->pSchBeaconFrameBegin);
	qdf_mem_free(session_ptr->pSchBeaconFrameEnd);

	session_ptr->gpLimPeerIdxpool = NULL;
	session_ptr->pSchProbeRspTemplate = NULL;
	session_ptr->pSchBeaconFrameBegin = NULL;
	session_ptr->pSchBeaconFrameEnd = NULL;

free_dp_hash_table:
	qdf_mem_free(session_ptr->dph.dphHashTable.pHashTable);
	qdf_mem_zero(session_ptr->dph.dphHashTable.pDphNodeArray,
		     sizeof(struct sDphHashNode) * (SIR_SAP_MAX_NUM_PEERS + 1));

	session_ptr->dph.dphHashTable.pHashTable = NULL;
	session_ptr->dph.dphHashTable.pDphNodeArray = NULL;
	session_ptr->valid = false;

	return NULL;
}

/*--------------------------------------------------------------------------
   \brief pe_find_session_by_bssid() - looks up the PE session given the BSSID.

   This function returns the session context and the session ID if the session
   corresponding to the given BSSID is found in the PE session table.

   \param mac                   - pointer to global adapter context
   \param bssid                   - BSSID of the session
   \param sessionId             -session ID is returned here, if session is found.

   \return struct pe_session *         - pointer to the session context or NULL if session is not found.

   \sa
   --------------------------------------------------------------------------*/
struct pe_session *pe_find_session_by_bssid(struct mac_context *mac, uint8_t *bssid,
				     uint8_t *sessionId)
{
	uint8_t i;

	for (i = 0; i < mac->lim.maxBssId; i++) {
		/* If BSSID matches return corresponding tables address */
		if ((mac->lim.gpSession[i].valid)
		    && (sir_compare_mac_addr(mac->lim.gpSession[i].bssId,
					    bssid))) {
			*sessionId = i;
			return &mac->lim.gpSession[i];
		}
	}

	return NULL;

}

struct pe_session *pe_find_session_by_vdev_id(struct mac_context *mac,
					      uint8_t vdev_id)
{
	uint8_t i;

	for (i = 0; i < mac->lim.maxBssId; i++) {
		/* If BSSID matches return corresponding tables address */
		if ((mac->lim.gpSession[i].valid) &&
		    (mac->lim.gpSession[i].vdev_id == vdev_id))
			return &mac->lim.gpSession[i];
	}
	pe_debug("Session lookup fails for vdev_id: %d", vdev_id);

	return NULL;
}

struct pe_session
*pe_find_session_by_vdev_id_and_state(struct mac_context *mac,
				      uint8_t vdev_id,
				      enum eLimMlmStates lim_state)
{
	uint8_t i;

	for (i = 0; i < mac->lim.maxBssId; i++) {
		if (mac->lim.gpSession[i].valid &&
		    mac->lim.gpSession[i].vdev_id == vdev_id &&
		    mac->lim.gpSession[i].limMlmState == lim_state)
			return &mac->lim.gpSession[i];
	}
	pe_debug("Session lookup fails for vdev_id: %d, mlm state: %d",
		 vdev_id, lim_state);

	return NULL;
}

/*--------------------------------------------------------------------------
   \brief pe_find_session_by_session_id() - looks up the PE session given the session ID.

   This function returns the session context  if the session
   corresponding to the given session ID is found in the PE session table.

   \param mac                   - pointer to global adapter context
   \param sessionId             -session ID for which session context needs to be looked up.

   \return struct pe_session *         - pointer to the session context or NULL if session is not found.

   \sa
   --------------------------------------------------------------------------*/
struct pe_session *pe_find_session_by_session_id(struct mac_context *mac,
					  uint8_t sessionId)
{
	if (sessionId >= mac->lim.maxBssId) {
		pe_err("Invalid sessionId: %d", sessionId);
		return NULL;
	}

	if (mac->lim.gpSession[sessionId].valid)
		return &mac->lim.gpSession[sessionId];

	return NULL;
}

#ifdef WLAN_FEATURE_11W
static void lim_clear_pmfcomeback_timer(struct pe_session *session)
{
	if (session->opmode != QDF_STA_MODE)
		return;

	pe_debug("deinit pmf comeback timer for vdev %d", session->vdev_id);
	if (QDF_TIMER_STATE_RUNNING ==
	    qdf_mc_timer_get_current_state(&session->pmf_retry_timer))
		qdf_mc_timer_stop(&session->pmf_retry_timer);
	qdf_mc_timer_destroy(&session->pmf_retry_timer);
	session->pmf_retry_timer_info.retried = false;
}
#else
static void lim_clear_pmfcomeback_timer(struct pe_session *session)
{
}
#endif

/**
 * pe_delete_session() - deletes the PE session given the session ID.
 * @mac_ctx: pointer to global adapter context
 * @session: session to be deleted.
 *
 * Deletes the given PE session
 *
 * Return: void
 */
void pe_delete_session(struct mac_context *mac_ctx, struct pe_session *session)
{
	uint16_t i = 0;
	uint16_t n;
	TX_TIMER *timer_ptr;
	struct wlan_objmgr_vdev *vdev;
	tpSirAssocRsp assoc_rsp;

	if (!session || (session && !session->valid)) {
		pe_debug("session already deleted or not valid");
		return;
	}

	pe_debug("Delete PE session: %d opmode: %d vdev_id: %d BSSID: "QDF_MAC_ADDR_FMT,
		 session->peSessionId, session->opmode, session->vdev_id,
		 QDF_MAC_ADDR_REF(session->bssId));

	lim_reset_bcn_probe_filter(mac_ctx, session);
	lim_sae_auth_cleanup_retry(mac_ctx, session->vdev_id);

	/* Restore default failure timeout */
	if (session->defaultAuthFailureTimeout) {
		pe_debug("Restore default failure timeout");
		if (cfg_in_range(CFG_AUTH_FAILURE_TIMEOUT,
				 session->defaultAuthFailureTimeout))
			mac_ctx->mlme_cfg->timeouts.auth_failure_timeout =
				session->defaultAuthFailureTimeout;
		else
			mac_ctx->mlme_cfg->timeouts.auth_failure_timeout =
				cfg_default(CFG_AUTH_FAILURE_TIMEOUT);
	}

	for (n = 0; n < (mac_ctx->lim.maxStation + 1); n++) {
		timer_ptr = &mac_ctx->lim.lim_timers.gpLimCnfWaitTimer[n];
		if (session->peSessionId == timer_ptr->sessionId)
			if (true == tx_timer_running(timer_ptr))
				tx_timer_deactivate(timer_ptr);
	}

	if (LIM_IS_AP_ROLE(session)) {
		qdf_runtime_lock_deinit(&session->ap_ecsa_runtime_lock);
		qdf_wake_lock_destroy(&session->ap_ecsa_wakelock);
		qdf_mc_timer_stop(&session->protection_fields_reset_timer);
		qdf_mc_timer_destroy(&session->protection_fields_reset_timer);
		session->dfsIncludeChanSwIe = 0;
		qdf_mc_timer_stop(&session->ap_ecsa_timer);
		qdf_mc_timer_destroy(&session->ap_ecsa_timer);
		lim_del_pmf_sa_query_timer(mac_ctx, session);
	}

	/* Delete FT related information */
	lim_ft_cleanup(mac_ctx, session);
	if (session->pLimStartBssReq) {
		qdf_mem_free(session->pLimStartBssReq);
		session->pLimStartBssReq = NULL;
	}

	if (session->lim_join_req) {
		qdf_mem_free(session->lim_join_req);
		session->lim_join_req = NULL;
	}

	if (session->pLimReAssocReq) {
		qdf_mem_free(session->pLimReAssocReq);
		session->pLimReAssocReq = NULL;
	}

	if (session->pLimMlmJoinReq) {
		qdf_mem_free(session->pLimMlmJoinReq);
		session->pLimMlmJoinReq = NULL;
	}

	if (session->dph.dphHashTable.pHashTable) {
		qdf_mem_free(session->dph.dphHashTable.pHashTable);
		session->dph.dphHashTable.pHashTable = NULL;
	}

	if (session->dph.dphHashTable.pDphNodeArray) {
		qdf_mem_zero(session->dph.dphHashTable.pDphNodeArray,
			sizeof(struct sDphHashNode) *
			(SIR_SAP_MAX_NUM_PEERS + 1));
		session->dph.dphHashTable.pDphNodeArray = NULL;
	}

	if (session->gpLimPeerIdxpool) {
		qdf_mem_free(session->gpLimPeerIdxpool);
		session->gpLimPeerIdxpool = NULL;
	}

	if (session->beacon) {
		qdf_mem_free(session->beacon);
		session->beacon = NULL;
		session->bcnLen = 0;
	}

	if (session->assoc_req) {
		qdf_mem_free(session->assoc_req);
		session->assoc_req = NULL;
		session->assocReqLen = 0;
	}

	if (session->assocRsp) {
		qdf_mem_free(session->assocRsp);
		session->assocRsp = NULL;
		session->assocRspLen = 0;
	}

	if (session->parsedAssocReq) {
		tpSirAssocReq tmp_ptr = NULL;
		/* Cleanup the individual allocation first */
		for (i = 0; i < session->dph.dphHashTable.size; i++) {
			if (!session->parsedAssocReq[i])
				continue;
			tmp_ptr = ((tpSirAssocReq)
				  (session->parsedAssocReq[i]));
			if (tmp_ptr->assocReqFrame) {
				qdf_mem_free(tmp_ptr->assocReqFrame);
				tmp_ptr->assocReqFrame = NULL;
				tmp_ptr->assocReqFrameLength = 0;
			}
			qdf_mem_free(session->parsedAssocReq[i]);
			session->parsedAssocReq[i] = NULL;
		}
		/* Cleanup the whole block */
		qdf_mem_free(session->parsedAssocReq);
		session->parsedAssocReq = NULL;
	}
	if (session->limAssocResponseData) {
		assoc_rsp = (tpSirAssocRsp) session->limAssocResponseData;
		qdf_mem_free(assoc_rsp->sha384_ft_subelem.gtk);
		qdf_mem_free(assoc_rsp->sha384_ft_subelem.igtk);
		qdf_mem_free(session->limAssocResponseData);
		session->limAssocResponseData = NULL;
	}
	if (session->pLimMlmReassocRetryReq) {
		qdf_mem_free(session->pLimMlmReassocRetryReq);
		session->pLimMlmReassocRetryReq = NULL;
	}
	if (session->pLimMlmReassocReq) {
		qdf_mem_free(session->pLimMlmReassocReq);
		session->pLimMlmReassocReq = NULL;
	}

	if (session->pSchProbeRspTemplate) {
		qdf_mem_free(session->pSchProbeRspTemplate);
		session->pSchProbeRspTemplate = NULL;
	}

	if (session->pSchBeaconFrameBegin) {
		qdf_mem_free(session->pSchBeaconFrameBegin);
		session->pSchBeaconFrameBegin = NULL;
	}

	if (session->pSchBeaconFrameEnd) {
		qdf_mem_free(session->pSchBeaconFrameEnd);
		session->pSchBeaconFrameEnd = NULL;
	}

	/* Must free the buffer before peSession invalid */
	if (session->add_ie_params.probeRespData_buff) {
		qdf_mem_free(session->add_ie_params.probeRespData_buff);
		session->add_ie_params.probeRespData_buff = NULL;
		session->add_ie_params.probeRespDataLen = 0;
	}
	if (session->add_ie_params.assocRespData_buff) {
		qdf_mem_free(session->add_ie_params.assocRespData_buff);
		session->add_ie_params.assocRespData_buff = NULL;
		session->add_ie_params.assocRespDataLen = 0;
	}
	if (session->add_ie_params.probeRespBCNData_buff) {
		qdf_mem_free(session->add_ie_params.probeRespBCNData_buff);
		session->add_ie_params.probeRespBCNData_buff = NULL;
		session->add_ie_params.probeRespBCNDataLen = 0;
	}
	pe_delete_fils_info(session);
	lim_clear_pmfcomeback_timer(session);
	session->valid = false;

	session->mac_ctx = NULL;

	qdf_mem_zero(session->WEPKeyMaterial,
		     sizeof(session->WEPKeyMaterial));

	if (session->access_policy_vendor_ie)
		qdf_mem_free(session->access_policy_vendor_ie);

	session->access_policy_vendor_ie = NULL;

	if (LIM_IS_AP_ROLE(session))
		lim_check_and_reset_protection_params(mac_ctx);

	vdev = session->vdev;
	session->vdev = NULL;
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}

/*--------------------------------------------------------------------------
   \brief pe_find_session_by_peer_sta() - looks up the PE session given the Station Address.

   This function returns the session context and the session ID if the session
   corresponding to the given station address is found in the PE session table.

   \param mac                   - pointer to global adapter context
   \param sa                       - Peer STA Address of the session
   \param sessionId             -session ID is returned here, if session is found.

   \return struct pe_session *         - pointer to the session context or NULL if session is not found.

   \sa
   --------------------------------------------------------------------------*/

struct pe_session *pe_find_session_by_peer_sta(struct mac_context *mac, uint8_t *sa,
					uint8_t *sessionId)
{
	uint8_t i;
	tpDphHashNode pSta;
	uint16_t aid;

	for (i = 0; i < mac->lim.maxBssId; i++) {
		if ((mac->lim.gpSession[i].valid)) {
			pSta =
				dph_lookup_hash_entry(mac, sa, &aid,
						      &mac->lim.gpSession[i].dph.
						      dphHashTable);
			if (pSta) {
				*sessionId = i;
				return &mac->lim.gpSession[i];
			}
		}
	}

	pe_debug("Session lookup fails for Peer: "QDF_MAC_ADDR_FMT,
		 QDF_MAC_ADDR_REF(sa));
	return NULL;
}

/**
 * pe_find_session_by_scan_id() - looks up the PE session for given scan id
 * @mac_ctx:   pointer to global adapter context
 * @scan_id:   scan id
 *
 * looks up the PE session for given scan id
 *
 * Return: pe session entry for given scan id if found else NULL
 */
struct pe_session *pe_find_session_by_scan_id(struct mac_context *mac_ctx,
				       uint32_t scan_id)
{
	uint8_t i;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		if ((mac_ctx->lim.gpSession[i].valid) &&
		    (mac_ctx->lim.gpSession[i].ftPEContext.pFTPreAuthReq) &&
		    (mac_ctx->lim.gpSession[i].ftPEContext.pFTPreAuthReq
		     ->scan_id == scan_id)) {
			return &mac_ctx->lim.gpSession[i];
		}
	}
	return NULL;
}

/**
 * pe_get_active_session_count() - function to return active pe session count
 *
 * @mac_ctx: pointer to global mac structure
 *
 * returns number of active pe session count
 *
 * Return: 0 if there are no active sessions else return number of active
 *          sessions
 */
uint8_t pe_get_active_session_count(struct mac_context *mac_ctx)
{
	uint8_t i, active_session_count = 0;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++)
		if (mac_ctx->lim.gpSession[i].valid)
			active_session_count++;

	return active_session_count;
}
