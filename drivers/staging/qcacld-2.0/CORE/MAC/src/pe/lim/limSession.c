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

/**=========================================================================

  \file  limSession.c

  \brief implementation for lim Session related APIs

  \author Sunit Bhatia

  ========================================================================*/


/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "aniGlobal.h"
#include "limDebug.h"
#ifdef WLAN_FEATURE_VOWIFI_11R
#include "limFTDefs.h"
#include "limFT.h"
#endif
#include "limSession.h"
#include "limUtils.h"
#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
#include "eseApi.h"
#endif

#include "pmmApi.h"
#include "schApi.h"
#include "limSendMessages.h"

/*--------------------------------------------------------------------------

  \brief peInitBeaconParams() - Initialize the beaconParams structure


  \param tpPESession          - pointer to the session context or NULL if session can not be created.
  \return void
  \sa

  --------------------------------------------------------------------------*/

void peInitBeaconParams(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    psessionEntry->beaconParams.beaconInterval = 0;
    psessionEntry->beaconParams.fShortPreamble = 0;
    psessionEntry->beaconParams.llaCoexist = 0;
    psessionEntry->beaconParams.llbCoexist = 0;
    psessionEntry->beaconParams.llgCoexist = 0;
    psessionEntry->beaconParams.ht20Coexist = 0;
    psessionEntry->beaconParams.llnNonGFCoexist = 0;
    psessionEntry->beaconParams.fRIFSMode = 0;
    psessionEntry->beaconParams.fLsigTXOPProtectionFullSupport = 0;
    psessionEntry->beaconParams.gHTObssMode = 0;

    // Number of legacy STAs associated
    vos_mem_set((void*)&psessionEntry->gLim11bParams, sizeof(tLimProtStaParams), 0);
    vos_mem_set((void*)&psessionEntry->gLim11aParams, sizeof(tLimProtStaParams), 0);
    vos_mem_set((void*)&psessionEntry->gLim11gParams, sizeof(tLimProtStaParams), 0);
    vos_mem_set((void*)&psessionEntry->gLimNonGfParams, sizeof(tLimProtStaParams), 0);
    vos_mem_set((void*)&psessionEntry->gLimHt20Params, sizeof(tLimProtStaParams), 0);
    vos_mem_set((void*)&psessionEntry->gLimLsigTxopParams, sizeof(tLimProtStaParams), 0);
    vos_mem_set((void*)&psessionEntry->gLimOlbcParams, sizeof(tLimProtStaParams), 0);
}

/**
 * pe_reset_protection_callback() - resets protection structs so that when an AP
 * causing use of protection goes away, corresponding protection bit can be
 * reset
 * @ptr:        pointer to pSessionEntry
 *
 * This function resets protection structs so that when an AP causing use of
 * protection goes away, corresponding protection bit can be reset. This allowes
 * protection bits to be reset once legacy overlapping APs are gone.
 *
 * Return: void
 */
void pe_reset_protection_callback(void *ptr)
{
    tpPESession pe_session_entry = (tpPESession)ptr;
    tpAniSirGlobal mac_ctx = (tpAniSirGlobal)pe_session_entry->mac_ctx;
    int8_t i = 0;
    tUpdateBeaconParams beacon_params;
    tANI_U16 current_protection_state = 0;
    tpDphHashNode station_hash_node = NULL;
    tSirMacHTOperatingMode old_op_mode;
    bool bcn_prms_changed = false;

    if (pe_session_entry->valid == false) {
        VOS_TRACE(VOS_MODULE_ID_PE,
                  VOS_TRACE_LEVEL_ERROR,
                  FL("session already deleted. exiting timer callback"));
        return;
    }

    current_protection_state |=
               pe_session_entry->gLimOverlap11gParams.protectionEnabled        |
               pe_session_entry->gLimOverlap11aParams.protectionEnabled   << 1 |
               pe_session_entry->gLimOverlapHt20Params.protectionEnabled  << 2 |
               pe_session_entry->gLimOverlapNonGfParams.protectionEnabled << 3 |
               pe_session_entry->gLimOlbcParams.protectionEnabled         << 4 ;

    VOS_TRACE(VOS_MODULE_ID_PE,
              VOS_TRACE_LEVEL_INFO,
              FL("old protection state: 0x%04X, new protection state: 0x%04X"),
              pe_session_entry->old_protection_state,
              current_protection_state);

    vos_mem_zero(&pe_session_entry->gLimOverlap11gParams,
                 sizeof(pe_session_entry->gLimOverlap11gParams));
    vos_mem_zero(&pe_session_entry->gLimOverlap11aParams,
                 sizeof(pe_session_entry->gLimOverlap11aParams));
    vos_mem_zero(&pe_session_entry->gLimOverlapHt20Params,
                 sizeof(pe_session_entry->gLimOverlapHt20Params));
    vos_mem_zero(&pe_session_entry->gLimOverlapNonGfParams,
                 sizeof(pe_session_entry->gLimOverlapNonGfParams));

    vos_mem_zero(&pe_session_entry->gLimOlbcParams,
                 sizeof(pe_session_entry->gLimOlbcParams));

    vos_mem_zero(&pe_session_entry->beaconParams,
                 sizeof(pe_session_entry->beaconParams));

    vos_mem_zero(&mac_ctx->lim.gLimOverlap11gParams,
                 sizeof(mac_ctx->lim.gLimOverlap11gParams));
    vos_mem_zero(&mac_ctx->lim.gLimOverlap11aParams,
                 sizeof(mac_ctx->lim.gLimOverlap11aParams));
    vos_mem_zero(&mac_ctx->lim.gLimOverlapHt20Params,
                 sizeof(mac_ctx->lim.gLimOverlapHt20Params));
    vos_mem_zero(&mac_ctx->lim.gLimOverlapNonGfParams,
                 sizeof(mac_ctx->lim.gLimOverlapNonGfParams));

    old_op_mode = pe_session_entry->htOperMode;
    pe_session_entry->htOperMode = eSIR_HT_OP_MODE_PURE;
    mac_ctx->lim.gHTOperMode = eSIR_HT_OP_MODE_PURE;

    vos_mem_zero(&beacon_params, sizeof(tUpdateBeaconParams));
    /* index 0, is self node, peers start from 1 */
    for(i = 1 ; i <= mac_ctx->lim.gLimAssocStaLimit ; i++)
    {
        station_hash_node = dphGetHashEntry(mac_ctx, i,
                              &pe_session_entry->dph.dphHashTable);
        if (NULL == station_hash_node)
            continue;
        limDecideApProtection(mac_ctx, station_hash_node->staAddr,
                              &beacon_params, pe_session_entry);
    }

    if (pe_session_entry->htOperMode != old_op_mode)
        bcn_prms_changed = true;

    if ((current_protection_state != pe_session_entry->old_protection_state) &&
        (VOS_FALSE == mac_ctx->sap.SapDfsInfo.is_dfs_cac_timer_running)) {
        VOS_TRACE(VOS_MODULE_ID_PE,
                  VOS_TRACE_LEVEL_ERROR,
                  FL("protection changed, update beacon template"));
        /* update beacon fix params and send update to FW */
        vos_mem_zero(&beacon_params, sizeof(tUpdateBeaconParams));
        beacon_params.bssIdx = pe_session_entry->bssIdx;
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
                pe_session_entry->beaconParams.fLsigTXOPProtectionFullSupport;
        beacon_params.fRIFSMode =
                    pe_session_entry->beaconParams.fRIFSMode;
        beacon_params.smeSessionId =
                    pe_session_entry->smeSessionId;
        beacon_params.paramChangeBitmap |= PARAM_llBCOEXIST_CHANGED;
        bcn_prms_changed = true;
    }

    if (bcn_prms_changed) {
        schSetFixedBeaconFields(mac_ctx, pe_session_entry);
        limSendBeaconParams(mac_ctx, &beacon_params, pe_session_entry);
    }

    pe_session_entry->old_protection_state = current_protection_state;
    if (VOS_STATUS_SUCCESS != vos_timer_start(
                             &pe_session_entry->protection_fields_reset_timer,
                             SCH_PROTECTION_RESET_TIME)) {
        VOS_TRACE(VOS_MODULE_ID_PE,
                  VOS_TRACE_LEVEL_ERROR,
                  FL("cannot create or start protectionFieldsResetTimer"));
    }
}

/*--------------------------------------------------------------------------

  \brief peCreateSession() - creates a new PE session given the BSSID

  This function returns the session context and the session ID if the session
  corresponding to the passed BSSID is found in the PE session table.

  \param pMac          - pointer to global adapter context
  \param bssid         - BSSID of the new session
  \param sessionId     - session ID is returned here, if session is created.
  \param bssType       - station or a
  \return tpPESession  - pointer to the session context or NULL if session
                         can not be created.

  \sa

  --------------------------------------------------------------------------*/
tpPESession peCreateSession(tpAniSirGlobal pMac,
                            tANI_U8 *bssid,
                            tANI_U8* sessionId,
                            tANI_U16 numSta,
                            tSirBssType bssType)
{
    VOS_STATUS status;
    tANI_U8 i;
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        /* Find first free room in session table */
        if(pMac->lim.gpSession[i].valid == FALSE)
        {
            vos_mem_set((void*)&pMac->lim.gpSession[i], sizeof(tPESession), 0);

            //Allocate space for Station Table for this session.
            pMac->lim.gpSession[i].dph.dphHashTable.pHashTable = vos_mem_malloc(
                                                  sizeof(tpDphHashNode)* (numSta + 1));
            if ( NULL == pMac->lim.gpSession[i].dph.dphHashTable.pHashTable )
            {
                limLog(pMac, LOGE, FL("memory allocate failed!"));
                return NULL;
            }
            pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray = vos_mem_malloc(
                                                       sizeof(tDphHashNode) * (numSta + 1));
            if ( NULL == pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray )
            {
                limLog(pMac, LOGE, FL("memory allocate failed!"));
                vos_mem_free(pMac->lim.gpSession[i].dph.dphHashTable.pHashTable);
                pMac->lim.gpSession[i].dph.dphHashTable.pHashTable = NULL;
                return NULL;
            }

            pMac->lim.gpSession[i].dph.dphHashTable.size = numSta + 1;

            dphHashTableClassInit(pMac,
                           &pMac->lim.gpSession[i].dph.dphHashTable);

            pMac->lim.gpSession[i].gpLimPeerIdxpool = vos_mem_malloc(sizeof(
                                *pMac->lim.gpSession[i].gpLimPeerIdxpool) * (numSta+1));
            if ( NULL == pMac->lim.gpSession[i].gpLimPeerIdxpool )
            {
                PELOGE(limLog(pMac, LOGE, FL("memory allocate failed!"));)
                vos_mem_free(pMac->lim.gpSession[i].dph.dphHashTable.pHashTable);
                vos_mem_free(pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray);
                pMac->lim.gpSession[i].dph.dphHashTable.pHashTable = NULL;
                pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray = NULL;
                return NULL;
            }
            vos_mem_set(pMac->lim.gpSession[i].gpLimPeerIdxpool,
                  sizeof(*pMac->lim.gpSession[i].gpLimPeerIdxpool) * (numSta+1), 0);
            pMac->lim.gpSession[i].freePeerIdxHead = 0;
            pMac->lim.gpSession[i].freePeerIdxTail = 0;
            pMac->lim.gpSession[i].gLimNumOfCurrentSTAs = 0;

            /* Copy the BSSID to the session table */
            sirCopyMacAddr(pMac->lim.gpSession[i].bssId, bssid);
            if (bssType == eSIR_MONITOR_MODE)
                sirCopyMacAddr(pMac->lim.gpSession[i].selfMacAddr, bssid);
            pMac->lim.gpSession[i].valid = TRUE;

            /* Initialize the SME and MLM states to IDLE */
            pMac->lim.gpSession[i].limMlmState = eLIM_MLM_IDLE_STATE;
            pMac->lim.gpSession[i].limSmeState = eLIM_SME_IDLE_STATE;
            pMac->lim.gpSession[i].limCurrentAuthType = eSIR_OPEN_SYSTEM;
            peInitBeaconParams(pMac, &pMac->lim.gpSession[i]);
#ifdef WLAN_FEATURE_VOWIFI_11R
            pMac->lim.gpSession[i].is11Rconnection = FALSE;
#endif

#ifdef FEATURE_WLAN_ESE
            pMac->lim.gpSession[i].isESEconnection = FALSE;
#endif

#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_ESE || defined(FEATURE_WLAN_LFR)
            pMac->lim.gpSession[i].isFastTransitionEnabled = FALSE;
#endif
#ifdef FEATURE_WLAN_LFR
            pMac->lim.gpSession[i].isFastRoamIniFeatureEnabled = FALSE;
#endif
            *sessionId = i;

            pMac->lim.gpSession[i].gLimPhyMode = WNI_CFG_PHY_MODE_11G; //TODO :Check with the team what should be default mode
            /* Initialize CB mode variables when session is created */
            pMac->lim.gpSession[i].htSupportedChannelWidthSet = 0;
            pMac->lim.gpSession[i].htRecommendedTxWidthSet = 0;
            pMac->lim.gpSession[i].htSecondaryChannelOffset = 0;
#ifdef FEATURE_WLAN_TDLS
            vos_mem_set(pMac->lim.gpSession[i].peerAIDBitmap,
                  sizeof(pMac->lim.gpSession[i].peerAIDBitmap), 0);
            pMac->lim.gpSession[i].tdls_prohibited = false;
            pMac->lim.gpSession[i].tdls_chan_swit_prohibited = false;
#endif
            pMac->lim.gpSession[i].fWaitForProbeRsp = 0;
            pMac->lim.gpSession[i].fIgnoreCapsChange = 0;

            VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_DEBUG,
              "Create a new PE session (%d) with BSSID: "
               MAC_ADDRESS_STR " Max No. of STA %d",
               pMac->lim.gpSession[i].peSessionId,
               MAC_ADDR_ARRAY(bssid), numSta);
            pMac->lim.gpSession[i].roaming_in_progress = false;

            /* Initialize PMM Ps Offload Module */
            if(pMac->psOffloadEnabled)
            {
                if(pmmPsOffloadOpen(pMac, &pMac->lim.gpSession[i])
                   != eHAL_STATUS_SUCCESS)
                {
                    limLog(pMac, LOGE,
                       FL("Failed to open ps offload for pe session %x"), i);
                }
            }

            if (eSIR_INFRA_AP_MODE == bssType ||
                    eSIR_IBSS_MODE == bssType ||
                    eSIR_BTAMP_AP_MODE == bssType)
            {
                 pMac->lim.gpSession[i].pSchProbeRspTemplate =
                                 vos_mem_malloc(SCH_MAX_PROBE_RESP_SIZE);
                 pMac->lim.gpSession[i].pSchBeaconFrameBegin =
                                 vos_mem_malloc(SCH_MAX_BEACON_SIZE);
                 pMac->lim.gpSession[i].pSchBeaconFrameEnd =
                                 vos_mem_malloc(SCH_MAX_BEACON_SIZE);
                 if ( (NULL == pMac->lim.gpSession[i].pSchProbeRspTemplate)
                       || (NULL == pMac->lim.gpSession[i].pSchBeaconFrameBegin)
                       || (NULL == pMac->lim.gpSession[i].pSchBeaconFrameEnd) )
                 {
                     PELOGE(limLog(pMac, LOGE, FL("memory allocate failed!"));)
                     vos_mem_free(pMac->lim.gpSession[i].dph.dphHashTable.pHashTable);
                     vos_mem_free(pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray);
                     vos_mem_free(pMac->lim.gpSession[i].gpLimPeerIdxpool);
                     vos_mem_free(pMac->lim.gpSession[i].pSchProbeRspTemplate);
                     vos_mem_free(pMac->lim.gpSession[i].pSchBeaconFrameBegin);
                     vos_mem_free(pMac->lim.gpSession[i].pSchBeaconFrameEnd);

                     pMac->lim.gpSession[i].dph.dphHashTable.pHashTable = NULL;
                     pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray = NULL;
                     pMac->lim.gpSession[i].gpLimPeerIdxpool = NULL;
                     pMac->lim.gpSession[i].pSchProbeRspTemplate = NULL;
                     pMac->lim.gpSession[i].pSchBeaconFrameBegin = NULL;
                     pMac->lim.gpSession[i].pSchBeaconFrameEnd = NULL;
                     return NULL;
                 }
            }

#if defined WLAN_FEATURE_VOWIFI_11R
            if (eSIR_INFRASTRUCTURE_MODE == bssType) {
               limFTOpen(pMac, &pMac->lim.gpSession[i]);
            }
#endif
            if (eSIR_MONITOR_MODE == bssType)
               limFTOpen(pMac, &pMac->lim.gpSession[i]);

            if (eSIR_INFRA_AP_MODE == bssType) {
                pMac->lim.gpSession[i].old_protection_state = 0;
                pMac->lim.gpSession[i].mac_ctx = (void *)pMac;
                status = vos_timer_init(
                         &pMac->lim.gpSession[i].protection_fields_reset_timer,
                         VOS_TIMER_TYPE_SW, pe_reset_protection_callback,
                         (void *)&pMac->lim.gpSession[i]);
                if (status == VOS_STATUS_SUCCESS) {
                    status = vos_timer_start(
                          &pMac->lim.gpSession[i].protection_fields_reset_timer,
                          SCH_PROTECTION_RESET_TIME);
                }
                if (status != VOS_STATUS_SUCCESS) {
                    VOS_TRACE(VOS_MODULE_ID_PE,
                              VOS_TRACE_LEVEL_ERROR,
                              FL("cannot create or start protectionFieldsResetTimer"));
                }
            }

            return(&pMac->lim.gpSession[i]);
        }
    }
    limLog(pMac, LOGE,
            FL("Session can not be created.. Reached Max permitted sessions"));
    return NULL;
}

/*--------------------------------------------------------------------------
  \brief peFindSessionByBssid() - looks up the PE session given the BSSID.

  This function returns the session context and the session ID if the session
  corresponding to the given BSSID is found in the PE session table.

  \param pMac                   - pointer to global adapter context
  \param bssid                   - BSSID of the session
  \param sessionId             -session ID is returned here, if session is found.

  \return tpPESession          - pointer to the session context or NULL if session is not found.

  \sa
  --------------------------------------------------------------------------*/
tpPESession peFindSessionByBssid(tpAniSirGlobal pMac,  tANI_U8*  bssid,    tANI_U8* sessionId)
{
    tANI_U8 i;

    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        /* If BSSID matches return corresponding tables address*/
        if( (pMac->lim.gpSession[i].valid) && (sirCompareMacAddr(pMac->lim.gpSession[i].bssId, bssid)))
        {
            *sessionId = i;
            return(&pMac->lim.gpSession[i]);
        }
    }

    limLog(pMac, LOG4, FL("Session lookup fails for BSSID:"));
    limPrintMacAddr(pMac, bssid, LOG4);
    return(NULL);

}


/*--------------------------------------------------------------------------
  \brief peFindSessionByBssIdx() - looks up the PE session given the bssIdx.

  This function returns the session context  if the session
  corresponding to the given bssIdx is found in the PE session table.
  \param pMac                   - pointer to global adapter context
  \param bssIdx                   - bss index of the session
  \return tpPESession          - pointer to the session context or NULL if session is not found.
  \sa
  --------------------------------------------------------------------------*/
tpPESession peFindSessionByBssIdx(tpAniSirGlobal pMac,  tANI_U8 bssIdx)
{
    tANI_U8 i;
    for (i = 0; i < pMac->lim.maxBssId; i++)
    {
        /* If BSSID matches return corresponding tables address*/
        if ( (pMac->lim.gpSession[i].valid) && (pMac->lim.gpSession[i].bssIdx == bssIdx))
        {
            return &pMac->lim.gpSession[i];
        }
    }
    limLog(pMac, LOG4, FL("Session lookup fails for bssIdx: %d"), bssIdx);
    return NULL;
}

/**
 * pe_find_session_by_sme_session_id() - looks up the PE session for given sme
 * session id
 * @mac_ctx:          pointer to global adapter context
 * @sme_session_id:   sme session id
 *
 * looks up the PE session for given sme session id
 *
 * Return: pe session entry for given sme session if found else NULL
 */
tpPESession pe_find_session_by_sme_session_id(tpAniSirGlobal mac_ctx,
					tANI_U8 sme_session_id)
{
	uint8_t i;
	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		if ( (mac_ctx->lim.gpSession[i].valid) &&
		    (mac_ctx->lim.gpSession[i].smeSessionId ==
			sme_session_id) ) {
			return &mac_ctx->lim.gpSession[i];
		}
	}
	limLog(mac_ctx, LOG4,
	       FL("Session lookup fails for smeSessionID: %d"),
	       sme_session_id);
	return NULL;
}

/*--------------------------------------------------------------------------
  \brief peFindSessionBySessionId() - looks up the PE session given the session ID.

  This function returns the session context  if the session
  corresponding to the given session ID is found in the PE session table.

  \param pMac                   - pointer to global adapter context
  \param sessionId             -session ID for which session context needs to be looked up.

  \return tpPESession          - pointer to the session context or NULL if session is not found.

  \sa
  --------------------------------------------------------------------------*/
 tpPESession peFindSessionBySessionId(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    if(sessionId >=  pMac->lim.maxBssId)
    {
        limLog(pMac, LOGE, FL("Invalid sessionId: %d"), sessionId);
        return(NULL);
    }
    if((pMac->lim.gpSession[sessionId].valid == TRUE))
    {
        return(&pMac->lim.gpSession[sessionId]);
    }
    return(NULL);

}


/*--------------------------------------------------------------------------
  \brief peFindSessionByStaId() - looks up the PE session given staid.

  This function returns the session context and the session ID if the session
  corresponding to the given StaId is found in the PE session table.

  \param pMac                   - pointer to global adapter context
  \param staid                   - StaId of the session
  \param sessionId             -session ID is returned here, if session is found.

  \return tpPESession          - pointer to the session context or NULL if session is not found.

  \sa
  --------------------------------------------------------------------------*/
tpPESession peFindSessionByStaId(tpAniSirGlobal pMac,  tANI_U8  staid,    tANI_U8* sessionId)
{
    tANI_U8 i, j;

    for(i =0; i < pMac->lim.maxBssId; i++)
    {
       if(pMac->lim.gpSession[i].valid)
       {
          for(j = 0; j < pMac->lim.gpSession[i].dph.dphHashTable.size; j++)
          {
             if((pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray[j].valid) &&
                 (pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray[j].added) &&
                (staid == pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray[j].staIndex))
             {
                *sessionId = i;
                return(&pMac->lim.gpSession[i]);
             }
          }
       }
    }

    limLog(pMac, LOG4, FL("Session lookup fails for StaId: %d"), staid);
    return(NULL);
}



/*--------------------------------------------------------------------------
  \brief peDeleteSession() - deletes the PE session given the session ID.


  \param pMac                   - pointer to global adapter context
  \param sessionId             -session ID of the session which needs to be deleted.

  \sa
  --------------------------------------------------------------------------*/
void peDeleteSession(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    tANI_U16 i = 0;
    tANI_U16 n;
    TX_TIMER *timer_ptr;

    VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_DEBUG,
          "Trying to delete PE session %d Opmode %d BssIdx %d"
          " BSSID: " MAC_ADDRESS_STR, psessionEntry->peSessionId,
           psessionEntry->operMode, psessionEntry->bssIdx,
           MAC_ADDR_ARRAY(psessionEntry->bssId));

    for (n = 0; n < (pMac->lim.maxStation + 1); n++)
    {
        timer_ptr = &pMac->lim.limTimers.gpLimCnfWaitTimer[n];

        if(psessionEntry->peSessionId == timer_ptr->sessionId)
        {
            if(VOS_TRUE == tx_timer_running(timer_ptr))
            {
                tx_timer_deactivate(timer_ptr);
            }
        }
    }

    if (LIM_IS_AP_ROLE(psessionEntry)) {
       vos_timer_stop(&psessionEntry->protection_fields_reset_timer);
       vos_timer_destroy(&psessionEntry->protection_fields_reset_timer);
    }

#if defined (WLAN_FEATURE_VOWIFI_11R)
    /* Delete FT related information */
    limFTCleanup(pMac, psessionEntry);
#endif

    if (psessionEntry->pLimStartBssReq != NULL)
    {
        vos_mem_free( psessionEntry->pLimStartBssReq );
        psessionEntry->pLimStartBssReq = NULL;
    }

    if (psessionEntry->pLimJoinReq != NULL)
    {
        vos_mem_free( psessionEntry->pLimJoinReq );
        psessionEntry->pLimJoinReq = NULL;
    }

    if (psessionEntry->pLimReAssocReq != NULL)
    {
        vos_mem_free( psessionEntry->pLimReAssocReq );
        psessionEntry->pLimReAssocReq = NULL;
    }

    if (psessionEntry->pLimMlmJoinReq != NULL)
    {
        vos_mem_free( psessionEntry->pLimMlmJoinReq );
        psessionEntry->pLimMlmJoinReq = NULL;
    }

    if (psessionEntry->dph.dphHashTable.pHashTable != NULL)
    {
        vos_mem_free(psessionEntry->dph.dphHashTable.pHashTable);
        psessionEntry->dph.dphHashTable.pHashTable = NULL;
    }

    if (psessionEntry->dph.dphHashTable.pDphNodeArray != NULL)
    {
        vos_mem_free(psessionEntry->dph.dphHashTable.pDphNodeArray);
        psessionEntry->dph.dphHashTable.pDphNodeArray = NULL;
    }

    if (psessionEntry->gpLimPeerIdxpool != NULL)
    {
        vos_mem_free(psessionEntry->gpLimPeerIdxpool);
        psessionEntry->gpLimPeerIdxpool = NULL;
    }

    if (psessionEntry->beacon != NULL)
    {
        vos_mem_free( psessionEntry->beacon);
        psessionEntry->beacon = NULL;
        psessionEntry->bcnLen = 0;
    }

    if (psessionEntry->assocReq != NULL)
    {
        vos_mem_free( psessionEntry->assocReq);
        psessionEntry->assocReq = NULL;
        psessionEntry->assocReqLen = 0;
    }

    if (psessionEntry->assocRsp != NULL)
    {
        vos_mem_free( psessionEntry->assocRsp);
        psessionEntry->assocRsp = NULL;
        psessionEntry->assocRspLen = 0;
    }


    if (psessionEntry->parsedAssocReq != NULL)
    {
        /* Clean up the individual allocation first */
        for (i=0; i < psessionEntry->dph.dphHashTable.size; i++)
        {
            if ( psessionEntry->parsedAssocReq[i] != NULL )
            {
                if( ((tpSirAssocReq)(psessionEntry->parsedAssocReq[i]))->assocReqFrame )
                {
                   vos_mem_free(((tpSirAssocReq)
                                (psessionEntry->parsedAssocReq[i]))->assocReqFrame);
                   ((tpSirAssocReq)(psessionEntry->parsedAssocReq[i]))->assocReqFrame = NULL;
                   ((tpSirAssocReq)(psessionEntry->parsedAssocReq[i]))->assocReqFrameLength = 0;
                }
                vos_mem_free(psessionEntry->parsedAssocReq[i]);
                psessionEntry->parsedAssocReq[i] = NULL;
            }
        }
        /* Clean up the whole block */
        vos_mem_free(psessionEntry->parsedAssocReq);
        psessionEntry->parsedAssocReq = NULL;
    }
    if (NULL != psessionEntry->limAssocResponseData)
    {
        vos_mem_free( psessionEntry->limAssocResponseData);
        psessionEntry->limAssocResponseData = NULL;
    }

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_ESE) || defined(FEATURE_WLAN_LFR)
    if (NULL != psessionEntry->pLimMlmReassocRetryReq)
    {
        vos_mem_free( psessionEntry->pLimMlmReassocRetryReq);
        psessionEntry->pLimMlmReassocRetryReq = NULL;
    }
#endif

    if (NULL != psessionEntry->pLimMlmReassocReq)
    {
        vos_mem_free( psessionEntry->pLimMlmReassocReq);
        psessionEntry->pLimMlmReassocReq = NULL;
    }

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
    limCleanupEseCtxt(pMac, psessionEntry);
#endif

    /* Initialize PMM Ps Offload Module */
    if(pMac->psOffloadEnabled)
    {
        if(pmmPsOffloadClose(pMac, psessionEntry)
           != eHAL_STATUS_SUCCESS)
        {
            limLog(pMac, LOGW,
                   FL("Failed to close ps offload for pe session %x"),
                   psessionEntry->peSessionId);
        }
    }

    if (NULL != psessionEntry->pSchProbeRspTemplate)
    {
        vos_mem_free(psessionEntry->pSchProbeRspTemplate);
        psessionEntry->pSchProbeRspTemplate = NULL;
    }

    if (NULL != psessionEntry->pSchBeaconFrameBegin)
    {
        vos_mem_free(psessionEntry->pSchBeaconFrameBegin);
        psessionEntry->pSchBeaconFrameBegin = NULL;
    }

    if (NULL != psessionEntry->pSchBeaconFrameEnd)
    {
        vos_mem_free(psessionEntry->pSchBeaconFrameEnd);
        psessionEntry->pSchBeaconFrameEnd = NULL;
    }

    /* Must free the buffer before peSession invalid */
    if (NULL != psessionEntry->addIeParams.probeRespData_buff)
    {
        vos_mem_free(psessionEntry->addIeParams.probeRespData_buff);
        psessionEntry->addIeParams.probeRespData_buff = NULL;
        psessionEntry->addIeParams.probeRespDataLen = 0;
    }
    if (NULL != psessionEntry->addIeParams.assocRespData_buff)
    {
        vos_mem_free(psessionEntry->addIeParams.assocRespData_buff);
        psessionEntry->addIeParams.assocRespData_buff = NULL;
        psessionEntry->addIeParams.assocRespDataLen = 0;
    }
    if (NULL != psessionEntry->addIeParams.probeRespBCNData_buff)
    {
        vos_mem_free(psessionEntry->addIeParams.probeRespBCNData_buff);
        psessionEntry->addIeParams.probeRespBCNData_buff = NULL;
        psessionEntry->addIeParams.probeRespBCNDataLen = 0;
    }

#ifdef WLAN_FEATURE_11W
    /* if PMF connection */
    if (psessionEntry->limRmfEnabled) {
        vos_timer_destroy(&psessionEntry->pmfComebackTimer);
    }
#endif

    psessionEntry->valid = FALSE;

    if (LIM_IS_AP_ROLE(psessionEntry))
         lim_check_and_reset_protection_params(pMac);

    return;
}


/*--------------------------------------------------------------------------
  \brief peFindSessionByPeerSta() - looks up the PE session given the Station Address.

  This function returns the session context and the session ID if the session
  corresponding to the given station address is found in the PE session table.

  \param pMac                   - pointer to global adapter context
  \param sa                       - Peer STA Address of the session
  \param sessionId             -session ID is returned here, if session is found.

  \return tpPESession          - pointer to the session context or NULL if session is not found.

  \sa
  --------------------------------------------------------------------------*/


tpPESession peFindSessionByPeerSta(tpAniSirGlobal pMac,  tANI_U8*  sa,    tANI_U8* sessionId)
{
   tANI_U8 i;
   tpDphHashNode pSta;
   tANI_U16  aid;

   for(i =0; i < pMac->lim.maxBssId; i++)
   {
      if( (pMac->lim.gpSession[i].valid))
      {
         pSta = dphLookupHashEntry(pMac, sa, &aid, &pMac->lim.gpSession[i].dph.dphHashTable);
         if (pSta != NULL)
         {
            *sessionId = i;
            return &pMac->lim.gpSession[i];
         }
      }
   }

   limLog(pMac, LOG1, FL("Session lookup fails for Peer StaId:"));
   limPrintMacAddr(pMac, sa, LOG1);
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
int pe_get_active_session_count(tpAniSirGlobal mac_ctx)
{
	int i, active_session_count = 0;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++)
		if (mac_ctx->lim.gpSession[i].valid)
			active_session_count++;

	return active_session_count;
}
