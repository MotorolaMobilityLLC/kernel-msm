/*
 * Copyright (c) 2013-2021, The Linux Foundation. All rights reserved.
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
 * DOC: Implement API's specific to control path stats component.
 */

#ifndef _WMI_UNIFIED_MC_CP_STATS_API_H_
#define _WMI_UNIFIED_MC_CP_STATS_API_H_

#include <wlan_cp_stats_mc_defs.h>

/**
 * wmi_extract_per_chain_rssi_stats() - extract rssi stats from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @index: Index into rssi stats
 * @rssi_stats: Pointer to hold rssi stats
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_per_chain_rssi_stats(wmi_unified_t wmi_handle, void *evt_buf,
			      uint32_t index,
			      struct wmi_host_per_chain_rssi_stats *rssi_stats);

/**
 * wmi_extract_peer_adv_stats() - extract advance (extd2) peer stats from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @peer_adv_stats: Pointer to hold extended peer stats
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_peer_adv_stats(wmi_unified_t wmi_handle, void *evt_buf,
			   struct wmi_host_peer_adv_stats *peer_adv_stats);

#ifdef WLAN_FEATURE_MIB_STATS
/**
 * wmi_extract_mib_stats() - extract mib stats from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @mib_stats: pointer to hold mib stats
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS wmi_extract_mib_stats(wmi_unified_t wmi_handle, void *evt_buf,
				 struct mib_stats_metrics *mib_stats);
#endif

/**
 * wmi_unified_peer_stats_request_send() - send peer stats request to fw
 * @wmi_handle: wmi handle
 * @param: pointer to peer stats request parameters
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_unified_peer_stats_request_send(wmi_unified_t wmi_handle,
				    struct peer_stats_request_params *param);

/**
 * wmi_extract_peer_stats_param() - extract all stats count from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @stats_param: Pointer to hold stats count
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_peer_stats_param(wmi_unified_t wmi_handle, void *evt_buf,
			     wmi_host_stats_event *stats_param);

/**
 * wmi_extract_peer_stats_info() - extract peer stats info from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @index: Index into beacon stats
 * @peer_stats_info: Pointer to hold peer stats info
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_peer_stats_info(wmi_unified_t wmi_handle, void *evt_buf,
			    uint32_t index,
			    wmi_host_peer_stats_info *peer_stats_info);

#ifdef WLAN_FEATURE_BIG_DATA_STATS
/**
 * wmi_extract_big_data_stats_param() - extract big data statsfrom event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @stats_param: Pointer to hold stats
 *
 * Return: QDF_STATUS_SUCCESS on success and QDF_STATUS_E_FAILURE for failure
 */
QDF_STATUS
wmi_extract_big_data_stats_param(wmi_unified_t wmi_handle, void *evt_buf,
				 struct big_data_stats_event *stats_param);
#endif

#endif /* _WMI_UNIFIED_MC_CP_STATS_API_H_ */
