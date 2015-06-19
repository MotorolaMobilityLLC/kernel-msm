/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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


#ifdef CONFIG_ANDROID_LOG

#include <android/log.h>

#define FWDEBUG_LOG_NAME        "ROME"
#define FWDEBUG_NAME            "ROME_DEBUG"
#define android_printf(...) \
       __android_log_print(ANDROID_LOG_INFO, FWDEBUG_LOG_NAME, __VA_ARGS__);

#define debug_printf(...) do {     \
    if (optionflag & DEBUG_FLAG)   \
       __android_log_print(ANDROID_LOG_INFO, FWDEBUG_NAME, __VA_ARGS__);    \
} while(0)
#else
#define android_printf printf
#define debug_printf(...) do {} while(0);
#endif

#define qxdm_log(buf) MSG_SPRINTF_1(MSG_SSID_WLAN, MSG_LEGACY_HIGH, "%s", buf);

unsigned char buf[RECLEN];
extern int optionflag;
extern int32_t max_records;
extern int32_t record;
extern FILE *log_out;

#define MAX_DBG_MSGS 256

module_dbg_print mod_print[WLAN_MODULE_ID_MAX];

void
diag_print_legacy_logs(const char *buf)
{
   uint32_t res;

    if (optionflag & QXDM_FLAG) {
        qxdm_log(buf);
    }

    if (optionflag & LOGFILE_FLAG) {
        record++;
        if (log_out)
            fprintf(log_out, "%s\n", buf);
        else
            return;
        if (record == max_records) {
            record = 0;
            fseek(log_out, record, SEEK_SET);
        }
    }
    if (optionflag & CONSOLE_FLAG) {
        android_printf("%s\n", buf);
    }
}

int
diag_msg_handler(uint32_t id, char *payload,  uint16_t vdevid,
    uint32_t timestamp)
{
    uint32_t moduleid = 0;
    moduleid = (id >> 9) & 0x3F;
    if (moduleid >= WLAN_MODULE_ID_MAX)
        return 0;
    if (mod_print[moduleid] != NULL) {
        if (!(mod_print[moduleid](moduleid, vdevid,
            DBGLOG_DBGID_SM_FRAMEWORK_PROXY_DBGLOG_MSG,
            timestamp, 4, ( uint32_t *)payload))) {
            return 0;
        }
        return 1;
    }
    return 0;
}

extern A_BOOL
dbglog_nan_print_handler(
    A_UINT32 mod_id,
    A_UINT16 vap_id,
    A_UINT32 dbg_id,
    A_UINT32 timestamp,
    A_UINT16 numargs,
    A_UINT32 *args);

const char *dbglog_get_module_str(A_UINT32 module_id)
{
    switch (module_id) {
    case WLAN_MODULE_INF:
        return "INF";
    case WLAN_MODULE_WMI:
        return "WMI";
    case WLAN_MODULE_STA_PWRSAVE:
        return "STA PS";
    case WLAN_MODULE_WHAL:
        return "WHAL";
    case WLAN_MODULE_COEX:
        return "COEX";
    case WLAN_MODULE_ROAM:
        return "ROAM";
    case WLAN_MODULE_RESMGR_CHAN_MANAGER:
        return "CHANMGR";
    case WLAN_MODULE_RESMGR:
        return "RESMGR";
    case WLAN_MODULE_VDEV_MGR:
        return "VDEV";
    case WLAN_MODULE_SCAN:
        return "SCAN";
    case WLAN_MODULE_RATECTRL:
        return "RC";
    case WLAN_MODULE_AP_PWRSAVE:
        return "AP PS";
    case WLAN_MODULE_BLOCKACK:
        return "BA";
    case WLAN_MODULE_MGMT_TXRX:
        return "MGMT";
    case WLAN_MODULE_DATA_TXRX:
        return "DATA";
    case WLAN_MODULE_HTT:
        return "HTT";
    case WLAN_MODULE_HOST:
        return "HOST";
    case WLAN_MODULE_BEACON:
        return "BEACON";
    case WLAN_MODULE_OFFLOAD:
        return "OFFLOAD";
    case WLAN_MODULE_WAL:
        return "WAL";
    case WAL_MODULE_DE:
        return "DE";
    case WLAN_MODULE_PCIELP:
        return "PCIELP";
    case WLAN_MODULE_RTT:
        return "RTT";
    case WLAN_MODULE_DCS:
        return "DCS";
    case WLAN_MODULE_CACHEMGR:
        return "CACHEMGR";
    case WLAN_MODULE_ANI:
        return "ANI";
    case WLAN_MODULE_TEST:
        return "TESTPOINT";
    case WLAN_MODULE_STA_SMPS:
        return "STA_SMPS";
    case WLAN_MODULE_TDLS:
        return "TDLS";
    case WLAN_MODULE_P2P:
        return "P2P";
    case WLAN_MODULE_WOW:
        return "WoW";
    case WLAN_MODULE_IBSS_PWRSAVE:
        return "IBSS PS";
    case WLAN_MODULE_EXTSCAN:
        return "ExtScan";
    case WLAN_MODULE_UNIT_TEST:
        return "UNIT_TEST";
    case WLAN_MODULE_MLME:
        return "MLME";
    case WLAN_MODULE_SUPPL:
        return "SUPPLICANT";
    default:
        return "UNKNOWN";
    }
}

char * DBG_MSG_ARR[WLAN_MODULE_ID_MAX][MAX_DBG_MSGS] =
{
    {
        "INF_MSG_START",
        "INF_ASSERTION_FAILED",
        "INF_TARGET_ID",
        "INF_MSG_END"
    },
    {
        "WMI_DBGID_DEFINITION_START",
        "WMI_CMD_RX_XTND_PKT_TOO_SHORT",
        "WMI_EXTENDED_CMD_NOT_HANDLED",
        "WMI_CMD_RX_PKT_TOO_SHORT",
        "WMI_CALLING_WMI_EXTENSION_FN",
        "WMI_CMD_NOT_HANDLED",
        "WMI_IN_SYNC",
        "WMI_TARGET_WMI_SYNC_CMD",
        "WMI_SET_SNR_THRESHOLD_PARAMS",
        "WMI_SET_RSSI_THRESHOLD_PARAMS",
        "WMI_SET_LQ_TRESHOLD_PARAMS",
        "WMI_TARGET_CREATE_PSTREAM_CMD",
        "WMI_WI_DTM_INUSE",
        "WMI_TARGET_DELETE_PSTREAM_CMD",
        "WMI_TARGET_IMPLICIT_DELETE_PSTREAM_CMD",
        "WMI_TARGET_GET_BIT_RATE_CMD",
        "WMI_GET_RATE_MASK_CMD_FIX_RATE_MASK_IS",
        "WMI_TARGET_GET_AVAILABLE_CHANNELS_CMD",
        "WMI_TARGET_GET_TX_PWR_CMD",
        "WMI_FREE_EVBUF_WMIBUF",
        "WMI_FREE_EVBUF_DATABUF",
        "WMI_FREE_EVBUF_BADFLAG",
        "WMI_HTC_RX_ERROR_DATA_PACKET",
        "WMI_HTC_RX_SYNC_PAUSING_FOR_MBOX",
        "WMI_INCORRECT_WMI_DATA_HDR_DROPPING_PKT",
        "WMI_SENDING_READY_EVENT",
        "WMI_SETPOWER_MDOE_TO_MAXPERF",
        "WMI_SETPOWER_MDOE_TO_REC",
        "WMI_BSSINFO_EVENT_FROM",
        "WMI_TARGET_GET_STATS_CMD",
        "WMI_SENDING_SCAN_COMPLETE_EVENT",
        "WMI_SENDING_RSSI_INDB_THRESHOLD_EVENT ",
        "WMI_SENDING_RSSI_INDBM_THRESHOLD_EVENT",
        "WMI_SENDING_LINK_QUALITY_THRESHOLD_EVENT",
        "WMI_SENDING_ERROR_REPORT_EVENT",
        "WMI_SENDING_CAC_EVENT",
        "WMI_TARGET_GET_ROAM_TABLE_CMD",
        "WMI_TARGET_GET_ROAM_DATA_CMD",
        "WMI_SENDING_GPIO_INTR_EVENT",
        "WMI_SENDING_GPIO_ACK_EVENT",
        "WMI_SENDING_GPIO_DATA_EVENT",
        "WMI_CMD_RX",
        "WMI_CMD_RX_XTND",
        "WMI_EVENT_SEND",
        "WMI_EVENT_SEND_XTND",
        "WMI_CMD_PARAMS_DUMP_START",
        "WMI_CMD_PARAMS_DUMP_END",
        "WMI_CMD_PARAMS",
        "WMI_EVENT_ALLOC_FAILURE",
        "WMI_DBGID_DCS_PARAM_CMD",
        "WMI_SEND_EVENT_WRONG_TLV",
        "WMI_SEND_EVENT_NO_TLV_DEF",
        "WMI_DBGID_DEFNITION_END",
    },
    {
        "PS_STA_DEFINITION_START",
        "PS_STA_PM_ARB_REQUEST",
        "PS_STA_DELIVER_EVENT",
        "PS_STA_PSPOLL_SEQ_DONE",
        "PS_STA_COEX_MODE",
        "PS_STA_PSPOLL_ALLOW",
        "PS_STA_SET_PARAM",
        "PS_STA_SPECPOLL_TIMER_STARTED",
        "PS_STA_SPECPOLL_TIMER_STOPPED",
    },
    {
        "WHAL_DBGID_DEFINITION_START",
        "WHAL_ERROR_ANI_CONTROL",
        "WHAL_ERROR_CHIP_TEST1",
        "WHAL_ERROR_CHIP_TEST2",
        "WHAL_ERROR_EEPROM_CHECKSUM",
        "WHAL_ERROR_EEPROM_MACADDR",
        "WHAL_ERROR_INTERRUPT_HIU",
        "WHAL_ERROR_KEYCACHE_RESET",
        "WHAL_ERROR_KEYCACHE_SET",
        "WHAL_ERROR_KEYCACHE_TYPE",
        "WHAL_ERROR_KEYCACHE_TKIPENTRY",
        "WHAL_ERROR_KEYCACHE_WEPLENGTH",
        "WHAL_ERROR_PHY_INVALID_CHANNEL",
        "WHAL_ERROR_POWER_AWAKE",
        "WHAL_ERROR_POWER_SET",
        "WHAL_ERROR_RECV_STOPDMA",
        "WHAL_ERROR_RECV_STOPPCU",
        "WHAL_ERROR_RESET_CHANNF1",
        "WHAL_ERROR_RESET_CHANNF2",
        "WHAL_ERROR_RESET_PM",
        "WHAL_ERROR_RESET_OFFSETCAL",
        "WHAL_ERROR_RESET_RFGRANT",
        "WHAL_ERROR_RESET_RXFRAME",
        "WHAL_ERROR_RESET_STOPDMA",
        "WHAL_ERROR_RESET_ERRID",
        "WHAL_ERROR_RESET_ADCDCCAL1",
        "WHAL_ERROR_RESET_ADCDCCAL2",
        "WHAL_ERROR_RESET_TXIQCAL",
        "WHAL_ERROR_RESET_RXIQCAL",
        "WHAL_ERROR_RESET_CARRIERLEAK",
        "WHAL_ERROR_XMIT_COMPUTE",
        "WHAL_ERROR_XMIT_NOQUEUE",
        "WHAL_ERROR_XMIT_ACTIVEQUEUE",
        "WHAL_ERROR_XMIT_BADTYPE",
        "WHAL_ERROR_XMIT_STOPDMA",
        "WHAL_ERROR_INTERRUPT_BB_PANIC",
        "WHAL_ERROR_PAPRD_MAXGAIN_ABOVE_WINDOW",
        "WHAL_ERROR_QCU_HW_PAUSE_MISMATCH",
        "WHAL_DBGID_DEFINITION_END",
    },
    {
        "COEX_DEBUGID_START",
        "BTCOEX_DBG_MCI_1",
        "BTCOEX_DBG_MCI_2",
        "BTCOEX_DBG_MCI_3",
        "BTCOEX_DBG_MCI_4",
        "BTCOEX_DBG_MCI_5",
        "BTCOEX_DBG_MCI_6",
        "BTCOEX_DBG_MCI_7",
        "BTCOEX_DBG_MCI_8",
        "BTCOEX_DBG_MCI_9",
        "BTCOEX_DBG_MCI_10",
        "COEX_WAL_BTCOEX_INIT",
        "COEX_WAL_PAUSE",
        "COEX_WAL_RESUME",
        "COEX_UPDATE_AFH",
        "COEX_HWQ_EMPTY_CB",
        "COEX_MCI_TIMER_HANDLER",
        "COEX_MCI_RECOVER",
        "ERROR_COEX_MCI_ISR",
        "ERROR_COEX_MCI_GPM",
        "COEX_ProfileType",
        "COEX_LinkID",
        "COEX_LinkState",
        "COEX_LinkRole",
        "COEX_LinkRate",
        "COEX_VoiceType",
        "COEX_TInterval",
        "COEX_WRetrx",
        "COEX_Attempts",
        "COEX_PerformanceState",
        "COEX_LinkType",
        "COEX_RX_MCI_GPM_VERSION_QUERY",
        "COEX_RX_MCI_GPM_VERSION_RESPONSE",
        "COEX_RX_MCI_GPM_STATUS_QUERY",
        "COEX_STATE_WLAN_VDEV_DOWN",
        "COEX_STATE_WLAN_VDEV_START",
        "COEX_STATE_WLAN_VDEV_CONNECTED",
        "COEX_STATE_WLAN_VDEV_SCAN_STARTED",
        "COEX_STATE_WLAN_VDEV_SCAN_END",
        "COEX_STATE_WLAN_DEFAULT",
        "COEX_CHANNEL_CHANGE",
        "COEX_POWER_CHANGE",
        "COEX_CONFIG_MGR",
        "COEX_TX_MCI_GPM_BT_CAL_REQ",
        "COEX_TX_MCI_GPM_BT_CAL_GRANT",
        "COEX_TX_MCI_GPM_BT_CAL_DONE",
        "COEX_TX_MCI_GPM_WLAN_CAL_REQ",
        "COEX_TX_MCI_GPM_WLAN_CAL_GRANT",
        "COEX_TX_MCI_GPM_WLAN_CAL_DONE",
        "COEX_TX_MCI_GPM_BT_DEBUG",
        "COEX_TX_MCI_GPM_VERSION_QUERY",
        "COEX_TX_MCI_GPM_VERSION_RESPONSE",
        "COEX_TX_MCI_GPM_STATUS_QUERY",
        "COEX_TX_MCI_GPM_HALT_BT_GPM",
        "COEX_TX_MCI_GPM_WLAN_CHANNELS",
        "COEX_TX_MCI_GPM_BT_PROFILE_INFO",
        "COEX_TX_MCI_GPM_BT_STATUS_UPDATE",
        "COEX_TX_MCI_GPM_BT_UPDATE_FLAGS",
        "COEX_TX_MCI_GPM_UNKNOWN",
        "COEX_TX_MCI_SYS_WAKING",
        "COEX_TX_MCI_LNA_TAKE",
        "COEX_TX_MCI_LNA_TRANS",
        "COEX_TX_MCI_SYS_SLEEPING",
        "COEX_TX_MCI_REQ_WAKE",
        "COEX_TX_MCI_REMOTE_RESET",
        "COEX_TX_MCI_TYPE_UNKNOWN",
        "COEX_WHAL_MCI_RESET",
        "COEX_POLL_BT_CAL_DONE_TIMEOUT",
        "COEX_WHAL_PAUSE",
        "COEX_RX_MCI_GPM_BT_CAL_REQ",
        "COEX_RX_MCI_GPM_BT_CAL_DONE",
        "COEX_RX_MCI_GPM_BT_CAL_GRANT",
        "COEX_WLAN_CAL_START",
        "COEX_WLAN_CAL_RESULT",
        "COEX_BtMciState",
        "COEX_BtCalState",
        "COEX_WlanCalState",
        "COEX_RxReqWakeCount",
        "COEX_RxRemoteResetCount",
        "COEX_RESTART_CAL",
        "COEX_SENDMSG_QUEUE",
        "COEX_RESETSEQ_LNAINFO_TIMEOUT",
        "COEX_MCI_ISR_IntRaw",
        "COEX_MCI_ISR_Int1Raw",
        "COEX_MCI_ISR_RxMsgRaw",
        "COEX_WHAL_COEX_RESET",
        "COEX_WAL_COEX_INIT",
        "COEX_TXRX_CNT_LIMIT_ISR",
        "COEX_CH_BUSY",
        "COEX_REASSESS_WLAN_STATE",
        "COEX_BTCOEX_WLAN_STATE_UPDATE",
        "COEX_BT_NUM_OF_PROFILES",
        "COEX_BT_NUM_OF_HID_PROFILES",
        "COEX_BT_NUM_OF_ACL_PROFILES",
        "COEX_BT_NUM_OF_HI_ACL_PROFILES",
        "COEX_BT_NUM_OF_VOICE_PROFILES",
        "COEX_WLAN_AGGR_LIMIT",
        "COEX_BT_LOW_PRIO_BUDGET",
        "COEX_BT_HI_PRIO_BUDGET",
        "COEX_BT_IDLE_TIME",
        "COEX_SET_COEX_WEIGHT",
        "COEX_WLAN_WEIGHT_GROUP",
        "COEX_BT_WEIGHT_GROUP",
        "COEX_BT_INTERVAL_ALLOC",
        "COEX_BT_SCHEME",
        "COEX_BT_MGR",
        "COEX_BT_SM_ERROR",
        "COEX_SYSTEM_UPDATE",
        "COEX_LOW_PRIO_LIMIT",
        "COEX_HI_PRIO_LIMIT",
        "COEX_BT_INTERVAL_START",
        "COEX_WLAN_INTERVAL_START",
        "COEX_NON_LINK_BUDGET",
        "COEX_CONTENTION_MSG",
        "COEX_SET_NSS",
        "COEX_SELF_GEN_MASK",
        "COEX_PROFILE_ERROR",
        "COEX_WLAN_INIT",
        "COEX_BEACON_MISS",
        "COEX_BEACON_OK",
        "COEX_BTCOEX_SCAN_ACTIVITY",
        "COEX_SCAN_ACTIVITY",
        "COEX_FORCE_QUIETTIME",
        "COEX_BT_MGR_QUIETTIME",
        "COEX_BT_INACTIVITY_TRIGGER",
        "COEX_BT_INACTIVITY_REPORTED",
        "COEX_TX_MCI_GPM_WLAN_PRIO",
        "COEX_TX_MCI_GPM_BT_PAUSE_PROFILE",
        "COEX_TX_MCI_GPM_WLAN_SET_ACL_INACTIVITY",
        "COEX_RX_MCI_GPM_BT_ACL_INACTIVITY_REPORT",
        "COEX_GENERIC_ERROR",
        "COEX_RX_RATE_THRESHOLD",
        "COEX_RSSI",
        "COEX_WLAN_VDEV_NOTIF_START", //                 133
        "COEX_WLAN_VDEV_NOTIF_UP", //                    134
        "COEX_WLAN_VDEV_NOTIF_DOWN",   //                135
        "COEX_WLAN_VDEV_NOTIF_STOP",    //               136
        "COEX_WLAN_VDEV_NOTIF_ADD_PEER",    //           137
        "COEX_WLAN_VDEV_NOTIF_DELETE_PEER",  //          138
        "COEX_WLAN_VDEV_NOTIF_CONNECTED_PEER",  //       139
        "COEX_WLAN_VDEV_NOTIF_PAUSE",   //               140
        "COEX_WLAN_VDEV_NOTIF_UNPAUSED",    //           141
        "COEX_STATE_WLAN_VDEV_PEER_ADD",    //           142
        "COEX_STATE_WLAN_VDEV_CONNECTED_PEER",    //     143
        "COEX_STATE_WLAN_VDEV_DELETE_PEER",  //          144
        "COEX_STATE_WLAN_VDEV_PAUSE",  //                145
        "COEX_STATE_WLAN_VDEV_UNPAUSED",    //           146
        "COEX_SCAN_CALLBACK",           //               147
        "COEX_RC_SET_CHAINMASK",           //            148
        "COEX_TX_MCI_GPM_WLAN_SET_BT_RXSS_THRES",  //    149
        "COEX_TX_MCI_GPM_BT_RXSS_THRES_QUERY",     //    150
        "COEX_BT_RXSS_THRES",           //               151
        "COEX_BT_PROFILE_ADD_RMV",           //          152
        "COEX_BT_SCHED_INFO",           //               153
        "COEX_TRF_MGMT",           //                    154
        "COEX_SCHED_START",           //                 155
        "COEX_SCHED_RESULT",           //                156
        "COEX_SCHED_ERROR",      //                      157
        "COEX_SCHED_PRE_OP",      //                     158
        "COEX_SCHED_POST_OP",      //                    159
        "COEX_RX_RATE",      //                          160
        "COEX_ACK_PRIORITY",      //                     161
        "COEX_STATE_WLAN_VDEV_UP",      //               162
        "COEX_STATE_WLAN_VDEV_PEER_UPDATE",      //      163
        "COEX_STATE_WLAN_VDEV_STOP",      //             164
        "COEX_WLAN_PAUSE_PEER",      //                  165
        "COEX_WLAN_UNPAUSE_PEER",      //                166
        "COEX_WLAN_PAUSE_INTERVAL_START",      //        167
        "COEX_WLAN_POSTPAUSE_INTERVAL_START",      //    168
        "COEX_TRF_FREERUN",      //                      169
        "COEX_TRF_SHAPE_PM",      //                     170
        "COEX_TRF_SHAPE_PSP",      //                    171
        "COEX_TRF_SHAPE_S_CTS",      //                  172
        "COEX_CHAIN_CONFIG",        //                   173
        "COEX_SYSTEM_MONITOR",        //                 174
        "COEX_SINGLECHAIN_INIT",        //               175
        "COEX_MULTICHAIN_INIT",        //                176
        "COEX_SINGLECHAIN_DBG_1",        //              177
        "COEX_SINGLECHAIN_DBG_2",        //              178
        "COEX_SINGLECHAIN_DBG_3",        //              179
        "COEX_MULTICHAIN_DBG_1",        //               180
        "COEX_MULTICHAIN_DBG_2",        //               181
        "COEX_MULTICHAIN_DBG_3",        //               182
        "COEX_PSP_TX_CB",       //                       183
        "COEX_PSP_RX_CB",       //                       184
        "COEX_PSP_STAT_1",      //                       185
        "COEX_PSP_SPEC_POLL",   //                       186
        "COEX_PSP_READY_STATE", //                       187
        "COEX_PSP_TX_STATUS_STATE",     //               188
        "COEX_PSP_RX_STATUS_STATE_1",   //               189
        "COEX_PSP_NOT_READY_STATE",     //               190
        "COEX_PSP_DISABLED_STATE",      //               191
        "COEX_PSP_ENABLED_STATE",       //               192
        "COEX_PSP_SEND_PSPOLL", //                       193
        "COEX_PSP_MGR_ENTER",   //                       194
        "COEX_PSP_MGR_RESULT",  //                       195
        "COEX_PSP_NONWLAN_INTERVAL",    //               196
        "COEX_PSP_STAT_2",      //                       197
        "COEX_PSP_RX_STATUS_STATE_2",   //               198
        "COEX_PSP_ERROR",       //                       199
        "COEX_T2BT",    //                               200
        "COEX_BT_DURATION", //                           201
        "COEX_TX_MCI_GPM_WLAN_SCHED_INFO_TRIG", //       202
        "COEX_TX_MCI_GPM_WLAN_SCHED_INFO_TRIG_RSP", //   203
        "COEX_TX_MCI_GPM_SCAN_OP",  //                   204
        "COEX_TX_MCI_GPM_BT_PAUSE_GPM_TX",	//       205
        "COEX_CTS2S_SEND",	//                       206
        "COEX_CTS2S_RESULT",	//                       207
        "COEX_ENTER_OCS",	//                       208
        "COEX_EXIT_OCS",	//                       209
        "COEX_UPDATE_OCS",	//                       210
        "COEX_STATUS_OCS",	//                       211
        "COEX_STATS_BT",	//                       212
        "COEX_MWS_WLAN_INIT",
        "COEX_MWS_WBTMR_SYNC",
        "COEX_MWS_TYPE2_RX",
        "COEX_MWS_TYPE2_TX",
        "COEX_MWS_WLAN_CHAVD",
        "COEX_MWS_WLAN_CHAVD_INSERT",
        "COEX_MWS_WLAN_CHAVD_MERGE",
        "COEX_MWS_WLAN_CHAVD_RPT",
        "COEX_MWS_CP_MSG_SEND",
        "COEX_MWS_CP_ESCAPE",
        "COEX_MWS_CP_UNFRAME",
        "COEX_MWS_CP_SYNC_UPDATE",
        "COEX_MWS_CP_SYNC",
        "COEX_MWS_CP_WLAN_STATE_IND",
        "COEX_MWS_CP_SYNCRESP_TIMEOUT",
        "COEX_MWS_SCHEME_UPDATE",
        "COEX_MWS_WLAN_EVENT",
        "COEX_MWS_UART_UNESCAPE",
        "COEX_MWS_UART_ENCODE_SEND",
        "COEX_MWS_UART_RECV_DECODE",
        "COEX_MWS_UL_HDL",
        "COEX_MWS_REMOTE_EVENT",
        "COEX_MWS_OTHER",
        "COEX_MWS_ERROR",
        "COEX_MWS_ANT_DIVERSITY",   //237
        "COEX_P2P_GO",
        "COEX_P2P_CLIENT",
        "COEX_SCC_1",
        "COEX_SCC_2",
        "COEX_MCC_1",
        "COEX_MCC_2",
        "COEX_TRF_SHAPE_NOA",
        "COEX_NOA_ONESHOT",
        "COEX_NOA_PERIODIC",
        "COEX_LE_1",
        "COEX_LE_2",
        "COEX_ANT_1",
        "COEX_ANT_2",
        "COEX_ENTER_NOA",
        "COEX_EXIT_NOA",
        "COEX_BT_SCAN_PROTECT", // 253
        "COEX_DEBUG_ID_END" // 254
    },
    {
        "ROAM_DBGID_DEFINITION_START",
        "ROAM_MODULE_INIT",
        "ROAM_DEV_START",
        "ROAM_CONFIG_RSSI_THRESH",
        "ROAM_CONFIG_SCAN_PERIOD",
        "ROAM_CONFIG_AP_PROFILE",
        "ROAM_CONFIG_CHAN_LIST",
        "ROAM_CONFIG_SCAN_PARAMS",
        "ROAM_CONFIG_RSSI_CHANGE",
        "ROAM_SCAN_TIMER_START",
        "ROAM_SCAN_TIMER_EXPIRE",
        "ROAM_SCAN_TIMER_STOP",
        "ROAM_SCAN_STARTED",
        "ROAM_SCAN_COMPLETE",
        "ROAM_SCAN_CANCELLED",
        "ROAM_CANDIDATE_FOUND",
        "ROAM_RSSI_ACTIVE_SCAN",
        "ROAM_RSSI_ACTIVE_ROAM",
        "ROAM_RSSI_GOOD",
        "ROAM_BMISS_FIRST_RECV",
        "ROAM_DEV_STOP",
        "ROAM_FW_OFFLOAD_ENABLE",
        "ROAM_CANDIDATE_SSID_MATCH",
        "ROAM_CANDIDATE_SECURITY_MATCH",
        "ROAM_LOW_RSSI_INTERRUPT",
        "ROAM_HIGH_RSSI_INTERRUPT",
        "ROAM_SCAN_REQUESTED",
        "ROAM_BETTER_CANDIDATE_FOUND",
        "ROAM_BETTER_AP_EVENT",
        "ROAM_CANCEL_LOW_PRIO_SCAN",
        "ROAM_FINAL_BMISS_RECVD",
        "ROAM_CONFIG_SCAN_MODE",
        "ROAM_BMISS_FINAL_SCAN_ENABLE",
        "ROAM_SUITABLE_AP_EVENT",
        "ROAM_RSN_IE_PARSE_ERROR",
        "ROAM_WPA_IE_PARSE_ERROR",
        "ROAM_SCAN_CMD_FROM_HOST",
        "ROAM_HO_SORT_CANDIDATE",
        "ROAM_HO_SAVE_CANDIDATE",
        "ROAM_HO_GET_CANDIDATE",
        "ROAM_HO_OFFLOAD_SET_PARAM",
        "ROAM_HO_SM",
        "ROAM_HO_HTT_SAVED",
        "ROAM_HO_SYNC_START",
        "ROAM_HO_START",
        "ROAM_HO_COMPLETE",
        "ROAM_HO_STOP",
        "ROAM_HO_HTT_FORWARD",
        "ROAM_DBGID_DEFINITION_END"
    },
    {
        "RESMGR_CHMGR_DEFINITION_START",
        "RESMGR_CHMGR_PAUSE_COMPLETE",
        "RESMGR_CHMGR_CHANNEL_CHANGE",
        "RESMGR_CHMGR_RESUME_COMPLETE",
        "RESMGR_CHMGR_VDEV_PAUSE",
        "RESMGR_CHMGR_VDEV_UNPAUSE",
        "RESMGR_CHMGR_CTS2S_TX_COMP",
        "RESMGR_CHMGR_CFEND_TX_COMP",
        "RESMGR_CHMGR_DEFINITION_END"
    },
    {
        "RESMGR_DEFINITION_START",
        "RESMGR_OCS_ALLOCRAM_SIZE",
        "RESMGR_OCS_RESOURCES",
        "RESMGR_LINK_CREATE",
        "RESMGR_LINK_DELETE",
        "RESMGR_OCS_CHREQ_CREATE",
        "RESMGR_OCS_CHREQ_DELETE",
        "RESMGR_OCS_CHREQ_START",
        "RESMGR_OCS_CHREQ_STOP",
        "RESMGR_OCS_SCHEDULER_INVOKED",
        "RESMGR_OCS_CHREQ_GRANT",
        "RESMGR_OCS_CHREQ_COMPLETE",
        "RESMGR_OCS_NEXT_TSFTIME",
        "RESMGR_OCS_TSF_TIMEOUT_US",
        "RESMGR_OCS_CURR_CAT_WINDOW",
        "RESMGR_OCS_CURR_CAT_WINDOW_REQ",
        "RESMGR_OCS_CURR_CAT_WINDOW_TIMESLOT",
        "RESMGR_OCS_CHREQ_RESTART",
        "RESMGR_OCS_CLEANUP_CH_ALLOCATORS",
        "RESMGR_OCS_PURGE_CHREQ",
        "RESMGR_OCS_CH_ALLOCATOR_FREE",
        "RESMGR_OCS_RECOMPUTE_SCHEDULE",
        "RESMGR_OCS_NEW_CAT_WINDOW_REQ",
        "RESMGR_OCS_NEW_CAT_WINDOW_TIMESLOT",
        "RESMGR_OCS_CUR_CH_ALLOC",
        "RESMGR_OCS_WIN_CH_ALLOC",
        "RESMGR_OCS_SCHED_CH_CHANGE",
        "RESMGR_OCS_CONSTRUCT_CAT_WIN",
        "RESMGR_OCS_CHREQ_PREEMPTED",
        "RESMGR_OCS_CH_SWITCH_REQ",
        "RESMGR_OCS_CHANNEL_SWITCHED",
        "RESMGR_OCS_CLEANUP_STALE_REQS",
        "RESMGR_OCS_CHREQ_UPDATE",
        "RESMGR_OCS_REG_NOA_NOTIF",
        "RESMGR_OCS_DEREG_NOA_NOTIF",
        "RESMGR_OCS_GEN_PERIODIC_NOA",
        "RESMGR_OCS_RECAL_QUOTAS",
        "RESMGR_OCS_GRANTED_QUOTA_STATS",
        "RESMGR_OCS_ALLOCATED_QUOTA_STATS",
        "RESMGR_OCS_REQ_QUOTA_STATS",
        "RESMGR_OCS_TRACKING_TIME_FIRED",
        "RESMGR_VC_ARBITRATE_ATTRIBUTES",
	"RESMGR_OCS_LATENCY_STRICT_TIME_SLOT",
	"RESMGR_OCS_CURR_TSF",
	"RESMGR_OCS_QUOTA_REM",
        "RESMGR_OCS_LATENCY_CASE_NO",
        "RESMGR_OCS_WIN_CAT_DUR",
        "RESMGR_VC_UPDATE_CUR_VC",
        "RESMGR_VC_REG_UNREG_LINK",
        "RESMGR_VC_PRINT_LINK",
        "RESMGR_OCS_MISS_TOLERANCE",
        "RESMGR_DYN_SCH_ALLOCRAM_SIZE",
        "RESMGR_DYN_SCH_ENABLE",
        "RESMGR_DYN_SCH_ACTIVE",
        "RESMGR_DYN_SCH_CH_STATS_START",
        "RESMGR_DYN_SCH_CH_SX_STATS",
        "RESMGR_DYN_SCH_TOT_UTIL_PER",
        "RESMGR_DYN_SCH_HOME_CH_QUOTA",
        "RESMGR_OCS_REG_RECAL_QUOTA_NOTIF",
        "RESMGR_OCS_DEREG_RECAL_QUOTA_NOTIF",
        "RESMGR_DEFINITION_END"
    },
    {
        "VDEV_MGR_DEBID_DEFINITION_START", /* vdev Mgr */
        "VDEV_MGR_FIRST_BEACON_MISS_DETECTED",
        "VDEV_MGR_FINAL_BEACON_MISS_DETECTED",
        "VDEV_MGR_BEACON_IN_SYNC",
        "VDEV_MGR_AP_KEEPALIVE_IDLE",
        "VDEV_MGR_AP_KEEPALIVE_INACTIVE",
        "VDEV_MGR_AP_KEEPALIVE_UNRESPONSIVE",
        "VDEV_MGR_AP_TBTT_CONFIG",
        "VDEV_MGR_FIRST_BCN_RECEIVED",
        "VDEV_MGR_VDEV_START",
        "VDEV_MGR_VDEV_UP",
        "VDEV_MGR_PEER_AUTHORIZED",
        "VDEV_MGR_OCS_HP_LP_REQ_POSTED",
        "VDEV_MGR_VDEV_START_OCS_HP_REQ_COMPLETE",
        "VDEV_MGR_VDEV_START_OCS_HP_REQ_STOP",
        "VDEV_MGR_HP_START_TIME",
        "VDEV_MGR_VDEV_PAUSE_DELAY_UPDATE",
        "VDEV_MGR_VDEV_PAUSE_FAIL",
        "VDEV_MGR_GEN_PERIODIC_NOA",
        "VDEV_MGR_OFF_CHAN_GO_CH_REQ_SETUP",
        "VDEV_MGR_DEFINITION_END",
    },
    {
      "SCAN_START_COMMAND_FAILED", /* scan */
      "SCAN_STOP_COMMAND_FAILED",
      "SCAN_EVENT_SEND_FAILED",
      "SCAN_ENGINE_START",
      "SCAN_ENGINE_CANCEL_COMMAND",
      "SCAN_ENGINE_STOP_DUE_TO_TIMEOUT",
      "SCAN_EVENT_SEND_TO_HOST",
      "SCAN_FWLOG_EVENT_ADD",
      "SCAN_FWLOG_EVENT_REM",
      "SCAN_FWLOG_EVENT_PREEMPTED",
      "SCAN_FWLOG_EVENT_RESTARTED",
      "SCAN_FWLOG_EVENT_COMPLETED",
    },
    {
        "RATECTRL_DBGID_DEFINITION_START", /* Rate ctrl*/
        "RATECTRL_DBGID_ASSOC",
        "RATECTRL_DBGID_NSS_CHANGE",
        "RATECTRL_DBGID_CHAINMASK_ERR",
        "RATECTRL_DBGID_UNEXPECTED_FRAME",
        "RATECTRL_DBGID_WAL_RCQUERY",
        "RATECTRL_DBGID_WAL_RCUPDATE",
        "RATECTRL_DBGID_GTX_UPDATE",
        "RATECTRL_DBGID_DEFINITION_END"
    },
    {
        "AP_PS_DBGID_DEFINITION_START",
        "AP_PS_DBGID_UPDATE_TIM",
        "AP_PS_DBGID_PEER_STATE_CHANGE",
        "AP_PS_DBGID_PSPOLL",
        "AP_PS_DBGID_PEER_CREATE",
        "AP_PS_DBGID_PEER_DELETE",
        "AP_PS_DBGID_VDEV_CREATE",
        "AP_PS_DBGID_VDEV_DELETE",
        "AP_PS_DBGID_SYNC_TIM",
        "AP_PS_DBGID_NEXT_RESPONSE",
        "AP_PS_DBGID_START_SP",
        "AP_PS_DBGID_COMPLETED_EOSP",
        "AP_PS_DBGID_TRIGGER",
        "AP_PS_DBGID_DUPLICATE_TRIGGER",
        "AP_PS_DBGID_UAPSD_RESPONSE",
        "AP_PS_DBGID_SEND_COMPLETE",
        "AP_PS_DBGID_SEND_N_COMPLETE",
        "AP_PS_DBGID_DETECT_OUT_OF_SYNC_STA",
        "AP_PS_DBGID_DELIVER_CAB",
    },
    {
        "" /* Block Ack */
    },
    /* Mgmt TxRx */
    {
        "MGMT_TXRX_DBGID_DEFINITION_START",
        "MGMT_TXRX_FORWARD_TO_HOST",
        "MGMT_TXRX_DBGID_DEFINITION_END",
    },
    { /* Data TxRx */
        "DATA_TXRX_DBGID_DEFINITION_START",
        "DATA_TXRX_DBGID_RX_DATA_SEQ_LEN_INFO",
        "DATA_TXRX_DBGID_DEFINITION_END",
    },
    { "" /* HTT */
    },
    { "" /* HOST */
    },
    { "" /* BEACON */
      "BEACON_EVENT_SWBA_SEND_FAILED",
      "BEACON_EVENT_EARLY_RX_BMISS_STATUS",
      "BEACON_EVENT_EARLY_RX_SLEEP_SLOP",
      "BEACON_EVENT_EARLY_RX_CONT_BMISS_TIMEOUT",
      "BEACON_EVENT_EARLY_RX_PAUSE_SKIP_BCN_NUM",
      "BEACON_EVENT_EARLY_RX_CLK_DRIFT",
      "BEACON_EVENT_EARLY_RX_AP_DRIFT",
      "BEACON_EVENT_EARLY_RX_BCN_TYPE",
    },
    { /* Offload Mgr */
        "OFFLOAD_MGR_DBGID_DEFINITION_START",
        "OFFLOADMGR_REGISTER_OFFLOAD",
        "OFFLOADMGR_DEREGISTER_OFFLOAD",
        "OFFLOADMGR_NO_REG_DATA_HANDLERS",
        "OFFLOADMGR_NO_REG_EVENT_HANDLERS",
        "OFFLOADMGR_REG_OFFLOAD_FAILED",
        "OFFLOADMGR_DBGID_DEFINITION_END",
    },
    {
        "WAL_DBGID_DEFINITION_START",
        "WAL_DBGID_FAST_WAKE_REQUEST",
        "WAL_DBGID_FAST_WAKE_RELEASE",
        "WAL_DBGID_SET_POWER_STATE",
        "WAL_DBGID_MISSING",
        "WAL_DBGID_CHANNEL_CHANGE_FORCE_RESET",
        "WAL_DBGID_CHANNEL_CHANGE",
        "WAL_DBGID_VDEV_START",
        "WAL_DBGID_VDEV_STOP",
        "WAL_DBGID_VDEV_UP",
        "WAL_DBGID_VDEV_DOWN",
        "WAL_DBGID_SW_WDOG_RESET",
        "WAL_DBGID_TX_SCH_REGISTER_TIDQ",
        "WAL_DBGID_TX_SCH_UNREGISTER_TIDQ",
        "WAL_DBGID_TX_SCH_TICKLE_TIDQ",
        "WAL_DBGID_XCESS_FAILURES",
        "WAL_DBGID_AST_ADD_WDS_ENTRY",
        "WAL_DBGID_AST_DEL_WDS_ENTRY",
        "WAL_DBGID_AST_WDS_ENTRY_PEER_CHG",
        "WAL_DBGID_AST_WDS_SRC_LEARN_FAIL",
        "WAL_DBGID_STA_KICKOUT",
        "WAL_DBGID_BAR_TX_FAIL",
        "WAL_DBGID_BAR_ALLOC_FAIL",
        "WAL_DBGID_LOCAL_DATA_TX_FAIL",
        "WAL_DBGID_SECURITY_PM4_QUEUED",
        "WAL_DBGID_SECURITY_GM1_QUEUED",
        "WAL_DBGID_SECURITY_PM4_SENT",
        "WAL_DBGID_SECURITY_ALLOW_DATA",
        "WAL_DBGID_SECURITY_UCAST_KEY_SET",
        "WAL_DBGID_SECURITY_MCAST_KEY_SET",
        "WAL_DBGID_SECURITY_ENCR_EN",
        "WAL_DBGID_BB_WDOG_TRIGGERED",
        "WAL_DBGID_RX_LOCAL_BUFS_LWM",
        "WAL_DBGID_RX_LOCAL_DROP_LARGE_MGMT",
        "WAL_DBGID_VHT_ILLEGAL_RATE_PHY_ERR_DETECTED",
        "WAL_DBGID_DEV_RESET",
        "WAL_DBGID_TX_BA_SETUP",
        "WAL_DBGID_RX_BA_SETUP",
        "WAL_DBGID_DEV_TX_TIMEOUT",
        "WAL_DBGID_DEV_RX_TIMEOUT",
        "WAL_DBGID_STA_VDEV_XRETRY",
        "WAL_DBGID_DCS",
        "WAL_DBGID_MGMT_TX_FAIL",
        "WAL_DBGID_SET_M4_SENT_MANUALLY",
        "WAL_DBGID_PROCESS_4_WAY_HANDSHAKE",
        "WAL_DBGID_WAL_CHANNEL_CHANGE_START",
        "WAL_DBGID_WAL_CHANNEL_CHANGE_COMPLETE",
        "WAL_DBGID_WHAL_CHANNEL_CHANGE_START",
        "WAL_DBGID_WHAL_CHANNEL_CHANGE_COMPLETE",
        "WAL_DBGID_TX_MGMT_DESCID_SEQ_TYPE_LEN",
        "WAL_DBGID_TX_DATA_MSDUID_SEQ_TYPE_LEN",
        "WAL_DBGID_TX_DISCARD",
        "WAL_DBGID_TX_MGMT_COMP_DESCID_STATUS",
        "WAL_DBGID_TX_DATA_COMP_MSDUID_STATUS",
        "WAL_DBGID_RESET_PCU_CYCLE_CNT",
        "WAL_DBGID_SETUP_RSSI_INTERRUPTS",
        "WAL_DBGID_BRSSI_CONFIG",
        "WAL_DBGID_CURRENT_BRSSI_AVE",
        "WAL_DBGID_BCN_TX_COMP",
        "WAL_DBGID_SET_HW_CHAINMASK",
        "WAL_DBGID_SET_HW_CHAINMASK_TXRX_STOP_FAIL",
        "WAL_DBGID_GET_HW_CHAINMASK",
        "WAL_DBGID_SMPS_DISABLE",
        "WAL_DBGID_SMPS_ENABLE_HW_CNTRL",
        "WAL_DBGID_SMPS_SWSEL_CHAINMASK",
        "WAL_DBGID_DEFINITION_END",
    },
    {
        "" /* DE */
    },
    {
        "" /* pcie lp */
    },
    {
        /* RTT */
        "RTT_CALL_FLOW",
        "RTT_REQ_SUB_TYPE",
        "RTT_MEAS_REQ_HEAD",
        "RTT_MEAS_REQ_BODY",
        "",
        "",
        "RTT_INIT_GLOBAL_STATE",
        "",
        "RTT_REPORT",
        "",
        "RTT_ERROR_REPORT",
        "RTT_TIMER_STOP",
        "RTT_SEND_TM_FRAME",
        "RTT_V3_RESP_CNT",
        "RTT_V3_RESP_FINISH",
        "RTT_CHANNEL_SWITCH_REQ",
        "RTT_CHANNEL_SWITCH_GRANT",
        "RTT_CHANNEL_SWITCH_COMPLETE",
        "RTT_CHANNEL_SWITCH_PREEMPT",
        "RTT_CHANNEL_SWITCH_STOP",
        "RTT_TIMER_START",
    },
    {      /* RESOURCE */
        "RESOURCE_DBGID_DEFINITION_START",
        "RESOURCE_PEER_ALLOC",
        "RESOURCE_PEER_FREE",
        "RESOURCE_PEER_ALLOC_WAL_PEER",
        "RESOURCE_DBGID_DEFINITION_END",
    },
    { /* DCS */
        "WLAN_DCS_DBGID_INIT",
        "WLAN_DCS_DBGID_WMI_CWINT",
        "WLAN_DCS_DBGID_TIMER",
        "WLAN_DCS_DBGID_CMDG",
        "WLAN_DCS_DBGID_CMDS",
        "WLAN_DCS_DBGID_DINIT"
    },
    {   /* CACHEMGR  */
        ""
    },
    {   /* ANI  */
        "ANI_DBGID_POLL",
        "ANI_DBGID_CONTROL",
        "ANI_DBGID_OFDM_PARAMS",
        "ANI_DBGID_CCK_PARAMS",
        "ANI_DBGID_RESET",
        "ANI_DBGID_RESTART",
        "ANI_DBGID_OFDM_LEVEL",
        "ANI_DBGID_CCK_LEVEL",
        "ANI_DBGID_FIRSTEP",
        "ANI_DBGID_CYCPWR",
        "ANI_DBGID_MRC_CCK",
        "ANI_DBGID_SELF_CORR_LOW",
        "ANI_DBGID_ENABLE",
        "ANI_DBGID_CURRENT_LEVEL",
        "ANI_DBGID_POLL_PERIOD",
        "ANI_DBGID_LISTEN_PERIOD",
        "ANI_DBGID_OFDM_LEVEL_CFG",
        "ANI_DBGID_CCK_LEVEL_CFG"
    },
    {
        "P2P_DBGID_DEFINITION_START",
        "P2P_DEV_REGISTER",
        "P2P_HANDLE_NOA",
        "P2P_UPDATE_SCHEDULE_OPPS",
        "P2P_UPDATE_SCHEDULE",
        "P2P_UPDATE_START_TIME",
        "P2P_UPDATE_START_TIME_DIFF_TSF32",
        "P2P_UPDATE_START_TIME_FINAL",
        "P2P_SETUP_SCHEDULE_TIMER",
        "P2P_PROCESS_SCHEDULE_AFTER_CALC",
        "P2P_PROCESS_SCHEDULE_STARTED_TIMER",
        "P2P_CALC_SCHEDULES_FIRST_CALL_ALL_NEXT_EVENT",
        "P2P_CALC_SCHEDULES_FIRST_VALUE",
        "P2P_CALC_SCHEDULES_EARLIEST_NEXT_EVENT",
        "P2P_CALC_SCHEDULES_SANITY_COUNT",
        "P2P_CALC_SCHEDULES_CALL_ALL_NEXT_EVENT_FROM_WHILE_LOOP",
        "P2P_CALC_SCHEDULES_TIMEOUT_1",
        "P2P_CALC_SCHEDULES_TIMEOUT_2",
        "P2P_FIND_ALL_NEXT_EVENTS_REQ_EXPIRED",
        "P2P_FIND_ALL_NEXT_EVENTS_REQ_ACTIVE",
        "P2P_FIND_NEXT_EVENT_REQ_NOT_STARTED",
        "P2P_FIND_NEXT_EVENT_REQ_COMPLETE_NON_PERIODIC",
        "P2P_FIND_NEXT_EVENT_IN_MID_OF_NOA",
        "P2P_FIND_NEXT_EVENT_REQ_COMPLETE",
        "P2P_SCHEDULE_TIMEOUT",
        "P2P_CALC_SCHEDULES_ENTER",
        "P2P_PROCESS_SCHEDULE_ENTER",
        "P2P_FIND_ALL_NEXT_EVENTS_INDIVIDUAL_REQ_AFTER_CHANGE",
        "P2P_FIND_ALL_NEXT_EVENTS_INDIVIDUAL_REQ_BEFORE_CHANGE",
        "P2P_FIND_ALL_NEXT_EVENTS_ENTER",
        "P2P_FIND_NEXT_EVENT_ENTER",
        "P2P_NOA_GO_PRESENT",
        "P2P_NOA_GO_ABSENT",
        "P2P_GO_NOA_NOTIF",
        "P2P_GO_TBTT_OFFSET",
        "P2P_GO_GET_NOA_INFO",
        "P2P_GO_ADD_ONE_SHOT_NOA",
        "P2P_GO_GET_NOA_IE",
        "P2P_GO_BCN_TX_COMP",
        "P2P_DBGID_DEFINITION_END",
    },
    {
        "CSA_DBGID_DEFINITION_START",
        "CSA_OFFLOAD_POOL_INIT",
        "CSA_OFFLOAD_REGISTER_VDEV",
        "CSA_OFFLOAD_DEREGISTER_VDEV",
        "CSA_DEREGISTER_VDEV_ERROR",
        "CSA_OFFLOAD_BEACON_RECEIVED",
        "CSA_OFFLOAD_BEACON_CSA_RECV",
        "CSA_OFFLOAD_CSA_RECV_ERROR_IE",
        "CSA_OFFLOAD_CSA_TIMER_ERROR",
        "CSA_OFFLOAD_CSA_TIMER_EXP",
        "CSA_OFFLOAD_WMI_EVENT_ERROR",
        "CSA_OFFLOAD_WMI_EVENT_SENT",
        "CSA_OFFLOAD_WMI_CHANSWITCH_RECV",
        "CSA_DBGID_DEFINITION_END",
    },
    {   /* NLO offload */
        ""
    },
    {
        "WLAN_CHATTER_DBGID_DEFINITION_START",
        "WLAN_CHATTER_ENTER",
        "WLAN_CHATTER_EXIT",
        "WLAN_CHATTER_FILTER_HIT",
        "WLAN_CHATTER_FILTER_MISS",
        "WLAN_CHATTER_FILTER_FULL",
        "WLAN_CHATTER_FILTER_TM_ADJ",
        "WLAN_CHATTER_BUFFER_FULL",
        "WLAN_CHATTER_TIMEOUT",
        "WLAN_CHATTER_DBGID_DEFINITION_END",
    },
    {
        "WOW_DBGID_DEFINITION_START",
        "WOW_ENABLE_CMDID",
        "WOW_RECV_DATA_PKT",
        "WOW_WAKE_HOST_DATA",
        "WOW_RECV_MGMT",
        "WOW_WAKE_HOST_MGMT",
        "WOW_RECV_EVENT",
        "WOW_WAKE_HOST_EVENT",
        "WOW_INIT",
        "WOW_RECV_MAGIC_PKT",
        "WOW_RECV_BITMAP_PATTERN",
        "WOW_DBGID_DEFINITION_END",
    },
    {   /* WAL_VDEV */
        ""
    },
    {   /* WAL_PDEV */
        ""
    },
    {   /* TEST  */
        "TP_CHANGE_CHANNEL",
        "TP_LOCAL_SEND",
    },
    {   /* STA SMPS  */
        "STA_SMPS_DBGID_DEFINITION_START",
        "STA_SMPS_DBGID_CREATE_PDEV_INSTANCE",
        "STA_SMPS_DBGID_CREATE_VIRTUAL_CHAN_INSTANCE",
        "STA_SMPS_DBGID_DELETE_VIRTUAL_CHAN_INSTANCE",
        "STA_SMPS_DBGID_CREATE_STA_INSTANCE",
        "STA_SMPS_DBGID_DELETE_STA_INSTANCE",
        "STA_SMPS_DBGID_VIRTUAL_CHAN_SMPS_START",
        "STA_SMPS_DBGID_VIRTUAL_CHAN_SMPS_STOP",
        "STA_SMPS_DBGID_SEND_SMPS_ACTION_FRAME",
        "STA_SMPS_DBGID_DTIM_EBT_EVENT_CHMASK_UPDATE",
        "STA_SMPS_DBGID_DTIM_CHMASK_UPDATE",
        "STA_SMPS_DBGID_DTIM_BEACON_EVENT_CHMASK_UPDATE",
        "STA_SMPS_DBGID_DTIM_POWER_STATE_CHANGE",
        "STA_SMPS_DBGID_DTIM_CHMASK_UPDATE_SLEEP",
        "STA_SMPS_DBGID_DTIM_CHMASK_UPDATE_AWAKE",
        "SMPS_DBGID_DEFINITION_END",
    },
    {   /* SWBMISS */
        "SWBMISS_DBGID_DEFINITION_START",
        "SWBMISS_ENABLED",
        "SWBMISS_DISABLED",
        "SWBMISS_DBGID_DEFINITION_END",
    },
    {   /* WMMAC */
        ""
    },
    {   /* TDLS */
        "TDLS_DBGID_DEFINITION_START",
        "TDLS_DBGID_VDEV_CREATE",
        "TDLS_DBGID_VDEV_DELETE",
        "TDLS_DBGID_ENABLED_PASSIVE",
        "TDLS_DBGID_ENABLED_ACTIVE",
        "TDLS_DBGID_DISABLED",
        "TDLS_DBGID_CONNTRACK_TIMER",
        "TDLS_DBGID_WAL_SET",
        "TDLS_DBGID_WAL_GET",
        "TDLS_DBGID_WAL_PEER_UPDATE_SET",
        "TDLS_DBGID_WAL_PEER_UPDATE_EVT",
        "TDLS_DBGID_WAL_VDEV_CREATE",
        "TDLS_DBGID_WAL_VDEV_DELETE",
        "TDLS_DBGID_WLAN_EVENT",
        "TDLS_DBGID_WLAN_PEER_UPDATE_SET",
        "TDLS_DBGID_PEER_EVT_DRP_THRESH",
        "TDLS_DBGID_PEER_EVT_DRP_RATE",
        "TDLS_DBGID_PEER_EVT_DRP_RSSI",
        "TDLS_DBGID_PEER_EVT_DISCOVER",
        "TDLS_DBGID_PEER_EVT_DELETE",
        "TDLS_DBGID_PEER_CAP_UPDATE",
        "TDLS_DBGID_UAPSD_SEND_PTI_FRAME",
        "TDLS_DBGID_UAPSD_SEND_PTI_FRAME2PEER",
        "TDLS_DBGID_UAPSD_START_PTR_TIMER",
        "TDLS_DBGID_UAPSD_CANCEL_PTR_TIMER",
        "TDLS_DBGID_UAPSD_PTR_TIMER_TIMEOUT",
        "TDLS_DBGID_UAPSD_STA_PS_EVENT_HANDLER",
        "TDLS_DBGID_UAPSD_PEER_EVENT_HANDLER",
        "TDLS_DBGID_UAPSD_PS_DEFAULT_SETTINGS",
        "TDLS_DBGID_UAPSD_GENERIC",
    },
    {   /* HB */
        "WLAN_HB_DBGID_DEFINITION_START",
        "WLAN_HB_DBGID_INIT",
        "WLAN_HB_DBGID_TCP_GET_TXBUF_FAIL",
        "WLAN_HB_DBGID_TCP_SEND_FAIL",
        "WLAN_HB_DBGID_BSS_PEER_NULL",
        "WLAN_HB_DBGID_UDP_GET_TXBUF_FAIL",
        "WLAN_HB_DBGID_UDP_SEND_FAIL",
        "WLAN_HB_DBGID_WMI_CMD_INVALID_PARAM",
        "WLAN_HB_DBGID_WMI_CMD_INVALID_OP",
        "WLAN_HB_DBGID_WOW_NOT_ENTERED",
        "WLAN_HB_DBGID_ALLOC_SESS_FAIL",
        "WLAN_HB_DBGID_CTX_NULL",
        "WLAN_HB_DBGID_CHKSUM_ERR",
        "WLAN_HB_DBGID_UDP_TX",
        "WLAN_HB_DBGID_TCP_TX",
        "WLAN_HB_DBGID_DEFINITION_END",
    },
    {   /* TXBF */
        "TXBFEE_DBGID_START",
        "TXBFEE_DBGID_NDPA_RECEIVED",
        "TXBFEE_DBGID_HOST_CONFIG_TXBFEE_TYPE",
        "TXBFER_DBGID_SEND_NDPA",
        "TXBFER_DBGID_GET_NDPA_BUF_FAIL",
        "TXBFER_DBGID_SEND_NDPA_FAIL",
        "TXBFER_DBGID_GET_NDP_BUF_FAIL",
        "TXBFER_DBGID_SEND_NDP_FAIL",
        "TXBFER_DBGID_GET_BRPOLL_BUF_FAIL",
        "TXBFER_DBGID_SEND_BRPOLL_FAIL",
        "TXBFER_DBGID_HOST_CONFIG_CMDID",
        "TXBFEE_DBGID_HOST_CONFIG_CMDID",
        "TXBFEE_DBGID_ENABLED_ENABLED_UPLOAD_H",
        "TXBFEE_DBGID_UPLOADH_CV_TAG",
        "TXBFEE_DBGID_UPLOADH_H_TAG",
        "TXBFEE_DBGID_CAPTUREH_RECEIVED",
        "TXBFEE_DBGID_PACKET_IS_STEERED",
        "TXBFEE_UPLOADH_EVENT_ALLOC_MEM_FAIL",
        "TXBFEE_DBGID_END",
    },
    { /*BATCH SCAN*/
    },
    { /*THERMAL MGR*/
        "THERMAL_MGR_DBGID_DEFINITION_START",
        "THERMAL_MGR_NEW_THRESH",
        "THERMAL_MGR_THRESH_CROSSED",
        "THERMAL_MGR_DBGID_DEFINITION END",
    },
    {   /* WLAN_MODULE_PHYERR_DFS */
        ""
    },
    {
        /* WLAN_MODULE_RMC */
        "RMC_DBGID_DEFINITION_START",
        "RMC_CREATE_INSTANCE",
        "RMC_DELETE_INSTANCE",
        "RMC_LDR_SEL",
        "RMC_NO_LDR",
        "RMC_LDR_NOT_SEL",
        "RMC_LDR_INF_SENT",
        "RMC_PEER_ADD",
        "RMC_PEER_DELETE",
        "RMC_PEER_UNKNOWN",
        "RMC_SET_MODE",
        "RMC_SET_ACTION_PERIOD",
        "RMC_ACRION_FRAME_RX",
        "RMC_DBGID_DEFINITION_END",
    },
    {
        /* WLAN_MODULE_STATS */
        "WLAN_STATS_DBGID_DEFINITION_START",
        "WLAN_STATS_DBGID_EST_LINKSPEED_VDEV_EN_DIS",
        "WLAN_STATS_DBGID_EST_LINKSPEED_CHAN_TIME_START",
        "WLAN_STATS_DBGID_EST_LINKSPEED_CHAN_TIME_END",
        "WLAN_STATS_DBGID_EST_LINKSPEED_CALC",
        "WLAN_STATS_DBGID_EST_LINKSPEED_UPDATE_HOME_CHAN",
        "WLAN_STATS_DBGID_DEFINITION_END",
    },
    {
        /* WLAN_MODULE_NAN */
    },
    {
        /* WLAN_MODULE_IBSS_PWRSAVE */
        "IBSS_PS_DBGID_DEFINITION_START",
        "IBSS_PS_DBGID_PEER_CREATE",
        "IBSS_PS_DBGID_PEER_DELETE",
        "IBSS_PS_DBGID_VDEV_CREATE",
        "IBSS_PS_DBGID_VDEV_DELETE",
        "IBSS_PS_DBGID_VDEV_EVENT",
        "IBSS_PS_DBGID_PEER_EVENT",
        "IBSS_PS_DBGID_DELIVER_CAB",
        "IBSS_PS_DBGID_DELIVER_UC_DATA",
        "IBSS_PS_DBGID_DELIVER_UC_DATA_ERROR",
        "IBSS_PS_DBGID_UC_INACTIVITY_TMR_RESTART",
        "IBSS_PS_DBGID_MC_INACTIVITY_TMR_RESTART",
        "IBSS_PS_DBGID_NULL_TX_COMPLETION",
        "IBSS_PS_DBGID_ATIM_TIMER_START",
        "IBSS_PS_DBGID_UC_ATIM_SEND",
        "IBSS_PS_DBGID_BC_ATIM_SEND",
        "IBSS_PS_DBGID_UC_TIMEOUT",
        "IBSS_PS_DBGID_PWR_COLLAPSE_ALLOWED",
        "IBSS_PS_DBGID_PWR_COLLAPSE_NOT_ALLOWED",
        "IBSS_PS_DBGID_SET_PARAM",
        "IBSS_PS_DBGID_HOST_TX_PAUSE",
        "IBSS_PS_DBGID_HOST_TX_UNPAUSE",
        "IBSS_PS_DBGID_PS_DESC_BIN_HWM",
        "IBSS_PS_DBGID_PS_DESC_BIN_LWM",
        "IBSS_PS_DBGID_PS_KICKOUT_PEER",
        "IBSS_PS_DBGID_SET_PEER_PARAM",
        "IBSS_PS_DBGID_BCN_ATIM_WIN_MISMATCH",
    },
    {
       /* HIF UART Interface DBGIDs */
       "HIF_UART_DBGID_START",
       "HIF_UART_DBGID_POWER_STATE",
       "HIF_UART_DBGID_TXRX_FLOW",
       "HIF_UART_DBGID_TXRX_CTRL_CHAR",
       "HIF_UART_DBGID_TXRX_BUF_DUMP",
    },
    {
       /* LPI */
       ""
    },
    {
       /* EXTSCAN DBGIDs */
       "EXTSCAN_START",
       "EXTSCAN_STOP",
       "EXTSCAN_CLEAR_ENTRY_CONTENT",
       "EXTSCAN_GET_FREE_ENTRY_SUCCESS",
       "EXTSCAN_GET_FREE_ENTRY_INCONSISTENT",
       "EXTSCAN_GET_FREE_ENTRY_NO_MORE_ENTRIES",
       "EXTSCAN_CREATE_ENTRY_SUCCESS",
       "EXTSCAN_CREATE_ENTRY_ERROR",
       "EXTSCAN_SEARCH_SCAN_ENTRY_QUEUE",
       "EXTSCAN_SEARCH_SCAN_ENTRY_KEY_FOUND",
       "EXTSCAN_SEARCH_SCAN_ENTRY_KEY_NOT_FOUND",
       "EXTSCAN_ADD_ENTRY",
       "EXTSCAN_BUCKET_SEND_OPERATION_EVENT",
       "EXTSCAN_BUCKET_SEND_OPERATION_EVENT_FAILED",
       "EXTSCAN_BUCKET_START_SCAN_CYCLE",
       "EXTSCAN_BUCKET_PERIODIC_TIMER",
       "EXTSCAN_SEND_START_STOP_EVENT",
       "EXTSCAN_NOTIFY_WLAN_CHANGE",
       "EXTSCAN_NOTIFY_WLAN_HOTLIST_MATCH",
       "EXTSCAN_MAIN_RECEIVED_FRAME",
       "EXTSCAN_MAIN_NO_SSID_IE",
       "EXTSCAN_MAIN_MALFORMED_FRAME",
       "EXTSCAN_FIND_BSSID_BY_REFERENCE",
       "EXTSCAN_FIND_BSSID_BY_REFERENCE_ERROR",
       "EXTSCAN_NOTIFY_TABLE_USAGE",
       "EXTSCAN_FOUND_RSSI_ENTRY",
       "EXTSCAN_BSSID_FOUND_RSSI_SAMPLE",
       "EXTSCAN_BSSID_ADDED_RSSI_SAMPLE",
       "EXTSCAN_BSSID_REPLACED_RSSI_SAMPLE",
       "EXTSCAN_BSSID_TRANSFER_CURRENT_SAMPLES",
       "EXTSCAN_BUCKET_PROCESS_SCAN_EVENT",
       "EXTSCAN_BUCKET_CANNOT_FIND_BUCKET",
       "EXTSCAN_START_SCAN_REQUEST_FAILED",
       "EXTSCAN_BUCKET_STOP_CURRENT_SCANS",
       "EXTSCAN_BUCKET_SCAN_STOP_REQUEST",
       "EXTSCAN_BUCKET_PERIODIC_TIMER_ERROR",
       "EXTSCAN_BUCKET_START_OPERATION",
       "EXTSCAN_START_INTERNAL_ERROR",
       "EXTSCAN_NOTIFY_HOTLIST_MATCH",
       "EXTSCAN_CONFIG_HOTLIST_TABLE",
       "EXTSCAN_CONFIG_WLAN_CHANGE_TABLE",
    },
    {  /* UNIT_TEST */
       "UNIT_TEST_GEN",
    },
    {  /* MLME */
       "MLME_DEBUG_CMN",
       "MLME_IF",
       "MLME_AUTH",
       "MLME_REASSOC",
       "MLME_DEAUTH",
       "MLME_DISASSOC",
       "MLME_ROAM",
       "MLME_RETRY",
       "MLME_TIMER",
       "MLME_FRMPARSE",
    },
    {  /*SUPPLICANT */
       "SUPPL_INIT",
       "SUPPL_RECV_EAPOL",
       "SUPPL_RECV_EAPOL_TIMEOUT",
       "SUPPL_SEND_EAPOL",
       "SUPPL_MIC_MISMATCH",
       "SUPPL_FINISH",
    },
};

static char *
dbglog_get_msg(A_UINT32 moduleid, A_UINT32 debugid)
{
    static char unknown_str[64];

    if (moduleid < WLAN_MODULE_ID_MAX && debugid < MAX_DBG_MSGS) {
        char *str = DBG_MSG_ARR[moduleid][debugid];
        if (str && str[0] != '\0') {
            return str;
        }
    }

    snprintf(unknown_str, sizeof(unknown_str),
            "UNKNOWN %u:%u",
            moduleid, debugid);

    return unknown_str;
}

void
dbglog_printf(
        A_UINT32 timestamp,
        A_UINT16 vap_id,
        const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    int j;

    if (vap_id < DBGLOG_MAX_VDEVID) {
        j = snprintf(buf, sizeof(buf), DBGLOG_PRINT_PREFIX
                     "[%u] vap-%u ", timestamp, vap_id);
    } else {
        j = snprintf(buf, sizeof(buf), DBGLOG_PRINT_PREFIX
                     "[%u] ", timestamp);
    }

    va_start(ap, fmt);
    vsnprintf(buf+j, sizeof(buf)-j, fmt, ap);
    va_end(ap);

    diag_print_legacy_logs(buf);
}

void
dbglog_printf_no_line_break(
        A_UINT32 timestamp,
        A_UINT16 vap_id,
        const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    int j;

    if (vap_id < DBGLOG_MAX_VDEVID) {
        j = snprintf(buf, sizeof(buf), DBGLOG_PRINT_PREFIX
                      "[%u] vap-%u ", timestamp, vap_id);
    } else {
        j = snprintf(buf, sizeof(buf), DBGLOG_PRINT_PREFIX
                     "[%u] ", timestamp);
    }

    va_start(ap, fmt);
    vsnprintf(buf+j, sizeof(buf)-j, fmt, ap);
    va_end(ap);

    diag_print_legacy_logs(buf);
}

#define USE_NUMERIC 0

A_BOOL
dbglog_default_print_handler(A_UINT32 mod_id, A_UINT16 vap_id, A_UINT32 dbg_id,
                             A_UINT32 timestamp, A_UINT16 numargs, A_UINT32 *args)
{
    int i, j;
    char tempbuf[RECLEN];

    if (vap_id < DBGLOG_MAX_VDEVID) {
        j = snprintf(tempbuf, sizeof(tempbuf), DBGLOG_PRINT_PREFIX
                     "[%u] vap-%u %s ( ", timestamp, vap_id,
                     dbglog_get_msg(mod_id, dbg_id));
    } else {
        j = snprintf(tempbuf, sizeof(tempbuf), DBGLOG_PRINT_PREFIX
                     "[%u] %s ( ", timestamp,
                     dbglog_get_msg(mod_id, dbg_id));
    }

    for (i = 0; i < numargs; i++) {
#if USE_NUMERIC
        j += snprintf(tempbuf+j, sizeof(tempbuf)-j, "%u", args[i]);
#else
        j += snprintf(tempbuf+j, sizeof(tempbuf)-j, "%#x,", args[i]);
#endif
        if ((i + 1) < numargs) {
            j += snprintf(tempbuf+j, sizeof(tempbuf)-j, ",");
        }
    }
    snprintf(tempbuf+j, sizeof(tempbuf)-j, ")\n");

    diag_print_legacy_logs(buf);
    return TRUE;
}

int
dbglog_parse_debug_logs(u_int8_t *datap, u_int16_t len, u_int16_t dropped)
{
    A_UINT32 count=0;
    A_UINT32 timestamp=0;
    A_UINT32 debugid=0;
    A_UINT32 moduleid=0;
    A_UINT16 vapid=0;
    A_UINT16 numargs=0;
    A_UINT32 *buffer;
    A_UINT32 length = len >> 2;

    if(dropped > 0)
        debug_printf("%d log buffer got dropped in firmware\n", dropped);

    buffer = (A_UINT32 *)datap;
    length = (len >> 2);

    while ((count + 2) < length) {

        timestamp = DBGLOG_GET_TIME_STAMP(buffer[count]);
        debugid = DBGLOG_GET_DBGID(buffer[count+1]);
        moduleid = DBGLOG_GET_MODULEID(buffer[count+1]);
        vapid = DBGLOG_GET_VDEVID(buffer[count+1]);
        numargs = DBGLOG_GET_NUMARGS(buffer[count+1]);

        if ((count + 2 + numargs) > length)
            return 0;

        if (moduleid >= WLAN_MODULE_ID_MAX)
            return 0;

        if (mod_print[moduleid] == NULL) {
            /* No module specific log registered use the default handler*/
            dbglog_default_print_handler(moduleid, vapid, debugid, timestamp,
                                         numargs,
                                         (((A_UINT32 *)buffer) + 2 + count));
        } else {
            if(!(mod_print[moduleid](moduleid, vapid, debugid, timestamp,
                            numargs,
                            (((A_UINT32 *)buffer) + 2 + count)))) {
                /* The message is not handled by the module specific handler*/
                dbglog_default_print_handler(moduleid, vapid, debugid, timestamp,
                        numargs,
                        (((A_UINT32 *)buffer) + 2 + count));

            }
        }

        count += numargs + 2; /* 32 bit Time stamp + 32 bit Dbg header*/
    }
    /* Always returns zero */
    return (0);
}

void
dbglog_reg_modprint(A_UINT32 mod_id, module_dbg_print printfn)
{
    if (!mod_print[mod_id]) {
        mod_print[mod_id] = printfn;
    } else {
        debug_printf("module print is already registered for this module %d\n",
               mod_id);
    }
}

void
dbglog_sm_print(
        A_UINT32 timestamp,
        A_UINT16 vap_id,
        A_UINT16 numargs,
        A_UINT32 *args,
        const char *module_prefix,
        const char *states[], A_UINT32 num_states,
        const char *events[], A_UINT32 num_events)
{
    A_UINT8 type, arg1, arg2, arg3;
    A_UINT32 extra, extra2, extra3;

    if (numargs != 4) {
        return;
    }

    type = (args[0] >> 24) & 0xff;
    arg1 = (args[0] >> 16) & 0xff;
    arg2 = (args[0] >>  8) & 0xff;
    arg3 = (args[0] >>  0) & 0xff;

    extra = args[1];
    extra2 = args[2];
    extra3 = args[3];

    switch (type) {
    case 0: /* state transition */
        if (arg1 < num_states && arg2 < num_states) {
            dbglog_printf(timestamp, vap_id, "%s: %s => %s (%#x, %#x, %#x)",
                    module_prefix, states[arg1], states[arg2], extra, extra2, extra3);
        } else {
            dbglog_printf(timestamp, vap_id, "%s: %u => %u (%#x, %#x, %#x)",
                    module_prefix, arg1, arg2, extra, extra2, extra3);
        }
        break;
    case 1: /* dispatch event */
        if (arg1 < num_states && arg2 < num_events) {
            dbglog_printf(timestamp, vap_id, "%s: %s < %s (%#x, %#x, %#x)",
                    module_prefix, states[arg1], events[arg2], extra, extra2, extra3);
        } else {
            dbglog_printf(timestamp, vap_id, "%s: %u < %u (%#x, %#x, %#x)",
                    module_prefix, arg1, arg2, extra, extra2, extra3);
        }
        break;
    case 2: /* warning */
        switch (arg1) {
        case 0: /* unhandled event */
            if (arg2 < num_states && arg3 < num_events) {
                dbglog_printf(timestamp, vap_id, "%s: unhandled event %s in state %s (%#x, %#x, %#x)",
                        module_prefix, events[arg3], states[arg2], extra, extra2, extra3);
            } else {
                dbglog_printf(timestamp, vap_id, "%s: unhandled event %u in state %u (%#x, %#x, %#x)",
                        module_prefix, arg3, arg2, extra, extra2, extra3);
            }
            break;
        default:
            break;

        }
        break;
    }
}

A_BOOL
dbglog_sta_powersave_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 *args)
{
    static const char *states[] = {
        "IDLE",
        "ACTIVE",
        "SLEEP_TXQ_FLUSH",
        "SLEEP_TX_SENT",
        "PAUSE",
        "SLEEP_DOZE",
        "SLEEP_AWAKE",
        "ACTIVE_TXQ_FLUSH",
        "ACTIVE_TX_SENT",
        "PAUSE_TXQ_FLUSH",
        "PAUSE_TX_SENT",
        "IDLE_TXQ_FLUSH",
        "IDLE_TX_SENT",
    };

    static const char *events[] = {
        "START",
        "STOP",
        "PAUSE",
        "UNPAUSE",
        "TIM",
        "DTIM",
        "SEND_COMPLETE",
        "PRE_SEND",
        "RX",
        "HWQ_EMPTY",
        "PAUSE_TIMEOUT",
        "TXRX_INACTIVITY_TIMEOUT",
        "PSPOLL_TIMEOUT",
        "UAPSD_TIMEOUT",
        "DELAYED_SLEEP_TIMEOUT",
        "SEND_N_COMPLETE",
        "TIDQ_PAUSE_COMPLETE",
        "SEND_PSPOLL",
        "SEND_SPEC_PSPOLL",
    };

    switch (dbg_id) {
    case DBGLOG_DBGID_SM_FRAMEWORK_PROXY_DBGLOG_MSG:
        dbglog_sm_print(timestamp, vap_id, numargs, args, "STA PS",
                states, ARRAY_LENGTH(states), events, ARRAY_LENGTH(events));
        break;
    case PS_STA_PM_ARB_REQUEST:
        if (numargs == 4) {
            dbglog_printf(timestamp, vap_id, "PM ARB request flags=%x, last_time=%x %s: %s",
                    args[1], args[2], dbglog_get_module_str(args[0]), args[3] ? "SLEEP" : "WAKE");
        }
        break;
    case PS_STA_DELIVER_EVENT:
        if (numargs == 2) {
            dbglog_printf(timestamp, vap_id, "STA PS: %s %s",
                    (args[0] == 0 ? "PAUSE_COMPLETE" :
                    (args[0] == 1 ? "UNPAUSE_COMPLETE" :
                    (args[0] == 2 ? "SLEEP" :
                    (args[0] == 3 ? "AWAKE" : "UNKNOWN")))),
                    (args[1] == 0 ? "SUCCESS" :
                     (args[1] == 1 ? "TXQ_FLUSH_TIMEOUT" :
                      (args[1] == 2 ? "NO_ACK" :
                       (args[1] == 3 ? "RX_LEAK_TIMEOUT" :
                        (args[1] == 4 ? "PSPOLL_UAPSD_BUSY_TIMEOUT" : "UNKNOWN"))))));
        }
        break;
    case PS_STA_PSPOLL_SEQ_DONE:
        if (numargs == 5) {
            dbglog_printf(timestamp, vap_id, "STA PS poll: queue=%u comp=%u rsp=%u rsp_dur=%u fc=%x qos=%x %s",
                    args[0], args[1], args[2], args[3],
                    (args[4] >> 16) & 0xffff,
                    (args[4] >> 8) & 0xff,
                    (args[4] & 0xff) == 0 ? "SUCCESS" :
                    (args[4] & 0xff) == 1 ? "NO_ACK" :
                    (args[4] & 0xff) == 2 ? "DROPPED" :
                    (args[4] & 0xff) == 3 ? "FILTERED" :
                    (args[4] & 0xff) == 4 ? "RSP_TIMEOUT" : "UNKNOWN");
        }
        break;
    case PS_STA_COEX_MODE:
        if (numargs == 1) {
            dbglog_printf(timestamp, vap_id, "STA PS COEX MODE %s",
                    args[0] ? "ENABLED" : "DISABLED");
        }
        break;
    case PS_STA_PSPOLL_ALLOW:
        if (numargs == 3) {
            dbglog_printf(timestamp, vap_id, "STA PS-Poll %s flags=%x time=%u",
                    args[0] ? "ALLOW" : "DISALLOW", args[1], args[2]);
        }
        break;
    case PS_STA_SET_PARAM:
        if (numargs == 2) {
            struct {
                char *name;
                int is_time_param;
            } params[] = {
                { "MAX_SLEEP_ATTEMPTS", 0 },
                { "DELAYED_SLEEP", 1 },
                { "TXRX_INACTIVITY", 1 },
                { "MAX_TX_BEFORE_WAKE", 0 },
                { "UAPSD_TIMEOUT", 1 },
                { "UAPSD_CONFIG", 0 },
                { "PSPOLL_RESPONSE_TIMEOUT", 1 },
                { "MAX_PSPOLL_BEFORE_WAKE", 0 },
                { "RX_WAKE_POLICY", 0 },
                { "DELAYED_PAUSE_RX_LEAK", 1 },
                { "TXRX_INACTIVITY_BLOCKED_RETRY", 1 },
                { "SPEC_WAKE_INTERVAL", 1 },
                { "MAX_SPEC_NODATA_PSPOLL", 0 },
                { "ESTIMATED_PSPOLL_RESP_TIME", 1 },
                { "QPOWER_MAX_PSPOLL_BEFORE_WAKE", 0 },
                { "QPOWER_ENABLE", 0 },
            };
            A_UINT32 param = args[0];
            A_UINT32 value = args[1];

            if (param < ARRAY_LENGTH(params)) {
                if (params[param].is_time_param) {
                    dbglog_printf(timestamp, vap_id, "STA PS SET_PARAM %s => %u (us)",
                            params[param].name, value);
                } else {
                    dbglog_printf(timestamp, vap_id, "STA PS SET_PARAM %s => %#x",
                            params[param].name, value);
        }
            } else {
                dbglog_printf(timestamp, vap_id, "STA PS SET_PARAM %x => %#x",
                        param, value);
            }
        }
        break;
    case PS_STA_SPECPOLL_TIMER_STARTED:
        dbglog_printf(timestamp, vap_id, "SPEC Poll Timer Started: Beacon time Remaining:%d wakeup interval:%d", args[0], args[1]);
        break;
    case PS_STA_SPECPOLL_TIMER_STOPPED:
        dbglog_printf(timestamp, vap_id,
                        "SPEC Poll Timer Stopped");
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

A_BOOL dbglog_ratectrl_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 *args)
{
    switch (dbg_id) {
        case RATECTRL_DBGID_ASSOC:
           dbglog_printf(timestamp, vap_id, "RATE: ChainMask %d, phymode %d, ni_flags 0x%08x, vht_mcs_set 0x%04x, ht_mcs_set 0x%04x",
              args[0], args[1], args[2], args[3], args[4]);
           break;
        case RATECTRL_DBGID_NSS_CHANGE:
           dbglog_printf(timestamp, vap_id, "RATE: NEW NSS %d\n", args[0]);
           break;
        case RATECTRL_DBGID_CHAINMASK_ERR:
           dbglog_printf(timestamp, vap_id, "RATE: Chainmask ERR %d %d %d\n",
               args[0], args[1], args[2]);
           break;
        case RATECTRL_DBGID_UNEXPECTED_FRAME:
           dbglog_printf(timestamp, vap_id, "RATE: WARN1: rate %d flags 0x%08x\n", args[0], args[1]);
           break;
        case RATECTRL_DBGID_WAL_RCQUERY:
            dbglog_printf(timestamp, vap_id, "ratectrl_dbgid_wal_rcquery [rix1 %d rix2 %d rix3 %d proberix %d ppduflag 0x%x] ",
                    args[0], args[1], args[2], args[3], args[4]);
            break;
        case RATECTRL_DBGID_WAL_RCUPDATE:
            dbglog_printf(timestamp, vap_id, "ratectrl_dbgid_wal_rcupdate [numelems %d ppduflag 0x%x] ",
                    args[0], args[1]);
            break;
        case RATECTRL_DBGID_GTX_UPDATE:
        {
             switch (args[0]) {
                 case 255:
                     dbglog_printf(timestamp, vap_id, "GtxInitPwrCfg [bw[last %d|cur %d] rtcode 0x%x tpc %d tpc_init_pwr_cfg %d] ",
                             args[1] >> 8, args[1] & 0xff, args[2], args[3], args[4]);
                     break;
                 case 254:
                     dbglog_printf(timestamp, vap_id, "gtx_cfg_addr [RTMask0@0x%x PERThreshold@0x%x gtxTPCMin@0x%x userGtxMask@0x%x] ",
                             args[1], args[2], args[3], args[4]);
                     break;
                 default:
                     dbglog_printf(timestamp, vap_id, "gtx_update [act %d bw %d rix 0x%x tpc %d per %d lastrssi %d] ",
                             args[0], args[1], args[2], args[3], args[4], args[5]);
              }
        }
	    break;

    }
    return TRUE;
}

A_BOOL dbglog_ani_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 *args)
{
  switch (dbg_id) {
   case ANI_DBGID_ENABLE:
        dbglog_printf(timestamp, vap_id,
        "ANI Enable:  %d", args[0]);
        break;
   case ANI_DBGID_POLL:
        dbglog_printf(timestamp, vap_id,
        "ANI POLLING: AccumListenTime %d ListenTime %d ofdmphyerr %d cckphyerr %d",
                args[0], args[1], args[2],args[3]);
        break;
   case ANI_DBGID_RESTART:
        dbglog_printf(timestamp, vap_id,"ANI Restart");
        break;
   case ANI_DBGID_CURRENT_LEVEL:
        dbglog_printf(timestamp, vap_id,
        "ANI CURRENT LEVEL ofdm level %d cck level %d",args[0],args[1]);
        break;
   case ANI_DBGID_OFDM_LEVEL:
        dbglog_printf(timestamp, vap_id,
        "ANI UPDATE ofdm level %d firstep %d firstep_low %d cycpwr_thr %d self_corr_low %d",
        args[0], args[1],args[2],args[3],args[4]);
        break;
   case ANI_DBGID_CCK_LEVEL:
        dbglog_printf(timestamp, vap_id,
                "ANI  UPDATE cck level %d firstep %d firstep_low %d mrc_cck %d",
                args[0],args[1],args[2],args[3]);
        break;
   case ANI_DBGID_CONTROL:
        dbglog_printf(timestamp, vap_id,
                "ANI CONTROL ofdmlevel %d ccklevel %d\n",
                args[0]);

        break;
   case ANI_DBGID_OFDM_PARAMS:
        dbglog_printf(timestamp, vap_id,
                "ANI ofdm_control firstep %d cycpwr %d\n",
                args[0],args[1]);
        break;
   case ANI_DBGID_CCK_PARAMS:
        dbglog_printf(timestamp, vap_id,
                "ANI cck_control mrc_cck %d barker_threshold %d\n",
                args[0],args[1]);
        break;
   case ANI_DBGID_RESET:
        dbglog_printf(timestamp, vap_id,
                "ANI resetting resetflag %d resetCause %8x channel index %d",
                args[0],args[1],args[2]);
        break;
   case ANI_DBGID_SELF_CORR_LOW:
        dbglog_printf(timestamp, vap_id,"ANI self_corr_low %d",args[0]);
        break;
   case ANI_DBGID_FIRSTEP:
        dbglog_printf(timestamp, vap_id,"ANI firstep %d firstep_low %d",
            args[0],args[1]);
        break;
   case ANI_DBGID_MRC_CCK:
        dbglog_printf(timestamp, vap_id,"ANI mrc_cck %d",args[0]);
        break;
   case ANI_DBGID_CYCPWR:
        dbglog_printf(timestamp, vap_id,"ANI cypwr_thresh %d",args[0]);
        break;
   case ANI_DBGID_POLL_PERIOD:
        dbglog_printf(timestamp, vap_id,"ANI Configure poll period to %d",args[0]);
        break;
   case ANI_DBGID_LISTEN_PERIOD:
        dbglog_printf(timestamp, vap_id,"ANI Configure listen period to %d",args[0]);
        break;
   case ANI_DBGID_OFDM_LEVEL_CFG:
        dbglog_printf(timestamp, vap_id,"ANI Configure ofdm level to %d",args[0]);
        break;
   case ANI_DBGID_CCK_LEVEL_CFG:
        dbglog_printf(timestamp, vap_id,"ANI Configure cck level to %d",args[0]);
        break;
   default:
        dbglog_printf(timestamp, vap_id,"ANI arg1 %d arg2 %d arg3 %d",
              args[0],args[1],args[2]);
        break;
  }
    return TRUE;
}

A_BOOL
dbglog_ap_powersave_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 *args)
{
    switch (dbg_id) {
    case AP_PS_DBGID_UPDATE_TIM:
        if (numargs == 2) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: TIM update AID=%u %s",
                    args[0], args[1] ? "set" : "clear");
        }
        break;
    case AP_PS_DBGID_PEER_STATE_CHANGE:
        if (numargs == 2) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u power save %s",
                    args[0], args[1] ? "enabled" : "disabled");
        }
        break;
    case AP_PS_DBGID_PSPOLL:
        if (numargs == 3) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u pspoll response tid=%u flags=%x",
                    args[0], args[1], args[2]);
        }
        break;
    case AP_PS_DBGID_PEER_CREATE:
        if (numargs == 1) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: create peer AID=%u", args[0]);
        }
        break;
    case AP_PS_DBGID_PEER_DELETE:
        if (numargs == 1) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: delete peer AID=%u", args[0]);
        }
        break;
    case AP_PS_DBGID_VDEV_CREATE:
        dbglog_printf(timestamp, vap_id, "AP PS: vdev create");
        break;
    case AP_PS_DBGID_VDEV_DELETE:
        dbglog_printf(timestamp, vap_id, "AP PS: vdev delete");
        break;
    case AP_PS_DBGID_SYNC_TIM:
        if (numargs == 3) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u advertised=%#x buffered=%#x", args[0], args[1], args[2]);
        }
        break;
    case AP_PS_DBGID_NEXT_RESPONSE:
        if (numargs == 4) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u select next response %s%s%s", args[0],
                    args[1] ? "(usp active) " : "",
                    args[2] ? "(pending usp) " : "",
                    args[3] ? "(pending poll response)" : "");
        }
        break;
    case AP_PS_DBGID_START_SP:
        if (numargs == 3) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u START SP tsf=%#x (%u)",
                    args[0], args[1], args[2]);
        }
        break;
    case AP_PS_DBGID_COMPLETED_EOSP:
        if (numargs == 3) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u EOSP eosp_tsf=%#x trigger_tsf=%#x",
                    args[0], args[1], args[2]);
        }
        break;
    case AP_PS_DBGID_TRIGGER:
        if (numargs == 4) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u TRIGGER tsf=%#x %s%s", args[0], args[1],
                    args[2] ? "(usp active) " : "",
                    args[3] ? "(send_n in progress)" : "");
        }
        break;
    case AP_PS_DBGID_DUPLICATE_TRIGGER:
        if (numargs == 4) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u DUP TRIGGER tsf=%#x seq=%u ac=%u",
                    args[0], args[1], args[2], args[3]);
        }
        break;
    case AP_PS_DBGID_UAPSD_RESPONSE:
        if (numargs == 5) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u UAPSD response tid=%u, n_mpdu=%u flags=%#x max_sp=%u current_sp=%u",
                    args[0], args[1], args[2], args[3], (args[4] >> 16) & 0xffff, args[4] & 0xffff);
        }
        break;
    case AP_PS_DBGID_SEND_COMPLETE:
        if (numargs == 5) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u SEND_COMPLETE fc=%#x qos=%#x %s%s",
                    args[0], args[1], args[2],
                    args[3] ? "(usp active) " : "",
                    args[4] ? "(pending poll response)" : "");
        }
        break;
    case AP_PS_DBGID_SEND_N_COMPLETE:
        if (numargs == 3) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u SEND_N_COMPLETE %s%s",
                    args[0],
                    args[1] ? "(usp active) " : "",
                    args[2] ? "(pending poll response)" : "");
        }
        break;
    case AP_PS_DBGID_DETECT_OUT_OF_SYNC_STA:
        if (numargs == 4) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: AID=%u detected out-of-sync now=%u tx_waiting=%u txq_depth=%u",
                   args[0], args[1], args[2], args[3]);
        }
        break;
    case AP_PS_DBGID_DELIVER_CAB:
        if (numargs == 4) {
            dbglog_printf(timestamp, vap_id,
                    "AP PS: CAB %s n_mpdus=%u, flags=%x, extra=%u",
                   (args[0] == 17) ? "MGMT" : "DATA", args[1], args[2], args[3]);
        }
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

A_BOOL
dbglog_wal_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 *args)
{
    static const char *states[] = {
        "ACTIVE",
        "WAIT",
        "WAIT_FILTER",
        "PAUSE",
        "PAUSE_SEND_N",
        "BLOCK",
    };

    static const char *events[] = {
        "PAUSE",
        "PAUSE_FILTER",
        "UNPAUSE",

        "BLOCK",
        "BLOCK_FILTER",
        "UNBLOCK",

        "HWQ_EMPTY",
        "ALLOW_N",
    };

#define WAL_VDEV_TYPE(type)     \
    (type == 0 ? "AP" :       \
    (type == 1 ? "STA" :        \
    (type == 2 ? "IBSS" :         \
    (type == 2 ? "MONITOR" :    \
     "UNKNOWN"))))

#define WAL_SLEEP_STATE(state)      \
    (state == 1 ? "NETWORK SLEEP" : \
    (state == 2 ? "AWAKE" :         \
    (state == 3 ? "SYSTEM SLEEP" :  \
    "UNKNOWN")))

    switch (dbg_id) {
    case DBGLOG_DBGID_SM_FRAMEWORK_PROXY_DBGLOG_MSG:
        dbglog_sm_print(timestamp, vap_id, numargs, args, "TID PAUSE",
                states, ARRAY_LENGTH(states), events, ARRAY_LENGTH(events));
        break;
    case WAL_DBGID_SET_POWER_STATE:
        if (numargs == 3) {
            dbglog_printf(timestamp, vap_id,
                    "WAL %s => %s, req_count=%u",
                    WAL_SLEEP_STATE(args[0]), WAL_SLEEP_STATE(args[1]),
                    args[2]);
        }
        break;
    case WAL_DBGID_CHANNEL_CHANGE_FORCE_RESET:
        if (numargs == 4) {
            dbglog_printf(timestamp, vap_id,
                    "WAL channel change (force reset) freq=%u, flags=%u mode=%u rx_ok=%u tx_ok=%u",
                    args[0] & 0x0000ffff, (args[0] & 0xffff0000) >> 16, args[1],
                    args[2], args[3]);
        }
        break;
    case WAL_DBGID_CHANNEL_CHANGE:
        if (numargs == 2) {
            dbglog_printf(timestamp, vap_id,
                    "WAL channel change freq=%u, mode=%u flags=%u rx_ok=1 tx_ok=1",
                    args[0] & 0x0000ffff, (args[0] & 0xffff0000) >> 16, args[1]);
        }
        break;
    case WAL_DBGID_VDEV_START:
        if (numargs == 1) {
            dbglog_printf(timestamp, vap_id, "WAL %s vdev started",
                    WAL_VDEV_TYPE(args[0]));
        }
        break;
    case WAL_DBGID_VDEV_STOP:
        dbglog_printf(timestamp, vap_id, "WAL %s vdev stopped",
                WAL_VDEV_TYPE(args[0]));
        break;
    case WAL_DBGID_VDEV_UP:
        dbglog_printf(timestamp, vap_id, "WAL %s vdev up, count=%u",
                WAL_VDEV_TYPE(args[0]), args[1]);
        break;
    case WAL_DBGID_VDEV_DOWN:
        dbglog_printf(timestamp, vap_id, "WAL %s vdev down, count=%u",
                WAL_VDEV_TYPE(args[0]), args[1]);
        break;
    case WAL_DBGID_TX_MGMT_DESCID_SEQ_TYPE_LEN:
        dbglog_printf(timestamp, vap_id, "WAL Tx Mgmt frame desc_id=0x%x, seq=0x%x, type=0x%x, len=0x%x islocal=0x%x",
                args[0], args[1], args[2], (args[3] & 0xffff0000) >> 16, args[3] & 0x0000ffff);
        break;
    case WAL_DBGID_TX_MGMT_COMP_DESCID_STATUS:
        dbglog_printf(timestamp, vap_id, "WAL Tx Mgmt frame completion desc_id=0x%x, status=0x%x, islocal=0x%x",
                args[0], args[1], args[2]);
        break;
    case WAL_DBGID_TX_DATA_MSDUID_SEQ_TYPE_LEN:
        dbglog_printf(timestamp, vap_id, "WAL Tx Data frame msdu_id=0x%x, seq=0x%x, type=0x%x, len=0x%x",
                args[0], args[1], args[2], args[3]);
        break;
    case WAL_DBGID_TX_DATA_COMP_MSDUID_STATUS:
        dbglog_printf(timestamp, vap_id, "WAL Tx Data frame completion desc_id=0x%x, status=0x%x, seq=0x%x",
                args[0], args[1], args[2]);
        break;
    case WAL_DBGID_RESET_PCU_CYCLE_CNT:
        dbglog_printf(timestamp, vap_id, "WAL PCU cycle counter value at reset:%x", args[0]);
        break;
    case WAL_DBGID_TX_DISCARD:
        dbglog_printf(timestamp, vap_id, "WAL Tx enqueue discard msdu_id=0x%x", args[0]);
        break;
    case WAL_DBGID_SET_HW_CHAINMASK:
        dbglog_printf(timestamp, vap_id, "WAL_DBGID_SET_HW_CHAINMASK "
                                         "pdev=%d, txchain=0x%x, rxchain=0x%x",
                args[0], args[1], args[2]);
        break;
    case WAL_DBGID_SET_HW_CHAINMASK_TXRX_STOP_FAIL:
        dbglog_printf(timestamp, vap_id, "WAL_DBGID_SET_HW_CHAINMASK_TXRX_STOP_FAIL: rxstop=%d, txstop=%d",
                args[0], args[1]);
        break;
    case WAL_DBGID_GET_HW_CHAINMASK:
         dbglog_printf(timestamp, vap_id, "WAL_DBGID_GET_HW_CHAINMASK "
                                          "txchain=0x%x, rxchain=0x%x",
                 args[0], args[1]);
        break;
    case WAL_DBGID_SMPS_DISABLE:
        dbglog_printf(timestamp, vap_id, "WAL_DBGID_SMPS_DISABLE");
        break;
    case WAL_DBGID_SMPS_ENABLE_HW_CNTRL:
        dbglog_printf(timestamp, vap_id, "WAL_DBGID_SMPS_ENABLE_HW_CNTRL low_pwr_mask=0x%x, high_pwr_mask=0x%x",
                args[0], args[1]);
        break;
    case WAL_DBGID_SMPS_SWSEL_CHAINMASK:
        dbglog_printf(timestamp, vap_id, "WAL_DBGID_SMPS_SWSEL_CHAINMASK low_pwr=0x%x, chain_mask=0x%x",
                args[0], args[1]);
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

A_BOOL
dbglog_scan_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 *args)
{
    static const char *states[] = {
        "IDLE",
        "BSSCHAN",
        "WAIT_FOREIGN_CHAN",
        "FOREIGN_CHANNEL",
        "TERMINATING"
    };

    static const char *events[] = {
        "REQ",
        "STOP",
        "BSSCHAN",
        "FOREIGN_CHAN",
        "CHECK_ACTIVITY",
        "REST_TIME_EXPIRE",
        "DWELL_TIME_EXPIRE",
        "PROBE_TIME_EXPIRE",
    };

    switch (dbg_id) {
    case DBGLOG_DBGID_SM_FRAMEWORK_PROXY_DBGLOG_MSG:
        dbglog_sm_print(timestamp, vap_id, numargs, args, "SCAN",
                states, ARRAY_LENGTH(states), events, ARRAY_LENGTH(events));
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

A_BOOL dbglog_coex_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 * args)
{
    A_UINT8 i, j = 0;
    char * dbg_id_str;

    static const char * wlan_rx_xput_status[] = {
        "WLAN_XPUT_NORMAL",
        "WLAN_XPUT_UNDER_THRESH",
        "WLAN_XPUT_CRITICAL",
        "WLAN_XPUT_RECOVERY_TIMEOUT",
    };

    static const char * coex_sched_req[] = {
        "SCHED_REQ_NEXT",
        "SCHED_REQ_BT",
        "SCHED_REQ_WLAN",
        "SCHED_REQ_POSTPAUSE",
        "SCHED_REQ_UNPAUSE",
    };

    static const char * coex_sched_type[] = {
        "SCHED_NONE",
        "SCHED_WLAN",
        "SCHED_BT",
        "SCHED_WLAN_PAUSE",
        "SCHED_WLAN_POSTPAUSE",
        "SCHED_WLAN_UNPAUSE",
    };

    static const char * coex_trf_mgmt_type[] = {
        "TRF_MGMT_FREERUN",
        "TRF_MGMT_SHAPE_PM",
        "TRF_MGMT_SHAPE_PSP",
        "TRF_MGMT_SHAPE_S_CTS",
    };

    static const char * coex_system_status[] = {
        "BT_OFF",
        "BTCOEX_NOT_REQD",
        "WLAN_IS_IDLE",
        "EXECUTE_SCHEME",
        "BT_FULL_CONCURRENCY",
        "WLAN_SLEEPING",
        "WLAN_IS_PAUSED",
        "WAIT_FOR_NEXT_ACTION",
    };

    static const char * wlan_rssi_type[] = {
        "LOW_RSSI",
        "MID_RSSI",
        "HI_RSSI",
        "INVALID_RSSI",
    };

    static const char * coex_bt_scheme[] = {
        "IDLE_CTRL",
        "ACTIVE_ASYNC_CTRL",
        "PASSIVE_SYNC_CTRL",
        "ACTIVE_SYNC_CTRL",
        "DEFAULT_CTRL",
        "CONCURRENCY_CTRL",
    };

    static const char * wal_peer_rx_rate_stats_event_sent[] = {
        "PR_RX_EVT_SENT_NONE",
        "PR_RX_EVT_SENT_LOWER",
        "PR_RX_EVT_SENT_UPPER",
    };

    static const char * wlan_psp_stimulus[] = {
        "ENTRY",
        "EXIT",
        "PS_READY",
        "PS_NOT_READY",
        "RX_MORE_DATA_RCVD",
        "RX_NO_MORE_DATA_RCVD",
        "TX_DATA_COMPLT",
        "TX_COMPLT",
        "TIM_SET",
        "REQ",
        "DONE_SUCCESS",
        "DONE_NO_PS_POLL_ACK",
        "DONE_RESPONSE_TMO",
        "DONE_DROPPED",
        "DONE_FILTERED",
        "WLAN_START",
        "NONWLAN_START",
        "NONWLAN_INTVL_UPDATE",
        "NULL_TX",
        "NULL_TX_COMPLT",
        "BMISS_FIRST",
        "NULL_TX_FAIL",
        "RX_NO_MORE_DATA_DATAFRM",
    };

    static const char * coex_pspoll_state[] = {
        "STATE_DISABLED",
        "STATE_NOT_READY",
        "STATE_ENABLED",
        "STATE_READY",
        "STATE_TX_STATUS",
        "STATE_RX_STATUS",
    };

    static const char * coex_scheduler_interval[] = {
        "COEX_SCHED_NONWLAN_INT",
        "COEX_SCHED_WLAN_INT",
    };

    static const char * wlan_weight[] = {
        "BT_COEX_BASE",
        "BT_COEX_LOW",
        "BT_COEX_MID",
        "BT_COEX_MID_NONSYNC",
        "BT_COEX_HI_NONVOICE",
        "BT_COEX_HI",
        "BT_COEX_CRITICAL",
    };

    static const char * coex_psp_error_type[] = {
        "DISABLED_STATE",
        "VDEV_NULL",
        "COEX_PSP_ENTRY",
        "ZERO_INTERVAL",
        "COEX_PSP_EXIT",
        "READY_DISABLED",
        "READY_NOT_DISABLED",
        "POLL_PKT_DROPPED",
        "SET_TIMER_PARAM",
    };

    dbg_id_str = dbglog_get_msg(mod_id, dbg_id);

    switch (dbg_id) {
        case COEX_SYSTEM_UPDATE:
            if (numargs >= 1 && args[0] < 8) {
                dbglog_printf(timestamp, vap_id, "%s: %s", dbg_id_str, coex_system_status[args[0]]);
            } else {
                return FALSE;
            }
            break;
        case COEX_SCHED_START:
            if (numargs >= 5 && args[0] < 5 && args[2] < 4 && args[3] < 4 && args[4] < 4) {
                if (args[1] == 0xffffffff) {
                    dbglog_printf(timestamp, vap_id, "%s: %s, DETERMINE_DURATION, %s, %s, %s",
                        dbg_id_str, coex_sched_req[args[0]], coex_trf_mgmt_type[args[2]], wlan_rx_xput_status[args[3]], wlan_rssi_type[args[4]]);
                } else {
                    dbglog_printf(timestamp, vap_id, "%s: %s, IntvlDur(%u), %s, %s, %s",
                        dbg_id_str, coex_sched_req[args[0]], args[1], coex_trf_mgmt_type[args[2]], wlan_rx_xput_status[args[3]], wlan_rssi_type[args[4]]);
                }
            } else {
                return FALSE;
            }
            break;
        case COEX_SCHED_RESULT:
            if (numargs >= 5 && args[0] < 6) {
                dbglog_printf(timestamp, vap_id, "%s: %s, CoexMgrPolicy(%u), WlanIsIdleOverride(%u), HidConcurTxOverride(%u), minRSSI(%u)",
                    dbg_id_str, coex_sched_type[args[0]], args[1], args[2], args[3], args[4]);
            } else {
                return FALSE;
            }
            break;
        case COEX_BT_SCHEME:
            if (numargs >= 1 && args[0] < 6) {
                dbglog_printf(timestamp, vap_id, "%s: %s", dbg_id_str, coex_bt_scheme[args[0]]);
            } else {
                return FALSE;
            }
            break;
        case COEX_TRF_FREERUN:
        case COEX_TRF_SHAPE_PM:
            if (numargs >= 5 && args[0] < 6) {
                dbglog_printf(timestamp, vap_id, "%s: %s, AllocatedBtIntvls(%u), BtIntvlCnt(%u), AllocatedWlanIntvls(%u), WlanIntvlCnt(%u)",
                    dbg_id_str, coex_sched_type[args[0]], args[1], args[2], args[3], args[4]);
            } else {
                return FALSE;
            }
            break;
        case COEX_SYSTEM_MONITOR:
            if (numargs >= 5 && args[1] < 4 && args[4] < 4) {
                dbglog_printf(timestamp, vap_id, "%s: WlanRxCritical(%u), %s, MinDirectRxRate(%u), XputMonitorActiveNum(%u), %s",
                    dbg_id_str, args[0], wlan_rx_xput_status[args[1]], args[2], args[3], wlan_rssi_type[args[4]]);
            } else {
                return FALSE;
            }
            break;
        case COEX_RX_RATE:
            if (numargs >= 5 && args[4] < 3) {
                dbglog_printf(timestamp, vap_id, "%s: NumUnderThreshPeers(%u), MinDirectRate(%u), LastRateSample(%u), DeltaT(%u), %s",
                    dbg_id_str, args[0], args[1], args[2], args[3], wal_peer_rx_rate_stats_event_sent[args[4]]);
            } else {
                return FALSE;
            }
            break;
        case COEX_WLAN_INTERVAL_START:
            if (numargs >= 4) {
                dbglog_printf(timestamp, vap_id, "%s: WlanIntvlCnt(%u), XputMonitorActiveNum(%u), Duration(%u), Weight(%u)",
                    dbg_id_str, args[0], args[1], args[2], args[3]);
            } else {
                return FALSE;
            }
            break;
        case COEX_WLAN_POSTPAUSE_INTERVAL_START:
            if (numargs >= 4) {
                dbglog_printf(timestamp, vap_id, "%s: WlanPostPauseIntvlCnt(%u), XputMonitorActiveNum(%u), Duration(%u), Weight(%u)",
                    dbg_id_str, args[0], args[1], args[2], args[3]);
            } else {
                return FALSE;
            }
            break;
        case COEX_BT_INTERVAL_START:
            if (numargs >= 5) {
                dbglog_printf(timestamp, vap_id, "%s: BtIntvlCnt(%u), HidConcurrentTxOverride(%u), EnableBtBwLimit(%u), Duration(%u), Weight(%u)",
                    dbg_id_str, args[0], args[1], args[2], args[3], args[4]);
            } else {
                return FALSE;
            }
            break;
        case COEX_PSP_MGR_ENTER:
            if (numargs >= 5 && args[0] < 23 && args[1] < 6 && args[3] < 2) {
                dbglog_printf(timestamp, vap_id, "%s: %s, %s, PsPollAvg(%u), %s, CurrT(%u)",
                    dbg_id_str, wlan_psp_stimulus[args[0]], coex_pspoll_state[args[1]], args[2], coex_scheduler_interval[args[3]], args[4]);
            } else {
                return FALSE;
            }
            break;
        case COEX_PSP_MGR_RESULT:
            if (numargs >= 5 && args[0] < 6) {
                dbglog_printf(timestamp, vap_id, "%s: %s, PsPollAvg(%u), EstimationOverrun(%u), EstimationUnderun(%u), NotReadyErr(%u)",
                    dbg_id_str, coex_pspoll_state[args[0]], args[1], args[2], args[3], args[4]);
            } else {
                return FALSE;
            }
            break;
        case COEX_TRF_SHAPE_PSP:
            if (numargs == 2 && args[0] < 6) {
                dbglog_printf(timestamp, vap_id, "%s: %s, CurrT(%u)",
                    dbg_id_str, coex_pspoll_state[args[0]], args[1]);
            } else if (numargs >= 5 && args[0] < 6 && args[1] < 7) {
                    dbglog_printf(timestamp, vap_id, "%s: %s, %s, Dur(%u), WlanOverride(%u), PrioritizeWlanDuringCollis(%u)",
                        dbg_id_str, coex_sched_type[args[0]], wlan_weight[args[1]], args[2], args[3], args[4]);
            } else {
                return FALSE;
            }
            break;
        case COEX_PSP_SPEC_POLL:
            if (numargs >= 5) {
                dbglog_printf(timestamp, vap_id, "%s: PsPollSpecEna(%u), Count(%u), NextTS(%u), AllowSpecPsPollTx(%u), Intvl(%u)",
                    dbg_id_str, args[0], args[1], args[2], args[3], args[4]);
            } else {
                return FALSE;
            }
            break;
        case COEX_PSP_READY_STATE:
            if (numargs >= 5) {
                dbglog_printf(timestamp, vap_id, "%s: T2BT(%u), CoexSchedulerEndTS(%u), MoreData(%u), PSPRespExpectedTS(%u), NonWlanIdleT(%u)",
                    dbg_id_str, args[0], args[1], args[2], args[3], args[4]);
            } else {
                return FALSE;
            }
            break;
        case COEX_PSP_NONWLAN_INTERVAL:
            if (numargs >= 4) {
                dbglog_printf(timestamp, vap_id, "%s: NonWlanBaseIntvl(%u), NonWlanIdleT(%u), PSPSpecIntvl(%u), ApRespTimeout(%u)",
                    dbg_id_str, args[0], args[1], args[2], args[3]);
            } else {
                return FALSE;
            }
            break;
        case COEX_PSP_ERROR:
            if (numargs >= 1 && args[0] < 9) {
                dbglog_printf_no_line_break(timestamp, vap_id, "%s: %s",
                    dbg_id_str, coex_psp_error_type[args[0]]);
                for (i = 1; i < numargs; i++) {
                    j += snprintf(buf + j, sizeof(buf) - j, ", %u", args[i]);
                }
                diag_print_legacy_logs(buf);
            } else {
                return FALSE;
            }
            break;
        case COEX_PSP_STAT_1:
            if (numargs >= 5) {
                dbglog_printf(timestamp, vap_id, "%s: ApResp0(%u), ApResp1(%u), ApResp2(%u), ApResp3(%u), ApResp4(%u)",
                    dbg_id_str, args[0], args[1], args[2], args[3], args[4]);
            } else {
                return FALSE;
            }
            break;
        case COEX_PSP_STAT_2:
            if (numargs >= 5) {
                dbglog_printf(timestamp, vap_id, "%s: DataPt(%u), Max(%u), NextApRespIndex(%u), NumOfValidDataPts(%u), PsPollAvg(%u)",
                    dbg_id_str, args[0], args[1], args[2], args[3], args[4]);
            } else {
                return FALSE;
            }
            break;
        case COEX_PSP_RX_STATUS_STATE_1:
            if (numargs >= 5) {
                if (args[2]) {
                    dbglog_printf(timestamp, vap_id, "%s: RsExpectedTS(%u), RespActualTS(%u), Overrun, RsOverrunT(%u), RsRxDur(%u)",
                        dbg_id_str, args[0], args[1], args[3], args[4]);
                } else {
                    dbglog_printf(timestamp, vap_id, "%s: RsExpectedTS(%u), RespActualTS(%u), Underrun, RsUnderrunT(%u), RsRxDur(%u)",
                        dbg_id_str, args[0], args[1], args[3], args[4]);
                }
            } else {
                return FALSE;
            }
            break;
        //Translate following into decimal
        case COEX_SINGLECHAIN_DBG_1:
        case COEX_SINGLECHAIN_DBG_2:
        case COEX_SINGLECHAIN_DBG_3:
        case COEX_MULTICHAIN_DBG_1:
        case COEX_MULTICHAIN_DBG_2:
        case COEX_MULTICHAIN_DBG_3:
            if (numargs > 0) {
                dbglog_printf_no_line_break(timestamp, vap_id, "%s: %u",
                        dbg_id_str, args[0]);
                for (i = 1; i < numargs; i++) {
                    j += snprintf(buf + j, sizeof(buf) - j, ", %u", args[i]);
                }
                diag_print_legacy_logs(buf);
            } else {
                return FALSE;
            }
            break;
        case COEX_LinkID:
            if (numargs >= 4) {
                if (args[0]) {  //Add profile
                    dbglog_printf(timestamp, vap_id, "%s Alloc: LocalID(%u), RemoteID(%u), MinFreeLocalID(%u)",
                        dbg_id_str, args[1], args[2], args[3]);
                } else {  //Remove profile
                    dbglog_printf(timestamp, vap_id, "%s Dealloc: LocalID(%u), RemoteID(%u), MinFreeLocalID(%u)",
                        dbg_id_str, args[1], args[2], args[3]);
                }
            } else {
                return FALSE;
            }
            break;
        case COEX_BT_DURATION:
            if (numargs == 3) {
                dbglog_printf(timestamp, vap_id, "%s: Result(%u), NumOfValidSchedMsgs(%u) PrioLowerLimit(%u)",
                    dbg_id_str, args[0], args[1], args[2]);
            } else {
                return FALSE;
            }
            break;
        case COEX_T2BT:
            if (numargs == 3) {
                dbglog_printf(timestamp, vap_id, "%s: Result(%u), BtTime1(%u), PrioLowerLimit(%u)",
                    dbg_id_str, args[0], args[1], args[2]);
            } else {
                return FALSE;
            }
            break;
        default:
            return FALSE;
    }

    return TRUE;
}

A_BOOL
dbglog_beacon_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 *args)
{
    static const char *states[] = {
        "INIT",
        "ADJUST_START",
        "ADJUSTING",
        "ADJUST_HOLD",
    };

    static const char *events[] = {
        "ADJUST_START",
        "ADJUST_RESTART",
        "ADJUST_STOP",
        "ADJUST_PAUSE",
        "ADJUST_UNPAUSE",
        "ADJUST_INC_SLOP_STEP",
        "ADJUST_HOLD",
        "ADJUST_HOLD_TIME_OUT",
    };

    switch (dbg_id) {
    case DBGLOG_DBGID_SM_FRAMEWORK_PROXY_DBGLOG_MSG:
        dbglog_sm_print(timestamp, vap_id, numargs, args, "EARLY_RX",
                states, ARRAY_LENGTH(states), events, ARRAY_LENGTH(events));
        break;
    case BEACON_EVENT_EARLY_RX_BMISS_STATUS:
        if (numargs == 3) {
            dbglog_printf(timestamp, vap_id,
                    "early_rx bmiss status:rcv=%d total=%d miss=%d",
                    args[0], args[1], args[2]);
        }
        break;
    case BEACON_EVENT_EARLY_RX_SLEEP_SLOP:
        if (numargs == 1) {
            dbglog_printf(timestamp, vap_id,
                    "early_rx update sleep_slop:%d",
                    args[0]);
        }
        break;
    case BEACON_EVENT_EARLY_RX_CONT_BMISS_TIMEOUT:
        if (numargs == 1) {
            dbglog_printf(timestamp, vap_id,
                    "early_rx cont bmiss timeout,update sleep_slop:%d",
                    args[0]);
        }
        break;
    case BEACON_EVENT_EARLY_RX_PAUSE_SKIP_BCN_NUM:
        if (numargs == 1) {
            dbglog_printf(timestamp, vap_id,
                    "early_rx skip bcn num:%d",
                    args[0]);
        }
        break;
    case BEACON_EVENT_EARLY_RX_CLK_DRIFT:
        if (numargs == 1) {
            dbglog_printf(timestamp, vap_id,
                    "early_rx clk drift:%d",
                    args[0]);
        }
        break;
    case BEACON_EVENT_EARLY_RX_AP_DRIFT:
        if (numargs == 1) {
            dbglog_printf(timestamp, vap_id,
                    "early_rx ap drift:%d",
                    args[0]);
        }
        break;
    case BEACON_EVENT_EARLY_RX_BCN_TYPE:
        if (numargs == 1) {
            dbglog_printf(timestamp, vap_id,
                    "early_rx skip bcn num:%d",
                    args[0]);
        }
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

A_BOOL
dbglog_data_txrx_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 *args)
{
    switch (dbg_id) {
    case DATA_TXRX_DBGID_RX_DATA_SEQ_LEN_INFO:
        dbglog_printf(timestamp, vap_id, "DATA RX seq=0x%x, len=0x%x, stored=0x%x, duperr=0x%x",
            args[0], args[1], (args[2] & 0xffff0000) >> 16, args[2] & 0x0000ffff);
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

A_BOOL dbglog_smps_print_handler(A_UINT32 mod_id,
                                           A_UINT16 vap_id,
                                           A_UINT32 dbg_id,
                                           A_UINT32 timestamp,
                                           A_UINT16 numargs,
                                           A_UINT32 *args)
{
    static const char *states[] = {
        "S_INACTIVE",
        "S_STATIC",
        "S_DYNAMIC",
        "S_STALLED",
        "S_INACTIVE_WAIT",
        "S_STATIC_WAIT",
        "S_DYNAMIC_WAIT",
    };

    static const char *events[] = {
        "E_STOP",
        "E_STOP_COMPL",
        "E_START",
        "E_STATIC",
        "E_STATIC_COMPL",
        "E_DYNAMIC",
        "E_DYNAMIC_COMPL",
        "E_STALL",
        "E_RSSI_ABOVE_THRESH",
        "E_RSSI_BELOW_THRESH",
        "E_FORCED_NONE",
    };

    switch(dbg_id) {
    case DBGLOG_DBGID_SM_FRAMEWORK_PROXY_DBGLOG_MSG:
        dbglog_sm_print(timestamp, vap_id, numargs, args, "STA_SMPS SM",
                states, ARRAY_LENGTH(states), events, ARRAY_LENGTH(events));
        break;
    case STA_SMPS_DBGID_CREATE_PDEV_INSTANCE:
        dbglog_printf(timestamp, vap_id, "STA_SMPS Create PDEV ctx %#x",
                args[0]);
        break;
    case STA_SMPS_DBGID_CREATE_VIRTUAL_CHAN_INSTANCE:
        dbglog_printf(timestamp, vap_id, "STA_SMPS Create Virtual Chan ctx %#x",
                args[0]);
        break;
    case STA_SMPS_DBGID_DELETE_VIRTUAL_CHAN_INSTANCE:
        dbglog_printf(timestamp, vap_id, "STA_SMPS Delete Virtual Chan ctx %#x",
                args[0]);
        break;
    case STA_SMPS_DBGID_CREATE_STA_INSTANCE:
        dbglog_printf(timestamp, vap_id, "STA_SMPS Create STA ctx %#x",
                args[0]);
        break;
    case STA_SMPS_DBGID_DELETE_STA_INSTANCE:
        dbglog_printf(timestamp, vap_id, "STA_SMPS Delete STA ctx %#x",
                args[0]);
        break;
    case STA_SMPS_DBGID_VIRTUAL_CHAN_SMPS_START:
        break;
    case STA_SMPS_DBGID_VIRTUAL_CHAN_SMPS_STOP:
        break;
    case STA_SMPS_DBGID_SEND_SMPS_ACTION_FRAME:
        dbglog_printf(timestamp, vap_id, "STA_SMPS STA %#x Signal SMPS mode as %s; cb_flags %#x",
                args[0],
                (args[1] == 0 ? "DISABLED":
                 (args[1] == 0x1 ? "STATIC" :
                  (args[1] == 0x3 ? "DYNAMIC" : "UNKNOWN"))),
                args[2]);
        break;
    case STA_SMPS_DBGID_DTIM_EBT_EVENT_CHMASK_UPDATE:
        dbglog_printf(timestamp, vap_id,  "STA_SMPS_DBGID_DTIM_EBT_EVENT_CHMASK_UPDATE");
        break;
    case STA_SMPS_DBGID_DTIM_CHMASK_UPDATE:
        dbglog_printf(timestamp, vap_id,  "STA_SMPS_DBGID_DTIM_CHMASK_UPDATE "
                                          "tx_mask %#x rx_mask %#x arb_dtim_mask %#x",
                args[0], args[1], args[2]);
        break;
    case STA_SMPS_DBGID_DTIM_BEACON_EVENT_CHMASK_UPDATE:
        dbglog_printf(timestamp, vap_id,  "STA_SMPS_DBGID_DTIM_BEACON_EVENT_CHMASK_UPDATE");
        break;
    case STA_SMPS_DBGID_DTIM_POWER_STATE_CHANGE:
        dbglog_printf(timestamp, vap_id, "STA_SMPS_DBGID_DTIM_POWER_STATE_CHANGE cur_pwr_state %s new_pwr_state %s",
                (args[0] == 0x1 ? "SLEEP":
                 (args[0] == 0x2 ? "AWAKE":
                  (args[0] == 0x3 ? "FULL_SLEEP" : "UNKNOWN"))),
                (args[1] == 0x1 ? "SLEEP":
                 (args[1] == 0x2 ? "AWAKE":
                  (args[1] == 0x3 ? "FULL_SLEEP" : "UNKNOWN"))));
        break;
    case STA_SMPS_DBGID_DTIM_CHMASK_UPDATE_SLEEP:
        dbglog_printf(timestamp, vap_id, "STA_SMPS_DBGID_DTIM_CHMASK_UPDATE_SLEEP "
                                         "tx_mask %#x rx_mask %#x orig_rx %#x dtim_rx %#x",
                args[0], args[1], args[2], args[3]);
        break;
    case STA_SMPS_DBGID_DTIM_CHMASK_UPDATE_AWAKE:
        dbglog_printf(timestamp, vap_id, "STA_SMPS_DBGID_DTIM_CHMASK_UPDATE_AWAKE "
                                         "tx_mask %#x rx_mask %#x orig_rx %#x",
                args[0], args[1], args[2]);
    break;
    default:
        dbglog_printf(
                timestamp,
                vap_id,
                "STA_SMPS: UNKNOWN DBGID!");
        return FALSE;
    }

    return TRUE;
}

A_BOOL
dbglog_p2p_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 *args)
{
    static const char *states[] = {
        "ACTIVE",
        "DOZE",
        "TX_BCN",
        "CTWIN",
        "OPPPS",
    };

    static const char *events[] = {
        "ONESHOT_NOA",
        "CTWINDOW",
        "PERIODIC_NOA",
        "IDLE",
        "NOA_CHANGED",
        "TBTT",
        "TX_BCN_CMP",
        "OPPPS_OK",
        "OPPPS_CHANGED",
    };

    switch (dbg_id) {
    case DBGLOG_DBGID_SM_FRAMEWORK_PROXY_DBGLOG_MSG:
        dbglog_sm_print(timestamp, vap_id, numargs, args, "P2P GO PS",
                states, ARRAY_LENGTH(states), events, ARRAY_LENGTH(events));
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

A_BOOL
dbglog_pcielp_print_handler(
        A_UINT32 mod_id,
        A_UINT16 vap_id,
        A_UINT32 dbg_id,
        A_UINT32 timestamp,
        A_UINT16 numargs,
        A_UINT32 *args)
{
    static const char *states[] = {
        "STOP",
        "TX",
        "RX",
        "SLEEP",
        "SUSPEND",
    };

    static const char *events[] = {
        "VDEV_UP",
        "ALL_VDEV_DOWN",
        "AWAKE",
        "SLEEP",
        "TX_ACTIVITY",
        "TX_INACTIVITY",
        "TX_AC_CHANGE",
        "SUSPEND",
        "RESUME",
    };

    switch (dbg_id) {
    case DBGLOG_DBGID_SM_FRAMEWORK_PROXY_DBGLOG_MSG:
        dbglog_sm_print(timestamp, vap_id, numargs, args, "PCIELP",
                states, ARRAY_LENGTH(states), events, ARRAY_LENGTH(events));
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

int parser_init()
{
    /* Registering parser */
    dbglog_reg_modprint(WLAN_MODULE_STA_PWRSAVE, dbglog_sta_powersave_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_AP_PWRSAVE, dbglog_ap_powersave_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_WAL, dbglog_wal_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_SCAN, dbglog_scan_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_RATECTRL, dbglog_ratectrl_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_ANI, dbglog_ani_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_COEX, dbglog_coex_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_BEACON,dbglog_beacon_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_DATA_TXRX,dbglog_data_txrx_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_STA_SMPS, dbglog_smps_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_P2P, dbglog_p2p_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_PCIELP, dbglog_pcielp_print_handler);
    dbglog_reg_modprint(WLAN_MODULE_NAN, dbglog_nan_print_handler);

    return 0;
}
