/*
 * Copyright (c) 2013-2014, 2016, 2018-2019 The Linux Foundation. All rights reserved.
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
 * Offload specific Opaque Data types.
 */
#ifndef _DEV_OL_DEFINES_H
#define _DEV_OL_DEFINES_H

#define OL_TXRX_PDEV_ID 0

#define NORMALIZED_TO_NOISE_FLOOR (-96)

 /**
  * ol_txrx_pdev_handle - opaque handle for txrx physical device
  * object
  */
struct ol_txrx_pdev_t;
typedef struct ol_txrx_pdev_t *ol_txrx_pdev_handle;

/**
 * ol_txrx_vdev_handle - opaque handle for txrx virtual device
 * object
 */
struct ol_txrx_vdev_t;
typedef struct ol_txrx_vdev_t *ol_txrx_vdev_handle;

/**
 * ol_pdev_handle - opaque handle for the configuration
 * associated with the physical device
 */
struct ol_pdev_t;
typedef struct ol_pdev_t *ol_pdev_handle;

/**
 * ol_txrx_peer_handle - opaque handle for txrx peer object
 */
struct ol_txrx_peer_t;
typedef struct ol_txrx_peer_t *ol_txrx_peer_handle;

#endif /* _DEV_OL_DEFINES_H */
