/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

#include "wma_api.h"
#include "wma_stub.h"
#include "i_vos_packet.h"

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
typedef enum
{
  eWDA_AUTH_TYPE_NONE,    //never used
  // MAC layer authentication types
  eWDA_AUTH_TYPE_OPEN_SYSTEM,
  // Upper layer authentication types
  eWDA_AUTH_TYPE_WPA,
  eWDA_AUTH_TYPE_WPA_PSK,

  eWDA_AUTH_TYPE_RSN,
  eWDA_AUTH_TYPE_RSN_PSK,
  eWDA_AUTH_TYPE_FT_RSN,
  eWDA_AUTH_TYPE_FT_RSN_PSK,
  eWDA_AUTH_TYPE_WAPI_WAI_CERTIFICATE,
  eWDA_AUTH_TYPE_WAPI_WAI_PSK,
  eWDA_AUTH_TYPE_CCKM_WPA,
  eWDA_AUTH_TYPE_CCKM_RSN,
  eWDA_AUTH_TYPE_WPA_NONE,
  eWDA_AUTH_TYPE_AUTOSWITCH,
  eWDA_AUTH_TYPE_SHARED_KEY,
  eWDA_NUM_OF_SUPPORT_AUTH_TYPE,
  eWDA_AUTH_TYPE_FAILED = 0xff,
  eWDA_AUTH_TYPE_UNKNOWN = eCSR_AUTH_TYPE_FAILED,
}WDA_AuthType;

#ifdef FEATURE_WLAN_TDLS
typedef enum
{
  WDA_TDLS_PEER_STATE_PEERING,
  WDA_TDLS_PEER_STATE_CONNECTED,
  WDA_TDLS_PEER_STATE_TEARDOWN,
} WDA_TdlsPeerState;
/* WMI_TDLS_SET_OFFCHAN_MODE_CMDID */
typedef enum
{
  WDA_TDLS_ENABLE_OFFCHANNEL,
  WDA_TDLS_DISABLE_OFFCHANNEL
}WDA_TdlsOffchanMode;
#endif /* FEATURE_WLAN_TDLS */

/*--------------------------------------------------------------------------
  Utilities
 --------------------------------------------------------------------------*/

#define WDA_TLI_CEIL( _a, _b)  (( 0 != (_a)%(_b))? (_a)/(_b) + 1: (_a)/(_b))


#define IS_MCC_SUPPORTED 1
#define IS_FEATURE_SUPPORTED_BY_FW(feat_enum_value) wma_getFwWlanFeatCaps(feat_enum_value)

#ifdef WLAN_ACTIVEMODE_OFFLOAD_FEATURE
#define IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE 1
#else
#define IS_ACTIVEMODE_OFFLOAD_FEATURE_ENABLE 0
#endif

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
#define IS_ROAM_SCAN_OFFLOAD_FEATURE_ENABLE 1
#else
#define IS_ROAM_SCAN_OFFLOAD_FEATURE_ENABLE 0
#endif

#define IS_IBSS_HEARTBEAT_OFFLOAD_FEATURE_ENABLE 1

#ifdef FEATURE_WLAN_TDLS
#define IS_ADVANCE_TDLS_ENABLE 0
#endif


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
 ----------------------------------------------------------------------*/

/* For backward compatability with SDIO. It's BAL header size for SDIO
   interface. It's nothing for integrated SOC */
#define WDA_DXE_HEADER_SIZE   0


/*Minimum resources needed - arbitrary*/

/*DXE + SD*/
#define WDA_WLAN_LIBRA_HEADER_LEN              (20 + 8)

#define WDA_TLI_BD_PDU_RESERVE_THRESHOLD    10


#  define WDA_TLI_MIN_RES_MF   1
#  define WDA_TLI_MIN_RES_BAP  2
#  define WDA_TLI_MIN_RES_DATA 3

#  define WDA_NUM_STA 8

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



/*--------------------------------------------------------------------------
  Message Definitions
 --------------------------------------------------------------------------*/

/* TX Tranmit request message. It serializes TX request to TX thread.
   The message is processed in TL.
*/
#define WDA_DS_TX_START_XMIT  WLANTL_TX_START_XMIT
#define WDA_DS_FINISH_ULA     WLANTL_FINISH_ULA

#define VOS_TO_WPAL_PKT(_vos_pkt) ((wpt_packet*)_vos_pkt)

#define WDA_TX_PACKET_FREED      0X0

/* Approximate amount of time to wait for WDA to stop WDI considering 1 pendig req too*/
#define WDA_STOP_TIMEOUT ( (WDI_RESPONSE_TIMEOUT * 2) + WDI_SET_POWER_STATE_TIMEOUT + 5)
/*--------------------------------------------------------------------------
  Functions
 --------------------------------------------------------------------------*/
typedef void (*pWDATxRxCompFunc)( v_PVOID_t pContext, void *pData,
                                  v_BOOL_t bFreeData );

//callback function for TX complete
//parameter 1 - global pMac pointer
//parameter 2 - txComplete status : 1- success, 0 - failure.
typedef eHalStatus (*pWDAAckFnTxComp)(tpAniSirGlobal, tANI_U32);

/* generic callback for updating parameters from target to UMAC */
typedef void (*wda_tgt_cfg_cb) (void *context, void *param);

/*
 * callback for Indicating Radar to HDD and disable Tx Queues
 * to stop accepting data Tx packets from netif as radar is
 * found on the current operating channel
 */
typedef void (*wda_dfs_radar_indication_cb) (void *context, void *param);

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

   /* Traffic Stats timer */
   TX_TIMER trafficStatsTimer ;
}tWdaTimers ;
#ifdef WLAN_SOFTAP_VSTA_FEATURE
#define WDA_MAX_STA    (41)
#else
#define WDA_MAX_STA    (16)
#endif
typedef struct
{
   v_PVOID_t            pVosContext;             /* global VOSS context*/
   v_PVOID_t            pWdiContext;             /* WDI context */
   WDA_state            wdaState ;               /* WDA state tracking */
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


   tWdaStaInfo          wdaStaInfo[WDA_MAX_STA];

   tANI_U8              wdaMaxSta;
   tWdaTimers           wdaTimers;

   /* driver mode, PRODUCTION or FTM */
   tDriverType          driverMode;

   /* FTM Command Request tracking */
   v_PVOID_t            wdaFTMCmdReq;

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
   v_BOOL_t             needShutdown;
   v_BOOL_t             wdiFailed;
   v_BOOL_t             wdaTimersCreated;

   /* Event to wait for WDA stop on FTM mode */
   vos_event_t          ftmStopDoneEvent;

} tWDA_CbContext ; 

typedef struct
{
   v_PVOID_t            pWdaContext;             /* pointer to WDA context*/
   v_PVOID_t            wdaMsgParam;            /* PE parameter tracking */
   v_PVOID_t            wdaWdiApiMsgParam;      /* WDI API paramter tracking */
} tWDA_ReqParams;

typedef struct {
   v_UINT_t param_id;
   v_UINT_t param_value;
   v_UINT_t param_sec_value;
   v_UINT_t param_vdev_id;
   v_UINT_t param_vp_dev;
} wda_cli_set_cmd_t;

/*
 * FUNCTION: WDA_MgmtDSTxPacket
 * Forward TX management frame to WDI
 */

VOS_STATUS WDA_TxPacket(void *pWDA,
                        void *pFrmBuf,
                        tANI_U16 frmLen,
                        eFrameType frmType,
                        eFrameTxDir txDir,
                        tANI_U8 tid,
                        pWDATxRxCompFunc pCompFunc,
                        void *pData,
                        pWDAAckFnTxComp pAckTxComp,
                        tANI_U8 txFlag,
                        tANI_U8 sessionId,
                        bool tdlsflag);

/*
 * FUNCTION: WDA_open
 * open WDA context
 */

VOS_STATUS WDA_open(v_PVOID_t pVosContext, v_PVOID_t pOSContext,
                          wda_tgt_cfg_cb pTgtUpdCB,
                          wda_dfs_radar_indication_cb radar_ind_cb,
                          tMacOpenParameters *pMacParams ) ;

#define WDA_start wma_start
#define WDA_MapChannel wma_map_channel

#define WDA_NVDownload_Start(x)    ({ VOS_STATUS_SUCCESS; })

#define WDA_preStart wma_pre_start
#define WDA_stop wma_stop
#define WDA_close wma_close
#define WDA_shutdown wma_shutdown
#define WDA_setNeedShutdown wma_setneedshutdown
#define WDA_needShutdown wma_needshutdown
#define WDA_McProcessMsg wma_mc_process_msg

#define DPU_FEEDBACK_UNPROTECTED_ERROR 0x0F


#define WDA_GET_RX_MAC_HEADER(pRxMeta) \
     (tpSirMacMgmtHdr)(((t_packetmeta *)pRxMeta)->mpdu_hdr_ptr)

#define WDA_GET_RX_MPDUHEADER3A(pRxMeta) \
     (tpSirMacDataHdr3a)(((t_packetmeta *)pRxMeta)->mpdu_hdr_ptr)

#define WDA_GET_RX_MPDU_HEADER_LEN(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->mpdu_hdr_len)

#define WDA_GET_RX_MPDU_LEN(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->mpdu_len)

#define WDA_GET_RX_PAYLOAD_LEN(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->mpdu_data_len)

#define WDA_GET_RX_TSF_DELTA(pRxMeta) \
    (((t_packetmeta *)pRxMeta)->tsf_delta)

#define WDA_GET_RX_MAC_RATE_IDX(pRxMeta) 0

#define WDA_GET_RX_MPDU_DATA(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->mpdu_data_ptr)

#define WDA_GET_RX_MPDU_HEADER_OFFSET(pRxMeta) 0

#define WDA_GET_RX_UNKNOWN_UCAST(pRxMeta) 0

#define WDA_GET_RX_CH(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->channel)

#define WDA_IS_RX_BCAST(pRxMeta) 0

#define WDA_GET_RX_FT_DONE(pRxMeta) 0

#define WDA_GET_RX_DPU_FEEDBACK(pRxMeta) \
	(((t_packetmeta *)pRxMeta)->dpuFeedback)

#define WDA_GET_RX_BEACON_SENT(pRxMeta) 0

#define WDA_GET_RX_TSF_LATER(pRxMeta) 0

#define WDA_GET_RX_TIMESTAMP(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->timestamp)

#define WDA_IS_RX_IN_SCAN(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->scan)

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
#define WDA_GET_OFFLOADSCANLEARN(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->offloadScanLearn)
#define WDA_GET_ROAMCANDIDATEIND(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->roamCandidateInd)
#define WDA_GET_SESSIONID(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->sessionId)
#define WMA_GET_SCAN_SRC(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->scan_src)
#endif

#ifdef FEATURE_WLAN_EXTSCAN
#define WMA_IS_EXTSCAN_SCAN_SRC(pRxMeta) \
     ((((t_packetmeta *)pRxMeta)->scan_src) & WMI_MGMT_RX_HDR_EXTSCAN)
#define WMA_IS_EPNO_SCAN_SRC(pRxMeta) \
     ((((t_packetmeta *)pRxMeta)->scan_src) & WMI_MGMT_RX_HDR_ENLO)
#endif /* FEATURE_WLAN_EXTSCAN */

#define WDA_GET_RX_SNR(pRxMeta) \
     (((t_packetmeta *)pRxMeta)->snr)

#define WDA_GetWcnssWlanCompiledVersion    WMA_GetWcnssWlanCompiledVersion
#define WDA_GetWcnssWlanReportedVersion WMA_GetWcnssWlanReportedVersion
#define WDA_GetWcnssSoftwareVersion WMA_GetWcnssSoftwareVersion
#define WDA_GetWcnssHardwareVersion WMA_GetWcnssHardwareVersion

#define WDA_GET_RX_RFBAND(pRxMeta) 0


tSirRetStatus uMacPostCtrlMsg(void* pSirGlobal, tSirMbMsg* pMb);


#define WDA_MAX_TXPOWER_INVALID HAL_MAX_TXPOWER_INVALID

/* rssi value normalized to noise floor of -96 dBm */
#define WDA_GET_RX_RSSI_NORMALIZED(pRxMeta) \
                       (((t_packetmeta *)pRxMeta)->rssi)

/* raw rssi based on actual noise floor in hardware */
#define WDA_GET_RX_RSSI_RAW(pRxMeta) \
                       (((t_packetmeta *)pRxMeta)->rssi_raw)

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
    (eHalStatus halPhySetRegDomain(tHalHandle hHal, eRegDomainId regDomain))
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
#define WDA_SEND_PROBE_RSP_TMPL        SIR_HAL_SEND_PROBE_RSP_TMPL

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
#define WDA_P2P_NOA_START_IND          SIR_HAL_P2P_NOA_START_IND
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
#define WDA_TIMER_TRAFFIC_STATS_IND    SIR_HAL_TRAFFIC_STATS_IND

#ifdef WLAN_FEATURE_11W
#define WDA_EXCLUDE_UNENCRYPTED_IND    SIR_HAL_EXCLUDE_UNENCRYPTED_IND
#endif

#ifdef FEATURE_WLAN_ESE
#define WDA_TSM_STATS_REQ              SIR_HAL_TSM_STATS_REQ
#define WDA_TSM_STATS_RSP              SIR_HAL_TSM_STATS_RSP
#endif
#define WDA_UPDATE_PROBE_RSP_IE_BITMAP_IND SIR_HAL_UPDATE_PROBE_RSP_IE_BITMAP_IND
#define WDA_UPDATE_UAPSD_IND           SIR_HAL_UPDATE_UAPSD_IND

#define WDA_SET_MIMOPS_REQ                      SIR_HAL_SET_MIMOPS_REQ
#define WDA_SET_MIMOPS_RSP                      SIR_HAL_SET_MIMOPS_RSP
#define WDA_SYS_READY_IND                       SIR_HAL_SYS_READY_IND
#define WDA_SET_TX_POWER_REQ                    SIR_HAL_SET_TX_POWER_REQ
#define WDA_SET_TX_POWER_RSP                    SIR_HAL_SET_TX_POWER_RSP
#define WDA_GET_TX_POWER_REQ                    SIR_HAL_GET_TX_POWER_REQ
#define WDA_GET_NOISE_REQ                       SIR_HAL_GET_NOISE_REQ
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
#define WDA_SIGNAL_BTAMP_EVENT         SIR_HAL_SIGNAL_BTAMP_EVENT

#ifdef FEATURE_OEM_DATA_SUPPORT
/* PE <-> HAL OEM_DATA RELATED MESSAGES */
#define WDA_START_OEM_DATA_REQ         SIR_HAL_START_OEM_DATA_REQ
#define WDA_START_OEM_DATA_RSP         SIR_HAL_START_OEM_DATA_RSP
#define WDA_FINISH_OEM_DATA_REQ        SIR_HAL_FINISH_OEM_DATA_REQ
#endif

#define WDA_SET_MAX_TX_POWER_REQ       SIR_HAL_SET_MAX_TX_POWER_REQ
#define WDA_SET_MAX_TX_POWER_RSP       SIR_HAL_SET_MAX_TX_POWER_RSP
#define WDA_SET_TX_POWER_REQ           SIR_HAL_SET_TX_POWER_REQ

#define WDA_SET_MAX_TX_POWER_PER_BAND_REQ \
        SIR_HAL_SET_MAX_TX_POWER_PER_BAND_REQ
#define WDA_SET_MAX_TX_POWER_PER_BAND_RSP \
        SIR_HAL_SET_MAX_TX_POWER_PER_BAND_RSP

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

#define WDA_SET_P2P_GO_NOA_REQ         SIR_HAL_SET_P2P_GO_NOA_REQ
#define WDA_SET_TDLS_LINK_ESTABLISH_REQ SIR_HAL_TDLS_LINK_ESTABLISH_REQ
#define WDA_SET_TDLS_LINK_ESTABLISH_REQ_RSP SIR_HAL_TDLS_LINK_ESTABLISH_REQ_RSP

#define WDA_TX_COMPLETE_TIMEOUT_IND  (WDA_MSG_TYPES_END - 1)
#define WDA_WLAN_SUSPEND_IND           SIR_HAL_WLAN_SUSPEND_IND
#define WDA_WLAN_RESUME_REQ           SIR_HAL_WLAN_RESUME_REQ
#define WDA_MSG_TYPES_END    SIR_HAL_MSG_TYPES_END

#define WDA_MMH_TXMB_READY_EVT SIR_HAL_MMH_TXMB_READY_EVT
#define WDA_MMH_RXMB_DONE_EVT  SIR_HAL_MMH_RXMB_DONE_EVT
#define WDA_MMH_MSGQ_NE_EVT    SIR_HAL_MMH_MSGQ_NE_EVT

#ifdef WLAN_FEATURE_VOWIFI_11R
#define WDA_AGGR_QOS_REQ               SIR_HAL_AGGR_QOS_REQ
#define WDA_AGGR_QOS_RSP               SIR_HAL_AGGR_QOS_RSP
#endif /* WLAN_FEATURE_VOWIFI_11R */

/* FTM CMD MSG */
#define WDA_FTM_CMD_REQ        SIR_PTT_MSG_TYPES_BEGIN
#define WDA_FTM_CMD_RSP        SIR_PTT_MSG_TYPES_END
#define WDA_CSA_OFFLOAD_EVENT  SIR_CSA_OFFLOAD_EVENT

#ifdef FEATURE_WLAN_SCAN_PNO
/*Requests sent to lower driver*/
#define WDA_SET_PNO_REQ             SIR_HAL_SET_PNO_REQ
#define WDA_SET_RSSI_FILTER_REQ     SIR_HAL_SET_RSSI_FILTER_REQ
#define WDA_UPDATE_SCAN_PARAMS_REQ  SIR_HAL_UPDATE_SCAN_PARAMS

/*Indication comming from lower driver*/
#define WDA_SET_PNO_CHANGED_IND     SIR_HAL_SET_PNO_CHANGED_IND
#endif // FEATURE_WLAN_SCAN_PNO

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
#define WDA_SET_PLM_REQ             SIR_HAL_SET_PLM_REQ
#endif

#ifdef WLAN_FEATURE_ROAM_SCAN_OFFLOAD
#define WDA_ROAM_SCAN_OFFLOAD_REQ   SIR_HAL_ROAM_SCAN_OFFLOAD_REQ
#define WDA_ROAM_SCAN_OFFLOAD_RSP   SIR_HAL_ROAM_SCAN_OFFLOAD_RSP
#define WDA_START_ROAM_CANDIDATE_LOOKUP_REQ             SIR_HAL_START_ROAM_CANDIDATE_LOOKUP_REQ
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#define WDA_ROAM_OFFLOAD_SYNCH_CNF  SIR_HAL_ROAM_OFFLOAD_SYNCH_CNF
#define WDA_ROAM_OFFLOAD_SYNCH_IND  SIR_HAL_ROAM_OFFLOAD_SYNCH_IND
#define WDA_ROAM_OFFLOAD_SYNCH_FAIL SIR_HAL_ROAM_OFFLOAD_SYNCH_FAIL
#endif
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
#define WDA_DHCP_START_IND              SIR_HAL_DHCP_START_IND
#define WDA_DHCP_STOP_IND               SIR_HAL_DHCP_STOP_IND

#define WDA_HIDDEN_SSID_VDEV_RESTART    SIR_HAL_HIDE_SSID_VDEV_RESTART

#ifdef WLAN_FEATURE_GTK_OFFLOAD
#define WDA_GTK_OFFLOAD_REQ             SIR_HAL_GTK_OFFLOAD_REQ
#define WDA_GTK_OFFLOAD_GETINFO_REQ     SIR_HAL_GTK_OFFLOAD_GETINFO_REQ
#define WDA_GTK_OFFLOAD_GETINFO_RSP     SIR_HAL_GTK_OFFLOAD_GETINFO_RSP
#endif //WLAN_FEATURE_GTK_OFFLOAD

#define WDA_SET_TM_LEVEL_REQ       SIR_HAL_SET_TM_LEVEL_REQ

#ifdef WLAN_FEATURE_11AC
#define WDA_UPDATE_OP_MODE         SIR_HAL_UPDATE_OP_MODE
#define WDA_UPDATE_RX_NSS          SIR_HAL_UPDATE_RX_NSS
#define WDA_UPDATE_MEMBERSHIP      SIR_HAL_UPDATE_MEMBERSHIP
#define WDA_UPDATE_USERPOS         SIR_HAL_UPDATE_USERPOS
#endif

#define WDA_GET_ROAM_RSSI_REQ      SIR_HAL_GET_ROAM_RSSI_REQ
#define WDA_GET_ROAM_RSSI_RSP      SIR_HAL_GET_ROAM_RSSI_RSP

#ifdef WLAN_FEATURE_NAN
#define WDA_NAN_REQUEST            SIR_HAL_NAN_REQUEST
#endif

#define WDA_START_SCAN_OFFLOAD_REQ  SIR_HAL_START_SCAN_OFFLOAD_REQ
#define WDA_START_SCAN_OFFLOAD_RSP  SIR_HAL_START_SCAN_OFFLOAD_RSP
#define WDA_STOP_SCAN_OFFLOAD_REQ  SIR_HAL_STOP_SCAN_OFFLOAD_REQ
#define WDA_STOP_SCAN_OFFLOAD_RSP  SIR_HAL_STOP_SCAN_OFFLOAD_RSP
#define WDA_UPDATE_CHAN_LIST_REQ    SIR_HAL_UPDATE_CHAN_LIST_REQ
#define WDA_UPDATE_CHAN_LIST_RSP    SIR_HAL_UPDATE_CHAN_LIST_RSP
#define WDA_RX_SCAN_EVENT           SIR_HAL_RX_SCAN_EVENT
#define WDA_IBSS_PEER_INACTIVITY_IND SIR_HAL_IBSS_PEER_INACTIVITY_IND

#define WDA_CLI_SET_CMD             SIR_HAL_CLI_SET_CMD
#define WDA_CLI_GET_CMD             SIR_HAL_CLI_GET_CMD
#ifdef FEATURE_WLAN_SCAN_PNO
#define WDA_SME_SCAN_CACHE_UPDATED  SIR_HAL_SME_SCAN_CACHE_UPDATED
#endif

#ifndef REMOVE_PKT_LOG
#define WDA_PKTLOG_ENABLE_REQ       SIR_HAL_PKTLOG_ENABLE_REQ
#endif

#ifdef FEATURE_WLAN_LPHB
#define WDA_LPHB_CONF_REQ          SIR_HAL_LPHB_CONF_IND
#define WDA_LPHB_WAIT_EXPIRE_IND   SIR_HAL_LPHB_WAIT_EXPIRE_IND
#endif /* FEATURE_WLAN_LPHB */

#ifdef FEATURE_WLAN_CH_AVOID
#define WDA_CH_AVOID_UPDATE_REQ    SIR_HAL_CH_AVOID_UPDATE_REQ
#endif /* FEATURE_WLAN_CH_AVOID */

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
#define WDA_SET_AUTO_SHUTDOWN_TIMER_REQ SIR_HAL_SET_AUTO_SHUTDOWN_TIMER_REQ
#endif

#define WDA_ADD_PERIODIC_TX_PTRN_IND    SIR_HAL_ADD_PERIODIC_TX_PTRN_IND
#define WDA_DEL_PERIODIC_TX_PTRN_IND    SIR_HAL_DEL_PERIODIC_TX_PTRN_IND

#define WDA_TX_POWER_LIMIT          SIR_HAL_SET_TX_POWER_LIMIT

#define WDA_RATE_UPDATE_IND         SIR_HAL_RATE_UPDATE_IND

#define WDA_INIT_THERMAL_INFO_CMD   SIR_HAL_INIT_THERMAL_INFO_CMD
#define WDA_SET_THERMAL_LEVEL       SIR_HAL_SET_THERMAL_LEVEL

#ifdef FEATURE_WLAN_TDLS
#define WDA_UPDATE_FW_TDLS_STATE      SIR_HAL_UPDATE_FW_TDLS_STATE
#define WDA_UPDATE_TDLS_PEER_STATE    SIR_HAL_UPDATE_TDLS_PEER_STATE
#define WDA_TDLS_SHOULD_DISCOVER      SIR_HAL_TDLS_SHOULD_DISCOVER
#define WDA_TDLS_SHOULD_TEARDOWN      SIR_HAL_TDLS_SHOULD_TEARDOWN
#define WDA_TDLS_PEER_DISCONNECTED    SIR_HAL_TDLS_PEER_DISCONNECTED
#define WDA_TDLS_SET_OFFCHAN_MODE     SIR_HAL_TDLS_SET_OFFCHAN_MODE
#endif
#define WDA_SET_SAP_INTRABSS_DIS      SIR_HAL_SET_SAP_INTRABSS_DIS

/* Message to Indicate Radar Presence on SAP Channel */
#define WDA_DFS_RADAR_IND           SIR_HAL_DFS_RADAR_IND

/* Message to indicate beacon tx completion after beacon template update
 * beacon offload case
 */
#define WDA_DFS_BEACON_TX_SUCCESS_IND   SIR_HAL_BEACON_TX_SUCCESS_IND
#define WDA_FW_STATS_IND           SIR_HAL_FW_STATS_IND
#define WDA_DISASSOC_TX_COMP       SIR_HAL_DISASSOC_TX_COMP
#define WDA_DEAUTH_TX_COMP         SIR_HAL_DEAUTH_TX_COMP
#define WDA_GET_LINK_SPEED         SIR_HAL_GET_LINK_SPEED

#define WDA_MODEM_POWER_STATE_IND SIR_HAL_MODEM_POWER_STATE_IND

#define WDA_VDEV_STOP_IND           SIR_HAL_VDEV_STOP_IND

#ifdef WLAN_FEATURE_STATS_EXT
#define WDA_STATS_EXT_REQUEST              SIR_HAL_STATS_EXT_REQUEST
#endif

#define WDA_VDEV_START_RSP_IND      SIR_HAL_VDEV_START_RSP_IND

#define WDA_ROAM_PREAUTH_IND        SIR_HAL_ROAM_PREAUTH_IND

#define WDA_TBTT_UPDATE_IND         SIR_HAL_TBTT_UPDATE_IND

#ifdef FEATURE_WLAN_EXTSCAN
#define WDA_EXTSCAN_GET_CAPABILITIES_REQ    SIR_HAL_EXTSCAN_GET_CAPABILITIES_REQ
#define WDA_EXTSCAN_START_REQ               SIR_HAL_EXTSCAN_START_REQ
#define WDA_EXTSCAN_STOP_REQ                SIR_HAL_EXTSCAN_STOP_REQ
#define WDA_EXTSCAN_SET_BSSID_HOTLIST_REQ   SIR_HAL_EXTSCAN_SET_BSS_HOTLIST_REQ
#define WDA_EXTSCAN_RESET_BSSID_HOTLIST_REQ SIR_HAL_EXTSCAN_RESET_BSS_HOTLIST_REQ
#define WDA_EXTSCAN_SET_SIGNF_CHANGE_REQ    SIR_HAL_EXTSCAN_SET_SIGNF_CHANGE_REQ
#define WDA_EXTSCAN_RESET_SIGNF_CHANGE_REQ  SIR_HAL_EXTSCAN_RESET_SIGNF_CHANGE_REQ
#define WDA_EXTSCAN_GET_CACHED_RESULTS_REQ  SIR_HAL_EXTSCAN_GET_CACHED_RESULTS_REQ
#define WDA_SET_EPNO_LIST_REQ               SIR_HAL_SET_EPNO_LIST_REQ
#define WDA_SET_PASSPOINT_LIST_REQ          SIR_HAL_SET_PASSPOINT_LIST_REQ
#define WDA_RESET_PASSPOINT_LIST_REQ        SIR_HAL_RESET_PASSPOINT_LIST_REQ
#define WDA_EXTSCAN_SET_SSID_HOTLIST_REQ    SIR_HAL_EXTSCAN_SET_SSID_HOTLIST_REQ
#define WDA_EXTSCAN_STATUS_IND              SIR_HAL_EXTSCAN_STATUS_IND
#define WDA_EXTSCAN_OPERATION_IND           SIR_HAL_EXTSCAN_OPERATION_IND

#endif /* FEATURE_WLAN_EXTSCAN */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
#define WDA_LINK_LAYER_STATS_CLEAR_REQ        SIR_HAL_LL_STATS_CLEAR_REQ
#define WDA_LINK_LAYER_STATS_SET_REQ          SIR_HAL_LL_STATS_SET_REQ
#define WDA_LINK_LAYER_STATS_GET_REQ          SIR_HAL_LL_STATS_GET_REQ
#define WDA_LINK_LAYER_STATS_RESULTS_RSP      SIR_HAL_LL_STATS_RESULTS_RSP
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

#define WDA_LINK_STATUS_GET_REQ SIR_HAL_LINK_STATUS_GET_REQ
#define WDA_GET_LINK_STATUS_RSP_IND SIR_HAL_GET_LINK_STATUS_RSP_IND

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
#define WDA_WLAN_EXT_WOW                      SIR_HAL_CONFIG_EXT_WOW
#define WDA_WLAN_SET_APP_TYPE1_PARAMS         SIR_HAL_CONFIG_APP_TYPE1_PARAMS
#define WDA_WLAN_SET_APP_TYPE2_PARAMS         SIR_HAL_CONFIG_APP_TYPE2_PARAMS
#endif

#define WDA_SET_SCAN_MAC_OUI_REQ              SIR_HAL_SET_SCAN_MAC_OUI_REQ

#ifdef FEATURE_RUNTIME_PM
#define WDA_RUNTIME_PM_SUSPEND_IND            SIR_HAL_RUNTIME_PM_SUSPEND_IND
#define WDA_RUNTIME_PM_RESUME_IND             SIR_HAL_RUNTIME_PM_RESUME_IND
#endif

#define WDA_FW_MEM_DUMP_REQ                   SIR_HAL_FW_MEM_DUMP_REQ
#define WDA_SET_RSSI_MONITOR_REQ              SIR_HAL_SET_RSSI_MONITOR_REQ

#define WDA_SET_IE_INFO                       SIR_HAL_SET_IE_INFO

tSirRetStatus wdaPostCtrlMsg(tpAniSirGlobal pMac, tSirMsgQ *pMsg);

#define HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME 0x40 // Bit 6 will be used to control BD rate for Management frames

#define halTxFrame(hHal, pFrmBuf, frmLen, frmType, txDir, tid, pCompFunc,\
                   pData, txFlag, sessionid) \
   (eHalStatus)( WDA_TxPacket(\
         vos_get_context(VOS_MODULE_ID_WDA,\
                         vos_get_global_context(VOS_MODULE_ID_WDA, (hHal))),\
         (pFrmBuf),\
         (frmLen),\
         (frmType),\
         (txDir),\
         (tid),\
         (pCompFunc),\
         (pData),\
         (NULL), \
         (txFlag),\
         (sessionid),\
         (false)) )

#define halTxFrameWithTxComplete(hHal, pFrmBuf, frmLen, frmType, txDir, tid,\
                         pCompFunc, pData, pCBackFnTxComp, txFlag, sessionid, tdlsflag) \
   (eHalStatus)( WDA_TxPacket(\
         vos_get_context(VOS_MODULE_ID_WDA,\
                         vos_get_global_context(VOS_MODULE_ID_WDA, (hHal))),\
         (pFrmBuf),\
         (frmLen),\
         (frmType),\
         (txDir),\
         (tid),\
         (pCompFunc),\
         (pData),\
         (pCBackFnTxComp), \
         (txFlag),\
         (sessionid),\
         (tdlsflag)) )


#define WDA_SetRegDomain WMA_SetRegDomain
#define WDA_SetHTConfig wma_set_htconfig
#define WDA_UpdateRssiBmps WMA_UpdateRssiBmps

VOS_STATUS WDA_SetIdlePsConfig(void *wda_handle, tANI_U32 idle_ps);
VOS_STATUS WDA_notify_modem_power_state(void *wda_handle, tANI_U32 value);
static inline void WDA_UpdateSnrBmps(v_PVOID_t pvosGCtx, v_U8_t staId,
            v_S7_t snr)
{

}

static inline int WDA_GetSnr(tANI_U8 ucSTAId, tANI_S8* pSnr)
{
     return VOS_STATUS_SUCCESS;
}

static inline void WDA_UpdateLinkCapacity(v_PVOID_t pvosGCtx, v_U8_t staId,
           v_U32_t linkCapacity)
{

}

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


#define WDA_HALDumpCmdReq WMA_HALDumpCmdReq

#define WDA_featureCapsExchange WMA_featureCapsExchange
#define WDA_disableCapablityFeature WMA_disableCapablityFeature
#define WDA_getFwWlanFeatCaps wma_getFwWlanFeatCaps

#define WDA_TransportChannelDebug(mac, disp_snapshot, \
                                  toggle_stall_detect) ({ \
                                  (void)mac;              \
                                  (void)disp_snapshot;    \
                                  (void)toggle_stall_detect; \
})

#define WDA_TrafficStatsTimerActivate WMA_TrafficStatsTimerActivate
#define WDA_SetEnableSSR(enable_ssr) (void)enable_ssr
void WDA_TxAbort(v_U8_t vdev_id);


/* Powersave Offload Changes */
typedef struct sUapsd_Params
{
    tANI_U8 bkDeliveryEnabled:1;
    tANI_U8 beDeliveryEnabled:1;
    tANI_U8 viDeliveryEnabled:1;
    tANI_U8 voDeliveryEnabled:1;
    tANI_U8 bkTriggerEnabled:1;
    tANI_U8 beTriggerEnabled:1;
    tANI_U8 viTriggerEnabled:1;
    tANI_U8 voTriggerEnabled:1;
}tUapsd_Params, *tpUapsd_Params;

/* Enable PowerSave Params */
typedef struct sEnablePsParams
{
    tSirAddonPsReq psSetting;

    tUapsd_Params uapsdParams;

    tSirMacAddr bssid;

    /* SmeSession Id or Vdev Id */
    tANI_U32 sessionid;

    /* Beacon DTIM Period */
    tANI_U8 bcnDtimPeriod;

    /* success or failure */
    tANI_U32   status;
}tEnablePsParams, *tpEnablePsParams;

/* Disable PowerSave Params */
typedef struct sDisablePsParams
{
    tSirAddonPsReq psSetting;

    tSirMacAddr bssid;

    /* SmeSession Id or Vdev Id */
    tANI_U32 sessionid;

    /* success or failure */
    tANI_U32   status;
}tDisablePsParams, *tpDisablePsParams;

/* Enable Uapsd Params */
typedef struct sEnableUapsdParams
{
    tUapsd_Params uapsdParams;

    tSirMacAddr bssid;

    /* SmeSession Id or Vdev Id */
    tANI_U32 sessionid;

    /* success or failure */
    tANI_U32   status;
}tEnableUapsdParams, *tpEnableUapsdParams;

/* Disable Uapsd Params */
typedef struct sDisableUapsdParams
{
    tSirMacAddr bssid;

    /* SmeSession Id or Vdev Id */
    tANI_U32 sessionid;

    /* success or failure */
    tANI_U32   status;
}tDisableUapsdParams, *tpDisableUapsdParams;

#endif
