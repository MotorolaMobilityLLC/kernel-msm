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

/*
 * This file sch_beacon_process.cc contains beacon processing related
 * functions
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "cds_api.h"
#include "wni_cfg.h"

#include "cfg_ucfg_api.h"
#include "lim_api.h"
#include "utils_api.h"
#include "sch_api.h"

#include "lim_utils.h"
#include "lim_send_messages.h"
#include "rrm_api.h"

#ifdef FEATURE_WLAN_DIAG_SUPPORT
#include "host_diag_core_log.h"
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#include "wma.h"

#include "wlan_lmac_if_def.h"
#include "wlan_reg_services_api.h"

static void
ap_beacon_process_5_ghz(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
			tpSchBeaconStruct bcn_struct,
			tpUpdateBeaconParams bcn_prm, struct pe_session *session,
			uint32_t phy_mode)
{
	tpSirMacMgmtHdr mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);

	if (!session->htCapability)
		return;

	if (bcn_struct->chan_freq != session->curr_op_freq)
		return;

	/* 11a (non HT) AP  overlaps or */
	/* HT AP with HT op mode as mixed overlaps. */
	/* HT AP with HT op mode as overlap legacy overlaps. */
	if (!bcn_struct->HTInfo.present
	    || (eSIR_HT_OP_MODE_MIXED == bcn_struct->HTInfo.opMode)
	    || (eSIR_HT_OP_MODE_OVERLAP_LEGACY == bcn_struct->HTInfo.opMode)) {
		lim_update_overlap_sta_param(mac_ctx, mac_hdr->bssId,
					&(session->gLimOverlap11aParams));

		if (session->gLimOverlap11aParams.numSta
		    && !session->gLimOverlap11aParams.protectionEnabled) {
			lim_update_11a_protection(mac_ctx, true, true,
						 bcn_prm, session);
		}
		return;
	}
	/* HT AP with HT20 op mode overlaps. */
	if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT != bcn_struct->HTInfo.opMode)
		return;

	lim_update_overlap_sta_param(mac_ctx, mac_hdr->bssId,
				     &(session->gLimOverlapHt20Params));

	if (session->gLimOverlapHt20Params.numSta
	    && !session->gLimOverlapHt20Params.protectionEnabled)
		lim_enable_ht20_protection(mac_ctx, true, true,
					   bcn_prm, session);
}

static void
ap_beacon_process_24_ghz(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
			 tpSchBeaconStruct bcn_struct,
			 tpUpdateBeaconParams bcn_prm, struct pe_session *session,
			 uint32_t phy_mode)
{
	tpSirMacMgmtHdr mac_hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	bool tmp_exp = false;
	/* We are 11G AP. */
	if ((phy_mode == WNI_CFG_PHY_MODE_11G) &&
	    (false == session->htCapability)) {
		if (bcn_struct->chan_freq != session->curr_op_freq)
			return;

		tmp_exp = (!bcn_struct->erpPresent &&
			   !bcn_struct->HTInfo.present) ||
			    /* if erp not present then  11B AP overlapping */
			  (!mac_ctx->mlme_cfg->sta.ignore_peer_erp_info &&
			   bcn_struct->erpPresent &&
			   (bcn_struct->erpIEInfo.useProtection ||
			    bcn_struct->erpIEInfo.nonErpPresent));
		if (!tmp_exp)
			return;
#ifdef FEATURE_WLAN_ESE
		if (session->isESEconnection)
			QDF_TRACE(QDF_MODULE_ID_PE,
				  QDF_TRACE_LEVEL_INFO,
				  FL("[INFOLOG]ESE 11g erpPresent=%d useProtection=%d nonErpPresent=%d"),
				  bcn_struct->erpPresent,
				  bcn_struct->erpIEInfo.useProtection,
				  bcn_struct->erpIEInfo.nonErpPresent);
#endif
		lim_enable_overlap11g_protection(mac_ctx, bcn_prm,
						 mac_hdr, session);
		return;
	}
	/* handling the case when HT AP has overlapping legacy BSS. */
	if (!session->htCapability)
		return;

	if (bcn_struct->chan_freq != session->curr_op_freq)
		return;

	tmp_exp = (!bcn_struct->erpPresent && !bcn_struct->HTInfo.present) ||
		    /* if erp not present then  11B AP overlapping */
		   (!mac_ctx->mlme_cfg->sta.ignore_peer_erp_info &&
		    bcn_struct->erpPresent &&
		    (bcn_struct->erpIEInfo.useProtection ||
		     bcn_struct->erpIEInfo.nonErpPresent));
	if (tmp_exp) {
#ifdef FEATURE_WLAN_ESE
		if (session->isESEconnection) {
			QDF_TRACE(QDF_MODULE_ID_PE, QDF_TRACE_LEVEL_INFO,
				  FL("[INFOLOG]ESE 11g erpPresent=%d useProtection=%d nonErpPresent=%d"),
				  bcn_struct->erpPresent,
				  bcn_struct->erpIEInfo.useProtection,
				  bcn_struct->erpIEInfo.nonErpPresent);
		}
#endif
		lim_enable_overlap11g_protection(mac_ctx, bcn_prm,
						 mac_hdr, session);
	}
	/* 11g device overlaps */
	tmp_exp = bcn_struct->erpPresent
		&& !(bcn_struct->erpIEInfo.useProtection
		     || bcn_struct->erpIEInfo.nonErpPresent)
		&& !(bcn_struct->HTInfo.present);
	if (tmp_exp) {
		lim_update_overlap_sta_param(mac_ctx, mac_hdr->bssId,
					     &(session->gLimOverlap11gParams));

		if (session->gLimOverlap11gParams.numSta
		    && !session->gLimOverlap11gParams.protectionEnabled)
			lim_enable_ht_protection_from11g(mac_ctx, true, true,
							 bcn_prm, session);
	}
	/* ht device overlaps.
	 * here we will check for HT related devices only which might need
	 * protection. check for 11b and 11g is already done in the previous
	 * blocks. so we will not check for HT operating mode as MIXED.
	 */
	if (!bcn_struct->HTInfo.present)
		return;

	/*
	 * if we are not already in mixed mode or legacy mode as HT operating
	 * mode and received beacon has HT operating mode as legacy then we need
	 * to enable protection from 11g station. we don't need protection from
	 * 11b because if that's needed then our operating mode would have
	 * already been set to legacy in the previous blocks.
	 */
	if ((eSIR_HT_OP_MODE_OVERLAP_LEGACY == bcn_struct->HTInfo.opMode) &&
		!mac_ctx->mlme_cfg->sap_protection_cfg.ignore_peer_ht_opmode) {
		if (eSIR_HT_OP_MODE_OVERLAP_LEGACY == mac_ctx->lim.gHTOperMode
		    || eSIR_HT_OP_MODE_MIXED == mac_ctx->lim.gHTOperMode)
			return;
		lim_update_overlap_sta_param(mac_ctx, mac_hdr->bssId,
					     &(session->gLimOverlap11gParams));
		if (session->gLimOverlap11gParams.numSta
		    && !session->gLimOverlap11gParams.protectionEnabled)
			lim_enable_ht_protection_from11g(mac_ctx, true, true,
							 bcn_prm, session);
		return;
	}

	if (eSIR_HT_OP_MODE_NO_LEGACY_20MHZ_HT == bcn_struct->HTInfo.opMode) {
		lim_update_overlap_sta_param(mac_ctx, mac_hdr->bssId,
					     &(session->gLimOverlapHt20Params));
		if (session->gLimOverlapHt20Params.numSta
		    && !session->gLimOverlapHt20Params.protectionEnabled)
			lim_enable_ht20_protection(mac_ctx, true, true,
						   bcn_prm, session);
	}
}

/**
 * ap_beacon_process() - processes incoming beacons
 *
 * @mac_ctx:         mac global context
 * @rx_pkt_info:     incoming beacon packet
 * @bcn_struct:      beacon struct
 * @bcn_prm:         beacon params
 * @session:         pe session entry
 *
 * Return: void
 */
static void
ap_beacon_process(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
		  tpSchBeaconStruct bcn_struct,
		  tpUpdateBeaconParams bcn_prm, struct pe_session *session)
{
	uint32_t phy_mode;
	enum reg_wifi_band rf_band = REG_BAND_UNKNOWN;
	/* Get RF band from session */
	rf_band = session->limRFBand;

	lim_get_phy_mode(mac_ctx, &phy_mode, session);

	if (REG_BAND_5G == rf_band)
		ap_beacon_process_5_ghz(mac_ctx, rx_pkt_info, bcn_struct,
					bcn_prm, session, phy_mode);
	else if (REG_BAND_2G == rf_band)
		ap_beacon_process_24_ghz(mac_ctx, rx_pkt_info, bcn_struct,
					 bcn_prm, session, phy_mode);
}

/* -------------------------------------------------------------------- */

/**
 * get_operating_channel_width() - Get operating channel width
 * @stads - station entry.
 *
 * This function returns the operating channel width based on
 * the supported channel width entry.
 *
 * Return: tSirMacHTChannelWidth on success
 */
static tSirMacHTChannelWidth get_operating_channel_width(tpDphHashNode stads)
{
	tSirMacHTChannelWidth ch_width = eHT_CHANNEL_WIDTH_20MHZ;

	if (stads->vhtSupportedChannelWidthSet ==
			WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ)
		ch_width = eHT_CHANNEL_WIDTH_160MHZ;
	else if (stads->vhtSupportedChannelWidthSet ==
			WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ)
		ch_width = eHT_CHANNEL_WIDTH_160MHZ;
	else if (stads->vhtSupportedChannelWidthSet ==
			WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ)
		ch_width = eHT_CHANNEL_WIDTH_80MHZ;
	else if (stads->htSupportedChannelWidthSet)
		ch_width = eHT_CHANNEL_WIDTH_40MHZ;
	else
		ch_width = eHT_CHANNEL_WIDTH_20MHZ;

	return ch_width;
}

/*
 * sch_bcn_process_sta() - Process the received beacon frame for sta
 * @mac_ctx:        mac_ctx
 * @bcn:            beacon struct
 * @rx_pkt_info:    received packet info
 * @session:        pe session pointer
 * @beaconParams:   update beacon params
 * @sendProbeReq:   out flag to indicate if probe rsp is to be sent
 * @pMh:            mac header
 *
 * Process the received beacon frame for sta
 *
 * Return: success of failure of operation
 */
static bool
sch_bcn_process_sta(struct mac_context *mac_ctx,
			       tpSchBeaconStruct bcn,
			       uint8_t *rx_pkt_info,
			       struct pe_session *session,
			       tUpdateBeaconParams *beaconParams,
			       uint8_t *sendProbeReq, tpSirMacMgmtHdr pMh)
{
	uint32_t bi;
	tpDphHashNode sta = NULL;
	QDF_STATUS status;

	/*
	 *  This handles two cases:
	 *  -- Infra STA receiving beacons from AP
	 */

	/**
	 * This is the Beacon received from the AP  we're currently associated
	 * with. Check if there are any changes in AP's capabilities
	 */
	if (bcn->chan_freq != session->curr_op_freq) {
		pe_err("Channel Change freq from %d --> %d - Ignoring beacon!",
		       session->curr_op_freq, bcn->chan_freq);
		return false;
	}

	/*
	 * Ignore bcn as channel switch IE present and csa offload is enabled,
	 * as in CSA offload enabled case FW will send Event to switch channel
	 */
	if (bcn->channelSwitchPresent && wma_is_csa_offload_enabled()) {
		pe_err_rl("Ignore bcn as channel switch IE present and csa offload is enabled");
		return false;
	}

	lim_detect_change_in_ap_capabilities(mac_ctx, bcn, session);
	beaconParams->bss_idx = session->vdev_id;
	qdf_mem_copy((uint8_t *) &session->lastBeaconTimeStamp,
			(uint8_t *) bcn->timeStamp, sizeof(uint64_t));
	session->currentBssBeaconCnt++;
	if (session->bcon_dtim_period != bcn->tim.dtimPeriod) {
		session->bcon_dtim_period = bcn->tim.dtimPeriod;
		lim_send_set_dtim_period(mac_ctx, bcn->tim.dtimPeriod,
				session);
	}
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_RX_MGMT_TSF,
	       session->peSessionId, bcn->timeStamp[0]));
	MTRACE(mac_trace(mac_ctx, TRACE_CODE_RX_MGMT_TSF,
	       session->peSessionId, bcn->timeStamp[1]));

	/* Read beacon interval session Entry */
	bi = session->beaconParams.beaconInterval;
	if (bi != bcn->beaconInterval) {
		pe_debug("Beacon interval changed from %d to %d",
		       bcn->beaconInterval, bi);

		bi = bcn->beaconInterval;
		session->beaconParams.beaconInterval = (uint16_t) bi;
		beaconParams->paramChangeBitmap |= PARAM_BCN_INTERVAL_CHANGED;
		beaconParams->beaconInterval = (uint16_t) bi;
	}

	if (bcn->cfPresent) {
		if (!cfg_in_range(CFG_CFP_PERIOD, bcn->cfParamSet.cfpPeriod)) {
			pe_err("Error in setting CFG item CFP Period");
			return false;
		}
		mac_ctx->mlme_cfg->rates.cfp_period = bcn->cfParamSet.cfpPeriod;
	}

	/* No need to send DTIM Period and Count to HAL/SMAC */
	/* SMAC already parses TIM bit. */
	if (bcn->timPresent) {
		if (cfg_in_range(CFG_DTIM_PERIOD, bcn->tim.dtimPeriod))
			mac_ctx->mlme_cfg->sap_cfg.dtim_interval =
						bcn->tim.dtimPeriod;
	}

	if (mac_ctx->lim.gLimProtectionControl !=
	    MLME_FORCE_POLICY_PROTECTION_DISABLE)
		lim_decide_sta_protection(mac_ctx, bcn, beaconParams, session);

	if (bcn->erpPresent) {
		if (bcn->erpIEInfo.barkerPreambleMode)
			lim_enable_short_preamble(mac_ctx, false,
						  beaconParams, session);
		else
			lim_enable_short_preamble(mac_ctx, true,
						  beaconParams, session);
	}
	lim_update_short_slot(mac_ctx, bcn, beaconParams, session);

	sta = dph_get_hash_entry(mac_ctx, DPH_STA_HASH_INDEX_PEER,
				    &session->dph.dphHashTable);
	if ((bcn->wmeEdcaPresent && session->limWmeEnabled) ||
	    (bcn->edcaPresent && session->limQosEnabled)) {
		if (bcn->edcaParams.qosInfo.count !=
		    session->gLimEdcaParamSetCount) {
			qdf_mem_copy(&sta->qos.peer_edca_params,
				     &bcn->edcaParams,
				     sizeof(bcn->edcaParams));
			status = sch_beacon_edca_process(mac_ctx,
							 &bcn->edcaParams,
							 session);
			if (QDF_IS_STATUS_ERROR(status)) {
				pe_err("EDCA parameter processing error");
			} else if (sta) {
				/* If needed, downgrade the EDCA parameters */
				lim_set_active_edca_params(mac_ctx,
					session->gLimEdcaParams, session);
				lim_send_edca_params(mac_ctx,
					session->gLimEdcaParamsActive,
					session->vdev_id, false);
				sch_qos_concurrency_update();
			} else {
				pe_err("Self Entry missing in Hash Table");
			}
		}
		return true;
	}

	if ((bcn->qosCapabilityPresent && session->limQosEnabled)
	    && (bcn->qosCapability.qosInfo.count !=
		session->gLimEdcaParamSetCount))
		*sendProbeReq = true;

	return true;
}

/**
 * update_nss() - Function to update NSS
 * @mac_ctx: pointer to Global Mac structure
 * @sta_ds: pointer to tpDphHashNode
 * @beacon: pointer to tpSchBeaconStruct
 * @session_entry: pointer to struct pe_session *
 * @mgmt_hdr: pointer to tpSirMacMgmtHdr
 *
 * function to update NSS
 *
 * Return: none
 */
static void update_nss(struct mac_context *mac_ctx, tpDphHashNode sta_ds,
		       tpSchBeaconStruct beacon, struct pe_session *session_entry,
		       tpSirMacMgmtHdr mgmt_hdr)
{
	if (sta_ds->vhtSupportedRxNss != (beacon->OperatingMode.rxNSS + 1)) {
		if (session_entry->nss_forced_1x1) {
			pe_debug("Not Updating NSS for special AP");
			return;
		}
		sta_ds->vhtSupportedRxNss =
			beacon->OperatingMode.rxNSS + 1;
		lim_set_nss_change(mac_ctx, session_entry,
			sta_ds->vhtSupportedRxNss,
			mgmt_hdr->sa);
	}
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
static void
sch_bcn_update_he_ies(struct mac_context *mac_ctx, tpDphHashNode sta_ds,
				struct pe_session *session, tpSchBeaconStruct bcn,
				tpSirMacMgmtHdr mac_hdr)
{
	uint8_t session_bss_col_disabled_flag;
	bool anything_changed = false;

	if (session->is_session_obss_color_collision_det_enabled)
		return;

	if (session->he_op.present && bcn->he_op.present) {
		if (bcn->vendor_he_bss_color_change.present &&
				(session->he_op.bss_color !=
				 bcn->vendor_he_bss_color_change.new_color)) {
			pe_debug("bss color changed from [%d] to [%d]",
				session->he_op.bss_color,
				bcn->vendor_he_bss_color_change.new_color);
			session->he_op.bss_color =
				bcn->vendor_he_bss_color_change.new_color;
			anything_changed = true;
		}
		session_bss_col_disabled_flag = session->he_op.bss_col_disabled;
		if (session_bss_col_disabled_flag !=
				bcn->he_op.bss_col_disabled) {
			pe_debug("color disable flag changed from [%d] to [%d]",
				session->he_op.bss_col_disabled,
				bcn->he_op.bss_col_disabled);
			session->he_op.bss_col_disabled =
				bcn->he_op.bss_col_disabled;
			anything_changed = true;
		}
	}
	if (anything_changed)
		lim_send_he_ie_update(mac_ctx, session);
}
#else
static void
sch_bcn_update_he_ies(struct mac_context *mac_ctx, tpDphHashNode sta_ds,
				struct pe_session *session, tpSchBeaconStruct bcn,
				tpSirMacMgmtHdr mac_hdr)
{
	return;
}
#endif

static void
sch_bcn_update_opmode_change(struct mac_context *mac_ctx, tpDphHashNode sta_ds,
				struct pe_session *session, tpSchBeaconStruct bcn,
				tpSirMacMgmtHdr mac_hdr, uint8_t cb_mode)
{
	bool skip_opmode_update = false;
	uint8_t oper_mode;
	uint32_t fw_vht_ch_wd = wma_get_vht_ch_width();
	uint8_t ch_width = 0;

	/*
	 * Ignore opmode change during channel change The opmode will be updated
	 * with the beacons on new channel once the AP move to new channel.
	 */
	if (session->ch_switch_in_progress) {
		pe_debug("Ignore opmode change as channel switch is in progress");
		return;
	}

	if (session->vhtCapability && bcn->OperatingMode.present) {
		update_nss(mac_ctx, sta_ds, bcn, session, mac_hdr);
		oper_mode = get_operating_channel_width(sta_ds);
		if ((oper_mode == eHT_CHANNEL_WIDTH_80MHZ) &&
		    (bcn->OperatingMode.chanWidth > eHT_CHANNEL_WIDTH_80MHZ))
			skip_opmode_update = true;

		if (WNI_CFG_CHANNEL_BONDING_MODE_DISABLE == cb_mode) {
			/*
			 * if channel bonding is disabled from INI do not
			 * update the chan width
			 */
			pe_debug_rl("CB disabled skip bw update: old[%d] new[%d]",
				    oper_mode,
				    bcn->OperatingMode.chanWidth);
			return;
		}

		if (!skip_opmode_update &&
			((oper_mode != bcn->OperatingMode.chanWidth) ||
			(sta_ds->vhtSupportedRxNss !=
			(bcn->OperatingMode.rxNSS + 1)))) {
			pe_debug("received OpMode Chanwidth %d",
				 bcn->OperatingMode.chanWidth);
			pe_debug("MAC - %0x:%0x:%0x:%0x:%0x:%0x",
			       mac_hdr->sa[0], mac_hdr->sa[1],
			       mac_hdr->sa[2], mac_hdr->sa[3],
			       mac_hdr->sa[4], mac_hdr->sa[5]);

			if ((bcn->OperatingMode.chanWidth >=
				eHT_CHANNEL_WIDTH_160MHZ) &&
				(fw_vht_ch_wd > eHT_CHANNEL_WIDTH_80MHZ)) {
				pe_debug("Updating the CH Width to 160MHz");
				sta_ds->vhtSupportedChannelWidthSet =
					WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ;
				sta_ds->htSupportedChannelWidthSet =
					eHT_CHANNEL_WIDTH_40MHZ;
				ch_width = eHT_CHANNEL_WIDTH_160MHZ;
			} else if (bcn->OperatingMode.chanWidth >=
				eHT_CHANNEL_WIDTH_80MHZ) {
				pe_debug("Updating the CH Width to 80MHz");
				sta_ds->vhtSupportedChannelWidthSet =
					WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
				sta_ds->htSupportedChannelWidthSet =
					eHT_CHANNEL_WIDTH_40MHZ;
				ch_width = eHT_CHANNEL_WIDTH_80MHZ;
			} else if (bcn->OperatingMode.chanWidth ==
				eHT_CHANNEL_WIDTH_40MHZ) {
				pe_debug("Updating the CH Width to 40MHz");
				sta_ds->vhtSupportedChannelWidthSet =
					WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
				sta_ds->htSupportedChannelWidthSet =
					eHT_CHANNEL_WIDTH_40MHZ;
				ch_width = eHT_CHANNEL_WIDTH_40MHZ;
			} else if (bcn->OperatingMode.chanWidth ==
				eHT_CHANNEL_WIDTH_20MHZ) {
				pe_debug("Updating the CH Width to 20MHz");
				sta_ds->vhtSupportedChannelWidthSet =
					WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
				sta_ds->htSupportedChannelWidthSet =
					eHT_CHANNEL_WIDTH_20MHZ;
				ch_width = eHT_CHANNEL_WIDTH_20MHZ;
			}
			lim_check_vht_op_mode_change(mac_ctx, session,
				ch_width, mac_hdr->sa);
			update_nss(mac_ctx, sta_ds, bcn, session, mac_hdr);
		}
		return;
	}

	if (!(session->vhtCapability && bcn->VHTOperation.present))
		return;

	oper_mode = sta_ds->vhtSupportedChannelWidthSet;
	if ((oper_mode == WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ) &&
	    (oper_mode < bcn->VHTOperation.chanWidth))
		skip_opmode_update = true;

	if (WNI_CFG_CHANNEL_BONDING_MODE_DISABLE == cb_mode) {
		/*
		 * if channel bonding is disabled from INI do not
		 * update the chan width
		 */
		pe_debug_rl("CB disabled skip bw update: old[%d] new[%d]",
			    oper_mode, bcn->OperatingMode.chanWidth);

		return;
	}

	if (!skip_opmode_update &&
	    (oper_mode != bcn->VHTOperation.chanWidth)) {
		pe_debug("received VHTOP CHWidth %d",
			 bcn->VHTOperation.chanWidth);
		pe_debug("MAC - %0x:%0x:%0x:%0x:%0x:%0x",
		       mac_hdr->sa[0], mac_hdr->sa[1],
		       mac_hdr->sa[2], mac_hdr->sa[3],
		       mac_hdr->sa[4], mac_hdr->sa[5]);

		if ((bcn->VHTOperation.chanWidth >=
			WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ) &&
			(fw_vht_ch_wd > eHT_CHANNEL_WIDTH_80MHZ)) {
			pe_debug("Updating the CH Width to 160MHz");
			sta_ds->vhtSupportedChannelWidthSet =
				bcn->VHTOperation.chanWidth;
			sta_ds->htSupportedChannelWidthSet =
				eHT_CHANNEL_WIDTH_40MHZ;
			ch_width = eHT_CHANNEL_WIDTH_160MHZ;
		} else if (bcn->VHTOperation.chanWidth >=
			WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ) {
			pe_debug("Updating the CH Width to 80MHz");
			sta_ds->vhtSupportedChannelWidthSet =
				WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ;
			sta_ds->htSupportedChannelWidthSet =
				eHT_CHANNEL_WIDTH_40MHZ;
			ch_width = eHT_CHANNEL_WIDTH_80MHZ;
		} else if (bcn->VHTOperation.chanWidth ==
			WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ) {
			sta_ds->vhtSupportedChannelWidthSet =
				WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
			if (bcn->HTCaps.supportedChannelWidthSet) {
				pe_debug("Updating the CH Width to 40MHz");
				sta_ds->htSupportedChannelWidthSet =
					eHT_CHANNEL_WIDTH_40MHZ;
				ch_width = eHT_CHANNEL_WIDTH_40MHZ;
			} else {
				pe_debug("Updating the CH Width to 20MHz");
				sta_ds->htSupportedChannelWidthSet =
					eHT_CHANNEL_WIDTH_20MHZ;
				ch_width = eHT_CHANNEL_WIDTH_20MHZ;
			}
		}
		lim_check_vht_op_mode_change(mac_ctx, session, ch_width,
						mac_hdr->sa);
	}
}

static void
sch_bcn_process_sta_opmode(struct mac_context *mac_ctx,
			    tpSchBeaconStruct bcn,
			    uint8_t *rx_pkt_info,
			    struct pe_session *session,
			    tUpdateBeaconParams *beaconParams,
			    uint8_t *sendProbeReq, tpSirMacMgmtHdr pMh)
{
	tpDphHashNode sta = NULL;
	uint16_t aid;
	uint8_t cb_mode;

	if (wlan_reg_is_24ghz_ch_freq(session->curr_op_freq)) {
		if (session->force_24ghz_in_ht20)
			cb_mode = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
		else
			cb_mode =
			   mac_ctx->roam.configParam.channelBondingMode24GHz;
	} else
		cb_mode = mac_ctx->roam.configParam.channelBondingMode5GHz;
	/* check for VHT capability */
	sta = dph_lookup_hash_entry(mac_ctx, pMh->sa, &aid,
			&session->dph.dphHashTable);
	if ((!sta))
		return;
	sch_bcn_update_opmode_change(mac_ctx, sta, session, bcn, pMh,
				     cb_mode);
	sch_bcn_update_he_ies(mac_ctx, sta, session, bcn, pMh);
	return;
}

/**
 * get_local_power_constraint_beacon() - extracts local constraint
 * from beacon
 * @bcn: beacon structure
 * @local_constraint: local constraint pointer
 *
 * Return: None
 */
#ifdef FEATURE_WLAN_ESE
static void get_local_power_constraint_beacon(
		tpSchBeaconStruct bcn,
		int8_t *local_constraint)
{
	if (bcn->eseTxPwr.present)
		*local_constraint = bcn->eseTxPwr.power_limit;
}
#else
static void get_local_power_constraint_beacon(
		tpSchBeaconStruct bcn,
		int8_t *local_constraint)
{

}
#endif

static void __sch_beacon_process_for_session(struct mac_context *mac_ctx,
					     tpSchBeaconStruct bcn,
					     uint8_t *rx_pkt_info,
					     struct pe_session *session)
{
	tUpdateBeaconParams beaconParams;
	uint8_t sendProbeReq = false;
	tpSirMacMgmtHdr pMh = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	int8_t local_constraint = 0;
	uint32_t chan_freq = 0;
	struct vdev_mlme_obj *mlme_obj;
	struct wlan_lmac_if_reg_tx_ops *tx_ops;
	bool ap_constraint_change = false, tpe_change = false;
	int8_t regMax = 0, maxTxPower = 0;
	QDF_STATUS status;
	bool skip_tpe = false;
	uint8_t programmed_country[REG_ALPHA2_LEN + 1];
	enum reg_6g_ap_type pwr_type_6g = REG_INDOOR_AP;
	bool ctry_code_match = false;

	qdf_mem_zero(&beaconParams, sizeof(tUpdateBeaconParams));
	beaconParams.paramChangeBitmap = 0;

	if (LIM_IS_STA_ROLE(session)) {
		if (false == sch_bcn_process_sta(mac_ctx, bcn, rx_pkt_info,
						 session, &beaconParams,
						 &sendProbeReq, pMh))
			return;
	}

	/*
	 * For vht session, if opermode ie or vht oper IE is present
	 * bandwidth change will be taken care using these vht IEs.
	 */
	if (!(session->vhtCapability && (bcn->OperatingMode.present ||
	   bcn->VHTOperation.present)) && session->htCapability &&
	   bcn->HTInfo.present)
		lim_update_sta_run_time_ht_switch_chnl_params(mac_ctx,
						&bcn->HTInfo, session);

	if (LIM_IS_STA_ROLE(session))
		sch_bcn_process_sta_opmode(mac_ctx, bcn, rx_pkt_info, session,
					    &beaconParams, &sendProbeReq, pMh);

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(session->vdev);
	if (!mlme_obj) {
		pe_err("vdev component object is NULL");
		return;
	}

	if (!wlan_reg_is_6ghz_chan_freq(bcn->chan_freq)) {
		skip_tpe = wlan_mlme_skip_tpe(mac_ctx->psoc);
	} else {
		if (!bcn->countryInfoParam.countryString[0]) {
			pe_err("Channel is 6G but country IE not present");
			return;
		}
		wlan_reg_read_current_country(mac_ctx->psoc,
					      programmed_country);
		status = wlan_reg_get_6g_power_type_for_ctry(
					bcn->countryInfoParam.countryString,
					programmed_country, &pwr_type_6g,
					&ctry_code_match);
		if (QDF_IS_STATUS_ERROR(status))
			return;
	}

	if (wlan_reg_is_ext_tpc_supported(mac_ctx->psoc)) {
		tx_ops = wlan_reg_get_tx_ops(mac_ctx->psoc);

		lim_parse_tpe_ie(mac_ctx, session, bcn->transmit_power_env,
				 bcn->num_transmit_power_env, &bcn->he_op,
				 &tpe_change);

		if (mac_ctx->mlme_cfg->sta.allow_tpc_from_ap) {
			get_local_power_constraint_beacon(bcn,
							  &local_constraint);

			if (mac_ctx->rrm.rrmPEContext.rrmEnable &&
			    bcn->powerConstraintPresent)
				local_constraint =
				bcn->localPowerConstraint.localPowerConstraints;
		}

		if (local_constraint !=
				mlme_obj->reg_tpc_obj.ap_constraint_power) {
			mlme_obj->reg_tpc_obj.ap_constraint_power =
							local_constraint;
			ap_constraint_change = true;
		}

		if ((ap_constraint_change && local_constraint) ||
		    (tpe_change && !skip_tpe)) {
			lim_calculate_tpc(mac_ctx, session, false, pwr_type_6g,
					  ctry_code_match);

			if (tx_ops->set_tpc_power)
				tx_ops->set_tpc_power(mac_ctx->psoc,
						      session->vdev_id,
						      &mlme_obj->reg_tpc_obj);
			}
	} else {
		/* Obtain the Max Tx power for the current regulatory  */
		regMax = wlan_reg_get_channel_reg_power_for_freq(
					mac_ctx->pdev, session->curr_op_freq);
		local_constraint = regMax;

		if (mac_ctx->mlme_cfg->sta.allow_tpc_from_ap) {
			get_local_power_constraint_beacon(bcn,
							  &local_constraint);

			if (mac_ctx->rrm.rrmPEContext.rrmEnable &&
			    bcn->powerConstraintPresent) {
				local_constraint = regMax;
				local_constraint -=
				bcn->localPowerConstraint.localPowerConstraints;
			}
		}
		mlme_obj->reg_tpc_obj.reg_max[0] = regMax;
		mlme_obj->reg_tpc_obj.ap_constraint_power = local_constraint;
		mlme_obj->reg_tpc_obj.frequency[0] = session->curr_op_freq;

		maxTxPower = lim_get_max_tx_power(mac_ctx, mlme_obj);

		/* If maxTxPower is increased or decreased */
		if (maxTxPower != session->maxTxPower) {
			pe_debug("New maxTx power %d, old pwr %d",
				 maxTxPower, session->maxTxPower);
			pe_debug("regMax %d, local %d", regMax,
				 local_constraint);
			status = lim_send_set_max_tx_power_req(mac_ctx,
							       maxTxPower,
							       session);
			if (status == QDF_STATUS_SUCCESS)
				session->maxTxPower = maxTxPower;
		}
	}
	/* Indicate to LIM that Beacon is received */
	if (bcn->HTInfo.present) {
		chan_freq = wlan_reg_legacy_chan_to_freq(mac_ctx->pdev,
							 bcn->HTInfo.primaryChannel);
		lim_received_hb_handler(mac_ctx, chan_freq, session);
	} else
		lim_received_hb_handler(mac_ctx, bcn->chan_freq, session);

	/*
	 * I don't know if any additional IE is required here. Currently, not
	 * include addIE.
	 */
	if (sendProbeReq)
		lim_send_probe_req_mgmt_frame(mac_ctx, &session->ssId,
			session->bssId, session->curr_op_freq,
			session->self_mac_addr, session->dot11mode, NULL, NULL);

	if ((false == mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running)
	    && beaconParams.paramChangeBitmap) {
		pe_debug("Beacon for session[%d] got changed param change bitmap: 0x%x",
			 session->peSessionId, beaconParams.paramChangeBitmap);
		lim_send_beacon_params(mac_ctx, &beaconParams, session);
	}

	if ((session->opmode == QDF_P2P_CLIENT_MODE) &&
	    session->send_p2p_conf_frame) {
		lim_p2p_oper_chan_change_confirm_action_frame(mac_ctx,
							      session->bssId,
							      session);
		session->send_p2p_conf_frame = false;
	}
}

#ifdef WLAN_FEATURE_11AX_BSS_COLOR
static void ap_update_bss_color_info(struct mac_context *mac_ctx,
						struct pe_session *session,
						uint8_t bss_color)
{
	if (!session)
		return;

	if (bss_color < 1 || bss_color > 63) {
		pe_warn("Invalid BSS color");
		return;
	}

	session->bss_color_info[bss_color - 1].seen_count++;
	session->bss_color_info[bss_color - 1].timestamp =
					qdf_get_system_timestamp();
}

static uint8_t ap_get_new_bss_color(struct mac_context *mac_ctx, struct pe_session *session)
{
	int i;
	uint8_t new_bss_color;
	struct bss_color_info color_info;
	qdf_time_t cur_timestamp;

	if (!session)
		return 0;

	color_info = session->bss_color_info[0];
	new_bss_color = 0;
	cur_timestamp = qdf_get_system_timestamp();
	for (i = 1; i < MAX_BSS_COLOR_VALUE; i++) {
		if (session->bss_color_info[i].seen_count == 0) {
			new_bss_color = i + 1;
			return new_bss_color;
		}

		if (color_info.seen_count >
				session->bss_color_info[i].seen_count &&
				(cur_timestamp - session->bss_color_info[i].
					timestamp) > TIME_BEACON_NOT_UPDATED) {
			color_info = session->bss_color_info[i];
			new_bss_color = i + 1;
		}
	}
	pe_debug("new bss color: %d", new_bss_color);
	return new_bss_color;
}

static void sch_check_bss_color_ie(struct mac_context *mac_ctx,
					struct pe_session *ap_session,
					tSchBeaconStruct *bcn,
					tUpdateBeaconParams *bcn_prm)
{
	/* check bss color in the beacon */
	if (ap_session->he_op.present && !ap_session->he_op.bss_color) {
		if (bcn->he_op.present &&
			(bcn->he_op.bss_color ==
					ap_session->he_op.bss_color)) {
			ap_session->he_op.bss_col_disabled = 1;
			bcn_prm->paramChangeBitmap |=
						PARAM_BSS_COLOR_CHANGED;
			ap_session->he_bss_color_change.countdown =
						BSS_COLOR_SWITCH_COUNTDOWN;
			ap_session->he_bss_color_change.new_color =
					ap_get_new_bss_color(mac_ctx,
								ap_session);
			ap_session->he_op.bss_color = ap_session->
						he_bss_color_change.new_color;
			bcn_prm->bss_color = ap_session->he_op.bss_color;
			bcn_prm->bss_color_disabled =
					ap_session->he_op.bss_col_disabled;
			ap_session->bss_color_changing = 1;
		} else {
			/* update info for the bss color */
			if (bcn->he_op.present)
				ap_update_bss_color_info(mac_ctx,
						ap_session,
						bcn->he_op.bss_color);
		}
	}
}

#else
static void  sch_check_bss_color_ie(struct mac_context *mac_ctx,
					struct pe_session *ap_session,
					tSchBeaconStruct *bcn,
					tUpdateBeaconParams *bcn_prm)
{
}
#endif

void sch_beacon_process_for_ap(struct mac_context *mac_ctx,
				uint8_t session_id,
				uint8_t *rx_pkt_info,
				tSchBeaconStruct *bcn)
{
	struct pe_session *ap_session;
	tUpdateBeaconParams bcn_prm;

	if (!bcn || !rx_pkt_info) {
		pe_debug("bcn %pK or rx_pkt_info %pKis NULL",
			 bcn, rx_pkt_info);
		return;
	}

	ap_session = pe_find_session_by_session_id(mac_ctx, session_id);
	if (!ap_session)
		return;

	if (!LIM_IS_AP_ROLE(ap_session))
		return;

	qdf_mem_zero(&bcn_prm, sizeof(tUpdateBeaconParams));
	bcn_prm.paramChangeBitmap = 0;

	bcn_prm.bss_idx = ap_session->vdev_id;

	if (!ap_session->is_session_obss_color_collision_det_enabled)
		sch_check_bss_color_ie(mac_ctx, ap_session,
					bcn, &bcn_prm);

	if ((ap_session->gLimProtectionControl !=
	     MLME_FORCE_POLICY_PROTECTION_DISABLE) &&
	    !ap_session->is_session_obss_offload_enabled)
		ap_beacon_process(mac_ctx, rx_pkt_info,
					bcn, &bcn_prm, ap_session);

	if ((false == mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running)
	    && bcn_prm.paramChangeBitmap) {
		/* Update the bcn and apply the new settings to HAL */
		sch_set_fixed_beacon_fields(mac_ctx, ap_session);
		pe_debug("Beacon for PE session[%d] got changed",
		       ap_session->peSessionId);
		pe_debug("sending beacon param change bitmap: 0x%x",
		       bcn_prm.paramChangeBitmap);
		lim_send_beacon_params(mac_ctx, &bcn_prm, ap_session);
	}
}

#ifdef WLAN_BCN_RECV_FEATURE
/*
 * sch_send_beacon_report() - To Fill beacon report for
 * each beacon coming from connected peer and sends it
 * to upper layer
 * @mac_ctx: Mac context
 * @beacon_struct: Pointing to beacon structure
 * @session: pointer to the PE session
 *
 * Return: None
 */
static
void sch_send_beacon_report(struct mac_context *mac_ctx,
			    struct sSirProbeRespBeacon *beacon_struct,
			    struct pe_session *session)
{
	struct wlan_beacon_report beacon_report;
	beacon_report_cb sme_bcn_cb;

	sme_bcn_cb = mac_ctx->lim.sme_bcn_rcv_callback;
	if (!sme_bcn_cb)
		return;

	if (!LIM_IS_STA_ROLE(session))
		return;

	if (sir_compare_mac_addr(session->bssId, beacon_struct->bssid)) {
		/* Prepare beacon report from incoming beacon */
		qdf_mem_copy(beacon_report.bssid.bytes, beacon_struct->bssid,
			     sizeof(tSirMacAddr));

		qdf_mem_copy(&beacon_report.time_stamp,
			     &beacon_struct->timeStamp, sizeof(qdf_time_t));
		beacon_report.beacon_interval = beacon_struct->beaconInterval;
		beacon_report.frequency = beacon_struct->chan_freq;

		beacon_report.ssid.length = beacon_struct->ssId.length;
		qdf_mem_copy(&beacon_report.ssid.ssid,
			     &beacon_struct->ssId.ssId,
			     beacon_report.ssid.length);

		beacon_report.boot_time =
				qdf_do_div(qdf_get_monotonic_boottime(),
					   QDF_MC_TIMER_TO_MS_UNIT);

		beacon_report.vdev_id = session->vdev_id;

		/* Send report to upper layer */
		sme_bcn_cb(mac_ctx->hdd_handle, &beacon_report);
	}
}

#else
static inline
void sch_send_beacon_report(struct mac_context *mac_ctx,
			    struct sSirProbeRespBeacon *beacon_struct,
			    struct pe_session *session)
{
}
#endif

/**
 * sch_beacon_process() - process the beacon frame
 * @mac_ctx: mac global context
 * @rx_pkt_info: pointer to buffer descriptor
 * @session: pointer to the PE session
 *
 * Return: None
 */
void
sch_beacon_process(struct mac_context *mac_ctx, uint8_t *rx_pkt_info,
		   struct pe_session *session)
{
	static tSchBeaconStruct bcn;

	if (!session)
		return;
	/* Convert the beacon frame into a structure */
	if (sir_convert_beacon_frame2_struct(mac_ctx, (uint8_t *) rx_pkt_info,
		&bcn) != QDF_STATUS_SUCCESS) {
		pe_err_rl("beacon parsing failed");
		return;
	}

	sch_send_beacon_report(mac_ctx, &bcn, session);
	__sch_beacon_process_for_session(mac_ctx, &bcn, rx_pkt_info, session);
}

/**
 * sch_beacon_edca_process(): Process the EDCA parameter set in the received
 * beacon frame
 *
 * @mac_ctx:    mac global context
 * @edca:       reference to edca parameters in beacon struct
 * @session :   pesession entry
 *
 * @return status of operation
 */
QDF_STATUS
sch_beacon_edca_process(struct mac_context *mac, tSirMacEdcaParamSetIE *edca,
			struct pe_session *session)
{
	uint8_t i;
	bool follow_ap_edca;
#ifdef FEATURE_WLAN_DIAG_SUPPORT
	host_log_qos_edca_pkt_type *log_ptr = NULL;
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

	if (!(mac->mlme_cfg)) {
		pe_err("invalid mlme cfg");
		return QDF_STATUS_E_FAILURE;
	}

	follow_ap_edca = mlme_get_follow_ap_edca_flag(session->vdev);

	session->gLimEdcaParamSetCount = edca->qosInfo.count;
	session->gLimEdcaParams[QCA_WLAN_AC_BE] = edca->acbe;
	session->gLimEdcaParams[QCA_WLAN_AC_BK] = edca->acbk;
	session->gLimEdcaParams[QCA_WLAN_AC_VI] = edca->acvi;
	session->gLimEdcaParams[QCA_WLAN_AC_VO] = edca->acvo;

	if (mac->mlme_cfg->edca_params.enable_edca_params && !follow_ap_edca) {
		session->gLimEdcaParams[QCA_WLAN_AC_VO].aci.aifsn =
			mac->mlme_cfg->edca_params.edca_ac_vo.vo_aifs;
		session->gLimEdcaParams[QCA_WLAN_AC_VI].aci.aifsn =
			mac->mlme_cfg->edca_params.edca_ac_vi.vi_aifs;
		session->gLimEdcaParams[QCA_WLAN_AC_BK].aci.aifsn =
			mac->mlme_cfg->edca_params.edca_ac_bk.bk_aifs;
		session->gLimEdcaParams[QCA_WLAN_AC_BE].aci.aifsn =
			mac->mlme_cfg->edca_params.edca_ac_be.be_aifs;

		session->gLimEdcaParams[QCA_WLAN_AC_VO].cw.min =
			mac->mlme_cfg->edca_params.edca_ac_vo.vo_cwmin;
		session->gLimEdcaParams[QCA_WLAN_AC_VI].cw.min =
			mac->mlme_cfg->edca_params.edca_ac_vi.vi_cwmin;
		session->gLimEdcaParams[QCA_WLAN_AC_BK].cw.min =
			mac->mlme_cfg->edca_params.edca_ac_bk.bk_cwmin;
		session->gLimEdcaParams[QCA_WLAN_AC_BE].cw.min =
			mac->mlme_cfg->edca_params.edca_ac_be.be_cwmin;

		session->gLimEdcaParams[QCA_WLAN_AC_VO].cw.max =
			mac->mlme_cfg->edca_params.edca_ac_vo.vo_cwmax;
		session->gLimEdcaParams[QCA_WLAN_AC_VI].cw.max =
			mac->mlme_cfg->edca_params.edca_ac_vi.vi_cwmax;
		session->gLimEdcaParams[QCA_WLAN_AC_BK].cw.max =
			mac->mlme_cfg->edca_params.edca_ac_bk.bk_cwmax;
		session->gLimEdcaParams[QCA_WLAN_AC_BE].cw.max =
			mac->mlme_cfg->edca_params.edca_ac_be.be_cwmax;
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT
	WLAN_HOST_DIAG_LOG_ALLOC(log_ptr, host_log_qos_edca_pkt_type,
				 LOG_WLAN_QOS_EDCA_C);
	if (log_ptr) {
		log_ptr->aci_be = session->gLimEdcaParams[QCA_WLAN_AC_BE].aci.aci;
		log_ptr->cw_be =
			session->gLimEdcaParams[QCA_WLAN_AC_BE].cw.max << 4
				| session->gLimEdcaParams[QCA_WLAN_AC_BE].cw.min;
		log_ptr->txoplimit_be =
			session->gLimEdcaParams[QCA_WLAN_AC_BE].txoplimit;
		log_ptr->aci_bk =
			session->gLimEdcaParams[QCA_WLAN_AC_BK].aci.aci;
		log_ptr->cw_bk =
			session->gLimEdcaParams[QCA_WLAN_AC_BK].cw.max << 4
				| session->gLimEdcaParams[QCA_WLAN_AC_BK].cw.min;
		log_ptr->txoplimit_bk =
			session->gLimEdcaParams[QCA_WLAN_AC_BK].txoplimit;
		log_ptr->aci_vi =
			session->gLimEdcaParams[QCA_WLAN_AC_VI].aci.aci;
		log_ptr->cw_vi =
			session->gLimEdcaParams[QCA_WLAN_AC_VI].cw.max << 4
				| session->gLimEdcaParams[QCA_WLAN_AC_VI].cw.min;
		log_ptr->txoplimit_vi =
			session->gLimEdcaParams[QCA_WLAN_AC_VI].txoplimit;
		log_ptr->aci_vo =
			session->gLimEdcaParams[QCA_WLAN_AC_VO].aci.aci;
		log_ptr->cw_vo =
			session->gLimEdcaParams[QCA_WLAN_AC_VO].cw.max << 4
				| session->gLimEdcaParams[QCA_WLAN_AC_VO].cw.min;
		log_ptr->txoplimit_vo =
			session->gLimEdcaParams[QCA_WLAN_AC_VO].txoplimit;
	}
	WLAN_HOST_DIAG_LOG_REPORT(log_ptr);
#endif /* FEATURE_WLAN_DIAG_SUPPORT */
	pe_debug("Edca param enabled %d. Updating Local Params to: ",
		 mac->mlme_cfg->edca_params.enable_edca_params);
	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		pe_nofl_debug("AC[%d]:  AIFSN: %d, ACM %d, CWmin %d, CWmax %d, TxOp %d",
			      i, session->gLimEdcaParams[i].aci.aifsn,
			      session->gLimEdcaParams[i].aci.acm,
			      session->gLimEdcaParams[i].cw.min,
			      session->gLimEdcaParams[i].cw.max,
			      session->gLimEdcaParams[i].txoplimit);
	}
	return QDF_STATUS_SUCCESS;
}

void lim_enable_obss_detection_config(struct mac_context *mac_ctx,
				      struct pe_session *session)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!session) {
		pe_err("Invalid session, protection not enabled");
		return;
	}

	if (session->gLimProtectionControl ==
	    MLME_FORCE_POLICY_PROTECTION_DISABLE) {
		pe_err("protectiond disabled, force policy, session %d",
		       session->smeSessionId);
		return;
	}

	if (mac_ctx->mlme_cfg->obss_ht40.obss_detection_offload_enabled) {
		status = lim_obss_send_detection_cfg(mac_ctx, session, true);
		if (QDF_IS_STATUS_ERROR(status)) {
			pe_err("vdev %d: offload enable failed, trying legacy",
			       session->smeSessionId);
			session->is_session_obss_offload_enabled = false;
		} else {
			pe_debug("vdev %d: offload detection enabled",
				 session->smeSessionId);
			session->is_session_obss_offload_enabled = true;
			lim_obss_send_detection_cfg(mac_ctx, session, true);
		}
	}

	if (!mac_ctx->mlme_cfg->obss_ht40.obss_detection_offload_enabled ||
	    QDF_IS_STATUS_ERROR(status)) {
		status = qdf_mc_timer_start(&session->
					    protection_fields_reset_timer,
					    SCH_PROTECTION_RESET_TIME);
		if (QDF_IS_STATUS_ERROR(status))
			pe_err("vdev %d: start timer failed",
			       session->smeSessionId);
		else
			pe_debug("vdev %d: legacy detection enabled",
				 session->smeSessionId);
	}
}

QDF_STATUS lim_obss_generate_detection_config(struct mac_context *mac_ctx,
					      struct pe_session *session,
					      struct obss_detection_cfg *cfg)
{
	uint32_t phy_mode;
	enum reg_wifi_band rf_band = REG_BAND_UNKNOWN;
	struct obss_detection_cfg *cur_detect;

	if (!mac_ctx || !session || !cfg) {
		pe_err("Invalid params mac_ctx %pK, session %pK, cfg %pK",
			mac_ctx, session, cfg);
		return QDF_STATUS_E_INVAL;
	}

	lim_get_phy_mode(mac_ctx, &phy_mode, session);
	rf_band = session->limRFBand;
	qdf_mem_zero(cfg, sizeof(*cfg));
	cur_detect = &session->current_obss_detection;

	pe_debug("band:%d, phy_mode:%d, ht_cap:%d, ht_oper_mode:%d",
		 rf_band, phy_mode, session->htCapability,
		 mac_ctx->lim.gHTOperMode);
	pe_debug("assoc_sta: 11b:%d, 11g:%d, 11a:%d, ht20:%d",
		 session->gLim11bParams.protectionEnabled,
		 session->gLim11gParams.protectionEnabled,
		 session->gLim11aParams.protectionEnabled,
		 session->gLimHt20Params.protectionEnabled);
	pe_debug("obss: 11b:%d, 11g:%d, 11a:%d, ht20:%d",
		 session->gLimOlbcParams.protectionEnabled,
		 session->gLimOverlap11gParams.protectionEnabled,
		 session->gLimOverlap11aParams.protectionEnabled,
		 session->gLimOverlapHt20Params.protectionEnabled);
	pe_debug("detect: b_ap:%d, b_s:%d, g:%d, a:%d, htl:%d, htm:%d, ht20:%d",
		 cur_detect->obss_11b_ap_detect_mode,
		 cur_detect->obss_11b_sta_detect_mode,
		 cur_detect->obss_11g_ap_detect_mode,
		 cur_detect->obss_11a_detect_mode,
		 cur_detect->obss_ht_legacy_detect_mode,
		 cur_detect->obss_ht_mixed_detect_mode,
		 cur_detect->obss_ht_20mhz_detect_mode);

	if (rf_band == REG_BAND_2G) {
		if ((phy_mode == WNI_CFG_PHY_MODE_11G ||
		    session->htCapability) &&
		    !session->gLim11bParams.protectionEnabled) {
			if (!session->gLimOlbcParams.protectionEnabled &&
			    !session->gLimOverlap11gParams.protectionEnabled) {
				cfg->obss_11b_ap_detect_mode =
					OBSS_OFFLOAD_DETECTION_PRESENT;
				cfg->obss_11b_sta_detect_mode =
					OBSS_OFFLOAD_DETECTION_PRESENT;
			} else {
				if (cur_detect->obss_11b_ap_detect_mode ==
				    OBSS_OFFLOAD_DETECTION_PRESENT)
					cfg->obss_11b_ap_detect_mode =
						OBSS_OFFLOAD_DETECTION_ABSENT;
				if (cur_detect->obss_11b_sta_detect_mode ==
				    OBSS_OFFLOAD_DETECTION_PRESENT)
					cfg->obss_11b_sta_detect_mode =
						OBSS_OFFLOAD_DETECTION_ABSENT;
			}
		} else if (session->gLim11bParams.protectionEnabled) {
			session->gLimOlbcParams.protectionEnabled = false;
		}

		if (session->htCapability &&
		    session->cfgProtection.overlapFromllg &&
		    !session->gLim11gParams.protectionEnabled) {
			if (!session->gLimOverlap11gParams.protectionEnabled) {
				cfg->obss_11g_ap_detect_mode =
					OBSS_OFFLOAD_DETECTION_PRESENT;
				cfg->obss_ht_legacy_detect_mode =
					OBSS_OFFLOAD_DETECTION_PRESENT;
				cfg->obss_ht_mixed_detect_mode =
					OBSS_OFFLOAD_DETECTION_PRESENT;
			} else {
				if (cur_detect->obss_11g_ap_detect_mode ==
				    OBSS_OFFLOAD_DETECTION_PRESENT)
					cfg->obss_11g_ap_detect_mode =
						OBSS_OFFLOAD_DETECTION_ABSENT;
				if (cur_detect->obss_ht_legacy_detect_mode ==
				    OBSS_OFFLOAD_DETECTION_PRESENT)
					cfg->obss_ht_legacy_detect_mode =
						OBSS_OFFLOAD_DETECTION_ABSENT;
				if (cur_detect->obss_ht_mixed_detect_mode ==
				    OBSS_OFFLOAD_DETECTION_PRESENT)
					cfg->obss_ht_mixed_detect_mode =
						OBSS_OFFLOAD_DETECTION_ABSENT;
			}
		} else if (session->gLim11gParams.protectionEnabled) {
			session->gLimOverlap11gParams.protectionEnabled = false;
		}

		/* INI related settings */
		if (mac_ctx->mlme_cfg->sta.ignore_peer_erp_info)
			cfg->obss_11b_sta_detect_mode =
				OBSS_OFFLOAD_DETECTION_DISABLED;

		if (mac_ctx->mlme_cfg->sap_protection_cfg.ignore_peer_ht_opmode)
			cfg->obss_ht_legacy_detect_mode =
				OBSS_OFFLOAD_DETECTION_DISABLED;
	}

	if ((rf_band == REG_BAND_5G) && session->htCapability) {
		if (!session->gLim11aParams.protectionEnabled) {
			if (!session->gLimOverlap11aParams.protectionEnabled)
				cfg->obss_11a_detect_mode =
					OBSS_OFFLOAD_DETECTION_PRESENT;
			else if (cur_detect->obss_11a_detect_mode ==
				 OBSS_OFFLOAD_DETECTION_PRESENT)
					cfg->obss_11a_detect_mode =
						OBSS_OFFLOAD_DETECTION_ABSENT;
		} else {
			session->gLimOverlap11aParams.protectionEnabled = false;
		}
	}

	if (((rf_band == REG_BAND_2G) || (rf_band == REG_BAND_5G)) &&
	    session->htCapability) {

		if (!session->gLimHt20Params.protectionEnabled) {
			if (!session->gLimOverlapHt20Params.protectionEnabled) {
				cfg->obss_ht_20mhz_detect_mode =
					OBSS_OFFLOAD_DETECTION_PRESENT;
			} else if (cur_detect->obss_ht_20mhz_detect_mode ==
				   OBSS_OFFLOAD_DETECTION_PRESENT) {
					cfg->obss_ht_20mhz_detect_mode =
					OBSS_OFFLOAD_DETECTION_ABSENT;
			}
		} else {
			session->gLimOverlapHt20Params.protectionEnabled =
				false;
		}
	}

	pe_debug("b_ap:%d, b_s:%d, g:%d, a:%d, ht_le:%d, ht_m:%d, ht_20:%d",
		 cfg->obss_11b_ap_detect_mode,
		 cfg->obss_11b_sta_detect_mode,
		 cfg->obss_11g_ap_detect_mode,
		 cfg->obss_11a_detect_mode,
		 cfg->obss_ht_legacy_detect_mode,
		 cfg->obss_ht_mixed_detect_mode,
		 cfg->obss_ht_20mhz_detect_mode);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS lim_obss_send_detection_cfg(struct mac_context *mac_ctx,
				       struct pe_session *session, bool force)
{
	QDF_STATUS status;
	struct obss_detection_cfg obss_cfg;
	struct wmi_obss_detection_cfg_param *req_param;

	if (!session) {
		pe_err("Invalid session");
		return QDF_STATUS_E_INVAL;
	}

	if (!session->is_session_obss_offload_enabled) {
		pe_debug("obss offload protectiond disabled, session %d",
		       session->smeSessionId);
		/* Send success */
		return QDF_STATUS_SUCCESS;
	}

	if (session->gLimProtectionControl ==
	    MLME_FORCE_POLICY_PROTECTION_DISABLE) {
		pe_debug("protectiond disabled, force from policy, session %d",
		       session->smeSessionId);
		/* Send success */
		return QDF_STATUS_SUCCESS;
	}

	status = lim_obss_generate_detection_config(mac_ctx,
						    session,
						    &obss_cfg);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Failed to generate obss detection cfg, session %d",
		       session->smeSessionId);
		return status;
	}

	if (qdf_mem_cmp(&session->obss_offload_cfg, &obss_cfg, sizeof(obss_cfg))
	    || force) {
		struct scheduler_msg msg = {0};
		req_param = qdf_mem_malloc(sizeof(*req_param));
		if (!req_param)
			return QDF_STATUS_E_NOMEM;
		qdf_mem_copy(&session->obss_offload_cfg, &obss_cfg,
				sizeof(obss_cfg));
		req_param->vdev_id = session->smeSessionId;
		req_param->obss_detect_period_ms = OBSS_DETECTION_PERIOD_MS;
		req_param->obss_11b_ap_detect_mode =
			obss_cfg.obss_11b_ap_detect_mode;
		req_param->obss_11b_sta_detect_mode =
			obss_cfg.obss_11b_sta_detect_mode;
		req_param->obss_11g_ap_detect_mode =
			obss_cfg.obss_11g_ap_detect_mode;
		req_param->obss_11a_detect_mode =
			obss_cfg.obss_11a_detect_mode;
		req_param->obss_ht_legacy_detect_mode =
			obss_cfg.obss_ht_legacy_detect_mode;
		req_param->obss_ht_20mhz_detect_mode =
			obss_cfg.obss_ht_20mhz_detect_mode;
		req_param->obss_ht_mixed_detect_mode =
			obss_cfg.obss_ht_mixed_detect_mode;

		msg.type = WMA_OBSS_DETECTION_REQ;
		msg.bodyptr = req_param;
		msg.reserved = 0;
		status = scheduler_post_message(QDF_MODULE_ID_PE,
						QDF_MODULE_ID_WMA,
						QDF_MODULE_ID_WMA, &msg);
		if (QDF_IS_STATUS_ERROR(status)) {
			qdf_mem_free(req_param);
			return status;
		}
	} else {
		pe_debug("Skiping WMA_OBSS_DETECTION_REQ, force = %d", force);
	}

	return status;
}

QDF_STATUS lim_process_obss_detection_ind(struct mac_context *mac_ctx,
					  struct wmi_obss_detect_info
					  *obss_detection)
{
	QDF_STATUS status;
	uint32_t detect_masks;
	uint32_t reason;
	struct obss_detection_cfg *obss_cfg;
	bool enable;
	struct pe_session *session;
	tUpdateBeaconParams bcn_prm;
	enum reg_wifi_band rf_band = REG_BAND_UNKNOWN;
	struct obss_detection_cfg *cur_detect;

	pe_debug("obss detect ind id %d, reason %d, msk 0x%x, " QDF_MAC_ADDR_FMT,
		 obss_detection->vdev_id, obss_detection->reason,
		 obss_detection->matched_detection_masks,
		 QDF_MAC_ADDR_REF(obss_detection->matched_bssid_addr));

	session = pe_find_session_by_vdev_id(mac_ctx, obss_detection->vdev_id);
	if (!session) {
		pe_err("Failed to get session for id %d",
		       obss_detection->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	if (!LIM_IS_AP_ROLE(session)) {
		pe_err("session %d is not AP", obss_detection->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	if (!session->is_session_obss_offload_enabled) {
		pe_err("Offload already disabled for session %d",
		       obss_detection->vdev_id);
		return QDF_STATUS_SUCCESS;
	}

	reason = obss_detection->reason;
	detect_masks = obss_detection->matched_detection_masks;

	if (reason == OBSS_OFFLOAD_DETECTION_PRESENT) {
		enable = true;
	} else if (reason == OBSS_OFFLOAD_DETECTION_ABSENT) {
		enable = false;
	} else if (reason == OBSS_OFFLOAD_DETECTION_DISABLED) {
		/*
		 * Most common reason for this event-type from firmware
		 * is insufficient memory.
		 * Disable offload OBSS detection and enable legacy-way
		 * of detecting OBSS by parsing beacons.
		 **/
		session->is_session_obss_offload_enabled = false;
		pe_err("FW indicated obss offload disabled");
		pe_err("Enabling host based detection, session %d",
		       obss_detection->vdev_id);

		status = qdf_mc_timer_start(&session->
					    protection_fields_reset_timer,
					    SCH_PROTECTION_RESET_TIME);
		if (QDF_IS_STATUS_ERROR(status))
			pe_err("cannot start protection reset timer");

		return QDF_STATUS_SUCCESS;
	} else {
		pe_err("Invalid reason %d, session %d",
		       obss_detection->reason,
		       obss_detection->vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	rf_band = session->limRFBand;
	qdf_mem_zero(&bcn_prm, sizeof(bcn_prm));
	obss_cfg = &session->obss_offload_cfg;
	cur_detect = &session->current_obss_detection;

	if (OBSS_DETECTION_IS_11B_AP(detect_masks)) {
		if (reason != obss_cfg->obss_11b_ap_detect_mode ||
		    rf_band != REG_BAND_2G)
			goto wrong_detection;

		lim_enable11g_protection(mac_ctx, enable, true,
					 &bcn_prm, session);
		cur_detect->obss_11b_ap_detect_mode = reason;
	}
	if (OBSS_DETECTION_IS_11B_STA(detect_masks)) {
		if (reason != obss_cfg->obss_11b_sta_detect_mode ||
		    rf_band != REG_BAND_2G)
			goto wrong_detection;

		lim_enable11g_protection(mac_ctx, enable, true,
					 &bcn_prm, session);
		cur_detect->obss_11b_sta_detect_mode = reason;
	}
	if (OBSS_DETECTION_IS_11G_AP(detect_masks)) {
		if (reason != obss_cfg->obss_11g_ap_detect_mode ||
		    rf_band != REG_BAND_2G)
			goto wrong_detection;

		lim_enable_ht_protection_from11g(mac_ctx, enable, true,
						 &bcn_prm, session);
		cur_detect->obss_11g_ap_detect_mode = reason;
	}
	if (OBSS_DETECTION_IS_11A(detect_masks)) {
		if (reason != obss_cfg->obss_11a_detect_mode ||
		    rf_band != REG_BAND_5G)
			goto wrong_detection;

		lim_update_11a_protection(mac_ctx, enable, true,
					  &bcn_prm, session);
		cur_detect->obss_11a_detect_mode = reason;
	}
	if (OBSS_DETECTION_IS_HT_LEGACY(detect_masks)) {
		/* for 5GHz, we have only 11a detection, which covers legacy */
		if (reason != obss_cfg->obss_ht_legacy_detect_mode ||
		    rf_band != REG_BAND_2G)
			goto wrong_detection;

		lim_enable_ht_protection_from11g(mac_ctx, enable, true,
						 &bcn_prm, session);
		cur_detect->obss_ht_legacy_detect_mode = reason;
	}
	if (OBSS_DETECTION_IS_HT_MIXED(detect_masks)) {
		/* for 5GHz, we have only 11a detection, which covers ht mix */
		if (reason != obss_cfg->obss_ht_mixed_detect_mode ||
		    rf_band != REG_BAND_2G)
			goto wrong_detection;

		lim_enable_ht_protection_from11g(mac_ctx, enable, true,
						 &bcn_prm, session);
		cur_detect->obss_ht_mixed_detect_mode = reason;
	}
	if (OBSS_DETECTION_IS_HT_20MHZ(detect_masks)) {
		if (reason != obss_cfg->obss_ht_20mhz_detect_mode)
			goto wrong_detection;

		lim_enable_ht20_protection(mac_ctx, enable, true,
					   &bcn_prm, session);
		cur_detect->obss_ht_20mhz_detect_mode = reason;
	}

	if ((false == mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running) &&
	    bcn_prm.paramChangeBitmap) {
		/* Update the bcn and apply the new settings to HAL */
		sch_set_fixed_beacon_fields(mac_ctx, session);
		pe_debug("Beacon for PE session: %d got changed: 0x%x",
			 session->smeSessionId, bcn_prm.paramChangeBitmap);
		if (!QDF_IS_STATUS_SUCCESS(lim_send_beacon_params(
		     mac_ctx, &bcn_prm, session))) {
			pe_err("Failed to send beacon param, session %d",
				obss_detection->vdev_id);
			return QDF_STATUS_E_FAULT;
		}
	}

	status = lim_obss_send_detection_cfg(mac_ctx, session, true);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("Failed to send obss detection cfg, session %d",
			obss_detection->vdev_id);
		return status;
	}

	return QDF_STATUS_SUCCESS;

wrong_detection:
	/*
	 * We may get this wrong detection before FW can update latest cfg,
	 * So keeping log level debug
	 **/
	pe_debug("Wrong detection, session %d", obss_detection->vdev_id);

	return QDF_STATUS_E_INVAL;
}
