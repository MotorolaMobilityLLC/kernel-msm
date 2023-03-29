/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
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
 * DOC: contains ocb structure definitions
 */

#ifndef _WLAN_OCB_STRUCTS_H_
#define _WLAN_OCB_STRUCTS_H_
#include <qdf_status.h>
#include "qca_vendor.h"

/* Don't add the RX stats header to packets received on this channel */
#define OCB_CHANNEL_FLAG_DISABLE_RX_STATS_HDR	(1 << 0)

/* The size of the utc time in bytes. */
#define OCB_SIZE_UTC_TIME                       (10)

/* The size of the utc time error in bytes. */
#define OCB_SIZE_UTC_TIME_ERROR                 (5)

/**
 * struct ocb_utc_param - parameters to set UTC time
 * @vdev_id: vdev id
 * @utc_time: number of nanoseconds from Jan 1st 1958
 * @time_error: the error in the UTC time. All 1's for unknown
 */
struct ocb_utc_param {
	uint32_t vdev_id;
	uint8_t utc_time[OCB_SIZE_UTC_TIME];
	uint8_t time_error[OCB_SIZE_UTC_TIME_ERROR];
};

/**
 * struct ocb_timing_advert_param - parameters to start/stop
 *  timing advertisement
 * @vdev_id: vdev id
 * @chan_freq: frequency on which to advertise (unit in Mhz)
 * @repeat_rate: the number of times it will send TA in 5 seconds
 * @timestamp_offset: offset of the timestamp field in the TA frame
 * @time_value_offset: offset of the time_value field in the TA frame
 * @template_length: size in bytes of the TA frame
 * @template_value: the TA frame
 */
struct ocb_timing_advert_param {
	uint32_t vdev_id;
	uint32_t chan_freq;
	uint32_t repeat_rate;
	uint32_t timestamp_offset;
	uint32_t time_value_offset;
	uint32_t template_length;
	uint8_t *template_value;
};

/**
 * struct ocb_dcc_get_stats_param - parameters to get DCC stats
 * @vdev_id: vdev id
 * @channel_count: number of dcc channels
 * @request_array_len: size in bytes of the request array
 * @request_array: the request array
 */
struct ocb_dcc_get_stats_param {
	uint32_t vdev_id;
	uint32_t channel_count;
	uint32_t request_array_len;
	void *request_array;
};

/**
 * struct ocb_dcc_update_ndl_param - parameters to update NDL
 * @vdev_id: vdev id
 * @channel_count: number of channels to be updated
 * @dcc_ndl_chan_list_len: size in bytes of the ndl_chan array
 * @dcc_ndl_chan_list: the ndl_chan array
 * @dcc_ndl_active_state_list_len: size in bytes of the active_state array
 * @dcc_ndl_active_state_list: the active state array
 */
struct ocb_dcc_update_ndl_param {
	uint32_t vdev_id;
	uint32_t channel_count;
	uint32_t dcc_ndl_chan_list_len;
	void *dcc_ndl_chan_list;
	uint32_t dcc_ndl_active_state_list_len;
	void *dcc_ndl_active_state_list;
};

/**
 * struct ocb_config_schdl - parameters for channel scheduling
 * @chan_freq: frequency of the channel (unit in Mhz)
 * @total_duration: duration of the schedule (unit in ms)
 * @guard_interval: guard interval on the start of the schedule (unit in ms)
 */
struct ocb_config_schdl {
	uint32_t chan_freq;
	uint32_t total_duration;
	uint32_t guard_interval;
};

/**
 * struct ocb_wmm_param - WMM parameters
 * @aifsn: AIFS number
 * @cwmin: value of CWmin
 * @cwmax: value of CWmax
 */
struct ocb_wmm_param {
	uint8_t aifsn;
	uint8_t cwmin;
	uint8_t cwmax;
};

/**
 * struct ocb_config_chan - parameters to configure a channel
 * @chan_freq: frequency of the channel (unit in MHz)
 * @bandwidth: bandwidth of the channel, either 10 or 20 MHz
 * @mac_address: MAC address assigned to this channel
 * @qos_params: QoS parameters
 * @max_pwr: maximum transmit power of the channel (dBm)
 * @min_pwr: minimum transmit power of the channel (dBm)
 * @reg_pwr: maximum transmit power specified by the regulatory domain (dBm)
 * @antenna_max: maximum antenna gain specified by the regulatory domain (dB)
 * @flags: bit0: 0 enable RX stats on this channel; 1 disable RX stats
 *         bit1: flag to indicate TSF expiry time in TX control.
 *               0 relative time is used. 1 absolute time is used.
 *         bit2: Frame mode from user layer.
 *               0 for 802.3 frame, 1 for 802.11 frame.
 * @ch_mode: channel mode
 */
struct ocb_config_chan {
	uint32_t chan_freq;
	uint32_t bandwidth;
	struct qdf_mac_addr mac_address;
	struct ocb_wmm_param qos_params[QCA_WLAN_AC_ALL];
	uint32_t max_pwr;
	uint32_t min_pwr;
	uint8_t reg_pwr;
	uint8_t antenna_max;
	uint16_t flags;
	uint32_t ch_mode;
};

/**
 * struct ocb_config - parameters for OCB vdev config
 * @vdev_id: vdev id
 * @channel_count: number of channels
 * @schedule_size: size of the channel schedule
 * @flags: reserved
 * @channels: array of OCB channels
 * @schedule: array of OCB schedule elements
 * @dcc_ndl_chan_list_len: size in bytes of the ndl_chan array
 * @dcc_ndl_chan_list: array of dcc channel info
 * @dcc_ndl_active_state_list_len: size in bytes of the active state array
 * @dcc_ndl_active_state_list: array of active states
 */
struct ocb_config {
	uint32_t vdev_id;
	uint32_t channel_count;
	uint32_t schedule_size;
	uint32_t flags;
	struct ocb_config_chan *channels;
	struct ocb_config_schdl *schedule;
	uint32_t dcc_ndl_chan_list_len;
	void *dcc_ndl_chan_list;
	uint32_t dcc_ndl_active_state_list_len;
	void *dcc_ndl_active_state_list;
};

/**
 * enum ocb_channel_config_status - ocb config status
 * @OCB_CHANNEL_CONFIG_SUCCESS: success
 * @OCB_CHANNEL_CONFIG_FAIL: failure
 * @OCB_CHANNEL_CONFIG_STATUS_MAX: place holder, not a real status
 */
enum ocb_channel_config_status {
	OCB_CHANNEL_CONFIG_SUCCESS = 0,
	OCB_CHANNEL_CONFIG_FAIL,
	OCB_CHANNEL_CONFIG_STATUS_MAX
};

/**
 * struct ocb_set_config_response - ocb config status
 * @status: response status. OCB_CHANNEL_CONFIG_SUCCESS for success.
 */
struct ocb_set_config_response {
	enum ocb_channel_config_status status;
};

/**
 * struct ocb_get_tsf_timer_response - TSF timer response
 * @vdev_id: vdev id
 * @timer_high: higher 32-bits of the timer
 * @timer_low: lower 32-bits of the timer
 */
struct ocb_get_tsf_timer_response {
	uint32_t vdev_id;
	uint32_t timer_high;
	uint32_t timer_low;
};

/**
 * struct ocb_get_tsf_timer_param - parameters to get tsf timer
 * @vdev_id: vdev id
 */
struct ocb_get_tsf_timer_param {
	uint32_t vdev_id;
};

/**
 * struct ocb_dcc_get_stats_response - DCC stats response
 * @vdev_id: vdev id
 * @num_channels: number of dcc channels
 * @channel_stats_array_len: size in bytes of the stats array
 * @channel_stats_array: the stats array
 */
struct ocb_dcc_get_stats_response {
	uint32_t vdev_id;
	uint32_t num_channels;
	uint32_t channel_stats_array_len;
	void *channel_stats_array;
};

/**
 * struct ocb_dcc_clear_stats_param - parameters to clear DCC stats
 * @vdev_id: vdev id
 * @dcc_stats_bitmap: bitmap of clear option
 */
struct ocb_dcc_clear_stats_param {
	uint32_t vdev_id;
	uint32_t dcc_stats_bitmap;
};

/**
 * struct ocb_dcc_update_ndl_response - NDP update response
 * @vdev_id: vdev id
 * @status: response status
 */
struct ocb_dcc_update_ndl_response {
	uint32_t vdev_id;
	uint32_t status;
};

/**
 * struct wlan_ocb_rx_ops - structure containing rx ops for OCB
 * @ocb_set_config_status: fp to get channel config status
 * @ocb_tsf_timer: fp to get TSF timer
 * @ocb_dcc_ndl_update: fp to get NDL update status
 * @ocb_dcc_stats_indicate: fp to get DCC stats
 */
struct wlan_ocb_rx_ops {
	QDF_STATUS (*ocb_set_config_status)(struct wlan_objmgr_psoc *psoc,
					    uint32_t status);
	QDF_STATUS (*ocb_tsf_timer)(struct wlan_objmgr_psoc *psoc,
				struct ocb_get_tsf_timer_response *response);
	QDF_STATUS (*ocb_dcc_ndl_update)(struct wlan_objmgr_psoc *psoc,
				struct ocb_dcc_update_ndl_response *resp);
	QDF_STATUS (*ocb_dcc_stats_indicate)(struct wlan_objmgr_psoc *psoc,
		struct ocb_dcc_get_stats_response *response, bool indicate);
};

/**
 * struct wlan_ocb_tx_ops - structures containing tx ops for OCB
 * @ocb_set_config: fp to set channel config
 * @ocb_set_utc_time: fp to set utc time
 * @ocb_start_timing_advert: fp to start timing advertisement
 * @ocb_stop_timing_advert: fp to stop timing advertisement
 * @ocb_get_tsf_timer: fp to get tsf timer
 * @ocb_dcc_get_stats: fp to get DCC stats
 * @ocb_dcc_clear_stats: fp to clear DCC stats
 * @ocb_dcc_update_ndl: fp to update ndl
 * @ocb_reg_ev_handler: fp to register event handler
 * @ocb_unreg_ev_handler: fp to unregister event handler
 */
struct wlan_ocb_tx_ops {
	QDF_STATUS (*ocb_set_config)(struct wlan_objmgr_psoc *psoc,
			struct ocb_config *config);
	QDF_STATUS (*ocb_set_utc_time)(struct wlan_objmgr_psoc *psoc,
			struct ocb_utc_param *utc);
	QDF_STATUS (*ocb_start_timing_advert)(struct wlan_objmgr_psoc *psoc,
			struct ocb_timing_advert_param *timing_advert);
	QDF_STATUS (*ocb_stop_timing_advert)(struct wlan_objmgr_psoc *psoc,
			struct ocb_timing_advert_param *timing_advert);
	QDF_STATUS (*ocb_get_tsf_timer)(struct wlan_objmgr_psoc *psoc,
			struct ocb_get_tsf_timer_param *request);
	QDF_STATUS (*ocb_dcc_get_stats)(struct wlan_objmgr_psoc *psoc,
			struct ocb_dcc_get_stats_param *get_stats_param);
	QDF_STATUS (*ocb_dcc_clear_stats)(struct wlan_objmgr_psoc *psoc,
			struct ocb_dcc_clear_stats_param *clear_stats);
	QDF_STATUS (*ocb_dcc_update_ndl)(struct wlan_objmgr_psoc *psoc,
			struct ocb_dcc_update_ndl_param *update_ndl_param);
	QDF_STATUS (*ocb_reg_ev_handler)(struct wlan_objmgr_psoc *psoc,
					 void *arg);
	QDF_STATUS (*ocb_unreg_ev_handler)(struct wlan_objmgr_psoc *psoc,
					   void *arg);
};

typedef void (*ocb_sync_callback)(void *context, void *response);

/**
 * struct ocb_callback - structure containing callback to legacy driver
 * @ocb_set_config_context: context for set channel config callback
 * @ocb_set_config_callback: set channel config callback
 * @ocb_get_tsf_timer_context: context for get tsf timer callback
 * @ocb_get_tsf_timer_callback: get tsf timer callback
 * @ocb_dcc_get_stats_context: context for get DCC stats callback
 * @ocb_dcc_get_stats_callback: get DCC stats callback
 * @ocb_dcc_update_ndl_context: context for NDL update callback
 * @ocb_dcc_update_ndl_callback: NDL update callback
 * @ocb_dcc_stats_event_context: context for DCC stats event callback
 * @ocb_dcc_stats_event_callback: DCC stats event callback
 * @start_ocb_vdev: start ocb callback
 */
struct ocb_callbacks {
	void *ocb_set_config_context;
	ocb_sync_callback ocb_set_config_callback;
	void *ocb_get_tsf_timer_context;
	ocb_sync_callback ocb_get_tsf_timer_callback;
	void *ocb_dcc_get_stats_context;
	ocb_sync_callback ocb_dcc_get_stats_callback;
	void *ocb_dcc_update_ndl_context;
	ocb_sync_callback ocb_dcc_update_ndl_callback;
	void *ocb_dcc_stats_event_context;
	ocb_sync_callback ocb_dcc_stats_event_callback;
	QDF_STATUS (*start_ocb_vdev)(struct ocb_config *config);
};
#endif
