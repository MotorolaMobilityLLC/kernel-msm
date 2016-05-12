/*
 * Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
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

/*===========================================================================

                      limProcessTdls.c

  OVERVIEW:

  DEPENDENCIES:

  Are listed for each API below.
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


   $Header$$DateTime$$Author$


  when        who     what, where, why
----------    ---    --------------------------------------------------------
05/05/2010   Ashwani    Initial Creation, added TDLS action frame functionality,
                         TDLS message exchange with SME..etc..

===========================================================================*/


/**
 * \file limProcessTdls.c
 *
 * \brief Code for preparing,processing and sending 802.11z action frames
 *
 */

#ifdef FEATURE_WLAN_TDLS

#include "sirApi.h"
#include "aniGlobal.h"
#include "sirMacProtDef.h"
#include "cfgApi.h"
#include "utilsApi.h"
#include "limTypes.h"
#include "limUtils.h"
#include "limSecurityUtils.h"
#include "dot11f.h"
#include "limStaHashApi.h"
#include "schApi.h"
#include "limSendMessages.h"
#include "utilsParser.h"
#include "limAssocUtils.h"
#include "dphHashTable.h"
#include "wlan_qct_wda.h"
#include "regdomain_common.h"

/* define NO_PAD_TDLS_MIN_8023_SIZE to NOT padding: See CR#447630
There was IOT issue with cisco 1252 open mode, where it pads
discovery req/teardown frame with some junk value up to min size.
To avoid this issue, we pad QCOM_VENDOR_IE.
If there is other IOT issue because of this bandage, define NO_PAD...
*/
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
#define MIN_IEEE_8023_SIZE              46
#define MIN_VENDOR_SPECIFIC_IE_SIZE     5
#endif

static tSirRetStatus limTdlsSetupAddSta(tpAniSirGlobal pMac,
                                        tSirTdlsAddStaReq *pAddStaReq,
                                        tpPESession psessionEntry) ;
void PopulateDot11fLinkIden(tpAniSirGlobal pMac, tpPESession psessionEntry,
                          tDot11fIELinkIdentifier *linkIden,
                             tSirMacAddr peerMac, tANI_U8 reqType) ;
void PopulateDot11fTdlsExtCapability(tpAniSirGlobal pMac,
                                     tpPESession psessionEntry,
                                     tDot11fIEExtCap *extCapability) ;

void PopulateDot11fTdlsOffchannelParams(tpAniSirGlobal pMac,
          tpPESession psessionEntry,
          tDot11fIESuppChannels *suppChannels,
          tDot11fIESuppOperatingClasses *suppOperClasses);
void limLogVHTCap(tpAniSirGlobal pMac,
                              tDot11fIEVHTCaps *pDot11f);
tSirRetStatus limPopulateVhtMcsSet(tpAniSirGlobal pMac,
                                  tpSirSupportedRates pRates,
                                  tDot11fIEVHTCaps *pPeerVHTCaps,
                                  tpPESession psessionEntry,
                                  uint8_t nss);
ePhyChanBondState  limGetHTCBState(ePhyChanBondState aniCBMode);

/*
 * TDLS data frames will go out/come in as non-qos data.
 * so, eth_890d_header will be aligned access..
 */
static const tANI_U8 eth_890d_header[] =
{
    0xaa, 0xaa, 0x03, 0x00,
    0x00, 0x00, 0x89, 0x0d,
} ;

/*
 * type of links used in TDLS
 */
enum tdlsLinks
{
    TDLS_LINK_AP,
    TDLS_LINK_DIRECT
} eTdlsLink ;

/*
 * node status in node searching
 */
enum tdlsLinkNodeStatus
{
    TDLS_NODE_NOT_FOUND,
    TDLS_NODE_FOUND
} eTdlsLinkNodeStatus ;


enum tdlsReqType
{
    TDLS_INITIATOR,
    TDLS_RESPONDER
} eTdlsReqType ;

typedef enum tdlsLinkSetupStatus
{
    TDLS_SETUP_STATUS_SUCCESS = 0,
    TDLS_SETUP_STATUS_FAILURE = 37
}etdlsLinkSetupStatus ;

/* These maps to Kernel TDLS peer capability
 * flags and should get changed as and when necessary
 */
enum tdls_peer_capability {
        TDLS_PEER_HT_CAP  = 0,
        TDLS_PEER_VHT_CAP = 1,
        TDLS_PEER_WMM_CAP = 2
} eTdlsPeerCapability;

/* some local defines */
#define LINK_IDEN_ADDR_OFFSET(x) (&x.LinkIdentifier)
#define PTI_LINK_IDEN_OFFSET     (5)
#define PTI_BUF_STATUS_OFFSET    (25)

/* TODO, Move this parameters to configuration */
#define PEER_PSM_SUPPORT          (0)
#define TDLS_SUPPORT              (1)
#define TDLS_PROHIBITED           (0)
#define TDLS_CH_SWITCH_PROHIBITED (1)
/** @brief Set bit manipulation macro */
#define SET_BIT(value,mask)       ((value) |= (1 << (mask)))
/** @brief Clear bit manipulation macro */
#define CLEAR_BIT(value,mask)     ((value) &= ~(1 << (mask)))
/** @brief Check bit manipulation macro */
#define CHECK_BIT(value, mask)    ((value) & (1 << (mask)))

#define SET_PEER_AID_BITMAP(peer_bitmap, aid) \
                                if ((aid) < (sizeof(tANI_U32) << 3)) \
                                        SET_BIT(peer_bitmap[0], (aid)); \
                                else if ((aid) < (sizeof(tANI_U32) << 4)) \
                                        SET_BIT(peer_bitmap[1], ((aid) - (sizeof(tANI_U32) << 3)));

#define CLEAR_PEER_AID_BITMAP(peer_bitmap, aid) \
                                if ((aid) < (sizeof(tANI_U32) << 3)) \
                                        CLEAR_BIT(peer_bitmap[0], (aid)); \
                                else if ((aid) < (sizeof(tANI_U32) << 4)) \
                                        CLEAR_BIT(peer_bitmap[1], ((aid) - (sizeof(tANI_U32) << 3)));


#ifdef LIM_DEBUG_TDLS

#ifdef FEATURE_WLAN_TDLS
#define WNI_CFG_TDLS_LINK_SETUP_RSP_TIMEOUT         (800)
#define WNI_CFG_TDLS_LINK_SETUP_CNF_TIMEOUT         (200)
#endif

#define IS_QOS_ENABLED(psessionEntry) ((((psessionEntry)->limQosEnabled) && \
                                                  SIR_MAC_GET_QOS((psessionEntry)->limCurrentBssCaps)) || \
                                       (((psessionEntry)->limWmeEnabled ) && \
                                                  LIM_BSS_CAPS_GET(WME, (psessionEntry)->limCurrentBssQosCaps)))

#define TID_AC_VI                  4
#define TID_AC_BK                  1

const tANI_U8* limTraceTdlsActionString( tANI_U8 tdlsActionCode )
{
   switch( tdlsActionCode )
   {
       CASE_RETURN_STRING(SIR_MAC_TDLS_SETUP_REQ);
       CASE_RETURN_STRING(SIR_MAC_TDLS_SETUP_RSP);
       CASE_RETURN_STRING(SIR_MAC_TDLS_SETUP_CNF);
       CASE_RETURN_STRING(SIR_MAC_TDLS_TEARDOWN);
       CASE_RETURN_STRING(SIR_MAC_TDLS_PEER_TRAFFIC_IND);
       CASE_RETURN_STRING(SIR_MAC_TDLS_CH_SWITCH_REQ);
       CASE_RETURN_STRING(SIR_MAC_TDLS_CH_SWITCH_RSP);
       CASE_RETURN_STRING(SIR_MAC_TDLS_PEER_TRAFFIC_RSP);
       CASE_RETURN_STRING(SIR_MAC_TDLS_DIS_REQ);
       CASE_RETURN_STRING(SIR_MAC_TDLS_DIS_RSP);
   }
   return (const tANI_U8*)"UNKNOWN";
}
#endif
/*
 * initialize TDLS setup list and related data structures.
 */
void limInitTdlsData(tpAniSirGlobal pMac, tpPESession pSessionEntry)
{
    limInitPeerIdxpool(pMac, pSessionEntry) ;

    return ;
}
/*
 * prepare TDLS frame header, it includes
 * |             |              |                |
 * |802.11 header|RFC1042 header|TDLS_PYLOAD_TYPE|PAYLOAD
 * |             |              |                |
 */
static tANI_U32 limPrepareTdlsFrameHeader(tpAniSirGlobal pMac, tANI_U8* pFrame,
           tDot11fIELinkIdentifier *link_iden, tANI_U8 tdlsLinkType, tANI_U8 reqType,
           tANI_U8 tid, tpPESession psessionEntry)
{
    tpSirMacDataHdr3a pMacHdr ;
    tANI_U32 header_offset = 0 ;
    tANI_U8 *addr1 = NULL ;
    tANI_U8 *addr3 = NULL ;
    tpDphHashNode pStaDs = NULL;
    tANI_U16      aid = 0;
    tANI_U8       qosMode = 0;

    tANI_U8 toDs = (tdlsLinkType == TDLS_LINK_AP)
                                       ? ANI_TXDIR_TODS :ANI_TXDIR_IBSS  ;
    tANI_U8 *peerMac = (reqType == TDLS_INITIATOR)
                                       ? link_iden->RespStaAddr : link_iden->InitStaAddr;
    tANI_U8 *staMac = (reqType == TDLS_INITIATOR)
                                       ? link_iden->InitStaAddr : link_iden->RespStaAddr;

    pMacHdr = (tpSirMacDataHdr3a) (pFrame);

    /*
     * if TDLS frame goes through the AP link, it follows normal address
     * pattern, if TDLS frame goes thorugh the direct link, then
     * A1--> Peer STA addr, A2-->Self STA address, A3--> BSSID
     */
    (tdlsLinkType == TDLS_LINK_AP) ? ((addr1 = (link_iden->bssid)),
                                      (addr3 = (peerMac)))
                                   : ((addr1 = (peerMac)),
                                     (addr3 = (link_iden->bssid))) ;
    /*
     * prepare 802.11 header
     */
    pMacHdr->fc.protVer = SIR_MAC_PROTOCOL_VERSION;
    pMacHdr->fc.type    = SIR_MAC_DATA_FRAME ;

    pStaDs = dphLookupHashEntry(pMac, peerMac, &aid,
                                      &psessionEntry->dph.dphHashTable);
    if (pStaDs)
    {
        qosMode = pStaDs->qosMode;
    }
    pMacHdr->fc.subType = ((IS_QOS_ENABLED(psessionEntry) &&
                           (tdlsLinkType == TDLS_LINK_AP)) ||
                           ((tdlsLinkType == TDLS_LINK_DIRECT) && qosMode))
                           ? SIR_MAC_DATA_QOS_DATA : SIR_MAC_DATA_DATA;

    /*
     * TL is not setting up below fields, so we are doing it here
     */
    pMacHdr->fc.toDS    = toDs ;
    pMacHdr->fc.powerMgmt = 0 ;
    pMacHdr->fc.wep = (psessionEntry->encryptType == eSIR_ED_NONE)? 0 : 1;


    vos_mem_copy( (tANI_U8 *) pMacHdr->addr1,
                  (tANI_U8 *)addr1,
                  sizeof( tSirMacAddr ));
    vos_mem_copy( (tANI_U8 *) pMacHdr->addr2,
                  (tANI_U8 *) staMac,
                  sizeof( tSirMacAddr ));

    vos_mem_copy( (tANI_U8 *) pMacHdr->addr3,
                  (tANI_U8 *) (addr3),
                  sizeof( tSirMacAddr ));

    limLog(pMac, LOG1,
           FL("Preparing TDLS frame header to %s A1:"MAC_ADDRESS_STR", A2:"MAC_ADDRESS_STR", A3:"MAC_ADDRESS_STR),
           (tdlsLinkType == TDLS_LINK_AP) ? "AP" : "DIRECT",
           MAC_ADDR_ARRAY(pMacHdr->addr1),
           MAC_ADDR_ARRAY(pMacHdr->addr2),
           MAC_ADDR_ARRAY(pMacHdr->addr3));

    if (((tdlsLinkType == TDLS_LINK_AP) && (IS_QOS_ENABLED(psessionEntry))) ||
        ((tdlsLinkType == TDLS_LINK_DIRECT) && qosMode))
    {
        pMacHdr->qosControl.tid = tid;
        header_offset += sizeof(tSirMacDataHdr3a);
    }
    else
        header_offset += sizeof(tSirMacMgmtHdr);

    /*
     * Now form RFC1042 header
     */
    vos_mem_copy((tANI_U8 *)(pFrame + header_offset),
                 (tANI_U8 *)eth_890d_header, sizeof(eth_890d_header)) ;

    header_offset += sizeof(eth_890d_header) ;

    /* add payload type as TDLS */
    *(pFrame + header_offset) = PAYLOAD_TYPE_TDLS ;

    return(header_offset += PAYLOAD_TYPE_TDLS_SIZE) ;
}

/**
 * lim_mgmt_tdls_tx_complete - callback to indicate Tx completion
 * @mac_ctx: pointer to mac structure
 * @tx_complete: indicates tx sucess/failure
 *
 * function will be invoked on receiving tx completion indication
 *
 * return: success: eHAL_STATUS_SUCCESS failure: eHAL_STATUS_FAILURE
 */
eHalStatus lim_mgmt_tdls_tx_complete(tpAniSirGlobal mac_ctx,
				uint32_t tx_complete)
{
	tpPESession session_entry = NULL ;

	limLog(mac_ctx, LOG1, FL("tdls_frm_session_id %x tx_complete %x"),
		mac_ctx->lim.tdls_frm_session_id, tx_complete);

	if (0xff != mac_ctx->lim.tdls_frm_session_id) {
		session_entry = peFindSessionBySessionId(mac_ctx,
					mac_ctx->lim.tdls_frm_session_id);
		if (NULL == session_entry) {
			limLog(mac_ctx, LOGE, FL("session id %d is not found"),
				mac_ctx->lim.tdls_frm_session_id);
			return eHAL_STATUS_FAILURE;
		}

		limSendSmeMgmtTXCompletion(mac_ctx, session_entry, tx_complete);
		mac_ctx->lim.tdls_frm_session_id = 0xff;
	}

	return eHAL_STATUS_SUCCESS;
}

/*
 * This function can be used for bacst or unicast discovery request
 * We are not differentiating it here, it will all depnds on peer MAC address,
 */
tSirRetStatus limSendTdlsDisReqFrame(tpAniSirGlobal pMac, tSirMacAddr peer_mac,
                                      tANI_U8 dialog, tpPESession psessionEntry)
{
    tDot11fTDLSDisReq   tdlsDisReq ;
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            size = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    tANI_U32            padLen = 0;
#endif
    tANI_U8             smeSessionId = 0;

    if (NULL == psessionEntry)
    {
        limLog(pMac, LOGE, FL("psessionEntry is NULL"));
        return eSIR_FAILURE;
    }
    smeSessionId = psessionEntry->smeSessionId;
    /*
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    vos_mem_set( (tANI_U8*)&tdlsDisReq,
                  sizeof( tDot11fTDLSDisReq ), 0 );

    /*
     * setup Fixed fields,
     */
    tdlsDisReq.Category.category = SIR_MAC_ACTION_TDLS ;
    tdlsDisReq.Action.action     = SIR_MAC_TDLS_DIS_REQ ;
    tdlsDisReq.DialogToken.token = dialog ;


    size = sizeof(tSirMacAddr) ;

    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsDisReq.LinkIdentifier,
                                                 peer_mac, TDLS_INITIATOR) ;

    /*
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSDisReqSize( pMac, &tdlsDisReq, &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for a discovery Request (0x%08x)."),
               status );
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fTDLSDisReq );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for a discovery Request (0x%08x)."),
               status );
    }

    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */


    nBytes = nPayload + ((IS_QOS_ENABLED(psessionEntry))
                              ? sizeof(tSirMacDataHdr3a) : sizeof(tSirMacMgmtHdr))
                      + sizeof( eth_890d_header )
                      + PAYLOAD_TYPE_TDLS_SIZE ;

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    /* IOT issue with some AP : some AP doesn't like the data packet size < minimum 802.3 frame length (64)
       Hence AP itself padding some bytes, which caused teardown packet is dropped at
       receiver side. To avoid such IOT issue, we added some extra bytes to meet data frame size >= 64
     */
    if (nPayload + PAYLOAD_TYPE_TDLS_SIZE < MIN_IEEE_8023_SIZE)
    {
        padLen = MIN_IEEE_8023_SIZE - (nPayload + PAYLOAD_TYPE_TDLS_SIZE ) ;

        /* if padLen is less than minimum vendorSpecific (5), pad up to 5 */
        if (padLen < MIN_VENDOR_SPECIFIC_IE_SIZE)
            padLen = MIN_VENDOR_SPECIFIC_IE_SIZE;

        nBytes += padLen;
    }
#endif

    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Discovery Request."),
               nBytes );
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set( pFrame, nBytes, 0 );

    /*
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * request frame
     */

    /* fill out the buffer descriptor */

    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame,
           LINK_IDEN_ADDR_OFFSET(tdlsDisReq), TDLS_LINK_AP, TDLS_INITIATOR, TID_AC_VI, psessionEntry) ;

    status = dot11fPackTDLSDisReq( pMac, &tdlsDisReq, pFrame
                               + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to pack a TDLS discovery req (0x%08x)."),
                           status );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while packing TDLS Discovery Request (0x%08x)."),
               status );
    }

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    if (padLen != 0)
    {
        /* QCOM VENDOR OUI = { 0x00, 0xA0, 0xC6, type = 0x0000 }; */
        tANI_U8 *padVendorSpecific = pFrame + header_offset + nPayload;
        /* make QCOM_VENDOR_OUI, and type = 0x0000, and all the payload to be zero */
        padVendorSpecific[0] = 221;
        padVendorSpecific[1] = padLen - 2;
        padVendorSpecific[2] = 0x00;
        padVendorSpecific[3] = 0xA0;
        padVendorSpecific[4] = 0xC6;

        limLog(pMac, LOG1, FL("Padding Vendor Specific Ie Len = %d"), padLen);

        /* padding zero if more than 5 bytes are required */
        if (padLen > MIN_VENDOR_SPECIFIC_IE_SIZE)
            vos_mem_set( pFrame + header_offset + nPayload + MIN_VENDOR_SPECIFIC_IE_SIZE,
                         padLen - MIN_VENDOR_SPECIFIC_IE_SIZE, 0);
    }
#endif

    limLog(pMac, LOG1,
           FL("[TDLS] action %d (%s) -AP-> OTA peer="MAC_ADDRESS_STR),
           SIR_MAC_TDLS_DIS_REQ,
           limTraceTdlsActionString(SIR_MAC_TDLS_DIS_REQ),
           MAC_ADDR_ARRAY(peer_mac));

    pMac->lim.tdls_frm_session_id = psessionEntry->peSessionId;
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            lim_mgmt_tdls_tx_complete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME,
                            smeSessionId, false );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.tdls_frm_session_id = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Discovery Request frame"));
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;

}




/*
 * This static function is consistent with any kind of TDLS management
 * frames we are sending. Currently it is being used by limSendTdlsDisRspFrame,
 * limSendTdlsLinkSetupReqFrame and limSendTdlsSetupRspFrame
 */
static void PopulateDot11fTdlsHtVhtCap(tpAniSirGlobal pMac, uint32 selfDot11Mode,
                                        tDot11fIEHTCaps *htCap, tDot11fIEVHTCaps *vhtCap,
                                        tpPESession psessionEntry)
{
    uint8_t nss;
    uint32_t val;

    if (IS_5G_CH(psessionEntry->currentOperChannel))
        nss = pMac->vdev_type_nss_5g.tdls;
    else
        nss = pMac->vdev_type_nss_2g.tdls;

    nss = VOS_MIN(nss, pMac->user_configured_nss);
    if (IS_DOT11_MODE_HT(selfDot11Mode))
    {
        /* Include HT Capability IE */
        PopulateDot11fHTCaps( pMac, NULL, htCap );
        val = SIZE_OF_SUPPORTED_MCS_SET;
        wlan_cfgGetStr(pMac, WNI_CFG_SUPPORTED_MCS_SET,
                 &htCap->supportedMCSSet[0], &val);
        if (NSS_1x1_MODE == nss)
                htCap->supportedMCSSet[1] = 0;
        /*
         * Advertize ht capability and max supported channel
         * bandwidth when populating HT IE in TDLS Setup Request/
         * Setup Response/Setup Confirmation frames.
         * 11.21.6.2 Setting up a 40 MHz direct link: A 40 MHz off-channel
         * direct link may be started if both TDLS peer STAs indicated
         * 40 MHz support in the Supported Channel Width Set field of the
         * HT Capabilities element (which is included in the TDLS Setup Request
         * frame and the TDLS Setup Response frame). Switching to a 40 MHz
         * off-channel direct link is achieved by including the following
         * information in the TDLS Channel Switch Request
         * 11.21.1 General: The channel width of the TDLS direct link on the
         * base channel shall not exceed the channel width of the BSS to which
         * the TDLS peer STAs are associated.
         */
        htCap->supportedChannelWidthSet = 1;
    }
    else
    {
        htCap->present = 0;
    }
    limLog(pMac, LOG1, FL("HT present = %hu, Chan Width = %hu"),
           htCap->present, htCap->supportedChannelWidthSet);
#ifdef WLAN_FEATURE_11AC
    if (((psessionEntry->currentOperChannel <= SIR_11B_CHANNEL_END) &&
          pMac->roam.configParam.enableVhtFor24GHz) ||
         (psessionEntry->currentOperChannel >= SIR_11B_CHANNEL_END))
    {
        if (IS_DOT11_MODE_VHT(selfDot11Mode) &&
            IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
        {
            /* Include VHT Capability IE */
            PopulateDot11fVHTCaps(pMac, psessionEntry, vhtCap);
            vhtCap->suBeamformeeCap = 0;
            vhtCap->suBeamFormerCap = 0;
            vhtCap->muBeamformeeCap = 0;
            vhtCap->muBeamformerCap = 0;

            wlan_cfgGetInt(pMac, WNI_CFG_VHT_RX_MCS_MAP, &val);
            vhtCap->rxMCSMap = val;
            wlan_cfgGetInt(pMac, WNI_CFG_VHT_RX_HIGHEST_SUPPORTED_DATA_RATE,
                            &val);
            vhtCap->rxHighSupDataRate = val;
            wlan_cfgGetInt(pMac, WNI_CFG_VHT_TX_MCS_MAP, &val);
            vhtCap->txMCSMap = val;
            wlan_cfgGetInt(pMac, WNI_CFG_VHT_TX_HIGHEST_SUPPORTED_DATA_RATE,
                            &val);
            vhtCap->txSupDataRate = val;
            if (nss == NSS_1x1_MODE) {
                    vhtCap->txMCSMap |= DISABLE_NSS2_MCS;
                    vhtCap->rxMCSMap |= DISABLE_NSS2_MCS;
                    vhtCap->txSupDataRate =
                            VHT_TX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
                    vhtCap->rxHighSupDataRate =
                            VHT_RX_HIGHEST_SUPPORTED_DATA_RATE_1_1;
            }
        }
        else
        {
            vhtCap->present = 0;
        }
    }
    else
    {
        /* Vht Disable from ini in 2.4 GHz */
        vhtCap->present = 0;
    }
    limLog(pMac, LOG1, FL("VHT present = %hu, Chan Width = %hu"),
           vhtCap->present, vhtCap->supportedChannelWidthSet);
#endif
}

/*
 * Send TDLS discovery response frame on direct link.
 */

static tSirRetStatus limSendTdlsDisRspFrame(tpAniSirGlobal pMac,
                     tSirMacAddr peerMac, tANI_U8 dialog,
                     tpPESession psessionEntry, tANI_U8 *addIe,
                     tANI_U16 addIeLen)
{
    tDot11fTDLSDisRsp   tdlsDisRsp ;
    tANI_U16            caps = 0 ;
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
    uint32              selfDot11Mode;
//  Placeholder to support different channel bonding mode of TDLS than AP.
//  Today, WNI_CFG_CHANNEL_BONDING_MODE will be overwritten when connecting to AP
//  To support this feature, we need to introduce WNI_CFG_TDLS_CHANNEL_BONDING_MODE
//  As of now, we hardcoded to max channel bonding of dot11Mode (i.e HT80 for 11ac/HT40 for 11n)
//  uint32 tdlsChannelBondingMode;
    tANI_U8             smeSessionId = 0;

    if (NULL == psessionEntry)
    {
        limLog(pMac, LOGE, FL("psessionEntry is NULL"));
        return eSIR_FAILURE;
    }
    smeSessionId = psessionEntry->smeSessionId;

    /*
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    vos_mem_set( ( tANI_U8* )&tdlsDisRsp,
                                      sizeof( tDot11fTDLSDisRsp ), 0 );

    /*
     * setup Fixed fields,
     */
    tdlsDisRsp.Category.category = SIR_MAC_ACTION_PUBLIC_USAGE;
    tdlsDisRsp.Action.action     = SIR_MAC_TDLS_DIS_RSP ;
    tdlsDisRsp.DialogToken.token = dialog ;

    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsDisRsp.LinkIdentifier,
                                           peerMac, TDLS_RESPONDER) ;

    if (cfgGetCapabilityInfo(pMac, &caps, psessionEntry) != eSIR_SUCCESS)
    {
        /*
         * Could not get Capabilities value
         * from CFG. Log error.
         */
         limLog(pMac, LOGP, FL("could not retrieve Capabilities value"));
    }
    swapBitField16(caps, ( tANI_U16* )&tdlsDisRsp.Capabilities );

    /* populate supported rate and ext supported rate IE */
    if (eSIR_FAILURE == populate_dot11f_rates_tdls(pMac, &tdlsDisRsp.SuppRates,
                               &tdlsDisRsp.ExtSuppRates,
                               psessionEntry->currentOperChannel))
        limLog(pMac, LOGE, FL("could not populate supported data rates"));


    /* Populate extended capability IE */
    PopulateDot11fTdlsExtCapability(pMac, psessionEntry, &tdlsDisRsp.ExtCap);

    wlan_cfgGetInt(pMac,WNI_CFG_DOT11_MODE,&selfDot11Mode);

    /* Populate HT/VHT Capabilities */
    PopulateDot11fTdlsHtVhtCap( pMac, selfDot11Mode, &tdlsDisRsp.HTCaps,
                               &tdlsDisRsp.VHTCaps, psessionEntry );

    /* Populate TDLS offchannel param only if offchannel is enabled
     * and TDLS Channel Switching is not prohibited by AP in ExtCap
     * IE in assoc/re-assoc response.
     */
    if ((1 == pMac->lim.gLimTDLSOffChannelEnabled) &&
        (!psessionEntry->tdls_chan_swit_prohibited))
    {
        PopulateDot11fTdlsOffchannelParams( pMac, psessionEntry,
                                            &tdlsDisRsp.SuppChannels,
                                            &tdlsDisRsp.SuppOperatingClasses);
        if ( pMac->roam.configParam.bandCapability != eCSR_BAND_24)
        {
            tdlsDisRsp.HT2040BSSCoexistence.present = 1;
            tdlsDisRsp.HT2040BSSCoexistence.infoRequest = 1;
        }
    }
    else
    {
        limLog(pMac, LOG1,
               FL("TDLS offchan not enabled, or channel switch prohibited by AP, gLimTDLSOffChannelEnabled (%d), tdls_chan_swit_prohibited (%d)"),
               pMac->lim.gLimTDLSOffChannelEnabled,
               psessionEntry->tdls_chan_swit_prohibited);
    }

    /*
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSDisRspSize( pMac, &tdlsDisRsp, &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for a Discovery Response (0x%08x)."),
               status );
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for a Discovery Response (0x%08x)."),
               status);
    }

    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */


    nBytes = nPayload + sizeof( tSirMacMgmtHdr ) + addIeLen;

    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Discovery Response."),
               nBytes );
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set( pFrame, nBytes, 0 );

    /*
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * response frame
     */

    /* Make public Action Frame */

    limPopulateMacHeader( pMac, pFrame, SIR_MAC_MGMT_FRAME,
              SIR_MAC_MGMT_ACTION, peerMac, psessionEntry->selfMacAddr);

    {
        tpSirMacMgmtHdr     pMacHdr;
        pMacHdr = ( tpSirMacMgmtHdr ) pFrame;
        pMacHdr->fc.toDS    = ANI_TXDIR_IBSS;
        pMacHdr->fc.powerMgmt = 0 ;
        sirCopyMacAddr(pMacHdr->bssId,psessionEntry->bssId);
    }

    status = dot11fPackTDLSDisRsp( pMac, &tdlsDisRsp, pFrame +
                                              sizeof( tSirMacMgmtHdr ),
                                                  nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to pack a TDLS Discovery Response (0x%08x)."),
               status );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while packing TDLS Discovery Response (0x%08x)."),
               status );
    }
    if (0 != addIeLen)
    {
        limLog(pMac, LOG1, FL("Copy Additional Ie Len = %d"), addIeLen );
        vos_mem_copy(pFrame + sizeof(tSirMacMgmtHdr) + nPayload,
                     addIe,
                     addIeLen);
    }
    limLog(pMac, LOG1,
           FL("[TDLS] action %d (%s) -DIRECT-> OTA peer="MAC_ADDRESS_STR),
           SIR_MAC_TDLS_DIS_RSP,
           limTraceTdlsActionString(SIR_MAC_TDLS_DIS_RSP),
           MAC_ADDR_ARRAY(peerMac));


    pMac->lim.tdls_frm_session_id = psessionEntry->peSessionId;
    /*
     * Transmit Discovery response and watch if this is delivered to
     * peer STA.
     */
    /* In CLD 2.0, pass Discovery Response as mgmt frame so that
     * wma does not do header conversion to 802.3 before calling tx/rx
     * routine and subsequenly target also sends frame as is OTA
     */
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_MGMT,
                            ANI_TXDIR_IBSS,
                            0,
                            limTxComplete, pFrame,
                            lim_mgmt_tdls_tx_complete,
                            HAL_USE_SELF_STA_REQUESTED_MASK, smeSessionId, false );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.tdls_frm_session_id = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Discovery Response frame!"));
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;

}

/*
 * This static function is currently used by limSendTdlsLinkSetupReqFrame and
 * limSendTdlsSetupRspFrame to populate the AID if device is 11ac capable.
 */
static void PopulateDotfTdlsVhtAID(tpAniSirGlobal pMac, uint32 selfDot11Mode,
                                   tSirMacAddr peerMac, tDot11fIEAID *Aid,
                                   tpPESession psessionEntry)
{
    if (((psessionEntry->currentOperChannel <= SIR_11B_CHANNEL_END) &&
          pMac->roam.configParam.enableVhtFor24GHz) ||
         (psessionEntry->currentOperChannel >= SIR_11B_CHANNEL_END))
    {
        if (IS_DOT11_MODE_VHT(selfDot11Mode) &&
            IS_FEATURE_SUPPORTED_BY_FW(DOT11AC))
        {

            tANI_U16 aid;
            tpDphHashNode       pStaDs;

            pStaDs = dphLookupHashEntry(pMac, peerMac, &aid, &psessionEntry->dph.dphHashTable);
            if (NULL != pStaDs)
            {
                 Aid->present = 1;
                 Aid->assocId = aid | LIM_AID_MASK; // set bit 14 and 15 1's
            }
            else
            {
                Aid->present = 0;
                limLog(pMac, LOGE, FL("pStaDs is NULL for " MAC_ADDRESS_STR),
                                   MAC_ADDR_ARRAY(peerMac));
            }
        }
    }
    else
    {
        Aid->present = 0;
        limLog(pMac, LOGW, FL("Vht not enable from ini for 2.4GHz."));
    }
}

/*
 * TDLS setup Request frame on AP link
 */

tSirRetStatus limSendTdlsLinkSetupReqFrame(tpAniSirGlobal pMac,
            tSirMacAddr peerMac, tANI_U8 dialog, tpPESession psessionEntry,
            tANI_U8 *addIe, tANI_U16 addIeLen)
{
    tDot11fTDLSSetupReq    tdlsSetupReq ;
    tANI_U16            caps = 0 ;
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
    uint32              selfDot11Mode;
    tANI_U8             smeSessionId = 0;
//  Placeholder to support different channel bonding mode of TDLS than AP.
//  Today, WNI_CFG_CHANNEL_BONDING_MODE will be overwritten when connecting to AP
//  To support this feature, we need to introduce WNI_CFG_TDLS_CHANNEL_BONDING_MODE
//  As of now, we hardcoded to max channel bonding of dot11Mode (i.e HT80 for 11ac/HT40 for 11n)
//  uint32 tdlsChannelBondingMode;

    /*
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    smeSessionId = psessionEntry->smeSessionId;

    vos_mem_set(( tANI_U8* )&tdlsSetupReq, sizeof( tDot11fTDLSSetupReq ), 0);
    tdlsSetupReq.Category.category = SIR_MAC_ACTION_TDLS ;
    tdlsSetupReq.Action.action     = SIR_MAC_TDLS_SETUP_REQ ;
    tdlsSetupReq.DialogToken.token = dialog ;


    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsSetupReq.LinkIdentifier,
                                                    peerMac, TDLS_INITIATOR) ;

    if (cfgGetCapabilityInfo(pMac, &caps, psessionEntry) != eSIR_SUCCESS)
    {
        /*
         * Could not get Capabilities value
         * from CFG. Log error.
         */
         limLog(pMac, LOGE, FL("could not retrieve Capabilities value"));
    }
    swapBitField16(caps, ( tANI_U16* )&tdlsSetupReq.Capabilities );

    limLog(pMac, LOG1, FL("Sending operating channel %d and dotl11mode %d\n"),
           psessionEntry->currentOperChannel, psessionEntry->dot11mode);

    /* populate supported rate and ext supported rate IE */
    populate_dot11f_rates_tdls(pMac, &tdlsSetupReq.SuppRates,
                               &tdlsSetupReq.ExtSuppRates,
                               psessionEntry->currentOperChannel);

    /* Populate extended supported rates */
    PopulateDot11fTdlsExtCapability(pMac, psessionEntry, &tdlsSetupReq.ExtCap);

    if (1 == pMac->lim.gLimTDLSWmmMode)
    {
        tANI_U32  val = 0;

        limLog(pMac, LOG1, FL("populate WMM IE in Setup Request Frame"));
        /* include WMM IE */
        tdlsSetupReq.WMMInfoStation.version = SIR_MAC_OUI_VERSION_1;
        tdlsSetupReq.WMMInfoStation.acvo_uapsd =
                     (pMac->lim.gLimTDLSUapsdMask & 0x01);
        tdlsSetupReq.WMMInfoStation.acvi_uapsd =
                     ((pMac->lim.gLimTDLSUapsdMask & 0x02) >> 1);
        tdlsSetupReq.WMMInfoStation.acbk_uapsd =
                     ((pMac->lim.gLimTDLSUapsdMask & 0x04) >> 2);
        tdlsSetupReq.WMMInfoStation.acbe_uapsd =
                     ((pMac->lim.gLimTDLSUapsdMask & 0x08) >> 3);

        if(wlan_cfgGetInt(pMac, WNI_CFG_MAX_SP_LENGTH, &val) != eSIR_SUCCESS)
            limLog(pMac, LOGE, FL("could not retrieve Max SP Length"));

        tdlsSetupReq.WMMInfoStation.max_sp_length = (tANI_U8)val;
        tdlsSetupReq.WMMInfoStation.present = 1;
    }
    else
    {
        /*
         * TODO: we need to see if we have to support conditions where we have
         * EDCA parameter info element is needed a) if we need different QOS
         * parameters for off channel operations or QOS is not supported on
         * AP link and we wanted to QOS on direct link.
         */
        /* Populate QOS info, needed for Peer U-APSD session */
        /* TODO: Now hardcoded, because PopulateDot11fQOSCapsStation() depends
           on AP's capability, and TDLS doesn't want to depend on AP's capability */
        limLog(pMac, LOG1, FL("populate QOS IE in Setup Request Frame"));
        tdlsSetupReq.QOSCapsStation.present = 1;
        tdlsSetupReq.QOSCapsStation.max_sp_length = 0;
        tdlsSetupReq.QOSCapsStation.qack = 0;
        tdlsSetupReq.QOSCapsStation.acbe_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x08) >> 3);
        tdlsSetupReq.QOSCapsStation.acbk_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x04) >> 2);
        tdlsSetupReq.QOSCapsStation.acvi_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x02) >> 1);
        tdlsSetupReq.QOSCapsStation.acvo_uapsd = (pMac->lim.gLimTDLSUapsdMask & 0x01);
    }

    /*
     * we will always try to init TDLS link with 11n capabilities
     * let TDLS setup response to come, and we will set our caps based
     * of peer caps
     */

    wlan_cfgGetInt(pMac,WNI_CFG_DOT11_MODE,&selfDot11Mode);

    /* Populate HT/VHT Capabilities */
    PopulateDot11fTdlsHtVhtCap( pMac, selfDot11Mode, &tdlsSetupReq.HTCaps,
                               &tdlsSetupReq.VHTCaps, psessionEntry );

    /* Populate AID */
    PopulateDotfTdlsVhtAID( pMac, selfDot11Mode, peerMac,
                            &tdlsSetupReq.AID, psessionEntry );

    /* Populate TDLS offchannel param only if offchannel is enabled
     * and TDLS Channel Switching is not prohibited by AP in ExtCap
     * IE in assoc/re-assoc response.
     */
    if ((1 == pMac->lim.gLimTDLSOffChannelEnabled) &&
        (!psessionEntry->tdls_chan_swit_prohibited))
    {
        PopulateDot11fTdlsOffchannelParams( pMac, psessionEntry,
                                            &tdlsSetupReq.SuppChannels,
                                            &tdlsSetupReq.SuppOperatingClasses);
        if ( pMac->roam.configParam.bandCapability != eCSR_BAND_24)
        {
            tdlsSetupReq.HT2040BSSCoexistence.present = 1;
            tdlsSetupReq.HT2040BSSCoexistence.infoRequest = 1;
        }
    }
    else
    {
        limLog(pMac, LOG1,
               FL("TDLS offchan not enabled, or channel switch prohibited by AP, gLimTDLSOffChannelEnabled (%d), tdls_chan_swit_prohibited (%d)"),
               pMac->lim.gLimTDLSOffChannelEnabled,
               psessionEntry->tdls_chan_swit_prohibited);
    }

    /*
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSSetupReqSize( pMac, &tdlsSetupReq,
                                                              &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for a Setup Request (0x%08x)."),
               status);
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for a Setup Request (0x%08x)."),
               status);
    }


    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */


    nBytes = nPayload + ((IS_QOS_ENABLED(psessionEntry))
                              ? sizeof(tSirMacDataHdr3a) : sizeof(tSirMacMgmtHdr))
                      + sizeof( eth_890d_header )
                      + PAYLOAD_TYPE_TDLS_SIZE
                      + addIeLen;

    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Setup Request."),
               nBytes );
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set( pFrame, nBytes, 0);

    /*
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * request frame
     */

    /* fill out the buffer descriptor */

    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame,
                     LINK_IDEN_ADDR_OFFSET(tdlsSetupReq), TDLS_LINK_AP,
                     TDLS_INITIATOR, TID_AC_VI, psessionEntry) ;

    limLog(pMac, LOGW, FL("SupportedChnlWidth %x rxMCSMap %x rxMCSMap %x txSupDataRate %x"),
                       tdlsSetupReq.VHTCaps.supportedChannelWidthSet,
                       tdlsSetupReq.VHTCaps.rxMCSMap,
                       tdlsSetupReq.VHTCaps.txMCSMap,
                       tdlsSetupReq.VHTCaps.txSupDataRate);

    status = dot11fPackTDLSSetupReq( pMac, &tdlsSetupReq, pFrame
                               + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to pack a TDLS Setup Request (0x%08x)."),
               status);
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while packing TDLS Setup Request (0x%08x)."),
               status);
    }

    //Copy the additional IE.
    //TODO : addIe is added at the end of the frame. This means it doesnt
    //follow the order. This should be ok, but we should consider changing this
    //if there is any IOT issue.
    if( addIeLen != 0 )
    {
        limLog(pMac, LOG1, FL("Copy Additional Ie Len = %d"),
                           addIeLen );
        vos_mem_copy( pFrame + header_offset + nPayload, addIe, addIeLen );
    }

    limLog(pMac, LOG1, FL("[TDLS] action %d (%s) -AP-> OTA peer="MAC_ADDRESS_STR),
                       SIR_MAC_TDLS_SETUP_REQ,
                       limTraceTdlsActionString(SIR_MAC_TDLS_SETUP_REQ),
                       MAC_ADDR_ARRAY(peerMac));

    pMac->lim.tdls_frm_session_id = psessionEntry->peSessionId;
#if defined(CONFIG_HL_SUPPORT)
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            lim_mgmt_tdls_tx_complete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME,
                            smeSessionId, true );
#else
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            lim_mgmt_tdls_tx_complete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME,
                            smeSessionId, false );
#endif
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.tdls_frm_session_id = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Setup Request frame!"));
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;

}
/*
 * Send TDLS Teardown frame on Direct link or AP link, depends on reason code.
 */

tSirRetStatus limSendTdlsTeardownFrame(tpAniSirGlobal pMac,
            tSirMacAddr peerMac, tANI_U16 reason, tANI_U8 responder, tpPESession psessionEntry,
            tANI_U8 *addIe, tANI_U16 addIeLen)
{
    tDot11fTDLSTeardown teardown ;
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    tANI_U32            padLen = 0;
#endif
    tANI_U8             smeSessionId = 0;
    tpDphHashNode       pStaDs = NULL;
    tANI_U16            aid = 0;
    tANI_U8             qosMode = 0;
    tANI_U8             tdlsLinkType = 0;

    if (NULL == psessionEntry)
    {
        limLog(pMac, LOGE, FL("psessionEntry is NULL"));
        return eSIR_FAILURE;
    }
    smeSessionId = psessionEntry->smeSessionId;
    /*
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    vos_mem_set( ( tANI_U8* )&teardown, sizeof( tDot11fTDLSTeardown ), 0 );
    teardown.Category.category = SIR_MAC_ACTION_TDLS ;
    teardown.Action.action     = SIR_MAC_TDLS_TEARDOWN ;
    teardown.Reason.code       = reason ;

    PopulateDot11fLinkIden( pMac, psessionEntry, &teardown.LinkIdentifier,
                                                peerMac, (responder == TRUE) ? TDLS_RESPONDER : TDLS_INITIATOR) ;
    /*
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSTeardownSize( pMac, &teardown, &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for Teardown frame (0x%08x)."),
               status);
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for Teardown frame (0x%08x)."),
               status);
    }
    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */
    pStaDs = dphLookupHashEntry(pMac, peerMac, &aid,
                                      &psessionEntry->dph.dphHashTable);
    if (pStaDs)
    {
        qosMode = pStaDs->qosMode;
    }
    tdlsLinkType = (reason == eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE)
                              ? TDLS_LINK_AP : TDLS_LINK_DIRECT;
    nBytes = nPayload + (((IS_QOS_ENABLED(psessionEntry) &&
                          (tdlsLinkType == TDLS_LINK_AP)) ||
                          ((tdlsLinkType == TDLS_LINK_DIRECT) && qosMode))
                              ? sizeof(tSirMacDataHdr3a) : sizeof(tSirMacMgmtHdr))
                      + sizeof( eth_890d_header )
                      + PAYLOAD_TYPE_TDLS_SIZE
                      + addIeLen;

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    /* IOT issue with some AP : some AP doesn't like the data packet size < minimum 802.3 frame length (64)
       Hence AP itself padding some bytes, which caused teardown packet is dropped at
       receiver side. To avoid such IOT issue, we added some extra bytes to meet data frame size >= 64
     */
    if (nPayload + PAYLOAD_TYPE_TDLS_SIZE < MIN_IEEE_8023_SIZE)
    {
        padLen = MIN_IEEE_8023_SIZE - (nPayload + PAYLOAD_TYPE_TDLS_SIZE ) ;

        /* if padLen is less than minimum vendorSpecific (5), pad up to 5 */
        if (padLen < MIN_VENDOR_SPECIFIC_IE_SIZE)
            padLen = MIN_VENDOR_SPECIFIC_IE_SIZE;

        nBytes += padLen;
    }
#endif

    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Teardown Frame."),
               nBytes);
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set( pFrame, nBytes, 0 );

    /*
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * request frame
     */
    limLog(pMac, LOGE, FL("Reason of TDLS Teardown: %d"), reason);
    /* fill out the buffer descriptor */
    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame,
                          LINK_IDEN_ADDR_OFFSET(teardown),
                          (reason == eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE)
                              ? TDLS_LINK_AP : TDLS_LINK_DIRECT,
                          (responder == TRUE) ? TDLS_RESPONDER : TDLS_INITIATOR,
                              TID_AC_VI, psessionEntry);

    status = dot11fPackTDLSTeardown( pMac, &teardown, pFrame
                               + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE, FL("Failed to pack a TDLS Teardown frame (0x%08x)."),
                           status );
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while packing TDLS Teardown frame (0x%08x)."),
               status);
    }

    if( addIeLen != 0 )
    {
       limLog(pMac, LOG1, FL("Copy Additional Ie Len = %d"), addIeLen);
       vos_mem_copy( pFrame + header_offset + nPayload, addIe, addIeLen );
    }

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    if (padLen != 0)
    {
        /* QCOM VENDOR OUI = { 0x00, 0xA0, 0xC6, type = 0x0000 }; */
        tANI_U8 *padVendorSpecific = pFrame + header_offset + nPayload + addIeLen;
        /* make QCOM_VENDOR_OUI, and type = 0x0000, and all the payload to be zero */
        padVendorSpecific[0] = 221;
        padVendorSpecific[1] = padLen - 2;
        padVendorSpecific[2] = 0x00;
        padVendorSpecific[3] = 0xA0;
        padVendorSpecific[4] = 0xC6;

        limLog(pMac, LOG1, FL("Padding Vendor Specific Ie Len = %d"), padLen);

        /* padding zero if more than 5 bytes are required */
        if (padLen > MIN_VENDOR_SPECIFIC_IE_SIZE)
            vos_mem_set( pFrame + header_offset + nPayload + addIeLen + MIN_VENDOR_SPECIFIC_IE_SIZE,
                         padLen - MIN_VENDOR_SPECIFIC_IE_SIZE, 0);
    }
#endif
    limLog(pMac, LOG1, FL("[TDLS] action %d (%s) -%s-> OTA peer="MAC_ADDRESS_STR),
                       SIR_MAC_TDLS_TEARDOWN,
                       limTraceTdlsActionString(SIR_MAC_TDLS_TEARDOWN),
                       ((reason == eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE) ?
                       "AP": "DIRECT"),
                       MAC_ADDR_ARRAY(peerMac));

    pMac->lim.tdls_frm_session_id = psessionEntry->peSessionId;
#if defined(CONFIG_HL_SUPPORT)
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            lim_mgmt_tdls_tx_complete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME,
                            smeSessionId,
                            (reason == eSIR_MAC_TDLS_TEARDOWN_PEER_UNREACHABLE) ? true : false );
#else
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            lim_mgmt_tdls_tx_complete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME,
                            smeSessionId,
                            false );
#endif
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.tdls_frm_session_id = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Teardown frame"));
        return eSIR_FAILURE;

    }
    return eSIR_SUCCESS;

}

/*
 * Send Setup RSP frame on AP link.
 */
static tSirRetStatus limSendTdlsSetupRspFrame(tpAniSirGlobal pMac,
                    tSirMacAddr peerMac, tANI_U8 dialog, tpPESession psessionEntry,
                    etdlsLinkSetupStatus setupStatus, tANI_U8 *addIe, tANI_U16 addIeLen )
{
    tDot11fTDLSSetupRsp  tdlsSetupRsp ;
    tANI_U32            status = 0 ;
    tANI_U16            caps = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
    uint32             selfDot11Mode;
//  Placeholder to support different channel bonding mode of TDLS than AP.
//  Today, WNI_CFG_CHANNEL_BONDING_MODE will be overwritten when connecting to AP
//  To support this feature, we need to introduce WNI_CFG_TDLS_CHANNEL_BONDING_MODE
//  As of now, we hardcoded to max channel bonding of dot11Mode (i.e HT80 for 11ac/HT40 for 11n)
//  uint32 tdlsChannelBondingMode;
    tANI_U8             smeSessionId = 0;

    if (NULL == psessionEntry)
    {
        limLog(pMac, LOGE, FL("psessionEntry is NULL"));
        return eSIR_FAILURE;
    }
       smeSessionId = psessionEntry->smeSessionId;

    /*
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    vos_mem_set( ( tANI_U8* )&tdlsSetupRsp, sizeof( tDot11fTDLSSetupRsp ),0 );

    /*
     * setup Fixed fields,
     */
    tdlsSetupRsp.Category.category = SIR_MAC_ACTION_TDLS;
    tdlsSetupRsp.Action.action     = SIR_MAC_TDLS_SETUP_RSP ;
    tdlsSetupRsp.DialogToken.token = dialog;

    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsSetupRsp.LinkIdentifier,
                 peerMac, TDLS_RESPONDER) ;

    if (cfgGetCapabilityInfo(pMac, &caps, psessionEntry) != eSIR_SUCCESS)
    {
        /*
         * Could not get Capabilities value
         * from CFG. Log error.
         */
         limLog(pMac, LOGE, FL("could not retrieve Capabilities value"));
    }
    swapBitField16(caps, ( tANI_U16* )&tdlsSetupRsp.Capabilities );

    /* populate supported rate and ext supported rate IE */
    populate_dot11f_rates_tdls(pMac, &tdlsSetupRsp.SuppRates,
                               &tdlsSetupRsp.ExtSuppRates,
                               psessionEntry->currentOperChannel);

    /* Populate extended supported rates */
    PopulateDot11fTdlsExtCapability(pMac, psessionEntry, &tdlsSetupRsp.ExtCap);

    if (1 == pMac->lim.gLimTDLSWmmMode)
    {
        tANI_U32  val = 0;

        limLog(pMac, LOG1, FL("populate WMM IE in Setup Response frame"));
        /* include WMM IE */
        tdlsSetupRsp.WMMInfoStation.version = SIR_MAC_OUI_VERSION_1;
        tdlsSetupRsp.WMMInfoStation.acvo_uapsd =
                     (pMac->lim.gLimTDLSUapsdMask & 0x01);
        tdlsSetupRsp.WMMInfoStation.acvi_uapsd =
                     ((pMac->lim.gLimTDLSUapsdMask & 0x02) >> 1);
        tdlsSetupRsp.WMMInfoStation.acbk_uapsd =
                     ((pMac->lim.gLimTDLSUapsdMask & 0x04) >> 2);
        tdlsSetupRsp.WMMInfoStation.acbe_uapsd =
                     ((pMac->lim.gLimTDLSUapsdMask & 0x08) >> 3);

        if(wlan_cfgGetInt(pMac, WNI_CFG_MAX_SP_LENGTH, &val) != eSIR_SUCCESS)
            limLog(pMac, LOGE, FL("could not retrieve Max SP Length"));

        tdlsSetupRsp.WMMInfoStation.max_sp_length = (tANI_U8)val;
        tdlsSetupRsp.WMMInfoStation.present = 1;
    }
    else
    {
        /*
         * TODO: we need to see if we have to support conditions where we have
         * EDCA parameter info element is needed a) if we need different QOS
         * parameters for off channel operations or QOS is not supported on
         * AP link and we wanted to QOS on direct link.
         */
        /* Populate QOS info, needed for Peer U-APSD session */
        /* TODO: Now hardcoded, because PopulateDot11fQOSCapsStation() depends on AP's capability, and
         TDLS doesn't want to depend on AP's capability */
        limLog(pMac, LOG1, FL("populate QOS IE in Setup Response frame"));
        tdlsSetupRsp.QOSCapsStation.present = 1;
        tdlsSetupRsp.QOSCapsStation.max_sp_length = 0;
        tdlsSetupRsp.QOSCapsStation.qack = 0;
        tdlsSetupRsp.QOSCapsStation.acbe_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x08) >> 3);
        tdlsSetupRsp.QOSCapsStation.acbk_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x04) >> 2);
        tdlsSetupRsp.QOSCapsStation.acvi_uapsd = ((pMac->lim.gLimTDLSUapsdMask & 0x02) >> 1);
        tdlsSetupRsp.QOSCapsStation.acvo_uapsd = (pMac->lim.gLimTDLSUapsdMask & 0x01);
    }

    wlan_cfgGetInt(pMac,WNI_CFG_DOT11_MODE,&selfDot11Mode);

    /* Populate HT/VHT Capabilities */
    PopulateDot11fTdlsHtVhtCap( pMac, selfDot11Mode, &tdlsSetupRsp.HTCaps,
                                &tdlsSetupRsp.VHTCaps, psessionEntry );

    /* Populate AID */
    PopulateDotfTdlsVhtAID( pMac, selfDot11Mode, peerMac,
                            &tdlsSetupRsp.AID, psessionEntry );

    /* Populate TDLS offchannel param only if offchannel is enabled
     * and TDLS Channel Switching is not prohibited by AP in ExtCap
     * IE in assoc/re-assoc response.
     */
    if ((1 == pMac->lim.gLimTDLSOffChannelEnabled) &&
        (!psessionEntry->tdls_chan_swit_prohibited))
    {
        PopulateDot11fTdlsOffchannelParams( pMac, psessionEntry,
                                            &tdlsSetupRsp.SuppChannels,
                                            &tdlsSetupRsp.SuppOperatingClasses);
        if ( pMac->roam.configParam.bandCapability != eCSR_BAND_24)
        {
            tdlsSetupRsp.HT2040BSSCoexistence.present = 1;
            tdlsSetupRsp.HT2040BSSCoexistence.infoRequest = 1;
        }
    }
    else
    {
        limLog(pMac, LOG1,
               FL("TDLS offchan not enabled, or channel switch prohibited by AP, gLimTDLSOffChannelEnabled (%d), tdls_chan_swit_prohibited (%d)"),
               pMac->lim.gLimTDLSOffChannelEnabled,
               psessionEntry->tdls_chan_swit_prohibited);
    }

    tdlsSetupRsp.Status.status = setupStatus ;

    /*
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSSetupRspSize( pMac, &tdlsSetupRsp,
                                                     &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for a Setup Response (0x%08x)."),
               status);
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for Setup Response (0x%08x)."),
               status);
    }

    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */


    nBytes = nPayload + ((IS_QOS_ENABLED(psessionEntry))
                              ? sizeof(tSirMacDataHdr3a) : sizeof(tSirMacMgmtHdr))
                      + sizeof( eth_890d_header )
                      + PAYLOAD_TYPE_TDLS_SIZE
                      + addIeLen;

    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Setup Response."),
               nBytes );
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set(  pFrame, nBytes, 0 );

    /*
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * request frame
     */

    /* fill out the buffer descriptor */

    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame,
                                 LINK_IDEN_ADDR_OFFSET(tdlsSetupRsp),
                                       TDLS_LINK_AP, TDLS_RESPONDER,
                                       TID_AC_VI, psessionEntry) ;

    limLog(pMac, LOG1,
           FL("SupportedChnlWidth %x rxMCSMap %x rxMCSMap %x txSupDataRate %x"),
           tdlsSetupRsp.VHTCaps.supportedChannelWidthSet,
           tdlsSetupRsp.VHTCaps.rxMCSMap,
           tdlsSetupRsp.VHTCaps.txMCSMap,
           tdlsSetupRsp.VHTCaps.txSupDataRate);
    status = dot11fPackTDLSSetupRsp( pMac, &tdlsSetupRsp, pFrame
                               + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to pack a TDLS Setup Response (0x%08x)."),
               status);
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while packing TDLS Setup Response (0x%08x)."),
               status);
    }

    //Copy the additional IE.
    //TODO : addIe is added at the end of the frame. This means it doesnt
    //follow the order. This should be ok, but we should consider changing this
    //if there is any IOT issue.
    if( addIeLen != 0 )
    {
       vos_mem_copy( pFrame + header_offset + nPayload, addIe, addIeLen );
    }

    limLog(pMac, LOG1,
           FL("[TDLS] action %d (%s) -AP-> OTA peer="MAC_ADDRESS_STR),
           SIR_MAC_TDLS_SETUP_RSP,
           limTraceTdlsActionString(SIR_MAC_TDLS_SETUP_RSP),
           MAC_ADDR_ARRAY(peerMac));

    pMac->lim.tdls_frm_session_id = psessionEntry->peSessionId;
#if defined(CONFIG_HL_SUPPORT)
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            lim_mgmt_tdls_tx_complete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME,
                            smeSessionId, true );
#else
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            lim_mgmt_tdls_tx_complete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME,
                            smeSessionId, false );
#endif
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.tdls_frm_session_id = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Setup Response"));
        return eSIR_FAILURE;
    }

    return eSIR_SUCCESS;

}

/*
 * Send TDLS setup CNF frame on AP link
 */

tSirRetStatus limSendTdlsLinkSetupCnfFrame(tpAniSirGlobal pMac,
                                           tSirMacAddr peerMac,
                                           tANI_U8 dialog,
                                           tANI_U32 peerCapability,
                                           tpPESession psessionEntry,
                                           tANI_U8* addIe, tANI_U16 addIeLen)
{
    tDot11fTDLSSetupCnf  tdlsSetupCnf ;
    tANI_U32            status = 0 ;
    tANI_U32            nPayload = 0 ;
    tANI_U32            nBytes = 0 ;
    tANI_U32            header_offset = 0 ;
    tANI_U8            *pFrame;
    void               *pPacket;
    eHalStatus          halstatus;
#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    tANI_U32            padLen = 0;
#endif
    tANI_U8              smeSessionId = 0;

    /*
     * The scheme here is to fill out a 'tDot11fProbeRequest' structure
     * and then hand it off to 'dot11fPackProbeRequest' (for
     * serialization).  We start by zero-initializing the structure:
     */
    smeSessionId = psessionEntry->smeSessionId;

    vos_mem_set( ( tANI_U8* )&tdlsSetupCnf, sizeof( tDot11fTDLSSetupCnf ), 0 );

    /*
     * setup Fixed fields,
     */
    tdlsSetupCnf.Category.category = SIR_MAC_ACTION_TDLS;
    tdlsSetupCnf.Action.action     = SIR_MAC_TDLS_SETUP_CNF ;
    tdlsSetupCnf.DialogToken.token = dialog ;

    PopulateDot11fLinkIden( pMac, psessionEntry, &tdlsSetupCnf.LinkIdentifier,
                      peerMac, TDLS_INITIATOR) ;
    /*
     * TODO: we need to see if we have to support conditions where we have
     * EDCA parameter info element is needed a) if we need different QOS
     * parameters for off channel operations or QOS is not supported on
     * AP link and we wanted to QOS on direct link.
     */

    /* Check self and peer WMM capable */
    if ((1 == pMac->lim.gLimTDLSWmmMode) &&
                    (CHECK_BIT(peerCapability, TDLS_PEER_WMM_CAP)))
    {
       limLog(pMac, LOG1, FL("populate WMM praram in Setup Confirm"));
       PopulateDot11fWMMParams(pMac, &tdlsSetupCnf.WMMParams, psessionEntry);
    }

    /* Check peer is VHT capable*/
    if (CHECK_BIT(peerCapability, TDLS_PEER_VHT_CAP))
    {
        PopulateDot11fVHTOperation(pMac,
                                   psessionEntry,
                                   &tdlsSetupCnf.VHTOperation);
        PopulateDot11fHTInfo( pMac, &tdlsSetupCnf.HTInfo, psessionEntry );
    }
    else if (CHECK_BIT(peerCapability, TDLS_PEER_HT_CAP)) /* Check peer is HT capable */
    {
        PopulateDot11fHTInfo( pMac, &tdlsSetupCnf.HTInfo, psessionEntry );
    }

    /*
     * now we pack it.  First, how much space are we going to need?
     */
    status = dot11fGetPackedTDLSSetupCnfSize( pMac, &tdlsSetupCnf,
                                                     &nPayload);
    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to calculate the packed size for Setup Confirm (0x%08x)."),
               status);
        /* We'll fall back on the worst case scenario: */
        nPayload = sizeof( tDot11fProbeRequest );
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while calculating the packed size for Setup Confirm (0x%08x)."),
               status);
    }
    /*
     * This frame is going out from PE as data frames with special ethertype
     * 89-0d.
     * 8 bytes of RFC 1042 header
     */
    nBytes = nPayload + ((IS_QOS_ENABLED(psessionEntry))
                              ? sizeof(tSirMacDataHdr3a) : sizeof(tSirMacMgmtHdr))
                      + sizeof( eth_890d_header )
                      + PAYLOAD_TYPE_TDLS_SIZE
                      + addIeLen;

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    /* IOT issue with some AP : some AP doesn't like the data packet size < minimum 802.3 frame length (64)
       Hence AP itself padding some bytes, which caused teardown packet is dropped at
       receiver side. To avoid such IOT issue, we added some extra bytes to meet data frame size >= 64
     */
    if (nPayload + PAYLOAD_TYPE_TDLS_SIZE < MIN_IEEE_8023_SIZE)
    {
        padLen = MIN_IEEE_8023_SIZE - (nPayload + PAYLOAD_TYPE_TDLS_SIZE ) ;

        /* if padLen is less than minimum vendorSpecific (5), pad up to 5 */
        if (padLen < MIN_VENDOR_SPECIFIC_IE_SIZE)
            padLen = MIN_VENDOR_SPECIFIC_IE_SIZE;

        nBytes += padLen;
    }
#endif


    /* Ok-- try to allocate memory from MGMT PKT pool */

    halstatus = palPktAlloc( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                             ( tANI_U16 )nBytes, ( void** ) &pFrame,
                             ( void** ) &pPacket );
    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to allocate %d bytes for a TDLS Setup Confirm."),
               nBytes);
        return eSIR_MEM_ALLOC_FAILED;
    }

    /* zero out the memory */
    vos_mem_set( pFrame, nBytes, 0 );

    /*
     * IE formation, memory allocation is completed, Now form TDLS discovery
     * request frame
     */

    /* fill out the buffer descriptor */

    header_offset = limPrepareTdlsFrameHeader(pMac, pFrame,
                     LINK_IDEN_ADDR_OFFSET(tdlsSetupCnf), TDLS_LINK_AP, TDLS_INITIATOR,
                     TID_AC_VI, psessionEntry) ;

    status = dot11fPackTDLSSetupCnf( pMac, &tdlsSetupCnf, pFrame
                               + header_offset, nPayload, &nPayload );

    if ( DOT11F_FAILED( status ) )
    {
        limLog(pMac, LOGE,
               FL("Failed to pack a TDLS Setup Confirm (0x%08x)."),
               status);
        palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
                                   ( void* ) pFrame, ( void* ) pPacket );
        return eSIR_FAILURE;
    }
    else if ( DOT11F_WARNED( status ) )
    {
        limLog(pMac, LOGW,
               FL("There were warnings while packing TDLS Setup Confirm (0x%08x)."),
               status);
    }
    //Copy the additional IE.
    //TODO : addIe is added at the end of the frame. This means it doesnt
    //follow the order. This should be ok, but we should consider changing this
    //if there is any IOT issue.
    if( addIeLen != 0 )
    {
       vos_mem_copy( pFrame + header_offset + nPayload, addIe, addIeLen );
    }

#ifndef NO_PAD_TDLS_MIN_8023_SIZE
    if (padLen != 0)
    {
        /* QCOM VENDOR OUI = { 0x00, 0xA0, 0xC6, type = 0x0000 }; */
        tANI_U8 *padVendorSpecific = pFrame + header_offset + nPayload + addIeLen;
        /* make QCOM_VENDOR_OUI, and type = 0x0000, and all the payload to be zero */
        padVendorSpecific[0] = 221;
        padVendorSpecific[1] = padLen - 2;
        padVendorSpecific[2] = 0x00;
        padVendorSpecific[3] = 0xA0;
        padVendorSpecific[4] = 0xC6;

        limLog(pMac, LOG1, FL("Padding Vendor Specific Ie Len = %d"), padLen);

        /* padding zero if more than 5 bytes are required */
        if (padLen > MIN_VENDOR_SPECIFIC_IE_SIZE)
            vos_mem_set( pFrame + header_offset + nPayload + addIeLen + MIN_VENDOR_SPECIFIC_IE_SIZE,
                         padLen - MIN_VENDOR_SPECIFIC_IE_SIZE, 0);
    }
#endif


    limLog(pMac, LOG1,
           FL("[TDLS] action %d (%s) -AP-> OTA peer="MAC_ADDRESS_STR),
           SIR_MAC_TDLS_SETUP_CNF,
           limTraceTdlsActionString(SIR_MAC_TDLS_SETUP_CNF),
           MAC_ADDR_ARRAY(peerMac));

    pMac->lim.tdls_frm_session_id = psessionEntry->peSessionId;
#if defined(CONFIG_HL_SUPPORT)
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            lim_mgmt_tdls_tx_complete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME,
                            smeSessionId, true );
#else
    halstatus = halTxFrameWithTxComplete( pMac, pPacket, ( tANI_U16 ) nBytes,
                            HAL_TXRX_FRM_802_11_DATA,
                            ANI_TXDIR_TODS,
                            TID_AC_VI,
                            limTxComplete, pFrame,
                            lim_mgmt_tdls_tx_complete,
                            HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME,
                            smeSessionId, false );
#endif

    if ( ! HAL_STATUS_SUCCESS ( halstatus ) )
    {
        pMac->lim.tdls_frm_session_id = 0xff;
        limLog(pMac, LOGE, FL("could not send TDLS Setup Confirm frame"));
        return eSIR_FAILURE;

    }

    return eSIR_SUCCESS;
}


/* This Function is similar to PopulateDot11fHTCaps, except that the HT Capabilities
 * are considered from the AddStaReq rather from the cfg.dat as in PopulateDot11fHTCaps
 */
static tSirRetStatus limTdlsPopulateDot11fHTCaps(tpAniSirGlobal pMac, tpPESession psessionEntry,
            tSirTdlsAddStaReq *pTdlsAddStaReq, tDot11fIEHTCaps *pDot11f)
{
    tANI_U32                         nCfgValue;
    tANI_U8                          nCfgValue8;
    tSirMacHTParametersInfo         *pHTParametersInfo;
    union {
        tANI_U16                        nCfgValue16;
        tSirMacHTCapabilityInfo         htCapInfo;
        tSirMacExtendedHTCapabilityInfo extHtCapInfo;
    } uHTCapabilityInfo;

    tSirMacTxBFCapabilityInfo       *pTxBFCapabilityInfo;
    tSirMacASCapabilityInfo         *pASCapabilityInfo;

    nCfgValue = pTdlsAddStaReq->htCap.capInfo;

    uHTCapabilityInfo.nCfgValue16 = nCfgValue & 0xFFFF;

    pDot11f->advCodingCap             = uHTCapabilityInfo.htCapInfo.advCodingCap;
    pDot11f->mimoPowerSave            = uHTCapabilityInfo.htCapInfo.mimoPowerSave;
    pDot11f->greenField               = uHTCapabilityInfo.htCapInfo.greenField;
    pDot11f->shortGI20MHz             = uHTCapabilityInfo.htCapInfo.shortGI20MHz;
    pDot11f->shortGI40MHz             = uHTCapabilityInfo.htCapInfo.shortGI40MHz;
    pDot11f->txSTBC                   = uHTCapabilityInfo.htCapInfo.txSTBC;
    pDot11f->rxSTBC                   = uHTCapabilityInfo.htCapInfo.rxSTBC;
    pDot11f->delayedBA                = uHTCapabilityInfo.htCapInfo.delayedBA;
    pDot11f->maximalAMSDUsize         = uHTCapabilityInfo.htCapInfo.maximalAMSDUsize;
    pDot11f->dsssCckMode40MHz         = uHTCapabilityInfo.htCapInfo.dsssCckMode40MHz;
    pDot11f->psmp                     = uHTCapabilityInfo.htCapInfo.psmp;
    pDot11f->stbcControlFrame         = uHTCapabilityInfo.htCapInfo.stbcControlFrame;
    pDot11f->lsigTXOPProtection       = uHTCapabilityInfo.htCapInfo.lsigTXOPProtection;

    // All sessionized entries will need the check below
    if (psessionEntry == NULL) // Only in case of NO session
    {
        pDot11f->supportedChannelWidthSet = uHTCapabilityInfo.htCapInfo.supportedChannelWidthSet;
    }
    else
    {
        pDot11f->supportedChannelWidthSet = psessionEntry->htSupportedChannelWidthSet;
    }

    /* Ensure that shortGI40MHz is Disabled if supportedChannelWidthSet is
       eHT_CHANNEL_WIDTH_20MHZ */
    if(pDot11f->supportedChannelWidthSet == eHT_CHANNEL_WIDTH_20MHZ)
    {
       pDot11f->shortGI40MHz = 0;
    }

    limLog(pMac, LOG1,
           FL("SupportedChnlWidth: %d, mimoPS: %d, GF: %d, shortGI20:%d, shortGI40: %d, dsssCck: %d"),
           pDot11f->supportedChannelWidthSet,
           pDot11f->mimoPowerSave,
           pDot11f->greenField,
           pDot11f->shortGI20MHz,
           pDot11f->shortGI40MHz,
           pDot11f->dsssCckMode40MHz);

    nCfgValue = pTdlsAddStaReq->htCap.ampduParamsInfo;

    nCfgValue8 = ( tANI_U8 ) nCfgValue;
    pHTParametersInfo = ( tSirMacHTParametersInfo* ) &nCfgValue8;

    pDot11f->maxRxAMPDUFactor = pHTParametersInfo->maxRxAMPDUFactor;
    pDot11f->mpduDensity      = pHTParametersInfo->mpduDensity;
    pDot11f->reserved1        = pHTParametersInfo->reserved;

    limLog(pMac, LOG1, FL("AMPDU Param: %x"), nCfgValue);

    vos_mem_copy( pDot11f->supportedMCSSet, pTdlsAddStaReq->htCap.suppMcsSet,
                  SIZE_OF_SUPPORTED_MCS_SET);

    nCfgValue = pTdlsAddStaReq->htCap.extendedHtCapInfo;

    uHTCapabilityInfo.nCfgValue16 = nCfgValue & 0xFFFF;

    pDot11f->pco            = uHTCapabilityInfo.extHtCapInfo.pco;
    pDot11f->transitionTime = uHTCapabilityInfo.extHtCapInfo.transitionTime;
    pDot11f->mcsFeedback    = uHTCapabilityInfo.extHtCapInfo.mcsFeedback;

    nCfgValue = pTdlsAddStaReq->htCap.txBFCapInfo;

    pTxBFCapabilityInfo = ( tSirMacTxBFCapabilityInfo* ) &nCfgValue;
    pDot11f->txBF                                       = pTxBFCapabilityInfo->txBF;
    pDot11f->rxStaggeredSounding                        = pTxBFCapabilityInfo->rxStaggeredSounding;
    pDot11f->txStaggeredSounding                        = pTxBFCapabilityInfo->txStaggeredSounding;
    pDot11f->rxZLF                                      = pTxBFCapabilityInfo->rxZLF;
    pDot11f->txZLF                                      = pTxBFCapabilityInfo->txZLF;
    pDot11f->implicitTxBF                               = pTxBFCapabilityInfo->implicitTxBF;
    pDot11f->calibration                                = pTxBFCapabilityInfo->calibration;
    pDot11f->explicitCSITxBF                            = pTxBFCapabilityInfo->explicitCSITxBF;
    pDot11f->explicitUncompressedSteeringMatrix         = pTxBFCapabilityInfo->explicitUncompressedSteeringMatrix;
    pDot11f->explicitBFCSIFeedback                      = pTxBFCapabilityInfo->explicitBFCSIFeedback;
    pDot11f->explicitUncompressedSteeringMatrixFeedback = pTxBFCapabilityInfo->explicitUncompressedSteeringMatrixFeedback;
    pDot11f->explicitCompressedSteeringMatrixFeedback   = pTxBFCapabilityInfo->explicitCompressedSteeringMatrixFeedback;
    pDot11f->csiNumBFAntennae                           = pTxBFCapabilityInfo->csiNumBFAntennae;
    pDot11f->uncompressedSteeringMatrixBFAntennae       = pTxBFCapabilityInfo->uncompressedSteeringMatrixBFAntennae;
    pDot11f->compressedSteeringMatrixBFAntennae         = pTxBFCapabilityInfo->compressedSteeringMatrixBFAntennae;

    nCfgValue = pTdlsAddStaReq->htCap.antennaSelectionInfo;

    nCfgValue8 = ( tANI_U8 ) nCfgValue;

    pASCapabilityInfo = ( tSirMacASCapabilityInfo* ) &nCfgValue8;
    pDot11f->antennaSelection         = pASCapabilityInfo->antennaSelection;
    pDot11f->explicitCSIFeedbackTx    = pASCapabilityInfo->explicitCSIFeedbackTx;
    pDot11f->antennaIndicesFeedbackTx = pASCapabilityInfo->antennaIndicesFeedbackTx;
    pDot11f->explicitCSIFeedback      = pASCapabilityInfo->explicitCSIFeedback;
    pDot11f->antennaIndicesFeedback   = pASCapabilityInfo->antennaIndicesFeedback;
    pDot11f->rxAS                     = pASCapabilityInfo->rxAS;
    pDot11f->txSoundingPPDUs          = pASCapabilityInfo->txSoundingPPDUs;

    pDot11f->present = pTdlsAddStaReq->htcap_present;

    return eSIR_SUCCESS;

}

tSirRetStatus
limTdlsPopulateDot11fVHTCaps(tpAniSirGlobal pMac,
                      tSirTdlsAddStaReq *pTdlsAddStaReq,
                      tDot11fIEVHTCaps  *pDot11f)
{
    tANI_U32             nCfgValue=0;
    union {
        tANI_U32                       nCfgValue32;
        tSirMacVHTCapabilityInfo       vhtCapInfo;
    } uVHTCapabilityInfo;
    union {
        tANI_U16                       nCfgValue16;
        tSirMacVHTTxSupDataRateInfo    vhtTxSupDataRateInfo;
        tSirMacVHTRxSupDataRateInfo    vhtRxsupDataRateInfo;
    } uVHTSupDataRateInfo;

    pDot11f->present = pTdlsAddStaReq->vhtcap_present;

    nCfgValue = pTdlsAddStaReq->vhtCap.vhtCapInfo;
    uVHTCapabilityInfo.nCfgValue32 = nCfgValue;

    pDot11f->maxMPDULen =  uVHTCapabilityInfo.vhtCapInfo.maxMPDULen;
    pDot11f->supportedChannelWidthSet =  uVHTCapabilityInfo.vhtCapInfo.supportedChannelWidthSet;
    pDot11f->ldpcCodingCap =  uVHTCapabilityInfo.vhtCapInfo.ldpcCodingCap;
    pDot11f->shortGI80MHz =  uVHTCapabilityInfo.vhtCapInfo.shortGI80MHz;
    pDot11f->shortGI160and80plus80MHz =  uVHTCapabilityInfo.vhtCapInfo.shortGI160and80plus80MHz;
    pDot11f->txSTBC =  uVHTCapabilityInfo.vhtCapInfo.txSTBC;
    pDot11f->rxSTBC =  uVHTCapabilityInfo.vhtCapInfo.rxSTBC;
    pDot11f->suBeamFormerCap = 0;
    pDot11f->suBeamformeeCap = 0;
    pDot11f->csnofBeamformerAntSup =  uVHTCapabilityInfo.vhtCapInfo.csnofBeamformerAntSup;
    pDot11f->numSoundingDim =  uVHTCapabilityInfo.vhtCapInfo.numSoundingDim;
    pDot11f->muBeamformerCap =  0;
    pDot11f->muBeamformeeCap =  0;
    pDot11f->vhtTXOPPS =  uVHTCapabilityInfo.vhtCapInfo.vhtTXOPPS;
    pDot11f->htcVHTCap =  uVHTCapabilityInfo.vhtCapInfo.htcVHTCap;
    pDot11f->maxAMPDULenExp =  uVHTCapabilityInfo.vhtCapInfo.maxAMPDULenExp;
    pDot11f->vhtLinkAdaptCap =  uVHTCapabilityInfo.vhtCapInfo.vhtLinkAdaptCap;
    pDot11f->rxAntPattern =  uVHTCapabilityInfo.vhtCapInfo.rxAntPattern;
    pDot11f->txAntPattern =  uVHTCapabilityInfo.vhtCapInfo.txAntPattern;
    pDot11f->reserved1= uVHTCapabilityInfo.vhtCapInfo.reserved1;

    pDot11f->rxMCSMap = pTdlsAddStaReq->vhtCap.suppMcs.rxMcsMap;

    nCfgValue = pTdlsAddStaReq->vhtCap.suppMcs.rxHighest;
    uVHTSupDataRateInfo.nCfgValue16 = nCfgValue & 0xffff;
    pDot11f->rxHighSupDataRate = uVHTSupDataRateInfo.vhtRxsupDataRateInfo.rxSupDataRate;

    pDot11f->txMCSMap = pTdlsAddStaReq->vhtCap.suppMcs.txMcsMap;

    nCfgValue = pTdlsAddStaReq->vhtCap.suppMcs.txHighest;
    uVHTSupDataRateInfo.nCfgValue16 = nCfgValue & 0xffff;
    pDot11f->txSupDataRate = uVHTSupDataRateInfo.vhtTxSupDataRateInfo.txSupDataRate;

    pDot11f->reserved3= uVHTSupDataRateInfo.vhtTxSupDataRateInfo.reserved;

    limLogVHTCap(pMac, pDot11f);

    return eSIR_SUCCESS;

}

static tSirRetStatus
limTdlsPopulateMatchingRateSet(tpAniSirGlobal pMac,
                           tpDphHashNode pStaDs,
                           tANI_U8 *pSupportedRateSet,
                           tANI_U8 supporteRatesLength,
                           tANI_U8* pSupportedMCSSet,
                           tpPESession  psessionEntry,
                           tDot11fIEVHTCaps *pVHTCaps)

{
    tSirMacRateSet    tempRateSet;
    tANI_U32          i,j,val,min,isArate;
    tSirMacRateSet    tempRateSet2;
    tANI_U32 phyMode;
    tANI_U8 mcsSet[SIZE_OF_SUPPORTED_MCS_SET];
    uint8_t nss;
    isArate=0;
    tempRateSet2.numRates = 0;

    limGetPhyMode(pMac, &phyMode, NULL);

    // get own rate set
    val = WNI_CFG_OPERATIONAL_RATE_SET_LEN;
    if (wlan_cfgGetStr(pMac, WNI_CFG_OPERATIONAL_RATE_SET,
                                          (tANI_U8 *) &tempRateSet.rate,
                                          &val) != eSIR_SUCCESS)
    {
        /// Could not get rateset from CFG. Log error.
        limLog(pMac, LOGE, FL("could not retrieve rateset"));
        val = 0;
    }
    tempRateSet.numRates = val;

    if (phyMode == WNI_CFG_PHY_MODE_11G)
    {

        // get own extended rate set
        val = WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET_LEN;
        if (wlan_cfgGetStr(pMac, WNI_CFG_EXTENDED_OPERATIONAL_RATE_SET,
                                                  (tANI_U8 *) &tempRateSet2.rate,
                                                  &val) != eSIR_SUCCESS)
        tempRateSet2.numRates = val;
    }

    if ((tempRateSet.numRates + tempRateSet2.numRates) > 12)
    {
        limLog(pMac, LOGE, FL("more than 12 rates in CFG"));
        goto error;
    }

    /**
         * Handling of the rate set IEs is the following:
         * - keep only rates that we support and that the station supports
         * - sort and the rates into the pSta->rate array
         */

    // Copy all rates in tempRateSet, there are 12 rates max
    for (i = 0; i < tempRateSet2.numRates; i++)
        tempRateSet.rate[i + tempRateSet.numRates] = tempRateSet2.rate[i];

    tempRateSet.numRates += tempRateSet2.numRates;

    /**
         * Sort rates in tempRateSet (they are likely to be already sorted)
         * put the result in tempRateSet2
         */
    tempRateSet2.numRates = 0;

    for (i = 0;i < tempRateSet.numRates; i++)
    {
        min = 0;
        val = 0xff;

        for(j = 0;j < tempRateSet.numRates; j++)
            if ((tANI_U32) (tempRateSet.rate[j] & 0x7f) < val)
            {
                val = tempRateSet.rate[j] & 0x7f;
                min = j;
            }

        tempRateSet2.rate[tempRateSet2.numRates++] = tempRateSet.rate[min];
        tempRateSet.rate[min] = 0xff;
    }

    /**
     * Copy received rates in tempRateSet, the parser has ensured
     * unicity of the rates so there cannot be more than 12 .
     */
    if (supporteRatesLength > SIR_MAC_RATESET_EID_MAX)
    {
        limLog(pMac, LOGW,
               FL("Supported rates length %d more than the Max limit, reset to Max"),
               supporteRatesLength);
        supporteRatesLength = SIR_MAC_RATESET_EID_MAX;
    }

    for (i = 0; i < supporteRatesLength; i++)
    {
        tempRateSet.rate[i] = pSupportedRateSet[i];
    }

    tempRateSet.numRates = supporteRatesLength;

    {
        tpSirSupportedRates  rates = &pStaDs->supportedRates;
        tANI_U8 aRateIndex = 0;
        tANI_U8 bRateIndex = 0;
        vos_mem_set( (tANI_U8 *) rates, sizeof(tSirSupportedRates), 0);

        for (i = 0;i < tempRateSet2.numRates; i++)
        {
            for (j = 0;j < tempRateSet.numRates; j++)
            {
                if ((tempRateSet2.rate[i] & 0x7F) ==
                    (tempRateSet.rate[j] & 0x7F))
                {

                    if ((bRateIndex > SIR_NUM_11B_RATES) || (aRateIndex > SIR_NUM_11A_RATES))
                    {
                        limLog(pMac, LOGE, FL("Invalid number of rates (11b->%d, 11a->%d)"),
                                           bRateIndex,
                                           aRateIndex);
                        return eSIR_FAILURE;
                    }

                    if (sirIsArate(tempRateSet2.rate[i] & 0x7f)) {
                        isArate=1;
                        if (aRateIndex < SIR_NUM_11A_RATES)
                            rates->llaRates[aRateIndex++] = tempRateSet2.rate[i];
                    }
                    else {
                        if (bRateIndex < SIR_NUM_11B_RATES)
                            rates->llbRates[bRateIndex++] = tempRateSet2.rate[i];
                    }
                    break;
                }
            }
        }
    }

    if (IS_5G_CH(psessionEntry->currentOperChannel))
        nss = pMac->vdev_type_nss_5g.tdls;
    else
        nss = pMac->vdev_type_nss_2g.tdls;
    nss = VOS_MIN(nss, pMac->user_configured_nss);
    //compute the matching MCS rate set, if peer is 11n capable and self mode is 11n
#ifdef FEATURE_WLAN_TDLS
    if (pStaDs->mlmStaContext.htCapability)
#else
    if (IS_DOT11_MODE_HT(psessionEntry->dot11mode) &&
       (pStaDs->mlmStaContext.htCapability))
#endif
    {
        val = SIZE_OF_SUPPORTED_MCS_SET;
        if (wlan_cfgGetStr(pMac, WNI_CFG_SUPPORTED_MCS_SET,
                           mcsSet,
                           &val) != eSIR_SUCCESS)
        {
            /// Could not get rateset from CFG. Log error.
            limLog(pMac, LOGP, FL("could not retrieve supportedMCSSet"));
            goto error;
        }
        if (NSS_1x1_MODE == nss)
            mcsSet[1] = 0;
        for (i=0; i<val; i++)
            pStaDs->supportedRates.supportedMCSSet[i] = mcsSet[i] & pSupportedMCSSet[i];

        limLog(pMac, LOG1, FL("MCS Rate Set Bitmap from CFG and DPH:"));
        for (i=0; i<SIR_MAC_MAX_SUPPORTED_MCS_SET; i++)
        {
            limLog(pMac, LOG1, FL("%x %x"),
                               mcsSet[i],
                               pStaDs->supportedRates.supportedMCSSet[i]);
        }
    }

#ifdef WLAN_FEATURE_11AC
    limPopulateVhtMcsSet(pMac, &pStaDs->supportedRates, pVHTCaps,
                    psessionEntry, nss);
#endif
    /**
         * Set the erpEnabled bit iff the phy is in G mode and at least
         * one A rate is supported
         */
    if ((phyMode == WNI_CFG_PHY_MODE_11G) && isArate)
        pStaDs->erpEnabled = eHAL_SET;



    return eSIR_SUCCESS;

 error:

    return eSIR_FAILURE;
}

/*
 * update HASH node entry info
 */
static void limTdlsUpdateHashNodeInfo(tpAniSirGlobal pMac, tDphHashNode *pStaDs,
              tSirTdlsAddStaReq *pTdlsAddStaReq, tpPESession psessionEntry)
{
    tDot11fIEHTCaps htCap = {0,};
    tDot11fIEHTCaps *htCaps;
    tDot11fIEVHTCaps *pVhtCaps = NULL;
    tDot11fIEVHTCaps *pVhtCaps_txbf = NULL;
#ifdef WLAN_FEATURE_11AC
    tDot11fIEVHTCaps vhtCap;
    tANI_U8 cbMode;
#endif
    tpDphHashNode pSessStaDs = NULL;
    tANI_U16 aid;

    if (pTdlsAddStaReq->tdlsAddOper == TDLS_OPER_ADD)
    {
        PopulateDot11fHTCaps(pMac, psessionEntry, &htCap);
    }
    else if (pTdlsAddStaReq->tdlsAddOper == TDLS_OPER_UPDATE)
    {
        limTdlsPopulateDot11fHTCaps(pMac, NULL, pTdlsAddStaReq, &htCap);
    }
    htCaps = &htCap;
    if (htCaps->present)
    {
        pStaDs->mlmStaContext.htCapability = 1 ;
        pStaDs->htGreenfield = htCaps->greenField ;
        /*
         * pStaDs->htSupportedChannelWidthSet should have the base channel
         * capability. The htSupportedChannelWidthSet of the TDLS link on
         * base channel should be less than or equal to channel width of
         * STA-AP link. So take this setting from the psessionEntry.
         */
        limLog(pMac, LOG1,
               FL("supportedChannelWidthSet %x htSupportedChannelWidthSet %x"),
               htCaps->supportedChannelWidthSet,
               psessionEntry->htSupportedChannelWidthSet);

        pStaDs->htSupportedChannelWidthSet =
            (htCaps->supportedChannelWidthSet <
              psessionEntry->htSupportedChannelWidthSet) ?
                 htCaps->supportedChannelWidthSet :
                 psessionEntry->htSupportedChannelWidthSet;

        limLog(pMac, LOG1, FL("pStaDs->htSupportedChannelWidthSet %x"),
               pStaDs->htSupportedChannelWidthSet);

        pStaDs->htMIMOPSState = htCaps->mimoPowerSave ;
        pStaDs->htMaxAmsduLength =  htCaps->maximalAMSDUsize;
        pStaDs->htAMpduDensity =    htCaps->mpduDensity;
        pStaDs->htDsssCckRate40MHzSupport = htCaps->dsssCckMode40MHz ;
        pStaDs->htShortGI20Mhz = htCaps->shortGI20MHz;
        pStaDs->htShortGI40Mhz = htCaps->shortGI40MHz;
        pStaDs->htMaxRxAMpduFactor = htCaps->maxRxAMPDUFactor;
        limFillRxHighestSupportedRate(pMac,
                             &pStaDs->supportedRates.rxHighestDataRate,
                                                 htCaps->supportedMCSSet);
        pStaDs->baPolicyFlag = 0xFF;
        pMac->lim.gLimTdlsLinkMode = TDLS_LINK_MODE_N ;
        pStaDs->ht_caps = pTdlsAddStaReq->htCap.capInfo;
    }
    else
    {
        pStaDs->mlmStaContext.htCapability = 0 ;
        pMac->lim.gLimTdlsLinkMode = TDLS_LINK_MODE_BG ;
    }
#ifdef WLAN_FEATURE_11AC
    limTdlsPopulateDot11fVHTCaps(pMac, pTdlsAddStaReq, &vhtCap);
    pVhtCaps = &vhtCap;
    if (pVhtCaps->present)
    {
        pStaDs->mlmStaContext.vhtCapability = 1 ;

        /*
         * channel width of TDLS link on base channel should not exceed
         * channel width of STA-AP link. Hence take this setting from the
         * psessionEntry.
         */
        pStaDs->vhtSupportedChannelWidthSet =
                psessionEntry->vhtTxChannelWidthSet;
        pStaDs->vhtLdpcCapable = pVhtCaps->ldpcCodingCap;
        pStaDs->vhtBeamFormerCapable = 0;
        pMac->lim.gLimTdlsLinkMode = TDLS_LINK_MODE_AC;
        pVhtCaps_txbf = (tDot11fIEVHTCaps *)(&pTdlsAddStaReq->vhtCap);
        pVhtCaps_txbf->suBeamformeeCap = 0;
        pVhtCaps_txbf->suBeamFormerCap = 0;
        pVhtCaps_txbf->muBeamformerCap = 0;
        pVhtCaps_txbf->muBeamformeeCap = 0;
        pStaDs->vht_caps = pTdlsAddStaReq->vhtCap.vhtCapInfo;
    }
    else
    {
        pStaDs->mlmStaContext.vhtCapability = 0 ;
        pStaDs->vhtSupportedChannelWidthSet = WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ;
    }
#endif
    /*Calculate the Secondary Coannel Offset */
    cbMode = limSelectCBMode(pStaDs, psessionEntry,
                             psessionEntry->currentOperChannel,
                             pStaDs->vhtSupportedChannelWidthSet);

    pStaDs->htSecondaryChannelOffset = cbMode;

#ifdef WLAN_FEATURE_11AC
    if ( pStaDs->mlmStaContext.vhtCapability )
    {
        pStaDs->htSecondaryChannelOffset = limGetHTCBState(cbMode);
    }
#endif

    pSessStaDs = dphLookupHashEntry(pMac, psessionEntry->bssId, &aid,
                                          &psessionEntry->dph.dphHashTable) ;

    /* Lets enable QOS parameter */
    pStaDs->qosMode    = (pTdlsAddStaReq->capability & CAPABILITIES_QOS_OFFSET)
                          || pTdlsAddStaReq->htcap_present;
    pStaDs->wmeEnabled = 1;
    pStaDs->lleEnabled = 0;
    /*  TDLS Dummy AddSTA does not have qosInfo , is it OK ??
     */
    pStaDs->qos.capability.qosInfo = (*(tSirMacQosInfoStation *) &pTdlsAddStaReq->uapsd_queues);

    /* populate matching rate set */

    /* TDLS Dummy AddSTA does not have HTCap,VHTCap,Rates info , is it OK ??
     */
    limTdlsPopulateMatchingRateSet(pMac, pStaDs, pTdlsAddStaReq->supported_rates,
                                   pTdlsAddStaReq->supported_rates_length,
                                   (tANI_U8 *)pTdlsAddStaReq->htCap.suppMcsSet,
                                   psessionEntry, pVhtCaps);

    /*  TDLS Dummy AddSTA does not have right capability , is it OK ??
     */
    pStaDs->mlmStaContext.capabilityInfo = ( *(tSirMacCapabilityInfo *) &pTdlsAddStaReq->capability);

    return ;
}

/*
 * Add STA for TDLS setup procedure
 */
static tSirRetStatus limTdlsSetupAddSta(tpAniSirGlobal pMac,
                                        tSirTdlsAddStaReq *pAddStaReq,
                                        tpPESession psessionEntry)
{
    tpDphHashNode pStaDs = NULL ;
    tSirRetStatus status = eSIR_SUCCESS ;
    tANI_U16 aid = 0 ;

    pStaDs = dphLookupHashEntry(pMac, pAddStaReq->peerMac, &aid,
                                      &psessionEntry->dph.dphHashTable);
    if(NULL == pStaDs)
    {
        aid = limAssignPeerIdx(pMac, psessionEntry) ;

        if( !aid )
        {
            limLog(pMac, LOGE, FL("No more free AID for peer " MAC_ADDRESS_STR),
                               MAC_ADDR_ARRAY(pAddStaReq->peerMac));
            return eSIR_FAILURE;
        }

        /* Set the aid in peerAIDBitmap as it has been assigned to TDLS peer */
        SET_PEER_AID_BITMAP(psessionEntry->peerAIDBitmap, aid);

        limLog(pMac, LOG1, FL("Aid = %d, for peer =" MAC_ADDRESS_STR),
                           aid, MAC_ADDR_ARRAY(pAddStaReq->peerMac));
        pStaDs = dphGetHashEntry(pMac, aid, &psessionEntry->dph.dphHashTable);

        if (pStaDs)
        {
            (void) limDelSta(pMac, pStaDs, false /*asynchronous*/, psessionEntry);
            limDeleteDphHashEntry(pMac, pStaDs->staAddr, aid, psessionEntry);
        }

        pStaDs = dphAddHashEntry(pMac, pAddStaReq->peerMac, aid,
                                             &psessionEntry->dph.dphHashTable) ;

        if(NULL == pStaDs)
        {
            limLog(pMac, LOGE, FL("add hash entry failed"));
            VOS_ASSERT(0) ;
            return eSIR_FAILURE;
        }
    }

    limTdlsUpdateHashNodeInfo(pMac, pStaDs, pAddStaReq, psessionEntry) ;

    pStaDs->staType = STA_ENTRY_TDLS_PEER ;

    status = limAddSta(pMac, pStaDs, (pAddStaReq->tdlsAddOper == TDLS_OPER_UPDATE) ? true: false, psessionEntry);

    if(eSIR_SUCCESS != status)
    {
        /* should not fail */
        VOS_ASSERT(0) ;
    }
    return status ;
}

/*
 * Del STA, after Link is teardown or discovery response sent on direct link
 */
static tpDphHashNode limTdlsDelSta(tpAniSirGlobal pMac, tSirMacAddr peerMac,
                                                    tpPESession psessionEntry)
{
    tSirRetStatus status = eSIR_SUCCESS ;
    tANI_U16 peerIdx = 0 ;
    tpDphHashNode pStaDs = NULL ;

    pStaDs = dphLookupHashEntry(pMac, peerMac, &peerIdx,
                                         &psessionEntry->dph.dphHashTable) ;

    if(pStaDs)
    {

        limLog(pMac, LOG1, FL("DEL STA peer MAC: "MAC_ADDRESS_STR),
                           MAC_ADDR_ARRAY(pStaDs->staAddr));
        limLog(pMac, LOG1, FL("STA type = %x, sta idx = %x"),
                           pStaDs->staType,
                           pStaDs->staIndex);
        status = limDelSta(pMac, pStaDs, false, psessionEntry) ;
    }

    return pStaDs ;
}


/*
 * Once Link is setup with PEER, send Add STA ind to SME
 */
static eHalStatus limSendSmeTdlsAddStaRsp(tpAniSirGlobal pMac,
                   tANI_U8 sessionId, tSirMacAddr peerMac, tANI_U8 updateSta,
                   tDphHashNode  *pStaDs, tANI_U8 status)
{
    tSirMsgQ  mmhMsg = {0} ;
    tSirTdlsAddStaRsp *addStaRsp = NULL ;
    mmhMsg.type = eWNI_SME_TDLS_ADD_STA_RSP ;

    addStaRsp = vos_mem_malloc(sizeof(tSirTdlsAddStaRsp));
    if ( NULL == addStaRsp )
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory"));
        return eSIR_FAILURE;
    }

    addStaRsp->sessionId = sessionId;
    addStaRsp->statusCode = status;
    if( pStaDs )
    {
        addStaRsp->staId = pStaDs->staIndex ;
        addStaRsp->ucastSig = pStaDs->ucUcastSig ;
        addStaRsp->bcastSig = pStaDs->ucBcastSig ;
    }
    if( peerMac )
    {
        vos_mem_copy( addStaRsp->peerMac,
                (tANI_U8 *) peerMac, sizeof(tSirMacAddr));
    }
    if (updateSta)
        addStaRsp->tdlsAddOper = TDLS_OPER_UPDATE;
    else
        addStaRsp->tdlsAddOper = TDLS_OPER_ADD;

    addStaRsp->length = sizeof(tSirTdlsAddStaRsp) ;
    addStaRsp->messageType = eWNI_SME_TDLS_ADD_STA_RSP ;

    mmhMsg.bodyptr = addStaRsp;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);

    return eSIR_SUCCESS ;

}
/*
 * STA RSP received from HAL
 */
eHalStatus limProcessTdlsAddStaRsp(tpAniSirGlobal pMac, void *msg,
                                                   tpPESession psessionEntry)
{
    tAddStaParams  *pAddStaParams = (tAddStaParams *) msg ;
    tANI_U8        status = eSIR_SUCCESS ;
    tDphHashNode   *pStaDs = NULL ;
    tANI_U16        aid = 0 ;

    SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
    limLog(pMac, LOG1, FL("staIdx=%d, staMac="MAC_ADDRESS_STR),
                       pAddStaParams->staIdx,
                       MAC_ADDR_ARRAY(pAddStaParams->staMac));

    if (pAddStaParams->status != eHAL_STATUS_SUCCESS)
    {
        VOS_ASSERT(0) ;
        limLog(pMac, LOGE, FL("Add sta failed "));
        status = eSIR_FAILURE;
        goto add_sta_error;
    }

    pStaDs = dphLookupHashEntry(pMac, pAddStaParams->staMac, &aid,
                                         &psessionEntry->dph.dphHashTable);
    if(NULL == pStaDs)
    {
        limLog(pMac, LOGE, FL("pStaDs is NULL "));
        status = eSIR_FAILURE;
        goto add_sta_error;
    }

    pStaDs->bssId                  = pAddStaParams->bssIdx;
    pStaDs->staIndex               = pAddStaParams->staIdx;
    pStaDs->ucUcastSig             = pAddStaParams->ucUcastSig;
    pStaDs->ucBcastSig             = pAddStaParams->ucBcastSig;
    pStaDs->mlmStaContext.mlmState = eLIM_MLM_LINK_ESTABLISHED_STATE;
    pStaDs->valid                  = 1 ;
add_sta_error:
    status = limSendSmeTdlsAddStaRsp(pMac, psessionEntry->smeSessionId,
                                        pAddStaParams->staMac, pAddStaParams->updateSta, pStaDs, status) ;
    vos_mem_free( pAddStaParams );
    return status ;
}

void PopulateDot11fTdlsOffchannelParams(tpAniSirGlobal pMac,
                             tpPESession psessionEntry,
                             tDot11fIESuppChannels *suppChannels,
                             tDot11fIESuppOperatingClasses *suppOperClasses)
{
    tANI_U32   numChans = WNI_CFG_VALID_CHANNEL_LIST_LEN;
    tANI_U8    validChan[WNI_CFG_VALID_CHANNEL_LIST_LEN];
    tANI_U8    i;
    tANI_U8    valid_count = 0;
    tANI_U8    chanOffset;
    tANI_U8    op_class;
    tANI_U8    numClasses;
    uint32_t   band;
    uint8_t    nss_2g;
    uint8_t    nss_5g;
    tANI_U8    classes[SIR_MAC_MAX_SUPP_OPER_CLASSES];
    if (wlan_cfgGetStr(pMac, WNI_CFG_VALID_CHANNEL_LIST,
                          validChan, &numChans) != eSIR_SUCCESS)
    {
        /**
         * Could not get Valid channel list from CFG.
         * Log error.
         */
         limLog(pMac, LOGE, FL("could not retrieve valid channel list"));
         return;
    }

    if (IS_5G_CH(psessionEntry->currentOperChannel))
        band = eCSR_BAND_5G;
    else
        band = eCSR_BAND_24;

    nss_5g = VOS_MIN(pMac->vdev_type_nss_5g.tdls, pMac->user_configured_nss);
    nss_2g = VOS_MIN(pMac->vdev_type_nss_2g.tdls, pMac->user_configured_nss);

    /* validating the channel list for DFS and 2G channels */
    for (i = 0U; i < numChans; i++) {
        if ((band == eCSR_BAND_5G) && (NSS_2x2_MODE == nss_5g) &&
            (NSS_1x1_MODE == nss_2g) &&
            (true == vos_nv_skip_dsrc_dfs_2g(validChan[i],
             NV_CHANNEL_SKIP_2G))) {
                limLog(pMac, LOG1,
                       FL("skipping channel %d, nss_5g: %d, nss_2g: %d"),
                       validChan[i], nss_5g, nss_2g);
                continue;
        } else {
            if (true == vos_nv_skip_dsrc_dfs_2g(validChan[i],
                NV_CHANNEL_SKIP_DSRC)) {
                limLog(pMac, LOG1,
                       FL("skipping channel %d from the valid channel list"),
                       validChan[i]);
                continue;
            }
        }

        if (valid_count >=
                sizeof(suppChannels->bands) / sizeof(suppChannels->bands[0]))
            break;

        suppChannels->bands[valid_count][0] = validChan[i];
        suppChannels->bands[valid_count][1] = 1;
        valid_count++;
    }

    suppChannels->num_bands = valid_count;
    suppChannels->present = 1 ;

    /* find channel offset and get op class for current operating channel */
    switch (psessionEntry->htSecondaryChannelOffset)
    {
      case PHY_SINGLE_CHANNEL_CENTERED:
          chanOffset = BW20;
          break;

      case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
          chanOffset = BW40_LOW_PRIMARY;
          break;

      case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
          chanOffset = BW40_HIGH_PRIMARY;
          break;

      default:
          chanOffset = BWALL;
          break;

    }

    op_class = regdm_get_opclass_from_channel(pMac->scan.countryCodeCurrent,
                                          psessionEntry->currentOperChannel,
                                          chanOffset);
    if (op_class == 0)
    {
        limLog(pMac, LOGE,
               FL("Present Operating class is wrong, countryCodeCurrent: %s, currentOperChannel: %d, htSecondaryChannelOffset: %d, chanOffset: %d"),
               pMac->scan.countryCodeCurrent,
               psessionEntry->currentOperChannel,
               psessionEntry->htSecondaryChannelOffset,
               chanOffset);
    }
    else
    {
        limLog(pMac, LOG1,
               FL("Present Operating channel: %d chanOffset: %d, op class=%d"),
               psessionEntry->currentOperChannel,
               chanOffset,
               op_class);
    }
    suppOperClasses->present = 1;
    suppOperClasses->classes[0] = op_class;

    regdm_get_curr_opclasses(&numClasses, &classes[0]);

    for (i = 0; i < numClasses; i++)
    {
        suppOperClasses->classes[i+1] = classes[i];
    }
    /* add one for present operating class, added in the beginning */
    suppOperClasses->num_classes = numClasses + 1;

    return ;
}
/*
 * FUNCTION: Populate Link Identifier element IE
 *
 */


void PopulateDot11fLinkIden(tpAniSirGlobal pMac, tpPESession psessionEntry,
                                 tDot11fIELinkIdentifier *linkIden,
                                       tSirMacAddr peerMac, tANI_U8 reqType)
{
    tANI_U8 *initStaAddr = NULL ;
    tANI_U8 *respStaAddr = NULL ;

    (reqType == TDLS_INITIATOR) ? ((initStaAddr = linkIden->InitStaAddr),
                                   (respStaAddr = linkIden->RespStaAddr))
                                : ((respStaAddr = linkIden->InitStaAddr ),
                                   (initStaAddr = linkIden->RespStaAddr)) ;
    vos_mem_copy( (tANI_U8 *)linkIden->bssid,
                     (tANI_U8 *) psessionEntry->bssId, sizeof(tSirMacAddr)) ;

    vos_mem_copy( (tANI_U8 *)initStaAddr,
                          psessionEntry->selfMacAddr, sizeof(tSirMacAddr)) ;

    vos_mem_copy( (tANI_U8 *)respStaAddr, (tANI_U8 *) peerMac,
                                                       sizeof( tSirMacAddr ));

    linkIden->present = 1 ;
    return ;

}

void PopulateDot11fTdlsExtCapability(tpAniSirGlobal pMac,
                                     tpPESession psessionEntry,
                                     tDot11fIEExtCap *extCapability)
{
    struct s_ext_cap *p_ext_cap = (struct s_ext_cap *)extCapability->bytes;

    p_ext_cap->TDLSPeerPSMSupp = PEER_PSM_SUPPORT ;
    p_ext_cap->TDLSPeerUAPSDBufferSTA = pMac->lim.gLimTDLSBufStaEnabled;

    /* Set TDLS channel switching bits only if offchannel is enabled
     * and TDLS Channel Switching is not prohibited by AP in ExtCap
     * IE in assoc/re-assoc response.
     */
    if ((1== pMac->lim.gLimTDLSOffChannelEnabled) &&
        (!psessionEntry->tdls_chan_swit_prohibited)) {
        p_ext_cap->TDLSChannelSwitching = 1;
        p_ext_cap->TDLSChanSwitProhibited = 0;
    } else {
        p_ext_cap->TDLSChannelSwitching = 0;
        p_ext_cap->TDLSChanSwitProhibited = TDLS_CH_SWITCH_PROHIBITED;
    }

    p_ext_cap->TDLSSupport = TDLS_SUPPORT ;
    p_ext_cap->TDLSProhibited = TDLS_PROHIBITED ;

    extCapability->present = 1 ;
    /* For STA cases we alwasy support 11mc - Allow MAX length */
    extCapability->num_bytes = DOT11F_IE_EXTCAP_MAX_LEN;

    return ;
}


/*
 * Process Send Mgmt Request from SME and transmit to AP.
 */
tSirRetStatus limProcessSmeTdlsMgmtSendReq(tpAniSirGlobal pMac,
                                                           tANI_U32 *pMsgBuf)
{
    /* get all discovery request parameters */
    tSirTdlsSendMgmtReq *pSendMgmtReq = (tSirTdlsSendMgmtReq*) pMsgBuf ;
    tpPESession psessionEntry;
    tANI_U8      sessionId;
    tSirResultCodes resultCode = eSIR_SME_INVALID_PARAMETERS;

    limLog(pMac, LOG1, FL("Send Mgmt Recieved"));

    if((psessionEntry = peFindSessionByBssid(pMac, pSendMgmtReq->bssid, &sessionId))
            == NULL)
    {
        limLog(pMac, LOGE,
               FL("PE Session does not exist for given sme sessionId %d"),
               pSendMgmtReq->sessionId);
        goto lim_tdls_send_mgmt_error;
    }

    /* check if we are in proper state to work as TDLS client */
    if (!LIM_IS_STA_ROLE(psessionEntry)) {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                "send mgmt received in wrong system Role %d",
                GET_LIM_SYSTEM_ROLE(psessionEntry));
        goto lim_tdls_send_mgmt_error;
    }

    /*
     * if we are still good, go ahead and check if we are in proper state to
     * do TDLS discovery req/rsp/....frames.
     */
    if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
            (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE))
    {

        limLog(pMac, LOGE, FL("send mgmt received in invalid LIMsme state (%d)"),
                           psessionEntry->limSmeState);
        goto lim_tdls_send_mgmt_error;
    }
    vos_tdls_tx_rx_mgmt_event(SIR_MAC_ACTION_TDLS,
              SIR_MAC_ACTION_TX, SIR_MAC_MGMT_ACTION,
              pSendMgmtReq->reqType, pSendMgmtReq->peerMac);
    switch( pSendMgmtReq->reqType )
    {
        case SIR_MAC_TDLS_DIS_REQ:
            limLog(pMac, LOG1, FL("Transmit Discovery Request Frame"));
            /* format TDLS discovery request frame and transmit it */
            limSendTdlsDisReqFrame(pMac, pSendMgmtReq->peerMac,
                                   pSendMgmtReq->dialog,
                                   psessionEntry) ;
            resultCode = eSIR_SME_SUCCESS;
            break;
        case SIR_MAC_TDLS_DIS_RSP:
            {
                limLog(pMac, LOG1, FL("Transmit Discovery Response Frame"));
                //Send a response mgmt action frame
                limSendTdlsDisRspFrame(pMac, pSendMgmtReq->peerMac,
                        pSendMgmtReq->dialog, psessionEntry,
                        &pSendMgmtReq->addIe[0],
                        (pSendMgmtReq->length - sizeof(tSirTdlsSendMgmtReq)));
                resultCode = eSIR_SME_SUCCESS;
            }
            break;
        case SIR_MAC_TDLS_SETUP_REQ:
            {
                limLog(pMac, LOG1, FL("Transmit Setup Request Frame"));
                limSendTdlsLinkSetupReqFrame(pMac,
                        pSendMgmtReq->peerMac, pSendMgmtReq->dialog, psessionEntry,
                        &pSendMgmtReq->addIe[0], (pSendMgmtReq->length - sizeof(tSirTdlsSendMgmtReq)));
                resultCode = eSIR_SME_SUCCESS;
            }
            break;
        case SIR_MAC_TDLS_SETUP_RSP:
            {
                limLog(pMac, LOG1, FL("Transmit Setup Response Frame"));
                limSendTdlsSetupRspFrame(pMac,
                        pSendMgmtReq->peerMac, pSendMgmtReq->dialog, psessionEntry, pSendMgmtReq->statusCode,
                        &pSendMgmtReq->addIe[0], (pSendMgmtReq->length - sizeof(tSirTdlsSendMgmtReq)));
                resultCode = eSIR_SME_SUCCESS;
            }
            break;
        case SIR_MAC_TDLS_SETUP_CNF:
            {
                limLog(pMac, LOG1, FL("Transmit Setup Confirm Frame"));
                limSendTdlsLinkSetupCnfFrame(pMac, pSendMgmtReq->peerMac, pSendMgmtReq->dialog, pSendMgmtReq->peerCapability,
                        psessionEntry, &pSendMgmtReq->addIe[0], (pSendMgmtReq->length - sizeof(tSirTdlsSendMgmtReq)));
                resultCode = eSIR_SME_SUCCESS;
            }
            break;
        case SIR_MAC_TDLS_TEARDOWN:
            {
                limLog(pMac, LOG1, FL("Transmit Teardown Frame"));
                limSendTdlsTeardownFrame(pMac,
                        pSendMgmtReq->peerMac, pSendMgmtReq->statusCode, pSendMgmtReq->responder, psessionEntry,
                        &pSendMgmtReq->addIe[0], (pSendMgmtReq->length - sizeof(tSirTdlsSendMgmtReq)));
                resultCode = eSIR_SME_SUCCESS;
            }
            break;
        case SIR_MAC_TDLS_PEER_TRAFFIC_IND:
            {
            }
            break;
        case SIR_MAC_TDLS_CH_SWITCH_REQ:
            {
            }
            break;
        case SIR_MAC_TDLS_CH_SWITCH_RSP:
            {
            }
            break;
        case SIR_MAC_TDLS_PEER_TRAFFIC_RSP:
            {
            }
            break;
        default:
            break;
    }

lim_tdls_send_mgmt_error:

    limSendSmeRsp( pMac, eWNI_SME_TDLS_SEND_MGMT_RSP,
            resultCode, pSendMgmtReq->sessionId, pSendMgmtReq->transactionId);

    return eSIR_SUCCESS;
}

/*
 * Send Response to Link Establish Request to SME
 */
void limSendSmeTdlsLinkEstablishReqRsp(tpAniSirGlobal pMac,
                    tANI_U8 sessionId, tSirMacAddr peerMac, tDphHashNode   *pStaDs,
                    tANI_U8 status)
{
    tSirMsgQ  mmhMsg = {0} ;

    tSirTdlsLinkEstablishReqRsp *pTdlsLinkEstablishReqRsp = NULL ;

    pTdlsLinkEstablishReqRsp = vos_mem_malloc(sizeof(tSirTdlsLinkEstablishReqRsp));
    if ( NULL == pTdlsLinkEstablishReqRsp )
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory"));
        return ;
    }
    pTdlsLinkEstablishReqRsp->statusCode = status ;
    if ( peerMac )
    {
        vos_mem_copy(pTdlsLinkEstablishReqRsp->peerMac, peerMac, sizeof(tSirMacAddr));
    }
    pTdlsLinkEstablishReqRsp->sessionId = sessionId;
    mmhMsg.type = eWNI_SME_TDLS_LINK_ESTABLISH_RSP ;
    mmhMsg.bodyptr = pTdlsLinkEstablishReqRsp;
    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return ;


}

/*
 * Once link is teardown, send Del Peer Ind to SME
 */
static eHalStatus limSendSmeTdlsDelStaRsp(tpAniSirGlobal pMac,
                    tANI_U8 sessionId, tSirMacAddr peerMac, tDphHashNode   *pStaDs,
                    tANI_U8 status)
{
    tSirMsgQ  mmhMsg = {0} ;
    tSirTdlsDelStaRsp *pDelSta = NULL ;
    mmhMsg.type = eWNI_SME_TDLS_DEL_STA_RSP ;

    pDelSta = vos_mem_malloc(sizeof(tSirTdlsDelStaRsp));
    if ( NULL == pDelSta )
    {
        limLog(pMac, LOGE, FL("Failed to allocate memory"));
            return eSIR_FAILURE;
    }

    pDelSta->sessionId = sessionId;
    pDelSta->statusCode =  status ;
    if( pStaDs )
    {
        pDelSta->staId = pStaDs->staIndex ;
    }
    else
        pDelSta->staId = HAL_STA_INVALID_IDX;

    if( peerMac )
    {
        vos_mem_copy(pDelSta->peerMac, peerMac, sizeof(tSirMacAddr));
    }

    pDelSta->length = sizeof(tSirTdlsDelStaRsp) ;
    pDelSta->messageType = eWNI_SME_TDLS_DEL_STA_RSP ;

    mmhMsg.bodyptr = pDelSta;

    mmhMsg.bodyval = 0;
    limSysProcessMmhMsgApi(pMac, &mmhMsg, ePROT);
    return eSIR_SUCCESS ;

}

/*
 * Process Send Mgmt Request from SME and transmit to AP.
 */
tSirRetStatus limProcessSmeTdlsAddStaReq(tpAniSirGlobal pMac,
                                                           tANI_U32 *pMsgBuf)
{
    /* get all discovery request parameters */
    tSirTdlsAddStaReq *pAddStaReq = (tSirTdlsAddStaReq*) pMsgBuf ;
    tpPESession psessionEntry;
    tANI_U8      sessionId;

    limLog(pMac, LOG1, FL("TDLS Add STA Request Recieved"));

    if((psessionEntry = peFindSessionByBssid(pMac, pAddStaReq->bssid, &sessionId))
                                                                        == NULL)
    {
         limLog(pMac, LOGE,
                FL("PE Session does not exist for given sme sessionId %d"),
                pAddStaReq->sessionId);
         goto lim_tdls_add_sta_error;
    }

    /* check if we are in proper state to work as TDLS client */
    if (!LIM_IS_STA_ROLE(psessionEntry)) {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                  "send mgmt received in wrong system Role %d",
                  GET_LIM_SYSTEM_ROLE(psessionEntry));
        goto lim_tdls_add_sta_error;
    }

    /*
     * if we are still good, go ahead and check if we are in proper state to
     * do TDLS discovery req/rsp/....frames.
     */
     if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
                (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE))
     {

         limLog(pMac, LOGE,
                FL("Add STA received in invalid LIMsme state (%d)"),
                psessionEntry->limSmeState);
         goto lim_tdls_add_sta_error;
     }

     pMac->lim.gLimAddStaTdls = true ;

     /* To start with, send add STA request to HAL */
     if (eSIR_FAILURE == limTdlsSetupAddSta(pMac, pAddStaReq, psessionEntry))
     {
         limLog(pMac, LOGE, FL("Add TDLS Station request failed"));
         goto lim_tdls_add_sta_error;
     }
     return eSIR_SUCCESS;
lim_tdls_add_sta_error:
     limSendSmeTdlsAddStaRsp(pMac,
                   pAddStaReq->sessionId, pAddStaReq->peerMac,
                   (pAddStaReq->tdlsAddOper == TDLS_OPER_UPDATE), NULL, eSIR_FAILURE );

   return eSIR_SUCCESS;
}
/*
 * Process Del Sta Request from SME .
 */
tSirRetStatus limProcessSmeTdlsDelStaReq(tpAniSirGlobal pMac,
                                                           tANI_U32 *pMsgBuf)
{
    /* get all discovery request parameters */
    tSirTdlsDelStaReq *pDelStaReq = (tSirTdlsDelStaReq*) pMsgBuf ;
    tpPESession psessionEntry;
    tANI_U8      sessionId;
    tpDphHashNode pStaDs = NULL ;

    limLog(pMac, LOG1, FL("TDLS Delete STA Request Recieved"));

    if((psessionEntry = peFindSessionByBssid(pMac, pDelStaReq->bssid, &sessionId))
            == NULL)
    {
        limLog(pMac, LOGE,
               FL("PE Session does not exist for given sme sessionId %d"),
               pDelStaReq->sessionId);
        limSendSmeTdlsDelStaRsp(pMac, pDelStaReq->sessionId, pDelStaReq->peerMac,
             NULL, eSIR_FAILURE) ;
        return eSIR_FAILURE;
    }

    /* check if we are in proper state to work as TDLS client */
    if (!LIM_IS_STA_ROLE(psessionEntry)) {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                "Del sta received in wrong system Role %d",
                GET_LIM_SYSTEM_ROLE(psessionEntry));
        goto lim_tdls_del_sta_error;
    }

    /*
     * if we are still good, go ahead and check if we are in proper state to
     * do TDLS discovery req/rsp/....frames.
     */
    if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
            (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE))
    {

        limLog(pMac, LOGE, FL("Del Sta received in invalid LIMsme state (%d)"),
                           psessionEntry->limSmeState);
        goto lim_tdls_del_sta_error;
    }

    pStaDs = limTdlsDelSta(pMac, pDelStaReq->peerMac, psessionEntry) ;

    /* now send indication to SME-->HDD->TL to remove STA from TL */

    if(pStaDs)
    {
        limSendSmeTdlsDelStaRsp(pMac, psessionEntry->smeSessionId, pDelStaReq->peerMac,
                pStaDs, eSIR_SUCCESS) ;
        limReleasePeerIdx(pMac, pStaDs->assocId, psessionEntry) ;

        /* Clear the aid in peerAIDBitmap as this aid is now in freepool */
        CLEAR_PEER_AID_BITMAP(psessionEntry->peerAIDBitmap, pStaDs->assocId);
        limDeleteDphHashEntry(pMac, pStaDs->staAddr, pStaDs->assocId, psessionEntry) ;

        return eSIR_SUCCESS;

    }

lim_tdls_del_sta_error:
     limSendSmeTdlsDelStaRsp(pMac, psessionEntry->smeSessionId, pDelStaReq->peerMac,
             NULL, eSIR_FAILURE) ;

    return eSIR_SUCCESS;
}

/* Intersects the two input arrays and outputs an array */
/* For now the array length of tANI_U8 suffices */
static void limTdlsGetIntersection(tANI_U8 *input_array1,tANI_U8 input1_length,
                            tANI_U8 *input_array2,tANI_U8 input2_length,
                            tANI_U8 *output_array,tANI_U8 *output_length)
{
    tANI_U8 i,j,k=0,flag=0;

    if (input1_length > WNI_CFG_VALID_CHANNEL_LIST_LEN)
       input1_length = WNI_CFG_VALID_CHANNEL_LIST_LEN;

    for(i=0;i<input1_length;i++)
    {
        flag=0;
        for(j=0;j<input2_length;j++)
        {
            if(input_array1[i]==input_array2[j])
            {
                flag=1;
                break;
            }
        }
        if(flag==1)
        {
            output_array[k]=input_array1[i];
            k++;
        }
    }
    *output_length = k;
}
/*
 * Process Link Establishment Request from SME .
 */
tSirRetStatus limProcesSmeTdlsLinkEstablishReq(tpAniSirGlobal pMac,
                                                           tANI_U32 *pMsgBuf)
{
    /* get all discovery request parameters */
    tSirTdlsLinkEstablishReq *pTdlsLinkEstablishReq = (tSirTdlsLinkEstablishReq*) pMsgBuf ;
    tpPESession psessionEntry;
    tANI_U8      sessionId;
    tpTdlsLinkEstablishParams pMsgTdlsLinkEstablishReq;
    tSirMsgQ msg;
    tANI_U16 peerIdx = 0 ;
    tpDphHashNode pStaDs = NULL ;

    limLog(pMac, LOG1, FL("Link Establish Request Recieved")) ;

    if((psessionEntry = peFindSessionByBssid(pMac, pTdlsLinkEstablishReq->bssid, &sessionId))
            == NULL)
    {
        limLog(pMac, LOGE,
               FL("PE Session does not exist for given sme sessionId %d"),
               pTdlsLinkEstablishReq->sessionId);
        limSendSmeTdlsLinkEstablishReqRsp(pMac, pTdlsLinkEstablishReq->sessionId, pTdlsLinkEstablishReq->peerMac,
             NULL, eSIR_FAILURE) ;
        return eSIR_FAILURE;
    }

    /* check if we are in proper state to work as TDLS client */
    if (!LIM_IS_STA_ROLE(psessionEntry))
    {
        VOS_TRACE(VOS_MODULE_ID_PE, VOS_TRACE_LEVEL_ERROR,
                "TDLS Link Establish Request received in wrong system Role %d",
                GET_LIM_SYSTEM_ROLE(psessionEntry));
        goto lim_tdls_link_establish_error;
    }

    /*
     * if we are still good, go ahead and check if we are in proper state to
     * do TDLS discovery req/rsp/....frames.
     */
    if ((psessionEntry->limSmeState != eLIM_SME_ASSOCIATED_STATE) &&
            (psessionEntry->limSmeState != eLIM_SME_LINK_EST_STATE))
    {

        limLog(pMac, LOGE,
               FL("TDLS Link Establish Request received in invalid LIMsme state (%d)"),
               psessionEntry->limSmeState);
        goto lim_tdls_link_establish_error;
    }
    /*TODO Sunil , TDLSPeer Entry has the STA ID , Use it */
    pStaDs = dphLookupHashEntry(pMac, pTdlsLinkEstablishReq->peerMac, &peerIdx,
                                &psessionEntry->dph.dphHashTable) ;
    if ( NULL == pStaDs )
    {
        limLog(pMac, LOGE, FL( "pStaDs is NULL"));
        goto lim_tdls_link_establish_error;

    }
    pMsgTdlsLinkEstablishReq = vos_mem_malloc(sizeof( tTdlsLinkEstablishParams ));
    if ( NULL == pMsgTdlsLinkEstablishReq )
    {
        limLog(pMac, LOGE,
               FL("Unable to allocate memory TDLS Link Establish Request"));
        return eSIR_MEM_ALLOC_FAILED;
    }

    vos_mem_set((tANI_U8 *)pMsgTdlsLinkEstablishReq,
                sizeof(tTdlsLinkEstablishParams), 0);

    pMsgTdlsLinkEstablishReq->staIdx = pStaDs->staIndex;
    pMsgTdlsLinkEstablishReq->isResponder = pTdlsLinkEstablishReq->isResponder;
    pMsgTdlsLinkEstablishReq->uapsdQueues = pTdlsLinkEstablishReq->uapsdQueues;
    pMsgTdlsLinkEstablishReq->maxSp = pTdlsLinkEstablishReq->maxSp;
    pMsgTdlsLinkEstablishReq->isBufsta = pTdlsLinkEstablishReq->isBufSta;
    pMsgTdlsLinkEstablishReq->isOffChannelSupported =
                                pTdlsLinkEstablishReq->isOffChannelSupported;

    if ((pTdlsLinkEstablishReq->supportedChannelsLen > 0) &&
        (pTdlsLinkEstablishReq->supportedChannelsLen <=
                SIR_MAC_MAX_SUPP_CHANNELS)) {
        tANI_U32   selfNumChans = WNI_CFG_VALID_CHANNEL_LIST_LEN;
        tANI_U8    selfSupportedChannels[WNI_CFG_VALID_CHANNEL_LIST_LEN];
        if (wlan_cfgGetStr(pMac, WNI_CFG_VALID_CHANNEL_LIST,
                          selfSupportedChannels, &selfNumChans) != eSIR_SUCCESS)
        {
            /**
             * Could not get Valid channel list from CFG.
             * Log error.
             */
             limLog(pMac, LOGE,
                    FL("could not retrieve Valid channel list"));
        }

        if (selfNumChans > WNI_CFG_VALID_CHANNEL_LIST_LEN) {
             limLog(pMac, LOGE,
                   FL("Channel List more than Valid Channel list"));
             selfNumChans = WNI_CFG_VALID_CHANNEL_LIST_LEN;
        }

        if (pTdlsLinkEstablishReq->supportedChannelsLen
                                  > SIR_MAC_MAX_SUPP_CHANNELS ) {
             limLog(pMac, LOGE,
                   FL("Channel List is more than the supported Channel list"));
             pTdlsLinkEstablishReq->supportedChannelsLen
                                       = SIR_MAC_MAX_SUPP_CHANNELS;
        }

        limTdlsGetIntersection(selfSupportedChannels, selfNumChans,
                               pTdlsLinkEstablishReq->supportedChannels,
                               pTdlsLinkEstablishReq->supportedChannelsLen,
                               pMsgTdlsLinkEstablishReq->validChannels,
                               &pMsgTdlsLinkEstablishReq->validChannelsLen);
    }
    vos_mem_copy(pMsgTdlsLinkEstablishReq->validOperClasses,
                        pTdlsLinkEstablishReq->supportedOperClasses, pTdlsLinkEstablishReq->supportedOperClassesLen);
    pMsgTdlsLinkEstablishReq->validOperClassesLen =
                                pTdlsLinkEstablishReq->supportedOperClassesLen;
    msg.type = WDA_SET_TDLS_LINK_ESTABLISH_REQ;
    msg.reserved = 0;
    msg.bodyptr = pMsgTdlsLinkEstablishReq;
    msg.bodyval = 0;
    if(eSIR_SUCCESS != wdaPostCtrlMsg(pMac, &msg))
    {
        limLog(pMac, LOGE, FL("halPostMsgApi failed"));
        goto lim_tdls_link_establish_error;
    }
    return eSIR_SUCCESS;
lim_tdls_link_establish_error:
     limSendSmeTdlsLinkEstablishReqRsp(pMac, psessionEntry->smeSessionId, pTdlsLinkEstablishReq->peerMac,
                                       NULL, eSIR_FAILURE) ;

    return eSIR_SUCCESS;
}

/**
 * lim_check_aid_and_delete_peer - Funtion to check aid and delete peer
 * @p_mac: pointer to mac context
 * @session_entry: pointer to PE session
 *
 * Function verifies aid and delete's peer with that aid from hash table
 *
 * return: none
 */
static void lim_check_aid_and_delete_peer(tpAniSirGlobal p_mac,
			tpPESession session_entry)
{
	tpDphHashNode sta_ds = NULL ;
	int i, aid;

	/*
	 * Check all the set bit in peerAIDBitmap and delete the
	 * peer (with that aid) entry from the hash table and add
	 * the aid in free pool
	 */
	for (i = 0; i < sizeof(session_entry->peerAIDBitmap)/sizeof(uint32_t);
				i++) {
		for (aid = 0; aid < (sizeof(uint32_t) << 3); aid++) {
			if (!CHECK_BIT(session_entry->peerAIDBitmap[i], aid))
				continue;

			sta_ds = dphGetHashEntry(p_mac,
				    (aid + i*(sizeof(uint32_t) << 3)),
				    &session_entry->dph.dphHashTable);

			if (NULL == sta_ds)
				goto skip;

			limLog(p_mac, LOG1,
			    FL("Deleting "MAC_ADDRESS_STR),
			    MAC_ADDR_ARRAY(sta_ds->staAddr));
			limSendDeauthMgmtFrame(p_mac,
			    eSIR_MAC_DEAUTH_LEAVING_BSS_REASON,
			    sta_ds->staAddr, session_entry,
			    FALSE);
			limTdlsDelSta(p_mac, sta_ds->staAddr,
				session_entry);
			dphDeleteHashEntry(p_mac,
			    sta_ds->staAddr,
			    sta_ds->assocId,
			    &session_entry->dph.dphHashTable);
skip:
			limReleasePeerIdx(p_mac,
				(aid + i*(sizeof(uint32_t) << 3)),
				session_entry);
			CLEAR_BIT(session_entry->peerAIDBitmap[i], aid);
		}
	}
}

/* Delete all the TDLS peer connected before leaving the BSS */
tSirRetStatus limDeleteTDLSPeers(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
    if (NULL == psessionEntry)
    {
        limLog(pMac, LOGE, FL("NULL psessionEntry"));
        return eSIR_FAILURE;
    }

    lim_check_aid_and_delete_peer(pMac, psessionEntry);

    limSendSmeTDLSDeleteAllPeerInd(pMac, psessionEntry);

    return eSIR_SUCCESS;
}

/**
 * lim_process_sme_del_all_tdls_peers: process delete tdls peers
 * @p_mac: pointer to mac context
 * @msg_buf: message buffer
 *
 * Function processes request to delete tdls peers
 *
 * Return: Sucess: eSIR_SUCCESS Failure: Error value
 */
tSirRetStatus lim_process_sme_del_all_tdls_peers(tpAniSirGlobal p_mac,
				uint32_t *msg_buf)
{
	struct sir_del_all_tdls_peers *msg;
	tpPESession session_entry;
	uint8_t session_id;

	msg = (struct sir_del_all_tdls_peers *)msg_buf;
	if (msg == NULL) {
		limLog(p_mac, LOGE, FL("NULL msg"));
		return eSIR_FAILURE;
	}

	session_entry = peFindSessionByBssid(p_mac, msg->bssid, &session_id);
	if (NULL == session_entry) {
		limLog(p_mac, LOGE, FL("NULL psessionEntry"));
		return eSIR_FAILURE;
	}

	lim_check_aid_and_delete_peer(p_mac, session_entry);

	return eSIR_SUCCESS;
}

#endif
