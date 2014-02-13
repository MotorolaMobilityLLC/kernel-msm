/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*============================================================================
ccmLogDump.c

Implements the dump commands specific to the ccm module. 

Copyright (c) 2007 QUALCOMM Incorporated.
All Rights Reserved.
Qualcomm Confidential and Proprietary
 ============================================================================*/


#include "aniGlobal.h"
#include "logDump.h"

#if defined(ANI_LOGDUMP)

static tDumpFuncEntry ccmMenuDumpTable[] = {

   {0,     "CCM (861-870)",                               NULL},
    //{861,   "CCM: CCM testing ",                         dump_ccm}

};

void ccmDumpInit(tHalHandle hHal)
{
   logDumpRegisterTable( (tpAniSirGlobal) hHal, &ccmMenuDumpTable[0], 
                         sizeof(ccmMenuDumpTable)/sizeof(ccmMenuDumpTable[0]) );
}

#endif //#if defined(ANI_LOGDUMP)

