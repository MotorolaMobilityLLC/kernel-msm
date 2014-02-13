/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifdef FEATURE_OEM_DATA_SUPPORT

/** ------------------------------------------------------------------------- * 
    ------------------------------------------------------------------------- *  

  
    \file oemDataInternal.h
  
    Exports and types for the Common OEM DATA REQ/RSP Module interfaces.
  
    Copyright (C) 2010 Qualcomm Inc.
  
   ========================================================================== */


#ifndef __OEM_DATA_INTERNAL_H__
#define __OEM_DATA_INTERNAL_H__

#include "palTimer.h"
#include "csrSupport.h"
#include "vos_nvitem.h"
#include "wlan_qct_tl.h"

#include "oemDataApi.h"

typedef struct tagOemDataStruct
{
    tANI_U32                         nextOemReqId; //a global req id
    tANI_BOOLEAN                     oemDataReqActive; //indicates that currently a request has been posted and 
                                                        //waiting for the response
    oemData_OemDataReqCompleteCallback   callback; //callback function pointer for returning the response
    void*                            pContext; //context of the original caller
    tANI_U32                         oemDataReqID; //original request ID
    tOemDataRsp*                     pOemDataRsp; //response
    tOemDataReqConfig                oemDataReqConfig; //current oem data request
    tANI_U8                          sessionId; //Session on which oem data req is active
} tOemDataStruct;

typedef struct tagOemDataCmd
{
    tANI_U32                            oemDataReqID;
    oemData_OemDataReqCompleteCallback      callback;
    void*                               pContext;
    tOemDataReq                         oemDataReq;
} tOemDataCmd;

#endif //__OEM_DATA_INTERNAL_H__

#endif //FEATURE_OEM_DATA_SUPPORT
