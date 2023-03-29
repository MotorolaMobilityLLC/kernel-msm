/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "ftm_time_sync_ucfg_api.h"
#include "wlan_hdd_main.h"

#ifdef FEATURE_WLAN_TIME_SYNC_FTM
/**
 * hdd_ftm_time_sync_sta_state_notify() - notify FTM TIME SYNC sta state change
 * @adapter: pointer to adapter
 * @state: enum ftm_time_sync_sta_state
 *
 * This function is called by hdd connect and disconnect handler and notifies
 * the FTM TIME SYNC component about the sta state.
 *
 * Return: None
 */
void
hdd_ftm_time_sync_sta_state_notify(struct hdd_adapter *adapter,
				   enum ftm_time_sync_sta_state state);

#else

static inline void
hdd_ftm_time_sync_sta_state_notify(struct hdd_adapter *adapter,
				   enum ftm_time_sync_sta_state state)
{
}

#endif
