/*
 * Copyright (c) 2004-2010 2013 The Linux Foundation. All rights reserved.
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
// Copyright (c) 2004-2010, 2013 Atheros Corporation.  All rights reserved.
// $ATH_LICENSE_HOSTSDK0_C$
//------------------------------------------------------------------------------
//==============================================================================
// Author(s): ="Atheros"
//==============================================================================

/*
 * This file contains the definitions of the WMI protocol specified in the
 * Wireless Module Interface (WMI).  It includes definitions of all the
 * commands and events. Commands are messages from the host to the WM.
 * Events and Replies are messages from the WM to the host.
 *
 * Ownership of correctness in regards to commands
 * belongs to the host driver and the WMI is not required to validate
 * parameters for value, proper range, or any other checking.
 *
 */

#ifndef _WMI_H_
#define _WMI_H_

#include "wlan_defs.h"
#include "wmix.h"
#include "wmi_unified.h"
#include "wmi_tlv_helper.h"
#include "wmi_tlv_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTC_PROTOCOL_VERSION    0x0002

#define WMI_PROTOCOL_VERSION    0x0002

#define WMI_MODE_MAX              8
#define WMI_MAX_RATE_MASK         6


PREPACK struct host_app_area_s {
    A_UINT32 wmi_protocol_ver;
} POSTPACK;


#undef MS
#define MS(_v, _f) (((_v) & _f##_MASK) >> _f##_LSB)
#undef SM
#define SM(_v, _f) (((_v) << _f##_LSB) & _f##_MASK)
#undef WO
#define WO(_f)      ((_f##_OFFSET) >> 2)

#undef GET_FIELD
#define GET_FIELD(_addr, _f) MS(*((A_UINT32 *)(_addr) + WO(_f)), _f)
#undef SET_FIELD
#define SET_FIELD(_addr, _f, _val)  \
    (*((A_UINT32 *)(_addr) + WO(_f)) = \
     (*((A_UINT32 *)(_addr) + WO(_f)) & ~_f##_MASK) | SM(_val, _f))

#define WMI_GET_FIELD(_msg_buf, _msg_type, _f) \
    GET_FIELD(_msg_buf, _msg_type ## _ ## _f)

#define WMI_SET_FIELD(_msg_buf, _msg_type, _f, _val) \
    SET_FIELD(_msg_buf, _msg_type ## _ ## _f, _val)

#define WMI_EP_APASS           0x0
#define WMI_EP_LPASS           0x1
#define WMI_EP_SENSOR          0x2

/*
 * Control Path
 */
typedef PREPACK struct {
    A_UINT32    commandId : 24,
                reserved  : 2, /* used for WMI endpoint ID */
                plt_priv  : 6; /* platform private */
} POSTPACK WMI_CMD_HDR;        /* used for commands and events */

#define WMI_CMD_HDR_COMMANDID_LSB           0
#define WMI_CMD_HDR_COMMANDID_MASK          0x00ffffff
#define WMI_CMD_HDR_COMMANDID_OFFSET        0x00000000
#define WMI_CMD_HDR_WMI_ENDPOINTID_MASK     0x03000000
#define WMI_CMD_HDR_WMI_ENDPOINTID_OFFSET   24
#define WMI_CMD_HDR_PLT_PRIV_LSB            24
#define WMI_CMD_HDR_PLT_PRIV_MASK           0xff000000
#define WMI_CMD_HDR_PLT_PRIV_OFFSET         0x00000000

/*
 * List of Commnands
 */
typedef enum {
    WMI_EXTENSION_CMDID,                     //used in wmi_svc.c   /* Non-wireless extensions */
    WMI_IGNORE_CMDID,				//used in wlan_wmi.c
} WMI_COMMAND_ID;


typedef enum {
    NONE_CRYPT          = 0x01,
    WEP_CRYPT           = 0x02,
    TKIP_CRYPT          = 0x04,
    AES_CRYPT           = 0x08,
#ifdef WAPI_ENABLE
    WAPI_CRYPT          = 0x10,
#endif /*WAPI_ENABLE*/
} CRYPTO_TYPE;

#define WMI_MAX_SSID_LEN    32

/*
 * WMI_SET_PMK_CMDID
 */
#define WMI_PMK_LEN     32


/*
 * WMI_ADD_CIPHER_KEY_CMDID
 */
typedef enum {
    PAIRWISE_USAGE      = 0x00,
    GROUP_USAGE         = 0x01,
    TX_USAGE            = 0x02,     /* default Tx Key - Static WEP only */
} KEY_USAGE;


/*
 * List of Events (target to host)
 */
typedef enum {
    WMI_EXTENSION_EVENTID,		//wmi_profhook.c and umac_wmi_events.c
} WMI_EVENT_ID;

typedef enum {
    WMI_11A_CAPABILITY   = 1,
    WMI_11G_CAPABILITY   = 2,
    WMI_11AG_CAPABILITY  = 3,
    WMI_11NA_CAPABILITY  = 4,
    WMI_11NG_CAPABILITY  = 5,
    WMI_11NAG_CAPABILITY = 6,
    WMI_11AC_CAPABILITY  = 7,
    // END CAPABILITY
    WMI_11N_CAPABILITY_OFFSET = (WMI_11NA_CAPABILITY - WMI_11A_CAPABILITY),
} WMI_PHY_CAPABILITY;


/* Deprectated, need clean up */
#define WMI_MAX_RX_META_SZ  (12)

typedef PREPACK struct {
    A_INT8      rssi;
    A_UINT8     info;               /* usage of 'info' field(8-bit):
                                     *  b1:b0       - WMI_MSG_TYPE
                                     *  b4:b3:b2    - UP(tid)
                                     *  b5          - Used in AP mode. More-data in tx dir, PS in rx.
                                     *  b7:b6       -  Dot3 header(0),
                                     *                 Dot11 Header(1),
                                     *                 ACL data(2)
                                     */

    A_UINT16    info2;              /* usage of 'info2' field(16-bit):
                                     * b11:b0       - seq_no
                                     * b12          - A-MSDU?
                                     * b15:b13      - META_DATA_VERSION 0 - 7
                                     */
    A_UINT16    info3;              /* b3:b2:b1:b0  - device id
                                     * b4           - Used in AP mode. uAPSD trigger in rx, EOSP in tx
                                     * b7:b5        - unused?
                                     * b15:b8       - pad before data start(irrespective of meta version)
                                     */
} POSTPACK WMI_DATA_HDR;

#ifdef __cplusplus
}
#endif

#endif /* _WMI_H_ */
