/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file schDebug.cc contains some debug functions.
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */


#include "vos_trace.h"
#include "schDebug.h"
#define LOG_SIZE 256

void schLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString, ...)
{

       VOS_TRACE_LEVEL  vosDebugLevel;
       char    logBuffer[LOG_SIZE];
       va_list marker;

      /* getting proper Debug level*/
       vosDebugLevel = getVosDebugLevel(loglevel);

      /* extracting arguments from pstring */
       va_start( marker, pString );
       vsnprintf(logBuffer, LOG_SIZE, pString, marker);
       VOS_TRACE(VOS_MODULE_ID_PE, vosDebugLevel, "%s", logBuffer);
       va_end( marker );
 }



// --------------------------------------------------------------------
