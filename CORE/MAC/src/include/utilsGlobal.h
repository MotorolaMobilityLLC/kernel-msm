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

#ifndef __UTILS_GLOBAL_H__
#define __UTILS_GLOBAL_H__

#include "sirParams.h"

/*
 * Current debug and event log level
 */
#define LOG_FIRST_MODULE_ID    SIR_FIRST_MODULE_ID
#define LOG_LAST_MODULE_ID     SIR_LAST_MODULE_ID
#define LOG_ENTRY_NUM          (LOG_LAST_MODULE_ID - LOG_FIRST_MODULE_ID + 1)

typedef struct sAniSirUtils
{
    tANI_U32  gLogEvtLevel[LOG_ENTRY_NUM];
    tANI_U32  gLogDbgLevel[LOG_ENTRY_NUM];

} tAniSirUtils, *tpAniSirUtils;

#endif
