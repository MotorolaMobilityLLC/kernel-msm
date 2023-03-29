/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
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
 * DOC: Implement API's specific to cp stats component.
 */

#include "wmi_unified_priv.h"
#include "wmi_unified_param.h"
#include "wmi_unified_mc_cp_stats_api.h"

QDF_STATUS
wmi_extract_per_chain_rssi_stats(wmi_unified_t wmi_handle, void *evt_buf,
			       uint32_t index,
			       struct wmi_host_per_chain_rssi_stats *rssi_stats)
{
	if (wmi_handle->ops->extract_per_chain_rssi_stats)
		return wmi_handle->ops->extract_per_chain_rssi_stats(wmi_handle,
			evt_buf, index, rssi_stats);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_extract_peer_adv_stats(wmi_unified_t wmi_handle, void *evt_buf,
			   struct wmi_host_peer_adv_stats *peer_adv_stats)
{
	if (wmi_handle->ops->extract_peer_adv_stats)
		return wmi_handle->ops->extract_peer_adv_stats(wmi_handle,
			evt_buf, peer_adv_stats);

	return QDF_STATUS_E_FAILURE;
}

#ifdef WLAN_FEATURE_MIB_STATS
QDF_STATUS wmi_extract_mib_stats(wmi_unified_t wmi_handle, void *evt_buf,
				 struct mib_stats_metrics *mib_stats)
{
	if (wmi_handle->ops->extract_mib_stats)
		return wmi_handle->ops->extract_mib_stats(wmi_handle,
							  evt_buf,
							  mib_stats);

	return QDF_STATUS_E_FAILURE;
}
#endif

QDF_STATUS
wmi_unified_peer_stats_request_send(wmi_unified_t wmi_handle,
				    struct peer_stats_request_params *param)
{
	if (wmi_handle->ops->send_request_peer_stats_info_cmd)
		return wmi_handle->ops->send_request_peer_stats_info_cmd(
							     wmi_handle, param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_extract_peer_stats_param(wmi_unified_t wmi_handle, void *evt_buf,
			     wmi_host_stats_event *stats_param)
{
	if (wmi_handle->ops->extract_peer_stats_count)
		return wmi_handle->ops->extract_peer_stats_count(wmi_handle,
			evt_buf, stats_param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_extract_peer_stats_info(wmi_unified_t wmi_handle, void *evt_buf,
			    uint32_t index,
			    wmi_host_peer_stats_info *peer_stats_info)
{
	if (wmi_handle->ops->extract_peer_stats_info)
		return wmi_handle->ops->extract_peer_stats_info(wmi_handle,
				evt_buf, index, peer_stats_info);

	return QDF_STATUS_E_FAILURE;
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
QDF_STATUS
wmi_extract_big_data_stats_param(wmi_unified_t wmi_handle, void *evt_buf,
				 struct big_data_stats_event *stats_param)
{
	if (wmi_handle->ops->extract_big_data_stats)
		return wmi_handle->ops->extract_big_data_stats(wmi_handle,
				evt_buf, stats_param);

	return QDF_STATUS_E_FAILURE;
}
#endif
