/*
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
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
// <copyright file="wmix.h" company="Atheros">
//    Copyright (c) 2004-2010 Atheros Corporation.  All rights reserved.
// $ATH_LICENSE_HOSTSDK0_C$
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================

/*
 * This file contains extensions of the WMI protocol specified in the
 * Wireless Module Interface (WMI).  It includes definitions of all
 * extended commands and events.  Extensions include useful commands
 * that are not directly related to wireless activities.  They may
 * be hardware-specific, and they might not be supported on all
 * implementations.
 *
 * Extended WMIX commands are encapsulated in a WMI message with
 * cmd=WMI_EXTENSION_CMD.
 */

#ifndef _WMIX_H_
#define _WMIX_H_

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Extended WMI commands are those that are needed during wireless
 * operation, but which are not really wireless commands.  This allows,
 * for instance, platform-specific commands.  Extended WMI commands are
 * embedded in a WMI command message with WMI_COMMAND_ID=WMI_EXTENSION_CMDID.
 * Extended WMI events are similarly embedded in a WMI event message with
 * WMI_EVENT_ID=WMI_EXTENSION_EVENTID.
 */
typedef struct {
    A_UINT32    commandId;
} POSTPACK WMIX_CMD_HDR;

typedef enum {
    WMIX_DSETOPEN_REPLY_CMDID           = 0x2001,
    WMIX_DSETDATA_REPLY_CMDID,
    WMIX_HB_CHALLENGE_RESP_CMDID,
    WMIX_DBGLOG_CFG_MODULE_CMDID,
    WMIX_PROF_CFG_CMDID,                 /* 0x200a */
    WMIX_PROF_ADDR_SET_CMDID,
    WMIX_PROF_START_CMDID,
    WMIX_PROF_STOP_CMDID,
    WMIX_PROF_COUNT_GET_CMDID,
} WMIX_COMMAND_ID;

typedef enum {
    WMIX_DSETOPENREQ_EVENTID            = 0x3001,
    WMIX_DSETCLOSE_EVENTID,
    WMIX_DSETDATAREQ_EVENTID,
    WMIX_HB_CHALLENGE_RESP_EVENTID,
    WMIX_DBGLOG_EVENTID,
    WMIX_PROF_COUNT_EVENTID,
    WMIX_PKTLOG_EVENTID,
} WMIX_EVENT_ID;

/*
 * =============DataSet support=================
 */

/*
 * WMIX_DSETOPENREQ_EVENTID
 * DataSet Open Request Event
 */
typedef struct {
    A_UINT32 dset_id;
    A_UINT32 targ_dset_handle;  /* echo'ed, not used by Host, */
    A_UINT32 targ_reply_fn;     /* echo'ed, not used by Host, */
    A_UINT32 targ_reply_arg;    /* echo'ed, not used by Host, */
} POSTPACK WMIX_DSETOPENREQ_EVENT;

/*
 * WMIX_DSETCLOSE_EVENTID
 * DataSet Close Event
 */
typedef struct {
    A_UINT32 access_cookie;
} POSTPACK WMIX_DSETCLOSE_EVENT;

/*
 * WMIX_DSETDATAREQ_EVENTID
 * DataSet Data Request Event
 */
typedef struct {
    A_UINT32 access_cookie;
    A_UINT32 offset;
    A_UINT32 length;
    A_UINT32 targ_buf;         /* echo'ed, not used by Host, */
    A_UINT32 targ_reply_fn;    /* echo'ed, not used by Host, */
    A_UINT32 targ_reply_arg;   /* echo'ed, not used by Host, */
} WMIX_DSETDATAREQ_EVENT;

typedef struct {
    A_UINT32              status;
    A_UINT32              targ_dset_handle;
    A_UINT32              targ_reply_fn;
    A_UINT32              targ_reply_arg;
    A_UINT32              access_cookie;
    A_UINT32              size;
    A_UINT32              version;
}  WMIX_DSETOPEN_REPLY_CMD;

typedef struct {
    A_UINT32              status;
    A_UINT32              targ_buf;
    A_UINT32              targ_reply_fn;
    A_UINT32              targ_reply_arg;
    A_UINT32              length;
    A_UINT8               buf[1];
}  WMIX_DSETDATA_REPLY_CMD;

/*
 * =============Error Detection support=================
 */

/*
 * WMIX_HB_CHALLENGE_RESP_CMDID
 * Heartbeat Challenge Response command
 */
typedef struct {
    A_UINT32              cookie;
    A_UINT32              source;
}  WMIX_HB_CHALLENGE_RESP_CMD;

/*
 * WMIX_HB_CHALLENGE_RESP_EVENTID
 * Heartbeat Challenge Response Event
 */
#define WMIX_HB_CHALLENGE_RESP_EVENT WMIX_HB_CHALLENGE_RESP_CMD

/*
 * =============Target Profiling support=================
 */

typedef struct {
    A_UINT32 period; /* Time (in 30.5us ticks) between samples */
    A_UINT32 nbins;
} WMIX_PROF_CFG_CMD;

typedef struct {
    A_UINT32 addr;
} WMIX_PROF_ADDR_SET_CMD;

/*
 * Target responds to Hosts's earlier WMIX_PROF_COUNT_GET_CMDID request
 * using a WMIX_PROF_COUNT_EVENT with
 *   addr set to the next address
 *   count set to the corresponding count
 */
typedef struct {
    A_UINT32              addr;
    A_UINT32              count;
} WMIX_PROF_COUNT_EVENT;


#ifdef __cplusplus
}
#endif

#endif /* _WMIX_H_ */
