/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __PMM_DEBUG_H__
#define __PMM_DEBUG_H__

#include "utilsApi.h"
#include "sirDebug.h"

#define UL_HI( field ) ( *( ( (ULONG *)(&(field)) ) + 1 ) )
#define UL_LO( field ) ( *( ( (ULONG *)(&(field)) ) + 0 ) )

#if !defined(__printf)
#define __printf(a,b)
#endif

void __printf(3,4) pmmLog(tpAniSirGlobal pMac, tANI_U32 loglevel,
                          const char *pString, ...) ;

#endif

