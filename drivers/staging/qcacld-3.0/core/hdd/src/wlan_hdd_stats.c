/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC : wlan_hdd_stats.c
 *
 * WLAN Host Device Driver statistics related implementation
 *
 */

#include "wlan_hdd_stats.h"
#include "sme_api.h"
#include "cds_sched.h"
#include "osif_sync.h"
#include "wlan_hdd_trace.h"
#include "wlan_hdd_lpass.h"
#include "hif.h"
#include <qca_vendor.h>
#include "wma_api.h"
#include "wlan_hdd_hostapd.h"
#include "wlan_osif_request_manager.h"
#include "wlan_hdd_debugfs_llstat.h"
#include "wlan_hdd_debugfs_mibstat.h"
#include "wlan_reg_services_api.h"
#include <wlan_cfg80211_mc_cp_stats.h>
#include "wlan_cp_stats_mc_ucfg_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_mlme_ucfg_api.h"
#include "wlan_hdd_sta_info.h"
#include "cdp_txrx_misc.h"
#include "cdp_txrx_host_stats.h"
#include "wlan_hdd_object_manager.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)) && !defined(WITH_BACKPORTS)
#define HDD_INFO_SIGNAL                 STATION_INFO_SIGNAL
#define HDD_INFO_SIGNAL_AVG             STATION_INFO_SIGNAL_AVG
#define HDD_INFO_TX_PACKETS             STATION_INFO_TX_PACKETS
#define HDD_INFO_TX_RETRIES             STATION_INFO_TX_RETRIES
#define HDD_INFO_TX_FAILED              STATION_INFO_TX_FAILED
#define HDD_INFO_TX_BITRATE             STATION_INFO_TX_BITRATE
#define HDD_INFO_RX_BITRATE             STATION_INFO_RX_BITRATE
#define HDD_INFO_TX_BYTES               STATION_INFO_TX_BYTES
#define HDD_INFO_CHAIN_SIGNAL_AVG       STATION_INFO_CHAIN_SIGNAL_AVG
#define HDD_INFO_EXPECTED_THROUGHPUT    0
#define HDD_INFO_RX_BYTES               STATION_INFO_RX_BYTES
#define HDD_INFO_RX_PACKETS             STATION_INFO_RX_PACKETS
#define HDD_INFO_TX_BYTES64             0
#define HDD_INFO_RX_BYTES64             0
#define HDD_INFO_INACTIVE_TIME          0
#define HDD_INFO_CONNECTED_TIME         0
#define HDD_INFO_STA_FLAGS              0
#define HDD_INFO_RX_MPDUS               0
#define HDD_INFO_FCS_ERROR_COUNT        0
#else
#define HDD_INFO_SIGNAL                 BIT(NL80211_STA_INFO_SIGNAL)
#define HDD_INFO_SIGNAL_AVG             BIT(NL80211_STA_INFO_SIGNAL_AVG)
#define HDD_INFO_TX_PACKETS             BIT(NL80211_STA_INFO_TX_PACKETS)
#define HDD_INFO_TX_RETRIES             BIT(NL80211_STA_INFO_TX_RETRIES)
#define HDD_INFO_TX_FAILED              BIT(NL80211_STA_INFO_TX_FAILED)
#define HDD_INFO_TX_BITRATE             BIT(NL80211_STA_INFO_TX_BITRATE)
#define HDD_INFO_RX_BITRATE             BIT(NL80211_STA_INFO_RX_BITRATE)
#define HDD_INFO_TX_BYTES               BIT(NL80211_STA_INFO_TX_BYTES)
#define HDD_INFO_CHAIN_SIGNAL_AVG       BIT(NL80211_STA_INFO_CHAIN_SIGNAL_AVG)
#define HDD_INFO_EXPECTED_THROUGHPUT  BIT(NL80211_STA_INFO_EXPECTED_THROUGHPUT)
#define HDD_INFO_RX_BYTES               BIT(NL80211_STA_INFO_RX_BYTES)
#define HDD_INFO_RX_PACKETS             BIT(NL80211_STA_INFO_RX_PACKETS)
#define HDD_INFO_TX_BYTES64             BIT(NL80211_STA_INFO_TX_BYTES64)
#define HDD_INFO_RX_BYTES64             BIT(NL80211_STA_INFO_RX_BYTES64)
#define HDD_INFO_INACTIVE_TIME          BIT(NL80211_STA_INFO_INACTIVE_TIME)
#define HDD_INFO_CONNECTED_TIME         BIT(NL80211_STA_INFO_CONNECTED_TIME)
#define HDD_INFO_STA_FLAGS              BIT(NL80211_STA_INFO_STA_FLAGS)
#define HDD_INFO_RX_MPDUS             BIT_ULL(NL80211_STA_INFO_RX_MPDUS)
#define HDD_INFO_FCS_ERROR_COUNT      BIT_ULL(NL80211_STA_INFO_FCS_ERROR_COUNT)
#endif /* kernel version less than 4.0.0 && no_backport */

#define HDD_LINK_STATS_MAX		5

/* 11B, 11G Rate table include Basic rate and Extended rate
 * The IDX field is the rate index
 * The HI field is the rate when RSSI is strong or being ignored
 *  (in this case we report actual rate)
 * The MID field is the rate when RSSI is moderate
 * (in this case we cap 11b rates at 5.5 and 11g rates at 24)
 * The LO field is the rate when RSSI is low
 *  (in this case we don't report rates, actual current rate used)
 */
static const struct index_data_rate_type supported_data_rate[] = {
	/* IDX     HI  HM  LM LO (RSSI-based index */
	{2,   { 10,  10, 10, 0} },
	{4,   { 20,  20, 10, 0} },
	{11,  { 55,  20, 10, 0} },
	{12,  { 60,  55, 20, 0} },
	{18,  { 90,  55, 20, 0} },
	{22,  {110,  55, 20, 0} },
	{24,  {120,  90, 60, 0} },
	{36,  {180, 120, 60, 0} },
	{44,  {220, 180, 60, 0} },
	{48,  {240, 180, 90, 0} },
	{66,  {330, 180, 90, 0} },
	{72,  {360, 240, 90, 0} },
	{96,  {480, 240, 120, 0} },
	{108, {540, 240, 120, 0} }
};
/* MCS Based rate table HT MCS parameters with Nss = 1 */
static struct index_data_rate_type supported_mcs_rate_nss1[] = {
/* MCS  L20   L40   S20  S40 */
	{0, {65, 135, 72, 150} },
	{1, {130, 270, 144, 300} },
	{2, {195, 405, 217, 450} },
	{3, {260, 540, 289, 600} },
	{4, {390, 810, 433, 900} },
	{5, {520, 1080, 578, 1200} },
	{6, {585, 1215, 650, 1350} },
	{7, {650, 1350, 722, 1500} }
};

/* HT MCS parameters with Nss = 2 */
static struct index_data_rate_type supported_mcs_rate_nss2[] = {
/* MCS  L20    L40   S20   S40 */
	{0, {130, 270, 144, 300} },
	{1, {260, 540, 289, 600} },
	{2, {390, 810, 433, 900} },
	{3, {520, 1080, 578, 1200} },
	{4, {780, 1620, 867, 1800} },
	{5, {1040, 2160, 1156, 2400} },
	{6, {1170, 2430, 1300, 2700} },
	{7, {1300, 2700, 1444, 3000} }
};

/* MCS Based VHT rate table MCS parameters with Nss = 1*/
static struct index_vht_data_rate_type supported_vht_mcs_rate_nss1[] = {
/* MCS  L80    S80     L40   S40    L20   S40*/
	{0, {293, 325}, {135, 150}, {65, 72} },
	{1, {585, 650}, {270, 300}, {130, 144} },
	{2, {878, 975}, {405, 450}, {195, 217} },
	{3, {1170, 1300}, {540, 600}, {260, 289} },
	{4, {1755, 1950}, {810, 900}, {390, 433} },
	{5, {2340, 2600}, {1080, 1200}, {520, 578} },
	{6, {2633, 2925}, {1215, 1350}, {585, 650} },
	{7, {2925, 3250}, {1350, 1500}, {650, 722} },
	{8, {3510, 3900}, {1620, 1800}, {780, 867} },
	{9, {3900, 4333}, {1800, 2000}, {780, 867} }
};

/*MCS parameters with Nss = 2*/
static struct index_vht_data_rate_type supported_vht_mcs_rate_nss2[] = {
/* MCS  L80    S80     L40   S40    L20   S40*/
	{0, {585, 650}, {270, 300}, {130, 144} },
	{1, {1170, 1300}, {540, 600}, {260, 289} },
	{2, {1755, 1950}, {810, 900}, {390, 433} },
	{3, {2340, 2600}, {1080, 1200}, {520, 578} },
	{4, {3510, 3900}, {1620, 1800}, {780, 867} },
	{5, {4680, 5200}, {2160, 2400}, {1040, 1156} },
	{6, {5265, 5850}, {2430, 2700}, {1170, 1300} },
	{7, {5850, 6500}, {2700, 3000}, {1300, 1444} },
	{8, {7020, 7800}, {3240, 3600}, {1560, 1733} },
	{9, {7800, 8667}, {3600, 4000}, {1730, 1920} }
};

/*array index points to MCS and array value points respective rssi*/
static int rssi_mcs_tbl[][14] = {
/*  MCS 0   1    2   3    4    5    6    7    8    9    10   11   12   13*/
	/* 20 */
	{-82, -79, -77, -74, -70, -66, -65, -64, -59, -57, -52, -48, -46, -42},
	/* 40 */
	{-79, -76, -74, -71, -67, -63, -62, -61, -56, -54, -49, -45, -43, -39},
	/* 80 */
	{-76, -73, -71, -68, -64, -60, -59, -58, -53, -51, -46, -42, -46, -36}
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static bool wlan_hdd_is_he_mcs_12_13_supported(uint16_t he_mcs_12_13_map)
{
	if (he_mcs_12_13_map)
		return true;
	else
		return false;
}
#else
static bool wlan_hdd_is_he_mcs_12_13_supported(uint16_t he_mcs_12_13_map)
{
	return false;
}
#endif

static bool get_station_fw_request_needed = true;

#ifdef WLAN_FEATURE_BIG_DATA_STATS
/*
 * copy_station_big_data_stats_to_adapter() - Copy big data stats to adapter
 * @adapter: Pointer to the adapter
 * @stats: Pointer to the big data stats event
 *
 * Return: 0 if success, non-zero for failure
 */
static void copy_station_big_data_stats_to_adapter(
					struct hdd_adapter *adapter,
					struct big_data_stats_event *stats)
{
	adapter->big_data_stats.vdev_id = stats->vdev_id;
	adapter->big_data_stats.tsf_out_of_sync = stats->tsf_out_of_sync;
	adapter->big_data_stats.ani_level = stats->ani_level;
	adapter->big_data_stats.last_data_tx_pwr =
					stats->last_data_tx_pwr;
	adapter->big_data_stats.target_power_dsss =
					stats->target_power_dsss;
	adapter->big_data_stats.target_power_ofdm =
					stats->target_power_ofdm;
	adapter->big_data_stats.last_tx_data_rix = stats->last_tx_data_rix;
	adapter->big_data_stats.last_tx_data_rate_kbps =
					stats->last_tx_data_rate_kbps;
}
#endif

#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
static void
hdd_update_station_stats_cached_timestamp(struct hdd_adapter *adapter)
{
	adapter->hdd_stats.sta_stats_cached_timestamp =
				qdf_system_ticks_to_msecs(qdf_system_ticks());
}
#else
static void
hdd_update_station_stats_cached_timestamp(struct hdd_adapter *adapter)
{
}
#endif /* FEATURE_CLUB_LL_STATS_AND_GET_STATION */

#ifdef WLAN_FEATURE_WMI_SEND_RECV_QMI
/**
 * wlan_hdd_qmi_get_sync_resume() - Get operation to trigger RTPM
 * sync resume without WoW exit
 *
 * call qmi_get before sending qmi, and do qmi_put after all the
 * qmi response rececived from fw. so this request wlan host to
 * wait for the last qmi response, if it doesn't wait, qmi put
 * which cause MHI enter M3(suspend) before all the qmi response,
 * and MHI will trigger a RTPM resume, this violated design of by
 * sending cmd by qmi without wow resume.
 *
 * Returns: 0 for success, non-zero for failure
 */
int wlan_hdd_qmi_get_sync_resume(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (!hdd_ctx->config->is_qmi_stats_enabled) {
		hdd_debug("periodic stats over qmi is disabled");
		return 0;
	}

	if (!qdf_ctx) {
		hdd_err("qdf_ctx is null");
		return -EINVAL;
	}

	return pld_qmi_send_get(qdf_ctx->dev);
}

/**
 * wlan_hdd_qmi_put_suspend() - Put operation to trigger RTPM suspend
 * without WoW entry
 *
 * Returns: 0 for success, non-zero for failure
 */
int wlan_hdd_qmi_put_suspend(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (wlan_hdd_validate_context(hdd_ctx))
		return -EINVAL;

	if (!hdd_ctx->config->is_qmi_stats_enabled) {
		hdd_debug("periodic stats over qmi is disabled");
		return 0;
	}

	if (!qdf_ctx) {
		hdd_err("qdf_ctx is null");
		return -EINVAL;
	}

	return pld_qmi_send_put(qdf_ctx->dev);
}
#else
int wlan_hdd_qmi_get_sync_resume(void)
{
	return 0;
}

int wlan_hdd_qmi_put_suspend(void)
{
	return 0;
}
#endif /* end if of WLAN_FEATURE_WMI_SEND_RECV_QMI */

#ifdef WLAN_FEATURE_LINK_LAYER_STATS

/**
 * struct hdd_ll_stats - buffered hdd link layer stats
 * @ll_stats_node: pointer to next stats buffered in scheduler thread context
 * @result_param_id: Received link layer stats ID
 * @result: received stats from FW
 * @more_data: if more stats are pending
 * @no_of_radios: no of radios
 * @no_of_peers: no of peers
 */
struct hdd_ll_stats {
	qdf_list_node_t ll_stats_node;
	u32 result_param_id;
	void *result;
	u32 more_data;
	union {
		u32 no_of_radios;
		u32 no_of_peers;
	} stats_nradio_npeer;
};

/**
 * struct hdd_ll_stats_priv - hdd link layer stats private
 * @ll_stats: head to different link layer stats received in scheduler
 *            thread context
 * @request_id: userspace-assigned link layer stats request id
 * @request_bitmap: userspace-assigned link layer stats request bitmap
 * @ll_stats_lock: Lock to serially access request_bitmap
 * @vdev_id: id of vdev handle
 */
struct hdd_ll_stats_priv {
	qdf_list_t ll_stats_q;
	uint32_t request_id;
	uint32_t request_bitmap;
	qdf_spinlock_t ll_stats_lock;
	uint8_t vdev_id;
};

/*
 * Used to allocate the size of 4096 for the link layer stats.
 * The size of 4096 is considered assuming that all data per
 * respective event fit with in the limit.Please take a call
 * on the limit based on the data requirements on link layer
 * statistics.
 */
#define LL_STATS_EVENT_BUF_SIZE 4096

/**
 * put_wifi_rate_stat() - put wifi rate stats
 * @stats: Pointer to stats context
 * @vendor_event: Pointer to vendor event
 *
 * Return: bool
 */
static bool put_wifi_rate_stat(struct wifi_rate_stat *stats,
			       struct sk_buff *vendor_event)
{
	if (nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_PREAMBLE,
		       stats->rate.preamble) ||
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_NSS,
		       stats->rate.nss) ||
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BW,
		       stats->rate.bw) ||
	    nla_put_u8(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MCS_INDEX,
		       stats->rate.rate_or_mcs_index) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_BIT_RATE,
			stats->rate.bitrate) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_TX_MPDU,
			stats->tx_mpdu) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RX_MPDU,
			stats->rx_mpdu) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_MPDU_LOST,
			stats->mpdu_lost) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES,
			stats->retries) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_SHORT,
			stats->retries_short) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RATE_RETRIES_LONG,
			stats->retries_long)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		return false;
	}

	return true;
}

/**
 * put_wifi_peer_rates() - put wifi peer rate info
 * @stats: Pointer to stats context
 * @vendor_event: Pointer to vendor event
 *
 * Return: bool
 */
static bool put_wifi_peer_rates(struct wifi_peer_info *stats,
				struct sk_buff *vendor_event)
{
	uint32_t i;
	struct wifi_rate_stat *rate_stat;
	int nest_id;
	struct nlattr *info;
	struct nlattr *rates;

	/* no rates is ok */
	if (!stats->num_rate)
		return true;

	nest_id = QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_RATE_INFO;
	info = nla_nest_start(vendor_event, nest_id);
	if (!info)
		return false;

	for (i = 0; i < stats->num_rate; i++) {
		rates = nla_nest_start(vendor_event, i);
		if (!rates)
			return false;
		rate_stat = &stats->rate_stats[i];
		if (!put_wifi_rate_stat(rate_stat, vendor_event)) {
			hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
			return false;
		}
		nla_nest_end(vendor_event, rates);
	}
	nla_nest_end(vendor_event, info);

	return true;
}

/**
 * put_wifi_peer_info() - put wifi peer info
 * @stats: Pointer to stats context
 * @vendor_event: Pointer to vendor event
 *
 * Return: bool
 */
static bool put_wifi_peer_info(struct wifi_peer_info *stats,
			       struct sk_buff *vendor_event)
{
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_TYPE,
			wmi_to_sir_peer_type(stats->type)) ||
	    nla_put(vendor_event,
		       QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_MAC_ADDRESS,
		       QDF_MAC_ADDR_SIZE, &stats->peer_macaddr.bytes[0]) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_CAPABILITIES,
			stats->capabilities) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO_NUM_RATES,
			stats->num_rate)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		return false;
	}

	return put_wifi_peer_rates(stats, vendor_event);
}

/**
 * put_wifi_wmm_ac_stat() - put wifi wmm ac stats
 * @stats: Pointer to stats context
 * @vendor_event: Pointer to vendor event
 *
 * Return: bool
 */
static bool put_wifi_wmm_ac_stat(wmi_wmm_ac_stats *stats,
				 struct sk_buff *vendor_event)
{
	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_AC,
			stats->ac_type) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MPDU,
			stats->tx_mpdu) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MPDU,
			stats->rx_mpdu) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_MCAST,
			stats->tx_mcast) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_MCAST,
			stats->rx_mcast) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RX_AMPDU,
			stats->rx_ampdu) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_TX_AMPDU,
			stats->tx_ampdu) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_MPDU_LOST,
			stats->mpdu_lost) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES,
			stats->retries) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_SHORT,
			stats->retries_short) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_RETRIES_LONG,
			stats->retries_long) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MIN,
			stats->contention_time_min) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_MAX,
			stats->contention_time_max) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_TIME_AVG,
			stats->contention_time_avg) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_AC_CONTENTION_NUM_SAMPLES,
			stats->contention_num_samples)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		return false;
	}

	return true;
}

/**
 * put_wifi_interface_info() - put wifi interface info
 * @stats: Pointer to stats context
 * @vendor_event: Pointer to vendor event
 *
 * Return: bool
 */
static bool put_wifi_interface_info(struct wifi_interface_info *stats,
				    struct sk_buff *vendor_event)
{
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MODE,
			stats->mode) ||
	    nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_MAC_ADDR,
		    QDF_MAC_ADDR_SIZE, stats->macAddr.bytes) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_STATE,
			stats->state) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_ROAMING,
			stats->roaming) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_CAPABILITIES,
			stats->capabilities) ||
	    nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_SSID,
		    strlen(stats->ssid), stats->ssid) ||
	    nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_BSSID,
		    QDF_MAC_ADDR_SIZE, stats->bssid.bytes) ||
	    nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_AP_COUNTRY_STR,
		    REG_ALPHA2_LEN + 1, stats->apCountryStr) ||
	    nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_INFO_COUNTRY_STR,
		    REG_ALPHA2_LEN + 1, stats->countryStr)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		return false;
	}

	return true;
}

/**
 * put_wifi_iface_stats() - put wifi interface stats
 * @if_stat: Pointer to interface stats context
 * @num_peer: Number of peers
 * @vendor_event: Pointer to vendor event
 *
 * Return: bool
 */
static bool put_wifi_iface_stats(struct wifi_interface_stats *if_stat,
				 u32 num_peers, struct sk_buff *vendor_event)
{
	int i = 0;
	struct nlattr *wmm_info;
	struct nlattr *wmm_stats;
	u64 average_tsf_offset;
	wmi_iface_link_stats *link_stats = &if_stat->link_stats;

	if (!put_wifi_interface_info(&if_stat->info, vendor_event)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		return false;

	}

	average_tsf_offset =  link_stats->avg_bcn_spread_offset_high;
	average_tsf_offset =  (average_tsf_offset << 32) |
		link_stats->avg_bcn_spread_offset_low;

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE_IFACE) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS,
			num_peers) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_BEACON_RX,
			link_stats->beacon_rx) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_RX,
			link_stats->mgmt_rx) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_RX,
			link_stats->mgmt_action_rx) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_MGMT_ACTION_TX,
			link_stats->mgmt_action_tx) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_MGMT,
			link_stats->rssi_mgmt) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_DATA,
			link_stats->rssi_data) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RSSI_ACK,
			link_stats->rssi_ack) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_DETECTED,
			link_stats->is_leaky_ap) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_AVG_NUM_FRAMES_LEAKED,
			link_stats->avg_rx_frms_leaked) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_LEAKY_AP_GUARD_TIME,
			link_stats->rx_leak_window) ||
	    hdd_wlan_nla_put_u64(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_AVERAGE_TSF_OFFSET,
			average_tsf_offset) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RTS_SUCC_CNT,
			if_stat->rts_succ_cnt) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_RTS_FAIL_CNT,
			if_stat->rts_fail_cnt) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_PPDU_SUCC_CNT,
			if_stat->ppdu_succ_cnt) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_PPDU_FAIL_CNT,
			if_stat->ppdu_fail_cnt)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		return false;
	}

	wmm_info = nla_nest_start(vendor_event,
				  QCA_WLAN_VENDOR_ATTR_LL_STATS_WMM_INFO);
	if (!wmm_info)
		return false;

	for (i = 0; i < WIFI_AC_MAX; i++) {
		wmm_stats = nla_nest_start(vendor_event, i);
		if (!wmm_stats)
			return false;

		if (!put_wifi_wmm_ac_stat(&if_stat->ac_stats[i],
					  vendor_event)) {
			hdd_err("put_wifi_wmm_ac_stat Fail");
			return false;
		}

		nla_nest_end(vendor_event, wmm_stats);
	}
	nla_nest_end(vendor_event, wmm_info);
	return true;
}

/**
 * hdd_map_device_to_ll_iface_mode() - map device to link layer interface mode
 * @device_mode: Device mode
 *
 * Return: interface mode
 */
static tSirWifiInterfaceMode hdd_map_device_to_ll_iface_mode(int device_mode)
{
	switch (device_mode) {
	case QDF_STA_MODE:
		return WIFI_INTERFACE_STA;
	case QDF_SAP_MODE:
		return WIFI_INTERFACE_SOFTAP;
	case QDF_P2P_CLIENT_MODE:
		return WIFI_INTERFACE_P2P_CLIENT;
	case QDF_P2P_GO_MODE:
		return WIFI_INTERFACE_P2P_GO;
	default:
		/* Return Interface Mode as STA for all the unsupported modes */
		return WIFI_INTERFACE_STA;
	}
}

bool hdd_get_interface_info(struct hdd_adapter *adapter,
			    struct wifi_interface_info *info)
{
	struct hdd_station_ctx *sta_ctx;

	info->mode = hdd_map_device_to_ll_iface_mode(adapter->device_mode);

	qdf_copy_macaddr(&info->macAddr, &adapter->mac_addr);

	if (((QDF_STA_MODE == adapter->device_mode) ||
	     (QDF_P2P_CLIENT_MODE == adapter->device_mode) ||
	     (QDF_P2P_DEVICE_MODE == adapter->device_mode))) {
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if (eConnectionState_NotConnected ==
		    sta_ctx->conn_info.conn_state) {
			info->state = WIFI_DISCONNECTED;
		}
		if (eConnectionState_Connecting ==
		    sta_ctx->conn_info.conn_state) {
			hdd_debug("Session ID %d, Connection is in progress",
				  adapter->vdev_id);
			info->state = WIFI_ASSOCIATING;
		}
		if ((eConnectionState_Associated ==
		     sta_ctx->conn_info.conn_state) &&
		    (!sta_ctx->conn_info.is_authenticated)) {
			hdd_err("client " QDF_MAC_ADDR_FMT
				" is in the middle of WPS/EAPOL exchange.",
				QDF_MAC_ADDR_REF(adapter->mac_addr.bytes));
			info->state = WIFI_AUTHENTICATING;
		}
		if (eConnectionState_Associated ==
		    sta_ctx->conn_info.conn_state) {
			info->state = WIFI_ASSOCIATED;
			qdf_copy_macaddr(&info->bssid,
					 &sta_ctx->conn_info.bssid);
			qdf_mem_copy(info->ssid,
				     sta_ctx->conn_info.ssid.SSID.ssId,
				     sta_ctx->conn_info.ssid.SSID.length);
			/*
			 * NULL Terminate the string
			 */
			info->ssid[sta_ctx->conn_info.ssid.SSID.length] = 0;
		}
	}

	wlan_reg_get_cc_and_src(adapter->hdd_ctx->psoc, info->countryStr);
	wlan_reg_get_cc_and_src(adapter->hdd_ctx->psoc, info->apCountryStr);

	return true;
}

/**
 * hdd_link_layer_process_peer_stats() - This function is called after
 * @adapter: Pointer to device adapter
 * @more_data: More data
 * @peer_stat: Pointer to stats data
 *
 * Receiving Link Layer Peer statistics from FW.This function converts
 * the firmware data to the NL data and sends the same to the kernel/upper
 * layers.
 *
 * Return: None
 */
static void hdd_link_layer_process_peer_stats(struct hdd_adapter *adapter,
					      u32 more_data,
					      struct wifi_peer_stat *peer_stat)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct wifi_peer_info *peer_info;
	struct sk_buff *vendor_event;
	int status, i;
	struct nlattr *peers;
	int num_rate;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status)
		return;

	hdd_nofl_debug("LL_STATS_PEER_ALL : num_peers %u, more data = %u",
		       peer_stat->num_peers, more_data);

	/*
	 * Allocate a size of 4096 for the peer stats comprising
	 * each of size = sizeof (struct wifi_peer_info) + num_rate *
	 * sizeof (struct wifi_rate_stat).Each field is put with an
	 * NL attribute.The size of 4096 is considered assuming
	 * that number of rates shall not exceed beyond 50 with
	 * the sizeof (struct wifi_rate_stat) being 32.
	 */
	vendor_event = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
				LL_STATS_EVENT_BUF_SIZE);

	if (!vendor_event) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return;
	}

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE_PEER) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RESULTS_MORE_DATA,
			more_data) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_IFACE_NUM_PEERS,
			peer_stat->num_peers)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");

		kfree_skb(vendor_event);
		return;
	}

	peer_info = (struct wifi_peer_info *) ((uint8_t *)
					     peer_stat->peer_info);

	if (peer_stat->num_peers) {
		struct nlattr *peer_nest;

		peer_nest = nla_nest_start(vendor_event,
					   QCA_WLAN_VENDOR_ATTR_LL_STATS_PEER_INFO);
		if (!peer_nest) {
			hdd_err("nla_nest_start failed");
			kfree_skb(vendor_event);
			return;
		}

		for (i = 1; i <= peer_stat->num_peers; i++) {
			peers = nla_nest_start(vendor_event, i);
			if (!peers) {
				hdd_err("nla_nest_start failed");
				kfree_skb(vendor_event);
				return;
			}

			num_rate = peer_info->num_rate;

			if (!put_wifi_peer_info(peer_info, vendor_event)) {
				hdd_err("put_wifi_peer_info fail");
				kfree_skb(vendor_event);
				return;
			}

			peer_info = (struct wifi_peer_info *)
				((uint8_t *)peer_stat->peer_info +
				 (i * sizeof(struct wifi_peer_info)) +
				 (num_rate * sizeof(struct wifi_rate_stat)));
			nla_nest_end(vendor_event, peers);
		}
		nla_nest_end(vendor_event, peer_nest);
	}

	cfg80211_vendor_cmd_reply(vendor_event);
}

/**
 * hdd_link_layer_process_iface_stats() - This function is called after
 * @adapter: Pointer to device adapter
 * @if_stat: Pointer to stats data
 * @num_peers: Number of peers
 *
 * Receiving Link Layer Interface statistics from FW.This function converts
 * the firmware data to the NL data and sends the same to the kernel/upper
 * layers.
 *
 * Return: None
 */
static void
hdd_link_layer_process_iface_stats(struct hdd_adapter *adapter,
				   struct wifi_interface_stats *if_stat,
				   u32 num_peers)
{
	struct sk_buff *vendor_event;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	/*
	 * There is no need for wlan_hdd_validate_context here. This is a NB
	 * operation that will come with DSC synchronization. This ensures that
	 * no driver transition will take place as long as this operation is
	 * not complete. Thus the need to check validity of hdd_context is not
	 * required.
	 */

	/*
	 * Allocate a size of 4096 for the interface stats comprising
	 * sizeof (struct wifi_interface_stats *).The size of 4096 is considered
	 * assuming that all these fit with in the limit.Please take
	 * a call on the limit based on the data requirements on
	 * interface statistics.
	 */
	vendor_event = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy,
				LL_STATS_EVENT_BUF_SIZE);

	if (!vendor_event) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return;
	}

	hdd_debug("WMI_LINK_STATS_IFACE Data");

	if (!hdd_get_interface_info(adapter, &if_stat->info)) {
		hdd_err("hdd_get_interface_info get fail");
		kfree_skb(vendor_event);
		return;
	}

	if (!put_wifi_iface_stats(if_stat, num_peers, vendor_event)) {
		hdd_err("put_wifi_iface_stats fail");
		kfree_skb(vendor_event);
		return;
	}

	cfg80211_vendor_cmd_reply(vendor_event);
}

/**
 * hdd_llstats_radio_fill_channels() - radio stats fill channels
 * @adapter: Pointer to device adapter
 * @radiostat: Pointer to stats data
 * @vendor_event: vendor event
 *
 * Return: 0 on success; errno on failure
 */
static int hdd_llstats_radio_fill_channels(struct hdd_adapter *adapter,
					   struct wifi_radio_stats *radiostat,
					   struct sk_buff *vendor_event)
{
	struct wifi_channel_stats *channel_stats;
	struct nlattr *chlist;
	struct nlattr *chinfo;
	int i;

	chlist = nla_nest_start(vendor_event,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CH_INFO);
	if (!chlist) {
		hdd_err("nla_nest_start failed, %u", radiostat->num_channels);
		return -EINVAL;
	}

	for (i = 0; i < radiostat->num_channels; i++) {
		channel_stats = (struct wifi_channel_stats *) ((uint8_t *)
				     radiostat->channels +
				     (i * sizeof(struct wifi_channel_stats)));

		chinfo = nla_nest_start(vendor_event, i);
		if (!chinfo) {
			hdd_err("nla_nest_start failed, chan number %u",
				radiostat->num_channels);
			return -EINVAL;
		}

		if (nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_WIDTH,
				channel_stats->channel.width) ||
		    nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ,
				channel_stats->channel.center_freq) ||
		    nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ0,
				channel_stats->channel.center_freq0) ||
		    nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_INFO_CENTER_FREQ1,
				channel_stats->channel.center_freq1) ||
		    nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_ON_TIME,
				channel_stats->on_time) ||
		    nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_CCA_BUSY_TIME,
				channel_stats->cca_busy_time)) {
			hdd_err("nla_put failed for channel info (%u, %d, %u)",
				radiostat->num_channels, i,
				channel_stats->channel.center_freq);
			return -EINVAL;
		}

		if (adapter->hdd_ctx &&
		    adapter->hdd_ctx->ll_stats_per_chan_rx_tx_time) {
			if (nla_put_u32(
				vendor_event,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_TX_TIME,
				channel_stats->tx_time) ||
			    nla_put_u32(
				vendor_event,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CHANNEL_RX_TIME,
				channel_stats->rx_time)) {
				hdd_err("nla_put failed for tx time (%u, %d)",
					radiostat->num_channels, i);
				return -EINVAL;
			}
		}

		nla_nest_end(vendor_event, chinfo);
	}
	nla_nest_end(vendor_event, chlist);

	return 0;
}

/**
 * hdd_llstats_free_radio_stats() - free wifi_radio_stats member pointers
 * @radiostat: Pointer to stats data
 *
 * Return: void
 */
static void hdd_llstats_free_radio_stats(struct wifi_radio_stats *radiostat)
{
	if (radiostat->total_num_tx_power_levels &&
	    radiostat->tx_time_per_power_level) {
		qdf_mem_free(radiostat->tx_time_per_power_level);
		radiostat->tx_time_per_power_level = NULL;
	}
	if (radiostat->num_channels && radiostat->channels) {
		qdf_mem_free(radiostat->channels);
		radiostat->channels = NULL;
	}
}

/**
 * hdd_llstats_post_radio_stats() - post radio stats
 * @adapter: Pointer to device adapter
 * @more_data: More data
 * @radiostat: Pointer to stats data
 * @num_radio: Number of radios
 *
 * Return: void
 */
static void hdd_llstats_post_radio_stats(struct hdd_adapter *adapter,
					 u32 more_data,
					 struct wifi_radio_stats *radiostat,
					 u32 num_radio)
{
	struct sk_buff *vendor_event;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int ret;

	/*
	 * Allocate a size of 4096 for the Radio stats comprising
	 * sizeof (struct wifi_radio_stats) + num_channels * sizeof
	 * (struct wifi_channel_stats).Each channel data is put with an
	 * NL attribute.The size of 4096 is considered assuming that
	 * number of channels shall not exceed beyond  60 with the
	 * sizeof (struct wifi_channel_stats) being 24 bytes.
	 */

	vendor_event = cfg80211_vendor_cmd_alloc_reply_skb(
					hdd_ctx->wiphy,
					LL_STATS_EVENT_BUF_SIZE);

	if (!vendor_event) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		hdd_llstats_free_radio_stats(radiostat);
		goto failure;
	}

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_TYPE_RADIO) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RESULTS_MORE_DATA,
			more_data) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_NUM_RADIOS,
			num_radio) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ID,
			radiostat->radio) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME,
			radiostat->on_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME,
			radiostat->tx_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_RX_TIME,
			radiostat->rx_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_SCAN,
			radiostat->on_time_scan) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_NBD,
			radiostat->on_time_nbd) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_GSCAN,
			radiostat->on_time_gscan) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_ROAM_SCAN,
			radiostat->on_time_roam_scan) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_PNO_SCAN,
			radiostat->on_time_pno_scan) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_ON_TIME_HS20,
			radiostat->on_time_hs20) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_TX_LEVELS,
			radiostat->total_num_tx_power_levels)    ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_NUM_CHANNELS,
			radiostat->num_channels)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		hdd_llstats_free_radio_stats(radiostat);

		goto failure;
	}

	if (radiostat->total_num_tx_power_levels) {
		ret =
		    nla_put(vendor_event,
			    QCA_WLAN_VENDOR_ATTR_LL_STATS_RADIO_TX_TIME_PER_LEVEL,
			    sizeof(u32) *
			    radiostat->total_num_tx_power_levels,
			    radiostat->tx_time_per_power_level);
		if (ret) {
			hdd_err("nla_put fail");
			goto failure;
		}
	}

	if (radiostat->num_channels) {
		ret = hdd_llstats_radio_fill_channels(adapter, radiostat,
						      vendor_event);
		if (ret)
			goto failure;
	}

	cfg80211_vendor_cmd_reply(vendor_event);
	hdd_llstats_free_radio_stats(radiostat);
	return;

failure:
	kfree_skb(vendor_event);
	hdd_llstats_free_radio_stats(radiostat);
}

/**
 * hdd_link_layer_process_radio_stats() - This function is called after
 * @adapter: Pointer to device adapter
 * @more_data: More data
 * @radio_stat: Pointer to stats data
 * @num_radios: Number of radios
 *
 * Receiving Link Layer Radio statistics from FW.This function converts
 * the firmware data to the NL data and sends the same to the kernel/upper
 * layers.
 *
 * Return: None
 */
static void
hdd_link_layer_process_radio_stats(struct hdd_adapter *adapter,
				   u32 more_data,
				   struct wifi_radio_stats *radio_stat,
				   u32 num_radio)
{
	int i, nr;
	struct wifi_radio_stats *radio_stat_save = radio_stat;

	/*
	 * There is no need for wlan_hdd_validate_context here. This is a NB
	 * operation that will come with DSC synchronization. This ensures that
	 * no driver transition will take place as long as this operation is
	 * not complete. Thus the need to check validity of hdd_context is not
	 * required.
	 */

	for (i = 0; i < num_radio; i++) {
		hdd_nofl_debug("LL_STATS_RADIO"
		       " radio: %u on_time: %u tx_time: %u rx_time: %u"
		       " on_time_scan: %u on_time_nbd: %u"
		       " on_time_gscan: %u on_time_roam_scan: %u"
		       " on_time_pno_scan: %u  on_time_hs20: %u"
		       " num_channels: %u total_num_tx_pwr_levels: %u"
		       " on_time_host_scan: %u, on_time_lpi_scan: %u",
		       radio_stat->radio, radio_stat->on_time,
		       radio_stat->tx_time, radio_stat->rx_time,
		       radio_stat->on_time_scan, radio_stat->on_time_nbd,
		       radio_stat->on_time_gscan,
		       radio_stat->on_time_roam_scan,
		       radio_stat->on_time_pno_scan,
		       radio_stat->on_time_hs20,
		       radio_stat->num_channels,
		       radio_stat->total_num_tx_power_levels,
		       radio_stat->on_time_host_scan,
		       radio_stat->on_time_lpi_scan);
		radio_stat++;
	}

	radio_stat = radio_stat_save;
	for (nr = 0; nr < num_radio; nr++) {
		hdd_llstats_post_radio_stats(adapter, more_data,
					     radio_stat, num_radio);
		radio_stat++;
	}

	hdd_exit();
}

static void hdd_process_ll_stats(tSirLLStatsResults *results,
				 struct osif_request *request)
{
	struct hdd_ll_stats_priv *priv = osif_request_priv(request);
	struct hdd_ll_stats *stats = NULL;
	size_t stat_size = 0;

	if (!(priv->request_bitmap & results->paramId))
		return;

	if (results->paramId & WMI_LINK_STATS_RADIO) {
		struct wifi_radio_stats *rs_results, *stat_result;
		u64 channel_size = 0, pwr_lvl_size = 0;
		int i;
		stats = qdf_mem_malloc(sizeof(*stats));
		if (!stats)
			goto exit;

		stat_size = sizeof(struct wifi_radio_stats) *
			    results->num_radio;
		stats->result_param_id = WMI_LINK_STATS_RADIO;
		stat_result = qdf_mem_malloc(stat_size);
		if (!stat_result) {
			qdf_mem_free(stats);
			goto exit;
		}
		stats->result = stat_result;
		rs_results = (struct wifi_radio_stats *)results->results;
		qdf_mem_copy(stats->result, results->results, stat_size);
		for (i = 0; i < results->num_radio; i++) {
			channel_size = rs_results->num_channels *
				       sizeof(struct wifi_channel_stats);
			pwr_lvl_size = sizeof(uint32_t) *
				       rs_results->total_num_tx_power_levels;

			if (rs_results->total_num_tx_power_levels &&
			    rs_results->tx_time_per_power_level) {
				stat_result->tx_time_per_power_level =
						qdf_mem_malloc(pwr_lvl_size);
				if (!stat_result->tx_time_per_power_level) {
					while (i-- > 0) {
						stat_result--;
						qdf_mem_free(stat_result->
						    tx_time_per_power_level);
						qdf_mem_free(stat_result->
							     channels);
					}
					qdf_mem_free(stat_result);
					qdf_mem_free(stats);
					goto exit;
				}
			      qdf_mem_copy(stat_result->tx_time_per_power_level,
					   rs_results->tx_time_per_power_level,
					   pwr_lvl_size);
			}
			if (channel_size) {
				stat_result->channels =
						qdf_mem_malloc(channel_size);
				if (!stat_result->channels) {
					qdf_mem_free(stat_result->
						     tx_time_per_power_level);
					while (i-- > 0) {
						stat_result--;
						qdf_mem_free(stat_result->
						    tx_time_per_power_level);
						qdf_mem_free(stat_result->
							     channels);
					}
					qdf_mem_free(stats->result);
					qdf_mem_free(stats);
					goto exit;
				}
				qdf_mem_copy(stat_result->channels,
					     rs_results->channels,
					     channel_size);
			}
			rs_results++;
			stat_result++;
		}
		stats->stats_nradio_npeer.no_of_radios = results->num_radio;
		stats->more_data = results->moreResultToFollow;
		if (!results->moreResultToFollow)
			priv->request_bitmap &= ~stats->result_param_id;
	} else if (results->paramId & WMI_LINK_STATS_IFACE) {
		stats = qdf_mem_malloc(sizeof(*stats));
		if (!stats)
			goto exit;

		stats->result_param_id = WMI_LINK_STATS_IFACE;
		stats->stats_nradio_npeer.no_of_peers = results->num_peers;
		stats->result = qdf_mem_malloc(sizeof(struct
					       wifi_interface_stats));
		if (!stats->result) {
			qdf_mem_free(stats);
			goto exit;
		}
		qdf_mem_copy(stats->result, results->results,
			     sizeof(struct wifi_interface_stats));
		if (!results->num_peers)
			priv->request_bitmap &= ~(WMI_LINK_STATS_ALL_PEER);
		priv->request_bitmap &= ~stats->result_param_id;
	} else if (results->paramId & WMI_LINK_STATS_ALL_PEER) {
		struct wifi_peer_stat *peer_stat = (struct wifi_peer_stat *)
						   results->results;
		struct wifi_peer_info *peer_info = NULL;
		u64 num_rate = 0, peers, rates;
		int i;
		stats = qdf_mem_malloc(sizeof(*stats));
		if (!stats)
			goto exit;

		peer_info = (struct wifi_peer_info *)peer_stat->peer_info;
		for (i = 1; i <= peer_stat->num_peers; i++) {
			num_rate += peer_info->num_rate;
			peer_info = (struct wifi_peer_info *)((uint8_t *)
				    peer_info + sizeof(struct wifi_peer_info) +
				    (peer_info->num_rate *
				    sizeof(struct wifi_rate_stat)));
		}

		peers = sizeof(struct wifi_peer_info) * peer_stat->num_peers;
		rates = sizeof(struct wifi_rate_stat) * num_rate;
		stat_size = sizeof(struct wifi_peer_stat) + peers + rates;
		stats->result_param_id = WMI_LINK_STATS_ALL_PEER;

		stats->result = qdf_mem_malloc(stat_size);
		if (!stats->result) {
			qdf_mem_free(stats);
			goto exit;
		}

		qdf_mem_copy(stats->result, results->results, stat_size);
		stats->more_data = results->moreResultToFollow;
		if (!results->moreResultToFollow)
			priv->request_bitmap &= ~stats->result_param_id;
	} else {
		hdd_err("INVALID LL_STATS_NOTIFY RESPONSE");
	}
	/* send indication to caller thread */
	if (stats)
		qdf_list_insert_back(&priv->ll_stats_q, &stats->ll_stats_node);

	if (!priv->request_bitmap)
exit:
		osif_request_complete(request);
}

static void hdd_debugfs_process_ll_stats(struct hdd_adapter *adapter,
					 tSirLLStatsResults *results,
					 struct osif_request *request)
{
	struct hdd_ll_stats_priv *priv = osif_request_priv(request);

	if (results->paramId & WMI_LINK_STATS_RADIO) {
		hdd_debugfs_process_radio_stats(adapter,
						results->moreResultToFollow,
						results->results,
						results->num_radio);
		if (!results->moreResultToFollow)
			priv->request_bitmap &= ~(WMI_LINK_STATS_RADIO);
	} else if (results->paramId & WMI_LINK_STATS_IFACE) {
		hdd_debugfs_process_iface_stats(adapter, results->results,
						results->num_peers);

		/* Firmware doesn't send peerstats event if no peers are
		 * connected. HDD should not wait for any peerstats in
		 * this case and return the status to middleware after
		 * receiving iface stats
		 */

		if (!results->num_peers)
			priv->request_bitmap &= ~(WMI_LINK_STATS_ALL_PEER);

		priv->request_bitmap &= ~(WMI_LINK_STATS_IFACE);
	} else if (results->paramId & WMI_LINK_STATS_ALL_PEER) {
		hdd_debugfs_process_peer_stats(adapter, results->results);
		if (!results->moreResultToFollow)
			priv->request_bitmap &= ~(WMI_LINK_STATS_ALL_PEER);
	} else {
		hdd_err("INVALID LL_STATS_NOTIFY RESPONSE");
	}

	if (!priv->request_bitmap)
		osif_request_complete(request);
}

void wlan_hdd_cfg80211_link_layer_stats_callback(hdd_handle_t hdd_handle,
						 int indication_type,
						 tSirLLStatsResults *results,
						 void *cookie)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	struct hdd_ll_stats_priv *priv;
	struct hdd_adapter *adapter = NULL;
	int status;
	struct osif_request *request;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return;

	switch (indication_type) {
	case SIR_HAL_LL_STATS_RESULTS_RSP:
	{
		hdd_nofl_debug("LL_STATS RESP paramID = 0x%x, ifaceId = %u, respId= %u , moreResultToFollow = %u, num radio = %u result = %pK",
			       results->paramId, results->ifaceId,
			       results->rspId, results->moreResultToFollow,
			       results->num_radio, results->results);

		request = osif_request_get(cookie);
		if (!request) {
			hdd_err("Obsolete request");
			return;
		}

		priv = osif_request_priv(request);

		/* validate response received from target */
		if (priv->request_id != results->rspId) {
			hdd_err("Request id %d response id %d request bitmap 0x%x response bitmap 0x%x",
				priv->request_id, results->rspId,
				priv->request_bitmap, results->paramId);
			osif_request_put(request);
			return;
		}

		adapter = hdd_get_adapter_by_vdev(hdd_ctx, priv->vdev_id);
		if (!adapter) {
			hdd_err("invalid vdev %d", priv->vdev_id);
			return;
		}

		if (results->rspId == DEBUGFS_LLSTATS_REQID) {
			hdd_debugfs_process_ll_stats(adapter, results, request);
		 } else {
			qdf_spin_lock(&priv->ll_stats_lock);
			if (priv->request_bitmap)
				hdd_process_ll_stats(results, request);
			qdf_spin_unlock(&priv->ll_stats_lock);
		}

		osif_request_put(request);
		break;
	}
	default:
		hdd_warn("invalid event type %d", indication_type);
		break;
	}
}

void hdd_lost_link_info_cb(hdd_handle_t hdd_handle,
			   struct sir_lost_link_info *lost_link_info)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	int status;
	struct hdd_adapter *adapter;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return;

	if (!lost_link_info) {
		hdd_err("lost_link_info is NULL");
		return;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, lost_link_info->vdev_id);
	if (!adapter) {
		hdd_err("invalid adapter");
		return;
	}

	adapter->rssi_on_disconnect = lost_link_info->rssi;
	hdd_debug("rssi on disconnect %d", adapter->rssi_on_disconnect);
}

const struct nla_policy qca_wlan_vendor_ll_set_policy[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD]
						= { .type = NLA_U32 },
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING]
						= { .type = NLA_U32 },
};

/**
 * __wlan_hdd_cfg80211_ll_stats_set() - set link layer stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: int
 */
static int
__wlan_hdd_cfg80211_ll_stats_set(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len)
{
	int status;
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX + 1];
	tSirLLStatsSetReq req;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status)
		return -EINVAL;

	if (hdd_validate_adapter(adapter))
		return -EINVAL;

	if (adapter->device_mode != QDF_STA_MODE &&
	    adapter->device_mode != QDF_P2P_CLIENT_MODE &&
	    adapter->device_mode != QDF_P2P_GO_MODE) {
		hdd_debug("Cannot set LL_STATS for device mode %d",
			  adapter->device_mode);
		return -EINVAL;
	}

	if (wlan_cfg80211_nla_parse(tb_vendor,
				    QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_MAX,
				    (struct nlattr *)data, data_len,
				    qca_wlan_vendor_ll_set_policy)) {
		hdd_err("maximum attribute not present");
		return -EINVAL;
	}

	if (!tb_vendor
	    [QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD]) {
		hdd_err("MPDU size Not present");
		return -EINVAL;
	}

	if (!tb_vendor
	    [QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING]) {
		hdd_err("Stats Gathering Not Present");
		return -EINVAL;
	}

	/* Shall take the request Id if the Upper layers pass. 1 For now. */
	req.reqId = 1;

	req.mpduSizeThreshold =
		nla_get_u32(tb_vendor
			    [QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_MPDU_SIZE_THRESHOLD]);

	req.aggressiveStatisticsGathering =
		nla_get_u32(tb_vendor
			    [QCA_WLAN_VENDOR_ATTR_LL_STATS_SET_CONFIG_AGGRESSIVE_STATS_GATHERING]);

	req.staId = adapter->vdev_id;

	hdd_debug("LL_STATS_SET reqId = %d, staId = %d, mpduSizeThreshold = %d, Statistics Gathering = %d",
		req.reqId, req.staId,
		req.mpduSizeThreshold,
		req.aggressiveStatisticsGathering);

	if (QDF_STATUS_SUCCESS != sme_ll_stats_set_req(hdd_ctx->mac_handle,
						       &req)) {
		hdd_err("sme_ll_stats_set_req Failed");
		return -EINVAL;
	}

	adapter->is_link_layer_stats_set = true;
	hdd_exit();
	return 0;
}

/**
 * wlan_hdd_cfg80211_ll_stats_set() - set ll stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 if success, non-zero for failure
 */
int wlan_hdd_cfg80211_ll_stats_set(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_ll_stats_set(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

const struct nla_policy qca_wlan_vendor_ll_get_policy[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX + 1] = {
	/* Unsigned 32bit value provided by the caller issuing the GET stats
	 * command. When reporting
	 * the stats results, the driver uses the same value to indicate
	 * which GET request the results
	 * correspond to.
	 */
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID] = {.type = NLA_U32},

	/* Unsigned 32bit value . bit mask to identify what statistics are
	 * requested for retrieval
	 */
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK] = {.type = NLA_U32}
};

static void wlan_hdd_handle_ll_stats(struct hdd_adapter *adapter,
				     struct hdd_ll_stats *stats,
				     int ret)
{
	switch (stats->result_param_id) {
	case WMI_LINK_STATS_RADIO:
	{
		struct wifi_radio_stats *radio_stat = stats->result;
		int i, num_radio = stats->stats_nradio_npeer.no_of_radios;

		if (ret == -ETIMEDOUT) {
			for (i = 0; i < num_radio; i++) {
				if (radio_stat->num_channels)
					qdf_mem_free(radio_stat->channels);
				if (radio_stat->total_num_tx_power_levels)
					qdf_mem_free(radio_stat->
						     tx_time_per_power_level);
				radio_stat++;
			}
			return;
		}
		hdd_link_layer_process_radio_stats(adapter, stats->more_data,
						   radio_stat, num_radio);
	}
		break;
	case WMI_LINK_STATS_IFACE:
		hdd_link_layer_process_iface_stats(adapter, stats->result,
						   stats->stats_nradio_npeer.
						   no_of_peers);
		break;
	case WMI_LINK_STATS_ALL_PEER:
		hdd_link_layer_process_peer_stats(adapter,
						  stats->more_data,
						  stats->result);
		break;
	default:
		hdd_err("not requested event");
	}
}

static void wlan_hdd_dealloc_ll_stats(void *priv)
{
	struct hdd_ll_stats_priv *ll_stats_priv = priv;
	struct hdd_ll_stats *stats = NULL;
	QDF_STATUS status;
	qdf_list_node_t *ll_node;

	if (!ll_stats_priv)
		return;

	qdf_spin_lock(&ll_stats_priv->ll_stats_lock);
	status = qdf_list_remove_front(&ll_stats_priv->ll_stats_q, &ll_node);
	qdf_spin_unlock(&ll_stats_priv->ll_stats_lock);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		stats =  qdf_container_of(ll_node, struct hdd_ll_stats,
					  ll_stats_node);

		if (stats->result_param_id == WMI_LINK_STATS_RADIO) {
			struct wifi_radio_stats *radio_stat = stats->result;
			int i;
			int num_radio = stats->stats_nradio_npeer.no_of_radios;

			for (i = 0; i < num_radio; i++) {
				if (radio_stat->num_channels)
					qdf_mem_free(radio_stat->channels);
				if (radio_stat->total_num_tx_power_levels)
					qdf_mem_free(radio_stat->
						tx_time_per_power_level);
				radio_stat++;
			}
		}

		qdf_mem_free(stats->result);
		qdf_mem_free(stats);
		qdf_spin_lock(&ll_stats_priv->ll_stats_lock);
		status = qdf_list_remove_front(&ll_stats_priv->ll_stats_q,
					       &ll_node);
		qdf_spin_unlock(&ll_stats_priv->ll_stats_lock);
	}
	qdf_list_destroy(&ll_stats_priv->ll_stats_q);
}

/*
 * copy_station_stats_to_adapter() - Copy station stats to adapter
 * @adapter: Pointer to the adapter
 * @stats: Pointer to the station stats event
 *
 * Return: 0 if success, non-zero for failure
 */
static int copy_station_stats_to_adapter(struct hdd_adapter *adapter,
					 struct stats_event *stats)
{
	int ret = 0;
	struct wlan_mlme_nss_chains *dynamic_cfg;
	uint32_t tx_nss, rx_nss;
	struct wlan_objmgr_vdev *vdev;
	uint16_t he_mcs_12_13_map;
	bool is_he_mcs_12_13_supported;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return -EINVAL;

	/* save summary stats to legacy location */
	qdf_mem_copy(adapter->hdd_stats.summary_stat.retry_cnt,
		     stats->vdev_summary_stats[0].stats.retry_cnt,
		     sizeof(adapter->hdd_stats.summary_stat.retry_cnt));
	qdf_mem_copy(
		adapter->hdd_stats.summary_stat.multiple_retry_cnt,
		stats->vdev_summary_stats[0].stats.multiple_retry_cnt,
		sizeof(adapter->hdd_stats.summary_stat.multiple_retry_cnt));
	qdf_mem_copy(adapter->hdd_stats.summary_stat.tx_frm_cnt,
		     stats->vdev_summary_stats[0].stats.tx_frm_cnt,
		     sizeof(adapter->hdd_stats.summary_stat.tx_frm_cnt));
	qdf_mem_copy(adapter->hdd_stats.summary_stat.fail_cnt,
		     stats->vdev_summary_stats[0].stats.fail_cnt,
		     sizeof(adapter->hdd_stats.summary_stat.fail_cnt));
	adapter->hdd_stats.summary_stat.snr =
			stats->vdev_summary_stats[0].stats.snr;
	adapter->hdd_stats.summary_stat.rssi =
			stats->vdev_summary_stats[0].stats.rssi;
	adapter->hdd_stats.summary_stat.rx_frm_cnt =
			stats->vdev_summary_stats[0].stats.rx_frm_cnt;
	adapter->hdd_stats.summary_stat.frm_dup_cnt =
			stats->vdev_summary_stats[0].stats.frm_dup_cnt;
	adapter->hdd_stats.summary_stat.rts_fail_cnt =
			stats->vdev_summary_stats[0].stats.rts_fail_cnt;
	adapter->hdd_stats.summary_stat.ack_fail_cnt =
			stats->vdev_summary_stats[0].stats.ack_fail_cnt;
	adapter->hdd_stats.summary_stat.rts_succ_cnt =
			stats->vdev_summary_stats[0].stats.rts_succ_cnt;
	adapter->hdd_stats.summary_stat.rx_discard_cnt =
			stats->vdev_summary_stats[0].stats.rx_discard_cnt;
	adapter->hdd_stats.summary_stat.rx_error_cnt =
			stats->vdev_summary_stats[0].stats.rx_error_cnt;
	adapter->hdd_stats.peer_stats.rx_count =
			stats->peer_adv_stats->rx_count;
	adapter->hdd_stats.peer_stats.rx_bytes =
			stats->peer_adv_stats->rx_bytes;
	adapter->hdd_stats.peer_stats.fcs_count =
			stats->peer_adv_stats->fcs_count;

	dynamic_cfg = mlme_get_dynamic_vdev_config(vdev);
	if (!dynamic_cfg) {
		hdd_err("nss chain dynamic config NULL");
		ret = -EINVAL;
		goto out;
	}

	switch (hdd_conn_get_connected_band(&adapter->session.station)) {
	case BAND_2G:
		tx_nss = dynamic_cfg->tx_nss[NSS_CHAINS_BAND_2GHZ];
		rx_nss = dynamic_cfg->rx_nss[NSS_CHAINS_BAND_2GHZ];
		break;
	case BAND_5G:
		tx_nss = dynamic_cfg->tx_nss[NSS_CHAINS_BAND_5GHZ];
		rx_nss = dynamic_cfg->rx_nss[NSS_CHAINS_BAND_5GHZ];
		break;
	default:
		tx_nss = wlan_vdev_mlme_get_nss(vdev);
		rx_nss = wlan_vdev_mlme_get_nss(vdev);
	}

	/* Intersection of self and AP's NSS capability */
	if (tx_nss > wlan_vdev_mlme_get_nss(vdev))
		tx_nss = wlan_vdev_mlme_get_nss(vdev);

	if (rx_nss > wlan_vdev_mlme_get_nss(vdev))
		rx_nss = wlan_vdev_mlme_get_nss(vdev);

	/* save class a stats to legacy location */
	adapter->hdd_stats.class_a_stat.tx_nss = tx_nss;
	adapter->hdd_stats.class_a_stat.rx_nss = rx_nss;
	adapter->hdd_stats.class_a_stat.tx_rate = stats->tx_rate;
	adapter->hdd_stats.class_a_stat.rx_rate = stats->rx_rate;
	adapter->hdd_stats.class_a_stat.tx_rx_rate_flags = stats->tx_rate_flags;

	he_mcs_12_13_map = wlan_vdev_mlme_get_he_mcs_12_13_map(vdev);
	is_he_mcs_12_13_supported =
			wlan_hdd_is_he_mcs_12_13_supported(he_mcs_12_13_map);
	adapter->hdd_stats.class_a_stat.tx_mcs_index =
		sme_get_mcs_idx(stats->tx_rate, stats->tx_rate_flags,
				is_he_mcs_12_13_supported,
				&adapter->hdd_stats.class_a_stat.tx_nss,
				&adapter->hdd_stats.class_a_stat.tx_dcm,
				&adapter->hdd_stats.class_a_stat.tx_gi,
				&adapter->hdd_stats.class_a_stat.
				tx_mcs_rate_flags);
	adapter->hdd_stats.class_a_stat.rx_mcs_index =
		sme_get_mcs_idx(stats->rx_rate, stats->tx_rate_flags,
				is_he_mcs_12_13_supported,
				&adapter->hdd_stats.class_a_stat.rx_nss,
				&adapter->hdd_stats.class_a_stat.rx_dcm,
				&adapter->hdd_stats.class_a_stat.rx_gi,
				&adapter->hdd_stats.class_a_stat.
				rx_mcs_rate_flags);

	/* save per chain rssi to legacy location */
	qdf_mem_copy(adapter->hdd_stats.per_chain_rssi_stats.rssi,
		     stats->vdev_chain_rssi[0].chain_rssi,
		     sizeof(stats->vdev_chain_rssi[0].chain_rssi));
	adapter->hdd_stats.bcn_protect_stats = stats->bcn_protect_stats;
out:
	hdd_objmgr_put_vdev(vdev);
	return ret;
}

#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
/**
 * cache_station_stats_cb() - cache_station_stats_cb callback function
 * @ev: station stats buffer
 * @cookie: cookie that contains the address of the adapter corresponding to
 *          the request
 *
 * Return: None
 */
static void cache_station_stats_cb(struct stats_event *ev, void *cookie)
{
	struct hdd_adapter *adapter = cookie, *next_adapter = NULL;
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;
	uint8_t vdev_id = adapter->vdev_id;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_DISPLAY_TXRX_STATS;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		if (adapter->vdev_id != vdev_id) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}
		copy_station_stats_to_adapter(adapter, ev);
		/* dev_put has to be done here */
		hdd_adapter_dev_put_debug(adapter, dbgid);
		if (next_adapter)
			hdd_adapter_dev_put_debug(next_adapter,
						  dbgid);
		break;
	}
}

static QDF_STATUS
wlan_hdd_set_station_stats_request_pending(struct hdd_adapter *adapter)
{
	struct wlan_objmgr_peer *peer;
	struct request_info info = {0};
	struct wlan_objmgr_vdev *vdev;

	if (!adapter->hdd_ctx->is_get_station_clubbed_in_ll_stats_req)
		return QDF_STATUS_E_INVAL;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return QDF_STATUS_E_INVAL;

	if (adapter->hdd_stats.is_ll_stats_req_in_progress) {
		hdd_err("Previous ll_stats request is in progress");
		hdd_objmgr_put_vdev(vdev);
		return QDF_STATUS_E_ALREADY;
	}

	info.cookie = adapter;
	info.u.get_station_stats_cb = cache_station_stats_cb;
	info.vdev_id = adapter->vdev_id;
	info.pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_OSIF_ID);
	if (!peer) {
		osif_err("peer is null");
		hdd_objmgr_put_vdev(vdev);
		return QDF_STATUS_E_INVAL;
	}

	adapter->hdd_stats.is_ll_stats_req_in_progress = true;

	qdf_mem_copy(info.peer_mac_addr, peer->macaddr, QDF_MAC_ADDR_SIZE);

	wlan_objmgr_peer_release_ref(peer, WLAN_OSIF_ID);

	ucfg_mc_cp_stats_set_pending_req(wlan_vdev_get_psoc(vdev),
					 TYPE_STATION_STATS, &info);
	hdd_objmgr_put_vdev(vdev);
	return QDF_STATUS_SUCCESS;
}

static void
wlan_hdd_reset_station_stats_request_pending(struct wlan_objmgr_psoc *psoc,
					     struct hdd_adapter *adapter)
{
	QDF_STATUS status;
	struct request_info last_req = {0};
	bool pending = false;

	if (!adapter->hdd_ctx->is_get_station_clubbed_in_ll_stats_req)
		return;

	adapter->hdd_stats.is_ll_stats_req_in_progress = false;

	status = ucfg_mc_cp_stats_get_pending_req(psoc, TYPE_STATION_STATS,
						  &last_req);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("ucfg_mc_cp_stats_get_pending_req failed");
		return;
	}

	ucfg_mc_cp_stats_reset_pending_req(psoc, TYPE_STATION_STATS,
					   &last_req, &pending);
}

static QDF_STATUS wlan_hdd_stats_request_needed(struct hdd_adapter *adapter)
{
	if (adapter->device_mode != QDF_STA_MODE)
		return QDF_STATUS_SUCCESS;

	if (!adapter->hdd_ctx->config) {
		hdd_err("Invalid hdd config");
		return QDF_STATUS_E_INVAL;
	}
	if (adapter->hdd_ctx->is_get_station_clubbed_in_ll_stats_req) {
		uint32_t stats_cached_duration;

		stats_cached_duration =
				qdf_system_ticks_to_msecs(qdf_system_ticks()) -
				adapter->hdd_stats.sta_stats_cached_timestamp;
		if (stats_cached_duration <=
			adapter->hdd_ctx->config->sta_stats_cache_expiry_time)
			return QDF_STATUS_E_ALREADY;
	}
	return QDF_STATUS_SUCCESS;
}

#else
static QDF_STATUS
wlan_hdd_set_station_stats_request_pending(struct hdd_adapter *adapter)
{
	return QDF_STATUS_SUCCESS;
}

static void
wlan_hdd_reset_station_stats_request_pending(struct wlan_objmgr_psoc *psoc,
					     struct hdd_adapter *adapter)
{
}

static QDF_STATUS wlan_hdd_stats_request_needed(struct hdd_adapter *adapter)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_CLUB_LL_STATS_AND_GET_STATION */

static int wlan_hdd_send_ll_stats_req(struct hdd_adapter *adapter,
				      tSirLLStatsGetReq *req)
{
	int ret = 0;
	struct hdd_ll_stats_priv *priv;
	struct hdd_ll_stats *stats = NULL;
	struct osif_request *request;
	qdf_list_node_t *ll_node;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	void *cookie;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_LL_STATS,
		.dealloc = wlan_hdd_dealloc_ll_stats,
	};

	hdd_enter_dev(adapter->dev);

	status = wlan_hdd_set_station_stats_request_pending(adapter);
	if (status == QDF_STATUS_E_ALREADY)
		return qdf_status_to_os_return(status);

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request Allocation Failure");
		wlan_hdd_reset_station_stats_request_pending(hdd_ctx->psoc,
							     adapter);
		return -ENOMEM;
	}

	cookie = osif_request_cookie(request);

	priv = osif_request_priv(request);

	priv->request_id = req->reqId;
	priv->request_bitmap = req->paramIdMask;
	priv->vdev_id = adapter->vdev_id;
	qdf_spinlock_create(&priv->ll_stats_lock);
	qdf_list_create(&priv->ll_stats_q, HDD_LINK_STATS_MAX);

	status = sme_ll_stats_get_req(hdd_ctx->mac_handle, req, cookie);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("sme_ll_stats_get_req Failed");
		ret = qdf_status_to_os_return(status);
		goto exit;
	}
	ret = osif_request_wait_for_response(request);
	if (ret) {
		hdd_err("Target response timed out request id %d request bitmap 0x%x",
			priv->request_id, priv->request_bitmap);
		qdf_spin_lock(&priv->ll_stats_lock);
		priv->request_bitmap = 0;
		qdf_spin_unlock(&priv->ll_stats_lock);
		ret = -ETIMEDOUT;
	} else {
		hdd_update_station_stats_cached_timestamp(adapter);
	}
	qdf_spin_lock(&priv->ll_stats_lock);
	status = qdf_list_remove_front(&priv->ll_stats_q, &ll_node);
	qdf_spin_unlock(&priv->ll_stats_lock);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		stats =  qdf_container_of(ll_node, struct hdd_ll_stats,
					  ll_stats_node);
		wlan_hdd_handle_ll_stats(adapter, stats, ret);
		qdf_mem_free(stats->result);
		qdf_mem_free(stats);
		qdf_spin_lock(&priv->ll_stats_lock);
		status = qdf_list_remove_front(&priv->ll_stats_q, &ll_node);
		qdf_spin_unlock(&priv->ll_stats_lock);
	}
	qdf_list_destroy(&priv->ll_stats_q);
exit:
	wlan_hdd_reset_station_stats_request_pending(hdd_ctx->psoc, adapter);
	hdd_exit();
	osif_request_put(request);

	return ret;
}

int wlan_hdd_ll_stats_get(struct hdd_adapter *adapter, uint32_t req_id,
			  uint32_t req_mask)
{
	int errno;
	tSirLLStatsGetReq get_req;
	struct hdd_station_ctx *hddstactx =
					WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	hdd_enter_dev(adapter->dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_warn("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (hddstactx->hdd_reassoc_scenario) {
		hdd_err("Roaming in progress, cannot process the request");
		return -EBUSY;
	}

	if (!adapter->is_link_layer_stats_set) {
		hdd_info("LL_STATs not set");
		return -EINVAL;
	}

	get_req.reqId = req_id;
	get_req.paramIdMask = req_mask;
	get_req.staId = adapter->vdev_id;

	rtnl_lock();
	errno = wlan_hdd_send_ll_stats_req(adapter, &get_req);
	rtnl_unlock();
	if (errno)
		hdd_err("Send LL stats req failed, id:%u, mask:%d, session:%d",
			req_id, req_mask, adapter->vdev_id);

	hdd_exit();

	return errno;
}

/**
 * __wlan_hdd_cfg80211_ll_stats_get() - get link layer stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: int
 */
static int
__wlan_hdd_cfg80211_ll_stats_get(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX + 1];
	tSirLLStatsGetReq LinkLayerStatsGetReq;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_station_ctx *hddstactx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	/* ENTER() intentionally not used in a frequently invoked API */

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return -EINVAL;

	if (!adapter->is_link_layer_stats_set) {
		hdd_nofl_debug("is_link_layer_stats_set: %d",
			       adapter->is_link_layer_stats_set);
		return -EINVAL;
	}

	if (hddstactx->hdd_reassoc_scenario) {
		hdd_err("Roaming in progress, cannot process the request");
		return -EBUSY;
	}

	if (wlan_cfg80211_nla_parse(tb_vendor,
				    QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_MAX,
				    (struct nlattr *)data, data_len,
				    qca_wlan_vendor_ll_get_policy)) {
		hdd_err("max attribute not present");
		return -EINVAL;
	}

	if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID]) {
		hdd_err("Request Id Not present");
		return -EINVAL;
	}

	if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK]) {
		hdd_err("Req Mask Not present");
		return -EINVAL;
	}

	LinkLayerStatsGetReq.reqId =
		nla_get_u32(tb_vendor
			    [QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_ID]);
	LinkLayerStatsGetReq.paramIdMask =
		nla_get_u32(tb_vendor
			    [QCA_WLAN_VENDOR_ATTR_LL_STATS_GET_CONFIG_REQ_MASK]);

	LinkLayerStatsGetReq.staId = adapter->vdev_id;

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	ret = wlan_hdd_send_ll_stats_req(adapter, &LinkLayerStatsGetReq);
	if (0 != ret) {
		hdd_err("Failed to send LL stats request (id:%u)",
			LinkLayerStatsGetReq.reqId);
		return ret;
	}

	hdd_exit();
	return 0;
}

/**
 * wlan_hdd_cfg80211_ll_stats_get() - get ll stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 if success, non-zero for failure
 */
int wlan_hdd_cfg80211_ll_stats_get(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (0 != errno)
		return -EINVAL;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = wlan_hdd_qmi_get_sync_resume();
	if (errno) {
		hdd_err("qmi sync resume failed: %d", errno);
		goto end;
	}
	errno = __wlan_hdd_cfg80211_ll_stats_get(wiphy, wdev, data, data_len);

	wlan_hdd_qmi_put_suspend();

end:
	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

const struct
nla_policy
	qca_wlan_vendor_ll_clr_policy[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK] = {.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP] = {.type = NLA_U8},
};

/**
 * __wlan_hdd_cfg80211_ll_stats_clear() - clear link layer stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: int
 */
static int
__wlan_hdd_cfg80211_ll_stats_clear(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data,
				    int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX + 1];
	tSirLLStatsClearReq LinkLayerStatsClearReq;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	u32 statsClearReqMask;
	u8 stopReq;
	int errno;
	QDF_STATUS status;
	struct sk_buff *skb;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return -EINVAL;

	if (!adapter->is_link_layer_stats_set) {
		hdd_warn("is_link_layer_stats_set : %d",
			  adapter->is_link_layer_stats_set);
		return -EINVAL;
	}

	if (wlan_cfg80211_nla_parse(tb_vendor,
				    QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_MAX,
				    (struct nlattr *)data, data_len,
				    qca_wlan_vendor_ll_clr_policy)) {
		hdd_err("STATS_CLR_MAX is not present");
		return -EINVAL;
	}

	if (!tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK] ||
	    !tb_vendor[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ]) {
		hdd_err("Error in LL_STATS CLR CONFIG PARA");
		return -EINVAL;
	}

	statsClearReqMask = LinkLayerStatsClearReq.statsClearReqMask =
				    nla_get_u32(tb_vendor
						[QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_REQ_MASK]);

	stopReq = LinkLayerStatsClearReq.stopReq =
			  nla_get_u8(tb_vendor
				     [QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_REQ]);

	/*
	 * Shall take the request Id if the Upper layers pass. 1 For now.
	 */
	LinkLayerStatsClearReq.reqId = 1;

	LinkLayerStatsClearReq.staId = adapter->vdev_id;

	hdd_debug("LL_STATS_CLEAR reqId = %d, staId = %d, statsClearReqMask = 0x%X, stopReq = %d",
		LinkLayerStatsClearReq.reqId,
		LinkLayerStatsClearReq.staId,
		LinkLayerStatsClearReq.statsClearReqMask,
		LinkLayerStatsClearReq.stopReq);

	status = sme_ll_stats_clear_req(hdd_ctx->mac_handle,
					&LinkLayerStatsClearReq);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("stats clear request failed, %d", status);
		return -EINVAL;
	}

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy,
						  2 * sizeof(u32) +
						  2 * NLMSG_HDRLEN);
	if (!skb) {
		hdd_err("skb allocation failed");
		return -ENOMEM;
	}

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_RSP_MASK,
			statsClearReqMask) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_CLR_CONFIG_STOP_RSP,
			stopReq)) {
		hdd_err("LL_STATS_CLR put fail");
		kfree_skb(skb);
		return -EINVAL;
	}

	/* If the ask is to stop the stats collection
	 * as part of clear (stopReq = 1), ensure
	 * that no further requests of get go to the
	 * firmware by having is_link_layer_stats_set set
	 * to 0.  However it the stopReq as part of
	 * the clear request is 0, the request to get
	 * the statistics are honoured as in this case
	 * the firmware is just asked to clear the
	 * statistics.
	 */
	if (stopReq == 1)
		adapter->is_link_layer_stats_set = false;

	hdd_exit();

	return cfg80211_vendor_cmd_reply(skb);
}

/**
 * wlan_hdd_cfg80211_ll_stats_clear() - clear ll stats
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 if success, non-zero for failure
 */
int wlan_hdd_cfg80211_ll_stats_clear(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_ll_stats_clear(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * wlan_hdd_clear_link_layer_stats() - clear link layer stats
 * @adapter: pointer to adapter
 *
 * Wrapper function to clear link layer stats.
 * return - void
 */
void wlan_hdd_clear_link_layer_stats(struct hdd_adapter *adapter)
{
	tSirLLStatsClearReq link_layer_stats_clear_req;
	mac_handle_t mac_handle = adapter->hdd_ctx->mac_handle;

	link_layer_stats_clear_req.statsClearReqMask = WIFI_STATS_IFACE_AC |
		WIFI_STATS_IFACE_ALL_PEER;
	link_layer_stats_clear_req.stopReq = 0;
	link_layer_stats_clear_req.reqId = 1;
	link_layer_stats_clear_req.staId = adapter->vdev_id;
	sme_ll_stats_clear_req(mac_handle, &link_layer_stats_clear_req);
}

/**
 * hdd_populate_per_peer_ps_info() - populate per peer sta's PS info
 * @wifi_peer_info: peer information
 * @vendor_event: buffer for vendor event
 *
 * Return: 0 success
 */
static inline int
hdd_populate_per_peer_ps_info(struct wifi_peer_info *wifi_peer_info,
			      struct sk_buff *vendor_event)
{
	if (!wifi_peer_info) {
		hdd_err("Invalid pointer to peer info.");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_STATE,
			wifi_peer_info->power_saving) ||
	    nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_MAC_ADDRESS,
		    QDF_MAC_ADDR_SIZE, &wifi_peer_info->peer_macaddr)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail.");
		return -EINVAL;
	}
	return 0;
}

/**
 * hdd_populate_wifi_peer_ps_info() - populate peer sta's power state
 * @data: stats for peer STA
 * @vendor_event: buffer for vendor event
 *
 * Return: 0 success
 */
static int hdd_populate_wifi_peer_ps_info(struct wifi_peer_stat *data,
					  struct sk_buff *vendor_event)
{
	uint32_t peer_num, i;
	struct wifi_peer_info *wifi_peer_info;
	struct nlattr *peer_info, *peers;

	if (!data) {
		hdd_err("Invalid pointer to Wifi peer stat.");
		return -EINVAL;
	}

	peer_num = data->num_peers;
	if (peer_num == 0) {
		hdd_err("Peer number is zero.");
		return -EINVAL;
	}

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_NUM,
			peer_num)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		return -EINVAL;
	}

	peer_info = nla_nest_start(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_CHG);
	if (!peer_info) {
		hdd_err("nla_nest_start failed");
		return -EINVAL;
	}

	for (i = 0; i < peer_num; i++) {
		wifi_peer_info = &data->peer_info[i];
		peers = nla_nest_start(vendor_event, i);

		if (!peers) {
			hdd_err("nla_nest_start failed");
			return -EINVAL;
		}

		if (hdd_populate_per_peer_ps_info(wifi_peer_info, vendor_event))
			return -EINVAL;

		nla_nest_end(vendor_event, peers);
	}
	nla_nest_end(vendor_event, peer_info);

	return 0;
}

/**
 * hdd_populate_tx_failure_info() - populate TX failure info
 * @tx_fail: TX failure info
 * @skb: buffer for vendor event
 *
 * Return: 0 Success
 */
static inline int
hdd_populate_tx_failure_info(struct sir_wifi_iface_tx_fail *tx_fail,
			     struct sk_buff *skb)
{
	int status = 0;

	if (!tx_fail || !skb)
		return -EINVAL;

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TID,
			tx_fail->tid) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_NUM_MSDU,
			tx_fail->msdu_num) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_STATUS,
			tx_fail->status)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		status = -EINVAL;
	}

	return status;
}

/**
 * hdd_populate_wifi_channel_cca_info() - put channel cca info to vendor event
 * @info: cca info array for all channels
 * @vendor_event: vendor event buffer
 *
 * Return: 0 Success, EINVAL failure
 */
static int
hdd_populate_wifi_channel_cca_info(struct sir_wifi_chan_cca_stats *cca,
				   struct sk_buff *vendor_event)
{
	/* There might be no CCA info for a channel */
	if (!cca)
		return 0;

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IDLE_TIME,
			cca->idle_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_TIME,
			cca->tx_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IN_BSS_TIME,
			cca->rx_in_bss_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_OUT_BSS_TIME,
			cca->rx_out_bss_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BUSY,
			cca->rx_busy_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BAD,
			cca->rx_in_bad_cond_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BAD,
			cca->tx_in_bad_cond_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_NO_AVAIL,
			cca->wlan_not_avail_time) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_ID,
			cca->vdev_id)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		return -EINVAL;
	}
	return 0;
}

/**
 * hdd_populate_wifi_signal_info - put chain signal info
 * @info: RF chain signal info
 * @skb: vendor event buffer
 *
 * Return: 0 Success, EINVAL failure
 */
static int
hdd_populate_wifi_signal_info(struct sir_wifi_peer_signal_stats *peer_signal,
			      struct sk_buff *skb)
{
	uint32_t i, chain_count;
	struct nlattr *chains, *att;

	/* There might be no signal info for a peer */
	if (!peer_signal)
		return 0;

	chain_count = peer_signal->num_chain < WIFI_MAX_CHAINS ?
		      peer_signal->num_chain : WIFI_MAX_CHAINS;
	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_ANT_NUM,
			chain_count)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		return -EINVAL;
	}

	att = nla_nest_start(skb,
			     QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_SIGNAL);
	if (!att) {
		hdd_err("nla_nest_start failed");
		return -EINVAL;
	}

	for (i = 0; i < chain_count; i++) {
		chains = nla_nest_start(skb, i);

		if (!chains) {
			hdd_err("nla_nest_start failed");
			return -EINVAL;
		}

		hdd_debug("SNR=%d, NF=%d, Rx=%d, Tx=%d",
			  peer_signal->per_ant_snr[i],
			  peer_signal->nf[i],
			  peer_signal->per_ant_rx_mpdus[i],
			  peer_signal->per_ant_tx_mpdus[i]);
		if (nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_SNR,
				peer_signal->per_ant_snr[i]) ||
		    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_NF,
				peer_signal->nf[i]) ||
		    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU,
				peer_signal->per_ant_rx_mpdus[i]) ||
		    nla_put_u32(skb,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MPDU,
				peer_signal->per_ant_tx_mpdus[i])) {
			hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
			return -EINVAL;
		}
		nla_nest_end(skb, chains);
	}
	nla_nest_end(skb, att);

	return 0;
}

/**
 * hdd_populate_wifi_wmm_ac_tx_info() - put AC TX info
 * @info: tx info
 * @skb: vendor event buffer
 *
 * Return: 0 Success, EINVAL failure
 */
static int
hdd_populate_wifi_wmm_ac_tx_info(struct sir_wifi_tx *tx_stats,
				 struct sk_buff *skb)
{
	uint32_t *agg_size, *succ_mcs, *fail_mcs, *delay;

	/* There might be no TX info for a peer */
	if (!tx_stats)
		return 0;

	agg_size = tx_stats->mpdu_aggr_size;
	succ_mcs = tx_stats->success_mcs;
	fail_mcs = tx_stats->fail_mcs;
	delay = tx_stats->delay;

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MSDU,
			tx_stats->msdus) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MPDU,
			tx_stats->mpdus) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_PPDU,
			tx_stats->ppdus) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BYTES,
			tx_stats->bytes) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP,
			tx_stats->drops) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP_BYTES,
			tx_stats->drop_bytes) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_RETRY,
			tx_stats->retries) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_ACK,
			tx_stats->failed) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_AGGR_NUM,
			tx_stats->aggr_len) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_SUCC_MCS_NUM,
			tx_stats->success_mcs_len) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_FAIL_MCS_NUM,
			tx_stats->fail_mcs_len) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_DELAY_ARRAY_SIZE,
			tx_stats->delay_len))
		goto put_attr_fail;

	if (agg_size) {
		if (nla_put(skb,
			    QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_AGGR,
			    tx_stats->aggr_len, agg_size))
			goto put_attr_fail;
	}

	if (succ_mcs) {
		if (nla_put(skb,
			    QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_SUCC_MCS,
			    tx_stats->success_mcs_len, succ_mcs))
			goto put_attr_fail;
	}

	if (fail_mcs) {
		if (nla_put(skb,
			    QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_FAIL_MCS,
			    tx_stats->fail_mcs_len, fail_mcs))
			goto put_attr_fail;
	}

	if (delay) {
		if (nla_put(skb,
			    QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DELAY,
			    tx_stats->delay_len, delay))
			goto put_attr_fail;
	}
	return 0;

put_attr_fail:
	hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
	return -EINVAL;
}

/**
 * hdd_populate_wifi_wmm_ac_rx_info() - put AC RX info
 * @info: rx info
 * @skb: vendor event buffer
 *
 * Return: 0 Success, EINVAL failure
 */
static int
hdd_populate_wifi_wmm_ac_rx_info(struct sir_wifi_rx *rx_stats,
				 struct sk_buff *skb)
{
	uint32_t *mcs, *aggr;

	/* There might be no RX info for a peer */
	if (!rx_stats)
		return 0;

	aggr = rx_stats->mpdu_aggr;
	mcs = rx_stats->mcs;

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU,
			rx_stats->mpdus) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_BYTES,
			rx_stats->bytes) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU,
			rx_stats->ppdus) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU_BYTES,
			rx_stats->ppdu_bytes) ||
	    nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_LOST,
			rx_stats->mpdu_lost) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_RETRY,
			rx_stats->mpdu_retry) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DUP,
			rx_stats->mpdu_dup) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DISCARD,
			rx_stats->mpdu_discard) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_AGGR_NUM,
			rx_stats->aggr_len) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MCS_NUM,
			rx_stats->mcs_len))
		goto put_attr_fail;

	if (aggr) {
		if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_AGGR,
			    rx_stats->aggr_len, aggr))
			goto put_attr_fail;
	}

	if (mcs) {
		if (nla_put(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MCS,
			    rx_stats->mcs_len, mcs))
			goto put_attr_fail;
	}

	return 0;

put_attr_fail:
	hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
	return -EINVAL;
}

/**
 * hdd_populate_wifi_wmm_ac_info() - put WMM AC info
 * @info: per AC stats
 * @skb: vendor event buffer
 *
 * Return: 0 Success, EINVAL failure
 */
static int
hdd_populate_wifi_wmm_ac_info(struct sir_wifi_ll_ext_wmm_ac_stats *ac_stats,
			      struct sk_buff *skb)
{
	struct nlattr *wmm;

	wmm = nla_nest_start(skb, ac_stats->type);
	if (!wmm)
		goto nest_start_fail;

	if (hdd_populate_wifi_wmm_ac_tx_info(ac_stats->tx_stats, skb) ||
	    hdd_populate_wifi_wmm_ac_rx_info(ac_stats->rx_stats, skb))
		goto put_attr_fail;

	nla_nest_end(skb, wmm);
	return 0;

nest_start_fail:
	hdd_err("nla_nest_start failed");
	return -EINVAL;

put_attr_fail:
	hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
	return -EINVAL;
}

/**
 * hdd_populate_wifi_ll_ext_peer_info() - put per peer info
 * @info: peer stats
 * @skb: vendor event buffer
 *
 * Return: 0 Success, EINVAL failure
 */
static int
hdd_populate_wifi_ll_ext_peer_info(struct sir_wifi_ll_ext_peer_stats *peers,
				   struct sk_buff *skb)
{
	uint32_t i;
	struct nlattr *wmm_ac;

	if (nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_ID,
			peers->peer_id) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_ID,
			peers->vdev_id) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_TIMES,
			peers->sta_ps_inds) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_DURATION,
			peers->sta_ps_durs) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PROBE_REQ,
			peers->rx_probe_reqs) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MGMT,
			peers->rx_oth_mgmts) ||
	    nla_put(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_MAC_ADDRESS,
		    QDF_MAC_ADDR_SIZE, peers->mac_address) ||
	    hdd_populate_wifi_signal_info(&peers->peer_signal_stats, skb)) {
		hdd_err("put peer signal attr failed");
		return -EINVAL;
	}

	wmm_ac = nla_nest_start(skb,
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_STATUS);
	if (!wmm_ac) {
		hdd_err("nla_nest_start failed");
		return -EINVAL;
	}

	for (i = 0; i < WLAN_MAX_AC; i++) {
		if (hdd_populate_wifi_wmm_ac_info(&peers->ac_stats[i], skb)) {
			hdd_err("put WMM AC attr failed");
			return -EINVAL;
		}
	}

	nla_nest_end(skb, wmm_ac);
	return 0;
}

/**
 * hdd_populate_wifi_ll_ext_stats() - put link layer extension stats
 * @info: link layer stats
 * @skb: vendor event buffer
 *
 * Return: 0 Success, EINVAL failure
 */
static int
hdd_populate_wifi_ll_ext_stats(struct sir_wifi_ll_ext_stats *stats,
			       struct sk_buff *skb)
{
	uint32_t i;
	struct nlattr *peer, *peer_info, *channels, *channel_info;

	if (nla_put_u32(skb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_EVENT_MODE,
			stats->trigger_cond_id) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CCA_BSS_BITMAP,
			stats->cca_chgd_bitmap) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_SIGNAL_BITMAP,
			stats->sig_chgd_bitmap) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BITMAP,
			stats->tx_chgd_bitmap) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BITMAP,
			stats->rx_chgd_bitmap) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CHANNEL_NUM,
			stats->channel_num) ||
	    nla_put_u32(skb,
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_NUM,
			stats->peer_num)) {
		goto put_attr_fail;
	}

	channels = nla_nest_start(skb,
				  QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CCA_BSS);
	if (!channels) {
		hdd_err("nla_nest_start failed");
		return -EINVAL;
	}

	for (i = 0; i < stats->channel_num; i++) {
		channel_info = nla_nest_start(skb, i);
		if (!channel_info) {
			hdd_err("nla_nest_start failed");
			return -EINVAL;
		}

		if (hdd_populate_wifi_channel_cca_info(&stats->cca[i], skb))
			goto put_attr_fail;
		nla_nest_end(skb, channel_info);
	}
	nla_nest_end(skb, channels);

	peer_info = nla_nest_start(skb,
				   QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER);
	if (!peer_info) {
		hdd_err("nla_nest_start failed");
		return -EINVAL;
	}

	for (i = 0; i < stats->peer_num; i++) {
		peer = nla_nest_start(skb, i);
		if (!peer) {
			hdd_err("nla_nest_start failed");
			return -EINVAL;
		}

		if (hdd_populate_wifi_ll_ext_peer_info(&stats->peer_stats[i],
						       skb))
			goto put_attr_fail;
		nla_nest_end(skb, peer);
	}

	nla_nest_end(skb, peer_info);
	return 0;

put_attr_fail:
	hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
	return -EINVAL;
}

/**
 * wlan_hdd_cfg80211_link_layer_stats_ext_callback() - Callback for LL ext
 * @ctx: HDD context
 * @rsp: msg from FW
 *
 * This function is an extension of
 * wlan_hdd_cfg80211_link_layer_stats_callback. It converts
 * monitoring parameters offloaded to NL data and send the same to the
 * kernel/upper layers.
 *
 * Return: None
 */
void wlan_hdd_cfg80211_link_layer_stats_ext_callback(hdd_handle_t ctx,
						     tSirLLStatsResults *rsp)
{
	struct hdd_context *hdd_ctx;
	struct sk_buff *skb;
	uint32_t param_id, index;
	struct hdd_adapter *adapter;
	struct wifi_peer_stat *peer_stats;
	uint8_t *results;
	int status;

	hdd_enter();

	if (!rsp) {
		hdd_err("Invalid result.");
		return;
	}

	hdd_ctx = hdd_handle_to_context(ctx);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status)
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, rsp->ifaceId);

	if (!adapter) {
		hdd_err("vdev_id %d does not exist with host.",
			rsp->ifaceId);
		return;
	}

	index = QCA_NL80211_VENDOR_SUBCMD_LL_STATS_EXT_INDEX;
	skb = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
			NULL, LL_STATS_EVENT_BUF_SIZE + NLMSG_HDRLEN,
			index, GFP_KERNEL);
	if (!skb) {
		hdd_err("cfg80211_vendor_event_alloc failed.");
		return;
	}

	results = rsp->results;
	param_id = rsp->paramId;
	hdd_info("LL_STATS RESP paramID = 0x%x, ifaceId = %u, result = %pK",
		 rsp->paramId, rsp->ifaceId, rsp->results);
	if (param_id & WMI_LL_STATS_EXT_PS_CHG) {
		peer_stats = (struct wifi_peer_stat *)results;
		status = hdd_populate_wifi_peer_ps_info(peer_stats, skb);
	} else if (param_id & WMI_LL_STATS_EXT_TX_FAIL) {
		struct sir_wifi_iface_tx_fail *tx_fail;

		tx_fail = (struct sir_wifi_iface_tx_fail *)results;
		status = hdd_populate_tx_failure_info(tx_fail, skb);
	} else if (param_id & WMI_LL_STATS_EXT_MAC_COUNTER) {
		hdd_info("MAC counters stats");
		status = hdd_populate_wifi_ll_ext_stats(
				(struct sir_wifi_ll_ext_stats *)
				rsp->results, skb);
	} else {
		hdd_info("Unknown link layer stats");
		status = -EINVAL;
	}

	if (status == 0)
		cfg80211_vendor_event(skb, GFP_KERNEL);
	else
		kfree_skb(skb);
	hdd_exit();
}

const struct nla_policy
qca_wlan_vendor_ll_ext_policy[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_CFG_PERIOD] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_CFG_THRESHOLD] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_CHG] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TID] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_NUM_MSDU] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_STATUS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_STATE] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_MAC_ADDRESS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_GLOBAL] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_EVENT_MODE] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_ID] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_ID] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BITMAP] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BITMAP] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CCA_BSS_BITMAP] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_SIGNAL_BITMAP] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_NUM] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CHANNEL_NUM] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CCA_BSS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MSDU] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MPDU] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_PPDU] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BYTES] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP_BYTES] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_RETRY] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_ACK] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_BACK] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_AGGR_NUM] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_SUCC_MCS_NUM] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_FAIL_MCS_NUM] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_AGGR] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_SUCC_MCS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_FAIL_MCS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_DELAY_ARRAY_SIZE] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DELAY] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_BYTES] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU_BYTES] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_LOST] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_RETRY] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DUP] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DISCARD] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_AGGR_NUM] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MCS_NUM] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MCS] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_AGGR] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_TIMES] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_DURATION] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PROBE_REQ] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MGMT] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IDLE_TIME] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_TIME] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_TIME] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BUSY] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BAD] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BAD] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_NO_AVAIL] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IN_BSS_TIME] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_OUT_BSS_TIME] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_ANT_NUM] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_SIGNAL] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_SNR] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_NF] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_RSSI_BEACON] = {
		.type = NLA_U32
	},
	[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IFACE_SNR_BEACON] = {
		.type = NLA_U32
	},
};

/**
 * __wlan_hdd_cfg80211_ll_stats_ext_set_param - config monitor parameters
 * @wiphy: wiphy handle
 * @wdev: wdev handle
 * @data: user layer input
 * @data_len: length of user layer input
 *
 * this function is called in ssr protected environment.
 *
 * return: 0 success, none zero for failure
 */
static int __wlan_hdd_cfg80211_ll_stats_ext_set_param(struct wiphy *wiphy,
						      struct wireless_dev *wdev,
						      const void *data,
						      int data_len)
{
	QDF_STATUS status;
	int errno;
	uint32_t period;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct sir_ll_ext_stats_threshold thresh = {0,};
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_MAX + 1];

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_warn("command not allowed in ftm mode");
		return -EPERM;
	}

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return -EPERM;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_MAX,
				    (struct nlattr *)data, data_len,
				    qca_wlan_vendor_ll_ext_policy)) {
		hdd_err("maximum attribute not present");
		return -EPERM;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_CFG_PERIOD]) {
		period = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CFG_PERIOD]);

		if (period != 0 && period < LL_STATS_MIN_PERIOD)
			period = LL_STATS_MIN_PERIOD;

		/*
		 * Only enable/disbale counters.
		 * Keep the last threshold settings.
		 */
		goto set_period;
	}

	/* global thresh is not enabled */
	if (!tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_CFG_THRESHOLD]) {
		thresh.global = false;
		hdd_warn("global thresh is not set");
	} else {
		thresh.global_threshold = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_CFG_THRESHOLD]);
		thresh.global = true;
		hdd_debug("globle thresh is %d", thresh.global_threshold);
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_GLOBAL]) {
		thresh.global = false;
		hdd_warn("global thresh is not enabled");
	} else {
		thresh.global = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_GLOBAL]);
		hdd_debug("global is %d", thresh.global);
	}

	thresh.enable_bitmap = false;
	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BITMAP]) {
		thresh.tx_bitmap = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BITMAP]);
		thresh.enable_bitmap = true;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BITMAP]) {
		thresh.rx_bitmap = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BITMAP]);
		thresh.enable_bitmap = true;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CCA_BSS_BITMAP]) {
		thresh.cca_bitmap = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_CCA_BSS_BITMAP]);
		thresh.enable_bitmap = true;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_SIGNAL_BITMAP]) {
		thresh.signal_bitmap = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_SIGNAL_BITMAP]);
		thresh.enable_bitmap = true;
	}

	if (!thresh.global && !thresh.enable_bitmap) {
		hdd_warn("threshold will be disabled.");
		thresh.enable = false;

		/* Just disable threshold */
		goto set_thresh;
	} else {
		thresh.enable = true;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MSDU]) {
		thresh.tx.msdu = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MSDU]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MPDU]) {
		thresh.tx.mpdu = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_MPDU]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_PPDU]) {
		thresh.tx.ppdu = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_PPDU]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BYTES]) {
		thresh.tx.bytes = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BYTES]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP]) {
		thresh.tx.msdu_drop = nla_get_u32(
			tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP_BYTES]) {
		thresh.tx.byte_drop = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DROP_BYTES]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_RETRY]) {
		thresh.tx.mpdu_retry = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_RETRY]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_ACK]) {
		thresh.tx.mpdu_fail = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_ACK]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_BACK]) {
		thresh.tx.ppdu_fail = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_NO_BACK]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_AGGR]) {
		thresh.tx.aggregation = nla_get_u32(tb[
				  QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_AGGR]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_SUCC_MCS]) {
		thresh.tx.succ_mcs = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_SUCC_MCS]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_FAIL_MCS]) {
		thresh.tx.fail_mcs = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_FAIL_MCS]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DELAY]) {
		thresh.tx.delay = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_DELAY]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU]) {
		thresh.rx.mpdu = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_BYTES]) {
		thresh.rx.bytes = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_BYTES]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU]) {
		thresh.rx.ppdu = nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU_BYTES]) {
		thresh.rx.ppdu_bytes = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PPDU_BYTES]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_LOST]) {
		thresh.rx.mpdu_lost = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_LOST]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_RETRY]) {
		thresh.rx.mpdu_retry = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_RETRY]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DUP]) {
		thresh.rx.mpdu_dup = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DUP]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DISCARD]) {
		thresh.rx.mpdu_discard = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MPDU_DISCARD]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_AGGR]) {
		thresh.rx.aggregation = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_AGGR]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MCS]) {
		thresh.rx.mcs = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MCS]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_TIMES]) {
		thresh.rx.ps_inds = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_TIMES]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_DURATION]) {
		thresh.rx.ps_durs = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_PEER_PS_DURATION]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PROBE_REQ]) {
		thresh.rx.probe_reqs = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_PROBE_REQ]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MGMT]) {
		thresh.rx.other_mgmt = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_MGMT]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IDLE_TIME]) {
		thresh.cca.idle_time = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IDLE_TIME]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_TIME]) {
		thresh.cca.tx_time = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_TIME]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IN_BSS_TIME]) {
		thresh.cca.rx_in_bss_time = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_IN_BSS_TIME]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_OUT_BSS_TIME]) {
		thresh.cca.rx_out_bss_time = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_OUT_BSS_TIME]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BUSY]) {
		thresh.cca.rx_busy_time = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BUSY]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BAD]) {
		thresh.cca.rx_in_bad_cond_time = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_RX_BAD]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BAD]) {
		thresh.cca.tx_in_bad_cond_time = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_TX_BAD]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_NO_AVAIL]) {
		thresh.cca.wlan_not_avail_time = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_NO_AVAIL]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_SNR]) {
		thresh.signal.snr = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_SNR]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_NF]) {
		thresh.signal.nf = nla_get_u32(tb[
			QCA_WLAN_VENDOR_ATTR_LL_STATS_EXT_ANT_NF]);
	}

set_thresh:
	hdd_info("send thresh settings to target");
	status = sme_ll_stats_set_thresh(hdd_ctx->mac_handle, &thresh);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("sme_ll_stats_set_thresh failed.");
		return -EINVAL;
	}
	return 0;

set_period:
	hdd_info("send period to target");
	errno = wma_cli_set_command(adapter->vdev_id,
				    WMI_PDEV_PARAM_STATS_OBSERVATION_PERIOD,
				    period, PDEV_CMD);
	if (errno) {
		hdd_err("wma_cli_set_command set_period failed.");
		return -EINVAL;
	}
	return 0;
}

/**
 * wlan_hdd_cfg80211_ll_stats_ext_set_param - config monitor parameters
 * @wiphy: wiphy handle
 * @wdev: wdev handle
 * @data: user layer input
 * @data_len: length of user layer input
 *
 * return: 0 success, einval failure
 */
int wlan_hdd_cfg80211_ll_stats_ext_set_param(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_ll_stats_ext_set_param(wiphy, wdev,
							   data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

#else
static QDF_STATUS wlan_hdd_stats_request_needed(struct hdd_adapter *adapter)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

#ifdef WLAN_FEATURE_STATS_EXT
/**
 * __wlan_hdd_cfg80211_stats_ext_request() - ext stats request
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: int
 */
static int __wlan_hdd_cfg80211_stats_ext_request(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	tStatsExtRequestReq stats_ext_req;
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	int ret_val;
	QDF_STATUS status;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);

	hdd_enter_dev(dev);

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	stats_ext_req.request_data_len = data_len;
	stats_ext_req.request_data = (void *)data;

	status = cdp_request_rx_hw_stats(soc, adapter->vdev_id);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err_rl("Failed to get hw stats: %u", status);
		ret_val = -EINVAL;
	}

	status = sme_stats_ext_request(adapter->vdev_id, &stats_ext_req);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err_rl("Failed to get fw stats: %u", status);
		ret_val = -EINVAL;
	}

	return ret_val;
}

/**
 * wlan_hdd_cfg80211_stats_ext_request() - ext stats request
 * @wiphy: Pointer to wiphy
 * @wdev: Pointer to wdev
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: int
 */
int wlan_hdd_cfg80211_stats_ext_request(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_stats_ext_request(wiphy, wdev,
						      data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

void wlan_hdd_cfg80211_stats_ext_callback(hdd_handle_t hdd_handle,
					  struct stats_ext_event *data)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	struct sk_buff *vendor_event;
	int status;
	int ret_val;
	struct hdd_adapter *adapter;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, data->vdev_id);
	if (!adapter) {
		hdd_err("vdev_id %d does not exist with host", data->vdev_id);
		return;
	}

	vendor_event = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
						   NULL,
						   data->event_data_len +
						   sizeof(uint32_t) +
						   NLMSG_HDRLEN + NLMSG_HDRLEN,
						   QCA_NL80211_VENDOR_SUBCMD_STATS_EXT_INDEX,
						   GFP_KERNEL);

	if (!vendor_event) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	ret_val = nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_IFINDEX,
			      adapter->dev->ifindex);
	if (ret_val) {
		hdd_err("QCA_WLAN_VENDOR_ATTR_IFINDEX put fail");
		kfree_skb(vendor_event);

		return;
	}

	ret_val = nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_STATS_EXT,
			  data->event_data_len, data->event_data);

	if (ret_val) {
		hdd_err("QCA_WLAN_VENDOR_ATTR_STATS_EXT put fail");
		kfree_skb(vendor_event);

		return;
	}

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

}

void
wlan_hdd_cfg80211_stats_ext2_callback(hdd_handle_t hdd_handle,
				      struct sir_sme_rx_aggr_hole_ind *pmsg)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	int status;
	uint32_t data_size, hole_info_size;
	struct sk_buff *vendor_event;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status)
		return;

	if (!pmsg) {
		hdd_err("msg received here is null");
		return;
	}

	hole_info_size = (pmsg->hole_cnt)*sizeof(pmsg->hole_info_array[0]);
	data_size = sizeof(struct sir_sme_rx_aggr_hole_ind) + hole_info_size;

	vendor_event = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
			NULL,
			data_size + NLMSG_HDRLEN + NLMSG_HDRLEN,
			QCA_NL80211_VENDOR_SUBCMD_STATS_EXT_INDEX,
			GFP_KERNEL);

	if (!vendor_event) {
		hdd_err("vendor_event_alloc failed for STATS_EXT2");
		return;
	}

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_RX_AGGREGATION_STATS_HOLES_NUM,
			pmsg->hole_cnt)) {
		hdd_err("%s put fail",
			"QCA_WLAN_VENDOR_ATTR_RX_AGGREGATION_STATS_HOLES_NUM");
		kfree_skb(vendor_event);
		return;
	}
	if (nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_RX_AGGREGATION_STATS_HOLES_INFO,
		    hole_info_size,
		    (void *)(pmsg->hole_info_array))) {
		hdd_err("%s put fail",
			"QCA_WLAN_VENDOR_ATTR_RX_AGGREGATION_STATS_HOLES_INFO");
		kfree_skb(vendor_event);
		return;
	}

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}

#else
void wlan_hdd_cfg80211_stats_ext_callback(hdd_handle_t hdd_handle,
					  struct stats_ext_event *data)
{
}

void
wlan_hdd_cfg80211_stats_ext2_callback(hdd_handle_t hdd_handle,
				      struct sir_sme_rx_aggr_hole_ind *pmsg)
{
}
#endif /* End of WLAN_FEATURE_STATS_EXT */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * enum roam_event_rt_info_reset - Reset the notif param value of struct
 * roam_event_rt_info to 0
 * @ROAM_EVENT_RT_INFO_RESET: Reset the value to 0
 */
enum roam_event_rt_info_reset {
	ROAM_EVENT_RT_INFO_RESET = 0,
};

/**
 * struct roam_ap - Roamed/Failed AP info
 * @num_cand: number of candidate APs
 * @bssid:    BSSID of roamed/failed AP
 * rssi:      RSSI of roamed/failed AP
 * freq:      Frequency of roamed/failed AP
 */
struct roam_ap {
	uint32_t num_cand;
	struct qdf_mac_addr bssid;
	int8_t rssi;
	uint16_t freq;
};

/**
 * hdd_get_roam_rt_stats_event_len() - calculate length of skb required for
 * sending roam events stats.
 * @roam_stats: pointer to mlme_roam_debug_info structure
 *
 * Return: length of skb
 */
static uint32_t
hdd_get_roam_rt_stats_event_len(struct mlme_roam_debug_info *roam_stats)
{
	uint32_t len = 0;
	uint8_t i = 0, num_cand = 0;

	/* QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_TRIGGER_REASON  */
	if (roam_stats->trigger.present)
		len += nla_total_size(sizeof(uint32_t));

	/* QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_INVOKE_FAIL_REASON */
	if (roam_stats->roam_event_param.roam_invoke_fail_reason)
		len += nla_total_size(sizeof(uint32_t));

	/* QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_STATE */
	if (roam_stats->roam_event_param.roam_scan_state)
		len += nla_total_size(sizeof(uint8_t));

	if (roam_stats->scan.present) {
		if (roam_stats->scan.num_chan && !roam_stats->scan.type)
			for (i = 0; i < roam_stats->scan.num_chan;)
				i++;

		/* QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_FREQ_LIST */
		len += (nla_total_size(sizeof(uint32_t)) * i);

		if (roam_stats->result.present &&
		    roam_stats->result.fail_reason) {
			num_cand++;
		} else if (roam_stats->trigger.present) {
			for (i = 0; i < roam_stats->scan.num_ap; i++) {
				if (roam_stats->scan.ap[i].type == 2)
					num_cand++;
			}
		}
		/* QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO */
		len += NLA_HDRLEN;
		/* QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_BSSID */
		len += (nla_total_size(QDF_MAC_ADDR_SIZE) * num_cand);
		/* QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_RSSI */
		len += (nla_total_size(sizeof(int32_t)) * num_cand);
		/* QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_FREQ */
		len += (nla_total_size(sizeof(uint32_t)) * num_cand);
		/* QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_FAIL_REASON */
		len += (nla_total_size(sizeof(uint32_t)) * num_cand);
	}

	/* QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_TYPE */
	if (len)
		len += nla_total_size(sizeof(uint32_t));

	return len;
}

#define SUBCMD_ROAM_EVENTS_INDEX \
	QCA_NL80211_VENDOR_SUBCMD_ROAM_EVENTS_INDEX
#define ROAM_SCAN_FREQ_LIST \
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_FREQ_LIST
#define ROAM_INVOKE_FAIL_REASON \
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_INVOKE_FAIL_REASON
#define ROAM_SCAN_STATE         QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_STATE
#define ROAM_EVENTS_CANDIDATE   QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO
#define CANDIDATE_BSSID \
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_BSSID
#define CANDIDATE_RSSI \
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_RSSI
#define CANDIDATE_FREQ \
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_FREQ
#define ROAM_FAIL_REASON \
	QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_CANDIDATE_INFO_FAIL_REASON

/**
 * roam_rt_stats_fill_scan_freq() - Fill the scan frequency list from the
 * roam stats event.
 * @vendor_event: pointer to sk_buff structure
 * @roam_stats:   pointer to mlme_roam_debug_info structure
 *
 * Return: none
 */
static void
roam_rt_stats_fill_scan_freq(struct sk_buff *vendor_event,
			     struct mlme_roam_debug_info *roam_stats)
{
	struct nlattr *nl_attr;
	uint8_t i;

	nl_attr = nla_nest_start(vendor_event, ROAM_SCAN_FREQ_LIST);
	if (!nl_attr) {
		hdd_err("nla nest start fail");
		kfree_skb(vendor_event);
		return;
	}
	if (roam_stats->scan.num_chan && !roam_stats->scan.type) {
		for (i = 0; i < roam_stats->scan.num_chan; i++) {
			if (nla_put_u32(vendor_event, i,
					roam_stats->scan.chan_freq[i])) {
				hdd_err("failed to put freq at index %d", i);
				kfree_skb(vendor_event);
				return;
			}
		}
	}
	nla_nest_end(vendor_event, nl_attr);
}

/**
 * roam_rt_stats_fill_cand_info() - Fill the roamed/failed AP info from the
 * roam stats event.
 * @vendor_event: pointer to sk_buff structure
 * @roam_stats:   pointer to mlme_roam_debug_info structure
 *
 * Return: none
 */
static void
roam_rt_stats_fill_cand_info(struct sk_buff *vendor_event,
			     struct mlme_roam_debug_info *roam_stats)
{
	struct nlattr *nl_attr, *nl_array;
	struct roam_ap cand_ap = {0};
	uint8_t i, num_cand = 0;

	if (roam_stats->result.present && roam_stats->result.fail_reason) {
		num_cand++;
		for (i = 0; i < roam_stats->scan.num_ap; i++) {
			if (roam_stats->scan.ap[i].type == 0 &&
			    qdf_is_macaddr_equal(&roam_stats->result.fail_bssid,
						 &roam_stats->
						 scan.ap[i].bssid)) {
				qdf_copy_macaddr(&cand_ap.bssid,
						 &roam_stats->scan.ap[i].bssid);
				cand_ap.rssi = roam_stats->scan.ap[i].rssi;
				cand_ap.freq = roam_stats->scan.ap[i].freq;
			}
		}
	} else if (roam_stats->trigger.present) {
		for (i = 0; i < roam_stats->scan.num_ap; i++) {
			if (roam_stats->scan.ap[i].type == 2) {
				num_cand++;
				qdf_copy_macaddr(&cand_ap.bssid,
						 &roam_stats->scan.ap[i].bssid);
				cand_ap.rssi = roam_stats->scan.ap[i].rssi;
				cand_ap.freq = roam_stats->scan.ap[i].freq;
			}
		}
	}

	nl_array = nla_nest_start(vendor_event, ROAM_EVENTS_CANDIDATE);
	if (!nl_array) {
		hdd_err("nl array nest start fail");
		kfree_skb(vendor_event);
		return;
	}
	for (i = 0; i < num_cand; i++) {
		nl_attr = nla_nest_start(vendor_event, i);
		if (!nl_attr) {
			hdd_err("nl attr nest start fail");
			kfree_skb(vendor_event);
			return;
		}
		if (nla_put(vendor_event, CANDIDATE_BSSID,
			    sizeof(cand_ap.bssid), cand_ap.bssid.bytes)) {
			hdd_err("%s put fail",
				"ROAM_EVENTS_CANDIDATE_INFO_BSSID");
			kfree_skb(vendor_event);
			return;
		}
		if (nla_put_s32(vendor_event, CANDIDATE_RSSI, cand_ap.rssi)) {
			hdd_err("%s put fail",
				"ROAM_EVENTS_CANDIDATE_INFO_RSSI");
			kfree_skb(vendor_event);
			return;
		}
		if (nla_put_u32(vendor_event, CANDIDATE_FREQ, cand_ap.freq)) {
			hdd_err("%s put fail",
				"ROAM_EVENTS_CANDIDATE_INFO_FREQ");
			kfree_skb(vendor_event);
			return;
		}
		if (roam_stats->result.present &&
		    roam_stats->result.fail_reason) {
			if (nla_put_u32(vendor_event, ROAM_FAIL_REASON,
					roam_stats->result.fail_reason)) {
				hdd_err("%s put fail",
					"ROAM_EVENTS_CANDIDATE_FAIL_REASON");
				kfree_skb(vendor_event);
				return;
			}
		}
		nla_nest_end(vendor_event, nl_attr);
	}
	nla_nest_end(vendor_event, nl_array);
}

void
wlan_hdd_cfg80211_roam_events_callback(hdd_handle_t hdd_handle,
				       struct mlme_roam_debug_info *roam_stats)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	int status;
	uint32_t data_size, roam_event_type = 0;
	struct sk_buff *vendor_event;
	struct hdd_adapter *adapter;

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status) {
		hdd_err("Invalid hdd_ctx");
		return;
	}

	if (!roam_stats) {
		hdd_err("msg received here is null");
		return;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx,
					  roam_stats->roam_event_param.vdev_id);
	if (!adapter) {
		hdd_err("vdev_id %d does not exist with host",
			roam_stats->roam_event_param.vdev_id);
		return;
	}

	data_size = hdd_get_roam_rt_stats_event_len(roam_stats);
	if (!data_size) {
		hdd_err("No data requested");
		return;
	}

	data_size += NLMSG_HDRLEN;
	vendor_event = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
						   &adapter->wdev,
						   data_size,
						   SUBCMD_ROAM_EVENTS_INDEX,
						   GFP_KERNEL);

	if (!vendor_event) {
		hdd_err("vendor_event_alloc failed for ROAM_EVENTS_STATS");
		return;
	}

	if (roam_stats->scan.present && roam_stats->trigger.present) {
		roam_rt_stats_fill_scan_freq(vendor_event, roam_stats);
		roam_rt_stats_fill_cand_info(vendor_event, roam_stats);
	}

	if (roam_stats->roam_event_param.roam_scan_state) {
		roam_event_type |= QCA_WLAN_VENDOR_ROAM_EVENT_ROAM_SCAN_STATE;
		if (nla_put_u8(vendor_event, ROAM_SCAN_STATE,
			       roam_stats->roam_event_param.roam_scan_state)) {
			hdd_err("%s put fail",
				"VENDOR_ATTR_ROAM_EVENTS_ROAM_SCAN_STATE");
			kfree_skb(vendor_event);
			return;
		}
		roam_stats->roam_event_param.roam_scan_state =
						ROAM_EVENT_RT_INFO_RESET;
	}
	if (roam_stats->trigger.present) {
		roam_event_type |= QCA_WLAN_VENDOR_ROAM_EVENT_TRIGGER_REASON;
		if (nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_TRIGGER_REASON,
				roam_stats->trigger.trigger_reason)) {
			hdd_err("%s put fail",
				"VENDOR_ATTR_ROAM_EVENTS_TRIGGER_REASON");
			kfree_skb(vendor_event);
			return;
		}
	}
	if (roam_stats->roam_event_param.roam_invoke_fail_reason) {
		roam_event_type |=
			QCA_WLAN_VENDOR_ROAM_EVENT_INVOKE_FAIL_REASON;
		if (nla_put_u32(vendor_event, ROAM_INVOKE_FAIL_REASON,
				roam_stats->
				roam_event_param.roam_invoke_fail_reason)) {
			hdd_err("%s put fail",
				"VENDOR_ATTR_ROAM_EVENTS_INVOKE_FAIL_REASON");
			kfree_skb(vendor_event);
			return;
		}
		roam_stats->roam_event_param.roam_invoke_fail_reason =
						ROAM_EVENT_RT_INFO_RESET;
	}
	if (roam_stats->result.present && roam_stats->result.fail_reason)
		roam_event_type |= QCA_WLAN_VENDOR_ROAM_EVENT_FAIL_REASON;

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_TYPE,
			roam_event_type)) {
		hdd_err("%s put fail", "QCA_WLAN_VENDOR_ATTR_ROAM_EVENTS_TYPE");
			kfree_skb(vendor_event);
		return;
	}

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
}

#undef SUBCMD_ROAM_EVENTS_INDEX
#undef ROAM_SCAN_FREQ_LIST
#undef ROAM_INVOKE_FAIL_REASON
#undef ROAM_SCAN_STATE
#undef ROAM_EVENTS_CANDIDATE
#undef CANDIDATE_BSSID
#undef CANDIDATE_RSSI
#undef CANDIDATE_FREQ
#undef ROAM_FAIL_REASON
#endif /* End of WLAN_FEATURE_ROAM_OFFLOAD */

#ifdef LINKSPEED_DEBUG_ENABLED
#define linkspeed_dbg(format, args...) pr_info(format, ## args)
#else
#define linkspeed_dbg(format, args...)
#endif /* LINKSPEED_DEBUG_ENABLED */

/**
 * wlan_hdd_fill_summary_stats() - populate station_info summary stats
 * @stats: summary stats to use as a source
 * @info: kernel station_info struct to use as a destination
 * @vdev_id: stats get from which vdev id
 *
 * Return: None
 */
static void wlan_hdd_fill_summary_stats(tCsrSummaryStatsInfo *stats,
					struct station_info *info,
					uint8_t vdev_id)
{
	int i;
	struct cds_vdev_dp_stats dp_stats;
	uint32_t orig_cnt;
	uint32_t orig_fail_cnt;

	info->rx_packets = stats->rx_frm_cnt;
	info->tx_packets = 0;
	info->tx_retries = 0;
	info->tx_failed = 0;

	for (i = 0; i < WIFI_MAX_AC; ++i) {
		info->tx_packets += stats->tx_frm_cnt[i];
		info->tx_retries += stats->multiple_retry_cnt[i];
		info->tx_failed += stats->fail_cnt[i];
	}

	if (cds_dp_get_vdev_stats(vdev_id, &dp_stats)) {
		orig_cnt = info->tx_retries;
		orig_fail_cnt = info->tx_failed;
		info->tx_retries = dp_stats.tx_retries;
		info->tx_failed += dp_stats.tx_mpdu_success_with_retries;
		hdd_debug("vdev %d tx retries adjust from %d to %d",
			  vdev_id, orig_cnt, info->tx_retries);
		hdd_debug("tx failed adjust from %d to %d",
			  orig_fail_cnt, info->tx_failed);
	}

	info->filled |= HDD_INFO_TX_PACKETS |
			HDD_INFO_TX_RETRIES |
			HDD_INFO_TX_FAILED  |
			HDD_INFO_RX_PACKETS;
}

/**
 * wlan_hdd_get_sap_stats() - get aggregate SAP stats
 * @adapter: sap adapter to get stats for
 * @info: kernel station_info struct to populate
 *
 * Fetch the vdev-level aggregate stats for the given SAP adapter. This is to
 * support "station dump" and "station get" for SAP vdevs, even though they
 * aren't technically stations.
 *
 * Return: errno
 */
static int
wlan_hdd_get_sap_stats(struct hdd_adapter *adapter, struct station_info *info)
{
	int ret;

	ret = wlan_hdd_get_station_stats(adapter);
	if (ret) {
		hdd_err("Failed to get SAP stats; status:%d", ret);
		return ret;
	}

	wlan_hdd_fill_summary_stats(&adapter->hdd_stats.summary_stat,
				    info,
				    adapter->vdev_id);

	return 0;
}

/**
 * hdd_get_max_rate_legacy() - get max rate for legacy mode
 * @stainfo: stainfo pointer
 * @rssidx: rssi index
 *
 * This function will get max rate for legacy mode
 *
 * Return: max rate on success, otherwise 0
 */
static uint32_t hdd_get_max_rate_legacy(struct hdd_station_info *stainfo,
					uint8_t rssidx)
{
	uint32_t maxrate = 0;
	/*Minimum max rate, 6Mbps*/
	int maxidx = 12;
	int i;

	/* check supported rates */
	if (stainfo->max_supp_idx != 0xff &&
	    maxidx < stainfo->max_supp_idx)
		maxidx = stainfo->max_supp_idx;

	/* check extended rates */
	if (stainfo->max_ext_idx != 0xff &&
	    maxidx < stainfo->max_ext_idx)
		maxidx = stainfo->max_ext_idx;

	for (i = 0; i < QDF_ARRAY_SIZE(supported_data_rate); i++) {
		if (supported_data_rate[i].beacon_rate_index == maxidx)
			maxrate =
				supported_data_rate[i].supported_rate[rssidx];
	}

	hdd_debug("maxrate %d", maxrate);

	return maxrate;
}

/**
 * hdd_get_max_rate_ht() - get max rate for ht mode
 * @stainfo: stainfo pointer
 * @stats: fw txrx status pointer
 * @rate_flags: rate flags
 * @nss: number of streams
 * @maxrate: returned max rate buffer pointer
 * @max_mcs_idx: max mcs idx
 * @report_max: report max rate or actual rate
 *
 * This function will get max rate for ht mode
 *
 * Return: None
 */
static void hdd_get_max_rate_ht(struct hdd_station_info *stainfo,
				struct hdd_fw_txrx_stats *stats,
				uint32_t rate_flags,
				uint8_t nss,
				uint32_t *maxrate,
				uint8_t *max_mcs_idx,
				bool report_max)
{
	struct index_data_rate_type *supported_mcs_rate;
	uint32_t tmprate;
	uint8_t flag = 0, mcsidx;
	int8_t rssi = stats->rssi;
	int mode;
	int i;

	if (rate_flags & TX_RATE_HT40)
		mode = 1;
	else
		mode = 0;

	if (rate_flags & TX_RATE_HT40)
		flag |= 1;
	if (rate_flags & TX_RATE_SGI)
		flag |= 2;

	supported_mcs_rate = (struct index_data_rate_type *)
		((nss == 1) ? &supported_mcs_rate_nss1 :
		 &supported_mcs_rate_nss2);

	if (stainfo->max_mcs_idx == 0xff) {
		hdd_err("invalid max_mcs_idx");
		/* report real mcs idx */
		mcsidx = stats->tx_rate.mcs;
	} else {
		mcsidx = stainfo->max_mcs_idx;
	}

	if (!report_max) {
		for (i = 0; i < mcsidx; i++) {
			if (rssi <= rssi_mcs_tbl[mode][i]) {
				mcsidx = i;
				break;
			}
		}
		if (mcsidx < stats->tx_rate.mcs)
			mcsidx = stats->tx_rate.mcs;
	}

	tmprate = supported_mcs_rate[mcsidx].supported_rate[flag];

	hdd_debug("tmprate %d mcsidx %d", tmprate, mcsidx);

	*maxrate = tmprate;
	*max_mcs_idx = mcsidx;
}

/**
 * hdd_get_max_rate_vht() - get max rate for vht mode
 * @stainfo: stainfo pointer
 * @stats: fw txrx status pointer
 * @rate_flags: rate flags
 * @nss: number of streams
 * @maxrate: returned max rate buffer pointer
 * @max_mcs_idx: max mcs idx
 * @report_max: report max rate or actual rate
 *
 * This function will get max rate for vht mode
 *
 * Return: None
 */
static void hdd_get_max_rate_vht(struct hdd_station_info *stainfo,
				 struct hdd_fw_txrx_stats *stats,
				 uint32_t rate_flags,
				 uint8_t nss,
				 uint32_t *maxrate,
				 uint8_t *max_mcs_idx,
				 bool report_max)
{
	struct index_vht_data_rate_type *supported_vht_mcs_rate;
	uint32_t tmprate = 0;
	uint32_t vht_max_mcs;
	uint8_t flag = 0, mcsidx = INVALID_MCS_IDX;
	int8_t rssi = stats->rssi;
	int mode;
	int i;

	supported_vht_mcs_rate = (struct index_vht_data_rate_type *)
		((nss == 1) ?
		 &supported_vht_mcs_rate_nss1 :
		 &supported_vht_mcs_rate_nss2);

	if (rate_flags & TX_RATE_VHT80)
		mode = 2;
	else if (rate_flags & TX_RATE_VHT40)
		mode = 1;
	else
		mode = 0;

	if (rate_flags &
	    (TX_RATE_VHT20 | TX_RATE_VHT40 | TX_RATE_VHT80)) {
		vht_max_mcs =
			(enum data_rate_11ac_max_mcs)
			(stainfo->tx_mcs_map & DATA_RATE_11AC_MCS_MASK);
		if (rate_flags & TX_RATE_SGI)
			flag |= 1;

		if (vht_max_mcs == DATA_RATE_11AC_MAX_MCS_7) {
			mcsidx = 7;
		} else if (vht_max_mcs == DATA_RATE_11AC_MAX_MCS_8) {
			mcsidx = 8;
		} else if (vht_max_mcs == DATA_RATE_11AC_MAX_MCS_9) {
			/*
			 * 'IEEE_P802.11ac_2013.pdf' page 325, 326
			 * - MCS9 is valid for VHT20 when Nss = 3 or Nss = 6
			 * - MCS9 is not valid for VHT20 when Nss = 1,2,4,5,7,8
			 */
			if ((rate_flags & TX_RATE_VHT20) &&
			    (nss != 3 && nss != 6))
				mcsidx = 8;
			else
				mcsidx = 9;
		} else {
			hdd_err("invalid vht_max_mcs");
			/* report real mcs idx */
			mcsidx = stats->tx_rate.mcs;
		}

		if (!report_max) {
			for (i = 0; i <= mcsidx; i++) {
				if (rssi <= rssi_mcs_tbl[mode][i]) {
					mcsidx = i;
					break;
				}
			}
			if (mcsidx < stats->tx_rate.mcs)
				mcsidx = stats->tx_rate.mcs;
		}

		if (rate_flags & TX_RATE_VHT80)
			tmprate =
		    supported_vht_mcs_rate[mcsidx].supported_VHT80_rate[flag];
		else if (rate_flags & TX_RATE_VHT40)
			tmprate =
		    supported_vht_mcs_rate[mcsidx].supported_VHT40_rate[flag];
		else if (rate_flags & TX_RATE_VHT20)
			tmprate =
		    supported_vht_mcs_rate[mcsidx].supported_VHT20_rate[flag];
	}

	hdd_debug("tmprate %d mcsidx %d", tmprate, mcsidx);

	*maxrate = tmprate;
	*max_mcs_idx = mcsidx;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0))
/**
 * hdd_fill_bw_mcs() - fill ch width and mcs flags
 * @rate_info: pointer to struct rate_info
 * @rate_flags: HDD rate flags
 * @mcsidx: mcs index
 * @nss: number of streams
 * @vht: vht mode or not
 *
 * This function will fill ch width and mcs flags
 *
 * Return: None
 */
static void hdd_fill_bw_mcs(struct rate_info *rate_info,
			    enum tx_rate_info rate_flags,
			    uint8_t mcsidx,
			    uint8_t nss,
			    bool vht)
{
	if (vht) {
		rate_info->nss = nss;
		rate_info->mcs = mcsidx;
		rate_info->flags |= RATE_INFO_FLAGS_VHT_MCS;
		if (rate_flags & TX_RATE_VHT80)
			rate_info->bw = RATE_INFO_BW_80;
		else if (rate_flags & TX_RATE_VHT40)
			rate_info->bw = RATE_INFO_BW_40;
		else if (rate_flags & TX_RATE_VHT20)
			rate_info->flags |= RATE_INFO_FLAGS_VHT_MCS;
	} else {
		rate_info->mcs = (nss - 1) << 3;
		rate_info->mcs |= mcsidx;
		rate_info->flags |= RATE_INFO_FLAGS_MCS;
		if (rate_flags & TX_RATE_HT40)
			rate_info->bw = RATE_INFO_BW_40;
	}
}
#else
/**
 * hdd_fill_bw_mcs() - fill ch width and mcs flags
 * @rate_info: pointer to struct rate_info
 * @rate_flags: HDD rate flags
 * @mcsidx: mcs index
 * @nss: number of streams
 * @vht: vht mode or not
 *
 * This function will fill ch width and mcs flags
 *
 * Return: None
 */
static void hdd_fill_bw_mcs(struct rate_info *rate_info,
			    enum tx_rate_info rate_flags,
			    uint8_t mcsidx,
			    uint8_t nss,
			    bool vht)
{
	if (vht) {
		rate_info->nss = nss;
		rate_info->mcs = mcsidx;
		rate_info->flags |= RATE_INFO_FLAGS_VHT_MCS;
		if (rate_flags & TX_RATE_VHT80)
			rate_info->flags |= RATE_INFO_FLAGS_80_MHZ_WIDTH;
		else if (rate_flags & TX_RATE_VHT40)
			rate_info->flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
		else if (rate_flags & TX_RATE_VHT20)
			rate_info->flags |= RATE_INFO_FLAGS_VHT_MCS;
	} else {
		rate_info->mcs = (nss - 1) << 3;
		rate_info->mcs |= mcsidx;
		rate_info->flags |= RATE_INFO_FLAGS_MCS;
		if (rate_flags & TX_RATE_HT40)
			rate_info->flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
	}
}
#endif

/**
 * hdd_fill_bw_mcs_vht() - fill ch width and mcs flags for VHT mode
 * @rate_info: pointer to struct rate_info
 * @rate_flags: HDD rate flags
 * @mcsidx: mcs index
 * @nss: number of streams
 *
 * This function will fill ch width and mcs flags for VHT mode
 *
 * Return: None
 */
static void hdd_fill_bw_mcs_vht(struct rate_info *rate_info,
				enum tx_rate_info rate_flags,
				uint8_t mcsidx,
				uint8_t nss)
{
	hdd_fill_bw_mcs(rate_info, rate_flags, mcsidx, nss, true);
}

/**
 * hdd_fill_sinfo_rate_info() - fill rate info of sinfo struct
 * @sinfo: pointer to struct station_info
 * @rate_flags: HDD rate flags
 * @mcsidx: mcs index
 * @nss: number of streams
 * @rate: data rate (kbps)
 * @is_tx: flag to indicate whether it is tx or rx
 *
 * This function will fill rate info of sinfo struct
 *
 * Return: None
 */
static void hdd_fill_sinfo_rate_info(struct station_info *sinfo,
				     uint32_t rate_flags,
				     uint8_t mcsidx,
				     uint8_t nss,
				     uint32_t rate,
				     bool is_tx)
{
	struct rate_info *rate_info;

	if (is_tx)
		rate_info = &sinfo->txrate;
	else
		rate_info = &sinfo->rxrate;

	if (rate_flags & TX_RATE_LEGACY) {
		/* provide to the UI in units of 100kbps */
		rate_info->legacy = rate;
	} else {
		/* must be MCS */
		if (rate_flags &
				(TX_RATE_VHT80 |
				 TX_RATE_VHT40 |
				 TX_RATE_VHT20))
			hdd_fill_bw_mcs_vht(rate_info, rate_flags, mcsidx, nss);

		if (rate_flags & (TX_RATE_HT20 | TX_RATE_HT40))
			hdd_fill_bw_mcs(rate_info, rate_flags, mcsidx, nss,
					false);

		if (rate_flags & TX_RATE_SGI) {
			if (!(rate_info->flags & RATE_INFO_FLAGS_VHT_MCS))
				rate_info->flags |= RATE_INFO_FLAGS_MCS;
			rate_info->flags |= RATE_INFO_FLAGS_SHORT_GI;
		}
	}

	hdd_info("flag %x mcs %d legacy %d nss %d",
		 rate_info->flags,
		 rate_info->mcs,
		 rate_info->legacy,
		 rate_info->nss);

	if (is_tx)
		sinfo->filled |= HDD_INFO_TX_BITRATE;
	else
		sinfo->filled |= HDD_INFO_RX_BITRATE;
}

/**
 * hdd_fill_sta_flags() - fill sta flags of sinfo
 * @sinfo: station_info struct pointer
 * @stainfo: stainfo pointer
 *
 * This function will fill sta flags of sinfo
 *
 * Return: None
 */
static void hdd_fill_sta_flags(struct station_info *sinfo,
			       struct hdd_station_info *stainfo)
{
	sinfo->sta_flags.mask = NL80211_STA_FLAG_WME;

	if (stainfo->is_qos_enabled)
		sinfo->sta_flags.set |= NL80211_STA_FLAG_WME;
	else
		sinfo->sta_flags.set &= ~NL80211_STA_FLAG_WME;

	sinfo->filled |= HDD_INFO_STA_FLAGS;
}

/**
 * hdd_fill_per_chain_avg_signal() - fill per chain avg rssi of sinfo
 * @sinfo: station_info struct pointer
 * @stainfo: stainfo pointer
 *
 * This function will fill per chain avg rssi of sinfo
 *
 * Return: None
 */
static void hdd_fill_per_chain_avg_signal(struct station_info *sinfo,
					  struct hdd_station_info *stainfo)
{
	bool rssi_stats_valid = false;
	uint8_t i;

	sinfo->signal_avg = WLAN_HDD_TGT_NOISE_FLOOR_DBM;
	for (i = 0; i < IEEE80211_MAX_CHAINS; i++) {
		sinfo->chain_signal_avg[i] = stainfo->peer_rssi_per_chain[i];
		sinfo->chains |= 1 << i;
		if (sinfo->chain_signal_avg[i] > sinfo->signal_avg &&
		    sinfo->chain_signal_avg[i] != 0)
			sinfo->signal_avg = sinfo->chain_signal_avg[i];

		if (sinfo->chain_signal_avg[i])
			rssi_stats_valid = true;
	}

	if (rssi_stats_valid) {
		sinfo->filled |= HDD_INFO_CHAIN_SIGNAL_AVG;
		sinfo->filled |= HDD_INFO_SIGNAL_AVG;
	}
}

/**
 * hdd_fill_rate_info() - fill rate info of sinfo
 * @psoc: psoc context
 * @sinfo: station_info struct pointer
 * @stainfo: stainfo pointer
 * @stats: fw txrx status pointer
 *
 * This function will fill rate info of sinfo
 *
 * Return: None
 */
static void hdd_fill_rate_info(struct wlan_objmgr_psoc *psoc,
			       struct station_info *sinfo,
			       struct hdd_station_info *stainfo,
			       struct hdd_fw_txrx_stats *stats)
{
	enum tx_rate_info rate_flags;
	uint8_t mcsidx = 0xff;
	uint32_t tx_rate, rx_rate, maxrate, tmprate;
	int rssidx;
	int nss = 1;
	int link_speed_rssi_high = 0;
	int link_speed_rssi_mid = 0;
	int link_speed_rssi_low = 0;
	uint32_t link_speed_rssi_report = 0;

	ucfg_mlme_stats_get_cfg_values(psoc,
				       &link_speed_rssi_high,
				       &link_speed_rssi_mid,
				       &link_speed_rssi_low,
				       &link_speed_rssi_report);

	hdd_info("reportMaxLinkSpeed %d", link_speed_rssi_report);

	/* convert to 100kbps expected in rate table */
	tx_rate = stats->tx_rate.rate / 100;
	rate_flags = stainfo->rate_flags;
	if (!(rate_flags & TX_RATE_LEGACY)) {
		nss = stainfo->nss;
		if (ucfg_mlme_stats_is_link_speed_report_actual(psoc)) {
			/* Get current rate flags if report actual */
			if (stats->tx_rate.rate_flags)
				rate_flags =
					stats->tx_rate.rate_flags;
			nss = stats->tx_rate.nss;
		}

		if (stats->tx_rate.mcs == INVALID_MCS_IDX)
			rate_flags = TX_RATE_LEGACY;
	}

	if (!ucfg_mlme_stats_is_link_speed_report_actual(psoc)) {
		/* we do not want to necessarily report the current speed */
		if (ucfg_mlme_stats_is_link_speed_report_max(psoc)) {
			/* report the max possible speed */
			rssidx = 0;
		} else if (ucfg_mlme_stats_is_link_speed_report_max_scaled(
					psoc)) {
			/* report the max possible speed with RSSI scaling */
			if (stats->rssi >= link_speed_rssi_high) {
				/* report the max possible speed */
				rssidx = 0;
			} else if (stats->rssi >= link_speed_rssi_mid) {
				/* report middle speed */
				rssidx = 1;
			} else if (stats->rssi >= link_speed_rssi_low) {
				/* report low speed */
				rssidx = 2;
			} else {
				/* report actual speed */
				rssidx = 3;
			}
		} else {
			/* unknown, treat as eHDD_LINK_SPEED_REPORT_MAX */
			hdd_err("Invalid value for reportMaxLinkSpeed: %u",
				link_speed_rssi_report);
			rssidx = 0;
		}

		maxrate = hdd_get_max_rate_legacy(stainfo, rssidx);

		/*
		 * Get MCS Rate Set --
		 * Only if we are connected in non legacy mode and not
		 * reporting actual speed
		 */
		if ((rssidx != 3) &&
		    !(rate_flags & TX_RATE_LEGACY)) {
			hdd_get_max_rate_vht(stainfo,
					     stats,
					     rate_flags,
					     nss,
					     &tmprate,
					     &mcsidx,
					     rssidx == 0);

			if (maxrate < tmprate &&
			    mcsidx != INVALID_MCS_IDX)
				maxrate = tmprate;

			if (mcsidx == INVALID_MCS_IDX)
				hdd_get_max_rate_ht(stainfo,
						    stats,
						    rate_flags,
						    nss,
						    &tmprate,
						    &mcsidx,
						    rssidx == 0);

			if (maxrate < tmprate &&
			    mcsidx != INVALID_MCS_IDX)
				maxrate = tmprate;
		} else if (!(rate_flags & TX_RATE_LEGACY)) {
			maxrate = tx_rate;
			mcsidx = stats->tx_rate.mcs;
		}

		/*
		 * make sure we report a value at least as big as our
		 * current rate
		 */
		if (maxrate < tx_rate || maxrate == 0) {
			maxrate = tx_rate;
			if (!(rate_flags & TX_RATE_LEGACY)) {
				mcsidx = stats->tx_rate.mcs;
				/*
				 * 'IEEE_P802.11ac_2013.pdf' page 325, 326
				 * - MCS9 is valid for VHT20 when Nss = 3 or
				 *   Nss = 6
				 * - MCS9 is not valid for VHT20 when
				 *   Nss = 1,2,4,5,7,8
				 */
				if ((rate_flags & TX_RATE_VHT20) &&
				    (mcsidx > 8) &&
				    (nss != 3 && nss != 6))
					mcsidx = 8;
			}
		}
	} else {
		/* report current rate instead of max rate */
		maxrate = tx_rate;
		if (!(rate_flags & TX_RATE_LEGACY))
			mcsidx = stats->tx_rate.mcs;
	}

	hdd_fill_sinfo_rate_info(sinfo, rate_flags, mcsidx, nss,
				 maxrate, true);

	/* convert to 100kbps expected in rate table */
	rx_rate = stats->rx_rate.rate / 100;

	/* report current rx rate*/
	rate_flags = stainfo->rate_flags;
	if (!(rate_flags & TX_RATE_LEGACY)) {
		if (stats->rx_rate.rate_flags)
			rate_flags = stats->rx_rate.rate_flags;
		nss = stats->rx_rate.nss;
		if (stats->rx_rate.mcs == INVALID_MCS_IDX)
			rate_flags = TX_RATE_LEGACY;
	}
	if (!(rate_flags & TX_RATE_LEGACY))
		mcsidx = stats->rx_rate.mcs;

	hdd_fill_sinfo_rate_info(sinfo, rate_flags, mcsidx, nss,
				 rx_rate, false);

	sinfo->expected_throughput = stainfo->max_phy_rate;
	sinfo->filled |= HDD_INFO_EXPECTED_THROUGHPUT;
}

/**
 * wlan_hdd_fill_station_info() - fill station_info struct
 * @psoc: psoc context
 * @sinfo: station_info struct pointer
 * @stainfo: stainfo pointer
 * @stats: fw txrx status pointer
 *
 * This function will fill station_info struct
 *
 * Return: None
 */
static void wlan_hdd_fill_station_info(struct wlan_objmgr_psoc *psoc,
				       struct station_info *sinfo,
				       struct hdd_station_info *stainfo,
				       struct hdd_fw_txrx_stats *stats)
{
	qdf_time_t curr_time, dur;

	curr_time = qdf_system_ticks();
	dur = curr_time - stainfo->assoc_ts;
	sinfo->connected_time = qdf_system_ticks_to_msecs(dur) / 1000;
	sinfo->filled |= HDD_INFO_CONNECTED_TIME;
	dur = curr_time - stainfo->last_tx_rx_ts;
	sinfo->inactive_time = qdf_system_ticks_to_msecs(dur);
	sinfo->filled |= HDD_INFO_INACTIVE_TIME;
	sinfo->signal = stats->rssi;
	sinfo->filled |= HDD_INFO_SIGNAL;
	sinfo->tx_bytes = stats->tx_bytes;
	sinfo->filled |= HDD_INFO_TX_BYTES | HDD_INFO_TX_BYTES64;
	sinfo->tx_packets = stats->tx_packets;
	sinfo->filled |= HDD_INFO_TX_PACKETS;
	sinfo->rx_bytes = stats->rx_bytes;
	sinfo->filled |= HDD_INFO_RX_BYTES | HDD_INFO_RX_BYTES64;
	sinfo->rx_packets = stats->rx_packets;
	sinfo->filled |= HDD_INFO_RX_PACKETS;
	sinfo->tx_failed = stats->tx_failed;
	sinfo->filled |= HDD_INFO_TX_FAILED;
	sinfo->tx_retries = stats->tx_retries;

	/* sta flags */
	hdd_fill_sta_flags(sinfo, stainfo);

	/* per chain avg rssi */
	hdd_fill_per_chain_avg_signal(sinfo, stainfo);

	/* tx / rx rate info */
	hdd_fill_rate_info(psoc, sinfo, stainfo, stats);

	/* assoc req ies */
	sinfo->assoc_req_ies = stainfo->assoc_req_ies.data;
	sinfo->assoc_req_ies_len = stainfo->assoc_req_ies.len;

	/* dump sta info*/
	hdd_info("dump stainfo");
	hdd_info("con_time %d inact_time %d tx_pkts %d rx_pkts %d",
		 sinfo->connected_time, sinfo->inactive_time,
		 sinfo->tx_packets, sinfo->rx_packets);
	hdd_info("failed %d retries %d tx_bytes %lld rx_bytes %lld",
		 sinfo->tx_failed, sinfo->tx_retries,
		 sinfo->tx_bytes, sinfo->rx_bytes);
	hdd_info("rssi %d tx mcs %d legacy %d nss %d flags %x",
		 sinfo->signal, sinfo->txrate.mcs,
		 sinfo->txrate.legacy, sinfo->txrate.nss,
		 sinfo->txrate.flags);
	hdd_info("rx mcs %d legacy %d nss %d flags %x",
		 sinfo->rxrate.mcs, sinfo->rxrate.legacy,
		 sinfo->rxrate.nss, sinfo->rxrate.flags);
}

/**
 * hdd_get_rate_flags_ht() - get HT rate flags based on rate, nss and mcs
 * @rate: Data rate (100 kbps)
 * @nss: Number of streams
 * @mcs: HT mcs index
 *
 * This function is used to construct HT rate flag with rate, nss and mcs
 *
 * Return: rate flags for success, 0 on failure.
 */
static uint8_t hdd_get_rate_flags_ht(uint32_t rate,
				     uint8_t nss,
				     uint8_t mcs)
{
	struct index_data_rate_type *mcs_rate;
	uint8_t flags = 0;

	mcs_rate = (struct index_data_rate_type *)
		((nss == 1) ? &supported_mcs_rate_nss1 :
		 &supported_mcs_rate_nss2);

	if (rate == mcs_rate[mcs].supported_rate[0]) {
		flags |= TX_RATE_HT20;
	} else if (rate == mcs_rate[mcs].supported_rate[1]) {
		flags |= TX_RATE_HT40;
	} else if (rate == mcs_rate[mcs].supported_rate[2]) {
		flags |= TX_RATE_HT20;
		flags |= TX_RATE_SGI;
	} else if (rate == mcs_rate[mcs].supported_rate[3]) {
		flags |= TX_RATE_HT40;
		flags |= TX_RATE_SGI;
	} else {
		hdd_err("invalid params rate %d nss %d mcs %d",
			rate, nss, mcs);
	}

	return flags;
}

/**
 * hdd_get_rate_flags_vht() - get VHT rate flags based on rate, nss and mcs
 * @rate: Data rate (100 kbps)
 * @nss: Number of streams
 * @mcs: VHT mcs index
 *
 * This function is used to construct VHT rate flag with rate, nss and mcs
 *
 * Return: rate flags for success, 0 on failure.
 */
static uint8_t hdd_get_rate_flags_vht(uint32_t rate,
				      uint8_t nss,
				      uint8_t mcs)
{
	struct index_vht_data_rate_type *mcs_rate;
	uint8_t flags = 0;

	mcs_rate = (struct index_vht_data_rate_type *)
		((nss == 1) ?
		 &supported_vht_mcs_rate_nss1 :
		 &supported_vht_mcs_rate_nss2);

	if (rate == mcs_rate[mcs].supported_VHT80_rate[0]) {
		flags |= TX_RATE_VHT80;
	} else if (rate == mcs_rate[mcs].supported_VHT80_rate[1]) {
		flags |= TX_RATE_VHT80;
		flags |= TX_RATE_SGI;
	} else if (rate == mcs_rate[mcs].supported_VHT40_rate[0]) {
		flags |= TX_RATE_VHT40;
	} else if (rate == mcs_rate[mcs].supported_VHT40_rate[1]) {
		flags |= TX_RATE_VHT40;
		flags |= TX_RATE_SGI;
	} else if (rate == mcs_rate[mcs].supported_VHT20_rate[0]) {
		flags |= TX_RATE_VHT20;
	} else if (rate == mcs_rate[mcs].supported_VHT20_rate[1]) {
		flags |= TX_RATE_VHT20;
		flags |= TX_RATE_SGI;
	} else {
		hdd_err("invalid params rate %d nss %d mcs %d",
			rate, nss, mcs);
	}

	return flags;
}

/**
 * hdd_get_rate_flags() - get HT/VHT rate flags based on rate, nss and mcs
 * @rate: Data rate (100 kbps)
 * @mode: Tx/Rx mode
 * @nss: Number of streams
 * @mcs: Mcs index
 *
 * This function is used to construct rate flag with rate, nss and mcs
 *
 * Return: rate flags for success, 0 on failure.
 */
static uint8_t hdd_get_rate_flags(uint32_t rate,
				  uint8_t mode,
				  uint8_t nss,
				  uint8_t mcs)
{
	uint8_t flags = 0;

	if (mode == SIR_SME_PHY_MODE_HT)
		flags = hdd_get_rate_flags_ht(rate, nss, mcs);
	else if (mode == SIR_SME_PHY_MODE_VHT)
		flags = hdd_get_rate_flags_vht(rate, nss, mcs);
	else
		hdd_err("invalid mode param %d", mode);

	return flags;
}

/**
 * wlan_hdd_fill_rate_info() - fill HDD rate info from peer info
 * @txrx_stats: pointer to txrx stats to be filled with rate info
 * @peer_info: peer info pointer
 *
 * This function is used to fill HDD rate info from peer info
 *
 * Return: None
 */
static void wlan_hdd_fill_rate_info(struct hdd_fw_txrx_stats *txrx_stats,
				    struct peer_stats_info_ext_event *peer_info)
{
	uint8_t flags;
	uint32_t rate_code;

	/* tx rate info */
	txrx_stats->tx_rate.rate = peer_info->tx_rate;
	rate_code = peer_info->tx_rate_code;

	if ((WMI_GET_HW_RATECODE_PREAM_V1(rate_code)) ==
			WMI_RATE_PREAMBLE_HT)
		txrx_stats->tx_rate.mode = SIR_SME_PHY_MODE_HT;
	else if ((WMI_GET_HW_RATECODE_PREAM_V1(rate_code)) ==
			WMI_RATE_PREAMBLE_VHT)
		txrx_stats->tx_rate.mode = SIR_SME_PHY_MODE_VHT;
	else
		txrx_stats->tx_rate.mode = SIR_SME_PHY_MODE_LEGACY;

	txrx_stats->tx_rate.nss = WMI_GET_HW_RATECODE_NSS_V1(rate_code) + 1;
	txrx_stats->tx_rate.mcs = WMI_GET_HW_RATECODE_RATE_V1(rate_code);

	flags = hdd_get_rate_flags(txrx_stats->tx_rate.rate / 100,
				   txrx_stats->tx_rate.mode,
				   txrx_stats->tx_rate.nss,
				   txrx_stats->tx_rate.mcs);

	txrx_stats->tx_rate.rate_flags = flags;

	hdd_debug("tx: mode %d nss %d mcs %d rate_flags %x flags %x",
		  txrx_stats->tx_rate.mode,
		  txrx_stats->tx_rate.nss,
		  txrx_stats->tx_rate.mcs,
		  txrx_stats->tx_rate.rate_flags,
		  flags);

	/* rx rate info */
	txrx_stats->rx_rate.rate = peer_info->rx_rate;
	rate_code = peer_info->rx_rate_code;

	if ((WMI_GET_HW_RATECODE_PREAM_V1(rate_code)) ==
			WMI_RATE_PREAMBLE_HT)
		txrx_stats->rx_rate.mode = SIR_SME_PHY_MODE_HT;
	else if ((WMI_GET_HW_RATECODE_PREAM_V1(rate_code)) ==
			WMI_RATE_PREAMBLE_VHT)
		txrx_stats->rx_rate.mode = SIR_SME_PHY_MODE_VHT;
	else
		txrx_stats->rx_rate.mode = SIR_SME_PHY_MODE_LEGACY;

	txrx_stats->rx_rate.nss = WMI_GET_HW_RATECODE_NSS_V1(rate_code) + 1;
	txrx_stats->rx_rate.mcs = WMI_GET_HW_RATECODE_RATE_V1(rate_code);

	flags = hdd_get_rate_flags(txrx_stats->rx_rate.rate / 100,
				   txrx_stats->rx_rate.mode,
				   txrx_stats->rx_rate.nss,
				   txrx_stats->rx_rate.mcs);

	txrx_stats->rx_rate.rate_flags = flags;

	hdd_info("rx: mode %d nss %d mcs %d rate_flags %x flags %x",
		 txrx_stats->rx_rate.mode,
		 txrx_stats->rx_rate.nss,
		 txrx_stats->rx_rate.mcs,
		 txrx_stats->rx_rate.rate_flags,
		 flags);
}

/**
 * wlan_hdd_get_station_remote() - NL80211_CMD_GET_STATION handler for SoftAP
 * @wiphy: pointer to wiphy
 * @dev: pointer to net_device structure
 * @mac: request peer mac address
 * @sinfo: pointer to station_info struct
 *
 * This function will get remote peer info from fw and fill sinfo struct
 *
 * Return: 0 on success, otherwise error value
 */
static int wlan_hdd_get_station_remote(struct wiphy *wiphy,
				       struct net_device *dev,
				       const u8 *mac,
				       struct station_info *sinfo)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hddctx = wiphy_priv(wiphy);
	struct hdd_station_info *stainfo = NULL;
	struct stats_event *stats;
	struct hdd_fw_txrx_stats txrx_stats;
	int i, status;

	status = wlan_hdd_validate_context(hddctx);
	if (status != 0)
		return status;

	hdd_debug("Peer "QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(mac));

	stainfo = hdd_get_sta_info_by_mac(&adapter->sta_info_list, mac,
					  STA_INFO_WLAN_HDD_GET_STATION_REMOTE);
	if (!stainfo) {
		hdd_err("peer "QDF_MAC_ADDR_FMT" not found",
			QDF_MAC_ADDR_REF(mac));
		return -EINVAL;
	}

	stats = wlan_cfg80211_mc_cp_stats_get_peer_stats(adapter->vdev,
							 mac, &status);
	if (status || !stats) {
		wlan_cfg80211_mc_cp_stats_free_stats_event(stats);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &stainfo, true,
				     STA_INFO_WLAN_HDD_GET_STATION_REMOTE);
		hdd_err("fail to get peer info from fw");
		return -EPERM;
	}

	for (i = 0; i < WMI_MAX_CHAINS; i++)
		stainfo->peer_rssi_per_chain[i] =
			    stats->peer_stats_info_ext->peer_rssi_per_chain[i] +
			    WLAN_HDD_TGT_NOISE_FLOOR_DBM;

	qdf_mem_zero(&txrx_stats, sizeof(txrx_stats));
	txrx_stats.tx_packets = stats->peer_stats_info_ext->tx_packets;
	txrx_stats.tx_bytes = stats->peer_stats_info_ext->tx_bytes;
	txrx_stats.rx_packets = stats->peer_stats_info_ext->rx_packets;
	txrx_stats.rx_bytes = stats->peer_stats_info_ext->rx_bytes;
	txrx_stats.tx_retries = stats->peer_stats_info_ext->tx_retries;
	txrx_stats.tx_failed = stats->peer_stats_info_ext->tx_failed;
	txrx_stats.tx_succeed = stats->peer_stats_info_ext->tx_succeed;
	txrx_stats.rssi = stats->peer_stats_info_ext->rssi
			+ WLAN_HDD_TGT_NOISE_FLOOR_DBM;
	wlan_hdd_fill_rate_info(&txrx_stats, stats->peer_stats_info_ext);
	wlan_hdd_fill_station_info(hddctx->psoc, sinfo, stainfo, &txrx_stats);
	wlan_cfg80211_mc_cp_stats_free_stats_event(stats);
	hdd_put_sta_info_ref(&adapter->sta_info_list, &stainfo, true,
			     STA_INFO_WLAN_HDD_GET_STATION_REMOTE);

	return status;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)) && \
	defined(WLAN_FEATURE_11AX)
/**
 * hdd_map_he_gi_to_os() - map txrate_gi to os guard interval
 * @guard_interval: guard interval get from fw rate
 *
 * Return: os guard interval value
 */
static inline uint8_t hdd_map_he_gi_to_os(enum txrate_gi guard_interval)
{
	switch (guard_interval) {
	case TXRATE_GI_0_8_US:
		return NL80211_RATE_INFO_HE_GI_0_8;
	case TXRATE_GI_1_6_US:
		return NL80211_RATE_INFO_HE_GI_1_6;
	case TXRATE_GI_3_2_US:
		return NL80211_RATE_INFO_HE_GI_3_2;
	default:
		return NL80211_RATE_INFO_HE_GI_0_8;
	}
}

/**
 * wlan_hdd_fill_os_he_rateflags() - Fill HE related rate_info
 * @os_rate: rate info for os
 * @rate_flags: rate flags
 * @dcm: dcm from rate
 * @guard_interval: guard interval from rate
 *
 * Return: none
 */
static void wlan_hdd_fill_os_he_rateflags(struct rate_info *os_rate,
					  enum tx_rate_info rate_flags,
					  uint8_t dcm,
					  enum txrate_gi guard_interval)
{
	/* as fw not yet report ofdma to host, so we doesn't
	 * fill RATE_INFO_BW_HE_RU.
	 */
	if (rate_flags & (TX_RATE_HE80 | TX_RATE_HE40 |
		TX_RATE_HE20 | TX_RATE_HE160)) {
		if (rate_flags & TX_RATE_HE160)
			hdd_set_rate_bw(os_rate, HDD_RATE_BW_160);
		else if (rate_flags & TX_RATE_HE80)
			hdd_set_rate_bw(os_rate, HDD_RATE_BW_80);
		else if (rate_flags & TX_RATE_HE40)
			hdd_set_rate_bw(os_rate, HDD_RATE_BW_40);

		os_rate->flags |= RATE_INFO_FLAGS_HE_MCS;

		os_rate->he_gi = hdd_map_he_gi_to_os(guard_interval);
		os_rate->he_dcm = dcm;
	}
}
#else
static void wlan_hdd_fill_os_he_rateflags(struct rate_info *os_rate,
					  enum tx_rate_info rate_flags,
					  uint8_t dcm,
					  enum txrate_gi guard_interval)
{}
#endif

/**
 * wlan_hdd_fill_os_rate_info() - Fill os related rate_info
 * @rate_flags: rate flags
 * @legacy_rate: 802.11abg rate
 * @os_rate: rate info for os
 * @mcs_index: mcs
 * @dcm: dcm from rate
 * @guard_interval: guard interval from rate
 *
 * Return: none
 */
static void wlan_hdd_fill_os_rate_info(enum tx_rate_info rate_flags,
				       uint16_t legacy_rate,
				       struct rate_info *os_rate,
				       uint8_t mcs_index, uint8_t nss,
				       uint8_t dcm,
				       enum txrate_gi guard_interval)
{
	if (rate_flags & TX_RATE_LEGACY) {
		os_rate->legacy = legacy_rate;
		hdd_debug("Reporting legacy rate %d", os_rate->legacy);
		return;
	}

	/* assume basic BW. anything else will override this later */
	hdd_set_rate_bw(os_rate, HDD_RATE_BW_20);
	os_rate->mcs = mcs_index;
	os_rate->nss = nss;

	wlan_hdd_fill_os_he_rateflags(os_rate, rate_flags, dcm, guard_interval);

	if (rate_flags & (TX_RATE_VHT160 | TX_RATE_VHT80 | TX_RATE_VHT40 |
	    TX_RATE_VHT20)) {
		if (rate_flags & TX_RATE_VHT160)
			hdd_set_rate_bw(os_rate, HDD_RATE_BW_160);
		else if (rate_flags & TX_RATE_VHT80)
			hdd_set_rate_bw(os_rate, HDD_RATE_BW_80);
		else if (rate_flags & TX_RATE_VHT40)
			hdd_set_rate_bw(os_rate, HDD_RATE_BW_40);
		os_rate->flags |= RATE_INFO_FLAGS_VHT_MCS;
	}

	if (rate_flags & (TX_RATE_HT20 | TX_RATE_HT40)) {
		if (rate_flags & TX_RATE_HT40)
			hdd_set_rate_bw(os_rate,
					HDD_RATE_BW_40);
		os_rate->flags |= RATE_INFO_FLAGS_MCS;
	}

	if (rate_flags & TX_RATE_SGI)
		os_rate->flags |= RATE_INFO_FLAGS_SHORT_GI;
}

bool hdd_report_max_rate(struct hdd_adapter *adapter,
			 mac_handle_t mac_handle,
			 struct rate_info *rate,
			 int8_t signal,
			 enum tx_rate_info rate_flags,
			 uint8_t mcs_index,
			 uint16_t fw_rate, uint8_t nss)
{
	uint8_t i, j, rssidx = 0;
	uint16_t max_rate = 0;
	uint32_t vht_mcs_map;
	bool is_vht20_mcs9 = false;
	uint16_t he_mcs_12_13_map = 0;
	uint16_t current_rate = 0;
	qdf_size_t or_leng = CSR_DOT11_SUPPORTED_RATES_MAX;
	uint8_t operational_rates[CSR_DOT11_SUPPORTED_RATES_MAX];
	uint8_t extended_rates[CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX];
	qdf_size_t er_leng = CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX;
	uint8_t mcs_rates[SIZE_OF_BASIC_MCS_SET];
	qdf_size_t mcs_leng = SIZE_OF_BASIC_MCS_SET;
	struct index_data_rate_type *supported_mcs_rate;
	enum data_rate_11ac_max_mcs vht_max_mcs;
	uint8_t max_mcs_idx = 0;
	uint8_t rate_flag = 1;
	int mode = 0, max_ht_idx;
	QDF_STATUS stat = QDF_STATUS_E_FAILURE;
	struct hdd_context *hdd_ctx;
	int link_speed_rssi_high = 0;
	int link_speed_rssi_mid = 0;
	int link_speed_rssi_low = 0;
	uint32_t link_speed_rssi_report = 0;
	struct wlan_objmgr_vdev *vdev;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return false;
	}

	ucfg_mlme_stats_get_cfg_values(hdd_ctx->psoc,
				       &link_speed_rssi_high,
				       &link_speed_rssi_mid,
				       &link_speed_rssi_low,
				       &link_speed_rssi_report);

	if (ucfg_mlme_stats_is_link_speed_report_max_scaled(hdd_ctx->psoc)) {
		/* report the max possible speed with RSSI scaling */
		if (signal >= link_speed_rssi_high) {
			/* report the max possible speed */
			rssidx = 0;
		} else if (signal >= link_speed_rssi_mid) {
			/* report middle speed */
			rssidx = 1;
		} else if (signal >= link_speed_rssi_low) {
			/* report middle speed */
			rssidx = 2;
		} else {
			/* report actual speed */
			rssidx = 3;
		}
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("failed to get vdev");
		return false;
	}

	/* Get Basic Rate Set */
	if (0 != ucfg_mlme_get_opr_rate(vdev, operational_rates,
					&or_leng)) {
		hdd_err("cfg get returned failure");
		hdd_objmgr_put_vdev(vdev);
		/*To keep GUI happy */
		return false;
	}

	for (i = 0; i < or_leng; i++) {
		for (j = 0;
			 j < ARRAY_SIZE(supported_data_rate); j++) {
			/* Validate Rate Set */
			if (supported_data_rate[j].beacon_rate_index ==
				(operational_rates[i] & 0x7F)) {
				current_rate =
					supported_data_rate[j].
					supported_rate[rssidx];
				break;
			}
		}
		/* Update MAX rate */
		max_rate = (current_rate > max_rate) ? current_rate : max_rate;
	}

	/* Get Extended Rate Set */
	if (0 != ucfg_mlme_get_ext_opr_rate(vdev, extended_rates,
					    &er_leng)) {
		hdd_err("cfg get returned failure");
		hdd_objmgr_put_vdev(vdev);
		/*To keep GUI happy */
		return false;
	}

	he_mcs_12_13_map = wlan_vdev_mlme_get_he_mcs_12_13_map(vdev);

	hdd_objmgr_put_vdev(vdev);

	for (i = 0; i < er_leng; i++) {
		for (j = 0; j < ARRAY_SIZE(supported_data_rate); j++) {
			if (supported_data_rate[j].beacon_rate_index ==
			    (extended_rates[i] & 0x7F)) {
				current_rate = supported_data_rate[j].
					       supported_rate[rssidx];
				break;
			}
		}
		/* Update MAX rate */
		max_rate = (current_rate > max_rate) ? current_rate : max_rate;
	}
	/* Get MCS Rate Set --
	 * Only if we are connected in non legacy mode and not reporting
	 * actual speed
	 */
	if ((3 != rssidx) && !(rate_flags & TX_RATE_LEGACY)) {
		if (0 != ucfg_mlme_get_current_mcs_set(hdd_ctx->psoc,
						       mcs_rates,
						       &mcs_leng)) {
			hdd_err("cfg get returned failure");
			/*To keep GUI happy */
			return false;
		}
		rate_flag = 0;

		if (rate_flags & (TX_RATE_VHT80 | TX_RATE_HE80 |
				TX_RATE_HE160 | TX_RATE_VHT160))
			mode = 2;
		else if (rate_flags & (TX_RATE_HT40 |
			 TX_RATE_VHT40 | TX_RATE_HE40))
			mode = 1;
		else
			mode = 0;

		if (rate_flags & (TX_RATE_VHT20 | TX_RATE_VHT40 |
		    TX_RATE_VHT80 | TX_RATE_HE20 | TX_RATE_HE40 |
		    TX_RATE_HE80 | TX_RATE_HE160 | TX_RATE_VHT160)) {
			stat = ucfg_mlme_cfg_get_vht_tx_mcs_map(hdd_ctx->psoc,
								&vht_mcs_map);
			if (QDF_IS_STATUS_ERROR(stat))
				hdd_err("failed to get tx_mcs_map");

			stat = ucfg_mlme_get_vht20_mcs9(hdd_ctx->psoc,
							&is_vht20_mcs9);
			if (QDF_IS_STATUS_ERROR(stat))
				hdd_err("Failed to get VHT20 MCS9 enable val");

			vht_max_mcs = (enum data_rate_11ac_max_mcs)
				(vht_mcs_map & DATA_RATE_11AC_MCS_MASK);
			if (rate_flags & TX_RATE_SGI)
				rate_flag |= 1;

			if (DATA_RATE_11AC_MAX_MCS_7 == vht_max_mcs) {
				max_mcs_idx = 7;
			} else if (DATA_RATE_11AC_MAX_MCS_8 == vht_max_mcs) {
				max_mcs_idx = 8;
			} else if (DATA_RATE_11AC_MAX_MCS_9 == vht_max_mcs) {
				/*
				 * If the ini enable_vht20_mcs9 is disabled,
				 * then max mcs index should not be set to 9
				 * for TX_RATE_VHT20
				 */
				if (!is_vht20_mcs9 &&
				    (rate_flags & TX_RATE_VHT20))
					max_mcs_idx = 8;
				else
					max_mcs_idx = 9;
			}

			if (rate_flags & (TX_RATE_HE20 | TX_RATE_HE40 |
			    TX_RATE_HE80 | TX_RATE_HE160)) {
				max_mcs_idx = 11;
				if (he_mcs_12_13_map)
					max_mcs_idx = 13;
			}

			if (rssidx != 0) {
				for (i = 0; i <= max_mcs_idx; i++) {
					if (signal <= rssi_mcs_tbl[mode][i]) {
						max_mcs_idx = i;
						break;
					}
				}
			}

			max_mcs_idx = (max_mcs_idx > mcs_index) ?
				max_mcs_idx : mcs_index;
		} else {
			if (rate_flags & TX_RATE_HT40)
				rate_flag |= 1;
			if (rate_flags & TX_RATE_SGI)
				rate_flag |= 2;

			supported_mcs_rate =
				(struct index_data_rate_type *)
				((nss == 1) ? &supported_mcs_rate_nss1 :
				 &supported_mcs_rate_nss2);

			max_ht_idx = MAX_HT_MCS_IDX;
			if (rssidx != 0) {
				for (i = 0; i < MAX_HT_MCS_IDX; i++) {
					if (signal <= rssi_mcs_tbl[mode][i]) {
						max_ht_idx = i + 1;
						break;
					}
				}
			}

			for (i = 0; i < mcs_leng; i++) {
				for (j = 0; j < max_ht_idx; j++) {
					if (supported_mcs_rate[j].
						beacon_rate_index ==
						mcs_rates[i]) {
						current_rate =
						  supported_mcs_rate[j].
						  supported_rate
						  [rate_flag];
						max_mcs_idx =
						  supported_mcs_rate[j].
						  beacon_rate_index;
						break;
					}
				}

				if ((j < MAX_HT_MCS_IDX) &&
				    (current_rate > max_rate))
					max_rate = current_rate;
			}

			if (nss == 2)
				max_mcs_idx += MAX_HT_MCS_IDX;
			max_mcs_idx = (max_mcs_idx > mcs_index) ?
				max_mcs_idx : mcs_index;
		}
	}

	else if (!(rate_flags & TX_RATE_LEGACY)) {
		max_rate = fw_rate;
		max_mcs_idx = mcs_index;
	}
	/* report a value at least as big as current rate */
	if ((max_rate < fw_rate) || (0 == max_rate)) {
		max_rate = fw_rate;
	}
	hdd_debug("RLMS %u, rate_flags 0x%x, max_rate %d mcs %d nss %d",
		  link_speed_rssi_report, rate_flags,
		  max_rate, max_mcs_idx, nss);
	wlan_hdd_fill_os_rate_info(rate_flags, max_rate, rate,
				   max_mcs_idx, nss, 0, 0);

	return true;
}

/**
 * hdd_report_actual_rate() - Fill the actual rate stats.
 *
 * @rate_flags: The rate flags computed from rate
 * @my_rate: The rate from fw stats
 * @rate: The station_info struct member strust rate_info to be filled
 * @mcs_index: The mcs index computed from rate
 * @nss: The NSS from fw stats
 * @dcm: the dcm computed from rate
 * @guard_interval: the guard interval computed from rate
 *
 * Return: None
 */
static void hdd_report_actual_rate(enum tx_rate_info rate_flags,
				   uint16_t my_rate,
				   struct rate_info *rate, uint8_t mcs_index,
				   uint8_t nss, uint8_t dcm,
				   enum txrate_gi guard_interval)
{
	/* report current rate instead of max rate */
	wlan_hdd_fill_os_rate_info(rate_flags, my_rate, rate,
				   mcs_index, nss, dcm, guard_interval);
}

/**
 * hdd_wlan_fill_per_chain_rssi_stats() - Fill per chain rssi stats
 *
 * @sinfo: The station_info structure to be filled.
 * @adapter: The HDD adapter structure
 *
 * Return: None
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
static inline
void hdd_wlan_fill_per_chain_rssi_stats(struct station_info *sinfo,
					struct hdd_adapter *adapter)
{
	bool rssi_stats_valid = false;
	uint8_t i;

	sinfo->signal_avg = WLAN_HDD_TGT_NOISE_FLOOR_DBM;
	for (i = 0; i < NUM_CHAINS_MAX; i++) {
		sinfo->chain_signal_avg[i] =
			   adapter->hdd_stats.per_chain_rssi_stats.rssi[i];
		sinfo->chains |= 1 << i;
		if (sinfo->chain_signal_avg[i] > sinfo->signal_avg &&
		    sinfo->chain_signal_avg[i] != 0)
			sinfo->signal_avg = sinfo->chain_signal_avg[i];

		hdd_debug("RSSI for chain %d, vdev_id %d is %d",
			  i, adapter->vdev_id, sinfo->chain_signal_avg[i]);

		if (!rssi_stats_valid && sinfo->chain_signal_avg[i])
			rssi_stats_valid = true;
	}

	if (rssi_stats_valid) {
		sinfo->filled |= HDD_INFO_CHAIN_SIGNAL_AVG;
		sinfo->filled |= HDD_INFO_SIGNAL_AVG;
	}
}
#else
static inline
void hdd_wlan_fill_per_chain_rssi_stats(struct station_info *sinfo,
					struct hdd_adapter *adapter)
{
}
#endif

#if defined(CFG80211_RX_FCS_ERROR_REPORTING_SUPPORT)
static void hdd_fill_fcs_and_mpdu_count(struct hdd_adapter *adapter,
					struct station_info *sinfo)
{
	sinfo->rx_mpdu_count = adapter->hdd_stats.peer_stats.rx_count;
	sinfo->fcs_err_count = adapter->hdd_stats.peer_stats.fcs_count;
	hdd_debug("RX mpdu count %d fcs_err_count %d",
		  sinfo->rx_mpdu_count, sinfo->fcs_err_count);
	sinfo->filled |= HDD_INFO_FCS_ERROR_COUNT | HDD_INFO_RX_MPDUS;
}
#else
static void hdd_fill_fcs_and_mpdu_count(struct hdd_adapter *adapter,
					struct station_info *sinfo)
{
}
#endif

void hdd_check_and_update_nss(struct hdd_context *hdd_ctx,
			      uint8_t *tx_nss, uint8_t *rx_nss)
{
	if (tx_nss && (*tx_nss > 1) &&
	    policy_mgr_is_current_hwmode_dbs(hdd_ctx->psoc) &&
	    !policy_mgr_is_hw_dbs_2x2_capable(hdd_ctx->psoc)) {
		hdd_debug("Hw mode is DBS, Reduce tx nss(%d) to 1", *tx_nss);
		(*tx_nss)--;
	}

	if (rx_nss && (*rx_nss > 1) &&
	    policy_mgr_is_current_hwmode_dbs(hdd_ctx->psoc) &&
	    !policy_mgr_is_hw_dbs_2x2_capable(hdd_ctx->psoc)) {
		hdd_debug("Hw mode is DBS, Reduce rx nss(%d) to 1", *rx_nss);
		(*rx_nss)--;
	}
}

/**
 * wlan_hdd_get_sta_stats() - get aggregate STA stats
 * @wiphy: wireless phy
 * @adapter: STA adapter to get stats for
 * @mac: mac address of sta
 * @sinfo: kernel station_info struct to populate
 *
 * Fetch the vdev-level aggregate stats for the given STA adapter. This is to
 * support "station dump" and "station get" for STA vdevs
 *
 * Return: errno
 */
static int wlan_hdd_get_sta_stats(struct wiphy *wiphy,
				  struct hdd_adapter *adapter,
				  const uint8_t *mac,
				  struct station_info *sinfo)
{
	struct hdd_station_ctx *sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	enum tx_rate_info rate_flags, tx_rate_flags, rx_rate_flags;
	uint8_t tx_mcs_index, rx_mcs_index;
	struct hdd_context *hdd_ctx = (struct hdd_context *) wiphy_priv(wiphy);
	mac_handle_t mac_handle;
	int8_t snr = 0;
	uint16_t my_tx_rate, my_rx_rate;
	uint8_t tx_nss = 1, rx_nss = 1, tx_dcm, rx_dcm;
	enum txrate_gi tx_gi, rx_gi;
	int32_t rcpi_value;
	int link_speed_rssi_high = 0;
	int link_speed_rssi_mid = 0;
	int link_speed_rssi_low = 0;
	uint32_t link_speed_rssi_report = 0;
	struct wlan_objmgr_vdev *vdev;

	qdf_mtrace(QDF_MODULE_ID_HDD, QDF_MODULE_ID_HDD,
		   TRACE_CODE_HDD_CFG80211_GET_STA,
		   adapter->vdev_id, 0);

	if (eConnectionState_Associated != sta_ctx->conn_info.conn_state) {
		hdd_debug("Not associated");
		/*To keep GUI happy */
		return 0;
	}

	if (sta_ctx->hdd_reassoc_scenario) {
		hdd_debug("Roaming is in progress, cannot continue with this request");
		/*
		 * supplicant reports very low rssi to upper layer
		 * and handover happens to cellular.
		 * send the cached rssi when get_station
		 */
		sinfo->signal = adapter->rssi;
		sinfo->filled |= HDD_INFO_SIGNAL;
		return 0;
	}

	ucfg_mlme_stats_get_cfg_values(hdd_ctx->psoc,
				       &link_speed_rssi_high,
				       &link_speed_rssi_mid,
				       &link_speed_rssi_low,
				       &link_speed_rssi_report);

	if (hdd_ctx->rcpi_enabled)
		wlan_hdd_get_rcpi(adapter, (uint8_t *)mac, &rcpi_value,
				  RCPI_MEASUREMENT_TYPE_AVG_MGMT);

	wlan_hdd_get_station_stats(adapter);

	adapter->rssi = adapter->hdd_stats.summary_stat.rssi;
	snr = adapter->hdd_stats.summary_stat.snr;

	/* for new connection there might be no valid previous RSSI */
	if (!adapter->rssi) {
		hdd_get_rssi_snr_by_bssid(adapter,
				sta_ctx->conn_info.bssid.bytes,
				&adapter->rssi, &snr);
	}

	/* If RSSi is reported as positive then it is invalid */
	if (adapter->rssi > 0) {
		hdd_debug_rl("RSSI invalid %d", adapter->rssi);
		adapter->rssi = 0;
		adapter->hdd_stats.summary_stat.rssi = 0;
	}

	sinfo->signal = adapter->rssi;
	hdd_debug("snr: %d, rssi: %d",
		adapter->hdd_stats.summary_stat.snr,
		adapter->hdd_stats.summary_stat.rssi);
	sta_ctx->conn_info.signal = sinfo->signal;
	sta_ctx->conn_info.noise =
		sta_ctx->conn_info.signal - snr;
	sta_ctx->cache_conn_info.signal = sinfo->signal;
	sta_ctx->cache_conn_info.noise = sta_ctx->conn_info.noise;
	sinfo->filled |= HDD_INFO_SIGNAL;

	/*
	 * we notify connect to lpass here instead of during actual
	 * connect processing because rssi info is not accurate during
	 * actual connection.  lpass will ensure the notification is
	 * only processed once per association.
	 */
	hdd_lpass_notify_connect(adapter);

	rate_flags = adapter->hdd_stats.class_a_stat.tx_rx_rate_flags;
	tx_rate_flags = rx_rate_flags = rate_flags;

	tx_mcs_index = adapter->hdd_stats.class_a_stat.tx_mcs_index;
	rx_mcs_index = adapter->hdd_stats.class_a_stat.rx_mcs_index;
	mac_handle = hdd_ctx->mac_handle;

	/* convert to the UI units of 100kbps */
	my_tx_rate = adapter->hdd_stats.class_a_stat.tx_rate;
	my_rx_rate = adapter->hdd_stats.class_a_stat.rx_rate;

	tx_dcm = adapter->hdd_stats.class_a_stat.tx_dcm;
	rx_dcm = adapter->hdd_stats.class_a_stat.rx_dcm;
	tx_gi = adapter->hdd_stats.class_a_stat.tx_gi;
	rx_gi = adapter->hdd_stats.class_a_stat.rx_gi;

	if (!(rate_flags & TX_RATE_LEGACY)) {
		tx_nss = adapter->hdd_stats.class_a_stat.tx_nss;
		rx_nss = adapter->hdd_stats.class_a_stat.rx_nss;

		hdd_check_and_update_nss(hdd_ctx, &tx_nss, &rx_nss);

		if (ucfg_mlme_stats_is_link_speed_report_actual(
					hdd_ctx->psoc)) {
			/* Get current rate flags if report actual */
			/* WMA fails to find mcs_index for legacy tx rates */
			if (tx_mcs_index == INVALID_MCS_IDX && my_tx_rate)
				tx_rate_flags = TX_RATE_LEGACY;
			else
				tx_rate_flags =
			      adapter->hdd_stats.class_a_stat.tx_mcs_rate_flags;

			if (rx_mcs_index == INVALID_MCS_IDX && my_rx_rate)
				rx_rate_flags = TX_RATE_LEGACY;
			else
				rx_rate_flags =
			      adapter->hdd_stats.class_a_stat.rx_mcs_rate_flags;
		}

		if (tx_mcs_index == INVALID_MCS_IDX)
			tx_mcs_index = 0;
		if (rx_mcs_index == INVALID_MCS_IDX)
			rx_mcs_index = 0;
	}

	hdd_debug("[RSSI %d, RLMS %u, rssi high %d, rssi mid %d, rssi low %d]-"
		  "[Rate info: TX: %d, RX: %d]-[Rate flags: TX: 0x%x, RX: 0x%x]"
		  "-[MCS Index: TX: %d, RX: %d]-[NSS: TX: %d, RX: %d]-"
		  "[dcm: TX: %d, RX: %d]-[guard interval: TX: %d, RX: %d",
		  sinfo->signal, link_speed_rssi_report,
		  link_speed_rssi_high, link_speed_rssi_mid,
		  link_speed_rssi_low, my_tx_rate, my_rx_rate,
		  (int)tx_rate_flags, (int)rx_rate_flags, (int)tx_mcs_index,
		  (int)rx_mcs_index, (int)tx_nss, (int)rx_nss,
		  (int)tx_dcm, (int)rx_dcm, (int)tx_gi, (int)rx_gi);

	if (!ucfg_mlme_stats_is_link_speed_report_actual(hdd_ctx->psoc)) {
		bool tx_rate_calc, rx_rate_calc;
		uint8_t tx_nss_max, rx_nss_max;

		vdev = hdd_objmgr_get_vdev(adapter);
		if (!vdev)
			/* Keep GUI happy */
			return 0;
		/*
		 * Take static NSS for reporting max rates. NSS from the FW
		 * is not reliable as it changes as per the environment
		 * quality.
		 */
		tx_nss_max = wlan_vdev_mlme_get_nss(vdev);
		rx_nss_max = wlan_vdev_mlme_get_nss(vdev);
		hdd_objmgr_put_vdev(vdev);

		hdd_check_and_update_nss(hdd_ctx, &tx_nss_max, &rx_nss_max);

		tx_rate_calc = hdd_report_max_rate(adapter, mac_handle,
						   &sinfo->txrate,
						   sinfo->signal,
						   tx_rate_flags,
						   tx_mcs_index,
						   my_tx_rate,
						   tx_nss_max);

		rx_rate_calc = hdd_report_max_rate(adapter, mac_handle,
						   &sinfo->rxrate,
						   sinfo->signal,
						   rx_rate_flags,
						   rx_mcs_index,
						   my_rx_rate,
						   rx_nss_max);

		if (!tx_rate_calc || !rx_rate_calc)
			/* Keep GUI happy */
			return 0;
	} else {

		/* Fill TX stats */
		hdd_report_actual_rate(tx_rate_flags, my_tx_rate,
				       &sinfo->txrate, tx_mcs_index,
				       tx_nss, tx_dcm, tx_gi);


		/* Fill RX stats */
		hdd_report_actual_rate(rx_rate_flags, my_rx_rate,
				       &sinfo->rxrate, rx_mcs_index,
				       rx_nss, rx_dcm, rx_gi);
	}

	wlan_hdd_fill_summary_stats(&adapter->hdd_stats.summary_stat,
				    sinfo,
				    adapter->vdev_id);
	sinfo->tx_bytes = adapter->stats.tx_bytes;
	sinfo->rx_bytes = adapter->stats.rx_bytes;
	sinfo->rx_packets = adapter->stats.rx_packets;

	hdd_fill_fcs_and_mpdu_count(adapter, sinfo);

	qdf_mem_copy(&sta_ctx->conn_info.txrate,
		     &sinfo->txrate, sizeof(sinfo->txrate));
	qdf_mem_copy(&sta_ctx->cache_conn_info.txrate,
		     &sinfo->txrate, sizeof(sinfo->txrate));

	qdf_mem_copy(&sta_ctx->conn_info.rxrate,
		     &sinfo->rxrate, sizeof(sinfo->rxrate));

	sinfo->filled |= HDD_INFO_TX_BITRATE |
			 HDD_INFO_RX_BITRATE |
			 HDD_INFO_TX_BYTES   |
			 HDD_INFO_RX_BYTES   |
			 HDD_INFO_RX_PACKETS;

	if (tx_rate_flags & TX_RATE_LEGACY) {
		hdd_debug("[TX: Reporting legacy rate %d pkt cnt %d]-"
			  "[RX: Reporting legacy rate %d pkt cnt %d]",
			  sinfo->txrate.legacy, sinfo->tx_packets,
			  sinfo->rxrate.legacy, sinfo->rx_packets);
	} else {
		hdd_debug("[TX: Reporting MCS rate %d, flags 0x%x pkt cnt %d, nss %d, bw %d]-"
			  "[RX: Reporting MCS rate %d, flags 0x%x pkt cnt %d, nss %d, bw %d]",
			  sinfo->txrate.mcs, sinfo->txrate.flags,
			  sinfo->tx_packets, sinfo->txrate.nss,
			  sinfo->rxrate.bw, sinfo->rxrate.mcs,
			  sinfo->rxrate.flags, sinfo->rx_packets,
			  sinfo->rxrate.nss, sinfo->rxrate.bw);
	}

	hdd_wlan_fill_per_chain_rssi_stats(sinfo, adapter);

	hdd_exit();

	return 0;
}

/**
 * __wlan_hdd_cfg80211_get_station() - get station statistics
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @mac: Pointer to mac
 * @sinfo: Pointer to station info
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_get_station(struct wiphy *wiphy,
					   struct net_device *dev,
					   const uint8_t *mac,
					   struct station_info *sinfo)
{
	int status;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	bool get_peer_info_enable;
	QDF_STATUS qdf_status;

	hdd_enter_dev(dev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return status;

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	if (adapter->device_mode == QDF_SAP_MODE) {
		qdf_status = ucfg_mlme_get_sap_get_peer_info(
				hdd_ctx->psoc, &get_peer_info_enable);
		if (qdf_status == QDF_STATUS_SUCCESS && get_peer_info_enable) {
			status = wlan_hdd_get_station_remote(wiphy, dev,
							     mac, sinfo);
			if (!status)
				return 0;
		}
		return wlan_hdd_get_sap_stats(adapter, sinfo);
	} else {
		return wlan_hdd_get_sta_stats(wiphy, adapter, mac, sinfo);
	}
}

/**
 * _wlan_hdd_cfg80211_get_station() - get station statistics
 *
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @mac: Pointer to mac
 * @sinfo: Pointer to station info
 *
 * This API tries runtime PM suspend right away after getting station
 * statistics.
 *
 * Return: 0 for success, non-zero for failure
 */
static int _wlan_hdd_cfg80211_get_station(struct wiphy *wiphy,
					  struct net_device *dev,
					  const uint8_t *mac,
					  struct station_info *sinfo)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	int errno;
	QDF_STATUS status;

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	status = wlan_hdd_stats_request_needed(adapter);
	if (QDF_IS_STATUS_ERROR(status)) {
		if (status == QDF_STATUS_E_ALREADY)
			get_station_fw_request_needed = false;
		else
			return -EINVAL;
	}

	if (get_station_fw_request_needed) {
		errno = wlan_hdd_qmi_get_sync_resume();
		if (errno) {
			hdd_err("qmi sync resume failed: %d", errno);
			return errno;
		}
	}

	errno = __wlan_hdd_cfg80211_get_station(wiphy, dev, mac, sinfo);

	if (get_station_fw_request_needed)
		wlan_hdd_qmi_put_suspend();

	get_station_fw_request_needed = true;

	return errno;
}

/**
 * wlan_hdd_cfg80211_get_station() - get station statistics
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @mac: Pointer to mac
 * @sinfo: Pointer to station info
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_get_station(struct wiphy *wiphy,
				  struct net_device *dev, const uint8_t *mac,
				  struct station_info *sinfo)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = _wlan_hdd_cfg80211_get_station(wiphy, dev, mac, sinfo);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __wlan_hdd_cfg80211_dump_station() - dump station statistics
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @idx: variable to determine whether to get stats or not
 * @mac: Pointer to mac
 * @sinfo: Pointer to station info
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_dump_station(struct wiphy *wiphy,
				struct net_device *dev,
				int idx, u8 *mac,
				struct station_info *sinfo)
{
	hdd_debug("idx: %d", idx);
	if (idx != 0)
		return -ENOENT;
	qdf_mem_copy(mac, dev->dev_addr, QDF_MAC_ADDR_SIZE);
	return __wlan_hdd_cfg80211_get_station(wiphy, dev, mac, sinfo);
}

/**
 * wlan_hdd_cfg80211_dump_station() - dump station statistics
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @idx: variable to determine whether to get stats or not
 * @mac: Pointer to mac
 * @sinfo: Pointer to station info
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_dump_station(struct wiphy *wiphy,
				struct net_device *dev,
				int idx, u8 *mac,
				struct station_info *sinfo)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_dump_station(wiphy, dev, idx, mac, sinfo);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * hdd_get_stats() - Function to retrieve interface statistics
 * @dev: pointer to network device
 *
 * This function is the ndo_get_stats method for all netdevs
 * registered with the kernel
 *
 * Return: pointer to net_device_stats structure
 */
struct net_device_stats *hdd_get_stats(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	hdd_enter_dev(dev);
	return &adapter->stats;
}


/*
 * time = cycle_count * cycle
 * cycle = 1 / clock_freq
 * Since the unit of clock_freq reported from
 * FW is MHZ, and we want to calculate time in
 * ms level, the result is
 * time = cycle / (clock_freq * 1000)
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0))
static bool wlan_fill_survey_result(struct survey_info *survey, int opfreq,
				    struct scan_chan_info *chan_info,
				    struct ieee80211_channel *channels)
{
	uint64_t clock_freq = chan_info->clock_freq * 1000;

	if (channels->center_freq != (uint16_t)chan_info->freq)
		return false;

	survey->channel = channels;
	survey->noise = chan_info->noise_floor;
	survey->filled = 0;

	if (chan_info->noise_floor)
		survey->filled |= SURVEY_INFO_NOISE_DBM;

	if (opfreq == chan_info->freq)
		survey->filled |= SURVEY_INFO_IN_USE;

	if (clock_freq == 0)
		return true;

	survey->time = qdf_do_div(chan_info->cycle_count, clock_freq);

	survey->time_busy = qdf_do_div(chan_info->rx_clear_count, clock_freq);

	survey->time_tx = qdf_do_div(chan_info->tx_frame_count, clock_freq);

	survey->filled |= SURVEY_INFO_TIME |
			  SURVEY_INFO_TIME_BUSY |
			  SURVEY_INFO_TIME_TX;
	return true;
}
#else
static bool wlan_fill_survey_result(struct survey_info *survey, int opfreq,
				    struct scan_chan_info *chan_info,
				    struct ieee80211_channel *channels)
{
	uint64_t clock_freq = chan_info->clock_freq * 1000;

	if (channels->center_freq != (uint16_t)chan_info->freq)
		return false;

	survey->channel = channels;
	survey->noise = chan_info->noise_floor;
	survey->filled = 0;

	if (chan_info->noise_floor)
		survey->filled |= SURVEY_INFO_NOISE_DBM;

	if (opfreq == chan_info->freq)
		survey->filled |= SURVEY_INFO_IN_USE;

	if (clock_freq == 0)
		return true;

	survey->channel_time = qdf_do_div(chan_info->cycle_count, clock_freq);

	survey->channel_time_busy = qdf_do_div(chan_info->rx_clear_count,
							 clock_freq);

	survey->channel_time_tx = qdf_do_div(chan_info->tx_frame_count,
							 clock_freq);

	survey->filled |= SURVEY_INFO_CHANNEL_TIME |
			  SURVEY_INFO_CHANNEL_TIME_BUSY |
			  SURVEY_INFO_CHANNEL_TIME_TX;
	return true;
}
#endif

static bool wlan_hdd_update_survey_info(struct wiphy *wiphy,
					struct hdd_adapter *adapter,
					struct survey_info *survey, int idx)
{
	bool filled = false;
	int i, j = 0;
	uint32_t opfreq = 0; /* Initialization Required */
	struct hdd_context *hdd_ctx;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	sme_get_operation_channel(hdd_ctx->mac_handle, &opfreq,
				  adapter->vdev_id);

	mutex_lock(&hdd_ctx->chan_info_lock);

	for (i = 0; i < HDD_NUM_NL80211_BANDS && !filled; i++) {
		if (!wiphy->bands[i])
			continue;

		for (j = 0; j < wiphy->bands[i]->n_channels && !filled; j++) {
			struct ieee80211_supported_band *band = wiphy->bands[i];

			filled = wlan_fill_survey_result(survey, opfreq,
				&hdd_ctx->chan_info[idx],
				&band->channels[j]);
		}
	}
	mutex_unlock(&hdd_ctx->chan_info_lock);

	return filled;
}

/**
 * __wlan_hdd_cfg80211_dump_survey() - get survey related info
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @idx: Index
 * @survey: Pointer to survey info
 *
 * Return: 0 for success, non-zero for failure
 */
static int __wlan_hdd_cfg80211_dump_survey(struct wiphy *wiphy,
					   struct net_device *dev,
					   int idx, struct survey_info *survey)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	struct hdd_station_ctx *sta_ctx;
	int status;
	bool filled = false;

	if (idx > NUM_CHANNELS - 1)
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status)
		return status;

	if (!hdd_ctx->chan_info) {
		hdd_debug("chan_info is NULL");
		return -EINVAL;
	}

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	if (!ucfg_scan_is_snr_monitor_enabled(hdd_ctx->psoc))
		return -ENONET;

	if (sta_ctx->hdd_reassoc_scenario) {
		hdd_debug("Roaming in progress, hence return");
		return -ENONET;
	}

	filled = wlan_hdd_update_survey_info(wiphy, adapter, survey, idx);

	if (!filled)
		return -ENONET;

	return 0;
}

/**
 * wlan_hdd_cfg80211_dump_survey() - get survey related info
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to network device
 * @idx: Index
 * @survey: Pointer to survey info
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_cfg80211_dump_survey(struct wiphy *wiphy,
				  struct net_device *dev,
				  int idx, struct survey_info *survey)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_dump_survey(wiphy, dev, idx, survey);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * hdd_display_hif_stats() - display hif stats
 *
 * Return: none
 *
 */
void hdd_display_hif_stats(void)
{
	void *hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);

	if (!hif_ctx)
		return;

	hif_display_stats(hif_ctx);
}

/**
 * hdd_clear_hif_stats() - clear hif stats
 *
 * Return: none
 */
void hdd_clear_hif_stats(void)
{
	void *hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);

	if (!hif_ctx)
		return;
	hif_clear_stats(hif_ctx);
}

/**
 * hdd_is_rcpi_applicable() - validates RCPI request
 * @adapter: adapter upon which the measurement is requested
 * @mac_addr: peer addr for which measurement is requested
 * @rcpi_value: pointer to where the RCPI should be returned
 * @reassoc: used to return cached RCPI during reassoc
 *
 * Return: true for success, false for failure
 */

static bool hdd_is_rcpi_applicable(struct hdd_adapter *adapter,
				   struct qdf_mac_addr *mac_addr,
				   int32_t *rcpi_value,
				   bool *reassoc)
{
	struct hdd_station_ctx *hdd_sta_ctx;

	if (adapter->device_mode == QDF_STA_MODE ||
	    adapter->device_mode == QDF_P2P_CLIENT_MODE) {
		hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if (hdd_sta_ctx->conn_info.conn_state !=
		    eConnectionState_Associated)
			return false;

		if (hdd_sta_ctx->hdd_reassoc_scenario) {
			/* return the cached rcpi, if mac addr matches */
			hdd_debug("Roaming in progress, return cached RCPI");
			if (!qdf_mem_cmp(&adapter->rcpi.mac_addr,
					 mac_addr, sizeof(*mac_addr))) {
				*rcpi_value = adapter->rcpi.rcpi;
				*reassoc = true;
				return true;
			}
			return false;
		}

		if (qdf_mem_cmp(mac_addr, &hdd_sta_ctx->conn_info.bssid,
				sizeof(*mac_addr))) {
			hdd_err("mac addr is different from bssid connected");
			return false;
		}
	} else if (adapter->device_mode == QDF_SAP_MODE ||
		   adapter->device_mode == QDF_P2P_GO_MODE) {
		if (!test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
			hdd_err("Invalid rcpi request, softap not started");
			return false;
		}

		/* check if peer mac addr is associated to softap */
		if (!hdd_is_peer_associated(adapter, mac_addr)) {
			hdd_err("invalid peer mac-addr: not associated");
			return false;
		}
	} else {
		hdd_err("Invalid rcpi request");
		return false;
	}

	*reassoc = false;
	return true;
}

/**
 * wlan_hdd_get_rcpi_cb() - callback function for rcpi response
 * @context: Pointer to rcpi context
 * @rcpi_req: Pointer to rcpi response
 *
 * Return: None
 */
static void wlan_hdd_get_rcpi_cb(void *context, struct qdf_mac_addr mac_addr,
				 int32_t rcpi, QDF_STATUS status)
{
	struct osif_request *request;
	struct rcpi_info *priv;

	if (!context) {
		hdd_err("No rcpi context");
		return;
	}

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete RCPI request");
		return;
	}

	priv = osif_request_priv(request);
	priv->mac_addr = mac_addr;

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		priv->rcpi = 0;
		hdd_err("Error in computing RCPI");
	} else {
		priv->rcpi = rcpi;
	}

	osif_request_complete(request);
	osif_request_put(request);
}

/**
 * wlan_hdd_get_rcpi() - local function to get RCPI
 * @adapter: adapter upon which the measurement is requested
 * @mac: peer addr for which measurement is requested
 * @rcpi_value: pointer to where the RCPI should be returned
 * @measurement_type: type of rcpi measurement
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_get_rcpi(struct hdd_adapter *adapter,
		      uint8_t *mac,
		      int32_t *rcpi_value,
		      enum rcpi_measurement_type measurement_type)
{
	struct hdd_context *hdd_ctx;
	int status = 0, ret = 0;
	struct qdf_mac_addr mac_addr;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct sme_rcpi_req *rcpi_req;
	void *cookie;
	struct rcpi_info *priv;
	struct osif_request *request;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_RCPI,
	};
	bool reassoc;

	hdd_enter();

	/* initialize the rcpi value to zero, useful in error cases */
	*rcpi_value = 0;

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (!adapter) {
		hdd_warn("adapter context is NULL");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (status)
		return -EINVAL;

	if (!hdd_ctx->rcpi_enabled) {
		hdd_debug("RCPI not supported");
		return -EINVAL;
	}

	if (!mac) {
		hdd_warn("RCPI peer mac-addr is NULL");
		return -EINVAL;
	}

	qdf_mem_copy(&mac_addr, mac, QDF_MAC_ADDR_SIZE);

	if (!hdd_is_rcpi_applicable(adapter, &mac_addr, rcpi_value, &reassoc))
		return -EINVAL;
	if (reassoc)
		return 0;

	rcpi_req = qdf_mem_malloc(sizeof(*rcpi_req));
	if (!rcpi_req)
		return -EINVAL;

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		qdf_mem_free(rcpi_req);
		return -ENOMEM;
	}
	cookie = osif_request_cookie(request);

	rcpi_req->mac_addr = mac_addr;
	rcpi_req->session_id = adapter->vdev_id;
	rcpi_req->measurement_type = measurement_type;
	rcpi_req->rcpi_callback = wlan_hdd_get_rcpi_cb;
	rcpi_req->rcpi_context = cookie;

	qdf_status = sme_get_rcpi(hdd_ctx->mac_handle, rcpi_req);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Unable to retrieve RCPI");
		status = qdf_status_to_os_return(qdf_status);
		goto out;
	}

	/* request was sent -- wait for the response */
	ret = osif_request_wait_for_response(request);
	if (ret) {
		hdd_err("SME timed out while retrieving RCPI");
		status = -EINVAL;
		goto out;
	}

	/* update the adapter with the fresh results */
	priv = osif_request_priv(request);
	adapter->rcpi.mac_addr = priv->mac_addr;
	adapter->rcpi.rcpi = priv->rcpi;
	if (qdf_mem_cmp(&mac_addr, &priv->mac_addr, sizeof(mac_addr))) {
		hdd_err("mis match of mac addr from call-back");
		status = -EINVAL;
		goto out;
	}

	*rcpi_value = adapter->rcpi.rcpi;
	hdd_debug("RCPI = %d", *rcpi_value);
out:
	qdf_mem_free(rcpi_req);
	osif_request_put(request);

	hdd_exit();
	return status;
}

#ifdef WLAN_FEATURE_MIB_STATS
QDF_STATUS wlan_hdd_get_mib_stats(struct hdd_adapter *adapter)
{
	int ret = 0;
	struct stats_event *stats;
	struct wlan_objmgr_vdev *vdev;

	if (!adapter) {
		hdd_err("Invalid context, adapter");
		return QDF_STATUS_E_FAULT;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return QDF_STATUS_E_FAULT;

	stats = wlan_cfg80211_mc_cp_stats_get_mib_stats(vdev, &ret);
	hdd_objmgr_put_vdev(vdev);
	if (ret || !stats) {
		wlan_cfg80211_mc_cp_stats_free_stats_event(stats);
		return ret;
	}

	hdd_debugfs_process_mib_stats(adapter, stats);

	wlan_cfg80211_mc_cp_stats_free_stats_event(stats);
	return ret;
}
#endif

QDF_STATUS wlan_hdd_get_rssi(struct hdd_adapter *adapter, int8_t *rssi_value)
{
	int ret = 0, i;
	struct hdd_station_ctx *sta_ctx;
	struct stats_event *rssi_info;
	struct wlan_objmgr_vdev *vdev;

	if (!adapter) {
		hdd_err("Invalid context, adapter");
		return QDF_STATUS_E_FAULT;
	}
	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state()) {
		hdd_err("Recovery in Progress. State: 0x%x Ignore!!!",
			cds_get_driver_state());
		/* return a cached value */
		*rssi_value = adapter->rssi;
		return QDF_STATUS_SUCCESS;
	}

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	if (eConnectionState_Associated != sta_ctx->conn_info.conn_state) {
		hdd_debug("Not associated!, rssi on disconnect %d",
			  adapter->rssi_on_disconnect);
		*rssi_value = adapter->rssi_on_disconnect;
		return QDF_STATUS_SUCCESS;
	}

	if (sta_ctx->hdd_reassoc_scenario) {
		hdd_debug("Roaming in progress, return cached RSSI");
		*rssi_value = adapter->rssi;
		return QDF_STATUS_SUCCESS;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		*rssi_value = adapter->rssi;
		return QDF_STATUS_SUCCESS;
	}

	rssi_info = wlan_cfg80211_mc_cp_stats_get_peer_rssi(
			vdev,
			sta_ctx->conn_info.bssid.bytes,
			&ret);
	hdd_objmgr_put_vdev(vdev);
	if (ret || !rssi_info) {
		wlan_cfg80211_mc_cp_stats_free_stats_event(rssi_info);
		return ret;
	}

	for (i = 0; i < rssi_info->num_peer_stats; i++) {
		if (!qdf_mem_cmp(rssi_info->peer_stats[i].peer_macaddr,
				 sta_ctx->conn_info.bssid.bytes,
				 QDF_MAC_ADDR_SIZE)) {
			*rssi_value = rssi_info->peer_stats[i].peer_rssi;
			hdd_debug("RSSI = %d", *rssi_value);
			wlan_cfg80211_mc_cp_stats_free_stats_event(rssi_info);
			return QDF_STATUS_SUCCESS;
		}
	}

	wlan_cfg80211_mc_cp_stats_free_stats_event(rssi_info);
	hdd_err("bss peer not present in returned result");
	return QDF_STATUS_E_FAULT;
}

struct snr_priv {
	int8_t snr;
};

/**
 * hdd_get_snr_cb() - "Get SNR" callback function
 * @snr: Current SNR of the station
 * @sta_id: ID of the station
 * @context: opaque context originally passed to SME.  HDD always passes
 *	a cookie for the request context
 *
 * Return: None
 */
static void hdd_get_snr_cb(int8_t snr, void *context)
{
	struct osif_request *request;
	struct snr_priv *priv;

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete request");
		return;
	}

	/* propagate response back to requesting thread */
	priv = osif_request_priv(request);
	priv->snr = snr;
	osif_request_complete(request);
	osif_request_put(request);
}

QDF_STATUS wlan_hdd_get_snr(struct hdd_adapter *adapter, int8_t *snr)
{
	struct hdd_context *hdd_ctx;
	struct hdd_station_ctx *sta_ctx;
	QDF_STATUS status;
	int ret;
	void *cookie;
	struct osif_request *request;
	struct snr_priv *priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_STATS,
	};

	hdd_enter();

	if (!adapter) {
		hdd_err("Invalid context, adapter");
		return QDF_STATUS_E_FAULT;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return QDF_STATUS_E_FAULT;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return QDF_STATUS_E_FAULT;
	}
	cookie = osif_request_cookie(request);

	status = sme_get_snr(hdd_ctx->mac_handle, hdd_get_snr_cb,
			     sta_ctx->conn_info.bssid, cookie);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Unable to retrieve RSSI");
		/* we'll returned a cached value below */
	} else {
		/* request was sent -- wait for the response */
		ret = osif_request_wait_for_response(request);
		if (ret) {
			hdd_err("SME timed out while retrieving SNR");
			/* we'll now returned a cached value below */
		} else {
			/* update the adapter with the fresh results */
			priv = osif_request_priv(request);
			adapter->snr = priv->snr;
		}
	}

	/*
	 * either we never sent a request, we sent a request and
	 * received a response or we sent a request and timed out.
	 * regardless we are done with the request.
	 */
	osif_request_put(request);

	*snr = adapter->snr;
	hdd_exit();
	return QDF_STATUS_SUCCESS;
}

struct linkspeed_priv {
	struct link_speed_info linkspeed_info;
};

static void
hdd_get_link_speed_cb(struct link_speed_info *linkspeed_info, void *context)
{
	struct osif_request *request;
	struct linkspeed_priv *priv;

	if (!linkspeed_info) {
		hdd_err("NULL linkspeed");
		return;
	}

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	priv->linkspeed_info = *linkspeed_info;
	osif_request_complete(request);
	osif_request_put(request);
}

int wlan_hdd_get_linkspeed_for_peermac(struct hdd_adapter *adapter,
				       struct qdf_mac_addr *mac_address,
				       uint32_t *linkspeed)
{
	int ret;
	QDF_STATUS status;
	void *cookie;
	struct link_speed_info *linkspeed_info;
	struct osif_request *request;
	struct linkspeed_priv *priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_STATS,
	};

	if ((!adapter) || (!linkspeed)) {
		hdd_err("NULL argument");
		return -EINVAL;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		ret = -ENOMEM;
		goto return_cached_value;
	}

	cookie = osif_request_cookie(request);
	priv = osif_request_priv(request);

	linkspeed_info = &priv->linkspeed_info;
	qdf_copy_macaddr(&linkspeed_info->peer_macaddr, mac_address);
	status = sme_get_link_speed(adapter->hdd_ctx->mac_handle,
				    linkspeed_info,
				    cookie, hdd_get_link_speed_cb);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to retrieve statistics for link speed");
		ret = qdf_status_to_os_return(status);
		goto cleanup;
	}
	ret = osif_request_wait_for_response(request);
	if (ret) {
		hdd_err("SME timed out while retrieving link speed");
		goto cleanup;
	}
	adapter->estimated_linkspeed = linkspeed_info->estLinkSpeed;

cleanup:
	/*
	 * either we never sent a request, we sent a request and
	 * received a response or we sent a request and timed out.
	 * regardless we are done with the request.
	 */
	osif_request_put(request);

return_cached_value:
	*linkspeed = adapter->estimated_linkspeed;

	return ret;
}

int wlan_hdd_get_link_speed(struct hdd_adapter *adapter, uint32_t *link_speed)
{
	struct hdd_context *hddctx = WLAN_HDD_GET_CTX(adapter);
	struct hdd_station_ctx *hdd_stactx =
				WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	int ret;

	ret = wlan_hdd_validate_context(hddctx);
	if (ret)
		return ret;

	/* Linkspeed is allowed only for P2P mode */
	if (adapter->device_mode != QDF_P2P_CLIENT_MODE) {
		hdd_err("Link Speed is not allowed in Device mode %s(%d)",
			qdf_opmode_str(adapter->device_mode),
			adapter->device_mode);
		return -ENOTSUPP;
	}

	if (eConnectionState_Associated != hdd_stactx->conn_info.conn_state) {
		/* we are not connected so we don't have a classAstats */
		*link_speed = 0;
	} else {
		struct qdf_mac_addr bssid;

		qdf_copy_macaddr(&bssid, &hdd_stactx->conn_info.bssid);

		ret = wlan_hdd_get_linkspeed_for_peermac(adapter, &bssid,
							 link_speed);
		if (ret) {
			hdd_err("Unable to retrieve SME linkspeed");
			return ret;
		}
		/* linkspeed in units of 500 kbps */
		*link_speed = (*link_speed) / 500;
	}
	return 0;
}

int wlan_hdd_get_station_stats(struct hdd_adapter *adapter)
{
	int ret = 0;
	struct stats_event *stats;
	struct wlan_objmgr_vdev *vdev;

	if (!get_station_fw_request_needed) {
		hdd_debug("return cached get_station stats");
		return 0;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return -EINVAL;

	stats = wlan_cfg80211_mc_cp_stats_get_station_stats(vdev, &ret);
	if (ret || !stats) {
		wlan_cfg80211_mc_cp_stats_free_stats_event(stats);
		goto out;
	}

	/* update get stats cached time stamp */
	hdd_update_station_stats_cached_timestamp(adapter);
	copy_station_stats_to_adapter(adapter, stats);
	wlan_cfg80211_mc_cp_stats_free_stats_event(stats);

out:
	hdd_objmgr_put_vdev(vdev);
	return ret;
}

#ifdef WLAN_FEATURE_BIG_DATA_STATS
int wlan_hdd_get_big_data_station_stats(struct hdd_adapter *adapter)
{
	int ret = 0;
	struct big_data_stats_event *big_data_stats;
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return -EINVAL;

	big_data_stats = wlan_cfg80211_mc_cp_get_big_data_stats(vdev,
								&ret);
	if (ret || !big_data_stats)
		goto out;

	copy_station_big_data_stats_to_adapter(adapter, big_data_stats);
out:
	if (big_data_stats)
		wlan_cfg80211_mc_cp_stats_free_big_data_stats_event(
								big_data_stats);
	hdd_objmgr_put_vdev(vdev);
	return ret;
}
#endif

struct temperature_priv {
	int temperature;
};

/**
 * hdd_get_temperature_cb() - "Get Temperature" callback function
 * @temperature: measured temperature
 * @context: callback context
 *
 * This function is passed to sme_get_temperature() as the callback
 * function to be invoked when the temperature measurement is
 * available.
 *
 * Return: None
 */
static void hdd_get_temperature_cb(int temperature, void *context)
{
	struct osif_request *request;
	struct temperature_priv *priv;

	hdd_enter();

	request = osif_request_get(context);
	if (!request) {
		hdd_err("Obsolete request");
		return;
	}

	priv = osif_request_priv(request);
	priv->temperature = temperature;
	osif_request_complete(request);
	osif_request_put(request);
	hdd_exit();
}

int wlan_hdd_get_temperature(struct hdd_adapter *adapter, int *temperature)
{
	QDF_STATUS status;
	int ret;
	void *cookie;
	struct osif_request *request;
	struct temperature_priv *priv;
	static const struct osif_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_STATS,
	};

	hdd_enter();
	if (!adapter) {
		hdd_err("adapter is NULL");
		return -EPERM;
	}

	request = osif_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -ENOMEM;
	}
	cookie = osif_request_cookie(request);
	status = sme_get_temperature(adapter->hdd_ctx->mac_handle, cookie,
				     hdd_get_temperature_cb);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Unable to retrieve temperature");
	} else {
		ret = osif_request_wait_for_response(request);
		if (ret) {
			hdd_err("SME timed out while retrieving temperature");
		} else {
			/* update the adapter with the fresh results */
			priv = osif_request_priv(request);
			if (priv->temperature)
				adapter->temperature = priv->temperature;
		}
	}

	/*
	 * either we never sent a request, we sent a request and
	 * received a response or we sent a request and timed out.
	 * regardless we are done with the request.
	 */
	osif_request_put(request);

	*temperature = adapter->temperature;
	hdd_exit();
	return 0;
}

void wlan_hdd_display_txrx_stats(struct hdd_context *ctx)
{
	struct hdd_adapter *adapter = NULL, *next_adapter = NULL;
	struct hdd_tx_rx_stats *stats;
	int i = 0;
	uint32_t total_rx_pkt, total_rx_dropped,
		 total_rx_delv, total_rx_refused;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_CACHE_STATION_STATS_CB;

	hdd_for_each_adapter_dev_held_safe(ctx, adapter, next_adapter,
					   dbgid) {
		total_rx_pkt = 0;
		total_rx_dropped = 0;
		total_rx_delv = 0;
		total_rx_refused = 0;
		stats = &adapter->hdd_stats.tx_rx_stats;

		if (adapter->vdev_id == INVAL_VDEV_ID) {
			hdd_adapter_dev_put_debug(adapter, dbgid);
			continue;
		}

		hdd_debug("adapter: %u", adapter->vdev_id);
		for (; i < NUM_CPUS; i++) {
			total_rx_pkt += stats->rx_packets[i];
			total_rx_dropped += stats->rx_dropped[i];
			total_rx_delv += stats->rx_delivered[i];
			total_rx_refused += stats->rx_refused[i];
		}

		/* dev_put has to be done here */
		hdd_adapter_dev_put_debug(adapter, dbgid);

		hdd_debug("TX - called %u, dropped %u orphan %u",
			  stats->tx_called, stats->tx_dropped,
			  stats->tx_orphaned);

		for (i = 0; i < NUM_CPUS; i++) {
			if (stats->rx_packets[i] == 0)
				continue;
			hdd_debug("Rx CPU[%d]: packets %u, dropped %u, delivered %u, refused %u",
				  i, stats->rx_packets[i], stats->rx_dropped[i],
				  stats->rx_delivered[i], stats->rx_refused[i]);
		}
		hdd_debug("RX - packets %u, dropped %u, unsolict_arp_n_mcast_drp %u, delivered %u, refused %u GRO - agg %u drop %u non-agg %u flush_skip %u low_tput_flush %u disabled(conc %u low-tput %u)",
			  total_rx_pkt, total_rx_dropped,
			  qdf_atomic_read(&stats->rx_usolict_arp_n_mcast_drp),
			  total_rx_delv,
			  total_rx_refused, stats->rx_aggregated,
			  stats->rx_gro_dropped, stats->rx_non_aggregated,
			  stats->rx_gro_flush_skip,
			  stats->rx_gro_low_tput_flush,
			  qdf_atomic_read(&ctx->disable_rx_ol_in_concurrency),
			  qdf_atomic_read(&ctx->disable_rx_ol_in_low_tput));
	}
}

#ifdef QCA_SUPPORT_CP_STATS
/**
 * hdd_lost_link_cp_stats_info_cb() - callback function to get lost
 * link information
 * @stats_ev: Stats event pointer
 * FW sends vdev stats on vdev down, this callback is registered
 * with cp_stats component to get the last available vdev stats
 * From the FW.
 *
 * Return: None
 */

static void hdd_lost_link_cp_stats_info_cb(void *stats_ev)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	struct hdd_adapter *adapter;
	struct stats_event *ev = stats_ev;
	uint8_t i;

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	for (i = 0; i < ev->num_summary_stats; i++) {
		adapter = hdd_get_adapter_by_vdev(
					hdd_ctx,
					ev->vdev_summary_stats[i].vdev_id);
		if (!adapter) {
			hdd_debug("invalid adapter");
			continue;
		}
		adapter->rssi_on_disconnect =
					ev->vdev_summary_stats[i].stats.rssi;
		hdd_debug("rssi %d for " QDF_MAC_ADDR_FMT,
			  adapter->rssi_on_disconnect,
			  QDF_MAC_ADDR_REF(adapter->mac_addr.bytes));
	}
}

void wlan_hdd_register_cp_stats_cb(struct hdd_context *hdd_ctx)
{
	ucfg_mc_cp_stats_register_lost_link_info_cb(
					hdd_ctx->psoc,
					hdd_lost_link_cp_stats_info_cb);
}
#endif
