/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * Copyright (c) 2008 QUALCOMM Incorporated. All Rights Reserved.
 * Qualcomm Confidential and Proprietary 
 */

/* This file is generated from btampFsm.cdd - do not edit manually*/
/* Generated on: Thu Oct 16 15:40:39 PDT 2008 */


#ifndef __BTAMPFSM_EXT_H__
#define __BTAMPFSM_EXT_H__

/* Events that can be sent to the state-machine */
typedef enum
{
  eWLAN_BAP_TIMER_CONNECT_ACCEPT_TIMEOUT=0U,
  eWLAN_BAP_MAC_CONNECT_COMPLETED
,
  eWLAN_BAP_CHANNEL_SELECTION_FAILED,
  eWLAN_BAP_MAC_CONNECT_FAILED,
  eWLAN_BAP_MAC_CONNECT_INDICATION
,
  eWLAN_BAP_MAC_KEY_SET_SUCCESS,
  eWLAN_BAP_HCI_PHYSICAL_LINK_ACCEPT,
  eWLAN_BAP_RSN_FAILURE,
  eWLAN_BAP_MAC_SCAN_COMPLETE,
  eWLAN_BAP_HCI_PHYSICAL_LINK_CREATE,
  eWLAN_BAP_MAC_READY_FOR_CONNECTIONS
,
  eWLAN_BAP_MAC_START_BSS_SUCCESS
,
  eWLAN_BAP_RSN_SUCCESS,
  eWLAN_BAP_MAC_START_FAILS,
  eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT,
  eWLAN_BAP_MAC_INDICATES_MEDIA_DISCONNECTION,
  eWLAN_BAP_HCI_WRITE_REMOTE_AMP_ASSOC
,
  NO_MSG
}MESSAGE_T;


#endif
