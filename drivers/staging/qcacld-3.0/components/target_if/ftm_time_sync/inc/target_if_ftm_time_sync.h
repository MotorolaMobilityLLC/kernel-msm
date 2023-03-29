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
 * DOC: Declare various api/struct which shall be used
 * by FTM time sync component for wmi cmd (tx path) and
 * event (rx) handling.
 */

#ifndef _TARGET_IF_FTM_TIME_SYNC_H_
#define _TARGET_IF_FTM_TIME_SYNC_H_

#include <wlan_objmgr_psoc_obj.h>
#include <wmi_unified_param.h>
#include "wlan_ftm_time_sync_public_struct.h"

/**
 * target_if_ftm_time_sync_register_rx_ops() - Register FTM TIME SYNC component
 *					       RX ops
 * @rx_ops: FTM time sync component reception ops
 *
 * Return: None
 */
void target_if_ftm_time_sync_register_rx_ops(struct wlan_ftm_time_sync_rx_ops
					     *rx_ops);

/**
 * target_if_ftm_time_sync_register_tx_ops() - Register FTM TIME SYNC component
 *					       TX OPS
 * @tx_ops: FTM time sync component transmit ops
 *
 * Return: None
 */
void target_if_ftm_time_sync_register_tx_ops(struct wlan_ftm_time_sync_tx_ops
					     *tx_ops);

/**
 * target_if_ftm_time_sync_unregister_ev_handlers() - Unregister wmi events
 *						      handlers
 * @psoc: psoc context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_ftm_time_sync_unregister_ev_handlers(struct wlan_objmgr_psoc *psoc);
#endif /*_TARGET_IF_FTM_TIME_SYNC_H_ */
