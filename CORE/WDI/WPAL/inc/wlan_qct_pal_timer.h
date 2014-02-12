/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __WLAN_QCT_PAL_TIMER_H )
#define __WLAN_QCT_PAL_TIMER_H

/**=========================================================================
  
  \file  wlan_qct_pal_timer.h
  
  \brief define synchronization objects PAL exports. wpt = (Wlan Pal Type) wpal = (Wlan PAL)
               
   Definitions for platform independent.
  
   Copyright 2010-2011 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

#include "wlan_qct_pal_type.h"
#include "wlan_qct_pal_status.h"
#include "wlan_qct_os_timer.h"


typedef VOS_TIMER_STATE WPAL_TIMER_STATE;

typedef void (*wpal_timer_callback)(void *pUserData);

typedef struct
{
   wpt_os_timer timer;
   wpal_timer_callback callback;
   void *pUserData;
} wpt_timer;


/*---------------------------------------------------------------------------
    wpalTimerInit - initialize a wpt_timer object
    Param:
        pTimer - a pointer to caller allocated wpt_timer object
        callback - A callback function
        pUserData - A pointer to data pass as parameter of the callback function.
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalTimerInit(wpt_timer * pTimer, wpal_timer_callback callback, void *pUserData);

/*---------------------------------------------------------------------------
    wpalTimerDelete - invalidate a wpt_timer object
    Param:
        pTimer - a pointer to caller allocated wpt_timer object
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalTimerDelete(wpt_timer * pTimer);

/*---------------------------------------------------------------------------
    wpalTimerStart - start a wpt_timer object with a timeout value
    Param:
        pTimer - a pointer to caller allocated wpt_timer object
        timeout - timeout value of the timer. In unit of milli-seconds.
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalTimerStart(wpt_timer * pTimer, wpt_uint32 timeout);

/*---------------------------------------------------------------------------
    wpalTimerStop - stop a wpt_timer object. Stop doesn’t guarantee the timer handler is not called if it is already timeout.
    Param:
        pTimer - a pointer to caller allocated wpt_timer object
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalTimerStop(wpt_timer * pTimer);

/*---------------------------------------------------------------------------
    wpalTimerGetCurStatus - Get the current status of timer

    pTimer - a pointer to caller allocated wpt_timer object

    return
        WPAL_TIMER_STATE
---------------------------------------------------------------------------*/
WPAL_TIMER_STATE wpalTimerGetCurStatus(wpt_timer * pTimer);

/*---------------------------------------------------------------------------
    wpalGetSystemTime - Get the system time in milliseconds

    return
        current time in milliseconds
---------------------------------------------------------------------------*/
wpt_uint32 wpalGetSystemTime(void);

/*---------------------------------------------------------------------------
    wpalGetArchCounterTime - Get time from physical counter

    return
        MPM counter value
---------------------------------------------------------------------------*/
wpt_uint64 wpalGetArchCounterTime(void);

/*---------------------------------------------------------------------------
    wpalSleep - sleep for a specified interval
    Param:
        timeout - amount of time to sleep. In unit of milli-seconds.
    Return:
        eWLAN_PAL_STATUS_SUCCESS - success. Fail otherwise.
---------------------------------------------------------------------------*/
wpt_status wpalSleep(wpt_uint32 timeout);

/*---------------------------------------------------------------------------
    wpalBusyWait - Thread busy wait with specified usec
    Param:
        usecDelay - amount of time to wait. In unit of micro-seconds.
    Return:
        NONE
---------------------------------------------------------------------------*/
void wpalBusyWait(wpt_uint32 usecDelay);

#endif // __WLAN_QCT_PAL_TIMER_H
