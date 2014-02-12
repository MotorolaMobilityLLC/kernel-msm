/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * sysWinStartup.cpp: System startup file for Windows platform.
 * Author:         Rajesh Bhagwat
 * Date:           11/01/02
 * History:-
 * 11/01/02        Created.
 * --------------------------------------------------------------------
 *
 */

#include "limApi.h"

#include "utilsApi.h"
#include "sysEntryFunc.h"
#include "sysStartup.h"
#include "cfgApi.h"

// Routine used to retrieve the Winwrapper context pointer from the pMac structure
extern tpAniSirTxWrapper sysGetTxWrapperContext(void *);


tpAniSirTxWrapper
sysGetTxWrapperContext(void *pMac)
{
    return &((tpAniSirGlobal)(pMac))->txWrapper;
}
