/*
* Copyright (c) 2012 Qualcomm Atheros, Inc.
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
*/


/**========================================================================

  \file  wlan_hdd_tdls.c

  \brief WLAN Host Device Driver implementation for P2P commands interface

  Copyright 2012 (c) Qualcomm, Incorporated.  All Rights Reserved.

  Qualcomm Confidential and Proprietary.

  ========================================================================*/

#include <wlan_hdd_includes.h>
#include <wlan_hdd_hostapd.h>
#include <net/cfg80211.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <net/ieee80211_radiotap.h>
#include "wlan_hdd_tdls.h"

typedef struct hddTdlsPeerInfo {
    tSirMacAddr peerMac;
    tANI_U16  	staId ;
} hddTdlsPInfo;

static struct {
    hddTdlsPInfo peer;
} hddTdlsPTable[MAX_NUM_TDLS_PEER];

int wlan_hdd_saveTdlsPeer(tCsrRoamInfo *pRoamInfo)
{
    int i;

    for (i = 0; i < MAX_NUM_TDLS_PEER; i++) {
       if (0 == hddTdlsPTable[i].peer.staId) break;

    }

    if (i < MAX_NUM_TDLS_PEER) {
       vos_mem_copy( hddTdlsPTable[i].peer.peerMac, pRoamInfo->peerMac, 6 );
       hddTdlsPTable[i].peer.staId = pRoamInfo->staId;
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, 
                     "saveTdlsPeer: saved staId %d", pRoamInfo->staId);
       return i;
    }

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
		 "saveTdlsPeer: NOT saving staId %d", pRoamInfo->staId);
    return -1;
}

int wlan_hdd_findTdlsPeer(tSirMacAddr peerMac)
{
    int i;

    for (i = 0; i < MAX_NUM_TDLS_PEER; i++) {
        if (VOS_TRUE == vos_mem_compare( peerMac, hddTdlsPTable[i].peer.peerMac, 6 ))
	    break;
    }

    if (i < MAX_NUM_TDLS_PEER) {
	VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, 
                     "findTdlsPeer: found staId %d", hddTdlsPTable[i].peer.staId);
	return hddTdlsPTable[i].peer.staId;
    }

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
		"findTdlsPeer: staId NOT found");
    return -1;
}
