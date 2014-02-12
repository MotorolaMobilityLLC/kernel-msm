/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __LEGACYPALTYPES_H__ )
#define __LEGACYPALTYPES_H__

/*==========================================================================
 *
 *  @file:     halLegacyPalTypes.h
 *
 *  @brief:    Exports and types for the Platform Abstraction Layer typedefs.
 *
 *  @author:   Kumar Anand
 *
 *             Copyright (C) 2010, Qualcomm, Inc.
 *             All rights reserved.
 *
 *=========================================================================*/

#include "qwlanfw_defs.h"

/* Common type definitions */
typedef uint8     tANI_U8;
typedef int8      tANI_S8;
typedef uint16    tANI_U16;
typedef int16     tANI_S16;
typedef uint32    tANI_U32;
typedef int32     tANI_S32;

#ifndef BUILD_QWPTTSTATIC
typedef uint64    tANI_U64;
#endif

typedef byte      tANI_BYTE;
typedef boolean   tANI_BOOLEAN;
typedef uint32    tANI_TIMESTAMP;

#endif /*__LEGACYPALTYPES_H__*/
