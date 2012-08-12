/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
 * */
/** ------------------------------------------------------------------------- * 
    ------------------------------------------------------------------------- *  

  
    \file csrNeighborRoam.c
  
    Implementation for the simple roaming algorithm for 802.11r Fast transitions and Legacy roaming for Android platform.
  
    Copyright (C) 2010 Qualcomm, Incorporated
  
 
   ========================================================================== */

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.



  when           who                 what, where, why
----------       ---                --------------------------------------------------------
08/01/10          Murali             Created

===========================================================================*/
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
#include "wlan_qct_wda.h"
#include "palApi.h"
#include "csrInsideApi.h"
#include "smsDebug.h"
#include "logDump.h"
#include "smeQosInternal.h"
#include "wlan_qct_tl.h"
#include "smeInside.h"
#include "vos_diag_core_event.h"
#include "vos_diag_core_log.h"
#include "csrApi.h"
#include "wlan_qct_tl.h"
#include "sme_Api.h"
#include "csrNeighborRoam.h"
#ifdef FEATURE_WLAN_CCX
#include "csrCcx.h"
#endif

#define WLAN_FEATURE_NEIGHBOR_ROAMING_DEBUG 1
#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING_DEBUG
#define NEIGHBOR_ROAM_DEBUG smsLog
#else
#define NEIGHBOR_ROAM_DEBUG(x...)
#endif

VOS_STATUS csrNeighborRoamNeighborLookupUPCallback (v_PVOID_t pAdapter, v_U8_t rssiNotification,
                                                                               v_PVOID_t pUserCtxt);
VOS_STATUS csrNeighborRoamNeighborLookupDOWNCallback (v_PVOID_t pAdapter, v_U8_t rssiNotification,
                                                                               v_PVOID_t pUserCtxt);
void csrNeighborRoamRRMNeighborReportResult(void *context, VOS_STATUS vosStatus);
eHalStatus csrRoamCopyConnectedProfile(tpAniSirGlobal pMac, tANI_U32 sessionId, tCsrRoamProfile *pDstProfile );

#ifdef WLAN_FEATURE_VOWIFI_11R
static eHalStatus csrNeighborRoamIssuePreauthReq(tpAniSirGlobal pMac);
VOS_STATUS csrNeighborRoamIssueNeighborRptRequest(tpAniSirGlobal pMac);
#endif

/* State Transition macro */
#define CSR_NEIGHBOR_ROAM_STATE_TRANSITION(newState)\
{\
    pMac->roam.neighborRoamInfo.prevNeighborRoamState = pMac->roam.neighborRoamInfo.neighborRoamState;\
    pMac->roam.neighborRoamInfo.neighborRoamState = newState;\
    smsLog(pMac, LOGE, FL("Neighbor Roam Transition from state %d ==> %d"), pMac->roam.neighborRoamInfo.prevNeighborRoamState, newState);\
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamFreeNeighborRoamBSSNode

    \brief  This function frees all the internal pointers CSR NeighborRoam BSS Info 
            and also frees the node itself

    \param  pMac - The handle returned by macOpen.
            neighborRoamBSSNode - Neighbor Roam BSS Node to be freed

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamFreeNeighborRoamBSSNode(tpAniSirGlobal pMac, tpCsrNeighborRoamBSSInfo neighborRoamBSSNode)
{
    if (neighborRoamBSSNode)
    {
        if (neighborRoamBSSNode->pBssDescription)
        {
            vos_mem_free(neighborRoamBSSNode->pBssDescription);
            neighborRoamBSSNode->pBssDescription = NULL;
        }
        vos_mem_free(neighborRoamBSSNode);
        neighborRoamBSSNode = NULL;
    }

    return;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamRemoveRoamableAPListEntry

    \brief  This function removes a given entry from the given list

    \param  pMac - The handle returned by macOpen.
            pList - The list from which the entry should be removed
            pNeighborEntry - Neighbor Roam BSS Node to be removed

    \return TRUE if successfully removed, else FALSE

---------------------------------------------------------------------------*/
tANI_BOOLEAN csrNeighborRoamRemoveRoamableAPListEntry(tpAniSirGlobal pMac, 
                                                tDblLinkList *pList, tpCsrNeighborRoamBSSInfo pNeighborEntry)
{
    if(pList)
    {
        return csrLLRemoveEntry(pList, &pNeighborEntry->List, LL_ACCESS_LOCK);
    }

    smsLog(pMac, LOGE, FL("Removing neighbor BSS node from list failed. Current count = %d\n"), csrLLCount(pList));

    return eANI_BOOLEAN_FALSE;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamGetRoamableAPListNextEntry

    \brief  Gets the entry next to passed entry. If NULL is passed, return the entry in the head of the list

    \param  pMac - The handle returned by macOpen.
            pList - The list from which the entry should be returned
            pNeighborEntry - Neighbor Roam BSS Node whose next entry should be returned

    \return Neighbor Roam BSS Node to be returned

---------------------------------------------------------------------------*/
tpCsrNeighborRoamBSSInfo csrNeighborRoamGetRoamableAPListNextEntry(tpAniSirGlobal pMac, 
                                        tDblLinkList *pList, tpCsrNeighborRoamBSSInfo pNeighborEntry)
{
    tListElem *pEntry = NULL;
    tpCsrNeighborRoamBSSInfo pResult = NULL;
    
    if(pList)
    {
        if(NULL == pNeighborEntry)
        {
            pEntry = csrLLPeekHead(pList, LL_ACCESS_LOCK);
        }
        else
        {
            pEntry = csrLLNext(pList, &pNeighborEntry->List, LL_ACCESS_LOCK);
        }
        if(pEntry)
        {
            pResult = GET_BASE_ADDR(pEntry, tCsrNeighborRoamBSSInfo, List);
        }
    }
    
    return pResult;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamFreeRoamableBSSList

    \brief   Empties and frees all the nodes in the roamable AP list 

    \param  pMac - The handle returned by macOpen.
            pList - Neighbor Roam BSS List to be emptied

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamFreeRoamableBSSList(tpAniSirGlobal pMac, tDblLinkList *pList)
{
    tpCsrNeighborRoamBSSInfo pResult = NULL;

    NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Emptying the BSS list. Current count = %d\n"), csrLLCount(pList));

    /* Pick up the head, remove and free the node till the list becomes empty */
    while ((pResult = csrNeighborRoamGetRoamableAPListNextEntry(pMac, pList, NULL)) != NULL)
    {
        csrNeighborRoamRemoveRoamableAPListEntry(pMac, pList, pResult);
        csrNeighborRoamFreeNeighborRoamBSSNode(pMac, pResult);
    }
    return;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamReassocIndCallback

    \brief Reassoc callback invoked by TL on crossing the registered re-assoc threshold.
           Directly triggere HO in case of non-11r association
           In case of 11R association, triggers a pre-auth eventually followed by actual HO

    \param  pAdapter - VOS Context
            trafficStatus - UP/DOWN indication from TL
            pUserCtxt - Parameter for callback registered during callback registration. Should be pMac

    \return VOID

---------------------------------------------------------------------------*/
VOS_STATUS csrNeighborRoamReassocIndCallback(v_PVOID_t pAdapter, 
                               v_U8_t trafficStatus, 
                               v_PVOID_t pUserCtxt)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( pUserCtxt );
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;   
 
    smsLog(pMac, LOG1, FL("Reassoc indication callback called"));


    //smsLog(pMac, LOGE, FL("Reassoc indication callback called at state %d"), pNeighborRoamInfo->neighborRoamState);

    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Deregistering DOWN event reassoc callback with TL. RSSI = %d"), pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1));


    vosStatus = WLANTL_DeregRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1),
                                                        WLANTL_HO_THRESHOLD_DOWN, 
                                                        csrNeighborRoamReassocIndCallback,
                                                        VOS_MODULE_ID_SME);
                        
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        //err msg
        smsLog(pMac, LOGW, FL(" Couldn't deregister csrNeighborRoamReassocIndCallback with TL: Status = %d\n"), vosStatus);
    }
    
    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Deregistering UP event neighbor lookup callback with TL. RSSI = %d"), pNeighborRoamInfo->cfgParams.neighborLookupThreshold * (-1));
    /* Deregister reassoc callback. Ignore return status */
    vosStatus = WLANTL_DeregRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->cfgParams.neighborLookupThreshold * (-1),
                                                        WLANTL_HO_THRESHOLD_DOWN, 
                                                        csrNeighborRoamNeighborLookupUPCallback,
                                                        VOS_MODULE_ID_SME);
                        
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        //err msg
        smsLog(pMac, LOGW, FL(" Couldn't deregister csrNeighborRoamReassocIndCallback with TL: Status = %d\n"), vosStatus);
    }

    /* We dont need to run this timer any more. */
    palTimerStop(pMac->hHdd, pNeighborRoamInfo->neighborResultsRefreshTimer);

#ifdef WLAN_FEATURE_VOWIFI_11R
    if (pNeighborRoamInfo->is11rAssoc)
    {
        if (eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN == pNeighborRoamInfo->neighborRoamState)
        {
            csrNeighborRoamIssuePreauthReq(pMac);
        }
        else
        {
            smsLog(pMac, LOGE, FL("11R Reassoc indication received in unexpected state %d"), pNeighborRoamInfo->neighborRoamState);
            VOS_ASSERT(0);
        }
    }
    else
#endif

#ifdef FEATURE_WLAN_CCX
    if (pNeighborRoamInfo->isCCXAssoc)
    {
        if (eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN == pNeighborRoamInfo->neighborRoamState)
        {
            csrNeighborRoamIssuePreauthReq(pMac);
        }
        else
        {
            smsLog(pMac, LOGE, FL("CCX Reassoc indication received in unexpected state %d"), pNeighborRoamInfo->neighborRoamState);
            VOS_ASSERT(0);
        }
    }
    else
#endif
    {
        if (eCSR_NEIGHBOR_ROAM_STATE_CFG_CHAN_LIST_SCAN == pNeighborRoamInfo->neighborRoamState)
        {
            csrNeighborRoamRequestHandoff(pMac);
        }
        else
        {
            smsLog(pMac, LOGE, FL("Non-11R Reassoc indication received in unexpected state %d"), pNeighborRoamInfo->neighborRoamState);
            VOS_ASSERT(0);
        }
    }
    return VOS_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamResetConnectedStateControlInfo

    \brief  This function will reset the neighbor roam control info data structures. 
            This function should be invoked whenever we move to CONNECTED state from 
            any state other than INIT state

    \param  pMac - The handle returned by macOpen.

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamResetConnectedStateControlInfo(tpAniSirGlobal pMac)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;

    /* Do not reset the currentNeighborLookup Threshold here. The threshold and multiplier will be set before calling this API */
    if ((pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived == FALSE) &&
        (pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels))
    {
        pNeighborRoamInfo->roamChannelInfo.currentChanIndex = CSR_NEIGHBOR_ROAM_INVALID_CHANNEL_INDEX;
        pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels = 0;

        if (pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList)
            vos_mem_free(pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList);
    
        pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL;
        pNeighborRoamInfo->roamChannelInfo.chanListScanInProgress = eANI_BOOLEAN_FALSE;
    }
    else 
    {
        pNeighborRoamInfo->roamChannelInfo.currentChanIndex = 0;
        pNeighborRoamInfo->roamChannelInfo.chanListScanInProgress = eANI_BOOLEAN_TRUE;
    }

    csrNeighborRoamFreeRoamableBSSList(pMac, &pNeighborRoamInfo->roamableAPList);

    palTimerStop(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer);

    /* Abort any ongoing BG scans */
    if (eANI_BOOLEAN_TRUE == pNeighborRoamInfo->scanRspPending)
        csrScanAbortMacScan(pMac);

    pNeighborRoamInfo->scanRspPending = eANI_BOOLEAN_FALSE;
    
    /* We dont need to run this timer any more. */
    palTimerStop(pMac->hHdd, pNeighborRoamInfo->neighborResultsRefreshTimer);

#ifdef WLAN_FEATURE_VOWIFI_11R
    /* Do not free up the preauth done list here */
    pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum = 0;
    pNeighborRoamInfo->FTRoamInfo.neighborRptPending = eANI_BOOLEAN_FALSE;
    pNeighborRoamInfo->FTRoamInfo.numPreAuthRetries = 0;
    pNeighborRoamInfo->FTRoamInfo.numBssFromNeighborReport = 0;
    pNeighborRoamInfo->FTRoamInfo.preauthRspPending = 0;
    vos_mem_zero(pNeighborRoamInfo->FTRoamInfo.neighboReportBssInfo, sizeof(tCsrNeighborReportBssInfo) * MAX_BSS_IN_NEIGHBOR_RPT);
    palTimerStop(pMac->hHdd, pNeighborRoamInfo->FTRoamInfo.preAuthRspWaitTimer);
#endif

}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamResetInitStateControlInfo

    \brief  This function will reset the neighbor roam control info data structures. 
            This function should be invoked whenever we move to CONNECTED state from 
            INIT state

    \param  pMac - The handle returned by macOpen.

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamResetInitStateControlInfo(tpAniSirGlobal pMac)
{
    tpCsrNeighborRoamControlInfo pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    VOS_STATUS                    vosStatus = VOS_STATUS_SUCCESS;

    csrNeighborRoamResetConnectedStateControlInfo(pMac);

    /* In addition to the above resets, we should clear off the curAPBssId/Session ID in the timers */
    pNeighborRoamInfo->csrSessionId            =   CSR_SESSION_ID_INVALID;
    vos_mem_set(pNeighborRoamInfo->currAPbssid, sizeof(tCsrBssid), 0);
    pNeighborRoamInfo->neighborScanTimerInfo.pMac = pMac;
    pNeighborRoamInfo->neighborScanTimerInfo.sessionId = CSR_SESSION_ID_INVALID;
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX)
    pNeighborRoamInfo->is11rAssoc = eANI_BOOLEAN_FALSE;
    pNeighborRoamInfo->FTRoamInfo.preAuthRspWaitTimerInfo.pMac = pMac;
    pNeighborRoamInfo->FTRoamInfo.preAuthRspWaitTimerInfo.sessionId = CSR_SESSION_ID_INVALID;
    csrNeighborRoamPurgePreauthFailedList(pMac);
#endif
#ifdef FEATURE_WLAN_CCX
    pNeighborRoamInfo->isCCXAssoc = eANI_BOOLEAN_FALSE;
    pNeighborRoamInfo->isVOAdmitted = eANI_BOOLEAN_FALSE;
    pNeighborRoamInfo->MinQBssLoadRequired = 0;
#endif
    
    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Deregistering DOWN event reassoc callback with TL. RSSI = %d"), pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1));
    /* Deregister reassoc callback. Ignore return status */
    vosStatus = WLANTL_DeregRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1),
                                                        WLANTL_HO_THRESHOLD_DOWN, 
                                                        csrNeighborRoamReassocIndCallback,
                                                        VOS_MODULE_ID_SME);
                        
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        //err msg
        smsLog(pMac, LOGW, FL(" Couldn't deregister csrNeighborRoamReassocIndCallback with TL: Status = %d\n"), vosStatus);
    }

    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Deregistering DOWN event neighborLookup callback with TL. RSSI = %d"), pNeighborRoamInfo->currentNeighborLookupThreshold * (-1));
    /* Deregister neighbor lookup callback. Ignore return status */
    vosStatus = WLANTL_DeregRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->currentNeighborLookupThreshold * (-1),
                                                        WLANTL_HO_THRESHOLD_DOWN, 
                                                        csrNeighborRoamNeighborLookupDOWNCallback,
                                                        VOS_MODULE_ID_SME);
                        
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        //err msg
        smsLog(pMac, LOGW, FL(" Couldn't deregister csrNeighborRoamNeighborLookupDOWNCallback with TL: Status = %d\n"), vosStatus);
    }

    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Deregistering UP event neighbor lookup callback with TL. RSSI = %d"), pNeighborRoamInfo->cfgParams.neighborLookupThreshold * (-1));
    /* Deregister reassoc callback. Ignore return status */
    vosStatus = WLANTL_DeregRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->cfgParams.neighborLookupThreshold * (-1),
                                                        WLANTL_HO_THRESHOLD_UP, 
                                                        csrNeighborRoamNeighborLookupUPCallback,
                                                        VOS_MODULE_ID_SME);
                        
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        //err msg
        smsLog(pMac, LOGW, FL(" Couldn't deregister csrNeighborRoamReassocIndCallback with TL: Status = %d\n"), vosStatus);
    }
    
    /* Reset currentNeighborLookupThreshold only after deregistering DOWN event from TL */
    pNeighborRoamInfo->currentLookupIncrementMultiplier = 0;
    pNeighborRoamInfo->currentNeighborLookupThreshold = pNeighborRoamInfo->cfgParams.neighborLookupThreshold;

    return;
}

#ifdef WLAN_FEATURE_VOWIFI_11R
/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamBssIdScanFilter

    \brief  This API is used to prepare a filter to obtain scan results when 
            we complete the scan in the REPORT_SCAN state after receiving a 
            valid neighbor report from AP. This filter includes BSSIDs received from 
            the neighbor report from the AP in addition to the other filter parameters 
            created from connected profile

    \param  pMac - The handle returned by macOpen.
            pScanFilter - Scan filter to be filled and returned

    \return eHAL_STATUS_SUCCESS on succesful filter creation, corresponding error 
            code otherwise

---------------------------------------------------------------------------*/
static eHalStatus csrNeighborRoamBssIdScanFilter(tpAniSirGlobal pMac, tCsrScanResultFilter *pScanFilter)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tANI_U8 i = 0;

    VOS_ASSERT(pScanFilter != NULL);
    vos_mem_zero(pScanFilter, sizeof(tCsrScanResultFilter));

    pScanFilter->BSSIDs.numOfBSSIDs = pNeighborRoamInfo->FTRoamInfo.numBssFromNeighborReport;
    pScanFilter->BSSIDs.bssid = vos_mem_malloc(sizeof(tSirMacAddr) * pScanFilter->BSSIDs.numOfBSSIDs);
    if (NULL == pScanFilter->BSSIDs.bssid)
    {
        smsLog(pMac, LOGE, FL("Scan Filter BSSID mem alloc failed"));
        return eHAL_STATUS_FAILED_ALLOC;
    }

    vos_mem_zero(pScanFilter->BSSIDs.bssid, sizeof(tSirMacAddr) * pScanFilter->BSSIDs.numOfBSSIDs);

    /* Populate the BSSID from Neighbor BSS info received from neighbor report */
    for (i = 0; i < pScanFilter->BSSIDs.numOfBSSIDs; i++)
    {
        vos_mem_copy(&pScanFilter->BSSIDs.bssid[i], 
                pNeighborRoamInfo->FTRoamInfo.neighboReportBssInfo[i].neighborBssId, sizeof(tSirMacAddr));
    }

    /* Fill other general scan filter params */
    return csrNeighborRoamPrepareScanProfileFilter(pMac, pScanFilter);
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamPurgePreauthFailList

    \brief  This function empties the preauth fail list

    \param  pMac - The handle returned by macOpen.

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamPurgePreauthFailList(tpAniSirGlobal pMac)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;

    NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Purging the preauth fail list"));
    while (pNeighborRoamInfo->FTRoamInfo.preAuthFailList.numMACAddress)
    {
        vos_mem_zero(pNeighborRoamInfo->FTRoamInfo.preAuthFailList.macAddress[pNeighborRoamInfo->FTRoamInfo.preAuthFailList.numMACAddress-1],
                                    sizeof(tSirMacAddr));
        pNeighborRoamInfo->FTRoamInfo.preAuthFailList.numMACAddress--;
    }
    return;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamAddBssIdToPreauthFailList

    \brief  This function adds the given BSSID to the Preauth fail list

    \param  pMac - The handle returned by macOpen.
            bssId - BSSID to be added to the preauth fail list

    \return eHAL_STATUS_SUCCESS on success, eHAL_STATUS_FAILURE otherwise

---------------------------------------------------------------------------*/
eHalStatus csrNeighborRoamAddBssIdToPreauthFailList(tpAniSirGlobal pMac, tSirMacAddr bssId)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;

    NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL(" Added BSSID %02x:%02x:%02x:%02x:%02x:%02x to Preauth failed list\n"), 
                        bssId[0], bssId[1], bssId[2], bssId[3], bssId[4], bssId[5]);


    if ((pNeighborRoamInfo->FTRoamInfo.preAuthFailList.numMACAddress + 1) >
            MAX_NUM_PREAUTH_FAIL_LIST_ADDRESS)
    {
        smsLog(pMac, LOGE, FL("Preauth fail list already full.. Cannot add new one"));
        return eHAL_STATUS_FAILURE;
    }
    vos_mem_copy(pNeighborRoamInfo->FTRoamInfo.preAuthFailList.macAddress[pNeighborRoamInfo->FTRoamInfo.preAuthFailList.numMACAddress],
                        bssId, sizeof(tSirMacAddr));
    pNeighborRoamInfo->FTRoamInfo.preAuthFailList.numMACAddress++;
    
    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamIsPreauthCandidate

    \brief  This function checks whether the given MAC address is already 
            present in the preauth fail list and returns TRUE/FALSE accordingly

    \param  pMac - The handle returned by macOpen.

    \return eANI_BOOLEAN_TRUE if preauth candidate, eANI_BOOLEAN_FALSE otherwise

---------------------------------------------------------------------------*/
tANI_BOOLEAN csrNeighborRoamIsPreauthCandidate(tpAniSirGlobal pMac, tSirMacAddr bssId)
{
    tANI_U8 i = 0;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;

    if (0 == pNeighborRoamInfo->FTRoamInfo.preAuthFailList.numMACAddress)
        return eANI_BOOLEAN_TRUE;
    
    for (i = 0; i < pNeighborRoamInfo->FTRoamInfo.preAuthFailList.numMACAddress; i++)
    {
        if (VOS_TRUE == vos_mem_compare(pNeighborRoamInfo->FTRoamInfo.preAuthFailList.macAddress[i],
                                                                        bssId, sizeof(tSirMacAddr)))
        {
            NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("BSSID %02x:%02x:%02x:%02x:%02x:%02x already present in preauth fail list"),
                                                bssId[0], bssId[1], bssId[2], bssId[3], bssId[4], bssId[5]);
            return eANI_BOOLEAN_FALSE;
        }
    }

    return eANI_BOOLEAN_TRUE;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamIssuePreauthReq

    \brief  This function issues preauth request to PE with the 1st AP entry in the 
            roamable AP list

    \param  pMac - The handle returned by macOpen.

    \return eHAL_STATUS_SUCCESS on success, eHAL_STATUS_FAILURE otherwise

---------------------------------------------------------------------------*/
static eHalStatus csrNeighborRoamIssuePreauthReq(tpAniSirGlobal pMac)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpCsrNeighborRoamBSSInfo    pNeighborBssNode;
    
    /* This must not be true here */
    VOS_ASSERT(pNeighborRoamInfo->FTRoamInfo.preauthRspPending == eANI_BOOLEAN_FALSE);

    /* Issue Preauth request to PE here */
    /* Need to issue the preauth request with the BSSID that is there in the head of the roamable AP list */
    /* Parameters that should be passed are BSSID, Channel number and the neighborScanPeriod(probably) */
    /* If roamableAPList gets empty, should transition to REPORT_SCAN state */
    pNeighborBssNode = csrNeighborRoamGetRoamableAPListNextEntry(pMac, &pNeighborRoamInfo->roamableAPList, NULL);

    if (NULL == pNeighborBssNode)
    {
        smsLog(pMac, LOG1, FL("Roamable AP list is empty.. "));
        return eHAL_STATUS_FAILURE;
    }
    else
    {
        status = csrRoamIssueFTPreauthReq(pMac, pNeighborRoamInfo->csrSessionId, pNeighborBssNode->pBssDescription); 
        if (eHAL_STATUS_SUCCESS != status)
        {
            smsLog(pMac, LOGE, FL("Send Preauth request to PE failed with status %d\n"), status);
            return status;
        }
    }
    
    pNeighborRoamInfo->FTRoamInfo.preauthRspPending = eANI_BOOLEAN_TRUE;

    /* Increment the preauth retry count */
    pNeighborRoamInfo->FTRoamInfo.numPreAuthRetries++;
    
    /* Transition the state to preauthenticating */
    CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING)
    
    /* Start the preauth rsp timer */
    status = palTimerStart(pMac->hHdd, pNeighborRoamInfo->FTRoamInfo.preAuthRspWaitTimer, 
                   CSR_NEIGHBOR_ROAM_PREAUTH_RSP_WAIT_MULTIPLIER * pNeighborRoamInfo->cfgParams.neighborScanPeriod * PAL_TIMER_TO_MS_UNIT,
                   eANI_BOOLEAN_FALSE);
    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("Preauth response wait timer start failed with status %d\n"), status);
        return status;
    }

    
    return status;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamPreauthRspHandler

    \brief  This function handle the Preauth response from PE
            Every preauth is allowed max 3 tries if it fails. If a bssid failed 
            for more than MAX_TRIES, we will remove it from the list and try 
            with the next node in the roamable AP list and add the BSSID to pre-auth failed 
            list. If no more entries present in 
            roamable AP list, transition to REPORT_SCAN state

    \param  pMac - The handle returned by macOpen.
            vosStatus - VOS_STATUS_SUCCESS/FAILURE/TIMEOUT status from PE

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamPreauthRspHandler(tpAniSirGlobal pMac, VOS_STATUS vosStatus)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    eHalStatus  status = eHAL_STATUS_SUCCESS;
    tpCsrNeighborRoamBSSInfo pPreauthRspNode = NULL;

    // We can receive it in these 2 states.
    VOS_ASSERT((pNeighborRoamInfo->neighborRoamState == eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING) ||
        (pNeighborRoamInfo->neighborRoamState == eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN));

    if ((pNeighborRoamInfo->neighborRoamState != eCSR_NEIGHBOR_ROAM_STATE_PREAUTHENTICATING) &&
        (pNeighborRoamInfo->neighborRoamState != eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN))
    {
        NEIGHBOR_ROAM_DEBUG(pMac, LOGW, FL("Preauth response received in state %\n"), 
            pNeighborRoamInfo->neighborRoamState);
    }

    if (VOS_STATUS_E_TIMEOUT != vosStatus)
    {
        /* This means we got the response from PE. Hence stop the timer */
        status = palTimerStop(pMac->hHdd, pNeighborRoamInfo->FTRoamInfo.preAuthRspWaitTimer);
        pNeighborRoamInfo->FTRoamInfo.preauthRspPending = eANI_BOOLEAN_FALSE;
    }

    if (VOS_STATUS_SUCCESS == vosStatus)
    {
        pPreauthRspNode = csrNeighborRoamGetRoamableAPListNextEntry(pMac, &pNeighborRoamInfo->roamableAPList, NULL);
    }
    if ((VOS_STATUS_SUCCESS == vosStatus) && (NULL != pPreauthRspNode))
    {
        NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Preauth completed successfully after %d tries\n"), pNeighborRoamInfo->FTRoamInfo.numPreAuthRetries);

        /* Preauth competer successfully. Insert the preauthenticated node to tail of preAuthDoneList */
        csrNeighborRoamRemoveRoamableAPListEntry(pMac, &pNeighborRoamInfo->roamableAPList, pPreauthRspNode);
        csrLLInsertTail(&pNeighborRoamInfo->FTRoamInfo.preAuthDoneList, &pPreauthRspNode->List, LL_ACCESS_LOCK);

        /* Pre-auth completed successfully. Transition to PREAUTH Done state */
        CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE)
        pNeighborRoamInfo->FTRoamInfo.numPreAuthRetries = 0;

        /* The caller of this function would start a timer and by the time it expires, supplicant should 
           have provided the updated FTIEs to SME. So, when it expires, handoff will be triggered then */
    }
    else
    {
        tpCsrNeighborRoamBSSInfo    pNeighborBssNode = NULL;
        tListElem                   *pEntry;

        smsLog(pMac, LOGE, FL("Preauth failed retry number %d, status = %d\n"), pNeighborRoamInfo->FTRoamInfo.numPreAuthRetries, vosStatus);
        
        /* Preauth failed. Add the bssId to the preAuth failed list MAC Address. Also remove the AP from roamable AP list */
        if (pNeighborRoamInfo->FTRoamInfo.numPreAuthRetries >=  CSR_NEIGHBOR_ROAM_MAX_NUM_PREAUTH_RETRIES)
        {
            /* We are going to remove the node as it fails for more than MAX tries. Reset this count to 0 */
            pNeighborRoamInfo->FTRoamInfo.numPreAuthRetries = 0;

            /* The one in the head of the list should be one with which we issued pre-auth and failed */
            pEntry = csrLLRemoveHead(&pNeighborRoamInfo->roamableAPList, LL_ACCESS_LOCK);
            if(pEntry)
            {
                pNeighborBssNode = GET_BASE_ADDR(pEntry, tCsrNeighborRoamBSSInfo, List);
            /* Add the BSSID to pre-auth fail list */
            status = csrNeighborRoamAddBssIdToPreauthFailList(pMac, pNeighborBssNode->pBssDescription->bssId);
            /* Now we can free this node */
            csrNeighborRoamFreeNeighborRoamBSSNode(pMac, pNeighborBssNode);
            }
        }

        /* Issue preauth request for the same/next entry */
        if (eHAL_STATUS_SUCCESS == csrNeighborRoamIssuePreauthReq(pMac))
            return;

        CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN)

        /* Start the neighbor results refresh timer and transition to REPORT_SCAN state to perform scan again */
        status = palTimerStart(pMac->hHdd, pNeighborRoamInfo->neighborResultsRefreshTimer, 
                        pNeighborRoamInfo->cfgParams.neighborResultsRefreshPeriod * PAL_TIMER_TO_MS_UNIT, 
                        eANI_BOOLEAN_FALSE);
        if (eHAL_STATUS_SUCCESS != status)
        {
            smsLog(pMac, LOGE, FL("Neighbor results refresh timer start failed with status %d\n"), status);
            return;
        }
    }
}
#endif  /* WLAN_FEATURE_NEIGHBOR_ROAMING */

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamPrepareScanProfileFilter

    \brief  This function creates a scan filter based on the currently connected profile.
            Based on this filter, scan results are obtained

    \param  pMac - The handle returned by macOpen.
            pScanFilter - Populated scan filter based on the connected profile

    \return eHAL_STATUS_SUCCESS on success, eHAL_STATUS_FAILURE otherwise

---------------------------------------------------------------------------*/
eHalStatus csrNeighborRoamPrepareScanProfileFilter(tpAniSirGlobal pMac, tCsrScanResultFilter *pScanFilter)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tANI_U8 sessionId   = (tANI_U8)pNeighborRoamInfo->csrSessionId;
    tCsrRoamConnectedProfile *pCurProfile = &pMac->roam.roamSession[sessionId].connectedProfile;
    tANI_U8 i = 0;
    
    VOS_ASSERT(pScanFilter != NULL);

    vos_mem_zero(pScanFilter, sizeof(tCsrScanResultFilter));

    /* We dont want to set BSSID based Filter */
    pScanFilter->BSSIDs.numOfBSSIDs = 0;

    /* Populate all the information from the connected profile */
    pScanFilter->SSIDs.numOfSSIDs = 1;  
    pScanFilter->SSIDs.SSIDList = vos_mem_malloc(sizeof(tCsrSSIDInfo));
    if (NULL == pScanFilter->SSIDs.SSIDList)
    {
        smsLog(pMac, LOGE, FL("Scan Filter SSID mem alloc failed"));
        return eHAL_STATUS_FAILED_ALLOC;
    }
    pScanFilter->SSIDs.SSIDList->handoffPermitted = 1;
    pScanFilter->SSIDs.SSIDList->ssidHidden = 0;
    pScanFilter->SSIDs.SSIDList->SSID.length =  pCurProfile->SSID.length;
    vos_mem_copy((void *)pScanFilter->SSIDs.SSIDList->SSID.ssId, (void *)pCurProfile->SSID.ssId, pCurProfile->SSID.length); 

    NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Filtering for SSID %s from scan results.. SSID Length = %d\n"), 
                        pScanFilter->SSIDs.SSIDList->SSID.ssId, pScanFilter->SSIDs.SSIDList->SSID.length);
    pScanFilter->authType.numEntries = 1;
    pScanFilter->authType.authType[0] = pCurProfile->AuthType;

    pScanFilter->EncryptionType.numEntries = 1; //This must be 1
    pScanFilter->EncryptionType.encryptionType[0] = pCurProfile->EncryptionType;

    pScanFilter->mcEncryptionType.numEntries = 1;
    pScanFilter->mcEncryptionType.encryptionType[0] = pCurProfile->mcEncryptionType;

    pScanFilter->BSSType = pCurProfile->BSSType;

    /* We are intrested only in the scan results on channels that we scanned  */
    pScanFilter->ChannelInfo.numOfChannels = pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels;
    pScanFilter->ChannelInfo.ChannelList = vos_mem_malloc(pScanFilter->ChannelInfo.numOfChannels * sizeof(tANI_U8));
    if (NULL == pScanFilter->ChannelInfo.ChannelList)
    {
        smsLog(pMac, LOGE, FL("Scan Filter Channel list mem alloc failed"));
        vos_mem_free(pScanFilter->SSIDs.SSIDList);
        pScanFilter->SSIDs.SSIDList = NULL;
        return eHAL_STATUS_FAILED_ALLOC;
    }
    for (i = 0; i < pScanFilter->ChannelInfo.numOfChannels; i++)
    {
        pScanFilter->ChannelInfo.ChannelList[i] = pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[i];
    }

#ifdef WLAN_FEATURE_VOWIFI_11R
    if (pNeighborRoamInfo->is11rAssoc)
    {
        /* MDIE should be added as a part of profile. This should be added as a part of filter as well  */
        pScanFilter->MDID.mdiePresent = pCurProfile->MDID.mdiePresent;
        pScanFilter->MDID.mobilityDomain = pCurProfile->MDID.mobilityDomain;
    }
#endif

    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamProcessScanResults

    \brief  This function extracts scan results, sorts on the basis of neighbor score(todo). 
            Assumed that the results are already sorted by RSSI by csrScanGetResult

    \param  pMac - The handle returned by macOpen.
            pScanResultList - Scan result result obtained from csrScanGetResult()

    \return VOID

---------------------------------------------------------------------------*/

static void csrNeighborRoamProcessScanResults(tpAniSirGlobal pMac, tScanResultHandle *pScanResultList)
{
    tCsrScanResultInfo *pScanResult;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tpCsrNeighborRoamBSSInfo    pBssInfo;

    /* Expecting the scan result already to be in the sorted order based on the RSSI */
    /* Based on the previous state we need to check whether the list should be sorted again taking neighbor score into consideration */
    /* If previous state is CFG_CHAN_LIST_SCAN, there should not be any neighbor score associated with any of the BSS.
       If the previous state is REPORT_QUERY, then there will be neighbor score for each of the APs */
    /* For now, let us take the top of the list provided as it is by the CSR Scan result API. This means it is assumed that neighbor score 
       and rssi score are in the same order. This will be taken care later */

    while (NULL != (pScanResult = csrScanResultGetNext(pMac, *pScanResultList)))
    {
        NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Scan result: BSSID : %02x:%02x:%02x:%02x:%02x:%02x"), 
                        pScanResult->BssDescriptor.bssId[0],
                        pScanResult->BssDescriptor.bssId[1],
                        pScanResult->BssDescriptor.bssId[2],
                        pScanResult->BssDescriptor.bssId[3],
                        pScanResult->BssDescriptor.bssId[4],
                        pScanResult->BssDescriptor.bssId[5]);

        if (VOS_TRUE == vos_mem_compare(pScanResult->BssDescriptor.bssId, 
                       pNeighborRoamInfo->currAPbssid, sizeof(tSirMacAddr)))
        {
            //currently associated AP. Do not have this in the roamable AP list
            continue;
        }

        if (abs(pNeighborRoamInfo->cfgParams.neighborReassocThreshold) < abs(pScanResult->BssDescriptor.rssi))
        {
            VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO,
                  "%s: [INFOLOG]Current reassoc threshold %d new ap rssi worse=%d\n", __func__,
                      (int)pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1),
                      (int)pScanResult->BssDescriptor.rssi * (-1) );
            continue;
        }        

#ifdef WLAN_FEATURE_VOWIFI_11R
        if (pNeighborRoamInfo->is11rAssoc)
        {
            if (!csrNeighborRoamIsPreauthCandidate(pMac, pScanResult->BssDescriptor.bssId))
            {
                smsLog(pMac, LOGE, FL("BSSID present in pre-auth fail list.. Ignoring"));
                continue;
            }
        }
#endif /* WLAN_FEATURE_VOWIFI_11R */

#ifdef FEATURE_WLAN_CCX
        if (pNeighborRoamInfo->isCCXAssoc)
        {
            if (!csrNeighborRoamIsPreauthCandidate(pMac, pScanResult->BssDescriptor.bssId))
            {
                smsLog(pMac, LOGE, FL("BSSID present in pre-auth fail list.. Ignoring"));
                continue;
            }
        }
        if ((pScanResult->BssDescriptor.QBSSLoad_present) &&
             (pScanResult->BssDescriptor.QBSSLoad_avail))
        {
            if (pNeighborRoamInfo->isVOAdmitted)
            {
                smsLog(pMac, LOG1, FL("New AP has %x BW available\n"), (unsigned int)pScanResult->BssDescriptor.QBSSLoad_avail);
                smsLog(pMac, LOG1, FL("We need %x BW available\n"),(unsigned int)pNeighborRoamInfo->MinQBssLoadRequired);
                if (pScanResult->BssDescriptor.QBSSLoad_avail < pNeighborRoamInfo->MinQBssLoadRequired) 
                {
                    VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                        "[INFOLOG]BSSID : %02x:%02x:%02x:%02x:%02x:%02x has no bandwidth ignoring..not adding to roam list\n",
                        pScanResult->BssDescriptor.bssId[0],
                        pScanResult->BssDescriptor.bssId[1],
                        pScanResult->BssDescriptor.bssId[2],
                        pScanResult->BssDescriptor.bssId[3],
                        pScanResult->BssDescriptor.bssId[4],
                        pScanResult->BssDescriptor.bssId[5]);
                    continue;
                }
            }
        }
        else
        {
            smsLog(pMac, LOGE, FL("No QBss %x %x\n"), (unsigned int)pScanResult->BssDescriptor.QBSSLoad_avail, (unsigned int)pScanResult->BssDescriptor.QBSSLoad_present);
            if (pNeighborRoamInfo->isVOAdmitted)
            {
                VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                    "[INFOLOG]BSSID : %02x:%02x:%02x:%02x:%02x:%02x has no QBSSLoad IE, ignoring..not adding to roam list\n",
                    pScanResult->BssDescriptor.bssId[0],
                    pScanResult->BssDescriptor.bssId[1],
                    pScanResult->BssDescriptor.bssId[2],
                    pScanResult->BssDescriptor.bssId[3],
                    pScanResult->BssDescriptor.bssId[4],
                    pScanResult->BssDescriptor.bssId[5]);
                continue;
            }
        }
#endif /* FEATURE_WLAN_CCX */

        /* If the received timestamp in BSS description is earlier than the scan request timestamp, skip 
         * this result */
        if (pNeighborRoamInfo->scanRequestTimeStamp >= pScanResult->BssDescriptor.nReceivedTime)
        {
            smsLog(pMac, LOGE, FL("Ignoring BSS as it is older than the scan request timestamp"));
            continue;
        }

        pBssInfo = vos_mem_malloc(sizeof(tCsrNeighborRoamBSSInfo));
        if (NULL == pBssInfo)
        {
            smsLog(pMac, LOGE, FL("Memory allocation for Neighbor Roam BSS Info failed.. Just ignoring"));
            continue;
        }

        pBssInfo->pBssDescription = vos_mem_malloc(pScanResult->BssDescriptor.length + sizeof(pScanResult->BssDescriptor.length));
        if (pBssInfo->pBssDescription != NULL)
        {
            vos_mem_copy(pBssInfo->pBssDescription, &pScanResult->BssDescriptor, 
                    pScanResult->BssDescriptor.length + sizeof(pScanResult->BssDescriptor.length));
        }
        else
        {
            smsLog(pMac, LOGE, FL("Memory allocation for Neighbor Roam BSS Descriptor failed.. Just ignoring"));
            vos_mem_free(pBssInfo);
            continue;
            
        }
        pBssInfo->apPreferenceVal = 10; //some value for now. Need to calculate the actual score based on RSSI and neighbor AP score

        /* Just add to the end of the list as it is already sorted by RSSI */
        csrLLInsertTail(&pNeighborRoamInfo->roamableAPList, &pBssInfo->List, LL_ACCESS_LOCK);
    }

    /* Now we have all the scan results in our local list. Good time to free up the the list we got as a part of csrGetScanResult */
    csrScanResultPurge(pMac, *pScanResultList);

    return;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamHandleEmptyScanResult

    \brief      This function will be invoked in CFG_CHAN_LIST_SCAN state when 
                there are no valid APs in the scan result for roaming. This means 
                out AP is the best and no other AP is around. No point in scanning 
                again and again. Performing the following here.
                1. Deregister the pre-auth callback from TL
                2. Stop the neighbor scan timer
                3. Re-register the neighbor lookup callback with increased pre-auth threshold
                4. Transition the state to CONNECTED state

    \param  pMac - The handle returned by macOpen.

    \return VOS_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
static VOS_STATUS csrNeighborRoamHandleEmptyScanResult(tpAniSirGlobal pMac)
{
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    eHalStatus  status = eHAL_STATUS_SUCCESS;

    /* Stop the neighbor scan timer now */
    status = palTimerStop(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer);
    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGW, FL(" palTimerStop failed with status %d\n"), status);
    }

    /* Increase the neighbor lookup threshold by a constant factor or 1 */
    if ((pNeighborRoamInfo->currentNeighborLookupThreshold+3) < pNeighborRoamInfo->cfgParams.neighborReassocThreshold)
    {
        pNeighborRoamInfo->currentNeighborLookupThreshold += 3;
    }


#ifdef WLAN_FEATURE_VOWIFI_11R
    /* Clear off the old neighbor report details */
    vos_mem_zero(&pNeighborRoamInfo->FTRoamInfo.neighboReportBssInfo, sizeof(tCsrNeighborReportBssInfo) * MAX_BSS_IN_NEIGHBOR_RPT);
#endif

    /* Reset all the necessary variables before transitioning to the CONNECTED state */
    csrNeighborRoamResetConnectedStateControlInfo(pMac);

    /* Transition to CONNECTED state */
    CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_CONNECTED)
    /* Re-register Neighbor Lookup threshold callback with TL */
    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Registering DOWN event neighbor lookup callback with TL for RSSI = %d"), pNeighborRoamInfo->currentNeighborLookupThreshold * (-1)); 
    vosStatus = WLANTL_RegRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->currentNeighborLookupThreshold * (-1),
                                    WLANTL_HO_THRESHOLD_DOWN, 
                                    csrNeighborRoamNeighborLookupDOWNCallback, 
                                    VOS_MODULE_ID_SME, pMac);
    
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
       //err msg
       smsLog(pMac, LOGW, FL(" Couldn't re-register csrNeighborRoamNeighborLookupDOWNCallback with TL: Status = %d\n"), status);
    }
    return vosStatus;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamScanRequestCallback

    \brief  This function is the callback function registered in csrScanRequest() to 
            indicate the completion of scan. If scan is completed for all the channels in 
            the channel list, this function gets the scan result and starts the refresh results
            timer to avoid having stale results. If scan is not completed on all the channels,
            it restarts the neighbor scan timer which on expiry issues scan on the next 
            channel

    \param  halHandle - The handle returned by macOpen.
            pContext - not used
            scanId - not used
            status - not used

    \return eHAL_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
static eHalStatus csrNeighborRoamScanRequestCallback(tHalHandle halHandle, void *pContext,
                         tANI_U32 scanId, eCsrScanStatus status)
{
    tpAniSirGlobal                  pMac = (tpAniSirGlobal) halHandle;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tANI_U8                         currentChanIndex;
    tCsrScanResultFilter    scanFilter;
    tScanResultHandle       scanResult;
    tANI_U32                tempVal = 0;
    eHalStatus              hstatus;

    pMac->roam.neighborRoamInfo.scanRspPending = eANI_BOOLEAN_FALSE;
    
    /* This can happen when we receive a UP event from TL in any of the scan states. Silently ignore it */
    if (eCSR_NEIGHBOR_ROAM_STATE_CONNECTED == pNeighborRoamInfo->neighborRoamState)
    {
        smsLog(pMac, LOGE, FL("Received in CONNECTED state. Must be because a UP event from TL after issuing scan request. Ignore it"));
        return eHAL_STATUS_SUCCESS;
    }

    /* -1 is done because the chanIndex would have got incremented after issuing a successful scan request */
    currentChanIndex = (pMac->roam.neighborRoamInfo.roamChannelInfo.currentChanIndex) ? (pMac->roam.neighborRoamInfo.roamChannelInfo.currentChanIndex - 1) : 0;

    /* Validate inputs */
    if (pMac->roam.neighborRoamInfo.roamChannelInfo.currentChannelListInfo.ChannelList) { 
        NEIGHBOR_ROAM_DEBUG(pMac, LOGW, FL("csrNeighborRoamScanRequestCallback received for Channel = %d, ChanIndex = %d"), 
                    pMac->roam.neighborRoamInfo.roamChannelInfo.currentChannelListInfo.ChannelList[currentChanIndex], currentChanIndex);
    }
    else
    {
        smsLog(pMac, LOG1, FL("Received during clean-up. Silently ignore scan completion event."));
        return eHAL_STATUS_SUCCESS;
    }

    if (eANI_BOOLEAN_FALSE == pNeighborRoamInfo->roamChannelInfo.chanListScanInProgress)
    {
        /* Scan is completed in the  CFG_CHAN_SCAN state. We can transition to REPORT_SCAN state 
           just to get the results and perform PREAUTH */
        /* Now we have completed scanning the channel list. We have get the result by applying appropriate filter
           sort the results based on neighborScore and RSSI and select the best candidate out of the list */
        NEIGHBOR_ROAM_DEBUG(pMac, LOGW, FL("Channel list scan completed. Current chan index = %d"), currentChanIndex);
        VOS_ASSERT(pNeighborRoamInfo->roamChannelInfo.currentChanIndex == 0);

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX)
        /* If the state is REPORT_SCAN, then this must be the scan after the REPORT_QUERY state. So, we 
           should use the BSSID filter made out of neighbor reports */
        if (eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN == pNeighborRoamInfo->neighborRoamState)
        {
            hstatus = csrNeighborRoamBssIdScanFilter(pMac, &scanFilter);
            NEIGHBOR_ROAM_DEBUG(pMac, LOGW, FL("11R or CCX Association: Prepare scan filter status  with neighbor AP = %d"), hstatus);
            tempVal = 1;
        }
        else
#endif
        {
            hstatus = csrNeighborRoamPrepareScanProfileFilter(pMac, &scanFilter);
            NEIGHBOR_ROAM_DEBUG(pMac, LOGW, FL("11R/CCX/Other Association: Prepare scan to find neighbor AP filter status  = %d"), hstatus);
        }
        if (eHAL_STATUS_SUCCESS != hstatus)
        {
            smsLog(pMac, LOGE, FL("Scan Filter preparation failed for Assoc type %d.. Bailing out.."), tempVal);
            return eHAL_STATUS_FAILURE;
        }
        hstatus = csrScanGetResult(pMac, &scanFilter, &scanResult);
        NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Get Scan Result status code %d"), hstatus);
        /* Process the scan results and update roamable AP list */
        csrNeighborRoamProcessScanResults(pMac, &scanResult);

        /* Free the scan filter */
        csrFreeScanFilter(pMac, &scanFilter);

        tempVal = csrLLCount(&pNeighborRoamInfo->roamableAPList);

        switch(pNeighborRoamInfo->neighborRoamState)
        {
            case eCSR_NEIGHBOR_ROAM_STATE_CFG_CHAN_LIST_SCAN:
                if (tempVal)
                {
#ifdef WLAN_FEATURE_VOWIFI_11R
                    /* If this is a non-11r association, then we can register the reassoc callback here as we have some 
                                        APs in the roamable AP list */
                    if (pNeighborRoamInfo->is11rAssoc)
                    {
                        /* Valid APs are found after scan. Now we can initiate pre-authentication */
                        CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN)
                    }
                    else
#endif
#ifdef FEATURE_WLAN_CCX
                    /* If this is a non-11r association, then we can register the reassoc callback here as we have some 
                                        APs in the roamable AP list */
                    if (pNeighborRoamInfo->isCCXAssoc)
                    {
                        /* Valid APs are found after scan. Now we can initiate pre-authentication */
                        CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN)
                    }
                    else
#endif
                    {
                       
                        NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Completed scanning of CFG CHAN LIST in non-11r association. Registering reassoc callback"));
                        /* Nothing much to do now. Will continue to remain in this state in case of non-11r association */
                        /* Stop the timer. But how long the roamable AP list will be valid in here. At some point of time, we 
                           need to restart the CFG CHAN list scan procedure if reassoc callback is not invoked from TL 
                           within certain duration */
                        
//                        palTimerStop(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer);
                    }
                }
                else
                {
                    NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("No candidate found after scanning in state %d.. "), pNeighborRoamInfo->neighborRoamState);
                    /* Handle it appropriately */
                    csrNeighborRoamHandleEmptyScanResult(pMac);
                }
                break;
#ifdef WLAN_FEATURE_VOWIFI_11R                
            case eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN:
                if (!tempVal)
                {
                    smsLog(pMac, LOGE, FL("No candidate found after scanning in state %d.. "), pNeighborRoamInfo->neighborRoamState);
                    /* Stop the timer here as the same timer will be started again in CFG_CHAN_SCAN_STATE */
                    csrNeighborRoamTransitToCFGChanScan(pMac);
                }
                break;
#endif /* WLAN_FEATURE_VOWIFI_11R */
            default:
                // Can come only in INIT state. Where in we are associated, we sent scan and user
                // in the meantime decides to disassoc, we will be in init state and still received call
                // back issued. Should not come here in any other state, printing just in case
                VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                        "%s: [INFOLOG] State %d\n", __func__, (pNeighborRoamInfo->neighborRoamState));

                // Lets just exit out silently.
                return eHAL_STATUS_SUCCESS;
        }

        if (tempVal)
        {
            VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;

           /* This timer should be started before registering the Reassoc callback with TL. This is because, it is very likely 
            * that the callback getting called immediately and the timer would never be stopped when pre-auth is in progress */
           if (eHAL_STATUS_SUCCESS != palTimerStart(pMac->hHdd, pNeighborRoamInfo->neighborResultsRefreshTimer, 
                        pNeighborRoamInfo->cfgParams.neighborResultsRefreshPeriod * PAL_TIMER_TO_MS_UNIT, 
                        eANI_BOOLEAN_FALSE))
            {
                smsLog(pMac, LOGE, FL("Neighbor results refresh timer failed to start, status = %d"), status);
                vos_mem_free(pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList);
                pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL;
                return eHAL_STATUS_FAILURE;
            }

            NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Registering DOWN event Reassoc callback with TL. RSSI = %d"), pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1));
            /* Register a reassoc Indication callback */
            vosStatus = WLANTL_RegRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1),
                                            WLANTL_HO_THRESHOLD_DOWN, 
                                            csrNeighborRoamReassocIndCallback,
                                            VOS_MODULE_ID_SME, pMac);
            
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
               //err msg
               smsLog(pMac, LOGW, FL(" Couldn't register csrNeighborRoamReassocIndCallback with TL: Status = %d\n"), vosStatus);
            }
 
        }
    }
    else
    {

        /* Restart the timer for the next scan sequence as scanning is not over */
        hstatus = palTimerStart(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer, 
                    pNeighborRoamInfo->cfgParams.neighborScanPeriod * PAL_TIMER_TO_MS_UNIT, 
                    eANI_BOOLEAN_FALSE);
    
        if (eHAL_STATUS_SUCCESS != hstatus)
        {
            /* Timer start failed.. Should we ASSERT here??? */
            smsLog(pMac, LOGE, FL("Neighbor scan PAL Timer start failed, status = %d, Ignoring state transition"), status);
            vos_mem_free(pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList);
            pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL;
            return eHAL_STATUS_FAILURE;
        }
    }
    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamIssueBgScanRequest

    \brief  This function issues CSR scan request after populating all the BG scan params 
            passed

    \param  pMac - The handle returned by macOpen.
            pBgScanParams - Params that need to be populated into csr Scan request

    \return eHAL_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
eHalStatus csrNeighborRoamIssueBgScanRequest(tpAniSirGlobal pMac, tCsrBGScanRequest *pBgScanParams)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tANI_U32 scanId;
    tCsrScanRequest scanReq;
    tANI_U8 channel;
    
    NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("csrNeighborRoamIssueBgScanRequest for Channel = %d, ChanIndex = %d"), 
                    pBgScanParams->ChannelInfo.ChannelList[0], pMac->roam.neighborRoamInfo.roamChannelInfo.currentChanIndex);


    //send down the scan req for 1 channel on the associated SSID
    palZeroMemory(pMac->hHdd, &scanReq, sizeof(tCsrScanRequest));
    /* Fill in the SSID Info */
    scanReq.SSIDs.numOfSSIDs = 1;
    scanReq.SSIDs.SSIDList = vos_mem_malloc(sizeof(tCsrSSIDInfo) * scanReq.SSIDs.numOfSSIDs);
    if(NULL == scanReq.SSIDs.SSIDList)
    {
       //err msg
       smsLog(pMac, LOGW, FL("Couldn't allocate memory for the SSID..Freeing memory allocated for Channel List\n"));
       return eHAL_STATUS_FAILURE;
    }
    vos_mem_zero(scanReq.SSIDs.SSIDList, sizeof(tCsrSSIDInfo) * scanReq.SSIDs.numOfSSIDs);

    scanReq.SSIDs.SSIDList[0].handoffPermitted = eANI_BOOLEAN_TRUE;
    scanReq.SSIDs.SSIDList[0].ssidHidden = eANI_BOOLEAN_TRUE;
    vos_mem_copy((void *)&scanReq.SSIDs.SSIDList[0].SSID, (void *)&pBgScanParams->SSID, sizeof(pBgScanParams->SSID));
    
    scanReq.ChannelInfo.numOfChannels = pBgScanParams->ChannelInfo.numOfChannels;
    
    channel = pBgScanParams->ChannelInfo.ChannelList[0];
    scanReq.ChannelInfo.ChannelList = &channel;    

    scanReq.BSSType = eCSR_BSS_TYPE_INFRASTRUCTURE;
    scanReq.scanType = eSIR_ACTIVE_SCAN;
    scanReq.requestType = eCSR_SCAN_HO_BG_SCAN;
    scanReq.maxChnTime = pBgScanParams->maxChnTime;
    scanReq.minChnTime = pBgScanParams->minChnTime;
    status = csrScanRequest(pMac, CSR_SESSION_ID_INVALID, &scanReq,
                        &scanId, csrNeighborRoamScanRequestCallback, NULL);
    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("CSR Scan Request failed with status %d"), status);
        vos_mem_free(scanReq.SSIDs.SSIDList);
        return status;
    }
    pMac->roam.neighborRoamInfo.scanRspPending = eANI_BOOLEAN_TRUE;

    vos_mem_free(scanReq.SSIDs.SSIDList);
    NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Channel List Address = %08x, Actual index = %d"), 
                    &pMac->roam.neighborRoamInfo.roamChannelInfo.currentChannelListInfo.ChannelList[0], 
                    pMac->roam.neighborRoamInfo.roamChannelInfo.currentChanIndex);
    return status;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamPerformBgScan

    \brief  This function is invoked on every expiry of neighborScanTimer till all 
            the channels in the channel list are scanned. It populates necessary 
            parameters for BG scan and calls appropriate AP to invoke the CSR scan 
            request

    \param  pMac - The handle returned by macOpen.

    \return eHAL_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
eHalStatus csrNeighborRoamPerformBgScan(tpAniSirGlobal pMac)
{
    eHalStatus      status = eHAL_STATUS_SUCCESS;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tCsrBGScanRequest   bgScanParams;
    tANI_U8             broadcastBssid[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    tANI_U8             channel = 0;

    if (pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList)
    {
        NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Channel List Address = %08x"), &pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[0]);
    }
    else 
    {
        NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Channel List Empty"));
        // Go back and restart. Mostly timer start failure has occured.
        // When timer start is declared a failure, then we delete the list.
        // Should not happen now as we stop and then only start the scan timer. 
        // still handle the unlikely case.
        csrNeighborRoamHandleEmptyScanResult(pMac);
        return status;
    }
    /* Need to perform scan here before getting the list */
    vos_mem_copy(bgScanParams.bssid, broadcastBssid, sizeof(tCsrBssid));
    bgScanParams.SSID.length = pMac->roam.roamSession[pNeighborRoamInfo->csrSessionId].connectedProfile.SSID.length;
    vos_mem_copy(bgScanParams.SSID.ssId, pMac->roam.roamSession[pNeighborRoamInfo->csrSessionId].connectedProfile.SSID.ssId, 
                                    pMac->roam.roamSession[pNeighborRoamInfo->csrSessionId].connectedProfile.SSID.length);

    channel = pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[pNeighborRoamInfo->roamChannelInfo.currentChanIndex];
    bgScanParams.ChannelInfo.numOfChannels = 1;
    bgScanParams.ChannelInfo.ChannelList = &channel;
   
    bgScanParams.minChnTime = pNeighborRoamInfo->cfgParams.minChannelScanTime;
    bgScanParams.maxChnTime = pNeighborRoamInfo->cfgParams.maxChannelScanTime;

    status = csrNeighborRoamIssueBgScanRequest(pMac, &bgScanParams);
    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("Issue of BG Scan request failed: Status = %d"), status);
        return status;
    }

    pNeighborRoamInfo->roamChannelInfo.currentChanIndex++;
    if (pNeighborRoamInfo->roamChannelInfo.currentChanIndex >= 
            pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels)
    {      
        NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Completed scanning channels in Channel List: CurrChanIndex = %d, Num Channels = %d"),
                                            pNeighborRoamInfo->roamChannelInfo.currentChanIndex, 
                                            pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels);
        /* We have completed scanning all the channels */
        pNeighborRoamInfo->roamChannelInfo.currentChanIndex = 0;
        /* We are no longer scanning the channel list. Next timer firing should be used to get the scan results 
           and select the best AP in the list */
        if (eANI_BOOLEAN_TRUE == pNeighborRoamInfo->roamChannelInfo.chanListScanInProgress)
        {
            pNeighborRoamInfo->roamChannelInfo.chanListScanInProgress = eANI_BOOLEAN_FALSE;
        }
    }

    return status;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamNeighborScanTimerCallback

    \brief  This function is the neighbor scan timer callback function. It invokes 
            the BG scan request based on the current and previous states

    \param  pv - CSR timer context info which includes pMac and session ID

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamNeighborScanTimerCallback(void *pv)
{
    tCsrTimerInfo *pInfo = (tCsrTimerInfo *)pv;
    tpAniSirGlobal pMac = pInfo->pMac;
    tANI_U32         sessionId = pInfo->sessionId;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;

    // check if bg scan is on going, no need to send down the new params if true
    if(eANI_BOOLEAN_TRUE == pNeighborRoamInfo->scanRspPending)
    {
       //msg
       smsLog(pMac, LOGW, FL("Already BgScanRsp is Pending\n"));
       return;
    }

    VOS_ASSERT(sessionId == pNeighborRoamInfo->csrSessionId);

    switch (pNeighborRoamInfo->neighborRoamState)
    {
#ifdef WLAN_FEATURE_VOWIFI_11R
        case eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN:
            switch(pNeighborRoamInfo->prevNeighborRoamState)
            {
                case eCSR_NEIGHBOR_ROAM_STATE_REPORT_QUERY:
                    csrNeighborRoamPerformBgScan(pMac);
                    break;
                default:
                    smsLog(pMac, LOGE, FL("Neighbor scan callback received in state %d, prev state = %d"), 
                                    pNeighborRoamInfo->neighborRoamState, pNeighborRoamInfo->prevNeighborRoamState);
                    break;
            }
            break;
#endif /* WLAN_FEATURE_VOWIFI_11R */
        case eCSR_NEIGHBOR_ROAM_STATE_CFG_CHAN_LIST_SCAN:     
            csrNeighborRoamPerformBgScan(pMac);
            break;
        default:
            break;
    }
    return;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamResultsRefreshTimerCallback

    \brief  This function is the timer callback function for results refresh timer.
            When this is invoked, it is as good as down event received from TL. So, 
            clear off the roamable AP list and start the scan procedure based on 11R 
            or non-11R association

    \param  context - CSR timer context info which includes pMac and session ID

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamResultsRefreshTimerCallback(void *context)
{
    tCsrTimerInfo *pInfo = (tCsrTimerInfo *)context;
    tpAniSirGlobal pMac = pInfo->pMac;
    VOS_STATUS     vosStatus = VOS_STATUS_SUCCESS;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
     
    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Deregistering DOWN event reassoc callback with TL. RSSI = %d"), pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1));

    /* Deregister reassoc callback. Ignore return status */
    vosStatus = WLANTL_DeregRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1),
                                                        WLANTL_HO_THRESHOLD_DOWN, 
                                                        csrNeighborRoamReassocIndCallback,
                                                        VOS_MODULE_ID_SME);
                        
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        //err msg
        smsLog(pMac, LOGW, FL(" Couldn't deregister csrNeighborRoamReassocIndCallback with TL: Status = %d\n"), vosStatus);
    }

    /* Reset all the variables just as no scan had happened before */
    csrNeighborRoamResetConnectedStateControlInfo(pMac);

#if defined WLAN_FEATURE_VOWIFI_11R && defined WLAN_FEATURE_VOWIFI
    if ((pNeighborRoamInfo->is11rAssoc) && (pMac->rrm.rrmSmeContext.rrmConfig.rrmEnabled))
    {
        NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("11R Association:Neighbor Lookup Down event received in CONNECTED state"));
        vosStatus = csrNeighborRoamIssueNeighborRptRequest(pMac);
        if (VOS_STATUS_SUCCESS != vosStatus)
        {
            smsLog(pMac, LOGE, FL("Neighbor report request failed. status = %d\n"), vosStatus);
            return;
        }
        /* Increment the neighbor report retry count after sending the neighbor request successfully */
        pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum++;
        pNeighborRoamInfo->FTRoamInfo.neighborRptPending = eANI_BOOLEAN_TRUE;
        CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_REPORT_QUERY)
    }
    else
#endif      
    {
        NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Non 11R or CCX Association:Neighbor Lookup Down event received in CONNECTED state"));
        vosStatus = csrNeighborRoamTransitToCFGChanScan(pMac);
        if (VOS_STATUS_SUCCESS != vosStatus)
        {
            return;
        }
    }
    return;
}

#if defined WLAN_FEATURE_VOWIFI_11R && defined WLAN_FEATURE_VOWIFI
/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamIssueNeighborRptRequest

    \brief  This function is invoked when TL issues a down event and the current assoc 
            is a 11R association. It invokes SME RRM API to issue the neighbor request to 
            the currently associated AP with the current SSID

    \param  pMac - The handle returned by macOpen.

    \return VOS_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
VOS_STATUS csrNeighborRoamIssueNeighborRptRequest(tpAniSirGlobal pMac)
{
    tRrmNeighborRspCallbackInfo callbackInfo;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tRrmNeighborReq neighborReq;


    neighborReq.no_ssid = 0;

    /* Fill in the SSID */
    neighborReq.ssid.length = pMac->roam.roamSession[pNeighborRoamInfo->csrSessionId].connectedProfile.SSID.length;
    vos_mem_copy(neighborReq.ssid.ssId, pMac->roam.roamSession[pNeighborRoamInfo->csrSessionId].connectedProfile.SSID.ssId, 
                                    pMac->roam.roamSession[pNeighborRoamInfo->csrSessionId].connectedProfile.SSID.length);
    
    callbackInfo.neighborRspCallback = csrNeighborRoamRRMNeighborReportResult;
    callbackInfo.neighborRspCallbackContext = pMac;
    callbackInfo.timeout = pNeighborRoamInfo->FTRoamInfo.neighborReportTimeout;

    return sme_NeighborReportRequest(pMac,(tANI_U8) pNeighborRoamInfo->csrSessionId, &neighborReq, &callbackInfo);
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamCreateChanListFromNeighborReport

    \brief  This function is invoked when neighbor report is received for the 
            neighbor request. Based on the channels present in the neighbor report, 
            it generates channel list which will be used in REPORT_SCAN state to
            scan for these neighbor APs

    \param  pMac - The handle returned by macOpen.

    \return VOS_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
VOS_STATUS csrNeighborRoamCreateChanListFromNeighborReport(tpAniSirGlobal pMac)
{
    tpRrmNeighborReportDesc pNeighborBssDesc;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tANI_U8         numChannels = 0, i = 0, j=0;
    tANI_U8         channelList[MAX_BSS_IN_NEIGHBOR_RPT];

    /* This should always start from 0 whenever we create a channel list out of neighbor AP list */
    pNeighborRoamInfo->FTRoamInfo.numBssFromNeighborReport = 0;

    pNeighborBssDesc = smeRrmGetFirstBssEntryFromNeighborCache(pMac);

    while (pNeighborBssDesc)
    {
        if (pNeighborRoamInfo->FTRoamInfo.numBssFromNeighborReport >= MAX_BSS_IN_NEIGHBOR_RPT) break;
        
        /* Update the neighbor BSS Info in the 11r FT Roam Info */
        pNeighborRoamInfo->FTRoamInfo.neighboReportBssInfo[pNeighborRoamInfo->FTRoamInfo.numBssFromNeighborReport].channelNum = 
                                    pNeighborBssDesc->pNeighborBssDescription->channel;
        pNeighborRoamInfo->FTRoamInfo.neighboReportBssInfo[pNeighborRoamInfo->FTRoamInfo.numBssFromNeighborReport].neighborScore = 
                                    (tANI_U8)pNeighborBssDesc->roamScore;
        vos_mem_copy(pNeighborRoamInfo->FTRoamInfo.neighboReportBssInfo[pNeighborRoamInfo->FTRoamInfo.numBssFromNeighborReport].neighborBssId,
                                     pNeighborBssDesc->pNeighborBssDescription->bssId, sizeof(tSirMacAddr));
        pNeighborRoamInfo->FTRoamInfo.numBssFromNeighborReport++;

        /* Saving the channel list non-redundantly */
        if (numChannels > 0)
        {
            for (i = 0; i < numChannels; i++)
            {
                if (pNeighborBssDesc->pNeighborBssDescription->channel == channelList[i])
                    break;
            }
            
        }
        if (i == numChannels)
        {
            if (pNeighborBssDesc->pNeighborBssDescription->channel)
            {
                // Make sure to add only if its the same band
                if ((pNeighborRoamInfo->currAPoperationChannel <= (RF_CHAN_14+1)) &&
                    (pNeighborBssDesc->pNeighborBssDescription->channel <= (RF_CHAN_14+1)))
                {
                        VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                                "%s: [INFOLOG] Adding %d to Neighbor channel list\n", __func__,
                                pNeighborBssDesc->pNeighborBssDescription->channel);
                        channelList[numChannels] = pNeighborBssDesc->pNeighborBssDescription->channel;
                        numChannels++;
                }
                else if ((pNeighborRoamInfo->currAPoperationChannel >= RF_CHAN_128) &&
                    (pNeighborBssDesc->pNeighborBssDescription->channel >= RF_CHAN_128))
                {
                        VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                                "%s: [INFOLOG] Adding %d to Neighbor channel list\n", __func__,
                                pNeighborBssDesc->pNeighborBssDescription->channel);
                        channelList[numChannels] = pNeighborBssDesc->pNeighborBssDescription->channel;
                        numChannels++;
                }
            }
        }
            
        pNeighborBssDesc = smeRrmGetNextBssEntryFromNeighborCache(pMac, pNeighborBssDesc);
    }

    if (pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList)
    {
        // Before we free the existing channel list for a safety net make sure
        // we have a union of the IAPP and the already existing list. 
        for (i = 0; i < pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels; i++)
        {
            for (j = 0; j < numChannels; j++)
            {
                if (pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[i] == channelList[j])
                    break;
            }
            if (j == numChannels)
            {
                if (pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[i])
                {
                    // Make sure to add only if its the same band
                    if ((pNeighborRoamInfo->currAPoperationChannel <= (RF_CHAN_14+1)) &&
                        (pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[i] <= (RF_CHAN_14+1)))
                    {
                            VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                                    "%s: [INFOLOG] Adding extra %d to Neighbor channel list\n", __func__,
                            pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[i]);
                            channelList[numChannels] = 
                            pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[i];
                            numChannels++;
                    }
                    if ((pNeighborRoamInfo->currAPoperationChannel >= RF_CHAN_128) &&
                         (pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[i] >= RF_CHAN_128))
                    {
                            VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, 
                                    "%s: [INFOLOG] Adding extra %d to Neighbor channel list\n", __func__,
                                pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[i]);
                            channelList[numChannels] = 
                                pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[i];
                            numChannels++;
                    }
                }
            }
        }
        vos_mem_free(pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList);
    }

    pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL;
    /* Store the obtained channel list to the Neighbor Control data structure */
    if (numChannels)
        pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = vos_mem_malloc((numChannels) * sizeof(tANI_U8));
    if (NULL == pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList)
    {
        smsLog(pMac, LOGE, FL("Memory allocation for Channel list failed.. TL event ignored"));
        return VOS_STATUS_E_RESOURCES;
    }

    vos_mem_copy(pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList, 
                                            channelList, (numChannels) * sizeof(tANI_U8));
    pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels = numChannels;
    if (numChannels)
    {
        smsLog(pMac, LOG1, FL("IAPP Neighbor list callback received as expected in state %d."), 
            pNeighborRoamInfo->neighborRoamState);
        pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived = eANI_BOOLEAN_TRUE;
    }
    pNeighborRoamInfo->roamChannelInfo.currentChanIndex = 0;
    pNeighborRoamInfo->roamChannelInfo.chanListScanInProgress = eANI_BOOLEAN_TRUE;
    
    return VOS_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamRRMNeighborReportResult

    \brief  This function is the neighbor report callback that will be invoked by 
            SME RRM on receiving a neighbor report or of neighbor report is not 
            received after timeout. On receiving a valid report, it generates a 
            channel list from the neighbor report and starts the 
            neighbor scan timer

    \param  context - The handle returned by macOpen.
            vosStatus - Status of the callback(SUCCESS/FAILURE)

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamRRMNeighborReportResult(void *context, VOS_STATUS vosStatus)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(context);
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    eHalStatus  status = eHAL_STATUS_SUCCESS;

    smsLog(pMac, LOG1, FL("Neighbor report result callback with status = %d\n"), vosStatus);
    switch (pNeighborRoamInfo->neighborRoamState)
    {
        case eCSR_NEIGHBOR_ROAM_STATE_REPORT_QUERY:
            /* Reset the report pending variable */
            pNeighborRoamInfo->FTRoamInfo.neighborRptPending = eANI_BOOLEAN_FALSE;
            if (VOS_STATUS_SUCCESS == vosStatus)
            {
                /* Need to create channel list based on the neighbor AP list and transition to REPORT_SCAN state */
                vosStatus = csrNeighborRoamCreateChanListFromNeighborReport(pMac);
                if (VOS_STATUS_SUCCESS == vosStatus)
                {
                    NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Channel List created from Neighbor report, Transitioning to NEIGHBOR_SCAN state\n")); 
                }

                /* We are gonna scan now. Remember the time stamp to filter out results only after this timestamp */
                pNeighborRoamInfo->scanRequestTimeStamp = (tANI_TIMESTAMP)palGetTickCount(pMac->hHdd);
                
                /* Now ready for neighbor scan based on the channel list created */
                /* Start Neighbor scan timer now. Multiplication by PAL_TIMER_TO_MS_UNIT is to convert ms to us which is 
                   what palTimerStart expects */
                status = palTimerStart(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer, 
                                pNeighborRoamInfo->cfgParams.neighborScanPeriod * PAL_TIMER_TO_MS_UNIT, 
                                eANI_BOOLEAN_FALSE);
                if (eHAL_STATUS_SUCCESS != status)
                {
                    /* Timer start failed.. Should we ASSERT here??? */
                    smsLog(pMac, LOGE, FL("PAL Timer start for neighbor scan timer failed, status = %d, Ignoring state transition"), status);
                    vos_mem_free(pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList);
                    pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL;
                    return;
                }
                pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum = 0;                
                /* Neighbor scan timer started. Transition to REPORT_SCAN state */
                CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_REPORT_SCAN)
            }
            else
            {
                /* Neighbor report timeout happened in SME RRM. We can try sending more neighbor requests until we 
                                reach the maxNeighborRetries or receiving a successful neighbor response */
                smsLog(pMac, LOGE, FL("Neighbor report result failed after %d retries, MAX RETRIES = %d\n"), 
                     pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum, pNeighborRoamInfo->cfgParams.maxNeighborRetries);
                if (pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum >= 
                        pNeighborRoamInfo->cfgParams.maxNeighborRetries)
                {
                    smsLog(pMac, LOGE, FL("Bailing out to CFG Channel list scan.. \n"));
                    vosStatus = csrNeighborRoamTransitToCFGChanScan(pMac);
                    if (VOS_STATUS_SUCCESS != vosStatus)
                    {
                        smsLog(pMac, LOGE, FL("Transit to CFG Channel list scan state failed with status %d \n"), vosStatus);
                        return;
                    }
                    /* We transitioned to different state now. Reset the Neighbor report retry count */
                    pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum = 0;
                }
                else
                {
                    vosStatus = csrNeighborRoamIssueNeighborRptRequest(pMac);
                    if (VOS_STATUS_SUCCESS != vosStatus)
                    {
                        smsLog(pMac, LOGE, FL("Neighbor report request failed. status = %d\n"), vosStatus);
                        return;
                    }
                    pNeighborRoamInfo->FTRoamInfo.neighborRptPending = eANI_BOOLEAN_TRUE;
                    /* Increment the neighbor report retry count after sending the neighbor request successfully */
                    pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum++;
                }
            }
            break;
        default:
            smsLog(pMac, LOGE, FL("Neighbor result callback not expected in state %d, Ignoring.."), pNeighborRoamInfo->neighborRoamState);
            break;
    }
    return;
}
#endif /* WLAN_FEATURE_VOWIFI_11R */


/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamTransitToCFGChanScan

    \brief  This function is called whenever there is a transition to CFG chan scan 
            state from any state. It frees up the current channel list and allocates 
            a new memory for the channels received from CFG item. It then starts the 
            neighbor scan timer to perform the scan on each channel one by one

    \param  pMac - The handle returned by macOpen.

    \return VOS_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
VOS_STATUS csrNeighborRoamTransitToCFGChanScan(tpAniSirGlobal pMac)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    eHalStatus  status  = eHAL_STATUS_SUCCESS;
    int i = 0;
    int numOfChannels = 0;
    tANI_U8   channelList[MAX_BSS_IN_NEIGHBOR_RPT];

    if (
#ifdef FEATURE_WLAN_CCX
        ((pNeighborRoamInfo->isCCXAssoc) && 
        (pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived == eANI_BOOLEAN_FALSE)) ||
        (pNeighborRoamInfo->isCCXAssoc == eANI_BOOLEAN_FALSE) || 
#endif // CCX
        pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels == 0)

    {
        smsLog(pMac, LOGW, FL("Falling back to CFG channel list"));


        /* Free up the channel list and allocate a new memory. This is because we dont know how much 
            was allocated last time. If we directly copy more number of bytes than allocated earlier, this might 
            result in memory corruption */
        if (NULL != pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList)
        {
            vos_mem_free(pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList);
            pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL;
        }
        // Find the right subset of the cfg list based on the current band we are on.
        for (i = 0; i < pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels; i++)
        {
            if (pNeighborRoamInfo->cfgParams.channelInfo.ChannelList[i])
            {
                // Make sure to add only if its the same band
                if ((pNeighborRoamInfo->currAPoperationChannel <= (RF_CHAN_14+1)) &&
                    (pNeighborRoamInfo->cfgParams.channelInfo.ChannelList[i] <= (RF_CHAN_14+1)))
                {
                        VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                                "%s: [INFOLOG] Adding %d to Neighbor channel list\n", __func__,
                                pNeighborRoamInfo->cfgParams.channelInfo.ChannelList[i]);
                        channelList[numOfChannels] = pNeighborRoamInfo->cfgParams.channelInfo.ChannelList[i];
                        numOfChannels++;
                }
                if ((pNeighborRoamInfo->currAPoperationChannel >= RF_CHAN_128) &&
                    (pNeighborRoamInfo->cfgParams.channelInfo.ChannelList[i] >= RF_CHAN_128))
                {
                        VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR, 
                                "%s: [INFOLOG] Adding %d to Neighbor channel list\n", __func__,
                                pNeighborRoamInfo->cfgParams.channelInfo.ChannelList[i]);
                        channelList[numOfChannels] = pNeighborRoamInfo->cfgParams.channelInfo.ChannelList[i];
                        numOfChannels++;
                }
            }
        }

        pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels = numOfChannels;
        pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL; 
        if (numOfChannels)
            pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = vos_mem_malloc(numOfChannels);
    
        if (NULL == pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList)
        {
            smsLog(pMac, LOGE, FL("Memory allocation for Channel list failed.. TL event ignored"));
            return VOS_STATUS_E_RESOURCES;
        }
    
        /* Since this is a legacy case, copy the channel list from CFG here */
        vos_mem_copy(pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList, 
                                channelList, numOfChannels * sizeof(tANI_U8));

        for (i = 0; i < pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels; i++)
        {
            NEIGHBOR_ROAM_DEBUG(pMac, LOGE, "Channel List from CFG = %d\n", 
                pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList[i]);
        }
    }

    /* We are gonna scan now. Remember the time stamp to filter out results only after this timestamp */
    pNeighborRoamInfo->scanRequestTimeStamp = (tANI_TIMESTAMP)palGetTickCount(pMac->hHdd);
    
    palTimerStop(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer);
    /* Start Neighbor scan timer now. Multiplication by PAL_TIMER_TO_MS_UNIT is to convert ms to us which is 
            what palTimerStart expects */
    status = palTimerStart(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer, 
                    pNeighborRoamInfo->cfgParams.neighborScanPeriod * PAL_TIMER_TO_MS_UNIT, 
                    eANI_BOOLEAN_FALSE);
    
    if (eHAL_STATUS_SUCCESS != status)
    {
        /* Timer start failed..  */
        smsLog(pMac, LOGE, FL("Neighbor scan PAL Timer start failed, status = %d, Ignoring state transition"), status);
        vos_mem_free(pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList);
        pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL;
        return VOS_STATUS_E_FAILURE;
    }
    
    pNeighborRoamInfo->roamChannelInfo.currentChanIndex = 0;
    pNeighborRoamInfo->roamChannelInfo.chanListScanInProgress = eANI_BOOLEAN_TRUE;
    
    /* Transition to CFG_CHAN_LIST_SCAN_STATE */
    CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_CFG_CHAN_LIST_SCAN)

    return VOS_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamNeighborLookupUpEvent

    \brief  This function is called as soon as TL indicates that the current AP's 
            RSSI is better than the neighbor lookup threshold. Here, we transition to 
            CONNECTED state and reset all the scan parameters

    \param  pMac - The handle returned by macOpen.

    \return VOS_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
VOS_STATUS  csrNeighborRoamNeighborLookupUpEvent(tpAniSirGlobal pMac)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    VOS_STATUS  vosStatus;

    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Deregistering UP event neighbor lookup callback with TL. RSSI = %d,"), pNeighborRoamInfo->cfgParams.neighborLookupThreshold * (-1));
    /* Deregister the UP event now */
    vosStatus = WLANTL_DeregRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->cfgParams.neighborLookupThreshold * (-1),
                                    WLANTL_HO_THRESHOLD_UP, 
                                    csrNeighborRoamNeighborLookupUPCallback, 
                                    VOS_MODULE_ID_SME);

    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
       //err msg
       smsLog(pMac, LOGW, FL(" Couldn't Deregister csrNeighborRoamNeighborLookupCallback UP event from TL: Status = %d\n"), vosStatus);
    }

    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Deregistering DOWN event neighbor lookup callback with TL. RSSI = %d,"), pNeighborRoamInfo->currentNeighborLookupThreshold * (-1));
    /* Deregister the UP event now */
    vosStatus = WLANTL_DeregRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->currentNeighborLookupThreshold * (-1),
                                    WLANTL_HO_THRESHOLD_DOWN, 
                                    csrNeighborRoamNeighborLookupDOWNCallback, 
                                    VOS_MODULE_ID_SME);

    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
       //err msg
       smsLog(pMac, LOGW, FL(" Couldn't Deregister csrNeighborRoamNeighborLookupCallback UP event from TL: Status = %d\n"), vosStatus);
    }

    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Deregistering DOWN event reassoc callback with TL. RSSI = %d"), pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1));
    /* Deregister reassoc callback. */
    vosStatus = WLANTL_DeregRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->cfgParams.neighborReassocThreshold * (-1),
                                                        WLANTL_HO_THRESHOLD_DOWN, 
                                                        csrNeighborRoamReassocIndCallback,
                                                        VOS_MODULE_ID_SME);
                        
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
        //err msg
        smsLog(pMac, LOGW, FL(" Couldn't deregister csrNeighborRoamReassocIndCallback with TL: Status = %d\n"), vosStatus);
    }


    /* RSSI got better than the CFG neighbor lookup threshold. Reset the threshold to older value and set the increment multiplier to 0 */
    pNeighborRoamInfo->currentLookupIncrementMultiplier = 0;

    pNeighborRoamInfo->currentNeighborLookupThreshold = pNeighborRoamInfo->cfgParams.neighborLookupThreshold;
    
    /* Reset all the neighbor roam info control variables. Free all the allocated memory. It is like we are just associated now */
    csrNeighborRoamResetConnectedStateControlInfo(pMac);

    /* Recheck whether the below check is needed. */
    if (pNeighborRoamInfo->neighborRoamState != eCSR_NEIGHBOR_ROAM_STATE_CONNECTED)
        CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_CONNECTED)
    
    NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Registering DOWN event neighbor lookup callback with TL. RSSI = %d,"), pNeighborRoamInfo->currentNeighborLookupThreshold * (-1));
    /* Register Neighbor Lookup threshold callback with TL for DOWN event now */
    vosStatus = WLANTL_RegRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->currentNeighborLookupThreshold * (-1),
                                    WLANTL_HO_THRESHOLD_DOWN, 
                                    csrNeighborRoamNeighborLookupDOWNCallback, 
                                    VOS_MODULE_ID_SME, pMac);
    if(!VOS_IS_STATUS_SUCCESS(vosStatus))
    {
       //err msg
       smsLog(pMac, LOGW, FL(" Couldn't register csrNeighborRoamNeighborLookupCallback DOWN event with TL: Status = %d\n"), vosStatus);
    }


    return vosStatus;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamNeighborLookupDownEvent

    \brief  This function is called as soon as TL indicates that the current AP's 
            RSSI falls below the current eighbor lookup threshold. Here, we transition to 
            REPORT_QUERY for 11r association and CFG_CHAN_LIST_SCAN state if the assoc is 
            a non-11R association.

    \param  pMac - The handle returned by macOpen.

    \return VOS_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
VOS_STATUS  csrNeighborRoamNeighborLookupDownEvent(tpAniSirGlobal pMac)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    VOS_STATUS  vosStatus = VOS_STATUS_SUCCESS;
    eHalStatus  status = eHAL_STATUS_SUCCESS;

    switch (pNeighborRoamInfo->neighborRoamState)
    {
        case eCSR_NEIGHBOR_ROAM_STATE_CONNECTED:
            
            NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Deregistering DOWN event neighbor lookup callback with TL. RSSI = %d,"), 
                                                            pNeighborRoamInfo->currentNeighborLookupThreshold * (-1));
            /* De-register Neighbor Lookup threshold callback with TL */
            vosStatus = WLANTL_DeregRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->currentNeighborLookupThreshold * (-1),
                                            WLANTL_HO_THRESHOLD_DOWN, 
                                            csrNeighborRoamNeighborLookupDOWNCallback, 
                                            VOS_MODULE_ID_SME);
            
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
               //err msg
               smsLog(pMac, LOGW, FL(" Couldn't Deregister csrNeighborRoamNeighborLookupCallback DOWN event from TL: Status = %d\n"), status);
            }
            
           
#if defined WLAN_FEATURE_VOWIFI_11R && defined WLAN_FEATURE_VOWIFI
            if ((pNeighborRoamInfo->is11rAssoc) && (pMac->rrm.rrmSmeContext.rrmConfig.rrmEnabled))
            {
               
                NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("11R Association:Neighbor Lookup Down event received in CONNECTED state"));
                vosStatus = csrNeighborRoamIssueNeighborRptRequest(pMac);
                if (VOS_STATUS_SUCCESS != vosStatus)
                {
                    smsLog(pMac, LOGE, FL("Neighbor report request failed. status = %d\n"), vosStatus);
                    return vosStatus;
                }
                /* Increment the neighbor report retry count after sending the neighbor request successfully */
                pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum++;
                pNeighborRoamInfo->FTRoamInfo.neighborRptPending = eANI_BOOLEAN_TRUE;
                CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_REPORT_QUERY)
            }
            else
#endif      
            {
                NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Non 11R or CCX Association:Neighbor Lookup Down event received in CONNECTED state"));

                vosStatus = csrNeighborRoamTransitToCFGChanScan(pMac);
                if (VOS_STATUS_SUCCESS != vosStatus)
                {
                    return vosStatus;
                }
            }
            NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Registering UP event neighbor lookup callback with TL. RSSI = %d,"), pNeighborRoamInfo->cfgParams.neighborLookupThreshold * (-1));
            /* Register Neighbor Lookup threshold callback with TL for UP event now */
            vosStatus = WLANTL_RegRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->cfgParams.neighborLookupThreshold * (-1),
                                            WLANTL_HO_THRESHOLD_UP, 
                                            csrNeighborRoamNeighborLookupUPCallback, 
                                            VOS_MODULE_ID_SME, pMac);
            if(!VOS_IS_STATUS_SUCCESS(vosStatus))
            {
               //err msg
               smsLog(pMac, LOGW, FL(" Couldn't register csrNeighborRoamNeighborLookupCallback UP event with TL: Status = %d\n"), status);
            }
            break;
        default:
            smsLog(pMac, LOGE, FL("DOWN event received in invalid state %d..Ignoring..."), pNeighborRoamInfo->neighborRoamState);
            break;
            
    }
    return vosStatus;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamNeighborLookupUPCallback

    \brief  This function is registered with TL to indicate whenever the RSSI 
            gets better than the neighborLookup RSSI Threshold

    \param  pAdapter - VOS Context
            trafficStatus - UP/DOWN indication from TL
            pUserCtxt - Parameter for callback registered during callback registration. Should be pMac

    \return VOS_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
VOS_STATUS csrNeighborRoamNeighborLookupUPCallback (v_PVOID_t pAdapter, v_U8_t rssiNotification,
                                                                               v_PVOID_t pUserCtxt)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( pUserCtxt );
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    VOS_STATUS  vosStatus = eHAL_STATUS_SUCCESS;

    NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Neighbor Lookup UP indication callback called with notification %d"), rssiNotification);

    if(!csrIsConnStateConnectedInfra(pMac, pNeighborRoamInfo->csrSessionId))
    {
       smsLog(pMac, LOGW, "Ignoring the indication as we are not connected\n");
       return VOS_STATUS_SUCCESS;
    }

    VOS_ASSERT(WLANTL_HO_THRESHOLD_UP == rssiNotification);
    vosStatus = csrNeighborRoamNeighborLookupUpEvent(pMac);
    return vosStatus;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamNeighborLookupDOWNCallback

    \brief  This function is registered with TL to indicate whenever the RSSI 
            falls below the current neighborLookup RSSI Threshold

    \param  pAdapter - VOS Context
            trafficStatus - UP/DOWN indication from TL
            pUserCtxt - Parameter for callback registered during callback registration. Should be pMac

    \return VOS_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
VOS_STATUS csrNeighborRoamNeighborLookupDOWNCallback (v_PVOID_t pAdapter, v_U8_t rssiNotification,
                                                                               v_PVOID_t pUserCtxt)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( pUserCtxt );
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    VOS_STATUS  vosStatus = eHAL_STATUS_SUCCESS;

    NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Neighbor Lookup DOWN indication callback called with notification %d"), rssiNotification);

    if(!csrIsConnStateConnectedInfra(pMac, pNeighborRoamInfo->csrSessionId))
    {
       smsLog(pMac, LOGW, "Ignoring the indication as we are not connected\n");
       return VOS_STATUS_SUCCESS;
    }

    VOS_ASSERT(WLANTL_HO_THRESHOLD_DOWN == rssiNotification);
    vosStatus = csrNeighborRoamNeighborLookupDownEvent(pMac);

    return vosStatus;
}

#ifdef RSSI_HACK
extern int dumpCmdRSSI;
#endif

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamIndicateDisconnect

    \brief  This function is called by CSR as soon as the station disconnects from 
            the AP. This function does the necessary cleanup of neighbor roam data 
            structures. Neighbor roam state transitions to INIT state whenever this 
            function is called except if the current state is REASSOCIATING

    \param  pMac - The handle returned by macOpen.
            sessionId - CSR session id that got disconnected

    \return eHAL_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
eHalStatus csrNeighborRoamIndicateDisconnect(tpAniSirGlobal pMac, tANI_U8 sessionId)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;

    smsLog(pMac, LOGE, FL("Disconnect indication received with session id %d in state %d"), sessionId, pNeighborRoamInfo->neighborRoamState);
 
#ifdef FEATURE_WLAN_CCX
    {
      tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, sessionId);
      if (pSession->connectedProfile.isCCXAssoc)
      {
          vos_mem_copy(&pSession->prevApSSID, &pSession->connectedProfile.SSID, sizeof(tSirMacSSid));
          vos_mem_copy(pSession->prevApBssid, pSession->connectedProfile.bssid, sizeof(tSirMacAddr));
          pSession->prevOpChannel = pSession->connectedProfile.operationChannel;
          pSession->isPrevApInfoValid = TRUE;
          pSession->roamTS1 = vos_timer_get_system_time();

      }
    }
#endif
   
#ifdef RSSI_HACK
    dumpCmdRSSI = -40;
#endif
    switch (pNeighborRoamInfo->neighborRoamState)
    {
        case eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING:
            // Stop scan and neighbor refresh timers.
            // These are indeed not required when we are in reassociating
            // state.
            palTimerStop(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer);
            palTimerStop(pMac->hHdd, pNeighborRoamInfo->neighborResultsRefreshTimer);
            break;

        case eCSR_NEIGHBOR_ROAM_STATE_INIT:
            NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Ignoring disconnect event in INIT state"));
            csrNeighborRoamResetInitStateControlInfo(pMac);
            break; 

        default:
            NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Received disconnect event in state %d"), pNeighborRoamInfo->neighborRoamState);
            NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("Transitioning to INIT state"));
            CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_INIT)
    }
    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamIndicateConnect

    \brief  This function is called by CSR as soon as the station connects to an AP.
            This initializes all the necessary data structures related to the 
            associated AP and transitions the state to CONNECTED state

    \param  pMac - The handle returned by macOpen.
            sessionId - CSR session id that got connected
            vosStatus - connect status SUCCESS/FAILURE

    \return eHAL_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
eHalStatus csrNeighborRoamIndicateConnect(tpAniSirGlobal pMac, tANI_U8 sessionId, VOS_STATUS vosStatus)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    eHalStatus  status = eHAL_STATUS_SUCCESS;
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX)
    VOS_STATUS  vstatus;
    int  init_ft_flag = FALSE;
#endif

    smsLog(pMac, LOGE, FL("Connect indication received with session id %d in state %d"), sessionId, pNeighborRoamInfo->neighborRoamState);

    switch (pNeighborRoamInfo->neighborRoamState)
    {
        case eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING:
            if (VOS_STATUS_SUCCESS != vosStatus)
            {
                /* Just transition the state to INIT state. Rest of the clean up happens when we get next connect indication */
                CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_INIT)
                break;
            }
            /* Fall through if the status is SUCCESS */
        case eCSR_NEIGHBOR_ROAM_STATE_INIT:
            /* Reset all the data structures here */ 
            csrNeighborRoamResetInitStateControlInfo(pMac);

            CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_CONNECTED)

            pNeighborRoamInfo->csrSessionId = sessionId;
            vos_mem_copy(pNeighborRoamInfo->currAPbssid, 
                        pMac->roam.roamSession[sessionId].connectedProfile.bssid, sizeof(tCsrBssid));
            pNeighborRoamInfo->currAPoperationChannel = pMac->roam.roamSession[sessionId].connectedProfile.operationChannel;
            pNeighborRoamInfo->neighborScanTimerInfo.pMac = pMac;
            pNeighborRoamInfo->neighborScanTimerInfo.sessionId = sessionId;
            
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX)
            /* Now we can clear the preauthDone that was saved as we are connected afresh */
            csrNeighborRoamFreeRoamableBSSList(pMac, &pMac->roam.neighborRoamInfo.FTRoamInfo.preAuthDoneList);
#endif
            
#ifdef WLAN_FEATURE_VOWIFI_11R
            // Based on the auth scheme tell if we are 11r
            if ( csrIsAuthType11r( pMac->roam.roamSession[sessionId].connectedProfile.AuthType ) )
            {
                if (pMac->roam.configParam.isFastTransitionEnabled)
                    init_ft_flag = TRUE;
                pNeighborRoamInfo->is11rAssoc = eANI_BOOLEAN_TRUE;
            }
            else
                pNeighborRoamInfo->is11rAssoc = eANI_BOOLEAN_FALSE;
            NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("11rAssoc is = %d"), pNeighborRoamInfo->is11rAssoc);
#endif

#ifdef FEATURE_WLAN_CCX
            // Based on the auth scheme tell if we are 11r
            if (pMac->roam.roamSession[sessionId].connectedProfile.isCCXAssoc)
            {
                if (pMac->roam.configParam.isFastTransitionEnabled)
                    init_ft_flag = TRUE;
                pNeighborRoamInfo->isCCXAssoc = eANI_BOOLEAN_TRUE;
            }
            else
                pNeighborRoamInfo->isCCXAssoc = eANI_BOOLEAN_FALSE;
            NEIGHBOR_ROAM_DEBUG(pMac, LOGE, FL("isCCXAssoc is = %d"), pNeighborRoamInfo->isCCXAssoc);
            VOS_TRACE (VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL, 
                        "ccx=%d ft=%d\n", pNeighborRoamInfo->isCCXAssoc, init_ft_flag);
                            
#endif

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX)
            if ( init_ft_flag == TRUE )
            {
                /* Initialize all the data structures needed for the 11r FT Preauth */
                pNeighborRoamInfo->FTRoamInfo.preAuthRspWaitTimerInfo.pMac = pMac;
                pNeighborRoamInfo->FTRoamInfo.preAuthRspWaitTimerInfo.sessionId = sessionId;
                pNeighborRoamInfo->FTRoamInfo.currentNeighborRptRetryNum = 0;
                csrNeighborRoamPurgePreauthFailedList(pMac);

                NEIGHBOR_ROAM_DEBUG(pMac, LOG2, FL("Registering neighbor lookup DOWN event with TL, RSSI = %d"), pNeighborRoamInfo->currentNeighborLookupThreshold);
                /* Register Neighbor Lookup threshold callback with TL for DOWN event only */
                vstatus = WLANTL_RegRSSIIndicationCB(pMac->roam.gVosContext, (v_S7_t)pNeighborRoamInfo->currentNeighborLookupThreshold * (-1),
                                            WLANTL_HO_THRESHOLD_DOWN, 
                                            csrNeighborRoamNeighborLookupDOWNCallback, 
                                            VOS_MODULE_ID_SME, pMac);
            
                if(!VOS_IS_STATUS_SUCCESS(vstatus))
                {
                   //err msg
                   smsLog(pMac, LOGW, FL(" Couldn't register csrNeighborRoamNeighborLookupDOWNCallback with TL: Status = %d\n"), vstatus);
                   status = eHAL_STATUS_FAILURE;
                }
            }
#endif
            break;
        default:
            smsLog(pMac, LOGE, FL("Connect event received in invalid state %d..Ignoring..."), pNeighborRoamInfo->neighborRoamState);
            break;
    }
    return status;
}


#ifdef WLAN_FEATURE_VOWIFI_11R
/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamPreAuthResponseWaitTimerHandler

    \brief  If this function is invoked, that means the preauthentication response 
            is timed out from the PE. Preauth rsp handler is called with status as 
            TIMEOUT

    \param  context - CSR Timer info which holds pMac and session ID

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamPreAuthResponseWaitTimerHandler(void *context)
{
    tCsrTimerInfo *pTimerInfo = (tCsrTimerInfo *)context;
    tpAniSirGlobal pMac = (tpAniSirGlobal)pTimerInfo->pMac;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;

    pNeighborRoamInfo->FTRoamInfo.preauthRspPending = eANI_BOOLEAN_FALSE;

    csrNeighborRoamPreauthRspHandler(pMac, VOS_STATUS_E_TIMEOUT);
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamPurgePreauthFailedList

    \brief  This function purges all the MAC addresses in the pre-auth fail list

    \param  pMac - The handle returned by macOpen.

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamPurgePreauthFailedList(tpAniSirGlobal pMac)
{
    tANI_U8 i;

    for (i = 0; i < pMac->roam.neighborRoamInfo.FTRoamInfo.preAuthFailList.numMACAddress; i++)
    {
        vos_mem_zero(pMac->roam.neighborRoamInfo.FTRoamInfo.preAuthFailList.macAddress[i], sizeof(tSirMacAddr));
    }
    pMac->roam.neighborRoamInfo.FTRoamInfo.preAuthFailList.numMACAddress = 0;

    return;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamInit11rAssocInfo

    \brief  This function initializes 11r related neighbor roam data structures

    \param  pMac - The handle returned by macOpen.

    \return eHAL_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
eHalStatus csrNeighborRoamInit11rAssocInfo(tpAniSirGlobal pMac)
{
    eHalStatus  status;
    tpCsr11rAssocNeighborInfo   pFTRoamInfo = &pMac->roam.neighborRoamInfo.FTRoamInfo;

    pMac->roam.neighborRoamInfo.is11rAssoc = eANI_BOOLEAN_FALSE;
    pMac->roam.neighborRoamInfo.cfgParams.maxNeighborRetries = pMac->roam.configParam.neighborRoamConfig.nMaxNeighborRetries;
    pFTRoamInfo->neighborReportTimeout = CSR_NEIGHBOR_ROAM_REPORT_QUERY_TIMEOUT;
    pFTRoamInfo->PEPreauthRespTimeout = CSR_NEIGHBOR_ROAM_PREAUTH_RSP_WAIT_MULTIPLIER * pMac->roam.neighborRoamInfo.cfgParams.neighborScanPeriod;
    pFTRoamInfo->neighborRptPending = eANI_BOOLEAN_FALSE;
    pFTRoamInfo->preauthRspPending = eANI_BOOLEAN_FALSE;
    
    pFTRoamInfo->preAuthRspWaitTimerInfo.pMac = pMac;
    pFTRoamInfo->preAuthRspWaitTimerInfo.sessionId = CSR_SESSION_ID_INVALID;
    status = palTimerAlloc(pMac->hHdd, &pFTRoamInfo->preAuthRspWaitTimer, 
                    csrNeighborRoamPreAuthResponseWaitTimerHandler, (void *)&pFTRoamInfo->preAuthRspWaitTimerInfo);

    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("Response wait Timer allocation failed"));
        return eHAL_STATUS_RESOURCES;
    }
    
    pMac->roam.neighborRoamInfo.FTRoamInfo.currentNeighborRptRetryNum = 0;
    pMac->roam.neighborRoamInfo.FTRoamInfo.numBssFromNeighborReport = 0;
    vos_mem_zero(pMac->roam.neighborRoamInfo.FTRoamInfo.neighboReportBssInfo, 
                            sizeof(tCsrNeighborReportBssInfo) * MAX_BSS_IN_NEIGHBOR_RPT);

    
    status = csrLLOpen(pMac->hHdd, &pFTRoamInfo->preAuthDoneList);
    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("LL Open of preauth done AP List failed"));
        palTimerFree(pMac->hHdd, pFTRoamInfo->preAuthRspWaitTimer);
        return eHAL_STATUS_RESOURCES;
    }
    return status;
}
#endif /* WLAN_FEATURE_VOWIFI_11R */

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamInit

    \brief  This function initializes neighbor roam data structures

    \param  pMac - The handle returned by macOpen.

    \return eHAL_STATUS_SUCCESS on success, corresponding error code otherwise

---------------------------------------------------------------------------*/
eHalStatus csrNeighborRoamInit(tpAniSirGlobal pMac)
{
    eHalStatus status;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;

    pNeighborRoamInfo->neighborRoamState       =   eCSR_NEIGHBOR_ROAM_STATE_CLOSED;
    pNeighborRoamInfo->prevNeighborRoamState   =   eCSR_NEIGHBOR_ROAM_STATE_CLOSED;
    pNeighborRoamInfo->csrSessionId            =   CSR_SESSION_ID_INVALID;
    pNeighborRoamInfo->cfgParams.maxChannelScanTime = pMac->roam.configParam.neighborRoamConfig.nNeighborScanMaxChanTime;
    pNeighborRoamInfo->cfgParams.minChannelScanTime = pMac->roam.configParam.neighborRoamConfig.nNeighborScanMinChanTime;
    pNeighborRoamInfo->cfgParams.maxNeighborRetries = 0;
    pNeighborRoamInfo->cfgParams.neighborLookupThreshold = pMac->roam.configParam.neighborRoamConfig.nNeighborLookupRssiThreshold;
    pNeighborRoamInfo->cfgParams.neighborReassocThreshold = pMac->roam.configParam.neighborRoamConfig.nNeighborReassocRssiThreshold;
    pNeighborRoamInfo->cfgParams.neighborScanPeriod = pMac->roam.configParam.neighborRoamConfig.nNeighborScanTimerPeriod;
    pNeighborRoamInfo->cfgParams.neighborResultsRefreshPeriod = pMac->roam.configParam.neighborRoamConfig.nNeighborResultsRefreshPeriod;
    
    pNeighborRoamInfo->cfgParams.channelInfo.numOfChannels   =   
                        pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels;

    pNeighborRoamInfo->cfgParams.channelInfo.ChannelList = 
                vos_mem_malloc(pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels);

    if (NULL == pNeighborRoamInfo->cfgParams.channelInfo.ChannelList)
    {
        smsLog(pMac, LOGE, FL("Memory Allocation for CFG Channel List failed"));
        return eHAL_STATUS_RESOURCES;
    }

    /* Update the roam global structure from CFG */
    palCopyMemory(pMac->hHdd, pNeighborRoamInfo->cfgParams.channelInfo.ChannelList, 
                        pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.channelList,
                        pMac->roam.configParam.neighborRoamConfig.neighborScanChanList.numChannels);

    vos_mem_set(pNeighborRoamInfo->currAPbssid, sizeof(tCsrBssid), 0);
    pNeighborRoamInfo->currentNeighborLookupThreshold = pMac->roam.neighborRoamInfo.cfgParams.neighborLookupThreshold;
    pNeighborRoamInfo->currentLookupIncrementMultiplier = 0;
    pNeighborRoamInfo->scanRspPending = eANI_BOOLEAN_FALSE;

    pNeighborRoamInfo->neighborScanTimerInfo.pMac = pMac;
    pNeighborRoamInfo->neighborScanTimerInfo.sessionId = CSR_SESSION_ID_INVALID;
    status = palTimerAlloc(pMac->hHdd, &pNeighborRoamInfo->neighborScanTimer, 
                    csrNeighborRoamNeighborScanTimerCallback, (void *)&pNeighborRoamInfo->neighborScanTimerInfo);

    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("Response wait Timer allocation failed"));
        vos_mem_free(pNeighborRoamInfo->cfgParams.channelInfo.ChannelList);
        pNeighborRoamInfo->cfgParams.channelInfo.ChannelList = NULL;
        return eHAL_STATUS_RESOURCES;
    }

    status = palTimerAlloc(pMac->hHdd, &pNeighborRoamInfo->neighborResultsRefreshTimer, 
                    csrNeighborRoamResultsRefreshTimerCallback, (void *)&pNeighborRoamInfo->neighborScanTimerInfo);

    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("Response wait Timer allocation failed"));
        smsLog(pMac, LOGE, FL("LL Open of roamable AP List failed"));
        vos_mem_free(pNeighborRoamInfo->cfgParams.channelInfo.ChannelList);
        pNeighborRoamInfo->cfgParams.channelInfo.ChannelList = NULL;
        palTimerFree(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer);
        return eHAL_STATUS_RESOURCES;
    }

    status = csrLLOpen(pMac->hHdd, &pNeighborRoamInfo->roamableAPList);
    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("LL Open of roamable AP List failed"));
        vos_mem_free(pNeighborRoamInfo->cfgParams.channelInfo.ChannelList);
        pNeighborRoamInfo->cfgParams.channelInfo.ChannelList = NULL;
        palTimerFree(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer);
        palTimerFree(pMac->hHdd, pNeighborRoamInfo->neighborResultsRefreshTimer);
        return eHAL_STATUS_RESOURCES;
    }

    pNeighborRoamInfo->roamChannelInfo.currentChanIndex = CSR_NEIGHBOR_ROAM_INVALID_CHANNEL_INDEX;
    pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels = 0;
    pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL;
    pNeighborRoamInfo->roamChannelInfo.chanListScanInProgress = eANI_BOOLEAN_FALSE;
    pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived = eANI_BOOLEAN_FALSE;

#ifdef WLAN_FEATURE_VOWIFI_11R
    status = csrNeighborRoamInit11rAssocInfo(pMac);
    if (eHAL_STATUS_SUCCESS != status)
    {
        smsLog(pMac, LOGE, FL("LL Open of roamable AP List failed"));
        vos_mem_free(pNeighborRoamInfo->cfgParams.channelInfo.ChannelList);
        pNeighborRoamInfo->cfgParams.channelInfo.ChannelList = NULL;
        palTimerFree(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer);
        palTimerFree(pMac->hHdd, pNeighborRoamInfo->neighborResultsRefreshTimer);        
        csrLLClose(&pNeighborRoamInfo->roamableAPList);
        return eHAL_STATUS_RESOURCES;
    }
#endif
    /* Initialize this with the current tick count */
    pNeighborRoamInfo->scanRequestTimeStamp = (tANI_TIMESTAMP)palGetTickCount(pMac->hHdd);

    CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_INIT)
    
    return eHAL_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamClose

    \brief  This function closes/frees all the neighbor roam data structures

    \param  pMac - The handle returned by macOpen.

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamClose(tpAniSirGlobal pMac)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;

    if (eCSR_NEIGHBOR_ROAM_STATE_CLOSED == pNeighborRoamInfo->neighborRoamState)
    {
        smsLog(pMac, LOGE, FL("Neighbor Roam Algorithm Already Closed\n"));
        return;
    }

    if (pNeighborRoamInfo->cfgParams.channelInfo.ChannelList)
        vos_mem_free(pNeighborRoamInfo->cfgParams.channelInfo.ChannelList);
   
    pNeighborRoamInfo->cfgParams.channelInfo.ChannelList = NULL;
    
    pNeighborRoamInfo->neighborScanTimerInfo.pMac = NULL;
    pNeighborRoamInfo->neighborScanTimerInfo.sessionId = CSR_SESSION_ID_INVALID;
    palTimerFree(pMac->hHdd, pNeighborRoamInfo->neighborScanTimer);
    palTimerFree(pMac->hHdd, pNeighborRoamInfo->neighborResultsRefreshTimer);

    /* Should free up the nodes in the list before closing the double Linked list */
    csrNeighborRoamFreeRoamableBSSList(pMac, &pNeighborRoamInfo->roamableAPList);
    csrLLClose(&pNeighborRoamInfo->roamableAPList);
    
    if (pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList)
    {
        vos_mem_free(pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList);
    }

    pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL;
    pNeighborRoamInfo->roamChannelInfo.currentChanIndex = CSR_NEIGHBOR_ROAM_INVALID_CHANNEL_INDEX;
    pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.numOfChannels = 0;
    pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo.ChannelList = NULL;
    pNeighborRoamInfo->roamChannelInfo.chanListScanInProgress = eANI_BOOLEAN_FALSE;    
    pNeighborRoamInfo->roamChannelInfo.IAPPNeighborListReceived = eANI_BOOLEAN_FALSE;

    /* Free the profile.. */ 
    csrReleaseProfile(pMac, &pNeighborRoamInfo->csrNeighborRoamProfile);

#ifdef WLAN_FEATURE_VOWIFI_11R
    pMac->roam.neighborRoamInfo.FTRoamInfo.currentNeighborRptRetryNum = 0;
    palTimerFree(pMac->hHdd, pMac->roam.neighborRoamInfo.FTRoamInfo.preAuthRspWaitTimer);
    pMac->roam.neighborRoamInfo.FTRoamInfo.preAuthRspWaitTimerInfo.pMac = NULL;
    pMac->roam.neighborRoamInfo.FTRoamInfo.preAuthRspWaitTimerInfo.sessionId = CSR_SESSION_ID_INVALID;
    pMac->roam.neighborRoamInfo.FTRoamInfo.numBssFromNeighborReport = 0;
    vos_mem_zero(pMac->roam.neighborRoamInfo.FTRoamInfo.neighboReportBssInfo, 
                            sizeof(tCsrNeighborReportBssInfo) * MAX_BSS_IN_NEIGHBOR_RPT);
    csrNeighborRoamFreeRoamableBSSList(pMac, &pMac->roam.neighborRoamInfo.FTRoamInfo.preAuthDoneList);
    csrLLClose(&pMac->roam.neighborRoamInfo.FTRoamInfo.preAuthDoneList);
#endif /* WLAN_FEATURE_VOWIFI_11R */

    CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_CLOSED)
    
    return;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamRequestHandoff

    \brief  This function triggers actual switching from one AP to the new AP.
            It issues disassociate with reason code as Handoff and CSR as a part of 
            handling disassoc rsp, issues reassociate to the new AP

    \param  pMac - The handle returned by macOpen.

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamRequestHandoff(tpAniSirGlobal pMac)
{

    tCsrRoamInfo roamInfo;
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tANI_U32 sessionId = pNeighborRoamInfo->csrSessionId;
    tCsrNeighborRoamBSSInfo     handoffNode;
    extern void csrRoamRoamingStateDisassocRspProcessor( tpAniSirGlobal pMac, tSirSmeDisassocRsp *pSmeDisassocRsp );
    tANI_U32 roamId = 0;

    if (pMac->roam.neighborRoamInfo.neighborRoamState != eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE) 
    {
        smsLog(pMac, LOGE, FL("Roam requested when Neighbor roam is in %d state"), 
            pMac->roam.neighborRoamInfo.neighborRoamState);
        return;
    }

    vos_mem_zero(&roamInfo, sizeof(tCsrRoamInfo));
    csrRoamCallCallback(pMac, pNeighborRoamInfo->csrSessionId, &roamInfo, roamId, eCSR_ROAM_FT_START, 
                eSIR_SME_SUCCESS);

    vos_mem_zero(&roamInfo, sizeof(tCsrRoamInfo));
    CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING)
    
    csrNeighborRoamGetHandoffAPInfo(pMac, &handoffNode);
    smsLog(pMac, LOGE, FL("HANDOFF CANDIDATE BSSID %02x:%02x:%02x:%02x:%02x:%02x"),
                                                handoffNode.pBssDescription->bssId[0], 
                                                handoffNode.pBssDescription->bssId[1], 
                                                handoffNode.pBssDescription->bssId[2], 
                                                handoffNode.pBssDescription->bssId[3], 
                                                handoffNode.pBssDescription->bssId[4], 
                                                handoffNode.pBssDescription->bssId[5]);
   
    /* Free the profile.. Just to make sure we dont leak memory here */ 
    csrReleaseProfile(pMac, &pNeighborRoamInfo->csrNeighborRoamProfile);
    /* Create the Handoff AP profile. Copy the currently connected profile and update only the BSSID and channel number
        This should happen before issuing disconnect */
    csrRoamCopyConnectedProfile(pMac, pNeighborRoamInfo->csrSessionId, &pNeighborRoamInfo->csrNeighborRoamProfile);
    vos_mem_copy(pNeighborRoamInfo->csrNeighborRoamProfile.BSSIDs.bssid, handoffNode.pBssDescription->bssId, sizeof(tSirMacAddr));
    pNeighborRoamInfo->csrNeighborRoamProfile.ChannelInfo.ChannelList[0] = handoffNode.pBssDescription->channelId;
    
    NEIGHBOR_ROAM_DEBUG(pMac, LOGW, " csrRoamHandoffRequested: disassociating with current AP\n");

    if(!HAL_STATUS_SUCCESS(csrRoamIssueDisassociateCmd(pMac, sessionId, eCSR_DISCONNECT_REASON_HANDOFF)))
    {
        smsLog(pMac, LOGW, "csrRoamHandoffRequested:  fail to issue disassociate\n");
        return;
    }                       

    //notify HDD for handoff, providing the BSSID too
    roamInfo.reasonCode = eCsrRoamReasonBetterAP;

    vos_mem_copy(roamInfo.bssid, 
                 handoffNode.pBssDescription->bssId, 
                 sizeof( tCsrBssid ));

    csrRoamCallCallback(pMac, sessionId, &roamInfo, 0, eCSR_ROAM_ROAMING_START, eCSR_ROAM_RESULT_NONE);


    return;
}

/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamIsHandoffInProgress

    \brief  This function returns whether handoff is in progress or not based on 
            the current neighbor roam state

    \param  pMac - The handle returned by macOpen.
            is11rReassoc - Return whether reassoc is of type 802.11r reassoc

    \return eANI_BOOLEAN_TRUE if reassoc in progress, eANI_BOOLEAN_FALSE otherwise

---------------------------------------------------------------------------*/
tANI_BOOLEAN csrNeighborRoamIsHandoffInProgress(tpAniSirGlobal pMac)
{
    if (eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING == pMac->roam.neighborRoamInfo.neighborRoamState)
        return eANI_BOOLEAN_TRUE;

    return eANI_BOOLEAN_FALSE;
}

#ifdef WLAN_FEATURE_VOWIFI_11R
/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamIs11rAssoc

    \brief  This function returns whether the current association is a 11r assoc or not

    \param  pMac - The handle returned by macOpen.

    \return eANI_BOOLEAN_TRUE if current assoc is 11r, eANI_BOOLEAN_FALSE otherwise

---------------------------------------------------------------------------*/
tANI_BOOLEAN csrNeighborRoamIs11rAssoc(tpAniSirGlobal pMac)
{
    return pMac->roam.neighborRoamInfo.is11rAssoc;
}
#endif /* WLAN_FEATURE_VOWIFI_11R */


/* ---------------------------------------------------------------------------

    \fn csrNeighborRoamGetHandoffAPInfo

    \brief  This function returns the best possible AP for handoff. For 11R case, it 
            returns the 1st entry from pre-auth done list. For non-11r case, it returns 
            the 1st entry from roamable AP list

    \param  pMac - The handle returned by macOpen.
            pHandoffNode - AP node that is the handoff candidate returned

    \return VOID

---------------------------------------------------------------------------*/
void csrNeighborRoamGetHandoffAPInfo(tpAniSirGlobal pMac, tpCsrNeighborRoamBSSInfo pHandoffNode)
{
    tpCsrNeighborRoamControlInfo    pNeighborRoamInfo = &pMac->roam.neighborRoamInfo;
    tpCsrNeighborRoamBSSInfo        pBssNode;
    
    VOS_ASSERT(NULL != pHandoffNode); 
        
#ifdef WLAN_FEATURE_VOWIFI_11R
    if (pNeighborRoamInfo->is11rAssoc)
    {
        /* Always the BSS info in the head is the handoff candidate */
        pBssNode = csrNeighborRoamGetRoamableAPListNextEntry(pMac, &pNeighborRoamInfo->FTRoamInfo.preAuthDoneList, NULL);
        NEIGHBOR_ROAM_DEBUG(pMac, LOG1, FL("Number of Handoff candidates = %d"), csrLLCount(&pNeighborRoamInfo->FTRoamInfo.preAuthDoneList));
    }
    else
#endif
#ifdef FEATURE_WLAN_CCX
    if (pNeighborRoamInfo->isCCXAssoc)
    {
        /* Always the BSS info in the head is the handoff candidate */
        pBssNode = csrNeighborRoamGetRoamableAPListNextEntry(pMac, &pNeighborRoamInfo->FTRoamInfo.preAuthDoneList, NULL);
        NEIGHBOR_ROAM_DEBUG(pMac, LOG1, FL("Number of Handoff candidates = %d"), csrLLCount(&pNeighborRoamInfo->FTRoamInfo.preAuthDoneList));
    }
    else
#endif
    {
        pBssNode = csrNeighborRoamGetRoamableAPListNextEntry(pMac, &pNeighborRoamInfo->roamableAPList, NULL);
        NEIGHBOR_ROAM_DEBUG(pMac, LOG1, FL("Number of Handoff candidates = %d"), csrLLCount(&pNeighborRoamInfo->roamableAPList));
    }
    vos_mem_copy(pHandoffNode, pBssNode, sizeof(tCsrNeighborRoamBSSInfo));

    return;
}

/* ---------------------------------------------------------------------------
    \brief  This function returns TRUE if preauth is completed 

    \param  pMac - The handle returned by macOpen.

    \return boolean

---------------------------------------------------------------------------*/
tANI_BOOLEAN csrNeighborRoamStatePreauthDone(tpAniSirGlobal pMac)
{
    return (pMac->roam.neighborRoamInfo.neighborRoamState == 
               eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE);
}

/* ---------------------------------------------------------------------------
    \brief  In the event that we are associated with AP1 and we have
    completed pre auth with AP2. Then we receive a deauth/disassoc from
    AP1. 
    At this point neighbor roam is in pre auth done state, pre auth timer
    is running. We now handle this case by stopping timer and clearing
    the pre-auth state. We basically clear up and just go to disconnected
    state. 

    \param  pMac - The handle returned by macOpen.

    \return boolean
---------------------------------------------------------------------------*/
void csrNeighborRoamTranistionPreauthDoneToDisconnected(tpAniSirGlobal pMac)
{
    if (pMac->roam.neighborRoamInfo.neighborRoamState != 
               eCSR_NEIGHBOR_ROAM_STATE_PREAUTH_DONE) return;

    // Stop timer
    palTimerStop(pMac->hHdd, pMac->ft.ftSmeContext.preAuthReassocIntvlTimer);

    // Transition to init state
    CSR_NEIGHBOR_ROAM_STATE_TRANSITION(eCSR_NEIGHBOR_ROAM_STATE_INIT)
}

#endif /* WLAN_FEATURE_NEIGHBOR_ROAMING */
