/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef __SYS_GLOBAL_H__
#define __SYS_GLOBAL_H__

typedef struct sAniSirSys
{
    tANI_U32 abort; /* system is aborting and will be unloaded, only MMH thread is running */

    tANI_U32 gSysFrameCount[4][16];
    tANI_U32 gSysBbtReceived;
    tANI_U32 gSysBbtPostedToLim;
    tANI_U32 gSysBbtPostedToSch;
    tANI_U32 gSysBbtPostedToPmm;
    tANI_U32 gSysBbtPostedToHal;
    tANI_U32 gSysBbtDropped;
    tANI_U32 gSysBbtNonLearnFrameInv;
    tANI_U32 gSysBbtLearnFrameInv;
    tANI_U32 gSysBbtCrcFail;
    tANI_U32 gSysBbtDuplicates;
    tANI_U32 gSysReleaseCount;
    tANI_U32 probeError, probeBadSsid, probeIgnore, probeRespond;

    tANI_U32 gSysEnableLearnMode;
    tANI_U32 gSysEnableScanMode;
    tANI_U32 gSysEnableLinkMonitorMode;
} tAniSirSys, *tpAniSirSys;

#endif
