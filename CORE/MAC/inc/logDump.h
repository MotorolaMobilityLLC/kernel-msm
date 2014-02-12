/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*============================================================================
Copyright (c) 2007 Qualcomm Technologies, Inc.
All Rights Reserved.
Qualcomm Technologies Confidential and Proprietary

logDump.h

Provides api's for dump commands.

Author:    Santosh Mandiganal
Date:      04/06/2008
============================================================================*/


#ifndef __LOGDUMP_H__
#define __LOGDUMP_H__

#define MAX_DUMP_CMD            999
#define MAX_DUMP_TABLE_ENTRY    10

typedef char * (*tpFunc)(tpAniSirGlobal, tANI_U32, tANI_U32, tANI_U32, tANI_U32, char *);

typedef struct sDumpFuncEntry  {
    tANI_U32    id;
    char       *description;
    tpFunc      func;
} tDumpFuncEntry;

typedef struct sDumpModuleEntry  {
    tANI_U32    mindumpid;
    tANI_U32    maxdumpid;
    tANI_U32    nItems;
    tDumpFuncEntry     *dumpTable;
} tDumpModuleEntry;

typedef struct sRegList {
    tANI_U32    addr;
    char       *name;
} tLogdRegList;

int log_sprintf(tpAniSirGlobal pMac, char *pBuf, char *fmt, ... );

char *
dump_log_level_set( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p);

char *
dump_cfg_set( tpAniSirGlobal pMac, tANI_U32 arg1,
              tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p);

char *
dump_cfg_get( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2,
              tANI_U32 arg3, tANI_U32 arg4, char *p);

char *
dump_cfg_group_get( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2,
                    tANI_U32 arg3, tANI_U32 arg4, char *p);

void logDumpRegisterTable( tpAniSirGlobal pMac, tDumpFuncEntry *pEntry,
                           tANI_U32   nItems );


void logDumpInit(tpAniSirGlobal pMac);

#endif /* __LOGDUMP_H__ */
