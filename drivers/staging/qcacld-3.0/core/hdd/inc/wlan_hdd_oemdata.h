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
 * DOC: wlan_hdd_oemdata.h
 *
 * Internal includes for the oem data
 */

#ifndef __WLAN_HDD_OEM_DATA_H__
#define __WLAN_HDD_OEM_DATA_H__

#include "wmi_unified_param.h"

struct hdd_context;

#ifdef FEATURE_OEM_DATA
#define WLAN_WAIT_TIME_GET_OEM_DATA 1000
#endif
#ifdef FEATURE_OEM_DATA_SUPPORT

#ifndef OEM_DATA_REQ_SIZE
#define OEM_DATA_REQ_SIZE 500
#endif

#ifndef OEM_DATA_RSP_SIZE
#define OEM_DATA_RSP_SIZE 1724
#endif

#define OEM_APP_SIGNATURE_LEN      16
#define OEM_APP_SIGNATURE_STR      "QUALCOMM-OEM-APP"

#define OEM_TARGET_SIGNATURE_LEN   8
#define OEM_TARGET_SIGNATURE       "QUALCOMM"

#define OEM_CAP_MAX_NUM_CHANNELS   128

/**
 * enum oem_err_code - OEM error codes
 * @OEM_ERR_NULL_CONTEXT: %NULL context
 * @OEM_ERR_APP_NOT_REGISTERED: OEM App is not registered
 * @OEM_ERR_INVALID_SIGNATURE: Invalid signature
 * @OEM_ERR_NULL_MESSAGE_HEADER: Invalid message header
 * @OEM_ERR_INVALID_MESSAGE_TYPE: Invalid message type
 * @OEM_ERR_INVALID_MESSAGE_LENGTH: Invalid length in message body
 */
enum oem_err_code {
	OEM_ERR_NULL_CONTEXT = 1,
	OEM_ERR_APP_NOT_REGISTERED,
	OEM_ERR_INVALID_SIGNATURE,
	OEM_ERR_NULL_MESSAGE_HEADER,
	OEM_ERR_INVALID_MESSAGE_TYPE,
	OEM_ERR_INVALID_MESSAGE_LENGTH
};

/**
 * struct driver_version - Driver version identifier (w.x.y.z)
 * @major: Version ID major number
 * @minor: Version ID minor number
 * @patch: Version ID patch number
 * @build: Version ID build number
 */
struct driver_version {
	uint8_t major;
	uint8_t minor;
	uint8_t patch;
	uint8_t build;
};

/**
 * struct oem_data_cap - OEM Data Capabilities
 * @oem_target_signature: Signature of chipset vendor, e.g. QUALCOMM
 * @oem_target_type: Chip type
 * @oem_fw_version: Firmware version
 * @driver_version: Host software version
 * @allowed_dwell_time_min: Channel dwell time - allowed minimum
 * @allowed_dwell_time_max: Channel dwell time - allowed maximum
 * @curr_dwell_time_min: Channel dwell time - current minimim
 * @curr_dwell_time_max: Channel dwell time - current maximum
 * @supported_bands: Supported bands, 2.4G or 5G Hz
 * @num_channels: Num of channels IDs to follow
 * @channel_list: List of channel IDs
 */
struct oem_data_cap {
	uint8_t oem_target_signature[OEM_TARGET_SIGNATURE_LEN];
	uint32_t oem_target_type;
	uint32_t oem_fw_version;
	struct driver_version driver_version;
	uint16_t allowed_dwell_time_min;
	uint16_t allowed_dwell_time_max;
	uint16_t curr_dwell_time_min;
	uint16_t curr_dwell_time_max;
	uint16_t supported_bands;
	uint16_t num_channels;
	uint8_t channel_list[OEM_CAP_MAX_NUM_CHANNELS];
};

/**
 * struct hdd_channel_info - Channel information
 * @reserved0: reserved for padding and future use
 * @mhz: primary 20 MHz channel frequency in mhz
 * @band_center_freq1: Center frequency 1 in MHz
 * @band_center_freq2: Center frequency 2 in MHz, valid only for 11ac
 *	VHT 80+80 mode
 * @info: channel info
 * @reg_info_1: regulatory information field 1 which contains min power,
 *	max power, reg power and reg class id
 * @reg_info_2: regulatory information field 2 which contains antennamax
 */
struct hdd_channel_info {
	uint32_t reserved0;
	uint32_t mhz;
	uint32_t band_center_freq1;
	uint32_t band_center_freq2;
	uint32_t info;
	uint32_t reg_info_1;
	uint32_t reg_info_2;
};

/**
 * struct peer_status_info - Status information for a given peer
 * @peer_mac_addr: peer mac address
 * @peer_status: peer status: 1: CONNECTED, 2: DISCONNECTED
 * @vdev_id: vdev_id for the peer mac
 * @peer_capability: peer capability: 0: RTT/RTT2, 1: RTT3. Default is 0
 * @reserved0: reserved0
 * @peer_chan_info: channel info on which peer is connected
 */
struct peer_status_info {
	uint8_t peer_mac_addr[ETH_ALEN];
	uint8_t peer_status;
	uint8_t vdev_id;
	uint32_t peer_capability;
	uint32_t reserved0;
	struct hdd_channel_info peer_chan_info;
};

/**
 * enum oem_capability_mask - mask field for userspace client capabilities
 * @OEM_CAP_RM_FTMRR: FTM range report mask bit
 * @OEM_CAP_RM_LCI: LCI capability mask bit
 */
enum oem_capability_mask {
	OEM_CAP_RM_FTMRR = (1 << (0)),
	OEM_CAP_RM_LCI = (1 << (1)),
};

/**
 * struct oem_get_capability_rsp - capabilities set by userspace and target.
 * @target_cap: target capabilities
 * @client_capabilities: capabilities set by userspace via set request
 */
struct oem_get_capability_rsp {
	struct oem_data_cap target_cap;
	struct sme_oem_capability cap;
};

/**
 * hdd_send_peer_status_ind_to_oem_app() -
 * Function to send peer status to a registered application
 * @peer_mac: MAC address of peer
 * @peer_status: ePeerConnected or ePeerDisconnected
 * @peer_capability: 0: RTT/RTT2, 1: RTT3. Default is 0
 * @vdev_id: vdev_id
 * @chan_info: operating channel information
 * @dev_mode: dev mode for which indication is sent
 *
 * Return: none
 */
void hdd_send_peer_status_ind_to_oem_app(struct qdf_mac_addr *peer_mac,
					 uint8_t peer_status,
					 uint8_t peer_capability,
					 uint8_t vdev_id,
					 struct oem_channel_info *chan_info,
					 enum QDF_OPMODE dev_mode);

int iw_get_oem_data_cap(struct net_device *dev, struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra);

/**
 * oem_activate_service() - API to register the oem command handler
 * @hdd_ctx: Pointer to HDD Context
 *
 * This API is used to register the handler to receive netlink message
 * from an OEM application process
 *
 * Return: 0 on success and errno on failure
 */
int oem_activate_service(struct hdd_context *hdd_ctx);

/**
 * oem_deactivate_service() - API to unregister the oem command handler
 *
 * This API is used to deregister the handler to receive netlink message
 * from an OEM application process
 *
 * Return: 0 on success and errno on failure
 */
int oem_deactivate_service(void);

void hdd_send_oem_data_rsp_msg(struct oem_data_rsp *oem_rsp);

/**
 * update_channel_bw_info() - set bandwidth info for the chan
 * @hdd_ctx: hdd context
 * @chan_freq: channel freq for which info are required
 * @chan_info: struct where the bandwidth info is filled
 *
 * This function finds the maximum bandwidth allowed, secondary
 * channel offset and center freq for the channel as per regulatory
 * domain and uses these info calculate the phy mode for the
 * channel.
 *
 * Return: void
 */
void hdd_update_channel_bw_info(struct hdd_context *hdd_ctx,
				uint32_t chan_freq,
				void *hdd_chan_info);
#else
static inline int oem_activate_service(struct hdd_context *hdd_ctx)
{
	return 0;
}

static inline int oem_deactivate_service(void)
{
	return 0;
}

static inline void hdd_send_oem_data_rsp_msg(void *oem_rsp) {}

static inline void hdd_update_channel_bw_info(struct hdd_context *hdd_ctx,
					      uint32_t chan_freq,
					      void *hdd_chan_info) {}
#endif /* FEATURE_OEM_DATA_SUPPORT */

#ifdef FEATURE_OEM_DATA
#define OEM_DATA_MAX_SIZE 1024
/**
 * wlan_hdd_cfg80211_oem_data_handler() - the handler for oem data
 * @wiphy: wiphy structure pointer
 * @wdev: Wireless device structure pointer
 * @data: Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */
int wlan_hdd_cfg80211_oem_data_handler(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data, int data_len);

extern const struct nla_policy
	oem_data_attr_policy
	[QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_MAX + 1];

#define FEATURE_OEM_DATA_VENDOR_COMMANDS                                \
{                                                                       \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                        \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_OEM_DATA,              \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                           \
		WIPHY_VENDOR_CMD_NEED_NETDEV |                          \
		WIPHY_VENDOR_CMD_NEED_RUNNING,                          \
	.doit = wlan_hdd_cfg80211_oem_data_handler,                     \
	vendor_command_policy(oem_data_attr_policy,                     \
			      QCA_WLAN_VENDOR_ATTR_OEM_DATA_PARAMS_MAX) \
},
#else
#define FEATURE_OEM_DATA_VENDOR_COMMANDS
#endif

#ifdef FEATURE_OEM_DATA
/**
 * hdd_oem_event_handler_cb() - callback for oem data event
 * @oem_event_data: oem data received in the event from the FW
 * @vdev_id: vdev id
 *
 * Return: None
 */
void hdd_oem_event_handler_cb(const struct oem_data *oem_event_data,
			      uint8_t vdev_id);
#else
static inline void hdd_oem_event_handler_cb(void *oem_event_data,
					    uint8_t vdev_id)
{
}
#endif

#endif /* __WLAN_HDD_OEM_DATA_H__ */
