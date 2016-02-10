/*
 * Copyright (c) 2011-2012, 2014 The Linux Foundation. All rights reserved.
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

#ifndef _WAL_RX_DESC__H_
#define _WAL_RX_DESC__H_


#if defined(ATH_TARGET)
#include <athdefs.h> /* A_UINT8 */
#else
#include <a_types.h> /* A_UINT8 */
#endif

/*
 * As this header is used by host also,
 * and host will access target registers by target reg tbl,
 * so disable direct-reference here for host.
 *
 */
#if !defined(ATH_PERF_PWR_OFFLOAD)
/* HW rx descriptor definitions */
#include <hw/mac_descriptors/rx_attention.h>
#include <hw/mac_descriptors/rx_frag_info.h>
#include <hw/mac_descriptors/rx_msdu_start.h>
#include <hw/mac_descriptors/rx_msdu_end.h>
#include <hw/mac_descriptors/rx_mpdu_start.h>
#include <hw/mac_descriptors/rx_mpdu_end.h>
#include <hw/mac_descriptors/rx_ppdu_start.h>
#include <hw/mac_descriptors/rx_ppdu_end.h>
/*
 * This struct defines the basic descriptor information, which is
 * written by the 11ac HW MAC into the WAL's rx status descriptor
 * ring.
 */
struct hw_rx_desc_base {
    struct rx_attention  attention;
    struct rx_frag_info  frag_info;
    struct rx_mpdu_start mpdu_start;
    struct rx_msdu_start msdu_start;
    struct rx_msdu_end   msdu_end;
    struct rx_mpdu_end   mpdu_end;
    struct rx_ppdu_start ppdu_start;
    struct rx_ppdu_end   ppdu_end;
};
#endif

/*
 * This struct defines the basic MSDU rx descriptor created by FW.
 */
struct fw_rx_desc_base {
    union {
        struct {
            A_UINT8 discard  : 1,
                    forward  : 1,
                    any_err  : 1,
                    dup_err  : 1,
                    reserved : 1,
                    inspect  : 1,
                    extension: 2;
        }bits;
        A_UINT8     val;
    }u;
};

#define FW_RX_DESC_DISCARD_M 0x1
#define FW_RX_DESC_DISCARD_S 0
#define FW_RX_DESC_FORWARD_M 0x2
#define FW_RX_DESC_FORWARD_S 1
#define FW_RX_DESC_MIC_ERR_M 0x4
#define FW_RX_DESC_MIC_ERR_S 2
#define FW_RX_DESC_DUP_ERR_M 0x8
#define FW_RX_DESC_DUP_ERR_S 3
#define FW_RX_DESC_INSPECT_M 0x20
#define FW_RX_DESC_INSPECT_S 5
#define FW_RX_DESC_EXT_M     0xc0
#define FW_RX_DESC_EXT_S     6

#define FW_RX_DESC_CNT_2_BYTES(_fw_desc_cnt)    (_fw_desc_cnt)

enum {
    FW_RX_DESC_EXT_NONE          = 0,
    FW_RX_DESC_EXT_LRO_ONLY,
    FW_RX_DESC_EXT_LRO_AND_OTHER,
    FW_RX_DESC_EXT_OTHER
};

#define FW_RX_DESC_DISCARD_GET(_var) \
    (((_var) & FW_RX_DESC_DISCARD_M) >> FW_RX_DESC_DISCARD_S)
#define FW_RX_DESC_DISCARD_SET(_var, _val) \
    ((_var) |= ((_val) << FW_RX_DESC_DISCARD_S))

#define FW_RX_DESC_FORWARD_GET(_var) \
    (((_var) & FW_RX_DESC_FORWARD_M) >> FW_RX_DESC_FORWARD_S)
#define FW_RX_DESC_FORWARD_SET(_var, _val) \
    ((_var) |= ((_val) << FW_RX_DESC_FORWARD_S))

#define FW_RX_DESC_INSPECT_GET(_var) \
    (((_var) & FW_RX_DESC_INSPECT_M) >> FW_RX_DESC_INSPECT_S)
#define FW_RX_DESC_INSPECT_SET(_var, _val) \
    ((_var) |= ((_val) << FW_RX_DESC_INSPECT_S))

#define FW_RX_DESC_EXT_GET(_var) \
    (((_var) & FW_RX_DESC_EXT_M) >> FW_RX_DESC_EXT_S)
#define FW_RX_DESC_EXT_SET(_var, _val) \
    ((_var) |= ((_val) << FW_RX_DESC_EXT_S))


#endif /* _WAL_RX_DESC__H_ */
