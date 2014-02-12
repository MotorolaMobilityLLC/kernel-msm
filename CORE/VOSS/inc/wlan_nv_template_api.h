/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined _WLAN_NV_TEMPLATE_API_H
#define  _WLAN_NV_TEMPLATE_API_H

#include "wlan_nv_types.h"

/*
* API Prototypes
* These are meant to be exposed to outside applications
*/
//extern void writeNvData(void);
//extern VOS_STATUS nvParser(tANI_U8 *pnvEncodedBuf, tANI_U32 nvReadBufSize, sHalNv *);

/*
* Parsing control bitmap
*/
#define _ABORT_WHEN_MISMATCH_MASK  0x00000001     /*set: abort when mismatch, clear: continue taking matched entries*/
#define _IGNORE_THE_APPENDED_MASK  0x00000002     /*set: ignore, clear: take*/

#define _FLAG_AND_ABORT(b)    (((b) & _ABORT_WHEN_MISMATCH_MASK) ? 1 : 0)

#endif /*#if !defined(_WLAN_NV_TEMPLATE_API_H) */
