/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**=========================================================================
  
  \file  pmmDebug.c
  
  \brief implementation for log Debug related APIs

  \author Sunit Bhatia
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "vos_trace.h"
#include "pmmDebug.h"
#define LOG_SIZE 256

void pmmLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString, ...)
 {
       VOS_TRACE_LEVEL  vosDebugLevel;
       char    logBuffer[LOG_SIZE];
       va_list marker;

       /*  getting proper Debug level  */
       vosDebugLevel = getVosDebugLevel(loglevel);

       /* extracting arguments from pstring */
       va_start( marker, pString );
       vsnprintf(logBuffer, LOG_SIZE, pString, marker);

       VOS_TRACE(VOS_MODULE_ID_PMC, vosDebugLevel, "%s", logBuffer);
       va_end( marker );
 }
