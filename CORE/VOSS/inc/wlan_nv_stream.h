/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined _WLAN_NV_STREAM_H
#define _WLAN_NV_STREAM_H

#include "wlan_nv_types.h"

typedef tANI_U8 _NV_STREAM_BUF;

typedef struct {
   _NV_STREAM_BUF *dataBuf;
   tANI_U32 currentIndex;
   tANI_U32 totalLength;
}_STREAM_BUF;

extern _STREAM_BUF streamBuf;

typedef enum {
   RC_FAIL,
   RC_SUCCESS,
} _STREAM_RC;

typedef enum {
   STREAM_READ,
   STREAM_WRITE,
} _STREAM_OPERATION;

_STREAM_RC nextStream (tANI_U32 *length, tANI_U8 *dataBuf);
_STREAM_RC initReadStream ( tANI_U8 *readBuf, tANI_U32 length);

#endif
