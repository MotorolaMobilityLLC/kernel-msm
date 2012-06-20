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
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file pmmAP.cc contains AP PM functions
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "sirCommon.h"

#include "aniGlobal.h"

#include "schApi.h"
#include "limApi.h"
#include "cfgApi.h"
#include "wniCfgAp.h"

#include "pmmApi.h"
#include "pmmDebug.h"

#define PMM_TIM_BITS_LIMIT        10
#define PMM_CF_POLLABLE_SCH_LIMIT 100

// sets how many beacons are skipped after a ps-poll before rxactivityset
// processing for that sta starts again
#define PMM_PSPOLL_PERSISTENCE    2

#if (WNI_POLARIS_FW_PRODUCT==AP)
// --------------------------------------------------------------------
/**
 *  @function: __isTimChanged
 *
 *  @brief : Compares and returns TRUE if the TIM BitMap needs change
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMac      AniSirGlobal 
 * @param newTim    Newly created Tim Data's
 * @param prevTim    Current Tim Data's
 * @return eANI_BOOLEAN     eANI_BOOLEAN_TRUE - If there is change in Tim Data's
 *                                      eANI_BOOLEAN_FALSE - If there is no need to change in Tim
 */

static tANI_BOOLEAN 
__isTimChanged(tpAniSirGlobal pMac, tpPmmTim pNewTim, tpPmmTim pPrevTim)
{
    if ( pNewTim->dtimCount != pPrevTim->dtimCount ||
         pNewTim->maxAssocId != pPrevTim->maxAssocId ||
         pNewTim->minAssocId != pPrevTim->minAssocId ||
         pNewTim->numStaWithData != pPrevTim->numStaWithData)         
        return eANI_BOOLEAN_TRUE;
    
    if (!palEqualMemory(pMac->hHdd, pNewTim->pTim, pPrevTim->pTim, sizeof(*pPrevTim->pTim) * pMac->lim.maxStation))
        return eANI_BOOLEAN_TRUE;
 
    return eANI_BOOLEAN_FALSE;
}

/**
 *  @function: __updatePmmGlobal
 *
 *  @brief : Updates the PMM Global Structure
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMac      AniSirGlobal 
 * @param newHdr    Header Details to be updated in Pmm globalHeader
 * @param globalHdr  global Header
 * @return None
 */
static /*inline */
void    __updatePmmGlobal( tpAniSirGlobal pMac, tpPmmTim pNewTim)
{
    tpPmmTim    pGlobalTim = &pMac->pmm.gPmmTim;
    palCopyMemory(pMac->hHdd, pGlobalTim->pTim,  pNewTim->pTim,
                                                        sizeof(*pNewTim->pTim) * pMac->lim.maxStation);

    pGlobalTim->dtimCount = pNewTim->dtimCount;
    pGlobalTim->maxAssocId = pNewTim->maxAssocId;
    pGlobalTim->minAssocId = pNewTim->minAssocId;
    pGlobalTim->numStaWithData = pNewTim->numStaWithData;    
}


void pmmUpdatePSPollState(tpAniSirGlobal pMac)
{
    tANI_U16 psFlag = 0;
    tANI_U32 sta;

    // if there are no stations which performed a ps-poll update, we are done
    if (pMac->pmm.gPmmPsPollUpdate == 0)
        return;

    /*
     * for all stations which had a state udpate due to a ps-poll, we check
     * if it is time to update their state from hardware. This is tracked with
     * per sta psPollUpdate counter which is initialized during ps-poll
     * processing.
     * if no stations need any further ps-poll processing, clear the global flag
     * so that we don't have to keep going thru the stations at every beacon
     */
    for (sta = 1; sta < pMac->lim.maxStation; sta++)
    {
        tANI_U8 ps = 0;

        if (pMac->pmm.gpPmmStaState[sta].psPollUpdate == 0)
            continue;

        if (pMac->pmm.gpPmmStaState[sta].psPollUpdate == 1)
        {
            pmmUpdatePMMode(pMac, sta, ps);
        }
        pMac->pmm.gpPmmStaState[sta].psPollUpdate--;
        psFlag++;
        PELOG4(pmmLog(pMac, LOG4, FL("UpdatePSPollState: sta %d, state %d (count %d)\n"), sta, ps, pMac->pmm.gpPmmStaState[sta].psPollUpdate););
    }

    if (psFlag == 0)
        pMac->pmm.gPmmPsPollUpdate = 0;
    PELOG3(pmmLog(pMac, LOG3, FL("UpdatePSPollState: %d ps-poll flagged sta's\n"), psFlag);)
}

/**
 * @function : pmmHandleTimBasedDisassociation
 * 
 * @brief :  Handle TIM based Disassoc for STA's in PS
 *
 * LOGIC:
 * Checks whether the AP has sent TIM enough
 * number of times for the station and if STA in PS didn't collect the buffered pckts,
 * disconnect the station
 *
 * NOTE:
 * @param : pMac - tpAniSirGlobal
 * @return : none
 **/
 
void pmmHandleTimBasedDisassociation (tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    tpPmmStaInfo pmmStaInfo = pMac->pmm.gPmmTim.pStaInfo;
    tANI_U32    staCnt;
    tANI_U32    staWithData = pMac->pmm.gPmmTim.numStaWithData;
    

    /** Traverse through all the assocId's in PS */
    for (staCnt = 0; staCnt < pMac->pmm.gPmmNumSta; staCnt++) {
        tpDphHashNode pSta = dphGetHashEntry(pMac, pmmStaInfo[staCnt].assocId);

        if ((pSta == NULL) ||(!pSta->valid )) {
            pmmLog(pMac, LOGE, FL("Invalid ASSOCID / associd points to notvalid STA assocId [%d] NumSta [%d]\n"),pmmStaInfo[staCnt].assocId, 
                pMac->pmm.gPmmNumSta );
            continue;
        }
        
        if (staCnt >= staWithData) /** If the AP doesn't have any data buffered for this STA*/
            pSta->timWaitCount = GET_TIM_WAIT_COUNT(pSta->mlmStaContext.listenInterval);   /** Reset the timWaitCount*/
        else {  /** AP has got some data buffered for this STA*/
            if (pSta->curTxMpduCnt == pmmStaInfo[staCnt].staTxAckCnt) {
                /** If we have sent enough number of TIM's to the STA trigger DisAssoc*/
                if ( !(--pSta->timWaitCount)) { 
                    PELOGE(pmmLog(pMac, LOGE, FL(" STA with AID %d did not respond to TIM, deleting..."),pmmStaInfo[staCnt].assocId);)
                    limTriggerSTAdeletion(pMac, pSta, psessionEntry);
                    continue;
                }
            }else /** if the STA in PS came up and got some data*/
                pSta->timWaitCount = GET_TIM_WAIT_COUNT(pSta->mlmStaContext.listenInterval);  /** Reset the timWaitCount*/
        }
        pSta->curTxMpduCnt = pmmStaInfo[staCnt].staTxAckCnt;
    }

    return;
 }

#endif

#ifdef WLAN_SOFTAP_FEATURE
/**
 * pmmGenerateTIM
 *
 * FUNCTION:
 * Generate TIM
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMac pointer to global mac structure
 * @param **pPtr pointer to the buffer, where the TIM bit is to be written.
 * @param *timLength pointer to limLength, which needs to be returned.
 * @return None
 */
void pmmGenerateTIM(tpAniSirGlobal pMac, tANI_U8 **pPtr, tANI_U16 *timLength, tANI_U8 dtimPeriod)
{
    tANI_U8 *ptr = *pPtr;
    tANI_U32 val = 0;
    tANI_U32 minAid = 1; // Always start with AID 1 as minimum
    tANI_U32 maxAid = HAL_NUM_STA;


    // Generate partial virtual bitmap
    tANI_U8 N1 = minAid / 8;
    tANI_U8 N2 = maxAid / 8;
    if (N1 & 1) N1--;

    *timLength = N2 - N1 + 4;
    val = dtimPeriod;

    /* 
     * 09/23/2011 - ASW team decision; 
     * Write 0xFF to firmware's field to detect firmware's mal-function early.
     * DTIM count and bitmap control usually cannot be 0xFF, so it is easy to know that 
     * firmware never updated DTIM count/bitmap control field after host driver downloaded
     * beacon template if end-user complaints that DTIM count and bitmapControl is 0xFF.
     */
    *ptr++ = SIR_MAC_TIM_EID;
    *ptr++ = (tANI_U8)(*timLength);
    *ptr++ = 0xFF; // location for dtimCount. will be filled in by FW.
    *ptr++ = (tANI_U8)val;

    *ptr++ = 0xFF; // location for bitmap contorl. will be filled in by FW.
    ptr += (N2 - N1 + 1);

    PELOG2(sirDumpBuf(pMac, SIR_PMM_MODULE_ID, LOG2, *pPtr, (*timLength)+2);)
    *pPtr = ptr;
}

#endif
#ifdef ANI_PRODUCT_TYPE_AP
/**
 * pmmUpdateTIM
 *
 * FUNCTION: This function creates TIM information and saves it under PMM context.
 * This TIM bit information is used to generated beacons later on.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMac pointer to global mac structure
 * @param pBeaconGenParams pointer to structure which has power save information returned by HAL.
 * @return None
 */

void pmmUpdateTIM(tpAniSirGlobal pMac, tpBeaconGenParams pBeaconGenParams)
{
    tANI_U16 i;
    tANI_U16 assocId;
    tPmmTim curTim;
    tPmmTim *pPmmTim = &pMac->pmm.gPmmTim;
    tANI_U16 totalSta;
    tpBeaconGenStaInfo pStaInfo;

    totalSta = pBeaconGenParams->numOfSta + pBeaconGenParams->numOfStaWithoutData;

    if(totalSta > pMac->lim.maxStation)
    {
        PELOGE(pmmLog(pMac, LOGE, FL("Error in TIM Update: Total STA count %d exceeds the MAX Allowed %d \n"), totalSta, pMac->lim.maxStation);)
        return;
    }
    palZeroMemory(pMac->hHdd, &curTim,  sizeof(curTim));
    palZeroMemory(pMac->hHdd, pMac->pmm.gpPmmPSState, 
        sizeof(*pMac->pmm.gpPmmPSState) * pMac->lim.maxStation);

    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
                  (void **) &curTim.pTim, sizeof(tANI_U8)*pMac->lim.maxStation))
    {
        pmmLog( pMac, LOGE, FL( "Unable to PAL allocate memory\n" ));
        return;                 
    }
    palZeroMemory(pMac->hHdd, curTim.pTim, sizeof(tANI_U8) * pMac->lim.maxStation);
    pPmmTim->numStaWithData = curTim.numStaWithData = pBeaconGenParams->numOfSta;
    pMac->pmm.gPmmNumSta = totalSta;
    curTim.minAssocId= pMac->lim.maxStation - 1;
    curTim.maxAssocId= 0;
    curTim.dtimCount = pBeaconGenParams->dtimCount;
    pStaInfo = (tpBeaconGenStaInfo)((tANI_U8 *)pBeaconGenParams + sizeof(*pBeaconGenParams));

    if (totalSta > 0) 
        palCopyMemory(pMac->hHdd, pPmmTim->pStaInfo, pStaInfo, 
                                                                sizeof(*pPmmTim->pStaInfo) * totalSta);
    
    for(i=0; i<totalSta; i++)
    {
        assocId = pStaInfo[i].assocId;      
        if(assocId < pMac->lim.maxStation)
        {
            //Update TIM for power save stations which have data pending.
            if(i<pBeaconGenParams->numOfSta)
            {
                PELOG2(pmmLog(pMac, LOG2, FL("Setting TIM for assocId: %d\n"), assocId);)
                curTim.pTim[assocId] = 1;
                if (assocId < curTim.minAssocId)
                    curTim.minAssocId= assocId;
                if (assocId > curTim.maxAssocId)
                    curTim.maxAssocId= assocId;
            }
            pMac->pmm.gpPmmPSState[assocId] = 1;
                    
        }    
    }

    if (!curTim.maxAssocId)
        curTim.minAssocId = 0;

    if(pBeaconGenParams->fBroadcastTrafficPending)
        curTim.pTim[0] = 1;

    //Now check if TIM contents have changed from the last beacon.
    if (!__isTimChanged(pMac, &curTim, pPmmTim)) {
        palFreeMemory(pMac->hHdd, curTim.pTim);
        return;
    }

    //Time to update the beacon
    __updatePmmGlobal(pMac, &curTim);
    
    pMac->sch.schObject.fBeaconChanged = 1;
    palFreeMemory(pMac->hHdd, curTim.pTim);
    
    return;
}




// --------------------------------------------------------------------
/**
 * pmmProcessPSPoll
 *
 * FUNCTION:
 * Process the PS poll message
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pBD pointer to the buffer descriptor for PS poll message
 * @return None
 */

void pmmProcessPSPoll(tpAniSirGlobal pMac, tpHalBufDesc pBd)
{

    tpSirMacMgmtHdr mh = SIR_MAC_BD_TO_MPDUHEADER(pBd);


    // Extract the AID of the station
    tANI_U16 aid = mh->durationLo + ((mh->durationHi & 0x3f) << 8);

    PELOG1(pmmLog(pMac, LOG1, FL("Received PS Poll AID 0x%x\n"), aid);)
    if (aid == 0 || aid >= pMac->lim.maxStation)
    {
       PELOG1(pmmLog(pMac, LOG1, FL("STA id %d in update PM, should be [1,%d]\n"),
               aid, pMac->lim.maxStation);)
        return;
    }

    tpDphHashNode pSta = dphGetHashEntry(pMac, aid);
    if (pSta && pSta->valid)
        pSta->numTimSent = 0;

    /*
     * since PS poll arrives asynchornously wrt the rx activity set, check
     * the current state of the BMU PS state for this sta (only for non-Ani)
     *
     * Since an asynchronous PS poll is being processed, we can't trust the
     * rx activity set for PSPOLL_PERSISTENCE beacons. So set the
     * Update counter - this will be checked during beacon processing
     */
    pMac->pmm.gpPmmStaState[aid].psPollUpdate = PMM_PSPOLL_PERSISTENCE;
    pMac->pmm.gPmmPsPollUpdate = 1;
    if (mh->fc.powerMgmt != pMac->pmm.gpPmmStaState[aid].powerSave)
    {
        tANI_U8 ps = 0;
        if (pMac->pmm.gpPmmStaState[aid].powerSave != ps)
        {
           PELOG1(pmmLog(pMac, LOG1, FL("Sta %d: PS Poll processing PS-state %d -> %d\n"),
                   aid, pMac->pmm.gpPmmStaState[aid].powerSave, ps);)
            pmmUpdatePMMode(pMac, aid, ps);
        }
    }

#ifdef PMM_APSD
    /*
     * See if all ACs are trigger enabled.
     * Section 3.6.1.6 of WMM-APSD spec.
     *
     * In case all ACs associated with the WMM STA are delivery-enabled ACs,
     * then no ACs have been selected for legacy power-save, and the AP shall
     * immediately send either an ACK frame, a Null function Data frame with
     * the More Data bit set to zero, or a Null function QoS Data frame with
     * the More Data bit and the EOSP bit set to zero in response to the
     * receipt of a PS-Poll frame.
     */

    {
        tANI_U8 uapsdFlag = (*((tANI_U8*) &pSta->qos.capability.qosInfo)) & 0xF;

        if ((uapsdFlag & 0xf) == 0xf)
        {
            /*
             * All ACs are trigger/delivery enabled.
             */
            apsdSendQosNull(pMac, pSta->assocId, 0, pSta->staAddr);
        }
    }

#endif

    /*
     * Set the PS_EN1 bit in BMU to enable transmission
     * of one packet from that queue
     * This is being done by the RHP now
     */

    // TBD - if HCF mode, schedule a transmission
}

void pmmProcessRxActivity(tpAniSirGlobal pMac, tANI_U16 staId, tANI_U8 pmMode)
{
    if (staId == 0 || staId >= pMac->lim.maxStation)
    {
       PELOG1(pmmLog(pMac, LOG1, FL("STA id %d in RxActProc, should be [1,%d]\n"),
               staId, pMac->lim.maxStation);)
        return;
    }

    PELOG1(pmmLog(pMac, LOG1, FL("STA id %d;) pmMode %d\n"), staId, pmMode);)

    // ignore rxActivity if the state has been updated asynchronously
    if (pMac->pmm.gpPmmStaState[staId].psPollUpdate == 0)
    {
        pmmUpdatePMMode(pMac, staId, pmMode);
    }
}

// --------------------------------------------------------------------
/**
 * pmmUpdatePMMode
 *
 * FUNCTION:
 * Update the power mode bit
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staId STA id of the station
 * @param pmMode PM bit received from the STA
 * @return None
 */

void pmmUpdatePMMode(tpAniSirGlobal pMac, tANI_U16 staId, tANI_U8 pmMode)
{
    if ((staId == 0) ||
        (staId == 1) ||
        (staId >= pMac->lim.maxStation))
    {
        pmmLog(pMac, LOGE, FL("STA id %d in update PM, should be [2,%d]\n"),
               staId, pMac->lim.maxStation);
        return;
    }

    // If the STA has been deleted, ignore this call
    tpDphHashNode pSta = dphGetHashEntry(pMac, staId);
    if (pSta == NULL || pSta->valid == 0)
        return;

    if (pMac->pmm.gpPmmStaState[staId].powerSave != pmMode)
    {
       PELOG1(pmmLog(pMac, LOG1, FL("STA id %d, PM mode %d\n"),
               staId, pmMode);)
#ifdef WMM_APSD
        pmmLog(pMac, LOGE, FL("pm mode change: STA id %d, PM mode %d\n"),
               staId, pmMode);
#endif

        // Update PM mode for sta id
        pMac->pmm.gpPmmStaState[staId].powerSave = pmMode;

        // Find the numerically preceding STA in power save state
        tANI_U16 i = (staId == 2) ? pMac->lim.maxStation-1 : staId-1;
        while (i != staId &&
               !(pMac->pmm.gpPmmStaState[i].powerSave == 1 &&
                 pMac->pmm.gpPmmStaState[i].cfPollable == pMac->pmm.gpPmmStaState[staId].cfPollable)
              )
        {
            i = (i == 2) ? pMac->lim.maxStation-1 : i-1;
        }

        // Adjust the linked list appropriately
        if (pMac->pmm.gpPmmStaState[staId].cfPollable)
            pmmUpdatePollablePMMode(pMac, staId, pmMode, i);
        else
            pmmUpdateNonPollablePMMode(pMac, staId, pmMode, i);

        // Update the count of number of STAs in PS
        if (pmMode)
            pMac->pmm.gPmmNumSta++;
        else
        {
            pMac->pmm.gPmmNumSta--;

            // reset the num TIMs sent since STA is out of power save
            pSta->numTimSent = 0;
        }

        // Update multicast PM mode bit appropriately
        tANI_U8 multicastPS = pMac->pmm.gPmmNumSta ? 1 : 0;
        if (pMac->pmm.gpPmmStaState[0].powerSave != multicastPS)
        {
            pMac->pmm.gpPmmStaState[0].powerSave = multicastPS;
            // TBD - write to BMU PS bit for STA 0
        }
    }
}

// --------------------------------------------------------------------
/**
 * pmmUpdateNonPollablePMMode
 *
 * FUNCTION:
 * Update the power mode bit
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staId STA id of the station
 * @param pmMode PM bit received from the STA
 * @param i index of the previous STA in the linked list
 * @return None
 */

void pmmUpdateNonPollablePMMode(tpAniSirGlobal pMac, tANI_U16 staId, tANI_U8 pmMode, tANI_U16 i)
{
    if (i == staId)
    {
        // There is no other non pollable STA in PS
        pMac->pmm.gpPmmStaState[staId].nextPS = (tANI_U8)staId;
        if (pmMode)
        {
            // Current STA is the only non pollable STA in PS
            pMac->pmm.gPmmNextSta = staId;
        }
        else
        {
            // There is absolutely no non pollable STA in PS
            pMac->pmm.gPmmNextSta = 0;
        }
    }
    else
    {
        // There is(are) other non pollable STA(s) in PS mode
        if (pmMode)
        {
            // Link in the current STA to the list of non pollable PS STAs
            pMac->pmm.gpPmmStaState[staId].nextPS = pMac->pmm.gpPmmStaState[i].nextPS;
            pMac->pmm.gpPmmStaState[i].nextPS = (tANI_U8)staId;
        }
        else
        {
            // Remove the current STA from the list of non pollable PS STAs
            pMac->pmm.gpPmmStaState[i].nextPS = pMac->pmm.gpPmmStaState[staId].nextPS;

            // If the STA being removed is the nextSTA, update nextSTA
            if (staId == pMac->pmm.gPmmNextSta)
                pMac->pmm.gPmmNextSta = pMac->pmm.gpPmmStaState[staId].nextPS;
        }
    }
}

// --------------------------------------------------------------------
/**
 * pmmUpdatePollablePMMode
 *
 * FUNCTION:
 * Update the power mode bit
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param staId STA id of the station
 * @param pmMode PM bit received from the STA
 * @param i index of the previous STA in the linked list
 * @return None
 */

void pmmUpdatePollablePMMode(tpAniSirGlobal pMac, tANI_U16 staId, tANI_U8 pmMode, tANI_U16 i)
{
    if (i == staId)
    {
        // There is no other pollable STA in PS
        pMac->pmm.gpPmmStaState[staId].nextPS = (tANI_U8)staId;
        if (pmMode)
        {
            // Current STA is the only pollable STA in PS
            pMac->pmm.gPmmNextCFPSta = staId;
        }
        else
        {
            // There is absolutely no pollable STA in PS
            pMac->pmm.gPmmNextCFPSta = 0;
        }
    }
    else
    {
        // There is(are) other pollable STA(s) in PS mode
        if (pmMode)
        {
            // Link in the current STA to the list of pollable PS STAs
            pMac->pmm.gpPmmStaState[staId].nextPS = pMac->pmm.gpPmmStaState[i].nextPS;
            pMac->pmm.gpPmmStaState[i].nextPS = (tANI_U8)staId;
        }
        else
        {
            // Remove the current STA from the list of pollable PS STAs
            pMac->pmm.gpPmmStaState[i].nextPS = pMac->pmm.gpPmmStaState[staId].nextPS;

            // If the STA being removed is the nextSTA, update nextSTA
            if (staId == pMac->pmm.gPmmNextCFPSta)
                pMac->pmm.gPmmNextCFPSta = pMac->pmm.gpPmmStaState[staId].nextPS;
        }
    }
}



#endif // (WNI_POLARIS_FW_PRODUCT==AP)

// --------------------------------------------------------------------

