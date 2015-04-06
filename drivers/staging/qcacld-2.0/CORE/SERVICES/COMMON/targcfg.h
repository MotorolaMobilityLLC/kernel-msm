/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
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

#ifndef __TARGCFG_H__
#define __TARGCFG_H__

#if defined(ATH_TARGET)
#include <osapi.h>   /* A_UINT32 */
#else
#include <a_types.h> /* A_UINT32 */
#endif

typedef struct _targcfg_t {
    A_UINT32 num_vdev;
    A_UINT32 num_peers;
    A_UINT32 num_peer_ast;
    A_UINT32 num_peer_keys;
    A_UINT32 num_peer_tid;
    A_UINT32 num_mcast_keys;
    A_UINT32 num_tx;
    A_UINT32 num_rx;
    A_UINT32 num_mgmt_tx;
    A_UINT32 num_mgmt_rx;
    A_UINT32 tx_chain_mask;
    A_UINT32 rx_chain_mask;
    A_UINT32 override; /* Override target with the values supplied above */
} targcfg_t;

#endif /* __TARGCFG_H__ */
