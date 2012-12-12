/*
* Copyright (c) 2012 Qualcomm Atheros, Inc.
* All Rights Reserved.
* Qualcomm Atheros Confidential and Proprietary.
*/

#ifndef __HDD_TDSL_H
#define __HDD_TDSL_H
/**===========================================================================

\file         wlan_hdd_tdls.h

\brief       Linux HDD TDLS include file
               Copyright 2012 (c) Qualcomm, Incorporated.
               All Rights Reserved.
               Qualcomm Confidential and Proprietary.

==========================================================================*/

#define MAX_NUM_TDLS_PEER	4

extern int wlan_hdd_saveTdlsPeer(tCsrRoamInfo *pRoamInfo);

extern int wlan_hdd_findTdlsPeer(tSirMacAddr peerMac);

#endif // __HDD_TDSL_H
