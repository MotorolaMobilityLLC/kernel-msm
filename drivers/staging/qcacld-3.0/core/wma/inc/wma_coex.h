/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_HDD_DEBUGFS_COEX_H
#define _WLAN_HDD_DEBUGFS_COEX_H

#include <wma.h>
#include "sme_api.h"

#ifdef WLAN_MWS_INFO_DEBUGFS
void wma_get_mws_coex_info_req(tp_wma_handle wma_handle,
			       struct sir_get_mws_coex_info *req);
void wma_register_mws_coex_events(tp_wma_handle wma_handle);
#else
static void wma_register_mws_coex_events(tp_wma_handle wma_handle)
{
}
#endif
#endif
