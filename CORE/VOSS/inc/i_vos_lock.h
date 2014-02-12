/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __I_VOS_LOCK_H )
#define __I_VOS_LOCK_H

/**=========================================================================
  
  \file  i_vos_lock.h
  
  \brief Linux-specific definitions for vOSS Locks
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/sched.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
typedef struct vos_lock_s
{
   struct mutex m_lock;
   v_U32_t cookie;
   int processID;
   v_U32_t state;
   v_U8_t  refcount;
} vos_lock_t;

typedef spinlock_t vos_spin_lock_t;

/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __I_VOS_LOCK_H
