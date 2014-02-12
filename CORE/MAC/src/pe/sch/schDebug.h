/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file schDebug.h contains some debug macros.
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __SCH_DEBUG_H__
#define __SCH_DEBUG_H__

#include "utilsApi.h"
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halCommonApi.h"
#endif
#include "sirDebug.h"

#if !defined(__printf)
#define __printf(a,b)
#endif

void __printf(3,4) schLog(tpAniSirGlobal pMac, tANI_U32 loglevel,
                          const char *pString, ...);

#endif
