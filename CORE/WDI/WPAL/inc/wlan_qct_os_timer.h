/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __WLAN_QCT_OS_TIMER_H )
#define __WLAN_QCT_OS_TIMER_H

/**=========================================================================
  
  \file  wlan_qct_os_timer.h
  
  \brief define synchronization objects PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform dependent (Linux Android).
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "vos_timer.h"

typedef struct
{
   vos_timer_t timerObj;
} wpt_os_timer;


#endif // __WLAN_QCT_OS_TIMER_H
