/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined __WLAN_NV_PARSER_H
#define __WLAN_NV_PARSER_H

#include <vos_status.h>

VOS_STATUS nvParser(tANI_U8 *pnvEncodedBuf, tANI_U32 nvReadBufSize, sHalNv *);

#endif
