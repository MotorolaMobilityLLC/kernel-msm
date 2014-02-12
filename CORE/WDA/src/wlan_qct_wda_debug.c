/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*===========================================================================

                       wlan_qct_wda_debug.c

  OVERVIEW:

  This software unit holds the implementation of the WLAN Device Adaptation
  Layer for debugging APIs.

  The functions externalized by this module are to be called ONLY by other
  WLAN modules that properly register with the Transport Layer initially.

  DEPENDENCIES:

  Are listed for each API below.


  Copyright (c) 2008 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/


#include "palTypes.h"
#include "wlan_qct_wda_debug.h"

void wdaLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString,...) {
    va_list marker;
    
    if(loglevel > pMac->utils.gLogDbgLevel[WDA_DEBUG_LOGIDX])
        return;
   
    va_start( marker, pString );     /* Initialize variable arguments. */
    
    logDebug(pMac, SIR_WDA_MODULE_ID, loglevel, pString, marker);
    
    va_end( marker );              /* Reset variable arguments.      */
}

