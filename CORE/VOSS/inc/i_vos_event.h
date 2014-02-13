/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __I_VOS_EVENT_H )
#define __I_VOS_EVENT_H

/**=========================================================================

  \file  i_vos_event.h

  \brief Linux-specific definitions for vOSS Events

   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <linux/completion.h>

/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/
#define LINUX_EVENT_COOKIE 0x12341234

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/

typedef struct evt
{
   struct completion complete;
   v_U32_t  cookie;
} vos_event_t;

/*-------------------------------------------------------------------------
  Function declarations and documenation
  ------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __I_VOS_EVENT_H
