/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file sysGlobal.h contains the logDump utility for system module.
 * Author:      V. K. Kandarpa
 * Date:        01/24/2002
 * History:-
 * Date         Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __SYS_DEBUG_H__
#define __SYS_DEBUG_H__

#include <stdarg.h>
# include "utilsApi.h"
# include "sirDebug.h"
# include "sirParams.h"

void sysLog(tpAniSirGlobal pMac, tANI_U32 loglevel, const char *pString,...);

#endif // __SYS_DEBUG_H__
