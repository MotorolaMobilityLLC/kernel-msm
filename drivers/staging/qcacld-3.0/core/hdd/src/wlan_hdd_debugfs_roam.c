/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_debugfs_roam.c
 *
 * WLAN Host Device Driver implementation to update
 * debugfs with roaming information
 */

#include <wlan_hdd_debugfs_csr.h>
#include <wlan_hdd_main.h>
#include <cds_sched.h>
#include <wma_api.h>
#include "qwlan_version.h"
#include "wmi_unified_param.h"
#include "wlan_osif_request_manager.h"

/**
 * hdd_roam_scan_stats_debugfs_dealloc() - Dealloc objects in hdd request mgr
 * @priv: Pointer to private data of hdd request object
 *
 * Return: None
 */
static void hdd_roam_scan_stats_debugfs_dealloc(void *priv)
{
	struct hdd_roam_scan_stats_debugfs_priv *local_priv = priv;
	struct wmi_roam_scan_stats_res *roam_scan_stats_res;

	if (!local_priv)
		return;

	roam_scan_stats_res = local_priv->roam_scan_stats_res;
	local_priv->roam_scan_stats_res = NULL;

	qdf_mem_free(roam_scan_stats_res);
}

/**
 * hdd_roam_scan_stats_cb() - Call back invoked from roam scan stats evt
 * @context: cookie to get request object
 * @res: roam scan stats response from firmware
 *
 * Return: None
 */
static void
hdd_roam_scan_stats_cb(void *context, struct wmi_roam_scan_stats_res *res)
{
	struct osif_request *request;
	struct hdd_roam_scan_stats_debugfs_priv *priv;
	struct wmi_roam_scan_stats_res *stats_res;
	uint32_t total_len;

	hdd_enter();

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete roam scan stats request");
		return;
	}

	if (!res) {
		hdd_err("Invalid response");
		goto end;
	}

	priv = osif_request_priv(request);

	total_len = sizeof(*res) + res->num_roam_scans *
		    sizeof(struct wmi_roam_scan_stats_params);

	stats_res = qdf_mem_malloc(total_len);
	if (!stats_res)
		goto end;

	qdf_mem_copy(stats_res, res, total_len);
	priv->roam_scan_stats_res = stats_res;

end:
	osif_request_complete(request);
	osif_request_put(request);

	hdd_exit();
}

/**
 * hdd_get_roam_scan_stats() - Get roam scan stats using request manager
 * @hdd_ctx: hdd context
 * @adapter: pointer to adapter
 *
 * Return: Pointer to struct wmi_roam_scan_stats_res which conatins response
 * from firmware
 */
static struct
wmi_roam_scan_stats_res *hdd_get_roam_scan_stats(struct hdd_context *hdd_ctx,
						 struct hdd_adapter *adapter)
{
	struct wmi_roam_scan_stats_res *res;
	struct wmi_roam_scan_stats_res *stats_res = NULL;
	void *context;
	struct osif_request *request;
	struct hdd_roam_scan_stats_debugfs_priv *priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_FW_ROAM_STATS,
		.dealloc = hdd_roam_scan_stats_debugfs_dealloc,
	};
	QDF_STATUS status;
	int ret;
	uint32_t total_len;

	hdd_enter();

	if (!wlan_hdd_validate_modules_state(hdd_ctx))
		return NULL;

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return NULL;
	}

	context = osif_request_cookie(request);

	status = sme_get_roam_scan_stats(hdd_ctx->mac_handle,
					 hdd_roam_scan_stats_cb,
					 context, adapter->vdev_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("roam scan stats request failed");
		goto cleanup;
	}

	ret = osif_request_wait_for_response(request);
	if (ret) {
		hdd_err("roam scan stats response time out");
		goto cleanup;
	}

	priv = osif_request_priv(request);
	res = priv->roam_scan_stats_res;
	if (!res) {
		hdd_err("Failure of roam scan stats response retrieval");
		goto cleanup;
	}

	total_len = sizeof(*res) + res->num_roam_scans *
		    sizeof(struct wmi_roam_scan_stats_params);

	stats_res = qdf_mem_malloc(total_len);
	if (!stats_res)
		goto cleanup;

	qdf_mem_copy(stats_res, res, total_len);

cleanup:
	osif_request_put(request);
	hdd_exit();

	return stats_res;
}

/**
 * hdd_roam_scan_trigger_to_str() - Get string for
 * enum WMI_ROAM_TRIGGER_REASON_ID
 * @roam_scan_trigger: roam scan trigger ID
 *
 * Return: Meaningful string from enum WMI_ROAM_TRIGGER_REASON_ID
 */
static char *hdd_roam_scan_trigger_to_str(uint32_t roam_scan_trigger)
{
	switch (roam_scan_trigger) {
	case WMI_ROAM_TRIGGER_REASON_PER:
		return "PER";
	case WMI_ROAM_TRIGGER_REASON_BMISS:
		return "BEACON MISS";
	case WMI_ROAM_TRIGGER_REASON_LOW_RSSI:
		return "LOW RSSI";
	case WMI_ROAM_TRIGGER_REASON_HIGH_RSSI:
		return "HIGH RSSI";
	case WMI_ROAM_TRIGGER_REASON_PERIODIC:
		return "PERIODIC SCAN";
	case WMI_ROAM_TRIGGER_REASON_MAWC:
		return "WMI_ROAM_TRIGGER_REASON_MAWC";
	case WMI_ROAM_TRIGGER_REASON_DENSE:
		return "DENSE ENVIRONMENT";
	case WMI_ROAM_TRIGGER_REASON_BACKGROUND:
		return "BACKGROUND SCAN";
	case WMI_ROAM_TRIGGER_REASON_FORCED:
		return "FORCED SCAN";
	case WMI_ROAM_TRIGGER_REASON_BTM:
		return "BTM TRIGGER";
	case WMI_ROAM_TRIGGER_REASON_UNIT_TEST:
		return "TEST COMMMAND";
	default:
		return "UNKNOWN REASON";
	}
	return "UNKNOWN REASON";
}

/**
 * hdd_roam_scan_trigger_value_to_str() - Get trigger value string for
 * enum WMI_ROAM_TRIGGER_REASON_ID
 * @roam_scan_trigger: roam scan trigger ID
 * @bool: output pointer to hold whether to print trigger value
 *
 * Return: Meaningful string from trigger value
 */
static char *hdd_roam_scan_trigger_value(uint32_t roam_scan_trigger,
					 bool *print)
{
	*print = true;

	switch (roam_scan_trigger) {
	case WMI_ROAM_TRIGGER_REASON_PER:
		return "percentage";
	case WMI_ROAM_TRIGGER_REASON_LOW_RSSI:
		return "dB";
	case WMI_ROAM_TRIGGER_REASON_HIGH_RSSI:
		return "dB";
	case WMI_ROAM_TRIGGER_REASON_PERIODIC:
		return "ms";
	case WMI_ROAM_TRIGGER_REASON_DENSE:
		return "(1 - Rx, 2 - Tx)";
	default:
		*print = false;
		return NULL;
	}
}

/**
 * hdd_client_id_to_str() - Helper func to get meaninful string from client id
 * @client_id: Id of the client which triggered roam scan in firmware
 *
 * Return: Meaningful string from enum WMI_SCAN_CLIENT_ID
 */
static char *hdd_client_id_to_str(uint32_t client_id)
{
	switch (client_id) {
	case WMI_SCAN_CLIENT_NLO:
		return "PNO";
	case WMI_SCAN_CLIENT_EXTSCAN:
		return "GSCAN";
	case WMI_SCAN_CLIENT_ROAM:
		return "ROAM";
	case WMI_SCAN_CLIENT_P2P:
		return "P2P";
	case WMI_SCAN_CLIENT_LPI:
		return "LPI";
	case WMI_SCAN_CLIENT_NAN:
		return "NAN";
	case WMI_SCAN_CLIENT_ANQP:
		return "ANQP";
	case WMI_SCAN_CLIENT_OBSS:
		return "OBSS";
	case WMI_SCAN_CLIENT_PLM:
		return "PLM";
	case WMI_SCAN_CLIENT_HOST:
		return "HOST";
	default:
		return "UNKNOWN";
	}
	return "UNKNOWN";
}

/**
 * hdd_roam_scan_trigger() - Print roam scan trigger info into buffer
 * @scan: roam scan event data
 * @buf: buffer to write roam scan trigger info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
hdd_roam_scan_trigger(struct wmi_roam_scan_stats_params *scan,
		      uint8_t *buf, ssize_t buf_avail_len)
{
	ssize_t length = 0;
	int ret;
	char *str;
	bool print_trigger_value;

	ret = scnprintf(buf, buf_avail_len,
			"Trigger reason is %s\n",
			hdd_roam_scan_trigger_to_str(scan->trigger_id));
	if (ret <= 0)
		return length;

	length = ret;

	str = hdd_roam_scan_trigger_value(scan->trigger_id,
					  &print_trigger_value);
	if (!print_trigger_value || !str)
		return length;

	if (length >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		length = buf_avail_len;
		return length;
	}

	ret = scnprintf(buf + length, buf_avail_len - length,
			"Trigger value is: %u %s\n",
			scan->trigger_value, str);
	if (ret <= 0)
		return length;

	length += ret;
	return length;
}

/**
 * hdd_roam_scan_chan() - Print roam scan chan freq info into buffer
 * @scan: roam scan event data
 * @buf: buffer to write roam scan freq info
 * @buf_avail_len: available buffer length
 *
 * Return: No.of bytes populated by this function in buffer
 */
static ssize_t
hdd_roam_scan_chan(struct wmi_roam_scan_stats_params *scan,
		   uint8_t *buf, ssize_t buf_avail_len)
{
	ssize_t length = 0;
	uint32_t i;
	int ret;

	ret = scnprintf(buf, buf_avail_len,
			"Num of scan channels: %u\n"
			"scan channel list:",
			scan->num_scan_chans);
	if (ret <= 0)
		return length;

	length = ret;

	for (i = 0; i < scan->num_scan_chans; i++) {
		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			length = buf_avail_len;
			return length;
		}

		ret = scnprintf(buf + length, buf_avail_len - length,
				"%u ", scan->scan_freqs[i]);
		if (ret <= 0)
			return length;

		length += ret;
	}

	return length;
}

/**
 * wlan_hdd_update_roam_stats() - Internal function to get roam scan stats
 * @hdd_ctx: hdd context
 * @adapter: pointer to adapter
 * @buf: buffer to hold the stats
 * @len: maximum available length in response buffer
 *
 * Return: Size of formatted roam scan response stats
 */
static ssize_t
wlan_hdd_update_roam_stats(struct hdd_context *hdd_ctx,
			   struct hdd_adapter *adapter,
			   uint8_t *buf, ssize_t buf_avail_len)
{
	ssize_t length = 0;
	struct wmi_roam_scan_stats_res *roam_stats;
	struct wmi_roam_scan_stats_params *scan;
	int ret;
	int rsi; /* roam scan iterator */
	int rci; /* roam candidate iterator */

	roam_stats = hdd_get_roam_scan_stats(hdd_ctx, adapter);
	if (!roam_stats) {
		hdd_err("Couldn't get roam stats");
		ret = scnprintf(buf, buf_avail_len,
				"Failed to fetch roam stats\n");
		if (ret <= 0)
			return length;
		length += ret;
		return length;
	}

	ret = scnprintf(buf, buf_avail_len,
			"\n\nStats of last %u roam scans\n",
			roam_stats->num_roam_scans);
	if (ret <= 0)
		goto free_mem;
	length += ret;

	for (rsi = 0; rsi < roam_stats->num_roam_scans; rsi++) {
		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			length = buf_avail_len;
			goto free_mem;
		}

		scan = &roam_stats->roam_scan[rsi];
		ret = scnprintf(buf + length, buf_avail_len - length,
				"\nRoam scan[%u] details\n", rsi);
		if (ret <= 0)
			goto free_mem;
		length += ret;

		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			length = buf_avail_len;
			goto free_mem;
		}

		ret = scnprintf(buf + length, buf_avail_len - length,
				"This scan is triggered by \"%s\" scan client\n",
				hdd_client_id_to_str(scan->client_id));

		if (ret <= 0)
			goto free_mem;
		length += ret;

		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			length = buf_avail_len;
			goto free_mem;
		}

		length += hdd_roam_scan_trigger(scan, buf + length,
						buf_avail_len - length);
		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			length = buf_avail_len;
			goto free_mem;
		}

		length += hdd_roam_scan_chan(scan, buf + length,
					     buf_avail_len - length);
		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			length = buf_avail_len;
			goto free_mem;
		}

		ret = scnprintf(buf + length, buf_avail_len - length,
				"\nRoam Scan time: 0x%llx\n",
				roam_stats->roam_scan[rsi].time_stamp);
		if (ret <= 0)
			goto free_mem;
		length += ret;

		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			length = buf_avail_len;
			goto free_mem;
		}

		if (scan->is_roam_successful) {
			ret = scnprintf(buf + length,
					buf_avail_len - length,
					"\nSTA roamed from "
					QDF_FULL_MAC_FMT " to "
					QDF_FULL_MAC_FMT "\n",
					QDF_FULL_MAC_REF(scan->old_bssid),
					QDF_FULL_MAC_REF(scan->new_bssid));
		} else {
			ret = scnprintf(buf + length,
					buf_avail_len - length,
					"\nSTA is connected to " QDF_FULL_MAC_FMT
					" before and after scan, not roamed\n",
					QDF_FULL_MAC_REF(scan->old_bssid));
		}
		if (ret <= 0)
			goto free_mem;
		length += ret;

		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			length = buf_avail_len;
			goto free_mem;
		}

		ret = scnprintf(buf + length, buf_avail_len - length,
				"Roam candidate details\n");
		if (ret <= 0)
			goto free_mem;
		length += ret;

		if (length >= buf_avail_len) {
			hdd_err("No sufficient buf_avail_len");
			length = buf_avail_len;
			goto free_mem;
		}

		ret = scnprintf(buf + length, buf_avail_len - length,
				"      BSSID     FREQ   SCORE  RSSI\n");
		if (ret <= 0)
			goto free_mem;
		length += ret;

		for (rci = 0; rci < scan->num_roam_candidates; rci++) {
			uint8_t *bssid = scan->cand[rci].bssid;

			if (length >= buf_avail_len) {
				hdd_err("No sufficient buf_avail_len");
				length = buf_avail_len;
				goto free_mem;
			}

			ret = scnprintf(buf + length,
					buf_avail_len - length,
					QDF_FULL_MAC_FMT " %4u  %3u   %3u\n",
					QDF_FULL_MAC_REF(bssid),
					scan->cand[rci].freq,
					scan->cand[rci].score,
					scan->cand[rci].rssi);
			if (ret <= 0)
				goto free_mem;
			length += ret;
		}
	}

free_mem:
	qdf_mem_free(roam_stats);
	return length;
}

ssize_t
wlan_hdd_debugfs_update_roam_stats(struct hdd_context *hdd_ctx,
				   struct hdd_adapter *adapter,
				   uint8_t *buf, ssize_t buf_avail_len)
{
	ssize_t len = 0;
	int ret_val;

	hdd_enter();

	len = wlan_hdd_current_time_info_debugfs(buf, buf_avail_len - len);

	if (len >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}
	if (adapter->device_mode != QDF_STA_MODE) {
		ret_val = scnprintf(buf + len, buf_avail_len - len,
				    "Interface is not in STA Mode\n");
		if (ret_val <= 0)
			return len;

		len += ret_val;
		return len;
	}

	if (len >= buf_avail_len) {
		hdd_err("No sufficient buf_avail_len");
		return buf_avail_len;
	}
	len += wlan_hdd_update_roam_stats(hdd_ctx, adapter, buf + len,
					  buf_avail_len - len);

	hdd_exit();

	return len;
}
