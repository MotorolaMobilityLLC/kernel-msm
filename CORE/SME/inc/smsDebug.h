/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/** ------------------------------------------------------------------------- *
    ------------------------------------------------------------------------- *


    \file smsDebug.h

    Define debug log interface for SMS.

   Copyright (C) 2006 Airgo Networks, Incorporated

   ========================================================================== */

#ifndef SMS_DEBUG_H__
#define SMS_DEBUG_H__

#include "utilsApi.h"
#include "sirDebug.h"

#if !defined(__printf)
#define __printf(a,b)
#endif

void __printf(3,4)
smsLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString, ...);

void __printf(3,4)
pmcLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString, ...);

#endif // __SMS_DEBUG_H__
