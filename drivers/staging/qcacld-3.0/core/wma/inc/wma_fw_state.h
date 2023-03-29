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

/**
 * DOC: wma_fw_state.h
 *
 * Get firmware state related API's and definitions
 */

#ifndef __WMA_FW_STATE_H
#define __WMA_FW_STATE_H

#include "wma.h"

#ifdef FEATURE_FW_STATE
/**
 * wma_get_fw_state() - send wmi cmd to get fw state
 * @wma_handle: wma handler
 *
 * Return: Return QDF_STATUS
 */
QDF_STATUS wma_get_fw_state(tp_wma_handle wma_handle);
void wma_register_fw_state_events(wmi_unified_t wmi_handle);
#else /* FEATURE_FW_STATE */
static inline
void wma_register_fw_state_events(WMA_HANDLE wma_handle)
{
}
#endif /* FEATURE_FW_STATE */
#endif /* __WMA_FW_STATE_H */
