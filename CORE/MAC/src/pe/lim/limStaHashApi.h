/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file limStaHashApi.h contains the
 * function prototypes for accessing station hash entry fields.
 *
 * Author:      Sunit Bhatia
 * Date:        09/19/2006
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __LIM_STA_HASH_API_H__
#define __LIM_STA_HASH_API_H__


#include "aniGlobal.h"
#include "limTypes.h"

tSirRetStatus limGetStaHashBssidx(tpAniSirGlobal pMac, tANI_U16 assocId, tANI_U8 *bssidx,tpPESession psessionEntry);

#endif




