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
 * This file limAIDmgmt.cc contains the functions related to
 * AID pool management like initialization, assignment etc.
 * Author:        Chandra Modumudi
 * Date:          03/20/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
 
#include "palTypes.h"
#include "wniCfgSta.h"
#include "aniGlobal.h"
#include "cfgApi.h"
#include "sirParams.h"
#include "limUtils.h"
#include "limTimerUtils.h"
#include "limSession.h"

#define LIM_START_AID   1


/**
 * limInitAIDpool()
 *
 *FUNCTION:
 * This function is called while starting a BSS at AP
 * to initialize AID pool. This may also be called while
 * starting/joining an IBSS if 'Association' is allowed
 * in IBSS.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void
limInitAIDpool(tpAniSirGlobal pMac,tpPESession sessionEntry)
{
    tANI_U8 i;
    tANI_U8 maxAssocSta = pMac->lim.gLimAssocStaLimit;

    pMac->lim.gpLimAIDpool[0]=0;
    pMac->lim.freeAidHead=LIM_START_AID;

    for (i=pMac->lim.freeAidHead;i<maxAssocSta; i++)
    {
        pMac->lim.gpLimAIDpool[i]         = i+1;
    }
    pMac->lim.gpLimAIDpool[i]         =  0;

    pMac->lim.freeAidTail=i;

}


/**
 * limAssignAID()
 *
 *FUNCTION:
 * This function is called during Association/Reassociation
 * frame handling to assign association ID (aid) to a STA.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return aid  - assigned Association ID for STA
 */

tANI_U16
limAssignAID(tpAniSirGlobal pMac)
{
    tANI_U16 aid;

    // make sure we haven't exceeded the configurable limit on associations
    if (pMac->lim.gLimNumOfCurrentSTAs >= pMac->lim.gLimAssocStaLimit)
    {
        // too many associations already active
        return 0;
    }

    /* return head of free list */

    if (pMac->lim.freeAidHead)
    {
        aid=pMac->lim.freeAidHead;
        pMac->lim.freeAidHead=pMac->lim.gpLimAIDpool[pMac->lim.freeAidHead];
        if (pMac->lim.freeAidHead==0)
            pMac->lim.freeAidTail=0;
        pMac->lim.gLimNumOfCurrentSTAs++;
        //PELOG2(limLog(pMac, LOG2,FL("Assign aid %d, numSta %d, head %d tail %d \n"),aid,pMac->lim.gLimNumOfCurrentSTAs,pMac->lim.freeAidHead,pMac->lim.freeAidTail);)
        return aid;
    }

    return 0; /* no more free aids */
}


/**
 * limReleaseAID()
 *
 *FUNCTION:
 * This function is called when a STA context is removed
 * at AP (or at a STA in IBSS mode) to return association ID (aid)
 * to free pool.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  aid - Association ID that need to return to free pool
 *
 * @return None
 */

void
limReleaseAID(tpAniSirGlobal pMac, tANI_U16 aid)
{
    pMac->lim.gLimNumOfCurrentSTAs--;

    /* insert at tail of free list */
    if (pMac->lim.freeAidTail)
    {
        pMac->lim.gpLimAIDpool[pMac->lim.freeAidTail]=(tANI_U8)aid;
        pMac->lim.freeAidTail=(tANI_U8)aid;
    }
    else
    {
        pMac->lim.freeAidTail=pMac->lim.freeAidHead=(tANI_U8)aid;
    }
    pMac->lim.gpLimAIDpool[(tANI_U8)aid]=0;
    //PELOG2(limLog(pMac, LOG2,FL("Release aid %d, numSta %d, head %d tail %d \n"),aid,pMac->lim.gLimNumOfCurrentSTAs,pMac->lim.freeAidHead,pMac->lim.freeAidTail);)

}
