/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

/**
 * DOC: wlan_hdd_btc_chain_mode.c
 *
 * The implementation of bt coex chain mode configuration
 *
 */

#include "wlan_hdd_main.h"
#include "wlan_hdd_btc_chain_mode.h"
#include "osif_sync.h"
#include "wlan_coex_ucfg_api.h"
#include "wlan_hdd_object_manager.h"
#include "wlan_cfg80211_coex.h"

static QDF_STATUS
wlan_hdd_btc_chain_mode_handler(struct wlan_objmgr_vdev *vdev)
{
	QDF_STATUS status;
	struct hdd_adapter *adapter;
	mac_handle_t mac_handle;
	uint8_t nss, mode, band;
	uint8_t vdev_id;
	uint32_t freq;
	struct wlan_objmgr_psoc *psoc;

	if (!vdev) {
		hdd_err("NULL vdev");
		return QDF_STATUS_E_INVAL;
	}

	vdev_id = wlan_vdev_get_id(vdev);
	if (wlan_hdd_validate_vdev_id(vdev_id))
		return QDF_STATUS_E_INVAL;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		hdd_err("NULL psoc");
		return QDF_STATUS_E_INVAL;
	}

	adapter = wlan_hdd_get_adapter_from_vdev(psoc, vdev_id);
	if (!adapter) {
		hdd_err("NULL adapter");
		return QDF_STATUS_E_INVAL;
	}

	status = ucfg_coex_psoc_get_btc_chain_mode(psoc, &mode);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("failed to get cur BTC chain mode, status %d", status);
		return -EFAULT;
	}

	mac_handle = adapter->hdd_ctx->mac_handle;
	if (!mac_handle) {
		hdd_err("NULL MAC handle");
		return -EINVAL;
	}

	nss = ((mode == QCA_BTC_CHAIN_SEPARATED) ? 1 : 2);

	hdd_debug("update nss to %d for vdev %d, device mode %d",
		  nss, adapter->vdev_id, adapter->device_mode);
	band = NSS_CHAINS_BAND_2GHZ;
	sme_update_nss_in_mlme_cfg(mac_handle, nss, nss,
				   adapter->device_mode, band);
	sme_update_vdev_type_nss(mac_handle, nss, band);
	hdd_store_nss_chains_cfg_in_vdev(adapter);
	sme_update_he_cap_nss(mac_handle, adapter->vdev_id, nss);

	freq = hdd_get_adapter_home_channel(adapter);

	/*
	 * BT coex chain mode is for COEX between BT and WiFi-2.4G.
	 * Nss and related parameters have been updated upon for
	 * NSS_CHAINS_BAND_2GHZ.
	 * If the current home channel is NOT 2.4G, these parameters
	 * will take effect when switching to 2.4G, so no need to do
	 * restart here.
	 */
	if (!WLAN_REG_IS_24GHZ_CH_FREQ(freq))
		return QDF_STATUS_SUCCESS;

	switch (adapter->device_mode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
		wlan_hdd_disconnect(adapter, 0, REASON_PREV_AUTH_NOT_VALID);
		break;
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
		hdd_restart_sap(adapter);
		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

void wlan_hdd_register_btc_chain_mode_handler(struct wlan_objmgr_psoc *psoc)
{
	ucfg_coex_register_cfg_updated_handler(psoc,
					       COEX_CONFIG_BTC_CHAIN_MODE,
					       wlan_hdd_btc_chain_mode_handler
					       );
}

/**
 * __wlan_hdd_cfg80211_set_btc_chain_mode() - set btc chain mode
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: pointer to btc chain mode command parameters.
 * @data_len: the length in byte of btc chain mode command parameters.
 *
 * Return: An error code or 0 on success.
 */
static int __wlan_hdd_cfg80211_set_btc_chain_mode(struct wiphy *wiphy,
						  struct wireless_dev *wdev,
						  const void *data,
						  int data_len)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(wdev->netdev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	int errno;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter_dev(wdev->netdev);

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno != 0)
		return errno;

	if (hdd_ctx->num_rf_chains < 2) {
		hdd_debug("Num of chains [%u] is less than 2, setting BTC separate chain mode is not allowed",
			  hdd_ctx->num_rf_chains);
		return -EINVAL;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return -EINVAL;

	errno = wlan_cfg80211_coex_set_btc_chain_mode(vdev, data, data_len);
	hdd_objmgr_put_vdev(vdev);

	return errno;
}

/**
 * wlan_hdd_cfg80211_set_btc_chain_mode() - set btc chain mode
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: pointer to btc chain mode command parameters.
 * @data_len: the length in byte of btc chain mode command parameters.
 *
 * Return: An error code or 0 on success.
 */
int wlan_hdd_cfg80211_set_btc_chain_mode(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_btc_chain_mode(wiphy, wdev,
						       data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
