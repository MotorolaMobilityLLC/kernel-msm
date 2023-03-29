/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_coex_config.c
 *
 * The implementation of coex configuration
 *
 */

#include "wlan_hdd_main.h"
#include "wmi_unified_param.h"
#include "wlan_hdd_coex_config.h"
#include "qca_vendor.h"
#include "wlan_osif_request_manager.h"
#include "osif_sync.h"
#include "wlan_fwol_ucfg_api.h"

const struct nla_policy
coex_config_three_way_policy[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_MAX + 1] = {
	[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_CONFIG_TYPE] = {
							      .type = NLA_U32},
	[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_1] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_2] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_3] = {.type = NLA_U32},
	[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_4] = {.type = NLA_U32},
};

static const uint32_t
config_type_to_wmi_tbl[QCA_WLAN_VENDOR_ATTR_COEX_CONFIG_TYPE_MAX] = {
	[QCA_WLAN_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_COEX_RESET] =
		WMI_COEX_CONFIG_THREE_WAY_COEX_RESET,
	[QCA_WLAN_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_COEX_START] =
		WMI_COEX_CONFIG_THREE_WAY_COEX_START,
};

/**
 * __wlan_hdd_cfg80211_set_coex_config() - set coex configuration
 * parameters
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: pointer to limit off-channel command parameters.
 * @data_len: the length in byte of  limit off-channel command parameters.
 *
 * Return: An error code or 0 on success.
 */
static int __wlan_hdd_cfg80211_set_coex_config(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data, int data_len)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(wdev->netdev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_MAX + 1];
	uint32_t config_type;
	struct coex_config_params coex_cfg_params = {0};
	struct wlan_fwol_coex_config config = {0};
	int errno;
	QDF_STATUS status;

	hdd_enter();

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno != 0)
		return errno;

	status = ucfg_fwol_get_coex_config_params(hdd_ctx->psoc, &config);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to get coex config params");
		return -EINVAL;
	}
	if (!config.btc_three_way_coex_config_legacy_enable) {
		hdd_err("Coex legacy feature should be enable first");
		return -EINVAL;
	}

	if (wlan_cfg80211_nla_parse(tb,
				    QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_MAX,
				    data, data_len,
				    coex_config_three_way_policy)) {
		hdd_err("Invalid coex config ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_CONFIG_TYPE]) {
		hdd_err("coex config - attr config_type failed");
		return -EINVAL;
	}

	config_type = nla_get_u32(
		tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_CONFIG_TYPE]);
	if (config_type >= QCA_WLAN_VENDOR_ATTR_COEX_CONFIG_TYPE_MAX) {
		hdd_err("config_type value %d exceeded Max value %d",
			config_type,
			QCA_WLAN_VENDOR_ATTR_COEX_CONFIG_TYPE_MAX);
		return -EINVAL;
	}
	coex_cfg_params.config_type = config_type_to_wmi_tbl[config_type];
	if (coex_cfg_params.config_type <
	    WMI_COEX_CONFIG_THREE_WAY_DELAY_PARA ||
	    coex_cfg_params.config_type >
	    WMI_COEX_CONFIG_THREE_WAY_COEX_START) {
		hdd_err("config_type_wmi val error %d",
			coex_cfg_params.config_type);
		return -EINVAL;
	}

	hdd_debug("config_type %d, config_type_wmi %d",
		  config_type, coex_cfg_params.config_type);

	if (!tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_1]) {
		hdd_err("coex config - attr priority1 failed");
		return -EINVAL;
	}
	coex_cfg_params.config_arg1 = nla_get_u32(
		tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_1]);

	hdd_debug("priority1 0x%x", coex_cfg_params.config_arg1);

	if (!tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_2]) {
		hdd_err("coex config - attr priority2 failed");
		return -EINVAL;
	}
	coex_cfg_params.config_arg2 = nla_get_u32(
		tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_2]);

	hdd_debug("priority2 0x%x", coex_cfg_params.config_arg2);

	if (!tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_3]) {
		hdd_err("coex config - attr priority3 failed");
		return -EINVAL;
	}
	coex_cfg_params.config_arg3 = nla_get_u32(
		tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_3]);

	hdd_debug("priority3 0x%x", coex_cfg_params.config_arg3);

	if (!tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_4]) {
		hdd_err("coex config - attr priority4 failed");
		return -EINVAL;
	}
	coex_cfg_params.config_arg4 = nla_get_u32(
		tb[QCA_VENDOR_ATTR_COEX_CONFIG_THREE_WAY_PRIORITY_4]);

	hdd_debug("priority4 0x%x", coex_cfg_params.config_arg4);

	coex_cfg_params.vdev_id = adapter->vdev_id;
	status = sme_send_coex_config_cmd(&coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex config params");
		return -EINVAL;
	}

	return 0;
}

/**
 * wlan_hdd_cfg80211_set_coex_config() - set coex configuration
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: pointer to limit off-channel command parameters.
 * @data_len: the length in byte of  limit off-channel command parameters.
 *
 *
 * Return: An error code or 0 on success.
 */
int wlan_hdd_cfg80211_set_coex_config(struct wiphy *wiphy,
				      struct wireless_dev *wdev,
				      const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_coex_config(wiphy, wdev,
						    data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
