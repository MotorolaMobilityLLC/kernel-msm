/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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
/*
 * This file contains the API definitions for the Unified Wireless Module Interface (WMI).
 */

#ifndef _WMI_UNIFIED_API_H_
#define _WMI_UNIFIED_API_H_

#include <osdep.h>
#include "a_types.h"
#include "ol_defines.h"
#include "wmi.h"
#include "htc_api.h"

typedef adf_nbuf_t wmi_buf_t;
#define wmi_buf_free(_buf) adf_nbuf_free(_buf)
#define wmi_buf_data(_buf) adf_nbuf_data(_buf)

/**
 * attach for unified WMI
 *
 *  @param scn_handle      : handle to SCN.
 *  @return opaque handle.
 */
void *
wmi_unified_attach(void *scn_handle, void (*func) (void*));
/**
 * detach for unified WMI
 *
 *  @param wmi_handle      : handle to WMI.
 *  @return void.
 */
void
wmi_unified_detach(struct wmi_unified* wmi_handle);

void
wmi_unified_remove_work(struct wmi_unified* wmi_handle);

/**
 * generic function to allocate WMI buffer
 *
 *  @param wmi_handle      : handle to WMI.
 *  @param len             : length of the buffer
 *  @return wmi_buf_t.
 */
wmi_buf_t
wmi_buf_alloc(wmi_unified_t wmi_handle, u_int16_t len);


/**
 * generic function to send unified WMI command
 *
 *  @param wmi_handle      : handle to WMI.
 *  @param buf             : wmi command buffer
 *  @param buflen          : wmi command buffer length
 *  @return 0  on success and -ve on failure.
 */
int
wmi_unified_cmd_send(wmi_unified_t wmi_handle, wmi_buf_t buf, int buflen, WMI_CMD_ID cmd_id);

/**
 * WMI event handler register function
 *
 *  @param wmi_handle      : handle to WMI.
 *  @param event_id        : WMI event ID
 *  @param handler_func    : Event handler call back function
 *  @return 0  on success and -ve on failure.
 */
int
wmi_unified_register_event_handler(wmi_unified_t wmi_handle, WMI_EVT_ID event_id,
				   wmi_unified_event_handler handler_func);

/**
 * WMI event handler unregister function
 *
 *  @param wmi_handle      : handle to WMI.
 *  @param event_id        : WMI event ID
 *  @return 0  on success and -ve on failure.
 */
int
wmi_unified_unregister_event_handler(wmi_unified_t wmi_handle, WMI_EVT_ID event_id);


/**
 * request wmi to connet its htc service.
 *  @param wmi_handle      : handle to WMI.
 *  @return void
 */
int
wmi_unified_connect_htc_service(struct wmi_unified * wmi_handle, void *htc_handle);

/*
 * WMI API to verify the host has enough credits to suspend
*/

int
wmi_is_suspend_ready(wmi_unified_t wmi_handle);

/**
 WMI API to get updated host_credits
*/

int
wmi_get_host_credits(wmi_unified_t wmi_handle);

/**
 WMI API to get WMI Pending Commands in the HTC queue
*/

int
wmi_get_pending_cmds(wmi_unified_t wmi_handle);

/**
 WMI API to set target suspend state
*/

void
wmi_set_target_suspend(wmi_unified_t wmi_handle, A_BOOL val);

void wmi_set_tgt_assert(wmi_unified_t wmi_handle, A_BOOL val);

#ifdef FEATURE_RUNTIME_PM
void
wmi_set_runtime_pm_inprogress(wmi_unified_t wmi_handle, A_BOOL val);
bool wmi_get_runtime_pm_inprogress(wmi_unified_t wmi_handle);
#else
static inline void
wmi_set_runtime_pm_inprogress(wmi_unified_t wmi_handle, A_BOOL val)
{
	return;
}
static inline bool wmi_get_runtime_pm_inprogress(wmi_unified_t wmi_handle)
{
	return false;
}
#endif

#ifdef FEATURE_WLAN_D0WOW
/**
 WMI API to set D0WOW flag
*/
void
wmi_set_d0wow_flag(wmi_unified_t wmi_handle, A_BOOL flag);

/**
 WMI API to get D0WOW flag
*/
A_BOOL
wmi_get_d0wow_flag(wmi_unified_t wmi_handle);
#endif

/**
 WMA Callback to get the Tx complete for WOW_ENABLE
*/
typedef void (*wma_wow_tx_complete_cbk)(void *scn_handle);

uint16_t wmi_get_max_msg_len(wmi_unified_t wmi_handle);

void wmi_tag_crash_inject(wmi_unified_t wmi_handle, A_BOOL flag);
#endif /* _WMI_UNIFIED_API_H_ */
