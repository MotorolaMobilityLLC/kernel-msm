/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <athdefs.h>
#include <a_types.h>
#include "dbglog.h"
#include "dbglog_id.h"
#include "dbglog_host.h"
#include "msg.h"
#include "msgcfg.h"
#include "diag_lsm.h"
#include "log.h"

#include "a_debug.h"
#include "ol_defines.h"
#include "ah_osdep.h"

extern void
dbglog_printf(
    A_UINT32 timestamp,
    A_UINT16 vap_id,
    const char *fmt, ...);

extern void
dbglog_sm_print(
    A_UINT32 timestamp,
    A_UINT16 vap_id,
    A_UINT16 numargs,
    A_UINT32 *args,
    const char *module_prefix,
    const char *states[], A_UINT32 num_states,
    const char *events[], A_UINT32 num_events);

static const char *
dbglog_get_nan_role_str(
    A_UINT8 nan_role);

static const char *
dbglog_get_nan_chan_type(
    A_UINT32 type);

static const char *
dbglog_get_nan_tlv_type_str(
    A_UINT16 tlv_type);

#define MAX_DBG_MSGS 256

/* NAN DBGIDs */
#define NAN_DBGID_START                             0

/* Debug IDs for debug logs. 3 args max, not fixed. */
#define NAN_DBGID_DBG_LOG_BASE                      1
#define NAN_DBGID_FUNC_BEGIN                        NAN_DBGID_DBG_LOG_BASE
#define NAN_DBGID_FUNC_END                          2
#define NAN_DBGID_MAIN_DEBUG                        3
#define NAN_DBGID_MAC_DEBUG                         4
#define NAN_DBGID_BLOOM_FILTER_DEBUG                5
#define NAN_DBGID_MAC_ADDR                          6
#define NAN_DBGID_PARAM_UPDATED                     7
#define NAN_DBGID_NULL_PTR                          8
#define NAN_DBGID_INVALID_FUNC_ARG                  9
#define NAN_DBGID_INVALID_MSG_PARAM                 10
#define NAN_DBGID_MISSING_MSG_PARAM                 11
#define NAN_DBGID_DEPRECATED_MSG_PARAM              12
#define NAN_DBGID_UNSUPPORTED_MSG_PARAM             13
#define NAN_DBGID_INVALID_PKT_DATA                  14
#define NAN_DBGID_LOG_PKT_DATA                      15
#define NAN_DBGID_INVALID_VALUE                     16
#define NAN_DBGID_INVALID_OPERATION                 17
#define NAN_DBGID_INVALID_STATE                     18
#define NAN_DBGID_FUNCTION_ENABLED                  19
#define NAN_DBGID_FUNCTION_DISABLED                 20
#define NAN_DBGID_INVALID_FUNCTION_STATE            21
#define NAN_DBGID_READ_ERROR                        22
#define NAN_DBGID_WRITE_ERROR                       23
#define NAN_DBGID_RECEIVE_ERROR                     24
#define NAN_DBGID_TRANSMIT_ERROR                    25
#define NAN_DBGID_PARSE_ERROR                       26
#define NAN_DBGID_RES_ALLOC_ERROR                   27
/* PLEASE KEEP THIS ONE AT THE END */
#define NAN_DBGID_DBG_LOG_LAST                      28

/* Debug IDs for event logs. */

#define NAN_DBGID_EVT_BASE                          NAN_DBGID_DBG_LOG_LAST
/* args: <none> */
#define NAN_DBGID_NAN_ENABLED                       (NAN_DBGID_EVT_BASE + 0)
/* args: <none> */
#define NAN_DBGID_NAN_DISABLED                      (NAN_DBGID_EVT_BASE + 1)
/* args: <none> */
#define NAN_DBGID_CONFIG_RESTORED                   (NAN_DBGID_EVT_BASE + 2)
/* args: framesQueued */
#define NAN_DBGID_SDF_QUEUED                        (NAN_DBGID_EVT_BASE + 3)
/* args: old, new */
#define NAN_DBGID_TW_CHANGED                        (NAN_DBGID_EVT_BASE + 4)
/* args: <none> */
#define NAN_DBGID_DW_START                          (NAN_DBGID_EVT_BASE + 5)
/* args: busyDiff */
#define NAN_DBGID_DW_END                            (NAN_DBGID_EVT_BASE + 6)
/* args: oldClusterId, newClusterId */
#define NAN_DBGID_CLUSTER_ID_CHANGED                (NAN_DBGID_EVT_BASE + 7)
/* args: cmd, buffer, length */
#define NAN_DBGID_WMI_CMD_RECEIVED                  (NAN_DBGID_EVT_BASE + 8)
/* args: pEventPkt, pEventBuf, eventSize, dataSize */
#define NAN_DBGID_WMI_EVT_SENT                      (NAN_DBGID_EVT_BASE + 9)
/* args: type length, readLen */
#define NAN_DBGID_TLV_READ                          (NAN_DBGID_EVT_BASE + 10)
/* args: type length, writeLen */
#define NAN_DBGID_TLV_WRITE                         (NAN_DBGID_EVT_BASE + 11)
/* args: handle */
#define NAN_DBGID_PUBSUB_UPDATED                    (NAN_DBGID_EVT_BASE + 12)
/* args: handle */
#define NAN_DBGID_PUBSUB_REMOVE_DEFERRED            (NAN_DBGID_EVT_BASE + 13)
/* args: handle */
#define NAN_DBGID_PUBSUB_REMOVE_PENDING             (NAN_DBGID_EVT_BASE + 14)
/* args: handle */
#define NAN_DBGID_PUBSUB_REMOVED                    (NAN_DBGID_EVT_BASE + 15)
/* args: handle, status, value */
#define NAN_DBGID_PUBSUB_PROCESSED                  (NAN_DBGID_EVT_BASE + 16)
/* args: handle, sid1, sid2, svcCtrl, length */
#define NAN_DBGID_PUBSUB_MATCHED                    (NAN_DBGID_EVT_BASE + 17)
/* args: handle, flags */
#define NAN_DBGID_PUBSUB_PREPARED                   (NAN_DBGID_EVT_BASE + 18)
/* args: handle, mac1, mac2 */
#define NAN_DBGID_PUBSUB_FOLLOWUP_TRANSMIT          (NAN_DBGID_EVT_BASE + 19)
/* args: handle, mac1, mac2 */
#define NAN_DBGID_PUBSUB_FOLLOWUP_RECEIVED          (NAN_DBGID_EVT_BASE + 20)
/* args: subscribeHandle, matchHandle, oldTimeout, newTimeout */
#define NAN_DBGID_SUBSCRIBE_UNMATCH_TIMEOUT_UPDATE  (NAN_DBGID_EVT_BASE + 21)
/* args: subscribeHandle, matchHandle, timestamp*/
#define NAN_DBGID_SUBSCRIBE_MATCH_NEW               (NAN_DBGID_EVT_BASE + 22)
/* args: subscribeHandle, matchHandle, timestamp*/
#define NAN_DBGID_SUBSCRIBE_MATCH_REPEAT            (NAN_DBGID_EVT_BASE + 23)
/* args: subscribeHandle, matchHandle, matchTimestamp, timestamp*/
#define NAN_DBGID_SUBSCRIBE_MATCH_EXPIRED           (NAN_DBGID_EVT_BASE + 24)
/* args: subscribeHandle, matchHandle, matchTimestamp, timestamp */
#define NAN_DBGID_SUBSCRIBE_MATCH_LOG               (NAN_DBGID_EVT_BASE + 25)
/* args: sid1, sid2 */
#define NAN_DBGID_SERVICE_ID_CREATED                (NAN_DBGID_EVT_BASE + 26)
/* args: size */
#define NAN_DBGID_SD_ATTR_BUILT                     (NAN_DBGID_EVT_BASE + 27)
/* args: offset */
#define NAN_DBGID_SERVICE_RSP_OFFSET                (NAN_DBGID_EVT_BASE + 28)
/* args: offset */
#define NAN_DBGID_SERVICE_INFO_OFFSET               (NAN_DBGID_EVT_BASE + 29)
/* args: chan, interval, start_time */
#define NAN_DBGID_CHREQ_CREATE                      (NAN_DBGID_EVT_BASE + 30)
/* args: start_time, status */
#define NAN_DBGID_CHREQ_UPDATE                      (NAN_DBGID_EVT_BASE + 31)
/* args: chan, interval, status */
#define NAN_DBGID_CHREQ_REMOVE                      (NAN_DBGID_EVT_BASE + 32)
/* args: type, timestamp */
#define NAN_DBGID_CHREQ_GRANT                       (NAN_DBGID_EVT_BASE + 33)
/* args: type, timestamp */
#define NAN_DBGID_CHREQ_END                         (NAN_DBGID_EVT_BASE + 34)
/* args: type, timestamp */
#define NAN_DBGID_CHREQ_ERROR                       (NAN_DBGID_EVT_BASE + 35)
/* args: type, length, timestamp, rssi */
#define NAN_DBGID_RX_CALLBACK                       (NAN_DBGID_EVT_BASE + 36)
/* args: type, handle, bufp, status, timestamp */
#define NAN_DBGID_TX_COMPLETE                       (NAN_DBGID_EVT_BASE + 37)
/* args: tsf, tsf */
#define NAN_DBGID_TSF_TIMEOUT                       (NAN_DBGID_EVT_BASE + 38)
/* args: clusterId, clusterStart */
#define NAN_DBGID_SYNC_START                        (NAN_DBGID_EVT_BASE + 39)
/* args: clusterId */
#define NAN_DBGID_SYNC_STOP                         (NAN_DBGID_EVT_BASE + 40)
/* args: enable, scanType, rval */
#define NAN_DBGID_NAN_SCAN                          (NAN_DBGID_EVT_BASE + 41)
/* args: scanType */
#define NAN_DBGID_NAN_SCAN_COMPLETE                 (NAN_DBGID_EVT_BASE + 42)
/* args: masterPref */
#define NAN_DBGID_MPREF_CHANGE                      (NAN_DBGID_EVT_BASE + 43)
/* args: masterPref, randFactor */
#define NAN_DBGID_WARMUP_EXPIRE                     (NAN_DBGID_EVT_BASE + 44)
/* args: randFactor */
#define NAN_DBGID_RANDOM_FACTOR_EXPIRE              (NAN_DBGID_EVT_BASE + 45)
/* args: tsf, tsf */
#define NAN_DBGID_DW_SKIP                           (NAN_DBGID_EVT_BASE + 46)
/* args: type, tsfDiff */
#define NAN_DBGID_DB_SKIP                           (NAN_DBGID_EVT_BASE + 47)
/* args: TBD */
#define NAN_DBGID_BEACON_RX                         (NAN_DBGID_EVT_BASE + 48)
/* args: TBD */
#define NAN_DBGID_BEACON_TX                         (NAN_DBGID_EVT_BASE + 49)
/* args: clusterId */
#define NAN_DBGID_CLUSTER_MERGE                     (NAN_DBGID_EVT_BASE + 50)
/* args: cmd, status, value */
#define NAN_DBGID_TEST_CMD_EXEC                     (NAN_DBGID_EVT_BASE + 51)
/* args: tsfHi, tsfLo, age */
#define NAN_DBGID_APPLY_BEACON_TSF                  (NAN_DBGID_EVT_BASE + 52)
/* args: behindFlag, diff */
#define NAN_DBGID_TSF_UPDATE                        (NAN_DBGID_EVT_BASE + 53)
/* args: argc==4 (rawTsfHi, rawTsfLo, nanTsfHi, nanTsfLo), argc==2(offsetHi, offsetLo) */
#define NAN_DBGID_SET_TSF                           (NAN_DBGID_EVT_BASE + 54)
/* args: rankHi, rankLo, mp, rf*/
#define NAN_DBGID_NEW_MASTERRANK                    (NAN_DBGID_EVT_BASE + 55)
/* args: amRankHi, amRankLo, mp, rf */
#define NAN_DBGID_NEW_ANCHORMASTER                  (NAN_DBGID_EVT_BASE + 56)
/* args: amRankHi, amRankLo, HC, BTT */
#define NAN_DBGID_ANCHORMASTER_RECORD_UPDATE        (NAN_DBGID_EVT_BASE + 57)
/* args: amRankHi, amRankLo, HC, BTT */
#define NAN_DBGID_ANCHORMASTER_RECORD_EXPIRED       (NAN_DBGID_EVT_BASE + 58)
/* args: reason, transitionsToAM */
#define NAN_DBGID_BECOMING_ANCHORMASTER             (NAN_DBGID_EVT_BASE + 59)
/* args: oldRole, newRole */
#define NAN_DBGID_ROLE_CHANGE                       (NAN_DBGID_EVT_BASE + 60)
/* args: oldRole, newRole */
#define NAN_DBGID_SYNC_BEACON_DW_STATS              (NAN_DBGID_EVT_BASE + 61)
/* args: attrId */
#define NAN_DBGID_RX_UNSUPPORTED_SDF_ATTR_ID        (NAN_DBGID_EVT_BASE + 62)
/* args: handle, sid1, sid2, svcCtrl, length */
#define NAN_DBGID_PUBSUB_MATCHED_SKIPPED_SSI        (NAN_DBGID_EVT_BASE + 63)
/* args: offset */
#define NAN_DBGID_MATCH_FILTER_OFFSET               (NAN_DBGID_EVT_BASE + 64)
/* args: tsSize, n, twIndex */
#define NAN_DBGID_TW_PARAMS                         (NAN_DBGID_EVT_BASE + 65)
/* args: */
#define NAN_DBGID_BEACON_SENDER                     (NAN_DBGID_EVT_BASE + 66)
/* args: */
#define NAN_DBGID_SPARE_67                          (NAN_DBGID_EVT_BASE + 67)
/* args: */
#define NAN_DBGID_SPARE_68                          (NAN_DBGID_EVT_BASE + 68)
/* args: */
#define NAN_DBGID_SPARE_69                          (NAN_DBGID_EVT_BASE + 69)
/* args: */
#define NAN_DBGID_SPARE_70                          (NAN_DBGID_EVT_BASE + 70)
/* args: */
#define NAN_DBGID_SPARE_71                          (NAN_DBGID_EVT_BASE + 71)
/* args: */
#define NAN_DBGID_SPARE_72                          (NAN_DBGID_EVT_BASE + 72)
/* args: */
#define NAN_DBGID_SPARE_73                          (NAN_DBGID_EVT_BASE + 73)
/* args: */
#define NAN_DBGID_SPARE_74                          (NAN_DBGID_EVT_BASE + 74)
/* args: */
#define NAN_DBGID_SPARE_75                          (NAN_DBGID_EVT_BASE + 75)

/* PLEASE KEEP THIS ONE AT THE END */
#define NAN_DBGID_EVT_LOG_LAST                      (NAN_DBGID_EVT_BASE + 76)

/* Debug IDs for message logs. */
#define NAN_DBGID_API_MSG_BASE                      NAN_DBGID_EVT_LOG_LAST
#define NAN_DBGID_API_MSG_HEADER                    (NAN_DBGID_API_MSG_BASE + 0)
#define NAN_DBGID_API_MSG_DATA                      (NAN_DBGID_API_MSG_BASE + 1)
#define NAN_DBGID_API_MSG_LAST                      (NAN_DBGID_API_MSG_BASE + 2)

/* Debug IDs for packet logs. */
#define NAN_DBGID_OTA_PKT_BASE                      NAN_DBGID_API_MSG_LAST
#define NAN_DBGID_OTA_PKT_HEADER                    (NAN_DBGID_OTA_PKT_BASE + 0)
#define NAN_DBGID_OTA_PKT_DATA                      (NAN_DBGID_OTA_PKT_BASE + 1)
#define NAN_DBGID_OTA_PKT_LAST                      (NAN_DBGID_OTA_PKT_BASE + 2)

#define NAN_DBGID_END                               NAN_DBGID_OTA_PKT_LAST

static char *DBG_MSG_ARR[MAX_DBG_MSGS] =
{
    /* Start */
    "NAN debug ID start",

    /* Debug log IDs */
    "NAN function begin",
    "NAN function end",
    "NAN main debug",
    "NAN MAC debug",
    "NAN bloom filter debug",
    "NAN MAC address",
    "NAN param updated",
    "NAN NULL pointer",
    "NAN invalid function argument",
    "NAN invalid message parameter",
    "NAN missing message parameter",
    "NAN deprecated message parameter",
    "NAN unsupported message parameter",
    "NAN invalid packet data",
    "NAN log packet data",
    "NAN invalid value",
    "NAN invalid operation",
    "NAN invalid state",
    "NAN function enabled",
    "NAN function disable",
    "NAN invalid funtion state",
    "NAN read error",
    "NAN write error",
    "NAN receive error",
    "NAN transmit error",
    "NAN parse error",
    "NAN resource allocation error",

    /* Event log IDs */
    "NAN enabled",
    "NAN disabled",
    "NAN config restored",
    "NAN SDF queued",
    "NAN TW changed",
    "NAN DW start",
    "NAN DW end",
    "NAN cluster ID changed",
    "NAN WMI command received",
    "NAN WMI event sent",
    "NAN TLV read",
    "NAN TLV write",
    "NAN pub/sub updated",
    "NAN pub/sub remove deferred",
    "NAN pub/sub remove pending",
    "NAN pub/sub removed",
    "NAN pub/sub processed",
    "NAN pub/sub matched",
    "NAN pub/sub prepared",
    "NAN pub/sub follow-up transmit",
    "NAN pub/sub follow-up received",
    "NAN subscribe unmatch timeout update",
    "NAN subscribe match new",
    "NAN subscribe match repeat",
    "NAN subscribe match expired",
    "NAN subscribe match log",
    "NAN service ID created",
    "NAN SD attribute built",
    "NAN service response offset",
    "NAN service info offset",
    "NAN channel request create",
    "NAN channel request update",
    "NAN channel request remove",
    "NAN channel request grant",
    "NAN channel request end",
    "NAN channel request error",
    "NAN rx callback",
    "NAN tx complete",
    "NAN TSF timeout",
    "NAN sync start",
    "NAN sync stop",
    "NAN scan",
    "NAN scan complete",
    "NAN master preference changed",
    "NAN warm-up timer expired",
    "NAN random factor timer expired",
    "NAN DW skip",
    "NAN DB skip",
    "NAN beacon rx",
    "NAN beacon tx",
    "NAN cluster merge",
    "NAN test command exec",
    "NAN apply beacon tsf",
    "NAN tsf update",
    "NAN set tsf",
    "NAN new master rank",
    "NAN new anchor master device",
    "NAN anchor master record update",
    "NAN anchor master record expired",
    "NAN becoming anchor master",
    "NAN role change",
    "NAN sync beacon stats",
    "NAN received unsupported attribute id",
    "NAN pub/sub matched/skipped",
    "NAN match filter offset",
    "NAN TW params",
    "NAN beacon sender",
    "NAN spare 67",
    "NAN spare 68",
    "NAN spare 69",
    "NAN spare 70",
    "NAN spare 71",
    "NAN spare 72",
    "NAN spare 73",
    "NAN spare 74",
    "NAN spare 75",

    /* Message log IDs */
    "NAN API message header",
    "NAN API Message data",

    /* Packet log IDs */
    "NAN OTA packet",
    "NAN OTA packet data",

    /* End */
    "NAN End"
};

#define CHARS_PER_DATA_BYTE 3
#define BYTES_PER_ARG       sizeof(A_UINT32)
#define CHARS_PER_ARG       (CHARS_PER_DATA_BYTE * BYTES_PER_ARG)
#define STR_INDEX(arg_idx, byte_idx) \
    (((arg_idx) * CHARS_PER_ARG) + ((byte_idx) * CHARS_PER_DATA_BYTE))

#define NAN_SUBSCRIBE_HANDLE_OFFSET 128

#define PUBSUB_STR(handle) \
    (((handle) < NAN_SUBSCRIBE_HANDLE_OFFSET) ? "publish" : "subscribe")

#define BOOL_STR(value) ((value) ? "TRUE" : "FALSE")

typedef enum
{
    NAN_MODULE_ID_ATTR,
    NAN_MODULE_ID_BEACON,
    NAN_MODULE_ID_BEACON_IE,
    NAN_MODULE_ID_BLOOM_FILTER,
    NAN_MODULE_ID_MAIN,
    NAN_MODULE_ID_CLUSTER_SM,
    NAN_MODULE_ID_CRC,
    NAN_MODULE_ID_CTRL,
    NAN_MODULE_ID_DE_IF,
    NAN_MODULE_ID_LOG,
    NAN_MODULE_ID_MAC,
    NAN_MODULE_ID_PUBLISH,
    NAN_MODULE_ID_RX,
    NAN_MODULE_ID_SUBSCRIBE,
    NAN_MODULE_ID_SYNC,
    NAN_MODULE_ID_TIME,
    NAN_MODULE_ID_TLV,
    NAN_MODULE_ID_UTIL,
    NAN_MODULE_ID_LAST
} tNanModuleId;

static A_BOOL
dbglog_nan_debug_print_handler(
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    static const char *modules[] =
    {
        "wlan_nan_attr.c",
        "wlan_nan_beacon.c",
        "wlan_nan_beacon_ie.c",
        "wlan_nan_bloom_filter.c",
        "wlan_nan.c",
        "wlan_nan_cluster_sm.c",
        "wlan_nan_crc.c",
        "wlan_nan_ctrl.c",
        "wlan_nan_de_if.c",
        "wlan_nan_log.c",
        "wlan_nan_mac.c",
        "wlan_nan_publish.c",
        "wlan_nan_rx.c",
        "wlan_nan_subscribe.c",
        "wlan_nan_sched.c",
        "wlan_nan_sync.c",
        "wlan_nan_time.c",
        "wlan_nan_tlv.c",
        "wlan_nan_util.c"
    };

    char str[64] = { 0 };
    A_UINT16 arg_idx;

    /*
     * Start at 2 here and subtract 2 below because arg[0] specifies the NAN
     * module ID and args[1] specifies the line number within the module.
     */
    for (arg_idx=2; arg_idx < numargs; ++arg_idx)
    {
        snprintf(&str[(arg_idx-2)*11], sizeof(&str[(arg_idx-2)*11]),
                "0x%08X ", args[arg_idx]);
    }

    if (args[0] < NAN_MODULE_ID_LAST)
    {
        dbglog_printf(timestamp, vap_id, "%s, line %u: %s, %s",
            modules[args[0]], args[1], DBG_MSG_ARR[dbg_id], &str[0]);
    }
    else
    {
        dbglog_printf(timestamp, vap_id, "Unknown module %u, line %u: %s, %s",
            args[0], args[1], DBG_MSG_ARR[dbg_id], &str[0]);
    }

    return TRUE;
}

static A_BOOL
dbglog_nan_event_print_handler(
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{

    switch (dbg_id)
    {
    case NAN_DBGID_NAN_ENABLED:
        dbglog_printf(timestamp, vap_id, "%s", DBG_MSG_ARR[dbg_id]);
        break;

    case NAN_DBGID_NAN_DISABLED:
        dbglog_printf(timestamp, vap_id, "%s", DBG_MSG_ARR[dbg_id]);
        break;

    case NAN_DBGID_CONFIG_RESTORED:
        dbglog_printf(timestamp, vap_id, "%s", DBG_MSG_ARR[dbg_id]);
        break;

    case NAN_DBGID_SDF_QUEUED:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s, frames queued %u",
            DBG_MSG_ARR[dbg_id], args[0]);
        break;

    case NAN_DBGID_TW_CHANGED:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s, old %u, new %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1]);
        break;

    case NAN_DBGID_DW_START:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "[==================> %s (%s)",
            DBG_MSG_ARR[dbg_id], args[0] ? "secondary" : "primary");
        break;

    case NAN_DBGID_DW_END:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "<==================] %s (%s)",
            DBG_MSG_ARR[dbg_id], args[0] ? "secondary" : "primary");
        break;

    case NAN_DBGID_CLUSTER_ID_CHANGED:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s, 0x%04X -> 0x%04X",
            DBG_MSG_ARR[dbg_id], args[0], args[1]);
        break;

    case NAN_DBGID_WMI_CMD_RECEIVED:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s, cmd 0x%08X, buffer 0x%08X, length %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2]);
        break;

    case NAN_DBGID_WMI_EVT_SENT:
        if (numargs != 4)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s, pEventPkt 0x%08X, pEventBuf 0x%08X, event size %u, data size %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3]);
        break;

    case NAN_DBGID_TLV_READ:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s, type %s (%u), read length %u",
            DBG_MSG_ARR[dbg_id], dbglog_get_nan_tlv_type_str(args[0]), args[0],
            args[1]);
        break;

    case NAN_DBGID_TLV_WRITE:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s, type %s (%u), length %u, write length %u",
            DBG_MSG_ARR[dbg_id], dbglog_get_nan_tlv_type_str(args[0]), args[0],
            args[1], args[2]);
        break;

    case NAN_DBGID_PUBSUB_UPDATED:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "NAN %s %u updated",
            PUBSUB_STR(args[0]), args[0]);
        break;

    case NAN_DBGID_PUBSUB_REMOVE_DEFERRED:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "NAN %s %u remove deferred",
            PUBSUB_STR(args[0]), args[0]);
        break;

    case NAN_DBGID_PUBSUB_REMOVE_PENDING:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "NAN %s %u remove pending",
            PUBSUB_STR(args[0]), args[0]);
        break;

    case NAN_DBGID_PUBSUB_REMOVED:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "NAN %s %u removed",
            PUBSUB_STR(args[0]), args[0]);
        break;

    case NAN_DBGID_PUBSUB_PROCESSED:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "NAN %s %u processed, status %u, value %u",
            PUBSUB_STR(args[0]), args[0], args[1], args[2]);
        break;

    case NAN_DBGID_PUBSUB_MATCHED:
        if (numargs != 5)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "NAN %s %u matched, instance id %u, requestor id %u, "
            "svc id %02X:%02X:%02X:%02X:%02X:%02X, svcCtrl 0x%02X, len %u",
            PUBSUB_STR(args[0]), args[0],
            (args[2] & 0xFF), ((args[2] >> 8) & 0xFF),
            ((args[1] >> 24) & 0xFF), ((args[1] >> 16) & 0xFF),
            ((args[1] >> 8) & 0xFF), (args[1] & 0xFF),
            ((args[2] >> 24) & 0xFF), ((args[2] >> 16) & 0xFF),
            args[3], args[4]);
        break;

    case NAN_DBGID_PUBSUB_PREPARED:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "NAN %s %u prepared, flags %X",
            PUBSUB_STR(args[0]), args[0], args[1]);
        break;

    case NAN_DBGID_PUBSUB_FOLLOWUP_TRANSMIT:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "NAN %s %u follow-up transmit, MAC address "
            "%02X:%02X:%02X:%02X:%02X:%02X",
            PUBSUB_STR(args[0]), args[0],
            ((args[1] >> 24) & 0xFF), ((args[1] >> 16) & 0xFF),
            ((args[1] >> 8) & 0xFF), (args[1] & 0xFF),
            ((args[2] >> 24) & 0xFF), ((args[2] >> 16) & 0xFF));
        break;

    case NAN_DBGID_PUBSUB_FOLLOWUP_RECEIVED:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "NAN %s %u follow-up received, MAC address "
            "%02X:%02X:%02X:%02X:%02X:%02X",
            PUBSUB_STR(args[0]), args[0],
            ((args[1] >> 24) & 0xFF), ((args[1] >> 16) & 0xFF),
            ((args[1] >> 8) & 0xFF), (args[1] & 0xFF),
            ((args[2] >> 24) & 0xFF), ((args[2] >> 16) & 0xFF));
        break;

    case NAN_DBGID_SUBSCRIBE_UNMATCH_TIMEOUT_UPDATE:
        if (numargs != 4)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s, handle %u, match handle %u, old timeout %u, new timeout %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3]);
        break;

    case NAN_DBGID_SUBSCRIBE_MATCH_NEW:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s, handle %u, match handle %u, timestamp %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2]);
        break;

    case NAN_DBGID_SUBSCRIBE_MATCH_REPEAT:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s, handle %u, match handle %u, timestamp %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2]);
        break;

    case NAN_DBGID_SUBSCRIBE_MATCH_EXPIRED:
        if (numargs != 4)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s, handle %u, match handle %u, match timestamp %u, timestamp %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3]);
        break;

    case NAN_DBGID_SUBSCRIBE_MATCH_LOG:
        if (numargs != 4)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s, handle %u, match handle %u, match timestamp %u, timestamp %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3]);
        break;

    case NAN_DBGID_SERVICE_ID_CREATED:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s %02X:%02X:%02X:%02X:%02X:%02X", DBG_MSG_ARR[dbg_id],
            ((args[0] >> 24) & 0xFF), ((args[0] >> 16) & 0xFF),
            ((args[0] >> 8) & 0xFF), (args[0] & 0xFF),
            ((args[1] >> 24) & 0xFF), ((args[1] >> 16) & 0xFF));
        break;

    case NAN_DBGID_SD_ATTR_BUILT:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s, size %u",
            DBG_MSG_ARR[dbg_id], args[0]);
        break;

    case NAN_DBGID_SERVICE_RSP_OFFSET:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s %u",
            DBG_MSG_ARR[dbg_id], args[0]);
        break;

    case NAN_DBGID_SERVICE_INFO_OFFSET:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s %u",
            DBG_MSG_ARR[dbg_id], args[0]);
        break;

    case NAN_DBGID_CHREQ_CREATE:
        if (numargs != 4)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s: chan %u, interval %u, start_time 0x%08X, duration %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3]);
        break;

    case NAN_DBGID_CHREQ_UPDATE:
        if (numargs != 4)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s: chan %u, interval %u, start_time 0x%08X, duration %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3]);
        break;

    case NAN_DBGID_CHREQ_REMOVE:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: chan %u, interval %u, status %d",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2]);
        break;

    case NAN_DBGID_CHREQ_GRANT:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: chan %u, %s, tsfLo 0x%08X",
            DBG_MSG_ARR[dbg_id], args[0], dbglog_get_nan_chan_type(args[1]),
            args[2]);
        break;

    case NAN_DBGID_CHREQ_END:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: chan %u, %s, tsfLo 0x%08X",
            DBG_MSG_ARR[dbg_id], args[0], dbglog_get_nan_chan_type(args[1]),
            args[2]);
        break;

    case NAN_DBGID_CHREQ_ERROR:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: chan %u, %s, tsfLo 0x%08X",
            DBG_MSG_ARR[dbg_id], args[0], dbglog_get_nan_chan_type(args[1]),
            args[2]);
        break;

    case NAN_DBGID_RX_CALLBACK:
        if (numargs != 5)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s: type 0x%X, length %u, timestamp 0x%08X, rssi %d, freq %d",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3], args[4]);
        break;

    case NAN_DBGID_TX_COMPLETE:
        if (numargs != 5)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s: type 0x%X, handle %u, bufp %u, status %u, timestamp 0x%08X",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3], args[4]);
        break;

    case NAN_DBGID_TSF_TIMEOUT:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: tsfLo 0x%X, usecs %u",
            DBG_MSG_ARR[dbg_id], args[0], args[1]);
        break;

    case NAN_DBGID_SYNC_START:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: %s clusterId 0x%04x",
            DBG_MSG_ARR[dbg_id], (args[1] ?  "**STARTING**" : "**JOINING**"),
            args[0]);
        break;

    case NAN_DBGID_SYNC_STOP:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: stopping clusterId 0x%04x",
            DBG_MSG_ARR[dbg_id], args[0]);
        break;

    case NAN_DBGID_NAN_SCAN:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: enable %u, type %u, rval %d",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2]);
        break;

    case NAN_DBGID_NAN_SCAN_COMPLETE:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: scan type %u completed",
            DBG_MSG_ARR[dbg_id], args[0]);
        break;

    case NAN_DBGID_MPREF_CHANGE:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s: new value=%u",
            DBG_MSG_ARR[dbg_id], args[0]);
        break;

    case NAN_DBGID_WARMUP_EXPIRE:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s: masterPref=%u, randomFactor=%u",
            DBG_MSG_ARR[dbg_id], args[0], args[1]);
        break;

    case NAN_DBGID_RANDOM_FACTOR_EXPIRE:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s: new randomFactor=%u",
            DBG_MSG_ARR[dbg_id], args[0]);
        break;

    case NAN_DBGID_DW_SKIP:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: tsfLo 0x%08X, tsfDiff 0x%08X",
            DBG_MSG_ARR[dbg_id], args[0], args[1]);
        break;

    case NAN_DBGID_DB_SKIP:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: reason %x, tsfDiff 0x%08X",
            DBG_MSG_ARR[dbg_id], args[0], args[1]);
        break;

    case NAN_DBGID_BEACON_RX:
        /* args: TBD */
        if (numargs == 5)
        {
            A_UINT8 ctl = (args[0] >> 24) & 0xFF;
            if (ctl == 0)
            {
                A_UINT8 mac[6];

                mac[0] = args[0] & 0xFF;
                mac[1] = (args[0] >> 8) & 0xFF;
                mac[2] = args[1] & 0xFF;
                mac[3] = (args[1] >> 8) & 0xFF;
                mac[4] = (args[1] >> 16) & 0xFF;
                mac[5] = (args[1] >> 24) & 0xFF;

                dbglog_printf(timestamp, vap_id,
                    "NAN Beacon from %02x:%02x:%02x:%02x:%02x:%02x, clusterId 0x%04x, "
                    "RSSI %d, TS 0x%08lx_%08lx",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                    (args[2] >> 16) & 0xFFFF,
                    args[2] & 0xFF,
                    args[3],
                    args[4]);
            }
            else if (ctl == 1)
            {
                dbglog_printf(timestamp, vap_id,
                    " - Times: NAN TSF 0x%08lx_%08lx, hwRxTs 0x%08lx, statusFlags 0x%08lx",
                    args[1],
                    args[2],
                    args[3],
                    args[4]);
            }
            else if (ctl == 2)
            {
                dbglog_printf(timestamp, vap_id,
                    " - Attrs: [mpref=%d randf=%d], [amRank=0x%08lx_%08lx amHC=%d amBTT=%08lx]",
                    args[0] & 0xFF,
                    (args[0] >> 8) & 0xFF,
                    args[1],
                    args[2],
                    args[3],
                    args[4]);
            }
            else if (ctl == 3)
            {
                dbglog_printf(timestamp, vap_id,
                    " - AvgRssi: %u, numSamples=%u [%u %u %u %u %u %u %u %u %u %u %u %u %u %u %u %u]",
                    (args[0]) & 0xFF,
                    (args[0] >> 8) & 0xFF,
                    (args[1]) & 0xFF,
                    (args[1] >> 8) & 0xFF,
                    (args[1] >> 16) & 0xFF,
                    (args[1] >> 24) & 0xFF,
                    (args[2]) & 0xFF,
                    (args[2] >> 8) & 0xFF,
                    (args[2] >> 16) & 0xFF,
                    (args[2] >> 24) & 0xFF,
                    (args[3]) & 0xFF,
                    (args[3] >> 8) & 0xFF,
                    (args[3] >> 16) & 0xFF,
                    (args[3] >> 24) & 0xFF,
                    (args[4]) & 0xFF,
                    (args[4] >> 8) & 0xFF,
                    (args[4] >> 16) & 0xFF,
                    (args[4] >> 24) & 0xFF);
            }
            else
            {
                dbglog_printf(timestamp, vap_id, "%s, unknown ctl value: %d",
                    DBG_MSG_ARR[dbg_id], ctl);
                return FALSE;
            }
        }
        else
        {
            char str[64] = { 0 };
            A_UINT16 i;
            for (i=0; i < numargs; ++i)
            {
                snprintf(&str[i*9], sizeof(&str[i*9]), "%08X ", args[i]);
            }
            dbglog_printf(timestamp, vap_id, "%s: numargs %u, %s",
                DBG_MSG_ARR[dbg_id], numargs, str);
        }
        break;

    case NAN_DBGID_BEACON_TX:
        /* args: TBD */
        {
            char str[64] = { 0 };
            A_UINT16 i;
            for (i=0; i < numargs; ++i)
            {
                snprintf(&str[i*9], sizeof(&str[i*9]), "%08X ", args[i]);
            }
            dbglog_printf(timestamp, vap_id, "%s: numargs %u, %s",
                DBG_MSG_ARR[dbg_id], numargs, str);
        }
        break;

    case NAN_DBGID_CLUSTER_MERGE:
        if (numargs != 5)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s: **MERGING** to clusterId 0x%04x [amRank=0x%08lx_%08lx, TS=%08lx_%08lx]",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3], args[4]);
        break;

    case NAN_DBGID_TEST_CMD_EXEC:
        if (numargs == 1)
        {
            dbglog_printf(timestamp, vap_id, "%s: cmd %u",
                DBG_MSG_ARR[dbg_id], args[0]);
        }
        else if (numargs == 2)
        {
            dbglog_printf(timestamp, vap_id, "%s: cmd %u, status %d",
                DBG_MSG_ARR[dbg_id], args[0], args[1]);
        }
        else if (numargs == 3)
        {
            dbglog_printf(timestamp, vap_id, "%s: cmd %u, status %d, value %u",
                DBG_MSG_ARR[dbg_id], args[0], args[1], args[2]);
        }
        else
        {
            dbglog_printf(timestamp, vap_id, "%s: too many args %u",
                DBG_MSG_ARR[dbg_id], numargs);
            return FALSE;
        }
        break;

    case NAN_DBGID_APPLY_BEACON_TSF:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: timestamp=0x%08X_%08X, age=%08X",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2]);
        break;

    case NAN_DBGID_TSF_UPDATE:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: diff %c%d",
            DBG_MSG_ARR[dbg_id], args[0] ? '-' : '+', args[1]);
        break;

    case NAN_DBGID_SET_TSF:
        if (numargs == 4)
        {
            dbglog_printf(timestamp, vap_id,
                "%s: raw 0x%08X_%08X, nan 0x%08X_%08x",
                DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3]);
        }
        else if (numargs == 2)
        {
            dbglog_printf(timestamp, vap_id, "%s: offset 0x%08X_%08X",
                DBG_MSG_ARR[dbg_id], args[0], args[1]);
        }
        else
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        break;

    case NAN_DBGID_NEW_MASTERRANK:
        if (numargs != 4)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "%s: our rank is 0x%08X_%08x (mp=%d, rf=%d)",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3]);
        break;

    case NAN_DBGID_NEW_ANCHORMASTER:
        if (numargs != 4)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        {
            A_UINT8 mac[6];

            mac[0] = args[1] & 0xFF;
            mac[1] = (args[1] >> 8) & 0xFF;
            mac[2] = (args[1] >> 16) & 0xFF;
            mac[3] = (args[1] >> 24) & 0xFF;
            mac[4] = args[0] & 0xFF;
            mac[5] = (args[0] >> 8) & 0xFF;
            dbglog_printf(timestamp, vap_id,
                "%s: %02x:%02x:%02x:%02x:%02x:%02x, rank is 0x%08X_%08x (mp=%d, rf=%d)",
                DBG_MSG_ARR[dbg_id], mac[0], mac[1], mac[2], mac[3], mac[4],
                mac[5], args[0], args[1], args[2], args[3]);
        }
        break;

    case NAN_DBGID_ANCHORMASTER_RECORD_UPDATE:
        if (numargs == 4)
        {
            dbglog_printf(timestamp, vap_id,
                "%s: rank=0x%08X_%08x HC=%d BTT=0x%08X",
                DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3]);
        }
        else if (numargs == 3)
        {
            dbglog_printf(timestamp, vap_id,
                "%s: rank=0x%08X_%08x BTT=0x%08X (*Last*)",
                DBG_MSG_ARR[dbg_id], args[0], args[1], args[2]);
        }
        else
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        break;

    case NAN_DBGID_ANCHORMASTER_RECORD_EXPIRED:
        if (numargs == 4)
        {
            dbglog_printf(timestamp, vap_id,
                "%s: rank=0x%08X_%08x HC=%d BTT=0x%08X",
                DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3]);
        }
        else if (numargs == 5)
        {
            dbglog_printf(timestamp, vap_id,
                "%s: rank=0x%08X_%08x HC=%d BTT=0x%08X ==> Setting HC to %d",
                DBG_MSG_ARR[dbg_id], args[0], args[1], args[2], args[3], args[4]);
        }
        else
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        break;

    case NAN_DBGID_BECOMING_ANCHORMASTER:
        if (numargs != 2)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: reason=%d, transitionsToAM=%d",
            DBG_MSG_ARR[dbg_id], args[0], args[1]);
        break;

    case NAN_DBGID_ROLE_CHANGE:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: oldRole=%s, newRole=%s %s",
            DBG_MSG_ARR[dbg_id],
            dbglog_get_nan_role_str(args[0]),
            dbglog_get_nan_role_str(args[1]),
            args[2] ? "(**FORCED**)" : "");
        break;

    case NAN_DBGID_SYNC_BEACON_DW_STATS:
        if (numargs != 4)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        {
            A_UINT16 c1_close, c1_middle, c1_poor;
            A_UINT16 c2_close, c2_middle, c2_poor;
            c1_close = args[1] >> 16,
            c2_close = args[1] & 0xFFFF,
            c1_middle = args[2] >> 16,
            c2_middle = args[2] & 0xFFFF,
            c1_poor = args[3] >> 16,
            c2_poor = args[3] & 0xFFFF,

            dbglog_printf(timestamp, vap_id,
                "%s: total rx %d, c1=(%d/%d/%d) c1=(%d/%d/%d)",
                DBG_MSG_ARR[dbg_id], args[0], c1_close, c1_middle, c1_poor,
                                              c2_close, c2_middle, c2_poor);
        }
        break;

    case NAN_DBGID_RX_UNSUPPORTED_SDF_ATTR_ID:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s %u",
            DBG_MSG_ARR[dbg_id], args[0]);
        break;

    case NAN_DBGID_PUBSUB_MATCHED_SKIPPED_SSI:
        if (numargs != 5)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg(s)",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id,
            "NAN %s %u matched and skipped because SSI is required, "
            "instance id %u, requestor id %u, "
            "svc id %02X:%02X:%02X:%02X:%02X:%02X, svcCtrl 0x%02X, len%u",
            PUBSUB_STR(args[0]), args[0],
            (args[2] & 0xFF), ((args[2] >> 8) && 0xFF),
            ((args[1] >> 24) & 0xFF), ((args[1] >> 16) & 0xFF),
            ((args[1] >> 8) & 0xFF), (args[1] & 0xFF),
            ((args[2] >> 24) & 0xFF), ((args[2] >> 16) & 0xFF),
            args[3], args[4]);
        break;

    case NAN_DBGID_MATCH_FILTER_OFFSET:
        if (numargs != 1)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s %u",
            DBG_MSG_ARR[dbg_id], args[0]);
        break;

    case NAN_DBGID_TW_PARAMS:
        if (numargs != 3)
        {
            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        dbglog_printf(timestamp, vap_id, "%s: twSize=%u, n=%u, twIndex=%u",
            DBG_MSG_ARR[dbg_id], args[0], args[1], args[2]);
        break;

    case NAN_DBGID_BEACON_SENDER:
        if (numargs == 1)
        {
            dbglog_printf(timestamp, vap_id, "%s: failed, reason %x",
                DBG_MSG_ARR[dbg_id], args[0]);
        }
        else if (numargs == 3)
        {
            dbglog_printf(timestamp, vap_id, "%s: %s idx=%u sender 0x%08x",
                DBG_MSG_ARR[dbg_id], args[0] ? "alloc" : "reclaim", args[1], args[2]);
        }
        else
        {

            dbglog_printf(timestamp, vap_id, "%s, missing arg",
                DBG_MSG_ARR[dbg_id]);
            return FALSE;
        }
        break;

    default:
        dbglog_printf(timestamp, vap_id,
            "NAN unknown event id %u (should never reach this case)", dbg_id);
        return FALSE;
    }

    return TRUE;
}

/* NAN message IDs */
typedef enum
{
    NAN_MSG_ID_ERROR_RSP                    = 0,
    NAN_MSG_ID_CONFIGURATION_REQ            = 1,
    NAN_MSG_ID_CONFIGURATION_RSP            = 2,
    NAN_MSG_ID_PUBLISH_SERVICE_REQ          = 3,
    NAN_MSG_ID_PUBLISH_SERVICE_RSP          = 4,
    NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_REQ   = 5,
    NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_RSP   = 6,
    NAN_MSG_ID_PUBLISH_REPLIED_IND          = 7,
    NAN_MSG_ID_PUBLISH_TERMINATED_IND       = 8,
    NAN_MSG_ID_SUBSCRIBE_SERVICE_REQ        = 9,
    NAN_MSG_ID_SUBSCRIBE_SERVICE_RSP        = 10,
    NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_REQ = 11,
    NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_RSP = 12,
    NAN_MSG_ID_SUBSCRIBE_MATCH_IND          = 13,
    NAN_MSG_ID_SUBSCRIBE_UNMATCH_IND        = 14,
    NAN_MSG_ID_SUBSCRIBE_TERMINATED_IND     = 15,
    NAN_MSG_ID_DE_EVENT_IND                 = 16,
    NAN_MSG_ID_TRANSMIT_FOLLOWUP_REQ        = 17,
    NAN_MSG_ID_TRANSMIT_FOLLOWUP_RSP        = 18,
    NAN_MSG_ID_FOLLOWUP_IND                 = 19,
    NAN_MSG_ID_STATS_REQ                    = 20,
    NAN_MSG_ID_STATS_RSP                    = 21,
    NAN_MSG_ID_ENABLE_REQ                   = 22,
    NAN_MSG_ID_ENABLE_RSP                   = 23,
    NAN_MSG_ID_DISABLE_REQ                  = 24,
    NAN_MSG_ID_DISABLE_RSP                  = 25,
    NAN_MSG_ID_DISABLE_IND                  = 26,
    NAN_MSG_ID_TCA_REQ                      = 27,
    NAN_MSG_ID_TCA_RSP                      = 28,
    NAN_MSG_ID_TCA_IND                      = 29,
    NAN_MSG_ID_BCN_SDF_PAYLOAD_REQ          = 30,
    NAN_MSG_ID_BCN_SDF_PAYLOAD_RSP          = 31,
    NAN_MSG_ID_BCN_SDF_PAYLOAD_IND          = 32,
    NAN_MSG_ID_LAST
} tNanMsgId;

/* NAN Event ID Codes */
typedef enum
{
    NAN_EVENT_ID_FIRST = 0,
    NAN_EVENT_ID_SELF_STA_MAC_ADDR = NAN_EVENT_ID_FIRST,
    NAN_EVENT_ID_STARTED_CLUSTER,
    NAN_EVENT_ID_JOINED_CLUSTER,
    NAN_EVENT_ID_LAST
} tNanEventId;

/* NAN Statistics Request ID Codes */
typedef enum
{
    NAN_STATS_ID_FIRST = 0,
    NAN_STATS_ID_DE_PUBLISH = NAN_STATS_ID_FIRST,
    NAN_STATS_ID_DE_SUBSCRIBE,
    NAN_STATS_ID_DE_MAC,
    NAN_STATS_ID_DE_TIMING_SYNC,
    NAN_STATS_ID_DE_DW,
    NAN_STATS_ID_DE,
    NAN_STATS_ID_LAST
} tNanStatsId;

typedef enum
{
    NAN_TLV_TYPE_FIRST = 0,

    /* Service Discovery Frame types */
    NAN_TLV_TYPE_SDF_FIRST = NAN_TLV_TYPE_FIRST,
    NAN_TLV_TYPE_SERVICE_NAME = NAN_TLV_TYPE_SDF_FIRST,
    NAN_TLV_TYPE_SDF_MATCH_FILTER,
    NAN_TLV_TYPE_TX_MATCH_FILTER,
    NAN_TLV_TYPE_RX_MATCH_FILTER,
    NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO,
    NAN_TLV_TYPE_EXT_SERVICE_SPECIFIC_INFO,
    NAN_TLV_TYPE_VENDOR_SPECIFIC_ATTRIBUTE_TRANSMIT,
    NAN_TLV_TYPE_VENDOR_SPECIFIC_ATTRIBUTE_RECEIVE,
    NAN_TLV_TYPE_POST_NAN_CONNECTIVITY_CAPABILITIES_RECEIVE,
    NAN_TLV_TYPE_POST_NAN_DISCOVERY_ATTRIBUTE_RECEIVE,
    NAN_TLV_TYPE_BEACON_SDF_PAYLOAD_RECEIVE,
    NAN_TLV_TYPE_SDF_LAST = 4095,

    /* Configuration types */
    NAN_TLV_TYPE_CONFIG_FIRST = 4096,
    NAN_TLV_TYPE_24G_SUPPORT = NAN_TLV_TYPE_CONFIG_FIRST,
    NAN_TLV_TYPE_24G_BEACON,
    NAN_TLV_TYPE_24G_SDF,
    NAN_TLV_TYPE_24G_RSSI_CLOSE,
    NAN_TLV_TYPE_24G_RSSI_MIDDLE,
    NAN_TLV_TYPE_24G_RSSI_CLOSE_PROXIMITY,
    NAN_TLV_TYPE_5G_SUPPORT,
    NAN_TLV_TYPE_5G_BEACON,
    NAN_TLV_TYPE_5G_SDF,
    NAN_TLV_TYPE_5G_RSSI_CLOSE,
    NAN_TLV_TYPE_5G_RSSI_MIDDLE,
    NAN_TLV_TYPE_5G_RSSI_CLOSE_PROXIMITY,
    NAN_TLV_TYPE_SID_BEACON,
    NAN_TLV_TYPE_HOP_COUNT_LIMIT,
    NAN_TLV_TYPE_MASTER_PREFERENCE,
    NAN_TLV_TYPE_CLUSTER_ID_LOW,
    NAN_TLV_TYPE_CLUSTER_ID_HIGH,
    NAN_TLV_TYPE_RSSI_AVERAGING_WINDOW_SIZE,
    NAN_TLV_TYPE_CLUSTER_OUI_NETWORK_ID,
    NAN_TLV_TYPE_SOURCE_MAC_ADDRESS,
    NAN_TLV_TYPE_CLUSTER_ATTRIBUTE_IN_SDF,
    NAN_TLV_TYPE_SOCIAL_CHANNEL_SCAN_PARAMS,
    NAN_TLV_TYPE_DEBUGGING_FLAGS,
    NAN_TLV_TYPE_POST_NAN_CONNECTIVITY_CAPABILITIES_TRANSMIT,
    NAN_TLV_TYPE_POST_NAN_DISCOVERY_ATTRIBUTE_TRANSMIT,
    NAN_TLV_TYPE_FURTHER_AVAILABILITY_MAP,
    NAN_TLV_TYPE_HOP_COUNT_FORCE,
    NAN_TLV_TYPE_RANDOM_FACTOR_FORCE,
    NAN_TLV_TYPE_CONFIG_LAST = 8191,


    /* Attributes types */
    NAN_TLV_TYPE_ATTRS_FIRST = 8192,
    NAN_TLV_TYPE_AVAILABILITY_INTERVALS_MAP = NAN_TLV_TYPE_ATTRS_FIRST,
    NAN_TLV_TYPE_WLAN_MESH_ID,
    NAN_TLV_TYPE_MAC_ADDRESS,
    NAN_TLV_TYPE_RECEIVED_RSSI_VALUE,
    NAN_TLV_TYPE_CLUSTER_ATTRIBUTE,
    NAN_TLV_TYPE_ATTRS_LAST = 12287,

    /* Event types */
    NAN_TLV_TYPE_EVENTS_FIRST = 12288,
    NAN_TLV_TYPE_EVENT_SELF_STATION_MAC_ADDRESS = NAN_TLV_TYPE_EVENTS_FIRST,
    NAN_TLV_TYPE_EVENT_STARTED_CLUSTER,
    NAN_TLV_TYPE_EVENT_JOINED_CLUSTER,
    NAN_TLV_TYPE_EVENT_CLUSTER_SCAN_RESULTS,
    NAN_TLV_TYPE_EVENTS_LAST = 16383,

    /* Thrshold Cross Alerts types */
    NAN_TLV_TYPE_TCA_FIRST = 16384,
    NAN_TLV_TYPE_CLUSTER_SIZE_REQ = NAN_TLV_TYPE_TCA_FIRST,
    NAN_TLV_TYPE_CLUSTER_SIZE_RSP,
    NAN_TLV_TYPE_TCA_LAST = 20479,

    /* Statistics types */
    NAN_TLV_TYPE_STATS_FIRST = 32768,
    NAN_TLV_TYPE_DE_PUBLISH_STATS = NAN_TLV_TYPE_STATS_FIRST,
    NAN_TLV_TYPE_DE_SUBSCRIBE_STATS,
    NAN_TLV_TYPE_DE_MAC_STATS,
    NAN_TLV_TYPE_DE_TIMING_SYNC_STATS,
    NAN_TLV_TYPE_DE_DW_STATS,
    NAN_TLV_TYPE_DE_STATS,
    NAN_TLV_TYPE_STATS_LAST,

    NAN_TLV_TYPE_LAST = 65535
} tNanTlvType;

/* NAN Miscellaneous Constants */
#define NAN_TTL_INFINITE            0
#define NAN_REPLY_COUNT_INFINITE    0

/* NAN Publish Types */
typedef enum
{
    NAN_PUBLISH_TYPE_UNSOLICITED = 0,
    NAN_PUBLISH_TYPE_SOLICITED,
    NAN_PUBLISH_TYPE_UNSOLICITED_SOLICITED,
    NAN_PUBLISH_TYPE_LAST,
} tNanPublishType;

/* NAN Transmit Types */
typedef enum
{
    NAN_TX_TYPE_BROADCAST = 0,
    NAN_TX_TYPE_UNICAST,
    NAN_TX_TYPE_LAST
} tNanTxType;

/* NAN Subscribe Type Bit */
#define NAN_SUBSCRIBE_TYPE_PASSIVE  0
#define NAN_SUBSCRIBE_TYPE_ACTIVE   1

/* NAN Service Response Filter Attribute Bit */
#define NAN_SRF_ATTR_BLOOM_FILTER       0
#define NAN_SRF_ATTR_PARTIAL_MAC_ADDR   1

/* NAN Service Response Filter Include Bit */
#define NAN_SRF_INCLUDE_DO_NOT_RESPOND  0
#define NAN_SRF_INCLUDE_RESPOND         1

/* NAN Match Algorithms */
typedef enum
{
    NAN_MATCH_ALG_FIRST = 0,
    NAN_MATCH_ALG_MATCH_ONCE = NAN_MATCH_ALG_FIRST,
    NAN_MATCH_ALG_MATCH_CONTINUOUS,
    NAN_MATCH_ALG_LAST
} tNanMatchAlg;


/* NAN Transmit Priorities */
typedef enum
{
    NAN_TX_PRIORITY_LOW = 0,
    NAN_TX_PRIORITY_NORMAL,
    NAN_TX_PRIORITY_HIGH,
    NAN_TX_PRIORITY_LAST
} tNanTxPriority;

/* NAN TLV Maximum Lengths */
#define NAN_MAX_SERVICE_NAME_LEN                255
#define NAN_MAX_MATCH_FILTER_LEN                255
#define NAN_MAX_SERVICE_SPECIFIC_INFO_LEN       255
#define NAN_MAX_GROUP_KEY_LEN                   32
#define NAN_MAX_EXT_SERVICE_SPECIFIC_INFO_LEN   1024
#define NAN_MAX_TLV_LEN     NAN_MAX_EXT_SERVICE_SPECIFIC_INFO_LEN

/* NAN Confguration 5G Channel Access Bit */
#define NAN_5G_CHANNEL_ACCESS_UNSUPPORTED   0
#define NAN_5G_CHANNEL_ACCESS_SUPPORTED     1

/* NAN Configuration Service IDs Enclosure Bit */
#define NAN_SIDS_NOT_ENCLOSED_IN_BEACONS    0
#define NAN_SIBS_ENCLOSED_IN_BECAONS        1

/* NAN Configuration Priority */
#define NAN_CFG_PRIORITY_SERVICE_DISCOVERY  0
#define NAN_CFG_PRIORITY_DATA_CONNECTION    1

/* NAN Configuration 5G Channel Usage */
#define NAN_5G_CHANNEL_USAGE_SYNC_AND_DISCOVERY 0
#define NAN_5G_CHANNEL_USAGE_DISCOVERY_ONLY     1

/* NAN Configuration TX_Beacon Content */
#define NAN_TX_BEACON_CONTENT_OLD_AM_INFO       0
#define NAN_TX_BEACON_CONTENT_UPDATED_AM_INFO   1

/* NAN Configuration Miscellaneous Constants */
#define NAN_MAC_INTERFACE_PERIODICITY_MIN   30
#define NAN_MAC_INTERFACE_PERIODICITY_MAX   255

#define NAN_DW_RANDOM_TIME_MIN  120
#define NAN_DW_RANDOM_TIME_MAX  240

#define NAN_INITIAL_SCAN_MIN_IDEAL_PERIOD   200
#define NAN_INITIAL_SCAN_MAX_IDEAL_PERIOD   300

#define NAN_ONGOING_SCAN_MIN_PERIOD 10
#define NAN_ONGOING_SCAN_MAX_PERIOD 30

#define NAN_HOP_COUNT_LIMIT 5

#define NAN_WINDOW_DW   0
#define NAN_WINDOW_FAW  1

/* TCA IDs */
typedef enum
{
    NAN_TCA_ID_FIRST = 0,
    NAN_TCA_ID_CLUSTER_SIZE = NAN_TCA_ID_FIRST,
    NAN_TCA_ID_LAST
} tNanTcaId;

/* NAN Status Codes */
typedef enum
{
    NAN_STATUS_SUCCESS = 0,
    NAN_STATUS_TIMEOUT,
    NAN_STATUS_DE_FAILURE,
    NAN_STATUS_INVALID_MSG_VERSION,
    NAN_STATUS_INVALID_MSG_LEN,
    NAN_STATUS_INVALID_MSG_ID,
    NAN_STATUS_INVALID_HANDLE,
    NAN_STATUS_NO_SPACE_AVAILABLE,
    NAN_STATUS_INVALID_PUBLISH_TYPE,
    NAN_STATUS_INVALID_TX_TYPE,
    NAN_STATUS_INVALID_MATCH_ALGORITHM,
    NAN_STATUS_DISABLE_IN_PROGRESS,
    NAN_STATUS_INVALID_TLV_LEN,
    NAN_STATUS_INVALID_TLV_TYPE,
    NAN_STATUS_MISSING_TLV_TYPE,
    NAN_STATUS_INVALID_TOTAL_TLVS_LEN,
    NAN_STATUS_INVALID_MATCH_HANDLE,
    NAN_STATUS_INVALID_TLV_VALUE,
    NAN_STATUS_INVALID_TX_PRIORITY,
    NAN_STATUS_INVALID_CONN_MAP,

    /* Config */
    NAN_STATUS_INVALID_RSSI_CLOSE_VALUE = 4096,
    NAN_STATUS_INVALID_RSSI_MIDDLE_VALUE,
    NAN_STATUS_INVALID_HOP_COUNT_LIMIT,
    NAN_STATUS_INVALID_MASTER_PREFERENCE_VALUE,
    NAN_STATUS_INVALID_LOW_CLUSTER_ID_VALUE,
    NAN_STATUS_INVALID_HIGH_CLUSTER_ID_VALUE,
    NAN_STATUS_INVALID_BACKGROUND_SCAN_PERIOD,
    NAN_STATUS_INVALID_RSSI_PROXIMITY_VALUE,
    NAN_STATUS_INVALID_SCAN_CHANNEL,
    NAN_STATUS_INVALID_POST_NAN_CONN_CAP_BITMAP,
    NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_NUM_CHAN,
    NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_AVAIL_INT_DURATION,
    NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_CLASS,
    NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_CHANNEL,
    NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_AVAIL_INT_BITMAP,
    NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_MAP_ID,
    NAN_STATUS_INVALID_POST_DISC_CONN_TYPE,
    NAN_STATUS_INVALID_POST_DISC_DEVICE_ROLE,
    NAN_STATUS_INVALID_POST_DISC_AVAIL_INT_DURATION,
    NAN_STATUS_MISSING_FURTHER_AVAIL_MAP,

    /* Terminated reasons */
    NAN_TERMINATED_REASON_INVALID = 8192,
    NAN_TERMINATED_REASON_TIMEOUT,
    NAN_TERMINATED_REASON_USER_REQUEST,
    NAN_TERMINATED_REASON_FAILURE,
    NAN_TERMINATED_REASON_COUNT_REACHED,
    NAN_TERMINATED_REASON_DE_SHUTDOWN,
    NAN_TERMINATED_REASON_DISABLE_IN_PROGRESS,
    NAN_TERMINATED_REASON_POST_DISC_ATTR_EXPIRED,
    NAN_TERMINATED_REASON_POST_DISC_LEN_EXCEEDED,
    NAN_TERMINATED_REASON_FURTHER_AVAIL_MAP_EMPTY,

    NAN_STATUS_LAST
} tNanStatusType;

typedef enum
{
    NAN_ROLE_INVALID = 0,
    NAN_ROLE_NON_MASTER_NON_SYNC = 1,
    NAN_ROLE_NON_MASTER_SYNC = 2,
    NAN_ROLE_MASTER = 3
} tNanRole;

static const char *
dbglog_get_nan_msg_id_str(
    A_UINT16 msg_id)
{
    switch (msg_id)
    {
    case NAN_MSG_ID_ERROR_RSP:
        return "Error Rsp";
    case NAN_MSG_ID_CONFIGURATION_REQ:
        return "Configuration Req";
    case NAN_MSG_ID_CONFIGURATION_RSP:
        return "Configuration Rsp";
    case NAN_MSG_ID_PUBLISH_SERVICE_REQ:
        return "Publish Service Req";
    case NAN_MSG_ID_PUBLISH_SERVICE_RSP:
        return "Publish Service Rsp";
    case NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_REQ:
        return "Publish Service Cancel Req";
    case NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_RSP:
        return "Publish Service Cancel Rsp";
    case NAN_MSG_ID_PUBLISH_REPLIED_IND:
        return "Publish Replied Ind";
    case NAN_MSG_ID_PUBLISH_TERMINATED_IND:
        return "Publish Terminated Ind";
    case NAN_MSG_ID_SUBSCRIBE_SERVICE_REQ:
        return "Subscribe Service Req";
    case NAN_MSG_ID_SUBSCRIBE_SERVICE_RSP:
        return "Subscribe Service Rsp";
    case NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_REQ:
        return "Subscribe Service Cancel Req";
    case NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_RSP:
        return "Subscribe Service Cancel Rsp";
    case NAN_MSG_ID_SUBSCRIBE_MATCH_IND:
        return "Subscribe Match Ind";
    case NAN_MSG_ID_SUBSCRIBE_UNMATCH_IND:
        return "Subscribe Unmatch Ind";
    case NAN_MSG_ID_SUBSCRIBE_TERMINATED_IND:
        return "Subscribe Terminated Ind";
    case NAN_MSG_ID_DE_EVENT_IND:
        return "DE Event Ind";
    case NAN_MSG_ID_TRANSMIT_FOLLOWUP_REQ:
        return "Transmit Followup Req";
    case NAN_MSG_ID_TRANSMIT_FOLLOWUP_RSP:
        return "Transmit Followup Rsp";
    case NAN_MSG_ID_FOLLOWUP_IND:
        return "Followup Ind";
    case NAN_MSG_ID_STATS_REQ:
        return "Stats Req";
    case NAN_MSG_ID_STATS_RSP:
        return "Stats Rsp";
    case NAN_MSG_ID_ENABLE_REQ:
        return "Enable Req";
    case NAN_MSG_ID_ENABLE_RSP:
        return "Enable Rsp";
    case NAN_MSG_ID_DISABLE_REQ:
        return "Disable Req";
    case NAN_MSG_ID_DISABLE_RSP:
        return "Disable Rsp";
    case NAN_MSG_ID_DISABLE_IND:
        return "Disable Ind";
    case NAN_MSG_ID_TCA_REQ:
        return "TCA Req";
    case NAN_MSG_ID_TCA_RSP:
        return "TCA Rsp";
    case NAN_MSG_ID_TCA_IND:
        return "TCA Ind";
    case NAN_MSG_ID_BCN_SDF_PAYLOAD_REQ:
        return "BCN/SDF Payload Req";
    case NAN_MSG_ID_BCN_SDF_PAYLOAD_RSP:
        return "BCN/SDF Payload Rsp";
    case NAN_MSG_ID_BCN_SDF_PAYLOAD_IND:
        return "BCN/SDF Payload Ind";
    default:
        return "Unknown Msg";
    }
}

static const char *
dbglog_get_nan_status_str(
    A_UINT32 status)
{
    switch (status)
    {
    case NAN_STATUS_SUCCESS:
        return "Success";
    case NAN_STATUS_TIMEOUT:
        return "Timeout";
    case NAN_STATUS_DE_FAILURE:
        return "DE Failure";
    case NAN_STATUS_INVALID_MSG_VERSION:
        return "Invalid Msg Version";
    case NAN_STATUS_INVALID_MSG_LEN:
        return "Invalid Msg Len";
    case NAN_STATUS_INVALID_MSG_ID:
        return "Invalid Msg Id";
    case NAN_STATUS_INVALID_HANDLE:
        return "Invalid Handle";
    case NAN_STATUS_NO_SPACE_AVAILABLE:
        return "No Space Available";
    case NAN_STATUS_INVALID_PUBLISH_TYPE:
        return "Invalid Publish Type";
    case NAN_STATUS_INVALID_TX_TYPE:
        return "Invalid Tx Type";
    case NAN_STATUS_INVALID_MATCH_ALGORITHM:
        return "Invalid Match Algorithm";
    case NAN_STATUS_DISABLE_IN_PROGRESS:
        return "Disable In Progress";
    case NAN_STATUS_INVALID_TLV_LEN:
        return "Invalid TLV Len";
    case NAN_STATUS_INVALID_TLV_TYPE:
        return "Invalid TLV Type";
    case NAN_STATUS_MISSING_TLV_TYPE:
        return "Missing TLV Type";
    case NAN_STATUS_INVALID_TOTAL_TLVS_LEN:
        return "Invalid Total TLVs Len";
    case NAN_STATUS_INVALID_TLV_VALUE:
        return "Invalid TLV Value";
    case NAN_STATUS_INVALID_MATCH_HANDLE:
        return "Invalid Match Handle";
    case NAN_STATUS_INVALID_TX_PRIORITY:
        return "Invalid Tx Priority";
    case NAN_STATUS_INVALID_CONN_MAP:
        return "Invalid Connection Capability Map";

    /* Config */
    case NAN_STATUS_INVALID_RSSI_CLOSE_VALUE:
        return "Invalid RSSI Close Value";
    case NAN_STATUS_INVALID_RSSI_MIDDLE_VALUE:
        return "Invalid RSSI Middle Value";
    case NAN_STATUS_INVALID_HOP_COUNT_LIMIT:
        return "Hop Count Limit";
    case NAN_STATUS_INVALID_MASTER_PREFERENCE_VALUE:
        return "Invalid Master Preference Value";
    case NAN_STATUS_INVALID_LOW_CLUSTER_ID_VALUE:
        return "Invalid Low Cluster ID Value";
    case NAN_STATUS_INVALID_HIGH_CLUSTER_ID_VALUE:
        return "Invalid High Cluster ID Value";
    case NAN_STATUS_INVALID_BACKGROUND_SCAN_PERIOD:
        return "Invalid Background Scan Period";
    case NAN_STATUS_INVALID_RSSI_PROXIMITY_VALUE:
        return "Invalid RSSI Proximity Value";
    case NAN_STATUS_INVALID_SCAN_CHANNEL:
        return "Invalid Scan Channel";
    case NAN_STATUS_INVALID_POST_NAN_CONN_CAP_BITMAP:
        return "Invalid Connection Capability Bitmap";
    case NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_NUM_CHAN:
        return "Invalid Further Availability Map Num Channels";
    case NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_AVAIL_INT_DURATION:
        return "Invalid Further Availability Map Availability Interval Duration";
    case NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_CLASS:
        return "Invalid Further Availability Map Class";
    case NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_CHANNEL:
        return "Invalid Further Availability Map Channel";
    case NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_AVAIL_INT_BITMAP:
        return "Invalid Further Availability Map Bitmap";
    case NAN_STATUS_INVALID_FURTHER_AVAIL_MAP_MAP_ID:
        return "Invalid Further Availability Map Map_ID";
    case NAN_STATUS_INVALID_POST_DISC_CONN_TYPE:
        return "Invalid Post-Discovery Connection Type";
    case NAN_STATUS_INVALID_POST_DISC_DEVICE_ROLE:
        return "Invalid Post-Discovery Device Role";
    case NAN_STATUS_INVALID_POST_DISC_AVAIL_INT_DURATION:
        return "Invalid Post-Discovery Availability Interval Duration";
    case NAN_STATUS_MISSING_FURTHER_AVAIL_MAP:
        return "Missing Further Availability Map";

    /* Terminated reasons */
    case NAN_TERMINATED_REASON_INVALID:
        return "Invalid";
    case NAN_TERMINATED_REASON_TIMEOUT:
        return "Timeout";
    case NAN_TERMINATED_REASON_USER_REQUEST:
        return "User Request";
    case NAN_TERMINATED_REASON_FAILURE:
        return "Failure";
    case NAN_TERMINATED_REASON_COUNT_REACHED:
        return "Count Reached";
    case NAN_TERMINATED_REASON_DE_SHUTDOWN:
        return "DE Shutdown";
    case NAN_TERMINATED_REASON_DISABLE_IN_PROGRESS:
        return "Disable In Progress";
    case NAN_TERMINATED_REASON_POST_DISC_ATTR_EXPIRED:
        return "Post-Discovery Attribute Expired";
    case NAN_TERMINATED_REASON_POST_DISC_LEN_EXCEEDED:
        return "Post-Discovery Attribute Length Exceeded";
    case NAN_TERMINATED_REASON_FURTHER_AVAIL_MAP_EMPTY:
        return "Further Availability Map Empty";
    default:
        return "Unknown";
    }
}

static const char *
dbglog_get_nan_event_id_str(
    A_UINT8 event_id)
{
    switch (event_id)
    {
    case NAN_EVENT_ID_SELF_STA_MAC_ADDR:
        return "Self-Station MAC Addr";
    case NAN_EVENT_ID_STARTED_CLUSTER:
        return "Started Cluster";
    case NAN_EVENT_ID_JOINED_CLUSTER:
        return "Joined Cluster";
    default:
        return "Unknown";
    }
}

const char *
dbglog_get_nan_stats_id_str(
    A_UINT8 stats_id)
{
    switch (stats_id)
    {
    case NAN_STATS_ID_DE_PUBLISH:
        return "DE Publish";
    case NAN_STATS_ID_DE_SUBSCRIBE:
        return "DE Subscribe";
    case NAN_STATS_ID_DE_MAC:
        return "DE MAC";
    case NAN_STATS_ID_DE_TIMING_SYNC:
        return "DE Timing Sync";
    case NAN_STATS_ID_DE_DW:
        return "DE DW";
    case NAN_STATS_ID_DE:
        return "DE";
    default:
        return "Unknown";
    }
}

static const char *
dbglog_get_nan_tlv_type_str(
    A_UINT16 tlv_type)
{
    switch (tlv_type)
    {
    case NAN_TLV_TYPE_SERVICE_NAME:
        return "Service Name";
    case NAN_TLV_TYPE_SDF_MATCH_FILTER:
        return "SDF Match Filter";
    case NAN_TLV_TYPE_TX_MATCH_FILTER:
        return "Tx Match Filter";
    case NAN_TLV_TYPE_RX_MATCH_FILTER:
        return "Rx Match Filter";
    case NAN_TLV_TYPE_SERVICE_SPECIFIC_INFO:
        return "Service Specific Info";
    case NAN_TLV_TYPE_EXT_SERVICE_SPECIFIC_INFO:
        return "Extended Service Specific Info";
    case NAN_TLV_TYPE_VENDOR_SPECIFIC_ATTRIBUTE_TRANSMIT:
        return "VSA Transmit";
    case NAN_TLV_TYPE_VENDOR_SPECIFIC_ATTRIBUTE_RECEIVE:
        return "VSA Receive";
    case NAN_TLV_TYPE_POST_NAN_CONNECTIVITY_CAPABILITIES_RECEIVE:
        return "Post-NAN Capabilities Receive";
    case NAN_TLV_TYPE_POST_NAN_DISCOVERY_ATTRIBUTE_RECEIVE:
        return "Post-NAN Discovery Receive";
    case NAN_TLV_TYPE_BEACON_SDF_PAYLOAD_RECEIVE:
        return "Bcn/SDF Payload Receive";

    // Config TLVs
    case NAN_TLV_TYPE_24G_SUPPORT:
        return "2.4G Support Enabled";
    case NAN_TLV_TYPE_24G_BEACON:
        return "2.4G Beacon Support";
    case NAN_TLV_TYPE_24G_SDF:
        return "2.4G SDF Support";
    case NAN_TLV_TYPE_24G_RSSI_CLOSE:
        return "2.4G RSSI Close";
    case NAN_TLV_TYPE_24G_RSSI_MIDDLE:
        return "2.4G RSSI Middle";
    case NAN_TLV_TYPE_24G_RSSI_CLOSE_PROXIMITY:
        return "2.4G RSSI Close Proximity";
    case NAN_TLV_TYPE_5G_SUPPORT:
        return "5G Support Enabled";
    case NAN_TLV_TYPE_5G_BEACON:
        return "5G Beacon Support";
    case NAN_TLV_TYPE_5G_SDF:
        return "5G SDF Support";
    case NAN_TLV_TYPE_5G_RSSI_CLOSE:
        return "5G RSSI Close";
    case NAN_TLV_TYPE_5G_RSSI_MIDDLE:
        return "5G RSSI Middle";
    case NAN_TLV_TYPE_5G_RSSI_CLOSE_PROXIMITY:
        return "5G RSSI Close Proximity";
    case NAN_TLV_TYPE_SID_BEACON:
        return "SID Beacon";
    case NAN_TLV_TYPE_HOP_COUNT_LIMIT:
        return "Hop Count Limit";
    case NAN_TLV_TYPE_MASTER_PREFERENCE:
        return "Master Preference";
    case NAN_TLV_TYPE_CLUSTER_ID_LOW:
        return "Cluster ID Low";
    case NAN_TLV_TYPE_CLUSTER_ID_HIGH:
        return "Cluster ID High";
    case NAN_TLV_TYPE_RSSI_AVERAGING_WINDOW_SIZE:
        return "RSSI Averaging Window Size";
    case NAN_TLV_TYPE_CLUSTER_OUI_NETWORK_ID:
        return "Cluster OUI NetworkID";
    case NAN_TLV_TYPE_SOURCE_MAC_ADDRESS:
        return "Source MAC Addr";
    case NAN_TLV_TYPE_CLUSTER_ATTRIBUTE_IN_SDF:
        return "Cluster Attribute in SDF";
    case NAN_TLV_TYPE_SOCIAL_CHANNEL_SCAN_PARAMS:
        return "Social Channel Scan Params";
    case NAN_TLV_TYPE_DEBUGGING_FLAGS:
        return "Debugging Flags";
    case NAN_TLV_TYPE_POST_NAN_CONNECTIVITY_CAPABILITIES_TRANSMIT:
        return "Post-NAN Capabilities Transmit";
    case NAN_TLV_TYPE_POST_NAN_DISCOVERY_ATTRIBUTE_TRANSMIT:
        return "Post-NAN Discovery Transmit";
    case NAN_TLV_TYPE_FURTHER_AVAILABILITY_MAP:
        return "Further Availability Map";
    case NAN_TLV_TYPE_HOP_COUNT_FORCE:
        return "Hop Count Force";
    case NAN_TLV_TYPE_RANDOM_FACTOR_FORCE:
        return "Random Factor Force";


    case NAN_TLV_TYPE_AVAILABILITY_INTERVALS_MAP:
        return "Availability Intervals Map";
    case NAN_TLV_TYPE_WLAN_MESH_ID:
        return "WLAN Mesh ID";
    case NAN_TLV_TYPE_MAC_ADDRESS:
        return "MAC Addr";
    case NAN_TLV_TYPE_RECEIVED_RSSI_VALUE:
        return "Recevied RSSI Value";
    case NAN_TLV_TYPE_CLUSTER_ATTRIBUTE:
        return "Cluster Attr";

    case NAN_TLV_TYPE_EVENT_SELF_STATION_MAC_ADDRESS:
        return "Self MAC Addr";
    case NAN_TLV_TYPE_EVENT_STARTED_CLUSTER:
        return "Start Cluster Addr";
    case NAN_TLV_TYPE_EVENT_JOINED_CLUSTER:
        return "Joined Cluster Addr";
    case NAN_TLV_TYPE_EVENT_CLUSTER_SCAN_RESULTS:
        return "Cluster Scan Results";

    case NAN_TLV_TYPE_CLUSTER_SIZE_REQ:
        return "TCA Cluster Size Req";
    case NAN_TLV_TYPE_CLUSTER_SIZE_RSP:
        return "TCA Cluster Size Rsp";

    case NAN_TLV_TYPE_DE_PUBLISH_STATS:
        return "DE Publish Stats";
    case NAN_TLV_TYPE_DE_SUBSCRIBE_STATS:
        return "DE Subscribe Stats";
    case NAN_TLV_TYPE_DE_MAC_STATS:
        return "DE MAC Stats";
    case NAN_TLV_TYPE_DE_TIMING_SYNC_STATS:
        return "DE Timing Sync Stats";
    case NAN_TLV_TYPE_DE_DW_STATS:
        return "DE DW Stats";
    case NAN_TLV_TYPE_DE_STATS:
        return "DE Stats";

    default:
        return "Unknown";
    }
}

static const char *
dbglog_get_nan_publish_type_str(
    A_UINT32 publish_type)
{
    switch (publish_type)
    {
    case NAN_PUBLISH_TYPE_UNSOLICITED:
        return "Unsolicited";
    case NAN_PUBLISH_TYPE_SOLICITED:
        return "Solicited";
    case NAN_PUBLISH_TYPE_UNSOLICITED_SOLICITED:
        return "Unsolicited/Solicitted";
    default:
        return "Unknown";
    }
}

static const char *
dbglog_get_nan_tx_type_str(
    A_UINT32 tx_type)
{
    switch (tx_type)
    {
    case NAN_TX_TYPE_BROADCAST:
        return "Broadcast";
    case NAN_TX_TYPE_UNICAST:
        return "Unicast";
    default:
        return "Unknown";
    }
}

static const char *
dbglog_get_nan_subscribe_type_str(
    A_UINT8 subscribe_type)
{
    switch (subscribe_type)
    {
    case NAN_SUBSCRIBE_TYPE_PASSIVE:
        return "Passive";
    case NAN_SUBSCRIBE_TYPE_ACTIVE:
        return "Active";
    default:
        return "Unknown";
    }
}

static const char *
dbglog_get_nan_srf_attr_str(
    A_UINT8 srf_attr)
{
    switch (srf_attr)
    {
    case NAN_SRF_ATTR_BLOOM_FILTER:
        return "Bloom Filter";
    case NAN_SRF_ATTR_PARTIAL_MAC_ADDR:
        return "Partial MAC Addr";
    default:
        return "Unknown";
    }
}

static const char *
dbglog_get_nan_src_include_bit_str(
    A_UINT8 src_include_bit)
{
    switch (src_include_bit)
    {
    case NAN_SRF_INCLUDE_DO_NOT_RESPOND:
        return "Do Not Respond";
    case NAN_SRF_INCLUDE_RESPOND:
        return "Respond";
    default:
        return "Unknown";
    }
}

static const char *
dbglog_get_nan_match_alg_str(
    A_UINT8 match_alg)
{
    switch (match_alg)
    {
    case NAN_MATCH_ALG_MATCH_ONCE:
        return "Match Once";
    case NAN_MATCH_ALG_MATCH_CONTINUOUS:
        return "Match Continuous";
    default:
        return "Unknown";
    }
}


static const char *
dbglog_get_nan_tx_priority_str(
    A_UINT32 tx_priority)
{
    switch (tx_priority)
    {
    case NAN_TX_PRIORITY_LOW:
        return "Low";
    case NAN_TX_PRIORITY_NORMAL:
        return "Normal";
    case NAN_TX_PRIORITY_HIGH:
        return "High";
    default:
        return "Unknown";
    }
}

static const char *
dbglog_get_nan_window_str(
    A_UINT8 window)
{
    switch (window)
    {
    case NAN_WINDOW_DW:
        return "DW";
    case NAN_WINDOW_FAW:
        return "FAW";
    default:
        return "Unknown";
    }
}


static const char *
dbglog_get_nan_role_str(
    A_UINT8 nan_role)
{
    switch (nan_role)
    {
    case NAN_ROLE_NON_MASTER_NON_SYNC:
        return "NON-MASTER-NON-SYNC";
    case NAN_ROLE_NON_MASTER_SYNC:
        return "NON-MASTER-SYNC";
    case NAN_ROLE_MASTER:
        return "MASTER";
    case NAN_ROLE_INVALID:
        break;
    default:
        break;
    }
    return "Unknown";
}

static const char *
dbglog_get_nan_chan_type(
    A_UINT32 type)
{
    switch (type)
    {
    case 1:
        return "Primary DW";
    case 2:
        return "Primary DiscBcn";
    case 3:
        return "Secondary DW";
    case 4:
        return "Secondary DiscBcn";
    default:
        break;
    }
    return "Invalid/Unknown";
}

/*
 * NOTE: Use the following convenience macros with care!!!
 *
 * They expect that certain variables are present and also have specific uses,
 * e.g. within switch case statements.
 */
#define GET_TYPE    (args[0] >> 16)
#define GET_LENGTH  (args[0] & 0xFFFFu)

#define CHECK_TYPE(param_max, type)                                     \
    if ((param_idx < (param_max)) && ((type) != NAN_TLV_TYPE_LAST))     \
    {                                                                   \
        dbglog_printf(timestamp, vap_id,                                \
            "    %s: unexpected type %u for parameter index %u",        \
            __func__, (type), param_idx);                               \
        return;                                                         \
    }

#define CHECK_LENGTH(rx_length, exp_length)                             \
    if ((rx_length) != (exp_length))                                    \
    {                                                                   \
        dbglog_printf(timestamp, vap_id,                                \
            "    %s: invalid size (%u vs. %u) for parameter index %u",  \
            __func__, (rx_length), (exp_length), param_idx);            \
        break;                                                          \
    }

#define LOG_UNKNOWN_PARAM()                                             \
    dbglog_printf(timestamp, vap_id,                                    \
        "    %s: unknown parameter with index %u, numargs %u",          \
        __func__, param_idx, numargs);

#if 1
#define VAR64BIT(__lo, __hi) \
    (A_UINT64)((A_UINT64)(__lo & 0xFFFFFFFFull) | \
    (A_UINT64)(__hi & 0xFFFFFFFFull) << 32)
#endif

static void
dbglog_nan_tlv_print_handler(
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;

    if (type == NAN_TLV_TYPE_LAST)
    {
        dbglog_printf(timestamp, vap_id,
            "    %s: unexpected type %u for parameter index %u",
            __func__, type);
        return;
    }

    char str[128] = { 0 };
    A_UINT16 arg_idx;
    A_UINT16 byte_idx;
    A_UINT16 shift;
    A_UINT16 byteCount = 0;

    /* TLV data starts at args[1]. */
    for (arg_idx=1; arg_idx < numargs; ++arg_idx)
    {
        for (byte_idx=0, shift=24; byte_idx < 4; ++byte_idx, shift -= 8)
        {
            if (byteCount < length)
            {
                byteCount++;
                snprintf(&str[STR_INDEX((arg_idx-1), byte_idx)],
                        sizeof(&str[STR_INDEX((arg_idx-1), byte_idx)]),"%02X ",
                        ((args[arg_idx] >> shift) & 0xFF));
            }
        }
    }

    dbglog_printf(timestamp, vap_id, "    %s: %s",
        dbglog_get_nan_tlv_type_str(type), str );
}

static void
dbglog_nan_error_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_configuration_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
}

static void
dbglog_nan_configuration_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_publish_service_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(9, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    ttl: %u", args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    period: %u", args[1]);
        break;

    case 2:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    replyIndFlag: %s (%u)",
            BOOL_STR(args[1]), args[1]);
        break;

    case 3:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "    publishType: %s (%u)",
            dbglog_get_nan_publish_type_str(args[1]), args[1]);
        break;

    case 4:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "    txType: %s (%u)",
            dbglog_get_nan_tx_type_str(args[1]), args[1]);
        break;

    case 5:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    useRssi: %s (%u)",
            BOOL_STR(args[1]), args[1]);
        break;

    case 6:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    otaFlag: %s (%u)",
            BOOL_STR(args[1]), args[1]);
        break;

    case 7:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "    count: %u", args[1]);
        break;

    case 8:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "    connMap: 0x%04X", args[1]);
        // FIXME: decode this further.
        break;

    default:
        dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
        break;
    }
}

static void
dbglog_nan_publish_service_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
            dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_publish_service_cancel_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    LOG_UNKNOWN_PARAM();
}

static void
dbglog_nan_publish_service_cancel_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_publish_replied_ind_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
}

static void
dbglog_nan_publish_terminated_ind_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(1, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    reason: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_subscribe_service_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(12, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    ttl: %u", args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    period: %u", args[1]);
        break;

    case 2:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "    subscribeType: %s (%u)",
            dbglog_get_nan_subscribe_type_str(args[1]), args[1]);
        break;

    case 3:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    srfAttr: %s (%u)",
            dbglog_get_nan_srf_attr_str(args[1]), args[1]);
        break;

    case 4:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    srfInclude: %s (%u)",
            BOOL_STR(args[1]), args[1]);
        break;

    case 5:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    srfSend: %s (%u)",
            BOOL_STR(args[1]), args[1]);
        break;

    case 6:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    ssiRequired: %s (%u)",
            BOOL_STR(args[1]), args[1]);
        break;

    case 7:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "    matchAlg: %s (%u)",
            dbglog_get_nan_match_alg_str(args[1]), args[1]);
        break;

    case 8:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "    count: %u", args[1]);
        break;

    case 9:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    useRssi: %s (%u)",
            BOOL_STR(args[1]), args[1]);
        break;

    case 10:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    otaFlag: %s (%u)",
            BOOL_STR(args[1]), args[1]);
        break;

    case 11:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "    connMap: 0x%04X", args[1]);
        // FIXME: decode this further.
        break;

    default:
        dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
        break;
    }
}

static void
dbglog_nan_subscribe_service_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_subscribe_service_cancel_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    LOG_UNKNOWN_PARAM();
}

static void
dbglog_nan_subscribe_service_cancel_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_subscribe_match_ind_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(1, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    matchHandle: %u", args[1]);
        break;

    default:
        dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
        break;
    }
}

static void
dbglog_nan_subscribe_unmatch_ind_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(1, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    matchHandle: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_subscribe_terminated_ind_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(1, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    reason: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_de_event_ind_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
}

static void
dbglog_nan_transmit_followup_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "   txPriority: %s (%u)",
            dbglog_get_nan_tx_priority_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "    window: %s (%u)",
            dbglog_get_nan_window_str(args[1]), args[1]);
        break;

    default:
        dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
        break;
    }
}

static void
dbglog_nan_transmit_followup_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_followup_ind_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(1, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id, "    window: %s (%u)",
            dbglog_get_nan_window_str(args[1]), args[1]);
        break;

    default:
        dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
        break;
    }
}

static void
dbglog_nan_stats_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id,
            "    statsId: %s (%u)",
            dbglog_get_nan_stats_id_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    clear: %s (%u)",
            BOOL_STR(args[1]), args[1]);
        break;

    default:
        dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
        break;
    }
}

static void
dbglog_nan_publish_stats_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(15, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validPublishServiceReqMsgs: %u", args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validPublishServiceRspMsgs: %u", args[1]);
        break;

    case 2:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validPublishServiceCancelReqMsgs: %u", args[1]);
        break;

    case 3:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validPublishServiceCancelRspMsgs: %u", args[1]);
        break;

    case 4:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validPublishRepliedIndMsgs: %u", args[1]);
        break;

    case 5:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validPublishTerminatedIndMsgs: %u", args[1]);
        break;

    case 6:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validActiveSubscribes: %u", args[1]);
        break;

    case 7:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validMatches: %u", args[1]);
        break;

    case 8:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validFollowups: %u", args[1]);
        break;

    case 9:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidPublishServiceReqMsgs: %u", args[1]);
        break;

    case 10:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidPublishServiceCancelReqMsgs: %u", args[1]);
        break;

    case 11:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidActiveSubscribes: %u", args[1]);
        break;

    case 12:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidMatches: %u", args[1]);
        break;

    case 13:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidFollowups: %u", args[1]);
        break;

    case 14:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    publishCount: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_subscribe_stats_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(17, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validSubscribeServiceReqMsgs: %u", args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validSubscribeServiceRspMsgs: %u", args[1]);
        break;

    case 2:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validSubscribeServiceCancelReqMsgs: %u", args[1]);
        break;

    case 3:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validSubscribeServiceCancelRspMsgs: %u", args[1]);
        break;

    case 4:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validSubscribeTerminatedIndMsgs: %u", args[1]);
        break;

    case 5:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validSubscribeMatchIndMsgs: %u", args[1]);
        break;

    case 6:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validSubscribeUnmatchIndMsgs: %u", args[1]);
        break;

    case 7:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validSolicitedPublishes: %u", args[1]);
        break;

    case 8:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validMatches: %u", args[1]);
        break;

    case 9:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validFollowups: %u", args[1]);
        break;

    case 10:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidSubscribeServiceReqMsgs: %u", args[1]);
        break;

    case 11:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidSubscribeServiceCancelReqMsgs: %u", args[1]);
        break;

    case 12:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidSolicitedPublishes: %u", args[1]);
        break;

    case 13:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidMatches: %u", args[1]);
        break;

    case 14:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidFollowups: %u", args[1]);
        break;

    case 15:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    subscribeCount: %u", args[1]);
        break;

    case 16:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    bloomFilterIndex: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_mac_stats_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(25, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validFrames: %u", args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validActionFrames: %u", args[1]);
        break;

    case 2:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validBeaconFrames: %u", args[1]);
        break;

    case 3:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    ignoredActionFrames: %u", args[1]);
        break;

    case 4:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    ignoredBeaconFrames: %u", args[1]);
        break;

    case 5:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidFrames: %u", args[1]);
        break;

    case 6:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidActionFrames: %u", args[1]);
        break;

    case 7:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidBeaconFrames: %u", args[1]);
        break;

    case 8:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidMacHeaders: %u", args[1]);
        break;

    case 9:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidPafHeaders: %u", args[1]);
        break;

    case 10:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    nonNanBeaconFrames: %u", args[1]);
        break;

    case 11:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    earlyActionFrames: %u", args[1]);
        break;

    case 12:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    inDwActionFrames: %u", args[1]);
        break;

    case 13:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    lateActionFrames: %u", args[1]);
        break;

    case 14:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    framesQueued: %u", args[1]);
        break;

    case 15:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    totalTRSpUpdates: %u", args[1]);
        break;

    case 16:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    completeByTRSp: %u", args[1]);
        break;

    case 17:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    completeByTp75DW: %u", args[1]);
        break;

    case 18:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    completeByTendDW: %u", args[1]);
        break;

    case 19:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    lateActionFramesTx: %u", args[1]);
        break;

    case 20:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    twIncreases: %u", args[1]);
        break;

    case 21:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    twDecreases: %u", args[1]);
        break;

    case 22:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    twChanges: %u", args[1]);
        break;

    case 23:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    twHighwater: %u", args[1]);
        break;

    case 24:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    bloomFilterIndex: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_timing_sync_stats_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(36, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT64));
        dbglog_printf(timestamp, vap_id,
            "    currTsf: 0x%08lx_%08lx", args[2], args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT64));
        dbglog_printf(timestamp, vap_id,
            "    myRank: 0x%08lx_%08lx", args[2], args[1]);
        break;

    case 2:
        CHECK_LENGTH(length, sizeof(A_UINT64));
        dbglog_printf(timestamp, vap_id,
            "    currAmRank: 0x%08lx_%08lx", args[2], args[1]);
        break;

    case 3:
        CHECK_LENGTH(length, sizeof(A_UINT64));
        dbglog_printf(timestamp, vap_id,
            "    lastAmRank: 0x%08lx_%08lx", args[2], args[1]);
        break;

    case 4:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    currAmBTT: 0x%08lx", args[1]);
        break;

    case 5:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    lastAmBTT: 0x%08lx", args[1]);
        break;

    case 6:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id,
            "    currAmHopCount: %u", args[1]);
        break;

    case 7:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id,
            "    currRole: %s (%u)",
            dbglog_get_nan_role_str(args[1]), args[1]);
        break;

    case 8:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id,
            "    clusterId: 0x%04x", args[1]);
        break;

    case 9:
        CHECK_LENGTH(length, sizeof(A_UINT64));
        dbglog_printf(timestamp, vap_id,
            "    timeSpentInCurrRole: 0x%08lx_%08lx, %lu seconds",
            args[2], args[1], (A_UINT32) (VAR64BIT(args[1], args[2])/1000000ull));
        break;

    case 10:
        CHECK_LENGTH(length, sizeof(A_UINT64));
        dbglog_printf(timestamp, vap_id,
            "    totalTimeSpentAsMaster: 0x%08lx_%08lx, %lu seconds",
            args[2], args[1], (A_UINT32) (VAR64BIT(args[1], args[2])/1000000ull));
        break;

    case 11:
        CHECK_LENGTH(length, sizeof(A_UINT64));
        dbglog_printf(timestamp, vap_id,
            "    totalTimeSpentAsNonMasterSync: 0x%08lx_%08lx, %lu seconds",
            args[2], args[1], (A_UINT32) (VAR64BIT(args[1], args[2])/1000000ull));
        break;

    case 12:
        CHECK_LENGTH(length, sizeof(A_UINT64));
        dbglog_printf(timestamp, vap_id,
            "    totalTimeSpentAsNonMasterNonSync: 0x%08lx_%08lx, %lu seconds",
            args[2], args[1], (A_UINT32) (VAR64BIT(args[1], args[2])/1000000ull));
        break;

    case 13:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    transitionsToAnchorMaster: %u", args[1]);
        break;

    case 14:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    transitionsToMaster: %u", args[1]);
        break;

    case 15:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    transitionsToNonMasterSync: %u", args[1]);
        break;

    case 16:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    transitionsToNonMasterNonSync: %u", args[1]);
        break;

    case 17:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    amrUpdateCount: %u", args[1]);
        break;

    case 18:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    amrUpdateRankChangedCount: %u", args[1]);
        break;

    case 19:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    amrUpdateBTTChangedCount: %u", args[1]);
        break;

    case 20:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    amrUpdateHcChangedCount: %u", args[1]);
        break;

    case 21:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    amrUpdateNewDeviceCount: %u", args[1]);
        break;

    case 22:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    amrExpireCount: %u", args[1]);
        break;

    case 23:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    mergeCount: %u", args[1]);
        break;

    case 24:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    beaconsAboveHcLimit: %u", args[1]);
        break;

    case 25:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    beaconsBelowRssiThresh: %u", args[1]);
        break;

    case 26:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    beaconsIgnoredNoSpace: %u", args[1]);
        break;

    case 27:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    beaconsForOurCluster: %u", args[1]);
        break;

    case 28:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    beaconsForOtherCluster: %u", args[1]);
        break;

    case 29:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    beaconCancelRequests: %u", args[1]);
        break;

    case 30:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    beaconCancelFailures: %u", args[1]);
        break;

    case 31:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    beaconUpdateRequests: %u", args[1]);
        break;

    case 32:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    beaconUpdateFailures: %u", args[1]);
        break;

    case 33:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    syncBeaconTxAttempts: %u", args[1]);
        break;

    case 34:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    syncBeaconTxFailures: %u", args[1]);
        break;

    case 35:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    discBeaconTxAttempts: %u", args[1]);
        break;

    case 36:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    discBeaconTxFailures: %u", args[1]);
        break;

    case 37:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    amHopCountExpireCount: %u", args[1]);
        break;

    case 38:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    reserved: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_dw_stats_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    /* DW stats are the same as MAC stats. */
    dbglog_nan_mac_stats_print_handler(
        param_idx, vap_id, dbg_id, timestamp, numargs, args);
}

static void
dbglog_nan_de_stats_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(23, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validErrorRspMsgs: %u", args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validTransmitFollowupReqMsgs: %u", args[1]);
        break;

    case 2:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validTransmitFollowupRspMsgs: %u", args[1]);
        break;

    case 3:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validFollowupIndMsgs: %u", args[1]);
        break;

    case 4:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validConfigurationReqMsgs: %u", args[1]);
        break;

    case 5:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validConfigurationRspMsgs: %u", args[1]);
        break;

    case 6:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validStatsReqMsgs: %u", args[1]);
        break;

    case 7:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validStatsRspMsgs: %u", args[1]);
        break;

    case 8:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validEnableReqMsgs: %u", args[1]);
        break;

    case 9:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validEnableRspMsgs: %u", args[1]);
        break;

    case 10:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validDisableReqMsgs: %u", args[1]);
        break;

    case 11:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validDisableRspMsgs: %u", args[1]);
        break;

    case 12:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validDisableIndMsgs: %u", args[1]);
        break;

    case 13:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validEventIndMsgs: %u", args[1]);
        break;

    case 14:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validTcaReqMsgs: %u", args[1]);
        break;

    case 15:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validTcaRspMsgs: %u", args[1]);
        break;

    case 16:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validTcaIndMsgs: %u", args[1]);
        break;

    case 17:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidTransmitFollowupReqMsgs: %u", args[1]);
        break;

    case 18:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidConfigurationReqMsgs: %u", args[1]);
        break;

    case 19:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidStatsReqMsgs: %u", args[1]);
        break;

    case 20:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidEnableReqMsgs: %u", args[1]);
        break;

    case 21:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidDisableReqMsgs: %u", args[1]);
        break;

    case 22:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidTcaReqMsgs: %u", args[1]);
        break;

    case 23:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validBcnSdfPayloadReqMsgs: %u", args[1]);
        break;

    case 24:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validBcnSdfPayloadRspMsgs: %u", args[1]);
        break;

    case 25:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    validBcnSdfPayloadIndMsgs: %u", args[1]);
        break;

    case 26:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id,
            "    invalidBcnSdfPayloadReqMsgs: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_stats_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    static A_UINT8 stats_id = NAN_STATS_ID_LAST;
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    //CHECK_TYPE(3, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    case 2:
        CHECK_LENGTH(length, sizeof(A_UINT8));
        dbglog_printf(timestamp, vap_id,
            "    statsId: %s (%u)",
            dbglog_get_nan_stats_id_str(args[1]), args[1]);
        stats_id = (A_UINT8)args[1];
        break;

    default:
        if (NAN_STATS_ID_LAST == stats_id)
        {
            dbglog_printf(timestamp, vap_id, "Stats ID not set yet");
            break;
        }

        /* -3 for the status, length, and stats ID already processed. */
        param_idx -= 3;

        switch (stats_id)
        {
        case NAN_STATS_ID_DE_PUBLISH:
            dbglog_nan_publish_stats_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_STATS_ID_DE_SUBSCRIBE:
            dbglog_nan_subscribe_stats_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_STATS_ID_DE_MAC:
            dbglog_nan_mac_stats_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_STATS_ID_DE_TIMING_SYNC:
            dbglog_nan_timing_sync_stats_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_STATS_ID_DE_DW:
            dbglog_nan_dw_stats_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_STATS_ID_DE:
            dbglog_nan_de_stats_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        default:
            dbglog_printf(timestamp, vap_id, "Invalid stats id %u", stats_id);
            break;
        }
        break;
    }
}

static void
dbglog_nan_enable_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
}

static void
dbglog_nan_enable_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_disable_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    LOG_UNKNOWN_PARAM();
}

static void
dbglog_nan_disable_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(1, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_disable_ind_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(1, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    reason: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_tca_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(5, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    tcaTlv: %s (%u)",
            dbglog_get_nan_tlv_type_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    rising: %s", BOOL_STR(args[1]));
        break;

    case 2:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    falling: %s", BOOL_STR(args[1]));
        break;

    case 3:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    clear: %s", BOOL_STR(args[1]));
        break;

    case 4:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id, "    threshold: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_tca_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(2, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_tca_ind_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(4, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    tcaTlv: %s (%u)",
            dbglog_get_nan_tlv_type_str(args[1]), args[1]);
        break;

    case 1:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    rising: %s", BOOL_STR(args[1]));
        break;

    case 2:
        CHECK_LENGTH(length, sizeof(A_BOOL));
        dbglog_printf(timestamp, vap_id, "    falling: %s", BOOL_STR(args[1]));
        break;

    case 3:
        CHECK_LENGTH(length, sizeof(A_UINT32));
        dbglog_printf(timestamp, vap_id, "    value: %u", args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}

static void
dbglog_nan_bcn_sdf_payload_req_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    //A_UINT16 type = GET_TYPE;
    //A_UINT16 length = GET_LENGTH;
    //CHECK_TYPE(0, type);

    dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
}

static void
dbglog_nan_bcn_sdf_payload_rsp_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    A_UINT16 type = GET_TYPE;
    A_UINT16 length = GET_LENGTH;
    CHECK_TYPE(1, type);

    switch (param_idx)
    {
    case 0:
        CHECK_LENGTH(length, sizeof(A_UINT16));
        dbglog_printf(timestamp, vap_id, "    status: %s (%u)",
            dbglog_get_nan_status_str(args[1]), args[1]);
        break;

    default:
        LOG_UNKNOWN_PARAM();
        break;
    }
}



static void
dbglog_nan_bcn_sdf_payload_ind_msg_print_handler(
    A_UINT32 param_idx,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    //A_UINT16 type = GET_TYPE;
    //A_UINT16 length = GET_LENGTH;
    //CHECK_TYPE(0, type);

    dbglog_nan_tlv_print_handler(vap_id, dbg_id, timestamp, numargs, args);
}

static A_BOOL
dbglog_nan_api_msg_print_handler(
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    static A_UINT32 msg_id = NAN_MSG_ID_LAST;
    static A_UINT32 param_idx = 0;

    if (dbg_id == NAN_DBGID_API_MSG_HEADER)
    {
        param_idx = 0;
        msg_id = args[0];

        if (args[0] < NAN_MSG_ID_LAST)
        {
            dbglog_printf(timestamp, vap_id,
                "NAN %s/%u (version %u, id %u, length %u, handle %u, "
                "transaction id %u)",
                dbglog_get_nan_msg_id_str(args[0]), args[0],
                args[1], args[0], args[2], args[3], args[4]);
        }
        else
        {
            dbglog_printf(timestamp, vap_id,
                "NAN unknown msg %u (version %u, length %u, handle %u, "
                "transaction id %u)",
                args[0], args[1], args[2], args[3], args[4]);
        }
        return TRUE;
    }
    else if (dbg_id == NAN_DBGID_API_MSG_DATA)
    {
        switch (msg_id)
        {
        case NAN_MSG_ID_ERROR_RSP:
            dbglog_nan_error_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_CONFIGURATION_REQ:
            dbglog_nan_configuration_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_CONFIGURATION_RSP:
            dbglog_nan_configuration_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_PUBLISH_SERVICE_REQ:
            dbglog_nan_publish_service_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_PUBLISH_SERVICE_RSP:
            dbglog_nan_publish_service_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_REQ:
            dbglog_nan_publish_service_cancel_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_PUBLISH_SERVICE_CANCEL_RSP:
            dbglog_nan_publish_service_cancel_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_PUBLISH_REPLIED_IND:
            dbglog_nan_publish_replied_ind_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_PUBLISH_TERMINATED_IND:
            dbglog_nan_publish_terminated_ind_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_SUBSCRIBE_SERVICE_REQ:
            dbglog_nan_subscribe_service_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_SUBSCRIBE_SERVICE_RSP:
            dbglog_nan_subscribe_service_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_REQ:
            dbglog_nan_subscribe_service_cancel_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_SUBSCRIBE_SERVICE_CANCEL_RSP:
            dbglog_nan_subscribe_service_cancel_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_SUBSCRIBE_MATCH_IND:
            dbglog_nan_subscribe_match_ind_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_SUBSCRIBE_UNMATCH_IND:
            dbglog_nan_subscribe_unmatch_ind_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_SUBSCRIBE_TERMINATED_IND:
            dbglog_nan_subscribe_terminated_ind_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_DE_EVENT_IND:
            dbglog_nan_de_event_ind_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_TRANSMIT_FOLLOWUP_REQ:
            dbglog_nan_transmit_followup_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_TRANSMIT_FOLLOWUP_RSP:
            dbglog_nan_transmit_followup_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_FOLLOWUP_IND:
            dbglog_nan_followup_ind_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_STATS_REQ:
            dbglog_nan_stats_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_STATS_RSP:
            dbglog_nan_stats_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_ENABLE_REQ:
            dbglog_nan_enable_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_ENABLE_RSP:
            dbglog_nan_enable_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_DISABLE_REQ:
            dbglog_nan_disable_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_DISABLE_RSP:
            dbglog_nan_disable_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_DISABLE_IND:
            dbglog_nan_disable_ind_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_TCA_REQ:
            dbglog_nan_tca_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_TCA_RSP:
            dbglog_nan_tca_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_TCA_IND:
            dbglog_nan_tca_ind_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_BCN_SDF_PAYLOAD_REQ:
            dbglog_nan_bcn_sdf_payload_req_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_BCN_SDF_PAYLOAD_RSP:
            dbglog_nan_bcn_sdf_payload_rsp_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;

        case NAN_MSG_ID_BCN_SDF_PAYLOAD_IND:
            dbglog_nan_bcn_sdf_payload_ind_msg_print_handler(
                param_idx, vap_id, dbg_id, timestamp, numargs, args);
            break;


        default:
            LOG_UNKNOWN_PARAM();
            break;
        }

        param_idx++;
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

static A_BOOL
dbglog_nan_ota_pkt_print_handler(
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    if (dbg_id == NAN_DBGID_OTA_PKT_HEADER)
    {
        dbglog_printf(timestamp, vap_id, "%s %s, length %u",
            DBG_MSG_ARR[dbg_id],
            (args[0] ? "transmitted" : "received"), args[1]);
        return TRUE;
    }

    if (dbg_id == NAN_DBGID_OTA_PKT_DATA)
    {
        char str[128];
        A_UINT16 arg_idx;
        A_UINT16 byte_idx;
        A_UINT16 shift;

        for (arg_idx=0; arg_idx < numargs; ++arg_idx)
        {
            for (byte_idx=0, shift=24; byte_idx < 4; ++byte_idx, shift -= 8)
            {
                snprintf(&str[STR_INDEX(arg_idx, byte_idx)],
                        sizeof(&str[STR_INDEX(arg_idx, byte_idx)]), "%02X ",
                        ((args[arg_idx] >> shift) & 0xFF));
            }
        }

        dbglog_printf(timestamp, vap_id, "%s: %s", DBG_MSG_ARR[dbg_id], str);
        return TRUE;
    }

    return FALSE;
}

A_BOOL
dbglog_nan_print_handler(
    A_UINT32 mod_id,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args)
{
    if ((dbg_id >= NAN_DBGID_DBG_LOG_BASE) &&
        (dbg_id < NAN_DBGID_DBG_LOG_LAST))
    {
        return dbglog_nan_debug_print_handler(
            vap_id, dbg_id, timestamp, numargs, args);
    }
    else if ((dbg_id >= NAN_DBGID_EVT_BASE) &&
             (dbg_id < NAN_DBGID_EVT_LOG_LAST))
    {
        return dbglog_nan_event_print_handler(
            vap_id, dbg_id, timestamp, numargs, args);
    }
    else if ((dbg_id >= NAN_DBGID_API_MSG_BASE) &&
             (dbg_id < NAN_DBGID_API_MSG_LAST))
    {
        return dbglog_nan_api_msg_print_handler(
            vap_id, dbg_id, timestamp, numargs, args);
    }
    else if ((dbg_id >= NAN_DBGID_OTA_PKT_BASE) &&
             (dbg_id < NAN_DBGID_OTA_PKT_LAST))
    {
        return dbglog_nan_ota_pkt_print_handler(
            vap_id, dbg_id, timestamp, numargs, args);
    }
    else if (dbg_id == DBGLOG_DBGID_SM_FRAMEWORK_PROXY_DBGLOG_MSG)
    {
        static const char *states[] =
        {
            "IDLE",
            "WAIT",
            "STARTING",
            "JOINING",
            "ACTIVE",
            "MERGING"
        };

        static const char *events[] =
        {
            "NONE",
            "STARTSM",
            "STOPSM",
            "BEACON_RECVD",
            "WAIT_TIMEOUT",
            "START_TIMEOUT",
            "DELAYED_START",
            "BGSCAN_TIMEOUT",
            "SCAN_TIMEOUT",
            "SCAN_COMPLETE",
            "CLUSTER_MERGE",
            "UNKNOWN"
        };

        dbglog_sm_print(timestamp, vap_id, numargs, args, "NAN Cluster SM",
            states, ARRAY_LENGTH(states), events, ARRAY_LENGTH(events));
        return TRUE;
    }

    return FALSE;
}

