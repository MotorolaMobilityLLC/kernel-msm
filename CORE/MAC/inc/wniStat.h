/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file wniStat.h contains statistics related definitions
 * exported by Sirius software modules.
 *
 * Author:      Kevin Nguyen 
 * Date:        08/21/2002
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#ifndef _WNISTAT_H
#define _WNISTAT_H


// WNI Statistic Parameter ID
#define WNI_STAT_RTS_SUCC_CNT              1 
#define WNI_STAT_RTS_FAILED_CNT            2
#define WNI_STAT_PACKET_CNT                3
#define WNI_STAT_MULTI_CNT                 4
#define WNI_STAT_DUPL_FRAG_CNT             5
#define WNI_STAT_TOTAL_BYTE_CNT            6
#define WNI_STAT_PKT_DROP_CNT              7
#define WNI_STAT_PKT64_CNT                 8
#define WNI_STAT_PKT127_CNT                9
#define WNI_STAT_PKT255_CNT               10
#define WNI_STAT_PKT511_CNT               11
#define WNI_STAT_PKT1023_CNT              12
#define WNI_STAT_PKT1518_CNT              13 
#define WNI_STAT_PKT2047_CNT              14
#define WNI_STAT_PKT4095_CNT              15
#define WNI_STAT_FRAG_CNT                 16
#define WNI_STAT_FCS_CNT                  17
#define WNI_STAT_BSSID_MISS_CNT           18
#define WNI_STAT_PDU_ERR_CNT              19
#define WNI_STAT_DST_MISS_CNT             20
#define WNI_STAT_DROP_CNT                 21
#define WNI_STAT_ABORT_CNT                22
#define WNI_STAT_RELAY_CNT                23
#define WNI_STAT_HASH_MISS_CNT            24
#define WNI_STAT_PLCP_CRC_ERR_CNT         25
#define WNI_STAT_PLCP_SIG_ERR_CNT         26
#define WNI_STAT_PLCP_SVC_ERR_CNT         27
#define WNI_STAT_BEACONS_RECEIVED_CNT     28
#define WNI_STAT_BEACONS_TRANSMITTED_CNT  29
#define WNI_STAT_CURRENT_TX_RATE          30

#define WNI_STAT_LAST_ID      WNI_STAT_CURRENT_TX_RATE


#endif 

