/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_nan.c
 *
 * WLAN Host Device Driver NAN API implementation
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/cfg80211.h>
#include <ani_global.h>
#include "sme_api.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_nan.h"
#include "osif_sync.h"
#include <qca_vendor.h>
#include "cfg_nan_api.h"
#include "os_if_nan.h"

/**
 * wlan_hdd_nan_is_supported() - HDD NAN support query function
 * @hdd_ctx: Pointer to hdd context
 *
 * This function is called to determine if NAN is supported by the
 * driver and by the firmware.
 *
 * Return: true if NAN is supported by the driver and firmware
 */
bool wlan_hdd_nan_is_supported(struct hdd_context *hdd_ctx)
{
	return cfg_nan_get_enable(hdd_ctx->psoc) &&
		sme_is_feature_supported_by_fw(NAN);
}

/**
 * __wlan_hdd_cfg80211_nan_ext_request() - cfg80211 NAN extended request handler
 * @wiphy: driver's wiphy struct
 * @wdev: wireless device to which the request is targeted
 * @data: actual request data (netlink-encapsulated)
 * @data_len: length of @data
 *
 * Handles NAN Extended vendor commands, sends the command to NAN component
 * which parses and forwards the NAN requests.
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_nan_ext_request(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	int ret_val;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	hdd_enter_dev(wdev->netdev);

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err_rl("Command not allowed in FTM mode");
		return -EPERM;
	}

	if (!wlan_hdd_nan_is_supported(hdd_ctx)) {
		hdd_debug_rl("NAN is not supported");
		return -EPERM;
	}

	if (hdd_is_connection_in_progress(NULL, NULL)) {
		hdd_err("Connection refused: conn in progress");
		return -EAGAIN;
	}

	return os_if_process_nan_req(hdd_ctx->psoc, adapter->vdev_id,
				     data, data_len);
}

int wlan_hdd_cfg80211_nan_ext_request(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data,
				      int data_len)

{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_nan_ext_request(wiphy, wdev,
						    data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

void hdd_nan_concurrency_update(void)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	int ret;

	hdd_enter();
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return;

	wlan_twt_concurrency_update(hdd_ctx);
	hdd_exit();
}
