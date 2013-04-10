/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

/**=========================================================================
  
  \file  limSession.c
  
  \brief implementation for lim Session related APIs

  \author Sunit Bhatia
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/


/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "aniGlobal.h"
#include "limDebug.h"
#include "limSession.h"
#include "limUtils.h"
#ifdef FEATURE_WLAN_CCX
#include "ccxApi.h"
#endif

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
    palZeroMemory(pMac->hHdd, (void*)&psessionEntry->gLim11bParams, sizeof(tLimProtStaParams)); 
    palZeroMemory(pMac->hHdd, (void*)&psessionEntry->gLim11aParams, sizeof(tLimProtStaParams)); 
    palZeroMemory(pMac->hHdd, (void*)&psessionEntry->gLim11gParams, sizeof(tLimProtStaParams)); 
    palZeroMemory(pMac->hHdd, (void*)&psessionEntry->gLimNonGfParams, sizeof(tLimProtStaParams)); 
    palZeroMemory(pMac->hHdd, (void*)&psessionEntry->gLimHt20Params, sizeof(tLimProtStaParams)); 
    palZeroMemory(pMac->hHdd, (void*)&psessionEntry->gLimLsigTxopParams, sizeof(tLimProtStaParams)); 
    palZeroMemory(pMac->hHdd, (void*)&psessionEntry->gLimOlbcParams, sizeof(tLimProtStaParams));
}

/*--------------------------------------------------------------------------
  
  \brief peCreateSession() - creates a new PE session given the BSSID

  This function returns the session context and the session ID if the session 
  corresponding to the passed BSSID is found in the PE session table.
    
  \param pMac                   - pointer to global adapter context
  \param bssid                   - BSSID of the new session
  \param sessionId             -session ID is returned here, if session is created.
  
  \return tpPESession          - pointer to the session context or NULL if session can not be created.
  
  \sa
  
  --------------------------------------------------------------------------*/
tpPESession peCreateSession(tpAniSirGlobal pMac, tANI_U8 *bssid , tANI_U8* sessionId, tANI_U16 numSta)
{
    tANI_U8 i;
    for(i =0; i < pMac->lim.maxBssId; i++)
    {
        /* Find first free room in session table */
        if(pMac->lim.gpSession[i].valid == FALSE)
        {
            palZeroMemory(pMac, (void*)&pMac->lim.gpSession[i], sizeof(tPESession));

            //Allocate space for Station Table for this session.
            if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
                     (void **) &pMac->lim.gpSession[i].dph.dphHashTable.pHashTable, sizeof(tpDphHashNode)*numSta))
            {
                limLog(pMac, LOGE, FL("memory allocate failed!"));
                return NULL;
            }

            if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
                  (void **) &pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray, sizeof(tDphHashNode)*numSta))
            {
                limLog(pMac, LOGE, FL("memory allocate failed!"));
                palFreeMemory(pMac->hHdd,pMac->lim.gpSession[i].dph.dphHashTable.pHashTable);
                return NULL;
            }
            pMac->lim.gpSession[i].dph.dphHashTable.size = numSta;

            dphHashTableClassInit(pMac, 
                           &pMac->lim.gpSession[i].dph.dphHashTable);

            if (eHAL_STATUS_SUCCESS != palAllocateMemory(pMac->hHdd,
                    (void **) &pMac->lim.gpSession[i].gpLimPeerIdxpool, 
                    sizeof(*pMac->lim.gpSession[i].gpLimPeerIdxpool) * (numSta+1)))
            {
                PELOGE(limLog(pMac, LOGE, FL("memory allocate failed!"));)
                palFreeMemory(pMac->hHdd,pMac->lim.gpSession[i].dph.dphHashTable.pHashTable);
                palFreeMemory(pMac->hHdd,pMac->lim.gpSession[i].dph.dphHashTable.pDphNodeArray);
                return NULL;
            }
            palZeroMemory(pMac->hHdd, pMac->lim.gpSession[i].gpLimPeerIdxpool,
                  sizeof(*pMac->lim.gpSession[i].gpLimPeerIdxpool) * (numSta+1));
            pMac->lim.gpSession[i].freePeerIdxHead = 0;
            pMac->lim.gpSession[i].freePeerIdxTail = 0;
            pMac->lim.gpSession[i].gLimNumOfCurrentSTAs = 0;

            /* Copy the BSSID to the session table */
            sirCopyMacAddr(pMac->lim.gpSession[i].bssId, bssid);
            pMac->lim.gpSession[i].valid = TRUE;
            
            /* Intialize the SME and MLM states to IDLE */
            pMac->lim.gpSession[i].limMlmState = eLIM_MLM_IDLE_STATE;
            pMac->lim.gpSession[i].limSmeState = eLIM_SME_IDLE_STATE;
            pMac->lim.gpSession[i].limCurrentAuthType = eSIR_OPEN_SYSTEM;
            peInitBeaconParams(pMac, &pMac->lim.gpSession[i]);
#ifdef WLAN_FEATURE_VOWIFI_11R
            pMac->lim.gpSession[i].is11Rconnection = FALSE;
#endif

#ifdef FEATURE_WLAN_CCX
            pMac->lim.gpSession[i].isCCXconnection = FALSE;
#endif

#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_CCX || defined(FEATURE_WLAN_LFR)
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
            palZeroMemory(pMac->hHdd, pMac->lim.gpSession[i].peerAIDBitmap,
                  sizeof(pMac->lim.gpSession[i].peerAIDBitmap));
#endif
            pMac->lim.gpSession[i].fWaitForProbeRsp = 0;
            pMac->lim.gpSession[i].fIgnoreCapsChange = 0;
            return(&pMac->lim.gpSession[i]);
        }
    }
    limLog(pMac, LOGE, FL("Session can not be created.. Reached Max permitted sessions \n "));
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

    limLog(pMac, LOG4, FL("Session lookup fails for BSSID: \n "));
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

/*--------------------------------------------------------------------------
  \brief peFindSessionBySessionId() - looks up the PE session given the session ID.

  This function returns the session context  if the session 
  corresponding to the given session ID is found in the PE session table.
    
  \param pMac                   - pointer to global adapter context
  \param sessionId             -session ID for which session context needs to be looked up.
  
  \return tpPESession          - pointer to the session context or NULL if session is not found.
  
  \sa
  --------------------------------------------------------------------------*/
 tpPESession peFindSessionBySessionId(tpAniSirGlobal pMac , tANI_U8 sessionId)
{
    if(sessionId >=  pMac->lim.maxBssId)
    {
        limLog(pMac, LOGE, FL("Invalid sessionId: %d \n "), sessionId);
        return(NULL);
    }
    if((pMac->lim.gpSession[sessionId].valid == TRUE))
    {
        return(&pMac->lim.gpSession[sessionId]);
    }
    limLog(pMac, LOG1, FL("Session %d  not active\n "), sessionId);
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

    limLog(pMac, LOG4, FL("Session lookup fails for StaId: %d\n "), staid);
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

    limLog(pMac, LOGW, FL("Trying to delete a session %d.\n "), psessionEntry->peSessionId);

    for (n = 0; n < pMac->lim.maxStation; n++)
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
    
    if(psessionEntry->pLimStartBssReq != NULL)
    {
        palFreeMemory( pMac->hHdd, psessionEntry->pLimStartBssReq );
        psessionEntry->pLimStartBssReq = NULL;
    }

    if(psessionEntry->pLimJoinReq != NULL)
    {
        palFreeMemory( pMac->hHdd, psessionEntry->pLimJoinReq );
        psessionEntry->pLimJoinReq = NULL;
    }

    if(psessionEntry->pLimReAssocReq != NULL)
    {
        palFreeMemory( pMac->hHdd, psessionEntry->pLimReAssocReq );
        psessionEntry->pLimReAssocReq = NULL;
    }

    if(psessionEntry->pLimMlmJoinReq != NULL)
    {
        palFreeMemory( pMac->hHdd, psessionEntry->pLimMlmJoinReq );
        psessionEntry->pLimMlmJoinReq = NULL;
    }

    if(psessionEntry->dph.dphHashTable.pHashTable != NULL)
    {
        palFreeMemory(pMac->hHdd, psessionEntry->dph.dphHashTable.pHashTable);
        psessionEntry->dph.dphHashTable.pHashTable = NULL;
    }

    if(psessionEntry->dph.dphHashTable.pDphNodeArray != NULL)
    {
        palFreeMemory(pMac->hHdd, psessionEntry->dph.dphHashTable.pDphNodeArray);
        psessionEntry->dph.dphHashTable.pDphNodeArray = NULL;
    }

    if(psessionEntry->gpLimPeerIdxpool != NULL)
    {
        palFreeMemory(pMac->hHdd, psessionEntry->gpLimPeerIdxpool);
        psessionEntry->gpLimPeerIdxpool = NULL;
    }

    if(psessionEntry->beacon != NULL)
    {
        palFreeMemory( pMac->hHdd, psessionEntry->beacon);
        psessionEntry->beacon = NULL;
    }

    if(psessionEntry->assocReq != NULL)
    {
        palFreeMemory( pMac->hHdd, psessionEntry->assocReq);
        psessionEntry->assocReq = NULL;
    }

    if(psessionEntry->assocRsp != NULL)
    {
        palFreeMemory( pMac->hHdd, psessionEntry->assocRsp);
        psessionEntry->assocRsp = NULL;
    }


    if(psessionEntry->parsedAssocReq != NULL)
    {
        // Cleanup the individual allocation first
        for (i=0; i < psessionEntry->dph.dphHashTable.size; i++)
        {
            if ( psessionEntry->parsedAssocReq[i] != NULL )
            {
                if( ((tpSirAssocReq)(psessionEntry->parsedAssocReq[i]))->assocReqFrame )
                {
                   palFreeMemory(pMac->hHdd, 
                      ((tpSirAssocReq)(psessionEntry->parsedAssocReq[i]))->assocReqFrame);
                   ((tpSirAssocReq)(psessionEntry->parsedAssocReq[i]))->assocReqFrame = NULL;
                   ((tpSirAssocReq)(psessionEntry->parsedAssocReq[i]))->assocReqFrameLength = 0;
                }
                palFreeMemory(pMac->hHdd, (void *)psessionEntry->parsedAssocReq[i]);
                psessionEntry->parsedAssocReq[i] = NULL;
            }
        }
        // Cleanup the whole block
        palFreeMemory(pMac->hHdd, (void *)psessionEntry->parsedAssocReq);
        psessionEntry->parsedAssocReq = NULL;
    }
    if (NULL != psessionEntry->limAssocResponseData)
    {
        palFreeMemory( pMac->hHdd, psessionEntry->limAssocResponseData);
        psessionEntry->limAssocResponseData = NULL;
    }

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
    if (NULL != psessionEntry->pLimMlmReassocRetryReq)
    {
        palFreeMemory( pMac->hHdd, psessionEntry->pLimMlmReassocRetryReq);
        psessionEntry->pLimMlmReassocRetryReq = NULL;
    }
#endif

    if (NULL != psessionEntry->pLimMlmReassocReq)
    {
        palFreeMemory( pMac->hHdd, psessionEntry->pLimMlmReassocReq);
        psessionEntry->pLimMlmReassocReq = NULL;
    }

#ifdef FEATURE_WLAN_CCX
    limCleanupCcxCtxt(pMac, psessionEntry); 
#endif

    psessionEntry->valid = FALSE;
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

   limLog(pMac, LOG1, FL("Session lookup fails for Peer StaId: \n "));
   limPrintMacAddr(pMac, sa, LOG1);
   return NULL;
}









