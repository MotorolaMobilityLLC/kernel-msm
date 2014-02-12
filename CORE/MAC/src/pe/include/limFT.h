/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if defined WLAN_FEATURE_VOWIFI_11R
/**=========================================================================
  
   Macros and Function prototypes FT and 802.11R purposes 

   Copyright 2010 (c) Qualcomm Technologies, Inc.  All Rights Reserved.
   Qualcomm Technologies Confidential and Proprietary.
  
  ========================================================================*/

#ifndef __LIMFT_H__
#define __LIMFT_H__


#include <palTypes.h>
#include <limGlobal.h>
#include <aniGlobal.h>
#include <limDebug.h>
#include <limSerDesUtils.h>


/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/
extern void limFTOpen(tpAniSirGlobal pMac);
extern void limFTCleanup(tpAniSirGlobal pMac);
extern void limFTInit(tpAniSirGlobal pMac);
extern int  limProcessFTPreAuthReq(tpAniSirGlobal pMac, tpSirMsgQ pMsg);
extern void limPerformFTPreAuth(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data, 
                tpPESession psessionEntry);
void        limPerformPostFTPreAuth(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data, 
                tpPESession psessionEntry);
void        limFTResumeLinkCb(tpAniSirGlobal pMac, eHalStatus status, tANI_U32 *data);
void        limPostFTPreAuthRsp(tpAniSirGlobal pMac, tSirRetStatus status,
                tANI_U8 *auth_rsp, tANI_U16  auth_rsp_length,
                tpPESession psessionEntry);
void        limHandleFTPreAuthRsp(tpAniSirGlobal pMac, tSirRetStatus status,
                tANI_U8 *auth_rsp, tANI_U16  auth_rsp_len,
                tpPESession psessionEntry);
void        limProcessMlmFTReassocReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf,
                tpPESession psessionEntry);
void        limProcessFTPreauthRspTimeout(tpAniSirGlobal pMac);

tANI_BOOLEAN   limProcessFTUpdateKey(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf );
tSirRetStatus  limProcessFTAggrQosReq(tpAniSirGlobal pMac, tANI_U32 *pMsgBuf );
void        limProcessFTAggrQoSRsp(tpAniSirGlobal pMac, tpSirMsgQ limMsg);

#endif /* __LIMFT_H__ */ 

#endif /* WLAN_FEATURE_VOWIFI_11R */
