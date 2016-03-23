/*
 * Copyright (c) 2011-2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*
 *
 * This file limPropExtsUtils.cc contains the utility functions
 * to populate, parse proprietary extensions required to
 * support ANI feature set.
 *
 * Author:        Chandra Modumudi
 * Date:          11/27/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "aniGlobal.h"
#include "wniCfgSta.h"
#include "sirCommon.h"
#include "sirDebug.h"
#include "utilsApi.h"
#include "cfgApi.h"
#include "limApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limPropExtsUtils.h"
#include "limSerDesUtils.h"
#include "limTrace.h"
#ifdef WLAN_FEATURE_VOWIFI_11R
#include "limFTDefs.h"
#endif
#include "limSession.h"
#define LIM_GET_NOISE_MAX_TRY 5

#include "wma.h"
/**
 * limExtractApCapability()
 *
 *FUNCTION:
 * This function is called to extract AP's HCF/WME/WSM capability
 * from the IEs received from it in Beacon/Probe Response frames
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param   pMac      Pointer to Global MAC structure
 * @param   pIE       Pointer to starting IE in Beacon/Probe Response
 * @param   ieLen     Length of all IEs combined
 * @param   qosCap    Bits are set according to capabilities
 * @return  0 - If AP does not assert HCF capability & 1 - otherwise
 */
void
limExtractApCapability(tpAniSirGlobal pMac, tANI_U8 *pIE, tANI_U16 ieLen,
                       tANI_U8 *qosCap, tANI_U16 *propCap, tANI_U8 *uapsd,
                       tPowerdBm *localConstraint,
                       tpPESession psessionEntry
                       )
{
    tSirProbeRespBeacon *pBeaconStruct;
#if !defined WLAN_FEATURE_VOWIFI
    tANI_U32            localPowerConstraints = 0;
#endif
    tANI_U32 enableTxBF20MHz;

    pBeaconStruct = vos_mem_malloc(sizeof(tSirProbeRespBeacon));
    if ( NULL == pBeaconStruct )
    {
        limLog(pMac, LOGE, FL("Unable to allocate memory in limExtractApCapability") );
        return;
    }

    vos_mem_set( (tANI_U8 *) pBeaconStruct, sizeof(tSirProbeRespBeacon), 0);
    *qosCap = 0;
    *propCap = 0;
    *uapsd = 0;
    PELOG3(limLog( pMac, LOG3,
        FL("In limExtractApCapability: The IE's being received are:"));
    sirDumpBuf( pMac, SIR_LIM_MODULE_ID, LOG3, pIE, ieLen );)
    if (sirParseBeaconIE(pMac, pBeaconStruct, pIE, (tANI_U32)ieLen) == eSIR_SUCCESS)
    {
        if (pBeaconStruct->wmeInfoPresent || pBeaconStruct->wmeEdcaPresent
            || pBeaconStruct->HTCaps.present)
            LIM_BSS_CAPS_SET(WME, *qosCap);
        if (LIM_BSS_CAPS_GET(WME, *qosCap) && pBeaconStruct->wsmCapablePresent)
            LIM_BSS_CAPS_SET(WSM, *qosCap);
        if (pBeaconStruct->propIEinfo.capabilityPresent)
            *propCap = pBeaconStruct->propIEinfo.capability;
        if (pBeaconStruct->HTCaps.present)
            pMac->lim.htCapabilityPresentInBeacon = 1;
        else
            pMac->lim.htCapabilityPresentInBeacon = 0;

#ifdef WLAN_FEATURE_11AC
        limLog(pMac, LOG1,
            FL("Beacon : VHTCaps.present: %d SU Beamformer: %d, IS_BSS_VHT_CAPABLE: %d"),
            pBeaconStruct->VHTCaps.present,
            pBeaconStruct->VHTCaps.suBeamFormerCap,
            IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps));

        if (IS_BSS_VHT_CAPABLE(pBeaconStruct->VHTCaps) && pBeaconStruct->VHTOperation.present)
        {
            /* If VHT is supported min 80 MHz support is must */
            uint32_t fw_vht_ch_wd = wma_get_vht_ch_width();
            uint32_t vht_ch_wd;

            psessionEntry->vhtCapabilityPresentInBeacon = 1;
            vht_ch_wd = VOS_MIN(fw_vht_ch_wd,
                            pBeaconStruct->VHTOperation.chanWidth);
            /*
             * First block covers 2 cases:
             * 1) AP and STA both have same vht capab
             * 2) AP is 160 (80+80), we are 160 only
             */
            if (vht_ch_wd == pBeaconStruct->VHTOperation.chanWidth ||
                vht_ch_wd >= WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ ) {
                psessionEntry->apCenterChan =
                    pBeaconStruct->VHTOperation.chanCenterFreqSeg1;
            } else {
                /* This is the case when AP was 160 but we were 80 only */
                psessionEntry->apCenterChan =
                    lim_get_80Mhz_center_channel(pBeaconStruct->channelNumber);
            }
            psessionEntry->apChanWidth = vht_ch_wd;
            psessionEntry->vhtTxChannelWidthSet = vht_ch_wd;

            if (pBeaconStruct->Vendor1IEPresent &&
                pBeaconStruct->Vendor2IEPresent &&
                pBeaconStruct->Vendor3IEPresent)
            {
                if (((pBeaconStruct->VHTCaps.txMCSMap & VHT_MCS_3x3_MASK) ==
                               VHT_MCS_3x3_MASK) &&
                   ((pBeaconStruct->VHTCaps.txMCSMap & VHT_MCS_2x2_MASK) !=
                               VHT_MCS_2x2_MASK))
                {
                    psessionEntry->txBFIniFeatureEnabled = 0;
                    if (cfgSetInt(pMac, WNI_CFG_VHT_SU_BEAMFORMEE_CAP, 0)
                                                             != eSIR_SUCCESS)
                    {
                        limLog(pMac, LOGP, FL("could not set "
                                  "WNI_CFG_VHT_SU_BEAMFORMEE_CAP at CFG"));
                    }
                }
            }
            if (!psessionEntry->htSupportedChannelWidthSet) {
                if (HAL_STATUS_SUCCESS(ccmCfgGetInt(pMac,
                                        WNI_CFG_VHT_ENABLE_TXBF_20MHZ,
                                        &enableTxBF20MHz))) {
                    if (VOS_FALSE == enableTxBF20MHz) {
                        psessionEntry->txBFIniFeatureEnabled = 0;
                        if (cfgSetInt(pMac, WNI_CFG_VHT_SU_BEAMFORMEE_CAP, 0)
                                                             != eSIR_SUCCESS) {
                            limLog(pMac, LOGP, FL("could not set "
                                  "WNI_CFG_VHT_SU_BEAMFORMEE_CAP at CFG"));
                        }
                    }
                }
            }
        }
        else
        {
            psessionEntry->vhtCapabilityPresentInBeacon = 0;
        }
#endif
        // Extract the UAPSD flag from WMM Parameter element
        if (pBeaconStruct->wmeEdcaPresent)
            *uapsd = pBeaconStruct->edcaParams.qosInfo.uapsd;
#if defined FEATURE_WLAN_ESE
        /* If there is Power Constraint Element specifically,
         * adapt to it. Hence there is else condition check
         * for this if statement.
         */
        if ( pBeaconStruct->eseTxPwr.present)
        {
            *localConstraint = pBeaconStruct->eseTxPwr.power_limit;
        }
        psessionEntry->is_ese_version_ie_present =
                              pBeaconStruct->is_ese_ver_ie_present;
#endif
        if (pBeaconStruct->powerConstraintPresent)
        {
#if defined WLAN_FEATURE_VOWIFI
           *localConstraint -= pBeaconStruct->localPowerConstraint.localPowerConstraints;
#else
           localPowerConstraints = (tANI_U32)pBeaconStruct->localPowerConstraint.localPowerConstraints;
#endif
        }
#if !defined WLAN_FEATURE_VOWIFI
        if (cfgSetInt(pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT, localPowerConstraints) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Could not update local power constraint to cfg."));
        }
#endif
        psessionEntry->countryInfoPresent = FALSE;
        /* Initializing before first use */
        if (pBeaconStruct->countryInfoPresent)
           psessionEntry->countryInfoPresent = TRUE;
    }
    /* Check if Extended caps are present in probe resp or not */
    if (pBeaconStruct->ExtCap.present)
        psessionEntry->is_ext_caps_present = true;
    vos_mem_free(pBeaconStruct);
    return;
} /****** end limExtractApCapability() ******/

/**
 * limGetHTCBState
 *
 *FUNCTION:
 * This routing provides the translation of Airgo Enum to HT enum for determining
 * secondary channel offset.
 * Airgo Enum is required for backward compatibility purposes.
 *
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return The corresponding HT enumeration
 */
ePhyChanBondState  limGetHTCBState(ePhyChanBondState aniCBMode)
{
    switch ( aniCBMode )
    {
#ifdef WLAN_FEATURE_11AC
        case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
#endif
        case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
        return PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
#ifdef WLAN_FEATURE_11AC
        case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
        case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
#endif
        case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
        return PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
#ifdef WLAN_FEATURE_11AC
        case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
           return PHY_SINGLE_CHANNEL_CENTERED;
#endif
        default :
           return PHY_SINGLE_CHANNEL_CENTERED;
     }
}

 /*
 * limGetStaPeerType
 *
 *FUNCTION:
 * This API returns STA peer type
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pStaDs - Pointer to the tpDphHashNode of the STA
 *         under consideration
 * @return tStaRateMode
 */
tStaRateMode limGetStaPeerType( tpAniSirGlobal pMac,
    tpDphHashNode pStaDs,
    tpPESession   psessionEntry)
{
  tStaRateMode staPeerType = eSTA_11b;
#ifdef WLAN_FEATURE_11AC
  if(pStaDs->mlmStaContext.vhtCapability)
      staPeerType = eSTA_11ac;
#endif
  else if(pStaDs->mlmStaContext.htCapability)
        staPeerType = eSTA_11n;
  else if(pStaDs->erpEnabled)
        staPeerType = eSTA_11bg;
  else if(psessionEntry->limRFBand == SIR_BAND_5_GHZ)
        staPeerType = eSTA_11a;
  return staPeerType;
}
