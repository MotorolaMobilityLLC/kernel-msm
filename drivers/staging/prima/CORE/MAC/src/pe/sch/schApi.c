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

/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file schApi.cc contains functions related to the API exposed
 * by scheduler module
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "palTypes.h"
#include "sirWrapper.h"
#include "aniGlobal.h"
#include "wniCfgAp.h"

#include "sirMacProtDef.h"
#include "sirMacPropExts.h"
#include "sirCommon.h"

#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halDataStruct.h"
#include "halCommonApi.h"
#endif

#include "cfgApi.h"
#include "pmmApi.h"

#include "limApi.h"

#include "schApi.h"
#include "schDebug.h"

#include "schSysParams.h"
#include "limTrace.h"
#ifdef WLAN_SOFTAP_FEATURE
#include "limTypes.h"
#endif

#include "wlan_qct_wda.h"

//--------------------------------------------------------------------
//
//                          Static Variables
//
//-------------------------------------------------------------------
static tANI_U8 gSchProbeRspTemplate[SCH_MAX_PROBE_RESP_SIZE];
static tANI_U8 gSchBeaconFrameBegin[SCH_MAX_BEACON_SIZE];
static tANI_U8 gSchBeaconFrameEnd[SCH_MAX_BEACON_SIZE];

// --------------------------------------------------------------------
/**
 * schGetCFPCount
 *
 * FUNCTION:
 * Function used by other Sirius modules to read CFPcount
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

tANI_U8
schGetCFPCount(tpAniSirGlobal pMac)
{
    return pMac->sch.schObject.gSchCFPCount;
}

// --------------------------------------------------------------------
/**
 * schGetCFPDurRemaining
 *
 * FUNCTION:
 * Function used by other Sirius modules to read CFPDuration remaining
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

tANI_U16
schGetCFPDurRemaining(tpAniSirGlobal pMac)
{
    return pMac->sch.schObject.gSchCFPDurRemaining;
}


// --------------------------------------------------------------------
/**
 * schInitialize
 *
 * FUNCTION:
 * Initialize
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

void
schInitialize(tpAniSirGlobal pMac)
{
    pmmInitialize(pMac);
}

// --------------------------------------------------------------------
/**
 * schInitGlobals
 *
 * FUNCTION:
 * Initialize globals
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

void
schInitGlobals(tpAniSirGlobal pMac)
{
    pMac->sch.gSchHcfEnabled = false;

    pMac->sch.gSchScanRequested = false;
    pMac->sch.gSchScanReqRcvd = false;

    pMac->sch.gSchGenBeacon = 1;
    pMac->sch.gSchBeaconsSent = 0;
    pMac->sch.gSchBeaconsWritten = 0;
    pMac->sch.gSchBcnParseErrorCnt = 0;
    pMac->sch.gSchBcnIgnored = 0;
    pMac->sch.gSchBBXportRcvCnt = 0;
    pMac->sch.gSchUnknownRcvCnt = 0;
    pMac->sch.gSchBcnRcvCnt = 0;
    pMac->sch.gSchRRRcvCnt = 0;
    pMac->sch.qosNullCnt = 0;
    pMac->sch.numData = 0;
    pMac->sch.numPoll = 0;
    pMac->sch.numCorrupt = 0;
    pMac->sch.numBogusInt = 0;
    pMac->sch.numTxAct0 = 0;
    pMac->sch.rrTimeout = SCH_RR_TIMEOUT;
    pMac->sch.pollPeriod = SCH_POLL_PERIOD;
    pMac->sch.keepAlive = 0;
    pMac->sch.multipleSched = 1;
    pMac->sch.maxPollTimeouts = 20;
    pMac->sch.checkCfbFlagStuck = 0;

    pMac->sch.schObject.gSchProbeRspTemplate = gSchProbeRspTemplate;
    pMac->sch.schObject.gSchBeaconFrameBegin = gSchBeaconFrameBegin;
    pMac->sch.schObject.gSchBeaconFrameEnd   = gSchBeaconFrameEnd;

}

// --------------------------------------------------------------------
/**
 * schPostMessage
 *
 * FUNCTION:
 * Post the beacon message to the scheduler message queue
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param pMsg pointer to message
 * @return None
 */

tSirRetStatus
schPostMessage(tpAniSirGlobal pMac, tpSirMsgQ pMsg)
{
#if defined(ANI_OS_TYPE_LINUX) || defined(ANI_OS_TYPE_OSX)
   PELOG3(schLog(pMac, LOG3, FL("Going to post message (%x) to SCH message queue\n"),
           pMsg->type);)
    if (tx_queue_send(&pMac->sys.gSirSchMsgQ, pMsg, TX_NO_WAIT) != TX_SUCCESS)
        return eSIR_FAILURE;
#else
    schProcessMessage(pMac, pMsg);
#endif 

    return eSIR_SUCCESS;
}





// ---------------------------------------------------------------------------
/**
 * schSendStartScanRsp
 *
 * FUNCTION:
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

void
schSendStartScanRsp(tpAniSirGlobal pMac)
{
    tSirMsgQ        msgQ;
    tANI_U32        retCode;

    PELOG1(schLog(pMac, LOG1, FL("Sending LIM message to go into scan\n"));)
    msgQ.type = SIR_SCH_START_SCAN_RSP;
    if ((retCode = limPostMsgApi(pMac, &msgQ)) != eSIR_SUCCESS)
        schLog(pMac, LOGE,
               FL("Posting START_SCAN_RSP to LIM failed, reason=%X\n"), retCode);
}

/**
 * schSendBeaconReq
 *
 * FUNCTION:
 *
 * LOGIC:
 * 1) SCH received SIR_SCH_BEACON_GEN_IND
 * 2) SCH updates TIM IE and other beacon related IE's
 * 3) SCH sends WDA_SEND_BEACON_REQ to HAL. HAL then copies the beacon
 *    template to memory
 *
 * ASSUMPTIONS:
 * Memory allocation is reqd to send this message and SCH allocates memory.
 * The assumption is that HAL will "free" this memory.
 *
 * NOTE:
 *
 * @param pMac global
 *
 * @param beaconPayload
 *
 * @param size - Length of the beacon
 *
 * @return eHalStatus
 */
tSirRetStatus schSendBeaconReq( tpAniSirGlobal pMac, tANI_U8 *beaconPayload, tANI_U16 size, tpPESession psessionEntry)
{
    tSirMsgQ msgQ;
    tpSendbeaconParams beaconParams = NULL;
    tSirRetStatus retCode;

  schLog( pMac, LOG2,
      FL( "Indicating HAL to copy the beacon template [%d bytes] to memory\n" ),
      size );

  if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
          (void **) &beaconParams,
          sizeof( tSendbeaconParams )))
    return eSIR_FAILURE;

  msgQ.type = WDA_SEND_BEACON_REQ;

  // No Dialog Token reqd, as a response is not solicited
  msgQ.reserved = 0;

  // Fill in tSendbeaconParams members
  /* Knock off all pMac global addresses */
  // limGetBssid( pMac, beaconParams->bssId);
  palCopyMemory(pMac, beaconParams->bssId, psessionEntry->bssId, sizeof(psessionEntry->bssId));

#ifdef WLAN_SOFTAP_FEATURE
  beaconParams->timIeOffset = pMac->sch.schObject.gSchBeaconOffsetBegin;
#ifdef WLAN_FEATURE_P2P
  beaconParams->p2pIeOffset = pMac->sch.schObject.p2pIeOffset;
#endif
#ifdef WLAN_SOFTAP_FW_BEACON_TX_PRNT_LOG
  schLog(pMac, LOGE,FL("TimIeOffset:[%d]\n"),beaconParams->TimIeOffset );
#endif
#endif

  beaconParams->beacon = beaconPayload;
  beaconParams->beaconLength = (tANI_U32) size;
  msgQ.bodyptr = beaconParams;
  msgQ.bodyval = 0;

  // Keep a copy of recent beacon frame sent

  // free previous copy of the beacon
  if (psessionEntry->beacon )
  {
    palFreeMemory(pMac->hHdd, psessionEntry->beacon);
  }

  psessionEntry->bcnLen = 0;
  psessionEntry->beacon = NULL;

  if ( eHAL_STATUS_SUCCESS == palAllocateMemory( pMac->hHdd,(void **) &psessionEntry->beacon, size)) 
  {
    palCopyMemory(pMac->hHdd, psessionEntry->beacon, beaconPayload, size);
    psessionEntry->bcnLen = size;
  }

  MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));
  if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
  {
    schLog( pMac, LOGE,
        FL("Posting SEND_BEACON_REQ to HAL failed, reason=%X\n"),
        retCode );
  } else
  {
    schLog( pMac, LOG2,
        FL("Successfully posted WDA_SEND_BEACON_REQ to HAL\n"));

#ifdef WLAN_SOFTAP_FEATURE
    if( (psessionEntry->limSystemRole == eLIM_AP_ROLE ) 
        && (psessionEntry->proxyProbeRspEn)
        && (pMac->sch.schObject.fBeaconChanged))
    {
        if(eSIR_SUCCESS != (retCode = limSendProbeRspTemplateToHal(pMac,psessionEntry,
                                    &psessionEntry->DefProbeRspIeBitmap[0])))
        {
            /* check whether we have to free any memory */
            schLog(pMac, LOGE, FL("FAILED to send probe response template with retCode %d\n"), retCode);
        }
    }
#endif
  }

  return retCode;
}

#ifdef WLAN_SOFTAP_FEATURE
tANI_U32 limSendProbeRspTemplateToHal(tpAniSirGlobal pMac,tpPESession psessionEntry
                                    ,tANI_U32* IeBitmap)
{
    tSirMsgQ  msgQ;
    tANI_U8 *pFrame2Hal = pMac->sch.schObject.gSchProbeRspTemplate;
    tpSendProbeRespParams pprobeRespParams=NULL;
    tANI_U32  retCode = eSIR_FAILURE;
    tANI_U32             nPayload,nBytes,nStatus;
    tpSirMacMgmtHdr      pMacHdr;
    tANI_U32             addnIEPresent;
    tANI_U32             addnIELen=0;
    tSirRetStatus        nSirStatus;

    nStatus = dot11fGetPackedProbeResponseSize( pMac, &psessionEntry->probeRespFrame, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        schLog( pMac, LOGE, FL("Failed to calculate the packed size f"
                               "or a Probe Response (0x%08x).\n"),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fProbeResponse );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        schLog( pMac, LOGE, FL("There were warnings while calculating"
                               "the packed size for a Probe Response "
                               "(0x%08x).\n"), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );
    
    //Check if probe response IE is set first before checking beacon/probe rsp IE
    if(wlan_cfgGetInt(pMac, WNI_CFG_PROBE_RSP_ADDNIE_FLAG, &addnIEPresent) != eSIR_SUCCESS)
    {
        schLog(pMac, LOGE, FL("Unable to get WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG\n"));
        return retCode;
    }

    if(!addnIEPresent)
    {
        //TODO: If additional IE needs to be added. Add then alloc required buffer.
        if(wlan_cfgGetInt(pMac, WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG, &addnIEPresent) != eSIR_SUCCESS)
        {
            schLog(pMac, LOGE, FL("Unable to get WNI_CFG_PROBE_RSP_BCN_ADDNIE_FLAG\n"));
            return retCode;
        }
    
        if(addnIEPresent)
        {
            if(wlan_cfgGetStrLen(pMac, WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA, &addnIELen) != eSIR_SUCCESS)
            {
                schLog(pMac, LOGE, FL("Unable to get WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA length"));
                return retCode;
            }
        }
    }
    else
    {
        //Probe rsp IE available
        if(wlan_cfgGetStrLen(pMac, WNI_CFG_PROBE_RSP_ADDNIE_DATA1, &addnIELen) != eSIR_SUCCESS)
        {
            limLog(pMac, LOGP, FL("Unable to get WNI_CFG_PROBE_RSP_BCN_ADDNIE_DATA length"));
            return retCode;
        }
    }

    if(addnIEPresent)
    {
        if((nBytes + addnIELen) <= SIR_MAX_PACKET_SIZE ) 
            nBytes += addnIELen;
        else 
            addnIEPresent = false; //Dont include the IE.     
    }
       
    // Paranoia:
    palZeroMemory( pMac->hHdd, pFrame2Hal, nBytes );

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame2Hal, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_PROBE_RSP, psessionEntry->selfMacAddr,psessionEntry->selfMacAddr);

    if ( eSIR_SUCCESS != nSirStatus )
    {
        schLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Probe Response (%d).\n"),
                nSirStatus );
        return retCode;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame2Hal;
  
    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);
    
    // That done, pack the Probe Response:
    nStatus = dot11fPackProbeResponse( pMac, &psessionEntry->probeRespFrame, pFrame2Hal + sizeof(tSirMacMgmtHdr),
                                       nPayload, &nPayload );

    if ( DOT11F_FAILED( nStatus ) )
    {
        schLog( pMac, LOGE, FL("Failed to pack a Probe Response (0x%08x).\n"),
                nStatus );
        return retCode;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        schLog( pMac, LOGE, FL("There were warnings while packing a P"
                               "robe Response (0x%08x).\n") );
    }

    if( eHAL_STATUS_SUCCESS != palAllocateMemory( pMac->hHdd,
                                                (void **) &pprobeRespParams,
                                                sizeof( tSendProbeRespParams )))
    {
        schLog( pMac, LOGE, FL("limSendProbeRspTemplateToHal: HAL probe response params malloc failed for bytes %d\n"), nBytes );
    }
    else
    {
        /*
        PELOGE(sirDumpBuf(pMac, SIR_LIM_MODULE_ID, LOGE,
                            pFrame2Hal,
                            nBytes);)
        */

        sirCopyMacAddr( pprobeRespParams->bssId  ,  psessionEntry->bssId);
        pprobeRespParams->pProbeRespTemplate   = pFrame2Hal;
        pprobeRespParams->probeRespTemplateLen = nBytes;
        palCopyMemory(pMac,pprobeRespParams->ucProxyProbeReqValidIEBmap,IeBitmap,
                            (sizeof(tANI_U32) * 8));
        msgQ.type     = WDA_UPDATE_PROBE_RSP_TEMPLATE_IND; 
        msgQ.reserved = 0;
        msgQ.bodyptr  = pprobeRespParams;
        msgQ.bodyval  = 0;

        if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
        {
            /* free the allocated Memory */
            schLog( pMac,LOGE, FL("limSendProbeRspTemplateToHal: FAIL bytes %d retcode[%X]\n"), nBytes , retCode );
            palFreeMemory(pMac->hHdd,pprobeRespParams);
        }
        else
        {
            schLog( pMac,LOGE, FL("limSendProbeRspTemplateToHal: Probe response template msg posted to HAL of bytes %d \n"),nBytes );
        }
    }

    return retCode;
}
#endif

