/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_station_info.c
 *
 * WLAN station info functions
 *
 */

#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <wlan_cfg80211_mc_cp_stats.h>
#include <wlan_cp_stats_mc_ucfg_api.h>
#include <wlan_hdd_stats.h>
#include <wlan_hdd_hostapd.h>
#include <wlan_hdd_station_info.h>
#include "wlan_mlme_ucfg_api.h"
#include "wlan_hdd_sta_info.h"
#include "wlan_hdd_object_manager.h"
#include "wlan_ipa_ucfg_api.h"

#include <cdp_txrx_handle.h>
#include <cdp_txrx_stats_struct.h>
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_host_stats.h>
#include "wlan_hdd_stats.h"

/*
 * define short names for the global vendor params
 * used by __wlan_hdd_cfg80211_get_station_cmd()
 */
#define STATION_INVALID \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INVALID
#define STATION_INFO \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO
#define STATION_ASSOC_FAIL_REASON \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_ASSOC_FAIL_REASON
#define STATION_REMOTE \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_REMOTE
#define STATION_MAX \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_MAX

#define STA_INFO_INVALID \
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_INVALID
#define STA_INFO_BIP_MIC_ERROR_COUNT \
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_MIC_ERROR_COUNT
#define STA_INFO_BIP_REPLAY_COUNT \
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BIP_REPLAY_COUNT
#define STA_INFO_BEACON_MIC_ERROR_COUNT \
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_MIC_ERROR_COUNT
#define STA_INFO_BEACON_REPLAY_COUNT \
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_BEACON_REPLAY_COUNT
#define STA_INFO_CONNECT_FAIL_REASON_CODE \
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_CONNECT_FAIL_REASON_CODE
#define STA_INFO_MAX \
	QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX

/* define short names for get station info attributes */
#define LINK_INFO_STANDARD_NL80211_ATTR \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_LINK_STANDARD_NL80211_ATTR
#define AP_INFO_STANDARD_NL80211_ATTR \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_AP_STANDARD_NL80211_ATTR
#define INFO_ROAM_COUNT \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_ROAM_COUNT
#define INFO_AKM \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_AKM
#define WLAN802_11_MODE \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_802_11_MODE
#define AP_INFO_HS20_INDICATION \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_AP_HS20_INDICATION
#define HT_OPERATION \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_HT_OPERATION
#define VHT_OPERATION \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_VHT_OPERATION
#define HE_OPERATION \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_HE_OPERATION
#define INFO_ASSOC_FAIL_REASON \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_ASSOC_FAIL_REASON
#define REMOTE_MAX_PHY_RATE \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_MAX_PHY_RATE
#define REMOTE_TX_PACKETS \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_TX_PACKETS
#define REMOTE_TX_BYTES \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_TX_BYTES
#define REMOTE_RX_PACKETS \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_RX_PACKETS
#define REMOTE_RX_BYTES \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_RX_BYTES
#define REMOTE_LAST_TX_RATE \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_LAST_TX_RATE
#define REMOTE_LAST_RX_RATE \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_LAST_RX_RATE
#define REMOTE_WMM \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_WMM
#define REMOTE_SUPPORTED_MODE \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_SUPPORTED_MODE
#define REMOTE_AMPDU \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_AMPDU
#define REMOTE_TX_STBC \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_TX_STBC
#define REMOTE_RX_STBC \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_RX_STBC
#define REMOTE_CH_WIDTH\
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_CH_WIDTH
#define REMOTE_SGI_ENABLE\
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_SGI_ENABLE
#define REMOTE_PAD\
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_PAD
#define REMOTE_RX_RETRY_COUNT \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_RX_RETRY_COUNT
#define REMOTE_RX_BC_MC_COUNT \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_REMOTE_RX_BC_MC_COUNT
#define DISCONNECT_REASON \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_DRIVER_DISCONNECT_REASON
#define BEACON_IES \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_BEACON_IES
#define ASSOC_REQ_IES \
	QCA_WLAN_VENDOR_ATTR_GET_STATION_INFO_ASSOC_REQ_IES

/*
 * MSB of rx_mc_bc_cnt indicates whether FW supports rx_mc_bc_cnt
 * feature or not, if first bit is 1 it indictes that FW supports this
 * feature, if it is 0 it indicates FW doesn't support this feature
 */
#define HDD_STATION_INFO_RX_MC_BC_COUNT (1 << 31)

const struct nla_policy
hdd_get_station_policy[STATION_MAX + 1] = {
	[STATION_INFO] = {.type = NLA_FLAG},
	[STATION_ASSOC_FAIL_REASON] = {.type = NLA_FLAG},
	[STATION_REMOTE] = {.type = NLA_BINARY, .len = QDF_MAC_ADDR_SIZE},
};

const struct nla_policy
hdd_get_sta_policy[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC] = {.type = NLA_BINARY,
						   .len = QDF_MAC_ADDR_SIZE},
};

static int hdd_get_sta_congestion(struct hdd_adapter *adapter,
				  uint32_t *congestion)
{
	QDF_STATUS status;
	struct cca_stats cca_stats;
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("vdev is NULL");
		return -EINVAL;
	}
	status = ucfg_mc_cp_stats_cca_stats_get(vdev, &cca_stats);
	hdd_objmgr_put_vdev(vdev);
	if (QDF_IS_STATUS_ERROR(status))
		return -EINVAL;

	*congestion = cca_stats.congestion;
	return 0;
}

/**
 * hdd_get_station_assoc_fail() - Handle get station assoc fail
 * @hdd_ctx: HDD context within host driver
 * @wdev: wireless device
 *
 * Handles QCA_NL80211_VENDOR_SUBCMD_GET_STATION_ASSOC_FAIL.
 * Validate cmd attributes and send the station info to upper layers.
 *
 * Return: Success(0) or reason code for failure
 */
static int hdd_get_station_assoc_fail(struct hdd_context *hdd_ctx,
				      struct hdd_adapter *adapter)
{
	struct sk_buff *skb = NULL;
	uint32_t nl_buf_len;
	struct hdd_station_ctx *hdd_sta_ctx;
	uint32_t congestion;

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len += sizeof(uint32_t);
	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);

	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	if (nla_put_u32(skb, INFO_ASSOC_FAIL_REASON,
			hdd_sta_ctx->conn_info.assoc_status_code)) {
		hdd_err("put fail");
		goto fail;
	}

	if (hdd_get_sta_congestion(adapter, &congestion))
		congestion = 0;

	hdd_info("congestion:%d", congestion);
	if (nla_put_u32(skb, NL80211_SURVEY_INFO_CHANNEL_TIME_BUSY,
			congestion)) {
		hdd_err("put fail");
		goto fail;
	}

	return cfg80211_vendor_cmd_reply(skb);
fail:
	if (skb)
		kfree_skb(skb);
	return -EINVAL;
}

/**
 * hdd_map_auth_type() - transform auth type specific to
 * vendor command
 * @auth_type: csr auth type
 *
 * Return: Success(0) or reason code for failure
 */
static int hdd_convert_auth_type(uint32_t auth_type)
{
	uint32_t ret_val;

	switch (auth_type) {
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		ret_val = QCA_WLAN_AUTH_TYPE_OPEN;
		break;
	case eCSR_AUTH_TYPE_SHARED_KEY:
		ret_val = QCA_WLAN_AUTH_TYPE_SHARED;
		break;
	case eCSR_AUTH_TYPE_WPA:
		ret_val = QCA_WLAN_AUTH_TYPE_WPA;
		break;
	case eCSR_AUTH_TYPE_WPA_PSK:
		ret_val = QCA_WLAN_AUTH_TYPE_WPA_PSK;
		break;
	case eCSR_AUTH_TYPE_AUTOSWITCH:
		ret_val = QCA_WLAN_AUTH_TYPE_AUTOSWITCH;
		break;
	case eCSR_AUTH_TYPE_WPA_NONE:
		ret_val = QCA_WLAN_AUTH_TYPE_WPA_NONE;
		break;
	case eCSR_AUTH_TYPE_RSN:
		ret_val = QCA_WLAN_AUTH_TYPE_RSN;
		break;
	case eCSR_AUTH_TYPE_RSN_PSK:
		ret_val = QCA_WLAN_AUTH_TYPE_RSN_PSK;
		break;
	case eCSR_AUTH_TYPE_FT_RSN:
		ret_val = QCA_WLAN_AUTH_TYPE_FT;
		break;
	case eCSR_AUTH_TYPE_FT_RSN_PSK:
		ret_val = QCA_WLAN_AUTH_TYPE_FT_PSK;
		break;
	case eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE:
		ret_val = QCA_WLAN_AUTH_TYPE_WAI;
		break;
	case eCSR_AUTH_TYPE_WAPI_WAI_PSK:
		ret_val = QCA_WLAN_AUTH_TYPE_WAI_PSK;
		break;
	case eCSR_AUTH_TYPE_CCKM_WPA:
		ret_val = QCA_WLAN_AUTH_TYPE_CCKM_WPA;
		break;
	case eCSR_AUTH_TYPE_CCKM_RSN:
		ret_val = QCA_WLAN_AUTH_TYPE_CCKM_RSN;
		break;
	case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
		ret_val = QCA_WLAN_AUTH_TYPE_SHA256_PSK;
		break;
	case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
		ret_val = QCA_WLAN_AUTH_TYPE_SHA256;
		break;
	case eCSR_AUTH_TYPE_FT_SAE:
		ret_val = QCA_WLAN_AUTH_TYPE_FT_SAE;
		break;
	case eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384:
		ret_val = QCA_WLAN_AUTH_TYPE_FT_SUITEB_EAP_SHA384;
		break;
	case eCSR_AUTH_TYPE_SAE:
		ret_val = QCA_WLAN_AUTH_TYPE_SAE;
		break;
	case eCSR_AUTH_TYPE_FILS_SHA256:
		ret_val = QCA_WLAN_AUTH_TYPE_FILS_SHA256;
		break;
	case eCSR_AUTH_TYPE_FILS_SHA384:
		ret_val = QCA_WLAN_AUTH_TYPE_FILS_SHA384;
		break;
	case eCSR_AUTH_TYPE_FT_FILS_SHA256:
		ret_val = QCA_WLAN_AUTH_TYPE_FT_FILS_SHA256;
		break;
	case eCSR_AUTH_TYPE_FT_FILS_SHA384:
		ret_val = QCA_WLAN_AUTH_TYPE_FT_FILS_SHA384;
		break;
	case eCSR_AUTH_TYPE_DPP_RSN:
		ret_val = QCA_WLAN_AUTH_TYPE_DPP_RSN;
		break;
	case eCSR_AUTH_TYPE_OWE:
		ret_val = QCA_WLAN_AUTH_TYPE_OWE;
		break;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA256:
		ret_val = QCA_WLAN_AUTH_TYPE_SUITEB_EAP_SHA256;
		break;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA384:
		ret_val = QCA_WLAN_AUTH_TYPE_SUITEB_EAP_SHA384;
		break;
	case eCSR_NUM_OF_SUPPORT_AUTH_TYPE:
	case eCSR_AUTH_TYPE_FAILED:
	case eCSR_AUTH_TYPE_NONE:
	default:
		ret_val = QCA_WLAN_AUTH_TYPE_INVALID;
		break;
	}
	return ret_val;
}

/**
 * hdd_map_dot_11_mode() - transform dot11mode type specific to
 * vendor command
 * @dot11mode: dot11mode
 *
 * Return: Success(0) or reason code for failure
 */
static int hdd_convert_dot11mode(uint32_t dot11mode)
{
	uint32_t ret_val;

	switch (dot11mode) {
	case eCSR_CFG_DOT11_MODE_11A:
		ret_val = QCA_WLAN_802_11_MODE_11A;
		break;
	case eCSR_CFG_DOT11_MODE_11B:
		ret_val = QCA_WLAN_802_11_MODE_11B;
		break;
	case eCSR_CFG_DOT11_MODE_11G:
		ret_val = QCA_WLAN_802_11_MODE_11G;
		break;
	case eCSR_CFG_DOT11_MODE_11N:
		ret_val = QCA_WLAN_802_11_MODE_11N;
		break;
	case eCSR_CFG_DOT11_MODE_11AC:
		ret_val = QCA_WLAN_802_11_MODE_11AC;
		break;
	case eCSR_CFG_DOT11_MODE_AUTO:
	case eCSR_CFG_DOT11_MODE_ABG:
	default:
		ret_val = QCA_WLAN_802_11_MODE_INVALID;
	}
	return ret_val;
}

/**
 * hdd_add_tx_bitrate() - add tx bitrate attribute
 * @skb: pointer to sk buff
 * @hdd_sta_ctx: pointer to hdd station context
 * @idx: attribute index
 *
 * Return: Success(0) or reason code for failure
 */
static int32_t hdd_add_tx_bitrate(struct sk_buff *skb,
				  struct hdd_station_ctx *hdd_sta_ctx,
				  int idx)
{
	struct nlattr *nla_attr;
	uint32_t bitrate, bitrate_compat;

	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr) {
		hdd_err("nla_nest_start failed");
		goto fail;
	}

	/* cfg80211_calculate_bitrate will return 0 for mcs >= 32 */
	if (hdd_conn_is_connected(hdd_sta_ctx))
		bitrate = cfg80211_calculate_bitrate(
				&hdd_sta_ctx->cache_conn_info.max_tx_bitrate);
	else
		bitrate = cfg80211_calculate_bitrate(
					&hdd_sta_ctx->cache_conn_info.txrate);
	/* report 16-bit bitrate only if we can */
	bitrate_compat = bitrate < (1UL << 16) ? bitrate : 0;

	if (bitrate > 0) {
		if (nla_put_u32(skb, NL80211_RATE_INFO_BITRATE32, bitrate)) {
			hdd_err("put fail bitrate: %u", bitrate);
			goto fail;
		}
	} else {
		hdd_err("Invalid bitrate: %u", bitrate);
	}

	if (bitrate_compat > 0) {
		if (nla_put_u16(skb, NL80211_RATE_INFO_BITRATE,
				bitrate_compat)) {
			hdd_err("put fail bitrate_compat: %u", bitrate_compat);
			goto fail;
		}
	} else {
		hdd_err("Invalid bitrate_compat: %u", bitrate_compat);
	}

	if (nla_put_u8(skb, NL80211_RATE_INFO_VHT_NSS,
		       hdd_sta_ctx->cache_conn_info.txrate.nss)) {
		hdd_err("put fail");
		goto fail;
	}
	nla_nest_end(skb, nla_attr);

	hdd_nofl_debug(
		"STA Tx rate info:: bitrate:%d, bitrate_compat:%d, NSS:%d",
		bitrate, bitrate_compat,
		hdd_sta_ctx->cache_conn_info.txrate.nss);

	return 0;
fail:
	return -EINVAL;
}

/**
 * hdd_get_max_tx_bitrate() - Get the max tx bitrate of the AP
 * @hdd_ctx: hdd context
 * @adapter: hostapd interface
 *
 * THis function gets the MAX supported rate by AP and cache
 * it into connection info structure
 *
 * Return: None
 */
static void hdd_get_max_tx_bitrate(struct hdd_context *hdd_ctx,
				   struct hdd_adapter *adapter)
{
	struct station_info sinfo;
	enum tx_rate_info tx_rate_flags;
	uint8_t tx_mcs_index, tx_nss = 1;
	uint16_t my_tx_rate;
	struct hdd_station_ctx *hdd_sta_ctx;
	struct wlan_objmgr_vdev *vdev;

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	qdf_mem_zero(&sinfo, sizeof(struct station_info));

	sinfo.signal = adapter->rssi;
	tx_rate_flags = adapter->hdd_stats.class_a_stat.tx_rx_rate_flags;
	tx_mcs_index = adapter->hdd_stats.class_a_stat.tx_mcs_index;
	my_tx_rate = adapter->hdd_stats.class_a_stat.tx_rate;

	if (!(tx_rate_flags & TX_RATE_LEGACY)) {
		vdev = hdd_objmgr_get_vdev(adapter);
		if (vdev) {
			/*
			 * Take static NSS for reporting max rates.
			 * NSS from FW is not reliable as it changes
			 * as per the environment quality.
			 */
			tx_nss = wlan_vdev_mlme_get_nss(vdev);
			hdd_objmgr_put_vdev(vdev);
		} else {
			tx_nss = adapter->hdd_stats.class_a_stat.tx_nss;
		}
		hdd_check_and_update_nss(hdd_ctx, &tx_nss, NULL);

		if (tx_mcs_index == INVALID_MCS_IDX)
			tx_mcs_index = 0;
	}

	if (hdd_report_max_rate(adapter, hdd_ctx->mac_handle, &sinfo.txrate,
				sinfo.signal, tx_rate_flags, tx_mcs_index,
				my_tx_rate, tx_nss)) {
		hdd_sta_ctx->cache_conn_info.max_tx_bitrate = sinfo.txrate;
		hdd_debug("Reporting max tx rate flags %d mcs %d nss %d bw %d",
			  sinfo.txrate.flags, sinfo.txrate.mcs,
			  sinfo.txrate.nss, sinfo.txrate.bw);
	}
}

/**
 * hdd_add_sta_info() - add station info attribute
 * @skb: pointer to sk buff
 * @hdd_sta_ctx: pointer to hdd station context
 * @idx: attribute index
 *
 * Return: Success(0) or reason code for failure
 */
static int32_t hdd_add_sta_info(struct sk_buff *skb,
				struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter,
				int idx)
{
	struct nlattr *nla_attr;
	struct hdd_station_ctx *hdd_sta_ctx;

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!hdd_sta_ctx) {
		hdd_err("Invalid sta ctx");
		goto fail;
	}

	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr) {
		hdd_err("nla_nest_start failed");
		goto fail;
	}

	if (nla_put_u8(skb, NL80211_STA_INFO_SIGNAL,
		       (hdd_sta_ctx->cache_conn_info.signal + 100))) {
		hdd_err("put fail");
		goto fail;
	}
	if (hdd_conn_is_connected(hdd_sta_ctx))
		hdd_get_max_tx_bitrate(hdd_ctx, adapter);

	if (hdd_add_tx_bitrate(skb, hdd_sta_ctx, NL80211_STA_INFO_TX_BITRATE)) {
		hdd_err("hdd_add_tx_bitrate failed");
		goto fail;
	}

	nla_nest_end(skb, nla_attr);
	return 0;
fail:
	return -EINVAL;
}

/**
 * hdd_add_survey_info() - add survey info attribute
 * @skb: pointer to sk buff
 * @hdd_sta_ctx: pointer to hdd station context
 * @idx: attribute index
 *
 * Return: Success(0) or reason code for failure
 */
static int32_t hdd_add_survey_info(struct sk_buff *skb,
				   struct hdd_station_ctx *hdd_sta_ctx,
				   int idx)
{
	struct nlattr *nla_attr;

	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr)
		goto fail;
	if (nla_put_u32(skb, NL80211_SURVEY_INFO_FREQUENCY,
			hdd_sta_ctx->cache_conn_info.chan_freq) ||
	    nla_put_u8(skb, NL80211_SURVEY_INFO_NOISE,
		       (hdd_sta_ctx->cache_conn_info.noise + 100))) {
		hdd_err("put fail");
		goto fail;
	}
	nla_nest_end(skb, nla_attr);
	return 0;
fail:
	return -EINVAL;
}

/**
 * hdd_add_link_standard_info() - add link info attribute
 * @skb: pointer to sk buff
 * @hdd_sta_ctx: pointer to hdd station context
 * @idx: attribute index
 *
 * Return: Success(0) or reason code for failure
 */
static int32_t
hdd_add_link_standard_info(struct sk_buff *skb,
			   struct hdd_context *hdd_ctx,
			   struct hdd_adapter *adapter, int idx)
{
	struct nlattr *nla_attr;
	struct hdd_station_ctx *hdd_sta_ctx;

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!hdd_sta_ctx) {
		hdd_err("Invalid sta ctx");
		goto fail;
	}

	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr) {
		hdd_err("nla_nest_start failed");
		goto fail;
	}

	if (nla_put(skb,
		    NL80211_ATTR_SSID,
		    hdd_sta_ctx->cache_conn_info.last_ssid.SSID.length,
		    hdd_sta_ctx->cache_conn_info.last_ssid.SSID.ssId)) {
		hdd_err("put fail");
		goto fail;
	}
	if (nla_put(skb, NL80211_ATTR_MAC, QDF_MAC_ADDR_SIZE,
		    hdd_sta_ctx->cache_conn_info.bssid.bytes)) {
		hdd_err("put bssid failed");
		goto fail;
	}
	if (hdd_add_survey_info(skb, hdd_sta_ctx, NL80211_ATTR_SURVEY_INFO)) {
		hdd_err("hdd_add_survey_info failed");
		goto fail;
	}

	if (hdd_add_sta_info(skb, hdd_ctx, adapter, NL80211_ATTR_STA_INFO)) {
		hdd_err("hdd_add_sta_info failed");
		goto fail;
	}
	nla_nest_end(skb, nla_attr);
	return 0;
fail:
	return -EINVAL;
}

/**
 * hdd_add_ap_standard_info() - add ap info attribute
 * @skb: pointer to sk buff
 * @hdd_sta_ctx: pointer to hdd station context
 * @idx: attribute index
 *
 * Return: Success(0) or reason code for failure
 */
static int32_t
hdd_add_ap_standard_info(struct sk_buff *skb,
			 struct hdd_station_ctx *hdd_sta_ctx, int idx)
{
	struct nlattr *nla_attr;
	struct hdd_connection_info *conn_info;

	conn_info = &hdd_sta_ctx->cache_conn_info;
	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr)
		goto fail;
	if (conn_info->conn_flag.vht_present) {
		if (nla_put(skb, NL80211_ATTR_VHT_CAPABILITY,
			    sizeof(conn_info->vht_caps),
			    &conn_info->vht_caps)) {
			hdd_err("put fail");
			goto fail;
		}
		hdd_nofl_debug("STA VHT capabilities:");
		qdf_trace_hex_dump(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
				   (uint8_t *)&conn_info->vht_caps,
				   sizeof(conn_info->vht_caps));
	}
	if (conn_info->conn_flag.ht_present) {
		if (nla_put(skb, NL80211_ATTR_HT_CAPABILITY,
			    sizeof(conn_info->ht_caps),
			    &conn_info->ht_caps)) {
			hdd_err("put fail");
			goto fail;
		}
		hdd_nofl_debug("STA HT capabilities:");
		qdf_trace_hex_dump(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
				   (uint8_t *)&conn_info->ht_caps,
				   sizeof(conn_info->ht_caps));
	}
	nla_nest_end(skb, nla_attr);
	return 0;
fail:
	return -EINVAL;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)) && \
     defined(WLAN_FEATURE_11AX)
static int32_t hdd_add_he_oper_info(struct sk_buff *skb,
				    struct hdd_station_ctx *hdd_sta_ctx)
{
	int32_t ret = 0;
	struct hdd_connection_info *conn_info;

	conn_info = &hdd_sta_ctx->cache_conn_info;
	if (!conn_info->he_oper_len || !conn_info->he_operation)
		return ret;

	if (nla_put(skb, HE_OPERATION, conn_info->he_oper_len,
		    conn_info->he_operation)) {
		ret = -EINVAL;
	} else {
		hdd_nofl_debug("STA HE operation:");
		qdf_trace_hex_dump(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
				   (uint8_t *)&conn_info->he_operation,
				   conn_info->he_oper_len);
	}

	qdf_mem_free(hdd_sta_ctx->cache_conn_info.he_operation);
	hdd_sta_ctx->cache_conn_info.he_operation = NULL;
	hdd_sta_ctx->cache_conn_info.he_oper_len = 0;
	return ret;
}

static int32_t hdd_get_he_op_len(struct hdd_station_ctx *hdd_sta_ctx)
{
	return hdd_sta_ctx->cache_conn_info.he_oper_len;
}
#else
static inline uint32_t hdd_add_he_oper_info(
					struct sk_buff *skb,
					struct hdd_station_ctx *hdd_sta_ctx)
{
	return 0;
}

static uint32_t hdd_get_he_op_len(struct hdd_station_ctx *hdd_sta_ctx)
{
	return 0;
}
#endif

/**
 * hdd_get_station_info() - send BSS information to supplicant
 * @hdd_ctx: pointer to hdd context
 * @adapter: pointer to adapter
 *
 * Return: 0 if success else error status
 */
static int hdd_get_station_info(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter)
{
	struct sk_buff *skb = NULL;
	uint8_t *tmp_hs20 = NULL, *ies = NULL;
	uint32_t nl_buf_len, ie_len = 0, hdd_he_op_len = 0;
	struct hdd_station_ctx *hdd_sta_ctx;
	QDF_STATUS status;

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len += sizeof(hdd_sta_ctx->
				cache_conn_info.last_ssid.SSID.length) +
		      QDF_MAC_ADDR_SIZE +
		      sizeof(hdd_sta_ctx->cache_conn_info.chan_freq) +
		      sizeof(hdd_sta_ctx->cache_conn_info.noise) +
		      sizeof(hdd_sta_ctx->cache_conn_info.signal) +
		      (sizeof(uint32_t) * 2) +
		      sizeof(hdd_sta_ctx->cache_conn_info.txrate.nss) +
		      sizeof(hdd_sta_ctx->cache_conn_info.roam_count) +
		      sizeof(hdd_sta_ctx->cache_conn_info.last_auth_type) +
		      sizeof(hdd_sta_ctx->cache_conn_info.dot11mode) +
		      sizeof(uint32_t);
	if (hdd_sta_ctx->cache_conn_info.conn_flag.vht_present)
		nl_buf_len += sizeof(hdd_sta_ctx->cache_conn_info.vht_caps);
	if (hdd_sta_ctx->cache_conn_info.conn_flag.ht_present)
		nl_buf_len += sizeof(hdd_sta_ctx->cache_conn_info.ht_caps);
	if (hdd_sta_ctx->cache_conn_info.conn_flag.hs20_present) {
		tmp_hs20 = (uint8_t *)&(hdd_sta_ctx->
						cache_conn_info.hs20vendor_ie);
		nl_buf_len += (sizeof(hdd_sta_ctx->
					cache_conn_info.hs20vendor_ie) - 1);
	}
	if (hdd_sta_ctx->cache_conn_info.conn_flag.ht_op_present)
		nl_buf_len += sizeof(hdd_sta_ctx->
						cache_conn_info.ht_operation);
	if (hdd_sta_ctx->cache_conn_info.conn_flag.vht_op_present)
		nl_buf_len += sizeof(hdd_sta_ctx->
						cache_conn_info.vht_operation);
	status = sme_get_prev_connected_bss_ies(hdd_ctx->mac_handle,
						adapter->vdev_id,
						&ies, &ie_len);
	if (QDF_IS_STATUS_SUCCESS(status))
		nl_buf_len += ie_len;

	hdd_he_op_len = hdd_get_he_op_len(hdd_sta_ctx);
	nl_buf_len += hdd_he_op_len;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);
	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	if (hdd_add_link_standard_info(skb, hdd_ctx, adapter,
				       LINK_INFO_STANDARD_NL80211_ATTR)) {
		hdd_err("put fail");
		goto fail;
	}
	if (hdd_add_ap_standard_info(skb, hdd_sta_ctx,
				     AP_INFO_STANDARD_NL80211_ATTR)) {
		hdd_err("put fail");
		goto fail;
	}
	if (nla_put_u32(skb, INFO_ROAM_COUNT,
			hdd_sta_ctx->cache_conn_info.roam_count) ||
	    nla_put_u32(skb, INFO_AKM,
			hdd_convert_auth_type(
			hdd_sta_ctx->cache_conn_info.last_auth_type)) ||
	    nla_put_u32(skb, WLAN802_11_MODE,
			hdd_convert_dot11mode(
			hdd_sta_ctx->cache_conn_info.dot11mode))) {
		hdd_err("put fail");
		goto fail;
	}
	if (hdd_sta_ctx->cache_conn_info.conn_flag.ht_op_present) {
		if (nla_put(skb, HT_OPERATION,
			    (sizeof(hdd_sta_ctx->cache_conn_info.ht_operation)),
			    &hdd_sta_ctx->cache_conn_info.ht_operation)) {
			hdd_err("put fail");
			goto fail;
		}
		hdd_nofl_debug("STA HT operation:");
		qdf_trace_hex_dump(
			QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
			(uint8_t *)&hdd_sta_ctx->cache_conn_info.ht_operation,
			sizeof(hdd_sta_ctx->cache_conn_info.ht_operation));
	}
	if (hdd_sta_ctx->cache_conn_info.conn_flag.vht_op_present) {
		if (nla_put(skb, VHT_OPERATION,
			    (sizeof(hdd_sta_ctx->
					cache_conn_info.vht_operation)),
			    &hdd_sta_ctx->cache_conn_info.vht_operation)) {
			hdd_err("put fail");
			goto fail;
		}
		hdd_nofl_debug("STA VHT operation:");
		qdf_trace_hex_dump(
			QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
			(uint8_t *)&hdd_sta_ctx->cache_conn_info.vht_operation,
			sizeof(hdd_sta_ctx->cache_conn_info.vht_operation));
	}
	if (hdd_add_he_oper_info(skb, hdd_sta_ctx)) {
		hdd_err("put fail");
		goto fail;
	}
	if (hdd_sta_ctx->cache_conn_info.conn_flag.hs20_present) {
		if (nla_put(skb, AP_INFO_HS20_INDICATION,
			    (sizeof(hdd_sta_ctx->cache_conn_info.hs20vendor_ie)
			     - 1),
			    tmp_hs20 + 1)) {
			hdd_err("put fail");
			goto fail;
		}
		hdd_nofl_debug("STA hs20 vendor IE:");
		qdf_trace_hex_dump(
			QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
			(uint8_t *)(tmp_hs20 + 1),
			sizeof(hdd_sta_ctx->cache_conn_info.hs20vendor_ie) - 1);
	}

	if (nla_put_u32(skb, DISCONNECT_REASON,
			adapter->last_disconnect_reason)) {
		hdd_err("Failed to put disconect reason");
		goto fail;
	}

	if (ie_len) {
		if (nla_put(skb, BEACON_IES, ie_len, ies)) {
			hdd_err("Failed to put beacon IEs: bytes left: %d, ie_len: %u total buf_len: %u",
				skb_tailroom(skb), ie_len, nl_buf_len);
			goto fail;
		}

		hdd_nofl_debug("Beacon IEs len: %u", ie_len);

		qdf_mem_free(ies);
	}

	hdd_nofl_debug(
		"STA Info:: SSID:%s, BSSID:" QDF_MAC_ADDR_FMT ", freq:%d, "
		"Noise:%d, signal:%d, roam_count:%d, last_auth_type:%d, "
		"dot11mode:%d, disconnect_reason:%d, ",
		hdd_sta_ctx->cache_conn_info.last_ssid.SSID.ssId,
		QDF_MAC_ADDR_REF(hdd_sta_ctx->cache_conn_info.bssid.bytes),
		hdd_sta_ctx->cache_conn_info.chan_freq,
		(hdd_sta_ctx->cache_conn_info.noise + 100),
		(hdd_sta_ctx->cache_conn_info.signal + 100),
		hdd_sta_ctx->cache_conn_info.roam_count,
		hdd_convert_auth_type(
			hdd_sta_ctx->cache_conn_info.last_auth_type),
		hdd_convert_dot11mode(hdd_sta_ctx->cache_conn_info.dot11mode),
		adapter->last_disconnect_reason);

	return cfg80211_vendor_cmd_reply(skb);
fail:
	if (skb)
		kfree_skb(skb);
	qdf_mem_free(ies);
	return -EINVAL;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0))
static inline int32_t remote_station_put_u64(struct sk_buff *skb,
					     int32_t attrtype,
					     uint64_t value)
{
	return nla_put_u64_64bit(skb, attrtype, value, REMOTE_PAD);
}
#else
static inline int32_t remote_station_put_u64(struct sk_buff *skb,
					     int32_t attrtype,
					     uint64_t value)
{
	return nla_put_u64(skb, attrtype, value);
}
#endif

/**
 * hdd_add_survey_info_sap_get_len - get data length used in
 * hdd_add_survey_info_sap()
 *
 * This function calculates the data length used in hdd_add_survey_info_sap()
 *
 * Return: total data length used in hdd_add_survey_info_sap()
 */
static uint32_t hdd_add_survey_info_sap_get_len(void)
{
	return ((NLA_HDRLEN) + (sizeof(uint32_t) + NLA_HDRLEN));
}

/**
 * hdd_add_survey_info - add survey info attribute
 * @skb: pointer to response skb buffer
 * @stainfo: station information
 * @idx: attribute type index for nla_next_start()
 *
 * This function adds survey info attribute to response skb buffer
 *
 * Return : 0 on success and errno on failure
 */
static int32_t hdd_add_survey_info_sap(struct sk_buff *skb,
				       struct hdd_station_info *stainfo,
				       int idx)
{
	struct nlattr *nla_attr;

	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr)
		goto fail;
	if (nla_put_u32(skb, NL80211_SURVEY_INFO_FREQUENCY,
			stainfo->freq)) {
		hdd_err("put fail");
		goto fail;
	}
	nla_nest_end(skb, nla_attr);
	hdd_nofl_debug("Remote STA freq: %d", stainfo->freq);
	return 0;
fail:
	return -EINVAL;
}

/**
 * hdd_add_tx_bitrate_sap_get_len - get data length used in
 * hdd_add_tx_bitrate_sap()
 *
 * This function calculates the data length used in hdd_add_tx_bitrate_sap()
 *
 * Return: total data length used in hdd_add_tx_bitrate_sap()
 */
static uint32_t hdd_add_tx_bitrate_sap_get_len(void)
{
	return ((NLA_HDRLEN) + (sizeof(uint8_t) + NLA_HDRLEN));
}

static uint32_t hdd_add_sta_capability_get_len(void)
{
	return nla_total_size(sizeof(uint16_t));
}

/**
 * hdd_add_tx_bitrate_sap - add vhs nss info attribute
 * @skb: pointer to response skb buffer
 * @stainfo: station information
 * @idx: attribute type index for nla_next_start()
 *
 * This function adds vht nss attribute to response skb buffer
 *
 * Return : 0 on success and errno on failure
 */
static int hdd_add_tx_bitrate_sap(struct sk_buff *skb,
				  struct hdd_station_info *stainfo,
				  int idx)
{
	struct nlattr *nla_attr;

	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr)
		goto fail;

	if (nla_put_u8(skb, NL80211_RATE_INFO_VHT_NSS,
		       stainfo->nss)) {
		hdd_err("put fail");
		goto fail;
	}
	nla_nest_end(skb, nla_attr);
	hdd_nofl_debug("Remote STA VHT NSS: %d", stainfo->nss);
	return 0;
fail:
	return -EINVAL;
}

/**
 * hdd_add_sta_info_sap_get_len - get data length used in
 * hdd_add_sta_info_sap()
 *
 * This function calculates the data length used in hdd_add_sta_info_sap()
 *
 * Return: total data length used in hdd_add_sta_info_sap()
 */
static uint32_t hdd_add_sta_info_sap_get_len(void)
{
	return ((NLA_HDRLEN) + (sizeof(uint8_t) + NLA_HDRLEN) +
		hdd_add_tx_bitrate_sap_get_len() +
		hdd_add_sta_capability_get_len());
}

/**
 * hdd_add_sta_info_sap - add sta signal info attribute
 * @skb: pointer to response skb buffer
 * @stainfo: station information
 * @idx: attribute type index for nla_next_start()
 *
 * This function adds sta signal attribute to response skb buffer
 *
 * Return : 0 on success and errno on failure
 */
static int32_t hdd_add_sta_info_sap(struct sk_buff *skb, int8_t rssi,
				    struct hdd_station_info *stainfo, int idx)
{
	struct nlattr *nla_attr;

	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr)
		goto fail;

	if (nla_put_u8(skb, NL80211_STA_INFO_SIGNAL,
		       rssi - HDD_NOISE_FLOOR_DBM)) {
		hdd_err("put fail");
		goto fail;
	}
	if (hdd_add_tx_bitrate_sap(skb, stainfo, NL80211_STA_INFO_TX_BITRATE))
		goto fail;

	nla_nest_end(skb, nla_attr);
	hdd_nofl_debug("Remote STA RSSI: %d", rssi - HDD_NOISE_FLOOR_DBM);
	return 0;
fail:
	return -EINVAL;
}

/**
 * hdd_add_link_standard_info_sap_get_len - get data length used in
 * hdd_add_link_standard_info_sap()
 *
 * This function calculates the data length used in
 * hdd_add_link_standard_info_sap()
 *
 * Return: total data length used in hdd_add_link_standard_info_sap()
 */
static uint32_t hdd_add_link_standard_info_sap_get_len(void)
{
	return ((NLA_HDRLEN) +
		hdd_add_survey_info_sap_get_len() +
		hdd_add_sta_info_sap_get_len() +
		(sizeof(uint32_t) + NLA_HDRLEN));
}

/**
 * hdd_add_link_standard_info_sap - add add link info attribut
 * @skb: pointer to response skb buffer
 * @stainfo: station information
 * @idx: attribute type index for nla_next_start()
 *
 * This function adds link info attribut to response skb buffer
 *
 * Return : 0 on success and errno on failure
 */
static int hdd_add_link_standard_info_sap(struct sk_buff *skb, int8_t rssi,
					  struct hdd_station_info *stainfo,
					  int idx)
{
	struct nlattr *nla_attr;

	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr)
		goto fail;
	if (hdd_add_survey_info_sap(skb, stainfo, NL80211_ATTR_SURVEY_INFO))
		goto fail;
	if (hdd_add_sta_info_sap(skb, rssi, stainfo, NL80211_ATTR_STA_INFO))
		goto fail;

	if (nla_put_u32(skb, NL80211_ATTR_REASON_CODE, stainfo->reason_code)) {
		hdd_err("Reason code put fail");
		goto fail;
	}
	if (nla_put_u16(skb, NL80211_ATTR_STA_CAPABILITY,
			stainfo->capability)) {
		hdd_err("put fail");
		goto fail;
	}
	nla_nest_end(skb, nla_attr);
	return 0;
fail:
	return -EINVAL;
}

/**
 * hdd_add_ap_standard_info_sap_get_len - get data length used in
 * hdd_add_ap_standard_info_sap()
 * @stainfo: station information
 *
 * This function calculates the data length used in
 * hdd_add_ap_standard_info_sap()
 *
 * Return: total data length used in hdd_add_ap_standard_info_sap()
 */
static uint32_t hdd_add_ap_standard_info_sap_get_len(
				struct hdd_station_info *stainfo)
{
	uint32_t len;

	len = NLA_HDRLEN;
	if (stainfo->vht_present)
		len += (sizeof(stainfo->vht_caps) + NLA_HDRLEN);
	if (stainfo->ht_present)
		len += (sizeof(stainfo->ht_caps) + NLA_HDRLEN);

	return len;
}

/**
 * hdd_add_ap_standard_info_sap - add HT and VHT info attributes
 * @skb: pointer to response skb buffer
 * @stainfo: station information
 * @idx: attribute type index for nla_next_start()
 *
 * This function adds HT and VHT info attributes to response skb buffer
 *
 * Return : 0 on success and errno on failure
 */
static int hdd_add_ap_standard_info_sap(struct sk_buff *skb,
					struct hdd_station_info *stainfo,
					int idx)
{
	struct nlattr *nla_attr;

	nla_attr = nla_nest_start(skb, idx);
	if (!nla_attr)
		goto fail;

	if (stainfo->vht_present) {
		if (nla_put(skb, NL80211_ATTR_VHT_CAPABILITY,
			    sizeof(stainfo->vht_caps),
			    &stainfo->vht_caps)) {
			hdd_err("put fail");
			goto fail;
		}

		hdd_nofl_debug("Remote STA VHT capabilities len:%u",
			       (uint32_t)sizeof(stainfo->vht_caps));
	}
	if (stainfo->ht_present) {
		if (nla_put(skb, NL80211_ATTR_HT_CAPABILITY,
			    sizeof(stainfo->ht_caps),
			    &stainfo->ht_caps)) {
			hdd_err("put fail");
			goto fail;
		}

		hdd_nofl_debug("Remote STA HT capabilities len:%u",
			       (uint32_t)sizeof(stainfo->ht_caps));
	}
	nla_nest_end(skb, nla_attr);
	return 0;
fail:
	return -EINVAL;
}

/**
 * hdd_decode_ch_width - decode channel band width based
 * @ch_width: encoded enum value holding channel band width
 *
 * This function decodes channel band width from the given encoded enum value.
 *
 * Returns: decoded channel band width.
 */
static uint8_t hdd_decode_ch_width(tSirMacHTChannelWidth ch_width)
{
	switch (ch_width) {
	case 0:
		return 20;
	case 1:
		return 40;
	case 2:
		return 80;
	case 3:
	case 4:
		return 160;
	default:
		hdd_debug("invalid enum: %d", ch_width);
		return 20;
	}
}

/**
 * hdd_get_cached_station_remote() - get cached(deleted) peer's info
 * @hdd_ctx: hdd context
 * @adapter: hostapd interface
 * @mac_addr: mac address of requested peer
 *
 * This function collect and indicate the cached(deleted) peer's info
 *
 * Return: 0 on success, otherwise error value
 */

static int hdd_get_cached_station_remote(struct hdd_context *hdd_ctx,
					 struct hdd_adapter *adapter,
					 struct qdf_mac_addr mac_addr)
{
	struct hdd_station_info *stainfo;
	struct sk_buff *skb = NULL;
	uint32_t nl_buf_len = NLMSG_HDRLEN;
	uint8_t channel_width;

	stainfo = hdd_get_sta_info_by_mac(&adapter->cache_sta_info_list,
					  mac_addr.bytes,
					  STA_INFO_GET_CACHED_STATION_REMOTE);

	if (!stainfo) {
		hdd_err("peer " QDF_MAC_ADDR_FMT " not found",
			QDF_MAC_ADDR_REF(mac_addr.bytes));
		return -EINVAL;
	}

	nl_buf_len += hdd_add_link_standard_info_sap_get_len() +
			hdd_add_ap_standard_info_sap_get_len(stainfo) +
			(sizeof(stainfo->dot11_mode) + NLA_HDRLEN) +
			(sizeof(stainfo->ch_width) + NLA_HDRLEN) +
			(sizeof(stainfo->tx_rate) + NLA_HDRLEN) +
			(sizeof(stainfo->rx_rate) + NLA_HDRLEN) +
			(sizeof(stainfo->support_mode) + NLA_HDRLEN) +

			(sizeof(stainfo->rx_mc_bc_cnt) +
			 NLA_HDRLEN) +
			(sizeof(stainfo->rx_retry_cnt) +
			 NLA_HDRLEN);
	if (stainfo->assoc_req_ies.len)
		nl_buf_len += stainfo->assoc_req_ies.len + NLA_HDRLEN;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);
	if (!skb) {
		hdd_put_sta_info_ref(&adapter->cache_sta_info_list,
				     &stainfo, true,
				     STA_INFO_GET_CACHED_STATION_REMOTE);
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	if (hdd_add_link_standard_info_sap(skb, stainfo->rssi, stainfo,
					   LINK_INFO_STANDARD_NL80211_ATTR)) {
		hdd_err("link standard put fail");
		goto fail;
	}

	if (hdd_add_ap_standard_info_sap(skb, stainfo,
					 AP_INFO_STANDARD_NL80211_ATTR)) {
		hdd_err("ap standard put fail");
		goto fail;
	}

	/* upper layer expects decoded channel BW */
	channel_width = hdd_decode_ch_width(stainfo->ch_width);

	if (nla_put_u32(skb, REMOTE_SUPPORTED_MODE,
			stainfo->support_mode) ||
	    nla_put_u8(skb, REMOTE_CH_WIDTH, channel_width)) {
		hdd_err("remote ch put fail");
		goto fail;
	}
	/* Convert the data from kbps to mbps as expected by the user space */
	if (nla_put_u32(skb, REMOTE_LAST_TX_RATE, stainfo->tx_rate / 1000)) {
		hdd_err("tx rate put fail");
		goto fail;
	}
	/* Convert the data from kbps to mbps as expected by the user space */
	if (nla_put_u32(skb, REMOTE_LAST_RX_RATE, stainfo->rx_rate / 1000)) {
		hdd_err("rx rate put fail");
		goto fail;
	}
	if (nla_put_u32(skb, WLAN802_11_MODE, stainfo->dot11_mode)) {
		hdd_err("dot11 mode put fail");
		goto fail;
	}

	if (!(stainfo->rx_mc_bc_cnt & HDD_STATION_INFO_RX_MC_BC_COUNT)) {
		hdd_debug("rx mc bc count is not supported by FW");
	} else if (nla_put_u32(skb, REMOTE_RX_BC_MC_COUNT,
			       (stainfo->rx_mc_bc_cnt &
			       (~HDD_STATION_INFO_RX_MC_BC_COUNT)))) {
		hdd_err("rx mc bc put fail");
		goto fail;
	} else {
		hdd_nofl_debug("Remote STA RX mc_bc_count: %d",
			       (stainfo->rx_mc_bc_cnt &
			       (~HDD_STATION_INFO_RX_MC_BC_COUNT)));
	}

	/* Currently rx_retry count is not supported */
	if (stainfo->rx_retry_cnt) {
		if (nla_put_u32(skb, REMOTE_RX_RETRY_COUNT,
				stainfo->rx_retry_cnt)) {
			hdd_err("rx retry count put fail");
			goto fail;
		}
		hdd_nofl_debug("Remote STA retry count: %d",
			       stainfo->rx_retry_cnt);
	}

	if (stainfo->assoc_req_ies.len) {
		if (nla_put(skb, ASSOC_REQ_IES, stainfo->assoc_req_ies.len,
			    stainfo->assoc_req_ies.data)) {
			hdd_err("Failed to put assoc req IEs");
			goto fail;
		}
		hdd_nofl_debug("Remote STA assoc req IE len: %d",
			       stainfo->assoc_req_ies.len);
	}

	hdd_nofl_debug(
		"Remote STA Info:: freq:%d, RSSI:%d, Tx NSS:%d, Reason code:%d,"
		"capability:0x%x, Supported mode:%d, chan_width:%d, Tx rate:%d,"
		"Rx rate:%d, dot11mode:%d",
		stainfo->freq, stainfo->rssi - HDD_NOISE_FLOOR_DBM,
		stainfo->nss, stainfo->reason_code, stainfo->capability,
		stainfo->support_mode, channel_width, stainfo->tx_rate,
		stainfo->rx_rate, stainfo->dot11_mode);

	hdd_sta_info_detach(&adapter->cache_sta_info_list, &stainfo);
	hdd_put_sta_info_ref(&adapter->cache_sta_info_list, &stainfo, true,
			     STA_INFO_GET_CACHED_STATION_REMOTE);
	qdf_atomic_dec(&adapter->cache_sta_count);

	return cfg80211_vendor_cmd_reply(skb);
fail:
	hdd_put_sta_info_ref(&adapter->cache_sta_info_list, &stainfo, true,
			     STA_INFO_GET_CACHED_STATION_REMOTE);
	if (skb)
		kfree_skb(skb);

	return -EINVAL;
}

/**
 * hdd_get_cached_station_remote() - get connected peer's info
 * @hdd_ctx: hdd context
 * @adapter: hostapd interface
 * @mac_addr: mac address of requested peer
 *
 * This function collect and indicate the connected peer's info
 *
 * Return: 0 on success, otherwise error value
 */
static int hdd_get_connected_station_info(struct hdd_context *hdd_ctx,
					  struct hdd_adapter *adapter,
					  struct qdf_mac_addr mac_addr,
					  struct hdd_station_info *stainfo)
{
	struct sk_buff *skb = NULL;
	uint32_t nl_buf_len;
	struct stats_event *stats;
	bool txrx_rate = false;
	bool value;
	QDF_STATUS status;
	int ret;

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len += (sizeof(stainfo->max_phy_rate) + NLA_HDRLEN) +
		(sizeof(stainfo->tx_packets) + NLA_HDRLEN) +
		(sizeof(stainfo->tx_bytes) + NLA_HDRLEN) +
		(sizeof(stainfo->rx_packets) + NLA_HDRLEN) +
		(sizeof(stainfo->rx_bytes) + NLA_HDRLEN) +
		(sizeof(stainfo->is_qos_enabled) + NLA_HDRLEN) +
		(sizeof(stainfo->mode) + NLA_HDRLEN);

	status = ucfg_mlme_get_sap_get_peer_info(hdd_ctx->psoc, &value);
	if (status != QDF_STATUS_SUCCESS)
		hdd_err("Unable to fetch sap ger peer info");
	if (value) {
		stats = wlan_cfg80211_mc_cp_stats_get_peer_stats(adapter->vdev,
								 mac_addr.bytes,
								 &ret);
		if (ret || !stats) {
			hdd_err("fail to get tx/rx rate");
			wlan_cfg80211_mc_cp_stats_free_stats_event(stats);
		} else {
			txrx_rate = true;
		}
	}

	if (txrx_rate) {
		stainfo->tx_rate = stats->peer_stats_info_ext->tx_rate;
		stainfo->rx_rate = stats->peer_stats_info_ext->rx_rate;
		nl_buf_len += (sizeof(stainfo->tx_rate) + NLA_HDRLEN) +
			(sizeof(stainfo->rx_rate) + NLA_HDRLEN);
		wlan_cfg80211_mc_cp_stats_free_stats_event(stats);
	}

	/* below info is only valid for HT/VHT mode */
	if (stainfo->mode > SIR_SME_PHY_MODE_LEGACY)
		nl_buf_len += (sizeof(stainfo->ampdu) + NLA_HDRLEN) +
			(sizeof(stainfo->tx_stbc) + NLA_HDRLEN) +
			(sizeof(stainfo->rx_stbc) + NLA_HDRLEN) +
			(sizeof(stainfo->ch_width) + NLA_HDRLEN) +
			(sizeof(stainfo->sgi_enable) + NLA_HDRLEN);

	hdd_info("buflen %d hdrlen %d", nl_buf_len, NLMSG_HDRLEN);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
						  nl_buf_len);
	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		goto fail;
	}

	hdd_info("stainfo");
	hdd_info("maxrate %x tx_pkts %x tx_bytes %llx",
		 stainfo->max_phy_rate, stainfo->tx_packets,
		 stainfo->tx_bytes);
	hdd_info("rx_pkts %x rx_bytes %llx mode %x",
		 stainfo->rx_packets, stainfo->rx_bytes,
		 stainfo->mode);
	if (stainfo->mode > SIR_SME_PHY_MODE_LEGACY) {
		hdd_info("ampdu %d tx_stbc %d rx_stbc %d",
			 stainfo->ampdu, stainfo->tx_stbc,
			 stainfo->rx_stbc);
		hdd_info("wmm %d chwidth %d sgi %d",
			 stainfo->is_qos_enabled,
			 stainfo->ch_width,
			 stainfo->sgi_enable);
	}

	if (nla_put_u32(skb, REMOTE_MAX_PHY_RATE, stainfo->max_phy_rate) ||
	    nla_put_u32(skb, REMOTE_TX_PACKETS, stainfo->tx_packets) ||
	    remote_station_put_u64(skb, REMOTE_TX_BYTES, stainfo->tx_bytes) ||
	    nla_put_u32(skb, REMOTE_RX_PACKETS, stainfo->rx_packets) ||
	    remote_station_put_u64(skb, REMOTE_RX_BYTES, stainfo->rx_bytes) ||
	    nla_put_u8(skb, REMOTE_WMM, stainfo->is_qos_enabled) ||
	    nla_put_u8(skb, REMOTE_SUPPORTED_MODE, stainfo->mode)) {
		hdd_err("put fail");
		goto fail;
	}

	if (txrx_rate) {
		if (nla_put_u32(skb, REMOTE_LAST_TX_RATE, stainfo->tx_rate) ||
		    nla_put_u32(skb, REMOTE_LAST_RX_RATE, stainfo->rx_rate)) {
			hdd_err("put fail");
			goto fail;
		} else {
			hdd_info("tx_rate %x rx_rate %x",
				 stainfo->tx_rate, stainfo->rx_rate);
		}
	}

	if (stainfo->mode > SIR_SME_PHY_MODE_LEGACY) {
		if (nla_put_u8(skb, REMOTE_AMPDU, stainfo->ampdu) ||
		    nla_put_u8(skb, REMOTE_TX_STBC, stainfo->tx_stbc) ||
		    nla_put_u8(skb, REMOTE_RX_STBC, stainfo->rx_stbc) ||
		    nla_put_u8(skb, REMOTE_CH_WIDTH, stainfo->ch_width) ||
		    nla_put_u8(skb, REMOTE_SGI_ENABLE, stainfo->sgi_enable)) {
			hdd_err("put fail");
			goto fail;
		}
	}

	return cfg80211_vendor_cmd_reply(skb);

fail:
	if (skb)
		kfree_skb(skb);

	return -EINVAL;
}

/**
 * hdd_get_station_remote() - get remote peer's info
 * @hdd_ctx: hdd context
 * @adapter: hostapd interface
 * @mac_addr: mac address of requested peer
 *
 * This function collect and indicate the remote peer's info
 *
 * Return: 0 on success, otherwise error value
 */
static int hdd_get_station_remote(struct hdd_context *hdd_ctx,
				  struct hdd_adapter *adapter,
				  struct qdf_mac_addr mac_addr)
{
	int status = 0;
	bool is_associated = false;
	struct hdd_station_info *stainfo =
			hdd_get_sta_info_by_mac(
					&adapter->sta_info_list,
					mac_addr.bytes,
					STA_INFO_HDD_GET_STATION_REMOTE);

	if (!stainfo) {
		status = hdd_get_cached_station_remote(hdd_ctx, adapter,
						       mac_addr);
		return status;
	}

	is_associated = hdd_is_peer_associated(adapter, &mac_addr);
	if (!is_associated) {
		status = hdd_get_cached_station_remote(hdd_ctx, adapter,
						       mac_addr);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &stainfo, true,
				     STA_INFO_HDD_GET_STATION_REMOTE);
		return status;
	}

	status = hdd_get_connected_station_info(hdd_ctx, adapter,
						mac_addr, stainfo);
	hdd_put_sta_info_ref(&adapter->sta_info_list, &stainfo, true,
			     STA_INFO_HDD_GET_STATION_REMOTE);
	return status;
}

/**
 * __hdd_cfg80211_get_station_cmd() - Handle get station vendor cmd
 * @wiphy: corestack handler
 * @wdev: wireless device
 * @data: data
 * @data_len: data length
 *
 * Handles QCA_NL80211_VENDOR_SUBCMD_GET_STATION.
 * Validate cmd attributes and send the station info to upper layers.
 *
 * Return: Success(0) or reason code for failure
 */
static int
__hdd_cfg80211_get_station_cmd(struct wiphy *wiphy,
			       struct wireless_dev *wdev,
			       const void *data,
			       int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_GET_STATION_MAX + 1];
	int32_t status;

	hdd_enter_dev(dev);
	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		status = -EPERM;
		goto out;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status != 0)
		goto out;

	status = wlan_cfg80211_nla_parse(tb,
					 QCA_WLAN_VENDOR_ATTR_GET_STATION_MAX,
					 data, data_len,
					 hdd_get_station_policy);
	if (status) {
		hdd_err("Invalid ATTR");
		goto out;
	}

	/* Parse and fetch Command Type*/
	if (tb[STATION_INFO]) {
		status = hdd_get_station_info(hdd_ctx, adapter);
	} else if (tb[STATION_ASSOC_FAIL_REASON]) {
		status = hdd_get_station_assoc_fail(hdd_ctx, adapter);
	} else if (tb[STATION_REMOTE]) {
		struct qdf_mac_addr mac_addr;

		if (adapter->device_mode != QDF_SAP_MODE &&
		    adapter->device_mode != QDF_P2P_GO_MODE) {
			hdd_err("invalid device_mode:%d", adapter->device_mode);
			status = -EINVAL;
			goto out;
		}

		nla_memcpy(mac_addr.bytes, tb[STATION_REMOTE],
			   QDF_MAC_ADDR_SIZE);

		hdd_debug("STATION_REMOTE " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(mac_addr.bytes));

		status = hdd_get_station_remote(hdd_ctx, adapter, mac_addr);
	} else {
		hdd_err("get station info cmd type failed");
		status = -EINVAL;
		goto out;
	}
	hdd_exit();
out:
	return status;
}

int32_t hdd_cfg80211_get_station_cmd(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __hdd_cfg80211_get_station_cmd(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * hdd_get_peer_stats - get peer statistics information
 * @adapter: pointer to adapter
 * @stainfo: station information
 *
 * This function gets peer statistics information. If IPA is
 * enabled the Rx bcast/mcast count is updated in the
 * exception callback invoked by the IPA driver. In case of
 * back pressure the packets may get routed to the sw path and
 * where eventually the peer mcast/bcast pkt counts are updated in
 * dp rx process handling.
 *
 * Return : 0 on success and errno on failure
 */
static int hdd_get_peer_stats(struct hdd_adapter *adapter,
			      struct hdd_station_info *stainfo)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct cdp_peer_stats *peer_stats;
	struct stats_event *stats;
	QDF_STATUS status;
	int i, ret = 0;

	peer_stats = qdf_mem_malloc(sizeof(*peer_stats));
	if (!peer_stats)
		return -ENOMEM;

	status = cdp_host_get_peer_stats(soc, adapter->vdev_id,
					 stainfo->sta_mac.bytes, peer_stats);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("cdp_host_get_peer_stats failed");
		qdf_mem_free(peer_stats);
		return -EINVAL;
	}

	stainfo->rx_retry_cnt = peer_stats->rx.rx_retries;
	if (!ucfg_ipa_is_enabled())
		stainfo->rx_mc_bc_cnt = peer_stats->rx.multicast.num +
					peer_stats->rx.bcast.num;
	else
		stainfo->rx_mc_bc_cnt += peer_stats->rx.multicast.num +
					 peer_stats->rx.bcast.num;

	qdf_mem_free(peer_stats);
	peer_stats = NULL;

	stats = wlan_cfg80211_mc_cp_stats_get_peer_stats(adapter->vdev,
							 stainfo->sta_mac.bytes,
							 &ret);
	if (ret || !stats) {
		wlan_cfg80211_mc_cp_stats_free_stats_event(stats);
		hdd_err("Failed to get peer stats info");
		return -EINVAL;
	}

	stainfo->tx_retry_succeed = stats->peer_stats_info_ext->tx_retries -
				    stats->peer_stats_info_ext->tx_failed;
	/* This host counter is not supported
	 * since currently tx retry is not done in host side
	 */
	stainfo->tx_retry_exhaust = 0;
	stainfo->tx_total_fw = stats->peer_stats_info_ext->tx_packets;
	stainfo->tx_retry_fw = stats->peer_stats_info_ext->tx_retries;
	stainfo->tx_retry_exhaust_fw = stats->peer_stats_info_ext->tx_failed;

	/* Optional, just print logs here */
	if (!stats->num_peer_adv_stats) {
		hdd_debug("Failed to get peer adv stats info");
		stainfo->rx_fcs_count = 0;
	}

	for (i = 0; i < stats->num_peer_adv_stats; i++) {
		if (!qdf_mem_cmp(stainfo->sta_mac.bytes,
				 stats->peer_adv_stats[i].peer_macaddr,
				 QDF_MAC_ADDR_SIZE)) {
			stainfo->rx_fcs_count = stats->peer_adv_stats[i].
								      fcs_count;
			break;
		}
	}

	wlan_cfg80211_mc_cp_stats_free_stats_event(stats);

	return ret;
}

/**
 * hdd_add_peer_stats_get_len - get data length used in
 * hdd_add_peer_stats()
 * @stainfo: station information
 *
 * This function calculates the data length used in
 * hdd_add_peer_stats()
 *
 * Return: total data length used in hdd_add_peer_stats()
 */
static uint32_t
hdd_add_peer_stats_get_len(struct hdd_station_info *stainfo)
{
	return (nla_attr_size(sizeof(stainfo->rx_retry_cnt)) +
		nla_attr_size(sizeof(stainfo->rx_mc_bc_cnt)) +
		nla_attr_size(sizeof(stainfo->tx_retry_succeed)) +
		nla_attr_size(sizeof(stainfo->tx_retry_exhaust)) +
		nla_attr_size(sizeof(stainfo->tx_total_fw)) +
		nla_attr_size(sizeof(stainfo->tx_retry_fw)) +
		nla_attr_size(sizeof(stainfo->tx_retry_exhaust_fw)) +
		nla_attr_size(sizeof(stainfo->rx_fcs_count)));
}

/**
 * hdd_get_pmf_bcn_protect_stats_len() - get pmf bcn protect counters len
 * @adapter: adapter holding valid bcn protect counters
 *
 * This function calculates the data length for valid pmf bcn counters.
 *
 * Return: total data length used in hdd_add_peer_stats()
 */
static uint32_t
hdd_get_pmf_bcn_protect_stats_len(struct hdd_adapter *adapter)
{
	if (!adapter->hdd_stats.bcn_protect_stats.pmf_bcn_stats_valid)
		return 0;

	/* 4 pmf becon protect counters each of 32 bit */
	return nla_total_size(sizeof(uint32_t)) * 4;
}

static uint32_t
hdd_get_connect_fail_reason_code_len(struct hdd_adapter *adapter)
{
	if (adapter->connect_req_status == STATUS_SUCCESS)
		return 0;

	return nla_total_size(sizeof(uint32_t));
}

/**
 * hdd_add_pmf_bcn_protect_stats() - add pmf bcn protect counters in resp
 * @skb: pointer to response skb buffer
 * @adapter: adapter holding valid bcn protect counters
 *
 * This function adds the pmf bcn stats in response.
 *
 * Return: 0 on success
 */
static int hdd_add_pmf_bcn_protect_stats(struct sk_buff *skb,
					 struct hdd_adapter *adapter)
{
	if (!adapter->hdd_stats.bcn_protect_stats.pmf_bcn_stats_valid)
		return 0;

	adapter->hdd_stats.bcn_protect_stats.pmf_bcn_stats_valid = 0;
	if (nla_put_u32(skb, STA_INFO_BIP_MIC_ERROR_COUNT,
			adapter->hdd_stats.bcn_protect_stats.igtk_mic_fail_cnt) ||
	    nla_put_u32(skb, STA_INFO_BIP_REPLAY_COUNT,
			adapter->hdd_stats.bcn_protect_stats.igtk_replay_cnt) ||
	    nla_put_u32(skb, STA_INFO_BEACON_MIC_ERROR_COUNT,
			adapter->hdd_stats.bcn_protect_stats.bcn_mic_fail_cnt) ||
	    nla_put_u32(skb, STA_INFO_BEACON_REPLAY_COUNT,
			adapter->hdd_stats.bcn_protect_stats.bcn_replay_cnt)) {
		hdd_err("put fail");
		return -EINVAL;
	}

	return 0;
}

/**
 * hdd_get_umac_to_osif_connect_fail_reason() - Convert to qca internal connect
 * fail reason
 * @internal_reason: Mac reason code of type @wlan_status_code
 *
 * Check if it is internal status code and convert it to the
 * enum qca_sta_connect_fail_reason_codes.
 *
 * Return: Reason code of type enum qca_sta_connect_fail_reason_codes
 */
static enum qca_sta_connect_fail_reason_codes
hdd_get_umac_to_osif_connect_fail_reason(enum wlan_status_code internal_reason)
{
	enum qca_sta_connect_fail_reason_codes reason = 0;

	if (internal_reason < STATUS_PROP_START)
		return reason;

	switch (internal_reason) {
	case STATUS_NO_NETWORK_FOUND:
		reason = QCA_STA_CONNECT_FAIL_REASON_NO_BSS_FOUND;
		break;
	case STATUS_AUTH_TX_FAIL:
		reason = QCA_STA_CONNECT_FAIL_REASON_AUTH_TX_FAIL;
		break;
	case STATUS_AUTH_NO_ACK_RECEIVED:
		reason = QCA_STA_CONNECT_FAIL_REASON_AUTH_NO_ACK_RECEIVED;
		break;
	case STATUS_AUTH_NO_RESP_RECEIVED:
		reason = QCA_STA_CONNECT_FAIL_REASON_AUTH_NO_RESP_RECEIVED;
		break;
	case STATUS_ASSOC_TX_FAIL:
		reason = QCA_STA_CONNECT_FAIL_REASON_ASSOC_REQ_TX_FAIL;
		break;
	case STATUS_ASSOC_NO_ACK_RECEIVED:
		reason = QCA_STA_CONNECT_FAIL_REASON_ASSOC_NO_ACK_RECEIVED;
		break;
	case STATUS_ASSOC_NO_RESP_RECEIVED:
		reason = QCA_STA_CONNECT_FAIL_REASON_ASSOC_NO_RESP_RECEIVED;
		break;
	default:
		hdd_debug("QCA code not present for internal status code %d",
			  internal_reason);
	}

	return reason;
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
/**
 * hdd_get_big_data_stats_len - get data length used in
 * hdd_big_data_pack_resp_nlmsg()
 * @adapter: hdd adapter
 *
 * This function calculates the data length used in
 * hdd_big_data_pack_resp_nlmsg()
 *
 * Return: total data length used in hdd_big_data_pack_resp_nlmsg()
 */
static uint32_t
hdd_get_big_data_stats_len(struct hdd_adapter *adapter)
{
	uint32_t len;

	len =
	nla_total_size(sizeof(adapter->big_data_stats.last_tx_data_rate_kbps)) +
	nla_total_size(sizeof(adapter->big_data_stats.target_power_ofdm)) +
	nla_total_size(sizeof(adapter->big_data_stats.target_power_dsss)) +
	nla_total_size(sizeof(adapter->big_data_stats.last_tx_data_rix)) +
	nla_total_size(sizeof(adapter->big_data_stats.tsf_out_of_sync)) +
	nla_total_size(sizeof(adapter->big_data_stats.ani_level)) +
	nla_total_size(sizeof(adapter->big_data_stats.last_data_tx_pwr));

	/** Add len of roam params **/
	len += nla_total_size(sizeof(uint32_t)) * 3;

	return len;
}

/**
 * hdd_big_data_pack_resp_nlmsg() - pack big data nl resp msg
 * @skb: pointer to response skb buffer
 * @adapter: adapter holding big data stats
 * @hdd_ctx: hdd context
 *
 * This function adds big data stats in response.
 *
 * Return: 0 on success
 */
static int
hdd_big_data_pack_resp_nlmsg(struct sk_buff *skb,
			     struct hdd_adapter *adapter,
			     struct hdd_context *hdd_ctx)
{
	struct hdd_station_ctx *hdd_sta_ctx;

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	if (!hdd_sta_ctx) {
		hdd_err("Invalid station context");
		return -EINVAL;
	}
	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_RATE,
			adapter->big_data_stats.last_tx_data_rate_kbps)){
		hdd_err("latest tx rate put fail");
		return -EINVAL;
	}

	if (WLAN_REG_IS_5GHZ_CH_FREQ(hdd_sta_ctx->cache_conn_info.chan_freq)) {
		if (nla_put_u32(
			skb,
			QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_5G_6MBPS,
			adapter->big_data_stats.target_power_ofdm)){
			hdd_err("5G ofdm power put fail");
			return -EINVAL;
		}
	} else if (WLAN_REG_IS_24GHZ_CH_FREQ(
				hdd_sta_ctx->cache_conn_info.chan_freq)){
		if (nla_put_u32(
		       skb,
		       QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_6MBPS,
		       adapter->big_data_stats.target_power_ofdm)){
			hdd_err("2.4G ofdm power put fail");
			return -EINVAL;
		}
		if (nla_put_u32(
		       skb,
		       QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_POWER_24G_1MBPS,
		       adapter->big_data_stats.target_power_dsss)){
			hdd_err("target power dsss put fail");
			return -EINVAL;
		}
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_RIX,
			adapter->big_data_stats.last_tx_data_rix)){
		hdd_err("last rix rate put fail");
		return -EINVAL;
	}
	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TSF_OUT_OF_SYNC_COUNT,
			adapter->big_data_stats.tsf_out_of_sync)){
		hdd_err("tsf out of sync put fail");
		return -EINVAL;
	}
	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ANI_LEVEL,
			adapter->big_data_stats.ani_level)){
		hdd_err("ani level put fail");
		return -EINVAL;
	}
	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_LATEST_TX_POWER,
			adapter->big_data_stats.last_data_tx_pwr)){
		hdd_err("last data tx power put fail");
		return -EINVAL;
	}
	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_TRIGGER_REASON,
			wlan_cm_get_roam_states(hdd_ctx->psoc, adapter->vdev_id,
						ROAM_TRIGGER_REASON))){
		hdd_err("roam trigger reason put fail");
		return -EINVAL;
	}
	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_FAIL_REASON,
			wlan_cm_get_roam_states(hdd_ctx->psoc, adapter->vdev_id,
						ROAM_FAIL_REASON))){
		hdd_err("roam fail reason put fail");
		return -EINVAL;
	}
	if (nla_put_u32(
		      skb,
		      QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_ROAM_INVOKE_FAIL_REASON,
		      wlan_cm_get_roam_states(hdd_ctx->psoc, adapter->vdev_id,
					      ROAM_INVOKE_FAIL_REASON))){
		hdd_err("roam invoke fail reason put fail");
		return -EINVAL;
	}

	return 0;
}

/**
 * hdd_reset_roam_params() - reset roam params
 * @psoc: psoc
 * @vdev_id: vdev id
 *
 * This function resets big data roam params
 *
 * Return: None
 */
static void
hdd_reset_roam_params(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{
	wlan_cm_update_roam_states(psoc, vdev_id,
				   0, ROAM_TRIGGER_REASON);
	wlan_cm_update_roam_states(psoc, vdev_id,
				   0, ROAM_FAIL_REASON);
	wlan_cm_update_roam_states(psoc, vdev_id,
				   0, ROAM_INVOKE_FAIL_REASON);
}
#else
static int
hdd_big_data_pack_resp_nlmsg(struct sk_buff *skb,
			     struct hdd_adapter *adapter,
			     struct hdd_context *hdd_ctx)
{
	return 0;
}

static uint32_t
hdd_get_big_data_stats_len(struct hdd_adapter *adapter)
{
	return 0;
}

static void
hdd_reset_roam_params(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id)
{}
#endif

/**
 * hdd_add_connect_fail_reason_code() - Fills connect fail reason code
 * @skb: pointer to skb
 * @adapter: pointer to hdd adapter
 *
 * Return: on success 0 else error code
 */
static int hdd_add_connect_fail_reason_code(struct sk_buff *skb,
					    struct hdd_adapter *adapter)
{
	uint32_t reason;

	reason = hdd_get_umac_to_osif_connect_fail_reason(
					adapter->connect_req_status);
	if (!reason)
		return 0;

	if (nla_put_u32(skb, STA_INFO_CONNECT_FAIL_REASON_CODE, reason)) {
		hdd_err("put fail");
		return -EINVAL;
	}

	return 0;
}

/**
 * hdd_add_peer_stats - add peer statistics information
 * @skb: pointer to response skb buffer
 * @stainfo: station information
 *
 * This function adds peer statistics information to response skb buffer
 *
 * Return : 0 on success and errno on failure
 */
static int hdd_add_peer_stats(struct sk_buff *skb,
			      struct hdd_station_info *stainfo)
{
	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_RETRY_COUNT,
			stainfo->rx_retry_cnt)) {
		hdd_err("Failed to put rx_retry_cnt");
		goto fail;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_BC_MC_COUNT,
			stainfo->rx_mc_bc_cnt)) {
		hdd_err("Failed to put rx_mc_bc_cnt");
		goto fail;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_SUCCEED,
			stainfo->tx_retry_succeed)) {
		hdd_err("Failed to put tx_retry_succeed");
		goto fail;
	}

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TX_RETRY_EXHAUSTED,
			stainfo->tx_retry_exhaust)) {
		hdd_err("Failed to put tx_retry_exhaust");
		goto fail;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_TOTAL,
			stainfo->tx_total_fw)) {
		hdd_err("Failed to put tx_total_fw");
		goto fail;
	}

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY,
			stainfo->tx_retry_fw)) {
		hdd_err("Failed to put tx_retry_fw");
		goto fail;
	}

	if (nla_put_u32(skb,
		    QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_TARGET_TX_RETRY_EXHAUSTED,
		    stainfo->tx_retry_exhaust_fw)) {
		hdd_err("Failed to put tx_retry_exhaust_fw");
		goto fail;
	}

	if (nla_put_u32(skb,
		     QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_RX_FRAMES_CRC_FAIL_COUNT,
		     stainfo->rx_fcs_count)) {
		hdd_err("Failed to put rx_fcs_count");
		goto fail;
	}

	return 0;
fail:
	return -EINVAL;
}

/**
 * hdd_get_connected_station_info_ex() - get connected peer's info
 * @hdd_ctx: hdd context
 * @adapter: hostapd interface
 * @stainfo: pointer to hdd_station_info
 *
 * This function collect and indicate the connected peer's info
 *
 * Return: 0 on success, otherwise error value
 */
static int hdd_get_connected_station_info_ex(struct hdd_context *hdd_ctx,
					     struct hdd_adapter *adapter,
					     struct hdd_station_info *stainfo)
{
	struct sk_buff *skb = NULL;
	uint32_t nl_buf_len, guard_interval;
	bool sap_get_peer_info;
	struct nl80211_sta_flag_update sta_flags = {0};
	QDF_STATUS status;

	if (hdd_get_peer_stats(adapter, stainfo)) {
		hdd_err_rl("hdd_get_peer_stats fail");
		return -EINVAL;
	}

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len += nla_attr_size(QDF_MAC_ADDR_SIZE);

	status = ucfg_mlme_get_sap_get_peer_info(hdd_ctx->psoc,
						 &sap_get_peer_info);
	if (status != QDF_STATUS_SUCCESS)
		hdd_err_rl("Unable to fetch sap ger peer info");

	if (sap_get_peer_info)
		nl_buf_len += hdd_add_peer_stats_get_len(stainfo);

	if (stainfo->mode > SIR_SME_PHY_MODE_LEGACY)
		nl_buf_len += nla_attr_size(sizeof(sta_flags)) +
			      nla_attr_size(sizeof(guard_interval));

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);
	if (!skb) {
		hdd_err_rl("cfg80211_vendor_cmd_alloc_reply_skb failed");
		goto fail;
	}

	if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC,
		    QDF_MAC_ADDR_SIZE, stainfo->sta_mac.bytes)) {
		hdd_err_rl("Failed to put MAC address");
		goto fail;
	}

	if (sap_get_peer_info && hdd_add_peer_stats(skb, stainfo)) {
		hdd_err_rl("hdd_add_peer_stats fail");
		goto fail;
	}

	if (stainfo->mode > SIR_SME_PHY_MODE_LEGACY) {
		sta_flags.mask = QCA_VENDOR_WLAN_STA_FLAG_AMPDU |
				 QCA_VENDOR_WLAN_STA_FLAG_TX_STBC |
				 QCA_VENDOR_WLAN_STA_FLAG_RX_STBC;

		if (stainfo->ampdu)
			sta_flags.set |= QCA_VENDOR_WLAN_STA_FLAG_AMPDU;
		if (stainfo->tx_stbc)
			sta_flags.set |= QCA_VENDOR_WLAN_STA_FLAG_TX_STBC;
		if (stainfo->rx_stbc)
			sta_flags.set |= QCA_VENDOR_WLAN_STA_FLAG_RX_STBC;

		if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_FLAGS,
			    sizeof(sta_flags), &sta_flags)) {
			hdd_err_rl("Failed to put STA flags");
			goto fail;
		}

		switch (stainfo->guard_interval) {
		case TXRATE_GI_0_8_US:
			guard_interval = QCA_VENDOR_WLAN_STA_GI_800_NS;
			break;
		case TXRATE_GI_0_4_US:
			guard_interval = QCA_VENDOR_WLAN_STA_GI_400_NS;
			break;
		case TXRATE_GI_1_6_US:
			guard_interval = QCA_VENDOR_WLAN_STA_GI_1600_NS;
			break;
		case TXRATE_GI_3_2_US:
			guard_interval = QCA_VENDOR_WLAN_STA_GI_3200_NS;
			break;
		default:
			hdd_err_rl("Invalid guard_interval %d",
				   stainfo->guard_interval);
			goto fail;
		}

		if (nla_put_u32(skb,
			       QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_GUARD_INTERVAL,
			       guard_interval)) {
			hdd_err_rl("Failed to put guard_interval");
			goto fail;
		}
	}

	return cfg80211_vendor_cmd_reply(skb);

fail:
	if (skb)
		kfree_skb(skb);

	return -EINVAL;
}

/**
 * hdd_get_station_remote_ex() - get remote peer's info, for SAP/GO mode only
 * @hdd_ctx: hdd context
 * @adapter: hostapd interface
 * @mac_addr: mac address of requested peer
 *
 * This function collect and indicate the remote peer's info
 *
 * Return: 0 on success, otherwise error value
 */
static int hdd_get_station_remote_ex(struct hdd_context *hdd_ctx,
				     struct hdd_adapter *adapter,
				     struct qdf_mac_addr mac_addr)
{
	bool is_associated = false;
	struct hdd_station_info *stainfo =
				hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					       mac_addr.bytes,
					       STA_INFO_HDD_GET_STATION_REMOTE);
	int status;

	/* For now, only connected STAs are supported */
	if (!stainfo) {
		hdd_err_rl("Failed to get peer STA " QDF_MAC_ADDR_FMT,
			   QDF_MAC_ADDR_REF(mac_addr.bytes));
		return -ENXIO;
	}

	is_associated = hdd_is_peer_associated(adapter, &mac_addr);
	if (!is_associated) {
		hdd_err_rl("Peer STA is not associated " QDF_MAC_ADDR_FMT,
			   QDF_MAC_ADDR_REF(mac_addr.bytes));
		hdd_put_sta_info_ref(&adapter->sta_info_list, &stainfo, true,
				     STA_INFO_HDD_GET_STATION_REMOTE);
		return -EINVAL;
	}

	status = hdd_get_connected_station_info_ex(hdd_ctx, adapter, stainfo);
	hdd_put_sta_info_ref(&adapter->sta_info_list, &stainfo, true,
			     STA_INFO_HDD_GET_STATION_REMOTE);

	return status;
}

/**
 * hdd_get_station_info_ex() - send STA info to userspace, for STA mode only
 * @hdd_ctx: pointer to hdd context
 * @adapter: pointer to adapter
 *
 * Return: 0 if success else error status
 */
static int hdd_get_station_info_ex(struct hdd_context *hdd_ctx,
				   struct hdd_adapter *adapter)
{
	struct sk_buff *skb;
	uint32_t nl_buf_len = 0, connect_fail_rsn_len;
	struct hdd_station_ctx *hdd_sta_ctx;
	bool big_data_stats_req = false;
	int ret;

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	if (!hdd_conn_is_connected(hdd_sta_ctx))
		big_data_stats_req = true;

	if (wlan_hdd_get_station_stats(adapter))
		hdd_err_rl("wlan_hdd_get_station_stats fail");

	if (big_data_stats_req) {
		if (wlan_hdd_get_big_data_station_stats(adapter)) {
			hdd_err_rl("wlan_hdd_get_big_data_station_stats fail");
			return -EINVAL;
		}
		nl_buf_len = hdd_get_big_data_stats_len(adapter);
	}

	nl_buf_len += hdd_get_pmf_bcn_protect_stats_len(adapter);
	connect_fail_rsn_len = hdd_get_connect_fail_reason_code_len(adapter);
	nl_buf_len += connect_fail_rsn_len;

	if (!nl_buf_len) {
		hdd_err_rl("Failed to get bcn pmf stats");
		return -EINVAL;
	}

	nl_buf_len += NLMSG_HDRLEN;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);
	if (!skb) {
		hdd_err_rl("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	if (hdd_add_pmf_bcn_protect_stats(skb, adapter)) {
		hdd_err_rl("hdd_add_pmf_bcn_protect_stats fail");
		kfree_skb(skb);
		return -EINVAL;
	}

	if (connect_fail_rsn_len) {
		if (hdd_add_connect_fail_reason_code(skb, adapter)) {
			hdd_err_rl("hdd_add_connect_fail_reason_code fail");
			return -ENOMEM;
		}
	}

	if (big_data_stats_req) {
		if (hdd_big_data_pack_resp_nlmsg(skb, adapter, hdd_ctx)) {
			kfree_skb(skb);
			return -EINVAL;
		}
	}

	ret = cfg80211_vendor_cmd_reply(skb);
	hdd_reset_roam_params(hdd_ctx->psoc, adapter->vdev_id);
	return ret;
}

/**
 * __hdd_cfg80211_get_sta_info_cmd() - Handle get sta info vendor cmd
 * @wiphy: pointer to wireless phy
 * @wdev: wireless device
 * @data: data
 * @data_len: data length
 *
 * Handles QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO.
 * Validate cmd attributes and send the station info to upper layers.
 *
 * Return: Success(0) or reason code for failure
 */
static int
__hdd_cfg80211_get_sta_info_cmd(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data,
				int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX + 1];
	struct qdf_mac_addr mac_addr;
	int32_t status;

	hdd_enter_dev(dev);

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE ||
	    hdd_get_conparam() == QDF_GLOBAL_MONITOR_MODE) {
		hdd_err_rl("Command not allowed in FTM / Monitor mode");
		status = -EPERM;
		goto out;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status != 0)
		goto out;

	status = wlan_cfg80211_nla_parse(tb,
					 QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAX,
					 data, data_len,
					 hdd_get_sta_policy);
	if (status) {
		hdd_err_rl("Invalid ATTR");
		goto out;
	}

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
		status = hdd_get_station_info_ex(hdd_ctx, adapter);
		break;
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
		if (!tb[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC]) {
			hdd_err_rl("MAC address is not present");
			status = -EINVAL;
			goto out;
		}

		nla_memcpy(mac_addr.bytes,
			   tb[QCA_WLAN_VENDOR_ATTR_GET_STA_INFO_MAC],
			   QDF_MAC_ADDR_SIZE);
		hdd_debug("STA " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(mac_addr.bytes));
		status = hdd_get_station_remote_ex(hdd_ctx, adapter, mac_addr);
		break;
	default:
		hdd_err_rl("Invalid device_mode: %d", adapter->device_mode);
		status = -EINVAL;
		goto out;
	}

	hdd_exit();
out:
	return status;
}

int32_t hdd_cfg80211_get_sta_info_cmd(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = wlan_hdd_qmi_get_sync_resume();
	if (errno) {
		hdd_err("qmi sync resume failed: %d", errno);
		goto end;
	}

	errno = __hdd_cfg80211_get_sta_info_cmd(wiphy, wdev, data, data_len);

	wlan_hdd_qmi_put_suspend();

end:
	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

