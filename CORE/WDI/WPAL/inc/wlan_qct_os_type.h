/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __WLAN_QCT_OS_TYPE_H )
#define __WLAN_QCT_OS_TYPE_H

/**=========================================================================
  
  \file  wlan_qct_pal_type.h
  
  \brief define basi types PAL exports. wpt = (Wlan Pal Type)
               
   Definitions for platform dependent. This is for Linux/Android
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include <linux/types.h>

typedef u32 wpt_uint32;

typedef s32 wpt_int32;

typedef u16 wpt_uint16;

typedef s16 wpt_int16;

typedef u8 wpt_uint8;

typedef wpt_uint8 wpt_byte;

typedef s8 wpt_int8;

typedef wpt_uint8 wpt_boolean;

typedef u64 wpt_uint64;

typedef s64 wpt_int64;

#define WPT_INLINE __inline__
#define WPT_STATIC static


#endif // __WLAN_QCT_OS_TYPE_H
