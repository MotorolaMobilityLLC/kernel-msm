/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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
 * DOC : wlan_hdd_spectralscan.h
 *
 * WLAN Host Device Driver spectral scan implementation
 *
 */

#if !defined(WLAN_HDD_SPECTRALSCAN_H)
#define WLAN_HDD_SPECTRALSCAN_H

#ifdef WLAN_CONV_SPECTRAL_ENABLE

/*
 * enum spectral_scan_msg_type - spectral scan registration
 * @SPECTRAL_SCAN_REGISTER_REQ: spectral scan app register request
 * @SPECTRAL_SCAN_REGISTER_RSP: spectral scan app register response
 */
enum spectral_scan_msg_type {
	SPECTRAL_SCAN_REGISTER_REQ,
	SPECTRAL_SCAN_REGISTER_RSP,
};

/*
 * struct spectral_scan_msg - spectral scan request message
 * @msg_type: message type
 * @pid: process id
 */
struct spectral_scan_msg {
	uint32_t msg_type;
	uint32_t pid;
};

/**
 * struct spectral_scan_msg_v - spectral scan request message included version
 * @msg_type: message type
 * @pid: process id
 * @version: version information
 * @sub_version: sub version information
 */
struct spectral_scan_msg_v {
	uint32_t msg_type;
	uint32_t pid;
	uint32_t version;
	uint32_t sub_version;
};

#define FEATURE_SPECTRAL_SCAN_VENDOR_COMMANDS                         \
{                                                                     \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                      \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START, \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | \
			WIPHY_VENDOR_CMD_NEED_NETDEV, \
	.doit = wlan_hdd_cfg80211_spectral_scan_start, \
	vendor_command_policy(spectral_scan_policy, \
			      QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX) \
}, \
{ \
	.info.vendor_id = QCA_NL80211_VENDOR_ID, \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_STOP, \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | \
		WIPHY_VENDOR_CMD_NEED_NETDEV, \
	.doit = wlan_hdd_cfg80211_spectral_scan_stop, \
	vendor_command_policy(spectral_scan_policy, \
			      QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX) \
}, \
{ \
	.info.vendor_id = QCA_NL80211_VENDOR_ID, \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CONFIG, \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | \
			WIPHY_VENDOR_CMD_NEED_NETDEV, \
	.doit = wlan_hdd_cfg80211_spectral_scam_get_config, \
	vendor_command_policy(spectral_scan_policy, \
			      QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX) \
}, \
{ \
	.info.vendor_id = QCA_NL80211_VENDOR_ID, \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_DIAG_STATS, \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                                  \
		WIPHY_VENDOR_CMD_NEED_NETDEV,                                  \
	.doit = wlan_hdd_cfg80211_spectral_scan_get_diag_stats,                \
	vendor_command_policy(VENDOR_CMD_RAW_DATA, 0)                          \
},                                                                             \
{                                                                            \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                             \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_CAP_INFO, \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                                \
			WIPHY_VENDOR_CMD_NEED_NETDEV,                        \
	.doit = wlan_hdd_cfg80211_spectral_scan_get_cap_info,                \
	vendor_command_policy(VENDOR_CMD_RAW_DATA, 0)                        \
},                                                                           \
{                                                                            \
	.info.vendor_id = QCA_NL80211_VENDOR_ID,                             \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_GET_STATUS,   \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV |                                \
		WIPHY_VENDOR_CMD_NEED_NETDEV,                                \
	.doit = wlan_hdd_cfg80211_spectral_scan_get_status,                  \
	vendor_command_policy(VENDOR_CMD_RAW_DATA, 0)                        \
},

/**
 * wlan_hdd_cfg80211_spectral_scan_start() - start spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function starts spectral scan
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_cfg80211_spectral_scan_start(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len);

/**
 * wlan_hdd_cfg80211_spectral_scan_stop() - stop spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function stops spectral scan
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_cfg80211_spectral_scan_stop(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len);

/**
 * wlan_hdd_cfg80211_spectral_scan_start() - start spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function starts spectral scan
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_cfg80211_spectral_scam_get_config(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len);

/**
 * wlan_hdd_cfg80211_spectral_scan_start() - start spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function starts spectral scan
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_cfg80211_spectral_scan_get_diag_stats(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len);

/**
 * wlan_hdd_cfg80211_spectral_scan_start() - start spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function starts spectral scan
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_cfg80211_spectral_scan_get_cap_info(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len);

/**
 * wlan_hdd_cfg80211_spectral_scan_start() - start spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function starts spectral scan
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_cfg80211_spectral_scan_get_status(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len);

/**
 * hdd_spectral_register_to_dbr() - Register spectral to DBR
 * @hdd_ctx: pointer to hdd context
 *
 * This function is that register spectral to Direct Buffer RX component.
 *
 * Return: None
 */
void hdd_spectral_register_to_dbr(struct hdd_context *hdd_ctx);
#else
#define FEATURE_SPECTRAL_SCAN_VENDOR_COMMANDS
static inline void
hdd_spectral_register_to_dbr(struct hdd_context *hdd_ctx)
{
}
#endif

#if defined(CNSS_GENL) && defined(WLAN_CONV_SPECTRAL_ENABLE)
/**
 * spectral_scan_activate_service() - Activate spectral scan  message handler
 * @hdd_ctx: callback context to use
 *
 * This function registers a handler to receive netlink message from
 * the spectral scan application process.
 *
 * Return: None
 */
void spectral_scan_activate_service(struct hdd_context *hdd_ctx);

/**
 * spectral_scan_deactivate_service() - Deactivate spectral scan message handler
 *
 * This function deregisters a handler to receive netlink message from
 * the spectral scan application process.
 *
 * Return: None
 */
void spectral_scan_deactivate_service(void);

/**
 * wlan_spectral_update_rx_chainmask() - API to update rx chainmask before
 * start spectral gen3
 * @adapter: pointer to adapter
 *
 * API to update rx chainmask before start spectral gen3.
 *
 * Return: QDF_STATUS_SUCCESS or non-zero on failure
 */
QDF_STATUS wlan_spectral_update_rx_chainmask(struct hdd_adapter *adapter);
#else
static inline void spectral_scan_activate_service(struct hdd_context *hdd_ctx)
{
}

static inline void spectral_scan_deactivate_service(void)
{
}

#ifdef WLAN_CONV_SPECTRAL_ENABLE
static inline QDF_STATUS
wlan_spectral_update_rx_chainmask(struct hdd_adapter *adapter)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_CONV_SPECTRAL_ENABLE */
#endif
#endif
