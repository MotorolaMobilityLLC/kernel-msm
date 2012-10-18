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

#if !defined( __SME_API_H )
#define __SME_API_H


/**=========================================================================
  
  \file  smeApi.h
  
  \brief prototype for SME APIs
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "ccmApi.h"
#include "csrApi.h"
#include "pmcApi.h"
#include "vos_mq.h"
#include "vos_lock.h"
#include "halTypes.h"
#include "sirApi.h"
#include "btcApi.h"
#include "vos_nvitem.h"
#include "p2p_Api.h"
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halFw.h"
#endif

#ifdef FEATURE_OEM_DATA_SUPPORT
#include "oemDataApi.h"
#endif

#if defined WLAN_FEATURE_VOWIFI
#include "smeRrmInternal.h"
#endif

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#define SME_SUMMARY_STATS         1
#define SME_GLOBAL_CLASSA_STATS   2
#define SME_GLOBAL_CLASSB_STATS   4
#define SME_GLOBAL_CLASSC_STATS   8
#define SME_GLOBAL_CLASSD_STATS  16
#define SME_PER_STA_STATS        32

#define SME_INVALID_COUNTRY_CODE "XX"

/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
typedef struct _smeConfigParams
{
   tCsrConfigParam  csrConfig;
#if defined WLAN_FEATURE_VOWIFI
   tRrmConfigParam  rrmConfig;
#endif
#if defined FEATURE_WLAN_LFR
    tANI_U8   isFastRoamIniFeatureEnabled;
#endif
#if defined FEATURE_WLAN_CCX
    tANI_U8   isCcxIniFeatureEnabled;
#endif
#if defined WLAN_FEATURE_P2P_INTERNAL
   tP2PConfigParam  p2pConfig;
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
    tANI_U8   isFastTransitionEnabled;
    tANI_U8   RoamRssiDiff;
#endif
} tSmeConfigParams, *tpSmeConfigParams;


/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
  
  \brief sme_Open() - Initialze all SME modules and put them at idle state
  
  The function initializes each module inside SME, PMC, CCM, CSR, etc. . Upon 
  successfully return, all modules are at idle state ready to start.

  smeOpen must be called before any other SME APIs can be involved. 
  smeOpen must be called after macOpen.
  
  \param hHal - The handle returned by macOpen.
  
  \return eHAL_STATUS_SUCCESS - SME is successfully initialized.
  
          Other status means SME is failed to be initialized     
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_Open(tHalHandle hHal);

/*--------------------------------------------------------------------------
  
  \brief sme_Close() - Release all SME modules and their resources.
  
  The function release each module in SME, PMC, CCM, CSR, etc. . Upon 
  return, all modules are at closed state.

  No SME APIs can be involved after sme_Close except sme_Open. 
  sme_Close must be called before macClose.
  
  \param hHal - The handle returned by macOpen.
  
  \return eHAL_STATUS_SUCCESS - SME is successfully close.
  
          Other status means SME is failed to be closed but caller still cannot
          call any other SME functions except smeOpen.
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_Close(tHalHandle hHal);

/*--------------------------------------------------------------------------
  
  \brief sme_Start() - Put all SME modules at ready state.
  
  The function starts each module in SME, PMC, CCM, CSR, etc. . Upon 
  successfully return, all modules are ready to run.

  \param hHal - The handle returned by macOpen.
  
  \return eHAL_STATUS_SUCCESS - SME is ready.
  
          Other status means SME is failed to start.     
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_Start(tHalHandle hHal);

/*--------------------------------------------------------------------------
  
  \brief sme_Stop() - Stop all SME modules and put them at idle state
  
  The function stops each module in SME, PMC, CCM, CSR, etc. . Upon 
  return, all modules are at idle state ready to start.

  
  \param hHal - The handle returned by macOpen.

  \param pmcFlag - The flag tells SME if we want to stop PMC or not
  
  \return eHAL_STATUS_SUCCESS - SME is stopped.
  
          Other status means SME is failed to stop but caller should still consider 
          SME is stopped.
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_Stop(tHalHandle hHal, tANI_BOOLEAN pmcFlag);


/*--------------------------------------------------------------------------
  
  \brief sme_OpenSession() - Open a session for scan/roam operation. 
  
  This is a synchronous API.

  
  \param hHal - The handle returned by macOpen.
  \param callback - A pointer to the function caller specifies for roam/connect status indication
  \param pContext - The context passed with callback
  \param pSelfMacAddr - Caller allocated memory filled with self MAC address (6 bytes)
  \param pbSessionId - pointer to a caller allocated buffer for returned session ID
  
  \return eHAL_STATUS_SUCCESS - session is opened. sessionId returned.
  
          Other status means SME is failed to open the session.  
          eHAL_STATUS_RESOURCES - no more session available.
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_OpenSession(tHalHandle hHal, csrRoamCompleteCallback callback, void *pContext, 
                           tANI_U8 *pSelfMacAddr, tANI_U8 *pbSessionId);


/*--------------------------------------------------------------------------
  
  \brief sme_CloseSession() - Open a session for scan/roam operation. 
  
  This is a synchronous API.

  
  \param hHal - The handle returned by macOpen.

  \param sessionId - A previous opened session's ID.
  
  \return eHAL_STATUS_SUCCESS - session is closed. 
  
          Other status means SME is failed to open the session.  
          eHAL_STATUS_INVALID_PARAMETER - session is not opened. 
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_CloseSession(tHalHandle hHal, tANI_U8 sessionId,
                         csrRoamSessionCloseCallback callback, void *pContext);



/*--------------------------------------------------------------------------
  
  \brief sme_UpdateConfig() - Change configurations for all SME moduels
  
  The function updates some configuration for modules in SME, CCM, CSR, etc
  during SMEs close -> open sequence.
   
  Modules inside SME apply the new configuration at the next transaction.

  
  \param hHal - The handle returned by macOpen.
  \Param pSmeConfigParams - a pointer to a caller allocated object of 
  typedef struct _smeConfigParams.
  
  \return eHAL_STATUS_SUCCESS - SME update the config parameters successfully.
  
          Other status means SME is failed to update the config parameters.
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_UpdateConfig(tHalHandle hHal, tpSmeConfigParams pSmeConfigParams);

#ifdef FEATURE_WLAN_SCAN_PNO
/*--------------------------------------------------------------------------

  \brief sme_UpdateChannelConfig() - Update channel configuration in RIVA.
 
  It is used at driver start up to inform RIVA of the default channel 
  configuration. 

  This is a synchronuous call

  \param hHal - The handle returned by macOpen.
  
  \return eHAL_STATUS_SUCCESS - SME update the channel config successfully.

          Other status means SME is failed to update the channel config.
  \sa

  --------------------------------------------------------------------------*/
eHalStatus sme_UpdateChannelConfig(tHalHandle hHal);

#endif // FEATURE_WLAN_SCAN_PNLO
#ifdef WLAN_SOFTAP_FEATURE
/*--------------------------------------------------------------------------
  
  \brief sme_set11dinfo() - Set the 11d information about valid channels
   and there power using information from nvRAM 
   This function is called only for AP.

  This is a synchronuous call

  \param hHal - The handle returned by macOpen.
  \Param pSmeConfigParams - a pointer to a caller allocated object of
  typedef struct _smeConfigParams.

  \return eHAL_STATUS_SUCCESS - SME update the config parameters successfully.

          Other status means SME is failed to update the config parameters.
  \sa
--------------------------------------------------------------------------*/

eHalStatus sme_set11dinfo(tHalHandle hHal,  tpSmeConfigParams pSmeConfigParams);

/*--------------------------------------------------------------------------

  \brief sme_getSoftApDomain() - Get the current regulatory domain of softAp.

  This is a synchronuous call

  \param hHal - The handle returned by HostapdAdapter.
  \Param v_REGDOMAIN_t - The current Regulatory Domain requested for SoftAp.

  \return eHAL_STATUS_SUCCESS - SME successfully completed the request.

          Other status means, failed to get the current regulatory domain.
  \sa
--------------------------------------------------------------------------*/

eHalStatus sme_getSoftApDomain(tHalHandle hHal,  v_REGDOMAIN_t *domainIdSoftAp);

eHalStatus sme_setRegInfo(tHalHandle hHal,  tANI_U8 *apCntryCode);

#endif

/* ---------------------------------------------------------------------------
    \fn sme_ChangeConfigParams
    \brief The SME API exposed for HDD to provide config params to SME during 
    SMEs stop -> start sequence. 
    
    If HDD changed the domain that will cause a reset. This function will 
    provide the new set of 11d information for the new domain. Currrently this
    API provides info regarding 11d only at reset but we can extend this for
    other params (PMC, QoS) which needs to be initialized again at reset.

    This is a synchronuous call
    
    \param hHal - The handle returned by macOpen.

    \Param
    pUpdateConfigParam - a pointer to a structure (tCsrUpdateConfigParam) that 
                currently provides 11d related information like Country code, 
                Regulatory domain, valid channel list, Tx power per channel, a 
                list with active/passive scan allowed per valid channel. 

    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_ChangeConfigParams(tHalHandle hHal, 
                                 tCsrUpdateConfigParam *pUpdateConfigParam);

/*--------------------------------------------------------------------------
  
  \brief sme_HDDReadyInd() - SME sends eWNI_SME_SYS_READY_IND to PE to inform that the NIC
  is ready tio run.
  
  The function is called by HDD at the end of initialization stage so PE/HAL can enable the NIC 
  to running state. 
  
  
  \param hHal - The handle returned by macOpen.
  
  \return eHAL_STATUS_SUCCESS - eWNI_SME_SYS_READY_IND is sent to PE successfully.
  
          Other status means SME failed to send the message to PE.
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_HDDReadyInd(tHalHandle hHal);


/*--------------------------------------------------------------------------
  
  \brief sme_ProcessMsg() - The main message processor for SME.
  
  The function is called by a message dispatcher when to process a message 
  targeted for SME. 
  
  
  \param hHal - The handle returned by macOpen.
  \param pMsg - A pointer to a caller allocated object of tSirMsgQ.
  
  \return eHAL_STATUS_SUCCESS - SME successfully process the message.
  
          Other status means SME failed to process the message.
  \sa
  
  --------------------------------------------------------------------------*/
eHalStatus sme_ProcessMsg(tHalHandle hHal, vos_msg_t* pMsg);

v_VOID_t sme_FreeMsg( tHalHandle hHal, vos_msg_t* pMsg );

/* ---------------------------------------------------------------------------
    \fn sme_ScanRequest
    \brief a wrapper function to Request a 11d or full scan from CSR.
    \param pScanRequestID - pointer to an object to get back the request ID
    \param callback - a callback function that scan calls upon finish, will not 
                      be called if csrScanRequest returns error
    \param pContext - a pointer passed in for the callback
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanRequest(tHalHandle hHal, tANI_U8 sessionId, tCsrScanRequest *, 
                           tANI_U32 *pScanRequestID, 
                           csrScanCompleteCallback callback, void *pContext);


/* ---------------------------------------------------------------------------
    \fn sme_ScanSetBGScanparams
    \brief a wrapper function to request CSR to set BG scan params in PE
    \param pScanReq - BG scan request structure
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanSetBGScanparams(tHalHandle hHal, tANI_U8 sessionId, tCsrBGScanRequest *pScanReq);


/* ---------------------------------------------------------------------------
    \fn sme_ScanGetResult
    \brief a wrapper function to request scan results from CSR.
    \param pFilter - If pFilter is NULL, all cached results are returned
    \param phResult - an object for the result.
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanGetResult(tHalHandle hHal, tANI_U8 sessionId, tCsrScanResultFilter *pFilter, 
                            tScanResultHandle *phResult);


/* ---------------------------------------------------------------------------
    \fn sme_ScanFlushResult
    \brief a wrapper function to request CSR to clear scan results.
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanFlushResult(tHalHandle hHal, tANI_U8 sessionId);
eHalStatus sme_ScanFlushP2PResult(tHalHandle hHal, tANI_U8 sessionId);

/* ---------------------------------------------------------------------------
    \fn sme_ScanResultGetFirst
    \brief a wrapper function to request CSR to returns the first element of 
           scan result.
    \param hScanResult - returned from csrScanGetResult
    \return tCsrScanResultInfo * - NULL if no result     
  ---------------------------------------------------------------------------*/
tCsrScanResultInfo *sme_ScanResultGetFirst(tHalHandle, 
                                          tScanResultHandle hScanResult);

/* ---------------------------------------------------------------------------
    \fn sme_ScanResultGetNext
    \brief a wrapper function to request CSR to returns the next element of 
           scan result. It can be called without calling csrScanResultGetFirst 
           first
    \param hScanResult - returned from csrScanGetResult
    \return Null if no result or reach the end     
  ---------------------------------------------------------------------------*/
tCsrScanResultInfo *sme_ScanResultGetNext(tHalHandle, 
                                          tScanResultHandle hScanResult);

/* ---------------------------------------------------------------------------
    \fn sme_ScanResultPurge
    \brief a wrapper function to request CSR to remove all items(tCsrScanResult) 
           in the list and free memory for each item
    \param hScanResult - returned from csrScanGetResult. hScanResult is 
                         considered gone by 
    calling this function and even before this function reutrns.
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanResultPurge(tHalHandle hHal, tScanResultHandle hScanResult);

/* ---------------------------------------------------------------------------
    \fn sme_ScanGetPMKIDCandidateList
    \brief a wrapper function to return the PMKID candidate list
    \param pPmkidList - caller allocated buffer point to an array of 
                        tPmkidCandidateInfo
    \param pNumItems - pointer to a variable that has the number of 
                       tPmkidCandidateInfo allocated when retruning, this is 
                       either the number needed or number of items put into 
                       pPmkidList
    \return eHalStatus - when fail, it usually means the buffer allocated is not 
                         big enough and pNumItems
    has the number of tPmkidCandidateInfo.
    \Note: pNumItems is a number of tPmkidCandidateInfo, 
           not sizeof(tPmkidCandidateInfo) * something
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanGetPMKIDCandidateList(tHalHandle hHal, tANI_U8 sessionId,
                                        tPmkidCandidateInfo *pPmkidList, 
                                        tANI_U32 *pNumItems );


/*----------------------------------------------------------------------------
  \fn sme_RoamRegisterLinkQualityIndCallback

  \brief
  a wrapper function to allow HDD to register a callback handler with CSR for 
  link quality indications. 

  Only one callback may be registered at any time.
  In order to deregister the callback, a NULL cback may be provided.

  Registration happens in the task context of the caller.

  \param callback - Call back being registered
  \param pContext - user data
  
  DEPENDENCIES: After CSR open

  \return eHalStatus  
-----------------------------------------------------------------------------*/
eHalStatus sme_RoamRegisterLinkQualityIndCallback(tHalHandle hHal, tANI_U8 sessionId,
                                                  csrRoamLinkQualityIndCallback   callback,  
                                                  void                           *pContext);


/* ---------------------------------------------------------------------------
    \fn sme_RoamConnect
    \brief a wrapper function to request CSR to inititiate an association
    \param sessionId - the sessionId returned by sme_OpenSession.
    \param pProfile - can be NULL to join to any open ones
    \param pRoamId - to get back the request ID
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamConnect(tHalHandle hHal, tANI_U8 sessionId, tCsrRoamProfile *pProfile, 
                           tANI_U32 *pRoamId);

/* ---------------------------------------------------------------------------
    \fn sme_RoamReassoc
    \brief a wrapper function to request CSR to inititiate a re-association
    \param pProfile - can be NULL to join the currently connected AP. In that 
    case modProfileFields should carry the modified field(s) which could trigger
    reassoc  
    \param modProfileFields - fields which are part of tCsrRoamConnectedProfile 
    that might need modification dynamically once STA is up & running and this 
    could trigger a reassoc
    \param pRoamId - to get back the request ID
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamReassoc(tHalHandle hHal, tANI_U8 sessionId, tCsrRoamProfile *pProfile,
                          tCsrRoamModifyProfileFields modProfileFields,
                          tANI_U32 *pRoamId);

/* ---------------------------------------------------------------------------
    \fn sme_RoamConnectToLastProfile
    \brief a wrapper function to request CSR to disconnect and reconnect with 
           the same profile
    \return eHalStatus. It returns fail if currently connected     
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamConnectToLastProfile(tHalHandle hHal, tANI_U8 sessionId);

/* ---------------------------------------------------------------------------
    \fn sme_RoamDisconnect
    \brief a wrapper function to request CSR to disconnect from a network
    \param reason -- To indicate the reason for disconnecting. Currently, only 
                     eCSR_DISCONNECT_REASON_MIC_ERROR is meanful.
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamDisconnect(tHalHandle hHal, tANI_U8 sessionId, eCsrRoamDisconnectReason reason);

#ifdef WLAN_SOFTAP_FEATURE
/* ---------------------------------------------------------------------------
    \fn sme_RoamStopBss
    \brief a wrapper function to request CSR to stop bss
    \param sessionId    - sessionId of SoftAP
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamStopBss(tHalHandle hHal, tANI_U8 sessionId);

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetAssociatedStas
    \brief To probe the list of associated stations from various modules of CORE stack.
    \This is an asynchronous API.
    \param sessionId    - sessionId of SoftAP
    \param modId        - Module from whom list of associtated stations is to be probed.
                          If an invalid module is passed then by default VOS_MODULE_ID_PE will be probed
    \param pUsrContext  - Opaque HDD context
    \param pfnSapEventCallback  - Sap event callback in HDD
    \param pAssocBuf    - Caller allocated memory to be filled with associatd stations info
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamGetAssociatedStas(tHalHandle hHal, tANI_U8 sessionId,
                                        VOS_MODULE_ID modId, void *pUsrContext,
                                        void *pfnSapEventCallback, tANI_U8 *pAssocStasBuf);

/* ---------------------------------------------------------------------------
    \fn sme_RoamDisconnectSta
    \brief To disassociate a station. This is an asynchronous API.
    \param pPeerMacAddr - Caller allocated memory filled with peer MAC address (6 bytes)
    \return eHalStatus  SUCCESS  Roam callback will be called to indicate actual results
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamDisconnectSta(tHalHandle hHal, tANI_U8 sessionId, tANI_U8 *pPeerMacAddr);

/* ---------------------------------------------------------------------------
    \fn sme_RoamDeauthSta
    \brief To disassociate a station. This is an asynchronous API.
    \param hHal - Global structure
    \param sessionId - sessionId of SoftAP
    \param pPeerMacAddr - Caller allocated memory filled with peer MAC address (6 bytes)
    \return eHalStatus  SUCCESS  Roam callback will be called to indicate actual results    
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamDeauthSta(tHalHandle hHal, tANI_U8 sessionId,
                                tANI_U8 *pPeerMacAddr);

/* ---------------------------------------------------------------------------
    \fn sme_RoamTKIPCounterMeasures
    \brief To start or stop TKIP counter measures. This is an asynchronous API.
    \param sessionId - sessionId of SoftAP
    \param bEnable - Flag to start/stop TKIP countermeasures
    \return eHalStatus  SUCCESS  Roam callback will be called to indicate actual results    
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamTKIPCounterMeasures(tHalHandle hHal, tANI_U8 sessionId, tANI_BOOLEAN bEnable);

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetWpsSessionOverlap
    \brief To get the WPS PBC session overlap information.
    \This is an asynchronous API.
    \param sessionId    - sessionId of SoftAP
    \param pUsrContext  - Opaque HDD context
    \param pfnSapEventCallback  - Sap event callback in HDD
    \return eHalStatus
  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamGetWpsSessionOverlap(tHalHandle hHal, tANI_U8 sessionId,
                                        void *pUsrContext, void *pfnSapEventCallback,
                                        v_MACADDR_t pRemoveMac);
#endif

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetConnectState
    \brief a wrapper function to request CSR to return the current connect state 
           of Roaming
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetConnectState(tHalHandle hHal, tANI_U8 sessionId, eCsrConnectState *pState);

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetConnectProfile
    \brief a wrapper function to request CSR to return the current connect 
           profile. Caller must call csrRoamFreeConnectProfile after it is done 
           and before reuse for another csrRoamGetConnectProfile call.
    \param pProfile - pointer to a caller allocated structure 
                      tCsrRoamConnectedProfile
    \return eHalStatus. Failure if not connected     
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetConnectProfile(tHalHandle hHal, tANI_U8 sessionId,
                                     tCsrRoamConnectedProfile *pProfile);

/* ---------------------------------------------------------------------------
    \fn sme_RoamFreeConnectProfile
    \brief a wrapper function to request CSR to free and reinitialize the 
           profile returned previously by csrRoamGetConnectProfile.
    \param pProfile - pointer to a caller allocated structure 
                      tCsrRoamConnectedProfile
    \return eHalStatus.      
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamFreeConnectProfile(tHalHandle hHal, 
                                      tCsrRoamConnectedProfile *pProfile);

/* ---------------------------------------------------------------------------
    \fn sme_RoamSetPMKIDCache
    \brief a wrapper function to request CSR to return the PMKID candidate list
    \param pPMKIDCache - caller allocated buffer point to an array of 
                         tPmkidCacheInfo
    \param numItems - a variable that has the number of tPmkidCacheInfo 
                      allocated when retruning, this is either the number needed 
                      or number of items put into pPMKIDCache
    \return eHalStatus - when fail, it usually means the buffer allocated is not 
                         big enough and pNumItems has the number of 
                         tPmkidCacheInfo.
    \Note: pNumItems is a number of tPmkidCacheInfo, 
           not sizeof(tPmkidCacheInfo) * something
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamSetPMKIDCache( tHalHandle hHal, tANI_U8 sessionId, tPmkidCacheInfo *pPMKIDCache, 
                                  tANI_U32 numItems );

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetSecurityReqIE
    \brief a wrapper function to request CSR to return the WPA or RSN or WAPI IE CSR
           passes to PE to JOIN request or START_BSS request
    This is a synchronuous call.
    \param sessionId - returned by sme_OpenSession.
    \param pLen - caller allocated memory that has the length of pBuf as input. 
                  Upon returned, *pLen has the needed or IE length in pBuf.
    \param pBuf - Caller allocated memory that contain the IE field, if any, 
                  upon return
    \param secType - Specifies whether looking for WPA/WPA2/WAPI IE                  
    \return eHalStatus - when fail, it usually means the buffer allocated is not 
                         big enough
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetSecurityReqIE(tHalHandle hHal, tANI_U8 sessionId, tANI_U32 *pLen,
                                  tANI_U8 *pBuf, eCsrSecurityType secType);

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetSecurityRspIE
    \brief a wrapper function to request CSR to return the WPA or RSN or WAPI IE from 
           the beacon or probe rsp if connected
    \param sessionId - returned by sme_OpenSession.
    \param pLen - caller allocated memory that has the length of pBuf as input. 
                  Upon returned, *pLen has the needed or IE length in pBuf.
    \param pBuf - Caller allocated memory that contain the IE field, if any, 
                  upon return
    \param secType - Specifies whether looking for WPA/WPA2/WAPI IE                                       
    \return eHalStatus - when fail, it usually means the buffer allocated is not 
                         big enough
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetSecurityRspIE(tHalHandle hHal, tANI_U8 sessionId, tANI_U32 *pLen,
                                  tANI_U8 *pBuf, eCsrSecurityType secType);


/* ---------------------------------------------------------------------------
    \fn sme_RoamGetNumPMKIDCache
    \brief a wrapper function to request CSR to return number of PMKID cache 
           entries
    \return tANI_U32 - the number of PMKID cache entries
  ---------------------------------------------------------------------------*/
tANI_U32 sme_RoamGetNumPMKIDCache(tHalHandle hHal, tANI_U8 sessionId);

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetPMKIDCache
    \brief a wrapper function to request CSR to return PMKID cache from CSR
    \param pNum - caller allocated memory that has the space of the number of 
                  pBuf tPmkidCacheInfo as input. Upon returned, *pNum has the 
                  needed or actually number in tPmkidCacheInfo.
    \param pPmkidCache - Caller allocated memory that contains PMKID cache, if 
                         any, upon return
    \return eHalStatus - when fail, it usually means the buffer allocated is not 
                         big enough
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetPMKIDCache(tHalHandle hHal, tANI_U8 sessionId, tANI_U32 *pNum, 
                                 tPmkidCacheInfo *pPmkidCache);

/* ---------------------------------------------------------------------------
    \fn sme_GetConfigParam
    \brief a wrapper function that HDD calls to get the global settings 
           currently maintained by CSR. 
    \param pParam - caller allocated memory
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetConfigParam(tHalHandle hHal, tSmeConfigParams *pParam);

/* ---------------------------------------------------------------------------
    \fn sme_GetStatistics
    \brief a wrapper function that client calls to register a callback to get 
    different PHY level statistics from CSR. 
    
    \param requesterId - different client requesting for statistics, HDD, UMA/GAN etc
    \param statsMask - The different category/categories of stats requester is looking for
    The order in which you set the bits in the statsMask for requesting 
    different type of stats is:

      eCsrSummaryStats = bit 0
      eCsrGlobalClassAStats = bit 1
      eCsrGlobalClassBStats = bit 2
      eCsrGlobalClassCStats = bit 3
      eCsrGlobalClassDStats = bit 4
      eCsrPerStaStats = bit 5

    \param callback - SME sends back the requested stats using the callback
    \param periodicity - If requester needs periodic update, 0 means it's an one 
                         time request
    \param cache - If requester is happy with cached stats
    \param staId - The station ID for which the stats is requested for
    \param pContext - user context to be passed back along with the callback
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetStatistics(tHalHandle hHal, eCsrStatsRequesterType requesterId, 
                             tANI_U32 statsMask, 
                             tCsrStatsCallback callback, 
                             tANI_U32 periodicity, tANI_BOOLEAN cache, 
                             tANI_U8 staId, void *pContext);

eHalStatus sme_GetRssi(tHalHandle hHal, 
                             tCsrRssiCallback callback, 
                             tANI_U8 staId, tCsrBssid bssId, void *pContext, void* pVosContext);

/* ---------------------------------------------------------------------------
    \fn sme_CfgSetInt
    \brief a wrapper function that HDD calls to set parameters in CFG. 
    \param cfgId - Configuration Parameter ID (type) for STA. 
    \param ccmValue - The information related to Configuration Parameter ID
                      which needs to be saved in CFG
    \param callback - To be registered by CSR with CCM. Once the CFG done with 
                      saving the information in the database, it notifies CCM & 
                      then the callback will be invoked to notify. 
    \param toBeSaved - To save the request for future reference
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_CfgSetInt(tHalHandle hHal, tANI_U32 cfgId, tANI_U32 ccmValue, 
                         tCcmCfgSetCallback callback, eAniBoolean toBeSaved) ;

/* ---------------------------------------------------------------------------
    \fn sme_CfgSetStr
    \brief a wrapper function that HDD calls to set parameters in CFG. 
    \param cfgId - Configuration Parameter ID (type) for STA. 
    \param pStr - Pointer to the byte array which carries the information needs 
                  to be saved in CFG
    \param length - Length of the data to be saved                  
    \param callback - To be registered by CSR with CCM. Once the CFG done with 
                      saving the information in the database, it notifies CCM & 
                      then the callback will be invoked to notify. 
    \param toBeSaved - To save the request for future reference
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_CfgSetStr(tHalHandle hHal, tANI_U32 cfgId, tANI_U8 *pStr, 
                         tANI_U32 length, tCcmCfgSetCallback callback, 
                         eAniBoolean toBeSaved) ;


/* ---------------------------------------------------------------------------
    \fn sme_GetModifyProfileFields
    \brief HDD or SME - QOS calls this function to get the current values of 
    connected profile fields, changing which can cause reassoc.
    This function must be called after CFG is downloaded and STA is in connected
    state. Also, make sure to call this function to get the current profile
    fields before calling the reassoc. So that pModifyProfileFields will have
    all the latest values plus the one(s) has been updated as part of reassoc
    request.
    \param pModifyProfileFields - pointer to the connected profile fields 
    changing which can cause reassoc

    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus sme_GetModifyProfileFields(tHalHandle hHal, tANI_U8 sessionId, 
                                     tCsrRoamModifyProfileFields * pModifyProfileFields);


/*--------------------------------------------------------------------------
    \fn sme_SetConfigPowerSave
    \brief  Wrapper fn to change power save configuration in SME (PMC) module.
            For BMPS related configuration, this function also updates the CFG
            and sends a message to FW to pick up the new values. Note: Calling
            this function only updates the configuration and does not enable 
            the specified power save mode.
    \param  hHal - The handle returned by macOpen.
    \param  psMode - Power Saving mode being modified
    \param  pConfigParams - a pointer to a caller allocated object of type
            tPmcSmpsConfigParams or tPmcBmpsConfigParams or tPmcImpsConfigParams 
    \return eHalStatus   
  --------------------------------------------------------------------------*/
eHalStatus sme_SetConfigPowerSave(tHalHandle hHal, tPmcPowerSavingMode psMode,
                                  void *pConfigParams);

/*--------------------------------------------------------------------------
    \fn sme_GetConfigPowerSave
    \brief  Wrapper fn to retireve power save configuration in SME (PMC) module
    \param  hHal - The handle returned by macOpen.
    \param  psMode - Power Saving mode
    \param  pConfigParams - a pointer to a caller allocated object of type
            tPmcSmpsConfigParams or tPmcBmpsConfigParams or tPmcImpsConfigParams 
    \return eHalStatus   
  --------------------------------------------------------------------------*/
eHalStatus sme_GetConfigPowerSave(tHalHandle hHal, tPmcPowerSavingMode psMode,
                                  void *pConfigParams);

/* ---------------------------------------------------------------------------
    \fn sme_SignalPowerEvent
    \brief  Signals to PMC that a power event has occurred. Used for putting
            the chip into deep sleep mode.
    \param  hHal - The handle returned by macOpen.
    \param  event - the event that has occurred
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_SignalPowerEvent (
   tHalHandle hHal,
   tPmcPowerEvent event);

/* ---------------------------------------------------------------------------
    \fn sme_EnablePowerSave
    \brief  Enables one of the power saving modes. This API does not cause a
            device state change. This is purely a configuration API.
    \param  hHal - The handle returned by macOpen.
    \param  psMode - The power saving mode to enable.
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_EnablePowerSave (
   tHalHandle hHal,
   tPmcPowerSavingMode psMode);

/* ---------------------------------------------------------------------------
    \fn sme_DisablePowerSave
    \brief   Disables one of the power saving modes.Disabling does not imply
             that device will be brought out of the current PS mode. This is 
             purely a configuration API.
    \param  hHal - The handle returned by macOpen.
    \param  psMode - The power saving mode to disable. 
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_DisablePowerSave (
   tHalHandle hHal,
   tPmcPowerSavingMode psMode);

/* ---------------------------------------------------------------------------
    \fn sme_StartAutoBmpsTimer
    \brief  Starts a timer that periodically polls all the registered
            module for entry into Bmps mode. This timer is started only if BMPS is
            enabled and whenever the device is in full power.
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_StartAutoBmpsTimer ( tHalHandle hHal);

/* ---------------------------------------------------------------------------
    \fn sme_StopAutoBmpsTimer
    \brief  Stops the Auto BMPS Timer that was started using sme_startAutoBmpsTimer
            Stopping the timer does not cause a device state change. Only the timer
            is stopped. If "Full Power" is desired, use the sme_RequestFullPower API
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_StopAutoBmpsTimer ( tHalHandle hHal);

/* ---------------------------------------------------------------------------
    \fn sme_QueryPowerState
    \brief  Returns the current power state of the device.
    \param  hHal - The handle returned by macOpen.
    \param pPowerState - pointer to location to return power state
    \param pSwWlanSwitchState - ptr to location to return SW WLAN Switch state
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_QueryPowerState (
   tHalHandle hHal,
   tPmcPowerState *pPowerState,
   tPmcSwitchState *pSwWlanSwitchState);

/* ---------------------------------------------------------------------------
    \fn sme_IsPowerSaveEnabled
    \brief  Checks if the device is able to enter a particular power save mode
            This does not imply that the device is in a particular PS mode
    \param  hHal - The handle returned by macOpen.
    \param psMode - the power saving mode
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
extern tANI_BOOLEAN sme_IsPowerSaveEnabled(
   tHalHandle hHal,
   tPmcPowerSavingMode psMode);

/* ---------------------------------------------------------------------------
    \fn sme_RequestFullPower
    \brief  Request that the device be brought to full power state.
            Note 1: If "fullPowerReason" specificied in this API is set to
            eSME_FULL_PWR_NEEDED_BY_HDD, PMC will clear any "buffered wowl" requests
            and also clear any "buffered BMPS requests by HDD". Assumption is that since
            HDD is requesting full power, we need to undo any previous HDD requests for 
            BMPS (using sme_RequestBmps) or WoWL (using sme_EnterWoWL). If the reason is
            specified anything other than above, the buffered requests for BMPS and WoWL
            will not be cleared.
            Note 2: Requesting full power (no matter what the fullPowerReason is) doesn't
            disable the "auto bmps timer" (if it is enabled) or clear any "buffered uapsd
            request".
            Note 3: When the device finally enters Full Power PMC will start a timer 
            if any of the following holds true:
            - Auto BMPS mode is enabled
            - Uapsd request is pending
            - HDD's request for BMPS is pending
            - HDD's request for WoWL is pending
            On timer expiry PMC will attempt to put the device in BMPS mode if following 
            (in addition to those listed above) holds true:
            - Polling of all modules through the Power Save Check routine passes
            - STA is associated to an access point
    \param  hHal - The handle returned by macOpen.
    \param  - callbackRoutine Callback routine invoked in case of success/failure
    \param  - callbackContext -  Cookie to be passed back during callback
    \param  - fullPowerReason - Reason why this API is being invoked. SME needs to
              distinguish between BAP and HDD requests
    \return eHalStatus - status 
     eHAL_STATUS_SUCCESS - device brought to full power state
     eHAL_STATUS_FAILURE - device cannot be brought to full power state
     eHAL_STATUS_PMC_PENDING - device is being brought to full power state,
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_RequestFullPower (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, eHalStatus status),
   void *callbackContext,
   tRequestFullPowerReason fullPowerReason);

/* ---------------------------------------------------------------------------
    \fn sme_RequestBmps
    \brief  Request that the device be put in BMPS state. Request will be 
            accepted only if BMPS mode is enabled and power save check routine
            passes. Only HDD should invoke this API.
    \param  hHal - The handle returned by macOpen.
    \param  - callbackRoutine Callback routine invoked in case of success/failure
    \param  - callbackContext -  Cookie to be passed back during callback
    \return eHalStatus
      eHAL_STATUS_SUCCESS - device is in BMPS state
      eHAL_STATUS_FAILURE - device cannot be brought to BMPS state
      eHAL_STATUS_PMC_PENDING - device is being brought to BMPS state
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_RequestBmps (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, eHalStatus status),
   void *callbackContext);

/* ---------------------------------------------------------------------------
    \fn  sme_SetDHCPTillPowerActiveFlag
    \brief  Sets/Clears DHCP related flag in PMC to disable/enable auto BMPS 
            entry by PMC
    \param  hHal - The handle returned by macOpen.
  ---------------------------------------------------------------------------*/
void  sme_SetDHCPTillPowerActiveFlag(tHalHandle hHal, tANI_U8 flag);


/* ---------------------------------------------------------------------------
    \fn sme_StartUapsd
    \brief  Request that the device be put in UAPSD state. If the device is in
            Full Power it will be put in BMPS mode first and then into UAPSD
            mode.
    \param  hHal - The handle returned by macOpen.
    \param  - callbackRoutine Callback routine invoked in case of success/failure
    \param  - callbackContext -  Cookie to be passed back during callback
      eHAL_STATUS_SUCCESS - device is in UAPSD state
      eHAL_STATUS_FAILURE - device cannot be brought to UAPSD state
      eHAL_STATUS_PMC_PENDING - device is being brought to UAPSD state
      eHAL_STATUS_PMC_DISABLED - UAPSD is disabled or BMPS mode is disabled
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_StartUapsd (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, eHalStatus status),
   void *callbackContext);

/* ---------------------------------------------------------------------------
    \fn sme_StopUapsd
    \brief  Request that the device be put out of UAPSD state. Device will be
            put in in BMPS state after stop UAPSD completes. Buffered requests for
            UAPSD will be cleared after this.
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus  
      eHAL_STATUS_SUCCESS - device is put out of UAPSD and back in BMPS state
      eHAL_STATUS_FAILURE - device cannot be brought out of UAPSD state
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_StopUapsd (tHalHandle hHal);

/* ---------------------------------------------------------------------------
    \fn sme_RequestStandby
    \brief  Request that the device be put in standby. It is HDD's responsibility
            to bring the chip to full power and do a discconnect before calling
            this API. Request for standby will be rejected if STA is associated
            to an AP.
    \param  hHal - The handle returned by macOpen.
    \param  - callbackRoutine Callback routine invoked in case of success/failure
    \param  - callbackContext -  Cookie to be passed back during callback
    \return eHalStatus  
      eHAL_STATUS_SUCCESS - device is in Standby mode
      eHAL_STATUS_FAILURE - device cannot be put in standby mode
      eHAL_STATUS_PMC_PENDING - device is being put in standby mode
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_RequestStandby (
   tHalHandle hHal,
   void (*callbackRoutine) (void *callbackContext, eHalStatus status),
   void *callbackContext);

/* ---------------------------------------------------------------------------
    \fn sme_RegisterPowerSaveCheck
    \brief  Register a power save check routine that is called whenever
            the device is about to enter one of the power save modes.
    \param  hHal - The handle returned by macOpen.
    \param  checkRoutine -  Power save check routine to be registered
    \param  callbackContext -  Cookie to be passed back during callback
    \return eHalStatus
            eHAL_STATUS_SUCCESS - successfully registered
            eHAL_STATUS_FAILURE - not successfully registered  
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_RegisterPowerSaveCheck (
   tHalHandle hHal, 
   tANI_BOOLEAN (*checkRoutine) (void *checkContext), void *checkContext);

/* ---------------------------------------------------------------------------
    \fn sme_DeregisterPowerSaveCheck
    \brief  Deregister a power save check routine
    \param  hHal - The handle returned by macOpen.
    \param  checkRoutine -  Power save check routine to be deregistered
    \return eHalStatus
            eHAL_STATUS_SUCCESS - successfully deregistered
            eHAL_STATUS_FAILURE - not successfully deregistered  
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_DeregisterPowerSaveCheck (
   tHalHandle hHal, 
   tANI_BOOLEAN (*checkRoutine) (void *checkContext));

/* ---------------------------------------------------------------------------
    \fn sme_RegisterDeviceStateUpdateInd
    \brief  Register a callback routine that is called whenever
            the device enters a new device state (Full Power, BMPS, UAPSD)
    \param  hHal - The handle returned by macOpen.
    \param  callbackRoutine -  Callback routine to be registered
    \param  callbackContext -  Cookie to be passed back during callback
    \return eHalStatus
            eHAL_STATUS_SUCCESS - successfully registered
            eHAL_STATUS_FAILURE - not successfully registered  
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_RegisterDeviceStateUpdateInd (
   tHalHandle hHal, 
   void (*callbackRoutine) (void *callbackContext, tPmcState pmcState),
   void *callbackContext);

/* ---------------------------------------------------------------------------
    \fn sme_DeregisterDeviceStateUpdateInd
    \brief  Deregister a routine that was registered for device state changes
    \param  hHal - The handle returned by macOpen.
    \param  callbackRoutine -  Callback routine to be deregistered
    \return eHalStatus
            eHAL_STATUS_SUCCESS - successfully deregistered
            eHAL_STATUS_FAILURE - not successfully deregistered  
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_DeregisterDeviceStateUpdateInd (
   tHalHandle hHal, 
   void (*callbackRoutine) (void *callbackContext, tPmcState pmcState));

/* ---------------------------------------------------------------------------
    \fn sme_WowlAddBcastPattern
    \brief  Add a pattern for Pattern Byte Matching in Wowl mode. Firmware will
            do a pattern match on these patterns when Wowl is enabled during BMPS
            mode.
    \param  hHal - The handle returned by macOpen.
    \param  pattern -  Pattern to be added
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot add pattern
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_WowlAddBcastPattern (
   tHalHandle hHal, 
   tpSirWowlAddBcastPtrn pattern);

/* ---------------------------------------------------------------------------
    \fn sme_WowlDelBcastPattern
    \brief  Delete a pattern that was added for Pattern Byte Matching.
    \param  hHal - The handle returned by macOpen.
    \param  pattern -  Pattern to be deleted
    \return eHalStatus
            eHAL_STATUS_FAILURE  Cannot delete pattern
            eHAL_STATUS_SUCCESS  Request accepted. 
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_WowlDelBcastPattern (
   tHalHandle hHal, 
   tpSirWowlDelBcastPtrn pattern);

/* ---------------------------------------------------------------------------
    \fn sme_EnterWowl
    \brief  This is the API to request entry into WOWL mode. 
            WoWLAN works on top of BMPS mode. If the device is not in BMPS mode, 
            SME will will cache the information that WOWL has been requested and
            attempt to put the device in BMPS first. On entry into BMPS, SME will
            enter the WOWL mode.
            Note 1: After WoWL request is accepted, If module other than HDD requests
            full power BEFORE WoWL request is completed, PMC will buffer the WoWL request
            and attempt to put the chip into BMPS+WOWL based on a timer.
            Note 2: Buffered request for WoWL will be cleared immedisately AFTER "enter Wowl"
            completes or if HDD requests full power or if sme_ExitWoWL API is invoked.
            Note 3: Both UAPSD and WOWL work on top of BMPS. On entry into BMPS, SME
            will give priority to UAPSD and enable only UAPSD if both UAPSD and WOWL
            are required. Currently there is no requirement or use case to support UAPSD
            and WOWL at the same time.
            Note 4. Request for WoWL is rejected if there is a pending UAPSD request.
            Note 5. Request for WoWL is rejected if BMPS is disabled.
            
    \param  hHal - The handle returned by macOpen.
    \param  enterWowlCallbackRoutine -  Callback routine provided by HDD.
                               Used for success/failure notification by SME
    \param  enterWowlCallbackContext - A cookie passed by HDD, that is passed back to HDD
                              at the time of callback.
    \param  wakeReasonIndCB -  Callback routine provided by HDD.
                               Used for Wake Reason Indication by SME
    \param  wakeReasonIndCBContext - A cookie passed by HDD, that is passed back to HDD
                              at the time of callback.
    \return eHalStatus
            eHAL_STATUS_SUCCESS  Device is already in WoWLAN mode
            eHAL_STATUS_FAILURE  Device cannot enter WoWLAN mode.
            eHAL_STATUS_PMC_PENDING  Request accepted. SME will enable WOWL when BMPS
                                      mode is entered.
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_EnterWowl (
    tHalHandle hHal,
    void (*enterWowlCallbackRoutine) (void *callbackContext, eHalStatus status),
    void *enterWowlCallbackContext,
#ifdef WLAN_WAKEUP_EVENTS
    void (*wakeReasonIndCB) (void *callbackContext, tpSirWakeReasonInd pWakeReasonInd),
    void *wakeReasonIndCBContext,
#endif // WLAN_WAKEUP_EVENTS
    tpSirSmeWowlEnterParams wowlEnterParams);

/* ---------------------------------------------------------------------------
    \fn sme_ExitWowl
    \brief  This is the SME API exposed to HDD to request exit from WoWLAN mode. 
            SME will initiate exit from WoWLAN mode and device will be put in BMPS 
            mode. Any Buffered request for WoWL will be cleared after this API.
    \param  hHal - The handle returned by macOpen.
    \return eHalStatus
            eHAL_STATUS_FAILURE  Device cannot exit WoWLAN mode. This can happen
                                  only if the previous "Enter WOWL" transaction has
                                  not even completed.
            eHAL_STATUS_SUCCESS  Request accepted to exit WoWLAN mode. 
  ---------------------------------------------------------------------------*/
extern eHalStatus sme_ExitWowl (tHalHandle hHal);

/* ---------------------------------------------------------------------------

    \fn sme_RoamSetKey

    \brief To set encryption key. This function should be called only when connected
    This is an asynchronous API.

    \param pSetKeyInfo - pointer to a caller allocated object of tCsrSetContextInfo

    \param pRoamId  Upon success return, this is the id caller can use to identify the request in roamcallback

    \return eHalStatus  SUCCESS  Roam callback will be called indicate actually results

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamSetKey(tHalHandle, tANI_U8 sessionId, tCsrRoamSetKey *pSetKey, tANI_U32 *pRoamId);

/* ---------------------------------------------------------------------------

    \fn sme_RoamRemoveKey

    \brief To set encryption key. This is an asynchronous API.

    \param pRemoveKey - pointer to a caller allocated object of tCsrRoamRemoveKey

    \param pRoamId  Upon success return, this is the id caller can use to identify the request in roamcallback

    \return eHalStatus  SUCCESS  Roam callback will be called indicate actually results

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamRemoveKey(tHalHandle, tANI_U8 sessionId, tCsrRoamRemoveKey *pRemoveKey, tANI_U32 *pRoamId);


/* ---------------------------------------------------------------------------

    \fn sme_GetCountryCode

    \brief To return the current country code. If no country code is applied, default country code is 
    used to fill the buffer.
    If 11d supported is turned off, an error is return and the last applied/default country code is used.
    This is a synchronous API.

    \param pBuf - pointer to a caller allocated buffer for returned country code.

    \param pbLen  For input, this parameter indicates how big is the buffer.
                   Upon return, this parameter has the number of bytes for country. If pBuf
                   doesn't have enough space, this function returns
                   fail status and this parameter contains the number that is needed.

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_GetCountryCode(tHalHandle hHal, tANI_U8 *pBuf, tANI_U8 *pbLen);

/* ---------------------------------------------------------------------------

    \fn sme_SetCountryCode

    \brief To change the current/default country code. 
    If 11d supported is turned off, an error is return.
    This is a synchronous API.

    \param pCountry - pointer to a caller allocated buffer for the country code.

    \param pfRestartNeeded  A pointer to caller allocated memory, upon successful return, it indicates
    whether a reset is required.

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_SetCountryCode(tHalHandle hHal, tANI_U8 *pCountry, tANI_BOOLEAN *pfRestartNeeded);

/* ---------------------------------------------------------------------------
    \fn sme_ResetCountryCodeInformation
    \brief this function is to reset the country code current being used back to EEPROM default
    this includes channel list and power setting. This is a synchronous API.
    \param pfRestartNeeded - pointer to a caller allocated space. Upon successful return, it indicates whether 
    a restart is needed to apply the change
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus sme_ResetCountryCodeInformation(tHalHandle hHal, tANI_BOOLEAN *pfRestartNeeded);

/* ---------------------------------------------------------------------------
    \fn sme_GetSupportedCountryCode
    \brief this function is to get a list of the country code current being supported
    \param pBuf - Caller allocated buffer with at least 3 bytes, upon success return, 
    this has the country code list. 3 bytes for each country code. This may be NULL if
    caller wants to know the needed byte count.
    \param pbLen - Caller allocated, as input, it indicates the length of pBuf. Upon success return,
    this contains the length of the data in pBuf. If pbuf is NULL, as input, *pbLen should be 0.
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus sme_GetSupportedCountryCode(tHalHandle hHal, tANI_U8 *pBuf, tANI_U32 *pbLen);

/* ---------------------------------------------------------------------------
    \fn sme_GetCurrentRegulatoryDomain
    \brief this function is to get the current regulatory domain. This is a synchronous API.
    This function must be called after CFG is downloaded and all the band/mode setting already passed into
    SME. The function fails if 11d support is turned off.
    \param pDomain - Caller allocated buffer to return the current domain.
    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.     
  -------------------------------------------------------------------------------*/
eHalStatus sme_GetCurrentRegulatoryDomain(tHalHandle hHal, v_REGDOMAIN_t *pDomain);

/* ---------------------------------------------------------------------------
    \fn sme_SetRegulatoryDomain
    \brief this function is to set the current regulatory domain.
    This function must be called after CFG is downloaded and all the band/mode setting already passed into
    SME. This is a synchronous API.
    \param domainId - indicate the domain (defined in the driver) needs to set to.  
    See v_REGDOMAIN_t for definition
    \param pfRestartNeeded - pointer to a caller allocated space. Upon successful return, it indicates whether 
    a restart is needed to apply the change
    \return eHalStatus     
  -------------------------------------------------------------------------------*/
eHalStatus sme_SetRegulatoryDomain(tHalHandle hHal, v_REGDOMAIN_t domainId, tANI_BOOLEAN *pfRestartNeeded);

/* ---------------------------------------------------------------------------

    \fn sme_GetRegulatoryDomainForCountry

    \brief To return a regulatory domain base on a country code. This is a synchronous API.

    \param pCountry - pointer to a caller allocated buffer for input country code.

    \param pDomainId  Upon successful return, it is the domain that country belongs to.
    If it is NULL, returning success means that the country code is known.

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_GetRegulatoryDomainForCountry(tHalHandle hHal, tANI_U8 *pCountry, v_REGDOMAIN_t *pDomainId);



/* ---------------------------------------------------------------------------

    \fn sme_GetSupportedRegulatoryDomains

    \brief To return a list of supported regulatory domains. This is a synchronous API.

    \param pDomains - pointer to a caller allocated buffer for returned regulatory domains.

    \param pNumDomains  For input, this parameter indicates howm many domains pDomains can hold.
                         Upon return, this parameter has the number for supported domains. If pDomains
                         doesn't have enough space for all the supported domains, this function returns
                         fail status and this parameter contains the number that is needed.

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_GetSupportedRegulatoryDomains(tHalHandle hHal, v_REGDOMAIN_t *pDomains, tANI_U32 *pNumDomains);

//some support functions
tANI_BOOLEAN sme_Is11dSupported(tHalHandle hHal);
tANI_BOOLEAN sme_Is11hSupported(tHalHandle hHal);
tANI_BOOLEAN sme_IsWmmSupported(tHalHandle hHal); 
//Upper layer to get the list of the base channels to scan for passively 11d info from csr
eHalStatus sme_ScanGetBaseChannels( tHalHandle hHal, tCsrChannelInfo * pChannelInfo );

typedef void ( *tSmeChangeCountryCallback)(void *pContext);
/* ---------------------------------------------------------------------------

    \fn sme_ChangeCountryCode

    \brief Change Country code from upperlayer during WLAN driver operation.
           This is a synchronous API.

    \param hHal - The handle returned by macOpen.

    \param pCountry New Country Code String

    \return eHalStatus  SUCCESS.

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_ChangeCountryCode( tHalHandle hHal,
                                  tSmeChangeCountryCallback callback,
                                  tANI_U8 *pCountry,
                                  void *pContext,
                                  void* pVosContext );


/* ---------------------------------------------------------------------------
    \fn sme_BtcSignalBtEvent
    \brief  API to signal Bluetooth (BT) event to the WLAN driver. Based on the
            BT event type and the current operating mode of Libra (full power,
            BMPS, UAPSD etc), appropriate Bluetooth Coexistence (BTC) strategy
            would be employed.
    \param  hHal - The handle returned by macOpen.
    \param  pBtcBtEvent -  Pointer to a caller allocated object of type tSmeBtEvent
                           Caller owns the memory and is responsible for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE  BT Event not passed to HAL. This can happen
                                   if driver has not yet been initialized or if BTC 
                                   Events Layer has been disabled.
            VOS_STATUS_SUCCESS    BT Event passed to HAL 
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_BtcSignalBtEvent (tHalHandle hHal, tpSmeBtEvent pBtcBtEvent);

/* ---------------------------------------------------------------------------
    \fn sme_BtcSetConfig
    \brief  API to change the current Bluetooth Coexistence (BTC) configuration
            This function should be invoked only after CFG download has completed.
            Calling it after sme_HDDReadyInd is recommended.
    \param  hHal - The handle returned by macOpen.
    \param  pSmeBtcConfig - Pointer to a caller allocated object of type
                            tSmeBtcConfig. Caller owns the memory and is responsible
                            for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE  Config not passed to HAL. 
            VOS_STATUS_SUCCESS  Config passed to HAL 
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_BtcSetConfig (tHalHandle hHal, tpSmeBtcConfig pSmeBtcConfig);

/* ---------------------------------------------------------------------------
    \fn sme_BtcGetConfig
    \brief  API to retrieve the current Bluetooth Coexistence (BTC) configuration
    \param  hHal - The handle returned by macOpen.
    \param  pSmeBtcConfig - Pointer to a caller allocated object of type tSmeBtcConfig.
                            Caller owns the memory and is responsible for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_BtcGetConfig (tHalHandle hHal, tpSmeBtcConfig pSmeBtcConfig);

/* ---------------------------------------------------------------------------
    \fn sme_SetCfgPrivacy
    \brief  API to set configure privacy parameters
    \param  hHal - The handle returned by macOpen.
    \param  pProfile - Pointer CSR Roam profile.
    \param  fPrivacy - This parameter indicates status of privacy 
                            
    \return void
  ---------------------------------------------------------------------------*/
void sme_SetCfgPrivacy(tHalHandle hHal, tCsrRoamProfile *pProfile, tANI_BOOLEAN fPrivacy);

#if defined WLAN_FEATURE_VOWIFI
/* ---------------------------------------------------------------------------
    \fn sme_NeighborReportRequest
    \brief  API to request neighbor report.
    \param  hHal - The handle returned by macOpen.
    \param  pRrmNeighborReq - Pointer to a caller allocated object of type
                            tRrmNeighborReq. Caller owns the memory and is responsible
                            for freeing it.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
VOS_STATUS sme_NeighborReportRequest (tHalHandle hHal, tANI_U8 sessionId, 
                tpRrmNeighborReq pRrmNeighborReq, tpRrmNeighborRspCallbackInfo callbackInfo);
#endif

//The following are debug APIs to support direct read/write register/memory
//They are placed in SME because HW cannot be access when in LOW_POWER state
//AND not connected. The knowledge and synchronization is done in SME

//sme_DbgReadRegister
//Caller needs to validate the input values
VOS_STATUS sme_DbgReadRegister(tHalHandle hHal, v_U32_t regAddr, v_U32_t *pRegValue);

//sme_DbgWriteRegister
//Caller needs to validate the input values
VOS_STATUS sme_DbgWriteRegister(tHalHandle hHal, v_U32_t regAddr, v_U32_t regValue);

//sme_DbgReadMemory
//Caller needs to validate the input values
//pBuf caller allocated buffer has the length of nLen
VOS_STATUS sme_DbgReadMemory(tHalHandle hHal, v_U32_t memAddr, v_U8_t *pBuf, v_U32_t nLen);

//sme_DbgWriteMemory
//Caller needs to validate the input values
VOS_STATUS sme_DbgWriteMemory(tHalHandle hHal, v_U32_t memAddr, v_U8_t *pBuf, v_U32_t nLen);

#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
//sme_GetFwVersion
VOS_STATUS sme_GetFwVersion (tHalHandle hHal,FwVersionInfo *pVersion);
#endif
#ifdef FEATURE_WLAN_INTEGRATED_SOC
VOS_STATUS sme_GetWcnssWlanCompiledVersion(tHalHandle hHal,
                                           tSirVersionType *pVersion);
VOS_STATUS sme_GetWcnssWlanReportedVersion(tHalHandle hHal,
                                           tSirVersionType *pVersion);
VOS_STATUS sme_GetWcnssSoftwareVersion(tHalHandle hHal,
                                       tANI_U8 *pVersion,
                                       tANI_U32 versionBufferSize);
VOS_STATUS sme_GetWcnssHardwareVersion(tHalHandle hHal,
                                       tANI_U8 *pVersion,
                                       tANI_U32 versionBufferSize);
#endif
eHalStatus sme_RoamRegisterCallback(tHalHandle hHal,
                                    csrRoamCompleteCallback callback,
                                    void *pContext);

#ifdef FEATURE_WLAN_WAPI
/* ---------------------------------------------------------------------------
    \fn sme_RoamSetBKIDCache
    \brief The SME API exposed to HDD to allow HDD to provde SME the BKID 
    candidate list.
    \param hHal - Handle to the HAL. The HAL handle is returned by the HAL after 
    it is opened (by calling halOpen).
    \param pBKIDCache - caller allocated buffer point to an array of tBkidCacheInfo
    \param numItems - a variable that has the number of tBkidCacheInfo allocated 
    when retruning, this is the number of items put into pBKIDCache
    \return eHalStatus - when fail, it usually means the buffer allocated is not 
    big enough and pNumItems has the number of tBkidCacheInfo.
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamSetBKIDCache( tHalHandle hHal, tANI_U32 sessionId, tBkidCacheInfo *pBKIDCache,
                                 tANI_U32 numItems );

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetBKIDCache
    \brief The SME API exposed to HDD to allow HDD to request SME to return its 
    BKID cache.
    \param hHal - Handle to the HAL. The HAL handle is returned by the HAL after 
    it is opened (by calling halOpen).
    \param pNum - caller allocated memory that has the space of the number of 
    tBkidCacheInfo as input. Upon returned, *pNum has the needed number of entries 
    in SME cache.
    \param pBkidCache - Caller allocated memory that contains BKID cache, if any, 
    upon return
    \return eHalStatus - when fail, it usually means the buffer allocated is not 
    big enough.
  ---------------------------------------------------------------------------*/
eHalStatus sme_RoamGetBKIDCache(tHalHandle hHal, tANI_U32 *pNum,
                                tBkidCacheInfo *pBkidCache);

/* ---------------------------------------------------------------------------
    \fn sme_RoamGetNumBKIDCache
    \brief The SME API exposed to HDD to allow HDD to request SME to return the 
    number of BKID cache entries.
    \param hHal - Handle to the HAL. The HAL handle is returned by the HAL after 
    it is opened (by calling halOpen).
    \return tANI_U32 - the number of BKID cache entries.
  ---------------------------------------------------------------------------*/
tANI_U32 sme_RoamGetNumBKIDCache(tHalHandle hHal, tANI_U32 sessionId);

/* ---------------------------------------------------------------------------
    \fn sme_ScanGetBKIDCandidateList
    \brief a wrapper function to return the BKID candidate list
    \param pBkidList - caller allocated buffer point to an array of 
                        tBkidCandidateInfo
    \param pNumItems - pointer to a variable that has the number of 
                       tBkidCandidateInfo allocated when retruning, this is 
                       either the number needed or number of items put into 
                       pPmkidList
    \return eHalStatus - when fail, it usually means the buffer allocated is not 
                         big enough and pNumItems
    has the number of tBkidCandidateInfo.
    \Note: pNumItems is a number of tBkidCandidateInfo, 
           not sizeof(tBkidCandidateInfo) * something
  ---------------------------------------------------------------------------*/
eHalStatus sme_ScanGetBKIDCandidateList(tHalHandle hHal, tANI_U32 sessionId,
                                        tBkidCandidateInfo *pBkidList, 
                                        tANI_U32 *pNumItems );
#endif /* FEATURE_WLAN_WAPI */

#ifdef FEATURE_OEM_DATA_SUPPORT
/********************************************************************************************
  Oem data related modifications
*********************************************************************************************/
/* ---------------------------------------------------------------------------
    \fn sme_OemDataReq
    \param sessionId - session id of session to be used for oem data req.
    \param pOemDataReqID - pointer to an object to get back the request ID
    \param callback - a callback function that is called upon finish
    \param pContext - a pointer passed in for the callback
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_OemDataReq(tHalHandle hHal, 
                                       tANI_U8 sessionId,
                                       tOemDataReqConfig *, 
                                       tANI_U32 *pOemDataReqID, 
                                       oemData_OemDataReqCompleteCallback callback, 
                                       void *pContext);

/* ---------------------------------------------------------------------------
    \fn sme_getOemDataRsp
    \param pOemDataRsp - A pointer to the response object
    \param pOemDataReqID - pointer to an object to get back the request ID
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_getOemDataRsp(tHalHandle hHal, 
                                         tOemDataRsp **pOemDataRsp);

#endif /*FEATURE_OEM_DATA_SUPPORT*/


#ifdef WLAN_SOFTAP_FEATURE

/* ---------------------------------------------------------------------------

    \fn sme_RoamUpdateAPWPSIE

    \brief To update AP's WPS IE. This function should be called after SME AP session is created
    This is an asynchronous API.

    \param pAPWPSIES - pointer to a caller allocated object of tCsrRoamAPWPSIES

    \return eHalStatus  SUCCESS  Roam callback will be called indicate actually results

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/

eHalStatus sme_RoamUpdateAPWPSIE(tHalHandle, tANI_U8 sessionId, tSirAPWPSIEs *pAPWPSIES);
/* ---------------------------------------------------------------------------

    \fn sme_RoamUpdateAPWPARSNIEs

    \brief To update AP's WPA/RSN IEs. This function should be called after SME AP session is created
    This is an asynchronous API.

    \param pAPSirRSNie - pointer to a caller allocated object of tSirRSNie with WPS/RSN IEs

    \return eHalStatus  SUCCESS  

                         FAILURE or RESOURCES  The API finished and failed.

  -------------------------------------------------------------------------------*/
eHalStatus sme_RoamUpdateAPWPARSNIEs(tHalHandle hHal, tANI_U8 sessionId, tSirRSNie * pAPSirRSNie);

#endif

/* ---------------------------------------------------------------------------
  \fn sme_sendBTAmpEvent
  \brief API to send the btAMPstate to FW
  \param  hHal - The handle returned by macOpen.
  \param  btAmpEvent -- btAMP event
  \return eHalStatus  SUCCESS 

                         FAILURE or RESOURCES  The API finished and failed.

--------------------------------------------------------------------------- */

eHalStatus sme_sendBTAmpEvent(tHalHandle hHal, tSmeBtAmpEvent btAmpEvent);



/* ---------------------------------------------------------------------------
    \fn sme_SetHostOffload
    \brief  API to set the host offload feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the offload request.
    \return eHalStatus
   ---------------------------------------------------------------------------*/
eHalStatus sme_SetHostOffload (tHalHandle hHal, tANI_U8 sessionId,
                                    tpSirHostOffloadReq pRequest);

/* ---------------------------------------------------------------------------
    \fn sme_SetKeepAlive
    \brief  API to set the Keep Alive feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the Keep Alive request.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetKeepAlive (tHalHandle hHal, tANI_U8 sessionId,
                                  tpSirKeepAliveReq pRequest);


/* ---------------------------------------------------------------------------
    \fn sme_AbortMacScan
    \brief  API to cancel MAC scan.
    \param  hHal - The handle returned by macOpen.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
eHalStatus sme_AbortMacScan(tHalHandle hHal);

/* ----------------------------------------------------------------------------
   \fn sme_GetOperationChannel
   \brief API to get current channel on which STA is parked
   this function gives channel information only of infra station or IBSS station.
   \param hHal, pointer to memory location and sessionId 
   \returns eHAL_STATUS_SUCCESS
            eHAL_STATUS_FAILURE
-------------------------------------------------------------------------------*/
eHalStatus sme_GetOperationChannel(tHalHandle hHal, tANI_U32 *pChannel, tANI_U8 sessionId);

#ifdef WLAN_FEATURE_P2P
/* ---------------------------------------------------------------------------

    \fn sme_RegisterMgtFrame

    \brief To register managment frame of specified type and subtype. 
    \param frameType - type of the frame that needs to be passed to HDD.
    \param matchData - data which needs to be matched before passing frame 
                       to HDD. 
    \param matchDataLen - Length of matched data.
    \return eHalStatus 
  -------------------------------------------------------------------------------*/
eHalStatus sme_RegisterMgmtFrame(tHalHandle hHal, tANI_U8 sessionId, 
                     tANI_U16 frameType, tANI_U8* matchData, tANI_U16 matchLen);

/* ---------------------------------------------------------------------------

    \fn sme_DeregisterMgtFrame

    \brief To De-register managment frame of specified type and subtype. 
    \param frameType - type of the frame that needs to be passed to HDD.
    \param matchData - data which needs to be matched before passing frame 
                       to HDD. 
    \param matchDataLen - Length of matched data.
    \return eHalStatus 
  -------------------------------------------------------------------------------*/
eHalStatus sme_DeregisterMgmtFrame(tHalHandle hHal, tANI_U8 sessionId, 
                     tANI_U16 frameType, tANI_U8* matchData, tANI_U16 matchLen);
#endif

/* ---------------------------------------------------------------------------

  \fn    sme_ConfigureRxpFilter

  \brief 
    SME will pass this request to lower mac to set/reset the filter on RXP for
    multicast & broadcast traffic.

  \param 

    hHal - The handle returned by macOpen. 
 
    filterMask- Currently the API takes a 1 or 0 (set or reset) as filter.
    Basically to enable/disable the filter (to filter "all" mcbc traffic) based
    on this param. In future we can use this as a mask to set various types of
    filters as suggested below:
    FILTER_ALL_MULTICAST:
    FILTER_ALL_BROADCAST:
    FILTER_ALL_MULTICAST_BROADCAST:

   
  \return eHalStatus    
  
  
--------------------------------------------------------------------------- */
eHalStatus sme_ConfigureRxpFilter( tHalHandle hHal, 
                              tpSirWlanSetRxpFilters  wlanRxpFilterParam);

/* ---------------------------------------------------------------------------

  \fn    sme_ConfigureAppsCpuWakeupState

  \brief 
    SME will pass this request to lower mac to dynamically adjusts the listen
    interval based on the WLAN/MSM activity. This feature is named as
    Telescopic Beacon wakeup feature.

  \param 

    hHal - The handle returned by macOpen. 
 
    isAppsAwake- Depicts the state of the Apps CPU

   
  \return eHalStatus    
  
  
--------------------------------------------------------------------------- */
eHalStatus sme_ConfigureAppsCpuWakeupState( tHalHandle hHal, tANI_BOOLEAN  isAppsAwake);

#ifdef FEATURE_WLAN_INTEGRATED_SOC
/* ---------------------------------------------------------------------------

  \fn    sme_ConfigureSuspendInd

  \brief 
    SME will pass this request to lower mac to Indicate that the wlan needs to 
    be suspended

  \param 

    hHal - The handle returned by macOpen. 
 
    wlanSuspendParam- Depicts the wlan suspend params

   
  \return eHalStatus    
  
  
--------------------------------------------------------------------------- */
eHalStatus sme_ConfigureSuspendInd( tHalHandle hHal, 
                             tpSirWlanSuspendParam  wlanSuspendParam);

/* ---------------------------------------------------------------------------

  \fn    sme_ConfigureResumeReq

  \brief 
    SME will pass this request to lower mac to Indicate that the wlan needs to 
    be Resumed

  \param 

    hHal - The handle returned by macOpen. 
 
    wlanResumeParam- Depicts the wlan resume params

   
  \return eHalStatus    
  
  
--------------------------------------------------------------------------- */
eHalStatus sme_ConfigureResumeReq( tHalHandle hHal, 
                             tpSirWlanResumeParam  wlanResumeParam);

#endif

/* ---------------------------------------------------------------------------

    \fn sme_GetInfraSessionId

    \brief To get the session ID for infra session, if connected
    This is a synchronous API.

    \param hHal - The handle returned by macOpen.

    \return sessionid, -1 if infra session is not connected

  -------------------------------------------------------------------------------*/
tANI_S8 sme_GetInfraSessionId(tHalHandle hHal);

/* ---------------------------------------------------------------------------

    \fn sme_GetInfraOperationChannel

    \brief To get the operating channel for infra session, if connected
    This is a synchronous API.

    \param hHal - The handle returned by macOpen.
    \param sessionId - the sessionId returned by sme_OpenSession.

    \return operating channel, 0 if infra session is not connected

  -------------------------------------------------------------------------------*/
tANI_U8 sme_GetInfraOperationChannel( tHalHandle hHal, tANI_U8 sessionId);
/* ---------------------------------------------------------------------------

    \fn sme_GetConcurrentOperationChannel

    \brief To get the operating channel for other concurrent sessions, if connected
    This is a synchronous API.

    \param hHal - The handle returned by macOpen.
    \param currentPersona - persona that is trying to come up.

    \return operating channel, 0 if infra session is not connected

  -------------------------------------------------------------------------------*/
tANI_U8 sme_GetConcurrentOperationChannel( tHalHandle hHal );

/* ---------------------------------------------------------------------------
    \fn sme_AbortMacScan
    \brief  API to cancel MAC scan.
    \param  hHal - The handle returned by macOpen.
    \return VOS_STATUS
            VOS_STATUS_E_FAILURE - failure
            VOS_STATUS_SUCCESS  success
  ---------------------------------------------------------------------------*/
eHalStatus sme_AbortMacScan(tHalHandle hHal);

/* ---------------------------------------------------------------------------
    \fn sme_GetCfgValidChannels
    \brief  API to get valid channel list
    \param  hHal - The handle returned by macOpen.
    \param  aValidChannels -  Pointer to the valid channel list
    \param  len -  valid channel list length
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetCfgValidChannels(tHalHandle hHal, tANI_U8 *aValidChannels, tANI_U32 *len);

#ifdef FEATURE_WLAN_SCAN_PNO

/* ---------------------------------------------------------------------------
    \fn sme_SetPreferredNetworkList
    \brief  API to set the Preferred Network List Offload feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the offload request.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetPreferredNetworkList (tHalHandle hHal, tpSirPNOScanReq pRequest, tANI_U8 sessionId, preferredNetworkFoundIndCallback callbackRoutine, void *callbackContext );

/* ---------------------------------------------------------------------------
    \fn sme_SetRSSIFilter
    \brief  API to set RSSI Filter feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the offload request.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetRSSIFilter(tHalHandle hHal, v_U8_t rssiThreshold);

/******************************************************************************
*
* Name: sme_PreferredNetworkFoundInd
*
* Description:
*    Invoke Preferred Network Found Indication 
*
* Parameters:
*    hHal - HAL handle for device
*    pMsg - found network description
* 
* Returns: eHalStatus
*
******************************************************************************/
eHalStatus sme_PreferredNetworkFoundInd (tHalHandle hHal, void* pMsg);
#endif // FEATURE_WLAN_SCAN_PNO

/* ---------------------------------------------------------------------------
    \fn sme_SetPowerParams
    \brief  API to set Power Parameters 
    \param  hHal - The handle returned by macOpen.
    \param  pwParams -  Pointer to the power parameters requested.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetPowerParams(tHalHandle hHal, tSirSetPowerParamsReq* pwParams);

/* ---------------------------------------------------------------------------
    \fn sme_SetTxPerTracking
    \brief  Set Tx PER tracking configuration parameters
    \param  hHal - The handle returned by macOpen.
    \param  pTxPerTrackingParam - Tx PER configuration parameters
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetTxPerTracking (
   tHalHandle hHal,
   void (*pCallbackfn) (void *pCallbackContext),
   void *pCallbackContext,
   tpSirTxPerTrackingParam pTxPerTrackingParam);

#ifdef WLAN_FEATURE_PACKET_FILTERING
/* ---------------------------------------------------------------------------
    \fn sme_ReceiveFilterSetFilter
    \brief  API to set 8023 Multicast Address List
    \param  hHal - The handle returned by macOpen.
    \param  pMulticastAddrs - Pointer to the Multicast Address List
    \return eHalStatus   
  ---------------------------------------------------------------------------*/
eHalStatus sme_8023MulticastList(tHalHandle hHal, tpSirRcvFltMcAddrList pMulticastAddrs);

/* ---------------------------------------------------------------------------
    \fn sme_ReceiveFilterSetFilter
    \brief  API to set Receive Packet Filter
    \param  hHal - The handle returned by macOpen.
    \param  pRcvPktFilterCfg - Receive Packet Filter parameter
    \return eHalStatus   
  ---------------------------------------------------------------------------*/
eHalStatus sme_ReceiveFilterSetFilter(tHalHandle hHal, tpSirRcvPktFilterCfgType pRcvPktFilterCfg,
                                           tANI_U8 sessionId);

/* ---------------------------------------------------------------------------
    \fn sme_GetFilterMatchCount
    \brief  API to get D0 PC Filter Match Count
    \param  hHal - The handle returned by macOpen 
    \param  callbackRoutine - Callback routine invoked to receive Packet Coalescing Filter Match Count
    \param  callbackContext - Cookie to be passed back during callback 
    \return eHalStatus   
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetFilterMatchCount(tHalHandle hHal, 
                                   FilterMatchCountCallback callbackRoutine, 
                                   void *callbackContext );

/* ---------------------------------------------------------------------------
    \fn sme_ReceiveFilterClearFilter
    \brief  API to clear Receive Packet Filter
    \param  hHal - The handle returned by macOpen.
    \param  pRcvFltPktClearParam - Receive Packet Filter Clear parameter
    \return eHalStatus   
  ---------------------------------------------------------------------------*/
eHalStatus sme_ReceiveFilterClearFilter(tHalHandle hHal,
                                        tpSirRcvFltPktClearParam pRcvFltPktClearParam,
                                        tANI_U8  sessionId);
#endif // WLAN_FEATURE_PACKET_FILTERING
/* ---------------------------------------------------------------------------

    \fn sme_IsChannelValid
    \brief To check if the channel is valid for currently established domain
    This is a synchronous API.

    \param hHal - The handle returned by macOpen.
    \param channel - channel to verify

    \return TRUE/FALSE, TRUE if channel is valid

  -------------------------------------------------------------------------------*/
tANI_BOOLEAN sme_IsChannelValid(tHalHandle hHal, tANI_U8 channel);

/* ---------------------------------------------------------------------------
    \fn sme_SetFreqBand
    \brief  Used to set frequency band.
    \param  hHal
    \eBand  band value to be configured
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_SetFreqBand(tHalHandle hHal, eCsrBand eBand);

/* ---------------------------------------------------------------------------
    \fn sme_GetFreqBand
    \brief  Used to get the current band settings.
    \param  hHal
    \pBand  pointer to hold the current band value
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_GetFreqBand(tHalHandle hHal, eCsrBand *pBand);

/* ---------------------------------------------------------------------------

    \fn sme_SetTxPerTracking
    \brief  Set Tx PER tracking configuration parameters
    \param  hHal - The handle returned by macOpen.
    \param  pTxPerTrackingParam - Tx PER configuration parameters
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetTxPerTracking (
   tHalHandle hHal,
   void (*pCallbackfn) (void *pCallbackContext),
   void *pCallbackContext,
   tpSirTxPerTrackingParam pTxPerTrackingParam);

#ifdef WLAN_FEATURE_GTK_OFFLOAD
/* ---------------------------------------------------------------------------
    \fn sme_SetGTKOffload
    \brief  API to set GTK offload feature.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the GTK offload request.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetGTKOffload (tHalHandle hHal, tpSirGtkOffloadParams pRequest);

/* ---------------------------------------------------------------------------
    \fn sme_GetGTKOffload
    \brief  API to get GTK offload information.
    \param  hHal - The handle returned by macOpen.
    \param  pRequest -  Pointer to the GTK offload response.
    \return eHalStatus
  ---------------------------------------------------------------------------*/
eHalStatus sme_GetGTKOffload (tHalHandle hHal, GTKOffloadGetInfoCallback callbackRoutine, void *callbackContext );
#endif // WLAN_FEATURE_GTK_OFFLOAD

#ifdef WLAN_WAKEUP_EVENTS
eHalStatus sme_WakeReasonIndCallback (tHalHandle hHal, void* pMsg);
#endif // WLAN_WAKEUP_EVENTS

/* ---------------------------------------------------------------------------
    \fn sme_SetTxPerTracking
    \brief  Set Tx PER tracking configuration parameters
    \param  hHal - The handle returned by macOpen.
    \param  pTxPerTrackingParam - Tx PER configuration parameters
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetTxPerTracking (
   tHalHandle hHal,
   void (*pCallbackfn) (void *pCallbackContext),
   void *pCallbackContext,
   tpSirTxPerTrackingParam pTxPerTrackingParam);


//return frequency for a particular channel
tANI_U16 sme_ChnToFreq(tANI_U8 chanNum);

tANI_BOOLEAN sme_IsChannelValid(tHalHandle hHal, tANI_U8 channel);

#if defined WLAN_FEATURE_P2P_INTERNAL

eHalStatus sme_p2pResetSession(tHalHandle hHal, tANI_U8 HDDSessionId);

/* ---------------------------------------------------------------------------
    \fn sme_p2pFlushDeviceList
    \brief  Remove cached P2P result from scan results
    \param  hHal - The handle returned by macOpen.
    \param  HDDSessionId - HDD's sessionId. Currently unused.
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_p2pFlushDeviceList(tHalHandle hHal, tANI_U8 HDDSessionId);

eHalStatus sme_p2pGetResultFilter(tHalHandle hHal, tANI_U8 HDDSessionId,
                              tCsrScanResultFilter *pFilter);

#endif //#if defined WLAN_FEATURE_P2P_INTERNAL
   
/* ---------------------------------------------------------------------------
    \fn sme_SetMaxTxPower
    \brief  Used to set the Maximum Transmit Power dynamically. Note: this
    setting will not persist over reboots
    \param  hHal
    \param pBssid  BSSID to set the power cap for
    \param pBssid  pSelfMacAddress self MAC Address
    \param pBssid  power to set in dB
    \- return eHalStatus
    -------------------------------------------------------------------------*/
eHalStatus sme_SetMaxTxPower(tHalHandle hHal, tSirMacAddr pBssid, 
                             tSirMacAddr pSelfMacAddress, v_S7_t dB);

#ifdef WLAN_SOFTAP_FEATURE
/* ---------------------------------------------------------------------------

    \fn sme_HideSSID

    \brief Enable/Disables hidden SSID dynamically. Note: this setting will
    not persist over reboots.

    \param hHal
    \param sessionId 
    \param ssidHidden 0 - Broadcast SSID, 1 - Disable broadcast SSID
    \- return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus sme_HideSSID(tHalHandle hHal, v_U8_t sessionId, v_U8_t ssidHidden);
#endif

/* ---------------------------------------------------------------------------

    \fn sme_SetTmLevel
    \brief  Set Thermal Mitigation Level to RIVA
    \param  hHal - The handle returned by macOpen.
    \param  newTMLevel - new Thermal Mitigation Level
    \param  tmMode - Thermal Mitigation handle mode, default 0
    \return eHalStatus     
  ---------------------------------------------------------------------------*/
eHalStatus sme_SetTmLevel(tHalHandle hHal, v_U16_t newTMLevel, v_U16_t tmMode);

/*---------------------------------------------------------------------------

  \brief sme_featureCapsExchange() - SME interface to exchange capabilities between
  Host and FW.

  \param  hHal - HAL handle for device

  \return NONE

---------------------------------------------------------------------------*/
void sme_featureCapsExchange(tHalHandle hHal);

/*---------------------------------------------------------------------------

  \brief sme_GetDefaultCountryCodeFrmNv() - SME interface to get the default 
         country code
  Host and FW.

  \param  hHal - HAL handle for device
  \param  pCountry - pointer to country code

  \return Sucess or failure

  ---------------------------------------------------------------------------*/
eHalStatus sme_GetDefaultCountryCodeFrmNv(tHalHandle hHal, tANI_U8 *pCountry);

/*---------------------------------------------------------------------------

  \brief sme_GetCurrentCountryCode() - SME interface to get the current operating
          country code.

  \param  hHal - HAL handle for device
  \param  pCountry - pointer to country code

  \return Success or failure

  ---------------------------------------------------------------------------*/
eHalStatus sme_GetCurrentCountryCode(tHalHandle hHal, tANI_U8 *pCountry);

/* ---------------------------------------------------------------------------
    \fn sme_transportDebug
    \brief  Dynamically monitoring Transport channels
            Private IOCTL will querry transport channel status if driver loaded
    \param  displaySnapshot Dispaly transport cahnnel snapshot option
    \param  toggleStallDetect Enable stall detect feature
                              This feature will take effect to data performance
                              Not integrate till fully verification
    \- return NONE
    -------------------------------------------------------------------------*/
void sme_transportDebug
(
   v_BOOL_t  displaySnapshot,
   v_BOOL_t  toggleStallDetect
);
#endif //#if !defined( __SME_API_H )
