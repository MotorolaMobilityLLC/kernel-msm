/*
 * Copyright (c) 2011, 2014-2015 The Linux Foundation. All rights reserved.
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

#ifndef _DBGLOG_COMMON_H_
#define _DBGLOG_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dbglog_id.h"
#include "dbglog.h"

#define MAX_DBG_MSGS 256

#define ATH6KL_FWLOG_PAYLOAD_SIZE              1500

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

#define CNSS_DIAG_SLEEP_INTERVAL    5   /* In secs */

#define ATH6KL_FWLOG_MAX_ENTRIES   20
#define ATH6KL_FWLOG_PAYLOAD_SIZE  1500

#define DIAG_WLAN_DRIVER_UNLOADED 6
#define DIAG_WLAN_DRIVER_LOADED   7
#define DIAG_TYPE_LOGS   1
#define DIAG_TYPE_EVENTS 2

typedef enum {
	DBGLOG_PROCESS_DEFAULT = 0,
	DBGLOG_PROCESS_PRINT_RAW,       /* print them in debug view */
	DBGLOG_PROCESS_POOL_RAW,        /* user buffer pool to save them */
	DBGLOG_PROCESS_NET_RAW,         /* user buffer pool to save them */
	DBGLOG_PROCESS_MAX,
} dbglog_process_t;

enum cnss_diag_type {
	DIAG_TYPE_FW_EVENT,           /* send fw event- to diag */
	DIAG_TYPE_FW_LOG,             /* send log event- to diag */
	DIAG_TYPE_FW_DEBUG_MSG,       /* send dbg message- to diag */
	DIAG_TYPE_INIT_REQ,           /* cnss_diag initialization- from diag */
	DIAG_TYPE_FW_MSG,             /* fw msg command-to diag */
	DIAG_TYPE_HOST_MSG,           /* host command-to diag */
	DIAG_TYPE_CRASH_INJECT,       /*crash inject-from diag */
	DIAG_TYPE_DBG_LEVEL,          /* DBG LEVEL-from diag */
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
	unsigned int word0;             /* type, length */
	unsigned int target_time;
	unsigned int code;              /* Diag log or event Code */
	uint8_t payload[0];
};

struct dbglog_slot {
	unsigned int diag_type;
	unsigned int timestamp;
	unsigned int length;
	unsigned int dropped;
	/* max ATH6KL_FWLOG_PAYLOAD_SIZE bytes */
	uint8_t payload[0];
} __packed;

typedef struct event_report_s {
	unsigned int diag_type;
	unsigned short event_id;
	unsigned short length;
} event_report_t;

/*
 * Custom debug_print handlers
 * Args:
 * module Id
 * vap id
 * debug msg id
 * Time stamp
 * no of arguments
 * pointer to the buffer holding the args
 */
typedef A_BOOL (*module_dbg_print)(A_UINT32, A_UINT16, A_UINT32,
				   A_UINT32, A_UINT16, A_UINT32 *);

/** Register module specific dbg print*/
void dbglog_reg_modprint(A_UINT32 mod_id, module_dbg_print printfn);

#ifdef __cplusplus
}
#endif

#endif /* _DBGLOG_COMMON_H_ */
