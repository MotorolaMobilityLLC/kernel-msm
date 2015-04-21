/*
 * Copyright (c) 2012, 2015 The Linux Foundation. All rights reserved.
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

//------------------------------------------------------------------------------
// <copyright file="dbglog.h" company="Atheros">
//    Copyright (c) 2004-2010 Atheros Corporation.  All rights reserved.
// $ATH_LICENSE_HOSTSDK0_C$
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================

#ifndef _DBGLOG_H_
#define _DBGLOG_H_

#ifndef ATH_TARGET
#include "athstartpack.h"
#endif

#include "wlan_module_ids.h"

#ifdef __cplusplus
extern "C" {
#endif
#define DBGLOG_TIMESTAMP_OFFSET          0
#define DBGLOG_TIMESTAMP_MASK            0xFFFFFFFF /* Bit 0-15. Contains bit
                                                       8-23 of the LF0 timer */
#define DBGLOG_DBGID_OFFSET              0
#define DBGLOG_DBGID_MASK                0x000003FF /* Bit 0-9 */
#define DBGLOG_DBGID_NUM_MAX             256 /* Upper limit is width of mask */

#define DBGLOG_MODULEID_OFFSET           10
#define DBGLOG_MODULEID_MASK             0x0003FC00 /* Bit 10-17 */
#define DBGLOG_MODULEID_NUM_MAX          32 /* Upper limit is width of mask */

#define DBGLOG_VDEVID_OFFSET              18
#define DBGLOG_VDEVID_MASK                0x03FC0000 /* Bit 20-25*/
#define DBGLOG_VDEVID_NUM_MAX             16

#define DBGLOG_NUM_ARGS_OFFSET            26
#define DBGLOG_NUM_ARGS_MASK              0xFC000000 /* Bit 26-31 */
#define DBGLOG_NUM_ARGS_MAX                9 /* it is bcoz of limitation
                                             of corebsp MSG*() to accept max 9 arg  */

#define DBGLOG_LOG_BUFFER_SIZE            1500
#define DBGLOG_DBGID_DEFINITION_LEN_MAX   90

#define DBGLOG_HOST_LOG_BUFFER_SIZE            DBGLOG_LOG_BUFFER_SIZE

#define DBGLOG_GET_DBGID(arg) \
    ((arg & DBGLOG_DBGID_MASK) >> DBGLOG_DBGID_OFFSET)

#define DBGLOG_GET_MODULEID(arg) \
    ((arg & DBGLOG_MODULEID_MASK) >> DBGLOG_MODULEID_OFFSET)

#define DBGLOG_GET_VDEVID(arg) \
    ((arg & DBGLOG_VDEVID_MASK) >> DBGLOG_VDEVID_OFFSET)

#define DBGLOG_GET_NUMARGS(arg) \
    ((arg & DBGLOG_NUM_ARGS_MASK) >> DBGLOG_NUM_ARGS_OFFSET)

#define DBGLOG_GET_TIME_STAMP(arg) \
    ((arg & DBGLOG_TIMESTAMP_MASK) >> DBGLOG_TIMESTAMP_OFFSET)


#define DIAG_FWID_OFFSET            24
#define DIAG_FWID_MASK              0xFF000000 /* Bit 24-31 */

#define DIAG_TIMESTAMP_OFFSET       0
#define DIAG_TIMESTAMP_MASK         0x00FFFFFF /* Bit 0-23 */

#define DIAG_ID_OFFSET              16
#define DIAG_ID_MASK                0xFFFF0000 /* Bit 16-31 */

#define DIAG_VDEVID_OFFSET          11
#define DIAG_VDEVID_MASK            0x0000F800 /* Bit 11-15 */
#define DIAG_VDEVID_NUM_MAX         16

#define DIAG_VDEVLEVEL_OFFSET       8
#define DIAG_VDEVLEVEL_MASK         0x00000700 /* Bit 8-10 */

#define DIAG_PAYLEN_OFFSET          0
#define DIAG_PAYLEN_MASK            0x000000FF /* Bit 0-7 */

#define DIAG_PAYLEN_OFFSET16        0
#define DIAG_PAYLEN_MASK16          0x0000FFFF /* Bit 0-16 */

#define DIAG_GET_TYPE(arg) \
    ((arg & DIAG_FWID_MASK) >> DIAG_FWID_OFFSET)

#define DIAG_GET_TIME_STAMP(arg) \
    ((arg & DIAG_TIMESTAMP_MASK) >> DIAG_TIMESTAMP_OFFSET)

#define DIAG_GET_ID(arg) \
    ((arg & DIAG_ID_MASK) >> DIAG_ID_OFFSET)

#define DIAG_GET_VDEVID(arg) \
    ((arg & DIAG_VDEVID_MASK) >> DIAG_VDEVID_OFFSET)

#define DIAG_GET_VDEVLEVEL(arg) \
    ((arg & DIAG_VDEVLEVEL_MASK) >> DIAG_VDEVLEVEL_OFFSET)

#define DIAG_GET_PAYLEN(arg) \
    ((arg & DIAG_PAYLEN_MASK) >> DIAG_PAYLEN_OFFSET)

#define DIAG_GET_PAYLEN16(arg) \
    ((arg & DIAG_PAYLEN_MASK16) >> DIAG_PAYLEN_OFFSET16)

/* Debug Log levels*/

typedef enum {
    DBGLOG_VERBOSE = 0,
    DBGLOG_INFO,
    DBGLOG_INFO_LVL_1,
    DBGLOG_INFO_LVL_2,
    DBGLOG_WARN,
    DBGLOG_ERR,
    DBGLOG_LVL_MAX
}DBGLOG_LOG_LVL;

PREPACK struct dbglog_buf_s {
    struct dbglog_buf_s *next;
    A_UINT8             *buffer;
    A_UINT32             bufsize;
    A_UINT32             length;
    A_UINT32             count;
    A_UINT32             free;
} POSTPACK;

PREPACK struct dbglog_hdr_s {
    struct dbglog_buf_s *dbuf;
    A_UINT32             dropped;
} POSTPACK;

PREPACK struct dbglog_buf_host {
    A_UINT32             next;
    A_UINT32             buffer;
    A_UINT32             bufsize;
    A_UINT32             length;
    A_UINT32             count;
    A_UINT32             free;
} POSTPACK;

PREPACK struct dbglog_hdr_host {
    A_UINT32             dbuf;
    A_UINT32             dropped;
} POSTPACK;

#define DBGLOG_MAX_VDEVID 15 /* 0-15 */

#ifdef __cplusplus
}
#endif


#endif /* _DBGLOG_H_ */
