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

#ifndef WLAN_QCT_WDA_H
#define WLAN_QCT_WDA_H

/*===========================================================================

               W L A N   DEVICE ADAPTATION   L A Y E R 
                       E X T E R N A L  A P I
                
                   
DESCRIPTION
  This file contains the external API exposed by the wlan adaptation layer for Prima
  and Volans.

  For Volans this layer is actually a thin layer that maps all WDA messages and
  functions to equivalent HAL messages and functions. The reason this layer was introduced
  was to keep the UMAC identical across Prima and Volans. This layer provides the glue 
  between SME, PE , TL and HAL.
  
  Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who          what, where, why
--------    ---         ----------------------------------------------
10/05/2011  haparna     Adding support for Keep Alive Feature
01/27/2011  rnair       Adding WDA support for Volans.
12/08/2010  seokyoun    Move down HAL interfaces from TL to WDA
                        for UMAC convergence btween Volans/Libra and Prima
08/25/2010  adwivedi    WDA Context and exposed API's 
=========================================================================== */

#include "aniGlobal.h"

#if defined( FEATURE_WLAN_INTEGRATED_SOC )

#  include "wlan_qct_wdi_ds.h"

#else 

#  include "sirParams.h"
#  include "limGlobal.h" 
#  include "halTypes.h"
#  include "wlan_qct_bal.h"
#  include "wlan_qct_hal.h" 
/* This header is for ADD and remove BA session */
#  include "halCommonApi.h"

#endif

/* Add Include */

typedef enum
{
   WDA_INIT_STATE,
   WDA_START_STATE,
   WDA_READY_STATE,
   WDA_PRE_ASSOC_STATE,
   WDA_BA_UPDATE_TL_STATE,
   WDA_BA_UPDATE_LIM_STATE,
   WDA_STOP_STATE,
   WDA_CLOSE_STATE
}WDA_state;

typedef enum
{
   WDA_PROCESS_SET_LINK_STATE,
   WDA_IGNORE_SET_LINK_STATE
}WDA_processSetLinkStateStatus;

typedef enum
{
   WDA_DISABLE_BA,
   WDA_ENABLE_BA
}WDA_BaEnableFlags;

typedef enum
{
   WDA_INVALID_STA_INDEX,
   WDA_VALID_STA_INDEX
}WDA_ValidStaIndex;

#if defined( FEATURE_WLAN_NON_INTEGRATED_SOC )
#if !defined( VOS_MQ_ID_WDA )
#define VOS_MQ_ID_WDA VOS_MQ_ID_HAL
#endif
#endif
/*--------------------------------------------------------------------------
  Utilities
 --------------------------------------------------------------------------*/

#define WDA_TLI_CEIL( _a, _b)  (( 0 != (_a)%(_b))? (_a)/(_b) + 1: (_a)/(_b))

/*
 * Check the version number and find if MCC feature is supported or not
 */
#define IS_MCC_SUPPORTED (WDA_IsWcnssWlanReportedVersionGreaterThanOrEqual( 0, 1, 1, 0))

/*--------------------------------------------------------------------------
  Definitions for Data path APIs
 --------------------------------------------------------------------------*/

/*As per 802.11 spec */
#define WDA_TLI_MGMT_FRAME_TYPE       0x00
#define WDA_TLI_CTRL_FRAME_TYPE       0x10
#define WDA_TLI_DATA_FRAME_TYPE       0x20

/*802.3 header definitions*/
#define  WDA_TLI_802_3_HEADER_LEN             14
/*802.11 header definitions - header len without QOS ctrl field*/
#define  WDA_TLI_802_11_HEADER_LEN            24

/*Determines the header len based on the disable xtl field*/
#define WDA_TLI_MAC_HEADER_LEN( _dxtl)                \
      ( ( 0 == _dxtl )?                               \
         WDA_TLI_802_3_HEADER_LEN:WDA_TLI_802_11_HEADER_LEN )

/* TX channel enum type:
      
   We have five types of TX packets so far and want to block/unblock each 
   traffic individually according to,  for example, low resouce condition. 
   Define five TX channels for UMAC here. WDA can map these logical
   channels to physical DXE channels if needed.
*/
typedef enum
{
   WDA_TXFLOW_AC_BK = 0,
   WDA_TXFLOW_AC_BE = 1,
   WDA_TXFLOW_AC_VI = 2,
   WDA_TXFLOW_AC_VO = 3,
   WDA_TXFLOW_MGMT  = 4,
   WDA_TXFLOW_BAP   = 1, /* BAP is sent as BE */
   WDA_TXFLOW_FC    = 1, /* FC is sent as BE  */
   WDA_TXFLOW_MAX
} WDA_TXFlowEnumType;

#define WDA_TXFLOWMASK  0x1F /* 1~4bit:low priority ch / 5bit: high */

/* ---------------------------------------------------------------------
   Libra and Volans specifics

   TODO Consider refactoring it and put it into two separate headers, 
   one for Prima and one for Volans 

   feturized with FEATURE_WLAN_INTEGRATED_SOC for Prima
 ----------------------------------------------------------------------*/

/* For backward compatability with SDIO. It's BAL header size for SDIO
   interface. It's nothing for integrated SOC */
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#define WDA_DXE_HEADER_SIZE   0
#else /* FEATURE_WLAN_INTEGRATED_SOC */
#define WDA_DXE_HEADER_SIZE   WLAN_BAL_DXE_HEADER_SIZE
#endif /* FEATURE_WLAN_INTEGRATED_SOC */

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#else /* FEATURE_WLAN_INTEGRATED_SOC */
/*Libra PDU size*/
#define WDA_TLI_PDU_RES_SIZE                 124
#endif /* FEATURE_WLAN_INTEGRATED_SOC */

/*Minimum resources needed - arbitrary*/

/*DXE + SD*/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#define WDA_WLAN_LIBRA_HEADER_LEN              20+8
#else /* FEATURE_WLAN_INTEGRATED_SOC */
#define WLAN_LIBRA_HEADER_LEN              20+8
#endif /* FEATURE_WLAN_INTEGRATED_SOC */

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#define WDA_TLI_BD_PDU_RESERVE_THRESHOLD    10
#else /* FEATURE_WLAN_INTEGRATED_SOC */
#define WDA_TLI_BD_PDU_RESERVE_THRESHOLD    150
#endif /* FEATURE_WLAN_INTEGRATED_SOC */


#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_TLI_MIN_RES_MF   1
#  define WDA_TLI_MIN_RES_BAP  2
#  define WDA_TLI_MIN_RES_DATA 3
#else
#  define WDA_TLI_MIN_RES_MF   13 /*Keeping for MF*/
#  define WDA_TLI_MIN_RES_BAP  WDA_TLI_MIN_RES_MF  + 13 /*Another for BAP*/
#  define WDA_TLI_MIN_RES_DATA WDA_TLI_MIN_RES_BAP + 13 /*Min 12 for data*/
#  define WLANTL_TH_RES_DATA                        254
#endif

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_NUM_STA 8
#else
#  define WDA_NUM_STA HAL_NUM_STA
#endif

/* For backward compatability with SDIO.
 
   For SDIO interface, calculate the TX frame length and number of PDU
   to transfter the frame.

   _vosBuff:   IN   VOS pakcet buffer pointer
   _usPktLen:  OUT  VOS packet length in bytes
   _uResLen:   OUT  Number of PDU to hold this VOS packet
   _uTotalPktLen: OUT Totoal packet length including BAL header size

   For integrated SOC, _usPktLen and _uTotalPktLen is VOS pakcet length
   which does include BD header length. _uResLen is hardcoded 2.
 */
#if defined( FEATURE_WLAN_INTEGRATED_SOC )

#ifdef WINDOWS_DT
#define WDA_TLI_PROCESS_FRAME_LEN( _vosBuff, _usPktLen,              \
                                            _uResLen, _uTotalPktLen) \
  do                                                                 \
  {                                                                  \
    _usPktLen = wpalPacketGetFragCount((wpt_packet*)_vosBuff) + 1/*BD*/;\
    _uResLen  = _usPktLen;                                           \
    _uTotalPktLen = _usPktLen;                                       \
  }                                                                  \
  while ( 0 )
#else /* WINDOWS_DT */
#define WDA_TLI_PROCESS_FRAME_LEN( _vosBuff, _usPktLen,              \
                                            _uResLen, _uTotalPktLen) \
  do                                                                 \
  {                                                                  \
    _usPktLen = 2;  /* Need 1 descriptor per a packet + packet*/     \
    _uResLen  = 2;  /* Assume that we spends two DXE descriptor */   \
    _uTotalPktLen = _usPktLen;                                       \
  }                                                                  \
  while ( 0 )
#endif /* WINDOWS_DT */

#else /* FEATURE_WLAN_INTEGRATED_SOC */

#define WLANTL_PDU_RES_SIZE                 124 /*Libra PDU size*/
#define WLAN_LIBRA_BD_HEADER_LEN            128 /*The lenght of the tx BD header*/

/*Minimum resources needed - arbitrary*/

/*DXE + SD*/
#define WLAN_LIBRA_HEADER_LEN              20+8

#define WLANTL_MAX_MSDU                    1538

#define WDA_TLI_PROCESS_FRAME_LEN( _vosBuff, _usPktLen,              \
                                            _uResLen, _uTotalPktLen) \
  do                                                                 \
  {                                                                  \
    vos_pkt_get_packet_length( _vosBuff, &_usPktLen );               \
    _uResLen = WDA_TLI_CEIL( (_usPktLen - WLAN_LIBRA_BD_HEADER_LEN), \
                               WLANTL_PDU_RES_SIZE ) + 1/*The BD*/;  \
    _uTotalPktLen = _usPktLen + WLAN_BAL_DXE_HEADER_SIZE;            \
  }                                                                  \
  while ( 0 )
#endif /* FEATURE_WLAN_INTEGRATED_SOC */


/*--------------------------------------------------------------------------
  Message Definitions
 --------------------------------------------------------------------------*/

/* TX Tranmit request message. It serializes TX request to TX thread.
   The message is processed in TL.
*/
#define WDA_DS_TX_START_XMIT  WLANTL_TX_START_XMIT
#define WDA_DS_FINISH_ULA     WLANTL_FINISH_ULA


#define WDA_TX_PACKET_FREED      0X0

/* Approximate amount of time to wait for WDA to stop WDI considering 1 pendig req too*/
#define WDA_STOP_TIMEOUT ( (WDI_RESPONSE_TIMEOUT * 2) + WDI_SET_POWER_STATE_TIMEOUT + 5)
/*--------------------------------------------------------------------------
  Functions
 --------------------------------------------------------------------------*/

/* For data client */
typedef VOS_STATUS (*WDA_DS_TxCompleteCallback) ( v_PVOID_t pContext, vos_pkt_t *pFrameDataBuff, VOS_STATUS txStatus );
typedef VOS_STATUS (*WDA_DS_RxPacketCallback)   ( v_PVOID_t pContext, vos_pkt_t *pFrameDataBuff );
typedef v_U32_t   (*WDA_DS_TxPacketCallback)   ( v_PVOID_t pContext, 
                                                  vos_pkt_t **ppFrameDataBuff, 
                                                  v_U32_t uSize, 
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
                                                  v_U8_t uFlowMask, 
#endif
                                                  v_BOOL_t *pbUrgent );
typedef VOS_STATUS (*WDA_DS_ResourceCB)      ( v_PVOID_t pContext, v_U32_t uCount );


#if defined( FEATURE_WLAN_INTEGRATED_SOC )
/* For management client */
typedef VOS_STATUS (*WDA_DS_TxCompleteCb)( v_PVOID_t     pContext, wpt_packet *pFrame );
typedef VOS_STATUS (*WDA_DS_RxCompleteCb)( v_PVOID_t pContext, wpt_packet *pFrame );
typedef VOS_STATUS (*WDA_DS_TxFlowControlCb)( v_PVOID_t pContext, v_U8_t acMask );
typedef void (*pWDATxRxCompFunc)( v_PVOID_t pContext, void *pData );

//callback function for TX complete
//parameter 1 - global pMac pointer
//parameter 2 - txComplete status : 1- success, 0 - failure.
typedef eHalStatus (*pWDAAckFnTxComp)(tpAniSirGlobal, tANI_U32);

typedef struct
{
   tANI_U16 ucValidStaIndex ;
   /* 
    * each bit in ucUseBaBitmap represent BA is enabled or not for this tid 
    * tid0 ..bit0, tid1..bit1 and so on..
    */
   tANI_U8    ucUseBaBitmap ;
   tANI_U8    bssIdx;
   tANI_U32   framesTxed[STACFG_MAX_TC];
}tWdaStaInfo, *tpWdaStaInfo ;

/* group all the WDA timers into this structure */
typedef struct
{
   /* BA activity check timer */
   TX_TIMER baActivityChkTmr ;

   /* Tx Complete Timeout timer */
   TX_TIMER TxCompleteTimer ;
}tWdaTimers ;

#define WDA_MAX_STA    (16)

typedef struct
{
   v_PVOID_t            pVosContext;             /* global VOSS context*/
   v_PVOID_t            pWdiContext;             /* WDI context */
   WDA_state            wdaState ;               /* WDA state tracking */ 
   v_PVOID_t            wdaMsgParam ;            /* PE parameter tracking */
   v_PVOID_t            wdaWdiApiMsgParam ;      /* WDI API paramter tracking */
   v_PVOID_t            wdaWdiCfgApiMsgParam ;   /* WDI API paramter tracking */
   vos_event_t          wdaWdiEvent;             /* WDI API sync event */

   /* Event to wait for tx completion */
   vos_event_t          txFrameEvent;

   /* call back function for tx complete*/
   pWDATxRxCompFunc     pTxCbFunc;
   /* call back function for tx packet ack */
   pWDAAckFnTxComp      pAckTxCbFunc;   
   tANI_U32             frameTransRequired;
   tSirMacAddr          macBSSID;             /*BSSID of the network */
   tSirMacAddr          macSTASelf;     /*Self STA MAC*/

   /* TX channel mask for flow control */
   v_U8_t               uTxFlowMask;
   /* TL's TX resource callback        */
   WDA_DS_ResourceCB    pfnTxResourceCB;
   /* TL's TX complete callback     */
   WDA_DS_TxCompleteCallback pfnTxCompleteCallback; 
   
   tWdaStaInfo          wdaStaInfo[WDA_MAX_STA];

   tANI_U8              wdaMaxSta;
   tWdaTimers           wdaTimers;

   /* STA, AP, IBSS, MULTI-BSS etc.*/
   tBssSystemRole       wdaGlobalSystemRole; 

   /* driver mode, PRODUCTION or FTM */
   tDriverType          driverMode;

#ifdef ANI_MANF_DIAG
   /* FTM Command Request tracking */
   v_PVOID_t            wdaFTMCmdReq;
#endif /* ANI_MANF_DIAG */

   /* Event to wait for suspend data tx*/
   vos_event_t          suspendDataTxEvent;
   /* Status frm TL after suspend/resume Tx */
   tANI_U8    txStatus;
   /* Flag set to true when TL suspend timesout.*/
   tANI_U8    txSuspendTimedOut;   

   vos_event_t          waitOnWdiIndicationCallBack;

   /* version information */
   tSirVersionType      wcnssWlanCompiledVersion;
   tSirVersionType      wcnssWlanReportedVersion;
   tSirVersionString    wcnssSoftwareVersionString;
   tSirVersionString    wcnssHardwareVersionString;

   
   tSirLinkState        linkState;
   /* set, when BT AMP session is going on */
   v_BOOL_t             wdaAmpSessionOn;
   v_U32_t              VosPacketToFree;
} tWDA_CbContext ; 

typedef struct
{
   v_PVOID_t            pWdaContext;             /* pointer to WDA context*/
   v_PVOID_t            wdaMsgParam;            /* PE parameter tracking */
   v_PVOID_t            wdaWdiApiMsgParam;      /* WDI API paramter tracking */
} tWDA_ReqParams; 

/*
 * FUNCTION: WDA_open
 * open WDA context
 */ 

VOS_STATUS WDA_open(v_PVOID_t pVosContext, v_PVOID_t pOSContext, 
                                              tMacOpenParameters *pMacParams ) ;

/*
 * FUNCTION: WDA_preStart
 * Trigger DAL-AL to start CFG download 
 */ 
VOS_STATUS WDA_start(v_PVOID_t pVosContext) ;

VOS_STATUS WDA_NVDownload_Start(v_PVOID_t pVosContext);

/*
 * FUNCTION: WDA_preStart
 * Trigger WDA to start CFG download 
 */ 
VOS_STATUS WDA_preStart(v_PVOID_t pVosContext) ;
/*
 * FUNCTION: WDA_stop
 * stop WDA
 */
VOS_STATUS WDA_stop(v_PVOID_t pVosContext,tANI_U8 reason);

/*
 * FUNCTION: WDA_close
 * close WDA context
 */
VOS_STATUS WDA_close(v_PVOID_t pVosContext);
/*
 * FUNCTION: WDA_shutdown
 * Shutdown will not close the control transport, added by SSR
 */
VOS_STATUS WDA_shutdown(v_PVOID_t pVosContext, wpt_boolean closeTransport);

/*
 * FUNCTION: WDA_McProcessMsg
 * DAL-AL message processing entry function 
 */ 

VOS_STATUS WDA_McProcessMsg( v_CONTEXT_t pVosContext, vos_msg_t *pMsg ) ;

/* -----------------------------------------------------------------
 * WDA data path API's
 * ----------------------------------------------------------------*/
/*
 * FUNCTION: WDA_MgmtDSRegister
 * Send Message back to PE
 */ 

VOS_STATUS WDA_MgmtDSRegister(tWDA_CbContext *pWDA, 
                              WDA_DS_TxCompleteCb WDA_TxCompleteCallback,
                              WDA_DS_RxCompleteCb WDA_RxCompleteCallback,  
                              WDA_DS_TxFlowControlCb WDA_TxFlowCtrlCallback 
                             ) ;
/*
 * FUNCTION: WDA_MgmtDSTxPacket
 * Forward TX management frame to WDI
 */ 

VOS_STATUS WDA_TxPacket(tWDA_CbContext *pWDA, 
                                    void *pFrmBuf,
                                    tANI_U16 frmLen,
                                    eFrameType frmType,
                                    eFrameTxDir txDir,
                                    tANI_U8 tid,
                                    pWDATxRxCompFunc pCompFunc,
                                    void *pData,
                                    pWDAAckFnTxComp pAckTxComp, 
                                    tANI_U8 txFlag);

/*
 * FUNCTION: WDA_PostMsgApi
 * API fpr PE to post Message to WDA
 */
VOS_STATUS WDA_PostMsgApi(tpAniSirGlobal pMac, tSirMsgQ *pMsg) ;

/* ---------------------------------------------------------
 * FUNCTION:  wdaGetGlobalSystemRole()
 *
 * Get the global HAL system role. 
 * ---------------------------------------------------------
 */
tBssSystemRole wdaGetGlobalSystemRole(tpAniSirGlobal pMac);

/* maximum wait time for WDA complete event (correct value has to be derived) 
 * for now giving the value 1000 ms */
#define WDA_WDI_COMPLETION_TIME_OUT 30000 /* in ms */

#define WDA_TL_TX_FRAME_TIMEOUT  10000  /* in msec a very high upper limit of 5,000 msec */
#define WDA_TL_SUSPEND_TIMEOUT   2000  /* in ms unit */

/*Tag used by WDA to mark a timed out frame*/
#define WDA_TL_TX_MGMT_TIMED_OUT   0xDEAD 

#define WDA_TL_TX_SUSPEND_SUCCESS   0
#define WDA_TL_TX_SUSPEND_FAILURE   1

// FIXME Temporary value for R33D integaration
//#define WDA_TL_TX_FRAME_TIMEOUT  20000 /* in msec a very high upper limit */

#endif /* FEATURE_WLAN_INTEGRATED_SOC */

#if defined( FEATURE_WLAN_NON_INTEGRATED_SOC )
#if !defined( wdaGetGlobalSystemRole )
#define wdaGetGlobalSystemRole halGetGlobalSystemRole
#endif
#endif

/* ---------------------------------------------------------------------------
 
   RX Meta info access for Integrated SOC
   RX BD header access for NON Integrated SOC

      These MACRO are for RX frames that are on flat buffers

  ---------------------------------------------------------------------------*/

/* WDA_GET_RX_MAC_HEADER *****************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_MAC_HEADER(pRxMeta)  \
      (tpSirMacMgmtHdr)( ((WDI_DS_RxMetaInfoType *)(pRxMeta))->mpduHeaderPtr )
#else
#  define WDA_GET_RX_MAC_HEADER(bdHd) SIR_MAC_BD_TO_MPDUHEADER(bdHd) 
#endif

/* WDA_GET_RX_MPDUHEADER3A ****************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_MPDUHEADER3A(pRxMeta) \
   (tpSirMacDataHdr3a)( ((WDI_DS_RxMetaInfoType *)(pRxMeta))->mpduHeaderPtr )
#else
#  define WDA_GET_RX_MPDUHEADER3A(bdHd)    SIR_MAC_BD_TO_MPDUHEADER3A(bdHd)
#endif

/* WDA_GET_RX_MPDU_HEADER_LEN *************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_MPDU_HEADER_LEN(pRxMeta)   \
                    ( ((WDI_DS_RxMetaInfoType *)(pRxMeta))->mpduHeaderLength )
#else 
#  define WDA_GET_RX_MPDU_HEADER_LEN(bdHd)   WLANHAL_RX_BD_GET_MPDU_H_LEN(bdHd)
#endif

/* WDA_GET_RX_MPDU_LEN ********************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_MPDU_LEN(pRxMeta)  \
               ( ((WDI_DS_RxMetaInfoType *)(pRxMeta))->mpduLength )
#else 
#  define WDA_GET_RX_MPDU_LEN(bdHd) WLANHAL_RX_BD_GET_MPDU_LEN(bdHd)
#endif

/* WDA_GET_RX_PAYLOAD_LEN ****************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_PAYLOAD_LEN(pRxMeta)   \
       ( WDA_GET_RX_MPDU_LEN(pRxMeta) - WDA_GET_RX_MPDU_HEADER_LEN(pRxMeta) )
#else
#  define WDA_GET_RX_PAYLOAD_LEN(bdHd) SIR_MAC_BD_TO_PAYLOAD_LEN(bdHd)
#endif

/* WDA_GET_RX_MAC_RATE_IDX ***************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_MAC_RATE_IDX(pRxMeta)  \
                          ( ((WDI_DS_RxMetaInfoType *)(pRxMeta))->rateIndex )
#else
#  define WDA_GET_RX_MAC_RATE_IDX(bdHd)     SIR_MAC_BD_TO_RATE_INDEX(bdHd)
#endif

/* WDA_GET_RX_MPDU_DATA ******************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_MPDU_DATA(pRxMeta)  \
                   ( ((WDI_DS_RxMetaInfoType *)(pRxMeta))->mpduDataPtr )
#else
#  define WDA_GET_RX_MPDU_DATA(bdHd) SIR_MAC_BD_TO_MPDUDATA(bdHd)
#endif

/* WDA_GET_RX_MPDU_DATA_OFFSET ***********************************************/
// For Integrated SOC: When UMAC receive the packet. BD is already stripped off.
//                     Data offset is the MPDU header length
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_MPDU_DATA_OFFSET(pRxMeta)  WDA_GET_RX_MPDU_HEADER_LEN(pRxMeta)
#else
#  define WDA_GET_RX_MPDU_DATA_OFFSET(bdHd) \
                WLANHAL_RX_BD_GET_MPDU_D_OFFSET(bdHd)
#endif

/* WDA_GET_RX_MPDU_HEADER_OFFSET *********************************************/
// For Integrated SOC: We UMAC receive the frame, 
//                     BD is gone and MAC header at offset 0
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_MPDU_HEADER_OFFSET(pRxMeta)   0
#else
#  define WDA_GET_RX_MPDU_HEADER_OFFSET(bdHd) WLANHAL_RX_BD_GET_MPDU_H_OFFSET(bdHd)
#endif

/* WDA_GET_RX_UNKNOWN_UCAST **************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_UNKNOWN_UCAST(pRxMeta)   \
                     ( ((WDI_DS_RxMetaInfoType *)(pRxMeta))->unknownUcastPkt )
#else
#  define WDA_GET_RX_UNKNOWN_UCAST(bdHd) SIR_MAC_BD_IS_UNKNOWN_UCAST_FRAME(bdHd)
#endif

/* WDA_GET_RX_TID ************************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_TID(pRxMeta) ( ((WDI_DS_RxMetaInfoType *)(pRxMeta))->tid )
#else
#  define WDA_GET_RX_TID(bdHd) WLANHAL_RX_BD_GET_TID(bdHd)
#endif

/* WDA_GET_RX_STAID **********************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_STAID(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->staId)
#else
#  define WDA_GET_RX_STAID(bdHd)    WLANHAL_RX_BD_GET_STA_ID(bdHd)
#endif

/* WDA_GET_RX_ADDR3_IDX ******************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_ADDR3_IDX(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->addr3Idx)
#else
#  define WDA_GET_RX_ADDR3_IDX(bdHd)    WLANHAL_RX_BD_GET_ADDR3_IDX(bdHd) 
#endif

/* WDA_GET_RX_CH *************************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_CH(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->rxChannel)
#else
#  define WDA_GET_RX_CH(bdHd) SIR_MAC_BD_TO_RX_CHANNEL(bdHd)
#endif

/* WDA_GET_RX_DPUSIG *********************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_DPUSIG(pRxMeta)  (((WDI_DS_RxMetaInfoType*)(pRxMeta))->dpuSig)
#else
#  define WDA_GET_RX_DPUSIG(bdHd)     WLANHAL_RX_BD_GET_DPU_SIG(bdHd)
#endif

/* WDA_IS_RX_BCAST ***********************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_IS_RX_BCAST(pRxMeta)   \
      ( (1 == ((WDI_DS_RxMetaInfoType*)(pRxMeta))->bcast) ? VOS_TRUE : VOS_FALSE )
#else
#  define WDA_IS_RX_BCAST(bdHd)   \
      ( ( 0xFF == WLANHAL_RX_BD_GET_ADDR1_IDX(bdHd)) ? VOS_TRUE : VOS_FALSE )
#endif
    
/* WDA_GET_RX_FT_DONE ********************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_FT_DONE(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->ft)
#else
#  define WDA_GET_RX_FT_DONE(bdHd)    WLANHAL_RX_BD_GET_FT(bdHd)
#endif

/* WDA_GET_RX_DPU_FEEDBACK **************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_DPU_FEEDBACK(pRxMeta) \
                      (((WDI_DS_RxMetaInfoType*)(pRxMeta))->dpuFeedback)
#endif

/* WDA_GET_RX_ASF ************************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_ASF(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->amsdu_asf)
#else
#  define WDA_GET_RX_ASF(bdHd)     WLANHAL_RX_BD_GET_ASF(bdHd)
#endif

/* WDA_GET_RX_AEF ************************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_AEF(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->amsdu_aef)
#else
#  define WDA_GET_RX_AEF(bdHd)    WLANHAL_RX_BD_GET_AEF(bdHd)
#endif

/* WDA_GET_RX_ESF ************************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_ESF(pRxMeta)  (((WDI_DS_RxMetaInfoType*)(pRxMeta))->amsdu_esf)
#else
#  define WDA_GET_RX_ESF(bdHd)     WLANHAL_RX_BD_GET_ESF(bdHd)
#endif

/* WDA_GET_RX_BEACON_SENT ****************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_BEACON_SENT(pRxMeta) \
                     (((WDI_DS_RxMetaInfoType*)(pRxMeta))->bsf)
#else
#  define WDA_GET_RX_BEACON_SENT(bdHd) SIR_MAC_BD_TO_IBSS_BCN_SENT(bdHd)
#endif

/* WDA_GET_RX_TSF_LATER *****************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_TSF_LATER(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->rtsf)
#else
#  define WDA_GET_RX_TSF_LATER(bdHd) SIR_MAC_BD_TO_IBSS_TSF_LATER(bdHd)
#endif

/* WDA_GET_RX_TYPE ***********************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_TYPE(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->type)
#else
#  define WDA_GET_RX_TYPE(bdHd) \
            ( ( WLANHAL_RX_BD_GET_TYPE_SUBTYPE(bdHd) & 0x30 ) >> 4 ) 
#endif

/* WDA_GET_RX_SUBTYPE ********************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_SUBTYPE(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->subtype)
#else
#  define WDA_GET_RX_SUBTYPE(bdHd) ( WLANHAL_RX_BD_GET_TYPE_SUBTYPE(bdHd) & 0x0F )
#endif

/* WDA_GET_RX_TYPE_SUBTYPE ****************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_TYPE_SUBTYPE(pRxMeta)  \
                 ((WDA_GET_RX_TYPE(pRxMeta)<<4)|WDA_GET_RX_SUBTYPE(pRxMeta))
#else
#  define WDA_GET_RX_TYPE_SUBTYPE(bdHd) WLANHAL_RX_BD_GET_TYPE_SUBTYPE(bdHd)
#endif

/* WDA_GET_RX_REORDER_OPCODE : For MSDU reorder *******************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_REORDER_OPCODE(pRxMeta) \
           (((WDI_DS_RxMetaInfoType*)(pRxMeta))->ampdu_reorderOpcode)
#else
#  define WDA_GET_RX_REORDER_OPCODE(bdHd) WLANHAL_RX_BD_GET_BA_OPCODE(bdHd) 
#endif

/* WDA_GET_RX_REORDER_SLOT_IDX : For MSDU reorder ****************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_REORDER_SLOT_IDX(pRxMeta) \
                (((WDI_DS_RxMetaInfoType*)(pRxMeta))->ampdu_reorderSlotIdx)
#else
#  define WDA_GET_RX_REORDER_SLOT_IDX(bdHd) WLANHAL_RX_BD_GET_BA_SI(bdHd)
#endif

/* WDA_GET_RX_REORDER_FWD_IDX : For MSDU reorder *****************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_REORDER_FWD_IDX(pRxMeta)  \
         (((WDI_DS_RxMetaInfoType*)(pRxMeta))->ampdu_reorderFwdIdx)
#else
#  define WDA_GET_RX_REORDER_FWD_IDX(bdHd) WLANHAL_RX_BD_GET_BA_FI(bdHd)
#endif

/* WDA_GET_RX_REORDER_CUR_PKT_SEQ_NO : Fro MSDU reorder **********************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_REORDER_CUR_PKT_SEQ_NO(pRxMeta)  \
         (((WDI_DS_RxMetaInfoType*)(pRxMeta))->currentPktSeqNo)
#else
#  define WDA_GET_RX_REORDER_CUR_PKT_SEQ_NO(bdHd) \
                              WLANHAL_RX_BD_GET_BA_CSN(bdHd) 
#endif

/* WDA_IS_RX_LLC_PRESENT *****************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_IS_RX_LLC_PRESENT(pRxMeta)    \
      ( (0 == ((WDI_DS_RxMetaInfoType*)(pRxMeta))->llcr) ? VOS_TRUE : VOS_FALSE )
#else
#  define WDA_IS_RX_LLC_PRESENT(bdHd)       \
           ( (0 == (v_U8_t)WLANHAL_RX_BD_GET_LLC(bdHd) ) ? VOS_TRUE : VOS_FALSE )
#endif

#define WLANWDA_HO_IS_AN_AMPDU                    0x4000
#define WLANWDA_HO_LAST_MPDU_OF_AMPDU             0x400

/* WDA_IS_RX_AN_AMPDU ********************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_IS_RX_AN_AMPDU(pRxMeta)       \
   ( ((WDI_DS_RxMetaInfoType*)(pRxMeta))->rxpFlags & WLANWDA_HO_IS_AN_AMPDU )
#else
#  define WDA_IS_RX_AN_AMPDU(bdHd)   WLAN_HAL_IS_AN_AMPDU(bdHd) 
#endif

/* WDA_IS_RX_LAST_MPDU *******************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_IS_RX_LAST_MPDU(pRxMeta)      \
   ( ((WDI_DS_RxMetaInfoType*)(pRxMeta))->rxpFlags & WLANWDA_HO_LAST_MPDU_OF_AMPDU ) 
#else
#  define WDA_IS_RX_LAST_MPDU(bdHd)         WLAN_HAL_IS_LAST_MPDU(bdHd)
#endif

/* WDA_GET_RX_TIMESTAMP *****************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_TIMESTAMP(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->mclkRxTimestamp)
#else
#  define WDA_GET_RX_TIMESTAMP(bdHd)    SIR_MAC_BD_RX_TIMESTAMP(bdHd)
#endif

/* WDA_IS_RX_IN_SCAN *********************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_IS_RX_IN_SCAN(pRxMeta)  (((WDI_DS_RxMetaInfoType*)(pRxMeta))->scan)
#else
#  define WDA_IS_RX_IN_SCAN(bdHd)     SIR_MAC_BD_TO_SCAN_LEARN(bdHd)
#endif

/* WDA_GET_RX_RSSI_DB ********************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
// Volans RF
#  define WDA_RSSI_OFFSET             100
#  define WDA_GET_RSSI0_DB(rssi0)     (rssi0 - WDA_RSSI_OFFSET)
#  define WDA_GET_RSSI1_DB(rssi0)     (0 - WDA_RSSI_OFFSET)
#  define WDA_MAX_OF_TWO(val1, val2)  ( ((val1) > (val2)) ? (val1) : (val2))
#  define WDA_GET_RSSI_DB(rssi0)  \
                WDA_MAX_OF_TWO(WDA_GET_RSSI0_DB(rssi0), WDA_GET_RSSI1_DB(rssi0))
#  define WDA_GET_RX_RSSI_DB(pRxMeta) \
                       WDA_GET_RSSI_DB((((WDI_DS_RxMetaInfoType*)(pRxMeta))->rssi0))
#else
#  define WDA_GET_RX_RSSI_DB(bdHd)     SIR_MAC_BD_TO_RSSI_DB(bdHd)
#endif

/* WDA_GET_RX_SNR ************************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_SNR(pRxMeta)  (((WDI_DS_RxMetaInfoType*)(pRxMeta))->snr)
#else
#  define WDA_GET_RX_SNR(bdHd) WLANHAL_RX_BD_GET_SNR(bdHd)
#endif

/* WDA_IS_RX_FC **************************************************************/
// Flow control frames
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
/* FIXME WDA should provide the meta info which indicates FC frame 
          In the meantime, use hardcoded FALSE, since we don't support FC yet */
#  define WDA_IS_RX_FC(pRxMeta)    (((WDI_DS_RxMetaInfoType*)(pRxMeta))->fc)
#else
#  define WDA_IS_RX_FC(bdHd)        WLANHAL_RX_BD_GET_FC(bdHd)
#endif

/* WDA_GET_RX_FC_VALID_STA_MASK **********************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_FC_VALID_STA_MASK(pRxMeta) \
                       (((WDI_DS_RxMetaInfoType*)(pRxMeta))->fcSTAValidMask)
#else
#  define WDA_GET_RX_FC_VALID_STA_MASK(bdHd) \
                       WLANHAL_RX_BD_GET_STA_VALID_MASK(bdHd)
#endif

/* WDA_GET_RX_FC_PWRSAVE_STA_MASK ********************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_FC_PWRSAVE_STA_MASK(pRxMeta) \
                 (((WDI_DS_RxMetaInfoType*)(pRxMeta))->fcSTAPwrSaveStateMask)
#else
#  define WDA_GET_RX_FC_PWRSAVE_STA_MASK(bdHd) \
                                          WLANHAL_RX_BD_GET_STA_PS_STATE(bdHd)
#endif

/* WDA_GET_RX_FC_STA_THRD_IND_MASK ********************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_FC_STA_THRD_IND_MASK(pRxMeta) \
                     (((WDI_DS_RxMetaInfoType*)(pRxMeta))->fcSTAThreshIndMask)
#else
#  define WDA_GET_RX_FC_STA_THRD_IND_MASK(bdHd) \
                                    WLANHAL_RX_BD_GET_STA_TH_IND(bdHd)
#endif

/* WDA_GET_RX_FC_FORCED_STA_TX_DISABLED_BITMAP ********************************************/
#  define WDA_GET_RX_FC_STA_TX_DISABLED_BITMAP(pRxMeta) \
                     (((WDI_DS_RxMetaInfoType*)(pRxMeta))->fcStaTxDisabledBitmap)

/* WDA_GET_RX_FC_STA_TXQ_LEN *************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_FC_STA_TXQ_LEN(pRxMeta, staId) \
                  (((WDI_DS_RxMetaInfoType*)(pRxMeta))->fcSTATxQLen[(staId)])
#else
#  define WDA_GET_RX_FC_STA_TXQ_LEN(bdHd, staId) \
                          WLANHAL_RX_BD_GET_STA_TXQ_LEN( _pvBDHeader, staId )
#endif

/* WDA_GET_RX_FC_STA_CUR_TXRATE **********************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_FC_STA_CUR_TXRATE(pRxMeta, staId) \
                (((WDI_DS_RxMetaInfoType*)(pRxMeta))->fcSTACurTxRate[(staId)])
#else
#  define WDA_GET_RX_FC_STA_CUR_TXRATE(bdHd, staId) \
                             WLANHAL_RX_BD_GET_STA_CUR_TX_RATE( bdHd, staIdx )
#endif

/* WDA_GET_RX_REPLAY_COUNT ***************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GET_RX_REPLAY_COUNT(pRxMeta) \
                            (((WDI_DS_RxMetaInfoType*)(pRxMeta))->replayCount)
#endif

/* WDA_GETRSSI0 ***************************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GETRSSI0(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->rssi0)
#else
#  define WDA_GETRSSI0(bdHd) (v_S7_t)(HAL_GET_RSSI0_DB(SIR_MAC_BD_TO_PHY_STATS0(bdHd)))
#endif

/* WDA_GETRSSI1 ***************************************************************/
#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_GETRSSI1(pRxMeta) (((WDI_DS_RxMetaInfoType*)(pRxMeta))->rssi1)
#else
#  define WDA_GETRSSI1(bdHd) (v_S7_t)(HAL_GET_RSSI1_DB(SIR_MAC_BD_TO_PHY_STATS0(bdHd)))
#endif



/* --------------------------------------------------------------------*/

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
uint8 WDA_IsWcnssWlanCompiledVersionGreaterThanOrEqual(uint8 major, uint8 minor, uint8 version, uint8 revision);
uint8 WDA_IsWcnssWlanReportedVersionGreaterThanOrEqual(uint8 major, uint8 minor, uint8 version, uint8 revision);


VOS_STATUS WDA_GetWcnssWlanCompiledVersion(v_PVOID_t pvosGCtx,
                                           tSirVersionType *pVersion);
VOS_STATUS WDA_GetWcnssWlanReportedVersion(v_PVOID_t pvosGCtx,
                                           tSirVersionType *pVersion);
VOS_STATUS WDA_GetWcnssSoftwareVersion(v_PVOID_t pvosGCtx,
                                       tANI_U8 *pVersion,
                                       tANI_U32 versionBufferSize);
VOS_STATUS WDA_GetWcnssHardwareVersion(v_PVOID_t pvosGCtx,
                                       tANI_U8 *pVersion,
                                       tANI_U32 versionBufferSize);

VOS_STATUS WDA_SetUapsdAcParamsReq(v_PVOID_t , v_U8_t , tUapsdInfo *);
VOS_STATUS WDA_ClearUapsdAcParamsReq(v_PVOID_t , v_U8_t , wpt_uint8 );
VOS_STATUS WDA_SetRSSIThresholdsReq(tpAniSirGlobal , tSirRSSIThresholds *);
// Just declare the function extern here and save some time.
extern tSirRetStatus halMmhForwardMBmsg(void*, tSirMbMsg*);
tSirRetStatus uMacPostCtrlMsg(void* pSirGlobal, tSirMbMsg* pMb);
#else
#  define uMacPostCtrlMsg(hal, msg)  halMmhForwardMBmsg(hal, msg)
#endif


#if defined(FEATURE_WLAN_NON_INTEGRATED_SOC) || defined(FEATURE_WLAN_INTEGRATED_SOC)
#define WDA_MAX_TXPOWER_INVALID HAL_MAX_TXPOWER_INVALID

//WDA Messages to HAL messages Mapping
#if 0
//Required by SME
//#define WDA_SIGNAL_BT_EVENT SIR_HAL_SIGNAL_BT_EVENT - this is defined in sirParams.h
//#define WDA_BTC_SET_CFG SIR_HAL_BTC_SET_CFG

//Required by PE
#define WDA_HOST_MSG_START SIR_HAL_HOST_MSG_START 
#define WDA_INITIAL_CAL_FAILED_NTF SIR_HAL_INITIAL_CAL_FAILED_NTF
#define WDA_SHUTDOWN_REQ SIR_HAL_SHUTDOWN_REQ
#define WDA_SHUTDOWN_CNF SIR_HAL_SHUTDOWN_CNF
#define WDA_RADIO_ON_OFF_IND SIR_HAL_RADIO_ON_OFF_IND
#define WDA_RESET_CNF SIR_HAL_RESET_CNF
#define WDA_SetRegDomain \
        eHalStatus halPhySetRegDomain(tHalHandle hHal, eRegDomainId regDomain);
#endif

#define WDA_APP_SETUP_NTF  SIR_HAL_APP_SETUP_NTF 
#define WDA_NIC_OPER_NTF   SIR_HAL_NIC_OPER_NTF
#define WDA_INIT_START_REQ SIR_HAL_INIT_START_REQ
#define WDA_RESET_REQ      SIR_HAL_RESET_REQ
#define WDA_HDD_ADDBA_REQ  SIR_HAL_HDD_ADDBA_REQ
#define WDA_HDD_ADDBA_RSP  SIR_HAL_HDD_ADDBA_RSP
#define WDA_DELETEBA_IND   SIR_HAL_DELETEBA_IND
#define WDA_BA_FAIL_IND    SIR_HAL_BA_FAIL_IND
#define WDA_TL_FLUSH_AC_REQ SIR_TL_HAL_FLUSH_AC_REQ
#define WDA_TL_FLUSH_AC_RSP SIR_HAL_TL_FLUSH_AC_RSP

#define WDA_MSG_TYPES_BEGIN            SIR_HAL_MSG_TYPES_BEGIN
#define WDA_ITC_MSG_TYPES_BEGIN        SIR_HAL_ITC_MSG_TYPES_BEGIN
#define WDA_RADAR_DETECTED_IND         SIR_HAL_RADAR_DETECTED_IND
#define WDA_WDT_KAM_RSP                SIR_HAL_WDT_KAM_RSP 
#define WDA_TIMER_TEMP_MEAS_REQ        SIR_HAL_TIMER_TEMP_MEAS_REQ
#define WDA_TIMER_PERIODIC_STATS_COLLECT_REQ   SIR_HAL_TIMER_PERIODIC_STATS_COLLECT_REQ
#define WDA_CAL_REQ_NTF                SIR_HAL_CAL_REQ_NTF
#define WDA_MNT_OPEN_TPC_TEMP_MEAS_REQ SIR_HAL_MNT_OPEN_TPC_TEMP_MEAS_REQ
#define WDA_CCA_MONITOR_INTERVAL_TO    SIR_HAL_CCA_MONITOR_INTERVAL_TO
#define WDA_CCA_MONITOR_DURATION_TO    SIR_HAL_CCA_MONITOR_DURATION_TO
#define WDA_CCA_MONITOR_START          SIR_HAL_CCA_MONITOR_START 
#define WDA_CCA_MONITOR_STOP           SIR_HAL_CCA_MONITOR_STOP
#define WDA_CCA_CHANGE_MODE            SIR_HAL_CCA_CHANGE_MODE
#define WDA_TIMER_WRAP_AROUND_STATS_COLLECT_REQ   SIR_HAL_TIMER_WRAP_AROUND_STATS_COLLECT_REQ

/*
 * New Taurus related messages
 */
#define WDA_ADD_STA_REQ                SIR_HAL_ADD_STA_REQ
#define WDA_ADD_STA_RSP                SIR_HAL_ADD_STA_RSP
#define WDA_ADD_STA_SELF_RSP           SIR_HAL_ADD_STA_SELF_RSP
#define WDA_DEL_STA_SELF_RSP           SIR_HAL_DEL_STA_SELF_RSP
#define WDA_DELETE_STA_REQ             SIR_HAL_DELETE_STA_REQ 
#define WDA_DELETE_STA_RSP             SIR_HAL_DELETE_STA_RSP
#define WDA_ADD_BSS_REQ                SIR_HAL_ADD_BSS_REQ
#define WDA_ADD_BSS_RSP                SIR_HAL_ADD_BSS_RSP
#define WDA_DELETE_BSS_REQ             SIR_HAL_DELETE_BSS_REQ
#define WDA_DELETE_BSS_RSP             SIR_HAL_DELETE_BSS_RSP
#define WDA_INIT_SCAN_REQ              SIR_HAL_INIT_SCAN_REQ
#define WDA_INIT_SCAN_RSP              SIR_HAL_INIT_SCAN_RSP
#define WDA_START_SCAN_REQ             SIR_HAL_START_SCAN_REQ
#define WDA_START_SCAN_RSP             SIR_HAL_START_SCAN_RSP
#define WDA_END_SCAN_REQ               SIR_HAL_END_SCAN_REQ
#define WDA_END_SCAN_RSP               SIR_HAL_END_SCAN_RSP
#define WDA_FINISH_SCAN_REQ            SIR_HAL_FINISH_SCAN_REQ
#define WDA_FINISH_SCAN_RSP            SIR_HAL_FINISH_SCAN_RSP
#define WDA_SEND_BEACON_REQ            SIR_HAL_SEND_BEACON_REQ
#define WDA_SEND_BEACON_RSP            SIR_HAL_SEND_BEACON_RSP

#define WDA_INIT_CFG_REQ               SIR_HAL_INIT_CFG_REQ
#define WDA_INIT_CFG_RSP               SIR_HAL_INIT_CFG_RSP

#define WDA_INIT_WM_CFG_REQ            SIR_HAL_INIT_WM_CFG_REQ
#define WDA_INIT_WM_CFG_RSP            SIR_HAL_INIT_WM_CFG_RSP

#define WDA_SET_BSSKEY_REQ             SIR_HAL_SET_BSSKEY_REQ
#define WDA_SET_BSSKEY_RSP             SIR_HAL_SET_BSSKEY_RSP
#define WDA_SET_STAKEY_REQ             SIR_HAL_SET_STAKEY_REQ
#define WDA_SET_STAKEY_RSP             SIR_HAL_SET_STAKEY_RSP
#define WDA_DPU_STATS_REQ              SIR_HAL_DPU_STATS_REQ 
#define WDA_DPU_STATS_RSP              SIR_HAL_DPU_STATS_RSP
#define WDA_GET_DPUINFO_REQ            SIR_HAL_GET_DPUINFO_REQ
#define WDA_GET_DPUINFO_RSP            SIR_HAL_GET_DPUINFO_RSP

#define WDA_UPDATE_EDCA_PROFILE_IND    SIR_HAL_UPDATE_EDCA_PROFILE_IND

#define WDA_UPDATE_STARATEINFO_REQ     SIR_HAL_UPDATE_STARATEINFO_REQ
#define WDA_UPDATE_STARATEINFO_RSP     SIR_HAL_UPDATE_STARATEINFO_RSP

#define WDA_UPDATE_BEACON_IND          SIR_HAL_UPDATE_BEACON_IND
#define WDA_UPDATE_CF_IND              SIR_HAL_UPDATE_CF_IND
#define WDA_CHNL_SWITCH_REQ            SIR_HAL_CHNL_SWITCH_REQ
#define WDA_ADD_TS_REQ                 SIR_HAL_ADD_TS_REQ
#define WDA_DEL_TS_REQ                 SIR_HAL_DEL_TS_REQ
#define WDA_SOFTMAC_TXSTAT_REPORT      SIR_HAL_SOFTMAC_TXSTAT_REPORT

#define WDA_MBOX_SENDMSG_COMPLETE_IND  SIR_HAL_MBOX_SENDMSG_COMPLETE_IND
#define WDA_EXIT_BMPS_REQ              SIR_HAL_EXIT_BMPS_REQ
#define WDA_EXIT_BMPS_RSP              SIR_HAL_EXIT_BMPS_RSP
#define WDA_EXIT_BMPS_IND              SIR_HAL_EXIT_BMPS_IND 
#define WDA_ENTER_BMPS_REQ             SIR_HAL_ENTER_BMPS_REQ
#define WDA_ENTER_BMPS_RSP             SIR_HAL_ENTER_BMPS_RSP
#define WDA_BMPS_STATUS_IND            SIR_HAL_BMPS_STATUS_IND
#define WDA_MISSED_BEACON_IND          SIR_HAL_MISSED_BEACON_IND

#define WDA_CFG_RXP_FILTER_REQ         SIR_HAL_CFG_RXP_FILTER_REQ
#define WDA_CFG_RXP_FILTER_RSP         SIR_HAL_CFG_RXP_FILTER_RSP

#define WDA_SWITCH_CHANNEL_RSP         SIR_HAL_SWITCH_CHANNEL_RSP
#define WDA_P2P_NOA_ATTR_IND           SIR_HAL_P2P_NOA_ATTR_IND
#define WDA_PWR_SAVE_CFG               SIR_HAL_PWR_SAVE_CFG

#define WDA_REGISTER_PE_CALLBACK       SIR_HAL_REGISTER_PE_CALLBACK
#define WDA_SOFTMAC_MEM_READREQUEST    SIR_HAL_SOFTMAC_MEM_READREQUEST
#define WDA_SOFTMAC_MEM_WRITEREQUEST   SIR_HAL_SOFTMAC_MEM_WRITEREQUEST

#define WDA_SOFTMAC_MEM_READRESPONSE   SIR_HAL_SOFTMAC_MEM_READRESPONSE
#define WDA_SOFTMAC_BULKREGWRITE_CONFIRM      SIR_HAL_SOFTMAC_BULKREGWRITE_CONFIRM
#define WDA_SOFTMAC_BULKREGREAD_RESPONSE      SIR_HAL_SOFTMAC_BULKREGREAD_RESPONSE
#define WDA_SOFTMAC_HOSTMESG_MSGPROCESSRESULT SIR_HAL_SOFTMAC_HOSTMESG_MSGPROCESSRESULT

#define WDA_ADDBA_REQ                  SIR_HAL_ADDBA_REQ 
#define WDA_ADDBA_RSP                  SIR_HAL_ADDBA_RSP
#define WDA_DELBA_IND                  SIR_HAL_DELBA_IND
#define WDA_DEL_BA_IND                 SIR_HAL_DEL_BA_IND
#define WDA_MIC_FAILURE_IND            SIR_HAL_MIC_FAILURE_IND

//message from sme to initiate delete block ack session.
#define WDA_DELBA_REQ                  SIR_HAL_DELBA_REQ
#define WDA_IBSS_STA_ADD               SIR_HAL_IBSS_STA_ADD
#define WDA_TIMER_ADJUST_ADAPTIVE_THRESHOLD_IND   SIR_HAL_TIMER_ADJUST_ADAPTIVE_THRESHOLD_IND
#define WDA_SET_LINK_STATE             SIR_HAL_SET_LINK_STATE
#define WDA_SET_LINK_STATE_RSP         SIR_HAL_SET_LINK_STATE_RSP
#define WDA_ENTER_IMPS_REQ             SIR_HAL_ENTER_IMPS_REQ
#define WDA_ENTER_IMPS_RSP             SIR_HAL_ENTER_IMPS_RSP
#define WDA_EXIT_IMPS_RSP              SIR_HAL_EXIT_IMPS_RSP
#define WDA_EXIT_IMPS_REQ              SIR_HAL_EXIT_IMPS_REQ
#define WDA_SOFTMAC_HOSTMESG_PS_STATUS_IND  SIR_HAL_SOFTMAC_HOSTMESG_PS_STATUS_IND  
#define WDA_POSTPONE_ENTER_IMPS_RSP    SIR_HAL_POSTPONE_ENTER_IMPS_RSP
#define WDA_STA_STAT_REQ               SIR_HAL_STA_STAT_REQ 
#define WDA_GLOBAL_STAT_REQ            SIR_HAL_GLOBAL_STAT_REQ
#define WDA_AGGR_STAT_REQ              SIR_HAL_AGGR_STAT_REQ 
#define WDA_STA_STAT_RSP               SIR_HAL_STA_STAT_RSP
#define WDA_GLOBAL_STAT_RSP            SIR_HAL_GLOBAL_STAT_RSP
#define WDA_AGGR_STAT_RSP              SIR_HAL_AGGR_STAT_RSP
#define WDA_STAT_SUMM_REQ              SIR_HAL_STAT_SUMM_REQ
#define WDA_STAT_SUMM_RSP              SIR_HAL_STAT_SUMM_RSP
#define WDA_REMOVE_BSSKEY_REQ          SIR_HAL_REMOVE_BSSKEY_REQ
#define WDA_REMOVE_BSSKEY_RSP          SIR_HAL_REMOVE_BSSKEY_RSP
#define WDA_REMOVE_STAKEY_REQ          SIR_HAL_REMOVE_STAKEY_REQ
#define WDA_REMOVE_STAKEY_RSP          SIR_HAL_REMOVE_STAKEY_RSP
#define WDA_SET_STA_BCASTKEY_REQ       SIR_HAL_SET_STA_BCASTKEY_REQ 
#define WDA_SET_STA_BCASTKEY_RSP       SIR_HAL_SET_STA_BCASTKEY_RSP
#define WDA_REMOVE_STA_BCASTKEY_REQ    SIR_HAL_REMOVE_STA_BCASTKEY_REQ
#define WDA_REMOVE_STA_BCASTKEY_RSP    SIR_HAL_REMOVE_STA_BCASTKEY_RSP
#define WDA_ADD_TS_RSP                 SIR_HAL_ADD_TS_RSP
#define WDA_DPU_MIC_ERROR              SIR_HAL_DPU_MIC_ERROR
#define WDA_TIMER_BA_ACTIVITY_REQ      SIR_HAL_TIMER_BA_ACTIVITY_REQ
#define WDA_TIMER_CHIP_MONITOR_TIMEOUT SIR_HAL_TIMER_CHIP_MONITOR_TIMEOUT
#define WDA_TIMER_TRAFFIC_ACTIVITY_REQ SIR_HAL_TIMER_TRAFFIC_ACTIVITY_REQ
#define WDA_TIMER_ADC_RSSI_STATS       SIR_HAL_TIMER_ADC_RSSI_STATS

#ifdef FEATURE_WLAN_CCX
#define WDA_TSM_STATS_REQ              SIR_HAL_TSM_STATS_REQ
#define WDA_TSM_STATS_RSP              SIR_HAL_TSM_STATS_RSP
#endif
#ifdef WLAN_SOFTAP_FEATURE
#define WDA_UPDATE_PROBE_RSP_IE_BITMAP_IND SIR_HAL_UPDATE_PROBE_RSP_IE_BITMAP_IND
#define WDA_UPDATE_UAPSD_IND           SIR_HAL_UPDATE_UAPSD_IND
#endif

#define WDA_SET_MIMOPS_REQ                      SIR_HAL_SET_MIMOPS_REQ 
#define WDA_SET_MIMOPS_RSP                      SIR_HAL_SET_MIMOPS_RSP
#define WDA_SYS_READY_IND                       SIR_HAL_SYS_READY_IND
#define WDA_SET_TX_POWER_REQ                    SIR_HAL_SET_TX_POWER_REQ
#define WDA_SET_TX_POWER_RSP                    SIR_HAL_SET_TX_POWER_RSP
#define WDA_GET_TX_POWER_REQ                    SIR_HAL_GET_TX_POWER_REQ
#define WDA_GET_TX_POWER_RSP                    SIR_HAL_GET_TX_POWER_RSP
#define WDA_GET_NOISE_REQ                       SIR_HAL_GET_NOISE_REQ 
#define WDA_GET_NOISE_RSP                       SIR_HAL_GET_NOISE_RSP
#define WDA_SET_TX_PER_TRACKING_REQ    SIR_HAL_SET_TX_PER_TRACKING_REQ

/* Messages to support transmit_halt and transmit_resume */
#define WDA_TRANSMISSION_CONTROL_IND            SIR_HAL_TRANSMISSION_CONTROL_IND
/* Indication from LIM to HAL to Initialize radar interrupt */
#define WDA_INIT_RADAR_IND                      SIR_HAL_INIT_RADAR_IND
/* Messages to support transmit_halt and transmit_resume */


#define WDA_BEACON_PRE_IND             SIR_HAL_BEACON_PRE_IND
#define WDA_ENTER_UAPSD_REQ            SIR_HAL_ENTER_UAPSD_REQ
#define WDA_ENTER_UAPSD_RSP            SIR_HAL_ENTER_UAPSD_RSP
#define WDA_EXIT_UAPSD_REQ             SIR_HAL_EXIT_UAPSD_REQ 
#define WDA_EXIT_UAPSD_RSP             SIR_HAL_EXIT_UAPSD_RSP
#define WDA_LOW_RSSI_IND               SIR_HAL_LOW_RSSI_IND 
#define WDA_BEACON_FILTER_IND          SIR_HAL_BEACON_FILTER_IND
/// PE <-> HAL WOWL messages
#define WDA_WOWL_ADD_BCAST_PTRN        SIR_HAL_WOWL_ADD_BCAST_PTRN
#define WDA_WOWL_DEL_BCAST_PTRN        SIR_HAL_WOWL_DEL_BCAST_PTRN
#define WDA_WOWL_ENTER_REQ             SIR_HAL_WOWL_ENTER_REQ
#define WDA_WOWL_ENTER_RSP             SIR_HAL_WOWL_ENTER_RSP
#define WDA_WOWL_EXIT_REQ              SIR_HAL_WOWL_EXIT_REQ
#define WDA_WOWL_EXIT_RSP              SIR_HAL_WOWL_EXIT_RSP
#define WDA_TX_COMPLETE_IND            SIR_HAL_TX_COMPLETE_IND
#define WDA_TIMER_RA_COLLECT_AND_ADAPT SIR_HAL_TIMER_RA_COLLECT_AND_ADAPT
/// PE <-> HAL statistics messages
#define WDA_GET_STATISTICS_REQ         SIR_HAL_GET_STATISTICS_REQ
#define WDA_GET_STATISTICS_RSP         SIR_HAL_GET_STATISTICS_RSP
#define WDA_SET_KEY_DONE               SIR_HAL_SET_KEY_DONE

/// PE <-> HAL BTC messages
#define WDA_BTC_SET_CFG                SIR_HAL_BTC_SET_CFG
#define WDA_SIGNAL_BT_EVENT            SIR_HAL_SIGNAL_BT_EVENT
#define WDA_HANDLE_FW_MBOX_RSP         SIR_HAL_HANDLE_FW_MBOX_RSP
#define WDA_UPDATE_PROBE_RSP_TEMPLATE_IND     SIR_HAL_UPDATE_PROBE_RSP_TEMPLATE_IND
#define WDA_SIGNAL_BTAMP_EVENT         SIR_HAL_SIGNAL_BTAMP_EVENT

#ifdef ANI_CHIPSET_VOLANS
#endif

#define WDA_SET_MAX_TX_POWER_REQ       SIR_HAL_SET_MAX_TX_POWER_REQ
#define WDA_SET_MAX_TX_POWER_RSP       SIR_HAL_SET_MAX_TX_POWER_RSP

#define WDA_SEND_MSG_COMPLETE          SIR_HAL_SEND_MSG_COMPLETE 

/// PE <-> HAL Host Offload message
#define WDA_SET_HOST_OFFLOAD           SIR_HAL_SET_HOST_OFFLOAD

/// PE <-> HAL Keep Alive message
#define WDA_SET_KEEP_ALIVE             SIR_HAL_SET_KEEP_ALIVE

#ifdef WLAN_NS_OFFLOAD
#define WDA_SET_NS_OFFLOAD             SIR_HAL_SET_NS_OFFLOAD
#endif //WLAN_NS_OFFLOAD
#define WDA_ADD_STA_SELF_REQ           SIR_HAL_ADD_STA_SELF_REQ
#define WDA_DEL_STA_SELF_REQ           SIR_HAL_DEL_STA_SELF_REQ

#ifdef WLAN_FEATURE_P2P
#define WDA_SET_P2P_GO_NOA_REQ         SIR_HAL_SET_P2P_GO_NOA_REQ
#endif

#define WDA_TX_COMPLETE_TIMEOUT_IND  WDA_MSG_TYPES_END - 1
#define WDA_WLAN_SUSPEND_IND           SIR_HAL_WLAN_SUSPEND_IND
#define WDA_WLAN_RESUME_REQ           SIR_HAL_WLAN_RESUME_REQ
#define WDA_MSG_TYPES_END    SIR_HAL_MSG_TYPES_END

#define WDA_SUSPEND_ACTIVITY_RSP SIR_HAL_SUSPEND_ACTIVITY_RSP


#define WDA_MMH_TXMB_READY_EVT SIR_HAL_MMH_TXMB_READY_EVT     
#define WDA_MMH_RXMB_DONE_EVT  SIR_HAL_MMH_RXMB_DONE_EVT    
#define WDA_MMH_MSGQ_NE_EVT    SIR_HAL_MMH_MSGQ_NE_EVT
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R
#define WDA_AGGR_QOS_REQ               SIR_HAL_AGGR_QOS_REQ
#define WDA_AGGR_QOS_RSP               SIR_HAL_AGGR_QOS_RSP
#endif /* WLAN_FEATURE_VOWIFI_11R */

#ifdef ANI_MANF_DIAG
/* FTM CMD MSG */
#define WDA_FTM_CMD_REQ        SIR_PTT_MSG_TYPES_BEGIN
#define WDA_FTM_CMD_RSP        SIR_PTT_MSG_TYPES_END
#endif /* ANI_MANF_DIAG */

#ifdef FEATURE_WLAN_SCAN_PNO
/*Requests sent to lower driver*/
#define WDA_SET_PNO_REQ             SIR_HAL_SET_PNO_REQ
#define WDA_SET_RSSI_FILTER_REQ     SIR_HAL_SET_RSSI_FILTER_REQ
#define WDA_UPDATE_SCAN_PARAMS_REQ  SIR_HAL_UPDATE_SCAN_PARAMS

/*Indication comming from lower driver*/
#define WDA_SET_PNO_CHANGED_IND     SIR_HAL_SET_PNO_CHANGED_IND
#endif // FEATURE_WLAN_SCAN_PNO

#ifdef WLAN_WAKEUP_EVENTS
#define WDA_WAKE_REASON_IND    SIR_HAL_WAKE_REASON_IND  
#endif // WLAN_WAKEUP_EVENTS

#ifdef WLAN_FEATURE_PACKET_FILTERING
#define WDA_8023_MULTICAST_LIST_REQ                     SIR_HAL_8023_MULTICAST_LIST_REQ
#define WDA_RECEIVE_FILTER_SET_FILTER_REQ               SIR_HAL_RECEIVE_FILTER_SET_FILTER_REQ
#define WDA_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ    SIR_HAL_PACKET_COALESCING_FILTER_MATCH_COUNT_REQ
#define WDA_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP    SIR_HAL_PACKET_COALESCING_FILTER_MATCH_COUNT_RSP
#define WDA_RECEIVE_FILTER_CLEAR_FILTER_REQ             SIR_HAL_RECEIVE_FILTER_CLEAR_FILTER_REQ   
#endif // WLAN_FEATURE_PACKET_FILTERING

#define WDA_SET_POWER_PARAMS_REQ   SIR_HAL_SET_POWER_PARAMS_REQ

#ifdef WLAN_FEATURE_GTK_OFFLOAD
#define WDA_GTK_OFFLOAD_REQ             SIR_HAL_GTK_OFFLOAD_REQ
#define WDA_GTK_OFFLOAD_GETINFO_REQ     SIR_HAL_GTK_OFFLOAD_GETINFO_REQ
#define WDA_GTK_OFFLOAD_GETINFO_RSP     SIR_HAL_GTK_OFFLOAD_GETINFO_RSP
#endif //WLAN_FEATURE_GTK_OFFLOAD

#define WDA_SET_TM_LEVEL_REQ       SIR_HAL_SET_TM_LEVEL_REQ

#ifdef FEATURE_WLAN_INTEGRATED_SOC
tSirRetStatus wdaPostCtrlMsg(tpAniSirGlobal pMac, tSirMsgQ *pMsg);
#endif

#if defined(FEATURE_WLAN_NON_INTEGRATED_SOC)
#define VOS_MODULE_ID_WDA VOS_MODULE_ID_HAL

//Required by TL

//WDA Functions to HAL functions Mapping
//Required by SME and PE
#define WDA_SetRegDomain halPhySetRegDomain 
#define wdaPostCtrlMsg halPostMsgApi
#else
eHalStatus WDA_SetRegDomain(void * clientCtxt, v_REGDOMAIN_t regId);
#endif //FEATURE_WLAN_NON_INTEGRATED_SOC

#define HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME 0x40 // Bit 6 will be used to control BD rate for Management frames

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#define halTxFrame(hHal, pFrmBuf, frmLen, frmType, txDir, tid, pCompFunc, pData, txFlag) \
   (eHalStatus)( WDA_TxPacket(\
         vos_get_context(VOS_MODULE_ID_WDA, vos_get_global_context(VOS_MODULE_ID_WDA, (hHal))),\
         (pFrmBuf),\
         (frmLen),\
         (frmType),\
         (txDir),\
         (tid),\
         (pCompFunc),\
         (pData),\
         (NULL), \
         (txFlag)) )

#define halTxFrameWithTxComplete(hHal, pFrmBuf, frmLen, frmType, txDir, tid, pCompFunc, pData, pCBackFnTxComp, txFlag) \
   (eHalStatus)( WDA_TxPacket(\
         vos_get_context(VOS_MODULE_ID_WDA, vos_get_global_context(VOS_MODULE_ID_WDA, (hHal))),\
         (pFrmBuf),\
         (frmLen),\
         (frmType),\
         (txDir),\
         (tid),\
         (pCompFunc),\
         (pData),\
         (pCBackFnTxComp), \
         (txFlag)) )
#endif

/* -----------------------------------------------------------------
  WDA data path API's for TL
 -------------------------------------------------------------------*/

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
v_BOOL_t WDA_IsHwFrameTxTranslationCapable(v_PVOID_t pVosGCtx, 
                                                      tANI_U8 staIdx);
#else
#  define WDA_IsHwFrameTxTranslationCapable(vosGCtx, staId) \
      WLANHAL_IsHwFrameTxTranslationCapable(vosGCtx, staId)
#endif

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_EnableUapsdAcParams(vosGCtx, staId, uapsdInfo) \
         WDA_SetUapsdAcParamsReq(vosGCtx, staId, uapsdInfo)
#else
#  define WDA_EnableUapsdAcParams(vosGCtx, staId, uapsdInfo) \
         WLANHAL_EnableUapsdAcParams(vosGCtx, staId, uapsdInfo)
#endif

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_DisableUapsdAcParams(vosGCtx, staId, ac) \
          WDA_ClearUapsdAcParamsReq(vosGCtx, staId, ac)
#else
#  define WDA_DisableUapsdAcParams(vosGCtx, staId, ac) \
         WLANHAL_DisableUapsdAcParams(vosGCtx, staId, ac)
#endif

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#  define WDA_SetRSSIThresholds(pMac, pThresholds) \
         WDA_SetRSSIThresholdsReq(pMac, pThresholds)
#else
#  define WDA_SetRSSIThresholds(pMac, pThresholds) \
         halPS_SetRSSIThresholds(pMac, pThresholds)
#endif

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
#define WDA_UpdateRssiBmps(pvosGCtx,  staId, rssi) \
        WLANTL_UpdateRssiBmps (pvosGCtx, staId, rssi)
#endif

#ifdef WLAN_PERF 
/*==========================================================================
  FUNCTION    WDA_TLI_FastHwFwdDataFrame

  DESCRIPTION 
    For NON integrated SOC, this function is called by TL.

    Fast path function to quickly forward a data frame if HAL determines BD 
    signature computed here matches the signature inside current VOSS packet. 
    If there is a match, HAL and TL fills in the swapped packet length into 
    BD header and DxE header, respectively. Otherwise, packet goes back to 
    normal (slow) path and a new BD signature would be tagged into BD in this
    VOSS packet later by the WLANHAL_FillTxBd() function.

  TODO  For integrated SOC, this function does nothing yet. Pima SLM/HAL 
        should provide the equivelant functionality.

  DEPENDENCIES 
     
  PARAMETERS 

   IN
        pvosGCtx    VOS context
        vosDataBuff Ptr to VOSS packet
        pMetaInfo   For getting frame's TID
        pStaInfo    For checking STA type
    
   OUT
        pvosStatus  returned status
        puFastFwdOK Flag to indicate whether frame could be fast forwarded
   
  RETURN VALUE
    No return.   

  SIDE EFFECTS 
  
============================================================================*/
void WDA_TLI_FastHwFwdDataFrame
(
  v_PVOID_t     pvosGCtx,
  vos_pkt_t*    vosDataBuff,
  VOS_STATUS*   pvosStatus,
  v_U32_t*       puFastFwdOK,
  WLANTL_MetaInfoType*  pMetaInfo,
  WLAN_STADescType*  pStaInfo
);
#endif /* WLAN_PERF */

/*==========================================================================
  FUNCTION    WDA_DS_Register

  DESCRIPTION 
    Register TL client to WDA. This function registers TL RX/TX functions
    to WDI by calling WDI_DS_Register.


    For NON integrated SOC, this function calls WLANBAL_RegTlCbFunctions
    to register TL's RX/TX functions to BAL

  TODO 
    For Prima, pfnResourceCB gets called in WDTS_OOResourceNotification.
    The uCount parameter is AC mask. It should be redefined to use the
    same resource callback function.

  DEPENDENCIES 
     
  PARAMETERS 

   IN
        pvosGCtx    VOS context
        pfnTxCompleteCallback       TX complete callback upon TX completion
        pfnRxPacketCallback         RX callback
        pfnResourceCB               gets called when updating TX PDU number
        uResTheshold                minimum TX PDU size for a packet
        pCallbackContext            WDI calls callback function with it
                                    VOS global context pointer
   OUT
        uAvailableTxBuf       available TX PDU numbder. 
                              BAL returns it for NON integrated SOC
   
  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS 
WDA_DS_Register 
( 
  v_PVOID_t                 pvosGCtx, 
  WDA_DS_TxCompleteCallback pfnTxCompleteCallback,
  WDA_DS_RxPacketCallback   pfnRxPacketCallback, 
  WDA_DS_TxPacketCallback   pfnTxPacketCallback,
  WDA_DS_ResourceCB         pfnResourceCB,
  v_U32_t                   uResTheshold,
  v_PVOID_t                 pCallbackContext,
  v_U32_t                   *uAvailableTxBuf
);

/*==========================================================================
  FUNCTION    WDA_DS_StartXmit

  DESCRIPTION 
    Serialize TX transmit reques to TX thread. 

  TODO This sends TX transmit request to TL. It should send to WDI for
         abstraction.

    For NON integrated SOC, this function calls WLANBAL_StartXmit

  DEPENDENCIES 
     
  PARAMETERS 

   IN
        pvosGCtx    VOS context
   
  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WDA_DS_StartXmit
(
  v_PVOID_t pvosGCtx
);

/*==========================================================================
  FUNCTION    WDA_DS_FinishULA

  DESCRIPTION 
    Serialize Finish Upper Level Authentication reques to TX thread. 

  DEPENDENCIES 
     
  PARAMETERS 

   IN
        callbackRoutine    routine to be called in TX thread
        callbackContext    user data for the above routine 
   
  RETURN VALUE
    please see vos_tx_mq_serialize

  SIDE EFFECTS 
  
============================================================================*/
VOS_STATUS
WDA_DS_FinishULA
(
 void (*callbackRoutine) (void *callbackContext),
 void  *callbackContext
);

/*==========================================================================
   FUNCTION    WDA_DS_BuildTxPacketInfo

  DESCRIPTION
    Build TX meta info for integrated SOC.
    
    Same function calls HAL for reserve BD header space into VOS packet and
    HAL function to fill it.
    
  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context
    vosDataBuff      vos data buffer
    pvDestMacAddr   destination MAC address ponter
    ucDisableFrmXtl  Is frame xtl disabled?
    ucQosEnabled     Is QoS enabled?
    ucWDSEnabled     Is WDS enabled?
    extraHeadSpace   Extra head bytes. If it's not 0 due to 4 bytes align
                     of BD header.
    typeSubtype      typeSubtype from MAC header or TX metainfo/BD
    pAddr2           address 2
    uTid             tid
    txFlag
    timeStamp
    ucIsEapol
    ucUP

   OUT
    *pusPktLen       Packet length

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_BuildTxPacketInfo
(
  v_PVOID_t       pvosGCtx,
  vos_pkt_t*      vosDataBuff,
  v_MACADDR_t*    pvDestMacAddr,
  v_U8_t          ucDisableFrmXtl,
  v_U16_t*        pusPktLen,
  v_U8_t          ucQosEnabled,
  v_U8_t          ucWDSEnabled,
  v_U8_t          extraHeadSpace,
  v_U8_t          typeSubtype,
  v_PVOID_t       pAddr2,
  v_U8_t          uTid,
  v_U8_t          txFlag,
  v_U32_t         timeStamp,
  v_U8_t          ucIsEapol,
  v_U8_t          ucUP
);

/*==========================================================================
   FUNCTION    WDA_DS_PeekRxPacketInfo

  DESCRIPTION
    Return RX metainfo pointer for for integrated SOC.
    
    Same function will return BD header pointer.
    
  DEPENDENCIES

  PARAMETERS

   IN
    vosDataBuff      vos data buffer

    pvDestMacAddr    destination MAC address ponter
    bSwap            Want to swap BD header? For backward compatability
                     It does nothing for integrated SOC
   OUT
    *ppRxHeader      RX metainfo pointer

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_PeekRxPacketInfo
(
  vos_pkt_t *vosDataBuff,
  v_PVOID_t *ppRxHeader,
  v_BOOL_t  bSwap
);

/*==========================================================================
   FUNCTION    WDA_DS_TrimRxPacketInfo

  DESCRIPTION
    Trim/Remove RX BD header for NON integrated SOC.
    It does nothing for integrated SOC.
    
  DEPENDENCIES

  PARAMETERS

   IN
    vosDataBuff      vos data buffer

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_TrimRxPacketInfo
( 
  vos_pkt_t *vosDataBuff
);

/*==========================================================================
   FUNCTION    WDA_DS_GetTxResources

  DESCRIPTION
    It does return hardcoded value for Prima. It should bigger number than 0.
    Returning 0 will put TL in out-of-resource condition for TX.

    Return current PDU resources from BAL for NON integrated SOC.
    
  DEPENDENCIES

  PARAMETERS

   IN
    vosDataBuff      vos data buffer
   
   OUT
    puResCount        available PDU number for TX

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_GetTxResources
( 
  v_PVOID_t pvosGCtx,
  v_U32_t*  puResCount
);

/*==========================================================================
   FUNCTION    WDA_DS_GetRssi

  DESCRIPTION
    Get RSSI 

  TODO It returns hardcoded value in the meantime since WDA/WDI does nothing
       support it yet for Prima.

  DEPENDENCIES

  PARAMETERS

   IN
    vosDataBuff      vos data buffer

   OUT
    puRssi           RSSI

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_GetRssi
(
  v_PVOID_t pvosGCtx,
  v_S7_t*   puRssi
);

/*==========================================================================
   FUNCTION    WDA_DS_RxAmsduBdFix

  DESCRIPTION
    For backward compatability with Libra/Volans. Need to call HAL function
    for HW BD bug fix

    It does nothing for integrated SOC.

  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context
    pvBDHeader       BD header pointer

   OUT

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_RxAmsduBdFix
(
  v_PVOID_t pvosGCtx,
  v_PVOID_t pvBDHeader
);

/*==========================================================================
   FUNCTION    WDA_DS_GetFrameTypeSubType

  DESCRIPTION
    Get typeSubtype from the packet. The BD header should have this.
    But some reason, Libra/Volans read it from 802.11 header and save it
    back to BD header. So for NON integrated SOC, this function does
    the same.

    For integrated SOC, WDI does the same, not TL. 
    It does return typeSubtype from RX meta info for integrated SOC.

  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context
    vosDataBuff      vos data buffer
    pRxHeader        RX meta info or BD header pointer

   OUT
    ucTypeSubtype    typeSubtype

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_GetFrameTypeSubType
(
  v_PVOID_t pvosGCtx,
  vos_pkt_t *vosDataBuff,
  v_PVOID_t pRxHeader,
  v_U8_t    *ucTypeSubtype
);

/*==========================================================================
   FUNCTION    WDA_DS_GetReplayCounter

  DESCRIPTION
    Return replay counter from BD header or RX meta info

  DEPENDENCIES

  PARAMETERS

   IN
    pRxHeader        RX meta info or BD header pointer

   OUT

  RETURN VALUE
    Replay Counter

  SIDE EFFECTS

============================================================================*/
v_U64_t
WDA_DS_GetReplayCounter
(
  v_PVOID_t pRxHeader
);

/*==========================================================================
   FUNCTION    WDA_DS_GetReplayCounter

  DESCRIPTION
    HO support. Set RSSI threshold via HAL function for NON integrated SOC

  TODO
    Same function should be provided by WDA/WDI for Prima.

  DEPENDENCIES

  PARAMETERS

   IN
    pMac             MAC global pointer
    pThresholds      pointer of threshold structure to set.

   OUT

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_SetRSSIThresholds
(
  tpAniSirGlobal      pMac,
  tpSirRSSIThresholds pThresholds
);

#if defined( FEATURE_WLAN_INTEGRATED_SOC )
/*==========================================================================
   FUNCTION    WDA_DS_TxFrames

  DESCRIPTION
    Pull packets from TL and push them to WDI. It gets invoked upon
    WDA_DS_TX_START_XMIT.

    This function is equivelant of WLANSSC_Transmit in Libra/Volans.

  TODO
    This function should be implemented and moved in WDI.

  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context

   OUT

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_TxFrames
( 
  v_PVOID_t pvosGCtx 
);
#endif

/*==========================================================================
   FUNCTION    WDA_DS_TxFlowControlCallback

  DESCRIPTION
    Invoked by WDI to control TX flow.

  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context
    uFlowMask        TX channel mask for flow control
                     Defined in WDA_TXFlowEnumType

   OUT

  RETURN VALUE

  SIDE EFFECTS

============================================================================*/
v_VOID_t
WDA_DS_TxFlowControlCallback
(
 v_PVOID_t pvosGCtx,
 v_U8_t    uFlowMask
);

/*==========================================================================
   FUNCTION    WDA_DS_GetTxFlowMask

  DESCRIPTION
    return TX flow mask control value

  DEPENDENCIES

  PARAMETERS

   IN
    pvosGCtx         VOS context

   OUT
    uFlowMask        TX channel mask for flow control
                     Defined in WDA_TXFlowEnumType

  RETURN VALUE
    VOS_STATUS_E_INVAL:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS
WDA_DS_GetTxFlowMask
(
 v_PVOID_t pvosGCtx,
 v_U8_t*   puFlowMask
);

/*==========================================================================
   FUNCTION    WDA_HALDumpCmdReq

  DESCRIPTION
    Send Dump commandsto WDI
    
  DEPENDENCIES

  PARAMETERS

   IN
    pMac             MAC global pointer
    cmd               Hal dump command
    arg1              Dump command argument 1
    arg2              Dump command argument 2
    arg3              Dump command argument 3
    arg4              Dump command argument 4

   OUT
       pBuffer          Dump command Response buffer

  RETURN VALUE
    VOS_STATUS_E_FAULT:  pointer is NULL and other errors 
    VOS_STATUS_SUCCESS:  Everything is good :)

  SIDE EFFECTS

============================================================================*/
VOS_STATUS WDA_HALDumpCmdReq(tpAniSirGlobal   pMac,tANI_U32 cmd, 
                 tANI_U32   arg1, tANI_U32   arg2, tANI_U32   arg3,
                 tANI_U32   arg4, tANI_U8   *pBuffer);

/*==========================================================================
   FUNCTION    WDA_featureCapsExchange

  DESCRIPTION
    WDA API to invoke capability exchange between host and FW

  DEPENDENCIES

  PARAMETERS

   IN
    pVosContext         VOS context

   OUT
    NONE

  RETURN VALUE
    NONE
    
  SIDE EFFECTS
============================================================================*/
void WDA_featureCapsExchange(v_PVOID_t pVosContext);

/*==========================================================================
   FUNCTION    WDA_getHostWlanFeatCaps

  DESCRIPTION
    Wrapper for WDI API, that will return if the feature (enum value).passed
    to this API is supported or not in Host

  DEPENDENCIES

  PARAMETERS

   IN
    featEnumValue     enum value for the feature as in placeHolderInCapBitmap in wlan_hal_msg.h.

   OUT
    NONE

  RETURN VALUE
    0 - implies feature is NOT Supported
    any non zero value - implies feature is SUPPORTED
    
  SIDE EFFECTS
============================================================================*/
tANI_U8 WDA_getHostWlanFeatCaps(tANI_U8 featEnumValue);

/*==========================================================================
   FUNCTION    WDA_getFwWlanFeatCaps

  DESCRIPTION
    Wrapper for WDI API, that will return if the feature (enum value).passed
    to this API is supported or not in FW

  DEPENDENCIES

  PARAMETERS

   IN
    featEnumValue     enum value for the feature as in placeHolderInCapBitmap in wlan_hal_msg.h.

   OUT
    NONE

  RETURN VALUE
    0 - implies feature is NOT Supported
    any non zero value - implies feature is SUPPORTED
    
  SIDE EFFECTS
============================================================================*/
tANI_U8 WDA_getFwWlanFeatCaps(tANI_U8 featEnumValue);

#endif
