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


#ifndef __SAPFSM_EXT_H__
#define __SAPFSM_EXT_H__

/* Events that can be sent to the SAP state-machine */
typedef enum
{
  eSAP_TIMER_CONNECT_ACCEPT_TIMEOUT=0U,
  eSAP_MAC_CONNECT_COMPLETED,
  eSAP_CHANNEL_SELECTION_FAILED,
  eSAP_MAC_CONNECT_INDICATION,
  eSAP_MAC_KEY_SET_SUCCESS,
  eSAP_RSN_FAILURE,
  eSAP_MAC_SCAN_COMPLETE,
  eSAP_HDD_START_INFRA_BSS,
  eSAP_MAC_READY_FOR_CONNECTIONS,
  eSAP_MAC_START_BSS_SUCCESS,
  eSAP_MAC_START_BSS_FAILURE,
  eSAP_RSN_SUCCESS,
  eSAP_MAC_START_FAILS,
  eSAP_HDD_STOP_INFRA_BSS,
  eSAP_WRITE_REMOTE_AMP_ASSOC,

  eSAP_NO_MSG
}eSapMsg_t;


#endif
