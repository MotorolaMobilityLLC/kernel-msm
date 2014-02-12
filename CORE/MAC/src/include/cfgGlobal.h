/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * Author:      Sandesh Goel
 * Date:        02/09/03
 * History:-
 * 04/09/02        Created.
 * --------------------------------------------------------------------
 *
 */

#ifndef __CFGGLOBAL_H
#define __CFGGLOBAL_H

#include "sirCommon.h"
#include "sirTypes.h"
#include "wniCfgSta.h"

#define CFG_MAX_NUM_STA      SIR_MAX_NUM_STA_IN_IBSS

#define CFG_MAX_STR_LEN       256    // as the number of channels grows, 128 is not big enough

/*--------------------------------------------------------------------*/
/* Configuration Control Structure                                    */
/*--------------------------------------------------------------------*/
typedef struct
{
    tANI_U32   control;
} tCfgCtl;


typedef struct sAniSirCfg
{
    // CFG module status
    tANI_U8    gCfgStatus;

    tCfgCtl    *gCfgEntry;
    tANI_U32   *gCfgIBufMin;
    tANI_U32   *gCfgIBufMax;
    tANI_U32   *gCfgIBuf;
    tANI_U8    *gCfgSBuf;

    tANI_U16    gCfgMaxIBufSize;
    tANI_U16    gCfgMaxSBufSize;

    // Static buffer for string parameter (must be word-aligned)
    tANI_U8    *gSBuffer;

    // Message parameter list buffer (enough for largest possible response)
    tANI_U32   *gParamList;
} tAniSirCfg,  *tpAniSirCfg;

#endif
