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

#ifndef WLAN_QCT_WDI_H
#define WLAN_QCT_WDI_H

/*===========================================================================

         W L A N   D E V I C E   A B S T R A C T I O N   L A Y E R 
                       E X T E R N A L  A P I
                
                   
DESCRIPTION
  This file contains the external API exposed by the wlan transport layer 
  module.
  
      
  Copyright (c) 2010-2011 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.


  $Header:$ $DateTime: $ $Author: $


when        who    what, where, why
--------    ---    ----------------------------------------------------------
10/05/11    hap     Adding support for Keep Alive
08/04/10    lti     Created module.

===========================================================================*/



/*===========================================================================

                          INCLUDE FILES FOR MODULE

===========================================================================*/

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "wlan_qct_pal_api.h" 
#include "wlan_qct_pal_type.h" 
#include "wlan_qct_pack_align.h" 
#include "wlan_qct_wdi_cfg.h" 

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#ifdef __cplusplus
 extern "C" {
#endif 
 
/* MAC ADDRESS LENGTH - per spec*/
#define WDI_MAC_ADDR_LEN 6

/* Max number of 11b rates -> 1,2,5.5,11 */
#define WDI_NUM_11B_RATES                 4  

/* Max number of 11g rates -> 6,9,12,18,24,36,48,54*/
#define WDI_NUM_11A_RATES                 8  

/* Max number of legacy rates -> 72, 96, 108*/
#define WDI_NUM_POLARIS_RATES             3  

/* Max supported MCS set*/
#define WDI_MAC_MAX_SUPPORTED_MCS_SET    16

/*Max number of Access Categories for QoS - per spec */
#define WDI_MAX_NO_AC                     4

/*Max. size for reserving the Beacon Template */
#define WDI_BEACON_TEMPLATE_SIZE  0x180

#define WDI_WOWL_BCAST_PATTERN_MAX_SIZE 128

#define WDI_WOWL_BCAST_MAX_NUM_PATTERNS 16

#define WDI_MAX_SSID_SIZE  32

/* The shared memory between WDI and HAL is 4K so maximum data can be transferred
from WDI to HAL is 4K.This 4K should also include the Message header so sending 4K
of NV fragment is nt possbile.The next multiple of 1Kb is 3K */

#define FRAGMENT_SIZE 3072

/* Macro to find the total number fragments of the NV Image*/
#define TOTALFRAGMENTS(x) ((x%FRAGMENT_SIZE)== 0) ? (x/FRAGMENT_SIZE):((x/FRAGMENT_SIZE)+1)

/* Beacon Filter Length*/
#define WDI_BEACON_FILTER_LEN 70

/* Coex Indication data size - should match WLAN_COEX_IND_DATA_SIZE */
#define WDI_COEX_IND_DATA_SIZE (4)

#define WDI_CIPHER_SEQ_CTR_SIZE 6

#define WDI_NUM_BSSID   2

/*Version string max length (including NUL) */
#define WDI_VERSION_LENGTH  64


/*WDI Response timeout - how long will WDI wait for a response from the device
    - it should be large enough to allow any other failure mechanism to kick
      in before we get to a timeout (ms units)*/
#define WDI_RESPONSE_TIMEOUT   10000

#define WDI_SET_POWER_STATE_TIMEOUT  10000 /* in msec a very high upper limit */

/*============================================================================
 *     GENERIC STRUCTURES 
  
============================================================================*/

/*---------------------------------------------------------------------------
 WDI Version Information
---------------------------------------------------------------------------*/
typedef struct
{
    wpt_uint8                  revision;
    wpt_uint8                  version;
    wpt_uint8                  minor;
    wpt_uint8                  major;
} WDI_WlanVersionType;

/*---------------------------------------------------------------------------
 WDI Device Capability
---------------------------------------------------------------------------*/
typedef struct 
{
  /*If this flag is true it means that the device can support 802.3/ETH2 to
    802.11 translation*/
  wpt_boolean   bFrameXtlSupported; 

  /*Maximum number of BSSes supported by the Device */
  wpt_uint8     ucMaxBSSSupported;

  /*Maximum number of stations supported by the Device */
  wpt_uint8     ucMaxSTASupported;
}WDI_DeviceCapabilityType; 

/*---------------------------------------------------------------------------
 WDI Channel Offset
---------------------------------------------------------------------------*/
typedef enum
{
  WDI_SECONDARY_CHANNEL_OFFSET_NONE   = 0,
  WDI_SECONDARY_CHANNEL_OFFSET_UP     = 1,
  WDI_SECONDARY_CHANNEL_OFFSET_DOWN   = 3,
#ifdef WLAN_FEATURE_11AC
  WDI_CHANNEL_20MHZ_LOW_40MHZ_CENTERED = 4, //20/40MHZ offset LOW 40/80MHZ offset CENTERED
  WDI_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED = 5, //20/40MHZ offset CENTERED 40/80MHZ offset CENTERED
  WDI_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED = 6, //20/40MHZ offset HIGH 40/80MHZ offset CENTERED
  WDI_CHANNEL_20MHZ_LOW_40MHZ_LOW = 7,//20/40MHZ offset LOW 40/80MHZ offset LOW
  WDI_CHANNEL_20MHZ_HIGH_40MHZ_LOW = 8, //20/40MHZ offset HIGH 40/80MHZ offset LOW
  WDI_CHANNEL_20MHZ_LOW_40MHZ_HIGH = 9, //20/40MHZ offset LOW 40/80MHZ offset HIGH
  WDI_CHANNEL_20MHZ_HIGH_40MHZ_HIGH = 10,//20/40MHZ offset-HIGH 40/80MHZ offset HIGH
#endif
  WDI_SECONDARY_CHANNEL_OFFSET_MAX
}WDI_HTSecondaryChannelOffset;

/*---------------------------------------------------------------------------
  WDI_MacFrameCtl
   Frame control field format (2 bytes)
---------------------------------------------------------------------------*/
typedef  struct 
{
    wpt_uint8 protVer :2;
    wpt_uint8 type :2;
    wpt_uint8 subType :4;

    wpt_uint8 toDS :1;
    wpt_uint8 fromDS :1;
    wpt_uint8 moreFrag :1;
    wpt_uint8 retry :1;
    wpt_uint8 powerMgmt :1;
    wpt_uint8 moreData :1;
    wpt_uint8 wep :1;
    wpt_uint8 order :1;

} WDI_MacFrameCtl;

/*---------------------------------------------------------------------------
  WDI Sequence control field
---------------------------------------------------------------------------*/
typedef struct 
{
  wpt_uint8 fragNum  : 4;
  wpt_uint8 seqNumLo : 4;
  wpt_uint8 seqNumHi : 8;
} WDI_MacSeqCtl;

/*---------------------------------------------------------------------------
  Management header format
---------------------------------------------------------------------------*/
typedef struct 
{
    WDI_MacFrameCtl     fc;
    wpt_uint8           durationLo;
    wpt_uint8           durationHi;
    wpt_uint8           da[WDI_MAC_ADDR_LEN];
    wpt_uint8           sa[WDI_MAC_ADDR_LEN];
    wpt_macAddr         bssId;
    WDI_MacSeqCtl       seqControl;
} WDI_MacMgmtHdr;

/*---------------------------------------------------------------------------
  NV Blob management sturcture
  ---------------------------------------------------------------------------*/

typedef struct
{
  /* NV image  fragments count */
  wpt_uint16 usTotalFragment;

  /* NV fragment size */
  wpt_uint16 usFragmentSize;

  /* current fragment to be sent */
  wpt_uint16 usCurrentFragment;

} WDI_NvBlobInfoParams;


/*---------------------------------------------------------------------------
  Data path enums memory pool resource
  ---------------------------------------------------------------------------*/

typedef enum
{
  /* managment resource pool ID */
  WDI_MGMT_POOL_ID = 0,
  /* Data resource pool ID */
  WDI_DATA_POOL_ID = 1
}WDI_ResPoolType;

/*============================================================================
 *     GENERIC STRUCTURES - END
 ============================================================================*/

/*----------------------------------------------------------------------------
 *  Type Declarations
 * -------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------
  WDI Status 
---------------------------------------------------------------------------*/
typedef enum
{
   WDI_STATUS_SUCCESS,       /* Operation has completed successfully*/
   WDI_STATUS_SUCCESS_SYNC,  /* Operation has completed successfully in a
                                synchronous way - no rsp will be generated*/
   WDI_STATUS_PENDING,       /* Operation result is pending and will be
                                provided asynchronously through the Req Status
                                Callback */
   WDI_STATUS_E_FAILURE,     /* Operation has ended in a generic failure*/
   WDI_STATUS_RES_FAILURE,   /* Operation has ended in a resource failure*/
   WDI_STATUS_MEM_FAILURE,   /* Operation has ended in a memory allocation
                               failure*/
   WDI_STATUS_E_NOT_ALLOWED, /* Operation is not allowed in the current state
                               of the driver*/
   WDI_STATUS_E_NOT_IMPLEMENT, /* Operation is not yet implemented*/

   WDI_STATUS_DEV_INTERNAL_FAILURE, /*An internal error has occurred in the device*/
   WDI_STATUS_MAX

}WDI_Status;


/*---------------------------------------------------------------------------
   WDI_ReqStatusCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL to deliver to UMAC the result of posting
   a previous request for which the return status was PENDING.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from the Control Transport
    pUserData:  user data  
 
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void (*WDI_ReqStatusCb)(WDI_Status   wdiStatus,
                                void*        pUserData);

/*---------------------------------------------------------------------------
  WDI_LowLevelIndEnumType
    Types of indication that can be posted to UMAC by DAL
---------------------------------------------------------------------------*/
typedef enum
{
  /*When RSSI monitoring is enabled of the Lower MAC and a threshold has been
    passed. */
  WDI_RSSI_NOTIFICATION_IND,

  /*Link loss in the low MAC */
  WDI_MISSED_BEACON_IND,

  /*when hardware has signaled an unknown addr2 frames. The indication will
  contain info from frames to be passed to the UMAC, this may use this info to
  deauth the STA*/
  WDI_UNKNOWN_ADDR2_FRAME_RX_IND,

  /*MIC Failure detected by HW*/
  WDI_MIC_FAILURE_IND,

  /*Fatal Error Ind*/
  WDI_FATAL_ERROR_IND, 

  /*Delete Station Ind*/
  WDI_DEL_STA_IND, 

  /*Indication from Coex*/
  WDI_COEX_IND,

  /* Indication for Tx Complete */
  WDI_TX_COMPLETE_IND,

  /*.P2P_NOA_Attr_Indication */
  WDI_P2P_NOA_ATTR_IND,

  /* Preferred Network Found Indication */
  WDI_PREF_NETWORK_FOUND_IND,

  WDI_WAKE_REASON_IND,

  /* Tx PER Tracking Indication */
  WDI_TX_PER_HIT_IND,
  
  WDI_MAX_IND
}WDI_LowLevelIndEnumType;


/*---------------------------------------------------------------------------
  WDI_LowRSSIThIndType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Positive crossing of Rssi Thresh1*/
   wpt_uint32             bRssiThres1PosCross : 1;
  /*Negative crossing of Rssi Thresh1*/
   wpt_uint32             bRssiThres1NegCross : 1;
  /*Positive crossing of Rssi Thresh2*/
   wpt_uint32             bRssiThres2PosCross : 1;
  /*Negative crossing of Rssi Thresh2*/
   wpt_uint32             bRssiThres2NegCross : 1;
  /*Positive crossing of Rssi Thresh3*/
   wpt_uint32             bRssiThres3PosCross : 1;
  /*Negative crossing of Rssi Thresh3*/
   wpt_uint32             bRssiThres3NegCross : 1;

   wpt_uint32             bReserved           : 26;

}WDI_LowRSSIThIndType;


/*---------------------------------------------------------------------------
  WDI_UnkAddr2FrmRxIndType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Rx Bd data of the unknown received addr2 frame.*/
  void*  bufRxBd;

  /*Buffer Length*/
  wpt_uint16  usBufLen; 
}WDI_UnkAddr2FrmRxIndType;

/*---------------------------------------------------------------------------
  WDI_DeleteSTAIndType
---------------------------------------------------------------------------*/
typedef struct
{
   /*ASSOC ID, as assigned by UMAC*/
   wpt_uint16    usAssocId;

   /*STA Index returned during DAL_PostAssocReq or DAL_ConfigStaReq*/
   wpt_uint8     ucSTAIdx;

   /*BSSID of STA*/
   wpt_macAddr   macBSSID; 

    /*MAC ADDR of STA*/
    wpt_macAddr  macADDR2;          
                                
    /* To unify the keepalive / unknown A2 / tim-based disa*/
    wpt_uint16   wptReasonCode;   

}WDI_DeleteSTAIndType;

/*---------------------------------------------------------------------------
  WDI_MicFailureIndType
---------------------------------------------------------------------------*/
typedef struct
{
 /*current BSSID*/
 wpt_macAddr bssId;
  
  /*Source mac address*/
 wpt_macAddr macSrcAddr;

 /*Transmitter mac address*/
 wpt_macAddr macTaAddr;

 /*Destination mac address*/
 wpt_macAddr macDstAddr;

 /*Multicast flag*/
 wpt_uint8   ucMulticast;

 /*First byte of IV*/
 wpt_uint8   ucIV1;

 /*Key Id*/
 wpt_uint8   keyId;

 /*Sequence Number*/
 wpt_uint8   TSC[WDI_CIPHER_SEQ_CTR_SIZE];

 /*receive address */
 wpt_macAddr   macRxAddr;
}WDI_MicFailureIndType;

/*---------------------------------------------------------------------------
  WDI_CoexIndType
---------------------------------------------------------------------------*/
typedef struct
{
  wpt_uint32  coexIndType;
  wpt_uint32  coexIndData[WDI_COEX_IND_DATA_SIZE];
} WDI_CoexIndType;

/*---------------------------------------------------------------------------
  WDI_MacSSid
---------------------------------------------------------------------------*/
typedef struct 
{
    wpt_uint8        ucLength;
    wpt_uint8        sSSID[WDI_MAX_SSID_SIZE];
} WDI_MacSSid;

#ifdef FEATURE_WLAN_SCAN_PNO
/*---------------------------------------------------------------------------
  WDI_PrefNetworkFoundInd    
---------------------------------------------------------------------------*/
typedef struct
{  
  /* Network that was found with the highest RSSI*/
  WDI_MacSSid ssId;
  /* Indicates the RSSI */
  wpt_uint8  rssi;
} WDI_PrefNetworkFoundInd;
#endif // FEATURE_WLAN_SCAN_PNO

#ifdef WLAN_FEATURE_P2P
/*---------------------------------------------------------------------------
 *WDI_P2pNoaAttrIndType
 *-------------------------------------------------------------------------*/

typedef struct
{
  wpt_uint8       ucIndex ;
  wpt_uint8       ucOppPsFlag ;
  wpt_uint16      usCtWin  ;

  wpt_uint16      usNoa1IntervalCnt;
  wpt_uint16      usRsvd1 ;
  wpt_uint32      uslNoa1Duration;
  wpt_uint32      uslNoa1Interval;
  wpt_uint32      uslNoa1StartTime;

  wpt_uint16      usNoa2IntervalCnt;
  wpt_uint16      usRsvd2;
  wpt_uint32      uslNoa2Duration;
  wpt_uint32      uslNoa2Interval;
  wpt_uint32      uslNoa2StartTime;

  wpt_uint32      status;
}WDI_P2pNoaAttrIndType;
#endif

#ifdef WLAN_WAKEUP_EVENTS
/*---------------------------------------------------------------------------
  WDI_WakeReasonIndType    
---------------------------------------------------------------------------*/
typedef struct
{  
    wpt_uint32      ulReason;        /* see tWakeReasonType */
    wpt_uint32      ulReasonArg;     /* argument specific to the reason type */
    wpt_uint32      ulStoredDataLen; /* length of optional data stored in this message, in case
                              HAL truncates the data (i.e. data packets) this length
                              will be less than the actual length */
    wpt_uint32      ulActualDataLen; /* actual length of data */
    wpt_uint8       aDataStart[1];  /* variable length start of data (length == storedDataLen)
                             see specific wake type */ 
} WDI_WakeReasonIndType;
#endif // WLAN_WAKEUP_EVENTS

/*---------------------------------------------------------------------------
  WDI_LowLevelIndType
    Inidcation type and information about the indication being carried
    over
---------------------------------------------------------------------------*/
typedef struct
{
  /*Inidcation type*/
  WDI_LowLevelIndEnumType  wdiIndicationType; 

  /*Indication data*/
  union
  {
    /*RSSI Threshold Info for WDI_LOW_RSSI_IND*/
    WDI_LowRSSIThIndType        wdiLowRSSIInfo; 

    /*Addr2 Frame Info for WDI_UNKNOWN_ADDR2_FRAME_RX_IND*/
    WDI_UnkAddr2FrmRxIndType    wdiUnkAddr2FrmInfo;

    /*MIC Failure info for WDI_MIC_FAILURE_IND*/
    WDI_MicFailureIndType       wdiMICFailureInfo; 

    /*Error code for WDI_FATAL_ERROR_IND*/
    wpt_uint16                  usErrorCode;

    /*Delete STA Indication*/
    WDI_DeleteSTAIndType        wdiDeleteSTAIndType; 
    
    /*Coex Indication*/
    WDI_CoexIndType             wdiCoexInfo;

    /* Tx Complete Indication */
    wpt_uint32                  tx_complete_status;

#ifdef WLAN_FEATURE_P2P
    /* P2P NOA ATTR Indication */
    WDI_P2pNoaAttrIndType        wdiP2pNoaAttrInfo;
#endif


#ifdef FEATURE_WLAN_SCAN_PNO
    WDI_PrefNetworkFoundInd     wdiPrefNetworkFoundInd;
#endif // FEATURE_WLAN_SCAN_PNO

#ifdef WLAN_WAKEUP_EVENTS
    WDI_WakeReasonIndType        wdiWakeReasonInd;
#endif // WLAN_WAKEUP_EVENTS
  }  wdiIndicationData;
}WDI_LowLevelIndType;

/*---------------------------------------------------------------------------
  WDI_LowLevelIndCBType

   DESCRIPTION   
 
   This callback is invoked by DAL to deliver to UMAC certain indications
   that has either received from the lower device or has generated itself.
 
   PARAMETERS 

    IN
    pwdiInd:  information about the indication sent over
    pUserData:  user data provided by UMAC during registration 
 
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void (*WDI_LowLevelIndCBType)(WDI_LowLevelIndType* pwdiInd,
                                      void*                pUserData);

/*---------------------------------------------------------------------------
  WDI_DriverType
---------------------------------------------------------------------------*/
typedef enum
{
    WDI_DRIVER_TYPE_PRODUCTION  = 0,
    WDI_DRIVER_TYPE_MFG         = 1,
    WDI_DRIVER_TYPE_DVT         = 2
} WDI_DriverType;

/*---------------------------------------------------------------------------
  WDI_StartReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*This is a TLV formatted buffer containing all config values that can
   be set through the DAL Interface
 
   The TLV is expected to be formatted like this:
 
   0            7          15              31 .... 
   | CONFIG ID  |  CFG LEN |   RESERVED    |  CFG BODY  |
 
   Or from a C construct point of VU it would look like this:
 
   typedef struct WPT_PACK_POST
   {
       #ifdef  WPT_BIG_ENDIAN
         wpt_uint32   ucCfgId:8;
         wpt_uint32   ucCfgLen:8;
         wpt_uint32   usReserved:16;
       #else
         wpt_uint32   usReserved:16;
         wpt_uint32   ucCfgLen:8;
         wpt_uint32   ucCfgId:8;
       #endif
 
       wpt_uint8   ucCfgBody[ucCfgLen];
   }WDI_ConfigType; 
 
   Multiple such tuplets are to be placed in the config buffer. One for
   each required configuration item:
 
     | TLV 1 |  TLV2 | ....
 
   The buffer is expected to be a flat area of memory that can be manipulated
   with standard memory routines.
 
   For more info please check paragraph 2.3.1 Config Structure from the
   HAL LLD.
 
   For a list of accepted configuration list and IDs please look up
   wlan_qct_dal_cfg.h
 
  */
  void*                   pConfigBuffer; 

  /*Length of the config buffer above*/
  wpt_uint16              usConfigBufferLen;

  /*Production or FTM driver*/
  WDI_DriverType          wdiDriverType; 

  /*Should device enable frame translation */
  wpt_uint8               bFrameTransEnabled;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb         wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                   pUserData;

  /*Indication callback given by UMAC to be called by the WLAN DAL when it
    wishes to send something back independent of a request*/
  WDI_LowLevelIndCBType   wdiLowLevelIndCB; 

  /*The user data passed in by UMAC, it will be sent back when the indication
    function pointer will be called */
  void*                   pIndUserData;
}WDI_StartReqParamsType;


/*---------------------------------------------------------------------------
  WDI_StartRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Status of the response*/
  WDI_Status    wdiStatus;

  /*Max number of STA supported by the device*/
  wpt_uint8     ucMaxStations;

  /*Max number of BSS supported by the device*/
  wpt_uint8     ucMaxBssids;

  /*Version of the WLAN HAL API with which we were compiled*/
  WDI_WlanVersionType wlanCompiledVersion;

  /*Version of the WLAN HAL API that was reported*/
  WDI_WlanVersionType wlanReportedVersion;

  /*WCNSS Software version string*/
  wpt_uint8 wcnssSoftwareVersion[WDI_VERSION_LENGTH];

  /*WCNSS Hardware version string*/
  wpt_uint8 wcnssHardwareVersion[WDI_VERSION_LENGTH];
}WDI_StartRspParamsType;


/*---------------------------------------------------------------------------
  WDI_StopType
---------------------------------------------------------------------------*/
typedef enum
{
  /*Device is being stopped due to a reset*/
  WDI_STOP_TYPE_SYS_RESET,

  /*Device is being stopped due to entering deep sleep*/
  WDI_STOP_TYPE_SYS_DEEP_SLEEP,

  /*Device is being stopped because the RF needs to shut off
    (e.g.:Airplane mode)*/
  WDI_STOP_TYPE_RF_KILL 
}WDI_StopType;

/*---------------------------------------------------------------------------
  WDI_StopReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{

  /*The reason for which the device is being stopped*/
  WDI_StopType   wdiStopReason;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_StopReqParamsType;


/*---------------------------------------------------------------------------
  WDI_ScanMode
---------------------------------------------------------------------------*/
typedef enum
{
  WDI_SCAN_MODE_NORMAL = 0,
  WDI_SCAN_MODE_LEARN,
  WDI_SCAN_MODE_SCAN,
  WDI_SCAN_MODE_PROMISC,
  WDI_SCAN_MODE_SUSPEND_LINK
} WDI_ScanMode;

/*---------------------------------------------------------------------------
  WDI_ScanEntry
---------------------------------------------------------------------------*/
typedef struct 
{
  wpt_uint8 bssIdx[WDI_NUM_BSSID];
  wpt_uint8 activeBSScnt;
}WDI_ScanEntry;

/*---------------------------------------------------------------------------
  WDI_InitScanReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
   /*LEARN - AP Role
    SCAN - STA Role*/
  WDI_ScanMode     wdiScanMode;

  /*BSSID of the BSS*/
  wpt_macAddr      macBSSID;

  /*Whether BSS needs to be notified*/
  wpt_boolean      bNotifyBSS;

  /*Kind of frame to be used for notifying the BSS (Data Null, QoS Null, or
  CTS to Self). Must always be a valid frame type.*/
  wpt_uint8        ucFrameType;

  /*UMAC has the option of passing the MAC frame to be used for notifying
   the BSS. If non-zero, HAL will use the MAC frame buffer pointed to by
   macMgmtHdr. If zero, HAL will generate the appropriate MAC frame based on
   frameType.*/
  wpt_uint8        ucFrameLength;

  /*Pointer to the MAC frame buffer. Used only if ucFrameLength is non-zero.*/
  WDI_MacMgmtHdr   wdiMACMgmtHdr;

  /*Entry to hold number of active BSS to send NULL frames before 
   * initiating SCAN*/
  WDI_ScanEntry    wdiScanEntry;

  /* Flag to enable/disable Single NOA*/
  wpt_boolean      bUseNOA;

  /* Indicates the scan duration (in ms) */
  wpt_uint16       scanDuration;

}WDI_InitScanReqInfoType; 

/*---------------------------------------------------------------------------
  WDI_InitScanReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*The info associated with the request that needs to be sent over to the
    device*/
  WDI_InitScanReqInfoType  wdiReqInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb          wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                    pUserData;
}WDI_InitScanReqParamsType;

/*---------------------------------------------------------------------------
  WDI_StartScanReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the channel to scan*/
  wpt_uint8         ucChannel;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_StartScanReqParamsType;

/*---------------------------------------------------------------------------
  WDI_StartScanRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the status of the operation */
  WDI_Status        wdiStatus;

#if defined WLAN_FEATURE_VOWIFI
  wpt_uint32        aStartTSF[2];
  wpt_int8          ucTxMgmtPower;
#endif
}WDI_StartScanRspParamsType;

/*---------------------------------------------------------------------------
  WDI_EndScanReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the channel to stop scanning.  Not used really. But retained
    for symmetry with "start Scan" message. It can also help in error
    check if needed.*/
  wpt_uint8         ucChannel;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_EndScanReqParamsType;

/*---------------------------------------------------------------------------
  WDI_PhyChanBondState
---------------------------------------------------------------------------*/
typedef enum
{
  WDI_PHY_SINGLE_CHANNEL_CENTERED = 0,            
  WDI_PHY_DOUBLE_CHANNEL_LOW_PRIMARY = 1,     
  WDI_PHY_DOUBLE_CHANNEL_CENTERED = 2,            
  WDI_PHY_DOUBLE_CHANNEL_HIGH_PRIMARY = 3,
#ifdef WLAN_FEATURE_11AC
  WDI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED = 4, //20/40MHZ offset LOW 40/80MHZ offset CENTERED
  WDI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED = 5, //20/40MHZ offset CENTERED 40/80MHZ offset CENTERED
  WDI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED = 6, //20/40MHZ offset HIGH 40/80MHZ offset CENTERED
  WDI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW = 7,//20/40MHZ offset LOW 40/80MHZ offset LOW
  WDI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW = 8, //20/40MHZ offset HIGH 40/80MHZ offset LOW
  WDI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH = 9, //20/40MHZ offset LOW 40/80MHZ offset HIGH
  WDI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH = 10,//20/40MHZ offset-HIGH 40/80MHZ offset HIGH
#endif
  WDI_MAX_CB_STATE
} WDI_PhyChanBondState;

/*---------------------------------------------------------------------------
  WDI_FinishScanReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
   /*LEARN - AP Role
    SCAN - STA Role*/
  WDI_ScanMode          wdiScanMode;

  /*Operating channel to tune to.*/
  wpt_uint8             ucCurrentOperatingChannel;

  /*Channel Bonding state If 20/40 MHz is operational, this will indicate the
  40 MHz extension channel in combination with the control channel*/
  WDI_PhyChanBondState  wdiCBState;

  /*BSSID of the BSS*/
  wpt_macAddr           macBSSID;

  /*Whether BSS needs to be notified*/
  wpt_boolean           bNotifyBSS;

  /*Kind of frame to be used for notifying the BSS (Data Null, QoS Null, or
  CTS to Self). Must always be a valid frame type.*/
  wpt_uint8             ucFrameType;

  /*UMAC has the option of passing the MAC frame to be used for notifying
   the BSS. If non-zero, HAL will use the MAC frame buffer pointed to by
   macMgmtHdr. If zero, HAL will generate the appropriate MAC frame based on
   frameType.*/
  wpt_uint8             ucFrameLength;

  /*Pointer to the MAC frame buffer. Used only if ucFrameLength is non-zero.*/
  WDI_MacMgmtHdr        wdiMACMgmtHdr;

  /*Entry to hold number of active BSS to send NULL frames after SCAN*/
  WDI_ScanEntry    wdiScanEntry;

}WDI_FinishScanReqInfoType; 

/*---------------------------------------------------------------------------
  WDI_SwitchChReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the channel to switch to.*/
  wpt_uint8         ucChannel;

  /*Local power constraint*/
  wpt_uint8         ucLocalPowerConstraint;

  /*Secondary channel offset */
  WDI_HTSecondaryChannelOffset  wdiSecondaryChannelOffset;

#ifdef WLAN_FEATURE_VOWIFI
  wpt_int8      cMaxTxPower;

  /*Self STA Mac address*/
  wpt_macAddr   macSelfStaMacAddr;
#endif
  /* VO Wifi comment: BSSID is needed to identify which session issued this request. As the 
     request has power constraints, this should be applied only to that session */
  /* V IMP: Keep bssId field at the end of this msg. It is used to mantain backward compatbility
   * by way of ignoring if using new host/old FW or old host/new FW since it is at the end of this struct
   */
  wpt_macAddr   macBSSId;

}WDI_SwitchChReqInfoType;

/*---------------------------------------------------------------------------
  WDI_SwitchChReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Channel Info*/
  WDI_SwitchChReqInfoType  wdiChInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_SwitchChReqParamsType;

/*---------------------------------------------------------------------------
  WDI_FinishScanReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Info for the Finish Scan request that will be sent down to the device*/
  WDI_FinishScanReqInfoType  wdiReqInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb            wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                      pUserData;
}WDI_FinishScanReqParamsType;

/*---------------------------------------------------------------------------
  WDI_JoinReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the BSSID to which STA is going to associate*/
  wpt_macAddr   macBSSID; 

  /*Indicates the MAC Address of the current Self STA*/
  wpt_macAddr   macSTASelf; 
 
  /*Indicates the link State determining the entity Type e.g. BTAMP-STA, STA etc.*/
  wpt_uint32    linkState;
  
  /*Indicates the channel to switch to.*/
  WDI_SwitchChReqInfoType  wdiChannelInfo; 

}WDI_JoinReqInfoType;

/*---------------------------------------------------------------------------
  WDI_JoinReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Info for the Join request that will be sent down to the device*/
  WDI_JoinReqInfoType   wdiReqInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb       wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                 pUserData;
}WDI_JoinReqParamsType;

/*---------------------------------------------------------------------------
  WDI_BssType
---------------------------------------------------------------------------*/
typedef enum 
{
  WDI_INFRASTRUCTURE_MODE,
  WDI_INFRA_AP_MODE,                    //Added for softAP support
  WDI_IBSS_MODE,
  WDI_BTAMP_STA_MODE, 
  WDI_BTAMP_AP_MODE,
  WDI_BSS_AUTO_MODE,
}WDI_BssType;

/*---------------------------------------------------------------------------
  WDI_NwType
---------------------------------------------------------------------------*/
typedef enum 
{
  WDI_11A_NW_TYPE,
  WDI_11B_NW_TYPE,
  WDI_11G_NW_TYPE,
  WDI_11N_NW_TYPE,
} WDI_NwType;

/*---------------------------------------------------------------------------
  WDI_ConfigAction
---------------------------------------------------------------------------*/
typedef enum 
{
  WDI_ADD_BSS,
  WDI_UPDATE_BSS
} WDI_ConfigAction;

/*---------------------------------------------------------------------------
  WDI_HTOperatingMode
---------------------------------------------------------------------------*/
typedef enum
{
  WDI_HT_OP_MODE_PURE,
  WDI_HT_OP_MODE_OVERLAP_LEGACY,  
  WDI_HT_OP_MODE_NO_LEGACY_20MHZ_HT,  
  WDI_HT_OP_MODE_MIXED

} WDI_HTOperatingMode;


/*---------------------------------------------------------------------------
  WDI_STAEntryType
---------------------------------------------------------------------------*/
typedef enum 
{
  WDI_STA_ENTRY_SELF,
  WDI_STA_ENTRY_PEER,
  WDI_STA_ENTRY_BSSID,
  WDI_STA_ENTRY_BCAST
}WDI_STAEntryType;

/*---------------------------------------------------------------------------
  WDI_ConfigActionType
---------------------------------------------------------------------------*/
typedef enum 
{
  WDI_ADD_STA,
  WDI_UPDATE_STA
} WDI_ConfigActionType;

/*---------------------------------------------------------------------------- 
  Each station added has a rate mode which specifies the sta attributes
  ----------------------------------------------------------------------------*/
typedef enum 
{
    WDI_RESERVED_1 = 0,
    WDI_RESERVED_2,
    WDI_RESERVED_3,
    WDI_11b,
    WDI_11bg,
    WDI_11a,
    WDI_11n,
} WDI_RateModeType;

/*---------------------------------------------------------------------------
  WDI_SupportedRatesType
---------------------------------------------------------------------------*/
typedef struct  
{
    /*
    * For Self STA Entry: this represents Self Mode.
    * For Peer Stations, this represents the mode of the peer.
    * On Station:
    * --this mode is updated when PE adds the Self Entry.
    * -- OR when PE sends 'ADD_BSS' message and station context in BSS is used to indicate the mode of the AP.
    * ON AP:
    * -- this mode is updated when PE sends 'ADD_BSS' and Sta entry for that BSS is used
    *     to indicate the self mode of the AP.
    * -- OR when a station is associated, PE sends 'ADD_STA' message with this mode updated.
    */

    WDI_RateModeType   opRateMode;

    /* 11b, 11a and aniLegacyRates are IE rates which gives rate in unit of 500Kbps */
    wpt_uint16         llbRates[WDI_NUM_11B_RATES];
    wpt_uint16         llaRates[WDI_NUM_11A_RATES];
    wpt_uint16         aLegacyRates[WDI_NUM_POLARIS_RATES];

    /*Taurus only supports 26 Titan Rates(no ESF/concat Rates will be supported)
      First 26 bits are reserved for those Titan rates and
     the last 4 bits(bit28-31) for Taurus, 2(bit26-27) bits are reserved.*/
    wpt_uint32         uEnhancedRateBitmap; //Titan and Taurus Rates

    /*
    * 0-76 bits used, remaining reserved
    * bits 0-15 and 32 should be set.
    */
    wpt_uint8           aSupportedMCSSet[WDI_MAC_MAX_SUPPORTED_MCS_SET];

    /*
     * RX Highest Supported Data Rate defines the highest data
     * rate that the STA is able to receive, in unites of 1Mbps.
     * This value is derived from "Supported MCS Set field" inside
     * the HT capability element.
     */
    wpt_uint16         aRxHighestDataRate;

   
#ifdef WLAN_FEATURE_11AC
   /*Indicates the Maximum MCS that can be received for each number
        of spacial streams */
    wpt_uint16         vhtRxMCSMap;
  /*Indicate the highest VHT data rate that the STA is able to receive*/
    wpt_uint16         vhtRxHighestDataRate;
  /*Indicates the Maximum MCS that can be transmitted  for each number
       of spacial streams */
    wpt_uint16         vhtTxMCSMap;
  /*Indicate the highest VHT data rate that the STA is able to transmit*/
    wpt_uint16         vhtTxHighestDataRate;
#endif

} WDI_SupportedRates;

/*-------------------------------------------------------------------------- 
  WDI_HTMIMOPowerSaveState 
    Spatial Multiplexing(SM) Power Save mode
 --------------------------------------------------------------------------*/
typedef enum 
{
  WDI_HT_MIMO_PS_STATIC   = 0,    // Static SM Power Save mode
  WDI_HT_MIMO_PS_DYNAMIC  = 1,   // Dynamic SM Power Save mode
  WDI_HT_MIMO_PS_NA       = 2,        // reserved
  WDI_HT_MIMO_PS_NO_LIMIT = 3,  // SM Power Save disabled
} WDI_HTMIMOPowerSaveState;

/*---------------------------------------------------------------------------
  WDI_ConfigStaReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*BSSID of STA*/
  wpt_macAddr               macBSSID;

  /*ASSOC ID, as assigned by UMAC*/
  wpt_uint16                usAssocId;

  /*Used for configuration of different HW modules.*/
  WDI_STAEntryType          wdiSTAType;

  /*Short Preamble Supported.*/
  wpt_uint8                 ucShortPreambleSupported;

  /*MAC Address of STA*/
  wpt_macAddr               macSTA;

  /*Listen interval of the STA*/
  wpt_uint16                usListenInterval;

  /*Support for 11e/WMM*/
  wpt_uint8                 ucWMMEnabled;

  /*11n HT capable STA*/
  wpt_uint8                 ucHTCapable;

  /*TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz*/
  wpt_uint8                 ucTXChannelWidthSet;

  /*RIFS mode 0 - NA, 1 - Allowed*/
  wpt_uint8                 ucRIFSMode;

  /*L-SIG TXOP Protection mechanism
  0 - No Support, 1 - Supported
      SG - there is global field*/
  wpt_uint8                 ucLSIGTxopProtection;

  /*Max Ampdu Size supported by STA. Device programming.
    0 : 8k , 1 : 16k, 2 : 32k, 3 : 64k */
  wpt_uint8                 ucMaxAmpduSize;

  /*Max Ampdu density. Used by RA. 3 : 0~7 : 2^(11nAMPDUdensity -4)*/
  wpt_uint8                 ucMaxAmpduDensity;

  /*Max AMSDU size 1 : 3839 bytes, 0 : 7935 bytes*/
  wpt_uint8                 ucMaxAmsduSize;

  /*Short GI support for 40Mhz packets*/
  wpt_uint8                 ucShortGI40Mhz;

  /*Short GI support for 20Mhz packets*/
  wpt_uint8                 ucShortGI20Mhz;

  /*These rates are the intersection of peer and self capabilities.*/
  WDI_SupportedRates        wdiSupportedRates;

  /*Robust Management Frame (RMF) enabled/disabled*/
  wpt_uint8                 ucRMFEnabled;

  /* The unicast encryption type in the association */
  wpt_uint32                ucEncryptType;

  /*HAL should update the existing STA entry, if this flag is set. UMAC 
   will set this flag in case of RE-ASSOC, where we want to reuse the old
   STA ID.*/
  WDI_ConfigActionType      wdiAction;

  /*U-APSD Flags: 1b per AC.  Encoded as follows:
     b7 b6 b5 b4 b3 b2 b1 b0 =
     X  X  X  X  BE BK VI VO
  */
  wpt_uint8                 ucAPSD;

  /*Max SP Length*/
  wpt_uint8                 ucMaxSPLen;

  /*11n Green Field preamble support*/
  wpt_uint8                 ucGreenFieldCapable;

  /*MIMO Power Save mode*/
  WDI_HTMIMOPowerSaveState  wdiMIMOPS;

  /*Delayed BA Support*/
  wpt_uint8                 ucDelayedBASupport;

  /*Max AMPDU duration in 32us*/
  wpt_uint8                 us32MaxAmpduDuratio;

  /*HT STA should set it to 1 if it is enabled in BSS
   HT STA should set it to 0 if AP does not support it. This indication is
   sent to HAL and HAL uses this flag to pickup up appropriate 40Mhz rates.
  */
  wpt_uint8                 ucDsssCckMode40Mhz;

  wpt_uint8                 ucP2pCapableSta;
#ifdef WLAN_FEATURE_11AC
  wpt_uint8                 ucVhtCapableSta;
  wpt_uint8                 ucVhtTxChannelWidthSet;
#endif
}WDI_ConfigStaReqInfoType;


/*---------------------------------------------------------------------------
  WDI_RateSet
 
  12 Bytes long because this structure can be used to represent rate
  and extended rate set IEs
  The parser assume this to be at least 12 
---------------------------------------------------------------------------*/
#define WDI_RATESET_EID_MAX            12

typedef struct 
{
    wpt_uint8  ucNumRates;
    wpt_uint8  aRates[WDI_RATESET_EID_MAX];
} WDI_RateSet;

/*---------------------------------------------------------------------------
  WDI_AciAifsnType
   access category record
---------------------------------------------------------------------------*/
typedef struct 
{
    wpt_uint8  rsvd  : 1;
    wpt_uint8  aci   : 2;
    wpt_uint8  acm   : 1;
    wpt_uint8  aifsn : 4;
} WDI_AciAifsnType;

/*---------------------------------------------------------------------------
  WDI_CWType
   contention window size
---------------------------------------------------------------------------*/
typedef struct 
{
    wpt_uint8  max : 4;
    wpt_uint8  min : 4;
} WDI_CWType;

/*---------------------------------------------------------------------------
  WDI_EdcaParamRecord
---------------------------------------------------------------------------*/
typedef struct 
{
    /*Access Category Record*/
    WDI_AciAifsnType  wdiACI;

    /*Contention WIndow Size*/
    WDI_CWType        wdiCW;

    /*TX Oportunity Limit*/
    wpt_uint16        usTXOPLimit;
} WDI_EdcaParamRecord;

/*---------------------------------------------------------------------------
  WDI_EDCAParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*BSS Index*/
  wpt_uint8      ucBSSIdx;
  
  /*?*/
  wpt_boolean    bHighPerformance;

  /*Best Effort*/
  WDI_EdcaParamRecord wdiACBE; 
           
  /*Background*/
  WDI_EdcaParamRecord wdiACBK; 
                            
  /*Video*/
  WDI_EdcaParamRecord wdiACVI; 
  
  /*Voice*/
  WDI_EdcaParamRecord acvo; // voice
} WDI_EDCAParamsType;

/* operMode in ADD BSS message */
#define WDI_BSS_OPERATIONAL_MODE_AP     0
#define WDI_BSS_OPERATIONAL_MODE_STA    1

/*---------------------------------------------------------------------------
  WDI_ConfigBSSRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Status of the response*/
  WDI_Status   wdiStatus; 

  /*BSSID of the BSS*/
  wpt_macAddr  macBSSID; 

  /*BSS Index*/
  wpt_uint8    ucBSSIdx;

  /*Unicast DPU signature*/
  wpt_uint8    ucUcastSig;

  /*Broadcast DPU Signature*/
  wpt_uint8    ucBcastSig;

  /*MAC Address of STA*/ 
  wpt_macAddr  macSTA;

  /*BSS STA ID*/
  wpt_uint8    ucSTAIdx;

#ifdef WLAN_FEATURE_VOWIFI
  /*HAL fills in the tx power used for mgmt frames in this field */
  wpt_int8    ucTxMgmtPower;
#endif

}WDI_ConfigBSSRspParamsType;

/*---------------------------------------------------------------------------
  WDI_DelBSSReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
   /*BSS Index of the BSS*/
   wpt_uint8      ucBssIdx;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_DelBSSReqParamsType;

/*---------------------------------------------------------------------------
  WDI_DelBSSRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Status of the response*/
  WDI_Status   wdiStatus; 

  /*BSSID of the BSS*/
  wpt_macAddr  macBSSID; 

  wpt_uint8    ucBssIdx;

}WDI_DelBSSRspParamsType;

/*---------------------------------------------------------------------------
  WDI_ConfigSTARspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Status of the response*/
  WDI_Status      wdiStatus;

  /*STA Idx allocated by HAL*/
  wpt_uint8       ucSTAIdx;

  /*MAC Address of STA*/
  wpt_macAddr     macSTA;

  /* BSSID Index of BSS to which the station is associated */
  wpt_uint8       ucBssIdx;
  
  /* DPU Index  - PTK */
  wpt_uint8       ucDpuIndex;

  /* Bcast DPU Index  - GTK */  
  wpt_uint8       ucBcastDpuIndex;

  /* Management DPU Index - IGTK - Why is it called bcastMgmtDpuIdx? */
  wpt_uint8       ucBcastMgmtDpuIdx;

  /*Unicast DPU signature*/
  wpt_uint8       ucUcastSig;

  /*Broadcast DPU Signature*/
  wpt_uint8       ucBcastSig;

  /* IGTK DPU signature*/
  wpt_uint8       ucMgmtSig;

}WDI_ConfigSTARspParamsType;

/*---------------------------------------------------------------------------
  WDI_PostAssocRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Status of the response*/
  WDI_Status   wdiStatus; 

  /*Parameters related to the BSS*/
  WDI_ConfigBSSRspParamsType bssParams;

  /*Parameters related to the self STA*/
  WDI_ConfigSTARspParamsType staParams;

}WDI_PostAssocRspParamsType;

/*---------------------------------------------------------------------------
  WDI_DelSTAReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*STA Index returned during DAL_PostAssocReq or DAL_ConfigStaReq*/
  wpt_uint8         ucSTAIdx;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_DelSTAReqParamsType;

/*---------------------------------------------------------------------------
  WDI_DelSTARspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
 /*Status of the response*/
  WDI_Status   wdiStatus; 

  /*STA Index returned during DAL_PostAssocReq or DAL_ConfigStaReq*/
  wpt_uint8    ucSTAIdx;
}WDI_DelSTARspParamsType;

/*---------------------------------------------------------------------------
  WDI_EncryptType
---------------------------------------------------------------------------*/
typedef enum 
{
    WDI_ENCR_NONE,
    WDI_ENCR_WEP40,
    WDI_ENCR_WEP104,
    WDI_ENCR_TKIP,
    WDI_ENCR_CCMP,
#if defined(FEATURE_WLAN_WAPI)
    WDI_ENCR_WPI,
#endif
    WDI_ENCR_AES_128_CMAC
} WDI_EncryptType;

/*---------------------------------------------------------------------------
  WDI_KeyDirectionType
---------------------------------------------------------------------------*/
typedef enum
{
    WDI_TX_ONLY,
    WDI_RX_ONLY,
    WDI_TX_RX,
#ifdef WLAN_SOFTAP_FEATURE
    WDI_TX_DEFAULT,
#endif
    WDI_DONOT_USE_KEY_DIRECTION
} WDI_KeyDirectionType;

#define WDI_MAX_ENCR_KEYS 4
#define WDI_MAX_KEY_LENGTH 32
#if defined(FEATURE_WLAN_WAPI)
#define WDI_MAX_KEY_RSC_LEN         16
#define WDI_WAPI_KEY_RSC_LEN        16
#else
#define WDI_MAX_KEY_RSC_LEN         8
#endif

typedef struct
{
    /* Key ID */
    wpt_uint8                  keyId;
    /* 0 for multicast */
    wpt_uint8                  unicast;     
    /* Key Direction */
    WDI_KeyDirectionType       keyDirection;
    /* Usage is unknown */
    wpt_uint8                  keyRsc[WDI_MAX_KEY_RSC_LEN];   
    /* =1 for authenticator, =0 for supplicant */
    wpt_uint8                  paeRole;     
    wpt_uint16                 keyLength;
    wpt_uint8                  key[WDI_MAX_KEY_LENGTH];

}WDI_KeysType;

/*---------------------------------------------------------------------------
  WDI_SetBSSKeyReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
   /*BSS Index of the BSS*/
  wpt_uint8      ucBssIdx; 

  /*Encryption Type used with peer*/
  WDI_EncryptType  wdiEncType;

  /*Number of keys*/
  wpt_uint8        ucNumKeys;

  /*Array of keys.*/
  WDI_KeysType     aKeys[WDI_MAX_ENCR_KEYS]; 

  /*Control for Replay Count, 1= Single TID based replay count on Tx
    0 = Per TID based replay count on TX */
  wpt_uint8        ucSingleTidRc; 
}WDI_SetBSSKeyReqInfoType; 

/*---------------------------------------------------------------------------
  WDI_SetBSSKeyReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Key Info */
  WDI_SetBSSKeyReqInfoType  wdiBSSKeyInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb           wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                      pUserData;
}WDI_SetBSSKeyReqParamsType;

/*---------------------------------------------------------------------------
  WDI_WepType
---------------------------------------------------------------------------*/
typedef enum 
{
  WDI_WEP_STATIC,
  WDI_WEP_DYNAMIC

} WDI_WepType;

/*---------------------------------------------------------------------------
  WDI_RemoveBSSKeyReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
   /*BSS Index of the BSS*/
  wpt_uint8      ucBssIdx; 

  /*Encryption Type used with peer*/
  WDI_EncryptType  wdiEncType;

  /*Key Id*/
  wpt_uint8    ucKeyId;

  /*STATIC/DYNAMIC. Used in Nullifying in Key Descriptors for Static/Dynamic
    keys*/
  WDI_WepType  wdiWEPType;
}WDI_RemoveBSSKeyReqInfoType;

/*---------------------------------------------------------------------------
  WDI_RemoveBSSKeyReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Key Info */
  WDI_RemoveBSSKeyReqInfoType  wdiKeyInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_RemoveBSSKeyReqParamsType;

/*---------------------------------------------------------------------------
  WDI_SetSTAKeyReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
   /*STA Index*/
  wpt_uint8        ucSTAIdx; 

  /*Encryption Type used with peer*/
  WDI_EncryptType  wdiEncType;

  /*STATIC/DYNAMIC*/
  WDI_WepType      wdiWEPType;

  /*Default WEP key, valid only for static WEP, must between 0 and 3.*/
  wpt_uint8        ucDefWEPIdx;

  /*Number of keys*/
  wpt_uint8        ucNumKeys;

  /*Array of keys.*/
  WDI_KeysType     wdiKey[WDI_MAX_ENCR_KEYS]; 

  /*Control for Replay Count, 1= Single TID based replay count on Tx
    0 = Per TID based replay count on TX */
  wpt_uint8        ucSingleTidRc; 
}WDI_SetSTAKeyReqInfoType; 

/*---------------------------------------------------------------------------
  WDI_ConfigBSSReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Peer BSSID*/
  wpt_macAddr              macBSSID;

  /*Self MAC Address*/
  wpt_macAddr              macSelfAddr; 

  /*BSS Type*/
  WDI_BssType              wdiBSSType;

  /*Operational Mode: AP =0, STA = 1*/
  wpt_uint8                ucOperMode;

  /*Network Type*/
  WDI_NwType               wdiNWType;

  /*Used to classify PURE_11G/11G_MIXED to program MTU*/
  wpt_uint8                ucShortSlotTimeSupported;

  /*Co-exist with 11a STA*/
  wpt_uint8                ucllaCoexist;

  /*Co-exist with 11b STA*/
  wpt_uint8                ucllbCoexist;

  /*Co-exist with 11g STA*/
  wpt_uint8                ucllgCoexist;

  /*Coexistence with 11n STA*/
  wpt_uint8                ucHT20Coexist;

  /*Non GF coexist flag*/
  wpt_uint8                ucllnNonGFCoexist;

  /*TXOP protection support*/
  wpt_uint8                ucTXOPProtectionFullSupport;

  /*RIFS mode*/
  wpt_uint8                ucRIFSMode;

  /*Beacon Interval in TU*/
  wpt_uint16               usBeaconInterval;

  /*DTIM period*/
  wpt_uint8                ucDTIMPeriod;

  /*TX Width Set: 0 - 20 MHz only, 1 - 20/40 MHz*/
  wpt_uint8                ucTXChannelWidthSet;

  /*Operating channel*/
  wpt_uint8                ucCurrentOperChannel;

  /*Extension channel for channel bonding*/
  wpt_uint8                ucCurrentExtChannel;

  /*Context of the station being added in HW.*/
  WDI_ConfigStaReqInfoType wdiSTAContext;

  /*SSID of the BSS*/
  WDI_MacSSid              wdiSSID;

  /*HAL should update the existing BSS entry, if this flag is set. UMAC will
    set this flag in case of RE-ASSOC, where we want to reuse the old BSSID*/
  WDI_ConfigAction         wdiAction;

  /*Basic Rate Set*/
  WDI_RateSet              wdiRateSet;

  /*Enable/Disable HT capabilities of the BSS*/
  wpt_uint8                ucHTCapable;

  /* Enable/Disable OBSS protection */
  wpt_uint8                ucObssProtEnabled;

  /*RMF enabled/disabled*/
  wpt_uint8                ucRMFEnabled;

  /*Determines the current HT Operating Mode operating mode of the
    802.11n STA*/
  WDI_HTOperatingMode      wdiHTOperMod;

  /*Dual CTS Protection: 0 - Unused, 1 - Used*/
  wpt_uint8                ucDualCTSProtection;

    /* Probe Response Max retries */
  wpt_uint8   ucMaxProbeRespRetryLimit;

  /* To Enable Hidden ssid */
  wpt_uint8   bHiddenSSIDEn;

  /* To Enable Disable FW Proxy Probe Resp */
  wpt_uint8   bProxyProbeRespEn;

 /* Boolean to indicate if EDCA params are valid. UMAC might not have valid 
    EDCA params or might not desire to apply EDCA params during config BSS. 
    0 implies Not Valid ; Non-Zero implies valid*/
  wpt_uint8   ucEDCAParamsValid;

   /*EDCA Parameters for BK*/  
  WDI_EdcaParamRecord       wdiBKEDCAParams; 

   /*EDCA Parameters for BE*/  
  WDI_EdcaParamRecord       wdiBEEDCAParams; 

   /*EDCA Parameters for VI*/  
  WDI_EdcaParamRecord       wdiVIEDCAParams; 

   /*EDCA Parameters for VO*/  
  WDI_EdcaParamRecord       wdiVOEDCAParams; 

#ifdef WLAN_FEATURE_VOWIFI
   /*max power to be used after applying the power constraint, if any */
  wpt_int8                  cMaxTxPower;
#endif

  /* Persona for the BSS can be STA,AP,GO,CLIENT, same as Connection Mode */  
  wpt_uint8                 ucPersona;

  /* Spectrum Mangement Indicator */
  wpt_uint8                 bSpectrumMgtEn;

#ifdef WLAN_FEATURE_VOWIFI_11R
  wpt_uint8                 bExtSetStaKeyParamValid;
  WDI_SetSTAKeyReqInfoType  wdiExtSetKeyParam;
#endif

#ifdef WLAN_FEATURE_11AC
  wpt_uint8                 ucVhtCapableSta;
  wpt_uint8                 ucVhtTxChannelWidthSet;
#endif

}WDI_ConfigBSSReqInfoType;

/*---------------------------------------------------------------------------
  WDI_PostAssocReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Config STA arguments.*/
  WDI_ConfigStaReqInfoType  wdiSTAParams; 

   /*Config BSS Arguments*/
  WDI_ConfigBSSReqInfoType  wdiBSSParams;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb           wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                     pUserData;
}WDI_PostAssocReqParamsType;

/*---------------------------------------------------------------------------
  WDI_ConfigBSSReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Info for the Join request that will be sent down to the device*/
  WDI_ConfigBSSReqInfoType   wdiReqInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb            wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                      pUserData;
}WDI_ConfigBSSReqParamsType;

/*---------------------------------------------------------------------------
  WDI_SetSTAKeyReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Key Info*/
  WDI_SetSTAKeyReqInfoType  wdiKeyInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_SetSTAKeyReqParamsType;

/*---------------------------------------------------------------------------
  WDI_RemoveSTAKeyReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*STA Index*/
  wpt_uint8        ucSTAIdx; 

  /*Encryption Type used with peer*/
  WDI_EncryptType  wdiEncType;

  /*Key Id*/
  wpt_uint8        ucKeyId;

  /*Whether to invalidate the Broadcast key or Unicast key. In case of WEP,
  the same key is used for both broadcast and unicast.*/
  wpt_uint8        ucUnicast;
}WDI_RemoveSTAKeyReqInfoType;

/*---------------------------------------------------------------------------
  WDI_RemoveSTAKeyReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Key Info */
  WDI_RemoveSTAKeyReqInfoType  wdiKeyInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_RemoveSTAKeyReqParamsType;

/*---------------------------------------------------------------------------
                            QOS Parameters
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  WDI_TSInfoTfc
---------------------------------------------------------------------------*/
typedef struct 
{
    wpt_uint16       ackPolicy:2;
    wpt_uint16       userPrio:3;
    wpt_uint16       psb:1;
    wpt_uint16       aggregation : 1;
    wpt_uint16       accessPolicy : 2;
    wpt_uint16       direction : 2;
    wpt_uint16       tsid : 4;
    wpt_uint16       trafficType : 1;
} WDI_TSInfoTfc;

/*---------------------------------------------------------------------------
  WDI_TSInfoSch
---------------------------------------------------------------------------*/
typedef struct 
{
    wpt_uint8        rsvd : 7;
    wpt_uint8        schedule : 1;
} WDI_TSInfoSch;

/*---------------------------------------------------------------------------
  WDI_TSInfoType
---------------------------------------------------------------------------*/
typedef struct 
{
    WDI_TSInfoTfc  wdiTraffic;
    WDI_TSInfoSch  wdiSchedule;
} WDI_TSInfoType;

/*---------------------------------------------------------------------------
  WDI_TspecIEType
---------------------------------------------------------------------------*/
typedef struct 
{
    wpt_uint8             ucType;
    wpt_uint8             ucLength;
    WDI_TSInfoType        wdiTSinfo;
    wpt_uint16            usNomMsduSz;
    wpt_uint16            usMaxMsduSz;
    wpt_uint32            uMinSvcInterval;
    wpt_uint32            uMaxSvcInterval;
    wpt_uint32            uInactInterval;
    wpt_uint32            uSuspendInterval;
    wpt_uint32            uSvcStartTime;
    wpt_uint32            uMinDataRate;
    wpt_uint32            uMeanDataRate;
    wpt_uint32            uPeakDataRate;
    wpt_uint32            uMaxBurstSz;
    wpt_uint32            uDelayBound;
    wpt_uint32            uMinPhyRate;
    wpt_uint16            usSurplusBw;
    wpt_uint16            usMediumTime;
}WDI_TspecIEType;

/*---------------------------------------------------------------------------
  WDI_AddTSReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*STA Index*/
  wpt_uint8         ucSTAIdx; 

  /*Identifier for TSpec*/
  wpt_uint16        ucTspecIdx;

  /*Tspec IE negotiated OTA*/
  WDI_TspecIEType   wdiTspecIE;

  /*UAPSD delivery and trigger enabled flags */
  wpt_uint8         ucUapsdFlags;

  /*SI for each AC*/
  wpt_uint8         ucServiceInterval[WDI_MAX_NO_AC];

  /*Suspend Interval for each AC*/
  wpt_uint8         ucSuspendInterval[WDI_MAX_NO_AC];

  /*DI for each AC*/
  wpt_uint8         ucDelayedInterval[WDI_MAX_NO_AC];

}WDI_AddTSReqInfoType;


/*---------------------------------------------------------------------------
  WDI_AddTSReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*TSpec Info */
  WDI_AddTSReqInfoType  wdiTsInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_AddTSReqParamsType;

/*---------------------------------------------------------------------------
  WDI_DelTSReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*STA Index*/
  wpt_uint8         ucSTAIdx; 

  /*Identifier for TSpec*/
  wpt_uint16        ucTspecIdx;

  /*BSSID of the BSS*/
  wpt_macAddr      macBSSID;
}WDI_DelTSReqInfoType;

/*---------------------------------------------------------------------------
  WDI_DelTSReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Del TSpec Info*/
  WDI_DelTSReqInfoType  wdiDelTSInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_DelTSReqParamsType;

/*---------------------------------------------------------------------------
  WDI_UpdateEDCAInfoType
---------------------------------------------------------------------------*/
typedef struct
{
   /*BSS Index of the BSS*/
   wpt_uint8      ucBssIdx;

  /* Boolean to indicate if EDCA params are valid. UMAC might not have valid 
    EDCA params or might not desire to apply EDCA params during config BSS. 
    0 implies Not Valid ; Non-Zero implies valid*/
  wpt_uint8   ucEDCAParamsValid;

  /*EDCA params for BE*/
  WDI_EdcaParamRecord wdiEdcaBEInfo;

  /*EDCA params for BK*/
  WDI_EdcaParamRecord wdiEdcaBKInfo;

  /*EDCA params for VI*/
  WDI_EdcaParamRecord wdiEdcaVIInfo;

  /*EDCA params for VO*/
  WDI_EdcaParamRecord wdiEdcaVOInfo;

}WDI_UpdateEDCAInfoType;

/*---------------------------------------------------------------------------
  WDI_UpdateEDCAParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*EDCA Info */
  WDI_UpdateEDCAInfoType  wdiEDCAInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_UpdateEDCAParamsType;

/*---------------------------------------------------------------------------
  WDI_AddBASessionReqinfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the station for which BA is added..*/
  wpt_uint8        ucSTAIdx;

  /*The peer mac address*/
  wpt_macAddr      macPeerAddr;

  /*TID for which BA was negotiated*/
  wpt_uint8        ucBaTID;

  /*Delayed or imediate */
  wpt_uint8        ucBaPolicy;

  /*The number of buffers for this TID (baTID)*/
  wpt_uint16       usBaBufferSize;
  
  /*BA timeout in TU's*/
  wpt_uint16       usBaTimeout;
  
  /*b0..b3 - Fragment Number - Always set to 0
   b4..b15 - Starting Sequence Number of first MSDU for which this BA is setup*/
  wpt_uint16       usBaSSN;
  
  /*Originator/Recipient*/
  wpt_uint8        ucBaDirection;
  
}WDI_AddBASessionReqinfoType;


/*---------------------------------------------------------------------------
  WDI_AddBASessionReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*BA Session Info Type*/
  WDI_AddBASessionReqinfoType  wdiBASessionInfoType; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb       wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                 pUserData;
}WDI_AddBASessionReqParamsType;

/*---------------------------------------------------------------------------
  WDI_AddBASessionRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Status of the response*/
  WDI_Status   wdiStatus; 
  
  /* Dialog token */
  wpt_uint8    ucBaDialogToken;
  
  /* TID for which the BA session has been setup */
  wpt_uint8    ucBaTID;
  
  /* BA Buffer Size allocated for the current BA session */
  wpt_uint8    ucBaBufferSize;

  /* BA session ID */
  wpt_uint16   usBaSessionID;
  
  /* Reordering Window buffer */
  wpt_uint8    ucWinSize;
  
  /*Station Index to id the sta */
  wpt_uint8    ucSTAIdx;
  
  /* Starting Sequence Number */
  wpt_uint16   usBaSSN;

}WDI_AddBASessionRspParamsType;

/*---------------------------------------------------------------------------
  WDI_AddBAReqinfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the station for which BA is added..*/
  wpt_uint8        ucSTAIdx;

  /* Session Id */
  wpt_uint8        ucBaSessionID;
  
  /* Reorder Window Size */
  wpt_uint8        ucWinSize;
  
#ifdef FEATURE_ON_CHIP_REORDERING
  wpt_boolean      bIsReorderingDoneOnChip;
#endif

}WDI_AddBAReqinfoType;


/*---------------------------------------------------------------------------
  WDI_AddBAReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*BA Info Type*/
  WDI_AddBAReqinfoType  wdiBAInfoType; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb       wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                 pUserData;
}WDI_AddBAReqParamsType;


/*---------------------------------------------------------------------------
  WDI_AddBARspinfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Status of the response*/
  WDI_Status   wdiStatus; 

  /* Dialog token */
  wpt_uint8    ucBaDialogToken;

}WDI_AddBARspinfoType;

/*---------------------------------------------------------------------------
  WDI_TriggerBAReqCandidateType
---------------------------------------------------------------------------*/
typedef struct
{
  /* STA index */
  wpt_uint8   ucSTAIdx;

  /* TID bit map for the STA's*/
  wpt_uint8   ucTidBitmap;

}WDI_TriggerBAReqCandidateType;


/*---------------------------------------------------------------------------
  WDI_TriggerBAReqinfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the station for which BA is added..*/
  wpt_uint8        ucSTAIdx;

  /* Session Id */
  wpt_uint8        ucBASessionID;

  /* Trigger BA Request candidate count */
  wpt_uint16       usBACandidateCnt;

  /* WDI_TriggerBAReqCandidateType  followed by this*/

}WDI_TriggerBAReqinfoType;


/*---------------------------------------------------------------------------
  WDI_TriggerBAReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*BA Trigger Info Type*/
  WDI_TriggerBAReqinfoType  wdiTriggerBAInfoType; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb       wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                 pUserData;
}WDI_TriggerBAReqParamsType;

/*---------------------------------------------------------------------------
  WDI_AddBAInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  wpt_uint16 fBaEnable : 1;
  wpt_uint16 startingSeqNum: 12;
  wpt_uint16 reserved : 3;
}WDI_AddBAInfoType;

/*---------------------------------------------------------------------------
  WDI_TriggerBARspCandidateType
---------------------------------------------------------------------------*/
#define STA_MAX_TC 8

typedef struct
{
  /* STA index */
  wpt_macAddr       macSTA;

  /* BA Info */
  WDI_AddBAInfoType wdiBAInfo[STA_MAX_TC];
}WDI_TriggerBARspCandidateType;

/*---------------------------------------------------------------------------
  WDI_TriggerBARspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Status of the response*/
  WDI_Status   wdiStatus; 

  /*BSSID of the BSS*/
  wpt_macAddr  macBSSID;

  /* Trigger BA response candidate count */
  wpt_uint16   usBaCandidateCnt;

  /* WDI_TriggerBARspCandidateType  followed by this*/

}WDI_TriggerBARspParamsType;

/*---------------------------------------------------------------------------
  WDI_DelBAReqinfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the station for which BA is added..*/
  wpt_uint8        ucSTAIdx;

  /*TID for which BA was negotiated*/
  wpt_uint8        ucBaTID;

  /*Originator/Recipient*/
  wpt_uint8        ucBaDirection;
  
}WDI_DelBAReqinfoType;

/*---------------------------------------------------------------------------
  WDI_DelBAReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*BA Info */
  WDI_DelBAReqinfoType  wdiBAInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_DelBAReqParamsType;


/*---------------------------------------------------------------------------
  WDI_SwitchCHRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
   /*Status of the response*/
  WDI_Status    wdiStatus;

  /*Indicates the channel that WLAN is on*/
  wpt_uint8     ucChannel;

#ifdef WLAN_FEATURE_VOWIFI
  /*HAL fills in the tx power used for mgmt frames in this field.*/
  wpt_int8     ucTxMgmtPower;
#endif

}WDI_SwitchCHRspParamsType;

/*---------------------------------------------------------------------------
  WDI_ConfigSTAReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Info for the Join request that will be sent down to the device*/
  WDI_ConfigStaReqInfoType   wdiReqInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb            wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                      pUserData;
}WDI_ConfigSTAReqParamsType;


/*---------------------------------------------------------------------------
  WDI_UpdateBeaconParamsInfoType
---------------------------------------------------------------------------*/

typedef struct
{
   /*BSS Index of the BSS*/
   wpt_uint8      ucBssIdx;

    /*shortPreamble mode. HAL should update all the STA rates when it
    receives this message*/
    wpt_uint8 ucfShortPreamble;
    /* short Slot time.*/
    wpt_uint8 ucfShortSlotTime;
    /* Beacon Interval */
    wpt_uint16 usBeaconInterval;
    /*Protection related */
    wpt_uint8 ucllaCoexist;
    wpt_uint8 ucllbCoexist;
    wpt_uint8 ucllgCoexist;
    wpt_uint8 ucHt20MhzCoexist;
    wpt_uint8 ucllnNonGFCoexist;
    wpt_uint8 ucfLsigTXOPProtectionFullSupport;
    wpt_uint8 ucfRIFSMode;

    wpt_uint16 usChangeBitmap;
}WDI_UpdateBeaconParamsInfoType;



/*---------------------------------------------------------------------------
  WDI_UpdateBeaconParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Update Beacon Params  Info*/
  WDI_UpdateBeaconParamsInfoType  wdiUpdateBeaconParamsInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_UpdateBeaconParamsType;

/*---------------------------------------------------------------------------
  WDI_SendBeaconParamsInfoType
---------------------------------------------------------------------------*/

typedef struct {

   /*BSSID of the BSS*/
   wpt_macAddr  macBSSID;

   /* Beacon data */
   wpt_uint8    beacon[WDI_BEACON_TEMPLATE_SIZE];     

   /* length of the template */
   wpt_uint32   beaconLength;

#ifdef WLAN_SOFTAP_FEATURE
   /* TIM IE offset from the beginning of the template.*/
   wpt_uint32   timIeOffset; 
#endif

#ifdef WLAN_FEATURE_P2P
   /* P2P IE offset from the beginning of the template */
   wpt_uint16   usP2PIeOffset;
#endif
} WDI_SendBeaconParamsInfoType;

/*---------------------------------------------------------------------------
  WDI_SendBeaconParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Send Beacon Params  Info*/
  WDI_SendBeaconParamsInfoType  wdiSendBeaconParamsInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_SendBeaconParamsType;

/*---------------------------------------------------------------------------
  WDI_LinkStateType
---------------------------------------------------------------------------*/
typedef enum 
{
    WDI_LINK_IDLE_STATE              = 0,
    WDI_LINK_PREASSOC_STATE          = 1,
    WDI_LINK_POSTASSOC_STATE         = 2,
    WDI_LINK_AP_STATE                = 3,
    WDI_LINK_IBSS_STATE              = 4,

    // BT-AMP Case
    WDI_LINK_BTAMP_PREASSOC_STATE    = 5,
    WDI_LINK_BTAMP_POSTASSOC_STATE   = 6,
    WDI_LINK_BTAMP_AP_STATE          = 7,
    WDI_LINK_BTAMP_STA_STATE         = 8,
    
    // Reserved for HAL internal use
    WDI_LINK_LEARN_STATE             = 9,
    WDI_LINK_SCAN_STATE              = 10,
    WDI_LINK_FINISH_SCAN_STATE       = 11,
    WDI_LINK_INIT_CAL_STATE          = 12,
    WDI_LINK_FINISH_CAL_STATE        = 13,
#ifdef WLAN_FEATURE_P2P
    WDI_LINK_LISTEN_STATE            = 14,
#endif
    WDI_LINK_MAX                     = 0x7FFFFFFF
} WDI_LinkStateType;

/*---------------------------------------------------------------------------
  WDI_SetLinkReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*BSSID of the BSS*/
  wpt_macAddr           macBSSID;

  /*Link state*/
  WDI_LinkStateType     wdiLinkState;

  /*BSSID of the BSS*/
  wpt_macAddr           macSelfStaMacAddr;
}WDI_SetLinkReqInfoType;

/*---------------------------------------------------------------------------
  WDI_SetLinkReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Link Info*/
  WDI_SetLinkReqInfoType  wdiLinkInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_SetLinkReqParamsType;

/*---------------------------------------------------------------------------
  WDI_GetStatsParamsInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the station for which Get Stats are requested..*/
  wpt_uint8        ucSTAIdx;

  /* categories of stats requested */
  wpt_uint32       uStatsMask;
}WDI_GetStatsParamsInfoType;

/*---------------------------------------------------------------------------
  WDI_GetStatsReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Get Stats Params  Info*/
  WDI_GetStatsParamsInfoType  wdiGetStatsParamsInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_GetStatsReqParamsType;

/*---------------------------------------------------------------------------
  WDI_GetStatsRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*message type is same as the request type*/
  wpt_uint16       usMsgType;

  /* length of the entire request, includes the pStatsBuf length too*/
  wpt_uint16       usMsgLen;
  
  /*Result of the operation*/
  WDI_Status       wdiStatus;

  /*Indicates the station for which Get Stats are requested..*/
  wpt_uint8        ucSTAIdx;

  /* categories of stats requested */
  wpt_uint32       uStatsMask;

  /* The Stats buffer starts here and can be an aggregate of more than one statistics 
   * structure depending on statsMask.*/
}WDI_GetStatsRspParamsType;

#ifdef FEATURE_WLAN_CCX
/*---------------------------------------------------------------------------
  WDI_TSMStatsParamsInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the station for which Get Stats are requested..*/
  wpt_uint8        ucTid;

  wpt_macAddr      bssid;
}WDI_TSMStatsParamsInfoType;

/*---------------------------------------------------------------------------
  WDI_TSMStatsReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Get TSM Stats Params  Info*/
  WDI_TSMStatsParamsInfoType  wdiTsmStatsParamsInfo;

  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;

}WDI_TSMStatsReqParamsType;


/*---------------------------------------------------------------------------
  WDI_TSMStatsRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Indicates the status of the operation */
  WDI_Status      wdiStatus;

  wpt_uint16      UplinkPktQueueDly;
  wpt_uint16      UplinkPktQueueDlyHist[4];
  wpt_uint32      UplinkPktTxDly;
  wpt_uint16      UplinkPktLoss;
  wpt_uint16      UplinkPktCount;
  wpt_uint8       RoamingCount;
  wpt_uint16      RoamingDly;
}WDI_TSMStatsRspParamsType;


#endif
/*---------------------------------------------------------------------------
  WDI_UpdateCfgReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*This is a TLV formatted buffer containing all config values that can
   be set through the DAL Interface
 
   The TLV is expected to be formatted like this:
 
   0            7          15              31 .... 
   | CONFIG ID  |  CFG LEN |   RESERVED    |  CFG BODY  |
 
   Or from a C construct point of VU it would look like this:
 
   typedef struct WPT_PACK_POST
   {
       #ifdef  WPT_BIG_ENDIAN
         wpt_uint32   ucCfgId:8;
         wpt_uint32   ucCfgLen:8;
         wpt_uint32   usReserved:16;
       #else
         wpt_uint32   usReserved:16;
         wpt_uint32   ucCfgLen:8;
         wpt_uint32   ucCfgId:8;
       #endif
 
       wpt_uint8   ucCfgBody[ucCfgLen];
   }WDI_ConfigType; 
 
   Multiple such tuplets are to be placed in the config buffer. One for
   each required configuration item:
 
     | TLV 1 |  TLV2 | ....
 
   The buffer is expected to be a flat area of memory that can be manipulated
   with standard memory routines.
 
   For more info please check paragraph 2.3.1 Config Structure from the
   HAL LLD.
 
   For a list of accepted configuration list and IDs please look up
   wlan_qct_dal_cfg.h
  */
  void*                   pConfigBuffer; 

  /*Length of the config buffer above*/
  wpt_uint32              uConfigBufferLen;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_UpdateCfgReqParamsType;

/*---------------------------------------------------------------------------
  WDI_UpdateProbeRspTemplateInfoType
---------------------------------------------------------------------------*/
//Default Beacon template size
#define WDI_PROBE_RSP_TEMPLATE_SIZE 0x180

#define WDI_PROBE_REQ_BITMAP_IE_LEN 8

typedef struct
{
  /*BSSID for which the Probe Template is to be used*/
  wpt_macAddr     macBSSID;

  /*Probe response template*/
  wpt_uint8      *pProbeRespTemplate[WDI_PROBE_RSP_TEMPLATE_SIZE];

  /*Template Len*/
  wpt_uint32      uProbeRespTemplateLen;

  /*Bitmap for the IEs that are to be handled at SLM level*/
  wpt_uint32      uaProxyProbeReqValidIEBmap[WDI_PROBE_REQ_BITMAP_IE_LEN];

}WDI_UpdateProbeRspTemplateInfoType;

/*---------------------------------------------------------------------------
  WDI_UpdateProbeRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Link Info*/
  WDI_UpdateProbeRspTemplateInfoType  wdiProbeRspTemplateInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_UpdateProbeRspTemplateParamsType;

/*---------------------------------------------------------------------------
  WDI_NvDownloadReqBlobInfo
---------------------------------------------------------------------------*/

typedef struct
{
  /* Blob starting address*/
  void *pBlobAddress;

  /* Blob size */
  wpt_uint32 uBlobSize;
  
}WDI_NvDownloadReqBlobInfo;

/*---------------------------------------------------------------------------
 WDI_NvDownloadReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*NV Blob Info*/
  WDI_NvDownloadReqBlobInfo  wdiBlobInfo; 

  /*Request status callback offered by UMAC - it is called if the current
   req has returned PENDING as status; it delivers the status of sending
   the message over the BUS */
  WDI_ReqStatusCb       wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */
  void*                 pUserData;
  
}WDI_NvDownloadReqParamsType;

/*---------------------------------------------------------------------------
  WDI_NvDownloadRspInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Status of the response*/
  WDI_Status   wdiStatus; 

}WDI_NvDownloadRspInfoType;

/*---------------------------------------------------------------------------
  WDI_SetMaxTxPowerInfoType
---------------------------------------------------------------------------*/

typedef struct
{
  /*BSSID is needed to identify which session issued this request. As the request has 
    power constraints, this should be applied only to that session*/
  wpt_macAddr macBSSId;


  wpt_macAddr macSelfStaMacAddr;

  /* In request  power == MaxTxpower to be used.*/
  wpt_int8  ucPower;

}WDI_SetMaxTxPowerInfoType;

/*---------------------------------------------------------------------------
  WDI_SetMaxTxPowerParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Link Info*/
  WDI_SetMaxTxPowerInfoType  wdiMaxTxPowerInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_SetMaxTxPowerParamsType;


/*---------------------------------------------------------------------------
  WDI_SetMaxTxPowerRspMsg
---------------------------------------------------------------------------*/

typedef struct
{
  /* In response, power==tx power used for management frames*/
  wpt_int8  ucPower;
  
  /*Result of the operation*/
  WDI_Status wdiStatus;
 
}WDI_SetMaxTxPowerRspMsg;

#ifdef WLAN_FEATURE_P2P
typedef struct
{
  wpt_uint8   ucOpp_ps;
  wpt_uint32  uCtWindow;
  wpt_uint8   ucCount; 
  wpt_uint32  uDuration;
  wpt_uint32  uInterval;
  wpt_uint32  uSingle_noa_duration;
  wpt_uint8   ucPsSelection;
}WDI_SetP2PGONOAReqInfoType;

/*---------------------------------------------------------------------------
  WDI_SetP2PGONOAReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*P2P GO NOA Req*/
  WDI_SetP2PGONOAReqInfoType  wdiP2PGONOAInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_SetP2PGONOAReqParamsType;
#endif


/*---------------------------------------------------------------------------
  WDI_SetAddSTASelfParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Self Station MAC address*/
  wpt_macAddr selfMacAddr;

  /*Status of the operation*/
  wpt_uint32  uStatus;
}WDI_AddSTASelfInfoType;

/*---------------------------------------------------------------------------
  WDI_SetAddSTASelfParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /* Add Sta Self Req */
  WDI_AddSTASelfInfoType  wdiAddSTASelfInfo;

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb         wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                   pUserData;
}WDI_AddSTASelfReqParamsType;


/*---------------------------------------------------------------------------
  WDI_AddSTASelfRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
 /*Status of the response*/
  WDI_Status   wdiStatus; 

  /*STA Idx allocated by HAL*/
  wpt_uint8    ucSTASelfIdx;

  /* DPU Index (IGTK, PTK, GTK all same) */
  wpt_uint8    dpuIdx;

  /* DPU Signature */
  wpt_uint8    dpuSignature;

  /*Self STA Mac*/
  wpt_macAddr  macSelfSta;

}WDI_AddSTASelfRspParamsType;

/*---------------------------------------------------------------------------
  WDI_DelSTASelfReqParamsType
  Del Sta Self info passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct
{
   wpt_macAddr       selfMacAddr;

}WDI_DelSTASelfInfoType;

/*---------------------------------------------------------------------------
  WDI_DelSTASelfReqParamsType
  Del Sta Self info passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct
{
  /*Del Sta Self Info Type */
   WDI_DelSTASelfInfoType     wdiDelStaSelfInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_DelSTASelfReqParamsType;

/*---------------------------------------------------------------------------
  WDI_DelSTASelfRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
 /*Status of the response*/
  WDI_Status   wdiStatus; 

  /*STA Index returned during DAL_PostAssocReq or DAL_ConfigStaReq*/
//  wpt_uint8   ucSTAIdx;
}WDI_DelSTASelfRspParamsType;

/*---------------------------------------------------------------------------
  WDI_UapsdInfoType
  UAPSD parameters passed per AC to WDA from UMAC
---------------------------------------------------------------------------*/
typedef struct  
{
   wpt_uint8  ucSTAIdx;        // STA index
   wpt_uint8  ucAc;            // Access Category
   wpt_uint8  ucUp;            // User Priority
   wpt_uint32 uSrvInterval;   // Service Interval
   wpt_uint32 uSusInterval;   // Suspend Interval
   wpt_uint32 uDelayInterval; // Delay Interval
} WDI_UapsdInfoType;

/*---------------------------------------------------------------------------
  WDI_SetUapsdAcParamsReqParamsType
  UAPSD parameters passed per AC to WDI from WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Enter BMPS Info Type, same as tEnterBmpsParams */ 
   WDI_UapsdInfoType wdiUapsdInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb   wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*             pUserData; 
}WDI_SetUapsdAcParamsReqParamsType;

/*---------------------------------------------------------------------------
  WDI_EnterBmpsReqinfoType
  Enter BMPS parameters passed to WDA from UMAC
---------------------------------------------------------------------------*/
typedef struct
{
   //TBTT value derived from the last beacon
   wpt_uint8         ucBssIdx;
   wpt_uint64        uTbtt;
   wpt_uint8         ucDtimCount;
   //DTIM period given to HAL during association may not be valid,
   //if association is based on ProbeRsp instead of beacon.
   wpt_uint8         ucDtimPeriod;
   /* DXE physical addr to be passed down to RIVA. RIVA HAL will use it to program
   DXE when DXE wakes up from power save*/
   unsigned int      dxePhyAddr;

   // For CCX and 11R Roaming
   wpt_uint32 rssiFilterPeriod;
   wpt_uint32 numBeaconPerRssiAverage;
   wpt_uint8  bRssiFilterEnable;
}WDI_EnterBmpsReqinfoType;

/*---------------------------------------------------------------------------
  WDI_EnterBmpsReqParamsType
  Enter BMPS parameters passed to WDI from WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Enter BMPS Info Type, same as tEnterBmpsParams */ 
   WDI_EnterBmpsReqinfoType wdiEnterBmpsInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb          wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                    pUserData; 
}WDI_EnterBmpsReqParamsType;

/*---------------------------------------------------------------------------
  WDI_ExitBmpsReqinfoType
  Exit BMPS parameters passed to WDA from UMAC
---------------------------------------------------------------------------*/
typedef struct
{
   wpt_uint8     ucSendDataNull;
   wpt_uint8     bssIdx;
}WDI_ExitBmpsReqinfoType;

/*---------------------------------------------------------------------------
  WDI_ExitBmpsReqParamsType
  Exit BMPS parameters passed to WDI from WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Exit BMPS Info Type, same as tExitBmpsParams */ 
   WDI_ExitBmpsReqinfoType wdiExitBmpsInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb         wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                   pUserData; 
}WDI_ExitBmpsReqParamsType;

/*---------------------------------------------------------------------------
  WDI_EnterUapsdReqinfoType
  Enter UAPSD parameters passed to WDA from UMAC
---------------------------------------------------------------------------*/
typedef struct
{
   wpt_uint8     ucBkDeliveryEnabled:1;
   wpt_uint8     ucBeDeliveryEnabled:1;
   wpt_uint8     ucViDeliveryEnabled:1;
   wpt_uint8     ucVoDeliveryEnabled:1;
   wpt_uint8     ucBkTriggerEnabled:1;
   wpt_uint8     ucBeTriggerEnabled:1;
   wpt_uint8     ucViTriggerEnabled:1;
   wpt_uint8     ucVoTriggerEnabled:1;
   wpt_uint8     bssIdx;
}WDI_EnterUapsdReqinfoType;

/*---------------------------------------------------------------------------
  WDI_EnterUapsdReqinfoType
  Enter UAPSD parameters passed to WDI from WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Enter UAPSD Info Type, same as tUapsdParams */ 
   WDI_EnterUapsdReqinfoType wdiEnterUapsdInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb           wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                     pUserData; 
}WDI_EnterUapsdReqParamsType;

/*---------------------------------------------------------------------------
  WDI_UpdateUapsdReqinfoType
  Update UAPSD parameters passed to WDA from UMAC
---------------------------------------------------------------------------*/
typedef struct
{
   wpt_uint8  ucSTAIdx;
   wpt_uint8  ucUapsdACMask; 
   wpt_uint32 uMaxSpLen;    
}WDI_UpdateUapsdReqinfoType;

/*---------------------------------------------------------------------------
  WDI_UpdateUapsdReqParamsType
  Update UAPSD parameters passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Update UAPSD Info Type, same as tUpdateUapsdParams */ 
   WDI_UpdateUapsdReqinfoType wdiUpdateUapsdInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_UpdateUapsdReqParamsType;

/*---------------------------------------------------------------------------
  WDI_ConfigureRxpFilterReqParamsType
  RXP filter parameters passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{
  /* Mode of Mcast and Bcast filters configured */
  wpt_uint8 ucSetMcstBcstFilterSetting;

  /* Mcast Bcast Filters enable/disable*/
  wpt_uint8 ucSetMcstBcstFilter;
}WDI_RxpFilterReqParamsType;

typedef struct 
{
  /* Rxp Filter */
  WDI_RxpFilterReqParamsType wdiRxpFilterParam;
  
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_ConfigureRxpFilterReqParamsType;

/*---------------------------------------------------------------------------
  WDI_BeaconFilterInfoType
  Beacon Filtering data structures passed to WDA form UMAC
---------------------------------------------------------------------------*/
typedef struct 
{ 
   wpt_uint16    usCapabilityInfo;
   wpt_uint16    usCapabilityMask;
   wpt_uint16    usBeaconInterval;
   wpt_uint16    usIeNum;
}WDI_BeaconFilterInfoType;

/*---------------------------------------------------------------------------
  WDI_BeaconFilterReqParamsType
  Beacon Filtering parameters passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Beacon Filtering Info Type, same as tBeaconFilterMsg */ 
   WDI_BeaconFilterInfoType wdiBeaconFilterInfo; 
   /*Beacon Filter(s) follow the "usIeNum" field, hence the array to ease the
   copy of params from WDA to WDI */ 
   wpt_uint8 aFilters[WDI_BEACON_FILTER_LEN];
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_BeaconFilterReqParamsType;

/*---------------------------------------------------------------------------
  WDI_RemBeaconFilterInfoType
  Beacon Filtering data structures (to be reomoved) passed to WDA form UMAC
---------------------------------------------------------------------------*/
typedef struct 
{ 
   wpt_uint8  ucIeCount;
   wpt_uint8  ucRemIeId[1];
}WDI_RemBeaconFilterInfoType;

/*---------------------------------------------------------------------------
  WDI_RemBeaconFilterReqParamsType
  Beacon Filtering parameters (to be reomoved)passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Beacon Filtering Info Type, same as tBeaconFilterMsg */ 
   WDI_RemBeaconFilterInfoType wdiBeaconFilterInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_RemBeaconFilterReqParamsType;

/*---------------------------------------------------------------------------
  WDI_RSSIThresholdsType
  RSSI thresholds data structures (to be reomoved) passed to WDA form UMAC
---------------------------------------------------------------------------*/
typedef struct
{
    wpt_int8    ucRssiThreshold1     : 8;
    wpt_int8    ucRssiThreshold2     : 8;
    wpt_int8    ucRssiThreshold3     : 8;
    wpt_uint8   bRssiThres1PosNotify : 1;
    wpt_uint8   bRssiThres1NegNotify : 1;
    wpt_uint8   bRssiThres2PosNotify : 1;
    wpt_uint8   bRssiThres2NegNotify : 1;
    wpt_uint8   bRssiThres3PosNotify : 1;
    wpt_uint8   bRssiThres3NegNotify : 1;
    wpt_uint8   bReserved10          : 2;
} WDI_RSSIThresholdsType;

/*---------------------------------------------------------------------------
  WDI_SetRSSIThresholdsReqParamsType
  RSSI thresholds parameters (to be reomoved)passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*RSSI thresholds Info Type, same as WDI_RSSIThresholds */ 
   WDI_RSSIThresholdsType     wdiRSSIThresholdsInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_SetRSSIThresholdsReqParamsType;

/*---------------------------------------------------------------------------
  WDI_HostOffloadReqType
  host offload info passed to WDA form UMAC
---------------------------------------------------------------------------*/
#ifdef WLAN_NS_OFFLOAD
typedef struct
{
   wpt_uint8 srcIPv6Addr[16];
   wpt_uint8 selfIPv6Addr[16];
   //Only support 2 possible Network Advertisement IPv6 address
   wpt_uint8 targetIPv6Addr1[16];
   wpt_uint8 targetIPv6Addr2[16];
   wpt_uint8 selfMacAddr[6];
   wpt_uint8 srcIPv6AddrValid : 1;
   wpt_uint8 targetIPv6Addr1Valid : 1;
   wpt_uint8 targetIPv6Addr2Valid : 1;
   wpt_uint8 bssIdx;
} WDI_NSOffloadParams;
#endif //WLAN_NS_OFFLOAD

typedef struct
{
   wpt_uint8 ucOffloadType;
   wpt_uint8 ucEnableOrDisable;
   wpt_uint8 bssIdx;
   union
   {
       wpt_uint8 aHostIpv4Addr [4];
       wpt_uint8 aHostIpv6Addr [16];
   } params;
} WDI_HostOffloadReqType;

/*---------------------------------------------------------------------------
  WDI_HostOffloadReqParamsType
  host offload info passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Host offload Info Type, same as tHalHostOffloadReq */ 
   WDI_HostOffloadReqType     wdiHostOffloadInfo; 
#ifdef WLAN_NS_OFFLOAD
   WDI_NSOffloadParams        wdiNsOffloadParams;
#endif //WLAN_NS_OFFLOAD
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_HostOffloadReqParamsType;

/*---------------------------------------------------------------------------
  WDI_KeepAliveReqType
  Keep Alive info passed to WDA form UMAC
---------------------------------------------------------------------------*/
typedef struct
{
    wpt_uint8  ucPacketType;
    wpt_uint32 ucTimePeriod;
    wpt_uint8  aHostIpv4Addr[4];
    wpt_uint8  aDestIpv4Addr[4];
    wpt_uint8  aDestMacAddr[6];
    wpt_uint8  bssIdx;
} WDI_KeepAliveReqType;

/*---------------------------------------------------------------------------
  WDI_KeepAliveReqParamsType
  Keep Alive passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /* Keep Alive Info Type, same as tHalKeepAliveReq */ 
   WDI_KeepAliveReqType     wdiKeepAliveInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_KeepAliveReqParamsType;

/*---------------------------------------------------------------------------
  WDI_WowlAddBcPtrnInfoType
  Wowl add ptrn info passed to WDA form UMAC
---------------------------------------------------------------------------*/
typedef struct
{
   wpt_uint8  ucPatternId;           // Pattern ID
   // Pattern byte offset from beginning of the 802.11 packet to start of the
   // wake-up pattern
   wpt_uint8  ucPatternByteOffset;   
   wpt_uint8  ucPatternSize;         // Non-Zero Pattern size
   wpt_uint8  ucPattern[WDI_WOWL_BCAST_PATTERN_MAX_SIZE]; // Pattern
   wpt_uint8  ucPatternMaskSize;     // Non-zero pattern mask size
   wpt_uint8  ucPatternMask[WDI_WOWL_BCAST_PATTERN_MAX_SIZE]; // Pattern mask
   wpt_uint8  ucPatternExt[WDI_WOWL_BCAST_PATTERN_MAX_SIZE]; // Extra pattern
   wpt_uint8  ucPatternMaskExt[WDI_WOWL_BCAST_PATTERN_MAX_SIZE]; // Extra pattern mask
   wpt_uint8  bssIdx;
} WDI_WowlAddBcPtrnInfoType;

/*---------------------------------------------------------------------------
  WDI_WowlAddBcPtrnReqParamsType
  Wowl add ptrn info passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Wowl add ptrn Info Type, same as tpSirWowlAddBcastPtrn */ 
   WDI_WowlAddBcPtrnInfoType     wdiWowlAddBcPtrnInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_WowlAddBcPtrnReqParamsType;

/*---------------------------------------------------------------------------
  WDI_WowlDelBcPtrnInfoType
  Wowl add ptrn info passed to WDA form UMAC
---------------------------------------------------------------------------*/
typedef struct
{
   /* Pattern ID of the wakeup pattern to be deleted */
   wpt_uint8  ucPatternId;
} WDI_WowlDelBcPtrnInfoType;

/*---------------------------------------------------------------------------
  WDI_WowlDelBcPtrnReqParamsType
  Wowl add ptrn info passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Wowl delete ptrn Info Type, same as WDI_WowlDelBcastPtrn */ 
   WDI_WowlDelBcPtrnInfoType     wdiWowlDelBcPtrnInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_WowlDelBcPtrnReqParamsType;

/*---------------------------------------------------------------------------
  WDI_WowlEnterInfoType
  Wowl enter info passed to WDA form UMAC
---------------------------------------------------------------------------*/
typedef struct
{
   /* Enables/disables magic packet filtering */
   wpt_uint8   ucMagicPktEnable; 

   /* Magic pattern */
   wpt_macAddr magicPtrn;

   /* Enables/disables packet pattern filtering in firmware. 
      Enabling this flag enables broadcast pattern matching 
      in Firmware. If unicast pattern matching is also desired,  
      ucUcastPatternFilteringEnable flag must be set tot true 
      as well 
   */
   wpt_uint8   ucPatternFilteringEnable;

   /* Enables/disables unicast packet pattern filtering. 
      This flag specifies whether we want to do pattern match 
      on unicast packets as well and not just broadcast packets. 
      This flag has no effect if the ucPatternFilteringEnable 
      (main controlling flag) is set to false
   */
   wpt_uint8   ucUcastPatternFilteringEnable;                     

   /* This configuration is valid only when magicPktEnable=1. 
    * It requests hardware to wake up when it receives the 
    * Channel Switch Action Frame.
    */
   wpt_uint8   ucWowChnlSwitchRcv;

   /* This configuration is valid only when magicPktEnable=1. 
    * It requests hardware to wake up when it receives the 
    * Deauthentication Frame. 
    */
   wpt_uint8   ucWowDeauthRcv;

   /* This configuration is valid only when magicPktEnable=1. 
    * It requests hardware to wake up when it receives the 
    * Disassociation Frame. 
    */
   wpt_uint8   ucWowDisassocRcv;

   /* This configuration is valid only when magicPktEnable=1. 
    * It requests hardware to wake up when it has missed
    * consecutive beacons. This is a hardware register
    * configuration (NOT a firmware configuration). 
    */
   wpt_uint8   ucWowMaxMissedBeacons;

   /* This configuration is valid only when magicPktEnable=1. 
    * This is a timeout value in units of microsec. It requests
    * hardware to unconditionally wake up after it has stayed
    * in WoWLAN mode for some time. Set 0 to disable this feature.      
    */
   wpt_uint8   ucWowMaxSleepUsec;

#ifdef WLAN_WAKEUP_EVENTS
    /* This configuration directs the WoW packet filtering to look for EAP-ID
     * requests embedded in EAPOL frames and use this as a wake source.
     */
    wpt_uint8   ucWoWEAPIDRequestEnable;

    /* This configuration directs the WoW packet filtering to look for EAPOL-4WAY
     * requests and use this as a wake source.
     */
    wpt_uint8   ucWoWEAPOL4WayEnable;

    /* This configuration allows a host wakeup on an network scan offload match.
     */
    wpt_uint8   ucWowNetScanOffloadMatch;

    /* This configuration allows a host wakeup on any GTK rekeying error.
     */
    wpt_uint8   ucWowGTKRekeyError;

    /* This configuration allows a host wakeup on BSS connection loss.
     */
    wpt_uint8   ucWoWBSSConnLoss;
#endif // WLAN_WAKEUP_EVENTS
} WDI_WowlEnterInfoType;

/*---------------------------------------------------------------------------
  WDI_WowlEnterReqParamsType
  Wowl enter info passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Wowl delete ptrn Info Type, same as WDI_SmeWowlEnterParams */ 
   WDI_WowlEnterInfoType     wdiWowlEnterInfo; 
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_WowlEnterReqParamsType;

/*---------------------------------------------------------------------------
  WDI_ConfigureAppsCpuWakeupStateReqParamsType
  Apps Cpu Wakeup State parameters passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /*Depicts the state of the Apps CPU */ 
   wpt_boolean                bIsAppsAwake;
   /*Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
}WDI_ConfigureAppsCpuWakeupStateReqParamsType;
/*---------------------------------------------------------------------------
  WDI_FlushAcReqinfoType
---------------------------------------------------------------------------*/
typedef struct
{
   // Message Type
   wpt_uint16 usMesgType;

   // Message Length
   wpt_uint16 usMesgLen;

   // Station Index. originates from HAL
   wpt_uint8  ucSTAId;

   // TID for which the transmit queue is being flushed 
   wpt_uint8  ucTid;
  
}WDI_FlushAcReqinfoType;

/*---------------------------------------------------------------------------
  WDI_FlushAcReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*AC Info */
  WDI_FlushAcReqinfoType  wdiFlushAcInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_FlushAcReqParamsType;

/*---------------------------------------------------------------------------
  WDI_BtAmpEventinfoType
      BT-AMP Event Structure
---------------------------------------------------------------------------*/
typedef struct
{
  wpt_uint8 ucBtAmpEventType;

} WDI_BtAmpEventinfoType;

/*---------------------------------------------------------------------------
  WDI_BtAmpEventParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*BT AMP event Info */
  WDI_BtAmpEventinfoType  wdiBtAmpEventInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_BtAmpEventParamsType;

#ifdef FEATURE_OEM_DATA_SUPPORT

#ifndef OEM_DATA_REQ_SIZE
#define OEM_DATA_REQ_SIZE 70
#endif
#ifndef OEM_DATA_RSP_SIZE
#define OEM_DATA_RSP_SIZE 968
#endif

/*----------------------------------------------------------------------------
  WDI_oemDataReqInfoType
----------------------------------------------------------------------------*/
typedef struct
{
  wpt_macAddr                  selfMacAddr;
  wpt_uint8                    oemDataReq[OEM_DATA_REQ_SIZE];
}WDI_oemDataReqInfoType;

/*----------------------------------------------------------------------------
  WDI_oemDataReqParamsType
----------------------------------------------------------------------------*/
typedef struct
{
  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb                wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*                          pUserData;

  /*OEM DATA REQ Info */
  WDI_oemDataReqInfoType         wdiOemDataReqInfo;

}WDI_oemDataReqParamsType;

/*----------------------------------------------------------------------------
  WDI_oemDataRspParamsType
----------------------------------------------------------------------------*/
typedef struct
{
  wpt_uint8           oemDataRsp[OEM_DATA_RSP_SIZE];
}WDI_oemDataRspParamsType;

#endif /* FEATURE_OEM_DATA_SUPPORT */

#ifdef WLAN_FEATURE_VOWIFI_11R
/*---------------------------------------------------------------------------
  WDI_AggrAddTSReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*STA Index*/
  wpt_uint8         ucSTAIdx; 

  /*Identifier for TSpec*/
  wpt_uint8         ucTspecIdx;

  /*Tspec IE negotiated OTA*/
  WDI_TspecIEType   wdiTspecIE[WDI_MAX_NO_AC];

  /*UAPSD delivery and trigger enabled flags */
  wpt_uint8         ucUapsdFlags;

  /*SI for each AC*/
  wpt_uint8         ucServiceInterval[WDI_MAX_NO_AC];

  /*Suspend Interval for each AC*/
  wpt_uint8         ucSuspendInterval[WDI_MAX_NO_AC];

  /*DI for each AC*/
  wpt_uint8         ucDelayedInterval[WDI_MAX_NO_AC];

}WDI_AggrAddTSReqInfoType;


/*---------------------------------------------------------------------------
  WDI_AggrAddTSReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*TSpec Info */
  WDI_AggrAddTSReqInfoType  wdiAggrTsInfo; 

  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}WDI_AggrAddTSReqParamsType;

#endif /* WLAN_FEATURE_VOWIFI_11R */

#ifdef ANI_MANF_DIAG
/*---------------------------------------------------------------------------
  WDI_FTMCommandReqType
---------------------------------------------------------------------------*/
typedef struct
{
   /* FTM Command Body length */
   wpt_uint32   bodyLength;
   /* Actual FTM Command body */
   void        *FTMCommandBody;
}WDI_FTMCommandReqType;
#endif /* ANI_MANF_DIAG */

/*---------------------------------------------------------------------------
  WDI_WlanSuspendInfoType
---------------------------------------------------------------------------*/
typedef struct 
{
  /* Mode of Mcast and Bcast filters configured */
  wpt_uint8 ucConfiguredMcstBcstFilterSetting;
}WDI_WlanSuspendInfoType;

/*---------------------------------------------------------------------------
  WDI_SuspendParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  WDI_WlanSuspendInfoType wdiSuspendParams;

   /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;

}WDI_SuspendParamsType;

/*---------------------------------------------------------------------------
  WDI_WlanResumeInfoType
---------------------------------------------------------------------------*/
typedef struct 
{
  /* Mode of Mcast and Bcast filters configured */
  wpt_uint8 ucConfiguredMcstBcstFilterSetting;
}WDI_WlanResumeInfoType;

/*---------------------------------------------------------------------------
  WDI_ResumeParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  WDI_WlanResumeInfoType wdiResumeParams;

   /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;

}WDI_ResumeParamsType;

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/*---------------------------------------------------------------------------
 * WDI_GTK_OFFLOAD_REQ
 *--------------------------------------------------------------------------*/

typedef struct
{
  wpt_uint32     ulFlags;             /* optional flags */
  wpt_uint8      aKCK[16];            /* Key confirmation key */ 
  wpt_uint8      aKEK[16];            /* key encryption key */
  wpt_uint64     ullKeyReplayCounter; /* replay counter */
} WDI_GtkOffloadReqParams;

typedef struct
{
   WDI_GtkOffloadReqParams gtkOffloadReqParams;

   /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
} WDI_GtkOffloadReqMsg;

/*---------------------------------------------------------------------------
 * WDI_GTK_OFFLOAD_RSP
 *--------------------------------------------------------------------------*/
typedef struct
{
    /* success or failure */
    wpt_uint32   ulStatus;
} WDI_GtkOffloadRspParams;

typedef struct
{
   WDI_GtkOffloadRspParams gtkOffloadRspParams;

   /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
} WDI_GtkOffloadRspMsg;


/*---------------------------------------------------------------------------
* WDI_GTK_OFFLOAD_GETINFO_REQ
*--------------------------------------------------------------------------*/

typedef struct
{
   /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
} WDI_GtkOffloadGetInfoReqMsg;

/*---------------------------------------------------------------------------
* WDI_GTK_OFFLOAD_GETINFO_RSP
*--------------------------------------------------------------------------*/
typedef struct
{
   wpt_uint32   ulStatus;             /* success or failure */
   wpt_uint64   ullKeyReplayCounter;  /* current replay counter value */
   wpt_uint32   ulTotalRekeyCount;    /* total rekey attempts */
   wpt_uint32   ulGTKRekeyCount;      /* successful GTK rekeys */
   wpt_uint32   ulIGTKRekeyCount;     /* successful iGTK rekeys */
} WDI_GtkOffloadGetInfoRspParams;

typedef struct
{
   WDI_GtkOffloadGetInfoRspParams gtkOffloadGetInfoRspParams;

   /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
}  WDI_GtkOffloadGetInfoRspMsg;
#endif // WLAN_FEATURE_GTK_OFFLOAD

/*---------------------------------------------------------------------------
  WDI_SuspendResumeRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Status of the response*/
  WDI_Status   wdiStatus; 
}WDI_SuspendResumeRspParamsType;


#ifdef FEATURE_WLAN_SCAN_PNO

/*Max number of channels for a given network supported by PNO*/
#define WDI_PNO_MAX_NETW_CHANNELS  26

/*The max number of programable networks for PNO*/
#define WDI_PNO_MAX_SUPP_NETWORKS  16

/*The max number of scan timers programable in Riva*/
#define WDI_PNO_MAX_SCAN_TIMERS    10

#define WDI_PNO_MAX_PROBE_SIZE    450


/*---------------------------------------------------------------------------
  WDI_AuthType
---------------------------------------------------------------------------*/
typedef enum 
{
    WDI_AUTH_TYPE_ANY     = 0, 

    WDI_AUTH_TYPE_NONE,   
    WDI_AUTH_TYPE_OPEN_SYSTEM,
    WDI_AUTH_TYPE_SHARED_KEY,

    WDI_AUTH_TYPE_WPA,
    WDI_AUTH_TYPE_WPA_PSK,
    WDI_AUTH_TYPE_WPA_NONE,

    WDI_AUTH_TYPE_RSN,
    WDI_AUTH_TYPE_RSN_PSK,
    WDI_AUTH_TYPE_FT_RSN,
    WDI_AUTH_TYPE_FT_RSN_PSK,
    WDI_AUTH_TYPE_WAPI_WAI_CERTIFICATE,
    WDI_AUTH_TYPE_WAPI_WAI_PSK,
    WDI_AUTH_TYPE_MAX = 0xFFFFFFFF /*expanding the type to UINT32*/

}WDI_AuthType;

/*---------------------------------------------------------------------------
  WDI_EdType
---------------------------------------------------------------------------*/
typedef enum
{
    WDI_ED_ANY        = 0, 
    WDI_ED_NONE, 
    WDI_ED_WEP40,
    WDI_ED_WEP104,
    WDI_ED_TKIP,
    WDI_ED_CCMP,
    WDI_ED_WPI,
    WDI_ED_AES_128_CMAC,
    WDI_ED_MAX = 0xFFFFFFFF /*expanding the type to UINT32*/
} WDI_EdType;


/*---------------------------------------------------------------------------
  WDI_PNOMode
---------------------------------------------------------------------------*/
typedef enum
{
  /*Network offload is to start immediately*/
  WDI_PNO_MODE_IMMEDIATE,

  /*Network offload is to start on host suspend*/
  WDI_PNO_MODE_ON_SUSPEND,

  /*Network offload is to start on host resume*/
  WDI_PNO_MODE_ON_RESUME,
  WDI_PNO_MODE_MAX = 0xFFFFFFFF
} WDI_PNOMode;

/* SSID broadcast  type */
typedef enum 
{
  WDI_BCAST_UNKNOWN      = 0,
  WDI_BCAST_NORMAL       = 1,
  WDI_BCAST_HIDDEN       = 2,

  WDI_BCAST_TYPE_MAX     = 0xFFFFFFFF
} WDI_SSIDBcastType;

/*---------------------------------------------------------------------------
  WDI_NetworkType
---------------------------------------------------------------------------*/
typedef struct 
{
  /*The SSID of the preferred network*/
  WDI_MacSSid  ssId;

  /*The authentication method of the preferred network*/
  WDI_AuthType wdiAuth; 

  /*The encryption method of the preferred network*/
  WDI_EdType   wdiEncryption; 

  /*SSID broadcast type, normal, hidden or unknown*/
  WDI_SSIDBcastType wdiBcastNetworkType;

  /*channel count - 0 for all channels*/
  wpt_uint8    ucChannelCount;

  /*the actual channels*/
  wpt_uint8    aChannels[WDI_PNO_MAX_NETW_CHANNELS];

  /*rssi threshold that a network must meet to be considered, 0 - for any*/
  wpt_uint8    rssiThreshold;
} WDI_NetworkType; 


/*---------------------------------------------------------------------------
  WDI_ScanTimer
---------------------------------------------------------------------------*/
typedef struct 
{
  /*The timer value*/
  wpt_uint32    uTimerValue; 

  /*The amount of time we should be repeating the interval*/
  wpt_uint32    uTimerRepeat; 
} WDI_ScanTimer; 

/*---------------------------------------------------------------------------
  WDI_ScanTimersType
---------------------------------------------------------------------------*/
typedef struct
{
  /*The number of value pair intervals present in the array*/
  wpt_uint8      ucScanTimersCount; 

  /*The time-repeat value pairs*/
  WDI_ScanTimer  aTimerValues[WDI_PNO_MAX_SCAN_TIMERS]; 
} WDI_ScanTimersType;

/*---------------------------------------------------------------------------
  WDI_PNOScanReqType
---------------------------------------------------------------------------*/
typedef struct 
{
  /*Enable or disable PNO feature*/
  wpt_uint8           bEnable;

  /*PNO mode requested*/
  WDI_PNOMode         wdiModePNO;

  /*Network count*/
  wpt_uint8           ucNetworksCount; 

  /*The networks to look for*/
  WDI_NetworkType     aNetworks[WDI_PNO_MAX_SUPP_NETWORKS];

  /*Scan timer intervals*/
  WDI_ScanTimersType  scanTimers; 

  /*Probe template for 2.4GHz band*/
  wpt_uint16          us24GProbeSize; 
  wpt_uint8           a24GProbeTemplate[WDI_PNO_MAX_PROBE_SIZE];

  /*Probe template for 5GHz band*/
  wpt_uint16          us5GProbeSize; 
  wpt_uint8           a5GProbeTemplate[WDI_PNO_MAX_PROBE_SIZE];
} WDI_PNOScanReqType;

/*---------------------------------------------------------------------------
  WDI_PNOScanReqParamsType
  PNO info passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /* PNO Info Type, same as tPrefNetwListParams */ 
   WDI_PNOScanReqType        wdiPNOScanInfo; 
   /* Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /* The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
} WDI_PNOScanReqParamsType;

/*---------------------------------------------------------------------------
  WDI_SetRssiFilterReqParamsType
  PNO info passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /* RSSI Threshold */ 
   wpt_uint8                  rssiThreshold; 
   /* Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /* The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
} WDI_SetRssiFilterReqParamsType;

/*---------------------------------------------------------------------------
  WDI_UpdateScanParamsInfo
---------------------------------------------------------------------------*/
typedef struct 
{
  /*Is 11d enabled*/
  wpt_uint8    b11dEnabled; 

  /*Was UMAc able to find the regulatory domain*/
  wpt_uint8    b11dResolved;

  /*Number of channel allowed in the regulatory domain*/
  wpt_uint8    ucChannelCount; 

  /*The actual channels allowed in the regulatory domain*/
  wpt_uint8    aChannels[WDI_PNO_MAX_NETW_CHANNELS]; 

  /*Passive min channel time*/
  wpt_uint16   usPassiveMinChTime; 

  /*Passive max channel time*/
  wpt_uint16   usPassiveMaxChTime; 

  /*Active min channel time*/
  wpt_uint16   usActiveMinChTime; 

  /*Active max channel time*/
  wpt_uint16   usActiveMaxChTime; 

  /*channel bonding info*/
  wpt_uint8    cbState; 
} WDI_UpdateScanParamsInfo;

/*---------------------------------------------------------------------------
  WDI_UpdateScanParamsInfoType
  UpdateScanParams info passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /* PNO Info Type, same as tUpdateScanParams */ 
   WDI_UpdateScanParamsInfo   wdiUpdateScanParamsInfo; 
   /* Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /* The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData; 
} WDI_UpdateScanParamsInfoType;
#endif // FEATURE_WLAN_SCAN_PNO

/*---------------------------------------------------------------------------
  WDI_UpdateScanParamsInfo
---------------------------------------------------------------------------*/
typedef struct 
{
   /*  Ignore DTIM */
  wpt_uint32 uIgnoreDTIM;

  /*DTIM Period*/
  wpt_uint32 uDTIMPeriod;

  /* Listen Interval */
  wpt_uint32 uListenInterval;

  /* Broadcast Multicas Filter  */
  wpt_uint32 uBcastMcastFilter;

  /* Beacon Early Termination */
  wpt_uint32 uEnableBET;

  /* Beacon Early Termination Interval */
  wpt_uint32 uBETInterval; 

} WDI_SetPowerParamsInfo;

/*---------------------------------------------------------------------------
  WDI_UpdateScanParamsInfoType
  UpdateScanParams info passed to WDI form WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
   /* Power params Info Type, same as tSetPowerParamsReq */ 
   WDI_SetPowerParamsInfo     wdiSetPowerParamsInfo; 
   /* Request status callback offered by UMAC - it is called if the current req
   has returned PENDING as status; it delivers the status of sending the message
   over the BUS */ 
   WDI_ReqStatusCb            wdiReqStatusCB; 
   /* The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */ 
   void*                      pUserData;
}WDI_SetPowerParamsReqParamsType;

/*---------------------------------------------------------------------------
  WDI_SetTxPerTrackingConfType
  Wowl add ptrn info passed to WDA form UMAC
---------------------------------------------------------------------------*/
typedef struct
{
   wpt_uint8  ucTxPerTrackingEnable;     /* 0: disable, 1:enable */
   wpt_uint8  ucTxPerTrackingPeriod;        /* Check period, unit is sec. */
   wpt_uint8  ucTxPerTrackingRatio;      /* (Fail TX packet)/(Total TX packet) ratio, the unit is 10%. */
   wpt_uint32 uTxPerTrackingWatermark;         /* A watermark of check number, once the tx packet exceed this number, we do the check, default is 5 */
} WDI_TxPerTrackingParamType;

/*---------------------------------------------------------------------------
  WDI_SetTxPerTrackingReqParamsType
  Tx PER Tracking parameters passed to WDI from WDA
---------------------------------------------------------------------------*/
typedef struct 
{ 
  /* Configurations for Tx PER Tracking */ 
  WDI_TxPerTrackingParamType     wdiTxPerTrackingParam;
  /*Request status callback offered by UMAC - it is called if the current req
    has returned PENDING as status; it delivers the status of sending the message
    over the BUS */ 
  WDI_ReqStatusCb            wdiReqStatusCB; 
  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */ 
  void*                      pUserData; 
}WDI_SetTxPerTrackingReqParamsType;

#ifdef WLAN_FEATURE_PACKET_FILTERING
/*---------------------------------------------------------------------------
  Packet Filtering Parameters
---------------------------------------------------------------------------*/

#define    WDI_IPV4_ADDR_LEN                  4
#define    WDI_MAC_ADDR_LEN                   6
#define    WDI_MAX_FILTER_TEST_DATA_LEN       8
#define    WDI_MAX_NUM_MULTICAST_ADDRESS    240
#define    WDI_MAX_NUM_FILTERS               20 
#define    WDI_MAX_NUM_TESTS_PER_FILTER      10 

//
// Receive Filter Parameters
//
typedef enum
{
  WDI_RCV_FILTER_TYPE_INVALID,
  WDI_RCV_FILTER_TYPE_FILTER_PKT,
  WDI_RCV_FILTER_TYPE_BUFFER_PKT,
  WDI_RCV_FILTER_TYPE_MAX_ENUM_SIZE
}WDI_ReceivePacketFilterType;

typedef enum 
{
  WDI_FILTER_HDR_TYPE_INVALID,
  WDI_FILTER_HDR_TYPE_MAC,
  WDI_FILTER_HDR_TYPE_ARP,
  WDI_FILTER_HDR_TYPE_IPV4,
  WDI_FILTER_HDR_TYPE_IPV6,
  WDI_FILTER_HDR_TYPE_UDP,
  WDI_FILTER_HDR_TYPE_MAX
}WDI_RcvPktFltProtocolType;

typedef enum 
{
  WDI_FILTER_CMP_TYPE_INVALID,
  WDI_FILTER_CMP_TYPE_EQUAL,
  WDI_FILTER_CMP_TYPE_MASK_EQUAL,
  WDI_FILTER_CMP_TYPE_NOT_EQUAL,
  WDI_FILTER_CMP_TYPE_MASK_NOT_EQUAL,
  WDI_FILTER_CMP_TYPE_MAX
}WDI_RcvPktFltCmpFlagType;

typedef struct
{
  WDI_RcvPktFltProtocolType          protocolLayer;
  WDI_RcvPktFltCmpFlagType           cmpFlag;
/* Length of the data to compare */
  wpt_uint16                         dataLength; 
/* from start of the respective frame header */  
  wpt_uint8                          dataOffset; 
  wpt_uint8                          reserved; /* Reserved field */
/* Data to compare */
  wpt_uint8                          compareData[WDI_MAX_FILTER_TEST_DATA_LEN];  
/* Mask to be applied on the received packet data before compare */
  wpt_uint8                          dataMask[WDI_MAX_FILTER_TEST_DATA_LEN];   
}WDI_RcvPktFilterFieldParams;

typedef struct
{
  wpt_uint8                       filterId; 
  wpt_uint8                       filterType;
  wpt_uint32                      numFieldParams;
  wpt_uint32                      coalesceTime;
  WDI_RcvPktFilterFieldParams     paramsData[1];
  wpt_macAddr                     selfMacAddr;
  wpt_macAddr                     bssId;
}WDI_RcvPktFilterCfgType;

typedef struct 
{
  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 
    
  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
    
  // Variable length packet filter field params
  WDI_RcvPktFilterCfgType wdiPktFilterCfg;
} WDI_SetRcvPktFilterReqParamsType;

//
// Filter Packet Match Count Parameters
//
typedef struct
{
  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 
    
  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
} WDI_RcvFltPktMatchCntReqParamsType;

typedef struct
{
  wpt_uint8    filterId;
  wpt_uint32   matchCnt;
} WDI_RcvFltPktMatchCnt;

typedef struct
{
  /* Success or Failure */
  wpt_uint32                 status;
  WDI_RcvFltPktMatchCnt    filterMatchCnt[WDI_MAX_NUM_FILTERS];
  
  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 
    
  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
} WDI_RcvFltPktMatchRspParams;

typedef struct
{
  WDI_RcvFltPktMatchRspParams fltPktMatchRspParams;
  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 
    
  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
} WDI_RcvFltPktMatchCntRspParamsType;


//
// Receive Filter Clear Parameters
//
typedef struct
{
  wpt_uint32   status;  /* only valid for response message */
  wpt_uint8    filterId;
  wpt_macAddr  selfMacAddr;
  wpt_macAddr  bssId;
}WDI_RcvFltPktClearParam;

typedef struct
{
  WDI_RcvFltPktClearParam     filterClearParam;
  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 
    
  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
} WDI_RcvFltPktClearReqParamsType;

//
// Multicast Address List Parameters
//
typedef struct 
{
  wpt_uint32     ulMulticastAddrCnt;
  wpt_macAddr    multicastAddr[WDI_MAX_NUM_MULTICAST_ADDRESS];
  wpt_macAddr    selfMacAddr;
  wpt_macAddr    bssId;
} WDI_RcvFltMcAddrListType;

typedef struct
{
  WDI_RcvFltMcAddrListType         mcAddrList;
  /*Request status callback offered by UMAC - it is called if the current
    req has returned PENDING as status; it delivers the status of sending
    the message over the BUS */
  WDI_ReqStatusCb   wdiReqStatusCB; 
    
  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  void*             pUserData;
} WDI_RcvFltPktSetMcListReqParamsType;
#endif // WLAN_FEATURE_PACKET_FILTERING

/*---------------------------------------------------------------------------
  WDI_HALDumpCmdReqInfoType
---------------------------------------------------------------------------*/
typedef struct
{
  /*command*/
  wpt_uint32 command;

  /*Arguments*/
  wpt_uint32 argument1;
  wpt_uint32 argument2;
  wpt_uint32 argument3;
  wpt_uint32 argument4;

}WDI_HALDumpCmdReqInfoType;

/*---------------------------------------------------------------------------
 WDI_HALDumpCmdReqParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*NV Blob Info*/
  WDI_HALDumpCmdReqInfoType  wdiHALDumpCmdInfoType; 

  /*Request status callback offered by UMAC - it is called if the current
   req has returned PENDING as status; it delivers the status of sending
   the message over the BUS */
  WDI_ReqStatusCb       wdiReqStatusCB; 

  /*The user data passed in by UMAC, it will be sent back when the above
   function pointer will be called */
  void*                 pUserData;
  
}WDI_HALDumpCmdReqParamsType;


/*---------------------------------------------------------------------------
  WDI_HALDumpCmdRspParamsType
---------------------------------------------------------------------------*/
typedef struct
{
  /*Result of the operation*/
  WDI_Status       wdiStatus;

  /* length of the buffer */
  wpt_uint16       usBufferLen;
    
  /* Buffer */
  wpt_uint8       *pBuffer;
}WDI_HALDumpCmdRspParamsType;


/*---------------------------------------------------------------------------
  WDI_SetTmLevelReqType
---------------------------------------------------------------------------*/
typedef struct
{
  wpt_uint16       tmMode;
  wpt_uint16       tmLevel;
  void*            pUserData;
}WDI_SetTmLevelReqType;

/*---------------------------------------------------------------------------
  WDI_SetTmLevelRspType
---------------------------------------------------------------------------*/
typedef struct
{
  WDI_Status       wdiStatus;
  void*            pUserData;
}WDI_SetTmLevelRspType;

/*----------------------------------------------------------------------------
 *   WDI callback types
 *--------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Start response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiRspParams:  response parameters received from HAL
    pUserData:      user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void (*WDI_StartRspCb)(WDI_StartRspParamsType*   pwdiRspParams,
                               void*                     pUserData);

/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Stop response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
 
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void (*WDI_StopRspCb)(WDI_Status   wdiStatus,
                              void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received an Init Scan response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_InitScanRspCb)(WDI_Status   wdiStatus,
                                   void*        pUserData);


/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a StartScan response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiParams:  response params received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_StartScanRspCb)(WDI_StartScanRspParamsType*  wdiParams,
                                    void*                        pUserData);


/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a End Scan response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_EndScanRspCb)(WDI_Status   wdiStatus,
                                  void*        pUserData);


/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Finish Scan response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_FinishScanRspCb)(WDI_Status   wdiStatus,
                                     void*        pUserData);


/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Join response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
 
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_JoinRspCb)(WDI_Status   wdiStatus,
                               void*        pUserData);


/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Config BSS response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiConfigBSSRsp:  response parameters received from HAL
    pUserData:      user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_ConfigBSSRspCb)(
                            WDI_ConfigBSSRspParamsType*   pwdiConfigBSSRsp,
                            void*                        pUserData);


/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Del BSS response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiDelBSSRsp:  response parameters received from HAL
    pUserData:      user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_DelBSSRspCb)(WDI_DelBSSRspParamsType*  pwdiDelBSSRsp,
                                 void*                     pUserData);


/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Post Assoc response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiRspParams:  response parameters received from HAL
    pUserData:      user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_PostAssocRspCb)(
                               WDI_PostAssocRspParamsType*  pwdiPostAssocRsp,
                               void*                        pUserData);


/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Del STA response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiDelSTARsp:  response parameters received from HAL
    pUserData:      user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_DelSTARspCb)(WDI_DelSTARspParamsType*   pwdiDelSTARsp,
                                 void*                      pUserData);



/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Set BSS Key response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetBSSKeyRspCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Remove BSS Key
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_RemoveBSSKeyRspCb)(WDI_Status   wdiStatus,
                                       void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Set STA Key response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetSTAKeyRspCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);

 
/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Remove STA Key
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_RemoveSTAKeyRspCb)(WDI_Status   wdiStatus,
                                       void*        pUserData);


#ifdef FEATURE_WLAN_CCX 
/*---------------------------------------------------------------------------
   WDI_TsmRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a TSM Stats response from the underlying device.
 
   PARAMETERS 

    IN
    pTSMStats:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_TsmRspCb)(WDI_TSMStatsRspParamsType *pTSMStats,
                                void*        pUserData);
#endif

/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Add TS response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_AddTsRspCb)(WDI_Status   wdiStatus,
                                void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Del TS response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_DelTsRspCb)(WDI_Status   wdiStatus,
                                void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received an Update EDCA Params
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
 
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_UpdateEDCAParamsRspCb)(WDI_Status   wdiStatus,
                                           void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Add BA response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_AddBASessionRspCb)(
                            WDI_AddBASessionRspParamsType* wdiAddBASessionRsp,
                            void*                         pUserData);

 
/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Del BA response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
    
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_DelBARspCb)(WDI_Status   wdiStatus,
                                void*        pUserData);

 
/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Switch Ch response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiRspParams:  response parameters received from HAL
    pUserData:      user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SwitchChRspCb)(WDI_SwitchCHRspParamsType*  pwdiSwitchChRsp,
                                   void*                       pUserData);

 
/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Config STA response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiRspParams:  response parameters received from HAL
    pUserData:      user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_ConfigSTARspCb)(
                            WDI_ConfigSTARspParamsType*  pwdiConfigSTARsp,
                            void*                        pUserData);

 
/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Set Link State
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiRspParams:  response parameters received from HAL
    pUserData:      user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetLinkStateRspCb)( WDI_Status   wdiStatus,
                                        void*        pUserData);

 
/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Get Stats response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiRspParams:  response parameters received from HAL
    pUserData:      user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_GetStatsRspCb)(WDI_GetStatsRspParamsType*  pwdiGetStatsRsp,
                                   void*                       pUserData);

 
/*---------------------------------------------------------------------------
   WDI_StartRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Update Cfg response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_UpdateCfgRspCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_AddBARspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a ADD BA response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_AddBARspCb)(WDI_AddBARspinfoType*   wdiAddBARsp,
                                    void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_TriggerBARspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a ADD BA response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_TriggerBARspCb)(WDI_TriggerBARspParamsType*   wdiTriggerBARsp,
                                    void*        pUserData);


/*---------------------------------------------------------------------------
   WDI_UpdateBeaconParamsRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Update Beacon Params response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_UpdateBeaconParamsRspCb)(WDI_Status   wdiStatus,
                                void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_SendBeaconParamsRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Send Beacon Params response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SendBeaconParamsRspCb)(WDI_Status   wdiStatus,
                                void*        pUserData);

/*---------------------------------------------------------------------------
   WDA_SetMaxTxPowerRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a set max Tx Power response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void (*WDA_SetMaxTxPowerRspCb)(WDI_SetMaxTxPowerRspMsg *wdiSetMaxTxPowerRsp,
                                             void* pUserData);

/*---------------------------------------------------------------------------
   WDI_UpdateProbeRspTemplateRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Probe RSP Template
   Update  response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
    
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_UpdateProbeRspTemplateRspCb)(WDI_Status   wdiStatus,
                                               void*        pUserData);

#ifdef WLAN_FEATURE_P2P
/*---------------------------------------------------------------------------
   WDI_SetP2PGONOAReqParamsRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a P2P GO NOA Params response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetP2PGONOAReqParamsRspCb)(WDI_Status   wdiStatus,
                                void*        pUserData);
#endif


/*---------------------------------------------------------------------------
   WDI_SetPwrSaveCfgCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a set Power Save CFG
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetPwrSaveCfgCb)(WDI_Status   wdiStatus,
                                     void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_SetUapsdAcParamsCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a set UAPSD params
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetUapsdAcParamsCb)(WDI_Status   wdiStatus,
                                        void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_EnterImpsRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Enter IMPS response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_EnterImpsRspCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_ExitImpsRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Exit IMPS response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_ExitImpsRspCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_EnterBmpsRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a enter BMPS response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_EnterBmpsRspCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_ExitBmpsRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a exit BMPS response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_ExitBmpsRspCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_EnterUapsdRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a enter UAPSD response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_EnterUapsdRspCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_ExitUapsdRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a exit UAPSD response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_ExitUapsdRspCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_UpdateUapsdParamsCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a update UAPSD params
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_UpdateUapsdParamsCb)(WDI_Status   wdiStatus,
                                        void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_ConfigureRxpFilterCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a config RXP filter
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_ConfigureRxpFilterCb)(WDI_Status   wdiStatus,
                                          void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_SetBeaconFilterCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a set beacon filter
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetBeaconFilterCb)(WDI_Status   wdiStatus,
                                       void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_RemBeaconFilterCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a remove beacon filter
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_RemBeaconFilterCb)(WDI_Status   wdiStatus,
                                       void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_SetRSSIThresholdsCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a set RSSI thresholds
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetRSSIThresholdsCb)(WDI_Status   wdiStatus,
                                         void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_HostOffloadCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a host offload
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_HostOffloadCb)(WDI_Status   wdiStatus,
                                   void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_KeepAliveCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Keep Alive
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_KeepAliveCb)(WDI_Status   wdiStatus,
                                   void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_WowlAddBcPtrnCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Wowl add Bcast ptrn
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_WowlAddBcPtrnCb)(WDI_Status   wdiStatus,
                                     void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_WowlDelBcPtrnCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Wowl delete Bcast ptrn
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_WowlDelBcPtrnCb)(WDI_Status   wdiStatus,
                                     void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_WowlEnterReqCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Wowl enter
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_WowlEnterReqCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_WowlExitReqCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Wowl exit
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_WowlExitReqCb)(WDI_Status   wdiStatus,
                                   void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_ConfigureAppsCpuWakeupStateCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a config Apps Cpu Wakeup
   State response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_ConfigureAppsCpuWakeupStateCb)(WDI_Status   wdiStatus,
                                                   void*        pUserData);
/*---------------------------------------------------------------------------
   WDI_NvDownloadRspCb
 
   DESCRIPTION
 
   This callback is invoked by DAL when it has received a NV Download response
   from the underlying device.
 
   PARAMETERS 

   IN
   wdiStatus:response status received from HAL
    pUserData:user data  
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_NvDownloadRspCb)(WDI_NvDownloadRspInfoType* wdiNvDownloadRsp,
                                          void*  pUserData);
/*---------------------------------------------------------------------------
   WDI_FlushAcRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Flush AC response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
    
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_FlushAcRspCb)(WDI_Status   wdiStatus,
                                  void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_BtAmpEventRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Bt AMP event response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_BtAmpEventRspCb)(WDI_Status   wdiStatus,
                                     void*        pUserData);

#ifdef FEATURE_OEM_DATA_SUPPORT
/*---------------------------------------------------------------------------
   WDI_oemDataRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Start oem data response from
   the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  
    
    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_oemDataRspCb)(WDI_oemDataRspParamsType* wdiOemDataRspParams, 
                                  void*                     pUserData);

#endif

/*---------------------------------------------------------------------------
   WDI_HostResumeEventRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Bt AMP event response
   from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_HostResumeEventRspCb)(
                        WDI_SuspendResumeRspParamsType   *resumeRspParams,
                        void*        pUserData);


#ifdef WLAN_FEATURE_VOWIFI_11R
/*---------------------------------------------------------------------------
   WDI_AggrAddTsRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Aggregated Add TS
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_AggrAddTsRspCb)(WDI_Status   wdiStatus,
                                    void*        pUserData);
#endif /* WLAN_FEATURE_VOWIFI_11R */

#ifdef ANI_MANF_DIAG
/*---------------------------------------------------------------------------
   WDI_FTMCommandRspCb
 
   DESCRIPTION   
 
   FTM Command response CB
 
   PARAMETERS 

    IN
    ftmCMDRspdata:  FTM response data from HAL
    pUserData:  user data  
    
  
  RETURN VALUE 
    NONE
---------------------------------------------------------------------------*/
typedef void (*WDI_FTMCommandRspCb)(void *ftmCMDRspdata,
                                    void *pUserData);
#endif /* ANI_MANF_DIAG */

/*---------------------------------------------------------------------------
   WDI_AddSTASelfParamsRspCb 
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Add Sta Self Params
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiAddSelfSTARsp:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_AddSTASelfParamsRspCb)(
                    WDI_AddSTASelfRspParamsType* pwdiAddSelfSTARsp,
                    void*                        pUserData);


/*---------------------------------------------------------------------------
   WDI_DelSTASelfRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a host offload
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_DelSTASelfRspCb)
(
WDI_DelSTASelfRspParamsType*     wdiDelStaSelfRspParams,
void*        pUserData
);

#ifdef FEATURE_WLAN_SCAN_PNO
/*---------------------------------------------------------------------------
   WDI_PNOScanCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Set PNO
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_PNOScanCb)(WDI_Status  wdiStatus,
                                void*       pUserData);

/*---------------------------------------------------------------------------
   WDI_PNOScanCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Set PNO
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_RssiFilterCb)(WDI_Status  wdiStatus,
                                void*       pUserData);

/*---------------------------------------------------------------------------
   WDI_UpdateScanParamsCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Update Scan Params
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_UpdateScanParamsCb)(WDI_Status  wdiStatus,
                                        void*       pUserData);
#endif // FEATURE_WLAN_SCAN_PNO

/*---------------------------------------------------------------------------
   WDI_SetTxPerTrackingRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Tx PER Tracking
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetTxPerTrackingRspCb)(WDI_Status   wdiStatus,
                                           void*        pUserData);
                                     
#ifdef WLAN_FEATURE_PACKET_FILTERING
/*---------------------------------------------------------------------------
   WDI_8023MulticastListCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a 8023 Multicast List
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_8023MulticastListCb)(WDI_Status   wdiStatus,
                                         void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_ReceiveFilterSetFilterCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Receive Filter Set Filter
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_ReceiveFilterSetFilterCb)(WDI_Status   wdiStatus,
                                              void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_FilterMatchCountCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Do PC Filter Match Count
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_FilterMatchCountCb)(WDI_Status   wdiStatus,
                                        void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_ReceiveFilterClearFilterCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Receive Filter Clear Filter
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_ReceiveFilterClearFilterCb)(WDI_Status   wdiStatus,
                                                void*        pUserData);
#endif // WLAN_FEATURE_PACKET_FILTERING

/*---------------------------------------------------------------------------
   WDI_HALDumpCmdRspCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a HAL DUMP Command 
response from
   the HAL layer.
 
   PARAMETERS 

    IN
    wdiHalDumpCmdRsp:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_HALDumpCmdRspCb)(WDI_HALDumpCmdRspParamsType* wdiHalDumpCmdRsp,
                                                                       void*  pUserData);

/*---------------------------------------------------------------------------
   WDI_SetPowerParamsCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Set Power Param
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetPowerParamsCb)(WDI_Status  wdiStatus,
                                      void*       pUserData);

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/*---------------------------------------------------------------------------
   WDI_GtkOffloadCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a GTK offload
   response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_GtkOffloadCb)(WDI_Status   wdiStatus,
                                  void*        pUserData);

/*---------------------------------------------------------------------------
   WDI_GtkOffloadGetInfoCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a GTK offload
   information response from the underlying device.
 
   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_GtkOffloadGetInfoCb)(WDI_Status   wdiStatus,
                                         void*        pUserData);
#endif // WLAN_FEATURE_GTK_OFFLOAD

/*---------------------------------------------------------------------------
   WDI_SetTmLevelCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a Set New TM Level
   done response from the underlying device.

   PARAMETERS 

    IN
    wdiStatus:  response status received from HAL
    pUserData:  user data  

    
  
  RETURN VALUE 
    The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_SetTmLevelCb)(WDI_Status  wdiStatus,
                                  void*       pUserData);

/*---------------------------------------------------------------------------
   WDI_featureCapsExchangeCb
 
   DESCRIPTION   
 
   This callback is invoked by DAL when it has received a HAL Feature Capbility 
   Exchange Response the HAL layer. This callback is put to mantain code
   similarity and is not being used right now.
 
   PARAMETERS 

   IN
   wdiFeatCapRspParams:  response parameters received from HAL
   pUserData:  user data     
  
   RETURN VALUE 
   The result code associated with performing the operation
---------------------------------------------------------------------------*/
typedef void  (*WDI_featureCapsExchangeCb)(void* wdiFeatCapRspParams,
                                                void*        pUserData);

/*========================================================================
 *     Function Declarations and Documentation
 ==========================================================================*/

/*======================================================================== 
 
                             INITIALIZATION APIs
 
==========================================================================*/

/**
 @brief WDI_Init is used to initialize the DAL.
 
 DAL will allocate all the resources it needs. It will open PAL, it will also
 open both the data and the control transport which in their turn will open
 DXE/SMD or any other drivers that they need. 
 
 @param pOSContext: pointer to the OS context provided by the UMAC
                    will be passed on to PAL on Open
        ppWDIGlobalCtx: output pointer of Global Context
        pWdiDevCapability: output pointer of device capability

 @return Result of the function call
*/
WDI_Status 
WDI_Init
( 
  void*                      pOSContext,
  void**                     ppWDIGlobalCtx,
  WDI_DeviceCapabilityType*  pWdiDevCapability,
  unsigned int               driverType
);

/**
 @brief WDI_Start will be called when the upper MAC is ready to
        commence operation with the WLAN Device. Upon the call
        of this API the WLAN DAL will pack and send a HAL Start
        message to the lower RIVA sub-system if the SMD channel
        has been fully opened and the RIVA subsystem is up.

         If the RIVA sub-system is not yet up and running DAL
         will queue the request for Open and will wait for the
         SMD notification before attempting to send down the
         message to HAL. 

 WDI_Init must have been called.

 @param wdiStartParams: the start parameters as specified by 
                      the Device Interface
  
        wdiStartRspCb: callback for passing back the response of
        the start operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_Start
(
  WDI_StartReqParamsType*  pwdiStartParams,
  WDI_StartRspCb           wdiStartRspCb,
  void*                    pUserData
);


/**
 @brief WDI_Stop will be called when the upper MAC is ready to
        stop any operation with the WLAN Device. Upon the call
        of this API the WLAN DAL will pack and send a HAL Stop
        message to the lower RIVA sub-system if the DAL Core is
        in started state.

         In state BUSY this request will be queued.
  
         Request will not be accepted in any other state. 

 WDI_Start must have been called.

 @param wdiStopParams: the stop parameters as specified by 
                      the Device Interface
  
        wdiStopRspCb: callback for passing back the response of
        the stop operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_Stop
(
  WDI_StopReqParamsType*  pwdiStopParams,
  WDI_StopRspCb           wdiStopRspCb,
  void*                   pUserData
);

/**
 @brief WDI_Close will be called when the upper MAC no longer 
        needs to interract with DAL. DAL will free its control
        block.
  
        It is only accepted in state STOPPED.  

 WDI_Stop must have been called.

 @param none
  
 @see WDI_Stop
 @return Result of the function call
*/
WDI_Status 
WDI_Close
(
  void
);


/**
 @brief  WDI_Shutdown will be called during 'SSR shutdown' operation.
         This will do most of the WDI stop & close
         operations without doing any handshake with Riva

         This will also make sure that the control transport
         will NOT be closed.

         This request will not be queued.


 WDI_Start must have been called.

 @param  closeTransport:  Close control channel if this is set

 @return Result of the function call
*/
WDI_Status
WDI_Shutdown
(
 wpt_boolean closeTransport
);

/*======================================================================== 
 
                             SCAN APIs
 
==========================================================================*/

/**
 @brief WDI_InitScanReq will be called when the upper MAC wants 
        the WLAN Device to get ready for a scan procedure. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Init Scan request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiInitScanParams: the init scan parameters as specified
                      by the Device Interface
  
        wdiInitScanRspCb: callback for passing back the response
        of the init scan operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_InitScanReq
(
  WDI_InitScanReqParamsType*  pwdiInitScanParams,
  WDI_InitScanRspCb           wdiInitScanRspCb,
  void*                       pUserData
);

/**
 @brief WDI_StartScanReq will be called when the upper MAC 
        wishes to change the Scan channel on the WLAN Device.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL Start Scan request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_InitScanReq must have been called.

 @param wdiStartScanParams: the start scan parameters as 
                      specified by the Device Interface
  
        wdiStartScanRspCb: callback for passing back the
        response of the start scan operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_InitScanReq
 @return Result of the function call
*/
WDI_Status 
WDI_StartScanReq
(
  WDI_StartScanReqParamsType*  pwdiStartScanParams,
  WDI_StartScanRspCb           wdiStartScanRspCb,
  void*                        pUserData
);


/**
 @brief WDI_EndScanReq will be called when the upper MAC is 
        wants to end scanning for a particular channel that it
        had set before by calling Scan Start on the WLAN Device.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL End Scan request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_StartScanReq must have been called.

 @param wdiEndScanParams: the end scan parameters as specified 
                      by the Device Interface
  
        wdiEndScanRspCb: callback for passing back the response
        of the end scan operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_StartScanReq
 @return Result of the function call
*/
WDI_Status 
WDI_EndScanReq
(
  WDI_EndScanReqParamsType* pwdiEndScanParams,
  WDI_EndScanRspCb          wdiEndScanRspCb,
  void*                     pUserData
);


/**
 @brief WDI_FinishScanReq will be called when the upper MAC has 
        completed the scan process on the WLAN Device. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Finish Scan Request request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_InitScanReq must have been called.

 @param wdiFinishScanParams: the finish scan  parameters as 
                      specified by the Device Interface
  
        wdiFinishScanRspCb: callback for passing back the
        response of the finish scan operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_InitScanReq
 @return Result of the function call
*/
WDI_Status 
WDI_FinishScanReq
(
  WDI_FinishScanReqParamsType* pwdiFinishScanParams,
  WDI_FinishScanRspCb          wdiFinishScanRspCb,
  void*                        pUserData
);

/*======================================================================== 
 
                          ASSOCIATION APIs
 
==========================================================================*/

/**
 @brief WDI_JoinReq will be called when the upper MAC is ready 
        to start an association procedure to a BSS. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Join request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiJoinParams: the join parameters as specified by 
                      the Device Interface
  
        wdiJoinRspCb: callback for passing back the response of
        the join operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_JoinReq
(
  WDI_JoinReqParamsType* pwdiJoinParams,
  WDI_JoinRspCb          wdiJoinRspCb,
  void*                  pUserData
);

/**
 @brief WDI_ConfigBSSReq will be called when the upper MAC 
        wishes to configure the newly acquired or in process of
        being acquired BSS to the HW . Upon the call of this API
        the WLAN DAL will pack and send a HAL Config BSS request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_JoinReq must have been called.

 @param wdiConfigBSSParams: the config BSS parameters as 
                      specified by the Device Interface
  
        wdiConfigBSSRspCb: callback for passing back the
        response of the config BSS operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_JoinReq
 @return Result of the function call
*/
WDI_Status 
WDI_ConfigBSSReq
(
  WDI_ConfigBSSReqParamsType* pwdiConfigBSSParams,
  WDI_ConfigBSSRspCb          wdiConfigBSSRspCb,
  void*                       pUserData
);

/**
 @brief WDI_DelBSSReq will be called when the upper MAC is 
        dissasociating from the BSS and wishes to notify HW.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL Del BSS request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_ConfigBSSReq or WDI_PostAssocReq must have been called.

 @param wdiDelBSSParams: the del BSS parameters as specified by 
                      the Device Interface
  
        wdiDelBSSRspCb: callback for passing back the response
        of the del bss operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_ConfigBSSReq, WDI_PostAssocReq 
 @return Result of the function call
*/
WDI_Status 
WDI_DelBSSReq
(
  WDI_DelBSSReqParamsType* pwdiDelBSSParams,
  WDI_DelBSSRspCb          wdiDelBSSRspCb,
  void*                    pUserData
);

/**
 @brief WDI_PostAssocReq will be called when the upper MAC has 
        associated to a BSS and wishes to configure HW for
        associated state. Upon the call of this API the WLAN DAL
        will pack and send a HAL Post Assoc request message to
        the lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_JoinReq must have been called.

 @param wdiPostAssocReqParams: the assoc parameters as specified
                      by the Device Interface
  
        wdiPostAssocRspCb: callback for passing back the
        response of the post assoc operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_JoinReq
 @return Result of the function call
*/
WDI_Status 
WDI_PostAssocReq
(
  WDI_PostAssocReqParamsType* pwdiPostAssocReqParams,
  WDI_PostAssocRspCb          wdiPostAssocRspCb,
  void*                       pUserData
);

/**
 @brief WDI_DelSTAReq will be called when the upper MAC when an 
        association with another STA has ended and the station
        must be deleted from HW. Upon the call of this API the
        WLAN DAL will pack and send a HAL Del STA request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiDelSTAParams: the Del STA parameters as specified by 
                      the Device Interface
  
        wdiDelSTARspCb: callback for passing back the response
        of the del STA operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_DelSTAReq
(
  WDI_DelSTAReqParamsType* pwdiDelSTAParams,
  WDI_DelSTARspCb          wdiDelSTARspCb,
  void*                    pUserData
);

/*======================================================================== 
 
                             SECURITY APIs
 
==========================================================================*/

/**
 @brief WDI_SetBSSKeyReq will be called when the upper MAC ito 
        install a BSS encryption key on the HW. Upon the call of
        this API the WLAN DAL will pack and send a HAL Start
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiSetBSSKeyParams: the BSS Key set parameters as 
                      specified by the Device Interface
  
        wdiSetBSSKeyRspCb: callback for passing back the
        response of the set BSS Key operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetBSSKeyReq
(
  WDI_SetBSSKeyReqParamsType* pwdiSetBSSKeyParams,
  WDI_SetBSSKeyRspCb          wdiSetBSSKeyRspCb,
  void*                       pUserData
);


/**
 @brief WDI_RemoveBSSKeyReq will be called when the upper MAC to
        uninstall a BSS key from HW. Upon the call of this API
        the WLAN DAL will pack and send a HAL Remove BSS Key
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_SetBSSKeyReq must have been called.

 @param wdiRemoveBSSKeyParams: the remove BSS key parameters as 
                      specified by the Device Interface
  
        wdiRemoveBSSKeyRspCb: callback for passing back the
        response of the remove BSS key operation received from
        the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_SetBSSKeyReq
 @return Result of the function call
*/
WDI_Status 
WDI_RemoveBSSKeyReq
(
  WDI_RemoveBSSKeyReqParamsType* pwdiRemoveBSSKeyParams,
  WDI_RemoveBSSKeyRspCb          wdiRemoveBSSKeyRspCb,
  void*                          pUserData
);


/**
 @brief WDI_SetSTAKeyReq will be called when the upper MAC is 
        ready to install a STA(ast) encryption key in HW. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Set STA Key request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiSetSTAKeyParams: the set STA key parameters as 
                      specified by the Device Interface
  
        wdiSetSTAKeyRspCb: callback for passing back the
        response of the set STA key operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetSTAKeyReq
(
  WDI_SetSTAKeyReqParamsType* pwdiSetSTAKeyParams,
  WDI_SetSTAKeyRspCb          wdiSetSTAKeyRspCb,
  void*                       pUserData
);


/**
 @brief WDI_RemoveSTAKeyReq will be called when the upper MAC 
        wants to unistall a previously set STA key in HW. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Remove STA Key request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_SetSTAKeyReq must have been called.

 @param wdiRemoveSTAKeyParams: the remove STA key parameters as 
                      specified by the Device Interface
  
        wdiRemoveSTAKeyRspCb: callback for passing back the
        response of the remove STA key operation received from
        the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_SetSTAKeyReq
 @return Result of the function call
*/
WDI_Status 
WDI_RemoveSTAKeyReq
(
  WDI_RemoveSTAKeyReqParamsType* pwdiRemoveSTAKeyParams,
  WDI_RemoveSTAKeyRspCb          wdiRemoveSTAKeyRspCb,
  void*                          pUserData
);

/**
 @brief WDI_SetSTABcastKeyReq will be called when the upper MAC 
        wants to install a STA Bcast encryption key on the HW.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL Start request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiSetSTABcastKeyParams: the BSS Key set parameters as 
                      specified by the Device Interface
  
        wdiSetSTABcastKeyRspCb: callback for passing back the
        response of the set BSS Key operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetSTABcastKeyReq
(
  WDI_SetSTAKeyReqParamsType* pwdiSetSTABcastKeyParams,
  WDI_SetSTAKeyRspCb          wdiSetSTABcastKeyRspCb,
  void*                       pUserData
);


/**
 @brief WDI_RemoveSTABcastKeyReq will be called when the upper 
        MAC to uninstall a STA Bcast key from HW. Upon the call
        of this API the WLAN DAL will pack and send a HAL Remove
        STA Bcast Key request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_SetSTABcastKeyReq must have been called.

 @param pwdiRemoveSTABcastKeyParams: the remove BSS key 
                      parameters as specified by the Device
                      Interface
  
        wdiRemoveSTABcastKeyRspCb: callback for passing back the
        response of the remove STA Bcast key operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_SetSTABcastKeyReq
 @return Result of the function call
*/
WDI_Status 
WDI_RemoveSTABcastKeyReq
(
  WDI_RemoveSTAKeyReqParamsType* pwdiRemoveSTABcastKeyParams,
  WDI_RemoveSTAKeyRspCb          wdiRemoveSTABcastKeyRspCb,
  void*                          pUserData
);

/**
 @brief WDI_SetMaxTxPowerReq will be called when the upper 
        MAC wants to set Max Tx Power to HW. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Remove STA Bcast Key request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_SetSTABcastKeyReq must have been called.

 @param pwdiRemoveSTABcastKeyParams: the remove BSS key 
                      parameters as specified by the Device
                      Interface
  
        wdiRemoveSTABcastKeyRspCb: callback for passing back the
        response of the remove STA Bcast key operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_SetMaxTxPowerReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetMaxTxPowerReq
(
  WDI_SetMaxTxPowerParamsType*   pwdiSetMaxTxPowerParams,
  WDA_SetMaxTxPowerRspCb         wdiReqStatusCb,
  void*                          pUserData
);

#ifdef FEATURE_WLAN_CCX
/**
 @brief WDI_TSMStatsReq will be called by the upper MAC to fetch 
        Traffic Stream metrics. 
        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 @param wdiAddTsReqParams: the add TS parameters as specified by
                      the Device Interface
  
        wdiAddTsRspCb: callback for passing back the response of
        the add TS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_TSMStatsReq
(
  WDI_TSMStatsReqParamsType* pwdiTsmStatsReqParams,
  WDI_TsmRspCb          wdiTsmStatsRspCb,
  void*                   pUserData
);


#endif

/*======================================================================== 
 
                            QoS and BA APIs
 
==========================================================================*/

/**
 @brief WDI_AddTSReq will be called when the upper MAC to inform
        the device of a successful add TSpec negotiation. HW
        needs to receive the TSpec Info from the UMAC in order
        to configure properly the QoS data traffic. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Add TS request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiAddTsReqParams: the add TS parameters as specified by
                      the Device Interface
  
        wdiAddTsRspCb: callback for passing back the response of
        the add TS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_AddTSReq
(
  WDI_AddTSReqParamsType* pwdiAddTsReqParams,
  WDI_AddTsRspCb          wdiAddTsRspCb,
  void*                   pUserData
);



/**
 @brief WDI_DelTSReq will be called when the upper MAC has ended
        admission on a specific AC. This is to inform HW that
        QoS traffic parameters must be rest. Upon the call of
        this API the WLAN DAL will pack and send a HAL Del TS
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_AddTSReq must have been called.

 @param wdiDelTsReqParams: the del TS parameters as specified by
                      the Device Interface
  
        wdiDelTsRspCb: callback for passing back the response of
        the del TS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddTSReq
 @return Result of the function call
*/
WDI_Status 
WDI_DelTSReq
(
  WDI_DelTSReqParamsType* pwdiDelTsReqParams,
  WDI_DelTsRspCb          wdiDelTsRspCb,
  void*                   pUserData
);



/**
 @brief WDI_UpdateEDCAParams will be called when the upper MAC 
        wishes to update the EDCA parameters used by HW for QoS
        data traffic. Upon the call of this API the WLAN DAL
        will pack and send a HAL Update EDCA Params request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiUpdateEDCAParams: the start parameters as specified 
                      by the Device Interface
  
        wdiUpdateEDCAParamsRspCb: callback for passing back the
        response of the start operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_UpdateEDCAParams
(
  WDI_UpdateEDCAParamsType*    pwdiUpdateEDCAParams,
  WDI_UpdateEDCAParamsRspCb    wdiUpdateEDCAParamsRspCb,
  void*                        pUserData
);



/**
 @brief WDI_AddBASessionReq will be called when the upper MAC has setup
        successfully a BA session and needs to notify the HW for
        the appropriate settings to take place. Upon the call of
        this API the WLAN DAL will pack and send a HAL Add BA
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiAddBAReqParams: the add BA parameters as specified by
                      the Device Interface
  
        wdiAddBARspCb: callback for passing back the response of
        the add BA operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_AddBASessionReq
(
  WDI_AddBASessionReqParamsType* pwdiAddBASessionReqParams,
  WDI_AddBASessionRspCb          wdiAddBASessionRspCb,
  void*                          pUserData
);


/**
 @brief WDI_DelBAReq will be called when the upper MAC wants to 
        inform HW that it has deleted a previously created BA
        session. Upon the call of this API the WLAN DAL will
        pack and send a HAL Del BA request message to the lower
        RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_AddBAReq must have been called.

 @param wdiDelBAReqParams: the del BA parameters as specified by
                      the Device Interface
  
        wdiDelBARspCb: callback for passing back the response of
        the del BA operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddBAReq
 @return Result of the function call
*/
WDI_Status 
WDI_DelBAReq
(
  WDI_DelBAReqParamsType* pwdiDelBAReqParams,
  WDI_DelBARspCb          wdiDelBARspCb,
  void*                   pUserData
);

/**
 @brief WDI_UpdateBeaconParamsReq will be called when the upper MAC wants to 
        inform HW that there is a change in the beacon parameters
         Upon the call of this API the WLAN DAL will
        pack and send a UpdateBeacon Params message to the lower
        RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_UpdateBeaconParamsReq must have been called.

 @param WDI_UpdateBeaconParamsType: the Update Beacon parameters as specified by
                      the Device Interface
  
        WDI_UpdateBeaconParamsRspCb: callback for passing back the response of
        the Update Beacon Params operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddBAReq
 @return Result of the function call
*/

WDI_Status 
WDI_UpdateBeaconParamsReq
(
  WDI_UpdateBeaconParamsType *   pwdiUpdateBeaconParams,
  WDI_UpdateBeaconParamsRspCb    wdiUpdateBeaconParamsRspCb,
  void*                          pUserData
);


/**
 @brief WDI_SendBeaconParamsReq will be called when the upper MAC wants to 
        update the beacon template to be transmitted as BT MAP STA/IBSS/Soft AP
         Upon the call of this API the WLAN DAL will
        pack and send the beacon Template  message to the lower
        RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_SendBeaconParamsReq must have been called.

 @param WDI_SendBeaconParamsType: the Update Beacon parameters as specified by
                      the Device Interface
  
        WDI_SendBeaconParamsRspCb: callback for passing back the response of
        the Send Beacon Params operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddBAReq
 @return Result of the function call
*/

WDI_Status 
WDI_SendBeaconParamsReq
(
  WDI_SendBeaconParamsType*    pwdiSendBeaconParams,
  WDI_SendBeaconParamsRspCb    wdiSendBeaconParamsRspCb,
  void*                        pUserData
);


/**
 @brief WDI_UpdateProbeRspTemplateReq will be called when the 
        upper MAC wants to update the probe response template to
        be transmitted as Soft AP
         Upon the call of this API the WLAN DAL will
        pack and send the probe rsp template  message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 


 @param pwdiUpdateProbeRspParams: the Update Beacon parameters as 
                      specified by the Device Interface
  
        wdiSendBeaconParamsRspCb: callback for passing back the
        response of the Send Beacon Params operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddBAReq
 @return Result of the function call
*/

WDI_Status 
WDI_UpdateProbeRspTemplateReq
(
  WDI_UpdateProbeRspTemplateParamsType*    pwdiUpdateProbeRspParams,
  WDI_UpdateProbeRspTemplateRspCb          wdiSendBeaconParamsRspCb,
  void*                                  pUserData
);

#ifdef WLAN_FEATURE_P2P
/**
 @brief WDI_SetP2PGONOAReq will be called when the 
        upper MAC wants to send Notice of Absence
         Upon the call of this API the WLAN DAL will
        pack and send the probe rsp template  message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 


 @param pwdiUpdateProbeRspParams: the Update Beacon parameters as 
                      specified by the Device Interface
  
        wdiSendBeaconParamsRspCb: callback for passing back the
        response of the Send Beacon Params operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_AddBAReq
 @return Result of the function call
*/
WDI_Status
WDI_SetP2PGONOAReq
(
  WDI_SetP2PGONOAReqParamsType*    pwdiP2PGONOAReqParams,
  WDI_SetP2PGONOAReqParamsRspCb    wdiP2PGONOAReqParamsRspCb,
  void*                            pUserData
);
#endif


/*======================================================================== 
 
                            Power Save APIs
 
==========================================================================*/

/**
 @brief WDI_SetPwrSaveCfgReq will be called when the upper MAC 
        wants to set the power save related configurations of
        the WLAN Device. Upon the call of this API the WLAN DAL
        will pack and send a HAL Update CFG request message to
        the lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param pwdiPowerSaveCfg: the power save cfg parameters as 
                      specified by the Device Interface
  
        wdiSetPwrSaveCfgCb: callback for passing back the
        response of the set power save cfg operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call  
*/ 
WDI_Status 
WDI_SetPwrSaveCfgReq
(
  WDI_UpdateCfgReqParamsType*   pwdiPowerSaveCfg,
  WDI_SetPwrSaveCfgCb     wdiSetPwrSaveCfgCb,
  void*                   pUserData
);

/**
 @brief WDI_EnterImpsReq will be called when the upper MAC to 
        request the device to get into IMPS power state. Upon
        the call of this API the WLAN DAL will send a HAL Enter
        IMPS request message to the lower RIVA sub-system if DAL
        is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param wdiEnterImpsRspCb: callback for passing back the 
        response of the Enter IMPS operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_EnterImpsReq
(
   WDI_EnterImpsRspCb  wdiEnterImpsRspCb,
   void*                   pUserData
);

/**
 @brief WDI_ExitImpsReq will be called when the upper MAC to 
        request the device to get out of IMPS power state. Upon
        the call of this API the WLAN DAL will send a HAL Exit
        IMPS request message to the lower RIVA sub-system if DAL
        is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 

 @param wdiExitImpsRspCb: callback for passing back the response 
        of the Exit IMPS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_ExitImpsReq
(
   WDI_ExitImpsRspCb  wdiExitImpsRspCb,
   void*                   pUserData
);

/**
 @brief WDI_EnterBmpsReq will be called when the upper MAC to 
        request the device to get into BMPS power state. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Enter BMPS request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiEnterBmpsReqParams: the Enter BMPS parameters as 
                      specified by the Device Interface
  
        wdiEnterBmpsRspCb: callback for passing back the
        response of the Enter BMPS operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_EnterBmpsReq
(
   WDI_EnterBmpsReqParamsType *pwdiEnterBmpsReqParams,
   WDI_EnterBmpsRspCb  wdiEnterBmpsRspCb,
   void*                   pUserData
);

/**
 @brief WDI_ExitBmpsReq will be called when the upper MAC to 
        request the device to get out of BMPS power state. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Exit BMPS request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiExitBmpsReqParams: the Exit BMPS parameters as 
                      specified by the Device Interface
  
        wdiExitBmpsRspCb: callback for passing back the response
        of the Exit BMPS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_ExitBmpsReq
(
   WDI_ExitBmpsReqParamsType *pwdiExitBmpsReqParams,
   WDI_ExitBmpsRspCb  wdiExitBmpsRspCb,
   void*                   pUserData
);

/**
 @brief WDI_EnterUapsdReq will be called when the upper MAC to 
        request the device to get into UAPSD power state. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Enter UAPSD request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.
 WDI_SetUapsdAcParamsReq must have been called.
  
 @param pwdiEnterUapsdReqParams: the Enter UAPSD parameters as 
                      specified by the Device Interface
  
        wdiEnterUapsdRspCb: callback for passing back the
        response of the Enter UAPSD operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq, WDI_SetUapsdAcParamsReq
 @return Result of the function call
*/
WDI_Status 
WDI_EnterUapsdReq
(
   WDI_EnterUapsdReqParamsType *pwdiEnterUapsdReqParams,
   WDI_EnterUapsdRspCb  wdiEnterUapsdRspCb,
   void*                   pUserData
);

/**
 @brief WDI_ExitUapsdReq will be called when the upper MAC to 
        request the device to get out of UAPSD power state. Upon
        the call of this API the WLAN DAL will send a HAL Exit
        UAPSD request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiExitUapsdRspCb: callback for passing back the 
        response of the Exit UAPSD operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_ExitUapsdReq
(
   WDI_ExitUapsdRspCb  wdiExitUapsdRspCb,
   void*                   pUserData
);

/**
 @brief WDI_UpdateUapsdParamsReq will be called when the upper 
        MAC wants to set the UAPSD related configurations
        of an associated STA (while acting as an AP) to the WLAN
        Device. Upon the call of this API the WLAN DAL will pack
        and send a HAL Update UAPSD params request message to
        the lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_ConfigBSSReq must have been called.

 @param pwdiUpdateUapsdReqParams: the UAPSD parameters 
                      as specified by the Device Interface
  
        wdiUpdateUapsdParamsCb: callback for passing back the
        response of the update UAPSD params operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_ConfigBSSReq
 @return Result of the function call
*/
WDI_Status 
WDI_UpdateUapsdParamsReq
(
   WDI_UpdateUapsdReqParamsType *pwdiUpdateUapsdReqParams,
   WDI_UpdateUapsdParamsCb  wdiUpdateUapsdParamsCb,
   void*                   pUserData
);

/**
 @brief WDI_SetUapsdAcParamsReq will be called when the upper 
        MAC wants to set the UAPSD related configurations before
        requesting for enter UAPSD power state to the WLAN
        Device. Upon the call of this API the WLAN DAL will pack
        and send a HAL Set UAPSD params request message to
        the lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiUapsdInfo: the UAPSD parameters as specified by
                      the Device Interface
  
        wdiSetUapsdAcParamsCb: callback for passing back the
        response of the set UAPSD params operation received from
        the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetUapsdAcParamsReq
(
  WDI_SetUapsdAcParamsReqParamsType*      pwdiPowerSaveCfg,
  WDI_SetUapsdAcParamsCb  wdiSetUapsdAcParamsCb,
  void*                   pUserData
);

/**
 @brief WDI_ConfigureRxpFilterReq will be called when the upper 
        MAC wants to set/reset the RXP filters for received pkts
        (MC, BC etc.). Upon the call of this API the WLAN DAL will pack
        and send a HAL configure RXP filter request message to
        the lower RIVA sub-system.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param pwdiConfigureRxpFilterReqParams: the RXP 
                      filter as specified by the Device
                      Interface
  
        wdiConfigureRxpFilterCb: callback for passing back the
        response of the configure RXP filter operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_ConfigureRxpFilterReq
(
   WDI_ConfigureRxpFilterReqParamsType *pwdiConfigureRxpFilterReqParams,
   WDI_ConfigureRxpFilterCb             wdiConfigureRxpFilterCb,
   void*                                pUserData
);

/**
 @brief WDI_SetBeaconFilterReq will be called when the upper MAC
        wants to set the beacon filters while in power save.
        Upon the call of this API the WLAN DAL will pack and
        send a Beacon filter request message to the
        lower RIVA sub-system.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param pwdiBeaconFilterReqParams: the beacon 
                      filter as specified by the Device
                      Interface
  
        wdiBeaconFilterCb: callback for passing back the
        response of the set beacon filter operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_SetBeaconFilterReq
(
   WDI_BeaconFilterReqParamsType   *pwdiBeaconFilterReqParams,
   WDI_SetBeaconFilterCb            wdiBeaconFilterCb,
   void*                            pUserData
);

/**
 @brief WDI_RemBeaconFilterReq will be called when the upper MAC
        wants to remove the beacon filter for perticular IE
        while in power save. Upon the call of this API the WLAN
        DAL will pack and send a remove Beacon filter request
        message to the lower RIVA sub-system.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param pwdiBeaconFilterReqParams: the beacon 
                      filter as specified by the Device
                      Interface
  
        wdiBeaconFilterCb: callback for passing back the
        response of the remove beacon filter operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_RemBeaconFilterReq
(
   WDI_RemBeaconFilterReqParamsType *pwdiBeaconFilterReqParams,
   WDI_RemBeaconFilterCb             wdiBeaconFilterCb,
   void*                             pUserData
);

/**
 @brief WDI_SetRSSIThresholdsReq will be called when the upper 
        MAC wants to set the RSSI thresholds related
        configurations while in power save. Upon the call of
        this API the WLAN DAL will pack and send a HAL Set RSSI
        thresholds request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiUapsdInfo: the UAPSD parameters as specified by
                      the Device Interface
  
        wdiSetUapsdAcParamsCb: callback for passing back the
        response of the set UAPSD params operation received from
        the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetRSSIThresholdsReq
(
  WDI_SetRSSIThresholdsReqParamsType*      pwdiRSSIThresholdsParams,
  WDI_SetRSSIThresholdsCb                  wdiSetRSSIThresholdsCb,
  void*                                    pUserData
);

/**
 @brief WDI_HostOffloadReq will be called when the upper MAC 
        wants to set the filter to minimize unnecessary host
        wakeup due to broadcast traffic while in power save.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL host offload request message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiHostOffloadParams: the host offload as specified 
                      by the Device Interface
  
        wdiHostOffloadCb: callback for passing back the response
        of the host offload operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_HostOffloadReq
(
  WDI_HostOffloadReqParamsType*      pwdiHostOffloadParams,
  WDI_HostOffloadCb                  wdiHostOffloadCb,
  void*                              pUserData
);

/**
 @brief WDI_KeepAliveReq will be called when the upper MAC 
        wants to set the filter to send NULL or unsolicited ARP responses 
        and minimize unnecessary host wakeups due to while in power save.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL Keep Alive request message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiKeepAliveParams: the Keep Alive as specified 
                      by the Device Interface
  
        wdiKeepAliveCb: callback for passing back the response
        of the Keep Alive operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_KeepAliveReq
(
  WDI_KeepAliveReqParamsType*        pwdiKeepAliveParams,
  WDI_KeepAliveCb                    wdiKeepAliveCb,
  void*                              pUserData
);

/**
 @brief WDI_WowlAddBcPtrnReq will be called when the upper MAC 
        wants to set the Wowl Bcast ptrn to minimize unnecessary
        host wakeup due to broadcast traffic while in power
        save. Upon the call of this API the WLAN DAL will pack
        and send a HAL Wowl Bcast ptrn request message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiWowlAddBcPtrnParams: the Wowl bcast ptrn as 
                      specified by the Device Interface
  
        wdiWowlAddBcPtrnCb: callback for passing back the
        response of the add Wowl bcast ptrn operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_WowlAddBcPtrnReq
(
  WDI_WowlAddBcPtrnReqParamsType*    pwdiWowlAddBcPtrnParams,
  WDI_WowlAddBcPtrnCb                wdiWowlAddBcPtrnCb,
  void*                              pUserData
);

/**
 @brief WDI_WowlDelBcPtrnReq will be called when the upper MAC 
        wants to clear the Wowl Bcast ptrn. Upon the call of
        this API the WLAN DAL will pack and send a HAL delete
        Wowl Bcast ptrn request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_WowlAddBcPtrnReq must have been called.

 @param pwdiWowlDelBcPtrnParams: the Wowl bcast ptrn as 
                      specified by the Device Interface
  
        wdiWowlDelBcPtrnCb: callback for passing back the
        response of the del Wowl bcast ptrn operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_WowlAddBcPtrnReq
 @return Result of the function call
*/
WDI_Status 
WDI_WowlDelBcPtrnReq
(
  WDI_WowlDelBcPtrnReqParamsType*    pwdiWowlDelBcPtrnParams,
  WDI_WowlDelBcPtrnCb                wdiWowlDelBcPtrnCb,
  void*                              pUserData
);

/**
 @brief WDI_WowlEnterReq will be called when the upper MAC 
        wants to enter the Wowl state to minimize unnecessary
        host wakeup while in power save. Upon the call of this
        API the WLAN DAL will pack and send a HAL Wowl enter
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiWowlEnterReqParams: the Wowl enter info as 
                      specified by the Device Interface
  
        wdiWowlEnterReqCb: callback for passing back the
        response of the enter Wowl operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_WowlEnterReq
(
  WDI_WowlEnterReqParamsType*    pwdiWowlEnterParams,
  WDI_WowlEnterReqCb             wdiWowlEnterCb,
  void*                          pUserData
);

/**
 @brief WDI_WowlExitReq will be called when the upper MAC 
        wants to exit the Wowl state. Upon the call of this API
        the WLAN DAL will pack and send a HAL Wowl exit request
        message to the lower RIVA sub-system if DAL is in state
        STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_WowlEnterReq must have been called.

 @param pwdiWowlExitReqParams: the Wowl exit info as 
                      specified by the Device Interface
  
        wdiWowlExitReqCb: callback for passing back the response
        of the exit Wowl operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_WowlEnterReq
 @return Result of the function call
*/
WDI_Status 
WDI_WowlExitReq
(
  WDI_WowlExitReqCb              wdiWowlExitCb,
  void*                          pUserData
);

/**
 @brief WDI_ConfigureAppsCpuWakeupStateReq will be called when 
        the upper MAC wants to dynamically adjusts the listen
        interval based on the WLAN/MSM activity. Upon the call
        of this API the WLAN DAL will pack and send a HAL
        configure Apps Cpu Wakeup State request message to the
        lower RIVA sub-system.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param pwdiConfigureAppsCpuWakeupStateReqParams: the 
                      Apps Cpu Wakeup State as specified by the
                      Device Interface
  
        wdiConfigureAppsCpuWakeupStateCb: callback for passing
        back the response of the configure Apps Cpu Wakeup State
        operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_ConfigureAppsCpuWakeupStateReq
(
   WDI_ConfigureAppsCpuWakeupStateReqParamsType *pwdiConfigureAppsCpuWakeupStateReqParams,
   WDI_ConfigureAppsCpuWakeupStateCb             wdiConfigureAppsCpuWakeupStateCb,
   void*                                         pUserData
);
/**
 @brief WDI_FlushAcReq will be called when the upper MAC wants 
        to to perform a flush operation on a given AC. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Flush AC request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param pwdiFlushAcReqParams: the Flush AC parameters as 
                      specified by the Device Interface
  
        wdiFlushAcRspCb: callback for passing back the response
        of the Flush AC operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
 
 @return Result of the function call
*/
WDI_Status 
WDI_FlushAcReq
(
  WDI_FlushAcReqParamsType* pwdiFlushAcReqParams,
  WDI_FlushAcRspCb          wdiFlushAcRspCb,
  void*                     pUserData
);

/**
 @brief WDI_BtAmpEventReq will be called when the upper MAC 
        wants to notify the lower mac on a BT AMP event. This is
        to inform BTC-SLM that some BT AMP event occurred. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL BT AMP event request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param wdiBtAmpEventReqParams: the BT AMP event parameters as 
                      specified by the Device Interface
  
        wdiBtAmpEventRspCb: callback for passing back the
        response of the BT AMP event operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
 
 @return Result of the function call
*/
WDI_Status 
WDI_BtAmpEventReq
(
  WDI_BtAmpEventParamsType* pwdiBtAmpEventReqParams,
  WDI_BtAmpEventRspCb       wdiBtAmpEventRspCb,
  void*                     pUserData
);

#ifdef FEATURE_OEM_DATA_SUPPORT
/**
 @brief WDI_Start oem data Req will be called when the upper MAC 
        wants to notify the lower mac on a oem data Req event.Upon
        the call of this API the WLAN DAL will pack and send a
        HAL OEM Data Req event request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

  
 @param pWdiOemDataReqParams: the oem data req parameters as 
                      specified by the Device Interface
  
        wdiStartOemDataRspCb: callback for passing back the
        response of the Oem Data Req received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
 
 @return Result of the function call
*/
WDI_Status 
WDI_StartOemDataReq
(
  WDI_oemDataReqParamsType*       pWdiOemDataReqParams,
  WDI_oemDataRspCb                wdiOemDataRspCb,
  void*                           pUserData
);
#endif

/*======================================================================== 
 
                             CONTROL APIs
 
==========================================================================*/
/**
 @brief WDI_SwitchChReq will be called when the upper MAC wants 
        the WLAN HW to change the current channel of operation.
        Upon the call of this API the WLAN DAL will pack and
        send a HAL Start request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiSwitchChReqParams: the switch ch parameters as 
                      specified by the Device Interface
  
        wdiSwitchChRspCb: callback for passing back the response
        of the switch ch operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_SwitchChReq
(
  WDI_SwitchChReqParamsType* pwdiSwitchChReqParams,
  WDI_SwitchChRspCb          wdiSwitchChRspCb,
  void*                      pUserData
);



/**
 @brief WDI_ConfigSTAReq will be called when the upper MAC 
        wishes to add or update a STA in HW. Upon the call of
        this API the WLAN DAL will pack and send a HAL Start
        message request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiConfigSTAReqParams: the config STA parameters as 
                      specified by the Device Interface
  
        wdiConfigSTARspCb: callback for passing back the
        response of the config STA operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_ConfigSTAReq
(
  WDI_ConfigSTAReqParamsType* pwdiConfigSTAReqParams,
  WDI_ConfigSTARspCb          wdiConfigSTARspCb,
  void*                       pUserData
);

/**
 @brief WDI_SetLinkStateReq will be called when the upper MAC 
        wants to change the state of an ongoing link. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Start message request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_JoinReq must have been called.

 @param wdiSetLinkStateReqParams: the set link state parameters 
                      as specified by the Device Interface
  
        wdiSetLinkStateRspCb: callback for passing back the
        response of the set link state operation received from
        the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_JoinStartReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetLinkStateReq
(
  WDI_SetLinkReqParamsType* pwdiSetLinkStateReqParams,
  WDI_SetLinkStateRspCb     wdiSetLinkStateRspCb,
  void*                     pUserData
);


/**
 @brief WDI_GetStatsReq will be called when the upper MAC wants 
        to get statistics (MIB counters) from the device. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL Start request message to the lower RIVA sub-system
        if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiGetStatsReqParams: the stats parameters to get as 
                      specified by the Device Interface
  
        wdiGetStatsRspCb: callback for passing back the response
        of the get stats operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_GetStatsReq
(
  WDI_GetStatsReqParamsType* pwdiGetStatsReqParams,
  WDI_GetStatsRspCb          wdiGetStatsRspCb,
  void*                      pUserData
);


/**
 @brief WDI_UpdateCfgReq will be called when the upper MAC when 
        it wishes to change the configuration of the WLAN
        Device. Upon the call of this API the WLAN DAL will pack
        and send a HAL Update CFG request message to the lower
        RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_Start must have been called.

 @param wdiUpdateCfgReqParams: the update cfg parameters as 
                      specified by the Device Interface
  
        wdiUpdateCfgsRspCb: callback for passing back the
        response of the update cfg operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_Start
 @return Result of the function call
*/
WDI_Status 
WDI_UpdateCfgReq
(
  WDI_UpdateCfgReqParamsType* pwdiUpdateCfgReqParams,
  WDI_UpdateCfgRspCb          wdiUpdateCfgsRspCb,
  void*                       pUserData
);

/**
 @brief WDI_NvDownloadReq will be called by the UMAC to dowload the NV blob
 to the NV memory.

    @param wdiNvDownloadReqParams: the NV Download parameters as specified by
    the Device Interface
   
      wdiNvDownloadRspCb: callback for passing back the response of
      the NV Download operation received from the device
 
       pUserData: user data will be passed back with the
       callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_NvDownloadReq
(
  WDI_NvDownloadReqParamsType* pwdiNvDownloadReqParams,
  WDI_NvDownloadRspCb      wdiNvDownloadRspCb,
   void*      pUserData
);
/**
 @brief WDI_AddBAReq will be called when the upper MAC has setup
        successfully a BA session and needs to notify the HW for
        the appropriate settings to take place. Upon the call of
        this API the WLAN DAL will pack and send a HAL Add BA
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiAddBAReqParams: the add BA parameters as specified by
                      the Device Interface
  
        wdiAddBARspCb: callback for passing back the response of
        the add BA operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_AddBAReq
(
  WDI_AddBAReqParamsType* pwdiAddBAReqParams,
  WDI_AddBARspCb          wdiAddBARspCb,
  void*                   pUserData
);

/**
 @brief WDI_TriggerBAReq will be called when the upper MAC has setup
        successfully a BA session and needs to notify the HW for
        the appropriate settings to take place. Upon the call of
        this API the WLAN DAL will pack and send a HAL Add BA
        request message to the lower RIVA sub-system if DAL is
        in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiAddBAReqParams: the add BA parameters as specified by
                      the Device Interface
  
        wdiAddBARspCb: callback for passing back the response of
        the add BA operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_TriggerBAReq
(
  WDI_TriggerBAReqParamsType* pwdiTriggerBAReqParams,
  WDI_TriggerBARspCb          wdiTriggerBARspCb,
  void*                       pUserData
);


/**
 @brief WDI_IsHwFrameTxTranslationCapable checks to see if HW 
        frame xtl is enabled for a particular STA.

 WDI_PostAssocReq must have been called.

 @param uSTAIdx: STA index 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
wpt_boolean WDI_IsHwFrameTxTranslationCapable
(
  wpt_uint8 uSTAIdx
);

#ifdef WLAN_FEATURE_VOWIFI_11R
/**
 @brief WDI_AggrAddTSReq will be called when the upper MAC to inform
        the device of a successful add TSpec negotiation for 11r. HW
        needs to receive the TSpec Info from the UMAC in order
        to configure properly the QoS data traffic. Upon the
        call of this API the WLAN DAL will pack and send a HAL
        Aggregated Add TS request message to the lower RIVA sub-system if
        DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param wdiAggrAddTsReqParams: the add TS parameters as specified by
                               the Device Interface
  
        wdiAggrAddTsRspCb: callback for passing back the response of
        the add TS operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_AggrAddTSReq
(
  WDI_AggrAddTSReqParamsType* pwdiAddTsReqParams,
  WDI_AggrAddTsRspCb          wdiAggrAddTsRspCb,
  void*                   pUserData
);
#endif /* WLAN_FEATURE_VOWIFI_11R */
/**
 @brief WDI_STATableInit - Initializes the STA tables. 
        Allocates the necesary memory.

 
 @param  pWDICtx:         pointer to the WLAN DAL context 
  
 @see
 @return Result of the function call
*/

WDI_Status WDI_StubRunTest
(
   wpt_uint8   ucTestNo
);

#ifdef ANI_MANF_DIAG
/**
 @brief WDI_FTMCommandReq -  
        Route FTMRequest Command to HAL
 
 @param  ftmCommandReq:         FTM request command body 
 @param  ftmCommandRspCb:       Response CB
 @param  pUserData:             User data will be included with CB

 @return Result of the function call
*/
WDI_Status WDI_FTMCommandReq
(
  WDI_FTMCommandReqType *ftmCommandReq,
  WDI_FTMCommandRspCb    ftmCommandRspCb,
  void                  *pUserData
);
#endif /* ANI_MANF_DIAG */

/**
 @brief WDI_HostResumeReq will be called 

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 


 @param pwdiResumeReqParams:  as specified by
                      the Device Interface
  
        wdiResumeReqRspCb: callback for passing back the response of
        the Resume Req received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_HostResumeReq
(
  WDI_ResumeParamsType*            pwdiResumeReqParams,
  WDI_HostResumeEventRspCb         wdiResumeReqRspCb,
  void*                            pUserData
);

/**
 @brief WDI_GetAvailableResCount - Function to get the available resource
        for data and managemnt frames. 
        
 @param  pContext:         pointer to the WDI context 
 @param  wdiResPool:       type of resource pool requesting
 @see
 @return Result of the function call
*/

wpt_uint32 WDI_GetAvailableResCount
(
  void            *pContext,
  WDI_ResPoolType wdiResPool
);

/**
 @brief WDI_SetAddSTASelfReq will be called when the
        UMAC wanted to add self STA while opening any new session
        In state BUSY this request will be queued. Request won't
        be allowed in any other state.


 @param pwdiAddSTASelfParams: the add self sta parameters as
                      specified by the Device Interface

        pUserData: user data will be passed back with the
        callback

 @see
 @return Result of the function call
*/
WDI_Status
WDI_AddSTASelfReq
(
  WDI_AddSTASelfReqParamsType*    pwdiAddSTASelfReqParams,
  WDI_AddSTASelfParamsRspCb       wdiAddSTASelfReqParamsRspCb,
  void*                           pUserData
);


/**
 @brief WDI_DelSTASelfReq will be called .

 @param WDI_DelSTASelfReqParamsType
  
        WDI_DelSTASelfRspCb: callback for passing back the
        response of the del sta self  operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_DelSTASelfReq
(
  WDI_DelSTASelfReqParamsType*    pwdiDelStaSelfParams,
  WDI_DelSTASelfRspCb             wdiDelStaSelfRspCb,
  void*                           pUserData
);

/**
 @brief WDI_HostSuspendInd 
  
        Suspend Indication from the upper layer will be sent
        down to HAL
  
 @param WDI_SuspendParamsType
 
 @see 
  
 @return Status of the request
*/
WDI_Status 
WDI_HostSuspendInd
(
  WDI_SuspendParamsType*    pwdiSuspendIndParams
);

#ifdef FEATURE_WLAN_SCAN_PNO
/**
 @brief WDI_SetPreferredNetworkList

 @param pwdiPNOScanReqParams: the Set PNO as specified 
                      by the Device Interface
  
        wdiPNOScanCb: callback for passing back the response
        of the Set PNO operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetPreferredNetworkReq
(
  WDI_PNOScanReqParamsType* pwdiPNOScanReqParams,
  WDI_PNOScanCb             wdiPNOScanCb,
  void*                      pUserData
);

/**
 @brief WDI_SetRssiFilterReq

 @param pwdiRssiFilterReqParams: the Set RSSI Filter as 
                      specified by the Device Interface
  
        wdiRssiFilterCb: callback for passing back the response
        of the Set RSSI Filter operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_SetRssiFilterReq
(
  WDI_SetRssiFilterReqParamsType* pwdiRssiFilterReqParams,
  WDI_RssiFilterCb                wdiRssiFilterCb,
  void*                           pUserData
);

/**
 @brief WDI_UpdateScanParams

 @param pwdiUpdateScanParamsInfoType: the Update Scan Params as specified 
                      by the Device Interface
  
        wdiUpdateScanParamsCb: callback for passing back the response
        of the Set PNO operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_UpdateScanParamsReq
(
  WDI_UpdateScanParamsInfoType* pwdiUpdateScanParamsInfoType,
  WDI_UpdateScanParamsCb        wdiUpdateScanParamsCb,
  void*                         pUserData
);
#endif // FEATURE_WLAN_SCAN_PNO

/**
 @brief WDI_SetTxPerTrackingReq will be called when the upper MAC 
        wants to set the Tx Per Tracking configurations. 
        Upon the call of this API the WLAN DAL will pack
        and send a HAL Set Tx Per Tracking request message to the
        lower RIVA sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 @param wdiSetTxPerTrackingConf: the Set Tx PER Tracking configurations as 
                      specified by the Device Interface
  
        wdiSetTxPerTrackingCb: callback for passing back the
        response of the set Tx PER Tracking configurations operation received
        from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_SetTxPerTrackingReq
(
  WDI_SetTxPerTrackingReqParamsType*      pwdiSetTxPerTrackingReqParams,
  WDI_SetTxPerTrackingRspCb               pwdiSetTxPerTrackingRspCb,
  void*                                   pUserData
);

/**
 @brief WDI_SetTmLevelReq
        If HW Thermal condition changed, driver should react based on new 
        HW thermal condition.

 @param pwdiSetTmLevelReq: New thermal condition information
  
        pwdiSetTmLevelRspCb: callback
  
        usrData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status
WDI_SetTmLevelReq
(
   WDI_SetTmLevelReqType        *pwdiSetTmLevelReq,
   WDI_SetTmLevelCb              pwdiSetTmLevelRspCb,
   void                         *usrData  
);

#ifdef WLAN_FEATURE_PACKET_FILTERING
/**
 @brief WDI_8023MulticastListReq

 @param pwdiRcvFltPktSetMcListReqInfo: the Set 8023 Multicast 
        List as specified by the Device Interface
  
        wdi8023MulticastListCallback: callback for passing back
        the response of the Set 8023 Multicast List operation
        received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_8023MulticastListReq
(
  WDI_RcvFltPktSetMcListReqParamsType*  pwdiRcvFltPktSetMcListReqInfo,
  WDI_8023MulticastListCb               wdi8023MulticastListCallback,
  void*                                 pUserData
);

/**
 @brief WDI_ReceiveFilterSetFilterReq

 @param pwdiSetRcvPktFilterReqInfo: the Set Receive Filter as 
        specified by the Device Interface
  
        wdiReceiveFilterSetFilterReqCallback: callback for
        passing back the response of the Set Receive Filter
        operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_ReceiveFilterSetFilterReq
(
  WDI_SetRcvPktFilterReqParamsType* pwdiSetRcvPktFilterReqInfo,
  WDI_ReceiveFilterSetFilterCb      wdiReceiveFilterSetFilterReqCallback,
  void*                             pUserData
);

/**
 @brief WDI_PCFilterMatchCountReq

 @param pwdiRcvFltPktMatchCntReqInfo: get D0 PC Filter Match 
                                    Count
  
        wdiPCFilterMatchCountCallback: callback for passing back
        the response of the D0 PC Filter Match Count operation
        received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_FilterMatchCountReq
(
  WDI_RcvFltPktMatchCntReqParamsType* pwdiRcvFltPktMatchCntReqInfo,
  WDI_FilterMatchCountCb              wdiFilterMatchCountCallback,
  void*                               pUserData
);

/**
 @brief WDI_ReceiveFilterClearFilterReq

 @param pwdiRcvFltPktClearReqInfo: the Clear Filter as 
                      specified by the Device Interface
  
        wdiReceiveFilterClearFilterCallback: callback for
        passing back the response of the Clear Filter
        operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_ReceiveFilterClearFilterReq
(
  WDI_RcvFltPktClearReqParamsType*  pwdiRcvFltPktClearReqInfo,
  WDI_ReceiveFilterClearFilterCb    wdiReceiveFilterClearFilterCallback,
  void*                             pUserData
);
#endif // WLAN_FEATURE_PACKET_FILTERING

/**
 @brief WDI_HALDumpCmdReq
        Post HAL DUMP Command Event
 
 @param  halDumpCmdReqParams:   Hal Dump Command Body 
 @param  halDumpCmdRspCb:         callback for passing back the
                                                 response 
 @param  pUserData:       Client Data
  
 @see
 @return Result of the function call
*/
WDI_Status WDI_HALDumpCmdReq(
  WDI_HALDumpCmdReqParamsType *halDumpCmdReqParams,
  WDI_HALDumpCmdRspCb    halDumpCmdRspCb,
  void                  *pUserData
);


/**
 @brief WDI_SetPowerParamsReq

 @param pwdiPowerParamsReqParams: the Set Power Params as 
                      specified by the Device Interface
  
        wdiPowerParamsCb: callback for passing back the response
        of the Set Power Params operation received from the
        device
  
        pUserData: user data will be passed back with the
        callback 
  
 @return Result of the function call
*/
WDI_Status 
WDI_SetPowerParamsReq
(
  WDI_SetPowerParamsReqParamsType* pwdiPowerParamsReqParams,
  WDI_SetPowerParamsCb             wdiPowerParamsCb,
  void*                            pUserData
);

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/**
 @brief WDI_GTKOffloadReq will be called when the upper MAC 
        wants to set GTK Rekey Counter while in power save. Upon
        the call of this API the WLAN DAL will pack and send a
        HAL GTK offload request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiGtkOffloadParams: the GTK offload as specified 
                      by the Device Interface
  
        wdiGtkOffloadCb: callback for passing back the response
        of the GTK offload operation received from the device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_GTKOffloadReq
(
  WDI_GtkOffloadReqMsg*      pwdiGtkOffloadReqMsg,
  WDI_GtkOffloadCb           wdiGtkOffloadCb,
  void*                      pUserData
);

/**
 @brief WDI_GTKOffloadGetInfoReq will be called when the upper 
          MAC wants to get GTK Rekey Counter while in power save.
          Upon the call of this API the WLAN DAL will pack and
          send a HAL GTK offload request message to the lower RIVA
        sub-system if DAL is in state STARTED.

        In state BUSY this request will be queued. Request won't
        be allowed in any other state. 

 WDI_PostAssocReq must have been called.

 @param pwdiGtkOffloadGetInfoReqMsg: the GTK Offload 
                        Information Message as specified by the
                        Device Interface
  
          wdiGtkOffloadGetInfoCb: callback for passing back the
          response of the GTK offload operation received from the
          device
  
        pUserData: user data will be passed back with the
        callback 
  
 @see WDI_PostAssocReq
 @return Result of the function call
*/
WDI_Status 
WDI_GTKOffloadGetInfoReq
(
  WDI_GtkOffloadGetInfoReqMsg*  pwdiGtkOffloadGetInfoReqMsg,
  WDI_GtkOffloadGetInfoCb       wdiGtkOffloadGetInfoCb,
  void*                          pUserData
);
#endif // WLAN_FEATURE_GTK_OFFLOAD

/**
 @brief WDI_featureCapsExchangeReq
        Post feature capability bitmap exchange event.
        Host will send its own capability to FW in this req and 
        expect FW to send its capability back as a bitmap in Response
 
 @param 
  
        wdiFeatCapsExcRspCb: callback called on getting the response.
        It is kept to mantain similarity between WDI reqs and if needed, can
        be used in future. Currently, It is set to NULL
  
        pUserData: user data will be passed back with the
        callback 
  
 @see
 @return Result of the function call
*/
WDI_Status
WDI_featureCapsExchangeReq
(
  WDI_featureCapsExchangeCb     wdiFeatureCapsExchangeCb,
  void*                         pUserData
);

/**
 @brief WDI_getHostWlanFeatCaps
        WDI API that returns whether the feature passed to it as enum value in
        "placeHolderInCapBitmap" is supported by Host or not. It uses WDI global
        variable storing host capability bitmap to find this. This can be used by
        other moduels to decide certain things like call different APIs based on
        whether a particular feature is supported.
 
 @param 
  
        feat_enum_value: enum value for the feature as in placeHolderInCapBitmap in wlan_hal_msg.h.

 @see
 @return 
        0 - if the feature is NOT supported in host
        any non-zero value - if the feature is SUPPORTED in host.
*/
wpt_uint8 WDI_getHostWlanFeatCaps(wpt_uint8 feat_enum_value);

/**
 @brief WDI_getFwWlanFeatCaps
        WDI API that returns whether the feature passed to it as enum value in
        "placeHolderInCapBitmap" is supported by FW or not. It uses WDI global
        variable storing host capability bitmap to find this. This can be used by
        other moduels to decide certain things like call different APIs based on
        whether a particular feature is supported.
 
 @param 
  
        feat_enum_value: enum value for the feature as in placeHolderInCapBitmap 
        in wlan_hal_msg.h.

 @see
 @return 
        0 - if the feature is NOT supported in FW
        any non-zero value - if the feature is SUPPORTED in FW.
*/
wpt_uint8 WDI_getFwWlanFeatCaps(wpt_uint8 feat_enum_value);

/**
 @brief WDI_GetWcnssCompiledApiVersion - Function to get wcnss compiled  
        api version 
        
 @param  WDI_WlanVersionType: Wlan version structure 
 @see
 @return none
*/

void WDI_GetWcnssCompiledApiVersion
(
  WDI_WlanVersionType     *pWcnssApiVersion
);



#ifdef __cplusplus
 }
#endif 


#endif /* #ifndef WLAN_QCT_WDI_H */
