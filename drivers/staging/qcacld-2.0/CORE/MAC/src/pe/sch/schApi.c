/*
 * Copyright (c) 2011-2014, 2016 The Linux Foundation. All rights reserved.
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
#include "aniGlobal.h"
#include "wniCfgSta.h"

#include "sirMacProtDef.h"
#include "sirMacPropExts.h"
#include "sirCommon.h"


#include "cfgApi.h"
#include "pmmApi.h"

#include "limApi.h"

#include "schApi.h"
#include "schDebug.h"

#include "schSysParams.h"
#include "limTrace.h"
#include "limTypes.h"
#include "limUtils.h"

#include "wlan_qct_wda.h"

//--------------------------------------------------------------------
//
//                          Static Variables
//
//-------------------------------------------------------------------

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
    schProcessMessage(pMac, pMsg);

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

    PELOG1(schLog(pMac, LOG1, FL("Sending LIM message to go into scan"));)
    msgQ.type = SIR_SCH_START_SCAN_RSP;
    if ((retCode = limPostMsgApi(pMac, &msgQ)) != eSIR_SUCCESS)
        schLog(pMac, LOGE,
               FL("Posting START_SCAN_RSP to LIM failed, reason=%X"), retCode);
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
         FL( "Indicating HAL to copy the beacon template [%d bytes] to memory" ),
         size );

  beaconParams = vos_mem_malloc(sizeof(tSendbeaconParams));
  if ( NULL == beaconParams )
      return eSIR_FAILURE;

  msgQ.type = WDA_SEND_BEACON_REQ;

  // No Dialog Token reqd, as a response is not solicited
  msgQ.reserved = 0;

  // Fill in tSendbeaconParams members
  vos_mem_copy(beaconParams->bssId, psessionEntry->bssId, sizeof(psessionEntry->bssId));

  if (LIM_IS_IBSS_ROLE(psessionEntry)) {
      beaconParams->timIeOffset = 0;
  } else {
      beaconParams->timIeOffset = psessionEntry->schBeaconOffsetBegin;
  }

  /* p2pIeOffset should be at-least greater than timIeOffset */
  if ((pMac->sch.schObject.p2pIeOffset != 0) &&
          (pMac->sch.schObject.p2pIeOffset <
           psessionEntry->schBeaconOffsetBegin))
  {
      schLog(pMac, LOGE,FL("Invalid p2pIeOffset:[%d]"),
              pMac->sch.schObject.p2pIeOffset);
      VOS_ASSERT( 0 );
      vos_mem_free(beaconParams);
      return eSIR_FAILURE;
  }
  beaconParams->p2pIeOffset = pMac->sch.schObject.p2pIeOffset;
#ifdef WLAN_SOFTAP_FW_BEACON_TX_PRNT_LOG
  schLog(pMac, LOGE,FL("TimIeOffset:[%d]"),beaconParams->TimIeOffset );
#endif

  beaconParams->beacon = beaconPayload;
  beaconParams->beaconLength = (tANI_U32) size;
  msgQ.bodyptr = beaconParams;
  msgQ.bodyval = 0;

  // Keep a copy of recent beacon frame sent

  // free previous copy of the beacon
  if (psessionEntry->beacon )
  {
    vos_mem_free(psessionEntry->beacon);
  }

  psessionEntry->bcnLen = 0;
  psessionEntry->beacon = NULL;

  psessionEntry->beacon = vos_mem_malloc(size);
  if ( psessionEntry->beacon != NULL )
  {
    vos_mem_copy(psessionEntry->beacon, beaconPayload, size);
    psessionEntry->bcnLen = size;
  }

  MTRACE(macTraceMsgTx(pMac, psessionEntry->peSessionId, msgQ.type));
  if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
  {
    schLog( pMac, LOGE,
        FL("Posting SEND_BEACON_REQ to HAL failed, reason=%X"),
        retCode );
  } else
  {
    schLog( pMac, LOG2,
        FL("Successfully posted WDA_SEND_BEACON_REQ to HAL"));

    if (LIM_IS_AP_ROLE(psessionEntry) &&
        pMac->sch.schObject.fBeaconChanged) {
        if(eSIR_SUCCESS != (retCode = limSendProbeRspTemplateToHal(pMac,psessionEntry,
                                    &psessionEntry->DefProbeRspIeBitmap[0])))
        {
            /* check whether we have to free any memory */
            schLog(pMac, LOGE, FL("FAILED to send probe response template with retCode %d"), retCode);
        }
    }
  }

  return retCode;
}

tANI_U32 limRemoveP2pIeFromAddIe(tpAniSirGlobal pMac,
                                 tpPESession psessionEntry,
                                 tANI_U8 *addIeWoP2pIe,
                                 tANI_U32 *addnIELenWoP2pIe)
{
    tANI_U32 left = psessionEntry->addIeParams.probeRespDataLen;
    v_U8_t *ptr = psessionEntry->addIeParams.probeRespData_buff;
    v_U8_t elem_id,elem_len;
    tANI_U32 offset=0;
    v_U8_t eid = 0xDD;

    vos_mem_copy(addIeWoP2pIe, ptr, left);
    *addnIELenWoP2pIe = left;

    if (addIeWoP2pIe != NULL)
    {
        while (left >= 2)
        {
            elem_id  = ptr[0];
            elem_len = ptr[1];
            left -= 2;
            if(elem_len > left)
            {
                schLog(pMac, LOGE, FL("Invalid IEs"));
                return eSIR_FAILURE;
            }
            if ((elem_id == eid) &&
                (vos_mem_compare( &ptr[2], "\x50\x6f\x9a\x09", 4)==VOS_TRUE))
            {
                left -= elem_len;
                ptr += (elem_len + 2);
                vos_mem_copy(&addIeWoP2pIe[offset], ptr, left);
                *addnIELenWoP2pIe -= (2 + elem_len);
            }
            else
            {
                left -= elem_len;
                ptr += (elem_len + 2);
                offset += 2 + elem_len;
            }
        }
    }
    return eSIR_SUCCESS;
}

tANI_U32 limSendProbeRspTemplateToHal(tpAniSirGlobal pMac,tpPESession psessionEntry
                                  ,tANI_U32* IeBitmap)
{
    tSirMsgQ  msgQ;
    tANI_U8 *pFrame2Hal = psessionEntry->pSchProbeRspTemplate;
    tpSendProbeRespParams pprobeRespParams=NULL;
    tANI_U32  retCode = eSIR_FAILURE;
    tANI_U32             nPayload,nBytes,nStatus;
    tpSirMacMgmtHdr      pMacHdr;
    tANI_U32             addnIEPresent = VOS_FALSE;
    tSirRetStatus        nSirStatus;
    tANI_U8              *addIE = NULL;
    tANI_U8              *addIeWoP2pIe = NULL;
    tANI_U32             addnIELenWoP2pIe = 0;
    tANI_U32             retStatus;
    tDot11fIEExtCap extracted_extcap;
    bool extcap_present = false;
    tDot11fProbeResponse *prb_rsp_frm;
    tSirRetStatus status;
    uint16_t addn_ielen = 0;

    nStatus = dot11fGetPackedProbeResponseSize( pMac, &psessionEntry->probeRespFrame, &nPayload );
    if ( DOT11F_FAILED( nStatus ) )
    {
        schLog( pMac, LOGE, FL("Failed to calculate the packed size f"
                               "or a Probe Response (0x%08x)."),
                nStatus );
        // We'll fall back on the worst case scenario:
        nPayload = sizeof( tDot11fProbeResponse );
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        schLog( pMac, LOGE, FL("There were warnings while calculating"
                               "the packed size for a Probe Response "
                               "(0x%08x)."), nStatus );
    }

    nBytes = nPayload + sizeof( tSirMacMgmtHdr );

    //Check if probe response IE is present or not
    addnIEPresent = (psessionEntry->addIeParams.probeRespDataLen != 0);
    if (addnIEPresent)
    {
        /*
         * probe response template should not have P2P IE.
         * In case probe request has P2P IE or WPS IE, the
         * probe request will be forwarded to the Host and
         * Host will send the probe response. In other cases
         * FW will send the probe response. So, if the template
         * has P2P IE, the probe response sent to non P2P devices
         * by the FW, may also have P2P IE which will fail
         * P2P cert case 6.1.3
         */
        addIeWoP2pIe = vos_mem_malloc(psessionEntry->addIeParams.probeRespDataLen);
        if ( NULL == addIeWoP2pIe )
        {
            schLog(pMac, LOGE, FL("FAILED to alloc memory when removing P2P IE"));
            return eSIR_FAILURE;
        }

        retStatus = limRemoveP2pIeFromAddIe(pMac, psessionEntry,
                                            addIeWoP2pIe, &addnIELenWoP2pIe);
        if (retStatus != eSIR_SUCCESS)
        {
            vos_mem_free(addIeWoP2pIe);
            return eSIR_FAILURE;
        }

        //Probe rsp IE available
        /*need to check the data length*/
        addIE = vos_mem_malloc(addnIELenWoP2pIe);

        if ( NULL == addIE )
        {
             schLog(pMac, LOGE,
                 FL("Unable to get WNI_CFG_PROBE_RSP_ADDNIE_DATA1 length"));
             vos_mem_free(addIeWoP2pIe);
             return retCode;
        }
        addn_ielen = addnIELenWoP2pIe;

        if (addn_ielen <= WNI_CFG_PROBE_RSP_ADDNIE_DATA1_LEN && addn_ielen &&
                                 (nBytes + addn_ielen) <= SIR_MAX_PACKET_SIZE)
        {
            vos_mem_copy(addIE, addIeWoP2pIe, addnIELenWoP2pIe);
        }
        vos_mem_free(addIeWoP2pIe);

        vos_mem_set((uint8_t *)&extracted_extcap, sizeof(tDot11fIEExtCap), 0);
        status = lim_strip_extcap_update_struct(pMac, addIE, &addn_ielen,
                                                   &extracted_extcap);
        if (eSIR_SUCCESS != status) {
            limLog(pMac, LOG1, FL("extcap not extracted"));
        } else {
            extcap_present = true;
        }

    }

    if (addnIEPresent)
    {
        if ((nBytes + addn_ielen) <= SIR_MAX_PACKET_SIZE )
            nBytes += addn_ielen;
        else
            addnIEPresent = false; //Dont include the IE.
    }


    // Paranoia:
    vos_mem_set(pFrame2Hal, nBytes, 0);

    // Next, we fill out the buffer descriptor:
    nSirStatus = limPopulateMacHeader( pMac, pFrame2Hal, SIR_MAC_MGMT_FRAME,
                                SIR_MAC_MGMT_PROBE_RSP, psessionEntry->selfMacAddr,psessionEntry->selfMacAddr);

    if ( eSIR_SUCCESS != nSirStatus )
    {
        schLog( pMac, LOGE, FL("Failed to populate the buffer descrip"
                               "tor for a Probe Response (%d)."),
                nSirStatus );

        vos_mem_free(addIE);
        return retCode;
    }

    pMacHdr = ( tpSirMacMgmtHdr ) pFrame2Hal;

    sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);

    /* merge extcap IE */
    prb_rsp_frm = &psessionEntry->probeRespFrame;
    if (extcap_present)
        lim_merge_extcap_struct(&prb_rsp_frm->ExtCap, &extracted_extcap);

    // That done, pack the Probe Response:
    nStatus = dot11fPackProbeResponse( pMac, &psessionEntry->probeRespFrame, pFrame2Hal + sizeof(tSirMacMgmtHdr),
                                       nPayload, &nPayload );

    if ( DOT11F_FAILED( nStatus ) )
    {
        schLog( pMac, LOGE, FL("Failed to pack a Probe Response (0x%08x)."),
                nStatus );

        vos_mem_free(addIE);
        return retCode;                 // allocated!
    }
    else if ( DOT11F_WARNED( nStatus ) )
    {
        schLog( pMac, LOGE, FL("There were warnings while packing a P"
                               "robe Response (0x%08x)."), nStatus );
    }

    if (addnIEPresent)
        vos_mem_copy(&pFrame2Hal[nBytes - addn_ielen], &addIE[0], addn_ielen);

    /* free the allocated Memory */
    vos_mem_free(addIE);

    pprobeRespParams = vos_mem_malloc(sizeof( tSendProbeRespParams ));
    if ( NULL == pprobeRespParams )
    {
        schLog( pMac, LOGE, FL("limSendProbeRspTemplateToHal: HAL probe response params malloc failed for bytes %d"), nBytes );
    }
    else
    {
        sirCopyMacAddr( pprobeRespParams->bssId, psessionEntry->bssId);
        pprobeRespParams->pProbeRespTemplate   = pFrame2Hal;
        pprobeRespParams->probeRespTemplateLen = nBytes;
        vos_mem_copy(pprobeRespParams->ucProxyProbeReqValidIEBmap,IeBitmap,(sizeof(tANI_U32) * 8));
        msgQ.type     = WDA_SEND_PROBE_RSP_TMPL;
        msgQ.reserved = 0;
        msgQ.bodyptr  = pprobeRespParams;
        msgQ.bodyval  = 0;

        if( eSIR_SUCCESS != (retCode = wdaPostCtrlMsg( pMac, &msgQ )))
        {
            /* free the allocated Memory */
            schLog( pMac,LOGE, FL("limSendProbeRspTemplateToHal: FAIL bytes %d retcode[%X]"), nBytes, retCode );
            vos_mem_free(pprobeRespParams);
        }
        else
        {
            schLog( pMac,LOG1, FL("limSendProbeRspTemplateToHal: Probe response template msg posted to HAL of bytes %d"),nBytes );
        }
    }

    return retCode;
}

/**
 * schGenTimingAdvertFrame() - Generate the TA frame and populate the buffer
 * @pMac: the global MAC context
 * @self_addr: the self MAC address
 * @buf: the buffer that will contain the frame
 * @timestamp_offset: return for the offset of the timestamp field
 * @time_value_offset: return for the time_value field in the TA IE
 *
 * Return: the length of the buffer.
 */
int schGenTimingAdvertFrame(tpAniSirGlobal mac_ctx, tSirMacAddr self_addr,
    uint8_t **buf, uint32_t *timestamp_offset, uint32_t *time_value_offset)
{
    tDot11fTimingAdvertisementFrame frame;
    uint32_t payload_size, buf_size;
    int status;
    v_MACADDR_t wildcard_bssid = {
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    };

    vos_mem_zero((uint8_t*)&frame, sizeof(tDot11fTimingAdvertisementFrame));

    /* Populate the TA fields */
    status = PopulateDot11fTimingAdvertFrame(mac_ctx, &frame);
    if (status) {
      schLog(mac_ctx, LOGE, FL("Error populating TA frame %x"), status);
      return status;
    }

    status = dot11fGetPackedTimingAdvertisementFrameSize(mac_ctx, &frame,
        &payload_size);
    if (DOT11F_FAILED(status)) {
        schLog(mac_ctx, LOGE, FL("Error getting packed frame size %x"), status);
        return status;
    } else if (DOT11F_WARNED(status)) {
        schLog(mac_ctx, LOGW, FL("Warning getting packed frame size"));
    }

    buf_size = sizeof(tSirMacMgmtHdr) + payload_size;
    *buf = vos_mem_malloc(buf_size);
    if (*buf == NULL) {
        schLog(mac_ctx, LOGE, FL("Cannot allocate memory"));
        return eSIR_FAILURE;
    }
    vos_mem_zero(*buf, buf_size);

    payload_size = 0;
    status = dot11fPackTimingAdvertisementFrame(mac_ctx, &frame,
        *buf + sizeof(tSirMacMgmtHdr), buf_size - sizeof(tSirMacMgmtHdr),
        &payload_size);
    schLog(mac_ctx, LOGE, FL("TA payload size2 = %d"), payload_size);
    if (DOT11F_FAILED(status)) {
        schLog(mac_ctx, LOGE, FL("Error packing frame %x"), status);
        goto fail;
    } else if (DOT11F_WARNED(status)) {
        schLog(mac_ctx, LOGE, FL("Warning packing frame"));
    }

    limPopulateMacHeader(mac_ctx, *buf, SIR_MAC_MGMT_FRAME,
        SIR_MAC_MGMT_TIME_ADVERT, wildcard_bssid.bytes, self_addr);

    /* The timestamp field is right after the header */
    *timestamp_offset = sizeof(tSirMacMgmtHdr);

    *time_value_offset = sizeof(tSirMacMgmtHdr) + sizeof(tDot11fFfTimeStamp) +
        sizeof(tDot11fFfCapabilities);

    /* Add the Country IE length */
    dot11fGetPackedIECountry(mac_ctx, &frame.Country, time_value_offset);
    /* Add 2 for Country IE EID and Length fields */
    *time_value_offset += 2;

    /* Add the PowerConstraint IE size */
    if (frame.Country.present == 1)
        *time_value_offset += 3;

    /* Add the offset inside TA IE */
    *time_value_offset += 3;

    return payload_size + sizeof(tSirMacMgmtHdr);

fail:
    if (*buf)
        vos_mem_free(*buf);
    return status;
}
