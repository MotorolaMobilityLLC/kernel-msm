/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __I_VOS_TIMER_H )
#define __I_VOS_TIMER_H

/**=========================================================================
  
  \file  i_vos_timer.h
  
  \brief Linux-specific definitions for vOSS packets
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_timer.h>
#include <vos_types.h>
#include <linux/timer.h>
#include <linux/time.h>
#include <linux/jiffies.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/

typedef struct vos_timer_platform_s
{
   struct timer_list Timer;
   int threadID; 
   v_U32_t cookie;
   spinlock_t  spinlock;

} vos_timer_platform_t;

/*
 * TODOs: Need to add deferred timer implementation 
 *
*/ 


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __I_VOS_TIMER_H
