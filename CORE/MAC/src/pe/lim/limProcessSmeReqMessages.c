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
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limProcessSmeReqMessages.cc contains the code
 * for processing SME request messages.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#include "palTypes.h"
#include "wniApi.h"
#ifdef ANI_PRODUCT_TYPE_AP
#include "wniCfgAp.h"
#else
#include "wniCfgSta.h"
#endif
#include "cfgApi.h"
#include "sirApi.h"
#include "schApi.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limAssocUtils.h"
#include "limSecurityUtils.h"
#include "limSerDesUtils.h"
#include "limSmeReqUtils.h"
#include "limIbssPeerMgmt.h"
#include "limAdmitControl.h"
#include "dphHashTable.h"
#include "limSendMessages.h"
#include "limApi.h"
#include "wmmApsd.h"

#ifdef WLAN_SOFTAP_FEATURE
#include "sapApi.h"
#endif

#if defined WLAN_FEATURE_VOWIFI
#include "rrmApi.h"
#endif
#if defined FEATURE_WLAN_CCX
#include "ccxApi.h"
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
#include <limFT.h>
#endif

#ifdef FEATURE_WLAN_CCX
/* These are the min/max tx power (non virtual rates) range
   supported by prima hardware */
#define MIN_TX_PWR_CAP    12
#define MAX_TX_PWR_CAP    19

#endif


// SME REQ processing function templates
static void __limProcessSmeStartReq(tpAniSirGlobal, tANI_U32 *);
static tANI_BOOLEAN __limProcessSmeSysReadyInd(tpAniSirGlobal, tANI_U32 *);
static tANI_BOOLEAN __limProcessSmeStartBssReq(tpAniSirGlobal, tpSirMsgQ pMsg);
static void __limProcessSmeScanReq(tpAniSirGlobal, tANI_U32 *);
static void __limProcessSmeJoinReq(tpAniSirGlobal, tANI_U32 *);
static void __limProcessSmeReassocReq(tpAniSirGlobal, tANI_U32 *);
static void __limProcessSmeDisassocReq(tpAniSirGlobal, tANI_U32 *);
static void __limProcessSmeDisassocCnf(tpAniSirGlobal, tANI_U32 *);
static void __limProcessSmeDeauthReq(tpAniSirGlobal, tANI_U32 *);
static void __limProcessSmeSetContextReq(tpAniSirGlobal, tANI_U32 *);
static tANI_BOOLEAN __limProcessSmeStopBssReq(tpAniSirGlobal, tpSirMsgQ pMsg);

#if 0
  static void __limProcessSmeAuthReq(tpAniSirGlobal, tANI_U32 *);
  static void __limProcessSmePromiscuousReq(tpAniSirGlobal, tANI_U32 *);
#endif

#ifdef ANI_PRODUCT_TYPE_AP
static void __limProcessSmeAssocCnf(tpAniSirGlobal, tANI_U32, tANI_U32 *);
#endif
void __limProcessSmeAssocCnfNew(tpAniSirGlobal, tANI_U32, tANI_U32 *);

#ifdef VOSS_ENABLED
extern void peRegisterTLHandle(tpAniSirGlobal pMac);
#endif

#ifdef WLAN_FEATURE_P2P
extern int limProcessRemainOnChnlReq(tpAniSirGlobal pMac, tANI_U32 *pMsg);
#endif

#ifdef ANI_PRODUCT_TYPE_CLIENT
#ifdef BACKGROUND_SCAN_ENABLED

// start the background scan timers if it hasn't already started
static void
__limBackgroundScanInitiate(tpAniSirGlobal pMac)
{
    if (pMac->lim.gLimBackgroundScanStarted)
        return;

    //make sure timer is created first
    if (TX_TIMER_VALID(pMac->lim.limTimers.gLimBackgroundScanTimer))
    {
        limDeactivateAndChangeTimer(pMac, eLIM_BACKGROUND_SCAN_TIMER);
     MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, 0, eLIM_BACKGROUND_SCAN_TIMER));
        if (tx_timer_activate(&pMac->lim.limTimers.gLimBackgroundScanTimer) != TX_SUCCESS)
            limLog(pMac, LOGP, FL("could not activate background scan timer\n"));
        pMac->lim.gLimBackgroundScanStarted   = true;
        pMac->lim.gLimBackgroundScanChannelId = 0;
    }
}

#endif // BACKGROUND_SCAN_ENABLED
#endif

// determine if a fresh scan request must be issued or not
/*
* PE will do fresh scan, if all of the active sessions are in good state (Link Est or BSS Started)
* If one of the sessions is not in one of the above states, then PE does not do fresh scan
* If no session exists (scanning very first time), then PE will always do fresh scan if SME
* asks it to do that.
*/
static tANI_U8
__limFreshScanReqd(tpAniSirGlobal pMac, tANI_U8 returnFreshResults)
{

    tANI_U8 validState = TRUE;
    int i;

    if(pMac->lim.gLimSmeState != eLIM_SME_IDLE_STATE)
    {
        return FALSE;
    }
    for(i =0; i < pMac->lim.maxBssId; i++)
    {

        if(pMac->lim.gpSession[i].valid == TRUE)
        {
            if(!( ( (  (pMac->lim.gpSession[i].bssType == eSIR_INFRASTRUCTURE_MODE) ||
                        (pMac->lim.gpSession[i].limSystemRole == eLIM_BT_AMP_STA_ROLE))&&
                       (pMac->lim.gpSession[i].limSmeState == eLIM_SME_LINK_EST_STATE) )||
                  
                  (    ( (pMac->lim.gpSession[i].bssType == eSIR_IBSS_MODE)||
                           (pMac->lim.gpSession[i].limSystemRole == eLIM_BT_AMP_AP_ROLE)||
                           (pMac->lim.gpSession[i].limSystemRole == eLIM_BT_AMP_STA_ROLE) )&&
                       (pMac->lim.gpSession[i].limSmeState == eLIM_SME_NORMAL_STATE) )
#ifdef WLAN_FEATURE_P2P
               ||  ( ( ( (pMac->lim.gpSession[i].bssType == eSIR_INFRA_AP_MODE) 
                      && ( pMac->lim.gpSession[i].pePersona == VOS_P2P_GO_MODE) )
                    || (pMac->lim.gpSession[i].limSystemRole == eLIM_AP_ROLE) )
                  && (pMac->lim.gpSession[i].limSmeState == eLIM_SME_NORMAL_STATE) )
#endif
             ))
                {
                validState = FALSE;
                break;
              }
            
        }
    }
    PELOG1(limLog(pMac, LOG1, FL("FreshScanReqd: %d \n"), validState);)

   if( (validState) && (returnFreshResults & SIR_BG_SCAN_RETURN_FRESH_RESULTS))
    return TRUE;

    return FALSE;
}

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
static tANI_BOOLEAN __limProcessSmeSwitchChlReq(tpAniSirGlobal, tpSirMsgQ pMsg);
#endif


/**
 * __limIsSmeAssocCnfValid()
 *
 *FUNCTION:
 * This function is called by limProcessLmmMessages() upon
 * receiving SME_ASSOC_CNF.
 *
 *LOGIC:
 * Message validity checks are performed in this function
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMeasReq  Pointer to Received ASSOC_CNF message
 * @return true      When received SME_ASSOC_CNF is formatted
 *                   correctly
 *         false     otherwise
 */

inline static tANI_U8
__limIsSmeAssocCnfValid(tpSirSmeAssocCnf pAssocCnf)
{
    if (limIsGroupAddr(pAssocCnf->peerMacAddr))
        return false;
    else
        return true;
} /*** end __limIsSmeAssocCnfValid() ***/


/**
 * __limGetSmeJoinReqSizeForAlloc()
 *
 *FUNCTION:
 * This function is called in various places to get IE length
 * from tSirBssDescription structure
 * number being scanned.
 *
 *PARAMS:
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 * NA
 *
 *NOTE:
 * NA
 *
 * @param     pBssDescr
 * @return    Total IE length
 */

static tANI_U16
__limGetSmeJoinReqSizeForAlloc(tANI_U8 *pBuf)
{
    tANI_U16 len = 0;

    if (!pBuf)
        return len;

    pBuf += sizeof(tANI_U16);
    len = limGetU16( pBuf );
    return (len + sizeof( tANI_U16 ));
} /*** end __limGetSmeJoinReqSizeForAlloc() ***/


/**----------------------------------------------------------------
\fn     __limIsDeferedMsgForLearn

\brief  Has role only if 11h is enabled. Not used on STA side.
        Defers the message if SME is in learn state and brings
        the LIM back to normal mode.

\param  pMac
\param  pMsg - Pointer to message posted from SME to LIM.
\return TRUE - If defered
        FALSE - Otherwise
------------------------------------------------------------------*/
static tANI_BOOLEAN
__limIsDeferedMsgForLearn(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    if (limIsSystemInScanState(pMac))
    {
        if (limDeferMsg(pMac, pMsg) != TX_SUCCESS)
        {
            PELOGE(limLog(pMac, LOGE, FL("Could not defer Msg = %d\n"), pMsg->type);)
            return eANI_BOOLEAN_FALSE;
        }
        PELOG1(limLog(pMac, LOG1, FL("Defer the message, in learn mode type = %d\n"),
                                                                 pMsg->type);)

        /** Send finish scan req to HAL only if LIM is not waiting for any response
         * from HAL like init scan rsp, start scan rsp etc.
         */        
        if (GET_LIM_PROCESS_DEFD_MESGS(pMac))
        {
            //Set the resume channel to Any valid channel (invalid). 
            //This will instruct HAL to set it to any previous valid channel.
            peSetResumeChannel(pMac, 0, 0);
            limSendHalFinishScanReq(pMac, eLIM_HAL_FINISH_LEARN_WAIT_STATE);
        }

        return eANI_BOOLEAN_TRUE;
    }
    return eANI_BOOLEAN_FALSE;
}

/**----------------------------------------------------------------
\fn     __limIsDeferedMsgForRadar

\brief  Has role only if 11h is enabled. Not used on STA side.
        Defers the message if radar is detected.

\param  pMac
\param  pMsg - Pointer to message posted from SME to LIM.
\return TRUE - If defered
        FALSE - Otherwise
------------------------------------------------------------------*/
static tANI_BOOLEAN
__limIsDeferedMsgForRadar(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    /** fRadarDetCurOperChan will be set only if we detect radar in current
     * operating channel and System Role == AP ROLE */
    if (LIM_IS_RADAR_DETECTED(pMac))
    {
        if (limDeferMsg(pMac, pMsg) != TX_SUCCESS)
        {
            PELOGE(limLog(pMac, LOGE, FL("Could not defer Msg = %d\n"), pMsg->type);)
            return eANI_BOOLEAN_FALSE;
        }
        PELOG1(limLog(pMac, LOG1, FL("Defer the message, in learn mode type = %d\n"),
                                                                 pMsg->type);)
        return eANI_BOOLEAN_TRUE;
    }
    return eANI_BOOLEAN_FALSE;
}


/**
 * __limProcessSmeStartReq()
 *
 *FUNCTION:
 * This function is called to process SME_START_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeStartReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tSirResultCodes  retCode = eSIR_SME_SUCCESS;
    tANI_U8          smesessionId;
    tANI_U16         smetransactionId;
    

   PELOG1(limLog(pMac, LOG1, FL("Received START_REQ\n"));)

    limGetSessionInfo(pMac,(tANI_U8 *)pMsgBuf,&smesessionId,&smetransactionId);
    
    if (pMac->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE)
    {
        pMac->lim.gLimSmeState = eLIM_SME_IDLE_STATE;
        
        MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));
        
        /// By default do not return after first scan match
        pMac->lim.gLimReturnAfterFirstMatch = 0;

        /// Initialize MLM state machine
        limInitMlm(pMac);

        /// By default return unique scan results
        pMac->lim.gLimReturnUniqueResults = true;
        pMac->lim.gLimSmeScanResultLength = 0;

#if defined(ANI_PRODUCT_TYPE_CLIENT) || defined(ANI_AP_CLIENT_SDK)
        if (((tSirSmeStartReq *) pMsgBuf)->sendNewBssInd)
        {
            /*
                     * Need to indicate new BSSs found during background scanning to
                     * host. Update this parameter at CFG
                     */
            if (cfgSetInt(pMac, WNI_CFG_NEW_BSS_FOUND_IND, ((tSirSmeStartReq *) pMsgBuf)->sendNewBssInd)
                != eSIR_SUCCESS)
            {
                limLog(pMac, LOGP, FL("could not set NEIGHBOR_BSS_IND at CFG\n"));
                retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
            }
        }
#endif
    }
    else
    {
        /**
         * Should not have received eWNI_SME_START_REQ in states
         * other than OFFLINE. Return response to host and
         * log error
         */
        limLog(pMac, LOGE, FL("Invalid SME_START_REQ received in SME state %X\n"),pMac->lim.gLimSmeState );
        limPrintSmeState(pMac, LOGE, pMac->lim.gLimSmeState);
        retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
    }
    limSendSmeRsp(pMac, eWNI_SME_START_RSP, retCode,smesessionId,smetransactionId);
} /*** end __limProcessSmeStartReq() ***/


/** -------------------------------------------------------------
\fn __limProcessSmeSysReadyInd
\brief handles the notification from HDD. PE just forwards this message to HAL.
\param   tpAniSirGlobal pMac
\param   tANI_U32* pMsgBuf
\return  TRUE-Posting to HAL failed, so PE will consume the buffer. 
\        FALSE-Posting to HAL successful, so HAL will consume the buffer.
  -------------------------------------------------------------*/
static tANI_BOOLEAN
__limProcessSmeSysReadyInd(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tSirMsgQ msg;
    
    msg.type = WDA_SYS_READY_IND;
    msg.reserved = 0;
    msg.bodyptr =  pMsgBuf;
    msg.bodyval = 0;

#ifdef VOSS_ENABLED
    if(pMac->gDriverType != eDRIVER_TYPE_MFG)
    {
    peRegisterTLHandle(pMac);
    }
#endif
    PELOGW(limLog(pMac, LOGW, FL("sending WDA_SYS_READY_IND msg to HAL\n"));)
    MTRACE(macTraceMsgTx(pMac, 0, msg.type));

    if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
    {
        limLog(pMac, LOGP, FL("wdaPostCtrlMsg failed\n"));
        return eANI_BOOLEAN_TRUE;
    }
    return eANI_BOOLEAN_FALSE;
}


/**
 * __limHandleSmeStartBssRequest()
 *
 *FUNCTION:
 * This function is called to process SME_START_BSS_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limHandleSmeStartBssRequest(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U16                size;
    tANI_U32                val = 0;
    tSirRetStatus           retStatus;
    tSirMacChanNum          channelNumber;
    tLimMlmStartReq         *pMlmStartReq;
    tpSirSmeStartBssReq     pSmeStartBssReq;                //Local variable for Start BSS Req.. Added For BT-AMP Support 
    tSirResultCodes         retCode = eSIR_SME_SUCCESS;
    tANI_U32                autoGenBssId = FALSE;           //Flag Used in case of IBSS to Auto generate BSSID.
    tSirMacHTChannelWidth   txWidthSet;
    tANI_U8                 sessionId;
    tpPESession             psessionEntry = NULL;
    tANI_U8                 smesessionId;
    tANI_U16                smetransactionId;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    //Since the session is not created yet, sending NULL. The response should have the correct state.
    limDiagEventReport(pMac, WLAN_PE_DIAG_START_BSS_REQ_EVENT, NULL, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    
    PELOG1(limLog(pMac, LOG1, FL("Received START_BSS_REQ\n"));)

    /* Global Sme state and mlm states are not defined yet , for BT-AMP Suppoprt . TO BE DONE */
    if ( (pMac->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE) ||
         (pMac->lim.gLimSmeState == eLIM_SME_IDLE_STATE))
    {
        size = sizeof(tSirSmeStartBssReq) + SIR_MAC_MAX_IE_LENGTH;

#ifdef ANI_PRODUCT_TYPE_AP
        size + = ANI_WDS_INFO_MAX_LENGTH;
#endif

        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pSmeStartBssReq, size))
        {
            PELOGE(limLog(pMac, LOGE, FL("palAllocateMemory failed for pMac->lim.gpLimStartBssReq\n"));)
            /// Send failure response to host
            retCode = eSIR_SME_RESOURCES_UNAVAILABLE;
            goto end;
        }
        
        (void) palZeroMemory(pMac->hHdd, (void *)pSmeStartBssReq, size);
        
        if ((limStartBssReqSerDes(pMac, pSmeStartBssReq, (tANI_U8 *) pMsgBuf) == eSIR_FAILURE) ||
                (!limIsSmeStartBssReqValid(pMac, pSmeStartBssReq)))
        {
            PELOGW(limLog(pMac, LOGW, FL("Received invalid eWNI_SME_START_BSS_REQ\n"));)
            retCode = eSIR_SME_INVALID_PARAMETERS;
            goto free;
        }
#if 0   
       PELOG3(limLog(pMac, LOG3,
           FL("Parsed START_BSS_REQ fields are bssType=%d, channelId=%d\n"),
           pMac->lim.gpLimStartBssReq->bssType, pMac->lim.gpLimStartBssReq->channelId);)
#endif 

        /* This is the place where PE is going to create a session.
         * If session is not existed , then create a new session */
        if((psessionEntry = peFindSessionByBssid(pMac,pSmeStartBssReq->bssId,&sessionId)) != NULL)
        {
            limLog(pMac, LOGW, FL("Session Already exists for given BSSID\n"));
            retCode = eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED;
            psessionEntry = NULL;
            goto free;
        }
        else
        {
            if((psessionEntry = peCreateSession(pMac,pSmeStartBssReq->bssId,&sessionId, pMac->lim.maxStation)) == NULL)
            {
                limLog(pMac, LOGW, FL("Session Can not be created \n"));
                retCode = eSIR_SME_RESOURCES_UNAVAILABLE;
                goto free;
            }

        }
     
        /* Store the session related parameters in newly created session */
        psessionEntry->pLimStartBssReq = pSmeStartBssReq;

        /* Store PE sessionId in session Table  */
        psessionEntry->peSessionId = sessionId;

        /* Store SME session Id in sessionTable */
        psessionEntry->smeSessionId = pSmeStartBssReq->sessionId;

        psessionEntry->transactionId = pSmeStartBssReq->transactionId;
                     
        sirCopyMacAddr(psessionEntry->selfMacAddr,pSmeStartBssReq->selfMacAddr);
        
        /* Copy SSID to session table */
        palCopyMemory( pMac->hHdd, (tANI_U8 *)&psessionEntry->ssId,
                     (tANI_U8 *)&pSmeStartBssReq->ssId,
                      (pSmeStartBssReq->ssId.length + 1));
        
       
        
        psessionEntry->bssType = pSmeStartBssReq->bssType;
        
        psessionEntry->nwType = pSmeStartBssReq->nwType;
        
        psessionEntry->beaconParams.beaconInterval = pSmeStartBssReq->beaconInterval;

        /* Store the channel number in session Table */
        psessionEntry->currentOperChannel = pSmeStartBssReq->channelId;

        /*Store Persona */
        psessionEntry->pePersona = pSmeStartBssReq->bssPersona;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,FL("PE PERSONA=%d\n"),
            psessionEntry->pePersona);

        /*Update the phymode*/
        psessionEntry->gLimPhyMode = pSmeStartBssReq->nwType;

        psessionEntry->maxTxPower = cfgGetRegulatoryMaxTransmitPower( pMac, 
            psessionEntry->currentOperChannel );
        /* Store the dot 11 mode in to the session Table*/

        psessionEntry->dot11mode = pSmeStartBssReq->dot11mode;
        psessionEntry->htCapabality = IS_DOT11_MODE_HT(psessionEntry->dot11mode);

        palCopyMemory(pMac->hHdd, (void*)&psessionEntry->rateSet,
            (void*)&pSmeStartBssReq->operationalRateSet,
            sizeof(tSirMacRateSet));
        palCopyMemory(pMac->hHdd, (void*)&psessionEntry->extRateSet,
            (void*)&pSmeStartBssReq->extendedRateSet,
            sizeof(tSirMacRateSet));

        switch(pSmeStartBssReq->bssType)
        {
#ifdef WLAN_SOFTAP_FEATURE
            case eSIR_INFRA_AP_MODE:
                 psessionEntry->limSystemRole = eLIM_AP_ROLE;
                 psessionEntry->privacy = pSmeStartBssReq->privacy;
                 psessionEntry->fwdWPSPBCProbeReq = pSmeStartBssReq->fwdWPSPBCProbeReq;
                 psessionEntry->authType = pSmeStartBssReq->authType;
                 /* Store the DTIM period */
                 psessionEntry->dtimPeriod = (tANI_U8)pSmeStartBssReq->dtimPeriod;
                 /*Enable/disable UAPSD*/
                 psessionEntry->apUapsdEnable = pSmeStartBssReq->apUapsdEnable;
                 if (psessionEntry->pePersona == VOS_P2P_GO_MODE)
                 {
                     psessionEntry->proxyProbeRspEn = 0;
                 }
                 else
                 {
                     /* To detect PBC overlap in SAP WPS mode, Host handles
                      * Probe Requests.
                      */
                     if(SAP_WPS_DISABLED == pSmeStartBssReq->wps_state)
                     {
                         psessionEntry->proxyProbeRspEn = 1;
                     }
                     else
                     {
                         psessionEntry->proxyProbeRspEn = 0;
                     }
                 }
                 psessionEntry->ssidHidden = pSmeStartBssReq->ssidHidden;
                 psessionEntry->wps_state = pSmeStartBssReq->wps_state;
                 break;
#endif
            case eSIR_IBSS_MODE:
                 psessionEntry->limSystemRole = eLIM_STA_IN_IBSS_ROLE;
                 break;

            case eSIR_BTAMP_AP_MODE:
                 psessionEntry->limSystemRole = eLIM_BT_AMP_AP_ROLE;
                 break;

            case eSIR_BTAMP_STA_MODE:
                 psessionEntry->limSystemRole = eLIM_BT_AMP_STA_ROLE;
                 break;

                 /* There is one more mode called auto mode. which is used no where */

                 //FORBUILD -TEMPFIX.. HOW TO use AUTO MODE?????


            default:
                                   //not used anywhere...used in scan function
                 break;
        }

        // BT-AMP: Allocate memory for the array of parsed (Re)Assoc request structure
        if ( (pSmeStartBssReq->bssType == eSIR_BTAMP_AP_MODE)
#ifdef WLAN_SOFTAP_FEATURE
        || (pSmeStartBssReq->bssType == eSIR_INFRA_AP_MODE)
#endif
        )
        {
            if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&psessionEntry->parsedAssocReq,
                                                          (psessionEntry->dph.dphHashTable.size * sizeof(tpSirAssocReq)) ))
            {
                limLog(pMac, LOGW, FL("palAllocateMemory() failed\n"));
                retCode = eSIR_SME_RESOURCES_UNAVAILABLE;
                goto free;
            }
            palZeroMemory(pMac->hHdd, psessionEntry->parsedAssocReq, (psessionEntry->dph.dphHashTable.size * sizeof(tpSirAssocReq)) );
        }

        /* Channel Bonding is not addressd yet for BT-AMP Support.. sunit will address channel bonding   */
        if (pSmeStartBssReq->channelId)
        {
            channelNumber = pSmeStartBssReq->channelId;
            /*Update cbMode received from sme with LIM's updated cbMode*/
            pSmeStartBssReq->cbMode = (tAniCBSecondaryMode)pMac->lim.gCbMode;

            setupCBState( pMac, pSmeStartBssReq->cbMode );
            pMac->lim.gHTSecondaryChannelOffset = limGetHTCBState(pSmeStartBssReq->cbMode);
#ifdef WLAN_SOFTAP_FEATURE
            txWidthSet = (tSirMacHTChannelWidth)limGetHTCapability(pMac, eHT_RECOMMENDED_TX_WIDTH_SET, psessionEntry);
#else
            txWidthSet = (tSirMacHTChannelWidth)limGetHTCapability(pMac, eHT_RECOMMENDED_TX_WIDTH_SET);
#endif

            /*
                * If there is a mismatch in secondaryChannelOffset being passed in the START_BSS request and
                * ChannelBonding CFG, then MAC will override the 'ChannelBonding' CFG with what is being passed
                * in StartBss Request.
                * HAL RA and PHY will go out of sync, if both these values are not consistent and will result in TXP Errors
                * when HAL RA tries to use 40Mhz rates when CB is turned off in PHY.
                */
            if(((pMac->lim.gHTSecondaryChannelOffset == eHT_SECONDARY_CHANNEL_OFFSET_NONE) &&
                    (txWidthSet == eHT_CHANNEL_WIDTH_40MHZ)) ||
                    ((pMac->lim.gHTSecondaryChannelOffset != eHT_SECONDARY_CHANNEL_OFFSET_NONE) &&
                    (txWidthSet == eHT_CHANNEL_WIDTH_20MHZ)))
                {
                   PELOGW(limLog(pMac, LOGW, FL("secondaryChannelOffset and txWidthSet don't match, resetting txWidthSet CFG\n"));)
                    txWidthSet = (txWidthSet == eHT_CHANNEL_WIDTH_20MHZ) ? eHT_CHANNEL_WIDTH_40MHZ : eHT_CHANNEL_WIDTH_20MHZ;
                    if (cfgSetInt(pMac, WNI_CFG_CHANNEL_BONDING_MODE, txWidthSet)
                                        != eSIR_SUCCESS)
                    {
                        limLog(pMac, LOGP, FL("could not set  WNI_CFG_CHANNEL_BONDING_MODE at CFG\n"));
                        retCode = eSIR_LOGP_EXCEPTION;
                        goto free;
                    }
                }
        }

        else
        {
            PELOGW(limLog(pMac, LOGW, FL("Received invalid eWNI_SME_START_BSS_REQ\n"));)
            retCode = eSIR_SME_INVALID_PARAMETERS;
            goto free;
        }

        // Delete pre-auth list if any
        limDeletePreAuthList(pMac);

        // Delete IBSS peer BSSdescription list if any
        //limIbssDelete(pMac); sep 26 review



#ifdef FIXME_GEN6   //following code may not be required. limInitMlm is now being invoked during peStart
        /// Initialize MLM state machine
#ifdef ANI_PRODUCT_TYPE_AP
            /* The Role is not set yet. Currently assuming the AddBss in Linux will be called by AP only.
             * This should be handled when IBSS functionality is implemented in the Linux
             * TODO */
            pMac->lim.gLimMlmState = eLIM_MLM_IDLE_STATE;
            MTRACE(macTrace(pMac, TRACE_CODE_MLM_STATE, 0, pMac->lim.gLimMlmState));
#else
        limInitMlm(pMac);
#endif
#endif

        psessionEntry->htCapabality = IS_DOT11_MODE_HT(pSmeStartBssReq->dot11mode);

#ifdef WLAN_SOFTAP_FEATURE
            /* keep the RSN/WPA IE information in PE Session Entry
             * later will be using this to check when received (Re)Assoc req
             * */
        limSetRSNieWPAiefromSmeStartBSSReqMessage(pMac,&pSmeStartBssReq->rsnIE,psessionEntry);

#endif

#ifdef WLAN_SOFTAP_FEATURE
        //Taken care for only softAP case rest need to be done
        if (psessionEntry->limSystemRole == eLIM_AP_ROLE){
            psessionEntry->gLimProtectionControl =  pSmeStartBssReq->protEnabled;
            /*each byte will have the following info
             *bit7       bit6    bit5   bit4 bit3   bit2  bit1 bit0
             *reserved reserved RIFS Lsig n-GF ht20 11g 11b*/
            palCopyMemory( pMac->hHdd, (void *) &psessionEntry->cfgProtection,
                          (void *) &pSmeStartBssReq->ht_capab,
                          sizeof( tCfgProtection ));
            psessionEntry->pAPWPSPBCSession = NULL; // Initialize WPS PBC session link list
        }
#endif

        // Prepare and Issue LIM_MLM_START_REQ to MLM
        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmStartReq, sizeof(tLimMlmStartReq)))
        {
            limLog(pMac, LOGP, FL("call to palAllocateMemory failed for mlmStartReq\n"));
            retCode = eSIR_SME_RESOURCES_UNAVAILABLE;
            goto free;
        }

        (void)palZeroMemory(pMac->hHdd, (void *) pMlmStartReq, sizeof(tLimMlmStartReq));

        /* Copy SSID to the MLM start structure */
        palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMlmStartReq->ssId,
                          (tANI_U8 *) &pSmeStartBssReq->ssId,
                          pSmeStartBssReq->ssId.length + 1);
#ifdef WLAN_SOFTAP_FEATURE
        pMlmStartReq->ssidHidden = pSmeStartBssReq->ssidHidden;
        pMlmStartReq->obssProtEnabled = pSmeStartBssReq->obssProtEnabled;
#endif


        pMlmStartReq->bssType = psessionEntry->bssType;

        /* Fill PE session Id from the session Table */
        pMlmStartReq->sessionId = psessionEntry->peSessionId;

        if( (pMlmStartReq->bssType == eSIR_BTAMP_STA_MODE) || (pMlmStartReq->bssType == eSIR_BTAMP_AP_MODE )
#ifdef WLAN_SOFTAP_FEATURE
            || (pMlmStartReq->bssType == eSIR_INFRA_AP_MODE)
#endif
        )
        {
            //len = sizeof(tSirMacAddr);
            //retStatus = wlan_cfgGetStr(pMac, WNI_CFG_STA_ID, (tANI_U8 *) pMlmStartReq->bssId, &len);
            //if (retStatus != eSIR_SUCCESS)
            //limLog(pMac, LOGP, FL("could not retrive BSSID, retStatus=%d\n"), retStatus);

            /* Copy the BSSId from sessionTable to mlmStartReq struct */
            sirCopyMacAddr(pMlmStartReq->bssId,psessionEntry->bssId);
        }

        else // ibss mode
        {
            pMac->lim.gLimIbssCoalescingHappened = false;

            if((retStatus = wlan_cfgGetInt(pMac, WNI_CFG_IBSS_AUTO_BSSID, &autoGenBssId)) != eSIR_SUCCESS)
            {
                limLog(pMac, LOGP, FL("Could not retrieve Auto Gen BSSID, retStatus=%d\n"), retStatus);
                retCode = eSIR_LOGP_EXCEPTION;
                goto free;
            }

            if(!autoGenBssId)
            {   
                // We're not auto generating BSSID. Instead, get it from session entry
                sirCopyMacAddr(pMlmStartReq->bssId,psessionEntry->bssId);
                
                if(pMlmStartReq->bssId[0] & 0x01)
                {
                   PELOGE(limLog(pMac, LOGE, FL("Request to start IBSS with group BSSID\n Autogenerating the BSSID\n"));)                    
                   autoGenBssId = TRUE;
                }             
            }

            if( autoGenBssId )
            {   //if BSSID is not any uc id. then use locally generated BSSID.
                //Autogenerate the BSSID
                limGetRandomBssid( pMac, pMlmStartReq->bssId);
                pMlmStartReq->bssId[0]= 0x02;
                
                /* Copy randomly generated BSSID to the session Table */
                sirCopyMacAddr(psessionEntry->bssId,pMlmStartReq->bssId);
            }
        }
        /* store the channel num in mlmstart req structure */
        pMlmStartReq->channelNumber = psessionEntry->currentOperChannel;
        pMlmStartReq->cbMode = pSmeStartBssReq->cbMode;        
        pMlmStartReq->beaconPeriod = psessionEntry->beaconParams.beaconInterval;

#ifdef WLAN_SOFTAP_FEATURE
        if(psessionEntry->limSystemRole == eLIM_AP_ROLE ){
            pMlmStartReq->dtimPeriod = psessionEntry->dtimPeriod;
            pMlmStartReq->wps_state = psessionEntry->wps_state;

        }else
#endif        
        {
            if (wlan_cfgGetInt(pMac, WNI_CFG_DTIM_PERIOD, &val) != eSIR_SUCCESS)
                limLog(pMac, LOGP, FL("could not retrieve DTIM Period\n"));
            pMlmStartReq->dtimPeriod = (tANI_U8)val;
        }   
            
        if (wlan_cfgGetInt(pMac, WNI_CFG_CFP_PERIOD, &val) != eSIR_SUCCESS)
            limLog(pMac, LOGP, FL("could not retrieve Beacon interval\n"));
        pMlmStartReq->cfParamSet.cfpPeriod = (tANI_U8)val;
            
        if (wlan_cfgGetInt(pMac, WNI_CFG_CFP_MAX_DURATION, &val) != eSIR_SUCCESS)
            limLog(pMac, LOGP, FL("could not retrieve CFPMaxDuration\n"));
        pMlmStartReq->cfParamSet.cfpMaxDuration = (tANI_U16) val;
        
        //this may not be needed anymore now, as rateSet is now included in the session entry and MLM has session context.
        palCopyMemory(pMac->hHdd, (void*)&pMlmStartReq->rateSet, (void*)&psessionEntry->rateSet,
                       sizeof(tSirMacRateSet));

                      
        // Now populate the 11n related parameters
        pMlmStartReq->nwType    = psessionEntry->nwType;
        pMlmStartReq->htCapable = psessionEntry->htCapabality;
        //
        // FIXME_GEN4 - Determine the appropriate defaults...
        //
        pMlmStartReq->htOperMode        = pMac->lim.gHTOperMode;
        pMlmStartReq->dualCTSProtection = pMac->lim.gHTDualCTSProtection; // Unused
        pMlmStartReq->txChannelWidthSet = pMac->lim.gHTRecommendedTxWidthSet;

        //Update the global LIM parameter, which is used to populate HT Info IEs in beacons/probe responses.
        pMac->lim.gHTSecondaryChannelOffset = limGetHTCBState(pMlmStartReq->cbMode);

        /* sep26 review */
        psessionEntry->limRFBand = limGetRFBand(channelNumber);

        // Initialize 11h Enable Flag
        psessionEntry->lim11hEnable = 0;
        if((pMlmStartReq->bssType != eSIR_IBSS_MODE) &&
            (SIR_BAND_5_GHZ == psessionEntry->limRFBand) )
        {
            if (wlan_cfgGetInt(pMac, WNI_CFG_11H_ENABLED, &val) != eSIR_SUCCESS)
                limLog(pMac, LOGP, FL("Fail to get WNI_CFG_11H_ENABLED \n"));
            psessionEntry->lim11hEnable = val;
        }
            
        if (!psessionEntry->lim11hEnable)
        {
            if (cfgSetInt(pMac, WNI_CFG_LOCAL_POWER_CONSTRAINT, 0) != eSIR_SUCCESS)
                limLog(pMac, LOGP, FL("Fail to get WNI_CFG_11H_ENABLED \n"));
        }

#ifdef ANI_PRODUCT_TYPE_AP
        PELOGE(limLog(pMac, LOGE, FL("Dot 11h is %s\n"), pMac->lim.gLim11hEnable?"Enabled":"Disabled");)
        if (pMac->lim.gLim11hEnable)
        { 
           PELOG2(limLog(pMac, LOG2, FL("Cb state = %d, SecChanOffset = %d\n"),
                   pMac->lim.gCbState, pMac->lim.gHTSecondaryChannelOffset);)
            limRadarInit(pMac);
        }
#endif
        psessionEntry ->limPrevSmeState = psessionEntry->limSmeState;
        psessionEntry ->limSmeState     =  eLIM_SME_WT_START_BSS_STATE;
     MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));

        limPostMlmMessage(pMac, LIM_MLM_START_REQ, (tANI_U32 *) pMlmStartReq);
        return;
    }
    else
    {
       
        limLog(pMac, LOGE, FL("Received unexpected START_BSS_REQ, in state %X\n"),pMac->lim.gLimSmeState);
        retCode = eSIR_SME_BSS_ALREADY_STARTED_OR_JOINED;
        goto end;
    } // if (pMac->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE)

free:
    palFreeMemory( pMac->hHdd, pSmeStartBssReq);
    pSmeStartBssReq = NULL;

end:

    /* This routine should return the sme sessionId and SME transaction Id */
    limGetSessionInfo(pMac,(tANI_U8*)pMsgBuf,&smesessionId,&smetransactionId); 
    
    if(NULL != psessionEntry)
    {
        peDeleteSession(pMac,psessionEntry);
        psessionEntry = NULL;
    }     
    limSendSmeStartBssRsp(pMac, eWNI_SME_START_BSS_RSP, retCode,psessionEntry,smesessionId,smetransactionId);
} /*** end __limHandleSmeStartBssRequest() ***/


/**--------------------------------------------------------------
\fn     __limProcessSmeStartBssReq

\brief  Wrapper for the function __limHandleSmeStartBssRequest
        This message will be defered until softmac come out of
        scan mode or if we have detected radar on the current
        operating channel.
\param  pMac
\param  pMsg

\return TRUE - If we consumed the buffer
        FALSE - If have defered the message.
 ---------------------------------------------------------------*/
static tANI_BOOLEAN
__limProcessSmeStartBssReq(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    if (__limIsDeferedMsgForLearn(pMac, pMsg) ||
            __limIsDeferedMsgForRadar(pMac, pMsg))
    {
        /**
         * If message defered, buffer is not consumed yet.
         * So return false
         */
        return eANI_BOOLEAN_FALSE;
    }

    __limHandleSmeStartBssRequest(pMac, (tANI_U32 *) pMsg->bodyptr);
    return eANI_BOOLEAN_TRUE;
}


/**
 *  limGetRandomBssid()
 *
 *  FUNCTION:This function is called to process generate the random number for bssid
 *  This function is called to process SME_SCAN_REQ message
 *  from HDD or upper layer application.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 * 1. geneartes the unique random number for bssid in ibss
 *
 *  @param  pMac      Pointer to Global MAC structure
 *  @param  *data      Pointer to  bssid  buffer
 *  @return None
 */
void limGetRandomBssid(tpAniSirGlobal pMac, tANI_U8 *data)
{
     tANI_U32 random[2] ;
     random[0] = tx_time_get();
     random[0] |= (random[0] << 15) ;
     random[1] = random[0] >> 1;
     palCopyMemory(pMac->hHdd, data, (tANI_U8*)random, sizeof(tSirMacAddr));
}


/**
 * __limProcessSmeScanReq()
 *
 *FUNCTION:
 * This function is called to process SME_SCAN_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 * 1. Periodic scanning should be requesting to return unique
 *    scan results.
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeScanReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U32            len;
    tLimMlmScanReq      *pMlmScanReq;
    tpSirSmeScanReq     pScanReq;
    tANI_U8             i = 0;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_SCAN_REQ_EVENT, NULL, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    
    pScanReq = (tpSirSmeScanReq) pMsgBuf;   
    PELOG1(limLog(pMac, LOG1, FL("SME SCAN REQ numChan %d min %d max %d IELen %d first %d fresh %d unique %d type %d rsp %d\n"),
           pScanReq->channelList.numChannels,
           pScanReq->minChannelTime,
           pScanReq->maxChannelTime,
           pScanReq->uIEFieldLen, 
           pScanReq->returnAfterFirstMatch,
           pScanReq->returnFreshResults,
           pScanReq->returnUniqueResults,
           pScanReq->scanType, pMac->lim.gLimRspReqd ? 1 : 0);)
        
    /*copy the Self MAC address from SmeReq to the globalplace , used for sending probe req.discussed on code review sep18*/
    sirCopyMacAddr(pMac->lim.gSelfMacAddr,  pScanReq->selfMacAddr);

   /* This routine should return the sme sessionId and SME transaction Id */
       
    if (!limIsSmeScanReqValid(pMac, pScanReq))
    {
        PELOGW(limLog(pMac, LOGW, FL("Received SME_SCAN_REQ with invalid parameters\n"));)

        if (pMac->lim.gLimRspReqd)
        {
            pMac->lim.gLimRspReqd = false;

            limSendSmeScanRsp(pMac, sizeof(tSirSmeScanRsp), eSIR_SME_INVALID_PARAMETERS, pScanReq->sessionId, pScanReq->transactionId);

        } // if (pMac->lim.gLimRspReqd)

        return;
    }

    //if scan is disabled then return as invalid scan request.
    //if scan in power save is disabled, and system is in power save mode, then ignore scan request.
    if( (pMac->lim.fScanDisabled) || (!pMac->lim.gScanInPowersave && !limIsSystemInActiveState(pMac))  )
    {
        limSendSmeScanRsp(pMac, 8, eSIR_SME_INVALID_PARAMETERS, pScanReq->sessionId, pScanReq->transactionId);
        return;
    }
    

    /**
     * If scan request is received in idle, joinFailed
     * states or in link established state (in STA role)
     * or in normal state (in STA-in-IBSS/AP role) with
     * 'return fresh scan results' request from HDD or
     * it is periodic background scanning request,
     * trigger fresh scan request to MLM
     */
  if (__limFreshScanReqd(pMac, pScanReq->returnFreshResults))
  {
        #if 0   
        // Update global SME state
        pMac->lim.gLimPrevSmeState = pMac->lim.gLimSmeState;
        if ((pMac->lim.gLimSmeState == eLIM_SME_IDLE_STATE) ||
            (pMac->lim.gLimSmeState == eLIM_SME_JOIN_FAILURE_STATE))
            pMac->lim.gLimSmeState = eLIM_SME_WT_SCAN_STATE;
        else if (pMac->lim.gLimSmeState == eLIM_SME_NORMAL_STATE)
            pMac->lim.gLimSmeState = eLIM_SME_NORMAL_CHANNEL_SCAN_STATE;
        else
            
        #endif //TO SUPPORT BT-AMP

        /*Change Global SME state  */

        /* Store the previous SME state */

         pMac->lim.gLimPrevSmeState = pMac->lim.gLimSmeState;
        
        pMac->lim.gLimSmeState = eLIM_SME_WT_SCAN_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));

        if (pScanReq->returnFreshResults & SIR_BG_SCAN_PURGE_RESUTLS)
        {
            // Discard previously cached scan results
            limReInitScanResults(pMac);
        }

        pMac->lim.gLim24Band11dScanDone     = 0;
        pMac->lim.gLim50Band11dScanDone     = 0;
        pMac->lim.gLimReturnAfterFirstMatch =
                                    pScanReq->returnAfterFirstMatch;

        pMac->lim.gLimReturnUniqueResults   =
              ((pScanReq->returnUniqueResults) > 0 ? true : false);
        /* De-activate Heartbeat timers for connected sessions while
         * scan is in progress if the system is in Active mode */
        if((ePMM_STATE_BMPS_WAKEUP == pMac->pmm.gPmmState) ||
           (ePMM_STATE_READY == pMac->pmm.gPmmState))
        {
          for(i=0;i<pMac->lim.maxBssId;i++)
          {
            if((pMac->lim.gpSession[i].valid == TRUE) &&
               (eLIM_MLM_LINK_ESTABLISHED_STATE == pMac->lim.gpSession[i].limMlmState))
            {
               limHeartBeatDeactivateAndChangeTimer(pMac, peFindSessionBySessionId(pMac,i));
            }   
          }
        }

        if (pScanReq->channelList.numChannels == 0)
        {
            tANI_U32            cfg_len;
            // Scan all channels
            len = sizeof(tLimMlmScanReq) + 
                  (sizeof( pScanReq->channelList.channelNumber ) * (WNI_CFG_VALID_CHANNEL_LIST_LEN - 1)) +
                  pScanReq->uIEFieldLen;
            if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmScanReq, len) )
            {
                // Log error
                limLog(pMac, LOGP,
                       FL("call to palAllocateMemory failed for mlmScanReq (%d)\n"), len);

                return;
            }

            // Initialize this buffer
            palZeroMemory( pMac->hHdd, (tANI_U8 *) pMlmScanReq, len );

            cfg_len = WNI_CFG_VALID_CHANNEL_LIST_LEN;
            if (wlan_cfgGetStr(pMac, WNI_CFG_VALID_CHANNEL_LIST,
                          pMlmScanReq->channelList.channelNumber,
                          &cfg_len) != eSIR_SUCCESS)
            {
                /**
                 * Could not get Valid channel list from CFG.
                 * Log error.
                 */
                limLog(pMac, LOGP,
                       FL("could not retrieve Valid channel list\n"));
            }
            pMlmScanReq->channelList.numChannels = (tANI_U8) cfg_len;
        }
        else
        {
            len = sizeof( tLimMlmScanReq ) - sizeof( pScanReq->channelList.channelNumber ) + 
                   (sizeof( pScanReq->channelList.channelNumber ) * pScanReq->channelList.numChannels ) +
                   pScanReq->uIEFieldLen;

            if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmScanReq, len) )
            {
                // Log error
                limLog(pMac, LOGP,
                    FL("call to palAllocateMemory failed for mlmScanReq(%d)\n"), len);

                return;
            }

            // Initialize this buffer
            palZeroMemory( pMac->hHdd, (tANI_U8 *) pMlmScanReq, len);
            pMlmScanReq->channelList.numChannels =
                            pScanReq->channelList.numChannels;

            palCopyMemory( pMac->hHdd, pMlmScanReq->channelList.channelNumber,
                          pScanReq->channelList.channelNumber,
                          pScanReq->channelList.numChannels);
        }

        pMlmScanReq->uIEFieldLen = pScanReq->uIEFieldLen;
        pMlmScanReq->uIEFieldOffset = len - pScanReq->uIEFieldLen;
        
        if(pScanReq->uIEFieldLen)
        {
            palCopyMemory( pMac->hHdd, (tANI_U8 *)pMlmScanReq+ pMlmScanReq->uIEFieldOffset,
                          (tANI_U8 *)pScanReq+(pScanReq->uIEFieldOffset),
                          pScanReq->uIEFieldLen);
        }

        pMlmScanReq->bssType = pScanReq->bssType;
        palCopyMemory( pMac->hHdd, pMlmScanReq->bssId,
                      pScanReq->bssId,
                      sizeof(tSirMacAddr));
        pMlmScanReq->numSsid = pScanReq->numSsid;

        i = 0;
        while (i < pMlmScanReq->numSsid)
        {
            palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMlmScanReq->ssId[i],
                      (tANI_U8 *) &pScanReq->ssId[i],
                      pScanReq->ssId[i].length + 1);

            i++;
        } 
       

        pMlmScanReq->scanType = pScanReq->scanType;
        pMlmScanReq->backgroundScanMode = pScanReq->backgroundScanMode;
        pMlmScanReq->minChannelTime = pScanReq->minChannelTime;
        pMlmScanReq->maxChannelTime = pScanReq->maxChannelTime;
        pMlmScanReq->dot11mode = pScanReq->dot11mode;
#ifdef WLAN_FEATURE_P2P
        pMlmScanReq->p2pSearch = pScanReq->p2pSearch;
#endif

        //Store the smeSessionID and transaction ID for later use.
        pMac->lim.gSmeSessionId = pScanReq->sessionId;
        pMac->lim.gTransactionId = pScanReq->transactionId;

        // Issue LIM_MLM_SCAN_REQ to MLM
        limPostMlmMessage(pMac, LIM_MLM_SCAN_REQ, (tANI_U32 *) pMlmScanReq);

    } // if ((pMac->lim.gLimSmeState == eLIM_SME_IDLE_STATE) || ...
    
    else
    {
        /// In all other cases return 'cached' scan results
        if ((pMac->lim.gLimRspReqd) || pMac->lim.gLimReportBackgroundScanResults)
        {
            tANI_U16    scanRspLen = sizeof(tSirSmeScanRsp);

            pMac->lim.gLimRspReqd = false;

            if (pMac->lim.gLimSmeScanResultLength == 0)
            {
                limSendSmeScanRsp(pMac, scanRspLen, eSIR_SME_SUCCESS, pScanReq->sessionId, pScanReq->transactionId);
            }
            else
            {
                scanRspLen = sizeof(tSirSmeScanRsp) +
                             pMac->lim.gLimSmeScanResultLength -
                             sizeof(tSirBssDescription);
                limSendSmeScanRsp(pMac, scanRspLen, eSIR_SME_SUCCESS, pScanReq->sessionId, pScanReq->transactionId);
            }

            if (pScanReq->returnFreshResults & SIR_BG_SCAN_PURGE_RESUTLS)
            {
                // Discard previously cached scan results
                limReInitScanResults(pMac);
            }

        } // if (pMac->lim.gLimRspReqd)
    } // else ((pMac->lim.gLimSmeState == eLIM_SME_IDLE_STATE) || ...

#if defined(ANI_PRODUCT_TYPE_CLIENT) || defined(ANI_AP_CLIENT_SDK)
#ifdef BACKGROUND_SCAN_ENABLED
    // start background scans if needed
    // There is a bug opened against softmac. Need to enable when the bug is fixed.
    __limBackgroundScanInitiate(pMac);
#endif
#endif

} /*** end __limProcessSmeScanReq() ***/




/**
 * __limProcessSmeJoinReq()
 *
 *FUNCTION:
 * This function is called to process SME_JOIN_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeJoinReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
  //  tANI_U8             *pBuf;
    //tANI_U32            len;
//    tSirMacAddr         currentBssId;
    tpSirSmeJoinReq     pSmeJoinReq = NULL;
    tLimMlmJoinReq      *pMlmJoinReq;
    tSirResultCodes     retCode = eSIR_SME_SUCCESS;
    tANI_U32            val = 0;
    tANI_U16            nSize;
    tANI_U8             sessionId;
    tpPESession         psessionEntry = NULL;
    tANI_U8             smesessionId;
    tANI_U16            smetransactionId;
    tPowerdBm           localPowerConstraint = 0, regMax = 0;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    //Not sending any session, since it is not created yet. The response whould have correct state.
    limDiagEventReport(pMac, WLAN_PE_DIAG_JOIN_REQ_EVENT, NULL, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    PELOG1(limLog(pMac, LOG1, FL("Received SME_JOIN_REQ\n"));)

#ifdef WLAN_FEATURE_VOWIFI
    /* Need to read the CFG here itself as this is used in limExtractAPCapability() below.
    * This CFG is actually read in rrmUpdateConfig() which is called later. Because this is not
    * read, RRM related path before calling rrmUpdateConfig() is not getting executed causing issues 
    * like not honoring power constraint on 1st association after driver loading. */
    if (wlan_cfgGetInt(pMac, WNI_CFG_RRM_ENABLED, &val) != eSIR_SUCCESS)
        limLog(pMac, LOGP, FL("cfg get rrm enabled failed\n"));
    pMac->rrm.rrmPEContext.rrmEnable = (val) ? 1 : 0;
    val = 0;
#endif /* WLAN_FEATURE_VOWIFI */

   /**
     * Expect Join request in idle state.
     * Reassociate request is expected in link established state.
     */
    
    /* Global SME and LIM states are not defined yet for BT-AMP Support */
    if(pMac->lim.gLimSmeState == eLIM_SME_IDLE_STATE)
    {
        nSize = __limGetSmeJoinReqSizeForAlloc((tANI_U8*) pMsgBuf);
        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pSmeJoinReq, nSize))
        {
            limLog(pMac, LOGP, FL("call to palAllocateMemory failed for pSmeJoinReq\n"));
            retCode = eSIR_SME_RESOURCES_UNAVAILABLE;
            goto end;
        }
        (void) palZeroMemory(pMac->hHdd, (void *) pSmeJoinReq, nSize);
 
#if defined(ANI_PRODUCT_TYPE_CLIENT) || defined(ANI_AP_CLIENT_SDK)
        handleHTCapabilityandHTInfo(pMac);
#endif
        if ((limJoinReqSerDes(pMac, pSmeJoinReq, (tANI_U8 *)pMsgBuf) == eSIR_FAILURE) ||
                (!limIsSmeJoinReqValid(pMac, pSmeJoinReq)))
        {
            /// Received invalid eWNI_SME_JOIN_REQ
            // Log the event
            limLog(pMac, LOGW, FL("received SME_JOIN_REQ with invalid data\n"));
            retCode = eSIR_SME_INVALID_PARAMETERS;
            goto end;
        }

        //pMac->lim.gpLimJoinReq = pSmeJoinReq; TO SUPPORT BT-AMP ,review os sep 23

        /* check for the existence of start BSS session  */
#ifdef FIXME_GEN6    
        if(pSmeJoinReq->bsstype == eSIR_BTAMP_AP_MODE)
        {
            if(peValidateBtJoinRequest(pMac)!= TRUE)
            {
                limLog(pMac, LOGW, FL("Start Bss session not present::SME_JOIN_REQ in unexpected state\n"));
                retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
                psessionEntry = NULL;
                goto end;   
            }
        }
        
#endif


        if((psessionEntry = peFindSessionByBssid(pMac,pSmeJoinReq->bssDescription.bssId,&sessionId)) != NULL)
        {
            limLog(pMac, LOGE, FL("Session Already exists for given BSSID\n"));
            
            if(psessionEntry->limSmeState == eLIM_SME_LINK_EST_STATE)
            {
                // Received eWNI_SME_JOIN_REQ for same
                // BSS as currently associated.
                // Log the event and send success
                PELOGW(limLog(pMac, LOGW, FL("Received SME_JOIN_REQ for currently joined BSS\n"));)
                /// Send Join success response to host
                retCode = eSIR_SME_SUCCESS;
                goto end;
            }
            else
            {
                retCode = eSIR_SME_REFUSED;
                psessionEntry = NULL;
                goto end;
            }    
        }    
        else       /* Session Entry does not exist for given BSSId */
        {       
            /* Try to Create a new session */
            if((psessionEntry = peCreateSession(pMac,pSmeJoinReq->bssDescription.bssId,&sessionId, pMac->lim.maxStation)) == NULL)
            {
                limLog(pMac, LOGE, FL("Session Can not be created \n"));
                retCode = eSIR_SME_RESOURCES_UNAVAILABLE;
                goto end;
            }
        }   
        
        /* Store Session related parameters */
        /* Store PE session Id in session Table */
        psessionEntry->peSessionId = sessionId;

        /* store the smejoin req handle in session table */
        psessionEntry->pLimJoinReq = pSmeJoinReq;
        
        /* Store SME session Id in sessionTable */
        psessionEntry->smeSessionId = pSmeJoinReq->sessionId;

        /* Store SME transaction Id in session Table */
        psessionEntry->transactionId = pSmeJoinReq->transactionId;

        /* Store beaconInterval */
        psessionEntry->beaconParams.beaconInterval = pSmeJoinReq->bssDescription.beaconInterval;

        /* Copying of bssId is already done, while creating session */
        //sirCopyMacAddr(psessionEntry->bssId,pSmeJoinReq->bssId);
        sirCopyMacAddr(psessionEntry->selfMacAddr,pSmeJoinReq->selfMacAddr);
        psessionEntry->bssType = pSmeJoinReq->bsstype;

        psessionEntry->statypeForBss = STA_ENTRY_PEER;

        /* Copy the dot 11 mode in to the session table */

        psessionEntry->dot11mode  = pSmeJoinReq->dot11mode;
        psessionEntry->nwType = pSmeJoinReq->bssDescription.nwType;

        /*Phy mode*/
        psessionEntry->gLimPhyMode = pSmeJoinReq->bssDescription.nwType;

        /* Copy The channel Id to the session Table */
        psessionEntry->currentOperChannel = pSmeJoinReq->bssDescription.channelId;


        /*Store Persona */
        psessionEntry->pePersona = pSmeJoinReq->staPersona;
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_INFO,
                  FL("PE PERSONA=%d"), psessionEntry->pePersona);
        
        /* Copy the SSID from smejoinreq to session entry  */  
        psessionEntry->ssId.length = pSmeJoinReq->ssId.length;
        palCopyMemory( pMac->hHdd,psessionEntry->ssId.ssId,pSmeJoinReq->ssId.ssId,psessionEntry->ssId.length);
            
            /* Copy the SSID from smejoinreq to session entry  */  
            psessionEntry->ssId.length = pSmeJoinReq->ssId.length;
            palCopyMemory( pMac->hHdd,psessionEntry->ssId.ssId,pSmeJoinReq->ssId.ssId,psessionEntry->ssId.length);
            
            // Determin 11r or CCX connection based on input from SME
            // which inturn is dependent on the profile the user wants to connect
            // to, So input is coming from supplicant
#ifdef WLAN_FEATURE_VOWIFI_11R
            psessionEntry->is11Rconnection = pSmeJoinReq->is11Rconnection;
#endif
#ifdef FEATURE_WLAN_CCX
            psessionEntry->isCCXconnection = pSmeJoinReq->isCCXconnection;
#endif
#if defined WLAN_FEATURE_VOWIFI_11R || defined FEATURE_WLAN_CCX || defined(FEATURE_WLAN_LFR)
            psessionEntry->isFastTransitionEnabled = pSmeJoinReq->isFastTransitionEnabled;
#endif
            
#ifdef FEATURE_WLAN_LFR
            psessionEntry->isFastRoamIniFeatureEnabled = pSmeJoinReq->isFastRoamIniFeatureEnabled;
#endif
            if(psessionEntry->bssType == eSIR_INFRASTRUCTURE_MODE)
            {
                psessionEntry->limSystemRole = eLIM_STA_ROLE;
            }
            else if(psessionEntry->bssType == eSIR_BTAMP_AP_MODE)
            {
                psessionEntry->limSystemRole = eLIM_BT_AMP_STA_ROLE;
            }
            else
            {   
                /* Throw an error and return and make sure to delete the session.*/
                limLog(pMac, LOGW, FL("received SME_JOIN_REQ with invalid bss type\n"));
                retCode = eSIR_SME_INVALID_PARAMETERS;
                goto end;
            }    

        if(pSmeJoinReq->addIEScan.length)
        {
            palCopyMemory(pMac->hHdd, &psessionEntry->pLimJoinReq->addIEScan,
                          &pSmeJoinReq->addIEScan, sizeof(tSirAddie));
        }

        if(pSmeJoinReq->addIEAssoc.length)
        {
            palCopyMemory(pMac->hHdd, &psessionEntry->pLimJoinReq->addIEAssoc,
                          &pSmeJoinReq->addIEAssoc, sizeof(tSirAddie));
        }
                 
#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)

        val = sizeof(tLimMlmJoinReq) + sizeof(tSirMacSSidIE) +
              sizeof(tSirMacRateSetIE) + sizeof(tSirMacDsParamSetIE);
#else
        val = sizeof(tLimMlmJoinReq) + psessionEntry->pLimJoinReq->bssDescription.length + 2;
#endif
        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmJoinReq, val))
        {
            limLog(pMac, LOGP, FL("call to palAllocateMemory failed for mlmJoinReq\n"));
            return;
        }
        (void) palZeroMemory(pMac->hHdd, (void *) pMlmJoinReq, val);

        /* PE SessionId is stored as a part of JoinReq*/
        pMlmJoinReq->sessionId = psessionEntry->peSessionId;
        
        if (wlan_cfgGetInt(pMac, WNI_CFG_JOIN_FAILURE_TIMEOUT, (tANI_U32 *) &pMlmJoinReq->joinFailureTimeout)
            != eSIR_SUCCESS)
            limLog(pMac, LOGP, FL("could not retrieve JoinFailureTimer value\n"));

        /* copy operational rate from psessionEntry*/
        palCopyMemory(pMac->hHdd, (void*)&psessionEntry->rateSet, (void*)&pSmeJoinReq->operationalRateSet,
                            sizeof(tSirMacRateSet));
        palCopyMemory(pMac->hHdd, (void*)&psessionEntry->extRateSet, (void*)&pSmeJoinReq->extendedRateSet,
                            sizeof(tSirMacRateSet));
        //this may not be needed anymore now, as rateSet is now included in the session entry and MLM has session context.
        palCopyMemory(pMac->hHdd, (void*)&pMlmJoinReq->operationalRateSet, (void*)&psessionEntry->rateSet,
                           sizeof(tSirMacRateSet));

        psessionEntry->encryptType = pSmeJoinReq->UCEncryptionType;

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
        palCopyMemory( pMac->hHdd, pMlmJoinReq->bssDescription.bssId,
                       pMac->lim.gpLimJoinReq->neighborBssList.bssList[0].bssId,
                       sizeof(tSirMacAddr));
        
        pMlmJoinReq->bssDescription.capabilityInfo = 1;
           
        pMlmJoinReq->bssDescription.aniIndicator =
              (tANI_U8) pMac->lim.gpLimJoinReq->neighborBssList.bssList[0].wniIndicator;
              
        pMlmJoinReq->bssDescription.nwType =
            pMac->lim.gpLimJoinReq->neighborBssList.bssList[0].nwType;

        pMlmJoinReq->bssDescription.channelId =
            pMac->lim.gpLimJoinReq->neighborBssList.bssList[0].channelId;

            limCopyNeighborInfoToCfg(pMac,
                pMac->lim.gpLimJoinReq->neighborBssList.bssList[0], psessionEntry);

        palCopyMemory( pMac->hHdd, pMac->lim.gLimCurrentBssId,
            pMac->lim.gpLimJoinReq->neighborBssList.bssList[0].bssId,
            sizeof(tSirMacAddr));

        pMac->lim.gLimCurrentChannelId =
            pMac->lim.gpLimJoinReq->neighborBssList.bssList[0].channelId;

        pMac->lim.gLimCurrentBssCaps =
            pMac->lim.gpLimJoinReq->neighborBssList.bssList[0].capabilityInfo;

        pMac->lim.gLimCurrentTitanHtCaps =
             pMac->lim.gpLimJoinReq->neighborBssList.bssList[0].titanHtCaps;

        palCopyMemory( pMac->hHdd,
         (tANI_U8 *) &pMac->lim.gLimCurrentSSID,
         (tANI_U8 *) &pMac->lim.gpLimJoinReq->neighborBssList.bssList[0].ssId,
        pMac->lim.gpLimJoinReq->neighborBssList.bssList[0].ssId.length+1);
#else
        pMlmJoinReq->bssDescription.length = psessionEntry->pLimJoinReq->bssDescription.length;

        palCopyMemory( pMac->hHdd,
           (tANI_U8 *) &pMlmJoinReq->bssDescription.bssId,
           (tANI_U8 *) &psessionEntry->pLimJoinReq->bssDescription.bssId,
           psessionEntry->pLimJoinReq->bssDescription.length + 2);

#if 0

        pMac->lim.gLimCurrentChannelId =
           psessionEntry->pLimJoinReq->bssDescription.channelId;
#endif //oct 9th review remove globals

        
        psessionEntry->limCurrentBssCaps =
               psessionEntry->pLimJoinReq->bssDescription.capabilityInfo;
        

        psessionEntry->limCurrentTitanHtCaps=
                psessionEntry->pLimJoinReq->bssDescription.titanHtCaps;

            regMax = cfgGetRegulatoryMaxTransmitPower( pMac, psessionEntry->currentOperChannel ); 
            localPowerConstraint = regMax;
            limExtractApCapability( pMac,
               (tANI_U8 *) psessionEntry->pLimJoinReq->bssDescription.ieFields,
               limGetIElenFromBssDescription(&psessionEntry->pLimJoinReq->bssDescription),
               &psessionEntry->limCurrentBssQosCaps,
               &psessionEntry->limCurrentBssPropCap,
               &pMac->lim.gLimCurrentBssUapsd //TBD-RAJESH  make gLimCurrentBssUapsd this session specific
               , &localPowerConstraint
               ); 
#ifdef FEATURE_WLAN_CCX
            psessionEntry->maxTxPower = limGetMaxTxPower(regMax, localPowerConstraint, pMac->roam.configParam.nTxPowerCap);
#else
            psessionEntry->maxTxPower = VOS_MIN( regMax , (localPowerConstraint) );
#endif
#if defined WLAN_VOWIFI_DEBUG
        limLog( pMac, LOGE, "Regulatory max = %d, local power constraint = %d, max tx = %d", regMax, localPowerConstraint, psessionEntry->maxTxPower );
#endif

        if (pMac->lim.gLimCurrentBssUapsd)
        {
            pMac->lim.gUapsdPerAcBitmask = psessionEntry->pLimJoinReq->uapsdPerAcBitmask;
            limLog( pMac, LOG1, FL("UAPSD flag for all AC - 0x%2x\n"), pMac->lim.gUapsdPerAcBitmask);

            // resetting the dynamic uapsd mask 
            pMac->lim.gUapsdPerAcDeliveryEnableMask = 0;
            pMac->lim.gUapsdPerAcTriggerEnableMask = 0;
        }
#endif

        psessionEntry->limRFBand = limGetRFBand(psessionEntry->currentOperChannel);

        // Initialize 11h Enable Flag
        if(SIR_BAND_5_GHZ == psessionEntry->limRFBand)
        {
            if (wlan_cfgGetInt(pMac, WNI_CFG_11H_ENABLED, &val) != eSIR_SUCCESS)
                limLog(pMac, LOGP, FL("Fail to get WNI_CFG_11H_ENABLED \n"));
            psessionEntry->lim11hEnable = val;
        }
        else
            psessionEntry->lim11hEnable = 0;
        
        //To care of the scenario when STA transitions from IBSS to Infrastructure mode.
        pMac->lim.gLimIbssCoalescingHappened = false;

        psessionEntry->limPrevSmeState = psessionEntry->limSmeState;
        psessionEntry->limSmeState = eLIM_SME_WT_JOIN_STATE;
        MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));

        PELOG1(limLog(pMac, LOG1, FL("SME JoinReq: SSID %d.%c%c%c%c%c%c\n"),
               psessionEntry->ssId.length,
               psessionEntry->ssId.ssId[0],
               psessionEntry->ssId.ssId[1],
               psessionEntry->ssId.ssId[2],
               psessionEntry->ssId.ssId[3],
               psessionEntry->ssId.ssId[4],
               psessionEntry->ssId.ssId[5]);
        limLog(pMac, LOG1, FL("Channel %d, BSSID %x:%x:%x:%x:%x:%x\n"),
               psessionEntry->currentOperChannel,
               psessionEntry->bssId[0],
               psessionEntry->bssId[1],
               psessionEntry->bssId[2],
               psessionEntry->bssId[3],
               psessionEntry->bssId[4],
               psessionEntry->bssId[5]);)

        /* Indicate whether spectrum management is enabled*/
        psessionEntry->spectrumMgtEnabled = 
           pSmeJoinReq->spectrumMgtIndicator;
        /* Issue LIM_MLM_JOIN_REQ to MLM */
        limPostMlmMessage(pMac, LIM_MLM_JOIN_REQ, (tANI_U32 *) pMlmJoinReq);
        return;

    }
    else
    {
        /* Received eWNI_SME_JOIN_REQ un expected state */
        limLog(pMac, LOGE, FL("received unexpected SME_JOIN_REQ in state %X\n"), pMac->lim.gLimSmeState);
        limPrintSmeState(pMac, LOGE, pMac->lim.gLimSmeState);
        retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
        psessionEntry = NULL;
        goto end;
        
    }

end:
    limGetSessionInfo(pMac,(tANI_U8*)pMsgBuf,&smesessionId,&smetransactionId); 
    
    if(pSmeJoinReq)
    {
        palFreeMemory( pMac->hHdd, pSmeJoinReq);
        pSmeJoinReq = NULL;
        if (NULL != psessionEntry)
        {
            psessionEntry->pLimJoinReq = NULL;
        }
    }
    
    if(retCode != eSIR_SME_SUCCESS)
    {
        if(NULL != psessionEntry)
        {
            peDeleteSession(pMac,psessionEntry);
            psessionEntry = NULL;
        }
    } 

    limSendSmeJoinReassocRsp(pMac, eWNI_SME_JOIN_RSP, retCode, eSIR_MAC_UNSPEC_FAILURE_STATUS,psessionEntry,smesessionId,smetransactionId);
} /*** end __limProcessSmeJoinReq() ***/


#ifdef FEATURE_WLAN_CCX
tANI_U8 limGetMaxTxPower(tPowerdBm regMax, tPowerdBm apTxPower, tPowerdBm iniTxPower)
{
    tANI_U8 maxTxPower = 0;
    tANI_U8 txPower = VOS_MIN( regMax , (apTxPower) );
    txPower = VOS_MIN(txPower, iniTxPower);
    if((txPower >= MIN_TX_PWR_CAP) && (txPower <= MAX_TX_PWR_CAP))
        maxTxPower =  txPower;
    else if (txPower < MIN_TX_PWR_CAP)
        maxTxPower = MIN_TX_PWR_CAP;
    else
        maxTxPower = MAX_TX_PWR_CAP;

    return (maxTxPower);
}
#endif


#if 0
/**
 * __limProcessSmeAuthReq()
 *
 *FUNCTION:
 * This function is called to process SME_AUTH_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeAuthReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{

    tAniAuthType       authMode;
    tLimMlmAuthReq     *pMlmAuthReq;
    tpSirSmeAuthReq    pSirSmeAuthReq;
    tSirResultCodes    retCode = eSIR_SME_SUCCESS;
    tpPESession        psessionEntry;
    tANI_U8            sessionId;

    
    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
           return;
    }

    pSirSmeAuthReq = (tpSirSmeAuthReq) pMsgBuf;

    if((psessionEntry = peFindSessionByBssid(pMac,pSirSmeAuthReq->bssId,&sessionId))== NULL)
    {
        limLog(pMac, LOGE,FL("Session Does not exist for given BssId\n"));
        return;
    }

    if (!limIsSmeAuthReqValid(pSirSmeAuthReq))
    {
        limLog(pMac, LOGW,
               FL("received invalid SME_AUTH_REQ message\n"));

        /// Send AUTH failure response to host
        retCode = eSIR_SME_INVALID_PARAMETERS;
        goto end;
    }

    PELOG1(limLog(pMac, LOG1,
           FL("RECEIVED AUTH_REQ\n"));)

    /**
     * Expect Auth request for STA in link established state
     * or STA in IBSS mode in normal state.
     */

    if ((psessionEntry->limSmeState == eLIM_SME_LINK_EST_STATE) ||
        (psessionEntry->limSmeState == eLIM_SME_JOIN_FAILURE_STATE) ||
        ((psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE) &&
         (psessionEntry->limSmeState == eLIM_SME_NORMAL_STATE)))
    {
        if (pSirSmeAuthReq->authType == eSIR_AUTO_SWITCH)
            authMode = eSIR_SHARED_KEY; // Try Shared Key first
        else
            authMode = pSirSmeAuthReq->authType;

        // Trigger MAC based Authentication
        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmAuthReq, sizeof(tLimMlmAuthReq)))
        {
            // Log error
            limLog(pMac, LOGP,
                   FL("call to palAllocateMemory failed for mlmAuthReq\n"));
            return;
        }

        pMac->lim.gLimPreAuthType = pSirSmeAuthReq->authType;

        psessionEntry->limPrevSmeState = psessionEntry->limSmeState;
        psessionEntry->limSmeState     = eLIM_SME_WT_PRE_AUTH_STATE;
     MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));

        // Store channel specified in auth request.
        // This will be programmed later by MLM.
        pMac->lim.gLimPreAuthChannelNumber =
                                    (tSirMacChanNum)
                                    pSirSmeAuthReq->channelNumber;

        palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMac->lim.gLimPreAuthPeerAddr,
                      (tANI_U8 *) &pSirSmeAuthReq->peerMacAddr,
                      sizeof(tSirMacAddr));

        palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMlmAuthReq->peerMacAddr,
                      (tANI_U8 *) &pSirSmeAuthReq->peerMacAddr,
                      sizeof(tSirMacAddr));

        pMlmAuthReq->authType = authMode;
        
        /* Update PE session  Id */
        pMlmAuthReq->sessionId = sessionId;

        if (wlan_cfgGetInt(pMac, WNI_CFG_AUTHENTICATE_FAILURE_TIMEOUT,
                      (tANI_U32 *) &pMlmAuthReq->authFailureTimeout)
                            != eSIR_SUCCESS)
        {
            /**
             * Could not get AuthFailureTimeout value from CFG.
             * Log error.
             */
            limLog(pMac, LOGP,
                   FL("could not retrieve AuthFailureTimeout value\n"));
        }

        limPostMlmMessage(pMac, LIM_MLM_AUTH_REQ, (tANI_U32 *) pMlmAuthReq);
        return;
    }
    else
    {
        /// Should not have received eWNI_SME_AUTH_REQ
        // Log the event
        limLog(pMac, LOGE,
               FL("received unexpected SME_AUTH_REQ in state %X\n"),psessionEntry->limSmeState);
        limPrintSmeState(pMac, LOGE, psessionEntry->limSmeState);

        /// Send AUTH failure response to host
        retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
        goto end;
    }

end:
    limSendSmeAuthRsp(pMac, retCode,
                      pSirSmeAuthReq->peerMacAddr,
                      pSirSmeAuthReq->authType,
                      eSIR_MAC_UNSPEC_FAILURE_STATUS );

} /*** end __limProcessSmeAuthReq() ***/
#endif


/**
 * __limProcessSmeReassocReq()
 *
 *FUNCTION:
 * This function is called to process SME_REASSOC_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeReassocReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U16                caps;
    tANI_U32                val;
    tpSirSmeReassocReq pReassocReq = NULL;
    tLimMlmReassocReq  *pMlmReassocReq;
    tSirResultCodes    retCode = eSIR_SME_SUCCESS;
    tpPESession        psessionEntry = NULL;
    tANI_U8            sessionId; 
    tANI_U8            smeSessionId; 
    tANI_U16           transactionId; 
    tPowerdBm            localPowerConstraint = 0, regMax = 0;
    tANI_U32           teleBcnEn = 0;


    PELOG3(limLog(pMac, LOG3, FL("Received REASSOC_REQ\n"));)
    

    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pReassocReq, __limGetSmeJoinReqSizeForAlloc((tANI_U8 *) pMsgBuf)))
    {
        // Log error
        limLog(pMac, LOGP,
               FL("call to palAllocateMemory failed for pReassocReq\n"));

        retCode = eSIR_SME_RESOURCES_UNAVAILABLE;
        goto end;
    }
    

    if ((limJoinReqSerDes(pMac, (tpSirSmeJoinReq) pReassocReq,
                          (tANI_U8 *) pMsgBuf) == eSIR_FAILURE) ||
        (!limIsSmeJoinReqValid(pMac,
                               (tpSirSmeJoinReq) pReassocReq)))
    {
        /// Received invalid eWNI_SME_REASSOC_REQ
        // Log the event
        limLog(pMac, LOGW,
               FL("received SME_REASSOC_REQ with invalid data\n"));

        retCode = eSIR_SME_INVALID_PARAMETERS;
        goto end;
    }

   if((psessionEntry = peFindSessionByBssid(pMac,pReassocReq->bssDescription.bssId,&sessionId))==NULL)
    {
        limPrintMacAddr(pMac, pReassocReq->bssDescription.bssId, LOGE);
        limLog(pMac, LOGP, FL("Session does not exist for given bssId\n"));
        retCode = eSIR_SME_INVALID_PARAMETERS;
        goto end;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_REASSOC_REQ_EVENT, psessionEntry, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    //pMac->lim.gpLimReassocReq = pReassocReq;//TO SUPPORT BT-AMP

    /* Store the reassoc handle in the session Table.. 23rd sep review */
    psessionEntry->pLimReAssocReq = pReassocReq;

    /**
     * Reassociate request is expected
     * in link established state only.
     */

    if (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE)
    {
#if defined(WLAN_FEATURE_VOWIFI_11R) || defined(FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
        if (psessionEntry->limSmeState == eLIM_SME_WT_REASSOC_STATE)
        {
            // May be from 11r FT pre-auth. So lets check it before we bail out
            limLog(pMac, LOGE, FL("Session in reassoc state is %d\n"), 
                psessionEntry->peSessionId);

            // Make sure its our preauth bssid
            if (!palEqualMemory( pMac->hHdd, pReassocReq->bssDescription.bssId, 
                pMac->ft.ftPEContext.pFTPreAuthReq->preAuthbssId, 6))
            {
                limPrintMacAddr(pMac, pReassocReq->bssDescription.bssId, LOGE);
                limPrintMacAddr(pMac, pMac->ft.ftPEContext.pFTPreAuthReq->preAuthbssId, LOGE);
                limLog(pMac, LOGP, FL("Unknown bssId in reassoc state\n"));
                retCode = eSIR_SME_INVALID_PARAMETERS;
                goto end;
            }

            limProcessMlmFTReassocReq(pMac, pMsgBuf, psessionEntry);
            return;
        }
#endif
        /// Should not have received eWNI_SME_REASSOC_REQ
        // Log the event
        limLog(pMac, LOGE,
               FL("received unexpected SME_REASSOC_REQ in state %X\n"),
               psessionEntry->limSmeState);
        limPrintSmeState(pMac, LOGE, psessionEntry->limSmeState);

        retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
        goto end;
    }

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
    limCopyNeighborInfoToCfg(pMac,
        psessionEntry->pLimReAssocReq->neighborBssList.bssList[0],
        psessionEntry);

    palCopyMemory( pMac->hHdd,
             pMac->lim.gLimReassocBssId,
             psessionEntry->pLimReAssocReq->neighborBssList.bssList[0].bssId,
             sizeof(tSirMacAddr));

    pMac->lim.gLimReassocChannelId =
         psessionEntry->pLimReAssocReq->neighborBssList.bssList[0].channelId;

    pMac->lim.gLimReassocBssCaps =
    psessionEntry->pLimReAssocReq->neighborBssList.bssList[0].capabilityInfo;

    pMac->lim.gLimReassocTitanHtCaps = 
        psessionEntry->pLimReAssocReq->neighborBssList.bssList[0].titanHtCaps;

    palCopyMemory( pMac->hHdd,
    (tANI_U8 *) &pMac->lim.gLimReassocSSID,
    (tANI_U8 *) &psessionEntry->pLimReAssocReq->neighborBssList.bssList[0].ssId,
    psessionEntry->pLimReAssocReq->neighborBssList.bssList[0].ssId.length+1);
#else
    palCopyMemory( pMac->hHdd,
             psessionEntry->limReAssocbssId,
             psessionEntry->pLimReAssocReq->bssDescription.bssId,
             sizeof(tSirMacAddr));

    psessionEntry->limReassocChannelId =
         psessionEntry->pLimReAssocReq->bssDescription.channelId;

    psessionEntry->limReassocBssCaps =
                psessionEntry->pLimReAssocReq->bssDescription.capabilityInfo;

    psessionEntry->limReassocTitanHtCaps =
            psessionEntry->pLimReAssocReq->bssDescription.titanHtCaps;
    
    regMax = cfgGetRegulatoryMaxTransmitPower( pMac, psessionEntry->currentOperChannel ); 
    localPowerConstraint = regMax;
    limExtractApCapability( pMac,
              (tANI_U8 *) psessionEntry->pLimReAssocReq->bssDescription.ieFields,
              limGetIElenFromBssDescription(
                     &psessionEntry->pLimReAssocReq->bssDescription),
              &psessionEntry->limReassocBssQosCaps,
              &psessionEntry->limReassocBssPropCap,
              &pMac->lim.gLimCurrentBssUapsd //TBD-RAJESH make gLimReassocBssUapsd session specific
              , &localPowerConstraint
              );

    psessionEntry->maxTxPower = VOS_MIN( regMax , (localPowerConstraint) );
#if defined WLAN_VOWIFI_DEBUG
            limLog( pMac, LOGE, "Regulatory max = %d, local power constraint = %d, max tx = %d", regMax, localPowerConstraint, psessionEntry->maxTxPower );
#endif
    {
    #if 0
    if (wlan_cfgGetStr(pMac, WNI_CFG_SSID, pMac->lim.gLimReassocSSID.ssId,
                  &cfgLen) != eSIR_SUCCESS)
    {
        /// Could not get SSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrive SSID\n"));
    }
    #endif//TO SUPPORT BT-AMP
    
    /* Copy the SSID from sessio entry to local variable */
    #if 0
    palCopyMemory( pMac->hHdd, pMac->lim.gLimReassocSSID.ssId,
                   psessionEntry->ssId.ssId,
                   psessionEntry->ssId.length);
    #endif
    psessionEntry->limReassocSSID.length = pReassocReq->ssId.length;
    palCopyMemory( pMac->hHdd, psessionEntry->limReassocSSID.ssId,
                    pReassocReq->ssId.ssId, psessionEntry->limReassocSSID.length);

    }

    if (pMac->lim.gLimCurrentBssUapsd)
    {
        pMac->lim.gUapsdPerAcBitmask = psessionEntry->pLimReAssocReq->uapsdPerAcBitmask;
        limLog( pMac, LOG1, FL("UAPSD flag for all AC - 0x%2x\n"), pMac->lim.gUapsdPerAcBitmask);
    }

#endif

    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmReassocReq, sizeof(tLimMlmReassocReq)))
    {
        // Log error
        limLog(pMac, LOGP,
               FL("call to palAllocateMemory failed for mlmReassocReq\n"));

        retCode = eSIR_SME_RESOURCES_UNAVAILABLE;
        goto end;
    }

    palCopyMemory( pMac->hHdd, pMlmReassocReq->peerMacAddr,
                  psessionEntry->limReAssocbssId,
                  sizeof(tSirMacAddr));

    if (wlan_cfgGetInt(pMac, WNI_CFG_REASSOCIATION_FAILURE_TIMEOUT,
                  (tANI_U32 *) &pMlmReassocReq->reassocFailureTimeout)
                           != eSIR_SUCCESS)
    {
        /**
         * Could not get ReassocFailureTimeout value
         * from CFG. Log error.
         */
        limLog(pMac, LOGP,
               FL("could not retrieve ReassocFailureTimeout value\n"));
    }

    if (cfgGetCapabilityInfo(pMac, &caps,psessionEntry) != eSIR_SUCCESS)
    {
        /**
         * Could not get Capabilities value
         * from CFG. Log error.
         */
        limLog(pMac, LOGP,
               FL("could not retrieve Capabilities value\n"));
    }
    pMlmReassocReq->capabilityInfo = caps;
    
    /* Update PE sessionId*/
    pMlmReassocReq->sessionId = sessionId;

   /* If telescopic beaconing is enabled, set listen interval to
     WNI_CFG_TELE_BCN_MAX_LI */
    if(wlan_cfgGetInt(pMac, WNI_CFG_TELE_BCN_WAKEUP_EN, &teleBcnEn) != 
       eSIR_SUCCESS) 
       limLog(pMac, LOGP, FL("Couldn't get WNI_CFG_TELE_BCN_WAKEUP_EN\n"));
   
    val = WNI_CFG_LISTEN_INTERVAL_STADEF;
   
    if(teleBcnEn)
    {
       if(wlan_cfgGetInt(pMac, WNI_CFG_TELE_BCN_MAX_LI, &val) != 
          eSIR_SUCCESS)
       {
            /**
            * Could not get ListenInterval value
            * from CFG. Log error.
          */
          limLog(pMac, LOGP, FL("could not retrieve ListenInterval\n"));
       }
    }
    else
    {
       if (wlan_cfgGetInt(pMac, WNI_CFG_LISTEN_INTERVAL, &val) != eSIR_SUCCESS)
       {
         /**
            * Could not get ListenInterval value
            * from CFG. Log error.
          */
          limLog(pMac, LOGP, FL("could not retrieve ListenInterval\n"));
       }
    }

    /* Delete all BA sessions before Re-Assoc.
     *  BA frames are class 3 frames and the session 
     *  is lost upon disassociation and reassociation.
     */

    limDelAllBASessions(pMac);

    pMlmReassocReq->listenInterval = (tANI_U16) val;

    /* Indicate whether spectrum management is enabled*/
    psessionEntry->spectrumMgtEnabled = pReassocReq->spectrumMgtIndicator;

    psessionEntry->limPrevSmeState = psessionEntry->limSmeState;
    psessionEntry->limSmeState    = eLIM_SME_WT_REASSOC_STATE;

    MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));

    limPostMlmMessage(pMac,
                      LIM_MLM_REASSOC_REQ,
                      (tANI_U32 *) pMlmReassocReq);
    return;

end:
    if (pReassocReq)
        palFreeMemory( pMac->hHdd, pReassocReq);

    if (psessionEntry)
    {
       // error occurred after we determined the session so extract
       // session and transaction info from there
       smeSessionId = psessionEntry->smeSessionId;
       transactionId = psessionEntry->transactionId;
    }
    else
    {
       // error occurred before or during the time we determined the session
       // so extract the session and transaction info from the message
       limGetSessionInfo(pMac,(tANI_U8*)pMsgBuf, &smeSessionId, &transactionId);
    }

    /// Send Reassoc failure response to host
    /// (note psessionEntry may be NULL, but that's OK)
    limSendSmeJoinReassocRsp(pMac, eWNI_SME_REASSOC_RSP,
                             retCode, eSIR_MAC_UNSPEC_FAILURE_STATUS,
                             psessionEntry, smeSessionId, transactionId);

} /*** end __limProcessSmeReassocReq() ***/


tANI_BOOLEAN sendDisassocFrame = 1;
/**
 * __limProcessSmeDisassocReq()
 *
 *FUNCTION:
 * This function is called to process SME_DISASSOC_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeDisassocReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U16                disassocTrigger, reasonCode;
    tLimMlmDisassocReq      *pMlmDisassocReq;
    tSirResultCodes         retCode = eSIR_SME_SUCCESS;
    tSirRetStatus           status;
    tSirSmeDisassocReq      smeDisassocReq;
    tpPESession             psessionEntry = NULL; 
    tANI_U8                 sessionId;
    tANI_U8                 smesessionId;
    tANI_U16                smetransactionId;

    PELOG1(limLog(pMac, LOG1,FL("received DISASSOC_REQ message\n"));)
    
    if (pMsgBuf == NULL)
    {
        limLog(pMac, LOGE, FL("Buffer is Pointing to NULL\n"));
        return;
    }

    limGetSessionInfo(pMac, (tANI_U8 *)pMsgBuf,&smesessionId, &smetransactionId);

    status = limDisassocReqSerDes(pMac, &smeDisassocReq, (tANI_U8 *) pMsgBuf);
    
    if ( (eSIR_FAILURE == status) ||
         (!limIsSmeDisassocReqValid(pMac, &smeDisassocReq, psessionEntry)) )
    {
        PELOGE(limLog(pMac, LOGE,
               FL("received invalid SME_DISASSOC_REQ message\n"));)

        if (pMac->lim.gLimRspReqd)
        {
            pMac->lim.gLimRspReqd = false;

            retCode         = eSIR_SME_INVALID_PARAMETERS;
            disassocTrigger = eLIM_HOST_DISASSOC;
            goto sendDisassoc;
        }

        return;
    }


    PELOGE(limLog(pMac, LOGE,   FL("received DISASSOC_REQ message. Reason: %d SmeState: %d\n"), 
                                                        smeDisassocReq.reasonCode, pMac->lim.gLimSmeState);)


    if((psessionEntry = peFindSessionByBssid(pMac,smeDisassocReq.bssId,&sessionId))== NULL)
    {
        limLog(pMac, LOGE,FL("session does not exist for given bssId\n"));
        retCode = eSIR_SME_INVALID_PARAMETERS;
        disassocTrigger = eLIM_HOST_DISASSOC;
        goto sendDisassoc;
        
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
   limDiagEventReport(pMac, WLAN_PE_DIAG_DISASSOC_REQ_EVENT, psessionEntry, 0, smeDisassocReq.reasonCode);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    
    /* Update SME session Id and SME transaction ID*/

    psessionEntry->smeSessionId = smesessionId;
    psessionEntry->transactionId = smetransactionId;

    switch (psessionEntry->limSystemRole)
    {
        case eLIM_STA_ROLE:
        case eLIM_BT_AMP_STA_ROLE:
            switch (psessionEntry->limSmeState)
            {
                case eLIM_SME_ASSOCIATED_STATE:
                case eLIM_SME_LINK_EST_STATE:
                    psessionEntry->limPrevSmeState = psessionEntry->limSmeState;
                    psessionEntry->limSmeState= eLIM_SME_WT_DISASSOC_STATE;
                      MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));
                    break;

                case eLIM_SME_WT_DEAUTH_STATE:
                    /* PE shall still process the DISASSOC_REQ and proceed with 
                     * link tear down even if it had already sent a DEAUTH_IND to
                     * to SME. pMac->lim.gLimPrevSmeState shall remain the same as
                     * its been set when PE entered WT_DEAUTH_STATE. 
                     */                  
                    psessionEntry->limSmeState= eLIM_SME_WT_DISASSOC_STATE;
                    MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));
                    limLog(pMac, LOG1, FL("Rcvd SME_DISASSOC_REQ while in SME_WT_DEAUTH_STATE. \n"));
                    break;

                case eLIM_SME_WT_DISASSOC_STATE:
                    /* PE Recieved a Disassoc frame. Normally it gets DISASSOC_CNF but it
                     * received DISASSOC_REQ. Which means host is also trying to disconnect.
                     * PE can continue processing DISASSOC_REQ and send the response instead
                     * of failing the request. SME will anyway ignore DEAUTH_IND that was sent
                     * for disassoc frame.
                     *
                     * It will send a disassoc, which is ok. However, we can use the global flag
                     * sendDisassoc to not send disassoc frame.
                     */
                    limLog(pMac, LOG1, FL("Rcvd SME_DISASSOC_REQ while in SME_WT_DISASSOC_STATE. \n"));
                    break;

                case eLIM_SME_JOIN_FAILURE_STATE: {
                    /** Return Success as we are already in Disconnected State*/
                     if (pMac->lim.gLimRspReqd) {
                        retCode = eSIR_SME_SUCCESS;  
                        disassocTrigger = eLIM_HOST_DISASSOC;
                        goto sendDisassoc;
                    }
                }break;
                default:
                    /**
                     * STA is not currently associated.
                     * Log error and send response to host
                     */
                    limLog(pMac, LOGE,
                       FL("received unexpected SME_DISASSOC_REQ in state %X\n"),
                       psessionEntry->limSmeState);
                    limPrintSmeState(pMac, LOGE, psessionEntry->limSmeState);

                    if (pMac->lim.gLimRspReqd)
                    {
                        if (psessionEntry->limSmeState !=
                                                eLIM_SME_WT_ASSOC_STATE)
                                    pMac->lim.gLimRspReqd = false;

                        retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
                        disassocTrigger = eLIM_HOST_DISASSOC;
                        goto sendDisassoc;
                    }

                    return;
            }

            break;

        case eLIM_AP_ROLE:
    case eLIM_BT_AMP_AP_ROLE:
            // Fall through
            break;

        case eLIM_STA_IN_IBSS_ROLE:
        default: // eLIM_UNKNOWN_ROLE
            limLog(pMac, LOGE,
               FL("received unexpected SME_DISASSOC_REQ for role %d\n"),
               psessionEntry->limSystemRole);

            retCode = eSIR_SME_UNEXPECTED_REQ_RESULT_CODE;
            disassocTrigger = eLIM_HOST_DISASSOC;
            goto sendDisassoc;
    } // end switch (pMac->lim.gLimSystemRole)

    if (smeDisassocReq.reasonCode == eLIM_LINK_MONITORING_DISASSOC)
    {
        /// Disassociation is triggered by Link Monitoring
        disassocTrigger = eLIM_LINK_MONITORING_DISASSOC;
        reasonCode      = eSIR_MAC_DISASSOC_DUE_TO_INACTIVITY_REASON;
    }
    else
    {
        disassocTrigger = eLIM_HOST_DISASSOC;
        reasonCode      = smeDisassocReq.reasonCode;
    }

    if (smeDisassocReq.doNotSendOverTheAir)
    {
        sendDisassocFrame = 0;     
    }
    // Trigger Disassociation frame to peer MAC entity
    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmDisassocReq, sizeof(tLimMlmDisassocReq)))
    {
        // Log error
        limLog(pMac, LOGP,
               FL("call to palAllocateMemory failed for mlmDisassocReq\n"));

        return;
    }

    palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMlmDisassocReq->peerMacAddr,
                  (tANI_U8 *) &smeDisassocReq.peerMacAddr,
                  sizeof(tSirMacAddr));

    pMlmDisassocReq->reasonCode      = reasonCode;
    pMlmDisassocReq->disassocTrigger = disassocTrigger;
    
    /* Update PE session ID*/
    pMlmDisassocReq->sessionId = sessionId;
#ifdef ANI_PRODUCT_TYPE_AP
    pMlmDisassocReq->aid             = smeDisassocReq.aid;
#endif

    limPostMlmMessage(pMac,
                      LIM_MLM_DISASSOC_REQ,
                      (tANI_U32 *) pMlmDisassocReq);
    return;

sendDisassoc:
    if (psessionEntry) 
        limSendSmeDisassocNtf(pMac, smeDisassocReq.peerMacAddr,
                          retCode,
                          disassocTrigger,
#ifdef ANI_PRODUCT_TYPE_AP
                          smeDisassocReq.aid);
#else
                          1,smesessionId,smetransactionId,psessionEntry);
#endif
    else 
        limSendSmeDisassocNtf(pMac, smeDisassocReq.peerMacAddr, 
                retCode, 
                disassocTrigger,
#ifdef ANI_PRODUCT_TYPE_AP
                smeDisassocReq.aid);
#else
                1, 0, 0, NULL);
#endif


} /*** end __limProcessSmeDisassocReq() ***/


/** -----------------------------------------------------------------
  \brief __limProcessSmeDisassocCnf() - Process SME_DISASSOC_CNF
   
  This function is called to process SME_DISASSOC_CNF message
  from HDD or upper layer application. 
    
  \param pMac - global mac structure
  \param pStaDs - station dph hash node 
  \return none 
  \sa
  ----------------------------------------------------------------- */
static void
__limProcessSmeDisassocCnf(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tSirSmeDisassocCnf  smeDisassocCnf;
    tANI_U16  aid;
    tpDphHashNode  pStaDs;
    tSirRetStatus  status = eSIR_SUCCESS;
    tpPESession         psessionEntry;
    tANI_U8             sessionId;


    PELOG1(limLog(pMac, LOG1, FL("received DISASSOC_CNF message\n"));)

    status = limDisassocCnfSerDes(pMac, &smeDisassocCnf,(tANI_U8 *) pMsgBuf);

    if (status == eSIR_FAILURE)
    {
        PELOGE(limLog(pMac, LOGE, FL("invalid SME_DISASSOC_CNF message\n"));)
        return;
    }

    if((psessionEntry = peFindSessionByBssid(pMac, smeDisassocCnf.bssId, &sessionId))== NULL)
    {
         limLog(pMac, LOGE,FL("session does not exist for given bssId\n"));
         return;
    }

    if (!limIsSmeDisassocCnfValid(pMac, &smeDisassocCnf, psessionEntry))
    {
        limLog(pMac, LOGW, FL("received invalid SME_DISASSOC_CNF message\n"));
        return;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    if (smeDisassocCnf.messageType == eWNI_SME_DISASSOC_CNF)
        limDiagEventReport(pMac, WLAN_PE_DIAG_DISASSOC_CNF_EVENT, psessionEntry, (tANI_U16)smeDisassocCnf.statusCode, 0);
    else if (smeDisassocCnf.messageType ==  eWNI_SME_DEAUTH_CNF)
        limDiagEventReport(pMac, WLAN_PE_DIAG_DEAUTH_CNF_EVENT, psessionEntry, (tANI_U16)smeDisassocCnf.statusCode, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    switch (psessionEntry->limSystemRole)
    {
        case eLIM_STA_ROLE:
        case eLIM_BT_AMP_STA_ROLE:  //To test reconn
            if ((psessionEntry->limSmeState != eLIM_SME_IDLE_STATE) &&
                (psessionEntry->limSmeState != eLIM_SME_WT_DISASSOC_STATE) &&
                (psessionEntry->limSmeState != eLIM_SME_WT_DEAUTH_STATE))
            {
                limLog(pMac, LOGE,
                   FL("received unexp SME_DISASSOC_CNF in state %X\n"),
                  psessionEntry->limSmeState);
                limPrintSmeState(pMac, LOGE, psessionEntry->limSmeState);
                return;
            }
            break;

        case eLIM_AP_ROLE:
            // Fall through
#ifdef WLAN_SOFTAP_FEATURE
            break;
#else
            return;
#endif

        case eLIM_STA_IN_IBSS_ROLE:
        default: // eLIM_UNKNOWN_ROLE
            limLog(pMac, LOGE,
               FL("received unexpected SME_DISASSOC_CNF role %d\n"),
               psessionEntry->limSystemRole);

            return;
    } 


    if ( (psessionEntry->limSmeState == eLIM_SME_WT_DISASSOC_STATE) || 
         (psessionEntry->limSmeState == eLIM_SME_WT_DEAUTH_STATE)
#ifdef WLAN_SOFTAP_FEATURE
          || (psessionEntry->limSystemRole == eLIM_AP_ROLE )   
#endif
     )
    {       
        pStaDs = dphLookupHashEntry(pMac, smeDisassocCnf.peerMacAddr, &aid, &psessionEntry->dph.dphHashTable);
        if (pStaDs == NULL)
        {
            PELOGW(limLog(pMac, LOGW, FL("received DISASSOC_CNF for a STA that does not have context, addr= "));
            limPrintMacAddr(pMac, smeDisassocCnf.peerMacAddr, LOGW);)
            return;
        }
        limCleanupRxPath(pMac, pStaDs, psessionEntry);
    }

    return;
} 


/**
 * __limProcessSmeDeauthReq()
 *
 *FUNCTION:
 * This function is called to process SME_DEAUTH_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeDeauthReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U16                deauthTrigger, reasonCode;
    tLimMlmDeauthReq        *pMlmDeauthReq;
    tSirSmeDeauthReq        smeDeauthReq;
    tSirResultCodes         retCode = eSIR_SME_SUCCESS;
    tSirRetStatus           status = eSIR_SUCCESS;
    tpPESession             psessionEntry; 
    tANI_U8                 sessionId; //PE sessionId
    tANI_U8                 smesessionId;  
    tANI_U16                smetransactionId;
    
    PELOG1(limLog(pMac, LOG1,FL("received DEAUTH_REQ message\n"));)

    status = limDeauthReqSerDes(pMac, &smeDeauthReq,(tANI_U8 *) pMsgBuf);

    limGetSessionInfo(pMac,(tANI_U8 *)pMsgBuf,&smesessionId,&smetransactionId);

    //We need to get a session first but we don't even know if the message is correct.
    if((psessionEntry = peFindSessionByBssid(pMac, smeDeauthReq.bssId, &sessionId)) == NULL)
    {
       limLog(pMac, LOGE,FL("session does not exist for given bssId\n"));
       retCode = eSIR_SME_INVALID_PARAMETERS;
       deauthTrigger = eLIM_HOST_DEAUTH;
       goto sendDeauth;
       
    }

    if ((status == eSIR_FAILURE) || (!limIsSmeDeauthReqValid(pMac, &smeDeauthReq, psessionEntry)))
    {
        PELOGE(limLog(pMac, LOGW,FL("received invalid SME_DEAUTH_REQ message\n"));)
        if (pMac->lim.gLimRspReqd)
        {
            pMac->lim.gLimRspReqd = false;

            retCode       = eSIR_SME_INVALID_PARAMETERS;
            deauthTrigger = eLIM_HOST_DEAUTH;
            goto sendDeauth;
        }

        return;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_DEAUTH_REQ_EVENT, psessionEntry, 0, smeDeauthReq.reasonCode);
#endif //FEATURE_WLAN_DIAG_SUPPORT

    /* Update SME session ID and Transaction ID */
    psessionEntry->smeSessionId = smesessionId;
    psessionEntry->transactionId = smetransactionId;
        

    switch (psessionEntry->limSystemRole)
    {
        case eLIM_STA_ROLE:
        case eLIM_BT_AMP_STA_ROLE:
            
            switch (psessionEntry->limSmeState)
            {
                case eLIM_SME_ASSOCIATED_STATE:
                case eLIM_SME_LINK_EST_STATE:
                case eLIM_SME_WT_ASSOC_STATE:
                case eLIM_SME_JOIN_FAILURE_STATE:
                case eLIM_SME_IDLE_STATE:
                    psessionEntry->limPrevSmeState = psessionEntry->limSmeState;
                    psessionEntry->limSmeState = eLIM_SME_WT_DEAUTH_STATE;
              MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));

                    // Send Deauthentication request to MLM below

                    break;

                default:
                    /**
                     * STA is not in a state to deauthenticate with
                     * peer. Log error and send response to host.
                     */
                    limLog(pMac, LOGE,
                    FL("received unexp SME_DEAUTH_REQ in state %X\n"),psessionEntry->limSmeState);
                    limPrintSmeState(pMac, LOGE, psessionEntry->limSmeState);

                    if (pMac->lim.gLimRspReqd)
                    {
                        pMac->lim.gLimRspReqd = false;

                        retCode       = eSIR_SME_STA_NOT_AUTHENTICATED;
                        deauthTrigger = eLIM_HOST_DEAUTH;
                        goto sendDeauth;
                    }

                    return;
            }

            break;

        case eLIM_STA_IN_IBSS_ROLE:

            return;

        case eLIM_AP_ROLE:
            // Fall through

            break;

        default:
            limLog(pMac, LOGE,
               FL("received unexpected SME_DEAUTH_REQ for role %X\n"),psessionEntry->limSystemRole);

            return;
    } // end switch (pMac->lim.gLimSystemRole)

    if (smeDeauthReq.reasonCode == eLIM_LINK_MONITORING_DEAUTH)
    {
        /// Deauthentication is triggered by Link Monitoring
        PELOG1(limLog(pMac, LOG1, FL("**** Lost link with AP ****\n"));)
        deauthTrigger = eLIM_LINK_MONITORING_DEAUTH;
        reasonCode    = eSIR_MAC_UNSPEC_FAILURE_REASON;
    }
    else
    {
        deauthTrigger = eLIM_HOST_DEAUTH;
        reasonCode    = smeDeauthReq.reasonCode;
    }

    // Trigger Deauthentication frame to peer MAC entity
    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmDeauthReq, sizeof(tLimMlmDeauthReq)))
    {
        // Log error
        limLog(pMac, LOGP,
               FL("call to palAllocateMemory failed for mlmDeauthReq\n"));

        return;
    }

    palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMlmDeauthReq->peerMacAddr,
                  (tANI_U8 *) &smeDeauthReq.peerMacAddr,
                  sizeof(tSirMacAddr));

    pMlmDeauthReq->reasonCode = reasonCode;
    pMlmDeauthReq->deauthTrigger = deauthTrigger;
#ifdef ANI_PRODUCT_TYPE_AP
    pMlmDeauthReq->aid = smeDeauthReq.aid;
#endif

    /* Update PE session Id*/
    pMlmDeauthReq->sessionId = sessionId;

    limPostMlmMessage(pMac,
                      LIM_MLM_DEAUTH_REQ,
                      (tANI_U32 *) pMlmDeauthReq);
    return;

sendDeauth:
    limSendSmeDeauthNtf(pMac, smeDeauthReq.peerMacAddr,
                        retCode,
                        deauthTrigger,
#ifdef ANI_PRODUCT_TYPE_AP
                        smeDeauthReq.aid,
#else
                        1, 
#endif
                        smesessionId, smetransactionId);
} /*** end __limProcessSmeDeauthReq() ***/



/**
 * __limProcessSmeSetContextReq()
 *
 *FUNCTION:
 * This function is called to process SME_SETCONTEXT_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeSetContextReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tpSirSmeSetContextReq  pSetContextReq;
    tLimMlmSetKeysReq      *pMlmSetKeysReq;
    tpPESession             psessionEntry;
    tANI_U8                 sessionId;  //PE sessionID
    tANI_U8                 smesessionId;
    tANI_U16                smetransactionId;
    

    PELOG1(limLog(pMac, LOG1,
           FL("received SETCONTEXT_REQ message\n")););

    
    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
        return;
    }

    limGetSessionInfo(pMac,(tANI_U8 *)pMsgBuf,&smesessionId,&smetransactionId);
    
    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pSetContextReq,
                                                (sizeof(tSirKeys) * SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS)))
    {
        limLog(pMac, LOGP, FL("call to palAllocateMemory failed for pSetContextReq\n"));
        return;
    }

    if ((limSetContextReqSerDes(pMac, pSetContextReq, (tANI_U8 *) pMsgBuf) == eSIR_FAILURE) ||
        (!limIsSmeSetContextReqValid(pMac, pSetContextReq)))
    {
        limLog(pMac, LOGW, FL("received invalid SME_SETCONTEXT_REQ message\n")); 
        goto end;
    }

    if(pSetContextReq->keyMaterial.numKeys > SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS)
    {
        PELOGE(limLog(pMac, LOGE, FL("numKeys:%d is more than SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS\n"), pSetContextReq->keyMaterial.numKeys);)        
        limSendSmeSetContextRsp(pMac,
                                pSetContextReq->peerMacAddr,
#ifdef ANI_PRODUCT_TYPE_AP
                                pSetContextReq->aid,
#else
                                1,
#endif
                                eSIR_SME_INVALID_PARAMETERS,NULL,
                                smesessionId,smetransactionId);

        goto end;
    }


    if((psessionEntry = peFindSessionByBssid(pMac, pSetContextReq->bssId, &sessionId)) == NULL)
    {
        limLog(pMac, LOGW, FL("Session does not exist for given BSSID\n"));
        limSendSmeSetContextRsp(pMac,
                                pSetContextReq->peerMacAddr,
#ifdef ANI_PRODUCT_TYPE_AP
                                pSetContextReq->aid,
#else
                                1,
#endif
                                eSIR_SME_INVALID_PARAMETERS,NULL,
                                smesessionId,smetransactionId);

        goto end;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_SETCONTEXT_REQ_EVENT, psessionEntry, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT


    if ((((psessionEntry->limSystemRole == eLIM_STA_ROLE) || (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE)) &&
         (psessionEntry->limSmeState == eLIM_SME_LINK_EST_STATE)) ||
        (((psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE) ||
          (psessionEntry->limSystemRole == eLIM_AP_ROLE)|| (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE)) &&
         (psessionEntry->limSmeState == eLIM_SME_NORMAL_STATE)))
    {
        // Trigger MLM_SETKEYS_REQ
        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmSetKeysReq, sizeof(tLimMlmSetKeysReq)))
        {
            // Log error
            limLog(pMac, LOGP, FL("call to palAllocateMemory failed for mlmSetKeysReq\n"));
            goto end;
        }

        pMlmSetKeysReq->edType  = pSetContextReq->keyMaterial.edType;
        pMlmSetKeysReq->numKeys = pSetContextReq->keyMaterial.numKeys;
        if(pMlmSetKeysReq->numKeys > SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS)
        {
            limLog(pMac, LOGP, FL("Num of keys exceeded max num of default keys limit\n"));
            goto end;
        }
        palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMlmSetKeysReq->peerMacAddr,
                      (tANI_U8 *) &pSetContextReq->peerMacAddr,
                      sizeof(tSirMacAddr));

#ifdef ANI_PRODUCT_TYPE_AP
        pMlmSetKeysReq->aid = pSetContextReq->aid;
#endif

        palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMlmSetKeysReq->key,
                      (tANI_U8 *) &pSetContextReq->keyMaterial.key,
                      sizeof(tSirKeys) * (pMlmSetKeysReq->numKeys ? pMlmSetKeysReq->numKeys : 1));

        pMlmSetKeysReq->sessionId = sessionId;
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
        PELOG1(limLog(pMac, LOG1,
           FL("received SETCONTEXT_REQ message sessionId=%d\n"), pMlmSetKeysReq->sessionId););
#endif

#ifdef WLAN_SOFTAP_FEATURE
        if(((pSetContextReq->keyMaterial.edType == eSIR_ED_WEP40) || (pSetContextReq->keyMaterial.edType == eSIR_ED_WEP104))
        && (psessionEntry->limSystemRole == eLIM_AP_ROLE))
        {
            if(pSetContextReq->keyMaterial.key[0].keyLength)
            {
                tANI_U8 keyId;
                keyId = pSetContextReq->keyMaterial.key[0].keyId;
                palCopyMemory(pMac, (tANI_U8 *)&psessionEntry->WEPKeyMaterial[keyId],
                   (tANI_U8 *) &pSetContextReq->keyMaterial, sizeof(tSirKeyMaterial));
            }
            else {
                tANI_U32 i;
                for( i = 0; i < SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS; i++)
                {
                    palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMlmSetKeysReq->key[i],
                        (tANI_U8 *)psessionEntry->WEPKeyMaterial[i].key, sizeof(tSirKeys));
                }
            }
        }
#endif

        limPostMlmMessage(pMac, LIM_MLM_SETKEYS_REQ, (tANI_U32 *) pMlmSetKeysReq);

#ifdef ANI_AP_SDK
        /* For SDK acting as STA under Linux, need to consider the AP as *
         * as authenticatated.                                           */
        if ( (psessionEntry->limSystemRole == eLIM_STA_ROLE) &&
             (psessionEntry->limSmeState == eLIM_SME_LINK_EST_STATE))
        {
            tpDphHashNode pSta;
            pSta = dphGetHashEntry(pMac, 0, &psessionEntry->dph.dphHashTable);
            if (pSta)
                pSta->staAuthenticated = 1;
        }
#endif
    }
    else
    {
        limLog(pMac, LOGE,
           FL("received unexpected SME_SETCONTEXT_REQ for role %d, state=%X\n"),
           psessionEntry->limSystemRole,
           psessionEntry->limSmeState);
        limPrintSmeState(pMac, LOGE, psessionEntry->limSmeState);

        limSendSmeSetContextRsp(pMac, pSetContextReq->peerMacAddr,
#ifdef ANI_PRODUCT_TYPE_AP
                                pSetContextReq->aid,
#else
                                1,
#endif
                                eSIR_SME_UNEXPECTED_REQ_RESULT_CODE,psessionEntry,
                                smesessionId,
                                smetransactionId);
    }

end:
    palFreeMemory( pMac->hHdd, pSetContextReq);
    return;
} /*** end __limProcessSmeSetContextReq() ***/

/**
 * __limProcessSmeRemoveKeyReq()
 *
 *FUNCTION:
 * This function is called to process SME_REMOVEKEY_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeRemoveKeyReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tpSirSmeRemoveKeyReq    pRemoveKeyReq;
    tLimMlmRemoveKeyReq     *pMlmRemoveKeyReq;
    tpPESession             psessionEntry;
    tANI_U8                 sessionId;  //PE sessionID
    tANI_U8                 smesessionId;  
    tANI_U16                smetransactionId;

    PELOG1(limLog(pMac, LOG1,
           FL("received REMOVEKEY_REQ message\n"));)

    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
           return;
    }

    
    limGetSessionInfo(pMac,(tANI_U8 *)pMsgBuf,&smesessionId,&smetransactionId);

    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pRemoveKeyReq,
                                                (sizeof(*pRemoveKeyReq))))
    {
        //Log error
        limLog(pMac, LOGP,
               FL("call to palAllocateMemory failed for pRemoveKeyReq\n"));

        return;
     }

    if ((limRemoveKeyReqSerDes(pMac,
                                pRemoveKeyReq,
                                (tANI_U8 *) pMsgBuf) == eSIR_FAILURE))
    {
        limLog(pMac, LOGW,
               FL("received invalid SME_REMOVECONTEXT_REQ message\n"));

        /* extra look up is needed since, session entry to be passed il limsendremovekey response */

        if((psessionEntry = peFindSessionByBssid(pMac,pRemoveKeyReq->bssId,&sessionId))== NULL)
        {     
            limLog(pMac, LOGE,FL("session does not exist for given bssId\n"));
            //goto end;
        }

        limSendSmeRemoveKeyRsp(pMac,
                                pRemoveKeyReq->peerMacAddr,
                                eSIR_SME_INVALID_PARAMETERS,psessionEntry,
                                smesessionId,smetransactionId);

        goto end;
    }

    if((psessionEntry = peFindSessionByBssid(pMac,pRemoveKeyReq->bssId, &sessionId))== NULL)
    {
        limLog(pMac, LOGE,
                      FL("session does not exist for given bssId\n"));
        limSendSmeRemoveKeyRsp(pMac,
                                pRemoveKeyReq->peerMacAddr,
                                eSIR_SME_UNEXPECTED_REQ_RESULT_CODE, NULL,
                                smesessionId, smetransactionId);
        goto end;
    }


    if ((((psessionEntry->limSystemRole == eLIM_STA_ROLE)|| (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE))&&
         (psessionEntry->limSmeState == eLIM_SME_LINK_EST_STATE)) ||
        (((psessionEntry->limSystemRole == eLIM_STA_IN_IBSS_ROLE) ||
          (psessionEntry->limSystemRole == eLIM_AP_ROLE)|| (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE)) &&
         (psessionEntry->limSmeState == eLIM_SME_NORMAL_STATE)))
    {
        // Trigger MLM_REMOVEKEYS_REQ
        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmRemoveKeyReq, sizeof(tLimMlmRemoveKeyReq)))
        {
            // Log error
            limLog(pMac, LOGP,
                   FL("call to palAllocateMemory failed for mlmRemoveKeysReq\n"));

            goto end;
        }

        pMlmRemoveKeyReq->edType  = (tAniEdType)pRemoveKeyReq->edType; 
        pMlmRemoveKeyReq->keyId = pRemoveKeyReq->keyId;
        pMlmRemoveKeyReq->wepType = pRemoveKeyReq->wepType;
        pMlmRemoveKeyReq->unicast = pRemoveKeyReq->unicast;
        
        /* Update PE session Id */
        pMlmRemoveKeyReq->sessionId = sessionId;

        palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMlmRemoveKeyReq->peerMacAddr,
                      (tANI_U8 *) &pRemoveKeyReq->peerMacAddr,
                      sizeof(tSirMacAddr));


        limPostMlmMessage(pMac,
                          LIM_MLM_REMOVEKEY_REQ,
                          (tANI_U32 *) pMlmRemoveKeyReq);
    }
    else
    {
        limLog(pMac, LOGE,
           FL("received unexpected SME_REMOVEKEY_REQ for role %d, state=%X\n"),
           psessionEntry->limSystemRole,
           psessionEntry->limSmeState);
        limPrintSmeState(pMac, LOGE, psessionEntry->limSmeState);

        limSendSmeRemoveKeyRsp(pMac,
                                pRemoveKeyReq->peerMacAddr,
                                eSIR_SME_UNEXPECTED_REQ_RESULT_CODE,psessionEntry,
                                smesessionId,smetransactionId);
    }

end:
    palFreeMemory( pMac->hHdd, pRemoveKeyReq);
} /*** end __limProcessSmeRemoveKeyReq() ***/



#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
/**
 * __limHandleSmeSwitchChlRequest()
 *
 *FUNCTION:
 *  This function is called to process the following SME messages
 *  received from HDD or WSM:
 *      - eWNI_SME_SWITCH_CHL_REQ
 *      - eWNI_SME_SWITCH_CHL_CB_PRIMARY_REQ
 *      - eWNI_SME_SWITCH_CHL_CB_SECONDARY_REQ
 *
 *ASSUMPTIONS:
 *
 *  eWNI_SME_SWITCH_CHL_REQ is issued only when 11h is enabled,
 *  and WSM wishes to switch its primary channel. AP shall
 *  populate the 802.11h channel switch IE in its Beacons/Probe Rsp.
 *
 *  eWNI_SME_SWITCH_CHL_CB_PRIMARY_REQ is issued only when 11h is enabled,
 *  and WSM wishes to switch both its primary channel and secondary channel.
 *  (In the case of if 11h is disabled, and WSM wants to change both
 *  primary & secondary channel, then WSM should issue a restart-BSS). AP
 *  shall populate the 802.11h channel switch IE in its Beacons/Probe Rsp.
 *
 *  eWNI_SME_SWITCH_CHL_CB_SECONDARY_REQ is issued when WSM wishes to
 *  switch/disable only its secondary channel. This can occur when 11h
 *  is enabled or disabled. AP shall populate the airgo proprietary
 *  channel switch IE in its Beacons/Probe Rsp.
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limHandleSmeSwitchChlRequest(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tpSirSmeSwitchChannelReq   pSmeMsg;
    eHalStatus                 status;

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_SWITCH_CHL_REQ_EVENT, NULL, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    
    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
           return;
    }

    if (pMac->lim.gLimSmeState != eLIM_SME_NORMAL_STATE ||
            pMac->lim.gLimSystemRole != eLIM_AP_ROLE ||
            pMac->lim.gLimSpecMgmt.dot11hChanSwState == eLIM_11H_CHANSW_RUNNING)
    {
        PELOGE(limLog(pMac, LOGE, "Rcvd Switch Chl Req in wrong state\n");)
        limSendSmeRsp(pMac, eWNI_SME_SWITCH_CHL_RSP, eSIR_SME_CHANNEL_SWITCH_FAIL);
        return;
    }
                
    status = palAllocateMemory( pMac->hHdd, (void **)&pSmeMsg, sizeof(tSirSmeSwitchChannelReq));
    if( eHAL_STATUS_SUCCESS != status)
    {
        PELOGE(limLog(pMac, LOGE, FL("palAllocateMemory failed, status = %d\n"), status);)
        return;
    }

    if (!limIsSmeSwitchChannelReqValid(pMac, (tANI_U8 *)pMsgBuf, pSmeMsg))
    {
        limLog(pMac, LOGE,
            FL("invalid sme message received\n"));
        palFreeMemory( pMac->hHdd, pSmeMsg);
        limSendSmeRsp(pMac, eWNI_SME_SWITCH_CHL_RSP, eSIR_SME_INVALID_PARAMETERS);
        return;
    }


    /* If we're already doing channel switching and we're in the
     * middle of counting down, then reject this channel switch msg.
     */
    if (pMac->lim.gLimChannelSwitch.state != eLIM_CHANNEL_SWITCH_IDLE)
    {
        limLog(pMac, LOGE,
            FL("channel switching is already in progress.\n"));
        palFreeMemory( pMac->hHdd, pSmeMsg);
        limSendSmeRsp(pMac, eWNI_SME_SWITCH_CHL_RSP, eSIR_SME_CHANNEL_SWITCH_DISABLED);
        return;
    }

    PELOG1(limLog(pMac, LOG1, FL("rcvd eWNI_SME_SWITCH_CHL_REQ, message type = %d\n"), pSmeMsg->messageType);)
    switch(pSmeMsg->messageType)
    {
        case eWNI_SME_SWITCH_CHL_REQ:
            pMac->lim.gLimChannelSwitch.state = eLIM_CHANNEL_SWITCH_PRIMARY_ONLY;
            break;

        case eWNI_SME_SWITCH_CHL_CB_PRIMARY_REQ:

            pMac->lim.gLimChannelSwitch.state = eLIM_CHANNEL_SWITCH_PRIMARY_AND_SECONDARY;
            break;

        case eWNI_SME_SWITCH_CHL_CB_SECONDARY_REQ:
            pMac->lim.gLimChannelSwitch.state = eLIM_CHANNEL_SWITCH_SECONDARY_ONLY;
            break;

        default:
            PELOGE(limLog(pMac, LOGE, FL("unknown message\n"));)
            palFreeMemory( pMac->hHdd, pSmeMsg);
            limSendSmeRsp(pMac, eWNI_SME_SWITCH_CHL_RSP, eSIR_SME_INVALID_PARAMETERS);
            return;
    }

    pMac->lim.gLimChannelSwitch.primaryChannel = pSmeMsg->channelId;
    pMac->lim.gLimChannelSwitch.secondarySubBand = pSmeMsg->cbMode;
    pMac->lim.gLimChannelSwitch.switchCount = computeChannelSwitchCount(pMac, pSmeMsg->dtimFactor);
    if (LIM_IS_RADAR_DETECTED(pMac))
    {
        /** Measurement timers not running */
        pMac->lim.gLimChannelSwitch.switchMode = eSIR_CHANSW_MODE_SILENT;
    }
    else
    {
        /** Stop measurement timers till channel switch */
        limStopMeasTimers(pMac);
        pMac->lim.gLimChannelSwitch.switchMode = eSIR_CHANSW_MODE_NORMAL;
    }

    PELOG1(limLog(pMac, LOG1, FL("state %d, primary %d, subband %d, count %d \n"),
           pMac->lim.gLimChannelSwitch.state,
           pMac->lim.gLimChannelSwitch.primaryChannel,
           pMac->lim.gLimChannelSwitch.secondarySubBand,
           pMac->lim.gLimChannelSwitch.switchCount);)
    palFreeMemory( pMac->hHdd, pSmeMsg);
    
    pMac->lim.gLimSpecMgmt.quietState = eLIM_QUIET_END;
    pMac->lim.gLimSpecMgmt.dot11hChanSwState = eLIM_11H_CHANSW_RUNNING;
    
    return;
} /*** end __limHandleSmeSwitchChlRequest() ***/


/**--------------------------------------------------------------
\fn     __limProcessSmeSwitchChlReq

\brief  Wrapper for the function __limHandleSmeSwitchChlRequest
        This message will be defered until softmac come out of
        scan mode.
\param  pMac
\param  pMsg

\return TRUE - If we consumed the buffer
        FALSE - If have defered the message.
 ---------------------------------------------------------------*/
static tANI_BOOLEAN
__limProcessSmeSwitchChlReq(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    if (__limIsDeferedMsgForLearn(pMac, pMsg))
    {
        /**
                * If message defered, buffer is not consumed yet.
                * So return false
                */
        return eANI_BOOLEAN_FALSE;
    }
    __limHandleSmeSwitchChlRequest(pMac, (tANI_U32 *) pMsg->bodyptr);
    return eANI_BOOLEAN_TRUE;
}
#endif


void limProcessSmeGetScanChannelInfo(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tSirMsgQ         mmhMsg;
    tpSmeGetScanChnRsp  pSirSmeRsp;
    tANI_U16 len = 0;

    if(pMac->lim.scanChnInfo.numChnInfo > SIR_MAX_SUPPORTED_CHANNEL_LIST)
    {
        limLog(pMac, LOGW, FL("numChn is out of bounds %d\n"),
                pMac->lim.scanChnInfo.numChnInfo);
        pMac->lim.scanChnInfo.numChnInfo = SIR_MAX_SUPPORTED_CHANNEL_LIST;
    }

    PELOG2(limLog(pMac, LOG2,
           FL("Sending message %s with number of channels %d\n"),
           limMsgStr(eWNI_SME_GET_SCANNED_CHANNEL_RSP), pMac->lim.scanChnInfo.numChnInfo);)

    len = sizeof(tSmeGetScanChnRsp) + (pMac->lim.scanChnInfo.numChnInfo - 1) * sizeof(tLimScanChn);
    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pSirSmeRsp, len ))
    {
        /// Buffer not available. Log error
        limLog(pMac, LOGP,
               FL("call to palAllocateMemory failed for JOIN/REASSOC_RSP\n"));

        return;
    }
    palZeroMemory(pMac->hHdd, pSirSmeRsp, len);

#if defined(ANI_PRODUCT_TYPE_AP) && defined(ANI_LITTLE_BYTE_ENDIAN)
    sirStoreU16N((tANI_U8*)&pSirSmeRsp->mesgType, eWNI_SME_GET_SCANNED_CHANNEL_RSP);
    sirStoreU16N((tANI_U8*)&pSirSmeRsp->mesgLen, len);
#else
    pSirSmeRsp->mesgType = eWNI_SME_GET_SCANNED_CHANNEL_RSP;
    pSirSmeRsp->mesgLen = len;
#endif
    pSirSmeRsp->sessionId = 0;

    if(pMac->lim.scanChnInfo.numChnInfo)
    {
        pSirSmeRsp->numChn = pMac->lim.scanChnInfo.numChnInfo;
        palCopyMemory(pMac->hHdd, pSirSmeRsp->scanChn, pMac->lim.scanChnInfo.scanChn, sizeof(tLimScanChn) * pSirSmeRsp->numChn);
    }
    //Clear the list
    limRessetScanChannelInfo(pMac);

    mmhMsg.type = eWNI_SME_GET_SCANNED_CHANNEL_RSP;
    mmhMsg.bodyptr = pSirSmeRsp;
    mmhMsg.bodyval = 0;
  
    pMac->lim.gLimRspReqd = false;
    MTRACE(macTraceMsgTx(pMac, 0, mmhMsg.type));
    limSysProcessMmhMsgApi(pMac, &mmhMsg,  ePROT);
}


#ifdef WLAN_SOFTAP_FEATURE
void limProcessSmeGetAssocSTAsInfo(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tSirSmeGetAssocSTAsReq  getAssocSTAsReq;
    tpDphHashNode           pStaDs = NULL;
    tpPESession             psessionEntry = NULL;
    tSap_Event              sapEvent;
    tpWLAN_SAPEventCB       pSapEventCallback = NULL;
    tpSap_AssocMacAddr      pAssocStasTemp = NULL;// #include "sapApi.h"
    tANI_U8                 sessionId = CSR_SESSION_ID_INVALID;
    tANI_U8                 assocId = 0;
    tANI_U8                 staCount = 0;

    if (!limIsSmeGetAssocSTAsReqValid(pMac, &getAssocSTAsReq, (tANI_U8 *) pMsgBuf))
    {
        limLog(pMac, LOGE,
                        FL("received invalid eWNI_SME_GET_ASSOC_STAS_REQ message\n"));
        goto limAssocStaEnd;
    }

    switch (getAssocSTAsReq.modId)
    {
/**        
        case VOS_MODULE_ID_HAL:
            wdaPostCtrlMsg( pMac, &msgQ );
            return;

        case VOS_MODULE_ID_TL:
            Post msg TL
            return;
*/
        case VOS_MODULE_ID_PE:
        default:
            break;
    }

    // Get Assoctiated stations from PE
    // Find PE session Entry
    if ((psessionEntry = peFindSessionByBssid(pMac, getAssocSTAsReq.bssId, &sessionId)) == NULL)
    {
        limLog(pMac, LOGE,
                        FL("session does not exist for given bssId\n"));
        goto limAssocStaEnd;
    }

    if (psessionEntry->limSystemRole != eLIM_AP_ROLE)
    {
        limLog(pMac, LOGE,
                        FL("Received unexpected message in state %X, in role %X\n"),
                        psessionEntry->limSmeState , psessionEntry->limSystemRole);
        goto limAssocStaEnd;
    }

    // Retrieve values obtained in the request message
    pSapEventCallback   = (tpWLAN_SAPEventCB)getAssocSTAsReq.pSapEventCallback;
    pAssocStasTemp      = (tpSap_AssocMacAddr)getAssocSTAsReq.pAssocStasArray;

    for (assocId = 0; assocId < psessionEntry->dph.dphHashTable.size; assocId++)// Softap dphHashTable.size = 8
    {
        pStaDs = dphGetHashEntry(pMac, assocId, &psessionEntry->dph.dphHashTable);

        if (NULL == pStaDs)
            continue;

        if (pStaDs->valid)
        {
            palCopyMemory(pMac->hHdd, (tANI_U8 *)&pAssocStasTemp->staMac,
                                        (tANI_U8 *)&pStaDs->staAddr,
                                        sizeof(v_MACADDR_t));  // Mac address
            pAssocStasTemp->assocId = (v_U8_t)pStaDs->assocId;         // Association Id
            pAssocStasTemp->staId   = (v_U8_t)pStaDs->staIndex;        // Station Id

            limLog(pMac, LOG1, FL("dph Station Number = %d"), staCount+1);
            limLog(pMac, LOG1, FL("MAC = %02x:%02x:%02x:%02x:%02x:%02x"),
                                        pStaDs->staAddr[0],
                                        pStaDs->staAddr[1],
                                        pStaDs->staAddr[2],
                                        pStaDs->staAddr[3],
                                        pStaDs->staAddr[4],
                                        pStaDs->staAddr[5]);
            limLog(pMac, LOG1, FL("Association Id = %d"),pStaDs->assocId);
            limLog(pMac, LOG1, FL("Station Index = %d"),pStaDs->staIndex);

            pAssocStasTemp++;
            staCount++;
        }
    }

limAssocStaEnd:
    // Call hdd callback with sap event to send the list of associated stations from PE
    if (pSapEventCallback != NULL)
    {
        sapEvent.sapHddEventCode = eSAP_ASSOC_STA_CALLBACK_EVENT;
        sapEvent.sapevt.sapAssocStaListEvent.module = VOS_MODULE_ID_PE;
        sapEvent.sapevt.sapAssocStaListEvent.noOfAssocSta = staCount;
        sapEvent.sapevt.sapAssocStaListEvent.pAssocStas = (tpSap_AssocMacAddr)getAssocSTAsReq.pAssocStasArray;
        pSapEventCallback(&sapEvent, getAssocSTAsReq.pUsrContext);
    }
}


/**
 * limProcessSmeGetWPSPBCSessions
 *
 *FUNCTION:
 * This function is called when query the WPS PBC overlap message is received
 *
 *LOGIC:
 * This function parses get WPS PBC overlap information message and call callback to pass  
 * WPS PBC overlap information back to hdd.
 *ASSUMPTIONS:
 *
 *
 *NOTE:
 *
 * @param  pMac     Pointer to Global MAC structure
 * @param  pMsgBuf  A pointer to WPS PBC overlap query message
*
 * @return None
 */
void limProcessSmeGetWPSPBCSessions(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tSirSmeGetWPSPBCSessionsReq  GetWPSPBCSessionsReq;
    tpPESession                  psessionEntry = NULL;
    tSap_Event                   sapEvent;
    tpWLAN_SAPEventCB            pSapEventCallback = NULL;
    tANI_U8                      sessionId = CSR_SESSION_ID_INVALID;
    tSirMacAddr                  zeroMac = {0,0,0,0,0,0};
        
    sapEvent.sapevt.sapGetWPSPBCSessionEvent.status = VOS_STATUS_E_FAULT;
    
    if (limIsSmeGetWPSPBCSessionsReqValid(pMac,  &GetWPSPBCSessionsReq, (tANI_U8 *) pMsgBuf) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGE,
                        FL("received invalid eWNI_SME_GET_ASSOC_STAS_REQ message\n"));
        goto limGetWPSPBCSessionsEnd;
    }

    // Get Assoctiated stations from PE
    // Find PE session Entry
    if ((psessionEntry = peFindSessionByBssid(pMac, GetWPSPBCSessionsReq.bssId, &sessionId)) == NULL)
    {
        limLog(pMac, LOGE,
                        FL("session does not exist for given bssId\n"));
        goto limGetWPSPBCSessionsEnd;
    }

    if (psessionEntry->limSystemRole != eLIM_AP_ROLE)
    {
        limLog(pMac, LOGE,
                        FL("Received unexpected message in role %X\n"),
                        psessionEntry->limSystemRole);
        goto limGetWPSPBCSessionsEnd;
    }

    // Call hdd callback with sap event to send the WPS PBC overlap infromation
    sapEvent.sapHddEventCode =  eSAP_GET_WPSPBC_SESSION_EVENT;
    sapEvent.sapevt.sapGetWPSPBCSessionEvent.module = VOS_MODULE_ID_PE;

    if (palEqualMemory(pMac->hHdd, zeroMac, GetWPSPBCSessionsReq.pRemoveMac, sizeof(tSirMacAddr)))
    { //This is GetWpsSession call

      limGetWPSPBCSessions(pMac,
              sapEvent.sapevt.sapGetWPSPBCSessionEvent.addr.bytes, sapEvent.sapevt.sapGetWPSPBCSessionEvent.UUID_E, 
              &sapEvent.sapevt.sapGetWPSPBCSessionEvent.wpsPBCOverlap, psessionEntry);
    }
    else
    {
      limRemovePBCSessions(pMac, GetWPSPBCSessionsReq.pRemoveMac,psessionEntry);
      /* don't have to inform the HDD/Host */
      return;
    }
    
    PELOG4(limLog(pMac, LOGE, FL("wpsPBCOverlap %d\n"), sapEvent.sapevt.sapGetWPSPBCSessionEvent.wpsPBCOverlap);)
    PELOG4(limPrintMacAddr(pMac, sapEvent.sapevt.sapGetWPSPBCSessionEvent.addr.bytes, LOG4);)
    
    sapEvent.sapevt.sapGetWPSPBCSessionEvent.status = VOS_STATUS_SUCCESS;
  
limGetWPSPBCSessionsEnd:
    pSapEventCallback   = (tpWLAN_SAPEventCB)GetWPSPBCSessionsReq.pSapEventCallback;
    pSapEventCallback(&sapEvent, GetWPSPBCSessionsReq.pUsrContext);
}

#endif


/**
 * __limCounterMeasures()
 *
 * FUNCTION:
 * This function is called to "implement" MIC counter measure
 * and is *temporary* only
 *
 * LOGIC: on AP, disassoc all STA associated thru TKIP,
 * we don't do the proper STA disassoc sequence since the
 * BSS will be stoped anyway
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return None
 */

static void
__limCounterMeasures(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    tSirMacAddr mac = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if ( (psessionEntry->limSystemRole == eLIM_AP_ROLE) || (psessionEntry->limSystemRole == eLIM_BT_AMP_AP_ROLE)
        || (psessionEntry->limSystemRole == eLIM_BT_AMP_STA_ROLE) )
         
        limSendDisassocMgmtFrame(pMac, eSIR_MAC_MIC_FAILURE_REASON, mac, psessionEntry);

    tx_thread_sleep(10);
};


#ifdef WLAN_SOFTAP_FEATURE
void
limProcessTkipCounterMeasures(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tSirSmeTkipCntrMeasReq  tkipCntrMeasReq;
    tpPESession             psessionEntry;
    tANI_U8                 sessionId;      //PE sessionId

    if ( limTkipCntrMeasReqSerDes( pMac, &tkipCntrMeasReq, (tANI_U8 *) pMsgBuf ) != eSIR_SUCCESS )
    {
        limLog(pMac, LOGE,
                        FL("received invalid eWNI_SME_TKIP_CNTR_MEAS_REQ message\n"));
        return;
    }

    if ( NULL == (psessionEntry = peFindSessionByBssid( pMac, tkipCntrMeasReq.bssId, &sessionId )) )
    {
        limLog(pMac, LOGE, FL("session does not exist for given BSSID \n"));
        return;
    }

    if ( tkipCntrMeasReq.bEnable )
    {
        __limCounterMeasures( pMac, psessionEntry );
    }

    psessionEntry->bTkipCntrMeasActive = tkipCntrMeasReq.bEnable;
}
#endif


static void
__limHandleSmeStopBssRequest(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tSirSmeStopBssReq  stopBssReq;
    tSirRetStatus      status;
    tLimSmeStates      prevState;
    tANI_U8            sessionId;  //PE sessionId
    tpPESession        psessionEntry;
    tANI_U8            smesessionId;
    tANI_U16           smetransactionId;
    
    limGetSessionInfo(pMac,(tANI_U8 *)pMsgBuf,&smesessionId,&smetransactionId);
        


    if ((limStopBssReqSerDes(pMac, &stopBssReq, (tANI_U8 *) pMsgBuf) != eSIR_SUCCESS) ||
        !limIsSmeStopBssReqValid(pMsgBuf))
    {
        PELOGW(limLog(pMac, LOGW, FL("received invalid SME_STOP_BSS_REQ message\n"));)
        /// Send Stop BSS response to host
        limSendSmeRsp(pMac, eWNI_SME_STOP_BSS_RSP, eSIR_SME_INVALID_PARAMETERS,smesessionId,smetransactionId);
        return;
    }

 
    if((psessionEntry = peFindSessionByBssid(pMac,stopBssReq.bssId,&sessionId)) == NULL)
    {
        limLog(pMac, LOGW, FL("session does not exist for given BSSID \n"));
        limSendSmeRsp(pMac, eWNI_SME_STOP_BSS_RSP, eSIR_SME_INVALID_PARAMETERS,smesessionId,smetransactionId);
        return;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_STOP_BSS_REQ_EVENT, psessionEntry, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT


    if ((psessionEntry->limSmeState != eLIM_SME_NORMAL_STATE) ||    /* Added For BT -AMP Support */
        (psessionEntry->limSystemRole == eLIM_STA_ROLE ))
    {
        /**
         * Should not have received STOP_BSS_REQ in states
         * other than 'normal' state or on STA in Infrastructure
         * mode. Log error and return response to host.
         */
        limLog(pMac, LOGE,
           FL("received unexpected SME_STOP_BSS_REQ in state %X, for role %d\n"),
           psessionEntry->limSmeState, psessionEntry->limSystemRole);
        limPrintSmeState(pMac, LOGE, psessionEntry->limSmeState);
        /// Send Stop BSS response to host
        limSendSmeRsp(pMac, eWNI_SME_STOP_BSS_RSP, eSIR_SME_UNEXPECTED_REQ_RESULT_CODE,smesessionId,smetransactionId);
        return;
    }

#ifdef WLAN_SOFTAP_FEATURE
    if (psessionEntry->limSystemRole == eLIM_AP_ROLE )
    {
        limWPSPBCClose(pMac, psessionEntry);
    }
#endif
    PELOGW(limLog(pMac, LOGW, FL("RECEIVED STOP_BSS_REQ with reason code=%d\n"), stopBssReq.reasonCode);)

    prevState = psessionEntry->limSmeState;

    psessionEntry->limSmeState = eLIM_SME_IDLE_STATE;
    MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));

    /* Update SME session Id and Transaction Id */
    psessionEntry->smeSessionId = smesessionId;
    psessionEntry->transactionId = smetransactionId;

    /* BTAMP_STA and STA_IN_IBSS should NOT send Disassoc frame */
    if ( (eLIM_STA_IN_IBSS_ROLE != psessionEntry->limSystemRole) && (eLIM_BT_AMP_STA_ROLE != psessionEntry->limSystemRole) )
    {
        tSirMacAddr   bcAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        if ((stopBssReq.reasonCode == eSIR_SME_MIC_COUNTER_MEASURES))
            // Send disassoc all stations associated thru TKIP
            __limCounterMeasures(pMac,psessionEntry);
        else
            limSendDisassocMgmtFrame(pMac, eSIR_MAC_DISASSOC_LEAVING_BSS_REASON, bcAddr,psessionEntry);
    }

    //limDelBss is also called as part of coalescing, when we send DEL BSS followed by Add Bss msg.
    pMac->lim.gLimIbssCoalescingHappened = false;
    
    /* send a delBss to HAL and wait for a response */
    status = limDelBss(pMac, NULL,psessionEntry->bssIdx,psessionEntry);
    
    if (status != eSIR_SUCCESS)
    {
        PELOGE(limLog(pMac, LOGE, FL("delBss failed for bss %d\n"), psessionEntry->bssIdx);)
        psessionEntry->limSmeState= prevState;

        MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));
   
        limSendSmeRsp(pMac, eWNI_SME_STOP_BSS_RSP, eSIR_SME_STOP_BSS_FAILURE,smesessionId,smetransactionId);
    }
}


/**--------------------------------------------------------------
\fn     __limProcessSmeStopBssReq

\brief  Wrapper for the function __limHandleSmeStopBssRequest
        This message will be defered until softmac come out of
        scan mode. Message should be handled even if we have
        detected radar in the current operating channel.
\param  pMac
\param  pMsg

\return TRUE - If we consumed the buffer
        FALSE - If have defered the message.
 ---------------------------------------------------------------*/
static tANI_BOOLEAN
__limProcessSmeStopBssReq(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    if (__limIsDeferedMsgForLearn(pMac, pMsg))
    {
        /**
         * If message defered, buffer is not consumed yet.
         * So return false
         */
        return eANI_BOOLEAN_FALSE;
    }
    __limHandleSmeStopBssRequest(pMac, (tANI_U32 *) pMsg->bodyptr);
    return eANI_BOOLEAN_TRUE;
} /*** end __limProcessSmeStopBssReq() ***/


void limProcessSmeDelBssRsp(
    tpAniSirGlobal  pMac,
    tANI_U32        body,tpPESession psessionEntry)
{

    (void) body;
    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
    //TBD: get the sessionEntry
    dphHashTableClassInit(pMac, &psessionEntry->dph.dphHashTable);
    limDeletePreAuthList(pMac);
    limIbssDelete(pMac,psessionEntry);
    limSendSmeRsp(pMac, eWNI_SME_STOP_BSS_RSP, eSIR_SME_SUCCESS,psessionEntry->smeSessionId,psessionEntry->transactionId);
    return;
}


#if 0
/**
 * __limProcessSmePromiscuousReq()
 *
 *FUNCTION:
 * This function is called to process SME_PROMISCUOUS_REQ message
 * from HDD or upper layer application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmePromiscuousReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{

    tANI_U32                cfg = sizeof(tSirMacAddr);
    tSirMacAddr        currentBssId;
    tLimMlmDisassocReq *pMlmDisassocReq;
    tSirMacAddr        bcAddr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    
    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
           return;
    }

    PELOG1(limLog(pMac, LOG1,
           FL("received PROMISCUOUS_REQ message\n"));)

    if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, currentBssId, &cfg) !=
                                eSIR_SUCCESS)
    {
        /// Could not get BSSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve BSSID\n"));
    }

    if ((((pMac->lim.gLimSystemRole == eLIM_STA_ROLE) ||
         (pMac->lim.gLimSystemRole == eLIM_STA_IN_IBSS_ROLE)) &&
         ((pMac->lim.gLimSmeState == eLIM_SME_ASSOCIATED_STATE) ||
          (pMac->lim.gLimSmeState == eLIM_SME_LINK_EST_STATE))) ||
        ((pMac->lim.gLimSystemRole == eLIM_AP_ROLE) &&
         (pMac->lim.gLimSmeState == eLIM_SME_NORMAL_STATE)))
    {
        // Trigger Disassociation frame to peer MAC entity
        if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void **)&pMlmDisassocReq, sizeof(tLimMlmDisassocReq)))
        {
            // Log error
            limLog(pMac, LOGP,
                   FL("call to palAllocateMemory failed for mlmDisassocReq\n"));

            return;
        }

        if (pMac->lim.gLimSystemRole == eLIM_AP_ROLE)
            palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMlmDisassocReq->peerMacAddr,
                          (tANI_U8 *) &bcAddr,
                          sizeof(tSirMacAddr));
        else
            palCopyMemory( pMac->hHdd, pMlmDisassocReq->peerMacAddr,
                          currentBssId,
                          sizeof(tSirMacAddr));

        pMlmDisassocReq->reasonCode      =
                                eSIR_MAC_DISASSOC_LEAVING_BSS_REASON;
        pMlmDisassocReq->disassocTrigger =
                                      eLIM_PROMISCUOUS_MODE_DISASSOC;

        pMac->lim.gLimPrevSmeState = pMac->lim.gLimSmeState;
        pMac->lim.gLimSmeState     = eLIM_SME_WT_DISASSOC_STATE;
     MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));

        limPostMlmMessage(pMac,
                          LIM_MLM_DISASSOC_REQ,
                          (tANI_U32 *) pMlmDisassocReq);
    }
    else
    {
        // Send Promiscuous mode response to host
        limSendSmePromiscuousModeRsp(pMac);

        pMac->lim.gLimSmeState = eLIM_SME_OFFLINE_STATE;
     MTRACE(macTrace(pMac, TRACE_CODE_SME_STATE, 0, pMac->lim.gLimSmeState));
    }

} /*** end __limProcessSmePromiscuousReq() ***/
#endif


/**---------------------------------------------------------------
\fn     __limProcessSmeAssocCnfNew
\brief  This function handles SME_ASSOC_CNF/SME_REASSOC_CNF 
\       in BTAMP AP.
\
\param  pMac
\param  msgType - message type
\param  pMsgBuf - a pointer to the SME message buffer
\return None
------------------------------------------------------------------*/

  void
__limProcessSmeAssocCnfNew(tpAniSirGlobal pMac, tANI_U32 msgType, tANI_U32 *pMsgBuf)
{
    tSirSmeAssocCnf    assocCnf;
    tpDphHashNode      pStaDs = NULL;
    tpPESession        psessionEntry= NULL;
    tANI_U8            sessionId; 
      

    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE, FL("pMsgBuf is NULL \n"));
        goto end;
    }

    if ((limAssocCnfSerDes(pMac, &assocCnf, (tANI_U8 *) pMsgBuf) == eSIR_FAILURE) ||
        !__limIsSmeAssocCnfValid(&assocCnf))
    {
        limLog(pMac, LOGE, FL("Received invalid SME_RE(ASSOC)_CNF message \n"));
        goto end;
    }

    if((psessionEntry = peFindSessionByBssid(pMac, assocCnf.bssId, &sessionId))== NULL)
    {        
        limLog(pMac, LOGE, FL("session does not exist for given bssId\n"));
        goto end;
    }

    if ( ((psessionEntry->limSystemRole != eLIM_AP_ROLE) && (psessionEntry->limSystemRole != eLIM_BT_AMP_AP_ROLE)) ||
         ((psessionEntry->limSmeState != eLIM_SME_NORMAL_STATE) && (psessionEntry->limSmeState != eLIM_SME_NORMAL_CHANNEL_SCAN_STATE)))
    {
        limLog(pMac, LOGE, FL("Received unexpected message %X in state %X, in role %X\n"),
               msgType, psessionEntry->limSmeState , psessionEntry->limSystemRole);
        goto end;
    }

    pStaDs = dphGetHashEntry(pMac, assocCnf.aid, &psessionEntry->dph.dphHashTable);
    
    if (pStaDs == NULL)
    {        
        limLog(pMac, LOG1,
            FL("Received invalid message %X due to no STA context, for aid %d, peer "),
            msgType, assocCnf.aid);
        limPrintMacAddr(pMac, assocCnf.peerMacAddr, LOG1);     

        /*
        ** send a DISASSOC_IND message to WSM to make sure
        ** the state in WSM and LIM is the same
        **/
       limSendSmeDisassocNtf( pMac, assocCnf.peerMacAddr, eSIR_SME_STA_NOT_ASSOCIATED,
                              eLIM_PEER_ENTITY_DISASSOC, assocCnf.aid,psessionEntry->smeSessionId,psessionEntry->transactionId,psessionEntry);
       goto end;
    }
    if ((pStaDs &&
         (( !palEqualMemory( pMac->hHdd,(tANI_U8 *) pStaDs->staAddr,
                     (tANI_U8 *) assocCnf.peerMacAddr,
                     sizeof(tSirMacAddr)) ) ||
          (pStaDs->mlmStaContext.mlmState != eLIM_MLM_WT_ASSOC_CNF_STATE) ||
          ((pStaDs->mlmStaContext.subType == LIM_ASSOC) &&
           (msgType != eWNI_SME_ASSOC_CNF)) ||
          ((pStaDs->mlmStaContext.subType == LIM_REASSOC) &&
#ifdef WLAN_SOFTAP_FEATURE
           (msgType != eWNI_SME_ASSOC_CNF))))) // since softap is passing this as ASSOC_CNF and subtype differs
#else
           (msgType != eWNI_SME_REASSOC_CNF)))))
#endif
    {
        limLog(pMac, LOG1,
           FL("Received invalid message %X due to peerMacAddr mismatched or not in eLIM_MLM_WT_ASSOC_CNF_STATE state, for aid %d, peer "),
           msgType, assocCnf.aid);
        limPrintMacAddr(pMac, assocCnf.peerMacAddr, LOG1);
        goto end;
    }

    /*
    ** Deactivate/delet CNF_WAIT timer since ASSOC_CNF
    ** has been received
    **/
    limLog(pMac, LOG1, FL("Received SME_ASSOC_CNF. Delete Timer\n"));
    limDeactivateAndChangePerStaIdTimer(pMac, eLIM_CNF_WAIT_TIMER, pStaDs->assocId);

    if (assocCnf.statusCode == eSIR_SME_SUCCESS)
    {        
        /* In BTAMP-AP, PE already finished the WDA_ADD_STA sequence
         * when it had received Assoc Request frame. Now, PE just needs to send 
         * Association Response frame to the requesting BTAMP-STA.
         */
        pStaDs->mlmStaContext.mlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
        limLog(pMac, LOG1, FL("sending Assoc Rsp frame to STA (assoc id=%d) \n"), pStaDs->assocId);
        limSendAssocRspMgmtFrame( pMac, eSIR_SUCCESS, pStaDs->assocId, pStaDs->staAddr, 
                                  pStaDs->mlmStaContext.subType, pStaDs, psessionEntry);
        goto end;      
    } // (assocCnf.statusCode == eSIR_SME_SUCCESS)
    else
    {
        // SME_ASSOC_CNF status is non-success, so STA is not allowed to be associated
        /*Since the HAL sta entry is created for denied STA we need to remove this HAL entry.So to do that set updateContext to 1*/
        if(!pStaDs->mlmStaContext.updateContext)
           pStaDs->mlmStaContext.updateContext = 1;
        limRejectAssociation(pMac, pStaDs->staAddr,
                             pStaDs->mlmStaContext.subType,
                             true, pStaDs->mlmStaContext.authType,
                             pStaDs->assocId, true,
                             eSIR_MAC_UNSPEC_FAILURE_STATUS, psessionEntry);
        return;
    }

end:
    if((psessionEntry != NULL) && (pStaDs != NULL))
    {
        if ( psessionEntry->parsedAssocReq[pStaDs->assocId] != NULL )
        {
            if ( ((tpSirAssocReq)(psessionEntry->parsedAssocReq[pStaDs->assocId]))->assocReqFrame) 
            {
                palFreeMemory(pMac->hHdd,((tpSirAssocReq)(psessionEntry->parsedAssocReq[pStaDs->assocId]))->assocReqFrame);
                ((tpSirAssocReq)(psessionEntry->parsedAssocReq[pStaDs->assocId]))->assocReqFrame = NULL;
            }

            palFreeMemory(pMac->hHdd, psessionEntry->parsedAssocReq[pStaDs->assocId]);  
            psessionEntry->parsedAssocReq[pStaDs->assocId] = NULL;
        }
    }

} /*** end __limProcessSmeAssocCnfNew() ***/


#ifdef ANI_PRODUCT_TYPE_AP
/**
 * __limProcessSmeAssocCnf()
 *
 *FUNCTION:
 * This function is called by limProcessSmeMessages() upon
 * receiving SME_ASSOC_CNF/SME_REASSOC_CNF from WSM.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 *
 * @return None
 */

static void
__limProcessSmeAssocCnf(tpAniSirGlobal pMac, tANI_U32 msgType, tANI_U32 *pMsgBuf)
{
    tSirSmeAssocCnf    assocCnf;
    tpDphHashNode      pStaDs;
    tpPESession        psessionEntry;
    tANI_U8            sessionId; 
    tHalBitVal cbBitVal;
    tANI_U8 tspecIdx = 0; //index in the lim tspec table.

    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
           return;
    }

   if ((limAssocCnfSerDes(pMac, &assocCnf, (tANI_U8 *) pMsgBuf) ==
                            eSIR_FAILURE) ||
        !__limIsSmeAssocCnfValid(&assocCnf))
    {
        limLog(pMac, LOGW,
           FL("Received re/assocCnf message with invalid parameters\n"));

        return;
    }

    if((psessionEntry = peFindSessionByBssid(pMac,assocCnf->bssId,&sessionId))== NULL)
    {
        
        limLog(pMac, LOGE,
                      FL("session does not exist for given bssId\n"));
        return;
    }

#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_ASSOC_CNF_EVENT, psessionEntry, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    
    if ( ((psessionEntry->limSystemRole != eLIM_AP_ROLE) && (psessionEntry->limSystemRole != eLIM_BT_AMP_AP_ROLE)) ||
         ((psessionEntry->limSmeState != eLIM_SME_NORMAL_STATE) && (psessionEntry->limSmeState != eLIM_SME_NORMAL_CHANNEL_SCAN_STATE)))
    {
        limLog(pMac, LOGE,
         FL("Received unexpected message %X in state %X, in role %X\n"),
         msgType, psessionEntry->limSmeState , psessionEntry->limSystemRole);

        return;
    }

    pStaDs = dphGetHashEntry(pMac, assocCnf.aid, &psessionEntry->dph.dphHashTable);


    if (pStaDs == NULL)
    {
        
        PELOG1(limLog(pMac, LOG1,
           FL("Received invalid message %X due to no STA context, for aid %d, peer "),
           msgType, assocCnf.aid);
        limPrintMacAddr(pMac, assocCnf.peerMacAddr, LOG1);)
       
        /*
        ** send a DISASSOC_IND message to WSM to make sure
        ** the state in WSM and LIM is the same
        **/

        limSendSmeDisassocNtf(pMac,
            assocCnf.peerMacAddr,
            eSIR_SME_STA_NOT_ASSOCIATED,
            eLIM_PEER_ENTITY_DISASSOC,
            assocCnf.aid,psessionEntry);

        return;
    }
    if ((pStaDs &&
         (( !palEqualMemory( pMac->hHdd,(tANI_U8 *) pStaDs->staAddr,
                     (tANI_U8 *) assocCnf.peerMacAddr,
                     sizeof(tSirMacAddr)) ) ||
          (pStaDs->mlmStaContext.mlmState !=
                          eLIM_MLM_WT_ASSOC_CNF_STATE) ||
          ((pStaDs->mlmStaContext.subType == LIM_ASSOC) &&
           (msgType != eWNI_SME_ASSOC_CNF)) ||
          ((pStaDs->mlmStaContext.subType == LIM_REASSOC) &&
           (msgType != eWNI_SME_REASSOC_CNF)))))
    {
        PELOG1(limLog(pMac, LOG1,
           FL("Received invalid message %X due to peerMacAddr mismatched or not in eLIM_MLM_WT_ASSOC_CNF_STATE state, for aid %d, peer "),
           msgType, assocCnf.aid);
        limPrintMacAddr(pMac, assocCnf.peerMacAddr, LOG1);)

        return;
    }

    /*
    ** Deactivate/delet CNF_WAIT timer since ASSOC_CNF
    ** has been received
    **/

    PELOG1(limLog(pMac, LOG1, FL("Received Cnf. Delete Timer\n"));)
    limDeactivateAndChangePerStaIdTimer(pMac, eLIM_CNF_WAIT_TIMER,
                                        pStaDs->assocId);

    if (assocCnf.statusCode == eSIR_SME_SUCCESS)
    {
        tSirMacScheduleIE schedule;

        // If STA is a TITAN-compatible device, then setup
        // the TITAN proprietary capabilities appropriately
        // in the per STA DS, as per the local TITAN Prop
        // capabilities of this AP

        // STA is allowed to be associated
        if (pStaDs->mlmStaContext.updateContext)
        {

              tLimMlmStates mlmPrevState = pStaDs->mlmStaContext.mlmState;
              //we need to set the mlmState here in order differentiate in limDelSta.
              pStaDs->mlmStaContext.mlmState = eLIM_MLM_WT_ASSOC_DEL_STA_RSP_STATE;
              if(limDelSta(pMac, pStaDs, true,psessionEntry) != eSIR_SUCCESS)
              {
                  limLog(pMac, LOGE,
                         FL("could not DEL STA with assocId=%d staId %d\n"),
                         pStaDs->assocId, pStaDs->staIndex);

                  limRejectAssociation(pMac,
                           pStaDs->staAddr,
                           pStaDs->mlmStaContext.subType,
                           true, pStaDs->mlmStaContext.authType,
                           pStaDs->assocId, true,
                           (tSirResultCodes) eSIR_MAC_UNSPEC_FAILURE_STATUS,
                           psessionEntry);

                  //Restoring the state back.
                  pStaDs->mlmStaContext.mlmState = mlmPrevState;
                  return;
              }
              return;
        }
        else
        {
            // Add STA context at HW
            if (limAddSta(pMac, pStaDs,psessionEntry) != eSIR_SUCCESS)
            {
                limLog(pMac, LOGE,
                       FL("could not Add STA with assocId=%d\n"),
                       pStaDs->assocId);

              // delete the TS if it has already been added.
               // send the response with error status.
                if(pStaDs->qos.addtsPresent)
                {
                  limAdmitControlDeleteTS(pMac, pStaDs->assocId, &pStaDs->qos.addts.tspec.tsinfo, NULL, &tspecIdx);
                }

                limRejectAssociation(pMac,
                         pStaDs->staAddr,
                         pStaDs->mlmStaContext.subType,
                         true, pStaDs->mlmStaContext.authType,
                         pStaDs->assocId, true,
                         (tSirResultCodes) eSIR_MAC_UNSPEC_FAILURE_STATUS,
                         psessionEntry);

            }
            return;
        }

   } // if (assocCnf.statusCode == eSIR_SME_SUCCESS)
    else
    {
        // STA is not allowed to be associated
        limRejectAssociation(pMac, pStaDs->staAddr,
                             pStaDs->mlmStaContext.subType,
                             true, pStaDs->mlmStaContext.authType,
                             pStaDs->assocId, true,
                             assocCnf.statusCode, psessionEntry);
    }
} /*** end __limProcessSmeAssocCnf() ***/
#endif


static void
__limProcessSmeAddtsReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tpDphHashNode   pStaDs;
    tSirMacAddr     peerMac;
    tpSirAddtsReq   pSirAddts;
    tANI_U32        timeout;
    tpPESession     psessionEntry;
    tANI_U8         sessionId;  //PE sessionId
    tANI_U8         smesessionId;
    tANI_U16        smetransactionId;


    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
           return;
    }

    limGetSessionInfo(pMac,(tANI_U8 *)pMsgBuf,&smesessionId,&smetransactionId);
    
    pSirAddts = (tpSirAddtsReq) pMsgBuf;

    if((psessionEntry = peFindSessionByBssid(pMac, pSirAddts->bssId,&sessionId))== NULL)
    {
        limLog(pMac, LOGE, "Session Does not exist for given bssId\n");
        return;
    }
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_ADDTS_REQ_EVENT, psessionEntry, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT
    


    /* if sta
     *  - verify assoc state
     *  - send addts request to ap
     *  - wait for addts response from ap
     * if ap, just ignore with error log
     */
    PELOG1(limLog(pMac, LOG1,
           FL("Received SME_ADDTS_REQ (TSid %d, UP %d)\n"),
           pSirAddts->req.tspec.tsinfo.traffic.tsid,
           pSirAddts->req.tspec.tsinfo.traffic.userPrio);)

    if ((psessionEntry->limSystemRole != eLIM_STA_ROLE)&&(psessionEntry->limSystemRole != eLIM_BT_AMP_STA_ROLE))
    {
        PELOGE(limLog(pMac, LOGE, "AddTs received on AP - ignoring\n");)
        limSendSmeAddtsRsp(pMac, pSirAddts->rspReqd, eSIR_FAILURE, psessionEntry, pSirAddts->req.tspec, 
                smesessionId,smetransactionId);
        return;
    }

    //Ignore the request if STA is in 11B mode.
    if(psessionEntry->dot11mode == WNI_CFG_DOT11_MODE_11B)
    {
        PELOGE(limLog(pMac, LOGE, "AddTS received while Dot11Mode is 11B - ignoring\n");)
        limSendSmeAddtsRsp(pMac, pSirAddts->rspReqd, eSIR_FAILURE, psessionEntry, pSirAddts->req.tspec, 
                smesessionId,smetransactionId);
        return;
    }


    pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);

    if(pStaDs == NULL)
    {
        PELOGE(limLog(pMac, LOGE, "Cannot find AP context for addts req\n");)
        limSendSmeAddtsRsp(pMac, pSirAddts->rspReqd, eSIR_FAILURE, psessionEntry, pSirAddts->req.tspec,
            smesessionId,smetransactionId);
        return;
    }

    if ((! pStaDs->valid) ||
        (pStaDs->mlmStaContext.mlmState != eLIM_MLM_LINK_ESTABLISHED_STATE))
    {
        PELOGE(limLog(pMac, LOGE, "AddTs received in invalid MLM state\n");)
        limSendSmeAddtsRsp(pMac, pSirAddts->rspReqd, eSIR_FAILURE, psessionEntry, pSirAddts->req.tspec,
            smesessionId,smetransactionId);
        return;
    }

    pSirAddts->req.wsmTspecPresent = 0;
    pSirAddts->req.wmeTspecPresent = 0;
    pSirAddts->req.lleTspecPresent = 0;
    
    if ((pStaDs->wsmEnabled) &&
        (pSirAddts->req.tspec.tsinfo.traffic.accessPolicy != SIR_MAC_ACCESSPOLICY_EDCA))
        pSirAddts->req.wsmTspecPresent = 1;
    else if (pStaDs->wmeEnabled)
        pSirAddts->req.wmeTspecPresent = 1;
    else if (pStaDs->lleEnabled)
        pSirAddts->req.lleTspecPresent = 1;
    else
    {
        PELOGW(limLog(pMac, LOGW, FL("ADDTS_REQ ignore - qos is disabled\n"));)
        limSendSmeAddtsRsp(pMac, pSirAddts->rspReqd, eSIR_FAILURE, psessionEntry, pSirAddts->req.tspec,
            smesessionId,smetransactionId);
        return;
    }

    if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
        (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE))
    {
        limLog(pMac, LOGE, "AddTs received in invalid LIMsme state (%d)\n",
              psessionEntry->limSmeState);
        limSendSmeAddtsRsp(pMac, pSirAddts->rspReqd, eSIR_FAILURE, psessionEntry, pSirAddts->req.tspec,
            smesessionId,smetransactionId);
        return;
    }

    if (pMac->lim.gLimAddtsSent)
    {
        limLog(pMac, LOGE, "Addts (token %d, tsid %d, up %d) is still pending\n",
               pMac->lim.gLimAddtsReq.req.dialogToken,
               pMac->lim.gLimAddtsReq.req.tspec.tsinfo.traffic.tsid,
               pMac->lim.gLimAddtsReq.req.tspec.tsinfo.traffic.userPrio);
        limSendSmeAddtsRsp(pMac, pSirAddts->rspReqd, eSIR_FAILURE, psessionEntry, pSirAddts->req.tspec,
            smesessionId,smetransactionId);
        return;
    }

    #if 0
    val = sizeof(tSirMacAddr);
    if (wlan_cfgGetStr(pMac, WNI_CFG_BSSID, peerMac, &val) != eSIR_SUCCESS)
    {
        /// Could not get BSSID from CFG. Log error.
        limLog(pMac, LOGP, FL("could not retrieve BSSID\n"));
        return;
    }
    #endif
    sirCopyMacAddr(peerMac,psessionEntry->bssId);

    // save the addts request
    pMac->lim.gLimAddtsSent = true;
    palCopyMemory( pMac->hHdd, (tANI_U8 *) &pMac->lim.gLimAddtsReq, (tANI_U8 *) pSirAddts, sizeof(tSirAddtsReq));

    // ship out the message now
    limSendAddtsReqActionFrame(pMac, peerMac, &pSirAddts->req,
            psessionEntry);
    PELOG1(limLog(pMac, LOG1, "Sent ADDTS request\n");)

    // start a timer to wait for the response
    if (pSirAddts->timeout) 
        timeout = pSirAddts->timeout;
    else if (wlan_cfgGetInt(pMac, WNI_CFG_ADDTS_RSP_TIMEOUT, &timeout) != eSIR_SUCCESS)
    {
        limLog(pMac, LOGP, FL("Unable to get Cfg param %d (Addts Rsp Timeout)\n"),
               WNI_CFG_ADDTS_RSP_TIMEOUT);
        return;
    }

    timeout = SYS_MS_TO_TICKS(timeout);
    if (tx_timer_change(&pMac->lim.limTimers.gLimAddtsRspTimer, timeout, 0) != TX_SUCCESS)
    {
        limLog(pMac, LOGP, FL("AddtsRsp timer change failed!\n"));
        return;
    }
    pMac->lim.gLimAddtsRspTimerCount++;
    if (tx_timer_change_context(&pMac->lim.limTimers.gLimAddtsRspTimer,
                                pMac->lim.gLimAddtsRspTimerCount) != TX_SUCCESS)
    {
        limLog(pMac, LOGP, FL("AddtsRsp timer change failed!\n"));
        return;
    }
    MTRACE(macTrace(pMac, TRACE_CODE_TIMER_ACTIVATE, 0, eLIM_ADDTS_RSP_TIMER));
    
    //add the sessionId to the timer object
    pMac->lim.limTimers.gLimAddtsRspTimer.sessionId = sessionId;
    if (tx_timer_activate(&pMac->lim.limTimers.gLimAddtsRspTimer) != TX_SUCCESS)
    {
        limLog(pMac, LOGP, FL("AddtsRsp timer activation failed!\n"));
        return;
    }
    return;
}


static void
__limProcessSmeDeltsReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tSirMacAddr     peerMacAddr;
    tANI_U8         ac;
    tSirMacTSInfo   *pTsinfo;
    tpSirDeltsReq   pDeltsReq = (tpSirDeltsReq) pMsgBuf;
    tpDphHashNode   pStaDs = NULL;
    tpPESession     psessionEntry;
    tANI_U8         sessionId;
    tANI_U32        status = eSIR_SUCCESS;
    tANI_U8         smesessionId;
    tANI_U16        smetransactionId;    

    limGetSessionInfo(pMac,(tANI_U8 *)pMsgBuf,&smesessionId,&smetransactionId);
    
    if((psessionEntry = peFindSessionByBssid(pMac, pDeltsReq->bssId, &sessionId))== NULL)
    {
        limLog(pMac, LOGE, "Session Does not exist for given bssId\n");
        status = eSIR_FAILURE;
        goto end;
    }
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM //FEATURE_WLAN_DIAG_SUPPORT 
    limDiagEventReport(pMac, WLAN_PE_DIAG_DELTS_REQ_EVENT, psessionEntry, 0, 0);
#endif //FEATURE_WLAN_DIAG_SUPPORT


    if (eSIR_SUCCESS != limValidateDeltsReq(pMac, pDeltsReq, peerMacAddr,psessionEntry))
    {
        PELOGE(limLog(pMac, LOGE, FL("limValidateDeltsReq failed\n"));)
        status = eSIR_FAILURE;
        limSendSmeDeltsRsp(pMac, pDeltsReq, eSIR_FAILURE,psessionEntry,smesessionId,smetransactionId);
        return;
    }

    PELOG1(limLog(pMac, LOG1, FL("Sent DELTS request to station with assocId = %d MacAddr = %x:%x:%x:%x:%x:%x\n"),
              pDeltsReq->aid, peerMacAddr[0], peerMacAddr[1], peerMacAddr[2],
              peerMacAddr[3], peerMacAddr[4], peerMacAddr[5]);)

    limSendDeltsReqActionFrame(pMac, peerMacAddr, pDeltsReq->req.wmeTspecPresent, &pDeltsReq->req.tsinfo, &pDeltsReq->req.tspec,
              psessionEntry);

    pTsinfo = pDeltsReq->req.wmeTspecPresent ? &pDeltsReq->req.tspec.tsinfo : &pDeltsReq->req.tsinfo;

    /* We've successfully send DELTS frame to AP. Update the 
     * dynamic UAPSD mask. The AC for this TSPEC to be deleted
     * is no longer trigger enabled or delivery enabled
     */
    limSetTspecUapsdMask(pMac, pTsinfo, CLEAR_UAPSD_MASK);

    /* We're deleting the TSPEC, so this particular AC is no longer
     * admitted.  PE needs to downgrade the EDCA
     * parameters(for the AC for which TS is being deleted) to the
     * next best AC for which ACM is not enabled, and send the
     * updated values to HAL. 
     */ 
    ac = upToAc(pTsinfo->traffic.userPrio);

    if(pTsinfo->traffic.direction == SIR_MAC_DIRECTION_UPLINK)
    {
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &= ~(1 << ac);
    }
    else if(pTsinfo->traffic.direction == SIR_MAC_DIRECTION_DNLINK)
    {
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] &= ~(1 << ac);
    }
    else if(pTsinfo->traffic.direction == SIR_MAC_DIRECTION_BIDIR)
    {
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_UPLINK] &= ~(1 << ac);
      pMac->lim.gAcAdmitMask[SIR_MAC_DIRECTION_DNLINK] &= ~(1 << ac);
    }

    limSetActiveEdcaParams(pMac, psessionEntry->gLimEdcaParams, psessionEntry);

    pStaDs = dphGetHashEntry(pMac, DPH_STA_HASH_INDEX_PEER, &psessionEntry->dph.dphHashTable);
    if (pStaDs != NULL)
    {
        if (pStaDs->aniPeer == eANI_BOOLEAN_TRUE) 
            limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_TRUE);
        else
            limSendEdcaParams(pMac, psessionEntry->gLimEdcaParamsActive, pStaDs->bssId, eANI_BOOLEAN_FALSE);
        status = eSIR_SUCCESS;
    }
    else
    {
        limLog(pMac, LOGE, FL("Self entry missing in Hash Table \n"));
     status = eSIR_FAILURE;
    }     
#ifdef FEATURE_WLAN_CCX
    limDeactivateAndChangeTimer(pMac,eLIM_TSM_TIMER);
#endif

    // send an sme response back
    end:
         limSendSmeDeltsRsp(pMac, pDeltsReq, eSIR_SUCCESS,psessionEntry,smesessionId,smetransactionId);
}


void
limProcessSmeAddtsRspTimeout(tpAniSirGlobal pMac, tANI_U32 param)
{
    //fetch the sessionEntry based on the sessionId
    tpPESession psessionEntry;
    if((psessionEntry = peFindSessionBySessionId(pMac, pMac->lim.limTimers.gLimAddtsRspTimer.sessionId))== NULL) 
    {
        limLog(pMac, LOGP,FL("Session Does not exist for given sessionID\n"));
        return;
    }

    if (  (psessionEntry->limSystemRole != eLIM_STA_ROLE) && (psessionEntry->limSystemRole != eLIM_BT_AMP_STA_ROLE)   )
    {
        limLog(pMac, LOGW, "AddtsRspTimeout in non-Sta role (%d)\n", psessionEntry->limSystemRole);
        pMac->lim.gLimAddtsSent = false;
        return;
    }

    if (! pMac->lim.gLimAddtsSent)
    {
        PELOGW(limLog(pMac, LOGW, "AddtsRspTimeout but no AddtsSent\n");)
        return;
    }

    if (param != pMac->lim.gLimAddtsRspTimerCount)
    {
        limLog(pMac, LOGE, FL("Invalid AddtsRsp Timer count %d (exp %d)\n"),
               param, pMac->lim.gLimAddtsRspTimerCount);
        return;
    }

    // this a real response timeout
    pMac->lim.gLimAddtsSent = false;
    pMac->lim.gLimAddtsRspTimerCount++;

    limSendSmeAddtsRsp(pMac, true, eSIR_SME_ADDTS_RSP_TIMEOUT, psessionEntry, pMac->lim.gLimAddtsReq.req.tspec,
            psessionEntry->smeSessionId, psessionEntry->transactionId);
}


/**
 * __limProcessSmeStatsRequest()
 *
 *FUNCTION:
 * 
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */
static void
__limProcessSmeStatsRequest(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tpAniGetStatsReq    pStatsReq;
    tSirMsgQ msgQ;
    tpPESession psessionEntry;
    tANI_U8     sessionId;

    
    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
           return;
    }
    
    pStatsReq = (tpAniGetStatsReq) pMsgBuf;
    
    if((psessionEntry = peFindSessionByBssid(pMac,pStatsReq->bssId,&sessionId))== NULL)
    {
        limLog(pMac, LOGE, FL("session does not exist for given bssId\n"));
        return;
    }

       
    
    switch(pStatsReq->msgType)
    {
        //Add Lim stats here. and send reqsponse.

        //HAL maintained Stats.
        case eWNI_SME_STA_STAT_REQ:
            msgQ.type = WDA_STA_STAT_REQ;
            break;
        case eWNI_SME_AGGR_STAT_REQ:
            msgQ.type = WDA_AGGR_STAT_REQ;
            break;
        case eWNI_SME_GLOBAL_STAT_REQ:
            msgQ.type = WDA_GLOBAL_STAT_REQ;
            break;
        case eWNI_SME_STAT_SUMM_REQ:
            msgQ.type = WDA_STAT_SUMM_REQ;
            break;   
        default: //Unknown request.
            PELOGE(limLog(pMac, LOGE, "Unknown Statistics request\n");)
            palFreeMemory( pMac, pMsgBuf );
            return;
    }

    if ( !pMac->lim.gLimRspReqd ) 
    {
        palFreeMemory( pMac, pMsgBuf );
        return;
    }
    else
    {
        pMac->lim.gLimRspReqd = FALSE;
    }

    msgQ.reserved = 0;
    msgQ.bodyptr = pMsgBuf;
    msgQ.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, 0, msgQ.type));

    if( eSIR_SUCCESS != (wdaPostCtrlMsg( pMac, &msgQ ))){
        limLog(pMac, LOGP, "Unable to forward request\n");
        return;
    }

    return;
}


/**
 * __limProcessSmeGetStatisticsRequest()
 *
 *FUNCTION:
 * 
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */
static void
__limProcessSmeGetStatisticsRequest(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tpAniGetPEStatsReq    pPEStatsReq;
    tSirMsgQ msgQ;

    pPEStatsReq = (tpAniGetPEStatsReq) pMsgBuf;
    
    //pPEStatsReq->msgType should be eWNI_SME_GET_STATISTICS_REQ

    msgQ.type = WDA_GET_STATISTICS_REQ;    

    if ( !pMac->lim.gLimRspReqd ) 
    {
        palFreeMemory( pMac, pMsgBuf );
        return;
    }
    else
    {
        pMac->lim.gLimRspReqd = FALSE;
    }

    msgQ.reserved = 0;
    msgQ.bodyptr = pMsgBuf;
    msgQ.bodyval = 0;
    MTRACE(macTraceMsgTx(pMac, 0, msgQ.type));

    if( eSIR_SUCCESS != (wdaPostCtrlMsg( pMac, &msgQ ))){
        palFreeMemory( pMac, pMsgBuf );
        limLog(pMac, LOGP, "Unable to forward request\n");
        return;
    }

    return;
}


#ifdef WLAN_SOFTAP_FEATURE
static void
__limProcessSmeUpdateAPWPSIEs(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tpSirUpdateAPWPSIEsReq  pUpdateAPWPSIEsReq;
    tpPESession             psessionEntry;
    tANI_U8                 sessionId;  //PE sessionID

    PELOG1(limLog(pMac, LOG1,
           FL("received UPDATE_APWPSIEs_REQ message\n")););
    
    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
        return;
    }
    
    if( palAllocateMemory( pMac->hHdd, (void **)&pUpdateAPWPSIEsReq, sizeof(tSirUpdateAPWPSIEsReq)))
    {
        limLog(pMac, LOGP, FL("call to palAllocateMemory failed for pUpdateAPWPSIEsReq\n"));
        return;
    }

    if ((limUpdateAPWPSIEsReqSerDes(pMac, pUpdateAPWPSIEsReq, (tANI_U8 *) pMsgBuf) == eSIR_FAILURE)) 
    {
        limLog(pMac, LOGW, FL("received invalid SME_SETCONTEXT_REQ message\n")); 
        goto end;
    }
    
    if((psessionEntry = peFindSessionByBssid(pMac, pUpdateAPWPSIEsReq->bssId, &sessionId)) == NULL)
    {
        limLog(pMac, LOGW, FL("Session does not exist for given BSSID\n"));
        goto end;
    }

    palCopyMemory(pMac->hHdd, &psessionEntry->APWPSIEs, &pUpdateAPWPSIEsReq->APWPSIEs, sizeof(tSirAPWPSIEs));
    
    schSetFixedBeaconFields(pMac, psessionEntry);
    limSendBeaconInd(pMac, psessionEntry); 

end:
    palFreeMemory( pMac->hHdd, pUpdateAPWPSIEsReq);
    return;
} /*** end __limProcessSmeUpdateAPWPSIEs(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf) ***/

static void
__limProcessSmeHideSSID(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tpSirUpdateParams       pUpdateParams;
    tpPESession             psessionEntry;

    PELOG1(limLog(pMac, LOG1,
           FL("received HIDE_SSID message\n")););
    
    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
        return;
    }

    pUpdateParams = (tpSirUpdateParams)pMsgBuf;
    
    if((psessionEntry = peFindSessionBySessionId(pMac, pUpdateParams->sessionId)) == NULL)
    {
        limLog(pMac, LOGW, "Session does not exist for given sessionId %d\n",
                      pUpdateParams->sessionId);
        return;
    }

    /* Update the session entry */
    psessionEntry->ssidHidden = pUpdateParams->ssidHidden;
   
    /* Update beacon */
    schSetFixedBeaconFields(pMac, psessionEntry);
    limSendBeaconInd(pMac, psessionEntry); 

    return;
} /*** end __limProcessSmeHideSSID(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf) ***/

static void
__limProcessSmeSetWPARSNIEs(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tpSirUpdateAPWPARSNIEsReq  pUpdateAPWPARSNIEsReq;
    tpPESession             psessionEntry;
    tANI_U8                 sessionId;  //PE sessionID

    if(pMsgBuf == NULL)
    {
        limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
        return;
    }
    
    if( palAllocateMemory( pMac->hHdd, (void **)&pUpdateAPWPARSNIEsReq, sizeof(tSirUpdateAPWPSIEsReq)))
    {
        limLog(pMac, LOGP, FL("call to palAllocateMemory failed for pUpdateAPWPARSNIEsReq\n"));
        return;
    }

    if ((limUpdateAPWPARSNIEsReqSerDes(pMac, pUpdateAPWPARSNIEsReq, (tANI_U8 *) pMsgBuf) == eSIR_FAILURE)) 
    {
        limLog(pMac, LOGW, FL("received invalid SME_SETCONTEXT_REQ message\n")); 
        goto end;
    }
    
    if((psessionEntry = peFindSessionByBssid(pMac, pUpdateAPWPARSNIEsReq->bssId, &sessionId)) == NULL)
    {
        limLog(pMac, LOGW, FL("Session does not exist for given BSSID\n"));
        goto end;
    }

    palCopyMemory(pMac->hHdd, &psessionEntry->pLimStartBssReq->rsnIE, &pUpdateAPWPARSNIEsReq->APWPARSNIEs, sizeof(tSirRSNie));
    
    limSetRSNieWPAiefromSmeStartBSSReqMessage(pMac, &psessionEntry->pLimStartBssReq->rsnIE, psessionEntry);
    
    psessionEntry->pLimStartBssReq->privacy = 1;
    psessionEntry->privacy = 1;
    
    schSetFixedBeaconFields(pMac, psessionEntry);
    limSendBeaconInd(pMac, psessionEntry); 

end:
    palFreeMemory( pMac->hHdd, pUpdateAPWPARSNIEsReq);
    return;
} /*** end __limProcessSmeSetWPARSNIEs(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf) ***/

#endif


/** -------------------------------------------------------------
\fn limProcessSmeDelBaPeerInd
\brief handles indication message from HDD to send delete BA request
\param   tpAniSirGlobal pMac
\param   tANI_U32 pMsgBuf
\return None
-------------------------------------------------------------*/
void
limProcessSmeDelBaPeerInd(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    tANI_U16            assocId =0;
    tpSmeDelBAPeerInd   pSmeDelBAPeerInd = (tpSmeDelBAPeerInd)pMsgBuf;
    tpDphHashNode       pSta;
    tpPESession         psessionEntry;
    tANI_U8             sessionId;
    


    if(NULL == pSmeDelBAPeerInd)
        return;

    if ((psessionEntry = peFindSessionByBssid(pMac,pSmeDelBAPeerInd->bssId,&sessionId))==NULL)
    {
        limLog(pMac, LOGE,FL("session does not exist for given bssId\n"));
        return;
    }
    limLog(pMac, LOGW, FL("called with staId = %d, tid = %d, baDirection = %d\n"), 
              pSmeDelBAPeerInd->staIdx, pSmeDelBAPeerInd->baTID, pSmeDelBAPeerInd->baDirection);

    pSta = dphLookupAssocId(pMac, pSmeDelBAPeerInd->staIdx, &assocId, &psessionEntry->dph.dphHashTable);    
    if( eSIR_SUCCESS != limPostMlmDelBAReq( pMac,
          pSta,
          pSmeDelBAPeerInd->baDirection,
          pSmeDelBAPeerInd->baTID,
          eSIR_MAC_UNSPEC_FAILURE_REASON,psessionEntry))
    {
      limLog( pMac, LOGW,
          FL( "Failed to post LIM_MLM_DELBA_REQ to " ));
      if (pSta)
          limPrintMacAddr(pMac, pSta->staAddr, LOGW); 
    }
}

// --------------------------------------------------------------------
/**
 * __limProcessReportMessage
 *
 * FUNCTION:  Processes the next received Radio Resource Management message
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @return None
 */

void __limProcessReportMessage(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
#ifdef WLAN_FEATURE_VOWIFI
   switch (pMsg->type)
   {
      case eWNI_SME_NEIGHBOR_REPORT_REQ_IND:
         rrmProcessNeighborReportReq( pMac, pMsg->bodyptr );
         break;
      case eWNI_SME_BEACON_REPORT_RESP_XMIT_IND:
        {
#if defined FEATURE_WLAN_CCX
         tpSirBeaconReportXmitInd pBcnReport=NULL;
         tpPESession psessionEntry=NULL;
         tANI_U8 sessionId;

         if(pMsg->bodyptr == NULL)
         {
            limLog(pMac, LOGE,FL("Buffer is Pointing to NULL\n"));
            return;
         }
         pBcnReport = (tpSirBeaconReportXmitInd )pMsg->bodyptr;
         if((psessionEntry = peFindSessionByBssid(pMac, pBcnReport->bssId,&sessionId))== NULL)
         {
            limLog(pMac, LOGE, "Session Does not exist for given bssId\n");
            return;
         }
         if (psessionEntry->isCCXconnection)
             ccxProcessBeaconReportXmit( pMac, pMsg->bodyptr);
         else
#endif
             rrmProcessBeaconReportXmit( pMac, pMsg->bodyptr );
        }
        break;
   }
#endif
}

#if defined(FEATURE_WLAN_CCX) || defined(WLAN_FEATURE_VOWIFI)
// --------------------------------------------------------------------
/**
 * limSendSetMaxTxPowerReq
 *
 * FUNCTION:  Send SIR_HAL_SET_MAX_TX_POWER_REQ message to change the max tx power.
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param txPower txPower to be set.
 * @param pSessionEntry session entry.
 * @return None
 */
tSirRetStatus
limSendSetMaxTxPowerReq ( tpAniSirGlobal pMac, tPowerdBm txPower, tpPESession pSessionEntry )
{
   tpMaxTxPowerParams pMaxTxParams = NULL;
   tSirRetStatus  retCode = eSIR_SUCCESS;
   tSirMsgQ       msgQ;

   if( pSessionEntry == NULL )
   {
      PELOGE(limLog(pMac, LOGE, "%s:%d: Inavalid parameters\n", __func__, __LINE__ );)
      return eSIR_FAILURE;
   }
   if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
            (void **) &pMaxTxParams, sizeof(tMaxTxPowerParams) ) ) 
   {
      limLog( pMac, LOGP, "%s:%d:Unable to allocate memory for pMaxTxParams \n", __func__, __LINE__);
      return eSIR_MEM_ALLOC_FAILED;

   }
#if defined(WLAN_VOWIFI_DEBUG) || defined(FEATURE_WLAN_CCX)
   PELOG1(limLog( pMac, LOG1, "%s:%d: Allocated memory for pMaxTxParams...will be freed in other module\n", __func__, __LINE__ );)
#endif
   pMaxTxParams->power = txPower;
   palCopyMemory( pMac->hHdd, pMaxTxParams->bssId, pSessionEntry->bssId, sizeof(tSirMacAddr) );
   palCopyMemory( pMac->hHdd, pMaxTxParams->selfStaMacAddr, pSessionEntry->selfMacAddr, sizeof(tSirMacAddr) );

    msgQ.type = WDA_SET_MAX_TX_POWER_REQ;
    msgQ.bodyptr = pMaxTxParams;
    msgQ.bodyval = 0;
    PELOGW(limLog(pMac, LOG1, FL("Posting WDA_SET_MAX_TX_POWER_REQ to WDA\n"));)
    if(eSIR_SUCCESS != (retCode = wdaPostCtrlMsg(pMac, &msgQ)))
    {
       PELOGW(limLog(pMac, LOGW, FL("wdaPostCtrlMsg() failed\n"));)
       if (NULL != pMaxTxParams)
       {
          palFreeMemory(pMac->hHdd, (tANI_U8*)pMaxTxParams);
       }
       return retCode;
    }
    return retCode;
}
#endif

/**
 * __limProcessSmeAddStaSelfReq()
 *
 *FUNCTION:
 * This function is called to process SME_ADD_STA_SELF_REQ message
 * from SME. It sends a SIR_HAL_ADD_STA_SELF_REQ message to HAL.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeAddStaSelfReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
   tSirMsgQ msg;
   tpAddStaSelfParams pAddStaSelfParams;
   tpSirSmeAddStaSelfReq pSmeReq = (tpSirSmeAddStaSelfReq) pMsgBuf;

   if ( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void**) &pAddStaSelfParams,
            sizeof( tAddStaSelfParams) ) )
   {
      limLog( pMac, LOGP, FL("Unable to allocate memory for tAddSelfStaParams") );
      return;
   }

   palCopyMemory( pMac->hHdd, pAddStaSelfParams->selfMacAddr, pSmeReq->selfMacAddr, sizeof(tSirMacAddr) ); 

   msg.type = SIR_HAL_ADD_STA_SELF_REQ;
   msg.reserved = 0;
   msg.bodyptr =  pAddStaSelfParams;
   msg.bodyval = 0;

   PELOGW(limLog(pMac, LOG1, FL("sending SIR_HAL_ADD_STA_SELF_REQ msg to HAL\n"));)
      MTRACE(macTraceMsgTx(pMac, 0, msg.type));

   if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
   {
      limLog(pMac, LOGP, FL("wdaPostCtrlMsg failed\n"));
   }
   return;
} /*** end __limProcessAddStaSelfReq() ***/


/**
 * __limProcessSmeDelStaSelfReq()
 *
 *FUNCTION:
 * This function is called to process SME_DEL_STA_SELF_REQ message
 * from SME. It sends a SIR_HAL_DEL_STA_SELF_REQ message to HAL.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */

static void
__limProcessSmeDelStaSelfReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
   tSirMsgQ msg;
   tpDelStaSelfParams pDelStaSelfParams;
   tpSirSmeDelStaSelfReq pSmeReq = (tpSirSmeDelStaSelfReq) pMsgBuf;

   if ( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd, (void**) &pDelStaSelfParams,
            sizeof( tDelStaSelfParams) ) )
   {
      limLog( pMac, LOGP, FL("Unable to allocate memory for tDelStaSelfParams") );
      return;
   }

   palCopyMemory( pMac->hHdd, pDelStaSelfParams->selfMacAddr, pSmeReq->selfMacAddr, sizeof(tSirMacAddr) ); 

   msg.type = SIR_HAL_DEL_STA_SELF_REQ;
   msg.reserved = 0;
   msg.bodyptr =  pDelStaSelfParams;
   msg.bodyval = 0;

   PELOGW(limLog(pMac, LOG1, FL("sending SIR_HAL_ADD_STA_SELF_REQ msg to HAL\n"));)
      MTRACE(macTraceMsgTx(pMac, 0, msg.type));

   if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
   {
      limLog(pMac, LOGP, FL("wdaPostCtrlMsg failed\n"));
   }
   return;
} /*** end __limProcessSmeDelStaSelfReq() ***/


#ifdef WLAN_FEATURE_P2P
/**
 * __limProcessSmeRegisterMgmtFrameReq()
 *
 *FUNCTION:
 * This function is called to process eWNI_SME_REGISTER_MGMT_FRAME_REQ message
 * from SME. It Register this information within PE.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */
static void
__limProcessSmeRegisterMgmtFrameReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf)
{
    VOS_STATUS vosStatus;
    tpSirRegisterMgmtFrame pSmeReq = (tpSirRegisterMgmtFrame)pMsgBuf;
    tpLimMgmtFrameRegistration pLimMgmtRegistration = NULL, pNext = NULL;
    tANI_BOOLEAN match = VOS_FALSE;
    PELOG1(limLog(pMac, LOG1, 
           FL("%s: registerFrame %d, frameType %d, matchLen %d\n", 
           __func__, pSmeReq->registerFrame, pSmeReq->frameType, 
       pSmeReq->matchLen)));

    /* First check whether entry exists already*/

    vos_list_peek_front(&pMac->lim.gLimMgmtFrameRegistratinQueue,
            (vos_list_node_t**)&pLimMgmtRegistration);

    while(pLimMgmtRegistration != NULL)
    {
        if (pLimMgmtRegistration->frameType == pSmeReq->frameType)
        {
            if(pSmeReq->matchLen)
            {
                if (pLimMgmtRegistration->matchLen == pSmeReq->matchLen)
                {
                    if (palEqualMemory(pMac, pLimMgmtRegistration->matchData, 
                                pSmeReq->matchData, pLimMgmtRegistration->matchLen))
                    {
                        /* found match! */   
                        match = VOS_TRUE;
                        break;
                    }
                }
            }
            else
            {
                /* found match! */   
                match = VOS_TRUE;
                break;
            }
        }
        vosStatus = vos_list_peek_next (
                &pMac->lim.gLimMgmtFrameRegistratinQueue,
                (vos_list_node_t*) pLimMgmtRegistration,
                (vos_list_node_t**) &pNext );

        pLimMgmtRegistration = pNext;
        pNext = NULL; 

    }

    if (match)
    {
        vos_list_remove_node(&pMac->lim.gLimMgmtFrameRegistratinQueue,
                (vos_list_node_t*)pLimMgmtRegistration);
        palFreeMemory(pMac,pLimMgmtRegistration);
    }

    if(pSmeReq->registerFrame)
    {
        palAllocateMemory(pMac, (void**)&pLimMgmtRegistration,
                        sizeof(tLimMgmtFrameRegistration) + pSmeReq->matchLen);
        if(pLimMgmtRegistration != NULL)
        {
            palZeroMemory(pMac, (void*)pLimMgmtRegistration, 
              sizeof(tLimMgmtFrameRegistration) + pSmeReq->matchLen );
            pLimMgmtRegistration->frameType = pSmeReq->frameType;
            pLimMgmtRegistration->matchLen  = pSmeReq->matchLen;
            pLimMgmtRegistration->sessionId = pSmeReq->sessionId;
            if(pSmeReq->matchLen)
            {
                palCopyMemory(pMac,pLimMgmtRegistration->matchData, 
                              pSmeReq->matchData, pSmeReq->matchLen);
            }
     
            vos_list_insert_front(&pMac->lim.gLimMgmtFrameRegistratinQueue,
                              &pLimMgmtRegistration->node);
        }
    }

    return;
} /*** end __limProcessSmeRegisterMgmtFrameReq() ***/
#endif


/**
 * limProcessSmeReqMessages()
 *
 *FUNCTION:
 * This function is called by limProcessMessageQueue(). This
 * function processes SME request messages from HDD or upper layer
 * application.
 *
 *LOGIC:
 *
 *ASSUMPTIONS:
 *
 *NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  msgType   Indicates the SME message type
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return Boolean - TRUE - if pMsgBuf is consumed and can be freed.
 *                   FALSE - if pMsgBuf is not to be freed.
 */

tANI_BOOLEAN
limProcessSmeReqMessages(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
    tANI_BOOLEAN bufConsumed = TRUE; //Set this flag to false within case block of any following message, that doesnt want pMsgBuf to be freed.
    tANI_U32 *pMsgBuf = pMsg->bodyptr;

    PELOG1(limLog(pMac, LOG1, FL("LIM Received SME Message %s(%d) LimSmeState:%s(%d) LimMlmState: %s(%d)\n"),
         limMsgStr(pMsg->type), pMsg->type,
         limSmeStateStr(pMac->lim.gLimSmeState), pMac->lim.gLimSmeState,
         limMlmStateStr(pMac->lim.gLimMlmState), pMac->lim.gLimMlmState );)

    switch (pMsg->type)
    {
        case eWNI_SME_START_REQ:
            __limProcessSmeStartReq(pMac, pMsgBuf);
            break;

        case eWNI_SME_SYS_READY_IND:
            bufConsumed = __limProcessSmeSysReadyInd(pMac, pMsgBuf);
            break;


        case eWNI_SME_START_BSS_REQ:
            bufConsumed = __limProcessSmeStartBssReq(pMac, pMsg);
            break;

        case eWNI_SME_SCAN_REQ:
            __limProcessSmeScanReq(pMac, pMsgBuf);

            break;

#ifdef WLAN_FEATURE_P2P
        case eWNI_SME_REMAIN_ON_CHANNEL_REQ:
            bufConsumed = limProcessRemainOnChnlReq(pMac, pMsgBuf);
            break;

        case eWNI_SME_UPDATE_NOA:
            __limProcessSmeNoAUpdate(pMac, pMsgBuf);
            break;
#endif
        case eWNI_SME_JOIN_REQ:
            __limProcessSmeJoinReq(pMac, pMsgBuf);

            break;

        case eWNI_SME_AUTH_REQ:
           // __limProcessSmeAuthReq(pMac, pMsgBuf);

            break;

        case eWNI_SME_REASSOC_REQ:
            __limProcessSmeReassocReq(pMac, pMsgBuf);

            break;

        case eWNI_SME_PROMISCUOUS_MODE_REQ:
            //__limProcessSmePromiscuousReq(pMac, pMsgBuf);

            break;

        case eWNI_SME_DISASSOC_REQ:
            __limProcessSmeDisassocReq(pMac, pMsgBuf);

            break;

        case eWNI_SME_DISASSOC_CNF:
        case eWNI_SME_DEAUTH_CNF:
            __limProcessSmeDisassocCnf(pMac, pMsgBuf);

            break;

        case eWNI_SME_DEAUTH_REQ:
            __limProcessSmeDeauthReq(pMac, pMsgBuf);

            break;

#if (WNI_POLARIS_FW_PACKAGE == ADVANCED) && defined(ANI_PRODUCT_TYPE_AP)
        case eWNI_SME_SWITCH_CHL_REQ:
        case eWNI_SME_SWITCH_CHL_CB_PRIMARY_REQ:
        case eWNI_SME_SWITCH_CHL_CB_SECONDARY_REQ:
            bufConsumed = __limProcessSmeSwitchChlReq(pMac, pMsg);
            break;
#endif


        case eWNI_SME_SETCONTEXT_REQ:
            __limProcessSmeSetContextReq(pMac, pMsgBuf);

            break;

        case eWNI_SME_REMOVEKEY_REQ:
            __limProcessSmeRemoveKeyReq(pMac, pMsgBuf);

            break;

        case eWNI_SME_STOP_BSS_REQ:
            bufConsumed = __limProcessSmeStopBssReq(pMac, pMsg);
            break;

        case eWNI_SME_ASSOC_CNF:
        case eWNI_SME_REASSOC_CNF:
            if (pMsg->type == eWNI_SME_ASSOC_CNF)
                PELOG1(limLog(pMac, LOG1, FL("Received ASSOC_CNF message\n"));)
            else
                PELOG1(limLog(pMac, LOG1, FL("Received REASSOC_CNF message\n"));)
#ifdef ANI_PRODUCT_TYPE_AP
            __limProcessSmeAssocCnf(pMac, pMsg->type, pMsgBuf);
#endif
            __limProcessSmeAssocCnfNew(pMac, pMsg->type, pMsgBuf);
            break;

        case eWNI_SME_ADDTS_REQ:
            PELOG1(limLog(pMac, LOG1, FL("Received ADDTS_REQ message\n"));)
            __limProcessSmeAddtsReq(pMac, pMsgBuf);
            break;

        case eWNI_SME_DELTS_REQ:
            PELOG1(limLog(pMac, LOG1, FL("Received DELTS_REQ message\n"));)
            __limProcessSmeDeltsReq(pMac, pMsgBuf);
            break;

        case SIR_LIM_ADDTS_RSP_TIMEOUT:
            PELOG1(limLog(pMac, LOG1, FL("Received SIR_LIM_ADDTS_RSP_TIMEOUT message \n"));)
            limProcessSmeAddtsRspTimeout(pMac, pMsg->bodyval);
            break;

        case eWNI_SME_STA_STAT_REQ:
        case eWNI_SME_AGGR_STAT_REQ:
        case eWNI_SME_GLOBAL_STAT_REQ:
        case eWNI_SME_STAT_SUMM_REQ:
            __limProcessSmeStatsRequest( pMac, pMsgBuf);
            //HAL consumes pMsgBuf. It will be freed there. Set bufConsumed to false.
            bufConsumed = FALSE;
            break;
        case eWNI_SME_GET_STATISTICS_REQ:
            __limProcessSmeGetStatisticsRequest( pMac, pMsgBuf);
            //HAL consumes pMsgBuf. It will be freed there. Set bufConsumed to false.
            bufConsumed = FALSE;
            break;              
        case eWNI_SME_DEL_BA_PEER_IND:
            limProcessSmeDelBaPeerInd(pMac, pMsgBuf);
            break;
        case eWNI_SME_GET_SCANNED_CHANNEL_REQ:
            limProcessSmeGetScanChannelInfo(pMac, pMsgBuf);
            break;
#ifdef WLAN_SOFTAP_FEATURE
        case eWNI_SME_GET_ASSOC_STAS_REQ:
            limProcessSmeGetAssocSTAsInfo(pMac, pMsgBuf);
            break;
        case eWNI_SME_TKIP_CNTR_MEAS_REQ:
            limProcessTkipCounterMeasures(pMac, pMsgBuf);
            break;

       case eWNI_SME_HIDE_SSID_REQ: 
            __limProcessSmeHideSSID(pMac, pMsgBuf);
            break;
       case eWNI_SME_UPDATE_APWPSIE_REQ: 
            __limProcessSmeUpdateAPWPSIEs(pMac, pMsgBuf);
            break;
        case eWNI_SME_GET_WPSPBC_SESSION_REQ:
             limProcessSmeGetWPSPBCSessions(pMac, pMsgBuf); 
             break;
         
        case eWNI_SME_SET_APWPARSNIEs_REQ:
              __limProcessSmeSetWPARSNIEs(pMac, pMsgBuf);        
              break;
#endif            
            
#if defined WLAN_FEATURE_VOWIFI 
        case eWNI_SME_NEIGHBOR_REPORT_REQ_IND:
        case eWNI_SME_BEACON_REPORT_RESP_XMIT_IND:
            __limProcessReportMessage(pMac, pMsg);
            break;
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
       case eWNI_SME_FT_PRE_AUTH_REQ:
            bufConsumed = (tANI_BOOLEAN)limProcessFTPreAuthReq(pMac, pMsg);
            break;
       case eWNI_SME_FT_UPDATE_KEY:
            limProcessFTUpdateKey(pMac, pMsgBuf);
            break;

       case eWNI_SME_FT_AGGR_QOS_REQ:
            limProcessFTAggrQosReq(pMac, pMsgBuf);
            break;
#endif

#if defined FEATURE_WLAN_CCX
       case eWNI_SME_CCX_ADJACENT_AP_REPORT:
            limProcessAdjacentAPRepMsg ( pMac, pMsgBuf );
            break;
#endif
       case eWNI_SME_ADD_STA_SELF_REQ:
            __limProcessSmeAddStaSelfReq( pMac, pMsgBuf );
            break;
        case eWNI_SME_DEL_STA_SELF_REQ:
            __limProcessSmeDelStaSelfReq( pMac, pMsgBuf );
            break;

#ifdef WLAN_FEATURE_P2P
        case eWNI_SME_REGISTER_MGMT_FRAME_REQ:
            __limProcessSmeRegisterMgmtFrameReq( pMac, pMsgBuf );
            break;
#endif        
            

        default:
            vos_mem_free((v_VOID_t*)pMsg->bodyptr);
            pMsg->bodyptr = NULL;
            break;
    } // switch (msgType)

    return bufConsumed;
} /*** end limProcessSmeReqMessages() ***/
