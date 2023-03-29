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

/**
 * DOC: Declares public API for ftm time sync to interact with target/WMI
 */

#ifndef _WLAN_FTM_TIME_SYNC_TGT_API_H_
#define _WLAN_FTM_TIME_SYNC_TGT_API_H_

#include <qdf_types.h>

struct wlan_objmgr_psoc;
struct ftm_time_sync_start_stop_params;
struct ftm_time_sync_offset;

/**
 * tgt_ftm_ts_start_stop_evt() - receive start/stop ftm event
 *				 from target if
 * @psoc: objmgr psoc object
 * @param: start/stop parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_ftm_ts_start_stop_evt(struct wlan_objmgr_psoc *psoc,
			  struct ftm_time_sync_start_stop_params *param);

/**
 * tgt_ftm_ts_offset_evt() - receive offset ftm event from target if
 * @psoc: objmgr psoc object
 * @param: offset parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tgt_ftm_ts_offset_evt(struct wlan_objmgr_psoc *psoc,
				 struct ftm_time_sync_offset *param);

#endif /*_WLAN_FTM_TIME_SYNC_TGT_API_H_ */
