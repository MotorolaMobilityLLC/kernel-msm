/*
 * Copyright (c) 2013-2016 The Linux Foundation. All rights reserved.
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

#ifndef HDD_IPA_H__
#define HDD_IPA_H__

/**===========================================================================

  \file  wlan_hdd_ipa.h

  \brief WLAN IPA interface module headers

  ==========================================================================*/

/* $HEADER$ */

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/
#ifdef IPA_OFFLOAD
#include <linux/ipa.h>

enum hdd_ipa_forward_type {
	HDD_IPA_FORWARD_PKT_NONE = 0,
	HDD_IPA_FORWARD_PKT_LOCAL_STACK = 1,
	HDD_IPA_FORWARD_PKT_DISCARD = 2
};
VOS_STATUS hdd_ipa_init(hdd_context_t *hdd_ctx);
VOS_STATUS hdd_ipa_cleanup(hdd_context_t *hdd_ctx);
int hdd_ipa_wlan_evt(hdd_adapter_t *adapter, uint8_t sta_id,
		enum ipa_wlan_event type, uint8_t *mac_addr);
VOS_STATUS hdd_ipa_process_rxt(v_VOID_t *vosContext, adf_nbuf_t rxBuf,
		v_U8_t sta_id);
bool hdd_ipa_is_enabled(hdd_context_t *pHddCtx);

int hdd_ipa_set_perf_level(hdd_context_t *hdd_ctx, uint64_t tx_packets,
		uint64_t rx_packets);

int hdd_ipa_suspend(hdd_context_t *hdd_ctx);
int hdd_ipa_resume(hdd_context_t *hdd_ctx);
void hdd_ipa_ready_cb(hdd_context_t *hdd_ctx);

#ifdef IPA_UC_STA_OFFLOAD
int hdd_ipa_send_mcc_scc_msg(hdd_context_t *hdd_ctx, bool mcc_mode);
#endif

#ifdef IPA_UC_OFFLOAD
void hdd_ipa_uc_force_pipe_shutdown(hdd_context_t *hdd_ctx);
int hdd_ipa_uc_ssr_reinit(void);
int hdd_ipa_uc_ssr_deinit(void);
void hdd_ipa_uc_stat_query(hdd_context_t *pHddCtx,
	uint32_t *ipa_tx_diff, uint32_t *ipa_rx_diff);
void hdd_ipa_uc_stat_request( hdd_adapter_t *adapter, uint8_t reason);
void hdd_ipa_uc_rt_debug_host_dump(hdd_context_t *hdd_ctx);
void hdd_ipa_dump_info(hdd_context_t *hdd_ctx);
#endif /* IPA_UC_OFFLOAD */
#endif /* IPA_OFFLOAD */

#if !defined(IPA_OFFLOAD) || !defined(IPA_UC_OFFLOAD)
static inline
void hdd_ipa_uc_force_pipe_shutdown(hdd_context_t *hdd_ctx)
{
	return;
}
static inline
void hdd_ipa_uc_rt_debug_host_dump(hdd_context_t *hdd_ctx)
{
	return;
}
static inline
void hdd_ipa_dump_info(hdd_context_t *hdd_ctx)
{
	return;
}
#endif
#endif
