/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __I_VOS_LIST_H )
#define __I_VOS_LIST_H

/**=========================================================================
  
  \file  i_vos_list.h
  
  \brief Linux-specific definitions for vOSS lists 
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <vos_status.h>
#include <vos_packet.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kernel.h>

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/*-------------------------------------------------------------------------- 
  Type declarations
  ------------------------------------------------------------------------*/
typedef struct vos_linux_list_s
{
   struct list_head anchor;
   v_SIZE_t count;
   struct mutex lock;
   v_U32_t cookie;
} vos_list_t;

typedef struct list_head vos_list_node_t;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __I_VOS_LIST_H
