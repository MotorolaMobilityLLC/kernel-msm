/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef __WLAN_HDD_KEEP_ALIVE_H__
#define __WLAN_HDD_KEEP_ALIVE_H__

/**===========================================================================

  \file  wlan_hdd_keep_alive.h

  \brief Android WLAN HDD Keep-Alive API

  Copyright 2011 (c) QUALCOMM Incorporated. All Rights Reserved.
  QUALCOMM Proprietary and Confidential.

  ==========================================================================*/

/* Packet Types. */
#define WLAN_KEEP_ALIVE_UNSOLICIT_ARP_RSP     2
#define WLAN_KEEP_ALIVE_NULL_PKT              1

/* Enable or disable offload. */
#define WLAN_KEEP_ALIVE_DISABLE     0
#define WLAN_KEEP_ALIVE_ENABLE    0x1

/* Offload request. */
typedef struct
{
    v_U8_t packetType;
    v_U32_t timePeriod;
    v_U8_t  hostIpv4Addr[4]; 
    v_U8_t  destIpv4Addr[4];
    v_U8_t  destMacAddr [6];
    v_U8_t  bssIdx;
} tKeepAliveRequest, *tpKeepAliveRequest;

#endif // __WLAN_HDD_KEEP_ALIVE_H__

