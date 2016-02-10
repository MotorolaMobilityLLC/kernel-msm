/*
 * Copyright (c) 2015 The Linux Foundation. All rights reserved.
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

#if !defined WLAN_HDD_TSF_H
#define WLAN_HDD_TSF_H

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/
#include <wlan_hdd_includes.h>

/*---------------------------------------------------------------------------
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  Function declarations and documentation
  -------------------------------------------------------------------------*/

#ifdef WLAN_FEATURE_TSF
void wlan_hdd_tsf_init(hdd_context_t *hdd_ctx);
int hdd_capture_tsf(hdd_adapter_t *adapter, uint32_t *buf, int len);
int hdd_indicate_tsf(hdd_adapter_t *adapter, uint32_t *buf, int len);
#else
static inline void
wlan_hdd_tsf_init(hdd_context_t *hdd_ctx)
{
	return;
}

static inline int
hdd_indicate_tsf(hdd_adapter_t *adapter, uint32_t *buf, int len)
{
	return -ENOTSUPP;
}

static inline int
hdd_capture_tsf(hdd_adapter_t *adapter, uint32_t *buf, int len)
{
	return -ENOTSUPP;
}
#endif

#endif
