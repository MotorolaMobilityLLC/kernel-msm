/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * Author:      Kevin Nguyen    
 * Date:        04/09/02
 * History:-
 * 04/09/02        Created.
 * --------------------------------------------------------------------
 */

#ifndef __CFG_DEBUG_H__
#define __CFG_DEBUG_H__

#include "sirDebug.h"
#include "utilsApi.h"
#include "limTrace.h"

#if !defined(__printf)
#define __printf(a,b)
#endif

void __printf(3,4) cfgLog(tpAniSirGlobal pMac, tANI_U32 loglevel,
                          const char *pString, ...);

#endif
