/*
 * Copyright (c) 2013-2020, The Linux Foundation. All rights reserved.
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

/*
 * This file contains the API definitions for the ROAMING WMI APIs.
 */

#ifndef _WMI_UNIFIED_ROAM_PARAM_H_
#define _WMI_UNIFIED_ROAM_PARAM_H_

#include <wlan_blm_public_struct.h>
#include <wlan_cm_bss_score_param.h>

/**
 * struct gateway_update_req_param - gateway parameter update request
 * @request_id: request id
 * @vdev_id: vdev id
 * @max_retries: Max ARP/NS retry attempts
 * @timeout: Retry interval
 * @ipv4_addr_type: on ipv4 network
 * @ipv6_addr_type: on ipv6 network
 * @gw_mac_addr: gateway mac addr
 * @ipv4_addr: ipv4 addr
 * @ipv6_addr: ipv6 addr
 */
struct gateway_update_req_param {
	uint32_t     request_id;
	uint32_t     vdev_id;
	uint32_t     max_retries;
	uint32_t     timeout;
	uint32_t     ipv4_addr_type;
	uint32_t     ipv6_addr_type;
	struct qdf_mac_addr  gw_mac_addr;
	uint8_t      ipv4_addr[QDF_IPV4_ADDR_SIZE];
	uint8_t      ipv6_addr[QDF_IPV6_ADDR_SIZE];
};

/**
 * struct rssi_monitor_param - rssi monitoring
 * @request_id: request id
 * @vdev_id: vdev id
 * @min_rssi: minimum rssi
 * @max_rssi: maximum rssi
 * @control: flag to indicate start or stop
 */
struct rssi_monitor_param {
	uint32_t request_id;
	uint32_t vdev_id;
	int8_t   min_rssi;
	int8_t   max_rssi;
	bool     control;
};

#define WMI_CFG_VALID_CHANNEL_LIST_LEN    100
/* Occupied channel list remains static */
#define WMI_CHANNEL_LIST_STATIC                   1
/* Occupied channel list can be learnt after init */
#define WMI_CHANNEL_LIST_DYNAMIC_INIT             2
/* Occupied channel list can be learnt after flush */
#define WMI_CHANNEL_LIST_DYNAMIC_FLUSH            3
/* Occupied channel list can be learnt after update */
#define WMI_CHANNEL_LIST_DYNAMIC_UPDATE           4

/**
 * struct plm_req_params - plm req parameter
 * @diag_token: Dialog token
 * @meas_token: measurement token
 * @num_bursts: total number of bursts
 * @burst_int: burst interval in seconds
 * @meas_duration:in TU's,STA goes off-ch
 * @burst_len: no of times the STA should cycle through PLM ch list
 * @desired_tx_pwr: desired tx power
 * @mac_addr: MC dest addr
 * @plm_num_ch: channel numbers
 * @plm_ch_freq_list: channel frequency list
 * @vdev_id: vdev id
 * @enable:  enable/disable
 */
struct plm_req_params {
	uint16_t diag_token;
	uint16_t meas_token;
	uint16_t num_bursts;
	uint16_t burst_int;
	uint16_t meas_duration;
	/* no of times the STA should cycle through PLM ch list */
	uint8_t burst_len;
	int8_t desired_tx_pwr;
	struct qdf_mac_addr mac_addr;
	/* no of channels */
	uint8_t plm_num_ch;
	/* channel frequency list */
	uint32_t plm_ch_freq_list[WMI_CFG_VALID_CHANNEL_LIST_LEN];
	uint8_t vdev_id;
	bool enable;
};

/**
 * struct wmi_roam_invoke_cmd - roam invoke command
 * @vdev_id: vdev id
 * @bssid: mac address
 * @channel: channel
 * @frame_len: frame length, includs mac header, fixed params and ies
 * @frame_buf: buffer contaning probe response or beacon
 * @is_same_bssid: flag to indicate if roaming is requested for same bssid
 * @forced_roaming: Roam to any bssid in any ch (here bssid & ch is not given)
 */
struct wmi_roam_invoke_cmd {
	uint32_t vdev_id;
	uint8_t bssid[QDF_MAC_ADDR_SIZE];
	uint32_t channel;
	uint32_t frame_len;
	uint8_t *frame_buf;
	uint8_t is_same_bssid;
	bool forced_roaming;
};

/**
 * struct wmi_limit_off_chan_param - limit off channel parameters
 * @vdev_id: vdev id
 * @status: status of the command (enable/disable)
 * @max_offchan_time: max off channel time
 * @rest_time: home channel time
 * @skip_dfs_chans: skip dfs channels during scan
 */
struct wmi_limit_off_chan_param {
	uint32_t vdev_id;
	bool status;
	uint32_t max_offchan_time;
	uint32_t rest_time;
	bool skip_dfs_chans;
};

#define WMI_MAX_HLP_IE_LEN 2048
/**
 * struct hlp_params - HLP info params
 * @vdev_id: vdev id
 * @hlp_ie_len: HLP IE length
 * @hlp_ie: HLP IE
 */
struct hlp_params {
	uint8_t vdev_id;
	uint32_t  hlp_ie_len;
	uint8_t hlp_ie[WMI_MAX_HLP_IE_LEN];
};

/**
 * struct wmi_roam_auth_status_params - WPA3 roam auth response status
 * parameters
 * @vdev_id: Vdev on which roam preauth is happening
 * @preauth_status: Status of the Auth response.
 *      IEEE80211_STATUS_SUCCESS(0) for success. Corresponding
 *      IEEE80211 failure status code for failure.
 *
 * @bssid: Candidate BSSID
 * @pmkid: PMKID derived for the auth
 */
struct wmi_roam_auth_status_params {
	uint32_t vdev_id;
	uint32_t preauth_status;
	struct qdf_mac_addr bssid;
	uint8_t pmkid[PMKID_LEN];
};

/**
 * struct wmi_invoke_neighbor_report_params - Invoke neighbor report request
 *	from IW to FW
 * @vdev_id: vdev id
 * @send_resp_to_host: bool to send response to host or not
 * @ssid: ssid given from the IW command
 */
struct wmi_invoke_neighbor_report_params {
	uint32_t vdev_id;
	uint32_t send_resp_to_host;
	struct wlan_ssid ssid;
};

/**
 * set_pcl_cmd_params  - Set PCL command params
 * @vdev_id: Vdev id
 * @weight: PCL weight
 */
struct set_pcl_cmd_params {
	uint8_t vdev_id;
	struct wmi_pcl_chan_weights *weights;
};
#endif /* _WMI_UNIFIED_ROAM_PARAM_H_ */
