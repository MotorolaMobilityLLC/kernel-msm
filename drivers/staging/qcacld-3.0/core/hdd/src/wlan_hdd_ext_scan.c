/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_ext_scan.c
 *
 * WLAN Host Device Driver EXT SCAN feature implementation
 *
 */

#ifdef FEATURE_WLAN_EXTSCAN

#include "osif_sync.h"
#include "wlan_hdd_ext_scan.h"
#include "wlan_hdd_regulatory.h"
#include "cds_utils.h"
#include "cds_sched.h"
#include <qca_vendor.h>
#include "wlan_extscan_ucfg_api.h"
#include "wlan_hdd_scan.h"

/* amount of time to wait for a synchronous request/response operation */
#define WLAN_WAIT_TIME_EXTSCAN  1000

/**
 * struct hdd_ext_scan_context - hdd ext scan context
 * @request_id: userspace-assigned ID associated with the request
 * @response_event: Ext scan wait event
 * @response_status: Status returned by FW in response to a request
 * @ignore_cached_results: Flag to ignore cached results or not
 * @context_lock: Spinlock to serialize all context accesses
 * @capability_response: Ext scan capability response data from target
 * @buckets_scanned: bitmask of buckets scanned in extscan cycle
 */
struct hdd_ext_scan_context {
	uint32_t request_id;
	int response_status;
	bool ignore_cached_results;
	struct completion response_event;
	spinlock_t context_lock;
	struct ext_scan_capabilities_response capability_response;
	uint32_t buckets_scanned;
};
static struct hdd_ext_scan_context ext_scan_context;

const struct nla_policy
wlan_hdd_extscan_config_policy[EXTSCAN_PARAM_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CHANNEL] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_DWELL_TIME] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_PASSIVE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CLASS] = {.type = NLA_U8},

	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_INDEX] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_BAND] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_PERIOD] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_REPORT_EVENTS] = {
				.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_BASE_PERIOD] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_MAX_AP_PER_SCAN] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD_PERCENT] = {
				.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD_NUM_SCANS] = {
				.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_NUM_BUCKETS] = {
				.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_FLUSH] = {
				.type = NLA_U8},

	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_MAX] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID] =
				VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW] = {
				.type = NLA_S32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH] = {
				.type = NLA_S32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_CHANNEL] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_NUM_AP] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_RSSI_SAMPLE_SIZE] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_LOST_AP_SAMPLE_SIZE] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_MIN_BREACHING] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_NUM_AP] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_MAX_PERIOD] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_BASE] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_STEP_COUNT] = {
				.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_SSID] = {
				.type = NLA_BINARY,
				.len = IEEE80211_MAX_SSID_LEN + 1 },
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_HOTLIST_PARAMS_LOST_SSID_SAMPLE_SIZE] = {
				.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_HOTLIST_PARAMS_NUM_SSID] = {
				.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_BAND] = {
				.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_RSSI_LOW] = {
				.type = NLA_S32 },
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_SSID_THRESHOLD_PARAM_RSSI_HIGH] = {
				.type = NLA_S32 },
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_CONFIGURATION_FLAGS] = {
				.type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_LOST_AP_SAMPLE_SIZE] = {
				.type = NLA_U32},
};

const struct nla_policy
wlan_hdd_pno_config_policy[QCA_WLAN_VENDOR_ATTR_PNO_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_LIST_PARAM_NUM] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ID] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_NUM_NETWORKS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_SSID] = {
		.type = NLA_BINARY,
		.len = IEEE80211_MAX_SSID_LEN + 1
	},
	[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_FLAGS] = {
		.type = NLA_U8
	},
	[QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_AUTH_BIT] = {
		.type = NLA_U8
	},
	[QCA_WLAN_VENDOR_ATTR_EPNO_MIN5GHZ_RSSI] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_EPNO_MIN24GHZ_RSSI] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_EPNO_INITIAL_SCORE_MAX] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_EPNO_CURRENT_CONNECTION_BONUS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_EPNO_SAME_NETWORK_BONUS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_EPNO_SECURE_BONUS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_EPNO_BAND5GHZ_BONUS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_PNO_CONFIG_REQUEST_ID] = {
		.type = NLA_U32
	},
};

/**
 * wlan_hdd_cfg80211_extscan_get_capabilities_rsp() - response from target
 * @hdd_ctx: Pointer to hdd context
 * @data: Pointer to ext scan capabilities response from fw
 *
 * Return: None
 */
static void
wlan_hdd_cfg80211_extscan_get_capabilities_rsp(struct hdd_context *hdd_ctx,
	struct ext_scan_capabilities_response *data)
{
	struct hdd_ext_scan_context *context;

	hdd_enter();

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	context = &ext_scan_context;

	spin_lock(&context->context_lock);
	/* validate response received from target*/
	if (context->request_id != data->requestId) {
		spin_unlock(&context->context_lock);
		hdd_err("Target response id did not match. request_id: %d response_id: %d",
			context->request_id, data->requestId);
		return;
	}

	context->capability_response = *data;
	complete(&context->response_event);
	spin_unlock(&context->context_lock);
}

/*
 * define short names for the global vendor params
 * used by hdd_extscan_nl_fill_bss()
 */
#define PARAM_TIME_STAMP \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_TIME_STAMP
#define PARAM_SSID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_SSID
#define PARAM_BSSID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BSSID
#define PARAM_CHANNEL \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CHANNEL
#define PARAM_RSSI \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RSSI
#define PARAM_RTT \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT
#define PARAM_RTT_SD \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT_SD
#define PARAM_BEACON_PERIOD \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BEACON_PERIOD
#define PARAM_CAPABILITY \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CAPABILITY
#define PARAM_IE_LENGTH \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_IE_LENGTH
#define PARAM_IE_DATA \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_IE_DATA

/** hdd_extscan_nl_fill_bss() - extscan nl fill bss
 * @skb: socket buffer
 * @ap: bss information
 * @idx: nesting index
 *
 * Return: 0 on success; error number otherwise
 */
static int hdd_extscan_nl_fill_bss(struct sk_buff *skb, tSirWifiScanResult *ap,
					int idx)
{
	struct nlattr *nla_ap;

	nla_ap = nla_nest_start(skb, idx);
	if (!nla_ap)
		return -EINVAL;

	if (hdd_wlan_nla_put_u64(skb, PARAM_TIME_STAMP, ap->ts) ||
	    nla_put(skb, PARAM_SSID, sizeof(ap->ssid), ap->ssid) ||
	    nla_put(skb, PARAM_BSSID, sizeof(ap->bssid), ap->bssid.bytes) ||
	    nla_put_u32(skb, PARAM_CHANNEL, ap->channel) ||
	    nla_put_s32(skb, PARAM_RSSI, ap->rssi) ||
	    nla_put_u32(skb, PARAM_RTT, ap->rtt) ||
	    nla_put_u32(skb, PARAM_RTT_SD, ap->rtt_sd) ||
	    nla_put_u16(skb, PARAM_BEACON_PERIOD, ap->beaconPeriod) ||
	    nla_put_u16(skb, PARAM_CAPABILITY, ap->capability) ||
	    nla_put_u16(skb, PARAM_IE_LENGTH, ap->ieLength)) {
		hdd_err("put fail");
		return -EINVAL;
	}

	if (ap->ieLength)
		if (nla_put(skb, PARAM_IE_DATA, ap->ieLength, ap->ieData)) {
			hdd_err("put fail");
			return -EINVAL;
		}

	nla_nest_end(skb, nla_ap);

	return 0;
}
/*
 * done with short names for the global vendor params
 * used by hdd_extscan_nl_fill_bss()
 */
#undef PARAM_TIME_STAMP
#undef PARAM_SSID
#undef PARAM_BSSID
#undef PARAM_CHANNEL
#undef PARAM_RSSI
#undef PARAM_RTT
#undef PARAM_RTT_SD
#undef PARAM_BEACON_PERIOD
#undef PARAM_CAPABILITY
#undef PARAM_IE_LENGTH
#undef PARAM_IE_DATA

/**
 * wlan_hdd_cfg80211_extscan_cached_results_ind() - get cached results
 * @hdd_ctx: hdd global context
 * @data: cached results
 *
 * This function reads the cached results %data, populated the NL
 * attributes and sends the NL event to the upper layer.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_cached_results_ind(struct hdd_context *hdd_ctx,
				struct extscan_cached_scan_results *data)
{
	struct sk_buff *skb = NULL;
	struct hdd_ext_scan_context *context;
	struct extscan_cached_scan_result *result;
	tSirWifiScanResult *ap;
	uint32_t i, j, nl_buf_len;
	bool ignore_cached_results = false;

	/* ENTER() intentionally not used in a frequently invoked API */

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	ignore_cached_results = context->ignore_cached_results;
	spin_unlock(&context->context_lock);

	if (ignore_cached_results) {
		hdd_err("Ignore the cached results received after timeout");
		return;
	}

#define EXTSCAN_CACHED_NEST_HDRLEN NLA_HDRLEN
#define EXTSCAN_CACHED_NL_FIXED_TLV \
		((sizeof(data->request_id) + NLA_HDRLEN) + \
		(sizeof(data->num_scan_ids) + NLA_HDRLEN) + \
		(sizeof(data->more_data) + NLA_HDRLEN))
#define EXTSCAN_CACHED_NL_SCAN_ID_TLV \
		((sizeof(result->scan_id) + NLA_HDRLEN) + \
		(sizeof(result->flags) + NLA_HDRLEN) + \
		(sizeof(result->num_results) + NLA_HDRLEN))+ \
		(sizeof(result->buckets_scanned) + NLA_HDRLEN)
#define EXTSCAN_CACHED_NL_SCAN_RESULTS_TLV \
		((sizeof(ap->ts) + NLA_HDRLEN) + \
		(sizeof(ap->ssid) + NLA_HDRLEN) + \
		(sizeof(ap->bssid) + NLA_HDRLEN) + \
		(sizeof(ap->channel) + NLA_HDRLEN) + \
		(sizeof(ap->rssi) + NLA_HDRLEN) + \
		(sizeof(ap->rtt) + NLA_HDRLEN) + \
		(sizeof(ap->rtt_sd) + NLA_HDRLEN) + \
		(sizeof(ap->beaconPeriod) + NLA_HDRLEN) + \
		(sizeof(ap->capability) + NLA_HDRLEN) + \
		(sizeof(ap->ieLength) + NLA_HDRLEN))
#define EXTSCAN_CACHED_NL_SCAN_RESULTS_IE_DATA_TLV \
		(ap->ieLength + NLA_HDRLEN)

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len += EXTSCAN_CACHED_NL_FIXED_TLV;
	if (data->num_scan_ids) {
		nl_buf_len += sizeof(result->scan_id) + NLA_HDRLEN;
		nl_buf_len += EXTSCAN_CACHED_NEST_HDRLEN;
		result = &data->result[0];
		for (i = 0; i < data->num_scan_ids; i++) {
			nl_buf_len += EXTSCAN_CACHED_NEST_HDRLEN;
			nl_buf_len += EXTSCAN_CACHED_NL_SCAN_ID_TLV;
			nl_buf_len += EXTSCAN_CACHED_NEST_HDRLEN;

			ap = &result->ap[0];
			for (j = 0; j < result->num_results; j++) {
				nl_buf_len += EXTSCAN_CACHED_NEST_HDRLEN;
				nl_buf_len +=
					EXTSCAN_CACHED_NL_SCAN_RESULTS_TLV;
				if (ap->ieLength)
					nl_buf_len +=
					EXTSCAN_CACHED_NL_SCAN_RESULTS_IE_DATA_TLV;
				ap++;
			}
			result++;
		}
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);

	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		goto fail;
	}
	hdd_debug("Req Id %u Num_scan_ids %u More Data %u",
		data->request_id, data->num_scan_ids, data->more_data);

	result = &data->result[0];
	for (i = 0; i < data->num_scan_ids; i++) {
		hdd_debug("[i=%d] scan_id %u flags %u num_results %u buckets scanned %u",
			i, result->scan_id, result->flags, result->num_results,
			result->buckets_scanned);

		ap = &result->ap[0];
		for (j = 0; j < result->num_results; j++) {
			/*
			 * Firmware returns timestamp from ext scan start till
			 * BSSID was cached (in micro seconds). Add this with
			 * time gap between system boot up to ext scan start
			 * to derive the time since boot when the
			 * BSSID was cached.
			 */
			ap->ts += hdd_ctx->ext_scan_start_since_boot;
			hdd_debug("Timestamp %llu "
				"Ssid: %s "
				"Bssid (" QDF_MAC_ADDR_FMT ") "
				"Channel %u "
				"Rssi %d "
				"RTT %u "
				"RTT_SD %u "
				"Beacon Period %u "
				"Capability 0x%x "
				"Ie length %d",
				ap->ts,
				ap->ssid,
				QDF_MAC_ADDR_REF(ap->bssid.bytes),
				ap->channel,
				ap->rssi,
				ap->rtt,
				ap->rtt_sd,
				ap->beaconPeriod,
				ap->capability,
				ap->ieLength);
			ap++;
		}
		result++;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->request_id) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
		data->num_scan_ids) ||
	    nla_put_u8(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
		data->more_data)) {
		hdd_err("put fail");
		goto fail;
	}

	if (data->num_scan_ids) {
		struct nlattr *nla_results;

		result = &data->result[0];

		if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_CACHED_RESULTS_SCAN_ID,
			result->scan_id)) {
			hdd_err("put fail");
			goto fail;
		}
		nla_results = nla_nest_start(skb,
			      QCA_WLAN_VENDOR_ATTR_EXTSCAN_CACHED_RESULTS_LIST);
		if (!nla_results)
			goto fail;

		for (i = 0; i < data->num_scan_ids; i++) {
			struct nlattr *nla_result;
			struct nlattr *nla_aps;

			nla_result = nla_nest_start(skb, i);
			if (!nla_result)
				goto fail;

			if (nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CACHED_RESULTS_SCAN_ID,
				result->scan_id) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CACHED_RESULTS_FLAGS,
				result->flags) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_BUCKETS_SCANNED,
				result->buckets_scanned) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
				result->num_results)) {
				hdd_err("put fail");
				goto fail;
			}

			nla_aps = nla_nest_start(skb,
				     QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
			if (!nla_aps)
				goto fail;

			ap = &result->ap[0];
			for (j = 0; j < result->num_results; j++) {
				if (hdd_extscan_nl_fill_bss(skb, ap, j))
					goto fail;

				ap++;
			}
			nla_nest_end(skb, nla_aps);
			nla_nest_end(skb, nla_result);
			result++;
		}
		nla_nest_end(skb, nla_results);
	}

	cfg80211_vendor_cmd_reply(skb);

	if (!data->more_data) {
		spin_lock(&context->context_lock);
		context->response_status = 0;
		complete(&context->response_event);
		spin_unlock(&context->context_lock);
	}
	return;

fail:
	if (skb)
		kfree_skb(skb);

	spin_lock(&context->context_lock);
	context->response_status = -EINVAL;
	spin_unlock(&context->context_lock);
}

/**
 * wlan_hdd_cfg80211_extscan_hotlist_match_ind() - hot list match ind
 * @hdd_ctx: Pointer to hdd context
 * @data: Pointer to ext scan result event
 *
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_hotlist_match_ind(struct hdd_context *hdd_ctx,
					    struct extscan_hotlist_match *data)
{
	struct sk_buff *skb = NULL;
	uint32_t i, index;
	int flags = cds_get_gfp_flags();

	hdd_enter();

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	if (data->ap_found)
		index = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_FOUND_INDEX;
	else
		index = QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_HOTLIST_AP_LOST_INDEX;

	skb = cfg80211_vendor_event_alloc(
		  hdd_ctx->wiphy,
		  NULL,
		  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
		  index, flags);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}
	hdd_debug("Req Id: %u Num_APs: %u MoreData: %u ap_found: %u",
			data->requestId, data->numOfAps, data->moreData,
			data->ap_found);

	for (i = 0; i < data->numOfAps; i++) {
		data->ap[i].ts = qdf_get_monotonic_boottime();

		hdd_debug("[i=%d] Timestamp %llu "
		       "Ssid: %s "
		       "Bssid (" QDF_MAC_ADDR_FMT ") "
		       "Channel %u "
		       "Rssi %d "
		       "RTT %u "
		       "RTT_SD %u",
		       i,
		       data->ap[i].ts,
		       data->ap[i].ssid,
		       QDF_MAC_ADDR_REF(data->ap[i].bssid.bytes),
		       data->ap[i].channel,
		       data->ap[i].rssi,
		       data->ap[i].rtt, data->ap[i].rtt_sd);
	}

	if (nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->requestId) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
		data->numOfAps)) {
		hdd_err("put fail");
		goto fail;
	}

	if (data->numOfAps) {
		struct nlattr *aps;

		aps = nla_nest_start(skb,
			       QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
		if (!aps)
			goto fail;

		for (i = 0; i < data->numOfAps; i++) {
			struct nlattr *ap;

			ap = nla_nest_start(skb, i);
			if (!ap)
				goto fail;

			if (hdd_wlan_nla_put_u64(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_TIME_STAMP,
				data->ap[i].ts) ||
			    nla_put(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_SSID,
				sizeof(data->ap[i].ssid),
				data->ap[i].ssid) ||
			    nla_put(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BSSID,
				sizeof(data->ap[i].bssid),
				data->ap[i].bssid.bytes) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CHANNEL,
				data->ap[i].channel) ||
			    nla_put_s32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RSSI,
				data->ap[i].rssi) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT,
				data->ap[i].rtt) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT_SD,
				data->ap[i].rtt_sd))
				goto fail;

			nla_nest_end(skb, ap);
		}
		nla_nest_end(skb, aps);

		if (nla_put_u8(skb,
		       QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
		       data->moreData))
			goto fail;
	}

	cfg80211_vendor_event(skb, flags);
	hdd_exit();
	return;

fail:
	kfree_skb(skb);
}

/**
 * wlan_hdd_cfg80211_extscan_signif_wifi_change_results_ind() -
 *	significant wifi change results indication
 * @hdd_ctx: Pointer to hdd context
 * @data: Pointer to signif wifi change event
 *
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_signif_wifi_change_results_ind(
			struct hdd_context *hdd_ctx,
			tpSirWifiSignificantChangeEvent data)
{
	struct sk_buff *skb = NULL;
	tSirWifiSignificantChange *ap_info;
	int32_t *rssi;
	uint32_t i, j;
	int flags = cds_get_gfp_flags();

	hdd_enter();

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	skb = cfg80211_vendor_event_alloc(
		hdd_ctx->wiphy,
		NULL,
		EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
		QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SIGNIFICANT_CHANGE_INDEX,
		flags);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}
	hdd_debug("Req Id %u Num results %u More Data %u",
		data->requestId, data->numResults, data->moreData);

	ap_info = &data->ap[0];
	for (i = 0; i < data->numResults; i++) {
		hdd_debug("[i=%d] "
		       "Bssid (" QDF_MAC_ADDR_FMT ") "
		       "Channel %u "
		       "numOfRssi %d",
		       i,
		       QDF_MAC_ADDR_REF(ap_info->bssid.bytes),
		       ap_info->channel, ap_info->numOfRssi);
		rssi = &(ap_info)->rssi[0];
		for (j = 0; j < ap_info->numOfRssi; j++)
			hdd_debug("Rssi %d", *rssi++);

		ap_info = (tSirWifiSignificantChange *)((char *)ap_info +
				ap_info->numOfRssi * sizeof(*rssi) +
				sizeof(*ap_info));
	}

	if (nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->requestId) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
		data->numResults)) {
		hdd_err("put fail");
		goto fail;
	}

	if (data->numResults) {
		struct nlattr *aps;

		aps = nla_nest_start(skb,
			       QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
		if (!aps)
			goto fail;

		ap_info = &data->ap[0];
		for (i = 0; i < data->numResults; i++) {
			struct nlattr *ap;

			ap = nla_nest_start(skb, i);
			if (!ap)
				goto fail;

			if (nla_put(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_BSSID,
				QDF_MAC_ADDR_SIZE, ap_info->bssid.bytes) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_CHANNEL,
				ap_info->channel) ||
			    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_NUM_RSSI,
				ap_info->numOfRssi) ||
			    nla_put(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SIGNIFICANT_CHANGE_RESULT_RSSI_LIST,
				sizeof(s32) * ap_info->numOfRssi,
				&(ap_info)->rssi[0]))
				goto fail;

			nla_nest_end(skb, ap);

			ap_info = (tSirWifiSignificantChange *)((char *)ap_info
					+ ap_info->numOfRssi * sizeof(*rssi) +
					sizeof(*ap_info));
		}
		nla_nest_end(skb, aps);

		if (nla_put_u8(skb,
		     QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
		     data->moreData))
			goto fail;
	}

	cfg80211_vendor_event(skb, flags);
	return;

fail:
	kfree_skb(skb);
	return;

}

/**
 * wlan_hdd_cfg80211_extscan_full_scan_result_event() - full scan result event
 * @hdd_ctx: Pointer to hdd context
 * @data: Pointer to full scan result event
 *
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_full_scan_result_event(struct hdd_context *hdd_ctx,
						 tpSirWifiFullScanResultEvent
						 data)
{
	struct sk_buff *skb;
	struct hdd_ext_scan_context *context;

	int flags = cds_get_gfp_flags();

	/* ENTER() intentionally not used in a frequently invoked API */

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	if ((sizeof(*data) + data->ap.ieLength) >= EXTSCAN_EVENT_BUF_SIZE) {
		hdd_err("Frame exceeded NL size limitation, drop it!!");
		return;
	}
	skb = cfg80211_vendor_event_alloc(
		  hdd_ctx->wiphy,
		  NULL,
		  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
		  QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_FULL_SCAN_RESULT_INDEX,
		  flags);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	data->ap.channel = cds_chan_to_freq(data->ap.channel);

	/*
	 * Android does not want the time stamp from the frame.
	 * Instead it wants a monotonic increasing value since boot
	 */
	data->ap.ts = qdf_get_monotonic_boottime();

	hdd_debug("Req Id %u More Data %u", data->requestId,
		  data->moreData);
	hdd_debug("AP Info: Timestamp %llu Ssid: %s "
	       "Bssid (" QDF_MAC_ADDR_FMT ") "
	       "Channel %u "
	       "Rssi %d "
	       "RTT %u "
	       "RTT_SD %u "
	       "Bcn Period %d "
	       "Capability 0x%X "
	       "IE Length %d",
	       data->ap.ts,
	       data->ap.ssid,
	       QDF_MAC_ADDR_REF(data->ap.bssid.bytes),
	       data->ap.channel,
	       data->ap.rssi,
	       data->ap.rtt,
	       data->ap.rtt_sd,
	       data->ap.beaconPeriod,
	       data->ap.capability, data->ap.ieLength);

	if (nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->requestId) ||
	    hdd_wlan_nla_put_u64(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_TIME_STAMP,
		data->ap.ts) ||
	    nla_put(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_SSID,
		sizeof(data->ap.ssid),
		data->ap.ssid) ||
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BSSID,
		sizeof(data->ap.bssid),
		data->ap.bssid.bytes) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CHANNEL,
		data->ap.channel) ||
	    nla_put_s32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RSSI,
		data->ap.rssi) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT,
		data->ap.rtt) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_RTT_SD,
		data->ap.rtt_sd) ||
	    nla_put_u16(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_BEACON_PERIOD,
		data->ap.beaconPeriod) ||
	    nla_put_u16(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_CAPABILITY,
		data->ap.capability) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_IE_LENGTH,
		data->ap.ieLength) ||
	    nla_put_u8(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
		data->moreData)) {
		hdd_err("nla put fail");
		goto nla_put_failure;
	}

	if (data->ap.ieLength) {
		if (nla_put(skb,
		    QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_IE_DATA,
		    data->ap.ieLength, data->ap.ieData))
			goto nla_put_failure;
	}

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	if (nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_BUCKETS_SCANNED,
		context->buckets_scanned)) {
		spin_unlock(&context->context_lock);
		hdd_debug("Failed to include buckets_scanned");
		goto nla_put_failure;
	}
	spin_unlock(&context->context_lock);

	cfg80211_vendor_event(skb, flags);
	return;

nla_put_failure:
	kfree_skb(skb);
}

/**
 * wlan_hdd_cfg80211_extscan_scan_res_available_event() - scan result event
 * @hdd_ctx: Pointer to hdd context
 * @data: Pointer to scan results available indication param
 *
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_scan_res_available_event(
			struct hdd_context *hdd_ctx,
			tpSirExtScanResultsAvailableIndParams data)
{
	struct sk_buff *skb;
	int flags = cds_get_gfp_flags();

	hdd_enter();

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	skb = cfg80211_vendor_event_alloc(
		 hdd_ctx->wiphy,
		 NULL,
		 EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
		 QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_RESULTS_AVAILABLE_INDEX,
		 flags);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	hdd_debug("Req Id %u Num results %u",
	       data->requestId, data->numResultsAvailable);
	if (nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->requestId) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
		data->numResultsAvailable)) {
		hdd_err("nla put fail");
		goto nla_put_failure;
	}

	cfg80211_vendor_event(skb, flags);
	hdd_exit();
	return;

nla_put_failure:
	kfree_skb(skb);
}

/**
 * wlan_hdd_cfg80211_extscan_scan_progress_event() - scan progress event
 * @hdd_ctx: Pointer to hdd context
 * @data: Pointer to scan event indication param
 *
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_scan_progress_event(struct hdd_context *hdd_ctx,
					      tpSirExtScanOnScanEventIndParams
					      data)
{
	struct sk_buff *skb;
	int flags = cds_get_gfp_flags();
	struct hdd_ext_scan_context *context;

	/* ENTER() intentionally not used in a frequently invoked API */

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	skb = cfg80211_vendor_event_alloc(
			hdd_ctx->wiphy,
			NULL,
			EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
			QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_SCAN_EVENT_INDEX,
			flags);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	hdd_debug("Request Id: %u Scan event type: %u Scan event status: %u buckets scanned: %u",
		  data->requestId, data->scanEventType, data->status,
		  data->buckets_scanned);

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	if (data->scanEventType == WIFI_EXTSCAN_CYCLE_COMPLETED_EVENT) {
		context->buckets_scanned = 0;
		data->scanEventType = WIFI_EXTSCAN_RESULTS_AVAILABLE;
		spin_unlock(&context->context_lock);
	} else if (data->scanEventType == WIFI_EXTSCAN_CYCLE_STARTED_EVENT) {
		context->buckets_scanned = data->buckets_scanned;
		/* No need to report to user space */
		spin_unlock(&context->context_lock);
		goto nla_put_failure;
	} else {
		spin_unlock(&context->context_lock);
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
			data->requestId) ||
	    nla_put_u8(skb,
		       QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_EVENT_TYPE,
		       data->scanEventType)) {
		hdd_err("nla put fail");
		goto nla_put_failure;
	}

	cfg80211_vendor_event(skb, flags);
	return;

nla_put_failure:
	kfree_skb(skb);
}

/**
 * wlan_hdd_cfg80211_extscan_epno_match_found() - pno match found
 * @hdd_ctx: HDD context
 * @data: matched network data
 *
 * This function reads the matched network data and fills NL vendor attributes
 * and send it to upper layer.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: 0 on success, error number otherwise
 */
static void
wlan_hdd_cfg80211_extscan_epno_match_found(struct hdd_context *hdd_ctx,
					   struct pno_match_found *data)
{
	struct sk_buff *skb;
	uint32_t len, i;
	int flags = cds_get_gfp_flags();

	hdd_enter();

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	/*
	 * If the number of match found APs including IE data exceeds NL 4K size
	 * limitation, drop that beacon/probe rsp frame.
	 */
	len = sizeof(*data) +
			(data->num_results + sizeof(tSirWifiScanResult));
	for (i = 0; i < data->num_results; i++)
		len += data->ap[i].ieLength;

	if (len >= EXTSCAN_EVENT_BUF_SIZE) {
		hdd_err("Frame exceeded NL size limitation, drop it!");
		return;
	}

	skb = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
		  NULL,
		  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
		QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_NETWORK_FOUND_INDEX,
		  flags);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	hdd_debug("Req Id %u More Data %u num_results %d",
		data->request_id, data->more_data, data->num_results);
	for (i = 0; i < data->num_results; i++) {
		data->ap[i].channel = cds_chan_to_freq(data->ap[i].channel);
		hdd_debug("AP Info: Timestamp %llu) Ssid: %s "
					"Bssid (" QDF_MAC_ADDR_FMT ") "
					"Channel %u "
					"Rssi %d "
					"RTT %u "
					"RTT_SD %u "
					"Bcn Period %d "
					"Capability 0x%X "
					"IE Length %d",
					data->ap[i].ts,
					data->ap[i].ssid,
					QDF_MAC_ADDR_REF(data->ap[i].bssid.bytes),
					data->ap[i].channel,
					data->ap[i].rssi,
					data->ap[i].rtt,
					data->ap[i].rtt_sd,
					data->ap[i].beaconPeriod,
					data->ap[i].capability,
					data->ap[i].ieLength);
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->request_id) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_NUM_RESULTS_AVAILABLE,
		data->num_results) ||
	    nla_put_u8(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
		data->more_data)) {
		hdd_err("nla put fail");
		goto fail;
	}

	if (data->num_results) {
		struct nlattr *nla_aps;

		nla_aps = nla_nest_start(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
		if (!nla_aps)
			goto fail;

		for (i = 0; i < data->num_results; i++) {
			if (hdd_extscan_nl_fill_bss(skb, &data->ap[i], i))
				goto fail;
		}
		nla_nest_end(skb, nla_aps);
	}

	cfg80211_vendor_event(skb, flags);
	return;

fail:
	kfree_skb(skb);
}

/**
 * wlan_hdd_cfg80211_passpoint_match_found() - passpoint match found
 * @hddctx: HDD context
 * @data: matched network data
 *
 * This function reads the match network %data and fill in the skb with
 * NL attributes and send up the NL event
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_passpoint_match_found(void *ctx,
					struct wifi_passpoint_match *data)
{
	struct hdd_context *hdd_ctx  = ctx;
	struct sk_buff *skb     = NULL;
	uint32_t len, i, num_matches = 1, more_data = 0;
	struct nlattr *nla_aps, *nla_bss;
	int flags = cds_get_gfp_flags();

	hdd_enter();

	if (wlan_hdd_validate_context(hdd_ctx))
		return;
	if (!data) {
		hdd_err("data is null");
		return;
	}

	len = sizeof(*data) + data->ap.ieLength + data->anqp_len;
	if (len >= EXTSCAN_EVENT_BUF_SIZE) {
		hdd_err("Result exceeded NL size limitation, drop it");
		return;
	}

	skb = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
		  NULL,
		  EXTSCAN_EVENT_BUF_SIZE + NLMSG_HDRLEN,
		  QCA_NL80211_VENDOR_SUBCMD_EXTSCAN_PNO_PASSPOINT_NETWORK_FOUND_INDEX,
		  flags);

	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	hdd_debug("Req Id %u Id %u ANQP length %u num_matches %u",
		data->request_id, data->id, data->anqp_len, num_matches);
	for (i = 0; i < num_matches; i++) {
		hdd_debug("AP Info: Timestamp %llu Ssid: %s "
					"Bssid (" QDF_MAC_ADDR_FMT ") "
					"Channel %u "
					"Rssi %d "
					"RTT %u "
					"RTT_SD %u "
					"Bcn Period %d "
					"Capability 0x%X "
					"IE Length %d",
					data->ap.ts,
					data->ap.ssid,
					QDF_MAC_ADDR_REF(data->ap.bssid.bytes),
					data->ap.channel,
					data->ap.rssi,
					data->ap.rtt,
					data->ap.rtt_sd,
					data->ap.beaconPeriod,
					data->ap.capability,
					data->ap.ieLength);
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_REQUEST_ID,
		data->request_id) ||
	    nla_put_u32(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_PNO_RESULTS_PASSPOINT_NETWORK_FOUND_NUM_MATCHES,
		num_matches) ||
	    nla_put_u8(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_SCAN_RESULT_MORE_DATA,
		more_data)) {
		hdd_err("nla put fail");
		goto fail;
	}

	nla_aps = nla_nest_start(skb,
		QCA_WLAN_VENDOR_ATTR_EXTSCAN_PNO_RESULTS_PASSPOINT_MATCH_RESULT_LIST);
	if (!nla_aps)
		goto fail;

	for (i = 0; i < num_matches; i++) {
		struct nlattr *nla_ap;

		nla_ap = nla_nest_start(skb, i);
		if (!nla_ap)
			goto fail;

		if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_PNO_RESULTS_PASSPOINT_MATCH_ID,
			data->id) ||
		    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_EXTSCAN_PNO_RESULTS_PASSPOINT_MATCH_ANQP_LEN,
			data->anqp_len)) {
			goto fail;
		}

		if (data->anqp_len)
			if (nla_put(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_PNO_RESULTS_PASSPOINT_MATCH_ANQP,
				data->anqp_len, data->anqp))
				goto fail;

		nla_bss = nla_nest_start(skb,
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_LIST);
		if (!nla_bss)
			goto fail;

		if (hdd_extscan_nl_fill_bss(skb, &data->ap, 0))
			goto fail;

		nla_nest_end(skb, nla_bss);
		nla_nest_end(skb, nla_ap);
	}
	nla_nest_end(skb, nla_aps);

	cfg80211_vendor_event(skb, flags);
	return;

fail:
	kfree_skb(skb);
}

/**
 * wlan_hdd_cfg80211_extscan_generic_rsp() -
 *	Handle a generic ExtScan Response message
 * @ctx: HDD context registered with SME
 * @response: The ExtScan response from firmware
 *
 * This function will handle a generic ExtScan response message from
 * firmware and will communicate the result to the userspace thread
 * that is waiting for the response.
 *
 * Return: none
 */
static void
wlan_hdd_cfg80211_extscan_generic_rsp(struct hdd_context *hdd_ctx,
	 struct sir_extscan_generic_response *response)
{
	struct hdd_ext_scan_context *context;

	hdd_enter();

	if (wlan_hdd_validate_context(hdd_ctx) || !response) {
		hdd_err("HDD context is not valid or response(%pK) is null",
		       response);
		return;
	}

	hdd_debug("request %u status %u",
	       response->request_id, response->status);

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	if (context->request_id == response->request_id) {
		context->response_status = response->status ? -EINVAL : 0;
		complete(&context->response_event);
	}
	spin_unlock(&context->context_lock);
}

void wlan_hdd_cfg80211_extscan_callback(hdd_handle_t hdd_handle,
					const uint16_t event_id, void *msg)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);

	/* ENTER() intentionally not used in a frequently invoked API */

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	hdd_debug("Rcvd Event %d", event_id);

	switch (event_id) {
	case eSIR_EXTSCAN_CACHED_RESULTS_RSP:
		/* There is no need to send this response to upper layer
		 * Just log the message
		 */
		hdd_debug("Rcvd eSIR_EXTSCAN_CACHED_RESULTS_RSP");
		break;

	case eSIR_EXTSCAN_GET_CAPABILITIES_IND:
		wlan_hdd_cfg80211_extscan_get_capabilities_rsp(hdd_ctx, msg);
		break;

	case eSIR_EXTSCAN_HOTLIST_MATCH_IND:
		wlan_hdd_cfg80211_extscan_hotlist_match_ind(hdd_ctx, msg);
		break;

	case eSIR_EXTSCAN_SIGNIFICANT_WIFI_CHANGE_RESULTS_IND:
		wlan_hdd_cfg80211_extscan_signif_wifi_change_results_ind(hdd_ctx,
									 msg);
		break;

	case eSIR_EXTSCAN_CACHED_RESULTS_IND:
		wlan_hdd_cfg80211_extscan_cached_results_ind(hdd_ctx, msg);
		break;

	case eSIR_EXTSCAN_SCAN_RES_AVAILABLE_IND:
		wlan_hdd_cfg80211_extscan_scan_res_available_event(hdd_ctx,
								   msg);
		break;

	case eSIR_EXTSCAN_FULL_SCAN_RESULT_IND:
		wlan_hdd_cfg80211_extscan_full_scan_result_event(hdd_ctx, msg);
		break;

	case eSIR_EPNO_NETWORK_FOUND_IND:
		wlan_hdd_cfg80211_extscan_epno_match_found(hdd_ctx, msg);
		break;

	case eSIR_EXTSCAN_SCAN_PROGRESS_EVENT_IND:
		wlan_hdd_cfg80211_extscan_scan_progress_event(hdd_ctx, msg);
		break;

	case eSIR_PASSPOINT_NETWORK_FOUND_IND:
		wlan_hdd_cfg80211_passpoint_match_found(hdd_ctx, msg);
		break;

	case eSIR_EXTSCAN_START_RSP:
	case eSIR_EXTSCAN_STOP_RSP:
	case eSIR_EXTSCAN_SET_BSSID_HOTLIST_RSP:
	case eSIR_EXTSCAN_RESET_BSSID_HOTLIST_RSP:
	case eSIR_EXTSCAN_SET_SIGNIFICANT_WIFI_CHANGE_RSP:
	case eSIR_EXTSCAN_RESET_SIGNIFICANT_WIFI_CHANGE_RSP:
	case eSIR_EXTSCAN_SET_SSID_HOTLIST_RSP:
	case eSIR_EXTSCAN_RESET_SSID_HOTLIST_RSP:
		wlan_hdd_cfg80211_extscan_generic_rsp(hdd_ctx, msg);
		break;

	default:
		hdd_err("Unknown event type: %u", event_id);
		break;
	}
}

/*
 * define short names for the global vendor params
 * used by wlan_hdd_send_ext_scan_capability()
 */
#define PARAM_REQUEST_ID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID
#define PARAM_STATUS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_STATUS
#define MAX_EXTSCAN_CACHE_SIZE \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SCAN_CACHE_SIZE
#define MAX_SCAN_BUCKETS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SCAN_BUCKETS
#define MAX_AP_CACHE_PER_SCAN \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_AP_CACHE_PER_SCAN
#define MAX_RSSI_SAMPLE_SIZE \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_RSSI_SAMPLE_SIZE
#define MAX_SCAN_RPT_THRHOLD \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SCAN_REPORTING_THRESHOLD
#define MAX_HOTLIST_BSSIDS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_HOTLIST_BSSIDS
#define MAX_SIGNIFICANT_WIFI_CHANGE_APS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_SIGNIFICANT_WIFI_CHANGE_APS
#define MAX_BSSID_HISTORY_ENTRIES \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_BSSID_HISTORY_ENTRIES
#define MAX_HOTLIST_SSIDS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_HOTLIST_SSIDS
#define MAX_NUM_EPNO_NETS \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_NUM_EPNO_NETS
#define MAX_NUM_EPNO_NETS_BY_SSID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_NUM_EPNO_NETS_BY_SSID
#define MAX_NUM_WHITELISTED_SSID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_RESULTS_CAPABILITIES_MAX_NUM_WHITELISTED_SSID
#define MAX_NUM_BLACKLISTED_BSSID \
	QCA_WLAN_VENDOR_ATTR_EXTSCAN_MAX_NUM_BLACKLISTED_BSSID
/**
 * wlan_hdd_send_ext_scan_capability - send ext scan capability to user space
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: 0 for success, non-zero for failure
 */
static int wlan_hdd_send_ext_scan_capability(struct hdd_context *hdd_ctx)
{
	int ret;
	struct sk_buff *skb;
	struct ext_scan_capabilities_response *data;
	uint32_t nl_buf_len;

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	data = &(ext_scan_context.capability_response);

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len += (sizeof(data->requestId) + NLA_HDRLEN) +
	(sizeof(data->status) + NLA_HDRLEN) +
	(sizeof(data->max_scan_cache_size) + NLA_HDRLEN) +
	(sizeof(data->max_scan_buckets) + NLA_HDRLEN) +
	(sizeof(data->max_ap_cache_per_scan) + NLA_HDRLEN) +
	(sizeof(data->max_rssi_sample_size) + NLA_HDRLEN) +
	(sizeof(data->max_scan_reporting_threshold) + NLA_HDRLEN) +
	(sizeof(data->max_hotlist_bssids) + NLA_HDRLEN) +
	(sizeof(data->max_significant_wifi_change_aps) + NLA_HDRLEN) +
	(sizeof(data->max_bssid_history_entries) + NLA_HDRLEN) +
	(sizeof(data->max_hotlist_ssids) + NLA_HDRLEN) +
	(sizeof(data->max_number_epno_networks) + NLA_HDRLEN) +
	(sizeof(data->max_number_epno_networks_by_ssid) + NLA_HDRLEN) +
	(sizeof(data->max_number_of_white_listed_ssid) + NLA_HDRLEN) +
	(sizeof(data->max_number_of_black_listed_bssid) + NLA_HDRLEN);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);

	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}


	hdd_debug("Req Id %u", data->requestId);
	hdd_debug("Status %u", data->status);
	hdd_debug("Scan cache size %u",
	       data->max_scan_cache_size);
	hdd_debug("Scan buckets %u", data->max_scan_buckets);
	hdd_debug("Max AP per scan %u",
	       data->max_ap_cache_per_scan);
	hdd_debug("max_rssi_sample_size %u",
	       data->max_rssi_sample_size);
	hdd_debug("max_scan_reporting_threshold %u",
	       data->max_scan_reporting_threshold);
	hdd_debug("max_hotlist_bssids %u",
	       data->max_hotlist_bssids);
	hdd_debug("max_significant_wifi_change_aps %u",
	       data->max_significant_wifi_change_aps);
	hdd_debug("max_bssid_history_entries %u",
	       data->max_bssid_history_entries);
	hdd_debug("max_hotlist_ssids %u", data->max_hotlist_ssids);
	hdd_debug("max_number_epno_networks %u",
					data->max_number_epno_networks);
	hdd_debug("max_number_epno_networks_by_ssid %u",
					data->max_number_epno_networks_by_ssid);
	hdd_debug("max_number_of_white_listed_ssid %u",
					data->max_number_of_white_listed_ssid);
	hdd_debug("max_number_of_black_listed_bssid (%u)",
					data->max_number_of_black_listed_bssid);

	if (nla_put_u32(skb, PARAM_REQUEST_ID, data->requestId) ||
	    nla_put_u32(skb, PARAM_STATUS, data->status) ||
	    nla_put_u32(skb, MAX_EXTSCAN_CACHE_SIZE,
					data->max_scan_cache_size) ||
	    nla_put_u32(skb, MAX_SCAN_BUCKETS, data->max_scan_buckets) ||
	    nla_put_u32(skb, MAX_AP_CACHE_PER_SCAN,
			data->max_ap_cache_per_scan) ||
	    nla_put_u32(skb, MAX_RSSI_SAMPLE_SIZE,
			data->max_rssi_sample_size) ||
	    nla_put_u32(skb, MAX_SCAN_RPT_THRHOLD,
			data->max_scan_reporting_threshold) ||
	    nla_put_u32(skb, MAX_HOTLIST_BSSIDS, data->max_hotlist_bssids) ||
	    nla_put_u32(skb, MAX_SIGNIFICANT_WIFI_CHANGE_APS,
			data->max_significant_wifi_change_aps) ||
	    nla_put_u32(skb, MAX_BSSID_HISTORY_ENTRIES,
			data->max_bssid_history_entries) ||
	    nla_put_u32(skb, MAX_HOTLIST_SSIDS,	data->max_hotlist_ssids) ||
	    nla_put_u32(skb, MAX_NUM_EPNO_NETS,
			data->max_number_epno_networks) ||
	    nla_put_u32(skb, MAX_NUM_EPNO_NETS_BY_SSID,
			data->max_number_epno_networks_by_ssid) ||
	    nla_put_u32(skb, MAX_NUM_WHITELISTED_SSID,
			data->max_number_of_white_listed_ssid) ||
	    nla_put_u32(skb, MAX_NUM_BLACKLISTED_BSSID,
			data->max_number_of_black_listed_bssid)) {
		hdd_err("nla put fail");
		goto nla_put_failure;
	}

	cfg80211_vendor_cmd_reply(skb);
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -EINVAL;
}
/*
 * done with short names for the global vendor params
 * used by wlan_hdd_send_ext_scan_capability()
 */
#undef PARAM_REQUEST_ID
#undef PARAM_STATUS
#undef MAX_EXTSCAN_CACHE_SIZE
#undef MAX_SCAN_BUCKETS
#undef MAX_AP_CACHE_PER_SCAN
#undef MAX_RSSI_SAMPLE_SIZE
#undef MAX_SCAN_RPT_THRHOLD
#undef MAX_HOTLIST_BSSIDS
#undef MAX_SIGNIFICANT_WIFI_CHANGE_APS
#undef MAX_BSSID_HISTORY_ENTRIES
#undef MAX_HOTLIST_SSIDS
#undef MAX_NUM_EPNO_NETS
#undef MAX_NUM_EPNO_NETS_BY_SSID
#undef MAX_NUM_WHITELISTED_SSID
#undef MAX_NUM_BLACKLISTED_BSSID

/**
 * __wlan_hdd_cfg80211_extscan_get_capabilities() - get ext scan capabilities
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: none
 */
static int
__wlan_hdd_cfg80211_extscan_get_capabilities(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data, int data_len)
{
	int id, ret;
	unsigned long rc;
	struct hdd_ext_scan_context *context;
	struct extscan_capabilities_params params;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[EXTSCAN_PARAM_MAX + 1];
	QDF_STATUS status;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return -EINVAL;

	if (hdd_ctx->driver_status == DRIVER_MODULES_CLOSED) {
		hdd_err("Driver Modules are closed");
		return -EINVAL;
	}

	if (!ucfg_extscan_get_enable(hdd_ctx->psoc)) {
		hdd_err("extscan not supported");
		return -ENOTSUPP;
	}
	if (wlan_cfg80211_nla_parse(tb, EXTSCAN_PARAM_MAX, data, data_len,
				    wlan_hdd_extscan_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		return -EINVAL;
	}

	params.request_id = nla_get_u32(tb[id]);
	params.vdev_id = adapter->vdev_id;
	hdd_debug("Req Id %d Vdev Id %d", params.request_id, params.vdev_id);

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	context->request_id = params.request_id;
	INIT_COMPLETION(context->response_event);
	spin_unlock(&context->context_lock);

	status = sme_ext_scan_get_capabilities(hdd_ctx->mac_handle, &params);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_ext_scan_get_capabilities failed(err=%d)",
			status);
		return -EINVAL;
	}

	rc = wait_for_completion_timeout(&context->response_event,
		msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));
	if (!rc) {
		hdd_err("Target response timed out");
		return -ETIMEDOUT;
	}

	ret = wlan_hdd_send_ext_scan_capability(hdd_ctx);
	if (ret)
		hdd_err("Failed to send ext scan capability to user space");
	hdd_exit();
	return ret;
}

/**
 * wlan_hdd_cfg80211_extscan_get_capabilities() - get ext scan capabilities
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_extscan_get_capabilities(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_extscan_get_capabilities(wiphy, wdev,
							     data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __wlan_hdd_cfg80211_extscan_get_cached_results() - extscan get cached results
 * @wiphy: wiphy pointer
 * @wdev: pointer to struct wireless_dev
 * @data: pointer to incoming NL vendor data
 * @data_len: length of @data
 *
 * This function parses the incoming NL vendor command data attributes and
 * invokes the SME Api and blocks on a completion variable.
 * Each WMI event with cached scan results data chunk results in
 * function call wlan_hdd_cfg80211_extscan_cached_results_ind and each
 * data chunk is sent up the layer in cfg80211_vendor_cmd_alloc_reply_skb.
 *
 * If timeout happens before receiving all of the data, this function sets
 * a context variable @ignore_cached_results to %true, all of the next data
 * chunks are checked against this variable and dropped.
 *
 * Return: 0 on success; error number otherwise.
 */
static int
__wlan_hdd_cfg80211_extscan_get_cached_results(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	struct extscan_cached_result_params params;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[EXTSCAN_PARAM_MAX + 1];
	struct hdd_ext_scan_context *context;
	QDF_STATUS status;
	int id, retval;
	unsigned long rc;

	/* ENTER_DEV() intentionally not used in a frequently invoked API */

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (0 != retval)
		return -EINVAL;

	if (!ucfg_extscan_get_enable(hdd_ctx->psoc)) {
		hdd_err("extscan not supported");
		return -ENOTSUPP;
	}
	if (wlan_cfg80211_nla_parse(tb, EXTSCAN_PARAM_MAX, data, data_len,
				    wlan_hdd_extscan_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		return -EINVAL;
	}

	params.request_id = nla_get_u32(tb[id]);
	params.vdev_id = adapter->vdev_id;

	/* Parse and fetch flush parameter */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_GET_CACHED_SCAN_RESULTS_CONFIG_PARAM_FLUSH;
	if (!tb[id]) {
		hdd_err("attr flush failed");
		return -EINVAL;
	}
	params.flush = nla_get_u8(tb[id]);
	hdd_debug("Req Id: %u Vdev Id: %d Flush: %d",
		  params.request_id, params.vdev_id, params.flush);

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	context->request_id = params.request_id;
	context->ignore_cached_results = false;
	INIT_COMPLETION(context->response_event);
	spin_unlock(&context->context_lock);

	status = sme_get_cached_results(hdd_ctx->mac_handle, &params);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_get_cached_results failed(err=%d)", status);
		return -EINVAL;
	}

	rc = wait_for_completion_timeout(&context->response_event,
			msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));
	if (!rc) {
		hdd_err("Target response timed out");
		retval = -ETIMEDOUT;
		spin_lock(&context->context_lock);
		context->ignore_cached_results = true;
		spin_unlock(&context->context_lock);
	} else {
		spin_lock(&context->context_lock);
		retval = context->response_status;
		spin_unlock(&context->context_lock);
	}
	return retval;
}

/**
 * wlan_hdd_cfg80211_extscan_get_cached_results() - extscan get cached results
 * @wiphy: wiphy pointer
 * @wdev: pointer to struct wireless_dev
 * @data: pointer to incoming NL vendor data
 * @data_len: length of @data
 *
 * This function parses the incoming NL vendor command data attributes and
 * invokes the SME Api and blocks on a completion variable.
 * Each WMI event with cached scan results data chunk results in
 * function call wlan_hdd_cfg80211_extscan_cached_results_ind and each
 * data chunk is sent up the layer in cfg80211_vendor_cmd_alloc_reply_skb.
 *
 * If timeout happens before receiving all of the data, this function sets
 * a context variable @ignore_cached_results to %true, all of the next data
 * chunks are checked against this variable and dropped.
 *
 * Return: 0 on success; error number otherwise.
 */
int wlan_hdd_cfg80211_extscan_get_cached_results(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_extscan_get_cached_results(wiphy, wdev,
							       data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * hdd_parse_ap_rssi_threshold() - parse AP RSSI threshold parameters
 * @attr: netlink attribute containing the AP RSSI threshold parameters
 * @ap: destination buffer for the parsed parameters
 *
 * This function parses the BSSID, low RSSI and high RSSI values from
 * the @attr netlink attribute, storing the parsed values in @ap.
 *
 * Return: 0 if @attr is parsed and all required attributes are
 * present, otherwise a negative errno.
 */
static int hdd_parse_ap_rssi_threshold(struct nlattr *attr,
				       struct ap_threshold_params *ap)
{
	struct nlattr *tb[EXTSCAN_PARAM_MAX + 1];
	int id;

	if (wlan_cfg80211_nla_parse(tb, EXTSCAN_PARAM_MAX,
				    nla_data(attr), nla_len(attr),
				    wlan_hdd_extscan_config_policy)) {
		hdd_err("nla_parse failed");
		return -EINVAL;
	}

	/* Parse and fetch MAC address */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_BSSID;
	if (!tb[id]) {
		hdd_err("attr mac address failed");
		return -EINVAL;
	}
	nla_memcpy(ap->bssid.bytes, tb[id], QDF_MAC_ADDR_SIZE);
	hdd_debug("BSSID: " QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(ap->bssid.bytes));

	/* Parse and fetch low RSSI */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_LOW;
	if (!tb[id]) {
		hdd_err("attr low RSSI failed");
		return -EINVAL;
	}
	ap->low = nla_get_s32(tb[id]);
	hdd_debug("RSSI low %d", ap->low);

	/* Parse and fetch high RSSI */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM_RSSI_HIGH;
	if (!tb[id]) {
		hdd_err("attr high RSSI failed");
		return -EINVAL;
	}
	ap->high = nla_get_s32(tb[id]);
	hdd_debug("RSSI High %d", ap->high);

	return 0;
}

/**
 * __wlan_hdd_cfg80211_extscan_set_bssid_hotlist() - set bssid hot list
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: none
 */
static int
__wlan_hdd_cfg80211_extscan_set_bssid_hotlist(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len)
{
	struct extscan_bssid_hotlist_set_params *params;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[EXTSCAN_PARAM_MAX + 1];
	struct nlattr *apth;
	struct hdd_ext_scan_context *context;
	QDF_STATUS status;
	uint8_t i;
	int id, rem, retval;
	unsigned long rc;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (0 != retval)
		return -EINVAL;

	if (!ucfg_extscan_get_enable(hdd_ctx->psoc)) {
		hdd_err("extscan not supported");
		return -ENOTSUPP;
	}

	if (wlan_cfg80211_nla_parse(tb, EXTSCAN_PARAM_MAX,
				    data, data_len,
				    wlan_hdd_extscan_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	params = qdf_mem_malloc(sizeof(*params));
	if (!params)
		return -ENOMEM;

	/* assume the worst until proven otherwise */
	retval = -EINVAL;

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		goto fail;
	}

	params->request_id = nla_get_u32(tb[id]);
	hdd_debug("Req Id %d", params->request_id);

	/* Parse and fetch number of APs */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_NUM_AP;
	if (!tb[id]) {
		hdd_err("attr number of AP failed");
		goto fail;
	}

	params->num_ap = nla_get_u32(tb[id]);
	if (params->num_ap > WMI_WLAN_EXTSCAN_MAX_HOTLIST_APS) {
		hdd_err("Number of AP: %u exceeds max: %u",
			params->num_ap, WMI_WLAN_EXTSCAN_MAX_HOTLIST_APS);
		goto fail;
	}
	params->vdev_id = adapter->vdev_id;
	hdd_debug("Number of AP %d vdev Id %d",
		  params->num_ap, params->vdev_id);

	/* Parse and fetch lost ap sample size */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BSSID_HOTLIST_PARAMS_LOST_AP_SAMPLE_SIZE;
	if (!tb[id]) {
		hdd_err("attr lost ap sample size failed");
		goto fail;
	}

	params->lost_ap_sample_size = nla_get_u32(tb[id]);
	hdd_debug("Lost ap sample size %d",
		  params->lost_ap_sample_size);

	/* Parse the AP Threshold array */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM;
	if (!tb[id]) {
		hdd_err("attr ap threshold failed");
		goto fail;
	}

	i = 0;
	nla_for_each_nested(apth, tb[id], rem) {
		if (i == params->num_ap) {
			hdd_warn("Ignoring excess AP");
			break;
		}

		retval = hdd_parse_ap_rssi_threshold(apth, &params->ap[i]);
		if (retval)
			goto fail;

		i++;
	}

	if (i < params->num_ap) {
		hdd_warn("Number of AP %u less than expected %u",
			 i, params->num_ap);
		params->num_ap = i;
	}

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	INIT_COMPLETION(context->response_event);
	context->request_id = params->request_id;
	spin_unlock(&context->context_lock);

	status = sme_set_bss_hotlist(hdd_ctx->mac_handle, params);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_set_bss_hotlist failed(err=%d)", status);
		retval = qdf_status_to_os_return(status);
		goto fail;
	}

	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout
		(&context->response_event,
		 msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));

	if (!rc) {
		hdd_err("sme_set_bss_hotlist timed out");
		retval = -ETIMEDOUT;
	} else {
		spin_lock(&context->context_lock);
		if (context->request_id == params->request_id)
			retval = context->response_status;
		else
			retval = -EINVAL;
		spin_unlock(&context->context_lock);
	}
	hdd_exit();

fail:
	qdf_mem_free(params);
	return retval;
}

/**
 * wlan_hdd_cfg80211_extscan_set_bssid_hotlist() - set ext scan bssid hotlist
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_extscan_set_bssid_hotlist(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_extscan_set_bssid_hotlist(wiphy, wdev,
							      data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}


/**
 * __wlan_hdd_cfg80211_extscan_set_significant_change() - set significant change
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: none
 */
static int
__wlan_hdd_cfg80211_extscan_set_significant_change(struct wiphy *wiphy,
						   struct wireless_dev *wdev,
						   const void *data,
						   int data_len)
{
	struct extscan_set_sig_changereq_params *params;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[EXTSCAN_PARAM_MAX + 1];
	struct nlattr *apth;
	struct hdd_ext_scan_context *context;
	QDF_STATUS status;
	uint8_t i;
	int id, rem, retval;
	unsigned long rc;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (0 != retval)
		return -EINVAL;

	if (wlan_cfg80211_nla_parse(tb, EXTSCAN_PARAM_MAX,
				    data, data_len,
				    wlan_hdd_extscan_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	params = qdf_mem_malloc(sizeof(*params));
	if (!params)
		return -ENOMEM;

	/* assume the worst until proven otherwise */
	retval = -EINVAL;

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		goto fail;
	}

	params->request_id = nla_get_u32(tb[id]);
	hdd_debug("Req Id %d", params->request_id);

	/* Parse and fetch RSSI sample size */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_RSSI_SAMPLE_SIZE;
	if (!tb[id]) {
		hdd_err("attr RSSI sample size failed");
		goto fail;
	}
	params->rssi_sample_size = nla_get_u32(tb[id]);
	hdd_debug("RSSI sample size %u", params->rssi_sample_size);

	/* Parse and fetch lost AP sample size */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_LOST_AP_SAMPLE_SIZE;
	if (!tb[id]) {
		hdd_err("attr lost AP sample size failed");
		goto fail;
	}
	params->lostap_sample_size = nla_get_u32(tb[id]);
	hdd_debug("Lost AP sample size %u", params->lostap_sample_size);

	/* Parse and fetch AP min breaching */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_MIN_BREACHING;
	if (!tb[id]) {
		hdd_err("attr AP min breaching");
		goto fail;
	}
	params->min_breaching = nla_get_u32(tb[id]);
	hdd_debug("AP min breaching %u", params->min_breaching);

	/* Parse and fetch number of APs */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SIGNIFICANT_CHANGE_PARAMS_NUM_AP;
	if (!tb[id]) {
		hdd_err("attr number of AP failed");
		goto fail;
	}
	params->num_ap = nla_get_u32(tb[id]);
	if (params->num_ap > WLAN_EXTSCAN_MAX_SIGNIFICANT_CHANGE_APS) {
		hdd_err("Number of AP %u exceeds max %u",
			params->num_ap,
			WLAN_EXTSCAN_MAX_SIGNIFICANT_CHANGE_APS);
		goto fail;
	}

	params->vdev_id = adapter->vdev_id;
	hdd_debug("Number of AP %d Vdev Id %d",
		  params->num_ap, params->vdev_id);

	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_AP_THRESHOLD_PARAM;
	if (!tb[id]) {
		hdd_err("attr ap threshold failed");
		goto fail;
	}
	i = 0;
	nla_for_each_nested(apth, tb[id], rem) {

		if (i == params->num_ap) {
			hdd_warn("Ignoring excess AP");
			break;
		}

		retval = hdd_parse_ap_rssi_threshold(apth, &params->ap[i]);
		if (retval)
			goto fail;

		i++;
	}
	if (i < params->num_ap) {
		hdd_warn("Number of AP %u less than expected %u",
			 i, params->num_ap);
		params->num_ap = i;
	}

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	INIT_COMPLETION(context->response_event);
	context->request_id = params->request_id;
	spin_unlock(&context->context_lock);

	status = sme_set_significant_change(hdd_ctx->mac_handle, params);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_set_significant_change failed(err=%d)", status);
		retval = qdf_status_to_os_return(status);
		goto fail;
	}

	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout(&context->response_event,
				 msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));

	if (!rc) {
		hdd_err("sme_set_significant_change timed out");
		retval = -ETIMEDOUT;
	} else {
		spin_lock(&context->context_lock);
		if (context->request_id == params->request_id)
			retval = context->response_status;
		else
			retval = -EINVAL;
		spin_unlock(&context->context_lock);
	}
	hdd_exit();

fail:
	qdf_mem_free(params);
	return retval;
}

/**
 * wlan_hdd_cfg80211_extscan_set_significant_change() - set significant change
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_extscan_set_significant_change(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_extscan_set_significant_change(wiphy, wdev,
								   data,
								   data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * hdd_extscan_update_dwell_time_limits() - update dwell times
 * @req_msg: Pointer to request message
 * @bkt_idx: Index of current bucket being processed
 * @active_min: minimum active dwell time
 * @active_max: maximum active dwell time
 * @passive_min: minimum passive dwell time
 * @passive_max: maximum passive dwell time
 *
 * Return: none
 */
static void
hdd_extscan_update_dwell_time_limits(struct wifi_scan_cmd_req_params *req_msg,
				     uint32_t bkt_idx, uint32_t active_min,
				     uint32_t active_max, uint32_t passive_min,
				     uint32_t passive_max)
{
	/* update per-bucket dwell times */
	if (req_msg->buckets[bkt_idx].min_dwell_time_active >
			active_min) {
		req_msg->buckets[bkt_idx].min_dwell_time_active =
			active_min;
	}
	if (req_msg->buckets[bkt_idx].max_dwell_time_active <
			active_max) {
		req_msg->buckets[bkt_idx].max_dwell_time_active =
			active_max;
	}
	if (req_msg->buckets[bkt_idx].min_dwell_time_passive >
			passive_min) {
		req_msg->buckets[bkt_idx].min_dwell_time_passive =
			passive_min;
	}
	if (req_msg->buckets[bkt_idx].max_dwell_time_passive <
			passive_max) {
		req_msg->buckets[bkt_idx].max_dwell_time_passive =
			passive_max;
	}
	/* update dwell-time across all buckets */
	if (req_msg->min_dwell_time_active >
			req_msg->buckets[bkt_idx].min_dwell_time_active) {
		req_msg->min_dwell_time_active =
			req_msg->buckets[bkt_idx].min_dwell_time_active;
	}
	if (req_msg->max_dwell_time_active <
			req_msg->buckets[bkt_idx].max_dwell_time_active) {
		req_msg->max_dwell_time_active =
			req_msg->buckets[bkt_idx].max_dwell_time_active;
	}
	if (req_msg->min_dwell_time_passive >
			req_msg->buckets[bkt_idx].min_dwell_time_passive) {
		req_msg->min_dwell_time_passive =
			req_msg->buckets[bkt_idx].min_dwell_time_passive;
	}
	if (req_msg->max_dwell_time_passive >
			req_msg->buckets[bkt_idx].max_dwell_time_passive) {
		req_msg->max_dwell_time_passive =
			req_msg->buckets[bkt_idx].max_dwell_time_passive;
	}
}

/**
 * hdd_extscan_channel_max_reached() - channel max reached
 * @req: extscan request structure
 * @total_channels: total number of channels
 *
 * Return: true if total channels reached max, false otherwise
 */
static bool
hdd_extscan_channel_max_reached(struct wifi_scan_cmd_req_params *req,
				uint8_t total_channels)
{
	if (total_channels == WMI_WLAN_EXTSCAN_MAX_CHANNELS) {
		hdd_warn("max #of channels %d reached, take only first %d bucket(s)",
			 total_channels, req->num_buckets);
		return true;
	}
	return false;
}

/**
 * hdd_extscan_start_fill_bucket_channel_spec() - fill bucket channel spec
 * @hdd_ctx: HDD global context
 * @req_msg: Pointer to request structure
 * @bucket_attr: pointer to bucket attribute
 *
 * Return: 0 on success; error number otherwise
 */
static int hdd_extscan_start_fill_bucket_channel_spec(
			struct hdd_context *hdd_ctx,
			struct wifi_scan_cmd_req_params *req_msg,
			struct nlattr *bucket_attr)
{
	mac_handle_t mac_handle;
	struct nlattr *bucket_tb[EXTSCAN_PARAM_MAX + 1];
	struct nlattr *channel_tb[EXTSCAN_PARAM_MAX + 1];
	struct nlattr *buckets;
	struct nlattr *channels;
	int id, rem1, rem2;
	QDF_STATUS status;
	uint8_t bkt_index, j, num_channels, total_channels = 0;
	uint32_t expected_buckets;
	uint32_t expected_channels;
	uint32_t chan_list[CFG_VALID_CHANNEL_LIST_LEN] = {0};
	uint32_t extscan_active_min_chn_time;
	uint32_t min_dwell_time_active_bucket;
	uint32_t max_dwell_time_active_bucket;
	uint32_t min_dwell_time_passive_bucket;
	uint32_t max_dwell_time_passive_bucket;
	struct wifi_scan_bucket_params *bucket;
	struct wifi_scan_channelspec_params *channel;
	struct nlattr *channel_attr;

	ucfg_extscan_get_active_min_time(hdd_ctx->psoc,
					&extscan_active_min_chn_time);
	ucfg_extscan_get_active_max_time(hdd_ctx->psoc,
					 &max_dwell_time_active_bucket);
	ucfg_extscan_get_passive_max_time(hdd_ctx->psoc,
					 &max_dwell_time_passive_bucket);

	min_dwell_time_active_bucket = max_dwell_time_active_bucket;
	min_dwell_time_passive_bucket = max_dwell_time_passive_bucket;

	req_msg->min_dwell_time_active =
		req_msg->max_dwell_time_active = max_dwell_time_active_bucket;

	req_msg->min_dwell_time_passive =
		req_msg->max_dwell_time_passive = max_dwell_time_passive_bucket;

	expected_buckets = req_msg->num_buckets;
	req_msg->num_buckets = 0;
	bkt_index = 0;

	mac_handle = hdd_ctx->mac_handle;
	nla_for_each_nested(buckets, bucket_attr, rem1) {

		if (bkt_index >= expected_buckets) {
			hdd_warn("ignoring excess buckets");
			break;
		}

		if (wlan_cfg80211_nla_parse(bucket_tb, EXTSCAN_PARAM_MAX,
					    nla_data(buckets), nla_len(buckets),
					    wlan_hdd_extscan_config_policy)) {
			hdd_err("nla_parse failed");
			return -EINVAL;
		}

		bucket = &req_msg->buckets[bkt_index];

		/* Parse and fetch bucket spec */
		id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_INDEX;
		if (!bucket_tb[id]) {
			hdd_err("attr bucket index failed");
			return -EINVAL;
		}
		bucket->bucket = nla_get_u8(bucket_tb[id]);

		/* Parse and fetch wifi band */
		id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_BAND;
		if (!bucket_tb[id]) {
			hdd_err("attr wifi band failed");
			return -EINVAL;
		}
		bucket->band = nla_get_u8(bucket_tb[id]);

		/* Parse and fetch period */
		id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_PERIOD;
		if (!bucket_tb[id]) {
			hdd_err("attr period failed");
			return -EINVAL;
		}
		bucket->period = nla_get_u32(bucket_tb[id]);

		/* Parse and fetch report events */
		id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_REPORT_EVENTS;
		if (!bucket_tb[id]) {
			hdd_err("attr report events failed");
			return -EINVAL;
		}
		bucket->report_events = nla_get_u8(bucket_tb[id]);

		/* Parse and fetch max period */
		id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_MAX_PERIOD;
		if (!bucket_tb[id]) {
			hdd_err("attr max period failed");
			return -EINVAL;
		}
		bucket->max_period = nla_get_u32(bucket_tb[id]);

		/* Parse and fetch base */
		id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_BASE;
		if (!bucket_tb[id]) {
			hdd_err("attr base failed");
			return -EINVAL;
		}
		bucket->exponent = nla_get_u32(bucket_tb[id]);

		/* Parse and fetch step count */
		id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_STEP_COUNT;
		if (!bucket_tb[id]) {
			hdd_err("attr step count failed");
			return -EINVAL;
		}
		bucket->step_count = nla_get_u32(bucket_tb[id]);

		hdd_debug("Bucket spec Index: %d Wifi band: %d period: %d report events: %d max period: %u base: %u Step count: %u",
			  bucket->bucket,
			  bucket->band,
			  bucket->period,
			  bucket->report_events,
			  bucket->max_period,
			  bucket->exponent,
			  bucket->step_count);

		/* start with known good values for bucket dwell times */
		bucket->min_dwell_time_active = max_dwell_time_active_bucket;
		bucket->max_dwell_time_active = max_dwell_time_active_bucket;
		bucket->min_dwell_time_passive = max_dwell_time_passive_bucket;
		bucket->max_dwell_time_passive = max_dwell_time_passive_bucket;

		/* Framework shall pass the channel list if the input
		 * WiFi band is WMI_WIFI_BAND_UNSPECIFIED.  If the
		 * input WiFi band is specified (any value other than
		 * WMI_WIFI_BAND_UNSPECIFIED) then driver populates
		 * the channel list.
		 */
		if (bucket->band != WMI_WIFI_BAND_UNSPECIFIED) {
			if (hdd_extscan_channel_max_reached(req_msg,
							    total_channels))
				return 0;

			num_channels = 0;
			hdd_debug("WiFi band is specified, driver to fill channel list");
			status = sme_get_valid_channels_by_band(mac_handle,
								bucket->band,
								chan_list,
								&num_channels);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				hdd_err("sme_get_valid_channels_by_band failed (err=%d)",
					status);
				return -EINVAL;
			}
			hdd_debug("before trimming, num_channels: %d",
				  num_channels);

			bucket->num_channels =
				QDF_MIN(num_channels,
					(WMI_WLAN_EXTSCAN_MAX_CHANNELS -
						total_channels));
			hdd_debug("Adj Num channels/bucket: %d total_channels: %d",
				  bucket->num_channels, total_channels);
			total_channels += bucket->num_channels;

			for (j = 0; j < bucket->num_channels; j++) {
				channel = &bucket->channels[j];

				channel->channel = chan_list[j];
				channel->channel_class = 0;
				if ((wlan_reg_get_channel_state_for_freq(
				     hdd_ctx->pdev, chan_list[j])) !=
				    CHANNEL_STATE_ENABLE) {
					channel->passive = 1;
					channel->dwell_time_ms =
						max_dwell_time_passive_bucket;
					/* reconfigure per-bucket dwell time */
					if (min_dwell_time_passive_bucket >
							channel->dwell_time_ms) {
						min_dwell_time_passive_bucket =
							channel->dwell_time_ms;
					}
					if (max_dwell_time_passive_bucket <
							channel->dwell_time_ms) {
						max_dwell_time_passive_bucket =
							channel->dwell_time_ms;
					}

				} else {
					channel->passive = 0;
					channel->dwell_time_ms =
						max_dwell_time_active_bucket;
					/* reconfigure per-bucket dwell times */
					if (min_dwell_time_active_bucket >
							channel->dwell_time_ms) {
						min_dwell_time_active_bucket =
							channel->dwell_time_ms;
					}
					if (max_dwell_time_active_bucket <
							channel->dwell_time_ms) {
						max_dwell_time_active_bucket =
							channel->dwell_time_ms;
					}

				}

				hdd_debug("Channel: %u Passive: %u Dwell time: %u ms Class: %u",
					  channel->channel,
					  channel->passive,
					  channel->dwell_time_ms,
					  channel->channel_class);
			}

			hdd_extscan_update_dwell_time_limits(
					req_msg, bkt_index,
					min_dwell_time_active_bucket,
					max_dwell_time_active_bucket,
					min_dwell_time_passive_bucket,
					max_dwell_time_passive_bucket);

			hdd_debug("bkt_index:%d actv_min:%d actv_max:%d pass_min:%d pass_max:%d",
					bkt_index,
					bucket->min_dwell_time_active,
					bucket->max_dwell_time_active,
					bucket->min_dwell_time_passive,
					bucket->max_dwell_time_passive);

			bkt_index++;
			req_msg->num_buckets++;
			continue;
		}

		/* Parse and fetch number of channels */
		id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC_NUM_CHANNEL_SPECS;
		if (!bucket_tb[id]) {
			hdd_err("attr num channels failed");
			return -EINVAL;
		}
		bucket->num_channels = nla_get_u32(bucket_tb[id]);
		hdd_debug("before trimming: num channels %d",
			  bucket->num_channels);

		bucket->num_channels =
			QDF_MIN(bucket->num_channels,
				(WMI_WLAN_EXTSCAN_MAX_CHANNELS -
							total_channels));
		hdd_debug("Num channels/bucket: %d total_channels: %d",
			  bucket->num_channels, total_channels);

		if (hdd_extscan_channel_max_reached(req_msg, total_channels))
			return 0;

		id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC;
		channel_attr = bucket_tb[id];
		if (!channel_attr) {
			hdd_err("attr channel spec failed");
			return -EINVAL;
		}

		expected_channels = bucket->num_channels;
		bucket->num_channels = 0;

		nla_for_each_nested(channels, channel_attr, rem2) {
			if ((bucket->num_channels >= expected_channels) ||
			    hdd_extscan_channel_max_reached(req_msg,
							    total_channels))
				break;

			if (wlan_cfg80211_nla_parse(channel_tb,
						    EXTSCAN_PARAM_MAX,
						    nla_data(channels),
						    nla_len(channels),
						    wlan_hdd_extscan_config_policy)) {
				hdd_err("nla_parse failed");
				return -EINVAL;
			}

			channel = &bucket->channels[bucket->num_channels];
			/* Parse and fetch channel */
			if (!channel_tb[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CHANNEL]) {
				hdd_err("attr channel failed");
				return -EINVAL;
			}
			channel->channel =
				nla_get_u32(channel_tb[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_CHANNEL]);
			hdd_debug("channel %u",
				channel->channel);

			/* Parse and fetch dwell time */
			if (!channel_tb[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_DWELL_TIME]) {
				hdd_err("attr dwelltime failed");
				return -EINVAL;
			}
			channel->dwell_time_ms =
				nla_get_u32(channel_tb[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_DWELL_TIME]);

			/* Override dwell time if required */
			if (channel->dwell_time_ms <
						extscan_active_min_chn_time ||
			    channel->dwell_time_ms >
						max_dwell_time_active_bucket) {
				hdd_debug("WiFi band is unspecified, dwellTime:%d",
						channel->dwell_time_ms);

				if ((wlan_reg_get_channel_state_for_freq(
				     hdd_ctx->pdev, channel->channel)) !=
				    CHANNEL_STATE_ENABLE) {
					channel->dwell_time_ms =
						max_dwell_time_passive_bucket;
				} else {
					channel->dwell_time_ms =
						max_dwell_time_active_bucket;
				}
			}

			hdd_debug("New Dwell time %u ms",
				channel->dwell_time_ms);

			if ((wlan_reg_get_channel_state_for_freq(
			     hdd_ctx->pdev, channel->channel)) !=
			    CHANNEL_STATE_ENABLE) {
				if (min_dwell_time_passive_bucket >
						channel->dwell_time_ms) {
					min_dwell_time_passive_bucket =
						channel->dwell_time_ms;
				}
				if (max_dwell_time_passive_bucket <
						channel->dwell_time_ms) {
					max_dwell_time_passive_bucket =
						channel->dwell_time_ms;
				}
			} else {
				if (min_dwell_time_active_bucket >
						channel->dwell_time_ms) {
					min_dwell_time_active_bucket =
						channel->dwell_time_ms;
				}
				if (max_dwell_time_active_bucket <
						channel->dwell_time_ms) {
					max_dwell_time_active_bucket =
						channel->dwell_time_ms;
				}
			}

			/* Parse and fetch channel spec passive */
			if (!channel_tb[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_PASSIVE]) {
				hdd_err("attr channel spec passive failed");
				return -EINVAL;
			}
			channel->passive =
				nla_get_u8(channel_tb[
				QCA_WLAN_VENDOR_ATTR_EXTSCAN_CHANNEL_SPEC_PASSIVE]);
			hdd_debug("Chnl spec passive %u",
				channel->passive);
			/* Override scan type if required */
			if ((wlan_reg_get_channel_state_for_freq(
							hdd_ctx->pdev,
							channel->channel))
			    != CHANNEL_STATE_ENABLE) {
				channel->passive = true;
			} else {
				channel->passive = false;
			}
			total_channels++;
			bucket->num_channels++;
		}

		if (bucket->num_channels != expected_channels)
			hdd_warn("channels: Expected %u got %u",
				 expected_channels,
				 bucket->num_channels);

		hdd_extscan_update_dwell_time_limits(
					req_msg, bkt_index,
					min_dwell_time_active_bucket,
					max_dwell_time_active_bucket,
					min_dwell_time_passive_bucket,
					max_dwell_time_passive_bucket);

		hdd_debug("bktIndex:%d actv_min:%d actv_max:%d pass_min:%d pass_max:%d",
				bkt_index,
				bucket->min_dwell_time_active,
				bucket->max_dwell_time_active,
				bucket->min_dwell_time_passive,
				bucket->max_dwell_time_passive);

		bkt_index++;
		req_msg->num_buckets++;
	}

	hdd_debug("Global: actv_min:%d actv_max:%d pass_min:%d pass_max:%d",
				req_msg->min_dwell_time_active,
				req_msg->max_dwell_time_active,
				req_msg->min_dwell_time_passive,
				req_msg->max_dwell_time_passive);
	return 0;
}

/*
 * hdd_extscan_map_usr_drv_config_flags() - map userspace to driver config flags
 * @config_flags - [input] configuration flags.
 *
 * This function maps user space received configuration flags to
 * driver representation.
 *
 * Return: configuration flags
 */
static uint32_t hdd_extscan_map_usr_drv_config_flags(uint32_t config_flags)
{
	uint32_t configuration_flags = 0;

	if (config_flags & EXTSCAN_LP_EXTENDED_BATCHING)
		configuration_flags |= EXTSCAN_LP_EXTENDED_BATCHING;

	return configuration_flags;
}

/**
 * __wlan_hdd_cfg80211_extscan_start() - ext scan start
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * Return: 0 on success; error number otherwise
 */
static int
__wlan_hdd_cfg80211_extscan_start(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data,
				  int data_len)
{
	struct wifi_scan_cmd_req_params *params;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[EXTSCAN_PARAM_MAX + 1];
	struct hdd_ext_scan_context *context;
	uint32_t num_buckets;
	QDF_STATUS status;
	int retval, id;
	unsigned long rc;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (QDF_NDI_MODE == adapter->device_mode) {
		hdd_err("Command not allowed for NDI interface");
		return -EPERM;
	}

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (0 != retval)
		return -EINVAL;

	if (!ucfg_extscan_get_enable(hdd_ctx->psoc)) {
		hdd_err("extscan not supported");
		return -ENOTSUPP;
	}
	if (wlan_cfg80211_nla_parse(tb, EXTSCAN_PARAM_MAX, data, data_len,
				    wlan_hdd_extscan_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	params = qdf_mem_malloc(sizeof(*params));
	if (!params)
		return -ENOMEM;

	/* assume the worst until proven otherwise */
	retval = -EINVAL;

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		goto fail;
	}

	params->request_id = nla_get_u32(tb[id]);
	params->vdev_id = adapter->vdev_id;

	/* Parse and fetch base period */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_BASE_PERIOD;
	if (!tb[id]) {
		hdd_err("attr base period failed");
		goto fail;
	}
	params->base_period = nla_get_u32(tb[id]);

	/* Parse and fetch max AP per scan */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_MAX_AP_PER_SCAN;
	if (!tb[id]) {
		hdd_err("attr max_ap_per_scan failed");
		goto fail;
	}
	params->max_ap_per_scan = nla_get_u32(tb[id]);

	/* Parse and fetch report threshold percent */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD_PERCENT;
	if (!tb[id]) {
		hdd_err("attr report_threshold percent failed");
		goto fail;
	}
	params->report_threshold_percent = nla_get_u8(tb[id]);

	/* Parse and fetch report threshold num scans */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_REPORT_THRESHOLD_NUM_SCANS;
	if (!tb[id]) {
		hdd_err("attr report_threshold num scans failed");
		goto fail;
	}
	params->report_threshold_num_scans = nla_get_u8(tb[id]);
	hdd_debug("Req Id: %d Vdev Id: %d Base Period: %d Max AP per Scan: %d Report Threshold percent: %d Report Threshold num scans: %d",
		  params->request_id, params->vdev_id,
		  params->base_period, params->max_ap_per_scan,
		  params->report_threshold_percent,
		  params->report_threshold_num_scans);

	/* Parse and fetch number of buckets */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SCAN_CMD_PARAMS_NUM_BUCKETS;
	if (!tb[id]) {
		hdd_err("attr number of buckets failed");
		goto fail;
	}
	num_buckets = nla_get_u8(tb[id]);

	if (num_buckets > WMI_WLAN_EXTSCAN_MAX_BUCKETS) {
		hdd_warn("Exceeded MAX number of buckets: %d",
			 WMI_WLAN_EXTSCAN_MAX_BUCKETS);
		num_buckets = WMI_WLAN_EXTSCAN_MAX_BUCKETS;
	}
	hdd_debug("Input: Number of Buckets %d", num_buckets);
	params->num_buckets = num_buckets;

	/* This is optional attribute, if not present set it to 0 */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_CONFIGURATION_FLAGS;
	if (!tb[id])
		params->configuration_flags = 0;
	else
		params->configuration_flags =
			hdd_extscan_map_usr_drv_config_flags(
				nla_get_u32(tb[id]));

	params->extscan_adaptive_dwell_mode =
		ucfg_scan_get_extscan_adaptive_dwell_mode(hdd_ctx->psoc);

	hdd_debug("Configuration flags: %u",
		  params->configuration_flags);

	/* Parse and fetch number the array of buckets */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_BUCKET_SPEC;
	if (!tb[id]) {
		hdd_err("attr bucket spec failed");
		goto fail;
	}
	retval = hdd_extscan_start_fill_bucket_channel_spec(hdd_ctx, params,
							    tb[id]);
	if (retval)
		goto fail;

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	INIT_COMPLETION(context->response_event);
	context->request_id = params->request_id;
	context->buckets_scanned = 0;
	spin_unlock(&context->context_lock);

	status = sme_ext_scan_start(hdd_ctx->mac_handle, params);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_ext_scan_start failed(err=%d)", status);
		retval = qdf_status_to_os_return(status);
		goto fail;
	}

	hdd_ctx->ext_scan_start_since_boot = qdf_get_monotonic_boottime();
	hdd_debug("Timestamp since boot: %llu",
		  hdd_ctx->ext_scan_start_since_boot);

	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout(&context->response_event,
				msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));

	if (!rc) {
		hdd_err("sme_ext_scan_start timed out");
		retval = -ETIMEDOUT;
	} else {
		spin_lock(&context->context_lock);
		if (context->request_id == params->request_id)
			retval = context->response_status;
		else
			retval = -EINVAL;
		spin_unlock(&context->context_lock);
	}
	hdd_exit();

fail:
	qdf_mem_free(params);
	return retval;
}

/**
 * wlan_hdd_cfg80211_extscan_start() - start extscan
 * @wiphy: Pointer to wireless phy.
 * @wdev: Pointer to wireless device.
 * @data: Pointer to input data.
 * @data_len: Length of @data.
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_extscan_start(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_extscan_start(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}


/**
 * __wlan_hdd_cfg80211_extscan_stop() - ext scan stop
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: none
 */
static int
__wlan_hdd_cfg80211_extscan_stop(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len)
{
	struct extscan_stop_req_params params;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[EXTSCAN_PARAM_MAX + 1];
	struct hdd_ext_scan_context *context;
	QDF_STATUS status;
	int id, retval;
	unsigned long rc;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (0 != retval)
		return -EINVAL;

	if (!ucfg_extscan_get_enable(hdd_ctx->psoc)) {
		hdd_err("extscan not supported");
		return -ENOTSUPP;
	}

	if (wlan_cfg80211_nla_parse(tb, EXTSCAN_PARAM_MAX, data, data_len,
				    wlan_hdd_extscan_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		return -EINVAL;
	}
	params.request_id = nla_get_u32(tb[id]);
	params.vdev_id = adapter->vdev_id;
	hdd_debug("Req Id %d Vdev Id %d",
		  params.request_id, params.vdev_id);

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	INIT_COMPLETION(context->response_event);
	context->request_id = params.request_id;
	spin_unlock(&context->context_lock);

	status = sme_ext_scan_stop(hdd_ctx->mac_handle, &params);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_ext_scan_stop failed(err=%d)", status);
		return qdf_status_to_os_return(status);
	}

	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout(&context->response_event,
				msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));

	if (!rc) {
		hdd_err("sme_ext_scan_stop timed out");
		retval = -ETIMEDOUT;
	} else {
		spin_lock(&context->context_lock);
		if (context->request_id == params.request_id)
			retval = context->response_status;
		else
			retval = -EINVAL;
		spin_unlock(&context->context_lock);
	}
	hdd_exit();
	return retval;
}

/**
 * wlan_hdd_cfg80211_extscan_stop() - stop extscan
 * @wiphy: Pointer to wireless phy.
 * @wdev: Pointer to wireless device.
 * @data: Pointer to input data.
 * @data_len: Length of @data.
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_extscan_stop(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_extscan_stop(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __wlan_hdd_cfg80211_extscan_reset_bssid_hotlist() - reset bssid hotlist
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: none
 */
static int
__wlan_hdd_cfg80211_extscan_reset_bssid_hotlist(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct extscan_bssid_hotlist_reset_params params;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[EXTSCAN_PARAM_MAX + 1];
	struct hdd_ext_scan_context *context;
	QDF_STATUS status;
	int id, retval;
	unsigned long rc;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (0 != retval)
		return -EINVAL;

	if (!ucfg_extscan_get_enable(hdd_ctx->psoc)) {
		hdd_err("extscan not supported");
		return -ENOTSUPP;
	}

	if (wlan_cfg80211_nla_parse(tb, EXTSCAN_PARAM_MAX,
				    data, data_len,
				    wlan_hdd_extscan_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		return -EINVAL;
	}

	params.request_id = nla_get_u32(tb[id]);
	params.vdev_id = adapter->vdev_id;
	hdd_debug("Req Id %d vdev Id %d", params.request_id, params.vdev_id);

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	INIT_COMPLETION(context->response_event);
	context->request_id = params.request_id;
	spin_unlock(&context->context_lock);

	status = sme_reset_bss_hotlist(hdd_ctx->mac_handle, &params);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_reset_bss_hotlist failed(err=%d)", status);
		return qdf_status_to_os_return(status);
	}

	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout
		(&context->response_event,
		 msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));
	if (!rc) {
		hdd_err("sme_reset_bss_hotlist timed out");
		retval = -ETIMEDOUT;
	} else {
		spin_lock(&context->context_lock);
		if (context->request_id == params.request_id)
			retval = context->response_status;
		else
			retval = -EINVAL;
		spin_unlock(&context->context_lock);
	}
	hdd_exit();
	return retval;
}

/**
 * wlan_hdd_cfg80211_extscan_reset_bssid_hotlist() - reset bssid hot list
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_extscan_reset_bssid_hotlist(struct wiphy *wiphy,
						  struct wireless_dev *wdev,
						  const void *data,
						  int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_extscan_reset_bssid_hotlist(wiphy, wdev,
								data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __wlan_hdd_cfg80211_extscan_reset_significant_change() -
 *		reset significant change
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: none
 */
static int
__wlan_hdd_cfg80211_extscan_reset_significant_change(struct wiphy *wiphy,
						     struct wireless_dev *wdev,
						     const void *data,
						     int data_len)
{
	struct extscan_capabilities_reset_params params;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[EXTSCAN_PARAM_MAX + 1];
	struct hdd_ext_scan_context *context;
	QDF_STATUS status;
	int id, retval;
	unsigned long rc;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	retval = wlan_hdd_validate_context(hdd_ctx);
	if (0 != retval)
		return -EINVAL;

	if (!ucfg_extscan_get_enable(hdd_ctx->psoc)) {
		hdd_err("extscan not supported");
		return -ENOTSUPP;
	}

	if (wlan_cfg80211_nla_parse(tb, EXTSCAN_PARAM_MAX, data, data_len,
				    wlan_hdd_extscan_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		return -EINVAL;
	}

	params.request_id = nla_get_u32(tb[id]);
	params.vdev_id = adapter->vdev_id;
	hdd_debug("Req Id %d Vdev Id %d", params.request_id, params.vdev_id);

	context = &ext_scan_context;
	spin_lock(&context->context_lock);
	INIT_COMPLETION(context->response_event);
	context->request_id = params.request_id;
	spin_unlock(&context->context_lock);

	status = sme_reset_significant_change(hdd_ctx->mac_handle, &params);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_reset_significant_change failed(err=%d)",
			status);
		return -EINVAL;
	}

	/* request was sent -- wait for the response */
	rc = wait_for_completion_timeout(&context->response_event,
				msecs_to_jiffies(WLAN_WAIT_TIME_EXTSCAN));

	if (!rc) {
		hdd_err("sme_ResetSignificantChange timed out");
		retval = -ETIMEDOUT;
	} else {
		spin_lock(&context->context_lock);
		if (context->request_id == params.request_id)
			retval = context->response_status;
		else
			retval = -EINVAL;
		spin_unlock(&context->context_lock);
	}
	hdd_exit();
	return retval;
}

/**
 * wlan_hdd_cfg80211_extscan_reset_significant_change() - reset significant
 *							change
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
int wlan_hdd_cfg80211_extscan_reset_significant_change(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_extscan_reset_significant_change(wiphy,
								     wdev,
								     data,
								     data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}


/**
 * hdd_extscan_epno_fill_network() - epno fill single network
 * @network: aggregate network attribute
 * @nw: epno network record to be filled
 *
 * This function takes a single network block NL vendor attribute from
 * @network and decodes it into the internal record @nw.
 *
 * Return: 0 on success, error number otherwise
 */
static int
hdd_extscan_epno_fill_network(struct nlattr *network,
			      struct wifi_epno_network_params *nw)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PNO_MAX + 1];
	int id, ssid_len;
	uint8_t *ssid;

	if (!network) {
		hdd_err("attr network attr failed");
		return -EINVAL;
	}

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PNO_MAX,
				    nla_data(network),
				    nla_len(network),
				    wlan_hdd_pno_config_policy)) {
		hdd_err("nla_parse failed");
		return -EINVAL;
	}

	/* Parse and fetch ssid */
	id = QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_SSID;
	if (!tb[id]) {
		hdd_err("attr network ssid failed");
		return -EINVAL;
	}
	ssid_len = nla_len(tb[id]);

	/* nla_parse will detect overflow but not underflow */
	if (0 == ssid_len) {
		hdd_err("zero ssid length");
		return -EINVAL;
	}

	/* Decrement by 1, don't count null character */
	ssid_len--;

	nw->ssid.length = ssid_len;
	hdd_debug("network ssid length %d", ssid_len);
	ssid = nla_data(tb[id]);
	qdf_mem_copy(nw->ssid.ssid, ssid, ssid_len);
	hdd_debug("Ssid (%.*s)", nw->ssid.length, nw->ssid.ssid);

	/* Parse and fetch epno flags */
	id = QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_FLAGS;
	if (!tb[id]) {
		hdd_err("attr epno flags failed");
		return -EINVAL;
	}
	nw->flags = nla_get_u8(tb[id]);
	hdd_debug("flags %u", nw->flags);

	/* Parse and fetch auth bit */
	id = QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORK_AUTH_BIT;
	if (!tb[id]) {
		hdd_err("attr auth bit failed");
		return -EINVAL;
	}
	nw->auth_bit_field = nla_get_u8(tb[id]);
	hdd_debug("auth bit %u", nw->auth_bit_field);

	return 0;
}

/**
 * hdd_extscan_epno_fill_network_list() - epno fill network list
 * @req_msg: request message
 * @networks: aggregate network list attribute
 *
 * This function reads the network block NL vendor attributes from
 * @networks and fills in the epno request message @req_msg.
 *
 * Return: 0 on success, error number otherwise
 */
static int
hdd_extscan_epno_fill_network_list(struct wifi_enhanced_pno_params *req_msg,
				   struct nlattr *networks)
{
	struct nlattr *network;
	int rem;
	uint32_t index;
	uint32_t expected_networks;
	struct wifi_epno_network_params *nw;

	if (!networks) {
		hdd_err("attr networks list failed");
		return -EINVAL;
	}

	expected_networks = req_msg->num_networks;
	index = 0;

	nla_for_each_nested(network, networks, rem) {
		if (index == expected_networks) {
			hdd_warn("ignoring excess networks");
			break;
		}

		nw = &req_msg->networks[index++];
		if (hdd_extscan_epno_fill_network(network, nw))
			return -EINVAL;
	}
	req_msg->num_networks = index;
	return 0;
}

/**
 * __wlan_hdd_cfg80211_set_epno_list() - epno set network list
 * @wiphy: wiphy
 * @wdev: pointer to wireless dev
 * @data: data pointer
 * @data_len: data length
 *
 * This function reads the NL vendor attributes from @data and
 * fills in the epno request message.
 *
 * Return: 0 on success, error number otherwise
 */
static int __wlan_hdd_cfg80211_set_epno_list(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	struct wifi_enhanced_pno_params *req_msg;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PNO_MAX + 1];
	struct nlattr *networks;
	QDF_STATUS status;
	uint32_t num_networks, len;
	int id, ret_val;

	hdd_enter_dev(dev);

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (!ucfg_extscan_get_enable(hdd_ctx->psoc)) {
		hdd_err("extscan not supported");
		return -ENOTSUPP;
	}

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PNO_MAX, data,
				    data_len, wlan_hdd_pno_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	/* Parse and fetch number of networks */
	id = QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_NUM_NETWORKS;
	if (!tb[id]) {
		hdd_err("attr num networks failed");
		return -EINVAL;
	}

	/*
	 * num_networks is also used as EPNO SET/RESET request.
	 * if num_networks is zero then it is treated as RESET.
	 */
	num_networks = nla_get_u32(tb[id]);

	if (num_networks > MAX_EPNO_NETWORKS) {
		hdd_debug("num of nw: %d exceeded max: %d, resetting to: %d",
			num_networks, MAX_EPNO_NETWORKS, MAX_EPNO_NETWORKS);
		num_networks = MAX_EPNO_NETWORKS;
	}

	hdd_debug("num networks %u", num_networks);
	len = sizeof(*req_msg) +
			(num_networks * sizeof(req_msg->networks[0]));

	req_msg = qdf_mem_malloc(len);
	if (!req_msg)
		return -ENOMEM;

	req_msg->num_networks = num_networks;

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_PNO_CONFIG_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		goto fail;
	}
	req_msg->request_id = nla_get_u32(tb[id]);
	hdd_debug("Req Id %u", req_msg->request_id);

	req_msg->vdev_id = adapter->vdev_id;
	hdd_debug("Vdev Id %d", req_msg->vdev_id);

	if (num_networks) {
		/* Parse and fetch min_5ghz_rssi */
		id = QCA_WLAN_VENDOR_ATTR_EPNO_MIN5GHZ_RSSI;
		if (!tb[id]) {
			hdd_err("min_5ghz_rssi id failed");
			goto fail;
		}
		req_msg->min_5ghz_rssi = nla_get_u32(tb[id]);

		/* Parse and fetch min_24ghz_rssi */
		id = QCA_WLAN_VENDOR_ATTR_EPNO_MIN24GHZ_RSSI;
		if (!tb[id]) {
			hdd_err("min_24ghz_rssi id failed");
			goto fail;
		}
		req_msg->min_24ghz_rssi = nla_get_u32(tb[id]);

		/* Parse and fetch initial_score_max */
		id = QCA_WLAN_VENDOR_ATTR_EPNO_INITIAL_SCORE_MAX;
		if (!tb[id]) {
			hdd_err("initial_score_max id failed");
			goto fail;
		}
		req_msg->initial_score_max = nla_get_u32(tb[id]);

		/* Parse and fetch current_connection_bonus */
		id = QCA_WLAN_VENDOR_ATTR_EPNO_CURRENT_CONNECTION_BONUS;
		if (!tb[id]) {
			hdd_err("current_connection_bonus id failed");
			goto fail;
		}
		req_msg->current_connection_bonus = nla_get_u32(tb[id]);

		/* Parse and fetch same_network_bonus */
		id = QCA_WLAN_VENDOR_ATTR_EPNO_SAME_NETWORK_BONUS;
		if (!tb[id]) {
			hdd_err("same_network_bonus id failed");
			goto fail;
		}
		req_msg->same_network_bonus = nla_get_u32(tb[id]);

		/* Parse and fetch secure_bonus */
		id = QCA_WLAN_VENDOR_ATTR_EPNO_SECURE_BONUS;
		if (!tb[id]) {
			hdd_err("secure_bonus id failed");
			goto fail;
		}
		req_msg->secure_bonus = nla_get_u32(tb[id]);

		/* Parse and fetch band_5ghz_bonus */
		id = QCA_WLAN_VENDOR_ATTR_EPNO_BAND5GHZ_BONUS;
		if (!tb[id]) {
			hdd_err("band_5ghz_bonus id failed");
			goto fail;
		}
		req_msg->band_5ghz_bonus = nla_get_u32(tb[id]);

		hdd_debug("min_5ghz_rssi: %d min_24ghz_rssi: %d",
			req_msg->min_5ghz_rssi,
			req_msg->min_24ghz_rssi);
		hdd_debug("initial_score_max: %d current_connection_bonus:%d",
			req_msg->initial_score_max,
			req_msg->current_connection_bonus);
		hdd_debug("Bonuses same_network: %d secure: %d band_5ghz: %d",
			req_msg->same_network_bonus,
			req_msg->secure_bonus,
			req_msg->band_5ghz_bonus);

		id = QCA_WLAN_VENDOR_ATTR_PNO_SET_LIST_PARAM_EPNO_NETWORKS_LIST;
		networks = tb[id];
		if (hdd_extscan_epno_fill_network_list(req_msg, networks))
			goto fail;

	}

	status = sme_set_epno_list(hdd_ctx->mac_handle, req_msg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_set_epno_list failed(err=%d)", status);
		goto fail;
	}

	hdd_exit();
	qdf_mem_free(req_msg);
	return 0;

fail:
	qdf_mem_free(req_msg);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_set_epno_list() - epno set network list
 * @wiphy: wiphy
 * @wdev: pointer to wireless dev
 * @data: data pointer
 * @data_len: data length
 *
 * This function reads the NL vendor attributes from %tb and
 * fill in the epno request message.
 *
 * Return: 0 on success, error number otherwise
 */
int wlan_hdd_cfg80211_set_epno_list(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data,
				    int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_epno_list(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * hdd_extscan_passpoint_fill_network() - passpoint fill single network
 * @network: aggregate network attribute
 * @nw: passpoint network record to be filled
 *
 * This function takes a single network block NL vendor attribute from
 * @network and decodes it into the internal record @nw.
 *
 * Return: 0 on success, error number otherwise
 */
static int
hdd_extscan_passpoint_fill_network(struct nlattr *network,
				   struct wifi_passpoint_network_param *nw)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PNO_MAX + 1];
	int id;
	size_t len;

	if (!network) {
		hdd_err("attr network attr failed");
		return -EINVAL;
	}

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PNO_MAX,
				    nla_data(network),
				    nla_len(network),
				    wlan_hdd_pno_config_policy)) {
		hdd_err("nla_parse failed");
		return -EINVAL;
	}

	/* Parse and fetch identifier */
	id = QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ID;
	if (!tb[id]) {
		hdd_err("attr passpoint id failed");
		return -EINVAL;
	}
	nw->id = nla_get_u32(tb[id]);
	hdd_debug("Id %u", nw->id);

	/* Parse and fetch realm */
	id = QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_REALM;
	if (!tb[id]) {
		hdd_err("attr realm failed");
		return -EINVAL;
	}
	len = nla_strlcpy(nw->realm, tb[id],
			  WMI_PASSPOINT_REALM_LEN);
	/* Don't send partial realm to firmware */
	if (len >= WMI_PASSPOINT_REALM_LEN) {
		hdd_err("user passed invalid realm, len:%zu", len);
		return -EINVAL;
	}

	hdd_debug("realm: %s", nw->realm);

	/* Parse and fetch roaming consortium ids */
	id = QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ROAM_CNSRTM_ID;
	if (!tb[id]) {
		hdd_err("attr roaming consortium ids failed");
		return -EINVAL;
	}
	nla_memcpy(&nw->roaming_consortium_ids, tb[id],
		   sizeof(nw->roaming_consortium_ids));
	hdd_debug("roaming consortium ids");

	/* Parse and fetch plmn */
	id = QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_NETWORK_PARAM_ROAM_PLMN;
	if (!tb[id]) {
		hdd_err("attr plmn failed");
		return -EINVAL;
	}
	nla_memcpy(&nw->plmn, tb[id],
		   WMI_PASSPOINT_PLMN_LEN);
	hdd_debug("plmn %02x:%02x:%02x)",
		  nw->plmn[0],
		  nw->plmn[1],
		  nw->plmn[2]);

	return 0;
}

/**
 * hdd_extscan_passpoint_fill_networks() - passpoint fill network list
 * @req_msg: request message
 * @networks: aggregate network list attribute
 *
 * This function reads the network block NL vendor attributes from
 * @networks and fills in the passpoint request message.
 *
 * Return: 0 on success, error number otherwise
 */
static int
hdd_extscan_passpoint_fill_networks(struct wifi_passpoint_req_param *req_msg,
				    struct nlattr *networks)
{
	struct nlattr *network;
	int rem;
	uint32_t index;
	uint32_t expected_networks;
	struct wifi_passpoint_network_param *nw;

	if (!networks) {
		hdd_err("attr networks list failed");
		return -EINVAL;
	}

	expected_networks = req_msg->num_networks;
	index = 0;

	nla_for_each_nested(network, networks, rem) {
		if (index == expected_networks) {
			hdd_warn("ignoring excess networks");
			break;
		}

		nw = &req_msg->networks[index++];
		if (hdd_extscan_passpoint_fill_network(network, nw))
			return -EINVAL;
	}
	req_msg->num_networks = index;
	return 0;
}

/**
 * __wlan_hdd_cfg80211_set_passpoint_list() - set passpoint network list
 * @wiphy: wiphy
 * @wdev: pointer to wireless dev
 * @data: data pointer
 * @data_len: data length
 *
 * This function reads the NL vendor attributes from %tb and
 * fill in the passpoint request message.
 *
 * Return: 0 on success, error number otherwise
 */
static int __wlan_hdd_cfg80211_set_passpoint_list(struct wiphy *wiphy,
						  struct wireless_dev *wdev,
						  const void *data,
						  int data_len)
{
	struct wifi_passpoint_req_param *req_msg;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PNO_MAX + 1];
	struct nlattr *networks;
	uint32_t num_networks;
	QDF_STATUS status;
	int id, ret;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PNO_MAX, data,
				    data_len, wlan_hdd_pno_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	/* Parse and fetch number of networks */
	id = QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_LIST_PARAM_NUM;
	if (!tb[id]) {
		hdd_err("attr num networks failed");
		return -EINVAL;
	}
	num_networks = nla_get_u32(tb[id]);
	if (num_networks > SIR_PASSPOINT_LIST_MAX_NETWORKS) {
		hdd_err("num networks %u exceeds max %u",
			num_networks, SIR_PASSPOINT_LIST_MAX_NETWORKS);
		return -EINVAL;
	}

	hdd_debug("num networks %u", num_networks);

	req_msg = qdf_mem_malloc(sizeof(*req_msg) +
			(num_networks * sizeof(req_msg->networks[0])));
	if (!req_msg)
		return -ENOMEM;

	req_msg->num_networks = num_networks;

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		goto fail;
	}
	req_msg->request_id = nla_get_u32(tb[id]);

	req_msg->vdev_id = adapter->vdev_id;
	hdd_debug("Req Id %u Vdev Id %d",
		  req_msg->request_id, req_msg->vdev_id);

	id = QCA_WLAN_VENDOR_ATTR_PNO_PASSPOINT_LIST_PARAM_NETWORK_ARRAY;
	networks = tb[id];
	if (hdd_extscan_passpoint_fill_networks(req_msg, networks))
		goto fail;

	status = sme_set_passpoint_list(hdd_ctx->mac_handle, req_msg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_set_passpoint_list failed(err=%d)", status);
		goto fail;
	}

	hdd_exit();
	qdf_mem_free(req_msg);
	return 0;

fail:
	qdf_mem_free(req_msg);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_set_passpoint_list() - set passpoint network list
 * @wiphy: wiphy
 * @wdev: pointer to wireless dev
 * @data: data pointer
 * @data_len: data length
 *
 * This function reads the NL vendor attributes from %tb and
 * fill in the passpoint request message.
 *
 * Return: 0 on success, error number otherwise
 */
int wlan_hdd_cfg80211_set_passpoint_list(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_passpoint_list(wiphy, wdev,
						       data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __wlan_hdd_cfg80211_reset_passpoint_list() - reset passpoint network list
 * @wiphy: wiphy
 * @wdev: pointer to wireless dev
 * @data: data pointer
 * @data_len: data length
 *
 * This function resets passpoint networks list
 *
 * Return: 0 on success, error number otherwise
 */
static int __wlan_hdd_cfg80211_reset_passpoint_list(struct wiphy *wiphy,
						    struct wireless_dev *wdev,
						    const void *data,
						    int data_len)
{
	struct wifi_passpoint_req_param *req_msg;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_PNO_MAX + 1];
	QDF_STATUS status;
	int id, ret;

	hdd_enter_dev(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_PNO_MAX, data,
				    data_len, wlan_hdd_extscan_config_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	req_msg = qdf_mem_malloc(sizeof(*req_msg));
	if (!req_msg)
		return -ENOMEM;

	/* Parse and fetch request Id */
	id = QCA_WLAN_VENDOR_ATTR_EXTSCAN_SUBCMD_CONFIG_PARAM_REQUEST_ID;
	if (!tb[id]) {
		hdd_err("attr request id failed");
		goto fail;
	}
	req_msg->request_id = nla_get_u32(tb[id]);

	req_msg->vdev_id = adapter->vdev_id;
	hdd_debug("Req Id %u Vdev Id %d",
		  req_msg->request_id, req_msg->vdev_id);

	status = sme_reset_passpoint_list(hdd_ctx->mac_handle, req_msg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_reset_passpoint_list failed(err=%d)", status);
		goto fail;
	}

	hdd_exit();
	qdf_mem_free(req_msg);
	return 0;

fail:
	qdf_mem_free(req_msg);
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_reset_passpoint_list() - reset passpoint network list
 * @wiphy: wiphy
 * @wdev: pointer to wireless dev
 * @data: data pointer
 * @data_len: data length
 *
 * This function resets passpoint networks list
 *
 * Return: 0 on success, error number otherwise
 */
int wlan_hdd_cfg80211_reset_passpoint_list(struct wiphy *wiphy,
					   struct wireless_dev *wdev,
					   const void *data,
					   int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_reset_passpoint_list(wiphy, wdev,
							 data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * wlan_hdd_cfg80211_extscan_init() - Initialize the ExtScan feature
 * @hdd_ctx: Global HDD context
 *
 * Return: none
 */
void wlan_hdd_cfg80211_extscan_init(struct hdd_context *hdd_ctx)
{
	init_completion(&ext_scan_context.response_event);
	spin_lock_init(&ext_scan_context.context_lock);
}

#endif /* FEATURE_WLAN_EXTSCAN */
