/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __WLAN_QCT_PAL_SYNC_H )
#define __WLAN_QCT_PAL_SYNC_H

/**=========================================================================
  
  \file  wlan_pal_sync.h
  
  \brief define synchronization objects PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform independent. 
  
   Copyright 2010 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"
#include "wlan_qct_os_sync.h"


#define WLAN_PAL_WAIT_INFINITE    0xFFFFFFFF

/*---------------------------------------------------------------------------
    wpalMutexInit – initialize a mutex object
    Param:
        pMutex – a pointer to caller allocated object of wpt_mutex
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexInit(wpt_mutex *pMutex);

/*---------------------------------------------------------------------------
    wpalMutexDelete – invalidate a mutex object
    Param:
        pMutex – a pointer to caller allocated object of wpt_mutex
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexDelete(wpt_mutex *pMutex);

/*---------------------------------------------------------------------------
    wpalMutexAcquire – acquire a mutex object. It is blocked until the object is acquired.
    Param:
        pMutex – a pointer to caller allocated object of wpt_mutex
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexAcquire(wpt_mutex *pMutex);

/*---------------------------------------------------------------------------
    wpalMutexRelease – Release a held mutex object
    Param:
        pMutex – a pointer to caller allocated object of wpt_mutex
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalMutexRelease(wpt_mutex *pMutex);

/*---------------------------------------------------------------------------
    wpalEventInit – initialize an event object
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventInit(wpt_event *pEvent);

/*---------------------------------------------------------------------------
    wpalEventDelete – invalidate an event object
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventDelete(wpt_event *pEvent);

/*---------------------------------------------------------------------------
    wpalEventWait – Wait on an event object
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
        timeout – timerout value at unit of milli-seconds. 0xffffffff means infinite wait
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventWait(wpt_event *pEvent, wpt_uint32 timeout);

/*---------------------------------------------------------------------------
    wpalEventSet – Set an event object to signaled state
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventSet(wpt_event *pEvent);

/*---------------------------------------------------------------------------
    wpalEventReset – Set an event object to non-signaled state
    Param:
        pEvent – a pointer to caller allocated object of wpt_event
    Return:
        eWLAN_PAL_STATUS_SUCCESS – success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalEventReset(wpt_event *pEvent);


#endif // __WLAN_QCT_PAL_SYNC_H
