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
 *  DOC: wlan_hdd_lpass.c
 *
 *  WLAN Host Device Driver LPASS feature implementation
 *
 */

/* Include Files */
#include "wlan_hdd_main.h"
#include "wlan_hdd_lpass.h"
#include "wlan_hdd_oemdata.h"
#include <cds_utils.h>
#include "qwlan_version.h"

/**
 * wlan_hdd_get_channel_info() - Get channel info
 * @hdd_ctx: HDD context
 * @chan_info: Pointer to the structure that stores channel info
 * @chan_freq: Channel freq
 *
 * Fill in the channel info to chan_info structure.
 */
static void wlan_hdd_get_channel_info(struct hdd_context *hdd_ctx,
				      struct svc_channel_info *chan_info,
				      uint32_t chan_freq)
{
	uint32_t reg_info_1;
	uint32_t reg_info_2;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	status = sme_get_reg_info(hdd_ctx->mac_handle, chan_freq,
				  &reg_info_1, &reg_info_2);
	if (status != QDF_STATUS_SUCCESS)
		return;

	chan_info->mhz = chan_freq;
	chan_info->band_center_freq1 = chan_info->mhz;
	chan_info->band_center_freq2 = 0;
	chan_info->info = 0;
	if (CHANNEL_STATE_DFS ==
	    wlan_reg_get_channel_state_for_freq(hdd_ctx->pdev, chan_freq))
		WMI_SET_CHANNEL_FLAG(chan_info,
				     WMI_CHAN_FLAG_DFS);
	hdd_update_channel_bw_info(hdd_ctx, chan_freq,
				   chan_info);
	chan_info->reg_info_1 = reg_info_1;
	chan_info->reg_info_2 = reg_info_2;
}

/**
 * wlan_hdd_gen_wlan_status_pack() - Create lpass adapter status package
 * @data: Status data record to be created
 * @adapter: Adapter whose status is to being packaged
 * @sta_ctx: Station-specific context of @adapter
 * @is_on: Is wlan driver loaded?
 * @is_connected: Is @adapter connected to an AP?
 *
 * Generate a wlan vdev status package. The status info includes wlan
 * on/off status, vdev ID, vdev mode, supported channels, etc.
 *
 * Return: 0 if package was created, otherwise a negative errno
 */
static int wlan_hdd_gen_wlan_status_pack(struct wlan_status_data *data,
					 struct hdd_adapter *adapter,
					 struct hdd_station_ctx *sta_ctx,
					 uint8_t is_on, uint8_t is_connected)
{
	struct hdd_context *hdd_ctx = NULL;
	int i;
	uint32_t chan_id;
	uint32_t *chan_freq_list, chan_freq_len;
	struct svc_channel_info *chan_info;
	bool lpass_support, wls_6ghz_capable = false;
	QDF_STATUS status;

	if (!data) {
		hdd_err("invalid data pointer");
		return -EINVAL;
	}
	if (!adapter) {
		if (is_on) {
			/* no active interface */
			data->lpss_support = 0;
			data->is_on = is_on;
			return 0;
		}
		hdd_err("invalid adapter pointer");
		return -EINVAL;
	}

	if (wlan_hdd_validate_vdev_id(adapter->vdev_id))
		return -EINVAL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	status = ucfg_mlme_get_lpass_support(hdd_ctx->psoc, &lpass_support);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to get LPASS support config");
		return -EIO;
	}
	ucfg_mlme_get_wls_6ghz_cap(hdd_ctx->psoc, &wls_6ghz_capable);
	if (hdd_ctx->lpss_support && lpass_support)
		data->lpss_support = 1;
	else
		data->lpss_support = 0;

	chan_freq_list =
		qdf_mem_malloc(sizeof(uint32_t) * WLAN_SVC_MAX_NUM_CHAN);
	if (!chan_freq_list)
		return -ENOMEM;

	chan_freq_len = WLAN_SVC_MAX_NUM_CHAN;
	sme_get_cfg_valid_channels(chan_freq_list, &chan_freq_len);

	data->numChannels = 0;
	for (i = 0; i < chan_freq_len; i++) {
		if (!wls_6ghz_capable &&
		    wlan_reg_is_6ghz_chan_freq(chan_freq_list[i]))
			continue;

		chan_id = wlan_reg_freq_to_chan(hdd_ctx->pdev,
						chan_freq_list[i]);
		if (!chan_id)
			continue;

		chan_info = &data->channel_info[data->numChannels];
		data->channel_list[data->numChannels] = chan_id;
		chan_info->chan_id = chan_id;
		wlan_hdd_get_channel_info(hdd_ctx,
					  chan_info,
					  chan_freq_list[i]);
		data->numChannels++;
	}

	qdf_mem_free(chan_freq_list);

	wlan_reg_get_cc_and_src(hdd_ctx->psoc, data->country_code);
	data->is_on = is_on;
	data->vdev_id = adapter->vdev_id;
	data->vdev_mode = adapter->device_mode;
	if (sta_ctx) {
		data->is_connected = is_connected;
		data->rssi = adapter->rssi;
		data->freq = sta_ctx->conn_info.chan_freq;
		if (WLAN_SVC_MAX_SSID_LEN >=
		    sta_ctx->conn_info.ssid.SSID.length) {
			data->ssid_len = sta_ctx->conn_info.ssid.SSID.length;
			memcpy(data->ssid,
			       sta_ctx->conn_info.ssid.SSID.ssId,
			       sta_ctx->conn_info.ssid.SSID.length);
		}
		if (QDF_MAC_ADDR_SIZE >= sizeof(sta_ctx->conn_info.bssid))
			memcpy(data->bssid, sta_ctx->conn_info.bssid.bytes,
			       QDF_MAC_ADDR_SIZE);
	}
	return 0;
}

/**
 * wlan_hdd_gen_wlan_version_pack() - Create lpass version package
 * @data: Version data record to be created
 * @fw_version: Version code from firmware
 * @chip_id: WLAN chip ID
 * @chip_name: WLAN chip name
 *
 * Generate a wlan software/hw version info package. The version info
 * includes wlan host driver version, wlan fw driver version, wlan hw
 * chip id & wlan hw chip name.
 *
 * Return: 0 if package was created, otherwise a negative errno
 */
static int wlan_hdd_gen_wlan_version_pack(struct wlan_version_data *data,
					  uint32_t fw_version,
					  uint32_t chip_id,
					  const char *chip_name)
{
	if (!data) {
		hdd_err("invalid data pointer");
		return -EINVAL;
	}

	data->chip_id = chip_id;
	strlcpy(data->chip_name, chip_name, WLAN_SVC_MAX_STR_LEN);
	if (strncmp(chip_name, "Unknown", 7))
		strlcpy(data->chip_from, "Qualcomm", WLAN_SVC_MAX_STR_LEN);
	else
		strlcpy(data->chip_from, "Unknown", WLAN_SVC_MAX_STR_LEN);
	strlcpy(data->host_version, QWLAN_VERSIONSTR, WLAN_SVC_MAX_STR_LEN);
	scnprintf(data->fw_version, WLAN_SVC_MAX_STR_LEN, "%d.%d.%d.%d",
		  (fw_version & 0xf0000000) >> 28,
		  (fw_version & 0xf000000) >> 24,
		  (fw_version & 0xf00000) >> 20, (fw_version & 0x7fff));
	return 0;
}

/**
 * wlan_hdd_send_status_pkg() - Send adapter status to lpass
 * @adapter: Adapter whose status is to be sent to lpass
 * @sta_ctx: Station-specific context of @adapter
 * @is_on: Is @adapter enabled
 * @is_connected: Is @adapter connected
 *
 * Generate wlan vdev status package and send it to a user space
 * daemon through netlink.
 *
 * Return: none
 */
static void wlan_hdd_send_status_pkg(struct hdd_adapter *adapter,
				     struct hdd_station_ctx *sta_ctx,
				     uint8_t is_on, uint8_t is_connected)
{
	int ret = 0;
	struct wlan_status_data *data = NULL;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam())
		return;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return;

	if (is_on)
		ret = wlan_hdd_gen_wlan_status_pack(data, adapter, sta_ctx,
						    is_on, is_connected);

	if (!ret)
		wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
					    WLAN_SVC_WLAN_STATUS_IND,
					    data, sizeof(*data));
	kfree(data);
}

/**
 * wlan_hdd_send_version_pkg() - report version information to lpass
 * @fw_version: Version code from firmware
 * @chip_id: WLAN chip ID
 * @chip_name: WLAN chip name
 *
 * Generate a wlan sw/hw version info package and send it to a user
 * space daemon through netlink.
 *
 * Return: none
 */
static void wlan_hdd_send_version_pkg(uint32_t fw_version,
				      uint32_t chip_id,
				      const char *chip_name)
{
	int ret = 0;
	struct wlan_version_data data;
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx)
		return;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam())
		return;

	memset(&data, 0, sizeof(struct wlan_version_data));
	ret = wlan_hdd_gen_wlan_version_pack(&data, fw_version, chip_id,
					     chip_name);
	if (!ret)
		wlan_hdd_send_svc_nlink_msg(hdd_ctx->radio_index,
					WLAN_SVC_WLAN_VERSION_IND,
					    &data, sizeof(data));
}

/**
 * wlan_hdd_send_scan_intf_info() - report scan interfaces to lpass
 * @hdd_ctx: The global HDD context
 * @adapter: Adapter that supports scanning.
 *
 * This function indicates adapter that supports scanning to lpass.
 *
 * Return: none
 */
static void wlan_hdd_send_scan_intf_info(struct hdd_context *hdd_ctx,
					 struct hdd_adapter *adapter)
{
	if (!hdd_ctx) {
		hdd_err("NULL pointer for hdd_ctx");
		return;
	}

	if (!adapter) {
		hdd_err("Adapter is Null");
		return;
	}

	wlan_hdd_send_status_pkg(adapter, NULL, 1, 0);
}

/*
 * hdd_lpass_target_config() - Handle LPASS target configuration
 * (public function documented in wlan_hdd_lpass.h)
 */
void hdd_lpass_target_config(struct hdd_context *hdd_ctx,
			     struct wma_tgt_cfg *target_config)
{
	hdd_ctx->lpss_support = target_config->lpss_support;
}

/*
 * hdd_lpass_populate_cds_config() - Populate LPASS configuration
 * (public function documented in wlan_hdd_lpass.h)
 */
void hdd_lpass_populate_cds_config(struct cds_config_info *cds_config,
				   struct hdd_context *hdd_ctx)
{
	bool lpass_support = false;
	QDF_STATUS status;

	status = ucfg_mlme_get_lpass_support(hdd_ctx->psoc, &lpass_support);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to get LPASS support config");

	cds_config->is_lpass_enabled = lpass_support;
}

/*
 * hdd_lpass_populate_pmo_config() - Populate LPASS configuration
 * (public function documented in wlan_hdd_lpass.h)
 */
void hdd_lpass_populate_pmo_config(struct pmo_psoc_cfg *pmo_config,
				   struct hdd_context *hdd_ctx)
{
	bool lpass_support = false;
	QDF_STATUS status;

	status = ucfg_mlme_get_lpass_support(hdd_ctx->psoc, &lpass_support);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to get LPASS support config");

	pmo_config->lpass_enable = lpass_support;
}

/*
 * hdd_lpass_notify_connect() - Notify LPASS of interface connect
 * (public function documented in wlan_hdd_lpass.h)
 */
void hdd_lpass_notify_connect(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *sta_ctx;

	/* only send once per connection */
	if (adapter->rssi_send)
		return;

	/* don't send if driver is unloading */
	if (cds_is_driver_unloading())
		return;

	adapter->rssi_send = true;
	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	wlan_hdd_send_status_pkg(adapter, sta_ctx, 1, 1);
}

/*
 * hdd_lpass_notify_disconnect() - Notify LPASS of interface disconnect
 * (public function documented in wlan_hdd_lpass.h)
 */
void hdd_lpass_notify_disconnect(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *sta_ctx;

	adapter->rssi_send = false;
	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
	wlan_hdd_send_status_pkg(adapter, sta_ctx, 1, 0);
}

void hdd_lpass_notify_mode_change(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctx;

	if (!adapter || adapter->device_mode != QDF_STA_MODE)
		return;

	hdd_debug("Sending Lpass mode change notification");

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	wlan_hdd_send_scan_intf_info(hdd_ctx, adapter);
}

/*
 * hdd_lpass_notify_wlan_version() - Notify LPASS WLAN Host/FW version
 *
 * implementation note: Notify LPASS for the WLAN host/firmware version.
 */
void hdd_lpass_notify_wlan_version(struct hdd_context *hdd_ctx)
{
	hdd_enter();

	wlan_hdd_send_version_pkg(hdd_ctx->target_fw_version,
				  hdd_ctx->target_hw_version,
				  hdd_ctx->target_hw_name);

	hdd_exit();
}

void hdd_lpass_notify_start(struct hdd_context *hdd_ctx,
			    struct hdd_adapter *adapter)
{
	hdd_enter();

	if (!adapter || adapter->device_mode != QDF_STA_MODE)
		return;

	hdd_debug("Sending Start Lpass notification");

	wlan_hdd_send_scan_intf_info(hdd_ctx, adapter);

	hdd_exit();
}

void hdd_lpass_notify_stop(struct hdd_context *hdd_ctx)
{
	hdd_debug("Sending Lpass stop notifcation");
	wlan_hdd_send_status_pkg(NULL, NULL, 0, 0);
}

/*
 * hdd_lpass_is_supported() - Is lpass feature supported?
 * (public function documented in wlan_hdd_lpass.h)
 */
bool hdd_lpass_is_supported(struct hdd_context *hdd_ctx)
{
	bool lpass_support = false;
	QDF_STATUS status;

	status = ucfg_mlme_get_lpass_support(hdd_ctx->psoc, &lpass_support);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_err("Failed to get LPASS support config");

	return lpass_support;
}
