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

/******************************************************************************
*
* Name:  pmc.c
*
* Description: 
      Power Management Control (PMC) processing routines.
*
* Copyright 2008 (c) Qualcomm, Incorporated. All Rights Reserved.
*     Qualcomm Confidential and Proprietary.
*
*
******************************************************************************/

#include "palTypes.h"
#include "aniGlobal.h"
#include "csrLinkList.h"
#include "csrApi.h"
#include "smeInside.h"
#include "sme_Api.h"
#include "smsDebug.h"
#include "pmc.h"
#include "wlan_ps_wow_diag.h"
#include <vos_power.h>
#include "csrInsideApi.h"

static void pmcProcessDeferredMsg( tpAniSirGlobal pMac );

/******************************************************************************
*
* Name:  pmcEnterLowPowerState
*
* Description:
*    Have the device enter Low Power State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterLowPowerState (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    smsLog(pMac, LOG2, FL("Entering pmcEnterLowPowerState\n"));

    /* If already in Low Power State, just return. */
    if (pMac->pmc.pmcState == LOW_POWER)
        return eHAL_STATUS_SUCCESS;

    /* Cancel any running timers. */
    if (palTimerStop(pMac->hHdd, pMac->pmc.hImpsTimer) != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGE, FL("Cannot cancel IMPS timer\n"));
        return eHAL_STATUS_FAILURE;
    }

    pmcStopTrafficTimer(hHal);

    if (palTimerStop(pMac->hHdd, pMac->pmc.hExitPowerSaveTimer) != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGE, FL("Cannot cancel exit power save mode timer\n"));
        return eHAL_STATUS_FAILURE;
    }

    /* Do all the callbacks. */
    pmcDoCallbacks(hHal, eHAL_STATUS_FAILURE);

    /* Change state. */
    pMac->pmc.pmcState = LOW_POWER;

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcExitLowPowerState
*
* Description:
*    Have the device exit the Low Power State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcExitLowPowerState (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    smsLog(pMac, LOG2, FL("Entering pmcExitLowPowerState\n"));

    /* Must be in Low Power State if we are going to exit that state. */
    if (pMac->pmc.pmcState != LOW_POWER)
    {
        smsLog(pMac, LOGE, FL("Cannot exit Low Power State if not in that state\n"));
        return eHAL_STATUS_FAILURE;
    }

    /* Both WLAN switches much be on to exit Low Power State. */
    if ((pMac->pmc.hwWlanSwitchState == ePMC_SWITCH_OFF) || (pMac->pmc.swWlanSwitchState == ePMC_SWITCH_OFF))
        return eHAL_STATUS_SUCCESS;

    /* Change state. */
    pMac->pmc.pmcState = FULL_POWER;
    if(pmcShouldBmpsTimerRun(pMac))
    {
        if (pmcStartTrafficTimer(hHal, pMac->pmc.bmpsConfig.trafficMeasurePeriod) != eHAL_STATUS_SUCCESS)
            return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcEnterFullPowerState
*
* Description:
*    Have the device enter the Full Power State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterFullPowerState (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    smsLog(pMac, LOG2, FL("Entering pmcEnterFullPowerState\n"));

    /* Take action based on the current state. */
    switch (pMac->pmc.pmcState)
    {

    /* Already in Full Power State. */
    case FULL_POWER:
        break;

    /* Notify everyone that we are going to full power.
       Change to Full Power State. */
    case REQUEST_FULL_POWER:
    case REQUEST_IMPS:
    case REQUEST_BMPS:
    case REQUEST_STANDBY:

        /* Change state. */
        pMac->pmc.pmcState = FULL_POWER;
        pMac->pmc.requestFullPowerPending = FALSE;

        if(pmcShouldBmpsTimerRun(pMac))
            (void)pmcStartTrafficTimer(hHal, pMac->pmc.bmpsConfig.trafficMeasurePeriod);

        pmcProcessDeferredMsg( pMac );
        /* Do all the callbacks. */
        pmcDoCallbacks(hHal, eHAL_STATUS_SUCCESS);

        /* Update registerd modules that we are entering Full Power. This is
           only way to inform modules if PMC exited a power save mode because
           of error conditions or if som other module requested full power */
        pmcDoDeviceStateUpdateCallbacks(hHal, FULL_POWER);
        break;

    /* Cannot go directly to Full Power State from these states. */
    default:
        smsLog(pMac, LOGE, FL("Trying to enter Full Power State from state %d\n"), pMac->pmc.pmcState);
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }

    smsLog(pMac, LOGW, "PMC: Enter full power done: Cancel XO Core ON vote\n");
    if (vos_chipVoteXOCore(NULL, NULL, NULL, VOS_FALSE) != VOS_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGE, "Could not cancel XO Core ON vote. Not returning failure. "
                                "Power consumed will be high\n");
    }

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcEnterRequestFullPowerState
*
* Description:
*    Have the device enter the Request Full Power State.
*
* Parameters:
*    hHal - HAL handle for device
*    fullPowerReason - Reason code for requesting full power
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterRequestFullPowerState (tHalHandle hHal, tRequestFullPowerReason fullPowerReason)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    vos_call_status_type callType;
    VOS_STATUS status;

    smsLog(pMac, LOG2, FL("Entering pmcEnterRequestFullPowerState\n"));

    /* Take action based on the current state of the device. */
    switch (pMac->pmc.pmcState)
    {

    /* Should not request full power if already there. */
    case FULL_POWER:
        smsLog(pMac, LOGE, FL("Requesting Full Power State when already there\n"));
        return eHAL_STATUS_FAILURE;

    /* Only power events can take device out of Low Power State. */
    case LOW_POWER:
        smsLog(pMac, LOGE, FL("Cannot request exit from Low Power State\n"));
        return eHAL_STATUS_FAILURE;

    /* Cannot go directly to Request Full Power state from these states.
       Record that this is pending and take care of it later. */
    case REQUEST_IMPS:
    case REQUEST_START_UAPSD:
    case REQUEST_STOP_UAPSD:
    case REQUEST_STANDBY:
    case REQUEST_BMPS:
    case REQUEST_ENTER_WOWL:
    case REQUEST_EXIT_WOWL:
        smsLog(pMac, LOGW, FL("Request for full power is being buffered. "
            "Current state is %d\n"), pMac->pmc.pmcState);
        //Ignore the new reason if request for full power is already pending
        if( !pMac->pmc.requestFullPowerPending )
        {
            pMac->pmc.requestFullPowerPending = TRUE;
            pMac->pmc.requestFullPowerReason = fullPowerReason;
        }
        return eHAL_STATUS_SUCCESS;

    /* Tell MAC to have device enter full power mode. */
    case IMPS:
        if ( pMac->pmc.rfSuppliesVotedOff )
        {
            status = vos_chipVoteOnRFSupply(&callType, NULL, NULL);
            VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ) );

            status = vos_chipVoteOnXOBuffer(&callType, NULL, NULL);
            VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ) );

            pMac->pmc.rfSuppliesVotedOff = FALSE;
        }

        if (pmcIssueCommand( pMac, eSmeCommandExitImps, NULL, 0, FALSE ) != eHAL_STATUS_SUCCESS)
        {
            return eHAL_STATUS_FAILURE;
        }
        return eHAL_STATUS_SUCCESS;

    /* Tell MAC to have device enter full power mode. */
    case BMPS:
    {
        tExitBmpsInfo exitBmpsInfo;
        exitBmpsInfo.exitBmpsReason = fullPowerReason;

        if (pmcIssueCommand(hHal, eSmeCommandExitBmps, &exitBmpsInfo, sizeof(tExitBmpsInfo), FALSE)
               != eHAL_STATUS_SUCCESS)
        {
            return eHAL_STATUS_FAILURE;
        }
        return eHAL_STATUS_SUCCESS;
    }
    /* Already in Request Full Power State. */
    case REQUEST_FULL_POWER:
        return eHAL_STATUS_SUCCESS;

    /* Tell MAC to have device enter full power mode. */
    case STANDBY:
        if ( pMac->pmc.rfSuppliesVotedOff )
        {
            status = vos_chipVoteOnXOBuffer(&callType, NULL, NULL);
            if(VOS_STATUS_SUCCESS != status)
            {
                return eHAL_STATUS_FAILURE;
            }
            status = vos_chipVoteOnRFSupply(&callType, NULL, NULL);
            if(VOS_STATUS_SUCCESS != status)
            {
                return eHAL_STATUS_FAILURE;
            }

            pMac->pmc.rfSuppliesVotedOff = FALSE;
        }

        if (pmcIssueCommand(hHal, eSmeCommandExitImps, NULL, 0, FALSE) !=
            eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGE, "PMC: failure to send message "
            "eWNI_PMC_EXIT_IMPS_REQ\n");
            return eHAL_STATUS_FAILURE;
        }
        
        return eHAL_STATUS_SUCCESS;

    /* Tell MAC to have device exit UAPSD mode first */
    case UAPSD:
        //Need to save the reason code here in case later on we need to exit BMPS as well
        if (pmcIssueCommand(hHal, eSmeCommandExitUapsd, &fullPowerReason, sizeof(tRequestFullPowerReason), FALSE) !=
            eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGE, "PMC: failure to send message "
            "eWNI_PMC_EXIT_UAPSD_REQ\n");
            return eHAL_STATUS_FAILURE;
        }
        return eHAL_STATUS_SUCCESS;

    /* Tell MAC to have device exit WOWL mode first */
    case WOWL:
        if (pmcIssueCommand(hHal, eSmeCommandExitWowl, &fullPowerReason, sizeof(tRequestFullPowerReason), FALSE) !=
            eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGP, "PMC: failure to send message "
            "eWNI_PMC_EXIT_WOWL_REQ\n");
            return eHAL_STATUS_FAILURE;
        }
        return eHAL_STATUS_SUCCESS;

    /* Cannot go directly to Request Full Power State from these states. */
    default:
        smsLog(pMac, LOGE, FL("Trying to enter Request Full Power State from state %d\n"), pMac->pmc.pmcState);
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }
}


/******************************************************************************
*
* Name:  pmcEnterRequestImpsState
*
* Description:
*    Have the device enter the Request IMPS State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterRequestImpsState (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    smsLog(pMac, LOG2, FL("Entering pmcEnterRequestImpsState\n"));

    /* Can enter Request IMPS State only from Full Power State. */
    if (pMac->pmc.pmcState != FULL_POWER)
    {
        smsLog(pMac, LOGE, FL("Trying to enter Request IMPS State from state %d\n"), pMac->pmc.pmcState);
        return eHAL_STATUS_FAILURE;
    }

    /* Make sure traffic timer that triggers bmps entry is not running */
    pmcStopTrafficTimer(hHal);

    /* Tell MAC to have device enter IMPS mode. */
    if (pmcIssueCommand(hHal, eSmeCommandEnterImps, NULL, 0, FALSE) != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGE, "PMC: failure to send message eWNI_PMC_ENTER_IMPS_REQ\n");
        pMac->pmc.pmcState = FULL_POWER;
        if(pmcShouldBmpsTimerRun(pMac))
            (void)pmcStartTrafficTimer(hHal, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
        return eHAL_STATUS_FAILURE;
     }

    smsLog(pMac, LOGW, FL("eWNI_PMC_ENTER_IMPS_REQ sent to PE\n"));

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcEnterImpsState
*
* Description:
*    Have the device enter the IMPS State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterImpsState (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    vos_call_status_type callType;
    VOS_STATUS status;
    smsLog(pMac, LOG2, FL("Entering pmcEnterImpsState\n"));

    /* Can enter IMPS State only from Request IMPS State. */
    if (pMac->pmc.pmcState != REQUEST_IMPS)
    {
        smsLog(pMac, LOGE, FL("Trying to enter IMPS State from state %d\n"), pMac->pmc.pmcState);
        return eHAL_STATUS_FAILURE;
    }

    /* Change state. */
    pMac->pmc.pmcState = IMPS;

    /* If we have a reqeust for full power pending then we have to go
       directly into full power. */
    if (pMac->pmc.requestFullPowerPending)
    {

#if defined(ANI_OS_TYPE_WINDOWS) && !defined(GEN6_ONWARDS)
        /* In Windows we cannot do this now since we are in DPC context and the message to the
           SoftMAC to put the device into IMPS will be sent at the end of DPC processing.  Instead
           we set a short timer and start the IMPS exit sequence when the timer fires. */
        if (palTimerStart(pMac->hHdd, pMac->pmc.hExitPowerSaveTimer, 1000, FALSE) != eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGE, FL("Cannot start exit power save mode timer\n"));
            PMC_ABORT;
            return eHAL_STATUS_FAILURE;
        }
        return eHAL_STATUS_SUCCESS;

#else
        /* Start exit IMPS sequence now. */
        return pmcEnterRequestFullPowerState(hHal, pMac->pmc.requestFullPowerReason);
#endif
    }

    /* Set timer to come out of IMPS.only if impsPeriod is non-Zero*/
    if(0 != pMac->pmc.impsPeriod)
    {
        if (palTimerStart(pMac->hHdd, pMac->pmc.hImpsTimer, pMac->pmc.impsPeriod * 1000, FALSE) != eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGE, FL("Cannot start IMPS timer\n"));
            PMC_ABORT;
            pmcEnterRequestFullPowerState(hHal, eSME_REASON_OTHER);
            return eHAL_STATUS_FAILURE;
        }
    }

    //Vote off RF supplies. Note RF supllies are not voted off if there is a 
    //pending request for full power already
    status = vos_chipVoteOffRFSupply(&callType, NULL, NULL);
    VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ) );

    status = vos_chipVoteOffXOBuffer(&callType, NULL, NULL);
    VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ) );

    pMac->pmc.rfSuppliesVotedOff= TRUE;

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcEnterRequestBmpsState
*
* Description:
*    Have the device enter the Request BMPS State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterRequestBmpsState (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    
    smsLog(pMac, LOG2, FL("Entering pmcEnterRequestBmpsState\n"));

    /* Can enter Request BMPS State only from Full Power State. */
    if (pMac->pmc.pmcState != FULL_POWER)
    {
        smsLog(pMac, LOGE, FL("Trying to enter Request BMPS State from state %d\n"), pMac->pmc.pmcState);
        return eHAL_STATUS_FAILURE;
    }

    /* Stop Traffic timer if running. Note: timer could have expired because of possible
       race conditions. So no need to check for errors. Just make sure timer is not running */
    pmcStopTrafficTimer(hHal);

    /* Tell MAC to have device enter BMPS mode. */
    if ( !pMac->pmc.bmpsRequestQueued )
    {
        pMac->pmc.bmpsRequestQueued = eANI_BOOLEAN_TRUE;
        if(pmcIssueCommand(hHal, eSmeCommandEnterBmps, NULL, 0, FALSE) != eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGE, "PMC: failure to send message eWNI_PMC_ENTER_BMPS_REQ\n");
            pMac->pmc.bmpsRequestQueued = eANI_BOOLEAN_FALSE;
            pMac->pmc.pmcState = FULL_POWER;
            if(pmcShouldBmpsTimerRun(pMac))
            {
                (void)pmcStartTrafficTimer(hHal, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
            }
            return eHAL_STATUS_FAILURE;
        }
    }
    else
    {
        smsLog(pMac, LOGE, "PMC: enter BMPS command already queued\n");
        //restart the timer if needed
        if(pmcShouldBmpsTimerRun(pMac))
        {
            (void)pmcStartTrafficTimer(hHal, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
        }
        return eHAL_STATUS_SUCCESS;
    }

    smsLog(pMac, LOGW, FL("eWNI_PMC_ENTER_BMPS_REQ sent to PE\n"));

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcEnterBmpsState
*
* Description:
*    Have the device enter the BMPS State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterBmpsState (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    smsLog(pMac, LOG2, FL("Entering pmcEnterBmpsState\n"));

    /* Can enter BMPS State only from 5 states. */
    if (pMac->pmc.pmcState != REQUEST_BMPS &&
        pMac->pmc.pmcState != REQUEST_START_UAPSD &&
        pMac->pmc.pmcState != REQUEST_STOP_UAPSD &&
        pMac->pmc.pmcState != REQUEST_ENTER_WOWL &&
        pMac->pmc.pmcState != REQUEST_EXIT_WOWL)
    {
        smsLog(pMac, LOGE, FL("Trying to enter BMPS State from state %d\n"), pMac->pmc.pmcState);
        return eHAL_STATUS_FAILURE;
    }

    /* Change state. */
    pMac->pmc.pmcState = BMPS;

   /* Update registerd modules that we are entering BMPS. This is
      only way to inform modules if PMC entered BMPS power save mode
      on its own because of traffic timer */
    pmcDoDeviceStateUpdateCallbacks(hHal, BMPS);

    /* If we have a reqeust for full power pending then we have to go directly into full power. */
    if (pMac->pmc.requestFullPowerPending)
    {

#if defined(ANI_OS_TYPE_WINDOWS) && !defined(GEN6_ONWARDS)
        /* In Windows, we cannot do this now since we are in DPC context and the message to the
           SoftMAC to put the device into BMPS will be sent at the end of DPC processing.  Instead
           we set a short timer and start the BMPS exit sequence when the timer fires. */
        if (palTimerStart(pMac->hHdd, pMac->pmc.hExitPowerSaveTimer, 1000, FALSE) != eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGE, FL("Cannot start exit power save mode timer\n"));
            PMC_ABORT;
            return eHAL_STATUS_FAILURE;
        }
        return eHAL_STATUS_SUCCESS;

#else
        /* Start exit BMPS sequence now. */
        smsLog(pMac, LOGW, FL("Pending Full Power request found on entering BMPS mode. "
                  "Start exit BMPS exit sequence\n"));
        //Note: Reason must have been set when requestFullPowerPending flag was set.
        pmcEnterRequestFullPowerState(hHal, pMac->pmc.requestFullPowerReason);
        return eHAL_STATUS_SUCCESS;
#endif
    }

    /*This should never happen ideally. WOWL and UAPSD not supported at the same time */
    if (pMac->pmc.wowlModeRequired && pMac->pmc.uapsdSessionRequired)
    {
        smsLog(pMac, LOGW, FL("Both UAPSD and WOWL is required on entering BMPS mode. "
               "UAPSD will be prioritized over WOWL\n"));
    }

    /* Do we need Uapsd?*/
    if (pMac->pmc.uapsdSessionRequired)
    {
        smsLog(pMac, LOGW, FL("UAPSD session is required on entering BMPS mode. "
                  "Start UAPSD entry sequence\n"));
        pmcEnterRequestStartUapsdState(hHal);
        return eHAL_STATUS_SUCCESS;
    }

    /* Do we need WOWL?*/
    if (pMac->pmc.wowlModeRequired)
    {
        smsLog(pMac, LOGW, FL("WOWL is required on entering BMPS mode. "
                  "Start WOWL entry sequence\n"));
        pmcRequestEnterWowlState(hHal, &(pMac->pmc.wowlEnterParams));
    }

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcPowerSaveCheck
*
* Description:
*    Check if device is allowed to enter a power save mode.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    TRUE - entry is allowed
*    FALSE - entry is not allowed at this time
*
******************************************************************************/
tANI_BOOLEAN pmcPowerSaveCheck (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tListElem *pEntry;
    tpPowerSaveCheckEntry pPowerSaveCheckEntry;
    tANI_BOOLEAN (*checkRoutine) (void *checkContext);
    tANI_BOOLEAN bResult=FALSE;

    smsLog(pMac, LOG2, FL("Entering pmcPowerSaveCheck\n"));

    /* Call the routines in the power save check routine list.  If any
       return FALSE, then we cannot go into power save mode. */
    pEntry = csrLLPeekHead(&pMac->pmc.powerSaveCheckList, FALSE);
    while (pEntry != NULL)
    {
        pPowerSaveCheckEntry = GET_BASE_ADDR(pEntry, tPowerSaveCheckEntry, link);
        checkRoutine = pPowerSaveCheckEntry->checkRoutine;

        /* If the checkRoutine is NULL for a paricular entry, proceed with other entries
         * in the list */
        if (NULL != checkRoutine)
        {
            if (!checkRoutine(pPowerSaveCheckEntry->checkContext))
            {
                smsLog(pMac, LOGE, FL("pmcPowerSaveCheck fail!\n"));
                bResult = FALSE; 
                break;
            }
            else
            {
                bResult = TRUE;
            }
        }
        pEntry = csrLLNext(&pMac->pmc.powerSaveCheckList, pEntry, FALSE);
    }

    return bResult;
}


/******************************************************************************
*
* Name:  pmcSendPowerSaveConfigMessage
*
* Description:
*    Send a message to PE/MAC to configure the power saving modes.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - message successfuly sent
*    eHAL_STATUS_FAILURE - error while sending message
*
******************************************************************************/
eHalStatus pmcSendPowerSaveConfigMessage (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tSirPowerSaveCfg powerSaveConfig;

    smsLog(pMac, LOG2, FL("Entering pmcSendPowerSaveConfigMessage\n"));
    
    palZeroMemory(pMac->hHdd, &(powerSaveConfig), sizeof(tSirPowerSaveCfg));

    switch (pMac->pmc.bmpsConfig.forwardBeacons)
    {
    case ePMC_NO_BEACONS:
        powerSaveConfig.beaconFwd = ePM_BEACON_FWD_NONE;
        break;
    case ePMC_BEACONS_WITH_TIM_SET:
        powerSaveConfig.beaconFwd = ePM_BEACON_FWD_TIM;
        break;
    case ePMC_BEACONS_WITH_DTIM_SET:
        powerSaveConfig.beaconFwd = ePM_BEACON_FWD_DTIM;
        break;
    case ePMC_NTH_BEACON:
        powerSaveConfig.beaconFwd = ePM_BEACON_FWD_NTH;
        powerSaveConfig.nthBeaconFwd = (tANI_U16)pMac->pmc.bmpsConfig.valueOfN;
        break;
    case ePMC_ALL_BEACONS:
        powerSaveConfig.beaconFwd = ePM_BEACON_FWD_NTH;
        powerSaveConfig.nthBeaconFwd = 1;
        break;
    }
    powerSaveConfig.fEnablePwrSaveImmediately = pMac->pmc.bmpsConfig.setPmOnLastFrame;
    powerSaveConfig.fPSPoll = pMac->pmc.bmpsConfig.usePsPoll;
    powerSaveConfig.fEnableBeaconEarlyTermination = 
        pMac->pmc.bmpsConfig.enableBeaconEarlyTermination;
    powerSaveConfig.bcnEarlyTermWakeInterval = 
        pMac->pmc.bmpsConfig.bcnEarlyTermWakeInterval;

    /* setcfg for listenInterval. Make sure CFG is updated because PE reads this 
       from CFG at the time of assoc or reassoc */
    ccmCfgSetInt(pMac, WNI_CFG_LISTEN_INTERVAL, pMac->pmc.bmpsConfig.bmpsPeriod, 
        NULL, eANI_BOOLEAN_FALSE);

    if( pMac->pmc.pmcState == IMPS || pMac->pmc.pmcState == REQUEST_IMPS )
    {
        //Wake up the chip first
        eHalStatus status = pmcDeferMsg( pMac, eWNI_PMC_PWR_SAVE_CFG, 
                                    &powerSaveConfig, sizeof(tSirPowerSaveCfg) );

        if( eHAL_STATUS_PMC_PENDING == status )
        {
            return eHAL_STATUS_SUCCESS;
        }
        else 
        {
            //either fail or already in full power
            if( !HAL_STATUS_SUCCESS( status ) )
            {
                return ( status );
            }
            //else let it through because it is in full power state
        }
    }
    /* Send a message so that FW System config is also updated and is in sync with
       the CFG.*/
    if (pmcSendMessage(hHal, eWNI_PMC_PWR_SAVE_CFG, &powerSaveConfig, sizeof(tSirPowerSaveCfg)) 
        != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGE, FL("Send of eWNI_PMC_PWR_SAVE_CFG to PE failed\n"));
        return eHAL_STATUS_FAILURE;
    }
   
    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcSendMessage
*
* Description:
*    Send a message to PE/MAC.
*
* Parameters:
*    hHal - HAL handle for device
*    messageType - message type to send
*    pMessageData - pointer to message data
*    messageSize - Size of the message data
*
* Returns:
*    eHAL_STATUS_SUCCESS - message successfuly sent
*    eHAL_STATUS_FAILURE - error while sending message
*
******************************************************************************/
eHalStatus pmcSendMessage (tpAniSirGlobal pMac, tANI_U16 messageType, void *pMessageData, tANI_U32 messageSize)
{
    tSirMbMsg *pMsg;

    smsLog(pMac, LOG2, FL("Entering pmcSendMessage, message type %d\n"), messageType);

    /* Allocate and fill in message. */
    if (palAllocateMemory(pMac->hHdd, (void **)&pMsg, WNI_CFG_MB_HDR_LEN + messageSize) != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGE, FL("Cannot allocate memory for message\n"));
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }
    pMsg->type = messageType;
    pMsg->msgLen = (tANI_U16) (WNI_CFG_MB_HDR_LEN + messageSize);
    if (messageSize > 0)
    {
        if (palCopyMemory(pMac->hHdd, pMsg->data, pMessageData, messageSize) != eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGE, FL("Cannot copy message data\n"));
            PMC_ABORT;
            return eHAL_STATUS_FAILURE;
        }
    }

    /* Send message. */
    if (palSendMBMessage(pMac->hHdd, pMsg) != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGE, FL("Cannot send message\n"));
        PMC_ABORT;
        return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcDoCallbacks
*
* Description:
*    Call the IMPS callback routine and the routines in the request full
*    power callback routine list.
*
* Parameters:
*    hHal - HAL handle for device
*    callbackStatus - status to pass to the callback routines
*
* Returns:
*    nothing
*
******************************************************************************/
void pmcDoCallbacks (tHalHandle hHal, eHalStatus callbackStatus)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tListElem *pEntry;
    tpRequestFullPowerEntry pRequestFullPowerEntry;

    smsLog(pMac, LOG2, FL("Entering pmcDoCallbacks\n"));

    /* Call IMPS callback routine. */
    if (pMac->pmc.impsCallbackRoutine != NULL)
    {
        pMac->pmc.impsCallbackRoutine(pMac->pmc.impsCallbackContext, callbackStatus);
        pMac->pmc.impsCallbackRoutine = NULL;
    }

    /* Call the routines in the request full power callback routine list. */
    while (NULL != (pEntry = csrLLRemoveHead(&pMac->pmc.requestFullPowerList, TRUE)))
    {
        pRequestFullPowerEntry = GET_BASE_ADDR(pEntry, tRequestFullPowerEntry, link);
        if (pRequestFullPowerEntry->callbackRoutine)
           pRequestFullPowerEntry->callbackRoutine(pRequestFullPowerEntry->callbackContext, callbackStatus);
        if (palFreeMemory(pMac->hHdd, pRequestFullPowerEntry) != eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGE, FL("Cannot free request full power routine list entry\n"));
            PMC_ABORT;
        }
    }

}


/******************************************************************************
*
* Name:  pmcStartTrafficTimer
*
* Description:
*    Start the timer used in Full Power State to measure traffic
*    levels and determine when to enter BMPS.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - timer successfuly started
*    eHAL_STATUS_FAILURE - error while starting timer
*
******************************************************************************/
eHalStatus pmcStartTrafficTimer (tHalHandle hHal, tANI_U32 expirationTime)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    VOS_STATUS vosStatus;

    smsLog(pMac, LOG2, FL("Entering pmcStartTrafficTimer\n"));

    vosStatus = vos_timer_start(&pMac->pmc.hTrafficTimer, expirationTime);
    if ( !VOS_IS_STATUS_SUCCESS(vosStatus) )
    {
        if( VOS_STATUS_E_ALREADY == vosStatus )
        {
            //Consider this ok since the timer is already started.
            smsLog(pMac, LOGW, FL("  traffic timer is already started\n"));
        }
        else
        {
            smsLog(pMac, LOGP, FL("Cannot start traffic timer\n"));
            return eHAL_STATUS_FAILURE;
        }
    }

    return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcStopTrafficTimer
*
* Description:
*    Cancels the timer (if running) used in Full Power State to measure traffic
*    levels and determine when to enter BMPS.
*
* Parameters:
*    hHal - HAL handle for device
*
*
******************************************************************************/
void pmcStopTrafficTimer (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    smsLog(pMac, LOG2, FL("Entering pmcStopTrafficTimer\n"));
    vos_timer_stop(&pMac->pmc.hTrafficTimer);
}


/******************************************************************************
*
* Name:  pmcImpsTimerExpired
*
* Description:
*    Called when IMPS timer expires.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    nothing
*
******************************************************************************/
void pmcImpsTimerExpired (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    smsLog(pMac, LOG2, FL("Entering pmcImpsTimerExpired\n"));

    /* If timer expires and we are in a state other than IMPS State then something is wrong. */
    if (pMac->pmc.pmcState != IMPS)
    {
        smsLog(pMac, LOGE, FL("Got IMPS timer expiration in state %d\n"), pMac->pmc.pmcState);
        PMC_ABORT;
        return;
    }

    /* Start on the path of going back to full power. */
    pmcEnterRequestFullPowerState(hHal, eSME_REASON_OTHER);
}


/******************************************************************************
*
* Name:  pmcTrafficTimerExpired
*
* Description:
*    Called when traffic measurement timer expires.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    nothing
*
******************************************************************************/
void pmcTrafficTimerExpired (tHalHandle hHal)
{

    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    VOS_STATUS vosStatus;

    smsLog(pMac, LOGW, FL("BMPS Traffic timer expired"));

    /* If timer expires and we are in a state other than Full Power State then something is wrong. */
    if (pMac->pmc.pmcState != FULL_POWER)
    {
        smsLog(pMac, LOGE, FL("Got traffic timer expiration in state %d"), pMac->pmc.pmcState);
        return;
    }

    /* Untill DHCP is not completed remain in power active */
    if(pMac->pmc.remainInPowerActiveTillDHCP)
    {
        smsLog(pMac, LOGE, FL("BMPS Traffic Timer expired before DHCP completion ignore enter BMPS\n"));
        pMac->pmc.remainInPowerActiveThreshold++;
        if( pMac->pmc.remainInPowerActiveThreshold >= DHCP_REMAIN_POWER_ACTIVE_THRESHOLD)
        {
           smsLog(pMac, LOGE, FL("Remain in power active DHCP threshold reached FALLBACK to enable enter BMPS\n"));
           /*FALLBACK: reset the flag to make BMPS entry possible*/
           pMac->pmc.remainInPowerActiveTillDHCP = FALSE;
           pMac->pmc.remainInPowerActiveThreshold = 0;
        }
        //Activate the Traffic Timer again for entering into BMPS
        vosStatus = vos_timer_start(&pMac->pmc.hTrafficTimer, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
        if ( !VOS_IS_STATUS_SUCCESS(vosStatus) && (VOS_STATUS_E_ALREADY != vosStatus) )
        {
            smsLog(pMac, LOGP, FL("Cannot re-start traffic timer\n"));
        }
        return;
    }

    /* Clear remain in power active threshold */
    pMac->pmc.remainInPowerActiveThreshold = 0;

    /* Check if the timer should be running */
    if (!pmcShouldBmpsTimerRun(pMac))
    {
        smsLog(pMac, LOGE, FL("BMPS timer should not be running"));
        return;
    }

    if (pmcPowerSaveCheck(hHal)) 
    {
        smsLog(pMac, LOGW, FL("BMPS entry criteria satisfied. Requesting BMPS state"));
        (void)pmcEnterRequestBmpsState(hHal);    
    } 
    else 
    {
        /*Some module voted against Power Save. So timer should be restarted again to retry BMPS */
        smsLog(pMac, LOGE, FL("Power Save check failed. Retry BMPS again later"));
        //Since hTrafficTimer is a vos_timer now, we need to restart the timer here
        vosStatus = vos_timer_start(&pMac->pmc.hTrafficTimer, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
        if ( !VOS_IS_STATUS_SUCCESS(vosStatus) && (VOS_STATUS_E_ALREADY != vosStatus) )
        {
            smsLog(pMac, LOGP, FL("Cannot start traffic timer\n"));
            return;
        }
    }
}


/******************************************************************************
*
* Name:  pmcExitPowerSaveTimerExpired
*
* Description:
*    Called when timer used to schedule a deferred power save mode exit expires.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    nothing
*
******************************************************************************/
void pmcExitPowerSaveTimerExpired (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    smsLog(pMac, LOG2, FL("Entering pmcExitPowerSaveTimerExpired\n"));

    /* Make sure process of exiting power save mode might hasn't already been started due to another trigger. */
    if (pMac->pmc.requestFullPowerPending)

        /* Start on the path of going back to full power. */
        pmcEnterRequestFullPowerState(hHal, pMac->pmc.requestFullPowerReason);
}

/******************************************************************************
*
* Name:  pmcDoBmpsCallbacks
*
* Description:
*    Call the registered BMPS callback routines because device is unable to
*    enter BMPS state
*
* Parameters:
*    hHal - HAL handle for device
*    callbackStatus - Success or Failure.
*
* Returns:
*    nothing
*
******************************************************************************/
void pmcDoBmpsCallbacks (tHalHandle hHal, eHalStatus callbackStatus)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   tListElem *pEntry;
   tpRequestBmpsEntry pRequestBmpsEntry;

   smsLog(pMac, LOG2, "PMC: entering pmcDoBmpsCallbacks\n");

   /* Call the routines in the request BMPS callback routine list. */
   pEntry = csrLLRemoveHead(&pMac->pmc.requestBmpsList, FALSE);
   while (pEntry != NULL)
   {
      pRequestBmpsEntry = GET_BASE_ADDR(pEntry, tRequestBmpsEntry, link);
      if (pRequestBmpsEntry->callbackRoutine)
         pRequestBmpsEntry->callbackRoutine(pRequestBmpsEntry->callbackContext,
         callbackStatus);
      palFreeMemory(pMac->hHdd, pRequestBmpsEntry);
      pEntry = csrLLRemoveHead(&pMac->pmc.requestBmpsList, FALSE);
   }
}




/******************************************************************************
*
* Name:  pmcDoStartUapsdCallbacks
*
* Description:
*    Call the registered UAPSD callback routines because device is unable to
*    start UAPSD state
*
* Parameters:
*    hHal - HAL handle for device
*    callbackStatus - Success or Failure. 
*
* Returns:
*    nothing
*
******************************************************************************/
void pmcDoStartUapsdCallbacks (tHalHandle hHal, eHalStatus callbackStatus)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   tListElem *pEntry;
   tpStartUapsdEntry pStartUapsdEntry;

   smsLog(pMac, LOG2, "PMC: entering pmcDoStartUapsdCallbacks\n");

   /* Call the routines in the request start UAPSD callback routine list. */
   pEntry = csrLLRemoveHead(&pMac->pmc.requestStartUapsdList, FALSE);
   while (pEntry != NULL)
   {
      pStartUapsdEntry = GET_BASE_ADDR(pEntry, tStartUapsdEntry, link);
      pStartUapsdEntry->callbackRoutine(pStartUapsdEntry->callbackContext,
         callbackStatus);
      palFreeMemory(pMac->hHdd, pStartUapsdEntry);
      pEntry = csrLLRemoveHead(&pMac->pmc.requestStartUapsdList, FALSE);
   }
}

/******************************************************************************
*
* Name:  pmcEnterRequestStartUapsdState
*
* Description:
*    Have the device enter the UAPSD State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterRequestStartUapsdState (tHalHandle hHal)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   v_BOOL_t fFullPower = VOS_FALSE;     //need to get back to full power state

   smsLog(pMac, LOG2, "PMC: entering pmcEnterRequestStartUapsdState\n");

   /* Can enter UAPSD State only from FULL_POWER or BMPS State. */
   switch (pMac->pmc.pmcState)
   {
      case FULL_POWER:
         /* Check that entry into a power save mode is allowed at this time. */
         if (!pmcPowerSaveCheck(hHal))
         {
            smsLog(pMac, LOGW, "PMC: Power save check failed. UAPSD request "
                      "will be accepted and buffered\n");
            /* UAPSD mode will be attempted when we enter BMPS later */
            pMac->pmc.uapsdSessionRequired = TRUE;
            /* Make sure the BMPS retry timer is running */
            if(pmcShouldBmpsTimerRun(pMac))
               (void)pmcStartTrafficTimer(hHal, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
            break;
         }
         else
         {
            pMac->pmc.uapsdSessionRequired = TRUE;
            //Check BTC state
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
            if( btcIsReadyForUapsd( pMac ) )
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
            {
               /* Put device in BMPS mode first. This step should NEVER fail.
                  That is why no need to buffer the UAPSD request*/
               if(pmcEnterRequestBmpsState(hHal) != eHAL_STATUS_SUCCESS)
               {
                   smsLog(pMac, LOGE, "PMC: Device in Full Power. Enter Request Bmps failed. "
                            "UAPSD request will be dropped \n");
                  return eHAL_STATUS_FAILURE;
               }
            }
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
            else
            {
               (void)pmcStartTrafficTimer(hHal, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
            }
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
         }
         break;

      case BMPS:
         //It is already in BMPS mode, check BTC state
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
         if( btcIsReadyForUapsd(pMac) )
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
         {
            /* Tell MAC to have device enter UAPSD mode. */
            if (pmcIssueCommand(hHal, eSmeCommandEnterUapsd, NULL, 0, FALSE) !=
               eHAL_STATUS_SUCCESS)
            {
               smsLog(pMac, LOGE, "PMC: failure to send message "
                  "eWNI_PMC_ENTER_BMPS_REQ\n");
               return eHAL_STATUS_FAILURE;
            }
         }
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
         else
         {
            //Not ready for UAPSD at this time, save it first and wake up the chip
            smsLog(pMac, LOGE, " PMC state = %d\n",pMac->pmc.pmcState);
            pMac->pmc.uapsdSessionRequired = TRUE;
            /* While BTC traffic is going on, STA can be in BMPS 
             * and need not go to Full Power */
            //fFullPower = VOS_TRUE; 
         }
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
         break;

      case REQUEST_START_UAPSD:
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
         if( !btcIsReadyForUapsd(pMac) )
         {
            //BTC rejects UAPSD, bring it back to full power
            fFullPower = VOS_TRUE;
         }
#endif
         break;

      case REQUEST_BMPS:
        /* Buffer request for UAPSD mode. */
        pMac->pmc.uapsdSessionRequired = TRUE;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
        if( !btcIsReadyForUapsd(pMac) )
         {
            //BTC rejects UAPSD, bring it back to full power
            fFullPower = VOS_TRUE;
         }
#endif /* WLAN_MDM_CODE_REDUCTION_OPT*/
        break;

      default:
         smsLog(pMac, LOGE, "PMC: trying to enter UAPSD State from state %d\n",
            pMac->pmc.pmcState);
         return eHAL_STATUS_FAILURE;
   }

   if(fFullPower)
   {
      if( eHAL_STATUS_PMC_PENDING != pmcRequestFullPower( pMac, NULL, NULL, eSME_REASON_OTHER ) )
      {
         //This is an error
         smsLog(pMac, LOGE, FL(" fail to request full power because BTC\n"));
      }
   }

   return eHAL_STATUS_SUCCESS;
}

/******************************************************************************
*
* Name:  pmcEnterUapsdState
*
* Description:
*    Have the device enter the UAPSD State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterUapsdState (tHalHandle hHal)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

   smsLog(pMac, LOG2, "PMC: entering pmcEnterUapsdState\n");

   /* Can enter UAPSD State only from Request UAPSD State. */
   if (pMac->pmc.pmcState != REQUEST_START_UAPSD )
   {
      smsLog(pMac, LOGE, "PMC: trying to enter UAPSD State from state %d\n",
      pMac->pmc.pmcState);
      return eHAL_STATUS_FAILURE;
   }

   /* Change state. */
   pMac->pmc.pmcState = UAPSD;

   /* Update registerd modules that we are entering UAPSD. This is
      only way to inform modules if PMC resumed UAPSD power save mode
      on its own after full power mode */
   pmcDoDeviceStateUpdateCallbacks(hHal, UAPSD);
   
   /* If we have a reqeust for full power pending then we have to go
   directly into full power. */
   if (pMac->pmc.requestFullPowerPending)
   {
      /* Start exit UAPSD sequence now. */
      return pmcEnterRequestFullPowerState(hHal, pMac->pmc.requestFullPowerReason);
   }

   return eHAL_STATUS_SUCCESS;
}


/******************************************************************************
*
* Name:  pmcEnterRequestStopUapsdState
*
* Description:
*    Have the device Stop the UAPSD State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterRequestStopUapsdState (tHalHandle hHal)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

   smsLog(pMac, LOG2, "PMC: entering pmcEnterRequestStopUapsdState\n");

   /* If already in REQUEST_STOP_UAPSD, simply return */
   if (pMac->pmc.pmcState == REQUEST_STOP_UAPSD)
   {
      return eHAL_STATUS_SUCCESS;
   }

   /* Can enter Request Stop UAPSD State only from UAPSD */
   if (pMac->pmc.pmcState != UAPSD)
   {
      smsLog(pMac, LOGE, "PMC: trying to enter Request Stop UAPSD State from "
         "state %d\n", pMac->pmc.pmcState);
      return eHAL_STATUS_FAILURE;
   }

   /* Tell MAC to have device exit UAPSD mode. */
   if (pmcIssueCommand(hHal, eSmeCommandExitUapsd, NULL, 0, FALSE) !=
      eHAL_STATUS_SUCCESS)
   {
      smsLog(pMac, LOGE, "PMC: failure to send message "
         "eWNI_PMC_EXIT_UAPSD_REQ\n");
      return eHAL_STATUS_FAILURE;
   }

   return eHAL_STATUS_SUCCESS;
}

/******************************************************************************
*
* Name:  pmcEnterRequestStandbyState
*
* Description:
*    Have the device enter the Request STANDBY State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterRequestStandbyState (tHalHandle hHal)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

   smsLog(pMac, LOG2, "PMC: entering pmcEnterRequestStandbyState\n");

   /* Can enter Standby State only from Full Power State. */
   if (pMac->pmc.pmcState != FULL_POWER)
   {
      smsLog(pMac, LOGE, "PMC: trying to enter Standby State from "
         "state %d\n", pMac->pmc.pmcState);
      return eHAL_STATUS_FAILURE;
   }

   // Stop traffic timer. Just making sure timer is not running
   pmcStopTrafficTimer(hHal);

   /* Tell MAC to have device enter STANDBY mode. We are using the same message
      as IMPS mode to avoid code changes in layer below (PE/HAL)*/
   if (pmcIssueCommand(hHal, eSmeCommandEnterStandby, NULL, 0, FALSE) !=
      eHAL_STATUS_SUCCESS)
   {
      smsLog(pMac, LOGE, "PMC: failure to send message "
         "eWNI_PMC_ENTER_IMPS_REQ\n");
      pMac->pmc.pmcState = FULL_POWER;

      if(pmcShouldBmpsTimerRun(pMac))
          (void)pmcStartTrafficTimer(hHal, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
      return eHAL_STATUS_FAILURE;
   }
   
   return eHAL_STATUS_SUCCESS;
}

/******************************************************************************
*
* Name:  pmcEnterStandbyState
*
* Description:
*    Have the device enter the STANDBY State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterStandbyState (tHalHandle hHal)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   vos_call_status_type callType;
   VOS_STATUS status;

   smsLog(pMac, LOG2, "PMC: entering pmcEnterStandbyState\n");

   /* Can enter STANDBY State only from REQUEST_STANDBY State. */
   if (pMac->pmc.pmcState != REQUEST_STANDBY)
   {
      smsLog(pMac, LOGE, "PMC: trying to enter STANDBY State from state %d\n",
         pMac->pmc.pmcState);
      return eHAL_STATUS_FAILURE;
   }

   /* Change state. */
   pMac->pmc.pmcState = STANDBY;

   /* If we have a reqeust for full power pending then we have to go
      directly into full power. */
   if (pMac->pmc.requestFullPowerPending)
   {
      /* Start exit STANDBY sequence now. */
      return pmcEnterRequestFullPowerState(hHal, pMac->pmc.requestFullPowerReason);
   }

   //Note that RF supplies are not voted off if there is already a pending request
   //for full power
   status = vos_chipVoteOffRFSupply(&callType, NULL, NULL);
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ) );

   status = vos_chipVoteOffXOBuffer(&callType, NULL, NULL);
   VOS_ASSERT( VOS_IS_STATUS_SUCCESS( status ) );

   pMac->pmc.rfSuppliesVotedOff= TRUE;

   return eHAL_STATUS_SUCCESS;
}

/******************************************************************************
*
* Name:  pmcDoStandbyCallbacks
*
* Description:
*    Call the registered Standby callback routines 
*
* Parameters:
*    hHal - HAL handle for device
*    callbackStatus - Success or Failure.
*
* Returns:
*    nothing
*
******************************************************************************/
void pmcDoStandbyCallbacks (tHalHandle hHal, eHalStatus callbackStatus)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
 
   smsLog(pMac, LOG2, "PMC: entering pmcDoStandbyCallbacks\n");

   /* Call Standby callback routine. */
   if (pMac->pmc.standbyCallbackRoutine != NULL)
      pMac->pmc.standbyCallbackRoutine(pMac->pmc.standbyCallbackContext, callbackStatus);
   pMac->pmc.standbyCallbackRoutine = NULL;
   pMac->pmc.standbyCallbackContext = NULL;
}

/******************************************************************************
*
* Name:  pmcGetPmcState
*
* Description:
*    Return the PMC state
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    tPmcState (one of IMPS, REQUEST_IMPS, BMPS, REQUEST_BMPS etc)
*
******************************************************************************/
tPmcState pmcGetPmcState (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    return pMac->pmc.pmcState; 
}

const char* pmcGetPmcStateStr(tPmcState state)
{
    switch(state)
    {
        case STOPPED:
            return "STOPPED";
        case FULL_POWER:
            return "FULL_POWER";
        case LOW_POWER:
            return "LOW_POWER";
        case IMPS:
            return "IMPS";
        case BMPS:
            return "BMPS";
        case UAPSD:
            return "UAPSD";
        case STANDBY:
            return "STANDBY";
        case REQUEST_IMPS:
            return "REQUEST_IMPS";
        case REQUEST_BMPS:
            return "REQUEST_BMPS";
        case REQUEST_START_UAPSD:
            return "REQUEST_START_UAPSD";
        case REQUEST_STOP_UAPSD:
            return "REQUEST_STOP_UAPSD";
        case REQUEST_FULL_POWER:
            return "REQUEST_FULL_POWER";
        case REQUEST_STANDBY:
            return "REQUEST_STANDBY";
        case REQUEST_ENTER_WOWL:
            return "REQUEST_ENTER_WOWL";
        case REQUEST_EXIT_WOWL:
            return "REQUEST_EXIT_WOWL";
        case WOWL:
            return "WOWL";
        default:
            break;
    }

    return "UNKNOWN";
}

void pmcDoDeviceStateUpdateCallbacks (tHalHandle hHal, tPmcState state)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tListElem *pEntry;
    tpDeviceStateUpdateIndEntry pDeviceStateUpdateIndEntry;
    void (*callbackRoutine) (void *callbackContext, tPmcState pmcState);

    smsLog(pMac, LOG2, FL("PMC - Update registered modules of new device "
           "state: %s\n"), pmcGetPmcStateStr(state));

    /* Call the routines in the update device state routine list. */
    pEntry = csrLLPeekHead(&pMac->pmc.deviceStateUpdateIndList, FALSE);
    while (pEntry != NULL)
    {
        pDeviceStateUpdateIndEntry = GET_BASE_ADDR(pEntry, tDeviceStateUpdateIndEntry, link);
        callbackRoutine = pDeviceStateUpdateIndEntry->callbackRoutine;
        callbackRoutine(pDeviceStateUpdateIndEntry->callbackContext, state);
        pEntry = csrLLNext(&pMac->pmc.deviceStateUpdateIndList, pEntry, FALSE);
    }
}

/******************************************************************************
*
* Name:  pmcRequestEnterWowlState
*
* Description:
*    Have the device enter the WOWL State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - WOWL mode will be entered
*    eHAL_STATUS_FAILURE - WOWL mode cannot be entered
*
******************************************************************************/
eHalStatus pmcRequestEnterWowlState(tHalHandle hHal, tpSirSmeWowlEnterParams wowlEnterParams)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   smsLog(pMac, LOG2, "PMC: entering pmcRequestEnterWowlState\n");

   switch (pMac->pmc.pmcState)
   {
      case FULL_POWER:
         /* Put device in BMPS mode first. This step should NEVER fail. */
         if(pmcEnterRequestBmpsState(hHal) != eHAL_STATUS_SUCCESS)
         {
            smsLog(pMac, LOGE, "PMC: Device in Full Power. pmcEnterRequestBmpsState failed. "
                    "Cannot enter WOWL\n");
            return eHAL_STATUS_FAILURE;
         }
         break;

      case REQUEST_BMPS:
         smsLog(pMac, LOGW, "PMC: BMPS transaction going on. WOWL request "
                    "will be buffered\n");         
         break;

      case BMPS:
      case WOWL:
         /* Tell MAC to have device enter WOWL mode. Note: We accept WOWL request
            when we are in WOWL mode. This allows HDD to change WOWL configuration
            without having to exit WOWL mode */
         if (pmcIssueCommand(hHal, eSmeCommandEnterWowl, wowlEnterParams, sizeof(tSirSmeWowlEnterParams), FALSE) !=
            eHAL_STATUS_SUCCESS)
         {
            smsLog(pMac, LOGE, "PMC: failure to send message eWNI_PMC_ENTER_WOWL_REQ\n");
            return eHAL_STATUS_FAILURE;
         }
         break;

      case REQUEST_ENTER_WOWL:
         //Multiple enter WOWL requests at the same time are not accepted
         smsLog(pMac, LOGE, "PMC: Enter WOWL transaction already going on. New WOWL request "
                    "will be rejected\n");
         return eHAL_STATUS_FAILURE;

      case REQUEST_EXIT_WOWL:
         smsLog(pMac, LOGW, "PMC: Exit WOWL transaction going on. New WOWL request "
                   "will be buffered\n");         
         break;

      default:
         smsLog(pMac, LOGE, "PMC: Trying to enter WOWL State from state %s\n",
            pmcGetPmcStateStr(pMac->pmc.pmcState));
         return eHAL_STATUS_FAILURE;
   }

   return eHAL_STATUS_SUCCESS;
}

/******************************************************************************
*
* Name:  pmcEnterWowlState
*
* Description:
*    Have the device enter the WOWL State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - changing state successful
*    eHAL_STATUS_FAILURE - changing state not successful
*
******************************************************************************/
eHalStatus pmcEnterWowlState (tHalHandle hHal)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

   smsLog(pMac, LOG2, "PMC: entering pmcEnterWowlState\n");

   /* Can enter WOWL State only from Request WOWL State. */
   if (pMac->pmc.pmcState != REQUEST_ENTER_WOWL )
   {
      smsLog(pMac, LOGP, "PMC: trying to enter WOWL State from state %d\n",
        pMac->pmc.pmcState);
      return eHAL_STATUS_FAILURE;
   }

   /* Change state. */
   pMac->pmc.pmcState = WOWL;

   /* Clear the buffered command for WOWL */
   pMac->pmc.wowlModeRequired = FALSE;

   /* If we have a reqeust for full power pending then we have to go
   directly into full power. */
   if (pMac->pmc.requestFullPowerPending)
   {
      /* Start exit Wowl sequence now. */
      return pmcEnterRequestFullPowerState(hHal, pMac->pmc.requestFullPowerReason);
   }

   return eHAL_STATUS_SUCCESS;
}

/******************************************************************************
*
* Name:  pmcRequestExitWowlState
*
* Description:
*    Have the device exit WOWL State.
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns:
*    eHAL_STATUS_SUCCESS - Exit WOWL successful
*    eHAL_STATUS_FAILURE - Exit WOWL unsuccessful
*
******************************************************************************/
eHalStatus pmcRequestExitWowlState(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    smsLog(pMac, LOG2, "PMC: entering pmcRequestExitWowlState\n");

    switch (pMac->pmc.pmcState)
    {
        case WOWL:
            /* Tell MAC to have device exit WOWL mode. */
            if (pmcIssueCommand(hHal, eSmeCommandExitWowl, NULL, 0, FALSE) !=
                eHAL_STATUS_SUCCESS)
            {
                smsLog(pMac, LOGP, "PMC: failure to send message eWNI_PMC_EXIT_WOWL_REQ\n");
                return eHAL_STATUS_FAILURE;
            }
            break;

        case REQUEST_ENTER_WOWL:
            smsLog(pMac, LOGP, "PMC: Rcvd exit WOWL even before enter WOWL was completed\n");
            return eHAL_STATUS_FAILURE;   

        default:
            smsLog(pMac, LOGW, "PMC: Got exit WOWL in state %s. Nothing to do as already out of WOWL\n",
            pmcGetPmcStateStr(pMac->pmc.pmcState));
            break;
    }

    return eHAL_STATUS_SUCCESS;
}

/******************************************************************************
*
* Name:  pmcDoEnterWowlCallbacks
*
* Description:
*    Invoke Enter WOWL callbacks
*
* Parameters:
*    hHal - HAL handle for device
*
* Returns: None
*
******************************************************************************/
void pmcDoEnterWowlCallbacks (tHalHandle hHal, eHalStatus callbackStatus)
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
 
   smsLog(pMac, LOG2, "PMC: entering pmcDoWowlCallbacks\n");

   /* Call Wowl callback routine. */
   if (pMac->pmc.enterWowlCallbackRoutine != NULL)
      pMac->pmc.enterWowlCallbackRoutine(pMac->pmc.enterWowlCallbackContext, callbackStatus);

   pMac->pmc.enterWowlCallbackRoutine = NULL;
   pMac->pmc.enterWowlCallbackContext = NULL;
}


static void pmcProcessDeferredMsg( tpAniSirGlobal pMac )
{
    tPmcDeferredMsg *pDeferredMsg;
    tListElem *pEntry;

    while( NULL != ( pEntry = csrLLRemoveHead( &pMac->pmc.deferredMsgList, eANI_BOOLEAN_TRUE ) ) )
    {
        pDeferredMsg = GET_BASE_ADDR( pEntry, tPmcDeferredMsg, link );
        switch (pDeferredMsg->messageType)
        {
        case eWNI_PMC_WOWL_ADD_BCAST_PTRN:
            VOS_ASSERT( pDeferredMsg->size == sizeof(tSirWowlAddBcastPtrn) );
            if (pmcSendMessage(pMac, eWNI_PMC_WOWL_ADD_BCAST_PTRN, 
                    &pDeferredMsg->u.wowlAddPattern, sizeof(tSirWowlAddBcastPtrn))
                    != eHAL_STATUS_SUCCESS)
            {
                smsLog(pMac, LOGE, FL("Send of eWNI_PMC_WOWL_ADD_BCAST_PTRN to PE failed\n"));
            }
            break;

        case eWNI_PMC_WOWL_DEL_BCAST_PTRN:
            VOS_ASSERT( pDeferredMsg->size == sizeof(tSirWowlDelBcastPtrn) );
            if (pmcSendMessage(pMac, eWNI_PMC_WOWL_DEL_BCAST_PTRN, 
                    &pDeferredMsg->u.wowlDelPattern, sizeof(tSirWowlDelBcastPtrn))
                    != eHAL_STATUS_SUCCESS)
            {
                smsLog(pMac, LOGE, FL("Send of eWNI_PMC_WOWL_ADD_BCAST_PTRN to PE failed\n"));
            }
            break;

        case eWNI_PMC_PWR_SAVE_CFG:
            VOS_ASSERT( pDeferredMsg->size == sizeof(tSirPowerSaveCfg) );
            if (pmcSendMessage(pMac, eWNI_PMC_PWR_SAVE_CFG, 
                    &pDeferredMsg->u.powerSaveConfig, sizeof(tSirPowerSaveCfg)) 
                != eHAL_STATUS_SUCCESS)
            {
                smsLog(pMac, LOGE, FL("Send of eWNI_PMC_PWR_SAVE_CFG to PE failed\n"));
            }
            break;

        default:
            smsLog(pMac, LOGE, FL("unknown message (%d)\n"), pDeferredMsg->messageType);
            break;
        }
        //Need to free the memory here
        palFreeMemory( pMac->hHdd, pDeferredMsg );
    } //while
}


eHalStatus pmcDeferMsg( tpAniSirGlobal pMac, tANI_U16 messageType, void *pData, tANI_U32 size)
{
    tPmcDeferredMsg *pDeferredMsg;
    eHalStatus status;

    if( !HAL_STATUS_SUCCESS( palAllocateMemory( pMac->hHdd, (void **)&pDeferredMsg, sizeof(tPmcDeferredMsg) ) ) )
    {
        smsLog(pMac, LOGE, FL("Cannot allocate memory for callback context\n"));
        return eHAL_STATUS_RESOURCES;
    }
    palZeroMemory( pMac->hHdd, pDeferredMsg, sizeof(tPmcDeferredMsg) );
    pDeferredMsg->messageType = messageType;
    pDeferredMsg->size = (tANI_U16)size;
    if( pData )
    {
        if( !HAL_STATUS_SUCCESS( palCopyMemory( pMac->hHdd, &pDeferredMsg->u.data, 
                                    pData, size ) ) )
        {
            smsLog(pMac, LOGE, FL("Cannot copy pattern for callback context\n"));
            palFreeMemory( pMac->hHdd, pDeferredMsg );
            return eHAL_STATUS_FAILURE;
        }
    }
    csrLLInsertTail( &pMac->pmc.deferredMsgList, &pDeferredMsg->link, eANI_BOOLEAN_TRUE );
    //No callback is needed. The messages are put into deferred queue and be processed first 
    //when enter full power is complete.
    status = pmcRequestFullPower( pMac, NULL, NULL, eSME_REASON_OTHER );
    if( eHAL_STATUS_PMC_PENDING != status )
    {
        //either fail or already in full power
        if( csrLLRemoveEntry( &pMac->pmc.deferredMsgList, &pDeferredMsg->link, eANI_BOOLEAN_TRUE ) )
        {
            palFreeMemory( pMac->hHdd, pDeferredMsg );
        }
        if( !HAL_STATUS_SUCCESS( status ) )
        {
            smsLog(pMac, LOGE, FL("failed to request full power status = %d\n"), status);
        }
    }

    return (status);
}

void pmcReleaseCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    if(!pCommand->u.pmcCmd.fReleaseWhenDone)
    {
        //This is a normal command, put it back to the free lsit
        pCommand->u.pmcCmd.size = 0;
        smeReleaseCommand( pMac, pCommand );
    }
    else
    {
        //this is a specially allocated comamnd due to out of command buffer. free it.
        palFreeMemory(pMac->hHdd, pCommand);
    }
}


//this function is used to abort a command where the normal processing of the command
//is terminated without going through the normal path. it is here to take care of callbacks for
//the command, if applicable.
void pmcAbortCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand, tANI_BOOLEAN fStopping )
{
    if( eSmePmcCommandMask & pCommand->command )
    {
        if( !fStopping )
        {
            switch( pCommand->command )
            {
            case eSmeCommandEnterImps:
                smsLog(pMac, LOGE, FL("aborting request to enter IMPS\n"));
                pmcEnterFullPowerState(pMac);
                break;

            case eSmeCommandExitImps:
                smsLog(pMac, LOGE, FL("aborting request to exit IMPS \n"));
                pmcEnterFullPowerState(pMac);
                break;

            case eSmeCommandEnterBmps:
                smsLog(pMac, LOGE, FL("aborting request to enter BMPS \n"));
                pMac->pmc.bmpsRequestQueued = eANI_BOOLEAN_FALSE;
                pmcEnterFullPowerState(pMac);
                pmcDoBmpsCallbacks(pMac, eHAL_STATUS_FAILURE);
                break;

            case eSmeCommandExitBmps:
                smsLog(pMac, LOGE, FL("aborting request to exit BMPS \n"));
                pmcEnterFullPowerState(pMac);
                break;

            case eSmeCommandEnterUapsd:
                smsLog(pMac, LOGE, FL("aborting request to enter UAPSD \n"));
                //Since there is no retry for UAPSD, tell the requester here we are done with failure
                pMac->pmc.uapsdSessionRequired = FALSE;
                pmcDoStartUapsdCallbacks(pMac, eHAL_STATUS_FAILURE);
                break;

            case eSmeCommandExitUapsd:
                smsLog(pMac, LOGE, FL("aborting request to exit UAPSD \n"));
                break;

            case eSmeCommandEnterWowl:
                smsLog(pMac, LOGE, FL("aborting request to enter WOWL \n"));
                pmcDoEnterWowlCallbacks(pMac, eHAL_STATUS_FAILURE);
                break;

            case eSmeCommandExitWowl:
                smsLog(pMac, LOGE, FL("aborting request to exit WOWL \n"));
                break;

            case eSmeCommandEnterStandby:
                smsLog(pMac, LOGE, FL("aborting request to enter Standby \n"));
                pmcDoStandbyCallbacks(pMac, eHAL_STATUS_FAILURE);
                break;

            default:
                smsLog(pMac, LOGE, FL("Request for PMC command (%d) is dropped\n"), pCommand->command);
                break;
            }
        }// !stopping
        pmcReleaseCommand( pMac, pCommand );
    }
}



//These commands are not supposed to fail due to out of command buffer,
//otherwise other commands are not executed and no command is released. It will be deadlock. 
#define PMC_IS_COMMAND_CANNOT_FAIL(cmdType)\
    ( (eSmeCommandEnterStandby == (cmdType )) ||\
      (eSmeCommandExitImps == (cmdType )) ||\
      (eSmeCommandExitBmps == (cmdType )) ||\
      (eSmeCommandExitUapsd == (cmdType )) ||\
      (eSmeCommandExitWowl == (cmdType )) )

eHalStatus pmcPrepareCommand( tpAniSirGlobal pMac, eSmeCommandType cmdType, void *pvParam, 
                            tANI_U32 size, tSmeCmd **ppCmd )
{
    eHalStatus status = eHAL_STATUS_RESOURCES;
    tSmeCmd *pCommand = NULL;

    VOS_ASSERT( ppCmd );
    do
    {
        pCommand = smeGetCommandBuffer( pMac );
        if ( pCommand )
        {
            //Make sure it will be put back to the list
            pCommand->u.pmcCmd.fReleaseWhenDone = FALSE;
        }
        else
        {
            smsLog( pMac, LOGE, FL(" fail to get command buffer for command 0x%X curState = %d"), cmdType, pMac->pmc.pmcState );
            //For certain PMC command, we cannot fail
            if( PMC_IS_COMMAND_CANNOT_FAIL(cmdType) )
            {
                smsLog( pMac, LOGE, FL(" command 0x%X  cannot fail try allocating memory for it"), cmdType );
                status = palAllocateMemory(pMac->hHdd, (void **)&pCommand, sizeof(tSmeCmd));
                if(!HAL_STATUS_SUCCESS(status))
                {
                    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL, "%s fail to allocate memory for command (0x%X)", 
                        __FUNCTION__, cmdType);
                    pCommand = NULL;
                    break;
                }
                palZeroMemory(pMac->hHdd, pCommand, sizeof(tSmeCmd));
                //Make sure it will be free when it is done
                pCommand->u.pmcCmd.fReleaseWhenDone = TRUE;
            }
            else
        {
            break;
        }
        }
        pCommand->command = cmdType;
        pCommand->u.pmcCmd.size = size;
        //Initialize the reason code here. It may be overwritten later when
        //a particular reason is needed.
        pCommand->u.pmcCmd.fullPowerReason = eSME_REASON_OTHER;
        switch ( cmdType )
        {
        case eSmeCommandEnterImps:
        case eSmeCommandExitImps:
        case eSmeCommandEnterBmps:
        case eSmeCommandEnterUapsd:
        case eSmeCommandEnterStandby:
            status = eHAL_STATUS_SUCCESS;
            break;

        case eSmeCommandExitUapsd:
        case eSmeCommandExitWowl:
            status = eHAL_STATUS_SUCCESS;
            if( pvParam )
            {
                pCommand->u.pmcCmd.fullPowerReason = *( (tRequestFullPowerReason *)pvParam );
            }
            break;

        case eSmeCommandExitBmps:
            status = eHAL_STATUS_SUCCESS;
            if( pvParam )
            {
                pCommand->u.pmcCmd.u.exitBmpsInfo = *( (tExitBmpsInfo *)pvParam );
                pCommand->u.pmcCmd.fullPowerReason = pCommand->u.pmcCmd.u.exitBmpsInfo.exitBmpsReason;
            }
            else
            {
                smsLog( pMac, LOGE, (" exit BMPS must have a reason code\n") );
            }
            break;

        case eSmeCommandEnterWowl:
            status = eHAL_STATUS_SUCCESS;
            if( pvParam )
            {
                pCommand->u.pmcCmd.u.enterWowlInfo = *( ( tSirSmeWowlEnterParams * )pvParam );
            }
            break;

        default:
            smsLog( pMac, LOGE, FL("  invalid command type %d\n"), cmdType );
            status = eHAL_STATUS_INVALID_PARAMETER;
            break;
        }

    } while( 0 );

    if( HAL_STATUS_SUCCESS( status ) && pCommand )
    {
        *ppCmd = pCommand;
    }
    else if( pCommand )
    {
        pmcReleaseCommand( pMac, pCommand );
    }

    return (status);
}


eHalStatus pmcIssueCommand( tpAniSirGlobal pMac, eSmeCommandType cmdType, void *pvParam, 
                            tANI_U32 size, tANI_BOOLEAN fPutToListHead )
{
    eHalStatus status = eHAL_STATUS_RESOURCES;
    tSmeCmd *pCommand = NULL;

    status = pmcPrepareCommand( pMac, cmdType, pvParam, size, &pCommand );
    if( HAL_STATUS_SUCCESS( status ) && pCommand )
    {
        smePushCommand( pMac, pCommand, fPutToListHead );
    }

    return( status );
}



tANI_BOOLEAN pmcProcessCommand( tpAniSirGlobal pMac, tSmeCmd *pCommand )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    VOS_STATUS vstatus;
    tANI_BOOLEAN fRemoveCmd = eANI_BOOLEAN_TRUE;

    do
    {
        switch ( pCommand->command )
        {
        case eSmeCommandEnterImps:
            if( FULL_POWER == pMac->pmc.pmcState )
            {
                status = pmcEnterImpsCheck( pMac );
                if( HAL_STATUS_SUCCESS( status ) )
                {
                    /* Change state. */
                    pMac->pmc.pmcState = REQUEST_IMPS;
                    status = pmcSendMessage(pMac, eWNI_PMC_ENTER_IMPS_REQ, NULL, 0);
                    if( HAL_STATUS_SUCCESS( status ) )
                    {
                        /* If we already went back Full Power State (meaning that request did not
                           get as far as the device) then we are not successfull. */
                        if ( FULL_POWER != pMac->pmc.pmcState )
                        {
                            fRemoveCmd = eANI_BOOLEAN_FALSE;
                        }
                    }
                }
                if( !HAL_STATUS_SUCCESS( status ) )
                {
                    smsLog(pMac, LOGE, "PMC: failure to send message eWNI_PMC_ENTER_IMPS_REQ or pmcEnterImpsCheck failed\n");
                    pmcEnterFullPowerState( pMac );
                    if(pmcShouldBmpsTimerRun(pMac))
                        (void)pmcStartTrafficTimer(pMac, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
                }
            }//full_power
            break;

        case eSmeCommandExitImps:
            pMac->pmc.requestFullPowerPending = FALSE;
            if( ( IMPS == pMac->pmc.pmcState ) || ( STANDBY == pMac->pmc.pmcState ) )
            {
                //Check state before sending message. The state may change after that
                if( STANDBY == pMac->pmc.pmcState )
                {
                    //Enable Idle scan in CSR
                    csrScanResumeIMPS(pMac);
                }

                status = pmcSendMessage(pMac, eWNI_PMC_EXIT_IMPS_REQ, NULL, 0);
                if ( HAL_STATUS_SUCCESS( status ) )
                {
                    pMac->pmc.pmcState = REQUEST_FULL_POWER;
                    smsLog(pMac, LOGW, FL("eWNI_PMC_EXIT_IMPS_REQ sent to PE\n")); 
                    fRemoveCmd = eANI_BOOLEAN_FALSE;
                }
                else
                {
                    smsLog(pMac, LOGE, FL("eWNI_PMC_EXIT_IMPS_REQ fail to be sent to PE status %d\n"), status); 
                    //Callbacks are called with success srarus, do we need to pass in real status??
                    pmcEnterFullPowerState(pMac);
                }
            }
            break;

        case eSmeCommandEnterBmps:
            if( FULL_POWER == pMac->pmc.pmcState )
            {
                //This function will not return success because the pmc state is not BMPS
                status = pmcEnterBmpsCheck( pMac );
                if( HAL_STATUS_SUCCESS( status ) )
                {
                    /* Change PMC state */
                    pMac->pmc.pmcState = REQUEST_BMPS;
                    smsLog(pMac, LOGW, "PMC: Enter BMPS req done: Force XO Core ON\n");
                    vstatus = vos_chipVoteXOCore(NULL, NULL, NULL, VOS_TRUE); 
                    if ( !VOS_IS_STATUS_SUCCESS(vstatus) )
                    {
                        smsLog(pMac, LOGE, "Could not turn XO Core ON. Can't go to BMPS\n");
                    }
                    else /* XO Core turn ON was successful */
                    {
                    /* Tell MAC to have device enter BMPS mode. */
                    status = pmcSendMessage(pMac, eWNI_PMC_ENTER_BMPS_REQ, NULL, 0);
                    if ( HAL_STATUS_SUCCESS( status ) )
                    {
                        fRemoveCmd = eANI_BOOLEAN_FALSE;
                    }
                    else
                    {
                        smsLog(pMac, LOGE, "Fail to send enter BMPS msg to PE\n");
                            /* Cancel the vote for XO Core */
                            smsLog(pMac, LOGW, "In module init: Cancel the vote for XO CORE ON "
                                                             "since send enter bmps failed\n");
                            if (vos_chipVoteXOCore(NULL, NULL, NULL, VOS_FALSE) != VOS_STATUS_SUCCESS)
                            {
                                smsLog(pMac, LOGE, "Could not cancel XO Core ON vote."
                                                   "Not returning failure."
                                                   "Power consumed will be high\n");
                            }  
                            
                        }
                    }
                }
                if( !HAL_STATUS_SUCCESS( status ) )
                {
                    smsLog(pMac, LOGE, "PMC: failure to send message eWNI_PMC_ENTER_BMPS_REQ status %d\n", status);
                    pMac->pmc.bmpsRequestQueued = eANI_BOOLEAN_FALSE;
                    pmcEnterFullPowerState(pMac);
                    //Do not call UAPSD callback here since it may be retried
                    pmcDoBmpsCallbacks(pMac, eHAL_STATUS_FAILURE);
                    if(pmcShouldBmpsTimerRun(pMac))
                        (void)pmcStartTrafficTimer(pMac, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
                }
            }
            break;

        case eSmeCommandExitBmps:
            if( BMPS == pMac->pmc.pmcState )
            {
                pMac->pmc.requestFullPowerPending = FALSE;

                status = pmcSendMessage( pMac, eWNI_PMC_EXIT_BMPS_REQ, 
                            &pCommand->u.pmcCmd.u.exitBmpsInfo, sizeof(tExitBmpsInfo) );
                if ( HAL_STATUS_SUCCESS( status ) )
                {
                    pMac->pmc.pmcState = REQUEST_FULL_POWER;
                    fRemoveCmd = eANI_BOOLEAN_FALSE;
                    smsLog(pMac, LOGW, FL("eWNI_PMC_EXIT_BMPS_REQ sent to PE\n"));

                }
                else
                {
                    smsLog(pMac, LOGE, FL("eWNI_PMC_EXIT_BMPS_REQ fail to be sent to PE status %d\n"), status);
                    pmcEnterFullPowerState(pMac);
                }
            }
            break;

        case eSmeCommandEnterUapsd:
            if( BMPS == pMac->pmc.pmcState )
            {
                pMac->pmc.uapsdSessionRequired = TRUE;
                status = pmcSendMessage(pMac, eWNI_PMC_ENTER_UAPSD_REQ, NULL, 0);
                if ( HAL_STATUS_SUCCESS( status ) )
                {
                    pMac->pmc.pmcState = REQUEST_START_UAPSD;
                    fRemoveCmd = eANI_BOOLEAN_FALSE;
                }
                else
                {
                    smsLog(pMac, LOGE, "PMC: failure to send message "
                       "eWNI_PMC_ENTER_BMPS_REQ\n");
                    //there is no retry for re-entering UAPSD so tell the requester we are done witgh failure.
                    pMac->pmc.uapsdSessionRequired = FALSE;
                    pmcDoStartUapsdCallbacks(pMac, eHAL_STATUS_FAILURE);
                }
            }
            break;

        case eSmeCommandExitUapsd:
           if( UAPSD == pMac->pmc.pmcState )
           {
               pMac->pmc.requestFullPowerPending = FALSE;
                /* If already in REQUEST_STOP_UAPSD, simply return */
               if (pMac->pmc.pmcState == REQUEST_STOP_UAPSD)
               {
                   break;
               }

               /* Tell MAC to have device exit UAPSD mode. */
               status = pmcSendMessage(pMac, eWNI_PMC_EXIT_UAPSD_REQ, NULL, 0);
               if ( HAL_STATUS_SUCCESS( status ) )
               {
                   /* Change state. Note that device will be put in BMPS state at the
                      end of REQUEST_STOP_UAPSD state even if response is a failure*/
                   pMac->pmc.pmcState = REQUEST_STOP_UAPSD;
                   pMac->pmc.requestFullPowerPending = TRUE;
                   pMac->pmc.requestFullPowerReason = pCommand->u.pmcCmd.fullPowerReason;
                   fRemoveCmd = eANI_BOOLEAN_FALSE;
               }
               else
               {
                   smsLog(pMac, LOGE, "PMC: failure to send message "
                      "eWNI_PMC_EXIT_UAPSD_REQ\n");
                   pmcEnterBmpsState(pMac);
               }
           }

           break;

        case eSmeCommandEnterWowl:
            if( ( BMPS == pMac->pmc.pmcState ) || ( WOWL == pMac->pmc.pmcState ) )
            {
                status = pmcSendMessage(pMac, eWNI_PMC_ENTER_WOWL_REQ, 
                        &pCommand->u.pmcCmd.u.enterWowlInfo, sizeof(tSirSmeWowlEnterParams));
                if ( HAL_STATUS_SUCCESS( status ) )
                {
                    pMac->pmc.pmcState = REQUEST_ENTER_WOWL;
                    fRemoveCmd = eANI_BOOLEAN_FALSE;
                }
                else
                {
                    smsLog(pMac, LOGE, "PMC: failure to send message eWNI_PMC_ENTER_WOWL_REQ\n");
                    pmcDoEnterWowlCallbacks(pMac, eHAL_STATUS_FAILURE);
                }
            }
            else
            {
                fRemoveCmd = eANI_BOOLEAN_TRUE;
            }
            break;

        case eSmeCommandExitWowl:
            if( WOWL == pMac->pmc.pmcState )
            {
                pMac->pmc.requestFullPowerPending = FALSE;
                pMac->pmc.pmcState = REQUEST_EXIT_WOWL;
                status = pmcSendMessage(pMac, eWNI_PMC_EXIT_WOWL_REQ, NULL, 0);
                if ( HAL_STATUS_SUCCESS( status ) )
                {
                    fRemoveCmd = eANI_BOOLEAN_FALSE;
                    pMac->pmc.requestFullPowerPending = TRUE;
                    pMac->pmc.requestFullPowerReason = pCommand->u.pmcCmd.fullPowerReason;
                }
                else
                {
                    smsLog(pMac, LOGP, "PMC: failure to send message eWNI_PMC_EXIT_WOWL_REQ\n");
                    pmcEnterBmpsState(pMac);
                }
            }
            break;

        case eSmeCommandEnterStandby:
            if( FULL_POWER == pMac->pmc.pmcState )
            {
               //Disallow standby if concurrent sessions are present. Note that CSR would have
               //caused the STA to disconnect the Infra session (if not already disconnected) because of 
               //standby request. But we are now failing the standby request because of concurrent session.
               //So was the tearing of infra session wasteful if we were going to fail the standby request ? 
               //Not really. This is beacuse if and when BT-AMP etc sessions are torn down we will transition
               //to IMPS/standby and still save power.
               if (csrIsIBSSStarted(pMac) || csrIsBTAMPStarted(pMac))
               {
                  VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_FATAL, 
                      "WLAN: IBSS or BT-AMP session present. Cannot honor standby request");

                  pmcDoStandbyCallbacks(pMac, eHAL_STATUS_PMC_NOT_NOW);
                  if(pmcShouldBmpsTimerRun(pMac))
                      (void)pmcStartTrafficTimer(pMac, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
                  break;
               }
 
                // Stop traffic timer. Just making sure timer is not running
                pmcStopTrafficTimer(pMac);

                /* Change state. */
                pMac->pmc.pmcState = REQUEST_STANDBY;

                /* Tell MAC to have device enter STANDBY mode. We are using the same message
                  as IMPS mode to avoid code changes in layer below (PE/HAL)*/
                status = pmcSendMessage(pMac, eWNI_PMC_ENTER_IMPS_REQ, NULL, 0);
                if ( HAL_STATUS_SUCCESS( status ) )
                {
                    //Disable Idle scan in CSR
                    csrScanSuspendIMPS(pMac);
                    fRemoveCmd = eANI_BOOLEAN_FALSE;
                }
                else
                {
                    smsLog(pMac, LOGE, "PMC: failure to send message "
                        "eWNI_PMC_ENTER_IMPS_REQ\n");
                    pmcEnterFullPowerState(pMac);
                    pmcDoStandbyCallbacks(pMac, eHAL_STATUS_FAILURE);
                    /* Start the timer only if Auto BMPS feature is enabled or an UAPSD session is
                     required */
                    if(pmcShouldBmpsTimerRun(pMac))
                        (void)pmcStartTrafficTimer(pMac, pMac->pmc.bmpsConfig.trafficMeasurePeriod);
                }
            }
            break;

        default:
            smsLog( pMac, LOGE, FL("  invalid command type %d\n"), pCommand->command );
            break;
        }

    } while( 0 );

    return( fRemoveCmd );
}

eHalStatus pmcEnterImpsCheck( tpAniSirGlobal pMac )
{

    if( !PMC_IS_READY(pMac) )
    {
        smsLog(pMac, LOGE, FL("Requesting IMPS when PMC not ready\n"));
        smsLog(pMac, LOGE, FL("pmcReady = %d pmcState = %s\n"), 
            pMac->pmc.pmcReady, pmcGetPmcStateStr(pMac->pmc.pmcState));
        return eHAL_STATUS_FAILURE;
    }

    /* Check if IMPS is enabled. */
    if (!pMac->pmc.impsEnabled)
    {
        smsLog(pMac, LOG2, FL("IMPS is disabled\n"));
        return eHAL_STATUS_PMC_DISABLED;
    }

    /* Check if IMPS enabled for current power source. */
    if ((pMac->pmc.powerSource == AC_POWER) && !pMac->pmc.impsConfig.enterOnAc)
    {
        smsLog(pMac, LOG2, FL("IMPS is disabled when operating on AC power\n"));
        return eHAL_STATUS_PMC_AC_POWER;
    }

    /* Check that entry into a power save mode is allowed at this time. */
    if (!pmcPowerSaveCheck(pMac))
    {
        smsLog(pMac, LOG2, FL("IMPS cannot be entered now\n"));
        return eHAL_STATUS_PMC_NOT_NOW;
    }

    /* Check that entry into a power save mode is allowed at this time if all
       running sessions agree. */
    if (!pmcAllowImps(pMac))
    {
        smsLog(pMac, LOG2, FL("IMPS cannot be entered now\n"));
        return eHAL_STATUS_PMC_NOT_NOW;
    }
    
    /* Check if already in IMPS. */
    if ((pMac->pmc.pmcState == REQUEST_IMPS) || (pMac->pmc.pmcState == IMPS) ||
        (pMac->pmc.pmcState == REQUEST_FULL_POWER))
    {
        smsLog(pMac, LOG2, FL("Already in IMPS\n"));
        return eHAL_STATUS_PMC_ALREADY_IN_IMPS;
    }

    return ( eHAL_STATUS_SUCCESS );
}

/* This API detrmines if it is ok to proceed with a Enter BMPS Request or not . Note when
   device is in BMPS/UAPSD states, this API returns failure because it is not ok to issue
   a BMPS request */
eHalStatus pmcEnterBmpsCheck( tpAniSirGlobal pMac )
{

   /* Check if BMPS is enabled. */
   if (!pMac->pmc.bmpsEnabled)
   {
      smsLog(pMac, LOGE, "PMC: Cannot initiate BMPS. BMPS is disabled\n");
      return eHAL_STATUS_PMC_DISABLED;
   }

   if( !PMC_IS_READY(pMac) )
   {
       smsLog(pMac, LOGE, FL("Requesting BMPS when PMC not ready\n"));
       smsLog(pMac, LOGE, FL("pmcReady = %d pmcState = %s\n"), 
           pMac->pmc.pmcReady, pmcGetPmcStateStr(pMac->pmc.pmcState));
       return eHAL_STATUS_FAILURE;
   }

   /* Check that we are associated with a single active session. */
   if (!pmcValidateConnectState( pMac ))
   {
      smsLog(pMac, LOGE, "PMC: STA not associated with an AP with single active session. BMPS cannot be entered\n");
      return eHAL_STATUS_FAILURE;
   }

   /* BMPS can only be requested when device is in Full Power */
   if (pMac->pmc.pmcState != FULL_POWER)
   {
      smsLog(pMac, LOGE, "PMC: Device not in full power. Cannot request BMPS. pmcState %d\n", pMac->pmc.pmcState);
      return eHAL_STATUS_FAILURE;
   }
   /* Check that entry into a power save mode is allowed at this time. */
   if (!pmcPowerSaveCheck(pMac))
   {
      smsLog(pMac, LOGE, "PMC: Power save check failed. BMPS cannot be entered now\n");
      return eHAL_STATUS_PMC_NOT_NOW;
   }

   smsLog(pMac, LOG1, FL("concurrency enabled %u\n"), pMac->roam.configParam.concurrencyEnabled);
   if (pMac->roam.configParam.concurrencyEnabled)
   {
      pMac->roam.configParam.concurrencyEnabled = 0;
      smsLog(pMac, LOG1, FL("reset concurrency to disabled %u\n"), pMac->roam.configParam.concurrencyEnabled);
      csrDisconnectAllActiveSessions(pMac);
   }
   return ( eHAL_STATUS_SUCCESS );
}

tANI_BOOLEAN pmcShouldBmpsTimerRun( tpAniSirGlobal pMac )
{
    /* Check if BMPS is enabled and if Auto BMPS Feature is still enabled 
     * or there is a pending Uapsd request or HDD requested BMPS or there
     * is a pending request for WoWL. In all these cases BMPS is required. 
     * Otherwise just stop the timer and return.
     */
    if (!(pMac->pmc.bmpsEnabled && (pMac->pmc.autoBmpsEntryEnabled || 
          pMac->pmc.uapsdSessionRequired || pMac->pmc.bmpsRequestedByHdd ||
          pMac->pmc.wowlModeRequired )))
    {
        smsLog(pMac, LOG1, FL("BMPS is not enabled or not required"));
        return eANI_BOOLEAN_FALSE;
    }

    /* Check if there is an Infra session. BMPS is possible only if there is 
     * an Infra session */
    if (!csrIsInfraConnected(pMac))
    {
        smsLog(pMac, LOG1, FL("No Infra Session or multiple sessions. BMPS should not be started"));
        return eANI_BOOLEAN_FALSE;
    }
    return eANI_BOOLEAN_TRUE;
}


#ifdef FEATURE_WLAN_DIAG_SUPPORT 

#define PMC_DIAG_EVT_TIMER_INTERVAL ( 5000 )

void pmcDiagEvtTimerExpired (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    WLAN_VOS_DIAG_EVENT_DEF(psRequest, vos_event_wlan_powersave_payload_type);

    vos_mem_zero(&psRequest, sizeof(vos_event_wlan_powersave_payload_type));
    psRequest.event_subtype = WLAN_PMC_CURRENT_STATE;
    psRequest.pmc_current_state = pMac->pmc.pmcState;

    WLAN_VOS_DIAG_EVENT_REPORT(&psRequest, EVENT_WLAN_POWERSAVE_GENERIC);

    smsLog(pMac, LOGW, FL("DIAG event timer expired\n"));

    /* re-arm timer */
    if (pmcStartDiagEvtTimer(hHal) != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGP, FL("Cannot re-arm DIAG evt timer\n"));
    }
}

eHalStatus pmcStartDiagEvtTimer (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    smsLog(pMac, LOG2, FL("Entering pmcStartDiagEvtTimer\n"));

    if (palTimerStart(pMac->hHdd, pMac->pmc.hDiagEvtTimer, PMC_DIAG_EVT_TIMER_INTERVAL *
                          1000, TRUE) != eHAL_STATUS_SUCCESS)
    {
       smsLog(pMac, LOGP, FL("Cannot start DIAG evt timer\n"));
       return eHAL_STATUS_FAILURE;
    }

    return eHAL_STATUS_SUCCESS;
}

void pmcStopDiagEvtTimer (tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    smsLog(pMac, LOG2, FL("Entering pmcStopDiagEvtTimer\n"));
    (void)palTimerStop(pMac->hHdd, pMac->pmc.hDiagEvtTimer);
}
#endif
