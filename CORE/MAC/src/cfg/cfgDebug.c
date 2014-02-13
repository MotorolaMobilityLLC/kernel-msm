/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**=========================================================================
  
  \file  cfgDebug.c
  
  \brief implementation for log Debug related APIs

  \author Sunit Bhatia
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "cfgDebug.h"

void cfgLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString,...) 
{
#ifdef WLAN_DEBUG
    // Verify against current log level
    if (loglevel > pMac->utils.gLogDbgLevel[LOG_INDEX_FOR_MODULE( SIR_CFG_MODULE_ID )])
        return;
    else
    {
        va_list marker;

        va_start( marker, pString );     /* Initialize variable arguments. */

        logDebug(pMac, SIR_CFG_MODULE_ID, loglevel, pString, marker);
        
        va_end( marker );              /* Reset variable arguments.      */
    }
#endif
}
