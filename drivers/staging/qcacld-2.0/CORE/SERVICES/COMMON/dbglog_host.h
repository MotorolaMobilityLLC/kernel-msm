/*
 * Copyright (c) 2011, 2016 The Linux Foundation. All rights reserved.
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


#ifndef _DBGLOG_HOST_H_
#define _DBGLOG_HOST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dbglog_id.h"
#include "dbglog.h"
#include "ol_defines.h"

#define MAX_DBG_MSGS 256

#define CLD_NETLINK_USER 17

#define LOGFILE_FLAG           0x01
#define CONSOLE_FLAG           0x02
#define QXDM_FLAG              0x04
#define SILENT_FLAG            0x08
#define DEBUG_FLAG             0x10

#define ATH6KL_FWLOG_PAYLOAD_SIZE              1500

#define HDRLEN 16
#define RECLEN (HDRLEN + ATH6KL_FWLOG_PAYLOAD_SIZE)

#define DBGLOG_PRINT_PREFIX "FWLOG: "

/* Handy Macros to read data length and type from FW */
#define WLAN_DIAG_0_TYPE_S          0
#define WLAN_DIAG_0_TYPE            0x000000ff
#define WLAN_DIAG_0_TYPE_GET(x)     WMI_F_MS(x, WLAN_DIAG_0_TYPE)
#define WLAN_DIAG_0_TYPE_SET(x, y)  WMI_F_RMW(x, y, WLAN_DIAG_0_TYPE)
/* bits 8-15 reserved */

/* length includes the size of wlan_diag_data */
#define WLAN_DIAG_0_LEN_S           16
#define WLAN_DIAG_0_LEN             0xffff0000
#define WLAN_DIAG_0_LEN_GET(x)      WMI_F_MS(x, WLAN_DIAG_0_LEN)
#define WLAN_DIAG_0_LEN_SET(x, y)   WMI_F_RMW(x, y, WLAN_DIAG_0_LEN)

#define CNSS_DIAG_SLEEP_INTERVAL    5 /* In secs */

#define SIZEOF_NL_MSG_LOAD     28 /* sizeof nlmsg and load length */
#define SIZEOF_NL_MSG_UNLOAD   28 /* sizeof nlmsg and Unload length */
#define SIZEOF_NL_MSG_DBG_MSG  1532
#define ATH6KL_FWLOG_MAX_ENTRIES   20
#define ATH6KL_FWLOG_PAYLOAD_SIZE  1500

#define DIAG_WLAN_DRIVER_UNLOADED 6
#define DIAG_WLAN_DRIVER_LOADED   7
#define DIAG_TYPE_LOGS   1
#define DIAG_TYPE_EVENTS 2


typedef enum {
    DBGLOG_PROCESS_DEFAULT = 0,
    DBGLOG_PROCESS_PRINT_RAW, /* print them in debug view */
    DBGLOG_PROCESS_POOL_RAW, /* user buffer pool to save them */
    DBGLOG_PROCESS_NET_RAW, /* user buffer pool to save them */
    DBGLOG_PROCESS_MAX,
} dbglog_process_t;

enum cnss_diag_type {
    DIAG_TYPE_FW_EVENT,     /* send fw event- to diag*/
    DIAG_TYPE_FW_LOG,       /* send log event- to diag*/
    DIAG_TYPE_FW_DEBUG_MSG, /* send dbg message- to diag*/
    DIAG_TYPE_INIT_REQ,     /* cnss_diag nitialization- from diag */
    DIAG_TYPE_FW_MSG,       /* fw msg command-to diag */
    DIAG_TYPE_HOST_MSG,     /* host command-to diag */
    DIAG_TYPE_CRASH_INJECT, /*crash inject-from diag */
    DIAG_TYPE_DBG_LEVEL,    /* DBG LEVEL-from diag */
};

enum wlan_diag_config_type {
    DIAG_VERSION_INFO,
};

enum wlan_diag_frame_type {
    WLAN_DIAG_TYPE_CONFIG,
    WLAN_DIAG_TYPE_EVENT,
    WLAN_DIAG_TYPE_LOG,
    WLAN_DIAG_TYPE_MSG,
    WLAN_DIAG_TYPE_LEGACY_MSG,
};

/* log/event are always 32-bit aligned. Padding is inserted after
 * optional payload to satisify this requirement */
struct wlan_diag_data {
    unsigned int word0; /* type, length */
    unsigned int target_time;
    unsigned int code;  /* Diag log or event Code */
    u_int8_t payload[0];
};


struct dbglog_slot {
    unsigned int diag_type;
    unsigned int timestamp;
    unsigned int length;
    unsigned int dropped;
    /* max ATH6KL_FWLOG_PAYLOAD_SIZE bytes */
    u_int8_t payload[0];
}__packed;

typedef struct event_report_s {
    unsigned int diag_type;
    unsigned short event_id;
    unsigned short length;
} event_report_t;

typedef struct wlan_bringup_s {
    unsigned short wlanStatus;
    char driverVersion[10];
} wlan_bringup_t;

static inline unsigned int get_32(const unsigned char *pos)
{
    return pos[0] | (pos[1] << 8) | (pos[2] << 16) | (pos[3] << 24);
}

/*
 * set the dbglog parser type
 */
int
dbglog_parser_type_init(wmi_unified_t wmi_handle, int type);

/** dbglog_int - Registers a WMI event handle for WMI_DBGMSG_EVENT
* @brief wmi_handle - handle to wmi module
*/
int
dbglog_init(wmi_unified_t wmi_handle);

/** dbglog_deinit - UnRegisters a WMI event handle for WMI_DBGMSG_EVENT
* @brief wmi_handle - handle to wmi module
*/
int
dbglog_deinit(wmi_unified_t wmi_handle);

/** set the size of the report size
* @brief wmi_handle - handle to Wmi module
* @brief size - Report size
*/
int
dbglog_set_report_size(wmi_unified_t  wmi_handle, A_UINT16 size);

/** Set the resolution for time stamp
* @brief wmi_handle - handle to Wmi module
* @ brief tsr - time stamp resolution
*/
int
dbglog_set_timestamp_resolution(wmi_unified_t  wmi_handle, A_UINT16 tsr);

/** Enable reporting. If it is set to false then Traget wont deliver
* any debug information
*/
int
dbglog_report_enable(wmi_unified_t  wmi_handle, A_BOOL isenable);

/** Set the log level
* @brief DBGLOG_INFO - Information lowest log level
* @brief DBGLOG_WARNING
* @brief DBGLOG_ERROR - default log level
*/
int
dbglog_set_log_lvl(wmi_unified_t  wmi_handle, DBGLOG_LOG_LVL log_lvl);

/*
 * set the debug log level for a given module
 *  mod_id_lvl : the format is more user friendly.
 *    module_id =  mod_id_lvl/10;
 *    log_level =  mod_id_lvl%10;
 * example : mod_id_lvl is 153. then module id is 15 and log level is 3. this format allows
 *         user to pass a sinlge value (which is the most convenient way for most of the OSs)
 *         to be passed from user to the driver.
 */
int
dbglog_set_mod_log_lvl(wmi_unified_t  wmi_handle, A_UINT32 mod_id_lvl);

/** Enable/Disable the logging for VAP */
int
dbglog_vap_log_enable(wmi_unified_t  wmi_handle, A_UINT16 vap_id,
			   A_BOOL isenable);
/** Enable/Disable logging for Module */
int
dbglog_module_log_enable(wmi_unified_t  wmi_handle, A_UINT32 mod_id,
			      A_BOOL isenable);

/** set vap enablie bitmap */
void
dbglog_set_vap_enable_bitmap(wmi_unified_t  wmi_handle, A_UINT32 vap_enable_bitmap);

/** set log level for all the modules specified in the bitmap. for all other modules
  * with 0 in the bitmap (or) outside the bitmap , the log level be reset to DBGLOG_ERR.
  */
void
dbglog_set_mod_enable_bitmap(wmi_unified_t  wmi_handle,A_UINT32 log_level,
   A_UINT32 *mod_enable_bitmap, A_UINT32 bitmap_len );

/** Custome debug_print handlers */
/* Args:
   module Id
   vap id
   debug msg id
   Time stamp
   no of arguments
   pointer to the buffer holding the args
*/
typedef A_BOOL (*module_dbg_print) (A_UINT32, A_UINT16, A_UINT32, A_UINT32,
                                   A_UINT16, A_UINT32 *);

/** Register module specific dbg print*/
void dbglog_reg_modprint(A_UINT32 mod_id, module_dbg_print printfn);

/** Register the cnss_diag activate with the wlan driver */
int cnss_diag_activate_service(void);

#ifdef __cplusplus
}
#endif

#endif /* _DBGLOG_HOST_H_ */
