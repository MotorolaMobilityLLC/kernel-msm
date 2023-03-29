/*
 * Copyright (c) 2015-2020 The Linux Foundation. All rights reserved.
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

#ifndef __WLAN_HDD_OCB_H
#define __WLAN_HDD_OCB_H

#include <net/iw_handler.h>
#include "sir_api.h"

#define WLAN_OCB_CHANNEL_MAX 5

/**
 * struct ocb_qos_params - QoS Parameters for each AC
 * @aifsn:  Arbitration Inter-Frame Spacing
 * @cwmin:  Contention Window (Min)
 * @cwmax:  Contention Window (Max)
 */
struct ocb_qos_params {
	uint8_t aifsn;
	uint8_t cwmin;
	uint8_t cwmax;
};

/**
 * struct ocb_channel - Parameters for each OCB channel
 * @channel_freq:           Channel Center Frequency (MHz)
 * @duration:               Channel Duration (ms)
 * @start_guard_interval:   Start Guard Interval (ms)
 * @channel_bandwidth:      Channel Bandwidth (MHz)
 * @tx_power:               Transmit Power (1/2 dBm)
 * @tx_rate:                Transmit Data Rate (mbit)
 * @qos_params:             Array of QoS Parameters
 * @per_packet_rx_stats:    Enable per packet RX statistics
 */
struct ocb_channel {
	uint32_t channel_freq;
	uint32_t duration;
	uint32_t start_guard_interval;
	uint32_t channel_bandwidth;
	uint32_t tx_power;
	uint32_t tx_rate;
	struct ocb_qos_params qos_params[QCA_WLAN_AC_ALL];
	uint32_t per_packet_rx_stats;
};

/**
 * struct dot11p_channel_sched - OCB channel schedule
 * @num_channels:   Number of channels
 * @channels:       Array of channel parameters
 * @off_channel_tx: Enable off channel TX
 */
struct dot11p_channel_sched {
	uint32_t num_channels;
	struct ocb_channel channels[WLAN_OCB_CHANNEL_MAX];
	uint32_t off_channel_tx;
};

/**
 * enum qca_wlan_vendor_attr_ocb_set_config - vendor subcmd to set ocb config
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_COUNT:
 *	number of channels in the configuration
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_SIZE: size of the schedule
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_ARRAY: array of channels
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_ARRAY:
 *	array of channels to be scheduled
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_CHANNEL_ARRAY:
 *	array of NDL channel information
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_ACTIVE_STATE_ARRAY:
 *	array of NDL active state configuration
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_FLAGS:
 *	flag to set the absolute expiry
 */
enum qca_wlan_vendor_attr_ocb_set_config {
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_COUNT,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_SIZE,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_CHANNEL_ARRAY,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_SCHEDULE_ARRAY,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_CHANNEL_ARRAY,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_NDL_ACTIVE_STATE_ARRAY,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_FLAGS,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_MAX =
		QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ocb_set_utc_time - vendor subcmd to set UTC time
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_VALUE:
 *	the UTC time as an array of 10 bytes
 * @QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_ERROR:
 *	the time error as an array of 5 bytes
 */
enum qca_wlan_vendor_attr_ocb_set_utc_time {
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_VALUE,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_ERROR,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_MAX =
		QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ocb_start_timing_advert - vendor subcmd to start
						       sending timing advert
						       frames
 * @QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_CHANNEL_FREQ:
 *	channel frequency on which to send the frames
 * @QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_REPEAT_RATE:
 *	number of times the frame is sent in 5 seconds
 */
enum qca_wlan_vendor_attr_ocb_start_timing_advert {
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_CHANNEL_FREQ,
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_REPEAT_RATE,
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_MAX =
		QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ocb_stop_timing_advert - vendor subcmd to stop
 *						      timing advert
 * @QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_CHANNEL_FREQ:
 *	the channel frequency on which to stop the timing advert
 */
enum qca_wlan_vendor_attr_ocb_stop_timing_advert {
	QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_CHANNEL_FREQ,
	QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_MAX =
		QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_dcc_get_tsf_response - vendor subcmd to get TSF
 *						    timer value
 * @QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_HIGH:
 *      higher 32 bits of the timer
 * @QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_LOW:
 *      lower 32 bits of the timer
 */
enum qca_wlan_vendor_attr_ocb_get_tsf_resp {
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_HIGH,
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_TIMER_LOW,
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_MAX =
		QCA_WLAN_VENDOR_ATTR_OCB_GET_TSF_RESP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_dcc_get_stats - vendor subcmd to get
 *					     dcc stats
 * @QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_CHANNEL_COUNT:
 *      the number of channels in the request array
 * @QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_REQUEST_ARRAY
 *      array of the channel and information being requested
 */
enum qca_wlan_vendor_attr_dcc_get_stats {
	QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_CHANNEL_COUNT,
	QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_REQUEST_ARRAY,
	QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_MAX =
		QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_dcc_get_stats_resp - response event from get
 *						  dcc stats
 * @QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_CHANNEL_COUNT:
 *      the number of channels in the request array
 * @QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_STATS_ARRAY
 *      array of the information being requested
 */
enum qca_wlan_vendor_attr_dcc_get_stats_resp {
	QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_CHANNEL_COUNT,
	QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_STATS_ARRAY,
	QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_MAX =
		QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_RESP_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_dcc_clear_stats - vendor subcmd to clear DCC stats
 * @QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_BITMAP:
 *      mask of the type of stats to be cleared
 */
enum qca_wlan_vendor_attr_dcc_clear_stats {
	QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_BITMAP,
	QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_MAX =
		QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_AFTER_LAST - 1,
};

/**
 * enum qca_wlan_vendor_attr_ocb_set_config - vendor subcmd to update dcc
 * @QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_COUNT:
 *	number of channels in the configuration
 * @QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_ARRAY: the array of NDL
 *  channel info
 * @QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_ACTIVE_STATE_ARRAY: the array of
 *  NDL active states
 */
enum qca_wlan_vendor_attr_dcc_update_ndl {
	QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_COUNT,
	QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_CHANNEL_ARRAY,
	QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_ACTIVE_STATE_ARRAY,
	QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_MAX =
		QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_AFTER_LAST - 1,
};

struct hdd_context;

#ifdef WLAN_WEXT_SUPPORT_ENABLE

#ifdef WLAN_FEATURE_DSRC
int iw_set_dot11p_channel_sched(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra);

extern const struct nla_policy qca_wlan_vendor_ocb_set_config_policy[
			QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_MAX + 1];
extern const struct nla_policy qca_wlan_vendor_ocb_set_utc_time_policy[
			QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_MAX + 1];
extern const struct nla_policy qca_wlan_vendor_ocb_start_timing_advert_policy[
			QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_MAX + 1];
extern const struct nla_policy  qca_wlan_vendor_ocb_stop_timing_advert_policy[
			QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_MAX + 1];
extern const struct nla_policy qca_wlan_vendor_dcc_get_stats[
			QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_MAX + 1];
extern const struct nla_policy qca_wlan_vendor_dcc_clear_stats[
			QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_MAX + 1];
extern const struct nla_policy qca_wlan_vendor_dcc_update_ndl[
			QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_MAX + 1];

#define FEATURE_OCB_VENDOR_COMMANDS					     \
{									     \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			     \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OCB_SET_CONFIG,	     \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_NETDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_RUNNING,				     \
	.doit = wlan_hdd_cfg80211_ocb_set_config,			     \
	vendor_command_policy(qca_wlan_vendor_ocb_set_config_policy,	     \
			      QCA_WLAN_VENDOR_ATTR_OCB_SET_CONFIG_MAX)	     \
},									     \
									     \
{									     \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			     \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OCB_SET_UTC_TIME,	     \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_NETDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_RUNNING,				     \
	.doit = wlan_hdd_cfg80211_ocb_set_utc_time,			     \
	vendor_command_policy(qca_wlan_vendor_ocb_set_utc_time_policy,	     \
			      QCA_WLAN_VENDOR_ATTR_OCB_SET_UTC_TIME_MAX)     \
},									     \
									     \
{									     \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			     \
	.info.subcmd =							     \
		QCA_NL80211_VENDOR_SUBCMD_OCB_START_TIMING_ADVERT,	     \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_NETDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_RUNNING,				     \
	.doit = wlan_hdd_cfg80211_ocb_start_timing_advert,		     \
	vendor_command_policy(						     \
			qca_wlan_vendor_ocb_start_timing_advert_policy,	     \
			QCA_WLAN_VENDOR_ATTR_OCB_START_TIMING_ADVERT_MAX)    \
},									     \
									     \
{									     \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			     \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OCB_STOP_TIMING_ADVERT,     \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_NETDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_RUNNING,				     \
	.doit = wlan_hdd_cfg80211_ocb_stop_timing_advert,		     \
	vendor_command_policy(						     \
			qca_wlan_vendor_ocb_stop_timing_advert_policy,	     \
			QCA_WLAN_VENDOR_ATTR_OCB_STOP_TIMING_ADVERT_MAX)     \
},									     \
									     \
{									     \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			     \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OCB_GET_TSF_TIMER,	     \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_NETDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_RUNNING,				     \
	.doit = wlan_hdd_cfg80211_ocb_get_tsf_timer,			     \
	vendor_command_policy(VENDOR_CMD_RAW_DATA, 0)			     \
},									     \
									     \
{									     \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			     \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCC_GET_STATS,		     \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_NETDEV |				     \
		 WIPHY_VENDOR_CMD_NEED_RUNNING,				     \
	.doit = wlan_hdd_cfg80211_dcc_get_stats,			     \
	vendor_command_policy(qca_wlan_vendor_dcc_get_stats,		     \
			      QCA_WLAN_VENDOR_ATTR_DCC_GET_STATS_MAX)	     \
},									     \
									     \
{									     \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			     \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCC_CLEAR_STATS,	     \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				     \
		WIPHY_VENDOR_CMD_NEED_NETDEV |				     \
		WIPHY_VENDOR_CMD_NEED_RUNNING,				     \
	.doit = wlan_hdd_cfg80211_dcc_clear_stats,			     \
	vendor_command_policy(qca_wlan_vendor_dcc_clear_stats,		     \
			      QCA_WLAN_VENDOR_ATTR_DCC_CLEAR_STATS_MAX)	     \
},									     \
									     \
{									     \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,			     \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_DCC_UPDATE_NDL,	     \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |				     \
		WIPHY_VENDOR_CMD_NEED_NETDEV |				     \
		WIPHY_VENDOR_CMD_NEED_RUNNING,				     \
	.doit = wlan_hdd_cfg80211_dcc_update_ndl,			     \
	vendor_command_policy(qca_wlan_vendor_dcc_update_ndl,		     \
			      QCA_WLAN_VENDOR_ATTR_DCC_UPDATE_NDL_MAX)	     \
},

#else
#define FEATURE_OCB_VENDOR_COMMANDS

static inline
int iw_set_dot11p_channel_sched(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	return 0;
}
#endif
#else
#define FEATURE_OCB_VENDOR_COMMANDS
#endif

#ifdef WLAN_FEATURE_DSRC
int wlan_hdd_cfg80211_ocb_set_config(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len);

int wlan_hdd_cfg80211_ocb_set_utc_time(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data,
				       int data_len);

int wlan_hdd_cfg80211_ocb_start_timing_advert(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data,
					      int data_len);

int wlan_hdd_cfg80211_ocb_stop_timing_advert(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     const void *data,
					     int data_len);

int wlan_hdd_cfg80211_ocb_get_tsf_timer(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					const void *data,
					int data_len);

int wlan_hdd_cfg80211_dcc_get_stats(struct wiphy *wiphy,
				    struct wireless_dev *wdev,
				    const void *data,
				    int data_len);

int wlan_hdd_cfg80211_dcc_clear_stats(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len);

int wlan_hdd_cfg80211_dcc_update_ndl(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len);

void wlan_hdd_dcc_register_for_dcc_stats_event(struct hdd_context *hdd_ctx);

void wlan_hdd_dcc_stats_event(void *context_ptr, void *response_ptr);
#else
static inline int wlan_hdd_cfg80211_ocb_set_config(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	return 0;
}

static inline int wlan_hdd_cfg80211_ocb_set_utc_time(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	return 0;
}

static inline int wlan_hdd_cfg80211_ocb_start_timing_advert(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	return 0;
}

static inline int wlan_hdd_cfg80211_ocb_stop_timing_advert(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	return 0;
}

static inline int wlan_hdd_cfg80211_ocb_get_tsf_timer(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	return 0;
}

static inline int wlan_hdd_cfg80211_dcc_get_stats(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	return 0;
}

static inline int wlan_hdd_cfg80211_dcc_clear_stats(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	return 0;
}

static inline int wlan_hdd_cfg80211_dcc_update_ndl(struct wiphy *wiphy,
		struct wireless_dev *wdev,
		const void *data,
		int data_len)
{
	return 0;
}

static inline void wlan_hdd_dcc_register_for_dcc_stats_event(
		struct hdd_context *hdd_ctx)
{
}

static inline void wlan_hdd_dcc_stats_event(void *context_ptr,
		void *response_ptr)
{
}
#endif

#endif /* __WLAN_HDD_OCB_H */
