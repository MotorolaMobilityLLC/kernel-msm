/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
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
 * DOC: nan_datapath.h
 *
 * MAC NAN Data path API specification
 */

#ifndef __MAC_NAN_DATAPATH_H
#define __MAC_NAN_DATAPATH_H

#ifdef WLAN_FEATURE_NAN

#include "sir_common.h"
#include "ani_global.h"
#include "sir_params.h"

struct peer_nan_datapath_map;

/**
 * lim_process_ndi_mlm_add_bss_rsp() - Process ADD_BSS response for NDI
 * @mac_ctx: Pointer to Global MAC structure
 * @add_bss_rsp: Bss params including rsp data
 * @session_entry: PE session
 *
 * Return: None
 */
void lim_process_ndi_mlm_add_bss_rsp(struct mac_context *mac_ctx,
				     struct add_bss_rsp *add_bss_rsp,
				     struct pe_session *session_entry);
/* Handler for DEL BSS resp for NDI interface */

/**
 * lim_ndi_del_bss_rsp() - Handler for DEL BSS resp for NDI interface
 * @mac_ctx: handle to mac structure
 * @del_bss: pointer to del bss response
 * @session_entry: session entry
 *
 * Return: None
 */
void lim_ndi_del_bss_rsp(struct mac_context * mac_ctx,
			 struct del_bss_resp *del_bss,
			 struct pe_session *session_entry);

void lim_ndp_add_sta_rsp(struct mac_context *mac_ctx,
			 struct pe_session *session_entry,
			 tAddStaParams *add_sta_rsp);

void lim_process_ndi_del_sta_rsp(struct mac_context *mac_ctx,
				 struct scheduler_msg *lim_msg,
				 struct pe_session *pe_session);

QDF_STATUS lim_add_ndi_peer_converged(uint32_t vdev_id,
				struct qdf_mac_addr peer_mac_addr);

void lim_ndp_delete_peers_converged(struct peer_nan_datapath_map *ndp_map,
				    uint8_t num_peers);

void lim_ndp_delete_peers_by_addr_converged(uint8_t vdev_id,
					struct qdf_mac_addr peer_ndi_mac_addr);

#else
static inline
void lim_process_ndi_mlm_add_bss_rsp(struct mac_context *mac_ctx,
				     struct add_bss_rsp *add_bss_rsp,
				     struct pe_session *session_entry)
{
}

/**
 * lim_ndi_del_bss_rsp() - Handler for DEL BSS resp for NDI interface
 * @mac_ctx: handle to mac structure
 * @del_bss: pointer to del bss response
 * @session_entry: session entry
 *
 * Return: None
 */
static inline
void lim_ndi_del_bss_rsp(struct mac_context *mac_ctx,
			 struct del_bss_resp *del_bss,
			 struct pe_session *session_entry)
{
}

static inline
void lim_process_ndi_del_sta_rsp(struct mac_context *mac_ctx,
				 struct scheduler_msg *lim_msg,
				 struct pe_session *pe_session)
{
}

static inline
void lim_ndp_add_sta_rsp(struct mac_context *mac_ctx,
			 struct pe_session *session_entry,
			 tAddStaParams *add_sta_rsp)
{
}

#endif /* WLAN_FEATURE_NAN */

#endif /* __MAC_NAN_DATAPATH_H */

