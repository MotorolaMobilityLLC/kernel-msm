/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __WLAN_QCT_OS_SYNC_H )
#define __WLAN_QCT_OS_SYNC_H

/**=========================================================================
  
  \file  wlan_qct_os_sync.h
  
  \brief define synchronization objects PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform dependent(LA). 
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "vos_event.h"
#include "vos_lock.h"

/*Reuse the vos lock and vos event*/
typedef vos_lock_t  wpt_mutex;
typedef vos_event_t wpt_event;


#endif // __WLAN_QCT_OS_SYNC_H
