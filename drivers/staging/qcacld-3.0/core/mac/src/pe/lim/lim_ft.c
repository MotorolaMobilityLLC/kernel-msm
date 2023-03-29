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

/**=========================================================================

   \brief implementation for PE 11r VoWiFi FT Protocol

   ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include <lim_send_messages.h>
#include <lim_types.h>
#include <lim_ft.h>
#include <lim_ft_defs.h>
#include <lim_utils.h>
#include <lim_prop_exts_utils.h>
#include <lim_assoc_utils.h>
#include <lim_session.h>
#include <lim_admit_control.h>
#include <lim_security_utils.h>
#include "wmm_apsd.h"
#include "wma.h"

/*--------------------------------------------------------------------------
   Initialize the FT variables.
   ------------------------------------------------------------------------*/
void lim_ft_open(struct mac_context *mac, struct pe_session *pe_session)
{
	if (pe_session)
		qdf_mem_zero(&pe_session->ftPEContext, sizeof(tftPEContext));
}

void lim_ft_cleanup_all_ft_sessions(struct mac_context *mac)
{
	/* Wrapper function to cleanup all FT sessions */
	int i;

	for (i = 0; i < mac->lim.maxBssId; i++) {
		if (true == mac->lim.gpSession[i].valid) {
			/* The session is valid, may have FT data */
			lim_ft_cleanup(mac, &mac->lim.gpSession[i]);
		}
	}
}

void lim_ft_cleanup(struct mac_context *mac, struct pe_session *pe_session)
{
	if (!pe_session) {
		pe_err("pe_session is NULL");
		return;
	}

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_debug("pe_session is not in STA mode");
		return;
	}

	if (pe_session->ftPEContext.pFTPreAuthReq) {
		pe_debug("Freeing pFTPreAuthReq: %pK",
			       pe_session->ftPEContext.pFTPreAuthReq);
		if (NULL !=
		    pe_session->ftPEContext.pFTPreAuthReq->
		    pbssDescription) {
			qdf_mem_free(pe_session->ftPEContext.pFTPreAuthReq->
				     pbssDescription);
			pe_session->ftPEContext.pFTPreAuthReq->
			pbssDescription = NULL;
		}
		qdf_mem_free(pe_session->ftPEContext.pFTPreAuthReq);
		pe_session->ftPEContext.pFTPreAuthReq = NULL;
	}

	if (pe_session->ftPEContext.pAddBssReq) {
		qdf_mem_free(pe_session->ftPEContext.pAddBssReq);
		pe_session->ftPEContext.pAddBssReq = NULL;
	}

	if (pe_session->ftPEContext.pAddStaReq) {
		qdf_mem_free(pe_session->ftPEContext.pAddStaReq);
		pe_session->ftPEContext.pAddStaReq = NULL;
	}

	/* The session is being deleted, cleanup the contents */
	qdf_mem_zero(&pe_session->ftPEContext, sizeof(tftPEContext));
}

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
/*------------------------------------------------------------------
 *
 * Create the new Add Bss Req to the new AP.
 * This will be used when we are ready to FT to the new AP.
 * The newly created ft Session entry is passed to this function
 *
 *------------------------------------------------------------------*/
void lim_ft_prepare_add_bss_req(struct mac_context *mac,
		struct pe_session *ft_session,
		struct bss_description *bssDescription)
{
	struct bss_params *pAddBssParams = NULL;
	tAddStaParams *sta_ctx;
	bool chan_width_support = false;
	tSchBeaconStruct *pBeaconStruct;
	tDot11fIEVHTCaps *vht_caps = NULL;

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(ft_session)) {
		pe_err("pe_session is not in STA mode");
		return;
	}

	pBeaconStruct = qdf_mem_malloc(sizeof(tSchBeaconStruct));
	if (!pBeaconStruct)
		return;

	/* Package SIR_HAL_ADD_BSS_REQ message parameters */
	pAddBssParams = qdf_mem_malloc(sizeof(struct bss_params));
	if (!pAddBssParams) {
		qdf_mem_free(pBeaconStruct);
		return;
	}

	lim_extract_ap_capabilities(mac, (uint8_t *) bssDescription->ieFields,
			lim_get_ielen_from_bss_description(bssDescription),
			pBeaconStruct);

	if (mac->lim.gLimProtectionControl !=
	    MLME_FORCE_POLICY_PROTECTION_DISABLE)
		lim_decide_sta_protection_on_assoc(mac, pBeaconStruct,
						   ft_session);

	qdf_mem_copy(pAddBssParams->bssId, bssDescription->bssId,
		     sizeof(tSirMacAddr));
	pAddBssParams->beaconInterval = bssDescription->beaconInterval;

	pAddBssParams->dtimPeriod = pBeaconStruct->tim.dtimPeriod;
	pAddBssParams->updateBss = false;

	pAddBssParams->nwType = bssDescription->nwType;

	pAddBssParams->shortSlotTimeSupported =
		(uint8_t) pBeaconStruct->capabilityInfo.shortSlotTime;
	pAddBssParams->llbCoexist =
		(uint8_t) ft_session->beaconParams.llbCoexist;
#ifdef WLAN_FEATURE_11W
	pAddBssParams->rmfEnabled = ft_session->limRmfEnabled;
#endif
	/* Use the advertised capabilities from the received beacon/PR */
	if (IS_DOT11_MODE_HT(ft_session->dot11mode) &&
	    (pBeaconStruct->HTCaps.present)) {
		chan_width_support =
			lim_get_ht_capability(mac,
					      eHT_SUPPORTED_CHANNEL_WIDTH_SET,
					      ft_session);
		lim_sta_add_bss_update_ht_parameter(bssDescription->chan_freq,
						    &pBeaconStruct->HTCaps,
						    &pBeaconStruct->HTInfo,
						    chan_width_support,
						    pAddBssParams);
		qdf_mem_copy(&pAddBssParams->staContext.capab_info,
			     &pBeaconStruct->capabilityInfo,
			     sizeof(pAddBssParams->staContext.capab_info));
		qdf_mem_copy(&pAddBssParams->staContext.ht_caps,
			     (uint8_t *) &pBeaconStruct->HTCaps +
			     sizeof(uint8_t),
			     sizeof(pAddBssParams->staContext.ht_caps));
	}

	ft_session->htSecondaryChannelOffset =
		pBeaconStruct->HTInfo.secondaryChannelOffset;
	sta_ctx = &pAddBssParams->staContext;
	/*
	 * in lim_extract_ap_capability function intersection of FW
	 * advertised channel width and AP advertised channel
	 * width has been taken into account for calculating
	 * pe_session->ch_width
	 */
	pAddBssParams->ch_width = ft_session->ch_width;
	sta_ctx->ch_width = ft_session->ch_width;

	if (ft_session->vhtCapability &&
	    ft_session->vhtCapabilityPresentInBeacon) {
		pAddBssParams->vhtCapable = pBeaconStruct->VHTCaps.present;
		vht_caps = &pBeaconStruct->VHTCaps;
		lim_update_vhtcaps_assoc_resp(mac, pAddBssParams,
					      vht_caps, ft_session);
	} else if (ft_session->vhtCapability &&
	    pBeaconStruct->vendor_vht_ie.VHTCaps.present) {
		pe_debug("VHT caps are present in vendor specific IE");
		pAddBssParams->vhtCapable =
			pBeaconStruct->vendor_vht_ie.VHTCaps.present;
		vht_caps = &pBeaconStruct->vendor_vht_ie.VHTCaps;
		lim_update_vhtcaps_assoc_resp(mac, pAddBssParams,
					      vht_caps, ft_session);
	} else {
		pAddBssParams->vhtCapable = 0;
	}

	if (lim_is_session_he_capable(ft_session) &&
	    pBeaconStruct->he_cap.present) {
		lim_update_bss_he_capable(mac, pAddBssParams);
		lim_add_bss_he_cfg(pAddBssParams, ft_session);
	}

	pe_debug("SIR_HAL_ADD_BSS_REQ with frequency: %d, width: %d",
		 bssDescription->chan_freq, pAddBssParams->ch_width);

	/* Populate the STA-related parameters here */
	/* Note that the STA here refers to the AP */
	{
		pAddBssParams->staContext.staType = STA_ENTRY_OTHER;

		qdf_mem_copy(pAddBssParams->staContext.bssId,
			     bssDescription->bssId, sizeof(tSirMacAddr));
		pAddBssParams->staContext.listenInterval =
			bssDescription->beaconInterval;

		pAddBssParams->staContext.assocId = 0;
		pAddBssParams->staContext.uAPSD = 0;
		pAddBssParams->staContext.maxSPLen = 0;
		pAddBssParams->staContext.updateSta = false;
		pAddBssParams->staContext.encryptType =
			ft_session->encryptType;
#ifdef WLAN_FEATURE_11W
		pAddBssParams->staContext.rmfEnabled =
			ft_session->limRmfEnabled;
#endif

		if (IS_DOT11_MODE_HT(ft_session->dot11mode) &&
		    (pBeaconStruct->HTCaps.present)) {
			pAddBssParams->staContext.htCapable = 1;
			if (ft_session->vhtCapability &&
			    IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps)) {
				pAddBssParams->staContext.vhtCapable = 1;
				if ((pBeaconStruct->VHTCaps.suBeamFormerCap ||
				     pBeaconStruct->VHTCaps.muBeamformerCap) &&
				    ft_session->vht_config.su_beam_formee)
					sta_ctx->vhtTxBFCapable
						= 1;
				if (pBeaconStruct->VHTCaps.suBeamformeeCap &&
				    ft_session->vht_config.su_beam_former)
					sta_ctx->enable_su_tx_bformer = 1;
			}
			if (lim_is_session_he_capable(ft_session) &&
				pBeaconStruct->he_cap.present)
				lim_intersect_ap_he_caps(ft_session,
					pAddBssParams, pBeaconStruct, NULL);

			pAddBssParams->staContext.mimoPS =
				(tSirMacHTMIMOPowerSaveState) pBeaconStruct->HTCaps.
				mimoPowerSave;
			pAddBssParams->staContext.maxAmpduDensity =
				pBeaconStruct->HTCaps.mpduDensity;
			pAddBssParams->staContext.fShortGI20Mhz =
				(uint8_t) pBeaconStruct->HTCaps.shortGI20MHz;
			pAddBssParams->staContext.fShortGI40Mhz =
				(uint8_t) pBeaconStruct->HTCaps.shortGI40MHz;
			pAddBssParams->staContext.maxAmpduSize =
				pBeaconStruct->HTCaps.maxRxAMPDUFactor;
		}

		if ((ft_session->limWmeEnabled
		     && pBeaconStruct->wmeEdcaPresent)
		    || (ft_session->limQosEnabled
			&& pBeaconStruct->edcaPresent))
			pAddBssParams->staContext.wmmEnabled = 1;
		else
			pAddBssParams->staContext.wmmEnabled = 0;

		pAddBssParams->staContext.wpa_rsn = pBeaconStruct->rsnPresent;
		/* For OSEN Connection AP does not advertise RSN or WPA IE
		 * so from the IEs we get from supplicant we get this info
		 * so for FW to transmit EAPOL message 4 we shall set
		 * wpa_rsn
		 */
		pAddBssParams->staContext.wpa_rsn |=
			(pBeaconStruct->wpaPresent << 1);
		if ((!pAddBssParams->staContext.wpa_rsn)
		    && (ft_session->isOSENConnection))
			pAddBssParams->staContext.wpa_rsn = 1;
		/* Update the rates */
		lim_populate_peer_rate_set(mac,
					   &pAddBssParams->staContext.
					   supportedRates,
					   pBeaconStruct->HTCaps.supportedMCSSet,
					   false, ft_session,
					   &pBeaconStruct->VHTCaps,
					   &pBeaconStruct->he_cap, NULL,
					   bssDescription);
	}

	pAddBssParams->maxTxPower = ft_session->maxTxPower;

#ifdef WLAN_FEATURE_11W
	if (ft_session->limRmfEnabled) {
		pAddBssParams->rmfEnabled = 1;
		pAddBssParams->staContext.rmfEnabled = 1;
	}
#endif
	pAddBssParams->staContext.sessionId = ft_session->peSessionId;
	pAddBssParams->staContext.smesessionId = ft_session->smeSessionId;

	/* Set a new state for MLME */
	if (!lim_is_roam_synch_in_progress(mac->psoc, ft_session)) {
		ft_session->limMlmState =
			eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE;
		MTRACE(mac_trace
			(mac, TRACE_CODE_MLM_STATE,
			ft_session->peSessionId,
			eLIM_MLM_WT_ADD_BSS_RSP_FT_REASSOC_STATE));
	}
	ft_session->ftPEContext.pAddBssReq = pAddBssParams;

	pe_debug("Saving SIR_HAL_ADD_BSS_REQ for pre-auth ap");

	qdf_mem_free(pBeaconStruct);
	return;
}
#endif

#if defined(WLAN_FEATURE_ROAM_OFFLOAD)

/**
 * lim_convert_phymode_to_dot11mode() - get dot11 mode from phymode
 * @phymode: phymode
 *
 * The function is to convert the phymode to corresponding dot11 mode
 *
 * Return: dot11mode.
 */
static uint8_t lim_convert_phymode_to_dot11mode(enum wlan_phymode phymode)
{

	if (IS_WLAN_PHYMODE_HE(phymode))
		return MLME_DOT11_MODE_11AX;

	if (IS_WLAN_PHYMODE_VHT(phymode))
		return MLME_DOT11_MODE_11AC;

	if (IS_WLAN_PHYMODE_HT(phymode))
		return MLME_DOT11_MODE_11N;

	if (phymode == WLAN_PHYMODE_11G)
		return MLME_DOT11_MODE_11G;

	if (phymode == WLAN_PHYMODE_11G_ONLY)
		return MLME_DOT11_MODE_11G_ONLY;

	if (phymode == WLAN_PHYMODE_11A)
		return MLME_DOT11_MODE_11A;

	if (phymode == WLAN_PHYMODE_11B)
		return MLME_DOT11_MODE_11B;

	return MLME_DOT11_MODE_ALL;
}

/**
 * lim_calculate_dot11_mode() - calculate dot11 mode.
 * @mac_context: mac context
 * @bcn: beacon structure
 * @band: reg_wifi_band
 *
 * The function is to calculate dot11 mode in case fw doen't send phy mode.
 *
 * Return: dot11mode.
 */
static uint8_t lim_calculate_dot11_mode(struct mac_context *mac_ctx,
					tSchBeaconStruct *bcn,
					enum reg_wifi_band band)
{
	enum mlme_dot11_mode self_dot11_mode;
	enum mlme_dot11_mode new_dot11_mode;

	self_dot11_mode = mac_ctx->mlme_cfg->dot11_mode.dot11_mode;

	if (band == REG_BAND_2G)
		new_dot11_mode = MLME_DOT11_MODE_11G;
	else
		new_dot11_mode = MLME_DOT11_MODE_11A;

	switch (self_dot11_mode) {
	case MLME_DOT11_MODE_11AX:
	case MLME_DOT11_MODE_11AX_ONLY:
	case MLME_DOT11_MODE_ALL:
		if (bcn->he_cap.present)
			return MLME_DOT11_MODE_11AX;
		else if ((bcn->VHTCaps.present ||
			  bcn->vendor_vht_ie.present) &&
			 (!(band == REG_BAND_2G &&
			  !mac_ctx->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band)
			 ))

			return MLME_DOT11_MODE_11AC;
		else if (bcn->HTCaps.present)
			return MLME_DOT11_MODE_11N;
		/* fallthrough */
	case MLME_DOT11_MODE_11AC:
	case MLME_DOT11_MODE_11AC_ONLY:
		if ((bcn->VHTCaps.present ||
		     bcn->vendor_vht_ie.present) &&
		   (!(band == REG_BAND_2G &&
		    !mac_ctx->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band)
		   ))
			return MLME_DOT11_MODE_11AC;
		else if (bcn->HTCaps.present)
			return MLME_DOT11_MODE_11N;
		/* fallthrough */
	case MLME_DOT11_MODE_11N:
	case MLME_DOT11_MODE_11N_ONLY:
		if (bcn->HTCaps.present)
			return MLME_DOT11_MODE_11N;
		/* fallthrough */
	default:
			return new_dot11_mode;
	}

}

/**
 * lim_fill_dot11mode() - to fill 802.11 mode in FT session
 * @mac_ctx: pointer to mac ctx
 * @ft_session: FT session
 * @pe_session: PE session
 * @bcn: AP beacon pointer
 * @bss_phymode: bss phy mode
 *
 * This API fills FT session's dot11mode either from pe session or
 * from CFG depending on the condition.
 *
 * Return: none
 */
static void lim_fill_dot11mode(struct mac_context *mac_ctx,
			       struct pe_session *ft_session,
			       struct pe_session *pe_session,
			       tSchBeaconStruct *bcn,
			       enum wlan_phymode bss_phymode)
{
	if (pe_session->ftPEContext.pFTPreAuthReq &&
	    !csr_is_roam_offload_enabled(mac_ctx)) {
		ft_session->dot11mode =
			pe_session->ftPEContext.pFTPreAuthReq->dot11mode;
		return;
	}

	if (bss_phymode == WLAN_PHYMODE_AUTO)
		ft_session->dot11mode = lim_calculate_dot11_mode(
							mac_ctx, bcn,
							ft_session->limRFBand);

	else
		ft_session->dot11mode =
			lim_convert_phymode_to_dot11mode(bss_phymode);
}
#elif defined(WLAN_FEATURE_HOST_ROAM)
/**
 * lim_fill_dot11mode() - to fill 802.11 mode in FT session
 * @mac_ctx: pointer to mac ctx
 * @ft_session: FT session
 * @pe_session: PE session
 * @bcn: AP beacon pointer
 * @bss_phymode: bss phy mode
 *
 * This API fills FT session's dot11mode either from pe session.
 *
 * Return: none
 */
static void lim_fill_dot11mode(struct mac_context *mac_ctx,
			       struct pe_session *ft_session,
			       struct pe_session *pe_session,
			       tSchBeaconStruct *bcn,
			       enum wlan_phymode bss_phymode)
{
	ft_session->dot11mode =
			pe_session->ftPEContext.pFTPreAuthReq->dot11mode;
}
#endif

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
/*------------------------------------------------------------------
 *
 * Setup the new session for the pre-auth AP.
 * Return the newly created session entry.
 *
 *------------------------------------------------------------------*/
void lim_fill_ft_session(struct mac_context *mac,
			 struct bss_description *pbssDescription,
			 struct pe_session *ft_session,
			 struct pe_session *pe_session,
			 enum wlan_phymode bss_phymode)
{
	uint8_t currentBssUapsd;
	uint8_t bss_chan_id;
	int8_t localPowerConstraint;
	int8_t regMax;
	tSchBeaconStruct *pBeaconStruct;
	ePhyChanBondState cbEnabledMode;
	struct vdev_mlme_obj *mlme_obj;
	bool is_pwr_constraint;

	pBeaconStruct = qdf_mem_malloc(sizeof(tSchBeaconStruct));
	if (!pBeaconStruct)
		return;

	/* Retrieve the session that was already created and update the entry */
	ft_session->limWmeEnabled = pe_session->limWmeEnabled;
	ft_session->limQosEnabled = pe_session->limQosEnabled;
	ft_session->limWsmEnabled = pe_session->limWsmEnabled;
	ft_session->lim11hEnable = pe_session->lim11hEnable;
	ft_session->isOSENConnection = pe_session->isOSENConnection;
	ft_session->connected_akm = pe_session->connected_akm;

	/* Fields to be filled later */
	ft_session->lim_join_req = NULL;
	ft_session->smeSessionId = pe_session->smeSessionId;

	lim_extract_ap_capabilities(mac, (uint8_t *) pbssDescription->ieFields,
			lim_get_ielen_from_bss_description(pbssDescription),
			pBeaconStruct);

	ft_session->rateSet.numRates =
		pBeaconStruct->supportedRates.numRates;
	qdf_mem_copy(ft_session->rateSet.rate,
		     pBeaconStruct->supportedRates.rate,
		     pBeaconStruct->supportedRates.numRates);

	ft_session->extRateSet.numRates =
		pBeaconStruct->extendedRates.numRates;
	qdf_mem_copy(ft_session->extRateSet.rate,
		     pBeaconStruct->extendedRates.rate,
		     ft_session->extRateSet.numRates);

	ft_session->ssId.length = pBeaconStruct->ssId.length;
	qdf_mem_copy(ft_session->ssId.ssId, pBeaconStruct->ssId.ssId,
		     ft_session->ssId.length);
	/* Copy The channel Id to the session Table */
	bss_chan_id =
		wlan_reg_freq_to_chan(mac->pdev, pbssDescription->chan_freq);
	ft_session->lim_reassoc_chan_freq = pbssDescription->chan_freq;
	ft_session->curr_op_freq = pbssDescription->chan_freq;
	ft_session->limRFBand = lim_get_rf_band(ft_session->curr_op_freq);

	lim_fill_dot11mode(mac, ft_session, pe_session, pBeaconStruct,
			   bss_phymode);
	pe_debug("dot11mode: %d bss_phymode %d", ft_session->dot11mode,
		 bss_phymode);

	ft_session->vhtCapability =
		(IS_DOT11_MODE_VHT(ft_session->dot11mode) &&
		 (IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps) ||
		  IS_BSS_VHT_CAPABLE(pBeaconStruct->vendor_vht_ie.VHTCaps)));
	ft_session->htCapability =
		(IS_DOT11_MODE_HT(ft_session->dot11mode)
		 && pBeaconStruct->HTCaps.present);

	if (IS_DOT11_MODE_HE(ft_session->dot11mode) &&
	    pBeaconStruct->he_cap.present)
		lim_update_session_he_capable(mac, ft_session);

	/* Assign default configured nss value in the new session */
	if (!wlan_reg_is_24ghz_ch_freq(ft_session->curr_op_freq))
		ft_session->vdev_nss = mac->vdev_type_nss_5g.sta;
	else
		ft_session->vdev_nss = mac->vdev_type_nss_2g.sta;

	ft_session->nss = ft_session ->vdev_nss;

	if (ft_session->limRFBand == REG_BAND_2G) {
		cbEnabledMode = mac->roam.configParam.channelBondingMode24GHz;
	} else {
		cbEnabledMode = mac->roam.configParam.channelBondingMode5GHz;
	}
	ft_session->htSupportedChannelWidthSet =
	    (pBeaconStruct->HTInfo.present) ?
	    (cbEnabledMode && pBeaconStruct->HTInfo.recommendedTxWidthSet &&
	     pBeaconStruct->HTCaps.supportedChannelWidthSet) : 0;
	ft_session->htRecommendedTxWidthSet =
		ft_session->htSupportedChannelWidthSet;

	if (IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps) &&
		pBeaconStruct->VHTOperation.present &&
		ft_session->vhtCapability) {
		ft_session->vhtCapabilityPresentInBeacon = 1;
	} else if (IS_BSS_VHT_CAPABLE(pBeaconStruct->vendor_vht_ie.VHTCaps) &&
		    pBeaconStruct->vendor_vht_ie.VHTOperation.present &&
		    ft_session->vhtCapability){
		ft_session->vhtCapabilityPresentInBeacon = 1;
	} else {
		ft_session->vhtCapabilityPresentInBeacon = 0;
	}

	if (ft_session->htRecommendedTxWidthSet) {
		ft_session->ch_width = CH_WIDTH_40MHZ;
		if (ft_session->vhtCapabilityPresentInBeacon &&
				pBeaconStruct->VHTOperation.chanWidth) {
			ft_session->ch_width =
				pBeaconStruct->VHTOperation.chanWidth + 1;
			ft_session->ch_center_freq_seg0 =
			pBeaconStruct->VHTOperation.chan_center_freq_seg0;
			ft_session->ch_center_freq_seg1 =
			pBeaconStruct->VHTOperation.chan_center_freq_seg1;
		} else if (ft_session->vhtCapabilityPresentInBeacon &&
			   pBeaconStruct->vendor_vht_ie.VHTOperation.chanWidth){
			ft_session->ch_width =
			pBeaconStruct->vendor_vht_ie.VHTOperation.chanWidth + 1;
			ft_session->ch_center_freq_seg0 =
		pBeaconStruct->vendor_vht_ie.VHTOperation.chan_center_freq_seg0;
			ft_session->ch_center_freq_seg1 =
		pBeaconStruct->vendor_vht_ie.VHTOperation.chan_center_freq_seg1;

		} else {
			if (pBeaconStruct->HTInfo.secondaryChannelOffset ==
					PHY_DOUBLE_CHANNEL_LOW_PRIMARY)
				ft_session->ch_center_freq_seg0 =
					bss_chan_id + 2;
			else if (pBeaconStruct->HTInfo.secondaryChannelOffset ==
					PHY_DOUBLE_CHANNEL_HIGH_PRIMARY)
				ft_session->ch_center_freq_seg0 =
					bss_chan_id - 2;
			else {
				pe_warn("Invalid sec ch offset");
				ft_session->ch_width = CH_WIDTH_20MHZ;
				ft_session->ch_center_freq_seg0 = 0;
				ft_session->ch_center_freq_seg1 = 0;
			}
		}
	} else {
		ft_session->ch_width = CH_WIDTH_20MHZ;
		ft_session->ch_center_freq_seg0 = 0;
		ft_session->ch_center_freq_seg1 = 0;
	}

	sir_copy_mac_addr(ft_session->self_mac_addr,
			  pe_session->self_mac_addr);
	sir_copy_mac_addr(ft_session->limReAssocbssId,
			  pbssDescription->bssId);
	sir_copy_mac_addr(ft_session->prev_ap_bssid, pe_session->bssId);

	/* Store beaconInterval */
	ft_session->beaconParams.beaconInterval =
		pbssDescription->beaconInterval;
	ft_session->bssType = pe_session->bssType;

	ft_session->statypeForBss = STA_ENTRY_PEER;
	ft_session->nwType = pbssDescription->nwType;


	if (ft_session->bssType == eSIR_INFRASTRUCTURE_MODE) {
		ft_session->limSystemRole = eLIM_STA_ROLE;
	} else {
		/* Throw an error & return & make sure to delete the session */
		pe_warn("Invalid bss type");
	}

	ft_session->limCurrentBssCaps = pbssDescription->capabilityInfo;
	ft_session->limReassocBssCaps = pbssDescription->capabilityInfo;
	if (mac->mlme_cfg->ht_caps.short_slot_time_enabled &&
	    SIR_MAC_GET_SHORT_SLOT_TIME(ft_session->limReassocBssCaps)) {
		ft_session->shortSlotTimeSupported = true;
	}

	mlme_obj = wlan_vdev_mlme_get_cmpt_obj(pe_session->vdev);
	if (!mlme_obj) {
		pe_err("vdev component object is NULL");
		qdf_mem_free(pBeaconStruct);
		return;
	}

	regMax = wlan_reg_get_channel_reg_power_for_freq(
		mac->pdev, ft_session->curr_op_freq);
	localPowerConstraint = regMax;
	lim_extract_ap_capability(mac, (uint8_t *) pbssDescription->ieFields,
		lim_get_ielen_from_bss_description(pbssDescription),
		&ft_session->limCurrentBssQosCaps,
		&currentBssUapsd,
		&localPowerConstraint, ft_session, &is_pwr_constraint);
	if (is_pwr_constraint)
		localPowerConstraint = regMax - localPowerConstraint;

	ft_session->limReassocBssQosCaps =
		ft_session->limCurrentBssQosCaps;

	ft_session->is11Rconnection = pe_session->is11Rconnection;
#ifdef FEATURE_WLAN_ESE
	ft_session->isESEconnection = pe_session->isESEconnection;
	ft_session->is_ese_version_ie_present =
		pBeaconStruct->is_ese_ver_ie_present;
#endif
	ft_session->isFastTransitionEnabled =
		pe_session->isFastTransitionEnabled;

	ft_session->isFastRoamIniFeatureEnabled =
		pe_session->isFastRoamIniFeatureEnabled;

	mlme_obj->reg_tpc_obj.reg_max[0] = regMax;
	mlme_obj->reg_tpc_obj.ap_constraint_power = localPowerConstraint;
	mlme_obj->reg_tpc_obj.frequency[0] = ft_session->curr_op_freq;

#ifdef FEATURE_WLAN_ESE
	ft_session->maxTxPower = lim_get_max_tx_power(mac, mlme_obj);
#else
	ft_session->maxTxPower = QDF_MIN(regMax, (localPowerConstraint));
#endif

	pe_debug("Reg max: %d local pwr: %d, max tx pwr: %d", regMax,
		 localPowerConstraint, ft_session->maxTxPower);
	if (!lim_is_roam_synch_in_progress(mac->psoc, pe_session)) {
		ft_session->limPrevSmeState = ft_session->limSmeState;
		ft_session->limSmeState = eLIM_SME_WT_REASSOC_STATE;
		MTRACE(mac_trace(mac,
				TRACE_CODE_SME_STATE,
				ft_session->peSessionId,
				ft_session->limSmeState));
	}
	ft_session->encryptType = pe_session->encryptType;
#ifdef WLAN_FEATURE_11W
	ft_session->limRmfEnabled = pe_session->limRmfEnabled;
#endif
	if ((ft_session->limRFBand == REG_BAND_2G) &&
		(ft_session->htSupportedChannelWidthSet ==
		eHT_CHANNEL_WIDTH_40MHZ))
		lim_init_obss_params(mac, ft_session);

	ft_session->enableHtSmps = pe_session->enableHtSmps;
	ft_session->htSmpsvalue = pe_session->htSmpsvalue;
	/*
	 * By default supported NSS 1x1 is set to true
	 * and later on updated while determining session
	 * supported rates which is the intersection of
	 * self and peer rates
	 */
	ft_session->supported_nss_1x1 = true;
	pe_debug("FT enable smps: %d mode: %d supported nss 1x1: %d",
		ft_session->enableHtSmps,
		ft_session->htSmpsvalue,
		ft_session->supported_nss_1x1);

	qdf_mem_free(pBeaconStruct);
}
#endif

static void
lim_ft_send_aggr_qos_rsp(struct mac_context *mac, uint8_t rspReqd,
			 struct aggr_add_ts_param *aggrQosRsp,
			 uint8_t smesessionId)
{
	tpSirAggrQosRsp rsp;
	int i = 0;

	if (!rspReqd) {
		return;
	}
	rsp = qdf_mem_malloc(sizeof(tSirAggrQosRsp));
	if (!rsp)
		return;

	rsp->messageType = eWNI_SME_FT_AGGR_QOS_RSP;
	rsp->sessionId = smesessionId;
	rsp->length = sizeof(*rsp);
	rsp->aggrInfo.tspecIdx = aggrQosRsp->tspecIdx;
	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		if ((1 << i) & aggrQosRsp->tspecIdx) {
			if (QDF_IS_STATUS_SUCCESS(aggrQosRsp->status[i]))
				rsp->aggrInfo.aggrRsp[i].status =
					STATUS_SUCCESS;
			else
				rsp->aggrInfo.aggrRsp[i].status =
					STATUS_UNSPECIFIED_FAILURE;
			rsp->aggrInfo.aggrRsp[i].tspec = aggrQosRsp->tspec[i];
		}
	}
	lim_send_sme_aggr_qos_rsp(mac, rsp, smesessionId);
	return;
}

void lim_process_ft_aggr_qos_rsp(struct mac_context *mac,
				 struct scheduler_msg *limMsg)
{
	struct aggr_add_ts_param *pAggrQosRspMsg;
	struct add_ts_param addTsParam = { 0 };
	tpDphHashNode pSta = NULL;
	uint16_t assocId = 0;
	tSirMacAddr peerMacAddr;
	uint8_t rspReqd = 1;
	struct pe_session *pe_session = NULL;
	int i = 0;

	pe_debug(" Received AGGR_QOS_RSP from HAL");
	SET_LIM_PROCESS_DEFD_MESGS(mac, true);
	pAggrQosRspMsg = limMsg->bodyptr;
	if (!pAggrQosRspMsg) {
		pe_err("NULL pAggrQosRspMsg");
		return;
	}
	pe_session =
		pe_find_session_by_session_id(mac, pAggrQosRspMsg->sessionId);
	if (!pe_session) {
		pe_err("Cant find session entry");
		if (pAggrQosRspMsg) {
			qdf_mem_free(pAggrQosRspMsg);
		}
		return;
	}
	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("pe_session is not in STA mode");
		return;
	}
	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		if ((((1 << i) & pAggrQosRspMsg->tspecIdx)) &&
		    (pAggrQosRspMsg->status[i] != QDF_STATUS_SUCCESS)) {
			sir_copy_mac_addr(peerMacAddr, pe_session->bssId);
			addTsParam.pe_session_id = pAggrQosRspMsg->sessionId;
			addTsParam.tspec = pAggrQosRspMsg->tspec[i];
			addTsParam.tspec_idx = pAggrQosRspMsg->tspecIdx;
			lim_send_delts_req_action_frame(mac, peerMacAddr,
							rspReqd,
							&addTsParam.tspec.tsinfo,
							&addTsParam.tspec,
							pe_session);
			pSta =
				dph_lookup_hash_entry(mac, peerMacAddr,
						      &assocId,
						      &pe_session->
						      dph.dphHashTable);

			if (pSta) {
				lim_admit_control_delete_ts(mac, assocId,
							    &addTsParam.tspec.
							    tsinfo, NULL,
							    (uint8_t *) &
							    addTsParam.tspec_idx);
			}
		}
	}
	lim_ft_send_aggr_qos_rsp(mac, rspReqd, pAggrQosRspMsg,
				 pe_session->smeSessionId);
	if (pAggrQosRspMsg) {
		qdf_mem_free(pAggrQosRspMsg);
	}
	return;
}

QDF_STATUS lim_process_ft_aggr_qos_req(struct mac_context *mac,
				       uint32_t *msg_buf)
{
	struct scheduler_msg msg = {0};
	tSirAggrQosReq *aggrQosReq = (tSirAggrQosReq *) msg_buf;
	struct aggr_add_ts_param *pAggrAddTsParam;
	struct pe_session *pe_session = NULL;
	tpLimTspecInfo tspecInfo;
	uint8_t ac;
	tpDphHashNode pSta;
	uint16_t aid;
	uint8_t sessionId;
	int i;

	pAggrAddTsParam = qdf_mem_malloc(sizeof(*pAggrAddTsParam));
	if (!pAggrAddTsParam)
		return QDF_STATUS_E_NOMEM;

	pe_session = pe_find_session_by_bssid(mac, aggrQosReq->bssid.bytes,
					      &sessionId);

	if (!pe_session) {
		pe_err("psession Entry Null for sessionId: %d",
			       aggrQosReq->sessionId);
		qdf_mem_free(pAggrAddTsParam);
		return QDF_STATUS_E_FAILURE;
	}

	/* Nothing to be done if the session is not in STA mode */
	if (!LIM_IS_STA_ROLE(pe_session)) {
		pe_err("pe_session is not in STA mode");
		qdf_mem_free(pAggrAddTsParam);
		return QDF_STATUS_E_FAILURE;
	}

	pSta = dph_lookup_hash_entry(mac, aggrQosReq->bssid.bytes, &aid,
				     &pe_session->dph.dphHashTable);
	if (!pSta) {
		pe_err("Station context not found - ignoring AddTsRsp");
		qdf_mem_free(pAggrAddTsParam);
		return QDF_STATUS_E_FAILURE;
	}

	/* Fill in the sessionId specific to PE */
	pAggrAddTsParam->sessionId = sessionId;
	pAggrAddTsParam->tspecIdx = aggrQosReq->aggrInfo.tspecIdx;
	pAggrAddTsParam->vdev_id = pe_session->smeSessionId;

	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		if (aggrQosReq->aggrInfo.tspecIdx & (1 << i)) {
			struct mac_tspec_ie *pTspec =
				&aggrQosReq->aggrInfo.aggrAddTsInfo[i].tspec;
			/* Since AddTS response was successful, check for the PSB flag
			 * and directional flag inside the TS Info field.
			 * An AC is trigger enabled AC if the PSB subfield is set to 1
			 * in the uplink direction.
			 * An AC is delivery enabled AC if the PSB subfield is set to 1
			 * in the downlink direction.
			 * An AC is trigger and delivery enabled AC if the PSB subfield
			 * is set to 1 in the bi-direction field.
			 */
			if (pTspec->tsinfo.traffic.psb == 1) {
				lim_set_tspec_uapsd_mask_per_session(mac,
								     pe_session,
								     &pTspec->
								     tsinfo,
								     SET_UAPSD_MASK);
			} else {
				lim_set_tspec_uapsd_mask_per_session(mac,
								     pe_session,
								     &pTspec->
								     tsinfo,
								     CLEAR_UAPSD_MASK);
			}
			/*
			 * ADDTS success, so AC is now admitted.
			 * We shall now use the default
			 * EDCA parameters as advertised by AP and
			 * send the updated EDCA params
			 * to HAL.
			 */
			ac = upToAc(pTspec->tsinfo.traffic.userPrio);
			if (pTspec->tsinfo.traffic.direction ==
			    SIR_MAC_DIRECTION_UPLINK) {
				pe_session->
				gAcAdmitMask
				[SIR_MAC_DIRECTION_UPLINK] |=
					(1 << ac);
			} else if (pTspec->tsinfo.traffic.direction ==
				   SIR_MAC_DIRECTION_DNLINK) {
				pe_session->
				gAcAdmitMask
				[SIR_MAC_DIRECTION_DNLINK] |=
					(1 << ac);
			} else if (pTspec->tsinfo.traffic.direction ==
				   SIR_MAC_DIRECTION_BIDIR) {
				pe_session->
				gAcAdmitMask
				[SIR_MAC_DIRECTION_UPLINK] |=
					(1 << ac);
				pe_session->
					gAcAdmitMask
					[SIR_MAC_DIRECTION_DNLINK] |=
					(1 << ac);
			}
			lim_set_active_edca_params(mac,
						   pe_session->gLimEdcaParams,
						   pe_session);

				lim_send_edca_params(mac,
					     pe_session->gLimEdcaParamsActive,
					     pe_session->vdev_id, false);

			if (QDF_STATUS_SUCCESS !=
			    lim_tspec_add(mac, pSta->staAddr, pSta->assocId,
					  pTspec, 0, &tspecInfo)) {
				pe_err("Adding entry in lim Tspec Table failed");
				mac->lim.gLimAddtsSent = false;
				qdf_mem_free(pAggrAddTsParam);
				return QDF_STATUS_E_FAILURE;
			}

			pAggrAddTsParam->tspec[i] =
				aggrQosReq->aggrInfo.aggrAddTsInfo[i].tspec;
		}
	}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (!mac->mlme_cfg->lfr.lfr3_roaming_offload ||
	    (mac->mlme_cfg->lfr.lfr3_roaming_offload &&
	     !pe_session->is11Rconnection))
#endif
	{
	msg.type = WMA_AGGR_QOS_REQ;
	msg.bodyptr = pAggrAddTsParam;
	msg.bodyval = 0;

	/* We need to defer any incoming messages until we get a
	 * WMA_AGGR_QOS_RSP from HAL.
	 */
	SET_LIM_PROCESS_DEFD_MESGS(mac, false);
	MTRACE(mac_trace_msg_tx(mac, pe_session->peSessionId, msg.type));

	if (QDF_STATUS_SUCCESS != wma_post_ctrl_msg(mac, &msg)) {
			pe_warn("wma_post_ctrl_msg() failed");
			SET_LIM_PROCESS_DEFD_MESGS(mac, true);
			qdf_mem_free(pAggrAddTsParam);
			return QDF_STATUS_E_FAILURE;
		}
	}
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	else {
		/* Implies it is a LFR3.0 based 11r connection
		 * so donot send add ts request to firmware since it
		 * already has the RIC IEs */

		/* Send the Aggr QoS response to SME */
		lim_ft_send_aggr_qos_rsp(mac, true, pAggrAddTsParam,
					 pe_session->smeSessionId);
		if (pAggrAddTsParam) {
			qdf_mem_free(pAggrAddTsParam);
		}
	}
#endif

	return QDF_STATUS_SUCCESS;
}
