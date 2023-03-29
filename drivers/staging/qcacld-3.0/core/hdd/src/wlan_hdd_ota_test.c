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
 * DOC: wlan_hdd_ota_test.c
 *
 * WLAN OTA test functions
 *
 */

#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <sme_power_save_api.h>
#include <wlan_hdd_ota_test.h>

const struct nla_policy qca_wlan_vendor_ota_test_policy[
		QCA_WLAN_VENDOR_ATTR_OTA_TEST_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_OTA_TEST_ENABLE] = {.type = NLA_U8 },
};

/**
 * __wlan_hdd_cfg80211_set_ota_test() - enable/disable OTA test
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Data length
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_set_ota_test(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx  = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_OTA_TEST_MAX + 1];
	uint8_t ota_enable = 0;
	QDF_STATUS status;
	uint32_t current_roam_state;
	mac_handle_t mac_handle;

	hdd_enter_dev(dev);

	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (wlan_hdd_validate_context(hdd_ctx) != 0)
		return -EINVAL;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_OTA_TEST_MAX,
				    data, data_len,
				    qca_wlan_vendor_ota_test_policy)) {
		hdd_err("invalid attr");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_OTA_TEST_ENABLE]) {
		hdd_err("attr ota test failed");
		return -EINVAL;
	}

	ota_enable = nla_get_u8(
		tb[QCA_WLAN_VENDOR_ATTR_OTA_TEST_ENABLE]);

	hdd_debug(" OTA test enable = %d", ota_enable);
	if (ota_enable != 1) {
		hdd_err("Invalid value, only enable test mode is supported!");
		return -EINVAL;
	}

	mac_handle = hdd_ctx->mac_handle;
	current_roam_state =
		sme_get_current_roam_state(mac_handle, adapter->vdev_id);
	status = sme_stop_roaming(mac_handle, adapter->vdev_id,
				  eCsrHddIssued, RSO_INVALID_REQUESTOR);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Enable/Disable roaming failed");
		return -EINVAL;
	}

	status = sme_ps_enable_disable(mac_handle, adapter->vdev_id,
				       SME_PS_DISABLE);
	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Enable/Disable power save failed");
		/* restore previous roaming setting */
		if (current_roam_state == eCSR_ROAMING_STATE_JOINING ||
		    current_roam_state == eCSR_ROAMING_STATE_JOINED)
			status = sme_start_roaming(mac_handle,
						 adapter->vdev_id,
						 eCsrHddIssued,
						 RSO_INVALID_REQUESTOR);
		else if (current_roam_state == eCSR_ROAMING_STATE_STOP ||
			 current_roam_state == eCSR_ROAMING_STATE_IDLE)
			status = sme_stop_roaming(mac_handle,
						 adapter->vdev_id,
						 eCsrHddIssued,
						 RSO_INVALID_REQUESTOR);

		if (status != QDF_STATUS_SUCCESS)
			hdd_err("Restoring roaming state failed");

		return -EINVAL;
	}
	return 0;
}

int wlan_hdd_cfg80211_set_ota_test(struct wiphy *wiphy,
				   struct wireless_dev *wdev,
				   const void *data,
				   int data_len)
{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_ota_test(wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

