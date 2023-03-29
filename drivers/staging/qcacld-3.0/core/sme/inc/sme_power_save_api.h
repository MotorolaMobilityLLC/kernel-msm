/*
 * Copyright (c) 2015-2021 The Linux Foundation. All rights reserved.
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

#if !defined(__SME_POWER_SAVE_API_H)
#define __SME_POWER_SAVE_API_H

#include "sme_power_save.h"
#include "ani_global.h"
#include "sme_inside.h"

QDF_STATUS sme_ps_enable_disable(mac_handle_t mac_handle, uint32_t session_id,
				 enum sme_ps_cmd command);

QDF_STATUS sme_ps_timer_flush_sync(mac_handle_t mac_handle,
				   uint8_t session_id);

QDF_STATUS sme_ps_uapsd_enable(mac_handle_t mac_handle, uint32_t session_id);

QDF_STATUS sme_ps_uapsd_disable(mac_handle_t mac_handle, uint32_t session_id);

QDF_STATUS sme_enable_sta_ps_check(struct mac_context *mac_ctx,
				   uint32_t session_id,
				   enum sme_ps_cmd command);

QDF_STATUS sme_ps_process_command(struct mac_context *mac_ctx,
				  uint32_t session_id,
				  enum sme_ps_cmd command);

void sme_set_tspec_uapsd_mask_per_session(struct mac_context *mac_ctx,
					  struct mac_ts_info *ts_info,
					  uint8_t session_id);

QDF_STATUS sme_ps_start_uapsd(mac_handle_t mac_handle, uint32_t session_id);
QDF_STATUS sme_set_ps_host_offload(mac_handle_t mac_handle,
				   struct sir_host_offload_req *request,
				   uint8_t session_id);

#ifdef WLAN_NS_OFFLOAD
QDF_STATUS sme_set_ps_ns_offload(mac_handle_t mac_handle,
				 struct sir_host_offload_req *request,
				 uint8_t session_id);

#endif /* WLAN_NS_OFFLOAD */
/* / Post a message to PE module */
QDF_STATUS sme_post_pe_message(struct mac_context *mac_ctx,
			       struct scheduler_msg *pMsg);

/**
 * sme_ps_enable_auto_ps_timer(): Enable power-save auto timer with timeout
 * @mac_handle: Opaque handle to the global MAC context
 * @session_id:    adapter session Id
 * @timeout:       timeout period in ms
 *
 * Returns: QDF_STATUS
 */
QDF_STATUS sme_ps_enable_auto_ps_timer(mac_handle_t mac_handle,
				       uint32_t sessionId, uint32_t timeout);
QDF_STATUS sme_ps_disable_auto_ps_timer(mac_handle_t mac_handle,
					uint32_t sessionId);

QDF_STATUS sme_ps_open(mac_handle_t mac_handle);

QDF_STATUS sme_ps_open_per_session(mac_handle_t mac_handle,
				   uint32_t session_id);

void sme_auto_ps_entry_timer_expired(void *ps_param);
QDF_STATUS sme_ps_close(mac_handle_t mac_handle);
QDF_STATUS sme_ps_close_per_session(mac_handle_t mac_handle,
				    uint32_t session_id);
#endif /* #if !defined(__SME_POWER_SAVE_API_H) */

