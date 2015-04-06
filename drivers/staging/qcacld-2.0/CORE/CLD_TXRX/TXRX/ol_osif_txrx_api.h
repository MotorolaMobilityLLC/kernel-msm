/*
 * Copyright (c) 2011 The Linux Foundation. All rights reserved.
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

/**
 * @file ol_osif_txrx_api.h
 * @brief Define the OS specific API functions called by txrx SW.
 */
#ifndef _OL_OSIF_TXRX_API_H_
#define _OL_OSIF_TXRX_API_H_

#include <adf_nbuf.h>      /* adf_nbuf_t */

/**
 * @brief Call tx completion handler to release the buffers
 * @details
 *
 * Invoke tx completion handler when the tx credit goes below low water mark.
 * This eliminate the packet drop in the host driver due to send routine not yielding
 * the cpu when the amount of traffic pumped from the network layer is very high.
 *
 * @param osdev
 */

void ol_osif_ath_tasklet(adf_os_device_t osdev);

#endif /* _OL_OSIF_TXRX_API_H_ */
