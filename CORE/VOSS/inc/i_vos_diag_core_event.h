/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#if !defined( __I_VOS_DIAG_CORE_EVENT_H )
#define __I_VOS_DIAG_CORE_EVENT_H

/**=========================================================================
  
  \file  i_vos_diag_core_event.h
  
  \brief Android specific definitions for vOSS DIAG events
  
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#ifdef FEATURE_WLAN_DIAG_SUPPORT
#include <event_defs.h>
#endif

/*-------------------------------------------------------------------------- 
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifdef FEATURE_WLAN_DIAG_SUPPORT

void vos_event_report_payload(v_U16_t event_Id, v_U16_t length, v_VOID_t *pPayload);
/*---------------------------------------------------------------------------
  Allocate an event payload holder
---------------------------------------------------------------------------*/
#define WLAN_VOS_DIAG_EVENT_DEF( payload_name, payload_type ) \
           payload_type(payload_name)                         

/*---------------------------------------------------------------------------
  Report the event
---------------------------------------------------------------------------*/
#define WLAN_VOS_DIAG_EVENT_REPORT( payload_ptr, ev_id ) \
   do                                                    \
   {                                                     \
       vos_event_report_payload( ev_id,                  \
                              sizeof( *(payload_ptr) ),  \
                              (void *)(payload_ptr) );   \
                                                       \
   } while (0)

#else /* FEATURE_WLAN_DIAG_SUPPORT */

#define WLAN_VOS_DIAG_EVENT_DEF( payload_name, payload_type ) 
#define WLAN_VOS_DIAG_EVENT_REPORT( payload_ptr, ev_id ) 

#endif /* FEATURE_WLAN_DIAG_SUPPORT */


/*------------------------------------------------------------------------- 
  Function declarations and documenation
  ------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // __I_VOS_DIAG_CORE_EVENT_H
