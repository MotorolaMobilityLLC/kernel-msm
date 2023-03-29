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
 * DOC: wlan_hdd_mpta_helper.c
 *
 * The implementation of mpta helper configuration
 */

#include "wlan_hdd_main.h"
#include "wmi_unified_param.h"
#include "wlan_hdd_mpta_helper.h"
#include "qca_vendor.h"
#include "wlan_osif_request_manager.h"
#include "osif_sync.h"

const struct nla_policy
qca_wlan_vendor_mpta_helper_attr[QCA_MPTA_HELPER_VENDOR_ATTR_MAX + 1] = {
	[QCA_MPTA_HELPER_VENDOR_ATTR_ZIGBEE_STATE] = {.type = NLA_U32 },
	[QCA_MPTA_HELPER_VENDOR_ATTR_INT_WLAN_DURATION] = {.type = NLA_U32 },
	[QCA_MPTA_HELPER_VENDOR_ATTR_INT_NON_WLAN_DURATION] = {
							.type = NLA_U32 },
	[QCA_MPTA_HELPER_VENDOR_ATTR_MON_WLAN_DURATION] = {.type = NLA_U32 },
	[QCA_MPTA_HELPER_VENDOR_ATTR_MON_NON_WLAN_DURATION] = {
							.type = NLA_U32 },
	[QCA_MPTA_HELPER_VENDOR_ATTR_INT_OCS_DURATION] = {.type = NLA_U32 },
	[QCA_MPTA_HELPER_VENDOR_ATTR_MON_OCS_DURATION] = {.type = NLA_U32 },
	[QCA_MPTA_HELPER_VENDOR_ATTR_ZIGBEE_CHAN] = {.type = NLA_U32 },
	[QCA_MPTA_HELPER_VENDOR_ATTR_WLAN_MUTE_DURATION] = {.type = NLA_U32 },
};

/**
 * __wlan_hdd_cfg80211_mpta_helper_config() - update
 * tri-radio coex status by mpta helper
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Return: 0 on success; error number otherwise.
 *
 */
static int
__wlan_hdd_cfg80211_mpta_helper_config(struct wiphy *wiphy,
				       struct wireless_dev *wdev,
				       const void *data,
				       int data_len)
{
	struct hdd_context *hdd_ctx  = wiphy_priv(wiphy);
	struct nlattr *tb[QCA_MPTA_HELPER_VENDOR_ATTR_MAX + 1];
	struct coex_config_params coex_cfg_params = {0};
	int errno;
	QDF_STATUS status;

	hdd_enter_dev(wdev->netdev);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	if (wlan_cfg80211_nla_parse(tb, QCA_MPTA_HELPER_VENDOR_ATTR_MAX, data,
				    data_len,
				    qca_wlan_vendor_mpta_helper_attr)) {
		hdd_err("invalid attr");
		return -EINVAL;
	}

	/* if no attributes specified, return -EINVAL */
	errno = -EINVAL;

	if (tb[QCA_MPTA_HELPER_VENDOR_ATTR_ZIGBEE_STATE]) {
		coex_cfg_params.config_type =
			WMI_COEX_CONFIG_MPTA_HELPER_ZIGBEE_STATE;
		coex_cfg_params.config_arg1 = nla_get_u32
			(tb[QCA_MPTA_HELPER_VENDOR_ATTR_ZIGBEE_STATE]);

		status = sme_send_coex_config_cmd(&coex_cfg_params);

		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to set zigbee STATE");
			return -EINVAL;
		}

		errno = 0;
	}

	if ((tb[QCA_MPTA_HELPER_VENDOR_ATTR_INT_WLAN_DURATION]) &&
	    (tb[QCA_MPTA_HELPER_VENDOR_ATTR_INT_NON_WLAN_DURATION])) {
		coex_cfg_params.config_type =
			WMI_COEX_CONFIG_MPTA_HELPER_INT_OCS_PARAMS;
		coex_cfg_params.config_arg1 = nla_get_u32
			(tb[QCA_MPTA_HELPER_VENDOR_ATTR_INT_WLAN_DURATION]);
		coex_cfg_params.config_arg2 = nla_get_u32
			(tb[QCA_MPTA_HELPER_VENDOR_ATTR_INT_NON_WLAN_DURATION]);

		status = sme_send_coex_config_cmd(&coex_cfg_params);

		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to set int OCS duration");
			return -EINVAL;
		}

		errno = 0;
	}

	if ((tb[QCA_MPTA_HELPER_VENDOR_ATTR_MON_WLAN_DURATION]) &&
	    (tb[QCA_MPTA_HELPER_VENDOR_ATTR_MON_NON_WLAN_DURATION])) {
		coex_cfg_params.config_type =
			WMI_COEX_CONFIG_MPTA_HELPER_MON_OCS_PARAMS;
		coex_cfg_params.config_arg1 = nla_get_u32
			(tb[QCA_MPTA_HELPER_VENDOR_ATTR_MON_WLAN_DURATION]);
		coex_cfg_params.config_arg2 = nla_get_u32
			(tb[QCA_MPTA_HELPER_VENDOR_ATTR_MON_NON_WLAN_DURATION]);

		status = sme_send_coex_config_cmd(&coex_cfg_params);

		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to set mon OCS duration");
			return -EINVAL;
		}

		errno = 0;
	}

	if ((tb[QCA_MPTA_HELPER_VENDOR_ATTR_INT_OCS_DURATION]) &&
	    (tb[QCA_MPTA_HELPER_VENDOR_ATTR_MON_OCS_DURATION])) {
		coex_cfg_params.config_type =
			WMI_COEX_CONFIG_MPTA_HELPER_INT_MON_DURATION;
		coex_cfg_params.config_arg1 = nla_get_u32
			(tb[QCA_MPTA_HELPER_VENDOR_ATTR_INT_OCS_DURATION]);
		coex_cfg_params.config_arg2 = nla_get_u32
			(tb[QCA_MPTA_HELPER_VENDOR_ATTR_MON_OCS_DURATION]);

		status = sme_send_coex_config_cmd(&coex_cfg_params);

		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to set int mon duration");
			return -EINVAL;
		}

		errno = 0;
	}

	if (tb[QCA_MPTA_HELPER_VENDOR_ATTR_ZIGBEE_CHAN]) {
		coex_cfg_params.config_type =
			WMI_COEX_CONFIG_MPTA_HELPER_ZIGBEE_CHANNEL;
		coex_cfg_params.config_arg1 = nla_get_u32
			(tb[QCA_MPTA_HELPER_VENDOR_ATTR_ZIGBEE_CHAN]);

		status = sme_send_coex_config_cmd(&coex_cfg_params);

		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to set zigbee chan");
			return -EINVAL;
		}

		errno = 0;
	}

	if (tb[QCA_MPTA_HELPER_VENDOR_ATTR_WLAN_MUTE_DURATION]) {
		coex_cfg_params.config_type =
			WMI_COEX_CONFIG_MPTA_HELPER_WLAN_MUTE_DURATION;
		coex_cfg_params.config_arg1 = nla_get_u32
			(tb[QCA_MPTA_HELPER_VENDOR_ATTR_WLAN_MUTE_DURATION]);

		status = sme_send_coex_config_cmd(&coex_cfg_params);

		if (QDF_IS_STATUS_ERROR(status)) {
			hdd_err("Failed to set wlan mute duration");
			return -EINVAL;
		}

		errno = 0;
	}

	return errno;
}

/**
 * wlan_hdd_cfg80211_mpta_helper_config() - update
 * tri-radio coex status by mpta helper
 * @wiphy: wiphy device pointer
 * @wdev: wireless device pointer
 * @data: Vendor command data buffer
 * @data_len: Buffer length
 *
 * Return: 0 on success; error number otherwise.
 *
 */
int
wlan_hdd_cfg80211_mpta_helper_config(struct wiphy *wiphy,
				     struct wireless_dev *wdev,
				     const void *data,
				     int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_mpta_helper_config(
					wiphy, wdev, data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * wlan_hdd_mpta_helper_enable() - enable/disable mpta helper
 * according to cfg from INI
 * @coex_cfg_params: pointer of coex config command params
 * @config: pointer of BTC config items
 *
 * Return: 0 on success; error number otherwise.
 *
 */
int
wlan_hdd_mpta_helper_enable(struct coex_config_params *coex_cfg_params,
			    struct wlan_fwol_coex_config *config)
{
	QDF_STATUS status;

	coex_cfg_params->config_type = WMI_COEX_CONFIG_MPTA_HELPER_ENABLE;
	coex_cfg_params->config_arg1 = config->btc_mpta_helper_enable;

	status = sme_send_coex_config_cmd(coex_cfg_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to send coex MPTA Helper Enable");
		return -EINVAL;
	}

	return 0;
}

